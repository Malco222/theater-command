// The convoy war by cause (release 2, a step of Units decide; audit #5; the design director's ruling of 23 November
// 2026 07:26Z on naval-battles/convoy-r2/form.md, under Brian's core theme of 07:11Z and 07:13Z). What a convoy loses
// and what its escorts sink come from the kit, the aircraft and the work of the ships on the table, the same for every
// navy; OEG Report 51 only sizes each (data/campaign/convoycause.json). Off, nothing of it is read.
import { afterEach, describe, expect, it } from 'vitest';
import stage from '../data/campaign/stage.json';
import { createGame, declareWar } from '../src/campaign/game';
import { CAUSE, FLEETS, NAVY, aswKit, convoyCount, convoyField, fit, convoyMeeting, convoyWar, huntExpected, huntShareSunk, huntSubs, huntersIn, incidentRate, laneLoss, navalUpkeep, seaCover, seaLossMul, subScene, zoneOf } from '../src/campaign/navy';
import { carrierRules } from '../src/campaign/air';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { UNITS_STAGE } from '../src/campaign/stage';
import { grantTech, techRule, techById, techView, whyNotResearch } from '../src/campaign/tech';
import { traitSeaLoss } from '../src/campaign/traits';
import { setUnitsDecide, unitsDecide } from '../src/campaign/units';
import type { Fleet, Game, Ships } from '../src/campaign/types';

const UNITS_ROWS = ['asvradar', 'patrolaircraft', 'hedgehog'];
const KIT = ['sonar', 'asvradar', 'patrolaircraft', 'hedgehog', 'escortgroups'];
const atlantic = NAVY.convoys.find((r) => r.id === 'atlantic')!;
const libya = NAVY.convoys.find((r) => r.id === 'libya')!;

/** A grand campaign at its start, with Units decide on or off, and every fleet in harbour (a test puts to sea what it needs). */
function open(on: boolean, seed = 1): Game {
  const g = startOpen(createGame(seed, null));
  if (on) g.flags.unitsDecide = 1; else delete g.flags.unitsDecide;
  delete g.flags.weather;
  for (const f of g.fleets) f.stance = 'port';
  return g;
}
const none = (): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 });
/** One of this nation's fleets, put to sea in a sea with only these ships, fully ready. */
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
const twin = (g: Game): Game => loadGame(saveGame(g))!;
const run = (x: number): [number, number] => [Math.floor(x), Math.floor(x) + 1];
/** With the escorts' losses form unread (convoycause.json 'escorts' null): the count's incidents at T2's rate and the split as first built. The form is tested in convoy-escorts-r2. */
function asFirst<T>(f: () => T): T {
  const keep = CAUSE.escorts;
  CAUSE.escorts = null;
  try { return f(); } finally { CAUSE.escorts = keep; }
}

afterEach(() => { convoyField.fought = undefined; });

