/**
 * The war's own clock, for the sandbox stops (4 and 5) of the history dial: a nation's later armies are raised for
 * the war it is fighting, so they come that many months into its own great war, not on the record's calendar
 * (the sandbox playtest's lock sweep, finding 3). Stops 1 to 3 never read this.
 */
import data from '../../data/campaign/war-clock.json';
import { majorsOf } from './diplomacy';
import { atWar } from './game';
import { stopDef } from './history';
import type { Game } from './types';

export interface ClockRow { war: number; from: number; foes: '*' | string[] }
export const WAR_CLOCK = data.nations as Record<string, ClockRow>;
/** Rows that come only in some wars: at peace with every nation of 'peaceWith', and at war with one of 'warWith'. */
const ROW_WARS = data.rows as Record<string, { peaceWith?: string[]; warWith?: string[] }>;

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

/** The share of the factories on its own first-day ground that the nation holds now (1 where it had none). */
export function factoriesHeld(g: Game, nat: string): number {
  let all = 0, held = 0;
  for (const r of g.regions) if ((r.first ?? r.owner) === nat) { all += r.fac; if (r.ctrl === nat) held += r.fac; }
  return all > 0 ? held / all : 1;
}

/** The corps a nation has lost, and the enemy corps it has destroyed, from month 'since' on (the memorial wall). */
export function corpsTrade(g: Game, nat: string, since: number): { lost: number; destroyed: number } {
  let lost = 0, destroyed = 0;
  for (const f of g.stories?.fallen ?? []) {
    if (f.turn < since) continue;
    if (f.nat === nat) lost++;
    else if (f.by === nat) destroyed++;
  }
  return { lost, destroyed };
}

/** The months in which the nation lost or destroyed at least one corps. */
export function combatMonths(g: Game, nat: string): number {
  const months = new Set<number>();
  for (const f of g.stories?.fallen ?? []) if (f.nat === nat || f.by === nat) months.add(f.turn);
  return months.size;
}

/** The most field corps the nation has had at once, as counted each month at the sandbox stops ('peak:NAT'). */
export const peakCorps = (g: Game, nat: string): number => g.flags[`peak:${nat}`] ?? 0;

/** Counts the nation's field corps for its peak: at the sandbox stops of the open grand campaign only. */
export function notePeak(g: Game, nat: string): void {
  if (g.flags.open !== 1 || !stopDef(g).ambitions) return;
  let n = 0;
  for (const c of g.corps) if (c.nat === nat && !c.dead && c.kind !== 'fort') n++;
  if (n > (g.flags[`peak:${nat}`] ?? 0)) g.flags[`peak:${nat}`] = n;
}
