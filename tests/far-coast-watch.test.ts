// The far side: the coast watch does not expect by sea an enemy already at grips with the watcher on its own home
// ground (the design director's ruling of 04:07Z of 21 November 2026, form (b), on the second rules builder's trace
// of the 1941 coast: staff.ts coastWatch, atGrips).
//
// The old chart's test asks only whether the enemy's army AT HOME has a land front with the watcher. Japan's home
// army is on its islands, so China read Japan as coming by sea alone and, at the book's stops where the line keeps
// its front quiet, stood a field army in each of its best harbours from the first year: the record's coast was held
// by a division or less in a port, with the armies inland. Behind the far switch, every stop, no number.
//
// Kept to a watcher whose own seat stands on the far side of the world (the design director's 06:05Z of the same
// day): the first read of the six moved Europe's watches too, and Europe's watches stay as they were. The general
// form is after-release item 18.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, createGame, removeCorps } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { coastWatch, gripsWatch, staffPhase, startOpen } from '../src/campaign/staff';
import { shoreOf } from '../src/campaign/world';
import type { Game } from '../src/campaign/types';

const war = (stop = 3): Game => { const g = startOpen(createGame(5, null)); if (stop !== 3) setStop(g, stop, true); return g; };
/** Whom a watch passed over as at grips with the watcher, while `f` ran. */
const told = (f: () => void): string[] => {
  const out: string[] = [];
  gripsWatch.tell = (e) => { out.push(`${e.nat} passes over ${e.foe}`); };
  try { f(); } finally { gripsWatch.tell = null; }
  return [...new Set(out)];
};
/** Japan's field troops off China's own ground: the war goes on, and Japan keeps the harbours it holds. */
const clearChina = (g: Game): void => { for (const c of [...g.corps]) if (c.nat === 'JAP' && !c.dead && c.kind !== 'fort' && g.regions[c.reg].owner === 'CHN') removeCorps(g, c); };

describe('a country at grips with an enemy on its own home ground does not expect him by sea', () => {
  it("China, a quiet year into its war: Japan's army stands on China's ground in touch with it, and no harbour is watched, at every stop", () => {
    for (const stop of [1, 2, 3]) {
      const g = war(stop);
      g.turn = 14;
      let watch: Record<string, number> = {};
      expect(told(() => { watch = coastWatch(g, 'CHN'); }), `stop ${stop}`).toEqual(['CHN passes over JAP']);
      expect(watch, `stop ${stop}`).toEqual({});
      const ph = staffPhase(g, 'CHN');
      expect(ph?.watch ?? [], `stop ${stop}`).toEqual([]);
    }
  });

  it("with Japan's army off China's own ground the old test stands: Japan is expected by sea and China's harbours are watched", () => {
    const g = war(1);
    clearChina(g);
    g.turn = 14;
    let watch: Record<string, number> = {};
    expect(told(() => { watch = coastWatch(g, 'CHN'); })).toEqual([]);
    const names = Object.keys(watch);
    expect(names.length).toBeGreaterThanOrEqual(2);
    for (const nm of names) { const r = g.byName[nm]; expect([r.owner, r.ctrl, !!r.port, r.overseas], nm).toEqual(['CHN', 'CHN', true, false]); }
  });

  it('ground overseas is not home ground: an enemy standing in a colony is still expected at home by sea', () => {
    const g = war(1);
    // (Japan's army in China is at grips with China; Hong Kong, Britain's ground overseas, makes nobody at grips with Britain.)
    g.turn = 14;
    const said = told(() => { for (const nat of Object.keys(g.nations)) if (g.nations[nat].alive && g.nations[nat].atWar.length) coastWatch(g, nat); });
    expect(said).toEqual(['CHN passes over JAP']);
  });

  it("kept to a watcher whose seat stands on the far side: a watcher of the old chart at grips on its own ground keeps the old test", () => {
    const g = war(1);
    g.turn = 14;
    // (Whom the mend can reach: the countries whose seats stand beyond the old chart, on the far side. Europe's, the
    // Soviet Union's and the Americas' are not among them.)
    const far = Object.values(g.nations).filter((n) => n.alive && n.capital >= 0 && shoreOf(n.capital) === 2).map((n) => n.id).sort();
    expect(far).toEqual(['AFG', 'AUS', 'CHN', 'IRN', 'JAP', 'LBR', 'MAN', 'MGL', 'NEP', 'NZL', 'SAF', 'SAU', 'THA', 'TIB', 'YEM']);
    // Britain watches its harbours for Germany. A German corps set down in London, Britain's own ground and in its
    // hands, is at grips with it by the far side's test: Britain is told nothing and its watch stands as it was.
    let before: Record<string, number> = {};
    expect(told(() => { before = coastWatch(g, 'UK'); })).toEqual([]);
    expect(Object.keys(before).length).toBeGreaterThanOrEqual(2);
    const london = g.byName.London;
    expect([london.owner, london.ctrl, london.overseas, shoreOf(london)]).toEqual(['UK', 'UK', false, 0]);
    addCorps(g, 'GER', 'Test corps', 'inf', 2, 'London', {});
    let after: Record<string, number> = {};
    expect(told(() => { after = coastWatch(g, 'UK'); })).toEqual([]);
    expect(after).toEqual(before);
  });
});
