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
