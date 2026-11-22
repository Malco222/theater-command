// The month's submarine scene (form D1, ruling 241): LOOK ONLY.
//
// When the player's own submarines or escorts have sunk ships or been sunk since his last month card, his month opens
// with a short night scene of it. This module stages that scene: it reads the month's facts (the news the rules have
// already written, and later the record-only fields the rules builder adds under `subScene`), fixes the outcome first,
// and writes a staged recording in the sea scene's recording format with the form's §5 names. There are no dice: every
// look value is a hash of the campaign's seed, the month and the fact (as the rules' `mix` look fields are), so the
// same facts give the same bytes. Nothing here is read by a rule, and nothing here writes to the game: it never changes
// a rule, a random draw, a save or the news (the viewing setting `subScene` lives outside the rules' save).
//
// The scene (form D1, "Shows"): the convoy in columns or the torpedoed fleet (8 s); a torpedo fan into a merchant or a
// ship of the class sunk (10 s); a depth-charge run whose bursts stay visibly clear unless a kill is booked (12 s); for
// a booked kill only, a boat crushed or scuttled, by aircraft where the kill's share is theirs (10 s). Up to 3 each of
// heavy ships, boats and merchant hits. 30-45 s on screen. The caption: the month's tons, and ships at about 5,200 GRT
// a ship (marked: to be checked by the historian before shipping).
//
// The recording's clock is battle time in seconds, as the fleet recordings' is; `field.staged.beats` names the windows
// of it the scene shows and the screen seconds each is given, so each window plays at (to - from) / screen. Between
// windows nothing the scene needs happens, and it may cut.
//
// Names, as the sea scene's reader takes them at 2cfd75b8 (SeaFeed.cpp ReadField, ReadPiece, ReadEvent), from form §5
// and field-side-read.md §3. A submarine's depth is never named `d` or `depth` (`d` is a piece's column length,
// SeaFeed.cpp:102): it is `keel`.
//   ss extra: dive ('surfaced' | 'periscope' | 'deep'), keel, scope, tubes, heard
//   escort extra: huntPhase ('search' | 'run' | 'blind' | 'open'), huntUntil, echo {on, kind, range, bearing, err}
//     while the run lasts
//   merchant extra: column
//   events: sea:dive {dive, keel, why}, sea:sonar {on, kind, range, bearing, err, lost?}, sea:torpedo {target, x, y,
//     running, share, fish, spread, wakeless, dive}, sea:torpedoHit {from}, sea:depthCharge {on, x, y, n, set,
//     sinking, spread, kind: 'pattern' | 'ahead'}, sea:depthChargeHit {from, harm, keel}, sea:sunk {cls, fleet, how?},
//     sea:report {by, at}, sea:starshell {by, x, y}, sea:rescue {by, on}, sea:air (an aircraft's attack on a boat)
//   zones: torpedo, sunk, pattern, oil, datum
//   header: classes (this scene's own table), subs {scope, deep, crush, dived}, dc {sink, lethal},
//     light {sun {up, from}, moon {lit, up, from}, vis}
// Readings the reader fixed where the form left them open: a salvo's `spread` is degrees across (a pattern's, metres);
// `sea:report.at` is the piece reported (the reader takes it as `on`); `light.sun.up` and `light.moon.up` are heights
// in degrees and `from` bearings in degrees clockwise from north; `vis` metres. Bearings in `sea:sonar` and `echo` are
// radians clockwise from north, as a piece's `face`; `err` radians.
//
// Each view carries a piece whole the first time it shows; after that only what changed since the view before (the
// reader keeps the rest: "What the view left out stands as the piece was last told", SeaFeed.cpp ReadPiece), except a
// boat's dive, keel and scope and a lost piece's `sunk`, which the reader takes afresh each view and so are in every one.

import LOOKS from './sub-scene-looks.json';
import EUROPE_SEAS from '../../../data/campaign/seas.json';
import WORLD_SEAS from '../../../data/campaign/world-seas.json';
import FAR_SEAS from '../../../data/campaign/far-seas.json';
import { turnToDate } from '../../../src/core/ticket';
import { wholeShares } from '../../../src/campaign/navy';
import type { Game, Headline } from '../../../src/campaign/types';

// ---- The month's facts ------------------------------------------------------------------------------------------

export type FactKind = 'torpedo' | 'hunt' | 'convoy';
export type HeavyClass = 'bb' | 'cv' | 'ca';

/**
 * One thing that happened at sea this month, as the rules have already booked it. Read from the news today (the
 * torpedoing's, the hunt's and the convoy month's headlines, navy.ts 1925-2097) and from `g.convoys`; the fields under
 * "record-only" are form D1's, which the rules builder adds under `subScene` and which this module reads when present.
 */
export interface SeaFact {
  kind: FactKind;
  /** The sea's key ('natl'); '' when the news names no sea we know. */
  sea: string;
  /** The month (g.turn) the news is dated, and its week as booked on the headline (Headline.week, never newsWeek). */
  turn: number;
  week: number;
  /** The side that struck: the hunters (hunt). '' where the rules name none: a torpedoing, a convoy month. */
  by: string;
  /** The side struck: the torpedoed fleet's nation; the boats' (hunt); the convoy route's owner (convoy). */
  on: string;
  /** A torpedoing: the class sunk, the fleet, and its ships afloat as the month closed (look only), where found. */
  cls?: HeavyClass;
  fleet?: string;
  ships?: Partial<Record<'bb' | 'cv' | 'ca' | 'dd' | 'ss', number>>;
  /** Boats sunk (hunt; a convoy month's subsLost) and destroyers lost (the hunt's cost; escortsLost). */
  boats?: number;
  escortsLost?: number;
  /** A convoy month: tons sunk, and the route. */
  tons?: number;
  route?: string;
  routeName?: string;
  /**
   * Submarines at sea in this sea as the month closed, by nation (read from g.fleets; for a convoy month only the
   * raiding and patrolling ones): what stands in for the record-only attackers and raiders below until the rules keep
   * them. Look only.
   */
  boatsAt?: Record<string, number>;
  // Record-only fields (form D1: "hunters, aircraft and quarry per sea (huntersIn, 1870-1898); a torpedoing's possible
  // attackers (as torpedoRisk, 1966-1972), one picked on look:torpedo:<fleet>; a convoy month's raiders, escorts, air
  // guard and tons by sea"). Until the rules builder adds them, `boatsAt` and the data rows in sub-scene-looks.json
  // stand in.
  /** huntersIn's weight in the sea, split into ships and aircraft (hunt; and a convoy month's guard). */
  hunters?: { ships: number; aircraft: number };
  /** A torpedoing's possible attackers, the one picked on look:torpedo:<fleet> first. */
  attackers?: string[];
  /** A convoy month's raider and escort nations, the largest first, its air guard, and tons sunk by sea. */
  raiders?: string[];
  escorts?: string[];
  airGuard?: number;
  tonsBy?: Record<string, number>;
}

/**
 * The record-only fields as the rules builder is to keep them under `subScene` (form D1), this month and last; read
 * when present (`g.subScene`), never written here. Names the builder may change: these are what the stager reads.
 */
export interface SubSceneRecord {
  /** huntersIn per sea and hunting nation: its ships' and aircraft's weight, the quarry's nation and the boats sunk. */
  hunts?: { turn?: number; nat: string; sea: string; hunters: number; aircraft: number; quarry: string; sunk?: number }[];
  /** A torpedoing: the fleet, its nation and sea, the nations whose boats could have fired (torpedoRisk's), and the one picked on look:torpedo:<fleet>. */
  torpedoes?: { turn?: number; fleet: number; nat: string; sea: string; attackers: string[]; picked?: string }[];
  /** A convoy route-month: raiders and escorts by nation (largest first), the air guard, and tons sunk by sea. */
  convoys?: { turn?: number; route: string; raiders: string[]; escorts: string[]; airGuard: number; tons: Record<string, number> }[];
}

export interface MonthFacts {
  /** The player's nation. */
  human: string;
  /** The campaign's seed (g.seed): only ever hashed, with the month and the fact, for the look. */
  seed: number;
  /** The month the facts close (g.turn). */
  turn: number;
  facts: SeaFact[];
  /** Who is at war with whom, for the nations the facts name and the player's. */
  foes: Record<string, string[]>;
  /** Nations' names and adjectives, for the words. */
  names?: Record<string, { name: string; adj: string }>;
}

// ---- Data rows -------------------------------------------------------------------------------------------------

type Dated = { from: string; basin?: string };
type Dive = 'surfaced' | 'periscope' | 'deep';
interface BoatRow extends Dated { cls: string; len: number; beam: number; draught: number; surf: number; dived: number; tubes: number; fan: number; range: number; attack: 'surfaced' | 'periscope'; deep: number; crush: number; pack: number; report: boolean }
interface FishRow extends Dated { fish: string; speed: number; wakeless: boolean }
interface EscortRow extends Dated { cls: string; len: number; beam: number; speed: number; turn: number; n: [number, number]; group: [number, number]; pattern: number; spread: number; set: [number, number]; sink: number; ahead: string | null; radar: boolean; starshell: boolean; contact: [number, number]; lost: number; sonar?: number }
interface ConvoyRow extends Dated { n: [number, number]; cols: [number, number]; speed: number }
interface AirRow extends Dated { name: string; share: number }

const L = LOOKS as unknown as {
  caption: { grt: number; check: string };
  beats: { convoy: number; fire: number; hits: number; drop: number; bursts: number; charges: number; end: number; pace: number; least: number; most: number };
  most: { heavy: number; boats: number; hits: number; escorts: number; merchants: number };
  convoy: { mv: { name: string; len: number; beam: number; draught: number; turn: number }; colGap: number; rowGap: number };
  heavy: Record<'bb' | 'cv' | 'ca' | 'dd', { len: number; beam: number; speed: number; turn: number }>;
  basins: { seas: Record<string, string[]>; navies: Record<string, [string, string, string][]> };
  boats: Record<string, BoatRow[]>;
  torpedoes: Record<string, FishRow[]>;
  escorts: Record<string, EscortRow[]>;
  convoys: Record<string, ConvoyRow[]>;
  air: Record<string, AirRow[]>;
  ends: { escort: number; air: number };
};

/** A date as months since January 1939 (0 = January 1939), from 'YYYY-MM'. */
const monthsOf = (s: string): number => { const [y, m] = s.split('-').map(Number); return (y - 1939) * 12 + (m - 1); };

/**
 * The row of a navy's table that stands at a date in a basin: the latest 'from' not after it, a basin's own row
 * first among equals; before a navy's first row, its first. A navy without rows takes the small navies' '_default'.
 */
export function rowOf<T extends Dated>(table: Record<string, T[]>, nat: string, at: number, basin: string): T {
  const rows = (hasRows(table, nat) ? table[nat] : table._default).filter((r) => !r.basin || r.basin === basin);
  let best: T | null = null;
  for (const r of rows) {
    if (monthsOf(r.from) > at) continue;
    if (!best || monthsOf(r.from) > monthsOf(best.from) || (monthsOf(r.from) === monthsOf(best.from) && !!r.basin && !best.basin)) best = r;
  }
  return best ?? [...rows].sort((a, b) => monthsOf(a.from) - monthsOf(b.from))[0] ?? table._default[0];
}

/** Whether a navy has rows of its own in a table (not the small navies' default). */
const hasRows = (table: Record<string, unknown>, nat: string): boolean => Object.prototype.hasOwnProperty.call(table, nat) && nat !== '_default' && !nat.startsWith('_');

/** The look rows a navy gets at a date in a basin (for the tests and the notes: every navy gets a sensible look). */
export function looksOf(nat: string, at: number, basin: string): { boat: BoatRow; fish: FishRow; escort: EscortRow; convoy: ConvoyRow; air: AirRow } {
  return { boat: rowOf(L.boats, nat, at, basin), fish: rowOf(L.torpedoes, nat, at, basin), escort: rowOf(L.escorts, nat, at, basin), convoy: rowOf(L.convoys, nat, at, basin), air: rowOf(L.air, nat, at, basin) };
}

// ---- Seas -----------------------------------------------------------------------------------------------------

export interface SeaPlace { key: string; name: string; lat: number; lon: number; basin: string }

type ZoneRow = { id: string; name: string; at?: [number, number] };
const SEAS: Record<string, SeaPlace> = (() => {
  const out: Record<string, SeaPlace> = {};
  const add = (zones: ZoneRow[], chart: 'europe' | 'world' | 'far'): void => {
    for (const z of zones) {
      if (!z.at || out[z.id]) continue;
      let basin = chart === 'far' ? 'pacific' : 'atlantic';
      for (const [b, keys] of Object.entries(L.basins.seas)) if (keys.includes(z.id)) basin = b;
      out[z.id] = { key: z.id, name: z.name, lat: z.at[0], lon: z.at[1], basin };
    }
  };
  add((EUROPE_SEAS as unknown as { zones: ZoneRow[] }).zones, 'europe');
  add((WORLD_SEAS as unknown as { zones: ZoneRow[] }).zones, 'world');
  add((FAR_SEAS as unknown as { zones: ZoneRow[] }).zones, 'far');
  return out;
})();

/** A sea by key; an unknown key is the open North Atlantic (look only). */
export const seaPlace = (key: string): SeaPlace => SEAS[key] ?? { key, name: 'open sea', lat: 50, lon: -25, basin: 'atlantic' };

// ---- The look's hashes (no dice) -----------------------------------------------------------------------------

/** A 32-bit FNV-1a hash with a murmur finaliser. */
function hash32(s: string): number {
  let h = 0x811c9dc5;
  for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 0x01000193) >>> 0; }
  h ^= h >>> 16; h = Math.imul(h, 0x85ebca6b) >>> 0; h ^= h >>> 13; h = Math.imul(h, 0xc2b2ae35) >>> 0; h ^= h >>> 16;
  return h >>> 0;
}
/** A look value in [0, 1) for a label: each label its own hash, so adding one never moves another. */
const looker = (key: string) => (label: string): number => hash32(`${key}|${label}`) / 4294967296;
const pickIn = (r: [number, number], u: number): number => Math.round(r[0] + (r[1] - r[0]) * u);
const clamp = (x: number, lo: number, hi: number): number => Math.max(lo, Math.min(hi, x));
/** A whole number with thousands marked (no locale: the game's script host may have none). */
const thousands = (n: number): string => String(Math.round(n)).replace(/\B(?=(\d{3})+(?!\d))/g, ',');

// ---- Night by month and latitude (look only) -----------------------------------------------------------------

