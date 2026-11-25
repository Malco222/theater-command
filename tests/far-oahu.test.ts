// Oahu's divisions as the record has them (the design director's notes of 06:09Z, 06:12Z and 06:21Z of 21 November
// 2026 to ruling 234; the historian's far-chart-town-guards-globe.md, section 5).
//
// As read before (no fighting, months 1 to 29): the Hawaiian Division stood at 0.7 filling to its cap 0.8 at Pearl
// Harbor, and the 25th Infantry Division's row of month 26 came beside it as new men: two divisions where the record
// has one division split in two. A row could not be made from another for America: a computer staff that ships its
// own army is sent no dated expedition (world.ts shipsItsOwn), and the formations list only added.
//
// The director's ruling of 06:21Z: the formations list honours `from`, far side, for every country. A row made from
// corps on the board takes its strength out of them, never more than they hold, and does not come where none is
// left. Then the record's rows: the two Guard regiments in October 1940 (the 298th on Oahu, the 299th's battalions
// on the outer islands), and in October 1941 the 25th made from the 298th and the division, the 24th from what is
// left of the division. The draftees' growth of 1941 is the recorded gap; no dated cap or row stands in for it.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { createGame, declineOffer, removeCorps, rowOffers, startPhase, takeOffer } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import { shipsItsOwn } from '../src/campaign/world';
import type { Corps, Game } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';
import world from '../data/campaign/world.json';

const HD = 'Hawaiian Division', D24 = '24th Infantry Division', D25 = '25th Infantry Division';
const G298 = '298th Infantry (Hawaii National Guard)', G299 = '299th Infantry (Hawaii National Guard)';
const ISLAND = ['Pearl Harbor', 'Honolulu'], OUTER = ['Hawaii', 'Maui', 'Kauai'];
const field = (g: Game, places: string[]): Corps[] => g.corps.filter((c) => c.nat === 'USA' && !c.dead && c.kind !== 'fort' && places.includes(g.regions[c.reg].name));
const sum = (g: Game, places: string[]): number => +field(g, places).reduce((s, c) => s + c.divs * c.str, 0).toFixed(3);
const months = (g: Game, from: number, to: number): void => { for (let t = from; t <= to; t++) { g.turn = t; startPhase(g, 'USA'); } };
const named = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'USA' && !c.dead && c.name === name);
const row = (name: string): any[] => ((setup as any).formations as any[]).find((r) => r[2] === name)!;

describe("the data: Oahu's rows", () => {
  it('the Hawaiian Division from the first day at 0.7 and never past 0.8', () => {
    expect(((world as any).corps as any[]).find((r) => r[1] === HD)).toEqual(['USA', HD, 'inf', 1, 'Pearl Harbor', { xp: 0.05, str: 0.7, est: 0.8 }]);
  });

  it('the Guard regiments in October 1940: the 298th beside the division, the 299th by battalions on the outer islands', () => {
    expect(row(G298)).toEqual([14, 'USA', G298, 'inf', 1, 'Pearl Harbor', { xp: 0.05, str: 0.11, est: 0.15 }]);
    expect(row(`${G299}, on Hawaii`)).toEqual([14, 'USA', `${G299}, on Hawaii`, 'inf', 1, 'Hawaii', { xp: 0.05, str: 0.04, est: 0.05, hold: 1 }]);
    expect(row(`${G299}, on Maui`)).toEqual([14, 'USA', `${G299}, on Maui`, 'inf', 1, 'Maui', { xp: 0.05, str: 0.035, est: 0.05, hold: 1 }]);
    expect(row(`${G299}, on Kauai`)).toEqual([14, 'USA', `${G299}, on Kauai`, 'inf', 1, 'Kauai', { xp: 0.05, str: 0.035, est: 0.05, hold: 1 }]);
    // (The record: the two regiments added about 3,000 to 3,500 men, at 15,000 to a division; the 299th about 0.15 by December 1941.)
    expect(0.11 + 0.04 + 0.035 + 0.035).toBeCloseTo(3300 / 15000, 2);
  });

  it('the split of October 1941: the 25th from the 298th and the division, then the 24th from what is left; 0.8 and, on Oahu, 0.65', () => {
    expect(row(D25)).toEqual([26, 'USA', D25, 'inf', 1, 'Honolulu', { xp: 0.05, str: 0.55, est: 0.8, from: [G298, HD] }]);
    expect(row(D24)).toEqual([26, 'USA', D24, 'inf', 1, 'Pearl Harbor', { xp: 0.05, str: 0.65, est: 0.65, from: [HD] }]);
    // (The record's 24th is 0.8 with the 299th, its third regiment, whose battalions are the outer islands' rows: 0.65 and 0.15.)
    expect(0.65 + 0.05 * 3).toBeCloseTo(0.8, 5);
    // The 25th's row is read before the 24th's: it takes its share, and the 24th what is left.
    const list = (setup as any).formations as any[];
    expect(list.indexOf(row(D25))).toBeLessThan(list.indexOf(row(D24)));
  });

  it('a computer America is sent no dated expedition, so its reorganisations stand in the formations list', () => {
    const g = startOpen(createGame(5, null));
    expect(shipsItsOwn(g, 'USA')).toBe(true);
    const far = new Set(['Pearl Harbor', 'Honolulu', 'Hawaii', 'Maui', 'Kauai', 'Midway', 'Wake', 'Guam', 'Manila', 'Bataan', 'Lingayen', 'Samoa']);
    expect(((setup as any).expeditions as any[]).filter((r) => r[1] === 'USA' && far.has(r[5]))).toEqual([]);
  });
});

