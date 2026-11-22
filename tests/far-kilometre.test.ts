// The chart's kilometre (item R; the design director's rulings of 04:41Z, 04:47Z and 08:55Z, 22 November 2026).
//
// The chart draws a degree of longitude everywhere as long as it is at the parallel of 50 north, so the map's straight
// line is true north and south, and east and west only at that parallel: in the tropics it reads about two-thirds of
// the distance on the globe. The rules' ranges are kilometres. So with the far side on, the distance between two
// provinces is the globe's wherever either of them is a province of the far side: the air war's kilometre (air.ts,
// km) and every search for the nearest place by a straight line. Between two provinces of the old chart (Europe, the
// Americas) the map's line is read as it always was, far on or off: the old chart's own form waits for its own gate.
// With the far side's switch off nothing reads the globe.
//
// The projection is the chart builder's, written into the chart (tools/build-geo-world.mjs, `world`): no rule carries
// a figure of it.
//
// The ranges are laid with it (the design director's rulings of 11:26Z and 11:46Z, 22 November 2026; far-air.json by
// tools/make-far-air.mjs): a type's reach is the farthest radius the record flew as routine, and its figure is that
// flight measured as the rules measure, from province centre to province centre on the globe.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterAll, describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { AIR, km, seaKm } from '../src/campaign/air';
import { FAR_FIRST, farOn } from '../src/campaign/world';
import { chartDist, chartDist2, globeDist, globeFor, globeOn, latOf, mapDist, mapDist2, ringStretch, seaDist } from '../src/campaign/wrap';
import europe from '../src/generated/world-map.json';
import americas from '../src/generated/world-americas.json';
import next from '../src/generated/world-next.json';
import cities from '../src/generated/world-cities.json';
import whole from '../src/generated/world-whole.json';
import type { Game, Region } from '../src/campaign/types';

const g: Game = startOpen(createGame(3, null));
const at = (name: string): Region => { const r = g.byName[name]; if (!r) throw new Error(`no province called ${name}`); return r; };
const U = AIR.kmPerUnit;
/** The map's straight line between two provinces, in kilometres as the old chart counts them. */
const line = (a: string, b: string): number => Math.round(mapDist(g, at(a).x, at(a).y, at(b).x, at(b).y) * U);
/** The globe's distance between them. */
const globe = (a: string, b: string): number => Math.round(globeDist(g, at(a).x, at(a).y, at(b).x, at(b).y) * U);
/** What the rules read. */
const read = (a: string, b: string): number => Math.round(km(g, at(a).id, at(b).id));

describe('the chart carries its own projection', () => {
  it('every chart the builder writes names the latitude of its top, its units to a degree and its parallel', () => {
    for (const chart of [europe, americas, next, cities, whole] as unknown as { world: Record<string, unknown> }[]) {
      expect([chart.world.north, chart.world.scale, chart.world.parallel]).toEqual([71.4, 40, 50]);
    }
    // A degree of latitude is 40 units, and the old chart's kilometre is a degree's 111.2 over them.
    expect(Math.round((111.2 / 40) * 100) / 100).toBe(U);
    expect(latOf(0)).toBe(71.4);
    expect(Math.round(latOf(at('Manila').y) * 10) / 10).toBe(14.6);
  });
});

describe("the globe's distance", () => {
  it('east and west in the tropics the map reads about two-thirds of it; north and south the two are one', () => {
    expect([line('Phnom Penh', 'Manila'), globe('Phnom Penh', 'Manila')]).toEqual([1198, 1774]);
    expect([line('Phnom Penh', 'Panay'), globe('Phnom Penh', 'Panay')]).toEqual([1258, 1918]);
    expect([line('Formosa', 'Hong Kong'), globe('Formosa', 'Hong Kong')]).toEqual([449, 644]);
    // North and south.
    expect([line('Formosa', 'Lingayen'), globe('Formosa', 'Lingayen')]).toEqual([745, 745]);
    expect([line('Truk', 'Rabaul'), globe('Truk', 'Rabaul')]).toEqual([1298, 1298]);
    // North of the parallel the map reads too much.
    expect(globe('Leningrad', 'Karelia')).toBeLessThan(line('Leningrad', 'Karelia'));
  });

  it('is the shorter way round the world, and nothing to itself', () => {
    // Hawaii lies at the chart's east end and California at its west: 3,850 km apart on the globe.
    const d = globe('Pearl Harbor', 'San Francisco');
    expect(d).toBeGreaterThan(3700);
    expect(d).toBeLessThan(4000);
    expect(globe('Manila', 'Manila')).toBe(0);
    expect(globe('Manila', 'Phnom Penh')).toBe(globe('Phnom Penh', 'Manila'));
  });
});

