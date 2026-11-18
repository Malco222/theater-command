// The computer staffs' programme at sea and in the air (rulings 68(b) and 70(a); the grand campaign, from stop 3 of
// the history dial). A computer staff orders ships and wings from the arsenal as a player does, by its war and its
// character: destroyers when its own routes are bled, fighters first while it is bombed, submarines against an enemy
// who lives by sea, a carrier for a carrier power, and wings up to its share. It pays with a share of its income set
// aside and with idle money, never with the money its army is saving for a corps.
import { beforeAll, describe, expect, it } from 'vitest';
import arsenal from '../data/campaign/arsenal.json';
import { airFleetsOf } from '../src/campaign/air';
import { aiProgramme, programmePurse, programmeWants } from '../src/campaign/arsenal';
import { PRODUCTION, corpsCost, income } from '../src/campaign/economy';
import { createGame } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { fleetsOf, shipCount } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Build, Game } from '../src/campaign/types';

const P = arsenal.ai.programme;
const grand = (human: string | null = null, seed = 3): Game => startOpen(createGame(seed, human));
const orders = (g: Game, nat: string, what: 'ship' | 'air'): Build[] => g.nations[nat].queue.filter((b) => b.what === what);
/** Says the Atlantic convoys lost this share of their cargo last month. */
const bleed = (g: Game, loss: number): void => { g.convoys.atlantic = { turn: g.turn, raid: 10, guard: 10, tons: 100000, total: 100000, share: 0.1, loss, subsLost: 0, escortsLost: 0 }; };

describe('the data', () => {
  it('says what a computer staff orders, from which stop of the dial, and what it pays with', () => {
    expect(P.fromStop).toBe(3);
    expect(P.bled).toBeGreaterThan(0);
    expect(P.share).toBeGreaterThan(0);
    expect(P.rate.ship).toBeGreaterThan(0);
    expect(P.rate.ship + P.rate.air).toBeLessThan(0.25);
    for (const k of ['dd', 'ss', 'cv', 'ftr', 'bmr'] as const) expect(P.each[k]).toBeGreaterThanOrEqual(1);
    expect(arsenal.ai._programme.length).toBeGreaterThan(200);
  });
});

