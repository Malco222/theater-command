// AHEAD OF THE RULES: ruling 214, THE LAND PAYS BY ITS WORTH (the design director, 2026-11-17 16:55Z and 16:57Z, on the
// income table of ruling 213(a): /mnt/project-files/map-tables/income-yardstick.md; the factors as data,
// /mnt/project-files/map-tables/land-factor-214.json). With the cities version, measured whole.
//
// (a) Sixteen smaller powers take a factor on their land AND harbour pay together (provinceYield's land and port
//     lines): Mongolia 0.02, Saudi Arabia 0.04, Australia 0.10, Iran 0.15, Brazil 0.16, South Africa 0.17, Mexico 0.17,
//     Peru 0.17, Venezuela 0.18, Chile 0.18, Canada 0.20, New Zealand 0.21, Portugal 0.21, Manchukuo 0.40,
//     Argentina 0.47, the Netherlands 0.60. One factor a country, home and colonies alike. Every other country is 1.
// (b) THE GROUND, NOT THE FLAG: a listed country's first-day provinces pay by its factor whoever holds them; land it
//     takes pays by that land's own factor. So no great power's first day moves (par).
// (c) On pay only: the capital's 200, oil, factories, trade and the abilities are untouched (and victory points,
//     supply and men, which are not pay).
// (e) The grand campaign only.
//
// The rules builder's shape (17:16Z): landFactor(r) beside provinceYield in economy.ts, keyed by the province's
// first-day owner from the map's own data. Every test here waits, skipped, until economy.ts exports landFactor.
//
// Not tested here: the ruled first-day totals (Australia 2,247 to 872, Canada 2,417 to 1,628 and the rest). They were
// measured on the far-side seats' rig (seed 7, chart df440fc), not on main, and Canada's war-work climb moves its own.
import { describe, expect, it } from 'vitest';
import * as economy from '../../src/campaign/economy';
import { BUDGET, landShare, provinceYield } from '../../src/campaign/economy';
import { capture, createGame } from '../../src/campaign/game';
import { majorsOf } from '../../src/campaign/diplomacy';
import { startOpen } from '../../src/campaign/staff';
import type { Game, Region } from '../../src/campaign/types';

type Factor = ((r: Region) => number) | ((g: Game, r: Region) => number);
const landFactorFn = (economy as unknown as Record<string, unknown>).landFactor as Factor | undefined;
const waiting = typeof landFactorFn !== 'function';
/** The factor on a province's land and harbour pay, whichever of the two shapes the rules builder gives it. */
const factorOf = (g: Game, r: Region): number => (landFactorFn!.length >= 2 ? (landFactorFn as (g: Game, r: Region) => number)(g, r) : (landFactorFn as (r: Region) => number)(r));

/** Ruling 214(a), as completed at 16:57Z. */
const RULED: Record<string, number> = {
  MGL: 0.02, SAU: 0.04, AUS: 0.1, IRN: 0.15, BRA: 0.16, SAF: 0.17, MEX: 0.17, PER: 0.17, VEN: 0.18, CHL: 0.18,
  CAN: 0.2, NZL: 0.21, POR: 0.21, MAN: 0.4, ARG: 0.47, NED: 0.6,
};

function grand(seed = 3, human = 'GER'): Game {
  return startOpen(createGame(seed, human));
}
/**
 * What a province's land and harbour lines paid before ruling 214: today's provinceYield (economy.ts) with no factor,
 * for a province at peace with no siege and no blocked harbour, as on the first day. If the cities version changes
 * how a city and its parent share the parent's land pay (ruling 210(e)), mend this one helper to match.
 */
function basePay(g: Game, r: Region, nat: string): { land: number; port: number } {
  const own = r.owner === nat;
  const k = !own ? BUDGET.occupied : r.overseas ? BUDGET.overseas : 1;
  // (A city and its province share the province's land pay, ruling 210(a); the sixteen's factor stands in for it.)
  return { land: BUDGET.land[r.terrain] * k * landShare(r), port: r.port ? BUDGET.port * k : 0 };
}
const close = (a: number, b: number): boolean => Math.abs(a - b) <= 1e-9 * Math.max(1, Math.abs(b));

