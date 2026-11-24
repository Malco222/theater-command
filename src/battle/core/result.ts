// From what happened on the field to where it falls on the campaign's scale.
//
// The campaign settles every battle with one ratio, R: the odds times the day's luck. R of 1.2 or more carries the
// ground; 3 is a heroic victory, 2 decisive, under 0.8 a decisive defeat (resolveBattle in game.ts). The losses,
// retreats, prisoners and morale the rules apply all follow from R. A played battle hands back an R of its own:
//   - who won decides the side of 1.2 it falls on;
//   - how lopsided the exchange was decides how far from 1.2: an even trade is a close result, and the more the
//     loser bled for each share the winner lost, the further out it goes.
// The constants are plain and in one place so they can be tuned (TODO(polish): tune against played battles).

import type { VictoryLevel } from '../../core/ticket';
import type { Side } from './types';

export const RATIO = {
  /** The campaign's line between taking the ground and being thrown back. */
  carry: 1.2,
  /** The furthest a played battle can go either way. */
  most: 4,
  least: 0.3,
  /** Added to both sides' losses before they are compared, so a fight with almost no losses reads as even. */
  floor: 0.05,
  /** How hard the exchange pushes R from the line: R = carry * exchange ^ power. */
  power: 0.5,
};

/**
 * R from a played battle. 'lost' is each side's share of its starting power lost (0..1), 'winner' who held the field.
 * The attacker's win is never below the line and his defeat never on or above it.
 */
export function fieldRatio(winner: Side, lost: Record<Side, number>): number {
  const ex = (lost.def + RATIO.floor) / (lost.att + RATIO.floor);
  const r = RATIO.carry * Math.pow(ex, RATIO.power);
  return winner === 'att'
    ? Math.min(RATIO.most, Math.max(RATIO.carry, r))
    : Math.max(RATIO.least, Math.min(RATIO.carry - 0.01, r));
}

/** The campaign's level for an R, read as the attacker reads it (the same bands as resolveBattle). */
export function levelOf(R: number, attLostMore: boolean): VictoryLevel {
  return R >= 3 ? 'Heroic victory' : R >= 2 ? 'Decisive victory' : R >= RATIO.carry ? (attLostMore ? 'Pyrrhic victory' : 'Close victory') : R >= 0.8 ? 'Close defeat' : 'Decisive defeat';
}

/** The same level as the defender reads it. */
export function levelFor(side: Side, R: number, attLostMore: boolean): VictoryLevel {
  const a = levelOf(R, attLostMore);
  if (side === 'att') return a;
  const flip: Record<VictoryLevel, VictoryLevel> = {
    'Heroic victory': 'Crushing defeat', 'Decisive victory': 'Decisive defeat', 'Close victory': 'Close defeat', 'Pyrrhic victory': 'Close defeat',
    'Close defeat': 'Close victory', 'Decisive defeat': 'Decisive victory', 'Crushing defeat': 'Heroic victory',
  };
  return flip[a];
}

/**
 * Ruling 217 (2), with (a) of 17:55Z: a fought battle never writes a result bigger than the dice could. The field's R
 * is held inside the run the dice could have rolled at the card's odds, except that where the field's verdict lies
 * outside that run, R may go as far as the line itself and no further (a field win books a bare close victory; a
 * field defeat a bare close defeat), so the report never says the opposite of the field he watched. 'dice' is the
 * strict reading, kept behind the switch.
 */
export const CLAMP: { mode: 'dice' | 'across' } = { mode: 'across' };
export function clampToDice(R: number, odds: number, dice: [number, number], winner: Side): number {
  const lo = odds * dice[0], hi = odds * dice[1];
  let r = Math.min(hi, Math.max(lo, R));
  if (CLAMP.mode === 'across') {
    if (winner === 'att' && r < RATIO.carry) r = RATIO.carry;
    if (winner === 'def' && r >= RATIO.carry) r = RATIO.carry - 0.01;
  }
  return r;
}

/**
 * Ruling 217 (4): the dice are always there. Handing the rest of a battle back to the dice rolls them at the ratio
 * then standing (each side's standing power, against the card's odds), and the result is held to the dice's run.
 * 'roll' is a number in 0..1 from the battle's own stream.
 */
export function diceNow(odds: number, dice: [number, number], standing: Record<Side, number>, roll: number): number {
  const now = odds * (Math.max(1e-6, standing.att) / Math.max(1e-6, standing.def));
  const R = now * (dice[0] + (dice[1] - dice[0]) * roll);
  return Math.min(odds * dice[1], Math.max(odds * dice[0], R));
}
