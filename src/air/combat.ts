// Gunnery, damage, and what happens to the pilot. One set of rules for every aircraft.
import { clamp } from '../core/math';
import type { Aircraft, World } from './types';
import { nearestPlace, say } from './world';

const BULLET_SPEED = 760;

export function hasAmmo(a: Aircraft): boolean {
  for (let i = 0; i < a.ammo.length; i++) if (a.ammo[i] > 0) return true;
  return false;
}

/** Fixed forward guns. Called every tick the pilot holds the trigger down. */
export function fireGuns(w: World, a: Aircraft, tg: Aircraft, dt: number): void {
  const dx = tg.x - a.x, dy = tg.y - a.y, dz = tg.z - a.z;
  const r = Math.hypot(dx, dy, dz);
  if (r > 520 || r < 1) return;
  const cg = Math.cos(tg.gam);
  const tvx = cg * Math.cos(tg.hdg), tvy = cg * Math.sin(tg.hdg), tvz = Math.sin(tg.gam);
  // 1 = firing from dead astern, -1 = head-on.
  const cosAspect = (dx * tvx + dy * tvy + dz * tvz) / r;
  const sinDef2 = 1 - cosAspect * cosAspect;
  const big = tg.type.role === 'bomber';
  const skill = a.pilot.skill * (1 - 0.3 * a.pilot.fatigue);
  const skillF = 0.25 + 1.5 * skill * skill;
  const rangeF = clamp(Math.pow(250 / Math.max(r, 60), 1.3), 0.12, 3);
  const sizeF = big ? 2.6 : tg.type.role === 'heavy_fighter' ? 1.6 : 1;
  const deflF = 1 / (1 + 4 * sinDef2 * (big ? 0.5 : 1));
  const gF = 1 / (1 + 0.25 * Math.max(0, tg.n - 1) + 0.15 * Math.max(0, a.n - 1));
  const headOnF = cosAspect < -0.7 ? 0.55 : 1;
  const p = clamp(0.03 * skillF * rangeF * sizeF * deflF * gF * headOnF, 0, 0.5);
  let dmg = 0;
  let heavy = false;
  for (let i = 0; i < a.type.guns.length; i++) {
    if (a.ammo[i] <= 0) continue;
    const g = a.type.guns[i];
    a.ammo[i] -= dt;
    dmg += w.rng.roll(g.count * g.rof * dt * p) * g.dmg;
    if (g.dmg > 3) heavy = true;
  }
  if ((w.tick + a.id) % 3 === 0) w.events.push({ t: 'shot', a, b: tg, heavy });
  // Being shot at gets a pilot's attention.
  tg.threat = a;
  if (dmg > 0) {
    tg.aware = true;
    applyDamage(w, tg, dmg, a, cosAspect);
  }
}

export function applyDamage(w: World, tg: Aircraft, dmg: number, by: Aircraft | null, cosAspect: number): void {
  if (!tg.alive) return;
  tg.hp -= dmg;
  tg.damaged = true;
  const sq = tg.sq;
  sq.morale -= (sq.isBomber ? 0.5 : 0.25) * Math.min(dmg, 6);
  // A fighter unit that is taking hits knows it is in a fight.
  if (!sq.isBomber && by && sq.state !== 'engaged' && sq.surprisedT === 0) sq.surprisedT = w.time + 1.5;
  if (tg.hp <= 0) { destroy(w, tg, by, 'structure', false); return; }
  const pc = 1 - Math.pow(1 - (tg.type.role === 'bomber' ? 0.03 : 0.042), dmg);
  if (!w.rng.chance(pc)) return;
  const roll = w.rng.next();
  const t = tg.type;
  if (roll < 0.34) {
    // Engine or cooling system.
    tg.enginesOut++;
    tg.smoking = true;
    if (tg.enginesOut >= t.engines || (t.engines === 1 && w.rng.chance(0.5))) { destroy(w, tg, by, 'engine', false); return; }
    tg.powerMul = t.engines === 1 ? 0.55 : ((t.engines - tg.enginesOut) / t.engines) * 0.92;
    tg.dragMul = 1.15;
    fallOut(w, tg);
  } else if (roll < 0.58) {
    // Fuel tanks.
    if (w.rng.chance(t.selfSealing ? 0.28 : 0.55)) { tg.onFire = true; destroy(w, tg, by, 'fire', false); return; }
    tg.leak += 1;
    tg.smoking = true;
  } else if (roll < 0.74) {
    // The pilot. Armour plate behind the seat only helps against fire from astern.
    const protect = t.pilotArmor * (cosAspect > 0.3 ? 1 : 0.3);
    if (w.rng.chance(protect)) return;
    if (t.crew > 1 ? w.rng.chance(0.25) : w.rng.chance(0.5)) { destroy(w, tg, by, 'pilot', true); return; }
    tg.pilot.skill *= 0.6;
    tg.pilot.note = 'wounded in the air';
    fallOut(w, tg);
  } else {
    // Control surfaces.
    tg.dragMul = Math.max(tg.dragMul, 1.12);
    tg.cmdG = 2.5;
    fallOut(w, tg);
  }
}

