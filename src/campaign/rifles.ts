// A nation's rifles: what fills its rifles shelf (src/campaign/arms.ts) each month, and what an enemy can cut.
//
// The design director's rulings of 05:37Z and 05:44Z of 25 November 2026 (pacific-japan/dd-form-arms-1125.md,
// sections 3, 4, 7, 14 and 15): a nation's new corps and drafts are armed from ONE stock of rifles. This module makes
// the stock FILL: the arsenals' month (arms.ts puts each works' gain while the nation holds its place), the roads that
// bring rifles and the ground that cuts them, a seller's spare, the news of a cut, and the views a page reads. Nothing
// here draws from the store for a corps or a draft: that is the next stage's.
//
// No country is named here: the rows decide (data/campaign/rifles.json, and the far side's data/campaign/far-arms.json
// laid over it by far-rows.ts). A nation with no rifles row, no shelf and no road is never read, written or told
// anything: with the far switch off no row and no road is laid, so nothing happens anywhere.
//
// State is kept in g.flags, so it is saved with the campaign. Written only for a nation the rows name:
//   rflm:<nat>   the last month its rifles were put on the shelf (riflesMonth)
//   rflc:<road>  0 while the road ran when last read, else the month it was found cut (for the news, once)
// and the shelves' own flags (arms.ts: sa:<nat>, sam:<nat>).

import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import data from '../../data/campaign/rifles.json';
import production from '../../data/campaign/production.json';
import { SHELVES, armsMonth, armsView, draw, gainOf, has, put, stockOf } from './arms';
import { atWar, headline, industryHeld, theName } from './game';
import { majorsOf } from './diplomacy';
import type { Game } from './types';

/** A nation's rifles row: rifles to its division's worth, the share a draft draws, and whether an empty store refuses anything (the next stage reads the last two). */
export interface RiflesRow { perDiv: number; refit?: number; binds?: boolean }
/** A road that brings a nation rifles. 'past' runs in order from where its cargoes start to where they reach the receiver. */
export interface RoadRow { id: string; name: string; to: string; past: string[]; tons: number; perTon: number; seller?: string }

export const RIFLES = data as unknown as { sellerPerDiv: number; nations: Record<string, RiflesRow>; roads: RoadRow[] };

const num = (x: unknown): number => (typeof x === 'number' && Number.isFinite(x) ? x : 0);
const pos = (x: unknown): number => Math.max(0, num(x));
const keys = (pre: string): ((k: string) => string) => { const K: Record<string, string> = {}; return (k) => (K[k] ??= `${pre}:${k}`); };
const kMonth = keys('rflm'), kCut = keys('rflc');

// ---- Rows -------------------------------------------------------------------------------------------------------------

/** The nation's rifles row, or null where it has none. */
export function riflesRow(_g: Game, nat: string): RiflesRow | null {
  const r = RIFLES.nations[nat];
  return r && typeof r === 'object' && num(r.perDiv) > 0 ? r : null;
}

/** The roads that end with the nation, in the order the rows give them. */
export const rifleRoads = (_g: Game, nat: string): RoadRow[] => RIFLES.roads.filter((r) => r.to === nat);

/** Rifles to a division's worth of this nation, as a seller reads them: its own row, else the sellers' figure (G). */
const perDivOf = (g: Game, nat: string): number => riflesRow(g, nat)?.perDiv ?? (num(RIFLES.sellerPerDiv) > 0 ? RIFLES.sellerPerDiv : 0);

// ---- What a seller's own arming asks ----------------------------------------------------------------------------------

type Need = (g: Game, nat: string) => number;
const NEEDS: Need[] = [];

/**
 * Registers a source of what a nation's own arming asks of its rifles this month (its new corps and drafts, armed
 * first), fn(g, nat) in its divisions' worth. A seller sells only what its stock holds beyond the sum. Returns the
 * call that takes the source away. With none registered a nation's own arming asks nothing.
 */
export function needsFrom(fn: Need): () => void {
  NEEDS.push(fn);
  return () => { const i = NEEDS.indexOf(fn); if (i >= 0) NEEDS.splice(i, 1); };
}

/** What a nation's own arming asks this month, in its divisions' worth. */
export const ownNeed = (g: Game, nat: string): number => NEEDS.reduce((s, fn) => s + pos(fn(g, nat)), 0);

/** What a seller with a rifles shelf can spare this month, in its own divisions' worth: its stock less its own need, never below 0. */
export const spareOf = (g: Game, nat: string): number => Math.max(0, stockOf(g, nat, 'rifles') - ownNeed(g, nat));

