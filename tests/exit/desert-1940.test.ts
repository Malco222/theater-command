// The framework's Phase 3 Mediterranean war, run early against the campaign rules: with every
// nation on the historical plan and nobody at the controls, the desert war should swing the way
// it did. The Italians shuffle into Egypt and stop, Operation Compass throws them back across
// Cyrenaica, Rommel wins it all back in a month but cannot take Tobruk, and Operation Crusader
// lifts the siege at the end of 1941. Through all of it Egypt never falls.
//
// The plan only sets intentions. Every region still has to be won under the rules the player
// uses, so the bands below are loose on purpose and are checked across twenty sets of dice.
import { describe, expect, it } from 'vitest';
import { createGame, men } from '../../src/campaign/game';
import { runTurnAI } from '../../src/campaign/turn';
import type { Game } from '../../src/campaign/types';

const JUNE_40 = 10, NOV_40 = 15, FEB_41 = 18, MAY_41 = 21, OCT_41 = 26, DEC_41 = 28, APRIL_42 = 32;
const EGYPT = ['Mersa Matruh', 'El Alamein', 'Alexandria', 'Cairo', 'Port Said', 'Suez'];
const LIBYA = ['Tripoli', 'Sirte', 'El Agheila', 'Agedabia', 'Benghazi', 'Mechili', 'Tobruk', 'Bardia'];

interface Run {
  g: Game;
  /** Who held each place at the end of each month. */
  held: Record<number, Record<string, string>>;
  /** Men in Libya and Egypt at the end of each month, by nation. */
  army: Record<number, Record<string, number>>;
  /** Months in which any part of Egypt behind the frontier was not British. */
  egyptLost: number[];
  /** Where Rommel's corps stood at the end of May 1941. */
  rommel: string | null;
}

function history(seed: number): Run {
  const g = createGame(seed, null);
  const run: Run = { g, held: {}, army: {}, egyptLost: [], rommel: null };
  const africa = new Set([...EGYPT, ...LIBYA, 'Sidi Barrani', 'Misrata', 'Derna', 'Bir Hakeim', 'Nalut'].map((n) => g.byName[n].id));
  for (let t = 1; t <= APRIL_42; t++) {
    runTurnAI(g);
    run.held[t] = Object.fromEntries([...EGYPT, ...LIBYA, 'Sidi Barrani', 'Malta'].map((n) => [n, g.byName[n].ctrl]));
    run.army[t] = {};
    for (const c of g.corps) if (africa.has(c.reg)) run.army[t][c.nat] = (run.army[t][c.nat] ?? 0) + men(c);
    if (EGYPT.some((n) => g.byName[n].ctrl !== 'UK')) run.egyptLost.push(t);
    if (t === MAY_41) {
      const gen = g.generals.find((x) => x.name === 'Erwin Rommel');
      const c = gen ? g.corps.find((x) => x.id === gen.corps) : undefined;
      run.rommel = c ? g.regions[c.reg].name : null;
    }
  }
  return run;
}

const SEEDS = Array.from({ length: 20 }, (_, i) => i + 1);
const runs = SEEDS.map(history);
const count = (ok: (r: Run) => boolean): number => runs.filter(ok).length;
const axis = (who: string): boolean => who === 'ITA' || who === 'GER';

