/**
 * Marches that carry on (Version 82; the design director's rulings 87 and 93, on Brian's ask of 2026-11-16: "if you
 * click on place to move for a unit but they are out of moves, then it will wait to the next turn and auto move
 * there").
 *
 * A march to friendly ground too far for this month, or ordered of an army with no moves left, is a standing order:
 * the corps goes as far as it can at once and on again as each of the player's months opens, until it arrives. It is
 * a player's order only: no computer country uses it, so no war that nobody plays can move.
 *
 * The rules for it:
 * - friendly ground only: a kept march never attacks, never enters a neutral or enemy ground, never crosses the sea;
 * - the road is found again each month by the rules' own costs, so railways, rivers, mud and lost ground all count;
 * - its first leg always marches, whenever that is: an army with no moves left that is ordered back from the front
 *   goes as the next month opens, whatever the enemy has done in between (ruling 93);
 * - from the second leg on it halts only for news: the leg would end on ground the enemy he can see could strike
 *   now and could not when the order was given, or the army was attacked since its last leg, or the way is closed;
 * - where the road is full it waits one month and then halts;
 * - a march that ends always tells the player, and says why; any other order to the corps ends it.
 *
 * One order for both interfaces, kept with the saved game in these flags:
 *   march:ID            where it is bound, as the region's id + 1
 *   marchRep:ID         the last battle report there was when it last marched (a later one with it in is an attack)
 *   marchWait:ID        1 once it has waited a month for a full road
 *   marchLegs:ID        the legs it has marched (0: not yet set off)
 *   marchAt:ID          the month the order was given
 *   marchAs:ID          how it stood when the march last left it (see settleMarches)
 *   hot:MONTH           1 once that month's reach is written down, and
 *   hot:MONTH:REGION    1 for each friendly province the visible enemy's reach covered in that month
 * A march without marchLegs and marchAt (one kept before ruling 93) is read as past its first leg, with nothing
 * known to have been covered when it was ordered.
 *
 * What an interface calls:
 *   planMarch(g, c, to)      before an order: can a march be kept there, how long, the road, and the pointer's line
 *   orderMarch(g, c, to)     the order
 *   marchState(g, c)         for a corps on a march: where it is bound and about how many months are left
 *   forgetMarch(g, ids)      as any other order is given to those corps (or settleMarches as the month is handed over)
 *   advanceMarches(g)        as the player's month opens
 */
import { hashString } from '../core/rng';
import { garrisonLine } from './armies';
import { allied, atWar, ensureSupply, headline, moveCost } from './game';
import { blocked } from './rivers';
import { canOrder, humanUp, onEve, orderMove, reach, sight, type Reach } from './turn';
import type { Corps, Game } from './types';

const key = (id: number): string => `march:${id}`;
/** Where a corps is marching to on a standing order, or -1. */
export const marchOf = (g: Game, c: Corps): number => { const v = g.flags[key(c.id)]; return v !== undefined && v > 0 ? v - 1 : -1; };
/** Ends the standing marches of these corps, without a word. */
export function forgetMarch(g: Game, ids: number[]): void {
  for (const id of ids) for (const k of ['march', 'marchRep', 'marchWait', 'marchAs', 'marchLegs', 'marchAt']) delete g.flags[`${k}:${id}`];
}
const friendlyTo = (g: Game, c: Corps, id: number): boolean => { const r = g.regions[id]; return !!r && allied(g, c.nat, r.ctrl); };
const lastReportId = (g: Game): number => (g.reports.length ? g.reports[g.reports.length - 1].id : 0);
/**
 * How a corps stands, as a number: any order that sends it somewhere or sets it to something else changes it (a
 * move, an attack, digging in, a job, a town's garrison, a joint army, leave). What it is to do if attacked does not.
 */
const standing = (c: Corps): number => hashString([c.reg, c.ap, c.fought, c.digs, c.job?.what, c.job?.reg, c.town, c.joint, c.leave, c.school].join('|')) + 1;

