// The staff's plan this month, as its campaign read it: where it attacks and at what (release 2; the support teams'
// reckoning, orderworth.ts, under the design director's ruling of 25 November 2026 21:33Z, "one card, not a sum":
// an order is weighed on the card the front where it would stand will fight under the staff's plan now). The
// campaign (ai.ts) sets it as it reads the month's plan, for the main army and for each theatre overseas it runs;
// nothing else reads it. Runtime only: never saved, and read only in the month it was set, so a game loaded or a
// nation at peace has none.
//
// A player who runs his own army has no staff plan: his own orders are his plan (the design director, 21:44Z of 25
// November 2026). His build desk, handed to the staff, reads the attacks his corps have made this month and in the
// month just ended (the desk works as his month opens, before he has given this month's orders): a front next to a
// province they attacked reads the attack card, the rest the defence card. Read from the battle reports the game
// keeps already, so nothing new is saved for it either.

import type { Game } from './types';

/** What the staff's plan does this month on one army's ground: attacks or not, and if so at which places only (null: anywhere). */
export interface PlanNow { attack: boolean; only: Set<string> | null }

interface Kept { turn: number; plan: PlanNow; ids: Set<number> | null }

const kept = new WeakMap<Game, Map<string, Map<number, Kept>>>();

/** The campaign's word on this month's plan: the main army (scope null, key -1) or a theatre overseas (its ground, key k). */
export function planSet(g: Game, nat: string, k: number, ids: Set<number> | null, plan: PlanNow): void {
  let by = kept.get(g);
  if (!by) kept.set(g, (by = new Map()));
  let mine = by.get(nat);
  if (!mine) by.set(nat, (mine = new Map()));
  mine.set(k, { turn: g.turn, plan, ids });
}

/**
 * The plan this month on the ground of a province: its theatre's overseas where one ran there, else the main army's;
 * for the player with no staff plan this month, his own orders (hisOrders); null where there is none.
 */
export function planAt(g: Game, nat: string, region: number): PlanNow | null {
  const mine = kept.get(g)?.get(nat);
  let main: PlanNow | null = null, ran = false;
  for (const [k, x] of mine ?? []) {
    if (x.turn !== g.turn) continue;
    ran = true;
    if (k >= 0 && x.ids?.has(region)) return x.plan;
    if (k < 0) main = x.plan;
  }
  return ran ? main : nat === g.human ? hisOrders(g, nat) : null;
}

/** A player's own orders read as his plan: attacking at the provinces his corps attacked this month or the month just ended, else holding. */
export function hisOrders(g: Game, nat: string): PlanNow {
  const only = new Set<string>();
  for (let i = g.reports.length - 1; i >= 0 && g.reports[i].turn >= g.turn - 1; i--) {
    const rep = g.reports[i];
    if (rep.attNat === nat && rep.turn <= g.turn) only.add(g.regions[rep.region]?.name ?? rep.regionName);
  }
  return { attack: only.size > 0, only };
}
