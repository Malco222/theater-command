// The far side's one measure for the boat (the design director's 18:03Z and 18:04Z of 22 November 2026; the
// historian's paper sub-patrols-japan-oil.md, S11 and section 4).
//
// The fault it mends: every boat on hand in a far sea raided, and every boat on hand was hunted. The record kept
// about one boat in three on station; the rest were on passage or refitting. Counting them all, the hunt rule sank
// America's boats at half again the record's count while their raid was already a quarter for the torpedoes.
//
// The rule: of a fleet's submarines in a far sea, the boats out on patrol this month are the record's share by navy
// and month (navy.json beats 'patrol'), and that one number is read wherever a boat at sea is counted: the raid on
// the cargoes, the hunt, the threat to an unscreened fleet, the oil book's ships at sea, and the weight that holds a
// sea. The beats' rows and the pools stay boats on hand. Every state here is set by hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { createGame, declareWar } from '../src/campaign/game';
import { fleetCard, seaOptions } from '../src/campaign/fleets';
import { FLEETS, NAVY, boatsOut, fit, fleetPower, huntShareSunk, huntSubs, huntersIn, quality, seaSide, subsWorth, torpedoRisk, zoneOf } from '../src/campaign/navy';
import { RES, stores } from '../src/campaign/resources';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game, Ships } from '../src/campaign/types';
import { FAR_SEAS_FIRST } from '../src/campaign/world';
import farBooks from '../data/campaign/far-books.json';

const grand = (seed = 3): Game => startOpen(createGame(seed, null));
const named = (g: Game, nat: string, name: string): Fleet => g.fleets.find((x) => x.nat === nat && x.name === name)!;
/** A fleet of these ships alone, set by hand in one sea. */
const squadron = (g: Game, f: Fleet, ships: Partial<Ships>, sea: string, stance: Fleet['stance']): Fleet => {
  f.ships = { bb: 0, cv: 0, ca: 0, dd: 0, ss: 0, ...ships };
  f.zone = zoneOf(g, sea); f.stance = stance; f.ready = 1;
  return f;
};
/** The same sums with the measure struck out: every boat on hand. */
const allBoats = <T>(nat: string, fn: () => T): T => {
  const B = NAVY.beats![nat], keep = B.patrol;
  delete B.patrol;
  try { return fn(); } finally { B.patrol = keep; }
};
const war = (g: Game): Game => { declareWar(g, 'JAP', 'USA'); return g; };

