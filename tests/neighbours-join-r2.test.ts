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
import { addCorps, assess, atWar, battleCost, createGame, declareWar, endPhase, ensureSupply, hostileUnits, limitAttackers, men, monthWeeks, moveCost, relocate, resolveBattle, screens, startPhase, weekOf, type Assessment } from '../src/campaign/game';
import { JOIN, JOINING, JOIN_ATTACK, JOIN_DEFENCE, MAY_JOIN, battleThisMonth, foughtNow, joinShare, joinWatch, joinWord, joinersOf, keptAshore, keptFor, marchWeeks, mayJoin, passedOver, weeksInHand, weeksOfMonth } from '../src/campaign/join';
import { identify } from '../src/campaign/intel';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { attackOutlook, beginHuman, candidates, humanUp, orderAttack, orderHoldBack, previewLanding, runTurnAI, stepAI } from '../src/campaign/turn';
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
    // Seven Polish corps stand next door: the five the fight sends here come up; Army Modlin and Army Prusy are kept
    // for Warsaw, which comes before Lodz in the map's order (keptFor: the design director's ruling 4).
    const poles = nextDoor(g, target).filter((c) => c.nat === 'POL' && real(c));
    expect(poles.length).toBe(7);
    const away = poles.filter((c) => !ids(a, 'def').includes(c.id));
    expect(away.map((c) => c.name).sort()).toEqual(['Army Modlin', 'Army Prusy']);
    for (const c of away) expect(keptFor(g, c, 'GER')).toBe(g.byName['Warsaw'].id);
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
    // The fight sends the Slovaks to Krakow, the first province next to them where Germany could fight a battle they would join.
    expect(keptFor(g, slovak, 'GER')).toBe(krakow.id);
    const a = assess(g, [xviii], krakow);
    expect(ids(a, 'att')).toContain(slovak.id);
    expect(ids(a, 'att')).not.toContain(italian.id);
    expect((a.join?.def ?? []).every((j) => j.nat === 'POL')).toBe(true);
    // At Przemysl, with the Hungarians next door (Germany's friends, not at war with Poland): neither they nor the Italians come up.
    const b = assess(g, [xviii], przemysl);
    expect([...ids(b, 'att'), ...ids(b, 'def')]).not.toContain(magyar.id);
    expect(ids(b, 'att')).not.toContain(italian.id);
    expect(keptFor(g, magyar, 'GER')).toBeNull();
    expect(keptFor(g, italian, 'GER')).toBeNull();
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

