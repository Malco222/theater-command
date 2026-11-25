// The names a division is known by where a dated row's own name does not read as its record does (release 2;
// data/campaign/divisions.json `reads`, armies.ts ownNames).
//
// The design director's rulings of 13:48Z, 13:50Z and 14:28Z, 25 November 2026, on the historian's read of every name
// two rows give (historian/named-twice-reads-1125.md): labels only, no row renamed, since a row's name is its key.
// One division, one name for life (New Zealand's); an American division carries its arm; another army's unit carries
// that army's name; a part of a division laid as a row of its own reads as itself. Read with the far side on, so that
// its rows are laid over the files as the game lays them (far-rows.ts).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import '../src/campaign/far-rows';
import divisions from '../data/campaign/divisions.json';
import diplomacyRows from '../data/campaign/diplomacy.json';
import legionRows from '../data/campaign/legions.json';
import openRows from '../data/campaign/open.json';
import pacificRows from '../data/campaign/pacific.json';
import setup from '../data/campaign/setup-1939.json';
import siegeRows from '../data/campaign/siege.json';
import worldRows from '../data/campaign/world.json';
import { divisionsNamed, divisionsOf, namedDivisions } from '../src/campaign/armies';
import { createGame, estAt } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { KIND, type Kind } from '../src/campaign/types';

const READS = (divisions as any).reads as Record<string, Record<string, [string, string][]>>;

interface Row { nat: string; name: string; kind: Kind; divs: number; o: Record<string, any> }
/** Every row of the data that names a formation of a country and an arm, as armies.ts reads them: [nation, name, kind, divisions, ...], dated or not. */
function rowsOf(x: unknown, out: Row[] = []): Row[] {
  if (Array.isArray(x)) {
    const at = typeof x[0] === 'number' ? 1 : 0;
    const nat = x[at], name = x[at + 1], kind = x[at + 2], divs = x[at + 3];
    if (typeof nat === 'string' && /^[A-Z]{2,3}$/.test(nat) && typeof name === 'string' && typeof kind === 'string' && kind in KIND && typeof divs === 'number') {
      out.push({ nat, name, kind: kind as Kind, divs, o: (x.find((y) => y && typeof y === 'object' && !Array.isArray(y)) as Record<string, any>) ?? {} });
    }
    for (const y of x) if (y && typeof y === 'object') rowsOf(y, out);
  } else if (x && typeof x === 'object') for (const y of Object.values(x)) if (y && typeof y === 'object') rowsOf(y, out);
  return out;
}
const ALL = [setup, worldRows, diplomacyRows, siegeRows, openRows, pacificRows, legionRows].flatMap((f) => rowsOf(f));
/** The fullest a row ever gets: under 1, it is a part and not a division. */
const fullest = (r: Row): number => (Array.isArray(r.o.est) ? Math.max(...(r.o.est as [number, number][]).map(([m]) => estAt(r.o.est, m) ?? 1)) : r.o.est ?? 1);

