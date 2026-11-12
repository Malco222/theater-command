// Campaign goals: a short campaign and a long one for each nation a person can play. They judge the war and
// never change it, so a war with nobody at the controls carries no trace of them.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { GOALS, GOAL_KEYS, goalChecks, goalEnds, goalState, judgeGoals, openGoals, weeksLasted } from '../src/campaign/goals';
import { beginEve, runTurnAI } from '../src/campaign/turn';
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
        expect([goal.by, goal.until, goal.outlast].filter((x) => x !== undefined).length, `${nat} ${key}`).toBe(1);
      }
      expect(goalEnds(pair.short!), nat).toBeLessThanOrEqual(goalEnds(pair.long!));
      expect(pair.short!.outlast ?? goalEnds(pair.short!), nat).toBeLessThan(pair.long!.outlast ?? goalEnds(pair.long!));
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
    const g = structuredClone(war);
    // London and Cairo are British all through history's 1941: the goal is decided when 1942 opens, not before.
    g.turn = 28;
    expect(judgeGoals(g, 'UK')).toEqual([]);
    g.turn = 29;
    expect(judgeGoals(g, 'UK').map((d) => [d.key, d.won])).toEqual([['short', true]]);
  });
});

describe('holding out', () => {
  it('gives history\'s Poland neither goal: it is beaten in the fourth week', () => {
    const g = structuredClone(war);
    expect(weeksLasted(g, 'POL')).toBe(4);
    expect(judgeGoals(g, 'POL').map((d) => [d.key, d.won])).toEqual([['short', false], ['long', false]]);
  });

  it('counts the week a nation fell, so going down fighting later than history is a goal won', () => {
    const fiveWeeks = structuredClone(war);
    fiveWeeks.nations.POL.fellOn = { turn: 2, week: 1, to: 'GER' };
    expect(judgeGoals(fiveWeeks, 'POL').map((d) => [d.key, d.won])).toEqual([['short', true], ['long', false]]);
    const sixWeeks = structuredClone(war);
    sixWeeks.nations.POL.fellOn = { turn: 2, week: 2, to: 'GER' };
    expect(judgeGoals(sixWeeks, 'POL').map((d) => [d.key, d.won])).toEqual([['short', true], ['long', true]]);
    // Both are dated to the week Poland fell.
    expect(sixWeeks.headlines.slice(-2).map((h) => [h.kind, h.turn, h.week])).toEqual([['goal', 2, 2], ['goal', 2, 2]]);
  });

  it('decides nothing on the eve of war, and gives a nation still standing its weeks when its own month comes round', () => {
    const g = createGame(1, 'POL');
    beginEve(g);
    expect(judgeGoals(g, 'POL')).toEqual([]);
    // Germany's September has been played and Poland is still there: four weeks, not yet five.
    g.flags.eve = 0;
    g.phase = g.order.indexOf('POL');
    expect(judgeGoals(g, 'POL').map((d) => [d.key, d.won])).toEqual([['short', true]]);
    // The news is dated to the last week Poland came through, not to the week the count was made.
    expect(g.headlines.at(-1)).toMatchObject({ kind: 'goal', turn: 1, week: 4 });
    // The month turns over: the fifth week has not been fought until Germany has moved again.
    g.turn = 2;
    g.phase = 0;
    expect(judgeGoals(g, 'POL')).toEqual([]);
    g.phase = g.order.indexOf('POL');
    expect(judgeGoals(g, 'POL').map((d) => [d.key, d.won])).toEqual([['long', true]]);
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
