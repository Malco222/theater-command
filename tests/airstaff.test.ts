// The computer's air staff in the grand campaign, and what a raid costs there. Every computer nation at war plans
// raids of its own at every level of difficulty and flies them at once: bombers against factories and railway
// junctions by day or night, against the armies on a live front, and fighters for the sky over it. A raid's losses
// are counted nearer history's: about a fifth of the bombers that go without escort into a strong patrol, and a
// few in a hundred of those that are escorted. The historical scenarios keep the air war they always had.
import { describe, expect, it } from 'vitest';
import airwar from '../data/campaign/airwar.json';
import levels from '../data/campaign/difficulty.json';
import { AIR, AIRWAR, airAI, airFleetsOf, airRange, airWar, assessAir, cover, fly, forecastAir, km } from '../src/campaign/air';
import { bombingFor, raidsMul, setLevel } from '../src/campaign/difficulty';
import { atWar, allied, createGame } from '../src/campaign/game';
import { railsAt } from '../src/campaign/rails';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { AirFleet, AirReport, Game } from '../src/campaign/types';

const grand = (human: string | null = 'FRA', level = 'colonel', seed = 3): Game => { const g = startOpen(createGame(seed, human)); setLevel(g, level); return g; };
const R = AIRWAR.ai.raids;
/** The bombers that flew a raid on land or on shipping: strikes on a fleet or a harbour are the sea staff's, with rules of their own (carriers.test.ts). */
const bombers = (r: AirReport): number => (r.mission === 'fleet' || r.mission === 'harbour' ? 0 : r.att.reduce((s, x) => s + x.bmr, 0));
/** Plays so many months with the player giving no orders, and returns the raids flown in them. */
function months(g: Game, n: number): AirReport[] {
  const from = g.airReports.length;
  for (let m = 0; m < n; m++) {
    let guard = 0;
    while (!humanUp(g) && guard++ < 200) stepAI(g);
    if (!humanUp(g)) break;
    beginHuman(g);
    endHuman(g);
  }
  return g.airReports.slice(from);
}

/**
 * A German bomber group of 250 and a fighter group worth 500 over the target, on an air base laid out beside the
 * border, and across it a factory province of an enemy's with a patrol of 300 fighters able to meet a raid on it:
 * a strong patrol, and a strong escort.
 */
function daylight(g: Game): { b: AirFleet; escorts: AirFleet[]; target: number; patrol: AirFleet[] } {
  let best: { home: number; target: number; d: number } | null = null;
  for (const t of g.regions) {
    if (!atWar(g, 'GER', t.ctrl) || t.fac <= 0) continue;
    for (const j of t.nb) { const d = km(g, j, t.id); if (g.regions[j].ctrl === 'GER' && (!best || d < best.d)) best = { home: j, target: t.id, d }; }
  }
  if (!best) throw new Error('no daylight target');
  const b = airFleetsOf(g, 'GER').find((f) => f.kind === 'bmr')!, e = airFleetsOf(g, 'GER').find((f) => f.kind === 'ftr')!;
  const p = g.airFleets.find((f) => f.nat === g.regions[best!.target].ctrl && f.kind === 'ftr')!;
  // Nobody else flies from either airfield, and nobody else is on patrol.
  for (const f of g.airFleets) {
    if (f === b || f === e || f === p) continue;
    if (f.base === best.home || f.base === best.target) { f.ftr = 0; f.bmr = 0; }
    if (f.nat !== 'GER') f.mission = 'rest';
  }
  g.flags[`af:${best.home}`] = 3; g.flags[`af:${best.target}`] = 3;
  b.base = e.base = best.home; p.base = best.target;
  b.bmr = 250; b.ready = e.ready = p.ready = 1; p.mission = 'support';
  e.ftr = Math.round(500 / cover(best.d, airRange(e).ftr));
  p.ftr = 300;
  // Without radar only part of a patrol gets up in time: make it 300 that do.
  p.ftr = Math.round(300 / assessAir(g, [b], 'bomb', best.target).def[0].w);
  return { b, escorts: [e], target: best.target, patrol: [p] };
}

