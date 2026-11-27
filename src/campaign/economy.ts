// The war economy: factories earn production points, losses are made good first, and
// what is left buys new formations and fortifications that take months to arrive.
// The numbers live in data/campaign/production.json so they can be changed without code.

import { byHand } from './depth';
import data from '../../data/campaign/production.json';
import budgetData from '../../data/campaign/budget.json';
import arsenalData from '../../data/campaign/arsenal.json';
import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import setup from '../../data/campaign/setup-1939.json';
import rivalsData from '../../data/campaign/rivals.json';
import { fits, islandOf, roomLeft, roomForDivision } from './islands';
import type { Build, Corps, Game, Kind, Region, ShipClass, Terrain } from './types';
import { addCorps, allied, armySize, fights, headline, roomless } from './game';
import { dropBattle, reformNames } from './drawn';
import { DIPLOMACY, tradePartners } from './diplomacy';
import { campXp, civicSum, deliverCivic, hallShare, settledFor, settledTownsOn } from './buildings';
import { dealsOf, tradeIncome } from './trade';
import { airIncomeLoss } from './air';
import { seaIncomeLoss } from './navy';
import { techMul } from './research';
import { fallenOf, inherit, noteCorps, raisedXp, revivable } from './stories';
import { airLine, deliverArsenal, worksUpkeep, type AirDelivery } from './arsenal';
import { barredKind } from './ground';
import { workingFactories } from './rails';
import { damageOf } from './citydamage';
import { incomeMul, levelOf } from './difficulty';
import { eliteOf, priceOf, traitMoney, whyNoElite } from './traits';
import { isTeam, TEAM_KINDS, TEAMS, teamName, teamsOn, type TeamKind } from './teams';
import { Reckoning } from './orderworth';
import { TECH, locked, whyNoPara } from './tech';
import { fieldOut, keepsStores, shortOfOil, supIncome } from './resources';
import { SIEGE, besieged } from './siege';
import { atomBill } from './atom';
import { prizeIncome, prizeOut, prizeRules } from './prizes';
import { govMul } from './government';
import { MANPOWER, drawMen, menSpare, raisable, returnMen } from './manpower';
import { armOrder, binds, canArm, carved, riflesRaised, unarmOrder } from './rifles';
import { raiseMorale } from './morale';
import { WORLD_MAP, armyIdle, peaceFooting, shoreOf, warFooting, warWorkLeft } from './world';
import { footLabel, footLineStays, footShare } from './footing';
import { bargainCost, bargainGoodsMoney, bargainMoney, bargainRules, bargainSales } from './bargains';

export interface BuildSpec { label: string; divs: number; cost: number; months: number; blurb: string }

export const PRODUCTION = data as unknown as {
  baseIncome: number; occupiedIncome: number; peaceIncome: number; replacementCost: number; manpower: number; aiLine?: number;
  aiBank: [number, number]; aiReserve: number;
  builds: Record<string, BuildSpec>;
  fort: { cost: number[]; months: number };
  firstCorpsNumber: Record<string, number>;
  armourName: Record<string, string>;
};

/**
 * What a corps of this kind costs to raise, or undefined if it cannot be ordered. The four classic corps can be
 * raised in any game; the kinds the arsenal adds, such as artillery, only in the grand campaign.
 */
/** A new corps' training and readiness when it finishes forming (a computer staff's; the player's training is his own). */
export const FRESH = { xp: 0.05, org: 0.7 } as const;

export function specOf(g: Game, kind: string): BuildSpec | undefined {
  // The support teams (release 2) only where they are switched on (src/campaign/teams.ts).
  if (isTeam({ kind })) return teamsOn(g) ? TEAMS[kind as TeamKind] : undefined;
  return PRODUCTION.builds[kind] ?? (budgeted(g) ? (arsenalData.land as Record<string, BuildSpec>)[kind] : undefined);
}

/** Every kind of corps that can be ordered in this game, lightest first. */
export function corpsKinds(g: Game): Kind[] {
  const kinds = [...Object.keys(PRODUCTION.builds), ...(budgeted(g) ? Object.keys(arsenalData.land) : []), ...(teamsOn(g) ? TEAM_KINDS : [])] as Kind[];
  // The fighting arms by the industry they need and their price, and the engineers after them.
  return kinds.sort((a, b) => Number(a === 'eng') - Number(b === 'eng') || needsOf(g, a) - needsOf(g, b) || (specOf(g, a)!.cost - specOf(g, b)!.cost));
}

/** What a corps of this kind is priced at for this nation: the list price, less what its own abilities take off (the grand campaign). */
export function corpsList(g: Game, nat: string, kind: string): number {
  const price = priceOf(g, nat, `corps:${kind}`, specOf(g, kind)?.cost ?? 0);
  // The player's Minister of Armaments drives the price down, or up (the grand campaign).
  const m = govMul(g, nat, 'cost');
  return m === 1 ? price : Math.round(price * m * 10) / 10;
}

/**
 * What a corps of this kind costs this nation in points: its price. Where the nation's store of rifles binds, the
 * price no longer charges for the rifles the store charges (rifles.ts carve).
 */
export const corpsCost = (g: Game, nat: string, kind: string): number => carved(g, nat, corpsList(g, nat, kind), specOf(g, kind)?.divs ?? 0);

/** How many factories a province must have for a corps of this kind to be raised there. None in a historical scenario. */
export const needsOf = (g: Game, kind: string): number => (budgeted(g) ? (arsenalData.needs as Record<string, number>)[kind] ?? 0 : 0);

/** True if this nation can raise a corps of this kind in this region: home ground, with the industry heavier equipment needs. */
export const canRaiseKind = (g: Game, nat: string, r: Region | undefined, kind: string): boolean => !!r && canRaise(g, nat, r) && r.fac >= needsOf(g, kind) && !barredKind(g, kind as Kind, r);

/** Where a new corps of this kind would form: the region asked for if it will do, else the capital, else the home province with the most industry. -1 if nowhere. */
export function raiseSite(g: Game, nat: string, kind: string, want = -1): number {
  if (canRaiseKind(g, nat, g.regions[want], kind)) return want;
  const cap = g.nations[nat].capital;
  if (canRaiseKind(g, nat, g.regions[cap], kind)) return cap;
  let best = -1;
  for (const r of g.regions) if (canRaiseKind(g, nat, r, kind) && (best < 0 || r.fac > g.regions[best].fac)) best = r.id;
  return best;
}

export const firstCorpsNumber = (nat: string): number => PRODUCTION.firstCorpsNumber[nat] ?? PRODUCTION.firstCorpsNumber._default;

/**
 * The budget: every province yields something, every corps, ship and aircraft costs upkeep, and the treasury can
 * run into debt. The numbers live in data/campaign/budget.json.
 */
export const BUDGET = budgetData as unknown as {
  scale: number; capital: number;
  land: Record<Terrain, number>; port: number; oil: number; occupied: number; overseas: number;
  /**
   * Rider b462 to ruling 231: the share of what a colony on the far side of the world pays that reaches its own
   * country's treasury, by part; 'home' names the colony-marked provinces out there that are part of their country's
   * own economy and pay it as home ground does (Korea, Formosa, Karafuto).
   */
  farColony: { land: number; port: number; oil: number; fac: number; home: string[]; guard?: number; posts?: string[] };
  /** Ruling b466: the share at which a country's own home ground on the far side counts, for its pay and for lines. */
  farHome: Record<string, number>;
  /** Ruling 214: a factor on the land and harbour pay of the provinces a smaller power owned on the first day. */
  landFactor: Record<string, number>;
  upkeep: { corps: Record<Kind, number>; ships: Record<ShipClass, number>; air: { ftr: number; bmr: number }; peace: number };
  debt: { floor: number; rest: number; desert: number; aiMargin: number };
};

