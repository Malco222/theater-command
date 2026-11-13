// Morale and rest (the grand campaign). Every corps carries its morale beside its strength and its rest: up with
// victories, on home soil, with friends close by and with a quiet month out of the line; down with defeats, heavy
// losses, being cut off and standing alone. A corps in good heart hits harder and holds together longer. Each change
// is written down with its reason, so both interfaces can say why. The historical scenarios keep no morale at all.
import { beforeAll, describe, expect, it } from 'vitest';
import data from '../data/campaign/morale.json';
import { addCorps, assess, computeSupply, createGame, removeCorps, startPhase } from '../src/campaign/game';
import { MORALE, corpsMood, feel, forceMorale, heart, moraleBattle, moraleCapital, moraleMonth, moraleRules, moraleWord, nerve, restWord, restedDrafts, restedMonth, restedOrg } from '../src/campaign/morale';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { moodOf, runTurnAI } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';

const fresh = (): Game => startOpen(createGame(3, null));
const corpsOf = (g: Game, nat: string): Corps[] => g.corps.filter((c) => c.nat === nat && !c.dead);
const why = (c: Corps): Record<string, number> => Object.fromEntries((c.mw ?? []).filter((e) => e[2] === 0).map((e) => [e[0], e[1]]));
/** A corps standing by itself in a province, with nobody else of its side in it. */
const alone = (g: Game, nat: string, region: string): Corps => {
  const r = g.byName[region];
  for (const c of [...r.units]) removeCorps(g, c);
  return addCorps(g, nat, 'Test Corps', 'inf', 3, r.id, {});
};

// One computer-run grand campaign played for two years, saved every so often: the soak the last tests read.
let late = '';
beforeAll(() => {
  const g = startOpen(createGame(2, null));
  for (let k = 0; k < 24; k++) runTurnAI(g);
  late = saveGame(g);
}, 120000);

describe('the morale data', () => {
  it('has a start, a level to settle at, and words in rising order', () => {
    expect(data.start).toBeGreaterThan(0);
    expect(data.start).toBeLessThan(1);
    expect(data.level).toBeGreaterThan(0.2);
    expect(data.level).toBeLessThan(0.8);
    expect(data.drift).toBeGreaterThan(0);
    expect(data.drift).toBeLessThan(1);
    for (const table of [data.words, data.restWords]) {
      expect(table[0][0]).toBe(0);
      for (let i = 1; i < table.length; i++) expect(table[i][0]).toBeGreaterThan(table[i - 1][0] as number);
    }
    expect(moraleWord(0)).toBe('Broken');
    expect(moraleWord(0.65)).toBe('Confident');
    expect(moraleWord(1)).toBe('Eager');
    expect(restWord(0.1)).toBe('Exhausted');
    expect(restWord(1)).toBe('Fresh');
  });

  it('rewards and punishes in the direction the player was promised', () => {
    const B = data.battle, M = data.month;
    for (const k of ['won', 'decisive', 'held', 'capital'] as const) expect(B[k]).toBeGreaterThan(0);
    for (const k of ['repulsed', 'thrown', 'lost', 'routed', 'bloody', 'capitalLost'] as const) expect(B[k]).toBeLessThan(0);
    expect(B.decisive).toBeGreaterThan(B.won);
    expect(B.routed).toBeLessThan(B.lost);
    for (const k of ['home', 'beside', 'near', 'rested', 'general'] as const) expect(M[k]).toBeGreaterThan(0);
    for (const k of ['alone', 'cutOff', 'unpaid', 'winter'] as const) expect(M[k]).toBeLessThan(0);
    // What a corps can gain every month adds up to a level under the top of the scale, so that victories still show.
    const most = data.level + (M.home + M.beside + M.near + M.rested) / data.drift;
    expect(most).toBeLessThanOrEqual(0.95);
    expect(most).toBeGreaterThan(data.start);
    // Every reason has its words.
    for (const k of [...Object.keys(B).filter((x) => x !== 'bloodyAt'), 'home', 'friends', 'alone', 'rested', 'cutOff', 'unpaid', 'winter', 'general', 'time']) expect(data.why[k as keyof typeof data.why], k).toBeTruthy();
  });
});

