/**
 * The telling: "your arrival waits because its corps is at the front" (release 2, the grand campaign; the UI
 * director's words of 21:49Z, 25 November 2026, pacific-japan/ui-words-row-waits-contact-UI-MARKS.md).
 *
 * Under the switch 'fromWait', at the dial's stops 3 to 5, a dated row of the player's that takes named corps whole
 * waits while one of them is at the front, or while the row that makes one of them waits (game.ts `expeditions`: the
 * far side's clause and the old chart's statement, one rule, and `rowHolds` for what holds a row as the board
 * stands). Every word the player reads of it is here, and every reader the screens and the bridge call; nothing here
 * knows which chart is up. Words only: nothing here changes the war. The rule acts as it did before the telling.
 * (One exception, section 6: `setFromWait`, the Options row's switch for the rule itself, in any month.)
 *
 * Its own flags (numbers only, saved with the game; none begins with "wait", so a tool may leave them out by prefix):
 *   rowTold:<i>   the month the wait began, told once in a headline: also the cards' "has waited since {month}"
 *   rowAt:<i>     the province the row is due in (where it forms, after `rowPlace`), for the cards' readers
 *   rowFront:<i>  which of the row's named corps were at the front in any month it waited, a bit mask over the order of
 *                 its `from`, kept across the waiting months; none where it only ever waited for another row. The
 *                 formed headline names them.
 *   rowId:<i>:<k> the id of the corps at the front that bit k of rowFront names, so that a corps renamed while the row
 *                 waits is known under its new name and never called lost (the sceptic's T2-3).
 *   rowPlace:<i>  the province the row was held at because it is not in friendly hands (game.ts `expeditions`, the place
 *                 line; no `wait:<i>` is written for it), told once in a headline: the report card's line and the formed
 *                 headline's "now in friendly hands" read it. The UI director's section 9, 12:48Z, 26 November 2026.
 *   rowPlaceSince:<i> the month that hold began, written with `rowPlace` in the month its headline is said: the report
 *                 card's "due in {place} since {month}" reads it, so that it agrees with the month the headline said "is
 *                 due in {place} this month" (the UI director's section 10, Q3, 00:39Z, 27 November 2026). `rowTold`
 *                 keeps the month the row was first told, for every other reader (a front wait's "has waited since",
 *                 the memorial wall's "since the row was told"). A save made before it has none: the card then reads
 *                 `rowTold`, as it did.
 * All of them are cleared wherever the row is sent or struck (`rowDone`), and where its nation leaves the war for good
 * (`nationDone`), and only there (the switch turned off keeps them until then). With the switch off none is ever
 * written and nothing prints.
 */
import setup from '../../data/campaign/setup-1939.json';
import { turnToDate } from '../core/ticket';
import { allied, atFrontNow, headline, rowHolds, theWords } from './game';
import { contactRuleActs, stopOf } from './history';
import type { Corps, Game, Region } from './types';

const ROWS = (): any[][] => (setup as any).expeditions as any[][] ?? [];
const toldKey = (i: number): string => `rowTold:${i}`;
const atKey = (i: number): string => `rowAt:${i}`;
const frontKey = (i: number): string => `rowFront:${i}`;
const idKey = (i: number, k: number): string => `rowId:${i}:${k}`;
const placeKey = (i: number): string => `rowPlace:${i}`;
const placeSinceKey = (i: number): string => `rowPlaceSince:${i}`;

/**
 * The article a name takes in the telling's own sentences: none where it opens with "the", is an "X Force" or is
 * Creforce (the general's line's rule, game.ts `generalsThe`: the UI director's section 0), or where its head, before
 * any bracket, ends in "Force" (her section 7, case 5, 00:44Z, 26 November 2026: "a name whose head (before any bracket)
 * ends in "Force" takes none, as "W Force" and "Creforce" do: "Special Force is due in ...""), else "the ". The telling's
 * sentences only, which exist only with the switch on: game.ts's general's line keeps the base's rule (`generalsThe`),
 * so that with the switch off every sentence is the base's, byte for byte.
 */
