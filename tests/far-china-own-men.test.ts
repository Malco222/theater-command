// China's men by the record, laid with its rifles (stage B2, slice S3; the design director's 04:25Z and 04:31Z of 25
// November 2026, "China's men", and her rulings on pacific-japan/dd-form-arms-1125.md, clause 9: the men laid in the
// same change as the taps). data/campaign/far-arms.json lays China's pool at 7.4 divisions' worth (about 0.5 million
// men) and its class at 2.5 (about 105,000 men a month from the land it holds on the first day), over ruling 216(b)'s
// 1.5 and 0.025, which stay in the tool-written far-world.json and are never read with the far side on. The same change
// retires rider B of 20:45Z, 20 November 2026, to ruling 234 (china-stop3.json's manpower.ownMen, manpower.ts ownMen,
// economy.ts paidInMen and the staff's two closures that read it): China's foot corps cost no points because the store
// of rifles charges their rifles (rifles.ts carve), and a corps now waits for rifles, not for men. The staff's bank
// passes a kind whose carved price is 0 for a nation whose store binds, and asks every other price its reserve (the
// director's answer 6 of 19:08Z). The last levy stands and brings men only. The state is set by hand on the first day's
// chart; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import * as economy from '../src/campaign/economy';
import { PRODUCTION, aiProduction, cadreTerms, corpsCost, corpsList, specOf } from '../src/campaign/economy';
import * as manpower from '../src/campaign/manpower';
import { MANPOWER, classFull, classNow, homeShare, menLeft, menSpare, poolAtStart } from '../src/campaign/manpower';
import { stockOf } from '../src/campaign/arms';
import { RIFLES, binds, levelOf } from '../src/campaign/rifles';
import { fieldStrength, traitLevy, traitsOf } from '../src/campaign/traits';
import { foldCorps } from '../src/campaign/fold';
import chinaStop3 from '../data/campaign/china-stop3.json';
import farWorld from '../data/campaign/far-world.json';
import type { Build, Game } from '../src/campaign/types';

/** A war nobody plays on the first day, far side on: China is at war with Japan from the start. */
const war = (): Game => startOpen(createGame(3, null));
/** A nation's corps on order. */
const forming = (g: Game, nat = 'CHN'): Build[] => g.nations[nat].queue.filter((b) => b.what === 'corps');
/** The last levy's row, as China's card carries it. */
const levy = () => traitsOf('CHN').find((t) => t.levy)!.levy!;
/** China's rifles in store, in its divisions' worth. */
const store = (g: Game, divs: number): void => { g.flags['sa:CHN'] = divs; };

