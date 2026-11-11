// The framework's Phase 1 exit test for the campaign layer: with every nation on the
// historical plan and nobody at the controls, France should fall in about six weeks.
import { describe, expect, it } from 'vitest';
import { createGame } from '../../src/campaign/game';
import { runTurnAI } from '../../src/campaign/turn';
import type { Game } from '../../src/campaign/types';

function history(seed: number, turns = 12): Game {
  const g = createGame(seed, null);
  for (let t = 1; t <= turns; t++) runTurnAI(g);
  return g;
}

/** Weeks from the first day of the offensive (turn `start`) to the day a nation asked for terms. */
function weeks(g: Game, nat: string, start: number): number {
  const f = g.nations[nat].fellOn;
  return f ? (f.turn - start) * 4 + f.week : Infinity;
}

const median = (a: number[]): number => [...a].sort((x, y) => x - y)[Math.floor(a.length / 2)];
const SEEDS = Array.from({ length: 20 }, (_, i) => i + 1);
const runs = SEEDS.map((s) => history(s));

describe('Phase 1 exit test: 1939-40 on the historical plan', () => {
  it('France falls in about six weeks', () => {
    const w = runs.map((g) => weeks(g, 'FRA', 9));
    expect(Math.min(...w)).toBeGreaterThanOrEqual(4);
    expect(Math.max(...w)).toBeLessThanOrEqual(8);
    expect(median(w)).toBeGreaterThanOrEqual(5);
    expect(median(w)).toBeLessThanOrEqual(7);
  });

  it('Poland falls in about a month', () => {
    const w = runs.map((g) => weeks(g, 'POL', 1));
    expect(Math.min(...w)).toBeGreaterThanOrEqual(3);
    expect(Math.max(...w)).toBeLessThanOrEqual(6);
  });

  it('the Low Countries fall before France does', () => {
    for (const g of runs) {
      expect(weeks(g, 'NED', 9)).toBeLessThan(weeks(g, 'FRA', 9));
      expect(weeks(g, 'BEL', 9)).toBeLessThanOrEqual(weeks(g, 'FRA', 9));
    }
  });

  it('most of the British Expeditionary Force gets home', () => {
    for (const g of runs) {
      const home = g.corps.filter((c) => c.nat === 'UK' && c.name.includes('BEF') && g.regions[c.reg].owner === 'UK');
      expect(home.length).toBeGreaterThanOrEqual(2);
    }
  });

  it('the armistice leaves a French government in the south and Britain still fighting', () => {
    for (const g of runs) {
      expect(g.nations.VIC.alive).toBe(true);
      expect(g.byName['Paris'].ctrl).toBe('GER');
      expect(g.byName['Marseille'].ctrl).toBe('VIC');
      expect(g.nations.UK.alive).toBe(true);
    }
  });

  it('the same seed replays the same war', () => {
    const a = history(7), b = history(7);
    expect(a.reports.map((r) => `${r.turn}:${r.regionName}:${r.odds.toFixed(4)}`)).toEqual(b.reports.map((r) => `${r.turn}:${r.regionName}:${r.odds.toFixed(4)}`));
  });
});
