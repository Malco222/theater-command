// A lane with nothing laid weighs nothing with any staff (the design director's ruling of 12:39Z, 25 November 2026,
// part A (2), on the Pacific planner's brief batch3-lanes-brief.md).
//
// The fault they guard: the three Pacific lanes (tests/far-lanes-laid.test.ts) are laid with oil 0 and sup 0 because
// no cargo figure has been read for them. "A 0 that stands for 'not read' is not a read zero." A staff that counted
// such a lane as shipping would send Japan's boats to Hawaiian Waters to risk loss for no gain, and bind America's
// and Australia's fleets to guard nothing: the missing figure would decide the Pacific war at sea.
//
// The rule (the ruling's words): a staff counts a lane only while it carries something laid: oil, supplies, fields,
// or a convoy route of its id. Every staff, both sides, every stop. One predicate, shared: it is the navy thread's,
// in src/campaign/navy.ts (cargoLaid, read through laneCounts and laneCarries by shippingIn, lanesTo, laneUnguarded,
// laneOnBeat and fleets.ts openEscorts), and these tests read it from outside. Every row laid before these three
// carries something, so no choice on either chart moves by it. The lane stays on the pages and open to the player:
// his escort and his raiders are his own order, not a staff's count (far-lanes-laid, the Pacific fleet and the
// raiders' tests).
//
// The brief's test 7 pinned the pull a lane with nothing laid would have: Japan's Advance Submarine Fleet off to
// Hawaiian Waters at stop 3. The ruling flips it: the station is the one the staff takes with the three rows out of the
// list, read here month by month beside it.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { stopOf } from '../src/campaign/history';
import { laneCarries, navalAI, placeBoats, sail, seaRoutes, shippingIn, zoneOf } from '../src/campaign/navy';
import { RES, laneStopped } from '../src/campaign/resources';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';

/** The three rows laid with no cargo figure read, in the order they stand in the list. */
const NEW = ['america-hawaii', 'america-australia', 'britain-india'];
/** A war nobody plays on the first day's chart, far side on, at stop 3 (the stop a new war opens at). */
const grand = (): Game => startOpen(createGame(3, null));
const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const row = (id: string): (typeof RES.lanes)[number] => RES.lanes.find((l) => l.id === id)!;
/**
 * The lanes list without the three rows while `f` runs, and the rows back where they stood after. (The routes
 * cargoes travel by are kept by game, seaRoutes: every game `f` asks must be one it opens itself.)
 */
const withoutTheThree = <T>(f: () => T): T => {
  const at = RES.lanes.findIndex((l) => l.id === NEW[0]);
  const rows = RES.lanes.splice(at, NEW.length);
  try {
    expect(rows.map((l) => l.id)).toEqual(NEW);
    expect(RES.lanes.some((l) => NEW.includes(l.id))).toBe(false);
    return f();
  } finally { RES.lanes.splice(at, 0, ...rows); }
};

