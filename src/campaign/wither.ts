// A cut-off island garrison withers, and is never a free prize (ruling 173(e) and its rider (o); ruling 181(f) and
// (g): "173(e) is a rule, not data"; the design director, 25 November 2026, form B9: "ruled and may be built behind
// the switch").
//
// In the Pacific no island garrison of any nation gave in to hunger alone: each fell to a landing or sat to the end.
// Before this, closing the seas round an island took it for nothing: the garrison laid down its arms in its third
// hungry month and the island stood empty. So on an island of the historian's table (islands.ts), on the whole-world
// chart alone and for every nation alike:
//
// - A garrison cut off there lives on its stores, as a corps cut off anywhere does: what its port keeps, and then
//   what it carries, the months the game has always counted (game.ts monthsUnfed: a fresh corps, three). While they
//   last hunger wears it as it always did, but never below the island's floor.
// - The month its stores are gone it has withered: it falls to the floor, a quarter of its strength and order on an
//   atoll and a half on an island large enough to feed itself (the table's own column, ruling 181(f)), and stays
//   there. It does not lay down its arms. It cannot attack, march or sail, and takes no drafts.
// - It mends when a ship of its side can reach it again: the month it is fed its count is struck off, and drafts and
//   rest come as to any corps.
// - It can be stormed like any corps: a landing meets what is left.
// - 173(e) speaks of an island with no enemy ashore, and rider (o)(3) carries the rule to a large island with the
//   enemy ashore. On an atoll with the enemy ashore, hunger is the siege's, as it always was.
// - Rider (o)(2): an air group whose field stands on such an island and has gone unfed flies nothing and counts for
//   nothing (the count ruling 184 already keeps, air.ts `unfed`): the men last by their stores, the aircraft do not.
//
// One number a corps is kept, in the game's flags under the corps' id, so the corps and the save keep their shape:
// `wither:<id>`, absent while it is fed, the months of its stores left while they last, and 0 once they are gone.
// With the far side off no island has a row, nothing here answers, and no flag is ever written.
//
// This file reads the islands' table and nothing of the rules, so that any module may ask it.

import { islandOf, type Island } from './islands';
import type { AirFleet, Corps, Game } from './types';

const key = (c: Corps): string => `wither:${c.id}`;

/** The table's word for an island's floor, as a card says it. */
export const floorWords = (isle: Island): string => (isle.floor >= 0.5 ? 'half' : 'a quarter');

/**
 * The island whose rule holds for a corps this month, or null where hunger is what it always was: ground that is no
 * island of the table, the far side off, and an atoll with the enemy ashore (173(e) is for an island with no enemy
 * ashore; rider (o)(3) carries it to a large island, one that can feed itself, with the enemy ashore). `ashore` is
 * the caller's: whether an enemy of the corps stands on its own province (game.ts enemyAshore).
 */
export function witherAt(g: Game, c: Corps, ashore: boolean): Island | null {
  const isle = islandOf(g, g.regions[c.reg]);
  return isle && (!ashore || isle.floor >= 0.5) ? isle : null;
}

/** True for a garrison whose stores are gone and that stands on an island of the table: it only defends (173(e)). */
export function withered(g: Game, c: Corps): boolean {
  return islandOf(g, g.regions[c.reg]) !== null && g.flags[key(c)] === 0;
}

/** The months of its stores a cut-off island garrison has left, 0 once they are gone; undefined where no count is kept. */
export const witherStores = (g: Game, c: Corps): number | undefined => g.flags[key(c)];

/**
 * A hungry month on an island of the table (game.ts startPhase, the unfed branch). The caller has already taken the
 * month's hunger from the corps as it does anywhere; `was` is its order and strength before that. `stores` is the
 * months its own stores last from the month it was cut off (asked only then), or for a fortress crew under the
 * fortress rows what its own clock said this month: 'held' while its works' stores last, 'starved' the month they
 * run out. Hunger never takes it below the island's floor, and the month its stores are gone it stands at the floor.
 * Returns 'withers' in that month, 'withered' in every month after it, 'lasting' before.
 */
export function witherMonth(g: Game, c: Corps, isle: Island, was: { org: number; str: number }, stores: number | 'held' | 'starved'): 'lasting' | 'withers' | 'withered' {
  // The floor is a share of the row's own full size (a brigade's garrison is a row that never fills past its size).
  const org = Math.min(was.org, isle.floor), str = Math.min(was.str, isle.floor * (c.full ?? 1));
  if (g.flags[key(c)] === 0) { c.org = org; c.str = str; return 'withered'; }
  c.org = Math.max(c.org, org);
  c.str = Math.max(c.str, str);
  if (stores === 'held') return 'lasting';
  const left = stores === 'starved' ? 0 : (g.flags[key(c)] ?? stores) - 1;
  if (left > 0) { g.flags[key(c)] = left; return 'lasting'; }
  g.flags[key(c)] = 0;
  c.org = org;
  c.str = str;
  return 'withers';
}

/** A month fed from outside: the garrison is restocked, and its count starts again the next time the sea closes. */
export function witherFed(g: Game, c: Corps): void {
  if (g.flags[key(c)] !== undefined) delete g.flags[key(c)];
}

/**
 * Rider (o)(2): true for a land air group grounded because its field stands on an island of the table and has gone
 * unfed. The count is the one ruling 184 keeps as its nation's month opens (air.ts airUpkeep, the far side's skies
 * only), so the aircraft stop the month after the sea closes while the men last by their stores. Never a carrier's wing.
 */
export const fieldWithered = (g: Game, f: AirFleet): boolean => !!f.unfed && f.ship === undefined && islandOf(g, g.regions[f.base]) !== null;
