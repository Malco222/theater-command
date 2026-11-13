// Government and the war cabinet (the grand campaign). Every nation has a form of government. The player's has a
// war cabinet of four seats he fills with ministers, each with a gain and a price, and decrees that put the economy
// and the call-up on a war footing in steps; both are paid for in political capital. The cabinet's trust in him
// moves with the month's news. Every few months the cabinet, or an ally, sets a directive: an optional job with a
// deadline and a reward. Computer nations keep a caretaker cabinet, so nothing here moves their wars, and the
// historical scenarios have none of it. The numbers are in data/campaign/government.json.
import { describe, expect, it } from 'vitest';
import { DIPLOMACY, pcIncome } from '../src/campaign/diplomacy';
import { corpsCost, income, ledger, specOf } from '../src/campaign/economy';
import { createGame, startPhase } from '../src/campaign/game';
import {
  GOV, appoint, appointCost, armyCount, cabinetOff, decree, directiveOdds, formOf, govCapital, govMonth, govMul, govOf, govRules, government, judgeDirective, setAside, setCabinet, whyNotAppoint, whyNotDecree,
  whyNotSetAside,
} from '../src/campaign/government';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { TECH, startResearch, techOf, techTick, whyNotResearch } from '../src/campaign/tech';
import { signTrade, tradeIncome } from '../src/campaign/trade';
import {
  beginHuman, cabinetIsOff, endHuman, governmentOf, humanUp, orderAppoint, orderCabinet, orderDecree, orderSetAside, runTurnAI, stepAI, whyNotAppointMinister, whyNotOrderDecree, whyNotSetAsideDirective,
} from '../src/campaign/turn';
import type { BattleReport, Directive, Game, GovState } from '../src/campaign/types';

