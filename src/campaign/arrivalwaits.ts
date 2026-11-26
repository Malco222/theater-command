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
 *   rowTold:<i>   the month the wait began, told once in a headline: also the card's "due ... since {month}"
 *   rowAt:<i>     the province the row is due in (where it forms, after `rowPlace`), for the cards' readers
 *   rowFront:<i>  which of the row's named corps were at the front in the latest month it waited, a bit mask over the
 *                 order of its `from`; none where it waited only for another row. The formed headline names them.
 * All three are cleared wherever the row is sent or struck (`rowDone`). With the switch off none is ever written and
 * nothing prints.
 */
import setup from '../../data/campaign/setup-1939.json';
import { turnToDate } from '../core/ticket';
import { headline, rowHolds, theWords } from './game';
import type { Corps, Game, Region } from './types';

const ROWS = (): any[][] => (setup as any).expeditions as any[][] ?? [];
const toldKey = (i: number): string => `rowTold:${i}`;
const atKey = (i: number): string => `rowAt:${i}`;
const frontKey = (i: number): string => `rowFront:${i}`;

/**
 * The article a name takes in running text: none where it opens with "the", is an "X Force" or is Creforce (the
 * general's line's rule, game.ts, lifted here: the UI director's section 0), else "the ".
 */
export const theOf = (name: string): string => (/^(the |[A-Z] Force|Creforce)/i.test(name) ? '' : 'the ');
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
const holdsNow = (g: Game, nat: string, from: string[], at: number | undefined): { front: Corps[]; unformed: string[] } => {
  const h = rowHolds(g, nat, from, at);
  return { front: inOrder(from, h.front), unformed: h.unformed };
};

// ---- 1. The month the wait begins: one headline, once ------------------------------------------------------------

/**
 * The headline the month a row's wait begins (section 1), or null where the UI director's words do not cover the case
 * (both causes at once with more than one of either; a row with nothing to name).
 */
