// Whether an attack is offered on the field, or the staff settles it (the design director's safety net, b418 and b439).
//
// The field shares out the card's own power and must land within a tenth of the card's chance. Measured on the
// campaign's own attacks (356 of them, 20 fights each, battles/b418-numbers.md), a kind of place the plain order still
// lands more than a tenth off its card is not offered on the field: its card says the staff will settle it, as every
// battle was settled before. Today no kind of place is more than a tenth off, so none is listed; a kind goes on the
// list only from a measure, and comes off it the same way.

import type { FieldTicket } from '../core/types';
import type { LandPlace, Terrain } from './field';

/** The kinds the campaign's attacks are measured by: the defence's size, a river before it, its works, and its ground. */
export type PlaceKind = 'smallest' | 'middle' | 'largest' | 'river' | 'works' | Terrain;

/**
 * Ruling 226's gate: no fight-it-yourself button reaches the campaign screen until the play version ships and the
 * retest passes marks 1 to 6. The campaign's FIGHT IT YOURSELF is built (b439) and stays off its screen until the
 * design director opens the gate here; a campaign page opened with ?field shows it for the directors' review.
 */
export const ON_CAMPAIGN = false;

/** The kinds the field is still more than a tenth off on, with the measure that put each there. None today (b439). */
export const STAFF_SETTLES: { kind: PlaceKind; measured: string }[] = [];

/** The kinds a battle belongs to: one size, its ground, and a river or works where it has them. */
export function kindsOf(t: FieldTicket<LandPlace>): PlaceKind[] {
  const divs = t.def.forces.reduce((a, f) => a + f.divs, 0);
  const size: PlaceKind = divs <= 2 ? 'smallest' : divs >= 11 ? 'largest' : 'middle';
  return [size, t.place.terrain, ...(t.place.river ? ['river' as const] : []), ...(t.place.fort > 0 ? ['works' as const] : [])];
}

/**
 * Each kind in running text after "every battle" (the UI director, 18 November): a river or works first, then the
 * ground, then the defence's size, worded from the defence's side so it reads true from either seat.
 */
const SAID: Record<PlaceKind, string> = {
  river: 'across a river', works: 'against fortress works',
  plains: 'in open country', forest: 'in the forests', hills: 'in hill country', mountain: 'in the mountains',
  marsh: 'in the marshes', urban: 'in city streets', desert: 'in the open desert',
  smallest: 'where two divisions or fewer defend', middle: 'where three to ten divisions defend', largest: 'where eleven or more divisions defend',
};
const ORDER: PlaceKind[] = ['river', 'works', 'plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert', 'smallest', 'middle', 'largest'];

/**
 * Null where the field offers this battle; else the one sentence the attack card says beside FIGHT IT YOURSELF, off,
 * naming the kinds of this battle that are on the list. It states the game as it stands, and goes when the kind comes
 * off the list.
 */
export function staffOnly(t: FieldTicket<LandPlace>, list: { kind: PlaceKind }[] = STAFF_SETTLES): string | null {
  const off = kindsOf(t).filter((k) => list.some((s) => s.kind === k));
  if (!off.length) return null;
  const said = ORDER.filter((k) => off.includes(k)).map((k) => SAID[k]).join(' ');
  return `The staff settles every battle ${said}: the field cannot fight one at the card's odds.`;
}
