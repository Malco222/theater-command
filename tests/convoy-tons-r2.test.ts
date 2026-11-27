// Tons counted from the convoys' side (release 2, Units decide; the design director's tons ruling of 24 November 2026
// 14:04Z on naval-battles/convoy-r2/tons-form.md). A convoy the boats attack loses its ships x P / (P + k E): P the
// pack's raid strength (the boats a convoy attacked, each with its kit, fit and the air over it, a boat on station
// counted at 1), E the convoy's share of the route's escort, k refit once at OEG T2's window. Tons = convoys attacked x
// ships sunk x GRT, both ways; the share lost is the share of the ships sunk. The month's swing acts on the ships sunk a
// convoy attacked, never past its ships. A surface raider or a fortress astride the route keeps the lanes' form, and the
// lanes' form keeps its boat weight on station (3.5) wherever it still counts boats. The boat ceiling (what a pack's
// torpedoes sink on an average night) is unread here, so these test the contest itself; its own tests are
// convoy-ceiling-r2. Where the ceiling holds (weak escorts, no aircraft with kit), the escort and the pack's size do not
// move the ships sunk a boat; there these claims are the contest's, not the game's.
import { afterAll, afterEach, beforeAll, describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { CAUSE, NAVY, convoyCount, convoyField, convoyMeeting, convoySideOf, convoyWar, fit, laneLoss, navalUpkeep, seaCover, zoneOf } from '../src/campaign/navy';
import type { ConvoyNights } from '../src/campaign/navy';
import { convoyTicket } from '../src/battle/convoy/ticket';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { grantTech } from '../src/campaign/tech';
import type { Fleet, Game, Ships } from '../src/campaign/types';

const atlantic = NAVY.convoys.find((r) => r.id === 'atlantic')!;
const libya = NAVY.convoys.find((r) => r.id === 'libya')!;

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
function withRows<T>(set: () => void, put: () => void, f: () => T): T { set(); try { return f(); } finally { put(); } }
/** Runs with the sinking figure unread: the game before the tons ruling (94f4e9df's Units decide game). */
const unread = <T>(f: () => T): T => {
  const keep = CAUSE.count.sink;
  return withRows(() => { CAUSE.count.sink = null; }, () => { CAUSE.count.sink = keep; }, f);
};
/** A pack and an escort on the Atlantic route. */
function meet(g: Game, ss: number, dd: number, zone = 'natl'): Fleet {
  sea(g, 'Home Fleet', 'wapp', { dd }, 'escort', 'atlantic');
  return sea(g, 'U-boat Fleet', zone, { ss }, 'raid');
}
/** A night the player fought on the Atlantic route, at this swing, booking no fleet's losses. */
const night = (swing: number): ConvoyNights => new Map([['atlantic', { fought: { swing, ss: {}, dd: {} }, ss: new Set<number>(), dd: new Set<number>(), clamped: false }]]);

afterEach(() => { convoyField.fought = undefined; });
// (The contest itself: the boat ceiling and the escort's radar before a boat fires are unread here, and tested in
// convoy-ceiling-r2 and convoy-firing-r2. E and k are the tons ruling's: the escorts' losses form, which moves E to the
// escorts on the screen's stations and sets k again on it (its Q5), is unread here, and tested in convoy-escorts-r2.)
const ceilingKept = CAUSE.ceiling.inside, firingKept = CAUSE.firing.t2, eyeKept = CAUSE.firing.eye!.effective41, escortsKept = CAUSE.escorts;
beforeAll(() => { CAUSE.ceiling.inside = null; CAUSE.firing.t2 = null; CAUSE.firing.eye!.effective41 = null; CAUSE.escorts = null; });
afterAll(() => { CAUSE.ceiling.inside = ceilingKept; CAUSE.firing.t2 = firingKept; CAUSE.firing.eye!.effective41 = eyeKept; CAUSE.escorts = escortsKept; });

describe('the figures', () => {
  it('lays k at 0.137: at T2\'s window 2 boats sink 3.75 ships a convoy attacked, inside T2\'s 3.2-4.4', () => {
    expect(CAUSE.count.sink).toBe(0.137);
    // (The window's own figures in six wars: a boat's raid strength on station 0.298, a convoy's escort worth 46.5.)
    const w = 0.298, E = 46.5, k = CAUSE.count.sink!;
    const sunk = (44 * 2 * w) / (2 * w + k * E);
    expect(sunk).toBeGreaterThan(3.2);
    expect(sunk).toBeLessThan(4.4);
    expect(sunk).toBeCloseTo(3.75, 1);
  });

  it('the lanes\' form keeps its boat weight on station, 3.5; the convoys\' side counts a boat on station at 1', () => {
    expect(CAUSE.station.raid).toBe(3.5);
    const g = open(true);
    meet(g, 60, 30);
    const m = convoyMeeting(g, 'UK', atlantic), c = convoyCount(g, atlantic)!, s = convoySideOf(g, 'UK', atlantic)!;
    // Boats only: the lanes' raid is the boats on station at 3.5, P a pack of them at 1.
    expect(s.P / c.pack).toBeCloseTo(m.raid / (CAUSE.station.raid! * c.boats), 9);
  });
});

describe('a convoy attacked', () => {
  it('loses its ships x P / (P + k E), and the share lost is the share of the ships sunk, both ways', () => {
    const g = open(true);
    meet(g, 60, 30);
    const c = convoyCount(g, atlantic)!;
    const s = convoySideOf(g, 'UK', atlantic)!;
    const m = convoyMeeting(g, 'UK', atlantic);
    expect(s.E).toBeCloseTo(m.G / c.sailed, 9);
    expect(s.P).toBeGreaterThan(0);
    expect(s.sunk).toBeCloseTo((44 * s.P) / (s.P + CAUSE.count.sink! * s.E), 9);
    expect(s.L).toBeCloseTo((c.attacked * s.sunk) / (c.sailed * 44), 9);
    expect(m.L).toBeCloseTo(s.L, 12);
    // Tons are the convoys attacked x the ships sunk x GRT, both ways.
    expect(m.tons).toBe(Math.round((c.attacked * s.sunk * 5200) / 1000) * 1000);
    expect(m.tons).toBe(Math.round((2 * atlantic.tons * m.L) / 1000) * 1000);
  });

  it('P is the pack times a boat\'s raid strength on station', () => {
    const g = open(true);
    meet(g, 40, 30);
    const a = convoySideOf(g, 'UK', atlantic)!, ca = convoyCount(g, atlantic)!;
    meet(g, 80, 30);
    const b = convoySideOf(g, 'UK', atlantic)!, cb = convoyCount(g, atlantic)!;
    expect(a.P / ca.pack).toBeCloseTo(b.P / cb.pack, 9);
  });

  it('P carries a boat\'s fit and the air over it: aircraft with kit cut it to 0.34, a tired pack by its fit', () => {
    const bare = open(true);
    meet(bare, 40, 30, 'irish');
    const covered = loadGame(saveGame(bare))!;
    for (const id of ['sonar', 'asvradar']) grantTech(covered, 'UK', id);
    expect(seaCover(covered, 'UK', zoneOf(covered, 'irish')).share).toBeGreaterThan(0.99);
    const p0 = convoySideOf(bare, 'UK', atlantic)!.P, p1 = convoySideOf(covered, 'UK', atlantic)!.P;
    expect(p1 / p0).toBeCloseTo(CAUSE.cover, 9);
    const tired = loadGame(saveGame(bare))!;
    const u = tired.fleets.find((f) => f.name === 'U-boat Fleet')!;
    const f1 = fit(tired, u);
    u.ready = 0.4;
    expect(convoySideOf(tired, 'UK', atlantic)!.P / p0).toBeCloseTo(fit(tired, u) / f1, 9);
  });

  it('the contest: bigger packs sink more a convoy, and less a boat (under the ceiling, each boat sinks the same: convoy-ceiling-r2)', () => {
    const g = open(true);
    let sunk = 0, each = Infinity;
    for (const n of [20, 40, 80, 160, 320]) {
      meet(g, n, 30);
      const s = convoySideOf(g, 'UK', atlantic)!, c = convoyCount(g, atlantic)!;
      expect(s.sunk).toBeGreaterThan(sunk);
      expect(s.sunk / c.pack).toBeLessThan(each);
      expect(s.sunk).toBeLessThan(44);
      sunk = s.sunk; each = s.sunk / c.pack;
    }
  });

  it('the contest: more escorts sink fewer (where the ceiling holds, they do not: convoy-ceiling-r2)', () => {
    const g = open(true);
    let last = Infinity;
    for (const dd of [10, 20, 40, 80]) {
      meet(g, 80, dd);
      const s = convoySideOf(g, 'UK', atlantic)!;
      expect(s.sunk).toBeLessThan(last);
      last = s.sunk;
    }
  });

  it('nothing in it reads the date: the same table in 1939 and 1944 sinks the same', () => {
    // (A boat's raid strength still carries its navy's quality, NAVY.quality, until the crews' store replaces it: audit #6, #8.)
    const a = open(true);
    meet(a, 60, 30);
    const b = loadGame(saveGame(a))!;
    b.turn += 60;
    expect(convoySideOf(b, 'UK', atlantic)).toEqual(convoySideOf(a, 'UK', atlantic));
  });
});

describe('the swing and the cap', () => {
  it('a fought night\'s swing acts on the ships sunk a convoy attacked, and the month books them', () => {
    const g = open(true);
    g.flags.convoyBattle = 1;
    meet(g, 60, 30);
    const s = convoySideOf(g, 'UK', atlantic)!;
    expect(s.sunk * 1.2).toBeLessThan(44);
    convoyWar(g, 'UK', undefined, night(1.2));
    const rec = g.convoys.atlantic;
    expect(rec.swing).toBe(1.2);
    expect(rec.loss).toBeCloseTo(s.L * 1.2, 9);
    expect(rec.tons).toBe(Math.round((2 * atlantic.tons * s.L * 1.2) / 1000) * 1000);
  });

  it('never sinks more ships than a convoy holds: with no escort a swing above 1 books the whole convoy, no more', () => {
    const g = open(true);
    g.flags.convoyBattle = 1;
    meet(g, 60, 0);
    const s = convoySideOf(g, 'UK', atlantic)!, c = convoyCount(g, atlantic)!;
    expect(s.E).toBe(0);
    expect(s.sunk).toBeCloseTo(44, 9);
    convoyWar(g, 'UK', undefined, night(1.2));
    // Every convoy attacked is lost, and no more: the share is the share attacked.
    expect(g.convoys.atlantic.loss).toBeCloseTo(c.attacked / c.sailed, 9);
  });

  it('the month\'s record carries the ships booked, after the swing and within the convoy', () => {
    const g = open(true);
    g.flags.convoyBattle = 1;
    sea(g, 'Home Fleet', 'wapp', { dd: 2 }, 'escort', 'atlantic');
    sea(g, 'U-boat Fleet', 'natl', { ss: 60 }, 'raid');
    convoyField.fought = (x) => (x.route === 'atlantic' ? { swing: 1.2, ss: {}, dd: {} } : undefined);
    navalUpkeep(g, 'UK');
    const rec = g.convoys.atlantic;
    expect(rec.count!.sunk).toBeLessThanOrEqual(44);
    expect(rec.loss).toBeLessThanOrEqual(rec.count!.attacked / rec.count!.sailed + 0.01);
    // (The record's count is to two places, so the tons agree to a part in a hundred.)
    expect(rec.tons / (rec.count!.attacked * rec.count!.sunk! * 5200)).toBeCloseTo(1, 1);
  });

  it('the boats\' and a surface group\'s shares together stay within the lanes\' cap, swing or none', () => {
    const g = open(true);
    sea(g, 'Home Fleet', 'wapp', { dd: 1 }, 'escort', 'atlantic');
    sea(g, 'U-boat Fleet', 'natl', { ss: 400 }, 'raid');
    sea(g, 'Battle Fleet', 'natl', { bb: 4, ca: 8 }, 'raid');
    expect(convoyMeeting(g, 'UK', atlantic).L).toBeLessThanOrEqual(0.95);
    convoyWar(g, 'UK');
    expect(g.convoys.atlantic.loss).toBeLessThanOrEqual(0.95);
    expect(g.convoys.atlantic.share).toBeLessThanOrEqual(atlantic.share * 0.95 + 1e-12);
  });
});

describe('the other raiders, the lanes, and the game before it', () => {
  it('a surface raider keeps the lanes\' form beside the boats, its tons of the loaded sailings as before', () => {
    const g = open(true);
    sea(g, 'Home Fleet', 'wapp', { dd: 30 }, 'escort', 'atlantic');
    const raider = sea(g, 'U-boat Fleet', 'natl', { ca: 2 }, 'raid');
    const m = convoyMeeting(g, 'UK', atlantic);
    expect(convoySideOf(g, 'UK', atlantic)!.L).toBe(0);
    expect(m.L).toBeCloseTo(m.raid / (m.raid + m.G + NAVY.convoy.ocean), 12);
    expect(m.tons).toBe(Math.round((atlantic.tons * m.L) / 1000) * 1000);
    expect(m.tons).toBe(unread(() => convoyMeeting(g, 'UK', atlantic)).tons);
    raider.ships.ss = 40;
    const both = convoyMeeting(g, 'UK', atlantic), s = convoySideOf(g, 'UK', atlantic)!, c = convoyCount(g, atlantic)!;
    // (The boats' part of the lanes' raid is P / pack a boat on station, weighed by 3.5; the rest is the cruisers'.)
    const rest = both.raid - (s.P / c.pack) * c.boats * CAUSE.station.raid!;
    expect(rest).toBeCloseTo(m.raid, 9);
    const Lo = rest / (rest + both.G + NAVY.convoy.ocean);
    expect(both.L).toBeCloseTo(s.L + Lo, 12);
    expect(both.tons).toBe(Math.round((2 * atlantic.tons * s.L + atlantic.tons * Lo) / 1000) * 1000);
  });

  it('a fortress astride the Libya route alone sinks as it did', () => {
    const g = open(true);
    const ita = g.nations.ITA, uk = g.nations.UK;
    if (!ita.atWar.includes('UK')) ita.atWar.push('UK');
    if (!uk.atWar.includes('ITA')) uk.atWar.push('ITA');
    const m = convoyMeeting(g, 'ITA', libya);
    const before = unread(() => convoyMeeting(g, 'ITA', libya));
    // (Malta's raid alone: the count is laid on the route, but no boat is at sea.)
    expect(m.raid).toBeGreaterThan(0);
    expect(m.tons).toBeGreaterThan(0);
    expect(m.L).toBeCloseTo(before.L, 12);
    expect(m.tons).toBe(before.tons);
  });

  it('the oil and supply lanes keep their form', () => {
    const g = open(true);
    meet(g, 160, 60);
    const zones = atlantic.zones.map((k) => zoneOf(g, k));
    expect(laneLoss(g, 'UK', zones)).toEqual(unread(() => laneLoss(g, 'UK', zones)));
  });

  it('with the sinking figure unread, the route keeps the lanes\' form with a boat on station at 3.5: the game before it', () => {
    const g = open(true);
    meet(g, 60, 30);
    unread(() => {
      const m = convoyMeeting(g, 'UK', atlantic), c = convoyCount(g, atlantic)!;
      expect(convoySideOf(g, 'UK', atlantic)).toBeUndefined();
      expect(m.L).toBeCloseTo(m.raid / (m.raid + m.G + NAVY.convoy.ocean), 12);
      expect(m.tons).toBe(Math.round((atlantic.tons * m.L) / 1000) * 1000);
      expect(m.count!.sunk).toBeUndefined();
      // The lanes' raid counts a boat on station at 3.5: from the Bay's far side, 0.25 x 3.5 = 0.875 of a whole boat.
      const s = withRows(() => { CAUSE.count.sink = 0.137; }, () => { CAUSE.count.sink = null; }, () => convoySideOf(g, 'UK', atlantic)!);
      expect(m.raid).toBeCloseTo((s.P / c.pack) * c.boats * 3.5, 9);
    });
  });

  it('off, nothing is counted', () => {
    const g = open(false);
    meet(g, 60, 30);
    expect(convoySideOf(g, 'UK', atlantic)).toBeUndefined();
  });

  it('the card and a save carry the ships a convoy attacked would lose', () => {
    const g = open(true);
    meet(g, 60, 30);
    const m = convoyMeeting(g, 'UK', atlantic);
    expect(m.count!.sunk).toBeCloseTo(convoySideOf(g, 'UK', atlantic)!.sunk, 2);
    expect(convoyTicket(g, m).place.card.count).toEqual(m.count);
    navalUpkeep(g, 'UK');
    const rec = g.convoys.atlantic;
    expect(rec.count!.sunk).toBeGreaterThan(0);
    expect(loadGame(saveGame(g))!.convoys.atlantic.count).toEqual(rec.count);
  });
});
