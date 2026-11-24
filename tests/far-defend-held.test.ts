// A staff's defence reads a threat as its own front sums do: an enemy corps held in its place is no threat (the
// design director's ruling of 05:55Z, 21 November 2026, on Mergui's trace, 4; ai.ts `defend`). Behind the far switch.
//
// As measured (war 22 at stop 3, Britain's month 30): Mergui stood empty beside Bangkok, where Thailand's 1st
// Division was held as its capital's garrison, and beside the Kra. The reserve rule counted the held garrison in
// full, as staff.ts `fronts` does not, and marched the whole of Burma's army out of Rangoon into Mergui, the end of
// a strip; Moulmein fell behind it the same month. One staff weighing a neighbour two ways is the fault. The states
// are set by hand; one month of Britain's own is played.
//
// Far-side ground only (the design director's ruling of 08:40Z, 22 November 2026, on Tripoli's trace): the reading
// applies where the province defended stands on the far side. The old chart's defence reads with the switch on as it
// does with it off: France's garrison of Tunisia, held at Mareth, is a threat to an empty Tripoli, and Italy's
// reserve rule answers it.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { reserveWatch, runAI } from '../src/campaign/ai';
import { addCorps, createGame, friendlyUnits, hostileUnits, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';
import { shoreOf } from '../src/campaign/world';

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

  it("on the old chart a held enemy next door is a threat as with the switch off: Mareth's garrison draws Italy's reserve to an empty Tripoli", () => {
    const g = startOpen(createGame(3, null));
    g.turn = 12;
    setWar(g, 'ITA', 'FRA');
    const tunisia = g.corps.find((c) => c.nat === 'FRA' && c.name === 'Tunisia Command')!;
    const tripoli = g.byName['Tripoli'], mareth = g.byName['Mareth'];
    // The ground: Tripoli and Mareth stand on the old chart, side by side; the garrison is held; Tripoli is empty.
    expect([shoreOf(tripoli), shoreOf(mareth), tripoli.nb.includes(mareth.id)]).toEqual([0, 0, true]);
    expect([at(g, tunisia), !!tunisia.hold]).toEqual(['Mareth', true]);
    expect(hostileUnits(g, mareth, 'ITA').every((u) => u.hold)).toBe(true);
    expect(friendlyUnits(g, tripoli, 'ITA')).toEqual([]);
    // (Told to whoever is reading the reserve rule: nothing in the game hears it.)
    const sent: string[] = [];
    reserveWatch.tell = (e) => { if (e.nat === 'ITA' && e.to === tripoli && !e.kept) sent.push(e.corps.name); };
    try { runAI(g, 'ITA'); } finally { reserveWatch.tell = null; }
    expect(sent.length).toBeGreaterThan(0);
  });
});
