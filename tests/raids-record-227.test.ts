// Ruling 227: day raids priced by the record. The ruling's set raids, on the fresh map of the grand campaign with only
// the date and the bases moved: what the raid card shows (planRaid) and what fly() charges on average. And the dated
// rows the historian gave: the bombers' own defence, day accuracy, and how far fighters reach.
import { describe, expect, it } from 'vitest';
import { AIR, AIRWAR, airRange, airWeight, assessAir, bomberDefence, dayAim, fly, kmTo, troopCut, troopWeight } from '../src/campaign/air';
import { planRaid } from '../src/campaign/airwar';
import { createGame, setWar } from '../src/campaign/game';
import { runTurnAI } from '../src/campaign/turn';
import { startOpen } from '../src/campaign/staff';
import type { AirFleet, AirMission, Corps, Game } from '../src/campaign/types';

const fresh = (): Game => startOpen(createGame(7, null));
const grp = (g: Game, nat: string, name: string): AirFleet => g.airFleets.find((f) => f.nat === nat && f.name === name)!;
const id = (g: Game, n: string): number => g.byName[n].id;

interface Raid { success: number; card: number; loss: number; defLost: number; perBomber: number }
/** The card, and fly() flown N times on copies of the same month. */
function raid(g: Game, m: AirMission, target: number, ids: number[], nat: string, N = 40): Raid {
  const p = planRaid(g, m as any, target, ids, nat);
  expect(p.why).toBeFalsy();
  let sent = 0, lost = 0, def = 0;
  for (let k = 0; k < N; k++) {
    const c = structuredClone(g);
    const rep = fly(c, nat, m, target, c.airFleets.filter((f) => p.picked.includes(f.id)), `:t227:${k}`)!;
    sent += rep.att.reduce((s, x) => s + x.bmr, 0);
    lost += rep.att.reduce((s, x) => s + x.lostBmr, 0);
    def += rep.def.reduce((s, x) => s + x.lostFtr, 0);
  }
  return { success: p.success, card: p.bomberLoss, loss: lost / sent, defLost: def / N, perBomber: def > 0 ? lost / def : Infinity };
}

