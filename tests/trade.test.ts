// Trade agreements, in the grand campaign. Two countries at peace sign one and both are paid every month, more the
// richer they are. Each agreement needs a trading house. Goods take the best open way: across a shared border, by
// sea (where raiders sink a share), by a border between colonies, or overland across countries at peace with both.
// War, a fall or bad feeling ends an agreement; with no way open it pays nothing. The history campaigns keep the
// flat trade they always had.
import { describe, expect, it } from 'vitest';
import data from '../data/campaign/trade.json';
import { ledger } from '../src/campaign/economy';
import { atWar, createGame } from '../src/campaign/game';
import { sail, seaRisk } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import {
  TRADE, dealWorth, dealsOf, endTrade, housesOf, openTrade, signTrade, tradeBook, tradeIncome, tradePath, tradeRoute, tradeTick, wealthOf, whyNoTrade,
} from '../src/campaign/trade';
import { beginHuman, endHuman, humanUp, orderEndTrade, orderTrade, runTurnAI, stepAI, tradeLedger } from '../src/campaign/turn';
import type { Game, TradeDeal } from '../src/campaign/types';

/** A grand campaign with this country at the table, at its first month of orders. */
function playing(nat: string, seed = 1): Game {
  const g = startOpen(createGame(seed, nat));
  let guard = 0;
  while (!humanUp(g) && guard++ < 200) stepAI(g);
  beginHuman(g);
  return g;
}

function month(g: Game): void {
  endHuman(g);
  let guard = 0;
  while (!humanUp(g) && guard++ < 400) stepAI(g);
  beginHuman(g);
}

const deal = (g: Game, a: string, b: string): TradeDeal | undefined => g.trade.find((d) => (d.a === a && d.b === b) || (d.a === b && d.b === a));
const war = (g: Game, a: string, b: string): void => { if (!atWar(g, a, b)) { g.nations[a].atWar.push(b); g.nations[b].atWar.push(a); } };
const peace = (g: Game, a: string, b: string): void => { g.nations[a].atWar = g.nations[a].atWar.filter((x) => x !== b); g.nations[b].atWar = g.nations[b].atWar.filter((x) => x !== a); };
const farWorth = (seas: number): number => Math.max(TRADE.farLeast, 1 - TRADE.far * Math.max(0, seas - 1));

describe('the data', () => {
  it('names real countries, and no pair that history had is one that will never sign', () => {
    const g = startOpen(createGame(1, null));
    for (const [a, b] of [...data.pairs, ...data.never]) { expect(g.nations[a], a).toBeTruthy(); expect(g.nations[b], b).toBeTruthy(); }
    for (const [a, b] of data.pairs) expect(data.never.some(([x, y]) => (x === a && y === b) || (x === b && y === a)), `${a}-${b}`).toBe(false);
    expect(TRADE.houses.major).toBeGreaterThan(TRADE.houses.minor);
  });
});

describe('the history campaigns', () => {
  it('have no agreements: trade there is the flat sum it always was', () => {
    const g = createGame(4, 'GER');
    expect(g.trade).toEqual([]);
    expect(tradeLedger(g)).toBeNull();
    expect(whyNoTrade(g, 'GER', 'SWE')).toMatch(/grand campaign/);
    expect(signTrade(g, 'GER', 'SWE')).toBeNull();
    openTrade(g);
    tradeTick(g, 'GER');
    expect(g.trade).toEqual([]);
    expect(tradeIncome(g, 'GER')).toBe(0);
    expect(saveGame(g)).not.toContain('"trade"');
    for (let m = 0; m < 3; m++) runTurnAI(g);
    expect(g.trade).toEqual([]);
    expect(ledger(g, 'GER').income.find((l) => l.key === 'trade')?.label ?? 'Trade with friendly neutrals').toBe('Trade with friendly neutrals');
  });
});

