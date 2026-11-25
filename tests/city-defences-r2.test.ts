// City defences (release 2; Brian 24 Nov 2026 22:08Z; city defences, the design director's ruling of 25 Nov 2026
// 02:23Z). One switch, 'cityDefences'. Slice 1: the switch, the data and the searchlights work: offered straight after
// the guns, by the guns' own rule, at 2 / 3 / 5, in a month, billed 0.015 a level, to level 3; and with the switch off
// nothing of it is offered, billed, written or read.
import { afterEach, describe, expect, it } from 'vitest';
import cd from '../data/campaign/citydefences.json';
import { assessAir, fly, type AirAssess } from '../src/campaign/air';
import { AC } from '../src/campaign/aircampaigns';
import { setAirPiece } from '../src/campaign/airversion';
import { raidTargets } from '../src/campaign/airwar';
import { cityNight, cityNightShare, flakCut, flakCutMost, flakLostWithTown, isFlakLine, townFlak, townFlakCut, townWorksAt, townWorksMonth, townWorksWorth } from '../src/campaign/citydefences';
import { SIEGE, besieged, fieldWalls, garrison, ringClosed, siegeMonth, siegeView, storesOf, townBonus } from '../src/campaign/siege';
import { townSize } from '../src/campaign/towns';
import { beginHuman, humanUp, orderMove, orderStorm, runTurnAI, stepAI, stormOutlook } from '../src/campaign/turn';
import { techOf } from '../src/campaign/tech';
import { WEATHER, setWeather, zoneOfRegion } from '../src/campaign/weather';
import stage from '../data/campaign/stage.json';
import { ARSENAL, aiArsenal, offers, workLevel, worksUpkeep, type Offer } from '../src/campaign/arsenal';
import { loadGame, saveGame } from '../src/campaign/save';
import '../unreal/TheaterCommand/Rules/campaign-bridge';
import { CITYDEF, LIGHTS_TOP, START_LAID, WORKS_TOP, cityDefences, cityDefencesOn, layStartGunsFromRecord, lightsAt, openCityDefences, setCityDefences } from '../src/campaign/citydefences';
import { cancelBuild, deliver, ledger, upkeepTotal } from '../src/campaign/economy';
import { assess, capture, createGame, endPhase, relocate } from '../src/campaign/game';
import { cityOrders } from '../src/campaign/orders';
import { GUARD_TOP } from '../src/campaign/guard';
import { startOpen } from '../src/campaign/staff';
import { arsenalOffers, orderArsenal } from '../src/campaign/turn';
import type { AirFleet, Build, Corps, Game, Region } from '../src/campaign/types';

/**
 * The switch turned on (or off) in a campaign under way on the bare ground of release 1: its record's start is marked as
 * laid first, so the lights and guns of the history start (slice 6) do not stand where these tests build their own.
 */
const switchBare = (g: Game, on: boolean): void => { if (on) g.flags[START_LAID] = 1; setCityDefences(g, on); };
const grand = (on: boolean): Game => { const g = startOpen(createGame(3, 'GER')); switchBare(g, on); g.nations.GER.pp = 500; return g; };
const offer = (g: Game, key: string, sel: number): Offer | undefined => offers(g, 'GER', sel).find((o) => o.key === key);
/** Lets a month go by for the order book and hands over what is due. */
const nextMonth = (g: Game): void => { g.turn += 1; deliver(g, 'GER'); };
const lightsFlags = (g: Game): string[] => Object.keys(g.flags).filter((k) => k.startsWith('wk:lights:'));
/** The offer keys release 1 lists at Hamburg (tests/arsenal.test.ts pins the same list). */
const OFF_KEYS = ['elite', 'corps:mtn', 'corps:inf', 'corps:mot', 'corps:art', 'corps:arm', 'corps:eng', 'para', 'air:ftr', 'air:bmr', 'ship:dd', 'ship:ss', 'ship:tp', 'ship:ca', 'ship:cv', 'ship:bb', 'work:factory', 'work:yard', 'work:aa', 'work:field', 'work:fort'];

describe('city defences: the switch and the data', () => {
  it('is off in stage.json, and one flag carries it', () => {
    expect((stage as Record<string, unknown>).cityDefences).toBe(false);
    expect(typeof (stage as Record<string, unknown>)._cityDefences).toBe('string');
    const g = startOpen(createGame(3, 'GER'));
    expect(g.flags.cityDefences).toBeUndefined();
    expect(cityDefences(g)).toBe(false);
    setCityDefences(g, true);
    expect(g.flags.cityDefences).toBe(1);
    expect(cityDefences(g) && cityDefencesOn(g)).toBe(true);
    setCityDefences(g, false);
    expect('cityDefences' in g.flags).toBe(false);
  });

  it('keeps every figure in the data file, each with its note', () => {
    expect(cd.night).toEqual({ dark: 0.2, radar: 0.45, lit: 0.2, both: 0.75 });
    expect(cd.saturation).toBe(100);
    expect(cd.fighters).toEqual([0, 0.002, 0.003, 0.004]);
    expect(cd.works).toEqual({ most: 3, worth: 0.3 });
    expect(cd.ground).toEqual({ armour: 0.07, most: 0.25, lost: 1 });
    expect(cd.ai).toEqual({ months: 2 });
    expect(cd.start.length).toBeGreaterThan(0);
    for (const k of ['night', 'saturation', 'fighters', 'works', 'ground', 'ai', 'start']) expect(typeof (cd as Record<string, unknown>)[`_${k}`], k).toBe('string');
    const w = ARSENAL.works.lights;
    expect(w.label).toBe('Searchlights');
    expect(w.cost).toEqual([0, 2, 3, 5]);
    expect(w.months).toBe(1);
    expect(w.upkeep).toBe(0.015);
    expect(w.blurb).toBe("Searchlights to hold the night bombers in their beams, by night and under a clear sky only. They help the night fighters a little, and where Fighter control's radar lays the guns, each level lights one level of anti-aircraft guns.");
  });
});

describe('city defences off: nothing of the searchlights', () => {
  it('offers the same list as release 1, bills nothing and writes no mark', () => {
    const g = grand(false);
    const r = g.byName['Hamburg'];
    expect(arsenalOffers(g, r.id).map((o) => o.key)).toEqual(OFF_KEYS);
    expect(offer(g, 'work:lights', r.id)).toBeUndefined();
    expect(orderArsenal(g, 'work:lights', r.id)).toBeNull();
    expect(g.nations.GER.queue).toHaveLength(0);
    for (let m = 0; m < 2; m++) nextMonth(g);
    expect(lightsFlags(g)).toEqual([]);
    // The start table is not laid with the switch off, even with a row in it.
    openCityDefences(g, [{ region: 'Hamburg', turns: [1], level: 2, mark: 'L', lay: true, why: 'a test row' }]);
    expect(lightsFlags(g)).toEqual([]);
  });

  it('reads no mark the ground keeps: no level, no upkeep, no ledger line', () => {
    const g = grand(false);
    const r = g.byName['Hamburg'];
    const before = upkeepTotal(g, 'GER');
    const lines = worksUpkeep(g, 'GER');
    g.flags[`wk:lights:${r.id}`] = 3;
    expect(lightsAt(g, r.id)).toBe(0);
    expect(worksUpkeep(g, 'GER')).toEqual(lines);
    expect(upkeepTotal(g, 'GER')).toBe(before);
    expect(ledger(g, 'GER').upkeep.some((l) => l.key === 'works:lights')).toBe(false);
    expect(offer(g, 'work:lights', r.id)).toBeUndefined();
  });
});

describe('city defences on: the searchlights work', () => {
  it('is offered straight after the guns, on any ground the nation holds', () => {
    const g = grand(true);
    const r = g.byName['Hamburg'];
    const keys = arsenalOffers(g, r.id).map((o) => o.key);
    expect(keys.filter((k) => k !== 'work:lights')).toEqual(OFF_KEYS);
    expect(keys.indexOf('work:lights')).toBe(keys.indexOf('work:aa') + 1);
    const o = offer(g, 'work:lights', r.id)!;
    expect(o.group).toBe('works');
    expect(o.label).toBe('Searchlights');
    expect(o.note).toBe(`${r.name} has no searchlights.`);
    expect(o.why).toBeNull();
    expect(offer(g, 'work:lights', g.byName['Paris'].id)!.why).toMatch(/not in your hands/);
    expect(offer(g, 'work:lights', -1)!.why).toMatch(/Pick one of your own provinces/);
  });

  it('costs 2, 3 and 5, is delivered in a month, bills 0.015 a level and stops at level 3', () => {
    const g = grand(true);
    const r = g.byName['Hamburg'];
    const before = upkeepTotal(g, 'GER');
    for (let lv = 1; lv <= LIGHTS_TOP; lv++) {
      const o = offer(g, 'work:lights', r.id)!;
      expect(o.size).toBe(`level ${lv}`);
      expect(o.cost).toBe([0, 2, 3, 5][lv]);
      expect(o.months).toBe(1);
      expect(o.upkeep).toBe(0.015);
      const pp = g.nations.GER.pp;
      const b = orderArsenal(g, 'work:lights', r.id)!;
      expect(b.item).toBe('lights');
      expect(b.eta).toBe(g.turn + 1);
      expect(b.name).toBe(`Searchlights, level ${lv}, at ${r.name}`);
      expect(g.nations.GER.pp).toBeCloseTo(pp - o.cost, 9);
      expect(lightsAt(g, r.id)).toBe(lv - 1);
      nextMonth(g);
      expect(lightsAt(g, r.id)).toBe(lv);
      expect(g.headlines.some((h) => h.text === `The searchlights at ${r.name} are in place: level ${lv}.`)).toBe(true);
    }
    expect(offer(g, 'work:lights', r.id)!.why).toBe(`The searchlights at ${r.name} are as many as they can be.`);
    expect(orderArsenal(g, 'work:lights', r.id)).toBeNull();
    expect(upkeepTotal(g, 'GER') - before).toBeCloseTo(3 * 0.015, 9);
    const line = ledger(g, 'GER').upkeep.find((l) => l.key === 'works:lights')!;
    expect(line).toMatchObject({ label: 'Searchlights', count: 3 });
    expect(line.amount).toBeCloseTo(0.045, 9);
    // The guns' own line is untouched by them.
    expect(ledger(g, 'GER').upkeep.find((l) => l.key === 'works:aa')?.count ?? 0).toBe(Object.keys(g.flags).filter((k) => k.startsWith('wk:aa:') && g.regions[+k.slice(6)].ctrl === 'GER').reduce((n, k) => n + g.flags[k], 0));
  });

  it('counts the levels on order, and gives the money back in the month it was ordered', () => {
    const g = grand(true);
    const r = g.byName['Hamburg'];
    const pp = g.nations.GER.pp;
    const a = orderArsenal(g, 'work:lights', r.id)!;
    const o = offer(g, 'work:lights', r.id)!;
    expect(o.size).toBe('level 2');
    expect(o.cost).toBe(3);
    expect(o.note).toBe(`${r.name} has no searchlights, level 1 building.`);
    expect(cancelBuild(g, 'GER', a.id)).toBe(true);
    expect(g.nations.GER.pp).toBeCloseTo(pp, 9);
    expect(g.nations.GER.queue).toHaveLength(0);
    // Lost with the ground before it was finished: not built.
    const b = orderArsenal(g, 'work:lights', r.id)!;
    r.ctrl = 'UK';
    nextMonth(g);
    expect(workLevel(g, 'lights', r.id)).toBe(0);
    expect(b.item).toBe('lights');
  });

  it('reads as searchlights in the order book', () => {
    const g = grand(true);
    const r = g.byName['Hamburg'];
    orderArsenal(g, 'work:lights', r.id);
    const line = cityOrders(g, 'GER').find((c) => c.region === r.id)!.lines.find((l) => l.sign === 'lights')!;
    expect(line.label).toBe('searchlights');
    expect(line.plural).toBe('searchlights');
    expect(line.mark).toBe('');
  });
});

