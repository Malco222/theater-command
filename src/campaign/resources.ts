// Oil and supplies (the grand campaign): the two stocks a great power keeps beside its treasury.
// Money builds an army, oil moves it, supplies keep it fighting. A nation makes some of each at home, buys some
// from its neighbours, and brings the rest in by sea along shipping lanes that an enemy can raid or shut.
// When a stock runs dry the forces get only what came in that month, and fight, fly, sail and recover worse.
// Every number lives in data/campaign/resources.json. The historical scenarios do not use any of this.

import data from '../../data/campaign/resources.json';
import { KIND, type Corps, type Game, type Kind, type Region, type ShipClass, type Stores, type Terrain } from './types';
import { allied, atWar, bloc, headline } from './game';
import { CLASSES, atSea, laneLoss, oreRoute, zoneOf } from './navy';
import { workingFactories } from './rails';
import { TECH } from './tech';

export type Good = 'oil' | 'sup';

interface LaneRow {
  id: string; name: string; from: string; to: string; zones: string[]; oil: number; sup: number; text: string;
  /** A nation the cargoes are bought from: a war with it ends the traffic. */
  source?: string;
  /** An oilfield the cargoes come from: the lane runs while its holder is no enemy. */
  field?: string;
  /** Narrows the lane must pass: an enemy who holds one shuts it. */
  past?: string[];
  /** Sea powers whose blockade shuts the lane for as long as the recipient is at war with one of them. */
  shutBy?: string[];
  /** The lane only runs while the recipient fights beside one of these. */
  allies?: string[];
  /** For drawing it on the map: where it comes onto the chart, the seas it is drawn through, and a place it passes on the way. */
  enter?: [number, number];
  draw?: string[];
  via?: string;
}
interface DealRow { id: string; name: string; to: string; oil: number; sup: number; text: string; field?: string; seller?: string; ore?: boolean }

export const RES = data as unknown as {
  nations: string[];
  oil: {
    fields: Record<string, number>; occupied: number; home: Record<string, number>;
    need: { corps: Record<string, number>; air: number; ships: Record<ShipClass, number>; port: number; peace: number };
    start: Record<string, number>; cap: number;
    short: { attack: number; defence: number; air: number; fleet: number; slowAt: number };
  };
  sup: {
    land: Record<Terrain, number>; factory: number; occupied: number; overseas: number; offmap: number;
    ratio: Record<string, number>; perDiv: number; peace: number; start: Record<string, number>; cap: number;
    short: { fight: number; drafts: number; income: number };
  };
  will: { sup: number; oil: number; heal: number; per: number; most: number };
  lanes: LaneRow[];
  deals: DealRow[];
  ai: { armourAt: number; synthAt: number };
};

/** True when oil and supplies are counted. That is the grand campaign: the historical scenarios keep history's clock. */
export const resRules = (g: Game): boolean => g.flags.open === 1;

/** True for a power that keeps stocks of its own. Every other country buys what little it needs abroad and is never short. */
export const keepsStores = (g: Game, nat: string): boolean => resRules(g) && RES.nations.includes(nat) && !!g.nations[nat];

const GOODS: Good[] = ['oil', 'sup'];
export const GOOD_NAME: Record<Good, string> = { oil: 'Oil', sup: 'Supplies' };
const pick = (row: Record<string, number>, nat: string): number => row[nat] ?? row._default ?? 0;

// ---- What a nation makes, and what it needs -----------------------------------------------------------

export interface ResLine { key: string; label: string; amount: number; count?: number; note?: string; lane?: string }

/** What an oilfield yields a month to whoever holds it: less to a conqueror than to its owner. */
export function fieldYield(g: Game, r: Region, nat: string = r.ctrl): number {
  if (!r.oil) return 0;
  return (RES.oil.fields[r.name] ?? RES.oil.fields._default) * (r.owner === nat ? 1 : RES.oil.occupied) * (1 - fieldOut(g, r));
}

