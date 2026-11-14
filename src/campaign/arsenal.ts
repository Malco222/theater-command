// The arsenal of the grand campaign: everything a nation can order with its treasury. Corps of every kind,
// aircraft by the wing, warships and troopships, and the works a province can be given: factories, shipyards,
// anti-aircraft guns and airfields. One list of offers says what can be ordered, where it would go, what it costs
// to buy and to keep, and why not when it cannot be; one function places the order. Both interfaces read the
// same list. The numbers live in data/campaign/arsenal.json. The historical scenarios offer only the four
// classic corps and fortifications, as they always did.

import data from '../../data/campaign/arsenal.json';
import { AIRWAR, airFleetsOf, airPool, airWar, carrierRules, foundAirFleet, hasAirfield } from './air';
import { syncWings } from './carriers';
import { MANPOWER, classNow, menLeft, menRules, menWords, raisable } from './manpower';
import { govMul } from './government';
import { BUDGET, PRODUCTION, budgeted, cadreTerms, cadresOf, canRaise, corpsCost, corpsKinds, corpsUpkeep, fortCost, isHome, needsOf, orderCadre, orderCorps, orderElite, orderFort, orderPara, raiseSite, specOf, whyNoCadre, type LedgerLine } from './economy';
import { TECH, locked, paraCount, whyNoPara } from './tech';
import { eliteCount, eliteOf, eliteRule, priceOf, whyNoElite } from './traits';
import { headline } from './game';
import { NAVY, fleetsOf, foundFleet, shipCount } from './navy';
import { MEN_PER_DIV, type AirFleet, type Build, type Fleet, type Game, type Kind, type Region, type ShipClass } from './types';

export type Wing = 'ftr' | 'bmr';
export type Hull = ShipClass | 'tp';
export type Work = 'factory' | 'yard' | 'aa' | 'field';

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
  };
  yards: Record<string, number>;
  guns: { soften: number; softenMost: number; thicken: number; thickenMost: number };
};

const TOP = 3;
const plural = (n: number, one: string, many = `${one}s`): string => `${n} ${n === 1 ? one : many}`;
const cap = (s: string): string => s[0].toUpperCase() + s.slice(1);

// ---- Works: what has been built in each province -------------------------------------------------------
// Levels are kept as marks on the game ('wk:aa:17' = 2), so a save carries them and a game that never builds
// any is exactly the game it was before.

/** The level of anti-aircraft guns or airfields built in a province. */
export const workLevel = (g: Game, what: 'aa' | 'field', region: number): number => g.flags[`wk:${what}:${region}`] ?? 0;

/** How many factories have been added to a province since the game began. */
export const factoriesBuilt = (g: Game, region: number): number => g.flags[`wk:fac:${region}`] ?? 0;

/** A port's shipyard: what history left there, or what has been built since, whichever is greater. */
export function yardLevel(g: Game, r: Region): number {
  return r.port ? Math.max(g.flags[`wk:yard:${r.id}`] ?? 0, ARSENAL.yards[r.name] ?? 0) : 0;
}

/** A city: a province where a factory can be built. */
export const isCity = (r: Region): boolean => r.terrain === 'urban' || r.cap || r.fac > 0 || r.vp >= 2;

const queued = (g: Game, nat: string, item: string, region: number): number => g.nations[nat].queue.filter((b) => b.what === 'work' && b.item === item && b.region === region).length;

/** What guns and airfields cost a nation to man each month, as lines for its ledger. */
export function worksUpkeep(g: Game, nat: string): LedgerLine[] {
  let aa = 0, field = 0;
  for (const k in g.flags) {
    if (!k.startsWith('wk:')) continue;
    const [, what, id] = k.split(':');
    if ((what !== 'aa' && what !== 'field') || g.regions[+id]?.ctrl !== nat) continue;
    if (what === 'aa') aa += g.flags[k]; else field += g.flags[k];
  }
  const out: LedgerLine[] = [];
  if (aa > 0) out.push({ key: 'works:aa', label: 'Anti-aircraft guns', amount: aa * ARSENAL.works.aa.upkeep, count: aa });
  if (field > 0) out.push({ key: 'works:field', label: 'Airfields', amount: field * ARSENAL.works.field.upkeep, count: field });
  return out;
}

/** On loading a save: the factories built since the game began go back on the map. */
export function restoreWorks(g: Game): void {
  for (const k in g.flags) if (k.startsWith('wk:fac:')) { const r = g.regions[+k.slice(7)]; if (r) r.fac += g.flags[k]; }
}

// ---- Where things go -------------------------------------------------------------------------------

