// The arsenal of the grand campaign: everything a nation can order with its treasury. Corps of every kind,
// aircraft by the wing, warships and troopships, and the works a province can be given: factories, shipyards,
// anti-aircraft guns and airfields. One list of offers says what can be ordered, where it would go, what it costs
// to buy and to keep, and why not when it cannot be; one function places the order. Both interfaces read the
// same list. The numbers live in data/campaign/arsenal.json. The historical scenarios offer only the four
// classic corps and fortifications, as they always did.

import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import { byHand } from './depth';
import data from '../../data/campaign/arsenal.json';
import { worksName } from './rivers';
import { AIR, AIRWAR, airFleetsOf, airPool, airRange, airWar, airfieldsHold, carrierRules, cover, foundAirFleet, hasAirfield, joinPilots, km, poolIn, whyNotBase } from './air';
import { fieldRules, fieldTier, setConcrete } from './fieldstate';
import { syncWings } from './carriers';
import { MANPOWER, classNow, menLeft, menRules, menSpare, menWords, raisable } from './manpower';
import { govMul } from './government';
import { settledFor, settledTownsOn, whyNotWorksHere } from './buildings';
import { cityDefences, lightsAt, nightRaided } from './citydefences';
import { AC } from './aircampaigns';
import { BUDGET, PRODUCTION, budgeted, cadreTerms, income, upkeepTotal, cadresOf, canRaise, corpsCost, corpsKinds, corpsList, corpsUpkeep, fortCost, isHome, needsOf, orderCadre, orderCorps, orderElite, orderFort, orderPara, raiseSite, specOf, whyNoCadre, type LedgerLine } from './economy';
import { TECH, hasTech, locked, paraCount, whyNoPara } from './tech';
import { eliteCount, eliteOf, eliteRule, priceOf, whyNoElite } from './traits';
import { allied, atWar, headline, theName } from './game';
import { majorsOf } from './diplomacy';
import { stopOf } from './history';
import { turnToDate } from '../core/ticket';
import { NAVY, fleetsOf, foundFleet, liftLeft, shipCount } from './navy';
import { onPlans, whyNoPlans } from './plans';
import { chartDist } from './wrap';
import { mendStars } from './stars';
import { MEN_PER_DIV, type AirFleet, type Build, type Fleet, type Game, type Kind, type Region, type ShipClass } from './types';
import { fighterTypesOn, joinBought, sellerType } from './fighters';

export type Wing = 'ftr' | 'bmr';
export type Hull = ShipClass | 'tp';
export type Work = 'factory' | 'yard' | 'aa' | 'field' | 'lights';

interface WingSpec { label: string; count: number; cost: number; months: number; factories: number; blurb: string }
interface HullSpec { label: string; count: number; cost: number; months: number; yard: number; blurb: string }
interface LevelSpec { label: string; cost: number[]; months: number; blurb: string }

export const ARSENAL = data as unknown as {
  needs: Record<string, number>;
  air: Record<Wing, WingSpec>;
  ships: Record<Hull, HullSpec>;
  lift: { most: number };
  works: {
    factory: { label: string; cost: number; step: number; months: number; most: { city: number; town: number }; blurb: string };
    yard: LevelSpec;
    aa: LevelSpec & { upkeep: number; cut: number; flak: number };
    field: LevelSpec & { upkeep: number; holds: number };
    /** Airfields as targets: a field strip (grass, steel mat, coral), offered only with that switch. */
    strip: LevelSpec & { upkeep: number; holds: number };
    /** City defences (release 2): searchlights, offered, built and billed only with that switch (citydefences.ts). */
    lights: LevelSpec & { upkeep: number };
  };
  yards: Record<string, number>;
  guns: { soften: number; softenMost: number; thicken: number; thickenMost: number };
  /** The computer staffs' own orders (see the note in the data): 'programme' is their ships and aircraft (rulings 68(b) and 70(a)). */
  ai?: { programme?: { fromStop: number; bled: number; share: number; each: Record<string, number>; rate?: { ship: number; air: number } } };
  /** Ruling 213(d) and the works rates of 218(d): a smaller power's aircraft works and the record's aircraft from abroad (see the note in the data). */
  smallAir: {
    perPoint: number;
    works: Record<string, { ftr: number | null; bmr: number | null; rate: [number | null, number | null]; earlier?: { until: number; rate: [number | null, number | null] } }>;
    gifts: [number, string, number, 'ftr' | 'bmr' | 'mixed', string[]][];
    /** Wings bought abroad (Brian's rule of 21 November 2026; see the note in the data). Absent: nothing is sold. */
    abroad?: { on: boolean; price: number; months: Record<Wing, number> };
  };
};

const TOP = 3;
const plural = (n: number, one: string, many = `${one}s`): string => `${n} ${n === 1 ? one : many}`;
const cap = (s: string): string => s[0].toUpperCase() + s.slice(1);

// ---- Works: what has been built in each province -------------------------------------------------------
// Levels are kept as marks on the game ('wk:aa:17' = 2), so a save carries them and a game that never builds
// any is exactly the game it was before.

/** The level of anti-aircraft guns, airfields or searchlights built in a province. (Searchlights are read for a rule
 * through citydefences.ts lightsAt, which asks the switch first: this is the bare mark.) */
export const workLevel = (g: Game, what: 'aa' | 'field' | 'lights', region: number): number => g.flags[`wk:${what}:${region}`] ?? 0;

/** How many factories have been added to a province since the game began. */
export const factoriesBuilt = (g: Game, region: number): number => g.flags[`wk:fac:${region}`] ?? 0;

/** A port's shipyard: what history left there, or what has been built since, whichever is greater. */
export function yardLevel(g: Game, r: Region): number {
  return r.port ? Math.max(g.flags[`wk:yard:${r.id}`] ?? 0, ARSENAL.yards[r.name] ?? 0) : 0;
}

/** A city: a province where a factory can be built. */
export const isCity = (r: Region): boolean => r.terrain === 'urban' || r.cap || r.fac > 0 || r.vp >= 2;

const queued = (g: Game, nat: string, item: string, region: number): number => g.nations[nat].queue.filter((b) => b.what === 'work' && b.item === item && b.region === region).length;

/** True for an order of a field strip (airfields as targets): a 'field' work marked kind 'strip'. */
export const isStripBuild = (b: Build): boolean => b.what === 'work' && b.item === 'field' && (b as { kind?: string }).kind === 'strip';

/**
 * The name of a work as every reader shows it (the book, the plate, the card, the ledger, the news): release 1's
 * label, except that with airfields as targets a level of 'field' is a "Concrete airfield", or a "Field strip" where
 * its order is marked 'strip'.
 */
export function workLabel(g: Game, item: string, kind?: string): string {
  if ((item === 'field' || item === 'strip') && fieldRules(g)) return kind === 'strip' || item === 'strip' ? ARSENAL.works.strip.label : 'Concrete airfield';
  return (ARSENAL.works as unknown as Record<string, { label: string }>)[item]?.label ?? 'Works';
}

/** What guns and airfields (and, with city defences, searchlights) cost a nation to man each month, as lines for its ledger. */
export function worksUpkeep(g: Game, nat: string): LedgerLine[] {
  let aa = 0, field = 0, strip = 0, lights = 0;
  // (City defences: the searchlights are counted only with the switch on; off, a mark left on the ground costs nothing.)
  const lit = cityDefences(g);
  // Province by province: the levels are whole numbers, so the order they are added in changes nothing.
  for (const r of g.regions) {
    if (r.ctrl !== nat) continue;
    aa += g.flags[`wk:aa:${r.id}`] ?? 0;
    field += g.flags[`wk:field:${r.id}`] ?? 0;
    // (Airfields as targets: the levels laid as strips cost a strip's upkeep. Never set with the switch off.)
    strip += g.flags[`wk:strip:${r.id}`] ?? 0;
    if (lit) lights += g.flags[`wk:lights:${r.id}`] ?? 0;
  }
  const out: LedgerLine[] = [];
  if (aa > 0) out.push({ key: 'works:aa', label: 'Anti-aircraft guns', amount: aa * ARSENAL.works.aa.upkeep, count: aa });
  if (field > 0) out.push({ key: 'works:field', label: 'Airfields', amount: strip > 0 ? (field - strip) * ARSENAL.works.field.upkeep + strip * ARSENAL.works.strip.upkeep : field * ARSENAL.works.field.upkeep, count: field });
  if (lights > 0) out.push({ key: 'works:lights', label: ARSENAL.works.lights.label, amount: lights * ARSENAL.works.lights.upkeep, count: lights });
  return out;
}

/** On loading a save: the factories built since the game began go back on the map. */
export function restoreWorks(g: Game): void {
  for (const k in g.flags) if (k.startsWith('wk:fac:')) { const r = g.regions[+k.slice(7)]; if (r) r.fac += g.flags[k]; }
}

// ---- Where things go -------------------------------------------------------------------------------

/** True if this nation has an aircraft industry: a home province with this many factories. */
const industry = (g: Game, nat: string, factories: number): boolean => g.regions.some((r) => isHome(g, nat, r) && r.fac >= factories);

// ---- A smaller power's industry (ruling 213, the grand campaign) -----------------------------------------------

const SMALL = () => ARSENAL.smallAir;
const WING_WORD: Record<Wing, string> = { ftr: 'fighters', bmr: 'bombers' };

/** True for a great power: its aircraft industry is its factories, and its factories are as many as it can pay for. */
export const greatPower = (g: Game, nat: string): boolean => majorsOf(g).includes(nat);

/** Ruling 213(d): the month from which a smaller power's own works build this kind (1 = September 1939), or null for never. */
export const worksFrom = (nat: string, wing: Wing): number | null => SMALL().works[nat]?.[wing] ?? null;

/** The most a smaller power's works build of this kind a month, in aircraft (ruling 218(d), the historian's rates). */
export function worksRate(g: Game, nat: string, wing: Wing): number {
  const w = SMALL().works[nat];
  if (!w) return 0;
  const rate = w.earlier && g.turn <= w.earlier.until ? w.earlier.rate : w.rate;
  return (rate[wing === 'ftr' ? 0 : 1] ?? 0) * SMALL().perPoint;
}

/**
 * Why a smaller power cannot order a wing of this kind this month, in the words passed for ruling 213(d), or null when
 * its works build it. A works that will open names its month, whatever the country builds today.
 */
