// Japan's supplies book (ruling 195 and the rider to its point (c); the Pacific planner's paper 03b).
//
// The oil book's form with other figures (data/campaign/far-books.json, laid by far-rows.ts). Colonies and occupied
// ground on the far side of the world give nobody anything at home with no ship: lanes stand for them. Japan's home
// ground gives 62 in 100 of what its people need, and five lanes bring shares of its need over the water: Korea 19,
// Formosa 5, the rice of Indochina and Siam 8, the rest of the south 9, America 4. The freeze stops America's and
// the southern lane; no lane runs through a sea closed to Japan; the stock is everyone's, 3 months and room for 6.
// Each state is set by hand on the first day's chart; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { closedTo, zoneOf } from '../src/campaign/navy';
import { RES, laneStopped, needOf, overTheWater, stores, storesOf, type Book, type Lane, type ResLine } from '../src/campaign/resources';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game, Ships } from '../src/campaign/types';
import { FAR_FIRST, FAR_SEAS_FIRST } from '../src/campaign/world';
import farBooks from '../data/campaign/far-books.json';

const grand = (seed = 3): Game => startOpen(createGame(seed, null));
const book = (g: Game, nat = 'JAP'): Book => stores(g, nat)!.sup;
const lane = (g: Game, id: string): Lane => stores(g, 'JAP')!.lanes.find((l) => l.id === id)!;
const line = (g: Game, nat: string, key: string): ResLine | undefined => book(g, nat).income.find((l) => l.key === key);
const LANES: Record<string, number> = { 'korea-japan': 0.19, 'formosa-japan': 0.05, 'rice-japan': 0.08, 'south-japan': 0.09, 'america-japan': 0.04 };
const SOUTH_SEAS = ['schina', 'luzonsea', 'luzonstrait', 'echina'];
/** An American squadron of these ships alone, set by hand in one sea. */
const squadron = (g: Game, ships: Partial<Ships>, sea: string, stance: Fleet['stance']): Fleet => {
  const f = g.fleets.find((x) => x.nat === 'USA' && x.name === 'Asiatic Fleet')!;
  f.ships = { bb: 0, cv: 0, ca: 0, dd: 0, ss: 0, ...ships };
  f.zone = zoneOf(g, sea); f.stance = stance; f.ready = 1;
  return f;
};

