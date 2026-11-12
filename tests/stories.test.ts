// Unit stories: service records, honours and what the player does with them, veterans and drafts, the memorial
// wall, home ties, rivalries and letters. Every unit keeps a record; only the player's own units fight differently for it.
import { describe, expect, it } from 'vitest';
import { createGame, removeCorps } from '../src/campaign/game';
import { COMMAND, gainXp, generalOf, rankOf } from '../src/campaign/commanders';
import { cancelBuild, deliver } from '../src/campaign/economy';
import { loadGame, saveGame } from '../src/campaign/save';
import {
  STORIES, awardTitle, cashHonour, cashWorth, citation, designation, diaryText, drafted, fameOf, grade, honourDef, honoursOf, isGuards,
  newStories, raisedXp, revivable, stat, storyAir, storyAttack, storyBattle, storyConvoy, storyDefence, storyEdge, storyFallen,
  storyMonth, storySea, training, unseen, whyNotCash,
} from '../src/campaign/stories';
import { beginHuman, canOrder, humanUp, orderBuildCorps, orderCash, orderDrafts, orderSeen, runTurnAI, stepAI } from '../src/campaign/turn';
import type { Award, BattleReport, Corps, Game, Region, Storied, UnitKind } from '../src/campaign/types';

function asPlayer(nat: string, seed = 7): Game {
  const g = createGame(seed, nat);
  let guard = 0;
  while (!humanUp(g) && guard++ < 60) stepAI(g);
  beginHuman(g);
  return g;
}

const corps = (g: Game, name: string): Corps => g.corps.find((c) => c.name === name)!;
const field = (g: Game, nat: string): Corps[] => g.corps.filter((c) => c.nat === nat && c.kind !== 'fort' && !c.dead).sort((a, b) => b.divs - a.divs || a.id - b.id);
const has = (g: Game, u: Storied, hon: string): Award | undefined => honoursOf(g, u).find((a) => a.hon === hon);

/** Puts an honour on a unit by hand, as the rules would file it. */
function give(g: Game, kind: UnitKind, u: Storied & { id: number; name: string; nat: string }, hon: string, at = -1): Award {
  const a: Award = { id: g.stories.awards.length + 1, turn: g.turn, hon, kind, unit: u.id, name: u.name, nat: u.nat, at };
  g.stories.awards.push(a);
  (u.hon ??= []).push(a.id);
  return a;
}

let battles = 0;

/** A land battle as the rules would report it, fed straight into the records. */
function fight(g: Game, attackers: Corps[], defenders: Corps[], target: Region, o: { win?: boolean; level?: string; preOdds?: number; prisoners?: number; capital?: boolean; fate?: string } = {}): BattleReport {
  const win = o.win ?? true;
  const unit = (c: Corps, fate: string) => ({ name: c.name, nat: c.nat, kind: c.kind, divs: c.divs, men: 30000, lost: 1000, fate });
  const rep = {
    id: ++battles, turn: g.turn, week: 1, region: target.id, regionName: target.name, attNat: attackers[0].nat, defNat: defenders[0]?.nat ?? target.ctrl,
    lines: [], aPower: 10, dPower: 5, odds: 2.5, level: o.level ?? (win ? 'Decisive victory' : 'Close defeat'), taken: win, attLost: 0, defLost: 0, prisoners: o.prisoners ?? 0,
    attackers: attackers.map((c) => unit(c, o.fate ?? '')), defenders: defenders.map((c) => unit(c, '')),
  } as unknown as BattleReport;
  storyBattle(g, rep, target, o.preOdds ?? 2.5, attackers, defenders, { capital: !!o.capital });
  return rep;
}

describe('every unit keeps a record', () => {
  it('battles go into the records and diaries of both sides, and a capital goes on the colours of the corps that marched in', () => {
    const g = createGame(1, null);
    for (let i = 0; i < 3; i++) runTurnAI(g);
    const fought = g.corps.filter((c) => c.nat === 'GER' && (c.rec?.battles ?? 0) > 0);
    expect(fought.length).toBeGreaterThan(5);
    for (const c of fought) {
      expect(c.log!.length).toBeGreaterThan(0);
      for (const line of c.log!) { const s = diaryText(g, c, line); expect(s).toMatch(/\.$/); expect(s).not.toContain('{'); expect(s).not.toContain('undefined'); }
      expect(c.rec!.won ?? 0).toBeLessThanOrEqual(c.rec!.battles);
    }
    const warsaw = g.stories.awards.filter((a) => a.hon === 'colours' && awardTitle(g, a) === 'Warsaw 1939');
    expect(warsaw.length).toBeGreaterThan(0);
    // Not every corps that fired a shot at the capital: only those that marched in.
    expect(warsaw.length).toBeLessThanOrEqual(5);
    expect(citation(g, warsaw[0])).toContain('Warsaw');
    // The first of a great power's formations to earn such a name is news even when nobody is playing.
    expect(g.headlines.some((h) => h.kind === 'honour' && h.award !== undefined)).toBe(true);
    // The Polish dead are on the wall.
    expect(g.stories.fallen.some((f) => f.nat === 'POL')).toBe(true);
  });

  it('a corps knows when and where it was raised', () => {
    const g = createGame(1, null);
    const c = corps(g, 'XIX Corps');
    expect(c.born).toBe(1);
    expect(g.regions[c.home!].owner).toBe('GER');
    g.turn = 13;
    expect(stat(g, c, 'service')).toBe(12);
  });

  it('experience has a name', () => {
    expect(grade(0)).toBe('Green');
    expect(grade(0.2)).toBe('Trained');
    expect(grade(0.45)).toBe('Seasoned');
    expect(grade(0.6)).toBe('Veteran');
    expect(grade(0.95)).toBe('Elite');
  });

  it('nothing is written for watching history that only a player would see', () => {
    const g = createGame(3, null);
    for (let i = 0; i < 6; i++) runTurnAI(g);
    expect(g.headlines.some((h) => h.kind === 'letter')).toBe(false);
    expect(g.stories.rivals).toEqual([]);
    expect(g.stories.mail).toEqual([]);
    expect(g.stories.schools).toEqual({});
    expect(unseen(g)).toEqual([]);
  });
});

