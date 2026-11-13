// The buildings of a town, in the grand campaign. Every province has a chief place; its size sets how many building
// plots it has, and each plot takes one line of building raised in three levels: commerce, farming, learning,
// training, medicine, administration. They are apart from the military works of the arsenal.
//
// A level is kept as a mark on the game ('cv:market:17' = 2), so a save carries it and a game that never builds
// carries nothing. Every number is in data/campaign/buildings.json. None of it touches the history campaigns.

import data from '../../data/campaign/buildings.json';
import budgetData from '../../data/campaign/budget.json';
import { townName, townSize, type TownSize } from './towns';
import type { Build, Corps, Game, Region, Terrain } from './types';

export interface CivicLine {
  id: string;
  /** The line as a whole ('Commerce') and its three levels by name. */
  name: string;
  levels: string[];
  cost: number[];
  months: number[];
  blurb: string;
  /** The smallest place that can hold it, and the only ground it can stand on. */
  size?: TownSize;
  ground?: Terrain[];
  /** True when it can be built on conquered ground as well as at home. */
  occupied?: boolean;
  money?: number; trade?: number; house?: number;
  sup?: number; moneyElse?: number;
  research?: number;
  xp?: number; drafts?: number;
  rest?: number; org?: number;
  tax?: number; occupiedShare?: number; pc?: number;
}

export const CIVIC = data as unknown as {
  plots: Record<TownSize, number>;
  place: { base: number; per: number };
  lines: CivicLine[];
  caps: { research: number; drafts: number; trade: number; pc: number; houses: number };
  ai: { share: number; reserve: number; gang: number; mix: Record<string, number> };
};

const LAND = (budgetData as unknown as { land: Record<Terrain, number>; occupied: number }).land;
const OCCUPIED = (budgetData as unknown as { occupied: number }).occupied;
const SIZES: TownSize[] = ['village', 'town', 'city', 'capital'];
const TOP = 3;
const lineOf = (id: string): CivicLine | undefined => CIVIC.lines.find((l) => l.id === id);
const key = (line: string, region: number): string => `cv:${line}:${region}`;

/** True when towns can be built up: the grand campaign. */
export const civicRules = (g: Game): boolean => g.flags.open === 1;

/** The level of one line of building in a province, 0 when there is none. */
export const civicLevel = (g: Game, line: string, region: number): number => (g.flags['cv:any'] ? g.flags[key(line, region)] ?? 0 : 0);

/** The buildings that stand in a province. */
export function civicAt(g: Game, region: number): { line: CivicLine; level: number }[] {
  if (!g.flags['cv:any']) return [];
  const out: { line: CivicLine; level: number }[] = [];
  for (const l of CIVIC.lines) { const lv = g.flags[key(l.id, region)] ?? 0; if (lv > 0) out.push({ line: l, level: lv }); }
  return out;
}

/** How many building plots the chief place of a province has. */
export const plotsOf = (g: Game, r: Region): number => CIVIC.plots[townSize(g, r)];

const building = (g: Game, nat: string, region: number): Build[] => g.nations[nat]?.queue.filter((b) => b.what === 'civic' && b.region === region) ?? [];

/** How much more a building pays in a rich place than a poor one. */
const placeWorth = (r: Region): number => CIVIC.place.base + LAND[r.terrain] / CIVIC.place.per;

/** What one more level of a line would do here, in a sentence. */
function effectOf(g: Game, l: CivicLine, r: Region, nat: string, keeps: boolean): string {
  const own = r.owner === nat;
  const bits: string[] = [];
  const cash = (x: number): string => `${Math.round(x * 100)}`;
  const pct = (x: number): string => `${Math.round(x * 1000) / 10}%`;
  if (l.money) bits.push(`${cash(l.money * placeWorth(r) * (own ? 1 : OCCUPIED))} more a month`);
  if (l.trade) bits.push(`trade agreements worth ${pct(l.trade)} more`);
  if (l.sup) bits.push(keeps ? `${l.sup.toFixed(1)} more supplies a month` : `${cash(l.moneyElse ?? 0)} more a month`);
  if (l.research) bits.push(`research ${pct(l.research)} faster`);
  if (l.xp) bits.push('corps raised here start more experienced');
  if (l.drafts) bits.push(`replacements ${pct(l.drafts)} faster`);
  if (l.rest) bits.push(`corps resting here or beside it recover ${pct(l.rest)} faster`);
  if (own && l.tax) bits.push(`${cash(l.tax)} more tax a month`);
  if (own && l.pc) bits.push(`${l.pc.toFixed(2)} political capital a month`);
  if (!own && l.occupiedShare) bits.push(`the province pays ${pct(l.occupiedShare)} more of its worth`);
  const s = bits.join(', ');
  return s ? `${s[0].toUpperCase()}${s.slice(1)}.` : '';
}