/**
 * Every province a corps could march to over friendly ground, however many months it took, by the rules' own cost
 * of each step (what a move pays, and what it cannot cross): the weeks it is off, and the province before it on the
 * cheapest way.
 */
export function farGround(g: Game, c: Corps): Map<number, { cost: number; prev: number }> {
  const out = new Map<number, { cost: number; prev: number }>();
  out.set(c.reg, { cost: 0, prev: -1 });
  const open = [c.reg], done = new Set<number>();
  while (open.length && done.size < 900) {
    let best = 0;
    for (let i = 1; i < open.length; i++) if (out.get(open[i])!.cost < out.get(open[best])!.cost) best = i;
    const a = open.splice(best, 1)[0];
    if (done.has(a)) continue;
    done.add(a);
    const ca = out.get(a)!.cost;
    for (const j of g.regions[a].nb) {
      if (done.has(j) || !friendlyTo(g, c, j)) continue;
      const r = g.regions[j];
      if (blocked(g, c, r, a)) continue;
      const step = Math.max(1, moveCost(g, c, r, a));
      const old = out.get(j);
      if (old && old.cost <= ca + step) continue;
      out.set(j, { cost: ca + step, prev: a });
      open.push(j);
    }
  }
  return out;
}

/** The road over friendly ground from where a corps stands to a place, the place itself last, and the weeks it costs; null if there is none. */
export function farRoad(g: Game, c: Corps, to: number): { road: number[]; cost: number } | null {
  return roadOver(g, c, to, () => farGround(g, c));
}

/** The corps' farGround, asked for only when it is needed: worked out once and kept while nothing in the war changes. */
export type GroundOf = () => Map<number, { cost: number; prev: number }>;

/** farRoad, over the corps' farGround as given. */
function roadOver(g: Game, c: Corps, to: number, groundOf: GroundOf): { road: number[]; cost: number } | null {
  if (to === c.reg || !friendlyTo(g, c, to)) return null;
  const ground = groundOf();
  const end = ground.get(to);
  if (!end) return null;
  const road: number[] = [];
  for (let at = to; at !== c.reg && at >= 0; at = ground.get(at)!.prev) road.unshift(at);
  return { road, cost: end.cost };
}

/** About how many months a march of so many weeks takes this corps, from the month it sets off. */
export const marchMonths = (c: Corps, cost: number): number => Math.max(1, Math.ceil(cost / Math.max(1, c.apMax ?? 4)));
/** "about a month", "about 3 months". */
export const aboutMonths = (n: number): string => (n === 1 ? 'about a month' : `about ${n} months`);

/** The leg of its road a corps can cover this month: the provinces it would pass through, the last the one it stops in. Empty if it cannot move along it. */
function marchLeg(g: Game, c: Corps, road: number[]): number[] {
  const m = reach(g, c);
  let far = -1;
  for (let i = 0; i < road.length; i++) { const r = m.get(road[i]); if (r && !r.attack) far = i; }
  if (far < 0) return [];
  const path: number[] = [];
  for (let at = road[far]; at !== c.reg && at >= 0; at = m.get(at)!.prev) path.unshift(at);
  return path;
}

/**
 * How far the enemy corps the player can see could strike next month, were each of them fresh: the ground it could
 * march onto, and the ground beside any place it could reach with a week still in hand. Kept to the ground that
 * matters to the asker. It reads the game and leaves it as it was.
 */
export function enemyReach(g: Game, matters: (id: number) => boolean): Set<number> {
  const danger = new Set<number>();
  const me = g.human;
  if (!me) return danger;
  const seen = sight(g);
  for (const e of g.corps) {
    if (e.dead || e.kind === 'fort' || !(seen[e.reg] > 0) || !atWar(g, e.nat, me)) continue;
    const had = e.ap;
    e.ap = Math.max(e.ap, e.apMax ?? e.ap);
    try {
      for (const [id, r] of reach(g, e)) {
        if (id !== e.reg && matters(id)) danger.add(id);
        if (!r.attack && e.ap - r.cost >= 1) {
          for (const j of g.regions[id].nb) if (atWar(g, e.nat, g.regions[j].ctrl) && matters(j)) danger.add(j);
        }
      }
    } finally { e.ap = had; }
  }
  return danger;
}