describe("one battle a month, each reserve kept in the map's order (the design director's ruling 4, as read here)", () => {
  it('no corps next door stands on two cards: each is read only where the fight would send it', () => {
    // A war nobody plays, Germany's first month: the card of every province with an enemy of hers in it that a German corps next door could attack.
    const g = startOpen(createGame(3, null));
    const on = new Map<number, number>();
    let cards = 0;
    for (const p of g.regions) {
      if (!hostileUnits(g, p, 'GER').length) continue;
      const near = p.nb.flatMap((j) => g.regions[j].units).filter((u) => u.nat === 'GER' && !u.dead && !u.hold && u.kind !== 'fort' && u.town === undefined && u.ap > 0 && !u.fought);
      if (!near.length) continue;
      cards++;
      const a = assess(g, [near[0]], p);
      for (const j of [...(a.join?.att ?? []), ...(a.join?.def ?? [])]) {
        expect(on.has(j.id)).toBe(false);
        on.set(j.id, p.id);
        expect(keptFor(g, j.corps, 'GER')).toBe(p.id);
      }
    }
    expect(cards).toBeGreaterThan(5);
    expect(on.size).toBeGreaterThan(5);
  });

  it("the map's order: Army Modlin comes up at Warsaw and not at Lodz; XVI Corps at Lodz, and at Polish Silesia once Lodz is cleared", () => {
    const { g, lodz: target, iv } = lodz({ screen: true });
    const warsaw = g.byName['Warsaw'], silesia = g.byName['Polish Silesia'];
    const modlin = corps(g, 'POL', 'Army Modlin'), xvi = corps(g, 'GER', 'XVI Corps'), viii = corps(g, 'GER', 'VIII Corps'), wodrig = corps(g, 'GER', 'Corps Wodrig');
    // Mlawa is next to Warsaw and to Lodz, and Warsaw comes first.
    for (const r of [warsaw, target]) expect(r.nb).toContain(modlin.reg);
    expect(warsaw.id).toBeLessThan(target.id);
    expect(keptFor(g, modlin, 'GER')).toBe(warsaw.id);
    const a = assess(g, [iv], target);
    for (const j of [...a.join!.att, ...a.join!.def]) expect(keptFor(g, j.corps, 'GER')).toBe(target.id);
    expect(ids(a, 'def')).not.toContain(modlin.id);
    expect(warsaw.nb).toContain(wodrig.reg);
    expect(ids(assess(g, [wodrig], warsaw), 'def')).toContain(modlin.id);
    // XVI Corps stands next to Lodz and Polish Silesia: while Lodz holds Poles it is kept for Lodz.
    for (const r of [target, silesia]) expect(r.nb).toContain(xvi.reg);
    expect(silesia.nb).toContain(viii.reg);
    expect(ids(a, 'att')).toContain(xvi.id);
    expect(ids(assess(g, [viii], silesia), 'att')).not.toContain(xvi.id);
    // Lodz cleared (its army marched out to Warsaw): the fight sends XVI Corps on to Polish Silesia.
    for (const u of [...target.units]) relocate(g, u, warsaw);
    expect(keptFor(g, xvi, 'GER')).toBe(silesia.id);
    expect(ids(assess(g, [viii], silesia), 'att')).toContain(xvi.id);
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
    expect(keptFor(g, army, 'GER')).toBe(target.id);
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

  it('a landing takes a reserve that no battle on land can reach this month; one kept for a battle on land stays for it', () => {
    // Germany's seat: the Konigsberg garrison lands in the Danzig Corridor, with Army Pomorze and XIX Corps next door.
    const g = seat('GER');
    const from = corps(g, 'GER', 'Konigsberg garrison'), beach = g.byName['Danzig Corridor'];
    screenFor(g, beach, 'POL');
    const pomorze = corps(g, 'POL', 'Army Pomorze'), xix = corps(g, 'GER', 'XIX Corps');
    for (const c of [pomorze, xix]) expect(beach.nb).toContain(c.reg);
    // Early in Germany's month Army Pomorze is kept for Lodz, and XIX Corps for Pomorze: neither comes up to the landing.
    expect(keptFor(g, pomorze, 'GER')).toBe(g.byName['Lodz'].id);
    expect(keptFor(g, xix, 'GER')).toBe(pomorze.reg);
    expect(previewLanding(g, [from], beach.id).assess!.join).toBeUndefined();
    // Late in it, every German corps on land but XIX Corps has had its battle and spent its weeks (a corps that has
    // fought with weeks left could still attack). No battle on land can reach XIX Corps, so it comes up to the landing;
    // Army Pomorze is kept for the Corridor itself, which XIX Corps could still attack.
    for (const c of g.corps) if (c.nat === 'GER' && c !== from && c !== xix) { c.fought = true; c.ap = 0; }
    expect(keptFor(g, xix, 'GER')).toBeNull();
    expect(keptFor(g, pomorze, 'GER')).toBe(beach.id);
    let a = previewLanding(g, [from], beach.id).assess!;
    expect(ids(a, 'att')).toEqual([xix.id]);
    expect(ids(a, 'def')).toEqual([pomorze.id]);
    // XIX Corps spent too: Army Pomorze is kept for no province on land, and comes up to the landing.
    xix.fought = true; xix.ap = 0;
    expect(keptFor(g, pomorze, 'GER')).toBeNull();
    a = previewLanding(g, [from], beach.id).assess!;
    expect(ids(a, 'def')).toEqual([pomorze.id]);
  });
});

describe('who could attack, a landing, and the second reckoning (the review of rulings 1 to 4)', () => {
  it('a corps that has fought this month and still has weeks in hand could attack again, so the battle it fights reads its neighbours', () => {
    const { g, lodz: target, iv } = lodz({ screen: true });
    const fresh = ids(assess(g, [iv], target), 'def');
    expect(fresh.length).toBeGreaterThan(0);
    // Every German corps has had a battle this month (as one attacked in another phase, or one that won and has weeks
    // left: moveIn and a repulse leave weeks); IV Corps has two weeks left. The game lets it attack again.
    for (const c of g.corps) if (c.nat === 'GER' && !c.dead) { c.fought = true; c.foughtIn = g.turn; }
    iv.ap = 2;
    expect(candidates(g, target)).toContain(iv);
    // The Poles next door that would come up are kept for Lodz as before: the same defence joiners.
    expect(ids(assess(g, [iv], target), 'def')).toEqual(fresh);
    // A corps with no weeks left could not attack: the walk passes it by.
    for (const c of g.corps) if (c.nat === 'GER' && !c.dead) c.ap = 0;
    const modlin = corps(g, 'POL', 'Army Modlin');
    expect(keptFor(g, modlin, 'GER')).toBeNull();
    // The battle being reckoned always counts its own attackers at its own province.
    expect(keptFor(g, modlin, 'GER', { target: target.id, attackers: [iv] })).toBe(target.id);
  });

  it("a staff's held corps could not attack, so no reserve is kept for a province only they face; the player's held corps could", () => {
    const g = startOpen(createGame(3, null));
    const warsaw = g.byName['Warsaw'], lodzR = g.byName['Lodz'];
    screenFor(g, lodzR, 'POL');
    const modlin = corps(g, 'POL', 'Army Modlin');
    expect(keptFor(g, modlin, 'GER')).toBe(warsaw.id);
    const near = warsaw.nb.flatMap((j) => g.regions[j].units).filter((u) => u.nat === 'GER');
    expect(near.length).toBeGreaterThan(0);
    for (const u of near) u.hold = true;
    // Nobody the staff would attack with stands next to Warsaw: Army Modlin is kept for Lodz, and comes up there.
    expect(keptFor(g, modlin, 'GER')).toBe(lodzR.id);
    const iv = corps(g, 'GER', 'IV Corps');
    expect(iv.hold).toBeFalsy();
    expect(ids(assess(g, [iv], lodzR), 'def')).toContain(modlin.id);
    // In Germany's seat a hold binds no corps of the player's: his held corps could still attack Warsaw.
    const h = lodz({ screen: true });
    const m2 = corps(h.g, 'POL', 'Army Modlin');
    for (const u of h.g.byName['Warsaw'].nb.flatMap((j) => h.g.regions[j].units)) if (u.nat === 'GER') u.hold = true;
    expect(keptFor(h.g, m2, 'GER')).toBe(h.g.byName['Warsaw'].id);
  });

  it('a reserve next to two shores is kept for the first, by number: it stands on one landing card', () => {
    const g = seat('GER');
    const from = corps(g, 'GER', 'Konigsberg garrison'), beach = g.byName['Danzig Corridor'], elbing = g.byName['Elbing'];
    screenFor(g, beach, 'POL');
    const pomorze = corps(g, 'POL', 'Army Pomorze');
    for (const c of g.corps) if (c.nat === 'GER' && c !== from) { c.fought = true; c.ap = 0; }
    expect(keptFor(g, pomorze, 'GER')).toBeNull();
    expect(keptAshore(g, pomorze, 'GER', ['GER'], beach.id)).toBe(beach.id);
    expect(ids(assess(g, [from], beach, { sea: 1 }), 'def')).toContain(pomorze.id);
    // Poles on a second shore next to Army Pomorze, Elbing, which comes first by number: it is kept for Elbing alone.
    expect(elbing.seas.length).toBeGreaterThan(0);
    expect(g.regions[pomorze.reg].nb).toContain(elbing.id);
    expect(elbing.id).toBeLessThan(beach.id);
    for (const u of [...elbing.units]) if (u.nat === 'GER') relocate(g, u, g.regions[from.reg]);
    addCorps(g, 'POL', 'Test Elbing', 'inf', 1, elbing.id, {});
    expect(hostileUnits(g, elbing, 'GER').map((u) => u.name)).toContain('Test Elbing');
    expect(keptAshore(g, pomorze, 'GER', ['GER'], beach.id)).toBe(elbing.id);
    expect(keptAshore(g, pomorze, 'GER', ['GER'], elbing.id)).toBe(elbing.id);
    expect(ids(assess(g, [from], beach, { sea: 1 }), 'def')).not.toContain(pomorze.id);
    expect(ids(assess(g, [from], elbing, { sea: 1 }), 'def')).toContain(pomorze.id);
    // Inland ground next to it is no shore: Lodz and Poznan, with Poles in them and lower numbers than the Corridor, are passed by.
    for (const r of ['Lodz', 'Poznan']) expect(g.byName[r].seas.length).toBe(0);
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
    expect(keptFor(g, modlin, 'GER')).toBe(target.id);
    const card = ids(assess(g, att, target), 'def');
    expect(card).toEqual([modlin.id]);
    const rep = orderAttack(g, att, target)!;
    expect(rep.defenders.find((u) => u.name === 'Army Lodz' && u.joined === undefined)!.fate).toBe('yielded');
    // Army Lodz now stands at Warsaw, which comes first by number; the battle still books the card's joiner.
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

  it("the war's records can count the reserves the keeping passes over at a battle; the watcher moves nothing", () => {
    const { g, lodz: target, iv } = lodz({ screen: true });
    const modlin = corps(g, 'POL', 'Army Modlin');
    const over = passedOver(g, [iv], target, hostileUnits(g, target, 'GER'));
    expect(over.find((x) => x.corps === modlin)).toEqual({ corps: modlin, side: 'def', kept: g.byName['Warsaw'].id });
    const a = assess(g, [iv], target);
    for (const x of over) expect([...ids(a, 'att'), ...ids(a, 'def')]).not.toContain(x.corps.id);
    expect(joinWatch.tell).toBeNull();
    const told: number[] = [];
    joinWatch.tell = (e) => { told.push(e.target.id); };
    try { orderAttack(g, [iv], target); } finally { joinWatch.tell = null; }
    expect(told).toEqual([target.id]);
    JOIN.on = false;
    expect(passedOver(g, [iv], target, hostileUnits(g, target, 'GER'))).toEqual([]);
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
