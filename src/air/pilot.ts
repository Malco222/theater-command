// The pilots. You never fly the aircraft. Each pilot flies it for you, inside the
// limits of the machine and of their own skill.
import { clamp, wrapAngle } from '../core/math';
import { excessPower, navControl, stallSpeed } from './flight';
import { destroy, fallOut, fireGuns, hasAmmo, release } from './combat';
import type { Aircraft, World } from './types';

const THINK = 0.2;

function turnIndex(a: Aircraft): number {
  return (a.type.clMax * a.type.S) / a.type.W;
}

function pickTarget(w: World, a: Aircraft): void {
  const sq = a.sq;
  let best: Aircraft | null = null;
  let bs = 1e12;
  let bombersAround = false, fightersAround = false;
  for (const e of sq.engaged) { if (e.isBomber) bombersAround = true; else fightersAround = true; }
  for (const E of sq.engaged) {
    for (const e of E.ac) {
      if (!e.alive || e.gone || e.mode === 'ground') continue;
      const d = Math.hypot(e.x - a.x, e.y - a.y, e.z - a.z);
      if (d > 7500) continue;
      let s = d + e.hunters * 900;
      if (E.isBomber && sq.stance === 'fighters' && fightersAround) s += 3000;
      if (!E.isBomber && sq.stance === 'bombers' && bombersAround) s += 3000;
      if (s < bs) { bs = s; best = e; }
    }
  }
  if (best) { a.tgt = best; best.hunters++; }
}

function energyLimit(a: Aircraft, wantGam: number): number {
  // A pilot cannot hold a climb the engine cannot pay for. Spare speed buys a short zoom.
  const vs = stallSpeed(a.type, a.z);
  const ps = excessPower(a.type, a.v, a.z, 1, 1, a.powerMul);
  const sustain = Math.asin(clamp(ps / a.v, -0.2, 0.5));
  const zoom = clamp((a.v - 1.45 * vs) / a.v, 0, 0.6) * 1.3;
  let g = Math.min(wantGam, sustain + zoom);
  if (a.v < 1.22 * vs) g = Math.min(g, -0.2);
  if (a.z < 450 && g < 0.08) g = 0.12;
  return g;
}

function pursue(w: World, a: Aircraft, tg: Aircraft): void {
  const t = a.type;
  const dx = tg.x - a.x, dy = tg.y - a.y, dz = tg.z - a.z;
  const r = Math.hypot(dx, dy, dz);
  const cgT = Math.cos(tg.gam);
  const tvx = tg.v * cgT * Math.cos(tg.hdg), tvy = tg.v * cgT * Math.sin(tg.hdg), tvz = tg.v * Math.sin(tg.gam);
  // Aim ahead of the target: by the bullet's flight time up close, by the closing time further out.
  const lead = r < 900 ? r / 760 : Math.min(6, (r / Math.max(a.v, 60)) * 0.55);
  const lx = dx + tvx * lead, ly = dy + tvy * lead, lz = dz + tvz * lead;
  const lr = Math.hypot(lx, ly, lz) || 1;
  const skill = a.pilot.skill;
  const big = tg.type.role === 'bomber';
  a.cmdHdg = Math.atan2(ly, lx);
  a.cmdGam = energyLimit(a, Math.asin(clamp(lz / lr, -0.95, 0.95)));
  a.cmdG = r > 1600 ? 3 : t.gLimit * (0.7 + 0.3 * skill) * (1 - 0.12 * a.pilot.fatigue) * (a.damaged ? 0.8 : 1);
  a.thr = 1;

  const cg = Math.cos(a.gam);
  const fx = cg * Math.cos(a.hdg), fy = cg * Math.sin(a.hdg), fz = Math.sin(a.gam);
  const cosOff = (fx * lx + fy * ly + fz * lz) / lr;

  // Sitting close behind someone puts them on notice.
  if (r < 700) {
    const behind = (dx * tvx + dy * tvy + dz * tvz) > 0;
    if (behind && tg.threat !== a) { tg.threat = a; tg.aware = false; }
  }

  const openR = big ? 430 : 390 - 160 * skill;
  a.firing = r < openR && cosOff > 0.9974 && hasAmmo(a);

  // Break away rather than collide, then come around again.
  const closure = a.v - (tvx * dx + tvy * dy + tvz * dz) / Math.max(r, 1);
  if (r < (big ? 95 : 55) + Math.max(0, closure) * 0.35) {
    a.ai = 'extend';
    a.aiT = w.time + (big ? 6 + w.rng.next() * 3 : 2 + w.rng.next() * 2);
    a.firing = false;
    a.cmdHdg = a.hdg + (w.rng.chance(0.5) ? 0.5 : -0.5);
    a.cmdGam = big ? -0.3 : 0.1;
    return;
  }
  // An aircraft that cannot out-turn its target does not try. It climbs away and comes back with speed.
  if (!big && r < 650 && cosOff < 0.45 && turnIndex(a) < turnIndex(tg) * 0.92) {
    a.ai = 'extend';
    a.aiT = w.time + 5 + w.rng.next() * 3;
    a.cmdHdg = a.hdg;
    a.cmdGam = 0.22;
  }
}

