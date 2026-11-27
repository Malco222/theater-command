// Worn out in the line (release 2).
//
// Brian, 27 November 2026 17:10Z, yes to the idea: "a corps that fights month after month slowly loses its edge, and a
// month in reserve brings it back". Under this switch:
//
// 1. In the line is the front the uniforms already wear on (stories.ts inLine): the corps fought in the month gone, or
//    an army at war with it holds the ground next door or stands in its own province.
// 2. Each month in the line takes 'step' off its fighting power, to 'most' (no count of months comes first: the DD,
//    17:15Z). A month neither at rest nor in the line (on the march behind the front) keeps what it has lost.
// 3. One whole month at rest (morale.ts restedMonth: in supply, no march, no fight) brings it all back.
// 4. The loss is read inside the battle's own assessment (game.ts assess, beside morale's heart), so the battle, the
//    attack card and every staff's estimate read it through the same function. The card says it.
// 5. Every army's corps alike, the player's and the computers'. A fort, which can never rest, does not wear. Where a
//    computer staff rests its worn corps (ai.ts relieve), one worn to 'most' counts as worn, read through edge().
//
// With the switch off nothing here is written, and edge() is exactly 1.

import data from '../../data/campaign/wornout.json';
import { restedMonth } from './morale';
import { inLine } from './stories';
import type { Corps, Game } from './types';

export const WORN = data as unknown as { step: number; most: number };

/** The switch: a grand campaign's own, off unless the release 2 ship list or the player turns it on. */
export const wornOn = (g: Game): boolean => g.flags.wornout === 1;

/** Turns the switch on or off at any time. Off gives every corps its edge back. */
export function setWorn(g: Game, on: boolean): boolean {
  if (on) {
    if (g.flags.open !== 1) return false;
    g.flags.wornout = 1;
    return true;
  }
  delete g.flags.wornout;
  for (const c of g.corps) { delete c.spent; delete c.runIn; }
  for (const v of g.atSea ?? []) { delete v.corps.spent; delete v.corps.runIn; }
  return true;
}

/** What a corps' months in the line leave of its fighting power: 1 fresh, down to 1 - 'most'. Exactly 1 with the switch off. */
export const edge = (c: Corps): number => (c.spent ? 1 - c.spent : 1);

/** True for a corps worn to the worst its months in the line can do, read through edge() (the staffs' rest, ai.ts). */
export const blunted = (c: Corps): boolean => edge(c) <= 1 - WORN.most + 1e-9;

/**
 * The reckoning that opens a corps' month (game.ts startPhase, before its moves are handed back): a month at rest
 * clears what it had lost; a month in the line takes 'step' more, to 'most'. 'runIn' counts the months in the line in
 * a row, for the card's words only.
 */
export function wornMonth(g: Game, c: Corps): void {
  if (!wornOn(g) || c.kind === 'fort') return;
  if (restedMonth(g, c)) { delete c.spent; delete c.runIn; return; }
  if (!(c.fought || inLine(g, c))) { delete c.runIn; return; }
  c.runIn = (c.runIn ?? 0) + 1;
  c.spent = Math.round(Math.min(WORN.most, (c.spent ?? 0) + WORN.step) * 1000) / 1000;
}

/** The corps card's line for a corps that has lost its edge: '' where it has lost none. */
export function wornLine(c: Corps): string {
  if (!c.spent) return '';
  const pct = Math.round(c.spent * 100), months = c.runIn ?? 0;
  return `Worn out: ${pct}% weaker${months ? ` after ${months} ${months === 1 ? 'month' : 'months'} in the line` : ''}. A month at rest brings it back.`;
}
