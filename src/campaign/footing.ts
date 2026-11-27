// The nation's footing (ruling 172 and its two riders, Brian's yes of 17 November 2026; the grand campaign only,
// behind a switch: data/campaign/footing.json 'on', kept with each campaign in g.flags.footing). One lever for the
// share of a country's industry on war work, and four footings:
//
//   Peacetime economy   at peace, what a country has always had: half of industry, or its own peace table
//   Rearmament          at peace only: the share climbs a quarter of industry higher, a step a month
//   War footing         at war, what every country has always had: all of industry, or America's climb to it
//   All-out war         at war a year, or with home ground lost: a second shift, a quarter more than all
//
// The ledger's line, the cabinet's old ladder for the economy (its 6% and 12%, which this replaces) and America's
// climb become this one rule. Untouched means unchanged: a country that never touches the lever earns exactly what
// it earned before, and with the switch off nothing here runs at all.
//
// The price is paid at home: political capital to order and each month to hold (the footing slips a step the month
// there is not enough), the war cabinet's trust where it sits, and for all-out war the will to fight, worn by
// rationing and empty shops: a twentieth a year, a tenth at most, a count of its own above what shortage takes,
// mended at twice the pace once the country steps down.
//
// The computer's countries use the same lever at the same price when the staffs' switch is on (g.flags.footStaffs):
// by the book at stops 1 and 2 of the history dial, by judgment at 3 to 5. A rival's footing is public.
//
// The hooks elsewhere are one line each: economy.ts (the share and the ledger's line), world.ts (America's climb
// starts from where rearmament left it), resources.ts (the will), government.ts (the cabinet's trust and the old
// ladder), game.ts (political capital each month), diplomacy.ts (the month), staff.ts (the switch, as a campaign
// opens), turn.ts (the player's orders).

import data from '../../data/campaign/footing.json';
import { collapseScore, headline, homeLost, theName } from './game';
import { GOV } from './government';
import { HISTORY, stopDef, stopOf } from './history';
import { readable } from './claims';
import { majorsOf, pcIncome } from './diplomacy';
import { civicSum } from './buildings';
import { PRODUCTION, income } from './economy';
import { peaceFooting, warFooting } from './world';
import type { Footing, Game, Nation } from './types';

export type FootingId = 'peace' | 'rearm' | 'war' | 'allout';
type Step = 'rearm' | 'allout';
interface StepRule { cost: number; hold: number; trust: number }
interface BookRow { allout?: number | 'never' | 'invaded'; rearm?: number; lift?: number }

export const FOOTING = data as unknown as {
  on: boolean;
  staffs: boolean;
  /** How far the share climbs in a month: America's own climb to war work is the model. */
  step: number;
  /** Months after a step down before any footing may be raised again. */
  wait: number;
  rearm: StepRule & { lift: number };
  allout: StepRule & { share: number; wear: number; most: number; mend: number; amberAt: number };
  names: Record<FootingId, string>;
  /** 'peaceKept': countries whose staffs keep their peace table at the stops of judgment (the rider's (p): the Soviet Union). */
  judge: { margin: number; rearmStops: number[]; peaceKept: string[] };
  book: Record<string, BookRow>;
  tips: { door: string; allout: string };
};

/** The four footings, lowest first. */
export const FOOTINGS: FootingId[] = ['peace', 'rearm', 'war', 'allout'];

/** True in a campaign played with the lever: the grand campaign, begun with the switch on. */
export const footingOn = (g: Game): boolean => g.flags.open === 1 && g.flags.footing === 1;
/** True where the computer's countries use the lever too. */
const staffsOn = (g: Game): boolean => footingOn(g) && g.flags.footStaffs === 1;

/** Sets the switch for a grand campaign as it opens, from the data: a campaign keeps the setting it began with. */
export function openFooting(g: Game): void {
  if (g.flags.open !== 1 || !FOOTING.on) return;
  g.flags.footing = 1;
  if (FOOTING.staffs) g.flags.footStaffs = 1;
}

/** Switches the lever on or off in a campaign, and the staffs' use of it (for tests, probes and measures). */
export function setFooting(g: Game, on: boolean, staffs = false): void {
  if (on && g.flags.open === 1) g.flags.footing = 1; else delete g.flags.footing;
  if (on && staffs && g.flags.open === 1) g.flags.footStaffs = 1; else delete g.flags.footStaffs;
}

