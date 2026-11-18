// Ruling 228, 1a: the trap before the Winter War. The Soviet call-up (east.json 'mobilisation') starts with a war on a
// great power (diplomacy.json 'majors') and runs while one lasts. A war with Finland, Estonia or any other small
// country calls up nobody, so the waves of 1941 still come in 1941.
import { describe, expect, it } from 'vitest';
import { EASTERN, createGame, declareWar, setWar, standingCallUp, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const NOVEMBER_39 = 3, JUNE_41 = 22;
const rifle = (g: Game): number => g.corps.filter((c) => c.nat === 'SOV' && !c.dead && c.name.endsWith(EASTERN.mobilisation.SOV.name)).length;

describe('the Soviet call-up and a war with a small country (ruling 228, 1a)', () => {
  for (const kind of ['history', 'grand'] as const) {
    it(`a war with Finland in November 1939 leaves the call-up as it was (${kind})`, () => {
      const g = kind === 'grand' ? startOpen(createGame(3, null)) : createGame(3, null);
      g.turn = NOVEMBER_39;
      setWar(g, 'SOV', 'FIN');
      for (const t of [3, 4, 5, 6]) { g.turn = t; startPhase(g, 'SOV'); }
      expect(g.flags['mobilised:SOV']).toBeUndefined();
      expect(rifle(g)).toBe(0);
      expect(g.headlines.some((h) => /general mobilisation/.test(h.text))).toBe(false);
      if (kind === 'grand') expect(standingCallUp(g, 'SOV')).toBeNull();
      // The war with Germany starts it, with the first wave of the plan, however long the war with Finland has run.
      g.turn = JUNE_41;
      declareWar(g, 'GER', 'SOV');
      startPhase(g, 'SOV');
      expect(g.flags['mobilised:SOV']).toBe(JUNE_41);
      expect(rifle(g)).toBeGreaterThan(0);
      expect(rifle(g)).toBeLessThanOrEqual(EASTERN.mobilisation.SOV.perMonth[0]);
      expect(g.headlines.some((h) => /general mobilisation/.test(h.text) && h.nat === 'SOV')).toBe(true);
    });
  }
});
