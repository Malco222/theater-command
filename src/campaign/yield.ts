// The yield form (the design director's 05:30Z, 06:41Z, 07:20Z and 07:28Z of 22 November 2026, on the historian's
// historian/capitulations-1939-45.md; the rules builder's paragraph, rules-builder/release2/yield-form-paragraph.md).
//
// Release 1 has one way for a country to fall: its losses weighed together pass what it will bear (game.ts
// collapseScore and checkCollapse). Nothing there asks whether its army is beaten or whether help can reach it, so a
// country beaten in the field holds on for months behind one town's walls (the Netherlands, 0-2 months late in every
// war). The record says otherwise: countries yielded when their field army was beaten and no relief was in reach, and
// the capital mattered less. This is a second way in, beside the first, which stays as it is.
//
// - Stops 3-5, and the player's own country at every stop: by cause, checked as the country's month opens. (i) Its
//   field army is beaten: no corps of its own stands in the field on the home chart in supply. (ii) No ally's relief is
//   in reach: no ally at war with its enemies has corps in supply that could attack the enemy's army on its home ground this
//   month or next (beside it, by rail, by a march of a month, or by sea) at the odds the computer itself asks before it
//   goes to a relief (ai.ts reliefBar, read through assess). A relief that cannot win is not in reach.
// - Stops 1-2, a computer-run country: the record's month, as months after the first blow of the enemy it yields to
//   (data/campaign/capitulations.json). Where that enemy's blow is not dated, or the theatre's book is closed, the row
//   lapses to the cause.
// - The player's country is told the month the cause is met (a red band, ruling 133(a)) and yields at the next
//   opening if it still holds. The cause is the same; the month is the warning before an end he cannot undo.
//
// What yielding does is today's fall (game.ts capitulate): its towns, garrisons and fortress rows go with it, its fleets
// are laid up and its air groups go. Behind its own switch (stage.ts YIELD_STAGE, the flag set as the campaign opens);
// off, nothing here is read and no flag is written.

import data from '../../data/campaign/capitulations.json';
import { reliefBar } from './ai';
import { skyDenies } from './air';
import { allied, annexMonth, assess, atWar, capitulate, corpsBase, ensureSupply, fights, friendlyUnits, gunsRule, headline, homeOf, hostileUnits, limitAttackers, overseasRead, railReachFor, theName, thornGround, victorOf } from './game';
import { FIT_ORG } from './staff';
import { stopDef, stopOf } from './history';
import { whyBarred } from './ground';
import { blocked } from './rivers';
import { lineShut } from './line';
import { standsFreed } from './liberation';
import { farGround, marchMonths } from './marches';
import { fieldCtrl } from './siege';
import { NAVY, landingOpts, seaPath, surpriseOn } from './navy';
import { prizeOut } from './prizes';
import { KIND, STACK, type Corps, type Game, type Region } from './types';

/** [nation, the enemy it yields to, months from that enemy's first blow to the yield, the book's theatre or null, mark]. */
export type YieldRow = [string, string, number, string | null, string];
export const YIELD_ROWS: YieldRow[] = (data as unknown as { rows: YieldRow[] }).rows;

/** The switch, as the campaign opened: the grand campaign only. */
export const yieldRule = (g: Game): boolean => g.flags.open === 1 && g.flags.yieldRule === 1;

/** How far a road march may start from the place and still arrive within a month: no corps marches further. */
const ROAD_HOPS = 6;
/** How far a train may bring a corps in a month, in provinces: the search for rail help goes no further. */
const RAIL_HOPS = 16;

/** A corps standing in the field of a province on the home chart, on its side's ground (not inside a town, not at sea, not overseas). */
function inField(g: Game, c: Corps): boolean {
  const r = g.regions[c.reg];
  return !!r && !overseasRead(g, r) && r.units.includes(c) && allied(g, c.nat, fieldCtrl(g, r));
}

/** Its own home ground, as the score reads it (game.ts homeLost, with the yield form's corrections: homeOf). */
const onHome = (g: Game, nat: string, c: Corps): boolean => { const r = g.regions[c.reg]; return !!r && homeOf(g, r) === nat; };

