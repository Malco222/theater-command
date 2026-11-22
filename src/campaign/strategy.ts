// The war plan (after release 1). A computer staff that thinks a season ahead: it gathers the army for a campaign
// where a front has gone to sleep, throws an enemy foothold back into the sea before it grows, sends an army that
// has won its theatre back into the war, and reckons an invasion as a campaign: the waves that follow the first, the
// enemy's reply, and the troopships it lays down for it. It keeps the force of an operation it means to launch, keeps
// the harbour of a foothold, and feeds an operation's landing from the day it is made. The numbers are in
// data/campaign/strategy.json; the deciding is in staff.ts (the campaign, the idle armies, the operations) and ai.ts
// (the armies and the ships). Off by default: with the switch off nothing here is read and the game is release 1's.

import data from '../../data/campaign/strategy.json';
import { stopOf } from './history';
import type { Game } from './types';

export const STRATEGY = data as unknown as {
  on: boolean; stops: number[]; staffs: string[];
  campaign: { quiet: number; few: number; ratio: [number, number]; strike: number; go: number; prep: [number, number]; push: number; rest: number; reach: number; doors: number; tanks: number; most: number; road: number; beside: number; works: number; least: number; own: number };
  foothold: { size: number; grown: number; safe: number; go: number; push: number; doors: number; tanks: number; most: number };
  recall?: { on: boolean; quiet: number; least: number; most: number; risk: number };
  landing?: { second: boolean; waves?: { within: number; share: number; reply: number; tanks?: number; ships?: number } };
  /** The operations and the landings (their own switch and stops: the faults they mend are the usual game's, and par is read at stop 3). */
  ops?: { on: boolean; stops: number[]; keep: boolean; lead?: number; front: boolean; home?: { guard: number; risk: number }; port?: { size: number; within: number };
    /** The landing as the design director ruled it on 22 November 2026: the first wave, the quay, the wait for the sky, the fleets, the beach's worth. */
    landing?: { on: boolean; wave: boolean; quay: boolean; wait: boolean; defer: boolean; allies?: boolean; patience?: number; fleet: boolean; beyond?: { marches: number; share: number } } };
};

/** True where a computer staff plans campaigns: the grand campaign, at the dial's stops named, never the player's own nation. */
export const plansAhead = (g: Game, nat: string): boolean =>
  STRATEGY.on && g.flags.open === 1 && nat !== g.human && STRATEGY.stops.includes(stopOf(g)) && (!STRATEGY.staffs.length || STRATEGY.staffs.includes(nat));

/**
 * True where a computer staff mounts its operations and its landings as wholes (strategy.json 'ops'): the grand
 * campaign, at the stops that switch names, every computer-run nation. It is a switch of its own, apart from the
 * war plan's: a staff need not plan campaigns to keep the force of an operation together.
 */
export const mounts = (g: Game, nat: string): boolean =>
  !!STRATEGY.ops?.on && g.flags.open === 1 && nat !== g.human && STRATEGY.ops.stops.includes(stopOf(g));

/**
 * What a campaign's sector is worth beyond the staff's own sums: a hook for the terms other rules add (ruling 239's
 * item 4, the corps a capture would cut off). Each takes the game, the attacker and the target province's id.
 */
export const sectorTerms: ((g: Game, nat: string, target: number) => number)[] = [];

/** A listener for what the staffs decide about their campaigns (the test runs read it; the game sets none). */
export const campaignLog: { say: ((g: Game, nat: string, what: string, where: string, more?: Record<string, number>) => void) | null } = { say: null };
