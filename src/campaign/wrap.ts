// The world is round. Once the far side is on the chart (stage.ts) the map runs all the way round it, from the seam
// at 141 W back to the seam, and a place at the map's east end (Hawaii, Alaska) lies just west of one at its west end
// (California, the Yukon): eight pairs of seas are neighbours across the seam. A straight line between two places
// is then the shorter of the two ways round. On any narrower chart (Europe, the Americas) nothing here changes a
// number.

import americasMap from '../generated/world-americas.json';
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

/** An x moved by the width of the world, if need be, to lie as near as may be to another: where to draw it beside that one. */
export function nearX(g: Game, x: number, to: number): number {
  const span = wrapSpan(g);
  if (span <= 0) return x;
  while (x - to > span / 2) x -= span;
  while (to - x > span / 2) x += span;
  return x;
}