/**
 * (i) The country's field army (the design director's 09:04Z and 09:05Z): its own corps in the field on its own home
 * ground (the home the score counts), in supply, not fortress crews, not inside a town and not on leave, that fight
 * (fights(), ruling 146: not guns or engineers alone) and stand at or above the staffs' own fit line (staff.ts FIT_ORG).
 * Beaten is none of them left, and none of its own off its home that could strike the enemy on its home ground as an
 * ally's relief could (ownRelief). No new number.
 */
export function fieldArmy(g: Game, nat: string): Corps[] {
  // (The supply picture as it stands now: a corps raised or moved since it was last drawn reads as fed until it is.)
  ensureSupply(g);
  return g.corps.filter((c) => !c.dead && c.nat === nat && c.kind !== 'fort' && c.town === undefined && c.leave === undefined && c.supplied && fights(c) && c.org >= FIT_ORG && onHome(g, nat, c) && inField(g, c));
}
/** (i) off its home: its own corps there count only as a relief, by (ii)'s own ways and bar: one rule for its own and its allies'. */
export function ownRelief(g: Game, nat: string): Relief | null {
  return reliefFrom(g, nat, [nat], (c) => !onHome(g, nat, c));
}
export const fieldBeaten = (g: Game, nat: string): boolean => fieldArmy(g, nat).length === 0 && !ownRelief(g, nat);

/** The country's home ground where the enemy's army stands, holding it or round its town: where a relief would strike. */
function enemyOnHome(g: Game, nat: string, foes: Set<string>): Region[] {
  return g.regions.filter((r) => homeOf(g, r) === nat && r.units.some((u) => !u.dead && foes.has(u.nat)));
}

/** Provinces within `most` steps of the place, by land, with their distance. */
function stepsFrom(g: Game, from: number, most: number): Map<number, number> {
  const d = new Map<number, number>([[from, 0]]);
  const q = [from];
  for (let i = 0; i < q.length; i++) {
    const k = d.get(q[i])!;
    if (k >= most) continue;
    for (const j of g.regions[q[i]].nb) if (!d.has(j)) { d.set(j, k + 1); q.push(j); }
  }
  return d;
}

/**
 * How a corps of an ally could come to strike the place. Where it stands on the place, or beside it with nothing
 * between, it attacks from there (stay). Otherwise every way it has is kept, and which it takes is the search's
 * (reliefFrom): the provinces of its side beside the place, clear of the enemy, that a train takes it to this month or
 * a month's march brings it to over friendly ground (land); the harbours of its side beside the place that a sailing
 * from its own harbour puts it in at (sea: navy.ts whyNotShip's tests but room, which the search counts); and a landing
 * on the place itself from that harbour, where the place is the enemy's (whyNotLand's tests for the corps: the ground
 * not barred to it, the book's line open, the sea's path within a landing's range, the sky not denied). Moves already
 * spent and shipping already in use this month do not count against it: the question is next month.
 */
type Ways = { stay?: number; land: number[]; sea: number[]; landing?: number };

/** A corps' moves as if its month were whole, while `f` asks where it could go; its own moves are put back after. */
function fresh<T>(c: Corps, f: () => T): T {
  const had = c.ap;
  c.ap = Math.max(c.ap, c.apMax ?? c.ap);
  try { return f(); } finally { c.ap = had; }
}

