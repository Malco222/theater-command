// The computer's generals. In Historical mode each nation follows the intentions in
// data/campaign/plan-historical.json: who to attack, when, and toward which objectives.
// The plan only sets intentions. Every region still has to be won under the same rules
// the player uses, so history can come out differently.

import plan from '../../data/campaign/plan-historical.json';
import { aiProduction } from './economy';
import { KIND, STACK, TERRAIN_DEF, type Corps, type Game, type PlanPhase, type Region } from './types';
import {
  allied, assess, atWar, collapseScore, corpsBase, declareWar, ensureSupply, evacuate, friendlyUnits, frontage,
  hostileUnits, limitAttackers, moveCorps, moveCost, railMove, resolveBattle, weekOf,
} from './game';

const PLAN = plan as unknown as Record<string, PlanPhase[] | string>;

function phases(nat: string): PlanPhase[] {
  const p = PLAN[nat];
  return Array.isArray(p) ? p : [];
}

export function phaseFor(g: Game, nat: string): PlanPhase | null {
  return phases(nat).find((p) => g.turn >= p.from && g.turn <= p.to) ?? null;
}

/** Runs one nation's month. */
export function runAI(g: Game, nat: string): void {
  campaign(g, nat);
  aiProduction(g, nat);
}

function campaign(g: Game, nat: string): void {
  const n = g.nations[nat];
  if (!n.alive) return;
  const planned = phaseFor(g, nat);
  // Some phases wait on someone else's war: Germany's allies only march east if Germany does.
  const ph = planned?.ifAtWar && !atWar(g, planned.ifAtWar[0], planned.ifAtWar[1]) ? null : planned;
  if (ph?.declare) {
    let go = true;
    if (ph.declareWhenCollapsing) {
      const t = g.nations[ph.declareWhenCollapsing];
      go = t.alive ? collapseScore(g, t.id) >= (ph.collapseAt ?? 0.3 * t.resilience) : !!t.fellOn;
    }
    if (go) for (const e of ph.declare) declareWar(g, nat, e);
  }
  if (!n.atWar.length) return;
  if (ph?.release) for (const c of g.corps) if (c.nat === nat && c.hold && c.kind !== 'fort' && ph.release.includes(g.regions[c.reg].name)) c.hold = false;
  const posture = ph?.posture ?? 'defend';
  if (posture === 'defend') { defend(g, nat); return; }
  const enemies = enemySet(g, nat);
  const follow = posture === 'attack' && !ph!.force;
  const halt = new Set([...(ph && g.turn === ph.from ? ph.haltOrder ?? [] : []), ...(ph?.siege ?? [])]);
  const only = ph?.targets ? new Set(ph.targets) : ph?.limited ? new Set(objectivesFor(g, nat)) : null;
  offensive(g, nat, objectivesFor(g, nat), enemies, ph?.minOdds ?? 1.3, ph?.axis ?? [], g.turn > (ph?.from ?? 0), follow, halt, only);
  if (!n.alive) return;
  if (posture === 'redeploy') redeploy(g, nat, ph!, enemies);
  else if (follow) {
    advance(g, nat, enemies, 0, null, halt);
    // With the month's attacks made, whoever still has marching time shores up the thin places.
    if (n.alive) defend(g, nat);
  }
}

/** Enemies this nation is actively campaigning against: everyone named in its plan so far, plus any invader. */
function enemySet(g: Game, nat: string): Set<string> {
  const n = g.nations[nat];
  const s = new Set<string>();
  for (const p of phases(nat)) if (p.from <= g.turn) for (const e of p.enemies ?? []) if (n.atWar.includes(e)) s.add(e);
  for (const r of g.regions) if (r.owner === nat && r.ctrl !== nat && n.atWar.includes(r.ctrl)) s.add(r.ctrl);
  return s;
}

function objectivesFor(g: Game, nat: string): string[] {
  const list = phases(nat).filter((p) => p.from <= g.turn).sort((a, b) => b.from - a.from);
  return list.flatMap((p) => p.objectives ?? []);
}

