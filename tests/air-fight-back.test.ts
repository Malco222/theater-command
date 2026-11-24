// The computer's air forces fight back (Brian, 24 November 2026: "im attacking other nations that have airforces but
// they arent using them"; release 1.5.1, airwar.json ai.back; the design director's conditions, 18:38Z): every staff
// puts fighters on ground support over the battles it defends, keeping as many on patrol over each; a country whose
// air force was only a rating raises it as air groups once at war, and the rating then no longer tilts its battles.
import { describe, expect, it } from 'vitest';
import '../unreal/TheaterCommand/Rules/campaign-bridge';
import { AIRWAR, airAI, airRange, cover, defendBattles, fightBack, km, ratedAir, raiseAirForce } from '../src/campaign/air';
import { createGame } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const TC = (globalThis as Record<string, unknown>).TC as any;
const copy = (): Game => loadGame(TC.save().text)!;
const B = AIRWAR.ai.back!;

describe('the fight back', () => {
  it('is on, with the design director\'s numbers', () => {
    expect(B.on).toBe(true);
    expect([B.close, B.success, B.worth, B.risk, B.invaded, B.least]).toEqual([0.34, 0.45, 10, 0.05, 4, 12]);
    expect([B.minor.per, B.minor.ftr, B.minor.refit]).toEqual([20, 0.6, 0.05]);
  });

  it('a country at war whose air force was only a rating raises it once, at its capital, and the rating stops counting', () => {
    TC.start({ view: 'grand', human: 'USA', from: 1, seed: 11, world: true });
    let g = copy();
    expect(g.airFleets.some((f) => f.nat === 'MEX')).toBe(false);
    expect(ratedAir(g, 'MEX')).toBe(2);
    // At peace it raises nothing.
    expect(raiseAirForce(g, 'MEX')).toEqual([]);
    expect(TC.declare('MEX').ok).toBe(true);
    TC.endMonth();
    g = copy();
    const mex = g.airFleets.filter((f) => f.nat === 'MEX');
    expect(mex.map((f) => [f.kind, f.ftr, f.bmr])).toEqual([['ftr', 24, 0], ['bmr', 0, 16]]);
    expect(mex.every((f) => f.est.ftr === f.ftr && f.est.bmr === f.bmr)).toBe(true);
    expect(g.flags['mair:MEX']).toBeGreaterThan(0);
    expect(ratedAir(g, 'MEX')).toBe(0);
    // The great powers' come from the record, and are never raised.
    expect(raiseAirForce(g, 'USA')).toEqual([]);
    // Once only: a country that has lost them raises no more.
    g.airFleets = g.airFleets.filter((f) => f.nat !== 'MEX');
    expect(raiseAirForce(g, 'MEX')).toEqual([]);
    // With the switch off it is as release 1.5: no air force raised, the rating counts.
    B.on = false;
    try { expect(ratedAir(g, 'MEX')).toBe(2); expect(fightBack(g, 'MEX')).toBe(false); } finally { B.on = true; }
  }, 120_000);

  it('a small air force goes to the invader: Mexico\'s bombers raid him or its fighters go up over its battles', () => {
    TC.start({ view: 'grand', human: 'USA', from: 1, seed: 11, world: true });
    const g = copy();
    const border = g.regions.filter((r) => r.ctrl === 'USA' && r.nb.some((j) => g.regions[j].ctrl === 'MEX')).map((r) => r.id);
    g.corps.filter((c) => c.nat === 'USA' && !c.dead && c.kind !== 'fort').slice(0, 6).forEach((c, i) => { c.reg = border[i % border.length]; c.str = 1; c.supplied = true; });
    for (const r of g.regions) r.units = g.corps.filter((c) => c.reg === r.id);
    TC.load(saveGame(g));
    expect(TC.declare('MEX').ok).toBe(true);
    const capital = g.nations.MEX.capital;
    let raids = 0, forward = 0;
    for (let m = 0; m < 6 && !(raids && forward); m++) {
      const h = copy();
      for (const c of h.corps.filter((x) => x.nat === 'USA' && !x.dead && x.kind !== 'fort')) {
        const to = h.regions[c.reg].nb.find((j) => h.regions[j].ctrl === 'MEX');
        if (to !== undefined) TC.attack(h.corps.filter((x) => x.nat === 'USA' && h.regions[x.reg].nb.includes(to)).map((x) => x.id), to);
      }
      const n0 = copy().airReports.length;
      TC.endMonth();
      const k = copy();
      raids += k.airReports.slice(n0).filter((r) => r.attNat === 'MEX').length;
      forward += k.airFleets.filter((f) => f.nat === 'MEX' && (f.close || f.base !== capital)).length;
    }
    // (Release 1.5.1 sees both. Release 2's bombers need a concrete airfield, so there they fly from Mexico City alone.)
    expect(raids + forward).toBeGreaterThan(0);
  }, 240_000);

  it('a staff defending puts fighters on ground support over its battles, and keeps as many on patrol over each', () => {
    const g = startOpen(createGame(23, null));
    let seen = 0;
    for (let t = 0; t < 12; t++) {
      runTurnAI(g);
      for (const nat of Object.keys(g.nations)) {
        const held = defendBattles(g, nat);
        if (!held.length || !g.nations[nat].alive) continue;
        // The staff's month as it plans it, on a copy: its ground support, then its sweeps and raids.
        const h = loadGame(saveGame(g))!;
        airAI(h, nat, null);
        const land = h.airFleets.filter((f) => f.nat === nat && f.kind === 'ftr' && f.ftr > 0 && f.ship === undefined);
        for (const b of held) {
          // (at the strengths the staff planned with: an escort flown later in its month may lose a few)
          const had = (id: number): number => g.airFleets.find((x) => x.id === id)?.ftr ?? 0;
          const over = (close: boolean): number => land.filter((f) => !!f.close === close && cover(km(h, f.base, b), airRange(f, h).ftr) > 0).reduce((s, f) => s + had(f.id), 0);
          const onCall = over(true);
          if (onCall > 0) seen++;
          expect(over(false)).toBeGreaterThanOrEqual(onCall);
        }
      }
    }
    expect(seen).toBeGreaterThan(0);
  }, 300_000);
});
