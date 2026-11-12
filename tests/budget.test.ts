// The budget of the grand campaign: every province pays into the treasury, every corps, ship and aircraft draws
// its upkeep from it, and a treasury that runs dry hurts the armies. The historical scenarios keep the plain
// arithmetic of factories, so that history's timetable holds.
import { describe, expect, it } from 'vitest';
import budget from '../data/campaign/budget.json';
import { BUDGET, PRODUCTION, aiProduction, budgeted, corpsUpkeep, income, ledger, orderCorps, provinceYield, upkeep, upkeepTotal } from '../src/campaign/economy';
import { addCorps, createGame, startPhase } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const grand = (seed = 3, human: string | null = null): Game => startOpen(createGame(seed, human));
const sum = (xs: { amount: number }[]): number => xs.reduce((s, l) => s + l.amount, 0);

describe('the budget data', () => {
  it('prices every kind of ground, every kind of corps and every class of ship', () => {
    for (const t of ['plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert']) expect((budget.land as Record<string, number>)[t], t).toBeGreaterThan(0);
    for (const k of ['inf', 'mot', 'arm', 'cav', 'mtn', 'fort']) expect((budget.upkeep.corps as Record<string, number>)[k], k).toBeGreaterThan(0);
    for (const c of ['bb', 'cv', 'ca', 'dd', 'ss']) expect((budget.upkeep.ships as Record<string, number>)[c], c).toBeGreaterThan(0);
    expect(budget.upkeep.air.ftr).toBeGreaterThan(0);
    expect(budget.upkeep.air.bmr).toBeGreaterThan(budget.upkeep.air.ftr);
    // A city is worth more than farmland, and farmland more than desert. Tanks cost more to keep than men on foot.
    expect(budget.land.urban).toBeGreaterThan(budget.land.plains);
    expect(budget.land.plains).toBeGreaterThan(budget.land.desert);
    expect(budget.upkeep.corps.arm).toBeGreaterThan(budget.upkeep.corps.inf);
    expect(budget.scale).toBe(100);
  });
});

describe('what the provinces pay', () => {
  it('in the grand campaign every province on the map pays something to whoever holds it', () => {
    const g = grand();
    expect(budgeted(g)).toBe(true);
    for (const r of g.regions) {
      const y = provinceYield(g, r);
      expect(y.land, r.name).toBeGreaterThan(0);
      expect(y.total, r.name).toBeCloseTo(y.land + y.port + y.oil + y.fac + y.capital, 9);
      expect(y.port > 0, r.name).toBe(r.port);
      expect(y.oil > 0, r.name).toBe(r.oil);
    }
    const berlin = provinceYield(g, g.byName['Berlin']);
    expect(berlin.capital).toBe(BUDGET.capital);
    expect(berlin.fac).toBe(g.byName['Berlin'].fac);
    expect(berlin.land).toBe(BUDGET.land.urban);
  });

  it('conquered ground and colonies pay half', () => {
    const g = grand();
    const warsaw = g.byName['Warsaw'];
    const own = provinceYield(g, warsaw, 'POL'), taken = provinceYield(g, warsaw, 'GER');
    expect(taken.land).toBeCloseTo(own.land * BUDGET.occupied, 9);
    expect(taken.fac).toBeCloseTo(own.fac * PRODUCTION.occupiedIncome, 9);
    expect(taken.capital).toBe(0);
    const colony = g.regions.find((r) => r.overseas && r.owner === r.ctrl)!;
    expect(provinceYield(g, colony).land).toBeCloseTo(BUDGET.land[colony.terrain] * BUDGET.overseas, 9);
  });

  it('a historical scenario counts only the factories, as it always did', () => {
    const g = createGame(3, null);
    expect(budgeted(g)).toBe(false);
    for (const r of g.regions) {
      const y = provinceYield(g, r);
      expect(y.land + y.port + y.oil + y.capital, r.name).toBe(0);
      expect(y.total, r.name).toBe(r.owner === r.ctrl ? r.fac : r.fac * PRODUCTION.occupiedIncome);
    }
    expect(upkeep(g, 'GER')).toEqual([]);
    expect(upkeepTotal(g, 'GER')).toBe(0);
    const L = ledger(g, 'GER');
    expect(L.budget).toBe(false);
    expect(L.gross).toBe(income(g, 'GER'));
    expect(L.net).toBe(L.gross);
    // The same nation on the same day earns more under the budget: its land, harbours and capital now count.
    expect(income(startOpen(createGame(3, null)), 'GER')).toBeGreaterThan(income(g, 'GER'));
  });
});

