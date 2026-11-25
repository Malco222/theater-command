// FIRST COMMAND: the first hour's ten lessons as rules (the design director's ruling 129; brief
// design-director/first-hour-brief.md; words cut by ruling 147). One list for both games, read from
// data/campaign/lessons.json: the order, the words, when each lesson comes due, and the test of each deed.
//
//   LESSONS, CLOSING, TIPS          the list as both games show it
//   lessonDue(g, book, l, m)        the lesson's moment has come (its turn in the order, and the situation)
//   currentLesson(g, book, m)       the one note on screen: the first lesson due, not done and not put away
//   lessonDeed(g, book, does, m)    a deed reported by name ('pick', 'march', 'attack', 'end', ...): the lesson it earns, stamped
//   lessonWords(g, l, names)        its heading, sentences and deed line, filled in from the campaign in hand
//   firstCommand(g)                 true in the campaign that FIRST COMMAND opened
//
// Which lessons are done (the notebook) is kept with the player, not with the save, so a second campaign does not
// teach them again unless he asks. Nothing here moves a piece or takes a control: a note waits until its deed is done.

import data from '../../data/campaign/lessons.json';
import { hostileUnits, placeName, trainReachFor } from './game';
import { besiegers } from './siege';
import { townName } from './towns';
import { armyRound } from './round';
import { isOpen } from './staff';
import { canOrder, humanUp, myAirGroups, onEve, raidTargetsFor, reach } from './turn';
import type { Corps, Game, Region } from './types';

/** How an interface reports a deed: what the player did, by its name in the list. */
export type Deed = 'card' | 'pick' | 'march' | 'attack' | 'end' | 'line' | 'siege' | 'rail' | 'raid' | 'order';
/** When a lesson's moment comes, by its name in the list. */
export type Due = 'card' | 'army' | 'reach' | 'foe' | 'month' | 'siege' | 'train' | 'bombers';

export interface Lesson {
  id: string;
  /** Its place in the list, from 1. */
  n: number;
  head: string;
  text: string;
  /** The deed line under the sentences, in the words the deed is tested by. Lesson 1 has none: the card's own button is its deed. */
  deed: string;
  does: Deed;
  due: Due;
  /** The lesson that comes before it. With `months`, either one is enough; alone, it must be done. */
  after?: string;
  /** Months since the campaign was taken over before it is due. With `after`, either one is enough. */
  months?: number;
}
export interface Tip { id: string; text: string; when: string }
export interface LessonGroup { from: number; label: string; plain: string }

type Raw = Omit<Lesson, 'n' | 'does' | 'due'> & { does: string; due: string };
export const LESSONS: Lesson[] = (data.lessons as Raw[]).map((l, i) => ({ ...l, n: i + 1, does: l.does as Deed, due: l.due as Due }));
export const CLOSING = data.closing as { id: string; head: string; text: string };
export const TIPS = data.tips as Tip[];
export const LESSON_GROUPS = data.groups as LessonGroup[];
export const FIRST_COMMAND = data.campaign as { nat: string; from: number; level: string; depth: string };
export const LESSON_DOOR = data.door as { label: string; fresh: string; again: string };
export const LESSON_SETTING = data.setting as string;
/** Each game's own word for a control where it is not the list's: pairs of (the list's words, the game's). */
export const LESSON_NAMES = data.names as unknown as Record<string, [string, string][]>;
/** Lesson 9 alone may run to 36 words: it has three steps (ruling 147(b)). */
export const WORDS_MOST = 30, WORDS_MOST_RAID = 36;

/** What the player has learnt, kept with the player and not with the save. */
export interface Notebook {
  /** "Staff notes for a new commander": on by default until the ten lessons are done. */
  on: boolean;
  /** Lessons stamped DONE, and the closing note once it has been closed. */
  done: string[];
  /** Lessons put away with LATER: they wait in the Guide. */
  later: string[];
  /** Tips already shown once. */
  tips: string[];
}
export const freshNotebook = (): Notebook => ({ on: true, done: [], later: [], tips: [] });
/** A notebook read back from storage, made whole: anything missing is as a new player's. */
export function readNotebook(raw: unknown): Notebook {
  const o = (raw && typeof raw === 'object' ? raw : {}) as Partial<Notebook>;
  // (Only names the list knows: a lesson taken out of a later list is forgotten, not counted.)
  const known = new Set([...LESSONS.map((l) => l.id), CLOSING.id]), tips = new Set(TIPS.map((t) => t.id));
  const ids = (x: unknown, of: Set<string>): string[] => (Array.isArray(x) ? [...new Set(x.filter((s): s is string => typeof s === 'string' && of.has(s)))] : []);
  return { on: o.on !== false, done: ids(o.done, known), later: ids(o.later, known), tips: ids(o.tips, tips) };
}

/** What only an interface can see: whether a month card is open, and the corps the player has in hand. */
export interface Moment {
  card?: boolean;
  inHand?: number[];
}

