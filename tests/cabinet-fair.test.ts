// A cabinet that is fair, and a say over the men (the grand campaign, Version 62): what the playtester found in
// Versions 59 and 60, put right. The cabinet reckons a town from the attack card with the corps in reach brought up,
// not from a head count. A promise is reckoned on the best day the cabinet remembers, so stepping back a province
// buys nothing, and a shorter day pays a little more. Switching the cabinet off settles what is open. A head is asked
// only over battles that could have been won, with the evidence, and not twice over the same ones. Hold orders come
// only where the enemy is on the move. A country at peace is given small jobs, and a table of doves asks for no town.
// The player can call a class up early, or hold the standing call-up. And he may make his office secure: the cabinet
// sits and judges as before, but it can never bring him down. None of it touches the historical scenarios.
import { describe, expect, it } from 'vitest';
import { afterBattle, corpsOf } from '../src/campaign/commanders';
import { civicOffers } from '../src/campaign/buildings';
import { atWar, createGame, relocate } from '../src/campaign/game';
import { GOV, directiveOdds, formOf, govMonth, govMul, govOf, heldAgainst, judgeDirective, promise, promiseTargets, promiseTerms, threatOdds } from '../src/campaign/government';
import { agentsOf } from '../src/campaign/intel';
import { MANPOWER, classArrives, classGapUntil, manpower, menLeft, poolAtStart } from '../src/campaign/manpower';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import {
  attackOutlook, beginHuman, cabinetIsOff, callUpOf, candidates, endHuman, governmentOf, humanUp, manpowerOf, officeIsSecure, orderBuilding, orderCabinet, orderCallUp, orderEarlyClass, orderOffice, orderRecruitAgent, runTurnAI, stepAI,
  whyNotOrderEarlyClass,
} from '../src/campaign/turn';
import { MEN_PER_DIV, type BattleReport, type Corps, type Directive, type Game, type General, type GovState, type Region } from '../src/campaign/types';

