// The escort's incidents and kills by cause (release 2, Units decide; the design director's ruling of 24 November 2026
// 17:58Z on naval-battles/convoy-r2/escort-losses-form.md, (a) on Q1 to Q4 with his notes of 17:59Z and 18:00Z, and Q5,
// 'price once', from his ruling of 16:58Z). For each convoy attacked the escort meets the boats that try x (1 - f^2): the
// screen frustrates 1 - f of them and finds those that fire at a = 1 - f, the same contest. An incident is coordinated
// with the chance spare / (spare + on stations), spare the escorts with the convoy beyond the close screen's stations
// and the support groups in the route's seas, shared over the route's convoys at sea at once; independent otherwise.
// Escort ships sunk keep the flat escortLoss; boats not raiding keep the hunt's curve; nothing is counted twice. The
// contest's E is the escorts on the stations, k solved once on it at the tons ruling's own record point on the record's
// own E there (the design director's rulings of 22:24Z: k, 1 (b); the groups shared, 3 (b)). One switch,
// convoycause.json 'escorts': null, the game is the build before it. Nothing in it reads the date or a nation's flag.
import { afterEach, describe, expect, it } from 'vitest';
import { createGame, declareWar } from '../src/campaign/game';
import { CAUSE, FLEETS, NAVY, aswKit, convoyCount, convoyField, convoyMeeting, convoySideOf, convoyWar, escortChance, escortWork, escortsAtSea, escortsMet, eyePair, fit, huntExpected, huntShareSunk, huntSubs, huntersIn, navalUpkeep, onStation, screenStations, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { grantTech, techById, techRule } from '../src/campaign/tech';
import type { Fleet, Game, Ships } from '../src/campaign/types';

const atlantic = NAVY.convoys.find((r) => r.id === 'atlantic')!;
const libya = NAVY.convoys.find((r) => r.id === 'libya')!;
const KIT = ['sonar', 'asvradar', 'patrolaircraft', 'hedgehog', 'escortgroups'];

/** A grand campaign at its start, with Units decide on or off, no weather, and every fleet in harbour. */
function open(on: boolean, seed = 1): Game {
  const g = startOpen(createGame(seed, null));
  if (on) g.flags.unitsDecide = 1; else delete g.flags.unitsDecide;
  delete g.flags.weather;
  for (const f of g.fleets) f.stance = 'port';
  return g;
}
const none = (): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 });
/** One of this nation's fleets (not one of 'skip'), put to sea in a sea with only these ships, fully ready. */
function sea(g: Game, nat: string, zone: string, ships: Partial<Ships>, stance: Fleet['stance'], lane?: string, skip: Fleet[] = []): Fleet {
  const f = g.fleets.find((x) => x.nat === nat && !skip.includes(x))!;
  f.ships = { ...none(), ...ships };
  f.zone = zoneOf(g, zone);
  f.stance = stance;
  if (lane) f.lane = lane; else delete f.lane;
  f.ready = 1;
  return f;
}
const give = (g: Game, nat: string, ids: string[]): void => { for (const id of ids) grantTech(g, nat, id); };
/** Runs with some rows set, and puts them back. */
function withRows<T>(set: () => void, put: () => void, f: () => T): T { set(); try { return f(); } finally { put(); } }
/** Runs with the form's one switch null: the build before it. */
const unread = <T>(f: () => T): T => { const keep = CAUSE.escorts; return withRows(() => { CAUSE.escorts = null; }, () => { CAUSE.escorts = keep; }, f); };
/** The record's whole-number run about an expectation: the month books its floor or one more. */
const run = (x: number): [number, number] => [Math.floor(x), Math.floor(x) + 1];
/** OEG Table 10's column for a kit, as the test reads the figures itself: coordinated or independent, with its kit steps. */
function column(g: Game, nat: string, coordinated: boolean): number {
  const k = aswKit(g, nat), I = CAUSE.incident;
  if (k.hedgehog && k.groups) return coordinated ? I.coordinatedKit : I.independentKit;
  return (coordinated ? I.coordinated : I.independent) * (k.hedgehog ? I.atw : 1);
}
/** Runs with the boat ceiling unread, so the contest itself is read. */
const bare = <T>(f: () => T): T => { const keep = CAUSE.ceiling.inside; return withRows(() => { CAUSE.ceiling.inside = null; }, () => { CAUSE.ceiling.inside = keep; }, f); };
/** The route's escort work and the convoy count as the fleets stand. */
const work = (g: Game, route = atlantic, nat = 'UK'): NonNullable<ReturnType<typeof escortWork>> => escortWork(g, nat, route, convoyCount(g, route))!;
/** The boats a raider is expected to lose to the convoy's escort this month, as the meeting gives it. */
const expectOf = (g: Game, f: Fleet, route = atlantic, nat = 'UK'): number => convoyMeeting(g, nat, route).raiders.find((r) => r.id === f.id)!.expect;
/** A pack in the North Atlantic and a close escort bound to the Atlantic route. */
function meet(g: Game, ss: number, dd: number): { boats: Fleet; esc: Fleet } {
  const esc = sea(g, 'UK', 'wapp', { dd }, 'escort', 'atlantic');
  return { boats: sea(g, 'GER', 'natl', { ss }, 'raid'), esc };
}

afterEach(() => { convoyField.fought = undefined; });