describe('when the grand campaign opens', () => {
  it('the agreements history had are standing, and every country has filled what houses it could', () => {
    const g = startOpen(createGame(1, null));
    expect(g.flags['trade:open']).toBe(1);
    for (const [a, b] of [['GER', 'ITA'], ['GER', 'SOV'], ['GER', 'SWE'], ['UK', 'FRA'], ['UK', 'POR'], ['UK', 'NOR'], ['FRA', 'BEL'], ['FRA', 'SUI'], ['SOV', 'TUR']]) expect(deal(g, a, b), `${a}-${b}`).toBeTruthy();
    expect(housesOf(g, 'GER')).toBe(TRADE.houses.major);
    expect(housesOf(g, 'BEL')).toBe(TRADE.houses.minor);
    const seen = new Set<string>();
    for (const d of g.trade) {
      const key = [d.a, d.b].sort().join('-');
      expect(seen.has(key), key).toBe(false);
      seen.add(key);
      expect(atWar(g, d.a, d.b), key).toBe(false);
      expect(d.state, key).not.toBe('cut');
      expect(d.worth, key).toBeGreaterThan(0);
      expect(d.since).toBe(g.turn);
    }
    for (const n of Object.values(g.nations)) expect(dealsOf(g, n.id).length, n.id).toBeLessThanOrEqual(housesOf(g, n.id));
    // The great powers are all trading at once.
    for (const id of ['GER', 'UK', 'FRA', 'SOV', 'ITA']) expect(dealsOf(g, id).length, id).toBe(TRADE.houses.major);
    // Opening happens once.
    const n = g.trade.length;
    openTrade(g);
    expect(g.trade.length).toBe(n);
  });
});

describe('what an agreement is worth', () => {
  it('grows with the wealth of both countries, the same to each', () => {
    const g = startOpen(createGame(1, null));
    const d = deal(g, 'GER', 'ITA')!;
    expect(d.way).toBe('land');
    expect(d.worth).toBeCloseTo(TRADE.base + TRADE.scale * Math.sqrt(wealthOf(g, 'GER') * wealthOf(g, 'ITA')), 6);
    expect(dealWorth(g, 'GER', 'ITA')).toBeCloseTo(dealWorth(g, 'ITA', 'GER'), 9);
    expect(dealWorth(g, 'GER', 'SOV')).toBeGreaterThan(dealWorth(g, 'GER', 'LUX'));
    // A way that leaves only part of the worth is worth that part.
    expect(dealWorth(g, 'GER', 'SOV', 0.8)).toBeCloseTo(dealWorth(g, 'GER', 'SOV') * 0.8, 9);
    // What a country earns is the sum of its agreements.
    const sum = dealsOf(g, 'GER').reduce((t, x) => t + x.worth * (1 - x.lost), 0);
    expect(tradeIncome(g, 'GER')).toBeCloseTo(sum, 9);
    expect(sum).toBeGreaterThan(2);
  });

  it('is raised for a country by its own commerce buildings', () => {
    const g = startOpen(createGame(1, null));
    const plain = tradeIncome(g, 'GER');
    g.flags['cv:any'] = 1;
    g.flags[`cv:market:${g.nations.GER.capital}`] = 1;
    expect(tradeIncome(g, 'GER')).toBeCloseTo(plain * 1.04, 6);
    // Italy's side of the same agreement is not changed by Germany's markets.
    expect(tradeIncome(g, 'ITA')).toBeCloseTo(dealsOf(g, 'ITA').reduce((t, x) => t + x.worth * (1 - x.lost), 0), 9);
  });

  it('shows in the ledger as its own line', () => {
    const g = playing('GER');
    const line = ledger(g, 'GER').income.find((l) => l.key === 'trade')!;
    expect(line.label).toBe('Trade agreements');
    expect(line.amount).toBeCloseTo(tradeIncome(g, 'GER'), 9);
    expect(line.count).toBe(dealsOf(g, 'GER').length);
  });
});

