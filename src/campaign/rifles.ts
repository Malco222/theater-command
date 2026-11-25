// A nation's rifles: what fills its rifles shelf (src/campaign/arms.ts) each month, and what an enemy can cut.
//
// The design director's rulings of 05:37Z and 05:44Z of 25 November 2026 (pacific-japan/dd-form-arms-1125.md,
// sections 3, 4, 7, 14 and 15): a nation's new corps and drafts are armed from ONE stock of rifles. This module makes
// the stock FILL: the arsenals' month (arms.ts puts each works' gain while the nation holds its place), the roads that
// bring rifles and the ground that cuts them, a seller's spare, the news of a cut, and the views a page reads. And the
// taps (stage B2): a new corps takes its rifles from the store when it is ordered, and is refused where the store is
// short and the nation's row binds; an order taken back gives them back; the price of a corps no longer charges points
// for the rifles the store now charges (the carve).
//
// No country is named here: the rows decide (data/campaign/rifles.json, and the far side's data/campaign/far-arms.json
// laid over it by far-rows.ts). A nation with no rifles row, no shelf and no road is never read, written or told
// anything: with the far switch off no row and no road is laid, so nothing happens anywhere.
//
// State is kept in g.flags, so it is saved with the campaign. Written only for a nation the rows name:
//   rflm:<nat>   the last month its rifles were put on the shelf (riflesMonth)
//   rflc:<road>  for the news, once: 0 while the road ran with its load when last read (a road of 0 tons, running);
//                minus the month it was found running dry (its seller sent nothing), which is no news; else the month
//                it was found cut, kept while a cut road comes back dry, so that "runs again" is told only when
//                its rifles can come; and half a month further from 0 once its seller's fall was told, while the
//                seller stays fallen (roadNews)
//   rfa:<id>     a corps' armed level, a share of its divisions, for a nation that binds; only below the corps' full
//   rfo:<nat>    1 once the nation's corps were armed at their strength (its opening, or an older save's next month)
// and the shelves' own flags (arms.ts: sa:<nat>, sam:<nat>). A corps on order that drew rifles carries what it drew in
// its Build's 'rifles' (divisions' worth), so the save keeps it and a cancel can give it back.

import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import data from '../../data/campaign/rifles.json';
import production from '../../data/campaign/production.json';
import { SHELVES, armsMonth, armsView, draw, gainOf, giveBack, has, put, stockOf } from './arms';
import { atWar, headline, industryHeld, theName } from './game';
import { majorsOf } from './diplomacy';
import { corpsList, needsOf } from './economy';
import type { Build, Corps, Game } from './types';

/** A nation's rifles row: rifles to its division's worth, the share a draft draws, and whether an empty store refuses anything (the next stage reads the last two). */
export interface RiflesRow { perDiv: number; refit?: number; binds?: boolean }
/** A road that brings a nation rifles. 'past' runs in order from where its cargoes start to where they reach the receiver. */
export interface RoadRow { id: string; name: string; to: string; past: string[]; tons: number; perTon: number; seller?: string }

export const RIFLES = data as unknown as { sellerPerDiv: number; nations: Record<string, RiflesRow>; roads: RoadRow[]; fallen?: string };

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

/** True where the nation has a rifles row and a rifles shelf: its new corps draw from the store. */
export const carries = (g: Game, nat: string): boolean => !!riflesRow(g, nat) && has(g, nat, 'rifles');

/** True where the store also refuses: the nation carries and its row is not marked binds: false (only false frees it). */
export const binds = (g: Game, nat: string): boolean => carries(g, nat) && riflesRow(g, nat)!.binds !== false;

// ---- The draw at the orders (stage B2) --------------------------------------------------------------------------------
// The design director's 05:37Z of 25 November 2026 (pacific-japan/dd-form-arms-1125.md, clause 3.4, sections 8 and 9):
// a new corps takes its rifles when it is ordered, by player and computer, in every order of the arsenal; a store short
// of them refuses the order; an order taken back returns its rifles and its men. A row marked binds: false counts its
// draws and refuses nothing (clause 8).

/** For a probe or a test: told what each order drew, in the nation's divisions' worth. Kept in no save. */
export const riflesWatch: { tell: ((e: { nat: string; what: 'order' | 'draft'; rifles: number }) => void) | null } = { tell: null };