/** The share of an oilfield's yield lost to sabotage this month: wells set alight burn for months. 0 for a field at work. */
export const fieldOut = (g: Game, r: Region): number => (resRules(g) && (g.flags[`oilout:${r.id}`] ?? 0) > g.turn ? (g.flags[`oilcut:${r.id}`] ?? 0) / 100 : 0);

/** Sets an oilfield alight: it gives this much less for this many months. False where there is no field, or it is burning already. */
export function fireField(g: Game, r: Region, months: number, cut: number): boolean {
  if (!resRules(g) || !r.oil || fieldOut(g, r) > 0) return false;
  g.flags[`oilout:${r.id}`] = g.turn + months;
  g.flags[`oilcut:${r.id}`] = Math.round(cut * 100);
  return true;
}

/** What the technology tree's fuel plants make each month: so much for every working factory at home, and none from a bombed one. */
function treeOil(g: Game, nat: string): { amount: number; factories: number } {
  let rate = 0;
  for (const id of g.nations[nat].tech?.done ?? []) rate += TECH.techs.find((t) => t.id === id)?.oil ?? 0;
  if (!rate) return { amount: 0, factories: 0 };
  let fac = g.nations[nat].offmap?.industry ?? 0;
  for (const r of g.regions) if (r.owner === nat && r.ctrl === nat && !r.overseas) fac += workingFactories(g, r);
  return { amount: rate * fac, factories: fac };
}

/** What the forces burn in a month of war, by arm. */
function oilUse(g: Game, nat: string): { fast: number; fastDivs: number; foot: number; footDivs: number; air: number; planes: number; sea: number; atSea: number; port: number; inPort: number } {
  const N = RES.oil.need;
  const u = { fast: 0, fastDivs: 0, foot: 0, footDivs: 0, air: 0, planes: 0, sea: 0, atSea: 0, port: 0, inPort: 0 };
  for (const c of g.corps) {
    if (c.nat !== nat || c.dead) continue;
    const rate = N.corps[c.kind] ?? N.corps._default;
    if (c.kind === 'arm' || c.kind === 'mot') { u.fast += rate * c.divs; u.fastDivs += c.divs; } else { u.foot += rate * c.divs; u.footDivs += c.divs; }
  }
  for (const f of g.airFleets) if (f.nat === nat) { u.planes += f.ftr + f.bmr; u.air += (f.ftr + f.bmr) * N.air; }
  for (const f of g.fleets) {
    if (f.nat !== nat) continue;
    let burn = 0, hulls = 0;
    for (const c of CLASSES) { burn += f.ships[c] * N.ships[c]; hulls += f.ships[c]; }
    if (atSea(f)) { u.sea += burn; u.atSea += hulls; } else { u.port += burn * N.port; u.inPort += hulls; }
  }
  return u;
}

/** What a province gives in supplies: what its ground grows and what its working factories turn out. */
function landYield(g: Game, r: Region, built = false): number {
  return RES.sup.land[r.terrain] + (built ? r.fac : workingFactories(g, r)) * RES.sup.factory;
}

const divisions = (g: Game, nat: string): number => g.corps.reduce((s, c) => s + (c.nat === nat && !c.dead ? c.divs : 0), 0);

/** A month's need of oil or supplies. 'war' asks what it would be at war, whatever the nation is doing now. */
export function needOf(g: Game, nat: string, good: Good, war = g.nations[nat].atWar.length > 0): number {
  return outLines(g, nat, good, war).reduce((s, l) => s + l.amount, 0);
}

