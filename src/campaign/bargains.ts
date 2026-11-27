// A neutral's bargains (release 2; Brian's yes of 22 November 2026, the design director's frame of 03:42 UTC and
// rulings of 03:47 and 03:55 UTC; the record is the historian's table, historian/neutrals-bargains-1939-45.md).
//
// A country at peace outside both alliances has goods or a right the countries at war want: Swedish iron ore, Turkish
// chrome, Spanish and Portuguese wolfram, Argentine beef, the Azores, and every other neutral's exports. It sells each
// to one buyer a term at a time. When no term runs, or a term is ending, or its buyer can no longer take it, a round of
// bids opens for a month: each country at war that can take the goods bids money a month, and the seller's seat
// chooses. A computer-run seller weighs the price against its own leaning, who is winning and who stands at its
// border (at stops 1 and 2 it sells to the record's buyer of the date); a seller in the player's hands chooses for
// himself, and nothing is chosen for him while he has not. The price of a sale is paid in coins the game already has:
// the buyer's money, the seller's opinion moving toward the buyer's side, and each enemy great power's patience with
// the seller, which at stops 4 and 5 can become a claim on its ground (claims.ts).
//
// Nothing here is a second economy. The money is the treasury's (economy.ts income and upkeep), the goods are
// resources.json's supply and oil (resources.ts lanesTo), the way is trade.ts's tradeRoute, a base is an airfield the
// buyer may use (prizes.ts airfieldOpenTo, air.ts fieldClass), and a good a country's industry depends on is a share
// of its production, as navy.json's Swedish ore always was (navy.ts seaIncomeLoss).
//
// The whole of it is switched by stage.json 'bargains': with it off no campaign carries the flag 'bargains', nothing
// here runs, and every war is release 1's. The numbers and rows live in data/campaign/bargains.json.

import data from '../../data/campaign/bargains.json';
import { allied, atWar, bloc, headline, theName, warning } from './game';
import { momentum, majorsOf } from './diplomacy';
import { TRADE, tradeRoute, wealthOf, type TradeCtx } from './trade';
import { RES, keepsStores, needOf, type Lane } from './resources';
import { NAVY, oreRoute } from './navy';
import { BUDGET, income } from './economy';
import { stopOf } from './history';
import { ownPath, small } from './league';
import { BARGAINS_STAGE } from './stage';
import { turnToDate } from '../core/ticket';
import { fronts } from './staff';
import { knowsBooks } from './intel';
import { LEGIONS, legionFeeds } from './legions';
import type { BargainBook, BargainHeld, Game, Nation } from './types';

export interface BargainRow {
  id: string;
  seller: string;
  name: string;
  sign: string;
  gives: { sup?: number; oil?: number; base?: string; field?: number };
  sizes?: [number, number][];
  output?: Record<string, number>;
  lag?: number;
  replaces?: string;
  carve?: { lane: string; share: number };
  inLane?: string[];
  record?: [number, number, string | null][];
  generic?: boolean;
}

export const BARGAINS = data as unknown as {
  term: number; round: number; cutMonths: number; asMoney: number;
  generic: { share: number; sign: string; never: string[]; names: Record<string, string>; oil: string[]; inLane: Record<string, string[]>; record: Record<string, [number, number, string | null][]> };
  judge: { lean: number; winning: number; fear: number };
  bid: { unit: number; deny: number; share: number; floor: number; most: number; step: number };
  pull: number;
  patience: { fall: number; heal: number; half: number };
  rows: BargainRow[];
};

/** True in a grand campaign opened with the bargains switched on. */
export const bargainRules = (g: Game): boolean => g.flags.open === 1 && g.flags.bargains === 1 && !!g.bargains;

const book = (g: Game): BargainBook => g.bargains!;
const r2 = (x: number): number => Math.round(x * 100) / 100;
const side = (n: Nation | undefined): number => { const b = n ? bloc(n) : null; return b === 'axis' ? 1 : b === 'allies' ? -1 : 0; };
const The = (n: Nation): string => theName(n, true);

// ---- The rows ------------------------------------------------------------------------------------------------

const ROWS_CACHE = new WeakMap<Record<string, number>, BargainRow[]>();

/** Every bargain of this campaign: the named rows, then each other neutral's exports. */
export function rowsOf(g: Game): BargainRow[] {
  if (!bargainRules(g)) return [];
  const gen = book(g).generic;
  let rows = ROWS_CACHE.get(gen);
  if (rows) return rows;
  const G = BARGAINS.generic;
  rows = [...BARGAINS.rows];
  for (const [seller, amount] of Object.entries(gen)) {
    const n = g.nations[seller];
    if (!n) continue;
    const oil = G.oil.includes(seller);
    rows.push({
      id: `gen-${seller}`, seller, name: G.names[seller] ?? `${n.adj} exports`, sign: oil ? 'oil' : G.sign,
      gives: oil ? { oil: amount } : { sup: amount }, inLane: G.inLane[seller], record: G.record[seller], generic: true,
    });
  }
  ROWS_CACHE.set(gen, rows);
  return rows;
}

export const rowById = (g: Game, id: string): BargainRow | undefined => rowsOf(g).find((r) => r.id === id);

/** The state of a bargain, made the first time it is asked for. */
function held(g: Game, id: string): BargainHeld {
  const rows = book(g).rows;
  return (rows[id] ??= { buyer: null, price: 0, since: 0, until: 0, way: 'none', seas: [], lost: 0, share: 0 });
}

/** The bargain's state as it stands, or undefined before anything has happened to it. */
export const heldOf = (g: Game, id: string): BargainHeld | undefined => (bargainRules(g) ? book(g).rows[id] : undefined);

/** What the goods are a month now: supply by the record's year where it changed, and a carved share of a lane's supply. */
export function goodsOf(g: Game, row: BargainRow): { sup: number; oil: number } {
  let sup = row.gives.sup ?? 0;
  for (const [from, s] of row.sizes ?? []) if (g.turn >= from) sup = s;
  if (row.carve) {
    const lane = RES.lanes.find((l) => l.id === row.carve!.lane);
    if (lane && g.nations[lane.to]) sup += row.carve.share * lane.sup * needOf(g, lane.to, 'sup');
  }
  return { sup, oil: row.gives.oil ?? 0 };
}

