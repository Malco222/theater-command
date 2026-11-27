// Japan's oil book (ruling 194, points 2 to 6; the Pacific planner's paper 03, "oil from overseas").
//
// With the far side on Japan keeps stocks as the six powers do (data/campaign/far-books.json, laid by far-rows.ts):
// 2 a month at home, 34 months in its tanks by the code's count of a month on the first day, a lane from America for
// nine tenths of its need until the freeze or a war with America, and the southern oil lane, which carries home
// what the seven oilfields of the south yield to Japan while it holds them: less what raiders sink in its seas, and
// nothing while one of those seas is closed to Japan. America's submarines raid at a quarter of their worth in the
// far side's seas while their torpedoes fail. Each state is set by hand on the first day's chart; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { NAVY, boatsOut, closedTo, laneLoss, subsWorth, zoneOf } from '../src/campaign/navy';
import { RES, fieldOverseas, fireField, keepsStores, laneStopped, needOf, plantsMost, plantsSpent, stores, storesOf, type Lane, type ResLine } from '../src/campaign/resources';
import { startOpen } from '../src/campaign/staff';
import { TECH, techOf, techRule } from '../src/campaign/tech';
import type { Fleet, Game, Ships } from '../src/campaign/types';
import { FAR_SEAS_FIRST } from '../src/campaign/world';
import farBooks from '../data/campaign/far-books.json';

const SIX = ['GER', 'UK', 'FRA', 'ITA', 'SOV', 'USA'];
const SOUTH = ['Palembang', 'Balikpapan', 'Medan', 'Soerabaja', 'Brunei', 'Yenangyaung', 'Tarakan'];
const SEAS = ['schina', 'luzonsea', 'luzonstrait', 'echina'];
const grand = (seed = 3): Game => startOpen(createGame(seed, null));
const lane = (g: Game, id: string): Lane => stores(g, 'JAP')!.lanes.find((l) => l.id === id)!;
const income = (g: Game, key: string): ResLine | undefined => stores(g, 'JAP')!.oil.income.find((l) => l.key === key);
const take = (g: Game, names: string[]): void => { for (const nm of names) g.byName[nm].ctrl = 'JAP'; };
/** An American squadron of these ships alone, set by hand in one sea. */
const squadron = (g: Game, ships: Partial<Ships>, sea: string, stance: Fleet['stance']): Fleet => {
  const f = g.fleets.find((x) => x.nat === 'USA' && x.name === 'Asiatic Fleet')!;
  f.ships = { bb: 0, cv: 0, ca: 0, dd: 0, ss: 0, ...ships };
  f.zone = zoneOf(g, sea); f.stance = stance; f.ready = 1;
  return f;
};

