// The cabinet talks back (the grand campaign, Version 60). Hawks and doves: every minister sits with one wing, and
// the lean of the table shapes what the cabinet asks for and what it is hard on. Call your shot: the player goes to
// the cabinet with a promise of his own, and the odds on the day set what keeping it earns and breaking it costs.
// The cabinet wants a head: after defeats it asks for a general's dismissal, and a general the player stands by can
// be vindicated. All of it lives behind the cabinet switch, none of it touches a nation the computer plays, and the
// historical scenarios have none. The numbers are in data/campaign/government.json.
import { describe, expect, it } from 'vitest';
import { COMMAND, cmdAttack, cmdDefence, corpsOf, popular } from '../src/campaign/commanders';
import { DIPLOMACY } from '../src/campaign/diplomacy';
import { allied, createGame } from '../src/campaign/game';
import {
  GOV, appoint, formOf, govMonth, govOf, government, headOf, judgeDirective, promise, promiseTargets, promiseTerms, setCabinet, standBy, takeBack, whyNotPromise, whyNotStandBy, whyNotTakeBack,
} from '../src/campaign/government';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { townName } from '../src/campaign/towns';
import {
  beginHuman, governmentOf, headWantedOf, humanUp, orderCabinet, orderPromise, orderSack, orderStandBy, orderTakeBack, promiseTargetsOf, promiseTermsOf, stepAI, whyNotOrderPromise, whyNotStandByGeneral,
  whyNotTakeBackPromise,
} from '../src/campaign/turn';
import { MEN_PER_DIV, type BattleReport, type Directive, type Game, type General, type GovState, type Region } from '../src/campaign/types';