/**
 * True when the budget rules are in force. That is the grand campaign: the historical scenarios keep the plain
 * arithmetic of factories and replacements, so that history's timetable holds.
 */
export const budgeted = (g: Game): boolean => g.flags.open === 1;

/** What one province brings its holder each month, before the nation-wide gains and losses. */
export interface Yield {
  land: number; port: number; oil: number; fac: number; capital: number; total: number;
  /** True while an enemy army stands round its town: it pays its holder little or nothing. */
  siege?: boolean;
  /** Damaged cities (the grand campaign with its switch): the share of the town in ruins, 0 to 0.8, while any of it is. Its factories, land, harbour and seat of government pay that much less. */
  damage?: number;
}

/**
 * Ruling 214: a smaller power's land and harbours pay by its worth, not by the count of its provinces (Australia's
 * 74 provinces paid nearly three times Italy's land). The factor is the first-day owner's and goes with the ground,
 * whoever holds it; a country not listed is 1. The grand campaign only.
 */
export const landFactor = (r: Region): number => BUDGET.landFactor[r.first ?? r.owner] ?? 1;

/**
 * Ruling 210(a) and (e): a cut moves worth and never adds it. A province and the cities cut out of it since the first
 * chart (the chart's 'of') pay together in land what the province pays alone, each by its own ground: Baltimore's and
 * Maryland's land add up to Maryland's. Where ruling 214's factor is set, the pay does not take the share again: the
 * historian set those sixteen factors on the chart with every city paying in full, to Italy's rate and no lower. Trade
 * reads every country's land with the share (`always`), so that no country's trade moves with the factor (214(c)).
 */
let shares: Map<number, number> | null = null;
export function landShare(r: Region, always = false): number {
  if (!always && landFactor(r) !== 1) return 1;
  if (!shares) {
    shares = new Map();
    const rows = WORLD_MAP.regions as { id: number; terrain: Terrain; of?: number }[];
    const rate = new Map(rows.map((x) => [x.id, BUDGET.land[x.terrain] ?? 0]));
    const families = new Map<number, number[]>();
    for (const x of rows) if (x.of !== undefined) families.set(x.of, [...(families.get(x.of) ?? [x.of]), x.id]);
    for (const [p, ids] of families) {
      const all = ids.reduce((s, id) => s + (rate.get(id) ?? 0), 0);
      const f = all > 0 ? (rate.get(p) ?? 0) / all : 1;
      for (const id of ids) shares.set(id, f);
    }
  }
  return shares.get(r.id) ?? 1;
}

const FAR_HOME = new Set(BUDGET.farColony.home);
/**
 * True for ground that pays its own country as a colony does, and whose factories form no corps for it: ground with
 * the colony mark, but for the few provinces that were part of their country's own economy (budget.json,
 * 'farColony.home'). Where its corps are raised and its works built is another matter (canRaise, arsenal.ts).
 */
export const colonyPurse = (r: Region): boolean => !!r.overseas && !FAR_HOME.has(r.name);
/** True for such ground on the far side of the world (world.ts shoreOf 2). None while the far side's switch is off. */
export const farColony = (r: Region): boolean => colonyPurse(r) && shoreOf(r) === 2;
const FAR_POSTS = new Set(BUDGET.farColony.posts ?? []);
/**
 * The share of a town's own guard that stands on this ground (ruling 234's rider b547; siege.ts guardOf). 1 on home
 * ground and everywhere on the near side. On the far side's colonial ground the record's towns were held by the
 * garrisons the rows already bring, not by a home town's division of depot troops, police and militia: a far post
 * whose garrison the historian has given (budget.json 'farColony.posts') keeps no guard beside it, so that the
 * record's figure is its whole defence, counted once; any other town there keeps the far side's one number, the tenth.
 * One rule for every power (rider b552): Japan's mandates are colonial ground, and Saipan and Truk take the tenth.
 * "Counted once" reaches the far colonial cities whose defenders the rows already hold (riders b561(d) and b563):
 * Hong Kong, Manila, Rangoon, Batavia, the Indies' outposts, the four cities of Indochina, Colombo and Wake, whose
 * fort row holds it; and Singapore, whose garrison row rose to the record's 0.8 as its guard came off. Midway, not yet
 * checked against the record, keeps the tenth beside its fort; Hawaii is home ground.
 * The post rule reaches home ground on the far side (rider b614 to ruling 234): Japan's island garrisons are rows at
 * the record's worth by half-year, the Mandates' and the home islands' alike, so Marcus Island, Iwo Jima and Chichi
 * Jima hold the record's worth and not a home town's whole guard. Only places the historian's table names are posts.
 */
const farPost = (r: Region): boolean => FAR_POSTS.has(r.name) && shoreOf(r) === 2;
export const farGuard = (r: Region): number => (farPost(r) ? 0 : !farColony(r) ? 1 : BUDGET.farColony.guard ?? 1);
/**
 * The far chart's guard table (data/campaign/far-guards.json, laid over siege.json with the far side's switch on;
 * the design director's rulings of 21 November 2026 on the historian's far-chart-town-guards.md and its globe
 * table): the row of the land a town on the far side of the world stands in, or null where its land has none. A land
 * is an owner's ground on the far side: all of it where the row names no places, or the provinces of the first chart
 * it names with the parts the chart cuts them into. A figure is a number, or the record's figure by month where it
 * changed, as the airfield table's is (game.ts estAt). With the switch off siege.json has no table, and a province
 * of the old chart or of the Americas is never looked up. Read by guardOf (siege.ts) alone.
 */
export type FarGuardFig = number | [number, number][];
export interface FarGuardRow { name: string; owner: string; town: FarGuardFig; city: FarGuardFig; capital: FarGuardFig; named?: Record<string, FarGuardFig>; places?: string[] }
let farLands: { whole: Map<string, FarGuardRow>; by: Map<string, FarGuardRow> } | null = null;
export function farGuardRow(g: Game, r: Region): FarGuardRow | null {
  const T = SIEGE.farGuard;
  if (!T || shoreOf(r) !== 2) return null;
  if (!farLands) {
    farLands = { whole: new Map(), by: new Map() };
    for (const row of T) { if (row.places) for (const nm of row.places) farLands.by.set(nm, row); else farLands.whole.set(row.owner, row); }
  }
  const of = (WORLD_MAP.regions[r.id - WORLD_MAP.first] as { of?: number } | undefined)?.of ?? r.id;
  const row = farLands.by.get(g.regions[of]?.name ?? '') ?? farLands.whole.get(r.owner);
  return row && row.owner === r.owner ? row : null;
}
/**
 * The share at which a province counts (ruling b466): 1 everywhere but on a country's own home ground on the far side
 * of the world, where budget.json's 'farHome' names a share for it. It is one factor on the whole of what the ground
 * pays (factories, land, harbour and an oilfield's money) and on the factories a staff counts its lines by, as the
 * colony's tenth is. The old chart's Soviet Union already carries the Soviet war economy, so her ground beyond it
 * would be counted twice. It goes with the ground, as ruling 214's land factor does: a conqueror's share is reckoned
 * on the same count. An oilfield's oil is another matter (resources.ts, ruling 194).
 */
export const farShare = (r: Region): number => (!r.overseas && shoreOf(r) === 2 ? BUDGET.farHome[r.owner] ?? 1 : 1);

/**
 * What a province gives the nation that holds it. Conquered ground and colonies give less than the home country.
 * `whole` leaves a siege out of account: what the province is worth to its holder once the roads are open again.
 * `bare` leaves out ruling 214's factor: trade reads a country's land as it was before the factor, so that no great
 * power's first day moves.
 */
