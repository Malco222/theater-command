// Fighter types (the setup switch 'Fighter types', g.flags.ftrtypes; off as shipped). Each nation flies its real
// fighters, by research stage: the Bf 109 E, then the F and the Fw 190, and so on. Where two air forces meet, the
// fighters' measured performance at the mission's height gives one side a small edge in the count: speed, climb,
// dive and roll, turn, toughness and weight of fire. Pilots (air.json's quality), numbers and radar count for more.
// The rows and ladders are data/campaign/fighters.json, written by the fighters research (fighters-table.json and
// fighter-types-design.md in air-battles/fighters). Off, nothing here runs: no field, no flag and no die.

import data from '../../data/campaign/fighters.json';
import type { AirFleet, AirMission, Game } from './types';
import { atWar, headline, inDesert } from './game';
import { techMul } from './research';
import { treeLevel } from './tech';
import { hasAircraftWorks } from './arsenal';
import { designYear } from '../battle/air/doctrine';
import { armsOf, unitsDecide, wayNow } from './units';

export type Band = 'low' | 'mid' | 'high';
export type Mix = Record<string, number>;

interface TypeRow {
  short: string; name: string;
  /** Speed (km/h) and climb (m/s) at the three bands, low, mid and high. */
  v: [number, number, number]; c: [number, number, number];
  /** Least sustained turn radius (m), roll rate (deg/s), dive limit (km/h), how quick into a dive (1 to 3), toughness (0 to 3), weight of fire (kg/s). */
  r: number; roll: number; dive: number; acc: number; tough: number; fire: number;
  plain: string; history: string;
  /**
   * The law's measures (the historian, 23 Nov 06:56Z; the design director 06:55Z and 07:00Z): top speed at rated height
   * (km/h), wing loading (kg/m2, loaded weight over wing area), climb (m/s), limiting dive speed (km/h), and which are laid.
   */
  top?: number; wl?: number; climb?: number; diveLimit?: number;
  laid?: { speed: boolean; turn: boolean; climb: boolean; dive: boolean };
  /** The year the mark first went to the front (the historian's in-service date): its design year for stolen plans. */
  year?: number;
}
interface Ladder {
  clock: 'research' | 'calendar';
  history: [string, Mix][];
  rungs: Record<string, [number, Mix][]>;
  deck?: [string, Mix][];
}

/** A nation's doctrine and pilots by month, laid only once read (the design director, 23 Nov 07:00Z). */
export type Doctrine = 'turn' | 'dive-zoom';
export const FIGHTERS = data as unknown as {
  law: { k: number; fitted: boolean; widest: number | null };
  doctrine: { rows: Record<string, [string, Doctrine][]> };
  hours: { rows: Record<string, [string, number][]> };
  words: { setting: string; help: string; tooltip: string; realWar: string };
  w: { speed: number; speedScale: number; climb: number; climbScale: number; dive: number; turn: number; turnScale: number; refuse: number; punch: number; fireScale: number; cap: number };
  bands: Record<Band, number>;
  byMission: Record<string, Band | null>;
  swap: Record<string, string>;
  types: Record<string, TypeRow>;
  ladders: Record<string, Ladder>;
};

const BI: Record<Band, number> = { low: 0, mid: 1, high: 2 };

/**
 * True when the campaign was set up with fighter types, or with Units decide, which folds the dogfight switch in
 * (BO:2093; the #2 form, Part 2): planes, pilots and doctrine decide the fighters' fight under the one switch.
 */
export const fighterTypesOn = (g: Game): boolean => g.flags.ftrtypes === 1 || unitsDecide(g);

/**
 * The law's k as the game reads it: the fitted k under Units decide only (fit-gc-1124.md: one anchor, L). It was fitted
 * with the store's hours read and the held level leaving a laid pair, so the Fighter types switch alone, which has
 * neither, keeps no k (the same anchor asks 2.7 of it with the held levels in) and gives no edge.
 */
