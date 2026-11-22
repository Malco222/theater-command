// A neutral's bargains and the volunteer legions, as the 3D game's campaign map draws them (release 2; build contract
// production/r2-bargains-legions/contract.md, part B).
//
// Both are switched off (data/campaign/stage.json 'bargains' and 'legions'), and while they are the map asks one
// question, TC.bargainsLook(), and is answered exactly {"on":false}. Nothing here decides anything: it reads the
// rules' own pictures (bargains.ts bargainsView, legions.ts legionsView) and adds what a map needs and the rules do
// not say: the names of the countries named, who the buyer is to the player, whether the player has an agent in
// either capital, the line the goods follow, and where a lane's goods are lost or cut off.
//
//   TC.bargainsLook()             -> { on: false } or { on, viewer, turn, month, term, round, scale, nations, rows, legions }
//   TC.bargainBid(id, pp)         -> { ok, note, view }: the player bids for a bargain whose round is open
//   TC.bargainSell(id, buyer|null)-> { ok, note, view }: the player, as the seller, chooses a buyer or nobody
//
// campaign-bridge.ts adds these to TC with Object.assign(TC, bargainCalls(game, view)).

import { BARGAINS, bargainRules, bargainsView, bidSteps, rowById, type BargainView } from '../../../src/campaign/bargains';
import { legionRules, legionsView, type LegionView } from '../../../src/campaign/legions';
import { allied, atWar, bloc, newsName, theName } from '../../../src/campaign/game';
import { tradePath, tradeRoute, type TradeCtx } from '../../../src/campaign/trade';
import { NAVY, oreRoute, zoneOf } from '../../../src/campaign/navy';
import { BUDGET } from '../../../src/campaign/economy';
import { mapDist } from '../../../src/campaign/wrap';
import { orderBid, orderSellTo } from '../../../src/campaign/turn';
import { turnToDate } from '../../../src/core/ticket';
import { MEN_PER_DIV, type Game, type Region, type TradeDeal } from '../../../src/campaign/types';

/** A sea on the map: its index, its name and its middle, where a lane's loss or cut is marked. */
export interface SeaMark { sea: number; name: string; x: number; y: number }

/** One bargain as the map reads it: the rules' row, and what the map adds. */
export interface LookRow extends BargainView {
  /** One of the generic rows, "its exports". */
  generic: boolean;
  /** The name of the seller's capital province, or ''. */
  capitalName: string;
  /** Who the buyer is to the viewer. */
  rel: 'none' | 'self' | 'ally' | 'enemy' | 'other';
  /** True while an agent of the viewer's is at work in the seller's capital, or in the buyer's. */
  agentSeller: boolean;
  agentBuyer: boolean;
  /** The line the goods follow, as x, y pairs in map units, seller first. Empty where nothing is drawn. */
  path: number[];
  /** The sea where most goods are lost, while some are. */
  loss: SeaMark | null;
  /** For a lane cut off at sea: where (the sea it was losing goods in, or its middle sea) and the month it was first seen cut. */
  cut: (SeaMark & { month: string }) | null;
}

/** A legion as the map reads it: the rules' row, and its home's capital, where it stands, how strong, and how far into its tour. */
export interface LookLegion extends LegionView {
  homeCapital: number;
  /** The name of the province it stands in, or '' on its way. */
  place: string;
  /** Its men as a share of its full size, in hundredths. */
  pct: number;
  /** Its tour in months, and which month of it this is (1 to tour). */
  tour: number;
  tourAt: number;
}

export interface LookNation { name: string; the: string; adj: string; bloc: string }

export type Look =
  | { on: false }
  | { on: true; viewer: string; turn: number; month: string; term: number; round: number; scale: number; nations: Record<string, LookNation>; rows: LookRow[]; legions: LookLegion[] };

// ---- The lanes cut off at sea ----------------------------------------------------------------------------------
//
// A cut lane has no way and no seas in the rules (its goods do not move), so the map remembers the last sea path it
// saw for each bargain and its buyer, and marks the cut on it. The memory belongs to this run of the game alone, not
// to the campaign: it is not saved, and a lane cut before a load is not drawn after it.

interface Kept { buyer: string; path: number[]; seas: number[]; loss: number; cutAt: number }
const KEPT = new WeakMap<Game, Map<string, Kept>>();

const r1 = (v: number): number => Math.round(v * 10) / 10;
const monthName = (turn: number): string => turnToDate(turn).label.split(' ')[0];

function seaMark(g: Game, z: number): SeaMark | null {
  const s = g.seas[z];
  return s ? { sea: z, name: s.name, x: r1(s.x), y: r1(s.y) } : null;
}

/** The country's seat for a lane: its capital while it holds it, else a province of its own. */
function seat(g: Game, nat: string): Region | null {
  const c = g.nations[nat]?.capital ?? -1;
  if (c >= 0 && g.regions[c]?.ctrl === nat) return g.regions[c];
  return g.regions.find((r) => r.ctrl === nat && r.owner === nat) ?? g.regions.find((r) => r.ctrl === nat) ?? null;
}

