// The men as dots on the 2D field page (the true-scale plan, GR5; the production lead's option A of 22 November
// 08:03Z: "cheap dots (men0/N per block, thinning with strength, no drill)"). Every block shows its men as plain dots,
// one for every N men it brings, at the page's own one N (LAND.scale.page) shared by both sides, so twice the men is
// twice the dots. As a block loses strength its dots thin, the last laid first gone, so the rest stay evenly spread.
// Looks only, and no drill: the page reads the sim's blocks and nothing more. This file decides counts and places; the
// page's draw.ts lays them on each block.
//
// Fog (b464(2)): the page's N counts every block as an average one of the player's own side, and a block of the enemy
// he has not met shows that plain count, unthinned, as it shows a plain box; so neither N nor a dot gives away what
// he cannot know.

import type { Side } from '../core/types';
import { LAND } from '../land/rules';
import type { LandBattle } from '../land/sim';
import type { LandUnit } from '../land/units';

/** The page's one N and the plain men of a block he cannot know (his own side's average). */
export interface PageScale { N: number; plain: number }

/** The page's one N for this battle, seen from the player's side (me): the men both sides bring over LAND.scale.page. */
export function pageScale(b: LandBattle, me: Side): PageScale {
  const mine = b.units.filter((u) => u.side === me);
  const plain = mine.reduce((s, u) => s + (Number.isFinite(u.men0) ? Math.max(0, u.men0) : 0), 0) / Math.max(1, mine.length);
  return { N: b.fog ? b.scale(() => plain, LAND.scale.page) : b.scale(undefined, LAND.scale.page), plain };
}

/**
 * The dots a block shows: its men over N, thinned by the strength it has left of what it came with, rounded to the
 * nearest; at least one while it stands. An enemy block he has not met: the plain count, unthinned.
 */
export function dotsOf(b: LandBattle, u: LandUnit, sc: PageScale): number {
  if (!b.known(u)) return Math.max(1, Math.round(sc.plain / sc.N));
  const men = Number.isFinite(u.men0) ? Math.max(0, u.men0) : 0;
  const left = u.str0 > 0 ? Math.max(0, Math.min(1, u.str / u.str0)) : 1;
  return Math.max(1, Math.round((men / sc.N) * left));
}

/**
 * Where a block's i-th dot lies in its box, each part 0 to 1 across (x, west to east as the block faces) and down
 * (y, front to back): an even spread (the R2 sequence), each block's begun at its own place so no two look alike. Any
 * first n of them are evenly spread too, so a block thinning from the last keeps its look.
 */
export function dotAt(id: number, i: number): [number, number] {
  const a = (id * 0.6180339887498949) % 1;
  return [(0.5 + a + i * 0.7548776662466927) % 1, (0.5 + a * 0.5 + i * 0.5698402909980532) % 1];
}
