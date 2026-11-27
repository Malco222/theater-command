// Japan's submarines on the far side: how many it has, and where they go (the design director's rulings of 19:44Z,
// 20:03Z and 20:04Z, 22 November 2026; the historian's japan-subs-building-and-duty-1943-45.md).
//
// The fault it mends: Japan lost 7 to 10 boats in a war against the record's 89 to ships and aircraft. Two of the
// four causes are its own rows. It began the war with 55 boats against the record's 63 and built none, where the
// record completed 111 ocean-going boats in the war. Every state here is set by hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { allied, atWar, createGame, declareWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { NAVY, baseInForce, fleetBeat, fleetReach, navalUpkeep, placeBoats, zoneOf } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game, Ships } from '../src/campaign/types';
import farBooks from '../data/campaign/far-books.json';

const grand = (seed = 3): Game => startOpen(createGame(seed, null));
const named = (g: Game, nat: string, name: string): Fleet => g.fleets.find((x) => x.nat === nat && x.name === name)!;
/** The book's war at a stop: Japan at war with America, Britain and the Dutch, who stand together, in a given month. */
const war = (stop: number, turn: number): Game => {
  const g = grand();
  if (stop !== 3) setStop(g, stop, true);
  g.turn = turn;
  for (const foe of ['USA', 'UK', 'NED']) if (!atWar(g, 'JAP', foe)) declareWar(g, 'JAP', foe);
  for (const nat of ['USA', 'NED', 'AUS']) g.nations[nat].align = 'allies';
  if (!atWar(g, 'JAP', 'AUS')) declareWar(g, 'JAP', 'AUS');
  return g;
};
/** A fleet of these ships alone, set by hand in one sea. */
const squadron = (g: Game, f: Fleet, s: Partial<Ships>, sea: string, stance: Fleet['stance']): Fleet => {
  f.ships = { bb: 0, cv: 0, ca: 0, dd: 0, ss: 0, ...s };
  f.zone = zoneOf(g, sea); f.stance = stance; f.ready = 1;
  return f;
};
/** Every fleet of these nations in harbour, by hand. */
const allHome = (g: Game, ...nats: string[]): void => { for (const f of g.fleets) if (nats.includes(f.nat)) { f.zone = f.home; f.stance = 'port'; } };
const JAP = () => NAVY.beats!.JAP;
const group = (g: Game): Fleet => named(g, 'JAP', 'Advance Submarine Fleet');
const station = (g: Game): string => g.seas[group(g).station].key;
/** The boats Japan's rows build from one month to another, both counted. */
const built = (from: number, to: number): number => {
  let n = 0;
  for (const b of NAVY.building) if (b.nat === 'JAP' && b.cls === 'ss') for (let t = Math.max(from, b.from); t <= Math.min(to, b.to); t++) n += b.perMonth;
  return n;
};

describe("the far side: Japan's submarines built", () => {
  it('the rows are the record\'s boats announced by half-year, I and RO boats only, in whole boats a month', () => {
    // (Japan's destroyers are rows of their own, far-world.json's: the navy thread, 25 November 2026.)
    const rows = NAVY.building.filter((b) => b.nat === 'JAP' && b.cls === 'ss');
    expect(rows.every((b) => b.fleet === '6th Fleet' && Number.isInteger(b.perMonth) && b.perMonth > 0)).toBe(true);
    // January 1942 is month 29, and a half-year is six months.
    const half = (i: number): number => built(29 + 6 * i, 34 + 6 * i);
    expect([0, 1, 2, 3, 4, 5].map(half)).toEqual([16, 18, 15, 20, 10, 8]);
    // 1945, January to August: the war's 111 completed less the 87 announced to the end of 1944.
    expect(built(65, 72)).toBe(24);
    expect(built(29, 72)).toBe(111);
    expect(built(73, 200)).toBe(0);
    // No month's rows overlap.
    for (let t = 1; t <= 72; t++) expect(rows.filter((b) => b.from <= t && t <= b.to).length, `month ${t}`).toBeLessThanOrEqual(1);
  });

  it('Japan goes to war in December 1941 with the record\'s 63 boats', () => {
    const g = grand();
    const boats = g.fleets.filter((f) => f.nat === 'JAP').reduce((n, f) => n + f.ships.ss, 0);
    expect(boats).toBe(55);
    expect(built(1, 28)).toBe(8);
    expect(boats + built(1, 28)).toBe(63);
  });

  it('a month\'s boats join the 6th Fleet, and still do when every boat it had is out with the Advance Submarine Fleet', () => {
    const g = grand();
    const pool = named(g, 'JAP', '6th Fleet');
    g.turn = 30;
    const had = pool.ships.ss;
    navalUpkeep(g, 'JAP');
    expect(pool.ships.ss).toBe(had + 3);
    pool.ships.ss = 0;
    g.turn = 31;
    navalUpkeep(g, 'JAP');
    expect(g.fleets).toContain(pool);
    expect(pool.ships.ss).toBe(3);
  });
});