describe('what the rules read with the far side on', () => {
  it('the far side is on, and the chart goes all the way round', () => {
    expect([farOn(g), globeOn(g)]).toEqual([true, true]);
  });

  it("between two provinces of the far side, the globe's distance", () => {
    for (const [a, b] of [['Phnom Penh', 'Manila'], ['Formosa', 'Hong Kong'], ['Saigon', 'Kuantan'], ['Truk', 'Rabaul']]) {
      expect([at(a).id >= FAR_FIRST, at(b).id >= FAR_FIRST], `${a}, ${b}`).toEqual([true, true]);
      expect(read(a, b), `${a} to ${b}`).toBe(globe(a, b));
    }
    expect(read('Phnom Penh', 'Manila')).toBe(1774);
  });

  it('between a province of the old chart and one of the far side, the globe\'s too', () => {
    expect([at('San Francisco').id < FAR_FIRST, at('Pearl Harbor').id >= FAR_FIRST]).toEqual([true, true]);
    expect(read('San Francisco', 'Pearl Harbor')).toBe(globe('San Francisco', 'Pearl Harbor'));
    expect(read('Pearl Harbor', 'San Francisco')).toBe(globe('San Francisco', 'Pearl Harbor'));
    expect(globeFor(g, at('San Francisco'), at('Pearl Harbor'))).toBe(true);
  });

  it("between two provinces of the old chart, the map's straight line as it always was", () => {
    for (const [a, b] of [['Leningrad', 'Karelia'], ['Berlin', 'Paris'], ['London', 'Berlin'], ['New York', 'Washington'], ['San Francisco', 'New York'], ['Panama City', 'Caracas']]) {
      expect([at(a).id < FAR_FIRST, at(b).id < FAR_FIRST], `${a}, ${b}`).toEqual([true, true]);
      expect(globeFor(g, at(a), at(b)), `${a}, ${b}`).toBe(false);
      expect(read(a, b), `${a} to ${b}`).toBe(line(a, b));
      expect(chartDist(g, at(a), at(b))).toBe(mapDist(g, at(a).x, at(a).y, at(b).x, at(b).y));
      expect(chartDist2(g, at(a), at(b))).toBe(mapDist2(g, at(a).x, at(a).y, at(b).x, at(b).y));
    }
    // Leningrad's fighters, with a reach of 200, are 203 km from Karelia on the map and stay so (182 on the globe).
    expect([read('Leningrad', 'Karelia'), globe('Leningrad', 'Karelia')]).toEqual([203, 182]);
  });

  it('the first province of the far side is world.ts\'s own', () => {
    const last = g.regions[FAR_FIRST - 1], first = g.regions[FAR_FIRST];
    expect(globeFor(g, last, at('Berlin'))).toBe(false);
    expect(globeFor(g, first, at('Berlin'))).toBe(true);
    expect(globeFor(g, at('Berlin'), first)).toBe(true);
  });

  it('a search for the nearest reads the same distance, squared', () => {
    for (const [a, b] of [['Phnom Penh', 'Manila'], ['San Francisco', 'Pearl Harbor'], ['Tokyo', 'Rabaul']]) {
      expect(chartDist2(g, at(a), at(b))).toBeCloseTo(chartDist(g, at(a), at(b)) ** 2, 6);
      expect(chartDist(g, at(a), at(b))).toBe(globeDist(g, at(a).x, at(a).y, at(b).x, at(b).y));
    }
  });

  it('a base on a sea\'s shore is no way from it, and one inland as far as its nearest shore on the globe', () => {
    const z = at('Manila').seas[0];
    expect(seaKm(g, at('Manila').id, z)).toBe(0);
    const from = at('Phnom Penh');
    const want = Math.min(...g.seas[z].coast.map((c) => globeDist(g, from.x, from.y, g.regions[c].x, g.regions[c].y) * U));
    expect(seaKm(g, from.id, z)).toBeCloseTo(want, 6);
  });

  it("a harbour's distance from the middle of a sea: the globe's where the harbour or the sea is of the far side, a sea counted by its own number", () => {
    // The whole chart keeps the old chart's seas first: the Gulf of Aden is its last, the Gulf of Alaska the far side's first.
    const first = americas.seas.zones.length;
    expect([first, g.seas[first - 1].key, g.seas[first].key, g.seas.every((z, i) => z.id === i)]).toEqual([67, 'aden', 'gulfalaska', true]);
    const sea = (key: string) => g.seas.find((z) => z.key === key)!;
    const sf = at('San Francisco'), guam = at('Guam');
    const onGlobe = (r: Region, z: { x: number; y: number }): number => globeDist(g, r.x, r.y, z.x, z.y);
    const onMap = (r: Region, z: { x: number; y: number }): number => mapDist(g, r.x, r.y, z.x, z.y);
    // An old-chart harbour and an old-chart sea: the map's line. The same harbour and a far-side sea: the globe.
    expect(seaDist(g, sf, sea('california'))).toBe(onMap(sf, sea('california')));
    expect(seaDist(g, sf, sea('hawaii'))).toBe(onGlobe(sf, sea('hawaii')));
    expect(seaDist(g, sf, g.seas[first - 1])).toBe(onMap(sf, g.seas[first - 1]));
    expect(seaDist(g, sf, g.seas[first])).toBe(onGlobe(sf, g.seas[first]));
    // A far-side harbour reads the globe to any sea.
    expect(seaDist(g, guam, sea('eastsol'))).toBe(onGlobe(guam, sea('eastsol')));
    expect(seaDist(g, guam, sea('california'))).toBe(onGlobe(guam, sea('california')));
    // The Eastern Solomons from Guam and from Samoa: the map's line has Samoa the nearer, the globe Guam.
    const km2 = (r: Region, f: (r: Region, z: { x: number; y: number }) => number): number => Math.round(f(r, sea('eastsol')) * U);
    expect([km2(guam, onMap), km2(at('Samoa'), onMap), km2(guam, onGlobe), km2(at('Samoa'), onGlobe)]).toEqual([2520, 2168, 2886, 3158]);
  });
});