describe('city defences turned off in a campaign under way', () => {
  it('stops the offer, the upkeep and every read, and leaves the mark on the ground', () => {
    const g = grand(true);
    const r = g.byName['Hamburg'];
    orderArsenal(g, 'work:lights', r.id);
    nextMonth(g);
    expect(lightsAt(g, r.id)).toBe(1);
    const pending = orderArsenal(g, 'work:lights', r.id)!;
    setCityDefences(g, false);
    expect(offer(g, 'work:lights', r.id)).toBeUndefined();
    expect(lightsAt(g, r.id)).toBe(0);
    expect(worksUpkeep(g, 'GER').some((l) => l.key === 'works:lights')).toBe(false);
    expect(g.flags[`wk:lights:${r.id}`]).toBe(1);
    // The order paid for waits in the book with the switch off: no mark is written.
    nextMonth(g);
    expect(g.flags[`wk:lights:${r.id}`]).toBe(1);
    expect(g.nations.GER.queue.some((b: Build) => b.id === pending.id)).toBe(true);
    // On again: the level stands, and the order waiting is delivered.
    setCityDefences(g, true);
    expect(lightsAt(g, r.id)).toBe(1);
    nextMonth(g);
    expect(lightsAt(g, r.id)).toBe(2);
    expect(g.nations.GER.queue.some((b: Build) => b.id === pending.id)).toBe(false);
  });
});

describe('city defences: the history start', () => {
  it('lays a table only with the switch on, by province, keeping the higher level', () => {
    const g = grand(true);
    const ham = g.byName['Hamburg'], ber = g.byName['Berlin'];
    g.flags[`wk:lights:${ber.id}`] = 3;
    const row = (region: string, level: number, why: string) => ({ region, turns: [1], level, mark: 'L' as const, lay: true, why });
    openCityDefences(g, [
      row('Hamburg', 2, 'a test row'),
      row('Berlin', 1, 'a test row: the higher level stays'),
      row('Nowhere at all', 3, 'a test row: no such province'),
      row('Bremen', 9, 'a test row: clamped to the top'),
    ]);
    expect(lightsAt(g, ham.id)).toBe(2);
    expect(lightsAt(g, ber.id)).toBe(3);
    expect(lightsAt(g, g.byName['Bremen'].id)).toBe(LIGHTS_TOP);
  });

  it('opens a campaign with the switch set where stage.json sets it: off, nothing laid', () => {
    const g = startOpen(createGame(3, 'GER'));
    expect(lightsFlags(g)).toEqual([]);
  });
});

// ---- Slice 2: the guns by night, the night fighters' floor, the raid card ----------------------------------------

// A zone's sky forced for every month, for a test, and put back after (as tests/weather-127.test.ts does).
const keptSky: [number, [number, number, number][]][] = [];
afterEach(() => { while (keptSky.length) { const [i, o] = keptSky.pop()!; WEATHER.zones[i].odds = o; } });

interface Night { on?: boolean; guns?: number; lights?: number; radar?: boolean; bombers?: number; fighters?: boolean; cloud?: boolean; ac?: boolean; turn?: number }
/**
 * The RAF's bombers by night over the Ruhr in August 1940 (turn 12): one group of `bombers`, and the German fighters
 * kept only when asked for. `radar`: Germany holds Fighter control. `cloud`: weather on, cloud over the Ruhr.
 */
function ruhrByNight(o: Night = {}): { g: Game; t: number; f: AirFleet; a: () => AirAssess } {
  const g = startOpen(createGame(7, null));
  const t = g.byName.Ruhr.id;
  if (o.cloud) {
    const i = zoneOfRegion(g, t);
    keptSky.push([i, WEATHER.zones[i].odds]);
    WEATHER.zones[i].odds = Array.from({ length: 12 }, () => [0, 10, 0] as [number, number, number]);
    setWeather(g, true);
  }
  g.turn = o.turn ?? 12;
  switchBare(g, o.on ?? true);
  if (o.ac) g.flags.ac = 1;
  const f = g.airFleets.find((x) => x.nat === 'UK' && x.bmr > 0 && x.ship === undefined)!;
  f.bmr = o.bombers ?? 300;
  f.ftr = 0;
  if (!o.fighters) for (const x of g.airFleets) if (x.nat === 'GER') x.ftr = 0;
  // (The Ruhr opens with level 1 guns: the test sets its own, or none.)
  if (o.guns) g.flags[`wk:aa:${t}`] = o.guns; else delete g.flags[`wk:aa:${t}`];
  if (o.lights) g.flags[`wk:lights:${t}`] = o.lights;
  if (o.radar) techOf(g, 'GER')!.done.push(AC.radar.control);
  return { g, t, f, a: () => assessAir(g, [f], 'night', t) };
}
/** The built guns' own term in a night raid with no fighters: the bombers lost less the standing flak's (0.01 x 0.7). */
const gunsTerm = (a: AirAssess, cloud = 1): number => a.gunLoss - 0.01 * cloud * 0.7;
const line = (a: AirAssess, label: string) => a.lines.find((l) => l.label === label);

