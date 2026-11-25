// One larder (the design director's rulings of 17:35Z and 19:01Z, 25 November 2026; src/campaign/game.ts keptStoresRun,
// src/campaign/siege.ts invest).
//
// "Under true reach a besieged port's siege begins with the months its kept stores have already run eaten, capped at
// its stores, as a pocket's town already does. One town, one larder, priced once."
//
// A fortified port overseas whose sea has shut lives on its kept stores (the supply picture's clock, game.ts
// computeSupply and portKeep). When the enemy then closes round the town, the siege used to open a full new larder.
// Under true reach it opens with the months the kept stores ran before this month already eaten: never more than they
// last (portKeep), never more than the town holds (storesOf), the larger of that and a pocket's eating (never the sum),
// and nothing for stores filled before the present holder took the port. Off, the siege opens as it always did.
//
// Every picture in this file is drawn under TC_SUPPLY_CHECK, which asks every kept answer afresh and throws on a difference.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_SUPPLY_CHECK = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, capture, computeSupply, createGame, declareWar, keptStoresRun, portKeep, trueReach } from '../src/campaign/game';
import { zoneOf } from '../src/campaign/navy';
import { arrive, besieged, garrison, garrisonFed, invest, monthsShut, ringClosed, siegeMonth, storesOf } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import type { Game, Region } from '../src/campaign/types';

/** A war nobody plays, the old chart's first day, with Units decide; true reach's sub-flag as asked (and the fortress rows). */
const war = (on: boolean, forts = false): Game => {
  const h = createGame(1, null);
  if (forts) h.flags.fortRows = 1;
  const g = startOpen(h);
  g.flags.unitsDecide = 1;
  if (on) g.flags.trueReach = 1;
  g.supplyDirty = true;
  return g;
};
/** One nation's fleets all patrolling one sea. */
const patrol = (g: Game, nat: string, key: string): void => { const z = zoneOf(g, key); for (const f of g.fleets) if (f.nat === nat) { f.zone = z; f.stance = 'patrol'; } };
/** A corps of the nation's set down in the province's field, from its capital. */
const round = (g: Game, nat: string, r: Region): void => {
  const s = addCorps(g, nat, 'Besieger test', 'inf', 1, g.regions[g.nations[nat].capital].name, {});
  g.regions[s.reg].units.splice(g.regions[s.reg].units.indexOf(s), 1);
  s.reg = r.id; r.units.push(s);
};
/** A siege opened round the town by the nation in this month: the months it begins with already eaten. */
const opened = (g: Game, r: Region, by: string): number => { invest(g, r, by); return monthsShut(g, r); };
/** The game at the month, the stores of the port last filled in month `last` (staged: the flag the supply picture keeps). */
const at = (g: Game, r: Region, turn: number, last: number | undefined): void => {
  g.turn = turn;
  if (last === undefined) delete g.flags[`stores:${r.id}`];
  else g.flags[`stores:${r.id}`] = last;
};

