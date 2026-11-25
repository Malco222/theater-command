// A lane counts in a staff's choice only while it carries something laid (release 2; the design director's ruling of
// 12:39Z, 25 November 2026, Pacific batch 3, A(2)): oil, supplies, oilfields, or a convoy route of its id. "A 0 that
// stands for 'not read' is not a read zero": a lane laid with nothing counted yet draws no staff's boats and binds no
// staff's escort, though it stays on the pages and open to the player. One predicate (navy.ts cargoLaid), read by every
// staff's count of lanes by sea. Played on the whole-world chart, Japan at war with America.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import { createGame, declareWar } from '../src/campaign/game';
import { escortLanes } from '../src/campaign/fleets';
import { NAVY, fleetReach, laneCarries, laneCounts, lanesTo, navalAI, seaRoutes, shippingIn, whyNotEscort, zoneOf } from '../src/campaign/navy';
import { RES } from '../src/campaign/resources';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const war = (): Game => { const g = startOpen(createGame(3, null)); declareWar(g, 'JAP', 'USA'); return g; };
const EMPTY = { id: 'zz-nothing-laid', name: 'the test lane', from: 'the West Coast', to: 'USA', zones: ['hawaii', 'calrun', 'california'], oil: 0, sup: 0, text: 'A lane laid with nothing counted yet, for this test only.' };
/** A game played with the test's empty lane laid beside the chart's own rows. */
function withEmpty<T>(f: (g: Game) => T, laid: Partial<typeof EMPTY> = {}): T {
  (RES.lanes as unknown[]).push({ ...EMPTY, ...laid });
  try { return f(war()); } finally { RES.lanes.splice(RES.lanes.findIndex((l) => l.id === EMPTY.id), 1); }
}

/** Japan's boats out raiding in Hawaiian Waters, where the lane runs, and America's Pacific Fleet free to patrol there. */
function raiders(g: Game): void {
  const subs = g.fleets.find((f) => f.nat === 'JAP' && f.ships.ss > 0)!;
  subs.ships = { bb: 0, cv: 0, ca: 0, dd: 0, ss: subs.ships.ss };
  subs.zone = zoneOf(g, 'hawaii'); subs.stance = 'raid'; subs.station = subs.zone;
  g.fleets.find((f) => f.nat === 'USA' && f.name === 'Pacific Fleet')!.duty = 'patrol';
}

describe('a lane in a staff\'s count by sea', () => {
  it('every lane and convoy route of the whole world laid before the ruling carries something, so none of their counts moves', () => {
    const g = war();
    expect(seaRoutes(g).length).toBeGreaterThan(10);
    expect(seaRoutes(g).filter((r) => !laneCounts(r)).map((r) => r.id)).toEqual([]);
    for (const c of NAVY.convoys) expect(laneCarries(g, c.id), c.id).toBe(true);
    // Japan's southern oil lane carries the oilfields' yield, with no share of need of its own.
    const south = RES.lanes.find((l) => l.id === 'south-oil');
    if (south) { expect([south.oil, south.sup]).toEqual([0, 0]); expect(laneCarries(g, 'south-oil')).toBe(true); }
  });

  it('counts a lane laid with nothing for no staff: no boat is drawn to its seas, no beat counts it', () => {
    const base = war();
    withEmpty((g) => {
      expect(seaRoutes(g).some((r) => r.id === EMPTY.id)).toBe(true);
      expect(laneCarries(g, EMPTY.id)).toBe(false);
      for (const k of EMPTY.zones) {
        const z = zoneOf(g, k);
        expect(z).toBeGreaterThanOrEqual(0);
        expect(shippingIn(g, 'JAP', z), k).toBe(shippingIn(base, 'JAP', z));
        expect(lanesTo(g, 'USA', z), k).toBe(lanesTo(base, 'USA', z));
      }
    });
  });

  it('binds no computer fleet to it as an escort, though raiders are out on its seas, and the staffs\' month is the same as without it', () => {
    const run = (g: Game): string => { raiders(g); navalAI(g, 'USA'); return JSON.stringify(g.fleets.filter((f) => f.nat === 'USA').map((f) => [f.name, f.stance, f.zone, f.lane ?? null])); };
    const without = run(war());
    withEmpty((g) => {
      expect(run(g)).toBe(without);
      expect(g.fleets.some((f) => f.lane === EMPTY.id)).toBe(false);
    });
  });

  it('once a cargo is laid on it, it counts by the same rule with nothing else to change', () => {
    const base = war();
    withEmpty((g) => {
      expect(laneCarries(g, EMPTY.id)).toBe(true);
      expect(shippingIn(g, 'JAP', zoneOf(g, 'hawaii'))).toBe(shippingIn(base, 'JAP', zoneOf(base, 'hawaii')) + 1);
      raiders(g);
      navalAI(g, 'USA');
      expect(g.fleets.find((f) => f.nat === 'USA' && f.name === 'Pacific Fleet')!.lane).toBe(EMPTY.id);
    }, { oil: 0.1 });
  });

  it('stays on the pages and open to the player: a fleet in reach may escort it', () => {
    withEmpty((g) => {
      expect(escortLanes(g, 'USA').some((l) => l.id === EMPTY.id)).toBe(true);
      const hawaii = zoneOf(g, 'hawaii');
      const f = g.fleets.find((x) => x.nat === 'USA' && x.ships && fleetReach(g, x).includes(hawaii))!;
      expect(f).toBeTruthy();
      expect(whyNotEscort(g, f, EMPTY.id)).toBeNull();
    });
  });
});