describe('the way the goods go', () => {
  it('is across the border where home ground touches home ground', () => {
    const g = startOpen(createGame(1, null));
    expect(tradeRoute(g, 'GER', 'ITA')).toEqual({ way: 'land', seas: [], lost: 0, keeps: 1 });
    expect(tradeRoute(g, 'FRA', 'BEL')).toMatchObject({ way: 'land', keeps: 1 });
    expect(tradeRoute(g, 'UK', 'IRL')).toMatchObject({ way: 'land', keeps: 1 });
  });

  it('is by sea between countries the sea divides, worth less the more seas are crossed', () => {
    const g = startOpen(createGame(1, null));
    // Britain and France meet in Palestine and Syria, but their trade crosses the Channel.
    const r = tradeRoute(g, 'UK', 'FRA');
    expect(r.way).toBe('sea');
    expect(r.seas.map((z) => g.seas[z].name)).toEqual(['English Channel']);
    expect(r.keeps).toBe(1);
    let long = 0;
    for (const a of Object.keys(g.nations)) {
      for (const b of Object.keys(g.nations)) {
        if (a >= b || !g.nations[a].alive || !g.nations[b].alive || g.nations[a].dormant || g.nations[b].dormant) continue;
        const x = tradeRoute(g, a, b);
        if (x.way !== 'sea') continue;
        expect(x.keeps, `${a}-${b}`).toBeCloseTo(farWorth(x.seas.length), 9);
        // Each sea on the way borders the next.
        for (let i = 1; i < x.seas.length; i++) expect(g.seas[x.seas[i - 1]].adj, `${a}-${b}`).toContain(x.seas[i]);
        if (x.seas.length >= 3) long++;
      }
    }
    expect(long).toBeGreaterThan(0);
  });

  it('uses any coast for a country with no harbour on the map', () => {
    const g = startOpen(createGame(1, null));
    const bare = Object.values(g.nations).filter((n) => n.alive && !n.dormant && !n.offmap && !g.regions.some((r) => r.ctrl === n.id && r.port) && g.regions.some((r) => r.ctrl === n.id && r.seas.length > 0));
    expect(bare.map((n) => n.id)).toContain('SWE');
    // Swedish ore reaches Germany across the water, though no Swedish harbour is drawn.
    expect(tradeRoute(g, 'GER', 'SWE').way).toBe('sea');
    expect(tradeRoute(g, 'SWE', 'EST').way).toBe('sea');
    // A country with no coast at all has no way by sea.
    expect(g.regions.some((r) => r.ctrl === 'SUI' && r.seas.length > 0)).toBe(false);
    expect(tradeRoute(g, 'SUI', 'UK').way).not.toBe('sea');
  });

  it('is by a border between colonies where the sea would be the long way round', () => {
    const g = startOpen(createGame(1, null));
    const r = tradeRoute(g, 'UK', 'IRQ');
    expect(r).toMatchObject({ way: 'land', keeps: TRADE.colony });
    expect(g.regions[r.via!].ctrl).toBe('UK');
    expect(g.regions[r.via!].overseas).toBe(true);
    expect(g.regions[r.via!].nb.some((j) => g.regions[j].ctrl === 'IRQ')).toBe(true);
  });

  it('is overland across countries at peace with both, for a toll, and never across one at war with either', () => {
    const g = startOpen(createGame(1, null));
    let r = tradeRoute(g, 'GER', 'ROM');
    expect(r.way).toBe('land');
    expect(r.through).toHaveLength(1);
    expect(r.keeps).toBeCloseTo(1 - TRADE.transit.toll, 9);
    // Close each country the goods cross in turn: the route finds another way or gives out, and never uses a closed one.
    const closed: string[] = [];
    for (let k = 0; k < 8 && r.way === 'land' && r.through?.length; k++) {
      for (const c of r.through) {
        expect(atWar(g, c, 'GER') || atWar(g, c, 'ROM'), c).toBe(false);
        expect(closed).not.toContain(c);
      }
      expect(r.through.length).toBeLessThanOrEqual(TRADE.transit.most);
      expect(r.keeps).toBeCloseTo(Math.max(TRADE.farLeast, 1 - TRADE.transit.toll * r.through.length), 9);
      closed.push(r.through[0]);
      war(g, r.through[0], 'ROM');
      r = tradeRoute(g, 'GER', 'ROM');
    }
    expect(closed.length).toBeGreaterThan(1);
    // Colonies carry no such traffic: Britain does not reach Turkey through Palestine and French Syria.
    const far = tradeRoute(g, 'UK', 'TUR');
    expect(far.through ?? []).toEqual([]);
  });

  it('by sea loses a share to raiders, and takes the way that pays best', () => {
    const g = playing('UK', 3);
    for (let m = 0; m < 6 && !atWar(g, 'UK', 'GER'); m++) month(g);
    expect(atWar(g, 'UK', 'GER')).toBe(true);
    const pay = (): number => { const r = tradeRoute(g, 'UK', 'FRA'); return r.keeps * (1 - r.lost); };
    const before = pay();
    const channel = g.seas.find((z) => z.name === 'English Channel')!.id;
    const subs = g.fleets.filter((f) => f.nat === 'GER' && f.ships.ss > 0).sort((a, b) => b.ships.ss - a.ships.ss)[0];
    expect(sail(g, subs, channel, 'raid')).toBe(true);
    const risk = seaRisk(g, channel, 'UK');
    expect(risk).toBeGreaterThan(0);
    const r = tradeRoute(g, 'UK', 'FRA');
    expect(r.keeps * (1 - r.lost)).toBeLessThanOrEqual(before + 1e-9);
    if (r.seas.includes(channel)) {
      expect(r.lost).toBeGreaterThan(0);
      expect(r.lost).toBeLessThanOrEqual(TRADE.sea.most);
    }
    // The loss is the chance of meeting the enemy on the way, times 'loss', never more than 'most'.
    let safe = 1;
    for (const z of r.seas) safe *= 1 - Math.max(seaRisk(g, z, 'UK'), g.nations.FRA.atWar.length ? seaRisk(g, z, 'FRA') : 0);
    expect(r.lost).toBeCloseTo(Math.min(TRADE.sea.most, (1 - safe) * TRADE.sea.loss), 9);
    // The agreement is looked over at the start of the month: it is marked as raided from 'raided' upward.
    tradeTick(g, 'UK');
    const d = deal(g, 'UK', 'FRA');
    if (d) expect(d.state).toBe(d.lost >= TRADE.sea.raided ? 'raided' : 'open');
    const row = tradeLedger(g)!.deals.find((x) => x.nat === 'FRA');
    if (row && d) { expect(row.pays).toBeCloseTo(d.worth * (1 - d.lost) * (1 + tradeLedger(g)!.boost), 9); expect(row.pays).toBeLessThanOrEqual(row.full + 1e-9); }
  });

  it('draws as a line on the map for every agreement with a way open', () => {
    const g = startOpen(createGame(1, null));
    for (const d of g.trade) {
      const p = tradePath(g, d);
      expect(p.length, `${d.a}-${d.b}`).toBeGreaterThanOrEqual(2);
      for (const [x, y] of p) { expect(Number.isFinite(x)).toBe(true); expect(Number.isFinite(y)).toBe(true); }
      if (d.way === 'sea') expect(p.length, `${d.a}-${d.b}`).toBeGreaterThanOrEqual(d.seas.length);
    }
    expect(tradePath(g, { ...g.trade[0], state: 'cut' })).toEqual([]);
  });
});

