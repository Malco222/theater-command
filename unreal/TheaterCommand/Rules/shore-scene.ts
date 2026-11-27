// The shelling scene: a coast shelled from the sea, staged for the month's sea scene (sea-scene/shore-guns/plan.md,
// rounds R0 and R1). LOOK ONLY.
//
// The contract (R0). When a fleet shells a coast in the grand campaign, the rules write a ShoreReport (fleets.ts bombard).
// This module stages that report as a short scene in the sea scene's recording format: the battle line off the coast,
// the guns, the falls on the troops' lines, the town or the works, the fort answering where the coast has one, and any
// ship the rules say it sank going down. It reads the report and never the map's present state for any fact: the
// shelling may be weeks old, so the holder is the report's defNat, the fort its 'Shore batteries' line (or look.fort),
// the shelter its 'Shelter' line, the town's damage its works and city fields, and the battleships' names the hull rows
// as they stood in the report's month. Only the coast's shape (the map's polygons, the province's terrain, whether it
// has a port and its chief place's name) is read live; the chief place's size (towns.ts townSize) is live too, and is
// said in staged.standIns.
//
// The outcome is fixed first, from the report alone: which ships fired, which the batteries sank or hit, how many of
// the shells fell on target. Then it is laid out. There are no dice: every look value is a hash of the scene's seed,
// the report's month and id and a label (`look(label)`, hash32 of "seed|turn|id|label"), so the same report gives the
// same bytes, another seed moves the looks and never a count. Nothing here is read by a rule, and nothing here writes
// to the game: it never changes a rule, a random draw, a save or the news. No number, odds or percent is put on the
// screen: the closing words give the place, the target, the date, the fleet and the ships lost, in words.
//
// Until the navy thread writes ShoreReport.look (plan section 2: fleetId, by, fired, split, fort, outcome, conditions,
// seed, salvos; written only while the 3D's scene record is on; built on navy-shore-guns 690599783, not joined here), the
// stager stands in from the report's own fields and the rows in shore-scene-looks.json, and says each stand-in in
// field.staged.standIns. A look's own gaps (no bore, no hour, no spotting with shoreGuns off) are stood in the same
// way and said. With the navy's rulings of
// 26 November 13:47Z: a gale or a closed sea stops the guns under shoreGuns (so a report with look never comes from a
// gale; without it a fleet shells in a gale as today, and the scene stages none); a battleship is never sunk, and with
// look it may be struck (hits and smoke, never sea:sunk); look.outcome.share is the rules' final share and sets only how
// many shells fall on target, as it is.
//
// The recording (R1.5): the envelope as sub-scene.ts writes it; `field` with size 30000, when{year, month, hour}, light,
// sky, sea, att, def, classes, wind, coast{...} and staged{scene: 'shore', beats[{what, from, to, screen, shots}], ...};
// pieces as the fleet recordings' with extra {cls, ships0, ships, fleet, name, mainMm, guns}, engaged -1 and no gun (so
// the show's own gunnery fires nothing); events sea:shore, sea:battery, sea:fire, sea:spot (new) and sea:gunHit,
// sea:sunk, sea:abandon, sea:men, sea:oil (as the show reads them). The names, as SeaFeed.cpp's shore reader takes them:
//   field.coast: bearing (degrees, the way the sea lies from the waterfront, clockwise from north), at {x, y} (the
//     waterfront, board metres), shore (land as closed rings of [x, y], board metres), form, relief, rise, kit, land,
//     ground, shelf, town? {name, size, look, port, x, y, r, quay? {x, y, face, len}, damage, skyline}, target {kind,
//     x, y, r, strips? [{x, y, len, wide, face, kind}]}, fort {level, fired, batteries [{x, y, face, mm}]}, air {spot,
//     nat, from}, whose
//   piece extra: mainMm, guns
//   sea:shore extra: ship, lands, mm, n, train, lay {on, sea, beach, over}, falls? [[x, y, kind]]
//   sea:battery extra: bat, x, y, mm, lands, on, ship, kind, falls [[x, y, kind]], dud?
//   sea:fire extra: x, y, r, kind, until, ship? (a ship's deck fire names her piece as the event's unit)
//   sea:spot extra: nat, x, y, r, alt, until
//   beats[].shots: [{at, kind, subject?, spot? [x, y]}]
// Bearings of a piece's face and a salvo's train are radians clockwise from north, as every recording's; the coast's
// and a battery's face, degrees. Falls' kinds: a salvo's 'on' (on target), 'sea' (short in the water), 'beach' (short
// on the beach), 'over'; a battery's 'short', 'over', 'straddle', 'hit'.

import LOOKS from './shore-scene-looks.json';
import FLEET_ROWS from '../../../data/campaign/fleets.json';
import SIEGE_ROWS from '../../../data/campaign/siege.json';
import HULL_ROWS from '../../../data/campaign/hulls.json';
import { turnToDate } from '../../../src/core/ticket';
import { fleetName, wholeShares } from '../../../src/campaign/navy';
import { hullAt, type HullRow } from '../../../src/campaign/hulls';
import { galeSeas } from '../../../src/campaign/heavyseas';
import { townName, townSite, townSize, type TownSize } from '../../../src/campaign/towns';
import { placeName, theWords } from '../../../src/campaign/game';
import { seaWhat } from '../../../src/campaign/sea-words';
import { latLonOf } from '../../../src/battle/air/sky';
import { factsOf, hisLoss, scenesOf, subSceneOf, type RouteRow, type SceneCase, type SceneView } from './sub-scene';
import type { Game, ShoreReport } from '../../../src/campaign/types';

// ---- The report's look (the navy thread's, mirrored) ---------------------------------------------------------

export type GunCls = 'bb' | 'ca' | 'dd';
export type ClassCount = Partial<Record<'bb' | 'cv' | 'ca' | 'dd' | 'ss', number>>;

/**
 * ShoreReport.look as the navy thread built it (navy-shore-guns 690599783, 26 November 21:48Z, src/campaign/types.ts
 * ShoreLook; not joined into this line), written only while the 3D turns its scene record on (shoreRecord below):
 * mirrored here, not imported, so this module builds on rules that do not have it yet. `fired` one entry a ship that
 * stood in, battleships first, only battleships, cruisers and destroyers (carriers and submarines stand in but carry no
 * shore guns); `name` only for a battleship the hull rows put in the fleet that month; `mm` and `guns` absent until the
 * historian's bores are on the hull rows (the looks file's rows stand in). `split` all 0. `outcome.sunk` only cruisers
 * and destroyers (a battleship is never sunk); `outcome.kept` the ships hit that damage control kept afloat (cruisers and
 * destroyers only: the rules' sinkShips fills it only for the classes the batteries' reply is laid on, fleets.ts bombard);
 * `outcome.share` the rules' final 0-1, no divisor (troops: the organisation lost, up to 0.345; a city: its output
 * stopped, at most 0.45; a town: the share of a full bombardment laid on its works); `outcome.struck` the readiness the
 * batteries cost the battleships (shoreGuns on only). `conditions`: `hour` never written; `sea` 'gale' only with
 * shoreGuns off; `spot` and `air` (the eyes in the air: the fleet's carriers, or the sky its side holds) only with
 * shoreGuns on; `sky` the air over the coast ('closed' under a closed sky, 'won' or 'held' where the fleet's side has the
 * air, else null). `seed` a hash (hashString of "shore:<seed>:<turn>:<fleet id>:<region>"), never a war die. Read
 * leniently: a field it may leave out is optional here too.
 */
export interface ShoreLook {
  fleetId: number;
  by: 'hand' | 'staff' | 'ai';
  fired: { cls: GunCls; name?: string | null; mm?: number; guns?: number }[];
  split?: ClassCount;
  fort: { level: number; fired: boolean };
  outcome: { sunk: ClassCount; kept: ClassCount; share: number; struck?: number };
  conditions: { hour?: number; sea: 'gale' | 'normal'; spot?: number; air?: 'carrier' | 'sky' | null; sky: 'held' | 'won' | 'closed' | null };
  seed: number;
  salvos?: number;
}
export type LookedReport = ShoreReport & { look?: ShoreLook };

/**
 * The 3D's scene record setting (TC.shoreSceneOn), outside the save, as navy.ts's subScene is for the submarine scene:
 * while it is on, the navy thread's bombard will write ShoreReport.look. Nothing reads it yet (the rules' side is not
 * built): the navy thread moves it beside its writer when it lands, and the bridge's TC.shoreSceneOn follows it there.
 */
export const shoreRecord = { on: false };

// ---- Data rows ------------------------------------------------------------------------------------------------

type Span = [number, number];
interface KitRow { kit: string; box?: [number, number, number, number]; lat?: Span; terrain?: string[]; land: string; town: string; shelf: number; rise?: Span; form: Form }
interface GunRow { from: string; mm: number; guns: number }
interface MixRow { from: string; mix: { cls: string; mm: number; guns: number; share: number }[] }
interface DdRow { from: string; cls: string; mm: number; guns: number }
type Form = 'straight' | 'bay' | 'headland' | 'harbour';

const L = LOOKS as unknown as {
  beats: { pace: number; least: number; most: number; pad: string[]; trim: string[]; fire0: number; fireEnd: number; after: Span; screens: Record<string, number> };
  bands: { bb: Span; ca: Span; dd: Span; clear: number; speed: number; spacing: Record<GunCls, number>; column: number };
  cadence: Record<GunCls, number>;
  flight: { rows: [number, number][] };
  pattern: { long: number; across: number };
  lay: { sea: number; beach: number; over: number };
  hits: { dd: number; ca: number; kept: Record<'ca' | 'dd', number>; struck: { hits: number; per: number }; gap: number; next: number };
  forts: Record<string, number[]> & { inland: Span; along: number; cadence: number; opening: number };
  troops: { len: Span; wide: Span; inland: Span; along: number };
  town: Record<TownSize, { r: number }> & { near: number; skyline: number; behind: Span; quay: Span };
  most: { bb: number; ca: number; dd: number; divs: number; fires: number; townFires: number; palls: number };
  ground: { palls: number };
  classes: Record<GunCls, { len: number; beam: number; speed: number; turn: number }>;
  crew: { ca: number; dd: number };
  hours: Span;
  sun: Span;
  seas: Record<string, Span>;
  skies: Record<string, { cover: number; kind: string }>;
  relief: Record<string, Span>;
  kits: { rows: KitRow[]; fallback: KitRow[] };
  guns: { navies: Record<string, { bb: GunRow[]; ca: MixRow[]; dd: DdRow[] }>; byName: Record<string, [number, number]> };
  caption: { troops: string[]; town: string[]; city: string[]; fort: string; struck: [string, string] };
};
const BOMBARD = (FLEET_ROWS as unknown as { bombard: { troops: { max: number }; city: { max: number } } }).bombard;
const BREACH = (SIEGE_ROWS as unknown as { breach: { shell: number; most: number } }).breach;
const HULLS = HULL_ROWS as unknown as { navies: Record<string, { bb?: (HullRow & { mm?: number; guns?: number })[] }> };

/** A date as months since January 1939, from 'YYYY-MM'. */
const monthsOf = (s: string): number => { const [y, m] = s.split('-').map(Number); return (y - 1939) * 12 + (m - 1); };
/** Months since January 1939 of a turn (turnToDate: turn 1 is September 1939). */
const monthsAt = (turn: number): number => { const d = turnToDate(turn); return (d.year - 1939) * 12 + d.month; };
/** The row of a navy's dated table that stands at a date: the latest 'from' not after it, else its first (as sub-scene.ts rowOf). */
function datedRow<T extends { from: string }>(rows: T[], at: number): T {
  let best: T | null = null;
  for (const r of rows) if (monthsOf(r.from) <= at && (!best || monthsOf(r.from) > monthsOf(best.from))) best = r;
  return best ?? [...rows].sort((a, b) => monthsOf(a.from) - monthsOf(b.from))[0];
}
const navyGuns = (nat: string): { bb: GunRow[]; ca: MixRow[]; dd: DdRow[] } => (Object.prototype.hasOwnProperty.call(L.guns.navies, nat) && !nat.startsWith('_') ? L.guns.navies[nat] : L.guns.navies._default);

/** The main guns of each navy's ships of the class in the war's middle years (SeaShow.cpp Show::Calibre, copied): the last stand-in. */
function calibre(cls: GunCls, nat: string): number {
  if (cls === 'bb') return nat === 'USA' ? 406 : nat === 'JAP' ? 410 : nat === 'GER' ? 380 : nat === 'UK' ? 381 : nat === 'ITA' ? 381 : nat === 'FRA' ? 380 : 356;
  if (cls === 'ca') return 203;
  return nat === 'USA' || nat === 'JAP' || nat === 'GER' ? 127 : 120;
}

/**
 * A ship's bore and main guns (plan R1.4): the rules' own (look.fired), then her hull row's mm, then the looks file (a
 * battleship by name, else her navy's dated row; a cruiser by her place in the navy's mix; a destroyer by its row),
 * then Show::Calibre. Each from 76 to 460 mm.
 */
export function boreOf(cls: GunCls, nat: string, turn: number, name: string | null, mixAt = 0): { mm: number; guns: number; cls: string; from: 'hull' | 'name' | 'row' | 'calibre' } {
  const at = monthsAt(turn);
  const rows = navyGuns(nat);
  const fix = (mm: number): number => Math.max(76, Math.min(460, Math.round(mm)));
  if (cls === 'bb') {
    const hull = name ? (HULLS.navies[nat]?.bb ?? []).find((r) => hullAt(r, turn).name === name || r.name === name) : undefined;
    if (hull?.mm && hull.guns) return { mm: fix(hull.mm), guns: hull.guns, cls: 'battleship', from: 'hull' };
    const by = name ? L.guns.byName[name] : undefined;
    if (by) return { mm: fix(by[0]), guns: by[1], cls: 'battleship', from: 'name' };
    if (rows.bb?.length) { const r = datedRow(rows.bb, at); return { mm: fix(r.mm), guns: r.guns, cls: 'battleship', from: 'row' }; }
  } else if (cls === 'ca' && rows.ca?.length) {
    const mix = datedRow(rows.ca, at).mix;
    const m = mix[Math.max(0, Math.min(mix.length - 1, mixAt))];
    return { mm: fix(m.mm), guns: m.guns, cls: m.cls, from: 'row' };
  } else if (cls === 'dd' && rows.dd?.length) {
    const r = datedRow(rows.dd, at);
    return { mm: fix(r.mm), guns: r.guns, cls: r.cls, from: 'row' };
  }
  return { mm: fix(calibre(cls, nat)), guns: cls === 'bb' ? 8 : cls === 'ca' ? 8 : 4, cls: cls === 'bb' ? 'battleship' : cls === 'ca' ? 'cruiser' : 'destroyer', from: 'calibre' };
}

/** A navy's cruiser mix at a month: each row's share of n cruisers, whole (the rows' order). */
export function cruiserMix(nat: string, turn: number, n: number): number[] {
  const rows = navyGuns(nat).ca;
  if (!rows?.length) return [n];
  return wholeShares(n, datedRow(rows, monthsAt(turn)).mix.map((m) => m.share));
}

// ---- The look's hashes (no dice) -----------------------------------------------------------------------------

/** A 32-bit FNV-1a hash with a murmur finaliser (copied from sub-scene.ts, whose bytes never move). */
function hash32(s: string): number {
  let h = 0x811c9dc5;
  for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 0x01000193) >>> 0; }
  h ^= h >>> 16; h = Math.imul(h, 0x85ebca6b) >>> 0; h ^= h >>> 13; h = Math.imul(h, 0xc2b2ae35) >>> 0; h ^= h >>> 16;
  return h >>> 0;
}
/** A look value in [0, 1) for a label: each label its own hash, so adding one never moves another (copied from sub-scene.ts). */
const looker = (key: string) => (label: string): number => hash32(`${key}|${label}`) / 4294967296;
type Look = (label: string) => number;
const pickIn = (r: Span, u: number): number => r[0] + (r[1] - r[0]) * u;
const clamp = (x: number, lo: number, hi: number): number => Math.max(lo, Math.min(hi, x));
const r1 = (x: number): number => Math.round(x * 10) / 10;
const r3 = (x: number): number => Math.round(x * 1000) / 1000;
const RAD = Math.PI / 180;
const TAU = Math.PI * 2;
const wrapDeg = (x: number): number => ((x % 360) + 360) % 360;
const wrap2Pi = (a: number): number => ((a % TAU) + TAU) % TAU;
/** A bearing in degrees to a tenth, 0 up to but not 360. */
const bearingDeg = (x: number): number => { const v = r1(wrapDeg(x)); return v >= 360 ? 0 : v; };

// ---- The sun (copied from sub-scene.ts's almanac, which is private there) ----------------------------------------

