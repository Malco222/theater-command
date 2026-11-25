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
import { ferryRoute, legCovered, pointLeg, realKm, setFerry } from '../src/campaign/ferry';
import { airAct } from '../src/campaign/airpages';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
import { loadGame, saveGame } from '../src/campaign/save';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { AirFleet, Game, Region } from '../src/campaign/types';
import { globeDist, mapDist, nearX } from '../src/campaign/wrap';

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

// One field, one distance (the review of the air follow-on, finding 1). Where the nearest point of a leg is one of its
// ends, legKmAt reads km() to that end whatever chart it is on; only a point inside the leg is measured by the leg's own
// ruler (the globe's where an end or the enemy's place is of the far side).
describe('a leg never reads one of its ends differently from km()', () => {
  /** The leg as legKmAt finds its nearest point on the chart: the share t along it, 0 at `from` and 1 at `to`. */
  const along = (g: Game, e: Region, a: Region, b: Region): number => {
    const bx = nearX(g, b.x, a.x), px = nearX(g, e.x, a.x), dx = bx - a.x, dy = b.y - a.y, L = dx * dx + dy * dy;
    return L > 0 ? Math.max(0, Math.min(1, ((px - a.x) * dx + (e.y - a.y) * dy) / L)) : 0;
  };
  const read = (g: Game, e: string, a: string, b: string) => {
    const E = at(g, e), A = at(g, a), B = at(g, b);
    const p = { x: E.x, y: E.y, region: E.id, sea: -1 };
    const f = { ...g.airFleets[0], base: E.id, ship: undefined } as AirFleet;
    const leg = legKmAt(g, p, A.id, B.id);
    expect(legKm(g, f, A.id, B.id)).toBe(leg);
    expect(pointLeg(g, p, A.id, B.id)).toBe(leg);
    return { E, A, B, t: along(g, E, A, B), leg };
  };

  it('the leg that crosses from the old chart to the far side: Moscow against Kirov to Yaroslavl reads km() to Yaroslavl', () => {
    const g = world();
    const r = read(g, 'Moscow', 'Kirov', 'Yaroslavl');
    // The nearest point is the leg's old-chart end, Yaroslavl, and both Moscow and Yaroslavl are of the old chart: km()
    // reads the map there, 259.688 km; the leg read the globe's 248.763 before this mend.
    expect(r.t).toBe(1);
    expect(r.leg).toBe(km(g, r.E.id, r.B.id));
    expect(r.leg).toBeCloseTo(259.688, 3);
    // Just inside that end the leg's own ruler (the globe's, Kirov being of the far side) reads about 248.763: the step
    // between the two rulers, about 10.9 km, is the far switch's known seam (wrap.ts), now at the end of the leg.
    const eps = 1e-6, qx = nearX(g, r.B.x, r.A.x) + eps * (r.A.x - nearX(g, r.B.x, r.A.x)), qy = r.B.y + eps * (r.A.y - r.B.y);
    const inside = globeDist(g, nearX(g, r.E.x, qx), r.E.y, qx, qy) * U;
    expect(inside).toBeCloseTo(248.763, 2);
    expect(r.leg - inside).toBeGreaterThan(10.9);
    expect(r.leg - inside).toBeLessThan(11);
    // Gorky against the same leg: its nearest point lies inside the leg, measured on the globe as before (197.031).
    const gk = read(g, 'Gorky', 'Kirov', 'Yaroslavl');
    expect(gk.t).toBeGreaterThan(0);
    expect(gk.t).toBeLessThan(1);
    expect(gk.leg).toBeCloseTo(197.031, 3);
  });

  it('a leg over the map\'s edge: Midway against Pearl Harbor to San Francisco reads km() to Pearl Harbor', () => {
    const g = world();
    const r = read(g, 'Midway', 'Pearl Harbor', 'San Francisco');
    expect(r.t).toBe(0);
    expect(r.leg).toBe(km(g, r.E.id, r.A.id));
    expect(r.leg).toBeCloseTo(2084.56, 2);
    // And the leg's own length, over the edge, is the globe's (km() from one end to the other).
    expect(legKmAt(g, { x: r.A.x, y: r.A.y, region: r.A.id, sea: -1 }, r.B.id, r.B.id)).toBe(km(g, r.A.id, r.B.id));
  });

  it('every case: where the nearest point is an end, the leg reads km() to it, to the last digit; far chart and old', () => {
    const g = world();
    const cases: [string, string, string][] = [
      ['Moscow', 'Kirov', 'Yaroslavl'], ['Gorky', 'Kirov', 'Yaroslavl'], ['Midway', 'Pearl Harbor', 'San Francisco'],
      ['Rabaul', 'Lae', 'Port Moresby'], ['Rabaul', 'Port Moresby', 'Port Moresby'], ['Truk', 'Guam', 'Saipan'],
      ['Manila', 'Formosa', 'Hong Kong'], ['Yaroslavl', 'Kirov', 'Kirov'], ['Kirov', 'Yaroslavl', 'Moscow'],
      ['Berlin', 'Paris', 'London'], ['Rome', 'Marseille', 'Lyon'], ['Warsaw', 'Kiev', 'Moscow'], ['Ruhr', 'Brest', 'Hamburg'],
      ['Stalingrad', 'Rostov', 'Kharkov'], ['Malta', 'Tunis', 'Tripoli'], ['Yaroslavl', 'Gorky', 'Moscow'],
    ];
    let ends = 0;
    for (const [e, a, b] of cases) {
      for (const [x, y] of [[a, b], [b, a]]) {
        const r = read(g, e, x, y);
        if (r.t <= 0) { expect(r.leg).toBe(km(g, r.E.id, r.A.id)); ends++; }
        else if (r.t >= 1) { expect(r.leg).toBe(km(g, r.E.id, r.B.id)); ends++; }
        // Inside the leg it is never farther than km() to the nearer end by more than the chart's own search allows
        // (the nearest point is found on the chart, not on the globe: Manila against Formosa to Hong Kong reads 904.052,
        // km() to Formosa 902.923, the review's note).
        else expect(r.leg).toBeLessThanOrEqual(Math.min(km(g, r.E.id, r.A.id), km(g, r.E.id, r.B.id)) + 1.2);
      }
    }
    expect(ends).toBeGreaterThanOrEqual(20);
  });
});

