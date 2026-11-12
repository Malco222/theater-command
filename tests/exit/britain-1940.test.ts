// The framework's Phase 2 exit test, run on the campaign map: played historically, the
// Luftwaffe fails to win air superiority over southern England by mid-September 1940.
// The what-if is tested too: a player who commits every air fleet against the airfields of
// one corner of England, and keeps at it, can win the sky that history's plan never did.
import { describe, expect, it } from 'vitest';
import { addCorps, createGame } from '../../src/campaign/game';
import { AIR, airFleetsOf, airPool, fightersOver, skyDenies, skyOver } from '../../src/campaign/air';
import { fleetPower, landingRisk, sortieAgainst, zoneOf } from '../../src/campaign/navy';
import { beginHuman, endHuman, humanUp, landTargets, orderAir, orderAirBase, orderLand, previewLanding, runTurnAI, stepAI } from '../../src/campaign/turn';
import type { AirReport, Game } from '../../src/campaign/types';

const JULY = 11, AUGUST = 12, SEPTEMBER = 13, OCTOBER = 14, NOVEMBER = 15;
const SOUTH = ['Kent', 'Hampshire', 'London'];

interface Run { g: Game; sky: string[] }

/** Every nation on history's plan to the end of November 1940, noting each month who holds the sky over southern England. */
function history(seed: number): Run {
  const g = createGame(seed, null);
  const sky: string[] = [];
  for (let t = 1; t <= NOVEMBER; t++) {
    runTurnAI(g);
    for (const n of SOUTH) { const s = skyOver(g, g.byName[n].id); if (s) sky.push(`${n} T${t} ${s}`); }
  }
  return { g, sky };
}

const SEEDS = Array.from({ length: 20 }, (_, i) => i + 1);
const runs = SEEDS.map((s) => history(s));
const of = (g: Game, turn: number, mission?: string): AirReport[] => g.airReports.filter((r) => r.turn === turn && (!mission || r.mission === mission));
const median = (a: number[]): number => [...a].sort((x, y) => x - y)[Math.floor(a.length / 2)];

