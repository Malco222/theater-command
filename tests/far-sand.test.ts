// The desert the old chart's desert runs on into reads the desert's rules (the design director's rulings of 21:55Z,
// 22:02Z, 22:08Z and 22:20Z of 21 November 2026, on the reading of Italy's march on the oases; data/campaign/far-sand.json,
// game.ts `sandRules`, `sandNoRail`). Behind the far switch.
//
// As measured (seed 3 at the stop next to the book's, months 19 to 28): a beaten corps of Italy's fell back to Kufra
// and walked Faya-Largeau, Dongola, Wadi Halfa and Kharga in one month and Asyut the next. Kufra and the oases are
// added provinces, and the Western Desert's own rules held on the old chart alone: the walk paid no drag and its
// line back to Tripoli read no length. The added provinces of desert ground joined to the old chart's desert by
// unbroken desert ground now read the line, the drag and the bands as the old chart's desert does. Those the record
// had on a working railway or a river line keep the trains the chart draws there, and no more: the open sand round
// the strip pays like the rest. The added sand had also been free ground round the old chart's edge, and seven of
// the old chart's own lines read short with the far side on; they read again as with the switch off (the design
// director's 22:20Z). Here the ground is the war's first day.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import sand from '../data/campaign/far-sand.json';
import { DESERT, addCorps, computeSupply, createGame, inDesert, lineDrag, railReach, sandNoRail, sandRules } from '../src/campaign/game';
import { railsAt } from '../src/campaign/rails';
import { startOpen } from '../src/campaign/staff';
import { beyondEurope } from '../src/campaign/world';
import type { Game, Region } from '../src/campaign/types';

const SAND: string[] = (sand as any).sand, RAIL: string[] = (sand as any).rail;
const open = (): Game => startOpen(createGame(3, null));
const desert = (r: Region): boolean => r.terrain === DESERT.terrain;
/** The added provinces of desert ground that unbroken desert ground joins to the old chart's desert. */
const joined = (g: Game): Region[] => {
  const seen = new Set<number>(), q: number[] = [];
  for (const r of g.regions) if (desert(r) && !beyondEurope(r)) { seen.add(r.id); q.push(r.id); }
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (!seen.has(j) && desert(g.regions[j])) { seen.add(j); q.push(j); }
  return g.regions.filter((r) => beyondEurope(r) && seen.has(r.id));
};
const drag = (length: number): number => (DESERT.drag.find(([max]) => length <= max) ?? [0, 0])[1];
/** The old chart's desert on the war's first day, as measured with the switch off: the length of each province's line as its holder's supply reads it. */
const OLD: Record<string, number> = {
  'Colomb-Bechar': 1, Laghouat: 1, Ouargla: 2, Biskra: 1, Mareth: 1, Nalut: 1, Misrata: 1, Sirte: 2, Hon: 2, 'El Agheila': 3,
  Agedabia: 3, Mechili: 3, Tobruk: 2, 'Bir Hakeim': 3, Bardia: 3, Jalo: 4, Jarabub: 4, 'Sidi Barrani': 3, 'Mersa Matruh': 2,
  'El Alamein': 1, Suez: 1, Siwa: 3, Faiyum: 1, Sinai: 1, Transjordan: 1, Palmyra: 1, 'Deir ez-Zor': 1, Rutbah: 1,
};

