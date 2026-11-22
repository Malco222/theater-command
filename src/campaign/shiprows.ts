// Ships by class and navy, the card side (Brian's ask of 22 November 2026, 04:03Z: a navy's ships are not all alike;
// the design director's rulings of 08:49Z and 11:09Z on the naval thread's plan, naval-battles/ships-by-class-plan.md).
//
// Each hull carries a row of the record for life (data/campaign/shiprows.json): her class's worth, the square root of
// her gunfire times her armour against the 1939-41 yardstick. A fleet keeps one list a kind, the hulls in the order
// the battle stars keep them (stars.ts): the longest in the fleet first, new hulls at the back, hulls split off from
// the back, a fleet joined to another at the back of its lists, and hulls lost by the same fixed rotation as the stars
// (b487; Q10). No random draw. A list shorter than its count reads its last hulls as hulls with no row; one longer has
// fallen behind a loss written nowhere else, and mends from the back, as the stars' lists do.
//
// A hull with no row reads today's weight for her kind, 1 (Q5). A hull with a row reads her row's worth times the
// kind's rebase, one factor for the campaign: the fleets that exist at its start keep their total weight in the kind
// (plan 7.4; the history campaigns: navy.json's first fleets; the grand campaign on the wider world: those and
// world.json's). In fleetPower a kind weighs its power times the sum of its hulls' worths, where it weighed its power
// times its count: every caller of fleetPower moves with it (the field fights the card's weight, plan 5.1 c1).
//
// New battleships (the grand campaign only, where the player alone orders them): route 2 (Q4), the navy's own best row
// of the steps it holds, lifted to the step's median once it holds step 1 or 2; the step is the one held when she is
// ordered (Build.step). Research is paid twice, deliberately (Q3): the row prices guns and armour, the 8% of techMul
// fire control by refit. A kin nation takes its senior partner's path and the better of the two steps (ruling 235(1),
// rulings.md 1.10); a smaller nation building on a great power's plans that power's path (plan 4.8); one with none
// builds hulls with no row (ruling 235).
//
// The switch: g.flags.shipRows, set as a campaign opens while data/campaign/stage.json 'shipRows' is on (stage.ts
// SHIPROWS_STAGE). Off, nothing here is read or written: no list, no field, no flag, and fleetPower is release 1's.

import data from '../../data/campaign/shiprows.json';
import hullData from '../../data/campaign/hulls.json';
import navyData from '../../data/campaign/navy.json';
import worldData from '../../data/campaign/world.json';
import { PLANS, planRules, plansFrom } from './plans';
import { hasTech, techRules } from './tech';
import type { Fleet, Game, ShipClass, Ships } from './types';

/** A hull's row: its id in shiprows.json, a built hull's step design ('step:1', 'step:2'), or null where the record has none. */
export type RowId = string | null;
/** A fleet's rows, hull by hull: for each kind that has rows, one entry a hull, those longest in the fleet first. */
export type ShipRows = Partial<Record<ShipClass, RowId[]>>;

/** One class of the record: its inputs and marks, or its worth where the naval thread worked it out from the record. */
export interface ShipRow {
  id: string; nat: string; step: number; label: string; mark: string; hulls: string[];
  guns?: number; rate?: number; shell?: number; belt?: number; deck?: number; worth?: number;
}

interface RowData {
  yardstick: { F: number; belt: number; deck: number };
  steps: { techs: string[]; periods: string[][]; labels: string[] };
  rows: Partial<Record<ShipClass, ShipRow[]>>;
  unsourced: Partial<Record<ShipClass, string[]>>;
  builder: Record<string, string>;
}
const D = data as unknown as RowData;

interface HullRow { name: string; from?: number; fleet?: string; later?: { from: number; name?: string; fleet?: string }[] }
const HULLS = (hullData as unknown as { navies: Record<string, Partial<Record<'bb' | 'cv', HullRow[]>>> }).navies;