// ---- Attack ---------------------------------------------------------------------------

function hops(g: Game, from: number, pass: (r: Region) => boolean): Int16Array {
  const d = new Int16Array(g.regions.length).fill(-1);
  d[from] = 0;
  const q = [from];
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (d[j] < 0 && pass(g.regions[j])) { d[j] = d[q[i]] + 1; q.push(j); }
  return d;
}

/** How strongly each region pulls the offensive: objectives pull hardest, and the ground on the way to them pulls too. */
function objectivePull(g: Game, nat: string, objs: string[]): Float32Array {
  const pull = new Float32Array(g.regions.length);
  objs.forEach((name, k) => {
    const o = g.byName[name];
    if (!o || allied(g, nat, o.ctrl)) return;
    const w = Math.max(3, 14 - 0.5 * k);
    const d = hops(g, o.id, () => true);
    for (let i = 0; i < pull.length; i++) if (d[i] >= 0 && d[i] <= 4) pull[i] = Math.max(pull[i], w * Math.pow(0.6, d[i]));
  });
  return pull;
}

const atkPower = (g: Game, c: Corps): number => corpsBase(g, c) * KIND[c.kind].atk;

/** Steps through friendly ground from every region to the nearest one that borders the goal. */
function doorstep(g: Game, nat: string, goal: Region | null): Int16Array | null {
  if (!goal) return null;
  const d = new Int16Array(g.regions.length).fill(-1);
  const q: number[] = [];
  for (const j of goal.nb) if (allied(g, nat, g.regions[j].ctrl)) { d[j] = 0; q.push(j); }
  if (!q.length) return null;
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (d[j] < 0 && allied(g, nat, g.regions[j].ctrl)) { d[j] = d[q[i]] + 1; q.push(j); }
  return d;
}