describe('the switch', () => {
  it('is off as shipped, and never reaches a history campaign', () => {
    expect((stage as { unitsDecide?: boolean }).unitsDecide).toBe(false);
    if (!UNITS_STAGE) expect(startOpen(createGame(1, null)).flags.unitsDecide).toBeUndefined();
    const h = createGame(1, null);
    h.flags.unitsDecide = 1;
    expect(unitsDecide(h)).toBe(false);
    const g = open(true);
    expect(unitsDecide(g)).toBe(true);
    expect(unitsDecide(twin(g))).toBe(true);
  });

  it('is set at setup only', () => {
    const g = startOpen(createGame(1, null));
    if (g.turn === 1) {
      expect(setUnitsDecide(g, true)).toBe(true);
      expect(unitsDecide(g)).toBe(true);
    }
    g.turn += 1;
    expect(setUnitsDecide(g, false)).toBe(false);
  });

  it('off, its research rows are not in the tree: nobody sees, orders, is given or picks them', () => {
    const g = open(false);
    const ids = techView(g, 'UK')!.nodes.map((n) => n.id);
    for (const id of UNITS_ROWS) {
      expect(ids).not.toContain(id);
      expect(whyNotResearch(g, 'UK', id)).toBe('There is no such technology.');
      expect(grantTech(g, 'UK', id)).toBe(false);
    }
    const on = open(true);
    const seen = techView(on, 'UK')!.nodes.map((n) => n.id);
    for (const id of UNITS_ROWS) expect(seen).toContain(id);
  });

  it('off, a save that holds its rows plays the month as one that does not, and writes none of its records', () => {
    // (Release 2 off is the same to the bit as before it: the whole save after 12 to 60 months of two AI wars matches
    // the base commit's, checked outside the suite (NOTES). Here: rows researched with the switch on read as nothing off.)
    const a = open(false);
    give(a, 'UK', ['sonar', 'escortgroups']);
    const b = twin(a);
    a.nations.UK.tech!.done.push(...UNITS_ROWS);
    for (const g of [a, b]) {
      sea(g, 'GER', 'natl', { ss: 12 }, 'raid');
      sea(g, 'UK', 'wapp', { dd: 20 }, 'escort', 'atlantic');
    }
    subScene.on = true;
    try {
      navalUpkeep(a, 'UK');
      navalUpkeep(b, 'UK');
    } finally { subScene.on = false; }
    expect(JSON.stringify([a.convoys, a.fleets, a.subLook])).toBe(JSON.stringify([b.convoys, b.fleets, b.subLook]));
    expect(a.convoys.atlantic.byEscorts).toBeUndefined();
    for (const l of a.subLook ?? []) for (const c of l.convoys) for (const s of c.seas) expect(Object.keys(s).sort()).toEqual(['air', 'escorts', 'raiders', 'tons', 'zone']);
    expect(seaLossMul(a, 'UK')).toBe(traitSeaLoss(a, 'UK'));
  });
});

describe('air cover (OEG T7)', () => {
  it('only aircraft with anti-submarine kit cover a sea: depth charges and sea-search radar', () => {
    const g = open(true);
    const z = zoneOf(g, 'irish');
    expect(seaCover(g, 'UK', z).kind).toBe('none');
    give(g, 'UK', ['sonar']);
    expect(seaCover(g, 'UK', z).share).toBe(0);
    give(g, 'UK', ['asvradar']);
    const c = seaCover(g, 'UK', z);
    expect(c.kind).toBe('shore');
    expect(c.share).toBeCloseTo(Math.min(1, g.seas[z].air), 9);
  });

  it('cuts what the raiders in a covered sea sink to 0.34 of their rate, and nothing where none covers', () => {
    const bare = open(true);
    sea(bare, 'GER', 'irish', { ss: 10 }, 'raid');
    const covered = twin(bare);
    give(covered, 'UK', ['sonar', 'asvradar']);
    const share = seaCover(covered, 'UK', zoneOf(covered, 'irish')).share;
    expect(share).toBeGreaterThan(0.99);
    const r0 = convoyMeeting(bare, 'UK', atlantic).raid, r1 = convoyMeeting(covered, 'UK', atlantic).raid;
    expect(r0).toBeGreaterThan(0);
    expect(r1 / r0).toBeCloseTo(CAUSE.cover, 9);
    expect(CAUSE.cover).toBe(0.34);
  });

  it('reaches the open ocean only with long-range patrol aircraft, and all of a coast sea with them', () => {
    const g = open(true);
    give(g, 'UK', ['sonar', 'asvradar']);
    const mid = zoneOf(g, 'midnatl'), natl = zoneOf(g, 'natl');
    expect(g.seas[mid].coast.length).toBe(0);
    expect(seaCover(g, 'UK', mid).kind).toBe('none');
    expect(seaCover(g, 'UK', natl)).toEqual({ kind: 'shore', share: g.seas[natl].air });
    give(g, 'UK', ['patrolaircraft']);
    expect(seaCover(g, 'UK', mid)).toEqual({ kind: 'long-range', share: 1 });
    expect(seaCover(g, 'UK', natl)).toEqual({ kind: 'long-range', share: 1 });
  });

  it('a carrier bound to the convoys covers her own sea, with kit', () => {
    const g = open(true);
    give(g, 'UK', ['sonar', 'asvradar']);
    const natl = zoneOf(g, 'natl');
    sea(g, 'UK', 'natl', { cv: 1, dd: 4 }, 'escort', 'atlantic');
    const c = seaCover(g, 'UK', natl, 'atlantic');
    expect(c.kind).toBe('carrier');
    expect(c.share).toBeGreaterThan(g.seas[natl].air);
    // (Bound to no lane, or to another, she is not this convoy's cover: the coast's aircraft are.)
    expect(seaCover(g, 'UK', natl).kind).toBe('shore');
    expect(seaCover(g, 'UK', natl, 'libya').kind).toBe('shore');
  });

  it('takes the flat air guard out of the escorts\' worth: the aircraft are the cover', () => {
    const g = open(true), off = open(false);
    for (const x of [g, off]) { sea(x, 'GER', 'irish', { ss: 10 }, 'raid'); sea(x, 'UK', 'wapp', { dd: 10 }, 'escort', 'atlantic'); }
    // (Release 2 off: coastal air adds to the guard; on, the guard is the ships alone.)
    expect(convoyMeeting(off, 'UK', atlantic).guard).toBeGreaterThan(convoyMeeting(g, 'UK', atlantic).guard);
  });
});