const free = (seed: number, human: string | null = null): Game => startOpen(createGame(seed, human));
/** A grand campaign with the player's first month open. */
function playing(human = 'GER', seed = 3): Game {
  const g = free(seed, human);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
/** The player's government with nothing due: no directive on the way, and the cabinet has read all the news there is. */
function quiet(g: Game, nat = g.human!): GovState {
  const st = govOf(g, nat)!;
  delete st.dir;
  st.next = g.turn + 99;
  st.headNext = g.turn + 99;
  govMonth(g, nat);
  return st;
}
const copy = (g: Game): Game => loadGame(saveGame(g))!;
const swing = (nat: string): number => GOV.forms[formOf(nat)].swing;
const said = (g: Game, re: RegExp, from = 0): boolean => g.headlines.slice(from).some((h) => re.test(h.text));
const HAWKS = { staff: 'organiser', arms: 'industrialist', foreign: 'statesman', finance: 'taxman' };
const DOVES = { staff: 'reformer', arms: 'planner', foreign: 'merchant', finance: 'paymaster' };
/** What a month's news does to the trust of a cabinet that sat at its rest, with these ministers in their seats. */
function month(g0: Game, seats: Record<string, string>, news: (g: Game, st: GovState) => void): { trust: number; moved: Record<string, number> } {
  const g = copy(g0), st = govOf(g, 'GER')!;
  st.seats = { ...seats };
  st.trust = government(g)!.rest;
  news(g, st);
  govMonth(g, 'GER');
  return { trust: st.trust, moved: Object.fromEntries((st.last ?? []).map((c) => [c.label, c.amount])) };
}
const lastId = (g: Game): number => (g.reports.length ? g.reports[g.reports.length - 1].id : -1);
/** A battle the player's troops fought, as the cabinet reads it. */
function battle(g: Game, o: { won: boolean; lost?: number; attacking?: boolean }): void {
  const us = { name: 'A corps', nat: 'GER', kind: 'inf', divs: 3, men: 45000, lost: o.lost ?? 0, fate: '' };
  const attacking = o.attacking ?? true;
  g.reports.push({
    id: lastId(g) + 1, turn: g.turn, region: 0, attNat: attacking ? 'GER' : 'POL', defNat: attacking ? 'POL' : 'GER',
    attackers: attacking ? [us] : [], defenders: attacking ? [] : [us], level: (attacking ? o.won : !o.won) ? 'Marginal victory' : 'Defeat',
  } as unknown as BattleReport);
}
/** An enemy town beside the player's troops, and one further off. */
function towns(g: Game): { near: Region; deep: Region } {
  const t = promiseTargets(g, 'GER', 40);
  const near = t.find((x) => x.depth === 1 && x.holder === 'POL')!, deep = t.find((x) => x.depth >= 2 && x.holder === 'POL') ?? t.find((x) => x.depth >= 2)!;
  return { near: g.regions[near.region], deep: g.regions[deep.region] };
}
/** A general of the player's with a corps, and the record of a man who has just lost `streak` battles running. */
function loser(g: Game, streak = 2, skip: number[] = []): General {
  const gen = g.generals.find((x) => x.nat === 'GER' && x.status === 'active' && x.corps >= 0 && !skip.includes(x.id))!;
  gen.rec.battles += streak;
  gen.rec.streak = streak;
  return gen;
}
/** Lets the months pass, with nothing else due, until the cabinet names a general or the months run out. */
function untilAsked(g: Game, st: GovState, most = 24): boolean {
  st.headNext = 0;
  for (let k = 0; k < most && !st.head; k++) { g.turn++; st.next = g.turn + 99; govMonth(g, 'GER'); }
  return !!st.head;
}

describe('the cabinet talks back only in the grand campaign, and only to the player', () => {
  it('the historical scenarios have none of it, and no general earns the mark by his record', () => {
    const g = createGame(3, 'GER');
    expect(governmentOf(g)).toBeNull();
    expect(promiseTargetsOf(g)).toEqual([]);
    expect(whyNotOrderPromise(g)).toMatch(/grand campaign|not your month/);
    expect(headWantedOf(g)).toBeNull();
    expect(COMMAND.traits.vindicated.needs).toBeUndefined();
    expect(COMMAND.traits.vindicated.good).toBe(true);
  });

  it('a nation the computer plays has no wings, makes no promises and is asked for no heads', () => {
    const g = playing('GER');
    const v = government(g, 'UK')!;
    expect(v.mine).toBe(false);
    expect(v.wings.lean).toBe(0);
    expect(v.promise).toBeNull();
    expect(v.head).toBeNull();
    expect(promiseTargets(g, 'UK')).toEqual([]);
    expect(promise(g, 'UK', 0, 3)).toBe(false);
    expect(standBy(g, 'UK')).toBe(false);
  });
});

describe('hawks and doves', () => {
  it('every minister but a caretaker sits with a wing, and every office offers one of each', () => {
    for (const s of GOV.seats) {
      const mine = GOV.ministers.filter((m) => m.seat === s.id);
      expect(mine.map((m) => m.wing).sort(), s.id).toEqual(['dove', 'hawk']);
    }
    expect(GOV.ministers.find((m) => m.id === 'caretaker')!.wing).toBeUndefined();
    const g = playing('GER');
    const v = governmentOf(g)!;
    expect(v.wings).toMatchObject({ hawks: 0, doves: 0, lean: 0, top: 'balanced' });
    expect(v.wings.effects).toEqual([]);
    expect(v.seats[0].options.filter((o) => o.id !== 'caretaker').map((o) => o.wing).sort()).toEqual(['dove', 'hawk']);
    expect(v.seats[0].minister.wing).toBeNull();
  });

  it('two more of one wing than the other and it has the table, which makes the news; one fewer and the table balances again', () => {
    const g = playing('GER');
    g.nations.GER.pc = DIPLOMACY.pcCap;
    const h = g.headlines.length;
    expect(appoint(g, 'GER', 'staff', 'organiser')).toBe(true);
    expect(governmentOf(g)!.wings).toMatchObject({ hawks: 1, lean: 1, top: 'balanced', word: 'Leaning to the hawks' });
    expect(said(g, /hawks now have the better/, h)).toBe(false);
    expect(said(g, /He sits with the hawks/, h)).toBe(true);
    expect(appoint(g, 'GER', 'arms', 'industrialist')).toBe(true);
    const v = governmentOf(g)!;
    expect(v.wings).toMatchObject({ hawks: 2, doves: 0, lean: 2, top: 'hawks' });
    expect(v.wings.effects).toHaveLength(3);
    expect(said(g, /hawks now have the better of your war cabinet/, h)).toBe(true);
    // A dove in a third office pulls the table back to the middle.
    const h2 = g.headlines.length;
    expect(appoint(g, 'GER', 'foreign', 'merchant')).toBe(true);
    expect(governmentOf(g)!.wings).toMatchObject({ hawks: 2, doves: 1, lean: 1, top: 'balanced' });
    expect(said(g, /Neither wing has the better/, h2)).toBe(true);
    // The shift is told once, not every month.
    const h3 = g.headlines.length;
    quiet(g);
    expect(said(g, /Neither wing|have the better/, h3)).toBe(false);
  });

  it('hawks make more of ground and battles won, and take ground lost harder', () => {
    const g = playing('GER');
    quiet(g);
    const W = GOV.wings.hawks, T = GOV.trust;
    // The hawks without a price in trust of their own: the organiser and the industrialist.
    const two = { staff: 'organiser', arms: 'industrialist' };
    const won = (seats: Record<string, string>): number => month(g, seats, (_x, st) => { st.mark.held -= 2; }).moved['Ground won'];
    expect(won({})).toBeCloseTo(2 * T.province * swing('GER'), 9);
    expect(won(two)).toBeCloseTo(2 * T.province * (1 + 2 * W.won) * swing('GER'), 9);
    const lost = (seats: Record<string, string>): number => month(g, seats, (_x, st) => { st.mark.home += 2; }).moved['Home ground lost'];
    expect(lost({})).toBeCloseTo(-2 * T.home * swing('GER'), 9);
    expect(lost(two)).toBeCloseTo(-2 * T.home * (1 + 2 * W.lost) * swing('GER'), 9);
    const wins = (seats: Record<string, string>): number => month(g, seats, (x) => { battle(x, { won: true }); battle(x, { won: true }); }).moved['Battles won'];
    expect(wins(two)).toBeCloseTo(wins({}) * (1 + 2 * W.wins), 9);
    // A battle lost counts the same with them as with a balanced table, and they do not count the dead.
    const beaten = (seats: Record<string, string>): Record<string, number> => month(g, seats, (x) => battle(x, { won: false, lost: 400000 })).moved;
    expect(beaten(two)['Battles lost']).toBeCloseTo(beaten({})['Battles lost'], 9);
    expect(beaten(two)['Heavy losses']).toBeUndefined();
  });

  it('doves take battles lost and heavy losses hard, and their table settles higher while nothing stands ignored', () => {
    const g = playing('GER');
    const st = quiet(g);
    const W = GOV.wings.doves;
    const two = { staff: 'reformer', arms: 'planner' };
    const beaten = (seats: Record<string, string>, lost: number): Record<string, number> => month(g, seats, (x) => battle(x, { won: false, lost })).moved;
    expect(beaten(two, 0)['Battles lost']).toBeCloseTo(beaten({}, 0)['Battles lost'] * (1 + 2 * W.defeats), 9);
    // Losses within what the doves will stand for cost nothing; beyond it, so much a hundredth of the army.
    const army = g.corps.reduce((s, c) => s + (c.nat === 'GER' && !c.dead && c.kind !== 'fort' ? c.divs * c.str * MEN_PER_DIV : 0), 0);
    expect(beaten(two, Math.round(army * 0.01))['Heavy losses']).toBeUndefined();
    const heavy = Math.round(army * 0.06), share = heavy / (army + heavy);
    expect(beaten(two, heavy)['Heavy losses']).toBeCloseTo(-2 * W.losses * (share - W.spare) * 100 * swing('GER'), 6);
    expect(beaten({}, heavy)['Heavy losses']).toBeUndefined();
    // No single cause moves the trust without limit: the worst month of losses is capped like any other news.
    expect(beaten(DOVES, army * 4)['Heavy losses']).toBeCloseTo(-GOV.trust.cap * swing('GER'), 9);
    // The level trust settles at.
    st.seats = { ...two };
    expect(government(g)!.rest).toBe(GOV.trust.rest + 2 * W.rest);
    st.seats = { ...DOVES };
    expect(government(g)!.rest).toBe(GOV.trust.rest + 4 * W.rest);
    st.missed = 1;
    expect(government(g)!.rest).toBe(GOV.trust.rest - GOV.directives.missed.rest);
    st.missed = 0;
    st.seats = { ...HAWKS };
    expect(government(g)!.rest).toBe(GOV.trust.rest);
  });

  /** The kinds of directive a cabinet sets over a run of wars and months, with these ministers at the table. */
  function kinds(seats: Record<string, string>, set?: (g: Game) => void): Record<string, number> {
    const out: Record<string, number> = {};
    for (const seed of [3, 5, 7, 11]) {
      const g = playing('GER', seed);
      const st = govOf(g, 'GER')!;
      st.seats = { ...seats };
      st.headNext = 999;
      set?.(g);
      for (let k = 0; k < 6; k++) {
        delete st.dir;
        st.next = g.turn;
        govMonth(g, 'GER');
        const d = govOf(g, 'GER')!.dir;
        out[d?.kind ?? 'none'] = (out[d?.kind ?? 'none'] ?? 0) + 1;
        if (d) st.lastKind = d.kind;
        g.turn++;
      }
    }
    return out;
  }

  it('with the hawks on top the cabinet asks for a town every time there is one to ask for', () => {
    const k = kinds(HAWKS);
    expect(k.take ?? 0).toBeGreaterThan(0);
    expect(Object.keys(k).filter((x) => x !== 'take' && x !== 'relief')).toEqual([]);
    // A balanced table asks for other things in between.
    const b = kinds({});
    expect(Object.keys(b).some((x) => x !== 'take' && x !== 'relief')).toBe(true);
  });

  it('with the doves on top it asks for a town only when nothing else fits', () => {
    // A country with money in hand can always be asked to build up its army: the doves never need to ask for a town.
    const k = kinds(DOVES, (g) => { g.nations.GER.pp = 500; });
    expect(k.take ?? 0).toBe(0);
    expect((k.raise ?? 0) + (k.hold ?? 0) + (k.spare ?? 0) + (k.trade ?? 0)).toBeGreaterThan(0);
  });

  it('doves ask a worn army to be spared: so many men lost in battle and no more', () => {
    const S = GOV.directives.spare;
    let d: Directive | undefined, g = playing('GER', 3);
    for (const seed of [3, 5, 7]) {
      g = playing('GER', seed);
      const st = govOf(g, 'GER')!;
      st.seats = { ...DOVES };
      st.headNext = 999;
      for (const c of g.corps) if (c.nat === 'GER' && !c.dead) c.str = 0.7;
      g.nations.GER.pp = -50;
      for (let k = 0; k < 8 && d?.kind !== 'spare'; k++) { delete st.dir; delete st.lastKind; st.next = g.turn; govMonth(g, 'GER'); d = govOf(g, 'GER')!.dir; g.turn++; }
      if (d?.kind === 'spare') break;
    }
    expect(d?.kind).toBe('spare');
    const st = govOf(g, 'GER')!;
    expect(d!.title).toBe('Spare the army');
    expect(d!.text).toMatch(/stand at 70% of their strength/);
    expect(d!.target).toBeGreaterThan(0);
    expect(d!.capital).toBe(S.capital);
    expect(d!.region).toBeUndefined();
    expect(governmentOf(g)!.directive!.progress).toMatch(/has lost 0 men in battle since the order came/);
    // A fresh army is not asked to be spared, and neither is one under a balanced table.
    const fresh = copy(g), fs = govOf(fresh, 'GER')!;
    for (const c of fresh.corps) if (c.nat === 'GER' && !c.dead) c.str = 1;
    for (let k = 0; k < 6; k++) { delete fs.dir; delete fs.lastKind; fs.next = fresh.turn; govMonth(fresh, 'GER'); expect(govOf(fresh, 'GER')!.dir?.kind).not.toBe('spare'); fresh.turn++; }
    // Losses within the allowance, and the order is carried out when its months are up.
    const kept = copy(g), ks = govOf(kept, 'GER')!;
    battle(kept, { won: true, lost: d!.target! - 1000 });
    judgeDirective(kept, 'GER');
    expect(ks.dir?.lost).toBe(d!.target! - 1000);
    kept.turn = d!.due + 1;
    const pc = kept.nations.GER.pc;
    judgeDirective(kept, 'GER');
    expect(ks.dir).toBeUndefined();
    expect(ks.done).toBe(1);
    expect(kept.nations.GER.pc).toBe(Math.min(DIPLOMACY.pcCap, pc + S.capital));
    // One man too many, and it has failed at once.
    battle(g, { won: false, lost: d!.target! - 1000 });
    judgeDirective(g, 'GER');
    expect(st.dir?.lost).toBe(d!.target! - 1000);
    battle(g, { won: false, lost: 2000, attacking: false });
    judgeDirective(g, 'GER');
    expect(st.dir).toBeUndefined();
    expect(st.missed).toBe(1);
    expect(said(g, /was not carried out: Spare the army/)).toBe(true);
  });

  it('a cabinet that falls goes back to caretakers, and the table with it', () => {
    const g = playing('GER');
    const st = quiet(g);
    st.seats = { ...HAWKS };
    st.wing = 'hawks';
    st.trust = 0;
    st.low = GOV.trust.crisisMonths - 1;
    const h = g.headlines.length;
    govMonth(g, 'GER');
    expect(said(g, /The government falls/, h)).toBe(true);
    expect(governmentOf(g)!.wings.top).toBe('balanced');
    expect(said(g, /Neither wing has the better/, h)).toBe(true);
  });
});

describe('call your shot', () => {
  it('the cabinet reads the odds on the day: weaker troops, a deeper town and a shorter time make a bolder promise', () => {
    const g = playing('GER');
    const P = GOV.promise;
    const rank = (b: string): number => P.bands.findIndex((x) => x.id === b);
    const list = promiseTargets(g, 'GER', 40);
    expect(list.length).toBeGreaterThan(5);
    for (const t of list) {
      expect(t.terms.map((x) => x.months)).toEqual(P.months);
      // More time never makes a promise bolder, and a bolder one never pays less or costs less.
      for (let i = 1; i < t.terms.length; i++) {
        expect(rank(t.terms[i].band), t.town).toBeLessThanOrEqual(rank(t.terms[i - 1].band));
        expect(t.terms[i].trust).toBeLessThanOrEqual(t.terms[i - 1].trust);
        expect(t.terms[i].price).toBeLessThanOrEqual(t.terms[i - 1].price);
      }
      for (const x of t.terms) {
        expect(x.due).toBe(g.turn + x.months - 1);
        expect(x.back).toBeCloseTo(Math.round(P.bands[rank(x.band)].price * P.back * swing('GER') * 10) / 10, 9);
        expect(x.text).toContain(x.bandName);
        expect(x.text).toContain(x.town);
      }
    }
    // Beside overwhelming strength it is safe; far off, behind a front, it is daring.
    const easy = list.filter((t) => t.depth === 1 && t.odds >= 4);
    expect(easy.length).toBeGreaterThan(0);
    for (const t of easy) expect(t.terms[t.terms.length - 1].band).toBe('safe');
    const far = list.filter((t) => t.depth >= 3 && t.odds < 0.5);
    expect(far.length).toBeGreaterThan(0);
    for (const t of far) expect(t.terms[0].band).toBe('daring');
    // The same town, with the troops beside it thinned out, is a bolder promise.
    const t0 = easy[0], r = g.regions[t0.region];
    const before = promiseTerms(g, 'GER', r.id, P.months[0])!;
    for (const c of g.corps) if (c.nat === 'GER' && !c.dead) c.str *= 0.15;
    const after = promiseTerms(g, 'GER', r.id, P.months[0])!;
    expect(after.odds).toBeLessThan(before.odds);
    expect(rank(after.band)).toBeGreaterThan(rank(before.band));
    expect(after.trust).toBeGreaterThan(before.trust);
    expect(after.price).toBeGreaterThan(before.price);
  });

  it('the list leads with capitals and then the towns beside him, and a capital pays more than a village', () => {
    const g = playing('GER');
    const list = promiseTargetsOf(g, 8);
    expect(list).toHaveLength(8);
    expect(list[0].size).toBe('capital');
    const rest = list.filter((t) => t.size !== 'capital');
    for (let i = 1; i < rest.length; i++) expect(rest[i].depth).toBeGreaterThanOrEqual(rest[i - 1].depth);
    const S = GOV.promise.size;
    expect(S.capital).toBeGreaterThan(S.city);
    expect(S.city).toBeGreaterThan(S.village);
  });

  it('he gives his word: one promise at a time, and the cabinet does not hear another for some months', () => {
    const g = playing('GER');
    const { near, deep } = towns(g);
    const P = GOV.promise;
    expect(whyNotOrderPromise(g)).toBeNull();
    expect(whyNotOrderPromise(g, near.id, 3)).toBeNull();
    expect(whyNotOrderPromise(g, near.id, 5)).toMatch(/A promise is for 2, 3, 4 or 6 months/);
    const t = promiseTermsOf(g, near.id, 3)!;
    const h = g.headlines.length;
    expect(orderPromise(g, near.id, 5)).toBe(false);
    expect(orderPromise(g, near.id, 3)).toBe(true);
    const st = govOf(g, 'GER')!;
    expect(st.vow).toMatchObject({ region: near.id, issued: g.turn, due: g.turn + 2, months: 3, band: t.band, trust: t.trust, capital: t.capital, money: t.money, price: t.price });
    expect(said(g, new RegExp(`You give the war cabinet your word: ${townName(near)}`), h)).toBe(true);
    const v = governmentOf(g)!;
    expect(v.promise).toMatchObject({ town: townName(near), left: 3, bandName: t.bandName, back: t.back });
    expect(v.promise!.progress).toMatch(/is held by Poland/);
    expect(v.promiseWhy).toMatch(/You have a promise standing/);
    expect(promiseTargetsOf(g)).toEqual([]);
    expect(orderPromise(g, deep.id, 3)).toBe(false);
    // It is kept in a save.
    expect(govOf(copy(g), 'GER')!.vow).toEqual(st.vow);
    // Taken back, the wait still runs from the day it was given.
    expect(orderTakeBack(g)).toBe(true);
    expect(whyNotOrderPromise(g)).toMatch(new RegExp(`it will hear another in ${P.every} months`));
    g.turn += P.every - 1;
    expect(whyNotPromise(g, 'GER')).toMatch(/in a month/);
    g.turn++;
    expect(whyNotPromise(g, 'GER')).toBeNull();
  });

  it('what cannot be promised, and why', () => {
    const g = playing('GER');
    const { near } = towns(g);
    const own = g.regions.find((r) => r.ctrl === 'GER')!, neutral = g.regions.find((r) => r.ctrl === 'SWE')!;
    expect(whyNotPromise(g, 'GER', own.id)).toMatch(/in your side's hands already/);
    expect(whyNotPromise(g, 'GER', neutral.id)).toMatch(/You are not at war with Sweden/);
    expect(promiseTerms(g, 'GER', neutral.id, 3)).toBeNull();
    // A town too far behind the front.
    const far = g.regions.find((r) => r.ctrl === 'UK' && r.owner === 'UK')!;
    expect(whyNotPromise(g, 'GER', far.id)).toMatch(new RegExp(`more than ${GOV.promise.reach} provinces beyond`));
    expect(promiseTerms(g, 'GER', far.id, 3)).toBeNull();
    // The town the cabinet has ordered taken already.
    const st = govOf(g, 'GER')!;
    st.dir = { id: g.turn, nat: 'GER', kind: 'take', from: 'GER', region: near.id, issued: g.turn, due: g.turn + 3, capital: 6, money: 2, title: 'Take the town', text: '' };
    expect(whyNotPromise(g, 'GER', near.id)).toMatch(/the cabinet's own directive already/);
    delete st.dir;
    expect(whyNotPromise(g, 'GER', near.id)).toBeNull();
    // A country at peace has nothing to promise.
    const s = playing('SWE');
    expect(whyNotOrderPromise(s)).toMatch(/Only a country at war/);
    expect(promiseTargetsOf(s)).toEqual([]);
    expect(governmentOf(s)!.promiseWhy).toMatch(/Only a country at war/);
  });

  it('kept: the cabinet pays what it said on the day, and forgives one directive ignored', () => {
    const g = playing('GER');
    const st = quiet(g);
    const { near } = towns(g);
    const t = promiseTerms(g, 'GER', near.id, 2)!;
    expect(promise(g, 'GER', near.id, 2)).toBe(true);
    st.missed = 2;
    const n = g.nations.GER, pc = n.pc, pp = n.pp, trust = st.trust, h = g.headlines.length;
    judgeDirective(g, 'GER');
    expect(st.vow).toBeDefined();
    near.ctrl = 'GER';
    judgeDirective(g, 'GER');
    expect(st.vow).toBeUndefined();
    expect(st.trust).toBeCloseTo(Math.min(100, trust + t.trust), 9);
    expect(n.pc).toBe(Math.min(Math.max(pc, DIPLOMACY.pcCap), pc + t.capital));
    expect(n.pp).toBeCloseTo(pp + t.money, 9);
    expect(st.missed).toBe(1);
    expect(st.kept).toBe(1);
    expect(said(g, /You kept your word/, h)).toBe(true);
    const v = governmentOf(g)!;
    expect(v.kept).toBe(1);
    expect(v.moved).toContainEqual({ label: 'A promise kept', amount: t.trust });
    // An ally's troops in the town keep it as well as his own.
    const g2 = playing('GER'), s2 = quiet(g2), r2 = towns(g2).near;
    expect(promise(g2, 'GER', r2.id, 2)).toBe(true);
    r2.ctrl = 'ITA';
    judgeDirective(g2, 'GER');
    expect(allied(g2, 'GER', 'ITA')).toBe(true);
    expect(s2.kept).toBe(1);
  });

  it('broken when its last month is out, at the price named; taken back for half; released if peace overtakes it', () => {
    const g = playing('GER');
    const st = quiet(g);
    const { near } = towns(g);
    const t = promiseTerms(g, 'GER', near.id, 3)!;
    // Broken.
    const b = copy(g), bs = govOf(b, 'GER')!;
    expect(promise(b, 'GER', near.id, 3)).toBe(true);
    b.turn = t.due;
    judgeDirective(b, 'GER');
    expect(bs.vow).toBeDefined();
    b.turn = t.due + 1;
    const h = b.headlines.length;
    judgeDirective(b, 'GER');
    expect(bs.vow).toBeUndefined();
    expect(bs.trust).toBeCloseTo(st.trust - t.price, 9);
    expect(bs.broken).toBe(1);
    expect(bs.missed).toBe(0);
    expect(said(b, /Your word was not kept/, h)).toBe(true);
    expect(government(b)!.broken).toBe(1);
    // Taken back.
    const k = copy(g), ks = govOf(k, 'GER')!;
    expect(whyNotTakeBack(k, 'GER')).toMatch(/no promise standing/);
    expect(takeBack(k, 'GER')).toBe(false);
    expect(promise(k, 'GER', near.id, 3)).toBe(true);
    expect(whyNotTakeBackPromise(k)).toBeNull();
    expect(takeBack(k, 'GER')).toBe(true);
    expect(ks.trust).toBeCloseTo(st.trust - t.back, 9);
    expect(ks.broken ?? 0).toBe(0);
    expect(t.back).toBeLessThan(t.price);
    // Released: the town passes to a country he is not at war with.
    const v = copy(g), vs = govOf(v, 'GER')!;
    expect(promise(v, 'GER', near.id, 3)).toBe(true);
    v.regions[near.id].ctrl = 'SWE';
    judgeDirective(v, 'GER');
    expect(vs.vow).toBeUndefined();
    expect(vs.trust).toBeCloseTo(st.trust, 9);
    expect(whyNotPromise(v, 'GER')).toBeNull();
    expect(said(v, /Your promise is released/)).toBe(true);
  });

  it('the cabinet does not order the town he has promised, and a promise is judged as the month closes too', () => {
    const g = playing('GER');
    const st = govOf(g, 'GER')!;
    st.seats = { ...HAWKS };
    st.headNext = 999;
    const first = promiseTargets(g, 'GER', 40).filter((t) => t.depth === 1);
    for (let k = 0; k < 10; k++) {
      delete st.dir;
      delete st.lastKind;
      delete st.vow;
      st.vowNext = 0;
      const t = first[k % first.length];
      expect(promise(g, 'GER', t.region, 4)).toBe(true);
      st.next = g.turn;
      govMonth(g, 'GER');
      expect(govOf(g, 'GER')!.dir?.region).not.toBe(t.region);
    }
  });
});

describe('the cabinet wants a head', () => {
  it('after defeats running the cabinet names the general, with months to answer, and says so', () => {
    const g = playing('GER');
    const st = quiet(g);
    const H = GOV.head;
    const gen = loser(g, H.streak);
    const h = g.headlines.length;
    expect(untilAsked(g, st)).toBe(true);
    expect(st.head).toMatchObject({ general: gen.id, issued: g.turn, due: g.turn + H.answer - 1 });
    expect(st.head!.why).toBe('He has lost his last two battles.');
    expect(said(g, new RegExp(`The war cabinet wants a head: it asks you to dismiss ${gen.name}`), h)).toBe(true);
    expect(headWantedOf(g)).toBe(gen.id);
    expect(headOf(g, 'UK')).toBeNull();
    const v = governmentOf(g)!.head!;
    expect(v).toMatchObject({ general: gen.id, name: gen.name, left: H.answer, command: corpsOf(g, gen)!.name, loved: popular(gen) });
    expect(v.gain).toBeCloseTo(Math.round(H.sacked * swing('GER') * 10) / 10, 9);
    expect(v.cost).toBeCloseTo(Math.round(H.refused * swing('GER') * 10) / 10, 9);
    expect(whyNotStandByGeneral(g)).toBeNull();
    // It is kept in a save, and it asks for one head at a time.
    expect(govOf(copy(g), 'GER')!.head).toEqual(st.head);
    loser(g, 5, [gen.id]);
    govMonth(g, 'GER');
    expect(st.head!.general).toBe(gen.id);
  });

  it('it names nobody without cause: one defeat is not enough, unless a directive was failed as well', () => {
    const g = playing('GER');
    const st = quiet(g);
    expect(untilAsked(g, st, 12)).toBe(false);
    const gen = loser(g, 1);
    expect(untilAsked(g, st, 12)).toBe(false);
    // The month a directive is failed, one defeat is cause enough.
    let asked = false;
    for (let k = 0; k < 24 && !asked; k++) {
      g.turn++;
      st.blame = g.turn;
      st.next = g.turn + 99;
      st.headNext = 0;
      govMonth(g, 'GER');
      asked = !!st.head;
    }
    expect(asked).toBe(true);
    expect(st.head!.general).toBe(gen.id);
    expect(st.head!.why).toBe('He lost his last battle, and the cabinet\'s directive was not carried out.');
  });

  it('it never names a general without a command, one it was proved wrong about, or anyone in a country at peace', () => {
    const g = playing('GER');
    const st = quiet(g);
    for (const x of g.generals) if (x.nat === 'GER') { x.rec.streak = 3; x.rec.battles += 3; if (x.corps >= 0) x.traits.push('vindicated'); }
    expect(untilAsked(g, st, 12)).toBe(false);
    const s = playing('SWE'), ss = quiet(s);
    for (const x of s.generals) if (x.nat === 'SWE') { x.rec.streak = 3; x.rec.battles += 3; }
    ss.headNext = 0;
    for (let k = 0; k < 12; k++) { s.turn++; ss.next = s.turn + 99; govMonth(s, 'SWE'); }
    expect(ss.head).toBeUndefined();
  });

  it('among several it names the one with the most defeats running, and it does not ask again for some months', () => {
    const g = playing('GER');
    const st = quiet(g);
    const a = loser(g, 2), b = loser(g, 4, [a.id]);
    expect(untilAsked(g, st)).toBe(true);
    expect(st.head!.general).toBe(b.id);
    const asked = g.turn;
    expect(orderStandBy(g)).toBe(true);
    expect(st.headNext).toBe(asked + GOV.head.every);
    for (let k = 0; k < GOV.head.every - 1; k++) { g.turn++; st.next = g.turn + 99; govMonth(g, 'GER'); expect(st.head).toBeUndefined(); }
  });

  it('dismissed: the cabinet has its head, its trust recovers, and it carries what his popularity would have cost', () => {
    const g = playing('GER');
    const st = quiet(g);
    const gen = loser(g, 2);
    gen.level = COMMAND.sack.popularLevel;
    expect(popular(gen)).toBe(true);
    expect(untilAsked(g, st)).toBe(true);
    const c = corpsOf(g, gen)!, org = c.org, n = g.nations.GER, trust = st.trust, h = g.headlines.length;
    n.pc = 10;
    expect(orderSack(g, gen.id)).toBe(true);
    expect(gen.status).toBe('sacked');
    expect(st.head).toBeUndefined();
    expect(st.trust).toBeCloseTo(trust + GOV.head.sacked * swing('GER'), 9);
    // The army still takes it badly; the political capital is not charged.
    expect(c.org).toBeCloseTo(Math.max(0, org - COMMAND.sack.org), 9);
    expect(n.pc).toBe(10);
    expect(said(g, /The war cabinet has its head/, h)).toBe(true);
    expect(headWantedOf(g)).toBeNull();
    expect(governmentOf(g)!.moved).toContainEqual({ label: 'A general dismissed at the cabinet\'s wish', amount: Math.round(GOV.head.sacked * swing('GER') * 10) / 10 });
    // A general nobody asked for is dismissed as before: no trust for it, and the price of his popularity is paid.
    const other = g.generals.find((x) => x.nat === 'GER' && x.status === 'active' && x.corps >= 0)!;
    other.level = COMMAND.sack.popularLevel;
    const t2 = st.trust;
    expect(orderSack(g, other.id)).toBe(true);
    expect(st.trust).toBe(t2);
    expect(n.pc).toBe(10 - COMMAND.sack.pc);
  });

  it('stood by, he wins his next battle: vindicated, for the rest of the war', () => {
    const g = playing('GER');
    const st = quiet(g);
    const H = GOV.head;
    const gen = loser(g, 2);
    expect(untilAsked(g, st)).toBe(true);
    const trust = st.trust, h = g.headlines.length;
    expect(orderStandBy(g)).toBe(true);
    expect(st.head).toBeUndefined();
    expect(st.trust).toBeCloseTo(trust - H.refused * swing('GER'), 9);
    expect(st.stood).toMatchObject({ general: gen.id, until: g.turn + H.watch - 1, battles: gen.rec.battles, won: gen.rec.won });
    expect(governmentOf(g)!.stoodBy).toMatchObject({ general: gen.id, name: gen.name, left: H.watch });
    expect(said(g, new RegExp(`You stand by ${gen.name}`), h)).toBe(true);
    expect(whyNotStandBy(g, 'GER')).toMatch(/asking for nobody/);
    // Nothing yet: he has not fought.
    judgeDirective(g, 'GER');
    expect(st.stood).toBeDefined();
    const c = corpsOf(g, gen)!, att = cmdAttack(g, c, false), def = cmdDefence(g, c);
    gen.rec.battles++;
    gen.rec.won++;
    gen.rec.streak = 0;
    const t2 = st.trust;
    judgeDirective(g, 'GER');
    expect(st.stood).toBeUndefined();
    expect(gen.traits).toContain('vindicated');
    expect(st.trust).toBeCloseTo(t2 + H.vindicated * swing('GER'), 9);
    expect(said(g, new RegExp(`Vindicated: ${gen.name}`), h)).toBe(true);
    expect(cmdAttack(g, c, false)).toBeCloseTo(att * COMMAND.traits.vindicated.attack!, 9);
    expect(cmdDefence(g, c)).toBeCloseTo(def * COMMAND.traits.vindicated.defence!, 9);
    // The mark is kept in a save, and the cabinet never asks for him again.
    expect(copy(g).generals.find((x) => x.id === gen.id)!.traits).toContain('vindicated');
    gen.rec.streak = 6;
    expect(untilAsked(g, st, 12)).toBe(false);
  });

  it('stood by, he loses again: the cabinet says it told you so, and no answer at all is taken as standing by him', () => {
    const g = playing('GER');
    const st = quiet(g);
    const H = GOV.head;
    const gen = loser(g, 2);
    expect(untilAsked(g, st)).toBe(true);
    // He does not answer: the months run out.
    const trust = st.trust;
    for (let k = 0; k < H.answer - 1; k++) { g.turn++; st.next = g.turn + 99; govMonth(g, 'GER'); expect(st.head).toBeDefined(); }
    const before = st.trust;
    g.turn++;
    st.next = g.turn + 99;
    govMonth(g, 'GER');
    expect(st.head).toBeUndefined();
    expect(st.stood?.general).toBe(gen.id);
    expect(st.trust).toBeLessThan(before);
    expect(trust).toBeGreaterThan(st.trust);
    // He loses his next battle.
    gen.rec.battles++;
    gen.rec.streak++;
    const t2 = st.trust, h = g.headlines.length;
    judgeDirective(g, 'GER');
    expect(st.stood).toBeUndefined();
    expect(gen.traits).not.toContain('vindicated');
    expect(st.trust).toBeCloseTo(t2 - H.wrong * swing('GER'), 9);
    expect(said(g, /has lost again. The war cabinet says it told you so/, h)).toBe(true);
  });

  it('the watch ends quietly if he never fights, and the demand lapses if he falls first', () => {
    const g = playing('GER');
    const st = quiet(g);
    const gen = loser(g, 2);
    expect(untilAsked(g, st)).toBe(true);
    // Killed before the player answers: nothing is won or lost.
    const k = copy(g), ks = govOf(k, 'GER')!, kg = k.generals.find((x) => x.id === gen.id)!;
    kg.status = 'dead';
    const t = ks.trust;
    k.turn++;
    ks.next = k.turn + 99;
    ks.headNext = k.turn + 99;
    govMonth(k, 'GER');
    expect(ks.head).toBeUndefined();
    expect(ks.stood).toBeUndefined();
    expect(governmentOf(k)!.moved.some((c) => /general/.test(c.label))).toBe(false);
    expect(Math.abs(ks.trust - t)).toBeLessThan(2);
    // Stood by, and never in a battle again while the cabinet watches.
    expect(standBy(g, 'GER')).toBe(true);
    const until = st.stood!.until;
    g.turn = until;
    judgeDirective(g, 'GER');
    expect(st.stood).toBeDefined();
    g.turn = until + 1;
    const t2 = st.trust;
    judgeDirective(g, 'GER');
    expect(st.stood).toBeUndefined();
    expect(st.trust).toBe(t2);
    expect(gen.traits).not.toContain('vindicated');
  });
});

describe('all of it lives behind the cabinet switch', () => {
  it('switched off: the promise, the demand and the watch are withdrawn without blame, and nothing new comes', () => {
    const g = playing('GER');
    const st = quiet(g);
    const { near } = towns(g);
    expect(promise(g, 'GER', near.id, 3)).toBe(true);
    const a = loser(g, 2);
    expect(untilAsked(g, st)).toBe(true);
    expect(standBy(g, 'GER')).toBe(true);
    st.head = { general: a.id, issued: g.turn, due: g.turn + 1, why: 'He has lost his last two battles.' };
    st.seats = { ...HAWKS };
    const trust = st.trust;
    expect(orderCabinet(g, false)).toBe(true);
    expect(st.vow).toBeUndefined();
    expect(st.head).toBeUndefined();
    expect(st.stood).toBeUndefined();
    expect(st.trust).toBe(trust);
    expect(governmentOf(g)).toBeNull();
    expect(headWantedOf(g)).toBeNull();
    expect(promiseTargetsOf(g)).toEqual([]);
    expect(whyNotOrderPromise(g)).toMatch(/Only your own war cabinet/);
    expect(orderPromise(g, near.id, 3)).toBe(false);
    expect(orderStandBy(g)).toBe(false);
    expect(orderTakeBack(g)).toBe(false);
    // A general dismissed while it is off is dismissed as he always was.
    a.rec.won++;
    judgeDirective(g, 'GER');
    expect(a.traits).not.toContain('vindicated');
    // Switched on again, the ministers sit as they were left and the table leans as it did.
    expect(setCabinet(g, true)).toBe(true);
    expect(governmentOf(g)!.wings.top).toBe('hawks');
    expect(governmentOf(g)!.promise).toBeNull();
  });
});