describe('honours', () => {
  it('five breakthroughs make a Spearhead: a ribbon, a name from the army, news, and a share for the general', () => {
    const g = asPlayer('GER');
    const c = corps(g, 'XIX Corps');
    const foe = field(g, 'POL')[0];
    const t = g.regions[foe.reg];
    const gen = generalOf(g, c)!;
    const before = gen.xp + gen.level * 100000;
    for (let i = 0; i < 4; i++) fight(g, [c], [foe], t);
    expect(has(g, c, 'spearhead')).toBeUndefined();
    expect(c.rec!.stormed).toBe(4);
    fight(g, [c], [foe], t);
    const a = has(g, c, 'spearhead')!;
    expect(a).toBeDefined();
    expect(a.first).toBe(true);
    expect(fameOf(a)).toBe(honourDef('spearhead')!.fame + 1);
    expect(c.nick).toBeTruthy();
    expect(g.headlines.some((h) => h.kind === 'honour' && h.award === a.id && h.text.includes(c.nick!))).toBe(true);
    expect(gen.xp + gen.level * 100000).toBeGreaterThan(before);
    expect(unseen(g).map((x) => x.id)).toContain(a.id);
    orderSeen(g, a.id);
    expect(unseen(g).map((x) => x.id)).not.toContain(a.id);
    // The diary says so.
    expect(c.log!.some((l) => l[1] === 'honour' && l[3] === a.id)).toBe(true);
    // Once is enough: a sixth breakthrough is not a second Spearhead.
    fight(g, [c], [foe], t);
    expect(honoursOf(g, c).filter((x) => x.hon === 'spearhead').length).toBe(1);
  });

  it('a close win is not a breakthrough, and a beaten attack earns nothing', () => {
    const g = asPlayer('GER');
    const c = corps(g, 'XIX Corps');
    const foe = field(g, 'POL')[0];
    const t = g.regions[foe.reg];
    for (let i = 0; i < 6; i++) fight(g, [c], [foe], t, { level: 'Close victory' });
    for (let i = 0; i < 6; i++) fight(g, [c], [foe], t, { win: false });
    expect(c.rec!.stormed).toBeUndefined();
    expect(c.rec!.battles).toBe(12);
    expect(c.rec!.won).toBe(6);
    expect(has(g, c, 'spearhead')).toBeUndefined();
  });

  it('standing twice against the odds makes a Rock, named for the place', () => {
    const g = asPlayer('GER');
    const c = corps(g, 'XIX Corps');
    const foe = field(g, 'POL')[0];
    const t = g.regions[c.reg];
    fight(g, [foe], [c], t, { win: false, preOdds: 0.8 });
    fight(g, [foe], [c], t, { win: false, preOdds: 1.4 });
    expect(has(g, c, 'rock')).toBeUndefined();
    fight(g, [foe], [c], t, { win: false, preOdds: 1.1 });
    expect(awardTitle(g, has(g, c, 'rock')!)).toBe(`Held ${t.name}`);
    // Three attacks beaten off in one month at one place are one line in the diary.
    const held = c.log!.filter((l) => l[1] === 'held');
    expect(held.length).toBe(1);
    expect(diaryText(g, c, held[0])).toBe(`Held ${t.name} against attack 3 times.`);
  });

  it('a second honour makes a formation Elite, and Guards in the Red Army', () => {
    const g = asPlayer('GER');
    const c = corps(g, 'XIX Corps');
    const foe = field(g, 'POL')[0];
    give(g, 'corps', c, 'rock', c.reg);
    expect(isGuards(g, c)).toBe(false);
    for (let i = 0; i < 5; i++) fight(g, [c], [foe], g.regions[foe.reg]);
    expect(isGuards(g, c)).toBe(true);
    expect(awardTitle(g, has(g, c, 'guards')!)).toBe('Elite');
    expect(designation('SOV')).toBe('Guards');
    // The designation is not itself one of the honours it is counted from.
    expect(stat(g, c, 'honours')).toBe(2);
  });

  it('the computer\'s units earn honours too, but only the first of a great power is news', () => {
    const g = asPlayer('GER');
    const pole = field(g, 'POL')[0], brit = field(g, 'UK')[0], ger = corps(g, 'XIX Corps');
    const news = (nat: string): number => g.headlines.filter((h) => h.kind === 'honour' && h.nat === nat).length;
    for (let i = 0; i < 2; i++) fight(g, [ger], [pole], g.regions[pole.reg], { win: false, preOdds: 1.5 });
    expect(has(g, pole, 'rock')).toBeDefined();
    expect(news('POL')).toBe(0);
    for (let i = 0; i < 2; i++) fight(g, [ger], [brit], g.regions[brit.reg], { win: false, preOdds: 1.5 });
    expect(has(g, brit, 'rock')).toBeDefined();
    expect(news('UK')).toBe(1);
    const other = field(g, 'UK')[1];
    for (let i = 0; i < 2; i++) fight(g, [ger], [other], g.regions[other.reg], { win: false, preOdds: 1.5 });
    expect(has(g, other, 'rock')).toBeDefined();
    expect(news('UK')).toBe(1);
    // None of it is the player's to read or decide.
    expect(unseen(g)).toEqual([]);
  });

  it('every honour in the data can be read out', () => {
    const g = createGame(1, null);
    const kinds = new Set(['corps', 'air', 'fleet']);
    expect(STORIES.honours.length).toBeGreaterThanOrEqual(20);
    for (const d of STORIES.honours) {
      expect(kinds.has(d.for)).toBe(true);
      expect(d.ribbon.length).toBe(2);
      expect(d.real.length).toBeGreaterThan(20);
      const u = d.for === 'corps' ? g.corps[0] : d.for === 'air' ? g.airFleets[0] : g.fleets[0];
      const a: Award = { id: 1, turn: 10, hon: d.id, kind: d.for, unit: u.id, name: u.name, nat: u.nat, at: 3, n: 4, rival: 'the other lot' };
      for (const s of [awardTitle(g, a), citation(g, a)]) { expect(s.length).toBeGreaterThan(3); expect(s).not.toContain('{'); expect(s).not.toContain('undefined'); }
    }
    // The example the whole idea started from: twenty-five missions is a tour.
    expect(honourDef('tour')!.needs).toEqual(['missions', 25]);
  });
});

