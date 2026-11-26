// Weather (ruling 127, Brian's ask of 16 November 2026: "if the weather is bad then you cant do bombing raids and
// airplanes are grounded ... this happened with the battle of the bulge"; the sky's words are ruling 143). The grand
// campaign only, behind a switch: data/campaign/weather.json 'on', kept with each campaign in g.flags.weather.
//
// Each month every weather zone (a theatre's worth of land, or a sea) is under one of three skies:
//
//   clear sky    everything flies
//   cloud        everything flies, but fewer bombs hit, fewer bombers are found and brought down, and air support
//                over a battle is worth half for both sides
//   closed sky   nothing flies in the zone for either side: no raid, no group based there, no air support, no sea
//                cover or strike, no air scouting. A mission needs an open sky at its base and at its target.
//
// The sky is dealt by the campaign's seed from the zone's odds for the month, apart from the game's own dice, so it is
// the same however often a month is played and it moves nothing else. This month's sky is known before any order;
// next month's is a forecast with the true odds. At stops 1 and 2 of the history dial a short list of history's skies
// holds (December 1944 over the Ardennes first).
//
// With the switch off nothing here runs: every hook elsewhere asks weatherOn() first, and the skies all read clear.
// The hooks are one line each: air.ts (who flies, what cloud does to bombs, fighters and guns), game.ts (the air in a
// land battle), navy.ts (air cover at sea), airwar.ts (the planner and scouting), staff.ts (the switch, as a campaign
// opens), and the cards' words below.

import data from '../../data/campaign/weather.json';
import { Rng, hashString } from '../core/rng';
import { turnToDate } from '../core/ticket';
import { stopDef } from './history';
import { beyondEurope } from './world';
import type { AirFleet, Game } from './types';

export type Sky = 'clear' | 'cloud' | 'closed';
interface Zone { key: string; name: string; the: boolean; plate: [number, number]; snow: number[]; seas: string[]; odds: [number, number, number][] }
interface Clause { zone: string; world: boolean; nats: string[]; x?: [number | null, number | null]; y?: [number | null, number | null] }
interface BookSky { turn: number; zone: string; sky: Sky; why: string }

export const WEATHER = data as unknown as {
  on: boolean;
  cloud: { bombsDay: number; bombsNight: number; met: number; gunsDay: number; gunsNight: number; support: number };
  zones: Zone[];
  lands: Clause[];
  book: BookSky[];
  words: { clear: string; cloud: string; closed: string; means: Record<Sky, string>; tip: string };
};

const SKIES: Sky[] = ['clear', 'cloud', 'closed'];
const MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];

/** True in a campaign played with weather: the grand campaign, begun with the switch on. */
export const weatherOn = (g: Game): boolean => g.flags.open === 1 && g.flags.weather === 1;

/** Sets the switch for a grand campaign as it opens, from the data: a campaign keeps the setting it began with. */
export function openWeather(g: Game): void {
  if (WEATHER.on) g.flags.weather = 1;
}

/** For tests and probes: weather on or off in this campaign. */
export function setWeather(g: Game, on: boolean): void {
  if (on) g.flags.weather = 1;
  else delete g.flags.weather;
}

// ---- Where each zone lies --------------------------------------------------------------------

interface ZoneMap { regions: Int16Array; seas: Int16Array }
const maps = new WeakMap<object, ZoneMap>();

const within = (v: number, b?: [number | null, number | null]): boolean => !b || ((b[0] === null || v >= b[0]) && (b[1] === null || v <= b[1]));

/** Each province's zone and each sea's, worked out once for a map (a province no clause matches has none: -1). */
function zoneMap(g: Game): ZoneMap {
  let m = maps.get(g.regions);
  if (m && m.regions.length === g.regions.length && m.seas.length === g.seas.length) return m;
  const index = new Map(WEATHER.zones.map((z, i) => [z.key, i]));
  const regions = new Int16Array(g.regions.length).fill(-1);
  for (const r of g.regions) {
    const who = r.first ?? r.owner, world = beyondEurope(r);
    const c = WEATHER.lands.find((l) => l.world === world && l.nats.includes(who) && within(r.x, l.x) && within(r.y, l.y));
    if (c) regions[r.id] = index.get(c.zone) ?? -1;
  }
  const seas = new Int16Array(g.seas.length).fill(-1);
  for (const z of g.seas) seas[z.id] = WEATHER.zones.findIndex((w) => w.seas.includes(z.name));
  m = { regions, seas };
  maps.set(g.regions, m);
  return m;
}

