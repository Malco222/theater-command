// Rearguards (release 2; Brian, 24 November 2026 18:24Z, "i like these", of the idea "a split-off division can hold a
// province for a month while your main army pulls back, slowing the enemy's pursuit"). Behind its own switch
// (stage.json 'rearguard', the flag 'rearguard'): off, nothing here is read and every war is the war without it.
//
// A small corps of the player's (a detachment split off a bigger one, breakup.ts, is the natural one) is ordered to hold
// where it stands until his next month. It gives up its own marching for the month. An enemy army that attacks its
// province before then pays for it in time: each of its corps loses weeks of its month (data/campaign/rearguard.json),
// so it cannot press on as far behind the army that pulled back. The fight itself is fought as any other. Under Units
// decide the rearguard also screens the corps of its side falling back into its province: the pursuit takes none of
// their men there. No computer nation sets one.

import data from '../../data/campaign/rearguard.json';
import type { Corps, Game, Region } from './types';

// (Who is friend and who foe is asked of game.ts by its caller, so that this file and game.ts do not import each other.)

export const REARGUARD = data as unknown as { most: number; weeksPerDiv: number; weeks: number; screen: boolean };

/** True where the player may order rearguards: the grand campaign, with the switch on. */
export const rearguardOn = (g: Game): boolean => g.flags.open === 1 && g.flags.rearguard === 1;

/** Turns rearguards on or off in a campaign under way. Off, those standing are stood down. */
export function setRearguard(g: Game, on: boolean): void {
  if (on) g.flags.rearguard = 1;
  else {
    delete g.flags.rearguard;
    for (const c of g.corps) if (c.rear) standDown(g, c);
  }
}

const hisMonth = (g: Game): boolean => g.human !== null && g.order[g.phase] === g.human;

/** True while this corps holds as a rearguard: ordered, and its side's next month not yet begun. */
export const isRearguard = (g: Game, c: Corps): boolean => rearguardOn(g) && !!c.rear && !c.dead;

/** Why this corps cannot be ordered to hold as a rearguard now, or null when it can. */
export function whyNotRearguard(g: Game, c: Corps | undefined): string | null {
  if (!rearguardOn(g)) return 'Rearguards are ordered only where that is switched on.';
  if (!c || c.dead) return 'There is no such corps.';
  if (c.nat !== g.human || !hisMonth(g)) return 'Only your own corps, in your own month.';
  if (c.rear) return 'It holds as a rearguard already.';
  if (c.kind === 'fort') return 'Fortress troops hold their works already.';
  if (c.town !== undefined) return 'A garrison holds its town already.';
  if (c.joint !== undefined) return "It marches with an ally's army, and takes that army's orders.";
  if (c.leave !== undefined) return 'It is at home, out of the line.';
  if (c.divs > REARGUARD.most) return `A rearguard is a small force: ${REARGUARD.most} divisions at most. Split some off first.`;
  return null;
}

/** Orders a corps to hold where it stands as a rearguard until its side's next month. False where it cannot be. */
export function orderRearguard(g: Game, c: Corps): boolean {
  if (whyNotRearguard(g, c) !== null) return false;
  c.rear = [g.turn, c.ap];
  c.ap = 0;
  return true;
}

/** Stands a rearguard down: in the month it was ordered it has its marching back. */
export function standDown(g: Game, c: Corps): boolean {
  if (!c.rear) return false;
  if (c.rear[0] === g.turn && hisMonth(g) && c.nat === g.human) c.ap = Math.max(c.ap, c.rear[1]);
  delete c.rear;
  return true;
}

/** At the start of a nation's month its rearguards of the month before are stood down. */
export function rearguardsLapse(g: Game, nat: string): void {
  for (const c of g.corps) if (c.nat === nat && c.rear && c.rear[0] < g.turn) delete c.rear;
}

/** The rearguards standing in a province against an attack: in the field, of a side the attackers are at war with ('foe'). */
export function rearguardsAt(g: Game, r: Region, foe: (nat: string) => boolean): Corps[] {
  if (!rearguardOn(g)) return [];
  return r.units.filter((c) => isRearguard(g, c) && c.reg === r.id && c.town === undefined && foe(c.nat));
}

/** The weeks each attacking corps loses to the rearguards of a province: by their divisions, at most REARGUARD.weeks. */
export function rearguardWeeks(rears: Corps[]): number {
  const divs = rears.reduce((n, c) => n + c.divs, 0);
  return Math.min(REARGUARD.weeks, divs * REARGUARD.weeksPerDiv);
}

/** Charges the attackers the time the rearguards cost them; returns the weeks charged (0 where none stood there). */
export function chargeDelay(attackers: Corps[], weeks: number): number {
  if (weeks <= 0) return 0;
  for (const c of attackers) if (!c.dead) c.ap = Math.max(0, c.ap - weeks);
  return weeks;
}

/**
 * Under Units decide: whether a rearguard of this corps' own side ('friend') standing in 'to' screens it from the
 * pursuit as it falls back there.
 */
export function screened(g: Game, c: Corps, to: Region, friend: (nat: string) => boolean): boolean {
  if (!rearguardOn(g) || !REARGUARD.screen) return false;
  return to.units.some((u) => u !== c && !u.dead && isRearguard(g, u) && u.town === undefined && friend(u.nat));
}