describe('only the player\'s own units fight differently for their stories', () => {
  it('an honour carried at the front is an edge for the player and nothing for anyone else', () => {
    const g = createGame(1, 'GER');
    const c = corps(g, 'XIX Corps');
    const t = g.byName['Warsaw'];
    give(g, 'corps', c, 'spearhead', t.id);
    expect(storyAttack(g, c, t, [])).toBeCloseTo(1.05, 6);
    expect(storyDefence(g, c, t, [])).toBe(1);
    g.human = 'UK';
    expect(storyAttack(g, c, t, [])).toBe(1);
    g.human = null;
    expect(storyAttack(g, c, t, [])).toBe(1);
    expect(storyDefence(g, c, g.regions[c.home!], [])).toBe(1);
  });

  it('honours add up only so far', () => {
    const g = createGame(1, 'GER');
    const c = corps(g, 'XIX Corps');
    const t = g.byName['Warsaw'];
    for (const h of ['spearhead', 'net', 'desert', 'firstwave', 'oldguard', 'colours']) give(g, 'corps', c, h, t.id);
    expect(storyAttack(g, c, t, [])).toBeCloseTo(1 + STORIES.edgeCap, 6);
  });

  it('a corps defends the place it was raised in harder, and attacks harder to win it back', () => {
    const g = createGame(1, 'GER');
    const c = corps(g, 'XIX Corps');
    const home = g.regions[c.home!];
    expect(storyDefence(g, c, home, [])).toBeCloseTo(1 + STORIES.home.defence, 6);
    expect(storyAttack(g, c, home, [])).toBeCloseTo(1 + STORIES.home.attack, 6);
    expect(storyAttack(g, c, g.byName['Warsaw'], [])).toBe(1);
    const fort = g.corps.find((x) => x.nat === 'GER' && x.kind === 'fort');
    if (fort) { fort.home = fort.reg; expect(storyDefence(g, fort, g.regions[fort.reg], [])).toBe(1); }
  });

  it('air fleets and fleets carry theirs the same way', () => {
    const g = createGame(1, 'GER');
    const mine = g.airFleets.find((f) => f.nat === 'GER')!, theirs = g.airFleets.find((f) => f.nat === 'UK')!;
    give(g, 'air', mine, 'tour');
    give(g, 'air', theirs, 'tour');
    expect(storyEdge(g, mine)).toBeCloseTo(1.04, 6);
    expect(storyEdge(g, theirs)).toBe(1);
    const fleet = g.fleets.find((f) => f.nat === 'GER')!;
    give(g, 'fleet', fleet, 'tonnage');
    expect(storyEdge(g, fleet)).toBeCloseTo(1.03, 6);
  });
});

