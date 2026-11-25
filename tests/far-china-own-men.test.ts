// China's pool and its last levy raise corps from their own men (rider B of 20:45Z, 20 November 2026, to ruling 234;
// src/campaign/manpower.ts ownMen, src/campaign/economy.ts paidInMen).
//
// Rulings 216(3) and 218(a) gave China a pool row that "holds the raising of new corps as well as the fill" and the
// last levy, 19.5 divisions' worth at most, so that a fought China does not starve where the real one did not. As
// built neither raised a corps. A corps asked 48 points with 16 kept back, and in 24 wars at stop 3 China's treasury
// never passed 30.4 on an income of 7.0 falling to 3.9. Nor could the staff's test of men pass: China's corps stand at
// about half strength as their standing shape, so the men "its armies are waiting for" (27.8 divisions' worth on the
// first day) always outran a reserve of 1.5 and a levy of 6.5. China's corps of the arms that need no industry are now
// paid for in men, as every corps is, and in no points; the men are counted in the reserve itself. No number is new.
// The state is set by hand on the first day's chart; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { PRODUCTION, aiProduction, cadreTerms, corpsCost, corpsList, deliver, paidInMen, specOf } from '../src/campaign/economy';
import { MANPOWER, levyMen, menLeft, menSpare, ownMen } from '../src/campaign/manpower';
import { traitsOf } from '../src/campaign/traits';
import { foldCorps } from '../src/campaign/fold';
import type { Build, Game } from '../src/campaign/types';

/** A war nobody plays on the first day, far side on: China is at war with Japan from the start. */
const war = (): Game => startOpen(createGame(3, null));
/** China's corps on order. */
const forming = (g: Game): Build[] => g.nations.CHN.queue.filter((b) => b.what === 'corps');
/** The last levy's row, as China's card carries it. */
const levy = () => traitsOf('CHN').find((t) => t.levy)!.levy!;

describe('China forms corps from its own men (rider B to ruling 234)', () => {
  it('is on the far side alone, and names China and nobody else', () => {
    const g = war();
    expect(MANPOWER.ownMen).toEqual(['CHN']);
    expect(ownMen(g, 'CHN')).toBe(true);
    // (The Soviet Union has the last levy too, and a treasury that pays for its corps.)
    for (const nat of ['SOV', 'JAP', 'GER', 'UK']) expect(ownMen(g, nat)).toBe(false);
  });

  it('asks no points for the arms that need no industry, and the list price for the rest', () => {
    const g = war();
    expect(corpsList(g, 'CHN', 'inf')).toBeGreaterThan(0);
    for (const kind of ['inf', 'mtn']) { expect(paidInMen(g, 'CHN', kind)).toBe(true); expect(corpsCost(g, 'CHN', kind)).toBe(0); }
    for (const kind of ['mot', 'arm', 'art', 'eng']) {
      expect(paidInMen(g, 'CHN', kind)).toBe(false);
      expect(corpsCost(g, 'CHN', kind)).toBe(corpsList(g, 'CHN', kind));
      expect(corpsCost(g, 'CHN', kind)).toBeGreaterThan(0);
    }
    // Everyone else pays as before.
    for (const nat of ['JAP', 'SOV']) expect(corpsCost(g, nat, 'inf')).toBe(corpsList(g, nat, 'inf'));
  });

  it('forms nothing from a reserve that holds less than a corps, whatever the treasury holds', () => {
    const g = war();
    expect(menLeft(g, 'CHN')).toBeLessThan(specOf(g, 'inf')!.divs);
    g.nations.CHN.pp = 500;
    aiProduction(g, 'CHN');
    expect(forming(g)).toHaveLength(0);
    expect(g.nations.CHN.pp).toBe(500);
  });

  it('forms corps from a levy\'s men with an empty treasury: paid in men, and no more than the men there are', () => {
    const g = war();
    const n = g.nations.CHN, divs = specOf(g, 'inf')!.divs;
    n.pp = 0;
    // The staff's usual test of men never passes for China: its thin corps are short of far more than a levy brings.
    levyMen(g, 'CHN', levy().men);
    const had = menLeft(g, 'CHN');
    expect(menSpare(g, 'CHN')).toBeLessThan(0);
    aiProduction(g, 'CHN');
    const q = forming(g);
    // Two forming at once is the staff's programme; the levy and the pool hold the men for two.
    expect(Math.floor(had / divs)).toBe(2);
    expect(q).toHaveLength(2);
    for (const b of q) { expect(b.kind).toBe('inf'); expect(b.cost).toBe(0); expect(b.divs).toBe(divs); expect(b.eta).toBe(g.turn + specOf(g, 'inf')!.months); }
    expect(n.pp).toBe(0);
    expect(menLeft(g, 'CHN')).toBeCloseTo(had - 2 * divs, 9);
    // And a third waits for men, not for money.
    n.queue = [];
    n.pp = 500;
    aiProduction(g, 'CHN');
    expect(forming(g)).toHaveLength(0);
  });

  it('brings the corps to the field when its months are up', () => {
    const g = war();
    const n = g.nations.CHN;
    n.pp = 0;
    levyMen(g, 'CHN', levy().men);
    aiProduction(g, 'CHN');
    const names = forming(g).map((b) => b.name);
    expect(names).toHaveLength(2);
    g.turn += specOf(g, 'inf')!.months;
    deliver(g, 'CHN');
    const raised = g.corps.filter((c) => c.nat === 'CHN' && names.includes(c.name));
    expect(raised).toHaveLength(2);
    for (const c of raised) { expect(c.str).toBe(1); expect(c.divs).toBe(specOf(g, 'inf')!.divs); expect(c.dead).toBeFalsy(); }
  });

  it('raises a cadre again first, for its own men and no points', () => {
    const g = war();
    const n = g.nations.CHN, cap = g.regions[n.capital].name;
    const a = addCorps(g, 'CHN', 'Kept corps', 'inf', 2, cap, {}), b = addCorps(g, 'CHN', 'Folded corps', 'inf', 2, cap, {});
    a.str = b.str = 0.2;
    expect(foldCorps(g, a, b)).toBe(true);
    expect(n.cadres?.map((c) => c.name)).toEqual(['Folded corps']);
    const t = cadreTerms(g, 'CHN', b);
    expect(t.cost).toBe(0);
    expect(t.divs).toBe(2);
    n.pp = 0;
    levyMen(g, 'CHN', levy().men);
    const had = menLeft(g, 'CHN');
    aiProduction(g, 'CHN');
    const q = forming(g);
    expect(q[0].cadre).toBe(b.id);
    expect(q[0].cost).toBe(0);
    // The cadre's two divisions, then one new corps: the staff's two places, and the reserve is not overdrawn.
    expect(q).toHaveLength(2);
    expect(menLeft(g, 'CHN')).toBeCloseTo(had - 2 - specOf(g, 'inf')!.divs, 9);
    expect(n.pp).toBe(0);
  });

  it('leaves the staff\'s bank mark for everyone else: Japan with an empty treasury orders nothing', () => {
    const g = war();
    g.nations.JAP.pp = PRODUCTION.aiReserve;
    const before = g.nations.JAP.queue.filter((b) => b.what === 'corps').length;
    aiProduction(g, 'JAP');
    expect(g.nations.JAP.queue.filter((b) => b.what === 'corps').length).toBe(before);
  });
});