describe("December 1941 on the globe (the ranges as laid by the record: Japan's bombers and fighters 1,048)", () => {
  it("the record's strikes stay in reach", () => {
    // Clark Field from Formosa, Force Z from Indochina, Kota Bharu, Hong Kong.
    expect(read('Formosa', 'Manila')).toBe(903);
    expect(read('Formosa', 'Lingayen')).toBe(745);
    expect(read('Saigon', 'Kuantan')).toBe(871);
    expect(read('Saigon', 'Kota Bharu')).toBe(745);
    expect(read('Phnom Penh', 'Kota Bharu')).toBe(707);
    expect(read('Formosa', 'Hong Kong')).toBe(644);
    const japan = { bmr: AIR.nations.JAP.bmrRange, ftr: AIR.ftrReach!.JAP[0][1] };
    expect(japan).toEqual({ bmr: 1048, ftr: 1048 });
    for (const d of [read('Formosa', 'Manila'), read('Saigon', 'Kuantan'), read('Saigon', 'Kota Bharu')]) expect(d).toBeLessThanOrEqual(japan.bmr);
    // The fighters flew with the bombers to Clark Field, as the record has them, and over Hong Kong.
    for (const d of [read('Formosa', 'Manila'), read('Formosa', 'Hong Kong')]) expect(d).toBeLessThanOrEqual(japan.ftr);
  });

  it('Indochina\'s bombers no longer reach the Philippines', () => {
    // (The map's line had them inside the 1,300 km the bombers flew on the planner's paper; the globe has them
    // beyond it, and beyond the 1,048 laid since.)
    for (const [a, b] of [['Phnom Penh', 'Manila'], ['Phnom Penh', 'Panay'], ['Saigon', 'Manila']]) {
      expect(line(a, b), `${a} to ${b} on the map`).toBeLessThanOrEqual(1300);
      expect(read(a, b), `${a} to ${b}`).toBeGreaterThan(1300);
    }
  });
});

