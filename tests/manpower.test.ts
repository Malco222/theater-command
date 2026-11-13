// Manpower, and the computer staff's guns (the grand campaign). Every nation has a reserve of men fit to bear arms,
// counted in divisions' worth. Replacements and every new corps are drawn from it, a class comes of age each month
// and joins it, and a nation whose reserve is spent makes good only what the class allows and raises no new corps.
// The same rule holds for the player and for the computer. A staff whose line has met dug-in troops orders massed
// guns. The historical scenarios have none of it. The numbers are in data/campaign/manpower.json and arsenal.json.
import { describe, expect, it } from 'vitest';
import arsenal from '../data/campaign/arsenal.json';
import { offers } from '../src/campaign/arsenal';
import { aiProduction, cancelBuild, corpsCost, orderCorps, raiseSite, specOf } from '../src/campaign/economy';
import { armySize, createGame, startPhase } from '../src/campaign/game';
import { decree, govOf } from '../src/campaign/government';
import {
  MANPOWER, classArrives, classFull, classNow, drawMen, homeShare, manpower, menLeft, menLevel, menRules, menSpare, menWarning, menWords, openManpower, poolAtStart, raisable, returnMen,
} from '../src/campaign/manpower';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, manpowerOf, runTurnAI, stepAI } from '../src/campaign/turn';
import { MEN_PER_DIV, type Game } from '../src/campaign/types';