describe('the islands by month, with no fighting', () => {
  for (const stop of [1, 3, 5]) {
    it(`stop ${stop}: one division, then the Guard regiments beside it, then two divisions made from them`, () => {
      const g = startOpen(createGame(5, null));
      if (stop !== 3) setStop(g, stop, true);
      expect(sum(g, ISLAND)).toBe(0.7);
      expect(field(g, OUTER)).toEqual([]);
      months(g, 2, 13);
      expect(field(g, ISLAND).map((c) => c.name)).toEqual([HD]);
      months(g, 14, 14);
      // October 1940: new men.
      expect(field(g, ISLAND).map((c) => [c.name, g.regions[c.reg].name, c.full])).toEqual([[HD, 'Pearl Harbor', 0.8], [G298, 'Pearl Harbor', 0.15]]);
      expect(field(g, OUTER).map((c) => [c.name, g.regions[c.reg].name, c.full, c.hold])).toEqual([
        [`${G299}, on Hawaii`, 'Hawaii', 0.05, true], [`${G299}, on Maui`, 'Maui', 0.05, true], [`${G299}, on Kauai`, 'Kauai', 0.05, true]]);
      expect(sum(g, ISLAND)).toBeGreaterThan(0.85);
      expect(sum(g, ISLAND)).toBeLessThanOrEqual(0.95);
      expect(sum(g, OUTER)).toBeCloseTo(0.11, 2);
      months(g, 15, 25);
      // (The chart's own fill, in peace: slowly, to the caps.)
      const before = sum(g, ISLAND);
      expect(before).toBeGreaterThan(0.9);
      expect(before).toBeLessThanOrEqual(0.95);
      expect(sum(g, OUTER)).toBeGreaterThan(0.11);
      expect(sum(g, OUTER)).toBeLessThanOrEqual(0.15);
      months(g, 26, 26);
      // October 1941: the split makes no man. The island reads what it read the month before and one month's fill.
      expect(field(g, ISLAND).map((c) => [c.name, g.regions[c.reg].name, c.full])).toEqual([[D25, 'Honolulu', 0.8], [D24, 'Pearl Harbor', 0.65]]);
      expect(named(g, HD)).toBeUndefined();
      expect(named(g, G298)).toBeUndefined();
      expect(named(g, D25)!.str).toBeGreaterThanOrEqual(0.55);
      expect(named(g, D25)!.str).toBeLessThan(0.6);
      expect(named(g, D24)!.str).toBeGreaterThan(0.36);
      expect(named(g, D24)!.str).toBeLessThan(0.45);
      expect(sum(g, ISLAND)).toBeGreaterThanOrEqual(before);
      expect(sum(g, ISLAND) - before).toBeLessThan(0.09);
      months(g, 27, 29);
      // December 1941 and after: the chart's own fill carries each toward its cap, and never past the record's 1.45 on Oahu.
      expect(sum(g, ISLAND)).toBeGreaterThan(1.0);
      expect(sum(g, ISLAND)).toBeLessThanOrEqual(1.45);
      expect(sum(g, OUTER)).toBeLessThanOrEqual(0.15);
      months(g, 30, 40);
      expect(sum(g, ISLAND)).toBeLessThanOrEqual(1.45);
      expect(sum(g, ISLAND) + sum(g, OUTER)).toBeLessThanOrEqual(1.6);
    });
  }

  // (Moved by the design director's ruling of 13:39Z, 25 November 2026, on the batch 1 readings, point 3 of the dated
  // rows: "Oahu's split: an offer too, yes. A reorganisation of the player's own forces is the player's choice." As
  // first built a player's America had the split made in October 1941 without a word, as a computer's has.)
  it("a player's America has the same rows, and the split is his to take or decline: offered in October 1941", () => {
    const g = startOpen(createGame(5, 'USA'));
    months(g, 2, 26);
    // Nothing is made: the division and the 298th stand as they were, and the two rows are offered, each with the corps it takes.
    expect(field(g, ISLAND).map((c) => c.name)).toEqual([HD, G298]);
    const list = rowOffers(g);
    expect(list.map((o) => [o.name, g.regions[o.region].name, o.since, o.last, o.takes.map((t) => t.name), o.why])).toEqual([
      [D25, 'Honolulu', 26, 32, [G298, HD], null], [D24, 'Pearl Harbor', 26, 32, [HD], null]]);
    expect(field(g, OUTER)).toHaveLength(3);
    // Taken, in the order of the list, the split happens as the rule has it: the 25th takes its share first, the 24th what is left.
    const was = sum(g, ISLAND);
    for (const o of list) expect(takeOffer(g, o.id)).toBeNull();
    expect(field(g, ISLAND).map((c) => [c.name, c.full])).toEqual([[D25, 0.8], [D24, 0.65]]);
    expect(named(g, HD)).toBeUndefined();
    expect(sum(g, ISLAND)).toBeCloseTo(was, 6);
    expect(rowOffers(g)).toEqual([]);
    months(g, 27, 28);
    expect(sum(g, ISLAND)).toBeGreaterThan(0.95);
    expect(sum(g, ISLAND)).toBeLessThanOrEqual(1.45);
  });

  it("declined, his Hawaiian Division stays whole and the two divisions never come out of it; a saved war keeps the offer", () => {
    const g = startOpen(createGame(5, 'USA'));
    months(g, 2, 26);
    const back = loadGame(saveGame(g))!;
    expect(rowOffers(back)).toEqual(rowOffers(g));
    const hd = named(g, HD)!;
    for (const o of rowOffers(g)) expect(declineOffer(g, o.id)).toBe(true);
    months(g, 27, 40);
    expect([named(g, D25), named(g, D24), named(g, HD)]).toEqual([undefined, undefined, hd]);
    expect(rowOffers(g)).toEqual([]);
  });

  it('left unanswered, the offers lapse after their half year, and he is told; his corps stay whole', () => {
    const g = startOpen(createGame(5, 'USA'));
    months(g, 2, 32);
    expect(rowOffers(g).map((o) => o.name)).toEqual([D25, D24]);
    const news = g.headlines.length;
    months(g, 33, 33);
    expect(rowOffers(g)).toEqual([]);
    expect(g.headlines.slice(news).map((h) => h.text).filter((t) => /has lapsed/.test(t))).toEqual([
      `The offer to form ${D25} has lapsed: its time has passed.`, `The offer to form ${D24} has lapsed: its time has passed.`]);
    expect([named(g, D25), named(g, D24), named(g, HD) !== undefined]).toEqual([undefined, undefined, true]);
  });
});

