// Campaign goals: what winning means for the nation a person plays. Each nation has a short campaign and a
// long one, written as data in data/campaign/goals.json. Goals judge the war and never change it: nothing here
// is read by a computer nation, and a war with nobody at the controls has no goals at all.
//
// What has been decided is kept in g.flags as goal:short and goal:long (1 won, 2 lost, 3 not in play), so it is
// saved with the campaign.
import data from '../../data/campaign/goals.json';
import { allied, headline } from './game';
import type { Game } from './types';

export interface Goal {
  title: string;
  text: string;
  /** Cities that must be in the nation's own hands or an ally's. */
  hold?: string[];
  /** Nations that must have capitulated. */
  beaten?: string[];
  /** The last month in which the goal can be reached. */
  by?: number;
  /** A goal of endurance: decided when the month after this one opens. */
  until?: number;
  /** A goal of holding out: the nation is still in the war after this many weeks of it, four to a month. */
  outlast?: number;
}

export type GoalKey = 'short' | 'long';
export type GoalState = 'open' | 'won' | 'lost' | 'out';

export const GOAL_KEYS: GoalKey[] = ['short', 'long'];
export const GOALS = data.nations as unknown as Record<string, Partial<Record<GoalKey, Goal>>>;

const CODE: Record<number, GoalState> = { 1: 'won', 2: 'lost', 3: 'out' };

/** The last month that counts toward a goal. */
export const goalEnds = (goal: Goal): number => goal.by ?? goal.until ?? (goal.outlast !== undefined ? Math.ceil(goal.outlast / 4) : 0);

/** Weeks of war a nation has come through: every nation that moves before it has had its month when its own comes round. */
function weeksThrough(g: Game, nat: string): number {
  // On the eve of war nobody has moved yet, whatever the order of play says.
  const moved = g.flags.eve !== 1 && g.phase >= g.order.indexOf(nat);
  return (g.turn - 1) * 4 + (moved ? 4 : 0);
}

/** The week of the war in which a beaten nation fell, counting four to a month. */
export function weeksLasted(g: Game, nat: string): number | null {
  const f = g.nations[nat]?.fellOn;
  return f ? (f.turn - 1) * 4 + f.week : null;
}

export function goalState(g: Game, key: GoalKey): GoalState {
  return CODE[g.flags[`goal:${key}`]] ?? 'open';
}

/** The separate things a goal asks for, and whether each one holds today. */
export function goalChecks(g: Game, nat: string, goal: Goal): { label: string; done: boolean }[] {
  const out: { label: string; done: boolean }[] = [];
  for (const name of goal.hold ?? []) {
    const r = g.byName[name];
    out.push({ label: name, done: !!r && allied(g, nat, r.ctrl) });
  }
  for (const id of goal.beaten ?? []) {
    const n = g.nations[id];
    out.push({ label: `${n?.name ?? id} beaten`, done: !!n && !n.alive && !n.dormant });
  }
  if (goal.outlast !== undefined) {
    const fell = weeksLasted(g, nat);
    out.push({ label: `More than ${goal.outlast} weeks in the war`, done: fell !== null ? fell > goal.outlast : weeksThrough(g, nat) >= goal.outlast });
  }
  if (!out.length) out.push({ label: `${g.nations[nat].name} still in the war`, done: g.nations[nat].alive });
  return out;
}

const met = (g: Game, nat: string, goal: Goal): boolean => g.nations[nat].alive && goalChecks(g, nat, goal).every((c) => c.done);

/** How long a goal gives, in words for the goals panel. */
export function goalWhen(goal: Goal, date: (turn: number) => string): string {
  if (goal.outlast !== undefined) return `decided by how long you hold out`;
  return goal.until !== undefined ? `decided when ${date(goal.until + 1)} opens` : `by the end of ${date(goal.by!)}`;
}

/**
 * Called once when a campaign opens in month `from`. A goal whose time has already gone by is not in play, and
 * neither is a goal that a later start hands the player already won.
 */
export function openGoals(g: Game, nat: string, from: number): void {
  for (const key of GOAL_KEYS) {
    const goal = GOALS[nat]?.[key];
    delete g.flags[`goal:${key}`];
    if (!goal) continue;
    if (goalEnds(goal) < from || (from > 1 && goal.by !== undefined && met(g, nat, goal))) g.flags[`goal:${key}`] = 3;
  }
}

/** Looks at the player's goals and returns those decided just now. Safe to call as often as the interface likes. */
export function judgeGoals(g: Game, nat: string): { key: GoalKey; goal: Goal; won: boolean }[] {
  const out: { key: GoalKey; goal: Goal; won: boolean }[] = [];
  for (const key of GOAL_KEYS) {
    const goal = GOALS[nat]?.[key];
    if (!goal || goalState(g, key) !== 'open') continue;
    const ok = met(g, nat, goal);
    let won: boolean | null = null;
    if (goal.outlast !== undefined) {
      // A nation that goes down fighting can still have held out long enough: what counts is the week it fell.
      const fell = weeksLasted(g, nat);
      if (fell !== null) won = fell > goal.outlast && (goal.hold ?? []).length === 0;
      else if (g.nations[nat].alive && weeksThrough(g, nat) >= goal.outlast) won = ok;
    }
    else if (!g.nations[nat].alive) won = false;
    else if (goal.until !== undefined) { if (g.turn > goal.until) won = ok; }
    // What stands when the month after the last one opens still counts: an ally may have finished the job.
    else if (ok && g.turn <= goalEnds(goal) + 1) won = true;
    else if (g.turn > goalEnds(goal)) won = false;
    if (won === null) continue;
    g.flags[`goal:${key}`] = won ? 1 : 2;
    headline(g, 'goal', `${g.nations[nat].name}'s ${key} campaign is ${won ? 'won' : 'lost'}: ${goal.title}.`, nat);
    // A goal settled by a nation's fall is dated to the week it fell.
    const fell = g.nations[nat].fellOn;
    if (fell && !g.nations[nat].alive) Object.assign(g.headlines[g.headlines.length - 1], { turn: fell.turn, week: fell.week });
    // A goal won by lasting is dated to the last week the nation came through.
    else if (goal.outlast !== undefined) Object.assign(g.headlines[g.headlines.length - 1], { turn: Math.max(1, weeksThrough(g, nat) / 4), week: 4 });
    out.push({ key, goal, won });
  }
  return out;
}
