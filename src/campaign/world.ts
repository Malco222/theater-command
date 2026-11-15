// The wider world.
//
// The history campaigns are fought on the map of Europe and never see any of this. A grand campaign opens on a
// wider chart: the same sheet carried on westward, on the same scale, to the Pacific coast of North America. The
// new provinces follow Europe's in number, the new seas follow Europe's seas, and Europe's own keep the numbers
// they have, so nothing that names a province or a sea has to change.
//
// What is attached: the provinces and seas of tools/build-geo-world.mjs, the countries that exist only out there
// (Canada, Mexico, Cuba), and the United States at home. Until now America was a country beyond the map's edge
// that woke in December 1941 with its industry out of reach. On the wider chart it is there from the first day:
// neutral, its capital at Washington, its factories on its own soil, its army a cadre.
//
// The world is attached once, as the grand campaign opens (staff.ts), before anything else is set up for it, and
// again whenever a campaign saved on it is loaded (save.ts).

import worldMap from '../generated/world-map.json';
import data from '../../data/campaign/world.json';
import { addCorps, allied, armySize, capitalDistances, computeSupply, headline, nationOf, regionOf, setWar } from './game';
import { addFleet, chartSeas, type FleetRow } from './navy';
import { newLab, researches } from './research';
import type { Game, Region } from './types';

interface WorldMap {
  frame: { x0: number; y0: number; x1: number; y1: number };
  /** The whole world's own coordinates: world x = (x + shift) mod width; the seam is the longitude where world x is 0. */
  world: { width: number; height: number; shift: number; seam: number };
  first: number;
  land: [number, number][][][];
  lakes: { name: string; level: number; x: number; y: number; outline: number[][] }[];
  regions: any[];
  links: [number, number][];
  seas: { zones: any[]; gates: any[]; borders: [number, number][][] };
}

export const WORLD_MAP = worldMap as unknown as WorldMap;

export const WORLD = data as unknown as {
  nations: Record<string, any>;
  home: Record<string, { align?: string }>;
  follow: Record<string, string>;
  corps: [string, string, string, number, string, { xp?: number; str?: number; hold?: number }][];
  fleets: FleetRow[];
  peace: Record<string, [number, number][]>;
  protector: string[];
  pairs: [string, string][];
  order: string[];
  great: string[];
  oilHome: Record<string, number>;
  idle: Record<string, number>;
};

/** The first province beyond the map of Europe, and how many there are. */
export const WORLD_FIRST = WORLD_MAP.first;
export const WORLD_REGIONS = WORLD_MAP.regions.length;

/** True for a campaign played on the wider world. */
export const worldOn = (g: Game): boolean => g.flags.world === 1;

/** True for a province beyond the map of Europe. */
export const beyondEurope = (r: Region | number): boolean => (typeof r === 'number' ? r : r.id) >= WORLD_FIRST;

/** The great lakes cut out of the land: their names and where to write them. None is a sea. */
export const lakesOf = (g: Game): WorldMap['lakes'] => (worldOn(g) ? WORLD_MAP.lakes : []);

/**
 * Attaches the wider world to a game on the map of Europe. Safe to call twice: the second call does nothing.
 * 'fresh' is false when a saved campaign is being laid over it (save.ts): the armies, fleets and wars of the first
 * day are then left out, since the save brings its own.
 */
