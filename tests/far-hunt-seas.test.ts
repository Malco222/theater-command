// The far side's hunt for the enemy's boats (the design director's rulings of 19:44Z and 19:52Z, 22 November 2026).
//
// The fault it mends: nothing but coastal aircraft ever hunted Japan's submarines. Its Advance Submarine Fleet raided
// off Hawaii through 1942 beside the Pacific Fleet's destroyers, which lay in Pearl Harbor because the fleet is laid
// for harbour, and a staff's fleet hunted on its own station and the seas next to it alone. Japan lost 7 to 10 boats
// in a war against the record's 89 to ships and aircraft.
//
// The rule, one for every side:
// (1) A fleet laid for harbour, lying in it, keeps the patrol's share of its destroyers hunting the sea at its own
//     door, as navies kept local patrols on their approaches. It adds no number: the share is the patrol's.
// (2) A staff's fleet with destroyers and nothing better to do hunts wherever the enemy's boats are out within its
//     reach, not on its own beat alone. The staff's fleets are sent one at a time, each to the sea where its hunters
//     would sink the most boats by the hunt's own rule, counting the hunters of its side already there.
// Every state here is set by hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { allied, createGame, declareWar } from '../src/campaign/game';
import { fleetCard } from '../src/campaign/fleets';
import { FLEETS, atSea, boatsOut, fleetReach, harbourPatrol, huntShareSunk, huntSubs, huntersIn, huntingSea, navalAI, sail, seaRoutes, zoneOf } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game, Ships } from '../src/campaign/types';
import { FAR_SEAS_FIRST } from '../src/campaign/world';

const grand = (seed = 3): Game => startOpen(createGame(seed, null));
const named = (g: Game, nat: string, name: string): Fleet => g.fleets.find((x) => x.nat === nat && x.name === name)!;
const war = (g: Game): Game => { declareWar(g, 'JAP', 'USA'); return g; };
const ships = (s: Partial<Ships>): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0, ...s });
/** A fleet of these ships alone, set by hand in one sea. */
const squadron = (g: Game, f: Fleet, s: Partial<Ships>, sea: number, stance: Fleet['stance']): Fleet => {
  f.ships = ships(s);
  f.zone = sea; f.stance = stance; f.ready = 1;
  return f;
};
/** Every fleet of a nation in harbour, by hand. */
const allHome = (g: Game, nat: string): void => { for (const f of g.fleets) if (f.nat === nat) { f.zone = f.home; f.stance = 'port'; } };
/** The Pacific Fleet in its harbour with these ships, and no other American destroyer in that port. */
const inHarbour = (f: Fleet, s: Partial<Ships>): Fleet => { f.ships = ships(s); f.zone = f.home; f.stance = 'port'; f.ready = 1; return f; };
/** The Pacific Fleet in its harbour with these ships, and no other American destroyer in that port. */
const pacific = (g: Game, s: Partial<Ships>): Fleet => {
  named(g, 'USA', 'Pacific Carrier Fleet').ships.dd = 0;
  return inHarbour(named(g, 'USA', 'Pacific Fleet'), s);
};