/** The share of a sea lane's supply a bargain has carved out of it while its seller sells (resources.ts seaLane). */
export function carvedShare(g: Game, lane: string): number {
  if (!bargainRules(g)) return 0;
  let s = 0;
  for (const row of rowsOf(g)) if (row.carve?.lane === lane && sells(g, row.seller)) s += row.carve.share;
  return s;
}

/** True while a deal of resources.json follows a bargain's buyer instead of its own row: the bargain's seller sells. */
export function replacedDeal(g: Game, deal: string): boolean {
  if (!bargainRules(g)) return false;
  return rowsOf(g).some((row) => row.replaces === deal && sells(g, row.seller));
}

// ---- Who sells, who may buy -------------------------------------------------------------------------------------

/** True for a country that has bargains to sell: alive, at peace with everyone, outside both alliances. */
export function sells(g: Game, nat: string): boolean {
  const n = g.nations[nat];
  if (!n || !n.alive || n.dormant || n.atWar.length || n.capital < 0) return false;
  const b = bloc(n);
  return b === null || b === 'league';
}

/** The buyer the record names for this month at stops 1 and 2: a country, null for nobody, undefined where it names none. */
export function recordBuyer(g: Game, row: BargainRow, turn = g.turn): string | null | undefined {
  if (stopOf(g) > 2) return undefined;
  for (const [from, to, buyer] of row.record ?? []) if (turn >= from && turn <= to) return buyer;
  return undefined;
}

/** The first month of the record's entry that covers this month, if any. */
function recordFrom(row: BargainRow, turn: number): number | undefined {
  for (const [from, to] of row.record ?? []) if (turn >= from && turn <= to) return from;
  return undefined;
}

/** The deal a bargain replaces: its buyer may bid at peace, as it bought at peace before. */
const dealBuyer = (row: BargainRow): string | undefined => (row.replaces ? RES.deals.find((d) => d.id === row.replaces)?.to : undefined);

/** How the goods would go from the seller to this buyer this month. */
function routeOf(g: Game, row: BargainRow, buyer: string, ctx: TradeCtx): { way: 'land' | 'sea' | 'none'; seas: number[]; lost: number } {
  // Germany's ore goes as it always went: across the Baltic in summer, down from Narvik in winter (navy.json 'ore').
  if (row.replaces === 'ore' && buyer === NAVY.ore.to) {
    const way = oreRoute(g);
    return { way: way === 'cut' ? 'none' : way === 'none' ? 'land' : 'sea', seas: [], lost: 0 };
  }
  const r = tradeRoute(g, row.seller, buyer, ctx);
  // Ore that cannot reach Germany cannot cross it either: no other buyer takes it overland through German ground then.
  if (row.replaces === 'ore' && r.way === 'land' && r.through?.includes(NAVY.ore.to) && oreRoute(g) === 'cut') return { way: 'none', seas: [], lost: 1 };
  return { way: r.way, seas: r.seas, lost: r.way === 'none' ? 1 : r.lost };
}

/** True for the record's buyer of this month or next at stops 1 and 2: it keeps to the book while its way is shut, and its goods come when the way opens. */
const bookBuyer = (g: Game, row: BargainRow, buyer: string | null): boolean => !!buyer && (recordBuyer(g, row) === buyer || recordBuyer(g, row, g.turn + 1) === buyer);

/** Why a country cannot take this bargain now; null when it can bid for it. */
export function whyNoBargain(g: Game, row: BargainRow, buyer: string, ctx: TradeCtx = new Map()): string | null {
  const S = g.nations[row.seller], B = g.nations[buyer];
  if (!S || !B || buyer === row.seller) return 'There is nobody to buy it.';
  if (!B.alive || B.dormant) return `${The(B)} is in no state to buy.`;
  if (!sells(g, row.seller)) return `${The(S)} has nothing to sell while it is at war or in an alliance.`;
  const atWarNow = B.atWar.length > 0;
  // (The record's buyer bids at peace for the month it buys in: a round opened this month closes next month.)
  if (!atWarNow && dealBuyer(row) !== buyer && recordBuyer(g, row) !== buyer && recordBuyer(g, row, g.turn + 1) !== buyer) return 'Only a country at war bids for it.';
  if (atWar(g, buyer, row.seller)) return `${The(B)} is at war with ${theName(S)}.`;
  if (TRADE.never.some(([x, y]) => (x === buyer && y === row.seller) || (x === row.seller && y === buyer))) return `${The(S)} will not sell to ${theName(B)}.`;
  const s = side(B);
  if (s !== 0 && S.opinion * s <= -TRADE.cold) return `Feeling in ${theName(S)} runs too strongly against ${theName(B)}'s side.`;
  if (!bookBuyer(g, row, buyer) && routeOf(g, row, buyer, ctx).way === 'none') return `No way is open for the goods between ${theName(S)} and ${theName(B)}.`;
  return null;
}

/** Every country that could bid for this bargain now. */
export function biddersOf(g: Game, row: BargainRow, ctx: TradeCtx = new Map()): string[] {
  return Object.keys(g.nations).filter((b) => whyNoBargain(g, row, b, ctx) === null);
}

// ---- What a bargain is worth, and the computer's bid -------------------------------------------------------------

/** What a bargain is worth to a country a month, in production points: its goods, the production it keeps going, a base right at the floor. */
export function worthTo(g: Game, row: BargainRow, buyer: string): number {
  const B = BARGAINS.bid, goods = goodsOf(g, row);
  const amount = row.inLane?.includes(buyer) ? 0 : goods.sup + goods.oil;
  let w = keepsStores(g, buyer) ? amount * B.unit : amount * BARGAINS.asMoney;
  const out = row.output?.[buyer] ?? 0;
  if (out > 0) w += out * Math.max(0, income(g, buyer));
  if (row.gives.base) w += B.floor / B.share;
  return w;
}

/** The most a country bids a month: a share of its income. */
export const bidMost = (g: Game, buyer: string): number => r2(BARGAINS.bid.most * Math.max(0, income(g, buyer)));