/** The kinds that have rows (today the battleships: carriers keep kind weight, Q7; cruisers and destroyers wait for the historian). */
export const ROW_KINDS = (Object.keys(D.rows) as ShipClass[]).filter((c) => (D.rows[c] ?? []).length > 0);
const hasRows = (c: ShipClass): boolean => ROW_KINDS.includes(c);

const BY_ID: Record<string, ShipRow & { kind: ShipClass }> = {};
for (const c of ROW_KINDS) for (const r of D.rows[c]!) BY_ID[r.id] = { ...r, kind: c };

/** True where the campaign plays its ships by class (the switch is set as it opens). */
export const rowRules = (g: Game): boolean => g.flags.shipRows === 1;

// ---- Worth ------------------------------------------------------------------------------------------------

/** A row's worth before the rebase: √(F/F₀ × √(belt × deck)/√(belt₀ × deck₀)), or the worth the record's cells give. */
export function rawWorth(r: ShipRow): number | null {
  // (A row whose cells are not all in and that states no worth is no row: its hulls read today's weight, Q5.)
  if (r.guns === undefined || r.rate === undefined || r.shell === undefined || r.belt === undefined || r.deck === undefined) return r.worth ?? null;
  const Y = D.yardstick;
  const F = (r.guns * r.rate * r.shell) / Y.F;
  const P = Math.sqrt(r.belt * r.deck) / Math.sqrt(Y.belt * Y.deck);
  return Math.sqrt(F * P);
}

/** The median of a step's rows (plan 4.2): 1.0459, 1.0943 and 1.1412 for steps 0, 1 and 2. */
export function stepMedian(k: number): number {
  const w = (D.steps.periods[k] ?? []).map((id) => rawWorth(BY_ID[id])).filter((x): x is number => x !== null).sort((a, b) => a - b);
  if (!w.length) return 1;
  const m = w.length >> 1;
  return w.length % 2 ? w[m] : (w[m - 1] + w[m]) / 2;
}

/** A row id's worth before the rebase: a class of the record, or a step's design (its median). Null: none. */
function rawOf(id: RowId): number | null {
  if (id === null) return null;
  if (id.startsWith('step:')) return stepMedian(Number(id.slice(5)));
  const r = BY_ID[id];
  return r ? rawWorth(r) : null;
}

/** The row a hull of a nation bears by her name, or null (unsourced, or not in the record). */
export function rowOfName(nat: string, c: ShipClass, name: string): RowId {
  for (const r of D.rows[c] ?? []) if (r.nat === nat && r.hulls.includes(name)) return r.id;
  // (A smaller navy's 1939 battleship, unsourced, reads the row the historian names of the navy that built her class: Q5.)
  return D.builder[`${nat}:${name}`] ?? null;
}

/** The hulls of a nation's fleet in a month by the record of names (hulls.json), in its order: the first `count`. */
export function namesIn(nat: string, c: ShipClass, fleet: string, turn: number, count: number): string[] {
  if ((c !== 'bb' && c !== 'cv') || count <= 0) return [];
  const out: string[] = [];
  for (const r of HULLS[nat]?.[c] ?? []) {
    if ((r.from ?? 1) > turn) continue;
    let at = r.fleet;
    for (const x of r.later ?? []) if (x.from <= turn) at = x.fleet ?? at;
    if (at === fleet) out.push(r.name);
    if (out.length >= count) break;
  }
  return out;
}

/** The rows of the hulls a fleet of the data opens with: her named hulls, then hulls with no row to its count. */
export function startRows(nat: string, c: ShipClass, fleet: string, turn: number, count: number): RowId[] {
  const n = Math.max(0, Math.floor(count));
  const names = namesIn(nat, c, fleet, turn, n);
  return Array.from({ length: n }, (_, i) => (i < names.length ? rowOfName(nat, c, names[i]) : null));
}