export const lawK = (g: Game): number => (unitsDecide(g) ? FIGHTERS.law.k : 0);

/** Sets the switch. Only at setup, before the first month has been played; returns whether it was set. */
export function setFighterTypes(g: Game, on: boolean): boolean {
  if (g.turn !== 1 || g.phase !== 0 || g.airReports.length || g.reports.length) return false;
  if (on) g.flags.ftrtypes = 1;
  else delete g.flags.ftrtypes;
  return true;
}

/** The height band a mission is fought at, or null at night (no matchup in the dark). */
export const bandOf = (m: AirMission): Band | null => (m in FIGHTERS.byMission ? FIGHTERS.byMission[m] : 'mid');

const clip = (x: number, a = -1, b = 1): number => Math.max(a, Math.min(b, x));

/** The parts of type A's edge over type B at a band, in log units (the research's matchup.py, line for line). */
export function edgeParts(A: TypeRow, B: TypeRow, band: Band): { speed: number; climb: number; diveRoll: number; turn: number; punch: number } {
  const w = FIGHTERS.w, k = BI[band];
  const dv = A.v[k] - B.v[k];
  const speed = w.speed * clip(dv / w.speedScale);
  const climb = w.climb * clip((A.c[k] - B.c[k]) / w.climbScale);
  const dInner = clip(0.5 * (A.dive - B.dive) / 100 + 0.25 * (A.acc - B.acc) + 0.5 * (A.roll - B.roll) / 60);
  const diveRoll = w.dive * dInner;
  const tRaw = w.turn * clip((B.r - A.r) / (w.turnScale * Math.min(A.r, B.r)));
  // The better turner wins a turning fight only if the other side cannot refuse it, by speed or by a dive it cannot follow.
  const free = tRaw > 0 ? clip(Math.max(0, -dv) / w.refuse + 0.5 * Math.max(0, -dInner), 0, 1) : clip(Math.max(0, dv) / w.refuse + 0.5 * Math.max(0, dInner), 0, 1);
  const turn = tRaw * (1 - free);
  const fa = Math.max(0.2, A.fire || 0.2), fb = Math.max(0.2, B.fire || 0.2);
  const punch = w.punch * clip(0.5 * (A.tough - B.tough) / 2 + 0.5 * Math.log(fa / fb) / Math.log(w.fireScale));
  return { speed, climb, diveRoll, turn, punch };
}

/**
 * Type A's edge over type B, in log units (the design director, 23 Nov 2026 06:55Z and 07:00Z; data 'law'): k times the
 * sum of the log ratios that apply. Top speed at rated height always: the faster side chooses whether to fight. A side
 * whose doctrine is the turning fight brings wing loading (the lower turns tighter); a side whose doctrine is
 * dive-and-climb brings climb and limiting dive speed. Doctrine only picks the term; with none laid for a side, it
 * brings neither. A figure not laid makes its term 1 (log 0). k is fitted to the record's exchange anchors; until then
 * it is 0 and no edge is given. No cap but the widest anchor. Antisymmetric. `band` is kept for the callers; the
 * law reads one measure a type.
 */
export function edge(a: string, b: string, band: Band, docA?: Doctrine, docB?: Doctrine, k = FIGHTERS.law.k): number {
  void band;
  const A = FIGHTERS.types[a], B = FIGHTERS.types[b], L = FIGHTERS.law;
  if (!A || !B || !k) return 0;
  const both = (k: keyof NonNullable<TypeRow['laid']>): boolean => !!(A.laid?.[k] && B.laid?.[k]);
  const ln = (x: number | undefined, y: number | undefined): number => (x && y ? Math.log(x / y) : 0);
  let t = both('speed') ? ln(A.top, B.top) : 0;
  // Each side brings the terms its own doctrine fights by: the turner its wing loading, the diver its climb and dive.
  const terms = (doc: Doctrine | undefined, sign: number): number => {
    if (doc === 'turn') return both('turn') ? sign * ln(B.wl, A.wl) : 0;
    if (doc === 'dive-zoom') return sign * ((both('climb') ? ln(A.climb, B.climb) : 0) + (both('dive') ? ln(A.diveLimit, B.diveLimit) : 0));
    return 0;
  };
  t += terms(docA, 1) + terms(docB, 1);
  const e = k * t;
  return L.widest ? clip(e, -L.widest, L.widest) : e;
}