function canCome(g: Game, c: Corps, t: Region, steps: Map<number, number>, kept: { rails: Map<number, Set<number>>; ground: Map<number, Map<number, { cost: number }>> }): Ways | null {
  const at = g.regions[c.reg];
  // (Where it stands on the place, or beside it with nothing between, it attacks from there; across a blown river it
  // looks for another bank like a corps further off.)
  if (at.id === t.id || (t.nb.includes(at.id) && !blocked(g, c, t))) return { stay: at.id, land: [], sea: [] };
  // (Ground of its side beside the place, clear of the enemy's army: somewhere to stand and attack from.)
  const beside = t.nb.map((j) => g.regions[j]).filter((p) => p.id !== at.id && allied(g, c.nat, p.ctrl) && !hostileUnits(g, p, c.nat).length);
  const land = new Set<number>(), sea = new Set<number>();
  let landing: number | undefined;
  const hops = steps.get(at.id);
  // By rail this month.
  if (hops !== undefined && hops <= RAIL_HOPS && beside.length) {
    let r = kept.rails.get(c.id);
    if (!r) { r = fresh(c, () => railReachFor(g, [c])); kept.rails.set(c.id, r); }
    for (const p of beside) if (r.has(p.id) && !blocked(g, c, t, p.id)) land.add(p.id);
  }
  // By road, a month's march over friendly ground (marches.ts farGround: the march a staff could order, whoever's month it is).
  if (hops !== undefined && hops <= ROAD_HOPS && beside.length) {
    let m = kept.ground.get(c.id);
    if (!m) { m = farGround(g, c); kept.ground.set(c.id, m); }
    for (const p of beside) { const e = m.get(p.id); if (e && marchMonths(c, e.cost) <= 1 && !blocked(g, c, t, p.id)) land.add(p.id); }
  }
  const n = g.nations[c.nat];
  if (at.port && allied(g, c.nat, at.ctrl) && n.sealift > 0 && c.kind !== 'fort' && !(prizeOut(g, at, 'harbour') > 0)) {
    // By ship, into a harbour of its side beside the place (room is the search's to count, as the moves would find it).
    for (const p of beside) {
      if (!p.port || thornGround(g, p, c.nat) || lineShut(g, c.nat, p) || prizeOut(g, p, 'harbour') > 0 || whyBarred(g, c, p)) continue;
      if (seaPath(g, c.nat, at, p) && !blocked(g, c, t, p.id)) sea.add(p.id);
    }
    // A landing on the place itself, where it is the enemy's.
    if (t.seas.length && atWar(g, c.nat, t.ctrl) && !allied(g, c.nat, t.ctrl) && !lineShut(g, c.nat, t) && !whyBarred(g, c, t)) {
      const surprise = surpriseOn(g, c.nat, t);
      if (seaPath(g, c.nat, at, t, { max: NAVY.landing.range, surprise }) && (surprise || !skyDenies(g, c.nat, t.id))) landing = at.id;
    }
  }
  if (!land.size && !sea.size && landing === undefined) return null;
  return { land: [...land].sort((a, b) => a - b), sea: [...sea].sort((a, b) => a - b), landing };
}

/** A relief in reach: who brings it, where it would strike, and the odds it would go in at. */
export interface Relief { by: string; target: number; odds: number; corps: number[] }

/** A corps' weight in an attack, as limitAttackers ranks it. */
const weight = (g: Game, c: Corps): number => corpsBase(g, c) * KIND[c.kind].atk;

/** A province beside the place to strike from: reached by land, or put in at by the sailing from harbour `quay`. */
type Way = { to: number; quay?: number };
/** A corps that could come, with its ways. */
type Mover = { c: Corps; ways: Way[] };
/** Where the corps that move would stand: corps id to its way. */
type Plan = Map<number, Way>;
/** A force weighed: the corps limitAttackers sends from where they would stand, and its odds. */
type Weighed = { odds: number; force: Corps[] };

/**
 * The most forces one search weighs (one ally, one place), and the most placings it tries. A search that reaches
 * either ends with the best it has found, and is counted in `searchCut` (no silent cap; in the wars measured none was).
 */
const SEARCH = { weighed: 3000, tried: 40000 };
export const searchCut = { n: 0 };

/**
 * Of corps alike in kind, nation and ways, the strongest are kept: enough to fill every province they could stand on
 * (STACK each). One weaker in all of that can do nothing the kept ones cannot do as well.
 */
function alike(g: Game, ms: Mover[]): Mover[] {
  const groups = new Map<string, Mover[]>();
  for (const m of ms) {
    const key = `${m.c.nat}|${m.c.kind}|${m.ways.map((w) => `${w.to}:${w.quay ?? ''}`).join(',')}`;
    const xs = groups.get(key);
    if (xs) xs.push(m); else groups.set(key, [m]);
  }
  const out: Mover[] = [];
  for (const xs of groups.values()) {
    const room = STACK * new Set(xs[0].ways.map((w) => w.to)).size;
    out.push(...xs.sort((a, b) => weight(g, b.c) - weight(g, a.c) || a.c.id - b.c.id).slice(0, room));
  }
  return out.sort((a, b) => weight(g, b.c) - weight(g, a.c) || a.c.id - b.c.id);
}