describe('the numbers', () => {
  it('say what a raid costs and how the computer plans one', () => {
    const L = airwar.losses;
    expect(L.bombers).toBeGreaterThan(0.15);
    expect(L.bombers + AIR.combat.flak).toBeLessThanOrEqual(L.most);
    expect(L.most).toBeLessThan(AIR.combat.maxBomberLoss);
    expect(L.power).toBeGreaterThan(1);
    expect(L.clash).toBeGreaterThan(0);
    expect(L.clash).toBeLessThan(1);
    expect(R.share).toBeGreaterThan(0);
    expect(R.share).toBeLessThan(0.5);
    expect(R.loss).toBeLessThan(L.bombers);
    expect(R.success).toBeGreaterThanOrEqual(0.5);
    // The levels: fewer raids from a rival on the easier ones, more and bolder on the harder, the player first.
    const byId = Object.fromEntries(levels.levels.map((l) => [l.id, l])) as Record<string, (typeof levels.levels)[number] & { bombing?: { share: number; loss: number; focus: number }; rival: { raids?: number } }>;
    expect(byId.cadet.rival.raids!).toBeLessThan(byId.captain.rival.raids!);
    expect(byId.captain.rival.raids!).toBeLessThan(1);
    expect(byId.colonel.rival.raids).toBeUndefined();
    expect(byId.general.bombing!.share).toBeGreaterThan(R.share);
    expect(byId.marshal.bombing!.share).toBeGreaterThan(byId.general.bombing!.share);
    expect(byId.general.bombing!.loss).toBeGreaterThan(R.loss);
    expect(byId.marshal.bombing!.focus).toBeGreaterThan(byId.general.bombing!.focus);
    for (const l of levels.levels) expect(l.lines.join(' '), l.id).toMatch(/raid|air/i);
  });
});

describe('what a raid costs in the grand campaign', () => {
  it('about a fifth of the bombers that go by day without escort into a strong patrol, and a few in a hundred with one', () => {
    const g = grand('GER');
    const { b, escorts, target, patrol } = daylight(g);
    const alone = assessAir(g, [b], 'bomb', target);
    expect(alone.B).toBe(250);
    expect(alone.D).toBeGreaterThan(270);
    expect(alone.D).toBeLessThan(330);
    expect(alone.bomberLoss).toBeGreaterThanOrEqual(0.15);
    expect(alone.bomberLoss).toBeLessThanOrEqual(0.25);
    const escorted = assessAir(g, [b, ...escorts], 'bomb', target);
    expect(escorted.E).toBeGreaterThan(450);
    expect(escorted.E).toBeLessThan(550);
    expect(escorted.bomberLoss).toBeGreaterThanOrEqual(0.03);
    expect(escorted.bomberLoss).toBeLessThanOrEqual(0.08);
    // By night few fighters find the bombers.
    const night = assessAir(g, [b], 'night', target);
    expect(night.bomberLoss).toBeGreaterThan(0);
    expect(night.bomberLoss).toBeLessThanOrEqual(0.05);
    expect(night.bomberLoss).toBeLessThan(alone.bomberLoss / 3);
    // With nobody up, only the guns.
    for (const f of patrol) f.mission = 'rest';
    expect(assessAir(g, [b], 'bomb', target).bomberLoss).toBeCloseTo(AIR.combat.flak, 9);
    // However strong the patrol, a raid never loses more than the most the rules allow.
    for (const f of patrol) { f.mission = 'support'; f.ftr = 5000; }
    const worst = assessAir(g, [b], 'bomb', target);
    expect(worst.bomberLoss).toBeLessThanOrEqual(AIRWAR.losses.most);
    expect(worst.bomberLoss).toBeGreaterThan(alone.bomberLoss);
  });

  it('is what the flight then costs, within the luck of the day, and the escort fights less hard than in a fight for the sky', () => {
    const g = grand('GER');
    const { b, escorts, target } = daylight(g);
    const a = assessAir(g, [b, ...escorts], 'bomb', target);
    const fc = forecastAir(g, a, 'bomb', target);
    const sky = forecastAir(g, assessAir(g, [b, ...escorts], 'superiority', target), 'superiority', target);
    expect(fc.ftrLoss).toBeGreaterThan(0);
    expect(fc.ftrLoss).toBeCloseTo(sky.ftrLoss * AIRWAR.losses.clash, 6);
    expect(fc.foeLoss).toBeCloseTo(sky.foeLoss * AIRWAR.losses.clash, 6);
    expect(fc.worth).toBeGreaterThan(0);
    const rep = fly(g, 'GER', 'bomb', target, [b, ...escorts], ':test')!;
    const lostB = rep.att.reduce((s, x) => s + x.lostBmr, 0), lostF = rep.att.reduce((s, x) => s + x.lostFtr, 0);
    expect(Math.abs(lostB - fc.bmrLoss)).toBeLessThanOrEqual(fc.bmrLoss * AIR.combat.luck + 2);
    expect(Math.abs(lostF - fc.ftrLoss)).toBeLessThanOrEqual(fc.ftrLoss * AIR.combat.luck + 2);
    expect(lostB / 250).toBeLessThanOrEqual(0.1);
  });

  it('does not touch a historical scenario, which counts its losses as it always did', () => {
    const g = createGame(3, 'GER');
    expect(airWar(g)).toBe(false);
    const C = AIR.combat;
    const b = airFleetsOf(g, 'GER').find((f) => f.bmr > 0)!;
    let checked = 0;
    for (const r of g.regions) {
      if (!atWar(g, 'GER', r.ctrl) || r.fac <= 0) continue;
      const a = assessAir(g, [b], 'bomb', r.id);
      if (a.why || a.aa) continue;
      const met = a.Dpow / Math.max(1, a.Dpow + a.Epow + a.B * C.bomberDefence);
      expect(a.bomberLoss, r.name).toBeCloseTo(Math.min(C.maxBomberLoss, C.bombers * met + C.flak), 9);
      checked++;
    }
    expect(checked).toBeGreaterThan(0);
  });
});