/** One mix's mean edge over another's, each pair weighted by both shares. */
export function mixEdge(a: Mix, b: Mix, band: Band, docA?: Doctrine, docB?: Doctrine, k = FIGHTERS.law.k): number {
  let t = 0, s = 0;
  for (const [x, sx] of Object.entries(a)) for (const [y, sy] of Object.entries(b)) { t += sx * sy * edge(x, y, band, docA, docB, k); s += sx * sy; }
  return s > 0 ? t / s : 0;
}

/** A month 'YYYY-MM' as a turn: September 1939 is 1 (research.ts's own count). */
export const turnOfMonth = (ym: string): number => {
  const [y, m] = ym.split('-').map(Number);
  return (y - 1939) * 12 + m - 8;
};

const rowAt = <T>(rows: [string, T][], t: number): number => {
  let k = 0;
  rows.forEach(([w], i) => { if (turnOfMonth(w) <= t) k = i; });
  return k;
};

/**
 * The month a nation's fighters have reached, as a turn. On the research clock (research.json's nations) it is the
 * calendar shifted by the months the fighter branch runs ahead of history (or behind it); otherwise the calendar.
 */
export function fighterClock(g: Game, nat: string): number {
  const lab = g.nations[nat]?.lab;
  return FIGHTERS.ladders[nat]?.clock === 'research' && lab ? g.turn + lab.clock.fighters - lab.months : g.turn;
}

/** The grand campaign's fighter stage: how many of the tree's fighter technologies the nation has (0 to 3). */
const stageOf = (g: Game, nat: string, L: Ladder): string => {
  const k = String(treeLevel(g, nat, 'fighters'));
  if (L.rungs[k]) return k;
  const keys = Object.keys(L.rungs).sort();
  return keys[keys.length - 1];
};

/** Which row of its ladder a nation flies now: a key for the news, and the mix. */
function landRow(g: Game, nat: string): { key: number; mix: Mix } | null {
  const L = FIGHTERS.ladders[nat];
  if (!L) return null;
  if (g.flags.open === 1) {
    const s = stageOf(g, nat, L);
    const since = g.flags[`ftr:${nat}:${s}`] ?? g.turn;
    const rows = L.rungs[s];
    let k = 0;
    rows.forEach(([m], i) => { if (m <= g.turn - since) k = i; });
    return { key: Number(s) * 100 + k + 1, mix: rows[k][1] };
  }
  const k = rowAt(L.history, fighterClock(g, nat));
  return { key: k + 1, mix: L.history[k][1] };
}

/** True for a carrier wing, at sea or put ashore. */
const deckBred = (f: AirFleet): boolean => f.ship !== undefined || f.ashore !== undefined;

