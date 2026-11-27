// Japan's seat on the whole-world chart (release 2, Japan and the Pacific, 24 November 2026): what a player who
// takes Japan plays for. Ruling 196, points 6 and 7, was ruled on 17 November and never built: Japan had no goals
// at all ("GOALS: short open; long open" in the playtester's logs, with nothing to win), and America's long goal
// still read Germany alone. Goals judge a war and never change it.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { GOALS, GOAL_KEYS, goalChecks, goalEnds, goalState, judgeGoals, openGoals } from '../src/campaign/goals';
import { startOpen } from '../src/campaign/staff';
import { TECH, gateOf, hasTech, teamsOf, techOf } from '../src/campaign/tech';
import type { Game } from '../src/campaign/types';

const MAY_42 = 33, DEC_45 = 76;
function seat(nat: string): Game {
  const g = startOpen(createGame(3, nat));
  openGoals(g, nat, 1);
  return g;
}
const give = (g: Game, nat: string, names: string[]): void => { for (const n of names) g.byName[n].ctrl = nat; };

describe("Japan's goals as ruling 196 wrote them", () => {
  it('Oil for the fleet, and The ring holds', () => {
    expect(GOALS.JAP.short).toMatchObject({ title: 'Oil for the fleet', hold: ['Singapore', 'Manila', 'Batavia', 'Palembang'], by: MAY_42 });
    expect(GOALS.JAP.long).toMatchObject({ title: 'The ring holds', hold: ['Tokyo', 'Okinawa', 'Saipan', 'Palembang'], until: DEC_45 });
    const g = seat('JAP');
    for (const key of GOAL_KEYS) {
      const goal = GOALS.JAP[key]!;
      for (const name of goal.hold ?? []) expect(g.byName[name], name).toBeDefined();
      expect(goalState(g, key)).toBe('open');
    }
    expect(goalEnds(GOALS.JAP.short!)).toBeLessThan(goalEnds(GOALS.JAP.long!));
    // On the first day Japan holds its own three of the ring and none of the four oil ports: nothing is won at the door.
    expect(goalChecks(g, 'JAP', GOALS.JAP.short!).map((c) => c.done)).toEqual([false, false, false, false]);
    expect(goalChecks(g, 'JAP', GOALS.JAP.long!).map((c) => [c.label, c.done])).toEqual([['Tokyo', true], ['Okinawa', true], ['Saipan', true], ['Palembang', false]]);
    expect(judgeGoals(g, 'JAP')).toEqual([]);
  }, 300000);

  it('the short campaign is won the month the four are Japan\'s, and lost when May 1942 has gone by', () => {
    const g = seat('JAP');
    g.turn = 30;
    give(g, 'JAP', ['Singapore', 'Manila', 'Batavia']);
    expect(judgeGoals(g, 'JAP')).toEqual([]);
    give(g, 'JAP', ['Palembang']);
    expect(judgeGoals(g, 'JAP').map((x) => [x.key, x.won])).toEqual([['short', true]]);
    expect(g.headlines[g.headlines.length - 1].text).toBe("Japan's short campaign is won: Oil for the fleet.");
    const late = seat('JAP');
    late.turn = MAY_42 + 2;
    give(late, 'JAP', ['Singapore', 'Manila', 'Batavia', 'Palembang']);
    expect(judgeGoals(late, 'JAP').map((x) => [x.key, x.won])).toEqual([['short', false]]);
  }, 300000);

  it('the long campaign is judged when 1946 opens: the ring held, or not', () => {
    const g = seat('JAP');
    g.flags['goal:short'] = 2;
    give(g, 'JAP', ['Palembang']);
    g.turn = DEC_45;
    expect(judgeGoals(g, 'JAP')).toEqual([]);
    g.turn = DEC_45 + 1;
    expect(judgeGoals(g, 'JAP').map((x) => [x.key, x.won])).toEqual([['long', true]]);
    const lost = seat('JAP');
    lost.flags['goal:short'] = 2;
    give(lost, 'JAP', ['Palembang']);
    give(lost, 'USA', ['Saipan']);
    lost.turn = DEC_45 + 1;
    expect(judgeGoals(lost, 'JAP').map((x) => [x.key, x.won])).toEqual([['long', false]]);
  }, 300000);
});

describe("America's long goal once Japan is on the chart", () => {
  it('Victory on two oceans: Germany and Japan beaten, the army on the continent, Washington held', () => {
    const long = GOALS.USA.long!;
    expect(long).toMatchObject({ title: 'Victory on two oceans', beaten: ['GER', 'JAP'], hold: ['Washington'], by: DEC_45 });
    expect(long.ashore?.in).toContain('FRA');
    expect(GOALS.USA.short).toMatchObject({ title: 'A second front' });
    const g = seat('USA');
    expect(goalChecks(g, 'USA', long).map((c) => c.label)).toEqual(['Washington', 'Germany beaten', 'Japan beaten', 'An American army on the continent']);
  }, 300000);
});

// Ruling 68(a), the standing rule (16 November): nothing is reserved to one country, and "Japan and America get
// starting lists, not private technologies". America had its list; Japan opened with none, with one research team
// where every other great power has two, and with the carrier's own technology unresearched (a year's work before
// its national ability, cheaper carriers, meant anything).
describe("Japan's place in the technology tree", () => {
  it('opens September 1939 with the historian\'s four rows, every one of them a row of the common tree', () => {
    const g = seat('JAP');
    // (The historian's seed of 25 November: anti-tank guns in, the tank arm and monoplane fighters to be earned.)
    expect(techOf(g, 'JAP')!.done).toEqual(['antitank', 'mediumbombers', 'carriers', 'fleetsubs']);
    for (const id of ['tanks', 'monoplanes']) expect(hasTech(g, 'JAP', id), id).toBe(false);
    for (const id of TECH.start.JAP) expect(TECH.techs.some((t) => t.id === id), id).toBe(true);
    // The carrier's gate is the common one, and Japan is past it on the first day; Britain and America as before.
    expect(gateOf('ship:cv')?.id).toBe('carriers');
    for (const nat of ['JAP', 'UK', 'USA']) expect(hasTech(g, nat, 'carriers'), nat).toBe(true);
    expect(hasTech(g, 'GER', 'carriers')).toBe(false);
  }, 300000);

  it('has two research teams, as each of the other six great powers has, and a staff whose order on the first day is the read of the equipment money', () => {
    const g = seat('JAP');
    for (const nat of ['GER', 'UK', 'FRA', 'SOV', 'ITA', 'USA', 'JAP']) expect(teamsOf(g, nat), nat).toBe(2);
    expect(teamsOf(g, 'CHN')).toBe(1);
    // (The historian's read of the 1936 to 1940 equipment money, ruled 25 November 03:22Z: the army's arms first; the air's place is flagged.)
    expect(TECH.ai.order.JAP).toEqual(['land', 'naval', 'air', 'industry', 'doctrine']);
  }, 300000);

  it('leaves every older row of the tree as it was', () => {
    expect(TECH.start.USA).toEqual(['tanks', 'motor', 'monoplanes', 'mediumbombers', 'carriers', 'machinetools', 'assembly']);
    expect(TECH.start.GER).toHaveLength(8);
    expect(TECH.slots).toMatchObject({ _default: 1, GER: 2, UK: 2, FRA: 2, SOV: 2, ITA: 2, USA: 2 });
  });
});
