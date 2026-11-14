// Version 65: rival staffs (the grand campaign; none of it in the historical scenarios).
//
// 1. Every computer staff has a character in each war: bold, cautious or opportunist, drawn from the war's seed
//    with a national lean. It moves the nerve the nation has, and it is shown to the player.
// 2. The main effort. A staff that stands on the defensive on a quiet front names one sector of it, masses there,
//    and attacks there and nowhere else for a season, then rests. An ally tells the player; an enemy's is learned
//    by an agent reading its books.
// 3. The will to finish. A general offensive with twice the enemy's strength and no ground gained for months takes
//    shorter odds and a third wave, and a computer nation whose capital has been lost for over a year wears out.
// 4. The staffs use the player's tools: a probe and the bombers on a prepared line, and worn corps folded together
//    with the cadre raised again.
import { describe, expect, it } from 'vitest';
import { foldWorn, softenLine } from '../src/campaign/ai';
import { airAI } from '../src/campaign/air';
import { aiProduction } from '../src/campaign/economy';
import { DEFENCE, addCorps, allied, assess, capture, collapseScore, createGame, endPhase, exhausted, groundWorks, limitAttackers, moveCorps, probeLine, probeWeight, removeCorps, startPhase, whyNoSwap, worksOf } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { RIVALS, characterOf, decide, effortOf, effortsOf, plansKnown, pressing, rivalRules, staffPhase, startOpen, temper, type Character } from '../src/campaign/staff';
import { beginHuman, humanUp, orderSwap, rivalStaff, rivalStaffs, runTurnAI, softenOptions, stepAI, swapChoices } from '../src/campaign/turn';
import { STACK, type Agent, type Corps, type Game, type Region } from '../src/campaign/types';