export function whyNoWorks(g: Game, nat: string, wing: Wing): string | null {
  const name = g.nations[nat].name;
  const w = SMALL().works[nat];
  if (!w || (w.ftr === null && w.bmr === null)) return `${name} has no aircraft industry: more aircraft come only from abroad.`;
  const from = w[wing], other: Wing = wing === 'ftr' ? 'bmr' : 'ftr';
  if (from !== null && (from > g.turn || worksRate(g, nat, wing) <= 0)) {
    // Shut until its month, or building none of this kind at its earlier rate: open the month after that rate ends.
    const at = worksOpensAt(g, nat, wing)!;
    return `${name} builds no ${WING_WORD[wing]} before ${turnToDate(at).label}: until then more ${WING_WORD[wing]} come only from abroad.`;
  }
  if (from === null) return `${name} builds ${WING_WORD[other]} at home, but no ${WING_WORD[wing]}: more ${WING_WORD[wing]} come only from abroad.`;
  return null;
}

/**
 * True for a nation with aircraft works of its own that build this kind, open yet or not: a great power's home province
 * with the factories a wing is built in, or a smaller power's works on the record (ruling 213(d)). Plans stolen need
 * works to build from (the fighter's plans, the design director 23 Nov 07:00Z).
 */
export function hasAircraftWorks(g: Game, nat: string, wing: Wing): boolean {
  if (budgeted(g) && !greatPower(g, nat)) return (SMALL().works[nat]?.[wing] ?? null) !== null;
  return industry(g, nat, ARSENAL.air[wing].factories);
}

/** The month a shut works opens for this kind, or null where it never builds it (the same reckoning as whyNoWorks). */
function worksOpensAt(g: Game, nat: string, wing: Wing): number | null {
  const w = SMALL().works[nat];
  const from = w?.[wing] ?? null;
  if (!w || from === null) return null;
  return Math.max(from, g.turn + 1, w.earlier && !(w.earlier.rate[wing === 'ftr' ? 0 : 1] ?? 0) ? w.earlier.until + 1 : 0);
}

// ---- Wings bought abroad (Brian, 21 November 2026: every battle mechanic applies to all playable nations) ----------

/** The data's terms for a wing bought abroad (read at call time, so a probe can set them), or undefined where there are none. */
export const ABROAD = (): { on: boolean; price: number; months: Record<Wing, number> } | undefined => SMALL().abroad;

/**
 * True where a smaller power's wings may be bought abroad: the grand campaign, the data's switch on, and in release 1
 * the PLAYER'S own order only (his nation, and he runs his building himself). No staff buys abroad, a player's own
 * staff included, so a war fought by staffs alone, or by a player who gives no orders, is the war it was.
 */
export const abroadRules = (g: Game, nat: string): boolean => budgeted(g) && ABROAD()?.on === true && !greatPower(g, nat) && byHand(g, nat, 'build');

/**
 * The great power that would sell a smaller power its wings (the design director, 21 November 2026): one that stands,
 * is not at war with the buyer and is not allied to a country at war with the buyer (Germany held back Finland's
 * orders in the Winter War). An ally of the buyer before any other, then the nearest, capital to capital. Null: none will.
 */
export function sellerOf(g: Game, nat: string): string | null {
  const me = g.nations[nat];
  if (!me?.alive) return null;
  const foes = Object.keys(g.nations).filter((x) => x !== nat && g.nations[x].alive && atWar(g, nat, x));
  const home = g.regions[me.capital];
  let best: string | null = null, rank = Infinity;
  for (const x of majorsOf(g)) {
    const n = g.nations[x];
    if (x === nat || !n?.alive || n.dormant || atWar(g, x, nat) || foes.some((e) => allied(g, x, e))) continue;
    const cap = g.regions[n.capital];
    const far = home && cap ? Math.hypot(cap.x - home.x, cap.y - home.y) : 1e9;
    const k = (allied(g, x, nat) ? 0 : 1e12) + far;
    if (k < rank) { rank = k; best = x; }
  }
  return best;
}

/** Who sells a smaller power this kind of wing this month: only a kind its own works do not build, and only where abroadRules holds. */
export const boughtFrom = (g: Game, nat: string, wing: Wing): string | null => (abroadRules(g, nat) && whyNoWorks(g, nat, wing) !== null ? sellerOf(g, nat) : null);

/** The months a wing bought abroad takes: the data's for its kind, after any wing of that kind already on order (218(d): one at a time). */
function abroadMonths(g: Game, nat: string, wing: Wing): number {
  const busy = Math.max(0, ...g.nations[nat].queue.filter((b) => b.what === 'air' && b.item === wing).map((b) => b.eta - g.turn));
  return busy + (ABROAD()?.months[wing] ?? ARSENAL.air[wing].months);
}

/** Why a smaller power that may buy abroad finds no seller, in words: every great power is at war with it or stands with its enemies. */
const noSeller = (g: Game, nat: string): string => `No great power will sell aircraft to ${theName(g.nations[nat])} now: each is at war with it or allied to its enemies.`;
/**
 * A group's name in the lines about a bought wing: 'the' before a name in English, which opens with an English
 * ordinal ('the 1st Fighter Group'); a name in its own language stays bare ('1. Hava Alayı', 'Regimiento Aéreo 1').
 * The UI director, 21 November (ui-director/words-bought-wings.md).
 */
export const theGroupName = (name: string): string => (/^\d+(?:st|nd|rd|th) /.test(name) ? `the ${name}` : name);

/**
 * The short tag on a province's offer card for a wing a smaller power's works do not build this month (ruling
 * 213(d)): "No aircraft industry", "Builds no bombers", or the month it opens, "Fighters from July 1942". Null when
 * the wing can be built.
 */
export function aircraftTag(g: Game, nat: string, wing: Wing): string | null {
  if (!budgeted(g) || greatPower(g, nat) || whyNoWorks(g, nat, wing) === null || boughtFrom(g, nat, wing) !== null) return null;
  const w = SMALL().works[nat];
  if (!w || (w.ftr === null && w.bmr === null)) return 'No aircraft industry';
  const at = worksOpensAt(g, nat, wing);
  if (at === null) return `Builds no ${WING_WORD[wing]}`;
  return `${cap(WING_WORD[wing])} from ${turnToDate(at).label}`;
}

/**
 * Said once (ruling 213(d)): where a smaller power can order neither kind this month, the reasons in place of the
 * Arsenal page's opening lines, each said once. Null where it can order one kind or both.
 */
export function worksOpening(g: Game, nat: string): string | null {
  if (!budgeted(g) || greatPower(g, nat)) return null;
  const why = (['ftr', 'bmr'] as Wing[]).map((w) => (boughtFrom(g, nat, w) !== null ? null : whyNoWorks(g, nat, w)));
  if (why.some((x) => x === null)) return null;
  const w = SMALL().works[nat];
  if (!w || (w.ftr === null && w.bmr === null)) return why[0];
  const name = g.nations[nat].name;
  const at = (['ftr', 'bmr'] as Wing[]).map((k) => ({ k, at: worksOpensAt(g, nat, k) }));
  const dated = at.filter((x) => x.at !== null).sort((a, b) => a.at! - b.at!);
  // Both still to open: one sentence, the earlier month first.
  if (dated.length === 2) return `${name} builds no ${WING_WORD[dated[0].k]} before ${turnToDate(dated[0].at!).label} and no ${WING_WORD[dated[1].k]} before ${turnToDate(dated[1].at!).label}: until then more aircraft come only from abroad.`;
  // One to open and one never: the two sentences as they stand, the one with a month first.
  const first = dated[0]?.k ?? 'ftr', other: Wing = first === 'ftr' ? 'bmr' : 'ftr';
  return `${whyNoWorks(g, nat, first)} ${whyNoWorks(g, nat, other)}`;
}

/**
 * How many months a smaller power's wing takes (ruling 218(d)): its works build at their rate, one wing of a kind at a
 * time, so a second order waits for the first.
 */
function worksMonths(g: Game, nat: string, wing: Wing): number {
  const spec = ARSENAL.air[wing];
  const per = worksRate(g, nat, wing);
  if (per <= 0) return spec.months;
  const busy = Math.max(0, ...g.nations[nat].queue.filter((b) => b.what === 'air' && b.item === wing).map((b) => b.eta - g.turn));
  return Math.max(spec.months, busy + Math.ceil(spec.count / per));
}

/** Ruling 213(e): the factories a smaller power began the war with, wherever they stand now. */
export function firstFactories(g: Game, nat: string): number {
  let n = 0;
  for (const r of g.regions) if ((r.first ?? r.owner) === nat) n += Math.max(0, r.fac - factoriesBuilt(g, r.id));
  return n;
}

/** Ruling 213(e): the most factories a smaller power's industry can carry in this war: half again its first-day count, rounded up. Null for a great power, and outside the grand campaign. */
export const factoryCeiling = (g: Game, nat: string): number | null => (!budgeted(g) || greatPower(g, nat) ? null : Math.ceil(firstFactories(g, nat) * 1.5));

/** The factories a country has now on its own ground, and those building. */
export function factoriesHeld(g: Game, nat: string): { held: number; building: number } {
  let held = 0;
  for (const r of g.regions) if (isHome(g, nat, r)) held += r.fac;
  // Factories it built in conquered towns it still holds count against its industry too (release 2, settled towns).
  // (Read whatever the switch says now: what was built stays built. The flag is written only with the switch on.)
  for (const r of g.regions) if (r.ctrl === nat && r.owner !== nat) held += g.flags[`wk:facon:${r.id}:${nat}`] ?? 0;
  const building = g.nations[nat].queue.filter((b) => b.what === 'work' && b.item === 'factory').length;
  return { held, building };
}

/** Why a smaller power may order no new factory, in the words passed for ruling 213(e), or null. A factory lost may be built back. */
export function whyNoFactory(g: Game, nat: string): string | null {
  const most = factoryCeiling(g, nat);
  if (most === null) return null;
  const name = g.nations[nat].name;
  if (most <= 0) return `${name} has no industry to build on.`;
  const { held, building } = factoriesHeld(g, nat);
  if (held + building < most) return null;
  return `${name}'s industry can carry ${plural(most, 'factory', 'factories')} in this war, and it has ${held + building}${building ? ', built or building' : ''}.`;
}

/** The short tag on a province's Industry slot when a smaller power may build no more (ruling 213(e)): "3 of 3 factories", or "No industry". */
export function factoryTag(g: Game, nat: string): string | null {
  if (whyNoFactory(g, nat) === null) return null;
  const most = factoryCeiling(g, nat)!;
  if (most <= 0) return 'No industry';
  const { held, building } = factoriesHeld(g, nat);
  return `${held + building} of ${plural(most, 'factory', 'factories')}`;
}