const RAD = Math.PI / 180;
const wrapDeg = (x: number): number => ((x % 360) + 360) % 360;

/** Julian day at an hour (UT) of a date (month0 0 = January). */
function julian(year: number, month0: number, day: number, hourUT: number): number {
  let y = year, m = month0 + 1;
  if (m <= 2) { y -= 1; m += 12; }
  const a = Math.floor(y / 100), b = 2 - a + Math.floor(a / 4);
  return Math.floor(365.25 * (y + 4716)) + Math.floor(30.6001 * (m + 1)) + day + b - 1524.5 + hourUT / 24;
}

/** The sun's ecliptic longitude, right ascension and declination (degrees) at a Julian day (the almanac's short series). */
function sunAt(jd: number): { lam: number; ra: number; dec: number; eqt: number } {
  const n = jd - 2451545.0;
  const Lm = wrapDeg(280.46 + 0.9856474 * n), g = wrapDeg(357.528 + 0.9856003 * n) * RAD;
  const lam = wrapDeg(Lm + 1.915 * Math.sin(g) + 0.02 * Math.sin(2 * g));
  const eps = (23.439 - 0.0000004 * n) * RAD;
  const ra = wrapDeg(Math.atan2(Math.cos(eps) * Math.sin(lam * RAD), Math.cos(lam * RAD)) / RAD);
  const dec = Math.asin(Math.sin(eps) * Math.sin(lam * RAD)) / RAD;
  let eqt = Lm - ra; // degrees: the equation of time
  if (eqt > 180) eqt -= 360;
  if (eqt < -180) eqt += 360;
  return { lam, ra, dec, eqt };
}

/** Height and bearing (degrees, bearing clockwise from north) of a body at an hour angle and declination. */
function upFrom(lat: number, ha: number, dec: number): { up: number; from: number } {
  const p = lat * RAD, h = ha * RAD, d = dec * RAD;
  const up = Math.asin(Math.sin(p) * Math.sin(d) + Math.cos(p) * Math.cos(d) * Math.cos(h)) / RAD;
  const from = wrapDeg(Math.atan2(-Math.cos(d) * Math.sin(h), Math.sin(d) * Math.cos(p) - Math.cos(d) * Math.cos(h) * Math.sin(p)) / RAD);
  return { up, from };
}

export interface Night {
  /** Hours from sunset to sunrise (sun 0.833 degrees below the horizon); 24 in a polar night, 0 under the midnight sun. */
  hours: number;
  /** Local solar hours of sunset and sunrise (sunrise past midnight counts on from 24). */
  dusk: number;
  dawn: number;
  /** The staged hour (local solar time, 0-24), and the sun and moon then. */
  hour: number;
  sun: { up: number; from: number };
  moon: { up: number; from: number; lit: number };
}

/** The night of a date at a place, and the hour the scene is staged at (`u` in [0, 1) places it in the dark hours). */
export function nightOf(lat: number, lon: number, year: number, month0: number, day: number, u: number): Night {
  const noon = sunAt(julian(year, month0, day, 12 - lon / 15));
  const p = lat * RAD, d = noon.dec * RAD;
  const c = (Math.sin(-0.833 * RAD) - Math.sin(p) * Math.sin(d)) / (Math.cos(p) * Math.cos(d));
  const solarNoon = 12 - noon.eqt / 15;
  let hours: number, dusk: number, dawn: number, hour: number;
  if (c >= 1) { hours = 24; dusk = solarNoon - 12; dawn = solarNoon + 12; hour = 24 * (0.15 + 0.7 * u); }
  // (an attack under the midnight sun is staged at its lowest, as near dark as the night comes)
  else if (c <= -1) { hours = 0; dusk = solarNoon + 12; dawn = solarNoon + 12; hour = solarNoon + 12; }
  else {
    const h0 = Math.acos(c) / RAD / 15;
    hours = 24 - 2 * h0; dusk = solarNoon + h0; dawn = solarNoon + 24 - h0;
    hour = dusk + hours * (0.25 + 0.5 * u);
  }
  const local = ((hour % 24) + 24) % 24;
  const jd = julian(year, month0, day, local - lon / 15);
  const s = sunAt(jd);
  const ha = (local - solarNoon) * 15;
  const sun = upFrom(lat, ha, s.dec);
  // The moon: her age from a known new moon (6 January 2000, 18:14 UT), placed that far east of the sun along the
  // ecliptic (her own 5 degree tilt left out: look only).
  const SYNODIC = 29.530588853;
  const age = (((jd - 2451550.26) % SYNODIC) + SYNODIC) % SYNODIC;
  const lit = (1 - Math.cos((2 * Math.PI * age) / SYNODIC)) / 2;
  const mlam = wrapDeg(s.lam + (360 * age) / SYNODIC) * RAD, eps = 23.439 * RAD;
  const mra = wrapDeg(Math.atan2(Math.cos(eps) * Math.sin(mlam), Math.cos(mlam)) / RAD);
  const mdec = Math.asin(Math.sin(eps) * Math.sin(mlam)) / RAD;
  const moon = upFrom(lat, ha - (mra - s.ra), mdec);
  return { hours, dusk, dawn, hour: local, sun, moon: { ...moon, lit } };
}

// ---- Picking the scene (form D1, "Trigger") ------------------------------------------------------------------

export type SceneCase = 'heavyLost' | 'boatsKill' | 'convoy' | 'hunt';

export interface ScenePick {
  case: SceneCase;
  fact: SeaFact;
  /** The boats' navy (the pack side, 'att') and the guard's (the escort side, 'def'), and which is the player's. */
  boats: string;
  guard: string;
  me: 'att' | 'def';
  /** Boats booked sunk in the scene's sea this month, and the hits booked: merchants (a convoy month) or heavy ships. */
  kills: number;
  ships: number;
  heavy: HeavyClass[];
  /** Where the boats' navy came from: the rules' record, the boats at sea as the month closed, or the data rows. */
  source: 'record' | 'fleets' | 'rows';
}

const atWarIn = (m: MonthFacts, a: string, b: string): boolean => (m.foes[a] ?? []).includes(b) || (m.foes[b] ?? []).includes(a);
/** Months since January 1939 of a turn (turnToDate: turn 1 is September 1939). */
const monthsAt = (turn: number): number => { const d = turnToDate(turn); return (d.year - 1939) * 12 + d.month; };

/** The navies whose boats work in a basin at a date, in the data rows' order. */
function basinNavies(basin: string, at: number): string[] {
  return (L.basins.navies[basin] ?? []).filter(([, a, b]) => monthsOf(a) <= at && at <= monthsOf(b)).map(([n]) => n);
}

/** The nations with boats there at war with a side, the most boats first (ties by name). */
function boatsAgainst(m: MonthFacts, f: SeaFact, struck: string): string[] {
  const at = f.boatsAt ?? {};
  return Object.keys(at).filter((n) => at[n] > 0 && atWarIn(m, n, struck)).sort((a, b) => at[b] - at[a] || (a < b ? -1 : 1));
}

/** The boats' navy against a side in a sea, from the record, the boats there, or the basin's rows (its first navy at war with it). */
function raiderOf(m: MonthFacts, f: SeaFact, struck: string, recorded?: string[]): { nat: string; source: ScenePick['source'] } {
  if (recorded?.length) return { nat: recorded[0], source: 'record' };
  const there = boatsAgainst(m, f, struck);
  if (there.length) return { nat: there[0], source: 'fleets' };
  const sea = seaPlace(f.sea), at = monthsAt(f.turn);
  for (const n of basinNavies(sea.basin, at)) if (atWarIn(m, n, struck)) return { nat: n, source: 'rows' };
  const foes = [...new Set([...(m.foes[struck] ?? []), ...Object.keys(m.foes).filter((k) => (m.foes[k] ?? []).includes(struck))])].sort();
  return { nat: foes.find((n) => hasRows(L.boats, n)) ?? foes[0] ?? '_default', source: 'rows' };
}

/** Whether the player's boats could be the ones in a sea: by the record, by his boats there, or by the basin's rows. */
function hisBoatsThere(m: MonthFacts, f: SeaFact, recorded?: string[]): boolean {
  const me = m.human;
  if (recorded) return recorded.includes(me);
  if (f.boatsAt) return (f.boatsAt[me] ?? 0) > 0;
  return basinNavies(seaPlace(f.sea).basin, monthsAt(f.turn)).includes(me);
}

const factOrder = (a: SeaFact, b: SeaFact): number => a.turn - b.turn || a.week - b.week || (a.sea < b.sea ? -1 : a.sea > b.sea ? 1 : 0) || (a.kind < b.kind ? -1 : a.kind > b.kind ? 1 : 0) || (a.on < b.on ? -1 : a.on > b.on ? 1 : 0);

/** The boats sunk this month in a sea from a navy (the hunts' facts). */
const huntedIn = (m: MonthFacts, sea: string, nat: string): number => m.facts.filter((f) => f.kind === 'hunt' && f.sea === sea && f.on === nat).reduce((s, f) => s + (f.boats ?? 0), 0);

/**
 * The scene, first match wins (form D1): his heavy ship lost; his boats' kill; his convoy month (the most tons); his
 * largest hunt. His nation may be on either side. Null when the month holds none of these for him.
 */
export function pickScene(m: MonthFacts): ScenePick | null {
  const me = m.human;
  if (!me) return null;
  const facts = m.facts.filter((f) => !!f.sea).sort(factOrder);
  // 1. His heavy ship lost to a submarine.
  for (const f of facts) {
    if (f.kind !== 'torpedo' || f.on !== me) continue;
    const r = raiderOf(m, f, me, f.attackers);
    return withHits(m, { case: 'heavyLost', fact: f, boats: r.nat, guard: me, me: 'def', kills: huntedIn(m, f.sea, r.nat), ships: 0, heavy: [], source: r.source });
  }
  // 2. His boats' kill: an enemy's heavy ship torpedoed where his boats are.
  for (const f of facts) {
    if (f.kind !== 'torpedo' || f.on === me || !atWarIn(m, me, f.on) || !hisBoatsThere(m, f, f.attackers)) continue;
    return withHits(m, { case: 'boatsKill', fact: f, boats: me, guard: f.on, me: 'att', kills: huntedIn(m, f.sea, me), ships: 0, heavy: [], source: f.attackers ? 'record' : f.boatsAt ? 'fleets' : 'rows' });
  }
  // 3. His convoy month: a route he owns, or raids, with tons above 0 (the most tons).
  const convoys = facts.filter((f) => f.kind === 'convoy' && (f.tons ?? 0) > 0).sort((a, b) => (b.tons ?? 0) - (a.tons ?? 0) || factOrder(a, b));
  for (const f of convoys) {
    let r = raiderOf(m, f, f.on, f.raiders), mine: 'att' | 'def' | null = null;
    if (f.on === me) mine = 'def';
    else if (atWarIn(m, me, f.on) && hisBoatsThere(m, f, f.raiders)) { mine = 'att'; r = { nat: me, source: f.raiders ? 'record' : f.boatsAt ? 'fleets' : 'rows' }; }
    if (!mine) continue;
    const ships = Math.max(1, Math.round((f.tons ?? 0) / L.caption.grt));
    return { case: 'convoy', fact: f, boats: r.nat, guard: f.on, me: mine, kills: f.boats ?? 0, ships, heavy: [], source: r.source };
  }
  // 4. His largest hunt: his hunters' kill, or his boats sunk.
  const hunts = facts.filter((f) => f.kind === 'hunt' && (f.boats ?? 0) > 0 && (f.by === me || f.on === me)).sort((a, b) => (b.boats ?? 0) - (a.boats ?? 0) || factOrder(a, b));
  if (hunts.length) {
    const f = hunts[0];
    return { case: 'hunt', fact: f, boats: f.on, guard: f.by, me: f.by === me ? 'def' : 'att', kills: f.boats ?? 0, ships: 0, heavy: [], source: f.hunters ? 'record' : 'rows' };
  }
  return null;
}

/** A torpedoing's hits: the heavy ships of the same side torpedoed in the same sea this month, up to the most drawn. */
function withHits(m: MonthFacts, p: ScenePick): ScenePick {
  const same = m.facts.filter((f) => f.kind === 'torpedo' && f.sea === p.fact.sea && f.on === p.fact.on && f !== p.fact).sort(factOrder);
  p.heavy = [p.fact, ...same].map((f) => f.cls ?? 'ca').slice(0, L.most.heavy);
  p.ships = p.heavy.length;
  return p;
}

// ---- Ships' ways (the show's own limits, so that it follows them as they are written) -------------------------

type Side = 'att' | 'def';
type Cls = 'ss' | 'mv' | 'dd' | HeavyClass;
interface Pose { x: number; y: number; h: number; v: number; r: number }
interface Pt { x: number; y: number }
/** An order to a ship from a moment: steer for a point (and hold the heading once over it) or a heading, at a speed. */
interface Order { at: number; to?: Pt; h?: number; v: number }
/** A class's handling: its full-helm turn (radians a second at its own speed), that speed, and how fast it gathers way. */
interface Handling { turn: number; speed: number; acc: number }

const DT = 0.25;
const TAU = Math.PI * 2;
const wrapPi = (a: number): number => { let x = a % TAU; if (x > Math.PI) x -= TAU; if (x < -Math.PI) x += TAU; return x; };
const wrap2Pi = (a: number): number => ((a % TAU) + TAU) % TAU;
const bearing = (ax: number, ay: number, bx: number, by: number): number => wrap2Pi(Math.atan2(bx - ax, by - ay));
const dist = (a: Pt, b: Pt): number => Math.hypot(a.x - b.x, a.y - b.y);

/**
 * A ship's way through the scene, sampled every DT. She turns no faster than the sea scene lets her class turn at her
 * speed (SeaShow.cpp: turn x 1.25 x her speed over the class's, between 0.15 and 1.45; here without the 1.25), puts
 * her helm over no faster than a third of that a second, and gathers or loses way at `acc`.
 */