export const theOf = (name: string): string => (/^(the |[A-Z] Force|Creforce)/i.test(name) || /Force$/.test(name.split('(')[0].trim()) ? '' : 'the ');
/** A name with its article: "The" at the head of a sentence, "the" elsewhere. (None after "your": write the bare name.) */
export const named = (name: string, head = false): string => {
  const a = theOf(name);
  return a ? `${head ? 'The' : 'the'} ${name}` : name;
};
const NUMBER = ['no', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine', 'ten', 'eleven', 'twelve'];
const numberWord = (n: number): string => NUMBER[n] ?? String(n);
/** Names joined whole: "A", "A and B", "A, B and C". */
const join = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
const placeOf = (g: Game, c: Corps): string => g.regions[c.reg].name;
const fromOf = (row: any[]): string[] => (Array.isArray(row?.[6]?.from) ? (row[6].from as string[]) : []);
/** The named corps of the row standing on the board, in the order the row names them. */
const present = (g: Game, nat: string, from: string[]): Corps[] => g.corps.filter((x) => x.nat === nat && !x.dead && from.includes(x.name));
const inOrder = (from: string[], cs: Corps[]): Corps[] => [...cs].sort((a, b) => from.indexOf(a.name) - from.indexOf(b.name));
const holdsNow = (g: Game, nat: string, from: string[], at: number | undefined, i: number): { front: Corps[]; unformed: string[] } => {
  const h = rowHolds(g, nat, from, at, i);
  return { front: inOrder(from, h.front), unformed: h.unformed };
};

// ---- 1. The month the wait begins: one headline, once ------------------------------------------------------------

/** The verb for one name or several: "{C}, which has not formed yet" / "{C} and {D}, which have not formed yet". */
const hasOrHave = (xs: unknown[]): string => (xs.length === 1 ? 'has' : 'have');

/**
 * Two or more corps at the front, each with its place, the place said once (the UI director's section 8 point 5, 02:07Z,
 * 26 November 2026: "{The 8th Tank Regiment} and {the 13th Tank Regiment} in {the Grenzmark} are at the front, ...
 * Places mixed: grouped by place, the groups set off by commas: "{A} and {B} in {X}, and {C} in {Y}, are at the
 * front""). Every place different, section 1's own form: "{A} in {X} and {B} in {Y} are at the front". The groups in
 * the order the first of each is named.
 */
function areAtFront(g: Game, front: Corps[]): string {
  const groups: { place: string; cs: Corps[] }[] = [];
  for (const c of front) {
    const place = placeOf(g, c);
    const at = groups.find((x) => x.place === place);
    if (at) at.cs.push(c); else groups.push({ place, cs: [c] });
  }
  let k = 0;
  const said = groups.map((x) => `${join(x.cs.map((c) => named(c.name, k++ === 0)))} in ${x.place}`);
  if (groups.length === 1 || groups.every((x) => x.cs.length === 1)) return `${join(said)} are at the front`;
  return `${said.slice(0, -1).join(', ')}, and ${said[said.length - 1]}, are at the front`;
}

/**
 * The headline the month a row's wait begins (section 1, and section 7 case 2 for both causes with more than one of
 * either), or null where there is nothing to name.
 */
export function waitBeginsLine(g: Game, nat: string, rowName: string, place: string, from: string[], front: Corps[], unformed: string[]): string | null {
  const head = `${named(rowName, true)} is due in ${place} this month`;
  const n = present(g, nat, from).length;
  if (front.length && unformed.length) {
    // (Section 7, case 2: "{A} in {X} and {B} in {Y} are at the front, and {C}, which it is also formed from, has not
    // formed yet, so the arrival waits for all of them." "both" where there are two in all; "{C} and {D} ... have not
    // formed yet".)
    const at = front.length === 1
      ? `${named(front[0].name, true)} is at the front in ${placeOf(g, front[0])}`
      : areAtFront(g, front);
    const all = front.length + unformed.length === 2 ? 'both' : 'all of them';
    return `${head}. ${at}, and ${join(unformed.map((x) => named(x)))}, which it is also formed from, ${hasOrHave(unformed)} not formed yet, so the arrival waits for ${all}.`;
  }
  if (unformed.length) {
    return unformed.length === 1
      ? `${head}, formed from ${named(unformed[0])}, which has not formed yet. The arrival waits for it.`
      : `${head}, formed from ${join(unformed.map((x) => named(x)))}, which have not formed yet. The arrival waits for them.`;
  }
  if (!front.length) return null;
  if (n === 1) return `${head}, formed from your ${front[0].name}, which is at the front in ${placeOf(g, front[0])}. The arrival waits, and forms the month after that corps leaves the front.`;
  const several = `${head}, formed from ${numberWord(n)} of your corps.`;
  if (front.length === 1) return `${several} ${named(front[0].name, true)} is at the front in ${placeOf(g, front[0])}, so the arrival waits, and forms the month after that corps leaves the front.`;
  return `${several} ${areAtFront(g, front)}, so the arrival waits, and forms the month after ${front.length === 2 ? 'both' : 'all of them'} leave the front.`;
}

/**
 * game.ts `expeditions`: a row has just been held this month by one of the two statements (or the chain), in the
 * telling's scope as the caller reads it (the switch on, stops 3 to 5, his rows not offers). For the player's own row
 * alone: the headline once, the month the wait begins, and the flags the cards and the formed headline read.
 */
export function tellWait(g: Game, i: number, at: Region | undefined): void {
  const row = ROWS()[i];
  if (!row || g.flags.fromWait !== 1 || !g.human || row[1] !== g.human || !at) return;
  const nat = row[1] as string, from = fromOf(row);
  const { front, unformed } = holdsNow(g, nat, from, at.id, i);
  // (Kept across the waiting months, not replaced: a row that waited on the front and then only for another row still
  // gets section 4's headline. The UI director's section 7, case 6: "It closes the wait he was told of, and it is true:
  // the corps did leave the front.")
  let mask = g.flags[frontKey(i)] ?? 0;
  for (const c of front) { const k = from.indexOf(c.name); mask |= 1 << k; g.flags[idKey(i, k)] = c.id; }
  if (mask) g.flags[frontKey(i)] = mask; else delete g.flags[frontKey(i)];
  g.flags[atKey(i)] = at.id;
  if (g.flags[toldKey(i)] !== undefined) return;
  g.flags[toldKey(i)] = g.turn;
  const line = waitBeginsLine(g, nat, row[2], at.name, from, front, unformed);
  if (line) headline(g, 'info', line, nat, at.id);
}

/**
 * The headline the month a row's hold for its place begins (the UI director's section 9, 12:48Z, 26 November 2026, in
 * the game's own words for a place, "is not in friendly hands"; the holder named, the place said once): "{The 12th
 * Army (62nd Division)} is due in {Kaifeng} this month, but {China} holds it, so the arrival waits until it is in
 * friendly hands." Null where nobody holds it to be named.
 */
export function placeHeldLine(g: Game, rowName: string, at: Region): string | null {
  const holder = at.ctrl ? g.nations[at.ctrl] : undefined;
  if (!holder) return null;
  return `${named(rowName, true)} is due in ${at.name} this month, but ${holder.name} holds it, so the arrival waits until it is in friendly hands.`;
}

/**
 * game.ts `expeditions`, the place line: a row that takes named corps whole has passed every other hold this month and
 * is held because its place is not in friendly hands (the design director's 06:10Z, 26 November 2026: "Tell the
 * maker's hold for its place like any wait"; the telling reads its own flags, so no `wait:<i>` is written). In the
 * telling's scope as the caller reads it, for the player's own row alone: the headline once, in the month the hold for
 * that place begins, and the flags the report card and the formed headline read. A row first told of another cause
 * keeps that month as the month it has waited since. A lost place together with another cause in one month never
 * reaches here: the other cause holds the row first and is told as today (her section 9: not worded). The hold's own
 * month (`rowPlaceSince`) is written with the place, in the month its headline is said: the card reads it (her section
 * 10, Q3). A save made before it, its hold already told, keeps none and writes none: the place is the same, nothing is
 * said again, and the card reads `rowTold`, as it did.
 */
export function tellPlace(g: Game, i: number, at: Region): void {
  const row = ROWS()[i];
  if (!row || g.flags.fromWait !== 1 || !g.human || row[1] !== g.human) return;
  g.flags[atKey(i)] = at.id;
  if (g.flags[toldKey(i)] === undefined) g.flags[toldKey(i)] = g.turn;
  if (g.flags[placeKey(i)] === at.id) return;
  g.flags[placeKey(i)] = at.id;
  g.flags[placeSinceKey(i)] = g.turn;
  const line = placeHeldLine(g, row[2], at);
  if (line) headline(g, 'info', line, row[1], at.id);
}

// ---- 4 and 5. The month it forms -----------------------------------------------------------------------------------

/**
 * The row is sent or struck (game.ts sets `sent:<i>`): the telling's three flags go with it, so no dead data stays in
 * the save. The design director's answer 3 of 00:43Z, 26 November 2026: "Clear the telling's three flags where the row
 * is sent or struck (no dead data)." Called at every place game.ts sets `sent:<i>` for a row, after the formed headline
 * has read them (`formedLine` calls it itself); a row that never waited has none, and nothing changes.
 */
export function rowDone(g: Game, i: number): void {
  delete g.flags[toldKey(i)]; delete g.flags[atKey(i)]; delete g.flags[frontKey(i)]; delete g.flags[placeKey(i)]; delete g.flags[placeSinceKey(i)];
  fromOf(ROWS()[i]).forEach((_, k) => { delete g.flags[idKey(i, k)]; });
}

/**
 * A nation leaves the war for good (game.ts `capitulate`; domination.ts `leave` and `layDown`): the four telling flags of
 * every row of that nation that carries any of them go, so no dead data stays in a lost war. The design director, 05:00Z,
 * 26 November 2026, on the sceptic's truth3-4: "Clear the four telling flags in capitulate and in domination's layDown and
 * leave, so no dead data stays in a lost war." Only those flags: the rule's own `wait:<i>` stays, and the row is not
 * struck (no `sent:<i>`). Rows of other nations are not touched. A row that carries none is left as it is, so a war that
 * never had the switch on (where no telling flag exists) is not changed by a byte.
 */
export function nationDone(g: Game, nat: string): void {
  ROWS().forEach((row, i) => {
    if (row?.[1] !== nat) return;
    const told = [toldKey(i), atKey(i), frontKey(i), placeKey(i), placeSinceKey(i), ...fromOf(row).map((_, k) => idKey(i, k))].some((k) => g.flags[k] !== undefined);
    if (told) rowDone(g, i);
  });
}

/**
 * What a row that waited on the front is formed from, read before its corps are taken (game.ts `expeditions`): how many
 * of its named corps stand, the one where it is one; and of the names that were at the front in any of its waiting
 * months (`rowFront`), those that stand and have left the front (`left`), those at the front in the very province the
 * row forms in (`stands`), and those no longer on the board and written on the memorial wall since the row was told
 * (`lost`).
 */
export interface WaitedOn { count: number; one: string | null; left: string[]; stands: string[]; lost: string[] }

/**
 * True where the memorial wall (stories.ts `storyFallen`, `g.stories.fallen`) holds a corps of `nat` named `name` that
 * fell in or after the month `since` (the month the row was told: `rowTold`). An older entry of the same name is another
 * corps, or the same one before the telling: it says nothing of this wait.
 */
const onWall = (g: Game, nat: string, name: string, since: number | undefined): boolean =>
  since !== undefined && (g.stories?.fallen ?? []).some((f) => f.nat === nat && f.name === name && f.turn >= since);

/**
 * Read in `expeditions` once a row has passed both statements, before it takes its corps: null unless it waited on the
 * front in any of its waiting months (`rowFront`), or where a corps of those is still at the front away from `at`, the
 * province the row forms in, or where a corps of those is gone from the board and not on the memorial wall since the row
 * was told (nothing true to say of it: today's line stands).
 */
export function waitedOn(g: Game, i: number, at: number | undefined): WaitedOn | null {
  const mask = g.flags[frontKey(i)];
  if (!mask) return null;
  const row = ROWS()[i];
  const from = fromOf(row);
  const here = present(g, row[1], from);
  const out: WaitedOn = { count: here.length, one: here.length === 1 ? here[0].name : null, left: [], stands: [], lost: [] };
  for (const [k, name] of from.entries()) {
    if ((mask & (1 << k)) === 0) continue;
    const x = here.find((c) => c.name === name);
    // (A corps not standing under its name is lost only where no living corps has its id: one renamed while the row
    // waits, by a row that joins it or otherwise, stands under its new name, and nothing true is said of it here; today's
    // line stands. The sceptic's T2-3.)
    const id = g.flags[idKey(i, k)];
    if (!x && id !== undefined && g.corps.some((c) => c.id === id && !c.dead)) return null;
    // (A corps standing under the name with another id is not the one he was told of: one raised again from the memorial
    // wall under its old name was never at the front. Nothing true is said of it here; today's line stands.)
    if (x && id !== undefined && x.id !== id) return null;
    // (Gone from the board, it is "lost" only where the memorial wall says so: a corps of the row's nation and that name
    // written there (stories.ts `storyFallen`: destroyed, surrendered or starved) in or after the month the row was told
    // (`rowTold`). Gone for any other cause, taken by another row, folded, broken up, lent, drawn on, stood down, struck
    // with its nation, it was not lost, and nothing true is said of it here: today's line stands. A corps the wall leaves
    // out, a fortress crew, gets today's line too: the safe side. Round 2's T2-1 and T2-2.)
    if (!x && !onWall(g, row[1], name, g.flags[toldKey(i)])) return null;
    // (The rule's own test for the front, the forming place not set apart: game.ts `atFrontNow`.)
    if (!x) out.lost.push(name);
    else if (!atFrontNow(g, row[1], x, from)) out.left.push(name);
    else if (x.reg === at) out.stands.push(name);
    else return null;
  }
  return out;
}

/**
 * The words the month a row forms (sections 4, 5 and 8), for game.ts's arrival headline: `alone` takes the place of
 * today's bare line ("{row} has landed at {place}."), `after` is said after a row's own news or its general's line.
 * Null where the telling has nothing to say and today's line stands. Clears the row's telling flags: it has formed.
 * `own` is true where the row came with men of its own while its corps were at the front, in the telling's scope.
 * `acts` is true where the rule acts for the row this month (game.ts `inFight`): then a row held for its place
 * (`rowPlace`) that forms there takes the place's clause after the place, the UI director's section 9, 12:48Z, 26
 * November 2026: "{The 12th Army (62nd Division)} has arrived in {Kaifeng}, now in friendly hands." with the "formed
 * from ..." clause following as now. A row that comes without its corps at the front (`own`) is a lost place together
 * with another cause, not worded: its line is today's.
 */
export function formedLine(g: Game, i: number, was: WaitedOn | null, own: boolean, c: Corps, reg: Region, bySea: boolean, acts = false): { alone: string | null; after: string | null } | null {
  const row = ROWS()[i];
  const held = acts && g.flags[placeKey(i)] === reg.id;
  rowDone(g, i);
  if (!row || !g.human || row[1] !== g.human) return null;
  const verb = bySea ? 'landed at' : 'arrived in';
  const head = `${named(c.name, true)} has ${verb} ${reg.name}${held && !own ? ', now in friendly hands' : ''}`;
  if (own) {
    const from = fromOf(row);
    const { front } = holdsNow(g, row[1], from, reg.id, i);
    const here = present(g, row[1], from);
    // (Every corps it leaves, and which of them is at the front: the words cover all of them at the front.)
    if (!front.length || front.length !== here.length) return null;
    const alone = front.length === 1
      ? `${head} without your ${front[0].name}, which is at the front in ${placeOf(g, front[0])} and stays a corps of its own.`
      // (Two corps in one province name the place once: the UI director's section 7, case 7.)
      : `${head} without ${join(front.map((x) => `your ${x.name}`))}, which are at the front in ${join([...new Set(front.map((x) => placeOf(g, x)))])} and stay corps of their own.`;
    return { alone, after: null };
  }
  const tail = was ? formedTail(was, c.name) : null;
  if (!tail) return held ? { alone: `${head}.`, after: null } : null;
  return { alone: `${head}, ${tail}`, after: `The arrival is ${tail}` };
}

/**
 * The words the month a told row joins a corps already landed (`joins`, game.ts `expeditions`): `alone` where the row
 * has no news of its own (today nothing is said then), `after` said after its news (section 4). The UI director's
 * section 8 point 3, 02:10Z, 26 November 2026: "{Row} has joined {the Old Corps} in {place}, formed from your {Test
 * Corps}, now that it has left the front." (with points 1's and 2's endings where those are the true case). `joined` is
 * the name of the corps it joins, read before the row renames it; `place` where that corps stands. Clears the row's
 * telling flags. Null where the telling has nothing to say, and today's words stand.
 */
export function joinedLine(g: Game, i: number, was: WaitedOn | null, joined: string, place: string): { alone: string | null; after: string | null } | null {
  const row = ROWS()[i];
  rowDone(g, i);
  if (!row || !g.human || row[1] !== g.human || !was) return null;
  const tail = formedTail(was, row[2]);
  return tail ? { alone: `${named(row[2], true)} has joined ${named(joined)} in ${place}, ${tail}`, after: `The arrival is ${tail}` } : null;
}

/**
 * The words the month a told row is closed because every division it names already stands (game.ts `expeditions`,
 * drawn.ts `formsAny`; today it is closed with no headline). The UI director's section 8 point 3, 02:10Z, 26 November
 * 2026: "{The 18th Division}, due in {Rangoon}, will not form: every division of it already stands with your forces."
 * The card and report lines stop that month: the caller clears the row's flags (`rowDone`) after reading this. Null for
 * a row he was never told of, or not his.
 */
export function notFormedLine(g: Game, i: number, place: string): string | null {
  const row = ROWS()[i];
  if (!row || !g.human || row[1] !== g.human || g.flags[toldKey(i)] === undefined) return null;
  return `${named(row[2], true)}, due in ${place}, will not form: every division of it already stands with your forces.`;
}

/**
 * The words after "{Row} has arrived in {place}, " and after "The arrival is ": how the row is formed, true of the month
 * it forms (sections 4 and 8), or null where nothing true is to be said and today's line stands. `name` is the formed
 * row's own name, for "your corps of that name".
 */
function formedTail(was: WaitedOn, name: string): string | null {
  const yours = (x: string): string => (x === name ? 'corps of that name' : x);
  const fromYour = was.count === 1 && was.one ? `from your ${yours(was.one)}` : `from ${numberWord(was.count)} of your corps`;
  if (was.lost.length) {
    // (A corps at the front lost while the row waits: the UI director's section 8 point 1, 02:10Z, 26 November 2026.
    // "formed without your {21st Army (18th Division)}, which has been lost"; same name, "formed without your corps of
    // that name, which has been lost"; several, "formed from {three} of your corps, without {the 13th Tank Regiment},
    // which has been lost".)
    const been = `which ${was.lost.length === 1 ? 'has' : 'have'} been lost`;
    return !was.count
      ? `formed without ${join(was.lost.map((x) => `your ${yours(x)}`))}, ${been}.`
      : `formed ${fromYour}, without ${join(was.lost.map((x) => named(x)))}, ${been}.`;
  }
  if (!was.count || !(was.left.length + was.stands.length)) return null;
  // (A corps at the front in the province the row forms in: the UI director's section 8 point 2, 02:10Z, 26 November
  // 2026. "formed from your {21st Army (18th Division)}, now that it stands there"; several, "now that {the 13th Tank
  // Regiment} stands there"; mixed, "now that {A} has left the front and {B} stands there".)
  if (was.count === 1 && was.one) return `formed ${fromYour}, now that it ${was.stands.length ? 'stands there' : 'has left the front'}.`;
  const left = was.left.length ? `${join(was.left.map((x) => named(x)))} ${was.left.length === 1 ? 'has' : 'have'} left the front` : '';
  const stands = was.stands.length ? `${join(was.stands.map((x) => named(x)))} ${was.stands.length === 1 ? 'stands' : 'stand'} there` : '';
  return `formed ${fromYour}, now that ${[left, stands].filter(Boolean).join(' and ')}.`;
}

// ---- 2. While it waits: the cards' readers -------------------------------------------------------------------------

/** The rows of the player's that wait in the telling (told, not sent), each with where it is due and its named corps. */
function waiting(g: Game, nat: string): { i: number; row: any[]; at: number; since: number; from: string[] }[] {
  if (g.flags.fromWait !== 1 || nat !== g.human) return [];
  const out: { i: number; row: any[]; at: number; since: number; from: string[] }[] = [];
  ROWS().forEach((row, i) => {
    const since = g.flags[toldKey(i)];
    if (since === undefined || row[1] !== nat || g.flags[`sent:${i}`]) return;
    // (A row the rule no longer holds, the dial turned to a book stop whose book is open where it is due, forms next
    // month wherever its corps stand: neither card speaks for it. history.ts `contactRuleActs`, as game.ts reads it.)
    if (!contactRuleActs(g, g.byName[row[5]])) return;
    // (Nor a row whose `stops` leave out the dial's stop: game.ts returns at its stops check before it can wait or form,
    // so while the dial stands there it never forms, and "it forms the month after this corps leaves the front" would be
    // false. Its three flags are kept, not cleared: the dial can come back to a stop it acts at, and then the card
    // returns with the wait's first month and the row is not told a second time. The refuter's interplay-3.)
    if (Array.isArray(row[6]?.stops) && !(row[6].stops as number[]).includes(stopOf(g))) return;
    const at = g.flags[atKey(i)];
    if (at === undefined || !g.regions[at]) return;
    out.push({ i, row, at, since, from: fromOf(row) });
  });
  return out;
}

/**
 * The standing line for a corps of the player's at the front that a waiting row is to be formed from (section 2, its
 * month as section 7 case 8 gives it: "has waited for it since {month}"; and section 7 case 2 for a row that also
 * waits for another row to form): `short` for the tray card ("An arrival waits for it"), `whole` for its hover, the
 * record card and the 3D corps card's muted note. Read from the board as it stands: the line goes the moment the corps
 * leaves the front. Null otherwise.
 */
export function arrivalWaitOf(g: Game, c: Corps): { short: string; whole: string } | null {
  if (c.dead || g.flags.fromWait !== 1 || c.nat !== g.human) return null;
  for (const w of waiting(g, c.nat)) {
    if (!w.from.includes(c.name)) continue;
    const { front, unformed } = holdsNow(g, c.nat, w.from, w.at, w.i);
    if (!front.includes(c)) continue;
    const others = front.filter((x) => x !== c).map((x) => named(x.name));
    const since = turnToDate(w.since).label;
    const due = `An arrival due in ${g.regions[w.at].name}, ${named(w.row[2])}, is to be formed from this corps`;
    const who = others.length ? `${join(['this corps', ...others])} leave` : 'this corps leaves';
    const rest = join(unformed.map((x) => named(x)));
    const whole = !unformed.length
      ? `${due}, and has waited for it since ${since}. It forms the month after ${who} the front.`
      : `${due} and from ${rest}, which ${hasOrHave(unformed)} not formed yet, and has waited for it since ${since}. It forms once ${who} the front and ${rest} ${hasOrHave(unformed)} formed.`;
    return { short: 'An arrival waits for it', whole: theWords(whole) };
  }
  return null;
}

/**
 * The month's report card (section 2): for each waiting row of the player's that waits only for another row to form,
 * or only for its place to be in friendly hands (section 9), and so has no corps to carry it, one muted line, every
 * month it waits.
 */
export function rowsWaitingOnRows(g: Game, nat: string | null): string[] {
  if (!nat) return [];
  const out: string[] = [];
  for (const w of waiting(g, nat)) {
    const { front, unformed } = holdsNow(g, nat, w.from, w.at, w.i);
    if (front.length) continue;
    if (!unformed.length) {
      // (A row held for its place has no corps to carry it either: the UI director's section 9, 12:48Z, 26 November
      // 2026, "the month's report card carries it, muted, as a row that waits for another row does". Read from the
      // board as it stands: the line goes once the place is in friendly hands. With a corps at the front or a row not
      // formed as well, that cause is told as today and this line is not: a lost place with another cause is not worded.)
      // (Its month is the month the place hold began, the month its headline was said: her section 10, Q3. A save
      // made before `rowPlaceSince` reads the row's month, as it did.)
      const p = g.flags[placeKey(w.i)];
      const place = p === undefined ? undefined : g.regions[p];
      const since = g.flags[placeSinceKey(w.i)] ?? w.since;
      if (place && !allied(g, nat, place.ctrl)) out.push(theWords(`${named(w.row[2], true)}, due in ${place.name} since ${turnToDate(since).label}, waits until ${place.name} is in friendly hands.`));
      continue;
    }
    // (Its month as the UI director's section 7, case 8, gives it: "has waited since {month}".)
    out.push(theWords(`${named(w.row[2], true)}, due in ${g.regions[w.at].name}, has waited since ${turnToDate(w.since).label} for ${join(unformed.map((x) => named(x)))}, which ${hasOrHave(unformed)} not formed yet.`));
  }
  return out;
}

// ---- 6. The Options row: the switch, in any month ------------------------------------------------------------------

/**
 * The Options row's name and line (the UI director's words of 21:49Z, section 6, its close as rewritten at 22:13Z).
 * For a screen that lists the release 2 switches; the 3D's own row carries the same words (CampaignPlayerController.cpp).
 */
export const FROMWAIT_ROW = {
  id: 'fromwaitrule',
  name: 'Arrivals wait while their corps are at the front',
  line: 'An arrival due on a date and formed from a corps already in the field, yours or any other side\'s, waits while that corps is at the front, and forms the month after the corps leaves it. Where an arrival is offered to you, you decide instead. Off, the corps is taken wherever it stands, as before. This changes the month some arrivals form. It works only in a grand campaign: from History\'s shape to Anything goes, and at By the book and Close to history once History\'s book has closed where the arrival is due. It can be switched on or off in any month.',
  // The notes the 3D shows on pressing the row (the bridge's act('fromwaitrule:on' / ':off') reads them here and holds
  // no words of its own): switched on, switched off, and refused outside a grand campaign. The UI director's section 7,
  // case 1, 00:44Z, 26 November 2026.
  on: 'Arrivals now wait while their corps are at the front: one formed from a corps in the field forms the month after that corps leaves the front.',
  off: 'Arrivals no longer wait: a corps an arrival is formed from is taken wherever it stands, as before.',
  refused: 'Arrivals wait for their corps only in a grand campaign.',
} as const;

/** The switch as the rule reads it, in a grand campaign (game.ts `expeditions` reads `g.flags.fromWait`). */
export const fromWaitOn = (g: Game): boolean => g.flags.open === 1 && g.flags.fromWait === 1;

/**
 * Switches 'fromWait' on or off in any month of a grand campaign; outside one it does nothing and answers false. On,
 * a row that takes named corps whole waits from the next `startPhase` that reads it (`expeditions` reads the flag there,
 * nation by nation): this month for the nations still to move after the player, the next month for his own.
 * Off deletes the flag, as a war that never had it: a row waiting this month forms next month wherever its corps
 * stand, with today's headline, and from the moment it is off nothing of the telling is shown (every reader here reads
 * the switch). The telling's own flags (`rowTold:`, `rowAt:`, `rowFront:`) are NOT cleared here: they go where the row
 * is sent or struck (`rowDone`), the design director's answer 3 of 00:43Z, 26 November 2026 ("Clear the telling's three
 * flags where the row is sent or struck (no dead data)"). So a row told, the switch then turned off and on again before
 * it forms, is not told a second time (section 1: "one headline, once"), and its standing line keeps the wait's first
 * month (section 7 case 8: "keep the wait's month ... so it is true whatever came before"). A war switched on and off
 * before any row has waited holds none of them, so its save is the untouched war's; one where a row had waited keeps
 * them until that row forms or is struck. The rule's own `wait:<i>` stays too: it is the month its half year is counted
 * from, as before. Her answer 2 of 00:43Z: "Keep the rule's own wait marker. Off acts from the month it is turned off,
 * and what already happened stays in the save, as everywhere." (tests/from-wait-switch-r2: a war never switched on is
 * the whole save of off; tests/from-wait-switch-war: a war switched off mid-war plays by off from that month with its
 * past kept; tests/from-wait-switch-told and -far: off and on again inside one month tells nothing twice.)
 */
export function setFromWait(g: Game, on: boolean): boolean {
  if (g.flags.open !== 1) return false;
  if (on) { g.flags.fromWait = 1; return true; }
  delete g.flags.fromWait;
  // (The battle record is the switch's own: a war switched off holds none, and on again starts from an empty one.)
  delete g.lastBattle;
  return true;
}