function julian(year: number, month0: number, day: number, hourUT: number): number {
  let y = year, m = month0 + 1;
  if (m <= 2) { y -= 1; m += 12; }
  const a = Math.floor(y / 100), b = 2 - a + Math.floor(a / 4);
  return Math.floor(365.25 * (y + 4716)) + Math.floor(30.6001 * (m + 1)) + day + b - 1524.5 + hourUT / 24;
}
function sunAt(jd: number): { lam: number; ra: number; dec: number; eqt: number } {
  const n = jd - 2451545.0;
  const Lm = wrapDeg(280.46 + 0.9856474 * n), g = wrapDeg(357.528 + 0.9856003 * n) * RAD;
  const lam = wrapDeg(Lm + 1.915 * Math.sin(g) + 0.02 * Math.sin(2 * g));
  const eps = (23.439 - 0.0000004 * n) * RAD;
  const ra = wrapDeg(Math.atan2(Math.cos(eps) * Math.sin(lam * RAD), Math.cos(lam * RAD)) / RAD);
  const dec = Math.asin(Math.sin(eps) * Math.sin(lam * RAD)) / RAD;
  let eqt = Lm - ra;
  if (eqt > 180) eqt -= 360;
  if (eqt < -180) eqt += 360;
  return { lam, ra, dec, eqt };
}
function upFrom(lat: number, ha: number, dec: number): { up: number; from: number } {
  const p = lat * RAD, h = ha * RAD, d = dec * RAD;
  const up = Math.asin(Math.sin(p) * Math.sin(d) + Math.cos(p) * Math.cos(d) * Math.cos(h)) / RAD;
  const from = wrapDeg(Math.atan2(-Math.cos(d) * Math.sin(h), Math.sin(d) * Math.cos(p) - Math.cos(d) * Math.cos(h) * Math.sin(p)) / RAD);
  return { up, from };
}
/** The sun and moon at a local solar hour of a date at a place (look only). */
export function skyAt(lat: number, lon: number, year: number, month0: number, day: number, hour: number): { sun: { up: number; from: number }; moon: { up: number; from: number; lit: number } } {
  const noon = sunAt(julian(year, month0, day, 12 - lon / 15));
  const solarNoon = 12 - noon.eqt / 15;
  const jd = julian(year, month0, day, hour - lon / 15);
  const s = sunAt(jd);
  const ha = (hour - solarNoon) * 15;
  const sun = upFrom(lat, ha, s.dec);
  const SYNODIC = 29.530588853;
  const age = (((jd - 2451550.26) % SYNODIC) + SYNODIC) % SYNODIC;
  const lit = (1 - Math.cos((2 * Math.PI * age) / SYNODIC)) / 2;
  const mlam = wrapDeg(s.lam + (360 * age) / SYNODIC) * RAD, eps = 23.439 * RAD;
  const mra = wrapDeg(Math.atan2(Math.cos(eps) * Math.sin(mlam), Math.cos(mlam)) / RAD);
  const mdec = Math.asin(Math.sin(eps) * Math.sin(mlam)) / RAD;
  return { sun, moon: { ...upFrom(lat, ha - (mra - s.ra), mdec), lit } };
}

// ---- The coast (coastOf) -------------------------------------------------------------------------------------

type P = [number, number];
type Ring = P[];
/** The map as the coast is read from it: the provinces' polygons and the seas' centres (a Game is one). */
export interface Chart { regions: { id: number; name: string; x: number; y: number; polys: [number, number][][][]; seas: number[]; terrain: string; port: boolean }[]; seas: { id: number; key: string; name: string; x: number; y: number }[] }

function inRing(x: number, y: number, ring: Ring): boolean {
  let inside = false;
  for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
    const [xi, yi] = ring[i], [xj, yj] = ring[j];
    if ((yi > y) !== (yj > y) && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) inside = !inside;
  }
  return inside;
}
/** Inside a province's polygons (as towns.ts's inside: its outer rings, not its holes). */
const insidePolys = (polys: Ring[][], x: number, y: number): boolean => polys.some((p) => p.length > 0 && inRing(x, y, p[0]) && !p.slice(1).some((hole) => inRing(x, y, hole)));
/** The distance from a point to a segment, and the nearest point on it. */
function toSegment(x: number, y: number, a: P, b: P): { d: number; p: P; t: number } {
  const dx = b[0] - a[0], dy = b[1] - a[1], len2 = dx * dx + dy * dy;
  const t = len2 > 0 ? clamp(((x - a[0]) * dx + (y - a[1]) * dy) / len2, 0, 1) : 0;
  const p: P = [a[0] + dx * t, a[1] + dy * t];
  return { d: Math.hypot(x - p[0], y - p[1]), p, t };
}

/** The map's land, indexed by cells: which provinces may hold a point. */
interface LandIndex { cell: number; cells: Map<string, number[]>; boxes: [number, number, number, number][] }
const INDEXES = new WeakMap<object, LandIndex>();
function indexOf(chart: Chart): LandIndex {
  const had = INDEXES.get(chart.regions);
  if (had) return had;
  const cell = 8;
  const cells = new Map<string, number[]>();
  const boxes = chart.regions.map((r) => {
    let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
    for (const poly of r.polys) for (const [x, y] of poly[0] ?? []) { x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y); }
    return [x0, y0, x1, y1] as [number, number, number, number];
  });
  boxes.forEach(([x0, y0, x1, y1], i) => {
    if (!Number.isFinite(x0)) return;
    for (let cx = Math.floor(x0 / cell); cx <= Math.floor(x1 / cell); cx++) for (let cy = Math.floor(y0 / cell); cy <= Math.floor(y1 / cell); cy++) {
      const k = `${cx},${cy}`;
      const list = cells.get(k);
      if (list) list.push(i); else cells.set(k, [i]);
    }
  });
  const out = { cell, cells, boxes };
  INDEXES.set(chart.regions, out);
  return out;
}
/** The province a map point lies in, or -1 for the sea. */
function provinceAt(chart: Chart, ix: LandIndex, x: number, y: number): number {
  for (const i of ix.cells.get(`${Math.floor(x / ix.cell)},${Math.floor(y / ix.cell)}`) ?? []) {
    const [x0, y0, x1, y1] = ix.boxes[i];
    if (x < x0 || x > x1 || y < y0 || y > y1) continue;
    if (insidePolys(chart.regions[i].polys, x, y)) return i;
  }
  return -1;
}

/** Metres a map unit is at a latitude, east and north (sky.ts latLonOf's projection: x = (lon + 11) cos 50 x 40, y = (71.4 - lat) x 40). */
const unitMetres = (lat: number): { kx: number; ky: number } => ({ kx: (111320 * Math.cos(lat * RAD)) / (40 * Math.cos(50 * RAD)), ky: 111320 / 40 });

/** The board: 30 km square (the 20 km default is too small for a battle line off a coast, SeaShow.h). */
export const SIZE = 30000;
/** How far the waterfront is laid landward of the board's middle, so the line to seaward and the land behind both fit. */
const SHIFT = 6000;
/** How far out free water is looked for, straight off the waterfront (m). */
const ROOM_MOST = 20000;
/** The board's land is kept this far past its edges (m). */
const MARGIN = 5000;

type Seg = [P, P];

export interface CoastPlan {
  region: number;
  zone: number;
  /**
   * How the waterfront was found: 'walk' (from the chief place toward the sea's centre), 'edge' (the province's own edge
   * nearest the sea's centre with water outside), 'near' (the province's own polygon has no edge on open water: the
   * first water past its neighbour's coast, on the walk toward the sea's centre or the nearest such coast to the chief
   * place), or 'form' (no map: a line by form).
   */
  how: 'walk' | 'edge' | 'near' | 'form';
  /** The waterfront on the map (map units), and where it is on the earth. */
  map: { x: number; y: number };
  lat: number;
  lon: number;
  /** The way the sea lies from the waterfront: degrees clockwise from north (the outward normal of its edge). */
  bearing: number;
  /** The waterfront on the board (m). */
  at: { x: number; y: number };
  /** The land on the board, as closed rings (m), its coasts roughened by hashed coves under 400 m. */
  land: Ring[];
  /** The coastline: the rings' edges with water on one side (not the borders between provinces, not the board's cut). */
  edges: Seg[];
  /** Free water straight out from the waterfront (m, at most 20 km). */
  room: number;
  /** The province's chief place: metres from the waterfront, and where it stands on the board. */
  townFrom: number;
  townAt: { x: number; y: number };
  /** Whether the waterfront lies on the province's own edge. */
  ownEdge: boolean;
  form: Form;
}

/** The unit vectors of a coast: û to seaward, t̂ along the shore (û turned a right angle clockwise). */
const unitsOf = (bearing: number): { ux: number; uy: number; tx: number; ty: number } => {
  const b = bearing * RAD;
  return { ux: Math.sin(b), uy: Math.cos(b), tx: Math.cos(b), ty: -Math.sin(b) };
};
type Box = [number, number, number, number];
const RING_BOXES = new WeakMap<Ring[], Box[]>();
/** Whether a board point is on the land (inside any ring; each ring's box tested first). */
export function onLand(land: Ring[], x: number, y: number): boolean {
  let boxes = RING_BOXES.get(land);
  if (!boxes) {
    boxes = land.map((r) => { let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity; for (const [a, b] of r) { x0 = Math.min(x0, a); y0 = Math.min(y0, b); x1 = Math.max(x1, a); y1 = Math.max(y1, b); } return [x0, y0, x1, y1] as Box; });
    RING_BOXES.set(land, boxes);
  }
  for (let i = 0; i < land.length; i++) { const b = boxes[i]; if (x >= b[0] && x <= b[2] && y >= b[1] && y <= b[3] && inRing(x, y, land[i])) return true; }
  return false;
}
/** The distance from a point on the board to the nearest stretch of coastline (m). */
function shoreDist(coast: { edges: Seg[] }, x: number, y: number): number {
  let best = Infinity;
  for (const [a, b] of coast.edges) {
    if (Math.abs(a[0] - x) > best && Math.abs(b[0] - x) > best && Math.sign(a[0] - x) === Math.sign(b[0] - x)) continue;
    if (Math.abs(a[1] - y) > best && Math.abs(b[1] - y) > best && Math.sign(a[1] - y) === Math.sign(b[1] - y)) continue;
    const d = toSegment(x, y, a, b).d;
    if (d < best) best = d;
  }
  return best;
}
const EDGE_CELL = 1000;
const EDGE_GRIDS = new WeakMap<Seg[], Map<string, Seg[]>>();
/** Whether any stretch of coastline lies within r metres of a point (the coastline's edges by 1 km cells). */
function nearShore(coast: { edges: Seg[] }, x: number, y: number, r: number): boolean {
  let grid = EDGE_GRIDS.get(coast.edges);
  if (!grid) {
    grid = new Map();
    for (const e of coast.edges) {
      const [a, b] = e;
      for (let cx = Math.floor(Math.min(a[0], b[0]) / EDGE_CELL); cx <= Math.floor(Math.max(a[0], b[0]) / EDGE_CELL); cx++) for (let cy = Math.floor(Math.min(a[1], b[1]) / EDGE_CELL); cy <= Math.floor(Math.max(a[1], b[1]) / EDGE_CELL); cy++) {
        const k = `${cx},${cy}`;
        const list = grid.get(k);
        if (list) list.push(e); else grid.set(k, [e]);
      }
    }
    EDGE_GRIDS.set(coast.edges, grid);
  }
  for (let cx = Math.floor((x - r) / EDGE_CELL); cx <= Math.floor((x + r) / EDGE_CELL); cx++) for (let cy = Math.floor((y - r) / EDGE_CELL); cy <= Math.floor((y + r) / EDGE_CELL); cy++) {
    for (const [a, b] of grid.get(`${cx},${cy}`) ?? []) if (toSegment(x, y, a, b).d < r) return true;
  }
  return false;
}
/** How far a point lies off the land: below 0 on land (how deep in), above it at sea. */
export const offLand = (coast: { land: Ring[]; edges: Seg[] }, x: number, y: number): number => (onLand(coast.land, x, y) ? -shoreDist(coast, x, y) : shoreDist(coast, x, y));
/**
 * The coastline of a board's land: each ring edge with land on one side and water on the other, probed 25 m either side
 * of its middle (so the borders between provinces, land on both sides, are left out), and none along the board's cut.
 */
function coastEdges(land: Ring[], lo: number, hi: number): Seg[] {
  const out: Seg[] = [];
  const onCut = (a: P, b: P): boolean => (a[0] === b[0] && (a[0] <= lo || a[0] >= hi)) || (a[1] === b[1] && (a[1] <= lo || a[1] >= hi));
  for (const r of land) for (let i = 0, j = r.length - 1; i < r.length; j = i++) {
    const a = r[j], b = r[i];
    const len = Math.hypot(b[0] - a[0], b[1] - a[1]);
    if (len < 1e-6 || onCut(a, b)) continue;
    const mx = (a[0] + b[0]) / 2, my = (a[1] + b[1]) / 2, nx = -(b[1] - a[1]) / len, ny = (b[0] - a[0]) / len;
    if (onLand(land, mx + nx * 25, my + ny * 25) !== onLand(land, mx - nx * 25, my - ny * 25)) out.push([a, b]);
  }
  return out;
}

/** A ring clipped to a box (Sutherland-Hodgman). */
function clipRing(ring: Ring, x0: number, y0: number, x1: number, y1: number): Ring {
  const edges: [(p: P) => boolean, (a: P, b: P) => P][] = [
    [(p) => p[0] >= x0, (a, b) => [x0, a[1] + ((b[1] - a[1]) * (x0 - a[0])) / (b[0] - a[0])]],
    [(p) => p[0] <= x1, (a, b) => [x1, a[1] + ((b[1] - a[1]) * (x1 - a[0])) / (b[0] - a[0])]],
    [(p) => p[1] >= y0, (a, b) => [a[0] + ((b[0] - a[0]) * (y0 - a[1])) / (b[1] - a[1]), y0]],
    [(p) => p[1] <= y1, (a, b) => [a[0] + ((b[0] - a[0]) * (y1 - a[1])) / (b[1] - a[1]), y1]],
  ];
  let out = ring;
  for (const [inside, cut] of edges) {
    const src = out;
    out = [];
    for (let i = 0; i < src.length; i++) {
      const a = src[(i + src.length - 1) % src.length], b = src[i];
      if (inside(b)) { if (!inside(a)) out.push(cut(a, b)); out.push(b); }
      else if (inside(a)) out.push(cut(a, b));
    }
    if (!out.length) break;
  }
  return out;
}

/**
 * Where a province meets one of its seas, and the land round it on a 30 km board (plan R1.3). The waterfront: a walk
 * from the province's chief place (towns.ts townSite) toward the sea's centre; the first point inside no province is
 * water, and the edge it crossed is the waterfront, its outward normal the way the sea lies. Where the walk enters
 * another province first, finds no water, or comes out on water too narrow for a battle line (under 4 km straight out),
 * the province's own edge nearest the sea's centre with water outside is taken instead [inferred; run on the map by the
 * tests]. Where the province's own polygon has no edge on open water at all (the map draws some coasts, as
 * Bessarabia's on the Black Sea, on a neighbour's polygon), the first water past the neighbour's coast is taken, on the
 * walk or, failing it, the nearest such coast to the chief place ('near', said in staged.standIns). The land is every
 * province's outer ring near it, in board metres (north up, map y running south), its edges roughened by hashed coves
 * under 400 m, the same coves for the same edge in every scene; the coastline is its edges with water on one side.
 */
