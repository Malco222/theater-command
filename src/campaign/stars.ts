// Battle stars (the design director's b477, Brian's own pick: "a ship that wins a sea battle adds a small battle star
// to its card").
//
// Every ship that comes through a fleet action its side won earns a star: on the dice, settled by the staff, or
// fought out on the field (navy.ts seaBattles books all three, after the ships sunk have gone down). The loser earns
// none; nor would a draw, though the rules never decide one. Every navy earns them. The record is kept ship by ship,
// as the first line of a ship's service record, but the rules count ships, not name them. So each fleet keeps one
// list a class, an entry for each of its ships, and one rule (below) says which entries go and come when the count
// changes. The war's actions are written once, in the order they were won (g.stories.seaActions), and a ship holds
// the places of its own in that list: numbers only, so that a save stays small and no rewriting of its words can
// touch them.
//
// The rule: a class's list runs from the ships longest in the fleet to the newest. New ships join at the back, with
// no stars; ships lost, or split off as a fleet of their own, go from the back; a fleet joined to another brings its
// ships in at the back of the other's lists. It is the only rule under which a list that has fallen behind its
// count mends itself the right way (a save from before the stars, or a loss written nowhere else): entries beyond
// the count are the ships lost, and ships beyond the entries are ships with no stars. Every path mends a list the
// same way, through mendStars, before it adds ships; a list is also mended as it is read.
//
// Nothing here draws on a random stream, adds, orders or renames a fleet, or changes a ship count, a strength or an
// outcome: the stars are a record and nothing more.

import { turnToDate } from '../core/ticket';
import type { Fleet, Game, ShipClass, Ships } from './types';

const CLASSES: ShipClass[] = ['bb', 'cv', 'ca', 'dd', 'ss'];

/** A fleet's battle stars, ship by ship: for each class, one entry a ship (longest in the fleet first), each the actions it won. */
export type ShipStars = Partial<Record<ShipClass, number[][]>>;

/**
 * One class of a fleet's ships, ship by ship, oldest in the fleet first: each the actions it came through on the
 * winning side, as places in g.stories.seaActions. Always as long as the count: a fresh copy, mended by the rule.
 */
export function shipStars(f: Fleet, c: ShipClass): number[][] {
  const have = f.stars?.[c] ?? [];
  return Array.from({ length: Math.max(0, f.ships[c]) }, (_, i) => [...(have[i] ?? [])]);
}

/** Writes a class's list in the form it is kept in: ships at the back with no stars left off, an empty list or record dropped. */
function keep(f: { stars?: ShipStars }, c: ShipClass, list: number[][]): void {
  let n = list.length;
  while (n > 0 && !list[n - 1].length) n--;
  if (n > 0) (f.stars ??= {})[c] = list.slice(0, n);
  else if (f.stars?.[c]) {
    delete f.stars[c];
    if (!Object.keys(f.stars).length) delete f.stars;
  }
}

/**
 * Brings a fleet's lists (one class, or all) back in step with its counts by the rule: the entries of ships no
 * longer there are dropped from the back. Called wherever ships are lost, and before any are added.
 */
export function mendStars(f: Fleet, only?: ShipClass): void {
  if (!f.stars) return;
  for (const c of only ? [only] : CLASSES) {
    const list = f.stars?.[c];
    if (list && list.length > Math.max(0, f.ships[c])) keep(f, c, list.slice(0, Math.max(0, f.ships[c])));
  }
}

/**
 * Ships about to be split off a fleet (fleets.ts detach), called before they leave: the last of each class go, and
 * their entries with them. Returns the new fleet's record, or undefined when none of the ships that go has a star.
 */
export function splitStars(f: Fleet, s: Ships): ShipStars | undefined {
  if (!f.stars) return undefined;
  const out: { stars?: ShipStars } = {};
  for (const c of CLASSES) {
    if (!f.stars?.[c]) continue;
    const all = shipStars(f, c), stay = Math.max(0, all.length - Math.max(0, s[c]));
    keep(f, c, all.slice(0, stay));
    keep(out, c, all.slice(stay));
  }
  return out.stars;
}

/** One fleet joined to another (fleets.ts merge), called before the ships move: a's ships come in at the back of b's lists. */
export function joinStars(a: Fleet, b: Fleet): void {
  if (!a.stars && !b.stars) return;
  for (const c of CLASSES) if (a.stars?.[c] || b.stars?.[c]) keep(b, c, [...shipStars(b, c), ...shipStars(a, c)]);
  delete a.stars;
}

