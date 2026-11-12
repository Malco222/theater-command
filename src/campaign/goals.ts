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
}

export type GoalKey = 'short' | 'long';
export type GoalState = 'open' | 'won' | 'lost' | 'out';

export const GOAL_KEYS: GoalKey[] = ['short', 'long'];
export const GOALS = data.nations as unknown as Record<string, Partial<Record<GoalKey, Goal>>>;

const CODE: Record<number, GoalState> = { 1: 'won', 2: 'lost', 3: 'out' };

/** The last month that counts toward a goal. */
export const goalEnds = (goal: Goal): number => goal.by ?? goal.until ?? 0;

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
  if (!out.length) out.push({ label: `${g.nations[nat].name} still in the war`, done: g.nations[nat].alive });
  return out;
}

const met = (g: Game, nat: string, goal: Goal): boolean => g.nations[nat].alive && goalChecks(g, nat, goal).every((c) => c.done);

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
    if (!g.nations[nat].alive) won = false;
    else if (goal.until !== undefined) { if (g.turn > goal.until) won = ok; }
    // What stands when the month after the last one opens still counts: an ally may have finished the job.
    else if (ok && g.turn <= goalEnds(goal) + 1) won = true;
    else if (g.turn > goalEnds(goal)) won = false;
    if (won === null) continue;
    g.flags[`goal:${key}`] = won ? 1 : 2;
    headline(g, 'goal', `${g.nations[nat].name}'s ${key} campaign is ${won ? 'won' : 'lost'}: ${goal.title}.`, nat);
    out.push({ key, goal, won });
  }
  return out;
}
