// Guns that guard a railway (the design director's ruling 125; the art director's guard-the-line brief). The rule
// is older than this file and stays where it was: anti-aircraft guns built in a province take a share a level off
// the chance that a raid cuts its railway (air.ts, rails.ts). Nothing said so. Here is what both interfaces say of
// it: the sentence on the railway's row of a province's card, the order that row offers (the same order as the
// build page's anti-aircraft guns: one order with two doors), and which stretches of a route wear the guard sign.
// Nothing here changes the game: no flag is written and no dice are thrown.
import { ARSENAL, offers, workLevel } from './arsenal';
import { groundRules } from './ground';
import { onNet } from './rails';
import { turnToDate } from '../core/ticket';
import type { Game } from './types';

/** The most levels of guns a province can have. */
export const GUARD_TOP = 3;

/** The share of the bombing that would cut a line which guns of a level stop: 0.15 a level. */
export const guardStops = (level: number): number => Math.max(0, level) * ARSENAL.works.aa.cut;

/** The order the railway's row offers: the build page's anti-aircraft guns, for this province. */
export interface GuardOrder {
  /** 'Guard the line' where no guns stand or are building, 'Guard it better' where some do. */
  head: string;
  /** 'build level 2 guns' */
  text: string;
  /** The level the order would build. */
  level: number;
  /** In production points: show it times BUDGET.scale. */
  cost: number;
  months: number;
  /** Why it cannot be placed now, or null when it can. */
  why: string | null;
  /** True when money is the only thing in the way. */
  poor: boolean;
}

/** What a railway's row says of the guns that guard it. */
export interface GuardSaid {
  /** The level of the guns that stand in the province now. */
  level: number;
  /** How many more levels are on order (the viewer's own ground only: nobody sees another's order book). */
  coming: number;
  /** The month the next level is ready, or null when none is on order. */
  ready: number | null;
  /** The share of the bombing that would cut the line which the guns stop now. */
  stops: number;
  /** The sentence: 'No guns are built here to guard the railway.' */
  line: string;
  /** The order, on ground in the viewer's own hands and under the top level; otherwise null. */
  order: GuardOrder | null;
}

/**
 * The guns that guard a province's railway, as its card says them. On enemy ground the level is said too (a raid's
 * planner tells him the guns he flies against), with no order. Null where the province has no railway, and outside
 * the grand campaign.
 */
export function guardSays(g: Game, region: number, viewer: string | null = g.human): GuardSaid | null {
  const r = g.regions[region];
  if (!r || !groundRules(g) || !onNet(region)) return null;
  const level = workLevel(g, 'aa', region);
  const own = !!viewer && !!g.nations[viewer]?.alive && r.ctrl === viewer;
  const due = own ? g.nations[viewer!].queue.filter((b) => b.what === 'work' && b.item === 'aa' && b.region === region).map((b) => b.eta).sort((a, b) => a - b) : [];
  const coming = due.length, ready = coming ? due[0] : null;
  const stops = guardStops(level);
  // (The UI director, 25 Nov 2026 21:22Z, case f: the light guns every province has stop none of the bombing that cuts a
  // railway, guardStops(0); the line speaks of built guns, so it says so.)
  const guards = level > 0 ? `Level ${level} guns guard the railway here: they stop ${Math.round(stops * 100)}% of the bombing that would cut it.` : 'No guns are built here to guard the railway.';
  const building = ready !== null ? `Level ${level + 1} guns are being built here: ready in ${turnToDate(ready).label}.` : '';
  const line = !building ? guards : level > 0 ? `${guards} ${building}` : building;
  let order: GuardOrder | null = null;
  if (own && level + coming < GUARD_TOP) {
    const o = offers(g, viewer!, region).find((x) => x.key === 'work:aa');
    if (o && o.at === region) {
      const next = level + coming + 1;
      order = { head: level + coming > 0 ? 'Guard it better' : 'Guard the line', text: `build level ${next} guns`, level: next, cost: o.cost, months: o.months, why: o.why, poor: o.poor };
    }
  }
  return { level, coming, ready, stops, line, order };
}

/** A stretch of a route and the guns that stand on it, for the guard sign beside the track. */
export interface GuardTab {
  region: number;
  /** The level of the guns there: 0 is the gap. */
  level: number;
}

/**
 * The guard signs of a set of provinces, in the order given: one for each that has a railway. With `gaps` false
 * only the guarded ones are returned (the railways lit for an army in hand: an unguarded stretch wears nothing);
 * with it true every one is, guarded or not (the route under the pointer: the gaps are drawn where he is looking).
 */
export function guardTabs(g: Game, regions: Iterable<number>, gaps: boolean): GuardTab[] {
  if (!groundRules(g)) return [];
  const out: GuardTab[] = [];
  const seen = new Set<number>();
  for (const id of regions) {
    if (seen.has(id) || !g.regions[id] || !onNet(id)) continue;
    seen.add(id);
    const level = workLevel(g, 'aa', id);
    if (level > 0 || gaps) out.push({ region: id, level });
  }
  return out;
}
