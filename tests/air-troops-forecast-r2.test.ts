// A raid on troops: which fighters the ground fire is charged on (release 2; the design director's ruling of 25 November
// 2026, 05:12Z, on lane (a)'s review): "the forecast charges ground fire only on the fighters expected to be left after
// the air fight, and only on those with the reach to strafe"; the card, the settled raid and the fought raid must agree,
// the settled raid pricing the same expected strafers behind the same switch (src/campaign/strafers.ts). The design
// director's answers of 12:59Z (design-director/air-names-1125.md): one switch, the lane's own (1); one count of
// strafers left pays the ground fire and adds the weight (2); the settled raid charges what its own throws leave, and the
// card is the exact expectation of the settled raid, the throws kept same-keyed (4).
//
// The raid is lane (a)'s own: France on the German army in the Saar in September 1939. Groupement de Chasse 3 flies from
// Lorraine, 65 km off, well inside its reach to strafe; Groupement de Chasse 1 is moved to Namur, 216 km off, inside
// its fighter range (220 km) but beyond its reach to strafe (0.9 of it, 198 km; the design director, 13:35Z): it escorts
// and goes home. (It stood at Belfort, 190 km off, while the reach was 0.6 of the range, 132 km; at 0.9 Belfort is in
// reach, and the first test says so.)
import { afterEach, describe, expect, it } from 'vitest';
import { launchRaid, raidTargets } from '../src/campaign/airwar';
import { AIR, AIRWAR, airRange, assessAir, canStrafe, forecastAir, kmTo, skyWon, strafersExpected } from '../src/campaign/air';
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

type Told = { route: 'card' | 'settled' | 'fought'; target: number; nat: string; strafers: number[]; fire: number; weight: number; cut?: number };
let heard: Told[] = [];

afterEach(() => { flyKinds([]); strafeWatch.tell = undefined; heard = []; });

const LORRAINE = 10, FAR_GROUP = 9, BOMBERS = 38;
/** Where Groupement de Chasse 1 is put to be out of the reach to strafe: 216 km of its 220, beyond 0.9 of it. */
const FAR_FIELD = 'Namur';