export const lessonById = (id: string): Lesson | undefined => LESSONS.find((l) => l.id === id);
export const isDone = (book: Notebook, id: string): boolean => book.done.includes(id);
export const allDone = (book: Notebook): boolean => LESSONS.every((l) => isDone(book, l.id));
export const doneCount = (book: Notebook): number => LESSONS.filter((l) => isDone(book, l.id)).length;
/** Notes put away with LATER and not done since: the count on the Guide's button. */
export const waitingCount = (book: Notebook): number => book.later.filter((id) => !isDone(book, id)).length;

/** The notes teach in a grand campaign with a player at the controls, while the setting is on (ruling 129(a)). */
export const lessonsOn = (g: Game, book: Notebook): boolean => book.on && isOpen(g) && !!g.human;

/** True in the campaign FIRST COMMAND opened (the flag main.ts has been waiting for, ruling 204's rider). */
export const firstCommand = (g: Game): boolean => g.flags.firstCommand === 1;
/** The month the player took the campaign over, from which the lessons' months count. */
const seat = (g: Game): number => g.flags.seat ?? 1;

const mine = (g: Game): Corps[] => (g.human ? g.corps.filter((c) => !c.dead && canOrder(g, c)) : []);
const held = (g: Game, m: Moment): Corps[] => {
  const ids = new Set(m.inHand ?? []);
  return ids.size ? mine(g).filter((c) => ids.has(c.id)) : [];
};
const foesBeside = (g: Game, r: Region, nat: string): boolean => r.nb.some((k) => hostileUnits(g, g.regions[k], nat).length > 0);

/** A province where the player's army stands round a town that still holds, with the town's name: lesson 7's moment. */
export function siegeOf(g: Game): { region: number; province: string; city: string } | null {
  const me = g.human;
  if (!me) return null;
  for (const r of g.regions) {
    if (r.ctrl === me || !besiegers(g, r).some((c) => c.nat === me)) continue;
    return { region: r.id, province: r.name, city: townName(r) };
  }
  return null;
}

/** The army lesson 2 names: the first at the front with a move left, else the first with one at all. */
export function pickupPlace(g: Game): number {
  const round = armyRound(g);
  // (Of the armies at the front with no orders, the one nearest his capital, where the map opens: Germany's first is
  // beside Poland, not on the Saar.)
  const cap = g.human ? g.regions[g.nations[g.human]?.capital ?? -1] : undefined;
  const far = (reg: number): number => (cap ? Math.hypot(g.regions[reg].x - cap.x, g.regions[reg].y - cap.y) : 0);
  const front = round.filter((a) => a.front && a.untouched).sort((a, b) => far(a.reg) - far(b.reg));
  return (front[0] ?? round[0])?.reg ?? -1;
}

/**
 * A place's name in running text: "the Saar", "the Danzig Corridor"; a town or a plain political division stays bare.
 * (The UI director's rule of 2026-11-17: the game's own list, data/campaign/places.json, as every other line has it.)
 */
export const placeThe = (name: string): string => placeName(name);
const capital = (s: string): string => s.charAt(0).toUpperCase() + s.slice(1);

/**
 * The army in hand, with no enemy beside it, and railway lit on the map that a train could take it to this month:
 * lesson 8's moment. The map's own test (trainReachFor), so the note never asks for blue ground that is not drawn.
 */
export function trainInHand(g: Game, m: Moment): boolean {
  const cs = held(g, m);
  const me = g.human;
  if (!me || !cs.length || cs.some((c) => foesBeside(g, g.regions[c.reg], me))) return false;
  const goes = new Set<number>();
  for (const c of cs) for (const id of reach(g, c).keys()) if (id !== c.reg) goes.add(id);
  return trainReachFor(g, cs, goes).size > 0;
}

// (A raid's targets are worked out from the whole air war: asked once a month and kept.)
let bombCache: { g: Game; turn: number; flown: number; yes: boolean } | null = null;
/** True while the player has bombers with a target in reach: lesson 9's moment. */
export function bombersInRange(g: Game): boolean {
  if (!g.human) return false;
  if (bombCache && bombCache.g === g && bombCache.turn === g.turn && bombCache.flown === g.airReports.length) return bombCache.yes;
  const yes = myAirGroups(g).some((x) => x.bmr > 0) && raidTargetsFor(g, 'bomb').length > 0;
  bombCache = { g, turn: g.turn, flown: g.airReports.length, yes };
  return yes;
}

/** Whether the lesson's turn in the order has come: the lesson before it done, or the months gone by. */
export function lessonReady(g: Game, book: Notebook, l: Lesson): boolean {
  const after = l.after ? isDone(book, l.after) : null;
  const months = l.months !== undefined ? g.turn - seat(g) >= l.months : null;
  if (after !== null && months !== null) return after || months;
  return (after ?? true) && (months ?? true);
}