/** What the levels of a line that stand in a province do for whoever holds it now, in a sentence. */
function doesOf(g: Game, l: CivicLine, r: Region, lv: number, keeps: boolean): string {
  const own = r.owner === r.ctrl;
  const share = own ? 1 : OCCUPIED;
  const bits: string[] = [];
  const cash = (x: number): string => `${Math.round(x * 100)}`;
  const pct = (x: number): string => `${Math.round(x * 1000) / 10}%`;
  if (l.money) bits.push(`pays ${cash(l.money * lv * placeWorth(r) * share)} a month`);
  if (own && l.trade) bits.push(`trade agreements worth ${pct(l.trade * lv)} more`);
  if (l.sup) bits.push(keeps ? `sends ${(l.sup * lv * share).toFixed(1)} supplies a month` : `pays ${cash((l.moneyElse ?? 0) * lv * share)} a month`);
  if (own && l.research) bits.push(`research ${pct(l.research * lv)} faster`);
  if (own && l.xp) bits.push('corps raised here start more experienced');
  if (own && l.drafts) bits.push(`replacements ${pct(l.drafts * lv)} faster`);
  if (l.rest) bits.push(`corps resting here or beside it recover ${pct(l.rest * lv)} faster`);
  if (own && l.tax) bits.push(`${cash(l.tax * lv)} tax a month`);
  if (own && l.pc) bits.push(`${(l.pc * lv).toFixed(2)} political capital a month`);
  if (!own && l.occupiedShare) bits.push(`the province pays ${pct(l.occupiedShare * lv)} more of its worth`);
  if (!bits.length) return 'Stands idle under occupation.';
  const s = bits.join(', ');
  return `${s[0].toUpperCase()}${s.slice(1)}.`;
}

/** Why this nation cannot build the next level of this line here, apart from money; null when it can. */
export function whyNotCivic(g: Game, nat: string, r: Region | undefined, line: string): string | null {
  const n = g.nations[nat], l = lineOf(line);
  if (!civicRules(g)) return 'Towns are built up in the grand campaign.';
  if (!n?.alive || !r || !l) return 'Nothing can be built there.';
  if (r.ctrl !== nat) return `${r.name} is not yours.`;
  const own = r.owner === nat;
  if (!own && !l.occupied) return 'Only administration and medicine can be set up on conquered ground.';
  const lv = civicLevel(g, line, r.id);
  if (lv >= TOP) return `The ${l.levels[TOP - 1].toLowerCase()} is as far as this goes.`;
  const size = townSize(g, r);
  if (l.size && SIZES.indexOf(size) < SIZES.indexOf(l.size)) return `Needs a ${l.size}: ${townName(r)} is a ${size}.`;
  if (l.ground && !l.ground.includes(r.terrain)) return 'The ground here will not carry it.';
  const busy = building(g, nat, r.id);
  if (busy.length) return `The builders at ${townName(r)} are busy with the ${busy[0].name.split(' at ')[0].toLowerCase()}.`;
  if (lv === 0 && civicAt(g, r.id).length >= plotsOf(g, r)) return `Every building plot at ${townName(r)} is taken. A bigger place has more.`;
  return null;
}

export interface CivicOffer {
  /** The line ('market'), its name ('Commerce') and the level this order would raise ('Merchant quarter', 2). */
  line: string;
  name: string;
  label: string;
  level: number;
  /** The level that stands now, 0 for none, and its name. */
  has: number;
  hasLabel: string;
  cost: number;
  months: number;
  /** What the next level would do here, and what the line is, each in a sentence. */
  effect: string;
  blurb: string;
  /** Why it cannot be ordered now, or null. 'poor' is true when money is the only thing in the way. */
  why: string | null;
  poor: boolean;
}

/** Every line of building, as an offer for this province: what the next level is, costs and does, or why not. */
export function civicOffers(g: Game, nat: string, region: number, keeps = false): CivicOffer[] {
  const r = g.regions[region], n = g.nations[nat];
  if (!r || !n) return [];
  return CIVIC.lines.map((l) => {
    const has = civicLevel(g, l.id, region);
    const next = Math.min(TOP, has + 1);
    const cost = l.cost[next - 1], months = l.months[next - 1];
    let why = whyNotCivic(g, nat, r, l.id);
    const poor = why === null && n.pp < cost;
    if (poor) why = 'The treasury cannot cover it.';
    return { line: l.id, name: l.name, label: l.levels[next - 1], level: next, has, hasLabel: has ? l.levels[has - 1] : '', cost, months, effect: effectOf(g, l, r, nat, keeps), blurb: l.blurb, why, poor };
  });
}

