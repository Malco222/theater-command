// City defences (release 2; Brian 24 November 2026 22:08Z, card 22:11Z "Both"; city defences, the design director's
// ruling of 25 Nov 2026 02:23Z). One switch, 'cityDefences' (stage.json): searchlights to build beside the
// anti-aircraft guns, guns that fire by night by what they can see, a garrison that fortifies the town it holds, and
// the town's heavy guns firing on attacking tanks. Every figure is in data/campaign/citydefences.json; the work's own
// row (label, cost, months, upkeep, blurb) is arsenal.json works.lights. One rule for every nation: nothing here reads
// a nation or a date. Off: nothing of it is read, no flag is written and every war is the war without it.

import data from '../../data/campaign/citydefences.json';
import type { Game } from './types';

/** A row of the history start's table: searchlights laid where the historian reads a city belt at that date. */
export interface LightsRow { region: string; level: number; why: string }

export const CITYDEF = data as unknown as {
  /** The share of a built gun level's day worth by night: dark, radar, lit (no radar), both. */
  night: { dark: number; radar: number; lit: number; both: number };
  /** Bombers over the target one level of searchlights can hold. */
  saturation: number;
  /** The night fighters' floor by searchlights level, 0 to 3. */
  fighters: number[];
  works: { most: number; worth: number };
  ground: { armour: number; most: number; lost: number };
  ai: { months: number };
  start: LightsRow[];
};

/** The most levels of searchlights a province can have (the guns' own top). */
export const LIGHTS_TOP = 3;

/** True where the city defences are in play: a game carrying the flag 'cityDefences'. Every new rule reads it first. */
export const cityDefences = (g: Game): boolean => g.flags.cityDefences === 1;

/** True where a grand campaign carries the city defences (stage.json 'cityDefences'), as settledTownsOn. */
export const cityDefencesOn = (g: Game): boolean => g.flags.open === 1 && cityDefences(g);

/**
 * Turns the city defences on or off in a campaign under way. What was built stays built either way: off, the
 * searchlights' marks stay on the ground and nothing reads them, offers them or bills them.
 */
export function setCityDefences(g: Game, on: boolean): void {
  if (!on) { delete g.flags.cityDefences; return; }
  g.flags.cityDefences = 1;
}

/** The level of searchlights in a province, 0 to 3: always 0 with the switch off, whatever marks the ground keeps. */
export const lightsAt = (g: Game, region: number): number => (cityDefences(g) ? g.flags[`wk:lights:${region}`] ?? 0 : 0);

/**
 * The history start (city defences, the design director's ruling of 25 Nov 2026 02:23Z): with the switch on, a
 * campaign opens with searchlights where the historian reads a city belt at that date (citydefences.json 'start',
 * province by name), at the level he reads; none where he finds none, and never by nation. A province keeps the
 * higher level, as the start guns do (airversion.ts). The table is empty until he reads. Off: nothing.
 */
export function openCityDefences(g: Game, rows: LightsRow[] = CITYDEF.start): void {
  if (!cityDefences(g)) return;
  for (const row of rows) {
    const r = g.regions.find((x) => x.name === row.region);
    const lv = Math.max(0, Math.min(LIGHTS_TOP, Math.floor(row.level)));
    if (!r || lv <= 0) continue;
    g.flags[`wk:lights:${r.id}`] = Math.max(g.flags[`wk:lights:${r.id}`] ?? 0, lv);
  }
}
