// What is on order, place by place (Version 78; the design director's ruling 46). Brian, 2026-11-15: "i want it to
// be clear that you can see what city is producing what unit and how many at a time and how many turns until they
// are ready to be deployed". Display only: nothing here changes a game. One call serves both interfaces
// (`cityOrders`), with one more for a single place (`ordersAt`).
//
// An order is a unit (a corps, a wing of aircraft, ships) or a work (fortifications, a factory, a shipyard, guns,
// airfields, a level of a town's building). Units come first in every list: they are what the map's badge shows.
// Aircraft and ships are shown at the place the rules record for them: the airfield a wing is ordered for and the
// yard a ship is laid down in. "How many at a time" is the number forming at a place now; no place has a limit.
// A turn is a month, so every figure here is in months.
//
// The badge (rulings 46 and 57): one a city, never one an order. Its sign is the unit ready soonest; its number
// counts the orders there of exactly what the sign shows, and a plus says other kinds of unit are on order too. A
// sign and its number always describe the same thing: destroyers, cruisers and battleships share the hull and count
// together, and a nation's special formations (the starred sign) count apart from plain corps of their kind. The
// whole number forming at the place is the first thing its list says.

import { ARSENAL, workLabel } from './arsenal';
import { CIVIC } from './buildings';
import { PRODUCTION } from './economy';
import { agentAt } from './intel';
import { townName } from './towns';
import { eliteOf } from './traits';
import { KIND, type Build, type Game } from './types';
import { turnToDate } from '../core/ticket';

/** One order at a place. */
export interface OrderLine {
  /** The order's own number: what cancelling it asks for. */
  id: number;
  what: Build['what'];
  /** A corps, a wing or ships: something that will take orders. False for works. */
  unit: boolean;
  /**
   * The key of its sign. A corps: its kind ('inf', 'arm', 'mot', 'cav', 'mtn', 'art', 'eng'), or 'para' for airborne
   * troops. Aircraft: 'ftr' or 'bmr'. Ships: the class ('dd', 'ss', 'tp', 'ca', 'cv', 'bb'). Works: 'fort', 'factory',
   * 'yard', 'aa', 'field', and 'civic' for a town's building.
   */
  sign: string;
  /** What kind of thing it is, in words, one of them and several: 'infantry corps', 'fighter wing' and 'fighter wings'. */
  label: string;
  plural: string;
  /** The order's own name: 'IV Corps', 'Fighter wing for Kent'. */
  name: string;
  /** How many aircraft or ships the one order holds (a hundred fighters, four destroyers); 1 for a corps or a work. */
  holds: number;
  /** One of the nation's special formations: its sign carries the star. */
  elite: boolean;
  /** The sign the badge draws for it: `sign`, but 'hull' for destroyers, cruisers and battleships alike. Empty for a work. */
  mark: string;
  /** The month it was ordered and the month it is ready, as turns, and that month in words. */
  ordered: number;
  eta: number;
  ready: string;
  /** Months until it is ready, the months the order takes in all, and the months done. */
  months: number;
  need: number;
  done: number;
  /** The province it will stand in when ready. (A corps whose place is full or lost forms at the capital instead.) */
  stands: number;
  cost: number;
  /** True while the order can still be cancelled for its price: the month it was placed. */
  fresh: boolean;
}

/** Everything on order at one place. */
export interface CityOrders {
  region: number;
  /** The province, and its city or town. */
  province: string;
  town: string;
  /** Whose orders they are; and true where the viewer knows of them only from an agent in the place. */
  holder: string;
  spied: boolean;
  /** Units first, each kind soonest first. */
  lines: OrderLine[];
  /** How many units are forming here now, and how many works are under way. */
  units: number;
  works: number;
  /**
   * The same by kind, soonest first, for a line such as "2 infantry corps, 1 fighter wing": each kind with the sign
   * the badge would draw for it (`mark`, empty for a work), whether it is starred, and how many are on order here.
   */
  kinds: { sign: string; mark: string; starred: boolean; label: string; plural: string; count: number; unit: boolean; months: number }[];
  /** The next order ready: a unit where any is forming (the badge's unit), otherwise the next work. And the months to it. */
  next: OrderLine;
  months: number;
  /**
   * The map's badge, or null where only works are under way: the sign of the unit ready soonest (and whether it is
   * starred), how many orders here are of exactly that sign, whether other kinds of unit are on order too, and the
   * months to the first of them.
   */
  badge: { mark: string; starred: boolean; count: number; plus: boolean; months: number } | null;
  /** "3 on order: 2 infantry corps, 1 fighter wing. Next ready in 2 months." */
  summary: string;
  /** Every order in a line each, for a tooltip. */
  list: string[];
}

const low = (s: string): string => s.charAt(0).toLowerCase() + s.slice(1);
const several = (s: string): string => (/(corps|ships|guns|airfields|searchlights|fortifications|troops)$/.test(s) ? s : /y$/.test(s) ? `${s.slice(0, -1)}ies` : `${s}s`);
/** "in 3 months", "in 1 month", "this month". */
export const inMonths = (m: number): string => (m <= 0 ? 'this month' : `in ${m} month${m === 1 ? '' : 's'}`);