/** The best airfield a country holds: the largest, its capital before others. */
function bestField(g: Game, nat: string): Region | null {
  let best: Region | null = null, rank = -1;
  for (const r of g.regions) {
    if (!canStation(g, nat, r)) continue;
    const k = workLevel(g, 'field', r.id) * 10 + (g.flags[`af:${r.id}`] ?? 0) * 10 + (g.nations[nat].capital === r.id ? 5 : 0) + r.fac;
    if (k > rank) { best = r; rank = k; }
  }
  return best;
}

/**
 * A grand campaign begun later in the war (ruling 213(d)): the record's aircraft from abroad of the months before it
 * are already in the order of battle history left, so their rows are marked as come.
 */
export function openSmallAir(g: Game): void {
  g.flags.gifts = 1;
  SMALL().gifts.forEach(([month], i) => { if (month < g.turn) g.flags[`gift:${i}`] = 1; });
}

/**
 * The month's aircraft from abroad (ruling 213(d)): each of the record's rows that falls due arrives, while one of its
 * givers stands and is not at war with the receiver, as a new air group on the receiver's best airfield (two for a
 * mixed row, half each). A row whose givers are gone, or whose receiver has fallen, is lost; one with no airfield to land on waits for one. And the
 * month a smaller power's works open, its own player reads one line of news. The grand campaign only.
 */
export function smallAirMonth(g: Game): void {
  if (!budgeted(g) || !airWar(g)) return;
  const S = SMALL();
  // The far side (ruling 234's rider b518, 7): a smaller power whose own works keep up the commands the record gives
  // it. What its works build this month, kind by kind from the month each opens, goes to its pool, with the men to fly
  // the fighters; its commands are made up from the pool as a great power's are. One count at every seat (rider b522):
  // while a wing of a kind is on order the works build that wing, and nothing of that kind goes to the pool besides.
  // And nothing built is lost (rider b539, 1): what the works make in those months is counted, and what they made
  // beyond the wing goes to the pool the month the wing is handed over (worksOver). An order taken back leaves what
  // was made of it for the pool.
  for (const nat of AIR.works ?? []) {
    const n = g.nations[nat];
    if (!n?.alive || n.dormant) continue;
    const p = airPool(g, nat);
    for (const wing of ['ftr', 'bmr'] as Wing[]) {
      const from = worksFrom(nat, wing);
      if (from === null || from > g.turn) continue;
      const k = Math.round(worksRate(g, nat, wing)), key = `built:${nat}:${wing}`;
      if (n.queue.some((q) => q.what === 'air' && q.item === wing)) { g.flags[key] = (g.flags[key] ?? 0) + k; continue; }
      const all = k + (g.flags[key] ?? 0);
      delete g.flags[key];
      p[wing] += all;
      if (wing === 'ftr') p.pilots += all;
    }
  }
  // (b518, 9, one count: a receiver whose additions arrive with the date on the record's own rows takes no row from abroad besides.)
  const dated = new Set(AIR.fleets.filter((r) => r.arrives).map((r) => r.nat));
  S.gifts.forEach(([month, to, points, kind, givers], i) => {
    const key = `gift:${i}`;
    if (month > g.turn || g.flags[key]) return;
    if (dated.has(to)) { g.flags[key] = 1; return; }
    const n = g.nations[to];
    // A receiver not on this map waits for it; one that has fallen loses the row.
    if (!n || n.dormant) return;
    if (!n.alive) { g.flags[key] = 1; return; }
    const giver = givers.find((x) => x === to || (g.nations[x]?.alive && !g.nations[x].dormant && !atWar(g, x, to)));
    if (!giver) { g.flags[key] = 1; return; }
    const r = bestField(g, to);
    if (!r) return;
    g.flags[key] = 1;
    const count = Math.round(points * S.perPoint);
    const parts: [Wing, number][] = kind === 'mixed' ? [['ftr', Math.ceil(count / 2)], ['bmr', Math.floor(count / 2)]] : [[kind, count]];
    const made: string[] = [];
    for (const [wing, k] of parts) {
      if (k <= 0) continue;
      const f = foundAirFleet(g, to, r, `${r.name} Air Command`, wing, k);
      f[wing] += k; f.est[wing] += k;
      (f.extra ??= { ftr: 0, bmr: 0 })[wing] += k;
      made.push(`${k} ${WING_WORD[wing]} (the ${f.name})`);
    }
    if (to === g.human && made.length) {
      const from = giver === to ? 'from its own works' : `from ${g.nations[giver].name}`;
      headline(g, 'info', `${cap(made.join(' and '))} arrive ${from} and form on the airfield at ${r.name}.`, to, r.id, { ready: 'air' });
    }
  });
  // The month a works opens: one plain line for its own player, never for a works that stands on the first day.
  const me = g.human;
  if (me && !greatPower(g, me) && g.nations[me]?.alive) {
    const open = (['ftr', 'bmr'] as Wing[]).filter((w) => worksFrom(me, w) === g.turn && g.turn > 1);
    if (open.length) {
      const name = g.nations[me].name;
      headline(g, 'info', open.length === 2
        ? `${name} now builds fighters and bombers at home: both wings can be ordered on the Arsenal page.`
        : `${name} now builds ${WING_WORD[open[0]]} at home: ${open[0] === 'ftr' ? 'Fighter' : 'Bomber'} wings can be ordered on the Arsenal page.`, me);
    }
  }
}

/**
 * The wing's remainder (ruling 234's rider b539, 1): a smaller power whose works feed its pool builds a wing in whole
 * months, so by the month the wing is handed over its works have made a little more than the wing. That remainder goes
 * to the pool, as the staff's output would, with the men to fly the fighters: a player who orders is never worse off
 * than the staff, and nothing built is lost.
 */
function worksOver(g: Game, nat: string, wing: Wing, count: number): void {
  if (!AIR.works?.includes(nat)) return;
  const key = `built:${nat}:${wing}`, over = Math.max(0, (g.flags[key] ?? 0) - count);
  delete g.flags[key];
  if (over <= 0) return;
  const p = airPool(g, nat);
  p[wing] += over;
  if (wing === 'ftr') p.pilots += over;
}

/** True if a new wing can be stationed in this region: the nation holds it and it has airfields, built or already in use by one of its fleets. */
export function canStation(g: Game, nat: string, r: Region | undefined): boolean {
  if (!r || r.ctrl !== nat || r.since >= g.turn) return false;
  // The air war: an airfield is a place, and aircraft are stationed only where there is one.
  if (airWar(g)) return hasAirfield(g, r.id);
  return workLevel(g, 'field', r.id) > 0 || g.airFleets.some((f) => f.nat === nat && f.base === r.id && f.ship === undefined);
}

const strongest = (fs: AirFleet[]): AirFleet | null => fs.reduce<AirFleet | null>((best, f) => (!best || f.ftr + f.bmr > best.ftr + best.bmr ? f : best), null);

/** Where a new wing would be stationed: the region asked for if it has airfields, else with the nation's strongest air fleet, else on any airfield it has built. */
export function stationSite(g: Game, nat: string, want = -1): number {
  if (canStation(g, nat, g.regions[want])) return want;
  const main = strongest(airFleetsOf(g, nat).filter((f) => f.ship === undefined && canStation(g, nat, g.regions[f.base])));
  if (main) return airWar(g) ? roomNear(g, nat, main.base) : main.base;
  return g.regions.find((r) => canStation(g, nat, r))?.id ?? -1;
}

/**
 * The air war: where new aircraft go when nobody names the place. The main air base while its fields have room for a
 * wing more, counting what is based there and what is on order for it; else the nation's field nearest it that has
 * that room; and the main base still if none has. (The third playtester, 84.3.1: every new Soviet group formed at one
 * field, Ryazan holding 3,399 aircraft where it serves 1,500, so that most of them could not fly.)
 */
function roomNear(g: Game, nat: string, base: number): number {
  const wing = Math.max(ARSENAL.air.ftr.count, ARSENAL.air.bmr.count);
  const free = (id: number): number => airfieldsHold(g, id)
    - g.airFleets.reduce((s, f) => s + (f.base === id && f.ship === undefined ? f.ftr + f.bmr : 0), 0)
    - g.nations[nat].queue.reduce((s, b) => s + (b.what === 'air' && b.region === id ? b.count ?? ARSENAL.air[b.item as Wing].count : 0), 0);
  if (free(base) >= wing) return base;
  const B = g.regions[base];
  let best = base, near = Infinity;
  for (const r of g.regions) {
    if (r.id === base || !canStation(g, nat, r)) continue;
    const d = chartDist(g, r, B);
    if (d < near && free(r.id) >= wing) { near = d; best = r.id; }
  }
  return best;
}

/**
 * The air fleet a wing stationed here would join, if one of the nation's is based here. In the air war a wing joins
 * an air group of its own kind that has room for it, and otherwise founds a new group.
 */
const fleetAt = (g: Game, nat: string, region: number, wing?: Wing): AirFleet | null => strongest(airFleetsOf(g, nat).filter((f) => f.base === region && f.ship === undefined && f.ashore === undefined
  && (!airWar(g) || !wing || (f.kind === wing && f.est[wing] + ARSENAL.air[wing].count <= AIRWAR.groups.most))));

/** True if a ship needing a yard of this level can be laid down here. */
export const canLay = (g: Game, nat: string, r: Region | undefined, yard: number): boolean => !!r && (isHome(g, nat, r) || settledFor(g, nat, r)) && r.port && yardLevel(g, r) >= yard;

/** Where a ship would be laid down: the port asked for if its yard will do, else the nation's best yard. -1 if it has none big enough. */
export function slipSite(g: Game, nat: string, yard: number, want = -1): number {
  if (canLay(g, nat, g.regions[want], yard)) return want;
  let best = -1;
  for (const r of g.regions) {
    if (!canLay(g, nat, r, yard)) continue;
    if (best < 0 || yardLevel(g, r) > yardLevel(g, g.regions[best]) || (yardLevel(g, r) === yardLevel(g, g.regions[best]) && r.fac > g.regions[best].fac)) best = r.id;
  }
  return best;
}

/** The fleet new ships of this class would join: the one based where they are built, else the nation's main fleet of that kind, else its largest. */
export function joinsFleet(g: Game, nat: string, cls: ShipClass, port: number): Fleet | null {
  const all = fleetsOf(g, nat);
  const most = (fs: Fleet[]): Fleet | null => fs.reduce<Fleet | null>((best, f) => (!best || f.ships[cls] > best.ships[cls] || (f.ships[cls] === best.ships[cls] && shipCount(f.ships) > shipCount(best.ships)) ? f : best), null);
  const here = all.filter((f) => f.base === port);
  // Submarines go to the submarine arm wherever it is based: a battle fleet has no use for them.
  if (cls === 'ss') { const arm = most(all.filter((f) => f.ships.ss > 0)); if (arm) return arm; }
  return most(here) ?? most(all.filter((f) => f.ships[cls] > 0)) ?? most(all);
}

