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
// and only for stores the present holder filled: under true reach the fill records whose stores they are (the
// holder's, `stores:by:<id>`, whoever's ships carried them), as a pocket's town records whose pocket it was, and a
// record that names another nation, or no record, charges nothing. Off, the siege opens as it always did, and no record
// is written.
//
// Every picture in this file is drawn under TC_SUPPLY_CHECK, which asks every kept answer afresh and throws on a difference.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_SUPPLY_CHECK = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, allied, capitulate, capture, computeSupply, createGame, declareWar, keptStoresRun, natIndex, portKeep, setTrueReach, trueReach } from '../src/campaign/game';
import { finishMonthEnd } from '../src/campaign/finish';
import { liberate } from '../src/campaign/liberation';
import { loadGame, saveGame } from '../src/campaign/save';
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
/**
 * The game at the month, the stores of the port last filled in month `last` by nation `by` (staged: the month mark and
 * the record of whose stores they are, as the supply picture writes them under true reach; by default the present
 * holder's, and none when `by` is null).
 */
const at = (g: Game, r: Region, turn: number, last: number | undefined, by: string | null = r.ctrl): void => {
  g.turn = turn;
  if (last === undefined) delete g.flags[`stores:${r.id}`];
  else g.flags[`stores:${r.id}`] = last;
  if (last === undefined || by === null) delete g.flags[`stores:by:${r.id}`];
  else g.flags[`stores:by:${r.id}`] = natIndex(g, by);
};
/** Whose stores the port's are by the record the supply picture wrote, or undefined where there is none. */
const filler = (g: Game, r: Region): string | undefined => {
  const i = g.flags[`stores:by:${r.id}`];
  return i === undefined ? undefined : Object.keys(g.nations)[i];
};