// ---- A road this month ------------------------------------------------------------------------------------------------

/** How a road stands: running, cut (by a place or the seller), or running with nothing to carry from its seller (dry). */
export type RoadState = 'open' | 'cut' | 'dry';
export interface RoadNow {
  id: string; name: string; tons: number;
  /** Rifles it would bring at its tons, before the seller's spare. */
  full: number;
  /** Rifles it brings this month. */
  rifles: number;
  state: RoadState;
  /** What stops it, as a sentence in the lanes' own words; empty while it runs. */
  why: string;
  /** What cut it: the place, or the seller. */
  by?: 'place' | 'seller';
  /** The place that cut it, where a place did. */
  at?: string;
  /** The nation that cut it: the holder of the place, or the seller. */
  who?: string;
}

const nameOf = (g: Game, id: string, start = false): string => (g.nations[id] ? theName(g.nations[id], start) : id);
const cap = (s: string): string => (s ? s[0].toUpperCase() + s.slice(1) : s);
const warWith = (g: Game, a: string, b: string): boolean => !!g.nations[a] && !!g.nations[b] && atWar(g, a, b);

/** Why nothing comes down a road this month, or null while it runs. */
function stopOf(g: Game, road: RoadRow): Pick<RoadNow, 'state' | 'why' | 'by' | 'at' | 'who'> | null {
  const to = road.to;
  // An enemy at war with the receiver holding any place on it, the first in the road's order. A neutral or a friend lets it by.
  for (const name of road.past ?? []) {
    const r = g.byName[name];
    if (r && r.ctrl !== to && warWith(g, r.ctrl, to)) return { state: 'cut', why: `${nameOf(g, r.ctrl, true)} holds ${r.name} and lets nothing past.`, by: 'place', at: r.name, who: r.ctrl };
  }
  // War with the seller.
  if (road.seller && warWith(g, to, road.seller)) return { state: 'cut', why: `${nameOf(g, to, true)} is at war with ${nameOf(g, road.seller)}, which sold the cargoes.`, by: 'seller', who: road.seller };
  return null;
}

/**
 * A road as it stands this month for its receiver: its state, and the rifles it brings. A road of 0 tons runs and
 * carries nothing. A road with a seller brings only what the seller can spare; `sell` is set to what is drawn from the
 * seller's shelf, in the seller's own divisions' worth.
 */
function roadNow(g: Game, road: RoadRow, sell?: { draw: number }): RoadNow {
  const full = pos(road.tons) * pos(road.perTon);
  const out: RoadNow = { id: road.id, name: road.name, tons: pos(road.tons), full, rifles: 0, state: 'open', why: '' };
  const stop = stopOf(g, road);
  if (stop) return { ...out, ...stop };
  const seller = road.seller;
  if (!seller || full <= 0) return { ...out, rifles: full };
  if (has(g, seller, 'rifles')) {
    // The seller's own new corps and drafts are armed first; what is left can be sold (05:44Z, point C).
    const per = perDivOf(g, seller);
    const can = per > 0 ? spareOf(g, seller) * per : 0, rifles = Math.min(full, can);
    if (sell) sell.draw = per > 0 ? rifles / per : 0;
    if (rifles <= 0) return { ...out, state: 'dry', why: `${nameOf(g, seller, true)} has no rifles to spare beyond its own needs.` };
    return { ...out, rifles };
  }
  // No rifles shelf of the seller's on this line: the design director's interim test, flagged G (05:44Z, point C:
  // "a seller at war with a great power sends nothing"), until the seller's row is read. At peace it sends the whole.
  const big = majorsOf(g);
  const foe = g.nations[seller]?.atWar.find((x) => big.includes(x));
  if (foe) return { ...out, state: 'dry', why: `${nameOf(g, seller, true)} is at war with ${nameOf(g, foe)} and sells nothing.` };
  return { ...out, rifles: full };
}

/** The nation's roads as they stand this month. */
export const roadsNow = (g: Game, nat: string): RoadNow[] => rifleRoads(g, nat).map((r) => roadNow(g, r));

// ---- The month --------------------------------------------------------------------------------------------------------

/** Whole rifles, written with commas, as the form writes them. */
const n0 = (x: number): string => Math.round(x).toLocaleString('en-US');
/** Rifles rounded to the hundred, as a figure for a sentence. */
const n100 = (x: number): string => n0(Math.round(x / 100) * 100);