describe('a reorganisation makes nothing from nothing', () => {
  it('with the division gone before the split, the 25th is made of the 298th alone and the 24th does not come', () => {
    const g = startOpen(createGame(5, null));
    months(g, 2, 25);
    const guard = named(g, G298)!.str;
    removeCorps(g, named(g, HD)!);
    months(g, 26, 26);
    expect(named(g, D24)).toBeUndefined();
    expect(named(g, G298)).toBeUndefined();
    // (One month's fill came to it after it was made.)
    expect(named(g, D25)!.str).toBeGreaterThanOrEqual(guard);
    expect(named(g, D25)!.str).toBeLessThan(guard + 0.06);
    expect(sum(g, ISLAND)).toBe(+named(g, D25)!.str.toFixed(3));
  });

  it('with both gone, neither division comes', () => {
    const g = startOpen(createGame(5, null));
    months(g, 2, 25);
    removeCorps(g, named(g, HD)!);
    removeCorps(g, named(g, G298)!);
    months(g, 26, 26);
    expect(field(g, ISLAND)).toEqual([]);
  });

  it('a weakened division gives only what it holds: the 25th takes its share first, the 24th what is left', () => {
    const g = startOpen(createGame(5, null));
    months(g, 2, 25);
    named(g, HD)!.str = 0.3;
    const guard = named(g, G298)!.str;
    months(g, 26, 26);
    // (One month's fill came to it after it was made.)
    const whole = sum(g, ISLAND);
    expect(whole).toBeGreaterThanOrEqual(+(0.3 + guard).toFixed(3));
    expect(whole).toBeLessThan(0.3 + guard + 0.06);
    expect(named(g, D25)!.str).toBeCloseTo(whole, 2);
    expect(named(g, D24)).toBeUndefined();
  });

  it('with Oahu lost the division fights on under its own name: no division is formed at the capital instead', () => {
    const g = startOpen(createGame(5, null));
    months(g, 2, 25);
    const hd = named(g, HD)!;
    for (const nm of ISLAND) g.byName[nm].ctrl = 'JAP';
    months(g, 26, 26);
    expect(named(g, D24)).toBeUndefined();
    expect(named(g, D25)).toBeUndefined();
    expect(hd.dead).toBe(false);
    expect(named(g, HD)).toBe(hd);
  });
});
