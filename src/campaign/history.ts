// The history dial (Version 70), in the grand campaign only. One setting from 1 (By the book) to 5 (Anything goes)
// for how far the war the computer runs may move away from the war that was fought. It leashes the computer-run
// world and never the player. The middle stop, History's shape, is the game with nothing added: a campaign with no
// stop stored is played at it, every number below is 1 there and nothing is switched on. The stop is kept in
// g.flags.hist, so it is saved with the game; g.flags.histMax keeps the loosest stop the campaign has been played at.
// The numbers and the calendar live in data/campaign/history.json. What the dial does to who goes to war with whom
// (the book's marks, the neutrals' ambitions) is in claims.ts.

import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import data from '../../data/campaign/history.json';
import { hashString, Rng } from '../core/rng';
import { turnToDate } from '../core/ticket';
import type { Game } from './types';
import { unitsDecide } from './units';

export interface Stop {
  stop: number; id: string; name: string; under: string; blurb: string; lines: string[];
  /** The great choices keep history's calendar while the war still looks like history. */
  book?: boolean;
  /** Months either way of history's date, by the war's seed. */
  slack?: number;
  /** A dated blow goes in on odds this much shorter than its staff would usually ask. */
  nerve?: number;
  mood: number;
  lean: 'real' | number;
  other: number; league: number; join: number;
  ambitions?: boolean; bold?: boolean; early?: number; deal?: number;
  /** The share of the claims that anyone holds in a given war, and the most months a bold claimant lets go by before it first presses one. */
  share?: number; wait?: number;
}
/** What history's calendar expects in a theatre: these must hold before `until`, or from the month after `by`. */
/**
 * One thing the book expects of a theatre. `why` is the reason the news gives when it fails; `expect`, where a mark
 * has it, is how the dial words the expectation beforehand ("The book is closed in ... if {expect} ..."), and an
 * empty one keeps the mark off the dial's list (one expectation told by several marks is listed once).
 */
export interface Mark { theatre: string; until?: number; by?: number; if: string[]; why: string; expect?: string }
/** A claim on named ground: who claims it, from whom, and why. 'op': the staff's own war for it (open.json): the claim is not pressed while that war may still come, and is put away once it has come (ruling 228). */
export interface Ambition { id: string; nat: string; on: string; ground: string[]; name: string; why: string; stop?: number; if?: string[]; lift?: number; op?: string; /** A great power's claim for a neutral that feeds its enemy (bargains.ts feedClaims, release 2). */ feeds?: boolean }

export const HISTORY = data as unknown as {
  start: number;
  stops: Stop[];
  dates: Record<string, Record<string, number>>;
  /** The conditions that stand in for a dated choice's own while the book dates it. */
  bookIf?: Record<string, string[] | string>;
  unhistoric: string[];
  joined: { axis: string[]; allies: string[] };
  theatre: Record<string, string>;
  /**
   * A theatre's book that does not close at once with one it comes after (b530): at the stops named, while the
   * month is no later than 'by', it waits; it stands if 'if' holds by then, and closes when 'by' is past without it.
   */
  theatres: Record<string, { name: string; after: string[]; stays?: { stops: number[]; by: number; if: string[]; why: string; ask: string } }>;
  marks: Mark[];
  shores: {
    id: string; from: number; owners: string[];
    /** By the book, the month for the shore's mainland where it is later than for the islands named (ruling 233(a)). */
    mainland?: number; islands?: string[];
    /** The far shore facing this one: nobody lands here while anyone on its owner's side holds one of these (ruling 233(e)). */
    facing?: string[];
    /** Off the book, at the stops where history's dates hold, the month before which nobody lands here (ruling 233(b)). */
    usual?: number;
  }[];
  claims: { weak: number; odds: number; thin: number; notice: number; chance: number; guard: number; lapse: number; aid: number; treaty: number; push: number };
  ambitions: Ambition[];
  /** The book's restraints (b509(c), b512): towns a computer staff does not storm, and ground it does not enter, at the stops named. */
  restraint?: { stops: number[]; front?: string[]; storm: { nat: string; town: string; why: string }[]; enter: { nat: string; ground: string[]; why: string }[]; fed?: { nat: string; town: string; by: string; why: string }[] };
};

/** The stop with nothing added: where a campaign starts, and what every game without a stop is played at. */
export const FREE = HISTORY.start;

/** True where the dial can be turned: the grand campaign. A historical scenario is always played by its own book. */
export const dialRules = (g: Game): boolean => g.flags.open === 1;