export interface PlotView {
  /** The line that stands on the plot, or null for a free plot. */
  line: string | null;
  name: string;
  label: string;
  level: number;
  /** True while the next level (or the first) is being built, and the month it is due. */
  building: boolean;
  due: number;
  /** What the levels that stand do for the holder now, in a sentence; empty for a free plot or a first level still building. */
  does: string;
}

/** A province's building plots in order: what stands, what is being built, and the free ones. */
export function plotsAt(g: Game, region: number, keeps = false): PlotView[] {
  const r = g.regions[region];
  if (!r || !civicRules(g)) return [];
  const busy = building(g, r.ctrl, region);
  const out: PlotView[] = civicAt(g, region).map(({ line, level }) => {
    const b = busy.find((x) => x.item === line.id);
    return { line: line.id, name: line.name, label: line.levels[level - 1], level, building: !!b, due: b?.eta ?? -1, does: doesOf(g, line, r, level, keeps) };
  });
  for (const b of busy) {
    if (out.some((p) => p.line === b.item)) continue;
    const l = lineOf(b.item ?? '');
    if (l) out.push({ line: l.id, name: l.name, label: l.levels[0], level: 0, building: true, due: b.eta, does: '' });
  }
  const plots = Math.max(plotsOf(g, r), out.length);
  while (out.length < plots) out.push({ line: null, name: '', label: '', level: 0, building: false, due: -1, does: '' });
  return out;
}

/** Orders the next level of a line of building in a province. The price is paid now. */
export function orderCivic(g: Game, nat: string, region: number, line: string): Build | null {
  const n = g.nations[nat], r = g.regions[region], l = lineOf(line);
  if (!n || !r || !l || whyNotCivic(g, nat, r, line) !== null) return null;
  const next = civicLevel(g, line, region) + 1;
  const cost = l.cost[next - 1];
  if (n.pp < cost) return null;
  n.pp -= cost;
  const b: Build = { id: g.nextBuild++, what: 'civic', item: line, name: `${l.levels[next - 1]} at ${townName(r)}`, region, ordered: g.turn, eta: g.turn + l.months[next - 1], cost };
  n.queue.push(b);
  return b;
}

/** Hands over a building that is finished. Returns the news of it, or null if the ground was lost meanwhile. */
export function deliverCivic(g: Game, nat: string, b: Build): string | null {
  const r = g.regions[b.region], l = lineOf(b.item ?? '');
  if (!r || !l || r.ctrl !== nat) return null;
  const lv = Math.min(TOP, civicLevel(g, l.id, r.id) + 1);
  g.flags['cv:any'] = 1;
  g.flags[key(l.id, r.id)] = lv;
  return `The ${l.levels[lv - 1].toLowerCase()} at ${townName(r)} is finished.`;
}

// ---- What the buildings are worth ---------------------------------------------------------------------

export interface CivicSum {
  /** Money a month from commerce, from farming (a country that keeps no stocks) and from taxes. */
  commerce: number; farms: number; tax: number;
  /** Supplies a month, for a power that keeps stocks. */
  sup: number;
  /** Shares added to research speed, replacements and the worth of trade agreements; political capital a month; trading houses. */
  research: number; drafts: number; trade: number; pc: number; houses: number;
  count: number;
}

const ZERO: CivicSum = { commerce: 0, farms: 0, tax: 0, sup: 0, research: 0, drafts: 0, trade: 0, pc: 0, houses: 0, count: 0 };

/** Everything a nation's buildings add up to this month. 'keeps' says whether it is a power that keeps stocks of supplies. */
export function civicSum(g: Game, nat: string, keeps = false): CivicSum {
  if (!g.flags['cv:any'] || !civicRules(g)) return ZERO;
  const s: CivicSum = { ...ZERO };
  const C = CIVIC.caps;
  for (const r of g.regions) {
    if (r.ctrl !== nat) continue;
    const own = r.owner === nat;
    for (const l of CIVIC.lines) {
      const lv = g.flags[key(l.id, r.id)] ?? 0;
      if (lv <= 0) continue;
      s.count++;
      if (l.money) s.commerce += l.money * lv * placeWorth(r) * (own ? 1 : OCCUPIED);
      if (l.sup) { if (keeps) s.sup += l.sup * lv * (own ? 1 : OCCUPIED); else s.farms += (l.moneyElse ?? 0) * lv * (own ? 1 : OCCUPIED); }
      if (!own) continue;
      if (l.trade) s.trade += l.trade * lv;
      if (l.house && lv >= l.house && (r.port || g.nations[nat].capital === r.id)) s.houses++;
      if (l.research) s.research += l.research * lv;
      if (l.drafts) s.drafts += l.drafts * lv;
      if (l.tax) s.tax += l.tax * lv;
      if (l.pc) s.pc += l.pc * lv;
    }
  }
  s.research = Math.min(C.research, s.research);
  s.drafts = Math.min(C.drafts, s.drafts);
  s.trade = Math.min(C.trade, s.trade);
  s.pc = Math.min(C.pc, s.pc);
  s.houses = Math.min(C.houses, s.houses);
  return s;
}