describe('the figures', () => {
  it('lays the form, ruled into the next bundle, and k on the stations at 0.984', () => {
    expect(CAUSE.escorts).toBe(true);
    expect(CAUSE.count.sinkOnStations).toBe(0.984);
    // (The tons ruling's k stands beside it, for the switch null.)
    expect(CAUSE.count.sink).toBe(0.137);
  });

  it('solves k once at the tons ruling\'s own record point on the record\'s own E there: 0.596 x (44 / 3.75 - 1) / 6.5 = 0.984', () => {
    // (OEG T2's window: 2 boats sink 3.75 of the convoy's 44 ships a convoy attacked. P the tons ruling's own, 2 boats
    // x 0.298, a boat's raid strength on station in its six AI wars; E the record's escorts a convoy there, about 6.5,
    // OEG ch. 10 Table 5, as the eye's T2 point reads them: eT2 for each of the 2 / 0.3 boats trying.)
    const P = 2 * 0.298, ships = 44, sunk = 3.75, E = 6.5;
    expect(P).toBeCloseTo(0.596, 12);
    expect(ships / sunk - 1).toBeCloseTo(10.733333, 6);
    const recordE = CAUSE.firing.eye!.eT2! * (CAUSE.count.perAttack / CAUSE.firing.t2!);
    expect(recordE).toBeCloseTo(6.4667, 4);
    expect(recordE).toBeCloseTo(E, 1);
    const k = (P * (ships / sunk - 1)) / E;
    expect(k).toBeCloseTo(0.596 * 10.733333 / 6.5, 6);
    expect(k).toBeCloseTo(0.984164, 6);
    expect(CAUSE.count.sinkOnStations).toBeCloseTo(k, 3);
    // The laid figure gives the record point back on the record's E.
    expect((ships * P) / (P + CAUSE.count.sinkOnStations! * E)).toBeCloseTo(3.7506, 4);
    expect((ships * P) / (P + CAUSE.count.sinkOnStations! * E)).toBeCloseTo(3.75, 2);
  });

  it('the game\'s E on the stations is high, the named fault: the 44-ship convoy\'s stations, 10.19, against the record\'s 6.5', () => {
    const P = 0.596, ships = 44, stations = screenStations(atlantic)!;
    expect(stations).toBeCloseTo(6 + 44 / 3.5 / 3, 12);
    expect(stations).toBeCloseTo(10.190476, 6);
    // (On the game's full stations 2 boats sink 2.47 ships, not 3.75: shown, never tuned.)
    expect((ships * P) / (P + CAUSE.count.sinkOnStations! * stations)).toBeCloseTo(2.4685, 3);
    // (Refused: solved on the game's 10.19, k would be 0.628, the fault baked into the constant.)
    const refused = (P * (ships / 3.75 - 1)) / stations;
    expect(refused).toBeCloseTo(0.62775, 5);
    expect(CAUSE.count.sinkOnStations).not.toBeCloseTo(refused, 2);
    // (Where the form is laid it replaces the tons ruling's 0.137, solved on the game's escort worth a convoy there, 46.5,
    // with the same inflation; 0.137 stands beside it for the form unread.)
    expect((P * (ships / 3.75 - 1)) / 46.5).toBeCloseTo(0.137571, 6);
    expect((ships * P) / (P + CAUSE.count.sink! * 46.5)).toBeCloseTo(3.75, 1);
  });

  it('the game\'s T2 fills the stations: with about 35 escorts a convoy, E is the stations, and 2 boats sink 2.47 ships there', () => {
    const g = open(true);
    // (About 35 escorts with each Atlantic convoy, as the game has them at T2's window.)
    meet(g, 60, 300);
    const s = bare(() => convoySideOf(g, 'UK', atlantic)!), w = work(g);
    expect(w.on + w.beyond).toBeGreaterThan(30);
    expect(s.E).toBeCloseTo(screenStations(atlantic)!, 12);
    expect((44 * 0.596) / (0.596 + CAUSE.count.sinkOnStations! * s.E)).toBeCloseTo(2.4685, 3);
  });
});

describe('Q1 (a): the incidents a convoy attacked, by cause', () => {
  it('are the boats that try x (1 - f^2): the frustrated, tries x (1 - f), and those found after firing, tries x f x (1 - f)', () => {
    expect(escortsMet(10, 0.3)).toEqual({ frustrated: 7, after: expect.closeTo(2.1, 12), met: expect.closeTo(9.1, 12) });
    // The form's figures: the game at T2, 10.3 tries at f 0.304; the record's T2, 6.7 tries at 0.3; the game's 1941.
    expect(escortsMet(10.3, 0.304).met).toBeCloseTo(10.3 * (1 - 0.304 ** 2), 12);
    expect(escortsMet(10.3, 0.304).met).toBeCloseTo(9.35, 2);
    expect(escortsMet(2 / 0.3, 0.3).met).toBeCloseTo(6.07, 2);
    expect(escortsMet(6.6, 0.57).met).toBeCloseTo(4.46, 2);
    // A screen that frustrates nothing finds nothing after firing: no incident; one that frustrates all, every try.
    expect(escortsMet(8, 1).met).toBe(0);
    expect(escortsMet(8, 0).met).toBe(8);
    // The after-firing part is largest where half fire.
    expect(escortsMet(1, 0.5).after).toBeGreaterThan(escortsMet(1, 0.4).after);
    expect(escortsMet(1, 0.5).after).toBeGreaterThan(escortsMet(1, 0.6).after);
  });

  it('read the same screen contest as the convoy\'s side: its tries, its share that fire, and a = 1 - f with no new figure', () => {
    const g = open(true);
    meet(g, 60, 30);
    const c = convoyCount(g, atlantic)!, w = work(g), s = convoySideOf(g, 'UK', atlantic)!, eye = eyePair()!;
    expect(w.tries).toBeCloseTo(c.pack / eye.t2, 12);
    expect(w.fire).toBe(s.fire);
    expect(w.tries * w.fire).toBeCloseTo(s.fired!, 12);
    expect(w.frustrated).toBeCloseTo(w.tries * (1 - s.fire!), 12);
    expect(w.after).toBeCloseTo(s.fired! * (1 - s.fire!), 12);
    expect(w.met).toBeCloseTo(w.tries * (1 - s.fire! ** 2), 12);
    // More escorts on the stations frustrate more and find more after firing too.
    meet(g, 60, 60);
    const more = work(g);
    expect(more.fire).toBeLessThan(w.fire);
    expect(more.frustrated).toBeGreaterThan(w.frustrated);
    expect(more.met).toBeGreaterThan(w.met);
  });

  it('with the eye unread, radar alone\'s contest; with no firing share laid, f = 1 and no boat meets the escort', () => {
    const g = open(true);
    const { boats } = meet(g, 60, 30);
    const eye = CAUSE.firing.eye!.effective41, t2 = CAUSE.firing.t2;
    withRows(() => { CAUSE.firing.eye!.effective41 = null; }, () => { CAUSE.firing.eye!.effective41 = eye; }, () => {
      // (Radar alone: an escort without the set lets every boat fire, so it neither frustrates nor finds.)
      expect(work(g).fire).toBe(1);
      expect(work(g).met).toBe(0);
      expect(expectOf(g, boats)).toBe(0);
      give(g, 'UK', ['asvradar']);
      const w = work(g), s = convoySideOf(g, 'UK', atlantic)!;
      expect(w.tries).toBeCloseTo(convoyCount(g, atlantic)!.pack / 0.3, 12);
      expect(w.fire).toBe(s.fire);
      expect(w.fire).toBeLessThan(1);
      expect(w.met).toBeCloseTo(w.tries * (1 - w.fire ** 2), 12);
      expect(expectOf(g, boats)).toBeGreaterThan(0);
      withRows(() => { CAUSE.firing.t2 = null; }, () => { CAUSE.firing.t2 = t2; }, () => {
        expect(work(g)).toMatchObject({ tries: convoyCount(g, atlantic)!.pack, fire: 1, met: 0, rate: 0 });
        expect(expectOf(g, boats)).toBe(0);
      });
    });
  });

  it('replace the count\'s 0.21 a boat on station: the route\'s month falls to its raiders by their share of its boats on station', () => {
    const g = open(true);
    const { boats } = meet(g, 60, 30);
    // (A second pack, in another sea of the route, from a port of its own.)
    const other = sea(g, 'GER', 'irish', { ss: 30 }, 'raid', undefined, [boats]);
    const c = convoyCount(g, atlantic)!, w = work(g);
    expect(w.rate * c.boats).toBeCloseTo(c.attacked * w.met, 12);
    expect(w.rate).not.toBeCloseTo(0.21, 2);
    // (No group and no escort past the stations: each incident at the close escort's 0.05, a full-ready ship.)
    for (const f of [boats, other]) {
      const ch = escortChance(g, 'UK', w);
      expect(ch.chance).toBeCloseTo(0.05, 12);
      expect(expectOf(g, f)).toBeCloseTo(f.ships.ss * onStation(g, f, f.zone) * w.rate * ch.chance, 12);
    }
    // The two packs' expectations together are the route's month of incidents at that rate.
    expect(expectOf(g, boats) + expectOf(g, other)).toBeCloseTo(c.attacked * w.met * 0.05, 12);
  });
});

