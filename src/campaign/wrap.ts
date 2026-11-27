// The world is round. Once the far side is on the chart (stage.ts) the map runs all the way round it, from the seam
// at 141 W back to the seam, and a place at the map's east end (Hawaii, Alaska) lies just west of one at its west end
// (California, the Yukon): eight pairs of seas are neighbours across the seam. A straight line between two places
// is then the shorter of the two ways round. On any narrower chart (Europe, the Americas) nothing here changes a
// number.

import americasMap from '../generated/world-americas.json';
import wholeMap from '../generated/world-whole.json';
import type { Game } from './types';

/** The width of the whole world in map units: the same on every stage of the chart. */
export const WORLD_WIDTH = (americasMap as unknown as { world: { width: number } }).world.width;

/** The width of the world where the game's chart goes all the way round it, else 0. */
export const wrapSpan = (g: Game): number => (g.flags.world === 1 && g.width >= WORLD_WIDTH - 0.5 ? WORLD_WIDTH : 0);

/** How far apart two points of the map lie east and west: the shorter way round where the chart closes on itself. */
export function dxOf(g: Game, ax: number, bx: number): number {
  const dx = Math.abs(ax - bx), span = wrapSpan(g);
  return span > 0 && dx > span / 2 ? span - dx : dx;
}

/** The straight-line distance between two points of the map. */
export const mapDist = (g: Game, ax: number, ay: number, bx: number, by: number): number => Math.hypot(dxOf(g, ax, bx), ay - by);

/** The square of it, for a search for the nearest. */
export const mapDist2 = (g: Game, ax: number, ay: number, bx: number, by: number): number => dxOf(g, ax, bx) ** 2 + (ay - by) ** 2;

// The world is round the other way too. The chart draws a degree of longitude everywhere as long as it is at the
// parallel of 50 north, so the map's straight line is true north and south and true east and west only there: in the
// tropics it reads about two-thirds of the distance on the globe (Phnom Penh to Manila 1,198 km for 1,774), and in
// the Arctic too much. The rules' ranges are true kilometres. So on a chart that goes all the way round the world
// (the far side on) every reader of distance reads the globe's wherever one of its two provinces is on the far side:
// the air war's kilometre (air.ts, km) and every search for the nearest place by a straight line. On any narrower
// chart nothing here changes a number. (The design director's rulings of 04:41Z, 04:47Z and 08:55Z, 22 November
// 2026, item R. The whole chart on the globe is after release, at the old chart's own gate: read over Europe it
// moves Europe's air war from the second month, and the book's marks with it. Until then an old-chart base reads far
// targets on the globe and old ones on the map: the seam is the far switch's, and known.)

/**
 * The chart's projection, as its builder wrote it into the chart (tools/build-geo-world.mjs, `world`): `scale` units
 * a degree of latitude southward from `north`, and a degree of longitude drawn everywhere as long as it is at
 * `parallel`. No rule carries the figures.
 */
const CHART = (americasMap as unknown as { world: { north: number; scale: number; parallel: number } }).world;
const RAD = Math.PI / 180;
/** Map units to a degree of longitude. */
const KX = CHART.scale * Math.cos(CHART.parallel * RAD);

/** The latitude of a point of the chart, in degrees north. */
export const latOf = (y: number): number => CHART.north - y / CHART.scale;

/**
 * The globe's distance between two points of the chart, in the chart's own units: a degree of the great circle is
 * as many of them as a degree of latitude is on the map. So due north and south it is the map's straight line, and
 * east and west it is the true distance, which the map draws right only at its parallel. The shorter way round.
 */
export function globeDist(g: Game, ax: number, ay: number, bx: number, by: number): number {
  const la = latOf(ay) * RAD, lb = latOf(by) * RAD, dl = (dxOf(g, ax, bx) / KX) * RAD;
  const h = Math.sin((la - lb) / 2) ** 2 + Math.cos(la) * Math.cos(lb) * Math.sin(dl / 2) ** 2;
  return ((2 * Math.asin(Math.min(1, Math.sqrt(h)))) / RAD) * CHART.scale;
}