export function attachWorld(g: Game, fresh = true): void {
  if (g.flags.world === 1 || g.regions.length !== WORLD_FIRST) return;
  g.flags.world = 1;
  // The provinces, numbered on from Europe's.
  for (const row of WORLD_MAP.regions) {
    const r = regionOf(row);
    g.regions.push(r);
    g.byName[r.name] = r;
    // A depot is a home province that can feed an army cut off from its capital.
    if (row.flags.depot) g.flags['depot:' + r.id] = 1;
  }
  // The chart itself: the coasts, the ferries, and the frame the interfaces draw in.
  g.links = [...g.links, ...WORLD_MAP.links];
  g.land = [...WORLD_MAP.land, ...g.land];
  g.x0 = WORLD_MAP.frame.x0;
  g.width = WORLD_MAP.frame.x1 - WORLD_MAP.frame.x0;
  g.height = Math.max(g.height, WORLD_MAP.frame.y1 - WORLD_MAP.frame.y0);
  // The seas: Europe's keep their numbers and gain the neighbours they have to the west; the oceans follow.
  chartSeas(g, WORLD_MAP.seas);
  // The countries that exist only out here take their place at the end of the order of play.
  for (const id of WORLD.order) {
    const d = WORLD.nations[id];
    if (!d || g.nations[id]) continue;
    const cap = g.regions.find((r) => r.owner === id && r.cap);
    const n = g.nations[id] = nationOf(id, d, cap ? cap.id : -1);
    n.lab = researches(id) ? newLab() : null;
    if (!g.order.includes(id)) g.order.push(id);
  }
  // And the countries whose homeland was beyond the map come home to it.
  for (const [id, h] of Object.entries(WORLD.home)) {
    const n = g.nations[id];
    const cap = g.regions.find((r) => r.owner === id && r.cap);
    if (!n || !cap) continue;
    n.capital = cap.id;
    n.offmap = null;
    // Still standing aside: it is there, and at peace. One already in the war (a campaign begun later) stays as it is.
    if (n.dormant) {
      n.dormant = false;
      n.alive = true;
      if (h.align) n.align = h.align as typeof n.align;
    }
  }
  // Every nation's distances to its capital are counted again on the wider map.
  for (const n of Object.values(g.nations)) if (n.capital >= 0) g.capDist[n.id] = capitalDistances(g, n.capital);
  // The supply picture keeps every country's place in it, widened to the new provinces: they are unsupplied until
  // it is drawn again.
  for (const [k, v] of Object.entries(g.supply)) { const w = new Uint8Array(g.regions.length); w.set(v); g.supply[k] = w; }
  g.reach = {};
  g.railhead = {};
  g.supplyDirty = true;
  // A saved campaign's picture is laid over one drawn here, with every country in its usual place in it. (A new
  // campaign's is drawn once everything else is set up: staff.ts.)
  if (!fresh) { computeSupply(g); return; }
  // The armies standing at home on the first day: peacetime cadres.
  for (const [nat, name, kind, divs, at, o] of WORLD.corps) {
    if (!g.nations[nat]?.alive || !g.byName[at] || g.byName[at].ctrl !== nat) continue;
    const c = addCorps(g, nat, name, kind as any, divs, at, o);
    if (o.str !== undefined) c.str = o.str;
  }
  // The squadrons in home waters.
  for (const row of WORLD.fleets) if (g.nations[row.nat]?.alive && g.byName[row.base ?? '']?.ctrl === row.nat) addFleet(g, row);
  // A dominion is in its mother country's wars from the day they began.
  for (const [id, of] of Object.entries(WORLD.follow)) {
    const n = g.nations[id], M = g.nations[of];
    if (!n?.alive || !M?.alive) continue;
    for (const e of M.atWar) if (g.nations[e]?.alive) setWar(g, id, e);
  }
  for (const n of Object.values(g.nations)) if (n.alive) n.armyPeak = Math.max(n.armyPeak, armySize(g, n.id));
}

/**
 * The wider world's month: a dominion follows its mother country into every war she is in, while the two stand in
 * the same alliance. One in the player's hands goes to war only when its player says so.
 */
export function worldMonth(g: Game): void {
  if (!worldOn(g) || (g.flags.dom ?? 0) > 0) return;
  for (const [id, of] of Object.entries(WORLD.follow)) {
    const n = g.nations[id], M = g.nations[of];
    if (!n?.alive || !M?.alive || id === g.human || !allied(g, id, of)) continue;
    for (const e of M.atWar) {
      const E = g.nations[e];
      if (!E?.alive || n.atWar.includes(e) || allied(g, id, e)) continue;
      setWar(g, id, e);
      headline(g, 'war', `${n.name} is at war with ${E.name}, beside ${M.name}.`, id);
    }
  }
}

/** The share of a country's industry on war work while it is at peace, where the wider world gives it one of its own, or null where the ordinary rule stands. */
export function peaceFooting(g: Game, nat: string): number | null {
  const rows = worldOn(g) ? WORLD.peace[nat] : undefined;
  if (!rows) return null;
  let share = rows[0][1];
  for (const [from, s] of rows) if (g.turn >= from) share = s;
  return share;
}

/** What a country's small wells and plants at home yield, where the wider world puts its oilfields on the map; null where the ordinary figure stands. */
export const oilAtHome = (g: Game, nat: string): number | null => (worldOn(g) ? WORLD.oilHome[nat] ?? null : null);

/**
 * True for a computer-run staff across the ocean from every enemy that already has as many field corps at home as
 * it has any use for: it raises no more to stand on the quay. An enemy ashore on its side of the water ends that.
 */
export function armyIdle(g: Game, nat: string): boolean {
  if (!worldOn(g) || nat === g.human) return false;
  const n = g.nations[nat];
  if (!n || n.capital < 0 || !beyondEurope(n.capital)) return false;
  // Corps on order count as if they had already formed.
  let home = n.queue.reduce((s, b) => s + (b.what === 'corps' && b.kind !== 'eng' ? 1 : 0), 0);
  for (let i = WORLD_FIRST; i < g.regions.length; i++) {
    const r = g.regions[i];
    if (n.atWar.includes(r.ctrl)) return false;
    for (const c of r.units) if (c.nat === nat && c.kind !== 'fort' && c.kind !== 'eng') home++;
    for (const c of r.garr ?? []) if (c.nat === nat && c.kind !== 'fort' && c.kind !== 'eng') home++;
  }
  return home >= (WORLD.idle[nat] ?? WORLD.idle._default);
}

/**
 * Who takes over a fallen country's ground beyond the old chart: islands and colonies an ocean away from the
 * victor's armies. The first power on the list that is alive, lives by its ships and is at war with the victor;
 * null where there is none, and the ground goes with the rest.
 */
export function protectorOf(g: Game, by: string): string | null {
  if (!worldOn(g)) return null;
  for (const id of WORLD.protector) {
    const n = g.nations[id];
    if (n?.alive && n.seaSupply && n.atWar.includes(by)) return id;
  }
  return null;
}