export function provinceYield(g: Game, r: Region, nat: string = r.ctrl, whole = false, bare = false): Yield {
  const own = r.owner === nat;
  // A factory wrecked by bombing earns nothing until engineers rebuild it (the grand campaign only).
  // (A country's own home ground on the far side of the world counts at its share, every part of its pay: ruling b466.)
  const share = farShare(r);
  const works = workingFactories(g, r) * share;
  if (!budgeted(g)) { const fac = own ? works : works * PRODUCTION.occupiedIncome; return { land: 0, port: 0, oil: 0, fac, capital: 0, total: fac }; }
  // An administration set up on conquered ground makes it pay more of what it is worth.
  const lift = own ? 0 : hallShare(g, r);
  // A colony on the far side of the world keeps its purse: a tenth of what it pays reaches its own country (rider
  // b462 to ruling 231: India's purse paid for India's army, which comes by its rows). The colonies of the old chart
  // and of the Americas pay as they always have, and so does ground a conqueror has taken.
  // (An ally that keeps such a colony for its fallen country is no conqueror: the Indies in Britain's keeping after
  // May 1940 are no more a purse of London's than India is, and pay her a tenth of a keeper's share.)
  const far = farColony(r) && (own || allied(g, nat, r.owner)) ? BUDGET.farColony : null;
  const fac = (own ? works : works * (PRODUCTION.occupiedIncome + lift)) * (far?.fac ?? 1);
  const k = !own ? BUDGET.occupied + lift : colonyPurse(r) ? BUDGET.overseas : 1;
  // (One cut, not two: ruling 234(e)3. Where the far colony's tenth is taken, ruling 214's factor is not, and the
  // ground pays as any country's far colony does; the factor falls on the rest of that country's ground.)
  const worth = bare || !budgeted(g) || far ? 1 : landFactor(r);
  const land = BUDGET.land[r.terrain] * k * worth * landShare(r, bare || !!far) * (far?.land ?? 1) * share;
  // A harbour blocked pays nothing until it is mended (prizes.ts), and an oilfield alight pays only for the wells still at work.
  const port = r.port && (whole || prizeOut(g, r, 'harbour') <= 0) ? BUDGET.port * k * worth * (far?.port ?? 1) * share : 0;
  const oil = r.oil ? BUDGET.oil * k * (whole ? 1 : 1 - fieldOut(g, r)) * (far?.oil ?? 1) * share : 0;
  const capital = own && g.nations[nat]?.capital === r.id ? BUDGET.capital : 0;
  // A town damaged by bombs, shells or the fighting pays that much less from all that stands in it, until it is
  // repaired (citydamage.ts). Its oilfields lie outside it. (Trade's reading, `bare`, sees the town as it was built.)
  const dmg = bare ? 0 : damageOf(g, r);
  if (dmg > 0) return damaged(g, r, nat, whole, land, port, oil, fac, capital, dmg);
  // With an enemy army round its town a province sends its holder next to nothing: the roads out are cut.
  if (!whole && nat === r.ctrl && besieged(g, r)) {
    const p = SIEGE.pay;
    return { land: land * p, port: port * p, oil: oil * p, fac: fac * p, capital: capital * p, total: (land + port + oil + fac + capital) * p, siege: true };
  }
  return { land, port, oil, fac, capital, total: land + port + oil + fac + capital };
}

/** provinceYield's ending for a damaged town: kept apart so that a town standing whole is reckoned exactly as before. */
function damaged(g: Game, r: Region, nat: string, whole: boolean, land0: number, port0: number, oil: number, fac0: number, capital0: number, dmg: number): Yield {
  const keep = 1 - dmg;
  const land = land0 * keep, port = port0 * keep, fac = fac0 * keep, capital = capital0 * keep;
  if (!whole && nat === r.ctrl && besieged(g, r)) {
    const p = SIEGE.pay;
    return { land: land * p, port: port * p, oil: oil * p, fac: fac * p, capital: capital * p, total: (land + port + oil + fac + capital) * p, siege: true, damage: dmg };
  }
  return { land, port, oil, fac, capital, total: land + port + oil + fac + capital, damage: dmg };
}

export interface LedgerLine { key: string; label: string; amount: number; count?: number }
export interface Ledger {
  nat: string;
  /** Whether upkeep is charged and the treasury can run into debt (the grand campaign), or only income is counted. */
  budget: boolean;
  /** One production point is shown as this many in the interface. */
  scale: number;
  treasury: number;
  /** Where the money comes from, before the nation-wide gains and losses. */
  income: LedgerLine[];
  /** Nation-wide gains and losses on that income: peace, sinkings, bombing, production methods. Signed. */
  changes: LedgerLine[];
  /** What comes in each month, after those. */
  gross: number;
  /** What the armed forces cost each month, by arm. */
  upkeep: LedgerLine[];
  upkeepTotal: number;
  /** Income less upkeep: what is left each month for replacements and new orders. */
  net: number;
  /** 0 when the treasury is in credit, rising to 1 when the nation owes as much as it is allowed to. */
  debt: number;
  /** Every province held, richest first. */
  provinces: ({ id: number; name: string } & Yield)[];
}

const KIND_LABEL: Record<Kind, string> = { inf: 'Infantry', mtn: 'Mountain troops', cav: 'Cavalry', mot: 'Motorised troops', art: 'Artillery', eng: 'Engineers', arm: 'Armour', fort: 'Fortress troops', mg: 'Machine-gun brigades', mor: 'Mortar brigades' };
const SHIP_LABEL: Record<ShipClass, string> = { bb: 'Battleships', cv: 'Carriers', ca: 'Cruisers', dd: 'Destroyers', ss: 'Submarines' };

/** What it costs each month to keep one corps of this kind and size in being. */
export const corpsUpkeep = (kind: Kind, divs: number, str = 1): number => (BUDGET.upkeep.corps[kind] ?? BUDGET.upkeep.corps.inf) * divs * str;

/** What the armed forces of a nation cost each month, by arm. Nothing in a historical scenario. */
export function upkeep(g: Game, nat: string): LedgerLine[] {
  if (!budgeted(g)) return [];
  const n = g.nations[nat];
  const peace = n.atWar.length ? 1 : BUDGET.upkeep.peace;
  const out: LedgerLine[] = [];
  const land: Partial<Record<Kind, { divs: number; cost: number }>> = {};
  // (Corps at sea on a long passage are paid for as well.)
  for (const c of g.atSea?.length ? [...g.corps, ...g.atSea.map((v) => v.corps)] : g.corps) {
    if (c.nat !== nat || c.dead) continue;
    const row = (land[c.kind] ??= { divs: 0, cost: 0 });
    row.divs += c.divs;
    row.cost += corpsUpkeep(c.kind, c.divs, c.str);
  }
  for (const kind of Object.keys(KIND_LABEL) as Kind[]) {
    const row = land[kind];
    if (row) out.push({ key: `corps:${kind}`, label: KIND_LABEL[kind], amount: row.cost * peace, count: row.divs });
  }
  const hulls: Partial<Record<ShipClass, number>> = {};
  for (const f of g.fleets) if (f.nat === nat) for (const cls of Object.keys(SHIP_LABEL) as ShipClass[]) hulls[cls] = (hulls[cls] ?? 0) + (f.ships[cls] ?? 0);
  for (const cls of Object.keys(SHIP_LABEL) as ShipClass[]) {
    const have = hulls[cls] ?? 0;
    if (have > 0) out.push({ key: `ships:${cls}`, label: SHIP_LABEL[cls], amount: have * BUDGET.upkeep.ships[cls] * peace, count: have });
  }
  let ftr = 0, bmr = 0;
  for (const f of g.airFleets) if (f.nat === nat) { ftr += f.ftr; bmr += f.bmr; }
  if (ftr > 0) out.push({ key: 'air:ftr', label: 'Fighters', amount: ftr * BUDGET.upkeep.air.ftr * peace, count: ftr });
  if (bmr > 0) out.push({ key: 'air:bmr', label: 'Bombers', amount: bmr * BUDGET.upkeep.air.bmr * peace, count: bmr });
  // Guns and airfields are manned whether the nation is at war or not.
  out.push(...worksUpkeep(g, nat));
  // What it pays neutrals for their goods this month (release 2).
  const bought = bargainCost(g, nat);
  if (bought > 0) out.push({ key: 'bargains', label: 'Paid to neutrals for their goods', amount: bought });
  // What the atomic project drew this month, its plants or the making of more bombs (release 2, atom.ts).
  const atom = atomBill(g, nat);
  if (atom > 0) out.push({ key: 'atom', label: 'The atomic plants', amount: atom });
  return out;
}

