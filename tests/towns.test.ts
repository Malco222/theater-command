// Cities and towns. Every province has a chief place: both interfaces get the same list to draw (name, size, where it
// stands, the signs it carries, what it is worth), and the same answer to what taking a province would bring. Nothing
// here changes the war: it says what is already true of the ground.
import { describe, expect, it } from 'vitest';
import data from '../data/campaign/towns.json';
import mapData from '../src/generated/campaign-map.json';
import { allied, atWar, capitulate, capture, createGame, removeCorps } from '../src/campaign/game';
import { BUDGET, provinceYield } from '../src/campaign/economy';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { TOWNS, prizeOf, prizesInReach, townAt, townName, townSite, townSize, townsOf, worthOf } from '../src/campaign/towns';
import { prize, prizesNear, townIn, towns } from '../src/campaign/turn';
import type { Game, Region } from '../src/campaign/types';

const open = (human: string | null = null, seed = 3): Game => startOpen(createGame(seed, human));
const take = (g: Game, name: string, nat: string): Region => { const r = g.byName[name]; for (const c of [...r.units]) removeCorps(g, c); capture(g, r, nat); return r; };

type Ring = [number, number][];
const inRing = (x: number, y: number, ring: Ring): boolean => {
  let inside = false;
  for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
    const [xi, yi] = ring[i], [xj, yj] = ring[j];
    if ((yi > y) !== (yj > y) && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) inside = !inside;
  }
  return inside;
};

describe('the town data', () => {
  const g = open();

  it('names a chief town only for provinces on the map, and never the province itself', () => {
    for (const [province, town] of Object.entries(data.names)) {
      expect(g.byName[province], province).toBeTruthy();
      expect(town, province).not.toBe(province);
      expect(town.length, province).toBeGreaterThan(2);
    }
  });

  it('gives every province a place of its own: no two share a name, and no town is called after another province', () => {
    const names = g.regions.map(townName);
    expect(new Set(names).size).toBe(g.regions.length);
    for (const r of g.regions) {
      const other = g.byName[townName(r)];
      expect(!other || other === r, `${r.name}: ${townName(r)}`).toBe(true);
    }
  });

  it('has a word for every size and every sign, and ranks that climb', () => {
    for (const k of ['capital', 'city', 'town', 'village', 'factory', 'harbour', 'oil', 'fort', 'yard', 'airfield']) expect(TOWNS.words[k], k).toBeTruthy();
    expect(TOWNS.ranks).toHaveLength(5);
    for (let i = 1; i < TOWNS.ranks.length; i++) expect(TOWNS.ranks[i]).toBeGreaterThan(TOWNS.ranks[i - 1]);
  });
});

describe('how big a place is', () => {
  it('calls the seat of a standing government a capital, and every country has exactly one', () => {
    const g = open();
    const list = townsOf(g);
    expect(list).toHaveLength(g.regions.length);
    for (const n of Object.values(g.nations)) {
      if (!n.alive || n.capital < 0 || !g.regions[n.capital].cap) continue;
      expect(list[n.capital].size, n.id).toBe('capital');
      expect(list.filter((t) => t.size === 'capital' && t.owner === n.id), n.id).toHaveLength(1);
    }
    expect(townAt(g, g.byName.Berlin.id)!.sizeWord).toBe('Capital');
  });

  it('sorts the rest by what is there: cities by victory points, industry or streets, towns by any of a factory, a harbour or an oilfield', () => {
    const g = open();
    for (const r of g.regions) {
      const size = townSize(g, r);
      if (size === 'capital') continue;
      if (r.cap || r.vp >= 2 || r.fac >= 2 || r.terrain === 'urban') expect(size, r.name).toBe('city');
      else if (r.vp >= 1 || r.fac > 0 || r.port || r.oil) expect(size, r.name).toBe('town');
      else expect(size, r.name).toBe('village');
    }
    expect(townSize(g, g.byName.Ruhr)).toBe('city');
    const sizes = new Set(townsOf(g).map((t) => t.size));
    expect([...sizes].sort()).toEqual(['capital', 'city', 'town', 'village']);
  });

  it('makes a fallen capital a city: the star goes with the government', () => {
    const g = open();
    take(g, 'Warsaw', 'GER');
    capitulate(g, 'POL', 'GER');
    expect(g.nations.POL.alive).toBe(false);
    expect(townSize(g, g.byName.Warsaw)).toBe('city');
  });

  it('uses the chief town for a province named for a stretch of country, and the province itself where it is named for its city', () => {
    const g = open();
    expect(townName(g.byName.Ruhr)).toBe('Essen');
    expect(townName(g.byName['Lower Silesia'])).toBe('Breslau');
    expect(townName(g.byName.Warsaw)).toBe('Warsaw');
    const t = townIn(g, g.byName.Lombardy.id)!;
    expect([t.name, t.province]).toEqual(['Milan', 'Lombardy']);
  });
});