describe("China's men by the record, laid with its rifles", () => {
  it("1. China's men by the record", () => {
    const g = war();
    expect(MANPOWER.pool.CHN).toBe(7.4);
    expect(MANPOWER.class.CHN).toBe(2.5);
    expect(poolAtStart(g, 'CHN')).toBe(7.4);
    expect(menLeft(g, 'CHN')).toBe(7.4);
    expect(classFull(g, 'CHN')).toBe(2.5);
    // The class is 1.56 over the share of the homeland China holds on the first day, as homeShare weighs it.
    const home = homeShare(g, 'CHN');
    expect(Math.abs(home - 0.626)).toBeLessThan(0.01);
    expect(Math.abs(classNow(g, 'CHN') - 1.56)).toBeLessThan(0.02);
    // It falls with the homeland lost: Chungking and Chengtu taken.
    for (const name of ['Chungking', 'Chengtu']) { const r = g.regions.find((x) => x.name === name)!; expect(r.owner).toBe('CHN'); r.ctrl = 'JAP'; }
    expect(homeShare(g, 'CHN')).toBeLessThan(home);
    expect(classNow(g, 'CHN')).toBeCloseTo(2.5 * Math.max(MANPOWER.classFloor, homeShare(g, 'CHN')), 12);
    expect(classNow(g, 'CHN')).toBeLessThan(1.54);
  });

  it("2. 216(b)'s rows and rider B's list are gone", () => {
    expect((MANPOWER as { ownMen?: string[] }).ownMen ?? []).toEqual([]);
    expect('ownMen' in manpower).toBe(false);
    expect('paidInMen' in economy).toBe(false);
    expect('manpower' in chinaStop3).toBe(false);
    expect('_riderB' in chinaStop3).toBe(false);
    // 216(b)'s rows stay in the tool-written file, overlaid by far-arms.json, which is laid after it.
    expect(farWorld.manpower.pool.CHN).toBe(1.5);
    expect(farWorld.manpower.class.CHN).toBe(0.025);
  });

  it('3. the levy brings men, never a corps; its trigger is half of 7.4', () => {
    const g = war();
    const L = levy();
    expect([L.spent, L.men, L.under]).toEqual([0.5, 6.5, 32]);
    expect(L.spent * poolAtStart(g, 'CHN')).toBeCloseTo(3.7, 12);
    // An army in the field under the levy's mark, and the enemy on China's soil (it holds 0.626 of its homeland).
    for (const c of g.corps) if (c.nat === 'CHN' && !c.dead && c.kind !== 'fort') c.str *= 0.8;
    expect(fieldStrength(g, 'CHN')).toBeLessThan(L.under);
    const corps = (): number => g.corps.filter((c) => c.nat === 'CHN' && !c.dead).length;
    const had = corps(), queue = g.nations.CHN.queue.length;
    // A reserve at 3.71: not yet.
    g.flags['mp:CHN'] = 3.71;
    expect(traitLevy(g, 'CHN')).toBe(0);
    expect(menLeft(g, 'CHN')).toBe(3.71);
    // At 3.69: the levy's 6.5 come into the reserve, and nothing else.
    g.flags['mp:CHN'] = 3.69;
    expect(traitLevy(g, 'CHN')).toBe(6.5);
    expect(menLeft(g, 'CHN')).toBeCloseTo(3.69 + 6.5, 12);
    expect(corps()).toBe(had);
    expect(g.nations.CHN.queue.length).toBe(queue);
    // The men wait in the reserve for rifles: with none in store the staff raises nothing.
    store(g, 0);
    aiProduction(g, 'CHN');
    expect(forming(g)).toHaveLength(0);
    expect(menLeft(g, 'CHN')).toBeCloseTo(3.69 + 6.5, 12);
  });

  it('4. a corps waits for rifles, not men', () => {
    const g = war();
    const n = g.nations.CHN, divs = specOf(g, 'inf')!.divs;
    expect(binds(g, 'CHN')).toBe(true);
    n.pp = 0;
    // The reserve full, the corps in the field short of nothing up to their level, and the store short: nothing.
    expect(menLeft(g, 'CHN')).toBe(poolAtStart(g, 'CHN'));
    expect(Math.abs(menSpare(g, 'CHN') - menLeft(g, 'CHN'))).toBeLessThan(0.01);
    store(g, divs - 0.1);
    aiProduction(g, 'CHN');
    expect(forming(g)).toHaveLength(0);
    expect(menLeft(g, 'CHN')).toBe(7.4);
    // With 3.0 in store and the men: one infantry corps, for no points, and the store is empty.
    store(g, divs);
    aiProduction(g, 'CHN');
    expect(forming(g).map((b) => [b.kind, b.cost, b.rifles])).toEqual([['inf', 0, divs]]);
    expect(stockOf(g, 'CHN', 'rifles')).toBe(0);
    expect(menLeft(g, 'CHN')).toBeCloseTo(7.4 - divs, 12);
    expect(n.pp).toBe(0);
  });

  it('4. (the men) the staff keeps back the men its corps are short of, up to their armed level', () => {
    const g = war();
    const n = g.nations.CHN, divs = specOf(g, 'inf')!.divs;
    n.pp = 0;
    store(g, 10);
    // A corps at full knocked down by battle: its level stands at full, so its drafts will want the men back.
    const c = addCorps(g, 'CHN', 'A full corps', 'inf', 3, g.regions[n.capital].name, {});
    expect(levelOf(g, c)).toBe(1);
    c.str = 1 - (menLeft(g, 'CHN') - divs + 0.5) / c.divs;
    expect(menLeft(g, 'CHN')).toBeGreaterThanOrEqual(divs);
    expect(menSpare(g, 'CHN')).toBeLessThan(divs);
    aiProduction(g, 'CHN');
    expect(forming(g)).toHaveLength(0);
    // Given its men back, the corps is raised.
    c.str = 1;
    aiProduction(g, 'CHN');
    expect(forming(g).map((b) => b.kind)).toEqual(['inf', 'inf']);
  });

  it('5. the staff\'s bank passes for a corps that costs nothing, and asks any carved price left its reserve', () => {
    /** The first day, with a mountain cadre of 2 divisions waiting and rifles enough in store. */
    const day = () => {
      const g = war();
      const n = g.nations.CHN, cap = g.regions[n.capital].name;
      const a = addCorps(g, 'CHN', 'Kept corps', 'mtn', 2, cap, {}), b = addCorps(g, 'CHN', 'Folded corps', 'mtn', 2, cap, {});
      a.str = b.str = 0.2;
      expect(foldCorps(g, a, b)).toBe(true);
      store(g, 20);
      return { g, n, b, t: cadreTerms(g, 'CHN', b) };
    };
    // The carve makes infantry free and leaves mountain 2 points (the form's section 9 table).
    const { g, n, b, t } = day();
    expect(corpsCost(g, 'CHN', 'inf')).toBe(0);
    expect(corpsCost(g, 'CHN', 'mtn')).toBe(2);
    expect(t.cost).toBeGreaterThan(0);
    // With an empty treasury the cadre, priced from the carved 2, waits; the staff goes on to new infantry corps,
    // whose bank passes.
    n.pp = 0;
    aiProduction(g, 'CHN');
    expect(forming(g).some((x) => x.cadre === b.id)).toBe(false);
    expect(forming(g).map((x) => [x.kind, x.cost])).toEqual([['inf', 0], ['inf', 0]]);
    // With its price and the reserve in hand, the cadre is raised first.
    const d2 = day();
    d2.n.pp = d2.t.cost + PRODUCTION.aiReserve;
    aiProduction(d2.g, 'CHN');
    expect(forming(d2.g)[0].cadre).toBe(d2.b.id);
    // A hundredth of a point short of that: it waits.
    const d3 = day();
    d3.n.pp = d3.t.cost + PRODUCTION.aiReserve - 0.01;
    aiProduction(d3.g, 'CHN');
    expect(forming(d3.g).some((x) => x.cadre === d3.b.id)).toBe(false);
  });

  it("5. (everyone else) a nation whose store does not bind: the bank is 16 above its price", () => {
    // Japan's staff raises nothing on the first day whatever its treasury (its army is idle by the far side's reading),
    // so the bank is shown on China with its rifles row taken out: then nothing binds, its infantry is at its list
    // price, and the staff asks that price and the 16 kept back, as Japan's and every other nation's does.
    expect(PRODUCTION.aiReserve).toBe(16);
    const row = RIFLES.nations.CHN;
    delete RIFLES.nations.CHN;
    try {
      const day = (pp: number): Game => {
        const g = war();
        expect(binds(g, 'CHN')).toBe(false);
        // Every corps at full, so the men its corps wait for are none and the reserve's 7.4 are all to spare (with no
        // rifles row a corps is short up to 1, as today, whatever its 'full').
        for (const c of g.corps) if (c.nat === 'CHN' && !c.dead) c.str = 1;
        expect(menSpare(g, 'CHN')).toBeCloseTo(7.4, 9);
        g.nations.CHN.pp = pp;
        aiProduction(g, 'CHN');
        return g;
      };
      const price = corpsCost(war(), 'CHN', 'inf');
      expect(price).toBe(corpsList(war(), 'CHN', 'inf'));
      expect(price).toBeGreaterThan(0);
      expect(forming(day(price + PRODUCTION.aiReserve)).map((b) => [b.kind, b.cost])).toEqual([['inf', price]]);
      expect(forming(day(price + PRODUCTION.aiReserve - 0.01))).toHaveLength(0);
    } finally { RIFLES.nations.CHN = row; }
  });

  it("6. leaves the staff's bank mark for everyone else: Japan with an empty treasury orders nothing", () => {
    const g = war();
    g.nations.JAP.pp = PRODUCTION.aiReserve;
    const before = g.nations.JAP.queue.filter((b) => b.what === 'corps').length;
    aiProduction(g, 'JAP');
    expect(g.nations.JAP.queue.filter((b) => b.what === 'corps').length).toBe(before);
  });
});
