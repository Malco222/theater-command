// Time on station and the convoy-side count (release 2, Units decide; the design director's one-pass ruling of 24
// November 2026 13:23Z on naval-battles/convoy-r2/one-pass-form.md, and the days of 13:31Z from the historian's reads,
// historian/convoy-build-reads-1124.md). A boat is on station only part of its cycle: the passage from its port and
// back by the sea graph and the refit in port take the rest, so a navy that bases nearer keeps more boats there, and
// its staff does. The convoys a route sails (both ways) and those found are counted from the convoys' side; the boats
// that meet their escort from the boats' side, at OEG T2's own rate (the reading ruling of 13:50Z). Off, nothing of it is read.
import { afterEach, describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { CAUSE, NAVY, convoyCount, convoyField, convoyMeeting, fit, huntExpected, navalAI, navalUpkeep, nearerPort, onStation, passageSeas, zoneOf } from '../src/campaign/navy';
import { convoyTicket } from '../src/battle/convoy/ticket';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { grantTech } from '../src/campaign/tech';
import type { Fleet, Game, Ships } from '../src/campaign/types';

const atlantic = NAVY.convoys.find((r) => r.id === 'atlantic')!;
const libya = NAVY.convoys.find((r) => r.id === 'libya')!;

/** A grand campaign at its start, with Units decide on or off, and every fleet in harbour (a test puts to sea what it needs). */
function open(on: boolean, seed = 1, human: string | null = null): Game {
  const g = startOpen(createGame(seed, human));
  if (on) g.flags.unitsDecide = 1; else delete g.flags.unitsDecide;
  delete g.flags.weather;
  for (const f of g.fleets) f.stance = 'port';
  return g;
}
const none = (): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 });
/** A fleet of this name put to sea in a sea with only these ships, fully ready. */
function sea(g: Game, name: string, zone: string, ships: Partial<Ships>, stance: Fleet['stance'], lane?: string): Fleet {
  const f = g.fleets.find((x) => x.name === name)!;
  f.ships = { ...none(), ...ships };
  f.zone = zoneOf(g, zone);
  f.stance = stance;
  if (lane) f.lane = lane; else delete f.lane;
  f.ready = 1;
  return f;
}
/** A region, handed to a nation since before this month. */
function hold(g: Game, name: string, nat: string): number {
  const r = g.byName[name];
  r.ctrl = nat;
  r.since = 0;
  return r.id;
}
/** Runs a test with figures changed, putting them back after. */
function withRows<T>(set: () => void, put: () => void, f: () => T): T { set(); try { return f(); } finally { put(); } }
const unread = <T>(f: () => T): T => {
  const keep = { ...CAUSE.station.ss! };
  return withRows(() => { CAUSE.station.ss!.patrol = null; }, () => { CAUSE.station.ss = keep; }, f);
};
const twin = (g: Game): Game => loadGame(saveGame(g))!;

afterEach(() => { convoyField.fought = undefined; });

