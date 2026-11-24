// No country names a division twice (armies.ts numberCorps). A formation whose own name is its division, as the
// record's "15th Panzer Division" sent to Tripoli in its month, is that division while it stands on the map: a stand-in
// formed after it passes over its number, and a stand-in that took the number before it came takes the next free one.
// (Found on 24 November 2026 joining the war plan onto release 2: in one war Germany had formed its fifteenth armoured
// division before the record's own arrived, and the two stood on the map under one name.)

import { describe, expect, it } from 'vitest';
import { addCorps, createGame } from '../src/campaign/game';
import { divisionsOf } from '../src/campaign/armies';
import { startOpen } from '../src/campaign/staff';
import { type Game } from '../src/campaign/types';

const twice = (g: Game, nat: string): string[] => {
  const seen = new Set<string>(), out: string[] = [];
  for (const c of g.corps) if (c.nat === nat && !c.dead) for (const d of divisionsOf(g, c)) { if (seen.has(d.name)) out.push(d.name); seen.add(d.name); }
  return out;
};

describe('a division named by the record and a stand-in never share a name', () => {
  for (const [nat, kind, name] of [['GER', 'arm', '15th Panzer Division'], ['UK', 'arm', '9th Armoured Division'], ['ROM', 'inf', '30th Infantry Division']] as const) {
    it(`${nat}: a stand-in formed after "${name}" passes over its number`, () => {
      const g = startOpen(createGame(3, null));
      const home = g.regions.find((r) => r.owner === nat && r.ctrl === nat)!;
      const real = addCorps(g, nat, name, kind, 1, home.id, {});
      expect(divisionsOf(g, real)[0].name).toBe(name);
      for (let i = 0; i < 40; i++) addCorps(g, nat, `Test Corps ${i}`, kind, 1, home.id, {});
      expect(twice(g, nat)).toEqual([]);
    });

    it(`${nat}: a stand-in that took the number before "${name}" came takes the next free one that day`, () => {
      const g = startOpen(createGame(3, null));
      const home = g.regions.find((r) => r.owner === nat && r.ctrl === nat)!;
      const made = Array.from({ length: 40 }, (_, i) => addCorps(g, nat, `Test Corps ${i}`, kind, 1, home.id, {}));
      const before = made.map((c) => divisionsOf(g, c)[0].name);
      expect(before, 'the set-up: a stand-in carries the name').toContain(name);
      const real = addCorps(g, nat, name, kind, 1, home.id, {});
      expect(divisionsOf(g, real)[0].name).toBe(name);
      expect(twice(g, nat)).toEqual([]);
      // Only the one stand-in changed its number.
      const after = made.map((c) => divisionsOf(g, c)[0].name);
      expect(after.filter((n, i) => n !== before[i]).length).toBe(1);
    });
  }
});
