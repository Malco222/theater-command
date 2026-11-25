// Pilots under Units decide (release 2; the design director's ruling of 24 Nov 2026 15:53Z on the rules builder's #2
// form, rules-builder/release2/soviet/form.md, Part 2, the air thread's points in section 6). The nation holds what its
// schools know (air.school, the deck school apart, the reserve's mean hours and experience); a group holds what its own
// pilots have lived through (hours and fxp). A new pilot leaves school with its hours and no experience, one who comes
// down and flies again carries his group's, and a refit blends them by count. Every group seasons in a fight survived,
// leader or not, and the card and the field read its experience as a leader's level, so it is priced once. Off, none of
// it is read or written.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { AIR, AIRWAR, airPool, assessAir, heldOf, deckBred, fxpOf, hoursOf, joinPilots, levelOf, openPilots, poolIn, splitFleet } from '../src/campaign/air';
import { UNITS, aceSchool, armsOf, schoolPilots, wayNow } from '../src/campaign/units';
import { FIGHTERS, doctrineOf, pilotsEdge } from '../src/campaign/fighters';
import { headStart } from '../src/campaign/bombers';
import { airTicket } from '../src/battle/air/ticket';
import { planRaid, raidTargets } from '../src/campaign/airwar';
import { loadGame, saveGame } from '../src/campaign/save';
import { runTurnAI } from '../src/campaign/turn';
import type { AirFleet, Game } from '../src/campaign/types';

/** A grand campaign at its first day, opened with Units decide or without it. */
function open(on: boolean, seed = 1): Game {
  const g = createGame(seed, null);
  if (on) g.flags.unitsDecide = 1;
  return startOpen(g);
}
const land = (g: Game, nat: string): AirFleet[] => g.airFleets.filter((f) => f.nat === nat && !deckBred(f));
/** A copy of a group, outside the game, for the blends. */
const copy = (f: AirFleet): AirFleet => JSON.parse(JSON.stringify(f));

describe('off: no pilots in the store', () => {
  const g = open(false);
  it('writes no hours or experience and reads none', () => {
    for (const f of g.airFleets) {
      expect('hours' in f || 'fxp' in f, f.name).toBe(false);
      expect(hoursOf(g, f)).toBeNull();
      expect(fxpOf(g, f)).toBe(0);
    }
    const f = land(g, 'GER')[0], was = airPool(g, 'GER').pilots;
    poolIn(g, 'GER', 10, f);
    joinPilots(g, f, 10, 'ftr', 'pool');
    openPilots(g);
    expect(airPool(g, 'GER').pilots).toBe(was + 10);
    expect('hours' in f || 'fxp' in f).toBe(false);
  });
});

describe('on: every group\'s pilots, seeded at the start', () => {
  const g = open(true);

  it('land groups at their school\'s hours where it is read, none where it is not; experience from the leader\'s level, so the start is today\'s', () => {
    expect(armsOf(g, 'GER')!.air.school).toBe(250);
    for (const f of land(g, 'GER')) expect(f.hours, f.name).toBe(250);
    for (const nat of ['UK', 'FRA', 'POL']) for (const f of land(g, nat)) { expect('hours' in f, f.name).toBe(false); expect(hoursOf(g, f)).toBeNull(); }
    for (const f of g.airFleets) {
      expect(f.fxp, f.name).toBeGreaterThanOrEqual(0);
      // A group's level is its leader's today (1 with none), so leaderMul and the field's experience start as they were.
      expect(levelOf(fxpOf(g, f)), f.name).toBeCloseTo(f.leader ? f.leader.level : 1, 9);
    }
  });

  it('a carrier wing takes the deck school, kept apart from the land school', () => {
    // No navy that sails at the 1939 start has a deck school read (JAP's 500 is the only one), so lay one by hand.
    const h = open(true), A = armsOf(h, 'UK')!.air;
    expect(A.deck).toBeNull();
    for (const f of h.airFleets.filter((x) => x.nat === 'UK')) expect('hours' in f, f.name).toBe(false);
    A.deck = 500;
    openPilots(h);
    const wings = h.airFleets.filter((f) => f.nat === 'UK' && deckBred(f));
    expect(wings.length).toBeGreaterThan(0);
    for (const f of wings) expect(f.hours, f.name).toBe(500);
    expect(A.school).toBeNull();
    for (const f of land(h, 'UK')) expect('hours' in f, f.name).toBe(false);
  });

  it('saves and loads with the group figures', () => {
    const h = loadGame(saveGame(g))!;
    const f = land(h, 'GER')[0];
    expect(f.hours).toBe(250);
    expect(f.fxp).toBe(land(g, 'GER')[0].fxp);
  });
});

