// Japan asks for terms (ruling 196, point 1; the Pacific planner's paper 06, "par in the Pacific", section 3).
//
// History's Japan gave in with no enemy army on its four main islands. A computer Japan asks for terms at the opening
// of its month when both of these hold: an enemy holds a twentieth of its home ground by worth (Okinawa alone is
// enough, Iwo Jima alone is not), and privation has worn all it can off its will to fight (the two books' count, at
// its limit). It gives in as any country does, to the enemy who holds the most of its home ground. No date decides
// it, and a player's Japan is spared. The test is a row by nation (data/campaign/far-books.json), and Japan's is the
// only row. Each state is set by hand on the first day's chart; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { atWar, checkCollapse, checkTerms, collapseScore, createGame, declareWar, homeTaken, startPhase, termsOf, victorOf } from '../src/campaign/game';
import { RES, hungerLost, storesOf } from '../src/campaign/resources';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';
import farBooks from '../data/campaign/far-books.json';

/** The first day's chart, with Japan at war with America and Britain. */
const war = (): Game => {
  const g = startOpen(createGame(3, null));
  for (const e of ['USA', 'UK']) if (!atWar(g, 'JAP', e)) declareWar(g, 'JAP', e);
  return g;
};
const take = (g: Game, name: string, by: string): void => { g.byName[name].ctrl = by; };
/** Privation at its limit: the count of hungry months that takes the whole 30 in 100 off the will to fight. */
const starve = (g: Game, nat = 'JAP', months = RES.will.most / RES.will.per): void => { storesOf(g, nat)!.hunger = months; };