/** A grand campaign with the player's first month open, as the game's own screen would have it. */
function playing(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const M = DEFENCE.entrench.months;
const fresh = (c: Corps): Corps => { c.supplied = true; c.ap = c.apMax; c.fought = false; c.org = 1; return c; };
const news = (g: Game, from: number): string => g.headlines.slice(from).map((h) => h.text).join('\n');
function war(g: Game, a: string, b: string): void {
  if (!g.nations[a].atWar.includes(b)) g.nations[a].atWar.push(b);
  if (!g.nations[b].atWar.includes(a)) g.nations[b].atWar.push(a);
}
/** Puts an agent of the player's straight to work reading the books in a capital. */
function reader(g: Game, at: number, nat = 'GER'): Agent {
  const a: Agent = { id: g.intel.next++, nat, name: `Test ${g.intel.next}`, xp: 0, reg: at, ready: g.turn, mission: 'books', born: g.turn };
  g.intel.agents.push(a);
  return a;
}
/** A province a computer nation holds beside one the player holds with field troops in it: the staff's side of the line, and the player's. */
function line(g: Game, nat: string, me = 'GER'): { ours: Region; theirs: Region } {
  for (const r of g.regions) {
    if (r.ctrl !== nat || r.id === g.nations[nat].capital) continue;
    const j = r.nb.find((k) => g.regions[k].ctrl === me && g.regions[k].units.some((u) => u.nat === me && !u.dead && u.kind !== 'fort'));
    if (j !== undefined) return { ours: r, theirs: g.regions[j] };
  }
  throw new Error(`no line between ${nat} and ${me}`);
}
/** Runs one thing with a staff's patience and standards set aside, so that the test does not wait months for a quiet front. */
function eager<T>(who: Character, run: () => T): T {
  const C = RIVALS.characters[who];
  const old = { ...C };
  Object.assign(C, { quiet: 0, strength: 0, local: 0.05 });
  try { return run(); } finally { Object.assign(C, old); }
}

describe('the character of a staff', () => {
  it('is one of three, the same all through a war, and leans the way the nation does', () => {
    const g = playing('GER');
    expect(rivalRules(g)).toBe(true);
    for (const nat of ['SOV', 'UK', 'FRA', 'ITA', 'POL']) {
      const c = characterOf(g, nat);
      expect(['bold', 'cautious', 'opportunist']).toContain(c);
      expect(characterOf(g, nat)).toBe(c);
      expect(characterOf(loadGame(saveGame(g))!, nat)).toBe(c);
    }
    // Over many wars Germany's staff is bold more often than not and France's cautious; no nation has one character only.
    const count = (nat: string): Record<Character, number> => {
      const n: Record<Character, number> = { bold: 0, cautious: 0, opportunist: 0 };
      for (let seed = 1; seed <= 120; seed++) n[characterOf({ seed, flags: { open: 1 } } as unknown as Game, nat)]++;
      return n;
    };
    const ger = count('GER'), fra = count('FRA'), ita = count('ITA');
    expect(ger.bold).toBeGreaterThan(ger.cautious + ger.opportunist);
    expect(fra.cautious).toBeGreaterThan(fra.bold + fra.opportunist);
    expect(ita.opportunist).toBeGreaterThan(Math.max(ita.bold, ita.cautious));
    for (const n of [ger, fra, ita]) expect(Object.values(n).filter((x) => x > 0)).toHaveLength(3);
  });

  it('moves the nerve the nation has: a bold staff attacks on less, a cautious one waits for more', () => {
    const g = playing('GER');
    const who = characterOf(g, 'SOV');
    const C = RIVALS.characters[who];
    const old = { ...C };
    const with_ = temper(g, 'SOV');
    Object.assign(C, { attackAt: 1, minOdds: 0 });
    let plain;
    try { plain = temper(g, 'SOV'); } finally { Object.assign(C, old); }
    expect(with_.attackAt).toBeCloseTo(plain.attackAt * C.attackAt, 6);
    expect(with_.persist).toBeCloseTo(plain.persist, 6);
    expect(RIVALS.characters.bold.attackAt).toBeLessThan(RIVALS.characters.opportunist.attackAt);
    expect(RIVALS.characters.opportunist.attackAt).toBeLessThan(RIVALS.characters.cautious.attackAt);
    expect(RIVALS.characters.bold.minOdds).toBeLessThan(RIVALS.characters.cautious.minOdds);
    expect(RIVALS.characters.bold.quiet).toBeLessThan(RIVALS.characters.cautious.quiet);
    // The historical scenarios know no characters: the staffs there follow history's plan.
    expect(rivalRules(createGame(3, 'GER'))).toBe(false);
  });

  it('is shown to the player for every country the computer leads, and never for his own', () => {
    const g = playing('GER');
    expect(rivalStaff(g, 'GER')).toBeNull();
    const all = rivalStaffs(g);
    expect(all.length).toBeGreaterThan(5);
    expect(all.some((v) => v.nat === 'GER')).toBe(false);
    for (const v of all) {
      expect(v.character).toBe(characterOf(g, v.nat));
      expect(v.label).toBe(RIVALS.characters[v.character].label);
      expect(v.says.length).toBeGreaterThan(20);
      expect(v.line.length).toBeGreaterThan(10);
    }
    // The nations at war come first.
    const firstPeace = all.findIndex((v) => !g.nations[v.nat].atWar.length);
    if (firstPeace >= 0) expect(all.slice(firstPeace).every((v) => !g.nations[v.nat].atWar.length)).toBe(true);
    expect(rivalStaff(createGame(3, 'GER'), 'SOV')).toBeNull();
  });
});

describe('the main effort', () => {
  /** Poland at war with the player, with two fresh corps beside a province of his that is held. */
  function poland(): { g: Game; ours: Region; theirs: Region } {
    const g = playing('GER');
    war(g, 'GER', 'POL');
    const { ours, theirs } = line(g, 'POL');
    for (const u of [...ours.units]) if (u.kind !== 'fort') removeCorps(g, u);
    fresh(addCorps(g, 'POL', 'Test Army A', 'inf', 3, ours.id, {}));
    fresh(addCorps(g, 'POL', 'Test Army B', 'inf', 3, ours.id, {}));
    return { g, ours, theirs };
  }
  const name = (g: Game): void => eager(characterOf(g, 'POL'), () => decide(g, 'POL'));

  it('is named on a quiet front: one enemy province, for a season, with the army massed opposite it', () => {
    const { g } = poland();
    expect(effortOf(g, 'POL')).toBeNull();
    name(g);
    const e = effortOf(g, 'POL')!;
    expect(e).not.toBeNull();
    const t = g.regions[e.target];
    // (It may fall on Germany or on an ally of hers who stands in the line.)
    expect(g.nations.POL.atWar).toContain(t.ctrl);
    expect(t.units.some((u) => !u.dead && g.nations.POL.atWar.includes(u.nat))).toBe(true);
    expect(t.nb.some((j) => g.regions[j].ctrl === 'POL')).toBe(true);
    expect(e.first).toBe(e.target);
    expect(e.enemy).toBe(t.ctrl);
    expect(e.from).toBe(g.turn);
    expect(e.to - e.from).toBe(RIVALS.effort.months); // a month of massing first (Version 67), then the months of the offensive
    expect(e.name).toMatch(/^the (spring|summer|autumn|winter) offensive towards /);
    expect(e.name.endsWith(t.name)).toBe(true);
    expect(effortsOf(g, 'POL')).toHaveLength(1);
    // The orders: attack there and beside it, nowhere else, and mass in the provinces next to the target.
    const ph = staffPhase(g, 'POL')!;
    expect(ph.posture).toBe('attack');
    expect(ph.effort).toBe(t.id);
    expect(ph.targets![0]).toBe(t.name);
    expect(ph.targets!.length).toBeLessThanOrEqual(RIVALS.effort.sector);
    for (const nm of ph.targets!) expect(g.nations.POL.atWar).toContain(g.byName[nm].ctrl);
    const doors = t.nb.filter((j) => g.regions[j].ctrl === 'POL' && j !== g.nations.POL.capital);
    expect(doors.length).toBeGreaterThan(0);
    for (const j of doors) {
      expect(ph.staging![g.regions[j].name]).toBe(Math.min(STACK, RIVALS.effort.mass));
      expect(ph.release).toContain(g.regions[j].name);
    }
    // It is kept with the campaign.
    expect(effortOf(loadGame(saveGame(g))!, 'POL')).toEqual(e);
    // A second month: the same effort, not a new one.
    decide(g, 'POL');
    expect(effortOf(g, 'POL')).toEqual(e);
  });

  it('is not named where the staff is too weak, or has waited too short a time', () => {
    const { g } = poland();
    const C = RIVALS.characters[characterOf(g, 'POL')];
    const old = { ...C };
    try {
      Object.assign(C, { quiet: 0, strength: 50, local: 0.05 });
      decide(g, 'POL');
      expect(effortOf(g, 'POL')).toBeNull();
      Object.assign(C, { quiet: 500, strength: 0, local: 0.05 });
      // (An opportunist does not wait while the enemy is busy with someone else: leave Germany with nobody else to fight.)
      for (const n of Object.values(g.nations)) { n.lastFight = {}; }
      decide(g, 'POL');
      expect(effortOf(g, 'POL')).toBeNull();
      Object.assign(C, { quiet: 0, strength: 0, local: 500 });
      decide(g, 'POL');
      expect(effortOf(g, 'POL')).toBeNull();
    } finally { Object.assign(C, old); }
  });

  it('ends when its months are up, the player is told, and the staff rests before the next', () => {
    const { g } = poland();
    name(g);
    const e = effortOf(g, 'POL')!;
    g.flags['st:effTo:POL'] = g.turn - 1;
    const h = g.headlines.length;
    name(g);
    expect(effortOf(g, 'POL')).toBeNull();
    expect(g.flags['st:effRest:POL']).toBe(g.turn + RIVALS.effort.rest);
    expect(news(g, h)).toContain(`${e.name.replace(/^the /, '')} is over`);
    expect(news(g, h)).toContain('The line held');
    // Resting: nothing new is named, however eager.
    name(g);
    expect(effortOf(g, 'POL')).toBeNull();
    expect(staffPhase(g, 'POL')?.effort).toBeUndefined();
  });

  it('is carried on to the next province when its target falls', () => {
    const { g, theirs: t } = poland();
    // The staff's main effort is aimed at a German province on the line.
    g.flags['st:eff:POL'] = g.flags['st:eff0:POL'] = t.id + 1;
    g.flags['st:effAt:POL'] = g.turn;
    g.flags['st:effTo:POL'] = g.turn + RIVALS.effort.months - 1;
    g.flags['st:effE:POL'] = Object.keys(g.nations).indexOf('GER') + 1;
    const e = effortOf(g, 'POL')!;
    expect(e.target).toBe(t.id);
    // The target falls to a strong force, and one weak German corps stands in a province beyond it.
    for (const u of [...t.units]) removeCorps(g, u);
    capture(g, t, 'POL');
    for (let k = 0; k < 4; k++) fresh(addCorps(g, 'POL', `Spearhead ${k}`, 'inf', 4, t.id, {}));
    const beyond = g.regions[t.nb.find((j) => g.regions[j].ctrl === 'GER' && g.regions[j].id !== g.nations.GER.capital)!];
    for (const u of [...beyond.units]) removeCorps(g, u);
    addCorps(g, 'GER', 'Rearguard', 'inf', 1, beyond.id, {});
    decide(g, 'POL');
    const e2 = effortOf(g, 'POL')!;
    expect(e2).not.toBeNull();
    expect(e2.won).toBe(1);
    expect(e2.first).toBe(e.first);
    expect(e2.target).not.toBe(e.target);
    expect(t.nb).toContain(e2.target);
    expect(e2.to).toBe(e.to);
    expect(e2.name).toBe(e.name);
  });

  it('is secret from an enemy, told by an ally, and learned by an agent reading the books', () => {
    const { g } = poland();
    const cap = g.nations.POL.capital;
    name(g);
    // No agent: the character is known, the plan is not.
    expect(plansKnown(g, 'POL')).toBeNull();
    let v = rivalStaff(g, 'POL')!;
    expect(v.source).toBeNull();
    expect(v.efforts).toEqual([]);
    expect(v.line).toContain('not known');
    expect(v.line).toContain(g.regions[cap].name);
    // An agent reading the books in the capital: the place and the months.
    const a = reader(g, cap);
    expect(plansKnown(g, 'POL')).toBe('agent');
    v = rivalStaff(g, 'POL')!;
    const e = effortOf(g, 'POL')!;
    expect(v.source).toBe('agent');
    expect(v.efforts).toHaveLength(1);
    expect(v.efforts[0]).toMatchObject({ name: e.name, target: e.target, targetName: g.regions[e.target].name, enemy: e.enemy, from: e.from, to: e.to, atYou: true });
    expect(v.line).toContain('Main effort');
    expect(v.line).toContain(g.regions[e.first].name);
    // With the agent in place the player is told the moment one is named.
    for (const k of ['eff', 'eff0', 'effAt', 'effTo', 'effE', 'effWon', 'effRest']) delete g.flags[`st:${k}:POL`];
    const h = g.headlines.length;
    name(g);
    expect(news(g, h)).toContain(`Agent ${a.name} reports from ${g.regions[cap].name}`);
    expect(news(g, h)).toContain('has named its main effort');
    // An ally's plans are always open, and the player's own allies' efforts are not aimed at him.
    const friend = Object.values(g.nations).find((n) => n.id !== 'GER' && n.alive && !n.dormant && allied(g, n.id, 'GER'));
    if (friend) expect(plansKnown(g, friend.id)).toBe('ally');
    // With nobody at the table everything is known.
    expect(plansKnown(startOpen(createGame(3, null)), 'POL')).toBe('ally');
  });

  it('is never named by the player\'s own staff', () => {
    const g = playing('POL');
    war(g, 'GER', 'POL');
    eager(characterOf(g, 'POL'), () => decide(g, 'POL'));
    expect(effortOf(g, 'POL')).toBeNull();
  });
});

describe('the will to finish', () => {
  it('takes shorter odds and a third wave once an offensive with the numbers has gained nothing for months', () => {
    const g = playing('GER');
    const P = RIVALS.press;
    g.turn = 30;
    g.flags['st:gain:SOV'] = g.turn - (P.after - 1);
    expect(pressing(g, 'SOV', { ratio: P.ratio + 1 }, 1.3)).toBeNull();
    g.flags['st:gain:SOV'] = g.turn - P.after;
    expect(pressing(g, 'SOV', { ratio: P.ratio - 0.1 }, 1.3)).toBeNull();
    const first = pressing(g, 'SOV', { ratio: P.ratio }, 1.3)!;
    expect(first.minOdds).toBeCloseTo(1.3 - P.step, 6);
    expect(first.waves).toBe(P.waves);
    g.flags['st:gain:SOV'] = g.turn - (P.after + 2);
    expect(pressing(g, 'SOV', { ratio: P.ratio }, 1.3)!.minOdds).toBeCloseTo(1.3 - 3 * P.step, 6);
    // Never below the floor, and a staff that already asks less than the floor is not made to ask more.
    g.flags['st:gain:SOV'] = g.turn - 25;
    expect(pressing(g, 'SOV', { ratio: P.ratio }, 1.3)!.minOdds).toBe(P.floor);
    expect(pressing(g, 'SOV', { ratio: P.ratio }, P.floor - 0.1)!.minOdds).toBeCloseTo(P.floor - 0.1, 6);
    // Not in the historical scenarios.
    const h = createGame(3, 'GER');
    h.turn = 30;
    h.flags['st:gain:SOV'] = 1;
    expect(pressing(h, 'SOV', { ratio: 9 }, 1.3)).toBeNull();
  });

  it('wears out a computer nation whose capital has been in enemy hands for more than a year', () => {
    const g = playing('GER');
    const X = RIVALS.exhaust;
    war(g, 'GER', 'SOV');
    const moscow = g.regions[g.nations.SOV.capital];
    expect(exhausted(g, 'SOV')).toBe(0);
    // The month the capital is found in enemy hands is written down.
    moscow.ctrl = 'GER';
    startPhase(g, 'SOV');
    expect(g.flags['capOut:SOV']).toBe(g.turn);
    expect(exhausted(g, 'SOV')).toBe(0);
    const before = collapseScore(g, 'SOV');
    // A year and five months on: five steps nearer to giving in.
    g.flags['capOut:SOV'] = g.turn - X.after - 5;
    expect(exhausted(g, 'SOV')).toBeCloseTo(5 * X.per, 6);
    expect(collapseScore(g, 'SOV')).toBeCloseTo(before + 5 * X.per, 6);
    // Retaken: the count starts again from nothing.
    moscow.ctrl = 'SOV';
    startPhase(g, 'SOV');
    expect(g.flags['capOut:SOV']).toBeUndefined();
    expect(exhausted(g, 'SOV')).toBe(0);
    // The player's own country is never worn out this way.
    g.flags['capOut:GER'] = 1;
    g.turn += 40;
    expect(exhausted(g, 'GER')).toBe(0);
    g.turn -= 40;
  });
});

describe('the staffs use the player\'s tools', () => {
  it('probes a prepared line before an attack that falls a little short', () => {
    const g = playing('GER');
    war(g, 'GER', 'POL');
    const { ours, theirs } = line(g, 'POL');
    for (const u of [...ours.units]) if (u.kind !== 'fort') removeCorps(g, u);
    const a = fresh(addCorps(g, 'POL', 'Test Army A', 'inf', 3, ours.id, {}));
    const b = fresh(addCorps(g, 'POL', 'Test Army B', 'inf', 3, ours.id, {}));
    const dug = theirs.units.filter((u) => u.nat === 'GER' && !u.dead && u.kind !== 'fort');
    for (const c of dug) { c.dug = true; c.dugFor = M; }
    const pool = limitAttackers(g, g.corps.filter((c) => c.nat === 'POL' && !c.dead && !c.hold && c.kind !== 'fort' && c.ap > 0 && c.org >= 0.35 && g.regions[c.reg].nb.includes(theirs.id)), theirs);
    const odds = assess(g, pool, theirs).odds;
    // The attack would be good enough as it is: nobody probes.
    let h = g.headlines.length;
    softenLine(g, 'POL', new Set(['GER']), new Set(), new Set([theirs.name]), odds * 0.9);
    expect(g.flags[`probed:${theirs.id}`]).toBeUndefined();
    // It is hopeless: nobody probes either.
    softenLine(g, 'POL', new Set(['GER']), new Set(), new Set([theirs.name]), odds / (RIVALS.soften.from * 0.9));
    expect(g.flags[`probed:${theirs.id}`]).toBeUndefined();
    // It falls a little short: one corps feels out the line, the works are the worse for it, and the player is told.
    softenLine(g, 'POL', new Set(['GER']), new Set(), new Set([theirs.name]), odds * 1.15);
    expect(g.flags[`probed:${theirs.id}`]).toBe(g.turn);
    expect(Math.max(...dug.map(worksOf))).toBeCloseTo(M - DEFENCE.probe!.wear, 6);
    expect([a, b].filter((c) => c.fought).length + g.corps.filter((c) => c.nat === 'POL' && c !== a && c !== b && c.fought && g.regions[c.reg].nb.includes(theirs.id)).length).toBe(1);
    expect(news(g, h)).toContain(`probe your line at ${theirs.name}`);
    // Once a month at one place.
    h = g.headlines.length;
    softenLine(g, 'POL', new Set(['GER']), new Set(), new Set([theirs.name]), odds * 1.15);
    expect(g.headlines.length).toBe(h);
    // A line that is not dug in is not probed, and the player's own staff never does it for him.
    const g2 = playing('GER');
    war(g2, 'GER', 'POL');
    const l2 = line(g2, 'POL');
    for (const c of l2.theirs.units) { delete c.dug; delete c.dugFor; }
    fresh(addCorps(g2, 'POL', 'Test Army A', 'inf', 3, l2.ours.id, {}));
    softenLine(g2, 'POL', new Set(['GER']), new Set(), new Set([l2.theirs.name]), 50);
    expect(g2.flags[`probed:${l2.theirs.id}`]).toBeUndefined();
  });

  it('sends the bombers at the fieldworks in front of its main effort', () => {
    const g = playing('GER');
    expect(g.nations.FRA.atWar).toContain('GER');
    // A German province on the French border, its troops dug in, named as the French staff's main effort.
    const { theirs } = line(g, 'FRA');
    const dug = theirs.units.filter((u) => u.nat === 'GER' && !u.dead);
    for (const c of dug) { c.dug = true; c.dugFor = M; }
    const bombers = g.airFleets.filter((f) => f.nat === 'FRA' && f.bmr > 0);
    expect(bombers.length).toBeGreaterThan(0);
    // The German fighters are elsewhere this month, and the French bombers are fit and many.
    for (const f of g.airFleets) if (f.nat === 'GER') { f.ftr = 0; f.bmr = 0; }
    for (const f of bombers) { f.bmr = Math.max(f.bmr, 250); f.ready = 1; f.mission = 'support'; }
    g.flags['st:eff:FRA'] = g.flags['st:eff0:FRA'] = theirs.id + 1;
    g.flags['st:effAt:FRA'] = g.turn;
    g.flags['st:effTo:FRA'] = g.turn + RIVALS.effort.months - 1;
    g.flags['st:effE:FRA'] = Object.keys(g.nations).indexOf('GER') + 1;
    const n = g.airReports.length;
    airAI(g, 'FRA');
    const raid = g.airReports.slice(n).find((r) => r.mission === 'line' && r.region === theirs.id);
    expect(raid).toBeDefined();
    expect(raid!.attNat).toBe('FRA');
    expect(Math.max(...dug.map(worksOf))).toBeLessThan(M);
    // No main effort, no offensive: the works are left alone.
    const g2 = playing('GER');
    const l2 = line(g2, 'FRA');
    for (const c of l2.theirs.units) { c.dug = true; c.dugFor = M; }
    for (const f of g2.airFleets) if (f.nat === 'GER') { f.ftr = 0; f.bmr = 0; }
    for (const f of g2.airFleets) if (f.nat === 'FRA' && f.bmr > 0) { f.bmr = Math.max(f.bmr, 250); f.ready = 1; f.mission = 'support'; }
    const n2 = g2.airReports.length;
    airAI(g2, 'FRA');
    expect(g2.airReports.slice(n2).some((r) => r.mission === 'line')).toBe(false);
  });

  it('folds two worn corps into one and raises the cadre again when the treasury allows', () => {
    const g = playing('GER');
    const paris = g.byName['Paris'];
    const a = fresh(addCorps(g, 'FRA', 'Worn Corps A', 'inf', 3, paris.id, {}));
    const b = fresh(addCorps(g, 'FRA', 'Worn Corps B', 'inf', 3, paris.id, {}));
    a.str = 0.3; b.str = 0.35; a.xp = 0.4; b.xp = 0.1;
    foldWorn(g, 'FRA');
    // The more experienced corps is kept with the men of both; the other has gone home as a cadre.
    expect(a.dead).toBeFalsy();
    expect(a.str).toBeCloseTo(0.65, 2);
    expect(g.corps.includes(b) && !b.dead).toBe(false);
    expect((g.nations.FRA.cadres ?? []).map((c) => c.name)).toContain('Worn Corps B');
    // Corps that are not worn are left alone, and so are two whose men would not fit in one.
    const c = fresh(addCorps(g, 'FRA', 'Sound Corps C', 'inf', 3, paris.id, {}));
    const d = fresh(addCorps(g, 'FRA', 'Sound Corps D', 'inf', 3, paris.id, {}));
    c.str = 0.3; d.str = RIVALS.fold.worn + 0.1;
    foldWorn(g, 'FRA');
    expect(c.dead).toBeFalsy();
    expect(d.dead).toBeFalsy();
    expect(c.str).toBeCloseTo(0.3, 6);
    // The cadre is raised again from the programme when the money is there.
    // (France's programme opens once history's own reinforcements have all arrived: move the calendar on.)
    g.nations.FRA.pp = 5000;
    g.nations.FRA.queue = [];
    g.turn = 40;
    aiProduction(g, 'FRA');
    expect(g.nations.FRA.queue.some((q) => q.cadre !== undefined)).toBe(true);
    g.turn = 1;
    // The player's own worn corps are his to fold.
    const berlin = g.byName['Berlin'];
    const x = fresh(addCorps(g, 'GER', 'Worn X', 'inf', 3, berlin.id, {}));
    const y = fresh(addCorps(g, 'GER', 'Worn Y', 'inf', 3, berlin.id, {}));
    x.str = 0.3; y.str = 0.3;
    foldWorn(g, 'GER');
    expect(x.str).toBeCloseTo(0.3, 6);
    expect(y.dead).toBeFalsy();
  });
});

describe('small fixes that came with the staffs', () => {
  it('a probe wears the works by the weight of the corps that makes it', () => {
    const g = playing('GER');
    war(g, 'GER', 'POL');
    const target = g.byName['Lodz'];
    const from = g.regions[target.nb.find((j) => g.regions[j].ctrl === 'GER')!];
    for (const u of [...target.units]) if (u.kind !== 'fort') removeCorps(g, u);
    for (const u of [...from.units]) if (u.kind !== 'fort') removeCorps(g, u);
    const def = fresh(addCorps(g, 'POL', 'Line 1', 'inf', 3, target.id, {}));
    def.dug = true; def.dugFor = 2;
    const husk = fresh(addCorps(g, 'GER', 'Husk', 'inf', 3, from.id, {}));
    husk.str = 0.2;
    const whole = fresh(addCorps(g, 'GER', 'Whole', 'inf', 3, from.id, {}));
    // 9,000 men are three tenths of the weight a probe in full takes; 45,000 are more than enough.
    expect(DEFENCE.probe!.weight).toBe(30000);
    expect(probeWeight(husk)).toBeCloseTo(0.3, 6);
    expect(probeWeight(whole)).toBe(1);
    // The card sends the corps with the weight, though the husk is the one the attack would miss least.
    const s = softenOptions(g, [husk, whole], target)!;
    expect(s.probe.corps).toBe(whole.id);
    expect(s.probe.wear).toBe(1);
    expect(s.probe.weight).toBe(1);
    // Alone, the husk is offered for what it can do, and the card says how much that is.
    const thin = softenOptions(g, [husk], target)!;
    if (thin.probe.corps === husk.id) { expect(thin.probe.wear).toBeCloseTo(0.3, 6); expect(thin.probe.weight).toBeCloseTo(0.3, 6); expect(thin.probe.full).toBe(30000); }
    // The husk scratches the line: three tenths of a month, and everyone there is named all the same.
    const rep = probeLine(g, husk, target)!;
    expect(rep.wear).toBeCloseTo(0.3, 6);
    expect(worksOf(def)).toBeCloseTo(1.7, 6);
    expect(rep.found).toEqual(['Line 1']);
  });

  it('a corps that stands the month where fieldworks are empty takes them as its own', () => {
    const g = playing('GER');
    const r = g.byName['Berlin'];
    for (const u of [...r.units]) if (u.kind !== 'fort') removeCorps(g, u);
    const c = fresh(addCorps(g, 'GER', 'New Corps', 'inf', 3, r.id, {}));
    delete c.dug; delete c.dugFor;
    g.flags[`fw:${r.id}`] = 2; g.flags[`fwk:${r.id}`] = 1; g.flags[`fwn:${r.id}`] = Object.keys(g.nations).indexOf('GER');
    expect(groundWorks(g, r.id)).toEqual({ months: 2, corps: 1, nat: 'GER' });
    endPhase(g, 'GER');
    expect(c.dug).toBe(true);
    expect(worksOf(c)).toBeCloseTo(2, 6);
    expect(groundWorks(g, r.id)).toBeNull();
    // With nothing on the ground it digs its first month, as it always did.
    const g2 = playing('GER');
    const r2 = g2.byName['Berlin'];
    const c2 = fresh(addCorps(g2, 'GER', 'New Corps', 'inf', 3, r2.id, {}));
    delete c2.dug; delete c2.dugFor;
    endPhase(g2, 'GER');
    expect(c2.dug).toBe(true);
    expect(worksOf(c2)).toBeCloseTo(1, 6);
    // And a corps that marched or fought that month takes nothing: the works wait for whoever comes to stay.
    const g3 = playing('GER');
    const r3 = g3.byName['Berlin'];
    for (const u of [...r3.units]) if (u.kind !== 'fort') removeCorps(g3, u);
    const c3 = fresh(addCorps(g3, 'GER', 'New Corps', 'inf', 3, r3.id, {}));
    delete c3.dug; delete c3.dugFor;
    c3.fought = true;
    g3.flags[`fw:${r3.id}`] = 2; g3.flags[`fwk:${r3.id}`] = 1; g3.flags[`fwn:${r3.id}`] = Object.keys(g3.nations).indexOf('GER');
    endPhase(g3, 'GER');
    expect(c3.dug).toBeFalsy();
    expect(groundWorks(g3, r3.id)).not.toBeNull();
  });
});

describe('two corps change places in one order', () => {
  /** Two German provinces side by side, far from any enemy, each holding every corps its roads will carry. */
  function full(g: Game): { a: Region; b: Region; as: Corps[]; bs: Corps[] } {
    const a = g.byName['Berlin'];
    const b = g.regions[a.nb.find((j) => g.regions[j].ctrl === 'GER' && g.regions[j].owner === 'GER' && !g.regions[j].nb.some((k) => g.regions[k].ctrl !== 'GER'))!];
    for (const r of [a, b]) for (const u of [...r.units]) if (u.kind !== 'fort') removeCorps(g, u);
    const as = Array.from({ length: STACK }, (_, i) => fresh(addCorps(g, 'GER', `Line ${i + 1}`, 'inf', 3, a.id, {})));
    const bs = Array.from({ length: STACK }, (_, i) => fresh(addCorps(g, 'GER', `Reserve ${i + 1}`, 'inf', 3, b.id, {})));
    for (const c of bs) { delete c.dug; delete c.dugFor; }
    return { a, b, as, bs };
  }

  it('on a full front, where neither could march in first: each takes over what the other leaves', () => {
    const g = playing('GER');
    const { a, b, as, bs } = full(g);
    // One worn corps in finished works, with others beside it that have none: the corps that comes in has first claim.
    for (const c of as) { delete c.dug; delete c.dugFor; }
    as[0].dug = true; as[0].dugFor = M;
    as[0].str = 0.4;
    // A march cannot do it: the province next door is full.
    expect(moveCorps(g, as[0], b)).toBe(false);
    expect(whyNoSwap(g, as[0], bs[0])).toBeNull();
    expect(swapChoices(g, as[0], b.id).map((c) => c.id).sort()).toEqual(bs.map((c) => c.id).sort());
    const ap = { a: as[0].ap, b: bs[0].ap };
    expect(orderSwap(g, as[0], bs[0])).toBe(true);
    expect(as[0].reg).toBe(b.id);
    expect(bs[0].reg).toBe(a.id);
    // The fresh corps stands in the works the worn one dug; the worn one, in the rear, has none.
    expect(bs[0].dug).toBe(true);
    expect(worksOf(bs[0])).toBeCloseTo(M, 6);
    expect(as[0].dug).toBeFalsy();
    expect(as.slice(1).some((c) => c.dug)).toBe(false);
    expect(groundWorks(g, a.id)).toBeNull();
    // Both marched, and both provinces hold what they held.
    expect(as[0].ap).toBeLessThan(ap.a);
    expect(bs[0].ap).toBeLessThan(ap.b);
    for (const r of [a, b]) expect(r.units.filter((u) => u.kind !== 'fort' && !u.dead).length).toBe(STACK);
    // The works it took over are its own when the month closes: it marched, so it digs nothing more, and loses nothing.
    endPhase(g, 'GER');
    expect(worksOf(bs[0])).toBeCloseTo(M, 6);
  });

  it('not without the moves, not across the map, not with a garrison, and not in a historical scenario', () => {
    const g = playing('GER');
    const { as, bs } = full(g);
    bs[1].ap = 0;
    expect(whyNoSwap(g, as[1], bs[1])).toMatch(/has not the moves/);
    expect(orderSwap(g, as[1], bs[1])).toBe(false);
    expect(whyNoSwap(g, as[1], as[2])).toMatch(/neighbouring/);
    const far = g.corps.find((c) => c.nat === 'GER' && !c.dead && c.kind !== 'fort' && c.reg !== as[0].reg && c.reg !== bs[0].reg && !g.regions[as[0].reg].nb.includes(c.reg))!;
    expect(whyNoSwap(g, as[1], far)).toMatch(/neighbouring/);
    const guns = fresh(addCorps(g, 'GER', 'Sappers', 'eng', 1, as[0].reg, {}));
    expect(whyNoSwap(g, guns, bs[2])).toMatch(/Engineers/);
    // A corps of a country the player is not allied with is no partner, and the computer's month is not the player's.
    const h = createGame(3, 'GER');
    const hc = h.corps.filter((c) => c.nat === 'GER' && !c.dead && c.kind !== 'fort');
    expect(whyNoSwap(h, hc[0], hc[1])).toMatch(/grand campaign/);
  });
});

describe('a war with every staff at work', () => {
  it('sees main efforts named, fought and ended, with nothing in the save that is not a number', () => {
    const g = startOpen(createGame(21, null));
    const named = new Set<string>();
    let ended = 0, live = 0;
    const before = new Map<string, number>();
    while (g.turn <= 40) {
      runTurnAI(g);
      const now = new Map<string, number>();
      for (const n of Object.values(g.nations)) {
        for (const e of effortsOf(g, n.id)) {
          named.add(`${n.id}:${e.from}:${e.first}`);
          now.set(`${n.id}:${e.from}:${e.first}`, e.to);
          expect(e.to - e.from).toBe(RIVALS.effort.months); // a month of massing first (Version 67), then the months of the offensive
          expect(g.regions[e.target]).toBeDefined();
          live++;
        }
      }
      for (const k of before.keys()) if (!now.has(k)) ended++;
      before.clear();
      for (const [k, v] of now) before.set(k, v);
    }
    expect(named.size).toBeGreaterThan(0);
    expect(ended).toBeGreaterThan(0);
    expect(live).toBeGreaterThan(named.size);
    for (const [k, v] of Object.entries(g.flags)) if (k.startsWith('st:eff') || k.startsWith('capOut:')) expect(typeof v).toBe('number');
    // Every staff at war is shown, and with nobody at the table their plans are open.
    for (const v of rivalStaffs(g)) expect(v.source).toBe('ally');
  }, 120000);
});