describe('Q2 (a): who works an incident', () => {
  it('with no spare ship every incident is independent: the close escort\'s column, with its kit steps', () => {
    const g = open(true);
    const { esc } = meet(g, 60, 30);
    const w = work(g);
    expect(w.on).toBeLessThan(screenStations(atlantic)!);
    expect(w.beyond).toBe(0);
    expect(escortChance(g, 'UK', w)).toEqual({ coordinated: 0, chance: expect.closeTo(0.05 * fit(g, esc), 12) });
    give(g, 'UK', ['sonar', 'hedgehog']);
    expect(escortChance(g, 'UK', work(g)).chance).toBeCloseTo(0.05 * 1.9 * fit(g, esc), 12);
    give(g, 'UK', ['escortgroups']);
    expect(escortChance(g, 'UK', work(g)).chance).toBeCloseTo(0.12 * fit(g, esc), 12);
  });

  it('escorts beyond the screen\'s stations are spare: they work incidents as coordinated hunts', () => {
    const g = open(true);
    meet(g, 60, 300);
    const w = work(g), stations = screenStations(atlantic)!;
    expect(w.on).toBeCloseTo(stations, 12);
    expect(w.beyond).toBeGreaterThan(stations);
    const ch = escortChance(g, 'UK', w);
    expect(ch.coordinated).toBeCloseTo(w.beyond / (w.beyond + w.on), 12);
    expect(ch.chance).toBeCloseTo((w.on * 0.05 + w.beyond * 0.17) / (w.on + w.beyond), 12);
    give(g, 'UK', ['sonar', 'hedgehog', 'escortgroups']);
    const k = work(g);
    expect(escortChance(g, 'UK', k).chance).toBeCloseTo((k.on * 0.12 + k.beyond * 0.55) / (k.on + k.beyond), 12);
  });

  it('a support group in any of the route\'s seas raises the coordinated share in all of them, at its own kit; one off the route does not', () => {
    const g = open(true);
    const { boats, esc } = meet(g, 60, 30);
    const before = escortChance(g, 'UK', work(g));
    expect(before.coordinated).toBe(0);
    // (A group hunting in the North Sea, off the route: not the route's.)
    const away = sea(g, 'UK', 'nsea', { dd: 20 }, 'hunt', undefined, [esc]);
    expect(escortChance(g, 'UK', work(g))).toEqual(before);
    // (A group hunting in the Western Approaches, a sea of the route the pack is not in: spare for the pack's incidents in
    // the North Atlantic, as the ruling words it, 'the hunting-duty destroyers in the route's seas'.)
    const group = sea(g, 'UK', 'wapp', { dd: 20 }, 'hunt', undefined, [esc, away]);
    expect(group.zone).not.toBe(boats.zone);
    const w = work(g), ch = escortChance(g, 'UK', w), n = w.convoys;
    expect(w.seas).toEqual(atlantic.zones.map((k) => zoneOf(g, k)));
    // (Shared over the route's convoys at sea at once, one convoy's part of the group.)
    expect(n).toBeGreaterThan(1);
    expect(ch.coordinated).toBeCloseTo((w.beyond + 20 / n) / (w.beyond + 20 / n + w.on), 12);
    expect(ch.coordinated).toBeGreaterThan(0.2);
    expect(ch.chance).toBeCloseTo((w.on * 0.05 * fit(g, esc) + (20 / n) * 0.17 * fit(g, group)) / (w.on + 20 / n), 12);
    // The boats in the North Atlantic lose more to the convoy's escort for it, and every sea of the route reads alike.
    expect(expectOf(g, boats)).toBeGreaterThan(expectWithout(g, group, boats));
    expect(expectOf(g, boats)).toBeCloseTo(60 * onStation(g, boats, boats.zone) * w.rate * ch.chance, 12);
    give(g, 'UK', ['sonar', 'hedgehog', 'escortgroups']);
    const k = work(g);
    expect(escortChance(g, 'UK', k).chance).toBeCloseTo((k.on * 0.12 + (20 / k.convoys) * 0.55) / (k.on + 20 / k.convoys), 12);
    // (Nothing is counted twice: the group hunts on the curve only in its own sea, and the pack in the North Atlantic
    // meets it only in its incidents. The look-ahead there is the aircraft's part of the curve and the incidents.)
    const h = huntersIn(g, 'UK', boats.zone);
    expect(h.fleets).not.toContain(group);
    const air = h.weight > 0 ? (60 * huntShareSunk(h.weight, 60) * h.air!) / h.weight : 0;
    expect(huntExpected(g, 'UK', boats.zone, [boats])).toBeCloseTo(air + expectOf(g, boats), 12);
  });

  it('each ship counts by its readiness: the close escort and the support group each at their own fit', () => {
    const g = open(true);
    const { esc } = meet(g, 60, 30);
    const group = sea(g, 'UK', 'natl', { dd: 20 }, 'hunt', undefined, [esc]);
    esc.ready = 0.5;
    group.ready = 0.8;
    const fe = fit(g, esc), fg = fit(g, group), L = FLEETS.escort.loose;
    expect(fe).toBeLessThan(fg);
    expect(fg).toBeLessThan(fit(g, { ...group, ready: 1 }));
    // (The screen counts every allied fleet in the route's seas at its share on the route, as routeForces does: the
    // escort bound to it whole, the group hunting in one of its seas at the loose share. Each ship at its own fit.)
    const w = work(g), ch = escortChance(g, 'UK', w), n = w.convoys;
    expect(w.beyond).toBe(0);
    expect(w.onRate).toBeCloseTo((0.05 * (30 * fe + 20 * L * fg)) / (30 + 20 * L), 12);
    expect(ch.chance).toBeCloseTo((w.on * w.onRate + (20 / n) * 0.17 * fg) / (w.on + 20 / n), 12);
    // (Past the stations, the spare escorts too at their own fit.)
    meet(g, 60, 300);
    esc.ready = 0.5;
    const m = work(g), mc = escortChance(g, 'UK', m);
    expect(m.beyond).toBeGreaterThan(0);
    expect(m.onRate).toBeCloseTo((0.05 * (300 * fe + 20 * L * fg)) / (300 + 20 * L), 12);
    expect(m.beyondRate).toBeCloseTo((0.17 * (300 * fe + 20 * L * fg)) / (300 + 20 * L), 12);
    expect(mc.chance).toBeCloseTo((m.on * m.onRate + m.beyond * m.beyondRate + (20 / m.convoys) * 0.17 * fg) / (m.on + m.beyond + 20 / m.convoys), 12);
  });

  it('kills are the incidents x the column\'s rate: a month\'s expectation, by kit', () => {
    for (const kit of [[], ['sonar', 'hedgehog'], ['sonar', 'hedgehog', 'escortgroups']]) {
      const g = open(true);
      give(g, 'UK', kit);
      const { boats, esc } = meet(g, 60, 300);
      sea(g, 'UK', 'natl', { dd: 20 }, 'hunt', undefined, [esc]);
      const w = work(g), ch = escortChance(g, 'UK', w);
      const k = aswKit(g, 'UK'), ind = k.hedgehog && k.groups ? 0.12 : 0.05 * (k.hedgehog ? 1.9 : 1), co = k.hedgehog && k.groups ? 0.55 : 0.17 * (k.hedgehog ? 1.9 : 1);
      const groups = 20 / w.convoys;
      expect(ch.chance, kit.join(',')).toBeCloseTo((w.on * ind + (w.beyond + groups) * co) / (w.on + w.beyond + groups), 12);
      expect(expectOf(g, boats), kit.join(',')).toBeCloseTo(60 * onStation(g, boats, boats.zone) * w.rate * ch.chance, 12);
    }
  });

  it('one rule for every navy: a German screen on Italy\'s route and a British one on Britain\'s, with the same kit, work an incident alike', () => {
    for (const dd of [20, 300]) {
      for (const kit of [[], ['sonar'], ['sonar', 'hedgehog'], ['sonar', 'hedgehog', 'escortgroups'], KIT]) {
        const what = `${dd} ${kit.join(',')}`;
        const g = open(true);
        declareWar(g, 'ITA', 'UK');
        for (const nat of ['UK', 'GER']) g.nations[nat].tech!.done = g.nations[nat].tech!.done.filter((id) => !KIT.includes(id));
        give(g, 'UK', kit); give(g, 'GER', kit);
        const ger = sea(g, 'GER', 'natl', { ss: 10 }, 'raid');
        const uk = sea(g, 'UK', 'wapp', { dd }, 'escort', 'atlantic');
        const brit = sea(g, 'UK', 'central', { ss: 10 }, 'raid', undefined, [uk]);
        const gesc = sea(g, 'GER', 'central', { dd }, 'escort', 'libya', [ger]);
        // (A little worn, and each fleet at its own readiness.)
        uk.ready = 0.7; gesc.ready = 0.9;
        const a = work(g, atlantic, 'UK'), b = work(g, libya, 'ITA');
        // The columns: each ship at its kit's column and its own fit, whatever its flag.
        expect(column(g, 'GER', false), what).toBe(column(g, 'UK', false));
        expect(b.onRate / fit(g, gesc), what).toBeCloseTo(a.onRate / fit(g, uk), 12);
        expect(b.beyondRate / fit(g, gesc), what).toBeCloseTo(a.beyondRate / fit(g, uk), 12);
        // Who works an incident, and the meeting's expectation: one rule, each route at its own screen.
        for (const [nat, route, w, esc, boats] of [['UK', atlantic, a, uk, ger], ['ITA', libya, b, gesc, brit]] as const) {
          const stations = screenStations(route)!;
          expect(w.on, `${what} ${nat}`).toBeCloseTo(Math.min(stations, w.on + w.beyond), 12);
          if (dd === 300) expect(w.beyond, `${what} ${nat}`).toBeGreaterThan(0);
          else expect(w.beyond, `${what} ${nat}`).toBe(0);
          const ch = escortChance(g, nat, w), q = fit(g, esc);
          expect(ch.coordinated, `${what} ${nat}`).toBeCloseTo(w.beyond / (w.beyond + w.on), 12);
          expect(ch.chance, `${what} ${nat}`).toBeCloseTo((q * (w.on * column(g, esc.nat, false) + w.beyond * column(g, esc.nat, true))) / (w.on + w.beyond), 12);
          expect(w.met, `${what} ${nat}`).toBeCloseTo(w.tries * (1 - w.fire ** 2), 12);
          expect(expectOf(g, boats, route, nat), `${what} ${nat}`).toBeCloseTo(boats.ships.ss * onStation(g, boats, boats.zone) * w.rate * ch.chance, 12);
        }
      }
    }
  });
});