/** The country's harbour on a sea nearest its seat: one of its own ground first, then any it holds. */
function harbour(g: Game, nat: string, z: number): Region | null {
  const home = seat(g, nat);
  let best: Region | null = null, far = Infinity;
  for (const id of g.seas[z]?.coast ?? []) {
    const r = g.regions[id];
    if (!r?.port || r.ctrl !== nat) continue;
    const d = (home ? mapDist(g, r.x, r.y, home.x, home.y) : 0) + (r.owner === nat ? 0 : 1e6);
    if (d < far) { far = d; best = r; }
  }
  return best;
}

/** The fewest seas from a province's coast to a sea where the country has a harbour. */
function seasFrom(g: Game, from: Region, nat: string): number[] {
  const N = g.seas.length, prev = new Array<number>(N).fill(-2), queue: number[] = [];
  for (let z = 0; z < N; z++) if (g.seas[z].coast.includes(from.id)) { prev[z] = -1; queue.push(z); }
  for (let k = 0; k < queue.length; k++) {
    const z = queue[k];
    if (harbour(g, nat, z)) { const out: number[] = []; for (let at = z; at >= 0; at = prev[at]) out.unshift(at); return out; }
    for (const n of g.seas[z].adj) if (prev[n] === -2) { prev[n] = z; queue.push(n); }
  }
  return [];
}

/** Germany's Swedish ore as it goes this month (navy.json 'ore'): across the Baltic in summer, down from Narvik in winter. */
function orePath(g: Game, buyer: string): { path: number[]; seas: number[] } | null {
  const way = oreRoute(g), O = NAVY.ore, src = g.byName[O.source];
  if (!src || (way !== 'baltic' && way !== 'narvik')) return null;
  const pts: number[] = [src.x, src.y];
  let seas: number[];
  if (way === 'baltic') seas = O.summerZones.map((k) => zoneOf(g, k)).filter((z) => z >= 0);
  else {
    const port = g.byName[O.winterPort];
    if (!port) return null;
    pts.push(port.x, port.y);
    seas = seasFrom(g, port, buyer);
  }
  if (!seas.length) return null;
  for (const z of seas) pts.push(g.seas[z].x, g.seas[z].y);
  const end = harbour(g, buyer, seas[seas.length - 1]);
  if (end) pts.push(end.x, end.y);
  return { path: pts.map(r1), seas };
}

/** The line a bargain's goods follow this month, and the seas it crosses. Empty where no way is open or it ships nothing. */
function laneOf(g: Game, v: BargainView, ctx: TradeCtx): { path: number[]; seas: number[] } {
  const none = { path: [], seas: [] };
  if (!v.buyer || v.base >= 0 || v.way === 'none') return none;
  if (rowById(g, v.id)?.replaces === 'ore' && v.buyer === NAVY.ore.to) {
    const ore = orePath(g, v.buyer);
    if (ore) return ore;
  }
  const deal: TradeDeal = { a: v.seller, b: v.buyer, since: 0, worth: 0, way: v.way, seas: v.seas, lost: 0, state: 'open' };
  if (v.way === 'land') {
    // (The rules keep no record of the countries a land way crosses: asked again, as trade.ts draws an agreement's.)
    const r = tradeRoute(g, v.seller, v.buyer, ctx);
    if (r.way === 'land') { deal.via = r.via; deal.through = r.through; }
  }
  return { path: tradePath(g, deal).flat().map(r1), seas: v.way === 'sea' ? v.seas : [] };
}

// ---- The look ------------------------------------------------------------------------------------------------------

const relOf = (g: Game, viewer: string | null, buyer: string | null): LookRow['rel'] => {
  if (!buyer) return 'none';
  if (!viewer) return 'other';
  if (buyer === viewer) return 'self';
  if (atWar(g, viewer, buyer)) return 'enemy';
  return allied(g, viewer, buyer) ? 'ally' : 'other';
};

/** True while an agent of the viewer's is at work in that country's capital: the rules' own test for seeing a bid (bargains.ts agentIn). */
function agentIn(g: Game, viewer: string | null, nat: string | null): boolean {
  const cap = nat ? g.nations[nat]?.capital ?? -1 : -1;
  if (!viewer || cap < 0) return false;
  return (g.intel?.agents ?? []).some((a) => a.nat === viewer && a.ready <= g.turn && a.reg === cap);
}

