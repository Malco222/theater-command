// The escort's radar before a boat fires (release 2, Units decide; the design director's ruling of 24 November 2026
// 16:35Z on naval-battles/convoy-r2/radar-firing-form.md, (a) on all three). The count's pack is the boats that fired
// at T2, against escorts with radar; the boats that try are the pack / 0.3, and those that fire are the tries x
// 1 / (1 + c r), r the radar-fitted escorts a convoy for each boat trying, c = 7/3 from the record's T2 ratio of about 1.
// With no radar-fitted escort every boat that tries fires. Applied once, on the pack the contest and the ceiling read;
// the escort's kills keep the count's incidents. Nothing in it reads the flag or the date.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { CAUSE, NAVY, boatCeiling, convoyCount, convoySideOf, firingShare, huntExpected, zoneOf } from '../src/campaign/navy';
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
function unread<T>(f: () => T): T {
  const keep = CAUSE.firing.t2;
  CAUSE.firing.t2 = null;
  try { return f(); } finally { CAUSE.firing.t2 = keep; }
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
    expect(s.torps).toBeCloseTo(Math.max(1, c.pack / 0.3) * boatCeiling(g, zoneOf(g, 'natl'))!, 12);
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
    // (The share is the ruled form of the escorts a convoy for each boat trying.)
    const c = convoyCount(g, atlantic)!;
    const tries = c.pack / 0.3;
    const r = (1 / most.fire! - 1) / (7 / 3);
    expect(most.fired).toBeCloseTo(tries * most.fire!, 12);
    expect(r).toBeGreaterThan(0);
    const before = at(60, 80);
    grantTech(g, 'GER', 'asvradar');
    expect(at(60, 80)).toEqual(before);
  });

  it('at T2\'s own ratio gives the count\'s pack back, so T2 is as the tons ruling fitted it', () => {
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
  it('the escort\'s kills keep the count\'s incidents (Q3 (a))', () => {
    const g = open(true);
    const ub = meet(g, 60, 30);
    const z = zoneOf(g, 'natl');
    expect(huntExpected(g, 'UK', z, [ub])).toBe(unread(() => huntExpected(g, 'UK', z, [ub])));
    expect(convoyCount(g, atlantic)).toEqual(unread(() => convoyCount(g, atlantic)));
  });

  it('unread, the pack fires as counted: the build before it', () => {
    const g = open(true);
    meet(g, 60, 30);
    unread(() => {
      const s = convoySideOf(g, 'UK', atlantic)!, c = convoyCount(g, atlantic)!;
      expect(s.fire).toBeUndefined();
      expect(s.fired).toBeUndefined();
      expect(s.torps).toBeCloseTo(Math.max(1, c.pack) * boatCeiling(g, zoneOf(g, 'natl'))!, 12);
    });
  });

  it('off, nothing is read', () => {
    const g = open(false);
    meet(g, 60, 30);
    expect(convoySideOf(g, 'UK', atlantic)).toBeUndefined();
  });
});