/** The stop a game is played at, 1 to 5. Outside the grand campaign it is always the middle one. */
export const stopOf = (g: Game): number => (g.flags.open === 1 ? g.flags.hist ?? FREE : FREE);
export const stopDef = (g: Game): Stop => HISTORY.stops.find((s) => s.stop === stopOf(g)) ?? HISTORY.stops.find((s) => s.stop === FREE)!;
export const stopByNumber = (n: number): Stop | undefined => HISTORY.stops.find((s) => s.stop === n);
/**
 * True with Units decide on at a stop that keeps no book (stops 3 to 5): where a held row on the old chart answers an
 * attack by land (ai.ts `answersHere`, B11) and a landing or a drop from the air on it leaves the mark of the shore it
 * won (navy.ts `landCorps`, `shoreWon`). The design director's rulings of 14:15Z and 15:09Z, 25 November 2026 (audit
 * item 38). At the book's stops the book's release is the record's hand, and with Units decide off nothing is read.
 * The one definition both read: ai.ts imports navy.ts, so it lives here, where neither import leads back.
 */
export const unitsFree = (g: Game): boolean => unitsDecide(g) && !stopDef(g).book;
/**
 * True where the book's restraints hold: a computer staff, at a stop that names them (b509(c), b512), in a war where
 * no player holds a seat on that front (b527: computer against computer, or not at all).
 */
const restrained = (g: Game, nat: string): boolean => !!HISTORY.restraint && nat !== g.human && g.flags.open === 1 && HISTORY.restraint.stops.includes(stopOf(g))
  && !(g.human !== null && (HISTORY.restraint.front ?? []).includes(g.human));
/** The computer staff whose own restraint keeps it from storming this town, if any: Germany before Leningrad by the book. */
export const stormHeldBy = (g: Game, town: string): string | null =>
  HISTORY.restraint?.storm.find((s) => s.town === town && restrained(g, s.nat))?.nat ?? null;
/** True for ground a computer staff's armies do not go into by the book: Finland's before Leningrad in 1941. */
export const halted = (g: Game, nat: string, place: string): boolean =>
  restrained(g, nat) && HISTORY.restraint!.enter.some((h) => h.nat === nat && h.ground.includes(place));
/**
 * True for a town the book feeds while it is held by its own side and the computer staff named for it stands round it,
 * with none but computer staffs beside it: Leningrad, fed across Lake Ladoga while Germany rings it. Its stores do not run down.
 */
export const bookFed = (g: Game, town: string, holder: string, ring: string[]): boolean =>
  !!HISTORY.restraint?.fed?.some((f) => f.town === town && f.nat === holder && ring.includes(f.by) && restrained(g, f.by) && !ring.some((n) => n === g.human));
/** The loosest stop this campaign has been played at: what its verdicts, medals and par are read against. */
export const loosest = (g: Game): number => (g.flags.open === 1 ? g.flags.histMax ?? stopOf(g) : FREE);

/** Why the dial cannot be set there, or null if it can. */
export function whyNotStop(g: Game, n: number): string | null {
  if (g.flags.open !== 1) return 'A historical scenario is always played as history fought it. The history dial belongs to the grand campaign.';
  if ((g.flags.dom ?? 0) > 0) return 'The last act has begun: there is no history left to keep to.';
  return stopByNumber(n) ? null : 'There is no such stop on the dial.';
}

/**
 * Turns the dial. It bears on what the computer's staffs and the neutral world decide from the next month on, and is
 * saved with the game. `fresh` is for a campaign that starts at this stop: it has been played at no other.
 */
export function setStop(g: Game, n: number, fresh = false): boolean {
  if (whyNotStop(g, n)) return false;
  const most = fresh ? n : Math.max(loosest(g), n);
  if (n === FREE) delete g.flags.hist; else g.flags.hist = n;
  if (most === FREE && g.flags.hist === undefined) delete g.flags.histMax; else g.flags.histMax = most;
  return true;
}

/** A draw between 0 and 1 that depends only on the war's seed and what is being decided, never on the battle dice. */
export const dialDraw = (g: Game, ...key: (string | number)[]): number => new Rng(hashString(`${g.seed}:hist:${key.join(':')}`)).next();

/** What the stop does to how far a staff's nerve differs from one war to the next: 1 for no change. */
export const moodMul = (g: Game): number => stopDef(g).mood;
/** What the stop does to the chance that small countries found a League or join one: 1 for no change. */
export const leagueMul = (g: Game): number => stopDef(g).league;