/** A nation's books for the month: where the money comes from, what the forces cost, and what is left. */
export function ledger(g: Game, nat: string): Ledger {
  const n = g.nations[nat];
  const budget = budgeted(g);
  const provinces: Ledger['provinces'] = [];
  let facHome = 0, facOcc = 0, nHome = 0, nOcc = 0, land = 0, ports = 0, oil = 0, capital = 0, held = 0, nPorts = 0, nOil = 0;
  for (const r of g.regions) {
    if (r.ctrl !== nat) continue;
    const y = provinceYield(g, r, nat);
    provinces.push({ id: r.id, name: r.name, ...y });
    held++;
    if (r.owner === nat) { facHome += y.fac; nHome += workingFactories(g, r); } else { facOcc += y.fac; nOcc += workingFactories(g, r); }
    land += y.land; ports += y.port; oil += y.oil; capital += y.capital;
    if (y.port) nPorts++;
    if (y.oil) nOil++;
  }
  provinces.sort((a, b) => b.total - a.total || a.id - b.id);
  const away = n.offmap?.industry ?? 0;
  // A government whose homeland is off the map sits there, out of reach.
  const seat = budget ? (n.offmap ? BUDGET.capital : capital) : PRODUCTION.baseIncome;
  // In the grand campaign trade is by agreement and the towns' buildings pay their own way.
  const partners = budget ? dealsOf(g, nat).length : tradePartners(g, nat).length;
  const trade = budget ? tradeIncome(g, nat) : partners * DIPLOMACY.tradeIncome;
  const towns = budget ? civicSum(g, nat, keepsStores(g, nat)) : null;
  const civic = towns ? towns.commerce + towns.farms + towns.tax : 0;
  // A neutral's bargains (release 2): its sales, and the goods bought by a country that keeps no stocks.
  const sales = budget ? bargainSales(g, nat) : 0, goodsBought = budget ? bargainGoodsMoney(g, nat) : 0;
  const peace = footShare(g, nat, n.atWar.length ? warFooting(g, nat) ?? 1 : peaceFooting(g, nat) ?? PRODUCTION.peaceIncome);
  const sea = seaIncomeLoss(g, nat), air = airIncomeLoss(g, nat), tech = techMul(g, nat, 'industry');
  // What its armies hold outside towns they have not yet taken: factory districts, oilfields and quays.
  const prizes = prizeIncome(g, nat);
  const before = seat + facHome + facOcc + away + land + ports + oil + prizes.total;
  // What the nation's own abilities earn it (the grand campaign): banks, ore, a merchant fleet, factories moved out of reach.
  const gift = traitMoney(g, nat);
  const atPeace = before * peace + trade + gift.flat + civic + (sales + goodsBought);
  const afterSea = atPeace * (1 - sea), afterAir = afterSea * (1 - air);
  const hard = budget ? incomeMul(g, nat) : 1;
  const fed = budget ? supIncome(g, nat) : 1;
  // The player's ministers and decrees (the grand campaign).
  const cabinet = budget ? govMul(g, nat, 'income') : 1;
  const gross = budget ? afterAir * tech * hard * gift.mul * fed * cabinet : income(g, nat);
  const lines: LedgerLine[] = [
    { key: 'capital', label: budget ? 'The capital and the state' : 'The state', amount: seat },
    { key: 'factories', label: 'Factories at home', amount: facHome, count: nHome },
    { key: 'occupied', label: 'Factories on occupied ground', amount: facOcc, count: nOcc },
    { key: 'homeland', label: 'Industry in the homeland, beyond the map', amount: away, count: away },
    { key: 'land', label: 'Towns, farms and land', amount: land, count: held },
    { key: 'ports', label: 'Harbours', amount: ports, count: nPorts },
    { key: 'oil', label: 'Oilfields', amount: oil, count: nOil },
    ...(prizeRules(g) ? [{ key: 'prizes', label: 'Prizes held outside towns under siege', amount: prizes.total, count: prizes.count }] : []),
    { key: 'trade', label: budget ? 'Trade agreements' : 'Trade with friendly neutrals', amount: trade, count: partners },
    { key: 'commerce', label: 'Commerce in the towns', amount: towns?.commerce ?? 0 },
    { key: 'farms', label: 'Farm produce sold', amount: towns?.farms ?? 0 },
    { key: 'tax', label: 'Taxes the administration gathers', amount: towns?.tax ?? 0 },
    ...(bargainRules(g) ? [{ key: 'bargains', label: 'Goods sold to countries at war', amount: sales }, { key: 'bought', label: 'Goods bought from neutrals', amount: goodsBought }] : []),
    ...gift.lines,
  ];
  const changes: LedgerLine[] = [
    // (At war, a country that climbs to full war work by steps says how far it has come and how long it has to go.)
    // (With the nation's footing switched on the line names the footing, and it never drops out: footing.ts, rider 2 of ruling 172.)
    { key: 'peace', label: footLabel(g, nat, peace, n.atWar.length ? (peace >= 1 ? 'At war: all of industry on war work' : `At war: ${Math.round(peace * 100)} in 100 of industry on war work so far, all of it in ${warWorkLeft(g, nat) === 1 ? 'a month' : `${warWorkLeft(g, nat)} months`}`) : peaceFooting(g, nat) === null ? 'At peace: industry at half speed' : `At peace: ${Math.round(peace * 100)} in 100 of industry on war work`), amount: before * (peace - 1) },
    { key: 'sea', label: 'Cargoes sunk at sea', amount: afterSea - atPeace },
    { key: 'air', label: 'Bombing of industry', amount: afterAir - afterSea },
    { key: 'tech', label: tech >= 1 ? 'Better production methods' : 'Production methods behind the times', amount: afterAir * (tech - 1) },
    { key: 'level', label: `Difficulty: ${levelOf(g).name}`, amount: afterAir * tech * (hard - 1) },
    { key: 'trait', label: gift.mulBy, amount: afterAir * tech * hard * (gift.mul - 1) },
    { key: 'supplies', label: 'Factories idle for want of supplies', amount: afterAir * tech * hard * gift.mul * (fed - 1) },
    { key: 'cabinet', label: 'The war cabinet and its decrees', amount: afterAir * tech * hard * gift.mul * fed * (cabinet - 1) },
  ];
  const costs = upkeep(g, nat);
  const upkeepTotal = costs.reduce((s, l) => s + l.amount, 0);
  const floor = BUDGET.debt.floor * Math.max(1, gross);
  return {
    nat, budget, scale: BUDGET.scale, treasury: n.pp,
    income: lines.filter((l) => l.amount > 1e-9), changes: changes.filter((l) => Math.abs(l.amount) > 1e-9 || footLineStays(g, l.key)),
    gross, upkeep: costs, upkeepTotal, net: gross - upkeepTotal,
    debt: budget && n.pp < 0 ? Math.min(1, -n.pp / floor) : 0,
    provinces,
  };
}