function offensive(g: Game, nat: string, objs: string[], enemies: Set<string>, minOdds: number, axisList: string[], axisOpen: boolean, follow: boolean, halt: Set<string>, only: Set<string> | null): void {
  const failed = new Set<number>();
  const axis = new Set(axisList);
  // In the first month the armoured thrust takes its objectives in order; after that the whole line is open.
  const notYet = (name: string): boolean => {
    if (axisOpen || !axis.has(name)) return false;
    for (const a of axisList) {
      if (a === name) return false;
      const r = g.byName[a];
      if (r && !allied(g, nat, r.ctrl)) return true;
    }
    return false;
  };
  const axisGoal = (): Region | null => {
    for (const a of axisList) {
      const r = g.byName[a];
      if (r && !allied(g, nat, r.ctrl)) return enemies.has(r.ctrl) && atWar(g, nat, r.ctrl) && !failed.has(r.id) ? r : null;
    }
    return null;
  };
  // Week by week, so that the first day's attacks all happen before anyone's second bound.
  for (let wave = 1; wave <= 4; wave++) {
   for (let pass = 0; pass < 3; pass++) {
    for (let iter = 0; iter < 200; iter++) {
      if (!g.nations[nat].alive) return;
      ensureSupply(g);
      const pull = objectivePull(g, nat, objs);
      const door = doorstep(g, nat, axisGoal());
      const cand = new Map<number, Corps[]>();
      for (const c of g.corps) {
        if (c.nat !== nat || c.hold || c.kind === 'fort' || c.ap <= 0 || weekOf(c, 1) > wave) continue;
        // Early in the month the fast troops are kept for the main thrust if they can get to it.
        const kept = door !== null && wave <= 2 && KIND[c.kind].mobile && door[c.reg] >= 0 && door[c.reg] <= c.ap;
        for (const j of g.regions[c.reg].nb) {
          const r = g.regions[j];
          if (failed.has(j) || halt.has(r.name) || (only && !only.has(r.name)) || !enemies.has(r.ctrl) || !atWar(g, nat, r.ctrl) || notYet(r.name)) continue;
          if (kept && !axis.has(r.name)) continue;
          let list = cand.get(j);
          if (!list) cand.set(j, (list = []));
          list.push(c);
        }
      }
      let best: { score: number; target: Region; corps: Corps[]; battle: boolean } | null = null;
      for (const [j, avail] of cand) {
        const target = g.regions[j];
        const value = target.vp * 2 + (target.cap ? 8 : 0) + pull[j];
        if (!hostileUnits(g, target, nat).length) {
          const movers = avail
            .filter((c) => c.ap >= Math.min(moveCost(g, c, target), 4))
            .sort((a, b) => KIND[a.kind].move - KIND[b.kind].move || b.ap - a.ap);
          if (!movers.length || value < 2.5) continue;
          if (!axis.has(target.name)) movers.sort((a, b) => (KIND[a.kind].mobile ? 1 : 0) - (KIND[b.kind].mobile ? 1 : 0) || b.ap - a.ap);
          const score = value + 2;
          if (!best || score > best.score) best = { score, target, corps: [movers[0]], battle: false };
        } else {
          const pool = limitAttackers(g, avail.filter((c) => c.org >= 0.35), target);
          if (!pool.length) continue;
          // The tanks go in together along the main axis; elsewhere the infantry does the work.
          const main = axis.has(target.name);
          const fast = (c: Corps): number => (KIND[c.kind].mobile ? 1 : 0);
          pool.sort((a, b) => (main ? fast(b) - fast(a) : fast(a) - fast(b)) || atkPower(g, b) - atkPower(g, a));
          const least = main ? Math.max(1, pool.filter((c) => fast(c)).length) : 1;
          let pick: Corps[] | null = null, odds = 0;
          for (let k = least; k <= pool.length; k++) {
            odds = assess(g, pool.slice(0, k), target).odds;
            if (odds >= 2.6) { pick = pool.slice(0, k); break; }
          }
          if (!pick) { if (odds < minOdds) continue; pick = pool; }
          const score = value + Math.min(odds, 4);
          if (!best || score > best.score) best = { score, target, corps: pick, battle: true };
        }
      }
      if (!best) break;
      let taken: boolean;
      if (best.battle) {
        taken = resolveBattle(g, best.corps, best.target).taken;
        // A slow command does not get a second assault organised in the same month.
        if (!taken && g.nations[nat].slowCommand) failed.add(best.target.id);
      }
      else { taken = moveCorps(g, best.corps[0], best.target); if (!taken) failed.add(best.target.id); }
      if (taken && g.nations[nat].alive) support(g, nat, best.target);
    }
    // Nothing left to hit this week: close up to the front, then look again.
    if (!follow || !g.nations[nat].alive || !advance(g, nat, enemies, wave, doorstep(g, nat, axisGoal()), halt)) break;
   }
  }
}

/** A spearhead alone is a spearhead lost: bring a second corps up behind it when the enemy is close. */
function support(g: Game, nat: string, at: Region): void {
  if (!allied(g, nat, at.ctrl)) return;
  const danger = at.nb.some((j) => hostileUnits(g, g.regions[j], nat).length > 0);
  const want = danger ? 2 : 1;
  let have = friendlyUnits(g, at, nat).length;
  if (have >= want) return;
  const pool = g.corps
    .filter((c) => c.nat === nat && !c.hold && c.kind !== 'fort' && g.regions[c.reg].nb.includes(at.id) && c.ap >= Math.min(moveCost(g, c, at), 4))
    .sort((a, b) => (a.kind === 'arm' ? 1 : 0) - (b.kind === 'arm' ? 1 : 0) || b.ap - a.ap);
  for (const c of pool) {
    if (have >= want) break;
    if (moveCorps(g, c, at)) have++;
  }
}