/**
 * The relief one ally could bring this month or next by land and by sea together, found by the odds themselves (the
 * design director's 12:20Z and 12:38Z, read as every move the month allows): the corps that stay where they stand, and
 * any of the others put where their ways take them, as the moves would allow it all at once:
 * - room as the moves count it: hasRoom's for a march (STACK corps, engineers apart up to 2), whyNotShip's for a
 *   sailing (STACK units of its side of every kind), its side's units there counted, those that would leave first;
 * - by sea, a corps each on the shipping in hand (sealift), and one sailing from each harbour, into one harbour of its
 *   side beside the place (the corps of one harbour go together);
 * - weighed as limitAttackers would send them from there (its roads, its sides, the guns rule) through assess.
 * The search adds, corps by corps, the move that raises the odds most. Where no single move raises them, it tries a
 * corps taken out, and a corps put in where another has made room, freed the shipping or turned its harbour's sailing,
 * that other put elsewhere if it can go; and a corps standing in the way moved off. It ends at the bar, or where no
 * such move raises the odds.
 */
function searchRelief(g: Game, ally: string, t: Region, bar: number, stays: Corps[], movers: Mover[], lift: number): Weighed | null {
  const byId = new Map(movers.map((m) => [m.c.id, m]));
  const home = new Map<number, Mover[]>();
  for (const m of movers) { const xs = home.get(m.c.reg); if (xs) xs.push(m); else home.set(m.c.reg, [m]); }
  const units = new Map<number, Corps[]>();
  const here = (id: number): Corps[] => { let u = units.get(id); if (!u) { u = friendlyUnits(g, g.regions[id], ally) as Corps[]; units.set(id, u); } return u; };
  const count = (p: Plan, id: number): { all: number; eng: number } => {
    let all = 0, eng = 0;
    for (const u of here(id)) if (!p.has(u.id)) { all++; if (u.kind === 'eng') eng++; }
    for (const [cid, w] of p) if (w.to === id) { all++; if (byId.get(cid)!.c.kind === 'eng') eng++; }
    return { all, eng };
  };
  const why = (p: Plan, m: Mover, w: Way): 'ok' | 'room' | 'lift' | 'quay' => {
    if (w.quay !== undefined) {
      let used = 0, dest: number | undefined;
      for (const x of p.values()) if (x.quay !== undefined) { used++; if (x.quay === w.quay) dest = x.to; }
      if (used >= lift) return 'lift';
      if (dest !== undefined && dest !== w.to) return 'quay';
    }
    const k = count(p, w.to);
    const fits = w.quay !== undefined ? k.all < STACK : m.c.kind === 'eng' ? k.eng < 2 : k.all - k.eng < STACK;
    return fits ? 'ok' : 'room';
  };
  const put = (p: Plan, id: number, w: Way): Plan => new Map(p).set(id, w);
  const out = (p: Plan, id: number): Plan => { const n = new Map(p); n.delete(id); return n; };
  const same = (a: Way, b: Way): boolean => a.to === b.to && a.quay === b.quay;
  let weighed = 0, tried = 0;
  const memo = new Map<string, number>();
  const weigh = (p: Plan): Weighed | null => {
    if (tried >= SEARCH.tried) return null;
    tried++;
    const pool = [...stays, ...[...p].map(([id, w]) => ({ ...byId.get(id)!.c, reg: w.to }))];
    const force = limitAttackers(g, pool, t);
    if (!force.length) return { odds: 0, force };
    const key = force.map((c) => `${c.id}@${c.reg}`).sort().join(' ');
    let odds = memo.get(key);
    if (odds === undefined) {
      if (weighed >= SEARCH.weighed) return null;
      weighed++;
      odds = assess(g, force, t).odds;
      memo.set(key, odds);
    }
    return { odds, force };
  };
  let plan: Plan = new Map();
  let cur = weigh(plan);
  if (!cur) return null;
  for (;;) {
    if (cur.odds >= bar) return cur;
    let best: { p: Plan; w: Weighed } | null = null;
    const tryPlan = (p: Plan): void => {
      const w = weigh(p);
      if (w && w.odds > (best ? best.w.odds : cur!.odds) + 1e-9) best = { p, w };
    };
    for (const m of movers) if (!plan.has(m.c.id)) for (const w of m.ways) if (why(plan, m, w) === 'ok') tryPlan(put(plan, m.c.id, w));
    if (!best) {
      // (No single move raises the odds.) A corps taken out: one that pushes a better one off the frontage.
      for (const id of plan.keys()) tryPlan(out(plan, id));
      for (const m of movers) {
        if (plan.has(m.c.id)) continue;
        for (const w of m.ways) {
          const y = why(plan, m, w);
          if (y === 'ok') continue;
          // Its harbour's sailing turned: the corps already put in from that harbour go where this one would.
          if (y === 'quay') {
            let n: Plan | null = new Map(plan);
            const mates = [...plan].filter(([, x]) => x.quay === w.quay);
            for (const [id] of mates) n.delete(id);
            for (const [id] of mates) { if (n && why(n, byId.get(id)!, { to: w.to, quay: w.quay }) === 'ok') n = put(n, id, { to: w.to, quay: w.quay }); else n = null; }
            if (n && why(n, m, w) === 'ok') tryPlan(put(n, m.c.id, w));
          }
          // A corps standing in the way, not yet moving, moved off to make room.
          if (y === 'room') {
            for (const b of home.get(w.to) ?? []) {
              if (b === m || plan.has(b.c.id)) continue;
              for (const bw of b.ways) {
                if (why(plan, b, bw) !== 'ok') continue;
                const n = put(plan, b.c.id, bw);
                if (why(n, m, w) === 'ok') tryPlan(put(n, m.c.id, w));
              }
            }
          }
          // A corps put in where another, taken out, has made room, freed the shipping or turned the sailing; the other
          // put elsewhere if it can go.
          for (const [id, was] of plan) {
            const n0 = out(plan, id);
            if (why(n0, m, w) !== 'ok') continue;
            const n1 = put(n0, m.c.id, w);
            tryPlan(n1);
            const x = byId.get(id)!;
            for (const xw of x.ways) if (!same(xw, was) && why(n1, x, xw) === 'ok') tryPlan(put(n1, id, xw));
          }
        }
      }
    }
    if (!best) break;
    const b = best as { p: Plan; w: Weighed };
    plan = b.p;
    cur = b.w;
  }
  if (weighed >= SEARCH.weighed || tried >= SEARCH.tried) searchCut.n++;
  return cur;
}

