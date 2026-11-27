// Plans of the weapons only the great powers make (Brian, 18 November: "they can get some ships but not carriers or
// battleships like the major nations. they only get this technology if they capture one of the major players
// cities, factories, territory"; "spys from smaller nations can also try and steal technology plans"; "keep british
// ships"; "heavy bombers, jets and rockets should all be treated like the battleships"). Ruling 235 and its rider b542.
//
// The great powers lay down battleships and carriers as before (the tech tree still holds carriers back until
// Carrier aviation) and research the four locked technologies as before. The dominions build on Britain's plans and
// Vichy and Free France on France's: ships as a great power does, and the four technologies wherever their great
// power has researched them. Every other country builds destroyers, submarines, cruisers and troopships, and gains a
// battleship, a carrier or one of the four only with a great power's plans: taken with ground that power owns, in a
// war with it (capture() in game.ts), or stolen by one of its agents (intel.ts). Plans of carriers open carriers
// without the country's own research; a technology's plans put it in service as if researched. Ships a country
// starts the war with are its own and stay. The grand campaign only (arsenal.json, 'plans').
import data from '../../data/campaign/arsenal.json';
import { atWar, headline, theWords } from './game';
import { grantTech, hasTech, locked } from './tech';
import type { Game, PlanHeld, Region } from './types';

export type PlanItem = PlanHeld['what'];
export type CapitalHull = Extract<PlanItem, 'bb' | 'cv'>;
export type PlanTech = Exclude<PlanItem, CapitalHull>;

export const PLANS = (data as unknown as { plans: { on: boolean; hulls: CapitalHull[]; techs: PlanTech[]; great: string[]; kin: Record<string, string> } }).plans;

/** Every weapon there are plans of: the two capital ships, then the four technologies. */
export const PLAN_ITEMS: PlanItem[] = [...PLANS.hulls, ...PLANS.techs];

const listOf = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);

/** True where the rule is in force: the grand campaign, with the switch on. */
export const planRules = (g: Game): boolean => PLANS.on === true && g.flags.open === 1;

export const isCapitalHull = (x: string): x is CapitalHull => PLANS.hulls.includes(x as CapitalHull);
export const isPlanTech = (x: string): x is PlanTech => PLANS.techs.includes(x as PlanTech);

/** True for a nation that builds capital ships as a great power does: one of the great powers, or kin of one. */
export const buildsOwn = (nat: string): boolean => PLANS.great.includes(nat) || PLANS.kin[nat] !== undefined;

/**
 * The words for each weapon: one and many, where a great power keeps its plans ('yard': the capital and the great
 * naval yards; 'works': the capital and the great aircraft works, of the wing named; 'capital': the capital alone),
 * and who can use them once they are held, and how ('verb' and 'many': "lay down battleships"; 'verbs' agrees with
 * 'who': "your air force flies"; 'them' stands for 'many': "rocket artillery ... has it").
 */
export const PLAN_WORDS: Record<PlanItem, { one: string; many: string; them: 'them' | 'it'; place: 'yard' | 'works' | 'capital'; wing?: 'ftr' | 'bmr'; who: string; verb: string; verbs: string }> = {
  bb: { one: 'battleship', many: 'battleships', them: 'them', place: 'yard', who: 'your shipyards', verb: 'lay down', verbs: 'lay down' },
  cv: { one: 'carrier', many: 'aircraft carriers', them: 'them', place: 'yard', who: 'your shipyards', verb: 'lay down', verbs: 'lay down' },
  heavybombers: { one: 'four-engined bomber', many: 'four-engined bombers', them: 'them', place: 'works', wing: 'bmr', who: 'your air force', verb: 'fly', verbs: 'flies' },
  jets: { one: 'jet fighter', many: 'jet fighters', them: 'them', place: 'works', wing: 'ftr', who: 'your air force', verb: 'fly', verbs: 'flies' },
  rockets: { one: 'rocket artillery', many: 'rocket artillery', them: 'it', place: 'capital', who: 'your army', verb: 'fire', verbs: 'fires' },
  guided: { one: 'guided weapon', many: 'guided weapons', them: 'them', place: 'capital', who: 'you', verb: 'launch', verbs: 'launch' },
};

/** "lay down battleships". */
export const doesWith = (x: PlanItem): string => `${PLAN_WORDS[x].verb} ${PLAN_WORDS[x].many}`;

/**
 * Where a great power keeps its plans, in words: 'got' out of (one place), 'kept' by the great powers in (all of
 * them), 'home' (one power's own), and the place one province lacks.
 */
export const PLAN_PLACES: Record<'yard' | 'works' | 'capital', { got: string; kept: string; home: string; lack: string }> = {
  yard: { got: 'its capital or one of its great naval yards', kept: 'their capitals and great naval yards', home: 'its capital and great naval yards', lack: 'There is no great naval yard' },
  works: { got: 'its capital or one of its great aircraft works', kept: 'their capitals and great aircraft works', home: 'its capital and great aircraft works', lack: 'There are no great aircraft works' },
  capital: { got: 'its capital', kept: 'their capitals', home: 'its capital', lack: '' },
};

const cap = (s: string): string => s.charAt(0).toUpperCase() + s.slice(1);

/**
 * What a nation can now do with plans it holds, one sentence for each arm: "Your shipyards can now lay down
 * battleships and aircraft carriers on German designs."
 */