/** The names of a nation's hulls of a kind that a line of words gives, in the order it gives them ("the battleships Idaho, Mississippi and New Mexico"). */
export function namedIn(nat: string, c: ShipClass, words: string): string[] {
  if (c !== 'bb' && c !== 'cv') return [];
  const all = new Set<string>();
  for (const r of HULLS[nat]?.[c] ?? []) { all.add(r.name); for (const x of r.later ?? []) if (x.name) all.add(x.name); }
  const found: [number, string][] = [];
  for (const name of all) {
    const m = new RegExp(`(^|[^\\p{L}])${name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}(?![\\p{L}])`, 'u').exec(words);
    if (m) found.push([m.index + m[1].length, name]);
  }
  // A name inside a longer one found at the same place ("Hiei" is not in "Kirishima"; the longer wins where two start together).
  found.sort((a, b) => a[0] - b[0] || b[1].length - a[1].length);
  return found.filter(([at], i) => i === 0 || at !== found[i - 1][0]).map(([, name]) => name);
}

interface FleetData { nat: string; name: string; from?: number; ships: Partial<Ships> }

const REBASE = new Map<string, number>();

/**
 * The kind's rebase for the campaign in hand (plan 7.4): the one factor that keeps the total weight of the fleets at its
 * start, the hulls with no row standing at 1. Worked from the data, never from the game, so that it is the same in
 * every month and every save: the history campaigns from navy.json's first fleets; a campaign on the wider world from
 * those and world.json's.
 */
export function rebase(g: Game, c: ShipClass): number {
  // (Fixed in the campaign's flags as its start fleets stand, so a save read by a build holding other data keeps it: pinRebase.)
  const pin = g.flags[`rebase:${c}`];
  return pin !== undefined ? pin : rebaseOfData(g, c);
}

/** The rebase worked from the data, for the chart the campaign is on (memoised per chart). */
function rebaseOfData(g: Game, c: ShipClass): number {
  const wide = g.flags.world === 1;
  const key = `${c}:${wide ? 'world' : 'europe'}`;
  const had = REBASE.get(key);
  if (had !== undefined) return had;
  const rows: FleetData[] = [...(navyData as unknown as { fleets: FleetData[] }).fleets, ...(wide ? (worldData as unknown as { fleets: FleetData[] }).fleets : [])];
  let known = 0, sum = 0;
  for (const f of rows) {
    if ((f.from ?? 1) > 1) continue;
    for (const id of startRows(f.nat, c, f.name, 1, f.ships[c] ?? 0)) {
      const w = rawOf(id);
      if (w !== null) { known++; sum += w; }
    }
  }
  const m = known > 0 && sum > 0 ? known / sum : 1;
  REBASE.set(key, m);
  return m;
}

/**
 * Fixes each kind's rebase in the campaign's flags (plan 7.4: one factor per campaign, set by the fleets at its start):
 * called as its start fleets are complete, after setupNavy and again once the wider world is attached. Nothing with the
 * switch off.
 */
export function pinRebase(g: Game): void {
  if (!rowRules(g)) return;
  for (const c of ROW_KINDS) g.flags[`rebase:${c}`] = rebaseOfData(g, c);
}

/** A hull's worth in the campaign in hand: her row's worth times the kind's rebase, or 1 where she has none (Q5). */
export function hullWorth(g: Game, c: ShipClass, id: RowId): number {
  const w = rawOf(id);
  return w === null ? 1 : w * rebase(g, c);
}

/** What a fleet's ships of one kind weigh, in hulls: the sum of their worths (where the switch is on), else their count. */
export function hullSum(g: Game, f: Fleet, c: ShipClass): number {
  const n = f.ships[c];
  if (!rowRules(g) || !hasRows(c) || !f.rows?.[c] || n <= 0) return n;
  const whole = Math.floor(n);
  let s = n - whole;
  for (const id of listOf(f, c)) s += hullWorth(g, c, id);
  return s;
}