/**
 * A landing on the place from one port (whyNotLand's force): no more corps than the shipping carries nor than STACK,
 * at a landing's strength (landingOpts), the guns never alone. The corps are found by the odds as the search above
 * finds them: the one that raises them most added, one swapped for another, one taken out, until none does.
 */
function landingRelief(g: Game, cs: Corps[], t: Region, most: number): Weighed | null {
  if (most <= 0 || !cs.length) return null;
  // (Of corps alike in kind, the strongest `most`: a weaker one does nothing a kept one cannot.)
  const kinds = new Map<string, Corps[]>();
  for (const c of cs) { const xs = kinds.get(c.kind); if (xs) xs.push(c); else kinds.set(c.kind, [c]); }
  const pool = [...kinds.values()].flatMap((xs) => xs.sort((a, b) => weight(g, b) - weight(g, a) || a.id - b.id).slice(0, most));
  const oddsOf = (xs: Corps[]): number => (!xs.length || (gunsRule(g) && !xs.some(fights)) ? 0 : assess(g, xs, t, landingOpts(g, xs, t)).odds);
  let group: Corps[] = [], odds = 0;
  for (let k = 0; k < 4 * pool.length + 8; k++) {
    let next: Corps[] | null = null, o = odds;
    const tryGroup = (xs: Corps[]): void => { const v = oddsOf(xs); if (v > o + 1e-9) { o = v; next = xs; } };
    for (const c of pool) {
      if (group.includes(c)) continue;
      if (group.length < most) tryGroup([...group, c]);
      for (let i = 0; i < group.length; i++) tryGroup(group.map((x, j) => (j === i ? c : x)));
    }
    for (let i = 0; i < group.length; i++) tryGroup(group.filter((_, j) => j !== i));
    if (!next) break;
    group = next;
    odds = o;
  }
  return group.length ? { odds, force: group } : null;
}

