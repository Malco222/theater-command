// The Karakoram and the people of China's far west (item 7 of the rider of 19:25Z, 20 November 2026, to ruling 234;
// historian/sinkiang-karakoram-1937-45.md): Tibet's sibling.
//
// Two facts of the record, carried by forms the game already had. From Sinkiang's oases to Leh and to Gilgit ran
// caravan roads over passes of about 4,700 to 5,540 m: no army crossed them, so the borders the map-maker draws
// there are on its list of borders that are not there (far.mjs, PACIFIC_NO_BORDER), as the high plateau's are, and
// no march, attack or supply line has a road across. Sinkiang's desert stays open, at desert pace: armies did cross
// it from Kansu by Hami. And the ground was not empty: Sheng Shicai's provincial army stood at Tihwa and the Ma
// generals' cavalry in Kansu's west, three held rows at the historian's figures (far-seats-pacific.json). With the
// far side's switch off there is no such ground and nothing here is read.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
// @ts-expect-error the map-maker's own file: plain JavaScript, with no types of its own
import { PACIFIC_NO_BORDER } from '../data/campaign/far.mjs';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';
import { farOn } from '../src/campaign/world';

const SINKIANG = ['Hami', 'Turfan', 'Tihwa', 'Kuldja', 'Chuguchak', 'Aksu', 'Kashgar', 'Yarkand', 'Khotan', 'Charkhlik'];
const grand = (seed = 3): Game => startOpen(createGame(seed, null));
const names = (g: Game, place: string): string[] => g.byName[place].nb.map((j) => g.regions[j].name);

describe('the Karakoram carries no army, and the far west is not empty (rider of 19:25Z to ruling 234, item 7)', () => {
  it('has no border from Sinkiang into India: Leh and Gilgit are reached from India alone', () => {
    const g = grand();
    expect(farOn(g)).toBe(true);
    for (const nm of SINKIANG) { expect(g.byName[nm], nm).toBeDefined(); expect(g.byName[nm].owner, nm).toBe('CHN'); }
    const sinkiang = new Set(SINKIANG.map((nm) => g.byName[nm].id));
    // No province of Sinkiang lists a province India's ruler owns, and none of his lists one of Sinkiang's.
    for (const nm of SINKIANG) for (const j of g.byName[nm].nb) expect(g.regions[j].owner, `${nm} still borders ${g.regions[j].name}`).not.toBe('UK');
    for (const r of g.regions) if (r.owner === 'UK') for (const j of r.nb) expect(sinkiang.has(j), `${r.name} still lists ${g.regions[j].name}`).toBe(false);
    expect(names(g, 'Leh').sort()).toEqual(['Gilgit', 'Simla', 'Srinagar']);
    expect(names(g, 'Gilgit')).not.toContain('Khotan');
    expect(names(g, 'Gilgit')).not.toContain('Yarkand');
    // The map-maker's list names the three. (Yarkand is a city cut from Kashgar's province: its border is Kashgar's there.)
    const has = (a: string, b: string): boolean => (PACIFIC_NO_BORDER as [string, string][]).some(([x, y]) => (x === a && y === b) || (x === b && y === a));
    expect([has('Khotan', 'Leh'), has('Khotan', 'Gilgit'), has('Kashgar', 'Gilgit')]).toEqual([true, true, true]);
  });

  it("leaves Sinkiang's desert open: a road still runs from Kansu by Hami to Tihwa and round the Tarim to Khotan", () => {
    const g = grand();
    const chn = (id: number): boolean => g.regions[id].owner === 'CHN';
    const seen = new Set([g.byName.Lanchow.id]), q = [g.byName.Lanchow.id];
    for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (chn(j) && !seen.has(j)) { seen.add(j); q.push(j); }
    for (const nm of SINKIANG) expect(seen.has(g.byName[nm].id), `${nm} is cut off from Kansu`).toBe(true);
    expect(names(g, 'Hami')).toContain('Anhsi');
    expect(names(g, 'Khotan')).toContain('Yarkand');
  });

  it("stands the record's people on the ground the chart had empty: three held rows of China's, at the historian's figures", () => {
    const g = grand();
    const row = (name: string) => { const c = g.corps.find((x) => x.nat === 'CHN' && x.name === name && !x.dead)!; return c && [g.regions[c.reg].name, c.kind, c.divs, c.str, !!c.hold]; };
    expect(row('Sinkiang Provincial Army')).toEqual(['Tihwa', 'inf', 1, 0.4, true]);
    expect(row('Qinghai Army')).toEqual(['Sining', 'cav', 1, 0.6, true]);
    expect(row('Hexi Cavalry')).toEqual(['Liangchow', 'cav', 1, 0.4, true]);
  });
});
