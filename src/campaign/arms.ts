// A nation's stock of arms: one module, three shelves (rifles, cadres, guns), every amount in divisions' worth.
//
// One stock per nation, drawn by everything that arms a man, never two. Whatever raises men draws their arms here and
// gives them back when the men go home; a rule that moves a fallen nation's arms takes them off its shelves and puts
// them on another's.
//
// Where a nation's row for a shelf comes from, the first found wins: a row source another module registered (rowsFrom,
// the latest first), then data/campaign/arms.json, then, for rifles alone, the depot seed another module registered
// (seedFrom). A nation with no row for a shelf keeps none: nothing is written for it, a draw gives all that is asked,
// and it is never short.
//
// This module reads no switch and moves nothing unless it is called. A rule under a campaign switch of its own lays its
// rows through rowsFrom, so they stand only in the campaigns where its switch is on. It imports nothing of the game's
// rules, so any module may register into it while the others load: the share of home industry a nation holds is the
// caller's to give.
//
// State is kept in g.flags, so it is saved with the campaign. A flag is written only when a shelf is moved; with none,
// the shelf stands at its row's first day.
//   sa:<nat>   the rifles on the shelf now;  sac:<nat> the cadres;  sag:<nat> the guns
//   saw:<nat>  the month its industry began to turn over to war (warFooting)
//   sam:<nat>  the last month its gain was put on the shelves (armsMonth)

import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import data from '../../data/campaign/arms.json';
import type { Corps, Game } from './types';

export type Shelf = 'rifles' | 'cadres' | 'guns';
export const SHELVES: readonly Shelf[] = ['rifles', 'cadres', 'guns'];

/** A gain a month from a named place, while the nation holds it. */
export interface Works { place: string; gain: number }
/** A nation's row for one shelf: its first day, its gain a month at peace and at war, the months its industry takes to turn over, and its works. All optional. */
export interface ShelfRow { start?: number; peace?: number; war?: number; turn?: number; works?: Works[] }
/** A nation's rows, by shelf. */
export type ArmsRows = Partial<Record<Shelf, ShelfRow>>;

export const ARMS = data as unknown as { nations: Record<string, ArmsRows> };

const keys = (pre: string): ((nat: string) => string) => { const K: Record<string, string> = {}; return (nat) => (K[nat] ??= `${pre}:${nat}`); };
const kShelf: Record<Shelf, (nat: string) => string> = { rifles: keys('sa'), cadres: keys('sac'), guns: keys('sag') };
const kFooting = keys('saw'), kMonth = keys('sam');

/** An amount that is negative, not a number or not finite counts as nothing. */
const amount = (x: number): number => (typeof x === 'number' && Number.isFinite(x) && x > 0 ? x : 0);
const num = (x: unknown): number => (typeof x === 'number' && Number.isFinite(x) ? x : 0);

// ---- Rows -------------------------------------------------------------------------------------------------------------

type RowSource = (g: Game, nat: string) => ArmsRows | undefined;
type DepotSeed = (nat: string) => number | undefined;

const SOURCES: RowSource[] = [];
let SEED: DepotSeed | null = null;

/**
 * Registers a row source: fn(g, nat) gives that nation's rows, or undefined. The latest registered is asked first, and
 * a source that gives no row for a shelf leaves that shelf to the next. Returns the call that takes the source away.
 */
export function rowsFrom(fn: RowSource): () => void {
  SOURCES.unshift(fn);
  return () => { const i = SOURCES.indexOf(fn); if (i >= 0) SOURCES.splice(i, 1); };
}

/**
 * Registers the depot seed, the last place a nation's rifles row is looked for: fn(nat) gives the rifles' first day,
 * or undefined where the depots carry no figure for it. One seed stands at a time (null for none). Returns the call
 * that puts back the one before.
 */
export function seedFrom(fn: DepotSeed | null): () => void {
  const before = SEED;
  SEED = fn;
  return () => { SEED = before; };
}

/** A nation's row for a shelf, the first found: a registered source, the table, then (rifles alone) the depot seed. */
function rowOf(g: Game, nat: string, shelf: Shelf): ShelfRow | undefined {
  for (const fn of SOURCES) { const r = fn(g, nat)?.[shelf]; if (r && typeof r === 'object') return r; }
  const t = ARMS.nations[nat]?.[shelf];
  if (t && typeof t === 'object') return t;
  if (shelf !== 'rifles' || !SEED) return undefined;
  const s = SEED(nat);
  return typeof s === 'number' && Number.isFinite(s) ? { start: s } : undefined;
}

/** True where the nation's row carries that shelf at all. */
export const has = (g: Game, nat: string, shelf: Shelf): boolean => rowOf(g, nat, shelf) !== undefined;

/** What stood on a shelf on the first day, in divisions' worth (0 where the nation has no figure for it). */
export const firstOf = (g: Game, nat: string, shelf: Shelf): number => amount(num(rowOf(g, nat, shelf)?.start));

// ---- The shelves ------------------------------------------------------------------------------------------------------

/** What stands on a shelf now, in divisions' worth (0 where the nation has no figure for it). */
export function stockOf(g: Game, nat: string, shelf: Shelf): number {
  const row = rowOf(g, nat, shelf);
  if (!row) return 0;
  const now = g.flags[kShelf[shelf](nat)];
  return now !== undefined ? now : amount(num(row.start));
}

/**
 * What a drawer may count on: the stock, or Infinity where the nation has no figure for the shelf (it is never short
 * of what it keeps no count of).
 */
export const countOn = (g: Game, nat: string, shelf: Shelf): number => (has(g, nat, shelf) ? stockOf(g, nat, shelf) : Infinity);

