// The hold rule (the design director, 20 November 2026 05:35Z; sandbox stops 4 and 5 only): a held row on the war's
// clock keeps its hold, and loses it to its own cause, whichever comes first: the blow lands on it, or the record's
// phase that freed it comes due on its own war's clock. Stops 1 to 3 are as they were.
import { describe, expect, it } from 'vitest';
import { createGame, setWar, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';
import { struck } from '../src/campaign/warclock';

function grand(stop: number, seed = 3): Game {
  const g = startOpen(createGame(seed, null));
  setStop(g, stop, true);
  return g;
}
const held = (g: Game, name: string): boolean | undefined => g.corps.find((c) => c.name === name && !c.dead)?.hold;
/** Runs a nation's month openings from 'from' to 'to'; a war with Germany opens in month 'war'. */
function months(g: Game, nat: string, from: number, to: number, war?: number): void {
  for (let t = from; t <= to; t++) {
    g.turn = t;
    if (t === war) {
      setWar(g, 'GER', nat);
      g.flags[`war:GER:${nat}`] = t;
    }
    startPhase(g, nat);
  }
}

describe('the Kursk fronts on the Soviet clock', () => {
  it('stop 5, war in month 30: the Central Front and 2nd Tank Army go in at its month 25, the Voronezh Front and 1st Tank Army at 26', () => {
    const g = grand(5);
    months(g, 'SOV', 23, 54, 30);
    for (const n of ['Central Front', '2nd Tank Army', 'Voronezh Front', '1st Tank Army']) expect(held(g, n)).toBe(true);
    months(g, 'SOV', 55, 55);
    expect(held(g, 'Central Front')).toBe(false);
    expect(held(g, '2nd Tank Army')).toBe(false);
    expect(held(g, 'Voronezh Front')).toBe(true);
    expect(held(g, '1st Tank Army')).toBe(true);
    months(g, 'SOV', 56, 56);
    expect(held(g, 'Voronezh Front')).toBe(false);
    expect(held(g, '1st Tank Army')).toBe(false);
  });

  it('stop 4, war on the record\'s month: they go in on Kutuzov\'s and Rumyantsev\'s months', () => {
    const g = grand(4);
    months(g, 'SOV', 22, 46, 22);
    expect(held(g, 'Central Front')).toBe(true);
    months(g, 'SOV', 47, 47);
    expect(held(g, 'Central Front')).toBe(false);
    expect(held(g, 'Voronezh Front')).toBe(true);
    months(g, 'SOV', 48, 48);
    expect(held(g, 'Voronezh Front')).toBe(false);
  });

  it('stop 5: the blow landing on a held front frees it at once, and only the fronts it lands on', () => {
    const g = grand(5);
    months(g, 'SOV', 22, 46, 22);
    struck(g, [{ name: 'Voronezh Front', nat: 'SOV' }]);
    expect(held(g, 'Voronezh Front')).toBe(false);
    expect(held(g, 'Central Front')).toBe(true);
    expect(g.flags[`wh:${g.corps.find((c) => c.name === 'Voronezh Front')!.id}`]).toBeUndefined();
  });

  it('stop 3: the fronts come held on the record\'s month and nothing on the clock frees them', () => {
    const g = grand(3);
    months(g, 'SOV', 22, 50, 22);
    expect(held(g, 'Central Front')).toBe(true);
    struck(g, [{ name: 'Central Front', nat: 'SOV' }]);
    expect(held(g, 'Central Front')).toBe(true);
    expect(Object.keys(g.flags).some((k) => k.startsWith('wh:'))).toBe(false);
  });
});

describe('the Weser and Norway forces', () => {
  it('stop 5: they keep their hold with no month on the clock; the blow or their landing frees them', () => {
    const g = grand(5);
    months(g, 'GER', 2, 12);
    for (const n of ['Group XXI', 'Mountain Corps Norway', 'Trondheim Landing Group', 'Bergen Landing Group']) {
      expect(held(g, n)).toBe(true);
      expect(g.flags[`wh:${g.corps.find((c) => c.name === n)!.id}`]).toBe(-1);
    }
    struck(g, [{ name: 'Group XXI', nat: 'GER' }]);
    expect(held(g, 'Group XXI')).toBe(false);
    expect(held(g, 'Bergen Landing Group')).toBe(true);
    // Sent some other way (its landing): the rule lets the flag go the next month.
    const b = g.corps.find((c) => c.name === 'Bergen Landing Group')!;
    b.hold = false;
    months(g, 'GER', 13, 13);
    expect(g.flags[`wh:${b.id}`]).toBeUndefined();
  });
});