/** True if this nation has an aircraft industry: a home province with this many factories. */
const industry = (g: Game, nat: string, factories: number): boolean => g.regions.some((r) => isHome(g, nat, r) && r.fac >= factories);

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
  if (main) return main.base;
  return g.regions.find((r) => canStation(g, nat, r))?.id ?? -1;
}

/**
 * The air fleet a wing stationed here would join, if one of the nation's is based here. In the air war a wing joins
 * an air group of its own kind that has room for it, and otherwise founds a new group.
 */
const fleetAt = (g: Game, nat: string, region: number, wing?: Wing): AirFleet | null => strongest(airFleetsOf(g, nat).filter((f) => f.base === region && f.ship === undefined
  && (!airWar(g) || !wing || (f.kind === wing && f.est[wing] + ARSENAL.air[wing].count <= AIRWAR.groups.most))));

/** True if a ship needing a yard of this level can be laid down here. */
export const canLay = (g: Game, nat: string, r: Region | undefined, yard: number): boolean => !!r && isHome(g, nat, r) && r.port && yardLevel(g, r) >= yard;

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

const liftBought = (g: Game, nat: string): number => (g.flags[`wk:tp:${nat}`] ?? 0) + g.nations[nat].queue.filter((b) => b.what === 'ship' && b.item === 'tp').length;

// ---- The offers -------------------------------------------------------------------------------------