/**
 * A fleet action decided, once its ships sunk have gone down: every ship still afloat in each of the winning fleets
 * earns it. 'winners' null, or empty, is an action nobody won (a draw): it is not written down and earns no star.
 * Returns the action's place in g.stories.seaActions, or -1.
 */
export function bookSeaAction(g: Game, zone: number, winners: Fleet[] | null): number {
  if (!winners?.length) return -1;
  const actions = (g.stories.seaActions ??= []);
  actions.push([g.turn, zone]);
  const at = actions.length - 1;
  for (const f of winners) {
    for (const c of CLASSES) {
      if (f.ships[c] <= 0) { mendStars(f, c); continue; }
      const list = shipStars(f, c);
      for (const ship of list) ship.push(at);
      keep(f, c, list);
    }
  }
  return at;
}

// ---- Reading them --------------------------------------------------------------------------------------------

/**
 * How a row of n stars is drawn (the art director, b477): up to five bronze in a row; at the sixth, five bronze become
 * one silver. Five bronze turn silver only when a sixth would appear, every time alike, so a row with any star ends in
 * one to five bronze: 5 is five bronze, 6 one silver and one bronze, 10 one silver and five bronze, 11 two and one.
 */
export function starSplit(n: number): { silver: number; bronze: number } {
  const k = Math.max(0, Math.floor(n));
  const silver = k > 0 ? Math.floor((k - 1) / 5) : 0;
  return { silver, bronze: k - 5 * silver };
}

const ORDINAL = ['', '', 'second', 'third', 'fourth', 'fifth', 'sixth', 'seventh', 'eighth', 'ninth', 'tenth'];

/**
 * An action as the stars name it, as the field names a fleet action ("The action in the North Sea"); a second in
 * the same sea in the same month is "The second action in the North Sea". And its month, "May 1940".
 */
export function seaActionName(g: Game, at: number): { name: string; month: string; turn: number; zone: number } {
  const actions = g.stories.seaActions ?? [];
  const [turn, zone] = actions[at] ?? [g.turn, -1];
  let nth = 1;
  for (let i = 0; i < at; i++) if (actions[i][0] === turn && actions[i][1] === zone) nth++;
  const sea = g.seas[zone]?.name;
  const which = nth <= 1 ? 'The action' : `The ${ORDINAL[nth] ?? `${nth}th`} action`;
  return { name: sea ? `${which} in the ${sea}` : `${which} at sea`, month: turnToDate(turn).label, turn, zone };
}

/** One action on a card's row of stars: its name and month, and how many of the card's ships came through it. */
export interface StarBattle {
  name: string;
  month: string;
  turn: number;
  zone: number;
  /** How many of the card's ships came through it, of how many the card has. */
  ships: number;
  of: number;
  /** As the hover lists it: "The action in the North Sea, May 1940", with "(2 of 3 ships)" where not every ship came through it. */
  line: string;
}

/**
 * The battle stars on one card of a fleet (a class of its ships), for both interfaces to draw alike: the row is the
 * most decorated ship's, as silver and bronze; each ship's count, oldest in the fleet first; the actions any of
 * them came through, oldest first; and the words the hover shows.
 */
export interface StarRow {
  most: number;
  silver: number;
  bronze: number;
  each: number[];
  battles: StarBattle[];
  /** The hover, a line each: what the row is, a silver star's worth where there is one, then each action. */
  words: string[];
}

/** The battle stars on a fleet's card for one class of its ships, or null where none of them has one. */
export function starRow(g: Game, f: Fleet, c: ShipClass): StarRow | null {
  const list = shipStars(f, c);
  const each = list.map((s) => s.length);
  const most = Math.max(0, ...each);
  if (most <= 0) return null;
  const count = new Map<number, number>();
  for (const s of list) for (const at of new Set(s)) count.set(at, (count.get(at) ?? 0) + 1);
  const of = list.length;
  const battles = [...count.keys()].sort((a, b) => a - b).map((at): StarBattle => {
    const { name, month, turn, zone } = seaActionName(g, at);
    const ships = count.get(at)!;
    return { name, month, turn, zone, ships, of, line: `${name}, ${month}${ships < of ? ` (${ships} of ${of} ships)` : ''}` };
  });
  const { silver, bronze } = starSplit(most);
  // Counts inside a sentence stand as figures (the UI director, b476 3), "1 battle star" as "7 battle stars".
  const head = `${of === 1 ? 'This ship has' : 'The most decorated of these ships has'} ${most} ${most === 1 ? 'battle star, for a sea battle' : 'battle stars, one for each sea battle'} it came through on the winning side.`;
  return { most, silver, bronze, each, battles, words: [head, ...(silver ? ['A silver star stands for 5 bronze stars.'] : []), ...battles.map((b) => b.line)] };
}