/** Production points a nation earns this month, before its forces are paid for. */
export function income(g: Game, nat: string): number {
  const n = g.nations[nat];
  if (budgeted(g)) {
    let sum = 0, seat = n.offmap ? BUDGET.capital : 0;
    for (const r of g.regions) {
      if (r.ctrl !== nat) continue;
      const y = provinceYield(g, r, nat);
      sum += y.land + y.port + y.oil + y.fac;
      seat += y.capital;
    }
    sum += seat + (n.offmap?.industry ?? 0) + prizeIncome(g, nat).total;
    // Trade is by agreement, and the towns' buildings pay their own way.
    const towns = civicSum(g, nat, keepsStores(g, nat));
    // (And a neutral's bargains, release 2: what it sells, and the goods a country that keeps no stocks buys.)
    const trade = tradeIncome(g, nat) + towns.commerce + towns.farms + towns.tax + bargainMoney(g, nat);
    // The level of the campaign adds to the player's earnings, or to his rivals': at the fair level, nothing.
    const gift = traitMoney(g, nat);
    // Factories short of steel, coal and rubber stand idle part of the month.
    return (sum * footShare(g, nat, n.atWar.length ? warFooting(g, nat) ?? 1 : peaceFooting(g, nat) ?? PRODUCTION.peaceIncome) + trade + gift.flat) * (1 - seaIncomeLoss(g, nat)) * (1 - airIncomeLoss(g, nat)) * techMul(g, nat, 'industry') * incomeMul(g, nat) * gift.mul * supIncome(g, nat) * govMul(g, nat, 'income');
  }
  let fac = 0;
  for (const r of g.regions) if (r.ctrl === nat) fac += r.owner === nat ? r.fac : r.fac * PRODUCTION.occupiedIncome;
  // A country whose homeland is off the map has its factories there, out of any enemy's reach.
  fac += n.offmap?.industry ?? 0;
  // Neutrals that lean toward a bloc trade with its major powers.
  const trade = tradePartners(g, nat).length * DIPLOMACY.tradeIncome;
  // What comes by sea can be sunk on the way: cargoes on the convoy routes, ore from the north.
  // And the factories turn out more, or less, as the nation's production methods run ahead of or behind their time.
  return ((PRODUCTION.baseIncome + fac) * (n.atWar.length ? 1 : PRODUCTION.peaceIncome) + trade) * (1 - seaIncomeLoss(g, nat)) * (1 - airIncomeLoss(g, nat)) * techMul(g, nat, 'industry');
}

/** What the forces cost this month, in all. */
export const upkeepTotal = (g: Game, nat: string): number => upkeep(g, nat).reduce((s, l) => s + l.amount, 0);

/** Last month in which this nation still has pre-war mobilisation arriving. */
function lastScheduled(nat: string): number {
  let t = 0;
  for (const row of (setup as any).reinforcements) if (row[1] === nat) t = Math.max(t, row[0]);
  return t;
}

/** A computer-run nation starts ordering new formations once it is at war and its mobilisation has run its course. */
export function canBuildYet(g: Game, nat: string): boolean {
  return g.nations[nat].atWar.length > 0 && g.turn > lastScheduled(nat);
}

const ROMAN: [number, string][] = [[100, 'C'], [90, 'XC'], [50, 'L'], [40, 'XL'], [10, 'X'], [9, 'IX'], [5, 'V'], [4, 'IV'], [1, 'I']];
function roman(n: number): string {
  let out = '';
  for (const [v, s] of ROMAN) while (n >= v) { out += s; n -= v; }
  return out;
}

/** A new corps' name. (A name is a name: "XIV Motorized Corps" keeps the spelling the order of battle uses, though the labels say "Motorised".) */
export function corpsName(nat: string, kind: Kind, no: number): string {
  const arm = PRODUCTION.armourName[nat] ?? PRODUCTION.armourName._default;
  const word = kind === 'arm' ? `${arm} ` : kind === 'mot' ? 'Motorized ' : kind === 'mtn' ? 'Mountain ' : kind === 'art' ? 'Artillery ' : kind === 'eng' ? 'Engineer ' : '';
  return `${roman(no)} ${word}Corps`;
}

export const isHome = (g: Game, nat: string, r: Region): boolean => r.owner === nat && r.ctrl === nat;

/** Where new formations can be raised: at home, not in the colonies. Troops for overseas are raised at home and shipped. */
export const canRaise = (g: Game, nat: string, r: Region): boolean => isHome(g, nat, r) && !r.overseas && !besieged(g, r);

/**
 * Orders a new corps. The points are paid now; the corps arrives in a few months. 'revive' names a destroyed
 * formation on the memorial wall: the new corps is raised under its name and carries its colours.
 */
export function orderCorps(g: Game, nat: string, kind: Kind, region: number, revive?: number): Build | null {
  const n = g.nations[nat];
  const spec = specOf(g, kind);
  const r = g.regions[region];
  const cost = corpsCost(g, nat, kind);
  if (!n.alive || !spec || !r || !canRaiseKind(g, nat, r, kind) || n.pp < cost) return null;
  // In the grand campaign tanks and lorried infantry wait on the technology that makes them.
  if (locked(g, nat, `corps:${kind}`)) return null;
  // And every new corps is filled from the reserve of men: with none left, none is raised.
  if (raisable(g, nat, spec.divs) < 1) return null;
  if (!canArm(g, nat, spec.divs)) return null;
  const old = revive !== undefined ? revivable(g, nat, kind).find((f) => f.id === revive) : undefined;
  if (revive !== undefined && !old) return null;
  n.pp -= cost;
  drawMen(g, nat, spec.divs);
  const rifles = armOrder(g, nat, spec.divs);
  // A support team is numbered apart from the corps (release 2), so no corps' number, nor the computer's tank cadence, moves.
  const name = old ? old.name : isTeam({ kind }) ? teamName(g, nat, kind as TeamKind) : corpsName(nat, kind, n.corpsNo++);
  const b: Build = { id: g.nextBuild++, what: 'corps', kind, divs: spec.divs, name, region, ordered: g.turn, eta: g.turn + spec.months, cost };
  if (old) b.revive = old.id;
  if (rifles > 0) b.rifles = rifles;
  n.queue.push(b);
  return b;
}

/**
 * Orders one of the nation's special formations (the grand campaign): the Guards, the Alpine corps, the heavy tanks.
 * It is raised like any corps of its kind and counts against the number the nation can keep at once.
 */
export function orderElite(g: Game, nat: string, region: number): Build | null {
  const n = g.nations[nat];
  const u = eliteOf(nat);
  const r = g.regions[region];
  const cost = u ? carved(g, nat, u.cost, u.divs) : 0;
  if (!n.alive || !u || whyNoElite(g, nat) !== null || locked(g, nat, `corps:${u.kind}`) || !r || !canRaiseKind(g, nat, r, u.kind) || n.pp < cost || raisable(g, nat, u.divs) < 1) return null;
  if (!canArm(g, nat, u.divs)) return null;
  n.pp -= cost;
  drawMen(g, nat, u.divs);
  const rifles = armOrder(g, nat, u.divs);
  const no = (g.flags[`elite:${nat}`] ?? 0) + 1;
  g.flags[`elite:${nat}`] = no;
  const b: Build = { id: g.nextBuild++, what: 'corps', kind: u.kind, divs: u.divs, name: `${roman(no)} ${u.word} Corps`, region, ordered: g.turn, eta: g.turn + u.months, cost, elite: u.id };
  if (rifles > 0) b.rifles = rifles;
  n.queue.push(b);
  return b;
}

/**
 * Orders an airborne corps (the grand campaign), once the technology tree has given the nation airborne forces.
 * It is raised like infantry and goes into action by air.
 */
