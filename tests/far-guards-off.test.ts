// The far chart's town guard with the far side's switch off: siege.json holds no table, no province is looked up, and
// every town's guard is the old reckoning's (far-guards.json; the design director's ruling of 05:54Z of 21 November
// 2026: with the switch off the hash and the 96 wars cannot move).
import { describe, expect, it } from 'vitest';
import { armySize, createGame } from '../src/campaign/game';
import { farGuard, farGuardRow } from '../src/campaign/economy';
import { FAR_LAID } from '../src/campaign/far-rows';
import { SIEGE, guardOf } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { townSize } from '../src/campaign/towns';
import siege from '../data/campaign/siege.json';

describe("the far chart's town guard, switch off", () => {
  it('no table is laid and every guard is the old reckoning', () => {
    expect(FAR_LAID).toEqual([]);
    expect([SIEGE.farGuard, (siege as any).farGuard]).toEqual([undefined, undefined]);
    const g = startOpen(createGame(5, null));
    let towns = 0;
    for (const r of g.regions) {
      expect(farGuardRow(g, r), r.name).toBeNull();
      const n = g.nations[r.owner];
      if (g.flags.sieges !== 1 || r.ctrl !== r.owner || !n?.alive) { expect(guardOf(g, r), r.name).toBe(0); continue; }
      const full = Math.min(SIEGE.guard[townSize(g, r)], SIEGE.guardShare * Math.max(n.armyPeak, armySize(g, n.id))) * farGuard(r);
      expect(guardOf(g, r), r.name).toBe(full > 0.02 ? full : 0);
      if (full > 0.02) towns++;
    }
    expect(towns).toBeGreaterThan(50);
  });
});
