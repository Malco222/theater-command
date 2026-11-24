// What a boat that fires puts into one convoy, and escorts without radar that frustrate too (release 2, Units decide;
// the design director's rulings of 24 November 2026 17:27Z and 17:37Z on naval-battles/convoy-r2/load-per-convoy-form.md,
// and 17:13Z, 17:17Z and 17:24Z on eye-term-form.md). Built together, so no working cause goes before its replacement.
// The eye term: the share that fires = 1 / (1 + c_e e + c_r r), e every escort on the stations for each boat trying, r the
// radar-fitted ones, c_e and c_r solved on the record's two points (1941's sample and T2), never on the game. The load:
// salvos of the ready tubes (5), a further approach with q = the firing share x the night a reload fits in x the share
// that regain contact (unread, so one approach), never more than the load; the load a cap for a patrol, not a convoy.
import { afterEach, describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import {
  CAUSE, NAVY, boatCeiling, convoyCount, convoySideOf, eyePair, firingShare, nightFits, salvosInto, screenStations, seaLatitude, torpedoesInto, zoneOf,
} from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { grantTech } from '../src/campaign/tech';
import type { Fleet, Game, Ships } from '../src/campaign/types';
import { dayShare, oddsOf, zoneOfSea } from '../src/campaign/weather';
import { turnToDate } from '../src/core/ticket';

const atlantic = NAVY.convoys.find((r) => r.id === 'atlantic')!;
const HIT = 0.45, SINK = 0.57;

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
/** With some figure set for the length of f, put back after. */
function withFigure<T>(set: () => () => void, f: () => T): T {
  const back = set();
  try { return f(); } finally { back(); }
}
const regainAt = (x: number | null) => (): (() => void) => { const keep = CAUSE.load!.regain; CAUSE.load!.regain = x; return () => { CAUSE.load!.regain = keep; }; };
const eyeUnread = (): (() => void) => { const keep = CAUSE.firing.eye!.effective41; CAUSE.firing.eye!.effective41 = null; return () => { CAUSE.firing.eye!.effective41 = keep; }; };
const loadUnread = (): (() => void) => { const keep = CAUSE.load!.reload; CAUSE.load!.reload = null; return () => { CAUSE.load!.reload = keep; }; };
const nextUnread = (): (() => void) => { const keep = CAUSE.load!.next!.boat; CAUSE.load!.next!.boat = null; return () => { CAUSE.load!.next!.boat = keep; }; };
const patrolDays = (d: number) => (): (() => void) => { const keep = CAUSE.station.ss!.patrol; CAUSE.station.ss!.patrol = d; return () => { CAUSE.station.ss!.patrol = keep; }; };

afterEach(() => { expect(CAUSE.load!.regain).toBeNull(); });

describe('the figures', () => {
  it('lays 5 ready tubes, 1.25 hours to reload them, the share that regain contact unread, and the eye term\'s two record points', () => {
    expect(CAUSE.ceiling.tubes).toBe(5);
    expect({ reload: CAUSE.load!.reload, regain: CAUSE.load!.regain }).toEqual({ reload: 1.25, regain: null });
    expect(CAUSE.firing.eye).toEqual({ effective41: 0.62, n41: 4.8, e41: 1.07, r41: 0.07, effectiveT2: 0.3, nT2: 5.8, eT2: 0.97, rT2: 0.97 });
  });

  it('solves c_e and c_r together on the record\'s points, record against record: about 0.36 and 1.94, 0.66 firing in 1941 and 0.31 at T2', () => {
    const p = eyePair()!;
    const fired41 = 0.62 / (1 - (1 - HIT) ** 4.8), firedT2 = 0.3 / (1 - (1 - HIT) ** 5.8);
    expect(fired41).toBeCloseTo(0.66, 2);
    expect(firedT2).toBeCloseTo(0.31, 2);
    expect(p.ce).toBeCloseTo(0.36, 2);
    expect(p.cr).toBeCloseTo(1.94, 2);
    expect(p.t2).toBeCloseTo(firedT2, 12);
    // (Each point is met exactly.)
    expect(1 / (1 + p.ce * 1.07 + p.cr * 0.07)).toBeCloseTo(fired41, 12);
    expect(1 / (1 + (p.ce + p.cr) * 0.97)).toBeCloseTo(firedT2, 12);
  });

  it('the check, never fitted: HX 229/SC 122, e and r about 0.5, lets about 0.46 fire', () => {
    const p = eyePair()!;
    expect(Math.abs(1 / (1 + (p.ce + p.cr) * 0.5) - 0.46)).toBeLessThan(0.02);
  });

  it('a boat that fired in 1941 put in about one salvo of 5: the record\'s n of 4.8, never fitted', () => {
    expect(Math.abs(CAUSE.ceiling.tubes! - 4.8)).toBeLessThan(0.5);
    // (About 2.2 hits an approach, against 2.3 a boat a convoy in 1941 and 2.7 at T2: the product's check.)
    expect(CAUSE.ceiling.tubes! * HIT).toBeCloseTo(2.25, 12);
  });
});

describe('the eye term', () => {
  it('escorts without radar frustrate too: every escort on the stations, for each boat trying, at c_e', () => {
    const g = open(true);
    meet(g, 60, 320);
    const p = eyePair()!, c = convoyCount(g, atlantic)!, s = convoySideOf(g, 'UK', atlantic)!;
    const tries = c.pack / p.t2;
    // (320 destroyers on the route fill all the stations, so the stations are the escorts that find.)
    expect(s.fire).toBeCloseTo(1 / (1 + (p.ce * screenStations(atlantic)!) / tries), 12);
    expect(s.fire!).toBeLessThan(1);
    expect(s.fired).toBeCloseTo(tries * s.fire!, 12);
  });

  it('a radar-fitted escort counts in both, so radar lets fewer fire than the eye alone', () => {
    const g = open(true);
    meet(g, 60, 320);
    const eye = convoySideOf(g, 'UK', atlantic)!;
    grantTech(g, 'UK', 'asvradar');
    const both = convoySideOf(g, 'UK', atlantic)!;
    const p = eyePair()!, c = convoyCount(g, atlantic)!;
    expect(both.fire).toBeCloseTo(1 / (1 + ((p.ce + p.cr) * screenStations(atlantic)!) / (c.pack / p.t2)), 12);
    expect(both.fire!).toBeLessThan(eye.fire!);
  });

  it('a thin screen finds less: fewer escorts on the stations, more boats fire', () => {
    const g = open(true);
    meet(g, 60, 6);
    const thin = convoySideOf(g, 'UK', atlantic)!;
    meet(g, 60, 320);
    expect(convoySideOf(g, 'UK', atlantic)!.fire!).toBeLessThan(thin.fire!);
    expect(thin.fire!).toBeLessThan(1);
  });

  it('at T2\'s own point the pack comes back (the pack only: a firing boat\'s ships are the ceiling\'s, below)', () => {
    const p = eyePair()!;
    for (const pack of [0.5, 2, 3.5]) expect((pack / p.t2) / (1 + (p.ce + p.cr) * 0.97)).toBeCloseTo(pack, 12);
  });

  it('unread, radar alone as first ruled', () => {
    const g = open(true);
    grantTech(g, 'UK', 'asvradar');
    meet(g, 60, 80);
    withFigure(eyeUnread, () => {
      expect(eyePair()).toBeNull();
      const s = convoySideOf(g, 'UK', atlantic)!;
      expect(s.fire).toBeLessThan(1);
      // (The boats that try are the pack / radar alone's t2, and the share is radar alone's; its r is pinned exactly in
      // convoy-firing-r2 and convoy-screen-r2.)
      expect(s.fired).toBeCloseTo((convoyCount(g, atlantic)!.pack / 0.3) * s.fire!, 12);
    });
    // (And without radar, unread, every boat fires.)
    const g2 = open(true);
    meet(g2, 60, 320);
    withFigure(eyeUnread, () => expect(convoySideOf(g2, 'UK', atlantic)!.fire).toBe(1));
  });

  it('the eye reads only its own figures: a null in radar alone\'s t2 or rT2 leaves it running', () => {
    const g = open(true);
    grantTech(g, 'UK', 'asvradar');
    meet(g, 60, 80);
    const laid = convoySideOf(g, 'UK', atlantic)!;
    for (const key of ['t2', 'rT2'] as const) {
      const keep = CAUSE.firing[key];
      CAUSE.firing[key] = null;
      try {
        const s = convoySideOf(g, 'UK', atlantic)!;
        expect(s.fire).toBeCloseTo(laid.fire!, 12);
        expect(s.fired).toBeCloseTo(laid.fired!, 12);
      } finally { CAUSE.firing[key] = keep; }
    }
    // (With the eye unread as well, nothing fires short: the count's pack, as before the firing share.)
    withFigure(eyeUnread, () => {
      const keep = CAUSE.firing.t2;
      CAUSE.firing.t2 = null;
      try { expect(convoySideOf(g, 'UK', atlantic)!.fire).toBeUndefined(); } finally { CAUSE.firing.t2 = keep; }
    });
  });
});

describe('the load', () => {
  it('salvos into a convoy: the first, then 1 + q + q^2 ... over load / tubes, the last a part salvo', () => {
    expect(salvosInto(0, 2.8)).toBe(1);
    expect(salvosInto(1, 2.8)).toBeCloseTo(2.8, 12);
    expect(salvosInto(0.5, 2.8)).toBeCloseTo(1 + 0.5 + 0.8 * 0.25, 12);
    expect(salvosInto(0.5, 0.6)).toBeCloseTo(0.6, 12);
    expect(salvosInto(0.9, 0)).toBe(0);
  });

  it('with the share that regain contact unread (and the next night unread too), a boat that fires makes one approach: 5 torpedoes, 1.28 ships at most, whatever the sea', () => {
    const g = open(true);
    for (const k of ['natl', 'wapp']) {
      const z = zoneOf(g, k);
      expect(torpedoesInto(g, z)).toBe(5);
      expect(boatCeiling(g, z)).toBeCloseTo(5 * HIT * SINK, 12);
    }
    meet(g, 60, 6);
    // (The next night, laid since (tests/convoy-regain-r2), is the route's further approach: unread here, one approach.)
    withFigure(nextUnread, () => {
      const s = convoySideOf(g, 'UK', atlantic)!;
      expect(s.torps).toBeCloseTo(Math.max(1, s.fired!) * 5 * HIT * SINK, 12);
    });
  });

  it('with it laid, a further approach goes ahead with the firing share x the night a reload fits in x the share that regain', () => {
    const g = open(true);
    const z = zoneOf(g, 'natl');
    withFigure(regainAt(0.5), () => {
      const fits = nightFits(g, z);
      expect(fits).toBeGreaterThan(0);
      const load = withFigure(loadUnread, () => torpedoesInto(g, z));
      for (const fire of [1, 0.6, 0.3]) {
        const t = torpedoesInto(g, z, fire);
        expect(t).toBeGreaterThan(5);
        expect(t).toBeLessThanOrEqual(14);
        expect(t).toBeCloseTo(5 * salvosInto(fire * fits * 0.5, load / 5), 12);
        expect(boatCeiling(g, z, fire)).toBeCloseTo(t * HIT * SINK, 12);
      }
      // (A weak screen lets a boat back in more often than a strong one.)
      expect(torpedoesInto(g, z, 1)).toBeGreaterThan(torpedoesInto(g, z, 0.3));
    });
  });

  it('a reload fits a long night better than a short one, and none where the sun never sets', () => {
    const g = open(true);
    const z = zoneOf(g, 'natl');
    const at = (turn: number): number => { g.turn = turn; return nightFits(g, z); };
    // (Turn 4 is December 1939, turn 10 June 1940.)
    const dec = at(4), jun = at(10);
    expect(dec).toBeGreaterThan(jun);
    g.turn = 10;
    const dark = 24 * (1 - dayShare(seaLatitude(g, z), turnToDate(g.turn).month));
    expect(jun).toBeCloseTo(Math.max(0, 1 - 1.25 / dark), 12);
    expect(Math.max(0, 1 - 1.25 / (24 * (1 - dayShare(70, 5))))).toBe(0);
    // (The game's own northernmost seas under the midnight sun: nightFits itself gives none.)
    const polar = g.seas.map((s) => s.id).filter((id) => dayShare(seaLatitude(g, id), 5) >= 1);
    expect(polar.length).toBeGreaterThan(0);
    g.turn = 10;
    for (const id of polar) expect(nightFits(g, id)).toBe(0);
  });

  it('the load is a cap for a patrol: past what the boats on station hold this month, each convoy\'s share falls alike', () => {
    const g = open(true);
    meet(g, 60, 6);
    const shipped = convoySideOf(g, 'UK', atlantic)!;
    // (At 20 days on station it does not bind here: each firing boat's torpedoes, with the next night as laid.)
    const z0 = zoneOf(g, 'natl'), each = torpedoesInto(g, z0, shipped.fire!, { route: 'atlantic', nat: 'GER' });
    expect(each).toBeGreaterThan(5);
    expect(shipped.torps).toBeCloseTo(Math.max(1, shipped.fired!) * each * HIT * SINK, 12);
    // (A boat that held one load for 4,000 days on station: the month's torpedoes are what the boats on station hold.)
    withFigure(patrolDays(4000), () => {
      const s = convoySideOf(g, 'UK', atlantic)!, c = convoyCount(g, atlantic)!;
      const z = zoneOf(g, 'natl'), o = oddsOf(zoneOfSea(g, z), g.turn), load = 12 + 2 * (o[0] / (o[0] + o[1] + o[2]));
      expect(s.torps!).toBeLessThan(shipped.torps!);
      expect((s.torps! * c.attacked) / (HIT * SINK)).toBeCloseTo((c.boats * load * 30) / 4000, 9);
    });
  });

  it('unread, the whole load into one convoy, as first built', () => {
    const g = open(true);
    const z = zoneOf(g, 'natl');
    withFigure(loadUnread, () => {
      expect(torpedoesInto(g, z)).toBeGreaterThanOrEqual(12);
      expect(boatCeiling(g, z)).toBeCloseTo(torpedoesInto(g, z) * HIT * SINK, 12);
    });
  });
});

describe('off', () => {
  it('reads nothing', () => {
    const g = open(false);
    meet(g, 60, 320);
    expect(convoySideOf(g, 'UK', atlantic)).toBeUndefined();
  });
});