/** Whether the situation the lesson teaches stands in front of the player now. */
export function lessonMoment(g: Game, l: Lesson, m: Moment): boolean {
  const me = g.human;
  if (!me || !g.nations[me]?.alive) return false;
  if (l.due === 'card') return !!m.card;
  if (!humanUp(g) || onEve(g)) return false;
  switch (l.due) {
    case 'army': return pickupPlace(g) >= 0;
    case 'reach': return held(g, m).some((c) => [...reach(g, c)].some(([id, x]) => id !== c.reg && !x.attack));
    case 'foe': return mine(g).some((c) => c.ap > 0 && c.kind !== 'art' && c.kind !== 'eng' && c.kind !== 'mg' && c.kind !== 'mor' && foesBeside(g, g.regions[c.reg], me));
    case 'month': return true;
    case 'siege': return siegeOf(g) !== null;
    case 'train': return trainInHand(g, m);
    case 'bombers': return bombersInRange(g);
  }
  return false;
}

export const lessonDue = (g: Game, book: Notebook, l: Lesson, m: Moment): boolean => !isDone(book, l.id) && lessonReady(g, book, l) && lessonMoment(g, l, m);

/** The one note on screen: the first lesson due, in the list's order, that is not done and not put away. Null when none is. */
export function currentLesson(g: Game, book: Notebook, m: Moment): Lesson | null {
  if (!lessonsOn(g, book)) return null;
  for (const l of LESSONS) if (!book.later.includes(l.id) && lessonDue(g, book, l, m)) return l;
  return null;
}

/** True while the closing note is owed: all ten done and the note not yet closed. */
export const closingOwed = (g: Game, book: Notebook): boolean => lessonsOn(g, book) && allDone(book) && !isDone(book, CLOSING.id);

/**
 * A deed the player did, reported by name. It earns the first lesson not yet done whose deed it is and whose turn in
 * the order has come (a month ended before the first attack does not stamp lesson 5, which has not yet been read).
 * An interface reports 'siege' when he storms a town, and as he ends a month while a siege of his stands (siegeOf
 * asked before the month is handed over). Returns the lessons stamped; the notebook is changed.
 */
export function lessonDeed(g: Game, book: Notebook, does: Deed, m: Moment = {}): Lesson[] {
  if (!lessonsOn(g, book)) return [];
  const out: Lesson[] = [];
  const stamp = (l: Lesson | undefined): void => {
    if (!l || isDone(book, l.id) || !lessonReady(g, book, l)) return;
    book.done.push(l.id);
    book.later = book.later.filter((x) => x !== l.id);
    out.push(l);
  };
  // (A month card closed earns lesson 1 only while lesson 1 lay on it: the interface says so by having the card open.)
  if (does === 'card' && !m.card) return out;
  stamp(LESSONS.find((l) => l.does === does && !isDone(book, l.id)));
  return out;
}

/** The raids that count as bombing for lesson 9: anything the bombers fly at a target, never a fight for the sky. */
export const BOMBING: readonly string[] = ['bomb', 'night', 'ground', 'line', 'town', 'harbour', 'strike', 'fleet'];

/** Puts the note away with LATER: it waits in the Guide, and the next lesson due comes up. */
export function putAway(book: Notebook, id: string): void {
  if (!book.later.includes(id) && !isDone(book, id)) book.later.push(id);
}
/** Brings a note put away back: it shows again the next time its moment comes. */
export function bringBack(book: Notebook, id: string): void {
  book.later = book.later.filter((x) => x !== id);
}
/** SHOW ME AGAIN: a lesson done is armed again for the next time its moment comes. */
export function showAgain(book: Notebook, id: string): void {
  book.done = book.done.filter((x) => x !== id && x !== CLOSING.id);
  bringBack(book, id);
  book.on = true;
}
/** All ten again, from the first (the door's FIRST COMMAND once they are done: "The ten lessons again."). */
export function lessonsAgain(book: Notebook): void {
  book.done = [];
  book.later = [];
  book.on = true;
}

/** A sentence's words, counted as a reader counts them. */
export const wordCount = (s: string): number => s.split(/\s+/).filter((w) => /[A-Za-z0-9{]/.test(w)).length;

/** A lesson's words with one game's own names put in. `under` is said where a control stands under another (the browser's BUILD under STAFF). */
export function namedWords(s: string, game: string, under = ''): string {
  let out = s;
  for (const [was, now] of LESSON_NAMES[game] ?? []) out = out.split(was).join(now.split('{under}').join(under));
  return out;
}

/** A lesson's heading, sentences and deed line as one game says them, filled in from the campaign in hand. */
export function lessonWords(g: Game, l: Lesson, game: string, under = ''): { head: string; text: string; deed: string } {
  const at = pickupPlace(g), s = siegeOf(g);
  const fill = (t: string): string => t
    .split('{place}').join(at >= 0 ? placeThe(g.regions[at].name) : 'the front')
    .split('{province}').join(s ? capital(placeThe(s.province)) : 'The province')
    // (A province named for its town is not named twice: "Vancouver is not yours until its town falls.")
    .split('{city}').join(s && s.city !== s.province ? placeThe(s.city) : 'its town');
  return { head: l.head, text: fill(namedWords(l.text, game, under)), deed: namedWords(l.deed, game, under) };
}
