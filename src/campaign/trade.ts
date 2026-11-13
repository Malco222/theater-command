// Trade agreements, in the grand campaign. Two countries at peace with each other can sign one, and it pays both of
// them every month: the richer the two, the more. A country keeps as many agreements as it has trading houses. Goods
// go overland between neighbours and otherwise by sea, where an enemy can sink a share of them or close the way.
//
// Every number is in data/campaign/trade.json. In the history campaigns trade stays the flat sum of diplomacy.json.

import data from '../../data/campaign/trade.json';
import diplomacyData from '../../data/campaign/diplomacy.json';
import { civicSum } from './buildings';
import { govMul } from './government';
import { provinceYield } from './economy';
import { atWar, bloc, headline } from './game';
import { canPass, closedTo, seaRisk, zoneOf } from './navy';
import type { Game, Nation, Region, TradeDeal } from './types';

export const TRADE = data as unknown as {
  base: number; scale: number; far: number; farLeast: number; colony: number; transit: { toll: number; most: number };
  houses: { major: number; minor: number; most: number };
  sign: { pc: number };
  cold: number;
  sea: { loss: number; most: number; raided: number };
  pairs: [string, string][];
  never: [string, string][];
};

const MAJORS = (diplomacyData as unknown as { majors: string[] }).majors;

/** True when trade is by agreement: the grand campaign. */
export const tradeRules = (g: Game): boolean => g.flags.open === 1;

const can = (n: Nation | undefined): boolean => !!n && n.alive && !n.dormant;

/** What a country's provinces pay it a month, before anything else is counted: the measure of how much it has to trade. */
export function wealthOf(g: Game, nat: string): number {
  const n = g.nations[nat];
  if (!n) return 0;
  let sum = n.offmap?.industry ?? 0;
  for (const r of g.regions) if (r.ctrl === nat) sum += provinceYield(g, r, nat).total;
  return sum;
}

/** A nation's agreements. */
export const dealsOf = (g: Game, nat: string): TradeDeal[] => g.trade.filter((d) => d.a === nat || d.b === nat);
const other = (d: TradeDeal, nat: string): string => (d.a === nat ? d.b : d.a);
const dealOf = (g: Game, a: string, b: string): TradeDeal | undefined => g.trade.find((d) => (d.a === a && d.b === b) || (d.a === b && d.b === a));

/** How many agreements a country can keep. */
export function housesOf(g: Game, nat: string): number {
  const H = TRADE.houses;
  return Math.min(H.most, (MAJORS.includes(nat) || !!g.nations[nat]?.offmap ? H.major : H.minor) + civicSum(g, nat).houses);
}

// ---- The way the goods go ---------------------------------------------------------------------------

/** What a long way by sea leaves of an agreement's worth. */
const farWorth = (seas: number): number => Math.max(TRADE.farLeast, 1 - TRADE.far * Math.max(0, seas - 1));

export interface TradeRoute {
  way: 'land' | 'sea' | 'none';
  /** The seas crossed, in order, for a sea route. */
  seas: number[];
  /** The share of the goods sunk on the way. */
  lost: number;
  /** The share of an agreement's full worth this way leaves, for distance and tolls, before anything is sunk: 1 across a border between the two countries' home ground. */
  keeps: number;
  /** A land route by a border between colonies: the first country's province at that border. */
  via?: number;
  /** A land route across other countries: those countries, in order. */
  through?: string[];
}

const NONE: TradeRoute = { way: 'none', seas: [], lost: 0, keeps: 0 };

/** What is worked out once for a nation and reused while several routes are looked at. */
interface SeaView { shut: boolean[]; risk: number[]; from: number[] }
export type TradeCtx = Map<string, SeaView>;