/** The types an air fleet flies, as shares: its nation's mix (a carrier wing's from the deck ladder), with the tropical Spitfire V in the desert. */
export function typeMix(g: Game, f: AirFleet): Mix | null {
  const L = FIGHTERS.ladders[f.nat];
  const bought = Object.entries(f.bought ?? {}).filter(([k, s]) => s > 0 && FIGHTERS.types[k]);
  const stolen = !deckBred(f) ? stolenLead(g, f.nat, L ? landRow(g, f.nat)!.mix : {}) : null;
  if (!L && !bought.length && !stolen) return null;
  let mix: Mix = {};
  if (L && deckBred(f) && L.deck) mix = L.deck[rowAt(L.deck, g.turn)][1];
  else if (stolen) mix = stolen;
  else if (L) mix = landRow(g, f.nat)!.mix;
  // Bought wings fly the seller's type; the group's own share flies the nation's own (07:00Z).
  if (bought.length) {
    const shareBought = Math.min(1, bought.reduce((a, [, s]) => a + s, 0)), own = L ? 1 - shareBought : 0;
    const out: Mix = {};
    for (const [k, s] of Object.entries(mix)) out[k] = (out[k] ?? 0) + s * own;
    for (const [k, s] of bought) out[k] = (out[k] ?? 0) + s / (L ? 1 : shareBought);
    mix = out;
  }
  // The Spitfire Vs of the desert, Malta and the far theatres carried a dust filter that cost them speed and climb.
  const base = g.regions[f.base];
  if (!deckBred(f) && base && inDesert(base) && Object.keys(mix).some((k) => FIGHTERS.swap[k])) {
    const out: Mix = {};
    for (const [k, s] of Object.entries(mix)) out[FIGHTERS.swap[k] ?? k] = (out[FIGHTERS.swap[k] ?? k] ?? 0) + s;
    mix = out;
  }
  return mix;
}

/**
 * A group in the fighters' fight with its fighting weight. Under Units decide the caller adds its pilots' mean hours
 * (h, air.ts hoursOf: null while its school is not read) and its held level (q, air.ts heldOf), which the pair reads.
 */
export interface Weighed { f: AirFleet; w: number; h?: number | null; q?: number }

/** The type a great power has in production now, its ladder's largest share: what a wing it sells flies (07:00Z). */
export function sellerType(g: Game, nat: string): string | null {
  const row = FIGHTERS.ladders[nat] ? landRow(g, nat) : null;
  if (!row) return null;
  return Object.entries(row.mix).sort((a, b) => b[1] - a[1])[0]?.[0] ?? null;
}

/** A bought wing of `count` fighters of type `mark` joins group `f` of `had` fighters: its share of the group is kept. */
export function joinBought(f: AirFleet, mark: string, had: number, count: number): void {
  const all = had + count;
  if (all <= 0) return;
  const b = (f.bought ??= {});
  for (const k of Object.keys(b)) b[k] = (b[k] * had) / all;
  b[mark] = (b[mark] ?? 0) + count / all;
}

// ---- The fighter's plans (the design director, 23 Nov 07:00Z, on Brian's 06:50Z; intel.ts 'fighterPlans') ----------
// Plans give the design; research gives the means. A mark stolen joins the thief's table and is built once the thief's
// own designers have reached its year. Then, as the latest mark in its table, it takes the place of the mark the
// thief builds most. One rule for every nation with aircraft works, of any other's fielded mark.

/** The marks a nation holds from others' plans, newest last. */
export const stolenMarks = (g: Game, nat: string): { mark: string; from: string; turn: number }[] => g.nations[nat]?.stolenMarks ?? [];

/** True for a nation with aircraft works of its own that build fighters (arsenal.ts hasAircraftWorks), open yet or not. */
export const hasFighterWorks = (g: Game, nat: string): boolean => hasAircraftWorks(g, nat, 'ftr');

/**
 * A nation's land mix with its stolen marks: the newest stolen mark whose year its designers have reached, if it is
 * newer than every mark in the mix, takes the leading mark's share. Null where no stolen mark is built (or the switch is off).
 */
function stolenLead(g: Game, nat: string, mix: Mix): Mix | null {
  const got = g.nations[nat]?.stolenMarks;
  if (!got?.length || !fighterTypesOn(g)) return null;
  const reach = designYear(g, nat, 'fighters');
  const newest = Math.max(0, ...Object.keys(mix).map((k) => FIGHTERS.types[k]?.year ?? 0));
  const best = got.map((x) => x.mark).filter((k) => (FIGHTERS.types[k]?.year ?? Infinity) <= reach && FIGHTERS.types[k].year! > newest)
    .sort((a, b) => FIGHTERS.types[b].year! - FIGHTERS.types[a].year!)[0];
  if (!best) return null;
  const lead = Object.entries(mix).sort((a, b) => b[1] - a[1])[0];
  if (!lead) return { [best]: 1 };
  const out: Mix = { ...mix };
  delete out[lead[0]];
  out[best] = (out[best] ?? 0) + lead[1];
  return out;
}