function outLines(g: Game, nat: string, good: Good, war = g.nations[nat].atWar.length > 0): ResLine[] {
  const n = g.nations[nat];
  if (good === 'oil') {
    const u = oilUse(g, nat), k = war ? 1 : RES.oil.need.peace;
    return [
      { key: 'fast', label: 'Tanks and lorried troops', amount: u.fast * k, count: u.fastDivs },
      { key: 'foot', label: 'The rest of the army', amount: u.foot * k, count: u.footDivs },
      { key: 'air', label: 'Aircraft', amount: u.air * k, count: u.planes },
      { key: 'sea', label: 'Ships at sea', amount: u.sea * k, count: u.atSea },
      { key: 'port', label: 'Ships in harbour', amount: u.port * k, count: u.inPort },
    ].filter((l) => l.amount > 1e-9);
  }
  // The people at home eat whether there is a war on or not; the army in the field uses far more in war.
  let home = 0, held = 0;
  for (const r of g.regions) if (r.owner === nat && r.ctrl === nat && !r.overseas) { home += landYield(g, r, true); held++; }
  if (n.offmap) home += n.offmap.industry * RES.sup.offmap;
  const divs = divisions(g, nat);
  return [
    { key: 'people', label: 'The people at home', amount: home / pick(RES.sup.ratio, nat), count: held },
    { key: 'army', label: 'The army in the field', amount: divs * RES.sup.perDiv * (war ? 1 : RES.sup.peace), count: divs },
  ].filter((l) => l.amount > 1e-9);
}

// ---- Lanes and deals ------------------------------------------------------------------------------------

export type LaneState = 'open' | 'raided' | 'cut' | 'idle';

export interface Lane {
  id: string;
  /** Points on the map to draw it through, from where it comes onto the chart to the port it serves. Empty for a lane that is not drawn. */
  path: [number, number][];
  /** A lane across the sea that raiders can attack, or a deal that comes overland. */
  kind: 'sea' | 'land';
  name: string;
  from: string;
  to: string;
  text: string;
  /** The seas it crosses, in order from the open ocean to the home ports. */
  zones: number[];
  /** What it would bring this month if every cargo arrived. */
  full: { oil: number; sup: number };
  /** The share that gets through, 0 to 1. */
  flow: number;
  /** What it brings this month at that flow. */
  oil: number;
  sup: number;
  /** Raiders against escorts on the lane, as the convoy war counts them. */
  raid: number;
  guard: number;
  state: LaneState;
  /** Why it is cut or idle, or what is happening on it, in a sentence. */
  why: string;
}

const friendly = (g: Game, holder: string, nat: string): boolean => {
  if (holder === nat) return true;
  const H = g.nations[holder], N = g.nations[nat];
  if (!H || !N || atWar(g, holder, nat)) return false;
  // A neutral sells to anyone; a country in the other camp does not.
  const hb = bloc(H), nb = bloc(N);
  return hb === null || nb === null || hb === nb;
};

const natOf = (g: Game, id: string): string => g.nations[id]?.name ?? id;
const the = (name: string): string => (/^(United|Soviet|Netherlands)/.test(name) ? `the ${name}` : name);

/** Where a sea lane runs on the map: in from the edge of the chart, through its seas, to the recipient's best port on the last of them. */
function lanePath(g: Game, row: LaneRow): [number, number][] {
  const seas = (row.draw ?? row.zones).map((k) => g.seas[zoneOf(g, k)]).filter(Boolean);
  if (!seas.length) return [];
  const pts: [number, number][] = [];
  if (row.enter) pts.push(row.enter);
  const via = row.via ? g.byName[row.via] : null;
  if (via) pts.push([via.x, via.y + 16]);
  // A little below each sea's name, so the line does not run through the lettering.
  for (const z of seas) pts.push([z.x, z.y + 22]);
  const last = seas[seas.length - 1].id;
  let port: Region | null = null;
  for (const r of g.regions) if (r.port && r.owner === row.to && !r.overseas && r.seas.includes(last) && (!port || r.fac > port.fac)) port = r;
  const cap = g.nations[row.to]?.capital ?? -1;
  const end = port ?? (cap >= 0 ? g.regions[cap] : null);
  if (end) pts.push([end.x, end.y]);
  return pts;
}

