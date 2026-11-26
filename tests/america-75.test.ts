// America ships its army overseas (Version 75; the wider world).
//
// 1. War work by steps. Going to war no longer turns the United States' factories over in a day: the share on war
//    work climbs from where the peace table stands, for a player and for the computer alike.
// 2. A computer-run America raises an army and ships it across the Atlantic to a port of an ally in the same war,
//    with a guard kept at home, shipping as the limit and no dated formations from history on top. From the far
//    shore its corps go the way every second front goes; they are never landed on a defended shore from the ocean.
// 3. No corps is shipped over a crossing too dangerous for its staff, and a full or dangerous beachhead harbour
//    does not stop the sailings: the corps are put down where the army is built up.
// 4. A staff heeds its own forecast: it lands nobody on a beach where the sea behind could be closed, except to
//    take a whole island (the design director's ruling 30).
// 5. The player's building staff lays down troopships for the crossing he has named, and his staff says where the
//    crossing stands: preparing, prepared but held back by the card's warning, or ready. It never sails itself.
// 6. The history dial: by the book the staff's envoys court only the countries history brought in on that side.
// 7. A harbour the player takes from the sea is a second front his allies' staffs can ship into.
// 8. An American player goes to war when he chooses (the design director's rulings 31 to 33): Germany and Italy are
//    on his list from the first month; at war with an Axis power he stands with the Allies, whose ports open to
//    him; and his goals ask for an American army ashore, so that a player who gives no orders cannot win them.
import { describe, expect, it } from 'vitest';
import { landingWatch } from '../src/campaign/ai';
import { liftView } from '../src/campaign/arsenal';
import { DEPTH, type DeskId } from '../src/campaign/depth';
import { crossingWord } from '../src/campaign/desks';
import { DIPLOMACY, canCourt } from '../src/campaign/diplomacy';
import { income, ledger } from '../src/campaign/economy';
import { addCorps, allied, atWar, bloc, capture, createGame, removeCorps, setWar } from '../src/campaign/game';
import { GOALS, goalChecks, goalState, judgeGoals, openGoals } from '../src/campaign/goals';
import { HISTORY, setStop, staysOut } from '../src/campaign/history';
import { NAVY, crossingRisk, planCrossing, seaPath, whyNotShip } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { frontAshore, heedsCard, landingForce, lodgment, oceanRuns, startOpen } from '../src/campaign/staff';
import { beginHuman, crossingStaffWord, crossings, declarable, endHuman, humanUp, landTargets, orderCourt, orderCrossingPlan, orderDeclare, orderDesk, orderLand, orderShip, runTurnAI, shipTargets, stepAI } from '../src/campaign/turn';
import { STACK, type Corps, type Game, type Region } from '../src/campaign/types';
import { FLAGS } from '../src/campaign/ui/flags';
import { WORLD, acrossTheWater, armyIdle, beyondEurope, overseasOf, sailings, shipsItsOwn, standWithAllies, warFooting, warWorkLeft, warWorkMonth, wouldStandWithAllies } from '../src/campaign/world';
import { turnToDate } from '../src/core/ticket';
import grand from '../data/campaign/grand.json';
import open from '../data/campaign/open.json';
import setup from '../data/campaign/setup-1939.json';

const DEC_41 = 28, JAN_42 = 29, SEP_42 = 37, JUN_43 = 46;
/** A grand campaign on the wider world as it opens. */
const wide = (seed = 1, human: string | null = null): Game => startOpen(createGame(seed, human));
const field = (g: Game, nat: string): Corps[] => g.corps.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort' && c.kind !== 'eng');
const atHome = (g: Game, nat: string): Corps[] => field(g, nat).filter((c) => g.regions[c.reg].owner === nat && !g.regions[c.reg].overseas);
const across = (g: Game, nat: string): Corps[] => field(g, nat).filter((c) => !beyondEurope(g.regions[c.reg]));

/** One war on the wider world with nobody playing, remembered as these months open, with every landing a staff made. */
const war: Record<number, string> = {};
const landings: { turn: number; nat: string; to: string; warned: boolean; island: boolean; wave: boolean }[] = [];
const guard: number[] = [];
{
  // Seed 5 since ruling 233 rider b564: with Force H on the Western Mediterranean seed 4's Britain takes Sardinia in
  // 1942, its America builds its army up there, and in October 1942 its staff sends no corps over. (Seed 4 since
  // Version 84.2.5: with guns no longer going forward alone (ruling 211(a)) seed 3's staffs make no landing before
  // June 1943. Seed 3 since Version 76, when seed 2's made none on the chart with Latin America.)
  const g = wide(5);
  landingWatch.tell = (e) => { landings.push({ turn: g.turn, nat: e.nat, to: e.to.name, warned: e.warned, island: e.to.nb.length === 0, wave: e.wave }); };
  while (g.turn < JUN_43) {
    runTurnAI(g);
    if ([DEC_41, JAN_42, SEP_42, JUN_43].includes(g.turn)) war[g.turn] = saveGame(g);
    if (g.turn > JAN_42) guard.push(atHome(g, 'USA').length);
  }
  landingWatch.tell = null;
}
const at = (turn: number): Game => loadGame(war[turn])!;

