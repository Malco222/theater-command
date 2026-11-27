// A fleet's beat lies within its base's reach after every move of its base, on the old chart too (release 2; the design
// director's rulings of 20:02Z, 20:19Z and 21:09Z (a), 25 November 2026; navy.ts beatReachOn, beatOnMove). The rule is
// read by rebase and turnBack on both charts: here, the Mediterranean Fleet's base falls on the old chart.
import { describe, expect, it } from 'vitest';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { fleetReach, navalUpkeep, setBeatReach } from '../src/campaign/navy';
import { farOn } from '../src/campaign/world';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game } from '../src/campaign/types';

const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;

function war(on: boolean): Game {
  const g = startOpen(createGame(3, null));
  expect(farOn(g)).toBe(false);
  if (on) setBeatReach(g, true);
  if (!atWar(g, 'ITA', 'UK')) declareWar(g, 'ITA', 'UK');
  return g;
}

describe('the beat-in-reach rule on the old chart', () => {
  it('a fleet whose base falls: on, a beat out of its new base\'s reach becomes its new home sea; off, it keeps the beat it had', () => {
    const g0 = war(false), f0 = fleet(g0, 'UK', 'Mediterranean Fleet');
    const lost0 = g0.regions[f0.base];
    lost0.ctrl = 'ITA';
    navalUpkeep(g0, 'UK');
    expect(g0.fleets).toContain(f0);
    const to = f0.base;
    expect(to).not.toBe(lost0.id);
    const out = g0.seas.findIndex((z, i) => z.adj.length > 0 && !fleetReach(g0, f0, true).includes(i));
    expect(out).toBeGreaterThanOrEqual(0);
    for (const on of [false, true]) {
      const g = war(on), f = fleet(g, 'UK', 'Mediterranean Fleet');
      const lost = g.regions[f.base];
      f.station = out;
      lost.ctrl = 'ITA';
      navalUpkeep(g, 'UK');
      expect(f.base).toBe(to);
      expect(f.station).toBe(on ? f.home : out);
      if (on) expect(fleetReach(g, f, true)).toContain(f.station);
    }
  });
});