/** Brings the rest of the army up to the front. With a week given, only as far as that week allows. Returns how many moves were made. */
function advance(g: Game, nat: string, enemies: Set<string>, wave: number, door: Int16Array | null, halt: Set<string>): number {
  let made = 0;
  const N = g.regions.length;
  const dist = new Float32Array(N).fill(1e9);
  const atFront = new Uint8Array(N);
  const full = (r: Region): boolean => friendlyUnits(g, r, nat).length >= STACK;
  const q: number[] = [];
  for (const r of g.regions) {
    if (r.ctrl !== nat) continue; // an army closes up to its own front, not to an ally's
    let open = false, walled = false;
    for (const j of r.nb) {
      const t = g.regions[j];
      if (!enemies.has(t.ctrl) || !atWar(g, nat, t.ctrl) || halt.has(t.name)) continue; // a sector under a halt order draws no one
      if (t.fort && t.fort.vs.includes(r.owner) && t.fort.lv >= 2) walled = true; else open = true;
    }
    if (open) atFront[r.id] = 1;
    // A stretch of front already packed with troops needs no more: the columns behind look further along.
    if ((open || walled) && !full(r)) { dist[r.id] = open ? 0 : 4; q.push(r.id); }
  }
  for (let i = 0; i < q.length; i++) {
    const a = q[i];
    // A region packed with troops cannot be marched through: the columns behind it go round.
    if (full(g.regions[a])) continue;
    for (const j of g.regions[a].nb) {
      if (!allied(g, nat, g.regions[j].ctrl)) continue;
      if (dist[a] + 1 < dist[j]) { dist[j] = dist[a] + 1; q.push(j); }
    }
  }
  const movers = g.corps
    .filter((c) => c.nat === nat && !c.hold && c.kind !== 'fort' && c.ap > 0)
    .sort((a, b) => KIND[a.kind].move - KIND[b.kind].move);
  // Two corps stay to screen any place that faces the enemy, even where the plan has called a halt.
  const screen = (c: Corps): boolean => {
    const here = g.regions[c.reg];
    return friendlyUnits(g, here, nat).length <= 2 && here.nb.some((j) => hostileUnits(g, g.regions[j], nat).length > 0);
  };
  for (const c of movers) {
    if (screen(c)) continue;
    if (door && KIND[c.kind].mobile && door[c.reg] > 0) {
      // Fast troops close up on the main thrust's next objective.
      for (let step = 0; step < 6 && c.ap > 0 && door[c.reg] > 0; step++) {
        let to: Region | null = null;
        for (const j of g.regions[c.reg].nb) {
          const r = g.regions[j];
          if (door[j] < 0 || door[j] >= door[c.reg] || friendlyUnits(g, r, nat).length >= STACK) continue;
          if (!to || door[j] < door[to.id]) to = r;
        }
        if (!to || (wave > 0 && weekOf(c, moveCost(g, c, to)) > wave) || !moveCorps(g, c, to)) break;
        made++;
      }
      continue;
    }
    if (wave <= 1 && c.ap >= 4 && dist[c.reg] >= 3 && dist[c.reg] < 1e9) {
      // Far behind the front: go by rail to the nearest railhead that has been in friendly hands a while.
      let to: Region | null = null, bd = dist[c.reg] - 1, bu = 99;
      for (const r of g.regions) {
        if (r.since >= g.turn || dist[r.id] > bd || !allied(g, nat, r.ctrl)) continue;
        const u = friendlyUnits(g, r, nat).length;
        if (u >= STACK) continue;
        if (dist[r.id] < bd || u < bu) { to = r; bd = dist[r.id]; bu = u; }
      }
      if (to && railMove(g, c, to)) { made++; continue; }
    }
    for (let step = 0; step < 6 && c.ap > 0; step++) {
      const here = c.reg;
      if (atFront[here] || dist[here] >= 1e9) break;
      let to: Region | null = null, bd = dist[here], bu = 99;
      for (const j of g.regions[here].nb) {
        const r = g.regions[j];
        if (!allied(g, nat, r.ctrl) || dist[j] >= bd + (to ? 0.001 : 0)) continue;
        const u = friendlyUnits(g, r, nat).length;
        if (u >= STACK) continue;
        if (dist[j] < bd || u < bu) { to = r; bd = dist[j]; bu = u; }
      }
      if (!to || (wave > 0 && weekOf(c, moveCost(g, c, to)) > wave) || !moveCorps(g, c, to)) break;
      made++;
    }
  }
  return made;
}