describe("Japan's supplies book (ruling 195)", () => {
  it('colonies and occupied ground on the far side give nobody anything at home with no ship', () => {
    const g = grand();
    // The old chart's figures on the first day (far side off, seed 3): each power's ground gives what it gave, to the unit.
    const OLD: Record<string, { land: [number, number]; fac: [number, number]; colonies?: [number, number] }> = {
      GER: { land: [24.6, 36], fac: [18.5, 37] },
      UK: { land: [9.1, 17], fac: [9, 18], colonies: [5.65, 28] },
      FRA: { land: [25.3, 37], fac: [7.5, 15], colonies: [3.975, 20] },
      ITA: { land: [8.8, 19], fac: [4, 8], colonies: [1.8, 16] },
      USA: { land: [116.5, 184], fac: [31, 62], colonies: [0.5, 2] },
    };
    for (const [nat, was] of Object.entries(OLD)) {
      for (const key of ['land', 'fac', 'colonies'] as const) {
        const l = line(g, nat, key), w = was[key];
        if (!w) { expect(l, `${nat} ${key}`).toBeUndefined(); continue; }
        expect(l!.amount, `${nat} ${key}`).toBeCloseTo(w[0], 9);
        expect(l!.count, `${nat} ${key}`).toBe(w[1]);
      }
      expect(line(g, nat, 'occupied'), nat).toBeUndefined();
    }
    // What stands on the far side for each of them, and gives nothing: Malaya and India, Indochina, East Africa, Hawaii and Alaska.
    const far = (nat: string): number => g.regions.filter((r) => r.ctrl === nat && overTheWater(r, nat)).length;
    expect([far('UK'), far('FRA'), far('ITA'), far('USA'), far('GER')]).toEqual([275, 107, 40, 46, 0]);
    for (const nm of ['Singapore', 'Calcutta', 'Saigon', 'Manila', 'Pearl Harbor', 'Anchorage']) expect(overTheWater(g.byName[nm], g.byName[nm].ctrl), nm).toBe(true);
    // The Soviet Union's home ground on the far side counts at home, with the people it feeds (195's exception).
    const sov = g.regions.filter((r) => r.ctrl === 'SOV' && r.id >= FAR_FIRST);
    expect(sov.length).toBe(169);
    expect(sov.some((r) => overTheWater(r, 'SOV'))).toBe(false);
    expect(line(g, 'SOV', 'land')!.count).toBe(77 + 169);
    expect(book(g, 'SOV').spend.find((l) => l.key === 'people')!.count).toBe(77 + 169);
    // Japan's own: Korea and Formosa, and the ground it holds in China, give nothing with no ship. Its home islands do.
    expect(line(g, 'JAP', 'colonies')).toBeUndefined();
    expect(line(g, 'JAP', 'occupied')).toBeUndefined();
    expect([far('JAP'), overTheWater(g.byName['Tokyo'], 'JAP'), overTheWater(g.byName['Pusan'], 'JAP'), overTheWater(g.byName['Vladivostok'], 'SOV')]).toEqual([61, false, true, false]);
    // And ground of the old chart counts as it did, a colony at half: the rule is the far side's alone.
    for (const r of g.regions) if (r.id < FAR_FIRST) expect(overTheWater(r, r.ctrl)).toBe(false);
  });

  it("Japan's home ground gives 62 in 100 of what its people need, and the stock is everyone's", () => {
    const g = grand();
    expect(RES.sup.ratio.JAP).toBe(0.62);
    const b = book(g);
    const land = line(g, 'JAP', 'land')!, fac = line(g, 'JAP', 'fac')!;
    expect([land.amount, land.count, fac.amount, fac.count].map((x) => +x!.toFixed(9))).toEqual([13.9, 28, 7, 14]);
    const people = b.spend.find((l) => l.key === 'people')!;
    expect(people.amount).toBeCloseTo(20.9 / 0.62, 9);
    expect(people.count).toBe(28);
    // Three months of its need on the first day and room for six: no row of Japan's own.
    expect(RES.sup.start.JAP).toBeUndefined();
    const month = needOf(g, 'JAP', 'sup', true);
    expect(storesOf(g, 'JAP')!.sup).toBeCloseTo(RES.sup.start._default * month, 9);
    expect(RES.sup.start._default).toBe(3);
    expect(b.cap).toBeCloseTo(6 * month, 9);
  });

  it('five lanes bring 45 in 100 of its need over the water, and before the freeze Japan is not short', () => {
    const g = grand();
    const need = needOf(g, 'JAP', 'sup');
    for (const [id, share] of Object.entries(LANES)) {
      const l = lane(g, id);
      expect([l.state, l.flow], id).toEqual(['open', 1]);
      expect(l.full.sup, id).toBeCloseTo(share * need, 9);
      expect(line(g, 'JAP', `lane:${id}`)!.amount, id).toBeCloseTo(share * need, 9);
      expect(laneStopped(g, id), id).toBeNull();
    }
    const rows = (farBooks as any)['resources.json'].lanes.filter((x: any) => x.sup > 0);
    expect(rows.map((x: any) => [x.id, x.sup])).toEqual([['america-japan', 0.04], ['korea-japan', 0.19], ['formosa-japan', 0.05], ['rice-japan', 0.08], ['south-japan', 0.09]]);
    // Where each sails: the Korea lane by the Yellow Sea and the Tsushima Strait, Formosa's by the East China Sea,
    // the rice and the rest of the south by the southern oil lane's seas; America's by no sea of the chart.
    const zones = (id: string): string[] => RES.lanes.find((x) => x.id === id)!.zones;
    expect([zones('korea-japan'), zones('formosa-japan'), zones('rice-japan'), zones('south-japan'), zones('america-japan')]).toEqual([['yellow', 'tsushima'], ['echina'], SOUTH_SEAS, SOUTH_SEAS, []]);
    expect(zones('south-oil')).toEqual(SOUTH_SEAS);
    for (const z of ['yellow', 'tsushima', ...SOUTH_SEAS]) expect(zoneOf(g, z), z).toBeGreaterThanOrEqual(FAR_SEAS_FIRST);
    // The book: home ground and the five lanes, and it covers the need.
    const b = book(g);
    expect(b.made).toBeCloseTo(20.9 + 0.45 * need, 9);
    expect(b.net).toBeGreaterThan(0);
    expect(b.net).toBeLessThan(0.05 * need);
  });

  it('the freeze stops America\'s lane and the southern lane, until Singapore is Japan\'s; the rice, Formosa\'s and Korea\'s sail on', () => {
    const g = grand();
    const need = needOf(g, 'JAP', 'sup');
    g.flags['pac:freeze'] = g.turn;
    const why = 'Stopped by the freeze: nothing is sold to Japan.';
    expect(lane(g, 'america-japan')).toMatchObject({ state: 'cut', flow: 0, sup: 0, why });
    expect(lane(g, 'south-japan')).toMatchObject({ state: 'cut', flow: 0, sup: 0, why });
    for (const id of ['korea-japan', 'formosa-japan', 'rice-japan']) expect(lane(g, id).state, id).toBe('open');
    expect(book(g).made).toBeCloseTo(20.9 + 0.32 * need, 9);
    expect(book(g).net).toBeLessThan(0);
    // Singapore taken: what Japan has made its own sails again. America's does not.
    g.byName['Singapore'].ctrl = 'JAP';
    expect(lane(g, 'south-japan')).toMatchObject({ state: 'open', flow: 1 });
    expect(lane(g, 'america-japan').state).toBe('cut');
    expect(book(g).made).toBeCloseTo(20.9 + 0.41 * need, 9);
  });

  it('each lane has its key: Pusan, the island of Formosa, Saigon and Singapore', () => {
    const g = grand();
    // The rice is bought of whoever holds Saigon, and the south's cargoes pass Singapore: an enemy there stops them.
    expect([g.byName['Saigon'].ctrl, g.byName['Singapore'].ctrl]).toEqual(['FRA', 'UK']);
    declareWar(g, 'JAP', 'UK');
    expect(lane(g, 'south-japan')).toMatchObject({ state: 'cut', flow: 0, why: 'The United Kingdom holds Singapore and lets nothing past.' });
    expect(lane(g, 'rice-japan').state).toBe('open');
    g.byName['Saigon'].ctrl = 'UK';
    expect(lane(g, 'rice-japan')).toMatchObject({ state: 'cut', flow: 0, why: 'The United Kingdom holds Saigon and lets nothing past.' });
    g.byName['Saigon'].ctrl = 'JAP';
    g.byName['Singapore'].ctrl = 'JAP';
    expect([lane(g, 'rice-japan').state, lane(g, 'south-japan').state]).toEqual(['open', 'open']);
    // Korea's sails while Pusan is Japan's.
    expect(atWar(g, 'JAP', 'UK')).toBe(true);
    g.byName['Pusan'].ctrl = 'UK';
    expect(lane(g, 'korea-japan')).toMatchObject({ state: 'cut', flow: 0, why: 'The United Kingdom holds Pusan and the road through it is shut.' });
    // Formosa is the island (the design director, 13:14Z, 22 November 2026): its cargoes sailed from Keelung in the
    // north and Takao in the south, so the lane runs while Japan holds either province and stops when an enemy holds both.
    expect(RES.lanes.find((x) => x.id === 'formosa-japan')!.ports).toEqual(['Taihoku', 'Formosa']);
    g.byName['Formosa'].ctrl = 'UK';
    expect(lane(g, 'formosa-japan')).toMatchObject({ state: 'open', flow: 1 });
    g.byName['Formosa'].ctrl = 'JAP';
    g.byName['Taihoku'].ctrl = 'UK';
    expect(lane(g, 'formosa-japan')).toMatchObject({ state: 'open', flow: 1 });
    g.byName['Formosa'].ctrl = 'UK';
    expect(lane(g, 'formosa-japan')).toMatchObject({ state: 'cut', flow: 0, sup: 0, why: 'The United Kingdom holds Taihoku and Formosa, and nothing sails from them.' });
    expect(lane(g, 'rice-japan').state).toBe('open');
  });

  it('no lane runs through a sea closed to Japan, and raiders sink a share of what sails', () => {
    const g = grand();
    declareWar(g, 'JAP', 'USA');
    const need = needOf(g, 'JAP', 'sup');
    expect(RES.lanes.filter((x) => x.closes).map((x) => x.id)).toEqual(['south-oil', 'korea-japan', 'formosa-japan', 'rice-japan', 'south-japan']);
    // Boats in the Yellow Sea, sixty on that beat and twenty of them out (the far side's one measure): the Korea lane is raided, the others are not.
    const f = squadron(g, { ss: 60 }, 'yellow', 'raid');
    const k = lane(g, 'korea-japan');
    expect(k.state).toBe('raided');
    expect(k.flow).toBeLessThan(1);
    expect(k.sup).toBeCloseTo(0.19 * need * k.flow, 9);
    expect(lane(g, 'formosa-japan')).toMatchObject({ state: 'open', flow: 1 });
    // A battle fleet in the East China Sea that Japan does not answer: Formosa's lane, the rice and the south's stop. Korea's does not.
    f.ships = { bb: 40, cv: 0, ca: 20, dd: 40, ss: 0 }; f.zone = zoneOf(g, 'echina'); f.stance = 'patrol';
    expect(closedTo(g, f.zone, 'JAP')).toBe(true);
    const why = 'The East China Sea is closed to the ships of Japan.';
    for (const id of ['formosa-japan', 'rice-japan']) expect(lane(g, id), id).toMatchObject({ state: 'cut', flow: 0, sup: 0, why });
    expect(lane(g, 'korea-japan')).toMatchObject({ state: 'open', flow: 1 });
    expect(line(g, 'JAP', 'lane:rice-japan')).toMatchObject({ amount: 0, note: why });
    // And in the Tsushima Strait: Korea's.
    f.zone = zoneOf(g, 'tsushima');
    expect(closedTo(g, f.zone, 'JAP')).toBe(true);
    expect(lane(g, 'korea-japan')).toMatchObject({ state: 'cut', flow: 0, sup: 0, why: 'The Tsushima Strait is closed to the ships of Japan.' });
    expect(lane(g, 'rice-japan').state).toBe('open');
  });
});
