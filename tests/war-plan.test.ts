// The war plan (strategy.ts, data/campaign/strategy.json; after release 1). A computer staff that thinks a season
// ahead: it masses for one planned blow where a front has gone quiet, and answers an enemy foothold on its coast
// with a force railed there at once. It ships switched off, and off it leaves no trace in a game; on, it is for
// computer staffs only, at the dial's stops the data names.

import { afterEach, describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { campaignOf, startOpen } from '../src/campaign/staff';
import { STRATEGY, campaignLog, plansAhead } from '../src/campaign/strategy';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const war = (seed: number, stop: number, months: number, human: string | null = null): Game => {
  const g = startOpen(createGame(seed, human));
  if (stop !== 3) setStop(g, stop, true);
  for (let t = 0; t < months; t++) runTurnAI(g);
  return g;
};
const planFlags = (g: Game): string[] => Object.keys(g.flags).filter((k) => /^st:cam/.test(k));

describe('the war plan', () => {
  const was = { on: STRATEGY.on, staffs: STRATEGY.staffs };
  afterEach(() => { STRATEGY.on = was.on; STRATEGY.staffs = was.staffs; campaignLog.say = null; });

  it('ships switched off, with its numbers in order', () => {
    expect(STRATEGY.on).toBe(false);
    const C = STRATEGY.campaign, F = STRATEGY.foothold;
    expect(C.prep[0]).toBeGreaterThanOrEqual(1);
    expect(C.prep[1]).toBeGreaterThanOrEqual(C.prep[0]);
    // (The staff's own sums ask more than the blow itself does: a plan is not made at odds it would not strike at.)
    expect(C.strike).toBeGreaterThanOrEqual(C.go);
    expect(F.grown).toBeGreaterThanOrEqual(F.size);
    // Never at the two stops that keep to the book.
    expect(STRATEGY.stops.some((s) => s < 3)).toBe(false);
  });

  it('off, no staff plans and nothing of the plan is written into a game', () => {
    const g = war(17, 5, 14);
    expect(plansAhead(g, 'GER')).toBe(false);
    expect(planFlags(g)).toEqual([]);
    expect(campaignOf(g, 'GER')).toBeNull();
  }, 120_000);

  it('on, it is for computer staffs at the named stops, never for the player', () => {
    STRATEGY.on = true;
    const g = startOpen(createGame(17, 'GER'));
    expect(plansAhead(g, 'GER')).toBe(false);
    expect(plansAhead(g, 'SOV')).toBe(STRATEGY.stops.includes(3));
    setStop(g, 1, true);
    expect(plansAhead(g, 'SOV')).toBe(false);
    setStop(g, 5, true);
    expect(plansAhead(g, 'SOV')).toBe(true);
    STRATEGY.staffs = ['UK'];
    expect(plansAhead(g, 'SOV')).toBe(false);
    expect(plansAhead(g, 'UK')).toBe(true);
  });

  it('on, a staff plans a blow, and strikes only where it planned', () => {
    STRATEGY.on = true;
    const said: { t: number; nat: string; what: string; where: string }[] = [];
    campaignLog.say = (g, nat, what, where) => { said.push({ t: g.turn, nat, what, where }); };
    const g = war(17, 5, 30);
    expect(said.some((e) => e.what === 'plan')).toBe(true);
    for (const e of said.filter((x) => x.what === 'strike')) {
      const plan = said.filter((x) => x.nat === e.nat && x.what === 'plan' && x.t <= e.t).pop();
      expect(plan?.where).toBe(e.where);
      // (At least the shortest month of massing lies between the plan and the blow.)
      expect(e.t - plan!.t).toBeGreaterThanOrEqual(STRATEGY.campaign.prep[0]);
    }
    expect(planFlags(g).length).toBeGreaterThan(0);
  }, 240_000);
});