describe('veterans and replacements', () => {
  it('green drafts thin out what the player\'s corps knows; the computer\'s are untouched', () => {
    const g = createGame(1, 'GER');
    const c = corps(g, 'XIX Corps');
    c.xp = 0.8;
    drafted(g, c, 0.5, 0.5);
    const w = 0.5 * STORIES.veterans.dilute;
    expect(c.xp).toBeCloseTo((0.8 * 0.5 + STORIES.veterans.green * w) / (0.5 + w), 6);
    expect(c.log!.at(-1)![1]).toBe('drafts');
    expect(diaryText(g, c, c.log!.at(-1)!)).toBe('Took in drafts: 50% of the corps is new to it.');
    const p = field(g, 'POL')[0];
    p.xp = 0.8;
    drafted(g, p, 0.5, 0.5);
    expect(p.xp).toBe(0.8);
    expect(p.log).toBeUndefined();
  });

  it('a handful of replacements is not worth a line in the diary', () => {
    const g = createGame(1, 'GER');
    const c = corps(g, 'XIX Corps');
    c.xp = 0.8;
    drafted(g, c, 0.95, 0.05);
    expect(c.xp).toBeLessThan(0.8);
    expect(c.xp).toBeGreaterThan(0.77);
    expect(c.log).toBeUndefined();
  });

  it('drafts trained by veterans thin it less', () => {
    const g = createGame(1, 'GER');
    const a = corps(g, 'XIX Corps');
    a.xp = 0.8;
    drafted(g, a, 0.5, 0.5);
    const plain = a.xp;
    a.xp = 0.8;
    g.stories.schools.GER = 6;
    expect(training(g, 'GER')).toBeCloseTo(STORIES.veterans.green + 6 * STORIES.veterans.perSchool, 6);
    expect(raisedXp(g, 'GER')).toBeCloseTo(STORIES.veterans.raised + 6 * STORIES.veterans.perSchool, 6);
    drafted(g, a, 0.5, 0.5);
    expect(a.xp).toBeGreaterThan(plain);
  });

  it('a corps told to keep its veterans together takes no drafts', () => {
    const g = createGame(7, 'GER');
    let guard = 0;
    while (!humanUp(g) && guard++ < 60) stepAI(g);
    const [a, b] = field(g, 'GER');
    a.str = 0.6; b.str = 0.6;
    a.xp = 0.7; b.xp = 0.7;
    expect(orderDrafts(g, a, false)).toBe(true);
    expect(orderDrafts(g, field(g, 'POL')[0], false)).toBe(false);
    g.nations.GER.pp = 500;
    beginHuman(g);
    expect(a.str).toBeCloseTo(0.6, 6);
    expect(a.xp).toBeCloseTo(0.7, 6);
    expect(b.str).toBeGreaterThan(0.6);
    expect(b.xp).toBeLessThan(0.7);
    orderDrafts(g, a, true);
    expect(a.lean).toBeUndefined();
  });
});