describe('boats lose where they fight (OEG T2 and T10)', () => {
  /** The boats a raider of 10 is expected to lose to a convoy's escort this month, as the meeting gives it. */
  const expectLoss = (g: Game, route = atlantic, nat = 'UK'): number => convoyMeeting(g, nat, route).raiders[0].expect;
  /** The month's incidents on a route (the one-pass ruling's count): all of them the lone raider's. */
  const incidents = (g: Game, route = atlantic): number => convoyCount(g, route)!.incidents;

  it('a close escort sinks 5 in 100 boats that attack, 9.5 with ahead-throwing weapons, 12 with escort-group training too (the count\'s incidents)', () => asFirst(() => {
    const g = open(true);
    sea(g, 'GER', 'natl', { ss: 10 }, 'raid');
    const esc = sea(g, 'UK', 'wapp', { dd: 20 }, 'escort', 'atlantic');
    expect(incidents(g)).toBeGreaterThan(0);
    expect(expectLoss(g)).toBeCloseTo(incidents(g) * 0.05 * fit(g, esc), 9);
    const kit = twin(g);
    give(kit, 'UK', ['sonar', 'hedgehog']);
    // Ahead-throwing weapons alone: OEG T9's first half-year edge over depth charges (7.5% against 4.0%).
    expect(CAUSE.incident.atw).toBe(1.9);
    expect(expectLoss(kit) / expectLoss(g)).toBeCloseTo(1.9, 9);
    give(kit, 'UK', ['escortgroups']);
    expect(expectLoss(kit) / expectLoss(g)).toBeCloseTo(0.12 / 0.05, 9);
  }));

  it('a support group in the sea sinks 17 in 100, 55 with the kit; the close escort keeps its own rate beside it (the split as first built)', () => asFirst(() => {
    const g = open(true);
    const boats = sea(g, 'GER', 'natl', { ss: 10 }, 'raid');
    const esc = sea(g, 'UK', 'wapp', { dd: 10 }, 'escort', 'atlantic');
    sea(g, 'UK', 'natl', { dd: 10 }, 'hunt', undefined, [esc]);
    const f = fit(g, esc);
    expect(expectLoss(g)).toBeCloseTo(incidents(g) * f * (0.17 + 0.05) / 2, 9);
    give(g, 'UK', ['sonar', 'hedgehog', 'escortgroups']);
    expect(expectLoss(g)).toBeCloseTo(incidents(g) * f * (0.55 + 0.12) / 2, 9);
    expect(boats.ships.ss).toBe(10);
  }));

  it('no destroyers, no incidents: an unescorted convoy sinks no boat', () => {
    const g = open(true);
    sea(g, 'GER', 'natl', { ss: 10 }, 'raid');
    expect(expectLoss(g)).toBe(0);
  });

  it('a German and a British navy with the same kit hunt alike (the count\'s incidents; by cause, convoy-escorts-r2)', () => asFirst(() => {
    // The Atlantic convoys to Britain, raided by German boats, guarded by British destroyers; and the Libyan convoys
    // to Italy, raided by British boats, guarded by German destroyers. The same ships, readiness and kit each way.
    for (const kit of [[], ['sonar'], ['sonar', 'hedgehog', 'escortgroups'], KIT]) {
      const g = open(true);
      declareWar(g, 'ITA', 'UK');
      for (const nat of ['UK', 'GER']) g.nations[nat].tech!.done = g.nations[nat].tech!.done.filter((id) => !KIT.includes(id));
      give(g, 'UK', kit); give(g, 'GER', kit);
      expect(aswKit(g, 'GER')).toEqual(aswKit(g, 'UK'));
      const ger = sea(g, 'GER', 'natl', { ss: 10 }, 'raid');
      const uk = sea(g, 'UK', 'wapp', { dd: 20 }, 'escort', 'atlantic');
      sea(g, 'UK', 'central', { ss: 10 }, 'raid', undefined, [uk]);
      const gesc = sea(g, 'GER', 'central', { dd: 20 }, 'escort', 'libya', [ger]);
      const a = convoyMeeting(g, 'UK', atlantic).raiders.find((r) => r.nat === 'GER')!;
      const b = convoyMeeting(g, 'ITA', libya).raiders.find((r) => r.nat === 'UK')!;
      // (Readiness and fuel are causes too, and each navy's own, and each route has its own convoys and its boats their
      // own passage: the test reads them out, and the escorts' answer to each incident is the same.)
      expect(b.expect / fit(g, gesc) / incidents(g, libya), kit.join(',')).toBeCloseTo(a.expect / fit(g, uk) / incidents(g, atlantic), 9);
      expect(a.expect).toBeGreaterThan(0);
      // The hunt's edge from kit is the same per weight too (the weight itself still carries the navy's quality: step 1's).
      for (const s of [gesc, uk]) s.stance = 'hunt';
      const hu = huntersIn(g, 'UK', zoneOf(g, 'wapp')), hg = huntersIn(g, 'GER', zoneOf(g, 'central'));
      expect(hu.weight - hu.air!).toBeGreaterThan(0);
      expect(hg.kit! / (hg.weight - hg.air!)).toBeCloseTo(hu.kit! / (hu.weight - hu.air!), 9);
      expect(incidentRate(aswKit(g, 'GER'), true)).toBe(incidentRate(aswKit(g, 'UK'), true));
    }
  }));

  it('a month books them: boats lost on the convoy, inside the hunt\'s cap, on their own key', () => {
    const g = open(true);
    give(g, 'UK', ['sonar', 'hedgehog', 'escortgroups']);
    const boats = sea(g, 'GER', 'natl', { ss: 40 }, 'raid');
    const esc = sea(g, 'UK', 'wapp', { dd: 30 }, 'escort', 'atlantic');
    sea(g, 'UK', 'natl', { dd: 30 }, 'hunt', undefined, [esc]);
    const before = boats.lost.ss;
    navalUpkeep(g, 'UK');
    const m = g.convoys.atlantic;
    expect(m.byEscorts).toBeGreaterThan(0);
    expect(m.subsLost).toBeGreaterThanOrEqual(m.byEscorts!);
    expect(boats.lost.ss - before).toBeLessThanOrEqual(Math.ceil(40 * FLEETS.hunt.most) + 1);
    // (The same month twice gives the same month.)
    const h = open(true);
    give(h, 'UK', ['sonar', 'hedgehog', 'escortgroups']);
    const b2 = sea(h, 'GER', 'natl', { ss: 40 }, 'raid');
    const e2 = sea(h, 'UK', 'wapp', { dd: 30 }, 'escort', 'atlantic');
    sea(h, 'UK', 'natl', { dd: 30 }, 'hunt', undefined, [e2]);
    navalUpkeep(h, 'UK');
    expect(b2.lost.ss).toBe(boats.lost.ss);
    expect(saveGame(h)).toBe(saveGame(g));
  });

  it('the hunt leaves a raiding boat to the convoy\'s escort, and still hunts what is not raiding', () => {
    const g = open(true);
    const raider = sea(g, 'GER', 'natl', { ss: 40 }, 'raid');
    const esc = sea(g, 'UK', 'wapp', { dd: 1 }, 'escort', 'atlantic');
    sea(g, 'UK', 'natl', { dd: 60 }, 'hunt', undefined, [esc]);
    const offG = open(false);
    sea(offG, 'GER', 'natl', { ss: 40 }, 'raid');
    const e3 = sea(offG, 'UK', 'wapp', { dd: 1 }, 'escort', 'atlantic');
    sea(offG, 'UK', 'natl', { dd: 60 }, 'hunt', undefined, [e3]);
    const on = huntSubs(g, 'UK'), off = huntSubs(offG, 'UK');
    // (Off, the hunt sinks raiders on its weight; on, the raiding boats meet the escorts in convoyWar instead.)
    expect(off[zoneOf(g, 'natl')] ?? 0).toBeGreaterThan(on[zoneOf(g, 'natl')] ?? 0);
    expect(raider.ships.ss).toBeGreaterThan(0);
    const patrol = open(true);
    const p = sea(patrol, 'GER', 'natl', { ss: 40 }, 'patrol');
    sea(patrol, 'UK', 'natl', { dd: 60 }, 'hunt');
    huntSubs(patrol, 'UK');
    expect(p.lost.ss).toBeGreaterThan(0);
  });
});

