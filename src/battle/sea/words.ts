// The fleet action's words that more than one place puts in a sentence: a fleet by its name, and the ships each side
// took home badly hurt. The sim's dispatches, the hover and the after card all say them the same way.

import type { Side } from '../core/types';
import { RULES_CLASSES, SEA } from './numbers';
import { SEA_CLASSES, type SeaShips } from './types';

/**
 * A fleet's name in a sentence (the design and UI directors, b458 6 and b467): a name that ends in "Fleet" or "Navy"
 * takes "the", as English does ("the Home Fleet", "the 1st Fleet", "the Polish Navy"); any other stands bare
 * ("Force H"), as the navies wrote it. The campaign's headlines follow the same rule.
 */
export const theFleet = (name: string): string => (/\b(?:Fleet|Navy)$/.test(name.trim()) ? `the ${name}` : name);

/** "of the Home Fleet", "of Force H": a ship's fleet after its kind. */
export const ofFleet = (name: string): string => `of ${theFleet(name)}`;

/**
 * A count that opens a sentence (the UI director, b476 3): one to nine are words ("Two Italian ships came home for
 * repair"), from 10 the figure stands. Inside a sentence figures stand ("1 battleship and 1 cruiser").
 */
export const headCount = (n: number): string => (n >= 1 && n <= 9 ? ['One', 'Two', 'Three', 'Four', 'Five', 'Six', 'Seven', 'Eight', 'Nine'][n - 1] : String(n));

/** A list as English says it: "a, b and c". */
export const andList = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);

/** Ships by kind, a kind at 0 left out: ["1 battleship", "2 cruisers"]. */
export const kindsOf = (s: SeaShips): string[] =>
  SEA_CLASSES.filter((c) => s[c] > 0).map((c) => `${s[c]} ${s[c] === 1 ? RULES_CLASSES[c].one : RULES_CLASSES[c].many}`);

const count = (s: SeaShips): number => s.bb + s.cv + s.ca + s.dd;

/**
 * A ship is badly hurt at the end of an action if it is still afloat with SEA.side.hurt of its hull or less left (half),
 * or went out of the fight burning (b458, 3). In a piece of several ships, the wear has reached one ship at a time
 * (sim.ts wear): the piece's ships afloat less one are whole, and the last is hurt by what is left of its share. So a
 * piece holds at most one such ship afloat.
 */
export function hurtAfloat(str: number, ships0: number, afloat: number): number {
  if (afloat <= 0) return 0;
  return str * ships0 - (afloat - 1) <= SEA.side.hurt + 1e-9 ? 1 : 0;
}

/**
 * The after card's lines for the ships each side took home badly hurt, in the repair line's form, every ship named by
 * its kind (b458, 3): "Two of your destroyers came home for repair.", "Two Italian ships came home for repair: 1
 * battleship and 1 cruiser." Yours first, then theirs, as the loss line says them. None for a side with none.
 */
export function repairLines(hurt: Record<Side, SeaShips>, me: Side, theirAdj: string): string[] {
  const them: Side = me === 'att' ? 'def' : 'att';
  const one = (s: SeaShips): string | null => { const k = SEA_CLASSES.filter((c) => s[c] > 0); return k.length === 1 ? (s[k[0]] === 1 ? RULES_CLASSES[k[0]].one : RULES_CLASSES[k[0]].many) : null; };
  const out: string[] = [];
  const m = hurt[me], t = hurt[them], nm = count(m), nt = count(t);
  if (nm) out.push(one(m) ? `${headCount(nm)} of your ${RULES_CLASSES[SEA_CLASSES.find((c) => m[c] > 0)!].many} came home for repair.` : `${headCount(nm)} of your ships came home for repair: ${andList(kindsOf(m))}.`);
  if (nt) out.push(one(t) ? `${headCount(nt)} ${theirAdj} ${one(t)} came home for repair.` : `${headCount(nt)} ${theirAdj} ships came home for repair: ${andList(kindsOf(t))}.`);
  return out;
}