// ---- Redeploy -------------------------------------------------------------------------

function redeploy(g: Game, nat: string, ph: PlanPhase, enemies: Set<string>): void {
  const engaged = (c: Corps): boolean => g.regions[c.reg].nb.some((j) => enemies.has(g.regions[j].ctrl) && atWar(g, nat, g.regions[j].ctrl));
  const free = (): Corps[] => g.corps.filter((c) => c.nat === nat && !c.hold && c.kind !== 'fort' && c.ap >= 4 && !engaged(c));
  const room = (r: Region): boolean => allied(g, nat, r.ctrl) && friendlyUnits(g, r, nat).length < STACK;
  const nearest = (list: Corps[], to: Region): Corps | null => {
    const d = hops(g, to.id, (r) => allied(g, nat, r.ctrl));
    let best: Corps | null = null, bd = 1e9;
    for (const c of list) if (d[c.reg] >= 0 && d[c.reg] < bd) { bd = d[c.reg]; best = c; }
    return best;
  };

  // Occupation troops for what has been taken.
  for (const name of ph.garrison ?? []) {
    const r = g.byName[name];
    if (!r || r.ctrl !== nat || r.units.some((u) => u.nat === nat && u.hold)) continue;
    const c = nearest(free().filter((c) => c.kind === 'inf'), r);
    if (!c) continue;
    if (c.reg !== r.id && !railMove(g, c, r)) continue;
    c.hold = true;
  }
  // Small forces kept at a jumping-off point for a later operation.
  const staged = new Set<number>();
  for (const [name, count] of Object.entries(ph.staging ?? {})) {
    const r = g.byName[name];
    if (!r || !allied(g, nat, r.ctrl)) continue;
    const there = r.units.filter((u) => u.nat === nat && !u.hold);
    there.forEach((u) => staged.add(u.id));
    for (let k = there.length; k < count; k++) {
      const c = nearest(free().filter((c) => c.kind === 'inf' && !staged.has(c.id)), r);
      if (!c || !railMove(g, c, r)) break;
      staged.add(c.id);
    }
  }
  const armour = new Set(ph.armour ?? []);
  const assembly = ph.assembly ?? [];
  for (const c of free()) {
    if (staged.has(c.id)) continue;
    const here = g.regions[c.reg].name;
    const mobile = KIND[c.kind].mobile;
    if (mobile && armour.has(here)) continue;
    if (!mobile && assembly.includes(here) && !armour.has(here)) continue;
    let to: Region | null = null;
    if (mobile) to = (ph.armour ?? []).map((nm) => g.byName[nm]).find((r) => r && room(r)) ?? null;
    if (!to) {
      // Infantry leaves the armour's assembly areas free while there is room elsewhere.
      const open = assembly.map((nm) => g.byName[nm]).filter((r) => r && room(r));
      const pref = mobile ? open : open.filter((r) => !armour.has(r.name)).concat(open.filter((r) => armour.has(r.name)));
      if (!mobile && assembly.includes(here)) continue;
      let bu = 99;
      for (const r of pref) {
        const u = friendlyUnits(g, r, nat).length + (!mobile && armour.has(r.name) ? 10 : 0);
        if (u < bu) { bu = u; to = r; }
      }
    }
    if (to) railMove(g, c, to);
  }
}

// ---- Defend ---------------------------------------------------------------------------

function defPower(g: Game, c: Corps, r: Region): number {
  return corpsBase(g, c) * KIND[c.kind].def * TERRAIN_DEF[r.terrain];
}