function seaView(g: Game, nat: string, ctx: TradeCtx): SeaView {
  let v = ctx.get(nat);
  if (v) return v;
  const n = g.nations[nat];
  const war = n.atWar.length > 0;
  v = {
    shut: g.seas.map((z) => war && closedTo(g, z.id, nat)),
    risk: g.seas.map((z) => (war ? seaRisk(g, z.id, nat) : 0)),
    from: [],
  };
  // Goods are landed at a harbour. A country with none on the map still has its fishing ports: any coast will do.
  const ports = new Set<number>(), coasts = new Set<number>();
  for (const r of g.regions) if (r.ctrl === nat) for (const z of r.seas) { coasts.add(z); if (r.port) ports.add(z); }
  if (n.offmap) { const z = zoneOf(g, n.offmap.sea); if (z >= 0) ports.add(z); }
  v.from = [...(ports.size ? ports : coasts)];
  ctx.set(nat, v);
  return v;
}

/** Which countries hold ground beside which, colonies left out, worked out once for a set of routes. */
const BORDERS = new WeakMap<TradeCtx, Map<string, Set<string>>>();
function borders(g: Game, ctx: TradeCtx): Map<string, Set<string>> {
  let m = BORDERS.get(ctx);
  if (m) return m;
  m = new Map();
  for (const r of g.regions) {
    if (r.overseas) continue;
    for (const j of r.nb) {
      const o = g.regions[j].ctrl;
      if (o === r.ctrl || g.regions[j].overseas) continue;
      let set = m.get(r.ctrl);
      if (!set) m.set(r.ctrl, (set = new Set()));
      set.add(o);
    }
  }
  BORDERS.set(ctx, m);
  return m;
}

/** The fewest other countries goods must cross overland from one country to another, through ground at peace with both; null if there is no such way. */
function landWay(g: Game, a: string, b: string, ctx: TradeCtx): string[] | null {
  const B = borders(g, ctx), most = TRADE.transit.most;
  const open = (c: string): boolean => { const n = g.nations[c]; return !!n && n.alive && !atWar(g, c, a) && !atWar(g, c, b); };
  const prev = new Map<string, string>([[a, '']]), depth = new Map<string, number>([[a, 0]]);
  const queue = [a];
  for (let i = 0; i < queue.length; i++) {
    const at = queue[i], d = depth.get(at)!;
    for (const c of B.get(at) ?? []) {
      if (prev.has(c)) continue;
      if (c === b) {
        const out: string[] = [];
        for (let x = at; x !== a; x = prev.get(x)!) out.unshift(x);
        return out;
      }
      if (d >= most || !open(c)) continue;
      prev.set(c, at); depth.set(c, d + 1);
      queue.push(c);
    }
  }
  return null;
}

/**
 * How goods would go between two countries. Across a border between their home ground if they have one. Otherwise
 * by whichever open way leaves most of the agreement's worth: by sea, by a border between their colonies, or
 * overland across countries at peace with both.
 */
export function tradeRoute(g: Game, a: string, b: string, ctx: TradeCtx = new Map()): TradeRoute {
  let colony = -1;
  for (const r of g.regions) {
    if (r.ctrl !== a) continue;
    for (const j of r.nb) {
      const o = g.regions[j];
      if (o.ctrl !== b) continue;
      if (!r.overseas && !o.overseas) return { way: 'land', seas: [], lost: 0, keeps: 1 };
      if (colony < 0) colony = r.id;
    }
  }
  const ways: TradeRoute[] = [];
  if (colony >= 0) ways.push({ way: 'land', seas: [], lost: 0, keeps: TRADE.colony, via: colony });
  else {
    const through = landWay(g, a, b, ctx);
    if (through?.length) ways.push({ way: 'land', seas: [], lost: 0, keeps: Math.max(TRADE.farLeast, 1 - TRADE.transit.toll * through.length), through });
  }
  const sea = seaRoute(g, a, b, ctx);
  if (sea.way === 'sea') ways.push(sea);
  let best = NONE;
  for (const w of ways) if (w.keeps * (1 - w.lost) > best.keeps * (1 - best.lost) + 1e-9) best = w;
  return best;
}