describe('on: the reserve and the refit', () => {
  it('new pilots come at the school\'s hours with no experience; a pilot back carries his group\'s; a refit blends by count', () => {
    const g = open(true), A = armsOf(g, 'GER')!.air, p = airPool(g, 'GER');
    p.pilots = 100; A.pool = 250; A.poolXp = 0;
    // A pilot back from a seasoned group of 400 hours: the reserve's means move by his share.
    const f = land(g, 'GER')[0];
    f.hours = 400; f.fxp = 0.5;
    poolIn(g, 'GER', 100, f);
    expect(p.pilots).toBe(200);
    expect(A.pool).toBeCloseTo(325, 9);
    expect(A.poolXp).toBeCloseTo(0.25, 9);
    // New pilots from school: the school's hours, no experience.
    schoolPilots(g, 'GER', 200);
    expect(A.pool).toBeCloseTo((325 * 200 + 250 * 200) / 400, 9);
    expect(A.poolXp).toBeCloseTo(0.125, 9);
    // A refit from the reserve: the group's figures become the means by count.
    const r = copy(f);
    r.ftr = 30; r.bmr = 0; r.hours = 400; r.fxp = 0.6;
    joinPilots(g, r, 10, 'ftr', 'pool');
    expect(r.hours).toBeCloseTo((400 * 30 + A.pool! * 10) / 40, 9);
    expect(r.fxp).toBeCloseTo((0.6 * 30 + A.poolXp * 10) / 40, 9);
  });

  it('a bought wing, a new carrier wing and a bomber group\'s new crews come from school with no experience', () => {
    const g = open(true);
    const f = copy(land(g, 'GER')[0]);
    f.ftr = 30; f.bmr = 0; f.hours = 400; f.fxp = 0.4;
    joinPilots(g, f, 30, 'ftr', 'school');
    expect(f.hours).toBeCloseTo(325, 9);
    expect(f.fxp).toBeCloseTo(0.2, 9);
    armsOf(g, 'UK')!.air.deck = 500;
    const w = copy(g.airFleets.find((x) => x.nat === 'UK' && deckBred(x))!);
    w.ftr = 0; w.bmr = 0; delete w.hours; delete w.fxp;
    joinPilots(g, w, 18, 'ftr', 'school');
    expect(w.hours).toBe(500);
    expect(w.fxp).toBe(0);
    const b = copy(land(g, 'GER').find((x) => x.bmr > 0)!);
    b.hours = 250; b.fxp = 0.5;
    joinPilots(g, b, b.bmr, 'bmr', 'pool');
    expect(b.hours).toBe(250);
    expect(b.fxp).toBeCloseTo(0.25, 9);
  });

  it('a group split off flies with the same pilots', () => {
    const g = open(true);
    const f = land(g, 'GER').find((x) => x.ftr > 0)!;
    const whole: AirFleet = { ...copy(f), id: g.nextAir++, name: 'Test Command', kind: undefined, parent: undefined, share: undefined, ftr: 2 * AIRWAR.groups.most + 10, est: { ftr: 2 * AIRWAR.groups.most + 10, bmr: 0 }, bmr: 0, hours: 310, fxp: 0.3 };
    g.airFleets.push(whole);
    const parts = splitFleet(g, whole);
    expect(parts.length).toBeGreaterThan(1);
    for (const x of parts) { expect(x.hours, x.name).toBe(310); expect(x.fxp, x.name).toBe(0.3); }
  });
});

