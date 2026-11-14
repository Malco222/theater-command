// The war economy: factories earn production points, losses are made good first, and
// what is left buys new formations and fortifications that take months to arrive.
// The numbers live in data/campaign/production.json so they can be changed without code.

import { byHand } from './depth';
import data from '../../data/campaign/production.json';
import budgetData from '../../data/campaign/budget.json';
import arsenalData from '../../data/campaign/arsenal.json';
import setup from '../../data/campaign/setup-1939.json';
import rivalsData from '../../data/campaign/rivals.json';
import { STACK, type Build, type Corps, type Game, type Kind, type Region, type ShipClass, type Terrain } from './types';
import { addCorps, allied, headline } from './game';
import { DIPLOMACY, tradePartners } from './diplomacy';
import { campXp, civicSum, deliverCivic, hallShare } from './buildings';
import { dealsOf, tradeIncome } from './trade';
import { airIncomeLoss } from './air';
import { seaIncomeLoss } from './navy';
import { techMul } from './research';
import { fallenOf, inherit, noteCorps, raisedXp, revivable } from './stories';
import { deliverArsenal, worksUpkeep } from './arsenal';
import { barredKind } from './ground';
import { workingFactories } from './rails';
import { incomeMul, levelOf } from './difficulty';
import { eliteOf, priceOf, traitMoney, whyNoElite } from './traits';
import { TECH, locked, whyNoPara } from './tech';
import { fieldOut, keepsStores, shortOfOil, supIncome } from './resources';
import { SIEGE, besieged } from './siege';
import { prizeIncome, prizeOut, prizeRules } from './prizes';
import { govMul } from './government';
import { MANPOWER, drawMen, menSpare, raisable, returnMen } from './manpower';
import { raiseMorale } from './morale';

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
export function specOf(g: Game, kind: string): BuildSpec | undefined {
  return PRODUCTION.builds[kind] ?? (budgeted(g) ? (arsenalData.land as Record<string, BuildSpec>)[kind] : undefined);
}

/** Every kind of corps that can be ordered in this game, lightest first. */
export function corpsKinds(g: Game): Kind[] {
  const kinds = [...Object.keys(PRODUCTION.builds), ...(budgeted(g) ? Object.keys(arsenalData.land) : [])] as Kind[];
  // The fighting arms by the industry they need and their price, and the engineers after them.
  return kinds.sort((a, b) => Number(a === 'eng') - Number(b === 'eng') || needsOf(g, a) - needsOf(g, b) || (specOf(g, a)!.cost - specOf(g, b)!.cost));
}

/** What a corps of this kind costs this nation: the list price, less what its own abilities take off (the grand campaign). */
export function corpsCost(g: Game, nat: string, kind: string): number {
  const price = priceOf(g, nat, `corps:${kind}`, specOf(g, kind)?.cost ?? 0);
  // The player's Minister of Armaments drives the price down, or up (the grand campaign).
  const m = govMul(g, nat, 'cost');
  return m === 1 ? price : Math.round(price * m * 10) / 10;
}

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
  upkeep: { corps: Record<Kind, number>; ships: Record<ShipClass, number>; air: { ftr: number; bmr: number }; peace: number };
  debt: { floor: number; rest: number; desert: number; aiMargin: number };
};

/**
 * True when the budget rules are in force. That is the grand campaign: the historical scenarios keep the plain
 * arithmetic of factories and replacements, so that history's timetable holds.
 */
export const budgeted = (g: Game): boolean => g.flags.open === 1;

/** What one province brings its holder each month, before the nation-wide gains and losses. */
export interface Yield { land: number; port: number; oil: number; fac: number; capital: number; total: number; /** True while an enemy army stands round its town: it pays its holder little or nothing. */ siege?: boolean }

/**
 * What a province gives the nation that holds it. Conquered ground and colonies give less than the home country.
 * `whole` leaves a siege out of account: what the province is worth to its holder once the roads are open again.
 */
