/**
 * The war's own clock, for the sandbox stops (4 and 5) of the history dial: a nation's later armies are raised for
 * the war it is fighting, so they come that many months into its own great war, not on the record's calendar
 * (the sandbox playtest's lock sweep, finding 3). Stops 1 to 3 never read this.
 */
import data from '../../data/campaign/war-clock.json';
import { majorsOf } from './diplomacy';
import { atWar } from './game';
import { stopDef } from './history';
import type { Corps, Game } from './types';

export interface ClockRow { war: number; from: number; foes: '*' | string[] }
export const WAR_CLOCK = data.nations as Record<string, ClockRow>;
/** Rows that come only in some wars: at peace with every nation of 'peaceWith', and at war with one of 'warWith'. */
const ROW_WARS = data.rows as Record<string, { peaceWith?: string[]; warWith?: string[] }>;
/** The record's month of the phase that freed a held row (the hold rule): 'holds' in war-clock.json. */
const HOLDS = (data as { holds?: Record<string, { release?: number }> }).holds ?? {};

/** True where a row on the clock may come this month: the wars it was raised for, or kept back by, are as it needs. */
export function rowWarOn(g: Game, nat: string, name: string): boolean {
  const w = ROW_WARS[name];
  if (!w) return true;
  const on = (f: string): boolean => !!g.nations[f]?.alive && atWar(g, nat, f);
  return !(w.peaceWith ?? []).some(on) && (!w.warWith || w.warWith.some(on));
}

/** The clock this nation's rows run on in this game: only at the sandbox stops of the open grand campaign. */
export const clockOf = (g: Game, nat: string): ClockRow | undefined =>
  g.flags.open === 1 && stopDef(g).ambitions ? WAR_CLOCK[nat] : undefined;

/** The month this nation's great war began, once it has one (kept in the flag 'gw:NAT'); undefined before. */
export const greatWarOf = (g: Game, nat: string): number | undefined => g.flags[`gw:${nat}`];

/**
 * Starts the clock the first month the nation is at war with one of its clock's foes: the month that war was
 * declared, or, for a war already on when the campaign began, the record's month (or this one, if earlier).
 */
export function startClock(g: Game, nat: string, clock: ClockRow): number | undefined {
  const k = `gw:${nat}`;
  if (g.flags[k] !== undefined) return g.flags[k];
  const foes = (clock.foes === '*' ? majorsOf(g) : clock.foes).filter((f) => f !== nat && g.nations[f]?.alive && atWar(g, nat, f));
  if (!foes.length) return undefined;
  let first = Infinity;
  for (const f of foes) for (const w of [`war:${nat}:${f}`, `war:${f}:${nat}`]) if (g.flags[w] !== undefined) first = Math.min(first, g.flags[w]);
  g.flags[k] = first === Infinity ? Math.min(clock.war, g.turn) : first;
  return g.flags[k];
}

/** The month a row dated 'month' is due on the clock, given the month the great war began. */
export const dueOn = (clock: ClockRow, began: number, month: number): number => began + Math.max(0, month - clock.war);

/** Months the war between two nations has run: from its declaration, or from month 1 for a war already on at the start. */
export function warMonths(g: Game, a: string, b: string): number {
  if (!atWar(g, a, b)) return -1;
  let first = Infinity;
  for (const w of [`war:${a}:${b}`, `war:${b}:${a}`]) if (g.flags[w] !== undefined) first = Math.min(first, g.flags[w]);
  return g.turn - (first === Infinity ? 1 : first);
}

/**
 * The hold rule (the design director, 20 November 05:35Z): a held row on the clock keeps its hold, and loses it to
 * its own cause, whichever comes first: the blow lands on it (struck), or the record's phase that freed it comes due
 * on its own war's clock (holdsDue). Its corps carries the month that phase is due in 'wh:ID' (-1: none, so only
 * the blow or the operation that sends it frees it).
 */
export function holdOnClock(g: Game, c: Corps, clock: ClockRow, began: number, name: string): void {
  if (!c.hold) return;
  const m = HOLDS[name]?.release;
  g.flags[`wh:${c.id}`] = m === undefined ? -1 : dueOn(clock, began, m);
}

/** A nation's month opening: its held rows whose freeing phase has come due go in with the rest. */
export function holdsDue(g: Game, nat: string): void {
  for (const c of g.corps) {
    if (c.nat !== nat) continue;
    const k = `wh:${c.id}`;
    const due = g.flags[k];
    if (due === undefined) continue;
    // Freed some other way (sent overseas, called home), or gone: the rule has nothing left to keep.
    if (c.dead || !c.hold) delete g.flags[k];
    else if (due >= 0 && g.turn >= due) {
      c.hold = false;
      delete g.flags[k];
    }
  }
}

/** The blow lands on it: a held row on the clock that is attacked where it stands loses its hold. */
export function struck(g: Game, defenders: { name: string; nat: string }[]): void {
  if (g.flags.open !== 1 || !stopDef(g).ambitions) return;
  for (const c of g.corps) {
    if (!c.hold || g.flags[`wh:${c.id}`] === undefined) continue;
    if (!defenders.some((u) => u.nat === c.nat && u.name === c.name)) continue;
    c.hold = false;
    delete g.flags[`wh:${c.id}`];
  }
}