describe('the desert the old chart\'s desert runs on into', () => {
  it('the list: the added desert provinces joined to the old chart\'s desert, all of them and no other; eleven keep their trains', () => {
    const g = open();
    expect(SAND).toHaveLength(50);
    expect(new Set(SAND).size).toBe(50);
    expect(joined(g).map((r) => r.name).sort()).toEqual([...SAND].sort());
    for (const nm of SAND) expect(g.regions.filter((r) => r.name === nm), nm).toHaveLength(1);
    expect(RAIL).toHaveLength(11);
    for (const nm of RAIL) expect(SAND, nm).toContain(nm);
  });

  it('the rule: each of the fifty reads the desert\'s rules; the desert that stands apart and the ground beside the sand do not', () => {
    const g = open();
    for (const nm of SAND) { expect(sandRules(g.byName[nm]), nm).toBe(true); expect(inDesert(g.byName[nm]), nm).toBe(true); }
    // (Desert ground with no desert road to the old chart's: the Gobi, Australia, the Red Sea coast.)
    for (const nm of ['Gobi', 'Alice Springs', 'Hurghada']) { expect(desert(g.byName[nm]), nm).toBe(true); expect(sandRules(g.byName[nm]), nm).toBe(false); }
    // (The Nile's own ground is not sand.)
    for (const nm of ['Asyut', 'Qena', 'Khartoum']) expect(sandRules(g.byName[nm]), nm).toBe(false);
    // (The old chart's desert is as it was.)
    for (const nm of ['Jalo', 'Siwa', 'Tobruk', 'Rutbah']) expect(sandRules(g.byName[nm]), nm).toBe(true);
  });

  it('the line: Kufra\'s is a step longer than the shortest beside it; the old chart\'s desert reads its lines as on the old chart alone', () => {
    const g = open();
    computeSupply(g);
    const reach = g.reach.ITA, fed = g.supply.ITA, kufra = g.byName.Kufra;
    expect(fed[kufra.id]).toBeTruthy();
    const beside = kufra.nb.filter((j) => fed[j]).map((j) => reach[j]);
    expect(beside.length).toBeGreaterThan(0);
    expect(reach[kufra.id]).toBeCloseTo(Math.min(...beside) + DESERT.step, 5);
    // (The old chart's 28, each as its holder's supply reads it. Keyed by the chart alone the added sand was free
    // ground round the old chart's edge, and seven of these read short with the far side on: Siwa 1 for 3, Jalo and
    // Jarabub 2 for 4, Mersa Matruh 1 for 2, Sidi Barrani 2 for 3, El Agheila 2 for 3, Ouargla 1 for 2.)
    expect(g.regions.filter((r) => desert(r) && !beyondEurope(r)).map((r) => r.name).sort()).toEqual(Object.keys(OLD).sort());
    for (const [nm, length] of Object.entries(OLD)) { const r = g.byName[nm]; expect(g.reach[r.ctrl][r.id], nm).toBe(length); }
  });

  it('the drag: a bound into the sand pays it, out of Kufra by the door it keeps and into the kept eleven by theirs, and not into the Nile\'s plain', () => {
    const g = open();
    // (Since the design director's notes of 14:18Z and 14:23Z of 22 November 2026 the sand between Kufra and the Nile
    // and the Great Sand Sea carry no army, and Kufra's roads run through Libya and Chad alone (tests/far-kufra.test.ts).
    // So the walk as it was measured is read door by door: out of Kufra into Chad, and into each of the kept eleven on
    // that walk from the province the chart still joins it to.)
    const kufra = g.byName.Kufra;
    for (const nm of ['Dongola', 'Wadi Halfa', 'Kharga']) expect(kufra.nb, nm).not.toContain(g.byName[nm].id);
    const WALK: [string, string][] = [['Kufra', 'Faya-Largeau'], ['Faya-Largeau', 'Dongola'], ['Dongola', 'Wadi Halfa'], ['Wadi Halfa', 'Kharga']];
    const groups = WALK.map(([from]) => addCorps(g, 'ITA', `a test group (${from})`, 'inf', 1, from, { xp: 0.3 }));
    computeSupply(g);
    for (const [i, [from, to]] of WALK.entries()) {
      const c = groups[i];
      expect(g.byName[from].nb, `${from} to ${to}`).toContain(g.byName[to].id);
      // (Set by hand: the line where the bound starts as it stands with the nearer harbours lost, six, then seven.)
      for (const length of [6, 7]) {
        g.reach.ITA[g.byName[from].id] = length;
        expect(lineDrag(g, c, g.byName[to]), `${to} from ${from} at ${length}`).toBe(drag(length + DESERT.step));
      }
    }
    expect(drag(7 + DESERT.step)).toBeGreaterThan(0);
    // (From Kharga on into Asyut: plains, and no desert's drag.)
    const d = addCorps(g, 'ITA', 'a test group (Kharga)', 'inf', 1, 'Kharga', { xp: 0.3 });
    // (On a copy: the picture computeSupply hands out may be kept for its next call, and is never written into.)
    g.reach.ITA = Float32Array.from(g.reach.ITA);
    g.reach.ITA[g.byName.Kharga.id] = 8;
    expect(lineDrag(g, d, g.byName.Asyut)).toBe(0);
  });

  it('the trains: no railway crosses the sand, but the eleven keep the railways the chart draws', () => {
    const g = open();
    for (const nm of SAND) expect(sandNoRail(g.byName[nm]), nm).toBe(!RAIL.includes(nm));
    // (None of the thirty-nine has a railway drawn on it, so the rule takes no train the chart had.)
    for (const nm of SAND) if (!RAIL.includes(nm)) expect(railsAt(g.byName[nm].id), nm).toEqual([]);
    // (The Sudan line: from Atbara a train still runs to Wadi Halfa and to Port Sudan.)
    const c = addCorps(g, 'UK', 'a test brigade (Atbara)', 'inf', 1, 'Atbara', { xp: 0.3 });
    c.ap = c.apMax = 4;
    const reach = railReach(g, c);
    for (const nm of ['Wadi Halfa', 'Port Sudan']) expect(reach.has(g.byName[nm].id), nm).toBe(true);
    // (And from Kufra none leaves.)
    const k = addCorps(g, 'ITA', 'a test group (Kufra)', 'inf', 1, 'Kufra', { xp: 0.3 });
    k.ap = k.apMax = 4;
    expect(railReach(g, k).size).toBe(0);
  });
});