export function coastOf(chart: Chart, region: number, zone: number): CoastPlan {
  const ix = indexOf(chart);
  const R = chart.regions[region];
  const Z = chart.seas[zone] ?? chart.seas[R.seas[0]];
  const site = townSite(region);
  const start: P = insidePolys(R.polys, site[0], site[1]) ? [site[0], site[1]] : [R.x, R.y];
  const [lat0] = latLonOf(start[0], start[1]);
  const { kx, ky } = unitMetres(lat0);
  const edgesOf = (i: number): Seg[] => {
    const out: Seg[] = [];
    for (const poly of chart.regions[i].polys) { const ring = poly[0] ?? []; for (let a = 0, b = ring.length - 1; a < ring.length; b = a++) out.push([ring[b] as P, ring[a] as P]); }
    return out;
  };
  const own = edgesOf(region);
  const water = (x: number, y: number): boolean => provinceAt(chart, ix, x, y) < 0;
  /** Free water from a map point along a bearing, metres (to ROOM_MOST). */
  const roomFrom = (p: P, bearing: number): number => {
    const { ux, uy } = unitsOf(bearing);
    for (let d = 250; d <= ROOM_MOST; d += 250) if (!water(p[0] + (ux * d) / kx, p[1] - (uy * d) / ky)) return d - 250;
    return ROOM_MOST;
  };
  /** The outward normal of an edge as a bearing, and whether it has water on one side only. */
  const normalOf = (a: P, b: P): { bearing: number; wet: boolean } => {
    const ex = (b[0] - a[0]) * kx, ey = -(b[1] - a[1]) * ky;
    const len = Math.hypot(ex, ey) || 1;
    const nx = ey / len, ny = -ex / len; // (one side; the other is its negative)
    const mx = (a[0] + b[0]) / 2, my = (a[1] + b[1]) / 2;
    const probe = (s: number): boolean => water(mx + (s * nx * 300) / kx, my - (s * ny * 300) / ky);
    const plus = probe(1), minus = probe(-1);
    const s = plus && !minus ? 1 : minus && !plus ? -1 : plus ? 1 : 0;
    return { bearing: wrapDeg(Math.atan2(s * nx, s * ny) / RAD), wet: s !== 0 && (plus !== minus) };
  };
  /** Where a step from land (`prev`, in province `at`) to water (`p`) crossed: the province's edge nearest the step, if it has water outside. */
  const crossing = (prev: P, p: P, at: number): { p: P; bearing: number } | null => {
    let best: { d: number; p: P; e: Seg } | null = null;
    for (const e of at === region ? own : edgesOf(at)) { const s = toSegment((prev[0] + p[0]) / 2, (prev[1] + p[1]) / 2, e[0], e[1]); if (!best || s.d < best.d) best = { d: s.d, p: s.p, e }; }
    if (!best || best.d >= 0.2) return null;
    const n = normalOf(best.e[0], best.e[1]);
    return n.wet ? { p: best.p, bearing: n.bearing } : null;
  };
  let how: CoastPlan['how'] = 'walk';
  let W: P | null = null, bearing = 0;
  /** The first water past a neighbour's coast on the walk (for a province with no edge of its own on open water). */
  let past: { p: P; bearing: number } | null = null;
  // The walk: from the chief place toward the sea's centre, a tenth of a map unit at a step.
  {
    const dx = Z.x - start[0], dy = Z.y - start[1], len = Math.hypot(dx, dy) || 1;
    const sx = (dx / len) * 0.1, sy = (dy / len) * 0.1;
    let prev: P = start, prevAt = region, left = false;
    for (let i = 1; i <= len / 0.1 + 1; i++) {
      const p: P = [start[0] + sx * i, start[1] + sy * i];
      const at = provinceAt(chart, ix, p[0], p[1]);
      if (at >= 0) { if (at !== region) left = true; prev = p; prevAt = at; continue; }
      // (water: the crossing, between the last point on land and this one, on the edge it crossed)
      const c = crossing(prev, p, prevAt);
      if (c && !left) { W = c.p; bearing = c.bearing; }
      else if (c) past = c;
      break;
    }
  }
  const zoneDist = (p: P): number => Math.hypot(p[0] - Z.x, p[1] - Z.y);
  if (!W || roomFrom(W, bearing) < 4000) {
    // The province's own edges with water outside, the nearest the sea's centre first; the first with 4 km of free water
    // straight out, else the one with the most.
    const wet = own.map((e) => ({ e, mid: [(e[0][0] + e[1][0]) / 2, (e[0][1] + e[1][1]) / 2] as P, n: normalOf(e[0], e[1]) }))
      .filter((c) => c.n.wet && Math.hypot((c.e[1][0] - c.e[0][0]) * kx, (c.e[1][1] - c.e[0][1]) * ky) > 200)
      .sort((a, b) => zoneDist(a.mid) - zoneDist(b.mid));
    let pick: { p: P; bearing: number; room: number } | null = W ? { p: W, bearing, room: roomFrom(W, bearing) } : null;
    for (const c of wet.slice(0, 40)) {
      const room = roomFrom(c.mid, c.n.bearing);
      if (!pick || (room >= 4000 && pick.room < 4000) || (pick.room < 4000 && room > pick.room)) pick = { p: c.mid, bearing: c.n.bearing, room };
      if (pick.room >= 4000 && pick.p === c.mid) break;
    }
    if (pick && (pick.p !== W)) { W = pick.p; bearing = pick.bearing; how = 'edge'; }
  }
  if (!W && past) { W = past.p; bearing = past.bearing; how = 'near'; }
  if (!W) {
    // (no water on the walk either: the nearest coast of any province to the chief place, within the province's box and
    // 2 map units round it, with water outside)
    const [x0, y0, x1, y1] = ix.boxes[region];
    let best: { d: number; p: P; bearing: number } | null = null;
    for (let i = 0; i < chart.regions.length; i++) {
      const b = ix.boxes[i];
      if (!Number.isFinite(b[0]) || b[2] < x0 - 2 || b[0] > x1 + 2 || b[3] < y0 - 2 || b[1] > y1 + 2) continue;
      for (const e of edgesOf(i)) {
        const s = toSegment(start[0], start[1], e[0], e[1]);
        if (best && s.d >= best.d) continue;
        const n = normalOf(e[0], e[1]);
        if (n.wet) best = { d: s.d, p: s.p, bearing: n.bearing };
      }
    }
    if (best) { W = best.p; bearing = best.bearing; how = 'near'; }
  }
  if (!W) { W = [R.x, R.y]; bearing = wrapDeg(Math.atan2((Z.x - R.x) * kx, -(Z.y - R.y) * ky) / RAD); how = 'near'; }
  const ownEdge = own.some((e) => toSegment(W![0], W![1], e[0], e[1]).d < 1e-6);
  const [lat, lon] = latLonOf(W[0], W[1]);
  const room = roomFrom(W, bearing);
  // The board: the waterfront SHIFT landward of the middle.
  const u = unitsOf(bearing);
  const at = { x: SIZE / 2 - u.ux * SHIFT, y: SIZE / 2 - u.uy * SHIFT };
  const toBoard = (p: P): P => [at.x + (p[0] - W![0]) * kx, at.y - (p[1] - W![1]) * ky];
  const lo = -MARGIN, hi = SIZE + MARGIN;
  const land: Ring[] = [];
  for (let i = 0; i < chart.regions.length; i++) {
    const [x0, y0, x1, y1] = ix.boxes[i];
    if (!Number.isFinite(x0)) continue;
    const a = toBoard([x0, y1]), b = toBoard([x1, y0]);
    if (b[0] < lo || a[0] > hi || b[1] < lo || a[1] > hi) continue;
    for (const poly of chart.regions[i].polys) {
      const ring = (poly[0] ?? []) as Ring;
      if (ring.length < 3) continue;
      // (the ring's own box on the board: a big ring with no corner near the board still crosses it)
      let rx0 = Infinity, ry0 = Infinity, rx1 = -Infinity, ry1 = -Infinity;
      for (const [x, y] of ring) { rx0 = Math.min(rx0, x); ry0 = Math.min(ry0, y); rx1 = Math.max(rx1, x); ry1 = Math.max(ry1, y); }
      const ra = toBoard([rx0, ry1]), rb = toBoard([rx1, ry0]);
      if (rb[0] < lo || ra[0] > hi || rb[1] < lo || ra[1] > hi) continue;
      const rough = roughen(ring, toBoard, at);
      const clipped = clipRing(rough, lo, lo, hi, hi);
      if (clipped.length >= 3) land.push(clipped.map(([x, y]) => [r1(x), r1(y)] as P));
    }
  }
  const edges = coastEdges(land, lo, hi);
  // The chief place, from the waterfront.
  const ts = toBoard(start);
  const townFrom = Math.hypot(ts[0] - at.x, ts[1] - at.y);
  // The coast's form round the waterfront: water on a 3 km circle about it (a headland stands in the sea, a bay is held by land).
  let wetShare = 0;
  for (let k = 0; k < 24; k++) { const a2 = (k / 24) * TAU; if (!onLand(land, at.x + Math.sin(a2) * 3000, at.y + Math.cos(a2) * 3000)) wetShare += 1 / 24; }
  const form: Form = R.port ? 'harbour' : wetShare > 0.62 ? 'headland' : wetShare < 0.38 ? 'bay' : 'straight';
  return { region, zone: Z.id, how, map: { x: r3(W[0]), y: r3(W[1]) }, lat: r3(lat), lon: r3(lon), bearing: bearingDeg(bearing), at: { x: r1(at.x), y: r1(at.y) }, land, edges, room, townFrom: Math.round(townFrom), townAt: { x: r1(ts[0]), y: r1(ts[1]) }, ownEdge, form };
}

/**
 * A ring in board metres with its edges roughened: each edge over 900 m is cut every 700 m or so and the cuts pushed
 * across it by a hash of the edge's own two ends (under 180 m either way: coves under 400 m), so the same edge is the
 * same in every ring that has it, and in every scene; none within 800 m of the waterfront moves, so it stays on its line.
 */
function roughen(ring: Ring, toBoard: (p: P) => P, keep: { x: number; y: number }): Ring {
  const out: Ring = [];
  for (let i = 0; i < ring.length; i++) {
    const A = ring[i], B = ring[(i + 1) % ring.length];
    const a = toBoard(A), b = toBoard(B);
    out.push(a);
    const len = Math.hypot(b[0] - a[0], b[1] - a[1]);
    if (len < 900) continue;
    // (the edge's own key, the same both ways round: its ends in order)
    const first = A[0] < B[0] || (A[0] === B[0] && A[1] <= B[1]);
    const [p0, p1] = first ? [a, b] : [b, a];
    const key = first ? `${A[0]},${A[1]}:${B[0]},${B[1]}` : `${B[0]},${B[1]}:${A[0]},${A[1]}`;
    const n = Math.floor(len / 700);
    const nx = -(p1[1] - p0[1]) / len, ny = (p1[0] - p0[0]) / len;
    const cuts: P[] = [];
    for (let k = 1; k < n; k++) {
      const t = k / n;
      const x = p0[0] + (p1[0] - p0[0]) * t, y = p0[1] + (p1[1] - p0[1]) * t;
      const push = Math.hypot(x - keep.x, y - keep.y) < 800 ? 0 : (hash32(`cove|${key}|${k}`) / 4294967296 - 0.5) * 360;
      cuts.push([x + nx * push, y + ny * push]);
    }
    out.push(...(first ? cuts : cuts.reverse()));
  }
  return out;
}

/**
 * A coast laid by form where there is no map ring for it (a stand-in: the far chart's places are not on this build's
 * map): a straight shore, a bay, a headland or a harbour with its mole, the sea to `bearing`. Said in staged.standIns.
 */
export function coastByForm(form: Form, bearing: number, lat: number, lon: number, island = false): CoastPlan {
  const u = unitsOf(bearing);
  const at = { x: SIZE / 2 - u.ux * SHIFT, y: SIZE / 2 - u.uy * SHIFT };
  // (in the coast's own frame: v along the sea's way (+ to seaward), s along the shore)
  const toBoard = (s: number, v: number): P => [r1(at.x + u.tx * s + u.ux * v), r1(at.y + u.ty * s + u.uy * v)];
  const pts: P[] = [];
  const W = 40000;
  for (let s = -W; s <= W; s += 1000) {
    let v = 0;
    if (form === 'bay') v = Math.abs(s) < 9000 ? -2500 * Math.cos((s / 9000) * (Math.PI / 2)) : 0;
    if (form === 'headland') v = Math.abs(s) < 5000 ? 2000 * Math.cos((s / 5000) * (Math.PI / 2)) : 0;
    // (the waterfront itself on the line, at s = 0, so the shape is shifted to pass it)
    pts.push([s, v]);
  }
  const v0 = pts.find(([s]) => s === 0)![1];
  let shore: P[] = pts.map(([s, v]) => [s, v - v0]);
  if (island) {
    // (an island: the shore curls round behind, about 6 km across)
    shore = [];
    for (let k = 0; k < 48; k++) { const a = (k / 48) * TAU; shore.push([Math.sin(a) * 4000, -4000 + Math.cos(a) * 4000]); }
  }
  const ring: Ring = island ? shore.map(([s, v]) => toBoard(s, v)) : [...shore.map(([s, v]) => toBoard(s, v)), toBoard(W, -W), toBoard(-W, -W)];
  const land: Ring[] = [clipRing(ring, -MARGIN, -MARGIN, SIZE + MARGIN, SIZE + MARGIN).map(([x, y]) => [r1(x), r1(y)] as P)];
  if (form === 'harbour') {
    // (the mole: a thin arm of land out from the shore, 600 m out, 1.5 km along the shore to one side)
    land.push([toBoard(900, 0), toBoard(900, 600), toBoard(2400, 600), toBoard(2400, 540), toBoard(960, 540), toBoard(960, -10)]);
  }
  return { region: -1, zone: -1, how: 'form', map: { x: 0, y: 0 }, lat, lon, bearing: bearingDeg(bearing), at: { x: r1(at.x), y: r1(at.y) }, land, edges: coastEdges(land, -MARGIN, SIZE + MARGIN), room: ROOM_MOST, townFrom: 1500, townAt: { x: r1(at.x - u.ux * 1500), y: r1(at.y - u.uy * 1500) }, ownEdge: true, form };
}

// ---- Laying the coast's pieces: the battle line, the batteries, the strips, the town -----------------------------

export interface Strip { x: number; y: number; len: number; wide: number; face: number; kind: 'open' | 'dug' | 'fort' }
export interface Site { x: number; y: number }

/** Whether a rectangle (centre, length along `face` degrees, width) lies on land, `inset` metres in from the shore at least. */
function rectOnLand(coast: CoastPlan, c: Site, len: number, wide: number, face: number, inset: number): boolean {
  const f = face * RAD, ax = Math.sin(f), ay = Math.cos(f), bx = Math.cos(f), by = -Math.sin(f);
  for (const [a, b] of [[0, 0], [0.5, 0.5], [0.5, -0.5], [-0.5, 0.5], [-0.5, -0.5], [0.5, 0], [-0.5, 0], [0, 0.5], [0, -0.5]] as P[]) {
    const x = c.x + ax * a * len + bx * b * wide, y = c.y + ay * a * len + by * b * wide;
    if (!onLand(coast.land, x, y) || nearShore(coast, x, y, inset)) return false;
  }
  return true;
}

/**
 * The battle line off the waterfront for a class: its distance off (d) and how far along the shore its middle is moved
 * (s), the track `half` metres either way of it keeping `clear` metres off any coastline and off the land. Tried in turn:
 * the band, from a hashed point of it outward, off the waterfront itself; the band moved along the shore, the nearest
 * first, up to 6 km; off the waterfront beyond the band to the free water's end, then nearer in; then any of those moved
 * up to 10 km. Null where none does: the caller lays it as far off as it can and says so.
 */
export function lineOff(coast: CoastPlan, band: Span, u0: number, half = 4000, clear = L.bands.clear): { d: number; s: number } | null {
  const u = unitsOf(coast.bearing);
  const ok = (d: number, s0: number): boolean => {
    const n = Math.ceil((2 * half) / 250);
    for (let k = 0; k <= n; k++) {
      const s = s0 - half + (2 * half * k) / n;
      const x = coast.at.x + u.ux * d + u.tx * s, y = coast.at.y + u.uy * d + u.ty * s;
      // (each sample point kept clear by half a step more, so every point between two samples keeps `clear`)
      if (x < 0 || y < 0 || x > SIZE || y > SIZE || onLand(coast.land, x, y) || nearShore(coast, x, y, clear + 126)) return false;
    }
    return true;
  };
  const inBand: number[] = [];
  const mid = Math.round(pickIn(band, u0) / 250) * 250;
  for (let k = 0; k <= 80; k++) for (const d of k === 0 ? [mid] : [mid + 250 * k, mid - 250 * k]) if (d >= band[0] && d <= band[1]) inBand.push(d);
  const outBand: number[] = [];
  for (let d = band[1] + 250; d <= Math.min(ROOM_MOST, SIZE / 2 + SHIFT - 1000); d += 250) outBand.push(d);
  for (let d = band[0] - 250; d >= clear; d -= 250) outBand.push(d);
  const along = (most: number): number[] => { const out: number[] = []; for (let s = 500; s <= most; s += 500) out.push(s, -s); return out; };
  for (const d of inBand) if (ok(d, 0)) return { d, s: 0 };
  for (const s0 of along(6000)) for (const d of inBand) if (ok(d, s0)) return { d, s: s0 };
  for (const d of outBand) if (ok(d, 0)) return { d, s: 0 };
  for (const s0 of along(10000)) for (const d of [...inBand, ...outBand]) if (Math.abs(s0) > 6000 || !inBand.includes(d) ? ok(d, s0) : false) return { d, s: s0 };
  return null;
}