describe('an agreement with no way open', () => {
  /** Switzerland trading with Hungary across Germany, then every way between them closed by war. */
  function boxedIn(human: string | null): Game {
    const g = human ? playing(human) : startOpen(createGame(1, null));
    for (const id of ['SUI', 'HUN']) for (const d of dealsOf(g, id)) endTrade(g, d.a, d.b);
    const d = signTrade(g, 'SUI', 'HUN', new Map(), true)!;
    expect(d).toMatchObject({ way: 'land', state: 'open' });
    expect(d.through!.length).toBeGreaterThan(0);
    for (let k = 0; k < 12; k++) {
      const r = tradeRoute(g, 'SUI', 'HUN');
      if (r.way === 'none') return g;
      expect(r.way).toBe('land');
      war(g, r.through![r.through!.length - 1], 'HUN');
    }
    throw new Error('a way stayed open');
  }

  it('pays nothing while that lasts, and the player is told', () => {
    const g = boxedIn('SUI');
    const before = tradeIncome(g, 'SUI');
    expect(before).toBeGreaterThan(0);
    tradeTick(g, 'SUI');
    const d = deal(g, 'SUI', 'HUN')!;
    expect(d).toMatchObject({ state: 'cut', lost: 1 });
    expect(tradeIncome(g, 'SUI')).toBe(0);
    expect(g.headlines.some((h) => /No goods are getting through from Hungary/.test(h.text))).toBe(true);
    const row = tradeLedger(g)!.deals.find((x) => x.nat === 'HUN')!;
    expect(row).toMatchObject({ state: 'cut', pays: 0, route: 'No way open', path: [] });
    expect(row.full).toBeGreaterThan(0);
    // The house is still taken until he ends it himself.
    expect(tradeLedger(g)!.used).toBe(1);
    // Peace on the way opens it again.
    for (const n of Object.values(g.nations)) peace(g, n.id, 'HUN');
    tradeTick(g, 'SUI');
    expect(deal(g, 'SUI', 'HUN')!.state).toBe('open');
    expect(tradeIncome(g, 'SUI')).toBeGreaterThan(0);
  });

  it('is told to the player in the other country\'s month too', () => {
    const g = boxedIn('SUI');
    const n = g.headlines.length;
    tradeTick(g, 'HUN');
    expect(deal(g, 'SUI', 'HUN')!.state).toBe('cut');
    expect(g.headlines.slice(n).some((h) => /No goods are getting through from Hungary/.test(h.text))).toBe(true);
  });

  it('is given up by two computer-run countries, who look elsewhere', () => {
    const g = boxedIn(null);
    tradeTick(g, 'SUI');
    expect(deal(g, 'SUI', 'HUN')).toBeUndefined();
  });
});

