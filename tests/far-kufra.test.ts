// The Libyan Desert between Kufra and the Nile carries no army (the design director's note of 14:18Z, 22 November
// 2026): the Karakoram's sibling, by the same form.
//
// The fault it mends: the map-maker draws Kufra's province against Dongola's and Wadi Halfa's, and supply runs as a
// road of held provinces of any length, so Libya fed Italy's army in the Sudan and an Indian division marched from
// Dongola to Kufra and took the Fezzan in the war's second year. In the record only patrols and a small garrison's
// track crossed that desert: Kufra fell from Chad in March 1941 and the Fezzan from Chad in January 1943. So the two
// borders are on the map-maker's list of borders that are not there (far.mjs, GLOBE_NO_BORDER), and no march, attack
// or supply line has a road across. Chad's borders with Kufra stay open. With the far side's switch off there is no
// such ground and nothing here is read.
//
// The director's note of 14:23Z gives the Great Sand Sea the same ruling: Kufra's borders with Egypt's oases
// (Bahariya, Farafra, Kharga, and Siwa on today's chart) are struck too, since only patrols and a car party crossed
// it. So every road out of Kufra runs through Libya or Chad, and none reaches the Nile or the Sudan over the desert.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
// @ts-expect-error the map-maker's own file: plain JavaScript, with no types of its own
import { GLOBE_NO_BORDER } from '../data/campaign/far.mjs';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';
import { farOn } from '../src/campaign/world';

const SUDAN = ['Wadi Halfa', 'Dongola', 'Atbara', 'Port Sudan', 'Khartoum', 'Wad Medani', 'Kassala', 'El Obeid', 'El Fasher', 'Malakal', 'Wau', 'Juba'];
const LIBYA_SOUTH = ['Sebha', 'Murzuk', 'Ghat', 'Waw el Kebir', 'Kufra'];
const OASES = ['Bahariya', 'Farafra', 'Kharga', 'Siwa'];
const NILE = ['Cairo', 'Faiyum', 'Minya', 'Asyut', 'Qena', 'Aswan'];
const grand = (seed = 3): Game => startOpen(createGame(seed, null));
const names = (g: Game, place: string): string[] => g.byName[place].nb.map((j) => g.regions[j].name);

describe('the desert between Kufra and the Nile carries no army (the design director, 14:18Z of 22 November 2026)', () => {
  it('has no border from Kufra into the Sudan, on either side', () => {
    const g = grand();
    expect(farOn(g)).toBe(true);
    for (const nm of [...SUDAN, ...LIBYA_SOUTH]) expect(g.byName[nm], nm).toBeDefined();
    expect(names(g, 'Kufra')).not.toContain('Dongola');
    expect(names(g, 'Kufra')).not.toContain('Wadi Halfa');
    expect(names(g, 'Dongola')).not.toContain('Kufra');
    expect(names(g, 'Wadi Halfa')).not.toContain('Kufra');
    // Nor any other road from Italy's desert into the Sudan: no province of Libya's south lists one of the Sudan's.
    const sudan = new Set(SUDAN.map((nm) => g.byName[nm].id));
    for (const nm of LIBYA_SOUTH) for (const j of g.byName[nm].nb) expect(sudan.has(j), `${nm} still borders ${g.regions[j].name}`).toBe(false);
    for (const nm of SUDAN) for (const j of g.byName[nm].nb) expect(LIBYA_SOUTH.includes(g.regions[j].name), `${nm} still lists ${g.regions[j].name}`).toBe(false);
    // The map-maker's list names the two.
    const has = (a: string, b: string): boolean => (GLOBE_NO_BORDER as [string, string][]).some(([x, y]) => (x === a && y === b) || (x === b && y === a));
    expect([has('Kufra', 'Dongola'), has('Kufra', 'Wadi Halfa')]).toEqual([true, true]);
  });

  it('has no border from Kufra over the Great Sand Sea to Egypt\'s oases, on either side', () => {
    const g = grand();
    for (const nm of [...OASES, ...NILE]) expect(g.byName[nm], nm).toBeDefined();
    for (const nm of OASES) { expect(names(g, 'Kufra'), nm).not.toContain(nm); expect(names(g, nm), nm).not.toContain('Kufra'); }
    const has = (a: string, b: string): boolean => (GLOBE_NO_BORDER as [string, string][]).some(([x, y]) => (x === a && y === b) || (x === b && y === a));
    // (Farafra is a city cut from an oasis province: the list names its province's border.)
    expect([has('Kufra', 'Bahariya'), has('Kufra', 'Kharga'), has('Kufra', 'Siwa')]).toEqual([true, true, true]);
    // The oases keep their own roads, to one another and to the Nile.
    expect(names(g, 'Bahariya').sort()).toEqual(['Faiyum', 'Farafra', 'Minya', 'Qattara Depression', 'Siwa']);
    expect(names(g, 'Farafra').sort()).toEqual(['Asyut', 'Bahariya', 'Kharga']);
    expect(names(g, 'Kharga').sort()).toEqual(['Aswan', 'Asyut', 'Farafra', 'Qena', 'Wadi Halfa']);
    expect(names(g, 'Siwa').sort()).toEqual(['Bahariya', 'Mersa Matruh', 'Qattara Depression']);
  });

  it('every road out of Kufra runs through Libya or Chad: none reaches the Nile or the Sudan over the desert', () => {
    const g = grand();
    expect(names(g, 'Kufra').sort()).toEqual(['Faya-Largeau', 'Jalo', 'Jarabub', 'Waw el Kebir', 'Zouar']);
    // Counted outwards from Kufra over ground that is neither Chad's nor the rest of Libya's, nothing is reached at all.
    const stop = new Set(['Faya-Largeau', 'Zouar', 'Waw el Kebir', 'Jalo', 'Jarabub'].map((nm) => g.byName[nm].id));
    const seen = new Set([g.byName.Kufra.id]), q = [g.byName.Kufra.id];
    for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (!stop.has(j) && !seen.has(j)) { seen.add(j); q.push(j); }
    expect(q.map((id) => g.regions[id].name)).toEqual(['Kufra']);
    for (const nm of [...NILE, ...SUDAN, ...OASES]) expect(seen.has(g.byName[nm].id), nm).toBe(false);
  });

  it("leaves Chad's road to Kufra open, and the Sudan's own roads as they were", () => {
    const g = grand();
    // The record's road: Kufra fell to a column from Chad.
    expect(names(g, 'Kufra')).toContain('Faya-Largeau');
    expect(names(g, 'Kufra')).toContain('Zouar');
    expect(names(g, 'Kufra')).toContain('Waw el Kebir');
    // Nothing else of the chart there moves: the Nile's line and the Sudan's west.
    expect(names(g, 'Dongola').sort()).toEqual(['Atbara', 'El Fasher', 'Faya-Largeau', 'Khartoum', 'Wadi Halfa']);
    expect(names(g, 'Wadi Halfa').sort()).toEqual(['Aswan', 'Atbara', 'Dongola', 'Kharga', 'Port Sudan']);
  });
});
