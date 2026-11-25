// China's rifles, stage B2: the taps (src/campaign/rifles.ts; the design director's rulings of 04:31Z, 05:37Z, 05:44Z and
// 16:44Z of 25 November 2026 on pacific-japan/dd-form-arms-1125.md, section 15). A new corps takes its rifles from the
// store when it is ordered, by player and computer, in every order; a store short of them refuses; an order taken back
// gives them back; the price of a corps no longer charges points for the rifles the store charges (the carve); a row
// marked binds: false counts its draws and refuses nothing. The tests are numbered as the stage's brief numbers them
// (STAGE-B2-SPEC.md section 6): 1 is in tests/rifles-taps-off.test.ts (the far switch is hoisted per file); 2 to 9
// are the draw at the orders (slice S1), and 27's rifles half. The rest come with the levels and the drafts.
//
// Rows laid for a test (a binding Japan or Britain, a binds: false row) go in by RIFLES.nations and rowsFrom, and are
// taken out after it. Every state is set by hand on the first day's chart; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { rowsFrom, stockOf } from '../src/campaign/arms';
import { offers } from '../src/campaign/arsenal';
import { MANPOWER, levyMen, menLeft } from '../src/campaign/manpower';
import { PRODUCTION, aiProduction, aiTeams, canBuildYet, cadreTerms, cancelBuild, corpsCost, corpsKinds, corpsList, orderCadre, orderCorps, orderElite, orderPara, specOf } from '../src/campaign/economy';
import { aiEngineers } from '../src/campaign/engineers';
import { foldCorps } from '../src/campaign/fold';
import { addCorps, capture, createGame, declareWar, removeCorps } from '../src/campaign/game';
import { RIFLES, binds, canArm, carries, carve, carved, noRifles, riflesView, riflesWatch, type RiflesRow } from '../src/campaign/rifles';
import { loadGame, saveGame } from '../src/campaign/save';
import { rearMonth, startOpen } from '../src/campaign/staff';
import { storyFallen } from '../src/campaign/stories';
import { setTeams } from '../src/campaign/teams';
import { techOf } from '../src/campaign/tech';
import { eliteOf, priceOf } from '../src/campaign/traits';
import { TECH } from '../src/campaign/tech';
import type { Build, Game } from '../src/campaign/types';

const PER = 21300;
const undo: (() => void)[] = [];
afterEach(() => { while (undo.length) undo.pop()!(); riflesWatch.tell = null; });

const grand = (human: string | null = null): Game => startOpen(createGame(3, human));
const stock = (g: Game, nat = 'CHN'): number => stockOf(g, nat, 'rifles');
/** Sets a nation's rifles in store, in its divisions' worth. */
const store = (g: Game, divs: number, nat = 'CHN'): void => { g.flags[`sa:${nat}`] = divs; };
/** Men enough in the reserve for any order here. */
const men = (g: Game, nat = 'CHN'): void => levyMen(g, nat, 30);
/** Lays a rifles row and a rifles shelf (0 on the first day) for a nation, for this test only. */
const lay = (nat: string, row: RiflesRow): void => {
  RIFLES.nations[nat] = row;
  undo.push(() => { delete RIFLES.nations[nat]; });
  undo.push(rowsFrom((_g, x) => (x === nat ? { rifles: { start: 0 } } : undefined)));
};
const corpsOn = (g: Game, nat: string): Build[] => g.nations[nat].queue.filter((b) => b.what === 'corps');
const r1 = (x: number): number => Math.round(x * 10) / 10;
/** What an order leaves untouched when it is refused. */
const state = (g: Game, nat: string) => ({ men: menLeft(g, nat), pp: g.nations[nat].pp, no: g.nations[nat].corpsNo, queue: g.nations[nat].queue.length, stock: stock(g, nat), next: g.nextBuild });

