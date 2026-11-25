// A nation counts in a sea's read only against those it is at war with (the design director's ruling of 03:23Z,
// 22 November 2026; src/campaign/navy.ts, besideIn).
//
// A sea is closed to a country when its enemies there are at least 15 strong and twice its own side. Its own side was
// every friend's fleets and aircraft in the sea, whoever those friends were at war with. In one war (seed 19, January
// 1942) the Honshu Coast read 34.5 on America's side: seven Soviet bomber divisions at Vladivostok and Khabarovsk,
// with the Soviet Union America's ally against Germany and at peace with Japan. Bombers that would not fly against
// Japan kept Japan's own coast open to American supply ships.
//
// The form, on the far side alone and for every country, with no new number: beside a country in a sea count itself,
// and those of its friends who are at war with one of its enemies standing in that sea's read (a fleet at sea in it,
// or aircraft counted over it). With none of its enemies there nothing is against it, the read closes nothing, and
// its friends count as before. The picture the supply table reads says the same. The state is set by hand on the
// first day's chart; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { allied, atWar, createGame, declareWar } from '../src/campaign/game';
import { NAVY, SeaPicture, airOver, besideIn, closedTo, quality, seaSide, zoneOf } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { groupOverSea, seaKm } from '../src/campaign/air';
import { skiesOver } from '../src/campaign/pacific';
import type { Fleet, Game } from '../src/campaign/types';

/** January 1942 on the first day's chart: Japan at war with America, Britain, the Dutch and the dominions; Germany with the Soviet Union and America. */
const war = (): Game => {
  const g = startOpen(createGame(3, null));
  for (const e of ['USA', 'UK', 'NED', 'AUS', 'NZL']) if (!atWar(g, 'JAP', e)) declareWar(g, 'JAP', e);
  for (const e of ['SOV', 'USA']) if (!atWar(g, 'GER', e)) declareWar(g, 'GER', e);
  g.turn = 30;
  return g;
};
const fifth = (g: Game): Fleet => g.fleets.find((f) => f.nat === 'JAP' && f.name === '5th Fleet')!;

