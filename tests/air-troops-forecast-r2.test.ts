// A raid on troops: which fighters the ground fire is charged on (release 2; the design director's ruling of 25 November
// 2026, 05:12Z, on lane (a)'s review): "the forecast charges ground fire only on the fighters expected to be left after
// the air fight, and only on those with the reach to strafe"; the card, the settled raid and the fought raid must agree,
// the settled raid pricing the same expected strafers behind the same switch (src/campaign/strafers.ts). The design
// director's answers of 12:59Z (design-director/air-names-1125.md): one switch, the lane's own (1); one count of
// strafers left pays the ground fire and adds the weight (2); the settled raid charges what its own throws leave, and the
// card is the exact expectation of the settled raid, the throws kept same-keyed (4).
//
// The raid is lane (a)'s own: France on the German army in the Saar in September 1939. Groupement de Chasse 3 flies from
// Lorraine, 65 km off, well inside its reach to strafe; Groupement de Chasse 1 is moved to Belfort, 190 km off, inside
// its fighter range (220 km) but beyond its reach to strafe (0.6 of it, 132 km): it escorts and goes home.
import { afterEach, describe, expect, it } from 'vitest';
import { launchRaid, raidTargets } from '../src/campaign/airwar';
import { AIR, AIRWAR, airRange, assessAir, canStrafe, forecastAir, kmTo, strafersExpected } from '../src/campaign/air';
import { createGame, setWar } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { Rng } from '../src/core/rng';
import { loadGame } from '../src/campaign/save';
import { strafeOn, strafeWatch } from '../src/campaign/strafers';
import type { Game } from '../src/campaign/types';
import { AirSim, airBattle } from '../src/battle/air/sim';
import { airTicket, flyKinds, foughtAir, raidGroups } from '../src/battle/air/ticket';
import { TROOPS } from '../src/battle/air/troops';

type Told = { route: 'card' | 'settled' | 'fought'; target: number; nat: string; strafers: number[]; fire: number; weight: number };
let heard: Told[] = [];

afterEach(() => { flyKinds([]); strafeWatch.tell = undefined; heard = []; });

const LORRAINE = 10, BELFORT_GROUP = 9, BOMBERS = 38;

/** The Saar raid, k-th luck; with `far`, Groupement de Chasse 1 flies from Belfort beside the raid's own groups. */
function saar(k: number, groups: 'both' | 'far' = 'both') {
  const g = startOpen(createGame(7, 'FRA'));
  setWar(g, 'FRA', 'GER');
  g.seed = (7 + 7919 * k) >>> 0;
  g.flags.airBattles = k;
  const target = raidTargets(g, 'ground', 'FRA').find((x) => g.regions[x.id].name === 'Saar')!.id;
  expect(raidGroups(g, 'ground', target, 'FRA')).toEqual([BOMBERS, LORRAINE]);
  const far = g.airFleets.find((f) => f.id === BELFORT_GROUP)!;
  far.base = g.regions.find((r) => r.name === 'Belfort')!.id;
  const ids = groups === 'both' ? [BOMBERS, LORRAINE, BELFORT_GROUP] : [BOMBERS, BELFORT_GROUP];
  return { g, target, ids, nat: 'FRA' };
}
const on = (): void => { flyKinds(['ground']); strafeWatch.tell = (x) => { heard.push(JSON.parse(JSON.stringify(x))); }; };
const fleetsOf = (g: Game, ids: number[]) => g.airFleets.filter((f) => ids.includes(f.id));
/** The groups with the reach to strafe by the rule itself: the troops within ground.reach of the group's fighter range. */
const byRule = (g: Game, target: number, ids: number[]): number[] =>
  fleetsOf(g, ids).filter((f) => f.ftr > 0 && kmTo(g, f, target) <= AIRWAR.ground.reach! * airRange(f, g).ftr).map((f) => f.id).sort((x, y) => x - y);
function fight(s: AirSim): void {
  s.ai.att = true; s.ai.def = true;
  let guard = 0;
  while (!s.over && guard++ < 20_000) s.step();
  expect(s.over).toBeTruthy();
}
const sorted = (x: number[] | undefined): number[] => [...(x ?? [])].sort((a, b) => a - b);

