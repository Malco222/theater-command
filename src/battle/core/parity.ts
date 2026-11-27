// Parity: a played battle must mean the same as the dice when nobody plays it well. With the computer on both sides,
// a sim's win rate across a spread of odds is compared with the chance the rules' dice give at the same odds
// (chanceOn in game.ts). Every domain's tests use this one helper, so "fair" means the same thing on land, at sea
// and in the air.

import { runOut } from './clock';
import type { BattleSim, FieldTicket } from './types';

/**
 * The rules' chance of carrying the day at these odds with this run of luck (the same sum as chanceOn in game.ts).
 * 'carry' is the ratio that wins: 1.2 on land; a domain whose rules draw the line elsewhere passes its own (sea: 1).
 */
export const diceChance = (odds: number, dice: [number, number], carry = 1.2): number => Math.max(0, Math.min(1, (dice[1] - carry / Math.max(1e-6, odds)) / (dice[1] - dice[0])));

export interface ParityRow { odds: number; played: number; dice: number; runs: number; R: number }

/**
 * For each odds, 'runs' battles with the computer on both sides: the share the attacker won, the dice's chance, and
 * the average R handed back. 'make' builds a sim for a ticket at those odds and that seed.
 */
export function parity(make: (odds: number, seed: number) => BattleSim, odds: number[], runs: number, dice: [number, number] = [0.8, 1.2], carry = 1.2): ParityRow[] {
  return odds.map((o) => {
    let won = 0, R = 0;
    for (let s = 1; s <= runs; s++) {
      const sim = make(o, s);
      sim.ai.att = true; sim.ai.def = true;
      runOut(sim);
      if (sim.over?.winner === 'att') won++;
      R += sim.over?.R ?? 0;
    }
    return { odds: o, played: won / runs, dice: diceChance(o, dice, carry), runs, R: R / runs };
  });
}

/** A ticket's own odds are the field's: scales the defence of a ticket to give the odds asked for (tests and calibration only). */
export function atOdds<P>(t: FieldTicket<P>, odds: number): FieldTicket<P> {
  return { ...t, reckoning: { ...t.reckoning, odds, D: t.reckoning.A / odds } };
}
