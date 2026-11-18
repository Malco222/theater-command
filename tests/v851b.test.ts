// Version 85.1, the second part: the code checker's America finds 1 and 2 (america-notes.md), and the design
// director's b434 and ruling 229(e) on the third playtester's wars and the Playtester's Version 85 check.
// 1. A campaign begun late does not leave the colonies of a country already fallen in Europe to it, frozen.
// 2. The Canal Zone's air groups, and any far island's of the wider world, go home by sea, one group a month.
// 3. "Not one step back" is judged the month the war with Germany ends, if it ends before its clock runs out.
// 4. The coast watch never warns of a landing once that landing has come.
// 5. Staff notes open by themselves only in the campaign FIRST COMMAND began, unless the player switches them on.
// 6. The Axis envoys courting a neutral of the Americas are news of home for a player's America, said once.
import { describe, expect, it } from 'vitest';
import { AIR, airBases, airFleetsOf, km, moveTargets, overOcean, whyNotMoveAir } from '../src/campaign/air';
import { DIPLOMACY, monthlyDiplomacy } from '../src/campaign/diplomacy';
import { startDomination } from '../src/campaign/domination';
import { addCorps, atWar, capitulate, createGame } from '../src/campaign/game';
import { GOALS, goalEndsIn, judgeGoals, openGoals } from '../src/campaign/goals';
import { freshNotebook, lessonsOn, notesWanted, readNotebook } from '../src/campaign/lessons';
import { NAVY, coastReport, crossingsOf, sail } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, orderAirBase, runTurnAI, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import { WORLD_FIRST } from '../src/campaign/world';