describe('ruling 227: the set raids land inside the record\'s bands', () => {
  it('the RAF on Bremen in 1939, 280 unescorted: a quarter to well over half lost, a fighter for every 5 or 6, under 2 in 10', () => {
    const g = fresh();
    g.turn = 3;
    const r = raid(g, 'bomb', id(g, 'Bremen'), [grp(g, 'UK', 'No. 1 Group').id, grp(g, 'UK', 'No. 2 Group').id], 'UK');
    expect(r.card).toBeGreaterThanOrEqual(0.25);
    expect(r.card).toBeLessThanOrEqual(0.6);
    expect(Math.abs(r.loss - r.card)).toBeLessThan(0.02);
    expect(r.perBomber).toBeGreaterThanOrEqual(4.5);
    expect(r.perBomber).toBeLessThanOrEqual(6.5);
    expect(r.success).toBeLessThan(0.2);
  });

  it('Luftflotte 5 on 15 August 1940, as two raids: the escorted raid loses less, some fighters lost, under half a chance', () => {
    // The historian's record (historian/air-on-armies-b422.md, section 1): the He 111s came from Stavanger with the
    // Bf 110s of I./ZG 76, weighed at about half a single-seat fighter, and met five squadrons of No. 13 Group; the
    // Ju 88s came from Aalborg alone and met two or three squadrons. 14 to 16 of 113 to 115 bombers lost, 12 to 14%.
    const north = (): Raid => {
      const g = fresh();
      g.turn = 12;
      for (const n of ['Oslo', 'Sorlandet', 'Bergen']) g.byName[n].ctrl = 'GER';
      g.flags[`af:${id(g, 'Sorlandet')}`] = 3;
      const he = grp(g, 'GER', '5. Fliegerdivision');
      he.base = id(g, 'Sorlandet');
      he.bmr = 64;
      // 27 Bf 110s with belly tanks, at half weight: their own reach, as a long-range group's row in air.json gives it.
      const zg = { ...structuredClone(grp(g, 'GER', '1. Jagddivision')), id: Math.max(...g.airFleets.map((f) => f.id)) + 1, name: 'I./ZG 76 (test)', base: he.base, ftr: 14, bmr: 0 } as AirFleet;
      delete (zg as { parent?: string }).parent;
      g.airFleets.push(zg);
      // Five squadrons of No. 13 Group come up.
      grp(g, 'UK', 'No. 13 Group').ftr = 60;
      AIR.fleets.push({ nat: 'GER', name: zg.name, base: 'Sorlandet', ftrRange: 1200 } as (typeof AIR.fleets)[number]);
      try {
        expect(airRange(zg, g).ftr).toBe(1200);
        return raid(g, 'bomb', id(g, 'Northumbria'), [he.id, zg.id], 'GER');
      } finally {
        AIR.fleets.pop();
      }
    };
    const south = (): Raid => {
      const g = fresh();
      g.turn = 12;
      g.byName.Jutland.ctrl = 'GER';
      g.flags[`af:${id(g, 'Jutland')}`] = 3;
      const ju = grp(g, 'GER', '3. Fliegerdivision');
      ju.base = id(g, 'Jutland');
      ju.bmr = 50;
      // Two or three squadrons meet it: No. 13 Group's, with No. 12 Group's held in the south.
      grp(g, 'UK', 'No. 13 Group').ftr = 30;
      grp(g, 'UK', 'No. 12 Group').mission = 'superiority';
      return raid(g, 'bomb', id(g, 'Yorkshire'), [ju.id], 'GER');
    };
    const n = north(), s = south();
    const card = (n.card * 64 + s.card * 50) / 114, flown = (n.loss * 64 + s.loss * 50) / 114;
    // (On the cities version with ruling 225 under it: north 27%, south 52%, together 38%.)
    expect(n.defLost + s.defLost).toBeGreaterThan(0);
    expect(n.success).toBeLessThan(0.5);
    expect(s.success).toBeLessThan(0.5);
    // Out of the record's band, accepted with this note (the design director, b422 item 4): together the two raids lose
    // about 38%, the Ju 88s about half. The game meets each raid with every fighter in reach; on the day No. 13 Group
    // split some seven squadrons between two raids 150 miles apart, and the Ju 88s, the fastest bombers, met the fewest
    // fighters for each bomber. That term (the share of the fighters in reach that meets each raid) is not in the
    // ruling. The record is the floor, and a guard at 45% keeps the curve from drifting further.
    expect(card).toBeGreaterThanOrEqual(0.12);
    expect(card).toBeLessThanOrEqual(0.45);
    expect(n.card).toBeLessThan(s.card);
    expect(Math.abs(flown - card)).toBeLessThan(0.03);
  });

  it('American heavies on Schweinfurt, October 1943, unescorted: 18 to 30% lost, a fighter for every 1.5 to 2.5, about even or below', () => {
    const g = fresh();
    g.turn = 50;
    setWar(g, 'USA', 'GER');
    const b = grp(g, 'USA', 'I Bomber Command');
    b.base = id(g, 'East Anglia');
    b.bmr = 291;
    const r = raid(g, 'bomb', id(g, 'Franconia'), [b.id], 'USA');
    expect(r.card).toBeGreaterThanOrEqual(0.18);
    expect(r.card).toBeLessThanOrEqual(0.3);
    expect(r.perBomber).toBeGreaterThanOrEqual(1.5);
    expect(r.perBomber).toBeLessThanOrEqual(2.5);
    expect(r.success).toBeLessThanOrEqual(0.55);
  });

  it('American heavies on the Ruhr in 1944, escorted all the way: 4 to 8% lost, many fighters lost to the escort, a high chance', () => {
    const g = fresh();
    g.turn = 58;
    setWar(g, 'USA', 'GER');
    const b = grp(g, 'USA', 'I Bomber Command'), f = grp(g, 'USA', 'First Air Force');
    b.base = f.base = id(g, 'East Anglia');
    b.bmr = 291;
    f.ftr = 400;
    // Bare ground: guns cleared; the band is the record's total including flak. (1.5.1, the design director, 24 Nov 2026
    // 22:58Z: the band was met with no guns at all, so once the Ruhr has guns, from the start or built by 1944, escorted
    // heavy raids over a defended city run above the record: 0.082 at the start guns' level 1, 0.094 at level 2. The
    // start guns expose this; they don't cause it. A named finding, traced in release 2.)
    delete g.flags[`wk:aa:${id(g, 'Ruhr')}`];
    // The escort reaches the Ruhr from England: the dated range, not a fleet's own.
    expect(airRange(f, g).ftr).toBeGreaterThan(452);
    const a = assessAir(g, [b, f], 'bomb', id(g, 'Ruhr'));
    expect(a.E).toBeGreaterThan(300);
    const r = raid(g, 'bomb', id(g, 'Ruhr'), [b.id, f.id], 'USA');
    expect(r.card).toBeGreaterThanOrEqual(0.04);
    expect(r.card).toBeLessThanOrEqual(0.08);
    expect(r.defLost).toBeGreaterThan(50);
    expect(r.success).toBeGreaterThan(0.9);
  });
});