describe("the far side: a fleet laid for harbour patrols the sea at its own door", () => {
  it('a quarter of its destroyers hunt there, the patrol\'s share, and its carriers do not', () => {
    const g = war(grand());
    const f = pacific(g, { bb: 8, cv: 2, dd: 20 });
    expect(f.duty).toBe('port');
    expect(atSea(f)).toBe(false);
    expect(harbourPatrol(g, f)).toBe(true);
    const z = f.home;
    const air = huntersIn(g, 'USA', z, f).weight;
    const h = huntersIn(g, 'USA', z);
    expect(h.fleets).toEqual([f]);
    expect(h.weight).toBeGreaterThan(air);
    // The same destroyers at sea on patrol in that sea, with no carrier, weigh the same: the share is the patrol's.
    squadron(g, f, { bb: 8, dd: 20 }, z, 'patrol');
    expect(huntersIn(g, 'USA', z).weight).toBeCloseTo(h.weight, 9);
    // And a quarter of what they weigh sent out to hunt.
    f.stance = 'hunt';
    expect(FLEETS.hunt.loose).toBe(0.25);
    expect(h.weight - air).toBeCloseTo((huntersIn(g, 'USA', z).weight - air) * FLEETS.hunt.loose, 9);
  });

  it('only in the sea its port stands on, only a fleet laid for harbour, only with destroyers, and never with the far side off', () => {
    const g = war(grand());
    const f = pacific(g, { bb: 8, dd: 20 });
    const z = f.home;
    for (const x of g.seas[z].adj) expect(huntersIn(g, 'USA', x).fleets).not.toContain(f);
    // No destroyers, no patrols.
    f.ships.dd = 0;
    expect(harbourPatrol(g, f)).toBe(false);
    expect(huntersIn(g, 'USA', z).fleets).toEqual([]);
    f.ships.dd = 20;
    // A fleet laid for patrol that lies in harbour is refitting, and sends nothing out.
    const a = inHarbour(named(g, 'USA', 'Asiatic Fleet'), { dd: 12 });
    expect(a.duty).toBe('patrol');
    expect(harbourPatrol(g, a)).toBe(false);
    expect(huntersIn(g, 'USA', a.home).fleets).toEqual([]);
    // The far side off: nobody's harbour patrols are out.
    g.flags.world = 0;
    try {
      expect(harbourPatrol(g, f)).toBe(false);
      expect(huntersIn(g, 'USA', z).fleets).toEqual([]);
    } finally { g.flags.world = 1; }
    expect(huntersIn(g, 'USA', z).fleets).toEqual([f]);
  });

  it('they sink the boats that raid off its port, the news names them, and the fleet\'s card says so', () => {
    const g = war(grand());
    const f = pacific(g, { bb: 8, dd: 120 });
    const z = f.home;
    allHome(g, 'JAP');
    const boats = squadron(g, named(g, 'JAP', '6th Fleet'), { ss: 90 }, z, 'raid');
    const out = boatsOut(g, boats);
    const want = out * huntShareSunk(huntersIn(g, 'USA', z).weight, out);
    expect(want).toBeGreaterThan(1);
    const sunk = huntSubs(g, 'USA')[z] ?? 0;
    expect(sunk).toBeGreaterThanOrEqual(Math.floor(want));
    expect(sunk).toBeLessThanOrEqual(Math.ceil(want));
    expect(boats.lost.ss).toBe(sunk);
    // The harbour patrols lose no destroyer for it: only a group sent out to hunt pays that.
    expect(f.ships.dd).toBe(120);
    expect(sunk).toBeGreaterThan(1);
    expect(g.headlines.some((h) => h.text.startsWith(`The harbour patrols of the Pacific Fleet sink ${sunk} Japanese submarines `))).toBe(true);
    expect(fleetCard(g, f).harbour).toBe(0.25);
    // At sea the card has no such line.
    squadron(g, f, { bb: 8, dd: 120 }, z, 'patrol');
    expect(fleetCard(g, f).harbour).toBeUndefined();
  });

  it('Japan\'s fleets laid for harbour do the same at theirs, and Germany\'s Battle Fleet is the old chart\'s one', () => {
    const g = war(grand());
    const laid = g.fleets.filter((f) => f.duty === 'port' && f.ships.dd > 0).map((f) => `${f.nat} ${f.name}`).sort();
    expect(laid).toEqual(['GER Battle Fleet', 'JAP Combined Fleet', 'JAP Mobile Fleet', 'USA Pacific Carrier Fleet', 'USA Pacific Fleet']);
    const c = inHarbour(named(g, 'JAP', 'Combined Fleet'), { bb: 6, dd: 24 });
    expect(huntersIn(g, 'JAP', c.home).fleets).toContain(c);
  });
});