function playing(human: string, seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
/** A grand campaign begun in this month, as the page begins one: history played to it on the old chart, then the wider world laid on. */
function lateStart(from: number, seed = 3): Game {
  const g = createGame(seed, null);
  while (g.turn < from) runTurnAI(g);
  return startOpen(g);
}
const COLONIES = ['Iceland', 'Greenland', 'Curacao', 'Dutch Guiana', 'Paramaribo', 'Martinique', 'French Guiana', 'Cayenne'];

describe('a campaign begun late (code checker, America find 1)', () => {
  it('puts the colonies of Denmark, the Netherlands and France into the keeping of a sea power at war with the victor', () => {
    const g = lateStart(17);
    for (const id of ['DEN', 'NED', 'FRA']) expect(g.nations[id].alive, id).toBe(false);
    for (const name of COLONIES) {
      const r = g.byName[name];
      expect(g.nations[r.ctrl].alive, `${name} held by ${r.ctrl}`).toBe(true);
      expect(r.ctrl, name).toBe('UK');
    }
    // Nothing of the wider world is left to a country that has fallen.
    for (let i = WORLD_FIRST; i < g.regions.length; i++) expect(g.nations[g.regions[i].ctrl].alive, g.regions[i].name).toBe(true);
  }, 120_000);

  it('leaves a campaign begun on the first day as it was: every colony its own country\'s', () => {
    const g = startOpen(createGame(3, null));
    for (const name of COLONIES) expect(g.byName[name].ctrl, name).toBe(g.byName[name].owner);
  });
});

describe('the far islands of the wider world by sea (code checker, America find 2)', () => {
  it('lets the Canal Zone\'s groups go home to the mainland by sea, and the mainland\'s come to it, a group a month', () => {
    const g = playing('USA');
    const canal = g.byName['Canal Zone'];
    const grp = airFleetsOf(g, 'USA').find((f) => f.ship === undefined && f.base === canal.id)!;
    expect(grp).toBeDefined();
    const home = g.regions.filter((r) => r.owner === 'USA' && !r.overseas && r.id >= WORLD_FIRST && km(g, canal.id, r.id) > AIR.ferry);
    expect(home.length).toBeGreaterThan(0);
    for (const r of home) expect(overOcean(g, grp, r), r.name).toBe(true);
    // (Not too far any more: it goes by sea, as across an ocean.)
    for (const r of home) expect(whyNotMoveAir(g, grp, r), r.name).not.toBe('Too far to move in one month.');
    const to = moveTargets(g, grp).find((r) => home.includes(r))!;
    expect(to).toBeDefined();
    // A staff does not send its groups this way: its own list is as it was.
    expect(airBases(g, grp).some((r) => home.includes(r))).toBe(false);
    expect(orderAirBase(g, grp.id, to.id)).toBe(true);
    expect(grp.base).toBe(to.id);
    expect(g.flags['fy:air:USA']).toBe(g.turn);
    // A second group the same month waits; the mainland within a month's flying is unchanged.
    const ny = airFleetsOf(g, 'USA').find((f) => f.ship === undefined && f.base === g.byName['New York'].id)!;
    expect(overOcean(g, ny, g.byName['Los Angeles'])).toBe(false);
    expect(whyNotMoveAir(g, ny, g.byName['Los Angeles'])).toBe('Too far to move in one month.');
    expect(overOcean(g, ny, canal)).toBe(true);
    expect(whyNotMoveAir(g, ny, canal)).toBe('One air group a month crosses an ocean by sea, and one has gone this month.');
  });
});

describe('"Not one step back" when the war with Germany ends early (b434)', () => {
  it('is judged the month that war ends by its own test, and not before while the war goes on', () => {
    const g = playing('SOV');
    openGoals(g, 'SOV', 1);
    const goal = GOALS.SOV.short!;
    expect(goal.war?.with).toBe('GER');
    g.flags['war:GER:SOV'] = goal.war!.from;
    g.turn = goal.war!.from + 8;
    // At war still: the clock runs.
    g.nations.SOV.atWar.push('GER');
    expect(judgeGoals(g, 'SOV').filter((x) => x.key === 'short')).toEqual([]);
    // Germany has fallen, or made peace: the goal is decided that month, by Moscow, Leningrad and Stalingrad.
    g.nations.SOV.atWar = g.nations.SOV.atWar.filter((e) => e !== 'GER');
    expect(judgeGoals(g, 'SOV').filter((x) => x.key === 'short').map((x) => x.won)).toEqual([true]);
  });

  it('runs its clock from the last act when that is where the war with Germany comes (the review of 85.1)', () => {
    // France and Britain beaten before Germany ever turns east: the great powers left are at peace, and the Soviet
    // player opens the last act.
    const g = playing('SOV');
    openGoals(g, 'SOV', 1);
    const goal = GOALS.SOV.short!;
    capitulate(g, 'FRA', 'GER');
    capitulate(g, 'UK', 'GER');
    expect(goalEndsIn(g, 'SOV', goal)).toBe(Infinity);
    expect(startDomination(g, 'SOV')).toBe(true);
    expect(g.nations.SOV.atWar).toContain('GER');
    expect(g.flags['war:SOV:GER'] ?? g.flags['war:GER:SOV']).toBe(g.turn);
    expect(goalEndsIn(g, 'SOV', goal)).toBe(goal.until! + Math.max(0, g.turn - goal.war!.from));
  });
});

describe('the coast watch after the landing has come (ruling 229(e))', () => {
  it('tells the player his shipping has been seen, and says nothing once his troops are ashore from the sea', () => {
    // (As the coast watch's own tests set it out: Germany once Poland has fallen, every British ship in harbour, two
    // corps on the quay nearest Britain and nobody else's on a quay.)
    const g = startOpen(createGame(3, 'GER'));
    for (let k = 0; k < 8 && (k < 2 || g.nations.POL.alive); k++) runTurnAI(g);
    for (const f of g.fleets) if (atWar(g, f.nat, 'GER')) sail(g, f, f.home, 'port');
    const x = crossingsOf(g, 'GER').find((c) => c.owner === 'UK' && c.home)!;
    expect(x).toBeDefined();
    for (const r of g.regions) if (r.port && r.units.some((u) => u.nat === 'GER')) { for (const c of r.units) c.dead = true; r.units = []; }
    addCorps(g, 'GER', 'Quay I', 'inf', 3, x.from.id, {});
    addCorps(g, 'GER', 'Quay II', 'inf', 3, x.from.id, {});
    const seen = (): number => g.headlines.filter((h) => /coast watchers have seen your shipping/.test(h.text)).length;
    coastReport(g, 'GER');
    expect(seen()).toBe(1);
    // Germans on British ground, and a landing on another coast of the same sea: nobody has come ashore on Britain
    // from the sea, and the watch still speaks.
    const elsewhere = g.regions.find((r) => r.owner !== 'UK' && r.seas.some((z) => x.to.seas.includes(z)))!;
    expect(elsewhere).toBeDefined();
    for (const z of elsewhere.seas) g.flags[`landed:${z}:GER`] = g.turn;
    g.flags[`landedAt:${elsewhere.id}:GER`] = g.turn;
    const ashore = addCorps(g, 'GER', 'Beach I', 'inf', 3, x.to.id, {});
    g.turn += NAVY.watch!.every;
    coastReport(g, 'GER');
    expect(seen()).toBe(2);
    // A landing on the British coast that stands (as landCorps writes it): the next report says nothing of a landing to come.
    g.flags[`landedAt:${x.to.id}:GER`] = g.turn;
    g.turn += NAVY.watch!.every;
    coastReport(g, 'GER');
    expect(seen()).toBe(2);
    // Thrown back into the sea, the watch speaks again.
    ashore.dead = true;
    x.to.units = x.to.units.filter((u) => u !== ashore);
    x.to.ctrl = 'UK';
    g.turn += NAVY.watch!.every;
    coastReport(g, 'GER');
    expect(seen()).toBe(3);
  });
});

describe('staff notes open by themselves only in FIRST COMMAND (b434)', () => {
  it('stand aside in any other campaign until the player switches them on', () => {
    const g = playing('ITA');
    const book = freshNotebook();
    expect(notesWanted(g, book)).toBe(false);
    expect(lessonsOn(g, book)).toBe(false);
    expect(lessonsOn(g, { ...book, on: true, set: true })).toBe(true);
    g.flags.firstCommand = 1;
    expect(lessonsOn(g, book)).toBe(true);
    expect(lessonsOn(g, { ...book, on: false, set: true })).toBe(false);
    // A notebook kept before: one the player had switched off stays off; one left alone follows the new rule.
    expect(readNotebook({ on: false }).set).toBe(true);
    expect(readNotebook({ on: true }).set).toBe(false);
  });
});

describe('the Axis envoys in the Americas, for a player\'s America (b434, ruling 223(b))', () => {
  it('are news of home the month the courting begins, and news from elsewhere for anyone else', () => {
    for (const human of ['USA', 'UK']) {
      const g = playing(human);
      g.nations.GER.pc = 30;
      const h = g.headlines.length;
      monthlyDiplomacy(g);
      const told = g.headlines.slice(h).filter((x) => / envoys are courting /.test(x.text));
      expect(told.length, human).toBe(1);
      const T = DIPLOMACY.americas!.of.find((id) => g.flags[`court:GER:${id}`] === g.turn)!;
      expect(told[0].region).toBe(g.nations[T].capital);
      if (human === 'USA') expect([told[0].nat, told[0].kind]).toEqual(['USA', 'info']);
      else expect([told[0].nat, told[0].kind]).toEqual([T, 'event']);
    }
  });
});