/** The mark a nation fields most now, stolen marks counted: what its plans would give a thief. Null with no mark of its own. */
export function fieldedMark(g: Game, nat: string): string | null {
  const own = FIGHTERS.ladders[nat] ? landRow(g, nat)!.mix : {};
  const mix = stolenLead(g, nat, own) ?? own;
  return Object.entries(mix).sort((a, b) => b[1] - a[1])[0]?.[0] ?? null;
}

/** True where the mark is in the nation's own table already: its ladder at any time, or plans it holds. */
export function holdsMark(g: Game, nat: string, mark: string): boolean {
  const L = FIGHTERS.ladders[nat];
  const inMix = (rows: [unknown, Mix][] | undefined): boolean => !!rows?.some(([, m]) => m[mark] !== undefined);
  return (!!L && (inMix(L.history) || Object.values(L.rungs).some(inMix) || inMix(L.deck))) || stolenMarks(g, nat).some((x) => x.mark === mark);
}

/** The thief's sentence once it holds the plans: when its works can build the mark. */
export function stolenWords(g: Game, nat: string, mark: string): string {
  const y = FIGHTERS.types[mark]?.year ?? 0, short = FIGHTERS.types[mark]?.short ?? mark;
  return designYear(g, nat, 'fighters') >= y ? `Your aircraft works can build the ${short} now.` : `Your aircraft works can build the ${short} once your designers reach ${y}.`;
}

/** The plans of a mark got out: it joins the thief's table. False where the thief holds it already. */
export function stealMark(g: Game, nat: string, from: string, mark: string): boolean {
  const N = g.nations[nat];
  if (!N || !FIGHTERS.types[mark] || holdsMark(g, nat, mark)) return false;
  (N.stolenMarks ??= []).push({ mark, from, turn: g.turn });
  return true;
}

/**
 * A nation's doctrine in the month, where it is laid; undefined where it is not (07:00Z: held rows are not laid). Under
 * Units decide, its fighter Way in the quality store, null (no term) where no row is read (#2 form Part 2 section 3).
 */
export function doctrineOf(g: Game, nat: string): Doctrine | undefined {
  if (unitsDecide(g)) { const w = armsOf(g, nat)?.air.fighter; return w ? wayNow(g, w) : undefined; }
  const rows = FIGHTERS.doctrine.rows[nat];
  return rows?.length ? rows[rowAt(rows, g.turn)][1] : undefined;
}

/** The pilots' edge from training hours before first combat, k times the log ratio; 0 where either side's is not laid. */
export function hoursEdge(g: Game, a: string, b: string, k = FIGHTERS.law.k): number {
  const R = FIGHTERS.hours.rows;
  if (!k || !R[a]?.length || !R[b]?.length) return 0;
  return k * Math.log(R[a][rowAt(R[a], g.turn)][1] / R[b][rowAt(R[b], g.turn)][1]);
}

/**
 * Under Units decide, the pilots' edge of one group over another (the #2 form, Part 2 section 1): k times the log ratio
 * of the two groups' mean hours, read from the store, with the held level leaving the pair, since the hours now price
 * the pilots it stood in for. Only when k > 0 and both groups' hours are laid; in every other pair both sides keep
 * their level and this is 0.
 */
export function pilotsEdge(a: Weighed, d: Weighed, k: number): number {
  if (!(k > 0) || !a.h || !d.h || !a.q || !d.q) return 0;
  return k * Math.log(a.h / d.h) - Math.log(a.q / d.q);
}

