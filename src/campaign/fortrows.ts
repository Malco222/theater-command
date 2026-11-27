// The fortress rows (the design director's 05:38Z, 06:12Z, 06:15Z, 06:17Z and 06:21Z of 22 November 2026; the
// historian's historian/fortress-garrisons-by-line.md and its addendum; data/campaign/fortrows.json).
//
// The crews of the fortified lines, by the record's dates, at every stop of the dial: the order of battle, never moved
// by a staff. They end by battle, by starving, or with their country; only a row the record's own orders took out of
// its works (ORDERED OUT) leaves on its date, and only at stops 1-2, its men going back to the reserve.
//
// Everything here is the grand campaign's, under the switch (stage.ts FORTS_STAGE, the flag set as the campaign
// opens: game.ts fortRows). The rules the crews fight by are in game.ts and siege.ts, keyed on the same flag: they take
// no place among the five a province holds (roomless), their works face every attacker for their share of the defence
// (fortShare), and beaten or cut off they stay in their works on their own stores.
//
// A fortress row comes only from the book: this file's rows and the first-day rows of setup-1939.json and
// far-world.json. Nothing a player or a staff can build or raise is of the fortress kind, so the place it does not take
// cannot be farmed (pinned by tests/fortress-rows-r2.test.ts).

import data from '../../data/campaign/fortrows.json';
import { growCorps } from './armies';
import { addCorps, allied, fortRows, headline, removeCorps } from './game';
import { stopOf } from './history';
import { returnMen } from './manpower';
import type { Corps, Game, Region } from './types';

/**
 * A crew's own stores, in months (the design director's 06:41Z, on the historian's 06:50Z addendum): the month count
 * its works were stocked for. Null for a row that has no stores clock and yields only with its army (the Metaxas
 * forts). A row the book gives no figure takes the default, marked judgment.
 */
export const STORES = data as unknown as { stores: Record<string, { months: number | null; mark?: string }>; storesDefault: number };
export function crewStores(c: Corps): number | null {
  const s = STORES.stores[`${c.nat}:${c.name}`];
  return s ? s.months : STORES.storesDefault;
}
/**
 * The months of its stores a fortress crew has eaten since it was last fed (the design director's 09:06Z): counted
 * apart from its organisation, in the game's flags under the corps' id, so the corps and the save keep their shape and
 * a campaign with the fortress rows off writes nothing.
 */
const eatenKey = (c: Corps): string => `eaten:${c.id}`;
export const storesEaten = (g: Game, c: Corps): number => g.flags[eatenKey(c)] ?? 0;
/** The months of its own stores a fortress crew has left, or null for a row with no clock. */
export function storesLeft(g: Game, c: Corps): number | null {
  const months = crewStores(c);
  return months === null ? null : Math.max(0, months - storesEaten(g, c));
}
/** A month fed in supply (09:06Z (b)): a relieved fortress is restocked, and its count starts again. */
export function crewFed(g: Game, c: Corps): void {
  delete g.flags[eatenKey(c)];
}
/**
 * A month cut off for a fortress crew (game.ts startPhase, the unfed branch, under the fortress rows): it eats a
 * month of its own stores, and neither its organisation nor its strength wastes for it (09:06Z: a fed crew holds its
 * organisation while its stores last). 'starved' in the month its stores run out, the historian's months being the
 * whole hold: it goes then, by the starved branch, with no hungry months after. 'held' while they last, or always for
 * a row with no clock.
 */
export function crewMonth(g: Game, c: Corps): 'held' | 'starved' {
  const months = crewStores(c);
  if (months === null) return 'held';
  const eaten = storesEaten(g, c) + 1;
  if (eaten >= months - 1e-9) { crewFed(g, c); return 'starved'; }
  g.flags[eatenKey(c)] = eaten;
  return 'held';
}

/**
 * The stores of a fortified port that holds a fortress row (the design director's 07:12Z, b418): the place's whole
 * stores are the row's, so the port rule reads the row's months in place of its own two. The largest of the living rows
 * there with a clock, in the field or in the town, of the port's side; null where none has one, and the port keeps its own.
 */
