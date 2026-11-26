// The night handed back to the campaign (ruling 217's seam; ruling 241 §2 and §4; the design director's rider of
// 24 November 12:34Z, mark 6: booking by quantile).
//
// The staff would settle the month by its dice: each fleet's boats and destroyers lost at the floor or the floor and
// one of their expectation (core/rng.ts roll), the tons by a swing drawn evenly between 0.8 and 1.2. A fought night
// lands inside the same runs, at the place its own counts earn against the computer's own nights for the same card:
// the field is fought out computer against computer on the card many times (each on its own seed, fixed by the
// ticket's, so the same ticket always weighs the same), and each of the night's counts is ranked among those nights
// (ties broken on a stream of their own). A night that cost a fleet more boats than (1 - frac) of the computer's own
// books the run's upper count; the merchants' rank sets the swing at low + (high - low) x rank. So the computer's own
// nights book exactly the dice's spread, and a player's night books what its counts earn inside the runs.

import { hashString, Rng } from '../../core/rng';
import { CONVOY_BATTLE, type ConvoyFought } from '../../campaign/navy';
import { ConvoySim } from './sim';
import type { ConvoyCounts, ConvoyRanks, ConvoyTally, ConvoyTicket } from './types';

/** The computer's own nights for a card: every count's value in each, and its mean at each tenth of the night. */
export interface StaffNights {
  n: number;
  /** By count ('ss:<fleet>', 'dd:<fleet>', 'mv'): the night's count in each of the computer's nights. */
  counts: Record<string, number[]>;
  /** By count: its mean over the nights at the end of each tenth of the night (index 0 the first tenth, 9 dawn). */
  profile: Record<string, number[]>;
  /** The same for the night's measures (sim.ts tally), to split a hand-over's dice into the after card's lines. */
  tallies: Record<string, number[]>;
}

/** The counts' keys a ticket has: each raider and escort fleet that brings anything to the night, and the merchants. */
export function countKeys(t: ConvoyTicket): string[] {
  return [...t.place.fleets.filter((f) => f.n > 0).map((f) => `${f.side === 'att' ? 'ss' : 'dd'}:${f.id}`), 'mv'];
}

const valueOf = (c: ConvoyCounts, key: string): number => key === 'mv' ? c.mv : key.startsWith('ss:') ? c.ss[+key.slice(3)] ?? 0 : c.dd[+key.slice(3)] ?? 0;

/** The night as the computer fights it on both sides, on one seed. */
export function staffNight(t: ConvoyTicket, seed: number): ConvoySim {
  const sim = new ConvoySim({ ...t, seed, att: { ...t.att, human: false }, def: { ...t.def, human: false }, place: { ...t.place, mine: undefined } });
  return sim;
}

/** Fights the card 'n' times computer against computer (the seeds from the ticket's): the distribution a night is ranked in. */
export function staffNights(t: ConvoyTicket, n = 40): StaffNights {
  const keys = countKeys(t);
  const counts: Record<string, number[]> = {}, sums: Record<string, number[]> = {}, tsums: Record<string, number[]> = {};
  for (const k of keys) { counts[k] = []; sums[k] = new Array(10).fill(0); }
  let tkeys: (keyof ConvoyTally)[] = [];
  for (let i = 0; i < n; i++) {
    const sim = staffNight(t, hashString(`convoystaff:${t.seed}:${i}`));
    if (!tkeys.length) { tkeys = Object.keys(sim.tally) as (keyof ConvoyTally)[]; for (const k of tkeys) tsums[k] = new Array(10).fill(0); }
    let tenth = 0;
    while (!sim.over) {
      sim.advance();
      while (tenth < 10 && (sim.worked >= ((tenth + 1) * sim.limit) / 10 || sim.over)) {
        for (const k of keys) sums[k][tenth] += valueOf(sim.counts, k);
        for (const k of tkeys) tsums[k][tenth] += sim.tally[k];
        tenth++;
      }
    }
    for (const k of keys) counts[k].push(valueOf(sim.counts, k));
  }
  const profile: Record<string, number[]> = {}, tallies: Record<string, number[]> = {};
  for (const k of keys) profile[k] = sums[k].map((s) => s / n);
  for (const k of tkeys) tallies[k] = tsums[k].map((s) => s / n);
  return { n, counts, profile, tallies };
}

/**
 * What the rest of the night would cost from this battle second, on the computer's own nights' means (for 'dice'), and
 * what the rest of its measures would come to on the same means (to split the dice into the after card's lines only).
 */
export function restFrom(staff: StaffNights, limit: number): (sec: number) => ConvoyCounts & { tally: Partial<ConvoyTally> } {
  return (sec) => {
    // (the staff's mean at this very second: read in a straight line between the ends of its tenth, the night opening at
    // nothing, so no part of the night is counted twice after a hand-over (the playtester's M7))
    const f = Math.min(10, Math.max(0, (10 * sec) / Math.max(1, limit)));
    const j = Math.min(9, Math.floor(f)), w = f - j;
    const at = (p: number[]): number => { const lo = j === 0 ? 0 : p[j - 1]; return lo + (p[j] - lo) * w; };
    const out: ConvoyCounts & { tally: Partial<ConvoyTally> } = { ss: {}, dd: {}, mv: 0, tally: {} };
    for (const [k, p] of Object.entries(staff.profile)) {
      const v = Math.max(0, p[9] - at(p));
      if (k === 'mv') out.mv = v; else if (k.startsWith('ss:')) out.ss[+k.slice(3)] = v; else out.dd[+k.slice(3)] = v;
    }
    for (const [k, p] of Object.entries(staff.tallies ?? {})) out.tally[k as keyof ConvoyTally] = Math.max(0, p[9] - at(p));
    return out;
  };
}

