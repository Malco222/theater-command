// Bomber hit rates on ships by kind (release 2; the design director's rulings of 22 November 2026, 18:13Z and 18:27Z,
// on the historian's counts). With the switch on, a strike on ships counts each bomber that released at its kind's rate
// for its navy (dive, torpedo, level, mast height), by whether fighters were over the ships or not, and at anchor by the
// surprises' own rows; the law's single 0.04, its naval skill, its spoil and the harbour's factors then give way. Off,
// nothing here is read and the law is as it was. data/campaign/hitrates.json holds the table and where each row comes from.
//
// The split by kind is the field's own: a carrier group's strike flies by the record's mix for its nation and year
// (deckMix, which the air battle's boxes fly by too). A land group (the design director's 18:40Z) flies by its type where
// the game carries one (a B-17 is a level bomber, a Stuka dives), else by the record's own count of its nation's strikes
// (Japan at Force Z), else by the historian's kinds for its nation and year (M), in equal shares while no share is found
// (the middle of the possible mixes, D; the design director's 18:45Z).

import data from '../../data/campaign/hitrates.json';
import { turnToDate } from '../core/ticket';
import type { Game } from './types';

export type HitKind = 'dive' | 'torpedo' | 'level' | 'mast';
export const HIT_KINDS: HitKind[] = ['dive', 'torpedo', 'level', 'mast'];
interface Rows { open: Record<string, number>; covered: Record<string, number> }

export const HITS = data as unknown as {
  on: boolean;
  sourced: boolean;
  underWay: Record<HitKind, Rows>;
  pool: Record<HitKind, { open: number; covered: number }>;
  anchor: { torpedo: number; level: number; diveTimes: number };
  kin: Record<string, string>;
  fixed: Record<string, Partial<Record<HitKind, number | null>>>;
  types: Record<string, HitKind>;
  landShares: Record<string, [number, Partial<Record<HitKind, number>>][]>;
  landKinds: Record<string, [number, HitKind[]][]>;
};

/** True in a campaign played with hit rates by kind: the grand campaign, begun with the switch on. */
export const hitRatesOn = (g: Game): boolean => g.flags.open === 1 && g.flags.hitKinds === 1;

/** True when a new grand campaign opens with hit rates by kind: only once the table is sourced (the land shares, the Mk 13's month). */
export const hitRatesShipped = (): boolean => HITS.on === true && HITS.sourced === true;

/** Turns hit rates by kind on or off (a new grand campaign as shipped, or the test kitchen). The bridge calls it by name. */
export function setHitRates(g: Game, on: boolean): void {
  if (on) g.flags.hitKinds = 1;
  else delete g.flags.hitKinds;
}

/** A month, as the air battle's doctrine reads it. */
interface When { year: number; month: number }
const at = (w: When): number => w.year + w.month / 12;
const COMMONWEALTH = new Set(['UK', 'CAN', 'AUS', 'NZL', 'SAF', 'IND']);

/**
 * The record's mix of a carrier strike by kind, for the historian: Pearl Harbor's two waves (40 torpedo, 103 level and
 * 129 dive bombers), the carrier battles of 1942 for strikes at sea, the Fleet Air Arm's Swordfish at Taranto. The air
 * battle's boxes fly by it (battle/air/ships.ts), and the law counts by it.
 */
export function deckMix(nat: string, w: When, m: 'harbour' | 'fleet'): ['torpedo' | 'dive' | 'level', number][] {
  const d = at(w);
  if (nat === 'JAP' || nat === 'JPN') return m === 'harbour' ? [['torpedo', 0.15], ['level', 0.38], ['dive', 0.47]] : [['torpedo', 0.45], ['dive', 0.55]];
  if (nat === 'USA') return d < 1943.5 ? [['torpedo', 0.25], ['dive', 0.75]] : [['torpedo', 0.35], ['dive', 0.65]];
  if (COMMONWEALTH.has(nat)) return m === 'harbour' ? [['torpedo', 0.55], ['dive', 0.45]] : [['torpedo', 0.8], ['dive', 0.2]];
  return [['torpedo', 0.4], ['dive', 0.6]];
}

const none = (): Record<HitKind, number> => ({ dive: 0, torpedo: 0, level: 0, mast: 0 });

/** A nation's row for the month from rows by [from turn, row] (its own, else its senior partner's); undefined before the first. */
function rowAt<T>(table: Record<string, [number, T][]>, nat: string, turn: number): T | undefined {
  const rows = table[nat] ?? table[HITS.kin[nat] ?? ''] ?? [];
  let out: T | undefined;
  for (const [from, row] of rows) if (turn >= from) out = row;
  return out;
}

/**
 * A land bomber group's kinds in a month: by its type where the game carries one ('types'); else by the record's count of
 * its nation's strikes ('landShares'); else by the historian's kinds for its nation ('landKinds', M), in equal shares
 * while no share is found; level with no row at all, as the law was.
 */
