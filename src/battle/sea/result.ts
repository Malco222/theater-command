// From what happened on the field to where it falls on the rules' own scale for a fleet action (navy.ts seaBattles).
//
// The rules settle a fleet action with one ratio, R: the attacker's strength to the defender's, times the day's luck
// (0.8 to 1.2). The attacker wins at R of 1 or more; the further R is from 1 the more the loser loses and the less the
// winner does; at 2 or more either way the result is decisive. A played battle hands back an R of its own, worked out
// from the exchange on the field, and (ruling 217) clamped to what the dice could have rolled at the battle's odds: a
// well-fought battle can do as well as the best luck the dice allow, and never better.
//
// Unlike the land, a fleet action also hands back the ships each fleet lost, exactly: a player who watched a
// battleship go down must find it gone on the campaign map. So that this too can never go past what the dice allow,
// no side loses more ships of a class on the field than the rules would sink at its worst luck (sunkCaps): a ship
// that is beaten to a wreck once its side has lost that many goes out of the fight burning instead, and lives.

import type { VictoryLevel } from '../../core/ticket';
import type { Side } from '../core/types';
import { DICE, RULES_CLASSES } from './numbers';
import { SEA_CLASSES, type FleetTicket, type SeaClass, type SeaShips } from './types';

const clamp = (v: number, lo: number, hi: number): number => (v < lo ? lo : v > hi ? hi : v);

/**
 * The share of its ships each side loses when the strengths stand at r after the dice (the attacker's to the
 * defender's): navy.ts seaBattles, `hard` and `light`, and seaplan.ts `shares`, word for word.
 */
export function lossShares(r: number): Record<Side, number> {
  const won = r >= 1, edge = won ? r : 1 / r;
  const hard = clamp(0.14 + 0.08 * edge, 0.18, 0.5), light = clamp(0.16 / edge, 0.03, 0.14);
  return won ? { att: light, def: hard } : { att: hard, def: light };
}

/** The lowest and highest R the dice can roll at these odds. */
export const diceRange = (odds: number): [number, number] => [odds * DICE[0], odds * DICE[1]];

/** The rules' chance that the attacker wins a fleet action at these odds: R = odds x U(0.8, 1.2) reaches 1. */
export const seaChance = (odds: number): number => clamp((DICE[1] - 1 / Math.max(1e-6, odds)) / (DICE[1] - DICE[0]), 0, 1);

/** Added to both sides' losses before they are compared, so a fight with almost no losses reads as even. */
const FLOOR = 0.05;
/** How hard the exchange pushes R from 1: R = exchange ^ POWER. The rules' own shares give about this (see the parity test). */
const POWER = 0.5;

/**
 * R from a played fleet action: who held the sea, and how lopsided the exchange of weight lost was. Clamped to what
 * the dice could roll at the battle's odds (ruling 217); within that, the attacker's win is never below 1 and their
 * defeat never at 1 or above.
 */
export function seaRatio(winner: Side, lost: Record<Side, number>, odds: number): number {
  const ex = (lost.def + FLOOR) / (lost.att + FLOOR);
  let r = Math.pow(ex, POWER);
  r = winner === 'att' ? Math.max(1, r) : Math.min(0.999, r);
  const [lo, hi] = diceRange(odds);
  r = clamp(r, lo, hi);
  // Where the fight's verdict lies outside the dice's run it reaches the line and no further (ruling 217, log b331):
  // a won fight books the narrowest win, a lost one the narrowest loss, so the report never contradicts the fight.
  return winner === 'att' ? Math.max(1, r) : Math.min(0.999, r);
}

/** The level as the rules word a fleet action, read by the attacker (navy.ts seaBattles). */
export function seaLevel(R: number): VictoryLevel {
  const won = R >= 1, edge = won ? R : 1 / R;
  return won ? (edge >= 2 ? 'Decisive victory' : 'Close victory') : edge >= 2 ? 'Decisive defeat' : 'Close defeat';
}

/**
 * The most ships of each class each side can lose on the field: what the rules would sink at that side's worst luck
 * (the attacker's at the bottom of the dice, the defender's at the top), rounding each fleet's figure up as the dice
 * can (sinkShips rolls about ships x share x tough). Each fleet's tough is its own where its damage control eases it
 * (ruling 240: the ticket's tough), else navy.json's; 'plain' reads navy.json's for every fleet, the caps the field
 * would have had without damage control.
 */
export function sunkCaps(fleets: FleetTicket[], odds: number, plain = false): Record<Side, SeaShips> {
  const [lo, hi] = diceRange(odds);
  const worst: Record<Side, number> = { att: lossShares(lo).att, def: lossShares(hi).def };
  const out: Record<Side, SeaShips> = { att: none(), def: none() };
  for (const f of fleets) for (const c of SEA_CLASSES) {
    out[f.side][c] += Math.min(f.ships[c], Math.ceil(f.ships[c] * worst[f.side] * ((plain ? undefined : f.tough?.[c]) ?? RULES_CLASSES[c].tough) - 1e-9));
  }
  return out;
}

export const none = (): SeaShips => ({ bb: 0, cv: 0, ca: 0, dd: 0 });
export const shipsIn = (s: SeaShips): number => s.bb + s.cv + s.ca + s.dd;

/** A fleet's ships listed as the rules list them: "2 battleships, 4 destroyers". */
export function shipWords(s: SeaShips): string {
  const parts: string[] = [];
  for (const c of SEA_CLASSES as SeaClass[]) if (s[c] > 0) parts.push(`${s[c]} ${s[c] === 1 ? RULES_CLASSES[c].one : RULES_CLASSES[c].many}`);
  return parts.length ? parts.join(', ') : 'no ships';
}
