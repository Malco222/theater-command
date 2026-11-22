// Neighbours join the fight (release 2; the design director's ruling of 22 November 2026, 03:15Z, Brian's approval).
// A corps next door to a battle, of either side, comes up and fights in it: priced on the campaign card in every
// battle, fought by hand or settled by the staffs, by the share of the battle it is there for, from the week of its
// month the game's own move rules bring it up in (the design director's ruling 1, 11:09Z).
// It stays where it stood, its losses are booked to it, and its month is spent. Behind its own switch (stage.json
// 'join', JOIN in stage.ts), off as it ships: off, no corps ever joins and every battle is release 1's.
//
// The scene: Germany's first month of the grand campaign (seed 3, the player in Germany's seat). Lodz is held by
// Army Lodz; IV Corps attacks it alone from Upper Silesia. Next door stand the rest of Germany's southern armies
// (Upper and Lower Silesia) and seven corps of Poland's (Warsaw, Polish Silesia, Poznan twice, Pomorze, Mlawa, Kielce),
// with Warsaw's Defence Command inside its town.
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import stageData from '../data/campaign/stage.json';
import brianNov39 from '../tools/save-guard/saves/brian-save-usa-1939-11-turn3.json';
import brianDec40 from '../tools/save-guard/saves/brian-save-usa-1940-12-turn16.json';
import joinData from '../data/campaign/join.json';
import { addCorps, assess, atWar, battleCost, createGame, declareWar, endPhase, ensureSupply, hostileUnits, limitAttackers, men, moveCost, relocate, resolveBattle, screens, startPhase, weekOf, type Assessment } from '../src/campaign/game';
import { JOIN, JOINING, JOIN_ATTACK, JOIN_DEFENCE, MAY_JOIN, joinShare, joinWord, joinersOf, marchWeeks, mayJoin, weeksInHand } from '../src/campaign/join';
import { identify } from '../src/campaign/intel';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { attackOutlook, beginHuman, candidates, humanUp, orderAttack, orderHoldBack, runTurnAI, stepAI } from '../src/campaign/turn';
import { STACK, type BattleReport, type Corps, type Game, type Region } from '../src/campaign/types';