describe('a nation counts in a sea\'s read only against those it is at war with (the director\'s ruling of 03:23Z, 22 November 2026)', () => {
  it('the Soviet bombers at Vladivostok are not America\'s own over the Honshu Coast while the Soviet Union is at peace with Japan', () => {
    const g = war();
    const honshu = zoneOf(g, 'honshu');
    expect([allied(g, 'USA', 'SOV'), atWar(g, 'SOV', 'JAP'), atWar(g, 'USA', 'JAP')]).toEqual([true, false, true]);
    // Japan's own aircraft are counted over her coast: an enemy of America's stands in the sea's read.
    expect(seaSide(g, honshu, 'USA', true).air).toBeGreaterThan(0);
    // By the old read America's side there is the Soviet bombers, and nothing else: she has nothing in the sea.
    const was = seaSide(g, honshu, 'USA', false, false);
    expect(was.ships).toBe(0);
    expect(was.air).toBeGreaterThan(30);
    expect(seaSide(g, honshu, 'USA', false)).toEqual({ ships: 0, air: 0, total: 0 });
    expect(airOver(g, honshu, 'USA')).toBe(0);
    // Japan's own read of her coast is as it was: her enemies there are those she is at war with, as they always were.
    expect(seaSide(g, honshu, 'JAP', true)).toEqual(seaSide(g, honshu, 'JAP', true, false));
    expect(seaSide(g, honshu, 'JAP', false)).toEqual(seaSide(g, honshu, 'JAP', false, false));
    // And the Soviet Union's own read counts its own bombers, at peace or not.
    expect(seaSide(g, honshu, 'SOV', false).air).toBeGreaterThanOrEqual(was.air);
  });

  it('so Japan closes her own coast to American supply ships with what would not have closed it', () => {
    const g = war();
    const honshu = zoneOf(g, 'honshu');
    const f = fifth(g);
    expect([g.seas[f.zone].key, f.stance]).toEqual(['honshu', 'port']);
    // (By hand: the 1st Air Division's 200 bombers fly from Nagoya, on this coast. From Hsinking, where the first
    // day's chart has them, the coast's nearest shore is Sapporo at 1,295 km on the globe, outside the 1,048 Japan's
    // bombers reach on the far side; the map's own line had it 1,149 and counted them here.)
    const division = g.airFleets.find((a) => a.nat === 'JAP' && a.name === '1st Air Division')!;
    expect(g.regions[division.base].name).toBe('Hsinking');
    division.base = g.byName['Nagoya'].id;
    expect(closedTo(g, honshu, 'USA')).toBe(false);
    f.stance = 'patrol';
    const e = seaSide(g, honshu, 'USA', true).total, was = seaSide(g, honshu, 'USA', false, false).total;
    // At least the 15 that closes a sea, and under twice the old read of America's side.
    expect(e).toBeGreaterThanOrEqual(NAVY.closed.min);
    expect(e).toBeLessThan(NAVY.closed.ratio * was);
    expect(closedTo(g, honshu, 'USA')).toBe(true);
    expect(new SeaPicture(g).closed(honshu, 'USA')).toBe(true);
    // The Soviet Union at war with Japan: its bombers are against her, and count beside America.
    declareWar(g, 'SOV', 'JAP');
    expect(seaSide(g, honshu, 'USA', false).total).toBeCloseTo(was, 9);
    expect(closedTo(g, honshu, 'USA')).toBe(false);
    expect(new SeaPicture(g).closed(honshu, 'USA')).toBe(false);
  });

  it('and with that division at Hsinking, where the first day\'s chart has it, the coast is out of its reach: its bombers are not counted over it (the chart\'s kilometre)', () => {
    const g = war();
    const honshu = zoneOf(g, 'honshu');
    const f = fifth(g);
    const division = g.airFleets.find((a) => a.nat === 'JAP' && a.name === '1st Air Division')!;
    expect(g.regions[division.base].name).toBe('Hsinking');
    // The coast's nearest shore to Hsinking is Sapporo, 1,295 km on the globe: outside the 1,048 Japan's bombers reach
    // on the far side (the map's own line had it 1,149). Bombers in Manchuria are not counted over this coast.
    expect(Math.round(seaKm(g, division.base, honshu))).toBe(1295);
    expect(groupOverSea(g, division, honshu, skiesOver(g, honshu)!)).toBe(0);
    f.stance = 'patrol';
    // Japan's read of her coast is the fleet's ten ships' worth at Japan's ship quality and the 150 aircraft at Tokyo,
    // 5.625: nothing of the division's 200 bombers. (At the old default of 0.7 a navy with no row fought at, that was
    // 7 and 12.625, under the 15 that closes a sea. The design director's form B1 of 25 November gave Japan's navy a
    // row of its own, so the fleet is worth more, and whether the coast closes is the read's own rule on the sums.)
    const e = seaSide(g, honshu, 'USA', true);
    expect(e.ships).toBeCloseTo(10 * quality(g, 'JAP'), 9);
    expect(e.air).toBeCloseTo(5.625, 9);
    expect(e.total).toBeCloseTo(e.ships + e.air, 9);
    const closes = e.total >= NAVY.closed.min && e.total >= NAVY.closed.ratio * seaSide(g, honshu, 'USA', false).total;
    expect(closedTo(g, honshu, 'USA')).toBe(closes);
    expect(new SeaPicture(g).closed(honshu, 'USA')).toBe(closes);
  });

  it('with none of its enemies in a sea nothing is against it, and its friends count as before', () => {
    const g = war();
    for (const z of g.seas) {
      if (seaSide(g, z.id, 'USA', true).total > 0) continue;
      expect(besideIn(g, z.id, 'USA'), z.key).toBeNull();
      expect(seaSide(g, z.id, 'USA', false), z.key).toEqual(seaSide(g, z.id, 'USA', false, false));
    }
  });

  it('a friend at war with the enemy in the sea counts as before: Britain and the Dutch beside America against Japan', () => {
    const g = war();
    for (const key of ['java', 'natuna', 'malacca', 'schina']) {
      const z = zoneOf(g, key);
      const beside = besideIn(g, z, 'USA');
      if (beside) expect(['USA', 'UK', 'NED', 'AUS', 'SOV'].map((x) => beside(x)), key).toEqual([true, true, true, true, false]);
      expect(seaSide(g, z, 'USA', false), key).toEqual(seaSide(g, z, 'USA', false, false));
    }
  });

  it('the picture the supply table reads closes the same seas, for every country and every sea', () => {
    const g = war();
    fifth(g).stance = 'patrol';
    const pic = new SeaPicture(g);
    let differ = 0;
    for (const n of Object.values(g.nations)) {
      for (const z of g.seas) {
        expect(pic.closed(z.id, n.id), `${n.id} ${z.key}`).toBe(closedTo(g, z.id, n.id));
        if (seaSide(g, z.id, n.id, false).total !== seaSide(g, z.id, n.id, false, false).total) differ++;
      }
    }
    // The form reads differently somewhere on this chart: the check above is not of two old reads.
    expect(differ).toBeGreaterThan(0);
  });
});