describe('a lane with nothing laid weighs nothing with any staff (the design director, 12:39Z of 25 November 2026)', () => {
  it("at stop 3 it pulls no boat: Japan's Advance Submarine Fleet takes the sea it takes without the three rows", () => {
    // December 1941, as the ruling names it, and then the first month of each later half-year of the record's list.
    const MONTHS = [28, 41, 47, 53, 59, 65, 71];
    /** Where Japan's staff sends the group that month, Japan at war with America and Australia, in a war opened for the asking. */
    const station = (turn: number): [number, string] => {
      const g = grand();
      expect(stopOf(g)).toBe(3);
      g.turn = turn;
      for (const foe of ['USA', 'AUS']) if (!atWar(g, 'JAP', foe)) declareWar(g, 'JAP', foe);
      placeBoats(g, 'JAP');
      const f = fleet(g, 'JAP', 'Advance Submarine Fleet');
      expect(f.ships.ss, `month ${turn}`).toBeGreaterThan(0);
      return [turn, g.seas[f.station].key];
    };
    const laid = MONTHS.map(station);
    const base = withoutTheThree(() => MONTHS.map(station));
    // (The rows are back in the list.)
    expect(RES.lanes.filter((l) => NEW.includes(l.id)).map((l) => l.id)).toEqual(NEW);
    // The ruling's month. Turn 28 is hawaii either way, so this line alone would pass even with a cargo laid on the
    // three rows; it stands only to name the month the ruling speaks of.
    expect(laid[0]).toEqual(base[0]);
    // The proof is the half-years after it: there a counted lane would pull the boats (with supplies 0.01 laid on the
    // three rows, turns 41 to 59 go to hawaii, hawaii, hawaii and calrun, by the review of 25 November 2026). The same
    // sea, month for month.
    expect(laid).toEqual(base);
  });

  it('it weighs 0 in every staff choice: no shipping for a raider to count, and no escort bound to it by need', () => {
    /** December 1941: Japan at war with America, Britain and Australia, who stand together. */
    const war = (): Game => {
      const g = grand();
      g.turn = 28;
      for (const foe of ['USA', 'UK', 'AUS']) if (!atWar(g, 'JAP', foe)) declareWar(g, 'JAP', foe);
      for (const nat of ['USA', 'UK', 'AUS']) g.nations[nat].align = 'allies';
      return g;
    };
    const g = war();
    // The navy's predicate: false for each of the three, true for every other lane of the list that is a route by sea
    // on this chart (a row with no seas is no route of the navy's and no staff can count it by sea).
    const routes = new Set(seaRoutes(g).map((r) => r.id));
    for (const l of RES.lanes) if (routes.has(l.id)) expect([l.id, laneCarries(g, l.id)]).toEqual([l.id, !NEW.includes(l.id)]);
    expect(NEW.every((id) => routes.has(id))).toBe(true);
    // And they sail: a 0 below is not a stopped lane's.
    for (const id of NEW) expect(laneStopped(g, id), id).toBeNull();
    // The seas they cross are crossed by no other route, and each feeds an enemy of Japan's.
    const seas = [...new Set(NEW.flatMap((id) => row(id).zones))];
    expect(seas).toEqual(['hawaii', 'calrun', 'california', 'line', 'samoa', 'fiji', 'tasman', 'arabian', 'cindian', 'sindian']);
    for (const k of seas) {
      const through = seaRoutes(g).filter((r) => r.zones.includes(zoneOf(g, k)));
      expect(through.length, k).toBeGreaterThan(0);
      for (const r of through) { expect(NEW, `${k}: ${r.id}`).toContain(r.id); expect(atWar(g, 'JAP', r.to), r.id).toBe(true); }
    }
    // The thickest-shipping count: nothing there for Japan's staff to send a boat after.
    const shipping = seas.map((k) => [k, shippingIn(g, 'JAP', zoneOf(g, k))]);

    // The computer's escort need. Japan's boats are sent by hand to raid a sea of each lane in turn; every navy of
    // the lanes' side is then given its month's orders by its own admirals, and none binds a fleet to a lane that
    // carries nothing. (What each fleet does instead is its admirals' business and is not pinned here.)
    const bound = (sea: string): string[] => {
      const h = war();
      placeBoats(h, 'JAP');
      const boats = fleet(h, 'JAP', 'Advance Submarine Fleet');
      boats.station = zoneOf(h, sea);
      expect(sail(h, boats, boats.station, 'raid'), sea).toBe(true);
      for (const nat of ['USA', 'UK', 'AUS', 'NZL']) navalAI(h, nat);
      return h.fleets.filter((f) => f.stance === 'escort' && NEW.includes(f.lane ?? '')).map((f) => `raiders in ${sea}: ${f.nat}'s ${f.name} escorts ${f.lane}`);
    };
    const escorts = ['hawaii', 'california', 'tasman', 'arabian'].flatMap(bound);

    // And over months of everyone's play: a war in which Japan fights America, Britain and Australia from the first
    // day, Japan's boats sent by hand into Hawaiian Waters to raid, and every navy then run by its own admirals. No
    // fleet of any side is bound to one of the three, in any month.
    const h = grand();
    for (const foe of ['USA', 'UK', 'AUS']) if (!atWar(h, 'JAP', foe)) declareWar(h, 'JAP', foe);
    for (const nat of ['USA', 'UK', 'AUS']) h.nations[nat].align = 'allies';
    expect(sail(h, fleet(h, 'JAP', '6th Fleet'), zoneOf(h, 'hawaii'), 'raid')).toBe(true);
    const months: string[] = [], t0 = h.turn;
    for (let m = 0; m < 3; m++) {
      runTurnAI(h);
      months.push(...h.fleets.filter((f) => f.stance === 'escort' && NEW.includes(f.lane ?? '')).map((f) => `month ${h.turn}: ${f.nat}'s ${f.name} escorts ${f.lane}`));
    }
    expect(h.turn).toBe(t0 + 3);

    // (Read together, so that a failure shows every count at once.)
    expect({ shipping, escorts, months }).toEqual({ shipping: seas.map((k) => [k, 0]), escorts: [], months: [] });
  }, 600000);
});