describe('what the player does with the fame', () => {
  it('a bond tour brings in production and political capital, and takes the corps out of the line', () => {
    const g = asPlayer('GER');
    const c = corps(g, 'XIX Corps');
    const t = g.byName['Warsaw'];
    const a = give(g, 'corps', c, 'spearhead', t.id);
    a.first = true;
    const n = g.nations.GER;
    n.pc = 0;
    const pp = n.pp, fame = fameOf(a);
    expect(storyAttack(g, c, t, [])).toBeCloseTo(1.05, 6);
    expect(whyNotCash(g, a, 'bonds')).toBe('');
    expect(cashWorth(g, a, 'bonds')).toBe(`+${STORIES.cash.bondsPp * fame} production points and +${STORIES.cash.bondsPc * fame} political capital`);
    expect(orderCash(g, a.id, 'bonds')).toBe(true);
    expect(n.pp).toBeCloseTo(pp + STORIES.cash.bondsPp * fame, 6);
    expect(n.pc).toBe(STORIES.cash.bondsPc * fame);
    expect(c.leave).toBe(g.turn + STORIES.cash.months);
    expect(g.regions[c.reg].owner).toBe('GER');
    expect(canOrder(g, c)).toBe(false);
    // The honour stays on the colours but no longer gives its edge.
    expect(has(g, c, 'spearhead')!.cash).toBe('bonds');
    expect(storyAttack(g, c, t, [])).toBe(1);
    // It can only be spent once.
    expect(orderCash(g, a.id, 'school')).toBe(false);
    expect(whyNotCash(g, a, 'school')).not.toBe('');
    // Two months later it is back.
    g.turn = c.leave!;
    storyMonth(g, 'GER');
    expect(c.leave).toBeUndefined();
    expect(c.log!.at(-1)![1]).toBe('back');
    expect(canOrder(g, c)).toBe(true);
  });

  it('instructors make every later draft and new corps better trained, at a cost to the corps that sends them', () => {
    const g = asPlayer('GER');
    const c = corps(g, 'XIX Corps');
    const a = give(g, 'corps', c, 'rock', c.reg);
    c.xp = 0.7;
    const fame = fameOf(a), was = training(g, 'GER'), raised = raisedXp(g, 'GER');
    expect(orderCash(g, a.id, 'school')).toBe(true);
    expect(g.stories.schools.GER).toBe(fame);
    expect(training(g, 'GER')).toBeCloseTo(was + STORIES.veterans.perSchool * fame, 6);
    expect(raisedXp(g, 'GER')).toBeCloseTo(raised + STORIES.veterans.perSchool * fame, 6);
    expect(c.xp).toBeCloseTo(0.7 - STORIES.cash.schoolXp * fame, 6);
    expect(c.leave).toBe(g.turn + STORIES.cash.months);
    // The schools can only use so many.
    g.stories.schools.GER = STORIES.veterans.maxSchools;
    const b = give(g, 'corps', corps(g, 'XV Corps') ?? field(g, 'GER')[1], 'rock');
    expect(whyNotCash(g, b, 'school')).toContain('schools');
    expect(whyNotCash(g, b, 'bonds')).toBe('');
  });

  it('an air fleet\'s crews can go home too; a fleet cannot be spared to the schools', () => {
    const g = asPlayer('GER');
    const f = g.airFleets.find((x) => x.nat === 'GER')!;
    const a = give(g, 'air', f, 'tour');
    const pilots = g.airPool.GER.pilots, ready = f.ready, fame = fameOf(a);
    expect(orderCash(g, a.id, 'school')).toBe(true);
    expect(g.airPool.GER.pilots).toBe(pilots + STORIES.cash.pilots * fame);
    expect(f.ready).toBeCloseTo(Math.max(0.3, ready - STORIES.cash.ready), 6);
    expect(storyEdge(g, f)).toBe(1);
    const fleet = g.fleets.find((x) => x.nat === 'GER')!;
    const s = give(g, 'fleet', fleet, 'tonnage');
    expect(whyNotCash(g, s, 'school')).toContain('fleet');
    expect(whyNotCash(g, s, 'bonds')).toBe('');
  });

  it('is refused for somebody else\'s unit, a corps that is cut off, and an honour inherited from the dead', () => {
    const g = asPlayer('GER');
    const brit = field(g, 'UK')[0];
    const theirs = give(g, 'corps', brit, 'rock');
    expect(whyNotCash(g, theirs, 'bonds')).not.toBe('');
    expect(cashHonour(g, theirs.id, 'bonds')).toBe(false);
    const c = corps(g, 'XIX Corps');
    const a = give(g, 'corps', c, 'rock');
    c.supplied = false;
    expect(whyNotCash(g, a, 'bonds')).toContain('cut off');
    c.supplied = true;
    a.old = true;
    expect(whyNotCash(g, a, 'bonds')).not.toBe('');
    expect(cashHonour(g, a.id, 'bonds')).toBe(false);
    expect(c.leave).toBeUndefined();
  });
});

describe('the memorial wall', () => {
  it('a destroyed corps is remembered with its record, and can be raised again under its old name', () => {
    const g = asPlayer('GER');
    const c = corps(g, 'XIX Corps');
    const { name, kind } = c;
    c.xp = 0.75;
    c.nick = 'the Flying Column';
    give(g, 'corps', c, 'spearhead', c.reg);
    c.rec = { battles: 9, won: 7 };
    storyFallen(g, c, 'destroyed', 'POL', c.reg);
    removeCorps(g, c);
    const f = g.stories.fallen.at(-1)!;
    expect(f.name).toBe(name);
    expect(f.nick).toBe('the Flying Column');
    expect(f.rec!.battles).toBe(9);
    expect(honoursOf(g, f).length).toBe(1);
    expect(g.headlines.at(-1)!.text).toContain('memorial wall');
    expect(revivable(g, 'GER', kind).map((x) => x.id)).toEqual([f.id]);
    expect(revivable(g, 'GER', kind === 'inf' ? 'arm' : 'inf')).toEqual([]);

    const n = g.nations.GER;
    n.pp = 500;
    const no = n.corpsNo;
    const at = g.regions[n.capital];
    const b = orderBuildCorps(g, kind, at.id, f.id)!;
    expect(b.name).toBe(name);
    expect(b.revive).toBe(f.id);
    expect(n.corpsNo).toBe(no);
    // It cannot be raised twice at once, and taking the order back gives the name back.
    expect(revivable(g, 'GER', kind)).toEqual([]);
    expect(orderBuildCorps(g, kind, at.id, f.id)).toBeNull();
    expect(cancelBuild(g, 'GER', b.id)).toBe(true);
    expect(n.corpsNo).toBe(no);
    expect(revivable(g, 'GER', kind).length).toBe(1);

    const again = orderBuildCorps(g, kind, at.id, f.id)!;
    g.turn = again.eta;
    deliver(g, 'GER');
    const heir = g.corps.find((x) => x.name === name && !x.dead)!;
    expect(heir).toBeDefined();
    expect(f.heir).toBe(heir.id);
    expect(heir.heir).toBe(f.id);
    expect(heir.nick).toBe('the Flying Column');
    // It wears the old ribbon, which gives no edge and cannot be spent: the new corps has its own name to make.
    const worn = honoursOf(g, heir);
    expect(worn.length).toBe(1);
    expect(worn[0].old).toBe(true);
    expect(storyAttack(g, heir, g.byName['Warsaw'], [])).toBe(1);
    expect(whyNotCash(g, worn[0], 'bonds')).not.toBe('');
    expect(unseen(g).some((a) => a.unit === heir.id)).toBe(false);
    // Part of what the old corps knew comes with the name.
    expect(heir.xp).toBeCloseTo(raisedXp(g, 'GER') + STORIES.legacy.xp * (0.75 - STORIES.veterans.green), 6);
    expect(heir.log!.at(-1)![1]).toBe('reraised');
    expect(revivable(g, 'GER', kind)).toEqual([]);
    expect(g.headlines.at(-1)!.text).toContain('raised again');
  });

  it('a corps that goes down fighting after months cut off is given a last honour; one that is starved out is not', () => {
    const g = asPlayer('GER');
    const [a, b] = field(g, 'GER');
    a.rec = { cutNow: 3 };
    storyFallen(g, a, 'surrendered', 'POL', a.reg);
    const last = honoursOf(g, g.stories.fallen.at(-1)!).find((x) => x.hon === 'laststand')!;
    expect(last).toBeDefined();
    expect(last.n).toBe(3);
    expect(citation(g, last)).not.toContain('{');
    b.rec = { cutNow: 3 };
    storyFallen(g, b, 'starved', 'POL', b.reg);
    expect(honoursOf(g, g.stories.fallen.at(-1)!).some((x) => x.hon === 'laststand')).toBe(false);
  });

  it('fortress troops are not written on the wall', () => {
    const g = createGame(1, null);
    const fort = g.corps.find((c) => c.kind === 'fort')!;
    storyFallen(g, fort, 'destroyed', 'GER', fort.reg);
    expect(g.stories.fallen).toEqual([]);
  });
});