/** A computer-run country's sealed bid for a round, or 0 for none: what the goods are worth to it and to an enemy that also bids. */
export function computerBid(g: Game, row: BargainRow, buyer: string, rivals: string[]): number {
  const B = BARGAINS.bid;
  let w = worthTo(g, row, buyer), deny = 0;
  for (const r of rivals) if (r !== buyer && atWar(g, r, buyer)) deny = Math.max(deny, worthTo(g, row, r));
  w += B.deny * deny;
  if (w < B.floor) return 0;
  const most = bidMost(g, buyer);
  if (most < B.floor) return 0;
  return r2(Math.min(Math.max(B.share * w, B.floor), most));
}

// ---- The computer seller's judgment ---------------------------------------------------------------------------------

/**
 * 1 when the seller's own staff, reading its map as if this buyer and the buyer's allies at war were its enemies
 * (staff.ts fronts), finds a front where the attack they could make from beyond its border is more than it could set
 * against it; 0 otherwise. A seller fears the side whose armies stand at its door (the design director's bound (b)).
 */
export function fearOf(g: Game, seller: string, buyer: string): number {
  const extra = new Set<string>([buyer]);
  for (const id of Object.keys(g.nations).sort()) if (id !== seller && allied(g, id, buyer) && g.nations[id].alive && g.nations[id].atWar.length) extra.add(id);
  // A seller with no army of its own has no staff to read the map (fronts() finds no front): it fears any army of theirs
  // standing at its border.
  if (!g.corps.some((c) => c.nat === seller && !c.dead)) {
    for (const r of g.regions) {
      if (r.owner !== seller || r.overseas) continue;
      for (const j of r.nb) {
        const R = g.regions[j];
        if (R.owner !== seller && R.units.some((u) => !u.dead && !u.hold && u.kind !== 'fort' && extra.has(u.nat))) return 1;
      }
    }
    return 0;
  }
  return fronts(g, seller, { extra }).some((f) => f.threat > f.atk + f.fixed) ? 1 : 0;
}

/** How a computer-run seller scores a bid: its price, weighed by its leaning, who is winning and who is at its border. */
export function bidScore(g: Game, row: BargainRow, buyer: string, price: number): number {
  const S = g.nations[row.seller], B = g.nations[buyer], J = BARGAINS.judge, s = side(B);
  const lean = Math.max(-1, Math.min(1, (S.opinion * s) / 100));
  const winning = momentum(g) * s;
  return price * (1 + J.lean * lean + J.winning * winning + J.fear * fearOf(g, row.seller, buyer));
}

/** The buyer a computer-run seller takes from a round's bids, or null for nobody. */
export function sellerChoice(g: Game, row: BargainRow, bids: Record<string, number>): string | null {
  const rb = recordBuyer(g, row);
  if (rb !== undefined) {
    if (rb === null) return null;
    if (bids[rb] !== undefined) return rb;
  }
  let best: string | null = null, top = 0;
  for (const [b, price] of Object.entries(bids).sort(([x], [y]) => (x < y ? -1 : 1))) {
    const sc = bidScore(g, row, b, price);
    if (sc > top) { top = sc; best = b; }
  }
  return best;
}

// ---- Terms and rounds ----------------------------------------------------------------------------------------------

const money = (pp: number): string => `${Math.round(pp * BUDGET.scale)}`;
const capOf = (g: Game, nat: string): number | undefined => { const c = g.nations[nat]?.capital ?? -1; return c >= 0 ? c : undefined; };
const named = (row: BargainRow): boolean => !row.generic;

/** Refreshes how a bargain's goods go to its buyer this month. */
function refresh(g: Game, row: BargainRow, h: BargainHeld, ctx: TradeCtx): void {
  if (!h.buyer) { h.way = 'none'; h.seas = []; h.lost = 0; h.share = 0; return; }
  const r = routeOf(g, row, h.buyer, ctx);
  h.way = r.way; h.seas = r.seas; h.lost = r.lost;
  // (A right to a base ships nothing for raiders to sink: it is paid in full while it holds, and nothing while its way is shut.)
  h.share = r.way === 'none' ? 0 : row.gives.base ? 1 : 1 - r.lost;
}

/** Starts a term: the seller sells to this buyer (or to nobody) for the next 'term' months. */
function setTerm(g: Game, row: BargainRow, buyer: string | null, price: number, ctx: TradeCtx, quiet = false, from = g.turn): void {
  const h = held(g, row.id), was = h.buyer;
  h.buyer = buyer; h.price = buyer ? price : 0; h.since = from; h.until = from + BARGAINS.term - 1;
  delete h.round; delete h.shut;
  refresh(g, row, h, ctx);
  if (quiet) return;
  const S = g.nations[row.seller], B = buyer ? g.nations[buyer] : null;
  // (The price is the winning sealed bid: only the seller and the buyer are told it. Everyone else hears who buys.)
  const plain = B ? `${S.name} sells ${row.name} to ${theName(B)} for a year.` : `${S.name} sells ${row.name} to nobody this year.`;
  const words = B ? `${S.name} sells ${row.name} to ${theName(B)} for a year, at ${money(price)} a month.` : plain;
  const me = g.human;
  if (me && (me === row.seller || me === buyer)) headline(g, 'info', words, me, capOf(g, row.seller));
  else if (me && was === me) headline(g, 'info', `${plain} Your term is over.`, me, capOf(g, row.seller));
  else if (named(row) && buyer !== was) headline(g, 'event', plain, row.seller, capOf(g, row.seller));
}

/** Opens a round of bids, if anybody can bid: the computer's bids go in at once; the player bids on the card this month. */
function openRound(g: Game, row: BargainRow, ctx: TradeCtx): void {
  const bidders = biddersOf(g, row, ctx);
  if (!bidders.length) return;
  const h = held(g, row.id);
  h.round = { opened: g.turn, bids: {} };
  for (const b of bidders) {
    if (b === g.human) continue;
    const p = computerBid(g, row, b, bidders);
    if (p > 0) h.round.bids[b] = p;
  }
  const me = g.human, S = g.nations[row.seller];
  if (!me) return;
  if (me === row.seller) headline(g, 'info', `The countries at war are bidding for your ${row.name}. Choose a buyer in the trade office, or sell to nobody.`, me, S.capital);
  else if (bidders.includes(me)) headline(g, 'info', `${S.name} is selling ${row.name} for a year. Make your offer this month in the trade office.`, me, S.capital);
}