function evade(w: World, a: Aircraft, th: Aircraft): void {
  const dx = th.x - a.x, dy = th.y - a.y;
  const side = Math.sign(Math.cos(a.hdg) * dy - Math.sin(a.hdg) * dx) || 1;
  a.thr = 1;
  a.firing = false;
  const diver = !a.type.floatCarb && th.type.floatCarb && a.z > 1500;
  if (diver && a.pilot.skill > 0.35) {
    // The fuel-injected engine keeps running under negative G: push straight over and dive away.
    a.cmdHdg = a.hdg + side * 0.35;
    a.cmdGam = -0.75;
    a.cmdG = 4;
  } else {
    a.cmdHdg = a.hdg + side * 1.7;
    a.cmdGam = a.z > 800 ? -0.12 : 0.05;
    a.cmdG = a.type.gLimit * (0.75 + 0.25 * a.pilot.skill);
  }
}

function headHome(w: World, a: Aircraft): void {
  const b = a.sq.base;
  const d = Math.hypot(b.x - a.x, b.y - a.y);
  const alt = a.side === 0 ? (d < 12000 ? 300 : Math.min(a.z, 3000)) : clamp(a.z, 1500, 5000);
  navControl(a, Math.atan2(b.y - a.y, b.x - a.x), alt, 999, 3);
  if (a.powerMul < 1) a.thr = Math.min(a.thr, 0.9);
  if (d < (a.side === 0 ? 2500 : 7000)) { a.gone = true; a.alive = false; a.pilot.fate = a.pilot.fate ?? (a.pilot.note ? 'wounded' : 'safe'); }
}

function thinkFight(w: World, a: Aircraft): void {
  const sq = a.sq;
  if (a.tgt && (!a.tgt.alive || a.tgt.gone || a.tgt.mode === 'ground')) release(a);
  let th = a.threat;
  if (th) {
    const d = Math.hypot(th.x - a.x, th.y - a.y, th.z - a.z);
    if (!th.alive || th.tgt !== a || d > 1300) { th = a.threat = null; a.aware = false; }
    else if (!a.aware) {
      const p = (0.03 + 0.12 * a.pilot.skill * (1 - 0.4 * a.pilot.fatigue)) * w.sides[a.side].lookout;
      if (w.rng.chance(p)) a.aware = true;
    }
  }
  const empty = !hasAmmo(a);
  if (empty && !sq.isBomber) {
    // Nothing left to shoot with: go home rather than circle as a target.
    fallOut(w, a);
    a.sq.morale += 2; // leaving with empty guns is not a defeat
    headHome(w, a);
    return;
  }
  if (th && a.aware) {
    if (a.ai !== 'evade') { a.ai = 'evade'; a.aiT = w.time + 2.5 + w.rng.next() * 3; }
  } else if (a.ai === 'evade') {
    a.ai = 'attack';
  }
  if (a.ai === 'evade' && th) {
    evade(w, a, th);
    if (w.time > a.aiT) {
      // Either the attacker has been shaken off, or it is time to turn the tables.
      a.ai = 'attack';
      if (!th.tgt || th.tgt === a) { release(a); a.tgt = th; th.hunters++; }
      a.threat = null; a.aware = false;
    }
    return;
  }
  if (a.ai === 'extend') {
    a.firing = false;
    a.cmdG = 3.5;
    a.thr = 1;
    a.cmdGam = energyLimit(a, a.cmdGam);
    if (w.time > a.aiT) { a.ai = 'attack'; release(a); }
    else return;
  }
  if (!a.tgt) pickTarget(w, a);
  if (a.tgt) pursue(w, a, a.tgt);
  else {
    // No one to fight: stay with the leader until the unit reforms.
    a.firing = false;
    const L = sq.leader && sq.leader !== a && sq.leader.alive ? sq.leader : null;
    if (L) navControl(a, Math.atan2(L.y - a.y, L.x - a.x), L.z, L.v + 15, 3);
    else navControl(a, a.hdg, a.z, a.type.cruise, 2);
  }
}

