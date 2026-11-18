// The cities version's code review (18 November 2026): what a second reading of the version found and mended before
// it shipped. A campaign saved before the version keeps its own year of aircraft from abroad and finds airfields on
// its new cities; a smaller power's ground never reads as paying nothing; Canada's months to full war work count from
// where a new war would start them; and the United States' envoys, in no bloc, say what they do.
import { describe, expect, it } from 'vitest';
import { ARSENAL } from '../src/campaign/arsenal';
import { fieldClass } from '../src/campaign/air';
import { commandView } from '../src/campaign/desks';
import { BUDGET, landFactor, provinceYield } from '../src/campaign/economy';
import { WORLD_MAP } from '../src/campaign/world';
import { createGame } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { prizeOf } from '../src/campaign/towns';
import { beginHuman, humanUp, orderDesk, stepAI } from '../src/campaign/turn';
import { staffRuns } from '../src/campaign/depth';
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
    // The province a city was cut out of keeps only the field the first day gives it now (no air group is based on
    // these): East Texas's went with Houston, Toronto's town keeps its landing ground.
    for (const name of ['East Texas', 'Toronto', 'Ohio Valley', 'Minas Gerais']) {
      const id = g.byName[name].id;
      expect(JSON.parse(brianSave).flags[`af:${id}`], name).toBeGreaterThan(fieldClass(fresh, id));
      expect(g.airFleets.some((f) => f.base === id)).toBe(false);
      expect(fieldClass(g, id), name).toBe(fieldClass(fresh, id));
    }
  });

  it('ground that pays something never reads as paying nothing on the prize card', () => {
    const g = playing('USA');
    let small = 0, under = 0;
    for (const r of g.regions) {
      if (r.owner !== 'MEX' && r.owner !== 'CAN') continue;
      const y = provinceYield(g, r, 'USA').total;
      if (y > 0 && Math.round(y * BUDGET.scale) < 1) small++;
      const p = prizeOf(g, r.id, 'USA');
      if (p && y > 0) expect(p.income, r.name).toBeGreaterThanOrEqual(1);
      // Its parts as the province card words them: none says 0, and one under one says so.
      const money = p?.items.find((x) => x.key === 'income')?.text ?? '';
      expect(money, r.name).not.toMatch(/(land|harbour|oilfield|factory|factories|seat of government) 0\b/);
      if (/under 1/.test(money)) under++;
      // And they add up to the total beside them: exactly, unless a part is under one.
      const said = [...(/\((.+)\)/.exec(money)?.[1] ?? '').matchAll(/(?<!under) (\d+)(?=,|$)/g)].reduce((s, m) => s + Number(m[1]), 0);
      if (p && /\(/.test(money)) {
        if (/under 1/.test(money)) expect(said, `${r.name}: ${money}`).toBeLessThanOrEqual(p.income);
        else expect(said, `${r.name}: ${money}`).toBe(p.income);
      }
    }
    expect(small).toBeGreaterThan(0);
    expect(under).toBeGreaterThan(0);
  });

  it('a city takes its land pay from its province: a province and its cities pay what the province paid alone (ruling 210(a))', () => {
    const g = startOpen(createGame(3, null));
    const families = new Map<number, number[]>();
    for (const x of WORLD_MAP.regions as { id: number; of?: number }[]) if (x.of !== undefined) families.set(x.of, [...(families.get(x.of) ?? [x.of]), x.id]);
    expect(families.size).toBeGreaterThan(100);
    let unlisted = 0;
    for (const [p, ids] of families) {
      const P = g.regions[p];
      if (landFactor(P) !== 1) continue;
      unlisted++;
      const pays = ids.reduce((s, id) => s + provinceYield(g, g.regions[id], g.regions[id].owner).land, 0);
      expect(pays, P.name).toBeCloseTo(BUDGET.land[P.terrain] * (P.overseas ? BUDGET.overseas : 1), 10);
    }
    expect(unlisted).toBeGreaterThan(50);
    // America's land pays on the first day what it paid before the cities were cut: 987 a month (Version 84.3.3).
    const usa = g.regions.filter((r) => r.owner === 'USA').reduce((s, r) => s + provinceYield(g, r, 'USA').land, 0);
    expect(Math.round(usa * BUDGET.scale)).toBe(987);
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
    const desk = (): string | undefined => commandView(g)?.desks.find((d) => d.id === 'diplomacy')?.state;
    const rest = ' Only a member of the Axis or the Allies courts other neutrals.';
    // (The UI director's words: who courts back, the staff or the player by hand.)
    expect(desk()).toBe(staffRuns(g, 'USA', 'diplomacy')
      ? `German envoys are courting Argentina. Your staff courts it back each month, at no cost.${rest}`
      : `German envoys are courting Argentina. Courting it back costs nothing: Court back, on its row under Powers.${rest}`);
    orderDesk(g, 'diplomacy', !staffRuns(g, 'USA', 'diplomacy'));
    g.flags['court:GER:MEX'] = g.turn;
    expect(desk()).toMatch(/^German envoys are courting Mexico and Argentina\. (Your staff courts them back|Courting them back costs nothing: Court back, on their rows under Powers\.)/);
    // Only a country that can still be courted is named: one gone to war is out of it.
    g.nations.ARG.atWar = ['GER'];
    expect(desk()).toMatch(/^German envoys are courting Mexico\. /);
  });
});