describe('time on station: the days, not the shares', () => {
  it('lays the historian\'s days: 20 on station, 5 a sea each way, 40 in port', () => {
    expect(CAUSE.station.ss).toEqual({ patrol: 20, passage: 5, refit: 40 });
    // A boat's raid weight on station in the lanes' form, refit once at T2's window so it keeps its tons: the window's
    // own share undone. (The convoys' side counts a boat on station at 1: convoy-tons-r2.)
    expect(CAUSE.station.raid).toBe(3.5);
  });

  it('a boat from the Bay of Biscay raiding the North Atlantic is on station a quarter of the time (the record\'s check)', () => {
    const g = open(true);
    const u = g.fleets.find((f) => f.name === 'U-boat Fleet')!;
    const brittany = { nat: 'GER', base: hold(g, 'Bordeaux', 'GER'), home: zoneOf(g, 'biscay') };
    expect(passageSeas(g, brittany, zoneOf(g, 'natl'))).toBe(2);
    // McCue's 10 days each way from Biscay to the North Atlantic, over the chart's two seas.
    expect(2 * CAUSE.station.ss!.passage! * passageSeas(g, brittany, zoneOf(g, 'natl'))).toBe(20);
    expect(onStation(g, brittany, zoneOf(g, 'natl'))).toBeCloseTo(0.25, 12);
    expect(onStation(g, brittany, zoneOf(g, 'wapp'))).toBeCloseTo(20 / 70, 12);
    // German ports by the same days a sea, with no figure of their own.
    expect(u.port).toBe('Wilhelmshaven');
    expect(passageSeas(g, u, zoneOf(g, 'wapp'))).toBe(2);
    expect(onStation(g, u, zoneOf(g, 'wapp'))).toBeCloseTo(0.25, 12);
    expect(onStation(g, u, zoneOf(g, 'natl'))).toBeCloseTo(20 / 70, 12);
    // The sea its own port opens on: no passage, the refit still.
    expect(onStation(g, u, zoneOf(g, 'nsea'))).toBeCloseTo(20 / 60, 12);
  });

  it('is 1 off, while a day is unread, and for a class without days', () => {
    const off = open(false);
    const u = off.fleets.find((f) => f.name === 'U-boat Fleet')!;
    expect(onStation(off, u, zoneOf(off, 'wapp'))).toBe(1);
    const g = open(true);
    unread(() => expect(onStation(g, g.fleets.find((f) => f.name === 'U-boat Fleet')!, zoneOf(g, 'wapp'))).toBe(1));
    expect(onStation(g, g.fleets.find((f) => f.name === 'U-boat Fleet')!, zoneOf(g, 'wapp'), 'ca')).toBe(1);
  });

  it('cuts the boats\' raid strength that sinks tons, never a surface raider\'s', () => {
    const g = open(true);
    sea(g, 'U-boat Fleet', 'wapp', { ss: 40, ca: 2 }, 'raid');
    const raid = convoyMeeting(g, 'UK', atlantic).raid;
    const all = unread(() => convoyMeeting(g, 'UK', atlantic).raid);
    const q = all / (40 * NAVY.classes.ss.raid + 2 * NAVY.classes.ca.raid);
    expect(raid).toBeCloseTo(q * (40 * NAVY.classes.ss.raid * 0.25 * CAUSE.station.raid! + 2 * NAVY.classes.ca.raid), 9);
    // So in the lanes' form a boat raiding from a port two seas off sinks 0.875 of what it would from one sea off, as at T2.
    expect(0.25 * CAUSE.station.raid!).toBeCloseTo(0.875, 12);
  });
});