/**
 * (Release 2: the city at a glance, display only.) Where each wing a nation has on order would go, placed in the order
 * the months hand them over (by month, then as ordered) and each counted in before the next is placed: deliverArsenal's
 * own choice, made on a copy. A group on the wing's field with room for it; else a new group founded there; else the
 * strongest group of its kind elsewhere; else nowhere (the reserve, null). Keyed by the order's id. `found` is a new
 * group on that field, `first` the wing that founds it (a later one joins it).
 */
export function airJoinsPlan(g: Game, nat: string): Map<number, { name: string; base: number; found: boolean; first: boolean; nth: number } | null> {
  const out = new Map<number, { name: string; base: number; found: boolean; first: boolean; nth: number } | null>();
  const n = g.nations[nat];
  if (!n) return out;
  const war = airWar(g);
  // (nth: for a group yet to be founded, which new group it is on its field, 1 for the first: two new groups on one
  // field are told apart in words, "forms a second new air group here")
  type Slot = { id: number; name: string; base: number; kind?: Wing; ftr: number; bmr: number; est: { ftr: number; bmr: number }; afloat: boolean; nth: number };
  const slots: Slot[] = airFleetsOf(g, nat).map((f) => ({
    id: f.id, name: f.name, base: f.base, kind: f.kind, ftr: f.ftr, bmr: f.bmr,
    est: { ftr: f.est?.ftr ?? 0, bmr: f.est?.bmr ?? 0 }, afloat: f.ship !== undefined || f.ashore !== undefined, nth: 0,
  }));
  const best = (fs: Slot[]): Slot | null => fs.reduce<Slot | null>((m, f) => (!m || f.ftr + f.bmr > m.ftr + m.bmr ? f : m), null);
  const wings = n.queue.map((b, i) => ({ b, i })).filter((x) => x.b.what === 'air').sort((x, y) => x.b.eta - y.b.eta || x.i - y.i);
  for (const { b } of wings) {
    const r = g.regions[b.region];
    if (!r) { out.set(b.id, null); continue; }
    const wing = (b.item ?? 'ftr') as Wing, count = b.count ?? ARSENAL.air[wing].count;
    let f = canStation(g, nat, r) || r.ctrl === nat
      ? best(slots.filter((x) => x.base === r.id && !x.afloat && (!war || (x.kind === wing && x.est[wing] + ARSENAL.air[wing].count <= AIRWAR.groups.most))))
      : null;
    let first = false;
    if (!f && r.ctrl === nat && (!war || hasAirfield(g, r.id))) {
      const nth = slots.filter((x) => x.id < 0 && x.base === r.id).length + 1;
      f = { id: -1 - slots.length, name: '', base: r.id, kind: war ? wing : undefined, ftr: 0, bmr: 0, est: { ftr: 0, bmr: 0 }, afloat: false, nth };
      slots.push(f);
      first = true;
    } else if (!f) {
      f = best(slots.filter((x) => !x.afloat && (!war || x.kind === wing)));
    }
    if (!f) { out.set(b.id, null); continue; }
    f[wing] += count;
    f.est[wing] += count;
    out.set(b.id, { name: f.name, base: f.base, found: f.id < 0, first, nth: f.nth });
  }
  return out;
}

const liftBought = (g: Game, nat: string): number => (g.flags[`wk:tp:${nat}`] ?? 0) + g.nations[nat].queue.filter((b) => b.what === 'ship' && b.item === 'tp').length;

/**
 * A nation's shipping for armies: how many corps it carries across the sea in a month, how many more this month,
 * the troopships on the slips (the month each lot is ready) and how many more lots its ports could still take.
 */
export interface LiftView { lift: number; left: number; building: number[]; room: number }

export function liftView(g: Game, nat: string): LiftView {
  const n = g.nations[nat];
  const building = n.queue.filter((b) => b.what === 'ship' && b.item === 'tp').map((b) => b.eta).sort((a, b) => a - b);
  return { lift: n.sealift, left: liftLeft(g, nat), building, room: Math.max(0, ARSENAL.lift.most - liftBought(g, nat)) };
}

// ---- The offers -------------------------------------------------------------------------------------

export interface Offer {
  /** What it is: 'corps:inf', 'air:ftr', 'ship:dd', 'work:aa', 'work:lights' (city defences), 'work:fort', 'elite' (the special formation), 'para' (an airborne corps), 'cadre:<corps id>' (a folded corps raised again). Pass it to orderOffer. */
  key: string;
  /** For a cadre: the kind of corps it is, for its picture. */
  kind?: Kind;
  group: 'army' | 'air' | 'navy' | 'works';
  label: string;
  blurb: string;
  /** What is delivered: '3 divisions', '100 fighters', '4 destroyers', 'level 2'. */
  size: string;
  cost: number;
  months: number;
  /** What it goes on costing each month once delivered, with the nation at war. */
  upkeep: number;
  /** The region where it would be raised, stationed, laid down or built, or -1 when there is nowhere. */
  at: number;
  /** In words: where it goes and what it joins, or what it needs. */
  note: string;
  /** Why it cannot be ordered now, or null when it can. */
  why: string | null;
  /** True when money is the only thing in the way. */
  poor: boolean;
  /** The id of the technology the nation must research before it can place this order (the grand campaign), or absent. */
  tech?: string;
  /** The men it takes from the reserve (the grand campaign): set for a new corps. */
  men?: number;
  /** A wing bought abroad: the great power that sells it. */
  from?: string;
}

const POOR = 'The treasury cannot cover it.';

/**
 * Everything this nation could order, with the province 'sel' in mind (the one picked on the map, or -1). An offer
 * that cannot be ordered now says why. Costs are in production points: show them times BUDGET.scale.
 */
export function offers(g: Game, nat: string, sel = -1): Offer[] {
  return listOffers(g, nat, sel).map((o) => ({ ...o, poor: o.why === POOR }));
}

