// Sniper teams on the battle card (release 2; behind `snipers`, off).
//
// Brian, 25 November 2026 16:07Z: "should we add snipers? how would that impact the battle?", then "yes" to a small
// specialist team: small in kills, large in nerve and tempo. On the field (src/battle/land/snipers.ts, withSnipers) each
// side's foot gives a share of itself, by its army's doctrine (sniperShare), to one to three sniper pairs. The design
// director ruled that the card carries what the pairs do by its causes and by no fitted figure (26 November 2026,
// 01:49Z and 02:32Z, after the field's trace placed the pin's cost after the lines meet, not before):
//
//   (a) After the lines meet, a side's pairs count at their worth of the foot they were taken from: what they strike
//       after contact, for their power, over what the rest of their side strikes for its own (G, both ends).
//   (b) The pin, where it lands: the share of a side's foot the other side's pairs reach and pin fights at
//       x (1 - pin.pace x bite) for its pinned part of the fight. The card has no clock (the known list, item iv), so
//       that part is a stand-in read from the field: LAND.pin.most seconds over the mean fight after the lines meet.
//       One rule both ways: it falls mostly on the attack because the attack is the side that walks.
//
// Every figure is the field's own rule (LAND.snipers, LAND.pin, sniperShare) or a reading of the field marked G below,
// pinned by tests/campaign-snipercard.test.ts. The pairs' own fire before the lines meet (0.7% of the defence's harm
// before contact, about 0.1% of the attack's line) is not priced here: the design director has not placed it.
//
// The field is laid at the power BEFORE this term (Reckoning.snipers, sim.ts): the pairs and their pins on the field are
// the term. Laid at the odds after it, the field would take it off a second time.
import { LAND, sniperShare } from '../battle/land/rules';
import type { Game } from './types';

/** True in a campaign whose battles are fought with sniper pairs (the Battle rules card's `snipers` row). Its own key. */
export const sniperCardOn = (g: Game): boolean => g.flags.snipers === 1;

/** Sets the switch: the 3D game as its Battle rules card's row stands, and tests. Off, the card is the card it was. */
export function setSniperCard(g: Game, on: boolean): void {
  if (on) g.flags.snipers = 1;
  else delete g.flags.snipers;
}

export const SNIPER_CARD = {
  /**
   * (a) A pair's worth of the foot it was taken from, after the lines meet (G: the field's tally of harm struck after
   * contact over power, a pair's over its side's other blocks'; 1,344 battles, seven grounds, eight odds, 26 Nov).
   */
  worth: { att: 0.07, def: 0.1 } as Record<'att' | 'def', number>,
  /**
   * (b) G stand-ins for the pin, by the side pinned (the field's trace of the same 1,344 battles, 26 Nov 02:48Z): the
   * share of that side's foot power the other's pairs pin at some time in a fight (att 0.735, def 0.178); the mean bite
   * on what they pin, read off each pinned block's pace as the field set it, 1 - pin.pace x b (att 0.060, def 0.066:
   * LAND.snipers.bite's own reckoning, pinned by a test on the fields as laid); and the mean seconds of a fight after
   * the lines meet (170.5). On the player's practice fields and towns (P, 384 battles) the same reads are att 0.816,
   * def 0.166; 0.061, 0.064; 190.8 s.
   */
  reached: { att: 0.74, def: 0.18 } as Record<'att' | 'def', number>,
  bite: { att: 0.06, def: 0.066 } as Record<'att' | 'def', number>,
  fight: 171,
};

/** One side as the field's ticket will share it out: each corps' kind, its weight in the side, and its nation. */
export type SniperCorps = [kind: string, weight: number, nat: string];

export interface SniperCardIn {
  att: SniperCorps[];
  def: SniperCorps[];
  /** The weight of a town's guard fighting beside the defence (a block of its own on the field: it gives no pairs). */
  guard?: number;
}

export interface SniperCard {
  /** What the attack's and the defence's strength are multiplied by: (a) and (b) together. */
  att: number;
  def: number;
  /** Each side's foot share of its power, and its pairs' share (the foot it gives them, by its doctrine). */
  foot: { att: number; def: number };
  pairs: { att: number; def: number };
  /** (b) What the other side's pins take off each side, as a share of it. */
  pinned: { att: number; def: number };
}

function sideOf(cs: SniperCorps[], guard = 0): { foot: number; pairs: number } {
  const S = LAND.snipers;
  let all = guard, foot = 0, pairs = 0;
  for (const [kind, w, nat] of cs) {
    if (!(w > 0)) continue;
    all += w;
    if (S.foot.includes(kind)) { foot += w; pairs += w * sniperShare(nat); }
  }
  return all > 0 ? { foot: foot / all, pairs: pairs / all } : { foot: 0, pairs: 0 };
}

/** What the sniper pairs do to a battle's odds, by the two causes ruled. Null where neither side has foot to give pairs. */
export function sniperCard(i: SniperCardIn): SniperCard | null {
  const A = sideOf(i.att), D = sideOf(i.def, i.guard ?? 0);
  if (A.pairs <= 0 && D.pairs <= 0) return null;
  const C = SNIPER_CARD, part = Math.min(1, LAND.pin.most / Math.max(1e-9, C.fight)), pace = LAND.snipers.pin.pace;
  // (b) A side's foot is pinned only by the other side's pairs.
  const pin = (who: 'att' | 'def', own: { foot: number }, theirs: { pairs: number }): number =>
    theirs.pairs > 0 ? own.foot * C.reached[who] * pace * C.bite[who] * part : 0;
  const pinned = { att: pin('att', A, D), def: pin('def', D, A) };
  // (a) After the lines meet, each side's pairs at their worth of the foot they came from.
  const worthA = 1 - A.pairs * (1 - C.worth.att), worthD = 1 - D.pairs * (1 - C.worth.def);
  return { att: worthA * (1 - pinned.att), def: worthD * (1 - pinned.def), foot: { att: A.foot, def: D.foot }, pairs: { att: A.pairs, def: D.pairs }, pinned };
}

/** The card's line for it (the UI director's words to follow): null where it moves neither side by a whole point in a hundred. */
export function sniperCardLine(f: SniperCard): { label: string; value: string; effect: string } | null {
  const a = Math.round((1 - f.att) * 100), d = Math.round((1 - f.def) * 100);
  if (!a && !d) return null;
  const fx = [a ? `attack down ${a}%` : '', d ? `defence down ${d}%` : ''].filter(Boolean).join(', ');
  return { label: 'Snipers', value: 'Sniper pairs pin men on the move and pick off their leaders', effect: `${fx[0].toUpperCase()}${fx.slice(1)}` };
}