/**
 * The month's rifles for a nation, once a month (a second call adds nothing), from game.ts startPhase beside the oil
 * and supply books: the arsenals (arms.ts armsMonth, by the share of home industry held, which scales no works), then
 * each running road's rifles put on the receiver's shelf, as its divisions' worth. A nation with no shelf and no road:
 * nothing happens and nothing is written.
 */
export function riflesMonth(g: Game, nat: string): void {
  const roads = rifleRoads(g, nat), shelf = SHELVES.some((s) => has(g, nat, s));
  if (!shelf && !roads.length) return;
  if (!g.nations[nat]?.alive || g.flags[kMonth(nat)] === g.turn) return;
  g.flags[kMonth(nat)] = g.turn;
  if (shelf) armsMonth(g, nat, industryHeld(g, nat));
  const per = riflesRow(g, nat)?.perDiv ?? 0;
  for (const road of roads) {
    const sell = { draw: 0 };
    const now = roadNow(g, road, sell);
    if (now.rifles > 0 && per > 0 && has(g, nat, 'rifles')) {
      // The sale is drawn from the seller's shelf, in its own divisions' worth; what reaches the receiver is what was drawn.
      const got = road.seller && sell.draw > 0 ? draw(g, road.seller, 'rifles', sell.draw) * perDivOf(g, road.seller) : now.rifles;
      put(g, nat, 'rifles', got / per);
    }
    roadNews(g, nat, road, now);
  }
}

/** The news of a road, once: the month it goes from running to cut, and the month it runs again. Told to the receiver's seat, as the books tell a seat. */
function roadNews(g: Game, nat: string, road: RoadRow, now: RoadNow): void {
  const k = kCut(road.id), was = g.flags[k];
  const cut = now.state === 'cut';
  if (was !== undefined && (was > 0) === cut) return;
  g.flags[k] = cut ? g.turn : 0;
  // (The first reading of a road is no change: a road cut on the first day is not news.)
  if (was === undefined || nat !== g.human) return;
  headline(g, 'info', cut ? cutWords(g, road, now) : runsWords(g, road), nat);
}

/**
 * Words to the UI director: the news of a cut (the form's section 7, "The Burma Road is cut: Japan holds Lashio.
 * 5,000 fewer rifles reach your store each month."), figures from the data.
 */
function cutWords(g: Game, road: RoadRow, now: RoadNow): string {
  const why = now.by === 'place' ? `${nameOf(g, now.who!, true)} holds ${now.at}.` : now.why;
  const lost = now.full > 0 ? ` ${n100(now.full)} fewer rifles reach your store each month.` : ' It carried no rifles to your store.';
  return `${cap(road.name)} is cut: ${why}${lost}`;
}

/** Words to the UI director: the news of a road running again. */
function runsWords(_g: Game, road: RoadRow): string {
  const full = pos(road.tons) * pos(road.perTon);
  return `${cap(road.name)} runs again${full > 0 ? `: ${n100(full)} rifles a month can reach your store.` : ', though it carries no rifles.'}`;
}

// ---- The views --------------------------------------------------------------------------------------------------------

export interface WorksNow { place: string; rifles: number; held: boolean }
export interface Inflow { works: WorksNow[]; roads: RoadNow[]; arsenals: number; total: number }

/** The month's inflow by source, in rifles: each arsenal with its place and whether held, each road. */
function inflowOf(g: Game, nat: string): Inflow {
  const per = riflesRow(g, nat)?.perDiv ?? 0;
  // (Each works with its place and whether held, as the shelves' own view reads them; a works' gain is never scaled.)
  const works = (has(g, nat, 'rifles') ? armsView(g, nat).rifles.works : []).map((w) => ({ place: w.place, rifles: w.gain * per, held: w.held }));
  const roads = roadsNow(g, nat);
  const arsenals = has(g, nat, 'rifles') ? gainOf(g, nat, 'rifles', industryHeld(g, nat)) * per : 0;
  return { works, roads, arsenals, total: arsenals + roads.reduce((s, r) => s + r.rifles, 0) };
}

/** Rifles an infantry corps takes, for this nation: its divisions times rifles to a division's worth. */
const corpsRifles = (g: Game, nat: string): number => (production as any).builds.inf.divs * (riflesRow(g, nat)?.perDiv ?? 0);