function seaRoute(g: Game, a: string, b: string, ctx: TradeCtx): TradeRoute {
  const A = seaView(g, a, ctx), B = seaView(g, b, ctx);
  if (!A.from.length || !B.from.length) return NONE;
  const N = g.seas.length;
  const shut = (z: number): boolean => A.shut[z] || B.shut[z];
  const risk = (z: number): number => Math.max(A.risk[z], B.risk[z]);
  const cost = new Array<number>(N).fill(Infinity), prev = new Array<number>(N).fill(-1), done = new Array<boolean>(N).fill(false);
  for (const z of A.from) if (!shut(z)) cost[z] = 1 + 20 * risk(z);
  for (;;) {
    let at = -1;
    for (let i = 0; i < N; i++) if (!done[i] && cost[i] < Infinity && (at < 0 || cost[i] < cost[at])) at = i;
    if (at < 0) break;
    done[at] = true;
    for (const j of g.seas[at].adj) {
      if (done[j] || shut(j) || !canPass(g, at, j, a) || !canPass(g, at, j, b)) continue;
      const c = cost[at] + 1 + 20 * risk(j);
      if (c < cost[j]) { cost[j] = c; prev[j] = at; }
    }
  }
  let end = -1;
  for (const z of B.from) if (cost[z] < Infinity && (end < 0 || cost[z] < cost[end])) end = z;
  if (end < 0) return NONE;
  const seas: number[] = [];
  for (let z = end; z >= 0; z = prev[z]) seas.unshift(z);
  let safe = 1;
  for (const z of seas) safe *= 1 - risk(z);
  return { way: 'sea', seas, lost: Math.min(TRADE.sea.most, (1 - safe) * TRADE.sea.loss), keeps: farWorth(seas.length) };
}

// ---- Who will sign ----------------------------------------------------------------------------------

const side = (n: Nation): number => { const b = bloc(n); return b === 'axis' ? 1 : b === 'allies' ? -1 : 0; };

/** True when the people of one country (outside both alliances) will not hear of trade with the other's alliance. */
function cold(a: Nation, b: Nation): boolean {
  const sa = side(a), sb = side(b);
  if (sa !== 0 && bloc(b) === null && b.opinion * sa <= -TRADE.cold) return true;
  if (sb !== 0 && bloc(a) === null && a.opinion * sb <= -TRADE.cold) return true;
  return false;
}

/** What an agreement between two countries would be worth to each a month, before either's own commerce is counted. 'keeps' is the share the way the goods go leaves of it (a route's 'keeps'). */
export function dealWorth(g: Game, a: string, b: string, keeps = 1): number {
  return (TRADE.base + TRADE.scale * Math.sqrt(Math.max(0, wealthOf(g, a)) * Math.max(0, wealthOf(g, b)))) * keeps;
}

const barred = (a: string, b: string): boolean => TRADE.never.some(([x, y]) => (x === a && y === b) || (x === b && y === a));

/** Why two countries cannot sign a trade agreement now; null when they can. Written for the first of the two. */
export function whyNoTrade(g: Game, a: string, b: string, ctx: TradeCtx = new Map(), ownHouse = true): string | null {
  const A = g.nations[a], B = g.nations[b];
  const the = (n: Nation): string => (/^(United|Soviet|Netherlands)/.test(n.name) ? `the ${n.name}` : n.name);
  const The = (n: Nation): string => { const s = the(n); return s[0].toUpperCase() + s.slice(1); };
  if (!tradeRules(g)) return 'Trade agreements are signed in the grand campaign.';
  if (!A || !B || a === b) return 'There is nobody to sign with.';
  if (!can(A)) return `${A.name} is in no state to trade.`;
  if (!can(B)) return B.dormant ? `${The(B)} keeps to itself for now.` : `${The(B)} has fallen.`;
  if (atWar(g, a, b)) return `You are at war with ${the(B)}.`;
  if (dealOf(g, a, b)) return `You already trade with ${the(B)}.`;
  if (barred(a, b)) return `${The(B)} will not sign with you.`;
  if (cold(A, B)) return `Feeling in ${bloc(B) === null ? the(B) : the(A)} runs too strongly against it.`;
  if (ownHouse && dealsOf(g, a).length >= housesOf(g, a)) return 'Every trading house you have is in use. A merchant quarter in a harbour town or your capital opens another.';
  if (dealsOf(g, b).length >= housesOf(g, b)) return `${The(B)} has no trading house free.`;
  if (tradeRoute(g, a, b, ctx).way === 'none') return `No way is open for goods between you and ${the(B)}: no border, no ground at peace with you both between, and the sea is closed.`;
  return null;
}

