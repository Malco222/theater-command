// Mines at sea (release 2, mines.ts; Brian 24 November 2026 20:38Z, "yes to all"): a fleet with destroyers, cruisers or
// submarines lays a field across a narrow sea or off a harbour an enemy fleet sails from; enemy ships at sea there, or
// sailing from a harbour on it, may lose a ship each month until destroyers sweep it. The computer's admirals lay fields
// by the same rule. Behind its own switch.
import { describe, expect, it } from 'vitest';
import stage from '../data/campaign/stage.json';
import { createGame } from '../src/campaign/game';
import { setFog } from '../src/campaign/intel';
import { MINES, aiMines, fieldKnown, mineLine, mineSeas, minesMonth, minesSeen, minesStrike, seaMinesOn, setSeaMines, whyNotMines } from '../src/campaign/mines';
import { shipCount } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { SEAMINES_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, fleetOptions, humanUp, orderMinesFleet, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

function seat(nat: string, seed: number, on: boolean): Game {
  const g = startOpen(createGame(seed, nat));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  setSeaMines(g, on);
  setFog(g, false);
  beginHuman(g);
  return g;
}
const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const sea = (g: Game, name: string): number => g.seas.findIndex((z) => z.name === name);

describe('the switch', () => {
  it('ships off, and off nothing of it is offered, laid or read', () => {
    expect((stage as { seaMines?: boolean }).seaMines).toBe(false);
    expect(SEAMINES_STAGE).toBe(false);
    const g = seat('UK', 4, false);
    expect(seaMinesOn(g)).toBe(false);
    const home = fleet(g, 'UK', 'Home Fleet');
    expect(whyNotMines(g, home.id, sea(g, 'North Sea'))).toMatch(/switched on/);
    expect(orderMinesFleet(g, home.id, sea(g, 'North Sea'))).toBe(false);
    expect(fleetOptions(g, home.id)!.mines).toBeUndefined();
    const before = saveGame(g);
    minesStrike(g, 'GER');
    minesMonth(g);
    aiMines(g, 'GER');
    expect(saveGame(g)).toBe(before);
    expect(saveGame(g)).not.toMatch(/"mines"/);
  });
  it('switched off in a campaign under way, every field is gone', () => {
    const g = seat('UK', 4, true);
    const home = fleet(g, 'UK', 'Home Fleet');
    expect(orderMinesFleet(g, home.id, sea(g, 'North Sea'))).toBe(true);
    expect(g.mines!.length).toBe(1);
    setSeaMines(g, false);
    expect(g.mines).toBeUndefined();
  });
});

describe('laying a field', () => {
  it('across a narrow sea or off an enemy harbour in reach, once a month; open water is refused', () => {
    const g = seat('UK', 4, true);
    const home = fleet(g, 'UK', 'Home Fleet');
    // The North Sea is off Wilhelmshaven, the German fleets' harbour; the Irish Sea and the Skagerrak are narrow.
    const can = mineSeas(g, home.id);
    expect(can).toEqual(expect.arrayContaining([sea(g, 'North Sea'), sea(g, 'Irish Sea'), sea(g, 'Skagerrak')]));
    expect(fleetOptions(g, home.id)!.mines).toEqual(can);
    expect(whyNotMines(g, home.id, sea(g, 'North Atlantic'))).toMatch(/open water/);
    const z = sea(g, 'Skagerrak');
    expect(orderMinesFleet(g, home.id, z)).toBe(true);
    expect(home.zone).toBe(z);
    expect(home.stance).toBe('patrol');
    expect(home.ordered).toBe(true);
    expect(g.mines).toEqual([{ z, nat: 'UK', s: MINES.lay, t: g.turn }]);
    expect(whyNotMines(g, home.id, sea(g, 'North Sea'))).toMatch(/already/);
    // Its own side knows it; the enemy does not, yet.
    expect(minesSeen(g, 'UK').map((m) => m.zone)).toEqual([z]);
    expect(minesSeen(g, 'GER')).toEqual([]);
    // A fleet of battleships and carriers alone carries no mines.
    const g2 = seat('UK', 4, true);
    const h2 = fleet(g2, 'UK', 'Home Fleet');
    h2.ships.ca = 0; h2.ships.dd = 0;
    expect(whyNotMines(g2, h2.id, sea(g2, 'North Sea'))).toMatch(/destroyers, cruisers or submarines/);
  });

  it("enemy ships at sea in it, or sailing from a harbour on it, lose a ship some months; its own and its allies' never", () => {
    const g = seat('UK', 4, true);
    const home = fleet(g, 'UK', 'Home Fleet');
    const z = sea(g, 'North Sea');
    expect(orderMinesFleet(g, home.id, z)).toBe(true);
    const base = saveGame(g);
    let hit = 0, months = 0;
    for (let k = 0; k < 40; k++) {
      const h = loadGame(base)!;
      h.turn += k;
      // The German U-boat Fleet sails from Wilhelmshaven, on the North Sea, wherever it raids.
      const boats = fleet(h, 'GER', 'U-boat Fleet');
      expect(boats.home).toBe(z);
      expect(boats.stance).not.toBe('port');
      const had = shipCount(boats.ships), lost = boats.lost.ss;
      const news = h.headlines.length;
      minesStrike(h, 'GER');
      months++;
      if (shipCount(boats.ships) < had) {
        hit++;
        expect(shipCount(boats.ships)).toBe(had - 1);
        expect(boats.lost.ss).toBe(lost + 1);
        expect(h.headlines.slice(news).some((x) => /Your minefield in the North Sea sinks a German submarine of the U-boat Fleet/.test(x.text))).toBe(true);
        expect(fieldKnown(h, h.mines![0], 'GER')).toBe(true);
      }
      // The British fleets in the North Sea never strike their own mines.
      const ours = fleet(h, 'UK', 'Home Fleet'), was = shipCount(ours.ships);
      minesStrike(h, 'UK');
      expect(shipCount(ours.ships)).toBe(was);
    }
    expect(hit).toBeGreaterThan(months * MINES.hit * 0.4);
    expect(hit).toBeLessThan(months * MINES.hit * 1.8);
  });

  it('wears each month, and enemy destroyers sweep it faster and find it; spent, it is gone', () => {
    const g = seat('UK', 4, true);
    const home = fleet(g, 'UK', 'Home Fleet');
    const z = sea(g, 'North Sea');
    expect(orderMinesFleet(g, home.id, z)).toBe(true);
    const base = saveGame(g);
    const quiet = loadGame(base)!;
    for (const f of quiet.fleets) if (f.nat === 'GER' && f.zone === z) f.zone = f.home === z ? 10 : f.zone;
    minesMonth(quiet);
    expect(quiet.mines![0].s).toBeCloseTo(MINES.lay - MINES.decay);
    const swept = loadGame(base)!;
    const battle = fleet(swept, 'GER', 'Battle Fleet');
    battle.zone = z; battle.stance = 'patrol';
    minesMonth(swept);
    expect(swept.mines![0].s).toBeCloseTo(MINES.lay - MINES.decay - MINES.sweep);
    expect(fieldKnown(swept, swept.mines![0], 'GER')).toBe(true);
    // A German fleet in a field it knows of is told so on its panel.
    expect(mineLine(swept, battle)).toMatch(/British mines in the North Sea/);
    for (let k = 0; k < 5; k++) minesMonth(swept);
    expect(swept.mines).toEqual([]);
  });
});

describe("the computer's admirals", () => {
  it('lay fields of their own, off the harbour of the strongest enemy fleet they reach or across a narrow sea', () => {
    const g = seat('GER', 3, true);
    const base = saveGame(g);
    let laid = 0;
    for (let k = 0; k < 12; k++) {
      const h = loadGame(base)!;
      h.turn += k;
      aiMines(h, 'UK');
      const mine = (h.mines ?? []).filter((m) => m.nat === 'UK');
      expect(mine.length).toBeLessThanOrEqual(1);
      if (!mine.length) continue;
      laid++;
      const z = mine[0].z;
      const offHarbour = h.fleets.some((e) => e.nat === 'GER' && shipCount(e.ships) > 0 && e.home === z);
      expect(offHarbour || h.seas[z].narrow).toBe(true);
      const layer = h.fleets.find((f) => f.nat === 'UK' && f.mined === h.turn)!;
      expect(layer.zone).toBe(z);
    }
    expect(laid).toBeGreaterThan(0);
    expect(laid).toBeLessThan(12);
  });
});

describe('through the bridge', () => {
  it('the fleet page offers LAY MINES, the order goes through act, and the map is told of the field', () => {
    const g = seat('UK', 4, true);
    const home = fleet(g, 'UK', 'Home Fleet');
    type Call = (...a: unknown[]) => unknown;
    const TC = (globalThis as unknown as { TC: Record<string, Call> }).TC;
    TC.load(saveGame(g));
    expect((TC.view() as { seaMines: boolean }).seaMines).toBe(true);
    const z = sea(g, 'Skagerrak');
    const page = TC.page(`fleet:${home.id}`) as { sections: { heading: string; rows: { title: string; buttons: { label: string; act: string }[] }[] }[] };
    const rows = page.sections.find((x) => x.heading === 'SEAS TO MINE')!.rows;
    expect(rows.some((r) => r.title.includes('Skagerrak') && r.buttons.some((b) => b.label === 'LAY MINES' && b.act === `fleetmines:${home.id}:${z}`))).toBe(true);
    const out = TC.act(`fleetmines:${home.id}:${z}`) as { ok: boolean; note: string };
    expect(out.ok).toBe(true);
    expect(out.note).toMatch(/lay a minefield in the Skagerrak/);
    expect((TC.view() as { mines: { zone: number; nat: string; own: boolean }[] }).mines).toEqual([{ zone: z, nat: 'UK', own: true, word: 'thick' }]);
    expect((TC.act('seaminesrule:off') as { ok: boolean }).ok).toBe(true);
    const v = TC.view() as { seaMines: boolean; mines: unknown[] };
    expect(v.seaMines).toBe(false);
    expect(v.mines).toEqual([]);
  });
});