export function orderPara(g: Game, nat: string, region: number): Build | null {
  const n = g.nations[nat];
  const u = TECH.units.para;
  const r = g.regions[region];
  const cost = carved(g, nat, priceOf(g, nat, 'para', u.cost), u.divs);
  if (!n.alive || whyNoPara(g, nat) !== null || !r || !canRaiseKind(g, nat, r, 'inf') || n.pp < cost || raisable(g, nat, u.divs) < 1) return null;
  if (!canArm(g, nat, u.divs)) return null;
  n.pp -= cost;
  drawMen(g, nat, u.divs);
  const rifles = armOrder(g, nat, u.divs);
  const no = (g.flags[`para:${nat}`] ?? 0) + 1;
  g.flags[`para:${nat}`] = no;
  const b: Build = { id: g.nextBuild++, what: 'corps', kind: 'inf', divs: u.divs, name: `${roman(no)} ${u.word} Corps`, region, ordered: g.turn, eta: g.turn + u.months, cost, airborne: true };
  if (rifles > 0) b.rifles = rifles;
  n.queue.push(b);
  return b;
}

// ---- Cadres (the grand campaign) -------------------------------------------------------------------
// A corps whose men were folded into another (fold.ts) is kept off the map as a cadre: its staff, its name, its
// record and its honours. It can be raised again for a share of the price of a new corps, with a new corps' men.

/** The cadres a nation keeps, newest first. */
export const cadresOf = (g: Game, nat: string): Corps[] => [...(g.nations[nat]?.cadres ?? [])].reverse();

/** What it takes to raise a cadre again as a full corps: the price, the months and the divisions' worth of men. */
export function cadreTerms(g: Game, nat: string, c: Corps): { cost: number; months: number; divs: number } {
  const u = c.elite ? eliteOf(nat) : null, p = TECH.units.para;
  const kind = specOf(g, c.kind) ? c.kind : 'inf';
  // (The elite's and the airborne's list prices are carved as their orders are; every other kind's through corpsCost.)
  const list = u ? carved(g, nat, u.cost, u.divs) : c.airborne ? carved(g, nat, priceOf(g, nat, 'para', p.cost), p.divs) : corpsCost(g, nat, kind);
  // (A corps its row keeps small comes back at its own establishment, for that share of the price and of the men:
  // rider b594 to ruling 234. No corps has one with the far side off.)
  const size = c.full ?? 1;
  return { cost: Math.round(list * MANPOWER.fold.price * size * 10) / 10, months: u ? u.months : c.airborne ? p.months : specOf(g, kind)!.months, divs: c.divs * size };
}

/** Why a cadre cannot be raised again now, apart from money and a place to raise it; null when it can. */
export function whyNoCadre(g: Game, nat: string, c: Corps): string | null {
  if (g.nations[nat].queue.some((b) => b.cadre === c.id)) return 'It is being raised again already.';
  if (c.elite) { const no = whyNoElite(g, nat); if (no) return no; }
  if (c.airborne) { const no = whyNoPara(g, nat); if (no) return no; }
  return null;
}

/** Orders a cadre raised again as a full corps. The price is paid now and the men are drawn now, as for a new corps. */
export function orderCadre(g: Game, nat: string, id: number, region: number): Build | null {
  const n = g.nations[nat];
  const c = n.cadres?.find((x) => x.id === id);
  const r = g.regions[region];
  if (!n.alive || !c || !r || !canRaiseKind(g, nat, r, c.kind) || whyNoCadre(g, nat, c) !== null) return null;
  const t = cadreTerms(g, nat, c);
  if (n.pp < t.cost || raisable(g, nat, t.divs) < 1) return null;
  // (A cadre comes back with new men, so it draws a new corps' rifles whole, and its old armed level is struck.)
  if (!canArm(g, nat, t.divs)) return null;
  n.pp -= t.cost;
  drawMen(g, nat, t.divs);
  const rifles = armOrder(g, nat, t.divs);
  riflesRaised(g, c.id);
  const b: Build = { id: g.nextBuild++, what: 'corps', kind: c.kind, divs: c.divs, name: c.name, region, ordered: g.turn, eta: g.turn + t.months, cost: t.cost, cadre: c.id };
  if (c.elite) b.elite = c.elite;
  if (c.airborne) b.airborne = true;
  if (rifles > 0) b.rifles = rifles;
  n.queue.push(b);
  return b;
}

/** A cadre takes the field again: the same corps, with its number, its diary and its ribbons, filled with new men. */
function reform(g: Game, c: Corps, at: Region): void {
  c.dead = false; c.str = c.full ?? 1; c.org = 0.7; c.ap = c.apMax; c.fought = false; c.supplied = true; c.general = null; c.reg = at.id; c.hold = false; c.dyle = null;
  c.xp = Math.max((c.nat === g.human ? raisedXp(g, c.nat) : 0.05) + campXp(g, at.id), c.xp * MANPOWER.fold.xp);
  for (const k of ['dug', 'dugFor', 'digging', 'pressed', 'blows', 'relief', 'yields', 'gaveWay', 'digs', 'joint', 'leave', 'town', 'job', 'rival'] as const) delete c[k];
  // (It took part in no battle the record holds: the contact rule's fight half, drawn.ts.)
  dropBattle(g, c.id);
  raiseMorale(g, c);
  // (A division of its name that a dated row has laid while it was off the map is that row's now: drawn.ts.)
  reformNames(g, c);
  g.corps.push(c);
  at.units.push(c);
  noteCorps(g, c, 'rebuilt', at.id);
}

/** What the next level of fortification would cost in a region, or null if it cannot be fortified further. */
export function fortCost(g: Game, nat: string, r: Region): number | null {
  // Home ground, or a conquered town that has settled (release 2, buildings.ts settledFor).
  if (!isHome(g, nat, r) && !settledFor(g, nat, r)) return null;
  const queued = g.nations[nat].queue.filter((b) => b.what === 'fort' && b.region === r.id).length;
  const next = (r.fort?.lv ?? 0) + queued + 1;
  return next > 3 ? null : priceOf(g, nat, 'work:fort', PRODUCTION.fort.cost[next]);
}

export function orderFort(g: Game, nat: string, region: number): Build | null {
  const n = g.nations[nat];
  const r = g.regions[region];
  const cost = r ? fortCost(g, nat, r) : null;
  if (!n.alive || cost === null || n.pp < cost) return null;
  n.pp -= cost;
  const b: Build = { id: g.nextBuild++, what: 'fort', name: `Fortifications at ${r.name}`, region, ordered: g.turn, eta: g.turn + PRODUCTION.fort.months, cost };
  // Ordered in a conquered town that has settled (release 2): finished while the town is held.
  if (r.owner !== nat) b.conq = true;
  n.queue.push(b);
  return b;
}

/** An order can be taken back for a full refund in the month it was placed. */
export function cancelBuild(g: Game, nat: string, id: number): boolean {
  const n = g.nations[nat];
  const i = n.queue.findIndex((b) => b.id === id);
  if (i < 0 || n.queue[i].ordered !== g.turn) return false;
  n.pp += n.queue[i].cost;
  // The men called up for a corps that was paid for, or that drew its rifles from the store, go back to the reserve,
  // and so do the rifles. (A cadre kept small drew only its own establishment's share, cadreTerms: that share goes back.
  // No corps has one with the far side off.)
  const kept = n.queue[i].cadre !== undefined ? n.cadres?.find((x) => x.id === n.queue[i].cadre)?.full : undefined;
  if (n.queue[i].what === 'corps' && (n.queue[i].cost > 0 || n.queue[i].rifles)) returnMen(g, nat, (n.queue[i].divs ?? 0) * (kept ?? 1));
  unarmOrder(g, nat, n.queue[i]);
  if (n.queue[i].cadre !== undefined) { /* the cadre keeps its own name and number: nothing to hand back */ }
  else if (n.queue[i].elite) g.flags[`elite:${nat}`] = (g.flags[`elite:${nat}`] ?? 1) - 1;
  else if (n.queue[i].airborne) g.flags[`para:${nat}`] = (g.flags[`para:${nat}`] ?? 1) - 1;
  // A support team gives its own number back (release 2, src/campaign/teams.ts).
  else if (n.queue[i].what === 'corps' && n.queue[i].revive === undefined && isTeam({ kind: n.queue[i].kind ?? '' })) g.flags[`team:${n.queue[i].kind}:${nat}`] = (g.flags[`team:${n.queue[i].kind}:${nat}`] ?? 1) - 1;
  else if (n.queue[i].what === 'corps' && n.queue[i].revive === undefined) n.corpsNo--;
  n.queue.splice(i, 1);
  return true;
}