/** The weather zone a province lies in, or -1. */
export const zoneOfRegion = (g: Game, region: number): number => (region >= 0 && region < g.regions.length ? zoneMap(g).regions[region] : -1);
/** The weather zone a sea lies in, or -1. */
export const zoneOfSea = (g: Game, sea: number): number => (sea >= 0 && sea < g.seas.length ? zoneMap(g).seas[sea] : -1);

/** A zone's name for a sentence: "North-west Europe", "the North Atlantic". */
export const zoneName = (zone: number): string => { const z = WEATHER.zones[zone]; return z ? `${z.the ? 'the ' : ''}${z.name}` : ''; };
/** A zone's name to start a line with. */
const zoneHead = (zone: number): string => { const s = zoneName(zone); return s[0].toUpperCase() + s.slice(1); };

// ---- The sky ---------------------------------------------------------------------------------

/** The month of a turn, 0 for January. */
const monthOf = (turn: number): number => turnToDate(turn).month;

/** History's sky for this zone and month at stops 1 and 2, if the list has one. */
function bookSky(g: Game, zone: number, turn: number): BookSky | null {
  if (!stopDef(g).book) return null;
  const key = WEATHER.zones[zone].key;
  return WEATHER.book.find((b) => b.turn === turn && b.zone === key) ?? null;
}

/** The odds of each sky for a zone and month, in tenths: [clear, cloud, closed]. */
export const oddsOf = (zone: number, turn: number): [number, number, number] => WEATHER.zones[zone].odds[monthOf(turn)];

const dealt = new WeakMap<object, { key: string; skies: Sky[] }>();

/** Every zone's sky in one month, dealt from the seed (never the game's own dice) or taken from history's list. */
function skiesOf(g: Game, turn: number): Sky[] {
  const key = `${g.seed}:${turn}:${stopDef(g).book ? 1 : 0}`;
  const hold = turn === g.turn ? dealt.get(g) : undefined;
  if (hold && hold.key === key) return hold.skies;
  const skies = WEATHER.zones.map((z, i): Sky => {
    const b = bookSky(g, i, turn);
    if (b) return b.sky;
    const roll = new Rng(hashString(`sky:${g.seed}:${turn}:${z.key}`)).next() * 10;
    const [clear, cloud] = oddsOf(i, turn);
    return roll < clear ? 'clear' : roll < clear + cloud ? 'cloud' : 'closed';
  });
  if (turn === g.turn) dealt.set(g, { key, skies });
  return skies;
}

/** A zone's sky in a month (this month by default). Clear where there is no weather. */
export function skyOf(g: Game, zone: number, turn = g.turn): Sky {
  if (!weatherOn(g) || zone < 0) return 'clear';
  return skiesOf(g, turn)[zone];
}

/** The sky over a province this month. */
export const skyAt = (g: Game, region: number): Sky => (weatherOn(g) ? skyOf(g, zoneOfRegion(g, region)) : 'clear');
/** The sky over a sea this month. */
export const skyAtSea = (g: Game, sea: number): Sky => (weatherOn(g) ? skyOf(g, zoneOfSea(g, sea)) : 'clear');

/** The zone an air group flies from: its airfield's, or its carrier's sea (or harbour). */
export function zoneOfGroup(g: Game, f: AirFleet): number {
  if (f.ship === undefined) return zoneOfRegion(g, f.base);
  const c = g.fleets.find((x) => x.id === f.ship);
  if (!c) return -1;
  return c.stance !== 'port' ? zoneOfSea(g, c.zone) : zoneOfRegion(g, c.base);
}

/** True when an air group cannot take off this month: the sky over its airfield (or its carrier) is closed. */
export const grounded = (g: Game, f: AirFleet): boolean => weatherOn(g) && skyOf(g, zoneOfGroup(g, f)) === 'closed';

/** What cloud over the target does to the bombs that find it: 1 under a clear sky, or with no weather. */
export const cloudAim = (sky: Sky, night: boolean): number => (sky === 'cloud' ? (night ? WEATHER.cloud.bombsNight : WEATHER.cloud.bombsDay) : 1);
/** What cloud over the target does to the defending fighters that find the bombers. */
export const cloudMet = (sky: Sky): number => (sky === 'cloud' ? WEATHER.cloud.met : 1);
/** What cloud over the target does to the bombers the guns bring down. */
export const cloudGuns = (sky: Sky, night: boolean): number => (sky === 'cloud' ? (night ? WEATHER.cloud.gunsNight : WEATHER.cloud.gunsDay) : 1);
/** What the sky over a battle leaves of the air on either side: none under a closed sky, half under cloud. */
export const skySupport = (sky: Sky): number => (sky === 'closed' ? 0 : sky === 'cloud' ? WEATHER.cloud.support : 1);