/** How fast a nation's research teams work: 1 with no schools. */
export const researchSpeed = (g: Game, nat: string): number => 1 + civicSum(g, nat).research;

/** How much faster replacements reach a nation's army for its training grounds: 1 with none. */
export const civicDrafts = (g: Game, nat: string): number => 1 + civicSum(g, nat).drafts;

/** The experience a training ground adds to a corps raised in this province. */
export function campXp(g: Game, region: number): number {
  const l = lineOf('camp');
  return l?.xp ? l.xp * civicLevel(g, 'camp', region) : 0;
}

/** The best hospital within reach of a corps: in its own province or a neighbouring one its side holds. */
export function hospitalNear(g: Game, c: Corps, friendly: (holder: string) => boolean): number {
  if (!g.flags['cv:any'] || !civicRules(g)) return 0;
  const r = g.regions[c.reg];
  let best = friendly(r.ctrl) ? civicLevel(g, 'hospital', r.id) : 0;
  for (const j of r.nb) if (friendly(g.regions[j].ctrl)) best = Math.max(best, civicLevel(g, 'hospital', j));
  return best;
}

/** What a hospital of this level adds to the drafts that reach a resting corps (a multiplier) and to the rest it gets back. */
export function hospitalWorth(level: number): { drafts: number; org: number } {
  const l = lineOf('hospital');
  return { drafts: 1 + (l?.rest ?? 0) * level, org: (l?.org ?? 0) * level };
}

/** The share of a conquered province's worth that an administration adds to what reaches the treasury. */
export function hallShare(g: Game, r: Region): number {
  const l = lineOf('hall');
  return l?.occupiedShare ? l.occupiedShare * civicLevel(g, 'hall', r.id) : 0;
}

// ---- The computer's builders --------------------------------------------------------------------------

/**
 * A computer-run nation's building for the month. It sets aside a share of what it earned, and when that covers
 * the best building on offer and the treasury can spare it, orders it: one at a time, richest places first.
 */
export function civicAI(g: Game, nat: string, earned: number, keeps = false): void {
  const n = g.nations[nat];
  if (!civicRules(g) || nat === g.human || !n?.alive || n.capital < 0) return;
  const A = CIVIC.ai, purse = `cvb:${nat}`;
  g.flags[purse] = (g.flags[purse] ?? 0) + Math.max(0, earned) * A.share;
  const worth = (r: Region): number => (g.nations[nat].capital === r.id ? 100 : 0) + r.vp * 3 + r.fac * 2 + LAND[r.terrain] * 10 + (r.port ? 1 : 0);
  const home = g.regions.filter((r) => r.ctrl === nat && r.owner === nat).sort((a, b) => worth(b) - worth(a) || a.id - b.id);
  // A big country has more builders: one gang at work for every 'gang' home provinces, and always one.
  const gangs = 1 + Math.floor(home.length / A.gang);
  // How many levels of each line it has already, counting what is on order: the line furthest behind its weight in the mix comes next.
  const built: Record<string, number> = {};
  for (const r of home) for (const { line, level } of civicAt(g, r.id)) built[line.id] = (built[line.id] ?? 0) + level;
  for (const b of n.queue) if (b.what === 'civic' && b.item) built[b.item] = (built[b.item] ?? 0) + 1;
  for (let k = n.queue.filter((b) => b.what === 'civic').length; k < gangs; k++) {
    const have = g.flags[purse];
    const lines = Object.keys(A.mix).sort((x, y) => (built[x] ?? 0) / A.mix[x] - (built[y] ?? 0) / A.mix[y] || A.mix[y] - A.mix[x]);
    let pick: { r: Region; o: CivicOffer } | null = null;
    for (const line of lines) {
      for (const r of home.slice(0, 30)) {
        const o = civicOffers(g, nat, r.id, keeps).find((x) => x.line === line);
        if (o && (o.why === null || o.poor)) { pick = { r, o }; break; }
      }
      if (pick) break;
    }
    if (!pick || have < pick.o.cost || n.pp < pick.o.cost + A.reserve) return;
    if (!orderCivic(g, nat, pick.r.id, pick.o.line)) return;
    g.flags[purse] = have - pick.o.cost;
    built[pick.o.line] = (built[pick.o.line] ?? 0) + 1;
  }
}