describe('history keeps no morale', () => {
  it('leaves every corps of a historical scenario without any, through months of war', () => {
    const g = createGame(1, null);
    expect(moraleRules(g)).toBe(false);
    for (let k = 0; k < 4; k++) runTurnAI(g);
    expect(g.reports.length).toBeGreaterThan(5);
    for (const c of g.corps) {
      expect(c.mor).toBeUndefined();
      expect(c.mw).toBeUndefined();
      expect(heart(c)).toBe(1);
      expect(nerve(c)).toBe(1);
      expect(restedDrafts(g, c)).toBe(1);
      expect(restedOrg(g, c)).toBe(0);
    }
    expect(g.reports.some((r) => r.lines.some((l) => l.label === 'Morale'))).toBe(false);
    const mood = moodOf(g, g.corps[0]);
    expect(mood.morale).toBeNull();
    expect(mood.rest).toBe(g.corps[0].org);
    expect(mood.restWord).toBeTruthy();
    expect(mood.why).toEqual([]);
  });
});

describe('morale in the grand campaign', () => {
  it('gives every corps its morale from the first day, and every corps raised later', () => {
    const g = fresh();
    expect(moraleRules(g)).toBe(true);
    for (const c of g.corps) expect(c.mor).toBe(MORALE.start);
    const c = addCorps(g, 'GER', 'New Corps', 'inf', 3, g.byName['Berlin'].id, {});
    expect(c.mor).toBe(MORALE.start);
  });

  it('moves with a reason, stays on the scale, and adds one reason up', () => {
    const g = fresh();
    const c = corpsOf(g, 'GER')[0];
    feel(c, 'won', 0.08);
    feel(c, 'won', 0.08);
    feel(c, 'bloody', -0.04);
    expect(c.mor).toBeCloseTo(MORALE.start + 0.12, 6);
    expect(why(c)).toEqual({ won: 0.16, bloody: -0.04 });
    for (let k = 0; k < 20; k++) feel(c, 'decisive', 0.12);
    expect(c.mor).toBe(1);
    for (let k = 0; k < 20; k++) feel(c, 'routed', -0.14);
    expect(c.mor).toBe(0);
  });

  it('is worth up to 15% either way in a fight, and steadies or unsteadies the ranks', () => {
    const g = fresh();
    const c = corpsOf(g, 'GER')[0];
    c.mor = 0.5; expect(heart(c)).toBeCloseTo(1, 9); expect(nerve(c)).toBeCloseTo(1, 9);
    c.mor = 1; expect(heart(c)).toBeCloseTo(1 + MORALE.fight.span / 2, 9); expect(nerve(c)).toBeCloseTo(1 - MORALE.fight.steady / 2, 9);
    c.mor = 0; expect(heart(c)).toBeCloseTo(1 - MORALE.fight.span / 2, 9); expect(nerve(c)).toBeCloseTo(1 + MORALE.fight.steady / 2, 9);
  });

  it('shows in the odds and on the ticket of a battle', () => {
    const g = fresh();
    computeSupply(g);
    // A German corps on the Polish border, and the Poles in front of it.
    const target = g.regions.find((r) => r.ctrl === 'POL' && r.units.some((u) => u.nat === 'POL') && r.nb.some((j) => g.regions[j].units.some((u) => u.nat === 'GER' && u.kind !== 'fort')))!;
    const att = target.nb.flatMap((j) => g.regions[j].units).filter((u) => u.nat === 'GER' && u.kind !== 'fort').slice(0, 2);
    const set = (a: number, d: number): void => { for (const c of att) c.mor = a; for (const c of target.units) c.mor = d; };
    set(0.5, 0.5);
    const even = assess(g, att, target);
    expect(even.lines.find((l) => l.label === 'Morale')!.effect).toBeUndefined();
    set(1, 0.5);
    const keen = assess(g, att, target);
    expect(keen.A / even.A).toBeCloseTo(1 + MORALE.fight.span / 2, 6);
    expect(keen.D).toBeCloseTo(even.D, 9);
    set(0.5, 0.1);
    const shaky = assess(g, att, target);
    expect(shaky.D / even.D).toBeCloseTo(1 + (0.1 - 0.5) * MORALE.fight.span, 6);
    const line = shaky.lines.find((l) => l.label === 'Morale')!;
    expect(line.value).toBe('Attackers steady (50%), defenders broken (10%)');
    expect(line.effect).toBe('Defence down 12%');
  });

  it('rises with victory and falls with defeat, more for a decisive one, and heavy losses hurt either way', () => {
    const g = fresh();
    const [a1, a2] = corpsOf(g, 'GER'), [d1, d2] = corpsOf(g, 'POL');
    const B = MORALE.battle;
    moraleBattle(g, [a1], [d1], 1.4, true, new Map([[a1.id, 0.05], [d1.id, 0.3]]), false);
    expect(why(a1)).toEqual({ won: B.won });
    expect(why(d1)).toEqual({ lost: B.lost, bloody: B.bloody });
    moraleBattle(g, [a2], [d2], 2.6, true, new Map(), true);
    expect(why(a2)).toEqual({ decisive: B.decisive, capital: B.capital });
    expect(why(d2)).toEqual({ routed: B.routed });
    const [a3, a4] = corpsOf(g, 'GER').slice(2), [d3, d4] = corpsOf(g, 'POL').slice(2);
    moraleBattle(g, [a3], [d3], 1.0, false, new Map([[a3.id, 0.16]]), false);
    expect(why(a3)).toEqual({ repulsed: B.repulsed, bloody: B.bloody });
    expect(why(d3)).toEqual({ held: B.held });
    moraleBattle(g, [a4], [d4], 0.6, false, new Map(), false);
    expect(why(a4)).toEqual({ thrown: B.thrown });
    expect(a1.mor!).toBeGreaterThan(MORALE.start);
    expect(d1.mor!).toBeLessThan(MORALE.start);
    expect(d3.mor!).toBeGreaterThan(MORALE.start);
  });

  it('is felt in real battles: after the first month of the war the winners are in better heart than the losers', () => {
    const g = fresh();
    runTurnAI(g);
    const fought = (nat: string): Corps[] => corpsOf(g, nat).filter((c) => (c.rec?.battles ?? 0) > 0);
    const avg = (cs: Corps[]): number => cs.reduce((s, c) => s + c.mor!, 0) / cs.length;
    expect(fought('GER').length).toBeGreaterThan(3);
    expect(avg(fought('GER'))).toBeGreaterThan(MORALE.start + 0.03);
    if (fought('POL').length) expect(avg(fought('POL'))).toBeLessThan(avg(fought('GER')));
    expect(g.reports.every((r) => r.lines.some((l) => l.label === 'Morale'))).toBe(true);
    expect(g.corps.some((c) => (c.mw ?? []).some((e) => e[0] === 'won' || e[0] === 'decisive'))).toBe(true);
  });

  it('counts home soil, friends close by, and standing alone in front of the enemy', () => {
    const g = fresh();
    const M = MORALE.month;
    // At home with the army all round it.
    const home = g.byName['Berlin'].units.find((c) => c.nat === 'GER')!;
    addCorps(g, 'GER', 'Second Corps', 'inf', 3, g.byName['Berlin'].id, {});
    home.ap = 0;
    moraleMonth(g, home, { owed: 0, cold: false });
    const w = why(home);
    expect(w.home).toBe(M.home);
    expect(w.friends).toBeCloseTo(M.beside + M.near, 9);
    expect(w.alone).toBeUndefined();
    expect(w.rested).toBeUndefined();
    // Abroad, by itself, with the enemy next door and no friend within reach.
    const g2 = fresh();
    const warsaw = g2.byName['Warsaw'];
    for (const j of warsaw.nb) for (const c of [...g2.regions[j].units]) if (c.nat !== 'POL') removeCorps(g2, c);
    const lone = alone(g2, 'GER', 'Warsaw');
    lone.ap = 0;
    expect(warsaw.nb.some((j) => g2.regions[j].units.some((u) => u.nat === 'POL'))).toBe(true);
    moraleMonth(g2, lone, { owed: 0, cold: false });
    expect(why(lone).alone).toBe(M.alone);
    expect(why(lone).home).toBeUndefined();
    expect(why(lone).friends).toBeUndefined();
    expect(lone.mor!).toBeLessThan(MORALE.start);
  });

  it('counts being cut off, unpaid and frozen, and a general the men trust', () => {
    const g = fresh();
    const M = MORALE.month;
    const c = g.byName['Berlin'].units.find((u) => u.nat === 'GER')!;
    c.ap = 0;
    c.supplied = false;
    moraleMonth(g, c, { owed: 0.5, cold: true });
    const w = why(c);
    expect(w.cutOff).toBe(M.cutOff);
    expect(w.unpaid).toBeCloseTo(M.unpaid * 0.5, 9);
    expect(w.winter).toBe(M.winter);
    const led = g.corps.find((x) => x.general !== null && g.generals[x.general!].skills.leadership > 0);
    if (led) { moraleMonth(g, led, { owed: 0, cold: false }); expect(why(led).general).toBe(M.general); }
  });

  it('settles: a quiet garrison at home climbs to a level and stays there, short of the top', () => {
    const g = fresh();
    const c = g.byName['Berlin'].units.find((u) => u.nat === 'GER' && u.kind !== 'fort')!;
    addCorps(g, 'GER', 'Second Corps', 'inf', 3, g.byName['Berlin'].id, {});
    g.turn = 3;
    let last = c.mor!;
    for (let k = 0; k < 40; k++) { moraleMonth(g, c, { owed: 0, cold: false }); expect(c.mor!).toBeGreaterThanOrEqual(last - 1e-9); last = c.mor!; }
    const M = MORALE.month;
    const level = MORALE.level + (M.home + M.beside + M.near + M.rested + (why(c).general ?? 0)) / MORALE.drift;
    expect(c.mor!).toBeCloseTo(Math.min(1, level), 2);
    expect(c.mor!).toBeGreaterThan(0.8);
    // And a corps that has been beaten comes back towards it with time.
    c.mor = 0.1;
    moraleMonth(g, c, { owed: 0, cold: false });
    expect(c.mor!).toBeGreaterThan(0.1);
    expect(moodOf(g, c).trend).toBe('rising');
  });

  it('remembers a battle through one reckoning and no longer', () => {
    const g = fresh();
    const c = g.byName['Berlin'].units.find((u) => u.nat === 'GER')!;
    feel(c, 'won', MORALE.battle.won);
    expect(moodOf(g, c).why.map((x) => [x.key, x.earlier])).toEqual([['won', false]]);
    moraleMonth(g, c, { owed: 0, cold: false });
    const second = moodOf(g, c).why;
    expect(second.find((x) => x.key === 'won')!.earlier).toBe(true);
    expect(second.some((x) => x.key === 'home' && !x.earlier)).toBe(true);
    moraleMonth(g, c, { owed: 0, cold: false });
    expect(moodOf(g, c).why.some((x) => x.key === 'won')).toBe(false);
    // The month's own reasons are written afresh each time, never twice.
    expect(moodOf(g, c).why.filter((x) => x.key === 'home')).toHaveLength(1);
  });

  it('is felt by every corps of a nation the day its capital falls', () => {
    const g = fresh();
    const warsaw = g.byName['Warsaw'];
    warsaw.ctrl = 'GER';
    moraleCapital(g, warsaw, 'POL');
    for (const c of corpsOf(g, 'POL')) expect(why(c).capitalLost).toBe(MORALE.battle.capitalLost);
    for (const c of corpsOf(g, 'GER')) expect(why(c).capitalLost).toBeUndefined();
    // Any other town is not the capital.
    const g2 = fresh();
    const lodz = g2.byName['Lodz'];
    lodz.ctrl = 'GER';
    moraleCapital(g2, lodz, 'POL');
    expect(corpsOf(g2, 'POL').every((c) => !c.mw)).toBe(true);
  });
});