describe('signing and ending', () => {
  it('costs the player political capital and needs a free trading house', () => {
    const g = playing('GER');
    const book = tradeLedger(g)!;
    expect(book).toMatchObject({ nat: 'GER', used: 3, houses: 3, most: TRADE.houses.most, cost: TRADE.sign.pc });
    expect(book.deals.map((d) => d.nat).sort()).toEqual(['ITA', 'SOV', 'SWE']);
    // The best agreement is listed first, and each says how its goods go.
    for (let i = 1; i < book.deals.length; i++) expect(book.deals[i - 1].full).toBeGreaterThanOrEqual(book.deals[i].full);
    expect(book.deals.find((d) => d.nat === 'ITA')!.route).toBe('Overland');
    expect(book.deals.find((d) => d.nat === 'SWE')!.route).toMatch(/^By sea through the /);
    expect(book.deals.find((d) => d.nat === 'SOV')!.route).toMatch(/^Overland across /);
    // Every house is in use: nobody can be signed, but those who would sign are marked.
    expect(book.offers.some((o) => o.why === null)).toBe(false);
    const would = book.offers.filter((o) => o.full);
    expect(would.length).toBeGreaterThan(0);
    expect(whyNoTrade(g, 'GER', would[0].nat)).toMatch(/Every trading house you have is in use/);
    expect(orderTrade(g, would[0].nat)).toBe(false);
    // End one, and its house is free for the best of them.
    expect(orderEndTrade(g, 'SWE')).toBe(true);
    expect(deal(g, 'GER', 'SWE')).toBeUndefined();
    const open = tradeLedger(g)!.offers.filter((o) => o.why === null);
    expect(open.length).toBeGreaterThan(0);
    for (let i = 1; i < open.length; i++) expect(open[i - 1].pays).toBeGreaterThanOrEqual(open[i].pays);
    g.nations.GER.pc = 5;
    expect(orderTrade(g, open[0].nat)).toBe(true);
    expect(g.nations.GER.pc).toBe(5 - TRADE.sign.pc);
    expect(deal(g, 'GER', open[0].nat)).toMatchObject({ since: g.turn, state: 'open' });
    expect(tradeLedger(g)!.used).toBe(3);
  });

  it('is refused without the political capital', () => {
    const g = playing('GER');
    orderEndTrade(g, 'SWE');
    g.nations.GER.pc = TRADE.sign.pc - 1;
    const o = tradeLedger(g)!.offers.find((x) => x.poor)!;
    expect(o.why).toMatch(/political capital/);
    expect(orderTrade(g, o.nat)).toBe(false);
    expect(g.nations.GER.pc).toBe(TRADE.sign.pc - 1);
    expect(dealsOf(g, 'GER')).toHaveLength(2);
  });

  it('is refused between enemies, twice with the same country, with a barred pair, and where feeling runs too high', () => {
    const g = playing('GER');
    expect(whyNoTrade(g, 'GER', 'FRA')).toMatch(/at war with France/);
    expect(whyNoTrade(g, 'GER', 'UK')).toMatch(/at war with the United Kingdom/);
    expect(whyNoTrade(g, 'GER', 'ITA')).toMatch(/already trade with Italy/);
    expect(whyNoTrade(g, 'GER', 'USA')).toMatch(/keeps to itself/);
    // Vichy France and Britain never sign.
    g.nations.VIC.alive = true; g.nations.VIC.dormant = false;
    expect(whyNoTrade(g, 'UK', 'VIC')).toMatch(/will not sign/);
    // A neutral whose people are set against an alliance will not trade with its members.
    g.nations.ESP.opinion = -TRADE.cold;
    expect(whyNoTrade(g, 'GER', 'ESP')).toMatch(/Feeling in Spain runs too strongly/);
    g.nations.ESP.opinion = -TRADE.cold + 1;
    expect(whyNoTrade(g, 'GER', 'ESP') ?? '').not.toMatch(/Feeling/);
    g.nations.ESP.opinion = TRADE.cold;
    expect(whyNoTrade(g, 'UK', 'ESP')).toMatch(/Feeling in Spain/);
  });

  it('a merchant quarter in the capital opens a house for one more agreement', () => {
    const g = playing('GER');
    expect(tradeLedger(g)!.offers.some((o) => o.why === null)).toBe(false);
    g.flags['cv:any'] = 1;
    g.flags[`cv:market:${g.nations.GER.capital}`] = 2;
    const book = tradeLedger(g)!;
    expect(book.houses).toBe(4);
    expect(book.boost).toBeCloseTo(0.08, 9);
    expect(book.offers.some((o) => o.why === null)).toBe(true);
  });
});

