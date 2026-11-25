// The field spread (release 2; rules-builder/release2/field-spread/form.md, ruled by the design director 24 Nov 2026
// 19:47Z, field-spread/rulings/dd-1947Z.md: "Go ahead with step 1"): its own step inside Units decide, beside the honest
// win chance and never with it. Bench-only through the form's steps 1-4: no grand start offers it (stage.json 'spread'
// is read by nothing), and only a measuring harness sets the flag (setSpread) or lays the draw (spreadKeys). The draw is
// keyed on the field ticket's own seed and attempt (ticket.ts landTicket), never on g.rng, so laying it moves no later
// battle's luck. Outcomes read the corps' organisation, morale, experience and the general: never a nation or the date.
//
// Step 1 lays each side's break point W (the field's spentOf). Step 1b's keys (N, the move, blocks on the reference, the
// army nerve ledger with m by part) are laid only where the data's steps or a harness ask for them. The card reads none
// of this until step 5 (game.ts is untouched).
import { unitsDecide } from './units';
import { corpsKnown } from './intel';
import type { Game } from './types';
import { Rng, hashString } from '../core/rng';
import { SPREAD, refStaying, stayingOf } from '../battle/land/spread';
import type { FieldTicket, ForceTicket, NervePart, Side, SideTicket, SpreadReckoning } from '../battle/core/types';
import type { LandPlace } from '../battle/land/field';

// ---- The switch ----------------------------------------------------------------------------------------------------

/** True in a campaign set for the field spread: inside Units decide, and never beside the honest win chance (G: also where both flags were set by hand). */
export const spreadOn = (g: Game): boolean => unitsDecide(g) && g.flags.spread === 1 && g.flags.winChance !== 1;

/**
 * Sets the switch for a measuring harness (bench-only through steps 1-4: nothing in the game calls it). Refuses to set it
 * without Units decide, or beside the honest win chance (form s9: never on together). Never refuses to clear it. No
 * first-month check: the bench sets it on saved positions from the four wars (G).
 */
export function setSpread(g: Game, on: boolean): boolean {
  if (!on) { delete g.flags.spread; return true; }
  if (!unitsDecide(g) || g.flags.winChance === 1) return false;
  g.flags.spread = 1;
  return true;
}

// ---- The one draw (form s6) ----------------------------------------------------------------------------------------

/** The normal quantile (Acklam), as the bench drew it (fs/final/benchq.ts zOf), coefficients verbatim. */
export function zOf(p: number): number {
  const a = [-39.69683028665376, 220.9460984245205, -275.9285104469687, 138.357751867269, -30.66479806614716, 2.506628277459239];
  const b = [-54.47609879822406, 161.5858368580409, -155.6989798598866, 66.80131188771972, -13.28068155288572];
  const c = [-0.007784894002430293, -0.3223964580411365, -2.400758277161838, -2.549732539343734, 4.374664141464968, 2.938163982698783];
  const d = [0.007784695709041462, 0.3224671290700398, 2.445134137142996, 3.754408661907416];
  const pl = 0.02425;
  if (p < pl) { const q = Math.sqrt(-2 * Math.log(p)); return (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) / ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1); }
  if (p > 1 - pl) { const q = Math.sqrt(-2 * Math.log(1 - p)); return -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) / ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1); }
  const q = p - 0.5, r = q * q;
  return (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q / (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1);
}

/**
 * The one draw (form s6), on the ticket's own seed and attempt: the fortune share (the field's own, on the same key as
 * sim.ts draws it, so it is held here and not written: G) and each side's share for its break point. Its own hashes,
 * never this.rng or g.rng.
 */
export function spreadDraw(seed: number, attempt?: number): { uF: number; u: Record<Side, number> } {
  const tail = attempt ? `:${attempt}` : '';
  const at = (k: string): number => new Rng(hashString(k)).next();
  return { uF: at(`fortune:${seed}${tail}`), u: { att: at(`nerve:${seed}${tail}:att`), def: at(`nerve:${seed}${tail}:def`) } };
}
/** The spread at one level: ln(tail / level) / z = 0.8211 (data tail). */
export const sigmaOne = (): number => Math.log(SPREAD.tail.W / SPREAD.level) / SPREAD.tail.z;
/** A side's break point: lognormal about its level, held to data hold. */
export function breakPoint(u: number, level: number, sigma: number): number {
  const [lo, hi] = SPREAD.hold;
  return Math.min(hi, Math.max(lo, level * Math.exp(sigma * zOf(Math.min(1 - 1e-9, Math.max(1e-9, u))))));
}

// ---- Step 1b: the level by causes (form s2c) -----------------------------------------------------------------------

/**
 * A side's N: its corps' staying, weighted by their shares (joiners' too), over the reference army's. 'known' (the mask)
 * reads an unidentified corps as the reference. Reads each corps' own experience from the ticket (the field's ledger
 * reads each block's, its divisions' mean: G). Never reads the nation or the date.
 */
