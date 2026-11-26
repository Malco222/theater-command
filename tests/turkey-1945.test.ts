// Ruling 228, item 17 (the design director's b528): Turkey's war of February 1945 is a declaration, not a front. By
// the record Turkey declared on 23 February 1945 to sit at the founding conference of the United Nations, with
// Germany beaten and no Axis army near its frontier; no Turkish soldier fought. At stops 1 and 2 it declares in that
// month if no Axis army stands on Turkish ground or ground bordering it; at stops 3 to 5 from that month on, once
// Allied armies also stand on Germany's home ground. A computer Turkey's corps then keep to Turkish ground, and a
// player's Turkey is never moved.
import { describe, expect, it } from 'vitest';
import { holds, monthlyDiplomacy } from '../src/campaign/diplomacy';
import { atWar, createGame, relocate, setWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { blocked, keepsHome } from '../src/campaign/rivers';
import { startOpen } from '../src/campaign/staff';
import type { Game, Region } from '../src/campaign/types';

function war(stop: number, human: string | null = null): Game {
  const g = startOpen(createGame(3, human));
  setStop(g, stop, true);
  setWar(g, 'GER', 'UK');
  return g;
}
function at(g: Game, turn: number): void {
  g.turn = turn;
  monthlyDiplomacy(g);
}
const turkish = (g: Game): Region[] => g.regions.filter((r) => r.owner === 'TUR');
/** Ground outside Turkey that borders it. */
const border = (g: Game): Region[] => {
  const home = new Set(turkish(g).map((r) => r.id));
  return g.regions.filter((r) => !home.has(r.id) && r.nb.some((j) => home.has(j)));
};
const told = (g: Game): string[] => g.headlines.filter((h) => /Turkey declares war/.test(h.text)).map((h) => h.text);

describe("Turkey's declaration of February 1945", () => {
  it('comes in that month at stops 1 and 2, with no Axis army near its frontier', () => {
    for (const stop of [1, 2]) {
      const g = war(stop);
      at(g, 65);
      expect(atWar(g, 'TUR', 'GER'), `stop ${stop}`).toBe(false);
      at(g, 66);
      expect(atWar(g, 'TUR', 'GER'), `stop ${stop}`).toBe(true);
      expect(g.nations.TUR.align, `stop ${stop}`).toBe('allies');
      expect(g.flags['stay:TUR'], `stop ${stop}`).toBe(66);
      expect(told(g), `stop ${stop}`).toEqual(['Turkey declares war on Germany and Japan. No Turkish army will march: the declaration buys Ankara a seat at the founding conference of the United Nations.']);
    }
  });

  it('does not come at stops 1 and 2 while an Axis army stands on ground bordering Turkey, nor later', () => {
    const g = war(1);
    const next = border(g).find((r) => !r.units.length && r.ctrl !== 'UK')!;
    expect(next).toBeTruthy();
    relocate(g, g.corps.find((c) => c.nat === 'GER' && !c.dead && !c.town && c.kind === 'inf')!, next);
    expect(holds(g, 'noArmyNear:axis,TUR')).toBe(false);
    at(g, 66);
    expect(atWar(g, 'TUR', 'GER')).toBe(false);
    for (const c of [...next.units]) c.dead = true;
    at(g, 67);
    expect(atWar(g, 'TUR', 'GER')).toBe(false);
  });

  it('waits at stops 3 to 5 until Allied armies stand on German soil', () => {
    for (const stop of [3, 5]) {
      const g = war(stop);
      at(g, 66);
      expect(atWar(g, 'TUR', 'GER'), `stop ${stop}`).toBe(false);
      const soil = g.regions.find((r) => r.owner === 'GER' && !r.overseas && !r.units.length)!;
      relocate(g, g.corps.find((c) => c.nat === 'UK' && !c.dead && !c.town && c.kind === 'inf')!, soil);
      expect(holds(g, 'armyOn:allies,GER'), `stop ${stop}`).toBe(true);
      at(g, 70);
      expect(atWar(g, 'TUR', 'GER'), `stop ${stop}`).toBe(true);
      expect(g.flags['stay:TUR'], `stop ${stop}`).toBe(70);
    }
  });

  it("keeps a computer Turkey's corps on Turkish ground once it has declared", () => {
    const g = war(1);
    at(g, 66);
    const c = g.corps.find((x) => x.nat === 'TUR' && !x.dead && !x.town)!;
    const out = g.regions[c.reg].nb.map((j) => g.regions[j]).find((r) => r.owner !== 'TUR');
    const home = g.regions[c.reg].nb.map((j) => g.regions[j]).find((r) => r.owner === 'TUR');
    if (out) { expect(keepsHome(g, c, out)).toBe(true); expect(blocked(g, c, out)).toBe(true); }
    if (home) expect(keepsHome(g, c, home)).toBe(false);
    expect(out ?? home).toBeTruthy();
    // Before it declared, nothing held it.
    const before = war(1);
    const b = before.corps.find((x) => x.nat === 'TUR' && !x.dead && !x.town)!;
    for (const j of before.regions[b.reg].nb) expect(keepsHome(before, b, before.regions[j])).toBe(false);
  });

  it("never moves a player's Turkey", () => {
    const g = war(1, 'TUR');
    const align = g.nations.TUR.align;
    at(g, 66);
    expect(atWar(g, 'TUR', 'GER')).toBe(false);
    expect(g.nations.TUR.align).toBe(align);
    expect(g.flags['stay:TUR']).toBeUndefined();
  });
});