/**
 * The edge of the escort's fighters over the defenders', in log units: the mean over every pair of an escorting and a
 * defending group, weighted by both groups' fighting weight, of their mixes' edge at the mission's height. A research
 * lead the count already gives through techMul is taken off first (guard A), so it is not counted twice. Under Units
 * decide the pilots' hours come in group by group, and the held level leaves a pair whose hours are both laid
 * (pilotsEdge). 0 when there is nothing to weigh or no height to fight at.
 */
export function typeEdge(g: Game, att: Weighed[], def: Weighed[], m: AirMission): number {
  const band = bandOf(m);
  if (!band) return 0;
  const ud = unitsDecide(g), k = lawK(g);
  const mixes = new Map<AirFleet, Mix | null>();
  const mixOf = (f: AirFleet): Mix | null => { if (!mixes.has(f)) mixes.set(f, typeMix(g, f)); return mixes.get(f)!; };
  let t = 0, s = 0;
  for (const a of att) {
    if (a.w <= 0) continue;
    for (const d of def) {
      if (d.w <= 0) continue;
      const ma = mixOf(a.f), md = mixOf(d.f);
      const plane = ma && md ? mixEdge(ma, md, band, doctrineOf(g, a.f.nat), doctrineOf(g, d.f.nat), k) : 0;
      // Under Units decide the pilots come from the store, group by group (pilotsEdge); else the dated rows by nation.
      const e = ud ? plane + pilotsEdge(a, d, k) : ma && md ? plane + hoursEdge(g, a.f.nat, d.f.nat, k) : 0;
      // M replaces fighterTech in the fighters' worth against each other (06:55Z): the research lead the count gives
      // through techMul is taken off, so the plane is priced once, by its type.
      const lead = Math.log(techMul(g, a.f.nat, 'fighters') / techMul(g, d.f.nat, 'fighters'));
      t += a.w * d.w * (e - lead);
      s += a.w * d.w;
    }
  }
  return s > 0 ? t / s : 0;
}

// ---- The month: stages and new types ------------------------------------------------------------------------

/** The type named by its short name and its first plain sentence, for the news. */
const firstSentence = (id: string): string => {
  const p = FIGHTERS.types[id]?.plain ?? '';
  const k = p.search(/[.!?](\s|$)/);
  return k >= 0 ? p.slice(0, k + 1) : p;
};

/** The news line of a type entering service: the player's own, or another nation's. */
export function serviceNews(g: Game, nat: string, id: string): string {
  const t = FIGHTERS.types[id];
  return nat === g.human
    ? `The ${t.short} enters service. ${firstSentence(id)}`
    : `${g.nations[nat].name} puts the ${t.short} into service. ${firstSentence(id)}`;
}

/**
 * A nation's month for its fighters, when the switch is on: in the grand campaign the month it reached its fighter
 * stage is written down (the tree keeps what a nation has, not when); and a type new to its fighters is news, for
 * the player's own nation and for his enemies.
 */
export function fighterMonth(g: Game, nat: string): void {
  if (!fighterTypesOn(g)) return;
  const L = FIGHTERS.ladders[nat];
  if (!L) return;
  if (g.flags.open === 1) {
    const s = stageOf(g, nat, L);
    if (g.flags[`ftr:${nat}:${s}`] === undefined) g.flags[`ftr:${nat}:${s}`] = g.turn;
  }
  const row = landRow(g, nat)!;
  const was = g.flags[`ftrrow:${nat}`];
  if (was === row.key) return;
  g.flags[`ftrrow:${nat}`] = row.key;
  if (was === undefined) return;
  const prev = prevMix(g, nat, was);
  const fresh = Object.keys(row.mix).filter((k) => !prev || !(k in prev));
  if (!fresh.length || !g.human || (nat !== g.human && !atWar(g, nat, g.human))) return;
  for (const id of fresh) headline(g, 'info', serviceNews(g, nat, id), nat);
}