export function waitBeginsLine(g: Game, nat: string, rowName: string, place: string, from: string[], front: Corps[], unformed: string[]): string | null {
  const head = `${named(rowName, true)} is due in ${place} this month`;
  const n = present(g, nat, from).length;
  if (front.length && unformed.length) {
    if (front.length !== 1 || unformed.length !== 1) return null;
    return `${head}. ${named(front[0].name, true)} is at the front in ${placeOf(g, front[0])}, and ${named(unformed[0])}, which it is also formed from, has not formed yet, so the arrival waits for both.`;
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
  const list = join(front.map((c, k) => `${named(c.name, k === 0)} in ${placeOf(g, c)}`));
  return `${several} ${list} are at the front, so the arrival waits, and forms the month after ${front.length === 2 ? 'both' : 'all of them'} leave the front.`;
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
  const { front, unformed } = holdsNow(g, nat, from, at.id);
  let mask = 0;
  for (const c of front) mask |= 1 << from.indexOf(c.name);
  if (mask) g.flags[frontKey(i)] = mask; else delete g.flags[frontKey(i)];
  g.flags[atKey(i)] = at.id;
  if (g.flags[toldKey(i)] !== undefined) return;
  g.flags[toldKey(i)] = g.turn;
  const line = waitBeginsLine(g, nat, row[2], at.name, from, front, unformed);
  if (line) headline(g, 'info', line, nat, at.id);
}

// ---- 4 and 5. The month it forms -----------------------------------------------------------------------------------

/**
 * The row is sent or struck (game.ts sets `sent:<i>`): the telling's three flags go with it, so no dead data stays in
 * the save. The design director's answer 3 of 00:43Z, 26 November 2026: "Clear the telling's three flags where the row
 * is sent or struck (no dead data)." Called at every place game.ts sets `sent:<i>` for a row, after the formed headline
 * has read them (`formedLine` calls it itself); a row that never waited has none, and nothing changes.
 */
export function rowDone(g: Game, i: number): void {
  delete g.flags[toldKey(i)]; delete g.flags[atKey(i)]; delete g.flags[frontKey(i)];
}

/** What a row that waited on the front is formed from, read before its corps are taken (game.ts `expeditions`). */
export interface WaitedOn { count: number; one: string | null; front: string[] }

/**
 * Read in `expeditions` once a row has passed both statements, before it takes its corps: null unless it waited on the
 * front in its latest waiting month (`rowFront`). How many of its named corps stand, the one where it is one, and the
 * names that were at the front.
 */
export function waitedOn(g: Game, i: number): WaitedOn | null {
  const mask = g.flags[frontKey(i)];
  if (!mask) return null;
  const row = ROWS()[i];
  const from = fromOf(row);
  const here = present(g, row[1], from);
  return { count: here.length, one: here.length === 1 ? here[0].name : null, front: from.filter((_, k) => (mask & (1 << k)) !== 0) };
}

/**
 * The words the month a row forms (sections 4 and 5), for game.ts's arrival headline: `alone` takes the place of
 * today's bare line ("{row} has landed at {place}."), `after` is said after a row's own news or its general's line.
 * Null where the telling has nothing to say and today's line stands. Clears the row's telling flags: it has formed.
 * `own` is true where the row came with men of its own while its corps were at the front, in the telling's scope.
 */
export function formedLine(g: Game, i: number, was: WaitedOn | null, own: boolean, c: Corps, reg: Region, bySea: boolean): { alone: string | null; after: string | null } | null {
  const row = ROWS()[i];
  rowDone(g, i);
  if (!row || !g.human || row[1] !== g.human) return null;
  const verb = bySea ? 'landed at' : 'arrived in';
  const head = `${named(c.name, true)} has ${verb} ${reg.name}`;
  if (own) {
    const from = fromOf(row);
    const { front } = holdsNow(g, row[1], from, reg.id);
    const here = present(g, row[1], from);
    // (Every corps it leaves, and which of them is at the front: the words cover all of them at the front.)
    if (!front.length || front.length !== here.length) return null;
    const alone = front.length === 1
      ? `${head} without your ${front[0].name}, which is at the front in ${placeOf(g, front[0])} and stays a corps of its own.`
      : `${head} without ${join(front.map((x) => `your ${x.name}`))}, which are at the front in ${join(front.map((x) => placeOf(g, x)))} and stay corps of their own.`;
    return { alone, after: null };
  }
  if (!was || !was.count || !was.front.length) return null;
  const left = `now that ${join(was.front.map((x) => named(x)))} ${was.front.length === 1 ? 'has' : 'have'} left the front`;
  if (was.count === 1 && was.one) {
    const alone = was.one === c.name
      ? `${head}, formed from your corps of that name, now that it has left the front.`
      : `${head}, formed from your ${was.one}, now that it has left the front.`;
    return { alone, after: `The arrival is formed from your ${was.one}, now that it has left the front.` };
  }
  return { alone: `${head}, formed from ${numberWord(was.count)} of your corps, ${left}.`, after: `The arrival is formed from ${numberWord(was.count)} of your corps, ${left}.` };
}

// ---- 2. While it waits: the cards' readers -------------------------------------------------------------------------

/** The rows of the player's that wait in the telling (told, not sent), each with where it is due and its named corps. */
function waiting(g: Game, nat: string): { i: number; row: any[]; at: number; since: number; from: string[] }[] {
  if (g.flags.fromWait !== 1 || nat !== g.human) return [];
  const out: { i: number; row: any[]; at: number; since: number; from: string[] }[] = [];
  ROWS().forEach((row, i) => {
    const since = g.flags[toldKey(i)];
    if (since === undefined || row[1] !== nat || g.flags[`sent:${i}`]) return;
    const at = g.flags[atKey(i)];
    if (at === undefined || !g.regions[at]) return;
    out.push({ i, row, at, since, from: fromOf(row) });
  });
  return out;
}

/**
 * The standing line for a corps of the player's at the front that a waiting row is to be formed from (section 2):
 * `short` for the tray card ("An arrival waits for it"), `whole` for its hover, the record card and the 3D corps card's
 * muted note. Read from the board as it stands: the line goes the moment the corps leaves the front. Null otherwise.
 */
export function arrivalWaitOf(g: Game, c: Corps): { short: string; whole: string } | null {
  if (c.dead || g.flags.fromWait !== 1 || c.nat !== g.human) return null;
  for (const w of waiting(g, c.nat)) {
    if (!w.from.includes(c.name)) continue;
    const { front, unformed } = holdsNow(g, c.nat, w.from, w.at);
    // (A row that also waits for another row to form: the UI director's words do not cover it; no line.)
    if (!front.includes(c) || unformed.length) continue;
    const others = front.filter((x) => x !== c).map((x) => named(x.name));
    const who = others.length ? `${join(['this corps', ...others])} leave` : 'this corps leaves';
    const whole = theWords(`An arrival due in ${g.regions[w.at].name} since ${turnToDate(w.since).label}, ${named(w.row[2])}, is to be formed from this corps. It forms the month after ${who} the front.`);
    return { short: 'An arrival waits for it', whole };
  }
  return null;
}

/**
 * The month's report card (section 2): for each waiting row of the player's that waits only for another row to form,
 * and so has no corps to carry it, one muted line, every month it waits.
 */
export function rowsWaitingOnRows(g: Game, nat: string | null): string[] {
  if (!nat) return [];
  const out: string[] = [];
  for (const w of waiting(g, nat)) {
    const { front, unformed } = holdsNow(g, nat, w.from, w.at);
    if (front.length || !unformed.length) continue;
    out.push(theWords(`${named(w.row[2], true)}, due in ${g.regions[w.at].name} since ${turnToDate(w.since).label}, waits for ${join(unformed.map((x) => named(x)))}, which ${unformed.length === 1 ? 'has' : 'have'} not formed yet.`));
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
 * a row that takes named corps whole waits from the next month it comes due (`expeditions` reads the flag each month).
 * Off deletes the flag, as a war that never had it, and the telling's own flags with it (`rowTold:`, `rowAt:`,
 * `rowFront:`), so a later "on" starts clean: a row waiting this month forms next month wherever its corps stand, with
 * today's headline. The rule's own `wait:<i>` stays: it is the month its half year is counted from, as before. The
 * design director's answer 2 of 00:43Z, 26 November 2026: "Keep the rule's own wait marker. Off acts from the month it
 * is turned off, and what already happened stays in the save, as everywhere." (tests/from-wait-switch-r2: a war never
 * switched on is the whole save of off; tests/from-wait-switch-war: a war switched off mid-war plays by off from that
 * month with its past kept.)
 */
export function setFromWait(g: Game, on: boolean): boolean {
  if (g.flags.open !== 1) return false;
  if (on) { g.flags.fromWait = 1; return true; }
  delete g.flags.fromWait;
  for (const k of Object.keys(g.flags)) if (k.startsWith('rowTold:') || k.startsWith('rowAt:') || k.startsWith('rowFront:')) delete g.flags[k];
  return true;
}