/** The player's own and his allies' ground: where a kept march can be, and so the ground whose danger matters to one. */
const ourGround = (g: Game) => (id: number): boolean => { const r = g.regions[id]; return !!g.human && (r.ctrl === g.human || allied(g, r.ctrl, g.human)); };

/** Writes down, once a month, the friendly ground the visible enemy's reach covers: what a march ordered this month was ordered in the knowledge of. */
function keepHot(g: Game): void {
  if (g.flags[`hot:${g.turn}`]) return;
  g.flags[`hot:${g.turn}`] = 1;
  for (const id of enemyReach(g, ourGround(g))) g.flags[`hot:${g.turn}:${id}`] = 1;
}
/** True if the enemy's reach covered a province when this march was ordered. */
const wasHot = (g: Game, id: number, reg: number): boolean => { const at = g.flags[`marchAt:${id}`]; return at !== undefined && g.flags[`hot:${at}:${reg}`] === 1; };
/** Drops the reach of every month no march was ordered in. */
function sweepHot(g: Game): void {
  const keys = Object.keys(g.flags);
  const live = new Set(keys.filter((k) => k.startsWith('marchAt:')).map((k) => g.flags[k]));
  for (const k of keys) if (k.startsWith('hot:') && !live.has(Number(k.split(':')[1]))) delete g.flags[k];
}

/** Why a march to a place cannot be kept for this corps, in one line; '' when it can. */
export function whyNotMarch(g: Game, c: Corps, to: number, groundOf: GroundOf = () => farGround(g, c)): string {
  const r = g.regions[to];
  if (!r || !g.human || !humanUp(g)) return 'No orders can be given now.';
  if (!canOrder(g, c)) return `${c.name} takes no orders this month.`;
  // A corps inside a town never has a move, so a march kept for it would never set off (Version 84.2.4): it is told to take the field.
  if (c.town !== undefined) return garrisonLine(g, [c]);
  if (onEve(g)) return 'On the eve of war every army stays where it is placed.';
  if (to === c.reg) return `${c.name} is there already.`;
  if (atWar(g, c.nat, r.ctrl)) return c.ap > 0 ? 'A march is never kept onto enemy ground: an attack is planned in the month it is made, with the odds in front of you.' : 'No moves left this month. An attack is planned in the month it is made.';
  if (!allied(g, c.nat, r.ctrl)) return 'A kept march goes through friendly ground only.';
  if (!roadOver(g, c, to, groundOf)) return `A kept march goes through friendly ground only, and there is no such road to ${r.name}.`;
  return '';
}

/** What the pointer says of a corps with no moves left, before anything else. */
export const NO_MOVES = 'No moves left this month.';

export interface MarchPlan {
  /** True when a march to the place can be ordered: made at once, or kept. */
  ok: boolean;
  /** True when the corps has no move left this month: a march ordered now sets off as the next month opens. */
  spent: boolean;
  /** True when the corps reaches the place this month by an ordinary move: nothing would be kept. */
  now: boolean;
  /** About how many months the march takes: from next month for a spent corps, this month counted otherwise. */
  months: number;
  /** The road, the place itself last; empty where there is none. */
  road: number[];
  /** What an order here would do, for the pointer, without the click that gives it ("IX Corps marches here from next month, about 2 months."); the reason where it cannot. */
  line: string;
}

/**
 * Before an order is given: can this corps be marched to that place on a standing order, about how long it would
 * take, and the line for the pointer. It reads the game and leaves it as it was.
 */
