// The contact rule, round 3, on the far side (TC_FAR for the whole file): the player's own loans whose end is his to
// answer (ruling B8, game.ts `loanStays`: a regiment a division's row awaits). The design director's "3 RULED IN: a
// loan's end waits while the test holds the corps", 01:37Z, 27 November 2026; the brief's R3-2: "The player's own loans
// (the offer made in the month the loan is over: flags `leave:N`, `kept:N`) keep their offer and their choice". As built
// here (the builder's lean, for her word): while the one test holds the regiment the offer is not made and the hold is
// told instead; the first month the test lets it go, the offer is made as it always was, and his answer decides.
// Germany seated, offers on; its test regiment alone on the Grenzmark, Poland's Army Poznan beside it in Poznan.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { setFromWait } from '../src/campaign/arrivalwaits';
import { addCorps, atWar, createGame, relocate, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { farOn } from '../src/campaign/world';
import type { Corps, Game } from '../src/campaign/types';

const ROWS = (setup as any).expeditions as any[];
let added = 0;
const addRow = (row: any[]): number => { ROWS.push(row); added++; return ROWS.length - 1; };
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } });
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'GER'); };
const HOME = 'The Lent Test Regiment, lent to the campaign, has come to the end of its loan this month, but it is at the front in the Grenzmark. It stays, and you are asked whether to let it go home to its division the month after it leaves the front.';

/** Germany seated with offers on; a regiment lent until month 2, awaited by a division's row of month 9. */
function board(o: { on?: boolean; relief?: boolean; offers?: boolean } = {}): { g: Game; j: number; reg: Corps } {
  const g = startOpen(createGame(3, null));
  expect(farOn(g)).toBe(true);
  g.human = 'GER';
  if (o.offers ?? true) g.flags.offers = 1;
  if (o.on ?? true) expect(setFromWait(g, true)).toBe(true);
  const gz = g.byName.Grenzmark;
  for (const u of [...gz.units]) relocate(g, u, g.byName.Bavaria);
  expect(atWar(g, 'GER', 'POL')).toBe(true);
  const reg = addCorps(g, 'GER', 'Lent Test Regiment', 'arm', 1, gz.id, {});
  const j = addRow([1, 'GER', 'Lent Test Regiment', 'arm', 1, 'Grenzmark', { until: 2 }]);
  addRow([9, 'GER', 'Test Tank Division', 'arm', 1, 'Magdeburg', { awaits: ['Lent Test Regiment'], str: 1 }]);
  g.flags[`sent:${j}`] = 1;
  g.flags[`lent:${j}`] = reg.id;
  if (o.relief) addCorps(g, 'GER', 'Relief Test Corps', 'inf', 2, gz.id, {});
  return { g, j, reg };
}

describe("R3-2 on the far side: the player's own loan whose end is an offer", () => {
  it('held by the one test: the offer is not made, the regiment stays, the wait is written and the hold told once', () => {
    const { g, j, reg } = board();
    month(g, 2);
    expect(g.flags[`leave:${j}`]).toBeUndefined();
    expect(g.flags[`wait:${j}`]).toBe(2);
    expect([reg.dead, g.regions[reg.reg].name]).toEqual([false, 'Grenzmark']);
    expect(g.headlines.filter((h: any) => /Lent Test Regiment/.test(h.text)).map((h: any) => h.text)).toEqual([HOME]);
    month(g, 3);
    expect(g.flags[`leave:${j}`]).toBeUndefined();
    expect(g.headlines.filter((h: any) => h.text === HOME)).toHaveLength(1);
  });

  it('the first month the test lets it go, the offer is made as it always was, the regiment stays for his answer, and the telling flags go', () => {
    const { g, j, reg } = board();
    month(g, 2);
    expect(g.flags[`rowTold:${j}`]).toBe(2);
    addCorps(g, 'GER', 'Relief Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
    month(g, 3);
    expect(g.flags[`leave:${j}`]).toBe(3);
    expect(reg.dead).toBe(false);
    expect(g.flags[`lent:${j}`]).toBe(reg.id);
    expect(g.flags[`rowTold:${j}`]).toBeUndefined();
    expect(g.flags[`rowAt:${j}`]).toBeUndefined();
  });

  it('relieved from the first: the offer is made in the month the loan is over, as before, and nothing is told', () => {
    const { g, j } = board({ relief: true });
    month(g, 2);
    expect(g.flags[`leave:${j}`]).toBe(2);
    expect(g.flags[`wait:${j}`]).toBeUndefined();
    expect(g.headlines.some((h: any) => h.text === HOME)).toBe(false);
  });

  it('switch off: the offer is made in the month the loan is over, alone beside the enemy or not', () => {
    const { g, j } = board({ on: false });
    month(g, 2);
    expect(g.flags[`leave:${j}`]).toBe(2);
    expect(g.flags[`wait:${j}`]).toBeUndefined();
    expect(g.flags[`rowTold:${j}`]).toBeUndefined();
  });
});

// (Finding S1-9 (b) of the rule sceptic 3: with offers off, the base sends the player's awaited regiment home to its
// division at its loan's end (`comeHome`), so the hold must not tell him it is due to leave for other fronts.)
describe("R3-2 on the far side, offers off: the player's awaited regiment held by the one test is told it goes home", () => {
  const REJOIN = 'The Lent Test Regiment, lent to the campaign, has come to the end of its loan this month, but it is at the front in the Grenzmark. It stays, and goes home to its division the month after it leaves the front.';
  it('held: it stays, the wait is written, and the hold is told in the home form, with no offer in it', () => {
    const { g, j, reg } = board({ offers: false });
    month(g, 2);
    expect(g.flags[`leave:${j}`]).toBeUndefined();
    expect(g.flags[`wait:${j}`]).toBe(2);
    expect(reg.dead).toBe(false);
    expect(g.headlines.filter((h: any) => /Lent Test Regiment/.test(h.text)).map((h: any) => h.text)).toEqual([REJOIN]);
  });

  it('switch off: it goes home to its division in the month the loan is over, as before (the base)', () => {
    const { g, j, reg } = board({ offers: false, on: false });
    month(g, 2);
    expect(reg.dead).toBe(true);
    expect(g.flags[`lent:${j}`]).toBe(-1);
    expect(g.flags[`home:${j}`]).toBeGreaterThan(0);
  });
});