describe("the far side: a staff's fleet hunts wherever the enemy's boats are out within its reach", () => {
  /** The Asiatic Fleet free to hunt, Japan's fleets in harbour, and two seas in its reach beyond its own beat that lie on nobody's shipping lane. */
  const setting = (): { g: Game; f: Fleet; a: number; b: number; boats: Fleet; more: Fleet } => {
    const g = war(grand());
    allHome(g, 'JAP');
    const f = named(g, 'USA', 'Asiatic Fleet');
    f.ships = ships({ ca: 2, dd: 12 }); f.ready = 1;
    const beat = new Set([f.station, ...g.seas[f.station].adj]);
    const lanes = new Set(seaRoutes(g).flatMap((r) => r.zones));
    const far = fleetReach(g, f).filter((z) => z >= FAR_SEAS_FIRST && !beat.has(z) && !lanes.has(z));
    expect(far.length).toBeGreaterThanOrEqual(2);
    const [a, b] = far;
    const boats = squadron(g, named(g, 'JAP', '6th Fleet'), { ss: 90 }, a, 'raid');
    const more = squadron(g, named(g, 'JAP', '4th Fleet'), { ss: 36 }, b, 'raid');
    return { g, f, a, b, boats, more };
  };

  it('it goes to the sea where its hunters would sink the most boats, beyond its own beat', () => {
    const { g, f, a, b } = setting();
    const reach = fleetReach(g, f);
    expect(g.seas[f.station].adj).not.toContain(a);
    expect(huntingSea(g, f, reach)).toBe(a);
    // The staff sends it there.
    navalAI(g, 'USA');
    expect(f.stance).toBe('hunt');
    expect(f.zone).toBe(a);
    // With no boats out in the first sea it goes to the second.
    expect(huntingSea(g, f, reach.filter((z) => z !== a))).toBe(b);
  });

  it('hunters enough already there send it on: the staff\'s fleets share the seas, and an ally\'s hunters count', () => {
    const { g, f, a, b } = setting();
    const reach = fleetReach(g, f);
    // A strong group of its own navy already hunts the first sea.
    const other = squadron(g, named(g, 'USA', 'Pacific Fleet'), { dd: 150 }, a, 'hunt');
    expect(huntingSea(g, f, reach)).toBe(b);
    // The fleet's own weight is never counted against the sea it is in.
    inHarbour(other, { dd: 150 });
    expect(sail(g, f, a, 'hunt')).toBe(true);
    expect(huntingSea(g, f, reach)).toBe(a);
    // An ally's hunters count as its own navy's do.
    const ally = named(g, 'UK', 'China Fleet');
    declareWar(g, 'JAP', 'UK');
    squadron(g, ally, { dd: 150 }, a, 'hunt');
    // (Britain no ally of America's yet: its hunters are nothing to the American staff.)
    expect(allied(g, 'UK', 'USA')).toBe(false);
    expect(huntingSea(g, f, reach)).toBe(a);
    g.nations.USA.align = 'allies';
    expect(allied(g, 'UK', 'USA')).toBe(true);
    expect(huntingSea(g, f, reach)).toBe(b);
  });

  it('a sea where enemy surface ships are out is no hunting ground, nor a sea with no boats; a carrier\'s aircraft hunt as destroyers do', () => {
    const { g, f, a, b, more } = setting();
    const reach = fleetReach(g, f);
    squadron(g, named(g, 'JAP', '2nd Fleet'), { ca: 4, dd: 8 }, a, 'patrol');
    expect(huntingSea(g, f, reach)).toBe(b);
    more.stance = 'port'; more.zone = more.home;
    expect(huntingSea(g, f, reach)).toBe(-1);
    more.stance = 'raid'; more.zone = b;
    // Cruisers alone are no hunters: the fleet has no hunting ground.
    f.ships.dd = 0;
    expect(huntingSea(g, f, reach)).toBe(-1);
    // A fleet of carriers with no destroyer is one the hunt counts, by its aircraft: it goes (the director's 20:03Z).
    f.ships.cv = 3;
    expect(huntingSea(g, f, reach)).toBe(b);
    navalAI(g, 'USA');
    expect(f.stance).toBe('hunt');
    expect(f.zone).toBe(b);
    expect(huntersIn(g, 'USA', b).fleets).toContain(f);
  });
});