describe('city defences: the guns by night (slice 2)', () => {
  it('gives the worked figures of the ruling: 300 bombers, level 3 guns, a clear sky', () => {
    expect(gunsTerm(ruhrByNight({ guns: 3 }).a())).toBeCloseTo(0.00504, 9);
    expect(gunsTerm(ruhrByNight({ guns: 3, lights: 3 }).a())).toBeCloseTo(0.00504, 9);
    expect(gunsTerm(ruhrByNight({ guns: 3, radar: true }).a())).toBeCloseTo(0.01134, 9);
    expect(gunsTerm(ruhrByNight({ guns: 3, lights: 3, radar: true }).a())).toBeCloseTo(0.0189, 9);
    expect(gunsTerm(ruhrByNight({ guns: 3, lights: 3, radar: true, bombers: 900 }).a())).toBeCloseTo(0.01386, 9);
    const r = ruhrByNight({ guns: 3, lights: 1, radar: true });
    expect(cityNightShare(r.g, r.t, 100, 'clear')).toBeCloseTo((2 * 0.45 + 0.75) / 3, 12);
    expect(cityNightShare(r.g, r.t, 100, 'clear')).toBeCloseTo(0.55, 12);
    // The bombers' loss is the guns' and the standing flak's alone here, so the card's two figures agree with it.
    const a = ruhrByNight({ guns: 3, lights: 3, radar: true }).a();
    expect(a.bomberLoss).toBeCloseTo(a.gunLoss, 12);
  });

  it('a blind radar lays no gun: the guns fire as in a town with none, by the dark alone (the last parameter, off unless asked)', () => {
    const r = ruhrByNight({ guns: 3, lights: 2, radar: true });
    const seen = cityNight(r.g, r.t, 200, 'clear'), blind = cityNight(r.g, r.t, 200, 'clear', true);
    expect(seen.radar).toBe(true);
    expect(seen.litShare).toBeGreaterThan(0);
    expect(blind.radar).toBe(false);
    expect(blind.litShare).toBe(0);
    // No radar: the dark cell and the lit cell with no radar are both 0.20, so the lights add nothing to the guns.
    expect(blind.share).toBeCloseTo(0.2, 12);
    expect(blind.share).toBeLessThan(seen.share);
    expect(cityNightShare(r.g, r.t, 200, 'clear', true)).toBe(blind.share);
    // The very figures of the same town with no radar built.
    const none = ruhrByNight({ guns: 3, lights: 2 });
    expect(blind).toEqual(cityNight(none.g, none.t, 200, 'clear'));
    // The guns, the lights and the sky are read as they stand, and nothing changes for a caller who does not ask.
    expect([blind.guns, blind.lights, blind.lit, blind.sat]).toEqual([seen.guns, seen.lights, seen.lit, seen.sat]);
    expect(cityNight(r.g, r.t, 200, 'clear', false)).toEqual(seen);
    expect(cityNightShare(r.g, r.t, 200, 'clear')).toBe(seen.share);
    expect(cityNightShare(r.g, r.t, 200, 'cloud', true)).toBeCloseTo(0.2, 12);
  });

  it('lets the lights do less of their work against more bombers than they can hold', () => {
    const r = ruhrByNight({ guns: 3, lights: 2, radar: true });
    expect(cityNight(r.g, r.t, 200, 'clear').sat).toBe(1);
    expect(cityNight(r.g, r.t, 400, 'clear').sat).toBeCloseTo(0.5, 12);
    expect(cityNight(r.g, r.t, 400, 'clear').share).toBeCloseTo((1 * 0.45 + 2 * (0.45 + 0.3 * 0.5)) / 3, 12);
    expect(cityNight(r.g, r.t, 400, 'clear').litShare).toBeCloseTo((2 * 0.5) / 3, 12);
    const a = ruhrByNight({ guns: 3, lights: 3, radar: true, bombers: 900 }).a();
    expect(line(a, 'Searchlights')).toEqual({ label: 'Searchlights', value: 'More bombers than the lights can hold', effect: 'About 900 bombers over Ruhr, and a beam holds one at a time: the lights do about 33% of their work.' });
    expect(a.lights).toEqual({ level: 3, cloud: false, lit: 1 / 3 });
    expect(line(a, 'Anti-aircraft guns')!.effect).toBe('They stop 45% of the bombing. By night they fire by radar and searchlight, and bring down about 55% as many bombers as they would by day.');
  });

  it('under cloud: the lights are dark, the floor is off, and the radar share stays', () => {
    const a = ruhrByNight({ guns: 3, lights: 3, radar: true, cloud: true }).a();
    expect(a.sky).toBe('cloud');
    expect(a.lights).toEqual({ level: 3, cloud: true, lit: 0 });
    // Cloud keeps its own price beside the table (weather, ruling 127: 0.5 by night).
    expect(gunsTerm(a, 0.5)).toBeCloseTo(3 * 0.012 * 0.7 * 0.5 * 0.45, 12);
    expect(line(a, 'Searchlights')).toEqual({ label: 'Searchlights', value: 'Cloud over Ruhr', effect: 'The searchlights cannot see the bombers through it.' });
    expect(line(a, 'Anti-aircraft guns')!.effect).toBe('They stop 45% of the bombing. By night they fire by radar, and bring down about 45% as many bombers as they would by day.');
    // No floor for the night fighters under cloud: the same fighters meet the raid, lights or none.
    const lit = ruhrByNight({ lights: 3, cloud: true, fighters: true, ac: true }).a(), dark = ruhrByNight({ cloud: true, fighters: true, ac: true }).a();
    expect(dark.D).toBeGreaterThan(0);
    expect(lit.D).toBe(dark.D);
  });

  it('gives the night fighters a floor, never a multiplier, with lights and neither guns nor radar', () => {
    // Air campaigns on, no Fighter control: Germany's dated row is its first, 0.001, under the level 3 floor 0.004.
    const dark = ruhrByNight({ fighters: true, ac: true }).a();
    const lit = ruhrByNight({ lights: 3, fighters: true, ac: true }).a();
    expect(dark.D).toBeGreaterThan(0);
    expect(lit.D).toBeCloseTo(dark.D * (0.004 / 0.001), 9);
    expect(ruhrByNight({ lights: 1, fighters: true, ac: true }).a().D).toBeCloseTo(dark.D * (0.002 / 0.001), 9);
    // Fighter control in 1943: the dated row (0.022 from turn 41) is above every floor, and the lights add nothing.
    const row = ruhrByNight({ fighters: true, ac: true, radar: true, turn: 41 }).a();
    expect(ruhrByNight({ lights: 3, fighters: true, ac: true, radar: true, turn: 41 }).a().D).toBe(row.D);
    // Release 1's flat 0.03 likewise.
    expect(ruhrByNight({ lights: 3, fighters: true }).a().D).toBe(ruhrByNight({ fighters: true }).a().D);
    // And with no guns the card says what they do.
    expect(line(lit, 'Searchlights')).toEqual({ label: 'Searchlights', value: 'Level 3 searchlights over Ruhr', effect: 'There are no guns for them to light. They help the night fighters a little.' });
    expect(line(lit, 'Anti-aircraft guns')).toBeUndefined();
    expect(lit.lights).toEqual({ level: 3, cloud: false, lit: 0 });
  });

  it('replaces the night guns of ruling 107(b) rather than halving them again', () => {
    const r = ruhrByNight({ guns: 3 });
    setAirPiece(r.g, 'night', true);
    expect(gunsTerm(r.a())).toBeCloseTo(0.00504, 9);
    const both = ruhrByNight({ guns: 3, lights: 3, radar: true });
    setAirPiece(both.g, 'night', true);
    expect(gunsTerm(both.a())).toBeCloseTo(0.0189, 9);
    // By day the switch moves nothing: the guns are worth their day figure.
    const day = ruhrByNight({ guns: 3, lights: 3, radar: true }), off = ruhrByNight({ guns: 3, lights: 3, radar: true, on: false });
    expect(assessAir(day.g, [day.f], 'bomb', day.t)).toEqual(assessAir(off.g, [off.f], 'bomb', off.t));
  });

  it('words the searchlights line in each of its forms, and the guns by night in theirs', () => {
    const some = ruhrByNight({ guns: 3, lights: 2, radar: true, bombers: 200 }).a();
    expect(line(some, 'Searchlights')).toEqual({ label: 'Searchlights', value: 'Level 2 searchlights over Ruhr', effect: 'With radar laying the guns, they light 2 of the 3 levels of guns, and they help the night fighters a little.' });
    expect(line(some, 'Anti-aircraft guns')).toEqual({ label: 'Anti-aircraft guns', value: 'Level 3 guns ring the factories of Ruhr', effect: `They stop 45% of the bombing. By night they fire by radar and searchlight, and bring down about ${Math.round(((1 * 0.45 + 2 * 0.75) / 3) * 100)}% as many bombers as they would by day.` });
    expect(some.lights).toEqual({ level: 2, cloud: false, lit: 2 / 3 });
    const all = ruhrByNight({ guns: 2, lights: 3, radar: true }).a();
    expect(line(all, 'Searchlights')!.effect).toBe('With radar laying the guns, they light every level of the guns, and they help the night fighters a little.');
    expect(all.lights!.lit).toBe(1);
    const blind = ruhrByNight({ guns: 3, lights: 3 }).a();
    expect(line(blind, 'Searchlights')).toEqual({ label: 'Searchlights', value: 'Level 3 searchlights over Ruhr', effect: "Without Fighter control's radar to lay the guns, they help only the night fighters, a little." });
    expect(line(blind, 'Anti-aircraft guns')!.effect).toBe('They stop 45% of the bombing. By night they fire blind, and bring down about 20% as many bombers as they would by day.');
    expect(blind.lights).toEqual({ level: 3, cloud: false, lit: 0 });
    // No lights: no searchlights line, the guns' night clause, and the reading at level 0.
    const none = ruhrByNight({ guns: 1 }).a();
    expect(line(none, 'Searchlights')).toBeUndefined();
    expect(line(none, 'Anti-aircraft guns')!.effect).toBe('They stop 15% of the bombing. By night they fire blind, and bring down about 20% as many bombers as they would by day.');
    expect(none.lights).toEqual({ level: 0, cloud: false, lit: 0 });
    // By day nothing of it.
    const r = ruhrByNight({ guns: 3, lights: 3, radar: true });
    const day = assessAir(r.g, [r.f], 'bomb', r.t);
    expect(line(day, 'Searchlights')).toBeUndefined();
    expect(line(day, 'Anti-aircraft guns')!.effect).toBe('They stop 45% of the bombing and bring down more of the bombers');
    expect('lights' in day).toBe(false);
  });

  it('names the searchlights after the guns on the bomb and night targets, only with the switch on', () => {
    for (const on of [true, false]) {
      const { g, t } = ruhrByNight({ on, guns: 3, lights: 2 });
      for (const kind of ['night', 'bomb'] as const) {
        const what = raidTargets(g, kind, 'UK').find((x) => x.id === t)!.what;
        expect(what).toMatch(on ? /level 3 guns, level 2 searchlights$/ : /level 3 guns$/);
      }
    }
  });

  it('flies as the card says: the report keeps the card and the searchlights\' reading', () => {
    const r = ruhrByNight({ guns: 3, lights: 2, radar: true, fighters: true, ac: true, bombers: 200 });
    const a = r.a();
    const rep = fly(r.g, 'UK', 'night', r.t, [r.f], ':cd2')!;
    expect(rep.lights).toEqual(a.lights);
    expect(rep.lights).toEqual({ level: 2, cloud: false, lit: 2 / 3 });
    expect(rep.lines).toEqual(a.lines);
    // The card's guns figure is the result's: the guns' term and the line's share are one figure.
    const share = cityNightShare(r.g, r.t, a.B, a.sky);
    expect(line(a, 'Anti-aircraft guns')!.effect).toContain(`about ${Math.round(share * 100)}% as many bombers`);
    expect(a.gunLoss).toBeCloseTo((0.01 + 3 * 0.012 * share) * 0.7, 12);
    expect(line(a, 'Flak')!.effect).toContain(`about ${Math.round(a.gunLoss * 100)}%`);
  });
});

describe('city defences off: the night raid is as it was (slice 2)', () => {
  it('reads no searchlights the ground keeps, no Fighter control, no floor: the assessment and the report are unchanged', () => {
    for (const o of [{ guns: 3 }, { guns: 3, radar: true }, { guns: 2, fighters: true, ac: true }, { guns: 3, radar: true, fighters: true, ac: true, turn: 41 }, { guns: 3, cloud: true }]) {
      const plain = ruhrByNight({ ...o, on: false });
      const marked = ruhrByNight({ ...o, on: false, lights: 3 });
      const a = marked.a();
      expect(a).toEqual(plain.a());
      expect('lights' in a).toBe(false);
      expect(line(a, 'Searchlights')).toBeUndefined();
      expect(line(a, 'Anti-aircraft guns')!.effect).toMatch(/and bring down more of the bombers$/);
      const p = fly(plain.g, 'UK', 'night', plain.t, [plain.f], ':off')!, m = fly(marked.g, 'UK', 'night', marked.t, [marked.f], ':off')!;
      expect('lights' in m).toBe(false);
      expect(m).toEqual(p);
    }
    // Today's figure, as shipped: the guns by night as by day, times the night's 0.7.
    expect(ruhrByNight({ on: false, guns: 3, lights: 3 }).a().gunLoss).toBeCloseTo((0.01 + 3 * 0.012) * 0.7, 12);
  });
});

