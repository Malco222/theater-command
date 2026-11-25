// A raid on troops: which fighters the ground fire is charged on (release 2; the design director's ruling of 25 November
// 2026, 05:12Z, on lane (a)'s review): "the forecast charges ground fire only on the fighters expected to be left after
// the air fight, and only on those with the reach to strafe"; the card, the settled raid and the fought raid must agree,
// the settled raid pricing the same expected strafers behind the same switch (src/campaign/strafers.ts).
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
import { strafeOn, strafeRule, strafeWatch } from '../src/campaign/strafers';
import type { Game } from '../src/campaign/types';
import { AirSim, airBattle } from '../src/battle/air/sim';
import { airTicket, flyKinds, foughtAir, raidGroups } from '../src/battle/air/ticket';
import { TROOPS } from '../src/battle/air/troops';

type Told = { route: 'card' | 'settled' | 'fought'; target: number; nat: string; strafers: number[]; fire: number; weight: number };
let heard: Told[] = [];

afterEach(() => { flyKinds([]); strafeRule(false); strafeWatch.tell = undefined; heard = []; });

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
const on = (): void => { flyKinds(['ground']); strafeRule(true); strafeWatch.tell = (x) => { heard.push(JSON.parse(JSON.stringify(x))); }; };
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
      const left = a.att.filter((x) => rule.includes(x.f.id)).reduce((s, x) => s + x.ftr * Math.max(0, 1 - air / a.E), 0);
      expect(fc.fire!).toBeCloseTo(left * AIRWAR.ground.fire, 12);
      // Not every fighter of the raid, and not before the air fight: less than the card charged on d06286d4.
      expect(fc.fire!).toBeLessThan(a.E * AIRWAR.ground.fire);
      expect(sorted(a.att.filter((x) => x.ftr > 0 && canStrafe(c.g, x)).map((x) => x.f.id))).toEqual(rule);
      expect(strafersExpected(c.g, a, air).ids).toEqual(rule);
      expect(heard.filter((x) => x.route === 'card').every((x) => JSON.stringify(sorted(x.strafers)) === JSON.stringify(rule))).toBe(true);
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
      expect(rep.att.filter((x) => x.ftr > 0).every((x) => x.lostFtr === x.ftr)).toBe(true);
      expect(settled.strafers).toEqual([]);
      expect(settled.fire).toBe(0);
    } finally {
      AIR.combat.fighters = was;
    }
  });

  it('switched off (either key): the card, the ticket and the staff\'s raid are as on d06286d4', () => {
    const plain = (setup: () => void) => {
      const c = saar(3);
      setup();
      const a = assessAir(c.g, fleetsOf(c.g, c.ids), 'ground', c.target);
      const fc = forecastAir(c.g, a, 'ground', c.target);
      const rep = launchRaid(c.g, 'ground', c.target, c.ids, c.nat)!;
      return { fc: JSON.stringify(fc), E: a.E, rep: JSON.stringify(rep), save: saveGame(c.g), told: heard.length };
    };
    strafeWatch.tell = (x) => { heard.push(x); };
    const off = plain(() => { flyKinds([]); strafeRule(false); });
    // As on d06286d4: every fighter of the raid charged, before the air fight; no strafers named.
    const fc = JSON.parse(off.fc);
    expect(fc.fire).toBeUndefined();
    expect(fc.strafers).toBeUndefined();
    const lane = plain(() => { flyKinds(['ground']); strafeRule(false); });
    const key = plain(() => { flyKinds([]); strafeRule(true); });
    for (const x of [lane, key]) {
      expect(x.fc).toBe(off.fc);
      expect(x.rep).toBe(off.rep);
      expect(x.save).toBe(off.save);
    }
    expect(heard.length).toBe(0);
    // The lane's switch alone: the ticket carries no reach, and arms the ground fire as before.
    flyKinds(['ground']); strafeRule(false);
    const c = saar(0);
    const t = airTicket(c.g, 'ground', c.target, c.ids, c.nat).ticket!;
    expect(t.place.marks!.law.reach).toBeUndefined();
    const a = assessAir(c.g, fleetsOf(c.g, c.ids), 'ground', c.target);
    expect(t.place.forecast.fire!).toBeCloseTo(Math.min(t.place.forecast.ftr, a.E * AIRWAR.ground.fire), 9);
    // And through the environment, the key turns only with the lane's switch.
    (globalThis as { TC_STRAFE?: string }).TC_STRAFE = '1';
    try {
      expect(strafeOn()).toBe(true);
      flyKinds([]);
      expect(strafeOn()).toBe(false);
    } finally { delete (globalThis as { TC_STRAFE?: string }).TC_STRAFE; }
  });
});