/** A fleet's rows for the field's ticket (plan 5.1 c1): for each kind with rows, the hulls grouped by row in list order, each with the worth fleetPower uses. */
export function fleetRows(g: Game, f: Fleet): Partial<Record<ShipClass, { id: RowId; label: string; worth: number; n: number }[]>> | undefined {
  if (!rowRules(g)) return undefined;
  const out: Partial<Record<ShipClass, { id: RowId; label: string; worth: number; n: number }[]>> = {};
  for (const c of ROW_KINDS) {
    if (f.ships[c] <= 0) continue;
    const groups: { id: RowId; label: string; worth: number; n: number }[] = [];
    for (const id of listOf(f, c)) {
      const at = groups.find((x) => x.id === id);
      if (at) at.n++;
      else groups.push({ id, label: labelOf(id), worth: hullWorth(g, c, id), n: 1 });
    }
    if (groups.length) out[c] = groups;
  }
  return out;
}

/** A row's words: the class's own ('King George V class'), a step's design, or '' for a hull with no row. */
export function labelOf(id: RowId): string {
  if (id === null) return '';
  if (id.startsWith('step:')) return D.steps.labels[Number(id.slice(5))] ?? '';
  return BY_ID[id]?.label ?? '';
}

// ---- New hulls (the grand campaign: Q4 route 2) ------------------------------------------------------------

/** The research step a navy holds (0, 1 or 2): from its research only, never the date; a kin nation the better of its own and its senior partner's. 0 outside the grand campaign. */
export function stepOf(g: Game, nat: string): number {
  if (!techRules(g)) return 0;
  const senior = PLANS.kin[nat];
  const has = (id: string): boolean => hasTech(g, nat, id) || (senior !== undefined && hasTech(g, senior, id));
  let k = 0;
  D.steps.techs.forEach((t, i) => { if (has(t)) k = Math.max(k, i + 1); });
  return k;
}

/** Whose rows a nation's new hulls of a kind are built to: its own, its senior partner's, the plans' great power, or none. */
export function pathOf(g: Game, nat: string, c: ShipClass): string | null {
  if (c === 'bb' || c === 'cv') {
    if (planRules(g)) return plansFrom(g, nat, c);
  }
  if (PLANS.great.includes(nat)) return nat;
  return PLANS.kin[nat] ?? null;
}

/** The row a nation's new hull of a kind is built to at a step (route 2, Q4): its path's best row of the steps held, lifted to the step's median from step 1. */
export function buildRow(g: Game, nat: string, c: ShipClass, step: number): RowId {
  if (!hasRows(c)) return null;
  const path = pathOf(g, nat, c);
  if (!path) return null;
  let best: ShipRow | null = null;
  for (const r of D.rows[c] ?? []) if (r.nat === path && r.step <= step && rawWorth(r) !== null && (!best || rawWorth(r)! > rawWorth(best)!)) best = r;
  if (step >= 1 && stepMedian(step) > (best ? rawWorth(best)! : 0)) return `step:${step}`;
  return best?.id ?? null;
}

// ---- The lists, kept in step with the counts (the stars' rule, stars.ts) ---------------------------------------

/** One kind of a fleet's hulls, hull by hull: always as long as the count, the hulls beyond the list with no row. A fresh copy. */
export function listOf(f: Fleet, c: ShipClass): RowId[] {
  const have = f.rows?.[c] ?? [];
  return Array.from({ length: Math.max(0, Math.floor(f.ships[c])) }, (_, i) => have[i] ?? null);
}

/** Writes a kind's list as it is kept: hulls at the back with no row left off, an empty list or record dropped. */
function keep(f: { rows?: ShipRows }, c: ShipClass, list: RowId[]): void {
  let n = list.length;
  while (n > 0 && list[n - 1] === null) n--;
  if (n > 0) (f.rows ??= {})[c] = list.slice(0, n);
  else if (f.rows?.[c]) {
    delete f.rows[c];
    if (!Object.keys(f.rows).length) delete f.rows;
  }
}