export function planMarch(g: Game, c: Corps, to: number, kept: { ground?: GroundOf; reach?: () => Map<number, Reach> } = {}): MarchPlan {
  const spent = c.ap <= 0;
  // The corps' ground is worked out once for the question and the road both (or handed in, by one asking of many places).
  let ground: Map<number, { cost: number; prev: number }> | null = null;
  const groundOf = kept.ground ?? (() => (ground ??= farGround(g, c)));
  const why = whyNotMarch(g, c, to, groundOf);
  if (why) return { ok: false, spent, now: false, months: 0, road: [], line: why };
  const way = roadOver(g, c, to, groundOf)!;
  const at = spent ? undefined : (kept.reach ? kept.reach() : reach(g, c)).get(to);
  if (at && !at.attack) return { ok: true, spent, now: true, months: 0, road: way.road, line: '' };
  const months = spent ? marchMonths(c, way.cost) : 1 + marchMonths(c, Math.max(1, way.cost - c.ap));
  const line = spent ? `${c.name} marches here from next month, ${aboutMonths(months)}.` : `${c.name} marches here over ${aboutMonths(months)}, going on by itself as each month opens.`;
  return { ok: true, spent, now: false, months, road: way.road, line };
}

/** For a corps on a standing march: where it is bound, about how many months it has still to go, and its road; null for any other. */
export function marchState(g: Game, c: Corps): { to: number; months: number; road: number[] } | null {
  const to = marchOf(g, c);
  if (to < 0 || !g.regions[to]) return null;
  const way = farRoad(g, c, to);
  return { to, months: way ? marchMonths(c, way.cost) : 1, road: way ? way.road : [] };
}

export interface MarchOrder {
  /** True when the march is kept: the corps goes on as each month opens. */
  kept: boolean;
  /** Provinces it marched through at once. */
  moved: number;
  /** About how many months it has still to go. */
  months: number;
  /** Why no march was ordered, where none was. */
  why: string;
}

/**
 * Orders a march to friendly ground however far: the corps sets off at once with the moves it has, and the rest of
 * the road is kept as a standing order. With no moves left it sets off as the next month opens, to the province
 * next door as to any other.
 */
export function orderMarch(g: Game, c: Corps, to: number): MarchOrder {
  const why = whyNotMarch(g, c, to);
  if (why) return { kept: false, moved: 0, months: 0, why };
  forgetMarch(g, [c.id]);
  const way = farRoad(g, c, to)!;
  const leg = marchLeg(g, c, way.road);
  const moved = leg.length ? orderMove(g, c, leg[leg.length - 1]) : 0;
  if (c.reg === to) return { kept: false, moved, months: 0, why: '' };
  keepHot(g);
  g.flags[key(c.id)] = to + 1;
  g.flags[`marchRep:${c.id}`] = lastReportId(g);
  g.flags[`marchLegs:${c.id}`] = moved > 0 ? 1 : 0;
  g.flags[`marchAt:${c.id}`] = g.turn;
  g.flags[`marchAs:${c.id}`] = standing(c);
  const left = farRoad(g, c, to);
  return { kept: true, moved, months: left ? marchMonths(c, left.cost) : 1, why: '' };
}

/**
 * As the player hands his month over: any other order to a corps ends its standing march. An order always leaves
 * the corps standing otherwise than the march left it (moved, spent, dug in, in a town, on a job), so that is the
 * test. An interface that calls forgetMarch as each order is given has no need of this; it does no harm there.
 */
export function settleMarches(g: Game): void {
  for (const k of Object.keys(g.flags)) {
    if (!k.startsWith('march:')) continue;
    const id = Number(k.slice(6));
    const c = g.corps.find((x) => x.id === id && !x.dead);
    if (!c) { forgetMarch(g, [id]); continue; }
    const as = g.flags[`marchAs:${id}`];
    if (as !== undefined && as !== standing(c)) forgetMarch(g, [id]);
  }
  sweepHot(g);
}

