// The next night's contact (release 2, Units decide; the design director's rulings of 24 November 2026 18:00Z and
// 18:03Z on the load form's Q2 (a)). A boat that fired on a convoy one night comes again the next with the share that
// hold or regain contact through the day: the margin of its surface speed over the convoy's (1 - convoy / boat, a D form
// with no figure), weighed over the route's convoys by class, x the time the aircraft that keep boats down by day leave
// it up (the sea patrol as built), x the calm share (a rough sea breaks contact). The second run within one night stays
// unread. A further approach meets the screen again: q = the firing share x (w + (1 - w) h), never past the load.
import { afterEach, describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { CAUSE, NAVY, calmShare, convoySideOf, nextNight, nightFits, salvosInto, torpedoesInto, zoneOf } from '../src/campaign/navy';
import { setMission } from '../src/campaign/air';
import { watchAgainst } from '../src/campaign/seapatrol';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game, Ships } from '../src/campaign/types';

const atlantic = NAVY.convoys.find((r) => r.id === 'atlantic')!;
const NEXT = structuredClone(CAUSE.load!.next!);
const PATROL = structuredClone(CAUSE.patrol);
const BOAT = 17.85;
/** The Atlantic's margin as laid: half the sailings each way, eastbound 0.37 slow (7 knots) and westbound 0.5, the rest fast (11). */
const MARGIN = (0.5 * 0.37 + 0.5 * 0.5) * (1 - 7 / BOAT) + (0.5 * 0.63 + 0.5 * 0.5) * (1 - 11 / BOAT);

function open(on = true, seed = 1): Game {
  const g = startOpen(createGame(seed, null));
  if (on) g.flags.unitsDecide = 1; else delete g.flags.unitsDecide;
  delete g.flags.weather;
  for (const f of g.fleets) f.stance = 'port';
  return g;
}
const none = (): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 });
function sea(g: Game, name: string, zone: string, ships: Partial<Ships>, stance: Fleet['stance'], lane?: string): Fleet {
  const f = g.fleets.find((x) => x.name === name)!;
  f.ships = { ...none(), ...ships };
  f.zone = zoneOf(g, zone);
  f.stance = stance;
  if (lane) f.lane = lane; else delete f.lane;
  f.ready = 1;
  return f;
}
function meet(g: Game, ss: number, dd: number): Fleet {
  sea(g, 'Home Fleet', 'wapp', { dd }, 'escort', 'atlantic');
  return sea(g, 'U-boat Fleet', 'natl', { ss }, 'raid');
}
/** The sea patrol laid, with the tests' own figures (as tests/sea-patrol-r2 lays them), and live where the switch exists. */
function layPatrol(): void {
  Object.assign(CAUSE.patrol, { reach: 'map', a0: 50, intensity: 0.5, attacks: { kit: 0.6, none: 0.3 }, kill: { kit: 0.1, none: 0.04 }, lost: null });
  const P = CAUSE.patrol as unknown as Record<string, unknown>;
  if ('live' in P) P.live = true;
}

afterEach(() => {
  CAUSE.load!.next = structuredClone(NEXT);
  Object.assign(CAUSE.patrol, structuredClone(PATROL));
  expect(CAUSE.load!.regain).toBeNull();
});

describe('the figures', () => {
  it('lays the VIIC\'s 17.85 knots, the Atlantic\'s classes at 7 and 11 knots by their share of the sailings each way, and Libya\'s convoys at 9', () => {
    expect(CAUSE.load!.next).toEqual({
      boat: 17.85,
      convoys: {
        atlantic: [
          { class: 'eastbound slow: SC', speed: 7, share: 0.185 },
          { class: 'eastbound fast: HX', speed: 11, share: 0.315 },
          { class: 'westbound slow: slow ON, and ONS', speed: 7, share: 0.25 },
          { class: 'westbound fast: fast ON', speed: 11, share: 0.25 },
        ],
        libya: [{ class: 'Italian convoys to Libya', speed: 9, share: 1 }],
      },
    });
    // (Half the sailings each way: eastbound 0.37 slow, westbound 0.5.)
    const A = CAUSE.load!.next!.convoys.atlantic!;
    expect(A[0].share! + A[1].share!).toBeCloseTo(0.5, 12);
    expect(A[0].share! / (A[0].share! + A[1].share!)).toBeCloseTo(0.37, 12);
    expect(A[2].share! / (A[2].share! + A[3].share!)).toBeCloseTo(0.5, 12);
    // (The second run within one night stays unread.)
    expect(CAUSE.load!.regain).toBeNull();
  });
});

