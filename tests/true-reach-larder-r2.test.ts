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
import { addCorps, capitulate, capture, computeSupply, createGame, declareWar, keptStoresRun, portKeep, trueReach } from '../src/campaign/game';
import { zoneOf } from '../src/campaign/navy';
import { arrive, besieged, garrison, garrisonFed, invest, monthsShut, ringClosed, siegeMonth, siegeView, storesOf } from '../src/campaign/siege';
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
const round = (g: Game, nat: string, r: Region, divs = 1): void => {
  const s = addCorps(g, nat, 'Besieger test', 'inf', divs, g.regions[g.nations[nat].capital].name, {});
  g.regions[s.reg].units.splice(g.regions[s.reg].units.indexOf(s), 1);
  s.reg = r.id; r.units.push(s);
};
/** Britain's month: its sieges' reckoning, then the draw of the supply picture (game.ts startPhase). */
const britain = (g: Game): void => { g.phase = g.order.indexOf('UK'); siegeMonth(g, 'UK'); computeSupply(g); };
/** A siege opened round the town by the nation in this month: the months it begins with already eaten. */
const opened = (g: Game, r: Region, by: string): number => { invest(g, r, by); return monthsShut(g, r); };
/** The game at the month, the stores of the port last filled in month `last` (staged: the flag the supply picture keeps). */
const at = (g: Game, r: Region, turn: number, last: number | undefined): void => {
  g.turn = turn;
  if (last === undefined) delete g.flags[`stores:${r.id}`];
  else g.flags[`stores:${r.id}`] = last;
};

