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
import { allied, assess, atWar, capitulate, corpsBase, ensureSupply, fights, friendlyUnits, gunsRule, headline, hostileUnits, limitAttackers, railReachFor, theName, thornGround, victorOf } from './game';
import { stopDef, stopOf } from './history';
import { whyBarred } from './ground';
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
  return !!r && !r.overseas && r.units.includes(c) && allied(g, c.nat, fieldCtrl(g, r));
}

/**
 * (i) The country's field army: its own corps in the field on the home chart, in supply, that are not fortress crews
 * and not on leave. Beaten is none of them left: gone, shut in towns, or every one cut off. No new number.
 */
export function fieldArmy(g: Game, nat: string): Corps[] {
  // (The supply picture as it stands now: a corps raised or moved since it was last drawn reads as fed until it is.)
  ensureSupply(g);
  return g.corps.filter((c) => !c.dead && c.nat === nat && c.kind !== 'fort' && c.town === undefined && c.leave === undefined && c.supplied && inField(g, c));
}
export const fieldBeaten = (g: Game, nat: string): boolean => fieldArmy(g, nat).length === 0;

/** The country's home ground where the enemy's army stands, holding it or round its town: where a relief would strike. */
function enemyOnHome(g: Game, nat: string, foes: Set<string>): Region[] {
  return g.regions.filter((r) => r.owner === nat && !r.overseas && r.units.some((u) => !u.dead && foes.has(u.nat)));
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

/** How an ally's corps could come to strike the place: the provinces it could attack from, or a landing on it from its port. */
type Coming = { from: number[]; ship: boolean } | { landing: number };

/** A corps' moves as if its month were whole, while `f` asks where it could go; its own moves are put back after. */
function fresh<T>(c: Corps, f: () => T): T {
  const had = c.ap;
  c.ap = Math.max(c.ap, c.apMax ?? c.ap);
  try { return f(); } finally { c.ap = had; }
}

/**
 * How a corps of an ally could come to attack the place this month or next. By land: the provinces of its side beside
 * the place, clear of the enemy, that it stands on already, that a train takes it to this month, or that a march of a
 * month brings it to over friendly ground. By ship: those of them with a harbour its side holds, from a harbour of its
 * own (navy.ts whyNotShip's tests). Or a landing on the place itself from its port, where the place is the enemy's
 * (whyNotLand's tests for the corps: the ground not barred to it, the book's line open, the sea's path within a
 * landing's range, the sky not denied). Moves already spent and shipping already in use this month do not count against
 * it: the question is next month. Where it stands on the place or beside it, it attacks from there.
 */
function canCome(g: Game, c: Corps, t: Region, steps: Map<number, number>, kept: { rails: Map<number, Set<number>>; ground: Map<number, Map<number, { cost: number }>> }): Coming | null {
  const at = g.regions[c.reg];
  if (at.id === t.id || t.nb.includes(at.id)) return { from: [at.id], ship: false };
  // (Ground of its side beside the place, clear of the enemy's army: somewhere to stand and attack from.)
  const beside = t.nb.map((j) => g.regions[j]).filter((p) => allied(g, c.nat, p.ctrl) && !hostileUnits(g, p, c.nat).length);
  const from = new Set<number>();
  const hops = steps.get(at.id);
  // By rail this month.
  if (hops !== undefined && hops <= RAIL_HOPS && beside.length) {
    let r = kept.rails.get(c.id);
    if (!r) { r = fresh(c, () => railReachFor(g, [c])); kept.rails.set(c.id, r); }
    for (const p of beside) if (r.has(p.id)) from.add(p.id);
  }
  // By road, a month's march over friendly ground (marches.ts farGround: the march a staff could order, whoever's month it is).
  if (hops !== undefined && hops <= ROAD_HOPS && beside.length) {
    let m = kept.ground.get(c.id);
    if (!m) { m = farGround(g, c); kept.ground.set(c.id, m); }
    for (const p of beside) { const e = m.get(p.id); if (e && marchMonths(c, e.cost) <= 1) from.add(p.id); }
  }
  if (from.size) return { from: [...from], ship: false };
  const n = g.nations[c.nat];
  if (!at.port || !allied(g, c.nat, at.ctrl) || n.sealift <= 0 || c.kind === 'fort' || prizeOut(g, at, 'harbour') > 0) return null;
  // By ship, into a harbour of its side beside the place.
  for (const p of beside) {
    if (!p.port || thornGround(g, p, c.nat) || lineShut(g, c.nat, p) || prizeOut(g, p, 'harbour') > 0 || whyBarred(g, c, p)) continue;
    if (seaPath(g, c.nat, at, p)) from.add(p.id);
  }
  if (from.size) return { from: [...from], ship: true };
  // A landing on the place itself, where it is the enemy's.
  if (t.seas.length && atWar(g, c.nat, t.ctrl) && !allied(g, c.nat, t.ctrl) && !lineShut(g, c.nat, t) && !whyBarred(g, c, t)) {
    const surprise = surpriseOn(g, c.nat, t);
    if (seaPath(g, c.nat, at, t, { max: NAVY.landing.range, surprise }) && (surprise || !skyDenies(g, c.nat, t.id))) return { landing: at.id };
  }
  return null;
}

/** A relief in reach: who brings it, where it would strike, and the odds it would go in at. */
export interface Relief { by: string; target: number; odds: number; corps: number[] }

/** A corps' weight in an attack, as limitAttackers ranks it. */
const weight = (g: Game, c: Corps): number => corpsBase(g, c) * KIND[c.kind].atk;

/**
 * The corps that come by land, each put on a province it could attack from while that province has room (STACK, its
 * side's corps already there counted), the strongest first and each where the fewest stand: copies of them standing
 * there, so the attack is weighed from where it would go in (limitAttackers' roads, its sides, assess's rivers and
 * flanks), and nothing in the war is moved.
 */
function standThem(g: Game, ally: string, cs: { c: Corps; from: number[] }[]): Corps[] {
  const mine = new Set(cs.map((x) => x.c));
  const load = new Map<number, number>();
  const room = (id: number): number => { if (!load.has(id)) load.set(id, friendlyUnits(g, g.regions[id], ally).filter((u) => !mine.has(u as Corps)).length); return load.get(id)!; };
  const out: Corps[] = [];
  for (const { c, from } of [...cs].sort((a, b) => weight(g, b.c) - weight(g, a.c) || a.c.id - b.c.id)) {
    // (One already on the place or beside it attacks from where it stands.)
    if (from.length === 1 && from[0] === c.reg) { out.push(c); load.set(c.reg, room(c.reg) + 1); continue; }
    const open = from.filter((id) => room(id) < STACK).sort((a, b) => room(a) - room(b) || a - b);
    if (!open.length) continue;
    load.set(open[0], room(open[0]) + 1);
    out.push({ ...c, reg: open[0] });
  }
  return out;
}

/**
 * (ii) An ally's relief in reach of the country, or null. Each ally at war with its enemies is reckoned on its own: its
 * corps in supply in the field that could attack a province of the country's home ground where the enemy's army
 * stands, this month or next, weighed as the computer weighs a relief (assess, the garrison's sortie included where the
 * place is a besieged town) and sent in as it would send them: by land from the provinces they would attack from
 * (limitAttackers), with as many shipped there as its shipping carries (sealift, a corps each); or as a landing on the
 * place from one port, no more than its shipping carries nor than STACK, at a landing's strength (landingOpts). In reach
 * when the odds reach the bar the computer asks before it goes to a relief: 1.25 at a capital or great city, 1.5
 * elsewhere (reliefBar).
 */
export function reliefInReach(g: Game, nat: string): Relief | null {
  const foes = new Set(g.nations[nat].atWar.filter((e) => g.nations[e]?.alive));
  if (!foes.size) return null;
  const targets = enemyOnHome(g, nat, foes);
  if (!targets.length) return null;
  // (Drawn before the pool is chosen: weighing the first ally's odds would otherwise redraw it halfway through.)
  ensureSupply(g);
  const friends = Object.values(g.nations).filter((n) => n.alive && n.id !== nat && allied(g, n.id, nat) && n.atWar.some((e) => foes.has(e))).map((n) => n.id);
  if (!friends.length) return null;
  const free = g.corps.filter((c) => !c.dead && friends.includes(c.nat) && c.kind !== 'fort' && c.town === undefined && c.leave === undefined && c.supplied && g.regions[c.reg]?.units.includes(c));
  if (!free.length) return null;
  const kept = { rails: new Map<number, Set<number>>(), ground: new Map<number, Map<number, { cost: number }>>() };
  for (const t of targets) {
    const steps = stepsFrom(g, t.id, RAIL_HOPS);
    // (The enemy there: whose army stands on it.)
    const foe = t.units.find((u) => !u.dead && foes.has(u.nat))?.nat;
    if (!foe) continue;
    const bar = reliefBar(g, t);
    for (const ally of friends) {
      if (!atWar(g, ally, foe)) continue;
      const lift = g.nations[ally].sealift;
      const land: { c: Corps; from: number[] }[] = [], shipped: { c: Corps; from: number[] }[] = [], ports = new Map<number, Corps[]>();
      for (const c of free) {
        if (c.nat !== ally) continue;
        const how = canCome(g, c, t, steps, kept);
        if (!how) continue;
        if ('landing' in how) { if (!ports.has(how.landing)) ports.set(how.landing, []); ports.get(how.landing)!.push(c); }
        else (how.ship ? shipped : land).push({ c, from: how.from });
      }
      // By land, with as many shipped up as the shipping carries, the strongest first.
      const byShip = shipped.sort((a, b) => weight(g, b.c) - weight(g, a.c) || a.c.id - b.c.id).slice(0, Math.max(0, lift));
      const force = limitAttackers(g, standThem(g, ally, [...land, ...byShip]), t);
      if (force.length) {
        const odds = assess(g, force, t).odds;
        if (odds >= bar) return { by: ally, target: t.id, odds, corps: force.map((c) => c.id) };
      }
      // Or a landing on the place from one port.
      for (const [, cs] of [...ports].sort(([a], [b]) => a - b)) {
        const group = cs.sort((a, b) => weight(g, b) - weight(g, a) || a.id - b.id).slice(0, Math.min(lift, STACK));
        if (!group.length || (gunsRule(g) && !group.some(fights))) continue;
        const odds = assess(g, group, t, landingOpts(g, group, t)).odds;
        if (odds >= bar) return { by: ally, target: t.id, odds, corps: group.map((c) => c.id) };
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
      if (rep.attNat === e && r && r.owner === nat && !r.overseas) at = Math.min(at ?? rep.turn, rep.turn);
    }
    // (With no battle to date it: an enemy that plays after the country in the month came in its own phase of the last.)
    if (at === undefined && g.regions.some((r) => r.owner === nat && !r.overseas && (r.ctrl === e || r.units.some((u) => !u.dead && u.nat === e)))) at = g.order.indexOf(e) > g.order.indexOf(nat) ? g.turn - 1 : g.turn;
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