describe('the strafers: card, settled raid and fought raid charge the ground fire by one rule', () => {
  it('the same raid through the card, the staff\'s settlement and a fought battle\'s booking: the same strafers, those expected left with the reach', () => {
    for (const k of [0, 1]) {
      // The card.
      const c = saar(k);
      on();
      heard = [];
      expect(strafeOn()).toBe(true);
      const rule = byRule(c.g, c.target, c.ids);
      expect(rule).toEqual([LORRAINE]);
      const a = assessAir(c.g, fleetsOf(c.g, c.ids), 'ground', c.target);
      // Both fighter groups are in the raid (each within its fighter range), but only one has the reach to strafe.
      expect(sorted(a.att.filter((x) => x.ftr > 0).map((x) => x.f.id))).toEqual([BELFORT_GROUP, LORRAINE]);
      const fc = forecastAir(c.g, a, 'ground', c.target);
      expect(sorted(fc.strafers)).toEqual(rule);
      const air = fc.ftrLoss - fc.fire!;
      // (Changed on the 12:59Z answer 4: the card's figure is now the settled raid's exact expectation, strafersExpected,
      // no longer the even spread x.ftr x (1 - air / E); the 200-luck comparison below holds it to the settled raid.)
      const s0 = strafersExpected(c.g, a, air);
      expect(fc.fire!).toBeCloseTo(s0.fire, 12);
      expect(fc.fire!).toBeCloseTo(s0.left * AIRWAR.ground.fire, 12);
      // Not every fighter of the raid, and not before the air fight: less than the card charged on d06286d4.
      expect(fc.fire!).toBeLessThan(a.E * AIRWAR.ground.fire);
      expect(sorted(a.att.filter((x) => x.ftr > 0 && canStrafe(c.g, x)).map((x) => x.f.id))).toEqual(rule);
      expect(s0.ids).toEqual(rule);
      // The card was heard, once, naming the same groups, at the same fire and weight.
      const cards = heard.filter((x) => x.route === 'card');
      expect(cards.length).toBe(1);
      expect(sorted(cards[0].strafers)).toEqual(rule);
      expect(cards[0].fire).toBeCloseTo(fc.fire!, 12);
      expect(cards[0].weight).toBeCloseTo(s0.weight, 12);
      // The ticket the field is armed from says the same groups, and the card's own ground fire.
      heard = [];
      const t = airTicket(c.g, 'ground', c.target, c.ids, c.nat).ticket!;
      expect(t).toBeTruthy();
      expect(sorted(t.place.marks!.law.reach)).toEqual(rule);
      expect(t.place.forecast.fire!).toBeCloseTo(fc.fire!, 9);

      // The staff's settlement of the same raid (no battle): the ground fire only on the groups with the reach, those of
      // their fighters the air fight left.
      const s = saar(k);
      on();
      launchRaid(s.g, 'ground', s.target, s.ids, s.nat);
      const settled = heard.filter((x) => x.route === 'settled');
      expect(settled.length).toBe(1);
      expect(sorted(settled[0].strafers)).toEqual(rule);

      // The same raid fought out by the computer on both sides, and booked back.
      heard = [];
      const f = saar(k);
      on();
      const sim = airBattle(airTicket(f.g, 'ground', f.target, f.ids, f.nat).ticket!) as AirSim;
      fight(sim);
      const o = sim.outcome();
      // Only the group with the reach went down on the troops; the one without escorted and went home.
      const down = new Set(sim.history().filter((e) => e.what === 'air:strafe').map((e) => sim.units.find((u) => u.id === e.unit)!.corps));
      expect([...down].sort((x, y) => x - y)).toEqual(rule);
      expect(sorted(o.strafers)).toEqual(rule);
      expect(o.strafeFire!).toBeGreaterThan(0);
      // Each fighter that went down paid the campaign's own share, as the card charged each it expected: on the passes
      // it completed (strafed) and on any it was partway down when the battle ended or it was jumped (at most one pass).
      const onePass = sim.units.filter((u) => down.has(u.corps) && u.air === 'ftr').reduce((t, u) => t + u.n0 / TROOPS.strafe.passes, 0);
      expect(o.strafeFire!).toBeGreaterThanOrEqual(o.strafed! * AIRWAR.ground.fire - 1e-9);
      expect(o.strafeFire!).toBeLessThanOrEqual((o.strafed! + onePass) * AIRWAR.ground.fire + 1e-9);
      heard = [];
      launchRaid(f.g, 'ground', f.target, f.ids, f.nat, foughtAir(o));
      const fought = heard.filter((x) => x.route === 'fought');
      expect(fought.length).toBe(1);
      expect(sorted(fought[0].strafers)).toEqual(rule);
      expect(fought[0].weight).toBe(o.strafed);
    }
  });

  it('no fighter with the reach to strafe: no ground fire is charged on fighters at all, on the card, settled or fought', () => {
    for (const k of [0, 1]) {
      const c = saar(k, 'far');
      on();
      expect(byRule(c.g, c.target, c.ids)).toEqual([]);
      const a = assessAir(c.g, fleetsOf(c.g, c.ids), 'ground', c.target);
      expect(a.E).toBeGreaterThan(0);
      const fc = forecastAir(c.g, a, 'ground', c.target);
      expect(fc.fire).toBe(0);
      expect(fc.strafers).toEqual([]);
      const t = airTicket(c.g, 'ground', c.target, c.ids, c.nat).ticket!;
      expect(t.place.marks!.law.reach).toEqual([]);
      expect(t.place.forecast.fire).toBe(0);

      const s = saar(k, 'far');
      on();
      heard = [];
      launchRaid(s.g, 'ground', s.target, s.ids, s.nat);
      const settled = heard.find((x) => x.route === 'settled')!;
      expect(settled.strafers).toEqual([]);
      expect(settled.fire).toBe(0);
      expect(settled.weight).toBe(0);

      const f = saar(k, 'far');
      on();
      const sim = airBattle(airTicket(f.g, 'ground', f.target, f.ids, f.nat).ticket!) as AirSim;
      fight(sim);
      const o = sim.outcome();
      expect(sim.history().some((e) => e.what === 'air:strafe')).toBe(false);
      expect(o.strafed).toBe(0);
      expect(o.strafers).toEqual([]);
      expect(o.strafeFire).toBe(0);
    }
  });

  it('the air fight expected to leave none: the card charges no ground fire, and the settled raid none on fighters it left none of', () => {
    const was = AIR.combat.fighters;
    try {
      // (Setup only: the month's fighter losses scaled up until the card expects the air fight to take every escort.)
      AIR.combat.fighters = was * 400;
      const c = saar(0);
      on();
      const a = assessAir(c.g, fleetsOf(c.g, c.ids), 'ground', c.target);
      const fc = forecastAir(c.g, a, 'ground', c.target);
      expect(fc.ftrLoss - fc.fire!).toBeGreaterThanOrEqual(a.E);
      expect(fc.fire).toBe(0);
      expect(fc.strafers).toEqual([]);
      // The reach is still there; it is the air fight that leaves nobody to go down.
      expect(byRule(c.g, c.target, c.ids)).toEqual([LORRAINE]);
      const t = airTicket(c.g, 'ground', c.target, c.ids, c.nat).ticket!;
      expect(t.place.forecast.fire).toBe(0);
      expect(t.place.marks!.law.reach).toEqual([LORRAINE]);

      const s = saar(0);
      on();
      heard = [];
      const rep = launchRaid(s.g, 'ground', s.target, s.ids, s.nat)!;
      const settled = heard.find((x) => x.route === 'settled')!;
      // Every escort fell in the air: none is left to go down, so none pays the ground fire.
      expect(rep.att.some((x) => x.ftr > 0)).toBe(true);
      expect(rep.att.filter((x) => x.ftr > 0).every((x) => x.lostFtr === x.ftr)).toBe(true);
      expect(settled.strafers).toEqual([]);
      expect(settled.fire).toBe(0);
    } finally {
      AIR.combat.fighters = was;
    }
  });

  it('switched off (the lane\'s switch, the only one): the card, the ticket and the staff\'s raid are as on d06286d4', () => {
    // (Changed on the 12:59Z answer 1: the rule's own key, strafeRule / TC_STRAFE, is gone; the lane's switch alone turns it.)
    const plain = () => {
      const c = saar(3);
      const a = assessAir(c.g, fleetsOf(c.g, c.ids), 'ground', c.target);
      const fc = forecastAir(c.g, a, 'ground', c.target);
      const rep = launchRaid(c.g, 'ground', c.target, c.ids, c.nat)!;
      return { fc: JSON.parse(JSON.stringify(fc)), a, rep };
    };
    strafeWatch.tell = (x) => { heard.push(x); };
    flyKinds([]);
    expect(strafeOn()).toBe(false);
    const off = plain();
    // As on d06286d4: every fighter of the raid charged, before the air fight; no strafers named; nothing told.
    expect(off.fc.fire).toBeUndefined();
    expect(off.fc.strafers).toBeUndefined();
    expect(heard.length).toBe(0);
    // The ticket's law carries no reach with the switch off (no ticket for a raid on troops is cut at all then).
    expect(airTicket(saar(0).g, 'ground', saar(0).target, saar(0).ids, 'FRA').ticket).toBeNull();
    // Through the environment, the rule turns with the lane's switch and with nothing else.
    (globalThis as { TC_FLY?: string }).TC_FLY = 'ground';
    try { expect(strafeOn()).toBe(true); } finally { delete (globalThis as { TC_FLY?: string }).TC_FLY; }
    expect(strafeOn()).toBe(false);
    (globalThis as { TC_STRAFE?: string }).TC_STRAFE = '1';
    try { expect(strafeOn()).toBe(false); } finally { delete (globalThis as { TC_STRAFE?: string }).TC_STRAFE; }
    flyKinds(['ground']);
    expect(strafeOn()).toBe(true);
    flyKinds([]);
    expect(strafeOn()).toBe(false);
  });
});