// ---- Slice 3: a garrison fortifies its town -------------------------------------------------------------------------

/** A grand campaign with the player's first month open, as the game's own screen would have it (tests/siege.test.ts). */
function playingGer(on: boolean): Game {
  const g = startOpen(createGame(3, 'GER'));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  setCityDefences(g, on);
  return g;
}
/**
 * Germany in September 1939 with Lodz (a town) held against her: one Polish corps inside the town, nobody in the
 * field, no forts. `inside: false` leaves the town to its guard alone.
 */
function lodzTown(on: boolean, o: { inside?: boolean } = {}): { g: Game; town: Region; inside: Corps | null; ger: Corps[] } {
  const g = playingGer(on);
  const town = g.byName['Lodz'];
  town.fort = null;
  let inside: Corps | null = null;
  if (o.inside !== false) {
    inside = town.units.find((u) => u.nat === 'POL' && u.kind === 'inf')!;
    expect(garrison(g, inside)).toBe(true);
  }
  for (const u of [...town.units]) relocate(g, u, g.byName['Warsaw']);
  const ger = town.nb.flatMap((j) => g.regions[j].units.filter((u) => u.nat === 'GER' && u.kind !== 'fort' && u.kind !== 'eng' && u.ap > 0));
  return { g, town, inside, ger };
}
const natIdx = (g: Game, nat: string): number => Object.keys(g.nations).indexOf(nat);
/** Lays town works by hand, as a garrison would have put them up for `nat`. */
const layWorks = (g: Game, r: Region, months: number, nat = r.ctrl): void => { g.flags[`tw:${r.id}`] = months; g.flags[`twn:${r.id}`] = natIdx(g, nat); };
/** The town works' marks of one province. */
const worksFlags = (g: Game, r: Region): string[] => Object.keys(g.flags).filter((k) => k === `tw:${r.id}` || k === `twn:${r.id}` || k === `tws:${r.id}`);

describe('city defences: a garrison fortifies its town (slice 3)', () => {
  it('a garrison puts up a month of works at each of its month ends, to three, besieged or not', () => {
    const { g, town } = lodzTown(true);
    expect(townWorksAt(g, town)).toBe(0);
    const bare = townBonus(g, town);
    for (const m of [1, 2, 3, 3]) {
      townWorksMonth(g, 'POL');
      expect(townWorksAt(g, town)).toBe(m);
      expect(g.flags[`twn:${town.id}`]).toBe(natIdx(g, 'POL'));
    }
    expect(townBonus(g, town)).toBeCloseTo(bare + 0.3, 9);
    // Another nation's month end does nothing for Poland's town.
    const other = lodzTown(true);
    townWorksMonth(other.g, 'GER');
    expect(townWorksAt(other.g, other.town)).toBe(0);
    // Under siege it still grows (ruling choice 7).
    const s = lodzTown(true);
    s.ger[0].str = 0.2;
    orderMove(s.g, s.ger[0], s.town.id);
    expect(besieged(s.g, s.town)).toBe(true);
    townWorksMonth(s.g, 'POL');
    townWorksMonth(s.g, 'POL');
    expect(townWorksAt(s.g, s.town)).toBe(2);
  });

  it('grows only for the holder\'s own corps: a garrison of an ally\'s corps alone puts up nothing, and the works fade', () => {
    const { g, town, inside } = lodzTown(true);
    // Lodz held by Poland with only another nation's living corps inside.
    inside!.nat = 'GBR';
    expect(town.ctrl).toBe('POL');
    expect(town.garr!.some((c) => !c.dead)).toBe(true);
    townWorksMonth(g, 'POL');
    expect(townWorksAt(g, town)).toBe(0);
    expect(worksFlags(g, town)).toEqual([]);
    layWorks(g, town, 2);
    townWorksMonth(g, 'POL');
    expect(townWorksAt(g, town)).toBe(1);
    // One of his own corps inside, and it grows again.
    inside!.nat = 'POL';
    townWorksMonth(g, 'POL');
    expect(townWorksAt(g, town)).toBe(2);
  });

  it('gains nothing at the month end after a storm, won or lost, and grows again the month after', () => {
    const { g, town, inside, ger } = lodzTown(true);
    layWorks(g, town, 1);
    inside!.str = 1; inside!.org = 1;
    const a = ger[0];
    a.str = 0.25;
    orderMove(g, a, town.id);
    const rep = orderStorm(g, town.id)!;
    expect(rep.taken).toBe(false);
    expect(g.flags[`tws:${town.id}`]).toBe(g.turn);
    townWorksMonth(g, 'POL');
    expect(townWorksAt(g, town)).toBe(1);
    expect(g.flags[`tws:${town.id}`]).toBeUndefined();
    townWorksMonth(g, 'POL');
    expect(townWorksAt(g, town)).toBe(2);
  });

  it('fades a month a month with no garrison inside, and is gone at nought', () => {
    const { g, town } = lodzTown(true, { inside: false });
    layWorks(g, town, 3);
    for (const m of [2, 1]) { townWorksMonth(g, 'POL'); expect(townWorksAt(g, town)).toBe(m); }
    townWorksMonth(g, 'POL');
    expect(townWorksAt(g, town)).toBe(0);
    expect(worksFlags(g, town)).toEqual([]);
  });

  it('is cleared when the town falls by storm, opens its gates or starves, and counts for nobody else', () => {
    // By storm.
    const st = lodzTown(true);
    layWorks(st.g, st.town, 3);
    st.inside!.str = 0.3; st.inside!.org = 0.3;
    for (const c of st.ger.slice(0, 4)) orderMove(st.g, c, st.town.id);
    expect(orderStorm(st.g, st.town.id)!.taken).toBe(true);
    expect(st.town.ctrl).toBe('GER');
    expect(st.g.flags[`tw:${st.town.id}`]).toBeUndefined();
    expect(st.g.flags[`twn:${st.town.id}`]).toBeUndefined();
    // The stormers' own month end puts up nothing in the month they stormed it.
    townWorksMonth(st.g, 'GER');
    expect(townWorksAt(st.g, st.town)).toBe(0);
    // Opening its gates to a summons.
    const gt = lodzTown(true, { inside: false });
    layWorks(gt.g, gt.town, 2);
    for (const c of gt.ger.slice(0, 4)) if (gt.town.ctrl !== 'GER') orderMove(gt.g, c, gt.town.id);
    expect(gt.town.ctrl).toBe('GER');
    expect(worksFlags(gt.g, gt.town)).toEqual([]);
    // Starved out.
    const hu = lodzTown(true, { inside: false });
    hu.ger[0].str = 0.2;
    orderMove(hu.g, hu.ger[0], hu.town.id);
    expect(ringClosed(hu.g, hu.town)).toBe(true);
    layWorks(hu.g, hu.town, 2);
    for (let k = 0; k <= storesOf(hu.g, hu.town) && hu.town.ctrl === 'POL'; k++) siegeMonth(hu.g, 'POL');
    expect(hu.town.ctrl).toBe('GER');
    expect(worksFlags(hu.g, hu.town)).toEqual([]);
    // Any other change of holder (here set by hand): the works count for nobody, and the next month end clears them.
    const ho = lodzTown(true);
    layWorks(ho.g, ho.town, 3);
    const before = townBonus(ho.g, ho.town);
    ho.town.ctrl = 'GER';
    expect(townWorksAt(ho.g, ho.town)).toBe(0);
    expect(townWorksWorth(ho.g, ho.town)).toBe(0);
    expect(townBonus(ho.g, ho.town)).toBeCloseTo(before - 0.3, 9);
    townWorksMonth(ho.g, 'ITA');
    expect(worksFlags(ho.g, ho.town)).toEqual([]);
  });

  it('counts the larger of the works and the forts, never both', () => {
    const { g, town } = lodzTown(true);
    const size = SIEGE.bonus[townSize(g, town)] - 1;
    for (const m of [0, 1, 2, 3]) {
      if (m) layWorks(g, town, m); else delete g.flags[`tw:${town.id}`];
      const works = m ? 0.3 * [0.5, 0.75, 1][m - 1] : 0;
      expect(townWorksWorth(g, town)).toBeCloseTo(works, 9);
      town.fort = null;
      expect(townBonus(g, town)).toBeCloseTo(1 + size + works, 9);
      for (const lv of [1, 2, 3]) {
        town.fort = { lv, vs: ['GER'] };
        expect(townBonus(g, town), `${m} months, fort ${lv}`).toBeCloseTo(1 + size + Math.max(SIEGE.fortBonus[lv], works), 9);
      }
    }
    // The guard fighting in the field gets the same larger-of, scaled by the share the forts do not already slow.
    town.fort = { lv: 1, vs: ['GER'] };
    layWorks(g, town, 3);
    expect(fieldWalls(g, town, 0, 0.5)).toBeCloseTo(1 + size + 0.3 * 0.5, 9);
    town.fort = null;
    expect(fieldWalls(g, town, 0, 1)).toBeCloseTo(1 + size + 0.3, 9);
  });

  it('is worn with the walls: a city at three months is 1 + (0.7 + 0.3) x (1 - breach)', () => {
    const { g, town } = lodzTown(true);
    // (Lodz is a town on the map: two victory points make it a city, towns.ts townSize.)
    town.vp = Math.max(town.vp, 2);
    expect(townSize(g, town)).toBe('city');
    layWorks(g, town, 3);
    for (const b of [0, 20, 45, 70]) {
      g.flags[`breach:${town.id}`] = b;
      expect(townBonus(g, town)).toBeCloseTo(1 + (0.7 + 0.3) * (1 - b / 100), 9);
    }
  });

  it('says the months in the town line: the siege panel, the storm card, the raid card and the targets', () => {
    const { g, town, ger } = lodzTown(true);
    layWorks(g, town, 3);
    expect(siegeView(g, town.id)!.lines[0]).toMatch(/^Troops inside Lodz fight 70% stronger, behind 3 months of barricades and strongpoints\. /);
    layWorks(g, town, 1);
    expect(siegeView(g, town.id)!.lines[0]).toMatch(/fight 55% stronger, behind 1 month of barricades and strongpoints\. /);
    town.fort = { lv: 2, vs: ['GER'] };
    expect(siegeView(g, town.id)!.lines[0]).toMatch(/fight 70% stronger, with 1 month of barricades and strongpoints, though its forts count for more\. /);
    town.fort = null;
    layWorks(g, town, 2);
    g.flags[`breach:${town.id}`] = 10;
    for (const c of ger.slice(0, 2)) orderMove(g, c, town.id);
    const look = stormOutlook(g, town.id)!;
    expect(look.lines.find((l) => l.label === 'The town')!.effect).toMatch(/stronger in the streets, behind 2 months of barricades and strongpoints: bombardment has already broken down 10% of its works$/);
    const a = assessAir(g, [g.airFleets.find((x) => x.nat === 'GER' && x.bmr > 0 && x.ship === undefined)!], 'town', town.id);
    expect(a.lines.find((l) => l.label === 'The town')!.value).toMatch(/stronger, behind 2 months of barricades and strongpoints, with 10% of its works already broken down$/);
    g.flags[`wk:aa:${town.id}`] = 2;
    g.flags[`wk:lights:${town.id}`] = 2;
    const t = raidTargets(g, 'town', 'GER').find((x) => x.id === town.id);
    expect(t).toBeDefined();
    expect(t!.what).toMatch(/fight \d+% stronger, behind 2 months of barricades and strongpoints, 10% of its works broken, under siege, level 2 guns, level 2 searchlights$/);
  });

  it('runs at the month end, after the month\'s storms', () => {
    const { g, town } = lodzTown(true);
    endPhase(g, 'POL');
    expect(townWorksAt(g, town)).toBe(1);
  });
});