/** True unless the nation binds and its store holds less than this many divisions' worth of rifles. */
export function canArm(g: Game, nat: string, divs: number): boolean {
  if (binds(g, nat) && stockOf(g, nat, 'rifles') + 1e-9 < divs) return false;
  return true;
}

/** Draws a new corps' rifles from the store where the nation carries, and gives what was drawn (0 where it does not). */
export function armOrder(g: Game, nat: string, divs: number): number {
  if (!carries(g, nat)) return 0;
  const got = draw(g, nat, 'rifles', divs);
  riflesWatch.tell?.({ nat, what: 'order', rifles: got });
  return got;
}

/** Gives back the rifles an order taken back had drawn. */
export function unarmOrder(g: Game, nat: string, b: Build): void {
  if (b.rifles && b.rifles > 0) giveBack(g, nat, 'rifles', b.rifles);
}

const kLevel = keys('rfa'), kOpen = keys('rfo');

/** A cadre ordered raised again comes back with new men at its full level: its old armed level is struck. */
export function riflesRaised(g: Game, id: number): void {
  const k = kLevel(String(id));
  if (k in g.flags) delete g.flags[k];
}

// ---- The armed level (stage B2) ---------------------------------------------------------------------------------------
// The design director's 05:37Z (clause 3.5; the form's sections 12 and 13; 15D): each corps of a nation whose row binds
// is armed to a level, a share of its divisions, which its drafts refill to and no further. Battle never lowers it; a
// broken corps loses all; on the first day it is the corps' strength; a new corps comes at full; men moved from one
// corps to another bring their rifles. Kept as rfa:<corps id> only where it stands below the corps' full; absent, it
// is the corps' full (c.full, which a row's 'est' sets, else 1). Read only for a nation that binds: for every other
// nation the callers keep today's figures. (A dated row without 'est': the director's answer 5, no hook until such a
// row is laid.)

/** The corps' armed level, a share of its divisions: its flag where it stands, else its full. */
export function levelOf(g: Game, c: Corps): number {
  const x = g.flags[kLevel(String(c.id))];
  return x !== undefined ? x : c.full ?? 1;
}

/** Sets a corps' armed level: a flag where it stands below the corps' full, none at or above it. */
function setLevel(g: Game, c: Corps, lvl: number): void {
  const k = kLevel(String(c.id));
  if (lvl < (c.full ?? 1)) g.flags[k] = lvl;
  else if (k in g.flags) delete g.flags[k];
}

/** Each living corps of a binding nation below its full is armed to its strength that day, and the opening is noted. */
function stampLevels(g: Game, nat: string): void {
  for (const c of g.corps) if (c.nat === nat && !c.dead && c.str < (c.full ?? 1)) g.flags[kLevel(String(c.id))] = c.str;
  g.flags[kOpen(nat)] = 1;
}

/**
 * The first day (staff.ts startOpen, after the garrisons are set and before anyone's battles): each corps of a nation
 * that binds is armed at its strength. An older whole-world save that was opened without it is stamped the same way at
 * the nation's next month (riflesMonth).
 */
export function openRifles(g: Game): void {
  for (const nat of Object.keys(g.nations)) if (binds(g, nat)) stampLevels(g, nat);
}

/** A fold (fold.ts): the kept corps' level rises to its new strength where that is higher; the men moved came armed. */
export function riflesFold(g: Game, keep: Corps): void {
  if (binds(g, keep.nat) && keep.str > levelOf(g, keep)) setLevel(g, keep, keep.str);
}

/**
 * A fold's leftover men, sent back to the reserve, bring their rifles back to the store (the design director's answer 4
 * of 19:08Z, 25 November 2026: "Men and rifles travel together; only a broken corps loses them"), in the nation's
 * divisions' worth (so whole rifles are that times its perDiv). Wherever the nation's orders and drafts draw, a row
 * that only counts included (the lead's reading: it follows the draw).
 */
export function riflesBack(g: Game, nat: string, divs: number): void {
  if (carries(g, nat) && divs > 0) giveBack(g, nat, 'rifles', divs);
}

/** The liberation army (liberation.ts) comes armed at the strength it forms at. */
export function riflesFormed(g: Game, c: Corps): void {
  if (binds(g, c.nat)) setLevel(g, c, c.str);
}

/** A corps broken up (breakup.ts): the part carries the whole corps' level; each division keeps its share. */
export function riflesSplit(g: Game, c: Corps, p: Corps): void {
  const k = kLevel(String(c.id));
  if (binds(g, c.nat) && k in g.flags) g.flags[kLevel(String(p.id))] = g.flags[k];
}