/** Sites on land behind the waterfront: `n` of them, `inland` metres in, spread `along` metres either way, each one's rectangle on land. */
function sitesBehind(coast: CoastPlan, n: number, inland: Span, along: number, size: (k: number) => { len: number; wide: number }, look: Look, label: string, inset = 60): (Site & { len: number; wide: number; face: number })[] {
  const u = unitsOf(coast.bearing);
  const face = wrapDeg(coast.bearing + 90);
  const out: (Site & { len: number; wide: number; face: number })[] = [];
  for (let k = 0; k < n; k++) {
    const { len, wide } = size(k);
    const slot = n > 1 ? (k / (n - 1) - 0.5) * 2 * along : 0;
    let got: (Site & { len: number; wide: number; face: number }) | null = null;
    for (let t = 0; t < 60 && !got; t++) {
      // (first its own slot, a little jitter; then wider, deeper, and at last anywhere behind the shore)
      const spread = t < 12 ? 300 : t < 30 ? along * 0.6 : along * 1.4;
      const s = slot + (look(`${label}:${k}:${t}:s`) - 0.5) * 2 * spread;
      const d = pickIn(t < 30 ? inland : [inland[0], inland[1] * 2.5], look(`${label}:${k}:${t}:d`));
      const c = { x: coast.at.x - u.ux * d + u.tx * s, y: coast.at.y - u.uy * d + u.ty * s };
      if (rectOnLand(coast, c, len, wide, face, inset) && !out.some((o) => Math.hypot(o.x - c.x, o.y - c.y) < (Math.max(o.len, len) * 0.6))) got = { x: r1(c.x), y: r1(c.y), len, wide, face };
    }
    if (!got) {
      // (a coast too thin for it: the deepest land point behind the waterfront)
      let best: Site & { len: number; wide: number; face: number } = { x: coast.at.x - u.ux * inland[0], y: coast.at.y - u.uy * inland[0], len, wide, face };
      let deep = -Infinity;
      for (let d = 100; d <= 6000; d += 100) for (let s = -along; s <= along; s += 250) {
        const x = coast.at.x - u.ux * d + u.tx * s, y = coast.at.y - u.uy * d + u.ty * s;
        const o = -offLand(coast, x, y);
        if (o > deep) { deep = o; best = { x: r1(x), y: r1(y), len: Math.min(len, Math.max(50, o * 1.5)), wide: Math.min(wide, Math.max(30, o)), face }; }
      }
      got = best;
    }
    out.push(got);
  }
  return out;
}

export interface CoastLayout {
  /** Each class's battle line: metres off the waterfront, and its middle's move along the shore (null where no water was clear enough: laid as far off as the board allows). */
  lines: Record<GunCls, { d: number; s: number } | null>;
  batteries: (Site & { face: number })[];
  strips: (Site & { len: number; wide: number; face: number })[];
  town: (Site & { r: number }) | null;
}

/** The coast's pieces for a scene: the lines, the batteries (by bore), the troops' strips and the town (plan R1.3). */
export function layCoast(coast: CoastPlan, o: { classes: GunCls[]; halves?: Partial<Record<GunCls, number>>; bores: number[]; strips: number; town: { r: number; at: 'waterfront' | 'behind' | 'site' } | null }, look: Look): CoastLayout {
  const lines = { bb: null, ca: null, dd: null } as CoastLayout['lines'];
  for (const c of o.classes) lines[c] = lineOff(coast, L.bands[c], look(`line:${c}`), o.halves?.[c] ?? 4000);
  const bats = sitesBehind(coast, o.bores.length, L.forts.inland, L.forts.along, () => ({ len: 60, wide: 40 }), look, 'battery', 40).map((s) => ({ x: s.x, y: s.y, face: coast.bearing }));
  const strips = sitesBehind(coast, o.strips, L.troops.inland, L.troops.along, (k) => ({ len: Math.round(pickIn(L.troops.len, look(`strip:${k}:len`))), wide: Math.round(pickIn(L.troops.wide, look(`strip:${k}:wide`))) }), look, 'strip', 120);
  let town: (Site & { r: number }) | null = null;
  if (o.town) {
    const u = unitsOf(coast.bearing);
    const r = o.town.r;
    if (o.town.at === 'site') town = { x: coast.townAt.x, y: coast.townAt.y, r };
    else {
      const d0 = o.town.at === 'waterfront' ? r * 0.8 : pickIn(L.town.behind, look('town:behind')) + r;
      const s = sitesBehind(coast, 1, [d0, d0 + 400], 1500, () => ({ len: Math.min(r, 600), wide: Math.min(r, 600) }), look, `town:${o.town.at}`, 40)[0];
      town = { x: s.x, y: s.y, r };
    }
    if (!onLand(coast.land, town.x, town.y)) {
      const s = sitesBehind(coast, 1, [r, r + 2000], 2000, () => ({ len: 300, wide: 300 }), look, 'town:again', 40)[0];
      town = { x: s.x, y: s.y, r };
    }
  }
  return { lines, batteries: bats, strips, town };
}

// ---- The facts a scene is staged from --------------------------------------------------------------------------

/** What the stager reads: the report, and what it needs of the map (the coast, the chief place, the terrain). */
export interface ShoreFacts {
  /** The player's nation, and the campaign's seed (only ever hashed). */
  human: string;
  seed: number;
  rep: LookedReport;
  names: Record<string, { name: string; adj: string }>;
  /** The sea the fleet stood in from: its key and name. */
  sea: { key: string; name: string };
  coast: CoastPlan;
  /** The province's chief place (towns.ts: its name and port live; its size live, a stand-in for the size as it stood). */
  town: { name: string; size: TownSize; port: boolean };
  terrain: string;
  /** The fleet's battleships' names as the hull rows stand at the report's month (afloat then), for his own scenes. */
  hulls: string[];
  /** Whether a gale blew over the sea that month (heavyseas.ts galeSeas: a hash of the seed and the month): stands in for look.conditions.sea. */
  gale: boolean;
  /** What the facts themselves stood in for (live reads). */
  standIns: string[];
}

/** The fleet's battleships afloat in the report's month, by the hull rows as they stood then (hulls.ts hullAt; a ship named lost after it was afloat then). */
function hullsAt(g: Game, nat: string, fleet: string, turn: number): string[] {
  const rows = HULLS.navies[nat]?.bb ?? [];
  const out: string[] = [];
  rows.forEach((r, i) => {
    const lost = g.flags[`hull:${nat}:bb:${i}`];
    if ((r.from ?? 1) > turn || (typeof lost === 'number' && lost < turn)) return;
    const at = hullAt(r, turn);
    if (at.fleet === fleet) out.push(at.name);
  });
  return out;
}

/** The facts of a report from the game: the coast from the map, the rest from the report. It reads the game and writes nothing. */
export function shoreFactsOf(g: Game, rep: LookedReport): ShoreFacts {
  const r = g.regions[rep.region];
  const zone = rep.zone >= 0 && g.seas[rep.zone] ? rep.zone : r.seas[0];
  const names: Record<string, { name: string; adj: string }> = {};
  for (const n of Object.values(g.nations)) names[n.id] = { name: n.name, adj: n.adj };
  const standIns = ["the chief place's size: as it stands now (towns.ts townSize), not as it stood that month"];
  let gale = false;
  if (!rep.look) {
    gale = galeSeas(g, rep.turn).has(zone);
  }
  return {
    human: g.human ?? '', seed: g.seed, rep, names, sea: { key: g.seas[zone].key, name: g.seas[zone].name },
    coast: coastOf(g as unknown as Chart, rep.region, zone), town: { name: townName(r), size: townSize(g, r), port: r.port }, terrain: r.terrain,
    hulls: hullsAt(g, rep.nat, rep.fleet, rep.turn), gale, standIns,
  };
}

// ---- Reading the report ----------------------------------------------------------------------------------------