/** Why nothing sails on a sea lane this month, if nothing does: it is cut by an enemy, or idle because the trade has not begun. */
function laneStop(g: Game, row: LaneRow): { state: 'cut' | 'idle'; why: string } | null {
  const cut = (why: string, state: 'cut' | 'idle' = 'cut'): { state: 'cut' | 'idle'; why: string } => ({ state, why });
  const to = row.to;
  if (row.allies) {
    const friend = row.allies.find((a) => g.nations[a]?.alive && allied(g, a, to) && g.nations[a].atWar.length > 0);
    if (!friend || !g.nations[to].atWar.length) return cut(`It runs only while ${the(natOf(g, to))} fights beside the western Allies.`, 'idle');
  }
  if (row.source && g.nations[row.source] && atWar(g, row.source, to)) return cut(`${the(natOf(g, to))} is at war with ${the(natOf(g, row.source))}, which sold the cargoes.`);
  if (row.field) {
    const f = g.byName[row.field];
    if (f && !friendly(g, f.ctrl, to)) return cut(`${f.name} is in the hands of ${the(natOf(g, f.ctrl))}.`);
  }
  for (const name of row.past ?? []) {
    const r = g.byName[name];
    if (r && atWar(g, r.ctrl, to)) return cut(`${the(natOf(g, r.ctrl))} holds ${r.name} and lets nothing past.`);
  }
  const jailer = (row.shutBy ?? []).find((x) => g.nations[x]?.alive && atWar(g, x, to));
  if (jailer) return cut(`Shut by the blockade of ${the(natOf(g, jailer))}.`);
  return null;
}

/** Why nothing sails on the shipping lane with this id this month, or null while cargoes are moving (or it is no lane of these books). */
export function laneStopped(g: Game, id: string): { state: 'cut' | 'idle'; why: string } | null {
  const row = RES.lanes.find((l) => l.id === id);
  return row && g.nations[row.to] ? laneStop(g, row) : null;
}

/** One sea lane as it stands this month for the nation it feeds. 'need' is that nation's monthly need of each good. */
function seaLane(g: Game, row: LaneRow, need: Record<Good, number>): Lane {
  const zones = row.zones.map((k) => zoneOf(g, k)).filter((z) => z >= 0);
  const full = { oil: row.oil * need.oil, sup: row.sup * need.sup };
  const lane: Lane = { id: row.id, path: lanePath(g, row), kind: 'sea', name: row.name, from: row.from, to: row.to, text: row.text, zones, full, flow: 1, oil: 0, sup: 0, raid: 0, guard: 0, state: 'open', why: '' };
  const to = row.to;
  const stop = laneStop(g, row);
  if (stop) return { ...lane, flow: 0, state: stop.state, why: stop.why };
  if (zones.length && g.nations[to].atWar.length) {
    const m = laneLoss(g, to, zones, row.id);
    lane.raid = m.raid; lane.guard = m.guard; lane.flow = 1 - m.loss;
    if (m.loss >= 0.02) { lane.state = 'raided'; lane.why = `Raiders are sinking ${Math.round(m.loss * 100)}% of the cargoes.`; }
  }
  lane.oil = full.oil * lane.flow; lane.sup = full.sup * lane.flow;
  return lane;
}

/** One overland deal as it stands this month. */
function deal(g: Game, row: DealRow): Lane {
  const full = { oil: row.oil, sup: row.sup };
  const lane: Lane = { id: row.id, path: [], kind: 'land', name: row.name, from: '', to: row.to, text: row.text, zones: [], full, flow: 1, oil: row.oil, sup: row.sup, raid: 0, guard: 0, state: 'open', why: '' };
  const stop = (why: string, state: LaneState = 'cut'): Lane => ({ ...lane, flow: 0, oil: 0, sup: 0, state, why });
  const to = row.to;
  if (row.ore) {
    lane.from = 'Sweden';
    const way = oreRoute(g);
    // At peace the ore comes as it always did; at war it comes by whichever way is open.
    if (way === 'cut') return stop('The ore cannot get through: the Baltic is frozen or closed and Narvik is in enemy hands.');
    lane.why = way === 'narvik' ? 'Coming through Narvik while the Baltic is frozen.' : '';
    return lane;
  }
  let seller = row.seller ?? '';
  if (row.field) {
    const f = g.byName[row.field];
    if (!f) return stop('', 'idle');
    seller = f.ctrl;
    lane.from = natOf(g, f.owner);
    // A field the buyer holds itself is its own production, not a purchase.
    if (seller === to) return stop(`${f.name} is in ${natOf(g, to)}'s own hands and counts as its own production.`, 'idle');
  } else lane.from = natOf(g, seller);
  const S = g.nations[seller];
  if (!S || (!S.alive && !row.field)) return stop(`${the(natOf(g, seller))} has fallen.`);
  if (!friendly(g, seller, to)) {
    const f = row.field ? g.byName[row.field] : null;
    if (f && f.owner !== seller) return stop(`${f.name} is in the hands of ${the(natOf(g, seller))}.`);
    return stop(atWar(g, seller, to) ? `${the(natOf(g, to))[0].toUpperCase()}${the(natOf(g, to)).slice(1)} is at war with ${the(natOf(g, seller))}.` : `${the(natOf(g, seller))[0].toUpperCase()}${the(natOf(g, seller)).slice(1)} has gone over to the other side.`);
  }
  return lane;
}

