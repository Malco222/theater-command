// Plans of the capital ships (Brian, 18 November: "they can get some ships but not carriers or battleships like the
// major nations. they only get this technology if they capture one of the major players cities, factories,
// territory"; "spys from smaller nations can also try and steal technology plans"; "keep british ships").
//
// The great powers lay down battleships and carriers as before, the tech tree still holding carriers back until
// Carrier aviation. The dominions build on Britain's plans and Vichy and Free France on France's, as a great power
// does. Every other country builds destroyers, submarines, cruisers and troopships, and lays down a battleship or a
// carrier only on plans it holds: taken with ground a great power owns, in a war with that power (capture() in
// game.ts), or stolen by one of its agents (intel.ts, the missions bbPlans and cvPlans). Plans of carriers open
// carriers without the country's own research. Ships a country starts the war with are its own and stay.
// The grand campaign only (arsenal.json, 'plans').
import data from '../../data/campaign/arsenal.json';
import { atWar, headline } from './game';
import { locked } from './tech';
import type { Game, PlanHeld, Region } from './types';

export type CapitalHull = PlanHeld['hull'];

export const PLANS = (data as unknown as { plans: { on: boolean; hulls: CapitalHull[]; great: string[]; kin: Record<string, string> } }).plans;

/** True where the rule is in force: the grand campaign, with the switch on. */
export const planRules = (g: Game): boolean => PLANS.on === true && g.flags.open === 1;

export const isCapitalHull = (hull: string): hull is CapitalHull => PLANS.hulls.includes(hull as CapitalHull);

/** True for a nation that builds capital ships as a great power does: one of the great powers, or kin of one. */
export const buildsOwn = (nat: string): boolean => PLANS.great.includes(nat) || PLANS.kin[nat] !== undefined;

/** True for a great power whose plans of this hull are there to be taken or stolen: one that can lay it down itself. */
export const givesPlans = (g: Game, nat: string, hull: CapitalHull): boolean =>
  PLANS.great.includes(nat) && g.nations[nat] !== undefined && locked(g, nat, `ship:${hull}`) === null;

export const plansOf = (g: Game, nat: string): PlanHeld[] => g.nations[nat]?.plans ?? [];

const HULL_WORDS: Record<CapitalHull, { one: string; many: string }> = {
  bb: { one: 'battleship', many: 'Battleships' },
  cv: { one: 'carrier', many: 'Aircraft carriers' },
};

/**
 * The great power whose plans of this hull the nation builds on: its own tag for a great power, the mother
 * country's for its kin, otherwise the plans it gained last, or null when it holds none. The 3D game draws a
 * smaller navy's battleship or carrier as this power's model, in the country's own grey and under its own flag.
 */
export function plansFrom(g: Game, nat: string, hull: CapitalHull): string | null {
  if (PLANS.great.includes(nat)) return nat;
  if (PLANS.kin[nat] !== undefined) return PLANS.kin[nat];
  const held = plansOf(g, nat).filter((p) => p.hull === hull);
  return held.length ? held[held.length - 1].from : null;
}

/** Why the nation cannot lay down this hull for want of its plans, or null. */
export function whyNoPlans(g: Game, nat: string, hull: string): string | null {
  if (!planRules(g) || !isCapitalHull(hull) || plansFrom(g, nat, hull) !== null) return null;
  return `${HULL_WORDS[hull].many} are built only by the great powers. Take ground a great power holds, in a war with it, or have an agent steal its plans.`;
}

/** True when the nation lays this hull down on plans it took or stole: the tech tree does not hold it back. */
export const onPlans = (g: Game, nat: string, hull: string): boolean =>
  planRules(g) && isCapitalHull(hull) && !buildsOwn(nat) && plansFrom(g, nat, hull) !== null;

/** Gives a smaller nation a great power's plans of one hull. False when it gains nothing new. */
export function grantPlans(g: Game, nat: string, hull: CapitalHull, from: string, how: PlanHeld['how']): boolean {
  const n = g.nations[nat];
  if (!planRules(g) || !n || buildsOwn(nat) || !givesPlans(g, from, hull)) return false;
  if (plansOf(g, nat).some((p) => p.hull === hull && p.from === from)) return false;
  (n.plans ??= []).push({ hull, from, how, turn: g.turn });
  return true;
}

/** capture(): ground a great power owns, taken from it in a war with it, gives the taker that power's plans. */
export function plansOnCapture(g: Game, r: Region, nat: string, old: string): void {
  if (!planRules(g) || buildsOwn(nat) || r.ctrl !== nat || r.owner !== old || !PLANS.great.includes(old) || !atWar(g, nat, old)) return;
  const got = PLANS.hulls.filter((h) => grantPlans(g, nat, h, old, 'taken'));
  if (!got.length || nat !== g.human) return;
  const O = g.nations[old];
  const what = got.map((h) => HULL_WORDS[h].one).join(' and ');
  const ships = got.map((h) => HULL_WORDS[h].many.toLowerCase()).join(' and ');
  headline(g, 'event', `${O.adj} ${what} plans are found at ${r.name}. Your shipyards can now lay down ${ships} on ${O.adj} designs.`, nat, r.id);
}

/** For the 3D game and the build page: the great power whose plans each nation builds its battleships and carriers on. */
export function plansView(g: Game): Record<string, Record<CapitalHull, string | null>> {
  const out: Record<string, Record<CapitalHull, string | null>> = {};
  for (const nat of Object.keys(g.nations)) out[nat] = { bb: plansFrom(g, nat, 'bb'), cv: plansFrom(g, nat, 'cv') };
  return out;
}
