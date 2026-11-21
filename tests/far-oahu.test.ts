// Oahu's two divisions at the record's weight (the design director's notes of 06:09Z and 06:12Z of 21 November 2026
// to ruling 234; the historian's far-chart-town-guards-globe.md, section 5). Data only: far-shores.json.
//
// As read (no fighting, months 1 to 29): the Hawaiian Division stood at 0.7 filling to its cap 0.8 at Pearl Harbor;
// the 25th Infantry Division's row of month 26 came at Honolulu at 0.8 with no cap, and stood at 1.0 by December
// 1941. Pearl Harbor and Honolulu are the one island's two provinces: 1.8 against the record's 1.6 (the 24th and the
// 25th at 0.75 to 0.85 each). The 25th is capped at the middle, 0.8. The Hawaiian Division's row stands for the 24th
// under its old name: a row cannot be made from another for America (a computer staff that ships its own army is sent
// no dated expedition, world.ts shipsItsOwn, and the formations list makes no row from another).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { createGame, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import { shipsItsOwn } from '../src/campaign/world';
import type { Corps, Game } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';
import world from '../data/campaign/world.json';

const HD = 'Hawaiian Division', D25 = '25th Infantry Division';
const ISLAND = ['Pearl Harbor', 'Honolulu'], OUTER = ['Hawaii', 'Maui', 'Kauai'];
const field = (g: Game, places: string[]): Corps[] => g.corps.filter((c) => c.nat === 'USA' && !c.dead && c.kind !== 'fort' && places.includes(g.regions[c.reg].name));
const sum = (g: Game, places: string[]): number => +field(g, places).reduce((s, c) => s + c.divs * c.str, 0).toFixed(3);
const months = (g: Game, from: number, to: number): void => { for (let t = from; t <= to; t++) { g.turn = t; startPhase(g, 'USA'); } };

describe("the data: Oahu's rows", () => {
  it('the Hawaiian Division from the first day at 0.7 and never past 0.8; the 25th in October 1941 at 0.8 and never past it', () => {
    expect(((world as any).corps as any[]).find((r) => r[1] === HD)).toEqual(['USA', HD, 'inf', 1, 'Pearl Harbor', { xp: 0.05, str: 0.7, est: 0.8 }]);
    expect(((setup as any).formations as any[]).find((r) => r[2] === D25)).toEqual([26, 'USA', D25, 'inf', 1, 'Honolulu', { xp: 0.05, str: 0.8, est: 0.8 }]);
    // (The record: each of the two divisions of December 1941 at 11,000 to 12,500 men, at 15,000 to a division.)
    expect((11000 + 12500) / 2 / 15000).toBeCloseTo(0.8, 1);
  });

  it('why no row is made from another here: a computer America is sent no dated expedition, and no dated expedition is America\'s on the far side', () => {
    const g = startOpen(createGame(5, null));
    expect(shipsItsOwn(g, 'USA')).toBe(true);
    const far = new Set(['Pearl Harbor', 'Honolulu', 'Hawaii', 'Maui', 'Kauai', 'Midway', 'Wake', 'Guam', 'Manila', 'Bataan', 'Lingayen', 'Samoa']);
    expect(((setup as any).expeditions as any[]).filter((r) => r[1] === 'USA' && far.has(r[5]))).toEqual([]);
  });
});

describe("the island's sum by month, with no fighting", () => {
  for (const stop of [1, 3, 5]) {
    it(`stop ${stop}: 0.7 on the first day, 0.8 before the split, and 1.6 from October 1941 through December`, () => {
      const g = startOpen(createGame(5, null));
      if (stop !== 3) setStop(g, stop, true);
      expect(sum(g, ISLAND)).toBe(0.7);
      // (The chart's own fill, in peace: slowly, to the cap.)
      months(g, 2, 25);
      expect(sum(g, ISLAND)).toBeGreaterThan(0.75);
      expect(sum(g, ISLAND)).toBeLessThanOrEqual(0.8);
      months(g, 26, 26);
      expect(field(g, ISLAND).map((c) => [c.name, g.regions[c.reg].name, c.full])).toEqual([[HD, 'Pearl Harbor', 0.8], [D25, 'Honolulu', 0.8]]);
      expect(field(g, ISLAND)[1].str).toBe(0.8);
      months(g, 27, 29);
      expect(sum(g, ISLAND)).toBeGreaterThan(1.58);
      expect(sum(g, ISLAND)).toBeLessThanOrEqual(1.6);
      // The outer islands hold nobody: the 299th's battalions are inside the island's 1.6.
      expect(field(g, OUTER)).toEqual([]);
    });
  }

  it("a player's America has the same two rows", () => {
    const g = startOpen(createGame(5, 'USA'));
    months(g, 2, 28);
    expect(field(g, ISLAND).map((c) => [c.name, c.full])).toEqual([[HD, 0.8], [D25, 0.8]]);
    expect(sum(g, ISLAND)).toBeGreaterThan(1.58);
    expect(sum(g, ISLAND)).toBeLessThanOrEqual(1.6);
  });
});
