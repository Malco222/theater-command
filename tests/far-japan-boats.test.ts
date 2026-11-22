// Japan's submarines on the far side: how many it has, and where they go (the design director's rulings of 19:44Z,
// 20:03Z and 20:04Z, 22 November 2026; the historian's japan-subs-building-and-duty-1943-45.md).
//
// The fault it mends: Japan lost 7 to 10 boats in a war against the record's 89 to ships and aircraft. Two of the
// four causes are its own rows. It began the war with 55 boats against the record's 63 and built none, where the
// record completed 111 ocean-going boats in the war. Every state here is set by hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { NAVY, navalUpkeep } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game } from '../src/campaign/types';

const grand = (seed = 3): Game => startOpen(createGame(seed, null));
const named = (g: Game, nat: string, name: string): Fleet => g.fleets.find((x) => x.nat === nat && x.name === name)!;
/** The boats Japan's rows build from one month to another, both counted. */
const built = (from: number, to: number): number => {
  let n = 0;
  for (const b of NAVY.building) if (b.nat === 'JAP' && b.cls === 'ss') for (let t = Math.max(from, b.from); t <= Math.min(to, b.to); t++) n += b.perMonth;
  return n;
};

describe("the far side: Japan's submarines built", () => {
  it('the rows are the record\'s boats announced by half-year, I and RO boats only, in whole boats a month', () => {
    const rows = NAVY.building.filter((b) => b.nat === 'JAP');
    expect(rows.every((b) => b.cls === 'ss' && b.fleet === '6th Fleet' && Number.isInteger(b.perMonth) && b.perMonth > 0)).toBe(true);
    // January 1942 is month 29, and a half-year is six months.
    const half = (i: number): number => built(29 + 6 * i, 34 + 6 * i);
    expect([0, 1, 2, 3, 4, 5].map(half)).toEqual([16, 18, 15, 20, 10, 8]);
    // 1945, January to August: the war's 111 completed less the 87 announced to the end of 1944.
    expect(built(65, 72)).toBe(24);
    expect(built(29, 72)).toBe(111);
    expect(built(73, 200)).toBe(0);
    // No month's rows overlap.
    for (let t = 1; t <= 72; t++) expect(rows.filter((b) => b.from <= t && t <= b.to).length, `month ${t}`).toBeLessThanOrEqual(1);
  });

  it('Japan goes to war in December 1941 with the record\'s 63 boats', () => {
    const g = grand();
    const boats = g.fleets.filter((f) => f.nat === 'JAP').reduce((n, f) => n + f.ships.ss, 0);
    expect(boats).toBe(55);
    expect(built(1, 28)).toBe(8);
    expect(boats + built(1, 28)).toBe(63);
  });

  it('a month\'s boats join the 6th Fleet, and still do when every boat it had is out with the Advance Submarine Fleet', () => {
    const g = grand();
    const pool = named(g, 'JAP', '6th Fleet');
    g.turn = 30;
    const had = pool.ships.ss;
    navalUpkeep(g, 'JAP');
    expect(pool.ships.ss).toBe(had + 3);
    pool.ships.ss = 0;
    g.turn = 31;
    navalUpkeep(g, 'JAP');
    expect(g.fleets).toContain(pool);
    expect(pool.ships.ss).toBe(3);
  });
});