describe('the ledger', () => {
  it('adds up: the lines make the income, and income less upkeep is what is left', () => {
    const g = grand();
    for (let k = 0; k < 14; k++) runTurnAI(g);
    for (const n of Object.values(g.nations)) {
      if (!n.alive || n.dormant) continue;
      const L = ledger(g, n.id);
      expect(L.gross, n.id).toBeCloseTo(income(g, n.id), 9);
      expect(sum(L.income) + sum(L.changes), n.id).toBeCloseTo(L.gross, 6);
      expect(L.upkeepTotal, n.id).toBeCloseTo(sum(L.upkeep), 9);
      expect(L.upkeepTotal, n.id).toBeCloseTo(upkeepTotal(g, n.id), 9);
      expect(L.net, n.id).toBeCloseTo(L.gross - L.upkeepTotal, 9);
      expect(L.treasury, n.id).toBe(n.pp);
      expect(L.scale).toBe(BUDGET.scale);
      // Every province held is listed, richest first.
      expect(L.provinces.length, n.id).toBe(g.regions.filter((r) => r.ctrl === n.id).length);
      for (let i = 1; i < L.provinces.length; i++) expect(L.provinces[i - 1].total, n.id).toBeGreaterThanOrEqual(L.provinces[i].total);
    }
  });

  it('shows where Germany\'s money comes from in September 1939', () => {
    const L = ledger(grand(), 'GER');
    const line = (key: string): number => L.income.find((l) => l.key === key)?.amount ?? 0;
    expect(line('capital')).toBe(BUDGET.capital);
    expect(line('factories')).toBeGreaterThan(30);
    expect(line('land')).toBeGreaterThan(3);
    expect(line('ports')).toBeGreaterThan(0);
    // The forces take a real share of the income, but far from all of it.
    expect(L.upkeepTotal / L.gross).toBeGreaterThan(0.08);
    expect(L.upkeepTotal / L.gross).toBeLessThan(0.35);
    expect(L.debt).toBe(0);
  });

  it('counts a capital only while it is held', () => {
    const g = grand();
    const before = income(g, 'POL');
    g.byName['Warsaw'].ctrl = 'GER';
    const L = ledger(g, 'POL');
    expect(L.income.find((l) => l.key === 'capital')).toBeUndefined();
    expect(before - L.gross).toBeGreaterThanOrEqual(BUDGET.capital);
  });
});

describe('upkeep', () => {
  it('charges every corps by its kind, its size and the men it still has', () => {
    const g = grand();
    const before = upkeepTotal(g, 'GER');
    const c = addCorps(g, 'GER', 'Test Panzer Corps', 'arm', 2, g.nations.GER.capital, {});
    expect(upkeepTotal(g, 'GER') - before).toBeCloseTo(corpsUpkeep('arm', 2), 9);
    expect(corpsUpkeep('arm', 2)).toBeCloseTo(2 * BUDGET.upkeep.corps.arm, 9);
    c.str = 0.5;
    expect(upkeepTotal(g, 'GER') - before).toBeCloseTo(corpsUpkeep('arm', 2) / 2, 9);
  });

  it('charges fleets by the hull and air fleets by the machine', () => {
    const g = grand();
    const lines = upkeep(g, 'UK');
    const get = (key: string): number => lines.find((l) => l.key === key)?.amount ?? 0;
    const hulls = (cls: 'bb' | 'dd'): number => g.fleets.filter((f) => f.nat === 'UK').reduce((s, f) => s + f.ships[cls], 0);
    expect(get('ships:bb')).toBeCloseTo(hulls('bb') * BUDGET.upkeep.ships.bb, 9);
    expect(get('ships:dd')).toBeCloseTo(hulls('dd') * BUDGET.upkeep.ships.dd, 9);
    const ftr = g.airFleets.filter((f) => f.nat === 'UK').reduce((s, f) => s + f.ftr, 0);
    expect(get('air:ftr')).toBeCloseTo(ftr * BUDGET.upkeep.air.ftr, 9);
    // The Royal Navy costs Britain more than its small army does.
    const of = (p: string): number => lines.filter((l) => l.key.startsWith(p)).reduce((s, l) => s + l.amount, 0);
    expect(of('ships:')).toBeGreaterThan(of('corps:'));
  });

  it('is halved for a nation at peace', () => {
    const g = grand();
    const peace = upkeepTotal(g, 'ITA');
    g.nations.ITA.atWar.push('FRA');
    expect(upkeepTotal(g, 'ITA')).toBeCloseTo(peace / BUDGET.upkeep.peace, 9);
  });

  it('comes out of the month\'s income before anything else is paid for', () => {
    const g = grand();
    const n = g.nations.SWE;
    // A neutral with a full-strength army asks for no replacements: the month adds exactly income less upkeep.
    for (const c of g.corps) if (c.nat === 'SWE') c.str = 1;
    n.pp = 10;
    const net = income(g, 'SWE') - upkeepTotal(g, 'SWE');
    startPhase(g, 'SWE');
    expect(n.pp).toBeCloseTo(10 + net, 6);
  });

  it('leaves no nation in the red in September 1939', () => {
    const g = grand();
    for (const n of Object.values(g.nations)) {
      if (!n.alive || n.dormant) continue;
      expect(ledger(g, n.id).net, n.id).toBeGreaterThan(0);
    }
  });
});