/** Ends a term: nobody buys from now until a round chooses again. Its seller and buyer, if one is the player, are told why. */
function endTerm(g: Game, row: BargainRow, h: BargainHeld, why: string): void {
  if (h.buyer && g.human && (g.human === h.buyer || g.human === row.seller)) headline(g, 'info', why, g.human, capOf(g, row.seller));
  h.buyer = null; h.price = 0; h.share = 0; h.way = 'none'; h.seas = []; h.lost = 0;
  delete h.shut;
}

/** True while the buyer of a term can still take the goods. */
function stillTakes(g: Game, row: BargainRow, h: BargainHeld): boolean {
  const B = h.buyer ? g.nations[h.buyer] : undefined;
  if (!B || !B.alive || B.dormant || atWar(g, h.buyer!, row.seller)) return false;
  return (h.shut ?? 0) < BARGAINS.cutMonths;
}

// ---- The month -----------------------------------------------------------------------------------------------------

/** The month's bargains, as it opens: terms end, rounds open and close, goodwill and patience move. */
export function bargainsMonth(g: Game): void {
  if (!bargainRules(g)) return;
  const ctx: TradeCtx = new Map();
  const bk = book(g);
  for (const row of rowsOf(g)) {
    const S = g.nations[row.seller];
    if (!S) continue;
    const h = bk.rows[row.id];
    if (!sells(g, row.seller)) {
      if (h?.buyer) endTerm(g, row, h, `${S.name} no longer sells ${row.name}: it is at war or has joined a side.`);
      // (No term runs on: the first month it sells again a round opens.)
      if (h) { delete h.round; delete h.shut; h.until = Math.min(h.until, g.turn); }
      continue;
    }
    const st = held(g, row.id);
    // The term's buyer: how its goods go this month, and whether it can still take them.
    if (st.buyer) {
      refresh(g, row, st, ctx);
      // (Germany's ore stops in a winter when the Baltic is frozen and Narvik is lost, and comes again in the spring, as
      // in release 1: its term runs on.)
      // At stops 1 and 2 the record's buyer keeps its term the same way while its way is shut: it pays nothing until it opens.
      const seasonal = (row.replaces === 'ore' && st.buyer === NAVY.ore.to) || bookBuyer(g, row, st.buyer);
      st.shut = st.way === 'none' && !seasonal ? (st.shut ?? 0) + 1 : 0;
      if (!stillTakes(g, row, st)) {
        endTerm(g, row, st, `${theName(g.nations[st.buyer], true)} can no longer take ${S.name}'s ${row.name}: the term is at an end.`);
        st.until = g.turn - 1;
      }
    }
    // At stops 1 and 2 a computer-run seller follows the record: where the record names another buyer from next month,
    // and the term began before the record's change to it, the term ends now. (A term the seller gave by judgment after
    // the change, because the record's buyer made no bid, runs its year.)
    if (row.seller !== g.human && !st.round) {
      const next = recordBuyer(g, row, g.turn + 1), from = recordFrom(row, g.turn + 1);
      if (next !== undefined && from !== undefined && next !== st.buyer && st.since < from && st.until > g.turn && (next === null || whyNoBargain(g, row, next, ctx) === null)) st.until = g.turn;
    }
    // A round opened last month closes: the computer's seller chooses; the player's waits for him, reminded once.
    if (st.round && st.round.opened < g.turn) {
      if (row.seller === g.human) {
        // A term that has run out ends: nothing is sold until he chooses (the design director, 03:47 UTC).
        if (st.buyer && g.turn > st.until) endTerm(g, row, st, `Your term with ${theName(g.nations[st.buyer])} for your ${row.name} is over.`);
        if (st.round.reminded === undefined) {
          st.round.reminded = g.turn;
          const n = Object.keys(st.round.bids).length;
          warning(g, 'Bids are waiting', `${n === 0 ? 'No bid stands' : n === 1 ? 'One bid stands' : `${n} bids stand`} for your ${row.name}. Nothing is sold until you choose a buyer, or nobody, in the trade office.`, g.human, S.capital);
        }
        continue;
      }
      // (A computer's seller takes only the bids that can still be taken. The player's bids stand while he waits: he is
      // refused one that cannot be taken when he chooses it.)
      for (const b of Object.keys(st.round.bids)) if (whyNoBargain(g, row, b, ctx) !== null) delete st.round.bids[b];
      const buyer = sellerChoice(g, row, st.round.bids);
      setTerm(g, row, buyer, buyer ? st.round.bids[buyer] : 0, ctx);
      continue;
    }
    // A round opens in a term's last month, or at once when no term runs.
    if (!st.round && g.turn >= st.until) openRound(g, row, ctx);
    // A term that has run out ends, whether a round has opened for the next or nobody can bid for one.
    if (st.buyer && g.turn > st.until) endTerm(g, row, st, `${S.name}'s term with ${theName(g.nations[st.buyer])} for ${row.name} is over${st.round ? '; a new round of bids is open' : ', and nobody bids for another'}.`);
  }
  dependence(g);
  goodwill(g);
  patience(g);
}

/** Notes who had the goods this month, for a good its production depends on. */
function dependence(g: Game): void {
  const bk = book(g);
  for (const row of rowsOf(g)) {
    const h = bk.rows[row.id];
    if (row.output && h?.buyer && h.share > 0 && row.output[h.buyer]) bk.got[`${row.id}:${h.buyer}`] = g.turn;
  }
}

/** A sale draws the seller toward its buyer's side, a little each month. Not a seat in the player's hands: its path is his. */
function goodwill(g: Game): void {
  const bk = book(g);
  const pulls = new Map<string, number>();
  for (const row of rowsOf(g)) {
    const h = bk.rows[row.id];
    if (!h?.buyer || h.share <= 0) continue;
    const s = side(g.nations[h.buyer]);
    if (s !== 0) pulls.set(row.seller, (pulls.get(row.seller) ?? 0) + s);
  }
  for (const [seller, s] of [...pulls].sort(([a], [b]) => (a < b ? -1 : 1))) {
    const S = g.nations[seller];
    if (!S || ownPath(g, seller) || seller === g.human) continue;
    const d = BARGAINS.pull * Math.sign(s);
    S.opinion = Math.max(-100, Math.min(100, S.opinion + d));
    S.home = Math.max(-100, Math.min(100, S.home + d / 3));
  }
}

