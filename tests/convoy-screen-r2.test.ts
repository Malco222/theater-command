// The escorts with a convoy, for the radar before a boat fires (release 2, Units decide; the design director's ruling of
// 24 November 2026 16:58Z on naval-battles/convoy-r2/screen-cycle-form.md). Q1 (d): the escorts at sea with the convoys
// at once, from the Mid-Ocean Escort Force's cycle (19 of 33 days with a convoy, a third of strength out of service),
// over the convoys at sea at once (a 9.5-day crossing). Q2 (b): the close screen's stations, 6 + columns / 3, the
// columns the convoy's ships / 3.5; escorts on stations are the lesser of those with the convoy and the stations, times
// the radar-fitted share. Q3 (a), Q4 (a): escorts beyond the stations add no frustration and still count in E.
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { CAUSE, NAVY, convoyCount, convoySideOf, escortsAtSea, screenStations, zoneOf } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { grantTech } from '../src/campaign/tech';
import type { Fleet, Game, Ships } from '../src/campaign/types';

const atlantic = NAVY.convoys.find((r) => r.id === 'atlantic')!;
const C = 7 / 3;

// (Radar alone, as this build was ruled: the eye term is unread here, and tested in convoy-load-r2.)
const eyeKept = CAUSE.firing.eye!.effective41;
beforeAll(() => { CAUSE.firing.eye!.effective41 = null; });
afterAll(() => { CAUSE.firing.eye!.effective41 = eyeKept; });
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
function meet(g: Game, ss: number, dd: number): void {
  sea(g, 'Home Fleet', 'wapp', { dd }, 'escort', 'atlantic');
  sea(g, 'U-boat Fleet', 'natl', { ss }, 'raid');
}
/** r, the radar-fitted escorts on stations for each boat trying, read back from the share. */
const rOf = (fire: number): number => (1 / fire - 1) / C;
/** With the escorts' losses form unread (convoycause.json 'escorts' null): E and k as the tons ruling set them. */
function withoutEscorts<T>(f: () => T): T {
  const keep = CAUSE.escorts;
  CAUSE.escorts = null;
  try { return f(); } finally { CAUSE.escorts = keep; }
}
/** Run with some of the ruling's figures unread. */
function without<T>(what: { cycle?: boolean; screen?: boolean }, f: () => T): T {
  const dd = CAUSE.station.dd!, sc = CAUSE.firing.screen!;
  const keep = { crossing: dd.crossing, ships: sc.shipsAColumn };
  if (what.cycle) dd.crossing = null;
  if (what.screen) sc.shipsAColumn = null;
  try { return f(); } finally { dd.crossing = keep.crossing; sc.shipsAColumn = keep.ships; }
}

describe('the figures', () => {
  it('lays the Mid-Ocean Escort Force\'s cycle: 0.38 of the escorts bound to a route at sea with a convoy, a 9.5-day crossing', () => {
    expect(CAUSE.station.dd).toEqual({ convoy: 19, turnaround: 6, refit: 8, unavailable: 0.333, crossing: 9.5 });
    expect(escortsAtSea()).toBeCloseTo((19 / 33) * (1 - 0.333), 12);
    expect(escortsAtSea()!).toBeCloseTo(0.38, 2);
  });

  it('lays the screen\'s stations by the convoy\'s front: 6 + columns / 3, a column 3.5 ships, at most 19 columns', () => {
    expect(CAUSE.firing.screen).toEqual({ stations: 6, columnsAStation: 3, shipsAColumn: 3.5, maxColumns: 19 });
    expect(screenStations(atlantic)).toBeCloseTo(6 + 44 / 3.5 / 3, 12);
    // (The historian's checks: 35 ships about 9, 45 about 10, 60 about 12; HX 300's 166 ships stop at 19 columns.)
    const ships = CAUSE.count.ships;
    const keep = ships.atlantic;
    try {
      for (const [n, want] of [[35, 9], [45, 10], [60, 12]] as const) { ships.atlantic = n; expect(Math.abs(screenStations(atlantic)! - want)).toBeLessThan(0.7); }
      ships.atlantic = 166;
      expect(screenStations(atlantic)).toBeCloseTo(6 + 19 / 3, 12);
    } finally { ships.atlantic = keep; }
  });
});