// ---- Signing and ending -----------------------------------------------------------------------------

function refresh(g: Game, d: TradeDeal, ctx: TradeCtx): void {
  const r = tradeRoute(g, d.a, d.b, ctx);
  d.worth = dealWorth(g, d.a, d.b, r.way === 'none' ? 1 : r.keeps);
  d.way = r.way === 'none' ? d.way : r.way;
  d.seas = r.seas;
  if (r.via !== undefined) d.via = r.via; else delete d.via;
  if (r.through?.length) d.through = r.through; else delete d.through;
  d.lost = r.way === 'none' ? 1 : r.lost;
  d.state = r.way === 'none' ? 'cut' : r.lost >= TRADE.sea.raided ? 'raided' : 'open';
}

/** Signs an agreement between two countries. The player pays political capital for one he signs himself. */
export function signTrade(g: Game, a: string, b: string, ctx: TradeCtx = new Map(), free = false): TradeDeal | null {
  if (whyNoTrade(g, a, b, ctx) !== null) return null;
  const A = g.nations[a];
  if (!free && a === g.human) {
    if (A.pc < TRADE.sign.pc) return null;
    A.pc -= TRADE.sign.pc;
  }
  const d: TradeDeal = { a, b, since: g.turn, worth: 0, way: 'land', seas: [], lost: 0, state: 'open' };
  refresh(g, d, ctx);
  g.trade.push(d);
  return d;
}

/** Ends the agreement between two countries, if there is one. */
export function endTrade(g: Game, a: string, b: string): boolean {
  const d = dealOf(g, a, b);
  if (!d) return false;
  g.trade = g.trade.filter((x) => x !== d);
  return true;
}

/** What a nation's agreements pay it this month, with its own commerce counted. */
export function tradeIncome(g: Game, nat: string): number {
  if (!tradeRules(g) || !g.trade.length) return 0;
  let sum = 0;
  for (const d of g.trade) if ((d.a === nat || d.b === nat) && d.state !== 'cut') sum += d.worth * (1 - d.lost);
  return sum > 0 ? sum * (1 + civicSum(g, nat).trade) * govMul(g, nat, 'trade') : 0;
}

/** The partner that would pay a country most, of those that would sign now. */
const byWorth = (g: Game, a: string, ctx: TradeCtx, skip: (b: string) => boolean): string | null => {
  let best: string | null = null, top = 0;
  for (const B of Object.values(g.nations)) {
    if (B.id === a || !can(B) || skip(B.id)) continue;
    if (whyNoTrade(g, a, B.id, ctx) !== null) continue;
    const r = tradeRoute(g, a, B.id, ctx);
    const pays = dealWorth(g, a, B.id, r.keeps) * (1 - r.lost);
    if (pays > top) { top = pays; best = B.id; }
  }
  return best;
};

/**
 * A nation's month of trade, at the start of its turn: agreements that war, a fall or bad feeling have ended are
 * struck out, the rest are looked over (what they are worth, how the goods go, what is sunk), and a computer-run
 * country with a free trading house signs with the best partner it can find.
 */
export function tradeTick(g: Game, nat: string): void {
  if (!tradeRules(g)) return;
  const n = g.nations[nat];
  if (!n) return;
  const ctx: TradeCtx = new Map();
  for (const d of dealsOf(g, nat)) {
    const o = other(d, nat), O = g.nations[o];
    const dead = !can(n) || !can(O), war = atWar(g, nat, o), sour = !dead && !war && cold(n, O);
    if (dead || war || sour) {
      endTrade(g, d.a, d.b);
      const mine = g.human === nat || g.human === o;
      if (mine) {
        const them = g.nations[g.human === nat ? o : nat].name;
        headline(g, 'info', war ? `War ends your trade with ${them}.` : dead ? `Your trade with ${them} is at an end.` : `${them} breaks off its trade agreement with you: feeling there runs too strongly against it.`, g.human!);
      }
      continue;
    }
    const was = d.state;
    refresh(g, d, ctx);
    // The player hears of it whichever of the two countries' months finds the way closed.
    if (d.state === 'cut' && was !== 'cut' && (nat === g.human || o === g.human)) headline(g, 'info', `No goods are getting through from ${(nat === g.human ? O : n).name}: the way is closed to your trade.`, g.human!);
    // Two computer-run countries give up an agreement nothing is getting through, and look elsewhere.
    else if (d.state === 'cut' && nat !== g.human && o !== g.human) endTrade(g, d.a, d.b);
  }
  if (nat === g.human || !can(n) || n.capital < 0) return;
  for (let k = 0; k < 2 && dealsOf(g, nat).length < housesOf(g, nat); k++) {
    const b = byWorth(g, nat, ctx, (id) => id === g.human);
    if (!b || !signTrade(g, nat, b, ctx, true)) break;
  }
}