/** The Saar raid, k-th luck; with `far`, Groupement de Chasse 1 flies from Namur beside the raid's own groups. */
function saar(k: number, groups: 'both' | 'far' = 'both') {
  const g = startOpen(createGame(7, 'FRA'));
  setWar(g, 'FRA', 'GER');
  g.seed = (7 + 7919 * k) >>> 0;
  g.flags.airBattles = k;
  const target = raidTargets(g, 'ground', 'FRA').find((x) => g.regions[x.id].name === 'Saar')!.id;
  expect(raidGroups(g, 'ground', target, 'FRA')).toEqual([BOMBERS, LORRAINE]);
  const far = g.airFleets.find((f) => f.id === FAR_GROUP)!;
  far.base = g.regions.find((r) => r.name === FAR_FIELD)!.id;
  const ids = groups === 'both' ? [BOMBERS, LORRAINE, FAR_GROUP] : [BOMBERS, FAR_GROUP];
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
      expect(sorted(a.att.filter((x) => x.ftr > 0).map((x) => x.f.id))).toEqual([FAR_GROUP, LORRAINE]);
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

  it('the ruled reach, 0.9 of the fighter range (13:35Z): a group inside it strafes, one beyond it does not', () => {
    expect(AIRWAR.ground.reach).toBe(0.9);
    // Groupement de Chasse 1 (fighter range 220 km): at Belfort, 190 km off (0.86 of its range), inside 0.9 and beyond
    // the old 0.6; at Namur, 216 km off (0.98), beyond 0.9 though still within its range, so it flies the escort.
    // (Changed on the sky gate, the design director 16:04Z: Groupement de Chasse 1 alone, 150 fighters, cannot win the sky
    // from the German group over the Saar, so as the raid stands nobody goes down from either field. The reach is read
    // again with the German fighters grounded in setup (none rose: the sky is won), where it alone decides.)
    for (const [field, strafes, clear] of [['Belfort', true, false], [FAR_FIELD, false, false], ['Belfort', true, true], [FAR_FIELD, false, true]] as const) {
      const c = saar(0, 'far');
      c.g.airFleets.find((f) => f.id === FAR_GROUP)!.base = c.g.regions.find((r) => r.name === field)!.id;
      if (clear) c.g.airFleets.find((f) => f.id === 3)!.ftr = 0;
      on();
      heard = [];
      const f9 = c.g.airFleets.find((f) => f.id === FAR_GROUP)!;
      const d = kmTo(c.g, f9, c.target), R = airRange(f9, c.g).ftr;
      expect(R).toBe(220);
      expect(d).toBeLessThanOrEqual(R);
      expect(d / R > 0.6).toBe(true);
      expect(d <= 0.9 * R).toBe(strafes);
      const a = assessAir(c.g, fleetsOf(c.g, c.ids), 'ground', c.target);
      expect(a.att.some((x) => x.f.id === FAR_GROUP && x.ftr > 0)).toBe(true);
      expect(a.att.filter((x) => x.ftr > 0 && canStrafe(c.g, x)).map((x) => x.f.id)).toEqual(strafes ? [FAR_GROUP] : []);
      const fc = forecastAir(c.g, a, 'ground', c.target);
      expect(skyWon(c.g, a)).toBe(clear);
      expect(fc.strafers).toEqual(strafes && clear ? [FAR_GROUP] : []);
      expect(fc.fire! > 0).toBe(strafes && clear);
      const s = saar(0, 'far');
      s.g.airFleets.find((f) => f.id === FAR_GROUP)!.base = s.g.regions.find((r) => r.name === field)!.id;
      if (clear) s.g.airFleets.find((f) => f.id === 3)!.ftr = 0;
      heard = [];
      launchRaid(s.g, 'ground', s.target, s.ids, s.nat);
      const settled = heard.find((x) => x.route === 'settled')!;
      expect(settled.strafers).toEqual(strafes && clear ? [FAR_GROUP] : []);
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
// and Groupement de Chasse 1 at Namur (216 km of 220, out of reach at 0.9; it stood at Belfort, 190 km, at 0.6).
// AIR.combat.fighters is scaled in setup only, to make the air fight deadlier; no die and no law of the game is changed.
const REVIEW_IDS = [BOMBERS, LORRAINE, 36, FAR_GROUP];
let reviewBase: string | null = null;
function reviewRaid(k: number) {
  if (!reviewBase) {
    const g = startOpen(createGame(7, 'FRA'));
    setWar(g, 'FRA', 'GER');
    g.airFleets.find((f) => f.id === 36)!.base = g.regions.find((r) => r.name === 'Verdun')!.id;
    g.airFleets.find((f) => f.id === FAR_GROUP)!.base = g.regions.find((r) => r.name === FAR_FIELD)!.id;
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
      expect(st[0].strafers).not.toContain(FAR_GROUP);
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
    // Groupe 36 at Verdun (127 km of 220) has the reach at the ruled 0.9 and loses it at 0.5 (setup only); its place in
    // the air fight does not depend on the reach to strafe, so everything but its own ground fire is the same raid.
    // (Changed on the 13:35Z ruling: the 'both' and 'off' runs read the ruled figure, 0.9, where they read 0.6.)
    const R = AIRWAR.ground.reach!;
    expect(R).toBe(0.9);
    const both = counted(R, true), one = counted(0.5, true), off = counted(R, false);
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

// The card's cut on the troops is an expectation too (the review of the air follow-on, finding 2; the design director,
// 12:59Z question 4: where a term is not linear the card gives the expectation of the outcome). troopCut clips at
// airwar.json ground.max; where the clip binds on part of the raid's luck the card's cut is the mean of the settled
// raid's, not the cut at the expected weight. ground.max is lowered in setup only, to make the clip bind on the Saar.
describe('the card\'s cut on the troops is the expectation of the settled raid\'s (finding 2)', () => {
  function cuts(ids: number[], max: number | null, seeds: number, mult = 1) {
    const was = AIRWAR.ground.max, fw = AIR.combat.fighters;
    try {
      if (max !== null) AIRWAR.ground.max = max;
      AIR.combat.fighters = fw * mult;
      on();
      heard = [];
      const c = reviewRaid(0);
      const a = assessAir(c.g, fleetsOf(c.g, ids), 'ground', c.target);
      const fc = forecastAir(c.g, a, 'ground', c.target);
      const card = heard.filter((x) => x.route === 'card');
      expect(card.length).toBe(1);
      expect(card[0].cut).toBe(fc.cut);
      let sum = 0, clipped = 0;
      for (let k = 0; k < seeds; k++) {
        heard = [];
        const s = reviewRaid(k);
        launchRaid(s.g, 'ground', s.target, ids, 'FRA');
        const st = heard.filter((x) => x.route === 'settled');
        expect(st.length).toBe(1);
        sum += st[0].cut!;
        if (st[0].cut! >= AIRWAR.ground.max * cover() * (1 - 1e-12)) clipped++;
      }
      return { card: fc.cut, mean: sum / seeds, clipped };
    } finally { AIRWAR.ground.max = was; AIR.combat.fighters = fw; }
  }
  /** The ground's cover at the Saar, which troopCut multiplies the clipped cut by. */
  const cover = (): number => { const g = reviewRaid(0).g; const r = raidTargets(g, 'ground', 'FRA').find((x) => g.regions[x.id].name === 'Saar')!; return AIRWAR.ground.cover[g.regions[r.id].terrain] ?? 1; };

  it('where the clip cannot bind, the card\'s cut is the cut at the expected weight, to the last digit', () => {
    // At the game's own ground.max the Saar's cut lies far below the clip: the card reads the same with the clip lifted.
    for (const ids of [REVIEW_IDS, [BOMBERS]]) {
      const own = cuts(ids, null, 0), lifted = cuts(ids, 1, 0);
      expect(own.card).toBeGreaterThan(0);
      expect(own.card).toBe(lifted.card);
      expect(own.card / cover()).toBeLessThan(AIRWAR.ground.max);
    }
  });

  it('the clip binding on part of the luck: the card against the settled mean over 200 lucks (escort and strafers, and bombers alone)', () => {
    for (const [ids, mult] of [[REVIEW_IDS, 1], [REVIEW_IDS, 30], [[BOMBERS], 1]] as const) {
      // The clip set at the cut the card expects with no clip: it binds on about half the raid's luck.
      const free = cuts([...ids], 1, 0, mult).card / cover();
      const r = cuts([...ids], free, 200, mult);
      // On part of the luck the settled raid is clipped, on part not.
      expect(r.clipped).toBeGreaterThan(20);
      expect(r.clipped).toBeLessThan(180);
      // The cut at the expected weight would be the clip itself; the card is the mean, below it, as the settled raid is.
      expect(r.card).toBeLessThan(free * cover());
      expect(r.mean).toBeLessThan(free * cover());
      near(r.mean, r.card, 0.01);
      // And nearer the settled mean than the cut at the expected weight is.
      expect(Math.abs(r.card - r.mean)).toBeLessThan(Math.abs(free * cover() - r.mean));
    }
  });
});

// The sky gate (the design director, 16:04Z): "A group goes down only when it is free of the air fight. Settled: its side
// won the sky (the enemy's fighters beaten or gone)." The settled raid reads it on the rules' own test of a won sky
// (air.ts skyWon: sky.least fighters left over the target and sky.win, 1.65, times what the defenders still in the air can
// put up; none left, the sky is won), and the card is the settled raid's expectation with the gate in: a group is named
// when it is more likely than not to win the sky and have a fighter left. The review's raid again, the German group over
// the Saar (Jagdgeschwader 3's 200 fighters, id 3) given more fighters in setup only, so that the sky is surely won (as
// built), near the edge, or surely lost.
const JG = 3;
const gateBase: Record<number, string> = {};
function gateRaid(k: number, defenders: number) {
  if (!gateBase[defenders]) {
    reviewRaid(0);
    const g = loadGame(reviewBase!)!;
    const d = g.airFleets.find((f) => f.id === JG)!;
    d.ftr = defenders; d.est.ftr = Math.max(d.est.ftr, defenders);
    gateBase[defenders] = saveGame(g);
  }
  const g = loadGame(gateBase[defenders])!;
  g.seed = (7 + 7919 * k) >>> 0;
  g.flags.airBattles = k;
  const target = raidTargets(g, 'ground', 'FRA').find((x) => g.regions[x.id].name === 'Saar')!.id;
  return { g, target };
}
function gateAgainstSettled(defenders: number, seeds = 200) {
  on();
  heard = [];
  const c = gateRaid(0, defenders);
  const a = assessAir(c.g, fleetsOf(c.g, REVIEW_IDS), 'ground', c.target);
  const fc = forecastAir(c.g, a, 'ground', c.target);
  const s = strafersExpected(c.g, a, fc.ftrLoss - fc.fire!);
  const card = heard.filter((x) => x.route === 'card');
  expect(card.length).toBe(1);
  expect(card[0].strafers).toEqual(s.ids);
  expect(card[0].fire).toBeCloseTo(fc.fire!, 12);
  expect(card[0].weight).toBeCloseTo(s.weight, 12);
  let fire = 0, weight = 0, cut = 0, won = 0;
  const named: Record<number, number> = {};
  for (let k = 0; k < seeds; k++) {
    heard = [];
    const r = gateRaid(k, defenders);
    launchRaid(r.g, 'ground', r.target, REVIEW_IDS, 'FRA');
    const st = heard.filter((x) => x.route === 'settled');
    expect(st.length).toBe(1);
    fire += st[0].fire; weight += st[0].weight; cut += st[0].cut!;
    if (st[0].strafers.length) won++;
    for (const id of st[0].strafers) named[id] = (named[id] ?? 0) + 1;
  }
  return { fc, s, card: card[0], fire: fire / seeds, weight: weight / seeds, cut: cut / seeds, won, named, seeds };
}

describe('the sky gate (the design director, 16:04Z): a group goes down only where its side won the sky', () => {
  it('the rules\' own test, read over the target: no defender, or every one lost, wins it; 1.65 to 1 of those left and sky.least of the raid\'s own', () => {
    expect(AIR.sky.win).toBe(1.65);
    expect(AIR.sky.least).toBe(50);
    const c = gateRaid(0, 200);
    const a = assessAir(c.g, fleetsOf(c.g, REVIEW_IDS), 'ground', c.target);
    expect(a.def.length).toBe(1);
    // As the review's raid stands (200 German fighters): won.
    expect(skyWon(c.g, a)).toBe(true);
    // Every defender that rose lost: won, however few of the raid's fighters are left.
    const d = a.def[0].f, was = d.ftr;
    d.ftr = 0;
    for (const x of a.att) x.f.ftr = Math.min(x.f.ftr, 10);
    expect(skyWon(c.g, a)).toBe(true);
    // One defender left against fewer than sky.least of the raid's fighters: not won.
    d.ftr = 1;
    expect(skyWon(c.g, a)).toBe(false);
    d.ftr = was;
    // No defender rose at all: won.
    const none = gateRaid(0, 200);
    const b = assessAir(none.g, fleetsOf(none.g, REVIEW_IDS), 'ground', none.target);
    b.def.length = 0;
    expect(skyWon(none.g, b)).toBe(true);
    // A strong defence (800): lost.
    const s = gateRaid(0, 800);
    expect(skyWon(s.g, assessAir(s.g, fleetsOf(s.g, REVIEW_IDS), 'ground', s.target))).toBe(false);
  });

  it('surely won (the review\'s raid as built): the card as before to the last digit, and the settled raid sends its strafers down on every luck', () => {
    const r = gateAgainstSettled(200);
    expect(r.s.gate).toBeUndefined();
    expect(sorted(r.card.strafers)).toEqual([LORRAINE, 36]);
    expect(r.won).toBe(r.seeds);
    near(r.fire, r.card.fire, 0.03); near(r.weight, r.card.weight, 0.03); near(r.cut, r.fc.cut, 0.01);
  });

  it('surely lost (800 German fighters): the card names none and charges no ground fire; nobody goes down on any luck; the cut is the bombers\' alone', () => {
    const r = gateAgainstSettled(800);
    expect(r.s.gate).toBeUndefined();
    expect(r.card.strafers).toEqual([]);
    expect(r.fc.strafers).toEqual([]);
    expect(r.fc.fire).toBe(0);
    expect(r.card.weight).toBe(0);
    expect(r.won).toBe(0);
    expect(r.fire).toBe(0);
    expect(r.weight).toBe(0);
    near(r.cut, r.fc.cut, 0.01);
    // The reach is still there: it is the sky that keeps them up.
    const c = gateRaid(0, 800);
    expect(byRule(c.g, c.target, REVIEW_IDS)).toEqual([LORRAINE, 36]);
  });

  it('near the edge (309 German fighters): the card against the settled mean over 200 lucks, and the groups it names are those more likely than not to go down', () => {
    const r = gateAgainstSettled(309);
    // Won on part of the luck only.
    expect(r.s.gate).toBeDefined();
    expect(r.won).toBeGreaterThan(20);
    expect(r.won).toBeLessThan(r.seeds);
    // The lane's own tolerances: fire and weight within 3%, the cut within 1%.
    near(r.fire, r.card.fire, 0.03); near(r.weight, r.card.weight, 0.03); near(r.cut, r.fc.cut, 0.01);
    expect(sorted(r.card.strafers)).toEqual([LORRAINE, 36]);
    for (const id of [LORRAINE, 36]) expect(r.named[id]).toBeGreaterThan(r.seeds / 2);
  });

  it('past the edge (315 German fighters): the sky won on about a quarter of the lucks; the card names none, and is the settled mean within the lane\'s 8%', () => {
    const r = gateAgainstSettled(315);
    expect(r.s.gate).toBeDefined();
    expect(r.won).toBeGreaterThan(20);
    expect(r.won).toBeLessThan(r.seeds / 2);
    expect(r.card.strafers).toEqual([]);
    expect(r.fc.fire).toBeGreaterThan(0);
    // (A step's mean over 200 lucks is the noisiest figure the lane proves: its own widest tolerance, the 8% of the
    // thirty-times-deadlier raid.)
    near(r.fire, r.card.fire, 0.08); near(r.weight, r.card.weight, 0.08); near(r.cut, r.fc.cut, 0.08);
    for (const id of [LORRAINE, 36]) expect(r.named[id] ?? 0).toBeLessThan(r.seeds / 2);
  });

  it('fog on raids: the fighters last seen near the target, which the card\'s odds count as rising, are in its sky gate too', () => {
    // (Round five's check: the gate read only the groups known by name, so a card whose odds counted 800 fighters last
    // seen near the Eifel still named a strafer. A hand-made knowledge, as airfog.ts would give one end of its band.)
    on();
    const g = startOpen(createGame(7, 'FRA'));
    setWar(g, 'FRA', 'GER');
    const target = g.regions.find((r) => r.name === 'Eifel')!.id;
    const fl = fleetsOf(g, [BOMBERS, LORRAINE]);
    const card = (extra: number) => {
      const a = assessAir(g, fl, 'ground', target, { sure: () => false, extra, extraNat: 'GER', aa: 0, ruined: 0 });
      expect(a.why).toBeNull();
      return { a, fc: forecastAir(g, a, 'ground', target) };
    };
    // No group known for sure and none last seen: on that knowledge nothing rises, the sky is won, the group is named.
    const none = card(0);
    expect(none.a.def.length).toBe(0);
    expect(none.a.D).toBe(0);
    expect(none.a.unseen).toBeUndefined();
    expect(skyWon(g, none.a)).toBe(true);
    expect(none.fc.strafers).toEqual([LORRAINE]);
    expect(none.fc.fire!).toBeGreaterThan(0);
    // 800 last seen near it: the odds count them as rising (no group by name), and the gate counts the same fighters:
    // the sky is lost on every luck, nobody is named and no ground fire is charged.
    const many = card(800);
    expect(many.a.def.length).toBe(0);
    expect(many.a.unseen?.n).toBe(800);
    expect(many.a.D).toBeCloseTo(800 * many.a.unseen!.w, 9);
    expect(skyWon(g, many.a)).toBe(false);
    expect(many.fc.strafers).toEqual([]);
    expect(many.fc.fire).toBe(0);
    // A handful last seen: still won on every luck, and the card is the card of a won sky.
    const few = card(20);
    expect(few.a.unseen?.n).toBe(20);
    expect(skyWon(g, few.a)).toBe(true);
    expect(few.fc.strafers).toEqual([LORRAINE]);
    expect(few.fc.fire!).toBeGreaterThan(0);
  });

  it('switched off: the sky is not read, and every figure is as on the lane\'s switch off', () => {
    flyKinds([]);
    expect(strafeOn()).toBe(false);
    const c = gateRaid(3, 800);
    const a = assessAir(c.g, fleetsOf(c.g, REVIEW_IDS), 'ground', c.target);
    const fc = forecastAir(c.g, a, 'ground', c.target);
    expect(fc.strafers).toBeUndefined();
    expect(fc.fire).toBeUndefined();
  });
});
