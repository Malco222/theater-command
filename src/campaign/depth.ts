// How deep the player plays (the grand campaign): which parts of the war are on his own desk and which his staff
// runs for him. This file holds only the switches, so that every staff routine can ask it who is in charge without
// pulling the rest of the game in. What the staff does with a desk is in desks.ts; the names, the three depths and
// the spending cap are in data/campaign/depth.json.
//
// A switch is a flag, 'dk:<desk>' = 1 while the staff runs that desk for the player. With no flag set the game is
// the full game, as it was before there were depths. Computer nations take no notice of any of it.

import data from '../../data/campaign/depth.json';
import type { Game } from './types';

export type DeskId = 'research' | 'build' | 'diplomacy' | 'spies' | 'air' | 'navy';

export interface DeskDef {
  id: DeskId; name: string;
  /** The panel that belongs to this desk, where it has one. */
  tab?: string;
  /** Who does the work, as the month's news names them. */
  lead: string;
  /** What the staff does with it, and what running it by hand means. */
  runs: string; hand: string;
}

export interface DepthDef {
  id: string; name: string;
  /** The plain word under the name: Simple, Standard, Full. */
  under: string;
  blurb: string;
  staff: DeskId[];
  cabinet: boolean; canFall: boolean; fog: boolean;
}

export const DEPTH = data as unknown as {
  start: string;
  cap: { steps: number[]; start: number };
  diplomacy: { keep: number };
  spies: { guards: number; peace: boolean };
  desks: DeskDef[];
  depths: DepthDef[];
};

export const DESKS: DeskId[] = DEPTH.desks.map((d) => d.id);
export const deskDef = (id: string): DeskDef | undefined => DEPTH.desks.find((d) => d.id === id);
export const depthDef = (id: string): DepthDef | undefined => DEPTH.depths.find((d) => d.id === id);

/** Depths belong to the grand campaign, and to a game somebody is playing. */
export const depthRules = (g: Game): boolean => g.flags.open === 1 && g.human !== null;

const key = (desk: DeskId): string => `dk:${desk}`;

/** True while the player's staff runs this desk for him. Never true for a computer nation, which runs everything itself. */
export const staffRuns = (g: Game, nat: string | null, desk: DeskId): boolean => g.flags.open === 1 && nat !== null && nat === g.human && g.flags[key(desk)] === 1;

/**
 * True where a staff routine must keep its hands off: this is the player's nation and he runs this desk himself.
 * Every routine a computer nation's staff uses asks this before it acts.
 */
export const byHand = (g: Game, nat: string, desk: DeskId): boolean => nat === g.human && g.flags[key(desk)] !== 1;

/** Hands a desk to the staff, or takes it back. Returns false where nothing changed. */
export function setDesk(g: Game, desk: DeskId, staff: boolean): boolean {
  if (!depthRules(g) || !DESKS.includes(desk) || (g.flags[key(desk)] === 1) === staff) return false;
  if (staff) g.flags[key(desk)] = 1; else delete g.flags[key(desk)];
  return true;
}

/** The share of the treasury the staff may spend in a month, in percent. */
export const capPct = (g: Game): number => g.flags['dk:cap'] ?? DEPTH.cap.start;
/** The same as a share, 0 to 1. */
export const capOf = (g: Game): number => capPct(g) / 100;

/** Sets what the staff may spend: one of the steps on offer. */
export function setCap(g: Game, pct: number): boolean {
  if (!depthRules(g) || !DEPTH.cap.steps.includes(pct) || capPct(g) === pct) return false;
  if (pct === DEPTH.cap.start) delete g.flags['dk:cap']; else g.flags['dk:cap'] = pct;
  return true;
}

/** True if this desk's staff has already done its month's work: a desk handed over twice in a month does not act twice. */
export const deskRan = (g: Game, desk: DeskId): boolean => g.flags[`dk:ran:${desk}`] === g.turn;
export const markRan = (g: Game, desk: DeskId): void => { g.flags[`dk:ran:${desk}`] = g.turn; };
