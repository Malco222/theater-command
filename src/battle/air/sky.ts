// The sky over an air battle: whether the raid's day had a cloud deck under the bombers. The art director's ruling
// (19 November 04:40Z, on Brian's clips): a deck 1.5 to 2.5 km below the band only on a day history had one, and on a
// clear day scattered cumulus below instead. Where the campaign keeps weather (ruling 127), the deck follows its sky over
// the target that month, the one draw the raid card already priced: cloud, or a closed sky, is a deck; a clear sky,
// cumulus (Brian's standing rule: the weather varies battle to battle from one draw, and the 3D uses it). Without it,
// the chance comes from data/battle/air-cloud.json by where the target lies and the month (the historian reviews the
// table). The draws hash the battle's own seed and touch no dice. Shown, never counted: nothing in the battle reads it.
// The table gives only a deck's base and tops (by place, and for three zones by season): on a cumulus day the same draw
// stands in for the scattered cumulus.
// Heights are above the sea; on high ground (the Andes, Yunnan, Ethiopia, Anatolia) the 3D game lifts the cloud to at
// least 300 m above the ground, as the table's note says.
// How much of the sky is cloud varies from battle to battle too (Brian, 20 November 21:53Z: "make sure the weather is
// different and not always cloudy or some days more cloudy"): a deck day is broken to overcast, and a cumulus day runs
// from a clear sky to scattered cloud, clear days more often where and when the table's decks are rare. Drawn from the
// same seed, so the same battle has the same sky; every battle scene can take it (cover and its word).
import cloudData from '../../../data/battle/air-cloud.json';
import type { Sky } from '../../campaign/weather';
import { hashString } from '../../core/rng';

/**
 * What the 3D game draws under the bombers: a deck of cloud, or scattered cumulus; and that day's cloud base and tops,
 * drawn once inside the table's ranges for the place and month (the deck's ranges, which stand in for the cumulus on a
 * clear day).
 */
export interface AirCloud {
  kind: 'deck' | 'cumulus';
  /** This battle's cloud base and tops, metres above the sea (the tops at least 300 m above the base). */
  base: number;
  top: number;
  /** The table's ranges they were drawn from: base and tops, metres above the sea. */
  baseRange: [number, number];
  topsRange: [number, number];
  /** The chance of a deck that day (the table's month, with its year rules), and the table's zone it came from. */
  chance: number;
  zone: string;
  /** The campaign's sky over the target that month, where it keeps weather: the deck or the cumulus is its, not the table's. */
  weather?: Sky;
  /** How much of the sky is cloud: 0 a clear day, 1 a closed overcast (a share, as a forecaster's eighths over 8). */
  cover: number;
  /** The cover's word (skyWord): clear, fair, scattered, broken or overcast. */
  sky: SkyWord;
}

/** The sky in a word, by the forecaster's eighths: none clear, one or two fair, three or four scattered, five to seven
 * broken, all eight overcast. */
export type SkyWord = 'clear' | 'fair' | 'scattered' | 'broken' | 'overcast';
export function skyWord(cover: number): SkyWord {
  const eighths = Math.round(Math.max(0, Math.min(1, cover)) * 8);
  return eighths === 0 ? 'clear' : eighths <= 2 ? 'fair' : eighths <= 4 ? 'scattered' : eighths <= 7 ? 'broken' : 'overcast';
}

/** The most of the sky a cumulus day's cloud covers, and the least a deck's. */
export const CUMULUS_MOST = 0.55;
export const DECK_LEAST = 0.6;

/**
 * How much of the sky is cloud on a day of this kind, from a fraction u (0..1) drawn from the battle's seed. A deck
 * covers 0.6 to all of it, leaning toward the more (about one deck in five closes the sky). Cumulus covers none to
 * 0.55, leaning toward the less where the table's chance of a deck is small: about three cumulus days in ten are clear
 * at a chance of 0.1, one in ten at 0.6.
 */
export function coverOf(kind: AirCloud['kind'], chance: number, u: number): number {
  const v = Math.max(0, Math.min(1, u));
  const r2 = (x: number): number => Math.round(x * 100) / 100;
  if (kind === 'deck') return r2(DECK_LEAST + (1 - DECK_LEAST) * Math.pow(v, 0.7));
  const lean = Math.max(0.8, Math.min(2, 2 - 1.8 * chance));
  return r2(CUMULUS_MOST * Math.pow(v, lean));
}

/** A season's own heights in a zone: the months they hold for (0 = January), and the deck's base and tops there. */
export interface CloudSeason { months: number[]; base_m: number[]; tops_m: number[] }
export interface CloudZone { zone: string; box: { latS: number; latN: number; lonW: number; lonE: number }; months: number[]; base_m: number[]; tops_m: number[]; seasons?: CloudSeason[] }
const ZONES = cloudData.zones as CloudZone[];
const FALLBACK = cloudData.fallback as { months: number[]; base_m: number[]; tops_m: number[] };
/** The table's date rules: a zone (by the start of its name) whose chance moves by `add` before a date, given as the year
 * plus the month over 12 (1943.75 is 1 October 1943). */
