// A neutral's bargains (release 2; Brian's yes of 22 November 2026, the design director's frame of 03:42 UTC and
// rulings of 03:47 and 03:55 UTC). A country at peace outside both alliances sells its goods or a right to one country
// at war a term at a time; the countries at war bid; the seller's seat chooses. Switched by stage.json 'bargains': off,
// no campaign carries any of it.
import { describe, expect, it } from 'vitest';
import stage from '../data/campaign/stage.json';
import {
  BARGAINS, bargainCost, bargainLanes, bargainOutputLoss, bargainRules, bargainSales, bargainsMonth, bargainsView, baseRight,
  biddersOf, feedClaims, heldOf, lapseFeedClaims, patienceOf, placeBid, recordBuyer, replacedDeal, rowById, rowsOf, sells, startBargains, whyNoBargain,
} from '../src/campaign/bargains';
import { fieldClass } from '../src/campaign/air';
import { ledger, upkeep } from '../src/campaign/economy';
import { atWar, createGame } from '../src/campaign/game';
import { setStop, stopOf } from '../src/campaign/history';
import { airfieldOpenTo } from '../src/campaign/prizes';
import { lanesTo } from '../src/campaign/resources';
import { tradeRoute } from '../src/campaign/trade';
import { loadGame, saveGame } from '../src/campaign/save';
import { BARGAINS_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, humanUp, orderBid, orderHistory, orderSellTo, runTurnAI, stepAI } from '../src/campaign/turn';
import type { BargainHeld, Game } from '../src/campaign/types';

/** A grand campaign with the bargains on, at this stop of the dial, nobody at the table. */
function world(stop = 1, seed = 1): Game {
  const g = startOpen(createGame(seed, null));
  if (stop !== stopOf(g)) setStop(g, stop, true);
  startBargains(g);
  return g;
}

