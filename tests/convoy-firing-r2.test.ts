// The escort's radar before a boat fires (release 2, Units decide; the design director's ruling of 24 November 2026
// 16:35Z on naval-battles/convoy-r2/radar-firing-form.md, (a) on all three). The count's pack is the boats that fired
// at T2, against escorts with radar; the boats that try are the pack / 0.3, and those that fire are the tries x
// 1 / (1 + c r), r the radar-fitted escorts a convoy for each boat trying, c = 7/3 from the record's T2 ratio of about 1.
// With no radar-fitted escort every boat that tries fires. Applied once, on the pack the contest and the ceiling read;
// the escort's kills keep the count's incidents. Nothing in it reads the flag or the date.
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { CAUSE, FLEETS, NAVY, boatCeiling, convoyCount, convoyMeeting, convoySideOf, firingShare, huntExpected, zoneOf } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { grantTech } from '../src/campaign/tech';
import type { Fleet, Game, Ships } from '../src/campaign/types';

const atlantic = NAVY.convoys.find((r) => r.id === 'atlantic')!;

function open(on: boolean, seed = 1): Game {
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
// (Radar alone, as first ruled: the eye term is unread here, and tested in convoy-load-r2.)
const eyeKept = CAUSE.firing.eye!.effective41;
beforeAll(() => { CAUSE.firing.eye!.effective41 = null; });
afterAll(() => { CAUSE.firing.eye!.effective41 = eyeKept; });
/** A firing boat's ceiling in the North Atlantic at the route's firing share, the next night read as laid (tests/convoy-regain-r2). */
const ceil = (g: Game, fire?: number): number => boatCeiling(g, zoneOf(g, 'natl'), fire ?? 1, { route: 'atlantic', nat: 'GER' })!;
function unread<T>(f: () => T): T {
  const keep = CAUSE.firing.t2;
  CAUSE.firing.t2 = null;
  try { return f(); } finally { CAUSE.firing.t2 = keep; }
}
/** As first built, before the escorts' cycle and the screen's stations (tested in convoy-screen-r2): escorts bound over convoys sailed. */
function firstBuilt<T>(f: () => T): T {
  const dd = CAUSE.station.dd!, sc = CAUSE.firing.screen!;
  const keep = { crossing: dd.crossing, ships: sc.shipsAColumn };
  dd.crossing = null; sc.shipsAColumn = null;
  try { return f(); } finally { dd.crossing = keep.crossing; sc.shipsAColumn = keep.ships; }
}

describe('the figures', () => {
  it('lays 0.3 of the boats that try firing at T2\'s ratio of about one radar-fitted escort to each boat trying: c = 7/3', () => {
    expect(CAUSE.firing).toMatchObject({ t2: 0.3, rT2: 1 });
    expect(firingShare(1, 1)).toBeCloseTo(0.3, 12);
    expect(firingShare(6.5, 6.5)).toBeCloseTo(0.3, 12);
    expect(firingShare(1, 1 / 0.3)).toBeCloseTo(1 / (1 + (7 / 3) * 0.3), 12);
    // With no radar-fitted escort every boat that tries fires; more of them a boat, fewer fire.
    expect(firingShare(0, 5)).toBe(1);
    expect(firingShare(2, 1)).toBeLessThan(firingShare(1, 1)!);
  });

  it('the check, never fitted: HX 229/SC 122, boats about 2:1 over the escorts, lets about 0.46 of them fire', () => {
    expect(firingShare(1, 2)).toBeCloseTo(0.4615, 3);
  });
});

describe('a convoy attacked', () => {
  it('with no radar-fitted escort, every boat that tries fires: the pack is the count\'s / 0.3, and the ceiling reads it', () => {
    const g = open(true);
    meet(g, 60, 30);
    const c = convoyCount(g, atlantic)!, s = convoySideOf(g, 'UK', atlantic)!, was = unread(() => convoySideOf(g, 'UK', atlantic)!);
    expect(s.fire).toBe(1);
    expect(s.fired).toBeCloseTo(c.pack / 0.3, 12);
    expect(s.P).toBeCloseTo(was.P / 0.3, 12);
    expect(s.torps).toBeCloseTo(Math.max(1, c.pack / 0.3) * ceil(g, s.fire), 12);
    expect(s.sunk).toBeGreaterThan(was.sunk);
  });

  it('with radar-fitted escorts fewer fire, the more escorts a boat trying the fewer, and the boats\' own radar does nothing', () => {
    const g = open(true);
    grantTech(g, 'UK', 'asvradar');
    const at = (ss: number, dd: number): NonNullable<ReturnType<typeof convoySideOf>> => { meet(g, ss, dd); return convoySideOf(g, 'UK', atlantic)!; };
    const few = at(60, 20), more = at(60, 80), most = at(60, 320);
    expect(few.fire!).toBeLessThan(1);
    expect(more.fire!).toBeLessThan(few.fire!);
    expect(most.fire!).toBeLessThan(more.fire!);
    const c = convoyCount(g, atlantic)!;
    expect(most.fired).toBeCloseTo((c.pack / 0.3) * most.fire!, 12);
    const before = at(60, 80);
    grantTech(g, 'GER', 'asvradar');
    expect(at(60, 80)).toEqual(before);
  });

  it('r is the ruled one: the radar-fitted escorts at their share on the route, over the convoys sailed, for each boat trying', () => {
    const g = open(true);
    grantTech(g, 'UK', 'asvradar');
    meet(g, 60, 80);
    // (A second escort, patrolling one of the route's seas: distant cover, at 'loose' of its destroyers.)
    sea(g, 'Dover Fleet', 'natl', { dd: 40 }, 'patrol');
    firstBuilt(() => {
      const c = convoyCount(g, atlantic)!, s = convoySideOf(g, 'UK', atlantic)!;
      const r = ((80 + 40 * FLEETS.escort.loose) / c.sailed) / (c.pack / 0.3);
      expect(FLEETS.escort.loose).toBeLessThan(1);
      expect(s.fire).toBeCloseTo(1 / (1 + (7 / 3) * r), 12);
    });
  });

  it('with radar, the contest and the ceiling both read the fired pack, the share applied once', () => {
    const g = open(true);
    grantTech(g, 'UK', 'asvradar');
    meet(g, 60, 80);
    const c = convoyCount(g, atlantic)!, s = convoySideOf(g, 'UK', atlantic)!, was = unread(() => convoySideOf(g, 'UK', atlantic)!);
    expect(s.fire!).toBeLessThan(1);
    expect(s.fired).toBeCloseTo((c.pack / 0.3) * s.fire!, 12);
    expect(s.P).toBeCloseTo((was.P * s.fired!) / c.pack, 12);
    expect(s.torps).toBeCloseTo(Math.max(1, s.fired!) * ceil(g, s.fire), 12);
  });

  it('the card says the boats that fire, so its boats each and its torpedoes agree', () => {
    const g = open(true);
    grantTech(g, 'UK', 'asvradar');
    meet(g, 60, 80);
    const s = convoySideOf(g, 'UK', atlantic)!, k = convoyMeeting(g, 'UK', atlantic).count!;
    expect(k.fired).toBeCloseTo(s.fired!, 2);
    expect(k.torps).toBeCloseTo(Math.max(1, k.fired!) * ceil(g, s.fire), 1);
    // (Unread, the card carries the count's pack and no fired, as before.)
    expect(unread(() => convoyMeeting(g, 'UK', atlantic).count!.fired)).toBeUndefined();
  });

  it('at T2\'s own ratio gives the count\'s pack back (the pack only: the ships sunk there are the ceiling\'s, convoy-load-r2)', () => {
    // (r = 1 radar-fitted escort for each boat trying: the tries x 0.3 are the count's pack.)
    for (const pack of [0.5, 2, 3.5]) expect((pack / 0.3) * firingShare(pack / 0.3, pack / 0.3)!).toBeCloseTo(pack, 12);
  });

  it('a navy that fits radar to its escorts earns it, whoever it is: an escort without the kit lets every boat fire', () => {
    const g = open(true);
    meet(g, 60, 80);
    const without = convoySideOf(g, 'UK', atlantic)!;
    grantTech(g, 'UK', 'asvradar');
    const withRadar = convoySideOf(g, 'UK', atlantic)!;
    expect(without.fire).toBe(1);
    expect(withRadar.fire!).toBeLessThan(1);
    expect(withRadar.sunk).toBeLessThan(without.sunk);
  });
});

describe('what it leaves alone', () => {
  it('with the escorts\' losses form unread, the escort\'s kills keep the count\'s incidents (Q3 (a)); laid, they read the same contest (convoy-escorts-r2)', () => {
    const g = open(true);
    grantTech(g, 'UK', 'asvradar');
    const ub = meet(g, 60, 30);
    const z = zoneOf(g, 'natl');
    const keep = CAUSE.escorts;
    CAUSE.escorts = null;
    try {
      expect(huntExpected(g, 'UK', z, [ub])).toBe(unread(() => huntExpected(g, 'UK', z, [ub])));
    } finally { CAUSE.escorts = keep; }
    expect(huntExpected(g, 'UK', z, [ub])).not.toBe(unread(() => huntExpected(g, 'UK', z, [ub])));
    expect(convoyCount(g, atlantic)).toEqual(unread(() => convoyCount(g, atlantic)));
  });

  it('unread, the pack fires as counted: the build before it', () => {
    const g = open(true);
    meet(g, 60, 30);
    unread(() => {
      const s = convoySideOf(g, 'UK', atlantic)!, c = convoyCount(g, atlantic)!;
      expect(s.fire).toBeUndefined();
      expect(s.fired).toBeUndefined();
      expect(s.torps).toBeCloseTo(Math.max(1, c.pack) * ceil(g, s.fire), 12);
    });
  });

  it('off, nothing is read', () => {
    const g = open(false);
    meet(g, 60, 30);
    expect(convoySideOf(g, 'UK', atlantic)).toBeUndefined();
  });
});