describe('the escorts at sea with the convoys at once (Q1 (d))', () => {
  it('scale the escorts a convoy by the share at sea over the convoys at sea at once: 0.38 x 30 / 9.5 of the first build\'s', () => {
    const g = open(true);
    grantTech(g, 'UK', 'asvradar');
    meet(g, 60, 20);
    // (A thin screen, under its stations, so only the cycle acts.)
    const cycled = convoySideOf(g, 'UK', atlantic)!, first = without({ cycle: true }, () => convoySideOf(g, 'UK', atlantic)!);
    expect(cycled.fire!).toBeLessThan(1);
    expect(rOf(cycled.fire!) / rOf(first.fire!)).toBeCloseTo((escortsAtSea()! * 30) / 9.5, 9);
  });

  it('read nothing while unread: the first build, escorts bound over convoys sailed a month', () => {
    const g = open(true);
    grantTech(g, 'UK', 'asvradar');
    meet(g, 60, 80);
    const a = without({ cycle: true, screen: true }, () => convoySideOf(g, 'UK', atlantic)!);
    const keep = { dd: CAUSE.station.dd, sc: CAUSE.firing.screen };
    delete CAUSE.station.dd; delete CAUSE.firing.screen;
    try { expect(convoySideOf(g, 'UK', atlantic)).toEqual(a); } finally { CAUSE.station.dd = keep.dd; CAUSE.firing.screen = keep.sc; }
  });
});

describe('the screen\'s stations (Q2 (b), Q3 (a), Q4 (a))', () => {
  it('cap the escorts that frustrate at the stations: past them, more escorts let as many boats fire', () => {
    const g = open(true);
    grantTech(g, 'UK', 'asvradar');
    meet(g, 60, 320);
    const many = convoySideOf(g, 'UK', atlantic)!;
    meet(g, 60, 640);
    const more = convoySideOf(g, 'UK', atlantic)!;
    const tries = convoyCount(g, atlantic)!.pack / 0.3;
    expect(many.fire).toBeCloseTo(1 / (1 + (C * screenStations(atlantic)!) / tries), 12);
    expect(more.fire).toBeCloseTo(many.fire!, 12);
    // (Q4 (a): while the escorts' losses form is unread, the escorts beyond the stations still count in the escort's
    // strength, so the contest still feels them.)
    const unreadE = (dd: number): ReturnType<typeof convoySideOf> => { meet(g, 60, dd); return withoutEscorts(() => convoySideOf(g, 'UK', atlantic)); };
    const manyU = unreadE(320)!, moreU = unreadE(640)!;
    expect(moreU.E).toBeGreaterThan(manyU.E);
    expect(moreU.sunk).toBeLessThanOrEqual(manyU.sunk);
    // (With it laid, as shipped, they hunt as spare ships and E is the escorts on the stations alone: its Q5,
    // convoy-escorts-r2.)
    expect(more.E).toBeCloseTo(screenStations(atlantic)!, 12);
    expect(many.E).toBeCloseTo(more.E, 12);
    meet(g, 60, 640);
    // (Uncapped, those escorts would have frustrated far more.)
    expect(without({ screen: true }, () => convoySideOf(g, 'UK', atlantic)!).fire!).toBeLessThan(more.fire!);
  });

  it('a thin screen fills fewer stations: under them the cap does nothing', () => {
    const g = open(true);
    grantTech(g, 'UK', 'asvradar');
    meet(g, 60, 6);
    const s = convoySideOf(g, 'UK', atlantic)!;
    expect(s.fire!).toBeLessThan(1);
    expect(s).toEqual(without({ screen: true }, () => convoySideOf(g, 'UK', atlantic)!));
  });

  it('with the eye term unread, escorts without radar fill stations but frustrate nothing', () => {
    const g = open(true);
    meet(g, 60, 320);
    expect(convoySideOf(g, 'UK', atlantic)!.fire).toBe(1);
  });
});

describe('off', () => {
  it('reads nothing', () => {
    const g = open(false);
    meet(g, 60, 320);
    expect(convoySideOf(g, 'UK', atlantic)).toBeUndefined();
  });
});