describe('the ranges as laid by the record: each reach is the flight that sets it, measured as the rules measure', () => {
  const usa = (AIR.fleets.find((r) => r.nat === 'USA' && r.name === 'Fifth Air Force') as unknown as { bmrRange: number }).bmrRange;
  const japan = AIR.nations.JAP.bmrRange;

  it("Japan's fighters and bombers: Rabaul to Guadalcanal, 1,048 km from centre to centre (the record's 1,044 is field to target)", () => {
    expect([japan, AIR.ftrReach!.JAP]).toEqual([1048, [[13, 1048]]]);
    const d = km(g, at('Rabaul').id, at('Guadalcanal').id);
    // The flight itself is in reach, and the figure is no more than the flight rounded up.
    expect(d).toBeLessThanOrEqual(japan);
    expect(d).toBeGreaterThan(japan - 1);
    // A reading, not the reach: Kendari to Darwin was flown once (19 February 1942), and lies out of routine reach.
    expect(read('Kendari', 'Darwin')).toBe(1350);
    expect(read('Kendari', 'Darwin')).toBeGreaterThan(japan);
    // Ambon to Darwin, the same day's other raid, is inside it.
    expect(read('Ambon', 'Darwin')).toBeLessThanOrEqual(japan);
  });

  it("America's bombers out here: Eniwetok to Truk, 1,243 km from centre to centre (the record's 1,228), and the long bombers' 2,550", () => {
    expect(usa).toBe(1243);
    const d = km(g, at('Eniwetok').id, at('Truk').id);
    expect(d).toBeLessThanOrEqual(usa);
    expect(d).toBeGreaterThan(usa - 1);
    for (const r of AIR.fleets.filter((x) => x.nat === 'USA' && (x as unknown as { bmrRange?: number }).bmrRange !== undefined)) {
      expect((r as unknown as { bmrRange: number }).bmrRange, r.name).toBe(/Bomber Command/.test(r.name) ? 2550 : 1243);
    }
    // The long bombers' own flights: Chengtu to Yawata (Fukuoka's ground on the chart) and Saipan to Tokyo.
    expect(read('Chengtu', 'Fukuoka')).toBeLessThanOrEqual(2550);
    expect(read('Saipan', 'Tokyo')).toBeLessThanOrEqual(2550);
  });

  it("by type, where a row carries one: Japan's navy land bombers reach Wake from Kwajalein, 1,181 km, and the Dutch B-25s Koepang from Darwin, 806", () => {
    const rows = AIR.fleets as unknown as { nat: string; name: string; bmrRange?: number }[];
    const NAVY = ['11th Air Fleet (21st and 23rd Air Flotillas)', '22nd Air Flotilla', '24th Air Flotilla', '25th Air Flotilla', '23rd Air Flotilla', '1st Air Fleet', '2nd Air Fleet', '5th Air Fleet'];
    // The air flotillas and the air fleets fly the navy's land bombers; the army's rows, and the home defence's two
    // (the army's and the navy's together), keep the nation's 1,048 until a source gives theirs.
    for (const r of rows.filter((x) => x.nat === 'JAP')) expect(r.bmrRange, r.name).toBe(NAVY.includes(r.name) ? 1181 : undefined);
    expect(rows.filter((x) => x.nat === 'JAP' && x.bmrRange !== undefined).map((x) => x.name).sort()).toEqual([...NAVY].sort());
    // The flight that sets it is in reach, and the figure is no more than the flight rounded up (1,180.03 on the chart).
    const wake = km(g, at('Kwajalein').id, at('Wake').id);
    expect(wake).toBeLessThanOrEqual(1181);
    expect(wake).toBeGreaterThan(1180);
    // Saigon to Singapore, the record's other routine route, is inside it; so are Kuching, Miri and Brunei from Saigon,
    // which the 1,048 left out of reach. Truk to Rabaul is not shown as a land bombers' raid, and stays outside.
    expect([read('Saigon', 'Singapore'), read('Saigon', 'Kuching'), read('Saigon', 'Miri'), read('Saigon', 'Brunei')]).toEqual([1102, 1124, 1081, 1126]);
    for (const to of ['Singapore', 'Kuching', 'Miri', 'Brunei']) { expect(read('Saigon', to), to).toBeLessThanOrEqual(1181); expect(read('Saigon', to), to).toBeGreaterThan(japan); }
    expect(read('Truk', 'Rabaul')).toBe(1298);
    // The army's bombers at Phnom Penh do not reach Kuching (1,277).
    expect(read('Phnom Penh', 'Kuching')).toBeGreaterThan(japan);
    // The Dutch squadron under Australian command: Timor from Darwin, and no farther (Ambon is 1,017).
    const dutch = rows.filter((x) => x.nat === 'AUS' && x.bmrRange !== undefined);
    expect(dutch.map((x) => [x.name, x.bmrRange])).toEqual([['No. 18 Squadron NEI', 806]]);
    const koepang = km(g, at('Darwin').id, at('Koepang').id);
    expect(koepang).toBeLessThanOrEqual(806);
    expect(koepang).toBeGreaterThan(805);
    expect(read('Darwin', 'Dili')).toBeLessThanOrEqual(806);
    expect(read('Darwin', 'Ambon')).toBeGreaterThan(806);
    // Above the 700 its bombers flew by air.json's default row.
    expect(AIR.nations._default.bmrRange).toBe(700);
  });

  it("America's escorts: 1,250 km from October 1944, Iwo Jima to Tokyo inside it; Morotai to Balikpapan is beyond it on the chart", () => {
    expect(AIR.jobs).toEqual({ USA: { cover: 550, raid: [[62, 1250]] } });
    expect(read('Iwo Jima', 'Tokyo')).toBeLessThanOrEqual(1250);
    expect(read('Saipan', 'Truk')).toBeLessThanOrEqual(1250);
    // (The record's 1,323 km from the field; 1,355 from centre to centre. Read and named, not mended.)
    expect(read('Morotai', 'Balikpapan')).toBe(1355);
  });
});