describe("Japan's oil book (ruling 194)", () => {
  it('Japan keeps stocks beside the six: 2 a month at home, and 34 months in tanks that hold as much', () => {
    const g = grand();
    expect(RES.nations).toEqual([...SIX, 'JAP']);
    expect(keepsStores(g, 'JAP')).toBe(true);
    // Akita's field, and the small wells and plants at home: a tenth of its need, as it was.
    expect(income(g, 'fields')).toMatchObject({ amount: 1, count: 1 });
    expect(income(g, 'home')!.amount).toBe(1);
    expect(income(g, 'taken')).toBeUndefined();
    // In store: 34 months as the code counts a month on the first day (the fleet lies in harbour), and no room for more.
    const month = needOf(g, 'JAP', 'oil', true);
    expect(storesOf(g, 'JAP')!.oil).toBeCloseTo(34 * month, 9);
    expect(stores(g, 'JAP')!.oil.cap).toBeCloseTo(34 * month, 9);
    // With the fleet at sea that is the year and a half Japan's planners counted on: 15 to 19 months of a fleet at war.
    for (const f of g.fleets) if (f.nat === 'JAP') f.stance = 'patrol';
    const atWarNeed = needOf(g, 'JAP', 'oil', true);
    expect(storesOf(g, 'JAP')!.oil / atWarNeed).toBeGreaterThan(15);
    expect(storesOf(g, 'JAP')!.oil / atWarNeed).toBeLessThan(19);
    // The tanks' figure is a row by nation, and Japan's is the only row: everyone else keeps 8.
    expect(RES.oil.caps).toEqual({ JAP: 34 });
    for (const nat of SIX) expect(stores(g, nat)!.oil.cap, nat).toBeCloseTo(RES.oil.cap * Math.max(needOf(g, nat, 'oil', true), 1), 9);
  });

  it('buys nine tenths of its need from America, until the freeze or a war with America', () => {
    const g = grand();
    const need = needOf(g, 'JAP', 'oil');
    const l = lane(g, 'america-japan');
    expect([l.state, l.flow]).toEqual(['open', 1]);
    expect(l.full.oil).toBeCloseTo(0.9 * need, 9);
    // (With cotton, machines and scrap on the same row: 4 in 100 of its supplies, the supplies book's.)
    expect([l.oil, l.sup]).toEqual([l.full.oil, 0.04 * needOf(g, 'JAP', 'sup')]);
    // While America sells Japan is not short: what comes in covers what it burns.
    const book = stores(g, 'JAP')!.oil;
    expect(book.made).toBeCloseTo(2 + 0.9 * need, 9);
    expect(book.net).toBeGreaterThan(0);
    expect(laneStopped(g, 'america-japan')).toBeNull();
    // The freeze (pacific.ts: August 1941 where the stop keeps the book) stops the sales.
    g.flags['pac:freeze'] = g.turn;
    expect(lane(g, 'america-japan')).toMatchObject({ state: 'cut', flow: 0, oil: 0, why: 'Stopped by the freeze: nothing is sold to Japan.' });
    expect(stores(g, 'JAP')!.oil.made).toBe(2);
    delete g.flags['pac:freeze'];
    expect(lane(g, 'america-japan').state).toBe('open');
    // And so does a war with America, as with every bought cargo.
    declareWar(g, 'JAP', 'USA');
    expect(atWar(g, 'JAP', 'USA')).toBe(true);
    expect(lane(g, 'america-japan')).toMatchObject({ state: 'cut', flow: 0, oil: 0 });
    expect(lane(g, 'america-japan').why).toMatch(/at war with the United States/);
  });

  it('the southern oil lane carries home what the fields of the south yield to Japan, and they give nothing at home', () => {
    const g = grand();
    const row = RES.lanes.find((x) => x.id === 'south-oil')!;
    expect(row.fields).toEqual(SOUTH);
    expect(row.zones).toEqual(SEAS);
    for (const z of SEAS) expect(zoneOf(g, z), z).toBeGreaterThanOrEqual(FAR_SEAS_FIRST);
    // Seven fields on colony ground, 33 a month to their owners.
    for (const nm of SOUTH) expect(fieldOverseas(g.byName[nm]), nm).toBe(true);
    expect(SOUTH.reduce((s, nm) => s + RES.oil.fields[nm], 0)).toBe(33);
    // Japan holds none: nothing sails, and the book lists no such lane.
    expect(lane(g, 'south-oil')).toMatchObject({ state: 'idle', flow: 0, oil: 0, full: { oil: 0, sup: 0 }, why: 'Japan holds none of the oilfields it carries.' });
    expect(income(g, 'lane:south-oil')).toBeUndefined();
    const before = stores(g, 'JAP')!.oil.made;
    // Palembang and Balikpapan taken: six tenths of their 19, by the lane and not at home.
    take(g, ['Palembang', 'Balikpapan']);
    expect(lane(g, 'south-oil')).toMatchObject({ state: 'open', flow: 1 });
    expect(lane(g, 'south-oil').oil).toBeCloseTo(RES.oil.occupied * 19, 9);
    expect(income(g, 'taken')).toBeUndefined();
    expect(income(g, 'lane:south-oil')!.amount).toBeCloseTo(11.4, 9);
    expect(stores(g, 'JAP')!.oil.made).toBeCloseTo(before + 11.4, 9);
    // A field set alight gives that much less for as long as it burns.
    expect(fireField(g, g.byName['Palembang'], 6, 0.5)).toBe(true);
    expect(lane(g, 'south-oil').oil).toBeCloseTo(RES.oil.occupied * (14 * 0.5 + 5), 9);
    g.flags[`oilout:${g.byName['Palembang'].id}`] = 0;
    // All seven: 19.8 to a conqueror, about nine tenths of what Japan burns in 1942.
    take(g, SOUTH);
    expect(lane(g, 'south-oil').oil).toBeCloseTo(19.8, 9);
    // A field its owner holds feeds no lane to Japan, and one an ally of Japan's holds is not Japan's.
    g.byName['Tarakan'].ctrl = g.byName['Tarakan'].owner;
    expect(lane(g, 'south-oil').oil).toBeCloseTo(19.8 - RES.oil.occupied * 2, 9);
  });

  it('raiders in its seas sink a share of the tankers, and America\'s submarines raid at a quarter while their torpedoes fail', () => {
    const g = grand();
    take(g, SOUTH);
    declareWar(g, 'JAP', 'USA');
    const began = g.turn;
    const zones = SEAS.map((k) => zoneOf(g, k));
    const raid = (): number => laneLoss(g, 'JAP', zones, 'south-oil').raid;
    expect(raid()).toBe(0);
    expect(lane(g, 'south-oil')).toMatchObject({ state: 'open', flow: 1 });
    // Sixty boats on the lane's beat, twenty of them out at the record's one in three (the far side's one measure,
    // `boatsOut`), and two cruisers with them: the boats alone are worth a quarter.
    const f = squadron(g, { ss: 60, ca: 2 }, 'schina', 'raid');
    expect(boatsOut(g, f)).toBe(20);
    const k = raid() / (20 * NAVY.classes.ss.raid * 0.25 + 2 * NAVY.classes.ca.raid);
    expect(k).toBeGreaterThan(0);
    expect(subsWorth(g, f)).toBe(0.25);
    const l = lane(g, 'south-oil');
    expect(l.state).toBe('raided');
    expect(l.flow).toBeLessThan(1);
    expect(l.oil).toBeCloseTo(19.8 * l.flow, 9);
    expect(l.raid).toBeCloseTo(raid(), 9);
    // Where history is loose no date decides it: in full from America's twenty-second month at war with Japan.
    g.turn = began + 20;
    expect(subsWorth(g, f)).toBe(0.25);
    g.turn = began + 21;
    expect(subsWorth(g, f)).toBe(1);
    expect(raid()).toBeCloseTo(k * (20 * NAVY.classes.ss.raid + 2 * NAVY.classes.ca.raid), 9);
    // Where the stop keeps the book it is history's month: September 1943, whenever the war began.
    setStop(g, 1, true);
    g.turn = 48;
    expect(subsWorth(g, f)).toBe(0.25);
    g.turn = 49;
    expect(subsWorth(g, f)).toBe(1);
    // Only America's boats, and only in the far side's seas.
    g.turn = 30;
    expect(subsWorth(g, f)).toBe(0.25);
    f.zone = zoneOf(g, 'natl');
    expect(f.zone).toBeLessThan(FAR_SEAS_FIRST);
    expect(subsWorth(g, f)).toBe(1);
    const u = g.fleets.find((x) => x.nat === 'GER')!;
    u.zone = zoneOf(g, 'schina');
    expect(subsWorth(g, u)).toBe(1);
    expect((farBooks as any)['navy.json'].subs).toEqual({ USA: { share: 0.25, book: 49, month: 22, foe: 'JAP' } });
  });

  it('nothing sails on it while one of its seas is closed to Japan', () => {
    const g = grand();
    take(g, SOUTH);
    declareWar(g, 'JAP', 'USA');
    const strait = zoneOf(g, 'luzonstrait');
    expect(closedTo(g, strait, 'JAP')).toBe(false);
    expect(lane(g, 'south-oil').state).toBe('open');
    // A battle fleet in the Luzon Strait that Japan does not answer: the sea is closed to her tankers.
    squadron(g, { bb: 40, ca: 20, dd: 40 }, 'luzonstrait', 'patrol');
    expect(closedTo(g, strait, 'JAP')).toBe(true);
    expect(lane(g, 'south-oil')).toMatchObject({ state: 'cut', flow: 0, oil: 0, why: 'The Luzon Strait is closed to the ships of Japan.' });
    expect(laneStopped(g, 'south-oil')!.state).toBe('cut');
    // The book keeps the lane's line, with nothing on it and the reason.
    expect(income(g, 'lane:south-oil')).toMatchObject({ amount: 0, note: 'The Luzon Strait is closed to the ships of Japan.' });
    // Only Japan's lanes are read so (194 point 3, and 195 point 5 for the supplies lanes): nothing of Europe's moves.
    expect(RES.lanes.filter((x) => x.closes).map((x) => x.to)).toEqual(['JAP', 'JAP', 'JAP', 'JAP', 'JAP']);
    expect(RES.lanes.filter((x) => x.closes && x.oil + (x.fields?.length ?? 0) > 0).map((x) => x.id)).toEqual(['south-oil']);
  });
});