describe('war work by steps', () => {
  it('going to war does not turn the factories over in a day', () => {
    const g = wide();
    expect(warFooting(g, 'USA')).toBe(0.1);
    // Every other country works at full stretch from the first day of its war, as it always did, but Canada since the
    // cities version (world.json climbFrom): 45 in 100 until the fall of France, then a step a month (tested below).
    for (const id of ['GER', 'UK', 'SOV']) expect(warFooting(g, id)).toBeNull();
    expect(warFooting(g, 'CAN')).toBe(0.45);
    const peace = income(g, 'USA');
    setWar(g, 'USA', 'GER');
    warWorkMonth(g);
    // The first month of war stands where the peace table stood: a tenth, and not the whole of it.
    expect(warFooting(g, 'USA')).toBeCloseTo(0.1, 9);
    expect(income(g, 'USA')).toBeLessThan(peace * 1.5);
    // Then so much more each month. At war in September 1939, full in October 1941: 25 months.
    let months = 0;
    while (warFooting(g, 'USA')! < 1 && months < 60) {
      const was = warFooting(g, 'USA')!, left = warWorkLeft(g, 'USA');
      g.turn++; months++;
      warWorkMonth(g);
      expect(warFooting(g, 'USA')!).toBeCloseTo(Math.min(1, was + WORLD.warWork.USA), 9);
      expect(warWorkLeft(g, 'USA')).toBe(left - 1);
    }
    expect(months).toBe(25);
    expect(warWorkLeft(g, 'USA')).toBe(0);
    expect(income(g, 'USA')).toBeGreaterThan(peace * 5);
  });

  it("Canada's factories wait for the fall of France, then climb to full war work by July 1942 (the cities version)", () => {
    // world.json: 45 in 100 on war work from the first day, war or no war, until June 1940 (turn 10); then 0.022 a
    // month, all of it by July 1942 (turn 35).
    const g = wide();
    expect(atWar(g, 'CAN', 'GER')).toBe(true);
    const seen: Record<number, number> = {};
    for (g.turn = 1; g.turn <= 40; g.turn++) { warWorkMonth(g); seen[g.turn] = warFooting(g, 'CAN')!; }
    for (let t = 1; t <= 10; t++) expect(seen[t], `turn ${t}`).toBeCloseTo(0.45, 9);
    expect(seen[11]).toBeCloseTo(0.472, 9);
    expect(seen[34]).toBeCloseTo(0.978, 9);
    expect(seen[35]).toBe(1);
    expect(seen[40]).toBe(1);
    // A campaign that opens later finds the share where the calendar has it, and the months left agree.
    const late = wide();
    late.turn = 23;
    warWorkMonth(late);
    expect(warFooting(late, 'CAN')).toBeCloseTo(0.45 + 0.022 * 13, 9);
    expect(warWorkLeft(late, 'CAN')).toBe(12);
  });

  it('a country at war in December 1941 is on full war work in June 1943', () => {
    const g = wide();
    g.turn = DEC_41;
    setWar(g, 'USA', 'GER');
    warWorkMonth(g);
    expect(warFooting(g, 'USA')).toBeCloseTo(0.35, 9);
    expect(warWorkLeft(g, 'USA')).toBe(JUN_43 - DEC_41);
    while (g.turn < JUN_43 - 1) { g.turn++; warWorkMonth(g); }
    expect(warFooting(g, 'USA')!).toBeLessThan(1);
    g.turn++;
    warWorkMonth(g);
    expect(warFooting(g, 'USA')).toBe(1);
    // Peace again: the factories go back to the peace table, and a new war begins from there.
    g.nations.USA.atWar = [];
    g.nations.GER.atWar = g.nations.GER.atWar.filter((e) => e !== 'USA');
    warWorkMonth(g);
    expect(warFooting(g, 'USA')).toBe(0.35);
  });

  it('is the same for a player, and is said in the ledger', () => {
    const g = wide(1, 'USA');
    expect(warFooting(g, 'USA')).toBe(0.1);
    const line = (): string => ledger(g, 'USA').changes.find((l) => l.key === 'peace')!.label;
    expect(line()).toBe('At peace: 10 in 100 of industry on war work');
    setWar(g, 'USA', 'GER');
    warWorkMonth(g);
    expect(line()).toBe('At war: 10 in 100 of industry on war work so far, all of it in 25 months');
    expect(ledger(g, 'USA').changes.find((l) => l.key === 'peace')!.amount).toBeLessThan(0);
    // On full war work there is nothing left to say: the line is gone from the ledger.
    g.flags['ww:USA'] = 1;
    expect(ledger(g, 'USA').changes.find((l) => l.key === 'peace')).toBeUndefined();
  });

  it('is unknown to history\'s scenarios and to a campaign kept to the map of Europe', () => {
    const h = createGame(1, null), e = startOpen(createGame(1, null), false);
    for (const g of [h, e]) {
      expect(warFooting(g, 'USA')).toBeNull();
      expect(warWorkLeft(g, 'USA')).toBe(0);
      expect(overseasOf(g, 'USA')).toBeNull();
      expect(shipsItsOwn(g, 'USA')).toBe(false);
      expect(oceanRuns(g, 'USA')).toEqual([]);
    }
  });
});