describe.skipIf(waiting)('ruling 214 the land pays by its worth: the sixteen factors, keyed to the ground', () => {
  it('every first-day province of a listed country carries its ruled factor; every other province carries 1', () => {
    const g = grand();
    let listed = 0;
    for (const r of g.regions) {
      const want = RULED[r.owner] ?? 1;
      expect(factorOf(g, r), `${r.name} (${r.owner})`).toBe(want);
      if (r.owner in RULED) listed++;
    }
    // Main carries ten of the sixteen today (Brazil, Mexico, Peru, Venezuela, Chile, Canada, Portugal, Argentina, the
    // Netherlands, Iran); the far side brings the rest.
    expect(listed).toBeGreaterThan(0);
  });

  it('no great power\'s first day moves: all their ground pays at 1 (par)', () => {
    const g = grand();
    const majors = majorsOf(g);
    expect(majors).toEqual(expect.arrayContaining(['GER', 'ITA', 'UK', 'FRA', 'SOV', 'USA']));
    for (const r of g.regions) if (majors.includes(r.owner)) expect(factorOf(g, r), r.name).toBe(1);
  });

  it('the factor multiplies the land and harbour lines together, and nothing else (214(a), (c))', () => {
    const g = grand();
    let seen = 0;
    for (const r of g.regions) {
      if (r.ctrl !== r.owner || !(r.owner in RULED)) continue;
      const y = provinceYield(g, r, r.owner);
      const base = basePay(g, r, r.owner);
      const f = RULED[r.owner];
      expect(close(y.land, base.land * f), `${r.name} land ${y.land} against ${base.land} times ${f}`).toBe(true);
      expect(close(y.port, base.port * f), `${r.name} harbour ${y.port} against ${base.port} times ${f}`).toBe(true);
      // The capital's pay, oil and the factories are untouched.
      expect(y.capital, r.name).toBe(g.nations[r.owner].capital === r.id ? BUDGET.capital : 0);
      expect(y.oil, r.name).toBe(r.oil ? BUDGET.oil * (r.overseas ? BUDGET.overseas : 1) : 0);
      expect(y.fac, r.name).toBe(r.fac);
      seen++;
    }
    expect(seen).toBeGreaterThan(0);
  });

  it('every other country\'s land pays as before', () => {
    const g = grand();
    for (const r of g.regions) {
      if (r.ctrl !== r.owner || r.owner in RULED) continue;
      const y = provinceYield(g, r, r.owner);
      const base = basePay(g, r, r.owner);
      expect(close(y.land, base.land), r.name).toBe(true);
      expect(close(y.port, base.port), r.name).toBe(true);
    }
  });
});

describe.skipIf(waiting)('ruling 214(b) the ground, not the flag', () => {
  it('a Canadian province taken by the United States still pays by Canada\'s factor, at the rate for conquered ground', () => {
    const g = grand(3, 'USA');
    const r = g.regions.find((x) => x.owner === 'CAN' && x.ctrl === 'CAN' && !x.cap && x.port)!;
    expect(r).toBeDefined();
    capture(g, r, 'USA');
    expect(r.ctrl).toBe('USA');
    expect(factorOf(g, r)).toBe(0.2);
    const y = provinceYield(g, r, 'USA');
    const base = basePay(g, r, 'USA');
    expect(close(y.land, base.land * 0.2)).toBe(true);
    expect(close(y.port, base.port * 0.2)).toBe(true);
  });

  it('land Canada takes from the United States pays by its own factor, which is 1: a conqueror is paid for rich ground', () => {
    const g = grand(3, 'USA');
    const r = g.regions.find((x) => x.owner === 'USA' && x.ctrl === 'USA' && !x.cap && x.port)!;
    capture(g, r, 'CAN');
    expect(r.ctrl).toBe('CAN');
    expect(factorOf(g, r)).toBe(1);
    const y = provinceYield(g, r, 'CAN');
    const base = basePay(g, r, 'CAN');
    expect(close(y.land, base.land)).toBe(true);
    expect(close(y.port, base.port)).toBe(true);
  });

  it('the factor stays with the ground even where the province changes owner (keyed by the first day, not by the flag)', () => {
    const g = grand(3, 'USA');
    const r = g.regions.find((x) => x.owner === 'MEX' && x.ctrl === 'MEX' && !x.cap)!;
    r.owner = 'USA';
    r.ctrl = 'USA';
    expect(factorOf(g, r)).toBe(0.17);
    const s = g.regions.find((x) => x.owner === 'USA' && x.ctrl === 'USA' && !x.cap)!;
    s.owner = 'MEX';
    s.ctrl = 'MEX';
    expect(factorOf(g, s)).toBe(1);
  });

  it('a whole country\'s first-day land and harbour pay falls by exactly its factor', () => {
    const g = grand();
    for (const [nat, f] of Object.entries(RULED)) {
      if (!g.nations[nat]) continue;
      let paid = 0, was = 0;
      for (const r of g.regions) {
        if (r.owner !== nat || r.ctrl !== nat) continue;
        const y = provinceYield(g, r, nat), b = basePay(g, r, nat);
        paid += y.land + y.port;
        was += b.land + b.port;
      }
      expect(close(paid, was * f), `${nat}: ${paid} against ${was} times ${f}`).toBe(true);
    }
  });
});

describe.skipIf(waiting)('ruling 214(e) the grand campaign only', () => {
  it('a history campaign pays as it always has: by factories, with no land line at all', () => {
    const g = createGame(3, 'GER');
    for (const r of g.regions) {
      const y = provinceYield(g, r, r.ctrl);
      expect(y.land).toBe(0);
      expect(y.port).toBe(0);
    }
  });
});
