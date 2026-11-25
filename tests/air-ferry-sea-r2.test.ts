// One field, one distance for a carrier wing at sea (the design director, 25 November 2026 14:59Z; the review of the
// air follow-on's second round, note 3). The rules' own distance from a carrier wing at sea to a field is kmTo's:
// seaKm, the wing being anywhere in its sea (a field on its shore at no distance). The ferry lane's legKmAt measured a
// leg's end from the sea's centre instead (North Atlantic against the Highlands 444.0 km). Now an end reads seaKm, and
// a point inside a leg is capped at it. On the old chart (no TC_FAR here). Behind the ferry switch, off as shipped.
import { describe, expect, it } from 'vitest';
import { AIR, airRange, airfieldsHold, assessAir, carrierOf, cover, groupAt, km, kmTo, legKm, legKmAt, seaKm } from '../src/campaign/air';
import { ferryRoute, legCovered, pointLeg, realKm, setFerry } from '../src/campaign/ferry';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { AirFleet, Game } from '../src/campaign/types';
import { globeOn, mapDist } from '../src/campaign/wrap';

const U = AIR.kmPerUnit;
const id = (g: Game, n: string): number => { const r = g.byName[n]; if (!r) throw new Error(`no province called ${n}`); return r.id; };
const sea = (g: Game, n: string): number => { const z = g.seas.find((s) => s.name === n); if (!z) throw new Error(`no sea called ${n}`); return z.id; };
/** The Home Fleet's carrier fighters, their fleet put at sea in the named sea (setup). */
function wingIn(g: Game, z: number): AirFleet {
  const w = g.airFleets.find((f) => f.name === 'Home Fleet carrier fighters')!;
  const c = carrierOf(g, w)!;
  c.zone = z; c.stance = 'patrol';
  const p = groupAt(g, w);
  expect([p.region, p.sea]).toEqual([-1, z]);
  return w;
}
/** The sea's centre to a field, as the leg's end read it before this mend (wrap.ts seaDist; the map's line on this chart). */
const centre = (g: Game, z: number, r: number): number => mapDist(g, g.seas[z].x, g.seas[z].y, g.regions[r].x, g.regions[r].y) * U;

describe('a carrier wing at sea reads a leg\'s end as kmTo reads that field', () => {
  it('the review\'s four cases: a field on the shore of the wing\'s sea is at no distance, not the sea centre\'s', () => {
    const g = startOpen(createGame(3, null));
    setFerry(g, true);
    expect(globeOn(g)).toBe(false);
    const cases: [string, string, number][] = [
      ['North Atlantic', 'Highlands', 444.0], ['Western Approaches', 'Cornwall', 357.4], ['Irish Sea', 'West Country', 325.3], ['Bay of Biscay', 'Rennes', 339.3],
    ];
    for (const [s, f, was] of cases) {
      const z = sea(g, s), r = id(g, f), w = wingIn(g, z);
      // Before: from the sea's centre.
      expect(+centre(g, z, r).toFixed(1)).toBe(was);
      // Now: seaKm, the same figure kmTo gives that wing for that field, through every road to the leg.
      expect(seaKm(g, r, z)).toBe(0);
      expect(kmTo(g, w, r)).toBe(0);
      expect(legKm(g, w, r, r)).toBe(kmTo(g, w, r));
      expect(pointLeg(g, groupAt(g, w), r, r)).toBe(kmTo(g, w, r));
      expect(legKmAt(g, groupAt(g, w), r, r)).toBe(seaKm(g, r, z));
    }
  });

  it('a field inland of the sea reads seaKm (the nearest shore), and a point inside a leg never reads farther than either end', () => {
    const g = startOpen(createGame(3, null));
    setFerry(g, true);
    const z = sea(g, 'Irish Sea'), w = wingIn(g, z);
    for (const f of ['Hampshire', 'Midlands', 'London', 'Yorkshire']) {
      const r = id(g, f);
      expect(seaKm(g, r, z)).toBeGreaterThan(0);
      expect(legKm(g, w, r, r)).toBe(kmTo(g, w, r));
    }
    // Every leg between two fields within a month's flight: at an end, kmTo to it; inside, at most kmTo to each end.
    const fields = g.regions.filter((r) => airfieldsHold(g, r.id) > 0).map((r) => r.id);
    let legs = 0;
    for (const a of fields) for (const b of fields) {
      if (a === b || realKm(g, a, b) > 1000 || Math.min(kmTo(g, w, a), kmTo(g, w, b)) > 1000) continue;
      const d = legKm(g, w, a, b);
      expect(d).toBeLessThanOrEqual(Math.min(kmTo(g, w, a), kmTo(g, w, b)));
      legs++;
    }
    expect(legs).toBeGreaterThan(100);
  });

  it('the warning, the interception and legCovered agree with cover(kmTo) for that wing, at a reach between the old reading and the new', () => {
    // The Home Fleet's carrier fighters (reach 350 km) at sea in the North Sea; a German group flies the leg Bremen to
    // Hamburg, both on the North Sea's shore. Before: 528.7 and 571.2 km from the sea's centre, out of reach. Now 0.
    const g = startOpen(createGame(3, 'GER'));
    setFerry(g, true);
    setWar(g, 'UK', 'GER');
    const z = sea(g, 'North Sea'), w = wingIn(g, z);
    // The wing is the only British fighter group left to come up (setup).
    for (const f of g.airFleets) if (f.nat === 'UK' && f !== w) f.mission = 'rest';
    w.mission = 'support'; w.close = false;
    const R = airRange(w, g).ftr;
    expect(R).toBe(350);
    const B = id(g, 'Bremen'), H = id(g, 'Hamburg');
    expect(centre(g, z, B)).toBeGreaterThan(R);
    expect(centre(g, z, H)).toBeGreaterThan(R);
    expect([seaKm(g, B, z), seaKm(g, H, z)]).toEqual([0, 0]);
    const ger = g.airFleets.find((f) => f.nat === 'GER' && f.ftr > 0 && f.ship === undefined)!;
    ger.base = B;
    // cover(kmTo): the rules' own reach of that wing over Hamburg.
    const covered = cover(kmTo(g, w, H), R) > 0;
    expect(covered).toBe(true);
    // The warning on the order's card: the hop to Hamburg is flagged (the card's own ruler, and the card itself).
    expect(pointLeg(g, groupAt(g, w), B, H) <= R).toBe(covered);
    const card = ferryRoute(g, ger, H)!;
    expect(card.how).toBe('air');
    expect(card.hops.map((h) => g.regions[h.to].name)).toEqual(['Hamburg']);
    expect(card.risky).toEqual([0]);
    // The interception that follows: the wing comes up against the hop.
    const a = assessAir(g, [ger], 'superiority', H, undefined, { from: B, to: H });
    expect(a.def.some((x) => x.f.id === w.id)).toBe(covered);
    // And the page's after-the-fact words.
    expect(legCovered(g, ger, B, H)).toBe(covered);
  });
});