describe('the army sent across the ocean', () => {
  it('is the programme of a computer-run America at war, an ocean away from every enemy', () => {
    const O = WORLD.overseas.USA;
    expect(Object.keys(O.oceans)).toEqual(['atlantic']);
    const peace = wide();
    expect(overseasOf(peace, 'USA')).toBeNull();
    expect(oceanRuns(peace, 'USA')).toEqual([]);
    const g = at(JAN_42);
    expect(g.nations.USA.atWar).toContain('GER');
    expect(overseasOf(g, 'USA')).toBe(O);
    expect(shipsItsOwn(g, 'USA')).toBe(true);
    // One figure for each ocean: the share of the month's shipping that works that run.
    expect(sailings(g, 'USA', O)).toBeCloseTo(O.oceans.atlantic * g.nations.USA.sealift, 9);
    // The staff raises corps only so far ahead of the sailings, on top of the guard: shipping is the limit.
    expect(armyIdle(g, 'USA')).toBe(false);
    for (let k = 0; k < 12; k++) addCorps(g, 'USA', `Idle Corps ${k}`, 'inf', 1, 'Chicago', {});
    expect(atHome(g, 'USA').length).toBeGreaterThanOrEqual(O.guard + Math.ceil(O.ahead * sailings(g, 'USA', O)));
    expect(armyIdle(g, 'USA')).toBe(true);
    // A player's America is not run by any of it: his army is his to ship.
    const mine = at(JAN_42);
    mine.human = 'USA';
    expect(overseasOf(mine, 'USA')).toBeNull();
    expect(shipsItsOwn(mine, 'USA')).toBe(false);
    // Nor a country with an invader on its own side of the ocean: its army has work at home.
    // (A hostile country of its own side beyond the sea is not that: Version 78, entry-78.test.ts.)
    const near = at(JAN_42);
    capture(near, near.byName.Havana ?? near.regions.find((r) => r.owner === 'CUB')!, 'GER');
    expect(overseasOf(near, 'USA')).toBeNull();
    expect(shipsItsOwn(near, 'USA')).toBe(true);
  });

  it('ships to a port of an ally in the same war, from the home port with the shortest way across', () => {
    const g = at(JAN_42);
    const runs = oceanRuns(g, 'USA');
    expect(runs.length).toBe(1);
    const run = runs[0];
    expect(run.ocean).toBe('atlantic');
    expect(run.share).toBe(WORLD.overseas.USA.oceans.atlantic);
    expect(run.front).toBe(false);
    expect(run.from.owner).toBe('USA');
    expect(run.from.port).toBe(true);
    expect(run.to.length).toBeGreaterThan(0);
    for (const p of run.to) {
      expect(beyondEurope(p)).toBe(false);
      expect(p.port).toBe(true);
      expect(allied(g, 'USA', p.ctrl)).toBe(true);
      expect(atWar(g, 'USA', p.ctrl)).toBe(false);
      // Never a shore the enemy holds: the ocean run puts nobody ashore against a defence.
      expect(p.units.some((u) => atWar(g, 'USA', u.nat))).toBe(false);
    }
    // The harbour where the army is built up is one Britain holds. (In this war, seed 5's since ruling 233 rider b564,
    // it is Cornwall.)
    expect(run.to[0].ctrl).toBe('UK');
  });

  it('puts its corps across month by month, keeps a guard at home, and takes no dated formation from history', () => {
    const O = WORLD.overseas.USA;
    expect(across(at(JAN_42), 'USA').length).toBeLessThanOrEqual(1);
    const g = at(JUN_43);
    const over = across(g, 'USA');
    expect(over.length).toBeGreaterThanOrEqual(4);
    // Each of them is a corps of the second front: it stays abroad and is not sent home again.
    for (const c of over) expect(g.flags[`sf:${c.id}`]).toBe(1);
    // Shipping is the limit: never more across than the share of the lift could have carried.
    const months = JUN_43 - DEC_41;
    expect(over.length).toBeLessThanOrEqual(Math.ceil(months * O.oceans.atlantic * g.nations.USA.sealift) + O.bank);
    // The guard never left home.
    expect(Math.min(...guard)).toBeGreaterThanOrEqual(O.guard);
    // History's dated formations are not sent as well: the corps it ships take their place.
    const dated = new Set((setup.expeditions as unknown as [number, string, string][]).filter((r) => r[1] === 'USA').map((r) => r[2]));
    expect(dated.size).toBeGreaterThan(20);
    expect(g.corps.filter((c) => c.nat === 'USA' && dated.has(c.name))).toEqual([]);
    // The first crossing was news.
    expect(g.headlines.some((h) => /^The first American troops to cross the ocean come ashore at /.test(h.text))).toBe(true);
  });

  it('ships nobody over a crossing too dangerous for its staff', () => {
    const O = WORLD.overseas.USA;
    const sail = (risk: number): Region[] => {
      // December 1941: a month in this war with danger on the way to the harbour where the army is built up, and no more
      // than the staff will bear. (Since ruling 233 riders b598, b601 and b603 the Bordeaux flotilla hunts the Western
      // Approaches and not the Iberian Coast, so in October 1942 the way to Gibraltar, that month's harbour, carries no
      // danger. Seed 5's war scanned month by month from turn 26 to 46: danger at turns 28 to 33, on the way to
      // Cornwall by the Western Approaches, and at 40 to 46, on the way to Sardinia; none at 34 to 39. January 1942
      // until the ship candidate of ruling 233 (the works rule taken over, b629; the record garrisons; the boats rows):
      // scanned again on it, the danger lies at turn 28 (Cornwall) and 35 to 45 (Cornwall and the West Country) and
      // 46 (Sardinia), and from 29 to 34 the harbour is Gibraltar, with none; so December 1941. December 1942 since the
      // computer's air forces fight back (airwar.json ai.back): scanned again, from 28 to 39 and at 46 the harbour is
      // Gibraltar, with none on the way, and from 40 to 45 it is Cornwall, with danger under the staff's bar. September
      // 1942 since 1.5.1's start guns moved the war: scanned again, from 29 to 34 and 40 to 45 the harbour is Gibraltar,
      // with none on the way, at 28 and from 35 to 39 it is Cornwall, with danger under the staff's bar, and at 46 Sardinia.
      // December 1941 since release 2's joins of 25 November 2026 (1125r) moved the war: scanned again, from 28 to 34 and
      // 40 to 45 the harbour is Cornwall or the West Country, with danger under the staff's bar, from 35 to 39
      // Gibraltar, with none, and at 46 Sardinia.)
      const g = at(DEC_41);
      const before = new Set(across(g, 'USA').map((c) => c.id));
      // A month's sailing banked and a fit corps on the quay.
      const run = oceanRuns(g, 'USA')[0];
      g.flags['fy:USA:atlantic'] = 1;
      const c = addCorps(g, 'USA', 'Test Corps', 'inf', 2, run.from.id, {});
      c.supplied = true;
      const path = seaPath(g, 'USA', run.from, run.to[0])!;
      expect(crossingRisk(g, 'USA', path)).toBeGreaterThan(0);
      expect(crossingRisk(g, 'USA', path)).toBeLessThanOrEqual(O.risk);
      const was = O.risk;
      O.risk = risk;
      // The danger of the safest way to each harbour from any home port, as the month opens.
      const quays = g.regions.filter((r) => r.port && r.owner === 'USA' && r.ctrl === 'USA' && beyondEurope(r));
      const danger = new Map<number, number>();
      for (const p of g.regions) {
        if (!p.port || beyondEurope(p)) continue;
        danger.set(p.id, Math.min(1, ...quays.map((q) => { const way = seaPath(g, 'USA', q, p); return way ? crossingRisk(g, 'USA', way) : 1; })));
      }
      try { runTurnAI(g); } finally { O.risk = was; }
      const went = across(g, 'USA').filter((x) => !before.has(x.id)).map((x) => g.regions[x.reg]);
      for (const p of went) expect(danger.get(p.id)!).toBeLessThanOrEqual(risk + 1e-9);
      return went;
    };
    // By its own rule a corps crosses to Britain. A staff that would bear no danger at all sends its corps only to
    // a harbour it can reach over seas where no enemy is at work (checked in `sail`), and one for which every
    // crossing is too dangerous sends nobody.
    const usual = sail(O.risk);
    expect(usual.length).toBeGreaterThanOrEqual(1);
    expect(usual[0].owner).toBe('UK');
    sail(0);
    expect(sail(-1)).toEqual([]);
  });

  it('joins a second front its side holds, but not beyond dangerous seas, and a full harbour does not stop the sailings', () => {
    const O = WORLD.overseas.USA;
    const g = at(JAN_42);
    // (Since Version 77 a harbour an ally holds near the enemy is a front too: Egypt's, with the Italians at the
    // frontier. No harbour of France is one before the landing.)
    expect((frontAshore(g, 'USA')?.ports ?? []).filter((p) => p.owner === 'FRA')).toEqual([]);
    // A British beachhead in Brittany, with the Germans all round it.
    const beach = g.byName.Brittany;
    for (const c of [...beach.units]) removeCorps(g, c);
    capture(g, beach, 'UK');
    g.flags['st:lodged:UK'] = beach.id + 1;
    expect(lodgment(g, 'UK')?.at).toBe(beach);
    const front = frontAshore(g, 'USA')!;
    expect(front.at).toBe(beach);
    expect(front.ports).toContain(beach);
    expect(front.ground[beach.id]).toBe(1);
    // An ally that is not in America's war offers no front to join.
    const was = g.nations.UK.atWar;
    g.nations.UK.atWar = [];
    expect(frontAshore(g, 'USA')).toBeNull();
    g.nations.UK.atWar = was;
    // The Bay of Biscay under German aircraft is too dangerous: the beachhead is not on the list, and the corps go to Britain.
    const path = seaPath(g, 'USA', oceanRuns(g, 'USA')[0].from, beach)!;
    expect(crossingRisk(g, 'USA', path)).toBeGreaterThan(O.risk);
    const careful = oceanRuns(g, 'USA')[0];
    expect(careful.front).toBe(false);
    expect(careful.to).not.toContain(beach);
    expect(careful.to[0].ctrl).toBe('UK');
    // With a staff that would bear any danger the beachhead comes first, and Britain's ports stand behind it for
    // the month its harbour is full.
    const keep = O.risk;
    O.risk = 1;
    try {
      const bold = oceanRuns(g, 'USA')[0];
      expect(bold.front).toBe(true);
      expect(bold.to[0]).toBe(beach);
      expect(bold.to.length).toBeGreaterThan(1);
      expect(bold.to.slice(1).every((p: Region) => p.ctrl === 'UK' && p !== beach)).toBe(true);
    } finally { O.risk = keep; }
  });
});

