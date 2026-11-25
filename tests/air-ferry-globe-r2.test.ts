// One ruler for the ferry lane on the far chart (the join's read of r2-head-1125k, fault 1; joins/r2-head-1125k-READ.md).
//
// On this line km() reads the globe wherever either province is of the far side (wrap.ts chartDist), and so do kmTo,
// fightersOver and every raid. The ferry lane's legKm (air.ts), its copy pointLeg (ferry.ts, the route's risk warning)
// and legCovered (ferry.ts, the page's after-the-fact words) measured a leg on the flat chart. Now all three read
// legKmAt: the nearest point of the leg in chart units as before, and the distance to it on the globe where an end of
// the leg or the enemy's own place is of the far side; on the old chart the map's straight line, to the last digit.
// Behind the ferry switch (stage.json ferry / TC_FERRY=1), off as shipped; the far chart behind TC_FAR.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { AIR, airRange, assessAir, groupAt, km, legKm, legKmAt } from '../src/campaign/air';
import { legCovered, pointLeg, setFerry } from '../src/campaign/ferry';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { AirFleet, Game, Region } from '../src/campaign/types';
import { mapDist } from '../src/campaign/wrap';

const rows = AIR.fleets.length;
afterEach(() => { AIR.fleets.splice(0, AIR.fleets.length - rows); });

function world(): Game {
  const g = startOpen(createGame(3, null));
  setFerry(g, true);
  return g;
}
const at = (g: Game, name: string): Region => { const r = g.byName[name]; if (!r) throw new Error(`no province called ${name}`); return r; };
/** One enemy fighter group put at a field, the only one of its side able to come up, with its fighter range set (setup). */
function enemyAt(g: Game, nat: string, foe: string, field: string, range: number): AirFleet {
  setWar(g, nat, foe);
  const own = g.airFleets.filter((f) => f.nat === nat && f.ftr > 0 && f.ship === undefined);
  const e = own[0];
  for (const f of g.airFleets) if (f.nat === nat && f !== e) f.mission = 'rest';
  e.base = at(g, field).id; e.mission = 'support'; e.close = false;
  // (A row of its own in air.json with this fighter range: reach() takes the largest of it and the dated rows.)
  AIR.fleets.unshift({ nat, name: e.parent ?? e.name, ftrRange: range } as (typeof AIR.fleets)[number]);
  expect(airRange(e, g).ftr).toBe(range);
  return e;
}
/** One of the other side's fighter groups, on its way between two fields. */
const ferried = (g: Game, nat: string): AirFleet => g.airFleets.find((f) => f.nat === nat && f.ftr > 0 && f.ship === undefined)!;
const U = AIR.kmPerUnit;
const flat = (g: Game, a: Region, b: Region): number => mapDist(g, a.x, a.y, b.x, b.y) * U;

describe('the ferry lane measures a leg as km() does', () => {
  it('Rabaul to Port Moresby reads the globe (about 800 km, not about 680) through legKm, pointLeg and legCovered', () => {
    const g = world();
    const R = at(g, 'Rabaul'), P = at(g, 'Port Moresby');
    expect(Math.round(flat(g, R, P))).toBe(677);
    expect(Math.round(km(g, R.id, P.id))).toBe(793);
    // An enemy fighter group at Rabaul, with a reach between the two figures (740 km).
    const e = enemyAt(g, 'JAP', 'USA', 'Rabaul', 740);
    // The leg ends at Port Moresby (from there to there: the field itself).
    expect(legKm(g, e, P.id, P.id)).toBeCloseTo(km(g, R.id, P.id), 9);
    expect(pointLeg(g, groupAt(g, e), P.id, P.id)).toBeCloseTo(km(g, R.id, P.id), 9);
    expect(legKmAt(g, groupAt(g, e), P.id, P.id)).toBeCloseTo(km(g, R.id, P.id), 9);
    // Beyond its reach on the globe, though inside it on the flat chart: the page's words say no one came up.
    const us = ferried(g, 'USA');
    expect(legCovered(g, us, P.id, P.id)).toBe(false);
  });

  it('the risk warning and the interception that follows read the same reach, the globe\'s, both ways round', () => {
    // In the tropics the flat chart reads short: in reach on the map, out of it on the globe.
    {
      const g = world();
      const P = at(g, 'Port Moresby');
      const e = enemyAt(g, 'JAP', 'USA', 'Rabaul', 740);
      const us = ferried(g, 'USA');
      const warned = pointLeg(g, groupAt(g, e), P.id, P.id) <= airRange(e, g).ftr;
      const a = assessAir(g, [us], 'superiority', P.id, undefined, { from: P.id, to: P.id });
      const met = a.def.some((x) => x.f.id === e.id);
      expect(flat(g, at(g, 'Rabaul'), P)).toBeLessThan(740);
      expect([warned, met, legCovered(g, us, P.id, P.id)]).toEqual([false, false, false]);
    }
    // In the north it reads long: out of reach on the map, in it on the globe (Yaroslavl, of the old chart, to Kirov,
    // of the far side: 725 km on the map, 598 on the globe).
    {
      const g = world();
      const Y = at(g, 'Yaroslavl'), K = at(g, 'Kirov');
      expect(Math.round(flat(g, Y, K))).toBe(725);
      expect(Math.round(km(g, Y.id, K.id))).toBe(598);
      const e = enemyAt(g, 'GER', 'SOV', 'Yaroslavl', 650);
      const sov = ferried(g, 'SOV');
      const warned = pointLeg(g, groupAt(g, e), K.id, K.id) <= airRange(e, g).ftr;
      const a = assessAir(g, [sov], 'superiority', K.id, undefined, { from: K.id, to: K.id });
      const met = a.def.some((x) => x.f.id === e.id);
      expect([warned, met, legCovered(g, sov, K.id, K.id)]).toEqual([true, true, true]);
    }
  });

  it('a leg of the old chart is measured as before, to the last digit', () => {
    const g = world();
    // (The three figures were taken from legKm on the head before this change, 93cdfb78 with the two ruled commits.)
    const legs: [string, string, string, number][] = [
      ['Berlin', 'Paris', 'London', 888.7421301142418],
      ['Rome', 'Marseille', 'Lyon', 542.6771486657605],
      ['Warsaw', 'Kiev', 'Moscow', 720.4979155223144],
    ];
    for (const [e, a, b, was] of legs) {
      const f = { ...g.airFleets[0], base: at(g, e).id, ship: undefined } as AirFleet;
      expect(legKm(g, f, at(g, a).id, at(g, b).id)).toBe(was);
      expect(pointLeg(g, groupAt(g, f), at(g, a).id, at(g, b).id)).toBe(was);
    }
  });
});