export function portStores(g: Game, r: Region): number | null {
  let most: number | null = null;
  for (const c of [...r.units, ...(r.garr ?? [])]) {
    if (c.dead || c.kind !== 'fort' || !allied(g, c.nat, r.ctrl)) continue;
    const m = crewStores(c);
    if (m !== null && (most === null || m > most)) most = m;
  }
  return most;
}

/** A row's options: the turn its orders took it out, the row it joins, and how sure its worth is. */
export interface FortOpts { out?: number; joins?: string; mark?: 'judgment' | 'unsure' }
/** [turn, nation, name, province, worth, options]: worth is divisions times strength. */
export type FortRow = [number, string, string, string, number, FortOpts];

export const FORT_ROWS: FortRow[] = (data as unknown as { rows: FortRow[] }).rows;

/** A fortress crew's experience (the design director's 11:56Z): trained, every nation's alike (fortrows.json crewXp). */
export const FORT_XP: number = (data as unknown as { crewXp: number }).crewXp;

/** As the campaign opens with the rows on: the crews already on the roster (setup-1939.json's first day) are trained. */
export function trainCrews(g: Game): void {
  for (const c of g.corps) if (!c.dead && c.kind === 'fort') c.xp = Math.max(c.xp, FORT_XP);
}

/** A row's shape: up to one division's worth is one division at that strength; more, two divisions at half it. */
export function fortShape(worth: number): { divs: number; str: number } {
  const divs = worth > 1 ? 2 : 1;
  return { divs, str: Math.round((worth / divs) * 1e6) / 1e6 };
}

/** The living fortress row of a nation by its name, or undefined. */
const living = (g: Game, nat: string, name: string): Corps | undefined => g.corps.find((c) => !c.dead && c.nat === nat && c.kind === 'fort' && c.name === name);

/** Forms a row where its country still holds the ground: never on ground it has lost. Returns the corps, or null. */
function form(g: Game, row: FortRow): Corps | null {
  const [, nat, name, place, worth] = row;
  const reg = g.byName[place];
  if (!reg || !g.nations[nat]?.alive || reg.ctrl !== nat || living(g, nat, name)) return null;
  const { divs, str } = fortShape(worth);
  const c = addCorps(g, nat, name, 'fort', divs, reg.id, { hold: 1, est: str });
  c.str = str;
  return c;
}

/** A row that joins another (the Guardia alla Frontiera's growth, June 1940): the named row grows by its worth, never past two divisions full. */
function grow(g: Game, c: Corps, worth: number): void {
  const total = Math.min(2, c.divs * c.str + worth);
  const { divs, str } = fortShape(total);
  if (divs > c.divs) growCorps(g, c, divs - c.divs, 'fort');
  c.str = str;
  if (str < 1) c.full = str; else delete c.full;
}

/** The first day of a 1939 grand campaign with the fortress rows on: the rows of turn 1 stand, before anything is counted from the roster. */
export function raiseForts(g: Game): void {
  if (g.turn !== 1 || !fortRows(g)) return;
  for (const row of FORT_ROWS) if (row[0] === 1) form(g, row);
}

/**
 * A nation's month, as its dated formations arrive (game.ts startPhase): the rows of this month form or join theirs,
 * and at stops 1-2 a row whose orders took it out on this date leaves its works, its men to the reserve. A row already
 * shut in its works by the enemy stays there.
 */
export function fortsMonth(g: Game, nat: string): void {
  if (!fortRows(g)) return;
  for (const row of FORT_ROWS) {
    if (row[1] !== nat) continue;
    const o = row[5];
    if (row[0] === g.turn && row[0] > 1) {
      const into = o.joins ? living(g, nat, o.joins) : undefined;
      if (into) grow(g, into, row[4]);
      else form(g, row);
    }
    if (o.out === g.turn && stopOf(g) <= 2) {
      const c = living(g, nat, row[2]);
      if (!c || c.town !== undefined) continue;
      const where = g.regions[c.reg];
      returnMen(g, nat, c.divs * c.str);
      removeCorps(g, c);
      headline(g, 'event', `${c.name}: the crews are ordered out of their works at ${where.name} and go back to the depots.`, nat, where.id);
    }
  }
}
