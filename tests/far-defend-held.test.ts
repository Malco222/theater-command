// A staff's defence reads a threat as its own front sums do: an enemy corps held in its place is no threat (the
// design director's ruling of 05:55Z, 21 November 2026, on Mergui's trace, 4; ai.ts `defend`). Behind the far switch.
//
// As measured (war 22 at stop 3, Britain's month 30): Mergui stood empty beside Bangkok, where Thailand's 1st
// Division was held as its capital's garrison, and beside the Kra. The reserve rule counted the held garrison in
// full, as staff.ts `fronts` does not, and marched the whole of Burma's army out of Rangoon into Mergui, the end of
// a strip; Moulmein fell behind it the same month. One staff weighing a neighbour two ways is the fault. The states
// are set by hand; one month of Britain's own is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { runAI } from '../src/campaign/ai';
import { addCorps, createGame, hostileUnits, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

const BURMA = 'Burma Army (the Burma Rifles; the 1st Burma Division from July 1941)';

/** Month 30 with Siam in the war against Britain, and one Thai garrison division at Bangkok, held or free. */
const war = (held: boolean): { g: Game; burma: Corps; thai: Corps } => {
  const g = startOpen(createGame(3, null));
  g.turn = 30;
  setWar(g, 'THA', 'UK');
  const bangkok = g.byName['Bangkok'];
  for (const u of bangkok.units) if (u.nat === 'THA') u.hold = true;
  const thai = addCorps(g, 'THA', 'a test garrison (Bangkok)', 'inf', 2, 'Bangkok', { xp: 0.2, hold: held });
  const burma = g.corps.find((c) => c.nat === 'UK' && c.name === BURMA)!;
  return { g, burma, thai };
};
const at = (g: Game, c: Corps): string => g.regions[c.reg].name;

describe("a staff's defence reads a threat as its front sums do", () => {
  it('the ground: Mergui lies beside Bangkok, and nobody else of the enemy stands beside Burma', () => {
    const { g, burma, thai } = war(true);
    expect(at(g, burma)).toBe('Rangoon');
    expect(g.byName['Mergui'].nb).toContain(g.byName['Bangkok'].id);
    expect(hostileUnits(g, g.byName['Bangkok'], 'UK')).toContain(thai);
    for (const nm of ['Kra', 'Kanchanaburi', 'Raheng', 'Chiang Mai', 'Paknampo']) expect(hostileUnits(g, g.byName[nm], 'UK'), nm).toEqual([]);
    expect(hostileUnits(g, g.byName['Bangkok'], 'UK').every((u) => u.hold)).toBe(true);
  });

  it("held garrisons next door send nobody: Burma's army stays at Rangoon", () => {
    const { g, burma } = war(true);
    runAI(g, 'UK');
    expect(at(g, burma)).toBe('Rangoon');
  });

  it('a free corps next door is a threat as before: the reserve goes to meet it', () => {
    const { g, burma } = war(false);
    runAI(g, 'UK');
    expect(at(g, burma)).not.toBe('Rangoon');
    expect(['Moulmein', 'Mergui']).toContain(at(g, burma));
  });
});