describe('a staff heeds its own forecast', () => {
  it('lands nobody on a mainland beach where the sea behind could be closed, and takes a whole island all the same', () => {
    const g = wide();
    expect((open as unknown as { descent: { heed: unknown } }).descent.heed).toEqual({ mainland: true, island: false, wave: true });
    expect(landingForce()).toBe((open as unknown as { descent: { force: number } }).descent.force);
    // A shore with a land frontier: the staff heeds its card. A province with none is a whole island: it does not.
    for (const name of ['Brittany', 'Holland', 'Kent', 'Sicily', 'Bergen', 'Tripoli']) {
      expect(g.byName[name].nb.length).toBeGreaterThan(0);
      expect(heedsCard(g.byName[name])).toBe(true);
    }
    for (const name of ['Sardinia', 'Corsica', 'Malta', 'Crete', 'Rhodes', 'Cyprus']) {
      expect(g.byName[name].nb).toEqual([]);
      expect(heedsCard(g.byName[name])).toBe(false);
      // A further wave over the beach to men already ashore never goes in under a warning, island or not.
      expect(heedsCard(g.byName[name], true)).toBe(true);
    }
    // Every landing the staffs made in a war nobody played was one their own card did not warn against, or took an island.
    expect(landings.length).toBeGreaterThan(0);
    expect(landings.filter((l) => l.warned && (!l.island || l.wave))).toEqual([]);
  });
});