describe('the draw at the orders', () => {
  it("2. an infantry corps takes 3 divisions' worth: 63,900 rifles", () => {
    const g = grand();
    const n = g.nations.CHN;
    men(g);
    store(g, 4);
    expect([carries(g, 'CHN'), binds(g, 'CHN')]).toEqual([true, true]);
    const b = orderCorps(g, 'CHN', 'inf', n.capital)!;
    expect(b).not.toBeNull();
    expect(stock(g)).toBeCloseTo(1, 12);
    expect(b.rifles).toBe(3);
    expect(Math.round(b.rifles! * PER)).toBe(63900);
  });

  it('3. short by a rifle, the order is refused and nothing is drawn', () => {
    const g = grand();
    const n = g.nations.CHN;
    men(g);
    store(g, 2.999);
    n.pp = 100;
    const was = state(g, 'CHN');
    expect(canArm(g, 'CHN', 3)).toBe(false);
    expect(orderCorps(g, 'CHN', 'inf', n.capital)).toBeNull();
    expect(state(g, 'CHN')).toEqual(was);
    // A rifle more than a corps' worth, short by a hair under one: ordered.
    store(g, 3 - 1e-12);
    expect(orderCorps(g, 'CHN', 'inf', n.capital)).not.toBeNull();
  });

  it('4. the four orders and the rear rows all draw and all refuse', () => {
    const g = grand();
    const n = g.nations.CHN, cap = n.capital, capName = g.regions[cap].name;
    men(g);
    n.pp = 500;
    // A corps.
    store(g, 2.99);
    expect(orderCorps(g, 'CHN', 'inf', cap)).toBeNull();
    store(g, 3);
    expect(orderCorps(g, 'CHN', 'inf', cap)!.rifles).toBe(3);
    expect(stock(g)).toBe(0);
    // A cadre: it draws a new corps' rifles whole, and its old armed level is struck.
    const a = addCorps(g, 'CHN', 'Kept corps', 'inf', 2, capName, {}), c = addCorps(g, 'CHN', 'Folded corps', 'inf', 2, capName, {});
    a.str = c.str = 0.2;
    expect(foldCorps(g, a, c)).toBe(true);
    g.flags[`rfa:${c.id}`] = 0.4;
    store(g, 1.99);
    expect(orderCadre(g, 'CHN', c.id, cap)).toBeNull();
    expect(g.flags[`rfa:${c.id}`]).toBe(0.4);
    store(g, 2);
    const bc = orderCadre(g, 'CHN', c.id, cap)!;
    expect([bc.cadre, bc.rifles]).toEqual([c.id, 2]);
    expect(`rfa:${c.id}` in g.flags).toBe(false);
    expect(stock(g)).toBe(0);
    // A corps raised again under a fallen corps' colours.
    const lost = addCorps(g, 'CHN', 'Lost corps', 'inf', 3, capName, {});
    storyFallen(g, lost, 'destroyed', 'JAP', lost.reg);
    removeCorps(g, lost);
    const f = g.stories.fallen.at(-1)!;
    store(g, 2.9);
    expect(orderCorps(g, 'CHN', 'inf', cap, f.id)).toBeNull();
    store(g, 3);
    const br = orderCorps(g, 'CHN', 'inf', cap, f.id)!;
    expect([br.revive, br.rifles]).toEqual([f.id, 3]);
    // An engineer corps through the engineers' rule: more repairs waiting than engineers (factories in ruins).
    const ruins = g.regions.filter((r) => r.ctrl === 'CHN' && r.fac > 0);
    expect(ruins.length).toBeGreaterThan(0);
    for (const r of ruins) g.flags[`ruin:${r.id}`] = 1;
    const eng = (): Build[] => corpsOn(g, 'CHN').filter((b) => b.kind === 'eng');
    store(g, 0.99);
    aiEngineers(g, 'CHN');
    expect(eng()).toEqual([]);
    store(g, 1);
    aiEngineers(g, 'CHN');
    expect(eng().map((b) => b.rifles)).toEqual([1]);
  });

  it('4. (laid rows) a support team, the elite, the airborne and a rear garrison of a binding nation draw and refuse', () => {
    lay('JAP', { perDiv: 15000, refit: 0.05, binds: true });
    lay('UK', { perDiv: 15000, refit: 0.05, binds: true });
    lay('GER', { perDiv: 15000, refit: 0.05, binds: true });
    const g = grand();
    for (const nat of ['JAP', 'UK', 'GER']) { men(g, nat); g.nations[nat].pp = 1000; }
    // A support team through the staff's own rule (teams switched on for the test; Germany's, at war on the first day,
    // since Japan's staff, an ocean from every enemy but China, keeps its army idle there).
    setTeams(g, true);
    const teams = (): Build[] => corpsOn(g, 'GER').filter((b) => b.kind === 'mg' || b.kind === 'mor');
    store(g, 0.99, 'GER');
    aiTeams(g, 'GER');
    expect(teams()).toEqual([]);
    store(g, 1, 'GER');
    aiTeams(g, 'GER');
    expect(teams().map((b) => b.rifles)).toEqual([1]);
    // The airborne (Japan, its airborne forces researched for the test).
    techOf(g, 'JAP')!.done.push('airborne');
    const p = TECH.units.para;
    store(g, p.divs - 0.01, 'JAP');
    expect(orderPara(g, 'JAP', g.nations.JAP.capital)).toBeNull();
    store(g, p.divs, 'JAP');
    expect(orderPara(g, 'JAP', g.nations.JAP.capital)!.rifles).toBe(p.divs);
    // The elite (Britain's: Japan has no special formation).
    const u = eliteOf('UK')!;
    store(g, u.divs - 0.01, 'UK');
    expect(orderElite(g, 'UK', g.nations.UK.capital)).toBeNull();
    store(g, u.divs, 'UK');
    expect(orderElite(g, 'UK', g.nations.UK.capital)!.rifles).toBe(u.divs);
    expect(stock(g, 'UK')).toBe(0);
  });

  it("4. (laid rows) a binding Japan's rear garrison draws its worth, is refused with less, and its level reads its worth", () => {
    lay('JAP', { perDiv: 15000, refit: 0.05, binds: true });
    const g = grand();
    g.turn = 29;
    g.flags['st:done:JAP:south'] = 28;
    for (const e of ['USA', 'UK', 'NED', 'AUS']) declareWar(g, 'JAP', e);
    const r = g.byName.Rabaul;
    for (const c of [...r.units]) if (c.nat !== 'JAP') removeCorps(g, c);
    capture(g, r, 'JAP');
    g.turn = 30;
    const mine = () => r.units.filter((c) => c.nat === 'JAP' && !c.dead);
    const was = menLeft(g, 'JAP');
    store(g, 0.09, 'JAP');
    rearMonth(g, 'JAP');
    expect(mine()).toEqual([]);
    expect(menLeft(g, 'JAP')).toBe(was);
    store(g, 0.1, 'JAP');
    rearMonth(g, 'JAP');
    const [c] = mine();
    expect([c.name, Math.round(c.str * 1000) / 1000, c.full]).toEqual(['8th Base Force (Rabaul)', 0.1, 0.1]);
    expect(stock(g, 'JAP')).toBeCloseTo(0, 12);
    // (Its level is its 'worth' through c.full, which the armed level reads when the drafts come: no flag of its own.)
    expect(`rfa:${c.id}` in g.flags).toBe(false);
  });

  it('5. an order taken back in its month returns its rifles and its men', () => {
    const g = grand();
    const n = g.nations.CHN;
    men(g);
    store(g, 3);
    const m = menLeft(g, 'CHN');
    const b = orderCorps(g, 'CHN', 'inf', n.capital)!;
    expect([b.cost, stock(g)]).toEqual([0, 0]);
    expect(menLeft(g, 'CHN')).toBeCloseTo(m - 3, 9);
    expect(cancelBuild(g, 'CHN', b.id)).toBe(true);
    expect(stock(g)).toBe(3);
    expect(menLeft(g, 'CHN')).toBeCloseTo(m, 9);
    // A nation with no row: its cancel is today's, its men back only for an order that cost points.
    const j = g.nations.JAP;
    men(g, 'JAP');
    j.pp = 500;
    const mj = menLeft(g, 'JAP');
    const bj = orderCorps(g, 'JAP', 'inf', j.capital)!;
    expect(bj.cost).toBeGreaterThan(0);
    expect(bj.rifles).toBeUndefined();
    expect(cancelBuild(g, 'JAP', bj.id)).toBe(true);
    expect(menLeft(g, 'JAP')).toBeCloseTo(mj, 9);
    const free: Build = { id: g.nextBuild++, what: 'corps', kind: 'inf', divs: 3, name: 'A free corps', region: j.capital, ordered: g.turn, eta: g.turn + 4, cost: 0 };
    j.queue.push(free);
    const mf = menLeft(g, 'JAP');
    expect(cancelBuild(g, 'JAP', free.id)).toBe(true);
    expect(menLeft(g, 'JAP')).toBe(mf);
  });

  it('6. the offer says why, in the form\'s words', () => {
    const g = grand();
    const n = g.nations.CHN;
    men(g);
    const offer = (key: string, nat = 'CHN') => offers(g, nat, g.nations[nat].capital).find((o) => o.key === key)!;
    store(g, 21000 / PER);
    expect(offer('corps:inf').rifles).toBe(63900);
    expect(offer('corps:mtn').rifles).toBe(42600);
    const inflow = riflesView(g, 'CHN')!.inflow.total;
    const months = Math.ceil((63900 - 21000) / inflow);
    // (On the first day about 8,300 a month and 6 months, D: the arsenals 4,500, the Burma Road 2,500, the Sinkiang road 1,300.)
    expect(inflow).toBeCloseTo(8300, -2);
    expect(months).toBe(6);
    const hundred = (Math.round(inflow / 100) * 100).toLocaleString('en-US');
    expect(offer('corps:inf').why).toBe(`Not rifles enough: this corps takes 64,000 and 21,000 are in store. ${hundred} come in each month, so it can be ordered in ${months} months.`);
    expect(noRifles(g, 'CHN', 3)).toBe(offer('corps:inf').why);
    // Every road and works stopped: the clause after "store." is gone.
    const kept = RIFLES.roads.splice(0);
    undo.push(() => { RIFLES.roads.splice(0, RIFLES.roads.length, ...kept); });
    for (const place of ['Chungking', 'Changteh', 'Kweiyang']) g.byName[place].ctrl = 'JAP';
    expect(riflesView(g, 'CHN')!.inflow.total).toBe(0);
    expect(offer('corps:inf').why).toBe('Not rifles enough: this corps takes 64,000 and 21,000 are in store.');
    // A store that holds the corps: no refusal.
    store(g, 3);
    expect(offer('corps:inf').why).toBeNull();
    void n;
    // Japan's offers carry no rifles, and none of its reasons is the store's.
    for (const o of offers(g, 'JAP', g.nations.JAP.capital)) {
      expect(o.rifles, o.key).toBeUndefined();
      expect(o.why ?? '', o.key).not.toMatch(/rifles/);
    }
  });

  it('7. the computer orders nothing it cannot arm', () => {
    const g = grand();
    const n = g.nations.CHN, capName = g.regions[n.capital].name;
    men(g);
    n.pp = 500;
    const a = addCorps(g, 'CHN', 'Kept corps', 'inf', 3, capName, {}), c = addCorps(g, 'CHN', 'Folded corps', 'inf', 3, capName, {});
    a.str = c.str = 0.2;
    expect(foldCorps(g, a, c)).toBe(true);
    expect(n.cadres!.map((x) => [x.id, x.divs])).toEqual([[c.id, 3]]);
    // A cadre of 3 divisions waiting, 2.9 in store: no cadre and no new corps.
    store(g, 2.9);
    aiProduction(g, 'CHN');
    expect(corpsOn(g, 'CHN')).toEqual([]);
    expect(stock(g)).toBe(2.9);
    // No cadre waiting, money and men, 2.9 in store: no corps; 3.0: one infantry corps, and the month's raising ends.
    n.cadres = [];
    store(g, 2.9);
    aiProduction(g, 'CHN');
    expect(corpsOn(g, 'CHN')).toEqual([]);
    store(g, 3);
    aiProduction(g, 'CHN');
    expect(corpsOn(g, 'CHN').map((b) => [b.kind, b.rifles])).toEqual([['inf', 3]]);
    expect(stock(g)).toBe(0);
  });

  it("7. (laid rows) a binding nation's staff orders no elite and no team it cannot arm, and its month's raising ends", () => {
    lay('UK', { perDiv: 15000, refit: 0.05, binds: true });
    lay('GER', { perDiv: 15000, refit: 0.05, binds: true });
    const g = grand();
    for (const nat of ['GER', 'UK']) { men(g, nat); g.nations[nat].pp = 1000; }
    // (Britain's staff waits on history's programmes in its first months: the test stands later, with the war on.)
    g.turn = 40;
    expect(canBuildYet(g, 'UK')).toBe(true);
    // Britain's elite comes first in its staff's month; with nothing in store nothing at all is ordered.
    aiProduction(g, 'UK');
    expect(corpsOn(g, 'UK')).toEqual([]);
    store(g, 100, 'UK');
    aiProduction(g, 'UK');
    expect(corpsOn(g, 'UK').some((b) => b.elite)).toBe(true);
    expect(corpsOn(g, 'UK').some((b) => !b.elite)).toBe(true);
    // Germany's support team (teams on) and its corps: none with nothing in store; with plenty, both.
    setTeams(g, true);
    expect(canBuildYet(g, 'GER')).toBe(true);
    aiTeams(g, 'GER');
    aiProduction(g, 'GER');
    expect(corpsOn(g, 'GER')).toEqual([]);
    store(g, 100, 'GER');
    aiTeams(g, 'GER');
    aiProduction(g, 'GER');
    expect(corpsOn(g, 'GER').some((b) => b.kind === 'mg' || b.kind === 'mor')).toBe(true);
    expect(corpsOn(g, 'GER').some((b) => b.kind !== 'mg' && b.kind !== 'mor')).toBe(true);
  });
});