/**
 * Each great power at war with a buyer loses patience with the seller that feeds it, and gets it back in other months.
 * (A neutral's volunteer legion fighting a great power costs its home that power's patience as well: legions.ts.)
 */
function patience(g: Game): void {
  const bk = book(g), P = BARGAINS.patience;
  const feeds = new Map<string, Set<string>>(), loud = new Set<string>(), fallOf = new Map<string, number>(), legion = new Map<string, string>();
  for (const row of rowsOf(g)) {
    const h = bk.rows[row.id];
    if (!h?.buyer || h.share <= 0) continue;
    for (const p of majorsOf(g)) {
      const N = g.nations[p];
      if (!N?.alive || N.dormant || p === row.seller || !atWar(g, p, h.buyer) || atWar(g, p, row.seller)) continue;
      const k = `${p}:${row.seller}`;
      if (!feeds.has(k)) feeds.set(k, new Set());
      feeds.get(k)!.add(row.name);
      fallOf.set(k, P.fall);
      if (named(row)) loud.add(k);
    }
  }
  for (const f of legionFeeds(g)) {
    const k = `${f.power}:${f.home}`;
    if (!feeds.has(k)) feeds.set(k, new Set());
    legion.set(k, f.name);
    fallOf.set(k, (fallOf.get(k) ?? 0) + LEGIONS.legionFall);
    loud.add(k);
  }
  // What the power holds against the seller, as its news says it.
  // (Told to the seller, to the power, or to the world.)
  const cause = (k: string, goods: Set<string>, to: 'seller' | 'power' | 'world'): string => {
    const sold = goods.size ? `${to === 'seller' ? 'your' : 'its'} ${[...goods].join(' and ')} goes to ${to === 'power' ? 'your' : 'its'} enemies` : '';
    const fights = legion.has(k) ? `${legion.get(k)} fights ${to === 'power' ? 'you' : 'it'}` : '';
    return [sold, fights].filter(Boolean).join(', and ');
  };
  for (const k of Object.keys(bk.patience).sort()) if (!feeds.has(k)) {
    const v = Math.min(100, bk.patience[k] + P.heal);
    if (v >= 100) delete bk.patience[k]; else bk.patience[k] = v;
    // Its claim leaves the list (feedClaims): a warning given for it lapses, so a later claim is warned afresh.
    const id = `feed:${k}`;
    if (g.flags[`amb:${id}`] !== undefined && g.flags[`amb:go:${id}`] === undefined) {
      delete g.flags[`amb:${id}`]; delete g.flags[`amb:last:${id}`];
      const [p, seller] = k.split(':');
      if (g.human === seller) headline(g, 'event', `${The(g.nations[p])} lets its claim on ${theName(g.nations[seller])} rest for now.`, p);
    }
  }
  for (const [k, goods] of [...feeds].sort(([a], [b]) => (a < b ? -1 : 1))) {
    const was = bk.patience[k] ?? 100, now = Math.max(0, was - (fallOf.get(k) ?? P.fall));
    bk.patience[k] = now;
    const [p, seller] = k.split(':');
    const N = g.nations[p], S = g.nations[seller];
    const what = [...goods].join(' and ');
    if (legion.has(k)) {
      // (A legion's news: what it does said as it is. A seller whose goods alone feed the power reads as it always did.)
      if (was > P.half && now <= P.half) {
        if (g.human === seller) warning(g, 'Patience running out', `${The(N)} is losing patience: ${cause(k, goods, 'seller')}. Half of its patience is gone.`, seller, S.capital, p);
        else if (g.human === p) headline(g, 'info', `Your patience with ${theName(S)} is half gone: ${cause(k, goods, 'power')}.`, p, S.capital);
      } else if (was > 0 && now === 0) {
        const late = stopOf(g) >= 4 && p !== g.human;
        if (g.human === seller) warning(g, 'Patience spent', `${The(N)} has lost patience with you: ${cause(k, goods, 'seller')}.${late ? ' Its staff may now press a claim on your ground.' : ''}`, seller, S.capital, p);
        else headline(g, g.human === p ? 'info' : 'event', `${The(N)} has lost patience with ${theName(S)}: ${cause(k, goods, g.human === p ? 'power' : 'world')}.`, p, S.capital);
      }
      continue;
    }
    if (was > P.half && now <= P.half) {
      if (g.human === seller) warning(g, 'Patience running out', `${The(N)} is losing patience: your ${what} goes to its enemies. Half of its patience is gone.`, seller, S.capital, p);
      else if (g.human === p) headline(g, 'info', `Your patience with ${theName(S)} is half gone: its ${what} goes to your enemies.`, p, S.capital);
    } else if (was > 0 && now === 0) {
      const late = stopOf(g) >= 4 && p !== g.human;
      if (g.human === seller) warning(g, 'Patience spent', `${The(N)} has lost patience with you: your ${what} goes to its enemies.${late ? ' Its staff may now press a claim on your ground.' : ''}`, seller, S.capital, p);
      else if (g.human === p || loud.has(k)) headline(g, g.human === p ? 'info' : 'event', `${The(N)} has lost patience with ${theName(S)}, whose ${what} goes to its enemies.`, p, S.capital);
    }
  }
}

/** A great power's patience with a seller, 0 to 100. */
export const patienceOf = (g: Game, power: string, seller: string): number => (bargainRules(g) ? book(g).patience[`${power}:${seller}`] ?? 100 : 100);

// ---- What the bargains pay and cost, and what they bring -------------------------------------------------------------

/** What a country's sales earn it this month, in production points. */
export function bargainSales(g: Game, nat: string): number {
  if (!bargainRules(g)) return 0;
  let s = 0;
  for (const row of rowsOf(g)) {
    if (row.seller !== nat || !sells(g, nat)) continue;
    const h = book(g).rows[row.id];
    if (h?.buyer) s += h.price * h.share;
  }
  return s;
}

/** What a country pays for the bargains it buys this month, in production points. */
export function bargainCost(g: Game, nat: string): number {
  if (!bargainRules(g)) return 0;
  let s = 0;
  for (const row of rowsOf(g)) {
    const h = book(g).rows[row.id];
    if (h?.buyer === nat && sells(g, row.seller)) s += h.price * h.share;
  }
  return s;
}

