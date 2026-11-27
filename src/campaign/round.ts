/**
 * NEXT ARMY (Version 82; the design director's ruling 88, on Brian's ask of 2026-11-16: "you need a way to cycle
 * through your units that have moves left that you haven't used yet so you dont miss them").
 *
 * The round is every army of the player's that he may order and that still has a move. An army here is a province:
 * his corps in the field there. The untouched come first and those at the front before those in the rear, then the
 * nearest, and each is visited once a round.
 *
 * Left out of the round: a corps on a kept march, a corps ordered to dig in, an engineer corps with work in hand, a
 * fortress, a garrison inside a town's walls, a corps away on leave, and a corps told to STAND THIS MONTH. That last
 * is the one small order that lets a round end: it lasts until the next month opens, and a second press takes it back.
 *
 * It is the player's own book-keeping and nothing more: no rule reads it and no computer country uses it, so no war
 * that nobody plays can move. It is kept with the saved game in one flag for each corps:
 *   stood:ID            the month it was told to stand and where, as MONTH * 100000 + the region's id + 1
 *
 * The two numbers (ruling 88(c) as amended by ruling 101(e)): the round is every army with a move, but the count that
 * keeps NEXT ARMY bright and holds the light from END THE MONTH is only the armies at the front not yet touched, and
 * any army raised this month that has had no order. The rear is a second, quieter number: the other armies not yet
 * touched. Tab visits them after the front, and they never keep END THE MONTH dark. A country at peace opens with
 * END THE MONTH lit. (An army that has marched part of its month stays in the round while it can still go somewhere,
 * after all the untouched, and is in neither number: it has had an order.)
 *
 * What an interface calls:
 *   armyRound(g)             the armies still wanting an order, in the round's order
 *   roundCount(g)            the two numbers beside NEXT ARMY: bright (it holds the light) and quiet (the rear's)
 *   nextArmy(g, seen, from)  the army to show next, given those already visited this round and where the eye is
 *   standArmy(g, reg, on)    STAND THIS MONTH for the army in that province, or taking it back
 *   armyStands(g, reg)       'all', 'some' or 'none' of the army there is standing this month (null: nothing to stand)
 *   frontIdle(g)             the armies at the front that have not moved at all and have no orders (the question)
 *   settleStands(g)          as the player's month opens: last month's marks are forgotten
 */
import { atWar } from './game';
import { marchOf } from './marches';
import { canOrder, humanUp, onEve, reach } from './turn';
import type { Corps, Game } from './types';

const key = (id: number): string => `stood:${id}`;
const stamp = (g: Game, c: Corps): number => g.turn * 100000 + c.reg + 1;

/** True for a corps told to stand this month, while it is still where it was told. */
export const standsThisMonth = (g: Game, c: Corps): boolean => g.flags[key(c.id)] === stamp(g, c);

/** A corps that has neither moved nor fought this month. */
const fresh = (c: Corps): boolean => c.ap >= c.apMax && !c.fought;

/** A corps the player could still tell to stand: his to order, in the field, with a move left, and no standing order of another kind. */
function couldStand(g: Game, c: Corps): boolean {
  if (!canOrder(g, c) || c.kind === 'fort' || c.town !== undefined || c.ap <= 0) return false;
  if (c.digs || c.job || marchOf(g, c) >= 0) return false;
  // A corps that has marched part of its month is in the round only while there is somewhere it can still go.
  return fresh(c) || reach(g, c).size > 0;
}

/** A corps of the player's that still wants an order this month. */
export const wantsOrder = (g: Game, c: Corps): boolean => couldStand(g, c) && !standsThisMonth(g, c);

/** True where the province touches ground held by a country the player is at war with. */
export function atFront(g: Game, reg: number): boolean {
  const me = g.human;
  return !!me && g.regions[reg].nb.some((j) => { const o = g.regions[j].ctrl; return !!o && atWar(g, me, o); });
}

export interface RoundArmy {
  /** The province it stands in. */
  reg: number;
  /** Its corps that still want an order. */
  corps: number[];
  /** Some of them have neither moved nor fought this month. */
  untouched: boolean;
  /** It stands beside the enemy. */
  front: boolean;
  /** A corps of it was raised this month and has had no order yet (never the armies a campaign opens with). */
  raised: boolean;
  /** It keeps NEXT ARMY bright and holds the light from END THE MONTH: untouched at the front, or newly raised. */
  bright: boolean;
}

const rank = (a: RoundArmy): number => (a.bright ? 0 : 4) + (a.untouched ? 0 : 2) + (a.front ? 0 : 1);

/** The player's armies that still want an order: the untouched first, the front before the rear. Empty when it is not his month. */
export function armyRound(g: Game): RoundArmy[] {
  if (!g.human || !humanUp(g) || onEve(g)) return [];
  const out: RoundArmy[] = [];
  // (The month a campaign opens every corps is new: none of them is "raised this month".)
  const opening = g.corps.every((c) => c.nat !== g.human || c.dead || c.born === g.turn);
  for (const r of g.regions) {
    const cs = r.units.filter((c) => wantsOrder(g, c));
    if (!cs.length) continue;
    const untouched = cs.some(fresh), front = atFront(g, r.id), raised = !opening && cs.some((c) => c.born === g.turn && fresh(c));
    out.push({ reg: r.id, corps: cs.map((c) => c.id), untouched, front, raised, bright: (front && untouched) || raised });
  }
  return out.sort((a, b) => rank(a) - rank(b) || a.reg - b.reg);
}