describe('the support groups shared over the route\'s convoys at sea at once (the design director\'s ruling of 22:24Z, 3 (b))', () => {
  /** The spare ships in one convoy's split, as an incident's coordinated share gives them back: spare = c x on / (1 - c). */
  const spareOf = (ch: { coordinated: number }, w: { on: number }): number => (ch.coordinated * w.on) / (1 - ch.coordinated);
  const tons = atlantic.tons;
  /** Runs with the Atlantic route's tons at this many times its own, so it sails that many times the convoys. */
  const timesTons = <T>(x: number, f: () => T): T => withRows(() => { atlantic.tons = x * tons; }, () => { atlantic.tons = tons; }, f);

  it('the convoys at sea at once are the count the screen already shares the escorts over; with the cycle unread, the convoys sailed', () => {
    const g = open(true);
    meet(g, 60, 300);
    const c = convoyCount(g, atlantic)!, w = work(g), D = CAUSE.station.dd!;
    // (The convoys sailed a month x a convoy's crossing over the month's 30 days: about 3.3 on the Atlantic route.)
    expect(w.convoys).toBeCloseTo((c.sailed * D.crossing!) / 30, 12);
    expect(w.convoys).toBeCloseTo(3.3, 1);
    // The escorts with a convoy are the escorts at sea with the convoys over that same count.
    expect(w.on + w.beyond).toBeCloseTo((300 * escortsAtSea()!) / w.convoys, 9);
    const crossing = D.crossing;
    withRows(() => { D.crossing = null; }, () => { D.crossing = crossing; }, () => {
      const u = work(g);
      expect(u.convoys).toBe(c.sailed);
      expect(u.on + u.beyond).toBeCloseTo(300 / c.sailed, 9);
    });
  });

  it('a 20-destroyer group over N convoys at sea adds 20 / N spare to each convoy\'s split; over twice the convoys, half', () => {
    const g = open(true);
    const { esc } = meet(g, 60, 300);
    const group = sea(g, 'UK', 'natl', { dd: 20 }, 'hunt', undefined, [esc]);
    const w = work(g), n = w.convoys, ch = escortChance(g, 'UK', w);
    expect(n).toBeGreaterThan(1);
    expect(spareOf(ch, w) - w.beyond).toBeCloseTo(20 / n, 9);
    expect(ch.coordinated).toBeCloseTo((w.beyond + 20 / n) / (w.on + w.beyond + 20 / n), 12);
    expect(ch.chance).toBeCloseTo((w.on * w.onRate + w.beyond * w.beyondRate + (20 / n) * 0.17 * fit(g, group)) / (w.on + w.beyond + 20 / n), 12);
    // (Counted whole with each convoy, as built before, it would be priced n times over.)
    expect(ch.coordinated).toBeLessThan((w.beyond + 20) / (w.on + w.beyond + 20));
    // (The screen's loose share of the group among the escorts with each convoy stays as it was: the overlap, named.)
    expect(w.on + w.beyond).toBeCloseTo(((300 + 20 * FLEETS.escort.loose) * escortsAtSea()!) / n, 9);
    timesTons(2, () => {
      const w2 = work(g), c2 = escortChance(g, 'UK', w2);
      expect(w2.convoys).toBeCloseTo(2 * n, 9);
      expect(spareOf(c2, w2) - w2.beyond).toBeCloseTo(20 / (2 * n), 9);
      expect(spareOf(c2, w2) - w2.beyond).toBeCloseTo((spareOf(ch, w) - w.beyond) / 2, 9);
    });
  });

  it('the escorts carry the cycle\'s share at sea and the groups none: a hunting destroyer counts about 2.85 escorts a convoy, named', () => {
    const g = open(true);
    // (A thin screen, so the stations are not full: 10 destroyers bound to the route and a 10-destroyer group.)
    const { esc } = meet(g, 60, 10);
    sea(g, 'UK', 'natl', { dd: 10 }, 'hunt', undefined, [esc]);
    const w = work(g), n = w.convoys, ch = escortChance(g, 'UK', w), atSea = escortsAtSea()!, loose = FLEETS.escort.loose;
    // (19 of 33 days with a convoy, times 1 - 0.333 in service.)
    expect(atSea).toBeCloseTo(0.384, 3);
    expect(w.beyond).toBe(0);
    // Each escort bound to the route counts atSea / n; each of the group's ships 1 / n spare and loose x atSea / n on the screen.
    expect(w.on).toBeCloseTo(((10 + 10 * loose) * atSea) / n, 9);
    expect(spareOf(ch, w)).toBeCloseTo(10 / n, 9);
    expect((1 + loose * atSea) / atSea).toBeCloseTo(2.85, 2);
    // Where the stations are not full the convoys at sea cancel: the coordinated share is that ratio alone.
    expect(ch.coordinated).toBeCloseTo(10 / (10 + (10 + 10 * loose) * atSea), 12);
    timesTons(2, () => expect(escortChance(g, 'UK', work(g)).coordinated).toBeCloseTo(ch.coordinated, 12));
    // The overlap beside the group's own part: about a tenth now; about 3% on the Atlantic route with the groups whole.
    expect(loose * atSea).toBeCloseTo(0.096, 3);
    expect((loose * atSea) / n).toBeCloseTo(0.029, 3);
  });

  it('the one-place-at-a-time rule (a group never counts more than whole with one convoy) binds on no route the game ships: each has more than one convoy at sea at once, from its own figures', () => {
    const g = open(true);
    for (const route of NAVY.convoys) expect(work(g, route, route.to).convoys, route.id).toBeGreaterThan(1);
    // (The route's tons, its convoy's ships and their GRT alone: about 3.3 on the Atlantic, 10 on Libya, whatever the fleets.)
    expect(work(g, atlantic, 'UK').convoys).toBeCloseTo(3.32, 2);
    expect(work(g, libya, 'ITA').convoys).toBeCloseTo(10, 9);
    meet(g, 60, 300);
    expect(work(g, atlantic, 'UK').convoys).toBeCloseTo(3.32, 2);
  });

  it('with no group the split is unchanged, whatever the convoys at sea', () => {
    const g = open(true);
    meet(g, 60, 300);
    const w = work(g), ch = escortChance(g, 'UK', w);
    expect(ch.coordinated).toBeCloseTo(w.beyond / (w.beyond + w.on), 12);
    expect(ch.chance).toBeCloseTo((w.on * w.onRate + w.beyond * w.beyondRate) / (w.on + w.beyond), 12);
    expect(escortChance(g, 'UK', { ...w, convoys: 2 * w.convoys })).toEqual(ch);
    expect(escortChance(g, 'UK', { ...w, convoys: 0 })).toEqual(ch);
    // (A group in harbour is no group.)
    const { esc } = meet(g, 60, 300);
    const group = sea(g, 'UK', 'natl', { dd: 20 }, 'hunt', undefined, [esc]);
    group.stance = 'port';
    expect(escortChance(g, 'UK', work(g))).toEqual(ch);
  });

  it('with no convoy at sea the groups add nothing, and a group is never more than whole with one convoy', () => {
    const g = open(true);
    const { esc } = meet(g, 60, 300);
    sea(g, 'UK', 'natl', { dd: 20 }, 'hunt', undefined, [esc]);
    const w = work(g);
    // (No convoy sailed: no convoy to share the group over, and nothing to divide by.)
    timesTons(0, () => {
      const z = work(g);
      expect(z.convoys).toBe(0);
      expect(escortChance(g, 'UK', z)).toEqual({ coordinated: 0, chance: 0 });
    });
    const none = escortChance(g, 'UK', { ...w, convoys: 0 });
    expect(none.coordinated).toBeCloseTo(w.beyond / (w.beyond + w.on), 12);
    expect(none.chance).toBeCloseTo((w.on * w.onRate + w.beyond * w.beyondRate) / (w.on + w.beyond), 12);
    // (Fewer than one convoy at sea at once: the group is whole with the one, never priced more than once.)
    const half = escortChance(g, 'UK', { ...w, convoys: 0.5 });
    expect(half).toEqual(escortChance(g, 'UK', { ...w, convoys: 1 }));
    expect(spareOf(half, w) - w.beyond).toBeCloseTo(20, 9);
  });

  it('one rule for every navy: a German group on Italy\'s route and a British one on Britain\'s, each over its own route\'s convoys at sea', () => {
    const g = open(true);
    declareWar(g, 'ITA', 'UK');
    const ger = sea(g, 'GER', 'natl', { ss: 10 }, 'raid');
    const uk = sea(g, 'UK', 'wapp', { dd: 300 }, 'escort', 'atlantic');
    const brit = sea(g, 'UK', 'central', { ss: 10 }, 'raid', undefined, [uk]);
    const gesc = sea(g, 'GER', 'central', { dd: 300 }, 'escort', 'libya', [ger]);
    const ukg = sea(g, 'UK', 'natl', { dd: 20 }, 'hunt', undefined, [uk, brit]);
    const gerg = sea(g, 'GER', 'central', { dd: 20 }, 'hunt', undefined, [ger, gesc]);
    const seen: number[] = [];
    for (const [nat, route, grp] of [['UK', atlantic, ukg], ['ITA', libya, gerg]] as const) {
      const c = convoyCount(g, route)!, w = work(g, route, nat), ch = escortChance(g, nat, w);
      expect(w.convoys, nat).toBeCloseTo((c.sailed * CAUSE.station.dd!.crossing!) / 30, 12);
      expect(w.convoys, nat).toBeGreaterThan(1);
      const groups = 20 / w.convoys;
      expect(spareOf(ch, w) - w.beyond, nat).toBeCloseTo(groups, 9);
      expect(ch.chance, nat).toBeCloseTo((w.on * w.onRate + w.beyond * w.beyondRate + groups * fit(g, grp) * column(g, grp.nat, true)) / (w.on + w.beyond + groups), 12);
      seen.push(w.convoys);
    }
    // (Each route's own count: the Atlantic's few big convoys, Libya's many small ones.)
    expect(seen[1]).toBeGreaterThan(2 * seen[0]);
  });
});