/** What the goods a country that keeps no stocks buys are worth to its treasury this month. */
export function bargainGoodsMoney(g: Game, nat: string): number {
  if (!bargainRules(g) || keepsStores(g, nat)) return 0;
  let s = 0;
  for (const row of rowsOf(g)) {
    const h = book(g).rows[row.id];
    if (h?.buyer !== nat || row.inLane?.includes(nat) || !sells(g, row.seller)) continue;
    const goods = goodsOf(g, row);
    s += (goods.sup + goods.oil) * BARGAINS.asMoney * h.share;
  }
  return s;
}

/** All the money the bargains bring a country this month: its sales, and goods bought by a country that keeps no stocks. */
export const bargainMoney = (g: Game, nat: string): number => (bargainRules(g) ? bargainSales(g, nat) + bargainGoodsMoney(g, nat) : 0);

/** The goods a power that keeps stocks buys this month, as lines of its depots' books (resources.ts lanesTo). */
export function bargainLanes(g: Game, nat: string): Lane[] {
  if (!bargainRules(g) || !keepsStores(g, nat)) return [];
  const out: Lane[] = [];
  for (const row of rowsOf(g)) {
    const h = book(g).rows[row.id];
    if (h?.buyer !== nat || row.inLane?.includes(nat) || !sells(g, row.seller)) continue;
    const goods = goodsOf(g, row);
    if (goods.sup <= 0 && goods.oil <= 0) continue;
    const full = { oil: goods.oil, sup: goods.sup };
    const state = h.share <= 0 ? 'cut' : h.lost >= TRADE.sea.raided ? 'raided' : 'open';
    const why = state === 'cut' ? 'No way is open for the goods.' : state === 'raided' ? `${Math.round(h.lost * 100)}% of the cargoes are sunk on the way.` : row.replaces === 'ore' && h.way === 'sea' && oreRoute(g) === 'narvik' ? 'Coming through Narvik while the Baltic is frozen.' : '';
    out.push({
      id: row.replaces && dealBuyer(row) === nat ? row.replaces : `bargain:${row.id}`, path: [], kind: 'land', name: row.name, from: g.nations[row.seller].name, to: nat,
      // (No price: the depots' books are open to more eyes than the sealed bid. bargainsView tells it to those who may see it.)
      text: `Bought from ${theName(g.nations[row.seller])}, to ${turnToDate(h.until).label}.`, zones: [],
      full, flow: h.share, oil: full.oil * h.share, sup: full.sup * h.share, raid: 0, guard: 0, state, why,
    });
  }
  return out;
}

/** The share of a country's production it loses this month for want of a good its industry has come to depend on. */
export function bargainOutputLoss(g: Game, nat: string): number {
  if (!bargainRules(g) || !g.nations[nat]?.atWar.length) return 0;
  let s = 0;
  for (const row of rowsOf(g)) {
    const out = row.output?.[nat];
    if (!out) continue;
    const last = book(g).got[`${row.id}:${nat}`];
    if (last === undefined) continue;
    const selling = sells(g, row.seller);
    // Once its seller no longer sells, a good whose old deal comes back is that deal's again (navy.ts counts the ore's
    // share as release 1 does), and a seller on the buyer's own side trades with it as an ally.
    if (!selling && (row.replaces || allied(g, nat, row.seller))) continue;
    // (Nor once the seller has fallen and its capital is held by the buyer or the buyer's side: the mines are theirs.)
    if (!selling && !g.nations[row.seller]?.alive && heldBySide(g, nat, row.seller)) continue;
    const h = book(g).rows[row.id];
    if (selling && h?.buyer === nat && h.share > 0) continue;
    if (g.turn - last > (row.lag ?? 0)) s += out;
  }
  return s;
}

/** True when a fallen seller's capital is held by this country or a country on its side. */
function heldBySide(g: Game, nat: string, seller: string): boolean {
  const cap = g.nations[seller]?.capital ?? -1;
  const at = cap >= 0 ? g.regions[cap] : g.regions.find((r) => r.owner === seller && !r.overseas);
  return !!at && (at.ctrl === nat || allied(g, at.ctrl, nat));
}

/** True while a country may use this region's airfield by a bargain's right. */
export function baseRight(g: Game, nat: string, region: number): boolean {
  if (!bargainRules(g)) return false;
  const name = g.regions[region]?.name;
  for (const row of rowsOf(g)) {
    if (row.gives.base !== name) continue;
    const h = book(g).rows[row.id];
    if (h?.buyer === nat && sells(g, row.seller) && g.regions[region].ctrl === row.seller) return true;
  }
  return false;
}

/** The class of airfield a bargain's right counts at this region while it is held (air.ts fieldClass). */
export function baseField(g: Game, region: number): number {
  if (!bargainRules(g)) return 0;
  const name = g.regions[region]?.name;
  let f = 0;
  for (const row of rowsOf(g)) {
    if (row.gives.base !== name || !row.gives.field) continue;
    const h = book(g).rows[row.id];
    if (h?.buyer && sells(g, row.seller)) f = Math.max(f, row.gives.field);
  }
  return f;
}

// ---- Claims for patience spent (stops 4 and 5; claims.ts) -------------------------------------------------------------

export interface FeedClaim { id: string; nat: string; on: string; ground: string[]; name: string; why: string; feeds: true; stop: number }

/**
 * The claims a computer-run great power whose patience with a seller is spent presses at stops 4 and 5: the seller's
 * ground next to the power's own, by the ambitions of claims.ts. A claim already marched for stays on the list while
 * that war lasts.
 */