function lineOf(g: Game, b: Build, nat: string): OrderLine {
  let sign = 'civic', label = 'town building', holds = 1;
  const unit = b.what === 'corps' || b.what === 'air' || b.what === 'ship';
  if (b.what === 'corps') {
    sign = b.airborne ? 'para' : b.kind ?? 'inf';
    // A nation's special formation goes by its own name: "heavy tank corps", "Commonwealth corps".
    const own = b.elite ? eliteOf(nat) : null;
    label = b.airborne ? 'airborne corps' : own && own.id === b.elite ? (/^(Commonwealth|Guards|Ardennes|Evzone|Legion|Alpine)\b/.test(own.label) ? own.label : low(own.label)) : low(PRODUCTION.builds[b.kind ?? 'inf']?.label ?? (ARSENAL as unknown as { land?: Record<string, { label: string }> }).land?.[b.kind ?? 'inf']?.label ?? (ARSENAL as unknown as { teams?: Record<string, { label: string }> }).teams?.[b.kind ?? 'inf']?.label ?? `${KIND[b.kind ?? 'inf']?.label ?? 'Infantry'} corps`);
  } else if (b.what === 'air') {
    sign = b.item ?? 'ftr';
    label = low(ARSENAL.air[sign as keyof typeof ARSENAL.air]?.label ?? 'air wing');
    holds = b.count ?? 1;
  } else if (b.what === 'ship') {
    sign = b.item ?? 'dd';
    label = low(ARSENAL.ships[sign as keyof typeof ARSENAL.ships]?.label ?? 'ships');
    holds = b.count ?? 1;
  } else if (b.what === 'fort') {
    sign = 'fort';
    label = 'fortifications';
  } else if (b.what === 'work') {
    sign = b.item ?? 'factory';
    // (One switch-aware name: with airfields as targets a field level is a concrete airfield or a field strip.)
    label = low(workLabel(g, sign, b.kind));
  } else {
    const line = CIVIC.lines.find((x) => x.id === b.item);
    label = line ? `${low(line.name)} building` : 'town building';
  }
  const need = Math.max(1, b.eta - b.ordered);
  const months = Math.max(0, b.eta - g.turn);
  return {
    id: b.id, what: b.what, unit, sign, label, plural: several(label), name: b.name, holds, elite: !!b.elite,
    // (A work has no sign of its own but one: anti-aircraft guns take the art director's gun sign.)
    mark: !unit ? (b.what === 'work' && sign === 'aa' ? 'aa' : '') : sign === 'dd' || sign === 'ca' || sign === 'bb' ? 'hull' : sign,
    ordered: b.ordered, eta: b.eta, ready: turnToDate(b.eta).label, months, need, done: Math.max(0, Math.min(need, need - months)),
    stands: b.region, cost: b.cost, fresh: b.ordered === g.turn,
  };
}

function cityOf(g: Game, region: number, holder: string, spied: boolean, builds: Build[]): CityOrders {
  const r = g.regions[region];
  const all0 = builds.map((b) => lineOf(g, b, holder));
  // How many orders here take the same sign on the badge (the starred apart).
  const same = (l: OrderLine): number => all0.filter((x) => x.unit && x.mark === l.mark && x.elite === l.elite).length;
  // Units first; the soonest first; on a tie the kind with more on order here, then the one ordered first.
  const lines = all0.sort((a, b) => Number(b.unit) - Number(a.unit) || a.eta - b.eta || (a.unit ? same(b) - same(a) : 0) || a.id - b.id);
  const kinds: CityOrders['kinds'] = [];
  for (const l of lines) {
    const k = kinds.find((x) => x.sign === l.sign && x.label === l.label && x.starred === l.elite);
    if (k) k.count++;
    else kinds.push({ sign: l.sign, mark: l.mark, starred: l.elite, label: l.label, plural: l.plural, count: 1, unit: l.unit, months: l.months });
  }
  const next = lines[0];
  const units = lines.filter((l) => l.unit);
  const badge = next.unit ? { mark: next.mark, starred: next.elite, count: same(next), plus: units.length > same(next), months: next.months } : null;
  const said = kinds.map((k) => `${k.count} ${k.count === 1 ? k.label : k.plural}`);
  const all = said.length > 1 ? `${said.slice(0, -1).join(', ')} and ${said[said.length - 1]}` : said[0];
  return {
    region, province: r.name, town: townName(r), holder, spied, lines,
    units: units.length, works: lines.length - units.length, kinds, next, months: next.months, badge,
    summary: `${lines.length} on order: ${all}. ${lines.length === 1 ? 'Ready' : 'Next ready'} ${inMonths(next.months)}.`,
    list: lines.map((l) => `${l.name}: ready ${l.ready} (${inMonths(l.months)})`),
  };
}

/**
 * Every place with something on order that the viewer may know of: his own country's orders, each at its place,
 * and an enemy's or a neutral's only where an agent of his is at work in the very province. Places with units
 * forming come first, the soonest first; then places with only works under way.
 */
export function cityOrders(g: Game, nat: string | null = g.human): CityOrders[] {
  const n = nat ? g.nations[nat] : null;
  if (!n) return [];
  const at = new Map<number, Build[]>();
  for (const b of n.queue) { const l = at.get(b.region); if (l) l.push(b); else at.set(b.region, [b]); }
  const out = [...at].map(([region, builds]) => cityOf(g, region, n.id, false, builds));
  // What his agents see of other countries' orders: only in the province an agent is at work in.
  for (const r of g.regions) {
    if (r.ctrl === n.id || at.has(r.id) || !agentAt(g, r.id, n.id)) continue;
    const theirs = g.nations[r.ctrl]?.queue.filter((b) => b.region === r.id) ?? [];
    if (theirs.length) out.push(cityOf(g, r.id, r.ctrl, true, theirs));
  }
  return out.sort((a, b) => Number(a.spied) - Number(b.spied) || Number(b.units > 0) - Number(a.units > 0) || a.months - b.months || a.region - b.region);
}

/** What is on order at one place, as the viewer may know it, or null where there is nothing (or nothing he can see). */
export function ordersAt(g: Game, region: number, nat: string | null = g.human): CityOrders | null {
  return cityOrders(g, nat).find((c) => c.region === region) ?? null;
}
