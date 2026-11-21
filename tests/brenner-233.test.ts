// Ruling 233, noted 10:33Z (historian/brenner-railway-233.md): the Brenner railway. The line ran from Munich by
// Kufstein and Innsbruck over the pass to Bolzano, Trento and Verona, and on over the Po to Bologna; at Verona it met
// the Milan line by Brescia. The map had Bavaria to Tyrol and Alto Adige to Veneto, but not the pass itself, so a
// German train reached Italy only through Switzerland, Savoy or Slovenia, none of them a way troops went. The pass
// (Tyrol to Alto Adige) and Verona to Milan (Veneto to Lombardy) are laid in data/campaign/rails.mjs, open all war:
// bombing cuts them like any other line. Styria to Friuli is not laid: on our map Carinthia is not inside Styria
// (Villach and the whole Pontebbana lie in Friuli, Klagenfurt in Slovenia), so the condition the ruling set fails.
import { describe, expect, it } from 'vitest';
import { keepWards } from '../src/campaign/ai';
import { addCorps, allied, createGame, declareWar, railMove, railReach, relocate } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { RAIL_LINKS, railLinks, railsAt } from '../src/campaign/rails';
import { startOpen } from '../src/campaign/staff';
import type { Game, PlanPhase, Region } from '../src/campaign/types';

/** The ways a train ran into Italy before the pass was laid, none of them open to German troops. */
const NOT_THE_WAY = ['Zurich', 'Bern', 'Graubunden', 'Savoy', 'Slovenia'];

/** A grand campaign (the whole world, or Europe alone) with Italy in the war beside Germany. */
function axis(wide: boolean): Game {
  const g = startOpen(createGame(3, null), wide);
  declareWar(g, 'ITA', 'UK');
  expect(allied(g, 'GER', 'ITA')).toBe(true);
  return g;
}
/** Cuts a province's line as the game records it. */
const sever = (g: Game, r: Region): void => { g.flags[`rail:${r.id}`] = g.turn; };
const linked = (g: Game, a: string, b: string): boolean => {
  const p = g.byName[a].id, q = g.byName[b].id;
  return railLinks(g).some((l) => l.a === Math.min(p, q) && l.b === Math.max(p, q));
};
/** Where a fresh German rifle corps set down in a province can go by train this month, by province name. */
const reachFrom = (g: Game, at: string): Set<string> => {
  const c = addCorps(g, 'GER', `Test Brenner Corps ${g.nextCorps}`, 'inf', 3, at, {});
  return new Set([...railReach(g, c)].map((id) => g.regions[id].name));
};

describe('ruling 233: the Brenner railway', () => {
  it('lays the pass (Tyrol to Alto Adige) and Verona to Milan (Veneto to Lombardy) in every campaign', () => {
    for (const wide of [true, false]) {
      const g = axis(wide);
      expect(linked(g, 'Tyrol', 'Alto Adige')).toBe(true);
      expect(linked(g, 'Lombardy', 'Veneto')).toBe(true);
      // The rest of the line was there already: Munich to Innsbruck, Bolzano to Verona, and Verona over the Po.
      for (const [a, b] of [['Bavaria', 'Tyrol'], ['Alto Adige', 'Veneto'], ['Veneto', 'Emilia']]) expect(linked(g, a, b), `${a} to ${b}`).toBe(true);
      expect(railsAt(g.byName.Tyrol.id)).toContain(g.byName['Alto Adige'].id);
      expect(railsAt(g.byName.Veneto.id)).toContain(g.byName.Lombardy.id);
    }
    // Both are Europe's lines, the same in every game.
    expect(RAIL_LINKS.filter((l) => [[32, 146], [140, 143]].some(([a, b]) => l.a === a && l.b === b))).toHaveLength(2);
  });

  it('runs a German train from Munich over the pass to Verona and the Po, at every stop, with Switzerland, Savoy and Slovenia shut', () => {
    for (const wide of [true, false]) {
      const g = axis(wide);
      // None of the old ways is open to a German train anyway, and here each is cut as well.
      for (const name of NOT_THE_WAY) {
        expect(allied(g, 'GER', g.byName[name].ctrl), name).toBe(false);
        sever(g, g.byName[name]);
      }
      for (const stop of [1, 2, 3, 4, 5]) {
        expect(setStop(g, stop, true)).toBe(true);
        for (const from of ['Bavaria', 'Tyrol']) {
          const reach = reachFrom(g, from);
          for (const to of ['Alto Adige', 'Veneto', 'Emilia', 'Lombardy']) expect(reach.has(to), `${from} to ${to}, stop ${stop}`).toBe(true);
          for (const name of NOT_THE_WAY) expect(reach.has(name), `${from} through ${name}`).toBe(false);
        }
      }
      // And the corps goes: Munich to Verona in the month.
      const c = addCorps(g, 'GER', 'Test Brenner Rider', 'inf', 3, 'Bavaria', {});
      expect(railMove(g, c, g.byName.Veneto)).toBe(true);
      expect(g.regions[c.reg].name).toBe('Veneto');
    }
  });

  it('the pass is the only way in: with Alto Adige\'s line cut, no German train reaches Italy', () => {
    const g = axis(true);
    sever(g, g.byName['Alto Adige']);
    const reach = reachFrom(g, 'Bavaria');
    expect([...reach].filter((name) => g.byName[name].owner === 'ITA')).toEqual([]);
  });

  it('runs Verona to Milan straight, without going round by Bologna', () => {
    const g = axis(true);
    sever(g, g.byName.Emilia);
    expect(reachFrom(g, 'Veneto').has('Lombardy')).toBe(true);
    expect(reachFrom(g, 'Bavaria').has('Lombardy')).toBe(true);
  });

  it('takes the ward\'s guard by train over the pass to Rome in its first month (rider b627: it was set down in Austria)', () => {
    const g = axis(true);
    const uk = addCorps(g, 'UK', 'Test Landing Corps', 'inf', 2, 'London', {});
    relocate(g, uk, g.byName.Calabria);
    for (const name of NOT_THE_WAY) sever(g, g.byName[name]);
    const ward: PlanPhase = { from: 1, to: 99, posture: 'defend', ward: { Rome: 1 } };
    keepWards(g, 'GER', ward, new Set());
    const c = g.corps.find((x) => x.id === g.flags['ward:GER:Rome:0'])!;
    expect(c.nat).toBe('GER');
    expect(g.regions[c.reg].name).toBe('Rome');
    expect(c.ap).toBe(0);
  });

  it('lays no line from Styria to Friuli: Carinthia is not inside our Styria, so the ruling\'s condition fails', () => {
    const g = axis(true);
    expect(linked(g, 'Styria', 'Friuli')).toBe(false);
  });
});