describe('city defences off: a town is as strong as its walls and forts (slice 3)', () => {
  it('reads no town works the ground keeps, writes none, clears none', () => {
    const on = lodzTown(false);
    const { g, town, ger } = on;
    const bonus = townBonus(g, town), field = fieldWalls(g, town, 0, 1), panel = siegeView(g, town.id)!.lines;
    layWorks(g, town, 3);
    g.flags[`tws:${town.id}`] = g.turn;
    expect(townWorksAt(g, town)).toBe(0);
    expect(townBonus(g, town)).toBe(bonus);
    expect(fieldWalls(g, town, 0, 1)).toBe(field);
    expect(siegeView(g, town.id)!.lines).toEqual(panel);
    townWorksMonth(g, 'POL');
    endPhase(g, 'POL');
    expect(g.flags[`tw:${town.id}`]).toBe(3);
    expect(g.flags[`tws:${town.id}`]).toBe(g.turn);
    const fresh = lodzTown(false);
    endPhase(fresh.g, 'POL');
    fresh.inside!.str = 0.3; fresh.inside!.org = 0.3;
    for (const c of fresh.ger.slice(0, 4)) orderMove(fresh.g, c, fresh.town.id);
    orderStorm(fresh.g, fresh.town.id);
    expect(worksFlags(fresh.g, fresh.town)).toEqual([]);
    // A capture with the switch off leaves the marks where they lie, and nothing reads them.
    for (const c of ger.slice(0, 4)) orderMove(g, c, town.id);
    capture(g, town, 'GER');
    expect(g.flags[`tw:${town.id}`]).toBe(3);
  });
});

// ---- Slice 4: the town's guns against the tanks that storm it -------------------------------------------------------

/**
 * Lodz held by one Polish corps, with the German army besieging it: one tank corps and one infantry corps inside the
 * province, ready to storm. `guns` the built gun levels in the town; `inside: false` leaves it to its guard alone.
 */
function lodzStorm(on: boolean, o: { guns?: number; inside?: boolean } = {}): { g: Game; town: Region; inside: Corps | null; tank: Corps; foot: Corps } {
  const { g, town, inside, ger } = lodzTown(on, { inside: o.inside });
  const foot = ger.find((c) => c.kind === 'inf')!;
  const tank = g.corps.find((c) => c.nat === 'GER' && c.kind === 'arm' && !c.dead)!;
  relocate(g, tank, g.regions[foot.reg]);
  tank.ap = Math.max(tank.ap, 3); foot.ap = Math.max(foot.ap, 3);
  orderMove(g, foot, town.id);
  orderMove(g, tank, town.id);
  expect(tank.reg).toBe(town.id);
  expect(foot.reg).toBe(town.id);
  if (o.guns) g.flags[`wk:aa:${town.id}`] = o.guns;
  return { g, town, inside, tank, foot };
}
const stormA = (g: Game, cs: Corps[], town: Region): number => assess(g, cs, town, { town: true }).A;
const armorLines = (lines: { label: string; value: string; effect?: string }[]) => lines.filter((l) => l.label === 'Armor');

describe('city defences: the town\'s guns against the tanks (slice 4)', () => {
  it('cuts the tanks\' storm by 7% a level on top of the streets\' 75%: x0.93, 0.86, 0.79', () => {
    const { g, town, tank } = lodzStorm(true);
    const bare = stormA(g, [tank], town);
    expect(armorLines(assess(g, [tank], town, { town: true }).lines).map((l) => l.value)).toEqual(['Tanks in the streets of the town']);
    for (const [lv, mul] of [[1, 0.93], [2, 0.86], [3, 0.79]]) {
      g.flags[`wk:aa:${town.id}`] = lv;
      expect(townFlak(g, town)).toBe(lv);
      expect(townFlakCut(g, town)).toBeCloseTo(1 - mul, 9);
      expect(stormA(g, [tank], town) / bare, `level ${lv}`).toBeCloseTo(mul, 9);
      const lines = armorLines(assess(g, [tank], town, { town: true }).lines);
      expect(lines).toEqual([
        { label: 'Armor', value: 'Tanks in the streets of the town', effect: 'Tank attack cut to 75%' },
        { label: 'Armor', value: 'Heavy anti-aircraft guns dug in with the garrison', effect: `Tank attack cut by a further ${Math.round((1 - mul) * 100)}%` },
      ]);
    }
  });

  it('cuts by 25% at most', () => {
    const { g, town, tank } = lodzStorm(true);
    const bare = stormA(g, [tank], town);
    g.flags[`wk:aa:${town.id}`] = 5;
    expect(townFlakCut(g, town)).toBe(CITYDEF.ground.most);
    expect(stormA(g, [tank], town) / bare).toBeCloseTo(0.75, 9);
    expect(flakCut(4)).toBe(0.25);
    expect(flakCutMost()).toBeCloseTo(0.21, 9);
    // The fog's figure is the guns' own top level's cut: the works' top, which the guns' row shares (guard.ts).
    expect(WORKS_TOP).toBe(GUARD_TOP);
    expect(flakCutMost()).toBe(flakCut(GUARD_TOP));
  });

  it('does nothing against infantry, with no garrison, or with no guns', () => {
    const s = lodzStorm(true);
    const foot0 = stormA(s.g, [s.foot], s.town);
    s.g.flags[`wk:aa:${s.town.id}`] = 3;
    expect(stormA(s.g, [s.foot], s.town)).toBe(foot0);
    expect(armorLines(assess(s.g, [s.foot], s.town, { town: true }).lines)).toEqual([]);
    // No living garrison: the guns are there, but nobody has dug them in.
    const n = lodzStorm(true);
    const tank0 = stormA(n.g, [n.tank], n.town);
    n.g.flags[`wk:aa:${n.town.id}`] = 3;
    n.inside!.dead = true;
    expect(townFlak(n.g, n.town)).toBe(0);
    expect(townFlakCut(n.g, n.town)).toBe(0);
    n.inside!.dead = false;
    expect(townFlak(n.g, n.town)).toBe(3);
    delete n.g.flags[`wk:aa:${n.town.id}`];
    expect(townFlak(n.g, n.town)).toBe(0);
    expect(stormA(n.g, [n.tank], n.town)).toBe(tank0);
    expect(armorLines(assess(n.g, [n.tank], n.town, { town: true }).lines)).toHaveLength(1);
  });

  it('the card says the figure the dice use, and the staff\'s forecast and the battle agree', () => {
    const { g, town, tank, foot } = lodzStorm(true, { guns: 2 });
    const a = assess(g, [tank, foot], town, { town: true });
    const line = armorLines(a.lines)[1];
    expect(line.effect).toBe(`Tank attack cut by a further ${Math.round(townFlakCut(g, town) * 100)}%`);
    expect(line.effect).toBe('Tank attack cut by a further 14%');
    // Identified: the outlook shows the line as the assessment has it.
    for (const c of town.garr!) (g.intel.known ??= {})[c.id] = g.turn;
    const look = stormOutlook(g, town.id)!;
    expect(armorLines(look.lines)).toContainEqual(line);
    // The battle is fought at the assessment's figures.
    const rep = orderStorm(g, town.id)!;
    expect(rep.aPower).toBeCloseTo(a.A, 9);
    expect(armorLines(rep.lines)).toContainEqual(line);
  });

  it('says only what the guns could take where the garrison is not identified', () => {
    const { g, town } = lodzStorm(true, { guns: 1 });
    for (const c of town.garr!) delete g.intel.known?.[c.id];
    const look = stormOutlook(g, town.id)!;
    expect(look.exact).toBe(false);
    expect(armorLines(look.lines)).toContainEqual({ label: 'Armor', value: 'The garrison may have heavy anti-aircraft guns dug in', effect: 'Tank attack cut by as much as 21% if it has' });
    expect(armorLines(look.lines).some((l) => /dug in with the garrison/.test(l.value))).toBe(false);
  });

  it('loses a level of the guns when the town falls by storm, and says so on the report', () => {
    const { g, town, inside } = lodzStorm(true, { guns: 2 });
    inside!.str = 0.2; inside!.org = 0.2;
    const rep = orderStorm(g, town.id)!;
    expect(rep.taken).toBe(true);
    expect(g.flags[`wk:aa:${town.id}`]).toBe(1);
    expect(g.flags[`aal:${town.id}`]).toBe(g.turn);
    expect(rep.lines).toContainEqual({ label: 'Anti-aircraft guns', value: 'The anti-aircraft guns of Lodz fought the tanks over open sights, and one level of them was lost with the town.' });
    // At level 1 the guns are gone with it.
    const one = lodzStorm(true, { guns: 1 });
    one.inside!.str = 0.2; one.inside!.org = 0.2;
    expect(orderStorm(one.g, one.town.id)!.taken).toBe(true);
    expect(`wk:aa:${one.town.id}` in one.g.flags).toBe(false);
  });

  // (The design director, 25 Nov 2026 12:18Z: the gun level is lost only where the flak line was on the card.)
  it('keeps the guns of a town stormed by infantry alone: they played no part, so nothing is charged and no line printed', () => {
    const { g, town, inside, foot } = lodzStorm(true, { guns: 2 });
    inside!.str = 0.2; inside!.org = 0.2;
    expect(townFlak(g, town)).toBe(2);
    expect(assess(g, [foot], town, { town: true }).lines.some(isFlakLine)).toBe(false);
    const rep = orderStorm(g, town.id, [foot])!;
    expect(rep.taken).toBe(true);
    expect(town.ctrl).toBe('GER');
    expect(g.flags[`wk:aa:${town.id}`]).toBe(2);
    expect(`aal:${town.id}` in g.flags).toBe(false);
    expect(rep.lines.some((l) => l.label === 'Anti-aircraft guns' || /anti-aircraft/.test(l.value))).toBe(false);
    // The same town stormed with tanks: the card carries the guns' line, and a level is lost, with the line and the mark.
    const t = lodzStorm(true, { guns: 2 });
    t.inside!.str = 0.2; t.inside!.org = 0.2;
    expect(assess(t.g, [t.tank, t.foot], t.town, { town: true }).lines.some(isFlakLine)).toBe(true);
    const tr = orderStorm(t.g, t.town.id, [t.tank, t.foot])!;
    expect(tr.taken).toBe(true);
    expect(tr.lines.some(isFlakLine)).toBe(true);
    expect(t.g.flags[`wk:aa:${t.town.id}`]).toBe(1);
    expect(t.g.flags[`aal:${t.town.id}`]).toBe(t.g.turn);
    expect(tr.lines).toContainEqual({ label: 'Anti-aircraft guns', value: 'The anti-aircraft guns of Lodz fought the tanks over open sights, and one level of them was lost with the town.' });
  });

  it('keeps the guns when the storm is thrown back, when the town had no garrison, or when it falls otherwise', () => {
    // Thrown back.
    const b = lodzStorm(true, { guns: 2 });
    b.inside!.str = 1; b.inside!.org = 1;
    const rep = orderStorm(b.g, b.town.id, [b.tank])!;
    expect(rep.taken).toBe(false);
    expect(b.g.flags[`wk:aa:${b.town.id}`]).toBe(2);
    expect(`aal:${b.town.id}` in b.g.flags).toBe(false);
    expect(rep.lines.some((l) => l.label === 'Anti-aircraft guns')).toBe(false);
    // A town with no garrison or no guns as the storm began pays nothing.
    const n = lodzStorm(true, { guns: 2 });
    expect(flakLostWithTown(n.g, n.town, 0, 'Lodz')).toBeNull();
    expect(n.g.flags[`wk:aa:${n.town.id}`]).toBe(2);
    expect(`aal:${n.town.id}` in n.g.flags).toBe(false);
    // Starved out.
    const hu = lodzTown(true, { inside: false });
    hu.g.flags[`wk:aa:${hu.town.id}`] = 2;
    hu.ger[0].str = 0.2;
    orderMove(hu.g, hu.ger[0], hu.town.id);
    for (let k = 0; k <= storesOf(hu.g, hu.town) && hu.town.ctrl === 'POL'; k++) siegeMonth(hu.g, 'POL');
    expect(hu.town.ctrl).toBe('GER');
    expect(hu.g.flags[`wk:aa:${hu.town.id}`]).toBe(2);
    expect(`aal:${hu.town.id}` in hu.g.flags).toBe(false);
  });
});

