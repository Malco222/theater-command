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
import { LANE_NO_STAFF, LANE_UNCOUNTED, NAVY, fleetReach, laneCarries, laneCounts, laneNear, lanesTo, navalAI, seaRoutes, shippingIn, whyNotEscort, zoneOf } from '../src/campaign/navy';
import { RES } from '../src/campaign/resources';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { humanUp, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

const war = (): Game => { const g = startOpen(createGame(3, null)); declareWar(g, 'JAP', 'USA'); return g; };
/** The Pacific's three lanes laid after the ruling (the Japan and Pacific thread, batch 3): their stores are not read yet, so they carry nothing, by design. */
const LAID_AFTER = ['america-hawaii', 'america-australia', 'britain-india'];
const EMPTY = { id: 'zz-nothing-laid', name: 'the test lane', from: 'the West Coast', to: 'USA', zones: ['hawaii', 'calrun', 'california'], oil: 0, sup: 0, text: 'A lane laid with nothing counted yet, for this test only.' };
/** A game played with the test's empty lane laid beside the chart's own rows. */
function withEmpty<T>(f: (g: Game) => T, laid: Partial<typeof EMPTY> = {}, make: () => Game = war): T {
  (RES.lanes as unknown[]).push({ ...EMPTY, ...laid });
  try { return f(make()); } finally { RES.lanes.splice(RES.lanes.findIndex((l) => l.id === EMPTY.id), 1); }
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
    expect(seaRoutes(g).filter((r) => !laneCounts(r) && !LAID_AFTER.includes(r.id)).map((r) => r.id)).toEqual([]);
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

  it("the player's fleet page says its shipping is not counted, in place of its losses, and that no staff guards or raids it", () => {
    const asAmerica = (): Game => {
      const g = startOpen(createGame(3, 'USA'));
      for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
      declareWar(g, 'JAP', 'USA');
      return g;
    };
    withEmpty((g) => {
      const f = g.fleets.find((x) => x.nat === 'USA' && x.name === 'Pacific Fleet')!;
      const empty = seaRoutes(g).find((r) => r.id === EMPTY.id)!;
      expect(laneNear(g, f, empty)).toBe(true);
      TC().load(saveGame(g));
      const sec = TC().page(`fleet:${f.id}`).sections.find((x) => x.heading === 'LANES TO ESCORT')!;
      const row = sec.rows.find((r) => r.title === EMPTY.name)!;
      expect(row.lines).toEqual([LANE_UNCOUNTED, LANE_NO_STAFF]);
      expect(row.buttons[0].ok).toBe(true);
      // A lane that carries something reads its losses as before.
      // (With the Pacific's three lanes laid, the Hawaii lane stands in this list too and is not counted either: the
      // row asked here is one whose lane carries something. Mended at the join of the lanes, 25 November 2026.)
      const other = sec.rows.find((r) => r.title !== EMPTY.name && !r.lines.includes(LANE_UNCOUNTED));
      if (other) expect(other.lines[0]).toMatch(/^Cargoes lost on it now: |cut|closed|no port/i);
    }, {}, asAmerica);
  });
});

type PageOut = { sections: { heading: string; rows: { title: string; lines: string[]; buttons: { ok: boolean }[] }[] }[] };
const TC = (): { load: (t: string) => unknown; page: (id: string) => PageOut } => (globalThis as unknown as { TC: { load: (t: string) => unknown; page: (id: string) => PageOut } }).TC;
