// The far stage's chart (step 13 and the globe's stage, built ahead behind the switch in stage.ts). With the far side
// on, the grand campaign's chart runs all the way round the world: the Americas with their cities as the cities
// version lays them (375 to 914), then the Pacific, the globe and their cities (915 to 2065) from the map builder's
// final chart (tools/build-world-whole.mjs). America's islands are there: Hawaii, Alaska and the Aleutians, Midway,
// Wake, Guam and Samoa (Tutuila; Upolu and Savaii, New Zealand's, are Apia beside it). The map has no edge east or west: a straight line goes the short way round, across the seam
// at 141 W where California and Hawaii are neighbours. This file turns the switch on for itself alone.
import { describe, expect, it, vi } from 'vitest';

vi.hoisted(() => { (globalThis as { TC_FAR?: unknown }).TC_FAR = 1; });

import { createGame } from '../src/campaign/game';
import { km } from '../src/campaign/air';
import { seaAt } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { FAR_STAGE } from '../src/campaign/stage';
import { runTurnAI } from '../src/campaign/turn';
import { FAR_FIRST, WORLD_FIRST, WORLD_MAP, WORLD_STAGES, farOn, worldOn } from '../src/campaign/world';
import { WORLD_WIDTH, dxOf, mapDist, nearX, wrapSpan } from '../src/campaign/wrap';
import americas from '../src/generated/world-americas.json';
import wholeRails from '../src/generated/world-whole-rails.json';
import type { Game } from '../src/campaign/types';

const wide = (seed = 7): Game => startOpen(createGame(seed, null));
const g = wide();
const at = (name: string) => {
  const r = g.byName[name];
  if (!r) throw new Error(`no province named ${name}`);
  return r;
};

