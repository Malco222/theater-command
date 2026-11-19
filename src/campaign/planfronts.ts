// The fronts of a plan taken from the record (rider b633 to ruling 234, with the design director's notes of 11:07Z,
// 11:17Z and 11:28Z of 19 November 2026; the record: historian/japan-plan-nov41-b631.md).
//
// A great choice of a staff (staff.ts, `Op`) may name FRONTS. Each front is the least the ruling asks:
//   place   where it ends: the aim the plan gave it. The front is live from the month the choice goes until that
//           place is its side's or every row of its list is gone. Once over it is over for good.
//   rows    its list. A row on a live front's list is that front's: no staff takes it off (not the ships, not the
//           railways, not the garrison rule), and on land it goes no farther from the front's place than it stands.
//           The free staff may add rows of its own to the fight; it takes none away.
//   held    rows of the list the plan keeps on their quay from the day they stand (the 2nd Division for Java).
//   land    moves by land: a row that joins its front by road (the Guards from Phnom Penh to Singora).
//   then    follow-ons by month, counted from the month the choice went. A follow-on goes by its date whatever
//           the state of the front its row came from (the 48th left Luzon while Bataan held), unless it names that
//           front's place in `after`: every place in `after` is to be its side's first (the hop before, by the
//           record). Until then the row waits as the plan's, where it stands. A follow-on ends with its front.
//           A row with several makes them in the table's order, one landing a month; two where the table gives
//           both the same month (Tarakan and then Balikpapan). `from` are the harbours the record staged it at,
//           used where the shore is out of a landing's reach from where the row stands; `stage` marks a move to
//           a harbour its side holds and no landing.
// Only where no book dates the country's rows (the stops that keep the book have their dated rows), never for the
// player, and only for a plan that names fronts: with the far side off there is none, and nothing here happens.
// This file is the state of the fronts; what a staff does with it is ai.ts (`frontsMonth`).

import { allied } from './game';
import { isOpen, ops, type Op } from './staff';
import { bookKept } from './stations';
import type { Corps, Game, Region } from './types';

export interface FollowOn { month: number; corps: string; with?: string[]; to: string[]; after?: string[]; from?: string[]; stage?: boolean; minOdds?: number }
export interface PlanFront { id: string; place: string; rows: string[]; held?: string[]; land?: { corps: string; with?: string[]; to: string }[]; then?: FollowOn[] }
export interface FrontPlan { op: Op; at: number; fronts: PlanFront[] }
/** A row on a live front: the front it is on, the follow-on that has it (if one does), and whether the plan holds it where it stands. */
export interface FrontRow { c: Corps; k: number; front: PlanFront; place: Region; next: { k: number; i: number; f: FollowOn } | null; waits: boolean }
export interface FrontsNow { nat: string; plan: FrontPlan; live: boolean[]; rows: Map<number, FrontRow> }

const overKey = (nat: string, k: number): string => `st:frO:${nat}:${k}`;
/** Set once a row of the front's list has stood on the board: a front whose rows are still to come has not lost its force. */
const stoodKey = (nat: string, k: number): string => `st:frH:${nat}:${k}`;
/** The flag a follow-on's end is kept under: the month it was made, or the month it ended unmade (negative). */
export const followKey = (nat: string, k: number, i: number): string => `st:frD:${nat}:${k}:${i}`;
/** The front a follow-on took a row to (its index + 1): the row is that front's from then on. */
const joinedKey = (c: Corps): string => `st:frJ:${c.id}`;
/** The month a row finished its move by land. */
export const walkedKey = (c: Corps): string => `st:frW:${c.id}`;
/** The month a row last made one of the plan's landings or sailings. */
export const sailedKey = (c: Corps): string => `st:frS:${c.id}`;

