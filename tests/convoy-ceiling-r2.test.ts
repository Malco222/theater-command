// The boat ceiling (release 2, Units decide; the design director's rulings of 24 November 2026 14:46Z, corrected 14:49Z,
// on naval-battles/convoy-r2/tons-reading-1124.md Finding 3 (a), and 16:27Z and 16:31Z on ceiling-reading-1124.md). One
// boat that fires sinks at most its usable torpedoes x the share that hit x the ships sunk a hit in one convoy battle on an
// average night: 12 inside the hull, 2 more outside it as far as the sea is calm (a clear sky with weather; with none, the
// weather table's chance of one that month), 0.45 a torpedo fired for every boat, 0.57 ships a hit
// (historian/naval-reads-3-1124.md sections 2 and 5). A convoy attacked loses at most the pack x that; the month's swing
// and a fought night act around it, mean 1. Nothing in it reads the flag or the date.
import { afterAll, afterEach, beforeAll, describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { CAUSE, CONVOY_BATTLE, NAVY, boatCeiling, convoyCount, convoyField, convoyMeeting, convoySideOf, convoyWar, laneLoss, navalUpkeep, zoneOf } from '../src/campaign/navy';
import type { ConvoyNights } from '../src/campaign/navy';
import { convoyTicket } from '../src/battle/convoy/ticket';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { grantTech } from '../src/campaign/tech';
import { oddsOf, skyAtSea, zoneOfSea } from '../src/campaign/weather';
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
/** A pack and an escort on the Atlantic route. */
function meet(g: Game, ss: number, dd: number): Fleet {
  sea(g, 'Home Fleet', 'wapp', { dd }, 'escort', 'atlantic');
  return sea(g, 'U-boat Fleet', 'natl', { ss }, 'raid');
}
/** Runs with the ceiling unread: the tons ruling as it was. */
function unread<T>(f: () => T): T {
  const keep = CAUSE.ceiling.inside;
  CAUSE.ceiling.inside = null;
  try { return f(); } finally { CAUSE.ceiling.inside = keep; }
}
/** A night the player fought on the Atlantic route, at this swing, booking no fleet's losses. */
const night = (swing: number): ConvoyNights => new Map([['atlantic', { fought: { swing, ss: {}, dd: {} }, ss: new Set<number>(), dd: new Set<number>(), clamped: false }]]);
/** The ceiling at a calm share, from the ruled figures. */
const top = (calm: number): number => (12 + 2 * calm) * 0.45 * 0.57;
/** The weather table's chance of a clear sky over a sea this month, as the ceiling reads it with no weather. */
const clearOdds = (g: Game, z: number): number => { const o = oddsOf(zoneOfSea(g, z), g.turn); return o[0] / (o[0] + o[1] + o[2]); };
/** A boat's ceiling in the North Atlantic, with no weather: the start's Atlantic route. */
const NATL = (g: Game): number => top(clearOdds(g, zoneOf(g, 'natl')));

afterEach(() => { convoyField.fought = undefined; });
// (The ceiling on the pack as the count counts it: the escort's radar before a boat fires is unread here, and tested
// with the ceiling in convoy-firing-r2; the whole load into one convoy, as first built: the load form is unread here,
// and tested in convoy-load-r2; the contest's E and k as the tons ruling set them: the escorts' losses form is unread
// here, and tested in convoy-escorts-r2.)
const firingKept = CAUSE.firing.t2, eyeKept = CAUSE.firing.eye!.effective41, reloadKept = CAUSE.load!.reload, escortsKept = CAUSE.escorts;
beforeAll(() => { CAUSE.firing.t2 = null; CAUSE.firing.eye!.effective41 = null; CAUSE.load!.reload = null; CAUSE.escorts = null; });
afterAll(() => { CAUSE.firing.t2 = firingKept; CAUSE.firing.eye!.effective41 = eyeKept; CAUSE.load!.reload = reloadKept; CAUSE.escorts = escortsKept; });

describe('the figures', () => {
  it('lays 12 torpedoes inside the hull, 2 outside, 0.45 a torpedo fired, 0.57 a hit: about 3.1 ships a boat that fires a battle, 3.6 in a calm sea', () => {
    expect(CAUSE.ceiling).toEqual({ inside: 12, outside: 2, hit: 0.45, sinkHit: 0.57, tubes: 5 });
    const g = open(true);
    expect(boatCeiling(g, zoneOf(g, 'natl'))).toBeCloseTo(NATL(g), 12);
    // Inside the design director's 2-4 a boat a battle, rough or calm ...
    for (const c of [0, 1]) { expect(top(c)).toBeGreaterThan(2); expect(top(c)).toBeLessThan(4); }
    // ... and the tail: the most a boat books in a month, at the swing's top, under the best single boat's 7.
    expect(top(1) * CONVOY_BATTLE.swing.high).toBeLessThan(7);
  });

  it('with no weather, counts the torpedoes outside the hull at the weather table\'s chance of a clear sky over the sea that month; one of two where the table has none', () => {
    const g = open(true);
    const natl = zoneOf(g, 'natl'), central = zoneOf(g, 'central'), moz = zoneOf(g, 'mozambique');
    const seen = new Set<number>();
    for (let t = 1; t <= 12; t++) {
      g.turn = t;
      const o = oddsOf(zoneOfSea(g, natl), t);
      expect(boatCeiling(g, natl)).toBeCloseTo((12 + (2 * o[0]) / (o[0] + o[1] + o[2])) * 0.45 * 0.57, 12);
      seen.add(boatCeiling(g, natl)!);
    }
    // (The sky the table expects moves with the season, and differs by sea: never all calm, never all rough.)
    expect(seen.size).toBeGreaterThan(1);
    g.turn = 1;
    expect(boatCeiling(g, central)).not.toBeCloseTo(boatCeiling(g, natl)!, 6);
    for (const x of seen) { expect(x).toBeGreaterThan(top(0) - 1e-12); expect(x).toBeLessThan(top(1) + 1e-12); }
    expect(zoneOfSea(g, moz)).toBeLessThan(0);
    expect(boatCeiling(g, moz)).toBeCloseTo(top(0.5), 12);
  });

  it('one hit share for every boat that fires: neither side\'s radar moves it', () => {
    const g = open(true);
    meet(g, 60, 30);
    const per = (): number => convoySideOf(g, 'UK', atlantic)!.torps! / convoyCount(g, atlantic)!.pack;
    const was = per();
    expect(was).toBeCloseTo(NATL(g), 12);
    grantTech(g, 'GER', 'asvradar');
    grantTech(g, 'UK', 'asvradar');
    expect(per()).toBeCloseTo(was, 12);
  });

  it('only a calm sea lets the torpedoes outside the hull in: with weather, all under a clear sky, none under cloud or a closed sky', () => {

    const g = open(true);
    g.flags.weather = 1;
    const z = zoneOf(g, 'natl');
    const seen = new Map<string, number>();
    for (let t = 1; t <= 120 && seen.size < 3; t++) { g.turn = t; const sky = skyAtSea(g, z); if (!seen.has(sky)) seen.set(sky, boatCeiling(g, z)!); }
    expect(seen.get('clear')).toBeCloseTo(top(1), 12);
    expect(seen.get('cloud')).toBeCloseTo(top(0), 12);
    expect(seen.get('closed')).toBeCloseTo(top(0), 12);
  });

  it('a route\'s ceiling is the mean of its boats\' own, each in its sea under its sky, weighted as the count weighs them', () => {
    const g = open(true);
    g.flags.weather = 1;
    const ger = g.fleets.filter((f) => f.nat === 'GER' && f.name !== 'U-boat Fleet');
    expect(ger.length).toBeGreaterThan(0);
    const natl = zoneOf(g, 'natl'), irish = zoneOf(g, 'irish');
    const clear = (z: number): boolean => skyAtSea(g, z) === 'clear';
    for (let t = 1; t <= 240; t++) { g.turn = t; if (clear(natl) !== clear(irish)) break; }
    expect(clear(natl)).not.toBe(clear(irish));
    sea(g, 'Home Fleet', 'wapp', { dd: 30 }, 'escort', 'atlantic');
    const a = sea(g, 'U-boat Fleet', 'natl', { ss: 20 }, 'raid');
    const b = sea(g, ger[0].name, 'irish', { ss: 10 }, 'patrol');
    const boatsOf = (f: Fleet): number => {
      const keep = f === a ? b : a, ss = keep.ships.ss;
      keep.ships.ss = 0;
      try { return convoyCount(g, atlantic)!.boats; } finally { keep.ships.ss = ss; }
    };
    const wa = boatsOf(a), wb = boatsOf(b);
    expect(wa).toBeGreaterThan(0);
    expect(wb).toBeGreaterThan(0);
    const want = (wa * boatCeiling(g, natl)! + wb * boatCeiling(g, irish)!) / (wa + wb);
    expect(boatCeiling(g, natl)).not.toBeCloseTo(boatCeiling(g, irish)!, 6);
    const c = convoyCount(g, atlantic)!, s = convoySideOf(g, 'UK', atlantic)!;
    expect(s.torps! / c.pack).toBeCloseTo(want, 12);
  });

  it('one figure for every navy and every date: British boats on the Libya route are held as German boats on the Atlantic', () => {
    const g = open(true);
    meet(g, 60, 30);
    const a = convoySideOf(g, 'UK', atlantic)!, ca = convoyCount(g, atlantic)!;
    expect(a.torps! / ca.pack).toBeCloseTo(NATL(g), 12);
    const later = loadGame(saveGame(g))!;
    later.turn += 60;
    expect(convoySideOf(later, 'UK', atlantic)).toEqual(a);
    const ita = g.nations.ITA, uk = g.nations.UK;
    if (!ita.atWar.includes('UK')) ita.atWar.push('UK');
    if (!uk.atWar.includes('ITA')) uk.atWar.push('ITA');
    sea(g, '1st Fleet', 'central', { dd: 4 }, 'escort', 'libya');
    sea(g, 'Mediterranean Fleet', 'central', { ss: 12 }, 'raid');
    const l = convoySideOf(g, 'ITA', libya)!, cl = convoyCount(g, libya)!;
    expect(cl.pack).toBeGreaterThan(0);
    // (The same rule in another sea, under its own sky.)
    expect(l.torps! / Math.max(1, cl.pack)).toBeCloseTo(boatCeiling(g, zoneOf(g, 'central'))!, 12);
  });

  it('a convoy attacked has at least one boat on it: where the count\'s pack is under one, the ceiling is one boat\'s', () => {
    const g = open(true);
    const ita = g.nations.ITA, uk = g.nations.UK;
    if (!ita.atWar.includes('UK')) ita.atWar.push('UK');
    if (!uk.atWar.includes('ITA')) uk.atWar.push('ITA');
    sea(g, '1st Fleet', 'central', { dd: 0 }, 'escort', 'libya');
    sea(g, 'Mediterranean Fleet', 'central', { ss: 4 }, 'raid');
    const l = convoySideOf(g, 'ITA', libya)!, cl = convoyCount(g, libya)!;
    expect(cl.pack).toBeGreaterThan(0);
    expect(cl.pack).toBeLessThan(1);
    const one = boatCeiling(g, zoneOf(g, 'central'))!;
    expect(l.torps).toBeCloseTo(one, 12);
    expect(l.sunk).toBeGreaterThan(cl.pack * one);
  });
});

describe('a convoy attacked', () => {
  it('with a weak escort loses no more than the pack\'s torpedoes can sink, not the contest\'s 30', () => {
    const g = open(true);
    meet(g, 60, 30);
    const s = convoySideOf(g, 'UK', atlantic)!, c = convoyCount(g, atlantic)!;
    const contest = unread(() => convoySideOf(g, 'UK', atlantic)!);
    expect(contest.torps).toBeUndefined();
    expect(contest.sunk).toBeGreaterThan(20);
    expect(s.torps).toBeCloseTo(c.pack * NATL(g), 12);
    expect(s.sunk).toBe(s.torps);
    expect(s.P).toBe(contest.P);
    expect(s.E).toBe(contest.E);
    // The share lost is the share of the ships sunk, as before.
    expect(s.L).toBeCloseTo((c.attacked * s.sunk) / (c.sailed * 44), 12);
    expect(convoyMeeting(g, 'UK', atlantic).tons).toBe(Math.round((c.attacked * s.sunk * 5200) / 1000) * 1000);
  });

  it('with a strong escort and aircraft with kit the contest sinks fewer than the torpedoes could, and is left as it is', () => {
    const g = open(true);
    for (const id of ['sonar', 'asvradar', 'hedgehog', 'escortgroups', 'patrolaircraft']) grantTech(g, 'UK', id);
    meet(g, 40, 600);
    const s = convoySideOf(g, 'UK', atlantic)!, contest = unread(() => convoySideOf(g, 'UK', atlantic)!);
    expect(contest.sunk).toBeLessThan(s.torps!);
    expect(s.sunk).toBe(contest.sunk);
    expect(s.L).toBe(contest.L);
  });

  it('bigger packs carry more torpedoes: the most a convoy can lose rises with the pack, each boat\'s the same', () => {
    const g = open(true);
    let top = 0;
    for (const n of [20, 40, 80, 160, 320]) {
      meet(g, n, 30);
      const s = convoySideOf(g, 'UK', atlantic)!, c = convoyCount(g, atlantic)!;
      expect(s.torps!).toBeGreaterThan(top);
      expect(s.torps! / c.pack).toBeCloseTo(NATL(g), 12);
      expect(s.sunk).toBeLessThanOrEqual(s.torps! + 1e-12);
      top = s.torps!;
    }
  });

  it('with the ceiling unread, the tons ruling is as it was', () => {
    const g = open(true);
    meet(g, 60, 30);
    unread(() => {
      const s = convoySideOf(g, 'UK', atlantic)!;
      expect(s.torps).toBeUndefined();
      expect(s.sunk).toBeCloseTo((44 * s.P) / (s.P + CAUSE.count.sink! * s.E), 12);
      expect(convoyMeeting(g, 'UK', atlantic).count!.torps).toBeUndefined();
    });
  });
});

describe('the swing', () => {
  /** The month booked at this swing, from a fresh game each time. */
  function month(w: number): { L: number; tons: number; s: ReturnType<typeof convoySideOf>; c: ReturnType<typeof convoyCount> } {
    const g = open(true);
    g.flags.convoyBattle = 1;
    meet(g, 60, 30);
    const s = convoySideOf(g, 'UK', atlantic), c = convoyCount(g, atlantic);
    convoyWar(g, 'UK', undefined, night(w));
    return { L: g.convoys.atlantic.loss ?? 0, tons: g.convoys.atlantic.tons ?? 0, s, c };
  }

  it('acts around the ceiling: a good night\'s torpedoes hit more often than the average\'s, a bad night\'s less', () => {
    const hi = month(1.2), lo = month(0.8), mid = month(1);
    expect(mid.s!.sunk).toBe(mid.s!.torps);
    expect(mid.L).toBeCloseTo(mid.s!.L, 12);
    expect(hi.L).toBeCloseTo(mid.s!.L * 1.2, 12);
    expect(lo.L).toBeCloseTo(mid.s!.L * 0.8, 12);
    expect(hi.tons).toBe(Math.round((mid.c!.attacked * mid.s!.torps! * 1.2 * 5200) / 1000) * 1000);
  });

  it('keeps ruling 241\'s mean of 1 where the ceiling holds: the swing\'s ends average to the card', () => {
    const { low, high } = CONVOY_BATTLE.swing;
    expect((low + high) / 2).toBeCloseTo(1, 12);
    const a = month(low), b = month(high), mid = month(1);
    expect((a.L + b.L) / 2).toBeCloseTo(mid.L, 12);
  });
});

describe('the escort where the ceiling holds', () => {
  it('a bigger escort does not cut the ships sunk while the contest stays above the pack\'s torpedoes; with aircraft and kit it gets under', () => {
    const g = open(true);
    const at = (dd: number): { s: NonNullable<ReturnType<typeof convoySideOf>>; contest: number } => {
      meet(g, 60, dd);
      return { s: convoySideOf(g, 'UK', atlantic)!, contest: unread(() => convoySideOf(g, 'UK', atlantic)!).sunk };
    };
    const few = at(24), more = at(60);
    expect(few.contest).toBeGreaterThan(few.s.torps!);
    expect(more.contest).toBeGreaterThan(more.s.torps!);
    expect(more.contest).toBeLessThan(few.contest);
    expect(more.s.sunk).toBe(few.s.sunk);
    for (const id of ['sonar', 'asvradar', 'hedgehog', 'escortgroups', 'patrolaircraft']) grantTech(g, 'UK', id);
    const strong = at(600);
    expect(strong.contest).toBeLessThan(strong.s.torps!);
    expect(strong.s.sunk).toBe(strong.contest);
  });
});

describe('the card, a save, the lanes, and off', () => {
  it('the card and a save carry the pack\'s torpedoes beside the ships sunk', () => {
    const g = open(true);
    g.flags.convoyBattle = 1;
    meet(g, 60, 30);
    convoyField.fought = (x) => (x.route === 'atlantic' ? { swing: 1.2, ss: {}, dd: {} } : undefined);
    const m = convoyMeeting(g, 'UK', atlantic);
    expect(m.count!.torps).toBeCloseTo(convoySideOf(g, 'UK', atlantic)!.torps!, 2);
    expect(m.count!.sunk).toBe(m.count!.torps);
    expect(convoyTicket(g, m).place.card.count).toEqual(m.count);
    navalUpkeep(g, 'UK');
    const rec = g.convoys.atlantic;
    expect(rec.count!.torps).toBeGreaterThan(0);
    expect(rec.swing).toBe(1.2);
    expect(rec.count!.sunk).toBeGreaterThan(rec.count!.torps!);
    expect(rec.count!.sunk).toBeLessThanOrEqual(rec.count!.torps! * 1.2 + 0.01);
    expect(loadGame(saveGame(g))!.convoys.atlantic.count).toEqual(rec.count);
  });

  it('the oil and supply lanes keep their form', () => {
    const g = open(true);
    meet(g, 160, 60);
    const zones = atlantic.zones.map((k) => zoneOf(g, k));
    expect(laneLoss(g, 'UK', zones)).toEqual(unread(() => laneLoss(g, 'UK', zones)));
  });

  it('off, nothing is read', () => {
    const g = open(false);
    meet(g, 60, 30);
    expect(convoySideOf(g, 'UK', atlantic)).toBeUndefined();
    expect(convoyMeeting(g, 'UK', atlantic)).toEqual(unread(() => convoyMeeting(g, 'UK', atlantic)));
  });
});