export function canNowWords(items: PlanItem[], adj: string): string {
  const by = new Map<string, PlanItem[]>();
  for (const x of items) by.set(PLAN_WORDS[x].who, [...(by.get(PLAN_WORDS[x].who) ?? []), x]);
  const out: string[] = [];
  for (const [who, xs] of by) {
    const verbs = new Map<string, string[]>();
    for (const x of xs) verbs.set(PLAN_WORDS[x].verb, [...(verbs.get(PLAN_WORDS[x].verb) ?? []), PLAN_WORDS[x].many]);
    out.push(`${cap(who)} can now ${listOf([...verbs].map(([v, ms]) => `${v} ${listOf(ms)}`))} on ${adj} designs.`);
  }
  return out.join(' ');
}

/** True for a great power whose plans of this weapon are there to be taken or stolen: one that makes it itself. */
export function givesPlans(g: Game, nat: string, what: PlanItem): boolean {
  if (!PLANS.great.includes(nat) || !g.nations[nat]) return false;
  return isCapitalHull(what) ? locked(g, nat, `ship:${what}`) === null : hasTech(g, nat, what);
}

export const plansOf = (g: Game, nat: string): PlanHeld[] => g.nations[nat]?.plans ?? [];

/**
 * The great power whose plans of this weapon the nation works to: its own tag for a great power; the mother
 * country's for its kin (for a technology, once she has researched it); otherwise the plans it gained last, or
 * null when it holds none. The 3D game draws a smaller country's battleship, carrier or aircraft as this power's
 * model, in the country's own colours and under its own flag.
 */
export function plansFrom(g: Game, nat: string, what: PlanItem): string | null {
  if (PLANS.great.includes(nat)) return nat;
  const mother = PLANS.kin[nat];
  if (mother !== undefined) return isCapitalHull(what) || hasTech(g, mother, what) ? mother : null;
  const held = plansOf(g, nat).filter((p) => p.what === what);
  return held.length ? held[held.length - 1].from : null;
}

const howToGet = (them: string): string => `Take ground that belongs to one that has ${them}, in a war with it, or send an agent to steal its plans.`;

/** Why the nation cannot lay down this hull for want of its plans, or null. */
export function whyNoPlans(g: Game, nat: string, hull: string): string | null {
  if (!planRules(g) || !isCapitalHull(hull) || plansFrom(g, nat, hull) !== null) return null;
  return `Your shipyards hold no plans of ${PLAN_WORDS[hull].many}: only the great powers draw them. Take ground that belongs to one that builds them, in a war with it, or send an agent to steal its plans.`;
}

/** Why the nation cannot research one of the four locked technologies, or null: the great powers alone develop them. */
export function whyNoTechPlans(g: Game, nat: string, id: string): string | null {
  if (!planRules(g) || !isPlanTech(id) || PLANS.great.includes(nat)) return null;
  const W = PLAN_WORDS[id], mother = PLANS.kin[nat];
  if (mother !== undefined) {
    const M = g.nations[mother];
    return theWords(`${g.nations[nat]?.name ?? nat} has ${W.many} on ${M?.adj ?? mother} designs once ${M?.name ?? mother} has ${W.them}.`);
  }
  return `Only the great powers develop ${W.many}. ${howToGet(W.them)}`;
}

/** True when the nation lays this hull down on plans it took or stole: the tech tree does not hold it back. */
export const onPlans = (g: Game, nat: string, hull: string): boolean =>
  planRules(g) && isCapitalHull(hull) && !buildsOwn(nat) && plansFrom(g, nat, hull) !== null;

/** Gives a smaller nation a great power's plans of one weapon. False when it gains nothing new. */
export function grantPlans(g: Game, nat: string, what: PlanItem, from: string, how: PlanHeld['how']): boolean {
  const n = g.nations[nat];
  if (!planRules(g) || !n || buildsOwn(nat) || !givesPlans(g, from, what)) return false;
  if (plansOf(g, nat).some((p) => p.what === what && p.from === from)) return false;
  (n.plans ??= []).push({ what, from, how, turn: g.turn });
  if (isPlanTech(what)) grantTech(g, nat, what);
  return true;
}

/** capture(): ground a great power owns, taken from it in a war with it, gives the taker that power's plans. */
export function plansOnCapture(g: Game, r: Region, nat: string, old: string): void {
  if (!planRules(g) || buildsOwn(nat) || r.ctrl !== nat || r.owner !== old || !PLANS.great.includes(old) || !atWar(g, nat, old)) return;
  const got = PLAN_ITEMS.filter((x) => grantPlans(g, nat, x, old, 'taken'));
  if (!got.length || nat !== g.human) return;
  const O = g.nations[old];
  headline(g, 'event', `${O.adj} ${listOf(got.map((x) => PLAN_WORDS[x].one))} plans are found at ${r.name}. ${canNowWords(got, O.adj)}`, nat, r.id);
}

/**
 * For the 3D game and the build page: the great power whose plans each nation works to, for each weapon.
 * `{ ESP: { bb: 'GER', cv: null, heavybombers: null, ... }, UK: { bb: 'UK', ... }, CAN: { bb: 'UK', ... } }`.
 */
export function plansView(g: Game): Record<string, Record<PlanItem, string | null>> {
  const out: Record<string, Record<PlanItem, string | null>> = {};
  for (const nat of Object.keys(g.nations)) {
    const row = {} as Record<PlanItem, string | null>;
    for (const x of PLAN_ITEMS) row[x] = plansFrom(g, nat, x);
    out[nat] = row;
  }
  return out;
}
