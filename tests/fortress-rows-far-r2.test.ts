// The fortress rows on the far side (release 2; the design director's 07:12Z of 22 November 2026, b418): Corregidor,
// the far world's "Harbor Defenses of Manila and Subic Bays" at Bataan, a fortified port overseas. The historian's one
// month is the place's whole stores: the port feeds from the row's stores in place of its own two, and the crew's
// clock runs from the month the sea closed. This file turns the far side on for itself (src/campaign/stage.ts).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { crewStores } from '../src/campaign/fortrows';
import { DESERT, computeSupply, createGame, onPortStores, portKeep, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { FAR_STAGE } from '../src/campaign/stage';
import type { Game } from '../src/campaign/types';

function open(on: boolean): Game {
  const g = createGame(3, null);
  if (on) g.flags.fortRows = 1;
  return startOpen(g);
}
const NAME = 'Harbor Defenses of Manila and Subic Bays';

describe('Corregidor: one stores, not two', () => {
  it('the far side is on in this file', () => expect(FAR_STAGE).toBe(true));

  it('its row stands at Bataan, a fortified port overseas, with a month of stores; off, the port keeps its own two', () => {
    for (const on of [false, true]) {
      const g = open(on);
      const crew = g.corps.find((c) => !c.dead && c.nat === 'USA' && c.name === NAME)!;
      expect(crew, `row ${on}`).toBeDefined();
      const bataan = g.regions[crew.reg];
      expect(bataan.name).toBe('Bataan');
      expect(bataan.overseas && !!bataan.port && !!bataan.fort).toBe(true);
      expect(crewStores(crew)).toBe(1);
      expect(portKeep(g, bataan)).toBe(on ? 1 : DESERT.stores);
    }
  });

  it('with the sea shut, the crew lives one month on the place\'s stores and is gone; the port then feeds nobody', () => {
    const g = open(true);
    const crew = g.corps.find((c) => !c.dead && c.nat === 'USA' && c.name === NAME)!;
    const bataan = g.regions[crew.reg];
    computeSupply(g);
    const opened = g.flags[`stores:${bataan.id}`];
    expect(opened).toBe(g.turn);
    for (const n of Object.values(g.nations)) { n.seaSupply = false; n.sealift = 0; }
    g.turn = opened + 1;
    computeSupply(g);
    expect(crew.supplied).toBe(true);
    expect(onPortStores(g, crew)).toBe(true);
    startPhase(g, 'USA');
    expect(crew.dead).toBe(true);
    expect(portKeep(g, bataan)).toBe(DESERT.stores);
  });
});
