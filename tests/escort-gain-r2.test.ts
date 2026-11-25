// What a fleet's escort of a convoy route is worth to its admirals (release 2, for the general's fleetWithArmy; the
// design director's ruling of 25 November 2026 05:31Z on naval-battles/convoy-r2/escort-gain-form.md, (a) on all five
// questions). escortGain prices the route's month twice as the rules price it, with the fleet off the route and with it
// bound as the route's full escort, and gives the share of the cargoes saved, the tons and the admirals' points (200 x
// the route's share x dL by the battleship, marked G, with 300 x by the destroyer beside it). It throws no dice and
// changes nothing, and nothing in the game calls it.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { CARRIERS } from '../src/campaign/air';
import { NAVY, convoyMeeting, escort, escortGain, sail, whyNotEscort, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game, Ships } from '../src/campaign/types';

const atlantic = NAVY.convoys.find((r) => r.id === 'atlantic')!;

/** A grand campaign at its start, with Units decide on or off and no weather, its fleets where its opening puts them. */
function open(on: boolean, seed = 1): Game {
  const g = startOpen(createGame(seed, null));
  if (on) g.flags.unitsDecide = 1; else delete g.flags.unitsDecide;
  delete g.flags.weather;
  return g;
}
const none = (): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 });
const named = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;
/** A pack of German boats raiding in the North Atlantic beside whatever the opening put there. */
function pack(g: Game, ss: number): Fleet {
  const f = g.fleets.find((x) => x.nat === 'GER' && x.ships.ss > 0)!;
  f.ships = { ...none(), ss };
  f.zone = zoneOf(g, 'natl');
  f.stance = 'raid';
  delete f.lane;
  f.ready = 1;
  return f;
}
/** The card's month on the Atlantic route on a copy of the game, after doing something to the fleet there. */
function cardAfter(g: Game, f: Fleet, act: (c: Game, cf: Fleet) => boolean): ReturnType<typeof convoyMeeting> {
  const c = loadGame(saveGame(g))!;
  const cf = c.fleets.find((x) => x.id === f.id)!;
  expect(act(c, cf)).toBe(true);
  return convoyMeeting(c, 'UK', atlantic);
}
// (The Home Fleet patrols the North Sea and carries no lane at the opening; the Western Approaches Fleet is already
// bound to the Atlantic route; the Dover Fleet patrols the Channel.)
const FLEETS = ['Home Fleet', 'Western Approaches Fleet', 'Dover Fleet'];

describe('the two months are the card\'s own (priced once)', () => {
  for (const on of [true, false]) {
    it(`with Units decide ${on ? 'on' : 'off'}: dL and the tons are the card's month with the fleet sent home less its month with the fleet really bound by escort`, () => {
      const g = open(on);
      pack(g, 30);
      for (const name of FLEETS) {
        const f = named(g, name);
        const r = escortGain(g, f, 'atlantic')!;
        expect(r).not.toBeNull();
        const bound = cardAfter(g, f, (c, cf) => escort(c, cf, 'atlantic'));
        const home = cardAfter(g, f, (c, cf) => sail(c, cf, cf.home, 'port'));
        // (The same two months to the bit: nothing priced beside them.)
        expect(r.dL).toBe(home.L - bound.L);
        // (The card rounds each month's tons to the thousand; the measure does not.)
        expect(Math.abs(r.tons - (home.tons - bound.tons))).toBeLessThanOrEqual(1000);
        expect(r.dL).toBeGreaterThan(0);
        expect(r.tons).toBeGreaterThan(0);
      }
    });
  }

  it('a fleet already bound to the route in the sea escort chooses prices the bound month as the game stands', () => {
    const g = open(true);
    pack(g, 30);
    const f = named(g, 'Western Approaches Fleet');
    const now = convoyMeeting(g, 'UK', atlantic);
    const bound = cardAfter(g, f, (c, cf) => escort(c, cf, 'atlantic'));
    // (escort leaves it where it is, so the card as it stands is the bound month.)
    expect(bound.L).toBe(now.L);
    const home = cardAfter(g, f, (c, cf) => sail(c, cf, cf.home, 'port'));
    expect(escortGain(g, f, 'atlantic')!.dL).toBe(home.L - now.L);
  });
});