// ---- The share of industry on war work ------------------------------------------------------------------------

/** The share that comes of itself: the peace table or half at peace; all of it at war, or America's climb to it. */
function baseShare(g: Game, nat: string): number {
  return g.nations[nat].atWar.length ? warFooting(g, nat) ?? 1 : peaceFooting(g, nat) ?? PRODUCTION.peaceIncome;
}
const peaceBase = (g: Game, nat: string): number => peaceFooting(g, nat) ?? PRODUCTION.peaceIncome;

/** The book the computer's staff of this country keeps, at the stops of the dial that keep one. */
const bookOf = (g: Game, nat: string): BookRow | undefined => (stopDef(g).book && nat !== g.human ? FOOTING.book[nat] : undefined);

/** How far above its peace figure rearmament takes a country: a quarter of industry, or the book's own size. */
const liftOf = (g: Game, nat: string): number => bookOf(g, nat)?.lift ?? FOOTING.rearm.lift;

/** The ordered step that stands: Rearmament while at peace, All-out war while at war; null for none. */
function stepIn(n: Nation): Step | null {
  const s = n.foot?.step;
  if (!s) return null;
  return (s === 'rearm') === (n.atWar.length === 0) ? s : null;
}

/** Where a footing's climb ends: the share of industry on war work once it is done. */
function targetOf(g: Game, nat: string, id: FootingId): number {
  if (id === 'allout') return FOOTING.allout.share;
  if (id === 'war') return 1;
  if (id === 'rearm') return Math.min(1, peaceBase(g, nat) + liftOf(g, nat));
  return peaceBase(g, nat);
}

/** The footing a country stands on now. */
export function footingId(g: Game, nat: string): FootingId {
  const n = g.nations[nat];
  return stepIn(n) ?? (n.atWar.length ? 'war' : 'peace');
}

/** A forced share, while the card weighs what a step would bring: the income at its end, against the income now. */
let forced: { nat: string; share: number } | null = null;

/**
 * The share of a country's industry on war work, given the share that would come of itself (the hook in
 * economy.ts): unchanged with the switch off, and for any country not on an ordered footing.
 */
export function footShare(g: Game, nat: string, base: number): number {
  if (!footingOn(g)) return base;
  if (forced && forced.nat === nat) return forced.share;
  const n = g.nations[nat], s = stepIn(n);
  return s && (n.foot!.share ?? 0) > base ? n.foot!.share! : base;
}

/** Where America's climb to war work starts when its war comes: from where rearmament left it (the hook in world.ts). */
export function rearmedAt(g: Game, nat: string): number {
  if (!footingOn(g)) return 0;
  const f = g.nations[nat]?.foot;
  return f?.step === 'rearm' ? f.share ?? 0 : 0;
}

/** Months to the end of the climb from this share. */
const monthsTo = (share: number, target: number): number => (share >= target - 1e-9 ? 0 : Math.ceil((target - share) / FOOTING.step - 1e-9));
const idx = (x: number): number => Math.round(x * 100);
const inMonths = (m: number): string => (m === 1 ? 'a month' : `${m} months`);

/**
 * The ledger's line, the door to the lever (rider 2 (s), (t)): it never drops out, and it always gives the number.
 * `share` is the share the ledger is counting with; `old` the words it had before the lever.
 */
export function footLabel(g: Game, nat: string, share: number, old: string): string {
  return footingOn(g) ? footingLine(g, nat, share) : old;
}

/** The ledger's line for a country's footing, in the words both games use. */
export function footingLine(g: Game, nat: string, share = footShare(g, nat, baseShare(g, nat))): string {
  const id = footingId(g, nat), t = targetOf(g, nat, id), left = monthsTo(share, t);
  if (id === 'peace') return `At peace: ${idx(share)} in 100 of industry on war work`;
  if (id === 'rearm') return left ? `Rearmament: ${idx(share)} in 100 of industry on war work so far, ${idx(t)} in 100 in ${inMonths(left)}` : `Rearmament: ${idx(t)} in 100 of industry on war work`;
  if (id === 'war') {
    const climb = monthsTo(share, 1);
    return climb ? `At war: ${idx(share)} in 100 of industry on war work so far, all of it in ${inMonths(climb)}` : 'At war: all of industry on war work';
  }
  if (share < 1 - 1e-9) return `All-out war: ${idx(share)} in 100 of industry on war work so far, ${idx(t)} in 100 in ${inMonths(left)}`;
  return left ? `All-out war: all of industry on war work, and the second shift adds ${idx(share - 1)} in 100 so far, ${idx(t - 1)} in 100 in ${inMonths(left)}` : 'All-out war: all of industry on war work, on two shifts';
}

