// The convoy night's figures (data/battle/convoy.json, each marked there) and the sky's arithmetic.

import convoyData from '../../../data/battle/convoy.json';
import { incidentRate } from '../../campaign/navy';
import type { ConvoyHabits, ConvoyKit } from './types';

export const CONVOY = convoyData as unknown as {
  tick: number;
  check: number;
  night: { least: number; most: number };
  convoy: { ships: Record<string, number>; most: number; perColumn: number; columnGap: number; shipGap: number; speed: number };
  boat: { surfaced: number; cruise: number; dived: number; creep: number; scope: number; deep: number; crush: number; diveTime: number; tubes: number; salvo: number; load: number; reload: number; fireAt: number; lookout: number; home: number; len: number; beam: number };
  escort: { screen: number; top: number; hunt: number; radar: number; asdic: number; arc: number; deaf: number; blind: number; pattern: number; ahead: number; spread: number; open: number; station: number; keepAway: number; gun: number; ram: number; huntFor: number; groupFor: number; lostFor: number; len: number; beam: number };
  detect: { radar: number; sight: number; asdic: number; deep: number; slip: number; boat: number };
  torpedo: { speed: number; hit: number; escortHit: number; escortSinks: number; column: number };
  hunt: { patterns: number; hurt: number; harm: number; forced: number };
  surface: { gunAt: number; gunEvery: number; boatHits: number; boatHarm: number; escortHits: number; escortHarm: number; fight: number; scuttleIn: [number, number]; repair: number; escape: number; strike: number; board: number; taken: number; ramShare: number; merchantGuns: number; merchantHits: number };
  light: { dark: number; moonlit: number; weather: [number, number] };
  arrive: { home: number; alone: number };
  doctrine: Record<string, ConvoyHabits | string>;
  guns: Record<string, number | string>;
  crew: { ss: number; mv: number };
  wakeless: string[];
};

/** A navy's staff habits as the history start seeds them (convoy.json doctrine), or the defaults. */
export function habitsOf(nat: string): ConvoyHabits {
  const d = CONVOY.doctrine[nat];
  return typeof d === 'object' ? { ...d } : { ...(CONVOY.doctrine.default as ConvoyHabits) };
}

/** A boat's deck gun, millimetres (a look). */
export const gunMmOf = (nat: string): number => { const g = CONVOY.guns[nat]; return typeof g === 'number' ? g : (CONVOY.guns.default as number); };

/**
 * The chance that one attack on a boat kills her, by the hunter's kit and whether a group hunts her: the card's
 * incident rate (navy.ts incidentRate, OEG T10, convoycause.json) spread over an incident's attacks (convoy.json hunt).
 */
export function attackKills(k: ConvoyKit, group: boolean): number {
  const R = incidentRate({ ...k, patrol: false }, group);
  return 1 - Math.pow(1 - R, 1 / CONVOY.hunt.patterns);
}

// ---- The sky (geography and the calendar, not a rule) ---------------------------------------------------------

const DAYS = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
const RAD = Math.PI / 180;

/** Days in a month (0 = January) of a year. */
export const daysIn = (year: number, month: number): number => DAYS[month] + (month === 1 && year % 4 === 0 && (year % 100 !== 0 || year % 400 === 0) ? 1 : 0);

/** The day of the year, 1 on 1 January. */
function dayOfYear(year: number, month: number, day: number): number {
  let n = day;
  for (let m = 0; m < month; m++) n += daysIn(year, m);
  return n;
}

/** The sun's declination on a day, degrees (a plain cosine fit, within about a degree: enough for the length of a night). */
export const declination = (doy: number): number => -23.44 * Math.cos((2 * Math.PI * (doy + 10)) / 365);

/** Hours from sunset to sunrise at a latitude on a day (the sun's upper edge at the horizon, refraction counted), held to convoy.json's night. */
export function nightHours(lat: number, year: number, month: number, day: number): number {
  const d = declination(dayOfYear(year, month, day)) * RAD, p = lat * RAD, h0 = -0.833 * RAD;
  const c = (Math.sin(h0) - Math.sin(p) * Math.sin(d)) / (Math.cos(p) * Math.cos(d));
  const day12 = c <= -1 ? 24 : c >= 1 ? 0 : (2 * Math.acos(c)) / RAD / 15;
  return Math.max(CONVOY.night.least, Math.min(CONVOY.night.most, 24 - day12));
}

/** The Julian day at noon of a date (month 0 = January). */
function julian(year: number, month: number, day: number): number {
  let y = year, m = month + 1;
  if (m <= 2) { y -= 1; m += 12; }
  const a = Math.floor(y / 100), b = 2 - a + Math.floor(a / 4);
  return Math.floor(365.25 * (y + 4716)) + Math.floor(30.6001 * (m + 1)) + day + b - 1524.5 + 0.5;
}

/** The share of the moon lit on a date's night, 0 new to 1 full (her age from a known new moon, 6 January 2000). */
export function moonLit(year: number, month: number, day: number): number {
  const synodic = 29.530588853;
  let age = (julian(year, month, day) - 2451550.1) % synodic;
  if (age < 0) age += synodic;
  return (1 - Math.cos((2 * Math.PI * age) / synodic)) / 2;
}
