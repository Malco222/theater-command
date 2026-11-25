// True reach under the far switch (audit item 41; the design director's ruling of 13:58Z, 25 November 2026: "the rules
// builder lays it under the far switch first"; src/campaign/game.ts trueReach). With the far side on the rule needs no
// flag and no Units decide. The old chart's readings are in tests/true-reach-r2.test.ts. Seas are closed by real
// fleets; no war is played. Drawn under TC_SUPPLY_CHECK.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; (globalThis as any).TC_SUPPLY_CHECK = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, atWar, computeSupply, createGame, declareWar, trueReach } from '../src/campaign/game';
import { SeaPicture, openSeas, zoneOf } from '../src/campaign/navy';
import { besieged, fedBySea, feeders, feedingSeas, garrison, garrisonFed, garrisonOf, monthsShut, ringClosed, siegeMonth, siegeView, storesOf } from '../src/campaign/siege';
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

  it('starves a besieged port across the water whose own sea is open to its holder while every way from the holder\'s home ports is shut: Britain\'s Malta', () => {
    // The far side's lanes set out from the home ports the roads join to the capital (navy.ts seaStarts, homeward), so
    // the town is fed only by the whole voyage there (siege.ts fedBySea; the design director's 13:58Z, 25 November
    // 2026: "paper reach is not reach"). Italy at war with Britain, one Italian fleet across the Western Mediterranean
    // and two across the Eastern; Britain's fleets in the North Atlantic. Malta's harbour lies inside its walls.
    const g = startOpen(createGame(1, null));
    declareWar(g, 'ITA', 'UK');
    g.fleets.filter((f) => f.nat === 'ITA').forEach((f, i) => { f.zone = zoneOf(g, i === 0 ? 'wmed' : 'emed'); f.stance = 'patrol'; });
    for (const f of g.fleets) if (f.nat === 'UK') { f.zone = zoneOf(g, 'natl'); f.stance = 'patrol'; }
    const r = g.byName['Malta'];
    const c = addCorps(g, 'UK', 'Malta test', 'inf', 1, 'Malta', {});
    expect(garrison(g, c)).toBe(true);
    const s = addCorps(g, 'ITA', 'Besieger test', 'inf', 1, 'Rome', {});
    g.regions[s.reg].units.splice(g.regions[s.reg].units.indexOf(s), 1);
    s.reg = r.id; r.units.push(s);
    g.flags[`siege:${r.id}`] = g.turn;
    g.supplyDirty = true;
    computeSupply(g);
    const pic = new SeaPicture(g), central = zoneOf(g, 'central');
    expect([trueReach(g), r.ctrl, r.owner, r.seas, besieged(g, r), ringClosed(g, r), garrisonOf(r).map((x) => x.nat), feeders(g, r)]).toEqual([true, 'UK', 'UK', [central], true, true, ['UK'], ['UK']]);
    // The paper: the sea off the harbour is open to Britain, and her ships could sail it from Malta's own quay (the
    // old chart's form of the walk, from every port she owns and holds); from her home ports they cannot reach it.
    expect(pic.closed(central, 'UK')).toBe(false);
    expect([openSeas(g, 'UK', pic)[central], openSeas(g, 'UK', pic, false)[central]]).toEqual([0, 1]);
    // The months of the siege, each opening Britain's month (siegeMonth) and drawing the picture: no ship comes in,
    // the stores are eaten, and then the British corps inside starves.
    const stores = storesOf(g, r);
    expect(stores).toBeGreaterThan(0);
    for (let k = 1; k <= stores + 1; k++) {
      g.turn++;
      siegeMonth(g, 'UK');
      computeSupply(g);
      const eating = k <= stores;
      expect([k, monthsShut(g, r), fedBySea(g, r), feedingSeas(g, r), garrisonFed(g, r), c.supplied, siegeView(g, r.id)!.lines.some((l) => l.startsWith('Ships still reach the harbour'))]).toEqual([k, k, false, [], eating, eating, false]);
    }
  });
});