describe('a place in a province on the old chart reads as before', () => {
  it('over every leg within 1,500 km and every province within 2,500 km of it, the cap never binds', () => {
    const g = startOpen(createGame(3, null));
    expect(globeOn(g)).toBe(false);
    const fields = g.regions.filter((r) => airfieldsHold(g, r.id) > 0).map((r) => r.id);
    let inside = 0, bound = 0, ends = 0, endsOff = 0;
    for (const a of fields) for (const b of fields) {
      if (a === b || realKm(g, a, b) > 1500) continue;
      const A = g.regions[a], Bf = g.regions[b], dx = Bf.x - A.x, dy = Bf.y - A.y, L = dx * dx + dy * dy;
      for (const r of g.regions) {
        if (Math.min(km(g, r.id, a), km(g, r.id, b)) > 2500) continue;
        const d = legKmAt(g, { x: r.x, y: r.y, region: r.id, sea: -1 }, a, b);
        const t = ((r.x - A.x) * dx + (r.y - A.y) * dy) / L;
        if (t <= 0 || t >= 1) { ends++; if (d !== km(g, r.id, t <= 0 ? a : b)) endsOff++; continue; }
        inside++;
        // The leg's own ruler, uncapped, as it always read.
        if (d !== Math.hypot(r.x - (A.x + t * dx), r.y - (A.y + t * dy)) * U) bound++;
      }
    }
    expect(inside).toBeGreaterThan(500_000);
    expect(ends).toBeGreaterThan(1_000_000);
    expect([bound, endsOff]).toEqual([0, 0]);
  });
});

describe('a carrier wing in a sea with no shore (a known behaviour)', () => {
  // The design director, 25 November 2026 15:31Z, ruled to leave it: "A hop over open ocean can be met by a carrier in
  // that ocean, and nothing over land is in its reach, as kmTo says. Please name it with one test so it stays a known
  // behaviour." Twelve seas of the chart have no shore (Mid-Atlantic Gap among them). A wing at sea in one reads every
  // field, and so every end of a leg, as out of reach: seaKm over an empty coast is Infinity, as kmTo reads it. A point
  // inside a leg is read by the leg's own ruler from the sea's centre, a finite distance, and the cap at the ends
  // (Infinity) does not bind.
  it('every field and every leg\'s end is Infinity, as kmTo; a point inside a leg is a finite distance from the sea\'s centre', () => {
    const g = startOpen(createGame(3, null));
    setFerry(g, true);
    expect(globeOn(g)).toBe(false);
    const bare = g.seas.filter((s) => s.coast.length === 0);
    expect(bare.length).toBe(12);
    const z = sea(g, 'Mid-Atlantic Gap');
    expect(g.seas[z].coast).toEqual([]);
    const w = wingIn(g, z), at = groupAt(g, w);
    const fields = g.regions.filter((r) => airfieldsHold(g, r.id) > 0).map((r) => r.id);
    expect(fields.length).toBeGreaterThan(100);
    // Every field: out of reach, by kmTo and by a leg that ends there.
    for (const r of fields) {
      expect(seaKm(g, r, z)).toBe(Infinity);
      expect(kmTo(g, w, r)).toBe(Infinity);
      expect(legKm(g, w, r, r)).toBe(Infinity);
    }
    // Every leg between two fields within 1,500 km: where the sea's centre lies beyond an end, Infinity; where it lies
    // inside the leg, the map's straight line from the sea's centre to the leg's nearest point, finite.
    let ends = 0, inside = 0;
    for (const a of fields) for (const b of fields) {
      if (a === b || realKm(g, a, b) > 1500) continue;
      const A = g.regions[a], B = g.regions[b], dx = B.x - A.x, dy = B.y - A.y, L = dx * dx + dy * dy;
      const t = ((at.x - A.x) * dx + (at.y - A.y) * dy) / L;
      const d = legKmAt(g, at, a, b);
      if (t <= 0 || t >= 1) { ends++; expect(d).toBe(Infinity); continue; }
      inside++;
      expect(Number.isFinite(d)).toBe(true);
      expect(d).toBe(Math.hypot(at.x - (A.x + t * dx), at.y - (A.y + t * dy)) * U);
    }
    expect(ends).toBeGreaterThan(1000);
    expect(inside).toBeGreaterThan(100);
  });
});