export function feedClaims(g: Game): FeedClaim[] {
  if (!bargainRules(g)) return [];
  const out: FeedClaim[] = [];
  const bk = book(g), late = stopOf(g) >= 4;
  // (A claim already marched for is listed at any stop and whatever its patience since, until claims.ts ends its war.)
  const keys = new Set(late ? Object.keys(bk.patience) : []);
  for (const f of Object.keys(g.flags)) if (f.startsWith('amb:go:feed:')) keys.add(f.slice('amb:go:feed:'.length));
  for (const k of [...keys].sort()) {
    const [p, seller] = k.split(':');
    const id = `feed:${p}:${seller}`;
    const went = g.flags[`amb:go:${id}`] !== undefined;
    if (!went && ((bk.patience[k] ?? 100) > 0 || !sells(g, seller))) continue;
    const P = g.nations[p], S = g.nations[seller];
    if (!P || !S || (!went && (p === g.human || !majorsOf(g).includes(p)))) continue;
    let ground = g.regions.filter((r) => r.owner === seller && !r.overseas && (r.nb.some((j) => g.regions[j].ctrl === p) || (went && r.ctrl === p))).map((r) => r.name);
    // (A claim already marched for stays listed while its war lasts, though the front has moved off the border: its ground is then the seller's whole country.)
    if (!ground.length && went) ground = g.regions.filter((r) => r.owner === seller && !r.overseas).map((r) => r.name);
    if (!ground.length) continue;
    const goods = rowsOf(g).filter((row) => row.seller === seller).map((row) => row.name);
    // (A home whose volunteer legion fights the power: that is the grievance named. legions.ts.)
    const vol = legionFeeds(g).find((f) => f.power === p && f.home === seller);
    out.push({ id, nat: p, on: seller, ground, name: `the ${S.adj} border`, why: vol ? `${S.adj} volunteers fight ${theName(P)} in ${vol.name}` : `${S.name} sells ${goods[0] ?? 'its goods'} to ${theName(P)}'s enemies`, feeds: true, stop: 4 });
  }
  return out;
}

/**
 * A warning given for a feed claim that has left the list (the dial turned under stop 4, the border lost, its claimant
 * now the player's) lapses, as a release 1 claim that stops being readable is put away: a later claim is warned
 * afresh. Called by claims.ts as its month begins. A claim marched for keeps its flags.
 */
export function lapseFeedClaims(g: Game): void {
  if (!bargainRules(g)) return;
  const listed = new Set(feedClaims(g).map((a) => a.id));
  for (const k of Object.keys(g.flags)) {
    if (!k.startsWith('amb:feed:')) continue;
    const id = k.slice('amb:'.length);
    if (listed.has(id) || g.flags[`amb:go:${id}`] !== undefined) continue;
    delete g.flags[k]; delete g.flags[`amb:last:${id}`];
  }
}

// ---- Opening, orders and the view ------------------------------------------------------------------------------------

/**
 * As a grand campaign opens with the switch on: the flag, every other neutral's exports from what it has to trade, and
 * the agreements standing on the first day, by the record.
 */
export function openBargains(g: Game): void {
  if (BARGAINS_STAGE) startBargains(g);
}

/** Turns the bargains on in a grand campaign whatever stage.json says (a test, the test kitchen), as openBargains would. */
export function startBargains(g: Game): void {
  if (g.flags.open !== 1 || g.bargains) return;
  g.flags.bargains = 1;
  const generic: Record<string, number> = {};
  const namedSellers = new Set(BARGAINS.rows.map((r) => r.seller));
  for (const n of Object.values(g.nations).sort((a, b) => (a.id < b.id ? -1 : 1))) {
    if (!small(n.id) || majorsOf(g).includes(n.id) || namedSellers.has(n.id) || BARGAINS.generic.never.includes(n.id) || n.dormant || !n.alive) continue;
    const w = r2(BARGAINS.generic.share * wealthOf(g, n.id));
    if (w > 0) generic[n.id] = w;
  }
  g.bargains = { rows: {}, patience: {}, generic, got: {} };
  bargainsFirstDay(g);
}

/**
 * The agreements standing on a campaign's first day, by the record: every row with no term yet whose record names a
 * buyer that day. Run as the bargains open, and again once a new campaign's dial is set (turn.ts orderHistory, fresh):
 * a campaign begun in 1940 or 1941 is opened before its stop is, and the record's buyers are read at that stop.
 */
export function bargainsFirstDay(g: Game): void {
  if (!bargainRules(g)) return;
  const ctx: TradeCtx = new Map();
  for (const row of rowsOf(g)) {
    if (!sells(g, row.seller)) continue;
    const h = book(g).rows[row.id];
    if (h?.buyer || h?.round) continue;
    let buyer: string | null | undefined;
    for (const [from, to, b] of row.record ?? []) if (g.turn >= from && g.turn <= to) buyer = b;
    if (!buyer || whyNoBargain(g, row, buyer, ctx) !== null) continue;
    setTerm(g, row, buyer, computerBid(g, row, buyer, [buyer]) || BARGAINS.bid.floor, ctx, true);
  }
  dependence(g);
}

/** The steps a country can bid this round: from the floor up to its most. */
export function bidSteps(g: Game, buyer: string): number[] {
  const B = BARGAINS.bid, most = bidMost(g, buyer), out: number[] = [];
  for (let p = B.floor; p <= most + 1e-9 && out.length < 60; p = r2(p + B.step)) out.push(p);
  return out;
}

/** The player (or a test) bids for a bargain whose round is open. Returns why not, or null when the bid stands. */
export function placeBid(g: Game, buyer: string, id: string, pp: number): string | null {
  if (!bargainRules(g)) return 'There are no bargains in this campaign.';
  const row = rowById(g, id), h = row ? book(g).rows[row.id] : undefined;
  if (!row || !h?.round) return 'No round of bids is open for it.';
  if (h.round.opened !== g.turn && row.seller !== g.human) return 'The bids are in: the seller is choosing.';
  const why = whyNoBargain(g, row, buyer);
  if (why) return why;
  if (pp < BARGAINS.bid.floor - 1e-9) return `The least bid is ${money(BARGAINS.bid.floor)} a month.`;
  if (pp > bidMost(g, buyer) + 1e-9) return `The treasury will not stretch to more than ${money(bidMost(g, buyer))} a month.`;
  h.round.bids[buyer] = r2(pp);
  return null;
}

/** The seller in the player's seat chooses a buyer from the bids that stand, or nobody. Returns why not, or null. */
export function chooseBuyer(g: Game, seller: string, id: string, buyer: string | null): string | null {
  if (!bargainRules(g)) return 'There are no bargains in this campaign.';
  const row = rowById(g, id), h = row ? book(g).rows[row.id] : undefined;
  if (!row || row.seller !== seller || !h?.round) return 'No round of bids is open for it.';
  if (buyer !== null) {
    if (h.round.bids[buyer] === undefined) return 'That country has made no bid.';
    const why = whyNoBargain(g, row, buyer);
    if (why) return why;
  }
  // (He chooses in his own month, after the countries at war have had theirs: the year is counted from next month,
  // as a computer's seller chooses as the next month opens. The term ending now runs to the end of this one.)
  setTerm(g, row, buyer, buyer ? h.round.bids[buyer] : 0, new Map(), false, g.turn + 1);
  return null;
}

