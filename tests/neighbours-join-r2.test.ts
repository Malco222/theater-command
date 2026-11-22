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
import { NO_MASK, addCorps, assess, atWar, battleCost, createGame, declareWar, endPhase, ensureSupply, hostileUnits, limitAttackers, men, monthWeeks, moveCost, relocate, resolveBattle, screens, startPhase, weekOf, type Assessment } from '../src/campaign/game';
import { JOIN, JOINING, JOIN_ATTACK, JOIN_DEFENCE, MAY_JOIN, battleThisMonth, foughtNow, joinShare, joinWatch, joinWord, joinersOf, marchWeeks, mayJoin, passedOver, staffMask, weeksInHand, weeksOfMonth } from '../src/campaign/join';
import { identify } from '../src/campaign/intel';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { attackOutlook, beginHuman, candidates, heldFromStart, humanUp, joinChoices, mayHold, orderAttack, orderHold, previewLanding, runTurnAI, stepAI, whyNotAttack } from '../src/campaign/turn';
import { fieldCtrl } from '../src/campaign/siege';
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
    // The player's garrison toggle is refused, and writes nothing.
    const xvi = corps(g, 'GER', 'XVI Corps');
    expect(orderHold(g, xvi, true)).toBe(false);
    expect(xvi.hold).toBe(false);
    expect('holdBack' in xvi).toBe(false);
    expect(Object.keys(g.flags).some((k) => k.startsWith('garrison:'))).toBe(false);
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
    // Six German corps stand next door: the five that come up soonest join, the sixth does not. (In the same week, the
    // corps with the lower number first.)
    const able = nextDoor(g, target).filter((c) => c.nat === 'GER' && c !== iv);
    expect(able.length).toBe(6);
    const last = att[att.length - 1];
    const left = able.filter((c) => !ids(a, 'att').includes(c.id));
    expect(left).toHaveLength(1);
    for (const c of left) {
      const w = marchWeeks(g, c, target);
      expect(w > last.weeks || (w === last.weeks && c.id > last.id)).toBe(true);
    }
    // Seven Polish corps stand next door, every one free for this card (the sequence rule: nothing keeps a reserve for
    // another place): the five that come up soonest join, and the other two are cut by the five a side.
    const poles = nextDoor(g, target).filter((c) => c.nat === 'POL' && real(c));
    expect(poles.length).toBe(7);
    const away = poles.filter((c) => !ids(a, 'def').includes(c.id));
    expect(away).toHaveLength(2);
    const lastDef = def[def.length - 1];
    for (const c of away) {
      const w = marchWeeks(g, c, target);
      expect(w > lastDef.weeks || (w === lastDef.weeks && c.id > lastDef.id)).toBe(true);
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
    const krakow = g.byName['Krakow'], przemysl = g.byName['Przemysl'], slovakia = g.byName['Slovakia'], debrecen = g.byName['Debrecen'];
    for (const r of [krakow, przemysl]) expect(r.nb).toContain(slovakia.id);
    expect(przemysl.nb).toContain(debrecen.id);
    const xviii = corps(g, 'GER', 'XVIII Corps'), slovak = corps(g, 'SVK', 'Army Bernolak'), magyar = debrecen.units.find((c) => c.nat === 'HUN')!;
    // (Slovakia's front rule met, screenFor; and Army Bernolak's hold lifted: a computer staff's corps under a hold
    // stays where it is, the design director's ruling 3.)
    screenFor(g, krakow, 'SVK');
    slovak.hold = false;
    // An Italian corps beside the Slovaks: Germany's ally, not at war with Poland.
    const italian = addCorps(g, 'ITA', 'Test Italian Corps', 'inf', 2, 'Slovakia', {});
    expect(atWar(g, 'SVK', 'POL') && !atWar(g, 'HUN', 'POL') && !atWar(g, 'ITA', 'POL')).toBe(true);
    const a = assess(g, [xviii], krakow);
    expect(ids(a, 'att')).toContain(slovak.id);
    expect(ids(a, 'att')).not.toContain(italian.id);
    expect((a.join?.def ?? []).every((j) => j.nat === 'POL')).toBe(true);
    // At Przemysl, with the Hungarians next door (Germany's friends, not at war with Poland): neither they nor the Italians come up.
    const b = assess(g, [xviii], przemysl);
    expect([...ids(b, 'att'), ...ids(b, 'def')]).not.toContain(magyar.id);
    expect(ids(b, 'att')).not.toContain(italian.id);
    // The Slovaks, next to both and not yet come up, stand on both cards.
    expect(ids(b, 'att')).toContain(slovak.id);
  });
});

