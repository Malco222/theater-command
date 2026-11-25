// Patch 1.5.2 (the playtester's finding 2; the design director, 25 Nov 04:24Z, section #2): each bombing raid on a
// province gets its own throw for the railway and for the factories, keyed on the raid's own number: how many bombing
// raids (by day or by night) the same side has flown on that province this month already. The first raid keeps the
// throw it always had; a second and a third throw afresh, so the chance the raid card shows is true for every raid.
// A raid fought out as a 3D battle is still decided by the field's own throws.
import { describe, expect, it, vi } from 'vitest';
import { Rng, hashString } from '../src/core/rng';
import { createGame } from '../src/campaign/game';
import { RAILWAYS, bombFactories, bombRail, mendRail, railBombChance, railCut, rebuildFactory, ruinChance, ruinedAt } from '../src/campaign/rails';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, orderRaid, previewRaid, raidTargetsFor, stepAI } from '../src/campaign/turn';
import type { AirMission, AirReport, Game, Region } from '../src/campaign/types';

// Every key the rules hash is noted, so the end-to-end test can see which throws the raids used. Nothing else changes.
const keys = vi.hoisted(() => ({ seen: [] as string[] }));
vi.mock('../src/core/rng', async (load) => {
  const real = await load<typeof import('../src/core/rng')>();
  return { ...real, hashString: (s: string): number => { keys.seen.push(s); return real.hashString(s); } };
});

const grand = (seed = 3, human: string | null = null): Game => startOpen(createGame(seed, human));
const with_ = <T>(table: Record<string, number>, key: string, value: number, run: () => T): T => { const was = table[key]; table[key] = value; try { return run(); } finally { table[key] = was; } };
const BOMB = RAILWAYS.bomb as unknown as Record<string, number>, RUIN = RAILWAYS.ruin as unknown as Record<string, number>;
const today = (what: 'railbomb' | 'ruin', g: Game, target: number, by: string): number => new Rng(hashString(`${what}:${g.seed}:${g.turn}:${target}:${by}`)).next();

/** Files a report of a raid as fly() files it, with only the fields the count reads. */
function flown(g: Game, by: string, target: number, mission: AirMission = 'night', turn = g.turn): void {
  g.airReports.push({ id: g.airReports.length + 1, turn, mission, region: target, zone: -1, attNat: by, defNat: g.regions[target].ctrl } as unknown as AirReport);
}

/** The number bombRail throws for the railway now, found by halving: the chance is set to x, and the line is relaid after each try. */
function railThrow(g: Game, by: string, target: number): number {
  return with_(BOMB, 'scale', 1, () => with_(BOMB, 'most', 1, () => {
    let lo = 0, hi = 1;
    for (let i = 0; i < 40; i++) {
      const x = (lo + hi) / 2;
      if (bombRail(g, by, target, x)) { hi = x; mendRail(g, target); } else lo = x;
    }
    return (lo + hi) / 2;
  }));
}

/** The number bombFactories throws now, the same way (the chance is capped at 0.95: a throw above it reads as 0.95). */
function ruinThrow(g: Game, by: string, r: Region): number {
  return with_(RUIN, 'chance', 1, () => with_(RUIN, 'over', 0, () => {
    let lo = 0, hi = 0.95;
    if (!bombFactories(g, by, r, hi)) return hi;
    rebuildFactory(g, r);
    for (let i = 0; i < 40; i++) {
      const x = (lo + hi) / 2;
      if (bombFactories(g, by, r, x)) { hi = x; rebuildFactory(g, r); } else lo = x;
    }
    return (lo + hi) / 2;
  }));
}