/** When the grand campaign opens: the agreements history had standing, then every country fills its houses with the best partners it can find. */
export function openTrade(g: Game): void {
  if (!tradeRules(g) || g.flags['trade:open']) return;
  g.flags['trade:open'] = 1;
  const ctx: TradeCtx = new Map();
  for (const [a, b] of TRADE.pairs) signTrade(g, a, b, ctx, true);
  const order = Object.values(g.nations).filter(can).sort((x, y) => wealthOf(g, y.id) - wealthOf(g, x.id) || (x.id < y.id ? -1 : 1));
  for (let round = 0; round < TRADE.houses.most; round++) {
    let signed = false;
    for (const n of order) {
      if (dealsOf(g, n.id).length >= housesOf(g, n.id)) continue;
      const b = byWorth(g, n.id, ctx, () => false);
      if (b && signTrade(g, n.id, b, ctx, true)) signed = true;
    }
    if (!signed) break;
  }
}

// ---- The books an interface shows -------------------------------------------------------------------

/** The line an agreement's goods follow on the map: capital to capital overland, harbour to harbour through the middle of each sea. Empty when no way is open. */
export function tradePath(g: Game, d: TradeDeal): [number, number][] {
  if (d.state === 'cut' || d.way === 'none') return [];
  const seat = (nat: string): Region | null => {
    const c = g.nations[nat]?.capital ?? -1;
    if (c >= 0 && g.regions[c]?.ctrl === nat) return g.regions[c];
    return g.regions.find((r) => r.ctrl === nat && r.owner === nat) ?? g.regions.find((r) => r.ctrl === nat) ?? null;
  };
  const A = seat(d.a), B = seat(d.b);
  if (d.way === 'land') {
    // A border between colonies: the line runs from the colony, not from the far-off capital.
    const from = d.via !== undefined && g.regions[d.via] ? g.regions[d.via] : A;
    const to = d.via !== undefined && from ? g.regions[from.nb.find((j) => g.regions[j].ctrl === d.b) ?? -1] ?? B : B;
    if (!from || !to) return [];
    // Across other countries the line runs by their capitals.
    const mid = (d.through ?? []).map(seat).filter((r): r is Region => !!r).map((r): [number, number] => [r.x, r.y]);
    return [[from.x, from.y], ...mid, [to.x, to.y]];
  }
  if (!d.seas.length) return [];
  const harbour = (nat: string, z: number, home: Region | null): Region | null => {
    let best: Region | null = null, far = Infinity;
    for (const id of g.seas[z].coast) {
      const r = g.regions[id];
      if (!r.port || r.ctrl !== nat) continue;
      const dist = home ? Math.hypot(r.x - home.x, r.y - home.y) : 0;
      if (dist < far) { far = dist; best = r; }
    }
    return best;
  };
  const pts: [number, number][] = [];
  const ha = harbour(d.a, d.seas[0], A), hb = harbour(d.b, d.seas[d.seas.length - 1], B);
  if (ha) pts.push([ha.x, ha.y]);
  for (const z of d.seas) pts.push([g.seas[z].x, g.seas[z].y]);
  if (hb) pts.push([hb.x, hb.y]);
  return pts.length >= 2 ? pts : [];
}

export interface TradeRow {
  nat: string;
  name: string;
  /** What it pays this nation a month now, and what it would with nothing sunk. */
  pays: number;
  full: number;
  way: 'land' | 'sea' | 'none';
  seas: number[];
  /** In words: 'Overland', 'By sea through the Baltic Sea'. */
  route: string;
  state: 'open' | 'raided' | 'cut';
  lost: number;
  since: number;
  /** The line to draw on the map, as points. */
  path: [number, number][];
}