/** A province of the chart, as these read it. */
export interface Spot { id: number; x: number; y: number }

/**
 * The first province of the far side, read from the whole chart as world.ts reads it (FAR_FIRST: the same figure;
 * this file stands under world.ts and cannot ask it).
 */
const WHOLE = wholeMap as unknown as { first: number; stages: number[] };
const FAR_FROM = WHOLE.first + WHOLE.stages[1];

/** True where the chart goes all the way round the world (the far side on). */
export const globeOn = (g: Game): boolean => wrapSpan(g) > 0;

/**
 * True where the rules read the globe's distance between two provinces: on the chart that goes all the way round
 * the world, when either of the two is a province of the far side. Between two provinces of the old chart (Europe,
 * the Americas) the map's straight line is read as it always was: the old chart's own form waits for its own gate.
 */
export const globeFor = (g: Game, a: Spot, b: Spot): boolean => globeOn(g) && (a.id >= FAR_FROM || b.id >= FAR_FROM);

/** The distance between two provinces as the rules read it: the globe's where `globeFor`, else the map's straight line. */
export const chartDist = (g: Game, a: Spot, b: Spot): number => (globeFor(g, a, b) ? globeDist(g, a.x, a.y, b.x, b.y) : mapDist(g, a.x, a.y, b.x, b.y));

/** The square of it, for a search for the nearest. */
export const chartDist2 = (g: Game, a: Spot, b: Spot): number => (globeFor(g, a, b) ? globeDist(g, a.x, a.y, b.x, b.y) ** 2 : mapDist2(g, a.x, a.y, b.x, b.y));

/**
 * True where the rules read the globe for the reach of a point to a straight leg between two provinces (air.ts legKmAt:
 * the ferry lane's legs): as globeFor, when either end of the leg or the point is of the far side. The point is a
 * province (region >= 0), or a sea where it is none (a carrier wing at sea).
 */
export const globeLeg = (g: Game, a: Spot, b: Spot, region: number, sea = -1): boolean =>
  globeOn(g) && (a.id >= FAR_FROM || b.id >= FAR_FROM || (region >= 0 ? region >= FAR_FROM : sea >= FAR_SEA_FROM));

/** The first sea of the far side: the whole chart keeps the old chart's seas first, in the old chart's order. */
const FAR_SEA_FROM = (americasMap as unknown as { seas: { zones: unknown[] } }).seas.zones.length;

/**
 * The distance between a province and the middle of a sea as the rules read it: the globe's where the province or
 * the sea is of the far side, else the map's straight line. (A sea's number is its own count, not a province's.)
 */
export const seaDist = (g: Game, a: Spot, sea: Spot): number => (globeOn(g) && (a.id >= FAR_FROM || sea.id >= FAR_SEA_FROM) ? globeDist(g, a.x, a.y, sea.x, sea.y) : mapDist(g, a.x, a.y, sea.x, sea.y));

/**
 * How much wider than it is tall the chart draws a circle of the globe round a province of the far side: the stretch
 * of a degree of longitude at its latitude against the chart's parallel. 1 round a province of the old chart, and
 * where the chart does not go all the way round.
 */
export const ringStretch = (g: Game, at: Spot): number => (globeOn(g) && at.id >= FAR_FROM ? Math.cos(CHART.parallel * RAD) / Math.max(0.05, Math.cos(latOf(at.y) * RAD)) : 1);

/** An x moved by the width of the world, if need be, to lie as near as may be to another: where to draw it beside that one. */
export function nearX(g: Game, x: number, to: number): number {
  const span = wrapSpan(g);
  if (span <= 0) return x;
  while (x - to > span / 2) x -= span;
  while (to - x > span / 2) x += span;
  return x;
}