describe('the far stage: the whole world on one chart', () => {
  it('is on for this file only, and lays the far side after the cities version', () => {
    expect(FAR_STAGE).toBe(true);
    expect(worldOn(g) && farOn(g)).toBe(true);
    expect(FAR_FIRST).toBe(915);
    expect(WORLD_STAGES).toEqual([104, 201, 540]);
    expect(g.regions.length).toBe(WORLD_FIRST + 1692);
    // The Americas, every province where the cities version has it.
    const A = (americas as { regions: { id: number; name: string; owner: string }[] }).regions;
    for (const r of A) expect([g.regions[r.id].name, g.regions[r.id].owner]).toEqual([r.name, r.owner]);
    expect(g.regions[FAR_FIRST].name).toBe('Pearl Harbor');
    // Every name on the map once.
    expect(new Set(g.regions.map((r) => r.name)).size).toBe(g.regions.length);
  });

  it('carries America\'s Pacific: Hawaii, Alaska and the Aleutians, Midway, Wake, Guam, Samoa', () => {
    for (const n of ['Pearl Harbor', 'Honolulu', 'Kauai', 'Maui', 'Hawaii', 'Anchorage', 'Fairbanks', 'Seward', 'Nome', 'Juneau', 'Ketchikan',
      'Kodiak', 'Dutch Harbor', 'Cold Bay', 'Adak', 'Kiska', 'Attu', 'Midway', 'Wake', 'Guam', 'Samoa']) {
      expect(at(n).owner, n).toBe('USA');
      expect(at(n).id, n).toBeGreaterThanOrEqual(FAR_FIRST);
    }
    // Oahu is two provinces, the fleet's harbour and the city beside it, each the other's neighbour; the Aleutians are islands.
    expect(at('Pearl Harbor').nb).toEqual([at('Honolulu').id]);
    for (const n of ['Dutch Harbor', 'Adak', 'Kiska', 'Attu', 'Midway', 'Wake', 'Guam']) expect(at(n).nb, n).toEqual([]);
    // The Alaska Railroad, Seward to Fairbanks.
    expect(at('Anchorage').nb).toContain(at('Seward').id);
    // Samoa is Tutuila and Pago Pago; Upolu and Savaii are Apia, a place of its own beside it.
    const apia = at('Apia');
    expect((WORLD_MAP.regions[apia.id - WORLD_FIRST] as { of?: number }).of).toBe(at('Samoa').id);
    expect(apia.owner).toBe('NZL');
  });

  it('gives the panhandle its mainland: Juneau and Ketchikan reach Canada by land, Skagway by the White Pass line', () => {
    expect(at('Juneau').nb).toEqual(expect.arrayContaining([at('Ketchikan').id, at('Copper River').id, at('Whitehorse').id, at('Watson Lake').id]));
    expect(at('Ketchikan').nb).toContain(at('Prince Rupert').id);
    expect(at('Prince Rupert').nb).toContain(at('Ketchikan').id);
    // Whitehorse lost its coast to Juneau: on the whole world it is inland, as the cities version is not.
    expect(at('Whitehorse').seas ?? []).toEqual([]);
    const rails = (wholeRails as { links: number[][] }).links;
    const [j, w] = [at('Juneau').id, at('Whitehorse').id];
    expect(rails.some(([a, b]) => (a === j && b === w) || (a === w && b === j))).toBe(true);
  });

  it('joins the old provinces to the new by land, both ways round', () => {
    const joins = WORLD_MAP.joins ?? [];
    // (61 since the Great Sand Sea's join, Siwa to Kufra, was struck: far.mjs GLOBE_NO_BORDER, the design director's 14:23Z of 22 November 2026.)
    expect(joins.length).toBe(61);
    for (const [a, b] of joins) {
      expect(g.regions[a].nb, `${g.regions[a].name} to ${g.regions[b].name}`).toContain(b);
      expect(g.regions[b].nb, `${g.regions[b].name} to ${g.regions[a].name}`).toContain(a);
    }
    // The Yukon meets Alaska; Gorky, on the map of Europe, meets the Volga country beyond it.
    expect(at('Copper River').nb).toContain(at('Whitehorse').id);
    expect(at('Gorky').nb.some((j) => j >= FAR_FIRST)).toBe(true);
    // A fresh game's lists are its own: the map file's arrays are not grown twice.
    const h = wide(8);
    expect(h.byName['Gorky'].nb).toEqual(at('Gorky').nb);
  });

  it('knows its 145 seas by the map, past the 127 one byte holds', () => {
    expect(g.seas.length).toBe(145);
    for (const z of [72, 73, 76, 128, 142, 144]) {
      const s = g.seas[z];
      expect(seaAt(s.x, s.y, g), s.name).toBe(z);
    }
    expect(g.seas[at('Pearl Harbor').seas[0]].name).toBe('Hawaiian Waters');
  });

  it('measures the short way round the world, across the seam at 141 W', () => {
    expect(wrapSpan(g)).toBe(WORLD_WIDTH);
    const sf = at('San Francisco'), ph = at('Pearl Harbor');
    // On the sheet they are at its two ends; round the world they are about 3,150 km apart on the chart's scale (the
    // great circle is about 3,850 km), not the 31,000 the long way across the sheet would make them.
    expect(Math.abs(sf.x - ph.x)).toBeGreaterThan(WORLD_WIDTH / 2);
    const d = km(g, sf.id, ph.id);
    expect(d).toBeGreaterThan(2500);
    expect(d).toBeLessThan(4500);
    expect(dxOf(g, sf.x, ph.x)).toBeCloseTo(WORLD_WIDTH - Math.abs(sf.x - ph.x), 6);
    expect(mapDist(g, sf.x, sf.y, ph.x, ph.y)).toBeLessThan(Math.hypot(sf.x - ph.x, sf.y - ph.y));
    // A place is drawn beside another a world's width away where that is nearer.
    expect(Math.abs(nearX(g, ph.x, sf.x) - sf.x)).toBeLessThan(WORLD_WIDTH / 2);
    // On the map of Europe nothing goes round.
    expect(wrapSpan(createGame(7, null))).toBe(0);
  });

  it('names a country for every owner on the chart, a bare one where the far side brings none yet', () => {
    for (const r of g.regions) expect(g.nations[r.owner], r.name).toBeDefined();
    const jap = g.nations.JAP;
    expect(jap.name).toBe('Japan');
    expect(jap.capital).toBeGreaterThanOrEqual(FAR_FIRST);
  });

  it('plays a month, and a campaign saved on it loads on it', () => {
    const h = wide(3);
    runTurnAI(h);
    const back = loadGame(saveGame(h))!;
    expect(back).toBeTruthy();
    expect(back.regions.length).toBe(h.regions.length);
    expect(back.byName['Midway'].ctrl).toBe('USA');
    expect(back.byName['Copper River'].nb).toContain(back.byName['Whitehorse'].id);
  });
});
