// The campaign map: every region belongs to someone, and the east is reachable.
import { describe, expect, it } from 'vitest';
import { allied, createGame } from '../src/campaign/game';

const g = createGame(1, null);

/** Regions between two places by land, or -1 if there is no way through. */
function hops(from: string, to: string): number {
  const a = g.byName[from].id, b = g.byName[to].id;
  const d = new Map<number, number>([[a, 0]]);
  const q = [a];
  for (let i = 0; i < q.length; i++) {
    if (q[i] === b) return d.get(b)!;
    for (const j of g.regions[q[i]].nb) if (!d.has(j)) { d.set(j, d.get(q[i])! + 1); q.push(j); }
  }
  return -1;
}

describe('the campaign map', () => {
  it('every region has land, an owner that exists, and a unique name', () => {
    expect(new Set(g.regions.map((r) => r.name)).size).toBe(g.regions.length);
    for (const r of g.regions) {
      expect(r.polys.length, r.name).toBeGreaterThan(0);
      expect(g.nations[r.owner], r.name).toBeDefined();
    }
  });
  it('only islands are cut off from their neighbours', () => {
    expect(g.regions.filter((r) => !r.nb.length).map((r) => r.name).sort()).toEqual(['Corsica', 'Sardinia']);
  });
  it('every nation with an army has a capital on the map', () => {
    for (const n of Object.values(g.nations)) if (!n.dormant) expect(n.capital, n.name).toBeGreaterThanOrEqual(0);
    expect(g.regions[g.nations.SOV.capital].name).toBe('Moscow');
  });
  it('every corps starts in a region its own side holds', () => {
    for (const c of g.corps) expect(allied(g, c.nat, g.regions[c.reg].ctrl), c.name).toBe(true);
  });
  it('reaches Moscow, the Volga and the Caucasus', () => {
    expect(hops('Warsaw', 'Moscow')).toBeGreaterThanOrEqual(5);
    expect(hops('Warsaw', 'Moscow')).toBeLessThanOrEqual(9);
    expect(hops('Moscow', 'Stalingrad')).toBeGreaterThan(2);
    expect(hops('Rostov', 'Grozny')).toBeGreaterThan(1);
    expect(hops('Kiev', 'Kharkov')).toBeLessThanOrEqual(3);
    expect(g.byName['Crimea'].nb).toContain(g.byName['Novorossiysk'].id);
    expect(hops('Leningrad', 'Moscow')).toBeGreaterThan(1);
  });
  it('Turkey lies between the Balkans and the Caucasus', () => {
    expect(hops('Istanbul', 'Batumi')).toBeGreaterThan(3);
    expect(g.byName['Syria'].owner).toBe('FRA');
    expect(g.byName['Syria'].nb.map((i) => g.regions[i].owner)).toContain('TUR');
  });
});