describe('a besieged port begins its siege with its kept stores\' months eaten (one larder, true reach)', () => {
  it('L1, the staged order: Britain\'s Malta, the sea shut by Italy\'s fleet, lives on its kept stores; Italy then closes round it: on, the siege begins with the month the stores ran eaten and the garrison goes hungry a month sooner (off: a full larder, pinned)', () => {
    const hungry: Record<string, number> = {};
    for (const on of [false, true]) {
      const g = war(on);
      const r = g.byName['Malta'];
      const c = addCorps(g, 'UK', 'Malta test', 'inf', 1, 'Malta', {});
      expect(garrison(g, c)).toBe(true);
      // Italy at war with Britain, her fleet across the central Mediterranean, the one sea off Malta's harbour.
      declareWar(g, 'ITA', 'UK');
      patrol(g, 'ITA', 'central');
      g.supplyDirty = true;
      computeSupply(g);
      expect([trueReach(g), r.ctrl, r.overseas, r.port, !!r.fort, r.since, r.seas, r.nb]).toEqual([on, 'UK', true, true, true, 0, [zoneOf(g, 'central')], []]);
      const keep = portKeep(g, r), stores = storesOf(g, r);
      expect([keep, stores]).toEqual([2, 4]);
      // The months of the supply picture, each opening Britain's month: on, no ship carries Britain's stores to Malta
      // and the flag stays at the first day, so month 2 is the first on the kept stores; off, the flag is kept fresh
      // by paper (tests/true-reach-r2.test.ts).
      for (let t = 2; t <= 3; t++) {
        g.turn = t;
        siegeMonth(g, 'UK');
        computeSupply(g);
        expect([on, g.turn, g.flags[`stores:${r.id}`], c.supplied]).toEqual([on, t, on ? 1 : t, true]);
      }
      // Month 3: Italy's corps comes into the field and the town holds: the siege begins. On, the one month the stores
      // ran before this month (month 2) is already eaten; off, the larder is full.
      round(g, 'ITA', r);
      expect(arrive(g, r, 'ITA')).toBe(false);
      expect([on, besieged(g, r), ringClosed(g, r), monthsShut(g, r)]).toEqual([on, true, true, on ? 1 : 0]);
      computeSupply(g);
      // The months of the siege: Britain's month opens with another month eaten, the ring closed and no ship coming in.
      for (let t = 4; t <= 9; t++) {
        g.turn = t;
        siegeMonth(g, 'UK');
        computeSupply(g);
        const shut = t - 3 + (on ? 1 : 0), eats = shut <= stores;
        expect([on, t, monthsShut(g, r), garrisonFed(g, r), c.supplied]).toEqual([on, t, shut, eats, eats]);
        if (!eats && hungry[String(on)] === undefined) hungry[String(on)] = t;
      }
    }
    // Off the garrison goes hungry in month 8; on, a month sooner.
    expect(hungry).toEqual({ false: 8, true: 7 });
  });

  it('L2: charges nothing where the sea is open and the stores were filled this month, or last month', () => {
    for (const on of [false, true]) {
      // Malta at peace: Britain's own ships fill its stores in every draw of the picture.
      const g = war(on);
      const r = g.byName['Malta'];
      g.turn = 3;
      computeSupply(g);
      expect([on, g.flags[`stores:${r.id}`], keptStoresRun(g, r)]).toEqual([on, 3, 0]);
      expect([on, opened(g, r, 'ITA')]).toEqual([on, 0]);
      // Filled last month: this month is the first on the stores, and it is the siege's own to charge.
      const h = war(on), m = h.byName['Malta'];
      at(h, m, 3, 2);
      expect([on, keptStoresRun(h, m), opened(h, m, 'ITA')]).toEqual([on, 0, 0]);
    }
  });

  it('L3: a pocket and kept stores together: the larger of the two, never the sum (off: the pocket alone, pinned)', () => {
    for (const on of [false, true]) {
      for (const [tenths, want] of [[30, 3], [10, on ? 2 : 1], [0, on ? 2 : 0]] as const) {
        const g = war(on);
        const r = g.byName['Malta'];
        // Its stores last filled in month 1, the siege in month 4: two months run (portKeep). A pocket round the town,
        // Britain's, that has eaten `tenths` tenths of a month of its stores.
        at(g, r, 4, 1);
        g.flags[`pkt:${r.id}`] = 2;
        g.flags[`pkte:${r.id}`] = tenths;
        g.flags[`pktn:${r.id}`] = Object.keys(g.nations).indexOf('UK');
        expect([on, keptStoresRun(g, r), storesOf(g, r)]).toEqual([on, 2, 4]);
        expect([on, tenths, opened(g, r, 'ITA'), g.flags[`pkt:${r.id}`]]).toEqual([on, tenths, want, undefined]);
      }
    }
  });

  it('L4: both caps: a sea shut longer than portKeep charges portKeep, and never more than the town holds (off: nothing, pinned)', () => {
    for (const on of [false, true]) {
      // Malta, its stores last filled in month 1 and the siege in month 8: six months, of which the stores ran two.
      const g = war(on);
      const r = g.byName['Malta'];
      at(g, r, 8, 1);
      expect([on, portKeep(g, r), storesOf(g, r), opened(g, r, 'ITA')]).toEqual([on, 2, 4, on ? 2 : 0]);
      // Gibraltar under the fortress rows keeps the historian's sixteen months of its garrison's stores: the stores
      // ran seven months before month 9, and the town holds four.
      const f = war(on, true);
      const gib = f.byName['Gibraltar'];
      at(f, gib, 9, 1);
      expect([on, portKeep(f, gib), keptStoresRun(f, gib), storesOf(f, gib), opened(f, gib, 'ESP')]).toEqual([on, 16, 7, 4, on ? 4 : 0]);
    }
  });

  it('L5: charges nothing in a town that keeps no stores: not overseas (Leningrad), no works (Alexandria), no port (El Alamein)', () => {
    for (const on of [false, true]) {
      for (const [name, why] of [['Leningrad', 'overseas'], ['Alexandria', 'fort'], ['El Alamein', 'port']] as const) {
        const g = war(on);
        const r = g.byName[name];
        expect([name, r.overseas, r.port, !!r.fort]).toEqual([name, why !== 'overseas', why !== 'port', why !== 'fort']);
        at(g, r, 4, 1);
        expect([on, name, keptStoresRun(g, r), opened(g, r, 'GER')]).toEqual([on, name, 0, 0]);
      }
    }
  });

  it('L6: charges nothing for stores filled before the present holder took the port: Italy\'s Tobruk taken by Britain', () => {
    for (const taken of [false, true]) {
      const g = war(true);
      const r = g.byName['Tobruk'];
      // Month 1: Italy at peace, her ships fill Tobruk's stores.
      computeSupply(g);
      expect(g.flags[`stores:${r.id}`]).toBe(1);
      // Month 2: Britain at war with Italy, her fleet across the Libyan Sea. Taken: Britain takes the town that month.
      g.turn = 2;
      declareWar(g, 'UK', 'ITA');
      patrol(g, 'UK', 'libyan');
      if (taken) capture(g, r, 'UK');
      g.supplyDirty = true;
      computeSupply(g);
      g.turn = 4;
      computeSupply(g);
      // The flag is the month Italy's ships last filled it. Kept by Italy, the stores have run two months; taken by
      // Britain, they were never Britain's.
      const holder = taken ? 'UK' : 'ITA', foe = taken ? 'ITA' : 'UK';
      expect([taken, r.ctrl, r.since, g.flags[`stores:${r.id}`]]).toEqual([taken, holder, taken ? 2 : 0, 1]);
      expect([taken, keptStoresRun(g, r), opened(g, r, foe)]).toEqual([taken, taken ? 0 : 2, taken ? 0 : 2]);
    }
  });
});