export function provinceYield(g: Game, r: Region, nat: string = r.ctrl, whole = false): Yield {
  const own = r.owner === nat;
  // A factory wrecked by bombing earns nothing until engineers rebuild it (the grand campaign only).
  const works = workingFactories(g, r);
  if (!budgeted(g)) { const fac = own ? works : works * PRODUCTION.occupiedIncome; return { land: 0, port: 0, oil: 0, fac, capital: 0, total: fac }; }
  // An administration set up on conquered ground makes it pay more of what it is worth.
  const lift = own ? 0 : hallShare(g, r);
  const fac = own ? works : works * (PRODUCTION.occupiedIncome + lift);
  const k = !own ? BUDGET.occupied + lift : r.overseas ? BUDGET.overseas : 1;
  const land = BUDGET.land[r.terrain] * k;
  // A harbour blocked pays nothing until it is mended (prizes.ts), and an oilfield alight pays only for the wells still at work.
  const port = r.port && (whole || prizeOut(g, r, 'harbour') <= 0) ? BUDGET.port * k : 0;
  const oil = r.oil ? BUDGET.oil * k * (whole ? 1 : 1 - fieldOut(g, r)) : 0;
  const capital = own && g.nations[nat]?.capital === r.id ? BUDGET.capital : 0;
  // With an enemy army round its town a province sends its holder next to nothing: the roads out are cut.
  if (!whole && nat === r.ctrl && besieged(g, r)) {
    const p = SIEGE.pay;
    return { land: land * p, port: port * p, oil: oil * p, fac: fac * p, capital: capital * p, total: (land + port + oil + fac + capital) * p, siege: true };
  }
  return { land, port, oil, fac, capital, total: land + port + oil + fac + capital };
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

const KIND_LABEL: Record<Kind, string> = { inf: 'Infantry', mtn: 'Mountain troops', cav: 'Cavalry', mot: 'Motorized troops', art: 'Artillery', eng: 'Engineers', arm: 'Armour', fort: 'Fortress troops' };
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
  for (const c of g.corps) {
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
  const peace = n.atWar.length ? 1 : PRODUCTION.peaceIncome;
  const sea = seaIncomeLoss(g, nat), air = airIncomeLoss(g, nat), tech = techMul(g, nat, 'industry');
  // What its armies hold outside towns they have not yet taken: factory districts, oilfields and quays.
  const prizes = prizeIncome(g, nat);
  const before = seat + facHome + facOcc + away + land + ports + oil + prizes.total;
  // What the nation's own abilities earn it (the grand campaign): banks, ore, a merchant fleet, factories moved out of reach.
  const gift = traitMoney(g, nat);
  const atPeace = before * peace + trade + gift.flat + civic;
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
    ...gift.lines,
  ];
  const changes: LedgerLine[] = [
    { key: 'peace', label: 'At peace: industry at half speed', amount: before * (peace - 1) },
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
    income: lines.filter((l) => l.amount > 1e-9), changes: changes.filter((l) => Math.abs(l.amount) > 1e-9),
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
    const trade = tradeIncome(g, nat) + towns.commerce + towns.farms + towns.tax;
    // The level of the campaign adds to the player's earnings, or to his rivals': at the fair level, nothing.
    const gift = traitMoney(g, nat);
    // Factories short of steel, coal and rubber stand idle part of the month.
    return (sum * (n.atWar.length ? 1 : PRODUCTION.peaceIncome) + trade + gift.flat) * (1 - seaIncomeLoss(g, nat)) * (1 - airIncomeLoss(g, nat)) * techMul(g, nat, 'industry') * incomeMul(g, nat) * gift.mul * supIncome(g, nat) * govMul(g, nat, 'income');
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
  const old = revive !== undefined ? revivable(g, nat, kind).find((f) => f.id === revive) : undefined;
  if (revive !== undefined && !old) return null;
  n.pp -= cost;
  drawMen(g, nat, spec.divs);
  const b: Build = { id: g.nextBuild++, what: 'corps', kind, divs: spec.divs, name: old ? old.name : corpsName(nat, kind, n.corpsNo++), region, ordered: g.turn, eta: g.turn + spec.months, cost };
  if (old) b.revive = old.id;
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
  if (!n.alive || !u || whyNoElite(g, nat) !== null || locked(g, nat, `corps:${u.kind}`) || !r || !canRaiseKind(g, nat, r, u.kind) || n.pp < u.cost || raisable(g, nat, u.divs) < 1) return null;
  n.pp -= u.cost;
  drawMen(g, nat, u.divs);
  const no = (g.flags[`elite:${nat}`] ?? 0) + 1;
  g.flags[`elite:${nat}`] = no;
  const b: Build = { id: g.nextBuild++, what: 'corps', kind: u.kind, divs: u.divs, name: `${roman(no)} ${u.word} Corps`, region, ordered: g.turn, eta: g.turn + u.months, cost: u.cost, elite: u.id };
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
  const cost = priceOf(g, nat, 'para', u.cost);
  if (!n.alive || whyNoPara(g, nat) !== null || !r || !canRaiseKind(g, nat, r, 'inf') || n.pp < cost || raisable(g, nat, u.divs) < 1) return null;
  n.pp -= cost;
  drawMen(g, nat, u.divs);
  const no = (g.flags[`para:${nat}`] ?? 0) + 1;
  g.flags[`para:${nat}`] = no;
  const b: Build = { id: g.nextBuild++, what: 'corps', kind: 'inf', divs: u.divs, name: `${roman(no)} ${u.word} Corps`, region, ordered: g.turn, eta: g.turn + u.months, cost, airborne: true };
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
  const list = u ? u.cost : c.airborne ? priceOf(g, nat, 'para', p.cost) : corpsCost(g, nat, kind);
  return { cost: Math.round(list * MANPOWER.fold.price * 10) / 10, months: u ? u.months : c.airborne ? p.months : specOf(g, kind)!.months, divs: c.divs };
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
  n.pp -= t.cost;
  drawMen(g, nat, t.divs);
  const b: Build = { id: g.nextBuild++, what: 'corps', kind: c.kind, divs: c.divs, name: c.name, region, ordered: g.turn, eta: g.turn + t.months, cost: t.cost, cadre: c.id };
  if (c.elite) b.elite = c.elite;
  if (c.airborne) b.airborne = true;
  n.queue.push(b);
  return b;
}

/** A cadre takes the field again: the same corps, with its number, its diary and its ribbons, filled with new men. */
function reform(g: Game, c: Corps, at: Region): void {
  c.dead = false; c.str = 1; c.org = 0.7; c.ap = c.apMax; c.fought = false; c.supplied = true; c.general = null; c.reg = at.id; c.hold = false; c.dyle = null;
  c.xp = Math.max((c.nat === g.human ? raisedXp(g, c.nat) : 0.05) + campXp(g, at.id), c.xp * MANPOWER.fold.xp);
  for (const k of ['dug', 'dugFor', 'pressed', 'blows', 'relief', 'yields', 'gaveWay', 'digs', 'joint', 'leave', 'town', 'job', 'rival'] as const) delete c[k];
  raiseMorale(g, c);
  g.corps.push(c);
  at.units.push(c);
  noteCorps(g, c, 'rebuilt', at.id);
}

/** What the next level of fortification would cost in a region, or null if it cannot be fortified further. */
export function fortCost(g: Game, nat: string, r: Region): number | null {
  if (!isHome(g, nat, r)) return null;
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
  n.queue.push(b);
  return b;
}

/** An order can be taken back for a full refund in the month it was placed. */
export function cancelBuild(g: Game, nat: string, id: number): boolean {
  const n = g.nations[nat];
  const i = n.queue.findIndex((b) => b.id === id);
  if (i < 0 || n.queue[i].ordered !== g.turn) return false;
  n.pp += n.queue[i].cost;
  // The men called up for a corps that was paid for go back to the reserve.
  if (n.queue[i].what === 'corps' && n.queue[i].cost > 0) returnMen(g, nat, n.queue[i].divs ?? 0);
  if (n.queue[i].cadre !== undefined) { /* the cadre keeps its own name and number: nothing to hand back */ }
  else if (n.queue[i].elite) g.flags[`elite:${nat}`] = (g.flags[`elite:${nat}`] ?? 1) - 1;
  else if (n.queue[i].airborne) g.flags[`para:${nat}`] = (g.flags[`para:${nat}`] ?? 1) - 1;
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
  for (const b of due) {
    const r = g.regions[b.region];
    if (b.what === 'air' || b.what === 'ship' || b.what === 'work') { deliverArsenal(g, nat, b); continue; }
    if (b.what === 'civic') { const news = deliverCivic(g, nat, b); if (news && nat === g.human) headline(g, 'info', news, nat, r.id); continue; }
    if (b.what === 'fort') {
      if (!isHome(g, nat, r)) continue; // the ground was lost before the works were finished
      const vs = new Set(r.fort?.vs ?? []);
      vs.add(r.owner);
      for (const j of r.nb) vs.add(g.regions[j].owner);
      r.fort = { lv: Math.min(3, (r.fort?.lv ?? 0) + 1), vs: [...vs] };
      if (nat === g.human) headline(g, 'info', `The new fortifications at ${r.name} are finished.`, nat, r.id);
      continue;
    }
    const room = (x: Region): boolean => canRaise(g, nat, x) && x.units.filter((u) => allied(g, u.nat, nat)).length < STACK;
    let at: Region | null = room(r) ? r : null;
    if (!at && n.capital >= 0 && room(g.regions[n.capital])) at = g.regions[n.capital];
    if (!at) at = g.regions.find((x) => room(x) && x.fac > 0) ?? g.regions.find(room) ?? null;
    if (!at) continue; // nowhere left to form up
    if (b.cadre !== undefined) {
      const i = (n.cadres ?? []).findIndex((x) => x.id === b.cadre);
      if (i < 0) continue;
      const c = n.cadres!.splice(i, 1)[0];
      reform(g, c, at);
      if (nat === g.human) headline(g, 'info', `${c.name} has been raised again round its cadre at ${at.name}, and is ready for orders.`, nat, at.id);
      continue;
    }
    // The player's new corps are as well trained as the instructors he has sent home can make them.
    const c = addCorps(g, nat, b.name, b.kind!, b.divs!, at.id, { xp: (nat === g.human ? raisedXp(g, nat) : 0.05) + campXp(g, at.id), airborne: b.airborne });
    c.org = 0.7;
    if (b.elite) c.elite = b.elite;
    const old = b.revive !== undefined ? fallenOf(g, b.revive) : undefined;
    if (old && old.heir === undefined) inherit(g, c, old.id);
    if (nat === g.human) headline(g, 'info', old ? `${b.name} is raised again at ${at.name}, ${g.turn - old.turn} months after it was lost at ${g.regions[old.reg].name}. It carries the old colours and is ready for orders.` : `${b.name} has finished forming at ${at.name} and is ready for orders.`, nat, at.id);
  }
}

/**
 * The grand campaign: true while a computer staff whose line has met dug-in troops wants massed guns to break them,
 * one corps of guns to every few of the rest, and has not got them yet.
 */
export function gunsWanted(g: Game, nat: string): boolean {
  const n = g.nations[nat];
  if (g.flags.open !== 1 || !n?.alive || !n.atWar.length) return false;
  const G = (arsenalData.ai as unknown as { guns: { per: number; most: number; facing: number } }).guns;
  let guns = n.queue.filter((b) => b.what === 'corps' && b.kind === 'art').length, rest = 0;
  for (const c of g.corps) if (c.nat === nat && !c.dead) { if (c.kind === 'art') guns++; else if (c.kind !== 'fort' && c.kind !== 'eng') rest++; }
  if (guns >= G.most || guns * G.per >= rest) return false;
  let dug = 0;
  for (const c of g.corps) {
    if (c.dead || !c.dug || !n.atWar.includes(c.nat) || !g.regions[c.reg].nb.some((j) => g.regions[j].ctrl === nat)) continue;
    if (++dug >= G.facing) return true;
  }
  return false;
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
  let home = 0;
  for (const r of g.regions) if (isHome(g, nat, r)) home += r.fac;
  // In the grand campaign a staff orders nothing its treasury could not keep in the field afterwards.
  const keep = (b: Build): number => (b.what === 'corps' ? corpsUpkeep(b.kind!, b.divs!) : 0);
  let spare = budgeted(g) ? income(g, nat) - upkeepTotal(g, nat) - n.queue.reduce((t, b) => t + keep(b), 0) : Infinity;
  // Works on order (the grand campaign's guns and airfields) do not hold up the raising of troops.
  // Engineers are ordered by their own rule (the grand campaign) and do not take a place in the army's programme.
  const slots = (): number => n.queue.filter((b) => (b.what === 'corps' && b.kind !== 'eng') || b.what === 'fort').length;
  // How many corps a staff has forming at once. Two, as history's programmes ran; in the grand campaign a great
  // industry keeps more in hand, so that a rich country at war turns its money into an army and does not sit on it.
  const most = g.flags.open === 1 && PRODUCTION.aiLine ? Math.max(2, Math.floor(home / PRODUCTION.aiLine)) : 2;
  // The nation's special formation comes first when it can be had (the grand campaign): one on order at a time.
  const u = budgeted(g) ? eliteOf(nat) : null;
  if (u && slots() < 2 && !n.queue.some((b) => b.elite) && whyNoElite(g, nat) === null && !locked(g, nat, `corps:${u.kind}`) && !(u.kind === 'arm' && shortOfOil(g, nat))) {
    const at = raiseSite(g, nat, u.kind, cap.id), cost = corpsUpkeep(u.kind, u.divs);
    // A staff never holds more than its bank allows, so a costly formation is ordered as soon as the bank is full.
    const need = Math.min(u.cost + PRODUCTION.aiReserve, PRODUCTION.aiBank[1]);
    if (at >= 0 && n.pp >= need && spare >= cost * BUDGET.debt.aiMargin && menSpare(g, nat) >= u.divs && orderElite(g, nat, at)) spare -= cost;
  }
  // The grand campaign: a staff whose line has met dug-in troops orders massed guns to break them, one corps of guns
  // to every few of the rest, and brings them up where the works are.
  const wantsGuns = (): boolean => gunsWanted(g, nat);
  while (slots() < most) {
    // A cadre waiting at home is raised again before anything new is formed (the grand campaign): the same corps, for a share of the price.
    // (So many at a time and no more: a staff does not rebuild its whole army at the cadre's price in one season.)
    const old = g.flags.open === 1 && n.queue.filter((b) => b.cadre !== undefined).length < rivalsData.fold.cadres ? (n.cadres ?? []).find((c) => !c.elite && !c.airborne && whyNoCadre(g, nat, c) === null) : undefined;
    if (old) {
      const t = cadreTerms(g, nat, old), up = corpsUpkeep(old.kind, old.divs);
      const site = budgeted(g) ? raiseSite(g, nat, old.kind, cap.id) : cap.id;
      if (n.pp >= t.cost + PRODUCTION.aiReserve && spare >= up * BUDGET.debt.aiMargin && menSpare(g, nat) >= old.divs && orderCadre(g, nat, old.id, site >= 0 ? site : cap.id)) { spare -= up; continue; }
    }
    let kind: Kind = wantsGuns() ? 'art' : home >= 20 && n.corpsNo % 3 === 2 ? 'arm' : 'inf';
    // In the grand campaign tanks are built where the factories are: at the capital if it has them, else in the
    // nation's greatest industrial province; a nation with no such province raises infantry instead.
    let at = budgeted(g) ? raiseSite(g, nat, kind, cap.id) : cap.id;
    // A nation that has not researched the tank arm raises infantry instead, and so does one with no fuel to run more tanks.
    if (at < 0 || locked(g, nat, `corps:${kind}`) || (kind === 'arm' && shortOfOil(g, nat))) { kind = 'inf'; at = cap.id; }
    if (n.pp < corpsCost(g, nat, kind) + PRODUCTION.aiReserve) break;
    const cost = corpsUpkeep(kind, specOf(g, kind)!.divs);
    if (spare < cost * BUDGET.debt.aiMargin) break;
    // Nor does it raise new corps with men its armies in the field are waiting for (the grand campaign).
    if (menSpare(g, nat) < specOf(g, kind)!.divs) break;
    if (!orderCorps(g, nat, kind, at)) break;
    spare -= cost;
  }
}
