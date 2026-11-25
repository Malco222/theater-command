// City defences (release 2; Brian 24 November 2026 22:08Z, card 22:11Z "Both"; city defences, the design director's
// ruling of 25 Nov 2026 02:23Z). One switch, 'cityDefences' (stage.json): searchlights to build beside the
// anti-aircraft guns, guns that fire by night by what they can see, a garrison that fortifies the town it holds, and
// the town's heavy guns firing on attacking tanks. Every figure is in data/campaign/citydefences.json; the work's own
// row (label, cost, months, upkeep, blurb) is arsenal.json works.lights. One rule for every nation: nothing here reads
// a nation or a date. Off: nothing of it is read, no flag is written and every war is the war without it.

import data from '../../data/campaign/citydefences.json';
import { gunsAt } from './air';
import { AC } from './aircampaigns';
import { hasTech } from './tech';
import type { Game } from './types';
import type { Sky } from './weather';

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

// ---- The guns by night (slice 2) ------------------------------------------------------------------------------------

/**
 * How the built guns at a target fight a night raid (city defences, the design director's ruling of 25 Nov 2026
 * 02:23Z, choices 1, 2 and 5). `guns` the built gun levels and `lights` the searchlights' levels there; `radar` true
 * where the province's holder has Fighter control (aircampaigns.json radar.control), a research row, never a date;
 * `lit` the gun levels a searchlight holds a bomber for, under a clear sky only; `sat` what is left of the lights'
 * work against the bombers over the target (one level holds 'saturation' of them); `share` the guns' night worth as
 * a share of their day worth, every level blended; `litShare` (0 to 1) the share of the gun levels firing by
 * searchlight after the too-many-bombers cut: none without radar, for lit without radar is ruled as dark.
 */
export interface CityNight { guns: number; lights: number; radar: boolean; cloud: boolean; lit: number; sat: number; share: number; litShare: number }

export function cityNight(g: Game, target: number, bombers: number, sky: Sky): CityNight {
  const N = CITYDEF.night;
  const L = gunsAt(g, target), Ls = lightsAt(g, target);
  const ctrl = g.regions[target]?.ctrl;
  const radar = !!ctrl && hasTech(g, ctrl, AC.radar.control);
  const clear = sky === 'clear';
  const lit = clear ? Math.min(L, Ls) : 0;
  const sat = Math.min(1, (CITYDEF.saturation * Ls) / Math.max(1, bombers));
  const d = radar ? N.radar : N.dark, t = radar ? N.both : N.lit;
  const share = L > 0 ? ((L - lit) * d + lit * (d + (t - d) * sat)) / L : d;
  return { guns: L, lights: Ls, radar, cloud: Ls > 0 && !clear, lit, sat, share, litShare: radar && L > 0 ? (lit * sat) / L : 0 };
}

/**
 * The share of a built gun level's day worth by night at a target, under the switch (the table of the ruling, choice
 * 1): it replaces the night guns' half of ruling 107(b) rather than riding on it, one cause, one price. Cloud keeps
 * its own price beside it (weather, ruling 127).
 */
export const cityNightShare = (g: Game, target: number, bombers: number, sky: Sky): number => cityNight(g, target, bombers, sky).share;

/**
 * The night fighters' floor over a target lit by searchlights (choice 4, the floor): a share of their day worth by
 * the lights' level, under a clear sky only; 0 under cloud or with the switch off. Lights serve the fighters with no
 * guns and no radar. A floor, never a multiplier: the caller takes the larger of it and the fighters' own figure.
 */
export const lightsFloor = (g: Game, target: number, sky: Sky): number => (cityDefences(g) && sky === 'clear' ? CITYDEF.fighters[lightsAt(g, target)] ?? 0 : 0);