describe('a besieged port begins its siege with its kept stores\' months eaten (one larder, true reach)', () => {
  it('L1, the staged order: Britain\'s Malta, the sea shut by Italy\'s fleet, lives on its kept stores; Italy then closes round it in her own month, which comes before Britain\'s: on, the siege begins with the month the stores ran eaten and the garrison goes hungry a month sooner (off: a full larder, pinned)', () => {
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
      expect(g.order.indexOf('ITA')).toBeLessThan(g.order.indexOf('UK'));
      const keep = portKeep(g, r), stores = storesOf(g, r);
      expect([keep, stores]).toEqual([2, 4]);
      // Month 2, Britain's month: no ship carries Britain's stores to Malta and the flag stays at the first day, so
      // month 2 is the first on the kept stores (off, the flag is kept fresh by paper: tests/true-reach-r2.test.ts).
      g.turn = 2;
      britain(g);
      expect([on, g.flags[`stores:${r.id}`], c.supplied]).toEqual([on, on ? 1 : 2, true]);
      // Month 3, Italy's month: her corps comes into the field and the town holds: the siege begins. On, the one month
      // the stores ran before this month (month 2) is already eaten; off, the larder is full. Britain's month then
      // charges month 3 on the siege's clock.
      g.turn = 3;
      g.phase = g.order.indexOf('ITA');
      round(g, 'ITA', r);
      expect(arrive(g, r, 'ITA')).toBe(false);
      expect([on, besieged(g, r), ringClosed(g, r), monthsShut(g, r)]).toEqual([on, true, true, on ? 1 : 0]);
      computeSupply(g);
      // The months of the siege: Britain's month opens with another month eaten, the ring closed and no ship coming in.
      for (let t = 3; t <= 9; t++) {
        g.turn = t;
        britain(g);
        const shut = t - 2 + (on ? 1 : 0), eats = shut <= stores;
        expect([on, t, monthsShut(g, r), garrisonFed(g, r), c.supplied]).toEqual([on, t, shut, eats, eats]);
        if (!eats && hungry[String(on)] === undefined) hungry[String(on)] = t;
      }
    }
    // Off the garrison goes hungry in month 7; on, a month sooner.
    expect(hungry).toEqual({ false: 7, true: 6 });
  });

  it('L7, both orders of the month: the garrison eats one larder, its stores\' months from the month the sea shut, whether the enemy closes round before or after the holder\'s month (off: a larder more, pinned)', () => {
    const fed: Record<string, number[]> = {};
    for (const on of [false, true]) {
      for (const order of ['before', 'after'] as const) {
        // Before: Italy closes round Britain's Malta in her own month, ahead of Britain's. After: Britain closes round
        // Italy's Tobruk in her own month, once Italy's month has drawn its supply from the kept stores.
        const g = war(on);
        const [holder, foe, town, sea] = order === 'before' ? ['UK', 'ITA', 'Malta', 'central'] : ['ITA', 'UK', 'Tobruk', 'libyan'];
        const r = g.byName[town];
        expect(g.order.indexOf(foe) < g.order.indexOf(holder)).toBe(order === 'before');
        const c = addCorps(g, holder, 'Larder test', 'inf', 1, town, {});
        expect(garrison(g, c)).toBe(true);
        // Month 1, at peace: the holder's ships fill the stores.
        computeSupply(g);
        expect([on, order, g.flags[`stores:${r.id}`], portKeep(g, r), storesOf(g, r)]).toEqual([on, order, 1, 2, order === 'before' ? 4 : 3]);
        declareWar(g, foe, holder);
        patrol(g, foe, sea);
        g.supplyDirty = true;
        const months: number[] = [];
        for (let t = 2; t <= 12; t++) {
          g.turn = t;
          const foeMonth = (): void => {
            g.phase = g.order.indexOf(foe);
            if (t === 3) { round(g, foe, r, order === 'before' ? 1 : 6); expect(arrive(g, r, foe)).toBe(false); expect(ringClosed(g, r)).toBe(true); }
            computeSupply(g);
          };
          if (order === 'before') foeMonth();
          g.phase = g.order.indexOf(holder);
          siegeMonth(g, holder);
          computeSupply(g);
          if (c.supplied) months.push(t);
          if (order === 'after') foeMonth();
        }
        fed[`${on} ${order}`] = months;
      }
    }
    // On: one larder from month 2, the first month the sea was shut, in either order: Malta's four months, Tobruk's
    // three. Off: the kept stores' months come on top of the siege's (one before Italy closes round Malta in month 3,
    // two before Britain closes round Tobruk once Italy's month 3 has drawn on them).
    expect(fed).toEqual({
      'false before': [2, 3, 4, 5, 6], 'false after': [2, 3, 4, 5, 6],
      'true before': [2, 3, 4, 5], 'true after': [2, 3, 4],
    });
  });

  it('L8: the card of a fortified port overseas living on its kept stores promises what a siege opened now would leave (off: its whole stores, pinned)', () => {
    for (const on of [false, true]) {
      const g = war(on);
      const r = g.byName['Malta'];
      const c = addCorps(g, 'UK', 'Malta test', 'inf', 1, 'Malta', {});
      expect(garrison(g, c)).toBe(true);
      declareWar(g, 'ITA', 'UK');
      patrol(g, 'ITA', 'central');
      g.supplyDirty = true;
      computeSupply(g);
      g.turn = 2;
      britain(g);
      g.turn = 3;
      const line = (): string => siegeView(g, r.id)!.lines.at(-1)!;
      // In Italy's month 3, before Britain's: a siege opened now would begin with month 2 eaten, and Britain's month
      // would charge month 3: three months of the four are left.
      g.phase = g.order.indexOf('ITA');
      const early = line();
      // In Britain's month 3, once the kept stores have fed the garrison this month too: two are left.
      britain(g);
      const late = line();
      expect([on, early, late]).toEqual([on,
        `Closely invested, its stores would feed the defenders for ${on ? '3 months' : '4 months'}, and for as long as ships can reach its harbour.`,
        `Closely invested, its stores would feed the defenders for ${on ? '2 months' : '4 months'}, and for as long as ships can reach its harbour.`]);
      // And the siege that opens then says the same.
      round(g, 'ITA', r);
      expect(arrive(g, r, 'ITA')).toBe(false);
      expect([on, line()]).toEqual([on, `Closely invested. Its stores will feed the defenders ${on ? '2 more months' : '4 more months'}; after that they go hungry.`]);
    }
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
      // A pocket that was another nation's (Italy's, round a British Malta) is not the town's eating: on, the kept
      // stores alone; off, nothing.
      const g = war(on);
      const r = g.byName['Malta'];
      at(g, r, 4, 1);
      g.flags[`pkt:${r.id}`] = 2;
      g.flags[`pkte:${r.id}`] = 30;
      g.flags[`pktn:${r.id}`] = Object.keys(g.nations).indexOf('ITA');
      expect([on, 'foreign', opened(g, r, 'GER'), g.flags[`pkt:${r.id}`]]).toEqual([on, 'foreign', on ? 2 : 0, undefined]);
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

  it('L6, without a battle: charges nothing for stores Italy filled in Tobruk once Italy has fallen and the German corps there holds it', () => {
    for (const falls of [false, true]) {
      const g = war(true);
      const r = g.byName['Tobruk'];
      // Month 1: Italy at peace, her ships fill Tobruk's stores; a German corps stands in the town.
      computeSupply(g);
      expect(g.flags[`stores:${r.id}`]).toBe(1);
      declareWar(g, 'UK', 'ITA');
      declareWar(g, 'UK', 'GER');
      addCorps(g, 'GER', 'Afrika test', 'inf', 1, 'Tobruk', {});
      // Month 2: Italy gives in to Britain; the ground the German corps stands on stays in German hands. Region.since
      // does not move.
      g.turn = 2;
      if (falls) capitulate(g, 'ITA', 'UK');
      g.turn = 4;
      expect([falls, r.ctrl, r.owner, r.since, g.flags[`stores:${r.id}`]]).toEqual([falls, falls ? 'GER' : 'ITA', 'ITA', 0, 1]);
      // Kept by Italy, the stores have run two months; handed to Germany, they were never Germany's.
      expect([falls, keptStoresRun(g, r), opened(g, r, 'UK')]).toEqual([falls, falls ? 0 : 2, falls ? 0 : 2]);
    }
  });
});