describe('a raid on a province throws its own dice', () => {
  it('the first raid on a province in a month throws exactly what it throws today', () => {
    const g = grand();
    const hamburg = g.byName['Hamburg'], paris = g.byName['Paris'], berlin = g.byName['Berlin'];
    expect(railBombChance(g, hamburg.id, BOMB.scale)).toBeGreaterThan(0);
    // Other raids leave it alone: another side's on the province, one of its own on another, last month's, and
    // a raid there that was not a bombing raid.
    flown(g, 'GER', hamburg.id);
    flown(g, 'UK', berlin.id);
    flown(g, 'UK', hamburg.id, 'night', g.turn - 1);
    flown(g, 'UK', hamburg.id, 'town');
    flown(g, 'UK', hamburg.id, 'superiority');
    flown(g, 'GER', paris.id, 'bomb', g.turn - 1);
    flown(g, 'UK', paris.id, 'ground');
    for (let s = 1; s <= 12; s++) {
      g.seed = s;
      expect(railThrow(g, 'UK', hamburg.id)).toBeCloseTo(today('railbomb', g, hamburg.id, 'UK'), 9);
      expect(ruinThrow(g, 'GER', paris)).toBeCloseTo(Math.min(0.95, today('ruin', g, paris.id, 'GER')), 9);
    }
    expect(railCut(g, hamburg.id)).toBe(false);
    expect(ruinedAt(g, paris)).toBe(0);
  });

  it('a second and a third raid the same month throw numbers different from the first and from each other', () => {
    const g = grand();
    const hamburg = g.byName['Hamburg'], paris = g.byName['Paris'];
    for (let s = 1; s <= 12; s++) {
      g.seed = s;
      g.airReports = [];
      const rail = [railThrow(g, 'UK', hamburg.id)], ruin = [ruinThrow(g, 'GER', paris)];
      flown(g, 'UK', hamburg.id, 'night');
      flown(g, 'GER', paris.id, 'bomb');
      rail.push(railThrow(g, 'UK', hamburg.id)); ruin.push(ruinThrow(g, 'GER', paris));
      // By day or by night, a bombing raid counts.
      flown(g, 'UK', hamburg.id, 'bomb');
      flown(g, 'GER', paris.id, 'night');
      rail.push(railThrow(g, 'UK', hamburg.id)); ruin.push(ruinThrow(g, 'GER', paris));
      expect(new Set(rail.map((x) => x.toFixed(9))).size, `seed ${s}: ${rail}`).toBe(3);
      // A throw above 0.95 cannot be told from another one above it: those are left out.
      const low = ruin.filter((x) => x < 0.95);
      expect(new Set(low.map((x) => x.toFixed(9))).size, `seed ${s}: ${ruin}`).toBe(low.length);
    }
  });

  it('the raid card is true for a second raid: it cuts the line and wrecks a factory as often as its chance says', () => {
    const g = grand();
    const hamburg = g.byName['Hamburg'], paris = g.byName['Paris'];
    const through = BOMB.scale / 2, stopped = 0.25;
    const pRail = railBombChance(g, hamburg.id, through), pRuin = ruinChance(g, paris, stopped);
    expect(pRail).toBeGreaterThan(0.2);
    expect(pRuin).toBeGreaterThan(0.2);
    const N = 3000;
    const band = (p: number, n: number): [number, number] => { const sd = Math.sqrt((p * (1 - p)) / n); return [p - 4.5 * sd, p + 4.5 * sd]; };
    let railAll = 0, railAfterMiss = 0, misses = 0, ruinAll = 0, ruinAfterMiss = 0, ruinMisses = 0, railAfterHit = 0, hits = 0, ruinAfterHit = 0, ruinHits = 0;
    for (let s = 1; s <= N; s++) {
      g.seed = s;
      g.airReports = [];
      // The first raid, as it falls today.
      const first = bombRail(g, 'UK', hamburg.id, through);
      if (first) mendRail(g, hamburg.id);
      const wrecked = bombFactories(g, 'UK', paris, stopped);
      if (wrecked) rebuildFactory(g, paris);
      flown(g, 'UK', hamburg.id, 'night');
      flown(g, 'UK', paris.id, 'night');
      // The second, with the same chance.
      const second = bombRail(g, 'UK', hamburg.id, through);
      if (second) mendRail(g, hamburg.id);
      const again = bombFactories(g, 'UK', paris, stopped);
      if (again) rebuildFactory(g, paris);
      railAll += +second; ruinAll += +again;
      if (first) { hits++; railAfterHit += +second; } else { misses++; railAfterMiss += +second; }
      if (wrecked) { ruinHits++; ruinAfterHit += +again; } else { ruinMisses++; ruinAfterMiss += +again; }
    }
    const inside = (k: number, n: number, p: number): void => { const [lo, hi] = band(p, n); expect(k / n, `${k} of ${n} against ${p.toFixed(3)}`).toBeGreaterThanOrEqual(lo); expect(k / n).toBeLessThanOrEqual(hi); };
    inside(railAll, N, pRail);
    inside(ruinAll, N, pRuin);
    // And whatever the first raid did: after a first that failed to cut the line a second can, and after one that
    // wrecked a factory the next is not sure to.
    inside(railAfterMiss, misses, pRail);
    inside(railAfterHit, hits, pRail);
    inside(ruinAfterMiss, ruinMisses, pRuin);
    inside(ruinAfterHit, ruinHits, pRuin);
  });

  it('the playtester\'s raids: two groups by night on one German province the same month do not share their throws', () => {
    const g = grand(7, 'UK');
    let guard = 0;
    while (!humanUp(g) && guard++ < 200) stepAI(g);
    expect(humanUp(g)).toBe(true);
    beginHuman(g);
    // A province on the railway with factories, which two of the player's bomber groups can each reach by night.
    let pick: { t: number; a: number; b: number } | null = null;
    for (const t of raidTargetsFor(g, 'night')) {
      const r = g.regions[t.id];
      if (r.ctrl !== 'GER' || r.fac <= 0 || railBombChance(g, r.id, 1) <= 0) continue;
      const seats = previewRaid(g, 'night', t.id).seats.filter((s) => s.kind === 'bmr' && !s.why).map((s) => s.id);
      if (seats.length >= 2) { pick = { t: t.id, a: seats[0], b: seats[1] }; break; }
    }
    expect(pick, 'a German province two groups can raid by night').not.toBeNull();
    const { t, a, b } = pick!;
    // The line is all but sure to stand, so that both raids throw for it; neither wrecks a factory for certain.
    const throws = (): string[] => keys.seen.filter((k) => k.startsWith(`railbomb:${g.seed}:${g.turn}:${t}:UK`));
    const reps = with_(BOMB, 'most', 1e-6, () => {
      keys.seen.length = 0;
      const one = orderRaid(g, 'night', t, [a]);
      const first = throws();
      const two = orderRaid(g, 'night', t, [b]);
      return { one, two, first, both: throws() };
    });
    expect(reps.one).not.toBeNull();
    expect(reps.two).not.toBeNull();
    expect(reps.one!.region).toBe(t);
    expect(reps.two!.region).toBe(t);
    expect(reps.first).toHaveLength(1);
    expect(reps.both).toHaveLength(2);
    // The first throws as today; the second throws its own.
    expect(reps.both[0]).toBe(`railbomb:${g.seed}:${g.turn}:${t}:UK`);
    expect(reps.both[1]).not.toBe(reps.both[0]);
    expect(new Rng(hashString(reps.both[1])).next()).not.toBe(new Rng(hashString(reps.both[0])).next());
    expect(railCut(g, t)).toBe(false);
  });

  it('a raid fought out as a battle is decided by the field\'s own throws, whatever its number', () => {
    const g = grand();
    const hamburg = g.byName['Hamburg'], paris = g.byName['Paris'];
    const through = BOMB.scale / 2, stopped = 0.25;
    flown(g, 'UK', hamburg.id);
    flown(g, 'UK', hamburg.id, 'bomb');
    flown(g, 'UK', paris.id);
    for (let s = 1; s <= 20; s++) {
      g.seed = s;
      expect(bombRail(g, 'UK', hamburg.id, through, 0, undefined, 0.999)).toBe(false);
      expect(bombFactories(g, 'UK', paris, stopped, 0.999)).toBe(false);
    }
    expect(bombRail(g, 'UK', hamburg.id, through, 0, undefined, 0)).toBe(true);
    expect(bombFactories(g, 'UK', paris, stopped, 0)).toBe(true);
    expect(ruinedAt(g, paris)).toBe(1);
  });
});