describe('a month of rest', () => {
  it('is a whole month in supply without marching or fighting', () => {
    const g = fresh();
    g.turn = 3;
    const c = g.byName['Berlin'].units.find((u) => u.nat === 'GER' && u.kind !== 'fort')!;
    expect(restedMonth(g, c)).toBe(true);
    expect(restedDrafts(g, c)).toBe(MORALE.rested.drafts);
    expect(restedOrg(g, c)).toBe(MORALE.rested.org);
    c.ap = c.apMax - 1;
    expect(restedMonth(g, c)).toBe(false);
    c.ap = c.apMax; c.fought = true;
    expect(restedMonth(g, c)).toBe(false);
    c.fought = false; c.supplied = false;
    expect(restedMonth(g, c)).toBe(false);
    c.supplied = true;
    // A corps raised this month has not rested: it has only just arrived.
    const raw = addCorps(g, 'GER', 'New Corps', 'inf', 3, g.byName['Berlin'].id, {});
    expect(restedMonth(g, raw)).toBe(false);
  });

  it('gives a battered corps more men, more rest and better spirits than one that marched', () => {
    const run = (marched: boolean): Corps => {
      const g = fresh();
      g.turn = 3;
      const c = g.byName['Berlin'].units.find((u) => u.nat === 'GER' && u.kind !== 'fort')!;
      c.str = 0.4; c.org = 0.2; c.mor = 0.4;
      if (marched) c.ap = c.apMax - 2;
      g.nations.GER.pp = 500;
      startPhase(g, 'GER');
      return c;
    };
    const rested = run(false), marched = run(true);
    expect(rested.str).toBeGreaterThan(marched.str + 0.05);
    expect(rested.str - 0.4).toBeCloseTo((marched.str - 0.4) * MORALE.rested.drafts, 2);
    expect(rested.org).toBeGreaterThan(marched.org + 0.1);
    expect(rested.mor!).toBeGreaterThan(marched.mor!);
    expect(why(rested).rested).toBe(MORALE.month.rested);
    expect(why(marched).rested).toBeUndefined();
  });
});

