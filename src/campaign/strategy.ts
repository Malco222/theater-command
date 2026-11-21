// The war plan (after release 1). A computer staff that thinks a season ahead: it gathers the army for a campaign
// where a front has gone to sleep, throws an enemy foothold back into the sea before it grows, sends an army that
// has won its theatre back into the war, and reckons an invasion as a campaign: the waves that follow the first, the
// enemy's reply, and the troopships it lays down for it. The numbers are in data/campaign/strategy.json; the deciding is in
// staff.ts (the campaign, the idle armies) and ai.ts (the armies and the ships). Off by default: with the switch off
// nothing here is read and the game is release 1's.

import data from '../../data/campaign/strategy.json';
import { stopOf } from './history';
import type { Game } from './types';

export const STRATEGY = data as unknown as {
  on: boolean; stops: number[]; staffs: string[];
  campaign: { quiet: number; few: number; ratio: [number, number]; strike: number; go: number; prep: [number, number]; push: number; rest: number; reach: number; doors: number; tanks: number; most: number; road: number; beside: number; works: number; least: number; own: number };
  foothold: { size: number; grown: number; safe: number; go: number; push: number; doors: number; tanks: number; most: number };
  recall?: { on: boolean; quiet: number; least: number; most: number; risk: number };
  landing?: { second: boolean; waves?: { within: number; share: number; reply: number; tanks?: number; ships?: number } };
};

/** True where a computer staff plans campaigns: the grand campaign, at the dial's stops named, never the player's own nation. */
export const plansAhead = (g: Game, nat: string): boolean =>
  STRATEGY.on && g.flags.open === 1 && nat !== g.human && STRATEGY.stops.includes(stopOf(g)) && (!STRATEGY.staffs.length || STRATEGY.staffs.includes(nat));

/**
 * What a campaign's sector is worth beyond the staff's own sums: a hook for the terms other rules add (ruling 239's
 * item 4, the corps a capture would cut off). Each takes the game, the attacker and the target province's id.
 */
export const sectorTerms: ((g: Game, nat: string, target: number) => number)[] = [];

/** A listener for what the staffs decide about their campaigns (the test runs read it; the game sets none). */
export const campaignLog: { say: ((g: Game, nat: string, what: string, where: string, more?: Record<string, number>) => void) | null } = { say: null };
