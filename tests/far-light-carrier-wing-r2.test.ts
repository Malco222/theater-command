// A light carrier that sailed alone brings its own, smaller wing (ruling 189(b); the historian's pacific-fleets-check.md,
// section 4: a third of a wing), built by the navy thread on the far side's audit of 25 November 2026 (FC-06): Ryujo
// with the 3rd Fleet from the first day, Shoho with the 4th Fleet from January 1942. Carriers joined to another fleet
// bring their share, and nothing is made or lost by the joining.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { beforeAll, describe, expect, it } from 'vitest';
import { airPool } from '../src/campaign/air';
import { syncWings, wingsOf } from '../src/campaign/carriers';
import { merge } from '../src/campaign/fleets';
import { createGame } from '../src/campaign/game';
import { NAVY, navalUpkeep } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';

const named = (g: Game, name: string): Fleet => g.fleets.find((f) => f.nat === 'JAP' && f.name === name)!;
const room = (g: Game, f: Fleet): [number, number] => [wingsOf(g, f.id).find((w) => w.kind === 'ftr')?.est.ftr ?? 0, wingsOf(g, f.id).find((w) => w.kind === 'bmr')?.est.bmr ?? 0];
let base: string;
beforeAll(() => { const g = startOpen(createGame(3, null)); runTurnAI(g); base = saveGame(g); }, 300000);

describe("Japan's lone light carriers", () => {
  it('Ryujo brings a third of a wing; the fleet carriers a whole one each', () => {
    const g = loadGame(base)!;
    expect(named(g, '3rd Fleet').deck).toBe(0.33);
    expect(room(g, named(g, '3rd Fleet'))).toEqual([8, 14]);
    const mf = named(g, 'Mobile Fleet');
    expect(mf.deck).toBeUndefined();
    expect(room(g, mf)).toEqual([24 * mf.ships.cv, 42 * mf.ships.cv]);
  });
  it('Shoho joins the 4th Fleet in January 1942 with a third of a wing', () => {
    const g = loadGame(base)!;
    expect(NAVY.arrivals.some((a) => a[0] === 29 && a[1] === 'JAP' && a[2] === '4th Fleet' && a[3].cv === 1)).toBe(true);
    const f4 = named(g, '4th Fleet');
    g.turn = 29;
    navalUpkeep(g, 'JAP');
    syncWings(g);
    expect(f4.ships.cv).toBe(1);
    expect(room(g, f4)).toEqual([8, 14]);
  });
  it('joined to the Mobile Fleet, Ryujo brings her share: nothing is made or lost', () => {
    const g = loadGame(base)!;
    const r = named(g, '3rd Fleet'), mf = named(g, 'Mobile Fleet');
    const cv = mf.ships.cv;
    mf.zone = r.zone; mf.stance = r.stance; mf.base = r.base; mf.home = r.home;
    const all = (k: 'ftr' | 'bmr'): number => g.airFleets.filter((w) => w.nat === 'JAP' && w.ship !== undefined).reduce((n, w) => n + w[k], 0) + airPool(g, 'JAP')[k];
    const had = [all('ftr'), all('bmr')];
    expect(merge(g, r, mf)).toBe(true);
    expect(mf.deck).toBeCloseTo((cv + 0.33) / (cv + 1), 6);
    expect(room(g, mf)).toEqual([Math.round(24 * (cv + 0.33)), Math.round(42 * (cv + 0.33))]);
    expect([all('ftr'), all('bmr')]).toEqual(had);
  });
});