describe('the names the rule reads (the design director, 14:28Z of 25 November 2026: labels only, no row renamed)', () => {
  it('each entry is the name of a row of its country, with a name for each of the row\'s divisions and none given twice', () => {
    for (const [nat, table] of Object.entries(READS)) for (const [name, list] of Object.entries(table)) {
      const rows = ALL.filter((r) => r.nat === nat && r.name === name);
      expect(rows.length, `${nat} "${name}" is some row's name`).toBeGreaterThan(0);
      for (const r of rows) {
        expect(list.length, `${nat} "${name}"`).toBeGreaterThanOrEqual(r.divs);
        expect(divisionsNamed(name, nat, r.kind), `${nat} "${name}"`).toEqual(list);
      }
      expect(new Set(list.map(([full]) => full)).size, `${nat} "${name}"`).toBe(list.length);
    }
  });

  it('New Zealand\'s division is one name in every row that gives it', () => {
    const rows = ALL.filter((r) => r.nat === 'UK' && /New Zealand/.test(r.name) && fullest(r) >= 1);
    expect(rows.length).toBeGreaterThanOrEqual(4);
    for (const r of rows) {
      const given = divisionsNamed(r.name, 'UK', r.kind).slice(0, r.divs).map(([full]) => full).filter((n) => /New Zealand/.test(n));
      expect(given, r.name).toEqual(['New Zealand Division']);
    }
  });

  it('an American division carries its arm, and a British division in an American row carries its army\'s name', () => {
    const xx = divisionsNamed('Third Army: XX Corps (7th Armored, 5th and 90th Infantry Divisions)', 'USA', 'inf').map(([full]) => full);
    expect(xx).toEqual(['7th Armored Division', '5th Infantry Division', '90th Infantry Division']);
    const vi5 = divisionsNamed('Fifth Army: VI Corps (3rd and 45th Infantry, 1st Armored and British 1st and 5th Divisions)', 'USA', 'inf').map(([full]) => full);
    expect(vi5).toContain('British 5th Division');
    expect(vi5).not.toContain('5th Division');
  });

  it('a part of a division laid as a row of its own reads as itself, never as the division it belongs to', () => {
    const parts = ALL.filter((r) => fullest(r) < 1 && r.divs === 1 && READS[r.nat]?.[r.name]);
    expect(parts.map((r) => r.name)).toEqual(expect.arrayContaining(['9th Indian Division (8th Brigade)', '9th Indian Division (22nd Brigade)', 'Kimura Detachment (16th Division)', '16th Division (9th Infantry)']));
    for (const r of parts) {
      const [full] = divisionsNamed(r.name, r.nat, r.kind)[0];
      expect(full, r.name).not.toMatch(/ Division$/);
    }
    // The two brigades of the 9th Indian, and the two parts of Japan's 16th beside the body of it, each read apart.
    const one = (nat: string, name: string): string => divisionsNamed(name, nat, 'inf')[0][0];
    expect(one('UK', '9th Indian Division (8th Brigade)')).toBe('8th Indian Brigade');
    expect(one('UK', '9th Indian Division (22nd Brigade)')).toBe('22nd Indian Brigade');
    expect(one('JAP', 'Kimura Detachment (16th Division)')).toBe('Kimura Detachment');
    expect(one('JAP', '16th Division (9th Infantry)')).toBe('9th Infantry Regiment');
  });

  it('another army\'s divisions never read as Britain\'s own: no Chinese row gives a bare British number', () => {
    const chinese = ALL.filter((r) => r.nat === 'UK' && /^Chinese /.test(r.name));
    expect(chinese.length).toBeGreaterThanOrEqual(5);
    for (const r of chinese) for (const [full] of divisionsNamed(r.name, 'UK', r.kind)) expect(full, r.name).toMatch(/^Chinese /);
  });

  it('no two whole rows of one country that the table names give the same division, but for the rows that are the same division at another date', () => {
    // (New Zealand's rows are one division by ruling: the rule finds it held and lays it once.)
    const same = (n: string): boolean => n === 'New Zealand Division' || n === '4th Indian Division';
    for (const [nat, table] of Object.entries(READS)) {
      const seen = new Map<string, string>();
      for (const [name, list] of Object.entries(table)) {
        const rows = ALL.filter((r) => r.nat === nat && r.name === name);
        if (!rows.length || rows.every((r) => fullest(r) < 1)) continue;
        for (const [full] of list.slice(0, Math.max(...rows.map((r) => r.divs)))) {
          if (!same(full) && !(nat === 'USA')) expect(seen.get(full), `${nat} ${full}: "${name}"`).toBeUndefined();
          seen.set(full, name);
        }
      }
    }
  });
});

describe('outside the grand campaign every name reads as it always did (a review of the rule, 25 November 2026)', () => {
  const HISTORIC = (divisions as any).historic as Record<string, Record<string, [string, string][]>>;
  it('the table changes no label of a historical scenario: its cards and battle tickets carry the names they were measured with', () => {
    let differ = 0;
    for (const [nat, table] of Object.entries(READS)) for (const [name, list] of Object.entries(table)) for (const r of ALL.filter((x) => x.nat === nat && x.name === name)) {
      const old = HISTORIC[nat]?.[name] ?? namedDivisions(name, nat, r.kind);
      expect(divisionsNamed(name, nat, r.kind, false), `${nat} "${name}"`).toEqual(old);
      if (JSON.stringify(old) !== JSON.stringify(list)) differ++;
    }
    // (The table is there because the two differ: were they the same everywhere this would prove nothing.)
    expect(differ).toBeGreaterThan(10);
  });

  it('a corps\' cards read by the table in the grand campaign and by the name alone in a scenario', () => {
    const NAME = 'New Zealand Corps (2nd New Zealand and 4th Indian Divisions)';
    const cards = (open: boolean): string[] => {
      const g = open ? startOpen(createGame(1, null)) : createGame(1, null);
      const like = g.corps.find((c) => c.nat === 'UK' && !c.dead && c.kind === 'inf' && c.divs === 2)!;
      const c = { ...like, name: NAME };
      delete (c as any).names;
      return divisionsOf(g, c).map((d) => d.name);
    };
    expect(cards(true)).toEqual(['New Zealand Division', '4th Indian Division']);
    expect(cards(false)).toEqual(['2nd New Zealand Division', '4th Indian Division']);
  });
});
