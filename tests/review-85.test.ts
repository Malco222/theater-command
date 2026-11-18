// The cities version's code review (18 November 2026): what a second reading of the version found and mended before
// it shipped. A campaign saved before the version keeps its own year of aircraft from abroad and finds airfields on
// its new cities; a smaller power's ground never reads as paying nothing; Canada's months to full war work count from
// where a new war would start them; and the United States' envoys, in no bloc, say what they do.
import { describe, expect, it } from 'vitest';
import { ARSENAL } from '../src/campaign/arsenal';
import { fieldClass } from '../src/campaign/air';
import { commandView } from '../src/campaign/desks';
import { BUDGET, provinceYield } from '../src/campaign/economy';
import { createGame } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { prizeOf } from '../src/campaign/towns';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import { warFooting, warWorkLeft } from '../src/campaign/world';
import brianSave from './ahead/saves/brian-usa-1940-12.json?raw';

const GIFTS = ARSENAL.smallAir.gifts;

function playing(human = 'USA', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}

describe('the cities version, as reviewed', () => {
  it('a campaign saved before the aircraft from abroad does not get them all at once when it is loaded', () => {
    const saved = JSON.parse(brianSave);
    expect(Object.keys(saved.flags).some((k) => k.startsWith('gift:'))).toBe(false);
    const g = loadGame(brianSave)!;
    expect(g.flags.gifts).toBe(1);
    // Every row due before the month it was saved in is marked as come; the rest still wait for their month.
    GIFTS.forEach(([month], i) => expect(!!g.flags[`gift:${i}`], `row ${i}, month ${month}`).toBe(month < g.turn));
    // And a campaign saved after it keeps what it had: a row still waiting is not marked by a second load.
    const h = loadGame(saveGame(g))!;
    const waiting = GIFTS.findIndex(([month]) => month >= g.turn);
    delete h.flags[`gift:${waiting}`];
    const k = loadGame(saveGame(h))!;
    expect(k.flags[`gift:${waiting}`]).toBeUndefined();
    GIFTS.forEach((_, i) => expect(!!h.flags[`gift:${i}`]).toBe(!!g.flags[`gift:${i}`] && i !== waiting));
  });

  it('a campaign saved on the smaller chart finds the first-day airfields on its new cities', () => {
    const g = loadGame(brianSave)!;
    const fresh = startOpen(createGame(g.seed, 'USA'));
    let seeded = 0;
    // (The save was made on the 201 provinces: the cities drawn since are the regions after its last one.)
    for (let i = JSON.parse(brianSave).regions.length; i < g.regions.length; i++) {
      expect(fieldClass(g, i), g.regions[i].name).toBeGreaterThanOrEqual(fieldClass(fresh, i));
      if (fieldClass(fresh, i) > 0) seeded++;
    }
    expect(seeded).toBeGreaterThan(10);
    expect(fieldClass(g, g.byName.Houston.id)).toBe(fieldClass(fresh, g.byName.Houston.id));
  });

  it('ground that pays something never reads as paying nothing on the prize card', () => {
    const g = playing('USA');
    let small = 0;
    for (const r of g.regions) {
      if (r.owner !== 'MEX' && r.owner !== 'CAN') continue;
      const y = provinceYield(g, r, 'USA').total;
      if (y > 0 && Math.round(y * BUDGET.scale) < 1) small++;
      const p = prizeOf(g, r.id, 'USA');
      if (p && y > 0) expect(p.income, r.name).toBeGreaterThanOrEqual(1);
    }
    expect(small).toBeGreaterThan(0);
  });

  it("Canada at peace after the fall of France counts its months to full war work from where a new war would start them", () => {
    const g = playing('USA');
    const C = g.nations.CAN;
    g.turn = 30;
    for (const e of [...C.atWar]) { C.atWar = C.atWar.filter((x) => x !== e); g.nations[e].atWar = g.nations[e].atWar.filter((x) => x !== 'CAN'); }
    delete g.flags['ww:CAN'];
    expect(warFooting(g, 'CAN')).toBe(0.45);
    // A war made now starts at 0.45 + 0.022 x 20 = 0.89, five months from full war work.
    expect(warWorkLeft(g, 'CAN')).toBe(5);
    g.turn = 4;
    // Before June 1940 it waits for the fall of France first: six months, then 25.
    expect(warWorkLeft(g, 'CAN')).toBe(6 + 25);
  });

  it("the United States' envoys, in no bloc, say they court back the neighbours the Axis courts", () => {
    const g = playing('USA');
    expect(g.nations.USA.align).not.toBe('allies');
    g.flags['court:GER:ARG'] = g.turn;
    const desk = commandView(g)?.desks.find((d) => d.id === 'diplomacy');
    expect(desk?.state).toMatch(/^The Axis is courting Argentina: your envoys court it back each month, at no cost\./);
  });
});