describe("the screen's ring", () => {
  it('round a base of the far side, an ellipse by its latitude: a degree of longitude there against the chart\'s parallel', () => {
    const manila = at('Manila');
    const want = Math.cos((50 * Math.PI) / 180) / Math.cos((latOf(manila.y) * Math.PI) / 180);
    expect(ringStretch(g, manila)).toBeCloseTo(want, 9);
    expect(Math.round(ringStretch(g, manila) * 1000) / 1000).toBe(0.664);
    // In the far north the chart draws a circle of the globe wider than tall.
    expect(ringStretch(g, at('Anchorage'))).toBeGreaterThan(1);
  });

  it('round a base of the old chart, the circle as it always was', () => {
    for (const nm of ['Berlin', 'Leningrad', 'New York', 'San Francisco']) expect(ringStretch(g, at(nm)), nm).toBe(1);
  });
});

describe("with the far side's switch off", () => {
  const G = globalThis as { TC_FAR?: unknown };
  afterAll(() => { G.TC_FAR = 1; });

  it("nothing reads the globe: every distance is the map's straight line, and the ring a circle", async () => {
    vi.resetModules();
    G.TC_FAR = 0;
    const { FAR_STAGE } = await import('../src/campaign/stage');
    expect(FAR_STAGE).toBe(false);
    const game = await import('../src/campaign/game');
    const staff = await import('../src/campaign/staff');
    const air = await import('../src/campaign/air');
    const wrap = await import('../src/campaign/wrap');
    const world = await import('../src/campaign/world');
    const o = staff.startOpen(game.createGame(3, null));
    expect([world.farOn(o), wrap.globeOn(o)]).toEqual([false, false]);
    for (const [a, b] of [['Leningrad', 'Karelia'], ['Berlin', 'Paris'], ['San Francisco', 'New York'], ['Panama City', 'Caracas']]) {
      const A = o.byName[a], B = o.byName[b];
      expect([!!A, !!B], `${a}, ${b}`).toEqual([true, true]);
      expect(wrap.globeFor(o, A, B)).toBe(false);
      expect(air.km(o, A.id, B.id)).toBe(wrap.mapDist(o, A.x, A.y, B.x, B.y) * air.AIR.kmPerUnit);
      expect(wrap.chartDist2(o, A, B)).toBe(wrap.mapDist2(o, A.x, A.y, B.x, B.y));
      expect(wrap.ringStretch(o, A)).toBe(1);
    }
    // No sea of the far side is on this chart, and a harbour reads the map's line to the seas it has.
    expect(o.seas.length).toBe(67);
    const sf = o.byName['San Francisco'], cal = o.seas.find((z) => z.key === 'california')!;
    expect(wrap.seaDist(o, sf, cal)).toBe(wrap.mapDist(o, sf.x, sf.y, cal.x, cal.y));
    // And the same figure as with the switch on, for two provinces of the old chart.
    expect(Math.round(air.km(o, o.byName.Leningrad.id, o.byName.Karelia.id))).toBe(203);
  });
});
