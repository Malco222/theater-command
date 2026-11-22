// A front's month of moves (strategy.json 'fronts.month'; the design director, 22 November 2026, 13:27Z; after
// release 1). The one-map fault: a staff's sum for a front counted every corps of its side on the friendly land
// touching it, however far, so the British in Egypt counted the Red Army and Germany counted Rommel's army into an
// attack out of Tunis. Behind its own key a front counts only what the month's moves can bring to it, by road or by
// train, and keeps back for the other fronts only what they cannot find nearer. It shipped switched off; it was
// switched on in release 2's rules on 22 November 2026 (the design director's ruling of 17:59Z, Brian's choice of
// 18:01Z) once its 96 wars held every mark of the gate.

import { afterEach, describe, expect, it } from 'vitest';
import { createGame, marchWeeks, railInto } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { fronts, startOpen, type Front } from '../src/campaign/staff';
import { STRATEGY, frontMonth } from '../src/campaign/strategy';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const start = (seed: number, stop: number): Game => {
  const g = startOpen(createGame(seed, null));
  if (stop !== 3) setStop(g, stop, true);
  return g;
};
const same = (a: Front, b: Front): boolean => a.key === b.key && a.edge.length === b.edge.length && a.edge.every((id, k) => b.edge[k] === id);

describe("a front's month of moves", () => {
  afterEach(() => { STRATEGY.fronts!.month = true; });

  it('is on in release 2\'s rules, has a key of its own, and is the grand campaign\'s alone', () => {
    expect(STRATEGY.fronts!.month).toBe(true);
    const g = start(20, 1);
    expect(frontMonth(g)).toBe(true);
    STRATEGY.fronts!.month = false;
    expect(frontMonth(g)).toBe(false);
    STRATEGY.fronts!.month = true;
    // (History's own campaigns keep the sums they were measured with.)
    expect(frontMonth(createGame(20, null))).toBe(false);
  });

  it('the roads and the trains it reads are the game\'s own', () => {
    const g = start(20, 1);
    const ger = g.byName.Berlin, alps = g.regions.find((r) => r.owner === 'GER' && r.terrain === 'mountain');
    // A week by road through country long friendly; a mountain province costs a week more, and ground not ours two weeks to enter.
    expect(marchWeeks(g, 'GER', ger, ger.nb[0], false)).toBe(1);
    if (alps) expect(marchWeeks(g, 'GER', alps, alps.nb[0], false)).toBe(2);
    expect(marchWeeks(g, 'GER', g.byName.Warsaw, g.byName.Warsaw.nb[0], false)).toBeGreaterThanOrEqual(2);
    // The trains run through Germany's own country and stop at the frontier of a country that is not its ally; none runs in the desert.
    const rail = railInto(g, 'GER', [ger.id]);
    expect(rail[ger.id]).toBe(1);
    expect(g.regions.filter((r) => r.owner === 'GER' && rail[r.id]).length).toBeGreaterThan(5);
    expect(rail[g.byName.Paris.id]).toBe(0);
    expect(rail[g.byName.Moscow.id]).toBe(0);
    // (Libya has no railway: from Tripoli a train brings nobody but who stands there; and from the open desert nobody at all.)
    const libya = railInto(g, 'ITA', [g.byName.Tripoli.id]);
    expect(libya.reduce((n, x) => n + x, 0)).toBe(1);
    expect(railInto(g, 'ITA', [g.byName.Nalut.id]).reduce((n, x) => n + x, 0)).toBe(0);
    // (Egypt's runs along the Nile and stops at the sand.)
    const nile = railInto(g, 'UK', [g.byName.Cairo.id]);
    expect(g.regions.filter((r) => nile[r.id]).map((r) => r.name).sort()).toEqual(['Alexandria', 'Cairo', 'Port Said']);
  });

  it('on: a front never counts more than it did, the enemy\'s side of the sum is untouched, and an army a season away is not counted', () => {
    const g = start(20, 1);
    // (The war is played to month 34 by the old sum, the war the fault was found in; the two sums are then read side by side.)
    STRATEGY.fronts!.month = false;
    while (g.turn < 34) runTurnAI(g);
    let seen = 0, less = 0;
    const pairs: [string, Front, Front][] = [];
    for (const nat of Object.keys(g.nations)) {
      if (!g.nations[nat].alive || !g.nations[nat].atWar.length) continue;
      STRATEGY.fronts!.month = false;
      const off = fronts(g, nat);
      STRATEGY.fronts!.month = true;
      const on = fronts(g, nat);
      expect(on.length, nat).toBe(off.length);
      for (const F of off) {
        const G = on.find((x) => same(x, F));
        expect(G, `${nat} v ${F.key}`).toBeTruthy();
        seen++;
        expect(G!.atk, `${nat} v ${F.key}`).toBeLessThanOrEqual(F.atk + 1e-9);
        expect(G!.def).toBe(F.def);
        expect(G!.defNear).toBe(F.defNear);
        expect(G!.threat).toBe(F.threat);
        expect(G!.fixed).toBe(F.fixed);
        if (G!.atk < F.atk - 1e-9) less++;
        pairs.push([nat, F, G!]);
      }
    }
    expect(seen).toBeGreaterThan(5);
    expect(less).toBeGreaterThan(0);
    // The British front in Egypt: by the old sum it counted the whole of its side on one piece of land from Cairo to
    // the Eastern Front (British Iraq touches Soviet-held Iran); by the month's it counts what stands in reach of the Nile.
    const egypt = pairs.find(([nat, F]) => nat === 'UK' && F.ours.some((id) => g.regions[id].name === 'Cairo'));
    expect(egypt).toBeTruthy();
    expect(egypt![2].atk).toBeLessThan(egypt![1].atk * 0.1);
    expect(egypt![2].ratio).toBeLessThan(egypt![1].ratio);
    // And off again the sum is the old one, to the last figure.
    STRATEGY.fronts!.month = false;
    const again = fronts(g, 'UK').find((x) => same(x, egypt![1]))!;
    expect(again.atk).toBe(egypt![1].atk);
    expect(again.ratio).toBe(egypt![1].ratio);
  }, 300_000);
});
