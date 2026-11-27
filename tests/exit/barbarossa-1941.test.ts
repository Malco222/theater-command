// The framework's Phase 3 exit test, run early against the campaign rules: with every nation on
// the historical plan and nobody at the controls, the war in the east should look like 1941.
// A fast summer, a slowing autumn, Moscow and Leningrad still standing when the snow comes,
// and a Soviet counter-stroke in December that hurts the German army without breaking it.
//
// The plan only sets intentions. Every region still has to be won under the rules the player
// uses, so the bands below are loose on purpose and are checked across twenty sets of dice.
import { describe, expect, it } from 'vitest';
import { armySize, createGame } from '../../src/campaign/game';
import { runTurnAI } from '../../src/campaign/turn';
import type { Game } from '../../src/campaign/types';

const JUNE_41 = 22, JULY = 23, AUGUST = 24, OCTOBER = 26, NOVEMBER = 27, DECEMBER = 28, FEBRUARY = 30, MARCH = 31;

interface Run {
  g: Game;
  /** First month each city left Soviet hands. */
  fell: Record<string, number>;
  /** Soviet regions in Axis hands at the end of each month. */
  taken: number[];
  /** Places the Red Army won back in battle between December and February. */
  retaken: string[];
  gerLost: number;
  sovLost: number;
}

const CITIES = ['Minsk', 'Smolensk', 'Kiev', 'Kharkov', 'Leningrad', 'Moscow'];

function history(seed: number): Run {
  const g = createGame(seed, null);
  const run: Run = { g, fell: {}, taken: [], retaken: [], gerLost: 0, sovLost: 0 };
  let seen = 0;
  for (let t = 1; t <= MARCH; t++) {
    runTurnAI(g);
    for (const r of g.reports.slice(seen)) {
      if (t < JUNE_41) continue;
      for (const u of [...r.attackers, ...r.defenders]) {
        if (u.nat === 'GER') run.gerLost += u.lost;
        if (u.nat === 'SOV') run.sovLost += u.lost;
      }
      if (r.attNat === 'SOV' && r.taken && t >= DECEMBER && t <= FEBRUARY) run.retaken.push(r.regionName);
    }
    seen = g.reports.length;
    for (const c of CITIES) if (run.fell[c] === undefined && g.byName[c].ctrl !== 'SOV') run.fell[c] = t;
    run.taken[t] = g.regions.filter((r) => r.owner === 'SOV' && r.ctrl !== 'SOV').length;
  }
  return run;
}

const SEEDS = Array.from({ length: 20 }, (_, i) => i + 1);
const runs = SEEDS.map(history);
const count = (ok: (r: Run) => boolean): number => runs.filter(ok).length;

describe('Phase 3 exit test: a believable Barbarossa on the historical plan', () => {
  it('the war in the east begins in June 1941 and the Axis allies march with Germany', () => {
    for (const { g } of runs) {
      expect(g.flags['mobilised:SOV']).toBe(JUNE_41);
      for (const ally of ['ROM', 'HUN', 'FIN']) expect(g.nations[ally].atWar).toContain('SOV');
    }
  });

  it('the frontier battles are a catastrophe for the Red Army: Minsk in weeks, Smolensk by the end of August', () => {
    for (const r of runs) {
      expect(r.fell.Minsk).toBeLessThanOrEqual(JULY);
      expect(r.fell.Smolensk).toBeLessThanOrEqual(AUGUST);
      expect(r.taken[JUNE_41]).toBeGreaterThanOrEqual(6);
    }
  });

  it('Kiev falls in the autumn, not in the first rush', () => {
    for (const r of runs) {
      expect(r.fell.Kiev).toBeGreaterThanOrEqual(AUGUST);
      expect(r.fell.Kiev).toBeLessThanOrEqual(OCTOBER);
    }
  });

  it('by the end of November the front runs deep inside Russia, but no deeper than history', () => {
    for (const r of runs) {
      expect(r.taken[NOVEMBER]).toBeGreaterThanOrEqual(28);
      expect(r.taken[NOVEMBER]).toBeLessThanOrEqual(45);
    }
    expect(count((r) => r.fell.Kharkov !== undefined && r.fell.Kharkov <= NOVEMBER)).toBeGreaterThanOrEqual(14);
  });

  it('Moscow and Leningrad are still Soviet when the winter ends', () => {
    expect(count((r) => r.g.byName['Moscow'].ctrl === 'SOV')).toBeGreaterThanOrEqual(18);
    expect(count((r) => r.g.byName['Leningrad'].ctrl === 'SOV')).toBeGreaterThanOrEqual(18);
    for (const { g } of runs) expect(g.nations.SOV.alive).toBe(true);
  });

  it('the Red Army strikes back in the winter and wins ground', () => {
    expect(count((r) => r.retaken.length >= 1)).toBeGreaterThanOrEqual(15);
  });

  it('the German army is hurt by the winter but not broken', () => {
    for (const r of runs) {
      expect(r.g.byName['Minsk'].ctrl).not.toBe('SOV');
      expect(r.g.byName['Kiev'].ctrl).not.toBe('SOV');
      expect(r.taken[MARCH]).toBeGreaterThanOrEqual(22);
      expect(armySize(r.g, 'GER')).toBeGreaterThanOrEqual(100);
    }
    expect(count((r) => r.g.byName['Smolensk'].ctrl !== 'SOV')).toBeGreaterThanOrEqual(14);
  });

  it('the cost is on the scale of the real thing', () => {
    for (const r of runs) {
      expect(r.sovLost).toBeGreaterThanOrEqual(2_000_000);
      expect(r.gerLost).toBeGreaterThanOrEqual(400_000);
      expect(r.gerLost).toBeLessThanOrEqual(1_400_000);
    }
  });
});
