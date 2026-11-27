// Rulings 214 and 213(d), (e), with the works rates of 218(d): the smaller powers of the grand campaign. A smaller
// power's land pays by its worth, not by the count of its provinces (214); it builds aircraft only where the record
// had a works, from the month it opened and at its rate, and the record's aircraft from abroad arrive in their month
// (213(d)); and its industry can carry half again the factories it began the war with (213(e)). The historical
// campaigns are untouched.
import { describe, expect, it } from 'vitest';
import { ARSENAL, aircraftTag, factoriesHeld, factoryCeiling, factoryTag, worksOpening, firstFactories, greatPower, offers, openSmallAir, orderOffer, smallAirMonth, whyNoFactory, whyNoWorks, worksFrom, worksRate, type Offer } from '../src/campaign/arsenal';
import { BUDGET, income, landFactor, landShare, provinceYield } from '../src/campaign/economy';
import { createGame, setWar } from '../src/campaign/game';
import { majorsOf } from '../src/campaign/diplomacy';
import { startOpen } from '../src/campaign/staff';
import { wealthOf } from '../src/campaign/trade';
import { turnToDate } from '../src/core/ticket';
import { loadGame, saveGame } from '../src/campaign/save';
import type { Game, Region } from '../src/campaign/types';

const grand = (seed = 3, human: string | null = null): Game => startOpen(createGame(seed, human));
const offer = (g: Game, nat: string, key: string, sel = -1): Offer => offers(g, nat, sel).find((o) => o.key === key)!;
const home = (g: Game, nat: string): Region[] => g.regions.filter((r) => r.owner === nat && r.ctrl === nat);
const month = (t: number): string => turnToDate(t).label;
/** The month's figures with ruling 214's table set aside. */
const withoutFactors = <T>(f: () => T): T => {
  const keep = BUDGET.landFactor;
  BUDGET.landFactor = {};
  try { return f(); } finally { BUDGET.landFactor = keep; }
};

describe('ruling 214: a smaller power\'s land pays by its worth', () => {
  it('reads every factor as a share of a full province, and great powers have none', () => {
    for (const [code, k] of Object.entries(BUDGET.landFactor)) {
      expect(k, code).toBeGreaterThan(0);
      expect(k, code).toBeLessThan(1);
      expect(['GER', 'ITA', 'UK', 'FRA', 'SOV', 'USA', 'JAP']).not.toContain(code);
    }
  });

  it('multiplies the land and harbours of a listed country, and nothing else', () => {
    const g = grand();
    for (const code of ['CAN', 'NED', 'ARG']) {
      const k = BUDGET.landFactor[code];
      for (const r of home(g, code)) {
        const y = provinceYield(g, r, code), bare = provinceYield(g, r, code, false, true);
        expect(landFactor(r)).toBe(k);
        // (Trade reads the land as ruling 210 holds it, a city sharing its province's pay; the factor scales the full
        // pay of each piece of ground, as the historian measured it.)
        expect(y.land).toBeCloseTo((bare.land / landShare(r, true)) * k, 10);
        expect(y.port).toBeCloseTo(bare.port * k, 10);
        expect(y.oil).toBe(bare.oil);
        expect(y.fac).toBe(bare.fac);
        expect(y.capital).toBe(bare.capital);
      }
    }
    // Canada pays a fifth of what its provinces and cities pay in full.
    const can = home(g, 'CAN');
    const land = can.reduce((s, r) => s + provinceYield(g, r, 'CAN').land, 0), bare = can.reduce((s, r) => s + provinceYield(g, r, 'CAN', false, true).land / landShare(r, true), 0);
    expect(land).toBeCloseTo(bare * 0.2, 10);
    expect(income(g, 'CAN')).toBeLessThan(withoutFactors(() => income(g, 'CAN')));
  });

  it('goes with the ground: a conqueror draws the factor of the province, and keeps it after a save', () => {
    const g = grand();
    const r = home(g, 'NED')[0];
    r.ctrl = 'GER';
    expect(landFactor(r)).toBe(BUDGET.landFactor.NED);
    expect(provinceYield(g, r, 'GER').land).toBeCloseTo(provinceYield(g, r, 'GER', false, true).land * BUDGET.landFactor.NED, 10);
    // Ground a great power began the war with stays whole in a smaller power's hands.
    const fr = home(g, 'FRA')[0];
    fr.ctrl = 'NED';
    expect(landFactor(fr)).toBe(1);
    const h = loadGame(saveGame(g))!;
    expect(h.regions[r.id].ctrl).toBe('GER');
    expect(h.regions[r.id].first).toBe('NED');
    expect(landFactor(h.regions[r.id])).toBe(BUDGET.landFactor.NED);
  });

  it('moves no great power\'s first day, and no country\'s trade', () => {
    const g = grand();
    for (const nat of majorsOf(g)) expect(income(g, nat), nat).toBe(withoutFactors(() => income(g, nat)));
    for (const nat of Object.keys(g.nations)) expect(wealthOf(g, nat), nat).toBe(withoutFactors(() => wealthOf(g, nat)));
  });

  it('leaves the historical campaigns as they were', () => {
    const g = createGame(3, null);
    for (const r of g.regions.filter((x) => x.owner === 'NED')) expect(provinceYield(g, r)).toEqual(provinceYield(g, r, r.ctrl, false, true));
  });
});