describe("who stays: the hold, one mark for every corps, and a staff's front rule (the design director's ruling 3, and Q3 folded)", () => {
  it("a corps under a hold stays, a staff's or the player's: the Berlin garrison and the Ploiesti guard among them", () => {
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
    // In Germany's seat the Berlin garrison is the player's own, and its hold from the setup binds it as it binds a
    // staff's (Q3 folded: "A player never lacks a tool the computer has"; mayJoin reads one mark).
    const s = seat('GER');
    const own = corps(s, 'GER', 'Berlin garrison'), berlinAttack = (): Assessment => assess(s, [corps(s, 'POL', 'Army Poznan')], s.byName['Neumark']);
    expect(own.hold).toBe(true);
    expect(mayJoin(s, own, s.byName['Neumark'])).toBe(false);
    expect(ids(berlinAttack(), 'def')).not.toContain(own.id);
    // He lifts it with his toggle, and it comes up.
    expect(orderHold(s, own, false)).toBe(true);
    expect(ids(berlinAttack(), 'def')).toContain(own.id);
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
      expect(j.weeks).toBe(weekOf(c, moveCost(g, c, target), weeksInHand(g, c), weeksOfMonth(g, c)));
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
    // Every German next door made a garrison, the attack is reckoned as with the switch off and the Polish joiners are still in the defence.
    for (const c of nextDoor(g, target)) if (c.nat === 'GER') expect(orderHold(g, c, true)).toBe(true);
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

describe("the player's garrison toggle (Q3 folded: one mark, one toggle, worded Garrison)", () => {
  it('his one toggle sets and lifts the hold, the mark the staffs read: a held corps of his does not join', () => {
    const { g, lodz: target, iv } = lodz();
    const xvi = corps(g, 'GER', 'XVI Corps');
    expect(xvi.hold).toBe(false);
    expect(ids(assess(g, [iv], target), 'att')).toContain(xvi.id);
    // Pressed: the corps' own hold is set, the very mark a computer staff's corps carries; it stays out.
    expect(mayHold(g, xvi)).toBe(true);
    expect(orderHold(g, xvi, true)).toBe(true);
    expect(xvi.hold).toBe(true);
    expect('holdBack' in xvi).toBe(false);
    expect(mayJoin(g, xvi, target)).toBe(false);
    const held = assess(g, [iv], target);
    expect(ids(held, 'att')).not.toContain(xvi.id);
    // (The next corps to come up takes its place among the five.)
    expect(held.join!.att).toHaveLength(STACK);
    // The attack card names it among his garrisons next door, and offers nothing to press.
    expect(joinChoices(g, held, target, [iv]).held.map((j) => j.corps)).toContain(xvi);
    // Not the enemy's corps, nor an ally's that does not march in his army.
    const prusy = corps(g, 'POL', 'Army Prusy'), slovak = corps(g, 'SVK', 'Army Bernolak');
    expect(orderHold(g, prusy, true)).toBe(false);
    expect(prusy.hold).toBe(false);
    const was = slovak.hold;
    expect(orderHold(g, slovak, !was)).toBe(false);
    expect(slovak.hold).toBe(was);
    // Pressed again: lifted, and it comes up.
    expect(orderHold(g, xvi, false)).toBe(true);
    expect(xvi.hold).toBe(false);
    expect(ids(assess(g, [iv], target), 'att')).toContain(xvi.id);
    // A garrison of his still fights where he sends it: the move rules never read his holds.
    orderHold(g, xvi, true);
    const rep = orderAttack(g, [xvi], g.byName['Polish Silesia']);
    expect(rep).not.toBeNull();
    expect(rep!.attackers[0].name).toBe('XVI Corps');
  });

  it('holds from the setup start set, and say so until he presses the toggle', () => {
    const g = seat('GER');
    const mine = g.corps.filter((c) => c.nat === 'GER' && !c.dead && c.hold);
    // Germany's first-day holds: the Berlin garrison, XII Corps at the Saar and the rest.
    expect(mine.length).toBeGreaterThan(3);
    for (const name of ['Berlin garrison', 'XII Corps']) expect(corps(g, 'GER', name).hold).toBe(true);
    for (const c of mine) expect(heldFromStart(g, c)).toBe(true);
    const xii = corps(g, 'GER', 'XII Corps');
    orderHold(g, xii, false);
    expect(heldFromStart(g, xii)).toBe(false);
    orderHold(g, xii, true);
    expect(xii.hold).toBe(true);
    expect(heldFromStart(g, xii)).toBe(false);
    // (The review of Q3, findings 4 and 13.) Only his own nation's corps of the first day's roll, in a campaign played
    // from its first month: a corps that comes later under a hold, as Group XXI does in April 1940, is no garrison from
    // the start, and neither is a hold he finds on taking his seat in a later year.
    const later = addCorps(g, 'GER', 'Group XXI', 'inf', 3, g.byName['Berlin'].id, { hold: 1 });
    expect(later.hold).toBe(true);
    (later as { born?: number }).born = 8;
    expect(heldFromStart(g, later)).toBe(false);
    const berlin = corps(g, 'GER', 'Berlin garrison');
    expect(heldFromStart(g, berlin)).toBe(true);
    g.flags.seat = 22;
    expect(heldFromStart(g, berlin)).toBe(false);
    g.flags.seat = 1;
    expect(heldFromStart(g, berlin)).toBe(true);
    // Nor an ally's corps that marches with him: its hold is its own staff's.
    const slovak = g.corps.find((c) => c.nat === 'SVK' && !c.dead && c.kind !== 'fort')!;
    slovak.hold = true;
    expect(heldFromStart(g, slovak)).toBe(false);
  });

  it('with the switch off the toggle does not exist: nothing is written, and the holds are release 1\'s', () => {
    const { g } = lodz();
    JOIN.on = false;
    const xvi = corps(g, 'GER', 'XVI Corps'), xii = corps(g, 'GER', 'XII Corps');
    const flags = JSON.stringify(g.flags);
    expect(mayHold(g, xvi)).toBe(false);
    expect(orderHold(g, xvi, true)).toBe(false);
    expect(orderHold(g, xii, false)).toBe(false);
    expect([xvi.hold, xii.hold]).toEqual([false, true]);
    expect(JSON.stringify(g.flags)).toBe(flags);
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
    // Germany's whole first month, on the computer: its battles have joiners, booked in the losses. (Poland's corps next
    // door stand one or two to a province facing the Germans, or are kept for Warsaw: none comes up in them yet.)
    const from = g.reports.length;
    stepAI(g);
    const reps = g.reports.slice(from).filter((r) => !r.yielded);
    expect(reps.some((r) => r.attackers.some((u) => u.joined !== undefined))).toBe(true);
    for (const r of reps) expect(r.attLost).toBe(r.attackers.reduce((s, u) => s + u.lost, 0));
    // A corps that joined a battle is in no later battle of the staff's month, as attacker or joiner.
    const spent = new Set<number>();
    for (const r of reps) {
      for (const u of r.attackers) if (u.joined === undefined) expect(spent.has(g.corps.find((c) => c.name === u.name && c.nat === u.nat)?.id ?? -1)).toBe(false);
      for (const u of r.attackers) if (u.corps !== undefined) { expect(spent.has(u.corps)).toBe(false); spent.add(u.corps); }
    }
    // The war's first two months, every staff's: the defence has its joiners too, and no corps comes up in two battles of a month.
    while (g.turn <= 2) stepAI(g);
    const all = g.reports.slice(from);
    expect(all.some((r) => r.defenders.some((u) => u.joined !== undefined))).toBe(true);
    for (const t of [1, 2]) {
      const once = new Set<number>();
      for (const r of all.filter((x) => x.turn === t)) for (const u of [...r.attackers, ...r.defenders]) if (u.joined !== undefined) {
        expect(once.has(u.corps!)).toBe(false);
        once.add(u.corps!);
      }
    }
  });
});

describe("one battle a month, in the order the battles are fought (the sequence rule: the design director's rulings of 17:43Z, Q1 and Q2)", () => {
  /** Every card Germany could fight this round, as its staff weighs them: the province, a German corps next door to attack it, and the card. */
  function cards(g: Game): { p: Region; by: Corps; a: Assessment }[] {
    const out: { p: Region; by: Corps; a: Assessment }[] = [];
    for (const p of g.regions) {
      if (!hostileUnits(g, p, 'GER').length) continue;
      const near = nextDoor(g, p).filter((u) => u.nat === 'GER' && !u.dead && !u.hold && u.kind !== 'fort' && u.town === undefined && u.ap > 0 && !u.fought);
      if (near.length) out.push({ p, by: near[0], a: assess(g, [near[0]], p) });
    }
    return out;
  }
  const on = (x: { a: Assessment }): number[] => [...ids(x.a, 'att'), ...ids(x.a, 'def')];

  it('a staff weighs a reserve on every card it could reach; once a battle is fought the cards are weighed again without those that came up', () => {
    // A war nobody plays, Germany's first month: every card her staff could fight this round.
    const g = startOpen(createGame(3, null));
    const round = cards(g);
    expect(round.length).toBeGreaterThan(5);
    const where = new Map<number, number[]>();
    for (const x of round) for (const id of on(x)) where.set(id, [...(where.get(id) ?? []), x.p.id]);
    // Within one round a reserve stands on every card it can reach: Army Prusy at Kielce on five, XV Corps on two.
    const prusy = corps(g, 'POL', 'Army Prusy'), xv = corps(g, 'GER', 'XV Corps');
    expect(where.get(prusy.id)!.length).toBeGreaterThanOrEqual(3);
    expect(where.get(xv.id)!.map((p) => g.regions[p].name).sort()).toEqual(['Lodz', 'Polish Silesia']);
    // The staff fights one of them, Lodz: those that came up there are spent, and every card of the next round reads without them.
    const lodzCard = round.find((x) => x.p.name === 'Lodz')!;
    const rep = resolveBattle(g, [lodzCard.by], lodzCard.p);
    const up = [...rep.attackers, ...rep.defenders].filter((u) => u.joined !== undefined).map((u) => u.corps!);
    expect(up).toContain(prusy.id);
    expect(up).toContain(xv.id);
    const next = cards(g);
    for (const x of next) for (const id of up) expect(on(x)).not.toContain(id);
    // Those on the Lodz card that did not come up are still read where they could come up.
    const still = [...where].filter(([id, ps]) => ps.length > 1 && !up.includes(id));
    for (const [id] of still) expect(next.some((x) => on(x).includes(id))).toBe(true);
  });

  it('a reserve next to two of the mover\'s attacks comes up at the first fought and is gone from the second, whichever is fought first', () => {
    // A war nobody plays: XV Corps (Upper Silesia) and Army Prusy (Kielce) stand next to Lodz and to Polish Silesia,
    // the one to join Germany's attack, the other the Polish defence. X Corps attacks Lodz, VIII Corps Polish Silesia.
    for (const first of ['Lodz', 'Polish Silesia']) {
      const g = startOpen(createGame(3, null));
      const lodzR = g.byName['Lodz'], silesia = g.byName['Polish Silesia'];
      const x = corps(g, 'GER', 'X Corps'), viii = corps(g, 'GER', 'VIII Corps'), xv = corps(g, 'GER', 'XV Corps'), prusy = corps(g, 'POL', 'Army Prusy');
      // Before either is fought, both cards read both.
      for (const [by, r] of [[x, lodzR], [viii, silesia]] as const) {
        const a = assess(g, [by], r);
        expect(ids(a, 'att')).toContain(xv.id);
        expect(ids(a, 'def')).toContain(prusy.id);
      }
      const [one, two] = first === 'Lodz' ? [[x, lodzR], [viii, silesia]] as const : [[viii, silesia], [x, lodzR]] as const;
      const a1 = assess(g, [one[0]], one[1]);
      const rep = resolveBattle(g, [one[0]], one[1]);
      // It comes up at the first battle fought, as its card said, and its month is spent.
      const booked = [...rep.attackers, ...rep.defenders].filter((u) => u.joined !== undefined).map((u) => u.corps);
      expect(booked).toEqual([...ids(a1, 'att'), ...ids(a1, 'def')]);
      for (const c of [xv, prusy]) { expect(booked).toContain(c.id); expect(c.joinedIn).toBe(g.turn); }
      // The second card reads without it.
      const a2 = assess(g, [two[0]], two[1]);
      expect([...ids(a2, 'att'), ...ids(a2, 'def')]).not.toContain(xv.id);
      expect([...ids(a2, 'att'), ...ids(a2, 'def')]).not.toContain(prusy.id);
      expect(mayJoin(g, xv, two[1])).toBe(false);
    }
  });

  it('a reserve cut by the five a side, or kept where it is by the front rule, did not come up: it is still free for the next card', () => {
    // Germany's seat (his corps are under no front rule; Poland's staff keeps to its own).
    const g = seat('GER');
    const lodzR = g.byName['Lodz'], silesia = g.byName['Polish Silesia'], warsaw = g.byName['Warsaw'], mlawa = g.byName['Mlawa'];
    const x = corps(g, 'GER', 'X Corps'), viii = corps(g, 'GER', 'VIII Corps'), wodrig = corps(g, 'GER', 'Corps Wodrig');
    const xi = corps(g, 'GER', 'XI Corps'), modlin = corps(g, 'POL', 'Army Modlin');
    // At Lodz, XI Corps passes every test but comes up sixth, and Army Modlin is kept at Mlawa by Poland's front rule.
    const over = passedOver(g, [x], lodzR, hostileUnits(g, lodzR, 'GER'));
    expect(over.find((o) => o.corps === xi)).toEqual({ corps: xi, side: 'att', why: 'stack' });
    expect(over.find((o) => o.corps === modlin)).toEqual({ corps: modlin, side: 'def', why: 'front' });
    const rep = orderAttack(g, [x], lodzR)!;
    const booked = [...rep.attackers, ...rep.defenders].filter((u) => u.joined !== undefined).map((u) => u.corps);
    expect(booked).toHaveLength(STACK + 1);
    // Neither came up, so neither is spent.
    for (const c of [xi, modlin]) {
      expect(booked).not.toContain(c.id);
      expect(c.joinedIn).toBeUndefined();
    }
    // XI Corps comes up at the next battle it can reach, Polish Silesia.
    expect(ids(assess(g, [viii], silesia), 'att')).toContain(xi.id);
    // Army Modlin comes up at Warsaw once its province's two are met (two more Polish corps at Mlawa that have fought).
    expect(mayJoin(g, modlin, warsaw)).toBe(true);
    expect(ids(assess(g, [wodrig], warsaw), 'def')).not.toContain(modlin.id);
    for (const k of [1, 2]) { const c = addCorps(g, 'POL', `Test Screen ${k} (Mlawa)`, 'inf', 1, mlawa.id, {}); c.fought = true; c.foughtIn = g.turn; }
    expect(ids(assess(g, [wodrig], warsaw), 'def')).toContain(modlin.id);
  });

  it("the player's card shows every reserve not yet used: his reserves come up at the first of his battles they can reach", () => {
    // Germany's seat. XV Corps stands next to Lodz and to Polish Silesia, and both his cards show it.
    const g = seat('GER');
    const lodzR = g.byName['Lodz'], silesia = g.byName['Polish Silesia'];
    const x = corps(g, 'GER', 'X Corps'), viii = corps(g, 'GER', 'VIII Corps'), xv = corps(g, 'GER', 'XV Corps');
    const mine = (o: ReturnType<typeof attackOutlook>): string[] => (o?.join?.att ?? []).map((j) => j.name);
    expect(mine(attackOutlook(g, [x], lodzR))).toContain('XV Corps');
    expect(mine(attackOutlook(g, [viii], silesia))).toContain('XV Corps');
    // He attacks Polish Silesia first (no province's number keeps it for Lodz): it comes up there.
    const rep = orderAttack(g, [viii], silesia)!;
    expect(rep.attackers.find((u) => u.joined !== undefined && u.corps === xv.id)).toBeDefined();
    expect(xv.joinedIn).toBe(g.turn);
    // His Lodz card now reads without it, and shows the reserves not yet used.
    const after = attackOutlook(g, [x], lodzR)!;
    expect(mine(after)).not.toContain('XV Corps');
    for (const j of after.join?.att ?? []) expect(corps(g, 'GER', j.name).joinedIn).toBeUndefined();
    expect(mine(after).length).toBeGreaterThan(0);
  });

  it("the enemy's reserves against his attacks go the same way: on every card of his they can reach, until they come up", () => {
    const g = seat('GER');
    const lodzR = g.byName['Lodz'], silesia = g.byName['Polish Silesia'];
    const x = corps(g, 'GER', 'X Corps'), viii = corps(g, 'GER', 'VIII Corps'), prusy = corps(g, 'POL', 'Army Prusy');
    // (His staff has identified the defence and the corps next door, so his cards name them.)
    identify(g, [...hostileUnits(g, lodzR, 'GER'), ...hostileUnits(g, silesia, 'GER'), ...nextDoor(g, lodzR), ...nextDoor(g, silesia)].filter((c) => c.nat === 'POL'), 'GER');
    const theirs = (o: ReturnType<typeof attackOutlook>): string[] => (o?.join?.def ?? []).map((j) => j.name);
    expect(theirs(attackOutlook(g, [x], lodzR))).toContain('Army Prusy');
    expect(theirs(attackOutlook(g, [viii], silesia))).toContain('Army Prusy');
    const rep = orderAttack(g, [viii], silesia)!;
    expect(rep.defenders.find((u) => u.joined !== undefined && u.corps === prusy.id)).toBeDefined();
    expect(prusy.joinedIn).toBe(g.turn);
    expect(ids(assess(g, [x], lodzR), 'def')).not.toContain(prusy.id);
    expect(theirs(attackOutlook(g, [x], lodzR))).not.toContain('Army Prusy');
  });

  it('the fight follows the card: the battle books exactly the joiners its card named', () => {
    const { g, lodz: target, iv } = lodz({ screen: true });
    const a = assess(g, [iv], target);
    const rep = orderAttack(g, [iv], target)!;
    const booked = (us: BattleReport['attackers']): (number | undefined)[] => us.filter((u) => u.joined !== undefined).map((u) => u.corps);
    expect(booked(rep.attackers)).toEqual(ids(a, 'att'));
    expect(booked(rep.defenders)).toEqual(ids(a, 'def'));
    expect(ids(a, 'att').length + ids(a, 'def').length).toBe(2 * STACK);
  });

  it('a defender that gave ground is no joiner of the battle it left, at its second reckoning or after', () => {
    // Germany's seat at Lodz: Army Lodz has the order to give ground, and a second Polish corps stands with it.
    const { g, lodz: target } = lodz({ screen: true });
    const army = corps(g, 'POL', 'Army Lodz'), warsaw = g.byName['Warsaw'];
    addCorps(g, 'POL', 'Test Stayer', 'inf', 1, target.id, {});
    const att = ['IV Corps', 'XVI Corps', 'XV Corps', 'XIV Corps', 'X Corps'].map((n) => corps(g, 'GER', n));
    // As the battle stands at its second reckoning: Army Lodz has given ground to Warsaw next door (giveWay), the other stays.
    relocate(g, army, warsaw);
    army.ap = 0; army.gaveWay = true; foughtNow(g, army);
    expect(battleThisMonth(g, army)).toBe(true);
    expect(mayJoin(g, army, target)).toBe(false);
    expect(ids(assess(g, att, target), 'def')).not.toContain(army.id);
    // Without the mark it would come up from Warsaw: the giving way alone keeps it out.
    delete army.gaveWay; delete army.foughtIn;
    expect(mayJoin(g, army, target)).toBe(true);
    expect(ids(assess(g, att, target), 'def')).toContain(army.id);
    // And in the battle itself.
    const s = lodz({ screen: true }), it2 = corps(s.g, 'POL', 'Army Lodz');
    it2.yields = true;
    addCorps(s.g, 'POL', 'Test Stayer', 'inf', 1, s.lodz.id, {});
    const rep = resolveBattle(s.g, ['IV Corps', 'XVI Corps', 'XV Corps', 'XIV Corps', 'X Corps'].map((n) => corps(s.g, 'GER', n)), s.lodz);
    expect(rep.defenders.find((u) => u.name === 'Army Lodz' && u.joined === undefined)!.fate).toBe('yielded');
    expect(rep.defenders.some((u) => u.joined !== undefined && u.corps === it2.id)).toBe(false);
    expect(mayJoin(s.g, it2, s.lodz)).toBe(false);
  });

  it('a landing reads the reserves next to its shore as any battle does: free until they come up', () => {
    // Germany's seat: the Konigsberg garrison lands in the Danzig Corridor, with Army Pomorze, XIX Corps and XXI Corps next door.
    const g = seat('GER');
    const from = corps(g, 'GER', 'Konigsberg garrison'), beach = g.byName['Danzig Corridor'], pomorzeR = g.byName['Pomorze'];
    screenFor(g, beach, 'POL');
    const pomorze = corps(g, 'POL', 'Army Pomorze'), xix = corps(g, 'GER', 'XIX Corps'), xxi = corps(g, 'GER', 'XXI Corps'), ii = corps(g, 'GER', 'II Corps');
    // Early in Germany's month, with every battle on land still to fight, the landing card reads them all: nothing keeps
    // them for a battle on land.
    let a = previewLanding(g, [from], beach.id).assess!;
    expect(ids(a, 'att')).toEqual(expect.arrayContaining([xix.id, xxi.id]));
    expect(ids(a, 'def')).toContain(pomorze.id);
    // XIX and XXI Corps come up first at Pomorze, which II Corps attacks: the landing card then reads without them, and
    // without Army Pomorze, which fought there.
    expect(ids(assess(g, [ii], pomorzeR), 'att')).toEqual(expect.arrayContaining([xix.id, xxi.id]));
    orderAttack(g, [ii], pomorzeR);
    for (const c of [xix, xxi]) expect(c.joinedIn).toBe(g.turn);
    a = previewLanding(g, [from], beach.id).assess!;
    expect([...ids(a, 'att'), ...ids(a, 'def')]).not.toEqual(expect.arrayContaining([xix.id]));
    expect(ids(a, 'att')).not.toContain(xxi.id);
    expect(ids(a, 'def')).not.toContain(pomorze.id);
  });

  it('a corps that has fought this month and still has weeks in hand may attack again, and the battle it fights reads its neighbours', () => {
    const { g, lodz: target, iv } = lodz({ screen: true });
    const fresh = ids(assess(g, [iv], target), 'def');
    expect(fresh.length).toBeGreaterThan(0);
    // Every German corps has had a battle this month (as one attacked in another phase, or one that won and has weeks
    // left: moveIn and a repulse leave weeks); IV Corps has two weeks left. The game lets it attack again.
    for (const c of g.corps) if (c.nat === 'GER' && !c.dead) { c.fought = true; c.foughtIn = g.turn; }
    iv.ap = 2;
    expect(candidates(g, target)).toContain(iv);
    // The Poles next door that would come up come up as before: the same defence joiners.
    expect(ids(assess(g, [iv], target), 'def')).toEqual(fresh);
  });

  it("the battle's second reckoning, after a defender gives ground, reads the joiners its card named and no others", () => {
    const g = seat('GER');
    const target = g.byName['Lodz'], warsaw = g.byName['Warsaw'];
    // Warsaw's field army marches off, out of the scene; Army Modlin at Mlawa is next to Warsaw and to Lodz.
    const far = g.regions.find((r) => r.ctrl === 'POL' && !target.nb.includes(r.id) && r.id !== target.id && !r.nb.includes(warsaw.id) && !r.nb.some((j) => g.regions[j].units.some((u) => u.nat === 'GER')))!;
    for (const u of [...warsaw.units]) if (u.nat === 'POL') relocate(g, u, far);
    screenFor(g, target, 'POL');
    const army = corps(g, 'POL', 'Army Lodz'), modlin = corps(g, 'POL', 'Army Modlin');
    for (const r of [warsaw, target]) expect(r.nb).toContain(modlin.reg);
    addCorps(g, 'POL', 'Test Stayer', 'inf', 1, target.id, {});
    for (const j of target.nb) for (const u of g.regions[j].units) if (u.nat === 'POL' && u !== modlin) { u.fought = true; u.foughtIn = g.turn; }
    army.yields = true;
    const att = ['IV Corps', 'XVI Corps', 'XV Corps', 'XIV Corps', 'X Corps', 'XIII Corps'].map((n) => corps(g, 'GER', n)).filter((c) => target.nb.includes(c.reg));
    expect(mayJoin(g, modlin, target)).toBe(true);
    const card = ids(assess(g, att, target), 'def');
    expect(card).toEqual([modlin.id]);
    const rep = orderAttack(g, att, target)!;
    expect(rep.defenders.find((u) => u.name === 'Army Lodz' && u.joined === undefined)!.fate).toBe('yielded');
    // Army Lodz now stands at Warsaw, next door; the battle still books the card's joiner, and no other.
    expect(army.reg).toBe(warsaw.id);
    expect(rep.defenders.filter((u) => u.joined !== undefined).map((u) => u.corps)).toEqual(card);
    expect(modlin.joinedIn).toBe(g.turn);
  });

  it('a corps that gave ground is barred for the calendar month, like any battle, and free the next', () => {
    const { g, lodz: target } = lodz({ screen: true });
    const krakow = corps(g, 'POL', 'Army Krakow (Silesia)');
    expect(mayJoin(g, krakow, target)).toBe(true);
    // It gave ground last month, to an attack in a phase after Poland's (Romania's, say): the mark stays till its own month.
    krakow.gaveWay = true; krakow.foughtIn = g.turn - 1;
    expect(battleThisMonth(g, krakow)).toBe(false);
    expect(mayJoin(g, krakow, target)).toBe(true);
    // Had it given ground this month, it was in that battle: it joins none until the next.
    krakow.foughtIn = g.turn;
    expect(mayJoin(g, krakow, target)).toBe(false);
  });

  it("a corps whose month is still to come reads this month's weeks, as its nation's month will give them, not last month's", () => {
    const { g, lodz: target } = lodz({ screen: true });
    const krakow = corps(g, 'POL', 'Army Krakow (Silesia)');
    expect(g.order.indexOf('POL')).toBeGreaterThan(g.phase);
    // Last month it was short of fuel, or out of an offensive's first months: three weeks.
    krakow.apMax = 3; krakow.ap = 0;
    const m = monthWeeks(g, krakow);
    expect(m).toBe(4);
    expect(weeksInHand(g, krakow)).toBe(m);
    expect(weeksOfMonth(g, krakow)).toBe(m);
    expect(marchWeeks(g, krakow, target)).toBe(weekOf(krakow, moveCost(g, krakow, target), m, m));
    expect(marchWeeks(g, krakow, target)).toBe(1);
    // Once Poland's month has begun, its weeks are what that month gave it.
    const was = g.phase;
    g.phase = g.order.indexOf('POL');
    expect(weeksInHand(g, krakow)).toBe(0);
    expect(weeksOfMonth(g, krakow)).toBe(3);
    g.phase = was;
    // And the weeks startPhase gives every corps are monthWeeks' own.
    const s = startOpen(createGame(5, null));
    startPhase(s, 'GER');
    for (const c of s.corps) if (c.nat === 'GER' && !c.dead) expect(c.apMax).toBe(monthWeeks(s, c));
  });

  it("the front rule reads the staff's own masks: a theatre plan's masked garrison is nobody to screen", () => {
    // History's war, month 60, the Americans ashore in Normandy and the Cotentin: their plan for the West is the
    // breakout, which masks Bordeaux (theatres.json), the garrison left to others.
    const g = createGame(3, null);
    g.turn = 60;
    for (const n of ['Normandy', 'Cotentin']) g.byName[n].ctrl = 'USA';
    const bordeaux = g.byName['Bordeaux'], gascony = g.byName['Gascony'];
    expect(bordeaux.nb).toContain(gascony.id);
    const us = addCorps(g, 'USA', 'Test US Corps', 'inf', 1, gascony.id, {});
    expect([...staffMask.of(g, us)]).toEqual([bordeaux.id]);
    // (America and Germany at war, as in 1944.)
    g.nations.USA.alive = true;
    for (const [a, b] of [['USA', 'GER'], ['GER', 'USA']]) g.nations[a].atWar.push(b);
    addCorps(g, 'GER', 'Test Bordeaux Garrison', 'inf', 1, bordeaux.id, {});
    expect(gascony.nb.filter((j) => hostileUnits(g, g.regions[j], 'USA').length)).toEqual([bordeaux.id]);
    // Without the mask the rule would keep it watching Bordeaux; the staff's own rule does not, and neither does the join's.
    expect(screens(g, us, 'USA', NO_MASK)).toBe(true);
    expect(screens(g, us, 'USA', staffMask.of(g, us))).toBe(false);
    // In free play no staff masks anything.
    const f = startOpen(createGame(3, null));
    f.turn = 60;
    for (const n of ['Normandy', 'Cotentin']) f.byName[n].ctrl = 'USA';
    expect(staffMask.of(f, addCorps(f, 'USA', 'Test US Corps', 'inf', 1, f.byName['Gascony'].id, {})).size).toBe(0);
  });

  it("the war's records can list the reserves a battle passes over, and why: cut by the five a side, kept by the front rule, or come up already; the watcher moves nothing", () => {
    const { g, lodz: target, iv } = lodz({ screen: true });
    const over = passedOver(g, [iv], target, hostileUnits(g, target, 'GER'));
    const a = assess(g, [iv], target);
    for (const x of over) expect([...ids(a, 'att'), ...ids(a, 'def')]).not.toContain(x.corps.id);
    // With Poland's front rule met next door, those left out are each side's sixth and after: cut by the five a side.
    expect(over.length).toBeGreaterThan(0);
    expect(over.every((x) => x.why === 'stack')).toBe(true);
    expect(over.filter((x) => x.side === 'def').map((x) => x.corps.name).sort()).toEqual(nextDoor(g, target).filter((c) => c.nat === 'POL' && real(c) && !ids(a, 'def').includes(c.id)).map((c) => c.name).sort());
    // Without it, the front rule keeps Poles where they stand.
    const p = lodz();
    expect(passedOver(p.g, [p.iv], p.lodz, hostileUnits(p.g, p.lodz, 'GER')).filter((x) => x.why === 'front' && x.side === 'def').length).toBeGreaterThan(0);
    expect(joinWatch.tell).toBeNull();
    const told: number[] = [];
    joinWatch.tell = (e) => { told.push(e.target.id); };
    try { orderAttack(g, [iv], target); } finally { joinWatch.tell = null; }
    expect(told).toEqual([target.id]);
    // Those that came up at Lodz and stand next to Polish Silesia too are read there as come up already, and are on no card.
    const silesia = g.byName['Polish Silesia'], viii = corps(g, 'GER', 'VIII Corps');
    const then = passedOver(g, [viii], silesia, hostileUnits(g, silesia, 'GER')).filter((x) => x.why === 'joined');
    expect(then.length).toBeGreaterThan(0);
    const card = assess(g, [viii], silesia);
    for (const x of then) {
      expect(x.corps.joinedIn).toBe(g.turn);
      expect(ids(card, x.side)).not.toContain(x.corps.id);
    }
    JOIN.on = false;
    expect(passedOver(g, [viii], silesia, hostileUnits(g, silesia, 'GER'))).toEqual([]);
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
    orderHold(g, xvi, true);
    const rep0 = orderAttack(g, [iv], target)!;
    const text = saveGame(g);
    const back = loadGame(text)!;
    expect(saveGame(back)).toBe(text);
    const xviBack = back.corps.find((c) => c.id === xvi.id)!;
    expect(xviBack.hold).toBe(true);
    expect(heldFromStart(back, xviBack)).toBe(false);
    expect(xviBack.joinedIn).toBeUndefined();
    expect(back.corps.filter((c) => c.joinedIn === g.turn).map((c) => c.id).sort()).toEqual([...rep0.attackers, ...rep0.defenders].filter((u) => u.joined !== undefined).map((u) => u.corps!).sort());
    const rep = back.reports[back.reports.length - 1];
    expect(rep).toEqual(rep0);
    expect(rep.attackers.some((u) => u.joined !== undefined && u.arrive !== undefined && u.from !== undefined && u.corps !== undefined)).toBe(true);
  });

  it("an old save's hold-back loads as that corps' hold, the one mark; with the switch off it is carried and never read", () => {
    // A save from the build before the fold: XVI Corps held back by the player's old order, its hold not set.
    const { g } = lodz();
    const xvi = corps(g, 'GER', 'XVI Corps');
    xvi.holdBack = true;
    const text = saveGame(g);
    expect(text).toContain('"holdBack":true');
    const back = loadGame(text)!;
    const x = back.corps.find((c) => c.id === xvi.id)!;
    expect(x.hold).toBe(true);
    expect(heldFromStart(back, x)).toBe(false);
    expect('holdBack' in x).toBe(false);
    expect(back.corps.some((c) => c.holdBack !== undefined)).toBe(false);
    expect(mayJoin(back, x, back.byName['Lodz'])).toBe(false);
    // Lifted with the toggle, like any hold.
    expect(orderHold(back, x, false)).toBe(true);
    expect(mayJoin(back, x, back.byName['Lodz'])).toBe(true);
    // Off: the save loads as it was written.
    JOIN.on = false;
    const off = loadGame(text)!;
    const y = off.corps.find((c) => c.id === xvi.id)!;
    expect([y.hold, y.holdBack]).toEqual([false, true]);
    expect(saveGame(off)).toBe(text);
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