const CLASS_ONE: Record<string, string> = { bb: 'battleship', cv: 'carrier', ca: 'cruiser', dd: 'destroyer', ss: 'submarine' };
const CLASS_MANY: Record<string, string> = { bb: 'battleships', cv: 'carriers', ca: 'cruisers', dd: 'destroyers', ss: 'submarines' };
const ONES = ['no', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine', 'ten', 'eleven', 'twelve', 'thirteen', 'fourteen', 'fifteen', 'sixteen', 'seventeen', 'eighteen', 'nineteen'];
const TENS = ['', '', 'twenty', 'thirty', 'forty', 'fifty', 'sixty', 'seventy', 'eighty', 'ninety'];
/** A count in words (the closing words carry no digit but the date's and the fleet's name). */
export function words(n: number): string {
  const k = Math.max(0, Math.round(n));
  if (k < 20) return ONES[k];
  if (k < 100) return `${TENS[Math.floor(k / 10)]}${k % 10 ? `-${ONES[k % 10]}` : ''}`;
  if (k < 1000) return `${ONES[Math.floor(k / 100)]} hundred${k % 100 ? ` and ${words(k % 100)}` : ''}`;
  return 'many';
}
const MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
const upFirst = (s: string): string => (s ? s[0].toUpperCase() + s.slice(1) : s);
const andList = (parts: string[]): string => (parts.length <= 1 ? parts.join('') : `${parts.slice(0, -1).join(', ')} and ${parts[parts.length - 1]}`);
/** Ships by class in words, the heaviest first: "a cruiser and two destroyers". */
function shipWords(c: ClassCount): string {
  return andList((['bb', 'cv', 'ca', 'dd', 'ss'] as const).filter((k) => (c[k] ?? 0) > 0).map((k) => (c[k] === 1 ? `a ${CLASS_ONE[k]}` : `${words(c[k]!)} ${CLASS_MANY[k]}`)));
}
const countOf = (c: ClassCount): number => (c.bb ?? 0) + (c.cv ?? 0) + (c.ca ?? 0) + (c.dd ?? 0) + (c.ss ?? 0);

/** The report's line by its label (the forecast's lines, kept in the report: fleets.ts planBombard). */
const lineOf = (rep: ShoreReport, label: string): string | undefined => rep.lines.find((l) => l.label === label)?.value;
/** The fort's level: look.fort, else the report's 'Shore batteries' line ('Fortress level N'), else none. */
export function fortOf(rep: LookedReport): { level: number; fired: boolean; from: 'look' | 'line' | 'none' } {
  if (rep.look) return { level: Math.max(0, Math.round(rep.look.fort.level)), fired: rep.look.fort.level > 0 && rep.look.fort.fired, from: 'look' };
  const m = /Fortress level (\d+)/.exec(lineOf(rep, 'Shore batteries') ?? '');
  const level = m ? Number(m[1]) : 0;
  // (under today's rules a fort above level 0 always answers: fleets.ts reply > 0, and its sinkShips draws)
  return { level, fired: level > 0, from: m ? 'line' : 'none' };
}
/** The troops' shelter: the report's 'Shelter' line. */
export function shelterOf(rep: ShoreReport): 'open' | 'dug' | 'fort' {
  const v = lineOf(rep, 'Shelter') ?? '';
  return /concrete|[Ff]ortress/.test(v) ? 'fort' : /dug in/.test(v) ? 'dug' : 'open';
}
/** The ships the repair parties kept afloat, read from the report's words (fleets.ts keptAfloat's past form): no count is kept but these words. */
export function keptFromWords(result: string): ClassCount {
  const m = /(?:^|\. )([^.]*?) (?:was|were) badly hit, but the repair parties kept the ships? afloat\./.exec(result);
  if (!m) return {};
  const out: ClassCount = {};
  const COUNT: Record<string, number> = { a: 1, an: 1, one: 1, two: 2, three: 3, four: 4, five: 5, six: 6, seven: 7, eight: 8, nine: 9, ten: 10 };
  const text = m[1].replace(/ of (?:the |your )?[A-Z][^,]*$/, '');
  for (const part of text.split(/, | and /)) {
    const p = /^(\w+)(?: of (?:the fleet's|its|your))? (battleships?|carriers?|cruisers?|destroyers?|submarines?)/i.exec(part.trim());
    if (!p) continue;
    const n = COUNT[p[1].toLowerCase()] ?? (Number(p[1]) || 1);
    const cls = (Object.keys(CLASS_ONE).find((k) => p[2].toLowerCase().startsWith(CLASS_ONE[k])) ?? 'ca') as keyof ClassCount;
    out[cls] = (out[cls] ?? 0) + n;
  }
  return out;
}

/** The share s the shelling did, 0 to 1, from the report's own figures (plan R1.2; the looks file's share rows): for the closing words' bands. */
export function bandShare(rep: ShoreReport): number {
  if (rep.what === 'troops') return clamp(rep.cut / BOMBARD.troops.max, 0, 1);
  if (rep.what === 'city') return clamp(rep.output / BOMBARD.city.max, 0, 1);
  return clamp((rep.works ?? 0) / BREACH.shell, 0, 1);
}

/** What fired: look.fired by class, else the ships that stood in, battleships, cruisers and destroyers only, less look.split. */
export function firedOf(rep: LookedReport): Record<GunCls, number> {
  if (rep.look?.fired?.length) {
    const out = { bb: 0, ca: 0, dd: 0 };
    for (const f of rep.look.fired) if (f.cls === 'bb' || f.cls === 'ca' || f.cls === 'dd') out[f.cls]++;
    return out;
  }
  const split = rep.look?.split ?? {};
  return { bb: Math.max(0, (rep.ships.bb ?? 0) - (split.bb ?? 0)), ca: Math.max(0, (rep.ships.ca ?? 0) - (split.ca ?? 0)), dd: Math.max(0, (rep.ships.dd ?? 0) - (split.dd ?? 0)) };
}

/** Why a report gets no scene before R6: night (the sun not up at its hour, or no daylight hour the sun stands 12 degrees up), a gale, or nothing. */
export function barOf(f: ShoreFacts): 'night' | 'gale' | null {
  const c = f.rep.look?.conditions;
  if (c?.sea === 'gale' || (!c && f.gale)) return 'gale';
  return hourOf(f).night ? 'night' : null;
}

/** The scene's local hour: look.conditions.hour, else a daylight hour 7-17 with the sun to seaward and 12-70 degrees up (the scene's clamp), by hash. */
export function hourOf(f: ShoreFacts): { hour: number; night: boolean; day: number; sun: { up: number; from: number } } {
  const { year, month: month0 } = turnToDate(f.rep.turn);
  const look = lookOf(f);
  const day = 1 + Math.floor(look('day') * 28);
  const lat = f.coast.lat, lon = f.coast.lon;
  const given = f.rep.look?.conditions.hour;
  if (typeof given === 'number') {
    const h = wrapDeg(given * 15) / 15;
    const s = skyAt(lat, lon, year, month0, day, h).sun;
    return { hour: h, night: s.up < L.sun[0], day, sun: s };
  }
  const ok: { hour: number; sun: { up: number; from: number }; sea: boolean }[] = [];
  for (let h = L.hours[0]; h <= L.hours[1]; h += 0.5) {
    const s = skyAt(lat, lon, year, month0, day, h).sun;
    if (s.up < L.sun[0] || s.up > L.sun[1]) continue;
    const off = Math.abs(((s.from - f.coast.bearing + 540) % 360) - 180);
    ok.push({ hour: h, sun: s, sea: off <= 90 });
  }
  const pool = ok.some((x) => x.sea) ? ok.filter((x) => x.sea) : ok;
  if (!pool.length) {
    const s = skyAt(lat, lon, year, month0, day, 12).sun;
    return { hour: 12, night: true, day, sun: s };
  }
  const p = pool[Math.min(pool.length - 1, Math.floor(look('hour') * pool.length))];
  return { hour: p.hour, night: false, day, sun: p.sun };
}

/** The scene's seed: look.seed, else hash32 of "<g.seed>:<turn>:shore:<report id>" (as sub-scene's recordings'). */
export const seedOf = (f: ShoreFacts): number => (typeof f.rep.look?.seed === 'number' ? f.rep.look.seed >>> 0 : hash32(`${f.seed}:${f.rep.turn}:shore:${f.rep.id}`));
/** Every look value: a hash of "seed|turn|report id|label". */
const lookOf = (f: ShoreFacts): Look => looker(`${seedOf(f)}|${f.rep.turn}|${f.rep.id}`);

/** The kit for a place: the first row whose box holds it, else the fallbacks by latitude and terrain. */
export function kitOf(lat: number, lon: number, terrain: string): KitRow {
  const inBox = (b: [number, number, number, number]): boolean => lat >= b[0] && lat <= b[1] && (b[2] <= b[3] ? lon >= b[2] && lon <= b[3] : lon >= b[2] || lon <= b[3]);
  for (const r of L.kits.rows) if (r.box && inBox(r.box) && (!r.terrain || r.terrain.includes(terrain))) return r;
  for (const r of L.kits.fallback) if (r.lat && lat >= r.lat[0] && lat <= r.lat[1] && (!r.terrain || r.terrain.includes(terrain))) return r;
  return L.kits.fallback[L.kits.fallback.length - 1];
}
const reliefOf = (terrain: string): string => (terrain === 'mountain' ? 'mountains' : terrain === 'hills' ? 'hills' : terrain === 'forest' ? 'rolling' : 'flat');
/** A shell's time of flight (s) by bore and range (m). */
export const flightOf = (mm: number, range: number): number => (L.flight.rows.find(([upTo]) => mm <= upTo) ?? L.flight.rows[L.flight.rows.length - 1])[1] * (range / 1000);

// ---- The closing words (the UI director's marks of 26 November 13:42Z) ----------------------------------------------

/**
 * The caption: line 1 what was shelled, by whom, from where and when, and the ships lost ("for the loss of"); line 2 the
 * result in bands of the share; line 3 the batteries, only where there is a fort. The fleet as its name gives it in his
 * own scenes, with its nation's adjective in a scene of his coast; no ship is named; never "you" or "your".
 */
export function captionOf(f: ShoreFacts, me: 'att' | 'def'): string[] {
  const rep = f.rep;
  const adj = (n: string): string => f.names[n]?.adj ?? n;
  const { year, month: month0 } = turnToDate(rep.turn);
  const when = `${MONTHS[month0]} ${year}`;
  const fleet = me === 'att' ? fleetName({ name: rep.fleet }) : `the ${adj(rep.nat)} ${rep.fleet}`;
  const what = rep.what === 'troops' ? `The ${adj(rep.defNat)} troops in ${placeName(rep.place)}` : rep.what === 'town' ? `The defences of ${f.town.name}` : `The factories and docks of ${placeName(rep.place)}`;
  const sunk = sunkOf(rep);
  const loss = countOf(sunk) ? `, for the loss of ${shipWords(sunk)}` : '';
  const lines = [theWords(`${what} shelled by ${fleet} from ${seaWhat(f.sea.name)}, ${when}${loss}.`)];
  const s = bandShare(rep);
  const band = (x: number): number => (x < 1 / 3 ? 1 : x < 2 / 3 ? 2 : 3);
  if (rep.what === 'troops') lines.push(L.caption.troops[rep.cut <= 0 ? 0 : band(s)]);
  else if (rep.what === 'city') lines.push(L.caption.city[rep.output <= 0 ? 0 : band(s)]);
  else lines.push(L.caption.town[/already broken down as far as/.test(rep.result) ? 4 : band(s)]);
  const fort = fortOf(rep);
  if (fort.level > 0 && fort.fired) {
    // (the ships kept afloat, as keptAfloat's past form; the battleships struck, in the rules' own words, never sunk)
    const kept = keptOf(rep);
    const n = countOf(kept);
    const said: string[] = [];
    if (n) said.push(`${upFirst(shipWords(kept))} ${n === 1 ? 'was' : 'were'} badly hit, but the repair parties kept the ${n === 1 ? 'ship' : 'ships'} afloat.`);
    const bb = firedOf(rep).bb;
    if (struckOf(rep) > 0 && bb > 0) said.push(L.caption.struck[bb === 1 ? 0 : 1]);
    if (said.length) lines.push(said.join(' '));
    else if (!countOf(sunk)) lines.push(L.caption.fort);
  }
  return lines;
}
/** The ships the batteries sank: cruisers and destroyers only (a battleship is never sunk: the navy's ruling 3). */
export function sunkOf(rep: LookedReport): ClassCount {
  const s = rep.look?.outcome.sunk ?? rep.sunk;
  return { ca: Math.max(0, s.ca ?? 0), dd: Math.max(0, s.dd ?? 0) };
}
/**
 * The ships hit and kept afloat: look.outcome.kept, else the report's words; cruisers and destroyers only, as the rules
 * keep them (fleets.ts bombard lays the batteries' reply on ca and dd alone, and sinkShips fills the list it is given for
 * those classes). A battleship hit is the look's outcome.struck (struckOf), never a ship kept afloat.
 */
export function keptOf(rep: LookedReport): ClassCount {
  const k = rep.look ? rep.look.outcome.kept : keptFromWords(rep.result);
  return { ca: Math.max(0, k.ca ?? 0), dd: Math.max(0, k.dd ?? 0) };
}
/** The readiness the batteries' fire on the battleships cost the fleet (look.outcome.struck: shoreGuns on only), else 0. */
export const struckOf = (rep: LookedReport): number => { const x = rep.look?.outcome.struck; return typeof x === 'number' && Number.isFinite(x) && x > 0 ? x : 0; };
/** The battleships shown hit (hits and smoke, never sunk: the navy's ruling 3): one for each `hits.struck.per` of struck, at least one, at most those drawn. */
export const struckShips = (rep: LookedReport, drawnBb: number): number => (struckOf(rep) > 0 ? Math.min(drawnBb, Math.max(1, Math.ceil(struckOf(rep) / L.hits.struck.per - 1e-9))) : 0);

// ---- The staged recording ------------------------------------------------------------------------------------

export interface Shot { at: number; kind: ShotKind; subject?: number; spot?: [number, number] }
export type ShotKind = 'SeaLine' | 'Broadside' | 'BehindTurret' | 'HillEye' | 'SpotterEye' | 'FortEye' | 'ShoreGlass' | 'Hit' | 'Sinking' | 'SeaWide';
export interface ShoreBeat { what: string; from: number; to: number; screen: number; shots: Shot[] }

export interface Staged {
  what: string; recording: true; key: string; seed: number; step: number;
  field: Record<string, unknown>; views: Record<string, unknown>[]; after: { lines: string[]; level: string; winner: string; seconds: number };
}

/** What the stager drew and from which facts: `field.staged`. */
export interface ShoreNote {
  scene: 'shore';
  beats: ShoreBeat[];
  seconds: number;
  caption: { lines: string[] };
  facts: Record<string, unknown>;
  drawn: {
    fired: Record<GunCls, number>; bb: number; ca: number; dd: number; pieces: number; divisions: number;
    sunk: { ca: number; dd: number }; kept: { ca: number; dd: number }; struck: number;
    salvos: number; shells: number; on: number; sea: number; beach: number; over: number; points: number;
    batteryRounds: number; batteryHits: number; strips: number; fires: number; palls: number;
  };
  standIns: string[];
}

/** The views' step (s), as the fleet recordings'. */
const STEP = 2;

interface Piece {
  id: number; cls: GunCls; name: string; slots: number; mm: number; guns: number; mixCls: string;
  d: number; dir: number; along0: number;
  /** Each slot's sinking moment, -1 afloat. */
  sunkAt: number[];
}
interface SceneEvent { t: number; what: string; unit?: number; side?: 'att' | 'def'; x?: number; y?: number; cue?: string; text: string; extra?: Record<string, unknown> }

/** Stages a report (stageShore's work: the outcome fixed first, then laid out). Null where it gets no scene (night, a gale). */
export function stageShore(f: ShoreFacts, meIn?: 'att' | 'def'): Staged | null {
  if (barOf(f)) return null;
  const rep = f.rep;
  const look = lookOf(f);
  const me: 'att' | 'def' = meIn ?? (rep.nat === f.human ? 'att' : rep.defNat === f.human ? 'def' : 'att');
  const { year, month: month0 } = turnToDate(rep.turn);
  const standIns = [...f.standIns];
  const coast = f.coast;
  if (coast.how === 'form') standIns.push(`the shore: a line by form (${coast.form}), no map ring for this place`);
  const u = unitsOf(coast.bearing);
  const B = L.beats;

  // ---- The outcome, fixed first ------------------------------------------------------------------------------
  const fired = firedOf(rep);
  if (!rep.look) standIns.push('the ships that fired: those that stood in, battleships, cruisers and destroyers (no look.fired)');
  const sunk = sunkOf(rep);
  const kept = keptOf(rep);
  if (!rep.look) standIns.push("the ships kept afloat: read from the result's words (no count kept)");
  const fort = fortOf(rep);
  if (fort.from === 'line') standIns.push("the fort: the report's 'Shore batteries' line (no look.fort)");
  const s = bandShare(rep);
  const shareOn = rep.look ? clamp(rep.look.outcome.share, 0, 1) : s;
  if (!rep.look) standIns.push("the share on target: the report's own figures over the rules' most, clamped 0-1 (no look.outcome.share)");
  const shelter = rep.what === 'troops' ? shelterOf(rep) : 'open';
  const cv = rep.ships.cv ?? 0;
  // (spotting aircraft only where the look says eyes in the air spotted: the fleet's carriers' aircraft, the carrier
  // unseen, or, under a sky its side holds, the ships' own floatplanes [inferred]; the rules write spot and air only with
  // shoreGuns on)
  const air = rep.look?.conditions.air ?? null;
  const spotBy: 'carrier' | 'floatplane' | 'none' = air === 'carrier' && cv > 0 ? 'carrier' : air === 'carrier' || air === 'sky' ? 'floatplane' : 'none';
  const spot = spotBy !== 'none';
  if (typeof rep.look?.conditions.spot !== 'number') standIns.push('spotting: none (the report does not say: no look, or shoreGuns off)');
  const hour = hourOf(f);
  if (typeof rep.look?.conditions.hour !== 'number') standIns.push('the hour: a daylight hour with the sun to seaward, by hash (the rules write none)');

  // The cast: each battleship a piece, each cruiser a piece, destroyers in divisions of four; the heaviest class leads.
  const drawnBb = Math.min(fired.bb, L.most.bb), drawnCa = Math.min(fired.ca, L.most.ca);
  const ddPieces = Math.min(Math.ceil(fired.dd / 4), L.most.dd), drawnDd = Math.min(fired.dd, ddPieces * 4);
  const ddSlots = ddPieces ? wholeShares(drawnDd, Array.from({ length: ddPieces }, () => 1)) : [];
  const classes: GunCls[] = (['bb', 'ca', 'dd'] as GunCls[]).filter((c) => (c === 'bb' ? drawnBb : c === 'ca' ? drawnCa : ddPieces) > 0);
  if (!classes.length) return null;
  // (the bores: the rules' own first; a cruiser's by her place in her navy's mix, by hash rank)
  const firedRows = rep.look?.fired ?? [];
  const caMix = cruiserMix(rep.nat, rep.turn, drawnCa);
  const caOrder = Array.from({ length: drawnCa }, (_, i) => i).sort((a, b) => look(`ca:${a}`) - look(`ca:${b}`));
  const caMixAt: number[] = new Array(drawnCa).fill(0);
  { let k = 0; caMix.forEach((n, m) => { for (let j = 0; j < n; j++) caMixAt[caOrder[k++]] = m; }); }
  const names = me === 'att' ? (firedRows.length ? firedRows.filter((x) => x.cls === 'bb').map((x) => x.name ?? '') : f.hulls) : [];
  if (me === 'att' && !firedRows.length && drawnBb > 0) standIns.push("the battleships' names: the hull rows of the fleet as they stood that month (the report names none)");
  const pieces: Piece[] = [];
  let nextId = 1;
  const flightRow = (cls: GunCls, k: number): ShoreLook['fired'][number] | undefined => firedRows.filter((x) => x.cls === cls)[k];
  /** A ship's bore and guns: the look's own where it gives them (held to 76-460 mm), else the stand-in rows (boreOf). */
  const boreFrom = new Set<string>();
  const boreFor = (cls: GunCls, k: number, name: string | null, mixAt = 0): { mm: number; guns: number; cls: string } => {
    const given = flightRow(cls, k);
    const rows = boreOf(cls, rep.nat, rep.turn, name, mixAt);
    const mm = typeof given?.mm === 'number' && Number.isFinite(given.mm) ? clamp(Math.round(given.mm), 76, 460) : null;
    const guns = typeof given?.guns === 'number' && Number.isFinite(given.guns) && given.guns >= 1 ? Math.round(given.guns) : null;
    if (mm === null || guns === null) boreFrom.add(`${cls}:${rows.from}`);
    return { mm: mm ?? rows.mm, guns: guns ?? rows.guns, cls: mm !== null ? (cls === 'bb' ? 'battleship' : cls === 'ca' ? 'cruiser' : 'destroyer') : rows.cls };
  };
  for (let k = 0; k < drawnBb; k++) {
    const name = names[k] || null;
    const b = boreFor('bb', k, name);
    pieces.push({ id: nextId++, cls: 'bb', name: name ?? `Battleship ${k + 1}`, slots: 1, mm: b.mm, guns: b.guns, mixCls: b.cls, d: 0, dir: 1, along0: 0, sunkAt: [-1] });
  }
  for (let k = 0; k < drawnCa; k++) {
    const b = boreFor('ca', k, null, caMixAt[k]);
    pieces.push({ id: nextId++, cls: 'ca', name: `Cruiser ${k + 1}`, slots: 1, mm: b.mm, guns: b.guns, mixCls: b.cls, d: 0, dir: 1, along0: 0, sunkAt: [-1] });
  }
  for (let k = 0; k < ddPieces; k++) {
    const b = boreFor('dd', k, null);
    pieces.push({ id: nextId++, cls: 'dd', name: `Destroyers ${k + 1}`, slots: ddSlots[k], mm: b.mm, guns: b.guns, mixCls: b.cls, d: 0, dir: 1, along0: 0, sunkAt: new Array(ddSlots[k]).fill(-1) });
  }
  // (the bores stood in, said: the rules give none until the historian's bores are on the hull rows)
  if (boreFrom.has('bb:name')) standIns.push("a battleship's bore: the looks file's row by her name (the hull rows carry no bore yet)");
  if (boreFrom.has('bb:row') || boreFrom.has('bb:calibre')) standIns.push("a battleship's bore: her navy's row (no name to read it by)");
  if ([...boreFrom].some((x) => x.startsWith('ca:') || x.startsWith('dd:'))) standIns.push("the cruisers' and destroyers' bores: their navy's dated rows, a cruiser's by her place in the mix by hash rank (the report gives none)");
  if (fired.bb > drawnBb || fired.ca > drawnCa || fired.dd > drawnDd) standIns.push('the rest of the fleet: out of frame (the most drawn: six battleships, eight cruisers, six divisions of destroyers)');

  // The target and the coast's pieces.
  const kit = kitOf(coast.lat, coast.lon, f.terrain);
  const divsDrawn = rep.what === 'troops' ? Math.min(Math.max(rep.divs, 1), L.most.divs) : 0;
  const townAt: 'waterfront' | 'behind' | 'site' | null = rep.what === 'troops' ? (coast.townFrom <= L.town.near ? 'site' : null)
    : f.town.port ? 'waterfront' : 'behind';
  const townR = L.town[f.town.size]?.r ?? L.town.town.r;
  const bores = fort.level > 0 ? (L.forts[String(Math.min(fort.level, 5))] ?? L.forts['5']) : [];
  // (each class's track along the shore: the scene's run either way of its middle, its pieces' spacing, a division's column)
  const halfOf = (c: GunCls): number => {
    const n = c === 'bb' ? drawnBb : c === 'ca' ? drawnCa : ddPieces;
    const tail = c === 'dd' ? (Math.max(1, ...ddSlots) - 1) * L.bands.column : 0;
    return Math.ceil((L.bands.speed * B.after[1]) / 2 + ((n - 1) / 2) * L.bands.spacing[c] + tail + L.classes[c].len / 2 + 250);
  };
  const halves = Object.fromEntries(classes.map((c) => [c, halfOf(c)])) as Partial<Record<GunCls, number>>;
  const lay = layCoast(coast, { classes, halves, bores, strips: divsDrawn, town: townAt ? { r: townR, at: townAt } : null }, look);
  if (classes.some((c) => lay.lines[c] === null)) standIns.push('the battle line: no water clear of the land by 1.5 km in its band; laid as far off as the board allows');

  // Shelter by strip: the Shelter line says only whether any are dug in or under concrete; half by hash rank.
  const stripKinds: Strip['kind'][] = (() => {
    const n = lay.strips.length;
    const order = Array.from({ length: n }, (_, i) => i).sort((a, b) => look(`shelter:${a}`) - look(`shelter:${b}`));
    const kinds: Strip['kind'][] = new Array(n).fill('open');
    const first = shelter === 'fort' ? 'fort' : shelter === 'dug' ? 'dug' : 'open';
    const second = shelter === 'fort' ? 'dug' : 'open';
    order.forEach((i, k) => { kinds[i] = k < Math.ceil(n / 2) ? first : second; });
    return kinds;
  })();
  if (rep.what === 'troops' && shelter !== 'open') standIns.push('which divisions are dug in or under concrete: half by hash rank (the Shelter line says only that some are)');
  const strips: Strip[] = lay.strips.map((x, k) => ({ x: x.x, y: x.y, len: x.len, wide: x.wide, face: r1(x.face), kind: stripKinds[k] }));
  const targetC: Site = rep.what === 'troops'
    ? (strips.length ? { x: strips.reduce((a, x) => a + x.x, 0) / strips.length, y: strips.reduce((a, x) => a + x.y, 0) / strips.length } : { x: coast.at.x - u.ux * 1500, y: coast.at.y - u.uy * 1500 })
    : lay.town ? { x: lay.town.x, y: lay.town.y } : { x: coast.at.x - u.ux * 1500, y: coast.at.y - u.uy * 1500 };
  const targetR = rep.what === 'troops' ? Math.max(600, ...strips.map((x) => Math.hypot(x.x - targetC.x, x.y - targetC.y) + x.len / 2)) : townR;

  // The battle line: each class on its own line off the coast, in line ahead, steaming along the shore; it passes the
  // waterfront at the scene's middle.
  const dir = look('dir') < 0.5 ? -1 : 1;
  const v = L.bands.speed;
  const T_END = B.after[1];
  const lineFar = (c: GunCls): { d: number; s: number } => lay.lines[c] ?? { d: Math.min(ROOM_MOST, Math.max(L.bands[c][0], coast.room - L.bands.clear)), s: 0 };
  for (const c of classes) {
    const ofClass = pieces.filter((p) => p.cls === c);
    const line = lineFar(c);
    ofClass.forEach((p, k) => { p.d = line.d; p.dir = dir; p.along0 = line.s - dir * (v * (T_END / 2)) - dir * k * L.bands.spacing[c] + ((ofClass.length - 1) / 2) * dir * L.bands.spacing[c]; });
  }
  const posAt = (p: Piece, t: number): { x: number; y: number; h: number } => {
    const s = p.along0 + p.dir * v * t;
    return { x: coast.at.x + u.ux * p.d + u.tx * s, y: coast.at.y + u.uy * p.d + u.ty * s, h: wrap2Pi(Math.atan2(u.tx * p.dir, u.ty * p.dir)) };
  };
  /** A slot's place in her division's column (her own: slot 0 leads). */
  const slotAt = (p: Piece, slot: number, t: number): { x: number; y: number; h: number } => {
    const q = posAt(p, t);
    return { x: q.x - Math.sin(q.h) * slot * L.bands.column, y: q.y - Math.cos(q.h) * slot * L.bands.column, h: q.h };
  };
  const dist = (a: Site, b: Site): number => Math.hypot(a.x - b.x, a.y - b.y);
  const lead = pieces[0];

  // ---- The salvos ----------------------------------------------------------------------------------------------
  interface Salvo { piece: Piece; slot: number; t: number; lands: number; n: number; mm: number; aim: Site; train: number; range: number; idx: number; lay?: { on: number; sea: number; beach: number; over: number }; kinds?: string[] }
  const salvos: Salvo[] = [];
  const aimFor = (k: number, p: Piece, slot: number): Site => {
    if (rep.what === 'troops' && strips.length) { const st = strips[Math.floor(look(`aim:${p.id}:${slot}:${k}`) * strips.length)]; return { x: st.x, y: st.y }; }
    return targetC;
  };
  // (the salvos each ship fires, counted before any look: at her cadence over the guns' span, or look.salvos shared out
  // over every ship that fired by her rate, whole, so that the ships drawn fire exactly their shares; then each laid by
  // hash in its own slot of the span, so that none falls outside it: another seed moves when they fire, never how many)
  const span = B.fireEnd - B.fire0 - 0.5;
  const given = typeof rep.look?.salvos === 'number' && Number.isFinite(rep.look.salvos) ? Math.max(0, Math.round(rep.look.salvos)) : null;
  const perShip: Record<GunCls, number[]> = { bb: [], ca: [], dd: [] };
  if (given !== null) {
    const byClass = wholeShares(given, [fired.bb / L.cadence.bb, fired.ca / L.cadence.ca, fired.dd / L.cadence.dd]);
    (['bb', 'ca', 'dd'] as GunCls[]).forEach((c, i) => { perShip[c] = wholeShares(byClass[i], Array.from({ length: fired[c] }, () => 1)); });
  }
  // (each drawn piece's place in its class; a cruiser takes her share by her place in the mix, then her number, so the
  // salvos each bore fires are fixed by the mix and not by which cruiser the hash gave it; a division's ships in turn)
  const inClass = new Map<number, number>();
  for (const c of ['bb', 'ca', 'dd'] as GunCls[]) pieces.filter((p) => p.cls === c).forEach((p, k) => inClass.set(p.id, k));
  const caRank = Array.from({ length: drawnCa }, (_, k) => k).sort((a, b) => caMixAt[a] - caMixAt[b] || a - b);
  const ddFirst = ddSlots.map((_, k) => ddSlots.slice(0, k).reduce((a, x) => a + x, 0));
  const salvosOf = (p: Piece, slot: number): number => {
    if (given === null) return Math.max(1, Math.floor(span / L.cadence[p.cls]));
    const k = inClass.get(p.id)!;
    return (p.cls === 'bb' ? perShip.bb[k] : p.cls === 'ca' ? perShip.ca[caRank.indexOf(k)] : perShip.dd[ddFirst[k] + slot]) ?? 0;
  };
  // (the opening salvo, at fire0: the leading ship's, or with look.salvos the first ship given any)
  const opener = pieces.find((p) => salvosOf(p, 0) > 0);
  if (!opener) return null;
  for (const p of pieces) for (let slot = 0; slot < p.slots; slot++) {
    const n = salvosOf(p, slot);
    const g = span / Math.max(1, n);
    const first = p === opener && slot === 0;
    const phase = first ? 0 : 0.8 * look(`first:${p.id}:${slot}`);
    for (let k = 0; k < n; k++) {
      const t = first && k === 0 ? B.fire0 : B.fire0 + 0.5 + g * (k + phase + 0.15 * look(`gap:${p.id}:${slot}:${k}`));
      const q = slotAt(p, slot, t);
      const aim = aimFor(k, p, slot);
      const range = dist(q, aim);
      salvos.push({ piece: p, slot, t: r1(t), lands: r1(t + flightOf(p.mm, range)), n: p.guns, mm: p.mm, aim, train: r3(wrap2Pi(Math.atan2(aim.x - q.x, aim.y - q.y))), range, idx: salvos.length });
    }
  }
  salvos.sort((a, b) => a.t - b.t || a.piece.id - b.piece.id || a.slot - b.slot);
  salvos.forEach((x, i) => { x.idx = i; });
  const leadSalvo = salvos.find((x) => x.piece === opener && x.slot === 0)!;
  const landLead = leadSalvo.lands;

  // Every shell on target, short in the water, short on the beach, or over, by hash rank so the counts are exact.
  const shells: { salvo: number; k: number }[] = [];
  for (const x of salvos) for (let k = 0; k < x.n; k++) shells.push({ salvo: x.idx, k });
  const N = shells.length;
  const onCount = shareOn > 0 && (rep.what !== 'troops' || rep.cut > 0) ? Math.round(N * (0.25 + 0.5 * shareOn)) : 0;
  const ranked = shells.map((x, i) => ({ i, h: look(`fall:${x.salvo}:${x.k}`) })).sort((a, b) => a.h - b.h || a.i - b.i);
  const kindOf: string[] = new Array(N).fill('over');
  const [nSea, nBeach] = wholeShares(N - onCount, [L.lay.sea, L.lay.beach, L.lay.over]);
  ranked.forEach((r, j) => { kindOf[r.i] = j < onCount ? 'on' : j < onCount + nSea ? 'sea' : j < onCount + nSea + nBeach ? 'beach' : 'over'; });
  { let i = 0; for (const x of salvos) { x.kinds = kindOf.slice(i, i + x.n); i += x.n; x.lay = { on: 0, sea: 0, beach: 0, over: 0 }; for (const k of x.kinds) x.lay[k as 'on']++; } }

  // ---- The batteries and the ships they strike -------------------------------------------------------------------
  interface Round { bat: number; t: number; lands: number; on: Piece; slot: number; kind: 'short' | 'over' | 'straddle' | 'hit' }
  const rounds: Round[] = [];
  const batteries = lay.batteries.map((b, i) => ({ ...b, mm: bores[i] }));
  const fortAt: Site = batteries.length ? { x: batteries.reduce((a, b) => a + b.x, 0) / batteries.length, y: batteries.reduce((a, b) => a + b.y, 0) / batteries.length } : coast.at;
  interface Doom { piece: Piece; slot: number; hits: number; sinks: boolean; hitAt: number[]; sunkAt: number }
  const dooms: Doom[] = [];
  const bt0 = landLead + 9;
  if (fort.level > 0 && fort.fired && batteries.length) {
    // The doomed: those nearest the fort, by hash rank among equals; the ships sunk first, then those kept afloat.
    const slotsOf = (c: GunCls): { piece: Piece; slot: number; d: number; h: number }[] => pieces.filter((p) => p.cls === c).flatMap((p) => Array.from({ length: p.slots }, (_, slot) => ({ piece: p, slot, d: Math.round(dist(posAt(p, bt0 + 30), fortAt) / 500), h: look(`doom:${p.id}:${slot}`) })))
      .sort((a, b) => a.d - b.d || a.h - b.h);
    const taken = new Set<string>();
    const take = (c: GunCls, n: number, sinks: boolean): number => {
      let got = 0;
      for (const x of slotsOf(c)) {
        if (got >= n) break;
        const k = `${x.piece.id}:${x.slot}`;
        if (taken.has(k)) continue;
        taken.add(k);
        // (the hits are fixed by class, never by a hash: another seed never moves a count)
        const hits = sinks ? (c === 'dd' ? L.hits.dd : L.hits.ca) : c === 'bb' ? L.hits.struck.hits : L.hits.kept[c];
        dooms.push({ piece: x.piece, slot: x.slot, hits, sinks, hitAt: [], sunkAt: -1 });
        got++;
      }
      return got;
    };
    const shortOf: string[] = [];
    for (const c of ['ca', 'dd'] as const) { const n = sunk[c] ?? 0; const got = take(c, n, true); if (got < n) shortOf.push(`${n - got} ${CLASS_MANY[c]} sunk`); }
    for (const c of ['ca', 'dd'] as const) { const n = kept[c] ?? 0; const got = take(c, n, false); if (got < n) shortOf.push(`${n - got} ${CLASS_MANY[c]} kept`); }
    // (the battleships struck: hits and a deck fire, never a sinking; the navy's ruling 3)
    take('bb', struckShips(rep, drawnBb), false);
    if (shortOf.length) standIns.push(`ships lost or hit beyond those drawn: ${shortOf.join(', ')} out of frame`);
    // The opening: short, over, straddle on the first doomed (or, with none, on the nearest ship), from the batteries in turn.
    const first = dooms[0] ?? (() => { const x = pieces.flatMap((p) => Array.from({ length: p.slots }, (_, slot) => ({ p, slot }))).sort((a, b) => dist(posAt(a.p, bt0 + 20), fortAt) - dist(posAt(b.p, bt0 + 20), fortAt) || a.p.id - b.p.id)[0]; return { piece: x.p, slot: x.slot }; })();
    const fireAt = (bat: number, t: number, on: Piece, slot: number, kind: Round['kind']): Round => {
      const b = batteries[bat];
      const q0 = slotAt(on, slot, t);
      const fl = flightOf(b.mm, dist(b, q0));
      const r: Round = { bat, t: r1(t), lands: r1(t + fl), on, slot, kind };
      rounds.push(r);
      return r;
    };
    const nb = batteries.length;
    fireAt(0, bt0 + 1, first.piece, first.slot, 'short');
    fireAt(1 % nb, bt0 + 5, first.piece, first.slot, 'over');
    const straddle = fireAt(2 % nb, bt0 + 10, first.piece, first.slot, 'straddle');
    // The hits: each doomed ship's, the gap apart; the next ship's first after the last's.
    let fire = straddle.t + 15;
    for (const [i, d] of dooms.entries()) {
      if (i > 0) { fireAt((i + 1) % nb, fire - 8, d.piece, d.slot, 'straddle'); }
      for (let h = 0; h < d.hits; h++) {
        const r = fireAt((i + h) % nb, fire, d.piece, d.slot, 'hit');
        d.hitAt.push(r.lands);
        fire += L.hits.gap;
      }
      const last = d.hitAt[d.hitAt.length - 1];
      if (d.sinks) { d.sunkAt = r1(last + 12); d.piece.sunkAt[d.slot] = d.sunkAt; }
      fire += L.hits.next - L.hits.gap;
    }
    // Every battery's own fire besides, at the ships that stay afloat: short, over or straddle, never a hit. Its rounds
    // are counted from the guns' span and its cadence before any look (none where every ship drawn is sunk), then each laid
    // by hash in its own slot of the span left after the opening rounds.
    const afloat = pieces.flatMap((p) => Array.from({ length: p.slots }, (_, slot) => ({ p, slot }))).filter((x) => x.p.sunkAt[x.slot] < 0);
    for (let b = 0; b < nb; b++) {
      const n = afloat.length ? Math.max(0, Math.floor((B.fireEnd - B.fire0 - L.forts.opening - 5 * b) / L.forts.cadence)) : 0;
      const t0 = bt0 + 12 + 5 * b;
      const g = Math.max(1, B.fireEnd - t0) / Math.max(1, n);
      for (let k = 0; k < n; k++) {
        const t = t0 + g * (k + 0.1 + 0.8 * look(`bat:${b}:${k}:gap`));
        const x = afloat[Math.floor(look(`bat:${b}:${k}:on`) * afloat.length)];
        const h = look(`bat:${b}:${k}:kind`);
        fireAt(b, t, x.p, x.slot, h < 0.35 ? 'short' : h < 0.7 ? 'over' : 'straddle');
      }
    }
    rounds.sort((a, b) => a.lands - b.lands || a.t - b.t || a.bat - b.bat);
  }
  // (battleships straddled at most, unless the rules' look says they were struck: hits and smoke then, never a sinking)
  if (dooms.some((d) => d.piece.cls === 'bb' && d.sinks)) throw new Error('a battleship is never sunk');

  // ---- The beats: windows of battle time, their screen seconds, and each one's shots ---------------------------------
  const beats: ShoreBeat[] = [];
  const spotOf = (p: Site): [number, number] => [Math.round(p.x), Math.round(p.y)];
  const win = (what: string, from: number, to: number, screen: number, shots: Shot[]): void => {
    from = Math.max(0, from);
    const inside = (a: number, b: number): Shot[] => shots.filter((x, i) => i === 0 || (x.at >= a && x.at < b)).map((x, i) => (i === 0 ? { ...x, at: r1(a) } : { ...x, at: r1(x.at) }));
    if ((to - from) / screen <= B.pace) { beats.push({ what, from: r1(from), to: r1(to), screen, shots: inside(from, to) }); return; }
    // (too long for its seconds: its first and its last moments, the middle cut; as sub-scene.ts's beat builder)
    const a = Math.max(2, Math.round(screen / 2));
    const aEnd = from + a * B.pace, bFrom = to - (screen - a) * B.pace;
    beats.push({ what, from: r1(from), to: r1(aEnd), screen: a, shots: inside(from, aEnd) });
    beats.push({ what, from: r1(bFrom), to: r1(to), screen: screen - a, shots: [{ ...shots[shots.length - 1], at: r1(bFrom) }] });
  };
  const S = B.screens;
  const tFire = B.fire0;
  const plan: { what: string; from: number; to: number; screen: number; shots: Shot[] }[] = [];
  plan.push({ what: 'line', from: 0, to: tFire, screen: S.line, shots: [{ at: 0, kind: 'SeaLine', subject: lead.id }] });
  plan.push({ what: 'broadside', from: tFire, to: tFire + 3.5, screen: S.broadside, shots: [{ at: tFire, kind: 'Broadside', subject: opener.id }] });
  plan.push({ what: 'climb', from: tFire + 3.5, to: tFire + 5.5, screen: S.climb, shots: [{ at: tFire + 3.5, kind: 'BehindTurret', subject: opener.id }] });
  const quiet0 = Math.max(tFire + 5.5, landLead - 6);
  if (landLead > quiet0 + 0.5) plan.push({ what: 'quiet', from: quiet0, to: landLead, screen: S.quiet, shots: [{ at: quiet0, kind: 'HillEye', spot: spotOf(targetC) }] });
  plan.push({ what: 'falls', from: Math.max(landLead, quiet0), to: Math.max(landLead, quiet0) + 3, screen: S.falls, shots: [{ at: Math.max(landLead, quiet0), kind: 'HillEye', spot: spotOf(targetC) }] });
  const fallsEnd = Math.max(landLead, quiet0) + 3;
  plan.push({ what: 'pattern', from: fallsEnd + 1, to: fallsEnd + 4, screen: S.pattern, shots: [{ at: fallsEnd + 1, kind: spot ? 'SpotterEye' : 'HillEye', spot: spotOf(targetC) }] });
  const sinkFirst = dooms.find((d) => d.sinks);
  if (rounds.length) {
    plan.push({ what: 'fort', from: bt0, to: bt0 + 6, screen: S.fort, shots: [{ at: bt0, kind: 'FortEye', spot: spotOf(batteries[0]) }] });
    const open = rounds.filter((r) => r.t <= bt0 + 10.01 && r.t >= bt0 + 0.99).sort((a, b) => a.lands - b.lands);
    const g0 = Math.max(bt0 + 7, open[0].lands - 2), g1 = Math.max(g0 + 3, open[open.length - 1].lands + 2);
    plan.push({ what: 'glass', from: g0, to: g1, screen: S.glass, shots: [{ at: g0, kind: 'ShoreGlass', subject: open[0].on.id, spot: spotOf(batteries[0]) }] });
    if (dooms.length) {
      const h0 = dooms[0].hitAt[0];
      plan.push({ what: 'hit', from: Math.max(g1 + 1, h0 - 4), to: Math.max(g1 + 1, h0 - 4) + 10, screen: S.hit, shots: [{ at: Math.max(g1 + 1, h0 - 4), kind: 'Hit', subject: dooms[0].piece.id, spot: spotOf(batteries[0]) }] });
    }
    if (sinkFirst) plan.push({ what: 'sinking', from: sinkFirst.sunkAt - 4, to: sinkFirst.sunkAt + 46, screen: S.sinking, shots: [{ at: sinkFirst.sunkAt - 4, kind: 'Sinking', subject: sinkFirst.piece.id }] });
  }
  plan.push({ what: 'after', from: B.after[0], to: B.after[1], screen: S.after, shots: [{ at: B.after[0], kind: 'SeaWide', spot: spotOf(targetC) }] });
  // (the pads, while the scene runs short: a later salvo's falls in free time, the next ship's broadside, another fall)
  const seconds = (): number => plan.reduce((a, x) => a + x.screen, 0);
  const free = (a: number, b: number): boolean => plan.every((x) => b + 1 <= x.from || a - 1 >= x.to);
  for (const pad of B.pad) {
    if (seconds() >= B.least) break;
    if (pad === 'broadside2') {
      const other = salvos.find((x) => x.t > tFire + 0.01 && free(x.t - 0.5, x.t + 3) && (x.piece !== opener || pieces.length === 1) && x.t < B.fireEnd - 30);
      if (other) plan.push({ what: 'broadside2', from: other.t - 0.5, to: other.t + 3, screen: S[pad], shots: [{ at: other.t - 0.5, kind: 'Broadside', subject: other.piece.id }] });
    } else {
      const later = salvos.find((x) => x.lands > fallsEnd + 6 && x.lands < B.fireEnd && free(x.lands - 0.5, x.lands + 2.5) && x.lay!.on + x.lay!.beach + x.lay!.over > 0);
      if (later) plan.push({ what: pad, from: later.lands - 0.5, to: later.lands + 2.5, screen: S[pad], shots: [{ at: later.lands - 0.5, kind: 'HillEye', spot: spotOf(later.aim) }] });
    }
  }
  for (const cut of B.trim) { if (seconds() <= B.most) break; const i = plan.findIndex((x) => x.what === cut); if (i >= 0) plan.splice(i, 1); }
  for (const x of plan) win(x.what, x.from, x.to, x.screen, x.shots);
  beats.sort((a, b) => a.from - b.from);
  // (windows that meet are joined, their seconds added; their shots keep their cuts)
  for (let i = 1; i < beats.length; i++) {
    if (beats[i].from <= beats[i - 1].to) {
      const a = beats[i - 1], b = beats[i];
      beats[i - 1] = { what: a.what === b.what ? a.what : `${a.what}+${b.what}`, from: a.from, to: Math.max(a.to, b.to), screen: a.screen + b.screen, shots: [...a.shots, ...b.shots] };
      beats.splice(i--, 1);
    }
  }
  for (const b of beats) {
    b.shots.sort((x, y) => x.at - y.at);
    // (a shot cut in before the window opens starts with it; two at one moment, the later kept)
    b.shots = b.shots.map((x) => ({ ...x, at: r1(clamp(x.at, b.from, b.to)) })).filter((x, i, all) => i === all.length - 1 || all[i + 1].at > x.at);
  }

  // ---- Where each shell falls (only the salvos landing within 10 s of a window carry their points) ----------------
  const nearWindow = (t: number): boolean => beats.some((b) => t >= b.from - 10 && t <= b.to + 10);
  const land = coast.land;
  const shoreCross = (from: Site, to: Site): Site => {
    // (from the aim back toward the ship, the last point on land: where the line of fire crosses the shore)
    const d = dist(from, to);
    let last = to;
    for (let k = 0; k <= d; k += 100) {
      const x = to.x + ((from.x - to.x) * k) / d, y = to.y + ((from.y - to.y) * k) / d;
      if (!onLand(land, x, y)) return last;
      last = { x, y };
    }
    return last;
  };
  let points = 0;
  const fallsOf = (x: Salvo): [number, number, string][] => {
    const q = slotAt(x.piece, x.slot, x.t);
    const fx = (x.aim.x - q.x) / x.range, fy = (x.aim.y - q.y) / x.range;
    const ax = fy, ay = -fx; // (across the line of fire)
    const S0 = shoreCross(q, x.aim);
    const long = L.pattern.long * x.range, across = L.pattern.across * x.range;
    const strip = rep.what === 'troops' && strips.length ? strips.reduce((b, st) => (Math.hypot(st.x - x.aim.x, st.y - x.aim.y) < Math.hypot(b.x - x.aim.x, b.y - x.aim.y) ? st : b), strips[0]) : null;
    return x.kinds!.map((k, i) => {
      const h1 = look(`pt:${x.idx}:${i}:a`), h2 = look(`pt:${x.idx}:${i}:b`);
      let p: Site = x.aim;
      for (let t = 0; t < 6; t++) {
        const j1 = t ? look(`pt:${x.idx}:${i}:${t}:a`) : h1, j2 = t ? look(`pt:${x.idx}:${i}:${t}:b`) : h2;
        if (k === 'on') {
          if (strip) {
            const fa = strip.face * RAD;
            p = { x: strip.x + Math.sin(fa) * (j1 - 0.5) * strip.len * 0.95 + Math.cos(fa) * (j2 - 0.5) * strip.wide * 0.95, y: strip.y + Math.cos(fa) * (j1 - 0.5) * strip.len * 0.95 - Math.sin(fa) * (j2 - 0.5) * strip.wide * 0.95 };
          } else {
            const r = Math.sqrt(j1) * targetR * 0.8, a = j2 * TAU;
            p = { x: x.aim.x + Math.sin(a) * r, y: x.aim.y + Math.cos(a) * r };
          }
          if (onLand(land, p.x, p.y)) break;
        } else if (k === 'sea') {
          const back = 50 + 550 * j1;
          p = { x: S0.x - fx * back + ax * (j2 - 0.5) * across * 4, y: S0.y - fy * back + ay * (j2 - 0.5) * across * 4 };
          if (!onLand(land, p.x, p.y)) break;
        } else if (k === 'beach') {
          const into = 10 + 290 * j1;
          p = { x: S0.x + fx * into + ax * (j2 - 0.5) * across * 4, y: S0.y + fy * into + ay * (j2 - 0.5) * across * 4 };
          if (onLand(land, p.x, p.y)) break;
        } else {
          const past = (rep.what === 'troops' ? 300 : targetR) + long * 2 + 1200 * j1;
          p = { x: x.aim.x + fx * past + ax * (j2 - 0.5) * across * 6, y: x.aim.y + fy * past + ay * (j2 - 0.5) * across * 6 };
          if (onLand(land, p.x, p.y)) break;
        }
      }
      return [Math.round(p.x), Math.round(p.y), k] as [number, number, string];
    });
  };

  // ---- The events --------------------------------------------------------------------------------------------
  const events: SceneEvent[] = [];
  const attSide: 'att' = 'att';
  const nameIn = (p: Piece): string => (p.cls === 'bb' ? (me === 'att' && names.includes(p.name) ? p.name : 'a battleship') : p.cls === 'ca' ? 'a cruiser' : 'a destroyer');
  for (const x of salvos) {
    const q = slotAt(x.piece, x.slot, x.t);
    const extra: Record<string, unknown> = { ship: x.slot + 1, lands: x.lands, mm: x.mm, n: x.n, train: x.train, lay: x.lay };
    if (nearWindow(x.lands)) { extra.falls = fallsOf(x); points += x.n; }
    events.push({ t: x.t, what: 'sea:shore', unit: x.piece.id, side: attSide, x: q.x, y: q.y, text: `${upFirst(nameIn(x.piece))} fires on the coast.`, extra });
  }
  for (const r of rounds) {
    const b = batteries[r.bat];
    const q = slotAt(r.on, r.slot, r.lands);
    const pts: [number, number, string][] = [];
    const n = 4;
    for (let k = 0; k < n; k++) {
      const a = look(`rf:${r.t}:${r.bat}:${k}:a`), c = look(`rf:${r.t}:${r.bat}:${k}:b`);
      const fx = (q.x - b.x) / Math.max(1, dist(b, q)), fy = (q.y - b.y) / Math.max(1, dist(b, q));
      const off = r.kind === 'short' ? -(150 + 250 * a) : r.kind === 'over' ? 150 + 250 * a : r.kind === 'straddle' ? (k % 2 ? 1 : -1) * (40 + 90 * a) : (k === 0 ? 0 : (k % 2 ? 1 : -1) * (30 + 80 * a));
      const kind = r.kind === 'hit' && k === 0 ? 'hit' : r.kind === 'hit' ? 'straddle' : r.kind;
      pts.push([Math.round(q.x + fx * off + fy * (c - 0.5) * 80), Math.round(q.y + fy * off - fx * (c - 0.5) * 80), kind]);
    }
    events.push({ t: r.t, what: 'sea:battery', side: 'def', x: b.x, y: b.y, text: '', extra: { bat: r.bat, x: Math.round(b.x), y: Math.round(b.y), mm: b.mm, lands: r.lands, on: r.on.id, ship: r.slot + 1, kind: r.kind, falls: pts } });
    if (r.kind === 'hit') events.push({ t: r.lands, what: 'sea:gunHit', unit: r.on.id, side: 'def', x: q.x, y: q.y, text: `A shell from the shore batteries hits ${nameIn(r.on)}.`, extra: { n: 1, from: -(r.bat + 2), ship: r.slot + 1 } });
  }
  const crewOf = (c: GunCls): number => (c === 'ca' ? L.crew.ca : L.crew.dd);
  const depthAt = (d: number): number => kit.shelf * (d / 3000);
  for (const d of dooms) {
    const p = d.piece;
    if (d.sinks) {
      const q = slotAt(p, d.slot, d.sunkAt);
      const how = depthAt(p.d) < 15 ? 'settle' : 'break';
      events.push({ t: r1(d.sunkAt - 6), what: 'sea:abandon', unit: p.id, side: attSide, x: q.x, y: q.y, text: `${upFirst(nameIn(p))}'s crew abandon her.`, extra: { ship: d.slot + 1 } });
      events.push({ t: d.sunkAt, what: 'sea:sunk', unit: p.id, side: attSide, x: q.x, y: q.y, cue: 'ship_sinking', text: `${upFirst(nameIn(p))} goes down.`, extra: { cls: p.cls, fleet: 1, ship: d.slot + 1, how } });
      const men = crewOf(p.cls);
      events.push({ t: r1(d.sunkAt + 2), what: 'sea:men', unit: p.id, side: attSide, text: '', extra: { ship: d.slot + 1, men: Math.round(men * (0.5 + 0.3 * look(`men:${p.id}:${d.slot}`))), dead: Math.round(men * (0.1 + 0.2 * look(`dead:${p.id}:${d.slot}`))) } });
      events.push({ t: r1(d.sunkAt + 4), what: 'sea:oil', unit: p.id, side: attSide, text: '', extra: { ship: d.slot + 1, r: Math.round(200 + 400 * look(`oil:${p.id}:${d.slot}`)) } });
    } else {
      // (kept afloat: a deck fire from her first hit, and she steams on; no sea:burning)
      const t0 = r1(d.hitAt[0] + 1);
      const q = slotAt(p, d.slot, t0);
      events.push({ t: t0, what: 'sea:fire', unit: p.id, side: attSide, x: q.x, y: q.y, text: '', extra: { ship: d.slot + 1, x: Math.round(q.x), y: Math.round(q.y), r: 15, kind: 'deck', until: r1(t0 + 120 + 120 * look(`deck:${p.id}:${d.slot}`)) } });
    }
  }
  // Ground: dust over the strips is the falls' own; fires as the share grows, none on concrete; a town's palls.
  const firstOn = salvos.find((x) => (x.lay?.on ?? 0) > 0)?.lands ?? landLead;
  let fires = 0, palls = 0;
  if (s > 0 && onCount > 0) {
    if (rep.what === 'troops') {
      const burnable = strips.filter((x) => x.kind !== 'fort');
      const n = burnable.length ? Math.min(L.most.fires, Math.max(1, Math.round(3 * s))) : 0;
      for (let k = 0; k < n; k++) {
        const st = burnable[Math.floor(look(`fire:${k}:strip`) * burnable.length)];
        const fa = st.face * RAD, a = look(`fire:${k}:a`) - 0.5, b = look(`fire:${k}:b`) - 0.5;
        const t = r1(firstOn + 10 + 100 * look(`fire:${k}:t`));
        events.push({ t, what: 'sea:fire', side: 'def', text: '', extra: { x: Math.round(st.x + Math.sin(fa) * a * st.len + Math.cos(fa) * b * st.wide), y: Math.round(st.y + Math.cos(fa) * a * st.len - Math.sin(fa) * b * st.wide), r: Math.round(15 + 25 * look(`fire:${k}:r`)), kind: look(`fire:${k}:kind`) < 0.5 ? 'vehicle' : 'grass', until: r1(T_END + 60) } });
        fires++;
      }
    } else if (lay.town) {
      const damage = rep.city ? clamp(rep.city.now / 100, 0, 1) : rep.what === 'town' ? clamp((rep.works ?? 0) / BREACH.most, 0, 1) : 1;
      const n = Math.max(1, Math.min(L.most.townFires, Math.ceil(s * L.most.townFires), Math.max(1, Math.round(damage * L.most.townFires * 1.5))));
      const np = s > L.ground.palls ? L.most.palls : 1;
      for (let k = 0; k < n + np; k++) {
        const pall = k >= n;
        let x = lay.town.x, y = lay.town.y;
        for (let t = 0; t < 8; t++) {
          const r = Math.sqrt(look(`tf:${k}:${t}:r`)) * lay.town.r * 0.7, a = look(`tf:${k}:${t}:a`) * TAU;
          x = lay.town.x + Math.sin(a) * r; y = lay.town.y + Math.cos(a) * r;
          if (onLand(land, x, y)) break;
        }
        const kind = pall ? (rep.what === 'city' && f.town.port && k === n ? 'oil' : 'pall') : rep.what === 'city' ? 'works' : 'block';
        events.push({ t: r1(firstOn + 15 + 120 * look(`tf:${k}:t`)), what: 'sea:fire', side: 'def', text: '', extra: { x: Math.round(x), y: Math.round(y), r: Math.round(pall ? 60 + 60 * look(`tf:${k}:pr`) : 25 + 40 * look(`tf:${k}:fr`)), kind, until: r1(T_END + 60) } });
        if (pall) palls++; else fires++;
      }
    }
  }
  // Spotting: the spotter pair over the target, only where the report says (floatplanes, or the carriers' aircraft, the carrier unseen).
  const spotFrom = spotBy === 'carrier' ? 'carrier' : spotBy === 'floatplane' ? 'ship' : '';
  if (spot) events.push({ t: r1(Math.max(1, landLead - 12)), what: 'sea:spot', side: attSide, text: '', extra: { nat: rep.nat, x: Math.round(targetC.x), y: Math.round(targetC.y), r: 2500, alt: Math.round(1000 + 600 * look('spot:alt')), until: B.fireEnd } });

  // ---- The views ---------------------------------------------------------------------------------------------
  events.sort((a, b) => a.t - b.t || (a.what === 'sea:gunHit' ? 0 : 1) - (b.what === 'sea:gunHit' ? 0 : 1));
  const told = new Map<number, { u: Record<string, unknown>; x: Record<string, unknown> }>();
  const unitOut = (p: Piece, t: number): Record<string, unknown> => {
    const lost = p.sunkAt.filter((x) => x >= 0 && x <= t + 1e-9).length;
    const gone = lost >= p.slots;
    const q = posAt(p, gone ? Math.max(...p.sunkAt) : t);
    const sz = L.classes[p.cls];
    const u: Record<string, unknown> = { id: p.id, side: attSide, kind: p.cls, name: p.name, nat: rep.nat };
    u.x = r1(q.x); u.y = r1(q.y); u.face = r3(q.h); u.str = r3((p.slots - lost) / p.slots); u.mor = 1; u.state = gone ? 'gone' : 'steady';
    u.figures0 = p.slots; u.figures = p.slots - lost; u.w = sz.beam;
    u.d = sz.len + (p.slots - 1) * L.bands.column;
    u.act = gone ? 'down' : 'march';
    u.speed = gone ? 0 : v; u.engaged = -1; u.loop = gone ? '' : 'ship_engine_loop';
    const x: Record<string, unknown> = { cls: p.cls, ships0: p.slots, ships: p.slots - lost, sunk: lost, way: gone ? 0 : v, speed: 'cruise', target: -1, fleet: rep.fleet, mainMm: p.mm, guns: p.guns };
    u.extra = x;
    const was = told.get(p.id);
    const kept2 = (val: unknown): unknown => (val !== null && typeof val === 'object' ? JSON.stringify(val) : val);
    const nowU: Record<string, unknown> = {}, nowX: Record<string, unknown> = {};
    for (const k in u) if (k !== 'extra') nowU[k] = kept2(u[k]);
    for (const k in x) nowX[k] = kept2(x[k]);
    told.set(p.id, { u: nowU, x: nowX });
    if (!was) return u;
    const out: Record<string, unknown> = { id: p.id };
    for (const k in u) if (k !== 'extra' && k !== 'id' && was.u[k] !== nowU[k]) out[k] = u[k];
    let ox: Record<string, unknown> | null = null;
    for (const k in x) if (was.x[k] !== nowX[k] || (k === 'sunk' && lost > 0)) (ox ??= {})[k] = x[k];
    if (ox) out.extra = ox;
    return out;
  };
  const zones: { id: number; kind: string; x: number; y: number; r: number; until: number; side: 'att' | 'def'; at: number; unit: number }[] = [];
  for (const d of dooms) if (d.sinks) { const q = slotAt(d.piece, d.slot, d.sunkAt); zones.push({ id: zones.length + 1, kind: 'sunk', x: r1(q.x), y: r1(q.y), r: 300, until: r1(d.sunkAt + 60), side: 'att', at: r1(d.sunkAt - 1), unit: d.piece.id }); }
  const tEnd = T_END;
  const views: Record<string, unknown>[] = [];
  let ei = 0, lastZones = '';
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
    const live = zones.filter((z) => z.at <= t + 1e-9 && z.until >= t - 1e-9);
    const zs = JSON.stringify(live);
    const view: Record<string, unknown> = { t, limit: tEnd, speed: 1, units: pieces.map((p) => unitOut(p, t)), balance: { att: 0.5, def: 0.5 }, events: evs, ai: { att: true, def: true }, over: null, me, breaking: { att: 0, def: 0 }, contact: r1(Math.min(t, tFire)), broken: null };
    if (zs !== lastZones || t === 0) { view.zones = live; lastZones = zs; }
    views.push(view);
  }

  // ---- The header, the caption and the note ------------------------------------------------------------------
  const caption = captionOf(f, me);
  views[views.length - 1].over = { winner: '', level: '', lines: caption };
  const skyNow = skyAt(coast.lat, coast.lon, year, month0, hour.day, hour.hour);
  const cond = rep.look?.conditions;
  // (the look's sky is who has the air over the coast; only a closed sky says cloud: the rest is the sea scene's own; its
  // sea is 'normal', the sea scene's own swell, or a gale, which gets no scene)
  const skyRow = cond?.sky && Object.prototype.hasOwnProperty.call(L.skies, cond.sky) ? L.skies[cond.sky] : undefined;
  if (!skyRow) standIns.push("the sky's cloud: the sea scene's own (the report gives cloud only for a closed sky)");
  if (!cond) standIns.push("the sea: the sea scene's own (the report says none; a gale is heavyseas.ts galeSeas's, a hash of the seed and month)");
  const vis = skyRow ? Math.round((skyRow.cover > 0.8 ? 9000 : skyRow.cover > 0.4 ? 15000 : 20000) * (0.9 + 0.2 * look('vis'))) : Math.round(15000 + 8000 * look('vis'));
  const lostShips = dooms.filter((d) => d.sinks);
  const keptShips = dooms.filter((d) => !d.sinks);
  const ddDrawnSlots = pieces.filter((p) => p.cls === 'dd').reduce((a, p) => a + p.slots, 0);
  const sumLay = (k: 'on' | 'sea' | 'beach' | 'over'): number => salvos.reduce((a, x) => a + (x.lay?.[k] ?? 0), 0);
  const staged: ShoreNote = {
    scene: 'shore', beats, seconds: beats.reduce((a, x) => a + x.screen, 0), caption: { lines: caption },
    facts: {
      id: rep.id, turn: rep.turn, what: rep.what, region: rep.region, place: rep.place, zone: rep.zone, sea: f.sea.key, nat: rep.nat, defNat: rep.defNat, fleet: rep.fleet,
      ships: { ...rep.ships }, fired, sunk, kept, fort: fort.level, fortFired: fort.fired, share: r3(s), shareOn: r3(shareOn), divs: rep.divs, shelter, hour: r1(hour.hour), look: !!rep.look,
      // (whose order it was, and the readiness the batteries cost the battleships: kept, never drawn or worded yet)
      ...(rep.look ? { by: rep.look.by, struck: r3(rep.look.outcome.struck ?? 0) } : {}),
    },
    drawn: {
      fired, bb: drawnBb, ca: drawnCa, dd: ddDrawnSlots, pieces: pieces.length, divisions: ddPieces,
      sunk: { ca: lostShips.filter((d) => d.piece.cls === 'ca').length, dd: lostShips.filter((d) => d.piece.cls === 'dd').length },
      kept: { ca: keptShips.filter((d) => d.piece.cls === 'ca').length, dd: keptShips.filter((d) => d.piece.cls === 'dd').length }, struck: keptShips.filter((d) => d.piece.cls === 'bb').length,
      salvos: salvos.length, shells: N, on: sumLay('on'), sea: sumLay('sea'), beach: sumLay('beach'), over: sumLay('over'), points,
      batteryRounds: rounds.length, batteryHits: rounds.filter((r) => r.kind === 'hit').length, strips: strips.length, fires, palls,
    },
    standIns: [...new Set(standIns)],
  };
  const adj = (n: string): string => f.names[n]?.adj ?? n;
  const shipsOf = (): Record<string, number> => ({ bb: drawnBb, ca: drawnCa, dd: ddDrawnSlots });
  const sizeOf = (c: GunCls): Record<string, number> => ({ ...L.classes[c], reach: 0 });
  const relief = reliefOf(f.terrain);
  const rise = Math.round(pickIn(kit.rise ?? L.relief[relief], look('rise')));
  const field: Record<string, unknown> = {
    size: SIZE, edge: 5000, zoneName: f.sea.name,
    title: rep.what === 'troops' ? `The guns off ${placeName(rep.place)}` : rep.what === 'town' ? `The guns off ${f.town.name}` : `The guns off ${placeName(rep.place)}`,
    date: `${hour.day} ${MONTHS[month0]} ${year}`, when: { year, month: month0, hour: r1(hour.hour) }, lat: coast.lat, lon: coast.lon,
    att: { nat: rep.nat, adj: adj(rep.nat), human: f.human === rep.nat, fleets: [{ id: 1, name: rep.fleet, ships: shipsOf() }] },
    def: { nat: rep.defNat, adj: adj(rep.defNat), human: f.human === rep.defNat, fleets: [] },
    classes: Object.fromEntries(classes.map((c) => [c, sizeOf(c)])),
    wind: { from: r3(TAU * look('wind')), speed: r1(3 + 6 * look('windspeed')) },
    light: { sun: { up: r1(skyNow.sun.up), from: bearingDeg(skyNow.sun.from) }, moon: { lit: r3(skyNow.moon.lit), up: r1(skyNow.moon.up), from: bearingDeg(skyNow.moon.from) }, vis },
    ...(skyRow ? { sky: { cover: skyRow.cover, kind: skyRow.kind, word: cond!.sky } } : {}),
    ...(cond && Object.prototype.hasOwnProperty.call(L.seas, cond.sea) ? { sea: { swell: { height: L.seas[cond.sea][0], period: L.seas[cond.sea][1], from: bearingDeg(coast.bearing + 180 + (look('swell') - 0.5) * 60) } } } : {}),
    coast: {
      bearing: coast.bearing, at: { x: coast.at.x, y: coast.at.y }, shore: coast.land, form: coast.form, relief, rise, kit: kit.kit, land: kit.land,
      ground: rep.what === 'troops' ? (shelter === 'fort' ? 'concrete' : 'earth') : 'town', shelf: kit.shelf,
      ...(lay.town ? { town: { name: f.town.name, size: f.town.size, look: kit.town, port: f.town.port, x: r1(lay.town.x), y: r1(lay.town.y), r: lay.town.r,
        ...(f.town.port && townAt === 'waterfront' ? { quay: { x: coast.at.x, y: coast.at.y, face: coast.bearing, len: Math.round(pickIn(L.town.quay, look('quay'))) } } : {}),
        damage: r3(rep.city ? clamp(rep.city.now / 100, 0, 1) : rep.what === 'town' ? clamp((rep.works ?? 0) / BREACH.most, 0, 1) : 0),
        skyline: rep.what === 'city' && coast.townFrom <= L.town.skyline } } : {}),
      target: { kind: rep.what, x: r1(targetC.x), y: r1(targetC.y), r: Math.round(targetR), ...(rep.what === 'troops' ? { strips } : {}) },
      fort: { level: fort.level, fired: fort.fired, batteries: batteries.map((b) => ({ x: b.x, y: b.y, face: r1(b.face), mm: b.mm })) },
      air: { spot: spotBy, nat: rep.nat, from: spotFrom },
      whose: me,
    },
    staged,
  };
  const key = `shore-${rep.turn}-${rep.id}`;
  return {
    what: "STAGED: the shelling scene (sea-scene/shore-guns/plan.md R1), staged from the month's shelling report with no dice, for the 3D scene to show. Look only: no rule reads it. Each view carries a piece whole the first time it shows, then only what changed. field.staged.beats are the windows of battle time the scene shows, the screen seconds each is given and the shots cut to in it; field.coast is the coast it stands off.",
    recording: true, key, seed: seedOf(f), step: STEP, field, views, after: { lines: caption, level: '', winner: '', seconds: tEnd },
  };
}

// ---- Picking the month's scene (the UI director's order, 26 November 13:42Z) ------------------------------------

export type ShoreCase = 'shoreLoss' | 'shore' | 'shoreCoast';
export interface ShorePick { case: ShoreCase; id: number; me: 'att' | 'def'; lost: number; share: number }

/**
 * What the scene can show, which the 3D says from its built switches (plan R1: troops from R3, towns from R4, cities and
 * his coast shelled from R6). Without it, the first drawn round's: troops shelled by his nation.
 */
export interface ShoreShow { kinds: ShoreReport['what'][]; coast: boolean }
export const SHOW_FIRST: ShoreShow = { kinds: ['troops'], coast: false };

/** Where the month card's shellings start: its mark's index into g.shoreReports, else (a month whose opening was not seen) the month's first report. */
export interface ShoreWindow { from?: number | null; month0?: number }
/** The reports a card shows from its window: from the mark's index, else from the month's first (as the bridge's markBackTo), else last month's. */
export function shoreWindowOf(g: Game, w: ShoreWindow = {}): ShoreReport[] {
  const all = g.shoreReports ?? [];
  if (typeof w.from === 'number' && w.from >= 0 && w.from <= all.length) return all.slice(w.from);
  const month0 = typeof w.month0 === 'number' ? w.month0 : g.turn - 1;
  return all.filter((r) => r.turn >= month0);
}

/**
 * The month's shellings for him, each with its case: his nation's with a ship lost, his nation's, his coast shelled
 * (the month card's filter, monthcard.ts: his nation shelled, or his coast; the holder as the report says it, never
 * the map's present holder). An ally's shelling, or an ally's coast, is not on his card and gets none. A report already
 * played is not picked again; a night or gale shelling is skipped; a target or his coast the 3D does not show yet is
 * left out (`show`). Within a case: the most ships lost, then the largest share, then the first report.
 */
export function shoreScenesOf(g: Game, o: { window?: ShoreWindow; played?: ReadonlySet<number>; show?: ShoreShow } = {}): ShorePick[] {
  const me = g.human;
  if (!me) return [];
  const show = o.show ?? SHOW_FIRST;
  const out: ShorePick[] = [];
  for (const r of shoreWindowOf(g, o.window) as LookedReport[]) {
    if (o.played?.has(r.id) || !show.kinds.includes(r.what)) continue;
    const mine = r.nat === me ? 'att' : r.defNat === me ? 'def' : null;
    if (!mine || (mine === 'def' && !show.coast)) continue;
    if (barOf(shoreFactsOf(g, r))) continue;
    const lost = countOf(sunkOf(r));
    out.push({ case: mine === 'att' ? (lost > 0 ? 'shoreLoss' : 'shore') : 'shoreCoast', id: r.id, me: mine, lost, share: r.look ? clamp(r.look.outcome.share, 0, 1) : bandShare(r) });
  }
  const rank: Record<ShoreCase, number> = { shoreLoss: 0, shore: 1, shoreCoast: 2 };
  return out.sort((a, b) => rank[a.case] - rank[b.case] || b.lost - a.lost || b.share - a.share || a.id - b.id);
}

/** The month's one scene, in the UI director's order: his heavy ship lost; his shelling with a ship lost; his boats' kill; his shelling; his convoy month; his coast shelled; his largest hunt. */
export const MONTH_ORDER: (SceneCase | ShoreCase)[] = ['heavyLost', 'shoreLoss', 'boatsKill', 'shore', 'convoy', 'shoreCoast', 'hunt'];

export interface MonthSeaAnswer { mark: number; pick: Record<string, unknown> | null; recording: string | null }

/**
 * The bridge's TC.monthSea(from?, view?, show?): the month's one sea scene, a shelling among the submarine scene's
 * cases. With no shelling picked, the answer is TC.subScene's, byte for byte (sub-scene.ts subSceneOf). The viewer's
 * 'mine' (MY LOSSES ONLY) takes his shelling with a ship lost and never his coast shelled; 'never' takes nothing.
 * `window` is the month card's own mark (the bridge keeps it as the card is made); `played` the reports whose scene
 * was played already. It reads the game and writes nothing.
 */
export function monthSeaOf(g: Game, routes: RouteRow[], from?: number, view: SceneView = 'all', o: { window?: ShoreWindow; played?: ReadonlySet<number>; show?: ShoreShow } = {}): MonthSeaAnswer {
  const sub = (): MonthSeaAnswer => subSceneOf(g, routes, from, view);
  if (!g.human || view === 'never') return sub();
  const shores = shoreScenesOf(g, o).filter((p) => view !== 'mine' || p.case === 'shoreLoss');
  if (!shores.length) return sub();
  const d1 = scenesOf(factsOf(g, routes, from)).filter((p) => view !== 'mine' || hisLoss(p));
  for (const c of MONTH_ORDER) {
    if (d1.some((p) => p.case === c)) return sub();
    for (const s of shores.filter((p) => p.case === c)) {
      const rep = g.shoreReports.find((r) => r.id === s.id)!;
      const staged = stageShore(shoreFactsOf(g, rep), s.me);
      if (!staged) continue;
      return { mark: g.headlines.length, pick: { case: s.case, id: s.id, me: s.me, region: rep.region, place: rep.place, nat: rep.nat, defNat: rep.defNat, what: rep.what }, recording: JSON.stringify(staged) };
    }
  }
  return sub();
}

/** The bridge's TC.shoreScene(id): one report's scene, staged ({pick, recording}); recording null where it gets none (night, a gale, no such report). */
export function shoreSceneOf(g: Game, id: number): { pick: Record<string, unknown> | null; recording: string | null } {
  const rep = (g.shoreReports ?? []).find((r) => r.id === id);
  if (!rep) return { pick: null, recording: null };
  const f = shoreFactsOf(g, rep);
  const me = rep.nat === g.human ? 'att' : rep.defNat === g.human ? 'def' : 'att';
  const bar = barOf(f);
  const pick = { case: me === 'att' ? (countOf(sunkOf(rep)) ? 'shoreLoss' : 'shore') : 'shoreCoast', id, me, region: rep.region, place: rep.place, nat: rep.nat, defNat: rep.defNat, what: rep.what, ...(bar ? { skip: bar } : {}) };
  if (bar) return { pick, recording: null };
  return { pick, recording: JSON.stringify(stageShore(f, me)) };
}