describe('ruling 227: the dated rows', () => {
  it('reads the bombers\' own defence by nation and month, and leaves the night as it was', () => {
    const g = fresh();
    expect(bomberDefence(g, 'UK')).toBe(0.35);
    expect(bomberDefence(g, 'POL')).toBe(0.25);
    expect(bomberDefence(g, 'UK', true)).toBe(0.25);
    g.turn = 44;
    expect(bomberDefence(g, 'USA')).toBe(1);
    // The historical campaigns keep the old flat figure.
    const h = createGame(7, null);
    h.turn = 44;
    expect(bomberDefence(h, 'USA')).toBe(0.25);
  });

  it('hits factories and railways less by day early in the war, by nation, and full weight from 1943 for the Americans; a besieged town at full weight', () => {
    const g = fresh();
    expect(dayAim(g, 'UK', 'bomb')).toBe(0.3);
    expect(dayAim(g, 'GER', 'bomb')).toBe(0.7);
    expect(dayAim(g, 'UK', 'strike')).toBe(1);
    // A town under siege is bombarded at full weight: Warsaw in 1939.
    expect(dayAim(g, 'GER', 'town')).toBe(1);
    g.turn = 47;
    expect(dayAim(g, 'USA', 'bomb')).toBe(1);
    expect(dayAim(createGame(7, null), 'UK', 'bomb')).toBe(1);
  });

  it('lets escorts reach further as the war goes on, and a fleet\'s own longer reach stands', () => {
    const g = fresh();
    const usa = grp(g, 'USA', 'First Air Force');
    expect(airRange(usa, g).ftr).toBe(300);
    g.turn = 52;
    expect(airRange(usa, g).ftr).toBe(750);
    g.turn = 58;
    expect(airRange(usa, g).ftr).toBe(1200);
    // A dated row lengthens a nation's reach and never shortens it: the Bf 109 keeps today's 200 km until the drop
    // tanks of 1941 take it to 300.
    const ger = grp(g, 'GER', '1. Jagddivision');
    g.turn = 1;
    expect(airRange(ger, g).ftr).toBe(200);
    g.turn = 22;
    expect(airRange(ger, g).ftr).toBe(300);
    // Outside the air war, the nation's fixed range.
    expect(airRange(ger).ftr).toBe(200);
  });

  it('weighs stopped output by the province\'s own factories', () => {
    expect(AIRWAR.success.factory).toBeGreaterThan(0);
    const g = fresh();
    g.turn = 58;
    setWar(g, 'USA', 'GER');
    const b = grp(g, 'USA', 'I Bomber Command');
    b.base = id(g, 'East Anglia');
    b.bmr = 291;
    // The same raid is worth more on the Ruhr's works than on a province of one factory.
    const ruhr = planRaid(g, 'bomb', id(g, 'Ruhr'), [b.id], 'USA'), bremen = planRaid(g, 'bomb', id(g, 'Bremen'), [b.id], 'USA');
    expect(g.byName.Ruhr.fac).toBeGreaterThan(g.byName.Bremen.fac);
    expect(ruhr.success).toBeGreaterThan(bremen.success);
  });
});