describe('the convoy-side count', () => {
  it('sails the route\'s tons both ways in convoys of 44 ships at 5,200 GRT, Libya\'s in convoys of 2 at 3,800', () => {
    const g = open(true);
    expect(CAUSE.count.ways).toBe(2);
    expect(convoyCount(g, atlantic)!.sailed).toBeCloseTo((2 * 1_200_000) / (44 * 5200), 12);
    expect(convoyCount(g, libya)!.sailed).toBeCloseTo((2 * 120_000) / (2 * 3800), 12);
    // About 10.5 a month on the Atlantic route, as T2's own 80 convoys over 8 months are 10.
    expect(convoyCount(g, atlantic)!.sailed).toBeCloseTo(10.49, 2);
  });

  it('one phi for every route: 0.42 of convoys attacked with 40 boats on station (OEG T2)', () => {
    const g = open(true);
    // (Unread days put every raiding boat on station, so 40 boats are 40 on station.)
    unread(() => {
      sea(g, 'U-boat Fleet', 'natl', { ss: 40 }, 'raid');
      const c = convoyCount(g, atlantic)!;
      expect(c.boats).toBe(40);
      expect(c.attacked / c.sailed).toBeCloseTo(0.42, 12);
      // A contact is a convoy sighted: 0.68 sighted where 0.42 are attacked.
      expect(c.sighted / c.sailed).toBeCloseTo(0.68, 12);
    });
  });

  it('the boats that meet the escort are boat-side, at T2\'s own rate: 0.21 a boat on station a month', () => {
    const g = open(true);
    expect(((CAUSE.count.t2.convoys / CAUSE.count.t2.months) * CAUSE.count.attacked * CAUSE.count.perAttack) / CAUSE.count.boatsT2).toBeCloseTo(0.21, 12);
    unread(() => {
      sea(g, 'U-boat Fleet', 'natl', { ss: 40 }, 'raid');
      expect(convoyCount(g, atlantic)!.incidents).toBeCloseTo(40 * 0.21, 12);
      // T2's window itself, 10 convoys a month with 40 boats on station: 2 boats a convoy attacked, OEG's own estimate.
      const t2 = { ...atlantic, tons: (10 * 44 * 5200) / 2 };
      const c = convoyCount(g, t2)!;
      expect(c.sailed).toBeCloseTo(10, 12);
      expect(c.pack).toBeCloseTo(2, 12);
    });
  });

  it('more boats find more convoys, never more than sail, and meet them in bigger packs', () => {
    const g = open(true);
    let last = -1, pack = 0;
    for (const n of [10, 40, 160, 640, 2560]) {
      sea(g, 'U-boat Fleet', 'natl', { ss: n }, 'raid');
      const c = convoyCount(g, atlantic)!;
      expect(c.incidents).toBeGreaterThan(last);
      expect(c.attacked).toBeLessThanOrEqual(c.sailed);
      expect(c.sighted).toBeLessThanOrEqual(c.sailed);
      expect(c.sighted).toBeGreaterThanOrEqual(c.attacked);
      expect(c.pack).toBeGreaterThan(pack);
      expect(c.pack).toBeCloseTo(c.incidents / c.attacked, 12);
      last = c.incidents; pack = c.pack;
    }
    sea(g, 'U-boat Fleet', 'natl', { ss: 0 }, 'raid');
    expect(convoyCount(g, atlantic)).toMatchObject({ boats: 0, attacked: 0, incidents: 0, pack: 0 });
  });

  it('shares the incidents among the raiders by their boats on station', () => {
    const g = open(true);
    const brittany = hold(g, 'Bordeaux', 'GER');
    const far = sea(g, 'U-boat Fleet', 'wapp', { ss: 20 }, 'raid');
    const near = sea(g, 'Battle Fleet', 'wapp', { ss: 20 }, 'raid');
    near.base = brittany; near.home = zoneOf(g, 'biscay');
    sea(g, 'Home Fleet', 'wapp', { dd: 20 }, 'escort', 'atlantic');
    const m = convoyMeeting(g, 'UK', atlantic);
    const by = (f: Fleet): number => m.raiders.find((r) => r.id === f.id)!.expect;
    // From Biscay 20/70 on station, from Wilhelmshaven 20/80.
    expect(by(near) / by(far)).toBeCloseTo((20 / 70) / (20 / 80), 9);
  });

  it('a route with a figure unread keeps the per-boat share of OEG T2', () => {
    const g = open(true);
    const ita = g.fleets.find((f) => f.nat === 'UK' && f.ships.ss > 0)!;
    sea(g, ita.name, 'central', { ss: 10 }, 'raid');
    g.nations.UK.atWar.includes('ITA') || g.nations.UK.atWar.push('ITA');
    g.nations.ITA.atWar.includes('UK') || g.nations.ITA.atWar.push('UK');
    const keep = CAUSE.count.ships.libya;
    withRows(() => { CAUSE.count.ships.libya = null; }, () => { CAUSE.count.ships.libya = keep; }, () => {
      expect(convoyCount(g, libya)).toBeNull();
      expect(convoyMeeting(g, 'ITA', libya).count).toBeUndefined();
    });
    expect(convoyCount(g, libya)).not.toBeNull();
  });

  it('off, nothing is counted and the card carries none', () => {
    const g = open(false);
    sea(g, 'U-boat Fleet', 'natl', { ss: 40 }, 'raid');
    expect(convoyCount(g, atlantic)).toBeNull();
    expect(convoyMeeting(g, 'UK', atlantic).count).toBeUndefined();
  });

  it('the month\'s record and the field\'s card carry it; a save keeps it', () => {
    const g = open(true);
    grantTech(g, 'UK', 'sonar');
    sea(g, 'U-boat Fleet', 'natl', { ss: 60 }, 'raid');
    sea(g, 'Home Fleet', 'wapp', { dd: 30 }, 'escort', 'atlantic');
    const m = convoyMeeting(g, 'UK', atlantic);
    expect(m.count).toBeDefined();
    expect(convoyTicket(g, m).place.card.count).toEqual(m.count);
    navalUpkeep(g, 'UK');
    const rec = g.convoys.atlantic.count!;
    expect(rec.sailed).toBeCloseTo(convoyCount(g, atlantic)!.sailed, 2);
    expect(rec.incidents).toBeGreaterThan(0);
    expect(twin(g).convoys.atlantic.count).toEqual(rec);
  });

  it('the fleet orders\' look-ahead reads the same count as the card', () => {
    const g = open(true);
    const u = sea(g, 'U-boat Fleet', 'natl', { ss: 30 }, 'raid');
    sea(g, 'Home Fleet', 'wapp', { dd: 30 }, 'escort', 'atlantic');
    // (Less what the coasts' aircraft would sink with no convoy found: the rest is the convoy's escort's.)
    const ahead = huntExpected(g, 'UK', u.zone, [u]);
    const keep = CAUSE.count.attacked;
    const air = withRows(() => { CAUSE.count.attacked = 0; }, () => { CAUSE.count.attacked = keep; }, () => huntExpected(g, 'UK', u.zone, [u]));
    expect(ahead - air).toBeGreaterThan(0);
    expect(ahead - air).toBeCloseTo(convoyMeeting(g, 'UK', atlantic).raiders[0].expect, 9);
  });

  it('nothing in it reads the date or the flag: the same table in 1939 and 1944 gives the same count', () => {
    const a = open(true);
    sea(a, 'U-boat Fleet', 'natl', { ss: 60 }, 'raid');
    const b = twin(a);
    b.turn += 60;
    expect(convoyCount(b, atlantic)).toEqual(convoyCount(a, atlantic));
  });
});