const prevMix = (g: Game, nat: string, key: number): Mix | null => {
  const L = FIGHTERS.ladders[nat];
  if (key >= 100 || g.flags.open === 1) {
    const rows = L.rungs[String(Math.floor(key / 100))];
    return rows?.[(key % 100) - 1]?.[1] ?? null;
  }
  return L.history[key - 1]?.[1] ?? null;
};

// ---- What the player reads ------------------------------------------------------------------------------------

const height = (b: Band): string => FIGHTERS.bands[b].toLocaleString('en-US');

/** Names on a card: three a side, then "and N more". */
export function namesOf(ids: string[]): string {
  const names = ids.map((i) => FIGHTERS.types[i]?.short ?? i);
  const shown = names.slice(0, 3), more = names.length - shown.length;
  if (more > 0) return `${shown.join(', ')} and ${more === 1 ? 'one' : more} more`;
  return shown.length <= 1 ? shown.join('') : `${shown.slice(0, -1).join(', ')} and ${shown[shown.length - 1]}`;
}

/** The types one side flies, most first, weighted by the fighters each group brings. */
export function sideTypes(g: Game, side: Weighed[]): string[] {
  const tot: Mix = {};
  for (const x of side) {
    if (x.w <= 0) continue;
    const mix = typeMix(g, x.f);
    if (!mix) continue;
    for (const [k, s] of Object.entries(mix)) tot[k] = (tot[k] ?? 0) + s * x.w;
  }
  return Object.entries(tot).sort((a, b) => b[1] - a[1] || (a[0] < b[0] ? -1 : 1)).map(([k]) => k);
}

/**
 * The raid card's two lines under the odds (the UI director's words, 22 Nov): the types at the mission's height,
 * escort first, and which side's aircraft are stronger, as one figure. Null when the switch is off, at night, or
 * with no defenders to meet.
 */
export function raidCardLines(g: Game, a: { att: { f: AirFleet; ftr: number }[]; def: { f: AirFleet; w: number }[]; typeEdge?: number; pilotsLine?: string }, m: AirMission): { types: string; edge: string | null; pilots?: string } | null {
  if (!fighterTypesOn(g)) return null;
  const band = bandOf(m);
  if (!band) return null;
  const esc = sideTypes(g, a.att.map((x) => ({ f: x.f, w: x.ftr })));
  const def = sideTypes(g, a.def.map((x) => ({ f: x.f, w: x.f.ftr * x.w })));
  if (!def.length) return null;
  if (!esc.length) return { types: `Defending fighters at ${height(band)} m: ${namesOf(def)}.`, edge: null };
  const E = a.typeEdge ?? 0, x = Math.exp(Math.abs(E) / 2) - 1;
  const edge = x < 0.02 ? 'The aircraft are about even.' : `The ${E > 0 ? "escort's" : "defenders'"} aircraft are about ${Math.round(x * 100)}% stronger.`;
  // Under Units decide, the pilots' training beside the types line, where it is read on both sides (the UI director 24 Nov 18:10Z).
  return { types: `Fighters at ${height(band)} m: ${namesOf(esc)} against ${namesOf(def)}.`, edge, ...(a.pilotsLine ? { pilots: a.pilotsLine } : {}) };
}

/** The air group panel: the types a group flies, with the share of each, the short and full names and the two kinds of words. */
export function fighterPanel(g: Game, f: AirFleet): { id: string; short: string; name: string; share: number; plain: string; history: string }[] {
  if (!fighterTypesOn(g)) return [];
  const mix = typeMix(g, f);
  if (!mix) return [];
  return Object.entries(mix).sort((a, b) => b[1] - a[1]).map(([id, share]) => {
    const t = FIGHTERS.types[id];
    return { id, short: t.short, name: t.name, share, plain: t.plain, history: t.history };
  });
}