describe("the far side's one measure for the boat", () => {
  it('one boat in three is out for America, Britain and the Netherlands, and Japan\'s own share for Japan, in the far side\'s seas alone', () => {
    const rows = (farBooks as any)['navy.json'].beats;
    for (const nat of ['USA', 'NED', 'UK']) expect(rows[nat].patrol, nat).toEqual([[0, 1, 3]]);
    // Japan's, like for like with the record (the director's 21:04Z and 21:23Z): a third of its 40 to 45 operational
    // boats, of all 64 on hand, and the middle of the counts is laid: 85 boats out of 384, 0.221.
    expect(rows.JAP.patrol).toEqual([[0, 85, 384]]);
    expect(85 / 384).toBeCloseTo((40 / 64 / 3 + 45 / 64 / 3) / 2, 12);
    {
      const j = war(grand());
      const boats = squadron(j, named(j, 'JAP', '6th Fleet'), { ss: 384 }, 'schina', 'raid');
      expect(boatsOut(j, boats)).toBeCloseTo(85, 12);
    }
    expect(Object.keys(NAVY.beats!).filter((nat) => NAVY.beats![nat].patrol).sort()).toEqual(['JAP', 'NED', 'UK', 'USA']);
    const g = war(grand());
    const f = squadron(g, named(g, 'USA', 'Asiatic Fleet'), { ss: 60 }, 'schina', 'raid');
    expect(f.zone).toBeGreaterThanOrEqual(FAR_SEAS_FIRST);
    expect(boatsOut(g, f)).toBe(20);
    // The boats on hand are untouched: the measure moves no boat.
    expect(f.ships.ss).toBe(60);
    // In the old chart's seas every boat counts, as it always did there.
    f.zone = zoneOf(g, 'natl');
    expect(f.zone).toBeLessThan(FAR_SEAS_FIRST);
    expect(boatsOut(g, f)).toBe(60);
    // A navy with no row: Germany's measure, every boat on hand, wherever it lies.
    const u = g.fleets.find((x) => x.nat === 'GER' && x.ships.ss > 0)!;
    u.zone = zoneOf(g, 'schina');
    expect(boatsOut(g, u)).toBe(u.ships.ss);
  });

  it('the row in force is the latest begun, so the record\'s share can change by month', () => {
    const g = war(grand());
    const f = squadron(g, named(g, 'USA', 'Asiatic Fleet'), { ss: 60 }, 'schina', 'raid');
    const P = NAVY.beats!.USA.patrol!;
    P.push([41, 1, 2]);
    try {
      g.turn = 40;
      expect(boatsOut(g, f)).toBe(20);
      g.turn = 41;
      expect(boatsOut(g, f)).toBe(30);
    } finally { P.pop(); }
    expect(P).toEqual([[0, 1, 3]]);
  });

  it('the boats out are the boats hunted: the same share of fewer boats, and none sunk that was not out', () => {
    const g = war(grand());
    const f = squadron(g, named(g, 'USA', 'Asiatic Fleet'), { ss: 60 }, 'schina', 'raid');
    const hunters = squadron(g, named(g, 'JAP', 'Combined Fleet'), { dd: 40 }, 'schina', 'hunt');
    const z = f.zone;
    const w = huntersIn(g, 'JAP', z).weight;
    expect(w).toBeGreaterThan(0);
    const share = huntShareSunk(w, 20), want = 20 * share;
    // The share is worked on the boats out, so it is the share twenty boats would suffer, never sixty's.
    expect(share).toBeGreaterThan(huntShareSunk(w, 60));
    const sunk = huntSubs(g, 'JAP')[z] ?? 0;
    expect(sunk).toBeGreaterThanOrEqual(Math.floor(want));
    expect(sunk).toBeLessThanOrEqual(Math.ceil(want));
    expect(f.ships.ss).toBe(60 - sunk);
    expect(f.lost.ss).toBe(sunk);
    // Counting every boat the rule expected this many in the month: more than the boats out could lose.
    expect(60 * huntShareSunk(w, 60)).toBeGreaterThan(want);
    expect(hunters.ships.dd).toBeLessThanOrEqual(40);
  });

  it('the boats out are the boats that threaten an unscreened fleet, and that weigh in holding a sea', () => {
    const g = war(grand());
    const f = squadron(g, named(g, 'USA', 'Asiatic Fleet'), { ss: 60 }, 'schina', 'raid');
    const heavy = squadron(g, named(g, 'JAP', 'Combined Fleet'), { bb: 4 }, 'schina', 'patrol');
    const z = f.zone;
    const each = quality(g, 'USA') * fit(g, f);
    expect(torpedoRisk(g, heavy, z)).toBeCloseTo(Math.min(FLEETS.screen.most, 20 * each * FLEETS.screen.risk), 12);
    expect(allBoats('USA', () => torpedoRisk(g, heavy, z))).toBeCloseTo(Math.min(FLEETS.screen.most, 60 * each * FLEETS.screen.risk), 12);
    // A boat at its base holds no sea: the fleet weighs what twenty boats weigh, and so does America's side of the sea.
    const sixty = allBoats('USA', () => fleetPower(g, f));
    expect(fleetPower(g, f)).toBeCloseTo(sixty / 3, 12);
    expect(seaSide(g, z, 'JAP', true).ships).toBeCloseTo(sixty / 3, 12);
    // Surface ships are counted as they were.
    f.ships = { bb: 2, cv: 0, ca: 4, dd: 8, ss: 0 };
    expect(fleetPower(g, f)).toBe(allBoats('USA', () => fleetPower(g, f)));
  });

  it('the boats out burn as ships at sea in the oil book, and the rest at the harbour\'s rate', () => {
    const g = war(grand());
    const N = RES.oil.need;
    // (Japan's share is 85 boats out of 384 on hand: 384 boats, so that the counts are whole.)
    const f = squadron(g, named(g, 'JAP', '6th Fleet'), { ss: 384 }, 'schina', 'raid');
    const spend = (key: string): { amount: number; count: number } => { const l = stores(g, 'JAP')!.oil.spend.find((x) => x.key === key); return { amount: l?.amount ?? 0, count: l?.count ?? 0 }; };
    const sea = spend('sea'), port = spend('port');
    const was = allBoats('JAP', () => ({ sea: spend('sea'), port: spend('port') }));
    // The 299 boats not out leave the count of ships at sea for the count in harbour.
    expect(was.sea.count - sea.count).toBe(299);
    expect(port.count - was.port.count).toBe(299);
    // The ships at sea burn what they would with 85 boats in that fleet and every boat counted.
    const out = allBoats('JAP', () => { f.ships.ss = 85; const a = spend('sea').amount; f.ships.ss = 384; return a; });
    expect(sea.amount).toBeCloseTo(out, 9);
    expect(sea.amount).toBeLessThan(was.sea.amount);
    // And the boats at the base burn at the harbour's rate: a quarter of what they burned at sea.
    expect(N.port).toBe(0.25);
    expect(port.amount - was.port.amount).toBeCloseTo((was.sea.amount - sea.amount) * N.port, 9);
    // In harbour the fleet burns as it always did.
    f.stance = 'port';
    expect(spend('sea').amount).toBeCloseTo(allBoats('JAP', () => spend('sea').amount), 12);
    expect(spend('port').amount).toBeCloseTo(allBoats('JAP', () => spend('port').amount), 12);
  });

  it('the cards count the boats out: the player\'s own fleet, and the enemy boats at work in a sea', () => {
    const g = war(grand());
    const f = squadron(g, named(g, 'USA', 'Asiatic Fleet'), { ss: 60 }, 'schina', 'raid');
    expect(fleetCard(g, f).boatsOut).toBe(20);
    // The torpedoes' quarter multiplies on top of the measure: it is another matter (ruling 194(f)).
    expect(subsWorth(g, f)).toBe(0.25);
    // In harbour, and in the old chart's seas, the card says nothing of it.
    f.stance = 'port';
    expect(fleetCard(g, f).boatsOut).toBeUndefined();
    f.stance = 'raid'; f.zone = zoneOf(g, 'natl');
    expect(fleetCard(g, f).boatsOut).toBeUndefined();
    // Japan's own card for a sea in its fleet's reach: twenty enemy submarines at work, never sixty; one where a part of a boat is out.
    const jp = named(g, 'JAP', 'Combined Fleet');
    jp.ships = { bb: 0, cv: 0, ca: 0, dd: 20, ss: 0 }; jp.ready = 1;
    const z = seaOptions(g, jp, null).find((o) => o.zone >= FAR_SEAS_FIRST)!.zone;
    f.zone = z;
    const here = (): number => seaOptions(g, jp, null).find((o) => o.zone === z)!.subs;
    expect(here()).toBe(20);
    f.ships.ss = 2;
    expect(here()).toBe(1);
    f.ships.ss = 0;
    expect(here()).toBe(0);
  });
});