// ---- The words -------------------------------------------------------------------------------

/** Why a group cannot fly this month, or null: its own airfield's sky, then the target's. */
export function whyNotSky(g: Game, f: AirFleet, targetZone: number): string | null {
  if (!weatherOn(g)) return null;
  const home = zoneOfGroup(g, f);
  if (skyOf(g, home) === 'closed') return `Grounded this month: the sky is closed over ${zoneName(home)}.`;
  if (skyOf(g, targetZone) === 'closed') return `Closed sky this month: nothing flies over ${zoneName(targetZone)}.`;
  return null;
}

/** The line on an attack card or a fleet's card for the sky over a place, or null under a clear sky. */
export function skyBattleLine(g: Game, zone: number, place: string): string | null {
  const s = skyOf(g, zone);
  if (s === 'closed') return `The sky is closed over ${place} this month: no aircraft will fly for either side.`;
  if (s === 'cloud') return `Cloud over ${place} this month: air support is worth half for both sides.`;
  return null;
}

/** The raid card's one line for the sky over its target, or null with no weather. A long name becomes "the target". */
export function skyRaidLine(g: Game, zone: number, place: string): string | null {
  if (!weatherOn(g) || zone < 0) return null;
  const where = place.length > 24 ? 'the target' : place;
  const s = skyOf(g, zone);
  return s === 'closed' ? `Closed sky over ${where}: nothing flies there this month.`
    : s === 'cloud' ? `Cloud over ${where}: fewer bombs hit, and fewer bombers are found.` : `Clear sky over ${where}.`;
}

const TENTHS: Record<number, [number, number]> = { 1: [1, 10], 2: [1, 5], 3: [3, 10], 4: [2, 5], 5: [1, 2], 6: [3, 5], 7: [7, 10], 8: [4, 5], 9: [9, 10] };
const chances = (t: number, first: boolean): string => { const [a, b] = TENTHS[t]; return first ? `${a} ${a === 1 ? 'chance' : 'chances'} in ${b}` : `${a} in ${b}`; };
const word = (s: Sky): string => (s === 'closed' ? 'closed' : s);

/**
 * Next month's forecast for a zone, with the true odds (ruling 127(c), 143(c)): "December: likely cloud, 1 chance in 2;
 * closed, 1 in 5." The closed chance is named whenever closed is not the likeliest and has one chance in ten or more.
 */
export function forecastLine(g: Game, zone: number, turn = g.turn + 1, lead?: string): string {
  const month = lead ?? MONTHS[monthOf(turn)];
  const b = bookSky(g, zone, turn);
  if (b) return `${month}: ${b.sky === 'clear' ? 'a clear sky' : b.sky === 'cloud' ? 'cloud' : 'a closed sky'}, as history had it.`;
  const odds = oddsOf(zone, turn);
  const top = Math.max(...odds);
  if (top >= 10) return `${month}: ${SKIES[odds.indexOf(10)] === 'clear' ? 'a clear sky' : SKIES[odds.indexOf(10)]}, for certain.`;
  // On a tie, both are named; the worse sky first.
  const best = SKIES.filter((_, i) => odds[i] === top).reverse();
  let line = best.length > 1 ? `${month}: ${best.map(word).join(' or ')}, ${chances(top, true)} each` : `${month}: likely ${word(best[0])}, ${chances(top, true)}`;
  if (!best.includes('closed') && odds[2] >= 1) line += `; closed, ${chances(odds[2], false)}`;
  return `${line}.`;
}

/** Zones where a nation flies or fights: its air groups' and the ground and seas next to its fronts. */
export function zonesOf(g: Game, nat: string): Set<number> {
  const out = new Set<number>();
  for (const f of g.airFleets) if (f.nat === nat && f.ftr + f.bmr > 0) { const z = zoneOfGroup(g, f); if (z >= 0) out.add(z); }
  const n = g.nations[nat];
  if (n?.atWar.length) {
    for (const r of g.regions) {
      if (r.ctrl !== nat) continue;
      if (r.nb.some((id) => n.atWar.includes(g.regions[id].ctrl))) { const z = zoneOfRegion(g, r.id); if (z >= 0) out.add(z); }
    }
  }
  return out;
}