/** The boats a raider would lose to the convoy's escort with this support group in harbour. */
function expectWithout(g: Game, group: Fleet, boats: Fleet): number {
  const was = group.stance;
  group.stance = 'port';
  try { return expectOf(g, boats); } finally { group.stance = was; }
}

describe('Q3 (a) and Q4 (a): what it leaves alone', () => {
  it('escort ships sunk keep the flat escortLoss', () => {
    const g = open(true);
    meet(g, 60, 300);
    const m = convoyMeeting(g, 'UK', atlantic), u = unread(() => convoyMeeting(g, 'UK', atlantic));
    expect(m.escorts).toEqual(u.escorts);
    expect(m.escorts[0].expect).toBeCloseTo(m.raid * NAVY.convoy.escortLoss, 12);
  });

  it('boats not raiding keep the hunt\'s curve exactly as before', () => {
    const g = open(true);
    const { esc } = meet(g, 60, 30);
    sea(g, 'UK', 'natl', { dd: 20 }, 'hunt', undefined, [esc]);
    // (A pack hunting, not raiding, in the route's sea: its boats meet no convoy, and the hunt's curve takes them.)
    const idle = sea(g, 'GER', 'natl', { ss: 20 }, 'hunt', undefined, [g.fleets.find((f) => f.name === 'U-boat Fleet')!]);
    const z = idle.zone;
    expect(huntExpected(g, 'UK', z, [idle])).toBe(unread(() => huntExpected(g, 'UK', z, [idle])));
    const h = huntersIn(g, 'UK', z);
    expect(huntExpected(g, 'UK', z, [idle])).toBeCloseTo(20 * Math.min(FLEETS.hunt.most, huntShareSunk(h.weight, 20) * (h.air! + h.kit!) / h.weight), 12);
  });

  it('nothing is counted twice: a support group works a raiding boat\'s incidents and never hunts it on the curve as well', () => {
    const g = open(true);
    const { boats, esc } = meet(g, 60, 30);
    sea(g, 'UK', 'natl', { dd: 40 }, 'hunt', undefined, [esc]);
    const z = boats.zone, h = huntersIn(g, 'UK', z);
    // (The hunt's ships take none of a raiding pack: only the aircraft's part of the curve, and the incidents.)
    const air = 60 * huntShareSunk(h.weight, 60) * (h.air! / h.weight);
    expect(h.kit!).toBeGreaterThan(0);
    expect(huntExpected(g, 'UK', z, [boats])).toBeCloseTo(air + expectOf(g, boats), 12);
    // (With no convoy found, the group's ships sink none of the raiding boats at all.)
    const keep = CAUSE.count.attacked;
    withRows(() => { CAUSE.count.attacked = 0; }, () => { CAUSE.count.attacked = keep; }, () => {
      expect(expectOf(g, boats)).toBe(0);
      expect(huntExpected(g, 'UK', z, [boats])).toBeCloseTo(air, 12);
    });
  });
});