describe('Phase 2 exit test: the Battle of Britain on the historical plan', () => {
  it('the Luftwaffe never wins the sky over southern England', () => {
    for (const { g, sky } of runs) {
      expect(sky).toEqual([]);
      expect(g.airReports.some((r) => r.sky)).toBe(false);
      // It is not even close: the fighters Germany has left never come near what Britain can still put up.
      for (const r of g.airReports.filter((x) => x.mission === 'superiority')) expect(r.hold).toBeLessThan(AIR.sky.win * 0.85);
    }
  });

  it('the campaign runs as it did: the Channel in July, the airfields in August, London in September, then the night Blitz', () => {
    for (const { g } of runs) {
      expect(of(g, JULY).map((r) => `${r.mission} ${r.place}`)).toEqual(['strike the English Channel']);
      const august = of(g, AUGUST).map((r) => `${r.mission} ${r.place}`);
      expect(august).toContain('superiority Kent');
      expect(august).toContain('superiority Hampshire');
      expect(of(g, SEPTEMBER, 'bomb').map((r) => r.place)).toEqual(['London']);
      expect(of(g, SEPTEMBER, 'night').map((r) => r.place)).toEqual(['London']);
      expect(of(g, NOVEMBER).every((r) => r.mission === 'night')).toBe(true);
      expect(of(g, NOVEMBER).length).toBeGreaterThanOrEqual(2);
    }
  });

  it('it costs Fighter Command about a thousand fighters and five hundred pilots', () => {
    const ftr = runs.map(({ g }) => airPool(g, 'UK').lostFtr), pilots = runs.map(({ g }) => airPool(g, 'UK').lostPilots);
    // History: 1,023 fighters and 544 pilots between July and October.
    expect(Math.min(...ftr)).toBeGreaterThan(800);
    expect(Math.max(...ftr)).toBeLessThan(1400);
    expect(median(ftr)).toBeGreaterThan(950);
    expect(median(ftr)).toBeLessThan(1200);
    expect(Math.min(...pilots)).toBeGreaterThan(400);
    expect(Math.max(...pilots)).toBeLessThan(750);
  });

  it('and the Luftwaffe nearly two thousand aircraft, more than it destroys', () => {
    const lost = runs.map(({ g }) => airPool(g, 'GER').lostFtr + airPool(g, 'GER').lostBmr);
    // History: 1,887 aircraft.
    expect(Math.min(...lost)).toBeGreaterThan(1500);
    expect(Math.max(...lost)).toBeLessThan(2400);
    expect(median(lost)).toBeGreaterThan(1700);
    expect(median(lost)).toBeLessThan(2150);
    for (const { g } of runs) {
      const ge = airPool(g, 'GER'), uk = airPool(g, 'UK');
      expect(ge.lostFtr + ge.lostBmr).toBeGreaterThan(uk.lostFtr + uk.lostBmr);
      expect(ge.lostPilots).toBeGreaterThan(uk.lostPilots * 0.8);
    }
  });

  it('Fighter Command ends the autumn as strong as it began the summer', () => {
    for (const { g } of runs) {
      const groups = airFleetsOf(g, 'UK').filter((f) => f.est.ftr > 0);
      expect(groups.length).toBe(4);
      const have = groups.reduce((s, f) => s + f.ftr, 0), est = groups.reduce((s, f) => s + f.est.ftr, 0);
      expect(have).toBeGreaterThanOrEqual(est * 0.9);
      expect(airPool(g, 'UK').pilots).toBeGreaterThan(100);
    }
  });

  it('London is bombed and Britain is never invaded', () => {
    for (const { g } of runs) {
      expect(g.nations.UK.alive).toBe(true);
      for (const r of g.regions) if (r.owner === 'UK') expect(r.ctrl, r.name).toBe('UK');
      const london = g.airReports.filter((r) => r.place === 'London');
      expect(london.length).toBeGreaterThanOrEqual(4);
      for (const r of london) expect(r.result).toMatch(/^The bombing stopped \d+% of London's output/);
    }
  });
});

/** History to the end of June 1940, then a player takes over the Luftwaffe. */
function germanPlayer(seed: number): Game {
  const g = createGame(seed, null);
  for (let t = 1; t < JULY; t++) runTurnAI(g);
  g.human = 'GER';
  beginHuman(g);
  return g;
}

function nextMonth(g: Game): void {
  endHuman(g);
  let guard = 0;
  while (!humanUp(g) && guard++ < 60) stepAI(g);
  beginHuman(g);
}

/** Sends the named fleets to their bases, rests them through July, then flies them against Kent every month. Returns the month the sky was won, or 0. */
function againstKent(seed: number, bases: Record<string, string>, until: number, game?: Game): number {
  const g = game ?? germanPlayer(seed);
  const kent = g.byName['Kent'].id;
  const mine = (): [number, string][] => airFleetsOf(g, 'GER').filter((f) => bases[f.name]).map((f) => [f.id, bases[f.name]]);
  for (const [id, base] of mine()) { expect(orderAirBase(g, id, g.byName[base].id)).toBe(true); expect(orderAir(g, id, 'rest')).toBe(true); }
  nextMonth(g);
  while (g.turn <= until) {
    for (const [id] of mine()) expect(orderAir(g, id, 'superiority', kent)).toBe(true);
    const t = g.turn;
    endHuman(g);
    const r = g.airReports[g.airReports.length - 1];
    expect(r.place).toBe('Kent');
    let guard = 0;
    if (r.sky) {
      expect(skyOver(g, kent)).toBe('GER');
      // Hand the player his next month, so that a caller can go on from here.
      while (!humanUp(g) && guard++ < 60) stepAI(g);
      beginHuman(g);
      return t;
    }
    while (!humanUp(g) && guard++ < 60) stepAI(g);
    beginHuman(g);
  }
  return 0;
}

describe('the what-if: a different Luftwaffe plan', () => {
  const EVERYTHING = { 'Luftflotte 2': 'Calais', 'Luftflotte 3': 'Flanders', 'Luftflotte 1': 'Lille', 'Luftflotte 4': 'Somme', 'Luftflotte 5': 'Rouen' };

  it('every air fleet, rested and thrown at the airfields of Kent month after month, wins the sky there by the autumn', () => {
    for (const seed of [1, 2, 3, 4, 5, 6]) {
      const won = againstKent(seed, EVERYTHING, NOVEMBER);
      // Not in the first month: it takes weeks of bombing to wear No. 11 Group's airfields down.
      expect(won, `seed ${seed}`).toBeGreaterThanOrEqual(SEPTEMBER);
      expect(won, `seed ${seed}`).toBeLessThanOrEqual(OCTOBER);
    }
  });

  it('the two western air fleets alone cannot, however long they try', () => {
    for (const seed of [1, 2, 3, 4]) {
      expect(againstKent(seed, { 'Luftflotte 2': 'Calais', 'Luftflotte 3': 'Flanders' }, NOVEMBER + 1), `seed ${seed}`).toBe(0);
    }
  });

  it('nor can everything crammed onto the airfields of Calais', () => {
    for (const seed of [1, 2]) {
      const all = Object.fromEntries(Object.keys(EVERYTHING).map((k) => [k, 'Calais']));
      expect(againstKent(seed, all, OCTOBER), `seed ${seed}`).toBe(0);
    }
  });
});

describe('the what-if: different research', () => {
  /** History to the end of June 1940, with one power's design teams moved on the first day of the war. */
  function withTeams(seed: number, nat: string, teams: Record<string, number>): Game {
    const g = createGame(seed, null);
    Object.assign(g.nations[nat].lab!.teams, teams);
    for (let t = 1; t < JULY; t++) runTurnAI(g);
    g.human = 'GER';
    beginHuman(g);
    return g;
  }

  it('a Britain that doubled its fighter and radar teams in 1939 keeps its sky even against every air fleet', () => {
    for (const seed of [1, 2]) {
      const g = withTeams(seed, 'UK', { fighters: 2, radar: 2, armour: 0, doctrine: 0 });
      const all = { 'Luftflotte 2': 'Calais', 'Luftflotte 3': 'Flanders', 'Luftflotte 1': 'Lille', 'Luftflotte 4': 'Somme', 'Luftflotte 5': 'Rouen' };
      expect(againstKent(seed, all, NOVEMBER, g), `seed ${seed}`).toBe(0);
    }
  });

  it('a Germany that put three teams on fighters in 1939 wins it with the two western air fleets alone', () => {
    for (const seed of [1, 2]) {
      const g = withTeams(seed, 'GER', { fighters: 3, navy: 0, radar: 0 });
      const won = againstKent(seed, { 'Luftflotte 2': 'Calais', 'Luftflotte 3': 'Flanders' }, NOVEMBER, g);
      expect(won, `seed ${seed}`).toBeGreaterThanOrEqual(SEPTEMBER);
      expect(won, `seed ${seed}`).toBeLessThanOrEqual(NOVEMBER);
    }
  });
});

describe('Operation Sea Lion waits on the air battle', () => {
  const EVERYTHING = { 'Luftflotte 2': 'Calais', 'Luftflotte 3': 'Flanders', 'Luftflotte 1': 'Lille', 'Luftflotte 4': 'Somme', 'Luftflotte 5': 'Rouen' };

  it('no invasion fleet can cross while British fighters hold the sky over the beach', () => {
    const g = germanPlayer(2);
    const kent = g.byName['Kent'];
    const corps = addCorps(g, 'GER', 'Sea Lion Corps', 'inf', 3, 'Calais', {});
    expect(fightersOver(g, kent.id, 'GER')).toBeGreaterThan(400);
    expect(skyDenies(g, 'GER', kent.id)).toBe(true);
    const plan = previewLanding(g, [corps], kent.id);
    expect(plan.why).toBe('British fighters still hold the sky over Kent, and no invasion fleet can cross under them. Win the sky there first.');
    const coasts = landTargets(g, [corps]).map((r) => r.name);
    for (const n of ['Kent', 'London', 'Hampshire', 'East Anglia']) expect(coasts).not.toContain(n);
    expect(orderLand(g, [corps], kent.id).ok).toBe(false);
    // A coast no fighter group can reach is not protected by the sky, only by the fleet.
    expect(fightersOver(g, g.byName['Cornwall'].id, 'GER')).toBe(0);
    expect(skyDenies(g, 'GER', g.byName['Cornwall'].id)).toBe(false);
  });

  it('the Royal Navy puts to sea against an invasion', () => {
    const g = germanPlayer(2);
    const channel = zoneOf(g, 'channel');
    const dover = g.fleets.find((f) => f.name === 'Dover Command')!;
    const coming = sortieAgainst(g, 'GER', [channel]);
    // Dover Command from its harbour on the Channel, and the fleets on the seas either side of it.
    expect(coming).toBeGreaterThan(fleetPower(g, dover) * 0.5);
    expect(coming).toBeGreaterThan(40);
    // Against a coast with no fighters over it, that alone makes a crossing very costly.
    const corps = addCorps(g, 'GER', 'Sea Lion Corps', 'inf', 3, 'Calais', {});
    const plan = previewLanding(g, [corps], g.byName['Cornwall'].id);
    expect(plan.why).toBeNull();
    expect(plan.sky).toBe(false);
    expect(plan.sortie).toBe(coming);
    expect(plan.risk).toBeGreaterThan(0.4);
  });

  it('once the sky over Kent is won the troopships can sail, at a price', () => {
    const g = germanPlayer(1);
    const kent = g.byName['Kent'];
    const won = againstKent(1, EVERYTHING, NOVEMBER, g);
    expect(won).toBeGreaterThan(0);
    expect(skyDenies(g, 'GER', kent.id)).toBe(false);
    const corps = [addCorps(g, 'GER', 'Sea Lion Corps', 'inf', 3, 'Calais', {}), addCorps(g, 'GER', 'Second Wave', 'inf', 3, 'Calais', {})];
    expect(landTargets(g, corps).map((r) => r.name)).toContain('Kent');
    const plan = previewLanding(g, corps, kent.id);
    expect(plan.why).toBeNull();
    expect(plan.sky).toBe(true);
    expect(plan.risk).toBeGreaterThan(0.1);
    expect(plan.risk).toBeLessThan(0.45);
    // Without the sky the same fleet would do far more harm.
    const held = g.sky[kent.id];
    delete g.sky[kent.id];
    const naked = landingRisk(g, 'GER', plan.path, kent);
    g.sky[kent.id] = held;
    expect(naked).toBeGreaterThan(plan.risk * 1.5);
    const res = orderLand(g, corps, kent.id);
    expect(res.ok).toBe(true);
    expect(res.lostAtSea).toBeGreaterThan(0);
    expect(res.report).not.toBeNull();
  });
});