class Track {
  readonly poses: Pose[] = [];
  constructor(start: Pose, orders: Order[], end: number, k: Handling) {
    if (end < 0) return;
    const os = [...orders].sort((a, b) => a.at - b.at);
    const p = { ...start };
    let o: Order | null = null, oi = -1, over = false;
    const rudder = k.turn / 3;
    for (let i = 0; i * DT <= end + 1e-9; i++) {
      const t = i * DT;
      this.poses.push({ ...p });
      while (oi + 1 < os.length && os[oi + 1].at <= t + 1e-9) { o = os[++oi]; over = false; }
      let want = p.h;
      const v = o ? o.v : p.v;
      if (o?.to) {
        // (over the point, or past it close aboard: she holds her heading on, as a ship running over a datum does)
        const d = dist(p, o.to), b = bearing(p.x, p.y, o.to.x, o.to.y);
        if (!over && (d < 30 || (d < 250 && Math.abs(wrapPi(b - p.h)) > 1.6))) over = true;
        if (!over) want = b;
      } else if (o && o.h !== undefined) want = o.h;
      const cap = k.turn * clamp(p.v / Math.max(0.1, k.speed), 0.15, 1.45);
      const rWant = clamp(wrapPi(want - p.h) * 0.3, -cap, cap);
      p.r = clamp(p.r + clamp(rWant - p.r, -rudder * DT, rudder * DT), -cap, cap);
      p.h = wrap2Pi(p.h + p.r * DT);
      p.v += clamp(v - p.v, -k.acc * DT, k.acc * DT);
      p.x += Math.sin(p.h) * p.v * DT;
      p.y += Math.cos(p.h) * p.v * DT;
    }
  }
  /** A track from poses already sailed. */
  static of(poses: Pose[]): Track {
    const t = new Track({ x: 0, y: 0, h: 0, v: 0, r: 0 }, [], -1, { turn: 0, speed: 1, acc: 0 });
    t.poses.push(...poses);
    return t;
  }
  at(t: number): Pose {
    const i = clamp(t / DT, 0, this.poses.length - 1);
    const a = this.poses[Math.floor(i)], b = this.poses[Math.min(this.poses.length - 1, Math.floor(i) + 1)], f = i - Math.floor(i);
    return { x: a.x + (b.x - a.x) * f, y: a.y + (b.y - a.y) * f, h: wrap2Pi(a.h + wrapPi(b.h - a.h) * f), v: a.v + (b.v - a.v) * f, r: a.r };
  }
  /** When a point carried with her (`ahead` metres before her middle) passes nearest a point after a moment. */
  nearest(q: Pt, from: number, ahead = 0): number {
    let best = from, bd = Infinity;
    for (let i = Math.ceil(from / DT); i < this.poses.length; i++) {
      const p = this.poses[i], d = Math.hypot(p.x + Math.sin(p.h) * ahead - q.x, p.y + Math.cos(p.h) * ahead - q.y);
      if (d < bd) { bd = d; best = i * DT; }
    }
    return best;
  }
  /** The first moment after `from` she heads within `within` (radians) of the bearing to a point, or -1. */
  pointing(q: Pt, from: number, within: number): number {
    for (let i = Math.ceil(from / DT); i < this.poses.length; i++) {
      const p = this.poses[i];
      if (Math.abs(wrapPi(bearing(p.x, p.y, q.x, q.y) - p.h)) <= within) return i * DT;
    }
    return -1;
  }
  /** The first moment after `from` she is within `r` of a moving point, or -1. */
  first(other: (t: number) => Pt, r: number, from: number, until: number): number {
    for (let i = Math.ceil(from / DT); i < this.poses.length && i * DT <= until; i++) if (dist(this.poses[i], other(i * DT)) <= r) return i * DT;
    return -1;
  }
}

/** A track that is the old one up to a moment and then follows new orders (their times on the battle's clock). */
function splice(old: Track, from: number, orders: Order[], end: number, k: Handling): Track {
  const cut = clamp(Math.round(from / DT), 0, old.poses.length - 1);
  const pose = old.poses[cut];
  const fresh = new Track(pose, orders.map((o) => ({ ...o, at: Math.max(0, o.at - cut * DT) })), Math.max(0, end - cut * DT), k);
  return Track.of([...old.poses.slice(0, cut), ...fresh.poses]);
}

// ---- A boat's depth (the show's own pace, so that the scene draws her where the recording says) ------------------

/** Periscope depth (keel, metres) [field-side-read §3; historian M: about 13-15]. */
const SCOPE = 13;
interface KeelAt { t: number; keel: number; dive: Dive; scope: boolean }

/**
 * Takes a boat from one keel to another from a moment, at the sea scene's own pace (SeaShow.cpp Trim: her casing under
 * and her at periscope depth in about 27 s, then about 1 m/s deeper; up at about 1.2 m/s, and from periscope depth to
 * the surface in about 20 s). Returns when she is there.
 */
function moveKeel(ks: KeelAt[], t: number, from: number, to: number, up: number, dive: Dive): number {
  const down = to > from;
  const shallowFor = (a: number, b: number): number => (Math.abs(b - a) / Math.max(1, SCOPE - up)) * (down ? 27 : 20);
  const deepFor = (a: number, b: number): number => Math.abs(b - a) / (down ? 1.0 : 1.2);
  const span = (a: number, b: number): number => {
    const lo = Math.min(a, b), hi = Math.max(a, b), sh = Math.max(0, Math.min(hi, SCOPE) - lo);
    return shallowFor(0, sh) + deepFor(0, hi - lo - sh);
  };
  ks.push({ t, keel: from, dive, scope: false });
  let at = t;
  // (the part above periscope depth at its own pace, the rest at the deep pace; a bend at periscope depth between)
  if ((from - SCOPE) * (to - SCOPE) < 0) { at += span(from, SCOPE); ks.push({ t: at, keel: SCOPE, dive, scope: false }); at += span(SCOPE, to); }
  else at += span(from, to);
  ks.push({ t: at, keel: to, dive, scope: false });
  return at;
}

/** Drops a boat's keel moments from a time on (her way down or up is retold from there). */
function cutKeels(ks: KeelAt[], t: number): void {
  const keep = ks.filter((k) => k.t < t);
  ks.length = 0;
  ks.push(...keep);
}

function keelOf(ks: KeelAt[], t: number): KeelAt {
  let i = 0;
  while (i + 1 < ks.length && ks[i + 1].t <= t + 1e-9) i++;
  const a = ks[i], b = ks[i + 1];
  const keel = b ? a.keel + (b.keel - a.keel) * clamp((t - a.t) / Math.max(1e-6, b.t - a.t), 0, 1) : a.keel;
  return { t, keel, dive: a.dive, scope: a.scope };
}

/** When a boat's keel last changed: from then on she is steady. */
const steadyFrom = (ks: KeelAt[]): number => ks.reduce((s, k, i) => (i > 0 && Math.abs(k.keel - ks[i - 1].keel) > 1e-6 ? k.t : s), 0);

// ---- The staged recording ------------------------------------------------------------------------------------

interface Piece {
  id: number; side: Side; cls: Cls; name: string; nat: string; fleet: string; fleetId: number;
  len: number; beam: number; loop: string; column?: number; k: Handling;
  track: Track;
  /** When she is sunk (a 'gone' piece from then), -1 never. */
  sunk: number;
  /** A boat's depth by moments. */
  keels?: KeelAt[];
  /** A boat's torpedoes, and when each fan left the tubes. */
  tubes?: number; fired?: { t: number; n: number }[];
  /** An escort's hunt: its phases on a boat. */
  hunt?: { from: number; to: number; phase: 'run' | 'blind' | 'open'; on: number; kind: 'asdic' | 'radar'; err: number }[];
  /** An escort busy (on a run, or standing by) until then. */
  busy?: number;
}

interface SceneEvent { t: number; what: string; unit?: number; side?: Side; x?: number; y?: number; cue?: string; text: string; extra?: Record<string, unknown> }
interface Zone { id: number; kind: string; x: number; y: number; r: number; until: number; side: Side; at: number; unit: number }
export interface Beat { what: string; from: number; to: number; screen: number }

const r1 = (x: number): number => Math.round(x * 10) / 10;
const r3 = (x: number): number => Math.round(x * 1000) / 1000;
/** A bearing in degrees to a tenth, 0 up to but not 360. */
const bearingDeg = (x: number): number => { const v = r1(wrapDeg(x)); return v >= 360 ? 0 : v; };
/** The views' step (s), as the fleet recordings'. */
const STEP = 2;
/** The board: 20 km square, as the fleet recordings'. */
const SIZE = 20000;
/**
 * A missed pattern's bursts are kept at least this far (m, across the water) from every boat, as each charge bursts:
 * "visibly clear unless a kill is booked" (form D1; the 21 November gunfire rule).
 */
export const CLEAR = 100;
/** A charge's lethal radius: 21 ft [OEG T3-T4]. */
const LETHAL = 6.4;
/** Hedgehog's bombs sink about 7 m/s (the sea scene's own figure for an ahead-thrown salvo, SeaShow.cpp). */
const AHEAD_SINK = 7;
/**
 * A booked kill's charge is steered to burst within this (m, across the water) of where the boat is as it bursts: the
 * lethal radius, so that the scene's blow (SeaShow.cpp sea:depthChargeHit, which moves the charge bursting nearest in
 * time onto her) moves it no more than that.
 */