describe('it changes nothing', () => {
  for (const on of [true, false]) {
    it(`with Units decide ${on ? 'on' : 'off'}: the save is the same to the byte after a call, the fleet's fields in the same order, and a second call gives the same answer`, () => {
      const g = open(on);
      pack(g, 30);
      // (A fleet bound to the route that was struck from the air since: in play a field can follow its lane, and the
      // save writes the fields in their order.)
      named(g, 'Western Approaches Fleet').struck = [g.turn, 1, zoneOf(g, 'wapp')];
      for (const name of FLEETS) {
        const f = named(g, name);
        const before = saveGame(g), keys = Object.keys(f).join(), dirty = g.supplyDirty;
        const first = escortGain(g, f, 'atlantic');
        expect(saveGame(g)).toBe(before);
        expect(Object.keys(f).join()).toBe(keys);
        expect(g.supplyDirty).toBe(dirty);
        expect(escortGain(g, f, 'atlantic')).toEqual(first);
        expect(saveGame(g)).toBe(before);
      }
    });
  }

  it('the fleet is put back even when a month it prices fails part-way', () => {
    const g = open(true);
    const f = named(g, 'Home Fleet');
    const boats = pack(g, 30), kept = boats.ships;
    const before = saveGame(g);
    // (A raider whose ships cannot be read while the Home Fleet is placed off the route, then while it is bound to it:
    // routeForces throws in the middle of the first month, then of the second.)
    for (const placed of [(): boolean => f.stance === 'port', (): boolean => f.stance === 'escort' && f.lane === 'atlantic']) {
      Object.defineProperty(boats, 'ships', { get: () => { if (placed()) throw new Error('unreadable'); return kept; }, configurable: true });
      expect(() => escortGain(g, f, 'atlantic')).toThrow('unreadable');
      Object.defineProperty(boats, 'ships', { value: kept, writable: true, enumerable: true, configurable: true });
      expect(f.stance).toBe('patrol');
      expect(f.zone).toBe(zoneOf(g, 'nsea'));
      expect('lane' in f).toBe(false);
      expect(saveGame(g)).toBe(before);
    }
  });
});

describe('which fleets and routes', () => {
  it('null where the fleet cannot escort the route, or the route is no convoy route, or its nation is not at war', () => {
    const g = open(true);
    const home = named(g, 'Home Fleet');
    // (No ship that guards a convoy: battleships and submarines alone.)
    const heavy = named(g, 'Dover Fleet');
    heavy.ships = { ...none(), bb: 2, ss: 4 };
    expect(whyNotEscort(g, heavy, 'atlantic')).not.toBeNull();
    expect(escortGain(g, heavy, 'atlantic')).toBeNull();
    // (An enemy's fleet: the route feeds no ally of it.)
    const ger = g.fleets.find((f) => f.nat === 'GER' && f.ships.dd > 0)!;
    expect(whyNotEscort(g, ger, 'atlantic')).not.toBeNull();
    expect(escortGain(g, ger, 'atlantic')).toBeNull();
    // (No such route; and a lane of resources.json alone, which laneLoss prices, not this.)
    expect(escortGain(g, home, 'nowhere')).toBeNull();
    expect(escortGain(g, home, 'french')).toBeNull();
    // (Italy is not at war at the opening: nothing sails against its convoys to Libya.)
    expect(g.nations.ITA.atWar.length).toBe(0);
    const ita = g.fleets.find((f) => f.nat === 'ITA' && f.ships.dd > 0)!;
    expect(escortGain(g, ita, 'libya')).toBeNull();
  });

  for (const on of [true, false]) {
    it(`with Units decide ${on ? 'on' : 'off'}: 0 or more for every fleet that can escort a route, a number for each`, () => {
      const g = open(on);
      let n = 0;
      for (const f of g.fleets) for (const r of NAVY.convoys) {
        const x = escortGain(g, f, r.id);
        expect(x === null).toBe(whyNotEscort(g, f, r.id) !== null || !g.nations[r.to].atWar.length);
        if (!x) continue;
        n++;
        for (const v of [x.dL, x.tons, x.points, x.pointsByDestroyer]) expect(Number.isFinite(v)).toBe(true);
        expect(x.dL).toBeGreaterThanOrEqual(0);
        expect(x.tons).toBeGreaterThanOrEqual(0);
      }
      expect(n).toBeGreaterThan(0);
    });
  }

  for (const on of [true, false]) {
    it(`with Units decide ${on ? 'on' : 'off'}: more destroyers on the fleet never give less dL`, () => {
      const g = open(on);
      pack(g, 40);
      const f = named(g, 'Dover Fleet');
      let last = -1;
      for (const dd of [1, 2, 4, 8, 16, 32, 64, 128]) {
        f.ships = { ...none(), dd };
        const x = escortGain(g, f, 'atlantic')!;
        expect(x.dL).toBeGreaterThanOrEqual(last);
        last = x.dL;
      }
      expect(last).toBeGreaterThan(0);
    });
  }
});

describe('the points (Q5 (a), marked G; (b) beside it)', () => {
  it('a battleship is 15 times its fleet power on the strike scale and a destroyer 10 times', () => {
    expect(CARRIERS.strike.value.bb / NAVY.classes.bb.power).toBe(15);
    expect(CARRIERS.strike.value.dd / NAVY.classes.dd.power).toBe(10);
  });

  for (const on of [true, false]) {
    it(`with Units decide ${on ? 'on' : 'off'}: points = 200 x the route's share x dL, and by the destroyer 300 x`, () => {
      const g = open(on);
      pack(g, 30);
      for (const name of FLEETS) {
        const x = escortGain(g, named(g, name), 'atlantic')!;
        expect(x.points).toBeCloseTo(200 * atlantic.share * x.dL, 12);
        expect(x.pointsByDestroyer).toBeCloseTo(300 * atlantic.share * x.dL, 12);
        expect(x.points).toBeGreaterThan(0);
      }
    });
  }
});