/** The two numbers beside NEXT ARMY. */
export interface RoundCount {
  /** Armies at the front not yet touched, and armies raised this month with no order yet: they keep NEXT ARMY bright and hold the light from END THE MONTH. */
  bright: number;
  /** The rear's quieter number, the other armies not yet touched: Tab visits them after the bright, and they never hold the light. */
  quiet: number;
  /** Of the bright, those that stand beside the enemy (the others are newly raised, in the rear). */
  front: number;
}
export function roundCount(g: Game): RoundCount {
  const all = armyRound(g);
  const bright = all.filter((a) => a.bright);
  return { bright: bright.length, quiet: all.filter((a) => !a.bright && a.untouched).length, front: bright.filter((a) => a.front).length };
}

/**
 * The bright count in words, the same beside NEXT ARMY and in the question before END THE MONTH (the design
 * director's ruling 111(f)): "10 at the front and 1 newly raised". One kind alone is named alone: "10 at the front".
 */
export function roundWords(c: RoundCount): string {
  const raised = c.bright - c.front;
  return c.front && raised ? `${c.front} at the front and ${raised} newly raised` : c.front ? `${c.front} at the front` : raised ? `${raised} newly raised` : '';
}

/**
 * The headline of the question before END THE MONTH, in words true of every army counted (ruling 116): an army that
 * has fought with one corps has had an order, but it still has moves left. "11 armies still have moves left: 10 at the
 * front and 1 newly raised"; one kind alone: "3 armies at the front still have moves left".
 */
export function roundQuestion(c: RoundCount): string {
  const n = c.bright, raised = n - c.front;
  if (c.front && raised) return `${n} armies still have moves left: ${roundWords(c)}`;
  return `${n} ${n === 1 ? 'army' : 'armies'} ${c.front ? 'at the front' : 'newly raised'} still ${n === 1 ? 'has' : 'have'} moves left`;
}

export interface NextArmy {
  /** The army to show, or null when every army has its orders. */
  army: RoundArmy | null;
  /** The provinces visited this round, this one included. */
  seen: number[];
  /** True when the round had been gone through and starts again with this army. */
  again: boolean;
}

/**
 * The army to show next: of those not yet visited this round, the first by the round's order and then the nearest
 * to where the eye is (`from`, on the map's own scale; `span` is the map's width where it wraps east to west).
 * When all have been visited the round starts again.
 */
export function nextArmy(g: Game, seen: readonly number[], from: { x: number; y: number } | null, span = 0): NextArmy {
  const all = armyRound(g);
  if (!all.length) return { army: null, seen: [], again: false };
  let left = all.filter((a) => !seen.includes(a.reg));
  const again = !left.length;
  if (again) left = all;
  const far = (a: RoundArmy): number => {
    if (!from) return 0;
    const r = g.regions[a.reg];
    let dx = Math.abs(r.x - from.x);
    if (span > 0) dx = Math.min(dx, span - dx);
    return Math.hypot(dx, r.y - from.y);
  };
  const best = left.map((a) => ({ a, d: far(a) })).sort((p, q) => rank(p.a) - rank(q.a) || p.d - q.d || p.a.reg - q.a.reg)[0].a;
  return { army: best, seen: [...(again ? [] : seen.filter((k) => all.some((a) => a.reg === k))), best.reg], again };
}

/** How much of the army in a province is standing this month; null where it has nothing that could be told to. */
export function armyStands(g: Game, reg: number): 'all' | 'some' | 'none' | null {
  const cs = g.regions[reg]?.units.filter((c) => couldStand(g, c)) ?? [];
  if (!cs.length) return null;
  const n = cs.filter((c) => standsThisMonth(g, c)).length;
  return n === cs.length ? 'all' : n ? 'some' : 'none';
}

/** STAND THIS MONTH for the army in a province (`on`), or taking the order back. Returns the corps it changed. */
export function standArmy(g: Game, reg: number, on: boolean): number {
  let n = 0;
  for (const c of g.regions[reg]?.units ?? []) {
    if (!couldStand(g, c) || standsThisMonth(g, c) === on) continue;
    if (on) g.flags[key(c.id)] = stamp(g, c); else delete g.flags[key(c.id)];
    n++;
  }
  return n;
}

/**
 * The armies at the front that have not moved at all and have no orders: what END THE MONTH asks about, once.
 * An army he has marched a corps into, or fought with, has had his eye and is not counted.
 */
export function frontIdle(g: Game): number[] {
  return armyRound(g).filter((a) => a.front && g.regions[a.reg].units.every((c) => !canOrder(g, c) || c.kind === 'fort' || fresh(c))).map((a) => a.reg);
}

/** As the player's month opens: the marks of earlier months are forgotten. */
export function settleStands(g: Game): void {
  for (const k of Object.keys(g.flags)) if (k.startsWith('stood:') && Math.floor(g.flags[k] / 100000) !== g.turn) delete g.flags[k];
}