/** True where the ledger keeps the footing's line though it changes nothing this month (rider 2 (s)). */
export const footLineStays = (g: Game, key: string): boolean => key === 'peace' && footingOn(g);

// ---- The price ------------------------------------------------------------------------------------------------

/** Months of war a country has behind it: the longest of its wars still going. */
function warMonths(g: Game, nat: string): number {
  let most = 0;
  for (const foe of g.nations[nat].atWar) {
    const since = g.flags[`war:${nat}:${foe}`] ?? g.flags[`war:${foe}:${nat}`];
    most = Math.max(most, g.turn - (since ?? 1));
  }
  return most;
}

/** True once all-out war is a country's to order by the rule: a year at war, or home ground lost (the cabinet's own test). */
export const mayGoAllOut = (g: Game, nat: string): boolean => g.nations[nat].atWar.length > 0 && (warMonths(g, nat) >= GOV.needs.longMonths || homeLost(g, nat) > 0);

/**
 * What holding the footing takes from political capital this month, given what comes in otherwise (the hook in
 * game.ts): 0 with the switch off or on a footing that comes of itself. The month there is not enough, the footing
 * slips a step and nothing is taken.
 */
export function footHold(g: Game, nat: string, net: number): number {
  if (!footingOn(g)) return 0;
  const n = g.nations[nat], s = stepIn(n);
  if (!s) return 0;
  const hold = FOOTING[s].hold;
  if (n.pc + net - hold >= -1e-9) return -hold;
  stepDown(g, nat, 'capital');
  return 0;
}

/** What the player's war cabinet loses in trust this month for his footing, where it sits (the hook in government.ts). */
export function footTrust(g: Game, nat: string): number {
  if (!footingOn(g)) return 0;
  const s = stepIn(g.nations[nat]);
  return s ? FOOTING[s].trust : 0;
}

/** The share of the will to fight all-out war has worn away: its own count, above what shortage takes (the hook in resources.ts). */
export const footWorn = (g: Game, nat: string): number => (footingOn(g) ? g.nations[nat]?.foot?.worn ?? 0 : 0);

/** True while the footing replaces the cabinet's old ladder for the economy (government.ts). */
export const replacesLadder = (g: Game): boolean => footingOn(g);
export const LADDER_GONE = 'The economy is set by the nation\'s footing now: the line under the arsenal, where you buy.';

// ---- Orders ---------------------------------------------------------------------------------------------------

/** Why a country cannot be put on this footing now, or null if it can. Stepping down is always free. */
export function whyNotFooting(g: Game, nat: string, to: FootingId): string | null {
  if (!footingOn(g)) return 'The nation\'s footing is set in the grand campaign.';
  const n = g.nations[nat];
  if (!n?.alive) return 'That country is no more.';
  if (!FOOTINGS.includes(to)) return 'There is no such footing.';
  const now = footingId(g, nat), war = n.atWar.length > 0;
  if (to === now) return 'That is the footing already.';
  if (war && (to === 'peace' || to === 'rearm')) return 'At peace only.';
  if (!war && to === 'war') return 'Comes of itself the day you are at war: no order, no price.';
  if (!war && to === 'allout') return `At war only: after ${GOV.needs.longMonths} months of it, or once home ground is lost.`;
  if (to === 'peace' || to === 'war') return null;
  const wait = (n.foot?.wait ?? 0) - g.turn;
  if (wait > 0) return `Stepped down lately: it cannot be raised again for ${inMonths(wait)}.`;
  if (to === 'allout' && !mayGoAllOut(g, nat)) return `The country will bear this after ${GOV.needs.longMonths} months of war, or once home ground is lost.`;
  const cost = FOOTING[to].cost;
  if (n.pc < cost) return `It takes ${cost} political capital to order: you have ${Math.floor(n.pc)}.`;
  return null;
}