export interface TradeOffer {
  nat: string;
  name: string;
  /** What it would pay this nation a month. */
  pays: number;
  way: 'land' | 'sea' | 'none';
  route: string;
  /** Why it cannot be signed now, or null. 'poor' is true when political capital is the only thing in the way, 'full' when only a free trading house of the nation's own is wanting. */
  why: string | null;
  poor: boolean;
  full: boolean;
}

export interface TradeBook {
  nat: string;
  /** Trading houses in use and in all, and the most any country can have. */
  used: number;
  houses: number;
  most: number;
  /** What all the agreements pay a month, and the share its commerce buildings add to that. */
  income: number;
  boost: number;
  /** What signing costs in political capital, and what the nation has. */
  cost: number;
  pc: number;
  deals: TradeRow[];
  offers: TradeOffer[];
}

const routeWords = (g: Game, r: { way: 'land' | 'sea' | 'none'; seas: number[]; via?: number; through?: string[] }): string => {
  if (r.way === 'land') {
    if (r.through?.length) { const ns = r.through.map((c) => g.nations[c]?.name ?? c); return `Overland across ${ns.length <= 1 ? ns.join('') : `${ns.slice(0, -1).join(', ')} and ${ns[ns.length - 1]}`}`; }
    return r.via !== undefined && g.regions[r.via] ? `Overland, by way of ${g.regions[r.via].name}` : 'Overland';
  }
  if (r.way === 'none' || !r.seas.length) return 'No way open';
  const names = r.seas.map((z) => `the ${g.seas[z].name}`);
  return `By sea through ${names.length <= 1 ? names.join('') : `${names.slice(0, -1).join(', ')} and ${names[names.length - 1]}`}`;
};

/** A nation's trade: its agreements, and every country it could sign with, the richest prospect first. */
export function tradeBook(g: Game, nat: string): TradeBook | null {
  const n = g.nations[nat];
  if (!tradeRules(g) || !n) return null;
  const ctx: TradeCtx = new Map();
  const boost = civicSum(g, nat).trade;
  const deals: TradeRow[] = dealsOf(g, nat).map((d) => {
    const o = other(d, nat);
    return {
      nat: o, name: g.nations[o].name, pays: d.state === 'cut' ? 0 : d.worth * (1 - d.lost) * (1 + boost), full: d.worth * (1 + boost),
      way: d.way, seas: d.seas, route: d.state === 'cut' ? 'No way open' : routeWords(g, d), state: d.state, lost: d.lost, since: d.since, path: tradePath(g, d),
    };
  }).sort((x, y) => y.full - x.full || (x.nat < y.nat ? -1 : 1));
  const offers: TradeOffer[] = [];
  for (const B of Object.values(g.nations)) {
    if (B.id === nat || !can(B) || dealOf(g, nat, B.id)) continue;
    let why = whyNoTrade(g, nat, B.id, ctx);
    const full = why !== null && whyNoTrade(g, nat, B.id, ctx, false) === null;
    const r = atWar(g, nat, B.id) ? NONE : tradeRoute(g, nat, B.id, ctx);
    const poor = why === null && n.pc < TRADE.sign.pc;
    if (poor) why = `Needs ${TRADE.sign.pc} political capital.`;
    offers.push({ nat: B.id, name: B.name, pays: dealWorth(g, nat, B.id, r.way === 'none' ? 1 : r.keeps) * (1 - r.lost) * (1 + boost), way: r.way, route: routeWords(g, r), why, poor, full });
  }
  const near = (o: TradeOffer): number => (o.why === null || o.poor || o.full ? 0 : 1);
  offers.sort((x, y) => near(x) - near(y) || y.pays - x.pays || (x.nat < y.nat ? -1 : 1));
  return { nat, used: deals.length, houses: housesOf(g, nat), most: TRADE.houses.most, income: tradeIncome(g, nat), boost, cost: TRADE.sign.pc, pc: n.pc, deals, offers };
}