describe('city defences off: the town\'s guns do not fight tanks (slice 4)', () => {
  it('reads no guns in a storm, cuts nothing, loses nothing, writes nothing', () => {
    const { g, town, tank, foot, inside } = lodzStorm(false);
    const a0 = assess(g, [tank, foot], town, { town: true });
    g.flags[`wk:aa:${town.id}`] = 3;
    expect(townFlak(g, town)).toBe(0);
    const a1 = assess(g, [tank, foot], town, { town: true });
    expect(a1.A).toBe(a0.A);
    expect(a1.lines).toEqual(a0.lines);
    inside!.str = 0.2; inside!.org = 0.2;
    const rep = orderStorm(g, town.id)!;
    expect(rep.taken).toBe(true);
    expect(g.flags[`wk:aa:${town.id}`]).toBe(3);
    expect(`aal:${town.id}` in g.flags).toBe(false);
    expect(rep.lines.some((l) => l.label === 'Anti-aircraft guns' || /anti-aircraft/.test(l.value))).toBe(false);
  });
});

// ---- Slice 5: the computer, the bridge, the Options row, the history start's rows ---------------------------------

/** A September 1939 grand campaign with the switch on, its start laid as startOpen lays it under the switch. */
function opened(on: boolean, from?: number): Game {
  const g = startOpen(createGame(3, null));
  if (from !== undefined) g.flags.openFrom = from;
  setCityDefences(g, on);
  openCityDefences(g);
  return g;
}
const lightsIn = (g: Game, name: string): number => lightsAt(g, g.byName[name].id);

describe('city defences: the history start\'s rows (slice 5)', () => {
  it('names only provinces on the map, every row laid, levels 1 to 3, a mark and a source each', () => {
    const g = startOpen(createGame(3, null));
    expect(CITYDEF.start.length).toBeGreaterThan(0);
    for (const row of CITYDEF.start) {
      expect(g.byName[row.region], row.region).toBeDefined();
      expect(row.lay, row.region).toBe(true);
      expect(['F', 'L', 'D', 'U']).toContain(row.mark);
      expect(row.level).toBeGreaterThanOrEqual(1);
      expect(row.level).toBeLessThanOrEqual(LIGHTS_TOP);
      expect(row.turns.length).toBeGreaterThan(0);
      expect(row.why.length).toBeGreaterThan(0);
    }
    // One reading for each province and start month.
    const seen = new Set<string>();
    for (const row of CITYDEF.start) for (const t of row.turns) { const k = `${row.region}:${t}`; expect(seen.has(k), k).toBe(false); seen.add(k); }
  });

  it('a September 1939 campaign with the switch on opens with London at 2, Berlin at 1, and no lights in Paris, Rome or Washington', () => {
    const g = opened(true);
    expect(lightsIn(g, 'London')).toBe(2);
    expect(lightsIn(g, 'Berlin')).toBe(1);
    expect(lightsIn(g, 'Ruhr')).toBe(1);
    expect(lightsIn(g, 'Kent')).toBe(2);
    expect(lightsIn(g, 'Ulster')).toBe(1);
    // Hamburg gains guns by the record, so its lights follow by the same ratio (the design director, 25 Nov 13:06Z).
    expect(lightsIn(g, 'Hamburg')).toBe(1);
    for (const none of ['Paris', 'Rome', 'Washington', 'Moscow', 'Bohemia', 'Cornwall', 'Liege']) expect(lightsIn(g, none), none).toBe(0);
  });

  it('with the switch off no searchlights mark exists at the start', () => {
    const g = opened(false);
    expect(lightsFlags(g)).toEqual([]);
    expect(lightsFlags(startOpen(createGame(3, null)))).toEqual([]);
  });

  it('a later start lays the reading of its month; the last laid level holds over a U row; a month between two takes the earlier', () => {
    const j42 = opened(true, 29);
    expect(lightsIn(j42, 'London')).toBe(3);
    expect(lightsIn(j42, 'Kent')).toBe(3);
    expect(lightsIn(j42, 'Moscow')).toBe(3);
    expect(lightsIn(j42, 'Leningrad')).toBe(3);
    expect(lightsIn(j42, 'Liege')).toBe(1);
    expect(lightsIn(j42, 'Berlin')).toBe(2);
    expect(lightsIn(j42, 'Hamburg')).toBe(2);
    const between = opened(true, 31);
    expect(lightsFlags(between).sort()).toEqual(lightsFlags(j42).sort());
    const m44 = opened(true, 57);
    expect(lightsIn(m44, 'London')).toBe(3);
    expect(lightsIn(m44, 'Northumbria')).toBe(1);
    expect(lightsIn(m44, 'Ruhr')).toBe(3);
    expect(lightsIn(m44, 'Hamburg')).toBe(2);
    expect(lightsIn(m44, 'Liege')).toBe(0);
  });

  it('a row marked not to be laid is not laid', () => {
    const g = grand(true);
    openCityDefences(g, [
      { region: 'Hamburg', turns: [1], level: 2, mark: 'U', lay: false, why: 'a test row: a U alone' },
      { region: 'Bremen', turns: [1], level: 1, mark: 'D', lay: true, why: 'a test row' },
    ]);
    expect(lightsIn(g, 'Hamburg')).toBe(0);
    expect(lightsIn(g, 'Bremen')).toBe(1);
  });
});

/** Germany's staff with money in hand, its fighters kept or not, Fighter control or not, and Berlin and the Ruhr's guns set. */
function staffOf(on: boolean, o: { fighters?: boolean; radar?: boolean; night?: string[]; raided?: boolean } = {}): Game {
  const g = startOpen(createGame(3, null));
  switchBare(g, on);
  g.nations.GER.pp = 500;
  g.flags[`wk:aa:${g.byName.Berlin.id}`] = 2;
  g.flags[`wk:aa:${g.byName.Ruhr.id}`] = 1;
  if (o.fighters === false) for (const f of g.airFleets) if (f.nat === 'GER' && f.kind === 'ftr') f.ftr = 0;
  if (o.radar) techOf(g, 'GER')!.done.push(AC.radar.control);
  for (const name of o.night ?? []) g.flags[`nr:${g.byName[name].id}`] = g.turn;
  if (o.raided) g.raids.GER = { turn: g.turn, share: 0.05 };
  return g;
}
const works = (g: Game): string[] => g.nations.GER.queue.filter((b) => b.what === 'work').map((b) => `${b.item}:${g.regions[b.region].name}`);

