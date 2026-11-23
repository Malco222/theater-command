// Units decide (release 2; Brian 23 Nov 2026 07:11Z and 07:13Z, the core theme: "It's about the units and you build
// and attack with not historical handcuffs that determine battles"; the design director's audit of 07:17Z,
// design-director/units-decide-audit-1123.md). One switch, built in steps on it: each step turns a constant keyed to a
// nation or a date into a cause any nation can build, research, train or lose. The record only sets how strong each
// cause is, and the history start's levels.
//
// Set at a grand campaign's start only (g.flags.unitsDecide), never in one under way, and never in a history
// campaign. Off, nothing behind it is read and every war is the war without it.
//
// Steps on it so far: the convoy war by cause (audit #5, navy.ts 'The convoy war by cause').

import type { Game } from './types';

/** True where this campaign was opened with Units decide: a grand campaign whose start set the flag. */
export const unitsDecide = (g: Game): boolean => g.flags.open === 1 && g.flags.unitsDecide === 1;

/** Sets the switch. Only at setup, before the first month has been played; returns whether it was set. */
export function setUnitsDecide(g: Game, on: boolean): boolean {
  if (g.turn !== 1 || g.phase !== 0 || g.airReports.length || g.reports.length) return false;
  if (on) g.flags.unitsDecide = 1;
  else delete g.flags.unitsDecide;
  return true;
}