describe('the carve', () => {
  it("8. the carve: China's prices as the form's table", () => {
    const g = grand();
    setTeams(g, true);
    const want: Record<string, number> = { inf: 0, mtn: 2, eng: 2, mot: 18, art: 34, arm: 34 };
    expect(carve(g, 'CHN', 1)).toBe(16);
    for (const [kind, pts] of Object.entries(want)) expect(carved(g, 'CHN', corpsList(g, 'CHN', kind), specOf(g, kind)!.divs), kind).toBe(pts);
    // The support teams carve the same way, to 0 and 2.
    expect([carved(g, 'CHN', corpsList(g, 'CHN', 'mg'), 1), carved(g, 'CHN', corpsList(g, 'CHN', 'mor'), 1)]).toEqual([0, 2]);
    // As corpsCost gives them: rider B still asks nothing for infantry and mountain until China's men are laid (S3).
    for (const kind of ['mot', 'art', 'arm', 'eng']) expect(corpsCost(g, 'CHN', kind), kind).toBe(want[kind]);
    for (const kind of ['inf', 'mtn']) expect(corpsCost(g, 'CHN', kind), kind).toBe(0);
    // Japan pays its list price, untouched, for every kind.
    for (const kind of corpsKinds(g)) expect(corpsCost(g, 'JAP', kind), kind).toBe(corpsList(g, 'JAP', kind));
    // China's cadres are priced from the carved price: carved x fold.price x size.
    const capName = g.regions[g.nations.CHN.capital].name;
    for (const [kind, divs] of [['inf', 3], ['mtn', 2], ['mot', 2]] as const) {
      const c = addCorps(g, 'CHN', `A ${kind} cadre`, kind, divs, capName, {});
      expect(cadreTerms(g, 'CHN', c).cost, kind).toBe(r1(corpsCost(g, 'CHN', kind) * MANPOWER.fold.price * 1));
    }
    expect(cadreTerms(g, 'CHN', addCorps(g, 'CHN', 'A mot cadre', 'mot', 2, capName, {})).cost).toBe(r1(18 * MANPOWER.fold.price));
  });

  it("8. (laid rows) a binding nation's elite and airborne are carved, as orders, as offers and as cadres, and draw their rifles", () => {
    lay('JAP', { perDiv: 15000, refit: 0.05, binds: true });
    lay('UK', { perDiv: 15000, refit: 0.05, binds: true });
    const g = grand();
    for (const nat of ['JAP', 'UK']) { men(g, nat); g.nations[nat].pp = 1000; store(g, 10, nat); }
    const u = eliteOf('UK')!, p = TECH.units.para;
    const elite = Math.max(0, r1(u.cost - (u.divs * corpsList(g, 'UK', 'inf')) / specOf(g, 'inf')!.divs));
    const paraList = priceOf(g, 'JAP', 'para', p.cost);
    const para = Math.max(0, r1(paraList - (p.divs * corpsList(g, 'JAP', 'inf')) / specOf(g, 'inf')!.divs));
    expect(para).toBeLessThan(paraList);
    expect(offers(g, 'UK', g.nations.UK.capital).find((o) => o.key === 'elite')!.cost).toBe(elite);
    techOf(g, 'JAP')!.done.push('airborne');
    expect(offers(g, 'JAP', g.nations.JAP.capital).find((o) => o.key === 'para')!.cost).toBe(para);
    const ppU = g.nations.UK.pp, ppJ = g.nations.JAP.pp;
    const be = orderElite(g, 'UK', g.nations.UK.capital)!, bp = orderPara(g, 'JAP', g.nations.JAP.capital)!;
    expect([be.cost, be.rifles, r1(ppU - g.nations.UK.pp)]).toEqual([elite, u.divs, elite]);
    expect([bp.cost, bp.rifles, r1(ppJ - g.nations.JAP.pp)]).toEqual([para, p.divs, para]);
    // As cadres (cadreTerms): the carved price, times the fold's share.
    const ce = addCorps(g, 'UK', 'An elite cadre', u.kind, u.divs, g.regions[g.nations.UK.capital].name, {});
    ce.elite = u.id;
    const cp = addCorps(g, 'JAP', 'An airborne cadre', 'inf', p.divs, g.regions[g.nations.JAP.capital].name, { airborne: true });
    cp.airborne = true;
    expect(cadreTerms(g, 'UK', ce).cost).toBe(r1(elite * MANPOWER.fold.price));
    expect(cadreTerms(g, 'JAP', cp).cost).toBe(r1(para * MANPOWER.fold.price));
    // And with no row the same orders keep their list prices.
    delete RIFLES.nations.JAP;
    expect(cadreTerms(g, 'JAP', cp).cost).toBe(r1(paraList * MANPOWER.fold.price));
  });
});

