// The stood war rows' loans with the far side on (release 2, the grand campaign). The map line's 1e2b5229 lends a
// stood war row's divisions for its war: layOnce remembers where each came from (`drew`), lent:<i> names the corps,
// and when the war is over each goes back, or stands on, and lent:<i> is -1. The map line's own tests pin it with the
// far side off; the fold's expeditions() puts its loan, offer and posting branches round the same code, so this file
// walks it with the far side on: a computer's Italy at war with Britain and Greece lays the four Albania rows, makes
// peace with Greece, and has every division back or standing; a player's Italy with the offers on has none of these
// rows lent, offered or listed among his ending loans (the skeptic's review of the merge, 25 November 2026: no far-on
// test caught the give-back branch removed, or layOnce told the war rows are not lent).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { divisionsOf } from '../src/campaign/armies';
import { signPeace } from '../src/campaign/diplomacy';
import { createGame, loansEnding, rowOffers, setWar, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { farOn } from '../src/campaign/world';
import type { Game } from '../src/campaign/types';

const E = (setup as any).expeditions as any[];
const at = (name: string): number => E.findIndex((r) => r[1] === 'ITA' && r[2] === name);
const ALBANIA = ['Ciamuria Corps and Julia Alpine Division', 'XXVI Corps', '9th Army', '11th Army reserves'].map(at);
const months = (g: Game, from: number, to: number): void => { for (let t = from; t <= to; t++) { g.turn = t; startPhase(g, 'ITA'); } };
const living = (g: Game) => g.corps.filter((c) => c.nat === 'ITA' && !c.dead);
const held = (g: Game): Map<string, number> => {
  const m = new Map<string, number>();
  for (const c of living(g)) for (const d of divisionsOf(g, c)) {
    expect(m.has(d.name), `${d.name} stands in two corps`).toBe(false);
    m.set(d.name, c.id);
  }
  return m;
};
const men = (g: Game): number => living(g).reduce((s, c) => s + c.divs * c.str, 0);
const italy = (human: string | null): Game => {
  const g = startOpen(createGame(1, human));
  if (human) g.flags.offers = 1;
  setWar(g, 'ITA', 'UK');
  setWar(g, 'ITA', 'GRE');
  return g;
};

describe("Italy's Albania rows are lent for the war with Greece, with the far side on", () => {
  it("a computer's Italy: the rows draw and are lent; peace with Greece gives every drawn division back or leaves it standing", () => {
    const g = italy(null);
    expect(farOn(g)).toBe(true);
    expect(ALBANIA.every((i) => i > -1)).toBe(true);
    months(g, 1, 19);
    const lentTo = ALBANIA.map((i) => g.flags[`lent:${i}`]);
    for (const id of lentTo) expect(living(g).some((c) => c.id === id)).toBe(true);
    // What they drew, and from which corps: the record the war's end reads.
    const drawn = ALBANIA.flatMap((i) => (living(g).find((c) => c.id === g.flags[`lent:${i}`])?.drew ?? []).map((r) => ({ ...r, by: g.flags[`lent:${i}`] as number })));
    expect(drawn.length).toBeGreaterThan(0);
    const before = held(g), m0 = men(g);
    expect(signPeace(g, 'ITA', 'GRE')).toBe(true);
    months(g, 20, 21);
    expect(ALBANIA.map((i) => g.flags[`lent:${i}`])).toEqual([-1, -1, -1, -1]);
    const after = held(g);
    expect([...before.keys()].filter((n) => !after.has(n))).toEqual([]);
    expect(men(g)).toBeGreaterThanOrEqual(m0);
    // At least one division drawn out of another corps is back in it (the 10th Infantry, from the Ciamuria Corps to the 8th Army).
    const home = drawn.filter((r) => r.from !== r.by && before.get(r.name[0]) === r.by && after.get(r.name[0]) === r.from);
    expect(home.length).toBeGreaterThan(0);
  });

  it("a player's Italy with the offers on: the rows come by their lines, never lent, offered or listed as an ending loan", () => {
    const g = italy('ITA');
    months(g, 1, 19);
    expect(ALBANIA.map((i) => g.flags[`sent:${i}`])).toEqual([1, 1, 1, 1]);
    expect(ALBANIA.map((i) => g.flags[`lent:${i}`])).toEqual([undefined, undefined, undefined, undefined]);
    expect(ALBANIA.map((i) => g.flags[`asked:${i}`])).toEqual([undefined, undefined, undefined, undefined]);
    const names = ALBANIA.map((i) => E[i][2]);
    expect(rowOffers(g).filter((o) => names.includes(o.name))).toEqual([]);
    expect(loansEnding(g).filter((l) => names.includes(l.name))).toEqual([]);
    held(g);
    expect(signPeace(g, 'ITA', 'GRE')).toBe(true);
    months(g, 20, 21);
    expect(ALBANIA.map((i) => g.flags[`lent:${i}`])).toEqual([undefined, undefined, undefined, undefined]);
    held(g);
  });
});
