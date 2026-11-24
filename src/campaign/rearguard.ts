// Rearguards (release 2; Brian, 24 November 2026 18:24Z, "i like these", of the idea "a split-off division can hold a
// province for a month while your main army pulls back, slowing the enemy's pursuit"). Behind its own switch
// (stage.json 'rearguard', the flag 'rearguard'): off, nothing here is read and every war is the war without it.
//
// A small corps (a detachment split off a bigger one, breakup.ts, is the natural one) is ordered to hold where it
// stands until its side's next month. It gives up its own marching for the month. The design director's ruling of
// 24 November 19:18Z (on the historian's read: the odds act only through the rearguard's own fight): all the attackers
// in its province fight it in one fight and share its delay, each losing it once; held, a week per rearguard division
// (data/campaign/rearguard.json), broken, half that; the time is lost before the pursuit is run. The rearguard fights
// as any corps and is chased and cut off by the same pursuit; driven out of its province it is a rearguard no longer.
// Under Units decide a rearguard standing where a beaten corps of its side falls back acts on the pursuit's race only
// through time: the pursuers lose its weeks first. The computer staffs order rearguards by the same rule (ai.ts,
// aiRearguards).

import data from '../../data/campaign/rearguard.json';
import type { Corps, Game, Region } from './types';

// (Who is friend and who foe is asked of game.ts by its caller, so that this file and game.ts do not import each other.)

export const REARGUARD = data as unknown as { most: number; weeksPerDiv: number; weeks: number; broken: number; screen: boolean; ai: { odds: number; perMonth: number }; bridges: number };

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

/**
 * True where rearguards blow the bridges behind them (release 2; Brian 24 November 2026 19:47Z, "yes to all"): its own
 * switch (stage.json 'rearBridges', the flag 'rearBridges'), read only with rearguards on. Off, the bridges of a province
 * a rearguard held go up on the usual chance (rivers.ts bridgesOnCapture), as any other province's.
 */
export const rearBridgesOn = (g: Game): boolean => rearguardOn(g) && g.flags.rearBridges === 1;

/** Turns the rearguards' bridges on or off in a campaign under way. */
export function setRearBridges(g: Game, on: boolean): void {
  if (on) g.flags.rearBridges = 1;
  else delete g.flags.rearBridges;
}

/**
 * True where a rearguard of the side `ofSide` names held province `region` and had the charges laid: ordered this month
 * or last and not yet stood down, whether it was driven out or destroyed there. Only with the bridges' switch on.
 */
export const chargesLaid = (g: Game, region: number, ofSide: (nat: string) => boolean): boolean =>
  rearBridgesOn(g) && g.corps.some((c) => !!c.rear && c.rear[2] === region && ofSide(c.nat));

/** True while this corps holds as a rearguard: ordered, still in the province it holds, its side's next month not yet begun. */
export const isRearguard = (g: Game, c: Corps): boolean => rearguardOn(g) && !!c.rear && !c.dead && c.reg === c.rear[2] && c.town === undefined;

/** Why this corps cannot be ordered to hold as a rearguard now, or null when it can. */
export function whyNotRearguard(g: Game, c: Corps | undefined): string | null {
  if (!rearguardOn(g)) return 'Rearguards are ordered only where that is switched on.';
  if (!c || c.dead) return 'There is no such corps.';
  if (c.nat !== g.human || !hisMonth(g)) return 'Only your own corps, in your own month.';
  return ruleOut(c);
}

/** The order's own rule, the same for the player and the computer staffs: why this corps cannot hold, or null. */
function ruleOut(c: Corps): string | null {
  if (c.rear) return 'It holds as a rearguard already.';
  if (c.kind === 'fort') return 'Fortress troops hold their works already.';
  if (c.town !== undefined) return 'A garrison holds its town already.';
  if (c.joint !== undefined) return "It marches with an ally's army, and takes that army's orders.";
  if (c.leave !== undefined) return 'It is at home, out of the line.';
  if (c.divs > REARGUARD.most) return `A rearguard is a small force: ${REARGUARD.most} divisions at most. Split some off first.`;
  return null;
}

/** Orders a corps of the player's to hold where it stands as a rearguard until his next month. False where it cannot be. */
export function orderRearguard(g: Game, c: Corps): boolean {
  if (whyNotRearguard(g, c) !== null) return false;
  hold(g, c);
  return true;
}

/** A computer staff orders a corps of its own to hold as a rearguard, in its own month, by the player's rule. */
export function staffRearguard(g: Game, c: Corps): boolean {
  if (!rearguardOn(g) || c.dead || c.nat === g.human || g.order[g.phase] !== c.nat || ruleOut(c) !== null) return false;
  hold(g, c);
  return true;
}

function hold(g: Game, c: Corps): void {
  c.rear = [g.turn, c.ap, c.reg];
  c.ap = 0;
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

/**
 * The weeks each attacking corps loses to the rearguards of a province, all of them sharing one fight's delay: a week
 * per rearguard division, REARGUARD.weeks at most, where they held; REARGUARD.broken of it where they were broken.
 */
export function rearguardWeeks(rears: Corps[], broken: boolean): number {
  const divs = rears.reduce((n, c) => n + c.divs, 0);
  if (divs <= 0) return 0;
  return Math.min(REARGUARD.weeks, divs * REARGUARD.weeksPerDiv) * (broken ? REARGUARD.broken : 1);
}

/** The time in words, to the day: "2 weeks", "1 week", "4 days", "less than a day". */
export function weeksWords(w: number): string {
  const days = Math.round(w * 7);
  if (days <= 0) return 'less than a day';
  if (days % 7 === 0) return `${days / 7} week${days === 7 ? '' : 's'}`;
  return `${days} day${days === 1 ? '' : 's'}`;
}

/** Charges the attackers the time the rearguards cost them; returns the weeks charged (0 where none stood there). */
export function chargeDelay(attackers: Corps[], weeks: number): number {
  if (weeks <= 0) return 0;
  for (const c of attackers) if (!c.dead) c.ap = Math.max(0, c.ap - weeks);
  return weeks;
}

/**
 * Under Units decide: the weeks the rearguards of this corps' own side ('friend') standing in 'to' cost each pursuer
 * in the pursuit's race as it falls back there: their delay where they hold. Time only, never a blanket exemption; a
 * rearguard never slows its own pursuers. 0 where none stands there.
 */
export function screenWeeks(g: Game, c: Corps, to: Region, friend: (nat: string) => boolean): number {
  if (!rearguardOn(g) || !REARGUARD.screen) return 0;
  return rearguardWeeks(to.units.filter((u) => u !== c && isRearguard(g, u) && friend(u.nat)), false);
}