const free = (seed: number, human: string | null = null): Game => startOpen(createGame(seed, human));
function playing(human = 'GER', seed = 3): Game {
  const g = free(seed, human);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const set = (g: Game, nat: string, divs: number): void => { g.flags[`mp:${nat}`] = divs; };
const strength = (g: Game, nat: string): number => g.corps.reduce((s, c) => s + (c.nat === nat && !c.dead ? c.divs * c.str : 0), 0);

describe('the reserve of men is for the grand campaign', () => {
  it('the historical scenarios count no men', () => {
    const g = createGame(1, null);
    expect(menRules(g)).toBe(false);
    expect(menLeft(g, 'GER')).toBe(Infinity);
    expect(raisable(g, 'GER', 3)).toBe(Infinity);
    expect(menSpare(g, 'GER')).toBe(Infinity);
    expect(manpower(g, 'GER')).toBeNull();
    expect(drawMen(g, 'GER', 5)).toBe(5);
    expect(classArrives(g, 'GER')).toBe(0);
    for (let k = 0; k < 3; k++) runTurnAI(g);
    expect(Object.keys(g.flags).filter((k) => k.startsWith('mp'))).toEqual([]);
  });

  it('a grand campaign opens with every reserve full: a great power by the table, any other country by the army it keeps', () => {
    const g = free(1);
    expect(menRules(g)).toBe(true);
    for (const nat of Object.keys(MANPOWER.pool)) {
      if (!g.nations[nat]) continue;
      expect(menLeft(g, nat), nat).toBe(MANPOWER.pool[nat]);
      expect(poolAtStart(g, nat), nat).toBe(MANPOWER.pool[nat]);
      expect(classFull(g, nat), nat).toBe(MANPOWER.class[nat]);
    }
    const divs = g.corps.reduce((s, c) => s + (c.nat === 'POL' && !c.dead && c.kind !== 'fort' ? c.divs : 0), 0);
    expect(divs).toBeGreaterThan(0);
    expect(menLeft(g, 'POL')).toBeCloseTo(MANPOWER.minor.pool + MANPOWER.minor.perDiv * divs, 9);
    expect(classFull(g, 'POL')).toBeCloseTo(menLeft(g, 'POL') * MANPOWER.minor.classShare, 9);
    // The Soviet Union has by far the deepest reserve, and Germany's is a third of it.
    expect(MANPOWER.pool.SOV).toBeGreaterThan(MANPOWER.pool.GER * 3);
    expect(MANPOWER.class.SOV).toBeGreaterThan(MANPOWER.class.GER * 2);
  });

  it('a campaign that opens later finds part of each warring nation\'s reserve spent', () => {
    const g = free(1);
    g.turn = 25;
    g.nations.GER.atWar = ['UK'];
    openManpower(g);
    expect(menLeft(g, 'GER')).toBeCloseTo(MANPOWER.pool.GER * (1 - MANPOWER.spentAYear * 2), 9);
    expect(poolAtStart(g, 'GER')).toBe(MANPOWER.pool.GER);
    g.turn = 12 * 9 + 1;
    openManpower(g);
    expect(menLeft(g, 'GER')).toBeCloseTo(MANPOWER.pool.GER * (1 - MANPOWER.spentMost), 9);
  });

  it('a save keeps the reserve and what has been drawn from it', () => {
    const g = free(2);
    drawMen(g, 'GER', 12.5);
    const back = loadGame(saveGame(g))!;
    expect(menLeft(back, 'GER')).toBe(MANPOWER.pool.GER - 12.5);
    expect(manpower(back, 'GER')!.drawn).toBe(12.5 * MEN_PER_DIV);
    expect(poolAtStart(back, 'GER')).toBe(MANPOWER.pool.GER);
  });
});

describe('drawing on the reserve', () => {
  it('men drawn leave it and are counted, and no more can be taken than it holds', () => {
    const g = free(1);
    expect(drawMen(g, 'GER', 10)).toBe(10);
    expect(menLeft(g, 'GER')).toBe(MANPOWER.pool.GER - 10);
    expect(drawMen(g, 'GER', 1000)).toBe(MANPOWER.pool.GER - 10);
    expect(menLeft(g, 'GER')).toBe(0);
    expect(drawMen(g, 'GER', 3)).toBe(0);
    expect(manpower(g, 'GER')!.drawn).toBe(MANPOWER.pool.GER * MEN_PER_DIV);
    returnMen(g, 'GER', 3);
    expect(menLeft(g, 'GER')).toBe(3);
    expect(raisable(g, 'GER', 3)).toBe(1);
    expect(raisable(g, 'GER', 2)).toBe(1);
    expect(raisable(g, 'GER', 4)).toBe(0);
  });

  it('the month\'s class joins it, and never fills it past what it opened with', () => {
    const g = free(1);
    expect(classArrives(g, 'GER')).toBe(0);
    set(g, 'GER', 50);
    expect(classArrives(g, 'GER')).toBeCloseTo(MANPOWER.class.GER, 9);
    expect(menLeft(g, 'GER')).toBeCloseTo(50 + MANPOWER.class.GER, 9);
    set(g, 'GER', MANPOWER.pool.GER - 1);
    expect(classArrives(g, 'GER')).toBeCloseTo(1, 9);
    expect(menLeft(g, 'GER')).toBe(MANPOWER.pool.GER * MANPOWER.most);
  });

  it('the class shrinks with the homeland lost, by the worth of the provinces, but never to nothing', () => {
    const g = free(1);
    expect(homeShare(g, 'GER')).toBe(1);
    const full = classNow(g, 'GER');
    const ruhr = g.regions.filter((r) => r.owner === 'GER' && !r.overseas).sort((a, b) => b.fac + b.vp - (a.fac + a.vp))[0];
    ruhr.ctrl = 'FRA';
    const share = homeShare(g, 'GER');
    expect(share).toBeLessThan(1);
    expect(classNow(g, 'GER')).toBeCloseTo(full * share, 9);
    // A rich province counts for more than a poor one.
    ruhr.ctrl = 'GER';
    const poor = g.regions.filter((r) => r.owner === 'GER' && !r.overseas).sort((a, b) => a.fac + a.vp - (b.fac + b.vp))[0];
    poor.ctrl = 'FRA';
    expect(homeShare(g, 'GER')).toBeGreaterThan(share);
    for (const r of g.regions) if (r.owner === 'GER') r.ctrl = 'FRA';
    expect(homeShare(g, 'GER')).toBe(0);
    expect(classNow(g, 'GER')).toBeCloseTo(full * MANPOWER.classFloor, 9);
  });

  it('replacements come out of the reserve, and with it spent only the month\'s class reaches the armies', () => {
    const g = free(3);
    for (const c of g.corps) if (c.nat === 'GER' && !c.dead) c.str = 0.5;
    g.nations.GER.pp = 5000;
    const before = strength(g, 'GER'), had = menLeft(g, 'GER');
    startPhase(g, 'GER');
    const made = strength(g, 'GER') - before;
    expect(made).toBeGreaterThan(1);
    expect(menLeft(g, 'GER')).toBeCloseTo(had - made, 6);
    // Now with nothing in reserve, and a class smaller than the depots could send out: the class is all there is.
    const full = MANPOWER.class.GER;
    try {
      MANPOWER.class.GER = made / 4;
      for (const c of g.corps) if (c.nat === 'GER' && !c.dead) c.str = 0.5;
      set(g, 'GER', 0);
      const low = strength(g, 'GER');
      startPhase(g, 'GER');
      expect(strength(g, 'GER') - low).toBeCloseTo(made / 4, 6);
      expect(menLeft(g, 'GER')).toBeCloseTo(0, 6);
    } finally { MANPOWER.class.GER = full; }
  });

  it('a new corps takes its men the day it is ordered, and gives them back if the order is taken back that month', () => {
    const g = free(3);
    g.nations.GER.pp = 5000;
    const inf = specOf(g, 'inf')!;
    const at = raiseSite(g, 'GER', 'inf', g.nations.GER.capital);
    const had = menLeft(g, 'GER');
    const b = orderCorps(g, 'GER', 'inf', at)!;
    expect(b).not.toBeNull();
    expect(menLeft(g, 'GER')).toBe(had - inf.divs);
    expect(cancelBuild(g, 'GER', b.id)).toBe(true);
    expect(menLeft(g, 'GER')).toBe(had);
    // With fewer men than the corps takes, it is not raised, and the money stays in the treasury.
    set(g, 'GER', inf.divs - 0.5);
    const pp = g.nations.GER.pp;
    expect(orderCorps(g, 'GER', 'inf', at)).toBeNull();
    expect(g.nations.GER.pp).toBe(pp);
    expect(g.nations.GER.pp).toBeGreaterThan(corpsCost(g, 'GER', 'inf'));
  });

  it('the Build list says what each corps takes in men, and why not when the reserve has too few', () => {
    const g = playing('GER');
    g.nations.GER.pp = 5000;
    const inf = specOf(g, 'inf')!;
    let o = offers(g, 'GER').find((x) => x.key === 'corps:inf')!;
    expect(o.men).toBe(inf.divs * MEN_PER_DIV);
    expect(o.why).toBeNull();
    expect(offers(g, 'GER').find((x) => x.group === 'air')!.men).toBeUndefined();
    set(g, 'GER', 1);
    o = offers(g, 'GER').find((x) => x.key === 'corps:inf')!;
    expect(o.why).toMatch(/The reserve has not the men: this takes 45,000 and 15,000 are left\. 36,000 come of age each month\./);
    expect(o.poor).toBe(false);
    // Outside the grand campaign an offer says nothing of men.
    const h = createGame(1, 'GER');
    expect(offers(h, 'GER').every((x) => x.men === undefined)).toBe(true);
  });

  it('the call-up decrees enlarge the class', () => {
    const g = playing('GER');
    const base = manpowerOf(g)!.classMen;
    expect(base).toBe(Math.round(MANPOWER.class.GER * MEN_PER_DIV));
    g.nations.GER.pc = 30;
    govOf(g, 'GER');
    expect(decree(g, 'GER', 'service', 1)).toBe(true);
    expect(manpowerOf(g)!.classMen).toBe(Math.round(MANPOWER.class.GER * 1.15 * MEN_PER_DIV));
    // A computer nation has no decrees: its class is the table's.
    expect(manpowerOf(g, 'SOV')!.classMen).toBe(Math.round(MANPOWER.class.SOV * MEN_PER_DIV));
  });
});

describe('what the player is told', () => {
  it('the reserve in a word and a sentence, as it runs down', () => {
    const g = playing('GER');
    let v = manpowerOf(g)!;
    expect(v.word).toBe('Ample');
    expect(v.men).toBe(MANPOWER.pool.GER * MEN_PER_DIV);
    expect(v.share).toBe(1);
    expect(v.home).toBe(1);
    expect(v.line).toBe('1.6 million men stand in reserve.');
    set(g, 'GER', MANPOWER.pool.GER * (MANPOWER.warn[0] - 0.01));
    v = manpowerOf(g)!;
    expect(v.word).toBe('Thinning');
    expect(v.line).toMatch(/^The reserve is thinning: .* men are left of 1\.6 million\.$/);
    set(g, 'GER', MANPOWER.pool.GER * (MANPOWER.warn[1] - 0.01));
    v = manpowerOf(g)!;
    expect(v.word).toBe('Short');
    expect(v.line).toMatch(/running short/);
    set(g, 'GER', 1);
    v = manpowerOf(g)!;
    expect(v.word).toBe('Spent');
    expect(menLevel(g, 'GER')).toBe(3);
    expect(v.line).toMatch(/The reserve is spent\. Only the 36,000 men who come of age each month reach the armies, and no new corps can be raised: a new infantry corps is about 2 months of them\./);
    expect(manpowerOf(g, 'XXX')).toBeNull();
  });

  it('a warning once at each mark, and again only if the reserve has filled in between', () => {
    const g = playing('GER');
    expect(menWarning(g, 'GER')).toBe(0);
    set(g, 'GER', MANPOWER.pool.GER * 0.4);
    expect(menWarning(g, 'GER')).toBe(1);
    expect(menWarning(g, 'GER')).toBe(0);
    set(g, 'GER', MANPOWER.pool.GER * 0.2);
    expect(menWarning(g, 'GER')).toBe(2);
    set(g, 'GER', 0);
    expect(menWarning(g, 'GER')).toBe(3);
    expect(menWarning(g, 'GER')).toBe(0);
    set(g, 'GER', MANPOWER.pool.GER * 0.4);
    expect(menWarning(g, 'GER')).toBe(0);
    set(g, 'GER', MANPOWER.pool.GER * 0.2);
    expect(menWarning(g, 'GER')).toBe(2);
  });

  it('the month the reserve passes a mark makes the news', () => {
    const g = playing('GER');
    for (const c of g.corps) if (c.nat === 'GER' && !c.dead) c.str = 0.6;
    g.nations.GER.pp = 5000;
    set(g, 'GER', MANPOWER.pool.GER * MANPOWER.warn[0] - 3);
    const h0 = g.headlines.length;
    startPhase(g, 'GER');
    expect(menLeft(g, 'GER')).toBeLessThan(MANPOWER.pool.GER * MANPOWER.warn[0]);
    expect(g.headlines.slice(h0).some((h) => /The reserve of men is thinning/.test(h.text))).toBe(true);
  });

  it('men in words', () => {
    expect(menWords(1660000)).toBe('1.7 million');
    expect(menWords(12000000)).toBe('12 million');
    expect(menWords(345000)).toBe('350,000');
    expect(menWords(36000)).toBe('36,000');
    expect(menWords(-5)).toBe('0');
  });
});

describe('the computer keeps to the same rule', () => {
  it('a staff raises no new corps with men its armies in the field are waiting for', () => {
    const g = free(3);
    const n = g.nations.GER;
    // A staff orders nothing of its own until history's mobilisation has run its course.
    g.turn = 30;
    n.pp = 5000;
    n.queue = [];
    for (const c of g.corps) if (c.nat === 'GER' && !c.dead) c.str = 0.5;
    const short = g.corps.reduce((s, c) => s + (c.nat === 'GER' && !c.dead ? (1 - c.str) * c.divs : 0), 0);
    set(g, 'GER', short);
    expect(menSpare(g, 'GER')).toBeCloseTo(0, 9);
    aiProduction(g, 'GER');
    expect(n.queue.filter((b) => b.what === 'corps')).toEqual([]);
    set(g, 'GER', short + 40);
    aiProduction(g, 'GER');
    expect(n.queue.filter((b) => b.what === 'corps').length).toBeGreaterThan(0);
  });

  it('two years of a war nobody plays: no reserve runs below nothing or above what it opened with, and the Red Army is no longer without end', { timeout: 240_000 }, () => {
    const g = free(2);
    let peak = 0;
    while (g.turn < 40) {
      runTurnAI(g);
      for (const n of Object.values(g.nations)) {
        const have = menLeft(g, n.id);
        expect(have, `${n.id} at ${g.turn}`).toBeGreaterThanOrEqual(-1e-9);
        expect(have, `${n.id} at ${g.turn}`).toBeLessThanOrEqual(poolAtStart(g, n.id) * MANPOWER.most + 1e-9);
      }
      peak = Math.max(peak, armySize(g, 'SOV'));
    }
    expect(manpower(g, 'SOV')!.drawn).toBeGreaterThan(0);
    expect(manpower(g, 'GER')!.drawn).toBeGreaterThan(0);
    // Every man under arms came from somewhere: the army it opened with, the reserve, and the classes since.
    expect(peak).toBeLessThan(84 + MANPOWER.pool.SOV + MANPOWER.class.SOV * 40);
  });
});

describe('the computer\'s guns', () => {
  const G = (arsenal as unknown as { ai: { guns: { per: number; most: number; facing: number } } }).ai.guns;
  /** Germany at war with Poland, rich, with nothing on order; `dug` Polish corps beside German ground are dug in. */
  function facing(dug: number): Game {
    const g = free(3);
    while (!g.nations.GER.atWar.includes('POL') && g.turn < 4) runTurnAI(g);
    expect(g.nations.GER.atWar).toContain('POL');
    const n = g.nations.GER;
    // A staff orders nothing of its own until history's mobilisation has run its course.
    g.turn = 30;
    n.pp = 5000;
    n.queue = [];
    for (const c of g.corps) if (c.dug) delete c.dug;
    const front = g.corps.filter((c) => c.nat === 'POL' && !c.dead && c.kind !== 'fort' && g.regions[c.reg].nb.some((j) => g.regions[j].ctrl === 'GER'));
    expect(front.length).toBeGreaterThanOrEqual(G.facing);
    for (const c of front.slice(0, dug)) c.dug = true;
    return g;
  }
  const guns = (g: Game): number => g.nations.GER.queue.filter((b) => b.what === 'corps' && b.kind === 'art').length;

  it('a staff whose line has met dug-in troops orders massed guns', () => {
    const g = facing(G.facing);
    aiProduction(g, 'GER');
    expect(guns(g)).toBeGreaterThan(0);
  });

  it('with the enemy in the open it orders none', () => {
    const g = facing(G.facing - 1);
    aiProduction(g, 'GER');
    expect(guns(g)).toBe(0);
    expect(g.nations.GER.queue.filter((b) => b.what === 'corps').length).toBeGreaterThan(0);
  });

  it('one corps of guns to every few of the rest, and no more than the staff can use', () => {
    const g = facing(G.facing);
    const army = g.corps.filter((c) => c.nat === 'GER' && !c.dead && c.kind !== 'fort' && c.kind !== 'eng');
    // Enough guns already in the field for the army there is: no more are ordered.
    const need = Math.ceil(army.filter((c) => c.kind !== 'art').length / G.per);
    let made = 0;
    for (const c of army) if (c.kind === 'inf' && made < Math.min(need, G.most)) { c.kind = 'art'; made++; }
    aiProduction(g, 'GER');
    expect(guns(g)).toBe(0);
  });

  it('the historical scenarios and a war at peace order none', () => {
    const g = createGame(3, null);
    for (let k = 0; k < 2; k++) runTurnAI(g);
    for (const c of g.corps) if (c.nat === 'POL' && !c.dead) c.dug = true;
    g.turn = 30;
    g.nations.GER.pp = 5000;
    g.nations.GER.queue = [];
    aiProduction(g, 'GER');
    expect(guns(g)).toBe(0);
  });
});