describe('no nation and no month', () => {
  it('Britain\'s signals trait is not read; the convoy system that comes with sonar is', () => {
    const g = open(true);
    give(g, 'UK', ['sonar', 'escortgroups']);
    expect(traitSeaLoss(g, 'UK')).toBeLessThan(techById('sonar')!.seaLoss! - 1e-9);
    expect(seaLossMul(g, 'UK')).toBeCloseTo(techById('sonar')!.seaLoss!, 9);
    const ger = open(true);
    give(ger, 'GER', ['sonar', 'escortgroups']);
    expect(seaLossMul(ger, 'GER')).toBe(seaLossMul(g, 'UK'));
  });

  it('the rows say what they do under the switch, and escort groups no longer cut losses flat', () => {
    const g = open(true), off = open(false);
    const eg = techById('escortgroups')!;
    expect(techRule(eg, off, 'UK')).toMatch(/Convoy losses down/);
    expect(techRule(eg, g, 'UK')).not.toMatch(/Convoy losses down/);
    expect(techRule(eg, g, 'UK')).toContain('55 of every 100');
    for (const id of UNITS_ROWS) expect(techRule(techById(id)!, g, 'UK').length).toBeGreaterThan(20);
  });

  it('nothing in the rule reads the date: the same table in 1939 and 1944 gives the same month', () => {
    const a = open(true);
    give(a, 'UK', KIT);
    sea(a, 'GER', 'natl', { ss: 30 }, 'raid');
    const e = sea(a, 'UK', 'wapp', { dd: 30 }, 'escort', 'atlantic');
    sea(a, 'UK', 'natl', { dd: 20 }, 'hunt', undefined, [e]);
    const b = twin(a);
    b.turn += 60;
    const ma = convoyMeeting(a, 'UK', atlantic), mb = convoyMeeting(b, 'UK', atlantic);
    expect(mb.L).toBeCloseTo(ma.L, 9);
    expect(mb.raiders.map((r) => r.expect)).toEqual(ma.raiders.map((r) => r.expect));
    expect(laneLoss(b, 'UK', atlantic.zones.map((k) => zoneOf(b, k)), 'atlantic').loss).toBeCloseTo(laneLoss(a, 'UK', atlantic.zones.map((k) => zoneOf(a, k)), 'atlantic').loss, 9);
  });

  it('the scene\'s record carries the kit, the cover and the boats lost, with the switch only', () => {
    const g = open(true);
    give(g, 'UK', ['sonar', 'asvradar', 'hedgehog']);
    sea(g, 'GER', 'irish', { ss: 20 }, 'raid');
    sea(g, 'UK', 'wapp', { dd: 20 }, 'escort', 'atlantic');
    subScene.on = true;
    try { navalUpkeep(g, 'UK'); } finally { subScene.on = false; }
    const look = g.subLook!.find((l) => l.nat === 'UK')!.convoys.find((c) => c.route === 'atlantic')!;
    const irish = look.seas.find((s) => s.zone === zoneOf(g, 'irish'))!;
    expect(irish.kit).toEqual({ asdic: true, radar: true, hedgehog: true, hfdf: false, groups: false });
    expect(irish.cover).toBe('shore');
    expect(typeof irish.lost).toBe('number');
    expect(look.seas.reduce((s, x) => s + (x.lost ?? 0), 0)).toBe(g.convoys.atlantic.byEscorts);
  });
});