/**
 * The weights a staff's character is dealt by at this stop, from its national lean (bold, cautious, opportunist).
 * By the book each staff has the character it most often has; higher up the lean is flattened toward an even deal.
 */
export function leanFor(g: Game, w: [number, number, number]): [number, number, number] {
  const lean = stopDef(g).lean;
  if (lean === 0) return w;
  if (lean === 'real') { const k = w.indexOf(Math.max(...w)); return [k === 0 ? 1 : 0, k === 1 ? 1 : 0, k === 2 ? 1 : 0]; }
  return [w[0] ** (1 - lean), w[1] ** (1 - lean), w[2] ** (1 - lean)];
}

/** True where a neutral whose opinion has reached the mark may join that bloc this month. */
export function mayJoin(g: Game, id: string, side: 'allies' | 'axis'): boolean {
  const p = stopDef(g).join;
  if (p >= 1 || HISTORY.joined[side].includes(id) || g.flags[`courted:${id}`]) return true;
  return p > 0 && dialDraw(g, 'join', id, g.turn) < p;
}

/**
 * True where the dial keeps a staff's envoys away from a neutral (stops 1 and 2): history did not bring it into the
 * war on that side, and the player has not courted it by hand. His own envoys go where he sends them, and a country
 * he has courted himself is one his staff may go on courting.
 */
export const staysOut = (g: Game, id: string, side: 'allies' | 'axis'): boolean => stopDef(g).join < 1 && !HISTORY.joined[side].includes(id) && !g.flags[`courted:${id}`];

// ---- The book: history's calendar for the great choices ---------------------------------------------------

/** The theatre one of the great choices belongs to, if the book has a word on it. */
export const theatreOf = (nat: string, opId: string): string | null => HISTORY.theatre[`${nat}:${opId}`] ?? null;
/** True while the book is still open in a theatre: the stop keeps one, and nothing there has left it. */
export const onBook = (g: Game, theatre: string | null): boolean => !!stopDef(g).book && theatre !== null && !g.flags[`hist:off:${theatre}`];

/**
 * A row fights at the figure of the army that raised it, whoever's flag it stands under (rider b626 to ruling 234,
 * with b628): the stations' `raised` table gives it by the row's name, as a figure (with a last month, where the
 * record's army changed) or as the country whose own army's figure it takes, or for every corps a fallen country
 * left in keeping (`armies`). A row's quality is a fact, so it holds at every stop of the history dial, with the
 * book open or shut (rider b630). Null where the table gives none: the corps then fights at the figure of the
 * country it answers to, as it always did. Nothing is read with the far side off.
 */
interface Raised { armies?: Record<string, number>; rows?: Record<string, number | string | [figure: number, until: number]> }
const RAISED = (): { theatre: string; raised?: Raised } | null => (HISTORY as unknown as { stations?: { theatre: string; raised?: Raised } }).stations ?? null;
export function raisedQuality(g: Game, c: { name: string; raised?: string }): number | null {
  const B = RAISED(), R = B?.raised;
  if (!R) return null;
  const row = R.rows?.[c.name];
  // (A figure with a last month holds to that month: after it the row is again as its flag's are.)
  const of = row !== undefined ? (typeof row === 'number' ? row : typeof row === 'string' ? g.nations[row]?.quality : g.turn <= row[1] ? row[0] : undefined)
    : c.raised !== undefined ? R.armies?.[c.raised] : undefined;
  return of ?? null;
}
/** The month the book was closed in a theatre, or 0. */
export const bookClosed = (g: Game, theatre: string): number => g.flags[`hist:off:${theatre}`] ?? 0;

/** How the dial bears on one of the great choices this month. */
export interface Leash {
  /** 'free': the staff decides, its monthly chance multiplied. 'dated': the blow falls in `date`. 'follow': it follows at once. 'never': not at this stop. */
  kind: 'free' | 'dated' | 'follow' | 'never';
  chance: number;
  /** The month the blow is to fall (the war's months, 1 for September 1939). */
  date?: number;
  /** A dated blow is taken up on odds this much shorter than usual. */
  nerve: number;
  /** Months before its usual first month that the choice may be taken up. */
  early: number;
}

/** The month the book gives one of the great choices in this war: history's, give or take the stop's slack by the war's seed. */
export function bookDate(g: Game, nat: string, opId: string): number | null {
  const date = HISTORY.dates[nat]?.[opId];
  if (date === undefined) return null;
  const slack = stopDef(g).slack ?? 0;
  return date + (slack ? Math.floor(dialDraw(g, 'date', nat, opId) * (2 * slack + 1)) - slack : 0);
}

