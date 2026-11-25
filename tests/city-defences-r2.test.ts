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
import { cityNight, cityNightShare } from '../src/campaign/citydefences';
import { techOf } from '../src/campaign/tech';
import { WEATHER, setWeather, zoneOfRegion } from '../src/campaign/weather';
import stage from '../data/campaign/stage.json';
import { ARSENAL, offers, workLevel, worksUpkeep, type Offer } from '../src/campaign/arsenal';
import { CITYDEF, LIGHTS_TOP, cityDefences, cityDefencesOn, lightsAt, openCityDefences, setCityDefences } from '../src/campaign/citydefences';
import { cancelBuild, deliver, ledger, upkeepTotal } from '../src/campaign/economy';
import { createGame } from '../src/campaign/game';
import { cityOrders } from '../src/campaign/orders';
import { startOpen } from '../src/campaign/staff';
import { arsenalOffers, orderArsenal } from '../src/campaign/turn';
import type { AirFleet, Build, Game } from '../src/campaign/types';

const grand = (on: boolean): Game => { const g = startOpen(createGame(3, 'GER')); setCityDefences(g, on); g.nations.GER.pp = 500; return g; };
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
    expect(cd.start).toEqual([]);
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
    openCityDefences(g, [{ region: 'Hamburg', level: 2, why: 'a test row' }]);
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
  it('lays the table only with the switch on, by province, keeping the higher level; empty until the historian reads', () => {
    expect(CITYDEF.start).toEqual([]);
    const g = grand(true);
    const ham = g.byName['Hamburg'], ber = g.byName['Berlin'];
    g.flags[`wk:lights:${ber.id}`] = 3;
    const before = lightsFlags(g);
    openCityDefences(g);
    expect(lightsFlags(g)).toEqual(before);
    openCityDefences(g, [
      { region: 'Hamburg', level: 2, why: 'a test row' },
      { region: 'Berlin', level: 1, why: 'a test row: the higher level stays' },
      { region: 'Nowhere at all', level: 3, why: 'a test row: no such province' },
      { region: 'Bremen', level: 9, why: 'a test row: clamped to the top' },
    ]);
    expect(lightsAt(g, ham.id)).toBe(2);
    expect(lightsAt(g, ber.id)).toBe(3);
    expect(lightsAt(g, g.byName['Bremen'].id)).toBe(LIGHTS_TOP);
  });

  it('opens a campaign with the switch set where stage.json sets it, and nothing laid while the table is empty', () => {
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
  setCityDefences(g, o.on ?? true);
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