describe('found in review (23 Nov)', () => {
  it('a month saved part-way plays on as the live one: the hunt cap\'s book is in the save', () => {
    const g = open(true);
    give(g, 'UK', ['sonar', 'hedgehog', 'escortgroups']);
    const boats = sea(g, 'GER', 'natl', { ss: 40 }, 'hunt');
    sea(g, 'UK', 'natl', { dd: 80 }, 'hunt');
    sea(g, 'FRA', 'natl', { dd: 80 }, 'hunt');
    navalUpkeep(g, 'UK');
    expect(boats.capBook?.turn).toBe(g.turn);
    const t = twin(g);
    navalUpkeep(g, 'FRA');
    navalUpkeep(t, 'FRA');
    expect(saveGame(t)).toBe(saveGame(g));
    // (Inside the cap for the whole month, both nations' hunts together.)
    expect(boats.lost.ss).toBeLessThanOrEqual(Math.ceil(40 * FLEETS.hunt.most) + 1);
    // Off, nothing is written to a fleet.
    const off = open(false);
    const b2 = sea(off, 'GER', 'natl', { ss: 40 }, 'hunt');
    sea(off, 'UK', 'natl', { dd: 80 }, 'hunt');
    navalUpkeep(off, 'UK');
    expect(b2.capBook).toBeUndefined();
  });

  it('a fought night books against the expectation its card showed, even where the hunt strikes first', () => {
    const g = open(true);
    g.flags.convoyBattle = 1;
    give(g, 'UK', ['sonar', 'asvradar', 'hedgehog', 'escortgroups']);
    // (Boats on patrol raid with half their time: the hunt's ships take the other half first, before the convoys.)
    const boats = sea(g, 'GER', 'irish', { ss: 60 }, 'patrol');
    const esc = sea(g, 'UK', 'wapp', { dd: 30 }, 'escort', 'atlantic');
    sea(g, 'UK', 'irish', { dd: 80 }, 'hunt', undefined, [esc]);
    let shown = -1;
    convoyField.fought = (m) => { const r = m.raiders.find((x) => x.id === boats.id)!; shown = r.expect; return { swing: 1, ss: { [r.id]: run(r.expect)[1] }, dd: {} }; };
    navalUpkeep(g, 'UK');
    expect(shown).toBeGreaterThan(0);
    // (The hunt took boats first, so this month's own expectation is lower than the card's.)
    expect(boats.lost.ss - (g.convoys.atlantic.byEscorts ?? 0)).toBeGreaterThan(0);
    expect(g.convoys.atlantic.fought).toBe('field');
    expect(g.convoys.atlantic.byEscorts).toBe(run(shown)[1]);
  });

  it('a carrier with no aircraft aboard covers nothing', () => {
    const g = open(true);
    give(g, 'UK', ['sonar', 'asvradar']);
    const cv = sea(g, 'UK', 'natl', { cv: 1, dd: 4 }, 'escort', 'atlantic');
    // (Her wing ordered ashore: decks whose wing has gone count as the empty decks they are, carriers.ts wingShare.)
    g.airFleets = g.airFleets.filter((w) => w.ship !== cv.id);
    cv.bare = { ftr: 12, bmr: 12 };
    const c = seaCover(g, 'UK', zoneOf(g, 'natl'), 'atlantic');
    // (Without the carrier rules every deck counts as full, as it does everywhere else.)
    expect(c.kind).toBe(carrierRules(g) ? 'shore' : 'carrier');
  });

  it('the research cards say what radar does with the switch: fighters only, not the convoys\' escorts', () => {
    const eg = techById('earlywarning')!;
    expect(techRule(eg, open(false), 'UK')).toMatch(/convoy/);
    expect(techRule(eg, open(true), 'UK')).not.toMatch(/convoy/);
    expect(techRule(eg, open(true), 'UK')).toMatch(/Fighters over their own sky/);
  });

  it('the fleet orders\' hunt look-ahead reads the hunt as it is played, and is release 2\'s off', () => {
    const off = open(false);
    const q = sea(off, 'GER', 'natl', { ss: 20 }, 'raid');
    sea(off, 'UK', 'natl', { dd: 40 }, 'hunt');
    const z = zoneOf(off, 'natl');
    expect(huntExpected(off, 'UK', z, [q])).toBeCloseTo(20 * huntShareSunk(huntersIn(off, 'UK', z).weight, 20), 12);
    // On, a raiding boat meets the convoy's escort, not the hunt's ships: the look-ahead counts that, and no more.
    const g = open(true);
    const r = sea(g, 'GER', 'natl', { ss: 20 }, 'raid');
    const e = sea(g, 'UK', 'wapp', { dd: 30 }, 'escort', 'atlantic');
    sea(g, 'UK', 'natl', { dd: 40 }, 'hunt', undefined, [e]);
    const h = huntersIn(g, 'UK', z);
    const air = 20 * huntShareSunk(h.weight, 20) * (h.air! / h.weight);
    expect(huntExpected(g, 'UK', z, [r])).toBeCloseTo(air + convoyMeeting(g, 'UK', atlantic).raiders.find((x) => x.id === r.id)!.expect, 9);
  });

  it('the incidents roll on their own key: the escorts\' losses and the hunt are the same with none found', () => {
    // (None found: no convoy attacked at any number of boats, the count's share 0.)
    const month = (attacked: number): Game => {
      const keep = CAUSE.count.attacked;
      CAUSE.count.attacked = attacked;
      try {
        const g = open(true);
        give(g, 'UK', ['sonar', 'hedgehog', 'escortgroups']);
        // (Enough boats that the escorts surely lose a ship: only a quarter or so of them are on station.)
        sea(g, 'GER', 'natl', { ss: 900 }, 'raid');
        const e = sea(g, 'UK', 'wapp', { dd: 200 }, 'escort', 'atlantic');
        sea(g, 'UK', 'natl', { dd: 60 }, 'hunt', undefined, [e]);
        navalUpkeep(g, 'UK');
        return g;
      } finally { CAUSE.count.attacked = keep; }
    };
    const a = month(CAUSE.count.attacked), b = month(0);
    expect(a.convoys.atlantic.byEscorts).toBeGreaterThan(0);
    expect(b.convoys.atlantic.byEscorts).toBe(0);
    expect(a.convoys.atlantic.escortsLost).toBeGreaterThan(0);
    expect(a.convoys.atlantic.escortsLost).toBe(b.convoys.atlantic.escortsLost);
    expect(a.convoys.atlantic.subsLost - a.convoys.atlantic.byEscorts!).toBe(b.convoys.atlantic.subsLost);
  });
});

describe('the convoy war as a whole', () => {
  it('convoyWar in a history campaign never reads it', () => {
    const h = createGame(1, null);
    h.flags.unitsDecide = 1;
    const t = twin(h);
    delete t.flags.unitsDecide;
    convoyWar(h, 'UK');
    convoyWar(t, 'UK');
    expect(JSON.stringify(h.convoys)).toBe(JSON.stringify(t.convoys));
  });
});