describe('Q5: price once', () => {
  it('E is the escorts on the close screen\'s stations with the convoy, k 0.984: the contest at exact values', () => {
    const g = open(true);
    for (const dd of [30, 300]) {
      meet(g, 60, dd);
      bare(() => {
        const s = convoySideOf(g, 'UK', atlantic)!, w = work(g);
        expect(s.E).toBeCloseTo(w.on, 12);
        expect(s.sunk).toBeCloseTo((44 * s.P) / (s.P + 0.984 * s.E), 12);
        // (The same ships the eye term reads: with no radar-fitted escort, f = 1 / (1 + c_e E / tries). Built as ruled,
        // and noted: E on the stations prices again what the screen prices.)
        expect(s.fire).toBeCloseTo(1 / (1 + (eyePair()!.ce * s.E) / w.tries), 12);
      });
    }
    // Escorts past the stations add nothing to E: they hunt as spare ships instead.
    meet(g, 60, 300);
    const many = bare(() => convoySideOf(g, 'UK', atlantic)!);
    meet(g, 60, 600);
    const more = bare(() => convoySideOf(g, 'UK', atlantic)!);
    expect(more.E).toBe(many.E);
    expect(more.E).toBeCloseTo(screenStations(atlantic)!, 12);
    expect(more.sunk).toBe(many.sunk);
  });

  it('with k on the stations unread the rest of the form runs, and E and k stand as the tons ruling set them', () => {
    const g = open(true);
    meet(g, 60, 30);
    const keep = CAUSE.count.sinkOnStations;
    withRows(() => { CAUSE.count.sinkOnStations = null; }, () => { CAUSE.count.sinkOnStations = keep; }, () => bare(() => {
      const s = convoySideOf(g, 'UK', atlantic)!, m = convoyMeeting(g, 'UK', atlantic), c = convoyCount(g, atlantic)!;
      expect(s.E).toBeCloseTo(m.G / c.sailed, 12);
      expect(s.sunk).toBeCloseTo((44 * s.P) / (s.P + 0.137 * s.E), 12);
      expect(escortWork(g, 'UK', atlantic, c)).not.toBeNull();
    }));
  });
});