/** Puts a country on a footing: one step up, for its price in political capital, or back down, at once and free. */
export function orderFooting(g: Game, nat: string, to: FootingId): boolean {
  if (whyNotFooting(g, nat, to)) return false;
  const n = g.nations[nat];
  if (to === 'peace' || to === 'war') { stepDown(g, nat, 'order'); return true; }
  n.pc -= FOOTING[to].cost;
  const f = (n.foot ??= {});
  f.step = to;
  f.share = baseShare(g, nat);
  f.since = g.turn;
  const t = targetOf(g, nat, to), name = FOOTING.names[to];
  if (nat === g.human) {
    headline(g, 'info', to === 'rearm'
      ? `${name} ordered: industry climbs to ${idx(t)} in 100 on war work, a step a month, in ${inMonths(monthsTo(f.share, t))}. Holding it takes ${FOOTING.rearm.hold} political capital a month.`
      : `${name} ordered: the factories go over to a second shift, ${idx(t)} in 100 in ${inMonths(monthsTo(Math.max(1, f.share), t))}. It takes ${FOOTING.allout.hold} political capital a month to hold, and rationing and empty shops wear the will to fight: ${pct(FOOTING.allout.wear * 12)} in a year, ${pct(FOOTING.allout.most)} at most.`, nat);
  } else headline(g, 'info', to === 'rearm' ? `${theName(n, true)} rearms: more of its industry goes over to war work.` : `${theName(n, true)} goes over to all-out war: its factories work a second shift.`, nat);
  return true;
}

const pct = (x: number): string => `${Math.round(x * 100)}%`;

/** A step down to the footing that comes of itself: by order, for want of political capital, or by its staff's word. */
function stepDown(g: Game, nat: string, why: 'order' | 'capital'): void {
  const n = g.nations[nat], f = n.foot;
  if (!f?.step) return;
  const was = f.step, to: FootingId = n.atWar.length ? 'war' : 'peace';
  delete f.step; delete f.share; delete f.since;
  f.wait = g.turn + FOOTING.wait;
  const name = FOOTING.names[to].toLowerCase();
  if (nat === g.human) {
    headline(g, 'info', why === 'capital'
      ? `The footing slips back to the ${name}: there is no political capital left to hold ${FOOTING.names[was].toLowerCase()}. It cannot be raised again for ${inMonths(FOOTING.wait)}.`
      : `Back to the ${name}: ${was === 'allout' ? 'the second shift is stood down' : 'rearmament is called off'}. It cannot be raised again for ${inMonths(FOOTING.wait)}.`, nat);
  } else headline(g, 'info', was === 'allout' ? `${theName(n, true)} stands down from all-out war.` : `${theName(n, true)} calls off its rearmament.`, nat);
}

// ---- The month ------------------------------------------------------------------------------------------------

/**
 * The month's turn of the lever, for every country, as the month opens (after America's own climb, world.ts):
 * war or peace that has come puts a country on the footing that comes with it; the computer's staffs decide; each
 * climb takes its step; and all-out war wears the will to fight, or the will mends.
 */
export function footingMonth(g: Game): void {
  if (!footingOn(g)) return;
  const near = staffsOn(g) ? warBorders(g) : null;
  for (const n of Object.values(g.nations)) {
    const f = n.foot;
    // War has come to a country that was rearming, or peace to one on all-out war: it stands on the footing that comes with it.
    if (f?.step && !stepIn(n)) { delete f.step; delete f.share; delete f.since; }
    if (near && n.alive && !n.dormant && n.id !== g.human) staffFooting(g, n, near);
    const s = stepIn(n);
    if (s && n.alive) climb(g, n, s);
    wear(g, n, s);
    if (n.foot && n.foot.step === undefined && (n.foot.wait ?? 0) <= g.turn && !n.foot.worn && n.foot.amber === undefined) delete n.foot;
  }
}

function climb(g: Game, n: Nation, s: Step): void {
  const f = n.foot!, base = baseShare(g, n.id), t = targetOf(g, n.id, s);
  const was = Math.max(base, f.share ?? base);
  const up = Math.min(t, was + FOOTING.step);
  f.share = Math.max(base, up >= t - 1e-9 ? t : up);
  if (was >= t - 1e-9 || f.share < t - 1e-9) return;
  const name = FOOTING.names[s];
  if (n.id === g.human) headline(g, 'info', s === 'rearm' ? `${name} is complete: ${idx(t)} in 100 of industry on war work.` : `${name}: all of industry on war work, on two shifts.`, n.id);
}

