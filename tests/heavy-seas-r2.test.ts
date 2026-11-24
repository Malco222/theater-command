// Heavy seas (release 2, heavyseas.ts; Brian 24 November 2026 20:38Z, "yes to all"): gales blow under a closed sky at
// sea, mostly in winter. Ships can sail into a sea in a gale but not on through it that month, convoys through one are
// late, and a shadow may lose its quarry in one. The same for every country. Behind its own switch.
import { describe, expect, it } from 'vitest';
import stage from '../data/campaign/stage.json';
import { createGame } from '../src/campaign/game';
import { detach } from '../src/campaign/fleets';
import { HEAVY, galeConvoyLoss, galeLine, galeNews, galeSeas, heavySeasOn, setHeavySeas } from '../src/campaign/heavyseas';
import { setFog } from '../src/campaign/intel';
import { fleetReach, laneLoss, seaIncomeLoss } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { setShadow, shadowsFollow } from '../src/campaign/shadow';
import { HEAVYSEAS_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, orderShadowFleet, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import { dealtSky, setWeather, skyAtSea, zoneOfSea } from '../src/campaign/weather';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

function seat(on: boolean): Game {
  const g = startOpen(createGame(4, 'UK'));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  setHeavySeas(g, on);
  setFog(g, false);
  beginHuman(g);
  return g;
}
const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const sea = (g: Game, name: string): number => g.seas.findIndex((z) => z.name === name);
/** The same campaign in another month (seed 4: October 1940 has gales over the Irish Sea and the North Sea, December 1940 over the North Atlantic and the Western Approaches, August 1940 none). */
const at = (g: Game, turn: number): Game => { const h = loadGame(saveGame(g))!; h.turn = turn; return h; };

describe('the switch', () => {
  it('ships off, and off no sea has a gale and nothing moves', () => {
    expect((stage as { heavySeas?: boolean }).heavySeas).toBe(false);
    expect(HEAVYSEAS_STAGE).toBe(false);
    const g = at(seat(false), 14);
    expect(heavySeasOn(g)).toBe(false);
    expect(galeSeas(g).size).toBe(0);
    const home = fleet(g, 'UK', 'Home Fleet');
    expect(fleetReach(g, home)).toEqual(fleetReach(g, home, true));
    expect(galeConvoyLoss(g, 'UK')).toBe(0);
    expect(galeLine(g, home)).toBe('');
    const before = saveGame(g);
    galeNews(g, 'UK');
    expect(saveGame(g)).toBe(before);
  });
});

describe('a gale', () => {
  it('blows under a closed sky, the same deal the weather reads', () => {
    const g = at(seat(true), 14);
    const gales = [...galeSeas(g)];
    expect(gales).toEqual(expect.arrayContaining([sea(g, 'Irish Sea'), sea(g, 'North Sea')]));
    for (const z of gales) expect(dealtSky(g, zoneOfSea(g, z))).toBe('closed');
    setWeather(g, true);
    for (const z of gales) expect(skyAtSea(g, z)).toBe('closed');
    expect(galeSeas(at(g, 13)).size).toBe(0);
  });

  it('can be sailed into but not on through: the seas beyond it are out of reach that month', () => {
    const g = at(seat(true), 14);
    const home = fleet(g, 'UK', 'Home Fleet');
    const calm = fleetReach(g, home, true), now = fleetReach(g, home);
    // From Scapa Flow: into the North Sea and the Irish Sea, in their gale, but not on to the Skagerrak and the Baltic.
    for (const n of ['North Sea', 'Irish Sea']) expect(now).toContain(sea(g, n));
    for (const n of ['Skagerrak', 'Baltic Sea']) { expect(calm).toContain(sea(g, n)); expect(now).not.toContain(sea(g, n)); }
    for (const z of now) expect(calm).toContain(z);
    expect(galeLine(g, home)).toMatch(/A gale blows over the North Sea this month/);
    // A tied fleet's beat is its beat whatever the weather: the calm reach.
    expect(fleetReach(g, home, true)).toEqual(fleetReach(at(seat(false), 14), fleet(at(seat(false), 14), 'UK', 'Home Fleet')));
  });

  it('makes the convoys through it late, and the lanes lose more of their cargoes', () => {
    const on = at(seat(true), 14), off = at(seat(false), 14);
    expect(galeConvoyLoss(on, 'UK')).toBe(HEAVY.convoy);
    expect(seaIncomeLoss(on, 'UK')).toBeGreaterThan(seaIncomeLoss(off, 'UK'));
    const irish = [sea(on, 'Irish Sea')];
    expect(laneLoss(on, 'UK', irish).loss - laneLoss(off, 'UK', irish).loss).toBeCloseTo(HEAVY.lane);
    // Italy's convoys to Libya cross no gale that month.
    expect(galeConvoyLoss(on, 'ITA')).toBe(0);
  });

  it('is in his news as his month opens, where it blows over his seas', () => {
    const g = at(seat(true), 14);
    const n = g.headlines.length;
    galeNews(g, 'UK');
    expect(g.headlines.slice(n).map((h) => h.text).join(' ')).toMatch(/Gales this month over the Irish Sea and the North Sea: ships can sail into them but not on through them\. Convoys through them are late/);
  });

  it('loses a shadow its quarry some of the time; out of a gale, never', () => {
    // (The shadows are set in calm weather: in September 1939 a gale over the Skagerrak keeps the Baltic out of reach.)
    const g = seat(false);
    setShadow(g, true);
    const home = fleet(g, 'UK', 'Home Fleet'), battle = fleet(g, 'GER', 'Battle Fleet');
    const squads: Fleet[] = [];
    for (let k = 0; k < 6; k++) {
      const sq = detach(g, home, { ca: 2 }, `Cruiser Squadron ${k + 1}`)!;
      expect(orderShadowFleet(g, sq.id, battle.id)).toBe(true);
      squads.push(sq);
    }
    const run = (turn: number): { lost: number; news: string[] } => {
      const h = at(g, turn);
      setHeavySeas(h, true);
      const b = fleet(h, 'GER', 'Battle Fleet');
      b.zone = sea(h, 'Western Approaches'); b.stance = 'patrol';
      const n = h.headlines.length;
      shadowsFollow(h, 'UK');
      return { lost: squads.filter((s) => h.fleets.find((x) => x.id === s.id)!.shadow === undefined).length, news: h.headlines.slice(n).map((x) => x.text) };
    };
    const storm = run(16);
    expect(storm.lost).toBeGreaterThan(0);
    expect(storm.lost).toBeLessThan(6);
    expect(storm.news.filter((t) => /has lost the Battle Fleet in the gale in the Western Approaches/.test(t)).length).toBe(storm.lost);
    expect(run(13).lost).toBe(0);
  });
});

describe('through the bridge', () => {
  it('the view carries the switch and the seas in a gale, and act switches it', () => {
    const g = at(seat(true), 14);
    type Call = (...a: unknown[]) => unknown;
    const TC = (globalThis as unknown as { TC: Record<string, Call> }).TC;
    TC.load(saveGame(g));
    const v = TC.view() as { heavySeas: boolean; gales: number[] };
    expect(v.heavySeas).toBe(true);
    expect(v.gales).toEqual([...galeSeas(g)]);
    expect((TC.act('heavyseasrule:off') as { ok: boolean }).ok).toBe(true);
    const w = TC.view() as { heavySeas: boolean; gales: number[] };
    expect(w.heavySeas).toBe(false);
    expect(w.gales).toEqual([]);
  });
});