/**
 * (ii) An ally's relief in reach of the country, or null. Each ally at war with its enemies is reckoned on its own: its
 * corps in supply in the field that could attack a province of the country's home ground where the enemy's army
 * stands, this month or next, weighed as the computer weighs a relief (assess, the garrison's sortie included where the
 * place is a besieged town) and sent in as it would send them: by land and by sea together, every move the month
 * allows (searchRelief: room as the moves count it, a corps each on the shipping in hand, one sailing from each harbour
 * into one harbour of its side beside the place; the design director's 12:20Z and 12:38Z), from the provinces they
 * would attack from (limitAttackers); or as a landing on the place from one port, no more than its shipping carries
 * nor than STACK, at a landing's strength (landingOpts). In reach when the odds reach the bar the computer asks before
 * it goes to a relief: 1.25 at a capital or great city, 1.5 elsewhere (reliefBar).
 */
export function reliefInReach(g: Game, nat: string): Relief | null {
  const foes = new Set(g.nations[nat].atWar.filter((e) => g.nations[e]?.alive));
  if (!foes.size) return null;
  const friends = Object.values(g.nations).filter((n) => n.alive && n.id !== nat && allied(g, n.id, nat) && n.atWar.some((e) => foes.has(e))).map((n) => n.id);
  return reliefFrom(g, nat, friends, () => true);
}

/** A relief of these countries' corps (those `may` lets count), reckoned country by country as reliefInReach describes. */
function reliefFrom(g: Game, nat: string, friends: string[], may: (c: Corps) => boolean): Relief | null {
  const foes = new Set(g.nations[nat].atWar.filter((e) => g.nations[e]?.alive));
  if (!foes.size || !friends.length) return null;
  const targets = enemyOnHome(g, nat, foes);
  if (!targets.length) return null;
  // (Drawn before the pool is chosen: weighing the first ally's odds would otherwise redraw it halfway through.)
  ensureSupply(g);
  const free = g.corps.filter((c) => !c.dead && friends.includes(c.nat) && c.kind !== 'fort' && c.town === undefined && c.leave === undefined && c.supplied && g.regions[c.reg]?.units.includes(c) && may(c));
  if (!free.length) return null;
  const kept = { rails: new Map<number, Set<number>>(), ground: new Map<number, Map<number, { cost: number }>>() };
  // (The places where the enemy stands weakest first: where a relief is in reach it is most likely there, and the
  // answer is the same whichever place is weighed first.)
  const held = (r: Region): number => r.units.reduce((s, u) => s + (!u.dead && foes.has(u.nat) ? u.divs * u.str : 0), 0);
  for (const t of [...targets].sort((a, b) => held(a) - held(b) || a.id - b.id)) {
    const steps = stepsFrom(g, t.id, RAIL_HOPS);
    // (The enemy there: whose army stands on it.)
    const foe = t.units.find((u) => !u.dead && foes.has(u.nat))?.nat;
    if (!foe) continue;
    const bar = reliefBar(g, t);
    for (const ally of friends) {
      if (!atWar(g, ally, foe)) continue;
      const lift = g.nations[ally].sealift;
      const stays: Corps[] = [], movers: Mover[] = [], ports = new Map<number, Corps[]>();
      for (const c of free) {
        if (c.nat !== ally) continue;
        const how = canCome(g, c, t, steps, kept);
        if (!how) continue;
        if (how.stay !== undefined) { stays.push(c); continue; }
        // (Shipped, it sails from the harbour it stands in: its sailing is that harbour's.)
        const ways: Way[] = [...how.land.map((to) => ({ to })), ...(lift > 0 ? how.sea.map((to) => ({ to, quay: c.reg })) : [])];
        if (ways.length) movers.push({ c, ways });
        if (how.landing !== undefined) { const xs = ports.get(how.landing); if (xs) xs.push(c); else ports.set(how.landing, [c]); }
      }
      if (stays.length || movers.length) {
        const w = searchRelief(g, ally, t, bar, stays, alike(g, movers), lift);
        if (w && w.odds >= bar) return { by: ally, target: t.id, odds: w.odds, corps: w.force.map((c) => c.id) };
      }
      // Or a landing on the place from one port.
      for (const [, cs] of [...ports].sort(([a], [b]) => a - b)) {
        const w = landingRelief(g, cs, t, Math.min(lift, STACK));
        if (w && w.odds >= bar) return { by: ally, target: t.id, odds: w.odds, corps: w.force.map((c) => c.id) };
      }
    }
  }
  return null;
}

