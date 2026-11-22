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
// no stars; ships split off as a fleet of their own go from the back (a fleet keeps its seasoned core, as navies
// did); a fleet joined to another brings its ships in at the back of the other's lists. Ships lost are taken by a
// fixed rotation (the design director, b487: newest-first losses would make a decorated ship all but unsinkable
// while its class had newer hulls): the fleet's earlier losses of the class plus the loss's order, modulo the ships
// there were (loseStars). A list that has fallen behind its count (a save from before the stars, or a loss written
// nowhere else) mends from the back: entries beyond the count are taken as the ships lost, and ships beyond the
// entries are ships with no stars. Every path mends a list that way, through mendStars, before it adds ships; a list
// is also mended as it is read.
//
// Nothing here draws on a random stream, adds, orders or renames a fleet, or changes a ship count, a strength or an
// outcome: the stars are a record and nothing more.

import { turnToDate } from '../core/ticket';
import type { Fleet, Game, ShipClass, Ships } from './types';
import { seaAction } from './sea-words';

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
 * Ships of one class just lost, the count already lowered by k and the fleet's tally of the class's losses already
 * raised by k: which of the class's ships they were is a fixed rotation (the design director, b487), the fleet's
 * earlier losses of the class plus the loss's order, modulo the ships still in the list. So a decorated ship goes
 * down in its turn like any other. No random draw: the stars stay a record and nothing more.
 */
export function loseStars(f: Fleet, c: ShipClass, k: number): void {
  if (k <= 0 || !f.stars?.[c]) return;
  const have = f.stars[c]!, there = Math.max(0, f.ships[c]) + k;
  // The ships there were before the loss, the list mended to them first (entries beyond them are earlier unseen losses).
  const list = Array.from({ length: there }, (_, i) => [...(have[i] ?? [])]);
  const from = Math.max(0, f.lost[c] - k);
  for (let j = 0; j < k && list.length; j++) list.splice((from + j) % list.length, 1);
  keep(f, c, list);
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
 * How a row of n stars is drawn (the art director, 13:27Z): every five make one silver, as the navies wore them, and
 * what is left over is bronze, silver first. 4 is four bronze, 5 one silver, 6 one silver and one bronze, 10 two
 * silver, 11 two silver and one bronze.
 */
export function starSplit(n: number): { silver: number; bronze: number } {
  const k = Math.max(0, Math.floor(n));
  return { silver: Math.floor(k / 5), bronze: k % 5 };
}

/**
 * An action as the stars name it, as the field names a fleet action: "The action" and where the sea is, by the UI
 * director's list ("The action in the North Sea", "The action off the Azores", "The action in Hudson Bay"); a second
 * in the same sea in the same month is "The second action off the Azores". And its month, "May 1940".
 */
export function seaActionName(g: Game, at: number): { name: string; month: string; turn: number; zone: number } {
  const actions = g.stories.seaActions ?? [];
  const [turn, zone] = actions[at] ?? [g.turn, -1];
  let nth = 1;
  for (let i = 0; i < at; i++) if (actions[i][0] === turn && actions[i][1] === zone) nth++;
  const sea = g.seas[zone]?.name;
  return { name: seaAction(sea ?? null, nth), month: turnToDate(turn).label, turn, zone };
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
  /** As the hover lists it: "The action off the Azores, May 1940", with "(2 of 3 ships)" where not every ship came through it. */
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
  // The UI director's words (13:29Z): a battle at sea is an action; a count the game fills in is a figure, and a count
  // of one is "a"; a number the rules fix is a word ("five").
  const head = `${of === 1 ? 'This ship has' : 'The most decorated of these ships has'} ${most === 1 ? 'a battle star, for an action' : `${most} battle stars, one for each action`} it came through on the winning side.`;
  return { most, silver, bronze, each, battles, words: [head, ...(silver ? ['A silver star stands for five bronze stars.'] : []), ...battles.map((b) => b.line)] };
}