const free = (seed: number, human: string | null = null): Game => startOpen(createGame(seed, human));
/** A grand campaign with the player's first month open. */
function playing(human = 'GER', seed = 3): Game {
  const g = free(seed, human);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
/** The player ends his month, everyone else plays, and his next month opens. */
function nextMonth(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
}
const copy = (g: Game): Game => loadGame(saveGame(g))!;
/** The player's government with nothing due: no directive on the way, and the cabinet has read all the news there is. */
function quiet(g: Game, nat = g.human!): GovState {
  const st = govOf(g, nat)!;
  delete st.dir;
  st.next = g.turn + 99;
  govMonth(g, nat);
  return st;
}
const rich = (g: Game, nat = g.human!): void => { g.nations[nat].pc = DIPLOMACY.pcCap; };
const swing = (nat: string): number => GOV.forms[formOf(nat)].swing;
const job = (g: Game, nat: string, d: Partial<Directive>): Directive => ({
  id: g.turn, nat, kind: 'take', from: nat, issued: g.turn, due: g.turn + 3, capital: 6, money: 2, title: 'Take the town', text: 'The war cabinet wants the town taken.', ...d,
});
const said = (g: Game, re: RegExp, from = 0): boolean => g.headlines.slice(from).some((h) => re.test(h.text));

describe('government is for the grand campaign', () => {
  it('the historical scenarios have none', () => {
    const g = createGame(3, 'GER');
    expect(govRules(g)).toBe(false);
    expect(government(g)).toBeNull();
    expect(govOf(g, 'GER')).toBeNull();
    expect(govMul(g, 'GER', 'income')).toBe(1);
    expect(govCapital(g, 'GER')).toBe(0);
    expect(whyNotAppoint(g, 'GER', 'staff', 'organiser')).toMatch(/grand campaign/);
    expect(appoint(g, 'GER', 'staff', 'organiser')).toBe(false);
    expect(decree(g, 'GER', 'economy', 1)).toBe(false);
    beginHuman(g);
    govMonth(g, 'GER');
    judgeDirective(g, 'GER');
    expect(g.nations.GER.gov).toBeUndefined();
  });

  it('every nation has a form of government, by its history', () => {
    expect(formOf('GER')).toBe('oneparty');
    expect(formOf('SOV')).toBe('oneparty');
    expect(formOf('UK')).toBe('democracy');
    expect(formOf('FRA')).toBe('democracy');
    expect(formOf('POL')).toBe('authoritarian');
    expect(formOf('JAP')).toBe('authoritarian');
    for (const f of Object.values(GOV.forms)) { expect(f.name.length).toBeGreaterThan(5); expect(f.blurb.length).toBeGreaterThan(20); }
    for (const [nat, f] of Object.entries(GOV.nations)) if (nat !== '_default') expect(GOV.forms[f], nat).toBeDefined();
  });

  it('only the player has a cabinet: a computer nation shows its form and nothing more, and no lever moves for it', () => {
    const g = playing('GER');
    expect(g.nations.GER.gov).toBeDefined();
    for (const n of Object.values(g.nations)) if (n.id !== 'GER') expect(n.gov, n.id).toBeUndefined();
    const uk = government(g, 'UK')!;
    expect(uk.mine).toBe(false);
    expect(uk.form.id).toBe('democracy');
    expect(uk.seats).toEqual([]);
    expect(uk.directive).toBeNull();
    // Even with a cabinet written in by hand, a nation the computer plays gets nothing from it.
    g.nations.UK.gov = { ...govOf(g, 'GER')!, seats: { finance: 'taxman' }, steps: { economy: 2 } };
    for (const lever of ['income', 'drafts', 'cost', 'research', 'trade'] as const) expect(govMul(g, 'UK', lever)).toBe(1);
    expect(govCapital(g, 'UK')).toBe(0);
    expect(appoint(g, 'UK', 'staff', 'organiser')).toBe(false);
    expect(governmentOf(g)!.nat).toBe('GER');
    expect(governmentOf(g, 'UK')!.mine).toBe(false);
  });

  it('a war the computer fights alone has no governments in it', () => {
    const g = free(5);
    for (let k = 0; k < 4; k++) runTurnAI(g);
    for (const n of Object.values(g.nations)) expect(n.gov, n.id).toBeUndefined();
    expect(government(g)).toBeNull();
  });

  it('a panel that only looks makes nothing', () => {
    const g = free(3, 'GER');
    delete g.nations.GER.gov;
    const v = government(g, 'GER')!;
    expect(v.mine).toBe(true);
    expect(v.seats).toHaveLength(4);
    expect(whyNotAppoint(g, 'GER', 'staff', 'caretaker')).toMatch(/already/);
    expect(whyNotDecree(g, 'GER', 'economy', 0)).toMatch(/already/);
    expect(whyNotSetAside(g, 'GER')).toMatch(/no directive/);
    expect(g.nations.GER.gov).toBeUndefined();
  });

  it('a save keeps the cabinet, the decrees, the trust and the directive', () => {
    const g = playing('GER');
    rich(g);
    expect(orderAppoint(g, 'finance', 'taxman')).toBe(true);
    expect(orderDecree(g, 'economy', 1)).toBe(true);
    const st = govOf(g, 'GER')!;
    st.dir = job(g, 'GER', { region: g.byName['Warsaw'].id });
    st.trust = 41.5;
    const h = copy(g);
    expect(h.nations.GER.gov).toEqual(g.nations.GER.gov);
    expect(government(h)).toEqual(government(g));
    expect(income(h, 'GER')).toBeCloseTo(income(g, 'GER'), 9);
  });

  it('a save from before governments gets one the first time it is needed', () => {
    const g = playing('GER');
    delete g.nations.GER.gov;
    const h = copy(g);
    expect(h.nations.GER.gov).toBeUndefined();
    rich(h);
    expect(appoint(h, 'GER', 'staff', 'organiser')).toBe(true);
    expect(h.nations.GER.gov!.seats.staff).toBe('organiser');
    expect(h.nations.GER.gov!.next).toBe(h.turn + GOV.directives.first);
  });
});

describe('the war cabinet', () => {
  it('opens with caretakers in four offices, and two candidates for each', () => {
    const v = government(playing('GER'))!;
    expect(v.seats.map((s) => s.id)).toEqual(['staff', 'arms', 'foreign', 'finance']);
    for (const s of v.seats) {
      expect(s.minister.id).toBe('caretaker');
      expect(s.options).toHaveLength(2);
      for (const o of s.options) { expect(o.plus.length).toBeGreaterThan(8); expect(o.minus.length).toBeGreaterThan(8); }
    }
    expect(v.sums).toEqual(['Political capital +1 a month']);
    expect(v.form.id).toBe('oneparty');
    expect(v.appointCost).toBe(GOV.appoint.cost);
    expect(v.capitalMost).toBe(DIPLOMACY.pcCap);
  });

  it('an appointment costs political capital, and the office cannot change hands again for some months', () => {
    const g = playing('GER');
    const n = g.nations.GER;
    n.pc = GOV.appoint.cost - 1;
    expect(whyNotAppointMinister(g, 'staff', 'organiser')).toMatch(/political capital/);
    expect(orderAppoint(g, 'staff', 'organiser')).toBe(false);
    n.pc = GOV.appoint.cost + 2;
    const h0 = g.headlines.length;
    expect(whyNotAppointMinister(g, 'staff', 'organiser')).toBeNull();
    expect(orderAppoint(g, 'staff', 'organiser')).toBe(true);
    expect(n.pc).toBe(2);
    expect(said(g, /A new Chief of the General Staff/, h0)).toBe(true);
    const v = government(g)!;
    expect(v.seats[0].minister.id).toBe('organiser');
    expect(v.seats[0].options.map((o) => o.id).sort()).toEqual(['caretaker', 'reformer']);
    expect(v.log[0].text).toMatch(/organiser/);
    rich(g);
    expect(whyNotAppointMinister(g, 'staff', 'reformer')).toMatch(/changed hands lately/);
    expect(whyNotAppointMinister(g, 'staff', 'organiser')).toMatch(/already/);
    g.turn += GOV.appoint.wait;
    expect(whyNotAppoint(g, 'GER', 'staff', 'reformer')).toBeNull();
  });

  it('a minister sits only in his own office, and a caretaker is free', () => {
    const g = playing('GER');
    rich(g);
    expect(whyNotAppointMinister(g, 'arms', 'organiser')).toMatch(/Nobody of that kind/);
    expect(whyNotAppointMinister(g, 'navy', 'organiser')).toMatch(/Nobody of that kind/);
    expect(whyNotAppointMinister(g, 'staff', 'nobody')).toMatch(/Nobody of that kind/);
    expect(orderAppoint(g, 'arms', 'planner')).toBe(true);
    g.turn += GOV.appoint.wait;
    g.nations.GER.pc = 0;
    expect(appoint(g, 'GER', 'arms', 'caretaker')).toBe(true);
    expect(government(g)!.seats[1].minister.id).toBe('caretaker');
    expect(g.nations.GER.pc).toBe(0);
  });

  it('the Minister of Finance moves income, and the ledger shows it on a line of its own', () => {
    const g = playing('GER');
    const before = income(g, 'GER');
    expect(ledger(g, 'GER').changes.find((l) => l.key === 'cabinet')).toBeUndefined();
    rich(g);
    appoint(g, 'GER', 'finance', 'taxman');
    expect(govMul(g, 'GER', 'income')).toBeCloseTo(1.06, 9);
    expect(income(g, 'GER')).toBeCloseTo(before * 1.06, 6);
    const L = ledger(g, 'GER');
    expect(L.gross).toBeCloseTo(income(g, 'GER'), 6);
    expect(L.changes.find((l) => l.key === 'cabinet')!.amount).toBeCloseTo(before * 0.06, 6);
    expect(government(g)!.sums).toContain('Income +6%');
    expect(government(g)!.sums).toContain('Trust −0.5 a month');
  });

  it('the Minister of Armaments moves the price of new corps', () => {
    const g = playing('GER');
    const list = corpsCost(g, 'GER', 'inf');
    rich(g);
    appoint(g, 'GER', 'arms', 'industrialist');
    expect(corpsCost(g, 'GER', 'inf')).toBeCloseTo(Math.round(list * 0.92 * 10) / 10, 9);
    expect(corpsCost(g, 'GER', 'arm')).toBeLessThan(specOf(g, 'arm')!.cost);
    expect(corpsCost(g, 'UK', 'inf')).toBe(corpsCost(copy(g), 'UK', 'inf'));
    g.turn += GOV.appoint.wait;
    appoint(g, 'GER', 'arms', 'planner');
    expect(corpsCost(g, 'GER', 'inf')).toBeCloseTo(Math.round(list * 1.04 * 10) / 10, 9);
    expect(government(g)!.sums).toContain('Price of new corps +4%');
  });

  it('the Chief of the General Staff quickens the replacements, or the research', () => {
    const g = playing('GER');
    const c = g.corps.find((x) => x.nat === 'GER' && !x.dead && x.supplied && x.kind === 'inf')!;
    c.str = 0.4;
    g.nations.GER.pp = 2000;
    const tech = TECH.techs.find((t) => whyNotResearch(g, 'GER', t.id) === null)!;
    expect(startResearch(g, 'GER', tech.id)).toBe(true);
    const plain = copy(g), org = copy(g), ref = copy(g);
    govOf(org, 'GER')!.seats.staff = 'organiser';
    govOf(ref, 'GER')!.seats.staff = 'reformer';
    for (const x of [plain, org, ref]) startPhase(x, 'GER');
    const str = (x: Game): number => x.corps.find((k) => k.id === c.id)!.str;
    expect(str(org)).toBeGreaterThan(str(plain));
    expect((str(org) - 0.4) / (str(plain) - 0.4)).toBeCloseTo(1.15, 2);
    expect((str(ref) - 0.4) / (str(plain) - 0.4)).toBeCloseTo(0.95, 2);
    // Research: the same month in the laboratories, a tenth more done.
    const left = (x: Game): number => techOf(x, 'GER')!.work.find((w) => w.id === tech.id)!.left;
    const was = left(g);
    const a = copy(g), b = copy(g);
    govOf(b, 'GER')!.seats.staff = 'reformer';
    techTick(a, 'GER');
    techTick(b, 'GER');
    expect((was - left(b)) / (was - left(a))).toBeCloseTo(1.1, 6);
  });

  it('the Foreign Minister makes trade pay more, or brings political capital', () => {
    const g = playing('GER');
    if (tradeIncome(g, 'GER') === 0) for (const n of Object.values(g.nations)) if (signTrade(g, 'GER', n.id, new Map(), true)) break;
    const before = tradeIncome(g, 'GER');
    expect(before).toBeGreaterThan(0);
    const st = quiet(g);
    st.trust = 60;
    st.seats.foreign = 'merchant';
    expect(tradeIncome(g, 'GER')).toBeCloseTo(before * 1.25, 9);
    st.seats.foreign = 'statesman';
    expect(tradeIncome(g, 'GER')).toBeCloseTo(before * 0.9, 9);
    // Germany's one-party state adds 1 a month, and the statesman 1 more.
    expect(govCapital(g, 'GER')).toBe(2);
    expect(government(g)!.capitalMonth).toBeCloseTo(pcIncome('GER') + 2, 6);
    expect(government(g)!.sums).toContain('Political capital +2 a month');
  });

  it('political capital comes in with the month, and what the cabinet adds comes with it', () => {
    const g = playing('GER');
    const st = quiet(g);
    st.seats.foreign = 'statesman';
    st.trust = 60;
    const a = copy(g), b = copy(g);
    delete b.nations.GER.gov!.seats.foreign;
    a.nations.GER.pc = 5; b.nations.GER.pc = 5;
    startPhase(a, 'GER');
    startPhase(b, 'GER');
    expect(a.nations.GER.pc - b.nations.GER.pc).toBeCloseTo(1, 6);
    expect(b.nations.GER.pc).toBeCloseTo(5 + pcIncome('GER') + 1, 1);
    // It never passes the most a nation can hold.
    a.nations.GER.pc = DIPLOMACY.pcCap - 1;
    startPhase(a, 'GER');
    expect(a.nations.GER.pc).toBe(DIPLOMACY.pcCap);
  });

  it('orders are the player\'s, in his own month', () => {
    const g = playing('GER');
    rich(g);
    endHuman(g);
    expect(whyNotAppointMinister(g, 'staff', 'organiser')).toBe('It is not your month.');
    expect(orderAppoint(g, 'staff', 'organiser')).toBe(false);
    expect(whyNotOrderDecree(g, 'economy', 1)).toBe('It is not your month.');
    expect(orderDecree(g, 'economy', 1)).toBe(false);
    expect(whyNotSetAsideDirective(g)).toBe('It is not your month.');
    expect(orderSetAside(g)).toBe(false);
  });
});

describe('decrees', () => {
  it('a country at peace keeps a peacetime economy', () => {
    const g = playing('ITA');
    rich(g);
    expect(g.nations.ITA.atWar).toEqual([]);
    expect(whyNotOrderDecree(g, 'economy', 1)).toMatch(/at war/);
    expect(whyNotOrderDecree(g, 'service', 1)).toMatch(/at war/);
    const v = government(g)!;
    expect(v.tracks.map((t) => t.at)).toEqual([0, 0]);
    expect(v.tracks[0].steps[0].on).toBe(true);
    expect(v.tracks[0].steps[1].why).toMatch(/at war/);
  });

  it('a war economy is carried for its price, and pays', () => {
    const g = playing('GER');
    const before = income(g, 'GER');
    g.nations.GER.pc = 7;
    expect(whyNotOrderDecree(g, 'economy', 1)).toMatch(/political capital/);
    rich(g);
    const h0 = g.headlines.length;
    expect(orderDecree(g, 'economy', 1)).toBe(true);
    expect(g.nations.GER.pc).toBe(DIPLOMACY.pcCap - 8);
    expect(income(g, 'GER')).toBeCloseTo(before * 1.06, 6);
    expect(said(g, /By decree: war economy/, h0)).toBe(true);
    const v = government(g)!;
    expect(v.tracks[0].at).toBe(1);
    expect(v.tracks[0].steps[1].on).toBe(true);
    expect(v.sums).toContain('Income +6%');
    expect(whyNotOrderDecree(g, 'economy', 1)).toMatch(/already/);
  });

  it('one step at a time, and the hardest steps only when the country will bear them', () => {
    const g = playing('GER');
    rich(g);
    expect(whyNotOrderDecree(g, 'economy', 2)).toMatch(/One step at a time/);
    expect(whyNotOrderDecree(g, 'economy', 5)).toMatch(/no such decree/);
    expect(whyNotOrderDecree(g, 'weather', 1)).toMatch(/no such decree/);
    orderDecree(g, 'economy', 1);
    expect(whyNotOrderDecree(g, 'economy', 2)).toMatch(/12 months of war/);
    orderDecree(g, 'service', 1);
    expect(govMul(g, 'GER', 'drafts')).toBeCloseTo(1.2, 9);
    expect(whyNotOrderDecree(g, 'service', 2)).toMatch(/back to the wall/);
    // A year of war on, total mobilisation; thirty months on, the last levy.
    g.turn += GOV.needs.longMonths;
    rich(g);
    expect(orderDecree(g, 'economy', 2)).toBe(true);
    expect(govMul(g, 'GER', 'income')).toBeCloseTo(1.12 * 0.98, 9);
    expect(whyNotOrderDecree(g, 'service', 2)).toMatch(/back to the wall/);
    g.turn += GOV.needs.pressedMonths;
    rich(g);
    expect(orderDecree(g, 'service', 2)).toBe(true);
    expect(govMul(g, 'GER', 'drafts')).toBeCloseTo(1.45, 9);
    // What the hardest decrees cost is on the card as figures: the cabinet's trust, and the political capital it takes to keep them.
    expect(government(g)!.sums).toContain('Trust −1.7 a month');
    expect(GOV.tracks.find((t) => t.id === 'economy')!.steps[2].text).toMatch(/trust falls 1\.2 a month/);
  });

  it('home ground lost opens the harder steps at once', () => {
    const g = playing('GER');
    rich(g);
    orderDecree(g, 'economy', 1);
    orderDecree(g, 'service', 1);
    g.regions.find((r) => r.owner === 'GER' && r.ctrl === 'GER' && !r.cap && !r.overseas && r.vp > 0)!.ctrl = 'POL';
    expect(whyNotOrderDecree(g, 'economy', 2)).toBeNull();
    for (const r of g.regions) if (r.owner === 'GER' && !r.overseas && r.id % 3 === 0) r.ctrl = 'POL';
    expect(whyNotOrderDecree(g, 'service', 2)).toBeNull();
  });

  it('a decree can be relaxed for nothing, but not tightened again for half a year', () => {
    const g = playing('GER');
    rich(g);
    orderDecree(g, 'economy', 1);
    const pc = g.nations.GER.pc;
    expect(whyNotOrderDecree(g, 'economy', 0)).toBeNull();
    expect(orderDecree(g, 'economy', 0)).toBe(true);
    expect(g.nations.GER.pc).toBe(pc);
    expect(govMul(g, 'GER', 'income')).toBe(1);
    expect(whyNotOrderDecree(g, 'economy', 1)).toMatch(/relaxed lately/);
    g.turn += GOV.decree.wait;
    expect(whyNotOrderDecree(g, 'economy', 1)).toBeNull();
  });
});

describe('the cabinet\'s trust', () => {
  it('begins where the form of government puts it, and with no news drifts a tenth of the way to its rest', () => {
    expect(government(free(3, 'UK'))!.trust).toBe(60);
    expect(government(free(3, 'GER'))!.trust).toBe(70);
    const g = playing('GER');
    const st = quiet(g);
    st.trust = 85;
    govMonth(g, 'GER');
    expect(st.trust).toBeCloseTo(85 - (85 - GOV.trust.rest) * GOV.trust.drift, 9);
    st.trust = 25;
    govMonth(g, 'GER');
    expect(st.trust).toBeCloseTo(25 + (GOV.trust.rest - 25) * GOV.trust.drift, 9);
  });

  it('ground won raises it and home ground lost lowers it, by the form of government, and no one cause counts for more than its cap', () => {
    for (const nat of ['GER', 'UK']) {
      const g = playing(nat);
      const st = quiet(g);
      const T = GOV.trust;
      st.trust = T.rest;
      st.mark.held -= 3;
      govMonth(g, nat);
      expect(st.trust, nat).toBeCloseTo(T.rest + 3 * T.province * swing(nat), 9);
      expect(government(g, nat)!.moved, nat).toEqual([{ label: 'Ground won', amount: Math.round(3 * T.province * swing(nat) * 10) / 10 }]);
      st.trust = T.rest;
      st.mark.home += 2;
      govMonth(g, nat);
      expect(st.trust, nat).toBeCloseTo(T.rest - 2 * T.home * swing(nat), 9);
      st.trust = T.rest;
      st.mark.held -= 40;
      govMonth(g, nat);
      expect(st.trust, nat).toBeCloseTo(T.rest + T.cap * swing(nat), 9);
      st.trust = T.rest;
      st.mark.caps -= 1;
      govMonth(g, nat);
      expect(st.trust, nat).toBeCloseTo(T.rest + T.capitalTaken * swing(nat), 9);
    }
  });

  it('battles won and lost count, and so does a treasury in debt', () => {
    const g = playing('GER');
    const st = quiet(g);
    const T = GOV.trust;
    const rep = (id: number, att: string, def: string, level: string): void => { g.reports.push({ ...(g.reports[0] ?? {}), id, attNat: att, defNat: def, level, yielded: false } as never); };
    const top = st.mark.report;
    rep(top + 1, 'GER', 'POL', 'Decisive victory');
    rep(top + 2, 'GER', 'POL', 'Close victory');
    rep(top + 3, 'POL', 'GER', 'Close defeat');
    rep(top + 4, 'GER', 'POL', 'Decisive defeat');
    rep(top + 5, 'UK', 'ITA', 'Heroic victory');
    st.trust = T.rest;
    govMonth(g, 'GER');
    expect(st.trust).toBeCloseTo(T.rest + (3 * T.win - T.defeat) * swing('GER'), 9);
    // Read once: the same battles are not news next month.
    st.trust = T.rest;
    govMonth(g, 'GER');
    expect(st.trust).toBeCloseTo(T.rest, 9);
    g.nations.GER.pp = -5;
    govMonth(g, 'GER');
    expect(st.trust).toBeCloseTo(T.rest - T.debt * swing('GER'), 9);
  });

  it('a paymaster keeps it up and a taxman wears it down', () => {
    const g = playing('GER');
    const st = quiet(g);
    st.trust = GOV.trust.rest;
    st.seats.finance = 'paymaster';
    govMonth(g, 'GER');
    // What a minister or a decree does to the trust is felt in full, whatever the form of government.
    expect(swing('GER')).not.toBe(1);
    // He sits with the doves, and a table that leans to them settles a little higher: the drift toward that is felt too.
    expect(st.trust).toBeCloseTo(GOV.trust.rest + 0.5 + GOV.wings.doves.rest * GOV.trust.drift, 9);
    st.trust = GOV.trust.rest;
    st.seats.finance = 'taxman';
    st.steps.economy = 2;
    govMonth(g, 'GER');
    expect(st.trust).toBeCloseTo(GOV.trust.rest - 1.7, 9);
    expect(government(g)!.moved).toEqual([{ label: 'Ministers and decrees', amount: -1.7 }]);
  });

  it('the money levers have a price: a war economy takes political capital to keep, and lapses when there is none', () => {
    const g = playing('SWE');
    const st = quiet(g);
    const C = (id: number): number => GOV.tracks.find((t) => t.id === 'economy')!.steps[id].effects.capital ?? 0;
    expect(C(1)).toBeLessThan(0);
    expect(C(2)).toBeLessThan(C(1));
    st.steps.economy = 2;
    expect(govCapital(g, 'SWE')).toBeCloseTo(GOV.forms[formOf('SWE')].capital + C(2), 9);
    // With capital in hand the decree stands.
    g.nations.SWE.pc = 10;
    govMonth(g, 'SWE');
    expect(st.steps.economy).toBe(2);
    // With none, and less coming in than it takes, it falls back a step and cannot be raised again for a while.
    g.nations.SWE.pc = 0;
    expect(pcIncome('SWE') + govCapital(g, 'SWE')).toBeLessThan(0);
    const h0 = g.headlines.length;
    govMonth(g, 'SWE');
    expect(st.steps.economy).toBeLessThan(2);
    expect(said(g, /The decree lapses: total mobilisation/, h0)).toBe(true);
    expect(st.wait['track:economy']).toBe(g.turn + GOV.decree.wait);
    expect(government(g)!.capitalMonth).toBeGreaterThanOrEqual(0);
  });

  it('a cabinet that doubts withholds political capital, and one that believes adds to it', () => {
    const g = playing('UK');
    const st = quiet(g);
    st.trust = 50;
    expect(govCapital(g, 'UK')).toBe(0);
    st.trust = GOV.capital.lowTrust - 1;
    expect(govCapital(g, 'UK')).toBeCloseTo(-pcIncome('UK') * GOV.capital.lowShare, 9);
    expect(government(g)!.trustWord).toBe('Doubtful');
    // It also holds back something every player has: a share of the treasury's income. The card says so.
    expect(govMul(g, 'UK', 'income')).toBeCloseTo(1 - GOV.capital.lowIncome, 9);
    expect(government(g)!.doubts).toBe(true);
    expect(government(g)!.withheld).toMatch(/8% of the treasury's income/);
    st.trust = 50;
    expect(govMul(g, 'UK', 'income')).toBe(1);
    expect(government(g)!.doubts).toBe(false);
    expect(government(g)!.withheld).toBeNull();
    st.trust = GOV.capital.lowTrust - 1;
    st.trust = GOV.capital.highTrust;
    expect(govCapital(g, 'UK')).toBe(GOV.capital.highAdd);
    expect(government(g)!.trustWord).toBe('Firm');
    expect(government(g)!.capitalMonth).toBeCloseTo(pcIncome('UK') + 1, 6);
  });

  it('a cabinet with no trust left for three months running falls: caretakers everywhere, the newest decrees withdrawn, the political capital spent', () => {
    const g = playing('UK');
    const st = quiet(g);
    st.seats = { staff: 'organiser', finance: 'taxman' };
    st.steps = { economy: 2, service: 1 };
    g.nations.UK.pc = 17;
    const h0 = g.headlines.length;
    for (let k = 0; k < GOV.trust.crisisMonths; k++) {
      expect(st.seats.staff).toBe('organiser');
      st.trust = 0;
      govMonth(g, 'UK');
      if (k < GOV.trust.crisisMonths - 1) { expect(st.low).toBe(k + 1); expect(government(g)!.trustWord).toBe('None left'); }
    }
    expect(st.seats).toEqual({});
    expect(st.steps).toEqual({ economy: 1, service: 0 });
    expect(g.nations.UK.pc).toBe(0);
    expect(st.trust).toBe(GOV.trust.afterCrisis);
    expect(st.low).toBe(0);
    expect(said(g, /The government falls: the war cabinet loses a vote of confidence/, h0)).toBe(true);
    // A month of trust in between starts the count again.
    st.trust = 0; govMonth(g, 'UK');
    st.trust = 60; govMonth(g, 'UK');
    expect(st.low).toBe(0);
  });
});

describe('directives', () => {
  it('the first comes a few months in, and is a job that fits the war', () => {
    const g = playing('GER');
    expect(government(g)!.directive).toBeNull();
    expect(government(g)!.nextIn).toBe(GOV.directives.first);
    const st = govOf(g, 'GER')!;
    st.next = g.turn;
    const h0 = g.headlines.length;
    govMonth(g, 'GER');
    const d = government(g)!.directive!;
    expect(d).not.toBeNull();
    expect(['take', 'hold']).toContain(d.kind);
    expect(d.by).toBe('The war cabinet');
    expect(d.due).toBeGreaterThan(g.turn);
    expect(d.left).toBe(d.due - g.turn + 1);
    expect(d.capital).toBeGreaterThan(0);
    expect(d.reward).toMatch(/political capital/);
    expect(d.progress.length).toBeGreaterThan(10);
    expect(d.price).toBe(Math.round(GOV.directives.missed.base * swing('GER') * 10) / 10);
    expect(d.asidePrice).toBe(Math.round(GOV.directives.missed.base * GOV.directives.missed.aside * swing('GER') * 10) / 10);
    expect(d.asidePrice).toBeLessThan(d.price);
    expect(d.asideNext).toBe(d.due + 1 + GOV.directives.every);
    const r = g.regions[d.region!];
    if (d.kind === 'take') { expect(g.nations.GER.atWar).toContain(r.ctrl); expect(d.money).toBeGreaterThan(0); expect(d.text).toMatch(/by you or an ally/); } else expect(r.ctrl).toBe('GER');
    expect(said(g, /A directive from the war cabinet/, h0)).toBe(true);
    expect(government(g)!.nextIn).toBe(0);
    expect(government(g)!.log[0].text).toMatch(/^Directive: /);
    // One at a time.
    st.next = g.turn;
    govMonth(g, 'GER');
    expect(govOf(g, 'GER')!.dir!.id).toBe(d.id);
  });

  it('a country at peace is asked to build up its army, by no more than its treasury can pay for', () => {
    const g = playing('ITA');
    const st = govOf(g, 'ITA')!;
    const inf = specOf(g, 'inf')!;
    // Italy opens with too little in hand for one more corps in the time: the cabinet does not ask for what cannot be paid for.
    st.next = g.turn;
    govMonth(g, 'ITA');
    expect(st.dir).toBeUndefined();
    expect(st.next).toBe(g.turn + GOV.directives.retry);
    // With the money for two corps and a margin, it asks for two and no more.
    g.nations.ITA.pp = (corpsCost(g, 'ITA', 'inf') * 2.5) / 0.6;
    st.next = g.turn;
    govMonth(g, 'ITA');
    const d = st.dir!;
    expect(d.kind).toBe('raise');
    const now = armyCount(g, 'ITA');
    expect(d.target).toBe(now + 2 * inf.divs);
    expect(d.title).toBe('Build up the army');
    expect(d.text).toMatch(/While the country is at peace, the war cabinet wants the army built up/);
    expect(government(g)!.directive!.progress).toBe(`The army stands at ${now} of ${d.target} divisions, in the field or forming.`);
    // The army is counted as the player sees it: every division under arms, and a corps still forming counts the day it is ordered.
    g.nations.ITA.queue.push({ id: g.nextBuild++, what: 'corps', kind: 'inf', divs: inf.divs, name: 'A new corps', region: g.nations.ITA.capital, ordered: g.turn, eta: g.turn + 5, cost: 1 });
    expect(armyCount(g, 'ITA')).toBe(now + inf.divs);
    judgeDirective(g, 'ITA');
    expect(st.dir, 'one corps is not two').toBeDefined();
    g.nations.ITA.queue.push({ id: g.nextBuild++, what: 'corps', kind: 'inf', divs: inf.divs, name: 'Another corps', region: g.nations.ITA.capital, ordered: g.turn, eta: g.turn + 5, cost: 1 });
    const pc = g.nations.ITA.pc;
    judgeDirective(g, 'ITA');
    expect(st.dir).toBeUndefined();
    expect(st.done).toBe(1);
    expect(g.nations.ITA.pc).toBe(Math.min(DIPLOMACY.pcCap, pc + GOV.directives.raise.capital));
  });

  it('the cabinet asks only for towns the troops within reach can take, and marks a hard one', () => {
    const T = GOV.directives.take;
    let takes = 0;
    for (const seed of [3, 5, 7, 11]) {
      const g = playing('GER', seed);
      const st = govOf(g, 'GER')!;
      for (let k = 0; k < 3; k++) {
        delete st.dir;
        delete st.lastKind;
        st.next = g.turn;
        govMonth(g, 'GER');
        const d = govOf(g, 'GER')!.dir;
        if (d?.kind === 'take') {
          takes++;
          const odds = directiveOdds(g, 'GER', d.region!);
          expect(odds, d.title).toBeGreaterThanOrEqual(T.least);
          expect(!!d.hard, d.title).toBe(odds < T.odds);
          if (d.hard) expect(d.text).toMatch(/strongly held/);
        }
        g.turn++;
      }
    }
    expect(takes).toBeGreaterThan(0);
    // The odds are the player's troops in and beside the province, and half of those two provinces off, over the defenders with their walls counted.
    const g = playing('GER');
    const warsaw = g.byName['Warsaw'];
    const before = directiveOdds(g, 'GER', warsaw.id);
    for (const c of g.corps) if (c.nat === 'GER' && !c.dead) c.str /= 2;
    expect(directiveOdds(g, 'GER', warsaw.id)).toBeCloseTo(before / 2, 6);
  });

  it('a town asked for and not given is not asked for again within a year while another will do', () => {
    const g = playing('GER');
    const st = govOf(g, 'GER')!;
    const first = (): Directive => { delete st.dir; st.lastKind = 'hold'; st.next = g.turn; govMonth(g, 'GER'); return st.dir!; };
    let d = first();
    for (let k = 0; k < 8 && d.kind !== 'take'; k++) { g.turn++; d = first(); }
    expect(d.kind).toBe('take');
    const town = d.region!;
    g.turn = d.due + 1;
    judgeDirective(g, 'GER');
    expect(st.missed).toBe(1);
    expect(st.asked![town]).toBe(g.turn);
    // Asked again at once, the cabinet names another town, or says plainly that it is asking again.
    for (let k = 0; k < 6; k++) {
      const next = first();
      if (next.kind === 'take' && next.region === town) { expect(next.again).toBe(true); expect(next.text).toMatch(/asks again/); }
      g.turn++;
    }
    // Carried out, the town is forgotten.
    st.dir = job(g, 'GER', { region: town });
    g.regions[town].ctrl = 'GER';
    judgeDirective(g, 'GER');
    expect(st.asked![town]).toBeUndefined();
  });

  it('a town taken in time earns political capital, money and the cabinet\'s trust', () => {
    const g = playing('GER');
    const st = quiet(g);
    const warsaw = g.byName['Warsaw'];
    st.dir = job(g, 'GER', { region: warsaw.id, capital: 6, money: 2.5 });
    st.trust = 50;
    g.nations.GER.pc = 3;
    const pp = g.nations.GER.pp;
    judgeDirective(g, 'GER');
    expect(st.dir).toBeDefined();
    expect(government(g)!.directive!.progress).toMatch(/Warsaw is held by Poland/);
    warsaw.ctrl = 'GER';
    const h0 = g.headlines.length;
    judgeDirective(g, 'GER');
    expect(st.dir).toBeUndefined();
    expect(g.nations.GER.pc).toBe(9);
    expect(g.nations.GER.pp).toBeCloseTo(pp + 2.5, 6);
    expect(st.trust).toBeCloseTo(50 + GOV.directives.success.trust * swing('GER'), 9);
    expect(st.done).toBe(1);
    expect(st.next).toBe(g.turn + GOV.directives.every);
    expect(said(g, /Directive carried out: Take the town/, h0)).toBe(true);
    expect(government(g)!.moved.find((c) => c.label === 'A directive carried out')!.amount).toBe(GOV.directives.success.trust * swing('GER'));
  });

  it('the directive is looked at as the player\'s month closes, so a job done in it is rewarded at once', () => {
    const g = playing('GER');
    const st = quiet(g);
    st.dir = job(g, 'GER', { kind: 'raise', target: 1, money: 0, title: 'Bring the army up to strength' });
    const h0 = g.headlines.length;
    endHuman(g);
    expect(st.dir).toBeUndefined();
    expect(st.done).toBe(1);
    expect(said(g, /Directive carried out: Bring the army up to strength/, h0)).toBe(true);
  });

  it('an ally taking the town counts, and the reward never lifts political capital past the most a nation can hold', () => {
    const g = playing('GER');
    const st = quiet(g);
    const warsaw = g.byName['Warsaw'];
    st.dir = job(g, 'GER', { region: warsaw.id, capital: 10 });
    g.nations.GER.pc = DIPLOMACY.pcCap - 4;
    warsaw.ctrl = 'ITA';
    judgeDirective(g, 'GER');
    expect(st.done).toBe(1);
    expect(g.nations.GER.pc).toBe(DIPLOMACY.pcCap);
  });

  it('a job not done by its last month costs trust, more each time in a row, and lowers the level trust settles at', () => {
    const g = playing('GER');
    const st = quiet(g);
    const M = GOV.directives.missed;
    const warsaw = g.byName['Warsaw'];
    let rest = GOV.trust.rest;
    const costs: number[] = [];
    for (let k = 0; k < 7; k++) {
      st.dir = job(g, 'GER', { region: warsaw.id, due: g.turn });
      st.trust = 60;
      judgeDirective(g, 'GER');
      expect(st.dir, 'open in its last month').toBeDefined();
      g.turn++;
      const h0 = g.headlines.length;
      judgeDirective(g, 'GER');
      expect(st.dir).toBeUndefined();
      const cost = Math.min(M.most, M.base + M.step * k);
      costs.push(cost);
      expect(st.trust).toBeCloseTo(60 - cost * swing('GER'), 9);
      expect(st.missed).toBe(k + 1);
      expect(said(g, /The directive was not carried out/, h0)).toBe(true);
      rest = GOV.trust.rest - Math.min(M.restMost, M.rest * (k + 1));
      expect(government(g)!.rest).toBe(rest);
    }
    expect(costs.slice(0, 5)).toEqual([3, 5, 7, 9, 11]);
    expect(rest).toBe(GOV.trust.rest - M.restMost);
    // Nothing holds the settling level up any more: enough ignored in a row and trust settles where any cabinet falls.
    for (const f of Object.values(GOV.forms)) expect(GOV.trust.rest - M.restMost).toBeLessThan(f.fallsAt);
    // With no news, trust now drifts to the lower level.
    st.next = g.turn + 99;
    st.trust = rest + 10;
    govMonth(g, 'GER');
    expect(st.trust).toBeCloseTo(rest + 9, 9);
    // One job done wipes the slate.
    st.dir = job(g, 'GER', { region: warsaw.id });
    warsaw.ctrl = 'GER';
    judgeDirective(g, 'GER');
    expect(st.missed).toBe(0);
    expect(government(g)!.rest).toBe(GOV.trust.rest);
  });

  it('a town that passes to a country at peace with the player voids the job: nothing won, nothing lost', () => {
    const g = playing('GER');
    const st = quiet(g);
    const warsaw = g.byName['Warsaw'];
    st.dir = job(g, 'GER', { region: warsaw.id });
    st.trust = 60;
    const pc = g.nations.GER.pc;
    warsaw.ctrl = 'SOV';
    expect(g.nations.GER.atWar).not.toContain('SOV');
    const h0 = g.headlines.length;
    judgeDirective(g, 'GER');
    expect(st.dir).toBeUndefined();
    expect(st.trust).toBe(60);
    expect(g.nations.GER.pc).toBe(pc);
    expect(st.done).toBe(0);
    expect(st.missed).toBe(0);
    expect(st.next).toBe(g.turn + GOV.directives.retry);
    expect(said(g, /The directive is withdrawn: Take the town/, h0)).toBe(true);
  });

  it('an order to hold counts only if the enemy came: untested it is withdrawn, attacked and held it is carried out, lost it fails', () => {
    const g = playing('GER');
    const st = quiet(g);
    const town = g.regions.find((r) => r.owner === 'GER' && r.ctrl === 'GER' && !r.cap && !r.overseas && r.units.every((u) => u.nat === 'GER'))!;
    const last = (): number => (g.reports.length ? g.reports[g.reports.length - 1].id : -1);
    // Nobody comes: when its time is up the order is withdrawn, with no reward and no blame.
    st.dir = job(g, 'GER', { kind: 'hold', region: town.id, due: g.turn + 2, money: 0, title: 'Hold the town', seen: last() });
    st.trust = 60;
    const pc = g.nations.GER.pc;
    judgeDirective(g, 'GER');
    expect(st.dir).toBeDefined();
    expect(government(g)!.directive!.progress).toMatch(/in your hands, and the enemy has not come for it yet/);
    g.turn += 2;
    judgeDirective(g, 'GER');
    expect(st.dir, 'still open in its last month').toBeDefined();
    g.turn++;
    let h0 = g.headlines.length;
    judgeDirective(g, 'GER');
    expect(st.dir).toBeUndefined();
    expect(st.done).toBe(0);
    expect(st.missed).toBe(0);
    expect(st.trust).toBe(60);
    expect(g.nations.GER.pc).toBe(pc);
    expect(st.next).toBe(g.turn + GOV.directives.retry);
    expect(said(g, /The directive is withdrawn: Hold the town\. The enemy did not come for it/, h0)).toBe(true);
    // The enemy attacks it and is thrown back: the order has been tested, and at the end it is carried out.
    st.dir = job(g, 'GER', { kind: 'hold', region: town.id, due: g.turn + 2, money: 0, title: 'Hold the town', seen: last() });
    g.reports.push({ id: last() + 1, turn: g.turn, region: town.id, attNat: 'POL', defNat: 'GER', level: 'defeat' } as unknown as BattleReport);
    judgeDirective(g, 'GER');
    expect(st.dir!.tested).toBe(true);
    expect(government(g)!.directive!.progress).toMatch(/has been attacked, and holds/);
    g.turn += 3;
    h0 = g.headlines.length;
    judgeDirective(g, 'GER');
    expect(st.done).toBe(1);
    expect(said(g, /Directive carried out: Hold the town/, h0)).toBe(true);
    // Lost, it fails that month.
    st.dir = job(g, 'GER', { kind: 'hold', region: town.id, due: g.turn + 2, title: 'Hold the town' });
    town.ctrl = 'POL';
    judgeDirective(g, 'GER');
    expect(st.dir).toBeUndefined();
    expect(st.missed).toBe(1);
  });

  it('an ally hard pressed asks for a blow at its enemy, and pays with a corps', () => {
    const g = playing('UK');
    // Germany overruns the north-east of France, up to the British lines.
    const bef = g.corps.find((c) => c.nat === 'UK' && !c.dead && g.regions[c.reg].owner === 'FRA')!;
    expect(bef).toBeDefined();
    const paris = g.regions[g.nations.FRA.capital];
    // The troops that stood there are gone: a town an ally's army still stands round is about to fall, and is not asked for.
    const empty = (r: typeof paris): void => { for (const u of r.units) u.dead = true; r.units = []; };
    const near = g.regions[bef.reg].nb.map((j) => g.regions[j]).find((r) => r.owner === 'FRA' && r.id !== paris.id)!;
    near.ctrl = 'GER'; empty(near);
    for (const r of g.regions) if (r.owner === 'FRA' && !r.overseas && r.id !== paris.id && r.id !== bef.reg && r.id % 2 === 0) { r.ctrl = 'GER'; empty(r); }
    const st = govOf(g, 'UK')!;
    delete st.dir;
    st.lastKind = 'take';
    st.next = g.turn;
    const h0 = g.headlines.length;
    govMonth(g, 'UK');
    const d = st.dir!;
    expect(d.kind).toBe('relief');
    expect(d.from).toBe('FRA');
    expect(d.corps).toBe('inf');
    expect(g.regions[d.region!].ctrl).toBe('GER');
    expect(d.title).toBe('Take the pressure off France');
    expect(d.text).toMatch(/France is hard pressed by Germany and asks you to strike back/);
    expect(directiveOdds(g, 'UK', d.region!)).toBeGreaterThanOrEqual(GOV.directives.take.least);
    expect(government(g)!.directive!.by).toBe('France');
    expect(government(g)!.directive!.reward).toMatch(/a new infantry corps, armed by your ally/);
    expect(said(g, /A directive from France/, h0)).toBe(true);
    // Carried out: the corps is on the way, and costs nothing.
    g.regions[d.region!].ctrl = 'UK';
    const q = g.nations.UK.queue.length;
    judgeDirective(g, 'UK');
    expect(st.done).toBe(1);
    const b = g.nations.UK.queue[q];
    expect(b.what).toBe('corps');
    expect(b.kind).toBe('inf');
    expect(b.cost).toBe(0);
    expect(b.eta).toBe(g.turn + 1);
    expect(b.region).toBe(g.nations.UK.capital);
  });

  it('not the same kind of job twice running, where there is a choice', () => {
    const g = playing('GER');
    const st = govOf(g, 'GER')!;
    const kinds = new Set<string>();
    for (let k = 0; k < 6; k++) {
      delete st.dir;
      st.next = g.turn;
      const last = st.lastKind;
      govMonth(g, 'GER');
      expect(st.dir).toBeDefined();
      kinds.add(st.dir!.kind);
      // Germany, at war with Poland, always has a town to take; so after any other job she is not asked for the same again.
      if (last && last !== 'take') expect(st.dir!.kind).not.toBe(last);
      setAside(g, 'GER');
      g.turn++;
    }
    expect(kinds.has('take')).toBe(true);
  });

  it('a directive can be set aside: the trust is lost at once and the next is set in its turn', () => {
    const g = playing('GER');
    const st = quiet(g);
    expect(whyNotSetAsideDirective(g)).toMatch(/no directive/);
    expect(orderSetAside(g)).toBe(false);
    const d = st.dir = job(g, 'GER', { region: g.byName['Warsaw'].id });
    st.trust = 60;
    expect(whyNotSetAsideDirective(g)).toBeNull();
    expect(government(g)!.directive!.asideNext).toBe(d.due + 1 + GOV.directives.every);
    const h0 = g.headlines.length;
    expect(orderSetAside(g)).toBe(true);
    expect(st.dir).toBeUndefined();
    // Declining costs half of what failing would, and the next directive comes no sooner than it would have.
    expect(GOV.directives.missed.aside).toBe(0.5);
    expect(st.trust).toBeCloseTo(60 - GOV.directives.missed.base * GOV.directives.missed.aside * swing('GER'), 9);
    expect(st.missed).toBe(1);
    expect(st.next).toBe(d.due + 1 + GOV.directives.every);
    expect(said(g, /You set the directive aside/, h0)).toBe(true);
    expect(government(g)!.nextIn).toBe(st.next - g.turn);
  });

  it('a paymaster halves what a directive missed costs', () => {
    const g = playing('GER');
    const st = quiet(g);
    st.seats.finance = 'paymaster';
    st.dir = job(g, 'GER', { region: g.byName['Warsaw'].id, due: g.turn - 1 });
    st.trust = 60;
    judgeDirective(g, 'GER');
    expect(st.trust).toBeCloseTo(60 - GOV.directives.missed.base * 0.5 * swing('GER'), 9);
  });

  it('half a year of play: the cabinet sets its first directive on time, and nothing runs out of bounds', { timeout: 60_000 }, () => {
    const g = playing('GER', 7);
    const first = govOf(g, 'GER')!.next;
    let set = 0;
    for (let k = 0; k < 6 && g.nations.GER.alive; k++) {
      nextMonth(g);
      const v = government(g)!;
      expect(v.trust).toBeGreaterThanOrEqual(0);
      expect(v.trust).toBeLessThanOrEqual(100);
      expect(v.capital).toBeLessThanOrEqual(DIPLOMACY.pcCap);
      expect(v.log.length).toBeLessThanOrEqual(12);
      if (g.turn < first) expect(v.directive).toBeNull();
      if (g.turn === first) expect(v.directive).not.toBeNull();
      if (v.directive) set++;
      for (const n of Object.values(g.nations)) if (n.id !== 'GER') expect(n.gov, n.id).toBeUndefined();
    }
    expect(set).toBeGreaterThan(0);
  });
});

describe('what the playtest of Version 57 changed', () => {
  it('a small country can use its cabinet: an appointment costs it less, and every government opens with the capital for one', () => {
    expect(appointCost('GER')).toBe(GOV.appoint.cost);
    expect(appointCost('POL')).toBe(GOV.appoint.costMinor);
    expect(GOV.appoint.costMinor).toBeLessThan(GOV.appoint.cost);
    for (const nat of ['POL', 'BEL', 'SWE', 'GER']) {
      const g = free(3, nat);
      expect(g.nations[nat].pc, nat).toBeGreaterThanOrEqual(GOV.appoint.start);
      expect(government(g, nat)!.appointCost, nat).toBe(appointCost(nat));
      expect(government(g, nat)!.since, nat).toBe(g.turn);
    }
    const g = playing('POL');
    const pc = g.nations.POL.pc;
    expect(whyNotAppointMinister(g, 'finance', 'paymaster')).toBeNull();
    expect(orderAppoint(g, 'finance', 'paymaster')).toBe(true);
    expect(g.nations.POL.pc).toBe(pc - GOV.appoint.costMinor);
  });

  it('every price on the card is a figure', () => {
    for (const m of GOV.ministers) if (m.effects.trust) expect(`${m.plus} ${m.minus}`, m.id).toMatch(/\d(\.\d)? a month/);
    for (const t of GOV.tracks) for (const s of t.steps) if (s.effects.trust || s.effects.capital) expect(s.text, s.name).toMatch(/\d(\.\d)? (a month|political capital a month)|half a point/);
  });

  it('an idle cabinet falls in time to matter: five directives ignored in a row bring any government down', () => {
    for (const nat of ['GER', 'UK', 'ITA']) {
      const g = playing(nat);
      const st = quiet(g, nat);
      let fell = -1;
      const h0 = g.headlines.length;
      for (let k = 0; k < 6 && fell < 0; k++) {
        // A directive with five months to run, ignored to its end, then the usual wait for the next, and no other news.
        st.dir = job(g, nat, { kind: 'raise', target: 9999, due: g.turn + 4, money: 0 });
        for (let m = 0; m < 5 + GOV.directives.every && fell < 0; m++) { g.turn++; st.next = g.turn + 99; govMonth(g, nat); if (said(g, /The government falls/, h0)) fell = st.missed; }
      }
      expect(fell, nat).toBeGreaterThan(2);
      expect(fell, nat).toBeLessThanOrEqual(5);
    }
  });
});

describe('the war cabinet can be switched off', () => {
  it('off: no government, no lever, no directive, and orders are refused', () => {
    const g = playing('GER');
    const st = quiet(g);
    rich(g);
    expect(orderAppoint(g, 'finance', 'taxman')).toBe(true);
    expect(orderDecree(g, 'economy', 1)).toBe(true);
    st.dir = job(g, 'GER', { region: g.byName['Warsaw'].id });
    st.trust = 20;
    expect(govMul(g, 'GER', 'income')).not.toBe(1);
    expect(cabinetIsOff(g)).toBe(false);
    expect(orderCabinet(g, false)).toBe(true);
    expect(orderCabinet(g, false)).toBe(false);
    expect(cabinetIsOff(g)).toBe(true);
    expect(cabinetOff(g)).toBe(true);
    expect(govRules(g)).toBe(false);
    expect(governmentOf(g)).toBeNull();
    expect(governmentOf(g, 'UK')).toBeNull();
    for (const lever of ['income', 'drafts', 'cost', 'research', 'trade', 'men'] as const) expect(govMul(g, 'GER', lever), lever).toBe(1);
    expect(govCapital(g, 'GER')).toBe(0);
    // The directive that stood is withdrawn without blame, and no month moves the trust.
    expect(st.dir).toBeUndefined();
    expect(st.missed).toBe(0);
    govMonth(g, 'GER');
    judgeDirective(g, 'GER');
    expect(st.trust).toBe(20);
    expect(orderAppoint(g, 'staff', 'organiser')).toBe(false);
    expect(orderDecree(g, 'service', 1)).toBe(false);
    expect(orderSetAside(g)).toBe(false);
    // A save keeps the switch.
    expect(cabinetIsOff(copy(g))).toBe(true);
  });

  it('months pass with it off, and nothing of the cabinet stirs', { timeout: 60_000 }, () => {
    const g = playing('GER', 7);
    orderCabinet(g, false);
    const h0 = g.headlines.length;
    for (let k = 0; k < 5; k++) nextMonth(g);
    expect(g.nations.GER.gov?.dir).toBeUndefined();
    expect(said(g, /directive|war cabinet/i, h0)).toBe(false);
    expect(g.nations.GER.pc).toBeLessThanOrEqual(DIPLOMACY.pcCap);
  });

  it('on again: the cabinet sits as it was left, reads the news from that day and sets its next directive some months on', () => {
    const g = playing('GER');
    const st = quiet(g);
    rich(g);
    orderAppoint(g, 'finance', 'taxman');
    st.trust = 62;
    orderCabinet(g, false);
    // Ground changes hands while the cabinet is away.
    for (const r of g.regions) if (r.owner === 'POL' && r.id % 2 === 0) r.ctrl = 'GER';
    g.turn += 6;
    expect(orderCabinet(g, true)).toBe(true);
    expect(cabinetIsOff(g)).toBe(false);
    const v = governmentOf(g)!;
    expect(v.mine).toBe(true);
    expect(v.seats.find((s) => s.id === 'finance')!.minister.id).toBe('taxman');
    expect(v.trust).toBe(62);
    expect(v.directive).toBeNull();
    expect(st.next).toBeGreaterThanOrEqual(g.turn + GOV.directives.first);
    // What was won while it was away is old news: the first month back moves the trust only by what a month always does.
    st.next = g.turn + 99;
    govMonth(g, 'GER');
    expect(government(g)!.moved.some((c) => c.label === 'Ground won')).toBe(false);
  });

  it('a campaign begun without a cabinet has none until it is switched on', () => {
    const g = free(3, 'UK');
    expect(orderCabinet(g, false)).toBe(true);
    for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
    beginHuman(g);
    expect(governmentOf(g)).toBeNull();
    expect(orderCabinet(g, true)).toBe(true);
    const v = governmentOf(g)!;
    expect(v.mine).toBe(true);
    expect(v.seats.every((s) => s.minister.id === 'caretaker')).toBe(true);
    // The historical scenarios have no switch to throw.
    const h = createGame(1, 'GER');
    expect(orderCabinet(h, false)).toBe(false);
    expect(setCabinet(h, true)).toBe(false);
  });
});
