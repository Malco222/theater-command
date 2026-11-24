// The field spread's field side (release 2; rules-builder/release2/field-spread/form.md, steps 1 and 1b; the design
// director's rulings of 24 Nov 2026 19:47Z): the reference army, the staying arithmetic the level is read by, the mask's
// hold and the ledger's parts of m. Every number is in data/campaign/spread.json with its mark. Nothing reads any of this
// unless the ticket carries reckoning.spread. Imports nothing of the campaign's code, so the field's own bundle
// (land-rules.js) stays free of it.
import spreadData from '../../../data/campaign/spread.json';
import { LAND } from './rules';
import { startNerve } from './units';
import type { NervePart, Side } from '../core/types';

export interface SpreadData {
  level: number;
  tail: { W: number; z: number };
  hold: [number, number];
  /** The spread no cause explains, by which parts of m the army reads: none (staying), all five (ledger), flank and rear alone (flankRear). */
  residual: { staying: number; ledger: number; flankRear: number };
  ref: { org: number; mor: number; xp: number; general: number };
  move: { p: number; c: number };
  mask: { hold: number };
  steps: { level: boolean; ledger: boolean };
  m: Record<NervePart, boolean>;
}
export const SPREAD = spreadData as unknown as SpreadData;

/**
 * The share of its strength a block of this corps can lose from the front before it runs: its starting nerve over the
 * rout line, against what each share of strength lost takes off it (units.ts makeBlocks; rules.ts nerve; sim.ts suffer).
 */
export function stayingOf(org: number, mor: number | undefined, xp: number, general: number, s: Side): number {
  const N = LAND.nerve;
  return (startNerve(org, mor) - N.rout) / (N.perLoss[s] * Math.max(0.3, 1 - N.xp * xp - N.general * general));
}
/** The reference army's staying (data ref): 1.609. */
export const refStaying = (s: Side): number => stayingOf(SPREAD.ref.org, SPREAD.ref.mor, SPREAD.ref.xp, SPREAD.ref.general, s);
/** What a block reads for its own nerve under N: the reference army's starting nerve and steadiness (form s2c). */
export const REF_NERVE = {
  mor0: startNerve(SPREAD.ref.org, SPREAD.ref.mor),
  steady: 1 - LAND.nerve.xp * SPREAD.ref.xp - LAND.nerve.general * SPREAD.ref.general,
};
/** The ledger's K: an army hit only from the front quits at level x N x draw (form 13A; anchored on the reference army, G). */
export const ledgerK = (s: Side): number => SPREAD.level / refStaying(s);

/** A blow's nerve weight by part (sim.ts blow): where it landed (nm by aspect, rules.ts aspect), fire from afar (guns or teams), an ambush's first fire. */
export interface NerveParts { aspect: 'front' | 'flank' | 'rear'; nm: number; fire: number; ambush: number }
/**
 * Each part read at the army (on) or at the block (off), never both: army x block is the blow's whole weight. A blow on
 * the front has no aspect part (its nm is 1).
 */
export function nerveSplit(p: NerveParts, on: Record<NervePart, boolean>): { army: number; block: number } {
  const a = p.aspect !== 'front' && on[p.aspect];
  return {
    army: (a ? p.nm : 1) * (on.fire ? p.fire : 1) * (on.ambush ? p.ambush : 1),
    block: (a ? 1 : p.nm) * (on.fire ? 1 : p.fire) * (on.ambush ? 1 : p.ambush),
  };
}