describe('what both interfaces read', () => {
  it('gives morale and rest on a scale of 0 to 1, each with a word and a sentence', () => {
    const g = fresh();
    const c = g.byName['Berlin'].units.find((u) => u.nat === 'GER' && u.kind !== 'fort')!;
    c.org = 0.6;
    feel(c, 'decisive', MORALE.battle.decisive);
    feel(c, 'capital', MORALE.battle.capital);
    const m = moodOf(g, c);
    expect(m.morale).toBeCloseTo(MORALE.start + MORALE.battle.decisive + MORALE.battle.capital, 6);
    expect(m.moraleWord).toBe('Eager');
    expect(m.trend).toBe('rising');
    expect(m.line).toBe('Eager and rising: a decisive victory and took an enemy capital.');
    expect(m.rest).toBe(0.6);
    expect(m.restWord).toBe('Tired');
    expect(m.restLine).toMatch(/^Tired: left where it stands for the month, it rests/);
    expect(m.resting).toBe(true);
    expect(m.why.map((w) => w.key)).toEqual(['decisive', 'capital']);
    expect(m.why[0].text).toBe(data.why.decisive);
    expect(corpsMood(g, c)).toEqual(m);
    // Beaten and bled.
    const d = corpsOf(g, 'POL')[0];
    d.fought = true;
    feel(d, 'routed', MORALE.battle.routed);
    feel(d, 'bloody', MORALE.battle.bloody);
    const low = moodOf(g, d);
    expect(low.trend).toBe('falling');
    expect(low.line).toBe(`${moraleWord(d.mor!)} and falling: routed and heavy losses.`);
    expect(low.restLine).toMatch(/it has fought this month/);
    expect(low.resting).toBe(false);
  });

  it('calls morale steady when it has nowhere left to go, or nothing has moved it', () => {
    const g = fresh();
    const c = corpsOf(g, 'GER')[0];
    expect(moodOf(g, c).trend).toBe('steady');
    expect(moodOf(g, c).line).toBe('Confident, and holding steady.');
    c.mor = 1;
    feel(c, 'won', MORALE.battle.won);
    expect(moodOf(g, c).trend).toBe('steady');
  });

  it('sums a force up as one figure, its corps weighed by their divisions', () => {
    const g = fresh();
    const [a, b] = corpsOf(g, 'GER');
    a.mor = 0.9; b.mor = 0.3;
    expect(forceMorale([a, b])).toBeCloseTo((0.9 * a.divs + 0.3 * b.divs) / (a.divs + b.divs), 9);
    expect(forceMorale(createGame(1, null).corps.slice(0, 3))).toBeNull();
  });

  it('is kept by a save, and an older save of a grand campaign picks it up', () => {
    const g = fresh();
    runTurnAI(g);
    const c = g.corps.find((x) => (x.mw ?? []).length > 0)!;
    const back = loadGame(saveGame(g))!;
    const same = back.corps.find((x) => x.id === c.id)!;
    expect(same.mor).toBe(c.mor);
    expect(same.mw).toEqual(c.mw);
    // A save from before morale was kept.
    const old = JSON.parse(saveGame(g));
    for (const x of old.corps) { delete x.mor; delete x.mw; }
    const lifted = loadGame(JSON.stringify(old))!;
    expect(lifted.corps.every((x) => x.mor === MORALE.start)).toBe(true);
    // A historical save gains none.
    const h = createGame(1, null);
    runTurnAI(h);
    expect(loadGame(saveGame(h))!.corps.every((x) => x.mor === undefined)).toBe(true);
  });
});