export interface BargainView {
  id: string; seller: string; sellerName: string; capital: number; name: string; sign: string;
  /** The term's price a month (the winning bid): null where the viewer cannot see it, as a bid. */
  buyer: string | null; price: number | null; monthsLeft: number; way: 'land' | 'sea' | 'none'; state: 'open' | 'raided' | 'cut' | 'none';
  /** The seas the goods cross, and the sea where most are lost (the map's gap), or -1. */
  seas: number[]; lossAt: number;
  /** The round, if one is open: months it has been open, each bid (null where the viewer cannot see it), the viewer's own. */
  round: { months: number; bids: { nat: string; offer: number | null }[]; mine: number | null; steps: number[]; canBid: string | null; accept: boolean } | null;
  /**
   * What reached the buyer this month, after the sea's losses: supply and oil as resources.json counts them, and for a
   * good its industry depends on, the production it kept going (pp). moved is false in a month nothing arrived.
   */
  cargo: { moved: boolean; sup: number; oil: number; pp: number | null };
  /** The seller's public opinion, -100 (the Allies) to 100 (the Axis): the dot the alignment track shows every viewer. */
  lean: number;
  /** For a right to a base, the region of that place (its plate on the map while the right holds), else -1: nothing ships, so no lane. */
  base: number;
  /** Each great power's patience with the seller (0-100), where some has been lost: null where the viewer cannot see it. */
  patience: { nat: string; left: number | null }[];
}

/** True when the viewer can see a bid: it is the seller's, its own, or an agent of its is at work in the seller's or the bidder's capital. */
function seesBid(g: Game, viewer: string, row: BargainRow, bidder: string): boolean {
  if (viewer === row.seller || viewer === bidder) return true;
  return agentIn(g, viewer, [row.seller, bidder]);
}

/** True while an agent of the viewer's is at work in one of these countries' capitals. */
function agentIn(g: Game, viewer: string, nats: string[]): boolean {
  const caps = nats.map((n) => g.nations[n]?.capital).filter((c): c is number => c !== undefined && c >= 0);
  return (g.intel?.agents ?? []).some((a) => a.nat === viewer && a.ready <= g.turn && caps.includes(a.reg));
}

/**
 * True when the viewer can see a great power's patience with a seller, as it sees a bid: its own, the seller's (whom
 * the power's warnings reach), or an agent of the viewer's at work in the power's capital.
 */
const seesPatience = (g: Game, viewer: string | null, seller: string, power: string): boolean =>
  !!viewer && (viewer === power || viewer === seller || agentIn(g, viewer, [power]));

/** What reached a bargain's buyer this month. The production it kept going is told only to a viewer who may read the buyer's books (intel.ts). */
function cargoOf(g: Game, row: BargainRow, h: BargainHeld, viewer: string | null): BargainView['cargo'] {
  if (!h.buyer || h.share <= 0 || row.gives.base) return { moved: false, sup: 0, oil: 0, pp: 0 };
  const goods = goodsOf(g, row), out = row.output?.[h.buyer] ?? 0;
  const pp = out > 0 ? (knowsBooks(g, h.buyer, viewer) ? r2(out * Math.max(0, income(g, h.buyer))) : null) : 0;
  return { moved: true, sup: r2(goods.sup * h.share), oil: r2(goods.oil * h.share), pp };
}

/** Every bargain with a buyer or an open round, as a viewer's map shows them. */
export function bargainsView(g: Game, viewer: string | null = g.human): BargainView[] {
  if (!bargainRules(g)) return [];
  const out: BargainView[] = [];
  for (const row of rowsOf(g)) {
    const h = book(g).rows[row.id], S = g.nations[row.seller];
    if (!h || !S || (!h.buyer && !h.round) || !sells(g, row.seller)) continue;
    let lossAt = -1;
    if (h.way === 'sea' && h.lost > 0 && h.seas.length) lossAt = h.seas[Math.floor(h.seas.length / 2)];
    const round = h.round ? {
      months: g.turn - h.round.opened + 1,
      // (Only the bids the viewer may see: that a rival has bid at all is part of its sealed bid.)
      bids: Object.entries(h.round.bids).filter(([nat]) => viewer && seesBid(g, viewer, row, nat)).sort(([a], [b]) => (a < b ? -1 : 1)).map(([nat, offer]) => ({ nat, offer })),
      mine: viewer ? h.round.bids[viewer] ?? null : null,
      steps: viewer && viewer !== row.seller ? bidSteps(g, viewer) : [],
      canBid: viewer && viewer !== row.seller ? (h.round.opened === g.turn ? whyNoBargain(g, row, viewer) : 'The bids are in: the seller is choosing.') : 'It is yours to sell.',
      accept: viewer === row.seller,
    } : null;
    // (Those the viewer can see, lowest first; then the hidden ones by name, so the order gives nothing away.)
    const patience = Object.entries(book(g).patience).filter(([k]) => k.endsWith(`:${row.seller}`))
      .map(([k, v]) => ({ nat: k.split(':')[0], left: seesPatience(g, viewer, row.seller, k.split(':')[0]) ? v : null }))
      .sort((a, b) => (a.left === null ? 1 : 0) - (b.left === null ? 1 : 0) || (a.left ?? 0) - (b.left ?? 0) || (a.nat < b.nat ? -1 : 1));
    out.push({
      id: row.id, seller: row.seller, sellerName: S.name, capital: S.capital, name: row.name, sign: row.sign,
      buyer: h.buyer, price: h.buyer && viewer && seesBid(g, viewer, row, h.buyer) ? h.price : null, monthsLeft: h.buyer ? Math.max(0, h.until - g.turn + 1) : 0, way: h.way,
      state: !h.buyer ? 'none' : h.share <= 0 ? 'cut' : h.lost >= TRADE.sea.raided ? 'raided' : 'open',
      seas: h.seas, lossAt, round, cargo: cargoOf(g, row, h, viewer), lean: r2(S.opinion), base: row.gives.base ? g.byName[row.gives.base]?.id ?? -1 : -1, patience,
    });
  }
  return out;
}
