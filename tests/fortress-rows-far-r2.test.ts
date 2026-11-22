// The fortress rows on the far side (release 2; the design director's 07:12Z of 22 November 2026, b418): Corregidor,
// the far world's "Harbor Defenses of Manila and Subic Bays" at Bataan, a fortified port overseas. The historian's one
// month is the place's whole stores: the port feeds from the row's stores in place of its own two, and the crew's
// clock runs from the month the sea closed. This file turns the far side on for itself (src/campaign/stage.ts).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { crewStores } from '../src/campaign/fortrows';
import { DESERT, addCorps, assess, computeSupply, createGame, onPortStores, portKeep, removeCorps, setWar, startPhase } from '../src/campaign/game';
import { SIEGE, breach, joinTown } from '../src/campaign/siege';
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

describe('Corregidor: the storm (07:36Z)', () => {
  it('after a long bombardment, its crew behind works broken to the most can be stormed', () => {
    const g = open(true);
    setWar(g, 'JAP', 'USA');
    const crew = g.corps.find((c) => !c.dead && c.nat === 'USA' && c.name === NAME)!;
    const bataan = g.regions[crew.reg];
    for (const c of [...bataan.units]) if (c !== crew) removeCorps(g, c);
    for (const c of [...(bataan.garr ?? [])]) if (c !== crew) removeCorps(g, c);
    if (crew.town === undefined) { bataan.units.splice(bataan.units.indexOf(crew), 1); joinTown(g, bataan, crew); }
    const force = [addCorps(g, 'JAP', 'Storm Force', 'inf', 4, bataan.id, {}), addCorps(g, 'JAP', 'Second Storm Force', 'inf', 4, bataan.id, {})];
    const fresh = assess(g, force, bataan, { town: true });
    expect(fresh.lines.some((l) => l.label === 'Fortress crew')).toBe(true);
    breach(g, bataan, 100);
    const broken = assess(g, force, bataan, { town: true });
    expect(broken.odds).toBeGreaterThan(fresh.odds);
    expect(broken.odds).toBeGreaterThanOrEqual(SIEGE.ai.assault);
  });
});