/** What a choice the book has dated waits on in place of its own conditions, where the book says so. */
export function bookIf(nat: string, opId: string): string[] | null {
  const c = HISTORY.bookIf?.[`${nat}:${opId}`];
  return Array.isArray(c) ? c : null;
}

export function leash(g: Game, nat: string, opId: string): Leash {
  const S = stopDef(g);
  const un = HISTORY.unhistoric.includes(`${nat}:${opId}`);
  // Off the book, or at a stop that keeps none: the staff decides, and a choice history never took is weighed by the stop.
  if (!S.book) return { kind: 'free', chance: un ? S.other : 1, nerve: 1, early: S.early ?? 0 };
  if (!onBook(g, theatreOf(nat, opId))) return { kind: 'free', chance: 1, nerve: 1, early: 0 };
  if (un) return S.other > 0 ? { kind: 'free', chance: S.other, nerve: 1, early: 0 } : { kind: 'never', chance: 0, nerve: 1, early: 0 };
  const date = bookDate(g, nat, opId);
  if (date === null) return { kind: 'follow', chance: 1, nerve: 1, early: 0 };
  return { kind: 'dated', chance: 1, date, nerve: S.nerve ?? 1, early: 0 };
}

/** A shore's own row: the home ground of its owner (the colonies and every other shore go by the '*' row, by the book alone). */
const SHORE = new Map(HISTORY.shores.flatMap((s) => s.owners.filter((o) => o !== '*').map((o) => [o, s] as const)));

/**
 * True where the far shore facing a country's own is no bar to a landing (ruling 233(e)): nobody on the country's
 * side holds one of the provinces named, whoever's flag flies there (a neutral's is no bar). It is a rule for the
 * first crossing: for as long as an enemy of the country stands on its home ground it is spent.
 */
function facingClear(g: Game, owner: string, facing: string[] | undefined): boolean {
  const X = g.nations[owner];
  if (!facing || !X) return true;
  const foe = (id: string): boolean => X.atWar.includes(id);
  // (On its side: the country itself, or anyone who shares an enemy with it and is not its enemy.)
  const beside = (id: string): boolean => id === owner || (!foe(id) && !!g.nations[id]?.atWar.some(foe));
  if (g.regions.some((r) => r.owner === owner && !r.overseas && (foe(r.ctrl) || r.units.some((u) => !u.dead && foe(u.nat))))) return true;
  return facing.every((name) => { const r = g.byName[name]; return !r || !(beside(r.ctrl) || r.units.some((u) => !u.dead && beside(u.nat))); });
}

/** A book shore's month this war: history's, moved by the stop's slack. */
const shoreMonth = (g: Game, id: string, month: number, slack: number): number => month + (slack ? Math.floor(dialDraw(g, 'shore', id) * (2 * slack + 1)) - slack : 0);

/**
 * The dial's word on the second front, for a computer staff: true where it may land an army from the sea this month.
 * `owner` is the country whose ground it is, `overseas` true for a colony, `where` the province.
 *
 * While the book stands (stops 1 and 2) nobody lands on a shore before history's first landing there, and on the
 * mainland of a shore that has islands named not before its own later month (ruling 233(a): Sicily in July 1943, the
 * mainland in September).
 *
 * Off the book, at the stops where history's dates still hold (stop 3, and a stop that keeps a book once the book is
 * closed on the second front), a shore with a 'usual' month is not landed on before it (ruling 233(b): Italy's home
 * ground, the islands too, not before April 1943).
 *
 * Either way a shore with a far shore 'facing' it stays shut while the enemy holds any of that (ruling 233(e): no
 * landing in Italy while an Axis army holds Tunis, Sfax or Mareth). The two loosest stops leave it all to the staff,
 * and so does every game that is not a grand campaign.
 */
export function shoreOpen(g: Game, owner: string, overseas: boolean, where?: string, pacific = false): boolean {
  const S = stopDef(g);
  // (Ruling 200, finding 2: the second front's row reads no ground in Asia and the Pacific. There the Pacific's own
  // rows decide, under the Pacific's theatre, `pacific`: pacific.ts, and no second front is opened while its book stands.)
  if (pacific) return !S.book || !onBook(g, 'pacific');
  const own = overseas ? undefined : SHORE.get(owner);
  if (S.book && onBook(g, 'second')) {
    const rule = own ?? HISTORY.shores.find((s) => s.owners.includes('*'));
    if (!rule) return true;
    const from = rule.mainland !== undefined && where !== undefined && !rule.islands?.includes(where) ? rule.mainland : rule.from;
    return g.turn >= shoreMonth(g, rule.id, from, S.slack ?? 0) && (rule !== own || facingClear(g, owner, rule.facing));
  }
  if (!own || own.usual === undefined || !dialRules(g) || S.ambitions) return true;
  return g.turn >= own.usual && facingClear(g, owner, own.facing);
}