export const KILL_NEAR = LETHAL;
/** A boat no escort can be over within this (s) of the screen turning on her gets no missed run: she slips away. */
const LET_GO = 240;
/** An ahead-thrower's salvo falls about this far (m) ahead of her middle (Hedgehog: some 200-250 yards ahead [L]). */
export const AHEAD_RANGE = 200;
const NUMS = ['no', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine', 'ten', 'eleven', 'twelve', 'thirteen', 'fourteen'];
const words = (n: number): string => NUMS[n] ?? String(n);
const MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
const CLASS_ONE: Record<string, string> = { bb: 'battleship', cv: 'carrier', ca: 'cruiser', dd: 'destroyer', ss: 'submarine', mv: 'merchant ship' };
const upFirst = (s: string): string => (s ? s[0].toUpperCase() + s.slice(1) : s);
const an = (w: string): string => (/^[aeiou]/i.test(w) ? `an ${w}` : `a ${w}`);
/** An escort's short word from its class ("Flower class corvette" -> "corvette"). */
const shipWord = (cls: string): string => { const w = cls.replace(/\(.*?\)/g, '').trim().split(/\s+/); return w[w.length - 1].toLowerCase() || 'escort'; };

export interface Staged {
  what: string; recording: true; key: string; seed: number; step: number;
  field: Record<string, unknown>; views: Record<string, unknown>[]; after: { lines: string[]; level: string; winner: string; seconds: number };
}

/** What the stager drew and from which facts: `field.staged` in the recording. */
export interface StagedNote {
  scene: 'sub'; case: SceneCase; beats: Beat[]; seconds: number;
  caption: { lines: string[]; check: string };
  facts: Record<string, unknown>;
  drawn: { boats: number; kills: number; byAir: number; hits: number; merchants: number; heavy: number; escorts: number; ends: string[] };
  looks: Record<string, unknown>;
  night: { hour: number; hours: number; dusk: number; dawn: number };
  standIns: string[];
}

/**
 * Stages the month's submarine scene: the outcome fixed first from the facts, then laid out with the look's hashes.
 * Null when the month holds no scene for the player (form D1: none in computer-only play).
 */
export function stageScene(m: MonthFacts): Staged | null {
  const pick = pickScene(m);
  return pick ? stagePick(m, pick) : null;
}

/**
 * How a scene is planned: how far every track runs (s), and whether a boat whose kill is booked to an escort gets her
 * kill as her first run (no missed run first).
 */
interface Plan { end: number; killFirst: boolean }
/**
 * The plans tried in turn, until the scene's last moment (every event, beat and burst, and the views' end) lies inside
 * its tracks with TRACK_SPARE to spare: a track stands still at its end, so nothing the scene shows may come after it.
 */
const PLANS: Plan[] = [{ end: 900, killFirst: false }, { end: 900, killFirst: true }, { end: 1800, killFirst: true }];
/** Seconds of track kept past the views' end. */
export const TRACK_SPARE = 20;

/** Stages a scene already picked (stageScene's work; the tests call it to stage each case). */
export function stagePick(m: MonthFacts, pick: ScenePick): Staged {
  let out = stageWith(m, pick, PLANS[0]);
  for (const plan of PLANS.slice(1)) {
    if (out.last + TRACK_SPARE <= out.end) break;
    out = stageWith(m, pick, plan);
  }
  return out.staged;
}

/** Stages a picked scene by a plan; `last` is the views' end and `end` the tracks'. */
function stageWith(m: MonthFacts, pick: ScenePick, plan: Plan): { staged: Staged; last: number; end: number } {
  const f = pick.fact;
  const sea = seaPlace(f.sea);
  const at = monthsAt(f.turn);
  const { year, month: month0 } = turnToDate(f.turn);
  // (the day: the week already booked on the headline, Headline.week; never newsWeek, which writes the save)
  const week = clamp(Math.round(f.week || 2), 1, 4);
  const day = Math.min(28, (week - 1) * 7 + 4);
  const key = `sub-${f.turn}-${pick.case}-${f.sea}`;
  const look = looker(`${m.seed}:${m.turn}:${key}:${f.on}:${f.by}:${f.route ?? f.fleet ?? ''}`);
  const nameOf = (n: string): { name: string; adj: string } => m.names?.[n] ?? { name: n, adj: n };

  const boatRow = rowOf(L.boats, pick.boats, at, sea.basin);
  const fishRow = rowOf(L.torpedoes, pick.boats, at, sea.basin);
  const escRow = rowOf(L.escorts, pick.guard, at, sea.basin);
  const airRow = rowOf(L.air, pick.guard, at, sea.basin);
  const convRow = rowOf(L.convoys, pick.guard, at, sea.basin);
  const standIns: string[] = [];
  if (pick.source !== 'record') standIns.push(pick.source === 'fleets' ? "the boats' navy: the boats at sea there as the month closed" : "the boats' navy: the basin's rows");
  if (!f.hunters) standIns.push("the kill's share by aircraft: the air row");
  if (pick.case === 'convoy' && !f.escorts) standIns.push('the escorts: the escort row');

  // ---- The outcome, fixed first ------------------------------------------------------------------------------
  const fan = pick.case !== 'hunt';
  const fleetEsc = pick.case === 'heavyLost' || pick.case === 'boatsKill';
  const killsDrawn = Math.min(L.most.boats, pick.kills);
  // Kills by aircraft where the kill's share is theirs (wholeShares, navy.ts:1650): the recorded hunters' split, or the row's.
  const airShare = f.hunters ? f.hunters.aircraft / Math.max(1e-9, f.hunters.ships + f.hunters.aircraft) : airRow.share;
  const [byShips, byAir] = wholeShares(killsDrawn, [1 - airShare, airShare]);
  const hits = pick.case === 'convoy' ? Math.min(L.most.hits, pick.ships) : fleetEsc ? Math.min(L.most.heavy, pick.heavy.length) : 0;
  const nBoats = pick.case === 'hunt' ? Math.max(1, killsDrawn)
    : clamp(Math.max(killsDrawn, pick.case === 'convoy' ? Math.max(boatRow.pack, Math.ceil(hits / 2)) : 1), 1, L.most.boats);
  const fate: ('escort' | 'air' | null)[] = Array.from({ length: nBoats }, (_, i) => (i < byShips ? 'escort' : i < byShips + byAir ? 'air' : null));
  // The escorts: the convoy's screen, the fleet's too few destroyers, or the hunting group; and at least one for each
  // kill booked to ships (the hunters that made the sea's kills), so that no booked run waits on another's.
  const nEsc = Math.min(L.most.escorts, Math.max(byShips,
    pick.case === 'convoy' ? clamp(pickIn(escRow.n, look('escorts')), 1, L.most.escorts)
      : pick.case === 'hunt' ? clamp(pickIn(escRow.group, look('group')), 1, L.most.escorts)
        : clamp(f.ships?.dd ?? 1, 1, 3)));
  // How each booked kill ends, crushed or scuttled, by the look; at most one scuttling for each escort, as one escort
  // stands by each scuttled boat's men (the sea scene's Rescue).
  let scuttles = 0;
  const endHow: ('crushed' | 'scuttled' | null)[] = fate.map((x, i) => {
    if (x === null) return null;
    if (look(`how:${i}`) < (x === 'air' ? L.ends.air : L.ends.escort) || scuttles >= nEsc) return 'crushed';
    scuttles++;
    return 'scuttled';
  });

  // ---- The board: laid out in the convoy's (or the fleet's, or the hunters') own frame, turned onto the board ----
  const course = wrap2Pi(TAU * look('course'));
  const cx = SIZE / 2, cy = SIZE / 2 - 1500;
  /** A point of the convoy's frame (u across to starboard, v ahead) on the board. */
  const toBoard = (u: number, v: number): Pt => ({ x: cx + u * Math.cos(course) + v * Math.sin(course), y: cy - u * Math.sin(course) + v * Math.cos(course) });
  const side = look('side') < 0.5 ? -1 : 1; // the boats come in from port (-1) or starboard (+1)
  const events: SceneEvent[] = [];
  const zones: Zone[] = [];
  const zone = (kind: string, p: Pt, r: number, from: number, until: number, zside: Side, unit: number): void => {
    zones.push({ id: zones.length + 1, kind, x: r1(p.x), y: r1(p.y), r: Math.round(r), until: r1(until), side: zside, at: r1(from), unit });
  };
  const attNat = pick.boats, defNat = pick.guard;
  const attAdj = nameOf(attNat).adj, defAdj = nameOf(defNat).adj;
  const boatFleet = `${attAdj} submarines`;
  const fleetText = f.fleet ?? `the ${defAdj} fleet`;
  const defFleet = pick.case === 'convoy' ? upFirst(f.routeName ?? `the ${defAdj} convoy`) : pick.case === 'hunt' ? `${defAdj} hunting group` : fleetText.replace(/^the /, '');
  /** How far every track runs: the views stop at least TRACK_SPARE sooner (stagePick re-plans a scene that would not). */
  const END = plan.end;

  // The guarded: a convoy in columns, or the torpedoed fleet's heavy ships in line ahead (none in a hunt).
  const mvK: Handling = { turn: L.convoy.mv.turn, speed: convRow.speed, acc: 0.03 };
  const guarded: Piece[] = [];
  let halfW = 0, halfL = 0;
  const guardV = pick.case === 'convoy' ? convRow.speed : pick.case === 'hunt' ? 5.0 : Math.min(...pick.heavy.map((c) => L.heavy[c].speed));
  if (pick.case === 'convoy') {
    const n = Math.min(L.most.merchants, Math.max(hits, pickIn(convRow.n, look('merchants'))));
    const cols = clamp(pickIn(convRow.cols, look('columns')), 1, n);
    const rows = Math.ceil(n / cols);
    halfW = ((cols - 1) * L.convoy.colGap) / 2; halfL = ((rows - 1) * L.convoy.rowGap) / 2;
    for (let i = 0; i < n; i++) {
      const col = i % cols, row = Math.floor(i / cols);
      const p0 = toBoard(col * L.convoy.colGap - halfW + (look(`jig:u:${i}`) - 0.5) * 60, halfL - row * L.convoy.rowGap + (look(`jig:v:${i}`) - 0.5) * 80);
      guarded.push({ id: 0, side: 'def', cls: 'mv', name: `Merchant ${i + 1}`, nat: defNat, fleet: defFleet, fleetId: 2, len: L.convoy.mv.len, beam: L.convoy.mv.beam, loop: 'ship_steam_engine_loop', column: col + 1, k: mvK, sunk: -1,
        track: new Track({ x: p0.x, y: p0.y, h: course, v: guardV, r: 0 }, [{ at: 0, h: course, v: guardV }], END, mvK) });
    }
  } else if (fleetEsc) {
    const known = f.ships ?? {};
    const heavy: HeavyClass[] = pick.heavy.slice(0, hits);
    for (const c of ['bb', 'cv', 'ca'] as HeavyClass[]) for (let k = 0; k < (known[c] ?? 0) && heavy.length < L.most.heavy; k++) heavy.push(c);
    // (in line ahead, 700 m apart; the ships torpedoed lead)
    halfL = ((heavy.length - 1) * 700) / 2;
    heavy.forEach((c, i) => {
      const p0 = toBoard(0, halfL - i * 700), hk: Handling = { turn: L.heavy[c].turn, speed: L.heavy[c].speed, acc: 0.04 };
      guarded.push({ id: 0, side: 'def', cls: c, name: `${upFirst(CLASS_ONE[c])} ${i + 1}`, nat: defNat, fleet: defFleet, fleetId: 2, len: L.heavy[c].len, beam: L.heavy[c].beam, loop: 'ship_engine_loop', k: hk, sunk: -1,
        track: new Track({ x: p0.x, y: p0.y, h: course, v: guardV, r: 0 }, [{ at: 0, h: course, v: guardV }], END, hk) });
    });
  }

  // The targets: merchants of the columns on the boats' side, the head of the outer column first; or the heavy ships hit.
  const targets: number[] = [];
  if (pick.case === 'convoy') {
    const cols = Math.max(...guarded.map((p) => p.column ?? 1));
    const outer = side > 0 ? cols : 1, next = side > 0 ? cols - 1 : 2;
    const order = [...guarded.keys()].filter((i) => guarded[i].column === outer)
      .concat([...guarded.keys()].filter((i) => guarded[i].column === next && next !== outer))
      .concat([...guarded.keys()].filter((i) => guarded[i].column !== outer && guarded[i].column !== next));
    targets.push(...order.slice(0, hits));
  } else if (fleetEsc) for (let h = 0; h < hits; h++) targets.push(h);

  // The fans: the hits shared among the boats in turn; a boat with two targets fires her second fan four seconds after her first.
  const tFire0 = 22;
  interface Fan { boat: number; target: number; t: number; range: number; running: number; aim: Pt; fish: number; spread: number }
  const perBoat = Math.max(1, Math.ceil(targets.length / nBoats));
  const fans: Fan[] = targets.map((ti, h) => {
    const b = Math.min(nBoats - 1, Math.floor(h / perBoat));
    const range = boatRow.range * (0.85 + 0.3 * look(`range:${h}`));
    // (the fan's width at the target about a ship's length and a half, in degrees across, as the reader takes it)
    const across = 160 + 80 * look(`spread:${h}`);
    return { boat: b, target: ti, t: tFire0 + 3 * b + 4 * (h - b * perBoat) + look(`fire:${h}`) * 1.5, range, running: range / fishRow.speed, aim: { x: 0, y: 0 }, fish: Math.min(boatRow.fan, boatRow.tubes), spread: r1((2 * Math.atan(across / 2 / range)) / RAD) };
  });
  for (const fn of fans) { const q = guarded[fn.target].track.at(fn.t + fn.running); fn.aim = { x: q.x, y: q.y }; }

  // The boats: in from the beam, a little ahead of the target, firing at the range; then away, and deep.
  const surfacedAttack = boatRow.attack === 'surfaced' && fan;
  const ssK: Handling = { turn: 0.07, speed: boatRow.surf, acc: 0.05 };
  const up = 0.07 * boatRow.len; // her keel surfaced, as the sea scene reckons it (SeaShow.cpp SurfacedKeel)
  const approachV = surfacedAttack ? Math.min(boatRow.surf * 0.6, 5.5) : Math.min(boatRow.dived * 0.5, 2.0);
  const deepV = Math.min(boatRow.dived * 0.5, 2.0);
  const boats: Piece[] = [];
  const dives: { t: number; keel: number; why: string; from: Dive }[] = [];
  let nextId = 1;
  for (let b = 0; b < nBoats; b++) {
    const mine = fans.filter((x) => x.boat === b);
    let firePt: Pt, hdg: number, t0: number;
    if (mine.length) {
      const fn = mine[0];
      const beta = (12 + 18 * look(`beta:${b}`)) * RAD;
      const off = { x: side * Math.cos(course) * Math.cos(beta) + Math.sin(course) * Math.sin(beta), y: -side * Math.sin(course) * Math.cos(beta) + Math.cos(course) * Math.sin(beta) };
      firePt = { x: fn.aim.x + off.x * fn.range, y: fn.aim.y + off.y * fn.range };
      hdg = bearing(firePt.x, firePt.y, fn.aim.x, fn.aim.y);
      t0 = fn.t;
    } else if (pick.case === 'hunt') {
      // (ahead of the sweeping line, coming on toward it at periscope depth)
      firePt = toBoard((b - (nBoats - 1) / 2) * 1500 + (look(`hu:${b}`) - 0.5) * 400, 900 + 300 * look(`hv:${b}`));
      hdg = wrap2Pi(course + Math.PI + (look(`hh:${b}`) - 0.5) * 0.8);
      t0 = 0;
    } else {
      // (a boat with no hit of her own: met by the screen as she closes)
      firePt = toBoard(side * (halfW + 2400 + 500 * b), halfL + 600 - 900 * b);
      hdg = wrap2Pi(course - side * 2.0);
      t0 = 0;
    }
    const v0 = mine.length ? approachV : deepV;
    const start: Pose = { x: firePt.x - Math.sin(hdg) * v0 * t0, y: firePt.y - Math.cos(hdg) * v0 * t0, h: hdg, v: v0, r: 0 };
    const lastFire = mine.length ? mine[mine.length - 1].t : 0;
    const firstAll = fans.length ? Math.min(...fans.map((x) => x.t + x.running)) : 0;
    // When she goes deep: a surfaced attacker as soon as she has fired where the screen has radar and is turning on her,
    // else as the hits come; one at periscope depth at once.
    const radarOn = fleetEsc || escRow.radar;
    const tDeep = !fan ? 6 + 3 * b : !mine.length ? tFire0 + 4 + 3 * b : surfacedAttack ? (radarOn ? lastFire + 3 + 1.5 * b : Math.max(lastFire + 3, firstAll + 2 + 1.5 * b)) : lastFire + 4 + 1.5 * b;
    const away = mine.length ? wrap2Pi(hdg + side * 2.4) : wrap2Pi(hdg + (look(`turn:${b}`) - 0.5) * 1.2);
    const orders: Order[] = [{ at: 0, h: hdg, v: v0 }];
    if (mine.length) orders.push({ at: lastFire + 1, h: away, v: surfacedAttack ? Math.min(boatRow.surf * 0.85, 7.5) : v0 });
    orders.push({ at: tDeep, h: away, v: deepV });
    const startDive: Dive = surfacedAttack ? 'surfaced' : 'periscope';
    const k0 = startDive === 'surfaced' ? up : SCOPE;
    const deep = Math.round(Math.min(boatRow.deep * (0.85 + 0.3 * look(`deep:${b}`)), boatRow.crush * 0.8));
    const keels: KeelAt[] = [{ t: 0, keel: k0, dive: startDive, scope: startDive === 'periscope' }];
    moveKeel(keels, tDeep, k0, deep, up, 'deep');
    dives.push({ t: tDeep, keel: deep, why: !fan ? 'evade' : surfacedAttack && escRow.radar && !fleetEsc ? 'radar' : 'evade', from: startDive });
    boats.push({ id: nextId++, side: 'att', cls: 'ss', name: `Submarine ${b + 1}`, nat: attNat, fleet: boatFleet, fleetId: 1, len: boatRow.len, beam: boatRow.beam, loop: 'ship_engine_loop', k: ssK, sunk: -1,
      track: new Track(start, orders, END, ssK), keels, tubes: boatRow.tubes, fired: mine.map((x) => ({ t: x.t, n: x.fish })) });
  }
  // A boat's second fan: from where she then is, its running time from that range.
  for (const fn of fans) {
    if (fans.find((x) => x.boat === fn.boat) === fn) continue;
    for (let pass = 0; pass < 3; pass++) {
      const bp = boats[fn.boat].track.at(fn.t);
      fn.range = dist(bp, fn.aim);
      fn.running = fn.range / fishRow.speed;
      const q = guarded[fn.target].track.at(fn.t + fn.running);
      fn.aim = { x: q.x, y: q.y };
    }
  }
  for (const p of guarded) p.id = nextId++;

  // The escorts (nEsc, above): round the convoy or the fleet, or the hunting group in line abreast.
  const esc = fleetEsc ? { cls: 'destroyer', len: L.heavy.dd.len, beam: L.heavy.dd.beam, speed: L.heavy.dd.speed, turn: L.heavy.dd.turn } : { cls: escRow.cls, len: escRow.len, beam: escRow.beam, speed: escRow.speed, turn: escRow.turn };
  const escK: Handling = { turn: esc.turn, speed: esc.speed, acc: 0.08 };
  const escWord = fleetEsc ? 'destroyer' : shipWord(escRow.cls);
  // When the screen turns on each boat: with radar, as a surfaced boat comes in, before she fires; else when the hits
  // tell (or, in a hunt, as the line finds her).
  const radarFirst = surfacedAttack && (fleetEsc || escRow.radar);
  const firstHitAt = fans.length ? Math.min(...fans.map((x) => x.t + x.running)) : -1;
  const reactAt = boats.map((_, i) => {
    const mine = fans.filter((x) => x.boat === i);
    return !fan ? 10 + 6 * i : radarFirst && mine.length ? Math.max(4, mine[0].t - 8) : firstHitAt + 4 + 4 * i;
  });
  // (their stations in the guarded's own frame: evenly round the convoy or the fleet, or abreast in a hunt; then for each
  // boat in turn, the nearest station not yet taken moves to some 400-600 m from her as the screen turns on her, so that
  // the screen meets her at once: the stations are a look, and the scene's minutes are few)
  const stations: { u: number; v: number }[] = [];
  const stationFor: number[] = [];
  for (let k = 0; k < nEsc; k++) {
    const a = (TAU * (k + 0.5)) / nEsc;
    stations.push(pick.case === 'hunt' ? { u: (k - (nEsc - 1) / 2) * 1800, v: 0 } : { u: Math.sin(a) * (halfW + 1500), v: Math.cos(a) * (halfL + 1600) });
  }
  if (pick.case !== 'hunt') {
    const taken = new Set<number>();
    boats.forEach((b, i) => {
      if (taken.size >= nEsc) return;
      const tRef = reactAt[i];
      // (her place in the guarded's frame, which steams at guardV on the course)
      const bp = b.track.at(tRef), o = toBoard(0, 0);
      const dx = bp.x - o.x - Math.sin(course) * guardV * tRef, dy = bp.y - o.y - Math.cos(course) * guardV * tRef;
      const bu = dx * Math.cos(course) - dy * Math.sin(course), bv = dx * Math.sin(course) + dy * Math.cos(course);
      // (astern of her in that frame and a little inside her: steaming on with the guarded, the escort closes her)
      const want = { u: bu - side * (100 + 150 * look(`st:u:${i}`)), v: bv - 350 - 200 * look(`st:v:${i}`) };
      let best = -1, bd = Infinity;
      stations.forEach((st, k) => { if (!taken.has(k)) { const d = Math.hypot(st.u - want.u, st.v - want.v); if (d < bd) { bd = d; best = k; } } });
      taken.add(best);
      stations[best] = want;
      stationFor[i] = best;
    });
  }
  const escorts: Piece[] = [];
  for (let k = 0; k < nEsc; k++) {
    const p0 = toBoard(stations[k].u, stations[k].v);
    escorts.push({ id: nextId++, side: 'def', cls: 'dd', name: `${upFirst(escWord)} ${k + 1}`, nat: defNat, fleet: defFleet, fleetId: 2, len: esc.len, beam: esc.beam, loop: 'destroyer_wash_loop', k: escK, sunk: -1,
      track: new Track({ x: p0.x, y: p0.y, h: course, v: guardV, r: 0 }, [{ at: 0, h: course, v: guardV }], END, escK), hunt: [], busy: 0 });
  }
  const pieces: Piece[] = [...boats, ...guarded, ...escorts];
  const byId = new Map(pieces.map((p) => [p.id, p]));

  // ---- The events: the report, the fans and their hits -------------------------------------------------------
  if (pick.case === 'convoy' && boatRow.report) {
    // (the shadower's report, at the convoy's nearest ship: the reader takes `at` as the piece reported)
    const b = boats[0], p = b.track.at(4);
    let near = guarded[0];
    for (const g of guarded) if (dist(g.track.at(4), p) < dist(near.track.at(4), p)) near = g;
    events.push({ t: 4, what: 'sea:report', unit: b.id, side: 'att', x: p.x, y: p.y, text: `${upFirst(an(attAdj))} submarine reports the convoy.`, extra: { by: b.id, at: near.id } });
  }
  const hitAt = new Map<number, number>();
  for (const fn of fans) {
    const b = boats[fn.boat], tg = guarded[fn.target], p = b.track.at(fn.t);
    const what = tg.cls === 'mv' ? 'a merchant ship of the convoy' : `${an(CLASS_ONE[tg.cls])} of ${fleetText}`;
    events.push({ t: fn.t, what: 'sea:torpedo', unit: b.id, side: 'att', x: p.x, y: p.y, cue: 'torpedo_launch', text: `${upFirst(an(attAdj))} submarine fires ${words(fn.fish)} torpedoes at ${what}.`,
      extra: { target: tg.id, x: r1(fn.aim.x), y: r1(fn.aim.y), running: r1(fn.running), share: 1, fish: fn.fish, spread: fn.spread, wakeless: fishRow.wakeless, dive: keelOf(b.keels!, fn.t).dive } });
    zone('torpedo', fn.aim, Math.round(fn.range * Math.tan((fn.spread / 2) * RAD)) + 60, fn.t, fn.t + fn.running, 'att', b.id);
    const th = fn.t + fn.running;
    if (hitAt.has(fn.target)) continue;
    hitAt.set(fn.target, th);
    const q = tg.track.at(th);
    events.push({ t: th, what: 'sea:torpedoHit', unit: tg.id, side: 'att', x: q.x, y: q.y, cue: 'torpedo_hit', text: `A torpedo hits ${what}.`, extra: { from: b.id } });
    // She is lost: her way comes off her, and the scene takes her down (its own WayDown).
    tg.track = splice(tg.track, th, [{ at: th, h: wrap2Pi(course + side * 0.25), v: 0 }], END, { ...tg.k, acc: 0.06 });
    tg.sunk = th + (tg.cls === 'mv' ? 1.5 : 6);
    const qs = tg.track.at(tg.sunk);
    events.push({ t: tg.sunk, what: 'sea:sunk', unit: tg.id, side: 'def', x: qs.x, y: qs.y, cue: 'ship_sinking', text: `${upFirst(what)} goes down.`, extra: { cls: tg.cls, fleet: tg.fleetId } });
    events.push({ t: tg.sunk, what: 'gone', unit: tg.id, side: 'def', x: qs.x, y: qs.y, text: '' });
    zone('sunk', qs, Math.round(tg.len / 2), tg.sunk, END, 'def', tg.id);
  }
  const firstHit = fans.length ? Math.min(...fans.map((x) => x.t + x.running)) : -1;
  const lastHit = fans.length ? Math.max(...fans.map((x) => x.t + x.running)) : 0;
  boats.forEach((b, i) => {
    const d = dives[i], p = b.track.at(d.t);
    events.push({ t: d.t, what: 'sea:dive', unit: b.id, side: 'att', x: p.x, y: p.y, text: d.from === 'surfaced' ? `${upFirst(an(attAdj))} submarine crash-dives.` : `${upFirst(an(attAdj))} submarine goes deep.`, extra: { dive: 'deep', keel: d.keel, why: d.why } });
  });

  // ---- The depth-charge runs ---------------------------------------------------------------------------------
  // Each boat is hunted by the escort that can be over her soonest. A missed pattern goes in where she is not, its
  // bursts kept CLEAR of her and of every other boat as each charge bursts. Where her kill is booked, the run is made
  // on her: the booked charge (of a pattern, the one off the rails nearest its middle; of an ahead-thrower, its salvo)
  // bursts within LETHAL of her keel where she is as it bursts, and the scene moves it the last metres onto her. A
  // doomed boat is first missed, then killed, unless the escorts are few (some would make three runs or more): then
  // her kill is her first run, and the booked kills are planned before the misses.
  const sinkRate = fleetEsc ? 3.0 : escRow.sink;
  const spreadOf = fleetEsc ? 100 : escRow.spread;
  const patternN = fleetEsc ? 6 : escRow.pattern;
  const setRange: [number, number] = fleetEsc ? [30, 60] : escRow.set;
  const lostAt = fleetEsc ? 160 : escRow.lost;
  const contactRange: [number, number] = fleetEsc ? [1300, 1800] : escRow.contact;
  /** A run: `burst` is when the booked charge bursts (a miss: the first), `last` when the last one does. */
  interface Run { escort: Piece; boat: Piece; kind: 'pattern' | 'ahead'; from: number; drop: number; burst: number; last: number; centre: Pt; set: number; n: number; spread: number; kill: boolean; sinking: number; nearest: number }
  const runs: Run[] = [];
  /**
   * Where each charge of a pattern bursts and when, laid as the sea scene lays them (SeaShow.cpp sea:depthCharge): in
   * from her stern over the pattern's length at her speed, every other one off the rails, the rest thrown to either side.
   */
  const chargesOf = (e: Piece, track: Track, drop: number, n: number, spread: number, sinking: number): { x: number; y: number; t: number }[] => {
    const p = track.at(drop), sp = Math.max(3, p.v), run = spread / sp;
    const ax = Math.sin(p.h), ay = Math.cos(p.h), rx = Math.cos(p.h), ry = -Math.sin(p.h);
    const stern = { x: p.x - ax * e.len * 0.5, y: p.y - ay * e.len * 0.5 };
    const out: { x: number; y: number; t: number }[] = [];
    for (let k = 0; k < n; k++) {
      const share = n > 1 ? k / (n - 1) : 0;
      const across = k % 2 === 0 ? 0 : (Math.floor(k / 2) % 2 === 0 ? 1 : -1) * (spread * 0.5 - e.beam * 0.5);
      out.push({ x: stern.x + ax * share * spread + rx * across, y: stern.y + ay * share * spread + ry * across, t: drop + share * run + sinking });
    }
    return out;
  };
  /** A kill pattern's booked charge: the one off the rails nearest its middle (the scene bursts the charge nearest in time). */
  const kb = patternN > 1 ? 2 * Math.round((patternN - 1) / 4) : 0;
  const kbShare = patternN > 1 ? kb / (patternN - 1) : 0;
  /** After a run she slows and comes round over where the boat then is, to search again; she is free from then. */
  const comeRound = (e: Piece, track: Track, b: Piece, drop: number, length: number, burst: number): void => {
    // (once the pattern is laid: the charges lie where the run put them, whatever she does next)
    const tBack = drop + length / Math.max(3, track.at(drop).v) + 3;
    e.track = splice(track, tBack, [{ at: tBack, to: b.track.at(tBack + 60), v: Math.min(esc.speed, 5) }], END, e.k);
    e.busy = Math.max(tBack, burst + 6);
  };
  /** A missed pattern on a boat, from a moment (never dropped before `notBefore`). */
  const planMiss = (e: Piece, b: Piece, from: number, label: string, notBefore: number): Run => {
    const base = e.track;
    const set = Math.round(setRange[0] + (setRange[1] - setRange[0]) * look(`set:${label}`));
    const sinking = r1(set / sinkRate);
    let offA = TAU * look(`off:${label}`);
    let far = spreadOf / 2 + CLEAR + 30 + 40 * look(`far:${label}`);
    // (the pattern's middle rides ahead of her own: her stern plus half the pattern's length)
    const lead = spreadOf / 2 - e.len / 2;
    const bp0 = b.track.at(from + 60);
    let centre: Pt = { x: bp0.x, y: bp0.y };
    let track = base, drop = from + 60, nearest = Infinity;
    for (let pass = 0; pass < 16; pass++) {
      // (she steers for where the pattern's middle must lie, less the lead, and drops as the middle passes over it)
      const p0 = base.at(from), a = bearing(p0.x, p0.y, centre.x, centre.y);
      track = splice(base, from, [{ at: from, to: { x: centre.x - Math.sin(a) * lead, y: centre.y - Math.cos(a) * lead }, v: esc.speed }], END, e.k);
      drop = track.nearest(centre, from + 5, lead);
      // (never before she is under: the escort holds on with the convoy a little longer and turns on her later)
      if (drop < notBefore - 0.5) { from += notBefore - drop; continue; }
      const bp = b.track.at(drop + sinking);
      centre = { x: bp.x + Math.sin(offA) * far, y: bp.y + Math.cos(offA) * far };
      // (each charge as it bursts against where she and every other boat then is, pushed further off her until all
      // are clear, or round her when another boat is the nearest)
      let near: Piece = b;
      nearest = Infinity;
      for (const c of chargesOf(e, track, drop, patternN, spreadOf, sinking)) for (const o of boats) { const d = dist(c, o.track.at(c.t)); if (d < nearest) { nearest = d; near = o; } }
      if (pass >= 2 && nearest >= CLEAR) break;
      if (nearest < CLEAR && near === b) far += CLEAR - nearest + 25;
      else if (nearest < CLEAR) offA += 1.1;
    }
    // (the pattern's middle as the charges lie, for the event's aim and the zone)
    const cs = chargesOf(e, track, drop, patternN, spreadOf, sinking);
    centre = { x: cs.reduce((a, c) => a + c.x, 0) / cs.length, y: cs.reduce((a, c) => a + c.y, 0) / cs.length };
    comeRound(e, track, b, drop, spreadOf, drop + sinking);
    return { escort: e, boat: b, kind: 'pattern', from, drop, burst: drop + sinking, last: Math.max(...cs.map((c) => c.t)), centre, set, n: patternN, spread: spreadOf, kill: false, sinking, nearest };
  };
  /**
   * The run that kills a boat, from a moment (never dropped before `notBefore`): the booked charge's place as the
   * pattern goes in (or the salvo's, AHEAD_RANGE before her) is steered over where the boat will be as it bursts, at
   * her keel then. Straight in first; where that cannot pass over her (she is too close, or turned away), she opens
   * out to come round onto a straight run-in, away from the mark, along her heading or to either side.
   */
  const planKill = (e: Piece, b: Piece, from0: number, ahead: boolean, notBefore: number): Run => {
    const base = e.track;
    const lead = ahead ? AHEAD_RANGE : kbShare * spreadOf - e.len / 2;
    interface Try { track: Track; from: number; drop: number; hit: number; set: number; sinking: number; miss: number; at: Pt }
    let best: Try | null = null;
    const ways: { open: number; dir: number }[] = [{ open: 0, dir: 0 }];
    for (const open of [500, 800]) for (let dir = 0; dir < 4; dir++) ways.push({ open, dir });
    for (const way of ways) {
      let from = from0, tried: Try | null = null, shifts = 0;
      let target: Pt = b.track.at(from + 60);
      let mark: Pt | null = null, out: Pt | null = null;
      // (the way straight in always gives a try: a pass that only moves her start later does not count)
      for (let pass = 0; pass < 10; pass++) {
        const pf = base.at(from);
        if (!mark) {
          // (the run-in's direction: from her to the boat; or, opening out, that, her heading, or square to it)
          const a = bearing(pf.x, pf.y, target.x, target.y);
          const dirA = way.open === 0 || way.dir === 0 ? a : way.dir === 1 ? pf.h : a + (way.dir === 2 ? 1 : -1) * Math.PI / 2;
          mark = { x: target.x - Math.sin(dirA) * lead, y: target.y - Math.cos(dirA) * lead };
          if (way.open) out = { x: mark.x - Math.sin(dirA) * way.open, y: mark.y - Math.cos(dirA) * way.open };
        }
        let track: Track, tIn: number;
        if (out) {
          const first = splice(base, from, [{ at: from, to: out, v: esc.speed }], END, e.k);
          const tOut = first.first(() => out!, 40, from, END);
          if (tOut < 0) break;
          track = splice(first, tOut, [{ at: tOut, to: mark, v: esc.speed }], END, e.k);
          tIn = tOut + 5;
        } else {
          track = splice(base, from, [{ at: from, to: mark, v: esc.speed }], END, e.k);
          tIn = from + 5;
        }
        const drop = track.nearest(target, tIn, lead);
        if (drop < notBefore - 0.5 && shifts++ < 20) { from += notBefore - drop; mark = null; out = null; pass--; continue; }
        const p = track.at(drop);
        const run = ahead ? 0 : (kbShare * spreadOf) / Math.max(3, p.v);
        // (set to her keel as the booked charge bursts; the event's clock is to a tenth of a second, as the scene's)
        let hit = drop + run + keelOf(b.keels!, drop).keel / (ahead ? AHEAD_SINK : sinkRate), set = 10, sinking = 0;
        for (let k = 0; k < 3; k++) {
          const keel = keelOf(b.keels!, hit).keel;
          set = Math.max(10, Math.round(keel));
          sinking = ahead ? r1(keel / AHEAD_SINK) : r1(set / sinkRate);
          hit = r1(r1(drop) + run + sinking);
        }
        const at = { x: p.x + Math.sin(p.h) * lead, y: p.y + Math.cos(p.h) * lead };
        const q = b.track.at(hit), miss = dist(at, q);
        if (!tried || miss < tried.miss) tried = { track, from, drop, hit, set, sinking, miss, at };
        if (miss <= KILL_NEAR) break;
        // (steer the mark by the miss, and aim at where she then is)
        mark = { x: mark.x + q.x - at.x, y: mark.y + q.y - at.y };
        target = { x: q.x, y: q.y };
      }
      if (tried && (!best || tried.miss < best.miss)) best = tried;
      if (best && best.miss <= KILL_NEAR) break;
    }
    const k = best!;
    const q = b.track.at(k.hit);
    let centre: Pt = { x: q.x, y: q.y }, last = k.hit;
    if (!ahead) {
      const cs = chargesOf(e, k.track, k.drop, patternN, spreadOf, k.sinking);
      centre = { x: cs.reduce((a, c) => a + c.x, 0) / cs.length, y: cs.reduce((a, c) => a + c.y, 0) / cs.length };
      last = Math.max(...cs.map((c) => c.t));
    }
    comeRound(e, k.track, b, k.drop, ahead ? 40 : spreadOf, k.hit);
    return { escort: e, boat: b, kind: ahead ? 'ahead' : 'pattern', from: k.from, drop: k.drop, burst: k.hit, last, centre, set: k.set, n: ahead ? 24 : patternN, spread: ahead ? 40 : spreadOf, kill: true, sinking: k.sinking, nearest: k.miss };
  };
  /**
   * The escort to send on a boat at a moment, and when it can be over her: the one that can be there soonest, free then
   * or once its run is done (from where it then is, at its speed, after its turn and its gathering way). An escort
   * kept to stand by a scuttled boat's men once its kill is made, or standing by them, is not sent; null if none is left.
   */
  const sendAt = (b: Piece, t: number): { e: Piece; t: number; at: number } | null => {
    let best: { e: Piece; t: number; at: number } | null = null;
    for (const e of escorts) {
      const ready = Math.max(t, e.busy ?? 0);
      if (ready >= END || keep.has(e)) continue;
      const p = e.track.at(ready), q = b.track.at(ready);
      const at = ready + dist(p, q) / esc.speed + Math.abs(wrapPi(bearing(p.x, p.y, q.x, q.y) - p.h)) / esc.turn + Math.max(0, esc.speed - p.v) / (2 * escK.acc);
      if (!best || at < best.at - 1e-6) best = { e, t: ready, at };
    }
    return best;
  };
  /** The escort whose station was set for a boat, if it is free then, else the one that can be over her soonest. */
  const sendOn = (b: Piece, i: number, t: number): { e: Piece; t: number; at: number } | null => {
    const mine = stationFor[i] === undefined ? undefined : escorts[stationFor[i]];
    return mine && !keep.has(mine) && (mine.busy ?? 0) <= t ? { e: mine, t, at: t } : sendAt(b, t);
  };
  /** Escorts kept, once their kill is made, to stand by that boat's men. */
  const keep = new Set<Piece>();
  const byEscort = fate.filter((x) => x === 'escort').length;
  const killFirst = plan.killFirst || nBoats + byEscort > 2 * nEsc;
  const clearOf = new Map<Piece, Run>(), killOf = new Map<Piece, Run>();
  const planKillOn = (b: Piece, i: number): void => {
    // (after her missed run, once she is steady at her depth; else as the screen turns on her, once she is under)
    const clear = clearOf.get(b);
    const t0 = clear ? Math.max(clear.burst + 8, steadyFrom(b.keels!) + 4) : reactAt[i];
    // (there is always one left: at least one escort for each kill booked to ships, nEsc)
    const s = (clear ? sendAt(b, t0) : sendOn(b, i, t0))!;
    const ahead = !fleetEsc && !!escRow.ahead && look(`ahead:${i}`) < 0.6;
    const run = planKill(s.e, b, s.t, ahead, clear ? t0 : dives[i].t + 25);
    runs.push(run);
    killOf.set(b, run);
    if (endHow[i] === 'scuttled') keep.add(s.e);
  };
  if (killFirst) boats.forEach((b, i) => { if (fate[i] === 'escort') planKillOn(b, i); });
  boats.forEach((b, i) => {
    if (killFirst && fate[i] === 'escort') return;
    // (a boat no escort can be over within LET_GO of the screen turning on her is let go: she gets no missed run)
    const s = sendOn(b, i, reactAt[i]);
    if (!s || s.at > reactAt[i] + LET_GO) return;
    const run = planMiss(s.e, b, s.t, `clear:${i}`, dives[i].t + 25);
    runs.push(run);
    clearOf.set(b, run);
  });
  if (!killFirst) boats.forEach((b, i) => { if (fate[i] === 'escort') planKillOn(b, i); });

  // The kills booked: the run that kills her, or an aircraft that finds her surfaced.
  interface End { boat: Piece; how: 'crushed' | 'scuttled'; by: 'escort' | 'air'; t: number; sunk: number; up?: number; rescue?: number; killer?: Piece }
  const ends: End[] = [];
  boats.forEach((b, i) => {
    const by = fate[i], how = endHow[i];
    if (!by || !how) return;
    const run = killOf.get(b);
    ends.push(run ? { boat: b, how, by, t: run.burst, sunk: -1, killer: run.escort } : { boat: b, how, by, t: -1, sunk: -1 });
  });
  for (const end of ends) {
    const b = end.boat, ks = b.keels!;
    const hold = (t: number, acc: number): void => { b.track = splice(b.track, t, [{ at: t, h: b.track.at(t).h, v: 0 }], END, { ...b.k, acc }); };
    if (end.by === 'air') {
      // She comes up to slip away on the surface in the dark (after the escort's run on her, if one was made), and an
      // aircraft finds her there.
      const tUp = Math.max((clearOf.get(b)?.burst ?? 0) + 15, steadyFrom(ks) + 2), k = keelOf(ks, tUp).keel;
      cutKeels(ks, tUp);
      const tTop = moveKeel(ks, tUp, k, up, up, 'surfaced');
      const p = b.track.at(tUp);
      events.push({ t: tUp, what: 'sea:dive', unit: b.id, side: 'att', x: p.x, y: p.y, text: `${upFirst(an(attAdj))} submarine surfaces to slip away in the dark.`, extra: { dive: 'surfaced', keel: r1(up), why: 'evade' } });
      const h = b.track.at(tUp).h;
      b.track = splice(b.track, tTop, [{ at: tTop, h, v: Math.min(boatRow.surf * 0.6, 5) }], END, b.k);
      const tAir = tTop + 10, q = b.track.at(tAir);
      events.push({ t: tAir, what: 'sea:air', unit: b.id, side: 'def', x: q.x, y: q.y, text: `${upFirst(an(defAdj))} ${airRow.name} attacks ${an(attAdj)} submarine on the surface.` });
      end.t = tAir;
      if (end.how === 'crushed') {
        // (she tries to go down, holed, and does not come up)
        const tDown = tAir + 2;
        const tGone = moveKeel(ks, tDown, up, Math.min(40, boatRow.crush * 0.8), up, 'deep');
        events.push({ t: tDown, what: 'sea:dive', unit: b.id, side: 'att', x: q.x, y: q.y, text: '', extra: { dive: 'deep', keel: Math.min(40, boatRow.crush * 0.8), why: 'aircraft' } });
        end.sunk = Math.min(tGone, tDown + 30);
        hold(tDown, 0.1);
      } else { end.sunk = tAir + 18; hold(tAir + 2, 0.15); }
    } else if (end.how === 'scuttled') {
      // Forced up by the blow: she comes up, stops, and her crew scuttle her (the scene's own men take to the water).
      const tUp = end.t + 2, k = keelOf(ks, end.t).keel;
      cutKeels(ks, tUp);
      const tTop = moveKeel(ks, tUp, k, up, up, 'surfaced');
      const p = b.track.at(tUp);
      events.push({ t: tUp, what: 'sea:dive', unit: b.id, side: 'att', x: p.x, y: p.y, text: `${upFirst(an(attAdj))} submarine is forced to the surface.`, extra: { dive: 'surfaced', keel: r1(up), why: 'forced' } });
      end.up = tTop;
      end.sunk = tTop + 14;
      hold(end.t, 0.05);
    } else {
      // (crushed below by the booked blow: she breaks up where she is)
      end.sunk = end.t + 3;
      hold(end.t, 0.3);
    }
    b.sunk = end.sunk;
    const q = b.track.at(end.sunk);
    events.push({ t: end.sunk, what: 'sea:sunk', unit: b.id, side: 'att', x: q.x, y: q.y, cue: 'ship_sinking', text: end.how === 'crushed' ? `${upFirst(an(attAdj))} submarine is destroyed.` : `${upFirst(an(attAdj))} submarine is scuttled by her crew.`, extra: { cls: 'ss', fleet: b.fleetId, how: end.how } });
    events.push({ t: end.sunk, what: 'gone', unit: b.id, side: 'att', x: q.x, y: q.y, text: '' });
    zone('sunk', q, 40, end.sunk, END, 'att', b.id);
    zone('oil', q, 120, end.sunk + 4, END, 'att', b.id);
    // The escort that forced her up (kept for it since its kill) comes back and stands by her men in the water (the sea
    // scene moves them toward her: Rescue); where the aircraft sank her, the one that can be there soonest (there is one:
    // at most one scuttling for each escort, endHow), if it can be there within LET_GO; else none comes, and her men
    // are the sea scene's alone.
    const s = end.how !== 'scuttled' ? null : end.killer ? { e: end.killer, t: Math.max(end.t + 4, end.killer.busy ?? 0), at: 0 } : sendAt(b, end.t);
    if (s && (end.killer || s.at <= end.t + LET_GO)) {
      // (she comes round to head for where the boat went down, steers for it at her speed and from some 600 m off at
      // 6 m/s, and takes the way off her to stop about 200 m short of it, or at once if she came round closer than
      // that; the event goes out once she is stopped and the boat is gone)
      const e = s.e, near = e.track.at(s.t), stopAcc = 0.2, slow = Math.min(esc.speed, 6), short = 200 + (slow * slow) / (2 * stopAcc);
      e.track = splice(e.track, s.t, [{ at: s.t, h: bearing(near.x, near.y, q.x, q.y), v: slow }], END, e.k);
      const pointed = e.track.pointing(q, s.t, 0.3), tOn = pointed >= 0 ? pointed : s.t + 30;
      e.track = splice(e.track, tOn, [{ at: tOn, to: q, v: esc.speed }], END, e.k);
      const nearer = e.track.first(() => q, short + 300, tOn, END), tEase = nearer >= 0 ? nearer : tOn;
      e.track = splice(e.track, tEase, [{ at: tEase, to: q, v: slow }], END, e.k);
      const close = e.track.first(() => q, short, tEase, END), tSlow = close >= 0 ? close : tEase;
      e.track = splice(e.track, tSlow, [{ at: tSlow, to: q, v: 0 }], END, { ...e.k, acc: stopAcc });
      const tStop = tSlow + Math.ceil(e.track.at(tSlow).v / stopAcc) + 1, tR = Math.max(tStop, end.sunk + 3);
      e.busy = END;
      end.rescue = tR;
      events.push({ t: tR, what: 'sea:rescue', unit: e.id, side: 'def', x: q.x, y: q.y, text: `${upFirst(an(escWord))} stands by to pick up the submarine's survivors.`, extra: { by: e.id, on: b.id } });
    }
  }
  // The runs as events: the contact, the contact lost running in, the drop, and the burst that kills.
  runs.sort((a, b) => a.drop - b.drop || a.escort.id - b.escort.id);
  for (const [k, run] of runs.entries()) {
    const e = run.escort, b = run.boat;
    const reach = pickIn(contactRange, look(`contact:${k}`));
    let tc = e.track.first((t) => b.track.at(t), reach, run.from, run.drop);
    if (tc < 0) tc = Math.max(run.from, run.drop - 20);
    const kind: 'asdic' | 'radar' = keelOf(b.keels!, tc).dive === 'surfaced' && (fleetEsc || escRow.radar) ? 'radar' : 'asdic';
    const err = ((look(`err:${k}`) - 0.5) * (run.kill ? 0.06 : 0.3)) / (fleetEsc ? 1 : escRow.sonar ?? 1);
    const ep = e.track.at(tc), bp = b.track.at(tc);
    events.push({ t: tc, what: 'sea:sonar', unit: e.id, side: 'def', x: ep.x, y: ep.y, text: kind === 'radar' ? `${upFirst(an(escWord))} has a radar contact.` : `${upFirst(an(escWord))} has an asdic contact.`, extra: { on: b.id, kind, range: Math.round(dist(ep, bp)), bearing: r3(bearing(ep.x, ep.y, bp.x, bp.y)), err: r3(err) } });
    // (running in with stern-dropped charges she loses the boat close aboard; an ahead-thrower keeps her to the end)
    let tl = -1;
    if (run.kind === 'pattern') {
      tl = e.track.first((t) => b.track.at(t), lostAt, tc + 1, run.drop);
      if (tl < 0) tl = Math.max(tc + 1, run.drop - 8);
      const lp = e.track.at(tl), lb = b.track.at(tl);
      events.push({ t: tl, what: 'sea:sonar', unit: e.id, side: 'def', x: lp.x, y: lp.y, text: '', extra: { on: b.id, kind: 'asdic', range: Math.round(dist(lp, lb)), bearing: r3(bearing(lp.x, lp.y, lb.x, lb.y)), err: r3(err), lost: 'close' } });
      zone('datum', lb, 150, tl, run.burst + 30, 'def', e.id);
    }
    e.hunt!.push({ from: tc, to: tl >= 0 ? tl : run.drop, phase: 'run', on: b.id, kind, err });
    if (tl >= 0) e.hunt!.push({ from: tl, to: run.burst, phase: 'blind', on: b.id, kind, err });
    e.hunt!.push({ from: tl >= 0 ? run.burst : run.drop, to: run.burst + 30, phase: 'open', on: b.id, kind, err });
    const dp = e.track.at(run.drop);
    const word = run.kind === 'ahead' ? `fires her ${escRow.ahead ?? 'ahead-thrower'}` : `drops a pattern of ${words(run.n)} depth charges`;
    events.push({ t: run.drop, what: 'sea:depthCharge', unit: e.id, side: 'def', x: dp.x, y: dp.y, text: `${upFirst(an(escWord))} ${word}.`, extra: { on: b.id, x: r1(run.centre.x), y: r1(run.centre.y), n: run.n, set: run.set, sinking: r1(run.sinking), spread: run.spread, kind: run.kind } });
    zone('pattern', run.centre, run.spread / 2, run.drop, run.burst + 6, 'def', e.id);
    if (run.kill) {
      const q = b.track.at(run.burst);
      events.push({ t: run.burst, what: 'sea:depthChargeHit', unit: b.id, side: 'def', x: q.x, y: q.y, text: `Depth charges hit ${an(attAdj)} submarine.`, extra: { from: e.id, harm: 1, keel: Math.round(keelOf(b.keels!, run.burst).keel) } });
    }
  }
  // Starshell: after the first hit, over the water the boats came from; and over a boat forced up, to light her.
  const shells: { t: number; over: Pt }[] = [];
  if (fleetEsc || escRow.starshell) {
    if (fan && firstHit >= 0) {
      const b = boats[0], q = b.track.at(firstHit + 3), a = TAU * look('shell');
      shells.push({ t: firstHit + 3, over: { x: q.x + Math.sin(a) * 300, y: q.y + Math.cos(a) * 300 } });
    }
    for (const end of ends) if (end.up !== undefined) { const q = end.boat.track.at(end.up); shells.push({ t: end.up + 2, over: { x: q.x + 120, y: q.y + 120 } }); }
  }
  for (const s of shells) {
    let e = escorts[0], bd = Infinity;
    for (const x of escorts) { const d = dist(x.track.at(s.t), s.over); if (d < bd) { bd = d; e = x; } }
    const ep = e.track.at(s.t);
    events.push({ t: s.t, what: 'sea:starshell', unit: e.id, side: 'def', x: ep.x, y: ep.y, text: `${upFirst(an(escWord))} fires starshell.`, extra: { by: e.id, x: r1(s.over.x), y: r1(s.over.y) } });
  }

  // ---- The beats: the windows of battle time the scene shows, and their screen seconds -------------------------
  const B = L.beats;
  const beats: Beat[] = [];
  const win = (what: string, from: number, to: number, screen: number): void => {
    from = Math.max(0, from);
    if ((to - from) / screen <= B.pace) { beats.push({ what, from: r1(from), to: r1(to), screen }); return; }
    // (too long for its seconds: its first and its last moments, the middle cut)
    const a = Math.max(2, Math.round(screen / 2));
    beats.push({ what, from: r1(from), to: r1(from + a * B.pace), screen: a }, { what, from: r1(to - (screen - a) * B.pace), to: r1(to), screen: screen - a });
  };
  win(pick.case === 'convoy' ? 'convoy' : pick.case === 'hunt' ? 'sweep' : 'fleet', 0, 16, B.convoy);
  if (fans.length) {
    win('fire', Math.min(...fans.map((x) => x.t)) - 2, Math.max(...fans.map((x) => x.t)) + 3, B.fire);
    win('hits', firstHit - 3, lastHit + 4, B.hits);
  }
  // (the missed runs; where every boat's first run was her kill, the kill runs)
  const clear = runs.filter((r) => !r.kill), shown = clear.length ? clear : runs;
  const d0 = Math.min(...shown.map((r) => r.drop)), d1 = Math.max(...shown.map((r) => r.drop));
  const b0 = Math.min(...shown.map((r) => r.drop + r.sinking)), b1 = Math.max(...shown.map((r) => r.last));
  if (b0 - 2 <= d1 + 3) win('charges', d0 - 6, b1 + 4, B.charges);
  else { win('drop', d0 - 6, d1 + 3, B.drop); win('bursts', b0 - 2, b1 + 4, B.bursts); }
  if (ends.length) {
    // (each boat's end its share of the seconds: the blow, and where she is forced up, her coming up and her crew's
    // going; the scene cuts between them)
    // (10 s for one, a little more for each other, within the most the scene may run)
    const others = beats.reduce((a, x) => a + x.screen, 0);
    const share = Math.min(B.end + 4 * (ends.length - 1), B.most - others) / ends.length;
    ends.forEach((x, i) => {
      const s = Math.round(share * (i + 1)) - Math.round(share * i);
      if (x.by === 'escort' && x.how === 'scuttled' && x.up !== undefined) {
        const a = Math.max(1, Math.round(s * 0.4));
        win('end', x.t - 3, x.t + 5, a);
        win('end', x.up - 4, Math.max(x.sunk + 8, Math.min(x.rescue ?? 0, x.sunk + 20)), s - a);
      } else win('end', x.by === 'air' ? x.t - 4 : x.t - 3, x.sunk + 6, s);
    });
  }
  beats.sort((a, b) => a.from - b.from);
  // (windows that meet are joined, their seconds added)
  for (let i = 1; i < beats.length; i++) {
    if (beats[i].from <= beats[i - 1].to) {
      const a = beats[i - 1], b = beats[i];
      beats[i - 1] = { what: a.what === b.what ? a.what : `${a.what}+${b.what}`, from: a.from, to: Math.max(a.to, b.to), screen: a.screen + b.screen };
      beats.splice(i--, 1);
    }
  }
  // (the views run to the last beat, event or burst, and stagePick sees that the tracks run on past it)
  const tEnd = Math.ceil((Math.max(...beats.map((x) => x.to), ...events.map((x) => x.t), ...runs.map((r) => r.last)) + 4) / STEP) * STEP;

  // ---- The views ---------------------------------------------------------------------------------------------
  events.sort((a, b) => a.t - b.t || (a.what === 'sea:torpedoHit' || a.what === 'sea:depthChargeHit' ? 0 : 1) - (b.what === 'sea:torpedoHit' || b.what === 'sea:depthChargeHit' ? 0 : 1));
  const contactT = events.find((e) => e.what === 'sea:torpedo' || e.what === 'sea:sonar' || e.what === 'sea:report')?.t ?? null;
  const told = new Map<number, Record<string, string>>();
  /** A piece as the view tells it: whole the first time; then what changed, and what the reader takes afresh. */
  const unitOut = (p: Piece, t: number): Record<string, unknown> => {
    const gone = p.sunk >= 0 && p.sunk <= t + 1e-9;
    const q = p.track.at(gone ? p.sunk : t);
    const u: Record<string, unknown> = { id: p.id, side: p.side, kind: p.cls, name: p.name, nat: p.nat };
    u.x = r1(q.x); u.y = r1(q.y); u.face = r3(q.h); u.str = gone ? 0 : 1; u.mor = 1; u.state = gone ? 'gone' : 'steady';
    u.figures0 = 1; u.figures = gone ? 0 : 1; u.w = p.beam;
    u.d = p.len; // (the piece's column length, as in every recording: a boat's depth is extra.keel, never d)
    const hunt = (p.hunt ?? []).find((h) => h.from <= t + 1e-9 && t < h.to);
    u.act = gone ? 'down' : hunt?.phase === 'run' ? 'run' : 'march';
    u.speed = gone ? 0 : r1(q.v); u.engaged = -1; u.loop = gone ? '' : p.loop;
    const x: Record<string, unknown> = { cls: p.cls, ships0: 1, ships: gone ? 0 : 1, sunk: gone ? 1 : 0, way: gone ? 0 : r1(q.v), speed: q.v > 7 ? 'full' : 'cruise', target: -1, fleet: p.fleet };
    if (p.column !== undefined) x.column = p.column;
    const fresh = new Set<string>();
    if (p.cls === 'ss' && !gone) {
      const k = keelOf(p.keels!, t);
      x.dive = k.dive; x.keel = r1(k.keel); x.scope = k.dive === 'periscope' && k.scope;
      fresh.add('dive'); fresh.add('keel'); fresh.add('scope');
      x.tubes = Math.max(0, (p.tubes ?? 0) - (p.fired ?? []).filter((fd) => fd.t <= t + 1e-9).reduce((s, fd) => s + fd.n, 0));
      x.heard = escorts.filter((e) => (e.hunt ?? []).some((h) => h.on === p.id && h.phase === 'run' && h.from <= t + 1e-9 && t < h.to)).map((e) => e.id);
    }
    if (gone) fresh.add('sunk');
    if (p.hunt && !gone) {
      x.huntPhase = hunt?.phase ?? 'search';
      if (hunt) x.huntUntil = r1(hunt.to);
      if (hunt?.phase === 'run') {
        const b = byId.get(hunt.on)!.track.at(t);
        x.echo = { on: hunt.on, kind: hunt.kind, range: Math.round(dist(q, b)), bearing: r3(bearing(q.x, q.y, b.x, b.y)), err: r3(hunt.err) };
        fresh.add('echo');
      }
    }
    u.extra = x;
    const was = told.get(p.id);
    const now: Record<string, string> = {};
    for (const [k, v] of Object.entries(u)) if (k !== 'extra') now[k] = JSON.stringify(v);
    for (const [k, v] of Object.entries(x)) now[`extra.${k}`] = JSON.stringify(v);
    told.set(p.id, now);
    if (!was) return u;
    // (after the first view: the id, and only what changed or is taken afresh)
    const out: Record<string, unknown> = { id: p.id };
    for (const k of Object.keys(u)) if (k !== 'extra' && k !== 'id' && was[k] !== now[k]) out[k] = u[k];
    const ox: Record<string, unknown> = {};
    for (const k of Object.keys(x)) if (fresh.has(k) || was[`extra.${k}`] !== now[`extra.${k}`]) ox[k] = x[k];
    if (Object.keys(ox).length) out.extra = ox;
    return out;
  };
  const views: Record<string, unknown>[] = [];
  let lastZones = '', ei = 0;
  for (let t = 0; t <= tEnd + 1e-9; t += STEP) {
    const evs: Record<string, unknown>[] = [];
    while (ei < events.length && events[ei].t <= t + 1e-9) {
      const e = events[ei++];
      const o: Record<string, unknown> = { t: r1(e.t) };
      if (e.x !== undefined && e.y !== undefined) { o.x = Math.round(e.x); o.y = Math.round(e.y); }
      if (e.cue) o.cue = e.cue;
      o.what = e.what;
      if (e.unit !== undefined) o.unit = e.unit;
      if (e.side) o.side = e.side;
      o.text = e.text;
      if (e.extra) o.extra = e.extra;
      evs.push(o);
    }
    const units = pieces.map((p) => unitOut(p, t));
    const live = zones.filter((z) => z.at <= t + 1e-9 && z.until >= t - 1e-9);
    const zs = JSON.stringify(live);
    const lostAtt = boats.filter((b) => b.sunk >= 0 && b.sunk <= t + 1e-9).length / boats.length;
    const lostDef = guarded.filter((g) => g.sunk >= 0 && g.sunk <= t + 1e-9).length / Math.max(1, guarded.length + escorts.length);
    const v: Record<string, unknown> = {
      t, limit: tEnd, speed: 1, units, balance: { att: 0.5, def: 0.5 }, events: evs, ai: { att: true, def: true }, over: null, me: pick.me,
      breaking: { att: 0, def: 0 }, contact: contactT !== null && contactT <= t + 1e-9 ? r1(contactT) : null, lost: { att: r3(lostAtt), def: r3(lostDef) }, broken: null,
    };
    if (zs !== lastZones || t === 0) { v.zones = live; lastZones = zs; }
    views.push(v);
  }

  // ---- The header, the caption and the card -------------------------------------------------------------------
  const caption = captionOf(m, pick, sea, year, month0);
  views[views.length - 1].over = { winner: '', level: '', lines: caption };
  const night = nightOf(sea.lat, sea.lon, year, month0, day, look('hour'));
  // (how far a ship is seen: a hash, brighter with the moon up and full; look only, as the form's `vis` is for D1)
  const moonLight = night.moon.up > 0 ? night.moon.lit : 0;
  const vis = Math.round(((1500 + 3500 * look('vis')) * (0.6 + 0.6 * moonLight)) / 100) * 100;
  const windy = (sea.basin === 'atlantic' || sea.basin === 'north') && (month0 >= 9 || month0 <= 2) ? 2 : 0;
  const classes: Record<string, Record<string, number | string>> = {
    ss: { len: boatRow.len, beam: boatRow.beam, speed: boatRow.surf, turn: ssK.turn, reach: 0, name: boatRow.cls },
    dd: { len: esc.len, beam: esc.beam, speed: esc.speed, turn: esc.turn, reach: 5000, name: esc.cls },
  };
  if (pick.case === 'convoy') classes.mv = { len: L.convoy.mv.len, beam: L.convoy.mv.beam, speed: convRow.speed, turn: L.convoy.mv.turn, reach: 0, name: L.convoy.mv.name };
  for (const c of new Set(guarded.map((p) => p.cls))) if (c !== 'mv') { const h = L.heavy[c as HeavyClass]; classes[c] = { len: h.len, beam: h.beam, speed: h.speed, turn: h.turn, reach: 5000 }; }
  const shipsOf = (ps: Piece[]): Record<string, number> => { const o: Record<string, number> = {}; for (const p of ps) o[p.cls] = (o[p.cls] ?? 0) + 1; return o; };
  const staged: StagedNote = {
    scene: 'sub', case: pick.case, beats, seconds: beats.reduce((s, x) => s + x.screen, 0),
    caption: { lines: caption, check: L.caption.check },
    facts: { kind: f.kind, sea: f.sea, by: f.by, on: f.on, week, tons: f.tons ?? 0, ships: pick.case === 'convoy' ? pick.ships : pick.heavy.length, boats: pick.kills, escortsLost: f.escortsLost ?? 0, source: pick.source },
    drawn: { boats: boats.length, kills: ends.length, byAir: ends.filter((x) => x.by === 'air').length, hits: hitAt.size, merchants: guarded.filter((p) => p.cls === 'mv').length, heavy: guarded.filter((p) => p.cls !== 'mv').length, escorts: escorts.length, ends: ends.map((x) => `${x.how} by ${x.by}`) },
    looks: { boat: boatRow.cls, torpedo: fishRow.fish, wakeless: fishRow.wakeless, escort: esc.cls, ahead: fleetEsc ? null : escRow.ahead, aircraft: airRow.name, radar: fleetEsc ? true : escRow.radar },
    night: { hour: r1(night.hour), hours: r1(night.hours), dusk: r1(night.dusk), dawn: r1(night.dawn) },
    standIns,
  };
  const field: Record<string, unknown> = {
    size: SIZE, edge: 5000, zoneName: sea.name,
    title: pick.case === 'convoy' ? `${upFirst(f.routeName ?? 'the convoy')}: a night in ${MONTHS[month0]}` : pick.case === 'hunt' ? `The hunt in the ${sea.name}` : `${upFirst(an(CLASS_ONE[pick.heavy[0]]))} torpedoed in the ${sea.name}`,
    date: `${day} ${MONTHS[month0]} ${year}`, when: { year, month: month0 }, lat: sea.lat, lon: sea.lon,
    att: { nat: attNat, adj: attAdj, human: m.human === attNat, fleets: [{ id: 1, name: boatFleet, ships: shipsOf(boats) }] },
    def: { nat: defNat, adj: defAdj, human: m.human === defNat, fleets: [{ id: 2, name: defFleet, ships: shipsOf([...guarded, ...escorts]) }] },
    classes,
    torp: { travel: r1(fans.length ? fans[0].running : 60), radius: fans.length ? Math.round(fans[0].range * Math.tan((fans[0].spread / 2) * RAD)) + 60 : 300 },
    strike: { windup: 90, radius: 1500 },
    wind: { from: r3(TAU * look('wind')), speed: r1(4 + 7 * look('windspeed') + windy) },
    subs: { scope: SCOPE, deep: boatRow.deep, crush: boatRow.crush, dived: boatRow.dived },
    dc: { sink: sinkRate, lethal: LETHAL },
    light: { sun: { up: r1(night.sun.up), from: bearingDeg(night.sun.from) }, moon: { lit: r3(night.moon.lit), up: r1(night.moon.up), from: bearingDeg(night.moon.from) }, vis },
    staged,
  };
  const staged0: Staged = {
    what: "STAGED: the month's submarine scene (form D1, ruling 241), staged from the month's booked facts with no dice, for the 3D scene to show. Look only: no rule reads it. Each view carries a piece whole the first time it shows, then only what changed (a boat's dive, keel and scope and a lost piece's sunk in every view), and the zones only where they changed. field.staged.beats are the windows of battle time the scene shows and the screen seconds each is given.",
    recording: true, key, seed: hash32(`${m.seed}:${m.turn}:${key}`), step: STEP, field, views,
    after: { lines: caption, level: '', winner: '', seconds: tEnd },
  };
  return { staged: staged0, last: tEnd, end: END };
}

/** The caption: the month's booked figures (for a convoy month, its tons and ships at about 5,200 GRT a ship). */
function captionOf(m: MonthFacts, pick: ScenePick, sea: SeaPlace, year: number, month0: number): string[] {
  const f = pick.fact;
  const adj = (n: string): string => m.names?.[n]?.adj ?? n;
  const when = `${MONTHS[month0]} ${year}`;
  const subs = (n: number, nat: string): string => `${thousands(n)} ${adj(nat)} submarine${n === 1 ? '' : 's'}`;
  if (pick.case === 'convoy') {
    // (the historian, 22 November: about 5,200 GRT a ship, locked; "by submarines", not "by U-boats")
    const lines = [`${upFirst(f.routeName ?? 'the convoy routes')}, ${when}: ${thousands(f.tons ?? 0)} tons of shipping sunk by submarines, about ${thousands(pick.ships)} ship${pick.ships === 1 ? '' : 's'} at about ${thousands(L.caption.grt)} tons a ship.`];
    if (pick.kills) lines.push(`${upFirst(subs(pick.kills, pick.boats))} sunk.`);
    return lines;
  }
  if (pick.case === 'hunt') {
    const cost = f.escortsLost ? `, for the loss of ${f.escortsLost} destroyer${f.escortsLost === 1 ? '' : 's'}` : '';
    return [`${upFirst(subs(pick.kills, pick.boats))} sunk in the ${sea.name}, ${when}${cost}.`];
  }
  const lines = [`${pick.heavy.length === 1 ? upFirst(an(CLASS_ONE[pick.heavy[0]])) : `${upFirst(words(pick.heavy.length))} heavy ships`} of ${f.fleet ?? `the ${adj(f.on)} fleet`} torpedoed and sunk in the ${sea.name}, ${when}.`];
  if (pick.kills) lines.push(`${upFirst(subs(pick.kills, pick.boats))} sunk there this month.`);
  return lines;
}

// ---- Reading the month from the game (no writes) ------------------------------------------------------------

/** A convoy route as navy.json lists it. */
export interface RouteRow { id: string; name: string; to: string; zones: string[] }

/** The index of the first headline of the scene's window: a mark the caller kept, else the month's news (as the bridge's markBackTo counts a month whose opening was not seen). */
function firstOf(g: Game, from?: number): number {
  const month = g.headlines.findIndex((h) => h.turn >= g.turn - 1);
  if (typeof from === 'number' && from >= 0 && from <= g.headlines.length) return from;
  return month < 0 ? g.headlines.length : month;
}

/**
 * The month's facts at sea from the news the rules wrote (and the record-only fields, when the rules keep them under
 * `subScene`), with the boats at sea as the month closed. It reads the game and writes nothing.
 */
export function factsOf(g: Game, routes: RouteRow[], from?: number, record?: SubSceneRecord): MonthFacts {
  const facts: SeaFact[] = [];
  const zoneByName = (text: string): number => {
    let best = -1, len = 0;
    g.seas.forEach((z, i) => { if (text.includes(z.name) && z.name.length > len) { best = i; len = z.name.length; } });
    return best;
  };
  const byAdj = (adj: string): string => Object.values(g.nations).find((n) => n.adj === adj)?.id ?? '';
  const atWarG = (a: string, b: string): boolean => !!g.nations[a]?.atWar.includes(b) || !!g.nations[b]?.atWar.includes(a);
  /** Submarines at sea in a sea by nation (any stance, or only raiding and patrolling ones), at war with a side. */
  const boatsIn = (z: number, struck: string, raiding = false): Record<string, number> => {
    const out: Record<string, number> = {};
    for (const x of g.fleets) {
      if (x.zone !== z || x.stance === 'port' || !(x.ships.ss > 0) || !atWarG(x.nat, struck)) continue;
      if (raiding && x.stance !== 'raid' && x.stance !== 'patrol') continue;
      out[x.nat] = (out[x.nat] ?? 0) + x.ships.ss;
    }
    return out;
  };
  const rec = record ?? (g as unknown as { subScene?: SubSceneRecord }).subScene;
  const hs: Headline[] = g.headlines.slice(firstOf(g, from));
  for (const h of hs) {
    const text = h.text;
    // A torpedoing (navy.ts torpedoes()): "A submarine torpedoes and sinks a cruiser of the Home Fleet in the North Atlantic: ..."
    const tor = /^A submarine torpedoes and sinks an? (battleship|carrier|cruiser) of (.+?) in the (.+?): /.exec(text);
    if (tor && h.nat) {
      const cls = ({ battleship: 'bb', carrier: 'cv', cruiser: 'ca' } as const)[tor[1] as 'battleship' | 'carrier' | 'cruiser'];
      const fleetName = tor[2].replace(/^the /, '');
      const fleet = g.fleets.find((x) => x.nat === h.nat && x.name === fleetName);
      const z = zoneByName(`in the ${tor[3]}`);
      const sea = z >= 0 ? g.seas[z].key : '';
      const r = rec?.torpedoes?.find((x) => (x.turn === undefined || x.turn === h.turn) && x.nat === h.nat && (!fleet || x.fleet === fleet.id) && x.sea === sea);
      facts.push({ kind: 'torpedo', sea, turn: h.turn, week: h.week, by: '', on: h.nat, cls, fleet: tor[2], ships: fleet ? { ...fleet.ships } : undefined, ...(z >= 0 ? { boatsAt: boatsIn(z, h.nat) } : {}),
        ...(r ? { attackers: r.picked ? [r.picked, ...r.attackers.filter((a) => a !== r.picked)] : r.attackers } : {}) });
      continue;
    }
    // A hunt (navy.ts huntSubs()): "Destroyers of the Home Fleet sink 2 German submarines in the North Atlantic, for the loss of 1 destroyer."
    const hunt = /\bsinks? (\d+) ([A-Z][A-Za-z ]*?) submarines? in the (.+?)(?:, for the loss of (\d+) destroyers?)?\.$/.exec(text);
    if (hunt && h.arm === 'sea' && h.won && h.lost) {
      const z = zoneByName(`in the ${hunt[3]}`);
      const sea = z >= 0 ? g.seas[z].key : '';
      const r = rec?.hunts?.find((x) => (x.turn === undefined || x.turn === h.turn) && x.nat === h.won && x.sea === sea);
      facts.push({ kind: 'hunt', sea, turn: h.turn, week: h.week, by: h.won, on: h.lost, boats: Number(hunt[1]), escortsLost: hunt[4] ? Number(hunt[4]) : 0, ...(r ? { hunters: { ships: r.hunters, aircraft: r.aircraft } } : {}) });
      continue;
    }
    // The routine hunts folded into one line of a computer nation's month (game.ts foldNews): "... 2 British submarines sunk in the North Sea; ..."
    if (h.kind === 'info' && h.nat && /^The month in the air and at sea, /.test(text)) {
      const re = /(\d+) ([A-Z][A-Za-z ]*?) submarines? sunk in the ([^;.]+)/g;
      for (let mm = re.exec(text); mm; mm = re.exec(text)) {
        const on = byAdj(mm[2]);
        const z = zoneByName(`in the ${mm[3]}`);
        if (!on || z < 0) continue;
        facts.push({ kind: 'hunt', sea: g.seas[z].key, turn: h.turn, week: h.week, by: h.nat, on, boats: Number(mm[1]), escortsLost: 0 });
      }
      continue;
    }
    // A convoy month (navy.ts convoyWar()): "The Atlantic convoys: 271,000 tons of shipping sunk this month, 2 submarines destroyed."
    if (h.kind === 'info' && h.arm === 'sea' && h.nat && /tons of shipping sunk this month/.test(text)) {
      const route = routes.find((r) => r.to === h.nat && text.toLowerCase().startsWith(`${r.name.toLowerCase()}:`));
      if (!route) continue;
      const book = g.convoys[route.id];
      const said = /: ([\d,]+) tons of shipping sunk this month(?:, (\d+) submarines? destroyed)?/.exec(text);
      const same = !!book && book.turn === h.turn;
      const r = rec?.convoys?.find((x) => (x.turn === undefined || x.turn === h.turn) && x.route === route.id);
      // Its sea: where the record has the most tons; else as escort() picks one (navy.ts:1441), where the raiders are
      // thickest, from the boats at sea as the month closed; else the route's first.
      const zones = route.zones.map((k) => g.seas.findIndex((z) => z.key === k)).filter((z) => z >= 0);
      const bySea = r?.tons ?? {};
      const top = Object.keys(bySea).sort((a, b) => bySea[b] - bySea[a] || (a < b ? -1 : 1))[0];
      let z = top !== undefined ? g.seas.findIndex((x) => x.key === top) : -1;
      if (z < 0) {
        let most = 0;
        for (const zi of zones) { const n = Object.values(boatsIn(zi, h.nat, true)).reduce((s, x) => s + x, 0); if (n > most) { most = n; z = zi; } }
      }
      if (z < 0) z = zones[0] ?? -1;
      facts.push({
        kind: 'convoy', sea: z >= 0 ? g.seas[z].key : route.zones[0], turn: h.turn, week: h.week, by: '', on: h.nat, route: route.id, routeName: route.name,
        tons: same ? book.tons : said ? Number(said[1].replace(/,/g, '')) : 0, boats: same ? book.subsLost : said?.[2] ? Number(said[2]) : 0, escortsLost: same ? book.escortsLost : 0,
        ...(z >= 0 ? { boatsAt: boatsIn(z, h.nat, true) } : {}),
        ...(r ? { raiders: r.raiders, escorts: r.escorts, airGuard: r.airGuard, tonsBy: r.tons } : {}),
      });
    }
  }
  const foes: Record<string, string[]> = {};
  const names: Record<string, { name: string; adj: string }> = {};
  for (const n of Object.values(g.nations)) { foes[n.id] = [...n.atWar]; names[n.id] = { name: n.name, adj: n.adj }; }
  return { human: g.human ?? '', seed: g.seed, turn: g.turn, facts, foes, names };
}

/**
 * The bridge's TC.subScene(from?): the month's submarine scene, staged. `from` is the headline mark to count from (the
 * `mark` an earlier call gave back, or the month card's own); without it, the last month's news. `mark` is where the
 * next window starts; `recording` is the staged recording's JSON text (what SeaFeed::ReadRecording reads), or null when
 * the month holds no scene for the player. It reads the game and writes nothing.
 */
export function subSceneOf(g: Game, routes: RouteRow[], from?: number): { mark: number; pick: { case: SceneCase; sea: string; me: string; boats: string; guard: string } | null; recording: string | null } {
  const mark = g.headlines.length;
  if (!g.human) return { mark, pick: null, recording: null };
  const m = factsOf(g, routes, from);
  const pick = pickScene(m);
  if (!pick) return { mark, pick: null, recording: null };
  return { mark, pick: { case: pick.case, sea: pick.fact.sea, me: pick.me, boats: pick.boats, guard: pick.guard }, recording: JSON.stringify(stagePick(m, pick)) };
}