describe('a besieged port begins its siege with its kept stores\' months eaten (one larder, true reach)', () => {
  it('L1, the staged order: Britain\'s Malta, the sea shut by Italy\'s fleet, lives on its kept stores; Italy then closes round it in her own month, which comes before Britain\'s: on, the siege begins with the month the stores ran eaten and the garrison goes hungry a month sooner (off: a full larder, pinned)', () => {
    const hungry: Record<string, number> = {};
    for (const on of [false, true]) {
      const g = war(on);
      const r = g.byName['Malta'];
      const c = addCorps(g, 'UK', 'Malta test', 'inf', 1, 'Malta', {});
      expect(garrison(g, c)).toBe(true);
      // Month 1, at peace: Britain's ships fill Malta's stores, and on, the draw writes her record with the month.
      computeSupply(g);
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
      // Month 1, at peace: Britain's ships fill Malta's stores, and on, the draw writes her record with the month.
      computeSupply(g);
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
      expect([taken, r.ctrl, r.since, g.flags[`stores:${r.id}`], filler(g, r)]).toEqual([taken, holder, taken ? 2 : 0, 1, 'ITA']);
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
      expect([falls, r.ctrl, r.owner, r.since, g.flags[`stores:${r.id}`], filler(g, r)]).toEqual([falls, falls ? 'GER' : 'ITA', 'ITA', 0, 1, 'ITA']);
      // Kept by Italy, the stores have run two months; handed to Germany, they were never Germany's.
      expect([falls, keptStoresRun(g, r), opened(g, r, 'UK')]).toEqual([falls, falls ? 0 : 2, falls ? 0 : 2]);
    }
  });

  it('L9: a country that falls to Britain changes nothing in Britain\'s own Malta: its kept stores are still charged when Italy closes round it', () => {
    for (const falls of [false, true]) {
      const g = war(true);
      const r = g.byName['Malta'];
      const c = addCorps(g, 'UK', 'Malta test', 'inf', 1, 'Malta', {});
      expect(garrison(g, c)).toBe(true);
      // Month 1, at peace: Britain's ships fill Malta's stores, and on, the draw writes her record with the month.
      computeSupply(g);
      // The sea shut by Italy's fleet from the first month: the stores were last filled in month 1.
      declareWar(g, 'ITA', 'UK');
      patrol(g, 'ITA', 'central');
      g.supplyDirty = true;
      computeSupply(g);
      g.turn = 2;
      britain(g);
      expect([g.flags[`stores:${r.id}`], filler(g, r)]).toEqual([1, 'UK']);
      // Month 3: Iran gives in to Britain, after the stores' month. Nothing of Malta changes hands.
      g.turn = 3;
      if (falls) capitulate(g, 'IRN', 'UK');
      expect([falls, g.nations.IRN.fellOn?.to, g.nations.IRN.fellOn?.turn, r.ctrl, r.owner, r.since, r.prev]).toEqual([falls, falls ? 'UK' : undefined, falls ? 3 : undefined, 'UK', 'UK', 0, null]);
      // Italy then closes round the town in her own month 3: month 2 on the stores is eaten either way.
      g.phase = g.order.indexOf('ITA');
      round(g, 'ITA', r);
      expect(arrive(g, r, 'ITA')).toBe(false);
      expect([falls, keptStoresRun(g, r), besieged(g, r), monthsShut(g, r)]).toEqual([falls, 1, true, 1]);
    }
  });

  it('L10: charges nothing for stores Germany filled once Tobruk is handed back to a freed Italy, and charges Italy\'s own stores again once she fills them', () => {
    for (const freed of [false, true]) {
      const g = war(true);
      const r = g.byName['Tobruk'];
      declareWar(g, 'UK', 'ITA');
      declareWar(g, 'UK', 'GER');
      addCorps(g, 'GER', 'Afrika test', 'inf', 1, 'Tobruk', {});
      // Month 2: Italy gives in to Britain; the German corps keeps Tobruk.
      g.turn = 2;
      capitulate(g, 'ITA', 'UK');
      expect([r.ctrl, r.owner, r.since]).toEqual(['GER', 'ITA', 0]);
      // Month 3: Germany's ships fill the stores, Germany's record (staged). Month 4: Germany frees Italy, and Tobruk is
      // Italy's again.
      at(g, r, 3, 3, 'GER');
      g.turn = 4;
      if (freed) expect(liberate(g, 'ITA', 'GER')).toBe(true);
      expect([freed, r.ctrl, r.owner, r.since, g.nations.ITA.freedOn?.turn]).toEqual([freed, freed ? 'ITA' : 'GER', 'ITA', 0, freed ? 4 : undefined]);
      // Month 6, Britain closes round the town: Germany's stores are Germany's two months; handed back, never Italy's.
      g.turn = 6;
      expect([freed, keptStoresRun(g, r), opened(g, r, 'UK')]).toEqual([freed, freed ? 0 : 2, freed ? 0 : 2]);
      // The holder's own fill in month 6 (his record), the sea shut after it: in month 9 his stores have run two months.
      at(g, r, 9, 6);
      expect([freed, 'refilled', keptStoresRun(g, r)]).toEqual([freed, 'refilled', 2]);
    }
  });

  it('L11, Vichy\'s ground: the stores France filled are not Vichy\'s after the armistice, nor are Vichy\'s France\'s after Vichy\'s end, until the new holder fills them (staged works: no fortified port overseas lies on Vichy\'s ground on the old chart)', () => {
    // Tunis, a port overseas on Vichy's ground, given works by hand so that it keeps stores.
    const tunis = (g: Game): Region => {
      const r = g.byName['Tunis'];
      expect([r.vichy, r.overseas, r.port, r.fort, r.owner, r.ctrl]).toEqual([true, true, true, null, 'FRA', 'FRA']);
      r.fort = { lv: 2, vs: ['ITA'], by: 'FRA' };
      return r;
    };
    for (const falls of [false, true]) {
      // The armistice: France's ships fill the stores in month 2; France falls to Germany in month 3 and Tunis is Vichy's.
      const g = war(true);
      const r = tunis(g);
      at(g, r, 2, 2, 'FRA');
      g.turn = 3;
      if (falls) capitulate(g, 'FRA', 'GER');
      expect([falls, r.ctrl, r.owner, r.since]).toEqual([falls, falls ? 'VIC' : 'FRA', falls ? 'VIC' : 'FRA', 0]);
      g.turn = 5;
      expect([falls, 'armistice', keptStoresRun(g, r), opened(g, r, 'UK')]).toEqual([falls, 'armistice', falls ? 0 : 2, falls ? 0 : 2]);
      at(g, r, 9, 6);
      expect([falls, 'armistice refilled', filler(g, r), keptStoresRun(g, r)]).toEqual([falls, 'armistice refilled', falls ? 'VIC' : 'FRA', 2]);
    }
    for (const ends of [false, true]) {
      // Vichy's end: France falls in month 2 and is freed by Britain in month 3; Vichy's ships fill Tunis's stores in
      // month 4; France holds Paris again, and at the end of month 5 Vichy's ground is France's.
      const g = war(true);
      const r = tunis(g);
      declareWar(g, 'UK', 'GER');
      g.turn = 2;
      capitulate(g, 'FRA', 'GER');
      g.turn = 3;
      expect(liberate(g, 'FRA', 'UK')).toBe(true);
      capture(g, g.regions[g.nations.FRA.capital], 'FRA');
      at(g, r, 5, 4, 'VIC');
      if (ends) finishMonthEnd(g);
      expect([ends, r.ctrl, r.owner, r.since, g.nations.FRA.freedOn?.turn, g.nations.VIC.fellOn?.turn]).toEqual([ends, ends ? 'FRA' : 'VIC', ends ? 'FRA' : 'VIC', 0, 3, ends ? 5 : undefined]);
      g.turn = 7;
      expect([ends, 'end', keptStoresRun(g, r), opened(g, r, 'GER')]).toEqual([ends, 'end', ends ? 0 : 2, ends ? 0 : 2]);
      at(g, r, 10, 7);
      expect([ends, 'end refilled', filler(g, r), keptStoresRun(g, r)]).toEqual([ends, 'end refilled', ends ? 'FRA' : 'VIC', 2]);
    }
  });

  it('L12, a victor\'s taking: charges nothing for Germany\'s stores once Germany\'s fall hands Italy\'s Tobruk to Britain, and charges Britain\'s own once she fills them', () => {
    for (const falls of [false, true]) {
      const g = war(true);
      const r = g.byName['Tobruk'];
      declareWar(g, 'UK', 'ITA');
      declareWar(g, 'UK', 'GER');
      addCorps(g, 'GER', 'Afrika test', 'inf', 1, 'Tobruk', {});
      g.turn = 2;
      capitulate(g, 'ITA', 'UK');
      // Month 3: Germany's ships fill the stores, Germany's record (staged). Month 4: Germany gives in to Britain, and
      // Italy's Tobruk, its owner fallen, goes to Britain.
      at(g, r, 3, 3, 'GER');
      g.turn = 4;
      if (falls) capitulate(g, 'GER', 'UK');
      expect([falls, r.ctrl, r.owner, r.since]).toEqual([falls, falls ? 'UK' : 'GER', 'ITA', 0]);
      g.turn = 6;
      expect([falls, keptStoresRun(g, r), opened(g, r, 'SOV')]).toEqual([falls, falls ? 0 : 2, falls ? 0 : 2]);
      at(g, r, 9, 6);
      expect([falls, 'refilled', filler(g, r), keptStoresRun(g, r)]).toEqual([falls, 'refilled', falls ? 'UK' : 'GER', 2]);
    }
  });

  it('L13, a hand back: charges nothing for Italy\'s stores once Britain\'s Malta comes back to Britain at Italy\'s fall, and charges Britain\'s own once she fills them', () => {
    for (const falls of [false, true]) {
      const g = war(true);
      const r = g.byName['Malta'];
      declareWar(g, 'ITA', 'UK');
      // Month 2: Italy takes Malta. Month 3: Italy's ships fill its stores, Italy's record (staged). Month 4: Italy
      // gives in to Britain, and the foreign ground she held goes back to its owner.
      g.turn = 2;
      capture(g, r, 'ITA');
      at(g, r, 3, 3, 'ITA');
      g.turn = 4;
      if (falls) capitulate(g, 'ITA', 'UK');
      expect([falls, r.ctrl, r.owner, r.since, r.prev]).toEqual([falls, falls ? 'UK' : 'ITA', 'UK', 2, 'UK']);
      g.turn = 6;
      expect([falls, keptStoresRun(g, r), opened(g, r, 'GER')]).toEqual([falls, falls ? 0 : 2, falls ? 0 : 2]);
      at(g, r, 9, 6);
      expect([falls, 'refilled', filler(g, r), keptStoresRun(g, r)]).toEqual([falls, 'refilled', falls ? 'UK' : 'ITA', 2]);
    }
  });

  it('L14: a siege the holder\'s own month opens (siegeMonth) charges the present month on the siege\'s clock, not twice: Malta, the stores filled last month', () => {
    for (const on of [false, true]) {
      const g = war(on);
      const r = g.byName['Malta'];
      const c = addCorps(g, 'UK', 'Malta test', 'inf', 1, 'Malta', {});
      expect(garrison(g, c)).toBe(true);
      // Month 1, at peace: Britain's ships fill Malta's stores, and on, the draw writes her record with the month.
      computeSupply(g);
      declareWar(g, 'ITA', 'UK');
      patrol(g, 'ITA', 'central');
      g.supplyDirty = true;
      computeSupply(g);
      // Month 2: the stores' flag is a month old (on). An Italian corps stands in the field, not yet round the town.
      g.turn = 2;
      expect([on, g.flags[`stores:${r.id}`]]).toEqual([on, 1]);
      round(g, 'ITA', r);
      expect(g.flags[`siege:${r.id}`]).toBeUndefined();
      // Britain's month opens the siege and charges month 2 on its own clock: T - 1 - L = 0 months of kept stores
      // first, then the present month.
      g.phase = g.order.indexOf('UK');
      siegeMonth(g, 'UK');
      expect([on, g.flags[`siege:${r.id}`], ringClosed(g, r), keptStoresRun(g, r), monthsShut(g, r)]).toEqual([on, 2, true, 0, 1]);
    }
  });

  it('L15, the last act: charges nothing for Italy\'s stores once a leaving Italy hands Malta back to Britain with no fall recorded, and charges Britain\'s own once she fills them (staged: domination.ts leave hands Malta, Italy\'s since month 2, back to Britain)', () => {
    for (const opens of [false, true]) {
      const g = war(true);
      const r = g.byName['Malta'];
      declareWar(g, 'ITA', 'UK');
      g.turn = 2;
      capture(g, r, 'ITA');
      at(g, r, 3, 3, 'ITA');
      // Month 4: the last act opens, and a nation that leaves the war hands what it held to its owner, as leave does:
      // the flag over the ground changes and nothing else (no fall, no Region.since).
      g.turn = 4;
      if (opens) { g.flags.dom = 4; r.ctrl = r.owner; }
      expect([opens, r.ctrl, r.owner, r.since, r.prev, g.nations.ITA.fellOn]).toEqual([opens, opens ? 'UK' : 'ITA', 'UK', 2, 'UK', undefined]);
      g.turn = 6;
      expect([opens, keptStoresRun(g, r), opened(g, r, 'GER')]).toEqual([opens, opens ? 0 : 2, opens ? 0 : 2]);
      at(g, r, 9, 6);
      expect([opens, 'refilled', filler(g, r), keptStoresRun(g, r)]).toEqual([opens, 'refilled', opens ? 'UK' : 'ITA', 2]);
    }
  });

  it('L16: a real fill writes whose stores they are, the holder\'s, and only under true reach (off: no record, and every other flag of the save the same)', () => {
    const flags: Record<string, Record<string, number>> = {};
    for (const on of [false, true]) {
      // Malta at peace in month 3: Britain's own ships fill its stores in the draw of the picture (nothing staged).
      const g = war(on);
      const r = g.byName['Malta'];
      g.turn = 3;
      computeSupply(g);
      expect([on, g.flags[`stores:${r.id}`], filler(g, r)]).toEqual([on, 3, on ? 'UK' : undefined]);
      flags[String(on)] = JSON.parse(saveGame(g)).flags;
    }
    // On, a record beside every month mark, and nothing else written; off, none.
    const on = { ...flags.true }, marks = Object.keys(on).filter((k) => k.startsWith('stores:by:'));
    expect(marks.length).toBeGreaterThan(0);
    expect(marks.map((k) => on[`stores:${k.slice('stores:by:'.length)}`] !== undefined)).toEqual(marks.map(() => true));
    for (const k of marks) delete on[k];
    delete on.trueReach;
    expect(Object.keys(flags.false).filter((k) => k.startsWith('stores:by:'))).toEqual([]);
    expect(on).toEqual(flags.false);
  });

  it('L17: an ally\'s ships fill the holder\'s stores: Britain\'s Malta, Britain with no ships of her own, filled by Canada\'s: the record names Britain, the holder, not Canada', () => {
    for (const on of [false, true]) {
      const g = war(on);
      const r = g.byName['Malta'];
      g.nations.UK.seaSupply = false;
      g.nations.UK.sealift = 0;
      g.supplyDirty = true;
      g.turn = 3;
      computeSupply(g);
      // The stores are filled this month, by an ally's ships alone (Canada's, the one ally of Britain's with any).
      const ships = Object.values(g.nations).filter((n) => n.alive && (n.seaSupply || n.sealift > 0) && allied(g, 'UK', n.id)).map((n) => n.id);
      expect([on, ships, g.flags[`stores:${r.id}`], filler(g, r)]).toEqual([on, ['CAN'], 3, on ? 'UK' : undefined]);
      // The siege that would open in month 6, the sea shut since: Britain's stores, two months run.
      at(g, r, 6, 3, on ? 'UK' : null);
      expect([on, keptStoresRun(g, r)]).toEqual([on, on ? 2 : 0]);
    }
  });

  it('L18, the rechecker\'s over-charge: Italy\'s stores in Malta, a German corps standing there when Italy gives in to Britain: Malta is Germany\'s with no battle, and Italy\'s stores are not Germany\'s to have eaten', () => {
    for (const falls of [false, true]) {
      const g = war(true);
      const r = g.byName['Malta'];
      declareWar(g, 'ITA', 'UK');
      declareWar(g, 'GER', 'UK');
      // Month 2: Italy takes Malta, and a German corps comes to stand in it. Month 3: Italy's ships fill its stores.
      g.turn = 2;
      capture(g, r, 'ITA');
      addCorps(g, 'GER', 'Stay test', 'inf', 1, 'Malta', {});
      at(g, r, 3, 3, 'ITA');
      // Month 4: Italy gives in to Britain; the ground the German corps stands on stays on the fallen side, Germany's.
      g.turn = 4;
      if (falls) capitulate(g, 'ITA', 'UK');
      expect([falls, r.ctrl, r.owner, r.since, filler(g, r)]).toEqual([falls, falls ? 'GER' : 'ITA', 'UK', 2, 'ITA']);
      g.turn = 6;
      expect([falls, keptStoresRun(g, r)]).toEqual([falls, falls ? 0 : 2]);
      // Germany's own fill in month 6, the sea shut after it: charged again in month 9.
      at(g, r, 9, 6);
      expect([falls, 'refilled', keptStoresRun(g, r)]).toEqual([falls, 'refilled', 2]);
    }
  });

  it('L19, the rechecker\'s under-charge: Malta back with Britain at Italy\'s fall, Britain\'s own stores of month 6, and Iran falling to Britain in month 7: still charged in month 9', () => {
    for (const falls of [false, true]) {
      const g = war(true);
      const r = g.byName['Malta'];
      declareWar(g, 'ITA', 'UK');
      g.turn = 2;
      capture(g, r, 'ITA');
      g.turn = 4;
      capitulate(g, 'ITA', 'UK');
      expect([r.ctrl, r.owner, r.since, r.prev]).toEqual(['UK', 'UK', 2, 'UK']);
      // Month 6: Britain fills the stores, her record. Month 7: Iran gives in to Britain. Nothing of Malta changes hands.
      at(g, r, 6, 6);
      g.turn = 7;
      if (falls) capitulate(g, 'IRN', 'UK');
      g.turn = 9;
      expect([falls, g.nations.IRN.fellOn?.to, r.ctrl, filler(g, r), keptStoresRun(g, r)]).toEqual([falls, falls ? 'UK' : undefined, 'UK', 'UK', 2]);
    }
  });

  it('L20: lost and retaken by battle: Britain fills Malta\'s stores in month 3, Italy takes it in month 4, Britain takes it back in month 5: the record names Britain, yet nothing is charged in month 7', () => {
    for (const retaken of [false, true]) {
      const g = war(true);
      const r = g.byName['Malta'];
      declareWar(g, 'ITA', 'UK');
      at(g, r, 3, 3);
      if (retaken) {
        g.turn = 4;
        capture(g, r, 'ITA');
        g.turn = 5;
        capture(g, r, 'UK');
      }
      g.turn = 7;
      expect([retaken, r.ctrl, r.since, filler(g, r), keptStoresRun(g, r)]).toEqual([retaken, 'UK', retaken ? 5 : 0, 'UK', retaken ? 0 : 2]);
    }
  });

  it('L21: no record, as in a war or a save from before it: a month mark with no record of whose stores they are charges nothing', () => {
    for (const recorded of [false, true]) {
      const g = war(true);
      const r = g.byName['Malta'];
      at(g, r, 6, 3, recorded ? 'UK' : null);
      expect([recorded, g.flags[`stores:by:${r.id}`] !== undefined, keptStoresRun(g, r), opened(g, r, 'ITA')]).toEqual([recorded, recorded, recorded ? 2 : 0, recorded ? 2 : 0]);
    }
  });

  it('L22: a load keeps the record as saved: Malta\'s stores Britain\'s of month 1 while Malta is Italy\'s, Italy\'s sea open, in month 3', () => {
    const g = war(true);
    const r = g.byName['Malta'];
    // Month 1: Britain's ships fill Malta's stores, her record written by the draw of the picture.
    computeSupply(g);
    // Month 3: Italy holds Malta, and no picture has been drawn since.
    g.turn = 3;
    capture(g, r, 'ITA');
    const text = saveGame(g);
    const back = loadGame(text)!;
    // The load drew the picture, in which Italy's ships fill the stores, and then put the save's marks back: every flag
    // is the save's.
    expect(back.flags).toEqual(JSON.parse(text).flags);
    expect([back.flags[`stores:${r.id}`], filler(back, back.regions[r.id]), filler(g, r)]).toEqual([1, 'UK', 'UK']);
    // (Drawn afresh, the picture would have written Italy's record of month 3.)
    back.supplyDirty = true;
    computeSupply(back);
    expect([back.flags[`stores:${r.id}`], filler(back, back.regions[r.id])]).toEqual([3, 'ITA']);
  });

  it('L23: a record left from a war fought under true reach, true reach then switched off: off charges nothing, neither on the card nor in the siege', () => {
    for (const on of [true, false]) {
      const g = war(true);
      const r = g.byName['Malta'];
      // Britain's stores of month 3, her record, the sea shut since; in month 6 they have run two months.
      at(g, r, 6, 3);
      if (!on) expect(setTrueReach(g, false)).toBe(true);
      expect([on, trueReach(g), filler(g, r), keptStoresRun(g, r)]).toEqual([on, on, 'UK', 2]);
      expect([on, siegeView(g, r.id)!.lines.at(-1), opened(g, r, 'ITA')]).toEqual([on,
        `Closely invested, its stores would feed the defenders for ${on ? '2 months' : '4 months'}, and for as long as ships can reach its harbour.`, on ? 2 : 0]);
    }
  });
});