function listOffers(g: Game, nat: string, sel: number): Omit<Offer, 'poor'>[] {
  const n = g.nations[nat];
  const out: Omit<Offer, 'poor'>[] = [];
  const poor = (cost: number): string | null => (n.pp < cost ? POOR : null);
  // Every new corps is filled from the reserve of men (the grand campaign).
  const noMen = (divs: number): string | null => (raisable(g, nat, divs) < 1 ? `The reserve has not the men: this takes ${menWords(divs * MEN_PER_DIV)} and ${menWords(menLeft(g, nat) * MEN_PER_DIV)} are left. ${menWords(classNow(g, nat, govMul(g, nat, 'men')) * MEN_PER_DIV)} come of age each month.` : null);
  const menFor = (divs: number): { men?: number } => (menRules(g) ? { men: divs * MEN_PER_DIV } : {});
  const r = g.regions[sel] as Region | undefined;
  // What the technology tree still holds back (the grand campaign): the order is listed, and says what it waits on.
  const gate = (key: string): { why: string; tech: string } | null => { const t = locked(g, nat, key); return t ? { why: `Needs the technology ${t.name}. Open Research to begin it.`, tech: t.id } : null; };

  // The nation's own special formation (the grand campaign): raised like a corps of its kind, a few at a time. It heads the list.
  const u = budgeted(g) ? eliteOf(nat) : null;
  if (u) {
    const need = needsOf(g, u.kind);
    const at = raiseSite(g, nat, u.kind, sel);
    const no = gate(`corps:${u.kind}`)?.why ?? whyNoElite(g, nat);
    out.push({
      key: 'elite', group: 'army', label: u.label, blurb: `${u.text} ${eliteRule(u)}`, size: plural(u.divs, 'division'), cost: u.cost, months: u.months,
      upkeep: corpsUpkeep(u.kind, u.divs), at,
      note: at < 0 ? '' : `Forms up at ${g.regions[at].name}. ${eliteCount(g, nat)} of ${u.most} in the field or on order.`,
      why: no ?? (at < 0 ? (need ? `Needs a home province with ${plural(need, 'factory', 'factories')}. Build factories in a city first.` : 'There is no home ground left to raise troops on.') : noMen(u.divs) ?? poor(u.cost)),
      ...menFor(u.divs),
    });
  }
  // Cadres (the grand campaign): corps folded into others, each waiting to be raised again for a share of the price.
  for (const c of cadresOf(g, nat)) {
    if (n.queue.some((b) => b.cadre === c.id)) continue;
    const t = cadreTerms(g, nat, c);
    const at = raiseSite(g, nat, c.kind, sel);
    out.push({
      key: `cadre:${c.id}`, group: 'army', kind: c.kind, label: `${c.name}, a cadre`,
      blurb: `The cadre of a corps whose men were folded into another: its staff, its name, its record and its honours. Raised again it is the same corps filled with new men, and it keeps ${Math.round(MANPOWER.fold.xp * 100)}% of what it knew.`,
      size: plural(c.divs, 'division'), cost: t.cost, months: t.months, upkeep: budgeted(g) ? corpsUpkeep(c.kind, c.divs) : 0, at,
      note: at < 0 ? '' : `Forms up at ${g.regions[at].name}, for ${Math.round(MANPOWER.fold.price * 100)}% of the price of a new corps.`,
      why: whyNoCadre(g, nat, c) ?? (at < 0 ? 'There is no home ground left to raise troops on.' : noMen(t.divs) ?? poor(t.cost)),
      ...menFor(t.divs),
    });
  }
  for (const kind of corpsKinds(g)) {
    const spec = specOf(g, kind)!;
    const need = needsOf(g, kind);
    const at = raiseSite(g, nat, kind, sel);
    const where = at >= 0 ? g.regions[at].name : '';
    const cost = corpsCost(g, nat, kind);
    const held = gate(`corps:${kind}`);
    out.push({
      key: `corps:${kind}`, group: 'army', label: spec.label, blurb: spec.blurb, size: plural(spec.divs, 'division'), cost, months: spec.months,
      upkeep: budgeted(g) ? corpsUpkeep(kind, spec.divs) : 0, at,
      note: at < 0 ? '' : `Forms up at ${where}${need && at !== sel && r && canRaise(g, nat, r) ? `: ${r.name} has not the ${plural(need, 'factory', 'factories')} this needs` : ''}.`,
      why: held?.why ?? (at < 0 ? (need ? `Needs a home province with ${plural(need, 'factory', 'factories')}. Build factories in a city first.` : 'There is no home ground left to raise troops on.') : noMen(spec.divs) ?? poor(cost)),
      ...(held ? { tech: held.tech } : {}),
      ...menFor(spec.divs),
    });
  }
  // Airborne troops, once the tree has given the nation its airborne forces (the grand campaign).
  if (budgeted(g)) {
    const p = TECH.units.para;
    const at = raiseSite(g, nat, 'inf', sel);
    const held = gate('para');
    const cost = priceOf(g, nat, 'para', p.cost);
    out.push({
      key: 'para', group: 'army', label: p.label, blurb: p.blurb, size: plural(p.divs, 'division'), cost, months: p.months,
      upkeep: corpsUpkeep('inf', p.divs), at,
      note: at < 0 ? '' : `Forms up at ${g.regions[at].name}. ${paraCount(g, nat)} of ${p.most} in the field or on order.`,
      why: held?.why ?? whyNoPara(g, nat) ?? (at < 0 ? 'There is no home ground left to raise troops on.' : noMen(p.divs) ?? poor(cost)),
      ...(held ? { tech: held.tech } : {}),
      ...menFor(p.divs),
    });
  }
  if (!budgeted(g)) {
    const cost = r ? fortCost(g, nat, r) : null;
    if (r && cost !== null) out.push(fortOffer(g, nat, r, cost, poor(cost)));
    return out;
  }

  for (const wing of ['ftr', 'bmr'] as Wing[]) {
    const spec = ARSENAL.air[wing];
    // Ruling 213(d): a smaller power builds aircraft where the record had a works, from the month it opened, at its rate.
    const small = budgeted(g) && !greatPower(g, nat);
    const none = small ? whyNoWorks(g, nat, wing) : null;
    // (Brian, 21 November 2026) A wing its own works do not build this month may be bought abroad, on the player's own order.
    const abroad = small && none !== null && abroadRules(g, nat);
    const seller = abroad ? sellerOf(g, nat) : null;
    const built = small ? none === null || seller !== null : industry(g, nat, spec.factories);
    const at = built ? stationSite(g, nat, sel) : -1;
    const joins = at >= 0 ? fleetAt(g, nat, at, wing) : null;
    const home = priceOf(g, nat, `air:${wing}`, spec.cost);
    const cost = seller ? Math.round(home * ABROAD()!.price) : home;
    const where = at < 0 ? '' : joins ? `Joins ${joins.name} at ${g.regions[at].name}.` : airWar(g) ? `Forms a new air group on the airfield at ${g.regions[at].name}.` : `Founds a new air fleet on the airfields of ${g.regions[at].name}.`;
    out.push({
      key: `air:${wing}`, group: 'air', label: spec.label, blurb: spec.blurb, size: `${spec.count} ${wing === 'ftr' ? 'fighters' : 'bombers'}`, cost, months: seller ? abroadMonths(g, nat, wing) : small && built ? worksMonths(g, nat, wing) : spec.months,
      upkeep: spec.count * BUDGET.upkeep.air[wing], at,
      note: seller && at >= 0 ? `Bought from ${theName(g.nations[seller])}. ${joins ? `Joins ${theGroupName(joins.name)} at ${g.regions[at].name}.` : where}` : where,
      why: seller ? (at < 0 ? 'Needs airfields to fly from. Build airfields in a province first.' : poor(cost))
        : abroad ? noSeller(g, nat)
        : none ?? (!built ? `Needs an aircraft industry: a home province with ${spec.factories} factories.` : at < 0 ? 'Needs airfields to fly from. Build airfields in a province first.' : poor(cost)),
      ...(seller ? { from: seller } : {}),
    });
  }

  for (const hull of ['dd', 'ss', 'tp', 'ca', 'cv', 'bb'] as Hull[]) {
    const spec = ARSENAL.ships[hull];
    const at = slipSite(g, nat, spec.yard, sel);
    const fleet = at >= 0 && hull !== 'tp' ? joinsFleet(g, nat, hull, at) : null;
    const full = hull === 'tp' && liftBought(g, nat) >= ARSENAL.lift.most;
    const cost = priceOf(g, nat, `ship:${hull}`, spec.cost);
    const one = hull === 'tp' ? '' : NAVY.classes[hull].one, many = hull === 'tp' ? '' : NAVY.classes[hull].many;
    // (Brian, 18 November: the smaller nations lay down battleships and carriers only on a great power's plans, plans.ts.)
    const noPlans = whyNoPlans(g, nat, hull);
    const held = noPlans || onPlans(g, nat, hull) ? null : gate(`ship:${hull}`);
    out.push({
      ...(held ? { tech: held.tech } : {}),
      key: `ship:${hull}`, group: 'navy', label: spec.label, blurb: spec.blurb, size: hull === 'tp' ? 'lift for one more corps a month' : plural(spec.count, one, many), cost, months: spec.months,
      upkeep: hull === 'tp' ? 0 : spec.count * BUDGET.upkeep.ships[hull], at,
      note: at < 0 ? '' : hull === 'tp' ? `Built at ${g.regions[at].name}.` : fleet ? `Laid down at ${g.regions[at].name}. Joins the ${fleet.name}.` : `Laid down at ${g.regions[at].name}. Forms a new fleet there.`,
      why: noPlans ?? held?.why ?? (at < 0 ? `Needs a shipyard of level ${spec.yard} in one of your home ports.` : full ? 'The nation has all the troopships its ports can handle.' : poor(cost)),
    });
  }

  // Works are built in the province picked on the map.
  const W = ARSENAL.works;
  const pick = 'Pick one of your own provinces on the map.';
  {
    const have = r ? factoriesBuilt(g, r.id) + queued(g, nat, 'factory', r.id) : 0;
    const most = r && (r.terrain === 'urban' || r.cap) ? W.factory.most.city : W.factory.most.town;
    const cost = W.factory.cost + W.factory.step * have;
    // Ruling 213(e): a smaller power at the end of its industry: the country's fact comes before the province's reasons.
    const ceiling = whyNoFactory(g, nat);
    const why = ceiling ?? (!r ? pick : r.overseas ? 'Factories are built in the home country.' : whyNotWorksHere(g, nat, r, 'Factories are built in the home country.', !isCity(r) ? 'Factories are built in cities: a province that is urban, the capital, has industry already or is a place of some size.' : null) ?? (!isCity(r) ? 'Factories are built in cities: a province that is urban, the capital, has industry already or is a place of some size.' : have >= most ? `${r.name} has room for no more new factories.` : poor(cost)));
    const carry = factoryCeiling(g, nat);
    const said = carry !== null && carry > 0 && !ceiling ? ` ${g.nations[nat].name} has ${factoriesHeld(g, nat).held + factoriesHeld(g, nat).building} of the ${plural(carry, 'factory', 'factories')} its industry can carry in this war.` : '';
    out.push({ key: 'work:factory', group: 'works', label: W.factory.label, blurb: W.factory.blurb, size: 'one factory', cost, months: W.factory.months, upkeep: 0, at: r ? r.id : -1, note: r ? `${r.name} has ${plural(r.fac, 'factory', 'factories')}${have ? `, ${have} of ${most} new ones built or building` : ''}.${said}` : '', why });
  }
  const FULL = { yard: 'The shipyard at % is as large as a yard can be.', aa: 'The guns at % are as heavy as they can be.', field: 'The airfields at % are as large as they can be.', lights: 'The searchlights at % are as many as they can be.' };
  const NONE = { yard: 'no shipyard', aa: 'no guns', field: 'only rough landing grounds', lights: 'no searchlights' };
  const level = (what: 'yard' | 'aa' | 'field' | 'lights', spec: LevelSpec, now: number, upkeep: number, why: string | null): void => {
    const next = now + (r ? queued(g, nat, what, r.id) : 0) + 1;
    const cost = priceOf(g, nat, `work:${what}`, spec.cost[Math.min(TOP, next)]);
    out.push({
      key: `work:${what}`, group: 'works', label: spec.label, blurb: spec.blurb, size: `level ${Math.min(TOP, next)}`, cost, months: spec.months, upkeep, at: r ? r.id : -1,
      note: r ? `${r.name} has ${now ? `level ${now}` : NONE[what]}${next - 1 > now ? `, level ${next - 1} building` : ''}.` : '',
      why: why ?? (next > TOP ? FULL[what].replace('%', r!.name) : poor(cost)),
    });
  };
  level('yard', W.yard, r ? yardLevel(g, r) : 0, 0, !r ? pick : whyNotWorksHere(g, nat, r, 'Shipyards are built in the home country.', !r.port ? `${r.name} has no harbour.` : null) ?? (!r.port ? `${r.name} has no harbour.` : null));
  level('aa', W.aa, r ? workLevel(g, 'aa', r.id) : 0, W.aa.upkeep, !r ? pick : r.ctrl !== nat ? 'That ground is not in your hands.' : null);
  // City defences (release 2; the design director's ruling of 25 Nov 2026 02:23Z): searchlights straight after the
  // guns, by the guns' own rule (any ground the nation holds). Offered only with the switch on.
  if (cityDefences(g)) level('lights', W.lights, r ? lightsAt(g, r.id) : 0, W.lights.upkeep, !r ? pick : r.ctrl !== nat ? 'That ground is not in your hands.' : null);
  if (!fieldRules(g)) level('field', W.field, r ? workLevel(g, 'field', r.id) : 0, W.field.upkeep, !r ? pick : r.ctrl !== nat ? 'That ground is not in your hands.' : null);
  else {
    // Airfields as targets: two offers on the one counter of built levels. A field strip in a month, for fighters, on
    // bare ground or a strip; a concrete level in release 1's months and price, which makes a strip a full airfield.
    const now = r ? workLevel(g, 'field', r.id) : 0;
    // (A strip is queued as a 'field' work with kind 'strip', so the one count covers both.)
    const next = now + (r ? queued(g, nat, 'field', r.id) : 0) + 1;
    const mine = !r ? pick : r.ctrl !== nat ? 'That ground is not in your hands.' : null;
    const tier = r ? fieldTier(g, r.id) : 'none';
    for (const what of ['strip', 'field'] as const) {
      const spec = W[what];
      const cost = priceOf(g, nat, `work:${what}`, spec.cost[Math.min(TOP, next)]);
      const why = mine ?? (what === 'strip' && tier === 'full' ? 'This is a concrete airfield: enlarge it with concrete.' : next > TOP ? FULL.field.replace('%', r!.name) : poor(cost));
      out.push({
        key: `work:${what}`, group: 'works', label: what === 'strip' ? spec.label : 'Concrete airfield', blurb: what === 'strip' ? spec.blurb : 'Concrete runways, hangars and control rooms: every air group can be based here, heavy bombers fly from it and jets keep their edge. Each level lets 500 more aircraft fly from the province; the first makes a field strip a full airfield.',
        size: `level ${Math.min(TOP, next)}`, cost, months: spec.months, upkeep: spec.upkeep, at: r ? r.id : -1,
        note: r ? `${r.name} has ${tier === 'none' ? 'no airfield' : `${tier === 'strip' ? 'a field strip' : 'a concrete airfield'}${now ? `, level ${now}` : ''}`}${next - 1 > now ? `, level ${next - 1} building` : ''}.` : '',
        why,
      });
    }
  }
  {
    const cost = r ? fortCost(g, nat, r) : null;
    if (r && cost !== null) out.push(fortOffer(g, nat, r, cost, poor(cost)));
    else out.push({ key: 'work:fort', group: 'works', label: 'Fortifications', blurb: FORT_BLURB, size: '', cost: priceOf(g, nat, 'work:fort', PRODUCTION.fort.cost[1]), months: PRODUCTION.fort.months, upkeep: 0, at: r ? r.id : -1, note: '', why: !r ? pick : whyNotWorksHere(g, nat, r, 'Fortifications are built on home ground.') ?? `${r.name} is fortified as heavily as it can be.` });
  }
  return out;
}