describe('the month and the record', () => {
  it('a month books the escort\'s kills by cause: the roll about the meeting\'s expectation, as the boats stand after the hunt', () => {
    const g = open(true);
    give(g, 'UK', ['sonar', 'hedgehog', 'escortgroups']);
    const { boats, esc } = meet(g, 400, 200);
    sea(g, 'UK', 'natl', { dd: 60 }, 'hunt', undefined, [esc]);
    // (The escorts' own losses held at none, so the escort the month reads is the one the card reads.)
    const keep = NAVY.convoy.escortLoss;
    withRows(() => { NAVY.convoy.escortLoss = 0; }, () => { NAVY.convoy.escortLoss = keep; }, () => {
      const sunk = huntSubs(g, 'UK');
      const x = expectOf(g, boats), w = work(g), old = unread(() => expectOf(g, boats));
      // (The build before the form would book about a quarter as many: the two runs never meet.)
      expect(run(old)[1]).toBeLessThan(run(x)[0]);
      const had = boats.ships.ss;
      convoyWar(g, 'UK', sunk);
      const m = g.convoys.atlantic;
      expect(run(x)).toContain(m.byEscorts);
      expect(had - boats.ships.ss).toBe(m.byEscorts);
      // The record carries the incidents a convoy attacked by cause, the frustrated and those found after firing.
      expect(m.count!.met).toBeCloseTo(w.met, 2);
      expect(m.count!.met).not.toBeCloseTo(w.frustrated, 1);
    });
  });

  it('a month played whole stays inside the hunt\'s cap, and the record and the card carry the incidents a convoy attacked', () => {
    const g = open(true);
    give(g, 'UK', ['sonar', 'hedgehog', 'escortgroups']);
    const { boats, esc } = meet(g, 400, 200);
    sea(g, 'UK', 'natl', { dd: 60 }, 'hunt', undefined, [esc]);
    const card = convoyMeeting(g, 'UK', atlantic).count!;
    expect(card.met).toBeCloseTo(work(g).met, 2);
    const had = boats.ships.ss;
    navalUpkeep(g, 'UK');
    const m = g.convoys.atlantic;
    expect(m.count!.met).toBeGreaterThan(0);
    expect(m.byEscorts).toBeGreaterThan(0);
    expect(boats.lost.ss).toBeLessThanOrEqual(Math.ceil(FLEETS.hunt.most * had) + 1);
    // (A save carries it, and plays on alike.)
    expect(loadGame(saveGame(g))!.convoys.atlantic.count!.met).toBe(m.count!.met);
  });
});