export interface Offer {
  /** What it is: 'corps:inf', 'air:ftr', 'ship:dd', 'work:aa', 'work:fort', 'elite' (the special formation), 'para' (an airborne corps), 'cadre:<corps id>' (a folded corps raised again). Pass it to orderOffer. */
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
      why: whyNoCadre(g, nat, c) ?? (at < 0 ? 'There is no home ground left to raise troops on.' : noMen(c.divs) ?? poor(t.cost)),
      ...menFor(c.divs),
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
    const built = industry(g, nat, spec.factories);
    const at = built ? stationSite(g, nat, sel) : -1;
    const joins = at >= 0 ? fleetAt(g, nat, at, wing) : null;
    const cost = priceOf(g, nat, `air:${wing}`, spec.cost);
    out.push({
      key: `air:${wing}`, group: 'air', label: spec.label, blurb: spec.blurb, size: `${spec.count} ${wing === 'ftr' ? 'fighters' : 'bombers'}`, cost, months: spec.months,
      upkeep: spec.count * BUDGET.upkeep.air[wing], at,
      note: at < 0 ? '' : joins ? `Joins ${joins.name} at ${g.regions[at].name}.` : airWar(g) ? `Forms a new air group on the airfield at ${g.regions[at].name}.` : `Founds a new air fleet on the airfields of ${g.regions[at].name}.`,
      why: !built ? `Needs an aircraft industry: a home province with ${spec.factories} factories.` : at < 0 ? 'Needs airfields to fly from. Build airfields in a province first.' : poor(cost),
    });
  }

  for (const hull of ['dd', 'ss', 'tp', 'ca', 'cv', 'bb'] as Hull[]) {
    const spec = ARSENAL.ships[hull];
    const at = slipSite(g, nat, spec.yard, sel);
    const fleet = at >= 0 && hull !== 'tp' ? joinsFleet(g, nat, hull, at) : null;
    const full = hull === 'tp' && liftBought(g, nat) >= ARSENAL.lift.most;
    const cost = priceOf(g, nat, `ship:${hull}`, spec.cost);
    const one = hull === 'tp' ? '' : NAVY.classes[hull].one, many = hull === 'tp' ? '' : NAVY.classes[hull].many;
    const held = gate(`ship:${hull}`);
    out.push({
      ...(held ? { tech: held.tech } : {}),
      key: `ship:${hull}`, group: 'navy', label: spec.label, blurb: spec.blurb, size: hull === 'tp' ? 'lift for one more corps a month' : plural(spec.count, one, many), cost, months: spec.months,
      upkeep: hull === 'tp' ? 0 : spec.count * BUDGET.upkeep.ships[hull], at,
      note: at < 0 ? '' : hull === 'tp' ? `Built at ${g.regions[at].name}.` : fleet ? `Laid down at ${g.regions[at].name}. Joins the ${fleet.name}.` : `Laid down at ${g.regions[at].name}. Forms a new squadron there.`,
      why: held?.why ?? (at < 0 ? `Needs a shipyard of level ${spec.yard} in one of your home ports.` : full ? 'The nation has all the troopships its ports can handle.' : poor(cost)),
    });
  }

  // Works are built in the province picked on the map.
  const W = ARSENAL.works;
  const pick = 'Pick one of your own provinces on the map.';
  {
    const have = r ? factoriesBuilt(g, r.id) + queued(g, nat, 'factory', r.id) : 0;
    const most = r && (r.terrain === 'urban' || r.cap) ? W.factory.most.city : W.factory.most.town;
    const cost = W.factory.cost + W.factory.step * have;
    const why = !r ? pick : !isHome(g, nat, r) || r.overseas ? 'Factories are built in the home country.' : !isCity(r) ? 'Factories are built in cities: a province that is urban, the capital, has industry already or is a place of some size.' : have >= most ? `${r.name} has room for no more new factories.` : poor(cost);
    out.push({ key: 'work:factory', group: 'works', label: W.factory.label, blurb: W.factory.blurb, size: 'one factory', cost, months: W.factory.months, upkeep: 0, at: r ? r.id : -1, note: r ? `${r.name} has ${plural(r.fac, 'factory', 'factories')}${have ? `, ${have} of ${most} new ones built or building` : ''}.` : '', why });
  }
  const FULL = { yard: 'The shipyard at % is as large as a yard can be.', aa: 'The guns at % are as heavy as they can be.', field: 'The airfields at % are as large as they can be.' };
  const NONE = { yard: 'no shipyard', aa: 'no guns', field: 'only rough landing grounds' };
  const level = (what: 'yard' | 'aa' | 'field', spec: LevelSpec, now: number, upkeep: number, why: string | null): void => {
    const next = now + (r ? queued(g, nat, what, r.id) : 0) + 1;
    const cost = priceOf(g, nat, `work:${what}`, spec.cost[Math.min(TOP, next)]);
    out.push({
      key: `work:${what}`, group: 'works', label: spec.label, blurb: spec.blurb, size: `level ${Math.min(TOP, next)}`, cost, months: spec.months, upkeep, at: r ? r.id : -1,
      note: r ? `${r.name} has ${now ? `level ${now}` : NONE[what]}${next - 1 > now ? `, level ${next - 1} building` : ''}.` : '',
      why: why ?? (next > TOP ? FULL[what].replace('%', r!.name) : poor(cost)),
    });
  };
  level('yard', W.yard, r ? yardLevel(g, r) : 0, 0, !r ? pick : !isHome(g, nat, r) ? 'Shipyards are built in the home country.' : !r.port ? `${r.name} has no harbour.` : null);
  level('aa', W.aa, r ? workLevel(g, 'aa', r.id) : 0, W.aa.upkeep, !r ? pick : r.ctrl !== nat ? 'That ground is not in your hands.' : null);
  level('field', W.field, r ? workLevel(g, 'field', r.id) : 0, W.field.upkeep, !r ? pick : r.ctrl !== nat ? 'That ground is not in your hands.' : null);
  {
    const cost = r ? fortCost(g, nat, r) : null;
    if (r && cost !== null) out.push(fortOffer(g, nat, r, cost, poor(cost)));
    else out.push({ key: 'work:fort', group: 'works', label: 'Fortifications', blurb: FORT_BLURB, size: '', cost: priceOf(g, nat, 'work:fort', PRODUCTION.fort.cost[1]), months: PRODUCTION.fort.months, upkeep: 0, at: r ? r.id : -1, note: '', why: !r ? pick : !isHome(g, nat, r) ? 'Fortifications are built on home ground.' : `${r.name} is fortified as heavily as it can be.` });
  }
  return out;
}

const FORT_BLURB = 'Defenders behind works fight far harder, and the works face every direction. A line only helps if the enemy has to come through it.';

function fortOffer(g: Game, nat: string, r: Region, cost: number, why: string | null): Omit<Offer, 'poor'> {
  const lv = (r.fort?.lv ?? 0) + g.nations[nat].queue.filter((b) => b.what === 'fort' && b.region === r.id).length + 1;
  return { key: 'work:fort', group: 'works', label: 'Fortifications', blurb: FORT_BLURB, size: `level ${lv}`, cost, months: PRODUCTION.fort.months, upkeep: 0, at: r.id, note: `${r.name} has ${r.fort ? `level ${r.fort.lv}` : 'none'}${lv - 1 > (r.fort?.lv ?? 0) ? `, level ${lv - 1} building` : ''}.`, why };
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
  if (group === 'air') { b.count = ARSENAL.air[item as Wing].count; b.name = `${o.label} for ${r.name}`; }
  else if (group === 'ship') { b.count = ARSENAL.ships[item as Hull].count; b.name = `${o.label} at ${r.name}`; }
  else b.name = `${o.label}${item === 'factory' ? '' : `, ${o.size},`} at ${r.name}`;
  n.queue.push(b);
  return b;
}

