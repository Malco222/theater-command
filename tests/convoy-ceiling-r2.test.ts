// The boat ceiling (release 2, Units decide; the design director's ruling of 24 November 2026 14:46Z, corrected 14:49Z,
// on naval-battles/convoy-r2/tons-reading-1124.md Finding 3 (a)). One boat sinks at most its usable torpedoes x the share
// that hit x the ships a hit sinks in one convoy battle: 12 inside the hull, 2 more outside it in a calm sea, 0.24 a
// torpedo for every navy, 0.7 a hit (historian/naval-reads-3-1124.md sections 2 and 5). A convoy attacked loses at most
// the pack x that, and the month's swing and a fought night never carry it past it. Nothing in it reads the flag or the date.
import { afterEach, describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { CAUSE, NAVY, boatCeiling, convoyCount, convoyField, convoyMeeting, convoySideOf, convoyWar, laneLoss, navalUpkeep, zoneOf } from '../src/campaign/navy';
import type { ConvoyNights } from '../src/campaign/navy';
import { convoyTicket } from '../src/battle/convoy/ticket';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { grantTech } from '../src/campaign/tech';
import { skyAtSea } from '../src/campaign/weather';
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
const CALM = 14 * 0.24 * 0.7, ROUGH = 12 * 0.24 * 0.7;

afterEach(() => { convoyField.fought = undefined; });

describe('the figures', () => {
  it('lays 12 torpedoes inside the hull, 2 outside, 0.24 a torpedo, 0.7 a hit: about 2.35 ships a boat a battle in a calm sea', () => {
    expect(CAUSE.ceiling).toEqual({ inside: 12, outside: 2, hit: 0.24, sinkHit: 0.7 });
    const g = open(true);
    const top = boatCeiling(g, zoneOf(g, 'natl'))!;
    expect(top).toBeCloseTo(CALM, 12);
    // The record's checks, never fitted: about 1-3 ships a boat a battle on average; the best single boat's 7 the tail.
    expect(top).toBeGreaterThan(1);
    expect(top).toBeLessThan(3);
    expect(top).toBeLessThan(7);
  });

  it('a rough sea keeps the torpedoes outside the hull out: about 2.0 under a closed sky, 2.35 under any other', () => {
    const g = open(true);
    g.flags.weather = 1;
    const z = zoneOf(g, 'natl');
    const seen = new Map<string, number>();
    for (let t = 1; t <= 120 && seen.size < 3; t++) { g.turn = t; const sky = skyAtSea(g, z); if (!seen.has(sky)) seen.set(sky, boatCeiling(g, z)!); }
    expect(seen.get('closed')).toBeCloseTo(ROUGH, 12);
    expect(seen.get('cloud')).toBeCloseTo(CALM, 12);
    expect(seen.get('clear')).toBeCloseTo(CALM, 12);
  });

  it('one figure for every navy and every date: British boats on the Libya route are held as German boats on the Atlantic', () => {
    const g = open(true);
    meet(g, 60, 30);
    const a = convoySideOf(g, 'UK', atlantic)!, ca = convoyCount(g, atlantic)!;
    expect(a.torps! / ca.pack).toBeCloseTo(CALM, 12);
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
    expect(l.torps! / cl.pack).toBeCloseTo(CALM, 12);
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
    expect(s.torps).toBeCloseTo(c.pack * CALM, 12);
    expect(s.sunk).toBe(s.torps);
    expect(s.P).toBe(contest.P);
    expect(s.E).toBe(contest.E);
    // The share lost is the share of the ships sunk, as before.
    expect(s.L).toBeCloseTo((c.attacked * s.sunk) / (c.sailed * 44), 12);
    expect(convoyMeeting(g, 'UK', atlantic).tons).toBe(Math.round((c.attacked * s.sunk * 5200) / 1000) * 1000);
  });

  it('with a strong escort with kit the contest sinks fewer than the torpedoes could, and is left as it is', () => {
    const g = open(true);
    for (const id of ['sonar', 'asvradar', 'hedgehog', 'escortgroups']) grantTech(g, 'UK', id);
    meet(g, 40, 800);
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
      expect(s.torps! / c.pack).toBeCloseTo(CALM, 12);
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
  it('a fought night\'s swing above 1 never carries a convoy past the pack\'s torpedoes', () => {
    const g = open(true);
    g.flags.convoyBattle = 1;
    meet(g, 60, 30);
    const s = convoySideOf(g, 'UK', atlantic)!, c = convoyCount(g, atlantic)!;
    expect(s.sunk).toBe(s.torps);
    convoyWar(g, 'UK', undefined, night(1.2));
    const rec = g.convoys.atlantic;
    expect(rec.swing).toBe(1.2);
    expect(rec.loss).toBeCloseTo(s.L, 12);
    expect(rec.tons).toBe(Math.round((c.attacked * s.torps! * 5200) / 1000) * 1000);
  });

  it('a swing below 1 still cuts the ships sunk', () => {
    const g = open(true);
    g.flags.convoyBattle = 1;
    meet(g, 60, 30);
    const s = convoySideOf(g, 'UK', atlantic)!;
    convoyWar(g, 'UK', undefined, night(0.8));
    expect(g.convoys.atlantic.loss).toBeCloseTo(s.L * 0.8, 12);
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
    expect(rec.count!.sunk).toBeLessThanOrEqual(rec.count!.torps! + 0.01);
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