/** Brings a fleet's lists (one kind, or all) back in step with its counts: entries beyond the count are hulls lost, dropped from the back. Before any hull is added. */
export function mendRows(f: Fleet, only?: ShipClass): void {
  if (!f.rows) return;
  for (const c of only ? [only] : ROW_KINDS) {
    const list = f.rows?.[c];
    if (list && list.length > Math.max(0, Math.floor(f.ships[c]))) keep(f, c, list.slice(0, Math.max(0, Math.floor(f.ships[c]))));
  }
}

/**
 * Hulls of one kind just lost, the count already lowered by k and the fleet's tally of the kind's losses already
 * raised by k: by the stars' fixed rotation (b487, Q10), the fleet's earlier losses of the kind plus the loss's order,
 * modulo the hulls there were. `rows`: where the field has said which rows went down (a fought action, plan 5.2
 * item 5), those first, by the rotation among them; the rest by the rotation.
 */
export function loseRows(f: Fleet, c: ShipClass, k: number, rows?: RowId[]): void {
  if (k <= 0 || !f.rows?.[c]) return;
  const there = Math.max(0, Math.floor(f.ships[c])) + k;
  const have = f.rows[c]!;
  const list = Array.from({ length: there }, (_, i) => have[i] ?? null);
  const from = Math.max(0, f.lost[c] - k);
  const named = [...(rows ?? [])];
  const went: RowId[] = [];
  for (let j = 0; j < k && list.length; j++) {
    const at = (from + j) % list.length;
    // (The field's rows: the first hull of that row from the rotation's place on, round the list.)
    const want = named.length ? named.shift()! : undefined;
    let pick = at;
    if (want !== undefined) {
      for (let s = 0; s < list.length; s++) if (list[(at + s) % list.length] === want) { pick = (at + s) % list.length; break; }
    }
    went.push(...list.splice(pick, 1));
  }
  keep(f, c, list);
  noteLost(f, c, went);
}

/** Notes the named hulls a fleet has lost (loseRows; the record's sinkings in navy.ts's arrivals), for takeRows' guard. */
export function noteLost(f: Fleet, c: ShipClass, went: RowId[]): void {
  const ids = went.filter((x): x is string => x !== null);
  if (!ids.length) return;
  (f.rowsLost ??= {})[c] = [...(f.rowsLost[c] ?? []), ...ids];
}

/** Hulls about to be split off a fleet (fleets.ts detach), called before they leave: the last of each kind go. The new fleet's rows, or undefined. */
export function splitRows(f: Fleet, s: Ships): ShipRows | undefined {
  if (!f.rows) return undefined;
  const out: { rows?: ShipRows } = {};
  for (const c of ROW_KINDS) {
    if (!f.rows?.[c]) continue;
    const all = listOf(f, c), stay = Math.max(0, all.length - Math.max(0, Math.floor(s[c])));
    keep(f, c, all.slice(0, stay));
    keep(out, c, all.slice(stay));
  }
  return out.rows;
}

/** One fleet joined to another (fleets.ts merge), called before the hulls move: a's hulls come in at the back of b's lists. */
export function joinRows(a: Fleet, b: Fleet): void {
  if (a.rowsLost) {
    for (const c of ROW_KINDS) if (a.rowsLost[c]?.length) noteLost(b, c, a.rowsLost[c]!);
    delete a.rowsLost;
  }
  if (!a.rows && !b.rows) return;
  for (const c of ROW_KINDS) if (a.rows?.[c] || b.rows?.[c]) keep(b, c, [...listOf(b, c), ...listOf(a, c)]);
  delete a.rows;
}