const FORT_BLURB = 'Defenders behind works fight far harder, and the works face every direction: barbed wire, gun pits and an anti-tank ditch, then pillboxes and dragon\'s teeth, then a line of concrete forts. A line only helps if the enemy has to come through it.';

function fortOffer(g: Game, nat: string, r: Region, cost: number, why: string | null): Omit<Offer, 'poor'> {
  const lv = (r.fort?.lv ?? 0) + g.nations[nat].queue.filter((b) => b.what === 'fort' && b.region === r.id).length + 1;
  return { key: 'work:fort', group: 'works', label: 'Fortifications', blurb: FORT_BLURB, size: `level ${lv}`, cost, months: PRODUCTION.fort.months, upkeep: 0, at: r.id, note: `Level ${lv} is ${worksName(lv)}. ${r.name} has ${r.fort ? `level ${r.fort.lv}` : 'none'}${lv - 1 > (r.fort?.lv ?? 0) ? `, level ${lv - 1} building` : ''}.`, why };
}

/** Places an order from the list of offers. The price is paid now; what was ordered arrives when its months are up. */
export function orderOffer(g: Game, nat: string, key: string, sel = -1): Build | null {
  const n = g.nations[nat];
  const o = offers(g, nat, sel).find((x) => x.key === key);
  if (!n.alive || !o || o.why !== null || o.at < 0 || n.pp < o.cost) return null;
  const [group, item] = key.split(':');
  if (group === 'elite') return orderElite(g, nat, o.at);
  if (group === 'para') return orderPara(g, nat, o.at);
  if (group === 'cadre') return orderCadre(g, nat, Number(item), o.at);
  if (group === 'corps') return orderCorps(g, nat, item as Kind, o.at);
  if (key === 'work:fort') return orderFort(g, nat, o.at);
  const r = g.regions[o.at];
  n.pp -= o.cost;
  const b: Build = { id: g.nextBuild++, what: group as 'air' | 'ship' | 'work', item, name: '', region: o.at, ordered: g.turn, eta: g.turn + o.months, cost: o.cost };
  if (group === 'air') { b.count = ARSENAL.air[item as Wing].count; b.name = o.from ? `${o.label} from ${theName(g.nations[o.from])} for ${r.name}` : `${o.label} for ${r.name}`; if (o.from) { b.from = o.from; const mk = item === 'ftr' && fighterTypesOn(g) ? sellerType(g, o.from) : null; if (mk) b.mark = mk; } }
  else if (group === 'ship') { b.count = ARSENAL.ships[item as Hull].count; b.name = `${o.label} at ${r.name}`; }
  else b.name = `${o.label}${item === 'factory' ? '' : `, ${o.size},`} at ${r.name}`;
  // Airfields as targets: a strip goes on the books as a level of 'field' marked 'strip' (one counter, one cap, and a
  // save an older build loads still delivers it as a level of airfield).
  if (key === 'work:strip') { b.item = 'field'; (b as { kind?: string }).kind = 'strip'; }
  // Ordered in a conquered town that has settled (release 2): it is delivered while the town is held.
  if (r && r.owner !== nat && settledFor(g, nat, r) && (group === 'ship' || item === 'factory' || item === 'yard')) b.conq = true;
  n.queue.push(b);
  return b;
}

// ---- Delivery ---------------------------------------------------------------------------------------

/** A wing of the player's from the factories: its own line, and what the month's one line says of it. */
export interface AirDelivery { text: string; region: number; wing: Wing; count: number; fleet: string; base: string; new: boolean; war: boolean }

/**
 * The month's aircraft from the factories in one line, wings joining the same group counted together: "The month's
 * aircraft from the factories: 200 fighters join 12th Fighter Aviation Division at Moscow; a new air group, the 13th
 * Fighter Aviation Regiment, forms on the airfield at Moscow with 100 fighters." One wing keeps its own line.
 */
export function airLine(list: AirDelivery[]): { text: string; region: number } | null {
  if (!list.length) return null;
  if (list.length === 1) return { text: list[0].text, region: list[0].region };
  const parts: AirDelivery[] = [];
  for (const d of list) {
    const same = !d.new && parts.find((p) => !p.new && p.fleet === d.fleet && p.wing === d.wing && p.base === d.base);
    if (same) same.count += d.count; else parts.push({ ...d });
  }
  const word = (d: AirDelivery): string => `${d.count} ${d.wing === 'ftr' ? 'fighters' : 'bombers'}`;
  const said = parts.map((d) => (d.new ? (d.war ? `a new air group, the ${d.fleet}, forms on the airfield at ${d.base} with ${word(d)}` : `a new air fleet, ${d.fleet}, forms on the airfields of ${d.base} with ${word(d)}`) : `${word(d)} join ${d.fleet} at ${d.base}`));
  return { text: `The month's aircraft from the factories: ${said.join('; ')}.`, region: list[0].region };
}

/**
 * Hands over an order from the arsenal that is due: aircraft to their fleet, ships to theirs, works to the province.
 * `air`, where given, takes the player's lines for aircraft instead of the news, so that a month's deliveries can be told
 * in one line (the third playtester, 84.3.1: about 1,800 such lines in 46 Soviet wars). See airLine.
 */
export function deliverArsenal(g: Game, nat: string, b: Build, air?: AirDelivery[]): void {
  const n = g.nations[nat];
  const r = g.regions[b.region];
  // (release 2: a unit handed over is tagged as ready, so that an interface can say so plainly; Brian, 24 Nov 18:30Z)
  const tell = (text: string, ready?: 'air' | 'ship'): void => { if (nat === g.human) headline(g, 'info', text, nat, r.id, ready ? { ready } : undefined); };
  if (b.what === 'air') {
    // (A wing bought abroad keeps its own line: the month's one line speaks of the factories.)
    const seller = b.from && g.nations[b.from] ? theName(g.nations[b.from]) : '';
    const tellAir = (d: AirDelivery): void => { if (nat !== g.human) return; if (air && !seller) air.push(d); else headline(g, 'info', d.text, nat, d.region, { ready: 'air' }); };
    const wing = b.item as Wing, count = b.count ?? ARSENAL.air[wing].count;
    const word = `${count} ${wing === 'ftr' ? 'fighters' : 'bombers'}`;
    worksOver(g, nat, wing, count);
    // The wing goes where it was ordered for; if that ground is lost, to the strongest fleet the nation has left.
    const war = airWar(g);
    let f = canStation(g, nat, r) || r.ctrl === nat ? fleetAt(g, nat, r.id, wing) : null;
    if (!f && r.ctrl === nat && (!war || hasAirfield(g, r.id))) {
      f = foundAirFleet(g, nat, r, `${r.name} Air Command`, war ? wing : undefined, count);
      tellAir({ text: seller ? `${cap(word)} (${theGroupName(f.name)}) arrive from ${seller} and form on the airfield at ${r.name}.` : war ? `A new air group, the ${f.name}, forms on the airfield at ${r.name} with ${word}.` : `A new air fleet, ${f.name}, forms on the airfields of ${r.name} with ${word}.`, region: r.id, wing, count, fleet: f.name, base: r.name, new: true, war });
    } else {
      // The air war: the wing goes to the strongest group of its own kind.
      f ??= strongest(airFleetsOf(g, nat).filter((x) => x.ship === undefined && x.ashore === undefined && (!war || x.kind === wing)));
      if (f) tellAir({ text: seller ? `${cap(word)} from ${seller} join ${theGroupName(f.name)} at ${g.regions[f.base].name}.` : `${cap(word)} from the factories join ${f.name} at ${g.regions[f.base].name}.`, region: f.base, wing, count, fleet: f.name, base: g.regions[f.base].name, new: false, war });
    }
    if (!f) {
      // No air fleet and nowhere to found one: the machines wait in reserve.
      const p = airPool(g, nat);
      if (wing === 'ftr') { p.ftr += count; poolIn(g, nat, count); } else p.bmr += count;
      return;
    }
    // A wing bought abroad keeps the seller's fighter type (the switch 'Fighter types'; the design director, 23 Nov 07:00Z).
    if (wing === 'ftr' && b.from && fighterTypesOn(g)) { const mk = b.mark ?? sellerType(g, b.from); if (mk) joinBought(f, mk, f.ftr, count); }
    // (Under Units decide a bought wing's pilots are the buyer's, from its own schools, with no experience: BO:2067.)
    joinPilots(g, f, count, wing, 'school');
    f[wing] += count;
    f.est[wing] += count;
    (f.extra ??= { ftr: 0, bmr: 0 })[wing] += count;
    return;
  }
  if (b.what === 'ship') {
    const hull = b.item as Hull, count = b.count ?? ARSENAL.ships[hull].count;
    if (hull === 'tp') {
      n.sealift += count;
      g.flags[`wk:tp:${nat}`] = (g.flags[`wk:tp:${nat}`] ?? 0) + 1;
      tell(`New troopships are ready: the nation can now move ${plural(n.sealift, 'corps', 'corps')} by sea each month.`, 'ship');
      return;
    }
    let f = joinsFleet(g, nat, hull, r.id);
    if (!f && (isHome(g, nat, r) || ((settledTownsOn(g) || b.conq) && r.ctrl === nat)) && r.port && r.seas.length) f = foundFleet(g, nat, r, `${r.name} Fleet`);
    if (!f) { tell(`With no port left to take them, the ${NAVY.classes[hull].many} building at ${r.name} are lost.`); return; }
    // New ships join at the back of their class, with no battle stars (stars.ts).
    mendStars(f, hull);
    f.ships[hull] += count;
    // A new carrier joins with its aircraft aboard (the grand campaign).
    const decks = hull === 'cv' && carrierRules(g);
    if (decks) syncWings(g);
    tell(`${cap(plural(count, `new ${NAVY.classes[hull].one}`, `new ${NAVY.classes[hull].many}`))} ${count === 1 ? 'joins' : 'join'} the ${f.name}${decks ? ', with fighters and strike aircraft aboard' : ''}.`, 'ship');
    return;
  }
  const what = b.item as Work;
  if (what === 'factory') {
    // The city was lost before the factory was finished. (A settled conquered town keeps one, release 2.)
    const conquered = (settledTownsOn(g) || !!b.conq) && r.ctrl === nat && r.owner !== nat;
    if (!isHome(g, nat, r) && !conquered) return;
    r.fac += 1;
    g.flags[`wk:fac:${r.id}`] = factoriesBuilt(g, r.id) + 1;
    if (conquered) g.flags[`wk:facon:${r.id}:${nat}`] = (g.flags[`wk:facon:${r.id}:${nat}`] ?? 0) + 1;
    tell(`The new factory at ${r.name} is at work: the province now has ${r.fac}.`);
    return;
  }
  if (what === 'yard') {
    if ((!isHome(g, nat, r) && !((settledTownsOn(g) || b.conq) && r.ctrl === nat)) || !r.port) return;
    g.flags[`wk:yard:${r.id}`] = Math.min(TOP, yardLevel(g, r) + 1);
    tell(`The shipyard at ${r.name} is enlarged to level ${yardLevel(g, r)}.`);
    return;
  }
  if (r.ctrl !== nat) return; // lost before the works were finished
  // City defences: searchlights go up only with the switch on. An order the switch was turned off under waits in
  // the book, paid for, and is delivered in the first month it is on again; off, no mark is written.
  if (what === 'lights' && !cityDefences(g)) { n.queue.push(b); return; }
  // Airfields as targets: a strip level adds to the field's built levels (and is counted as a strip's); a concrete level marks the field concrete.
  if (what === 'field' && isStripBuild(b) && fieldRules(g)) {
    if (workLevel(g, 'field', r.id) >= TOP) return;
    g.flags[`wk:field:${r.id}`] = workLevel(g, 'field', r.id) + 1;
    g.flags[`wk:strip:${r.id}`] = (g.flags[`wk:strip:${r.id}`] ?? 0) + 1;
    tell(`The field strip at ${r.name} is laid: it serves ${airfieldsHold(g, r.id).toLocaleString('en-US')} aircraft.`);
    return;
  }
  if (what === 'field') setConcrete(g, r.id);
  g.flags[`wk:${what}:${r.id}`] = Math.min(TOP, workLevel(g, what, r.id) + 1);
  tell(what === 'aa' ? `The anti-aircraft guns at ${r.name} are in place: level ${workLevel(g, 'aa', r.id)}.`
    : what === 'lights' ? `The searchlights at ${r.name} are in place: level ${workLevel(g, 'lights', r.id)}.`
    : fieldRules(g) ? `The concrete airfield at ${r.name} is ready: level ${workLevel(g, 'field', r.id)}, serving ${airfieldsHold(g, r.id).toLocaleString('en-US')} aircraft.`
    : `The airfields at ${r.name} are ready: level ${workLevel(g, 'field', r.id)}.`);
}