describe('what ends an agreement', () => {
  it('war between the two', () => {
    const g = playing('GER');
    expect(deal(g, 'GER', 'SOV')).toBeTruthy();
    war(g, 'GER', 'SOV');
    tradeTick(g, 'GER');
    expect(deal(g, 'GER', 'SOV')).toBeUndefined();
    expect(g.headlines.some((h) => /War ends your trade with Soviet Union/.test(h.text))).toBe(true);
  });

  it('the fall of either', () => {
    const g = playing('GER');
    g.nations.SWE.alive = false;
    tradeTick(g, 'GER');
    expect(deal(g, 'GER', 'SWE')).toBeUndefined();
    expect(g.headlines.some((h) => /Your trade with Sweden is at an end/.test(h.text))).toBe(true);
  });

  it('feeling in a neutral country turning against the other\'s alliance', () => {
    const g = playing('GER');
    g.nations.SWE.opinion = -TRADE.cold - 5;
    tradeTick(g, 'SWE');
    expect(deal(g, 'GER', 'SWE')).toBeUndefined();
    expect(g.headlines.some((h) => /Sweden breaks off its trade agreement with you/.test(h.text))).toBe(true);
  });
});

describe('computer-run countries', () => {
  it('fill a free trading house with the partner that pays them most', () => {
    const g = startOpen(createGame(1, null));
    endTrade(g, 'GER', 'ITA');
    expect(dealsOf(g, 'GER')).toHaveLength(2);
    tradeTick(g, 'GER');
    expect(dealsOf(g, 'GER')).toHaveLength(3);
  });

  it('never sign with the player unasked', () => {
    const g = playing('GER');
    endTrade(g, 'GER', 'ITA');
    for (const n of Object.values(g.nations)) if (n.id !== 'GER') tradeTick(g, n.id);
    expect(dealsOf(g, 'GER')).toHaveLength(2);
    for (let m = 0; m < 3; m++) month(g);
    expect(dealsOf(g, 'GER').length).toBeLessThanOrEqual(2);
  });

  it('keep trading through a long war without breaking the rules', () => {
    const g = startOpen(createGame(5, null));
    for (let m = 0; m < 18; m++) {
      runTurnAI(g);
      const seen = new Set<string>();
      for (const d of g.trade) {
        const key = [d.a, d.b].sort().join('-');
        expect(seen.has(key), `T${g.turn} ${key}`).toBe(false);
        seen.add(key);
        expect(d.worth).toBeGreaterThan(0);
        expect(d.lost).toBeGreaterThanOrEqual(0);
        expect(d.lost).toBeLessThanOrEqual(1);
      }
      for (const n of Object.values(g.nations)) expect(dealsOf(g, n.id).length, `T${g.turn} ${n.id}`).toBeLessThanOrEqual(housesOf(g, n.id));
    }
    // The world is still trading, and nobody trades with a country it is fighting for longer than a month.
    expect(g.trade.length).toBeGreaterThan(10);
    for (const n of Object.values(g.nations)) { if (n.alive) tradeTick(g, n.id); }
    for (const d of g.trade) expect(atWar(g, d.a, d.b), `${d.a}-${d.b}`).toBe(false);
  }, 30_000);
});

describe('saving', () => {
  it('keeps every agreement as it stands', () => {
    const g = playing('GER');
    const back = loadGame(saveGame(g))!;
    expect(back.trade).toEqual(g.trade);
    expect(tradeIncome(back, 'GER')).toBeCloseTo(tradeIncome(g, 'GER'), 9);
    expect(tradeBook(back, 'GER')!.deals.map((d) => d.route)).toEqual(tradeBook(g, 'GER')!.deals.map((d) => d.route));
  });

  it('opens trade in a grand campaign saved before there was any', () => {
    const g = playing('GER');
    const old = JSON.parse(saveGame(g));
    delete old.trade;
    delete old.flags['trade:open'];
    const back = loadGame(JSON.stringify(old))!;
    expect(back.flags['trade:open']).toBe(1);
    expect(back.trade.length).toBeGreaterThan(10);
    expect(dealsOf(back, 'GER').length).toBeGreaterThan(0);
  });
});