// The card against the settled raid over many lucks (12:59Z, answer 4). The review's own raid: the bombers, Groupement
// de Chasse 3 at Lorraine (65 km), Groupe 36 moved to Verdun (127 km, inside the reach to strafe but with a crowded field)
// and Groupement de Chasse 1 at Belfort (190 km, out of reach). AIR.combat.fighters is scaled in setup only, to make the
// air fight deadlier; no die and no law of the game is changed.
const REVIEW_IDS = [BOMBERS, LORRAINE, 36, BELFORT_GROUP];
let reviewBase: string | null = null;
function reviewRaid(k: number) {
  if (!reviewBase) {
    const g = startOpen(createGame(7, 'FRA'));
    setWar(g, 'FRA', 'GER');
    g.airFleets.find((f) => f.id === 36)!.base = g.regions.find((r) => r.name === 'Verdun')!.id;
    g.airFleets.find((f) => f.id === BELFORT_GROUP)!.base = g.regions.find((r) => r.name === 'Belfort')!.id;
    reviewBase = saveGame(g);
  }
  const g = loadGame(reviewBase)!;
  g.seed = (7 + 7919 * k) >>> 0;
  g.flags.airBattles = k;
  const target = raidTargets(g, 'ground', 'FRA').find((x) => g.regions[x.id].name === 'Saar')!.id;
  return { g, target };
}
function cardAgainstSettled(mult: number, seeds = 200) {
  const was = AIR.combat.fighters;
  try {
    AIR.combat.fighters = was * mult;
    on();
    heard = [];
    const c = reviewRaid(0);
    const a = assessAir(c.g, fleetsOf(c.g, REVIEW_IDS), 'ground', c.target);
    const fc = forecastAir(c.g, a, 'ground', c.target);
    const cards = heard.filter((x) => x.route === 'card');
    expect(cards.length).toBe(1);
    const card = cards[0];
    expect(card.fire).toBeCloseTo(fc.fire!, 12);
    let fire = 0, weight = 0, same = 0;
    for (let k = 0; k < seeds; k++) {
      heard = [];
      const s = reviewRaid(k);
      launchRaid(s.g, 'ground', s.target, REVIEW_IDS, 'FRA');
      const st = heard.filter((x) => x.route === 'settled');
      expect(st.length).toBe(1);
      // Never a group out of reach.
      expect(st[0].strafers).not.toContain(BELFORT_GROUP);
      fire += st[0].fire; weight += st[0].weight;
      if (JSON.stringify(sorted(st[0].strafers)) === JSON.stringify(sorted(card.strafers))) same++;
    }
    return { card, fire: fire / seeds, weight: weight / seeds, same, seeds };
  } finally { AIR.combat.fighters = was; }
}
const near = (x: number, y: number, tol: number) => { expect(Math.abs(x / y - 1)).toBeLessThanOrEqual(tol); };

