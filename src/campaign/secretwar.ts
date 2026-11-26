// The secret war (rulings 151(c), 135 and 153; the design director's b482, item 4): spy hunters and stolen orders,
// each half behind its own switch in data/campaign/intel.json 'secret', kept with each campaign in g.flags
// ('sw:hunt', 'sw:orders'). The grand campaign only, and only ever against or for the player: the computer's staffs
// keep no agents, so a war nobody plays is fought as before.
//
// This file holds the switches and the stolen orders' book, which both the secret service (intel.ts) and the air
// staff (air.ts) read; the agents' work itself is in intel.ts. The hooks: intel.ts (the missions, the hunt, the
// arrests, the plots), air.ts (the staff flies a read order first; fighters meet it in full), staff.ts (the switches).

import data from '../../data/campaign/intel.json';
import { headline } from './game';
import type { Game, StolenOrder } from './types';

const S = (data as unknown as { secret: { hunters: { on: boolean }; orders: { on: boolean; ambush: number } } }).secret;

/** True in a grand campaign begun with spy hunters (ruling 151(c)). */
export const huntersOn = (g: Game): boolean => g.flags.open === 1 && g.flags['sw:hunt'] === 1;
/** True in a grand campaign begun with stolen orders (ruling 135). */
export const ordersOn = (g: Game): boolean => g.flags.open === 1 && g.flags['sw:orders'] === 1;

/** Sets the switches for a grand campaign as it opens, from the data: a campaign keeps the settings it began with. */
export function openSecretWar(g: Game): void {
  if (S.hunters.on) g.flags['sw:hunt'] = 1;
  if (S.orders.on) g.flags['sw:orders'] = 1;
}

/** For tests and probes: one half of the secret war on or off in this campaign. */
export function setSecretWar(g: Game, part: 'hunt' | 'orders', on: boolean): void {
  if (on) g.flags[`sw:${part}`] = 1;
  else delete g.flags[`sw:${part}`];
}

// ---- The stolen orders' book ---------------------------------------------------------------------------------

/** The orders read of this staff that fall due this month and are not yet flown or lapsed. */
export const ordersDue = (g: Game, nat: string): StolenOrder[] =>
  ordersOn(g) ? (g.intel.orders ?? []).filter((o) => o.nat === nat && o.turn === g.turn && !o.done) : [];

/** The orders the player's side has read that are still to come or came this month: THE ENEMY'S ORDERS. */
export const ordersKnown = (g: Game, viewer: string | null = g.human): StolenOrder[] =>
  viewer && ordersOn(g) ? (g.intel.orders ?? []).filter((o) => o.by === viewer && o.turn >= g.turn) : [];

/** An order that cannot be flown as written: the player is told, and why (ruling 135(d)). */
export function orderLapsed(g: Game, o: StolenOrder, why: string): void {
  o.done = 'lapsed';
  const t = g.regions[o.target];
  headline(g, 'info', `The order against ${t?.name ?? 'the target'} was cancelled: ${why}.`, o.by, o.target);
}

/** While a read order is flown, its target: fighters that meet it meet it in full (ruling 135(c)). */
export const ambushAt = (g: Game, target: number): boolean => g.flags['ord:fly'] === target + 1;
export const AMBUSH = (): number => S.orders.ambush;

/** Marks the raid now flying as a read order, or clears the mark. */
export function flyingOrder(g: Game, target: number | null): void {
  if (target === null) delete g.flags['ord:fly'];
  else g.flags['ord:fly'] = target + 1;
}

/** Old orders are struck from the book once their month is past. */
export function pruneOrders(g: Game): void {
  if (g.intel.orders) g.intel.orders = g.intel.orders.filter((o) => o.turn >= g.turn - 1);
}
