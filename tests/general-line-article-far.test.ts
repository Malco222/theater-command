// The general's line keeps the base's article, the far side (game.ts `generalsThe`). With the switch off, the far side's
// "Special Force (3rd Indian Division, the Chindits)" and "Mars Task Force (...)" rows print their general's line exactly
// as 70d6f6a1f does: "... with the Special Force (...)". The UI director's section 7 case 5 is for the telling's own
// sentences (arrivalwaits.ts `theOf`) only. The refuter's switch-01 (its F2 probe: seed 3, nobody seated, the switch never
// touched, the United Kingdom's months 53 to 55 and 62 to 64). The old chart: tests/general-line-article.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import farWorld from '../data/campaign/far-world.json';
import { createGame, declareWar, generalsThe, startPhase } from '../src/campaign/game';
import { stopOf } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

/** The base's rule, as 70d6f6a1f writes it in game.ts's general's line. */
const BASE = (name: string): string => (/^(the |[A-Z] Force|Creforce)/i.test(name) ? '' : 'the ');

describe("the general's line's article is the base's, for every dated row of the far side", () => {
  it('a table over the rows as the game reads them (setup-1939.json laid with the far side\'s files) and far-world.json\'s own', () => {
    const laid = (setup as any).expeditions as any[][];
    const own = (farWorld as any)['setup-1939'].expeditions as any[][];
    expect(own.length).toBeGreaterThan(50);
    const differ = [...laid, ...own].filter((r) => generalsThe(r[2]) !== BASE(r[2])).map((r) => r[2]);
    expect(differ).toEqual([]);
    for (const nm of ['Special Force (3rd Indian Division, the Chindits)', "Mars Task Force (America's 475th Infantry and 124th Cavalry)"]) {
      expect(laid.some((r) => r[2] === nm)).toBe(true);
      expect(generalsThe(nm)).toBe('the ');
    }
  });
});

describe('a real far-side war, the switch off: the Chindits and Mars Task Force as the base prints them', () => {
  it('seed 3, nobody seated, stop 3, the United Kingdom\'s months 53 to 55 and 62 to 64', () => {
    const g: Game = startOpen(createGame(3, null));
    expect(g.flags.fromWait).toBeUndefined();
    expect(stopOf(g)).toBe(3);
    declareWar(g, 'UK', 'JAP');
    for (const t of [53, 54, 55, 62, 63, 64]) { g.turn = t; startPhase(g, 'UK'); }
    const lines = g.headlines.filter((h) => /Wingate|Willey/.test(h.text)).map((h) => h.text);
    expect(lines).toEqual([
      'Orde Wingate arrives in Dibrugarh with the Special Force (3rd Indian Division, the Chindits).',
      "John Willey arrives in Dibrugarh with the Mars Task Force (America's 475th Infantry and 124th Cavalry).",
    ]);
  });
});
