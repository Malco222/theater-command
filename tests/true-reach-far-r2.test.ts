// True reach under the far switch (audit item 41; the design director's ruling of 13:58Z, 25 November 2026: "the rules
// builder lays it under the far switch first"; src/campaign/game.ts trueReach). With the far side on the rule needs no
// flag and no Units decide. The old chart's readings are in tests/true-reach-r2.test.ts. Seas are closed by real
// fleets; no war is played. Drawn under TC_SUPPLY_CHECK.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; (globalThis as any).TC_SUPPLY_CHECK = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, atWar, computeSupply, createGame, trueReach } from '../src/campaign/game';
import { SeaPicture, openSeas } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { unitsDecide } from '../src/campaign/units';
import { farOn } from '../src/campaign/world';

describe('true reach under the far switch', () => {
  it('is on with the far side and no flag: Italy\'s Rhodes does not feed a German corps through a sea Britain shuts to Germany', () => {
    const g = startOpen(createGame(1, null));
    expect([farOn(g), unitsDecide(g), g.flags.trueReach, trueReach(g)]).toEqual([true, false, undefined, true]);
    const r = g.byName['Rhodes'];
    expect([r.ctrl, r.overseas, r.theatre, r.seas.length, atWar(g, 'ITA', 'UK')]).toEqual(['ITA', true, -1, 1, false]);
    const sea = r.seas[0];
    for (const f of g.fleets) if (f.nat === 'UK') { f.zone = sea; f.stance = 'patrol'; }
    const c = addCorps(g, 'GER', 'Rhodes test', 'inf', 1, 'Rhodes', {});
    computeSupply(g);
    const pic = new SeaPicture(g);
    // The paper: the sea is shut to Germany and open to Italy, whose own lanes reach it; Germany's do not, and Italy's
    // ships carrying Germany's stores do not either.
    expect([pic.closed(sea, 'GER'), pic.closed(sea, 'ITA')]).toEqual([true, false]);
    expect([openSeas(g, 'GER', pic)[sea], openSeas(g, 'ITA', pic)[sea], openSeas(g, 'ITA', pic, undefined, undefined, undefined, 'GER')[sea]]).toEqual([0, 1, 0]);
    expect([!!g.supply.GER[r.id], c.supplied]).toEqual([false, false]);
    // Britain's fleet home again: the sea is open to Germany and Italy feeds the corps.
    for (const f of g.fleets) if (f.nat === 'UK') { f.zone = f.home; f.stance = 'port'; }
    computeSupply(g);
    expect([!!g.supply.GER[r.id], c.supplied]).toEqual([true, true]);
  });
});
