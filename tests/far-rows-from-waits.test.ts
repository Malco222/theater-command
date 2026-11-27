// The far side, under the switch 'fromWait': a row whose named corps is made by another dated row that is waiting.
// The design director's ruling of 19:47Z, 25 November 2026 ("one sentence, both doors: a date never lifts a corps out
// of a fight"): a row that takes a named corps whole waits while that corps is in contact, and has its half year from
// the month the corps comes free. Japan's 25th Army (18th Division) row (month 27, Hainan) takes the 21st Army's 18th
// Division; the 18th Division's row (month 32, Rangoon) takes the 25th Army (18th Division). With the 21st Army's 18th
// Division in the line at Tungkun the first row waits, so the 25th Army is not on the board in month 32. As built the
// Rangoon row then found nobody to take and came whole from nobody, and when the 21st Army's 18th Division came free
// the Hainan row made a second 18th Division beside it (the skeptics' check of fold-w, run zz-probe-eighteenth).
// Now the Rangoon row waits with the row that makes its corps (game.ts `fromWaits`), and comes once that corps does.
// Switch off, the Hainan row takes its corps wherever it stands, as before, and the Rangoon row takes the 25th Army.
// The states are set by hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { corpsInContact } from '../src/campaign/drawn';
import { createGame, declareWar, relocate, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import type { Game, Region } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';

const ARMY21 = '21st Army (18th Division)', ARMY25 = '25th Army (18th Division)', D18 = '18th Division';
const rows = (setup as any).expeditions as any[][];
const index = (name: string): number => rows.findIndex((r) => r[1] === 'JAP' && r[2] === name);
const lose = (g: Game, name: string, to: string, away: string): Region => {
  const r = g.byName[name];
  for (const u of [...r.units]) relocate(g, u, g.byName[away]);
  r.ctrl = to;
  return r;
};
/** Japan's corps that carry the 18th Division whole (not a detachment of it), by name and place. */
const eighteenth = (g: Game): string[] => g.corps.filter((c) => c.nat === 'JAP' && !c.dead && [ARMY21, ARMY25, D18].includes(c.name)).map((c) => `${c.name}@${g.regions[c.reg].name}`).sort();
/** Seed 3, the far side on, Japan's strike south gone in month 28, Rangoon Japan's, Singapore hers in month 35: the skeptics' case. */
const war = (on: boolean, stop = 3): Game => {
  const g = startOpen(createGame(3, null));
  if (stop !== 3) setStop(g, stop, true);
  if (on) g.flags.fromWait = 1;
  lose(g, 'Cam Ranh', 'JAP', 'Saigon');
  g.turn = 27; startPhase(g, 'JAP');
  g.turn = 28; g.flags['st:done:JAP:south'] = 28;
  for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
  lose(g, 'Rangoon', 'JAP', 'Calcutta');
  return g;
};
/** Month by month, Japan's alone; Singapore falls to her in month 35, so the Malaya front lets its corps go. */
const months = (g: Game, from: number, to: number): void => { for (let t = from; t <= to; t++) { if (t === 35) lose(g, 'Singapore', 'JAP', 'Calcutta'); g.turn = t; startPhase(g, 'JAP'); } };

describe('the data: the two rows', () => {
  it("the Hainan row takes the 21st Army's 18th Division, the Rangoon row takes the 25th Army's, with no body of its own", () => {
    expect(rows[index(ARMY25)].slice(0, 6)).toEqual([27, 'JAP', ARMY25, 'inf', 1, 'Hainan']);
    expect(rows[index(ARMY25)][6]).toMatchObject({ from: [ARMY21] });
    expect(rows[index(D18)].slice(0, 6)).toEqual([32, 'JAP', D18, 'inf', 1, 'Rangoon']);
    expect(rows[index(D18)][6]).toMatchObject({ from: [ARMY25] });
    expect(rows[index(D18)][6].adds ?? 0).toBe(0);
  });
});

describe("switch on: the Rangoon row waits with the row that makes its corps", () => {
  it('waits from month 32 while the Hainan row waits on contact, and the 18th Division stands once when both have come', () => {
    const g = war(true);
    months(g, 29, 34);
    const c = g.corps.find((x) => x.nat === 'JAP' && !x.dead && x.name === ARMY21)!;
    expect(corpsInContact(g, 'JAP', c)).toBe(true);
    expect(g.flags[`sent:${index(ARMY25)}`]).toBeUndefined();
    expect(g.flags[`wait:${index(ARMY25)}`]).toBe(34);
    // Not formed from nobody: it waits, and its half year runs from the month its corps comes.
    expect(g.flags[`sent:${index(D18)}`]).toBeUndefined();
    expect(g.flags[`wait:${index(D18)}`]).toBe(34);
    expect(eighteenth(g)).toEqual([`${ARMY21}@Tungkun`]);
    months(g, 35, 41);
    expect(g.flags[`sent:${index(D18)}`]).toBeUndefined();
    expect(eighteenth(g)).toEqual([`${ARMY21}@Tungkun`]);
    // The 21st Army's 18th Division stands clear: the Hainan row comes. Its corps stands in contact at Hainan, so the
    // Rangoon row waits on it as on any corps in contact; one 18th Division stands.
    relocate(g, c, g.byName['Hiroshima']);
    months(g, 42, 44);
    expect(g.flags[`sent:${index(ARMY25)}`]).toBe(1);
    const a = g.corps.find((x) => x.nat === 'JAP' && !x.dead && x.name === ARMY25)!;
    expect(corpsInContact(g, 'JAP', a)).toBe(true);
    expect(g.flags[`sent:${index(D18)}`]).toBeUndefined();
    expect(g.flags[`wait:${index(D18)}`]).toBe(44);
    expect(eighteenth(g)).toEqual([`${ARMY25}@Hainan`]);
    // It stands clear in its turn: the Rangoon row takes it whole, and still one 18th Division stands.
    relocate(g, a, g.byName['Hiroshima']);
    months(g, 45, 46);
    expect(g.flags[`sent:${index(D18)}`]).toBe(1);
    expect(eighteenth(g)).toEqual([`${D18}@Rangoon`]);
  });
});

describe('switch off: as before', () => {
  it('the Hainan row takes its corps out of the line in month 29, and the Rangoon row takes the 25th Army when it is free', () => {
    const g = war(false);
    months(g, 29, 29);
    expect(g.flags[`sent:${index(ARMY25)}`]).toBe(1);
    expect(eighteenth(g)).toEqual([`${ARMY25}@Hainan`]);
    months(g, 30, 41);
    expect(eighteenth(g).length).toBe(1);
  });
});

describe("switch on at the book's stops, 1 and 2: the book's rows keep their book (the design director's 21:25Z)", () => {
  it.each([1, 2])('at stop %i neither row waits on contact: the Hainan row takes its corps out of the line in month 29, and one 18th Division stands', (stop) => {
    const g = war(true, stop);
    months(g, 29, 29);
    expect(g.flags[`sent:${index(ARMY25)}`]).toBe(1);
    expect(eighteenth(g)).toEqual([`${ARMY25}@Hainan`]);
    months(g, 30, 41);
    expect(eighteenth(g).length).toBe(1);
  });
});