describe('on: seasoning, the ace\'s school, the crews\' head start and doctrine', () => {
  it('every group seasons in a fight survived, leader or not, in the leader\'s steps', () => {
    const g = open(true);
    // Three months of the war: groups that fought have more experience than they started with.
    const before = new Map(g.airFleets.map((f) => [f.id, fxpOf(g, f)]));
    for (let m = 0; m < 3; m++) runTurnAI(g);
    const rose = g.airFleets.filter((f) => before.has(f.id) && fxpOf(g, f) > before.get(f.id)! + 1e-9);
    expect(rose.length).toBeGreaterThan(0);
    expect(rose.some((f) => !f.leader)).toBe(true);
    for (const f of g.airFleets) expect(fxpOf(g, f)).toBeLessThanOrEqual(1);
  });

  it('an air ace home to train pilots steps the flying school, never the land drafts\' schools; the step is unread until laid', () => {
    const g = open(true), A = armsOf(g, 'GER')!.air;
    const schools = { ...g.stories.schools };
    aceSchool(g, 'GER');
    expect(A.school).toBe(250);
    const was = UNITS.air.aceStep;
    UNITS.air.aceStep = 25;
    try {
      aceSchool(g, 'GER');
      expect(A.school).toBe(275);
      aceSchool(g, 'UK');
      expect(armsOf(g, 'UK')!.air.school).toBeNull();
    } finally { UNITS.air.aceStep = was; }
    expect(g.stories.schools).toEqual(schools);
  });

  it('a new bomber crew\'s head start is its nation\'s school\'s, for every nation, none until the scale is laid', () => {
    const g = open(true);
    g.human = 'UK';
    expect(headStart(g, 'GER')).toBe(0);
    expect(headStart(g, 'UK')).toBe(0);
    const was = UNITS.air.head;
    UNITS.air.head = 0.01;
    try {
      expect(headStart(g, 'GER')).toBe(2);
      expect(headStart(g, 'UK')).toBe(0);
    } finally { UNITS.air.head = was; }
  });

  it('fighter doctrine is the store\'s Way: none where unread; a change trains in until its month', () => {
    const g = open(true), A = armsOf(g, 'GER')!.air;
    expect(A.fighter).toBeNull();
    expect(doctrineOf(g, 'GER')).toBeUndefined();
    A.fighter = { id: 'turn', next: 'dive-zoom', ready: g.turn + 2, by: 'chosen' };
    expect(doctrineOf(g, 'GER')).toBe('turn');
    g.turn += 2;
    expect(wayNow(g, A.fighter)).toBe('dive-zoom');
    expect(doctrineOf(g, 'GER')).toBe('dive-zoom');
  });

  it('the field reads a group\'s experience as a leader\'s level', () => {
    const g = open(true);
    const t = raidTargets(g, 'superiority', 'GER')[0];
    const ids = planRaid(g, 'superiority', t.id, undefined, 'GER').picked ?? [];
    if (!ids.length) return;
    const f = g.airFleets.find((x) => x.id === ids[0])!;
    f.fxp = 0.5;
    const tk = airTicket(g, 'superiority', t.id, ids, 'GER').ticket!;
    const force = tk.att.forces.find((x) => x.corps === f.id)!;
    expect(force.xp).toBeCloseTo(levelOf(0.5) / 3, 9);
    expect(levelOf(1)).toBe(AIR.leader.maxLevel);
  });
});

describe('on: the pilots in the fighters\' fight, k as fitted', () => {
  it('a pair whose hours are both laid: k times the log ratio of the hours, and the held level leaves it; else 0', () => {
    const k = FIGHTERS.law.k, f = {} as AirFleet;
    expect(k).toBeGreaterThan(0);
    const a = { f, w: 1, h: 250, q: 1.1 }, d = { f, w: 1, h: 350, q: 1.05 };
    expect(pilotsEdge(a, d, k)).toBeCloseTo(k * Math.log(250 / 350) - Math.log(1.1 / 1.05), 12);
    expect(pilotsEdge(a, { ...d, h: null }, k)).toBe(0);
    expect(pilotsEdge({ ...a, h: undefined }, d, k)).toBe(0);
    expect(pilotsEdge(a, d, 0)).toBe(0);
  });

  it('in a sweep: both sides keep their level until the defenders\' school is read; then the hours price the pilots', () => {
    const g = open(true);
    // The first sweep whose defenders are one nation's land groups, with no school read.
    for (const t of raidTargets(g, 'superiority', 'GER')) {
      const ids = planRaid(g, 'superiority', t.id, undefined, 'GER').picked ?? [];
      const fs = g.airFleets.filter((x) => ids.includes(x.id));
      if (!fs.length) continue;
      const a1 = assessAir(g, fs, 'superiority', t.id);
      const nats = new Set(a1.def.map((x) => x.f.nat));
      if (a1.why || a1.Epow <= 0 || a1.Dpow <= 0 || nats.size !== 1 || a1.def.some((x) => deckBred(x.f))) continue;
      const q = (xs: AirFleet[]) => new Set(xs.map((x) => heldOf(g, x)));
      const qa = q(fs), qd = q(a1.def.map((x) => x.f));
      if (qa.size !== 1 || qd.size !== 1) continue;
      const nat = [...nats][0], A = armsOf(g, nat)!.air;
      expect(A.school).toBeNull();
      for (const x of a1.def) expect(hoursOf(g, x.f)).toBeNull();
      // Read a school for the defenders: their groups take its hours, and the pair prices the pilots in place of the levels.
      A.school = 350; A.pool = 350;
      openPilots(g);
      const a2 = assessAir(g, fs, 'superiority', t.id);
      const want = FIGHTERS.law.k * Math.log(250 / 350) - Math.log([...qa][0] / [...qd][0]);
      expect(a2.typeEdge! - a1.typeEdge!).toBeCloseTo(want, 9);
      expect(a2.Epow / a1.Epow).toBeCloseTo(Math.exp(want / 2), 9);
      expect(a2.Dpow / a1.Dpow).toBeCloseTo(Math.exp(-want / 2), 9);
      return;
    }
    throw new Error('no sweep with one nation\'s land groups defending');
  });
});