/** Every lane and deal that feeds a nation, as it stands this month. */
export function lanesTo(g: Game, nat: string): Lane[] {
  if (!keepsStores(g, nat)) return [];
  const need = { oil: needOf(g, nat, 'oil'), sup: needOf(g, nat, 'sup') };
  return [...RES.lanes.filter((l) => l.to === nat).map((l) => seaLane(g, l, need)), ...RES.deals.filter((d) => d.to === nat).map((d) => deal(g, d))];
}

/** Every sea lane on the map, whoever it feeds: what an interface draws across the water. */
export function seaLanes(g: Game): Lane[] {
  if (!resRules(g)) return [];
  const out: Lane[] = [];
  for (const nat of RES.nations) {
    if (!g.nations[nat]?.alive) continue;
    for (const l of lanesTo(g, nat)) if (l.kind === 'sea' && l.zones.length) out.push(l);
  }
  return out;
}

// ---- The books ---------------------------------------------------------------------------------------------

export interface Book {
  good: Good;
  /** In the depots now, and the most they can hold. */
  stock: number;
  cap: number;
  /** Where it comes from each month, and where it goes. */
  income: ResLine[];
  spend: ResLine[];
  made: number;
  need: number;
  net: number;
  /** Months until the depots are empty at this rate; Infinity when the stock is steady or growing. */
  months: number;
  /** The share of the need that was met when the month began: 1 unless the depots were empty. */
  met: number;
  /** What the shortage is costing, in words. Empty when the need is met. */
  effects: string[];
}


export interface StoresView {
  nat: string; oil: Book; sup: Book; lanes: Lane[];
  /** The share of the nation's will to fight that privation at home has worn away, 0 to 1. */
  willLost: number;
}