describe('ruling 227 (b424, b428): a raid on troops priced by the record', () => {
  // One raid on troops is a month's raiding on the troops in one province. The share of their men and guns it costs:
  const men = (g: Game, weight: number, divs: number): number => troopCut(g.byName.Zhitomir, [{ divs } as Corps], weight) * AIRWAR.ground.str;

  it('costs an army 0.1 to 0.3% a month in 1942 and 1943, and 1 to 2% under a clear sky; never more than 2%', () => {
    const g = fresh();
    expect(g.byName.Zhitomir.terrain).toBe('plains');
    // A German raid of the usual weight on twelve divisions in the open (240: the median the staffs flew in twelve free wars).
    expect(men(g, 240, 12)).toBeGreaterThanOrEqual(0.001);
    expect(men(g, 240, 12)).toBeLessThanOrEqual(0.003);
    // The same raid by the Soviet air force in 1942, at its weight beside the Luftwaffe.
    g.turn = 40;
    expect(troopWeight(g, 'SOV')).toBe(0.4);
    expect(men(g, 240 * troopWeight(g, 'SOV'), 12)).toBeGreaterThanOrEqual(0.001);
    expect(men(g, 240 * troopWeight(g, 'SOV'), 12)).toBeLessThanOrEqual(0.003);
    // 600 bombers and 1,000 fighters on four divisions falling back, with nobody to meet them: 1 to 2%.
    expect(men(g, 600 + 1000 * AIRWAR.ground.fighters, 4)).toBeGreaterThanOrEqual(0.01);
    expect(men(g, 600 + 1000 * AIRWAR.ground.fighters, 4)).toBeLessThanOrEqual(0.02);
    expect(men(g, 1e6, 1)).toBeCloseTo(0.02, 6);
    // The organisation cut is twice the share of men lost: about the same fighting power for a month.
    expect(AIRWAR.ground.str).toBe(0.5);
  });

  it('a German raid on a Soviet army in July 1941, flown: well under 1% of its men, its organisation cut by twice that', () => {
    const g = fresh();
    g.turn = 23;
    setWar(g, 'GER', 'SOV');
    const from = g.byName.Volhynia, at = g.byName.Zhitomir;
    from.ctrl = 'GER';
    g.flags[`af:${from.id}`] = 3;
    const b = grp(g, 'GER', '1. Fliegerdivision'), f = grp(g, 'GER', 'Jagdgeschwader 1');
    b.base = f.base = from.id;
    const p = planRaid(g, 'ground', at.id, [b.id, f.id], 'GER');
    expect(p.why).toBeFalsy();
    const strength = (h: Game): number => h.regions[at.id].units.filter((u) => u.nat === 'SOV' && !u.dead).reduce((s, u) => s + u.divs * u.str, 0);
    const org = (h: Game): number => h.regions[at.id].units.filter((u) => u.nat === 'SOV' && !u.dead && !u.dug).reduce((s, u) => s + u.org, 0);
    let lost = 0, cut = 0;
    for (let k = 0; k < 10; k++) {
      const c = structuredClone(g);
      const before = strength(c), o = org(c), n = c.regions[at.id].units.filter((u) => u.nat === 'SOV' && !u.dead && !u.dug).length;
      expect(fly(c, 'GER', 'ground', at.id, c.airFleets.filter((x) => p.picked.includes(x.id)), `:t227g:${k}`)).toBeTruthy();
      lost += (before - strength(c)) / before / 10;
      if (n) cut += (o - org(c)) / n / 10;
    }
    expect(lost).toBeGreaterThan(0);
    expect(lost).toBeLessThan(0.01);
    if (cut > 0) expect(cut / lost).toBeGreaterThan(1.5);
  });

  it('weighs the Soviet air force by the historian\'s row, the Luftwaffe as 1, in the grand campaign only', () => {
    const g = fresh();
    expect(airWeight(g, 'GER')).toBeNull();
    expect(airWeight(g, 'SOV')).toBe(0.3);
    expect(troopWeight(g, 'GER')).toBe(1);
    for (const [t, w] of [[22, 0.3], [28, 0.35], [37, 0.4], [44, 0.5], [47, 0.55], [53, 0.65], [58, 0.7], [70, 0.7]]) {
      g.turn = t;
      expect(airWeight(g, 'SOV')).toBe(w);
    }
    expect(airWeight(createGame(7, null), 'SOV')).toBeNull();
  });

  it('bases a staff\'s air by the threat and its main effort (b428, b447, b450): half of Germany\'s operational groups or more in reach of the eastern front in 1941 and 1942, and the homeland still guarded', () => {
    const g = startOpen(createGame(10, null));
    const front = (): number[] => g.regions.filter((r) => (r.ctrl === 'GER' && r.nb.some((j) => g.regions[j].ctrl === 'SOV')) || (r.ctrl === 'SOV' && r.nb.some((j) => g.regions[j].ctrl === 'GER'))).map((r) => r.id);
    let months = 0;
    while (g.turn <= 40) {
      runTurnAI(g);
      if (g.turn < 25 || !g.nations.GER.alive || !g.nations.GER.atWar.includes('SOV')) continue;
      // Every land group that flies, fighters by a fighter's reach and bombers by a bomber's: its bombers stand in
      // reach of the battles of its main effort, its fighters by the threat with the effort's troops asking for the sky.
      const groups = g.airFleets.filter((f) => f.nat === 'GER' && f.ftr + f.bmr > 0 && f.ship === undefined);
      const east = front();
      const near = groups.filter((f) => east.some((r) => kmTo(g, f, r) <= (f.kind === 'ftr' ? airRange(f, g).ftr : airRange(f, g).bmr)));
      expect(near.length * 2).toBeGreaterThanOrEqual(groups.length);
      // Home defence is kept for the raids the staff expects: a fighter group still guards the Ruhr's works.
      expect(groups.some((f) => f.kind === 'ftr' && kmTo(g, f, g.byName.Ruhr.id) <= airRange(f, g).ftr)).toBe(true);
      months++;
    }
    expect(months).toBeGreaterThanOrEqual(12);
  }, 120_000);
});