// Form 2 (i), the design director's ruling of 20:41Z, 22 November 2026, on the historian's
// japan-oil-1941-45-partial.md. The fault: the technology tree gave Japan's fourteen factories 0.25 each, 3.5 a month,
// 0.18 of the southern lane's full flow. The record's synthetic plans "failed to yield significant results": 1 million
// barrels in 1943 against imports at a rate of 21 million a year at their peak, 0.048 of it. On the lane's own scale
// (19.8 at its full) that is 0.94 a month, the plants' yield at full build.
describe("Japan's fuel plants make the record's yield", () => {
  const tech = (g: Game, nat: string): ResLine | undefined => stores(g, nat)!.oil.income.find((l) => l.key === 'tech');
  const SYN = TECH.techs.find((x) => x.id === 'synthetics')!, HYD = TECH.techs.find((x) => x.id === 'hydrogenation')!;

  it('0.94 a month at full build, an equal share a factory of the homeland, whatever the tree would give', () => {
    const g = grand();
    expect(RES.oil.plants).toEqual({ JAP: 0.94 });
    // The row's own arithmetic: 1 million barrels against 1.75 million a month for a year, of the lane's 19.8.
    expect(19.8 / (1.75 * 12)).toBeCloseTo(0.94, 2);
    expect(plantsMost(g, 'JAP')).toBe(0.94);
    expect(plantsMost(g, 'GER')).toBeNull();
    expect(tech(g, 'JAP')).toBeUndefined();
    techOf(g, 'JAP')!.done.push('synthetics');
    const home = g.regions.filter((r) => r.owner === 'JAP' && !r.overseas);
    const all = home.reduce((n, r) => n + r.fac, 0);
    expect(all).toBeGreaterThan(8);
    expect(tech(g, 'JAP')).toMatchObject({ count: all });
    expect(tech(g, 'JAP')!.amount).toBeCloseTo(0.94, 9);
    // The tree's own rate would have given it several times that.
    expect(SYN.oil! * all).toBeGreaterThan(2);
    // More of the tree's fuel chemistry adds nothing: the yield is the record's at full build.
    techOf(g, 'JAP')!.done.push('hydrogenation');
    expect(tech(g, 'JAP')!.amount).toBeCloseTo(0.94, 9);
    // A factory lost makes none of its share, and the rest make no more for it.
    const lost = home.find((r) => r.fac > 0 && r.id !== g.nations.JAP.capital)!;
    lost.ctrl = 'USA';
    expect(tech(g, 'JAP')!.amount).toBeCloseTo(0.94 * (all - lost.fac) / all, 9);
    lost.ctrl = 'JAP';
    // Germany's plants make the tree's rate, as on the old chart.
    techOf(g, 'GER')!.done.push('synthetics');
    expect(tech(g, 'GER')!.amount).toBeCloseTo(SYN.oil! * tech(g, 'GER')!.count!, 9);
    // The far side off: the row is nobody's.
    g.flags.world = 0;
    try { expect(plantsMost(g, 'JAP')).toBeNull(); } finally { g.flags.world = 1; }
  });

  it('its staff gains nothing by more fuel chemistry once the plants make all they can, and the card says what they make', () => {
    const g = grand();
    expect(plantsSpent(g, 'JAP')).toBe(false);
    // (The UI director's words of 20:45Z: the country's name to anyone but its own player, and "your" to him.)
    const first = 'fuel plants make 0.94 oil a month between them, far less than was planned for them. Submarines cannot touch this oil, but bombers can hit the plants.';
    expect(techRule(SYN, g, 'JAP')).toContain(`With every factory at home working, Japan's ${first}`);
    expect(techRule(SYN, g, 'GER')).toMatch(/Every working factory at home also makes [\d.]+ oil a month/);
    techOf(g, 'JAP')!.done.push('synthetics');
    expect(plantsSpent(g, 'JAP')).toBe(true);
    // The first step's card still says what it gives; the second says that it adds nothing.
    expect(plantsSpent(g, 'JAP', 'synthetics')).toBe(false);
    expect(techRule(SYN, g, 'JAP')).toContain(`With every factory at home working, Japan's ${first}`);
    expect(techRule(HYD, g, 'JAP')).toContain("Japan's fuel plants already make all they can: this adds no oil.");
    const mine = startOpen(createGame(3, 'JAP'));
    expect(techRule(SYN, mine, 'JAP')).toContain(`With every factory at home working, your ${first}`);
    techOf(mine, 'JAP')!.done.push('synthetics');
    expect(techRule(HYD, mine, 'JAP')).toContain('Your fuel plants already make all they can: this adds no oil.');
    techOf(g, 'GER')!.done.push('synthetics');
    expect(plantsSpent(g, 'GER')).toBe(false);
  });
});