describe('the card is the exact expectation of the settled raid (200 lucks)', () => {
  it('at the game\'s own figures: mean fire and mean weight within 3% of the card, the same strafers', () => {
    const r = cardAgainstSettled(1);
    expect(sorted(r.card.strafers)).toEqual([LORRAINE, 36]);
    near(r.fire, r.card.fire, 0.03); near(r.weight, r.card.weight, 0.03);
    expect(r.same).toBe(r.seeds);
  });
  it('the air fight six times deadlier: within 3%', () => {
    const r = cardAgainstSettled(6);
    near(r.fire, r.card.fire, 0.03); near(r.weight, r.card.weight, 0.03);
  });
  it('thirty times deadlier: within 8%', () => {
    const r = cardAgainstSettled(30);
    expect(r.card.fire).toBeGreaterThan(0);
    near(r.fire, r.card.fire, 0.08); near(r.weight, r.card.weight, 0.08);
  });
  it('forty-five times deadlier: the card names the strafers the settled raid names on most lucks, and still charges them', () => {
    const r = cardAgainstSettled(45);
    expect(r.card.strafers.length).toBeGreaterThan(0);
    expect(r.card.fire).toBeGreaterThan(0);
    expect(r.same).toBeGreaterThan(r.seeds / 2);
  });
});