function defend(g: Game, nat: string): void {
  ensureSupply(g);
  const n = g.nations[nat];
  if (n.seaSupply) evacuations(g, nat);

  // Where is the danger, and how well is each place held?
  const N = g.regions.length;
  const threat = new Float32Array(N), held = new Float32Array(N);
  // What this nation has to hold: its own country, and whatever it has taken and occupies.
  const mine = (r: Region): boolean => r.ctrl === nat || (r.owner === nat && allied(g, nat, r.ctrl));
  for (const r of g.regions) {
    if (!mine(r)) continue;
    for (const j of r.nb) {
      const e = g.regions[j];
      if (!atWar(g, nat, e.ctrl)) continue;
      const foes = hostileUnits(g, e, nat).filter((u) => u.kind !== 'fort').map((u) => atkPower(g, u)).sort((a, b) => b - a);
      for (const p of foes.slice(0, frontage(r))) threat[r.id] += p;
    }
    for (const u of friendlyUnits(g, r, nat)) held[r.id] += defPower(g, u, r);
  }
  const importance = (r: Region): number => 1 + 0.5 * r.vp + (r.cap ? 4 : 0);
  const need = (r: Region): number => importance(r) * Math.max(0, 1.3 * threat[r.id] - held[r.id]);

  // Ground worth fighting to get back: the nation's own, its allies', and what it held until the enemy took it.
  const lost = (r: Region): boolean => allied(g, nat, r.owner) || r.prev === nat;
  // A slow command needs a month to organise any answer to ground it has just lost.
  const late = (r: Region): boolean => n.slowCommand && r.since >= g.turn;
  const movers = g.corps.filter((c) => c.nat === nat && !c.hold && !c.dyle && c.kind !== 'fort' && c.ap > 0);
  for (const c of movers) {
    if (c.dead || c.ap <= 0) continue;
    const here = g.regions[c.reg];
    if (!mine(here)) continue; // abroad on an ally's ground: stand where the plan put it

    // Step back into ground the enemy has left empty behind his spearheads.
    {
      const others = friendlyUnits(g, here, nat).length > 1 || threat[here.id] === 0;
      const gap = here.nb.map((j) => g.regions[j]).find((r) => atWar(g, nat, r.ctrl) && lost(r) && !late(r) && !hostileUnits(g, r, nat).length && c.ap >= Math.min(moveCost(g, c, r), 4));
      if (gap && others && moveCorps(g, c, gap)) continue;
    }
    let spared = 0;
    if (threat[here.id] > 0) {
      // In the line. It can be spared only if the place stays well held without it.
      spared = defPower(g, c, here);
      if (friendlyUnits(g, here, nat).length <= 2 || held[here.id] - spared < 1.6 * threat[here.id]) continue;
    }

    // A reserve: go where the line is thinnest for what it protects.
    const d = hops(g, here.id, (r) => allied(g, nat, r.ctrl));
    let to: Region | null = null, bs = 0;
    for (const r of g.regions) {
      if (!mine(r) || d[r.id] <= 0 || friendlyUnits(g, r, nat).length >= STACK) continue;
      const s = need(r) / (1 + 0.5 * d[r.id]);
      if (s > bs) { bs = s; to = r; }
    }
    if (!to) continue;
    const before = c.reg;
    if (!n.slowCommand && d[to.id] > 2 && c.ap >= 4) railMove(g, c, to);
    else walkToward(g, c, to);
    if (c.reg !== before) { held[here.id] -= spared; held[to.id] += defPower(g, c, to); }
  }

  // Counter-attack where the odds are clearly good.
  for (const r of g.regions) {
    if (!n.alive) return;
    if (!atWar(g, nat, r.ctrl) || !lost(r) || late(r) || !hostileUnits(g, r, nat).length) continue;
    // Troops that have already fought this month are in no state to be thrown straight into another attack.
    const pool = limitAttackers(g, g.corps.filter((c) => c.nat === nat && !c.hold && c.kind !== 'fort' && c.ap > 0 && !c.fought && c.org >= 0.6 && g.regions[c.reg].nb.includes(r.id)), r);
    if (pool.length && assess(g, pool, r).odds >= 1.8) resolveBattle(g, pool, r);
  }
}

