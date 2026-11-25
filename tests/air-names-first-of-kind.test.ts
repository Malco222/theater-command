// Where two lines of release 2 meet: the Pacific's first group of a kind (a command the record gives fighters for
// the first time, far-pacific-air.test.ts) named under the air arms' own names (the switch 'airNames', the design
// director's rulings of 25 November 2026). The names lane tells every group it names its short form and whether its
// name says its role; the Pacific's new group came in beside it and was named without being told. Found by the
// read of the join, 25 November 2026.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { airUpkeep } from '../src/campaign/air';
import { setAirNames } from '../src/campaign/airnames';
import { groupRow } from '../src/campaign/airwar';
import { createGame } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import type { AirFleet, Game } from '../src/campaign/types';

const war = (names: boolean): Game => {
  const g = startOpen(createGame(3, null));
  setStop(g, 1, true);
  if (names) setAirNames(g, true);
  g.turn = 24;
  airUpkeep(g, 'UK');
  g.turn = 25;
  g.airPool.UK = { ftr: 500, bmr: 500, pilots: 500, lostFtr: 0, lostBmr: 0, lostPilots: 0 };
  airUpkeep(g, 'UK');
  return g;
};
const fighters = (g: Game): AirFleet => g.airFleets.find((f) => (f.raised ?? f.nat) === 'UK' && f.parent === 'Far East Command' && f.kind === 'ftr')!;

describe('Far East Command\'s first fighters, named by the air arm\'s own names', () => {
  it('the new group is told whether its name says its role, as every other group named under the switch is', () => {
    const g0 = war(true);
    const f = fighters(g0);
    expect(f).toBeDefined();
    expect(f.roleNamed).toBeDefined();
    // (And its row's short form is the arm's own where the arm keeps one, never left as the long name by default.)
    expect(groupRow(g0, f).short).toBe(f.short ?? f.name);
  });

  it('with the switch off the new group is told nothing, and is named as ruling 220 names it', () => {
    const f = fighters(war(false));
    expect(f).toBeDefined();
    expect(f.roleNamed).toBeUndefined();
    expect(f.short).toBeUndefined();
  });
});