describe('the ground fire\'s throws are same-keyed (12:59Z, answer 4)', () => {
  /** Every Rng drawn from during one settled raid, and how many draws each gave; and the month stream's state after. */
  function counted(reachShare: number, lane: boolean) {
    const was = AIRWAR.ground.reach!, nx = Rng.prototype.next;
    const per = new Map<Rng, number>();
    try {
      AIRWAR.ground.reach = reachShare;
      if (lane) on(); else { flyKinds([]); strafeWatch.tell = (x) => { heard.push(JSON.parse(JSON.stringify(x))); }; }
      heard = [];
      const s = reviewRaid(5);
      Rng.prototype.next = function (this: Rng) { per.set(this, (per.get(this) ?? 0) + 1); return nx.call(this); };
      const rep = launchRaid(s.g, 'ground', s.target, REVIEW_IDS, 'FRA')!;
      Rng.prototype.next = nx;
      return { rep, counts: [...per.values()], month: JSON.stringify((s.g as unknown as { rng: unknown }).rng), told: heard.filter((x) => x.route === 'settled') };
    } finally { Rng.prototype.next = nx; AIRWAR.ground.reach = was; }
  }
  it('a group that no longer throws (out of reach) moves no other group\'s throw, no later throw of the raid, and nothing in the month', () => {
    // Groupe 36 at Verdun (127 km of 220) has the reach at 0.6 and loses it at 0.5 (setup only); its place in the air
    // fight does not depend on the reach to strafe, so everything but its own ground fire is the same raid.
    const both = counted(0.6, true), one = counted(0.5, true), off = counted(0.6, false);
    expect(sorted(both.told[0].strafers)).toEqual([LORRAINE, 36]);
    expect(sorted(one.told[0].strafers)).toEqual([LORRAINE]);
    // Groupement de Chasse 3's ground fire, and every other group's book, are the same whether 36 throws or not.
    for (const id of REVIEW_IDS) {
      if (id === 36) continue;
      expect(JSON.stringify(one.rep.att.find((x) => x.id === id))).toBe(JSON.stringify(both.rep.att.find((x) => x.id === id)));
    }
    expect(JSON.stringify(one.rep.def)).toBe(JSON.stringify(both.rep.def));
    // The raid's own stream gives as many draws in all three (as with the switch off: each group's ground fire is still
    // thrown on it and set aside); the strafers' own streams come beside it, one each, two or three draws apiece.
    const big = (x: number[]) => Math.max(...x);
    expect(big(both.counts)).toBe(big(off.counts));
    expect(big(one.counts)).toBe(big(off.counts));
    const small = (x: number[]) => x.filter((n) => n !== big(x)).sort((p, q) => p - q);
    expect(small(both.counts).length).toBe(small(one.counts).length + 1);
    for (const n of [...small(both.counts), ...small(one.counts)]) expect([2, 3]).toContain(n);
    expect(small(off.counts)).toEqual([]);
    // Nothing after the raid moves: the month's own stream stands where it stood.
    expect(one.month).toBe(both.month);
    expect(both.month).toBe(off.month);
  });
});