describe("the far side: Japan's boats from 1943", () => {
  it('no row sends them home, and the seas of each half-year are where the record lost them', () => {
    const B = (farBooks as any)['navy.json'].beats.JAP;
    expect(B.rows).toEqual([[28, 63]]);
    const G = B.groups[0];
    expect(G.hunts.after).toBe('fleet');
    expect(G.hunts.seas.map((r: [number, string[]]) => r[0])).toEqual([28, 41, 47, 53, 59, 65, 71]);
    const first = Object.fromEntries(G.hunts.seas.map((r: [number, string[]]) => [r[0], r[1][0]]));
    expect(first).toEqual({ 28: 'hawaii', 41: 'aleutian', 47: 'gilberts', 53: 'bismarck', 59: 'marianas', 65: 'ryukyu', 71: 'schina' });
    // Every sea named is on the chart.
    const g = grand();
    for (const [, seas] of G.hunts.seas) for (const k of seas) expect(zoneOf(g, k), k).toBeGreaterThanOrEqual(0);
    // Its base is the 6th Fleet's: Kwajalein to February 1944, Saipan to July 1944, then home.
    expect(G.bases).toEqual([[54, 'Saipan', 'Saipan', 'marianas'], [59, 'Hiroshima', 'Kure', 'inland']]);
    const w = war(1, 53);
    g.byName.Saipan.ctrl = 'JAP'; w.byName.Saipan.ctrl = 'JAP';
    expect(baseInForce(w, 'JAP', JAP().groups[0])?.port).toBe('Kwajalein');
    w.turn = 54;
    expect(baseInForce(w, 'JAP', JAP().groups[0])?.port).toBe('Saipan');
    w.turn = 59;
    expect(baseInForce(w, 'JAP', JAP().groups[0])?.port).toBe('Kure');
  });

  it('the boats are out through 1943 and 1944, where every one went home before', () => {
    const g = war(1, 28);
    allHome(g, 'USA', 'UK', 'NED', 'AUS');
    placeBoats(g, 'JAP');
    expect(group(g).ships.ss).toBe(46);
    for (const turn of [41, 47, 53, 58]) {
      g.turn = turn;
      placeBoats(g, 'JAP');
      expect(group(g).ships.ss, `month ${turn}`).toBe(46);
      expect(named(g, 'JAP', '6th Fleet').ships.ss, `month ${turn}`).toBe(0);
    }
  });

  it('the row\'s 63 is the first month\'s count and no ceiling: every boat built joins the group, and in peace all are with the 6th Fleet', () => {
    // (The design director's ruling of 21:00Z and 21:04Z, 22 November 2026: 141 boats on hand in August 1945, 71 of
    // them in the 6th Fleet's harbour all war, where no hunter reached them.)
    expect(JAP().far).toEqual({ war: 28, all: true });
    const g = war(1, 41);
    allHome(g, 'USA', 'UK', 'NED', 'AUS');
    const pool = named(g, 'JAP', '6th Fleet');
    pool.ships.ss = 100;
    placeBoats(g, 'JAP');
    expect(group(g).ships.ss).toBe(100);
    expect(pool.ships.ss).toBe(0);
    // The next month's boats, built to the 6th Fleet, are with the group that month.
    g.turn = 42;
    navalUpkeep(g, 'JAP');
    expect(built(42, 42)).toBe(3);
    expect(group(g).ships.ss).toBe(103);
    expect(pool.ships.ss).toBe(0);
    // America's rows are dated counts by command through the war, and stay the measure of its groups.
    for (const nat of ['USA', 'UK', 'NED']) expect(NAVY.beats![nat].far, nat).toEqual({ war: 28 });
    const us = named(g, 'USA', 'Pacific Submarine Fleet');
    us.ships.ss += 200;
    placeBoats(g, 'USA');
    let row: number[] = [];
    for (const r of NAVY.beats!.USA.rows) if (r[0] <= g.turn) row = r;
    const out = NAVY.beats!.USA.groups.reduce((n, G) => n + (g.fleets.find((f) => f.nat === 'USA' && f.name === G.name)?.ships.ss ?? 0), 0);
    expect(out).toBeLessThanOrEqual(row.slice(1).reduce((a, b) => a + b, 0));
    expect(us.ships.ss).toBeGreaterThan(0);
    // Peace: with no war against a nation the group hunts, every boat is with its pool.
    const p = grand();
    p.turn = 41;
    named(p, 'JAP', '6th Fleet').ships.ss = 100;
    placeBoats(p, 'JAP');
    expect(p.fleets.find((f) => f.nat === 'JAP' && f.name === 'Advance Submarine Fleet')).toBeUndefined();
    expect(named(p, 'JAP', '6th Fleet').ships.ss).toBe(100);
  });

  it('a lost base: at stops 1 and 2 the record\'s dates stand; at stops 3 to 5 the date of the next base its side holds gives way, and the fallen-base rule moves it', () => {
    // (The design director's ruling of 21:00Z and 21:04Z, 22 November 2026: at stop 3 the group stood down from
    // January to July 1943, with Kwajalein lost and Saipan dated February 1944.)
    const G = JAP().groups[0];
    const port = (g: Game): string | undefined => baseInForce(g, 'JAP', G)?.port;
    for (const stop of [1, 2, 3, 4, 5]) {
      const g = war(stop, 41);
      allHome(g, 'USA', 'UK', 'NED', 'AUS');
      g.byName.Saipan.ctrl = 'JAP';
      expect(port(g), `stop ${stop}`).toBe('Kwajalein');
      placeBoats(g, 'JAP');
      const boats = group(g).ships.ss;
      expect(boats).toBeGreaterThan(0);
      g.byName.Kwajalein.ctrl = 'USA';
      expect(port(g), `stop ${stop}`).toBe(stop <= 2 ? undefined : 'Saipan');
      placeBoats(g, 'JAP');
      const f = g.fleets.find((x) => x.nat === 'JAP' && x.name === 'Advance Submarine Fleet');
      if (stop <= 2) {
        // The dates stand: no base in force, and every boat goes home.
        expect(f, `stop ${stop}`).toBeUndefined();
        expect(named(g, 'JAP', '6th Fleet').ships.ss, `stop ${stop}`).toBe(boats);
        continue;
      }
      // The group keeps every boat. The record's dates move no base at stops 3 to 5 (Units decide audit item 43, the
      // design director's ruling of 16:19Z, 25 November 2026, 3a): it lies at Kwajalein until the one rule for a fallen
      // base moves it, in the same month (navalUpkeep), to a harbour its side holds.
      expect(f!.ships.ss, `stop ${stop}`).toBe(boats);
      expect(g.regions[f!.base].name, `stop ${stop}`).toBe('Kwajalein');
      navalUpkeep(g, 'JAP');
      const moved = g.fleets.find((x) => x.nat === 'JAP' && x.name === 'Advance Submarine Fleet');
      expect(moved && allied(g, 'JAP', g.regions[moved.base].ctrl), `stop ${stop}`).toBe(true);
      // The record's base of the day, where a group forms: Saipan lost too, the next on the list, home.
      g.byName.Saipan.ctrl = 'USA';
      expect(port(g), `stop ${stop}`).toBe('Kure');
      // Kwajalein retaken, its date long come: it takes the group back.
      g.byName.Kwajalein.ctrl = 'JAP';
      expect(port(g), `stop ${stop}`).toBe('Kwajalein');
      // None on the whole list: the group stands down.
      for (const name of ['Kwajalein', 'Saipan', 'Hiroshima']) g.byName[name].ctrl = 'USA';
      expect(port(g), `stop ${stop}`).toBeUndefined();
    }
  });

  it('at stops 1 and 2 they go to the listed sea where America\'s fleet stands, the strongest, or where it has landed; else the first listed', () => {
    for (const stop of [1, 2]) {
      const g = war(stop, 47);
      allHome(g, 'USA', 'UK', 'NED', 'AUS');
      placeBoats(g, 'JAP');
      // No fleet out and no landing: the first of the half-year's seas.
      expect(station(g), `stop ${stop}`).toBe('gilberts');
      expect(fleetBeat(g, group(g), JAP().groups[0].hunts!)).toBe(-1);
      // America's fleet in the Solomon Sea, which is on the list.
      const f = squadron(g, named(g, 'USA', 'Pacific Fleet'), { bb: 4, dd: 30 }, 'solomon', 'patrol');
      placeBoats(g, 'JAP');
      expect(station(g), `stop ${stop}`).toBe('solomon');
      // A stronger fleet of its ally's in the Coral Sea: the boats go there.
      squadron(g, named(g, 'AUS', 'Australian Fleet'), { ca: 20, dd: 30 }, 'coral', 'patrol');
      placeBoats(g, 'JAP');
      expect(station(g), `stop ${stop}`).toBe('coral');
      // A fleet in a sea that is not on the half-year's list is not followed.
      allHome(g, 'AUS');
      f.zone = zoneOf(g, 'hawaii');
      expect(fleetReach(g, group(g), true)).toContain(f.zone);
      placeBoats(g, 'JAP');
      expect(station(g), `stop ${stop}`).toBe('gilberts');
      // No fleet at sea, and a landing in the Marshalls this month: there.
      allHome(g, 'USA');
      g.flags[`landed:${zoneOf(g, 'marshalls')}:USA`] = g.turn;
      placeBoats(g, 'JAP');
      expect(station(g), `stop ${stop}`).toBe('marshalls');
      // Boats alone are no fleet to go after.
      delete g.flags[`landed:${zoneOf(g, 'marshalls')}:USA`];
      squadron(g, named(g, 'USA', 'Pacific Submarine Fleet'), { ss: 20 }, 'solomon', 'raid');
      placeBoats(g, 'JAP');
      expect(station(g), `stop ${stop}`).toBe('gilberts');
    }
  });

  it('America\'s own groups go after the cargoes, as they did', () => {
    for (const G of NAVY.beats!.USA.groups) expect(G.hunts?.after, G.name).toBeUndefined();
  });
});