describe('debt', () => {
  /** Germany deep in debt at the start of a month, with every corps worn down a little. */
  function broke(owe: number): Game {
    const g = grand(5);
    for (const c of g.corps) if (c.nat === 'GER') { c.str = 0.8; c.org = 0.5; c.fought = false; }
    g.nations.GER.pp = -owe;
    return g;
  }

  it('stops replacements, spoils the armies\' rest and lets men drift away', () => {
    const rich = broke(0), poor = broke(1000);
    rich.nations.GER.pp = 500;
    startPhase(rich, 'GER');
    startPhase(poor, 'GER');
    const a = rich.corps.filter((c) => c.nat === 'GER' && c.supplied), b = poor.corps.filter((c) => c.nat === 'GER' && c.supplied);
    expect(a.length).toBe(b.length);
    expect(a.length).toBeGreaterThan(20);
    for (let i = 0; i < a.length; i++) {
      expect(a[i].str, a[i].name).toBeGreaterThan(0.8);
      expect(b[i].str, b[i].name).toBeLessThan(0.8);
      expect(b[i].str, b[i].name).toBeGreaterThanOrEqual(0.8 * (1 - BUDGET.debt.desert) - 1e-9);
      expect(b[i].org, b[i].name).toBeLessThan(a[i].org);
      expect(b[i].org, b[i].name).toBeGreaterThan(0.5);
    }
  });

  it('has a floor: creditors stop lending at a few months of income', () => {
    const g = broke(100000);
    startPhase(g, 'GER');
    const floor = BUDGET.debt.floor * income(g, 'GER');
    expect(g.nations.GER.pp).toBeLessThan(0);
    expect(g.nations.GER.pp).toBeGreaterThanOrEqual(-floor * 1.3);
    expect(ledger(g, 'GER').debt).toBeGreaterThan(0.7);
  });

  it('refuses new orders until the books balance', () => {
    const g = broke(5);
    expect(orderCorps(g, 'GER', 'inf', g.nations.GER.capital)).toBeNull();
  });

  it('is news for the player when it begins and when it ends', () => {
    const g = startOpen(createGame(5, 'GER'));
    const n = g.nations.GER;
    n.pp = -500;
    startPhase(g, 'GER');
    expect(g.flags['debt:GER']).toBe(g.turn);
    expect(g.headlines.some((h) => /treasury is empty/.test(h.text))).toBe(true);
    n.pp = 500;
    startPhase(g, 'GER');
    expect(g.flags['debt:GER']).toBeUndefined();
    expect(g.headlines.some((h) => /books balance again/.test(h.text))).toBe(true);
  });

  it('is not a thing in a historical scenario: the treasury never goes below nothing', () => {
    const g = createGame(5, null);
    g.nations.GER.pp = 0;
    for (let k = 0; k < 6; k++) runTurnAI(g);
    for (const n of Object.values(g.nations)) expect(n.pp, n.id).toBeGreaterThanOrEqual(0);
    expect(Object.keys(g.flags).some((k) => k.startsWith('debt:'))).toBe(false);
  });

  it('survives a save', () => {
    const g = grand(5);
    g.nations.ITA.pp = -7.5;
    const again = loadGame(saveGame(g))!;
    expect(again.nations.ITA.pp).toBe(-7.5);
    expect(ledger(again, 'ITA').debt).toBeGreaterThan(0);
  });
});

describe('the computer\'s nations', () => {
  it('order nothing they could not keep in the field', () => {
    const g = grand(4);
    while (g.turn < 6) runTurnAI(g);
    const n = g.nations.FRA;
    n.queue = [];
    n.pp = 500;
    // With money in the bank but no monthly surplus to feed another corps, the staff waits.
    const spare = income(g, 'FRA') - upkeepTotal(g, 'FRA');
    for (let k = 0; spare - k * corpsUpkeep('arm', 4) > 0; k++) addCorps(g, 'FRA', `Paper Corps ${k}`, 'arm', 4, n.capital, {});
    expect(income(g, 'FRA') - upkeepTotal(g, 'FRA')).toBeLessThan(corpsUpkeep('inf', PRODUCTION.builds.inf.divs));
    aiProduction(g, 'FRA');
    expect(n.queue.length).toBe(0);
  });

  it('stay out of debt through a whole war', () => {
    const g = grand(2);
    let worst = 0;
    while (g.turn <= 40) {
      runTurnAI(g);
      for (const n of Object.values(g.nations)) if (n.alive && !n.dormant) worst = Math.min(worst, n.pp);
    }
    expect(worst).toBeGreaterThanOrEqual(0);
  });
});