export function landMix(nat: string, turn: number, type?: string): Record<HitKind, number> {
  const out = none();
  const byType = type ? HITS.types[type] : undefined;
  if (byType) { out[byType] = 1; return out; }
  const share = rowAt(HITS.landShares, nat, turn);
  if (share) {
    const sum = HIT_KINDS.reduce((s, k) => s + (share[k] ?? 0), 0);
    if (sum > 0) { for (const k of HIT_KINDS) out[k] = (share[k] ?? 0) / sum; return out; }
  }
  const kinds = rowAt(HITS.landKinds, nat, turn) ?? [];
  if (!kinds.length) { out.level = 1; return out; }
  for (const k of kinds) out[k] += 1 / kinds.length;
  return out;
}

/** A group's split by kind: a carrier group's by the record's deck mix, a land group's by its type or its nation's kinds. */
export function mixOf(nat: string, deck: boolean, turn: number, harbour: boolean, type?: string): Record<HitKind, number> {
  if (!deck) return landMix(nat, turn, type);
  const d = turnToDate(turn), out = none();
  for (const [k, s] of deckMix(nat, { year: d.year, month: d.month }, harbour ? 'harbour' : 'fleet')) out[k] += s;
  return out;
}

/** A navy's own row under way for a kind, its senior partner's, or the pool; a row given way (fixed) from its month on. */
function underWay(nat: string, kind: HitKind, covered: boolean, turn: number): number {
  const rows = HITS.underWay[kind][covered ? 'covered' : 'open'];
  for (const n of [nat, HITS.kin[nat]]) {
    if (!n) continue;
    const fix = HITS.fixed[n]?.[kind];
    if (fix !== undefined && fix !== null && turn >= fix) continue;
    if (rows[n] !== undefined) return rows[n];
  }
  return HITS.pool[kind][covered ? 'covered' : 'open'];
}

/** Hits a bomber of this kind and navy scores in this month: under way by its cover; at anchor by the surprises' rows. */
export function hitRate(nat: string, kind: HitKind, covered: boolean, harbour: boolean, turn: number): number {
  if (!harbour) return underWay(nat, kind, covered, turn);
  if (kind === 'torpedo') return HITS.anchor.torpedo;
  if (kind === 'level') return HITS.anchor.level;
  if (kind === 'dive') return underWay(nat, kind, covered, turn) * HITS.anchor.diveTimes;
  return underWay(nat, kind, covered, turn);
}

/** What a strike's bombers are worth against ships by kind: the share of its bombers of each kind, and each kind's rate (weighed across its navies). */
export interface StrikeRates { mix: Record<HitKind, number>; rates: Record<HitKind, number>; land: Record<HitKind, number>; covered: boolean }

/** A strike's rates by kind, from its bomber groups (their nation, their bombers, whether they are carrier crews, and a land group's type). Null with no bombers. */
export function strikeRates(groups: { nat: string; bmr: number; deck: boolean; type?: string }[], turn: number, harbour: boolean, covered: boolean): StrikeRates | null {
  const B = groups.reduce((s, x) => s + Math.max(0, x.bmr), 0);
  if (B <= 0) return null;
  const n = none(), worth = none(), landN = none();
  for (const x of groups) {
    if (x.bmr <= 0) continue;
    const mix = mixOf(x.nat, x.deck, turn, harbour, x.type);
    for (const k of HIT_KINDS) {
      const b = x.bmr * mix[k];
      n[k] += b;
      worth[k] += b * hitRate(x.nat, k, covered, harbour, turn);
      if (!x.deck) landN[k] += b;
    }
  }
  const mix = none(), rates = none(), land = none();
  const L = HIT_KINDS.reduce((s, k) => s + landN[k], 0);
  for (const k of HIT_KINDS) {
    mix[k] = n[k] / B;
    rates[k] = n[k] > 0 ? worth[k] / n[k] : 0;
    land[k] = L > 0 ? landN[k] / L : k === 'level' ? 1 : 0;
  }
  return { mix, rates, land, covered };
}

const KIND_WORDS: Record<HitKind, [string, string]> = { dive: ['dive bomber', 'dive bombers'], torpedo: ['torpedo bomber', 'torpedo bombers'], level: ['level bomber', 'level bombers'], mast: ['mast-height bomber', 'mast-height bombers'] };

/** The raid card's line of hits by kind: "Hits: about 7 from the dive bombers, none from the torpedo bombers." Only the kinds that flew. */
export function hitsLine(kinds: Partial<Record<HitKind, number>>, flew: Partial<Record<HitKind, number>>): string {
  const parts = HIT_KINDS.filter((k) => (flew[k] ?? 0) > 0).map((k) => {
    const h = Math.round(kinds[k] ?? 0);
    return `${h === 0 ? 'none' : h === 1 ? 'about one' : `about ${h}`} from the ${KIND_WORDS[k][1]}`;
  });
  return parts.length ? `Hits: ${parts.join(', ')}.` : '';
}