export interface SkyRow { zone: number; name: string; sky: Sky; text: string; forecast: string; snow: boolean }
export interface SkyView {
  /** Closed zones first, a line each; then the cloud zones, sharing one line; then one line for the rest, always. */
  lines: { sky: Sky; text: string; forecast: string[] }[];
  rows: SkyRow[];
  /** True when a zone where he has a front or an air group is closed: THE SKY is never folded away then. */
  open: boolean;
  /** Last month's report, naming its month, when the weather kept his air groups on the ground. */
  report: string | null;
  /** The sky's tip (ruling 143(g)), the first month a sky where he flies or fights is closed; the card that shows it marks it told (g.flags['wx:tip']). */
  tip: string | null;
}

/** THE SKY on the month card (ruling 143(b)), for a nation: the zones it flies or fights in, and next month's chances. */
export function skyView(g: Game, nat: string | null = g.human): SkyView | null {
  if (!weatherOn(g) || !nat || !g.nations[nat]) return null;
  const mine = [...zonesOf(g, nat)].sort((a, b) => a - b);
  const rows: SkyRow[] = WEATHER.zones.map((z, i) => ({ zone: i, name: zoneName(i), sky: skyOf(g, i), text: '', forecast: forecastLine(g, i), snow: z.snow.includes(monthOf(g.turn) + 1) }));
  for (const r of rows) r.text = r.sky === 'closed' ? `${zoneHead(r.zone)}: closed sky. Nothing flies there this month.` : r.sky === 'cloud' ? `${zoneHead(r.zone)}: cloud. Raids and air support count for less.` : `${zoneHead(r.zone)}: clear sky.`;
  const lines: SkyView['lines'] = [];
  const closed = mine.filter((z) => rows[z].sky === 'closed'), cloud = mine.filter((z) => rows[z].sky === 'cloud');
  for (const z of closed) lines.push({ sky: 'closed', text: rows[z].text, forecast: [rows[z].forecast] });
  if (cloud.length) {
    const names = cloud.map((z, i) => (i === 0 ? zoneHead(z) : zoneName(z)));
    lines.push({ sky: 'cloud', text: `${names.join(' and ')}: cloud. Raids and air support count for less.`, forecast: cloud.map((z) => (cloud.length > 1 ? `${zoneHead(z)}. ${rows[z].forecast}` : rows[z].forecast)) });
  }
  lines.push({ sky: 'clear', text: closed.length || cloud.length ? 'Clear sky everywhere else you fly.' : 'Clear sky everywhere you fly.', forecast: [] });
  const told = g.flags['wx:tip'];
  const tip = closed.length && nat === g.human && (!told || told === g.turn) ? WEATHER.words.tip : null;
  return { lines, rows, open: closed.length > 0, report: groundedReport(g, nat), tip };
}

/** Records, as a new month opens, how many of a nation's air groups the closed sky kept on the ground in the month gone. */
export function weatherMonth(g: Game): void {
  if (!weatherOn(g)) return;
  const was = g.turn - 1;
  for (const nat of Object.keys(g.nations)) {
    const n = g.nations[nat];
    const kept = n.alive && n.atWar.length ? g.airFleets.filter((f) => f.nat === nat && f.ftr + f.bmr > 0 && skyOf(g, zoneOfGroup(g, f), was) === 'closed').length : 0;
    if (kept) g.flags[`wx:kept:${nat}`] = was * 1000 + kept;
    else delete g.flags[`wx:kept:${nat}`];
  }
}

/** "In November the closed sky over North-west Europe kept 4 of your air groups on the ground." (ruling 143(d)). */
export function groundedReport(g: Game, nat: string): string | null {
  const v = g.flags[`wx:kept:${nat}`];
  if (!v) return null;
  const turn = Math.floor(v / 1000), k = v % 1000;
  if (turn !== g.turn - 1) return null;
  const zones = [...new Set(g.airFleets.filter((f) => f.nat === nat).map((f) => zoneOfGroup(g, f)).filter((z) => z >= 0 && skyOf(g, z, turn) === 'closed'))];
  const where = zones.length === 1 ? ` over ${zoneName(zones[0])}` : '';
  return `In ${MONTHS[monthOf(turn)]} the closed sky${where} kept ${k} of your air groups on the ground.`;
}