describe('home ties', () => {
  it('the player\'s corps loses heart when its home falls and takes heart when it is freed', () => {
    const g = asPlayer('GER');
    const c = corps(g, 'XIX Corps');
    const h = g.regions.find((r) => r.owner === 'GER' && r.ctrl === 'GER' && r.id !== c.reg && !r.units.length)!;
    c.home = h.id;
    c.org = 0.6;
    h.ctrl = 'POL';
    storyMonth(g, 'GER');
    expect(c.rec!.exile).toBe(1);
    expect(c.org).toBeCloseTo(0.6 - STORIES.home.lostOrg, 6);
    expect(c.log!.some((l) => l[1] === 'homelost')).toBe(true);
    // Told once, not every month.
    storyMonth(g, 'GER');
    expect(c.org).toBeCloseTo(0.6 - STORIES.home.lostOrg, 6);
    expect(storyAttack(g, c, h, [])).toBeCloseTo(1 + STORIES.home.attack, 6);
    h.ctrl = 'GER';
    storyMonth(g, 'GER');
    expect(c.rec!.exile).toBeUndefined();
    expect(c.org).toBeCloseTo(0.6 - STORIES.home.lostOrg + STORIES.home.freedOrg, 6);
    expect(c.log!.some((l) => l[1] === 'homefree')).toBe(true);
  });

  it('a computer\'s corps has it in its diary and fights as before', () => {
    const g = asPlayer('GER');
    const p = field(g, 'POL')[0];
    const h = g.regions.find((r) => r.owner === 'POL' && r.id !== p.reg)!;
    p.home = h.id;
    const org = p.org;
    h.ctrl = 'GER';
    storyMonth(g, 'POL');
    expect(p.rec!.exile).toBe(1);
    expect(p.org).toBe(org);
    expect(storyAttack(g, p, h, [])).toBe(1);
  });
});