function wear(g: Game, n: Nation, s: Step | null): void {
  const A = FOOTING.allout;
  if (s === 'allout' && n.alive) {
    const f = n.foot!, was = f.worn ?? 0;
    f.worn = Math.min(A.most, was + A.wear);
    if (f.amber === undefined && f.worn >= A.amberAt - 1e-9) {
      f.amber = g.turn;
      if (n.id === g.human) headline(g, 'info', `All-out war wears at home: rationing and empty shops have worn away ${pct(f.worn)} of the will to fight, and it goes on at ${pct(A.wear * 12)} a year, ${pct(A.most)} at most. Stepping down to the war footing mends it.`, n.id, undefined);
    }
    return;
  }
  const f = n.foot;
  if (!f?.worn) return;
  f.worn = Math.max(0, f.worn - A.mend);
  if (f.worn <= 1e-9) delete f.worn;
}

// ---- The computer's staffs ------------------------------------------------------------------------------------

/** The countries a great power at war stands beside: its ground touches theirs. */
function warBorders(g: Game): Set<string> {
  const out = new Set<string>();
  const warring = new Set(majorsOf(g).filter((id) => g.nations[id]?.alive && g.nations[id].atWar.length > 0));
  if (!warring.size) return out;
  for (const r of g.regions) {
    if (!warring.has(r.ctrl)) continue;
    for (const j of r.nb) { const o = g.regions[j]; if (o.ctrl !== r.ctrl && o.owner === o.ctrl) out.add(o.ctrl); }
  }
  return out;
}

/** True where a country could pay for a footing and hold it from what comes in each month. */
function affords(g: Game, nat: string, s: Step): boolean {
  const n = g.nations[nat], R = FOOTING[s];
  return n.pc >= R.cost && pcIncome(nat, g) + civicSum(g, nat).pc >= R.hold;
}

/**
 * A computer staff's word on its country's footing. By the book (stops 1 and 2): history's month, and nobody steps
 * back while at war; at peace only the Soviet Union rearms. By judgment (stops 3 to 5): up when the rule allows,
 * the price can be held, and the will can bear a tenth more; down when the will is within a fifth of its limit. At
 * peace a staff keeps the peace table, except at stops 4 and 5 where an ambition of its own or a great power's war on
 * its border gives it cause to rearm.
 */
function staffFooting(g: Game, n: Nation, near: Set<string>): void {
  const nat = n.id, now = footingId(g, nat), war = n.atWar.length > 0, book = stopDef(g).book;
  if (!war) {
    if (now === 'rearm') return;
    const want = book ? (bookOf(g, nat)?.rearm ?? Infinity) <= g.turn
      : FOOTING.judge.rearmStops.includes(stopOf(g)) && !FOOTING.judge.peaceKept.includes(nat) && (near.has(nat) || HISTORY.ambitions.some((a) => a.nat === nat && readable(g, a)));
    if (want && affords(g, nat, 'rearm')) orderFooting(g, nat, 'rearm');
    return;
  }
  if (book) {
    if (now === 'allout') return;
    const at = bookOf(g, nat)?.allout;
    const due = at === 'invaded' ? homeLost(g, nat) > 0 : typeof at === 'number' && g.turn >= at;
    if (due) orderFooting(g, nat, 'allout');
    return;
  }
  if (n.resilience <= 0) return;
  const limit = (1 - FOOTING.judge.margin) * n.resilience, score = collapseScore(g, nat);
  if (now === 'allout') { if (score >= limit) orderFooting(g, nat, 'war'); return; }
  if (score + FOOTING.allout.most < limit && affords(g, nat, 'allout')) orderFooting(g, nat, 'allout');
}

// ---- What both games show -------------------------------------------------------------------------------------

export interface FootingRow {
  id: FootingId;
  name: string;
  /** All of industry on one shift = 100. */
  index: number;
  /** The one in force; one that can be ordered now; or one that cannot, with its sentence. */
  state: 'on' | 'open' | 'locked';
  why: string | null;
}

