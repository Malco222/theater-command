// Campaign goals: a short campaign and a long one for each nation a person can play. They judge the war and
// never change it, so a war with nobody at the controls carries no trace of them.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { GOALS, GOAL_KEYS, goalChecks, goalEnds, goalState, judgeGoals, openGoals } from '../src/campaign/goals';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const JAN_41 = 17;

function history(seed: number, until: number): Game {
  const g = createGame(seed, null);
  while (g.turn < until) runTurnAI(g);
  return g;
}

// One war on history's plan to the opening of 1941, shared by the tests below.
const war = history(3, JAN_41);

describe('the goals as written', () => {
  it('name nations and cities that are on the map, and each has one kind of time limit', () => {
    const g = createGame(1, null);
    for (const [nat, pair] of Object.entries(GOALS)) {
      expect(g.nations[nat], nat).toBeDefined();
      for (const key of GOAL_KEYS) {
        const goal = pair[key]!;
        expect(goal, `${nat} ${key}`).toBeDefined();
        for (const name of goal.hold ?? []) expect(g.byName[name], `${nat} ${key}: ${name}`).toBeDefined();
        for (const id of goal.beaten ?? []) expect(g.nations[id], `${nat} ${key}: ${id}`).toBeDefined();
        expect((goal.by === undefined) !== (goal.until === undefined), `${nat} ${key}`).toBe(true);
      }
      expect(goalEnds(pair.short!), nat).toBeLessThan(goalEnds(pair.long!));
    }
  });

  it('are not already won on the first day', () => {
    const g = createGame(2, null);
    for (const nat of Object.keys(GOALS)) for (const key of GOAL_KEYS) {
      const goal = GOALS[nat][key]!;
      if (goal.by !== undefined) expect(goalChecks(g, nat, goal).every((c) => c.done), `${nat} ${key}`).toBe(false);
    }
  });
});

describe('judging a war', () => {
  it('leaves a war nobody plays untouched', () => {
    expect(Object.keys(war.flags).filter((k) => k.startsWith('goal:'))).toEqual([]);
    expect(war.headlines.some((h) => h.kind === 'goal')).toBe(false);
  });

  it('gives history\'s Germany its short campaign: Warsaw, Oslo and Paris by the end of 1940', () => {
    const g = structuredClone(war);
    expect(goalChecks(g, 'GER', GOALS.GER.short!).map((c) => c.done)).toEqual([true, true, true, true]);
    const decided = judgeGoals(g, 'GER');
    expect(decided.map((d) => [d.key, d.won])).toEqual([['short', true]]);
    expect(goalState(g, 'short')).toBe('won');
    expect(goalState(g, 'long')).toBe('open');
    expect(g.headlines[g.headlines.length - 1]).toMatchObject({ kind: 'goal', nat: 'GER' });
    // Asked again, it has nothing new to say.
    expect(judgeGoals(g, 'GER')).toEqual([]);
  });

  it('gives history\'s France neither: Paris is lost, and a beaten nation has no long campaign left', () => {
    const g = structuredClone(war);
    expect(judgeGoals(g, 'FRA').map((d) => [d.key, d.won])).toEqual([['short', false], ['long', false]]);
  });

  it('decides nothing early for a goal of endurance', () => {
    const g = structuredClone(war);
    expect(judgeGoals(g, 'UK')).toEqual([]);
    expect(goalState(g, 'short')).toBe('open');
  });

  it('counts a goal of endurance when its month has gone by', () => {
    const g = history(5, 3);
    const alive = g.nations.POL.alive;
    const decided = judgeGoals(g, 'POL');
    expect(decided.find((d) => d.key === 'short')?.won).toBe(alive);
  });
});

describe('a campaign that opens late', () => {
  it('leaves out a goal whose time has gone by, and one the start would hand over already won', () => {
    const g = structuredClone(war);
    openGoals(g, 'GER', JAN_41);
    expect(goalState(g, 'short')).toBe('out');
    expect(goalState(g, 'long')).toBe('open');
    const early = history(4, 12);
    openGoals(early, 'GER', 12);
    // Paris has fallen by August 1940: the short campaign would be won before the player gave an order.
    expect(early.byName['Paris'].ctrl).toBe('GER');
    expect(goalState(early, 'short')).toBe('out');
    openGoals(early, 'UK', 12);
    expect(goalState(early, 'short')).toBe('open');
  });
});