describe('ruling 213(d): a smaller power builds aircraft only where the record had a works', () => {
  it('reads the works and the gifts as the historian wrote them', () => {
    const S = ARSENAL.smallAir;
    expect(S.perPoint).toBe(35);
    for (const [code, w] of Object.entries(S.works)) {
      expect(['GER', 'ITA', 'UK', 'FRA', 'SOV', 'USA', 'JAP']).not.toContain(code);
      expect(w.ftr !== null || w.bmr !== null).toBe(true);
      if (w.ftr !== null) expect(w.rate[0]).toBeGreaterThan(0);
      if (w.bmr !== null) expect(w.rate[1]).toBeGreaterThan(0);
    }
    expect(S.gifts.length).toBe(29);
    for (const [m, to, pts, kind, givers] of S.gifts) {
      expect(m).toBeGreaterThan(0);
      expect(pts).toBeGreaterThan(0);
      expect(['ftr', 'bmr', 'mixed']).toContain(kind);
      expect(givers.length, to).toBeGreaterThan(0);
    }
  });

  it('says why in the passed words: no works, not yet, and one kind only', () => {
    const g = grand();
    expect(g.turn).toBe(1);
    // No aircraft industry at all.
    for (const code of ['BUL', 'TUR', 'BRA', 'MEX', 'POR', 'ARG']) {
      expect(whyNoWorks(g, code, 'ftr')).toBe(`${g.nations[code].name} has no aircraft industry: more aircraft come only from abroad.`);
      expect(whyNoWorks(g, code, 'bmr')).toBe(`${g.nations[code].name} has no aircraft industry: more aircraft come only from abroad.`);
    }
    // A works that opens later names its month.
    expect(whyNoWorks(g, 'SWE', 'ftr')).toBe(`Sweden builds no fighters before ${month(50)}: until then more fighters come only from abroad.`);
    expect(month(50)).toBe('October 1943');
    expect(whyNoWorks(g, 'HUN', 'ftr')).toBe(`Hungary builds no fighters before ${month(13)}: until then more fighters come only from abroad.`);
    expect(whyNoWorks(g, 'HUN', 'bmr')).toBe(`Hungary builds no bombers before ${month(49)}: until then more bombers come only from abroad.`);
    // One kind and never the other.
    expect(whyNoWorks(g, 'ESP', 'bmr')).toBe('Spain builds fighters at home, but no bombers: more bombers come only from abroad.');
    // Works that stand on the first day.
    for (const code of ['POL', 'NED', 'BEL', 'ROM']) for (const w of ['ftr', 'bmr'] as const) expect(whyNoWorks(g, code, w), `${code} ${w}`).toBeNull();
    // And the Arsenal page gives the same reason.
    expect(offer(g, 'BUL', 'air:ftr').why).toBe(whyNoWorks(g, 'BUL', 'ftr'));
    expect(offer(g, 'SWE', 'air:ftr').why).toBe(whyNoWorks(g, 'SWE', 'ftr'));
    // A works opens in its month.
    g.turn = 13;
    expect(whyNoWorks(g, 'HUN', 'ftr')).toBeNull();
    g.turn = 50;
    expect(whyNoWorks(g, 'SWE', 'ftr')).toBeNull();
  });

  it('gives the short tags and the sentence said once, as passed', () => {
    const g = grand();
    expect(aircraftTag(g, 'BUL', 'ftr')).toBe('No aircraft industry');
    expect(aircraftTag(g, 'ESP', 'bmr')).toBe('Builds no bombers');
    expect(aircraftTag(g, 'ESP', 'ftr')).toBeNull();
    expect(aircraftTag(g, 'SWE', 'ftr')).toBe('Fighters from October 1943');
    expect(aircraftTag(g, 'HUN', 'bmr')).toBe(`Bombers from ${month(49)}`);
    expect(aircraftTag(g, 'GER', 'ftr')).toBeNull();
    // Neither kind this month: the reasons in place of the opening lines, each said once.
    expect(worksOpening(g, 'BUL')).toBe('Bulgaria has no aircraft industry: more aircraft come only from abroad.');
    expect(worksOpening(g, 'HUN')).toBe(`Hungary builds no fighters before ${month(13)} and no bombers before ${month(49)}: until then more aircraft come only from abroad.`);
    expect(worksOpening(g, 'CAN')).toBe(`Canada builds no bombers before ${month(3)} and no fighters before ${month(6)}: until then more aircraft come only from abroad.`);
    // One kind on: the opening lines stay.
    expect(worksOpening(g, 'SWE')).toBeNull();
    expect(worksOpening(g, 'ESP')).toBeNull();
    expect(worksOpening(g, 'GER')).toBeNull();
    // The month a works opens the tag is gone.
    g.turn = 50;
    expect(aircraftTag(g, 'SWE', 'ftr')).toBeNull();
  });

  it('builds at the works\' rate, one wing of a kind at a time (218(d))', () => {
    const g = grand();
    const count = ARSENAL.air.bmr.count;
    // Canada's bombers: 0.6 points a month until October 1943, then 4.
    g.turn = 3;
    expect(worksRate(g, 'CAN', 'bmr')).toBeCloseTo(0.6 * 35, 10);
    g.nations.CAN.pp = 1000;
    const first = offer(g, 'CAN', 'air:bmr');
    expect(first.why).toBeNull();
    expect(first.months).toBe(Math.ceil(count / (0.6 * 35)));
    const b = orderOffer(g, 'CAN', 'air:bmr')!;
    expect(b.eta - g.turn).toBe(first.months);
    // A second waits for the first.
    expect(offer(g, 'CAN', 'air:bmr').months).toBe(first.months * 2);
    g.turn = 49;
    expect(worksRate(g, 'CAN', 'bmr')).toBeCloseTo(4 * 35, 10);
    g.nations.CAN.queue = [];
    expect(offer(g, 'CAN', 'air:bmr').months).toBe(ARSENAL.air.bmr.months);
    // Sweden's bombers before its fighter works open: 0.1 points a month.
    const h = grand();
    expect(worksRate(h, 'SWE', 'bmr')).toBeCloseTo(3.5, 10);
    expect(worksRate(h, 'SWE', 'ftr')).toBe(0);
    expect(offer(h, 'SWE', 'air:bmr').months).toBe(Math.ceil(count / 3.5));
  });

  it('leaves a great power\'s aircraft to its factories', () => {
    const g = grand();
    for (const nat of majorsOf(g)) expect(greatPower(g, nat)).toBe(true);
    expect(greatPower(g, 'SWE')).toBe(false);
    g.nations.GER.pp = 1000;
    for (const w of ['ftr', 'bmr'] as const) {
      const o = offer(g, 'GER', `air:${w}`);
      expect(o.why).toBeNull();
      expect(o.months).toBe(ARSENAL.air[w].months);
    }
  });

  it('brings the record\'s aircraft from abroad in their month, as a new air group on the best airfield', () => {
    const g = grand(3, 'BUL');
    const groups = (nat: string) => g.airFleets.filter((f) => f.nat === nat);
    const before = groups('BUL').length;
    g.turn = 6;
    smallAirMonth(g);
    const now = groups('BUL').slice(before);
    // Two points, mixed: 35 fighters and 35 bombers, each a group of its own.
    expect(now.map((f) => [f.kind, f.ftr, f.bmr])).toEqual([['ftr', 35, 0], ['bmr', 0, 35]]);
    expect(now.every((f) => f.base === g.nations.BUL.capital)).toBe(true);
    expect(g.headlines.some((h) => h.nat === 'BUL' && /35 fighters .* and 35 bombers .* arrive from Germany/.test(h.text))).toBe(true);
    // Once only.
    smallAirMonth(g);
    expect(groups('BUL').length).toBe(before + 2);
  });

  it('holds a row while the receiver has no airfield, and loses one whose givers are gone or at war with it', () => {
    const g = grand();
    const bul = home(g, 'BUL');
    const n = g.airFleets.filter((f) => f.nat === 'BUL').length;
    g.turn = 6;
    for (const r of bul) r.since = g.turn;
    smallAirMonth(g);
    expect(g.airFleets.filter((f) => f.nat === 'BUL').length).toBe(n);
    for (const r of bul) r.since = 0;
    g.turn = 7;
    smallAirMonth(g);
    expect(g.airFleets.filter((f) => f.nat === 'BUL').length).toBe(n + 2);
    // Germany at war with Bulgaria: the rows of 1943 never come, even once peace is made.
    const h = grand();
    setWar(h, 'GER', 'BUL');
    h.turn = 48;
    const m = h.airFleets.filter((f) => f.nat === 'BUL').length;
    smallAirMonth(h);
    expect(h.airFleets.filter((f) => f.nat === 'BUL').length).toBe(m);
    h.nations.GER.atWar = h.nations.GER.atWar.filter((x) => x !== 'BUL');
    h.nations.BUL.atWar = h.nations.BUL.atWar.filter((x) => x !== 'GER');
    smallAirMonth(h);
    expect(h.airFleets.filter((f) => f.nat === 'BUL').length).toBe(m);
    // Turkey's row of 1940 comes from Britain while France has fallen.
    const k = grand();
    k.nations.FRA.alive = false;
    k.turn = 6;
    smallAirMonth(k);
    expect(k.airFleets.filter((f) => f.nat === 'TUR').reduce((s, f) => s + f.ftr + f.bmr, 0)).toBe(70);
  });

  it('counts the rows before a later start as come: history left them in the order of battle', () => {
    const g = grand();
    g.turn = 14;
    openSmallAir(g);
    g.turn = 15;
    const n = g.airFleets.length;
    smallAirMonth(g);
    expect(g.airFleets.length).toBe(n);
    // The rows still to come arrive in their month.
    g.turn = 28;
    smallAirMonth(g);
    expect(g.airFleets.filter((f) => f.nat === 'SWE').length).toBeGreaterThan(0);
  });

  it('tells a smaller power\'s player the month its works open, once', () => {
    const g = grand(3, 'HUN');
    g.turn = 13;
    smallAirMonth(g);
    expect(g.headlines.filter((h) => h.text === 'Hungary now builds fighters at home: Fighter wings can be ordered on the Arsenal page.').length).toBe(1);
    g.turn = 14;
    smallAirMonth(g);
    expect(g.headlines.filter((h) => /now builds/.test(h.text)).length).toBe(1);
  });

  it('touches nothing in the historical campaigns', () => {
    const g = createGame(3, null);
    g.turn = 6;
    const n = g.airFleets.length;
    smallAirMonth(g);
    expect(g.airFleets.length).toBe(n);
    expect(factoryCeiling(g, 'SWE')).toBeNull();
    expect(whyNoFactory(g, 'SWE')).toBeNull();
  });
});

