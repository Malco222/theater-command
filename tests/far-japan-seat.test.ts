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
