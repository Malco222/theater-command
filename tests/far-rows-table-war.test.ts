// The offers page's list of rows that wait for their war reads the war the rule waits for (release 2, the grand
// campaign, the far side and the offers on). The map line's stood war rows (1e2b5229, drawn.ts countingFor and
// data/campaign/counting.json) give two of Italy's rows the table's war: the Ciamuria Corps and Julia Alpine Division
// and the XXVI Corps keep ["ITA","UK"] in the setup row and wait, in expeditions(), for ["ITA","GRE"]. As merged,
// rowsAwaitingWar read the setup row's war, so the page said they formed "once you are at war with the United
// Kingdom" and dropped them the month he went to war with Britain, though the rule still held them for Greece (the
// skeptic's review of the merge, 25 November 2026). Here they wait for Greece at peace and at war with Britain alone,
// and leave the list the month the war with Greece lets them come.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { createGame, rowsAwaitingWar, setWar, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';
import { asideLines } from '../unreal/TheaterCommand/Rules/offers-look';

const E = (setup as any).expeditions as any[];
const at = (name: string): number => E.findIndex((r) => r[1] === 'ITA' && r[2] === name);
const CIAMURIA = at('Ciamuria Corps and Julia Alpine Division'), XXVI = at('XXVI Corps');
const months = (g: Game, from: number, to: number): void => { for (let t = from; t <= to; t++) { g.turn = t; startPhase(g, 'ITA'); } };
const theTwo = (g: Game) => rowsAwaitingWar(g).filter((w) => w.id === CIAMURIA || w.id === XXVI).map((w) => [w.name, w.with]);
const lines = (g: Game): string[] => asideLines(g).waiting.filter((l) => /Ciamuria|XXVI/.test(l));
const italy = (wars: [string, string][]): Game => {
  const g = startOpen(createGame(5, 'ITA'));
  g.flags.offers = 1;
  for (const [a, b] of wars) setWar(g, a, b);
  return g;
};
const WAITS = [['Ciamuria Corps and Julia Alpine Division', 'GRE'], ['XXVI Corps', 'GRE']];

describe("a player's Italy: the rows the table keys to the war with Greece wait for Greece on the offers page", () => {
  it('the setup rows still say Britain: the table is what moves them', () => {
    expect(CIAMURIA).toBeGreaterThan(-1);
    expect(XXVI).toBeGreaterThan(-1);
    expect([E[CIAMURIA][6].war, E[XXVI][6].war]).toEqual([['ITA', 'UK'], ['ITA', 'UK']]);
  });

  it('at peace, month 16: both wait for Greece, and the page says so', () => {
    const g = italy([]);
    months(g, 1, 16);
    expect([g.flags[`sent:${CIAMURIA}`], g.flags[`sent:${XXVI}`]]).toEqual([undefined, undefined]);
    expect(theTwo(g)).toEqual(WAITS);
    expect(lines(g)).toHaveLength(2);
    for (const l of lines(g)) expect(l).toMatch(/at war with Greece\.$/);
  });

  it('at war with Britain alone, month 16: both are still unsent and still listed as waiting for Greece', () => {
    const g = italy([['ITA', 'UK']]);
    months(g, 1, 16);
    expect([g.flags[`sent:${CIAMURIA}`], g.flags[`sent:${XXVI}`]]).toEqual([undefined, undefined]);
    expect(theTwo(g)).toEqual(WAITS);
    expect(lines(g)).toHaveLength(2);
  });

  it('at war with Britain and Greece: both come, and the list has neither', () => {
    const g = italy([['ITA', 'UK'], ['ITA', 'GRE']]);
    months(g, 1, 16);
    expect([g.flags[`sent:${CIAMURIA}`], g.flags[`sent:${XXVI}`]]).toEqual([1, 1]);
    expect(theTwo(g)).toEqual([]);
    expect(lines(g)).toEqual([]);
  });
});