export function levelN(side: SideTicket, s: Side, known: (f: ForceTicket) => boolean = () => true): number {
  const fs = [...side.forces, ...(side.joiners ?? [])], gen = side.general?.level ?? 0, ref = refStaying(s);
  let w = 0, n = 0;
  for (const f of fs) { w += f.share; n += f.share * (known(f) ? stayingOf(f.org, f.mor, f.xp, gen, s) : ref); }
  return w > 0 ? n / w / ref : 1;
}

// ---- The keys a ticket carries -------------------------------------------------------------------------------------

/**
 * Which of step 1b's keys to lay (absent: the data's steps and m), the mask's shown levels, and a residual for a set of
 * m's parts at the army that has none derived in the data (residualOf).
 */
export interface SpreadParts { level?: boolean; ledger?: boolean; m?: Partial<Record<NervePart, boolean>>; shown?: Record<Side, number>; residual?: number }

const PARTS: NervePart[] = ['flank', 'rear', 'fire', 'ambush', 'air'];
/**
 * The residual's spread for a ticket that carries N, by the parts of m its army ledger reads (null: no ledger). Each
 * set-up at its own (DD 19:47Z: "(B) at its own residual 0.791: re-run, don't reuse 0.766"): no part at the army, the
 * level by staying alone, 0.791 (N without a ledger, or 13 (B)); flank and rear alone, 0.774 (13 (A) as ruled: fire,
 * ambush and air to 1); all five, 0.766. Any other set (a part alone, for the gate) has none derived, so the harness
 * gives it ('given', spread.py's rule on those parts) or this refuses: no arm draws at another set-up's residual.
 */
export function residualOf(m: Record<NervePart, boolean> | null, given?: number): number {
  if (given !== undefined) return given;
  const at = m ? PARTS.filter((p) => m[p]) : [];
  if (!at.length) return SPREAD.residual.staying;
  if (at.length === PARTS.length) return SPREAD.residual.ledger;
  if (at.length === 2 && m!.flank && m!.rear) return SPREAD.residual.flankRear;
  throw new Error(`field spread: no residual derived for m read at the army by ${at.join(', ')} alone; give parts.residual (spread.py's rule on those parts)`);
}

/**
 * The keys a ticket carries (the bench's hook, form s9 proof 3): the draw on the ticket's own seed and attempt, with
 * step 1b's parts where asked (data steps and m by default). A harness that re-seeds a ticket lays them afresh with this.
 * With the data as laid (step 1) it is exactly { W }.
 *
 * The spread its break points are drawn at: no N, one level, 0.8211 (sigmaOne); with N, the residual for the parts of m
 * the ledger reads at the army (residualOf: none 0.791, flank and rear 0.774, all five 0.766), or parts.residual.
 */
export function spreadKeys(t: FieldTicket<LandPlace>, parts: SpreadParts = {}): SpreadReckoning {
  const level = parts.level ?? SPREAD.steps.level, ledger = level && (parts.ledger ?? SPREAD.steps.ledger);
  const { u } = spreadDraw(t.seed, t.attempt);
  if (!level) {
    const s1 = sigmaOne();
    return { W: { att: breakPoint(u.att, SPREAD.level, s1), def: breakPoint(u.def, SPREAD.level, s1) }, ...(parts.shown ? { shown: parts.shown } : {}) };
  }
  const m = { ...SPREAD.m, ...(parts.m ?? {}) } as Record<NervePart, boolean>;
  const sigma = residualOf(ledger ? m : null, parts.residual);
  const N = { att: levelN(t.att, 'att'), def: levelN(t.def, 'def') };
  const W = { att: breakPoint(u.att, SPREAD.level * N.att, sigma), def: breakPoint(u.def, SPREAD.level * N.def, sigma) };
  return {
    W, N, move: Math.pow(N.def / N.att, SPREAD.move.p / SPREAD.move.c),
    ...(parts.shown ? { shown: parts.shown } : {}),
    // Each side's residual draw, held as W is, so its front-on point stays in data hold (fsx/brk/causes.ts arm()).
    ...(ledger ? { ledger: { draw: { att: W.att / (SPREAD.level * N.att), def: W.def / (SPREAD.level * N.def) }, m } } : {}),
  };
}

/**
 * What the campaign lays on a field ticket under the switch: the keys, and under N the level each side is shown at to the
 * other's player. At step 1 (data steps off) the mask reads the data level, so nothing is shown but { W }.
 */
export function spreadFor(g: Game, t: FieldTicket<LandPlace>): SpreadReckoning {
  if (!SPREAD.steps.level) return spreadKeys(t);
  // The level each side's bar is shown at to the other side (the mask; the identified corps as intel reads them): its
  // corps identified by the other side read at their own staying, the rest at the reference (G). Reads only.
  const seenBy = (s: Side) => (f: ForceTicket): boolean => {
    if (f.corps < 0) return true; // the town's guard is no secret (ticket.ts cardKnows)
    const c = g.corps.find((x) => x.id === f.corps);
    return !!c && corpsKnown(g, c, t[s === 'att' ? 'def' : 'att'].nat).level !== 'none';
  };
  const shown = { att: SPREAD.level * levelN(t.att, 'att', seenBy('att')), def: SPREAD.level * levelN(t.def, 'def', seenBy('def')) };
  return spreadKeys(t, { shown });
}