/**
 * A part rejoining (breakup.ts, before the divisions are counted together): the corps' level is the two levels
 * weighted by divisions, never below the strength the rejoin gives; the part's own flag is struck.
 */
export function riflesJoin(g: Game, part: Corps, into: Corps): void {
  if (!binds(g, into.nat)) return;
  const a = into.divs, b = part.divs, all = a + b;
  if (all <= 0) return;
  const weighed = (levelOf(g, into) * a + levelOf(g, part) * b) / all, str = Math.max(0, Math.min(1, (into.str * a + part.str * b) / all));
  setLevel(g, into, Math.max(weighed, str));
  const k = kLevel(String(part.id));
  if (k in g.flags) delete g.flags[k];
}

// ---- The drafts (stage B2) --------------------------------------------------------------------------------------------
// Clause 6 and the form's section 12 share split, as ruled: for a nation that binds, a foot corps' drafts (a kind that
// needs no industry) cost men and rifles and no points, a heavy corps' drafts cost their points and no rifles; the
// depots' and the men's limits stay common, the treasury limits only the drafts paid in points, the stock only the drafts
// that draw rifles. A draft refilling a corps to its level draws 'refit' of what it brings in men (15D); above the level
// it would draw the whole, which the cap below keeps from happening (the director's answer 1: drafts fill to the lower
// of size and level). A row marked binds: false keeps today's drafts and counts what they draw.

const isFoot = (g: Game, c: Corps): boolean => needsOf(g, c.kind) === 0;

/** Each corps' want of drafts capped at its armed level, in place, for a nation that binds; untouched for every other. */
export function armWants(g: Game, nat: string, army: Corps[], want: number[]): void {
  if (!binds(g, nat)) return;
  army.forEach((c, i) => { if (want[i] > 0) want[i] = Math.min(want[i], Math.max(0, levelOf(g, c) - c.str) * c.divs); });
}

/** A binding nation's drafts this month: what each corps is given, the points it pays a division's worth, and where it is foot. */
export interface ArmShares { give: number[]; price: number[]; foot: boolean[]; room: number[] }

/**
 * The share split for a nation that binds (null for every other, whose drafts keep today's one share). `common` is the
 * depots' and the men's limit, `treasury` the points and the held call-up men in divisions' worth.
 */
export function armShares(g: Game, nat: string, army: Corps[], want: number[], common: number, treasury: number, perDiv: number): ArmShares | null {
  if (!binds(g, nat)) return null;
  const refit = pos(riflesRow(g, nat)!.refit), stock = stockOf(g, nat, 'rifles');
  const foot = army.map((c) => isFoot(g, c));
  let footAsk = 0, heavyAsk = 0;
  want.forEach((w, i) => { if (foot[i]) footAsk += w; else heavyAsk += w; });
  const footShare = Math.min(common, footAsk > 0 && refit > 0 ? stock / (refit * footAsk) : 1);
  const heavyShare = Math.min(common, heavyAsk > 0 ? treasury / heavyAsk : 1);
  return {
    give: want.map((w, i) => w * (foot[i] ? footShare : heavyShare)),
    price: foot.map((f) => (f ? 0 : perDiv)),
    foot,
    // (Room below each corps' level before the drafts: what a draft brings beyond it would draw whole.)
    room: army.map((c) => Math.max(0, levelOf(g, c) - c.str) * c.divs),
  };
}

/**
 * The rifles the month's drafts drew, after they are given (game.ts startPhase): for a nation that carries, 'refit' of
 * each foot corps' draft up to its level and the whole above it; heavy drafts draw none. A binds: false row's drafts are
 * today's, want x share, and draw 'refit' of it.
 */
export function armDrafts(g: Game, nat: string, army: Corps[], want: number[], share: number, arm: ArmShares | null): void {
  if (!carries(g, nat)) return;
  const refit = pos(riflesRow(g, nat)!.refit);
  let need = 0;
  army.forEach((c, i) => {
    if (!(arm ? arm.foot[i] : isFoot(g, c))) return;
    const give = arm ? arm.give[i] : want[i] * share;
    if (!(give > 0)) return;
    const room = arm ? arm.room[i] : give;
    need += refit * Math.min(give, room) + Math.max(0, give - room);
  });
  if (need <= 0) return;
  const got = draw(g, nat, 'rifles', need);
  riflesWatch.tell?.({ nat, what: 'draft', rifles: got });
}