/**
 * Draws from a shelf: gives what there is, up to the amount, and returns how much it gave. A nation with no figure for
 * the shelf is never short: it is given the whole amount and nothing is written.
 */
export function draw(g: Game, nat: string, shelf: Shelf, want: number): number {
  const x = amount(want);
  if (!has(g, nat, shelf)) return x;
  const now = stockOf(g, nat, shelf), got = Math.min(x, Math.max(0, now));
  if (got > 0) g.flags[kShelf[shelf](nat)] = now - got;
  return got;
}

/** Adds to a shelf the nation's row carries, with no bound, and returns what was added (0 where it carries none). */
export function put(g: Game, nat: string, shelf: Shelf, more: number): number {
  const x = amount(more);
  if (x <= 0 || !has(g, nat, shelf)) return 0;
  g.flags[kShelf[shelf](nat)] = stockOf(g, nat, shelf) + x;
  return x;
}

/** Gives back what a draw took. What comes back to a shelf has no bound, as with put. Returns what was given back. */
export const giveBack = (g: Game, nat: string, shelf: Shelf, back: number): number => put(g, nat, shelf, back);

/** Empties a shelf and returns what stood on it. The shelf stands at 0 after, not at its first day. */
export function take(g: Game, nat: string, shelf: Shelf): number {
  if (!has(g, nat, shelf)) return 0;
  const was = stockOf(g, nat, shelf);
  g.flags[kShelf[shelf](nat)] = 0;
  return was;
}

/**
 * The rifles and guns a living corps carries at its strength this month, in divisions' worth (a dead one, none). Every
 * corps is whole for now: the gun figure a rule keeps on a corps, and the armed level of the rifles' own rule, come in
 * here.
 */
export function armsOf(_g: Game, c: Corps): { rifles: number; guns: number } {
  const w = c.dead ? 0 : amount(c.divs * c.str);
  return { rifles: w, guns: w };
}

// ---- The month's gain -------------------------------------------------------------------------------------------------

/**
 * Notes the month a nation's industry began to turn over to war, once (the caller's cause: the month its men are
 * called up). Nothing is written for a nation with no war figure on any shelf.
 */
export function warFooting(g: Game, nat: string): void {
  if (g.flags[kFooting(nat)] !== undefined) return;
  if (!SHELVES.some((s) => typeof rowOf(g, nat, s)?.war === 'number')) return;
  g.flags[kFooting(nat)] = g.turn;
}

/** Strikes the note: its industry is back on the peace figure. */
export function peaceFooting(g: Game, nat: string): void {
  if (g.flags[kFooting(nat)] !== undefined) delete g.flags[kFooting(nat)];
}

/** How far a shelf's industry has turned over to war, 0 to 1: nothing before the footing is noted, then month by month over its 'turn', at once where it has none. */
function ramp(g: Game, nat: string, row: ShelfRow): number {
  const since = g.flags[kFooting(nat)];
  if (since === undefined) return 0;
  const turn = num(row.turn);
  return turn > 0 ? Math.min(1, Math.max(0, g.turn - since) / turn) : 1;
}

/** A shelf's works, each with its gain and whether the nation holds its place now (a place this chart lacks gives nothing). */
function worksOf(g: Game, nat: string, row: ShelfRow | undefined): { place: string; gain: number; held: boolean }[] {
  return (Array.isArray(row?.works) ? row!.works : []).map((w) => ({ place: w.place, gain: amount(w.gain), held: g.byName[w.place]?.ctrl === nat }));
}

/**
 * This month's gain on a shelf: what the nation's industry makes, by its peace and war figures and how far it has
 * turned over, scaled by `held`, the share of its home industry it holds (the caller's to give; 1 where not given);
 * and the gain of each works place it holds, which is lost with the place and not scaled.
 */
export function gainOf(g: Game, nat: string, shelf: Shelf, held = 1): number {
  const row = rowOf(g, nat, shelf);
  if (!row) return 0;
  const peace = num(row.peace), war = typeof row.war === 'number' ? num(row.war) : peace;
  const share = Number.isFinite(held) ? Math.min(1, Math.max(0, held)) : 1;
  let gain = peace !== 0 || war !== 0 ? amount(share * (peace + (war - peace) * ramp(g, nat, row))) : 0;
  for (const w of worksOf(g, nat, row)) if (w.held) gain += w.gain;
  return gain;
}

/**
 * Puts the month's gain on each shelf of a living nation, once a month: a second call in the month adds nothing.
 * Nothing is written where nothing is gained. `held` as for gainOf.
 */
export function armsMonth(g: Game, nat: string, held = 1): void {
  if (!g.nations[nat]?.alive || g.flags[kMonth(nat)] === g.turn) return;
  let any = false;
  for (const s of SHELVES) if (put(g, nat, s, gainOf(g, nat, s, held)) > 0) any = true;
  if (any) g.flags[kMonth(nat)] = g.turn;
}

// ---- The view ---------------------------------------------------------------------------------------------------------

export interface ShelfView { has: boolean; stock: number; first: number; gain: number; works: { place: string; gain: number; held: boolean }[] }

/** A nation's shelves, for a page or a probe. `held` as for gainOf. */
export function armsView(g: Game, nat: string, held = 1): Record<Shelf, ShelfView> {
  const out = {} as Record<Shelf, ShelfView>;
  for (const s of SHELVES) {
    const row = rowOf(g, nat, s);
    out[s] = { has: !!row, stock: stockOf(g, nat, s), first: firstOf(g, nat, s), gain: gainOf(g, nat, s, held), works: worksOf(g, nat, row) };
  }
  return out;
}