// ---- The computer's use of it -----------------------------------------------------------------------

/**
 * What a computer-run nation's staff orders from the arsenal in the grand campaign, beside the corps its factories
 * raise (economy.ts): guns for its bombed industry, and airfields behind its army. (Release 2, the building desk
 * split: for the player the guns are towns and trade's, a work on the town and no corps, and the airfields
 * recruitment's. 'guns' and 'fields' say which of the two a staff's month asks for.)
 */
export function aiArsenal(g: Game, nat: string, guns = true, fields = true): void {
  const n = g.nations[nat];
  if (!budgeted(g) || !n.alive) return;
  if (guns && !byHand(g, nat, 'towns')) aiGuns(g, nat);
  if (fields && !byHand(g, nat, 'build')) aiAirfields(g, nat);
}

/**
 * A computer staff lays out airfields behind its army (ruling 52(a); the air war of the grand campaign). Where a
 * stretch of its front lies beyond the reach of every airfield its fighters could use, it orders a field from the
 * arsenal in the province of its own that would cover most of the divisions standing there: one behind the line for
 * choice, one in it where it holds nothing else. It waits until its front has stood out of reach so many months
 * running (airwar.json ai.basing 'fields': a field is no answer to a campaign that is over in a month), except
 * where a war begins with the front already out of reach: that field is ordered in the war's first month. One on order
 * at a time, paid for as its guns are, above the staff's reserve. The fighters come up by the staff's own basing rule
 * once the field is open (air.ts, baseByThreat). A player's own production desk orders none: that is his to decide.
 */
function aiAirfields(g: Game, nat: string): void {
  const n = g.nations[nat];
  const wait = AIRWAR.ai.basing.fields;
  if (!airWar(g) || wait === undefined || nat === g.human) return;
  if (n.queue.some((b) => b.what === 'work' && b.item === 'field')) return;
  const key = `bf:${nat}`;
  const bare = bareFront(g, nat);
  if (!bare.length) { delete g.flags[key]; return; }
  // (The month it was first seen so, kept one up so that the campaign's first month is not a nought.)
  const since = g.flags[key] ?? (g.flags[key] = g.turn + 1);
  // A front that begins a war out of reach of every field is not waited on: the staff knew where the war would open.
  const fresh = bare.some(([id]) => g.regions[id].nb.some((j) => {
    const e = g.regions[j].ctrl;
    const began = atWar(g, nat, e) ? g.flags[`war:${nat}:${e}`] ?? g.flags[`war:${e}:${nat}`] : undefined;
    return began !== undefined && g.turn - began <= 1;
  }));
  if (!fresh && g.turn + 1 - since < wait) return;
  const R = fighterReach(g, nat);
  let best: Region | null = null, bv = 0;
  for (const r of g.regions) {
    if (r.ctrl !== nat || hasAirfield(g, r.id)) continue;
    let v = 0;
    for (const [id, divs] of bare) v += divs * cover(km(g, r.id, id), R);
    if (v <= 0) continue;
    // (A field in the front line is the first thing an enemy advance takes.)
    if (!r.nb.some((j) => atWar(g, nat, g.regions[j].ctrl))) v *= 2;
    if (v > bv) { best = r; bv = v; }
  }
  if (!best) return;
  // (Airfields as targets: a forward field is a strip, as the record's were.)
  const want = fieldRules(g) ? 'work:strip' : 'work:field';
  const o = offers(g, nat, best.id).find((x) => x.key === want);
  if (o && o.why === null && n.pp >= o.cost + PRODUCTION.aiReserve) orderOffer(g, nat, want, best.id);
}

/** The land fighter groups a nation has, and the shortest reach among them. */
const landFighters = (g: Game, nat: string) => airFleetsOf(g, nat).filter((f) => f.kind === 'ftr' && f.ftr > 0 && f.ship === undefined);
const fighterReach = (g: Game, nat: string): number => Math.min(...landFighters(g, nat).map((f) => airRange(f, g).ftr));

/**
 * The stretches of a nation's front that no airfield open to its fighter groups can reach: each province where its
 * field corps stand beside ground an enemy holds, with the divisions it has there. Empty when it is at peace or has
 * no fighters.
 */
export function bareFront(g: Game, nat: string): [number, number][] {
  const n = g.nations[nat];
  if (!n?.alive || !n.atWar.length) return [];
  const fighters = landFighters(g, nat);
  if (!fighters.length) return [];
  const R = Math.min(...fighters.map((f) => airRange(f, g).ftr));
  // The front: its field corps beside ground an enemy holds, by their divisions.
  const front = new Map<number, number>();
  for (const c of g.corps) {
    if (c.nat !== nat || c.dead || c.kind === 'fort' || c.kind === 'eng') continue;
    if (g.regions[c.reg].nb.some((j) => atWar(g, nat, g.regions[j].ctrl))) front.set(c.reg, (front.get(c.reg) ?? 0) + c.divs);
  }
  if (!front.size) return [];
  // (A province with no airfield is passed by before the longer question: whyNotBase is never null there.)
  const open = g.regions.filter((r) => r.id === fighters[0].base || (hasAirfield(g, r.id) && whyNotBase(g, fighters[0], r, true) === null));
  return [...front.entries()].filter(([id]) => !open.some((r) => km(g, r.id, id) <= R));
}

/**
 * A computer-run nation in the grand campaign puts guns on its industry once it has been bombed: one level at a
 * time on the province with the most factories that is not yet fully defended.
 */
function aiGuns(g: Game, nat: string): void {
  const n = g.nations[nat];
  // City defences: searchlights asked for before more guns, where a night raid has found them wanting and they count.
  // The guns' own order then goes ahead as it would have, in the same month: a player may order searchlights and guns
  // in one town in one month, money allowing, and the computer is held to no tighter limit (the design director, 25 Nov
  // 2026 12:18Z, on the order of asking).
  if (cityDefences(g)) aiLights(g, nat);
  const raided = g.raids[nat];
  if (!raided || g.turn - raided.turn > 2) return;
  if (n.queue.some((b) => b.what === 'work' && b.item === 'aa')) return;
  let best: Region | null = null;
  for (const r of g.regions) {
    if (!isHome(g, nat, r) || r.fac <= 0 || workLevel(g, 'aa', r.id) >= TOP) continue;
    if (!best || r.fac - 2 * workLevel(g, 'aa', r.id) > best.fac - 2 * workLevel(g, 'aa', best.id)) best = r;
  }
  if (!best) return;
  const o = offers(g, nat, best.id).find((x) => x.key === 'work:aa');
  if (o && o.why === null && n.pp >= o.cost + PRODUCTION.aiReserve) orderOffer(g, nat, 'work:aa', best.id);
}

/**
 * The computer's order of searchlights (city defences, the design director's ruling of 25 Nov 2026 02:23Z, 'Also';
 * form section 7), before its guns order, with the switch on: among its home provinces where a night raid's bombs
 * fell within the last months (citydefences.json ai.months), with more levels of guns built than of searchlights
 * (those on order counted), and where the lights count, for it holds Fighter control's radar (aircampaigns.json
 * radar.control: the lights lift the guns) or one of its fighter groups has the province in reach (the night
 * fighters' floor), it orders a level of searchlights at the one with the most levels of guns (ties: the most
 * factories), one order of searchlights at a time and above the staff's reserve, as its guns. It is asked first; the
 * guns' order follows it in the same month as it would without it. True where a level was ordered, or one is already
 * on order while more are wanted.
 */