/** A damaged aircraft leaves the fight and heads for home on its own. */
export function fallOut(w: World, a: Aircraft): void {
  if (a.mode === 'out' || !a.alive) return;
  release(a);
  a.mode = 'out';
  a.firing = false;
  if (a.bombs) a.bombs = false; // jettisoned
  a.sq.morale -= a.sq.isBomber ? 3 : 2;
}

export function release(a: Aircraft): void {
  if (a.tgt) { a.tgt.hunters = Math.max(0, a.tgt.hunters - 1); a.tgt = null; }
  a.firing = false;
}

export function destroy(w: World, tg: Aircraft, by: Aircraft | null, cause: 'structure' | 'engine' | 'fire' | 'pilot' | 'fuel' | 'flak', pilotKilled: boolean): void {
  if (!tg.alive) return;
  release(tg);
  tg.alive = false;
  tg.falling = true;
  tg.firing = false;
  tg.smoking = true;
  const sq = tg.sq;
  const wasLeader = sq.leader === tg;
  sq.lost++;
  sq.morale -= (sq.isBomber ? 7 : 6) * (wasLeader ? 2 : 1);
  if (by && by.side !== tg.side) {
    by.pilot.killsNow++;
    by.sq.kills++;
    by.sq.morale = Math.min(100, by.sq.morale + 3);
  }
  // What happens to the pilot depends on where the aircraft comes down.
  const p = tg.pilot;
  const where = w.overHome(tg.x, tg.y); // 0 sea, 1 defender's soil, 2 attacker's side
  const home = tg.side === 0 ? 1 : 2;
  let out = false;
  if (pilotKilled) p.fate = 'killed';
  else {
    const pSurvive = cause === 'fire' ? 0.62 : cause === 'engine' || cause === 'fuel' ? 0.9 : cause === 'structure' ? 0.6 : 0.75;
    if (tg.z < 250 || !w.rng.chance(pSurvive)) p.fate = 'killed';
    else {
      out = true;
      if (where === home) p.fate = w.rng.chance(0.3) ? 'wounded' : 'safe';
      else if (where === 0) p.fate = w.rng.chance(tg.side === 1 ? 0.45 : 0.3) ? 'rescued' : 'missing';
      else p.fate = 'captured';
    }
  }
  w.events.push({ t: 'boom', x: tg.x, y: tg.y, z: tg.z, big: cause === 'fire' || cause === 'structure' });
  if (out) w.events.push({ t: 'chute', x: tg.x, y: tg.y, z: tg.z, side: tg.side });
  const place = nearestPlace(w, tg.x, tg.y);
  const what = `${/^[AEIOU]/.test(tg.type.short) ? 'an' : 'a'} ${tg.type.short}`;
  const fateText = p.fate === 'killed' ? (tg.type.crew > 1 ? 'crew lost' : 'pilot killed')
    : p.fate === 'safe' ? 'pilot safe' : p.fate === 'wounded' ? 'pilot wounded but safe'
    : p.fate === 'captured' ? (tg.type.crew > 1 ? 'crew captured' : 'pilot captured')
    : p.fate === 'rescued' ? 'pilot picked up from the sea' : 'pilot missing';
  if (by && by.side !== tg.side) {
    const who = by.type.role === 'bomber' ? `A gunner of ${by.sq.name}` : `${by.pilot.rank} ${by.pilot.name} (${by.sq.name})`;
    say(w, by.side, 'good', `${who} shot down ${what} ${place}.`, tg.x, tg.y);
  }
  const why = cause === 'fuel' ? 'out of fuel' : cause === 'flak' ? 'hit by anti-aircraft fire' : 'shot down';
  say(w, tg.side, 'bad', `${sq.name} lost ${what} ${place} (${why}); ${fateText}.`, tg.x, tg.y);
  if (wasLeader) sq.leader = null;
}