// A ferry hop on the far chart, seen in a played war (the review of the air follow-on, note 5: the proofs never saw one).
// The Soviet player sends the 1st Fighter Aviation Division from Leningrad to Chita through the bridge's own door, as
// the page does (airAct 'ferry:<id>:<field>'), and the months are stepped as a player steps them. The way crosses from
// the old chart to the far side at Kirov and flies on across Siberia.
describe('a group ordered across the far side in a played war', () => {
  it('flies the card\'s hops in order, each as long as the card said, and arrives when the card said', () => {
    let g = startOpen(createGame(2, 'SOV'));
    setFerry(g, true);
    let guard = 0;
    while (!humanUp(g) && guard++ < 400) stepAI(g);
    beginHuman(g);
    g = loadGame(saveGame(g))!;
    const f = () => g.airFleets.find((x) => x.name === '1st Fighter Aviation Division')!;
    expect(g.regions[f().base].name).toBe('Leningrad');
    const to = at(g, 'Chita').id;
    const card = ferryRoute(g, f(), to)!;
    expect(card.how).toBe('air');
    const names = card.hops.map((h) => g.regions[h.to].name);
    expect(names).toEqual(['Kirov', 'Tyumen', 'Tomsk', 'Irkutsk', 'Chita']);
    expect(card.turns).toBe(2);
    expect(card.risky).toEqual([]);
    expect(card.words).toBe('2 turns by Kirov, Tyumen, Tomsk and Irkutsk. No enemy fighters known on the way.');
    // Each leg's length on the card is the great circle between its fields, and so is what it flies.
    for (const h of card.hops) expect(h.km).toBe(realKm(g, h.from, h.to));
    const reports = g.airReports.length;
    const res = airAct(g, `ferry:${f().id}:${to}`)!;
    expect(res.ok).toBe(true);
    // This month's four hops at once: it stands at Irkutsk with Chita to go.
    expect(g.regions[f().base].name).toBe('Irkutsk');
    expect(f().way!.stops.map((x) => g.regions[x].name)).toEqual(['Chita']);
    endHuman(g);
    guard = 0;
    while (!humanUp(g) && guard++ < 400) stepAI(g);
    expect(g.regions[f().base].name).toBe('Irkutsk');
    beginHuman(g);
    // The fifth hop as the player's next month opens, and it is there: no enemy came up on the way (nobody is at war with it).
    expect(g.regions[f().base].name).toBe('Chita');
    expect(f().way).toBeUndefined();
    expect(g.airReports.slice(reports).some((r) => r.ferry !== undefined)).toBe(false);
  });
});