function thinkSolo(w: World, a: Aircraft): void {
  const L = a.sq.leader;
  if (!L || L === a || !L.alive) { navControl(a, a.hdg, a.z, a.type.cruise, 2); return; }
  const c = Math.cos(L.hdg), s = Math.sin(L.hdg);
  const k = a.sq.morale < 50 && a.sq.isBomber ? 1.7 : 1;
  const tx = L.x + c * a.slotX * k - s * a.slotY * k + c * 250;
  const ty = L.y + s * a.slotX * k + c * a.slotY * k + s * 250;
  const tz = L.z + a.slotZ;
  const d = Math.hypot(tx - a.x, ty - a.y, tz - a.z);
  if (d < 330) { a.mode = 'form'; return; }
  navControl(a, Math.atan2(ty - a.y, tx - a.x), tz, L.v + clamp(d * 0.05, 5, 45), 3.5);
}

function thinkLeader(w: World, a: Aircraft): void {
  const sq = a.sq;
  const nav = sq.nav;
  let hdg = Math.atan2(nav.y - a.y, nav.x - a.x);
  if (sq.weave > 0) hdg += sq.weave * Math.sin(w.time * 0.17 + sq.id);
  navControl(a, hdg, nav.alt, nav.spd, nav.g);
}

function thinkOut(w: World, a: Aircraft): void {
  const th = a.threat;
  if (th && th.alive && th.tgt === a && Math.hypot(th.x - a.x, th.y - a.y, th.z - a.z) < 900 && a.type.role !== 'bomber') {
    if (!a.aware && w.rng.chance(0.08)) a.aware = true;
    if (a.aware) { evade(w, a, th); return; }
  }
  headHome(w, a);
}

export function think(w: World, a: Aircraft): void {
  a.thinkAt = w.time + THINK;
  switch (a.mode) {
    case 'fight': thinkFight(w, a); break;
    case 'solo': thinkSolo(w, a); break;
    case 'form': thinkLeader(w, a); break;
    case 'out': thinkOut(w, a); break;
  }
}

/** Fuel, the ground, and the guns: the things that happen every tick whatever the pilot is doing. */
export function afterFlight(w: World, a: Aircraft, dt: number): void {
  const t = a.type;
  const combat = clamp((a.thr - 0.6) / 0.4, 0, 1);
  a.fuel -= (t.burnCruise + (t.burnCombat - t.burnCruise) * combat + a.leak * 0.22) * dt;
  if (a.fuel <= 0 && t.role !== 'bomber') { a.fuel = 0; destroy(w, a, null, 'fuel', false); return; }
  if (a.z <= 5) {
    if (a.mode === 'out' || a.gam > -0.2) { a.z = 5; a.gam = Math.max(a.gam, 0.02); }
    else { destroy(w, a, null, 'structure', true); a.z = 0; }
    return;
  }
  if (a.firing && a.tgt && a.tgt.alive) fireGuns(w, a, a.tgt, dt);
}