describe('city defences: the computer orders searchlights where they count (slice 5)', () => {
  // (The design director, 25 Nov 2026 12:18Z: the computer asks for lights first where they count, and its guns order
  // then goes ahead in the same month wherever its own guns logic would order it, for a player may order both.)
  it('after a night raid, with fighters in reach, at the province with the most guns, one order at a time, asked before the guns', () => {
    const off = staffOf(false, { night: ['Berlin', 'Ruhr'], raided: true });
    aiArsenal(off, 'GER');
    const guns = works(off);
    expect(guns).toEqual(['aa:Ruhr']);
    const g = staffOf(true, { night: ['Berlin', 'Ruhr'], raided: true });
    aiArsenal(g, 'GER');
    expect(works(g)).toEqual(['lights:Berlin', ...guns]);
    aiArsenal(g, 'GER');
    expect(works(g)).toEqual(['lights:Berlin', ...guns]);
  });

  it('with Fighter control and no fighters, the lights still count for the guns', () => {
    const g = staffOf(true, { fighters: false, radar: true, night: ['Ruhr'], raided: true });
    aiArsenal(g, 'GER');
    expect(works(g)).toEqual(['lights:Ruhr', 'aa:Ruhr']);
  });

  it('a player may order searchlights and guns in one town in one month, money allowing: so may the computer', () => {
    const g = grand(true);
    const r = g.byName.Hamburg;
    expect(orderArsenal(g, 'work:lights', r.id)).not.toBeNull();
    expect(orderArsenal(g, 'work:aa', r.id)).not.toBeNull();
    expect(orderArsenal(g, 'work:aa', r.id)).not.toBeNull();
    expect(g.nations.GER.queue.filter((b) => b.region === r.id).map((b) => b.item)).toEqual(['lights', 'aa', 'aa']);
    // What stops him is money: a treasury short of the price.
    g.nations.GER.pp = 0;
    expect(offer(g, 'work:lights', r.id)!.why).not.toBeNull();
    expect(orderArsenal(g, 'work:lights', r.id)).toBeNull();
    // And the computer, held to the same reserve as its guns: with no money above it, it orders neither.
    const poor = staffOf(true, { night: ['Berlin', 'Ruhr'], raided: true });
    poor.nations.GER.pp = 0;
    aiArsenal(poor, 'GER');
    expect(works(poor)).toEqual([]);
  });

  it('with neither radar nor fighters in reach, or with no night raid, or with the lights as many as the guns, it orders guns as today', () => {
    const off = staffOf(false, { night: ['Berlin'], raided: true });
    aiArsenal(off, 'GER');
    const today = works(off);
    expect(today.length).toBe(1);
    expect(today[0]).toMatch(/^aa:/);
    for (const g of [
      staffOf(true, { fighters: false, night: ['Berlin'], raided: true }),
      staffOf(true, { raided: true }),
    ]) { aiArsenal(g, 'GER'); expect(works(g)).toEqual(today); }
    const lit = staffOf(true, { night: ['Berlin'], raided: true });
    lit.flags[`wk:lights:${lit.byName.Berlin.id}`] = 2;
    aiArsenal(lit, 'GER');
    expect(works(lit)).toEqual(today);
  });

  it('a night raid writes its own mark where its bombs fall, only with the switch on', () => {
    for (const on of [true, false]) {
      const { g, t, f } = ruhrByNight({ on, guns: 1 });
      fly(g, 'UK', 'night', t, [f]);
      expect(g.flags[`nr:${t}`], String(on)).toBe(on ? g.turn : undefined);
    }
    const { g, t, f } = ruhrByNight({ on: true, guns: 1 });
    g.raids = {};
    fly(g, 'UK', 'bomb', t, [f]);
    expect(`nr:${t}` in g.flags).toBe(false);
  });
});

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
interface ViewRegion { id: number; lt?: number; tw?: number; gl?: number; w?: number[] }
interface BridgeView { cityDefences: boolean; regions: ViewRegion[] }
function britain(on: boolean): Game {
  const g = startOpen(createGame(3, 'UK'));
  setCityDefences(g, on);
  if (on) openCityDefences(g);
  let guard = 0;
  while (!humanUp(g) && guard++ < 200) stepAI(g);
  beginHuman(g);
  return g;
}

describe('city defences in the bridge (slice 5)', () => {
  it('the view carries the switch, and per region the lights, the town works and the guns lost, only with it on', () => {
    const g = britain(true);
    const lon = g.byName.London.id, ber = g.byName.Berlin.id;
    g.flags[`tw:${lon}`] = 2; g.flags[`twn:${lon}`] = Object.keys(g.nations).indexOf('UK');
    g.flags[`aal:${ber}`] = g.turn;
    TC().load(saveGame(g));
    const v = TC().view() as BridgeView;
    expect(v.cityDefences).toBe(true);
    const at = (id: number) => v.regions.find((r) => r.id === id)!;
    expect(at(lon).lt).toBe(2);
    expect(at(lon).tw).toBe(2);
    expect(at(ber).lt).toBe(1);
    expect(at(ber).gl).toBe(1);
    expect(at(ber).w?.length).toBe(4);
    const paris = at(g.byName.Paris.id);
    expect('lt' in paris || 'tw' in paris || 'gl' in paris).toBe(false);
    // Off: the same marks on the ground, and not a field of them in the view.
    expect((TC().act('citydefencesrule:off') as { ok: boolean; note: string }).note).toMatch(/^Off, the guns are as good by night as by day/);
    const w = TC().view() as BridgeView;
    expect(w.cityDefences).toBe(false);
    expect(w.regions.some((r) => 'lt' in r || 'tw' in r || 'gl' in r)).toBe(false);
  });

  it('act and TC turn it on and off', () => {
    const g = britain(false);
    TC().load(saveGame(g));
    expect((TC().view() as BridgeView).cityDefences).toBe(false);
    const on = TC().act('citydefencesrule:on') as { ok: boolean; note: string };
    expect(on.ok).toBe(true);
    expect(on.note).toMatch(/^Searchlights to build beside the anti-aircraft guns\./);
    expect((TC().view() as BridgeView).cityDefences).toBe(true);
    expect((TC().cityDefences(false) as { on: boolean }).on).toBe(false);
    expect((TC().view() as BridgeView).cityDefences).toBe(false);
    expect((TC().cityDefences(true) as { on: boolean }).on).toBe(true);
  });

  it('the 3D ship list can turn it on, once: a war takes it as it opens, and a switch the player turns off afterwards stays off', () => {
    const g = britain(false);
    TC().load(saveGame(g));
    expect((TC().view() as BridgeView).cityDefences).toBe(false);
    // A list that does not name it switches nothing and marks nothing.
    expect(TC().r2Ship(['nosuchrule'])).toBe(null);
    expect((TC().view() as BridgeView).cityDefences).toBe(false);
    const took = TC().r2Ship(['citydefencesrule']) as BridgeView | null;
    expect(took?.cityDefences).toBe(true);
    const kept = loadGame((TC().save() as { text: string }).text)!;
    expect(kept.flags['r2:citydefencesrule']).toBe(1);
    expect(cityDefences(kept)).toBe(true);
    // The start lay came with it, as with the Options row: the record's lights stand.
    expect(START_LAID in kept.flags).toBe(true);
    // Turned off by the player, the list leaves it off, in this session and after a save and a load.
    TC().act('citydefencesrule:off');
    expect(TC().r2Ship(['citydefencesrule'])).toBe(null);
    expect((TC().view() as BridgeView).cityDefences).toBe(false);
    TC().load((TC().save() as { text: string }).text);
    expect(TC().r2Ship(['citydefencesrule'])).toBe(null);
    expect((TC().view() as BridgeView).cityDefences).toBe(false);
  });

  it('the AIR DEFENCE shelf shows the searchlights beside the guns only with the switch on', () => {
    for (const on of [true, false]) {
      const g = britain(on);
      g.flags[`wk:aa:${g.byName.London.id}`] = 3;
      TC().load(saveGame(g));
      const air = (TC().province(g.byName.London.id) as { glance: { air: Record<string, unknown> | null } }).glance.air!;
      expect(air.add && (air.add as { key: string }).key).toBe('work:aa');
      if (!on) { expect('lights' in air).toBe(false); continue; }
      const l = air.lights as { level: number; lit: number; guns: number; line: string; add: { key: string } | null };
      expect(l.level).toBe(2);
      expect(l.lit).toBe(2);
      expect(l.guns).toBe(3);
      expect(l.line).toBe('level 2: 2 of the 3 levels of guns are lit by night, under a clear sky');
      expect(l.add?.key).toBe('work:lights');
    }
  });

  it('the AIR DEFENCE shelf\'s searchlights line, with lights and no guns, uses the raid card\'s no-guns words', () => {
    const g = britain(true);
    const id = g.byName.London.id;
    delete g.flags[`wk:aa:${id}`];
    g.flags[`wk:lights:${id}`] = 2;
    TC().load(saveGame(g));
    const air = (TC().province(id) as { glance: { air: Record<string, unknown> | null } }).glance.air!;
    const l = air.lights as { level: number; lit: number; guns: number; line: string };
    expect(l.level).toBe(2);
    expect(l.guns).toBe(0);
    expect(l.lit).toBe(0);
    expect(l.line).toBe('level 2: There are no guns for them to light. They help the night fighters a little.');
    expect(l.line).not.toMatch(/0 of the 0/);
  });

  it('a night raid\'s flight row carries the searchlights from its report, only with the switch on', () => {
    for (const on of [true, false]) {
      const g = britain(on);
      const t = g.byName.Ruhr.id;
      g.flags[`wk:aa:${t}`] = 3;
      if (on) g.flags[`wk:lights:${t}`] = 2;
      techOf(g, 'GER')!.done.push(AC.radar.control);
      const f = g.airFleets.find((x) => x.nat === 'UK' && x.bmr > 0 && x.ship === undefined)!;
      f.bmr = 100;
      TC().load(saveGame(g));
      const out = TC().act(`raid:night:${t}:${f.id}`) as { ok: boolean; flies: Record<string, unknown>[]; note: string };
      expect(out.ok, out.note).toBe(true);
      const first = out.flies[0];
      expect(first).toBeDefined();
      if (!on) { expect('sl' in first || 'lit' in first || 'cloud' in first).toBe(false); continue; }
      expect(first.sl).toBe(2);
      expect(first.lit as number).toBeGreaterThan(0);
      expect('light' in first).toBe(false);
    }
  });
});

