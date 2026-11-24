// The secret war (rulings 151(c), 135 and 153; the design director's b482, item 4): spy hunters and stolen orders,
// each half behind its own switch in data/campaign/intel.json 'secret', kept with each campaign in g.flags
// ('sw:hunt', 'sw:orders'); and (Brian, 22 November, release 2) every country at war with the player spying on
// him, 'sw:all'; and (Brian, 22 November, release 2, yes (3)) double agents both ways, with a telltale sign, 'sw:double';
// and (Brian, 22 November, release 2: 'Busier, mostly watchers') enemy agents watching his front, 'sw:watch'.
// The grand campaign only, and only ever against or for the player: the computer's staffs
// keep no agents, so a war nobody plays is fought as before.
//
// This file holds the switches and the stolen orders' book, which both the secret service (intel.ts) and the air
// staff (air.ts) read; the agents' work itself is in intel.ts. The hooks: intel.ts (the missions, the hunt, the
// arrests, the plots), air.ts (the staff flies a read order first; fighters meet it in full), staff.ts (the switches).

import data from '../../data/campaign/intel.json';
import { headline } from './game';
import type { Game, StolenOrder } from './types';

export interface DoublesRules { on: boolean; most: number; blown: number; share: number; lie: number; betray: number; home: number }
export interface WatchersRules { on: boolean; chance: number; attack: number; ready: number; reserve: number }
export interface NetworkRules { on: boolean; great: number; small: number }
const S = (data as unknown as { secret: { hunters: { on: boolean }; orders: { on: boolean; ambush: number }; everyone: { on: boolean; smaller: number }; watchers: WatchersRules; doubles: DoublesRules; network: NetworkRules } }).secret;

/** True in a grand campaign begun with spy hunters (ruling 151(c)). */
export const huntersOn = (g: Game): boolean => g.flags.open === 1 && g.flags['sw:hunt'] === 1;
/** True in a grand campaign begun with stolen orders (ruling 135). */
export const ordersOn = (g: Game): boolean => g.flags.open === 1 && g.flags['sw:orders'] === 1;
/**
 * True in a grand campaign begun with every country's service at work against the player (Brian, 22 November,
 * release 2: every country the computer plays runs real spies against the player, sized to the country).
 */
export const everyoneOn = (g: Game): boolean => g.flags.open === 1 && g.flags['sw:all'] === 1;
/**
 * True in a grand campaign begun with double agents (Brian, 22 November, release 2, yes (3): double agents, with a
 * telltale sign so a false report is never unfair). The player can turn an enemy agent his hunters uncovered, and the
 * enemy can turn one of his agents abroad.
 */
export const doublesOn = (g: Game): boolean => g.flags.open === 1 && g.flags['sw:double'] === 1;
/**
 * True in a grand campaign begun with enemy watchers (Brian, 22 November, release 2: 'Busier, mostly watchers'): the
 * other side's services put agents on the player's front that map its defences for their attacks. A watcher is one
 * of the hidden agents the spy hunters search for, so it needs them on too.
 */
export const watchersOn = (g: Game): boolean => huntersOn(g) && g.flags['sw:watch'] === 1;
/**
 * True in a grand campaign begun with a service in being (the spies thread, 24 November 2026, release 2: Brian played
 * and saw no spies, because a campaign opened with none and they waited behind the SPIES door). The player's service
 * opens the war with a few agents already at work, standing on the map from the first month (intel.ts layNetwork).
 */
export const networkOn = (g: Game): boolean => g.flags.open === 1 && g.flags['sw:net'] === 1;
/** The network's numbers (intel.json secret.network): how many agents a great power's service and a smaller one's open with. */
export const NETWORK = (): NetworkRules => S.network;
/** The watchers' numbers (intel.json secret.watchers). */
export const WATCHERS = (): WatchersRules => S.watchers;
/** The double agents' numbers (intel.json secret.doubles). */
export const DOUBLES = (): DoublesRules => S.doubles;
/** With everyoneOn: the share of a great power's rate at which a smaller nation's service plots. */
export const SMALLER = (): number => S.everyone.smaller;

/** Sets the switches for a grand campaign as it opens, from the data: a campaign keeps the settings it began with. */
export function openSecretWar(g: Game): void {
  if (S.hunters.on) g.flags['sw:hunt'] = 1;
  if (S.orders.on) g.flags['sw:orders'] = 1;
  if (S.everyone.on) g.flags['sw:all'] = 1;
  if (S.doubles.on) g.flags['sw:double'] = 1;
  if (S.watchers.on) g.flags['sw:watch'] = 1;
  if (S.network.on) g.flags['sw:net'] = 1;
}

/**
 * (Release 2, the spies thread of 24 November 2026.) The secret war as the options page shows it, one switch for all of
 * release 2's spying: the network the service opens with, the hunt for enemy agents, their watchers, every country's
 * service at work, stolen orders and double agents. Off, the secret service is release 1's: agents recruited, sent,
 * watching, reading books and wrecking, and the enemy's saboteurs a chance each month.
 */
export const SECRET_PARTS = ['hunt', 'orders', 'all', 'double', 'watch', 'net'] as const;
export type SecretPart = (typeof SECRET_PARTS)[number];
/** True where any part of release 2's secret war is on in this campaign. */
export const secretWarOn = (g: Game): boolean => g.flags.open === 1 && SECRET_PARTS.some((p) => g.flags[`sw:${p}`] === 1);
/** The secret war can be switched in a grand campaign's opening month only: its services are at work from the first month's end. */
export const secretWarSettable = (g: Game): boolean => g.flags.open === 1 && g.turn === (g.flags.openFrom ?? 1);

/** For tests and probes: one half of the secret war on or off in this campaign. */
export function setSecretWar(g: Game, part: SecretPart, on: boolean): void {
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
