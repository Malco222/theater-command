// Poland, 1939, with a person in command. History's Poland is beaten in the fourth week by this game's clock and
// was beaten in the fifth by the calendar. A player who places the army well gets through September; one who
// also gives it the right orders holds out longer than history did. Nobody holds out for ever.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { judgeGoals, weeksLasted } from '../src/campaign/goals';
import { beginEve, beginHuman, endEve, endHuman, evePlan, humanUp, orderEvePlan, orderStance, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

/** Plays Poland from the eve of war with the deployment given and no orders after it, until it falls or three months pass. */
function campaign(seed: number, deploy: (g: Game) => void): { g: Game; goals: Record<string, boolean> } {
  const g = createGame(seed, 'POL');
  const goals: Record<string, boolean> = {};
  const judge = (): void => { for (const d of judgeGoals(g, 'POL')) goals[d.key] = d.won; };
  beginEve(g);
  deploy(g);
  endEve(g);
  for (let month = 0; month < 3 && g.nations.POL.alive; month++) {
    for (let guard = 0; !humanUp(g) && g.nations.POL.alive && guard < 80; guard++) stepAI(g);
    judge();
    if (!humanUp(g)) break;
    beginHuman(g);
    endHuman(g);
  }
  judge();
  return { g, goals };
}

const garrisonDigsRestFallBack = (g: Game): void => {
  orderEvePlan(g);
  for (const c of g.corps) if (c.nat === 'POL') orderStance(g, c, g.regions[c.reg].name === 'Warsaw' ? 'dig' : 'back');
};

describe('Poland with a person in command', () => {
  it('has a staff with advice for the eve of war, and nobody else has', () => {
    const g = createGame(1, 'POL');
    expect(evePlan(g)).toBeNull();
    beginEve(g);
    const plan = evePlan(g)!;
    expect(plan.advice.length).toBeGreaterThan(0);
    for (const [name] of plan.place) {
      expect(g.byName[name]?.owner, name).toBe('POL');
      // The staff keeps the army out of the provinces the Red Army will take.
      expect(g.byName[name].sov, name).toBe(false);
    }
    expect(orderEvePlan(g)).toBeGreaterThan(10);
    for (const [name, n] of plan.place) expect(g.byName[name].units.filter((c) => c.nat === 'POL' && c.kind !== 'fort').length, name).toBe(n);
    // Asked twice, it has nothing more to move.
    expect(orderEvePlan(g)).toBe(0);
    endEve(g);
    expect(evePlan(g)).toBeNull();
    expect(orderEvePlan(g)).toBe(0);
    expect(evePlan(createGame(1, 'GER'))).toBeNull();
  });

  for (const seed of [1, 2, 3]) {
    it(`left on the frontier it goes the way history's did (seed ${seed})`, () => {
      const { g, goals } = campaign(seed, () => {});
      expect(g.nations.POL.alive).toBe(false);
      expect(weeksLasted(g, 'POL')).toBe(4);
      expect(goals).toEqual({ short: false, long: false });
    });

    it(`placed as the staff advises it gets through September, and no further (seed ${seed})`, () => {
      const { g, goals } = campaign(seed, (x) => { orderEvePlan(x); });
      expect(weeksLasted(g, 'POL')).toBe(5);
      expect(goals).toEqual({ short: true, long: false });
    });

    it(`with the garrison dug in and the field armies falling back it outlasts history (seed ${seed})`, () => {
      const { g, goals } = campaign(seed, garrisonDigsRestFallBack);
      expect(weeksLasted(g, 'POL') ?? 99).toBeGreaterThan(5);
      expect(goals).toEqual({ short: true, long: true });
      // Seven corps reached the capital whole, and Germany had to bring up most of its army to take it.
      const last = g.reports.filter((r) => r.regionName === 'Warsaw').pop()!;
      expect(last.defenders.length).toBe(7);
      expect(last.attackers.length).toBeGreaterThanOrEqual(12);
    });
  }
});