const free = (seed: number, human: string | null = null): Game => startOpen(createGame(seed, human));
function playing(human = 'GER', seed = 3): Game {
  const g = free(seed, human);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const copy = (g: Game): Game => loadGame(saveGame(g))!;
const swing = (nat: string): number => GOV.forms[formOf(nat)].swing;
const HAWKS = { staff: 'organiser', arms: 'industrialist', foreign: 'statesman', finance: 'taxman' };
const DOVES = { staff: 'reformer', arms: 'planner', foreign: 'merchant', finance: 'paymaster' };
const rank = (b: string): number => GOV.promise.bands.findIndex((x) => x.id === b);
/** The cabinet's next directive, drawn now. */
function ask(g: Game, st: GovState, nat = g.human!): Directive | undefined {
  delete st.dir;
  st.next = g.turn;
  st.headNext = g.turn + 99;
  govMonth(g, nat);
  return st.dir;
}
/** Every province within `far` steps of one. */
function within(g: Game, from: number, far: number): Set<number> {
  const seen = new Set<number>([from]);
  let ring = [from];
  for (let d = 0; d < far; d++) { const next: number[] = []; for (const i of ring) for (const j of g.regions[i].nb) if (!seen.has(j)) { seen.add(j); next.push(j); } ring = next; }
  return seen;
}
const field = (g: Game, nat: string): Corps[] => g.corps.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort' && !c.town);
/** An enemy town the player's corps can attack today, and what the card reads for all of them going. */
function attackable(g: Game, nat: string): { r: Region; card: number }[] {
  const out: { r: Region; card: number }[] = [];
  for (const r of g.regions) {
    if (!atWar(g, nat, r.ctrl)) continue;
    const look = attackOutlook(g, candidates(g, r), r);
    if (look) out.push({ r, card: look.odds });
  }
  return out;
}

describe('the cabinet reckons a town from the attack card', () => {
  it('with the corps in reach brought up it never reads worse than the card does today, and the same where nothing more can come', () => {
    const g = playing('GER');
    const list = attackable(g, 'GER');
    expect(list.length).toBeGreaterThan(3);
    for (const { r, card } of list) expect(directiveOdds(g, 'GER', r.id), r.name).toBeGreaterThanOrEqual(card - 0.011);
    // Take away every corps that is not beside the town already: nothing is left to bring up, and the two agree.
    const { r } = list[0];
    const g2 = copy(g), r2 = g2.regions[r.id];
    for (const c of field(g2, 'GER')) if (!r2.nb.includes(c.reg)) { c.dead = true; g2.regions[c.reg].units = g2.regions[c.reg].units.filter((u) => u !== c); }
    const look = attackOutlook(g2, candidates(g2, r2), r2)!;
    expect(directiveOdds(g2, 'GER', r2.id)).toBeCloseTo(look.odds, 2);
    // With nobody of his within reach the cabinet reads nothing at all, and asks for no such town.
    for (const c of field(g2, 'GER')) { c.dead = true; g2.regions[c.reg].units = g2.regions[c.reg].units.filter((u) => u !== c); }
    expect(directiveOdds(g2, 'GER', r2.id)).toBe(0);
  });

  it('fieldworks and the quality of the troops count, as they do on the card', () => {
    const g = playing('GER');
    const { r } = attackable(g, 'GER').sort((a, b) => a.card - b.card)[0];
    const open = directiveOdds(g, 'GER', r.id);
    const g2 = copy(g);
    for (const c of g2.regions[r.id].units) if (atWar(g2, c.nat, 'GER') && !c.town && c.kind !== 'fort') { c.dug = true; c.dugFor = 3; }
    expect(directiveOdds(g2, 'GER', r.id)).toBeLessThan(open);
    // Green troops against the same defenders: a head count would not notice, the card does.
    const g3 = copy(g);
    for (const c of field(g3, 'GER')) c.xp = 0;
    const g4 = copy(g);
    for (const c of field(g4, 'GER')) c.xp = 100;
    expect(directiveOdds(g4, 'GER', r.id)).toBeGreaterThan(directiveOdds(g3, 'GER', r.id));
  });

  it('a directive to take a town says what the card reads, never names one below the least it will ask for, and the view carries today\'s line', () => {
    const T = GOV.directives.take;
    let seen = 0;
    for (const seed of [3, 7, 11]) {
      const g = playing('GER', seed), st = govOf(g, 'GER')!;
      st.seats = { ...HAWKS };
      const d = ask(g, st);
      if (!d || d.kind !== 'take') continue;
      seen++;
      const odds = directiveOdds(g, 'GER', d.region!);
      expect(odds).toBeGreaterThanOrEqual(T.least);
      expect(!!d.hard).toBe(odds < T.odds);
      expect(d.text).toMatch(/With your corps in reach brought up, the attack card reads (about [\d.]+|better than 10) to 1\./);
      const v = governmentOf(g)!.directive!;
      expect(v.oddsLine).toMatch(/^With your corps in reach brought up, the attack card reads (about [\d.]+|better than 10) to 1 today\.$/);
      // March everything away and the line says so.
      for (const c of field(g, 'GER')) { c.dead = true; g.regions[c.reg].units = g.regions[c.reg].units.filter((u) => u !== c); }
      expect(governmentOf(g)!.directive!.oddsLine).toBe('None of your corps is within two provinces of it today.');
    }
    expect(seen).toBeGreaterThan(0);
  });
});

describe('a promise is reckoned on the best day the cabinet remembers', () => {
  /** Germany's first month, a town beside its troops, and every corps within the promise's reach of it sent far to the rear. */
  function steppedBack(): { g: Game; r: Region; before: ReturnType<typeof promiseTerms> } {
    const g = playing('GER'), P = GOV.promise;
    const t = promiseTargets(g, 'GER', 40).find((x) => x.depth === 1 && x.holder === 'POL' && x.terms[0].band !== 'daring')!;
    const r = g.regions[t.region];
    const before = promiseTerms(g, 'GER', r.id, P.months[0]);
    const near = within(g, r.id, P.reach + 1);
    const rear = g.regions.find((x) => x.ctrl === 'GER' && x.owner === 'GER' && !near.has(x.id))!;
    expect(rear).toBeDefined();
    for (const c of field(g, 'GER')) if (near.has(c.reg)) relocate(g, c, rear);
    return { g, r, before };
  }

  it('stepping back before naming the bet changes nothing: the cabinet goes by how the month opened', () => {
    const P = GOV.promise;
    const { g, r, before } = steppedBack();
    const st = govOf(g, 'GER')!;
    expect(st.reach!.some((m) => m.t === g.turn && m.odds[r.id] !== undefined)).toBe(true);
    const after = promiseTerms(g, 'GER', r.id, P.months[0])!;
    expect(after.band).toBe(before!.band);
    expect(after.odds).toBeCloseTo(before!.odds, 1);
    expect(after.odds).toBeGreaterThan(0.5);
    expect(after.depth).toBe(before!.depth);
    expect(after.trust).toBe(before!.trust);
    expect(after.price).toBe(before!.price);
    expect(after.text).toContain(`against ${after.town}, as it did when the month opened. It lay beside your ground or your troops then`);
    // Without its memory the cabinet would have been taken in: the same town reads a bolder promise that pays more.
    const g2 = copy(g);
    delete govOf(g2, 'GER')!.reach;
    const fooled = promiseTerms(g2, 'GER', r.id, P.months[0])!;
    expect(rank(fooled.band)).toBeGreaterThan(rank(before!.band));
    expect(fooled.trust).toBeGreaterThan(before!.trust);
    // It is kept in a save.
    expect(promiseTerms(copy(g), 'GER', r.id, P.months[0])).toEqual(after);
  });

  it('it remembers for so many months and no longer, and says which month it goes by', () => {
    const P = GOV.promise;
    const { g, r, before } = steppedBack();
    const st = govOf(g, 'GER')!;
    const opened = g.turn;
    g.turn += P.memory - 1;
    const late = promiseTerms(g, 'GER', r.id, P.months[0])!;
    expect(late.band).toBe(before!.band);
    expect(late.text).toMatch(/, as it did in [A-Z][a-z]+ 19\d\d\. It lay /);
    g.turn = opened + P.memory;
    const lapsed = promiseTerms(g, 'GER', r.id, P.months[0])!;
    expect(rank(lapsed.band)).toBeGreaterThan(rank(before!.band));
    expect(lapsed.text).not.toContain('as it did');
    expect(lapsed.text).toContain('It lies ');
    // Each month's opening is noted, and old months are dropped.
    govMonth(g, 'GER');
    expect(st.reach!.every((m) => g.turn - m.t < P.memory)).toBe(true);
    expect(st.reach!.some((m) => m.t === g.turn)).toBe(true);
  });

  it('within one band a shorter day pays a little more, and costs the same to break', () => {
    const g = playing('GER'), P = GOV.promise;
    const flat = promiseTargets(g, 'GER', 40).filter((t) => new Set(t.terms.map((x) => x.band)).size === 1);
    expect(flat.length).toBeGreaterThan(0);
    for (const t of flat) {
      const first = t.terms[0], last = t.terms[t.terms.length - 1];
      const band = P.bands[rank(first.band)];
      expect(first.trust, t.town).toBeCloseTo(Math.round(band.trust * swing('GER') * (1 + P.haste) * 10) / 10, 9);
      expect(last.trust, t.town).toBeCloseTo(Math.round(band.trust * swing('GER') * 10) / 10, 9);
      expect(first.trust).toBeGreaterThan(last.trust);
      for (let i = 1; i < t.terms.length; i++) {
        expect(t.terms[i].trust).toBeLessThanOrEqual(t.terms[i - 1].trust);
        expect(t.terms[i].capital).toBeLessThanOrEqual(t.terms[i - 1].capital);
        expect(t.terms[i].money).toBeLessThanOrEqual(t.terms[i - 1].money);
        expect(t.terms[i].price).toBe(t.terms[i - 1].price);
      }
    }
  });

  it('a town four provinces off is a daring promise at the shortest day, whatever the card reads against it', () => {
    const g = playing('GER'), P = GOV.promise;
    expect(P.reach).toBe(4);
    const far = promiseTargets(g, 'GER', 60).filter((t) => t.depth === 4);
    expect(far.length).toBeGreaterThan(0);
    for (const t of far) expect(t.terms[0].band, t.town).toBe('daring');
  });

  it('a safe promise kept forgives no directive ignored; a bolder one does', () => {
    for (const safe of [true, false]) {
      const g = playing('GER'), st = govOf(g, 'GER')!;
      delete st.dir; st.next = g.turn + 99;
      const t = promiseTargets(g, 'GER', 40).find((x) => x.holder === 'POL' && (x.terms[0].band === 'safe') === safe)!;
      expect(t, `${safe}`).toBeDefined();
      expect(promise(g, 'GER', t.region, GOV.promise.months[0])).toBe(true);
      expect(st.vow!.band === 'safe').toBe(safe);
      st.missed = 2;
      g.regions[t.region].ctrl = 'GER';
      judgeDirective(g, 'GER');
      expect(st.vow).toBeUndefined();
      expect(st.kept).toBe(1);
      expect(st.missed).toBe(safe ? 2 : 1);
    }
  });
});

describe('the cabinet asks for a head only over battles that could have been won', () => {
  it('a battle is held against a general when he lost a defence to an attack that had less than an even chance, or was thrown back attacking at good odds', () => {
    const W = GOV.head.winnable;
    const gen = { rec: { lost: [
      { t: 1, reg: 0, odds: 1.1, att: false },               // a defence the attack had less than an even chance against
      { t: 1, reg: 0, odds: 1.3, att: false },               // in the open the attacker was likelier than not to carry it (Version 63)
      { t: 1, reg: 0, odds: 1.4, att: false, even: 1.7 },    // against prepared positions the same attack was a gamble: it could have been held
      { t: 1, reg: 0, odds: W + 0.5, att: false },           // a defence nobody could have held
      { t: 1, reg: 0, odds: W, att: true },                  // an attack that should have gone in
      { t: 1, reg: 0, odds: W - 0.1, att: true },            // an attack that was a gamble
      { t: 1, reg: 0, odds: W + 0.1, att: true, even: 1.7 }, // an attack on prepared positions at less than an even chance
    ] } } as unknown as General;
    expect(heldAgainst(gen).map((b) => [b.odds, b.att])).toEqual([[1.1, false], [1.4, false], [W, true]]);
    expect(heldAgainst({ rec: {} } as unknown as General)).toEqual([]);
  });

  it('a general\'s defeats are written down with their odds in the grand campaign, wiped by a win, and not at all in the historical scenarios', () => {
    const fight = (g: Game, gen: General, c: Corps, o: { taken: boolean; attacking: boolean; odds: number }): void => {
      const target = g.regions[c.reg];
      const unit = { name: c.name, nat: c.nat, kind: c.kind, divs: c.divs, men: c.divs * MEN_PER_DIV, lost: 1000, fate: '' };
      const rep = { id: g.reports.length + 500, turn: g.turn, region: target.id, regionName: target.name, attNat: o.attacking ? c.nat : 'POL', defNat: o.attacking ? 'POL' : c.nat,
        attackers: o.attacking ? [unit] : [], defenders: o.attacking ? [] : [unit], taken: o.taken, level: 'Marginal victory' } as unknown as BattleReport;
      if (o.attacking) afterBattle(g, rep, target, o.odds, [c], [gen], [], []); else afterBattle(g, rep, target, o.odds, [], [], [c], [gen]);
    };
    const g = playing('GER');
    const gen = g.generals.find((x) => x.nat === 'GER' && x.status === 'active' && x.corps >= 0)!, c = corpsOf(g, gen)!;
    fight(g, gen, c, { taken: true, attacking: false, odds: 2.64 });
    fight(g, gen, c, { taken: false, attacking: true, odds: 1.8 });
    expect(gen.rec.lost).toEqual([{ t: g.turn, reg: c.reg, odds: 2.64, att: false, even: 1.2 }, { t: g.turn, reg: c.reg, odds: 1.8, att: true, even: 1.2 }]);
    // Only the second could have been won.
    expect(heldAgainst(gen)).toHaveLength(1);
    // No more than six are kept, and a win wipes the slate.
    for (let k = 0; k < 8; k++) fight(g, gen, c, { taken: true, attacking: false, odds: 1.1 });
    expect(gen.rec.lost).toHaveLength(6);
    fight(g, gen, c, { taken: false, attacking: false, odds: 1.1 });
    expect(gen.rec.lost).toBeUndefined();
    // History follows its own course: nothing is written down there.
    const h = createGame(1, null);
    const hg = h.generals.find((x) => x.status === 'active' && x.corps >= 0)!, hc = corpsOf(h, hg)!;
    fight(h, hg, hc, { taken: true, attacking: false, odds: 1.1 });
    expect(hg.rec.lost).toBeUndefined();
  });

  it('two hopeless defences are no cause: nobody is named, however many months pass', () => {
    const g = playing('GER'), st = govOf(g, 'GER')!;
    delete st.dir;
    const gen = g.generals.find((x) => x.nat === 'GER' && x.status === 'active' && x.corps >= 0)!;
    gen.rec.streak = 3;
    gen.rec.lost = [{ t: g.turn, reg: 0, odds: 2.4, att: false }, { t: g.turn, reg: 0, odds: 3.1, att: false }, { t: g.turn, reg: 0, odds: 1.2, att: true }];
    st.headNext = 0;
    for (let k = 0; k < 24; k++) { g.turn++; st.next = g.turn + 99; govMonth(g, 'GER'); }
    expect(st.head).toBeUndefined();
    // One more that could have been held, with one already on the slate, and the cabinet has its cause.
    gen.rec.lost.push({ t: g.turn, reg: 0, odds: 1.1, att: false }, { t: g.turn, reg: 0, odds: 1.15, att: false });
    for (let k = 0; k < 24 && !st.head; k++) { g.turn++; st.next = g.turn + 99; govMonth(g, 'GER'); }
    expect(st.head?.general).toBe(gen.id);
    expect(st.head!.lost).toHaveLength(2);
    expect(st.head!.lost!.every((b) => b.odds < 1.2)).toBe(true);
  });
});

describe('what the cabinet asks for', () => {
  it('a table of doves never sends the army at a town: with nothing else to ask for, it asks for nothing', () => {
    for (const seed of [3, 7, 11, 13]) {
      const g = playing('GER', seed), st = govOf(g, 'GER')!;
      st.seats = { ...DOVES };
      for (let k = 0; k < 6; k++) {
        const d = ask(g, st);
        expect(d?.kind, `seed ${seed}`).not.toBe('take');
        if (d) st.lastKind = d.kind;
        g.turn++;
      }
    }
  });

  it('a balanced table that has just asked for a town asks for something quiet next, unless an ally is calling for help', () => {
    const g = playing('GER'), st = govOf(g, 'GER')!;
    expect(governmentOf(g)!.wings.top).toBe('balanced');
    st.lastKind = 'take';
    const d = ask(g, st)!;
    expect(d).toBeDefined();
    expect(['raise', 'build', 'agent', 'trade', 'hold']).toContain(d.kind);
  });

  it('a country at peace is given small jobs it can pay for, and doing the job carries the directive out', () => {
    const g = playing('ITA'), st = govOf(g, 'ITA')!, n = g.nations.ITA;
    expect(n.atWar).toEqual([]);
    n.pp = 400;
    const kinds = new Set<string>();
    for (let k = 0; k < 12 && kinds.size < 2; k++) {
      const d = ask(g, st);
      if (d) { kinds.add(d.kind); st.lastKind = d.kind; }
      g.turn++;
    }
    expect([...kinds].every((x) => ['raise', 'build', 'agent', 'trade'].includes(x))).toBe(true);
    // Build up a town: one more building, standing or ordered.
    const b = playing('ITA'), bs = govOf(b, 'ITA')!;
    b.nations.ITA.pp = 400;
    let d: Directive | undefined;
    for (let k = 0; k < 40 && d?.kind !== 'build'; k++) { bs.lastKind = 'raise'; d = ask(b, bs); if (d?.kind !== 'build') b.turn++; }
    expect(d?.kind).toBe('build');
    expect(d!.title).toBe('Build up a town');
    expect(d!.text).toMatch(/order a new building in one of your towns by the end of .+\. The cheapest today is the .+ at .+, for /);
    expect(governmentOf(b)!.directive!.progress).toMatch(/building/i);
    const pc = b.nations.ITA.pc;
    const site = b.regions.filter((r) => r.ctrl === 'ITA' && r.owner === 'ITA').flatMap((r) => civicOffers(b, 'ITA', r.id).filter((o) => !o.why).map((o) => ({ r, o }))).sort((x, y) => x.o.cost - y.o.cost)[0];
    expect(site).toBeDefined();
    expect(orderBuilding(b, site.r.id, site.o.line)).not.toBeNull();
    judgeDirective(b, 'ITA');
    expect(bs.dir).toBeUndefined();
    expect(bs.done).toBe(1);
    expect(b.nations.ITA.pc).toBeGreaterThanOrEqual(Math.min(pc + GOV.directives.build.capital, pc));
    // Recruit an agent: one more in the service.
    const a = playing('ITA'), as = govOf(a, 'ITA')!;
    a.nations.ITA.pp = 400;
    d = undefined;
    for (let k = 0; k < 40 && d?.kind !== 'agent'; k++) { as.lastKind = 'raise'; d = ask(a, as); if (d?.kind !== 'agent') a.turn++; }
    expect(d?.kind).toBe('agent');
    expect(d!.title).toBe('Recruit an agent');
    expect(d!.target).toBe(agentsOf(a, 'ITA').length + 1);
    expect(orderRecruitAgent(a)).not.toBeNull();
    judgeDirective(a, 'ITA');
    expect(as.dir).toBeUndefined();
    expect(as.done).toBe(1);
  });

  it('a job to build up the army drops its peacetime opening once the country is at war', () => {
    const g = playing('ITA'), st = govOf(g, 'ITA')!;
    st.dir = { id: 1, nat: 'ITA', kind: 'raise', from: 'ITA', issued: g.turn, due: g.turn + 5, capital: 5, money: 0, target: 999, title: 'Build up the army',
      text: 'While the country is at peace, the war cabinet wants the army built up: bring it from 10 to 999 divisions.' };
    expect(governmentOf(g)!.directive!.text).toMatch(/^While the country is at peace, the war cabinet wants/);
    g.nations.ITA.atWar.push('FRA'); g.nations.FRA.atWar.push('ITA');
    expect(governmentOf(g)!.directive!.text).toMatch(/^The war cabinet wants the army built up/);
  });

  it('a place is ordered held only where an enemy who has lately attacked could bring a real blow against it', () => {
    const H = GOV.directives.hold;
    // Germany in September 1939: France stands on the frontier and has attacked nobody. No hold order, month after month.
    const g = playing('GER'), st = govOf(g, 'GER')!;
    for (const n of Object.values(g.nations)) n.lastFight = {};
    for (let k = 0; k < 8; k++) {
      const d = ask(g, st);
      expect(d?.kind).not.toBe('hold');
      if (d) st.lastKind = d.kind === 'take' ? undefined : d.kind;
      g.turn++;
    }
    // The threat to a place of his is the enemy's own attack card against it.
    const mine = g.regions.filter((r) => r.ctrl === 'GER').map((r) => ({ r, t: threatOdds(g, 'GER', r.id) })).filter((x) => x.t);
    expect(mine.length).toBeGreaterThan(0);
    for (const x of mine) { expect(x.t!.odds).toBeGreaterThan(0); expect(atWar(g, 'GER', x.t!.by)).toBe(true); }
    expect(H.threat).toBeLessThan(H.lost);
    expect(H.fresh).toBeGreaterThan(0);
  });

  it('with the enemy on the move against a place he could carry, the order comes, and says what he could bring', () => {
    const H = GOV.directives.hold;
    const g = playing('GER'), st = govOf(g, 'GER')!;
    const size = (r: Region): boolean => !!r.fort || !!r.oil || r.cap || r.vp >= 2;
    const x = g.regions.filter((r) => r.ctrl === 'GER' && size(r)).map((r) => ({ r, t: threatOdds(g, 'GER', r.id) })).filter((y) => y.t && y.t.odds > 0)[0];
    expect(x).toBeDefined();
    // Bring the odds to about 2 to 1: a real blow, and not a lost cause.
    const k = 2 / x.t!.odds;
    if (k < 1) { for (const c of field(g, x.t!.by)) c.str *= k; } else for (const c of x.r.units) if (c.nat === 'GER') c.str /= k;
    const now = threatOdds(g, 'GER', x.r.id)!;
    expect(now.odds).toBeGreaterThanOrEqual(H.threat);
    expect(now.odds).toBeLessThanOrEqual(H.lost);
    let d: Directive | undefined;
    const draw = (fresh: boolean): boolean => {
      for (let n = 0; n < 40; n++) {
        for (const foe of Object.values(g.nations)) foe.lastFight = fresh && foe.id === now.by ? { GER: g.turn } : {};
        st.lastKind = 'take';
        d = ask(g, st);
        if (d?.kind === 'hold') return true;
        g.turn++;
      }
      return false;
    };
    // An enemy who has not fired a shot in months is not about to come.
    expect(draw(false)).toBe(false);
    expect(draw(true)).toBe(true);
    expect(d!.text).toMatch(/ is on the move, and its corps in reach could attack the place at (about [\d.]+|better than 10) to 1\. The war cabinet wants it held to the end of /);
    expect(d!.text).toContain('If the enemy never comes, the order is withdrawn.');
    expect(governmentOf(g)!.directive!.oddsLine).toMatch(/ could attack it today at (about [\d.]+|better than 10) to 1 with its corps in reach\.$/);
  });

  it('the trust on the card is rounded down, so a cabinet shown at the mark has not fallen below it', () => {
    const g = playing('GER'), st = govOf(g, 'GER')!;
    const form = GOV.forms[formOf('GER')];
    st.trust = form.fallsAt + 0.96;
    expect(governmentOf(g)!.trust).toBe(form.fallsAt);
    st.trust = form.fallsAt + 1.02;
    expect(governmentOf(g)!.trust).toBe(form.fallsAt + 1);
  });
});

describe('a say over the call-up', () => {
  it('calling the next class up early brings months of men at once, and then no class comes of age for a while', () => {
    const E = MANPOWER.early;
    const g = playing('GER');
    expect(g.nations.GER.atWar.length).toBeGreaterThan(0);
    // Spend some of the reserve first: a full one has nowhere to put them.
    expect(whyNotOrderEarlyClass(g)).toBe('The reserve is full: there is nowhere to put them.');
    g.flags['mp:GER'] = poolAtStart(g, 'GER') * 0.3;
    expect(whyNotOrderEarlyClass(g)).toBeNull();
    const v = callUpOf(g)!;
    expect(v.early).toMatchObject({ months: E.months, gap: E.gap, why: null });
    expect(v.early.men).toBeGreaterThan(0);
    expect(v.early.text).toContain('Call up the next class early');
    const before = menLeft(g, 'GER'), h = g.headlines.length;
    expect(orderEarlyClass(g)).toBe(true);
    expect((menLeft(g, 'GER') - before) * MEN_PER_DIV).toBeCloseTo(v.early.men, -1);
    expect(classGapUntil(g, 'GER')).toBe(g.turn + E.gap);
    expect(g.headlines.slice(h).some((x) => /The next class is called up early/.test(x.text))).toBe(true);
    // Not twice: the boys are with the colours already, and for those months nobody comes of age.
    expect(orderEarlyClass(g)).toBe(false);
    expect(whyNotOrderEarlyClass(g)).toMatch(/^The next class is with the colours already: none comes of age until /);
    expect(manpowerOf(g)!.gap).toBe(E.gap);
    const have = menLeft(g, 'GER');
    g.turn += E.gap;
    expect(classArrives(g, 'GER')).toBe(0);
    expect(menLeft(g, 'GER')).toBe(have);
    g.turn += 1;
    expect(classArrives(g, 'GER')).toBeGreaterThan(0);
    expect(whyNotOrderEarlyClass(g)).toBeNull();
    // It is a trade, not a gift: what comes early is no more than what the quiet months would have brought.
    expect(E.months).toBeLessThanOrEqual(E.gap);
  });

  it('a country at peace calls nobody up early, the historical scenarios have no such order, and the computer never uses it', () => {
    const g = playing('ITA');
    expect(whyNotOrderEarlyClass(g)).toBe('Only a country at war calls its boys up before their time.');
    expect(orderEarlyClass(g)).toBe(false);
    expect(callUpOf(g)!.callUp).toBeNull();
    const h = createGame(1, null);
    expect(callUpOf(h)).toBeNull();
    const w = free(5);
    for (let k = 0; k < 14; k++) runTurnAI(w);
    expect(Object.keys(w.flags).filter((k) => k.startsWith('mpe:') || k.startsWith('mph'))).toEqual([]);
  });

  it('the ledger adds up: what the reserve opened with, and the men who came of age, less the men drawn, is what stands in it', () => {
    const g = playing('SOV', 5);
    for (let k = 0; k < 4; k++) { while (!humanUp(g)) stepAI(g); beginHuman(g); if (k < 3) { g.flags['mp:SOV'] = menLeft(g, 'SOV') - 7; g.flags['mpx:SOV'] = (g.flags['mpx:SOV'] ?? 0) + 7; runTurnAI(g); } }
    const v = manpower(g, 'SOV')!;
    expect(v.drawn).toBeGreaterThan(0);
    expect(Math.abs(Math.round(v.start * MEN_PER_DIV) + v.came - v.drawn - v.men)).toBeLessThanOrEqual(2);
  });

  it('the standing call-up can be held: no new corps are raised, the waves wait, and they pick up where they stopped', { timeout: 60_000 }, () => {
    const g = playing('SOV', 5);
    // Bring the Soviet Union into a war, so that its call-up runs.
    const count = (x: Game): number => x.corps.filter((c) => c.nat === 'SOV' && !c.dead).length;
    const war = (x: Game): void => { if (!x.nations.SOV.atWar.includes('GER')) { x.nations.SOV.atWar.push('GER'); x.nations.GER.atWar.push('SOV'); } };
    war(g);
    const v = callUpOf(g)!;
    expect(v.callUp).not.toBeNull();
    expect(v.callUp!.held).toBe(false);
    expect(v.callUp!.text).toContain('The standing call-up is running');
    const held = copy(g), ran = copy(g);
    expect(orderCallUp(held, true)).toBe(true);
    expect(orderCallUp(held, true)).toBe(false);
    expect(callUpOf(held)!.callUp!.held).toBe(true);
    expect(callUpOf(held)!.callUp!.text).toContain('The standing call-up is held');
    const month = (x: Game): void => { endHuman(x); for (let k = 0; !humanUp(x) && k < 80; k++) stepAI(x); beginHuman(x); };
    const h0 = count(held), r0 = count(ran);
    for (let k = 0; k < 2; k++) { month(held); month(ran); }
    // The side that let it run has raised its waves; the side that held it has raised none of them.
    expect(count(ran) - r0).toBeGreaterThan(count(held) - h0);
    expect(held.flags['mphn:SOV']).toBe(2);
    // Let go, the waves start where they stopped: the first wave is still to come.
    const next = callUpOf(held)!.callUp!;
    expect(orderCallUp(held, false)).toBe(true);
    expect(callUpOf(held)!.callUp!.held).toBe(false);
    expect(next.corps).toBeGreaterThan(0);
    // A save keeps the order.
    const again = copy(held);
    expect(orderCallUp(again, true)).toBe(true);
    expect(callUpOf(copy(again))!.callUp!.held).toBe(true);
  });
});

describe('your office is secure: a government that cannot fall', () => {
  /** A British player with ministers, decrees and capital to lose, and nothing else due. */
  function seated(): { g: Game; st: GovState } {
    const g = playing('UK');
    const st = govOf(g, 'UK')!;
    delete st.dir;
    st.next = g.turn + 99; st.headNext = g.turn + 99;
    st.seats = { staff: 'organiser', finance: 'taxman' };
    st.steps = { economy: 2, service: 1 };
    g.nations.UK.pc = 17;
    return { g, st };
  }
  const fell = (g: Game, from: number): boolean => g.headlines.slice(from).some((h) => /The government falls/.test(h.text));
  const starve = (g: Game, st: GovState): void => { st.trust = 0; st.next = g.turn + 99; govMonth(g, 'UK'); };

  it('with his office secure a cabinet with no trust never falls, and it still withholds what a doubting cabinet withholds', () => {
    const { g, st } = seated();
    expect(officeIsSecure(g)).toBe(false);
    expect(governmentOf(g)!.secure).toBe(false);
    expect(orderOffice(g, false)).toBe(true);
    expect(orderOffice(g, false)).toBe(false);
    expect(officeIsSecure(g)).toBe(true);
    const h0 = g.headlines.length;
    for (let k = 0; k < GOV.trust.crisisMonths * 4; k++) { starve(g, st); expect(st.low).toBe(0); }
    expect(fell(g, h0)).toBe(false);
    expect(st.seats).toEqual({ staff: 'organiser', finance: 'taxman' });
    expect(st.steps).toEqual({ economy: 2, service: 1 });
    expect(g.nations.UK.pc).toBe(17);
    // Nothing else about the cabinet is switched off: it has no trust in him, says so, and holds money back.
    const v = governmentOf(g)!;
    expect(v.secure).toBe(true);
    expect(v.trustWord).toBe('None left');
    expect(v.doubts).toBe(true);
    expect(v.withheld).toMatch(/withholds/);
    // The taxman adds to the income and the doubt takes its share off: without the doubt it would be more.
    const doubted = govMul(g, 'UK', 'income');
    st.trust = 60;
    expect(govMul(g, 'UK', 'income')).toBeCloseTo(doubted / (1 - GOV.capital.lowIncome), 6);
    // A save keeps the setting.
    expect(officeIsSecure(copy(g))).toBe(true);
    expect(governmentOf(copy(g))!.secure).toBe(true);
  });

  it('switched back, the cabinet can fall again, and the months are counted from that day', () => {
    const { g, st } = seated();
    // Two months without trust, one short of the fall.
    for (let k = 0; k < GOV.trust.crisisMonths - 1; k++) starve(g, st);
    expect(st.low).toBe(GOV.trust.crisisMonths - 1);
    expect(orderOffice(g, false)).toBe(true);
    expect(st.low).toBe(0);
    const h0 = g.headlines.length;
    for (let k = 0; k < 6; k++) starve(g, st);
    expect(fell(g, h0)).toBe(false);
    expect(orderOffice(g, true)).toBe(true);
    expect(orderOffice(g, true)).toBe(false);
    expect(officeIsSecure(g)).toBe(false);
    // It takes the full count again, not one month more.
    for (let k = 0; k < GOV.trust.crisisMonths - 1; k++) { starve(g, st); expect(fell(g, h0)).toBe(false); }
    starve(g, st);
    expect(fell(g, h0)).toBe(true);
    expect(st.seats).toEqual({});
    expect(g.nations.UK.pc).toBe(0);
  });

  it('it is a setting of its own: the cabinet switch does not move it, and it does not move the cabinet', () => {
    const { g } = seated();
    expect(orderOffice(g, false)).toBe(true);
    expect(cabinetIsOff(g)).toBe(false);
    expect(governmentOf(g)).not.toBeNull();
    expect(orderCabinet(g, false)).toBe(true);
    expect(officeIsSecure(g)).toBe(true);
    expect(orderCabinet(g, true)).toBe(true);
    expect(officeIsSecure(g)).toBe(true);
    expect(governmentOf(g)!.secure).toBe(true);
    // And with the cabinet off it can still be set, for the day the cabinet comes back.
    expect(orderCabinet(g, false)).toBe(true);
    expect(orderOffice(g, true)).toBe(true);
    expect(orderCabinet(g, true)).toBe(true);
    expect(governmentOf(g)!.secure).toBe(false);
  });

  it('the historical scenarios and a war nobody plays have no such setting', () => {
    const hist = createGame(3, 'GER');
    expect(orderOffice(hist, false)).toBe(false);
    expect(officeIsSecure(hist)).toBe(false);
    expect(hist.flags.nofall).toBeUndefined();
    const watched = free(3);
    expect(orderOffice(watched, false)).toBe(false);
    expect(officeIsSecure(watched)).toBe(false);
  });
});
