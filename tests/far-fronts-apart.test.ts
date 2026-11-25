// The far side: each enemy country's ground is a front of its own (the design director's rulings of 05:10Z and 05:45Z
// of 21 November 2026, form (a), on the second rules builder's probe of the 1944 choices; staff.ts fronts).
//
// fronts() made one front of every piece of enemy ground joined by land, whoever held it. Before a Soviet war that
// joined China's main ground with Burma and India; once the Soviet Union was at war with Japan it joined the Soviet
// frontier, Mongolia, all of China, Burma and India into one front keyed to the Red Army, and a dated choice in Hunan
// was weighed against it: in three wars the 1944 push was taken up and dropped the month the Soviet war opened. On
// the far side of the world two neighbouring enemy provinces under different holders now stand apart, from the first
// day, as the record's commands did. Where either province stands on the old chart, or in the Americas, they are
// joined as they were. (What a front may use was what the others spared; since one membership, the design
// director's 14:08Z of 21 November 2026, a far-side front counts its members only.) Behind the far switch; no number.
// The states are set by hand; no war is played here.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { fronts, startOpen, type Front } from '../src/campaign/staff';
import { shoreOf } from '../src/campaign/world';
import type { Game } from '../src/campaign/types';

const war = (): Game => startOpen(createGame(5, null));
const fight = (g: Game, nat: string, foes: string[]): void => { for (const foe of foes) if (!atWar(g, nat, foe)) declareWar(g, nat, foe); };
/** A front as its figures: what a staff reads of it. */
const read = (f: Front): unknown => ({ key: f.key, enemies: [...f.enemies].sort(), edge: [...f.edge].sort((a, b) => a - b), ours: [...f.ours].sort((a, b) => a - b), atk: f.atk, def: f.def, threat: f.threat, fixed: f.fixed, ratio: f.ratio, near: f.near });
const names = (fs: Front[]): string[][] => fs.map((f) => [...f.enemies].sort());
/** The front whose edge holds a province. */
const frontAt = (g: Game, fs: Front[], place: string): Front => fs.find((f) => f.edge.includes(g.byName[place].id))!;

describe("on the far side each enemy country's ground is a front of its own", () => {
  it("Japan at war with the Soviet Union and Mongolia as well as China: China's front is its own, with the defence it had", () => {
    const before = war();
    const was = frontAt(before, fronts(before, 'JAP'), 'Loyang');
    const g = war();
    fight(g, 'JAP', ['SOV', 'MGL']);
    // Joined, as it was: one front of China, Mongolia and the Soviet Union.
    const joined = fronts(g, 'JAP', { joined: true });
    expect(names(joined)).toContainEqual(['CHN', 'MGL', 'SOV']);
    expect(frontAt(g, joined, 'Loyang')).toBe(frontAt(g, joined, 'Vladivostok'));
    // Apart: no front names China with either, and China's main front has the edge and the defence it had before the
    // Soviet war. The Soviet coast and Mongolia are fronts of their own.
    const apart = fronts(g, 'JAP');
    for (const f of apart) expect(f.enemies.includes('CHN') && (f.enemies.includes('SOV') || f.enemies.includes('MGL')), f.enemies.join()).toBe(false);
    const china = frontAt(g, apart, 'Loyang'), coast = frontAt(g, apart, 'Vladivostok'), steppe = frontAt(g, apart, 'Choibalsan');
    expect(new Set([china, coast, steppe]).size).toBe(3);
    expect([...china.edge].sort()).toEqual([...was.edge].sort());
    expect(china.def).toBeCloseTo(was.def, 9);
    expect(china.enemies).toEqual(['CHN']);
    // One membership (the design director's 14:08Z and 14:31Z of 21 November 2026): a front counts its members and
    // nobody else's, and no front keeps cover out of another's. The armies the Soviet fronts take for theirs, the
    // Kwantung Army and Korea's, hold their posts far from China's line and were never in its reading, so China's
    // front reads to the last figure what it read before the Soviet war, and over what the joined front reads.
    expect(china.members).toBe(was.members);
    expect(china.ratio).toBe(was.ratio);
    expect(china.ratio).toBe(china.members! / china.def);
    expect(china.ratio).toBeGreaterThan(frontAt(g, joined, 'Loyang').ratio);
    // The Soviet coast's front reads on its own members, the rows that stand before it: thin, however full China is.
    expect(coast.members).toBeGreaterThan(0);
    expect(coast.members!).toBeLessThan(0.1 * china.members!);
    expect(coast.ratio).toBe(coast.members! / coast.def);
  });

  it('a staff with one enemy on the far side reads the same either way: Japan and China on the first day', () => {
    const g = war();
    expect(fronts(g, 'JAP').map(read)).toEqual(fronts(g, 'JAP', { joined: true }).map(read));
    // (China's main front no longer counts Manchukuo's ground behind Japan's as part of it; its edge is the same.)
    const apart = fronts(g, 'CHN'), joined = fronts(g, 'CHN', { joined: true });
    expect(apart.map((f) => [...f.edge].sort())).toEqual(joined.map((f) => [...f.edge].sort()));
    expect(names(apart)).not.toContainEqual(['JAP', 'MAN']);
  });

  it("where either province stands on the old chart they are joined as they were: Europe's fronts on the old chart read the same to the last figure", () => {
    const g = war();
    let seen = 0;
    for (const n of Object.values(g.nations)) {
      if (!n.alive || n.capital < 0 || shoreOf(n.capital) !== 0 || !n.atWar.length) continue;
      const apart = fronts(g, n.id), joined = fronts(g, n.id, { joined: true });
      expect(apart.map(read), n.id).toEqual(joined.map(read));
      seen += apart.length;
    }
    expect(seen).toBeGreaterThan(4);
    // Germany's two fronts of the first day, Poland's and France's, and Poland's own that names Germany with Slovakia.
    expect(names(fronts(g, 'GER'))).toEqual([['FRA'], ['POL']]);
    expect(names(fronts(g, 'POL'))).toContainEqual(['GER', 'SVK']);
    // Italy at war with Britain and France: the Alps read the same.
    fight(g, 'ITA', ['UK', 'FRA']);
    const apart = fronts(g, 'ITA'), joined = fronts(g, 'ITA', { joined: true });
    expect(read(frontAt(g, apart, 'Riviera'))).toEqual(read(frontAt(g, joined, 'Riviera')));
  });

  it("far-side ground parts for every staff: Italy's front in Africa, where France's Chad meets Britain's Sudan beyond the old chart", () => {
    // (The design director's ruling of 13:06Z of 21 November 2026: far-side ground parts by enemy country for every
    // staff, and the split in Africa is the record's. In 1940 Italy's Fifth Army faced France in Tripolitania and
    // Tunisia and its Tenth faced Britain from Cyrenaica into Egypt, under separate commands; one African front
    // against both, joined through Chad and the Sudan, was the old chart's join reaching across ground it never held.)
    const g = war();
    fight(g, 'ITA', ['UK', 'FRA']);
    const apart = fronts(g, 'ITA'), joined = fronts(g, 'ITA', { joined: true });
    expect(frontAt(g, joined, 'Mareth')).toBe(frontAt(g, joined, 'Sidi Barrani'));
    expect(frontAt(g, apart, 'Mareth')).not.toBe(frontAt(g, apart, 'Sidi Barrani'));
    expect(frontAt(g, apart, 'Mareth').enemies).toEqual(['FRA']);
    expect(apart.length).toBe(joined.length + 1);
  });
});