describe('binds: false', () => {
  it('9. binds false counts and refuses nothing', () => {
    lay('JAP', { perDiv: 15000, refit: 0.05, binds: false });
    const g = grand();
    const j = g.nations.JAP;
    men(g, 'JAP');
    j.pp = 1000;
    store(g, 1, 'JAP');
    expect([carries(g, 'JAP'), binds(g, 'JAP'), canArm(g, 'JAP', 3), noRifles(g, 'JAP', 3), carve(g, 'JAP', 3)]).toEqual([true, false, true, null, 0]);
    for (const kind of corpsKinds(g)) expect(corpsCost(g, 'JAP', kind), kind).toBe(corpsList(g, 'JAP', kind));
    const told: number[] = [];
    riflesWatch.tell = (e) => { if (e.nat === 'JAP') told.push(e.rifles); };
    // What there is is drawn, and the order is never refused.
    const a = orderCorps(g, 'JAP', 'inf', j.capital)!;
    expect([a.rifles, stock(g, 'JAP')]).toEqual([1, 0]);
    const b = orderCorps(g, 'JAP', 'inf', j.capital)!;
    expect(b).not.toBeNull();
    expect(b.rifles).toBeUndefined();
    expect(told).toEqual([1, 0]);
    const inf = offers(g, 'JAP', j.capital).find((o) => o.key === 'corps:inf')!;
    expect([inf.rifles, inf.why]).toEqual([45000, null]);
    // Taken back, the first gives its rifles back.
    expect(cancelBuild(g, 'JAP', a.id)).toBe(true);
    expect(stock(g, 'JAP')).toBe(1);
  });

  it('9. (and only false frees it) a row that says nothing of binds binds', () => {
    lay('UK', { perDiv: 15000 });
    const g = grand();
    store(g, 2.9, 'UK');
    expect([carries(g, 'UK'), binds(g, 'UK'), canArm(g, 'UK', 3)]).toEqual([true, true, false]);
    expect(carve(g, 'UK', 3)).toBe(corpsList(g, 'UK', 'inf'));
  });
});

describe('the save', () => {
  it('27. a save keeps what B2 holds (the orders\' rifles)', () => {
    const g = grand('CHN');
    men(g);
    store(g, 5);
    const b = orderCorps(g, 'CHN', 'inf', g.nations.CHN.capital)!;
    const h = loadGame(saveGame(g))!;
    expect(h.nations.CHN.queue.find((x) => x.id === b.id)!.rifles).toBe(3);
    const flags = (x: Game) => Object.entries(x.flags).filter(([k]) => /^(sa|sam|rfa|rfo|rflm|rflc):/.test(k)).sort();
    expect(flags(h)).toEqual(flags(g));
    expect(cancelBuild(h, 'CHN', b.id)).toBe(true);
    expect(stock(h)).toBe(5);
  });
});

void PRODUCTION;