describe('two years of computer-run war', () => {
  it('keeps every corps on the scale, with armies in high spirits and armies in low', () => {
    const g = loadGame(late)!;
    const all = g.corps.filter((c) => !c.dead);
    expect(all.length).toBeGreaterThan(100);
    for (const c of all) {
      expect(c.mor).toBeGreaterThanOrEqual(0);
      expect(c.mor).toBeLessThanOrEqual(1);
      for (const e of c.mw ?? []) { expect(data.why[e[0] as keyof typeof data.why], e[0]).toBeTruthy(); expect(e[2] === 0 || e[2] === 1).toBe(true); expect((c.mw ?? []).length).toBeLessThanOrEqual(16); }
    }
    const ms = all.map((c) => c.mor!);
    expect(Math.max(...ms)).toBeGreaterThan(0.85);
    expect(Math.min(...ms)).toBeLessThan(0.6);
    // Not everybody ends up at the top of the scale.
    expect(ms.filter((m) => m > 0.97).length).toBeLessThan(all.length * 0.25);
    // Every mood reads as a sentence.
    for (const c of all.slice(0, 60)) { const m = moodOf(g, c); expect(m.line).toMatch(/^[A-Z][a-z]+(,| and) .*\.$/); expect(m.restLine).toMatch(/\.$/); }
  });
});