function incomeLines(g: Game, nat: string, good: Good, lanes: Lane[]): ResLine[] {
  const n = g.nations[nat];
  const out: ResLine[] = [];
  if (good === 'oil') {
    let own = 0, nOwn = 0, taken = 0, nTaken = 0;
    for (const r of g.regions) {
      if (!r.oil || r.ctrl !== nat) continue;
      if (r.owner === nat) { own += fieldYield(g, r, nat); nOwn++; } else { taken += fieldYield(g, r, nat); nTaken++; }
    }
    // The wells and plants at home work for as long as the homeland is held.
    const home = pick(RES.oil.home, nat) * (n.offmap || (n.capital >= 0 && g.regions[n.capital].ctrl === nat) ? 1 : 0.5);
    out.push({ key: 'fields', label: 'Oilfields at home', amount: own, count: nOwn });
    out.push({ key: 'taken', label: 'Captured oilfields', amount: taken, count: nTaken });
    out.push({ key: 'home', label: n.offmap ? 'The oilfields of the homeland, beyond the map' : 'Small wells and plants at home', amount: home });
    const plants = treeOil(g, nat);
    out.push({ key: 'tech', label: 'Synthetic fuel plants', amount: plants.amount, count: plants.factories });
  } else {
    let land = 0, held = 0, fac = 0, nFac = 0, occ = 0, nOcc = 0, sea = 0, nSea = 0;
    for (const r of g.regions) {
      if (r.ctrl !== nat) continue;
      const works = workingFactories(g, r);
      if (r.owner !== nat) { occ += landYield(g, r) * RES.sup.occupied; nOcc++; }
      else if (r.overseas) { sea += landYield(g, r) * RES.sup.overseas; nSea++; }
      else { land += RES.sup.land[r.terrain]; held++; fac += works * RES.sup.factory; nFac += works; }
    }
    out.push({ key: 'land', label: 'Farms, mines and forests at home', amount: land, count: held });
    out.push({ key: 'fac', label: 'Factories at home', amount: fac, count: nFac });
    if (n.offmap) out.push({ key: 'homeland', label: 'The farms and mills of the homeland, beyond the map', amount: n.offmap.industry * RES.sup.offmap });
    out.push({ key: 'colonies', label: 'The colonies', amount: sea, count: nSea });
    out.push({ key: 'occupied', label: 'Taken from occupied ground', amount: occ, count: nOcc });
  }
  for (const l of lanes) {
    if (l.full[good] <= 0) continue;
    const label = l.kind === 'sea' ? `${l.name[0].toUpperCase()}${l.name.slice(1)}` : `${l.name}${l.from ? `, from ${the(l.from)}` : ''}`;
    out.push({ key: `lane:${l.id}`, label, amount: l[good], lane: l.id, note: l.state === 'open' ? (l.why || undefined) : l.state === 'raided' ? `${Math.round(l.flow * 100)}% getting through` : l.why });
  }
  // What the nation has agreed to send abroad is gone before its own forces draw a drop.
  for (const d of RES.deals) {
    if (d.seller !== nat || d[good] <= 0) continue;
    const l = deal(g, d);
    if (l.state === 'open') out.push({ key: `sold:${d.id}`, label: `Deliveries to ${the(natOf(g, d.to))}`, amount: -d[good] });
  }
  return out.filter((l) => Math.abs(l.amount) > 1e-9 || l.lane !== undefined);
}

const SHORT_OIL = (m: number): string[] => {
  const s = RES.oil.short, lack = 1 - m, pc = (x: number): number => Math.round(x * lack * 100);
  return [
    `Tanks and lorried troops attack ${pc(s.attack)}% weaker and defend ${pc(s.defence)}% weaker`,
    ...(m < s.slowAt ? ['They lose one move a month'] : []),
    `Aircraft are worth ${pc(s.air)}% less in every fight`,
    `The fleet is ${pc(s.fleet)}% less fit for sea`,
  ];
};
const SHORT_SUP = (m: number): string[] => {
  const s = RES.sup.short, lack = 1 - m, pc = (x: number): number => Math.round(x * lack * 100);
  return [`Every corps fights ${pc(s.fight)}% weaker`, `${pc(s.drafts)}% fewer replacements reach the armies`, `The treasury's income is down ${pc(s.income)}% as factories stand idle`];
};

/** The state a nation's stocks are kept in, made the first time it is asked for: depots filled for so many months of war. */
export function storesOf(g: Game, nat: string): Stores | null {
  if (!keepsStores(g, nat)) return null;
  const n = g.nations[nat];
  if (!n.res) {
    n.res = { oil: 0, sup: 0, oilMet: 1, supMet: 1 };
    n.res.oil = pick(RES.oil.start, nat) * needOf(g, nat, 'oil', true);
    n.res.sup = pick(RES.sup.start, nat) * needOf(g, nat, 'sup', true);
  }
  return n.res;
}