describe('where a town stands', () => {
  const regions = (mapData as any).regions as { id: number; name: string; x: number; y: number; polys: Ring[][] }[];

  it('is inside its own province, near its centre, and never on the centre where there is room', () => {
    let centred = 0;
    for (const r of regions) {
      const [x, y] = townSite(r.id);
      expect(r.polys.some((p) => inRing(x, y, p[0])), r.name).toBe(true);
      expect(Math.hypot(x - r.x, y - r.y), r.name).toBeLessThanOrEqual(35);
      if (x === r.x && y === r.y) centred++;
    }
    expect(centred).toBeLessThan(6);
  });

  it('is a fixed place: the same in every game, whoever holds the ground', () => {
    const a = open(null, 3), b = open('UK', 9);
    take(b, 'Lodz', 'GER');
    for (const r of a.regions) {
      const ta = townAt(a, r.id)!, tb = townAt(b, r.id)!;
      expect([ta.x, ta.y], r.name).toEqual([tb.x, tb.y]);
      expect([ta.x, ta.y], r.name).toEqual(townSite(r.id));
    }
  });
});

describe('what a place is worth', () => {
  it('is what the province pays its own country a month, as the interface shows money', () => {
    const g = open();
    for (const r of g.regions) {
      if (r.ctrl !== r.owner) continue;
      expect(worthOf(g, r), r.name).toBe(Math.round(provinceYield(g, r, r.owner).total * BUDGET.scale));
    }
  });

  it('ranks every province 0 to 5, the great capitals at the top and empty country at the bottom', () => {
    const g = open();
    const list = townsOf(g);
    for (const t of list) {
      expect(t.rank, t.name).toBe(TOWNS.ranks.filter((x) => t.worth >= x).length);
      expect(t.rank).toBeGreaterThanOrEqual(0);
      expect(t.rank).toBeLessThanOrEqual(5);
    }
    for (const nm of ['Berlin', 'Paris', 'London', 'Moscow']) expect(list[g.byName[nm].id].rank, nm).toBe(5);
    expect(list.filter((t) => t.rank === 0).length).toBeGreaterThan(list.length / 3);
    expect(list.filter((t) => t.rank >= 4).length).toBeLessThan(list.length / 8);
  });

  it('carries a sign for each thing a map should show there', () => {
    const g = open();
    for (const t of townsOf(g)) {
      const r = g.regions[t.region];
      expect(t.signs.includes('factory'), t.name).toBe(r.fac > 0);
      expect(t.signs.includes('harbour'), t.name).toBe(!!r.port);
      expect(t.signs.includes('oil'), t.name).toBe(!!r.oil);
      expect(t.signs.includes('fort'), t.name).toBe(!!r.fort);
      expect(t.factories).toBe(r.fac);
      expect(t.holder).toBe(r.ctrl);
    }
    expect(townsOf(g).some((t) => t.signs.includes('oil'))).toBe(true);
    expect(townsOf(g).some((t) => t.signs.includes('yard'))).toBe(true);
  });

  it('keeps its worth when it changes hands: the figure on the map is the place, not the holder', () => {
    const g = open();
    const before = townAt(g, g.byName.Lodz.id)!.worth;
    take(g, 'Lodz', 'GER');
    const after = townAt(g, g.byName.Lodz.id)!;
    expect(after.worth).toBe(before);
    expect(after.holder).toBe('GER');
    expect(after.owner).toBe('POL');
  });
});

