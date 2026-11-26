// The switch and the telling's flags, the far side (arrivalwaits.ts `setFromWait`, `waiting`). Off mid-wait: the corps'
// standing line and the month card's line for a row that waits on a row go at once, while the telling's flags stay in
// the save until the row is sent or struck (`rowDone`; the design director's answers 2 and 3 of 00:43Z, 26 November
// 2026). Off and on again in the same month: the month card's line comes back with the wait's first month, and nothing
// is told a second time. The board is tests/arrival-waits-told's: seed 3, Japan the player's, the 25th Army (18th
// Division)'s row for Hainan waits on the 21st Army at Tungkun, and the 18th Division's row for Rangoon waits on it.
// The refuter's switch-02; the old chart: tests/from-wait-switch-told.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { arrivalWaitOf, rowsWaitingOnRows, setFromWait } from '../src/campaign/arrivalwaits';
import { createGame, declareWar, relocate, startPhase } from '../src/campaign/game';
import { markFor, monthCard } from '../src/campaign/monthcard';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';

const WORDS = {
  standing: 'An arrival due in Hainan, the 25th Army (18th Division), is to be formed from this corps, and has waited for it since November 1941. It forms the month after this corps leaves the front.',
  card: 'The 18th Division, due in Rangoon, has waited since April 1942 for the 25th Army (18th Division), which has not formed yet.',
};
const ARMY21 = '21st Army (18th Division)', ARMY25 = '25th Army (18th Division)', D18 = '18th Division';
const rows = (setup as any).expeditions as any[][];
const index = (name: string): number => rows.findIndex((r) => r[1] === 'JAP' && r[2] === name);
const corps = (g: Game, name: string): Corps => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead)!;
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'JAP'); };
const lose = (g: Game, name: string, to: string, away: string): Region => { const r = g.byName[name]; for (const u of [...r.units]) relocate(g, u, g.byName[away]); r.ctrl = to; return r; };
const telling = (g: Game, t: number): string[] => g.headlines.filter((h) => h.turn === t && h.nat === 'JAP' && /this month|formed from|left the front|has not formed yet|without your/.test(h.text)).map((h) => h.text);
const tel = (g: Game, i: number): number[] => ['rowTold', 'rowAt', 'rowFront'].map((k) => g.flags[`${k}:${i}`]).filter((v) => v !== undefined);
const war = (): Game => {
  const g = startOpen(createGame(3, 'JAP'));
  g.flags.fromWait = 1;
  lose(g, 'Cam Ranh', 'JAP', 'Saigon');
  month(g, 27);
  g.turn = 28; g.flags['st:done:JAP:south'] = 28;
  for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
  lose(g, 'Rangoon', 'JAP', 'Calcutta');
  for (let t = 29; t <= 33; t++) month(g, t);
  return g;
};

describe('the far side: off mid-wait, and off and on again', () => {
  it('off: both cards go at once, the flags stay until the rows go', () => {
    const g = war();
    const i25 = index(ARMY25), i18 = rows.findIndex((r) => r[1] === 'JAP' && r[2] === D18 && r[5] === 'Rangoon');
    expect(arrivalWaitOf(g, corps(g, ARMY21))?.whole).toBe(WORDS.standing);
    expect(rowsWaitingOnRows(g, 'JAP')).toEqual([WORDS.card]);
    expect(monthCard(g, markFor(g), 'JAP').waits).toEqual([WORDS.card]);
    const kept25 = tel(g, i25), kept18 = tel(g, i18);
    expect(kept25).toHaveLength(3);
    expect(kept18).toHaveLength(2);
    expect(setFromWait(g, false)).toBe(true);
    expect(arrivalWaitOf(g, corps(g, ARMY21))).toBeNull();
    expect(rowsWaitingOnRows(g, 'JAP')).toEqual([]);
    expect(monthCard(g, markFor(g), 'JAP').waits).toBeUndefined();
    expect(tel(g, i25)).toEqual(kept25);
    expect(tel(g, i18)).toEqual(kept18);
    month(g, 34);
    // (The Hainan row is sent while the switch is off: today's line, its flags gone.)
    expect(g.flags[`sent:${i25}`]).toBe(1);
    expect(telling(g, 34)).toEqual([]);
    expect(tel(g, i25)).toEqual([]);
  });

  it('off and on again in the same month: the card comes back with its first month, and nothing is told twice', () => {
    const g = war();
    expect(setFromWait(g, false) && setFromWait(g, true)).toBe(true);
    expect(rowsWaitingOnRows(g, 'JAP')).toEqual([WORDS.card]);
    expect(arrivalWaitOf(g, corps(g, ARMY21))?.whole).toBe(WORDS.standing);
    month(g, 34);
    expect(telling(g, 34)).toEqual([]);
    expect(g.headlines.filter((h) => h.text.startsWith('The 25th Army (18th Division) is due in Hainan this month'))).toHaveLength(1);
    expect(g.headlines.filter((h) => h.text.startsWith('The 18th Division is due in Rangoon this month'))).toHaveLength(1);
  });
});