/**
 * Ruling 233(i): while the book stands, the march over a strait from a shore's island to its mainland is a crossing
 * like any other. False where a computer staff's corps, at war with the shore's owner, would march or attack from
 * one of the islands named onto that mainland before the mainland's month (Sicily to Calabria before September
 * 1943). A player's corps, and one that marches with his army, goes when he likes.
 */
export function straitOpen(g: Game, c: { nat: string; joint?: string }, from: { name: string }, to: { name: string; owner: string; overseas?: boolean }): boolean {
  const rule = to.overseas ? undefined : SHORE.get(to.owner);
  if (rule?.mainland === undefined || !rule.islands || rule.islands.includes(to.name) || !rule.islands.includes(from.name)) return true;
  if (c.nat === g.human || (c.joint !== undefined && c.joint === g.human) || !g.nations[to.owner]?.atWar.includes(c.nat)) return true;
  const S = stopDef(g);
  return !S.book || !onBook(g, 'second') || g.turn >= shoreMonth(g, rule.id, rule.mainland, S.slack ?? 0);
}

/** The dial as the player reads it: the stop, its name and what it does, and the loosest stop the campaign has seen. */
export interface DialView {
  stop: number; name: string; under: string; blurb: string; lines: string[];
  loosest: number; loosestName: string;
  /** Where the book has been closed, at a stop that keeps one: the theatre, the month and why. */
  closed: { theatre: string; name: string; turn: number }[];
  /** What the book still expects, at a stop that keeps one: the theatre it would be closed in, by when, and in what case. */
  expects: { theatre: string; name: string; turn: number; text: string }[];
  /** Why it cannot be turned now, or null. */
  locked: string | null;
}

/**
 * What history's calendar still asks of the war, soonest first: each mark not yet passed in a theatre where the book
 * is still open, in words. A player who means to keep to the book can read here what would close it.
 */
export function bookExpects(g: Game): DialView['expects'] {
  const S = stopDef(g);
  if (!S.book) return [];
  const slack = S.slack ?? 0;
  const out: DialView['expects'] = [];
  HISTORY.marks.forEach((m, k) => {
    const t = HISTORY.theatres[m.theatre];
    if (!t || g.flags[`hist:off:${m.theatre}`]) return;
    // (A mark may word what is expected apart from the reason the news gives when it fails; one worded with nothing is not listed.)
    const why = m.expect ?? m.why;
    if (!why) return;
    if (m.until !== undefined) {
      const last = m.until - slack;
      if (g.turn < last) out.push({ theatre: m.theatre, name: t.name, turn: last, text: `The book is closed in ${t.name} if ${why}: that is, before ${turnToDate(last).label}.` });
    } else if (m.by !== undefined && !g.flags[`hist:mk:${k}`]) {
      const last = m.by + slack;
      out.push({ theatre: m.theatre, name: t.name, turn: last, text: `The book is closed in ${t.name} if ${why} when ${turnToDate(last).label} is over.` });
    }
  });
  // A theatre left waiting when one it comes after closed (b530).
  for (const [id, t] of Object.entries(HISTORY.theatres)) {
    if (!t.stays || g.flags[`hist:wait:${id}`] === undefined || g.flags[`hist:off:${id}`]) continue;
    out.push({ theatre: id, name: t.name, turn: t.stays.by, text: `The book is closed in ${t.name} if ${t.stays.ask} when ${turnToDate(t.stays.by).label} is over.` });
  }
  return out.sort((a, b) => a.turn - b.turn);
}

export function dialView(g: Game): DialView | null {
  if (!dialRules(g)) return null;
  const S = stopDef(g);
  const most = loosest(g);
  const closed = S.book ? Object.entries(HISTORY.theatres).filter(([id]) => bookClosed(g, id) > 0).map(([id, t]) => ({ theatre: id, name: t.name, turn: bookClosed(g, id) })) : [];
  return { stop: S.stop, name: S.name, under: S.under, blurb: S.blurb, lines: S.lines, loosest: most, loosestName: stopByNumber(most)?.name ?? S.name, closed, expects: bookExpects(g), locked: (g.flags.dom ?? 0) > 0 ? whyNotStop(g, S.stop) : null };
}