describe('with the switch null', () => {
  it('every number is the build\'s before it: the count\'s 0.21 a boat on station, the old split, E = G / convoys sailed, k 0.137', () => {
    const g = open(true);
    const { boats, esc } = meet(g, 60, 30);
    const group = sea(g, 'UK', 'natl', { dd: 20 }, 'hunt', undefined, [esc]);
    unread(() => {
      const c = convoyCount(g, atlantic)!;
      expect(escortWork(g, 'UK', atlantic, c)).toBeNull();
      // (As first built: T2's rate a boat on station, and the support group and the close escort by destroyers.)
      const chance = (30 * 0.05 * fit(g, esc) + 20 * 0.17 * fit(g, group)) / 50;
      expect(c.incidents).toBeCloseTo(c.boats * 0.21, 12);
      expect(expectOf(g, boats)).toBeCloseTo(60 * onStation(g, boats, boats.zone) * 0.21 * chance, 12);
      const m = convoyMeeting(g, 'UK', atlantic);
      expect(m.count!.met).toBeUndefined();
      bare(() => {
        const s = convoySideOf(g, 'UK', atlantic)!;
        expect(s.E).toBeCloseTo(m.G / c.sailed, 12);
        expect(s.sunk).toBeCloseTo((44 * s.P) / (s.P + 0.137 * s.E), 12);
      });
    });
    // Laid, the same table reads otherwise.
    expect(expectOf(g, boats)).not.toBeCloseTo(unread(() => expectOf(g, boats)), 6);
  });

  it('the convoy count is the form\'s to read, never to change, laid or null', () => {
    const g = open(true);
    meet(g, 60, 300);
    expect(convoyCount(g, atlantic)).toEqual(unread(() => convoyCount(g, atlantic)));
  });

  it('a month played null leaves no trace of the form in the record, and laid books otherwise', () => {
    const month = (laid: boolean): { rec: string; lost: number } => {
      const g = open(true);
      give(g, 'UK', ['sonar', 'hedgehog', 'escortgroups']);
      const { boats, esc } = meet(g, 400, 200);
      sea(g, 'UK', 'natl', { dd: 60 }, 'hunt', undefined, [esc]);
      if (laid) navalUpkeep(g, 'UK'); else unread(() => navalUpkeep(g, 'UK'));
      return { rec: JSON.stringify(g.convoys.atlantic), lost: boats.lost.ss };
    };
    const a = month(false), c = month(true);
    expect(JSON.parse(a.rec).count.met).toBeUndefined();
    expect(JSON.parse(c.rec).count.met).toBeGreaterThan(0);
    // The escort's kills and the raider's losses are the form's where it is laid: several times the build's before it.
    const ka = JSON.parse(a.rec).byEscorts, kc = JSON.parse(c.rec).byEscorts;
    expect(kc).toBeGreaterThan(2 * ka);
    expect(c.lost - a.lost).toBe(kc - ka);
  });
});

describe('the research cards', () => {
  it('say who works each column as the switch lays it: the escorts beyond the close screen with the hunting groups, those on it alone', () => {
    const g = open(true);
    const card = (id: string): string => techRule(techById(id)!, g, 'UK');
    const laid = { groups: card('escortgroups'), hedgehog: card('hedgehog') };
    const was = unread(() => ({ groups: card('escortgroups'), hedgehog: card('hedgehog') }));
    expect(laid.groups).toContain("a hunting group and a convoy's escorts beyond its close screen sink the submarine in 55 of every 100 attacks they answer, and the escorts on the close screen in 12, against 17 and 5 with depth charges alone");
    expect(laid.hedgehog).toContain("The escorts on a convoy's close screen sink the submarine in 10 of every 100 attacks they answer, and a hunting group and the convoy's escorts beyond the screen in 32, against 5 and 17 with depth charges alone.");
    expect(laid.groups).not.toContain("a convoy's own escort");
    expect(laid.hedgehog).not.toContain("a convoy's own escort");
    // (Null, the cards read as they did.)
    expect(was.groups).toContain("a hunting group sinks the submarine in 55 of every 100 attacks it answers and a convoy's own escort in 12, against 17 and 5 with depth charges alone");
    expect(was.hedgehog).toContain("A convoy's own escort sinks the submarine in 10 of every 100 attacks it answers, and a hunting group in 32, against 5 and 17 with depth charges alone.");
    expect(was.groups).not.toContain('beyond');
    expect(was.hedgehog).not.toContain('beyond');
  });
});

describe('off', () => {
  it('nothing is read', () => {
    const g = open(false);
    const { esc } = meet(g, 60, 300);
    sea(g, 'UK', 'natl', { dd: 20 }, 'hunt', undefined, [esc]);
    expect(convoyCount(g, atlantic)).toBeNull();
    expect(escortWork(g, 'UK', atlantic, convoyCount(g, atlantic))).toBeNull();
    expect(convoySideOf(g, 'UK', atlantic)).toBeUndefined();
    expect(convoyMeeting(g, 'UK', atlantic)).toEqual(unread(() => convoyMeeting(g, 'UK', atlantic)));
    const t = loadGame(saveGame(g))!;
    navalUpkeep(g, 'UK');
    unread(() => navalUpkeep(t, 'UK'));
    expect(saveGame(g)).toBe(saveGame(t));
  });
});