// ---- Delivery ---------------------------------------------------------------------------------------

/** Hands over an order from the arsenal that is due: aircraft to their fleet, ships to theirs, works to the province. */
export function deliverArsenal(g: Game, nat: string, b: Build): void {
  const n = g.nations[nat];
  const r = g.regions[b.region];
  const tell = (text: string): void => { if (nat === g.human) headline(g, 'info', text, nat, r.id); };
  if (b.what === 'air') {
    const wing = b.item as Wing, count = b.count ?? ARSENAL.air[wing].count;
    const word = `${count} ${wing === 'ftr' ? 'fighters' : 'bombers'}`;
    // The wing goes where it was ordered for; if that ground is lost, to the strongest fleet the nation has left.
    const war = airWar(g);
    let f = canStation(g, nat, r) || r.ctrl === nat ? fleetAt(g, nat, r.id, wing) : null;
    if (!f && r.ctrl === nat && (!war || hasAirfield(g, r.id))) {
      f = foundAirFleet(g, nat, r, `${r.name} Air Command`, war ? wing : undefined);
      tell(war ? `A new air group, the ${f.name}, forms on the airfield at ${r.name} with ${word}.` : `A new air fleet, ${f.name}, forms on the airfields of ${r.name} with ${word}.`);
    } else {
      // The air war: the wing goes to the strongest group of its own kind.
      f ??= strongest(airFleetsOf(g, nat).filter((x) => x.ship === undefined && (!war || x.kind === wing)));
      if (f) tell(`${cap(word)} from the factories join ${f.name} at ${g.regions[f.base].name}.`);
    }
    if (!f) {
      // No air fleet and nowhere to found one: the machines wait in reserve.
      const p = airPool(g, nat);
      if (wing === 'ftr') { p.ftr += count; p.pilots += count; } else p.bmr += count;
      return;
    }
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
      tell(`New troopships are ready: the nation can now move ${plural(n.sealift, 'corps', 'corps')} by sea each month.`);
      return;
    }
    let f = joinsFleet(g, nat, hull, r.id);
    if (!f && isHome(g, nat, r) && r.port && r.seas.length) f = foundFleet(g, nat, r, `${r.name} Squadron`);
    if (!f) { tell(`With no port left to take them, the ${NAVY.classes[hull].many} building at ${r.name} are lost.`); return; }
    f.ships[hull] += count;
    // A new carrier joins with its aircraft aboard (the grand campaign).
    const decks = hull === 'cv' && carrierRules(g);
    if (decks) syncWings(g);
    tell(`${cap(plural(count, `new ${NAVY.classes[hull].one}`, `new ${NAVY.classes[hull].many}`))} ${count === 1 ? 'joins' : 'join'} the ${f.name}${decks ? ', with fighters and strike aircraft aboard' : ''}.`);
    return;
  }
  const what = b.item as Work;
  if (what === 'factory') {
    if (!isHome(g, nat, r)) return; // the city was lost before the factory was finished
    r.fac += 1;
    g.flags[`wk:fac:${r.id}`] = factoriesBuilt(g, r.id) + 1;
    tell(`The new factory at ${r.name} is at work: the province now has ${r.fac}.`);
    return;
  }
  if (what === 'yard') {
    if (!isHome(g, nat, r) || !r.port) return;
    g.flags[`wk:yard:${r.id}`] = Math.min(TOP, yardLevel(g, r) + 1);
    tell(`The shipyard at ${r.name} is enlarged to level ${yardLevel(g, r)}.`);
    return;
  }
  if (r.ctrl !== nat) return; // lost before the works were finished
  g.flags[`wk:${what}:${r.id}`] = Math.min(TOP, workLevel(g, what, r.id) + 1);
  tell(what === 'aa' ? `The anti-aircraft guns at ${r.name} are in place: level ${workLevel(g, 'aa', r.id)}.` : `The airfields at ${r.name} are ready: level ${workLevel(g, 'field', r.id)}.`);
}

// ---- The computer's use of it -----------------------------------------------------------------------

/**
 * A computer-run nation in the grand campaign puts guns on its industry once it has been bombed: one level at a
 * time on the province with the most factories that is not yet fully defended. It buys nothing else from the
 * arsenal: its fleets and air forces grow on history's programmes.
 */
export function aiArsenal(g: Game, nat: string): void {
  const n = g.nations[nat];
  if (!budgeted(g) || nat === g.human || !n.alive) return;
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