/** Hands over everything on order that is due this month. */
export function deliver(g: Game, nat: string): void {
  const n = g.nations[nat];
  if (!n.queue.length) return;
  const due = n.queue.filter((b) => b.eta <= g.turn);
  if (!due.length) return;
  n.queue = n.queue.filter((b) => b.eta > g.turn);
  // The month's aircraft from the factories are told in one line, not one a wing (the third playtester, 84.3.1).
  const air: AirDelivery[] = [];
  for (const b of due) {
    const r = g.regions[b.region];
    if (b.what === 'air' || b.what === 'ship' || b.what === 'work') { deliverArsenal(g, nat, b, air); continue; }
    if (b.what === 'civic') { const news = deliverCivic(g, nat, b); if (news && nat === g.human) headline(g, 'info', news, nat, r.id); continue; }
    if (b.what === 'fort') {
      // The ground was lost before the works were finished. (A conquered town its builder still holds keeps them, release 2.)
      if (!isHome(g, nat, r) && !((settledTownsOn(g) || b.conq) && r.ctrl === nat)) continue;
      const vs = new Set(r.fort?.vs ?? []);
      vs.add(r.owner);
      for (const j of r.nb) vs.add(g.regions[j].owner);
      r.fort = { lv: Math.min(3, (r.fort?.lv ?? 0) + 1), vs: [...vs], by: nat };
      if (nat === g.human) headline(g, 'info', `The new fortifications at ${r.name} are finished.`, nat, r.id);
      continue;
    }
    // (On an island of the Pacific the new corps itself must fit in men, at the full strength it forms at: the island
    // refuses only new corps, the design director, 21:37Z, 26 November 2026, 4. Other ground: a place among the five.)
    const room = (x: Region): boolean => canRaise(g, nat, x) && (islandOf(g, x)
      ? fits(g, x, nat, [{ nat, kind: b.kind!, divs: b.divs!, str: 1 } as Corps])
      : roomForDivision(g, x, nat, x.units.filter((u) => allied(g, u.nat, nat) && !roomless(g, u)).length));
    let at: Region | null = room(r) ? r : null;
    if (!at && n.capital >= 0 && room(g.regions[n.capital])) at = g.regions[n.capital];
    if (!at) at = g.regions.find((x) => room(x) && x.fac > 0) ?? g.regions.find(room) ?? null;
    if (!at) continue; // nowhere left to form up
    if (b.cadre !== undefined) {
      const i = (n.cadres ?? []).findIndex((x) => x.id === b.cadre);
      if (i < 0) continue;
      const c = n.cadres!.splice(i, 1)[0];
      reform(g, c, at);
      if (nat === g.human) headline(g, 'info', `${c.name} has been raised again round its cadre at ${at.name}, and is ready for orders.`, nat, at.id, { ready: 'corps', unit: c.id });
      continue;
    }
    // The player's new corps are as well trained as the instructors he has sent home can make them.
    const c = addCorps(g, nat, b.name, b.kind!, b.divs!, at.id, { xp: (nat === g.human ? raisedXp(g, nat) : FRESH.xp) + campXp(g, at.id), airborne: b.airborne });
    c.org = FRESH.org;
    if (b.elite) c.elite = b.elite;
    const old = b.revive !== undefined ? fallenOf(g, b.revive) : undefined;
    if (old && old.heir === undefined) inherit(g, c, old.id);
    if (nat === g.human) headline(g, 'info', old ? `${b.name} is raised again at ${at.name}, ${g.turn - old.turn} months after it was lost at ${g.regions[old.reg].name}. It carries the old colours and is ready for orders.` : `${b.name} has finished forming at ${at.name} and is ready for orders.`, nat, at.id, { ready: 'corps', unit: c.id });
  }
  const line = airLine(air);
  if (line) headline(g, 'info', line.text, nat, line.region, { ready: 'air' });
}

/**
 * The grand campaign: true for a nation at war whose field army is under 'fit' (arsenal.json, 'ai.guns') of the
 * largest it has been: an army that is being worn away. Its staff raises men for the line before guns, and its
 * builders wait (Version 69).
 */
export function armyWorn(g: Game, nat: string): boolean {
  const n = g.nations[nat];
  const fit = (arsenalData.ai as unknown as { guns: { fit?: number } }).guns.fit;
  return g.flags.open === 1 && !!fit && !!n?.alive && n.atWar.length > 0 && n.armyPeak > 0 && armySize(g, nat) < fit * n.armyPeak;
}

/**
 * The grand campaign: true while a computer staff whose line has met dug-in troops wants massed guns to break them,
 * one corps of guns to every few of the rest, and has not got them yet. An army that is being worn away wants men
 * in the line before guns (Version 69): none are asked for while the field army is under 'fit' of the largest it has been.
 */
export function gunsWanted(g: Game, nat: string): boolean {
  const n = g.nations[nat];
  if (g.flags.open !== 1 || !n?.alive || !n.atWar.length) return false;
  const G = (arsenalData.ai as unknown as { guns: { per: number; most: number; facing: number } }).guns;
  if (armyWorn(g, nat)) return false;
  let guns = n.queue.filter((b) => b.what === 'corps' && b.kind === 'art').length, rest = 0;
  for (const c of g.corps) if (c.nat === nat && !c.dead) { if (c.kind === 'art') guns++; else if (c.kind !== 'fort' && c.kind !== 'eng' && !isTeam(c)) rest++; }
  if (guns >= G.most || guns * G.per >= rest) return false;
  let dug = 0;
  for (const c of g.corps) {
    if (c.dead || !c.dug || !n.atWar.includes(c.nat) || !g.regions[c.reg].nb.some((j) => g.regions[j].ctrl === nat)) continue;
    if (++dug >= G.facing) return true;
  }
  return false;
}

/**
 * The factories a computer staff counts its lines by: those of the home country alone. A colony's never add a line
 * (rider b462 to ruling 231: with the far side on, India's fourteen and three more gave Britain five lines for three,
 * and India's army comes by its rows). Korea's, Formosa's and Karafuto's count with Japan's own (budget.json), and
 * the Soviet Union's beyond the old chart at their share (ruling b466, 'farHome').
 */
export function lineFactories(g: Game, nat: string): number {
  let home = 0;
  for (const r of g.regions) if (isHome(g, nat, r) && !colonyPurse(r)) home += r.fac * farShare(r);
  return home;
}

/**
 * The build pick: the kind of corps the computer's factory policy raises next, and where. The grand campaign: a staff
 * whose line has met dug-in troops orders massed guns to break them, one corps of guns to every few of the rest, and
 * brings them up where the works are; a great industry raises every third corps as tanks; else infantry.
 */
export function buildPick(g: Game, nat: string, home: number, capital: number): { kind: Kind; at: number } {
  let kind: Kind = gunsWanted(g, nat) ? 'art' : home >= 20 && g.nations[nat].corpsNo % 3 === 2 ? 'arm' : 'inf';
  // In the grand campaign tanks are built where the factories are: at the capital if it has them, else in the
  // nation's greatest industrial province; a nation with no such province raises infantry instead.
  let at = budgeted(g) ? raiseSite(g, nat, kind, capital) : capital;
  // A nation that has not researched the tank arm raises infantry instead, and so does one with no fuel to run more tanks.
  if (at < 0 || locked(g, nat, `corps:${kind}`) || (kind === 'arm' && shortOfOil(g, nat))) { kind = 'inf'; at = capital; }
  return { kind, at };
}

