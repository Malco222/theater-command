// The war economy: factories earn production points, losses are made good first, and
// what is left buys new formations and fortifications that take months to arrive.
// The numbers live in data/campaign/production.json so they can be changed without code.

import data from '../../data/campaign/production.json';
import budgetData from '../../data/campaign/budget.json';
import arsenalData from '../../data/campaign/arsenal.json';
import setup from '../../data/campaign/setup-1939.json';
import { STACK, type Build, type Game, type Kind, type Region, type ShipClass, type Terrain } from './types';
import { addCorps, allied, headline } from './game';
import { DIPLOMACY, tradePartners } from './diplomacy';
import { airIncomeLoss } from './air';
import { seaIncomeLoss } from './navy';
import { techMul } from './research';
import { fallenOf, inherit, raisedXp, revivable } from './stories';
import { deliverArsenal, worksUpkeep } from './arsenal';
import { barredKind } from './ground';

export interface BuildSpec { label: string; divs: number; cost: number; months: number; blurb: string }

export const PRODUCTION = data as unknown as {
  baseIncome: number; occupiedIncome: number; peaceIncome: number; replacementCost: number; manpower: number;
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
  return kinds.sort((a, b) => needsOf(g, a) - needsOf(g, b) || (specOf(g, a)!.cost - specOf(g, b)!.cost));
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
export interface Yield { land: number; port: number; oil: number; fac: number; capital: number; total: number }

/** What a province gives the nation that holds it. Conquered ground and colonies give less than the home country. */
export function provinceYield(g: Game, r: Region, nat: string = r.ctrl): Yield {
  const own = r.owner === nat;
  const fac = own ? r.fac : r.fac * PRODUCTION.occupiedIncome;
  if (!budgeted(g)) return { land: 0, port: 0, oil: 0, fac, capital: 0, total: fac };
  const k = !own ? BUDGET.occupied : r.overseas ? BUDGET.overseas : 1;
  const land = BUDGET.land[r.terrain] * k;
  const port = r.port ? BUDGET.port * k : 0;
  const oil = r.oil ? BUDGET.oil * k : 0;
  const capital = own && g.nations[nat]?.capital === r.id ? BUDGET.capital : 0;
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

const KIND_LABEL: Record<Kind, string> = { inf: 'Infantry', mtn: 'Mountain troops', cav: 'Cavalry', mot: 'Motorized troops', art: 'Artillery', arm: 'Armour', fort: 'Fortress troops' };
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
    if (r.owner === nat) { facHome += y.fac; nHome += r.fac; } else { facOcc += y.fac; nOcc += r.fac; }
    land += y.land; ports += y.port; oil += y.oil; capital += y.capital;
    if (y.port) nPorts++;
    if (y.oil) nOil++;
  }
  provinces.sort((a, b) => b.total - a.total || a.id - b.id);
  const away = n.offmap?.industry ?? 0;
  // A government whose homeland is off the map sits there, out of reach.
  const seat = budget ? (n.offmap ? BUDGET.capital : capital) : PRODUCTION.baseIncome;
  const partners = tradePartners(g, nat).length;
  const trade = partners * DIPLOMACY.tradeIncome;
  const peace = n.atWar.length ? 1 : PRODUCTION.peaceIncome;
  const sea = seaIncomeLoss(g, nat), air = airIncomeLoss(g, nat), tech = techMul(g, nat, 'industry');
  const before = seat + facHome + facOcc + away + land + ports + oil;
  const atPeace = before * peace + trade;
  const afterSea = atPeace * (1 - sea), afterAir = afterSea * (1 - air);
  const gross = budget ? afterAir * tech : income(g, nat);
  const lines: LedgerLine[] = [
    { key: 'capital', label: budget ? 'The capital and the state' : 'The state', amount: seat },
    { key: 'factories', label: 'Factories at home', amount: facHome, count: nHome },
    { key: 'occupied', label: 'Factories on occupied ground', amount: facOcc, count: nOcc },
    { key: 'homeland', label: 'Industry in the homeland, beyond the map', amount: away, count: away },
    { key: 'land', label: 'Towns, farms and land', amount: land, count: held },
    { key: 'ports', label: 'Harbours', amount: ports, count: nPorts },
    { key: 'oil', label: 'Oilfields', amount: oil, count: nOil },
    { key: 'trade', label: 'Trade with friendly neutrals', amount: trade, count: partners },
  ];
  const changes: LedgerLine[] = [
    { key: 'peace', label: 'At peace: industry at half speed', amount: before * (peace - 1) },
    { key: 'sea', label: 'Cargoes sunk at sea', amount: afterSea - atPeace },
    { key: 'air', label: 'Bombing of industry', amount: afterAir - afterSea },
    { key: 'tech', label: tech >= 1 ? 'Better production methods' : 'Production methods behind the times', amount: afterAir * (tech - 1) },
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
    sum += seat + (n.offmap?.industry ?? 0);
    const trade = tradePartners(g, nat).length * DIPLOMACY.tradeIncome;
    return (sum * (n.atWar.length ? 1 : PRODUCTION.peaceIncome) + trade) * (1 - seaIncomeLoss(g, nat)) * (1 - airIncomeLoss(g, nat)) * techMul(g, nat, 'industry');
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

function corpsName(nat: string, kind: Kind, no: number): string {
  const arm = PRODUCTION.armourName[nat] ?? PRODUCTION.armourName._default;
  const word = kind === 'arm' ? `${arm} ` : kind === 'mot' ? 'Motorized ' : kind === 'mtn' ? 'Mountain ' : kind === 'art' ? 'Artillery ' : '';
  return `${roman(no)} ${word}Corps`;
}

export const isHome = (g: Game, nat: string, r: Region): boolean => r.owner === nat && r.ctrl === nat;

/** Where new formations can be raised: at home, not in the colonies. Troops for overseas are raised at home and shipped. */
export const canRaise = (g: Game, nat: string, r: Region): boolean => isHome(g, nat, r) && !r.overseas;

/**
 * Orders a new corps. The points are paid now; the corps arrives in a few months. 'revive' names a destroyed
 * formation on the memorial wall: the new corps is raised under its name and carries its colours.
 */
export function orderCorps(g: Game, nat: string, kind: Kind, region: number, revive?: number): Build | null {
  const n = g.nations[nat];
  const spec = specOf(g, kind);
  const r = g.regions[region];
  if (!n.alive || !spec || !r || !canRaiseKind(g, nat, r, kind) || n.pp < spec.cost) return null;
  const old = revive !== undefined ? revivable(g, nat, kind).find((f) => f.id === revive) : undefined;
  if (revive !== undefined && !old) return null;
  n.pp -= spec.cost;
  const b: Build = { id: g.nextBuild++, what: 'corps', kind, divs: spec.divs, name: old ? old.name : corpsName(nat, kind, n.corpsNo++), region, ordered: g.turn, eta: g.turn + spec.months, cost: spec.cost };
  if (old) b.revive = old.id;
  n.queue.push(b);
  return b;
}

/** What the next level of fortification would cost in a region, or null if it cannot be fortified further. */
export function fortCost(g: Game, nat: string, r: Region): number | null {
  if (!isHome(g, nat, r)) return null;
  const queued = g.nations[nat].queue.filter((b) => b.what === 'fort' && b.region === r.id).length;
  const next = (r.fort?.lv ?? 0) + queued + 1;
  return next > 3 ? null : PRODUCTION.fort.cost[next];
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
  if (n.queue[i].what === 'corps' && n.queue[i].revive === undefined) n.corpsNo--;
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
    // The player's new corps are as well trained as the instructors he has sent home can make them.
    const c = addCorps(g, nat, b.name, b.kind!, b.divs!, at.id, { xp: nat === g.human ? raisedXp(g, nat) : 0.05 });
    c.org = 0.7;
    const old = b.revive !== undefined ? fallenOf(g, b.revive) : undefined;
    if (old && old.heir === undefined) inherit(g, c, old.id);
    if (nat === g.human) headline(g, 'info', old ? `${b.name} is raised again at ${at.name}, ${g.turn - old.turn} months after it was lost at ${g.regions[old.reg].name}. It carries the old colours and is ready for orders.` : `${b.name} has finished forming at ${at.name} and is ready for orders.`, nat, at.id);
  }
}

/** The computer's factory policy: keep a reserve for replacements, spend the rest on new corps. */
export function aiProduction(g: Game, nat: string): void {
  const n = g.nations[nat];
  if (nat === g.human || !n.alive || n.capital < 0 || !canBuildYet(g, nat)) return;
  const cap = g.regions[n.capital];
  if (!isHome(g, nat, cap)) return;
  let home = 0;
  for (const r of g.regions) if (isHome(g, nat, r)) home += r.fac;
  // In the grand campaign a staff orders nothing its treasury could not keep in the field afterwards.
  const keep = (b: Build): number => (b.what === 'corps' ? corpsUpkeep(b.kind!, b.divs!) : 0);
  let spare = budgeted(g) ? income(g, nat) - upkeepTotal(g, nat) - n.queue.reduce((t, b) => t + keep(b), 0) : Infinity;
  // Works on order (the grand campaign's guns and airfields) do not hold up the raising of troops.
  while (n.queue.filter((b) => b.what === 'corps' || b.what === 'fort').length < 2) {
    let kind: Kind = home >= 20 && n.corpsNo % 3 === 2 ? 'arm' : 'inf';
    // In the grand campaign tanks are built where the factories are: at the capital if it has them, else in the
    // nation's greatest industrial province; a nation with no such province raises infantry instead.
    let at = budgeted(g) ? raiseSite(g, nat, kind, cap.id) : cap.id;
    if (at < 0) { kind = 'inf'; at = cap.id; }
    if (n.pp < PRODUCTION.builds[kind].cost + PRODUCTION.aiReserve) break;
    const cost = corpsUpkeep(kind, PRODUCTION.builds[kind].divs);
    if (spare < cost * BUDGET.debt.aiMargin) break;
    if (!orderCorps(g, nat, kind, at)) break;
    spare -= cost;
  }
}