describe('the computer\'s air staff', () => {
  it('flies raids of its own at every level, at once, and the groups that went are away for the month', () => {
    for (const id of ['cadet', 'captain', 'colonel', 'general', 'marshal']) {
      const g = grand('FRA', id);
      expect(g.airReports.length).toBe(0);
      airAI(g, 'GER');
      const raids = g.airReports;
      expect(raids.length, id).toBeGreaterThan(0);
      for (const r of raids) {
        expect(r.attNat).toBe('GER');
        expect(r.turn).toBe(g.turn);
        expect(r.won, `${id} ${r.mission} ${r.place}`).toBe(true);
        // Always against a country Germany is at war with.
        if (r.region >= 0) expect(atWar(g, 'GER', g.regions[r.region].ctrl), r.place).toBe(true);
        for (const x of r.att) {
          const f = g.airFleets.find((y) => y.id === x.id)!;
          expect(f.flew).toBe(g.turn);
          expect(f.mission).toBe(r.mission);
          expect(f.target).toBe(r.region);
        }
      }
      // No group flies twice, and the staff does not hit one place twice in a month.
      const flew = raids.flatMap((r) => r.att.map((x) => x.id));
      expect(new Set(flew).size).toBe(flew.length);
      expect(new Set(raids.map((r) => r.region)).size).toBe(raids.length);
      // The rest stay on patrol, and a second call in the month sends nobody else out on a second raid.
      expect(airFleetsOf(g, 'GER').filter((f) => f.flew !== g.turn).every((f) => f.mission === 'support')).toBe(true);
      const n = raids.length;
      airAI(g, 'GER');
      for (const r of g.airReports.slice(n)) for (const x of r.att) expect(flew).not.toContain(x.id);
    }
  });

  it('sends out a share of its bombers each month, more of them for a rival on the harder levels and fewer on the easier', () => {
    const count = (level: string): { raids: number; onMe: number } => {
      const g = grand('FRA', level);
      const rs = months(g, 6).filter((r) => r.attNat === 'GER' && bombers(r) > 0);
      return { raids: rs.length, onMe: rs.filter((r) => r.defNat === 'FRA').length };
    };
    const cadet = count('cadet'), colonel = count('colonel'), marshal = count('marshal');
    expect(cadet.raids).toBeGreaterThan(0);
    expect(colonel.raids).toBeGreaterThan(cadet.raids);
    expect(marshal.raids).toBeGreaterThan(colonel.raids);
    // On the hardest level the player is raided first.
    expect(marshal.onMe).toBeGreaterThan(colonel.onMe);
    // Never more than its share of the bomber groups it has, to the next whole raid.
    const g = grand('FRA');
    const groups = airFleetsOf(g, 'GER').filter((f) => f.kind === 'bmr').length;
    airAI(g, 'GER');
    expect(g.airReports.filter((r) => bombers(r) > 0).length).toBeLessThanOrEqual(Math.ceil(groups * R.share));
  }, 60000);

  it('goes only where the forecast says a raid should pay, within the losses it will bear', () => {
    const g = grand('FRA');
    const rs = months(g, 8).filter((r) => bombers(r) > 0);
    expect(rs.length).toBeGreaterThan(10);
    // Several nations fly, not only the strongest.
    expect(new Set(rs.map((r) => r.attNat)).size).toBeGreaterThan(1);
    for (const r of rs) {
      const lost = r.att.reduce((s, x) => s + x.lostBmr, 0);
      expect(lost / bombers(r), `${r.attNat} ${r.mission} ${r.place}`).toBeLessThanOrEqual(R.loss * (1 + AIR.combat.luck) + 0.02);
      expect(r.att.every((x) => x.bmr === 0 || x.bmr >= R.least), r.place).toBe(true);
    }
    expect(rs.filter((r) => r.won).length / rs.length).toBeGreaterThan(0.8);
    // Factories and junctions are bombed, not open country.
    for (const r of rs) if (r.mission === 'bomb' || r.mission === 'night') { const p = g.regions[r.region]; expect(p.fac > 0 || railsAt(p.id).length >= 3, p.name).toBe(true); }
  });

  it('comes to a front only where the armies are fighting: a quiet front stays quiet in the air', () => {
    const g = grand('FRA');
    // September 1939: Germany is at war with France and Poland, and fighting only Poland.
    airAI(g, 'GER');
    const tactical = g.airReports.filter((r) => r.mission === 'ground' || r.mission === 'superiority');
    for (const r of tactical) expect(g.regions[r.region].units.some((u) => u.nat === 'FRA'), r.place).toBe(false);
    // Over the months that follow the armies in the field are raided and the sky over them fought for.
    const later = months(g, 10);
    expect(later.some((r) => r.mission === 'ground')).toBe(true);
    for (const r of later.filter((x) => x.mission === 'ground')) expect(r.result, r.place).toMatch(/organisation|little to hit/);
  });

  it('flies for the player\'s allies against the common enemy, and never against the player or a friend', () => {
    const g = grand('FRA', 'marshal');
    const rs = months(g, 6);
    const uk = rs.filter((r) => r.attNat === 'UK');
    expect(uk.length).toBeGreaterThan(0);
    for (const r of rs) {
      if (r.region < 0) continue;
      expect(allied(g, r.attNat, r.defNat), `${r.attNat} on ${r.defNat}`).toBe(false);
    }
    for (const r of uk) expect(r.defNat).not.toBe('FRA');
    // An ally of the player's is nobody's rival: it raids by the ordinary rules at any level.
    expect(bombingFor(g, 'UK')).toBeNull();
    expect(raidsMul(g, 'UK')).toBe(1);
    expect(bombingFor(g, 'GER')).not.toBeNull();
    setLevel(g, 'cadet');
    expect(raidsMul(g, 'GER')).toBe(0.5);
    expect(raidsMul(g, 'UK')).toBe(1);
    setLevel(g, 'colonel');
    expect(raidsMul(g, 'GER')).toBe(1);
  });

  it('flies nothing for a nation at peace, for the player, or in a historical scenario', () => {
    const g = grand('FRA');
    airAI(g, 'USA');
    airAI(g, 'FRA');
    expect(g.airReports.length).toBe(0);
    expect(airFleetsOf(g, 'FRA').every((f) => f.flew === undefined)).toBe(true);
    // History's own air plan gives standing orders, flown when the month's orders are all in.
    const h = createGame(3, 'FRA');
    airAI(h, 'GER');
    expect(h.airReports.length).toBe(0);
    expect(airFleetsOf(h, 'GER').every((f) => f.flew === undefined)).toBe(true);
  });

  it('plays the same war twice from the same seed', () => {
    const a = months(grand('FRA', 'general', 11), 4), b = months(grand('FRA', 'general', 11), 4);
    expect(a.length).toBeGreaterThan(0);
    expect(a.map((r) => `${r.turn}:${r.attNat}:${r.mission}:${r.region}:${r.att.map((x) => x.lostBmr + x.lostFtr).join(',')}`)).toEqual(b.map((r) => `${r.turn}:${r.attNat}:${r.mission}:${r.region}:${r.att.map((x) => x.lostBmr + x.lostFtr).join(',')}`));
  });
});