function walkToward(g: Game, c: Corps, to: Region): void {
  // The way there, going round any region too packed with troops to march through.
  const d = hops(g, to.id, (r) => allied(g, c.nat, r.ctrl) && (r.id === c.reg || friendlyUnits(g, r, c.nat).length < STACK));
  for (let step = 0; step < 6 && c.ap > 0 && c.reg !== to.id; step++) {
    let next: Region | null = null;
    for (const j of g.regions[c.reg].nb) {
      if (d[j] < 0 || d[j] >= d[c.reg]) continue;
      const r = g.regions[j];
      if (friendlyUnits(g, r, c.nat).length >= STACK) continue;
      if (!next || d[j] < d[next.id]) next = r;
    }
    if (!next || !moveCorps(g, c, next)) return;
  }
}

/** An expeditionary corps goes home when it is cut off, broken, or its host is collapsing. */
function wantsOut(g: Game, c: Corps): boolean {
  if (g.regions[c.reg].owner === c.nat || c.dead) return false;
  // The host is the biggest continental ally. If it has fallen, there is nothing left to stay for.
  let host: string | null = null, size = 0;
  for (const n of Object.values(g.nations)) {
    if (n.id === c.nat || n.dormant || !allied(g, c.nat, n.id) || n.capital < 0 || n.seaSupply) continue;
    if (n.armyPeak > size) { size = n.armyPeak; host = n.id; }
  }
  if (!c.supplied || c.org < 0.45 || !host || !g.nations[host].alive) return true;
  const H = g.nations[host];
  const d = hops(g, c.reg, (r) => allied(g, c.nat, r.ctrl));
  return d[H.capital] < 0 || collapseScore(g, host) >= 0.7 * H.resilience;
}

function evacuations(g: Game, nat: string): void {
  ensureSupply(g);
  const leaving = g.corps.filter((c) => c.nat === nat && wantsOut(g, c));
  if (!leaving.length) return;
  const stuck: Corps[] = [];
  for (const c of leaving) {
    if (!g.regions[c.reg].port) {
      const d = hops(g, c.reg, (r) => allied(g, c.nat, r.ctrl));
      let port: Region | null = null;
      for (const r of g.regions) {
        if (!r.port || r.owner === c.nat || d[r.id] < 0 || friendlyUnits(g, r, c.nat).length >= STACK) continue;
        if (!port || d[r.id] < d[port.id]) port = r;
      }
      if (port) walkToward(g, c, port);
    }
    if (g.regions[c.reg].port) evacuate(g, c); else stuck.push(c);
  }
  // No way to the sea: fight a way through to the nearest port.
  for (let tries = 0; tries < 3; tries++) {
    const trapped = stuck.filter((c) => !c.dead && c.ap > 0 && g.regions[c.reg].owner !== nat);
    if (!trapped.length) return;
    let goal: Region | null = null, most = 0;
    for (const r of g.regions) {
      if (!r.port || !atWar(g, nat, r.ctrl)) continue;
      const k = trapped.filter((c) => g.regions[c.reg].nb.includes(r.id)).length;
      if (k > most) { most = k; goal = r; }
    }
    if (!goal) return;
    const force = limitAttackers(g, trapped.filter((c) => g.regions[c.reg].nb.includes(goal!.id)), goal);
    if (hostileUnits(g, goal, nat).length) {
      if (assess(g, force, goal).odds < 1) return;
      if (!resolveBattle(g, force, goal).taken) return;
    } else if (!moveCorps(g, force[0], goal)) return;
    for (const c of trapped) {
      if (c.dead) continue;
      if (c.reg !== goal.id && g.regions[c.reg].nb.includes(goal.id)) moveCorps(g, c, goal);
      if (c.reg === goal.id) evacuate(g, c);
    }
  }
}
