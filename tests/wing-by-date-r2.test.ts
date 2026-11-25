// Carrier wings by date (release 2, the switch 'Carrier wings by date'; the design director's riders of 17 November
// 2026 to rulings 189 and 202, built by the navy thread on the far side's audit of 25 November). Britain's wing a deck
// changes on the historian's dates; a change of room gives nothing and loses nothing: what is over goes to the reserve,
// and new room fills as losses are made good, in harbour. Played on the old chart; no war is fought but the first month.
import { describe, expect, it } from 'vitest';
import stage from '../data/campaign/stage.json';
import { airPool, airUpkeep } from '../src/campaign/air';
import { setWingByDate, syncWings, wingByDateOn, wingSize, wingStep, wingsOf } from '../src/campaign/carriers';
import { createGame } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { WINGBYDATE_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, runTurnAI, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type TCApi = { load: (t: string) => unknown; act: (a: string) => { ok: boolean; note: string } };
const TC = (globalThis as unknown as { TC: TCApi }).TC;

/** Britain's grand campaign after its first month, its carriers' wings given. */
function britain(on: boolean, human: string | null = null): Game {
  const g = startOpen(createGame(3, human));
  if (human) { for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g); beginHuman(g); } else runTurnAI(g);
  setWingByDate(g, on);
  return g;
}
const home = (g: Game): Fleet => g.fleets.find((f) => f.nat === 'UK' && f.ships.cv > 0 && f.name === 'Home Fleet')!;
const wing = (g: Game, f: Fleet, kind: 'ftr' | 'bmr') => wingsOf(g, f.id).find((w) => w.kind === kind)!;

describe('the switch', () => {
  it('ships off, and off every navy keeps one wing', () => {
    expect((stage as { wingByDate?: boolean }).wingByDate).toBe(false);
    expect(WINGBYDATE_STAGE).toBe(false);
    const g = britain(false);
    expect(wingByDateOn(g)).toBe(false);
    g.turn = 70;
    expect(wingSize('UK', g)).toEqual({ ftr: 12, bmr: 24 });
  });
  it('is saved with the campaign, and switched from the options', () => {
    const g = britain(true, 'UK');
    expect(loadGame(saveGame(g))!.flags.wingByDate).toBe(1);
    TC.load(saveGame(g));
    expect(TC.act('wingbydaterule:off').ok).toBe(true);
    expect(TC.act('wingbydaterule:on').note).toMatch(/Carrier wings by date are on/);
  });
});

describe("Britain's steps", () => {
  it('reads the historian\'s four steps by month', () => {
    const g = britain(true);
    const at = (t: number) => { g.turn = t; return wingSize('UK', g); };
    expect([at(1), at(35), at(36), at(52), at(53), at(64), at(65), at(80)]).toEqual([
      { ftr: 12, bmr: 24 }, { ftr: 12, bmr: 24 }, { ftr: 24, bmr: 12 }, { ftr: 24, bmr: 12 }, { ftr: 28, bmr: 20 }, { ftr: 28, bmr: 20 }, { ftr: 37, bmr: 17 }, { ftr: 37, bmr: 17 },
    ]);
    g.turn = 70;
    expect(wingSize('USA', g)).toEqual({ ftr: 30, bmr: 48 });
  });
  it('on the date nothing is given and nothing lost: the strike aircraft over the room go to the reserve, the new fighter room stands empty', () => {
    const g = britain(true);
    const f = home(g), cv = f.ships.cv;
    const F = wing(g, f, 'ftr'), B = wing(g, f, 'bmr'), p = airPool(g, 'UK');
    expect([F.est.ftr, B.est.bmr]).toEqual([12 * cv, 24 * cv]);
    const had = { ftr: F.ftr, bmr: B.bmr, lostF: F.lost.ftr, lostB: B.lost.bmr, pool: p.bmr };
    // (Every British strike aircraft, aboard any carrier or in the reserve: the step moves them, it makes or loses none.)
    const all = (): number => g.airFleets.filter((w) => w.nat === 'UK' && w.ship !== undefined).reduce((n, w) => n + w.bmr, 0) + airPool(g, 'UK').bmr;
    const total = all();
    g.turn = 36;
    syncWings(g);
    expect([F.est.ftr, B.est.bmr]).toEqual([24 * cv, 12 * cv]);
    expect(F.ftr).toBe(had.ftr);
    expect(B.bmr).toBe(12 * cv);
    expect(airPool(g, 'UK').bmr).toBeGreaterThanOrEqual(had.pool + had.bmr - 12 * cv);
    expect(all()).toBe(total);
    expect([F.lost.ftr, B.lost.bmr]).toEqual([had.lostF, had.lostB]);
    // (Synced again, nothing more moves.)
    syncWings(g);
    expect([F.est.ftr, F.ftr, B.est.bmr, B.bmr]).toEqual([24 * cv, had.ftr, 12 * cv, 12 * cv]);
    // (In harbour the new room fills from the reserve, as losses are made good.)
    p.ftr = 500; p.pilots = 500;
    f.stance = 'port'; f.zone = f.home;
    airUpkeep(g, 'UK');
    expect(F.ftr).toBe(24 * cv);
  });
  it('a player who is Britain is told in the month\'s news; the computer\'s Britain is not', () => {
    const g = britain(true, 'UK');
    g.turn = 36;
    wingStep(g, 'UK');
    expect(g.headlines.some((h) => h.text === 'The Fleet Air Arm re-equips: each of your carriers now has deck room for 24 fighters and 12 strike aircraft, where it had room for 12 and 24.')).toBe(true);
    const c = britain(true);
    c.turn = 36;
    wingStep(c, 'UK');
    expect(c.headlines.some((h) => /Fleet Air Arm re-equips/.test(h.text))).toBe(false);
  });
});

describe('off, nothing is written', () => {
  it('no wing notes a room of its own, and no fleet a share', () => {
    const g = britain(false);
    runTurnAI(g);
    expect(g.airFleets.some((w) => w.size !== undefined)).toBe(false);
    expect(g.fleets.some((f) => f.deck !== undefined)).toBe(false);
  }, 300000);
});
