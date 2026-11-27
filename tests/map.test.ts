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
    expect(g.regions.filter((r) => !r.nb.length).map((r) => r.name).sort()).toEqual(['Corsica', 'Crete', 'Cyprus', 'Malta', 'Rhodes', 'Sardinia']);
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
  it('the desert road runs from Tripoli to the Nile, and El Alamein is its gate', () => {
    expect(hops('Tripoli', 'Alexandria')).toBeGreaterThanOrEqual(9);
    expect(hops('Tripoli', 'Benghazi')).toBeGreaterThanOrEqual(5);
    expect(hops('Tobruk', 'Alexandria')).toBe(5);
    // Nothing west of El Alamein touches the delta except El Alamein itself.
    const delta = ['Alexandria', 'Cairo', 'Faiyum', 'Port Said', 'Suez'].map((n) => g.byName[n].id);
    for (const r of g.regions) {
      if (delta.includes(r.id) || r.name === 'El Alamein' || r.name === 'Sinai') continue;
      for (const j of r.nb) expect(delta, `${r.name} and ${g.regions[j].name}`).not.toContain(j);
    }
    expect(g.byName['Qattara Depression'].nb.map((i) => g.regions[i].name).sort()).toEqual(['Mersa Matruh', 'Siwa']);
    expect(g.byName['Qattara Depression'].terrain).toBe('marsh');
    expect(g.byName['Tobruk'].terrain).toBe('desert');
  });
  it('North Africa, the Levant and the islands belong to the right flags in 1939', () => {
    const owner = (n: string) => g.byName[n].owner;
    for (const n of ['Tripoli', 'Benghazi', 'Tobruk', 'Bardia', 'Rhodes']) expect(owner(n), n).toBe('ITA');
    for (const n of ['Sidi Barrani', 'Alexandria', 'Cairo', 'Suez', 'Palestine', 'Malta', 'Cyprus', 'Gibraltar']) expect(owner(n), n).toBe('UK');
    for (const n of ['Casablanca', 'Algiers', 'Tunis', 'Lebanon', 'Damascus']) expect(owner(n), n).toBe('FRA');
    expect(owner('Crete')).toBe('GRE');
    expect(owner('Baghdad')).toBe('IRQ');
    expect(g.byName['Gibraltar'].nb.map((i) => g.regions[i].name)).toEqual(['Cadiz']);
    expect(hops('Tunis', 'Tripoli')).toBeGreaterThan(1);
    expect(hops('Cairo', 'Baghdad')).toBeGreaterThan(3);
    expect(hops('Palestine', 'Syria')).toBe(2);
  });
  it('adding the south moved nothing in the north', () => {
    expect(g.byName['Berlin'].id).toBe(createGame(1, null).byName['Berlin'].id);
    expect(g.regions.length).toBe(375);
    expect(g.byName['Casablanca'].id).toBe(315);
    expect(g.byName['Rhodes'].id).toBe(370);
    // The two places the Italian campaign of 1944 turns on went on the end.
    expect(g.byName['Cassino'].id).toBe(373);
    expect(g.byName['Anzio'].id).toBe(374);
  });
  it('the land bridge between the Don and the Volga is the only way to Stalingrad from the west', () => {
    const near = (n: string): string[] => g.byName[n].nb.map((j) => g.regions[j].name).sort();
    // The rivers are barriers: nobody steps from the Don bend straight into the city, or from Salsk onto the land bridge.
    expect(near('Kalach')).toEqual(['Don Bend', 'Kalmyk Steppe', 'Stalingrad']);
    expect(near('Stalingrad')).toEqual(['Astrakhan', 'Kalach', 'Kalmyk Steppe', 'Kamyshin']);
    expect(near('Don Bend')).not.toContain('Stalingrad');
    expect(near('Salsk')).not.toContain('Kalach');
    // New places go on the end, so that saved games still find every older region where it was.
    expect(g.byName['Kalach'].id).toBe(371);
    expect(g.byName['Astrakhan'].id).toBe(372);
  });
});