/** A grand campaign with the bargains on and this country at the table, at its first month of orders. */
function playing(nat: string, stop = 1, seed = 1): Game {
  const g = startOpen(createGame(seed, nat));
  if (stop !== stopOf(g)) setStop(g, stop, true);
  startBargains(g);
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

const war = (g: Game, a: string, b: string): void => { if (!atWar(g, a, b)) { g.nations[a].atWar.push(b); g.nations[b].atWar.push(a); } };

describe('the switch', () => {
  it('is off in release 1: no campaign carries the bargains', () => {
    expect((stage as { bargains?: boolean }).bargains).toBe(false);
    if (BARGAINS_STAGE) return; // (a run with TC_BARGAINS set)
    const g = startOpen(createGame(1, null));
    expect(g.bargains).toBeUndefined();
    expect(g.flags.bargains).toBeUndefined();
    expect(bargainRules(g)).toBe(false);
    expect(rowsOf(g)).toEqual([]);
    expect(bargainsView(g, 'GER')).toEqual([]);
    expect(replacedDeal(g, 'ore')).toBe(false);
    expect(bargainOutputLoss(g, 'GER')).toBe(0);
    expect(ledger(g, 'SWE').income.some((l) => l.key === 'bargains')).toBe(false);
    bargainsMonth(g);
    expect(g.bargains).toBeUndefined();
  });

  it('never reaches a history campaign', () => {
    const g = createGame(1, null);
    startBargains(g);
    expect(g.bargains).toBeUndefined();
    expect(bargainRules(g)).toBe(false);
  });
});

describe('the rows', () => {
  it('the six great powers sell no row of their own, and Romania none of its own exports', () => {
    const g = world();
    const sellers = new Set(rowsOf(g).map((r) => r.seller));
    for (const p of ['GER', 'UK', 'FRA', 'ITA', 'SOV', 'USA']) expect(sellers.has(p)).toBe(false);
    expect(rowsOf(g).some((r) => r.generic && r.seller === 'ROM')).toBe(false);
  });

  it('every row has a terms and a sign of the map\'s kit', () => {
    const g = world();
    const kit = ['ore', 'oil', 'chrome', 'wolfram', 'beef', 'base', 'harbour', 'exports'];
    for (const r of rowsOf(g)) expect(kit).toContain(r.sign);
    expect(BARGAINS.term).toBe(12);
  });
});

describe('the first day, by the record', () => {
  it('stands the record\'s terms of September 1939 at every stop', () => {
    for (const stop of [1, 3, 5]) {
      const g = world(stop);
      expect(heldOf(g, 'swe-ore')?.buyer).toBe('GER');
      expect(heldOf(g, 'rom-oil')?.buyer).toBe('GER');
      expect(heldOf(g, 'rom-oil-ita')?.buyer).toBe('ITA');
      expect(heldOf(g, 'irl-food')?.buyer).toBe('UK');
      expect(heldOf(g, 'swe-ore')?.until).toBe(g.turn + BARGAINS.term - 1);
    }
  });

  it('the ore is Sweden\'s to sell: its deal follows the bargain, once', () => {
    const g = world();
    expect(replacedDeal(g, 'ore')).toBe(true);
    const ore = lanesTo(g, 'GER').filter((l) => l.id === 'ore');
    expect(ore).toHaveLength(1);
    expect(ore[0].text).toMatch(/Bought from/);
    expect(bargainLanes(g, 'UK').some((l) => l.id === 'ore')).toBe(false);
  });

  it('a campaign set at stop 1 after the bargains open stands the record\'s terms of its first day at that stop', () => {
    // (Portugal's wolfram goes to Germany from January 1940 by the record, while Germany has no way to it: at stop 3 it
    // cannot bid, at stops 1 and 2 the record's buyer wins its term though its way is shut.)
    const g = startOpen(createGame(1, null));
    g.turn = 6;
    startBargains(g);
    expect(heldOf(g, 'por-wolfram')?.buyer ?? null).toBeNull();
    const ore = { ...heldOf(g, 'swe-ore')! };
    expect(orderHistory(g, 1, true)).toBe(true);
    expect(heldOf(g, 'por-wolfram')?.buyer).toBe('GER');
    // (A term already standing is left as it was.)
    expect(heldOf(g, 'swe-ore')).toEqual(ore);
  });

  it('the record\'s buyer is read at stops 1 and 2 only', () => {
    const g = world(1);
    const row = rowById(g, 'tur-chrome')!;
    expect(recordBuyer(g, row, 1)).toBeNull();
    expect(recordBuyer(g, row, 5)).toBe('UK');
    expect(recordBuyer(g, row, 41)).toBe('GER');
    setStop(g, 3, true);
    expect(recordBuyer(g, row, 41)).toBeUndefined();
  });
});

describe('a round of bids', () => {
  it('opens in a term\'s last month and the computer\'s seller chooses the next month, by the record at stop 1', () => {
    const g = world(1);
    const h = heldOf(g, 'swe-ore')!;
    h.until = g.turn;
    bargainsMonth(g);
    expect(h.round?.opened).toBe(g.turn);
    expect(Object.keys(h.round!.bids)).toContain('GER');
    g.turn++;
    bargainsMonth(g);
    expect(h.round).toBeUndefined();
    expect(h.buyer).toBe('GER');
    expect(h.since).toBe(g.turn);
    expect(h.until).toBe(g.turn + BARGAINS.term - 1);
  });

  it('a term that has run out ends in the month a late round opens', () => {
    const g = world(1);
    const h = heldOf(g, 'swe-ore')!;
    h.until = g.turn - 1;
    bargainsMonth(g);
    expect(h.round?.opened).toBe(g.turn);
    expect(h.buyer).toBeNull();
    expect(bargainSales(g, 'SWE')).toBe(0);
  });

  it('only a country at war bids, unless the record or the old deal names it', () => {
    const g = world(1);
    const row = rowById(g, 'swe-ore')!;
    expect(whyNoBargain(g, row, 'USA')).toMatch(/at war/);
    expect(whyNoBargain(g, row, 'GER')).toBeNull();
    expect(biddersOf(g, row)).not.toContain('SWE');
  });

  it('a country at war with the seller cannot buy from it', () => {
    const g = world(3);
    const row = rowById(g, 'tur-chrome')!;
    war(g, 'UK', 'TUR');
    expect(sells(g, 'TUR')).toBe(false);
    expect(whyNoBargain(g, row, 'UK')).not.toBeNull();
  });
});

describe('the player\'s seat', () => {
  it('a seller in the player\'s hands gets nobody until he chooses, and is reminded once', () => {
    const g = playing('SWE', 3);
    const h = heldOf(g, 'swe-ore')!;
    h.until = g.turn;
    bargainsMonth(g);
    expect(h.round).toBeDefined();
    const opened = h.round!.opened;
    const warn = (): number => g.headlines.filter((x) => x.warn === 'Bids are waiting').length;
    const before = warn();
    expect(h.buyer).toBe('GER');
    g.turn++;
    bargainsMonth(g);
    expect(h.round?.opened).toBe(opened);
    expect(warn()).toBe(before + 1);
    // The term has run out and he has not chosen: nobody buys, nothing is paid, nothing is carried.
    expect(h.buyer).toBeNull();
    expect(bargainSales(g, 'SWE')).toBe(0);
    expect(lanesTo(g, 'GER').some((l) => l.id === 'ore' && l.text.startsWith('Bought'))).toBe(false);
    g.turn++;
    bargainsMonth(g);
    expect(h.round).toBeDefined();
    expect(warn()).toBe(before + 1);
  });

  it('he sells to a bidder or to nobody on the card', () => {
    const g = playing('SWE', 3);
    const h = heldOf(g, 'swe-ore')!;
    war(g, 'ITA', 'FRA');
    h.round = { opened: g.turn - 1, bids: { GER: 2, ITA: 1.5 } };
    expect(orderSellTo(g, 'swe-ore', 'USA')).toMatch(/no bid/);
    expect(orderSellTo(g, 'swe-ore', 'ITA')).toBeNull();
    expect(h.buyer).toBe('ITA');
    expect(h.price).toBe(1.5);
    expect(h.round).toBeUndefined();
    h.round = { opened: g.turn, bids: { GER: 2 } };
    expect(orderSellTo(g, 'swe-ore', null)).toBeNull();
    expect(h.buyer).toBeNull();
    expect(h.until).toBe(g.turn + BARGAINS.term);
  });

  it('the bids stand while he waits, and one that can no longer be taken is refused when he chooses it', () => {
    const g = playing('SWE', 3);
    const h = heldOf(g, 'swe-ore')!;
    h.round = { opened: g.turn - 1, bids: { GER: 2, ITA: 1.5 } };
    expect(whyNoBargain(g, rowById(g, 'swe-ore')!, 'ITA')).not.toBeNull();
    bargainsMonth(g);
    expect(Object.keys(h.round?.bids ?? {}).sort()).toEqual(['GER', 'ITA']);
    expect(orderSellTo(g, 'swe-ore', 'ITA')).not.toBeNull();
    expect(orderSellTo(g, 'swe-ore', 'GER')).toBeNull();
    expect(h.buyer).toBe('GER');
  });

  it('a buyer in the player\'s hands bids by steps, in the month the round opens', () => {
    const g = playing('UK', 3);
    const row = rowById(g, 'tur-chrome')!;
    const h: BargainHeld = heldOf(g, row.id) ?? (g.bargains!.rows[row.id] = { buyer: null, price: 0, since: g.turn, until: g.turn, way: 'none', seas: [], lost: 0, share: 0 });
    h.round = { opened: g.turn, bids: {} };
    expect(whyNoBargain(g, row, 'UK')).toBeNull();
    expect(orderBid(g, row.id, 0.1)).toMatch(/least bid/);
    expect(orderBid(g, row.id, 1e6)).toMatch(/treasury/);
    expect(orderBid(g, row.id, BARGAINS.bid.floor)).toBeNull();
    expect(h.round.bids.UK).toBe(BARGAINS.bid.floor);
    h.round.opened = g.turn - 1;
    expect(orderBid(g, row.id, BARGAINS.bid.floor)).toMatch(/bids are in/);
  });
});

describe('what a sale pays and carries', () => {
  it('the buyer pays the seller the price for the share that arrives', () => {
    const g = world(1);
    const h = heldOf(g, 'swe-ore')!;
    h.price = 3; h.share = 0;
    const sales0 = bargainSales(g, 'SWE'), cost0 = bargainCost(g, 'GER');
    h.share = 0.5;
    expect(bargainSales(g, 'SWE') - sales0).toBeCloseTo(1.5, 6);
    expect(bargainCost(g, 'GER') - cost0).toBeCloseTo(1.5, 6);
    expect(upkeep(g, 'GER').find((l) => l.key === 'bargains')?.amount).toBeCloseTo(bargainCost(g, 'GER'), 6);
    expect(ledger(g, 'SWE').income.find((l) => l.key === 'bargains')?.amount).toBeCloseTo(bargainSales(g, 'SWE'), 6);
  });

  it('Germany loses the ore\'s share of its output when it goes without, once the lag has passed', () => {
    const g = world(1);
    const h = heldOf(g, 'swe-ore')!;
    h.share = 1;
    expect(bargainOutputLoss(g, 'GER')).toBe(0);
    g.bargains!.got['swe-ore:GER'] = g.turn - 1;
    h.buyer = 'ITA';
    expect(bargainOutputLoss(g, 'GER')).toBeCloseTo(0.08, 6);
    h.buyer = 'GER'; h.share = 0;
    expect(bargainOutputLoss(g, 'GER')).toBeCloseTo(0.08, 6);
  });

  it('a base right opens the airfield to its buyer and counts it as a field', () => {
    const g = world(1);
    const az = g.byName.Azores;
    expect(az).toBeDefined();
    expect(baseRight(g, 'UK', az.id)).toBe(false);
    const h: BargainHeld = (g.bargains!.rows['por-azores'] = { buyer: 'UK', price: 0.75, since: g.turn, until: g.turn + 11, way: 'land', seas: [], lost: 0, share: 1 });
    expect(baseRight(g, 'UK', az.id)).toBe(true);
    expect(baseRight(g, 'GER', az.id)).toBe(false);
    expect(airfieldOpenTo(g, 'UK', az)).toBe(true);
    expect(fieldClass(g, az.id)).toBeGreaterThanOrEqual(2);
    const v = bargainsView(g, 'UK').find((x) => x.id === 'por-azores')!;
    expect(v.base).toBe(az.id);
    expect(v.cargo.moved).toBe(false);
    h.buyer = null;
    expect(baseRight(g, 'UK', az.id)).toBe(false);
  });
});

describe('the record\'s buyer keeps to the book', () => {
  it('at stops 1 and 2 it wins its term while its way is shut, keeps it, and pays nothing until the way opens', () => {
    const g = world(1);
    // (Portugal's wolfram goes to Germany from January 1940 by the record; while France stands Germany has no way to it.)
    expect(tradeRoute(g, 'POR', 'GER').way).toBe('none');
    const h0 = heldOf(g, 'por-wolfram');
    expect(h0?.buyer ?? null).toBeNull();
    g.turn = 5;
    bargainsMonth(g);
    const h = heldOf(g, 'por-wolfram')!;
    expect(Object.keys(h.round?.bids ?? {})).toContain('GER');
    g.turn++;
    bargainsMonth(g);
    expect(h.buyer).toBe('GER');
    expect(h.share).toBe(0);
    const cost = bargainCost(g, 'GER');
    h.price = 7;
    expect(bargainCost(g, 'GER')).toBeCloseTo(cost, 9);
    for (let i = 0; i < BARGAINS.cutMonths + 2; i++) { g.turn++; bargainsMonth(g); }
    expect(h.buyer).toBe('GER');
  });

  it('a term begun before the record\'s change is cut whenever it is found running past it, at stops 1 and 2', () => {
    // (Sweden's ore goes to nobody from turn 64 by the record. A term that began at turn 60 and is found running at
    // turn 66, by a dial turned to stop 1 that month or a cut missed, ends now; one begun after the change runs its year.)
    for (const since of [60, 65]) {
      const g = world(1);
      const h = heldOf(g, 'swe-ore')!;
      g.turn = 66;
      h.buyer = 'GER'; h.since = since; h.until = since + BARGAINS.term - 1;
      bargainsMonth(g);
      if (since === 60) expect(h.until).toBe(66);
      else expect(h.until).toBe(since + BARGAINS.term - 1);
    }
  });

  it('at stop 3 a buyer with no way cannot bid', () => {
    const g = world(3);
    expect(whyNoBargain(g, rowById(g, 'por-wolfram')!, 'GER')).toMatch(/No way/);
  });

  it('a term the seller gave by judgment, because the record\'s buyer made no bid, runs its year', () => {
    const g = world(1);
    const row = rowById(g, 'irl-food')!;
    const other = biddersOf(g, row).find((b) => b !== 'UK');
    expect(other).toBeDefined();
    const h = heldOf(g, 'irl-food')!;
    h.round = { opened: g.turn - 1, bids: { [other!]: 1 } };
    bargainsMonth(g);
    expect(h.buyer).toBe(other);
    const until = h.until;
    g.turn++;
    bargainsMonth(g);
    expect(h.until).toBe(until);
    expect(h.round).toBeUndefined();
  });
});

describe('a seller that falls', () => {
  it('costs its buyer no output once the buyer\'s side holds its capital: the mines are theirs', () => {
    const g = world(3);
    const row = rowById(g, 'tur-chrome')!;
    g.bargains!.got['tur-chrome:GER'] = g.turn - (row.lag ?? 0) - 2;
    war(g, 'GER', 'TUR');
    const T = g.nations.TUR;
    const cap = g.regions[T.capital];
    expect(bargainOutputLoss(g, 'GER')).toBeCloseTo(row.output!.GER, 6);
    T.alive = false;
    expect(bargainOutputLoss(g, 'GER')).toBeCloseTo(row.output!.GER, 6);
    cap.ctrl = 'GER';
    expect(bargainOutputLoss(g, 'GER')).toBe(0);
  });
});

describe('a seller that stops selling', () => {
  it('ends its terms, and opens a round the first month it sells again', () => {
    const g = world(3);
    const h = heldOf(g, 'swe-ore')!;
    war(g, 'SOV', 'SWE');
    bargainsMonth(g);
    expect(h.buyer).toBeNull();
    expect(h.until).toBeLessThanOrEqual(g.turn);
    g.nations.SOV.atWar = g.nations.SOV.atWar.filter((x) => x !== 'SWE'); g.nations.SWE.atWar = [];
    g.turn++;
    bargainsMonth(g);
    expect(h.round?.opened).toBe(g.turn);
  });

  it('gives the ore back to release 1\'s deal, and Germany loses its share only as release 1 does', () => {
    const g = world(1);
    g.bargains!.got['swe-ore:GER'] = g.turn - 1;
    war(g, 'GER', 'SWE');
    expect(replacedDeal(g, 'ore')).toBe(false);
    expect(bargainOutputLoss(g, 'GER')).toBe(0);
    expect(lanesTo(g, 'GER').filter((l) => l.id === 'ore')).toHaveLength(1);
    // (No second ore line from the bargain, and nothing paid for it, in the month the war begins.)
    expect(bargainLanes(g, 'GER').some((l) => l.name === rowById(g, 'swe-ore')!.name)).toBe(false);
    heldOf(g, 'swe-ore')!.price = 9;
    expect(bargainSales(g, 'SWE')).toBe(0);
  });
});

describe('the price in patience', () => {
  it('each enemy great power loses patience with the seller that feeds its enemy, and gets it back after', () => {
    const g = world(1);
    expect(patienceOf(g, 'UK', 'SWE')).toBe(100);
    bargainsMonth(g);
    expect(patienceOf(g, 'UK', 'SWE')).toBe(100 - BARGAINS.patience.fall);
    expect(patienceOf(g, 'ITA', 'SWE')).toBe(100);
    heldOf(g, 'swe-ore')!.buyer = null;
    for (const r of rowsOf(g)) if (r.seller === 'SWE') { const h = heldOf(g, r.id); if (h) { h.buyer = null; delete h.round; h.until = g.turn + 50; } }
    bargainsMonth(g);
    expect(patienceOf(g, 'UK', 'SWE')).toBe(100 - BARGAINS.patience.fall + BARGAINS.patience.heal);
  });

  it('spent patience is a claim at stops 4 and 5, and only news before', () => {
    for (const stop of [3, 4]) {
      const g = world(stop);
      g.bargains!.patience['FRA:SUI'] = 0;
      const claims = feedClaims(g).filter((c) => c.on === 'SUI');
      if (stop < 4) expect(claims).toEqual([]);
      else {
        expect(claims.map((c) => c.nat)).toContain('FRA');
        expect(claims[0].ground.length).toBeGreaterThan(0);
      }
    }
  });
});

describe('a claim for patience spent', () => {
  it('lapses when patience comes back, so a later claim is warned afresh', () => {
    const g = world(4);
    for (const r of rowsOf(g)) if (r.seller === 'SUI') { const h = heldOf(g, r.id); if (h) { h.buyer = null; h.until = g.turn + 50; delete h.round; } }
    g.bargains!.patience['FRA:SUI'] = 0;
    g.flags['amb:feed:FRA:SUI'] = g.turn - 3;
    g.flags['amb:last:feed:FRA:SUI'] = g.turn - 1;
    bargainsMonth(g);
    expect(patienceOf(g, 'FRA', 'SUI')).toBe(BARGAINS.patience.heal);
    expect(g.flags['amb:feed:FRA:SUI']).toBeUndefined();
    expect(feedClaims(g).some((c) => c.on === 'SUI' && c.nat === 'FRA')).toBe(false);
  });
});

describe('a warning for a feed claim', () => {
  it('lapses when the claim leaves the list, as the dial turns under stop 4, unless it was marched for', () => {
    for (const marched of [false, true]) {
      const g = world(4);
      g.bargains!.patience['FRA:SUI'] = 0;
      expect(feedClaims(g).some((c) => c.id === 'feed:FRA:SUI')).toBe(true);
      g.flags['amb:feed:FRA:SUI'] = g.turn; g.flags['amb:last:feed:FRA:SUI'] = g.turn;
      if (marched) g.flags['amb:go:feed:FRA:SUI'] = g.turn;
      setStop(g, 3, false);
      lapseFeedClaims(g);
      expect(g.flags['amb:feed:FRA:SUI'] !== undefined).toBe(marched);
    }
  });
});

describe('the map\'s view', () => {
  it('shows a term\'s cargo, the seller\'s public lean, and patience only to those who may see it', () => {
    const g = world(1);
    bargainsMonth(g);
    const ore = (viewer: string): ReturnType<typeof bargainsView>[number] => bargainsView(g, viewer).find((v) => v.id === 'swe-ore')!;
    const v = ore('USA');
    expect(v.buyer).toBe('GER');
    expect(v.sign).toBe('ore');
    expect(v.lean).toBe(Math.round(g.nations.SWE.opinion * 100) / 100);
    if (v.state !== 'cut') {
      expect(v.cargo.moved).toBe(true);
      expect(v.cargo.sup).toBeGreaterThan(0);
      expect(v.cargo.pp).toBeNull();
      expect(ore('GER').cargo.pp).toBeGreaterThan(0);
    }
    expect(v.price).toBeNull();
    expect(ore('GER').price).toBe(heldOf(g, 'swe-ore')!.price);
    expect(ore('SWE').price).toBe(heldOf(g, 'swe-ore')!.price);
    expect(v.patience.find((p) => p.nat === 'UK')?.left).toBeNull();
    // (Whether a rival has bid at all is part of its sealed bid.)
    const h = heldOf(g, 'swe-ore')!;
    h.round = { opened: g.turn, bids: { GER: 2 } };
    expect(ore('USA').round!.bids).toEqual([]);
    expect(ore('SWE').round!.bids.map((b) => b.nat)).toEqual(['GER']);
    delete h.round;
    expect(ore('UK').patience.find((p) => p.nat === 'UK')?.left).toBe(patienceOf(g, 'UK', 'SWE'));
    expect(ore('SWE').patience.find((p) => p.nat === 'UK')?.left).toBe(patienceOf(g, 'UK', 'SWE'));
  });
});

describe('saving', () => {
  it('a save keeps the bargains', () => {
    const g = world(1);
    bargainsMonth(g);
    const back = loadGame(saveGame(g))!;
    expect(back.bargains).toEqual(g.bargains);
    expect(bargainRules(back)).toBe(true);
  });
});

describe('a war with the bargains on', () => {
  it('runs a year at stop 1 with the record\'s ore to Germany and no throw', () => {
    const g = world(1, 17);
    while (g.turn <= 14) runTurnAI(g);
    expect(heldOf(g, 'swe-ore')?.buyer).toBe('GER');
    expect(heldOf(g, 'tur-chrome')?.buyer).toBe('UK');
  });
});