describe('Phase 3 exit test: the desert war on the historical plan', () => {
  it('Italy enters the war as France is falling, and the desert stays quiet through the summer', () => {
    for (const { g, held } of runs) {
      // Mussolini waits for France to break: late May or June 1940, depending on how fast it goes.
      const entered = g.flags['war:ITA:UK'] ?? g.flags['war:UK:ITA'];
      expect(entered).toBeGreaterThanOrEqual(JUNE_40 - 1);
      expect(entered).toBeLessThanOrEqual(JUNE_40);
      expect(held[JUNE_40 + 2]['Sidi Barrani']).toBe('UK');
      expect(held[JUNE_40 + 2]['Bardia']).toBe('ITA');
    }
  });

  it('autumn 1940: the Italians reach Sidi Barrani and stop there', () => {
    expect(count((r) => r.held[NOV_40]['Sidi Barrani'] === 'ITA')).toBeGreaterThanOrEqual(18);
    for (const { held } of runs) expect(held[NOV_40]['Mersa Matruh']).toBe('UK');
  });

  it('Operation Compass: by February 1941 the British hold Cyrenaica and the 10th Army is wrecked', () => {
    expect(count((r) => r.held[FEB_41]['Tobruk'] === 'UK' && r.held[FEB_41]['Benghazi'] === 'UK')).toBeGreaterThanOrEqual(18);
    expect(count((r) => r.held[FEB_41]['Agedabia'] === 'UK')).toBeGreaterThanOrEqual(16);
    // More than half the Italian army in Libya is gone, to a force a fraction of its size.
    expect(count((r) => r.army[FEB_41].ITA < 0.55 * r.army[NOV_40].ITA)).toBeGreaterThanOrEqual(18);
    expect(count((r) => r.army[NOV_40].ITA > 1.5 * r.army[NOV_40].UK)).toBeGreaterThanOrEqual(18);
    // They do not run on to Tripoli.
    for (const { held } of runs) expect(held[FEB_41]['Tripoli']).toBe('ITA');
  });

  it('Rommel: by May 1941 the Axis is back on the Egyptian frontier, but Tobruk holds', () => {
    expect(count((r) => axis(r.held[MAY_41]['Benghazi']) && axis(r.held[MAY_41]['Agedabia']))).toBeGreaterThanOrEqual(18);
    expect(count((r) => axis(r.held[MAY_41]['Bardia']))).toBeGreaterThanOrEqual(16);
    expect(count((r) => r.held[MAY_41]['Tobruk'] === 'UK')).toBeGreaterThanOrEqual(18);
    // The general who led a panzer division in France is the same man who lands at Tripoli.
    for (const { g } of runs) expect(g.generals.filter((x) => x.name === 'Erwin Rommel').length).toBe(1);
    expect(count((r) => r.rommel !== null && r.g.byName[r.rommel].overseas)).toBeGreaterThanOrEqual(18);
    for (const r of runs) expect(r.army[MAY_41].GER ?? 0).toBeGreaterThan(0);
  });

  it('the siege: Tobruk is still British in October 1941', () => {
    expect(count((r) => r.held[OCT_41]['Tobruk'] === 'UK')).toBeGreaterThanOrEqual(18);
    expect(count((r) => axis(r.held[OCT_41]['Bardia']))).toBeGreaterThanOrEqual(16);
  });

  it('Operation Crusader: by December 1941 the siege is lifted and the frontier is cleared', () => {
    expect(count((r) => r.held[DEC_41]['Tobruk'] === 'UK' && r.held[DEC_41]['Bardia'] === 'UK' && r.held[DEC_41]['Sidi Barrani'] === 'UK')).toBeGreaterThanOrEqual(18);
    // The Axis army is beaten, not wiped out: it still holds Tripolitania.
    for (const { held } of runs) expect(axis(held[DEC_41]['Tripoli'])).toBe(true);
    expect(count((r) => (r.army[DEC_41].ITA ?? 0) + (r.army[DEC_41].GER ?? 0) > 40000)).toBeGreaterThanOrEqual(18);
  });

  it('Egypt never falls, and Malta holds out', () => {
    for (const { egyptLost, held } of runs) {
      expect(egyptLost).toEqual([]);
      expect(held[APRIL_42]['Malta']).toBe('UK');
    }
  });

  it('nobody collapses over a colony', () => {
    for (const { g } of runs) {
      expect(g.nations.ITA.alive).toBe(true);
      expect(g.nations.UK.alive).toBe(true);
    }
  });
});