describe('the prize', () => {
  it('says what a province gives the nation that holds it', () => {
    const g = open('GER');
    const p = prizeOf(g, g.byName.Berlin.id, 'GER')!;
    expect(p.kind).toBe('yours');
    expect(p.to).toBe('GER');
    expect(p.income).toBe(Math.round(provinceYield(g, g.byName.Berlin, 'GER').total * BUDGET.scale));
    expect(p.income).toBe(p.incomeNow);
    expect(p.line).toMatch(/^Berlin pays \d+ a month into your treasury: \d+ factories and \d+ victory points of Germany's \d+\.$/);
    expect(p.items.map((i) => i.key)).toEqual(expect.arrayContaining(['income', 'factory', 'vp']));
    expect(p.items.some((i) => i.key === 'capital' || i.key === 'fort' || i.key === 'frees')).toBe(false);
  });

  it('says what taking an enemy province would bring, and what it would cost the enemy', () => {
    const g = open('GER');
    const w = g.byName.Warsaw;
    const p = prizeOf(g, w.id, 'GER')!;
    expect(p.kind).toBe('taken');
    expect(p.to).toBe('GER');
    expect(p.income).toBe(Math.round(provinceYield(g, w, 'GER').total * BUDGET.scale));
    expect(p.incomeNow).toBe(Math.round(provinceYield(g, w, 'POL').total * BUDGET.scale));
    // Conquered ground pays less than it pays its own country.
    expect(p.income).toBeGreaterThan(0);
    expect(p.income).toBeLessThan(p.incomeNow);
    expect(p.capitalOf).toBe('POL');
    expect(p.vp).toBe(w.vp);
    expect(p.line).toContain(`Taking Warsaw is worth ${p.income} a month to you and costs Poland ${p.incomeNow}`);
    expect(p.line).toContain("It is Poland's capital.");
    const keys = p.items.map((i) => i.key);
    expect(keys).toEqual(expect.arrayContaining(['income', 'factory', 'vp', 'capital']));
    expect(keys.includes('fort')).toBe(!!w.fort);
    for (const it of p.items) { expect(it.label).toBeTruthy(); expect(it.text.length).toBeGreaterThan(8); expect(it.text.endsWith('.')).toBe(false); }
  });

  it('says what an ally holds, and that ground taken back from the enemy returns to the ally who owns it', () => {
    const g = open('UK');
    expect(allied(g, 'UK', 'FRA')).toBe(true);
    const paris = prizeOf(g, g.byName.Paris.id, 'UK')!;
    expect(paris.kind).toBe('ally');
    expect(paris.to).toBe('FRA');
    expect(paris.line).toContain('your ally');
    take(g, 'Lorraine', 'GER');
    const nancy = prizeOf(g, g.byName.Lorraine.id, 'UK')!;
    expect(nancy.kind).toBe('restored');
    expect(nancy.to).toBe('FRA');
    expect(nancy.town).toBe('Nancy');
    expect(nancy.line).toMatch(/^Taking Nancy gives it back to France, your ally, and takes \d+ a month from Germany/);
    // And for France itself it is ground to take back as its own.
    const own = prizeOf(g, g.byName.Lorraine.id, 'FRA')!;
    expect(own.kind).toBe('taken');
    expect(own.income).toBeGreaterThan(nancy.incomeNow);
  });

  it('names the fallen country that taking its capital would bring back', () => {
    const g = open('FRA');
    take(g, 'Warsaw', 'GER');
    capitulate(g, 'POL', 'GER');
    const w = g.byName.Warsaw.id;
    expect(atWar(g, 'FRA', 'GER')).toBe(true);
    const p = prizeOf(g, w, 'FRA')!;
    expect(p.frees).toBe('POL');
    // The city goes back to the country it frees, not to the nation that takes it.
    expect(p.kind).toBe('restored');
    expect(p.to).toBe('POL');
    expect(p.capitalOf).toBeNull();
    expect(p.items.map((i) => i.key)).toContain('frees');
    expect(p.items.find((i) => i.key === 'income')!.text).toContain("Poland's treasury");
    expect(p.line).toMatch(/^Taking Warsaw frees Poland, which returns to the war on your side and takes its capital back\. Germany loses \d+ a month/);
    // Its conqueror frees nobody by holding it, and nor does a country that is not in the war.
    expect(prizeOf(g, w, 'GER')!.frees).toBeNull();
    expect(prizeOf(g, w, 'SWE')!.frees).toBeNull();
  });

  it('counts oil only where there is an oilfield, and less of it for a conqueror', () => {
    const g = open('GER');
    const field = g.regions.find((r) => r.oil && r.owner !== 'GER' && r.ctrl === r.owner)!;
    const p = prizeOf(g, field.id, 'GER')!;
    expect(p.oilNow).toBeGreaterThan(0);
    expect(p.oil).toBeGreaterThan(0);
    expect(p.oil).toBeLessThanOrEqual(p.oilNow);
    expect(p.items.map((i) => i.key)).toContain('oil');
    expect(prizeOf(g, g.byName.Lodz.id, 'GER')!.oil).toBe(0);
  });

  it('answers for the player through the calls both interfaces use, and for nobody when nobody plays', () => {
    const g = open('GER');
    expect(towns(g)).toEqual(townsOf(g));
    expect(prize(g, g.byName.Warsaw.id)).toEqual(prizeOf(g, g.byName.Warsaw.id, 'GER'));
    expect(prize(open(null), 0)).toBeNull();
    expect(townIn(g, -1)).toBeNull();
    expect(prizeOf(g, 9999, 'GER')).toBeNull();
  });
});

describe('the richest prizes within reach', () => {
  it('lists enemy ground beside the nation or its armies, a capital first', () => {
    const g = open('GER');
    const list = prizesInReach(g, 'GER');
    expect(list.length).toBeGreaterThan(2);
    expect(list.length).toBeLessThanOrEqual(5);
    expect(list[0].town).toBe('Warsaw');
    for (const p of list) {
      const r = g.regions[p.region];
      expect(atWar(g, 'GER', r.ctrl), p.town).toBe(true);
      expect(p.kind === 'taken' || p.kind === 'restored').toBe(true);
      expect(r.nb.some((j) => g.regions[j].ctrl === 'GER' || g.regions[j].units.some((c) => c.nat === 'GER')), p.town).toBe(true);
    }
    expect(prizesNear(g)).toEqual(list);
    expect(prizesNear(g, 2)).toHaveLength(2);
  });

  it('is empty for a country at peace, and for a country that has fallen', () => {
    const g = open('SWE');
    expect(prizesInReach(g, 'SWE')).toEqual([]);
    take(g, 'Warsaw', 'GER');
    capitulate(g, 'POL', 'GER');
    expect(prizesInReach(g, 'POL')).toEqual([]);
  });
});

describe('nothing here changes the war', () => {
  it('leaves the game exactly as it found it', () => {
    const g = open('GER');
    const before = saveGame(g);
    townsOf(g);
    for (const r of g.regions) prizeOf(g, r.id, 'GER');
    prizesInReach(g, 'GER', 10);
    expect(saveGame(g)).toBe(before);
  });

  it('works in a historical scenario too, where a province is worth its industry', () => {
    const g = createGame(3, 'GER');
    const list = towns(g);
    expect(list).toHaveLength(g.regions.length);
    for (const t of list) expect(t.worth, t.name).toBe(g.regions[t.region].fac * BUDGET.scale);
    const p = prize(g, g.byName.Warsaw.id)!;
    expect(p.kind).toBe('taken');
    expect(p.line).toContain('Taking Warsaw');
    expect(p.oil).toBe(0);
    expect(p.rails).toBe(0);
  });
});
