// China's rifles, stage B2, test 1 of tests/far-rifles-taps.test.ts, kept in a file of its own because the far switch
// is hoisted per file: with the switch off no nation carries a rifles row, so every tap returns today's figure, no
// order draws or leaves a flag of the store's, and every price is the list price.
import { describe, expect, it } from 'vitest';
import { RIFLES, armOrder, binds, canArm, carries, carve, carved, noRifles, riflesFor } from '../src/campaign/rifles';
import { cancelBuild, corpsCost, corpsKinds, corpsList, orderCorps, specOf } from '../src/campaign/economy';
import { offers } from '../src/campaign/arsenal';
import { createGame } from '../src/campaign/game';
import { levyMen, menLeft } from '../src/campaign/manpower';
import { startOpen } from '../src/campaign/staff';

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
  });
});