/** The player's first month in a seat: the computer's months before his are played. */
function seat(nat: string, seed = 3, open = true): Game {
  const g = open ? startOpen(createGame(seed, nat)) : createGame(seed, nat);
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const corps = (g: Game, nat: string, name: string): Corps => g.corps.find((c) => c.nat === nat && c.name === name && !c.dead)!;
/**
 * The staffs' front rule, screen (ai.ts advance; the design director's ruling 3): two corps stay to screen any place
 * that faces the enemy, so a computer staff's corps that stand one or two to such a province never come up from it.
 * Where a test is about something else, two test corps of that side, of the kind of the first corps there, are raised
 * in each province next door to the battle where the side stands. They have had a battle this month, so they never
 * come up themselves: they stand in the screen, and the real corps there may come up.
 */
function screenFor(g: Game, target: Region, nat: string): void {
  for (const j of target.nb) {
    const r = g.regions[j], first = r.units.find((u) => u.nat === nat && u.kind !== 'fort');
    if (!first) continue;
    for (const k of [1, 2]) {
      const c = addCorps(g, nat, `Test Screen ${k} (${r.name})`, first.kind, 1, r.id, {});
      c.fought = true; c.foughtIn = g.turn;
    }
  }
}
/** Germany's seat at Lodz, IV Corps to attack it alone; with 'screen', Poland's front rule met next door (screenFor). */
function lodz(o: { open?: boolean; screen?: boolean } = {}): { g: Game; lodz: Region; iv: Corps } {
  const g = seat('GER', 3, o.open ?? true);
  if (o.screen) screenFor(g, g.byName['Lodz'], 'POL');
  return { g, lodz: g.byName['Lodz'], iv: corps(g, 'GER', 'IV Corps') };
}
/** The real corps of a nation (not the test corps of the screen). */
const real = (c: Corps): boolean => !c.name.startsWith('Test ');
const ids = (a: Assessment, side: 'att' | 'def'): number[] => (a.join?.[side] ?? []).map((j) => j.id);
const joinedUnits = (rep: BattleReport): number => [...rep.attackers, ...rep.defenders].filter((u) => u.joined !== undefined).length;
/** Everything of the field next door to a province, both sides. */
const nextDoor = (g: Game, r: Region): Corps[] => r.nb.flatMap((j) => g.regions[j].units);

beforeEach(() => { JOIN.on = true; });
afterEach(() => { JOIN.on = false; });

describe('the switch', () => {
  it('ships off, and off no corps joins, the card has no new line and nothing new is written', () => {
    JOIN.on = false;
    expect((stageData as { join?: boolean }).join).toBe(false);
    const { g, lodz: target, iv } = lodz();
    const a = assess(g, [iv], target);
    expect(a.join).toBeUndefined();
    expect('join' in a).toBe(false);
    expect(a.lines.some((l) => l.label === JOIN_ATTACK || l.label === JOIN_DEFENCE)).toBe(false);
    expect(joinersOf(g, [iv], target, a.defenders)).toBeNull();
    const o = attackOutlook(g, [iv], target)!;
    expect('join' in o).toBe(false);
    // The player's hold-back order is refused, and writes nothing.
    const xvi = corps(g, 'GER', 'XVI Corps');
    expect(orderHoldBack(g, xvi, true)).toBe(false);
    expect('holdBack' in xvi).toBe(false);
    const rep = orderAttack(g, [iv], target)!;
    expect(joinedUnits(rep)).toBe(0);
    // A month of every staff's battles after it: nobody joins one, and no corps carries the new fields.
    runTurnAI(g);
    expect(g.reports.every((r) => joinedUnits(r) === 0)).toBe(true);
    expect(g.corps.some((c) => 'joinedIn' in c || 'holdBack' in c || 'foughtIn' in c)).toBe(false);
    expect(saveGame(g)).not.toMatch(/"joinedIn"|"holdBack"|"foughtIn"/);
  });

  it('on, with nobody next door able to come up, the card, the odds, the dice and the bill are those of the switch off', () => {
    const play = (on: boolean): { a: Assessment; rep: BattleReport; rng: string; cost: string } => {
      JOIN.on = on;
      const { g, lodz: target, iv } = lodz();
      // Everyone next door has fought already this month: nobody can join.
      for (const c of nextDoor(g, target)) { c.fought = true; c.foughtIn = g.turn; }
      const a = assess(g, [iv], target);
      const cost = JSON.stringify(battleCost(g, a, [iv], target));
      const rep = resolveBattle(g, [iv], target);
      return { a, rep, rng: JSON.stringify(g.rng), cost };
    };
    const off = play(false), on = play(true);
    expect(on.a.join).toBeUndefined();
    expect(on.a.A).toBe(off.a.A);
    expect(on.a.D).toBe(off.a.D);
    expect(on.a.odds).toBe(off.a.odds);
    expect(on.a.lines).toEqual(off.a.lines);
    expect(on.cost).toBe(off.cost);
    expect(on.rep).toEqual(off.rep);
    expect(on.rng).toBe(off.rng);
  });
});

describe('who joins', () => {
  it('corps of both sides next door, of their own side, in the field and fit to march in: at most five a side, the earliest first', () => {
    const { g, lodz: target, iv } = lodz({ screen: true });
    const a = assess(g, [iv], target);
    expect(a.join).toBeDefined();
    const { att, def } = a.join!;
    expect(att.length).toBe(STACK);
    expect(def.length).toBe(STACK);
    for (const j of [...att, ...def]) {
      const c = j.corps, from = g.regions[j.from];
      expect(target.nb).toContain(j.from);
      expect(c.reg).toBe(j.from);
      expect(c.supplied && !c.fought && c.town === undefined && c.kind !== 'fort').toBe(true);
      expect(c.ap).toBeGreaterThanOrEqual(Math.min(moveCost(g, c, target), 4));
      // Its side holds the province it stands in.
      expect(from.ctrl).toBe(c.nat);
    }
    expect(new Set(att.map((j) => j.nat))).toEqual(new Set(['GER']));
    expect(new Set(def.map((j) => j.nat))).toEqual(new Set(['POL']));
    expect(att.some((j) => j.id === iv.id) || def.some((j) => a.defenders.includes(j.corps))).toBe(false);
    // Seven Polish corps stand next door: the five that come up soonest join, the other two do not. (In the same week,
    // the corps with the lower number first.)
    const able = nextDoor(g, target).filter((c) => c.nat === 'POL' && real(c));
    expect(able.length).toBe(7);
    const last = def[def.length - 1];
    for (const c of able.filter((c) => !ids(a, 'def').includes(c.id))) {
      const w = marchWeeks(g, c, target);
      expect(w > last.weeks || (w === last.weeks && c.id > last.id)).toBe(true);
    }
    // Each side's list is in the order they come up: by the week, then the corps' number.
    for (const side of [att, def]) for (let i = 1; i < side.length; i++) expect(side[i].weeks > side[i - 1].weeks || (side[i].weeks === side[i - 1].weeks && side[i].id > side[i - 1].id)).toBe(true);
  });

  it('never the town guard, a garrison or a corps on ground its side does not hold', () => {
    const { g, lodz: target, iv } = lodz();
    // Warsaw is next door, with its Defence Command inside the town: a garrison never comes out to join.
    const warsaw = g.byName['Warsaw'];
    const command = (warsaw.garr ?? []).find((c) => c.name === 'Warsaw Defence Command')!;
    expect(command).toBeDefined();
    expect(ids(assess(g, [iv], target), 'def')).not.toContain(command.id);
    // A German corps round Warsaw (its field cleared, the town still Polish) is besieging a town, not on its side's ground.
    for (const c of [...warsaw.units]) relocate(g, c, g.byName['Lublin']);
    const xiii = corps(g, 'GER', 'XIII Corps');
    relocate(g, xiii, warsaw);
    expect(warsaw.ctrl).toBe('POL');
    expect(xiii.reg).toBe(warsaw.id);
    expect(xiii.town).toBeUndefined();
    // (Fed and fresh, so that only the ground it stands on keeps it out.)
    ensureSupply(g);
    xiii.supplied = true;
    const a = assess(g, [iv], target);
    expect(ids(a, 'att')).not.toContain(xiii.id);
    // The guard of Lodz itself fights in the battle (ruling 232) as it did: it is no corps and joins nothing.
    expect(a.guard).toBeGreaterThan(0);
    expect([...a.join!.att, ...a.join!.def].every((j) => g.corps.includes(j.corps))).toBe(true);
  });

  it('not unfed, not after a battle this month, not without the weeks to march in', () => {
    const { g, lodz: target, iv } = lodz({ screen: true });
    ensureSupply(g);
    const base = assess(g, [iv], target);
    const pomorze = corps(g, 'POL', 'Army Pomorze'), poznan = corps(g, 'POL', 'Army Poznan'), krakow = corps(g, 'POL', 'Army Krakow (Silesia)');
    for (const c of [pomorze, poznan, krakow]) expect(ids(base, 'def')).toContain(c.id);
    pomorze.supplied = false;
    // (A battle earlier this month, in another nation's phase; and, Poland's month being still to come, no week of it
    // spent: what Army Krakow left unspent of last month is no part of this one.)
    poznan.fought = true; poznan.foughtIn = g.turn;
    krakow.ap = 0;
    const a = assess(g, [iv], target);
    for (const c of [pomorze, poznan]) expect(ids(a, 'def')).not.toContain(c.id);
    expect(ids(a, 'def')).toContain(krakow.id);
    // Germany's own month has begun: a German corps that has fought since, or has no weeks left of it, stays out.
    const xv = corps(g, 'GER', 'XV Corps'), xiv = corps(g, 'GER', 'XIV Corps');
    for (const c of [xv, xiv]) expect(ids(a, 'att')).toContain(c.id);
    xv.fought = true; xiv.ap = 0;
    const a2 = assess(g, [iv], target);
    for (const c of [xv, xiv]) expect(ids(a2, 'att')).not.toContain(c.id);
    // Infantry needs two weeks to march onto enemy ground: with one week in hand it cannot march in at all.
    const xi = corps(g, 'GER', 'XI Corps');
    expect(moveCost(g, xi, target)).toBe(2);
    xi.ap = 1;
    expect(mayJoin(g, xi, target)).toBe(false);
  });

  it("allies at war with the enemy join; neutrals, and allies not in this war, never do", () => {
    const g = seat('GER');
    const przemysl = g.byName['Przemysl'], slovakia = g.byName['Slovakia'], debrecen = g.byName['Debrecen'];
    expect(przemysl.nb).toContain(slovakia.id);
    expect(przemysl.nb).toContain(debrecen.id);
    const xviii = corps(g, 'GER', 'XVIII Corps'), slovak = corps(g, 'SVK', 'Army Bernolak'), magyar = debrecen.units.find((c) => c.nat === 'HUN')!;
    // (Slovakia's front rule met, screenFor; and Army Bernolak's hold lifted: a computer staff's corps under a hold
    // stays where it is, the design director's ruling 3.)
    screenFor(g, przemysl, 'SVK');
    slovak.hold = false;
    // An Italian corps beside the Slovaks: Germany's ally, not at war with Poland.
    const italian = addCorps(g, 'ITA', 'Test Italian Corps', 'inf', 2, 'Slovakia', {});
    expect(atWar(g, 'SVK', 'POL') && !atWar(g, 'HUN', 'POL') && !atWar(g, 'ITA', 'POL')).toBe(true);
    const a = assess(g, [xviii], przemysl);
    expect(ids(a, 'att')).toContain(slovak.id);
    expect(ids(a, 'att')).not.toContain(italian.id);
    expect([...ids(a, 'att'), ...ids(a, 'def')]).not.toContain(magyar.id);
    expect(a.join!.def.every((j) => j.nat === 'POL')).toBe(true);
  });
});

describe("who stays: a staff's hold, and its front rule (the design director's ruling 3)", () => {
  it("a computer staff's corps under a hold stays, the Berlin garrison and the Ploiesti guard among them; the player's own does not", () => {
    // A war nobody plays: Army Poznan falls on Neumark, next door to Berlin; Bulgaria, at war with Romania, on Bucharest, next door to Ploiesti.
    const g = startOpen(createGame(3, null));
    const garrison = corps(g, 'GER', 'Berlin garrison'), guard = corps(g, 'ROM', 'Ploiesti guard');
    expect(garrison.hold && guard.hold).toBe(true);
    const neumark = g.byName['Neumark'], bucharest = g.byName['Bucharest'];
    expect(neumark.nb).toContain(garrison.reg);
    expect(bucharest.nb).toContain(guard.reg);
    declareWar(g, 'BUL', 'ROM');
    const poznan = corps(g, 'POL', 'Army Poznan'), bulgar = corps(g, 'BUL', '1st Army');
    const counted = (): boolean[] => [ids(assess(g, [poznan], neumark), 'def').includes(garrison.id), ids(assess(g, [bulgar], bucharest), 'def').includes(guard.id)];
    expect(counted()).toEqual([false, false]);
    // Their holds lifted, both come up.
    garrison.hold = false; guard.hold = false;
    expect(counted()).toEqual([true, true]);
    // In Germany's seat the Berlin garrison is the player's own: a hold binds a staff, never him.
    const s = seat('GER');
    const own = corps(s, 'GER', 'Berlin garrison');
    expect(own.hold).toBe(true);
    expect(ids(assess(s, [corps(s, 'POL', 'Army Poznan')], s.byName['Neumark']), 'def')).toContain(own.id);
  });

  it("a staff's corps its front rule keeps stays: two corps stay to screen any place that faces the enemy (screen); the player's are under no front rule", () => {
    const { g, lodz: target, iv } = lodz();
    // Poznan holds Army Poznan and the Wielkopolska Cavalry, and faces the Germans next door: the rule keeps both.
    const at = g.byName['Poznan'], poznan = corps(g, 'POL', 'Army Poznan'), cavalry = corps(g, 'POL', 'Wielkopolska Cavalry');
    expect(at.units.map((c) => c.id)).toEqual([poznan.id, cavalry.id]);
    expect(at.nb.some((j) => hostileUnits(g, g.regions[j], 'POL').length > 0)).toBe(true);
    for (const c of [poznan, cavalry]) {
      expect(mayJoin(g, c, target)).toBe(true);
      expect(screens(g, c, 'POL')).toBe(true);
    }
    let a = assess(g, [iv], target);
    for (const c of [poznan, cavalry]) expect(ids(a, 'def')).not.toContain(c.id);
    // A third Pole raised there: the earliest to come up comes (the week, then the corps' number), and two stay.
    const third = addCorps(g, 'POL', 'Test Third Pole', 'inf', 1, at.id, {});
    expect(mayJoin(g, third, target)).toBe(true);
    a = assess(g, [iv], target);
    expect(ids(a, 'def')).toContain(poznan.id);
    for (const c of [cavalry, third]) expect(ids(a, 'def')).not.toContain(c.id);
    // Once it has joined, it is gone from Poznan's count: Poznan sends nobody to a second card this month.
    orderAttack(g, [iv], target);
    expect(poznan.joinedIn).toBe(g.turn);
    expect(at.units.map((c) => c.id)).toEqual([poznan.id, cavalry.id, third.id]);
    const pomorze = g.byName['Pomorze'], xix = corps(g, 'GER', 'XIX Corps');
    expect(pomorze.nb).toContain(at.id);
    expect(pomorze.nb).toContain(xix.reg);
    for (const c of [cavalry, third]) expect(mayJoin(g, c, pomorze)).toBe(true);
    const b = assess(g, [xix], pomorze);
    expect(ids(b, 'def').some((id) => at.units.some((c) => c.id === id))).toBe(false);
    // The player's own corps are under no front rule: all three of Lower Silesia come up at Lodz, though it faces the Poles.
    const s = lodz();
    const silesians = s.g.byName['Lower Silesia'].units;
    expect(silesians).toHaveLength(3);
    expect(s.g.byName['Lower Silesia'].nb.some((j) => hostileUnits(s.g, s.g.regions[j], 'GER').length > 0)).toBe(true);
    for (const c of silesians) expect(ids(assess(s.g, [s.iv], s.lodz), 'att')).toContain(c.id);
    // In a war nobody plays the same corps are a staff's: Lower Silesia keeps two, and one comes up.
    const w = startOpen(createGame(3, null)), lw = w.byName['Lower Silesia'];
    const nobody = assess(w, [corps(w, 'GER', 'IV Corps')], w.byName['Lodz']);
    expect(ids(nobody, 'att').filter((id) => lw.units.some((c) => c.id === id))).toHaveLength(lw.units.length - 2);
  });
});

describe('when they come up', () => {
  it("by the march engine's time, never by the dice: the same game always gives the same joiners, and the luck is untouched", () => {
    const { g, lodz: target, iv } = lodz();
    const rng = JSON.stringify(g.rng);
    const a = assess(g, [iv], target), b = assess(g, [iv], target);
    expect(JSON.stringify(g.rng)).toBe(rng);
    const view = (x: Assessment): unknown => [...x.join!.att, ...x.join!.def].map((j) => [j.id, j.weeks, j.share, j.word]);
    expect(view(b)).toEqual(view(a));
    // A second game of the same seed, in the same state, names the same joiners at the same shares.
    const again = lodz();
    expect(view(assess(again.g, [again.iv], again.lodz))).toEqual(view(a));
    // (Every joiner: the week the move rules give its march in, out of the weeks it has in hand, and the share and word of that week.)
    for (const j of [...a.join!.att, ...a.join!.def]) {
      const c = j.corps;
      expect(j.weeks).toBe(weekOf(c, moveCost(g, c, target), weeksInHand(g, c)));
      expect(j.weeks).toBe(marchWeeks(g, c, target));
      expect(Number.isInteger(j.weeks) && j.weeks >= 1 && j.weeks <= 3).toBe(true);
      expect(j.share).toBe(1 - j.weeks / JOINING.window);
      expect(j.word).toBe(joinWord(j.share));
    }
    // From the same province lorries come up in the first week and infantry in the second, so more of them counts.
    const xiv = a.join!.att.find((j) => j.name === 'XIV Corps')!, x = a.join!.att.find((j) => j.name === 'X Corps')!;
    expect(xiv.from).toBe(x.from);
    expect(xiv.weeks).toBe(1);
    expect(x.weeks).toBe(2);
    expect(xiv.share).toBeGreaterThan(x.share);
    // Early, midway, late: the thirds of the share.
    expect([joinWord(0.9), joinWord(0.5), joinWord(0.2)]).toEqual(['early', 'midway', 'late']);
    expect(joinShare(JOINING.window)).toBe(0);
    expect(a.join!.att.map((j) => j.word)).toContain('midway');
  });

  it('the rule carries one number, the window of four weeks: no step, no nearest, no rail, no least, and no share above three quarters', () => {
    expect(Object.keys(joinData).sort()).toEqual(['_about', 'window']);
    expect(JOINING.window).toBe(4);
    const { g, lodz: target, iv } = lodz();
    const a = assess(g, [iv], target);
    const all = [...a.join!.att, ...a.join!.def];
    expect(all.length).toBeGreaterThan(0);
    for (const j of all) expect([0.75, 0.5, 0.25]).toContain(j.share);
    expect(Math.max(...all.map((j) => j.share))).toBe(0.75);
  });

  it('a corps that would come up in the last week of the month is no part of the battle', () => {
    const { g, lodz: target, iv } = lodz();
    // (Room among the five: X and XIII Corps have had a battle this month.)
    for (const n of ['X Corps', 'XIII Corps']) { const c = corps(g, 'GER', n); c.fought = true; c.foughtIn = g.turn; }
    // XI Corps, infantry with four weeks a month, costs two to march onto enemy ground: with three in hand it is up in the third week, late.
    const xi = corps(g, 'GER', 'XI Corps'), xvi = corps(g, 'GER', 'XVI Corps');
    expect(xi.apMax).toBe(4);
    expect(moveCost(g, xi, target)).toBe(2);
    xi.ap = 3;
    let j = assess(g, [iv], target).join!.att.find((x) => x.id === xi.id)!;
    expect(j.weeks).toBe(3);
    expect(j.share).toBe(0.25);
    expect(j.word).toBe('late');
    // With two in hand it may march in, but it is up in the fourth week: not on the card.
    xi.ap = 2;
    expect(mayJoin(g, xi, target)).toBe(true);
    expect(marchWeeks(g, xi, target)).toBe(4);
    expect(ids(assess(g, [iv], target), 'att')).not.toContain(xi.id);
    // XVI Corps, tanks with five weeks, one in hand: the fourth week too.
    expect(xvi.apMax).toBe(5);
    expect(moveCost(g, xvi, target)).toBe(1);
    xvi.ap = 1;
    expect(mayJoin(g, xvi, target)).toBe(true);
    expect(marchWeeks(g, xvi, target)).toBe(4);
    expect(ids(assess(g, [iv], target), 'att')).not.toContain(xvi.id);
    // (A tank corps fresh for the month comes up in the first.)
    xvi.ap = xvi.apMax;
    j = assess(g, [iv], target).join!.att.find((x) => x.id === xvi.id)!;
    expect(j.weeks).toBe(1);
  });

  it('each joiner counts in the odds by its share, on both sides', () => {
    const { g, lodz: target, iv } = lodz({ screen: true });
    const on = assess(g, [iv], target);
    JOIN.on = false;
    const off = assess(g, [iv], target);
    JOIN.on = true;
    expect(on.A).toBeGreaterThan(off.A);
    expect(on.D).toBeGreaterThan(off.D);
    // Every German next door held back, the attack is reckoned as with the switch off and the Polish joiners are still in the defence.
    for (const c of nextDoor(g, target)) if (c.nat === 'GER') expect(orderHoldBack(g, c, true)).toBe(true);
    const held = assess(g, [iv], target);
    expect(held.join!.att).toHaveLength(0);
    expect(held.A).toBe(off.A);
    expect(held.D).toBe(on.D);
  });
});

describe('the battle', () => {
  it('books losses to the joiners, who stay where they stood with their month spent', () => {
    const { g, lodz: target, iv } = lodz({ screen: true });
    const a = assess(g, [iv], target);
    const joiners = [...a.join!.att, ...a.join!.def].map((j) => ({ c: j.corps, reg: j.corps.reg, men: men(j.corps), share: j.share, side: a.join!.att.includes(j) ? 'att' : 'def' }));
    const rep = orderAttack(g, [iv], target)!;
    expect(rep).not.toBeNull();
    // The joiners are in the report after everyone else, with their share, the province they came from and when.
    const attJ = rep.attackers.filter((u) => u.joined !== undefined), defJ = rep.defenders.filter((u) => u.joined !== undefined);
    expect(attJ).toHaveLength(STACK);
    expect(defJ).toHaveLength(STACK);
    expect(rep.attackers.slice(-attJ.length)).toEqual(attJ);
    expect(rep.defenders.slice(-defJ.length)).toEqual(defJ);
    for (const j of joiners) {
      const u = [...attJ, ...defJ].find((x) => x.corps === j.c.id)!;
      expect(u.lost).toBeGreaterThan(0);
      expect(u.lost).toBe(j.men - men(j.c));
      expect(u.from).toBe(j.reg);
      expect(u.joined).toBeCloseTo(j.share, 3);
      expect(['early', 'midway', 'late']).toContain(u.arrive);
      expect(u.fate).toBe('');
      // Home, spent.
      expect(j.c.reg).toBe(j.reg);
      expect(j.c.dead).toBe(false);
      expect(j.c.ap).toBe(0);
      expect(j.c.fought).toBe(true);
      expect(j.c.joinedIn).toBe(g.turn);
    }
    expect(rep.attLost).toBe(rep.attackers.reduce((s, u) => s + u.lost, 0));
    // (Lodz's guard was taken prisoner with the town; Army Lodz fell back. Everyone else's losses are the defence's bill.)
    const paid = rep.defenders.filter((u) => !(u.guard && u.fate === 'surrendered'));
    expect(paid.every((u) => u.fate !== 'surrendered' && u.fate !== 'destroyed')).toBe(true);
    expect(rep.defLost).toBe(paid.reduce((s, u) => s + u.lost, 0));
    // Lodz was won: IV Corps marched in, and nobody who joined did.
    if (rep.taken) expect(target.units.some((c) => joiners.some((j) => j.c === c))).toBe(false);
    // A German joiner cannot attack later this month.
    const xvi = corps(g, 'GER', 'XVI Corps');
    expect(joiners.some((j) => j.c === xvi)).toBe(true);
    const silesia = g.byName['Polish Silesia'];
    expect(candidates(g, silesia)).not.toContain(xvi);
    expect(orderAttack(g, [xvi], silesia)).toBeNull();
    // Nor join another battle this month.
    const viii = corps(g, 'GER', 'VIII Corps');
    expect(silesia.nb).toContain(viii.reg);
    const other = assess(g, [viii], silesia);
    expect(other.join!.att.length).toBeGreaterThan(0);
    expect(ids(other, 'att')).not.toContain(xvi.id);
  });

  it("a defender's joiner whose own month comes later in the month has it spent too, and the next month is its own again", () => {
    const { g, lodz: target, iv } = lodz({ screen: true });
    const a = assess(g, [iv], target);
    const pole = a.join!.def[0].corps, other = corps(g, 'POL', 'Army Prusy');
    expect(ids(a, 'def')).not.toContain(other.id);
    // It stands in works of two months' digging where it is.
    pole.dug = true; pole.dugFor = 2;
    orderAttack(g, [iv], target);
    expect(pole.joinedIn).toBe(g.turn);
    startPhase(g, 'POL');
    expect(pole.ap).toBe(0);
    expect(other.ap).toBe(other.apMax);
    // It stayed where it stood: at its month's end it keeps its works, and has dug no further.
    endPhase(g, 'POL');
    expect(pole.dug).toBe(true);
    expect(pole.dugFor).toBe(2);
    g.turn += 1;
    startPhase(g, 'POL');
    expect(pole.ap).toBe(pole.apMax);
    expect(pole.fought).toBe(false);
    expect('joinedIn' in pole).toBe(false);
  });

  it("the bill on the attack card counts the joiners as the battle books them", () => {
    const { g, lodz: target, iv } = lodz({ screen: true });
    const a = assess(g, [iv], target);
    const bare: Assessment = { ...a, join: undefined };
    const withJ = battleCost(g, a, [iv], target), without = battleCost(g, bare, [iv], target);
    for (const k of ['won', 'lost'] as const) {
      if (!withJ[k]) continue;
      expect(withJ[k]!.ours).toBeGreaterThan(without[k]!.ours);
      expect(withJ[k]!.theirs).toBeGreaterThan(without[k]!.theirs);
    }
  });
});

describe("the player's hold-back", () => {
  it('keeps his corps out of the battles next door until he lets them go; he cannot hold back the enemy', () => {
    const { g, lodz: target, iv } = lodz();
    const xvi = corps(g, 'GER', 'XVI Corps');
    expect(ids(assess(g, [iv], target), 'att')).toContain(xvi.id);
    expect(orderHoldBack(g, xvi, true)).toBe(true);
    expect(xvi.holdBack).toBe(true);
    const held = assess(g, [iv], target);
    expect(ids(held, 'att')).not.toContain(xvi.id);
    // (The next corps to come up takes its place among the five.)
    expect(held.join!.att).toHaveLength(STACK);
    expect(orderHoldBack(g, corps(g, 'POL', 'Army Prusy'), true)).toBe(false);
    expect(orderHoldBack(g, xvi, false)).toBe(true);
    expect('holdBack' in xvi).toBe(false);
    expect(ids(assess(g, [iv], target), 'att')).toContain(xvi.id);
    // A corps held back fights its own battles as before.
    orderHoldBack(g, xvi, true);
    const rep = orderAttack(g, [xvi], g.byName['Polish Silesia']);
    expect(rep).not.toBeNull();
    expect(rep!.attackers[0].name).toBe('XVI Corps');
  });
});

describe("the computer staffs", () => {
  it("read the joiners in the odds they choose their attacks by, and their joiners are spent for the month", () => {
    const g = startOpen(createGame(3, null));
    const target = g.byName['Lodz'];
    // The force a staff weighs for an attack is cut to the roads (limitAttackers) and judged by assess's odds.
    const pool = limitAttackers(g, [corps(g, 'GER', 'IV Corps')], target);
    const on = assess(g, pool, target);
    JOIN.on = false;
    const off = assess(g, pool, target);
    JOIN.on = true;
    expect(on.join!.att.length + on.join!.def.length).toBeGreaterThan(0);
    expect(on.odds).not.toBe(off.odds);
    expect(on.odds).toBeCloseTo(on.A / on.D, 12);
    // Germany's whole first month, on the computer: its battles have joiners on both sides, booked in the losses.
    const from = g.reports.length;
    stepAI(g);
    const reps = g.reports.slice(from).filter((r) => !r.yielded);
    expect(reps.some((r) => r.attackers.some((u) => u.joined !== undefined))).toBe(true);
    expect(reps.some((r) => r.defenders.some((u) => u.joined !== undefined))).toBe(true);
    for (const r of reps) expect(r.attLost).toBe(r.attackers.reduce((s, u) => s + u.lost, 0));
    // A corps that joined a battle is in no later battle of the staff's month, as attacker or joiner.
    const spent = new Set<number>();
    for (const r of reps) {
      for (const u of r.attackers) if (u.joined === undefined) expect(spent.has(g.corps.find((c) => c.name === u.name && c.nat === u.nat)?.id ?? -1)).toBe(false);
      for (const u of r.attackers) if (u.corps !== undefined) { expect(spent.has(u.corps)).toBe(false); spent.add(u.corps); }
    }
  });
});

describe('the battle card', () => {
  it('names the joiners early, midway or late; the Forces line is as it was', () => {
    const { g, lodz: target, iv } = lodz({ screen: true });
    const a = assess(g, [iv], target);
    const att = a.lines.find((l) => l.label === JOIN_ATTACK)!, def = a.lines.find((l) => l.label === JOIN_DEFENCE)!;
    expect(att.value).toMatch(/^Early: XVI Corps from Upper Silesia/);
    expect(att.value).toMatch(/; midway: X Corps from Lower Silesia, XIII Corps from Lower Silesia$/);
    expect(def.value).toMatch(/^Early: Army Pomorze from Pomorze/);
    expect(att.effect).toMatch(/They stay where they stand, and their month is spent$/);
    for (const l of [att, def]) expect(`${l.value} ${l.effect}`).not.toMatch(/undefined|NaN|null/);
    // (Right after the Forces, the ground and the town; the Forces line itself unchanged, so the card's readers still read it.)
    const at = a.lines.indexOf(att);
    expect(a.lines.slice(0, at).map((l) => l.label)).toEqual(['Forces', 'Terrain', 'The town']);
    JOIN.on = false;
    expect(a.lines[0]).toEqual(assess(g, [iv], target).lines[0]);
    expect(a.lines[0].value).toMatch(/ and the town guard of about [\d,]+$/);
  });

  it('to the player an enemy next door his staff has not identified is "armies next door may join": no names, no numbers', () => {
    const { g, lodz: target, iv } = lodz({ screen: true });
    const a = assess(g, [iv], target);
    const o = attackOutlook(g, [iv], target)!;
    expect(o.join!.hidden).toBe(true);
    expect(o.join!.def).toEqual([]);
    expect(o.join!.att.map((j) => j.name)).toEqual(a.join!.att.map((j) => j.name));
    expect(o.lines.find((l) => l.label === JOIN_DEFENCE)).toEqual({ label: JOIN_DEFENCE, value: MAY_JOIN });
    const said = JSON.stringify(o);
    for (const j of a.join!.def) expect(said).not.toContain(j.name);
    expect(o.exact).toBe(false);
    expect(o.lo).toBeLessThan(o.hi);
    expect(o.note).toMatch(/armies next door may join/);
    // Identified (a battle, an agent, a flight), they are named, with their shares.
    identify(g, [...a.defenders, ...a.join!.def.map((j) => j.corps)], 'GER');
    const seen = attackOutlook(g, [iv], target)!;
    expect(seen.join!.hidden).toBe(false);
    expect(seen.join!.def.map((j) => j.name)).toEqual(a.join!.def.map((j) => j.name));
    expect(seen.lines.find((l) => l.label === JOIN_DEFENCE)!.value).toMatch(/^Early: Army Pomorze from Pomorze/);
    expect(seen.exact).toBe(true);
    // Every defender known but the enemy next door not: the defence's own lines stand, and only the band says so.
    const g2 = lodz({ screen: true });
    identify(g2.g, hostileUnits(g2.g, g2.lodz, 'GER'), 'GER');
    const o2 = attackOutlook(g2.g, [g2.iv], g2.lodz)!;
    expect(o2.join!.hidden).toBe(true);
    expect(o2.exact).toBe(false);
    expect(o2.lines.find((l) => l.label === 'Forces')).toEqual(a.lines.find((l) => l.label === 'Forces'));
    expect(o2.note).toMatch(/^Armies next door may join, so the odds are the staff's estimate/);
    expect(o2.narrow).toMatch(/corps next door/);
  });
});

describe('practice battles: the historical scenarios reckon alike', () => {
  it('the same rule prices and books the joiners in a scenario as in the grand campaign', () => {
    const { g, lodz: target, iv } = lodz({ open: false, screen: true });
    expect(g.flags.open).toBeUndefined();
    const a = assess(g, [iv], target);
    expect(a.join!.att.length).toBeGreaterThan(0);
    expect(a.join!.def.length).toBeGreaterThan(0);
    for (const j of [...a.join!.att, ...a.join!.def]) expect(j.share).toBe(joinShare(marchWeeks(g, j.corps, target)));
    const rep = resolveBattle(g, [iv], target);
    expect(joinedUnits(rep)).toBe(a.join!.att.length + a.join!.def.length);
    expect(rep.lines.some((l) => l.label === JOIN_ATTACK)).toBe(true);
  });
});

describe('saves', () => {
  it('keep what the rule writes, and give it back as it was', () => {
    const { g, lodz: target, iv } = lodz();
    const xvi = corps(g, 'GER', 'XVI Corps');
    orderHoldBack(g, xvi, true);
    const rep0 = orderAttack(g, [iv], target)!;
    const text = saveGame(g);
    const back = loadGame(text)!;
    expect(saveGame(back)).toBe(text);
    const xviBack = back.corps.find((c) => c.id === xvi.id)!;
    expect(xviBack.holdBack).toBe(true);
    expect(xviBack.joinedIn).toBeUndefined();
    expect(back.corps.filter((c) => c.joinedIn === g.turn).map((c) => c.id).sort()).toEqual([...rep0.attackers, ...rep0.defenders].filter((u) => u.joined !== undefined).map((u) => u.corps!).sort());
    const rep = back.reports[back.reports.length - 1];
    expect(rep).toEqual(rep0);
    expect(rep.attackers.some((u) => u.joined !== undefined && u.arrive !== undefined && u.from !== undefined && u.corps !== undefined)).toBe(true);
  });

  it('saved without the new fields, load as before: nobody held back, nobody spent, and the rule reads them from there', () => {
    // (Brian's own saved campaigns, from before the rule: the save guard's.)
    for (const saved of [brianNov39, brianDec40] as unknown[]) {
      const text = JSON.stringify(saved);
      expect(text).not.toMatch(/"holdBack"|"joinedIn"/);
      for (const on of [false, true]) {
        JOIN.on = on;
        const g = loadGame(text)!;
        expect(g).not.toBeNull();
        expect(g.corps.some((c) => c.holdBack !== undefined || c.joinedIn !== undefined)).toBe(false);
        // Off, the save is written back as it was read.
        if (!on) expect(saveGame(loadGame(saveGame(g))!)).toBe(saveGame(g));
      }
    }
  });
});