// ---- The carve (stage B2) ---------------------------------------------------------------------------------------------
// The design director's 05:37Z, point 2 (clause 6, the form's section 9): a corps is priced once. Where the store
// charges the rifles, the price stops charging points for them: an infantry division's worth at the nation's own price
// of infantry comes off every kind, by its divisions (16 points a division at list price).

/** The points a corps of this many divisions no longer pays for its rifles: 0 unless the nation binds. */
export function carve(g: Game, nat: string, divs: number): number {
  if (!binds(g, nat)) return 0;
  const inf = (production as any).builds.inf.divs;
  return inf > 0 ? (pos(divs) * corpsList(g, nat, 'inf')) / inf : 0;
}

/** A list price less the carve, to a tenth and never below 0; the list price itself, untouched, where nothing is carved. */
export function carved(g: Game, nat: string, list: number, divs: number): number {
  const c = carve(g, nat, divs);
  return c > 0 ? Math.max(0, Math.round((list - c) * 10) / 10) : list;
}

// ---- The offer (stage B2) ---------------------------------------------------------------------------------------------

/**
 * The rifles an order of this many divisions takes, for the offer a page reads: set only where the nation carries.
 * Words to the UI director, not wired (the offer's note is made in campaign-bridge.ts for the 3D game and in ui/main.ts
 * for the page, from Offer.men): "Takes 3 divisions' worth of men and 64,000 rifles." in place of "Takes 45,000 men
 * from the reserve.", the rifles to the thousand.
 */
export const riflesFor = (g: Game, nat: string, divs: number): { rifles?: number } => (carries(g, nat) ? { rifles: Math.round(divs * riflesRow(g, nat)!.perDiv) } : {});

/**
 * Why the store refuses an order of this many divisions, or null where it does not. Words to the UI director (the
 * form's section 7): "Not rifles enough: this corps takes 64,000 and 21,000 are in store. 8,300 come in each month, so
 * it can be ordered in 6 months." The corps' rifles to the thousand, the store and the inflow to the hundred, the
 * months from the live inflow; with nothing coming in, the clause after "store." is left out (words owed).
 */
export function noRifles(g: Game, nat: string, divs: number): string | null {
  if (canArm(g, nat, divs)) return null;
  const per = riflesRow(g, nat)!.perDiv, need = divs * per, stock = stockOf(g, nat, 'rifles') * per, inflow = inflowOf(g, nat).total;
  const months = inflow > 0 ? Math.ceil((need - stock) / inflow) : 0;
  const when = inflow > 0 ? ` ${n100(inflow)} come in each month, so it can be ordered in ${months} month${months === 1 ? '' : 's'}.` : '';
  return `Not rifles enough: this corps takes ${n0(Math.round(need / 1000) * 1000)} and ${n100(stock)} are in store.${when}`;
}

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
  // War with the seller, first, as the lanes read it (resources.ts laneStopBy): the seller's own ground on the road is
  // the seller's, so its war is told in the seller's words, never as a place held.
  if (road.seller && warWith(g, to, road.seller)) return { state: 'cut', why: `${nameOf(g, to, true)} is at war with ${nameOf(g, road.seller)}, which sold the cargoes.`, by: 'seller', who: road.seller };
  // An enemy at war with the receiver holding any place on it, the first in the road's order. A neutral or a friend lets it by.
  for (const name of road.past ?? []) {
    const r = g.byName[name];
    if (r && r.ctrl !== to && warWith(g, r.ctrl, to)) return { state: 'cut', why: `${nameOf(g, r.ctrl, true)} holds ${r.name} and lets nothing past.`, by: 'place', at: r.name, who: r.ctrl };
  }
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
  // A seller that has fallen sells nothing, shelf or none: a capitulated nation's wars are ended, so the interim test
  // below would read it as at peace.
  if (!g.nations[seller]?.alive) return { ...out, state: 'dry', why: `${nameOf(g, seller, true)} has fallen and sells nothing.` };
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
  // (An older whole-world save, opened before the armed level was kept: each corps is armed at its strength now, once.)
  if (binds(g, nat) && g.flags[kOpen(nat)] === undefined) stampLevels(g, nat);
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