/** A count's rank among the computer's nights: the share below it, and a draw's share of those equal to it. */
export function rankIn(v: number, sample: number[], u: number): number {
  let below = 0, equal = 0;
  for (const x of sample) { if (x < v) below++; else if (x === v) equal++; }
  return (below + u * equal) / Math.max(1, sample.length);
}

/** The night's ranks: each count against the computer's own, ties broken on the ticket's own stream. */
export function ranksOf(t: ConvoyTicket, counts: ConvoyCounts, staff: StaffNights): ConvoyRanks {
  const r = new Rng(hashString(`convoyrank:${t.seed}`));
  const out: ConvoyRanks = { ss: {}, dd: {}, mv: 0 };
  for (const k of countKeys(t)) {
    const rank = rankIn(valueOf(counts, k), staff.counts[k] ?? [], r.next());
    if (k === 'mv') out.mv = rank; else if (k.startsWith('ss:')) out.ss[+k.slice(3)] = rank; else out.dd[+k.slice(3)] = rank;
  }
  return out;
}

/** The run's count a rank earns: the floor of the expectation, and one more where the rank is past (1 - frac). */
export const inRun = (expect: number, rank: number): number => { const lo = Math.floor(expect); return lo + (rank >= 1 - (expect - lo) ? 1 : 0); };

/**
 * The night as the rules take it (navy.ts ConvoyFought): each fleet's boats and destroyers lost at the count its rank
 * earns in its run, and the swing its merchants' rank earns. A fleet that brought nothing to the night is left out, and
 * rolls its dice.
 */
export function foughtOf(t: ConvoyTicket, ranks: ConvoyRanks): ConvoyFought {
  const { low, high } = CONVOY_BATTLE.swing;
  const out: ConvoyFought = { swing: Math.round((low + (high - low) * ranks.mv) * 1e6) / 1e6, ss: {}, dd: {} };
  for (const f of t.place.fleets) {
    if (f.n <= 0) continue;
    if (f.side === 'att' && ranks.ss[f.id] !== undefined) out.ss[f.id] = inRun(f.expect, ranks.ss[f.id]);
    if (f.side === 'def' && ranks.dd[f.id] !== undefined) out.dd[f.id] = inRun(f.expect, ranks.dd[f.id]);
  }
  return out;
}

/** How the night reads for the side that fought it: better than how many of the staff's own nights, by the escorts' measure (boats killed against ships lost). */
export function verdictOf(ranks: ConvoyRanks): { def: number } {
  const ss = Object.values(ranks.ss), dd = Object.values(ranks.dd);
  const kills = ss.length ? ss.reduce((s, x) => s + x, 0) / ss.length : 0.5;
  const lost = (ranks.mv * 2 + (dd.length ? dd.reduce((s, x) => s + x, 0) / dd.length : 0.5)) / 3;
  return { def: (kills + (1 - lost)) / 2 };
}

/** One of the night's totals against the staff's own nights: in how many the staff's came to less, the same, or more. */
export interface AgainstStaff { less: number; same: number; more: number }

/**
 * The night's totals against the staff's own nights on the card, as plain counts for the after card (words only; the
 * night books by its ranks): merchants sunk, boats lost and escort ships lost, each side's fleets summed. 'Your pack
 * sank more ships than the staff did in 21 of its 30 nights' is mv.less; 'and lost fewer boats than in 12' is ss.more.
 */
export function againstStaff(t: ConvoyTicket, counts: ConvoyCounts, staff: StaffNights): { n: number; mv: AgainstStaff; ss: AgainstStaff; dd: AgainstStaff } {
  const keys = countKeys(t);
  const sumOf = (pre: string, i: number): number => keys.filter((k) => k.startsWith(pre)).reduce((s, k) => s + (staff.counts[k]?.[i] ?? 0), 0);
  const mine = { mv: counts.mv, ss: Object.values(counts.ss).reduce((s, x) => s + x, 0), dd: Object.values(counts.dd).reduce((s, x) => s + x, 0) };
  const out = { n: staff.n, mv: { less: 0, same: 0, more: 0 }, ss: { less: 0, same: 0, more: 0 }, dd: { less: 0, same: 0, more: 0 } };
  for (let i = 0; i < staff.n; i++) {
    for (const [k, theirs] of [['mv', staff.counts.mv?.[i] ?? 0], ['ss', sumOf('ss:', i)], ['dd', sumOf('dd:', i)]] as const) {
      const v = mine[k], o = out[k];
      if (theirs < v) o.less++; else if (theirs > v) o.more++; else o.same++;
    }
  }
  return out;
}
