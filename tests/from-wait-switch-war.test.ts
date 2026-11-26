// The switch turned off in the middle of a war (arrivalwaits.ts `setFromWait`). The design director's answer 2 of
// 00:43Z, 26 November 2026: "Keep the rule's own wait marker. Off acts from the month it is turned off, and what
// already happened stays in the save, as everywhere. Two tests: a war never switched on is the whole save of off; a
// war switched off mid-war plays by off from that month with its past kept." The first is in
// tests/from-wait-switch-r2.test.ts; this is the second.
//
// A short war (the far side, seed 18, Japan seated with every country on the computer, as the gate's whole-save runs
// play it), the switch on from the start: Japan's 13th Army (Chekiang landing force) waits from month 19 while its
// corps is at the front, and is told. In month 21 the switch is turned off by the setter. From that month the war is,
// month by month, the war continued from the same saved game with the flag simply absent (the whole save, the
// telling's own flags and the headlines apart): the rule's `wait:<i>` markers are kept, and no row is struck for the
// months it waited.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { setFromWait } from '../src/campaign/arrivalwaits';
import { createGame } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const rows = (setup as any).expeditions as any[][];
const OFF = 21, LAST = 24;
/** The whole saved game, the telling's own flags and the headlines left out (the gate's print with TC_NOHEAD and those prefixes). */
const whole = (g: Game): string => {
  const heads = g.headlines, kept: Record<string, number> = {};
  for (const k of Object.keys(g.flags)) if (/^row(Told|At|Front):/.test(k)) { kept[k] = g.flags[k]; delete g.flags[k]; }
  g.headlines = [];
  const text = saveGame(g);
  g.headlines = heads;
  Object.assign(g.flags, kept);
  return text;
};
const waits = (g: Game): Record<string, number> => Object.fromEntries(Object.keys(g.flags).filter((k) => k.startsWith('wait:')).sort().map((k) => [k, g.flags[k]]));

describe('a war switched off mid-war plays by off from that month, with its past kept', () => {
  it('the far side, seed 18, Japan seated: off by the setter in month 21 is, month by month, the war with the flag simply absent', () => {
    const g = startOpen(createGame(18, 'JAP'));
    expect(setFromWait(g, true)).toBe(true);
    while (g.turn < OFF) runTurnAI(g);
    // A row of the player's has waited on the front, and was told.
    const i = rows.findIndex((r) => r[1] === 'JAP' && r[2] === '13th Army (Chekiang landing force)');
    expect(g.flags[`wait:${i}`]).toBe(OFF - 1);
    expect(g.flags[`rowTold:${i}`]).toBe(19);
    expect(g.flags[`sent:${i}`]).toBeUndefined();
    const saved = saveGame(g);

    const off = loadGame(saved)!;
    expect(setFromWait(off, false)).toBe(true);
    const absent = loadGame(saved)!;
    delete absent.flags.fromWait;
    // (What already happened stays: the rule's own markers are the save's, kept as they were.)
    const before = waits(absent);
    expect(Object.keys(before).length).toBeGreaterThan(0);
    expect(waits(off)).toEqual(before);
    expect(whole(off)).toBe(whole(absent));

    for (let t = OFF; t < LAST; t++) {
      runTurnAI(off);
      runTurnAI(absent);
      expect(off.turn).toBe(t + 1);
      expect(whole(off)).toBe(whole(absent));
    }
    // The row formed the month it was turned off, its corps taken where it stood: not struck for the months it waited.
    expect(off.flags[`sent:${i}`]).toBe(1);
    expect(off.flags[`wait:${i}`]).toBe(OFF - 1);
    expect(off.corps.some((c) => c.nat === 'JAP' && !c.dead && c.name === rows[i][2])).toBe(true);
    // No row that had waited when the switch went off was struck since.
    for (const k of Object.keys(before)) expect(off.flags[`sent:${k.slice(5)}`]).not.toBe(2);
    expect(off.flags.fromWait).toBeUndefined();
  }, 1800000);
});