/** The one move the card asks about: one step up, or back down. */
export interface FootingMove {
  to: FootingId;
  name: string;
  up: boolean;
  /** What it brings into the treasury a month once its climb is done (negative for a step down), as the ledger counts it. */
  brings: number;
  /** Months the climb takes from where it stands. */
  months: number;
  /** For all-out war: what it wears from the will, a year and at most, as shares. */
  will: { year: number; most: number } | null;
  /** Political capital to order, and each month to hold; the cabinet's trust each month where it sits, or null. */
  cost: number;
  hold: number;
  trust: number | null;
  /** Months after a step down before any footing may be raised again. */
  again: number;
  /** Why it cannot be ordered now, or null. */
  why: string | null;
}

export interface FootingView {
  nat: string;
  id: FootingId;
  name: string;
  /** The share now, and its index (one shift = 100). */
  share: number;
  index: number;
  /** The ledger's line, word for word. */
  line: string;
  /** Months left in the climb, 0 when done or when nothing climbs. */
  months: number;
  rows: FootingRow[];
  move: FootingMove | null;
  /** What all-out war has worn from the will, as a share; amber from a twentieth (rider 2 (u), (y)). */
  worn: number;
  amber: boolean;
  /** The tip that fits the month, for the player: at peace on the peace table, or the first months all-out war is his to order. */
  tip: 'door' | 'allout' | null;
}

/** A country's footing as both games show it: the line, the four footings as a tally with their locks, and the one move the card asks about. Null with the switch off. */
export function footingView(g: Game, nat: string | null = g.human): FootingView | null {
  if (!nat || !footingOn(g) || !g.nations[nat]) return null;
  const n = g.nations[nat], id = footingId(g, nat), share = footShare(g, nat, baseShare(g, nat));
  const rows: FootingRow[] = FOOTINGS.map((x) => {
    const why = x === id ? null : whyNotFooting(g, nat, x);
    return { id: x, name: FOOTING.names[x], index: idx(targetOf(g, nat, x)), state: x === id ? 'on' : why ? 'locked' : 'open', why };
  });
  const war = n.atWar.length > 0, onStep = id === 'rearm' || id === 'allout';
  const to: FootingId = onStep ? (war ? 'war' : 'peace') : war ? 'allout' : 'rearm';
  const why = whyNotFooting(g, nat, to);
  // A step that could not be asked about at all (all-out war before a year at war) still shows what it would bring.
  const target = targetOf(g, nat, to);
  const brings = weigh(g, nat, onStep ? baseShare(g, nat) : target) - income(g, nat);
  const step = to === 'rearm' || to === 'allout' ? FOOTING[to] : null;
  const move: FootingMove = {
    to, name: FOOTING.names[to], up: !onStep, brings,
    months: onStep ? 0 : monthsTo(to === 'allout' ? Math.max(1, baseShare(g, nat)) : baseShare(g, nat), target),
    will: to === 'allout' ? { year: FOOTING.allout.wear * 12, most: FOOTING.allout.most } : null,
    cost: step?.cost ?? 0, hold: step?.hold ?? 0,
    trust: step && n.gov && nat === g.human && g.flags.nogov !== 1 ? step.trust : null,
    again: FOOTING.wait, why,
  };
  const worn = n.foot?.worn ?? 0;
  const tip = nat !== g.human ? null : !war && id === 'peace' && !n.foot ? 'door' : war && id === 'war' && mayGoAllOut(g, nat) && !n.foot ? 'allout' : null;
  return {
    nat, id, name: FOOTING.names[id], share, index: idx(share), line: footingLine(g, nat, share),
    months: onStep ? monthsTo(share, targetOf(g, nat, id)) : id === 'war' ? monthsTo(share, 1) : 0,
    rows, move, worn, amber: worn >= FOOTING.allout.amberAt - 1e-9, tip,
  };
}

/** The income a month this country would have with its share of industry on war work at this figure. */
function weigh(g: Game, nat: string, share: number): number {
  forced = { nat, share };
  try { return income(g, nat); } finally { forced = null; }
}

/** One word for a country's footing, for the Powers tab: public, like its alliances. Null with the switch off. */
export const footingWord = (g: Game, nat: string): string | null => (footingOn(g) && g.nations[nat]?.alive ? FOOTING.names[footingId(g, nat)] : null);