describe('the share that come again the next night', () => {
  it('is the speed margin x the calm share in every sea of the route, while no aircraft watch by day', () => {
    const g = open();
    for (const k of atlantic.zones) {
      const z = zoneOf(g, k);
      expect(nextNight(g, z, 'atlantic', 'GER')).toBeCloseTo(MARGIN * calmShare(g, z), 12);
    }
    expect(MARGIN).toBeCloseTo(0.481, 3);
  });

  it('is none against a convoy as fast as the boat, or faster: no class gives a negative margin', () => {
    const g = open(), z = zoneOf(g, 'natl');
    CAUSE.load!.next!.convoys.atlantic = [{ class: 'as fast', speed: BOAT, share: 1 }];
    expect(nextNight(g, z, 'atlantic', 'GER')).toBe(0);
    CAUSE.load!.next!.convoys.atlantic = [{ class: 'faster', speed: 25, share: 0.5 }, { class: 'slow', speed: 7, share: 0.5 }];
    expect(nextNight(g, z, 'atlantic', 'GER')).toBeCloseTo((1 - 7 / BOAT) * 0.5 * calmShare(g, z), 12);
  });

  it('Libya\'s convoys at 9 knots: the same rule, whoever\'s boats they are', () => {
    const g = open(), central = zoneOf(g, 'central');
    expect(nextNight(g, central, 'libya', 'UK')).toBeCloseTo((1 - 9 / BOAT) * calmShare(g, central), 12);
  });

  it('is none where a speed or share is unread, or the route has none laid', () => {
    const g = open(), z = zoneOf(g, 'natl'), central = zoneOf(g, 'central');
    CAUSE.load!.next!.convoys.libya = null;
    expect(nextNight(g, central, 'libya', 'UK')).toBe(0);
    CAUSE.load!.next = structuredClone(NEXT);
    CAUSE.load!.next!.boat = null;
    expect(nextNight(g, z, 'atlantic', 'GER')).toBe(0);
    CAUSE.load!.next = structuredClone(NEXT);
    CAUSE.load!.next!.convoys.atlantic![1].speed = null;
    expect(nextNight(g, z, 'atlantic', 'GER')).toBe(0);
    CAUSE.load!.next = structuredClone(NEXT);
    CAUSE.load!.next!.convoys.atlantic![0].share = null;
    expect(nextNight(g, z, 'atlantic', 'GER')).toBe(0);
  });

  it('is cut by the aircraft that keep boats down by day, as the sea patrol keeps them down (one cause, acting once)', () => {
    const g = open(), wapp = zoneOf(g, 'wapp');
    const clear = nextNight(g, wapp, 'atlantic', 'GER');
    layPatrol();
    setMission(g, g.airFleets.find((f) => f.name === 'No. 1 Group')!, 'seapatrol', wapp);
    const w = watchAgainst(g, wapp, 'GER')!;
    expect(w.none).toBeGreaterThan(0);
    const h = nextNight(g, wapp, 'atlantic', 'GER');
    expect(h).toBeCloseTo(clear * (1 - w.none), 12);
    expect(h).toBeLessThan(clear);
  });
});

describe('the torpedoes a firing boat puts in', () => {
  it('a further salvo the next night at q = the firing share x h, never past the load', () => {
    const g = open(), z = zoneOf(g, 'natl'), at = { route: 'atlantic', nat: 'GER' };
    const load = CAUSE.ceiling.inside! + calmShare(g, z) * (CAUSE.ceiling.outside ?? 0);
    const h = nextNight(g, z, 'atlantic', 'GER');
    expect(h).toBeGreaterThan(0);
    for (const fire of [1, 0.66, 0.31]) {
      const t = torpedoesInto(g, z, fire, at);
      expect(t).toBeCloseTo(5 * salvosInto(fire * h, load / 5), 12);
      expect(t).toBeGreaterThan(5);
      expect(t).toBeLessThanOrEqual(load + 1e-9);
    }
    // (Without the route it is one approach, as the load form first laid it.)
    expect(torpedoesInto(g, z, 1)).toBe(5);
  });

  it('with the night\'s own second run laid too, a further approach is that run or else the next night: q = f (w + (1 - w) h)', () => {
    const g = open(), z = zoneOf(g, 'natl'), at = { route: 'atlantic', nat: 'GER' };
    const load = CAUSE.ceiling.inside! + calmShare(g, z) * (CAUSE.ceiling.outside ?? 0);
    const h = nextNight(g, z, 'atlantic', 'GER');
    const keep = CAUSE.load!.regain;
    CAUSE.load!.regain = 0.5;
    try {
      const w = nightFits(g, z) * 0.5;
      expect(w).toBeGreaterThan(0);
      for (const fire of [1, 0.66, 0.31]) {
        const t = torpedoesInto(g, z, fire, at);
        expect(t).toBeCloseTo(5 * salvosInto(fire * (w + (1 - w) * h), load / 5), 12);
        expect(t).toBeGreaterThan(torpedoesInto(g, z, fire));
        expect(t).toBeLessThanOrEqual(load + 1e-9);
      }
    } finally { CAUSE.load!.regain = keep; }
  });

  it('on the route, the boats on station put in more than one salvo each where the speeds are laid, and one where they are not', () => {
    const g = open();
    meet(g, 30, 10);
    const laid = convoySideOf(g, 'UK', atlantic)!;
    CAUSE.load!.next!.boat = null;
    const unread = convoySideOf(g, 'UK', atlantic)!;
    expect(laid.torps!).toBeGreaterThan(unread.torps!);
    expect(laid.sunk).toBeGreaterThanOrEqual(unread.sunk);
  });
});

describe('off', () => {
  it('with Units decide off the route reads none of it', () => {
    const g = open(false);
    meet(g, 30, 10);
    expect(convoySideOf(g, 'UK', atlantic)).toBeUndefined();
  });
});