describe('Japan asks for terms (ruling 196)', () => {
  it('the test is a row by nation, and Japan\'s is the only row: a twentieth of its home ground', () => {
    expect(RES.will.terms).toEqual({ JAP: { home: 0.05 } });
    expect((farBooks as any)['resources.json'].will).toEqual({ terms: { JAP: { home: 0.05 } } });
    const g = war();
    expect(termsOf(g, 'JAP')).toEqual({ ground: false, privation: false, taken: 0, share: 0.05 });
    for (const nat of ['GER', 'ITA', 'UK', 'USA', 'SOV', 'CHN']) expect(termsOf(g, nat), nat).toBeNull();
    // Japan's home ground on the chart: 28 provinces worth 29, Tokyo 6 of it, Okinawa 2, Iwo Jima 1.
    const home = g.regions.filter((r) => r.owner === 'JAP' && !r.overseas);
    expect([home.length, home.reduce((s, r) => s + r.vp, 0)]).toEqual([28, 29]);
    expect(['Tokyo', 'Okinawa', 'Iwo Jima'].map((nm) => g.byName[nm].vp)).toEqual([6, 2, 1]);
  });

  it('the war has reached its home ground with Okinawa alone, and not with Iwo Jima alone', () => {
    const g = war();
    take(g, 'Iwo Jima', 'USA');
    expect(homeTaken(g, 'JAP')).toBeCloseTo(1 / 29, 12);
    expect(termsOf(g, 'JAP')!.ground).toBe(false);
    take(g, 'Iwo Jima', 'JAP');
    take(g, 'Okinawa', 'USA');
    expect(homeTaken(g, 'JAP')).toBeCloseTo(2 / 29, 12);
    expect(termsOf(g, 'JAP')!.ground).toBe(true);
    // Iwo Jima and one more of its worth are a second gate.
    take(g, 'Okinawa', 'JAP');
    take(g, 'Iwo Jima', 'USA');
    take(g, 'Sapporo', 'USA');
    expect(termsOf(g, 'JAP')!.ground).toBe(true);
    // Ground a friend holds is not taken: an enemy must hold it.
    take(g, 'Sapporo', 'JAP');
    take(g, 'Okinawa', 'GER');
    expect(atWar(g, 'JAP', 'GER')).toBe(false);
    expect(termsOf(g, 'JAP')!.ground).toBe(false);
  });

  it('its people can bear no more when privation has worn all it can off the will to fight', () => {
    const g = war();
    expect(RES.will.most / RES.will.per).toBeCloseTo(3.75, 12);
    starve(g, 'JAP', 3.7);
    expect(hungerLost(g, 'JAP')).toBeLessThan(RES.will.most);
    expect(termsOf(g, 'JAP')!.privation).toBe(false);
    starve(g);
    expect(hungerLost(g, 'JAP')).toBeCloseTo(RES.will.most, 12);
    expect(termsOf(g, 'JAP')!.privation).toBe(true);
    starve(g, 'JAP', 9);
    expect(termsOf(g, 'JAP')!.privation).toBe(true);
  });

  it('neither alone brings Japan to terms: not the gate with the larder stocked, not hunger with no enemy on its home ground', () => {
    const g = war();
    take(g, 'Okinawa', 'USA');
    expect(checkTerms(g, 'JAP')).toBe(false);
    take(g, 'Okinawa', 'JAP');
    starve(g);
    expect(termsOf(g, 'JAP')).toMatchObject({ ground: false, privation: true });
    expect(checkTerms(g, 'JAP')).toBe(false);
    // Iwo Jima lost and the people starving: still not.
    take(g, 'Iwo Jima', 'USA');
    expect(checkTerms(g, 'JAP')).toBe(false);
    expect(g.nations.JAP.alive).toBe(true);
    // And by the reckoning every country keeps, this Japan is far from giving in: under half of what it bears.
    take(g, 'Okinawa', 'USA');
    expect(collapseScore(g, 'JAP')).toBeLessThan(0.5 * g.nations.JAP.resilience);
    checkCollapse(g, 'JAP', 'USA');
    expect(g.nations.JAP.alive).toBe(true);
  });

  it('with both it asks for terms at the opening of its month, and gives in to the enemy who holds the most of its home ground', () => {
    const g = war();
    take(g, 'Okinawa', 'USA');
    take(g, 'Iwo Jima', 'UK');
    starve(g, 'JAP', 6);
    expect(termsOf(g, 'JAP')).toMatchObject({ ground: true, privation: true });
    expect(victorOf(g, 'JAP')).toBe('USA');
    const news = g.headlines.length;
    startPhase(g, 'JAP');
    const J = g.nations.JAP;
    expect(J.alive).toBe(false);
    expect(J.fellOn).toMatchObject({ turn: g.turn, to: 'USA', how: 'beaten' });
    // (Ruling 201, point 4: Europe is still at war, so the fall's band carries its second line.)
    expect(g.headlines.slice(news).filter((h) => h.kind === 'fall').map((h) => h.text)).toEqual(['Japan accepts terms and is out of the war. Japan is beaten. The war goes on in Europe: Britain, France and Germany are still at war.']);
    expect(g.headlines.slice(news).some((h) => /capitulates/.test(h.text))).toBe(false);
    // Its armies lay down their arms, and its wars are over.
    expect(g.corps.filter((c) => c.nat === 'JAP' && !c.dead).length).toBe(0);
    expect(J.atWar).toEqual([]);
    expect(g.nations.USA.atWar.includes('JAP')).toBe(false);
  });

  it('a player\'s Japan is spared: he decides when he is beaten', () => {
    const g = startOpen(createGame(3, 'JAP'));
    for (const e of ['USA', 'UK']) if (!atWar(g, 'JAP', e)) declareWar(g, 'JAP', e);
    take(g, 'Okinawa', 'USA');
    starve(g, 'JAP', 6);
    expect(termsOf(g, 'JAP')).toMatchObject({ ground: true, privation: true });
    expect(checkTerms(g, 'JAP')).toBe(false);
    expect(g.nations.JAP.alive).toBe(true);
  });

  it('it is read for the nations with a row alone: a starving Germany with an enemy on its home ground does not ask', () => {
    const g = startOpen(createGame(3, null));
    if (!atWar(g, 'GER', 'UK')) declareWar(g, 'GER', 'UK');
    const home = g.regions.filter((r) => r.owner === 'GER' && !r.overseas && !r.cap && r.vp > 0).slice(0, 3);
    for (const r of home) r.ctrl = 'UK';
    starve(g, 'GER', 6);
    expect(homeTaken(g, 'GER')).toBeGreaterThan(0.05);
    expect(hungerLost(g, 'GER')).toBeCloseTo(RES.will.most, 12);
    expect(checkTerms(g, 'GER')).toBe(false);
    expect(g.nations.GER.alive).toBe(true);
  });
});