export interface RiflesView {
  nat: string; perDiv: number; refit: number; binds: boolean;
  /** Rifles in store, whole rifles. */
  stock: number;
  inflow: Inflow;
  /** The picture line's parts (the form's section 7): the bar, the stock, what an infantry corps takes, the month's total and its parts. */
  picture: { bar: string; stock: number; corps: number; total: number; parts: { label: string; rifles: number }[]; line: string };
  /** Words to the UI director: the arsenal page's line. */
  says: string;
}

/** A label for a source of rifles on the picture line: "Chungking's arsenals", "the Burma Road". */
const worksLabel = (place: string): string => `${place}'s arsenals`;

/**
 * The nation's own seat: its rifles in store, the month's inflow by source and the picture line, or null for a
 * nation the rows do not name.
 */
export function riflesView(g: Game, nat: string): RiflesView | null {
  const row = riflesRow(g, nat);
  if (!row || !has(g, nat, 'rifles')) return null;
  const per = row.perDiv, stock = Math.round(stockOf(g, nat, 'rifles') * per), inflow = inflowOf(g, nat), corps = corpsRifles(g, nat);
  const parts = [...inflow.works.filter((w) => w.held && w.rifles > 0).map((w) => ({ label: worksLabel(w.place), rifles: w.rifles })), ...inflow.roads.filter((r) => r.rifles > 0).map((r) => ({ label: r.name, rifles: r.rifles }))];
  const filled = corps > 0 ? Math.max(0, Math.min(10, Math.round((10 * stock) / corps))) : 0;
  const bar = '■'.repeat(filled) + '□'.repeat(10 - filled);
  // Words to the UI director (the form's section 7): "Rifles in store ■■■□□□□□□□ 21,000 of the 64,000 an infantry
  // corps takes · +9,500 a month: Chungking's arsenals 4,500 · the Burma Road 5,000".
  const line = `Rifles in store ${bar} ${n100(stock)} of the ${n0(Math.round(corps / 1000) * 1000)} an infantry corps takes · +${n100(inflow.total)} a month${parts.length ? `: ${parts.map((p) => `${p.label} ${n100(p.rifles)}`).join(' · ')}` : ''}`;
  // Words to the UI director (the form's section 7): the arsenal page.
  const says = 'Your new corps and your drafts are armed from this store. It fills from your arsenals and from the roads you still hold.';
  return { nat, perDiv: per, refit: num(row.refit), binds: row.binds !== false, stock, inflow, picture: { bar, stock, corps, total: inflow.total, parts, line }, says };
}

export interface RiflesSeen { nat: string; inflow: Inflow; line: string; card: string }

/**
 * Another seat's view of a nation's rifles: its roads and its monthly inflow, never its stock. Null for a nation the
 * rows do not name.
 */
export function riflesSeen(g: Game, nat: string, by: string | null): RiflesSeen | null {
  const row = riflesRow(g, nat);
  if (!row || !has(g, nat, 'rifles')) return null;
  const inflow = inflowOf(g, nat), who = nameOf(g, nat, true), corps = corpsRifles(g, nat);
  const roads = inflow.roads.filter((r) => r.rifles > 0);
  const blocks = (x: number): string => '▮'.repeat(Math.max(1, Math.round(x / 1000)));
  // Words to the UI director (the form's section 7, Japan's view): "China's rifles +9,500 a month: arsenals ▮▮▮▮▮ ·
  // Burma Road ▮▮▮▮▮".
  const parts = [...(inflow.arsenals > 0 ? [`arsenals ${blocks(inflow.arsenals)}`] : []), ...roads.map((r) => `${cap(r.name.replace(/^the /, ''))} ${blocks(r.rifles)}`)];
  const line = `${who}'s rifles +${n100(inflow.total)} a month${parts.length ? `: ${parts.join(' · ')}` : ''}`;
  // Words to the UI director (the form's section 7, China's card): "China arms its new corps from its arsenals and the
  // Burma Road: about 9,500 rifles a month, a new infantry corps about every seven months."
  const from = ['its arsenals', ...roads.map((r) => r.name)];
  const list = from.length === 1 ? from[0] : `${from.slice(0, -1).join(', ')} and ${from[from.length - 1]}`;
  const every = inflow.total > 0 && corps > 0 ? Math.round(corps / inflow.total) : 0;
  const card = `${who} arms its new corps from ${list}: about ${n100(inflow.total)} rifles a month${every > 0 ? `, a new infantry corps about every ${every} month${every === 1 ? '' : 's'}` : ''}.`;
  void by;
  return { nat, inflow, line, card };
}