describe('rivalries', () => {
  it('two corps that keep meeting become old enemies, and settling the score pays', () => {
    const g = asPlayer('GER');
    const c = corps(g, 'XIX Corps');
    const foe = field(g, 'POL')[0];
    const t = g.regions[foe.reg];
    fight(g, [c], [foe], t, { win: false });
    fight(g, [c], [foe], t, { win: false });
    expect(g.stories.rivals).toEqual([]);
    expect(storyAttack(g, c, t, [foe])).toBe(1);
    fight(g, [c], [foe], t, { win: false });
    const r = g.stories.rivals[0];
    expect(r).toBeDefined();
    expect(r.mine).toBe(c.id);
    expect(r.theirs).toBe(foe.id);
    expect(r.met).toBe(STORIES.rivals.meets);
    expect(c.rival).toBe(r.id);
    expect(g.headlines.at(-1)!.text).toContain('have now met 3 times');
    expect(storyAttack(g, c, t, [foe])).toBeCloseTo(1 + STORIES.rivals.edge, 6);
    expect(storyDefence(g, c, g.regions[c.reg], [foe])).toBeCloseTo(1 + STORIES.rivals.edge + (c.home === c.reg ? STORIES.home.defence : 0), 6);
    expect(storyAttack(g, c, t, [field(g, 'POL')[1]])).toBe(1);

    const n = g.nations.GER;
    n.pc = 0;
    c.org = 0.5;
    const xp = c.xp;
    foe.dead = true;
    fight(g, [c], [foe], t, { level: 'Close victory' });
    expect(r.over).toBe('won');
    expect(c.rival).toBeUndefined();
    expect(c.org).toBeCloseTo(0.5 + STORIES.rivals.org, 6);
    expect(c.xp).toBeCloseTo(Math.min(1, xp + STORIES.rivals.xp), 6);
    expect(n.pc).toBe(STORIES.rivals.pc);
    expect(awardTitle(g, has(g, c, 'score')!)).toBe(`Settled with ${foe.name}`);
    expect(storyAttack(g, c, t, [foe])).toBeCloseTo(1 + (honourDef('score')!.edge!.attack ?? 0), 6);
  });

  it('a rivalry whose other half is lost somewhere else is simply over', () => {
    const g = asPlayer('GER');
    const c = corps(g, 'XIX Corps');
    const foe = field(g, 'POL')[0];
    for (let i = 0; i < 3; i++) fight(g, [c], [foe], g.regions[foe.reg], { win: false });
    removeCorps(g, foe);
    storyMonth(g, 'GER');
    expect(g.stories.rivals[0].over).toBe('gone');
    expect(c.rival).toBeUndefined();
    expect(has(g, c, 'score')).toBeUndefined();
  });

  it('the computer\'s armies keep no rivalries', () => {
    const g = createGame(1, null);
    const [a] = field(g, 'GER'), [b] = field(g, 'POL');
    for (let i = 0; i < 5; i++) fight(g, [a], [b], g.regions[b.reg], { win: false });
    expect(g.stories.rivals).toEqual([]);
    expect(g.stories.meets).toEqual({});
  });
});

describe('letters and dispatches', () => {
  it('the player gets at most two letters a month, from units of his own, with nothing left blank', () => {
    const g = createGame(2, 'GER');
    const perMonth = new Map<number, number>();
    while (g.turn < 12) runTurnAI(g);
    const letters = g.headlines.filter((h) => h.kind === 'letter');
    expect(letters.length).toBeGreaterThan(6);
    for (const h of letters) {
      perMonth.set(h.turn, (perMonth.get(h.turn) ?? 0) + 1);
      expect(h.text).not.toContain('{');
      expect(h.text).not.toContain('undefined');
      expect(h.by).toBeTruthy();
      expect(h.nat).toBe('GER');
      expect(h.unit).toBeDefined();
    }
    for (const n of perMonth.values()) expect(n).toBeLessThanOrEqual(STORIES.lettersPerMonth);
    // The same kind of letter is not written over and over.
    const texts = letters.map((h) => h.text);
    expect(new Set(texts).size).toBeGreaterThan(texts.length * 0.6);
    // Nothing is left waiting in the post.
    expect(g.stories.mail.every((m) => m.w > 0)).toBe(true);
  });

  it('every letter in the data has a voice or leaves it to chance, and names only what the rules can fill in', () => {
    const known = new Set(['place', 'enemy', 'general', 'home', 'rival', 'title', 'nick', 'first', 'n']);
    for (const [moment, bank] of Object.entries(STORIES.letters)) {
      expect(bank.length, moment).toBeGreaterThan(0);
      for (const [who, text] of bank) {
        expect(typeof who).toBe('string');
        expect(text.length).toBeGreaterThan(20);
        for (const m of text.matchAll(/\{(\w+)\}/g)) expect(known.has(m[1]), `${moment}: {${m[1]}}`).toBe(true);
      }
    }
    expect(STORIES.voices.length).toBeGreaterThan(4);
  });
});