/** The bargains and legions as the viewer's map shows them. Asks the rules only; changes nothing of the campaign. */
export function bargainsLookOf(g: Game, viewer: string | null = g.human): Look {
  if (!bargainRules(g) && !legionRules(g)) return { on: false };
  const kept = KEPT.get(g) ?? new Map<string, Kept>();
  KEPT.set(g, kept);
  const ctx: TradeCtx = new Map();
  const named = new Set<string>(viewer ? [viewer] : []);
  const rows: LookRow[] = bargainsView(g, viewer).map((v) => {
    named.add(v.seller);
    if (v.buyer) named.add(v.buyer);
    for (const b of v.round?.bids ?? []) named.add(b.nat);
    let { path, seas } = laneOf(g, v, ctx);
    let cut: LookRow['cut'] = null;
    if (v.state === 'cut' && v.buyer) {
      const k = kept.get(v.id);
      if (k && k.buyer === v.buyer) {
        if (k.cutAt < 0) k.cutAt = g.turn;
        path = k.path;
        const at = seaMark(g, k.loss >= 0 ? k.loss : k.seas[Math.floor(k.seas.length / 2)]);
        cut = at ? { ...at, month: monthName(k.cutAt) } : null;
      } else if (seas.length && path.length >= 4) {
        // (A sea way that loses every cargo: the rules still name its seas.)
        const at = seaMark(g, v.lossAt >= 0 ? v.lossAt : seas[Math.floor(seas.length / 2)]);
        cut = at ? { ...at, month: monthName(g.turn) } : null;
        kept.set(v.id, { buyer: v.buyer, path, seas, loss: v.lossAt, cutAt: g.turn });
      } else path = [];
    } else if (v.buyer && seas.length && path.length >= 4) kept.set(v.id, { buyer: v.buyer, path, seas, loss: v.lossAt, cutAt: -1 });
    return {
      ...v,
      generic: !!rowById(g, v.id)?.generic,
      capitalName: g.regions[v.capital]?.name ?? '',
      rel: relOf(g, viewer, v.buyer),
      agentSeller: agentIn(g, viewer, v.seller),
      agentBuyer: agentIn(g, viewer, v.buyer),
      path,
      loss: v.lossAt >= 0 && v.state !== 'cut' ? seaMark(g, v.lossAt) : null,
      cut,
    };
  });
  const legions: LookLegion[] = legionsView(g).map((l) => {
    named.add(l.home); named.add(l.patron); named.add(l.against);
    const tour = Math.max(1, l.until - l.since + 1);
    return {
      ...l,
      homeCapital: g.nations[l.home]?.capital ?? -1,
      place: l.region >= 0 ? g.regions[l.region]?.name ?? '' : '',
      pct: l.est > 0 ? Math.round((100 * l.men) / (l.est * MEN_PER_DIV)) : 0,
      tour,
      tourAt: Math.max(1, Math.min(tour, tour - l.monthsLeft + 1)),
    };
  });
  const nations: Record<string, LookNation> = {};
  for (const tag of [...named].sort()) {
    const n = g.nations[tag];
    if (!n) continue;
    const name = newsName(n);
    nations[tag] = { name, the: theName({ ...n, name }), adj: n.adj, bloc: bloc(n) ?? '' };
  }
  // (scale: how the game shows money, budget.json's, so that the card's bids read as the rules' notes and the treasury do)
  return { on: true, viewer: viewer ?? '', turn: g.turn, month: monthName(g.turn), term: BARGAINS.term, round: BARGAINS.round, scale: BUDGET.scale, nations, rows, legions };
}

// ---- The orders ----------------------------------------------------------------------------------------------------

const money = (pp: number): string => `${Math.round(pp * BUDGET.scale)}`;

/** The step of the player's bid nearest the figure asked: a bid is always one of the round's own steps. */
function stepNear(g: Game, pp: number): number | null {
  const steps = g.human ? bidSteps(g, g.human) : [];
  let best: number | null = null;
  for (const s of steps) if (best === null || Math.abs(s - pp) < Math.abs(best - pp)) best = s;
  return best;
}

/** The three calls, for campaign-bridge.ts to add to TC: `game` gives the campaign in play, `view` the whole map's picture. */
export function bargainCalls(game: () => Game, view: () => unknown) {
  return {
    /** The bargains and legions as the player's map shows them, or exactly { on: false } while both are switched off. */
    bargainsLook: (): Look => {
      const g = game();
      return bargainsLookOf(g, g.human);
    },
    /** The player bids for a bargain whose round is open; the figure is taken to the nearest of his steps. */
    bargainBid: (id: string, pp: number) => {
      const g = game();
      const row = rowById(g, id), step = Number.isFinite(Number(pp)) ? stepNear(g, Number(pp)) : null;
      const why = step === null ? 'No bid can be made for it.' : orderBid(g, id, step);
      const note = why ?? `Your bid stands: ${money(step!)} a month for ${row?.name ?? 'the bargain'}.`;
      return { ok: why === null, note, view: view() };
    },
    /** The player, as the seller, chooses a buyer from the bids that stand, or nobody (null). */
    bargainSell: (id: string, buyer: string | null) => {
      const g = game();
      const h0 = g.headlines.length;
      const why = orderSellTo(g, id, buyer || null);
      const news = g.headlines.slice(h0).map((h) => h.text);
      const row = rowById(g, id);
      const note = why ?? (news.length ? news.join(' ') : buyer ? `Sold to ${theName(g.nations[buyer])}.` : `You sell ${row?.name ?? 'it'} to nobody this year.`);
      return { ok: why === null, note, view: view() };
    },
  };
}