/** The computer's factory policy: keep a reserve for replacements, spend the rest on new corps. */
export function aiProduction(g: Game, nat: string): void {
  const n = g.nations[nat];
  // The player places his own orders, unless he has handed building to his staff (depth.ts).
  // (His staff does not wait for history's mobilisation to run its course, as a computer nation does: the player's
  // treasury is not skimmed while it waits, so the money is there to be spent, in peace as in war.)
  if (byHand(g, nat, 'build') || !n.alive || n.capital < 0 || (nat !== g.human && !canBuildYet(g, nat))) return;
  const cap = g.regions[n.capital];
  if (!isHome(g, nat, cap)) return;
  const home = lineFactories(g, nat);
  // In the grand campaign a staff orders nothing its treasury could not keep in the field afterwards.
  const keep = (b: Build): number => (b.what === 'corps' ? corpsUpkeep(b.kind!, b.divs!) : 0);
  let spare = budgeted(g) ? income(g, nat) - upkeepTotal(g, nat) - n.queue.reduce((t, b) => t + keep(b), 0) : Infinity;
  // Works on order (the grand campaign's guns and airfields) do not hold up the raising of troops.
  // Engineers are ordered by their own rule (the grand campaign) and do not take a place in the army's programme.
  // (Nor do the fortifications a computer staff orders with idle money, arsenal.ts: a player's own take a place, as they always did.)
  // Nor do the support teams (release 2): they have their own rule, aiTeams.
  const slots = (): number => n.queue.filter((b) => (b.what === 'corps' && b.kind !== 'eng' && !isTeam({ kind: b.kind ?? '' })) || (b.what === 'fort' && (nat === g.human || g.flags.open !== 1))).length;
  // How many corps a staff has forming at once. Two, as history's programmes ran; in the grand campaign a great
  // industry keeps more in hand, so that a rich country at war turns its money into an army and does not sit on it.
  const most = g.flags.open === 1 && PRODUCTION.aiLine ? Math.max(2, Math.floor(home / PRODUCTION.aiLine)) : 2;
  // The nation's special formation comes first when it can be had (the grand campaign): one on order at a time.
  const u = budgeted(g) && !armyIdle(g, nat) ? eliteOf(nat) : null;
  if (u && slots() < 2 && !n.queue.some((b) => b.elite) && whyNoElite(g, nat) === null && !locked(g, nat, `corps:${u.kind}`) && !(u.kind === 'arm' && shortOfOil(g, nat))) {
    const at = raiseSite(g, nat, u.kind, cap.id), cost = corpsUpkeep(u.kind, u.divs);
    // A staff never holds more than its bank allows, so a costly formation is ordered as soon as the bank is full.
    const need = Math.min(carved(g, nat, u.cost, u.divs) + PRODUCTION.aiReserve, PRODUCTION.aiBank[1]);
    if (at >= 0 && n.pp >= need && spare >= cost * BUDGET.debt.aiMargin && menSpare(g, nat) >= u.divs && orderElite(g, nat, at)) spare -= cost;
  }
  // The bank: the price and the reserve kept back. Where the nation's store of rifles binds, a kind the carve makes
  // free passes, read from the price the order pays (corpsCost), so a staff is not held by a bank for a corps that
  // costs nothing (the design director's answer 6 of 25 November 2026, 19:08Z); any carved price left takes the test.
  // The men: the reserve beyond what the corps in the field are short of, which for a binding nation counts only up to
  // each corps' armed level (manpower.ts menSpare). The rifles' own test is the order's (canArm).
  const bank = (kind: string, price: number): boolean => (binds(g, nat) && corpsCost(g, nat, kind) === 0) || n.pp >= price + PRODUCTION.aiReserve;
  const men = (_kind: string, divs: number): boolean => menSpare(g, nat) >= divs;
  while (slots() < most) {
    // The wider world: a staff an ocean away from every enemy raises no more corps than it has a use for at home.
    if (armyIdle(g, nat)) break;
    // A cadre waiting at home is raised again before anything new is formed (the grand campaign): the same corps, for a share of the price.
    // (So many at a time and no more: a staff does not rebuild its whole army at the cadre's price in one season.)
    const old = g.flags.open === 1 && n.queue.filter((b) => b.cadre !== undefined).length < rivalsData.fold.cadres ? (n.cadres ?? []).find((c) => !c.elite && !c.airborne && whyNoCadre(g, nat, c) === null) : undefined;
    if (old) {
      const t = cadreTerms(g, nat, old), up = corpsUpkeep(old.kind, old.divs);
      const site = budgeted(g) ? raiseSite(g, nat, old.kind, cap.id) : cap.id;
      if (bank(old.kind, t.cost) && spare >= up * BUDGET.debt.aiMargin && men(old.kind, t.divs) && orderCadre(g, nat, old.id, site >= 0 ? site : cap.id)) { spare -= up; continue; }
    }
    const { kind, at } = buildPick(g, nat, home, cap.id);
    if (!bank(kind, corpsCost(g, nat, kind))) break;
    const cost = corpsUpkeep(kind, specOf(g, kind)!.divs);
    if (spare < cost * BUDGET.debt.aiMargin) break;
    // Nor does it raise new corps with men its armies in the field are waiting for (the grand campaign).
    if (!men(kind, specOf(g, kind)!.divs)) break;
    if (!orderCorps(g, nat, kind, at)) break;
    spare -= cost;
  }
}

/**
 * The support teams (release 2, src/campaign/teams.ts): a staff at war orders a machine-gun or mortar brigade only
 * when, at its own sight, it buys more on the card for its price than the corps its build pick would raise or the refit
 * the same money would pay for (the design director's rulings of 25 November 2026, 19:48Z, 20:08Z and 21:33Z;
 * orderworth.ts: one reckoning for every order, the battle card read with the order and without it where it would
 * stand, each front on the one card its plan fights there this month, whatever the order's kind). Of the two, the one
 * that leads the other orders by most; one order a month at most, outside the army's programme (as engineers are),
 * and only with the money, the upkeep and the men to spare. Nothing while the teams are off, or while the army is
 * being worn away and wants men in the line first.
 */
export function aiTeams(g: Game, nat: string): void {
  if (!teamsOn(g)) return;
  const n = g.nations[nat];
  if (byHand(g, nat, 'build') || !n?.alive || n.capital < 0 || !n.atWar.length || armyWorn(g, nat) || armyIdle(g, nat)) return;
  const spare = income(g, nat) - upkeepTotal(g, nat) - n.queue.reduce((t, b) => t + (b.what === 'corps' ? corpsUpkeep(b.kind!, b.divs!) : 0), 0);
  const can = TEAM_KINDS.filter((k) => {
    const price = corpsCost(g, nat, k);
    return price > 0 && n.pp >= price + PRODUCTION.aiReserve && spare >= corpsUpkeep(k, TEAMS[k].divs) * BUDGET.debt.aiMargin && menSpare(g, nat) >= TEAMS[k].divs && raiseSite(g, nat, k, n.capital) >= 0;
  });
  if (!can.length) return;
  const card = new Reckoning(g, nat);
  const pick = buildPick(g, nat, lineFactories(g, nat), n.capital).kind;
  let kind: TeamKind | null = null, lead = 1;
  for (const k of can) {
    const mine = card.worth(k);
    const other = Math.max(card.worth(pick), card.worth('refit', corpsCost(g, nat, k)));
    const by = other > 0 ? mine / other : mine > 0 ? Infinity : 0;
    if (by > lead) { kind = k; lead = by; }
  }
  if (kind) orderCorps(g, nat, kind, raiseSite(g, nat, kind, n.capital));
}