/** The plan with fronts that this staff has taken up, where no book dates its rows; null with none. */
export function planFronts(g: Game, nat: string): FrontPlan | null {
  if (!isOpen(g) || nat === g.human) return null;
  for (const op of ops(nat)) {
    if (!op.fronts?.length) continue;
    const at = g.flags[`st:done:${nat}:${op.id}`] ?? 0;
    if (at > 0) return bookKept(g, nat) ? null : { op, at, fronts: op.fronts };
  }
  return null;
}

/** The rows a plan not yet taken up keeps on their quays, where no book dates them: held from the day they stand. */
export function plansHold(g: Game, nat: string): void {
  if (!isOpen(g) || nat === g.human) return;
  for (const op of ops(nat)) {
    if (!op.fronts?.length || (g.flags[`st:done:${nat}:${op.id}`] ?? 0) !== 0 || g.turn > (op.to ?? 999) || bookKept(g, nat)) continue;
    for (const F of op.fronts) for (const nm of F.held ?? []) {
      const c = g.corps.find((x) => x.nat === nat && x.name === nm && !x.dead);
      if (c && !c.hold) c.hold = true;
    }
  }
}

const marches = new WeakMap<Region[], Map<number, Int16Array>>();
/** Marches by land from a place to every region, over any ground; -1 where no road leads. */
export function landMarches(g: Game, from: number): Int16Array {
  let of = marches.get(g.regions);
  if (!of) marches.set(g.regions, (of = new Map()));
  let d = of.get(from);
  if (d) return d;
  d = new Int16Array(g.regions.length).fill(-1);
  d[from] = 0;
  const q = [from];
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (d[j] < 0) { d[j] = d[q[i]] + 1; q.push(j); }
  of.set(from, d);
  return d;
}

/** Every row a front names: its list, its moves by land and its follow-ons. */
const named = (F: PlanFront): string[] => [...F.rows, ...(F.land ?? []).flatMap((m) => [m.corps, ...(m.with ?? [])]), ...(F.then ?? []).flatMap((f) => [f.corps, ...(f.with ?? [])])];

/**
 * Reads the fronts for one staff's month: which are live, which follow-ons have ended, whose each row is. It ends a
 * front whose place has fallen or whose force is gone, ends the follow-ons of a front that is over, and lets go the
 * rows a front that ended this month still held. Nothing for a staff with no such plan.
 */
function read(g: Game, nat: string, change: boolean): FrontsNow | null {
  const plan = planFronts(g, nat);
  if (!plan) return null;
  const corps = (nm: string): Corps | null => g.corps.find((c) => c.nat === nat && c.name === nm && !c.dead) ?? null;
  const ended: number[] = [];
  const live = plan.fronts.map((F, k) => {
    if (g.flags[overKey(nat, k)]) return false;
    const place = g.byName[F.place];
    const force = F.rows.some((nm) => corps(nm));
    if (force && change) g.flags[stoodKey(nat, k)] = 1;
    const on = !!place && !allied(g, nat, place.ctrl) && (force || !g.flags[stoodKey(nat, k)]);
    if (!on && change) { g.flags[overKey(nat, k)] = g.turn; ended.push(k); }
    return on;
  });
  // A follow-on whose row is gone has ended; so has every follow-on of a front that is over.
  if (change) plan.fronts.forEach((F, k) => (F.then ?? []).forEach((f, i) => {
    if (g.flags[followKey(nat, k, i)] === undefined && (!live[k] || !corps(f.corps))) g.flags[followKey(nat, k, i)] = -g.turn;
  }));
  const pending = (nm: string): { k: number; i: number; f: FollowOn } | null => {
    for (let k = 0; k < plan.fronts.length; k++) {
      if (!live[k]) continue;
      const T = plan.fronts[k].then ?? [];
      for (let i = 0; i < T.length; i++) {
        if (g.flags[followKey(nat, k, i)] !== undefined) continue;
        if (T[i].corps === nm || (T[i].with ?? []).includes(nm)) return { k, i, f: T[i] };
      }
    }
    return null;
  };
  const rows = new Map<number, FrontRow>();
  for (const nm of new Set(plan.fronts.flatMap(named))) {
    const c = corps(nm);
    if (!c) continue;
    const joined = (g.flags[joinedKey(c)] ?? 0) - 1;
    const s = joined >= 0 ? joined : plan.fronts.findIndex((F) => F.rows.includes(nm));
    const p = pending(nm);
    let k = -1, next: FrontRow['next'] = null;
    if (s >= 0 && live[s]) {
      // (A follow-on on another front takes the row by its date, unless it waits for the fall of the row's own front.)
      if (p && p.k !== s && g.turn >= plan.at + p.f.month && !(p.f.after ?? []).includes(plan.fronts[s].place)) { k = p.k; next = p; }
      else { k = s; next = p && p.k === s ? p : null; }
    } else if (p) { k = p.k; next = p; }
    if (k < 0) continue;
    if (change && next && k !== s) g.flags[joinedKey(c)] = k + 1;
    const F = plan.fronts[k];
    const byRoad = (F.land ?? []).some((m) => m.corps === nm || (m.with ?? []).includes(nm)) && g.flags[walkedKey(c)] === undefined;
    rows.set(c.id, { c, k, front: F, place: g.byName[F.place], next, waits: !!next || byRoad });
  }
  // The rows a front that ended this month named, and no live front has now, are their staff's own again.
  if (change) for (const k of ended) for (const nm of new Set(named(plan.fronts[k]))) {
    const c = corps(nm);
    if (c && !rows.has(c.id) && c.hold && !c.stay) c.hold = false;
  }
  return { nat, plan, live, rows };
}

