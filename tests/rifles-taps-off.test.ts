// China's rifles, stage B2, test 1 of tests/far-rifles-taps.test.ts, kept in a file of its own because the far switch
// is hoisted per file: with the switch off no nation carries a rifles row, so every tap returns today's figure, no
// order draws or leaves a flag of the store's, and every price is the list price.
import { describe, expect, it } from 'vitest';
import { RIFLES, armOrder, binds, canArm, carries, carve, carved, noRifles, riflesFor } from '../src/campaign/rifles';
import { cancelBuild, corpsCost, corpsKinds, corpsList, orderCorps, specOf } from '../src/campaign/economy';
import { offers } from '../src/campaign/arsenal';
import { addCorps, createGame } from '../src/campaign/game';
import { foldCorps } from '../src/campaign/fold';
import { levyMen, menLeft, raisable } from '../src/campaign/manpower';
import { startOpen } from '../src/campaign/staff';
import { breakUp, setBreakUp } from '../src/campaign/breakup';
import { divisionsOf } from '../src/campaign/armies';

describe('far off', () => {
  it('1. far off: no row, no draw, no carve, no level', () => {
    expect(RIFLES.nations).toEqual({});
    const g = startOpen(createGame(3, null));
    const nats = Object.keys(g.nations);
    expect(nats.length).toBeGreaterThan(10);
    for (const nat of nats) {
      expect([carries(g, nat), binds(g, nat), canArm(g, nat, 1e9), carve(g, nat, 3), carved(g, nat, 48, 3), noRifles(g, nat, 1e9), riflesFor(g, nat, 3)], nat).toEqual([false, false, true, 0, 0 + 48, null, {}]);
      expect(armOrder(g, nat, 3), nat).toBe(0);
      for (const kind of corpsKinds(g)) expect(corpsCost(g, nat, kind), `${nat} ${kind}`).toBe(corpsList(g, nat, kind));
    }
    // An order of the nation's that the far side would bind, if it stands on this chart, draws nothing and leaves no flag of the store's.
    const nat = g.nations.CHN?.alive ? 'CHN' : 'GER', n = g.nations[nat];
    levyMen(g, nat, 20);
    n.pp = 500;
    const men = menLeft(g, nat);
    const b = orderCorps(g, nat, 'inf', n.capital);
    expect(b).not.toBeNull();
    expect(b!.rifles).toBeUndefined();
    expect(Object.keys(g.flags).filter((k) => /^(sa|sam|rfa|rfo|rflm|rflc):/.test(k))).toEqual([]);
    expect(menLeft(g, nat)).toBeCloseTo(men - specOf(g, 'inf')!.divs, 9);
    expect(cancelBuild(g, nat, b!.id)).toBe(true);
    expect(menLeft(g, nat)).toBeCloseTo(men, 9);
    expect(Object.keys(g.flags).filter((k) => /^(sa|sam|rfa|rfo|rflm|rflc):/.test(k))).toEqual([]);
    for (const o of offers(g, nat, n.capital)) { expect(o.rifles, o.key).toBeUndefined(); expect(o.why ?? '', o.key).not.toMatch(/rifles/); }
    // A fold with men left over, into a corps that would pass its full (the ruling of 22:29Z): the men go home as
    // today, and nothing of the store's is written.
    const capName = g.regions[n.capital].name;
    const a = addCorps(g, nat, 'Kept corps', 'inf', 2, capName, {}), c = addCorps(g, nat, 'Folded corps', 'inf', 2, capName, {});
    a.str = c.str = 0.6;
    const m = menLeft(g, nat);
    expect(foldCorps(g, a, c)).toBe(true);
    expect(a.str).toBe(1);
    expect(menLeft(g, nat) - m).toBeCloseTo(0.4, 12);
    expect(Object.keys(g.flags).filter((k) => /^(sa|sam|rfa|rfo|rflm|rflc):/.test(k))).toEqual([]);
  });

  it("1. far off: the fold's news says no rifles, with men sent back or none (the UI director's sixth read, point 3, is under the rifles switch)", () => {
    // (With the far side off China is not on the chart: Germany, a nation the rows would never name, as the fold's
    // speaker; the sentence is the one any nation without a store or a scale of its own reads.)
    const g = startOpen(createGame(3, 'GER'));
    const nat = 'GER', capName = g.regions[g.nations[nat].capital].name;
    levyMen(g, nat, 20);
    const last = (): string => g.headlines[g.headlines.length - 1].text;
    const a = addCorps(g, nat, 'Kept corps', 'inf', 2, capName, {}), c = addCorps(g, nat, 'Folded corps', 'inf', 2, capName, {});
    a.str = c.str = 0.3;
    expect(foldCorps(g, a, c)).toBe(true);
    expect(last()).toBe(`Folded corps is folded into Kept corps at ${capName}: 18,000 men in one corps. Its cadre has gone home and can be raised again from the arsenal.`);
    const d = addCorps(g, nat, 'Second kept corps', 'inf', 2, capName, {}), e = addCorps(g, nat, 'Second folded corps', 'inf', 2, capName, {});
    d.str = e.str = 0.6;
    expect(foldCorps(g, d, e)).toBe(true);
    expect(last()).toBe(`Second folded corps is folded into Second kept corps at ${capName}: 30,000 men in one corps, 6,000 sent back to the reserve. Its cadre has gone home and can be raised again from the arsenal.`);
  });

  it("1. far off: a corps with an establishment below 1 (laid by hand) breaks up as built (the design director's ruling 3 of 06:04Z is under the far switch)", () => {
    const g = startOpen(createGame(3, 'GER'));
    g.phase = g.order.indexOf('GER');
    setBreakUp(g, true);
    // (On this chart two corps stand below 1, Kiev's fortified region in its town and Helsinki's garrison of one
    // division, and neither can be broken up: here one is laid by hand.)
    const c = g.corps.find((x) => x.nat === 'GER' && !x.dead && x.divs >= 3 && x.kind === 'inf' && x.town === undefined && x.joint === undefined && x.leave === undefined && !x.school)!;
    c.full = 0.8;
    c.str = 0.8;
    const cards = divisionsOf(g, c), best = cards.reduce((a, b) => (b.str > a.str ? b : a), cards[0]);
    expect(best.str).toBeGreaterThan(0.8 + 1e-6);
    const men = c.str * c.divs, d0 = c.divs;
    const p = breakUp(g, c, [best.slot])!;
    // The card picked takes its own men, above the part's full, as built.
    expect(p.str).toBe(best.str);
    expect(c.str).toBe((men - best.str) / (d0 - 1));
    expect(Object.keys(g.flags).filter((k) => /^(sa|sam|rfa|rfo|rflm|rflc):/.test(k))).toEqual([]);
  });

  it("1. far off: the arsenal's floor of a tenth of a division stands as built (the design director's ruling 4 of 06:04Z is under the far switch)", () => {
    const g = startOpen(createGame(3, 'GER'));
    expect(g.flags.open).toBe(1);
    // An order under a tenth of a division's worth is held to a tenth, as built: 0.097 in reserve does not fill 0.09.
    for (const divs of [0, 0.05, 0.09, 0.1, 0.27, 0.5, 1, 3]) {
      for (const left of [0, 0.03, 0.05, 0.0899, 0.09, 0.097, 0.0999, 0.1, 0.27, 0.6, 1, 2.99, 3, 7.5]) {
        g.flags['mp:GER'] = left;
        expect(raisable(g, 'GER', divs), `${divs} of ${left}`).toBe(Math.floor((left + 1e-9) / Math.max(0.1, divs)));
      }
    }
    g.flags['mp:GER'] = 0.097;
    expect(raisable(g, 'GER', 0.09)).toBe(0);
  });
});