/** A grand campaign with the player's first month open. */
function playing(human: string, seed = 3, stop?: number): Game {
  const g = startOpen(createGame(seed, human));
  if (stop !== undefined) expect(setStop(g, stop, true)).toBe(true);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
function month(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
}
const lines = (g: Game, desk: DeskId, turn = g.turn): string[] => g.headlines.filter((h) => h.desk === desk && h.turn === turn).map((h) => h.text);
const troopships = (g: Game, nat: string): number => g.nations[nat].queue.filter((b) => b.what === 'ship' && b.item === 'tp').length;

describe('the building staff and the crossing the player names', () => {
  /** A quay and a beach the player could name, with so many of his corps on the quay. */
  function named(g: Game, corps: number): { from: Region; to: Region } {
    const me = g.human!;
    const put = (from: Region): void => {
      for (const c of field(g, me).filter((u) => u.reg === from.id)) removeCorps(g, c);
      for (let k = 0; k < corps; k++) addCorps(g, me, `Quay Corps ${k}`, 'inf', 1, from.id, {});
    };
    // A shore his army cannot march to, if he is at war with one: the first on the list of crossings.
    const row = crossings(g)[0];
    if (row && planCrossing(g, me, row.from, row.to)) { put(row.from); return { from: row.from, to: row.to }; }
    for (const from of g.regions.filter((r) => r.port && r.ctrl === me && r.owner === me)) {
      for (const to of g.regions.filter((r) => r.seas.length > 0 && atWar(g, me, r.ctrl))) {
        if (!planCrossing(g, me, from, to)) continue;
        put(from);
        return { from, to };
      }
    }
    throw new Error('no crossing to name');
  }

  it('lays down troopships until the shipping can carry the force on the quay, one lot a month', () => {
    const g = playing('GER');
    g.nations.GER.pp = 400;
    const lift = g.nations.GER.sealift;
    expect(lift).toBeLessThan(STACK);
    named(g, STACK);
    expect(troopships(g, 'GER')).toBe(0);
    orderDesk(g, 'build', true);
    expect(troopships(g, 'GER')).toBe(1);
    expect(lines(g, 'build').join(' ')).toMatch(/troopships at /);
    expect(liftView(g, 'GER').building.length).toBe(1);
    // The next month it has what the crossing needs on the slips, and orders no more.
    g.nations.GER.pp = 400;
    month(g);
    expect(troopships(g, 'GER')).toBe(STACK - lift);
  });

  it('says where the named crossing stands, and never sails under the card\'s warning: that order is the player\'s', () => {
    const g = playing('GER');
    expect(crossingStaffWord(g)).toBeNull();
    expect(crossings(g).length).toBeGreaterThan(0);
    const { from, to } = named(g, 1);
    // One corps on the quay: the staff is still preparing, and says what is wanting.
    const few = crossingStaffWord(g)!;
    expect([few.from, few.to]).toEqual([from, to]);
    expect(few.state).toBe('waiting');
    expect(few.text).toMatch(/^Your staff is preparing the crossing from /);
    expect(few.text).toMatch(/2 more corps on the quay/);
    // With a landing's force on the quay it is ready, or held back by the card's warning, as the card itself reads.
    for (let k = 0; k < landingForce(); k++) addCorps(g, 'GER', `More Corps ${k}`, 'inf', 1, from.id, {});
    const row = crossings(g).find((x) => x.planned)!;
    const word = crossingWord(g, 'GER')!;
    if (row.bar) expect(word.state).toBe('waiting');
    else if (!row.account.hold.open) {
      expect(word.state).toBe('warned');
      // A whole island is told what it is (Version 76, ruling 36): a force on its own, and for how long.
      expect(word.text).toMatch(row.account.hold.island
        ? /it waits: the force can take .*, but the enemy can close the .* behind it, and cut off it holds out for about three months on what it carries\. The order to sail under that warning is yours alone\.$/
        : /it waits: once the force is ashore the enemy can close the .* behind it\. The order to sail under that warning is yours alone\.$/);
    } else {
      expect(word.state).toBe('ready');
      expect(word.text).toMatch(/^Your staff reports the crossing from .* ready: .* The order to land is yours\.$/);
    }
    // Whatever it says, the staff gives no order to sail: after its month the corps still stand on the quay.
    const onQuay = field(g, 'GER').filter((c) => c.reg === from.id).length;
    for (const d of ['build', 'air', 'navy'] as DeskId[]) orderDesk(g, d, true);
    month(g);
    expect(field(g, 'GER').filter((c) => c.reg === from.id).length).toBe(onQuay);
    expect(g.headlines.some((h) => /land at .* from the sea/.test(h.text) && h.nat === 'GER' && h.turn >= g.turn - 1)).toBe(false);
    // Ready or held back is said once in the month's news; dropping the plan ends it.
    if (crossingStaffWord(g)!.state !== 'waiting') expect(g.headlines.filter((h) => h.text === crossingStaffWord(g)!.text).length).toBe(1);
    expect(orderCrossingPlan(g, null, null)).toBe(true);
    expect(crossingStaffWord(g)).toBeNull();
  });

  it('buys none for a force its shipping can already carry, and none with no crossing named', () => {
    const g = playing('GER');
    g.nations.GER.pp = 400;
    orderDesk(g, 'build', true);
    expect(troopships(g, 'GER')).toBe(0);
    const h = playing('GER');
    h.nations.GER.pp = 400;
    named(h, landingForce());
    expect(h.nations.GER.sealift).toBeGreaterThanOrEqual(landingForce());
    orderDesk(h, 'build', true);
    expect(troopships(h, 'GER')).toBe(0);
  });
});

describe('the staff\'s envoys keep to the history dial', () => {
  const courted = (g: Game, nat: string): string[] => Object.keys(g.flags).filter((k) => k.startsWith(`court:${nat}:`) && g.flags[k] === g.turn).map((k) => k.split(':')[2]);

  it('by the book they court only the countries history brought in on that side', () => {
    expect(HISTORY.joined.axis.length).toBeGreaterThan(0);
    expect(HISTORY.joined.allies).toEqual([]);
    for (const stop of [1, 2]) {
      const g = playing('GER', 3, stop);
      g.nations.GER.pc = 40;
      expect(staysOut(g, 'SPA', 'axis')).toBe(true);
      expect(staysOut(g, 'HUN', 'axis')).toBe(false);
      orderDesk(g, 'diplomacy', true);
      const who = courted(g, 'GER');
      expect(who.length).toBe(1);
      expect(HISTORY.joined.axis).toContain(who[0]);
    }
  });

  it('an Allied staff sends none by the book, says why once, and follows where the player has courted by hand', () => {
    const g = playing('UK', 3, 1);
    g.nations.UK.pc = 40;
    const open = Object.keys(g.nations).filter((id) => canCourt(g, 'UK', id).ok);
    expect(open.length).toBeGreaterThan(0);
    orderDesk(g, 'diplomacy', true);
    expect(courted(g, 'UK')).toEqual([]);
    expect(g.nations.UK.pc).toBe(40);
    expect(lines(g, 'diplomacy').join(' ')).toMatch(/sent no envoys: by the book/);
    month(g);
    g.nations.UK.pc = 40;
    expect(lines(g, 'diplomacy')).toEqual([]);
    // The player courts one himself: he has left the book there, and his staff carries on with it.
    orderDesk(g, 'diplomacy', false);
    const target = Object.keys(g.nations).find((id) => canCourt(g, 'UK', id).ok)!;
    expect(orderCourt(g, target)).toBe(true);
    expect(staysOut(g, target, 'allies')).toBe(false);
    month(g);
    g.nations.UK.pc = 40;
    g.flags['dk:ran:diplomacy'] = 0;
    orderDesk(g, 'diplomacy', true);
    if (canCourt(g, 'UK', target).ok || courted(g, 'UK').length) expect(courted(g, 'UK')).toEqual([target]);
  });

  it('at the middle stop and above the envoys go where they are best heard', () => {
    const g = playing('UK', 3);
    g.nations.UK.pc = 40;
    expect(Object.keys(g.nations).some((id) => staysOut(g, id, 'allies'))).toBe(false);
    orderDesk(g, 'diplomacy', true);
    expect(courted(g, 'UK').length).toBe(1);
    expect(g.nations.UK.pc).toBe(40 - DIPLOMACY.court.cost);
    expect(DEPTH.diplomacy.keep).toBeGreaterThan(0);
  });
});

describe('a harbour the player takes from the sea', () => {
  it('is a second front his allies\' staffs know of', () => {
    const g = playing('UK', 5);
    // An empty German harbour within a landing's range of a British quay, and a corps on that quay.
    // (Since ruling 52 Germany's fighters stand over its own coast from the first month, and no invasion fleet
    // crosses under them: the sky over its harbours is taken as won here. That rule is crossing-72's business.)
    for (const r of g.regions) if (r.port && r.ctrl === 'GER') g.sky[r.id] = { nat: 'UK', turn: g.turn };
    let done = false;
    for (const from of g.regions.filter((r) => r.port && r.ctrl === 'UK' && r.owner === 'UK' && !r.overseas)) {
      const c = addCorps(g, 'UK', 'Landing Corps', 'inf', 2, from.id, {});
      const to = landTargets(g, [c]).find((r) => r.port && r.ctrl === 'GER');
      if (!to) { removeCorps(g, c); continue; }
      for (const u of [...to.units]) removeCorps(g, u);
      expect(lodgment(g, 'UK')).toBeNull();
      const res = orderLand(g, [c], to.id);
      expect(res.ok).toBe(true);
      expect(res.taken).toBe(true);
      expect(g.flags['st:lodged:UK']).toBe(to.id + 1);
      expect(lodgment(g, 'UK')?.at).toBe(to);
      // A second harbour taken while the first still stands does not move the mark.
      done = true;
      break;
    }
    expect(done).toBe(true);
    expect(NAVY.landing.range).toBeGreaterThan(0);
  });
});

describe('North America\'s colours and Canada\'s flag (the art director)', () => {
  it('Canada is peach, Cuba indigo, Mexico stands, and the Red Ensign\'s shield has its quarters', () => {
    const g = wide();
    expect(g.nations.CAN.color).toBe('#eba181');
    expect(g.nations.CUB.color).toBe('#644eb2');
    expect(g.nations.MEX.color).toBe('#1e7046');
    expect(FLAGS.CAN).toContain('#f1bf00');
    expect(FLAGS.CAN).toContain('#00247d');
    expect(FLAGS.CAN).toContain('#1b8a4b');
  });
});

describe('an American player goes to war when he chooses (rulings 31 to 33)', () => {
  it('is offered Germany and Italy from the first month, and they are the only ones his ships cannot reach', () => {
    const g = playing('USA');
    expect(g.turn).toBe(1);
    const list = declarable(g);
    expect(list).toContain('GER');
    expect(list).toContain('ITA');
    // The great powers of the other side, and nobody else across the water: not Japan's friends, not the Allies.
    expect(acrossTheWater(g, 'USA', ['GER', 'ITA', 'UK', 'FRA', 'SOV', 'USA'])).toEqual(['GER', 'ITA']);
    // A country with no army to ship across an ocean is offered nobody by this rule, and neither is anyone on the map of Europe.
    expect(acrossTheWater(g, 'MEX', ['GER', 'ITA'])).toEqual([]);
    const small = startOpen(createGame(3, 'USA'), false);
    expect(acrossTheWater(small, 'USA', ['GER', 'ITA'])).toEqual([]);
    // The start screen says when America comes in if he does nothing.
    const us = (grand as unknown as { nations: { id: string; under?: string; peaceUntil?: number }[] }).nations.find((n) => n.id === 'USA')!;
    expect(us.under).toBe('A long peace comes first: in history the United States was not at war until December 1941. Take over in 1942 to begin at war.');
    // (Ruling 138. The line is shown for as long as the year picked begins before that month.)
    expect(turnToDate(us.peaceUntil!).label).toBe('December 1941');
  });

  it('at war with Germany he stands with the Allies, and their ports are open to his corps the same month', () => {
    const g = playing('USA');
    expect(bloc(g.nations.USA)).toBeNull();
    expect(allied(g, 'USA', 'UK')).toBe(false);
    expect(wouldStandWithAllies(g, 'USA', 'GER')).toBe(true);
    expect(wouldStandWithAllies(g, 'USA', 'MEX')).toBe(false);
    const quay = g.byName['New York'];
    const c = field(g, 'USA').find((u) => u.reg === quay.id) ?? addCorps(g, 'USA', 'First Army', 'inf', 1, quay.id, {});
    // At peace no British port takes an American corps.
    expect(shipTargets(g, c).some((r) => r.owner === 'UK')).toBe(false);
    expect(orderDeclare(g, 'GER')).toBe(true);
    expect(atWar(g, 'USA', 'GER')).toBe(true);
    expect(bloc(g.nations.USA)).toBe('allies');
    expect(allied(g, 'USA', 'UK')).toBe(true);
    expect(g.headlines.filter((h) => h.text.startsWith('The United States stands with the Allies')).length).toBe(1);
    // The price is ruling 28: war work climbs from a tenth.
    expect(warFooting(g, 'USA')).toBeLessThan(0.2);
    const british = shipTargets(g, c).filter((r) => r.owner === 'UK' && !r.overseas);
    expect(british.length).toBeGreaterThan(0);
    // A port that cannot take him says why (the page shows it).
    for (const r of g.regions.filter((x) => x.port && x.owner === 'UK' && !x.overseas && !british.includes(x))) expect(whyNotShip(g, c, r)).toBeTruthy();
    expect(orderShip(g, c, british[0].id)).toBe(true);
    expect(c.reg).toBe(british[0].id);
    // Italy is still his to choose.
    expect(declarable(g)).toContain('ITA');
    expect(declarable(g)).not.toContain('GER');
  });

  it('at war with an Axis power that fights no ally yet, he stands with the Allies all the same; a computer America is left to history', () => {
    const g = playing('USA');
    // Italy is at peace with everyone in September 1939.
    expect(g.nations.ITA.atWar.length).toBe(0);
    expect(wouldStandWithAllies(g, 'USA', 'ITA')).toBe(true);
    expect(orderDeclare(g, 'ITA')).toBe(true);
    expect(g.nations.USA.align).toBe('allies_cobelligerent');
    expect(g.headlines.some((h) => /stands with the Allies/.test(h.text))).toBe(true);
    // Asked again it changes nothing.
    expect(standWithAllies(g, 'USA')).toBe(false);
    // A country at war with an Allied power is not moved to the Allies.
    const h = playing('USA');
    setWar(h, 'USA', 'GER');
    setWar(h, 'USA', 'UK');
    expect(standWithAllies(h, 'USA')).toBe(false);
    expect(bloc(h.nations.USA)).toBeNull();
    // Nor is a country that ships no army of its own across the ocean.
    const m = playing('MEX');
    setWar(m, 'MEX', 'GER');
    expect(standWithAllies(m, 'MEX')).toBe(false);
  });

  it('his goals ask for an American army ashore: Paris in Allied hands is not enough, and an army that comes after the fall wins nothing', () => {
    const short = GOALS.USA.short!, long = GOALS.USA.long!;
    expect(short.text).toBe('An American army is ashore in France and Paris is in Allied hands when 1945 opens, with Washington still yours.');
    expect(long.text).toBe('Germany is beaten with an American army on the continent, and Washington still yours.');
    const g = playing('USA');
    openGoals(g, 'USA', 1);
    setWar(g, 'USA', 'GER');
    g.nations.USA.align = 'allies';
    const label = (goal: typeof short, text: string): boolean => goalChecks(g, 'USA', goal).find((c) => c.label === text)!.done;
    // Paris is French and Washington American, and no American stands in France.
    expect(label(short, 'Paris')).toBe(true);
    expect(label(short, 'An American army ashore in France')).toBe(false);
    expect(judgeGoals(g, 'USA')).toEqual([]);
    // A corps in Algiers (France overseas) or on Corsica (a lone island) is not ashore in France; one in Normandy is.
    const far = addCorps(g, 'USA', 'II Corps', 'inf', 2, g.byName.Algiers.id, {});
    expect(label(short, 'An American army ashore in France')).toBe(false);
    removeCorps(g, far);
    const isle = addCorps(g, 'USA', 'II Corps', 'inf', 2, g.byName.Corsica.id, {});
    expect(label(short, 'An American army ashore in France')).toBe(false);
    removeCorps(g, isle);
    const ashore = addCorps(g, 'USA', 'V Corps', 'inf', 2, g.byName.Normandy.id, {});
    expect(label(short, 'An American army ashore in France')).toBe(true);
    expect(label(long, 'An American army on the continent')).toBe(true);
    // It is settled on its date, as January 1945 opens (Version 76, ruling 41: the Playtester won it in September
    // 1939 with one corps in Bordeaux).
    expect(judgeGoals(g, 'USA')).toEqual([]);
    g.turn = 65;
    expect(judgeGoals(g, 'USA').map((x) => [x.key, x.won])).toEqual([['short', true]]);
    // Germany falls with the corps gone: the long campaign is lost that day, and a corps sent afterwards does not win it back.
    removeCorps(g, ashore);
    g.nations.GER.alive = false;
    expect(judgeGoals(g, 'USA').map((x) => [x.key, x.won])).toEqual([['long', false]]);
    addCorps(g, 'USA', 'V Corps', 'inf', 2, g.byName.Normandy.id, {});
    expect(judgeGoals(g, 'USA')).toEqual([]);
    expect(goalState(g, 'long')).toBe('lost');
    // With an American army on the continent on the day Germany falls it is won.
    const w = playing('USA');
    openGoals(w, 'USA', 1);
    setWar(w, 'USA', 'GER');
    w.nations.USA.align = 'allies';
    addCorps(w, 'USA', 'V Corps', 'inf', 2, w.byName.Normandy.id, {});
    w.nations.GER.alive = false;
    expect(judgeGoals(w, 'USA').some((x) => x.key === 'long' && x.won)).toBe(true);
  });
});
