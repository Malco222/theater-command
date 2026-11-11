// Diplomacy: the alignment track, courting, and history's events as data.
import { describe, expect, it } from 'vitest';
import { allied, createGame } from '../src/campaign/game';
import { DIPLOMACY, canCourt, court, holds, momentum, monthlyDiplomacy, receptive, step, tradePartners } from '../src/campaign/diplomacy';
import { income } from '../src/campaign/economy';
import { beginHuman, humanUp, orderCourt, passMonth, runTurnAI, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

function history(seed: number, turns: number): Game {
  const g = createGame(seed, null);
  for (let t = 1; t <= turns; t++) runTurnAI(g);
  return g;
}

function asPlayer(nat: string, seed = 5): Game {
  const g = createGame(seed, nat);
  let guard = 0;
  while (!humanUp(g) && guard++ < 60) stepAI(g);
  beginHuman(g);
  return g;
}

describe('the alignment track in September 1939', () => {
  const g = createGame(1, null);
  it('places every nation where the framework says it stood', () => {
    expect(step(g.nations.UK)).toBe(-3);
    expect(step(g.nations.TUR)).toBe(-1);
    expect(step(g.nations.SOV)).toBe(0);
    expect(step(g.nations.SWE)).toBe(0);
    expect(step(g.nations.HUN)).toBe(1);
    expect(step(g.nations.ESP)).toBe(1);
    expect(step(g.nations.SVK)).toBe(2);
    expect(step(g.nations.GER)).toBe(3);
  });
  it('nobody is winning yet', () => {
    expect(Math.abs(momentum(g))).toBeLessThan(0.2);
  });
  it('neutrals that lean toward a bloc trade with its major powers', () => {
    expect(tradePartners(g, 'GER').map((n) => n.id).sort()).toEqual(['ESP', 'HUN']);
    expect(tradePartners(g, 'UK').map((n) => n.id).sort()).toEqual(['IRQ', 'TUR']);
    expect(tradePartners(g, 'SVK')).toEqual([]);
    const before = income(g, 'GER');
    g.nations.ESP.align = 'independent';
    expect(income(g, 'GER')).toBeCloseTo(before - DIPLOMACY.tradeIncome, 5);
    g.nations.ESP.align = 'leaning_axis';
  });
});

describe('history as events, on the historical plan', () => {
  const runs = [2, 9].map((s) => history(s, 20));
  it('the Soviet Union takes the Baltic states and Bessarabia in the summer of 1940', () => {
    for (const g of runs) {
      for (const id of ['EST', 'LAT', 'LIT']) expect(g.nations[id].alive).toBe(false);
      for (const name of ['Estonia', 'Latvia', 'Lithuania', 'Bessarabia']) expect(g.byName[name].ctrl).toBe('SOV');
      expect(g.flags['event:baltic']).toBeLessThanOrEqual(12);
      expect(g.corps.some((c) => c.nat === 'ROM' && g.regions[c.reg].ctrl !== 'ROM')).toBe(false);
    }
  });
  it('the Danube states join the Axis after France falls, and Bulgaria follows in 1941', () => {
    for (const g of runs) {
      expect(g.byName['Transylvania'].owner).toBe('HUN');
      expect(g.nations.HUN.align).toBe('axis');
      expect(g.nations.ROM.align).toBe('axis');
      expect(g.nations.BUL.align).toBe('axis');
      expect(g.flags['event:tripartite']).toBeGreaterThanOrEqual(15);
      expect(allied(g, 'GER', 'ROM')).toBe(true);
      expect(g.nations.ROM.atWar.length).toBe(0);
    }
  });
  it('Spain, Sweden, Switzerland, Turkey and the Soviet Union stay where they were', () => {
    for (const g of runs) {
      expect(step(g.nations.ESP)).toBe(1);
      expect(step(g.nations.SWE)).toBe(0);
      expect(step(g.nations.SUI)).toBe(0);
      expect(step(g.nations.TUR)).toBe(-1);
      expect(step(g.nations.SOV)).toBe(0);
    }
  });
});

describe('events only fire if their conditions still hold', () => {
  it('no Tripartite Pact while France still stands', () => {
    const g = createGame(4, null);
    g.turn = 15;
    monthlyDiplomacy(g);
    expect(g.flags['event:tripartite']).toBeUndefined();
    expect(g.nations.HUN.align).toBe('leaning_axis');
  });
  it('no Soviet ultimatum to a Romania that is already under German protection', () => {
    const g = createGame(4, null);
    g.nations.POL.alive = false;
    g.nations.ROM.align = 'axis';
    g.turn = 10;
    monthlyDiplomacy(g);
    expect(g.flags['event:bessarabia']).toBeUndefined();
    expect(g.byName['Bessarabia'].ctrl).toBe('ROM');
    expect(g.flags['event:baltic']).toBe(10);
  });
  it('a condition nobody has defined never holds', () => {
    const g = createGame(4, null);
    expect(holds(g, 'alive:GER')).toBe(true);
    expect(holds(g, 'alvie:GER')).toBe(false);
    expect(holds(g, 'momentum>=:0.5')).toBe(false);
  });
});

describe('courting neutrals', () => {
  it('costs political capital, moves opinion, and can be done once a month per country', () => {
    const g = asPlayer('GER');
    const ger = g.nations.GER, esp = g.nations.ESP;
    ger.pc = 20;
    const was = esp.opinion;
    const k = receptive(esp, 'axis');
    expect(orderCourt(g, 'ESP')).toBe(true);
    expect(ger.pc).toBe(20 - DIPLOMACY.court.cost);
    expect(esp.opinion).toBeCloseTo(was + DIPLOMACY.court.opinion * k, 5);
    expect(orderCourt(g, 'ESP')).toBe(false);
    expect(canCourt(g, 'GER', 'ESP').why).toMatch(/this month/);
    expect(orderCourt(g, 'SWE')).toBe(true);
    ger.pc = 0;
    expect(orderCourt(g, 'TUR')).toBe(false);
  });
  it('moves that match public opinion go further than moves against it', () => {
    const g = createGame(1, null);
    expect(receptive(g.nations.HUN, 'axis')).toBeGreaterThan(receptive(g.nations.SWE, 'axis'));
    expect(receptive(g.nations.SWE, 'axis')).toBeGreaterThan(receptive(g.nations.TUR, 'axis'));
    expect(receptive(g.nations.SOV, 'axis')).toBeLessThan(0.5);
  });
  it('a nation at war or already in a bloc cannot be courted', () => {
    const g = asPlayer('GER');
    g.nations.GER.pc = 30;
    expect(court(g, 'GER', 'ITA')).toBe(false);
    expect(court(g, 'GER', 'POL')).toBe(false);
    expect(court(g, 'SWE', 'NOR')).toBe(false);
  });
  it('sustained courting can bring Spain into the Axis, which history never managed', () => {
    const g = asPlayer('GER');
    let joined = 0;
    for (let m = 0; m < 14 && !joined; m++) {
      g.nations.GER.pc = 30;
      orderCourt(g, 'ESP');
      if (!passMonth(g)) break;
      if (g.nations.ESP.align === 'axis') joined = g.turn;
    }
    expect(joined).toBeGreaterThan(0);
    expect(allied(g, 'GER', 'ESP')).toBe(true);
    expect(g.headlines.some((h) => h.text === 'Spain joins the Axis.')).toBe(true);
  });
  it('political capital comes in every month and is capped', () => {
    const g = asPlayer('UK');
    const start = g.nations.UK.pc;
    passMonth(g);
    expect(g.nations.UK.pc).toBe(start + DIPLOMACY.pcIncome._major);
    for (let i = 0; i < 14; i++) passMonth(g);
    expect(g.nations.UK.pc).toBeLessThanOrEqual(DIPLOMACY.pcCap);
  });
});
