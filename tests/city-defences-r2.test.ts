// City defences (release 2; Brian 24 Nov 2026 22:08Z; city defences, the design director's ruling of 25 Nov 2026
// 02:23Z). One switch, 'cityDefences'. Slice 1: the switch, the data and the searchlights work: offered straight after
// the guns, by the guns' own rule, at 2 / 3 / 5, in a month, billed 0.015 a level, to level 3; and with the switch off
// nothing of it is offered, billed, written or read.
import { describe, expect, it } from 'vitest';
import cd from '../data/campaign/citydefences.json';
import stage from '../data/campaign/stage.json';
import { ARSENAL, offers, workLevel, worksUpkeep, type Offer } from '../src/campaign/arsenal';
import { CITYDEF, LIGHTS_TOP, cityDefences, cityDefencesOn, lightsAt, openCityDefences, setCityDefences } from '../src/campaign/citydefences';
import { cancelBuild, deliver, ledger, upkeepTotal } from '../src/campaign/economy';
import { createGame } from '../src/campaign/game';
import { cityOrders } from '../src/campaign/orders';
import { startOpen } from '../src/campaign/staff';
import { arsenalOffers, orderArsenal } from '../src/campaign/turn';
import type { Build, Game } from '../src/campaign/types';

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