/** New hulls of a kind joining a fleet at the back, called before its count is raised: the list mended, then theirs added. */
export function addRows(f: Fleet, c: ShipClass, rows: RowId[]): void {
  if (!hasRows(c)) return;
  mendRows(f, c);
  if (!rows.some((r) => r !== null)) return;
  keep(f, c, [...listOf(f, c), ...rows]);
}

/**
 * Hulls of a kind leaving a fleet (to another, or by the record's sinking), called before its count is lowered: the
 * named hulls' rows where it has them, then the last. Returns the rows that went, to join the fleet they go to.
 */
export function takeRows(f: Fleet, c: ShipClass, n: number, want: RowId[] = [], g?: Game): RowId[] {
  const k = Math.max(0, Math.min(Math.floor(n), Math.floor(f.ships[c])));
  if (!hasRows(c) || k <= 0) return [];
  const list = listOf(f, c), out: RowId[] = [];
  for (let j = 0; j < k; j++) {
    const w = want[j];
    let at = w !== undefined ? list.lastIndexOf(w) : -1;
    let carry: RowId | undefined;
    // (The words name her and the fleet shows no hull of her row: a hull with no row goes, and as her, since the record
    // of names, not the fleet's list, says which ship sailed; but only while her navy shows fewer hulls of that row,
    // afloat on the chart or lost, than the record has in being, so that no row is laid twice and none comes back from
    // the bottom. Otherwise, or where there is no hull with none, the last goes as it stands.)
    if (at < 0 && w != null && g && rowRoom(g, f.nat, c, w, out)) { at = list.lastIndexOf(null); if (at >= 0) carry = w; }
    if (at < 0) at = list.length - 1;
    out.push(carry !== undefined ? carry : list[at]);
    list.splice(at, 1);
  }
  keep(f, c, list);
  return out;
}

/** True while a navy shows fewer hulls of a row (in its fleets' lists, lost, or on their way in `moving`) than the record of names has in being this month. */
function rowRoom(g: Game, nat: string, c: ShipClass, w: string, moving: RowId[]): boolean {
  let shown = moving.filter((x) => x === w).length;
  for (const f of g.fleets) {
    if (f.nat !== nat) continue;
    shown += listOf(f, c).filter((x) => x === w).length + (f.rowsLost?.[c]?.filter((x) => x === w).length ?? 0);
  }
  let being = 0;
  for (const r of HULLS[nat]?.[c as 'bb' | 'cv'] ?? []) if ((r.from ?? 1) <= g.turn && rowOfName(nat, c, r.name) === w) being++;
  return shown < being;
}

/**
 * Lays a fleet's lists as it forms (navy.ts addFleet): its named hulls by the record of names in this month, the rest
 * with no row. A fleet formed by its book lays the hulls the book names (Force H's Resolution in June 1940, whose
 * posting the record of names dates a month later).
 */
export function layRows(g: Game, f: Fleet, named?: Partial<Record<ShipClass, string[]>>): void {
  if (!rowRules(g)) return;
  for (const c of ROW_KINDS) {
    if (f.ships[c] <= 0) continue;
    const n = Math.floor(f.ships[c]), names = named?.[c];
    keep(f, c, names ? Array.from({ length: n }, (_, i) => (i < names.length ? rowOfName(f.nat, c, names[i]) : null)) : startRows(f.nat, c, f.name, g.turn, f.ships[c]));
  }
}

/** Lays the lists of every fleet that has none (a campaign given the switch after its fleets formed: a test, or a probe). */
export function layAllRows(g: Game): void {
  if (!rowRules(g)) return;
  for (const f of g.fleets) if (!f.rows) layRows(g, f);
}

/** The rows of a line's named hulls (an arrival's words), as many as `n`, the rest with no row. */
export function rowsNamed(nat: string, c: ShipClass, words: string, n: number): RowId[] {
  const names = namedIn(nat, c, words);
  return Array.from({ length: Math.max(0, Math.floor(n)) }, (_, i) => (i < names.length ? rowOfName(nat, c, names[i]) : null));
}