describe('the staff bases a raider nearer its beat', () => {
  it('moves the U-boat Fleet to a held French port, a month in harbour, with a line of news', () => {
    const g = open(true);
    const u = g.fleets.find((f) => f.name === 'U-boat Fleet')!;
    expect(nearerPort(g, u)).toBeNull();
    for (const r of ['Brittany', 'Nantes', 'Poitou', 'Bordeaux', 'Calais', 'Rouen', 'Cotentin']) hold(g, r, 'GER');
    const to = nearerPort(g, u)!;
    expect(to).not.toBeNull();
    // Every one of them is a sea from the Western Approaches; the staff takes the nearest on the chart.
    expect(passageSeas(g, { nat: 'GER', base: to.id, home: to.seas[0] }, u.station)).toBe(1);
    const before = onStation(g, u, u.station);
    navalAI(g, 'GER');
    expect(u.base).toBe(to.id);
    expect(u.stance).toBe('port');
    expect(onStation(g, u, u.station)).toBeGreaterThan(before);
    expect(g.headlines.some((h) => h.text.includes(`moves its base from Wilhelmshaven to ${to.name}`))).toBe(true);
    // Next month it raids from there, and stays.
    expect(nearerPort(g, u)).toBeNull();
  });

  it('not while the days are unread, nor off, nor for a tied fleet, nor to a port taken this month', () => {
    const g = open(true);
    const u = g.fleets.find((f) => f.name === 'U-boat Fleet')!;
    hold(g, 'Bordeaux', 'GER');
    unread(() => expect(nearerPort(g, u)).toBeNull());
    const off = open(false);
    hold(off, 'Bordeaux', 'GER');
    expect(nearerPort(off, off.fleets.find((f) => f.name === 'U-boat Fleet')!)).toBeNull();
    u.tied = true;
    expect(nearerPort(g, u)).toBeNull();
    u.tied = false;
    g.byName.Bordeaux.since = g.turn;
    expect(nearerPort(g, u)).toBeNull();
  });

  it('never moves the player\'s own fleets: he orders them himself', () => {
    const g = open(true, 1, 'GER');
    const u = g.fleets.find((f) => f.name === 'U-boat Fleet')!;
    hold(g, 'Bordeaux', 'GER');
    const base = u.base;
    navalAI(g, 'GER');
    expect(u.base).toBe(base);
  });

  it('is one rule for every navy: an Italian raider moves as a German one would', () => {
    const g = open(true);
    const it = g.fleets.find((f) => f.nat === 'ITA' && f.duty === 'raid' && f.ships.ss > 0)!;
    it.station = zoneOf(g, 'wapp');
    hold(g, 'Bordeaux', 'ITA');
    const to = nearerPort(g, it);
    expect(to?.name).toBe('Bordeaux');
    expect(fit(g, it)).toBeGreaterThan(0);
  });
});
