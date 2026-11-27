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
import { allied, annexMonth, assess, atWar, capitulate, corpsBase, ensureSupply, fights, friendlyUnits, gunsRule, headline, homeOf, hostileUnits, limitAttackers, overseasRead, railReachFor, shoreRoom, theName, thornGround, victorOf } from './game';
import { engRoom, roomAt } from './islands';
import { FIT_ORG } from './staff';
import { stopDef, stopOf } from './history';
import { whyBarred } from './ground';
import { blocked } from './rivers';
import { lineShut } from './line';
import { standsFreed } from './liberation';
import { farGround, marchMonths } from './marches';
import { fieldCtrl } from './siege';
import { NAVY, landingOpts, sailWay, seaPath, surpriseOn } from './navy';
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
  // (Where it stands on the place, or beside it with nothing between, it attacks from there; across a blown river it
  // looks for another bank like a corps further off.)
  if (at.id === t.id || (t.nb.includes(at.id) && !blocked(g, c, t))) return { from: [at.id], ship: false };
  // (Ground of its side beside the place, clear of the enemy's army: somewhere to stand and attack from.)
  const beside = t.nb.map((j) => g.regions[j]).filter((p) => allied(g, c.nat, p.ctrl) && !hostileUnits(g, p, c.nat).length);
  const from = new Set<number>();
  const hops = steps.get(at.id);
  // By rail this month.
  if (hops !== undefined && hops <= RAIL_HOPS && beside.length) {
    let r = kept.rails.get(c.id);
    if (!r) { r = fresh(c, () => railReachFor(g, [c])); kept.rails.set(c.id, r); }
    for (const p of beside) if (r.has(p.id) && !blocked(g, c, t, p.id)) from.add(p.id);
  }
  // By road, a month's march over friendly ground (marches.ts farGround: the march a staff could order, whoever's month it is).
  if (hops !== undefined && hops <= ROAD_HOPS && beside.length) {
    let m = kept.ground.get(c.id);
    if (!m) { m = farGround(g, c); kept.ground.set(c.id, m); }
    for (const p of beside) { const e = m.get(p.id); if (e && marchMonths(c, e.cost) <= 1 && !blocked(g, c, t, p.id)) from.add(p.id); }
  }
  if (from.size) return { from: [...from], ship: false };
  const n = g.nations[c.nat];
  if (!at.port || !allied(g, c.nat, at.ctrl) || n.sealift <= 0 || c.kind === 'fort' || prizeOut(g, at, 'harbour') > 0) return null;
  // By ship, into a harbour of its side beside the place.
  for (const p of beside) {
    if (!p.port || thornGround(g, p, c.nat) || lineShut(g, c.nat, p) || prizeOut(g, p, 'harbour') > 0 || whyBarred(g, c, p)) continue;
    if (sailWay(g, c.nat, at, p) && !blocked(g, c, t, p.id)) from.add(p.id);
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

/** A corps that could come, and the provinces it could attack from; shipped, the harbour it sails from. */
type Stand = { c: Corps; from: number[]; quay?: number };

/** The most plans of sailings weighed for one ally against one place (each gives every harbour one place to put in at). */
const PLANS = 64;

/**
 * The corps that come by land, each put on a province it could attack from while that province has room (hasRoom's:
 * the province's room in corps, islands.ts roomAt; engineers apart up to 2, its side's corps already there counted,
 * those that stay where they stand first), the strongest first and each where the fewest stand: copies of them
 * standing there, so the attack is weighed from where it would go in (limitAttackers' roads, its sides, assess's
 * rivers and flanks), and nothing in the war is moved.
 * Those shipped up come with the one harbour their sailing puts in at (sailingPlans).
 */
function standThem(g: Game, ally: string, cs: Stand[]): Corps[] {
  const stays = (x: Stand): boolean => x.from.length === 1 && x.from[0] === x.c.reg;
  // (Only those that would move are left out of where they stand now: they are about to leave it.)
  const movers = new Set(cs.filter((x) => !stays(x)).map((x) => x.c));
  const load = new Map<number, { line: number; eng: number }>();
  const at = (id: number): { line: number; eng: number } => {
    let l = load.get(id);
    if (!l) {
      const here = friendlyUnits(g, g.regions[id], ally).filter((u) => !movers.has(u as Corps));
      const eng = here.filter((u) => u.kind === 'eng').length;
      l = { line: here.length - eng, eng };
      load.set(id, l);
    }
    return l;
  };
  const fits = (c: Corps, id: number): boolean => (c.kind === 'eng' ? at(id).eng < engRoom(g, g.regions[id]) : at(id).line < roomAt(g, g.regions[id], ally));
  const out: Corps[] = cs.filter(stays).map((x) => x.c);
  for (const { c, from } of cs.filter((x) => !stays(x)).sort((a, b) => weight(g, b.c) - weight(g, a.c) || a.c.id - b.c.id)) {
    const open = from.filter((id) => fits(c, id)).sort((a, b) => at(a).line - at(b).line || a - b);
    if (!open.length) continue;
    if (c.kind === 'eng') at(open[0]).eng++; else at(open[0]).line++;
    out.push({ ...c, reg: open[0] });
  }
  return out;
}

/**
 * The month's plans of sailings (the design director's 12:38Z): every harbour with corps to ship sends one sailing,
 * into one harbour of its side beside the place, and each plan names that harbour for every quay. The quays whose
 * strongest corps is heaviest are planned first; past PLANS plans, the later quays keep the first harbour open to
 * them (in the wars measured an ally rarely has more than three quays and two harbours to put in at).
 */
function sailingPlans(g: Game, shipped: Stand[]): Map<number, number>[] {
  const opts = new Map<number, { to: number[]; w: number }>();
  for (const x of shipped) {
    const o = opts.get(x.quay!) ?? { to: [], w: 0 };
    for (const id of x.from) if (!o.to.includes(id)) o.to.push(id);
    o.w = Math.max(o.w, weight(g, x.c));
    opts.set(x.quay!, o);
  }
  let plans: Map<number, number>[] = [new Map()];
  for (const [q, o] of [...opts].sort(([a, x], [b, y]) => y.w - x.w || a - b)) {
    const next: Map<number, number>[] = [];
    for (const p of plans) for (const to of [...o.to].sort((a, b) => a - b)) next.push(new Map(p).set(q, to));
    plans = next.length > PLANS ? plans.map((p) => new Map(p).set(q, Math.min(...o.to))) : next;
  }
  return plans;
}

/**
 * The corps a plan of sailings ships, sharing the lift (a corps each): the strongest first, each into its quay's
 * harbour of the plan while that harbour has room as whyNotShip counts it (roomAt of its side's units of every kind),
 * none twice; under the guns rule a sailing of guns alone takes the strongest corps that fights in its last place.
 */
function shipFor(g: Game, ally: string, shipped: Stand[], plan: Map<number, number>, lift: number, skip: Set<number>): Stand[] {
  const room = new Map<number, number>();
  const left = (id: number): number => room.get(id) ?? roomAt(g, g.regions[id], ally) - friendlyUnits(g, g.regions[id], ally).length;
  const can = shipped.filter((x) => !skip.has(x.c.id) && x.from.includes(plan.get(x.quay!)!)).sort((a, b) => weight(g, b.c) - weight(g, a.c) || a.c.id - b.c.id);
  const pick: Stand[] = [];
  for (const x of can) {
    if (pick.length >= lift) break;
    const to = plan.get(x.quay!)!;
    if (left(to) <= 0) continue;
    room.set(to, left(to) - 1);
    pick.push({ c: x.c, from: [to] });
  }
  if (pick.length && gunsRule(g) && !pick.some((x) => fights(x.c))) {
    const last = pick[pick.length - 1];
    room.set(last.from[0], left(last.from[0]) + 1);
    const f = can.find((x) => fights(x.c) && left(plan.get(x.quay!)!) > 0);
    if (f) pick[pick.length - 1] = { c: f.c, from: [plan.get(f.quay!)!] };
  }
  return pick;
}

/**
 * The strongest `n` of these as limitAttackers ranks them; under the guns rule, where they would be guns alone and a
 * corps that fights is among them, the strongest that fights takes the weakest place (guns never go alone).
 */
function strongest<T>(g: Game, xs: T[], n: number, corps: (x: T) => Corps): T[] {
  const by = [...xs].sort((a, b) => weight(g, corps(b)) - weight(g, corps(a)) || corps(a).id - corps(b).id);
  const top = by.slice(0, Math.max(0, n));
  if (top.length && gunsRule(g) && !top.some((x) => fights(corps(x)))) {
    const f = by.find((x) => fights(corps(x)));
    if (f) top[top.length - 1] = f;
  }
  return top;
}

/**
 * (ii) An ally's relief in reach of the country, or null. Each ally at war with its enemies is reckoned on its own: its
 * corps in supply in the field that could attack a province of the country's home ground where the enemy's army
 * stands, this month or next, weighed as the computer weighs a relief (assess, the garrison's sortie included where the
 * place is a besieged town) and sent in as it would send them: by land from the provinces they would attack from
 * (limitAttackers), with as many shipped there as its shipping carries (sealift, a corps each), one sailing from each
 * harbour into one harbour of its side beside the place, every sailing the month's moves allow (the design director's
 * 12:20Z and 12:38Z); or as a landing on the place from one port, no more than its shipping carries, nor than STACK,
 * nor than an island of the Pacific has room for, at a landing's strength (landingOpts). In reach
 * when the odds reach the bar the computer asks before it goes to a relief: 1.25 at a capital or great city, 1.5
 * elsewhere (reliefBar).
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
  for (const t of targets) {
    const steps = stepsFrom(g, t.id, RAIL_HOPS);
    // (The enemy there: whose army stands on it.)
    const foe = t.units.find((u) => !u.dead && foes.has(u.nat))?.nat;
    if (!foe) continue;
    const bar = reliefBar(g, t);
    for (const ally of friends) {
      if (!atWar(g, ally, foe)) continue;
      const lift = g.nations[ally].sealift;
      const land: Stand[] = [], shipped: Stand[] = [], ports = new Map<number, Corps[]>();
      for (const c of free) {
        if (c.nat !== ally) continue;
        const how = canCome(g, c, t, steps, kept);
        if (!how) continue;
        if ('landing' in how) { if (!ports.has(how.landing)) ports.set(how.landing, []); ports.get(how.landing)!.push(c); }
        // (Shipped, it sails from the harbour it stands in: its sailing is that harbour's.)
        else (how.ship ? shipped : land).push(how.ship ? { c, from: how.from, quay: c.reg } : { c, from: how.from });
      }
      // By land alone; then with each harbour's sailing on its own (the corps of one harbour shipped into one harbour
      // of its side beside the place); then with every sailing the month allows (the design director's 12:38Z): one
      // from each harbour, all in the month, sharing the lift in hand, each plan of sailings weighed in turn. (A corps
      // shipped up can push a better one off the frontage, so the fewer sailings are weighed as well as the most.) A
      // shipped corps the attack leaves out (no room once the march is in, or past the frontage) has its lift given to
      // the next corps of the plan, and the plan is weighed again.
      const weigh = (sail: Stand[]): Relief | null => {
        const force = limitAttackers(g, standThem(g, ally, [...land, ...sail]), t);
        if (!force.length) return null;
        const odds = assess(g, force, t).odds;
        return { by: ally, target: t.id, odds, corps: force.map((c) => c.id) };
      };
      if (land.length) {
        const w = weigh([]);
        if (w && w.odds >= bar) return w;
      }
      if (lift > 0 && shipped.length) {
        const singles = [...new Set(shipped.flatMap((x) => x.from.map((to) => `${x.quay}:${to}`)))].sort().map((k) => new Map([k.split(':').map(Number) as [number, number]]));
        const plans = sailingPlans(g, shipped);
        for (const plan of [...singles, ...(plans[0]?.size > 1 ? plans : [])]) {
          const skip = new Set<number>();
          for (;;) {
            const sail = shipFor(g, ally, shipped, plan, lift, skip);
            if (!sail.length) break;
            const w = weigh(sail);
            if (w && w.odds >= bar) return w;
            const idle = sail.filter((x) => !w?.corps.includes(x.c.id));
            // (Nothing left out, or nobody left to take the idle corps' places: this plan is weighed.)
            if (!idle.length) break;
            for (const x of idle) skip.add(x.c.id);
            if (!shipped.some((x) => !skip.has(x.c.id) && !sail.some((y) => y.c === x.c) && x.from.includes(plan.get(x.quay!)!))) break;
          }
        }
      }
      // Or a landing on the place from one port.
      for (const [, cs] of [...ports].sort(([a], [b]) => a - b)) {
        const group = strongest(g, cs, Math.min(lift, STACK, shoreRoom(g, t, ally)), (c) => c);
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