/**
 * The news of a road, once: the month it goes from running to cut, and the month a cut road runs again with its load.
 * A road running dry (its seller sends nothing) is no news either way, and a cut road that comes back dry is not told
 * as running: it is told when its rifles can come. Told to the receiver's seat only, as the books tell a seat.
 *
 * But a seller's fall is news once (the design director's 18:30Z of 25 November 2026): the month a road is first found
 * dry because its seller has fallen, its receiver's seat is told so, and never again for that road while the seller
 * stays fallen, whether the road is cut and freed meanwhile or not. That it was told is kept in the road's own flag,
 * as half a month further from 0 (so its sign, and the month it was cut or ran dry, still read as before); the half
 * comes off when the seller stands again. A seller that stands and sends nothing, at war or with nothing to spare, is
 * still no news. A road whose seller has fallen when it is first read is not news either.
 *
 * (So a war nobody plays gains only the rflc: flags, and a whole-save print of one, as the identity runs make, can
 * never show a headline of this module's. A played receiver's save gains one headline at each change told here.)
 */
function roadNews(g: Game, nat: string, road: RoadRow, now: RoadNow): void {
  const k = kCut(road.id);
  let was = g.flags[k];
  const mark = now.state === 'cut' ? g.turn : now.state === 'dry' ? -g.turn : 0;
  const gone = !!road.seller && !g.nations[road.seller]?.alive, fell = gone && now.state === 'dry';
  // (The half that says the fall was told comes off once the seller stands again.)
  if (was !== undefined && !gone && !Number.isInteger(was)) g.flags[k] = was = Math.trunc(was);
  const told = was !== undefined && !Number.isInteger(was);
  /** A mark with the told half: half a month further from 0, so its sign is kept. */
  const half = (x: number): number => (x > 0 ? x + 0.5 : x < 0 ? x - 0.5 : x);
  // (The first reading of a road is no change: a road cut on the first day is not news, nor a seller fallen by then.)
  if (was === undefined) { g.flags[k] = gone ? half(mark) : mark; return; }
  const wasCut = was > 0, wasDry = was < 0;
  if (fell) {
    if (told) return;
    // (A cut road keeps its cut month, so that its running again is told when the seller stands again.)
    g.flags[k] = half(wasCut || wasDry ? was : mark);
    if (nat === g.human) { const says = fallWords(g, road); if (says) headline(g, 'info', says, nat); }
    return;
  }
  let says = '';
  if (now.state === 'cut') {
    if (wasCut) return;
    says = cutWords(g, road, now, wasDry);
  } else if (now.state === 'dry') {
    // (Kept as cut while a cut road comes back dry; open to dry is marked, and told nothing.)
    if (wasCut || wasDry) return;
  } else {
    if (was === 0) return;
    if (wasCut) says = runsWords(g, road);
  }
  g.flags[k] = told ? half(mark) : mark;
  if (says && nat === g.human) headline(g, 'info', says, nat);
}

/**
 * Words to the UI director, placeholders until hers come: the news of a seller's fall, from the far side's rows
 * ('fallen', beside the roads: "{seller} has fallen. {road} carries no more rifles."). Nothing is told where no words
 * are laid.
 */
function fallWords(g: Game, road: RoadRow): string {
  const words = typeof RIFLES.fallen === 'string' ? RIFLES.fallen : '';
  return words && road.seller ? words.replace('{seller}', nameOf(g, road.seller, true)).replace('{road}', cap(road.name)) : '';
}

/**
 * Words to the UI director: the news of a cut (the form's section 7, "The Burma Road is cut: Japan holds Lashio.
 * 5,000 fewer rifles reach your store each month."), figures from the data.
 */
function cutWords(g: Game, road: RoadRow, now: RoadNow, wasDry = false): string {
  const why = now.by === 'place' ? `${nameOf(g, now.who!, true)} holds ${now.at}.` : now.why;
  // (A road that was running dry brought nothing, so nothing is counted lost.)
  const lost = now.full > 0 && !wasDry ? ` ${n100(now.full)} fewer rifles reach your store each month.` : ' It carried no rifles to your store.';
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
 *
 * (Two things another seat can still work out, for the UI director to rule on. A road whose seller has a rifles shelf
 * and less to spare than the road's load shows rifles equal to the seller's spare: a third nation's stock, read off
 * the road. No seller has a shelf on this line, so it is not live yet. The inflows another seat has read since the
 * first day no longer add up to the stock: the nation's new corps draw from it, and what they drew is never shown.)
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