// ---- Slice 6: start guns from the record, and the start lay wherever the switch comes on --------------------------
// (City defences, the design director's ruling of 25 Nov 2026 04:10Z, "Start guns from the record"; the historian's
// rows, historian/start-guns-1125.md, in citydefences.json startGuns.)

/** History played by the computer up to a month, kept as saved text so that it is played once (the bridge's historyTo). */
const histories = new Map<number, string>();
function historyAt(from: number): Game {
  let text = histories.get(from);
  if (!text) {
    const h = createGame(3, null);
    while (h.turn < from) runTurnAI(h);
    text = saveGame(h);
    histories.set(from, text);
  }
  return loadGame(text)!;
}
/** A grand campaign begun at a start month, as the bench runners begin it: the switch set as it opens (stage.json's path), or not. */
function begun(from: number, on: boolean): Game {
  const g = from <= 1 ? createGame(3, null) : historyAt(from);
  if (on) g.flags.cityDefences = 1;
  return startOpen(g);
}
const gunsIn = (g: Game, name: string): number => g.flags[`wk:aa:${g.byName[name].id}`] ?? 0;
const levels = (g: Game, prefix: string): Record<string, number> =>
  Object.fromEntries(Object.keys(g.flags).filter((k) => k.startsWith(prefix)).sort().map((k) => [g.regions[+k.slice(prefix.length)].name, g.flags[k]]));
/** The places of ruling 124 that the record does not read: they keep 124's level under the switch. */
const STAND_INS = ['Paris', 'Lille', 'Lyon', 'Gorky', 'Kharkov'];

describe('city defences: start guns from the record (slice 6)', () => {
  it('keeps a row for every place read, none included, and a stand-in row not laid for each place of 124 not read', () => {
    expect(typeof (cd as Record<string, unknown>)._startGuns).toBe('string');
    for (const row of CITYDEF.startGuns) {
      expect(row.why.length, row.region).toBeGreaterThan(10);
      if (row.lay) expect(row.level! >= 0 && row.level! <= WORKS_TOP, row.region).toBe(true);
      else { expect(row.mark).toBe('G'); expect(row.level).toBeUndefined(); }
    }
    const g = startOpen(createGame(3, null));
    // Every name is the map's own, but Tokyo, which stands on the wider world beyond this one.
    const missing = [...new Set(CITYDEF.startGuns.map((r) => r.region))].filter((n) => !g.byName[n]);
    expect(missing).toEqual(['Tokyo']);
    for (const n of ['Saxony', 'Frankfurt', 'Hanover', 'Rhineland']) expect(CITYDEF.start.some((r) => r.region === n && r.lay && r.level === 1 && r.mark === 'D'), n).toBe(true);
  });

  it('September 1939 with the switch on: the record decides, and 124 stands where nothing is read', () => {
    const off = begun(1, false), g = begun(1, true);
    for (const n of ['Berlin', 'Hamburg', 'Ruhr', 'Saxony', 'Frankfurt', 'Hanover']) expect(gunsIn(g, n), n).toBe(1);
    expect(gunsIn(g, 'Rhineland')).toBe(0);
    expect(gunsIn(off, 'Bohemia')).toBe(1);
    expect(`wk:aa:${g.byName.Bohemia.id}` in g.flags).toBe(false);
    expect(gunsIn(g, 'London')).toBe(1);
    for (const n of ['Midlands', 'Lancashire', 'Washington', 'New York', 'Detroit', 'Philadelphia']) expect(`wk:aa:${g.byName[n].id}` in g.flags, n).toBe(false);
    expect(gunsIn(g, 'Moscow')).toBe(3);
    for (const n of ['Rome', 'Lombardy', 'Piedmont']) expect(gunsIn(g, n), n).toBe(1);
    for (const n of STAND_INS) { expect(gunsIn(off, n), n).toBe(1); expect(gunsIn(g, n), n).toBe(gunsIn(off, n)); }
    // New York is read with Washington and Philadelphia (the design director, 25 Nov 13:06Z): 124 laid it, the record bares it.
    expect(gunsIn(off, 'New York')).toBe(1);
    expect(gunsIn(g, 'New York')).toBe(0);
    // The new lights rows are laid; London's and Berlin's stand as slice 5 laid them; none in Paris, Rome or Washington.
    for (const n of ['Saxony', 'Frankfurt', 'Hanover', 'Hamburg']) expect(lightsIn(g, n), n).toBe(1);
    expect(lightsIn(g, 'Rhineland')).toBe(0);
    expect(lightsIn(g, 'London')).toBe(2);
    expect(lightsIn(g, 'Berlin')).toBe(1);
    for (const n of ['Paris', 'Rome', 'Washington']) expect(lightsIn(g, n), n).toBe(0);
  });

  it('with the switch off, ruling 124 exactly: no lights, no mark of the lay', () => {
    const g = begun(1, false);
    expect(lightsFlags(g)).toEqual([]);
    expect(START_LAID in g.flags).toBe(false);
    expect(levels(g, 'wk:aa:')).toEqual({ Berlin: 1, Ruhr: 1, Bohemia: 1, Rome: 1, Lombardy: 1, Piedmont: 1, London: 1, Midlands: 1, Lancashire: 1, Paris: 1, Lille: 1, Lyon: 1, Moscow: 1, Gorky: 1, Kharkov: 1, Washington: 1, Philadelphia: 1, 'New York': 1 });
  });

  it('the same 1939 start when the switch comes on in the opening month of a war begun with it off', () => {
    const on = begun(1, true), later = begun(1, false);
    setCityDefences(later, true);
    expect(levels(later, 'wk:aa:')).toEqual(levels(on, 'wk:aa:'));
    expect(levels(later, 'wk:lights:')).toEqual(levels(on, 'wk:lights:'));
  });

  it('June 1941 (turn 22): Berlin, Hamburg and the Ruhr at 2, Cologne\'s region at 1, London 2, the Midlands and Lancashire 1', () => {
    const g = begun(22, true);
    expect(g.flags.openFrom).toBe(22);
    for (const n of ['Berlin', 'Hamburg', 'Ruhr']) expect(gunsIn(g, n), n).toBe(2);
    expect(gunsIn(g, 'Rhineland')).toBe(1);
    expect(gunsIn(g, 'London')).toBe(2);
    expect(gunsIn(g, 'Midlands')).toBe(1);
    expect(gunsIn(g, 'Lancashire')).toBe(1);
    expect(lightsIn(g, 'Rhineland')).toBe(1);
    // Hamburg's lights follow its guns, and the American three stand bare in a June 1941 start (the design director, 25 Nov 13:06Z).
    expect(lightsIn(g, 'Hamburg')).toBe(2);
    for (const n of ['Washington', 'New York', 'Philadelphia']) expect(`wk:aa:${g.byName[n].id}` in g.flags, n).toBe(false);
  }, 240_000);

  it('the turn 29 start (December 1941): Leningrad 3, and the American three at ruling 124\'s level', () => {
    const off = begun(29, false), g = begun(29, true);
    expect(gunsIn(g, 'Leningrad')).toBe(3);
    for (const n of ['Washington', 'New York', 'Detroit', 'Philadelphia']) expect(gunsIn(g, n), n).toBe(gunsIn(off, n));
    expect(gunsIn(off, 'Washington')).toBe(2);
    expect(gunsIn(off, 'New York')).toBeGreaterThan(0);
  }, 240_000);

  it('turned on in a later month: only raised, and once', () => {
    const g = begun(1, false);
    for (let k = 0; k < 2; k++) runTurnAI(g);
    expect(g.turn).toBeGreaterThan(1);
    const bohemia = gunsIn(g, 'Bohemia');
    expect(bohemia).toBe(1);
    const before = levels(g, 'wk:aa:');
    setCityDefences(g, true);
    expect(gunsIn(g, 'Bohemia')).toBe(bohemia);
    expect(gunsIn(g, 'Moscow')).toBe(3);
    expect(gunsIn(g, 'Midlands')).toBe(before.Midlands);
    for (const [name, lv] of Object.entries(before)) expect(gunsIn(g, name), name).toBeGreaterThanOrEqual(lv);
    expect(gunsIn(g, 'Hamburg')).toBe(1);
    expect(lightsIn(g, 'London')).toBe(2);
    // A second turning on changes nothing, even where the ground has changed since.
    const laid = { aa: levels(g, 'wk:aa:'), lights: levels(g, 'wk:lights:') };
    delete g.flags[`wk:aa:${g.byName.Moscow.id}`];
    setCityDefences(g, false);
    setCityDefences(g, true);
    expect(gunsIn(g, 'Moscow')).toBe(0);
    g.flags[`wk:aa:${g.byName.Moscow.id}`] = 3;
    expect({ aa: levels(g, 'wk:aa:'), lights: levels(g, 'wk:lights:') }).toEqual(laid);
  });

  it('lays nothing outside a grand campaign, and passes over a row whose region the game does not have', () => {
    const h = createGame(3, null);
    setCityDefences(h, true);
    expect(START_LAID in h.flags).toBe(false);
    expect(Object.keys(h.flags).filter((k) => k.startsWith('wk:'))).toEqual([]);
    const g = begun(1, false);
    g.flags.cityDefences = 1;
    const before = levels(g, 'wk:aa:');
    expect(() => layStartGunsFromRecord(g, true, [
      { region: 'Nowhere at all', turns: [1], level: 3, mark: 'L', lay: true, why: 'a test row: no such province' },
      { region: 'Bohemia', turns: [1], level: 2, mark: 'G', lay: false, why: 'a test row: not laid' },
    ])).not.toThrow();
    expect(levels(g, 'wk:aa:')).toEqual(before);
  });
});