/**
 * The first blow of each enemy on the country's home ground, noted as its month opens: the month of the first battle
 * it fought there (a battle lost counts), or of the first month its army stood there or held ground there.
 */
function noteBlows(g: Game, nat: string): void {
  const n = g.nations[nat];
  for (const e of n.atWar) {
    const key = `blow:${nat}:${e}`;
    if (g.flags[key] !== undefined) continue;
    let at: number | undefined;
    for (let i = g.reports.length - 1; i >= 0; i--) {
      const rep = g.reports[i];
      if (rep.turn < g.turn - 1) break;
      const r = g.regions[rep.region];
      if (rep.attNat === e && r && homeOf(g, r) === nat) at = Math.min(at ?? rep.turn, rep.turn);
    }
    // (With no battle to date it: an enemy that plays after the country in the month came in its own phase of the last.)
    if (at === undefined && g.regions.some((r) => homeOf(g, r) === nat && (r.ctrl === e || r.units.some((u) => !u.dead && u.nat === e)))) at = g.order.indexOf(e) > g.order.indexOf(nat) ? g.turn - 1 : g.turn;
    if (at !== undefined) g.flags[key] = at;
  }
}

/** The row of the record for a country yielding to this enemy, where the book still stands over it. */
function bookRow(g: Game, nat: string, by: string): YieldRow | undefined {
  const row = YIELD_ROWS.find((r) => r[0] === nat && r[1] === by);
  if (!row) return undefined;
  return row[3] === null || !g.flags[`hist:off:${row[3]}`] ? row : undefined;
}

/** The cause, as it stands: who the country would yield to, or null where its army is not beaten or a relief is in reach. */
export function yieldCause(g: Game, nat: string): string | null {
  const by = victorOf(g, nat);
  if (!by || !fieldBeaten(g, nat) || reliefInReach(g, nat)) return null;
  return by;
}

/** The player's band (danger.ts): his country met the cause as this month opened, and yields at the next if it holds. */
export const yieldBand = (g: Game, nat: string): boolean => yieldRule(g) && g.flags[`yield:${nat}`] === g.turn;

/**
 * A country's month opening under the yield form (turn.ts, after the month's own opening and the fall's check). A
 * computer-run country at stops 1-2 yields in the record's month where its row stands; otherwise by cause, at once for
 * a computer-run country and at the next opening for the player's.
 */
export function yieldMonth(g: Game, nat: string): void {
  if (!yieldRule(g)) return;
  // (The record's formal annexations, fixed as their dates come: game.ts annexMonth, read by homeOf.)
  annexMonth(g);
  const n = g.nations[nat];
  if (!n?.alive || n.capital < 0) return;
  noteBlows(g, nat);
  const key = `yield:${nat}`;
  // A country that has been liberated stands for as long as its capital does, as it does against the fall.
  if (standsFreed(g, n)) { delete g.flags[key]; return; }
  const mine = nat === g.human;
  // (The record's row is the first fall's. A country liberated since falls again by the cause.)
  if (!mine && !n.freedOn && stopOf(g) <= 2 && stopDef(g).book) {
    const by = victorOf(g, nat);
    const row = by ? bookRow(g, nat, by) : undefined;
    if (by && row) {
      const blow = g.flags[`blow:${nat}:${by}`];
      if (blow !== undefined && g.turn >= blow + row[2]) capitulate(g, nat, by);
      return;
    }
  }
  const by = yieldCause(g, nat);
  if (!by) { delete g.flags[key]; return; }
  if (mine && g.flags[key] !== g.turn - 1) { g.flags[key] = g.turn; return; }
  delete g.flags[key];
  headline(g, 'event', `The ${n.adj} field army is beaten, and no ally can come to ${theName(n)} in time.`, nat);
  capitulate(g, nat, by);
}