describe('what a computer staff wants', () => {
  it('is submarines against an enemy who lives by sea, for a navy that raids', () => {
    const g = grand();
    const w = programmeWants(g, 'GER');
    expect(w).toContain('ship:ss');
    // Nothing feeds Germany by convoy, and it has no carriers.
    expect(w).not.toContain('ship:dd');
    expect(w).not.toContain('ship:cv');
    // A navy with no submarines out raiding has nothing to send them to.
    expect(programmeWants(g, 'UK')).not.toContain('ship:ss');
  });

  it('is destroyers when its own routes are bled, and a carrier for a carrier power', () => {
    const g = grand();
    expect(fleetsOf(g, 'UK').some((f) => f.ships.cv > 0)).toBe(true);
    bleed(g, P.bled / 2);
    expect(programmeWants(g, 'UK')).not.toContain('ship:dd');
    bleed(g, P.bled * 2);
    const w = programmeWants(g, 'UK');
    expect(w[0]).toBe('ship:dd');
    expect(w).toContain('ship:cv');
    // Last year's losses are last year's.
    g.convoys.atlantic.turn = g.turn - 3;
    expect(programmeWants(g, 'UK')).not.toContain('ship:dd');
  });

  it('is fighters first while its factories are being bombed, and otherwise the kind it has bought least of', () => {
    const g = grand();
    expect(programmeWants(g, 'UK').filter((k) => k.startsWith('air:'))).toEqual(['air:ftr', 'air:bmr']);
    // A wing of fighters bought: bombers are next.
    g.nations.UK.queue.push({ id: g.nextBuild++, what: 'air', item: 'ftr', name: '', region: g.nations.UK.capital, ordered: g.turn, eta: g.turn + 3, cost: 12 } as Build);
    expect(programmeWants(g, 'UK').filter((k) => k.startsWith('air:'))).toEqual(['air:bmr']);
    g.nations.UK.queue.pop();
    const f = airFleetsOf(g, 'UK').find((x) => x.kind === 'ftr' && x.ship === undefined)!;
    f.est.ftr += 1; f.extra = { ftr: 1, bmr: 0 };
    expect(programmeWants(g, 'UK').filter((k) => k.startsWith('air:'))).toEqual(['air:bmr', 'air:ftr']);
    g.raids.UK = { turn: g.turn, share: 0.05 };
    expect(programmeWants(g, 'UK').filter((k) => k.startsWith('air:'))).toEqual(['air:ftr', 'air:bmr']);
  });

  it('is never more of a kind than it may have on order at a time, nor aircraft beyond its share', () => {
    const g = grand();
    const n = g.nations.GER;
    for (let k = 0; k < P.each.ss; k++) n.queue.push({ id: g.nextBuild++, what: 'ship', item: 'ss', name: '', region: n.capital, ordered: g.turn, eta: g.turn + 7, cost: 8 } as Build);
    expect(programmeWants(g, 'GER')).not.toContain('ship:ss');
    const est = airFleetsOf(g, 'GER').filter((x) => x.ship === undefined).reduce((s, x) => s + x.est.bmr, 0);
    n.queue.push({ id: g.nextBuild++, what: 'air', item: 'bmr', count: Math.ceil(est * P.share), name: '', region: n.capital, ordered: g.turn, eta: g.turn + 4, cost: 18 } as Build);
    expect(programmeWants(g, 'GER')).not.toContain('air:bmr');
  });

  it('is nothing at peace', () => {
    const g = grand();
    expect(g.nations.SOV.atWar).toEqual([]);
    expect(programmeWants(g, 'SOV')).toEqual([]);
  });
});

describe('what it pays with', () => {
  it('is a share of the month\'s income set aside for its navy and as much for its air force, and spent on nothing else', () => {
    const g = grand();
    const n = g.nations.GER;
    // A treasury with no idle money in it: under the price of a corps of infantry and the reserve.
    n.pp = PRODUCTION.aiReserve + corpsCost(g, 'GER', 'inf') - 1;
    const had = n.pp, inc = income(g, 'GER');
    aiProgramme(g, 'GER');
    const purse = programmePurse(g, 'GER');
    expect(purse.ship).toBeCloseTo(Math.min(8, P.rate.ship * inc), 1);
    expect(purse.air).toBeCloseTo(Math.min(18, P.rate.air * inc), 1);
    expect(n.pp).toBeCloseTo(had - purse.ship - purse.air, 1);
    expect(orders(g, 'GER', 'ship').length + orders(g, 'GER', 'air').length).toBe(0);
    // Once it holds the price, the order is placed with it and the treasury is no poorer for the order.
    g.flags['pg:GER:ship'] = 8;
    const before = n.pp;
    aiProgramme(g, 'GER');
    expect(orders(g, 'GER', 'ship').map((b) => b.item)).toEqual(['ss']);
    expect(programmePurse(g, 'GER').ship).toBeLessThan(8);
    expect(before - n.pp).toBeLessThanOrEqual((P.rate.ship + P.rate.air) * inc + 0.01);
    expect(n.pp).toBeGreaterThanOrEqual(PRODUCTION.aiReserve - 0.01);
  });

  it('never takes the treasury below the staff\'s reserve, and holds no more than the dearest thing it wants costs', () => {
    const g = grand();
    const n = g.nations.GER;
    n.pp = PRODUCTION.aiReserve;
    aiProgramme(g, 'GER');
    expect(programmePurse(g, 'GER')).toEqual({ ship: 0, air: 0 });
    expect(n.pp).toBe(PRODUCTION.aiReserve);
    g.flags['pg:GER:air'] = 17.5;
    n.pp = PRODUCTION.aiReserve + 10;
    aiProgramme(g, 'GER');
    // (Fighters are bought from it at once; what is left is topped up no higher than a bomber wing's price.)
    expect(programmePurse(g, 'GER').air).toBeLessThanOrEqual(18);
  });

  it('is idle money besides: one order a month for the navy and one for the air force', () => {
    const g = grand();
    const n = g.nations.GER;
    n.pp = 500;
    aiProgramme(g, 'GER');
    expect(orders(g, 'GER', 'ship').length).toBe(1);
    expect(orders(g, 'GER', 'air').length).toBe(1);
    expect(n.pp).toBeLessThan(500);
    aiProgramme(g, 'GER');
    // The second month's order is of another kind: never two of one on order together.
    const air = orders(g, 'GER', 'air').map((b) => b.item);
    expect(new Set(air).size).toBe(air.length);
    expect(orders(g, 'GER', 'ship').length).toBe(1);
  });
});