describe('ruling 213(e): a smaller power\'s industry carries half again its first-day factories', () => {
  it('counts the first day\'s factories wherever they stand, and none for a great power', () => {
    const g = grand();
    expect(firstFactories(g, 'SWE')).toBe(home(g, 'SWE').reduce((s, r) => s + r.fac, 0));
    expect(factoryCeiling(g, 'SWE')).toBe(Math.ceil(firstFactories(g, 'SWE') * 1.5));
    for (const nat of majorsOf(g)) expect(factoryCeiling(g, nat)).toBeNull();
    expect(whyNoFactory(g, 'GER')).toBeNull();
  });

  it('stops at the ceiling in the passed words, and a factory lost may be built back', () => {
    const g = grand();
    const most = factoryCeiling(g, 'SWE')!;
    const held = factoriesHeld(g, 'SWE').held;
    expect(most).toBeGreaterThan(held);
    expect(whyNoFactory(g, 'SWE')).toBeNull();
    // The Arsenal page says how far it has come.
    const r = home(g, 'SWE').find((x) => x.cap)!;
    expect(offer(g, 'SWE', 'work:factory', r.id).note).toContain(`Sweden has ${held} of the ${most} factories its industry can carry in this war.`);
    // Built up to the ceiling, with one still building.
    r.fac += most - held - 1;
    g.flags[`wk:fac:${r.id}`] = most - held - 1;
    g.nations.SWE.queue.push({ id: g.nextBuild++, what: 'work', item: 'factory', name: 'Factory', region: r.id, ordered: 1, eta: 9, cost: 30 });
    expect(whyNoFactory(g, 'SWE')).toBe(`Sweden's industry can carry ${most} factories in this war, and it has ${most}, built or building.`);
    expect(factoryTag(g, 'SWE')).toBe(`${most} of ${most} factories`);
    expect(offer(g, 'SWE', 'work:factory', r.id).why).toBe(whyNoFactory(g, 'SWE'));
    // The first-day count does not grow with what is built.
    expect(factoryCeiling(g, 'SWE')).toBe(most);
    // Ground lost takes its factories with it: the country may build them back.
    g.nations.SWE.queue = [];
    r.fac += 1;
    g.flags[`wk:fac:${r.id}`] += 1;
    expect(whyNoFactory(g, 'SWE')).toMatch(/and it has \d+\.$/);
    const lost = home(g, 'SWE').find((x) => !x.cap && x.fac > 0)!;
    lost.ctrl = 'GER';
    expect(whyNoFactory(g, 'SWE')).toBeNull();
  });

  it('gives a country with no industry its own sentence', () => {
    const g = grand();
    expect(firstFactories(g, 'BUL')).toBe(0);
    expect(whyNoFactory(g, 'BUL')).toBe('Bulgaria has no industry to build on.');
    expect(offer(g, 'BUL', 'work:factory').why).toBe('Bulgaria has no industry to build on.');
    expect(factoryTag(g, 'BUL')).toBe('No industry');
    expect(factoryTag(g, 'SWE')).toBeNull();
    expect(factoryTag(g, 'GER')).toBeNull();
  });
});
