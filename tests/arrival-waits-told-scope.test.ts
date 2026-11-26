// The telling's scope, the far side (arrivalwaits.ts; the UI director's words of 21:49Z, 25 November 2026): only the
// player's own rows, only with the switch 'fromWait' on, only at the dial's stops 3 to 5, and not where his rows are
// offers. Outside it nothing prints and none of the telling's flags (rowTold, rowAt, rowFront) is ever written. And a
// wait told before a save is not told again after it. The board is tests/far-rows-from-waits's: seed 3, the 21st
// Army's 18th Division in the line at Tungkun, so the 25th Army (18th Division)'s row for Hainan waits from month 27.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { arrivalWaitOf, rowsWaitingOnRows } from '../src/campaign/arrivalwaits';
import { createGame, declareWar, relocate, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';

const ARMY21 = '21st Army (18th Division)', ARMY25 = '25th Army (18th Division)';
const rows = (setup as any).expeditions as any[][];
const index = (name: string): number => rows.findIndex((r) => r[1] === 'JAP' && r[2] === name);
const corps = (g: Game, name: string): Corps => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead)!;
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'JAP'); };
const lose = (g: Game, name: string, to: string, away: string): Region => { const r = g.byName[name]; for (const u of [...r.units]) relocate(g, u, g.byName[away]); r.ctrl = to; return r; };
/** Every line of the telling, in any month. */
const tellings = (g: Game): string[] => g.headlines.filter((h) => /formed from|left the front|has not formed yet|without your|The arrival/.test(h.text)).map((h) => h.text);
const telFlags = (g: Game): string[] => Object.keys(g.flags).filter((k) => /^row(Told|At|Front):/.test(k));
/** Months 27 to 34 of seed 3, Japan's alone, as tests/far-rows-from-waits plays them. */
const war = (seat: string | null, on: boolean, stop = 3, offers = false): Game => {
  const g = startOpen(createGame(3, seat));
  if (stop !== 3) setStop(g, stop, true);
  if (on) g.flags.fromWait = 1;
  if (offers) g.flags.offers = 1;
  lose(g, 'Cam Ranh', 'JAP', 'Saigon');
  month(g, 27);
  g.turn = 28; g.flags['st:done:JAP:south'] = 28;
  for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
  lose(g, 'Rangoon', 'JAP', 'Calcutta');
  for (let t = 29; t <= 34; t++) month(g, t);
  return g;
};

describe('outside the scope nothing is told and no flag is written', () => {
  it('the switch off: the row takes its corps as before, nothing is told, no flag of the telling', () => {
    const g = war('JAP', false);
    expect(g.flags[`sent:${index(ARMY25)}`]).toBe(1);
    expect(tellings(g)).toEqual([]);
    expect(telFlags(g)).toEqual([]);
    expect(rowsWaitingOnRows(g, 'JAP')).toEqual([]);
    expect(g.corps.filter((c) => c.nat === 'JAP' && !c.dead).map((c) => arrivalWaitOf(g, c)).filter(Boolean)).toEqual([]);
  });

  it.each([1, 2])('at stop %i, the switch on: nothing told, no flag', (stop) => {
    const g = war('JAP', true, stop);
    expect(g.flags[`wait:${index(ARMY25)}`]).toBeUndefined();
    expect(tellings(g)).toEqual([]);
    expect(telFlags(g)).toEqual([]);
  });

  it("a computer's row: Japan nobody's, the switch on, the row waits as the rule says and nothing is told", () => {
    const g = war(null, true);
    expect(g.flags[`wait:${index(ARMY25)}`]).toBe(34);
    expect(tellings(g)).toEqual([]);
    expect(telFlags(g)).toEqual([]);
  });

  it("another country's row while Japan is the player's: none of the telling's flags is written for it", () => {
    const g = war('JAP', true);
    const mine = new Set(rows.map((r, i) => (r[1] === 'JAP' ? i : -1)).filter((i) => i >= 0));
    expect(telFlags(g).filter((k) => !mine.has(Number(k.split(':')[1])))).toEqual([]);
  });

  it('where his rows are offers: he is asked, nothing waits, nothing is told', () => {
    const g = war('JAP', true, 3, true);
    expect(g.flags[`wait:${index(ARMY25)}`]).toBeUndefined();
    expect(tellings(g)).toEqual([]);
    expect(telFlags(g)).toEqual([]);
  });
});

describe('a save in the middle of a wait', () => {
  it('rides in the save: the wait is not told again, the card reads on, and the formed headline comes', () => {
    const g = war('JAP', true);
    const i = index(ARMY25);
    const back = loadGame(saveGame(g))!;
    expect([back.flags[`rowTold:${i}`], back.flags[`rowFront:${i}`], back.flags[`rowAt:${i}`]]).toEqual([27, 1, back.byName['Hainan'].id]);
    expect(arrivalWaitOf(back, corps(back, ARMY21))?.whole).toBe('An arrival due in Hainan, the 25th Army (18th Division), is to be formed from this corps, and has waited for it since November 1941. It forms the month after this corps leaves the front.');
    month(back, 35);
    expect(back.headlines.filter((h) => h.text.startsWith('The 25th Army (18th Division) is due in'))).toHaveLength(1);
    relocate(back, corps(back, ARMY21), back.byName['Hiroshima']);
    month(back, 36);
    expect(back.headlines.filter((h) => h.turn === 36).map((h) => h.text)).toContain('The 25th Army (18th Division) has landed at Hainan, formed from your 21st Army (18th Division), now that it has left the front.');
  });
});