describe('who has no programme', () => {
  it('the player, a nation at peace, the lower stops of the history dial, and a history campaign', () => {
    const h = grand('GER');
    h.nations.GER.pp = 500;
    aiProgramme(h, 'GER');
    expect(orders(h, 'GER', 'ship').length + orders(h, 'GER', 'air').length).toBe(0);
    expect(h.nations.GER.pp).toBe(500);

    const g = grand();
    g.nations.SOV.pp = 500;
    aiProgramme(g, 'SOV');
    expect(g.nations.SOV.queue.filter((b) => b.what === 'ship' || b.what === 'air')).toEqual([]);

    for (const stop of [1, 2]) {
      const low = grand();
      expect(setStop(low, stop, true)).toBe(true);
      low.nations.GER.pp = 500;
      aiProgramme(low, 'GER');
      expect(orders(low, 'GER', 'ship').length + orders(low, 'GER', 'air').length, `stop ${stop}`).toBe(0);
      expect(low.nations.GER.pp).toBe(500);
    }

    const s = createGame(1, null);
    s.nations.GER.pp = 500;
    aiProgramme(s, 'GER');
    expect(s.nations.GER.queue.filter((b) => b.what === 'ship' || b.what === 'air')).toEqual([]);
  });
});

describe('a computer-run grand campaign', () => {
  let laid: Record<string, Record<string, number>> = {};
  let end: Game;
  beforeAll(() => {
    const g = grand(null, 7);
    laid = {};
    while (g.turn <= 30) {
      const t = g.turn;
      runTurnAI(g);
      for (const n of Object.values(g.nations)) for (const b of n.queue) if (b.ordered === t && (b.what === 'ship' || b.what === 'air')) { const L = (laid[n.id] ??= {}); const k = `${b.what}:${b.item}`; L[k] = (L[k] ?? 0) + 1; }
    }
    end = g;
  }, 180000);

  it('sees ships laid down and wings ordered by the great powers at war, each by its war', () => {
    const all = Object.values(laid).reduce((s, L) => s + Object.values(L).reduce((a, b) => a + b, 0), 0);
    expect(all).toBeGreaterThan(8);
    expect(laid.GER?.['ship:ss'] ?? 0).toBeGreaterThan(0);
    expect((laid.GER?.['air:ftr'] ?? 0) + (laid.GER?.['air:bmr'] ?? 0)).toBeGreaterThan(0);
    expect(laid.UK?.['ship:dd'] ?? 0).toBeGreaterThan(0);
    expect(laid.GER?.['ship:cv'] ?? 0).toBe(0);
  });

  it('keeps every navy and treasury sound', () => {
    for (const n of Object.values(end.nations)) {
      if (!n.alive) continue;
      expect(Number.isFinite(n.pp), n.id).toBe(true);
      const purse = programmePurse(end, n.id);
      expect(purse.ship).toBeGreaterThanOrEqual(0);
      expect(purse.ship).toBeLessThanOrEqual(40);
      expect(purse.air).toBeLessThanOrEqual(18);
      for (const f of fleetsOf(end, n.id)) expect(shipCount(f.ships)).toBeGreaterThanOrEqual(0);
    }
  });
});