const ADJUST = cloudData.adjust as { zone: string; before?: number; add: number }[];
/** The thinnest a cloud is drawn: its tops at least this far above its base (metres). */
export const CLOUD_THICK = 300;

const KX = Math.cos((50 * Math.PI) / 180);
/**
 * Latitude and longitude of a point on the campaign map. Every chart shares one projection (tools/build-geo-campaign.mjs,
 * build-geo-world.mjs, build-geo-far.mjs): x = (lon + 11) cos 50 x 40, y = (71.4 - lat) x 40, with the far side of the
 * world counted on eastward past 180.
 */
export function latLonOf(x: number, y: number): [number, number] {
  const lat = 71.4 - y / 40;
  const lon = -11 + x / (KX * 40);
  return [lat, ((((lon + 180) % 360) + 360) % 360) - 180];
}

const inBox = (lat: number, lon: number, b: CloudZone['box']): boolean =>
  lat >= b.latS && lat <= b.latN && (b.lonW <= b.lonE ? lon >= b.lonW && lon <= b.lonE : lon >= b.lonW || lon <= b.lonE);

/** The table's zone for a place (the first box that holds it), or null for the fallback. */
export function cloudZone(lat: number, lon: number): CloudZone | null {
  if (!Number.isFinite(lat) || !Number.isFinite(lon)) return null;
  return ZONES.find((z) => inBox(lat, lon, z.box)) ?? null;
}

/** A well-mixed fraction 0..1 from a string (FNV-1a, then MurmurHash3's finaliser so nearby seeds spread evenly). */
function fraction(s: string): number {
  let h = hashString(s);
  h ^= h >>> 16; h = Math.imul(h, 0x85ebca6b);
  h ^= h >>> 13; h = Math.imul(h, 0xc2b2ae35);
  h ^= h >>> 16;
  return (h >>> 0) / 4294967296;
}

/** The deck chance in a zone (null: the fallback) on a date, with the table's date rules, kept inside 0..1. */
export function deckChance(z: CloudZone | null, when: { year: number; month: number }): number {
  const row = z ?? FALLBACK;
  let p = row.months[((when.month % 12) + 12) % 12];
  if (z) for (const a of ADJUST) if (z.zone.startsWith(a.zone) && (a.before === undefined || when.year + (((when.month % 12) + 12) % 12) / 12 < a.before - 1e-9)) p += a.add;
  return Math.max(0, Math.min(1, Math.round(p * 100) / 100));
}

/**
 * The sky over a target at map point (x, y) on a date (month 0 = January), drawn once from the battle's seed: four
 * draws, for the deck, its base, its tops and the cover, each from its own hash of the seed. Given the campaign's sky
 * over the target that month (weather), the deck is its: under cloud or a closed sky, a deck; under a clear one, cumulus.
 */
export function cloudOf(x: number, y: number, when: { year: number; month: number }, seed: number, weather?: Sky): AirCloud {
  // Back to the 0.01 degree of the regions' seed points (data/campaign/*.mjs): the map keeps x and y to 0.1 unit, about
  // 0.002 degree, so a seed on a box's edge would otherwise fall to either side and miss the first-box rule.
  const q = (v: number): number => Math.round(v * 100) / 100;
  const [lat, lon] = latLonOf(x, y).map(q);
  const z = cloudZone(lat, lon);
  const row = z ?? FALLBACK;
  const chance = deckChance(z, when);
  const month = ((when.month % 12) + 12) % 12;
  const h = (z?.seasons ?? []).find((s) => s.months.includes(month)) ?? row;
  const baseRange: [number, number] = [h.base_m[0], h.base_m[1]];
  const topsRange: [number, number] = [h.tops_m[0], h.tops_m[1]];
  const at = (r: [number, number], u: number): number => Math.round((r[0] + u * (r[1] - r[0])) / 10) * 10;
  const base = at(baseRange, fraction(`airsky:base:${seed}`));
  const top = Math.max(at(topsRange, fraction(`airsky:top:${seed}`)), base + CLOUD_THICK);
  const kind: AirCloud['kind'] = weather ? (weather === 'clear' ? 'cumulus' : 'deck') : fraction(`airsky:${seed}`) < chance ? 'deck' : 'cumulus';
  const cover = coverOf(kind, chance, fraction(`airsky:cover:${seed}`));
  return {
    kind,
    base, top, baseRange, topsRange,
    chance,
    zone: z ? z.zone : 'elsewhere',
    cover, sky: skyWord(cover),
    ...(weather ? { weather } : {}),
  };
}