describe('the air war and the war at sea', () => {
  it('twenty-five bombing missions are a tour of duty', () => {
    const g = createGame(1, 'GER');
    const f = g.airFleets.find((x) => x.nat === 'GER')!;
    const t = g.byName['Warsaw'].id;
    const none = { sky: false, sunk: 0, attLost: 0, defLost: 0 };
    const months = Math.ceil(25 / STORIES.tour.perMonth);
    for (let i = 0; i < months - 1; i++) storyAir(g, 'bomb', t, [f], [], none);
    expect(has(g, f, 'tour')).toBeUndefined();
    // Fighting for the sky is not a bombing mission.
    storyAir(g, 'superiority', t, [f], [], { ...none, sky: true });
    expect(has(g, f, 'tour')).toBeUndefined();
    expect(has(g, f, 'skies')).toBeDefined();
    storyAir(g, 'bomb', t, [f], [], none);
    const a = has(g, f, 'tour')!;
    expect(awardTitle(g, a)).toBe('Twenty-five missions');
    expect(f.rec!.missions).toBe(months * STORIES.tour.perMonth);
    expect(g.stories.mail.some((m) => m.m === 'tour')).toBe(true);
    expect(g.headlines.some((h) => h.kind === 'honour' && h.award === a.id)).toBe(true);
    expect(diaryText(g, f, f.log!.at(-2)!)).toBe('Bombed Warsaw.');
  });

  it('fighters that break up the raids are remembered for the sky they held', () => {
    const g = createGame(1, 'UK');
    const f = g.airFleets.find((x) => x.nat === 'UK' && x.ftr > 0)!;
    const raider = g.airFleets.find((x) => x.nat === 'GER')!;
    storyAir(g, 'bomb', f.base, [raider], [f], { sky: false, sunk: 0, attLost: 120, defLost: 30 });
    expect(f.rec!.saved).toBe(120);
    expect(has(g, f, 'few')).toBeUndefined();
    storyAir(g, 'bomb', f.base, [raider], [f], { sky: false, sunk: 0, attLost: 140, defLost: 30 });
    expect(awardTitle(g, has(g, f, 'few')!)).toBe(`Defence of ${g.regions[f.base].name}`);
    expect(storyEdge(g, f)).toBeCloseTo(1 + honourDef('few')!.edge!.all!, 6);
  });

  it('a great ship sunk goes on a fleet\'s colours, and convoy work is honoured by the month and the ton', () => {
    const g = createGame(1, 'UK');
    const f = g.fleets.find((x) => x.nat === 'UK')!;
    const sea = g.seas[f.zone];
    storySea(g, f.zone, [f], [], 0, 1, 0);
    expect(has(g, f, 'seavictory')).toBeUndefined();
    storySea(g, f.zone, [f], [], 1, 3, 0);
    expect(awardTitle(g, has(g, f, 'seavictory')!)).toBe(`${sea.name} 1939`);
    expect(diaryText(g, f, f.log!.at(-2)!)).toBe(`Won a fleet action in the ${sea.name}: 3 enemy ships sunk.`);
    for (let i = 0; i < 11; i++) storyConvoy(g, [], [f], 0, true);
    expect(has(g, f, 'escort')).toBeUndefined();
    storyConvoy(g, [], [f], 0, false);
    expect(has(g, f, 'escort')).toBeUndefined();
    storyConvoy(g, [], [f], 0, true);
    expect(has(g, f, 'escort')).toBeDefined();
    const raider = g.fleets.find((x) => x.nat === 'GER')!;
    storyConvoy(g, [raider], [], 1_900_000, true);
    expect(has(g, raider, 'tonnage')).toBeUndefined();
    storyConvoy(g, [raider], [], 150_000, true);
    expect(has(g, raider, 'tonnage')).toBeDefined();
    // The German raider is the computer's here: honoured, and no stronger for it.
    expect(storyEdge(g, raider)).toBe(1);
  });
});

describe('generals have ranks', () => {
  it('each army has its own, by level', () => {
    expect(rankOf({ nat: 'GER', level: 1 })).toBe('Generalleutnant');
    expect(rankOf({ nat: 'GER', level: 5 })).toBe('General');
    expect(rankOf({ nat: 'GER', level: 6 })).toBe('Generaloberst');
    expect(rankOf({ nat: 'GER', level: 9 })).toBe('Generalfeldmarschall');
    expect(rankOf({ nat: 'SOV', level: 9 })).toBe('Marshal of the Soviet Union');
    expect(rankOf({ nat: 'NOR', level: 4 })).toBe(COMMAND.ranks._default[1][1]);
    for (const rows of Object.values(COMMAND.ranks)) expect(rows.map((r) => r[0])).toEqual([1, 3, 6, 9]);
  });

  it('a general who grows into the next rank is promoted, and the newsreel says so', () => {
    const g = asPlayer('GER');
    const gen = generalOf(g, corps(g, 'XIX Corps'))!;
    const was = rankOf(gen);
    let guard = 0;
    while (rankOf(gen) === was && guard++ < 40) gainXp(g, gen, 400);
    expect(rankOf(gen)).not.toBe(was);
    expect(g.headlines.some((h) => h.text === `${gen.name} is promoted to ${rankOf(gen)}.`)).toBe(true);
  });
});

describe('saved with the game', () => {
  it('records, honours, the wall and the post all come back', () => {
    const g = createGame(2, 'GER');
    while (g.turn < 8) runTurnAI(g);
    const c = g.corps.find((x) => x.nat === 'GER' && x.hon?.length)!;
    expect(c).toBeDefined();
    const back = loadGame(saveGame(g))!;
    expect(back.stories).toEqual(g.stories);
    const same = back.corps.find((x) => x.id === c.id)!;
    expect(same.rec).toEqual(c.rec);
    expect(same.log).toEqual(c.log);
    expect(same.hon).toEqual(c.hon);
    expect(same.nick).toEqual(c.nick);
    expect(same.born).toBe(c.born);
    expect(same.home).toBe(c.home);
    expect(back.airFleets.map((f) => f.rec)).toEqual(g.airFleets.map((f) => f.rec));
    expect(back.fleets.map((f) => f.hon)).toEqual(g.fleets.map((f) => f.hon));
    expect(back.headlines.filter((h) => h.kind === 'letter')).toEqual(g.headlines.filter((h) => h.kind === 'letter'));
  });

  it('a campaign saved before there were unit stories starts keeping them from where it is', () => {
    const g = createGame(2, 'GER');
    runTurnAI(g);
    const d = JSON.parse(saveGame(g));
    delete d.stories;
    const back = loadGame(JSON.stringify(d))!;
    expect(back.stories).toEqual(newStories());
    runTurnAI(back);
    expect(back.turn).toBe(g.turn + 1);
  });
});