/** A nation's books for oil and for supplies this month: what comes in, what goes out, what is left, and every lane that feeds it. */
export function stores(g: Game, nat: string): StoresView | null {
  const st = storesOf(g, nat);
  if (!st) return null;
  const lanes = lanesTo(g, nat);
  const book = (good: Good): Book => {
    const income = incomeLines(g, nat, good, lanes), spend = outLines(g, nat, good);
    const made = income.reduce((s, l) => s + l.amount, 0), need = spend.reduce((s, l) => s + l.amount, 0);
    const cap = (good === 'oil' ? RES.oil.cap : RES.sup.cap) * Math.max(needOf(g, nat, good, true), 1);
    const net = made - need;
    const met = good === 'oil' ? st.oilMet : st.supMet;
    return { good, stock: st[good], cap, income, spend, made, need, net, months: net < -1e-9 ? st[good] / -net : Infinity, met, effects: met < 0.995 ? (good === 'oil' ? SHORT_OIL(met) : SHORT_SUP(met)) : [] };
  };
  return { nat, oil: book('oil'), sup: book('sup'), lanes, willLost: willLost(g, nat) };
}

/** Opens the stocks of every power that keeps them, once, when a grand campaign begins. */
export function openStores(g: Game): void {
  if (!resRules(g)) return;
  for (const nat of RES.nations) storesOf(g, nat);
}

/**
 * The month's reckoning, at the start of a nation's turn: what was made and brought in goes into the depots, what the
 * forces and the people need comes out. If the depots run dry the need is only part met, and the nation suffers for it
 * until next month's reckoning.
 */
export function resourcesTick(g: Game, nat: string): void {
  const v = stores(g, nat);
  if (!v) return;
  const st = g.nations[nat].res!;
  const hungryBefore = st.hunger ?? 0;
  for (const good of GOODS) {
    const b = v[good];
    let stock = st[good] + b.made - b.need, met = 1;
    if (stock < 0) { met = b.need > 0 ? Math.max(0, Math.min(1, (b.need + stock) / b.need)) : 1; stock = 0; }
    st[good] = Math.min(stock, b.cap);
    const was = good === 'oil' ? st.oilMet : st.supMet;
    if (good === 'oil') st.oilMet = met; else st.supMet = met;
    if (nat !== g.human) continue;
    const name = GOOD_NAME[good].toLowerCase(), key = `res:${good}:${nat}`, low = `res:low:${good}:${nat}`;
    if (met < 0.995 && was >= 0.995) {
      headline(g, 'info', good === 'oil'
        ? `The oil has run out. Only ${Math.round(met * 100)}% of what the forces need came in this month: tanks, aircraft and ships are short of fuel. Open Oil on the top bar to see where it comes from and what is stopping it.`
        : `The depots are empty. Only ${Math.round(met * 100)}% of the supplies the nation needs came in this month: the armies fight weaker, fewer replacements go out and factories stand idle. Open Supplies on the top bar to see why.`, nat);
      g.flags[key] = g.turn;
    } else if (met >= 0.995 && was < 0.995) {
      headline(g, 'info', `There is ${name} enough again: the forces have all they need this month.`, nat);
      delete g.flags[key];
    }
    // A warning while there is still time to do something about it.
    const left = b.net < -1e-9 ? st[good] / -b.net : Infinity;
    if (met >= 0.995 && left <= 3 && g.flags[low] === undefined) {
      g.flags[low] = g.turn;
      headline(g, 'info', `${GOOD_NAME[good]} for ${Math.max(1, Math.round(left))} more month${Math.round(left) <= 1 ? '' : 's'}: more is going out than coming in. Open ${GOOD_NAME[good]} on the top bar.`, nat);
    } else if (left > 6 && g.flags[low] !== undefined) delete g.flags[low];
  }
  // Privation at home: every month the depots are empty the people go shorter, and a people that goes short long
  // enough will not fight on. A month with everything it needs lets it recover.
  const W = RES.will, lack = W.sup * (1 - st.supMet) + W.oil * (1 - st.oilMet);
  const hunger = lack > 1e-9 ? hungryBefore + lack : Math.max(0, hungryBefore - W.heal);
  if (hunger > 0) st.hunger = hunger; else delete st.hunger;
  if (nat === g.human && willLost(g, nat) >= 0.1 && hungryBefore * W.per < 0.1) {
    headline(g, 'info', `Rationing bites at home. The longer the depots stay empty the less the nation will bear: its will to fight is falling, and a country that is starved long enough asks for terms. Open Powers to see how much fight is left.`, nat);
  }
}