/** Why a march ended: it arrived, the enemy's reach now covers its next halt, it was attacked, or the way is closed. */
export type MarchEnd = 'arrived' | 'enemy' | 'attacked' | 'closed';
export interface MarchNews { note: string; region: number; corps: number; arrived: boolean; why: MarchEnd }

/**
 * As the player's month opens: every corps with a standing march goes on, or halts. Returns a line for each march
 * that ended (with why, and the province to show), and the provinces armies marched into. Each line is in the
 * month's news as well.
 */
export function advanceMarches(g: Game): { ended: MarchNews[]; went: number[] } {
  const ended: MarchNews[] = [], went: number[] = [];
  const me = g.human;
  if (!me || !humanUp(g) || onEve(g)) return { ended, went };
  const keys = Object.keys(g.flags).filter((k) => k.startsWith('march:'));
  if (!keys.length) return { ended, went };
  let danger: Set<number> | null = null;
  const struck = (reg: number): boolean => (danger ??= enemyReach(g, ourGround(g))).has(reg);
  for (const k of keys) {
    const id = Number(k.slice(6)), c = g.corps.find((x) => x.id === id && !x.dead);
    const to = c ? marchOf(g, c) : -1;
    if (!c || to < 0 || !g.regions[to]) { forgetMarch(g, [id]); continue; } // (the corps is no more: there is nobody to tell of)
    const here = g.regions[c.reg].name, goal = g.regions[to].name;
    const end = (why: MarchEnd, note: string, region: number): void => {
      forgetMarch(g, [id]);
      ended.push({ note, region, corps: id, arrived: why === 'arrived', why });
      headline(g, 'info', note, me, region);
    };
    const halt = (why: MarchEnd, because: string): void => end(why, `${c.name} has halted at ${here} on its march to ${goal}: ${because}. It waits for orders.`, c.reg);
    const arrive = (): void => end('arrived', `${c.name} has arrived at ${goal}.`, to);
    if (c.reg === to) { arrive(); continue; }
    if (!canOrder(g, c)) { halt('closed', 'the way is closed'); continue; }
    // Its first leg always marches. After that it halts for news: an attack on it since its last leg, or (below)
    // a next halt the enemy can strike now and could not when the order was given.
    const first = g.flags[`marchLegs:${id}`] === 0;
    const since = g.flags[`marchRep:${id}`] ?? 0;
    const attacked = g.reports.some((r) => r.id > since && [...r.attackers, ...r.defenders].some((u) => u.name === c.name && u.nat === c.nat));
    if (!first && attacked) { halt('attacked', 'it was attacked'); continue; }
    const way = farRoad(g, c, to);
    if (!way) { halt('closed', 'the way is closed'); continue; }
    const leg = marchLeg(g, c, way.road);
    if (!leg.length) {
      // The next province is full, or cannot be entered this month: it waits one month, and then halts.
      if (g.flags[`marchWait:${id}`]) halt('closed', 'the way is closed');
      else { g.flags[`marchWait:${id}`] = 1; g.flags[`marchAs:${id}`] = standing(c); }
      continue;
    }
    const stop = leg[leg.length - 1];
    if (!first && struck(stop) && !wasHot(g, id, stop)) { halt('enemy', `the enemy can now reach ${g.regions[stop].name}, its next halt`); continue; }
    delete g.flags[`marchWait:${id}`];
    const n = orderMove(g, c, stop);
    g.flags[`marchRep:${id}`] = lastReportId(g);
    if (n > 0) { went.push(c.reg); g.flags[`marchLegs:${id}`] = (g.flags[`marchLegs:${id}`] ?? 1) + 1; }
    if (c.reg === to) arrive();
    else g.flags[`marchAs:${id}`] = standing(c);
  }
  sweepHot(g);
  // (The supply window, turn.ts stepAI: the marches that took ground leave a fresh supply picture behind them.)
  ensureSupply(g);
  return { ended, went: [...new Set(went)] };
}