function aiLights(g: Game, nat: string): boolean {
  const n = g.nations[nat];
  const radar = hasTech(g, nat, AC.radar.control);
  const fighters = radar ? [] : landFighters(g, nat);
  let best: Region | null = null;
  for (const r of g.regions) {
    if (!isHome(g, nat, r) || !nightRaided(g, r.id)) continue;
    const guns = workLevel(g, 'aa', r.id);
    if (guns <= lightsAt(g, r.id) + queued(g, nat, 'lights', r.id)) continue;
    if (!radar && !fighters.some((f) => km(g, f.base, r.id) <= airRange(f, g).ftr)) continue;
    const bg = best ? workLevel(g, 'aa', best.id) : -1;
    if (!best || guns > bg || (guns === bg && r.fac > best.fac)) best = r;
  }
  if (!best) return false;
  if (n.queue.some((b) => b.what === 'work' && b.item === 'lights')) return true;
  const o = offers(g, nat, best.id).find((x) => x.key === 'work:lights');
  if (!o || o.why !== null || n.pp < o.cost + PRODUCTION.aiReserve) return false;
  return !!orderOffer(g, nat, 'work:lights', best.id);
}

/** The aircraft a nation has bought over its air force's own establishment, with the wings on order, and that establishment, by kind. */
function airBought(g: Game, nat: string): { over: Record<Wing, number>; base: Record<Wing, number> } {
  const over = { ftr: 0, bmr: 0 }, base = { ftr: 0, bmr: 0 };
  for (const f of airFleetsOf(g, nat)) {
    if (f.ship !== undefined || f.ashore !== undefined) continue;
    for (const w of ['ftr', 'bmr'] as Wing[]) { const x = f.extra?.[w] ?? 0; over[w] += x; base[w] += Math.max(0, f.est[w] - x); }
  }
  for (const b of g.nations[nat].queue) if (b.what === 'air' && (b.item === 'ftr' || b.item === 'bmr')) over[b.item] += b.count ?? ARSENAL.air[b.item].count;
  return { over, base };
}

/**
 * What a computer staff's programme would order next, by its character and its war (rulings 68(b) and 70(a)), as
 * the arsenal's own keys in the order it wants them:
 *  - destroyers when its own routes are bled: a convoy route that feeds it lost 'bled' of its cargo last month;
 *  - fighters first while its factories are being bombed;
 *  - submarines against an enemy who lives by sea: it is at war with a nation a convoy route feeds, and has a
 *    fleet of submarines out raiding to send them to;
 *  - a carrier for a carrier power: a navy that has carriers already;
 *  - and wings of fighters and of bombers, the kind it has bought least of first, until what it has bought stands
 *    at 'share' of its air force's own establishment (as a player's own building staff, depth.json 'machines').
 * Never more than 'each' of a kind on order at a time. Whether the yards, the factories, the technology and the
 * treasury allow it is the arsenal's to say, as for a player.
 */
export function programmeWants(g: Game, nat: string): string[] {
  const P = ARSENAL.ai?.programme, n = g.nations[nat];
  if (!P || !n?.alive || !n.atWar.length) return [];
  const room = (what: 'ship' | 'air', item: string): boolean => n.queue.filter((b) => b.what === what && b.item === item).length < (P.each[item] ?? 0);
  const out: string[] = [];
  const fleets = fleetsOf(g, nat);
  const bled = (id: string): number => { const m = g.convoys[id]; return m && m.turn >= g.turn - 1 ? m.loss ?? 0 : 0; };
  if (room('ship', 'dd') && NAVY.convoys.some((c) => c.to === nat && bled(c.id) >= P.bled)) out.push('ship:dd');
  const A = airBought(g, nat);
  const can = (w: Wing): boolean => room('air', w) && A.over[w] + ARSENAL.air[w].count <= P.share * A.base[w];
  const raid = g.raids[nat];
  if (raid && g.turn - raid.turn <= 2 && can('ftr')) out.push('air:ftr');
  if (room('ship', 'ss') && fleets.some((f) => f.duty === 'raid' && f.ships.ss > 0) && NAVY.convoys.some((c) => g.nations[c.to]?.alive && atWar(g, nat, c.to))) out.push('ship:ss');
  if (room('ship', 'cv') && fleets.some((f) => f.ships.cv > 0)) out.push('ship:cv');
  const fill = (w: Wing): number => A.over[w] / Math.max(1, A.base[w]);
  for (const w of (fill('ftr') <= fill('bmr') ? ['ftr', 'bmr'] : ['bmr', 'ftr']) as Wing[]) if (can(w) && !out.includes(`air:${w}`)) out.push(`air:${w}`);
  return out;
}

/**
 * A computer staff's programme at sea and in the air (rulings 68(b) and 70(a)): the grand campaign, from the stop
 * of the history dial the data names (arsenal.json ai.programme; below it history's schedules are the whole of
 * its building, as the book). It orders ships and wings from the arsenal as a player does, at his prices and
 * under his rules, and pays for them in two ways:
 *  - its share. While there is something the arsenal would build for it, the staff sets aside 'rate' of the
 *    month's income for its navy and as much for its air force, never taking the treasury below its reserve and
 *    never holding more than the dearest thing it wants costs. What is set aside is spent on nothing else.
 *  - idle money: what the treasury holds over the price of a corps of infantry and the reserve, or over the
 *    reserve alone while it can raise no corps for want of men (money without men).
 * It comes after the corps, the guns and the works; one order a month for the navy and one for the air force, the
 * first of what it wants (programmeWants) that the arsenal allows and the treasury could keep.
 */
export function aiProgramme(g: Game, nat: string): void {
  const P = ARSENAL.ai?.programme, n = g.nations[nat];
  if (!P || !budgeted(g) || nat === g.human || !n?.alive || !n.atWar.length || stopOf(g) < P.fromStop) return;
  const wants = programmeWants(g, nat);
  if (!wants.length) return;
  const men = menSpare(g, nat) >= (specOf(g, 'inf')?.divs ?? 1);
  const keep = PRODUCTION.aiReserve + (men ? corpsList(g, nat, 'inf') : 0);
  const inc = income(g, nat);
  let spare = inc - upkeepTotal(g, nat);
  // What the arsenal would build for it: an order it could place, or one that waits only for the money.
  const list = offers(g, nat).filter((o) => o.at >= 0 && (o.why === null || o.poor));
  for (const arm of ['ship', 'air'] as const) {
    const mine = wants.filter((k) => k.startsWith(`${arm}:`)).map((k) => list.find((o) => o.key === k)).filter((o): o is Offer => !!o);
    const key = purseKey(nat, arm);
    let purse = g.flags[key] ?? 0;
    if (mine.length) {
      const put = Math.min(Math.max(...mine.map((o) => o.cost)) - purse, (P.rate?.[arm] ?? 0) * inc, n.pp - PRODUCTION.aiReserve);
      if (put > 0) { n.pp -= put; purse += put; }
    }
    for (const o of mine) {
      if (spare < o.upkeep * BUDGET.debt.aiMargin) continue;
      const set = purse >= o.cost;
      if (!set && n.pp < o.cost + keep) continue;
      // (What was set aside comes back to the treasury to pay for the order it was kept for.)
      if (set) n.pp += o.cost;
      if (orderOffer(g, nat, o.key, o.at)) { if (set) purse -= o.cost; spare -= o.upkeep; break; }
      if (set) n.pp -= o.cost;
    }
    if (purse > 0.005) g.flags[key] = Math.round(purse * 100) / 100; else delete g.flags[key];
  }
}

const purseKey = (nat: string, arm: 'ship' | 'air'): string => `pg:${nat}:${arm}`;
/** What a computer staff has set aside for its navy and for its air force and not yet spent (aiProgramme). */
export const programmePurse = (g: Game, nat: string): { ship: number; air: number } => ({ ship: g.flags[purseKey(nat, 'ship')] ?? 0, air: g.flags[purseKey(nat, 'air')] ?? 0 });

/** The coast watch's and the diggers' numbers (open.json, 'coast'): staff.ts keeps them, ai.ts hands them over. */
export interface FortRules { works: number; dig?: { orders: number; top: number; capital: number; keep: number } }

/**
 * A computer-run nation in the grand campaign fortifies (Version 69), with idle money only: what the treasury
 * holds over the price of a corps of infantry and the reserve. (The factories raise corps first, each month; money
 * above that price is money they could not turn into troops, for want of men, of upkeep or of room in the
 * programme. So the works never take the place of a corps the treasury is saving for.) The harbours of its
 * homeland where its staff keeps a coast watch are given works first, one order at a time. And a nation that can
 * raise no more corps for want of men digs in: fortifications in the provinces of its homeland nearest the enemy
 * that he does not yet stand next to, so many orders a month, with 'keep' never touched.
 * The player's own building staff does neither: his fortifications are his to order.
 */
export function aiFortify(g: Game, nat: string, watch: string[], C: FortRules | undefined): void {
  const n = g.nations[nat];
  if (!C || g.flags.open !== 1 || !budgeted(g) || nat === g.human || !n?.alive || n.capital < 0 || !n.atWar.length) return;
  const level = (r: Region): number => (r.fort?.lv ?? 0) + n.queue.filter((b) => b.what === 'fort' && b.region === r.id).length;
  const order = (r: Region, keep: number): boolean => { const cost = fortCost(g, nat, r); return cost !== null && n.pp >= cost + keep && !!orderFort(g, nat, r.id); };
  const idle = corpsList(g, nat, 'inf') + PRODUCTION.aiReserve;
  const coast = watch.map((nm) => g.byName[nm]).filter((r) => !!r && isHome(g, nat, r) && level(r) < C.works).sort((a, b) => level(a) - level(b) || a.id - b.id);
  if (coast.length && !n.queue.some((b) => b.what === 'fort' && watch.includes(g.regions[b.region].name))) order(coast[0], idle);
  const D = C.dig;
  if (!D || menSpare(g, nat) >= (specOf(g, 'inf')?.divs ?? 1)) return;
  // How many marches each province lies from ground an enemy holds.
  const dist = new Int16Array(g.regions.length).fill(-1);
  const q: number[] = [];
  for (const r of g.regions) if (atWar(g, nat, r.ctrl)) { dist[r.id] = 0; q.push(r.id); }
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (dist[j] < 0) { dist[j] = dist[q[i]] + 1; q.push(j); }
  const cands = g.regions
    .filter((r) => isHome(g, nat, r) && !r.overseas && dist[r.id] >= 2 && level(r) < (r.id === n.capital ? D.capital : D.top))
    .sort((a, b) => dist[a.id] - dist[b.id] || level(a) - level(b) || a.id - b.id);
  let k = 0;
  for (const r of cands) { if (k >= D.orders || !order(r, Math.max(D.keep, idle))) break; k++; }
}