let current: FrontsNow | null = null;
/** How many of the plan's own orders are being carried out: a row the plan itself moves is not held to its front's ground. */
let ordered = 0;

/** Opens a staff's month: reads the fronts, with what that changes, and keeps the reading for the month. */
export function frontsTick(g: Game, nat: string): FrontsNow | null { return (current = read(g, nat, true)); }
/** The month's reading again after the plan has moved its rows. */
export function frontsAgain(g: Game): FrontsNow | null { return current ? (current = read(g, current.nat, false)) : null; }
/** The staff's month is over. */
export function frontsClear(): void { current = null; ordered = 0; }
export const frontsNow = (): FrontsNow | null => current;
/** True for a row on a live front of the staff whose month this is. */
export const onFront = (c: Corps): boolean => !!current && current.rows.has(c.id);
/** True for such a row that the plan has waiting where it stands: for a follow-on, or on its road. */
export const planWaits = (c: Corps): boolean => !!current?.rows.get(c.id)?.waits;
/** Runs one of the plan's own orders. */
export function byThePlan<T>(f: () => T): T { ordered++; try { return f(); } finally { ordered--; } }

/**
 * True where a move would take a row off its front: on the land its front's place stands on, a row goes no farther
 * from that place than it stands. (Where no road leads there, the land it stands on is all it can reach.)
 */
export function offFront(g: Game, c: Corps, to: Region): boolean {
  if (!current || ordered) return false;
  const row = current.rows.get(c.id);
  if (!row || !row.place) return false;
  const d = landMarches(g, row.place.id);
  return d[c.reg] >= 0 && (d[to.id] < 0 || d[to.id] > d[c.reg]);
}

/** For the measuring harness and the tests: each front, whether it is live, the month it ended, and the rows it has. It changes nothing. */
export function frontsState(g: Game, nat: string): { id: string; place: string; live: boolean; over: number; rows: string[]; waiting: string[] }[] {
  const now = read(g, nat, false);
  if (!now) return [];
  return now.plan.fronts.map((F, k) => {
    const mine = [...now.rows.values()].filter((r) => r.k === k);
    return { id: F.id, place: F.place, live: now.live[k], over: g.flags[overKey(nat, k)] ?? 0, rows: mine.map((r) => r.c.name), waiting: mine.filter((r) => r.waits).map((r) => r.c.name) };
  });
}
