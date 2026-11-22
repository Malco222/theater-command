// The war plan (after release 1). A computer staff that thinks a season ahead: it gathers the army for a campaign
// where a front has gone to sleep, throws an enemy foothold back into the sea before it grows, sends an army that
// has won its theatre back into the war, and reckons an invasion as a campaign: the waves that follow the first, the
// enemy's reply, and the troopships it lays down for it. It keeps the force of an operation it means to launch, keeps
// the harbour of a foothold, and feeds an operation's landing from the day it is made. The numbers are in
// data/campaign/strategy.json; the deciding is in staff.ts (the campaign, the idle armies, the operations) and ai.ts
// (the armies and the ships). Off by default: with the switch off nothing here is read and the game is release 1's.

import data from '../../data/campaign/strategy.json';
import { bookOnlyOn, stopOf } from './history';
import type { Game } from './types';

export const STRATEGY = data as unknown as {
  on: boolean; stops: number[]; staffs: string[];
  campaign: { quiet: number; few: number; ratio: [number, number]; strike: number; go: number; prep: [number, number]; push: number; rest: number; reach: number; doors: number; tanks: number; most: number; road: number; beside: number; works: number; least: number; own: number };
  foothold: { size: number; grown: number; safe: number; go: number; push: number; doors: number; tanks: number; most: number };
  recall?: { on: boolean; quiet: number; least: number; most: number; risk: number };
  landing?: { second: boolean; waves?: { within: number; share: number; reply: number; tanks?: number; ships?: number } };
  /** The operations and the landings (their own switch and stops: the faults they mend are the usual game's, and par is read at stop 3). */
  ops?: { on: boolean; stops: number[]; keep: boolean; lead?: number; front: boolean;
    /**
     * `held`, `reach`: the guard as the design director ruled it on 22 November 2026 (19:36Z). The corps the guard
     * keeps are held where they stand for the month (a keep that marches away is no keep), and the enemy it reckons
     * with is every enemy corps the staff can see that could strike the ground within a month's march (the reach the
     * player is shown, marches.ts), in place of those on the ground and next to it.
     */
    home?: { guard: number; risk: number; held?: boolean; reach?: boolean }; port?: { size: number; within: number };
    /**
     * The coast watch on every coast a side holds (the same ruling): each country's coast is a watch of its own, the
     * homeland's and each conquered country's, a corps a harbour up to the sum of the sea lift of every enemy whose
     * ships reach it, with no share of the army set over it (open.json 'coast.share' and 'coast.least' are the
     * homeland watch's as shipped).
     */
    watch?: boolean;
    /** The landing as the design director ruled it on 22 November 2026: the first wave, the quay, the wait for the sky, the fleets, the beach's worth. */
    landing?: { on: boolean; wave: boolean; quay: boolean; wait: boolean; defer: boolean; allies?: boolean; patience?: number; fleet: boolean; beyond?: { marches: number; share: number } } };
  /**
   * Operation Torch by the book (the design director's rulings of 22 November 2026; its own switch and stops): the
   * choices of open.json that are the book's alone, the event that is the French turn, and the months before the day
   * that a landing's own corps are kept at their quay.
   */
  torch?: { on: boolean; stops: number[]; ops: string[]; turn: string; lead?: number;
    /** A key of its own, off (struck by the design director, 22 November 2026): a force thrown back on the day goes ashore at the French turn. Kept only so that the wars read with it can be run again. */
    ceasefire?: boolean; week?: number };
  /** The one-map fault's mend (the design director, 22 November 2026, 13:27Z; its own key, off): a front counts only what the month's moves can bring. */
  fronts?: { month: boolean };
  /**
   * The strait: every short crossing is water (a key of its own). `more`: borders of the map that are crossings too
   * (the Dardanelles); `cut`: borders that are open sea, where no army goes but by a landing (the Kattegat); `craft`:
   * what a crossing's own craft carry in a month to a bank the side holds, in divisions' worth, the record's busiest
   * month there; `lift`: the record's sea lift for a nation whose sealift is a stand-in, in corps a month, read at
   * the crossings (on the seas named, where any are); `bridges`: a crossing that is a land link for some months
   * while one country holds both banks (the Kerch railway bridge).
   */
  straits?: {
    on: boolean;
    more?: [string, string][];
    cut?: [string, string][];
    craft?: [string, string, number][];
    lift?: Record<string, { n: number; seas?: string[] }>;
    bridges?: { pair: [string, string]; from: number; to: number; holds: string; since?: number }[];
  };
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
 * True where Operation Torch is the book's (strategy.json 'torch'): the grand campaign, at the stops that switch
 * names. Its own switch: the operations' and the war plan's need not be on. Off, nothing of it is read.
 */
export const torchBook = (g: Game): boolean => !!STRATEGY.torch?.on && g.flags.open === 1 && STRATEGY.torch.stops.includes(stopOf(g));
/**
 * The week of the landing's month in which the book's French turn falls, where Torch is the book's (strategy.json
 * 'torch.week'; the design director, 22 November 2026, 13:27Z): the turn fires right after the landings, a landing's
 * fight is cut short by it, and both sides lose that many quarters of the month's toll. 0 where the turn keeps the
 * month's end and the fight its month.
 */
export const torchWeek = (g: Game): number => (torchBook(g) ? Math.max(0, Math.min(4, STRATEGY.torch!.week ?? 0)) : 0);
/**
 * True where a staff's front counts only what the month's moves can bring to it (strategy.json 'fronts.month'): the
 * grand campaign, every staff, every stop. A key of its own; off, the sum is the one every war was measured with.
 */
export const frontMonth = (g: Game): boolean => !!STRATEGY.fronts?.month && g.flags.open === 1;
/**
 * True where every short crossing of the sea is water (strategy.json 'straits'; the design director, 22 November
 * 2026, 09:20Z): the grand campaign, every nation, every stop. A key of its own; off, a crossing is a land border.
 */
export const straitsOn = (g: Game): boolean => !!STRATEGY.straits?.on && g.flags.open === 1;
/** True for the event that is the French turn where Torch is the book's: its partition runs as the record wrote it. */
export const torchTurn = (g: Game, eventId: string): boolean => torchBook(g) && STRATEGY.torch!.turn === eventId;
// (The book's word on its own choices: history.ts asks here whether one of them is switched on.)
bookOnlyOn.test = (g, nat, opId) => torchBook(g) && STRATEGY.torch!.ops.includes(`${nat}:${opId}`);

/**
 * What a campaign's sector is worth beyond the staff's own sums: a hook for the terms other rules add (ruling 239's
 * item 4, the corps a capture would cut off). Each takes the game, the attacker and the target province's id.
 */
export const sectorTerms: ((g: Game, nat: string, target: number) => number)[] = [];

/** A listener for what the staffs decide about their campaigns (the test runs read it; the game sets none). */
export const campaignLog: { say: ((g: Game, nat: string, what: string, where: string, more?: Record<string, number>) => void) | null } = { say: null };