/**
 * What privation has taken from a nation's will to fight, as a share of the whole: it counts toward surrender beside
 * a lost capital, lost ground and a broken army. This is how a blockade chokes a country that cannot feed itself.
 */
export function willLost(g: Game, nat: string): number {
  if (g.flags.open !== 1) return 0;
  const h = g.nations[nat]?.res?.hunger ?? 0;
  return h > 0 ? Math.min(RES.will.most, h * RES.will.per) : 0;
}

// ---- What a shortage does ---------------------------------------------------------------------------------

const metOf = (g: Game, nat: string, good: Good): number => {
  if (g.flags.open !== 1) return 1;
  const r = g.nations[nat]?.res;
  return r ? (good === 'oil' ? r.oilMet : r.supMet) : 1;
};

/** The share of its fuel need a nation's forces have this month, and of its supplies. 1 when nothing is short. */
export const fuelMet = (g: Game, nat: string): number => metOf(g, nat, 'oil');
export const suppliesMet = (g: Game, nat: string): number => metOf(g, nat, 'sup');

const runsOnFuel = (kind: Kind): boolean => kind === 'arm' || kind === 'mot';

/** What a shortage leaves of a corps' attack: fuel for tanks and lorries, and supplies for everyone. */
export function shortAttack(g: Game, c: Corps): number {
  const sup = 1 - RES.sup.short.fight * (1 - suppliesMet(g, c.nat));
  return runsOnFuel(c.kind) ? sup * (1 - RES.oil.short.attack * (1 - fuelMet(g, c.nat))) : sup;
}

export function shortDefence(g: Game, c: Corps): number {
  const sup = 1 - RES.sup.short.fight * (1 - suppliesMet(g, c.nat));
  return runsOnFuel(c.kind) ? sup * (1 - RES.oil.short.defence * (1 - fuelMet(g, c.nat))) : sup;
}

/** True when tanks and lorries are so short of fuel that they lose a move this month. */
export const fuelSlows = (g: Game, c: Corps): boolean => runsOnFuel(c.kind) && KIND[c.kind].mobile && fuelMet(g, c.nat) < RES.oil.short.slowAt;

/** What a fuel shortage leaves of an air force's worth, and of a fleet's fitness for sea. */
export const fuelAir = (g: Game, nat: string): number => 1 - RES.oil.short.air * (1 - fuelMet(g, nat));
export const fuelFleet = (g: Game, nat: string): number => 1 - RES.oil.short.fleet * (1 - fuelMet(g, nat));

/** What a shortage of supplies leaves of the monthly drafts, and of the treasury's income. */
export const supDrafts = (g: Game, nat: string): number => 1 - RES.sup.short.drafts * (1 - suppliesMet(g, nat));
export const supIncome = (g: Game, nat: string): number => 1 - RES.sup.short.income * (1 - suppliesMet(g, nat));

// ---- For the computer's staffs ------------------------------------------------------------------------------

/** Months of oil a nation has in hand at the rate it is going: Infinity when it is not losing any, or keeps no stock. */
export function oilMonths(g: Game, nat: string): number {
  const v = stores(g, nat);
  if (!v) return Infinity;
  if (v.oil.met < 0.995) return 0;
  return v.oil.months;
}

/** True when a staff should raise no more tanks for want of fuel to run them. */
export const shortOfOil = (g: Game, nat: string): boolean => oilMonths(g, nat) < RES.ai.armourAt;

/** A word for how a nation stands, for those who can only judge it from outside. */
export function standing(b: Book): 'ample' | 'steady' | 'tightening' | 'short' | 'dry' {
  if (b.met < 0.6) return 'dry';
  if (b.met < 0.995) return 'short';
  if (b.months <= 6) return 'tightening';
  return b.stock >= b.cap * 0.6 ? 'ample' : 'steady';
}
