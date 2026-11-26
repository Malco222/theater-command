// Manpower (the grand campaign).
//
// Every nation has a reserve of men fit to bear arms, counted in divisions' worth. The replacements that fill up
// worn corps and every new corps are drawn from it. Each month a class comes of age and is added to it: more in a
// great country than a small one, and less as home ground is lost. A nation whose reserve is spent can make good
// only as many losses as the month's class allows, and can raise no new corps at all.
//
// The same rule holds for every nation, the player's and the computer's alike. The numbers are in
// data/campaign/manpower.json. The historical scenarios have no reserve: their armies are filled as they always were.
//
// State is kept in g.flags, so it is saved with the campaign:
//   mp:<nat>    the reserve, in divisions' worth of men
//   mps:<nat>   the reserve it opened the campaign with
//   mpx:<nat>   men drawn from it since the campaign opened, in divisions' worth
//   mpl:<nat>   the lowest warning the player has been given (1 half gone, 2 three quarters gone, 3 spent)
//   mpe:<nat>   the last month in which no class comes of age, after one was called up early

import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import data from '../../data/campaign/manpower.json';
import { turnToDate } from '../core/ticket';
import { MEN_PER_DIV, type Corps, type Game } from './types';
import { farShare } from './economy';
import { binds, inWords, levelOf } from './rifles';

export const MANPOWER = data as unknown as {
  /** The reserve a great power opens a campaign of September 1939 with, in divisions' worth. */
  pool: Record<string, number>;
  /** The class that comes of age each month, in divisions' worth, with the whole homeland held. */
  class: Record<string, number>;
  /** Any other country: a reserve of 'pool' and 'perDiv' for each division it has under arms as the campaign opens, and a monthly class of 'classShare' of that reserve. */
  minor: { pool: number; perDiv: number; classShare: number };
  /** The reserve never holds more than this many times what it opened with. */
  most: number;
  /** For a campaign that opens later: the share of the reserve already spent for each year of war behind it, and the most that can be gone. */
  spentAYear: number;
  spentMost: number;
  /** The player is warned as the reserve falls under these shares of what it opened with. */
  warn: number[];
  /** The least share of the month's class a country still raises with its homeland overrun. */
  classFloor: number;
  /** Calling the next class up early: that many months of classes come at once, and none comes of age for 'gap' months after. */
  early: { months: number; gap: number };
  /** Folding two worn corps into one: see the note in the data. */
  fold: { worn: number; org: number; price: number; xp: number };
  /** Men to one of the nation's divisions' worth, for its sentences, where the rows lay a scale of its own (the far side's China, data/campaign/far-arms.json '_scale'); any other nation's men are MEN_PER_DIV a division's worth. */
  scale?: Record<string, number>;
};

const KP: Record<string, string> = {}, KX: Record<string, string> = {}, KL: Record<string, string> = {}, KS: Record<string, string> = {};
const ks = (nat: string): string => (KS[nat] ??= `mps:${nat}`);
const kp = (nat: string): string => (KP[nat] ??= `mp:${nat}`);
const kx = (nat: string): string => (KX[nat] ??= `mpx:${nat}`);
const kl = (nat: string): string => (KL[nat] ??= `mpl:${nat}`);
const ke = (nat: string): string => `mpe:${nat}`;

/** True where the reserve of men is counted: the grand campaign. */
export const menRules = (g: Game): boolean => g.flags.open === 1;

/** The reserve a nation has by the tables: a great power's own figure, any other country's by the army it keeps. */
function tablePool(g: Game, nat: string): number {
  const own = MANPOWER.pool[nat];
  if (own !== undefined) return own;
  let divs = 0;
  for (const c of g.corps) if (c.nat === nat && !c.dead && c.kind !== 'fort') divs += c.divs;
  return MANPOWER.minor.pool + MANPOWER.minor.perDiv * divs;
}

/** The reserve a nation opened the campaign with. */
export const poolAtStart = (g: Game, nat: string): number => g.flags[ks(nat)] ?? tablePool(g, nat);

/** The month's class with the whole homeland held. */
export const classFull = (g: Game, nat: string): number => MANPOWER.class[nat] ?? poolAtStart(g, nat) * MANPOWER.minor.classShare;

/** Fixes the reserve a nation opened with, the first time anything is taken from it or added to it. */
function ensure(g: Game, nat: string): void {
  if (g.flags[ks(nat)] !== undefined) return;
  const start = tablePool(g, nat);
  g.flags[ks(nat)] = start;
  if (g.flags[kp(nat)] === undefined) g.flags[kp(nat)] = start;
}

/**
 * The share of its homeland a nation still holds, by the worth of the provinces: where its young men are. Home ground
 * on the far side of the world counts at its share here, as it does for money, for lines and for the standing call-up
 * (ruling b483, economy.ts farShare): the Soviet Union's 169 provinces beyond the old chart weigh 280 beside the old
 * chart's 194 and are never lost, and counted whole they kept the month's class at nine tenths of its size where the
 * old chart's reckoning cut it to three quarters.
 */
export function homeShare(g: Game, nat: string): number {
  let all = 0, held = 0;
  for (const r of g.regions) {
    if (r.owner !== nat || r.overseas) continue;
    const w = (1 + r.vp + r.fac) * farShare(r);
    all += w;
    if (r.ctrl === nat) held += w;
  }
  return all ? held / all : 1;
}

/** The men in a nation's reserve, in divisions' worth. Infinite where no reserve is counted. */
export function menLeft(g: Game, nat: string): number {
  if (!menRules(g)) return Infinity;
  return g.flags[kp(nat)] ?? poolAtStart(g, nat);
}

/** How many new corps of this many divisions the reserve could fill today. */
export const raisable = (g: Game, nat: string, divs: number): number => (menRules(g) ? Math.floor((menLeft(g, nat) + 1e-9) / Math.max(0.1, divs)) : Infinity);

/**
 * What the reserve holds beyond what the armies in the field are short of: the men a staff can put into new corps
 * without starving the old ones of their drafts. (Where a nation's rifles row binds, a corps is short only up to its
 * armed level, which its drafts refill to and no further: rifles.ts, the design director's 05:37Z, the form's section 8.)
 */
export function menSpare(g: Game, nat: string): number {
  if (!menRules(g)) return Infinity;
  const lvl = binds(g, nat) ? (c: Corps): number => Math.min(levelOf(g, c), 1) : (): number => 1;
  let short = 0;
  for (const c of g.corps) if (c.nat === nat && !c.dead) short += (lvl(c) - c.str) * c.divs;
  for (const v of g.atSea ?? []) if (v.nat === nat) short += (lvl(v.corps) - v.corps.str) * v.corps.divs;
  return menLeft(g, nat) - short;
}

/** The class that comes of age this month: the nation's own, by the share of the homeland it holds, and what its government's call-up makes of it. */
export function classNow(g: Game, nat: string, callUp = 1): number {
  if (!menRules(g)) return 0;
  return classFull(g, nat) * Math.max(MANPOWER.classFloor, homeShare(g, nat)) * callUp;
}

/** Takes men from the reserve: as many as asked for, or all there are. Returns what was given, in divisions' worth. */
export function drawMen(g: Game, nat: string, divs: number): number {
  if (!menRules(g) || divs <= 0) return divs;
  ensure(g, nat);
  const have = menLeft(g, nat);
  const give = Math.min(have, divs);
  g.flags[kp(nat)] = have - give;
  g.flags[kx(nat)] = (g.flags[kx(nat)] ?? 0) + give;
  return give;
}

/** Hands men back to the reserve: an order for a new corps taken back in the month it was given. */
export function returnMen(g: Game, nat: string, divs: number): void {
  if (!menRules(g) || divs <= 0) return;
  ensure(g, nat);
  g.flags[kp(nat)] = menLeft(g, nat) + divs;
  g.flags[kx(nat)] = Math.max(0, (g.flags[kx(nat)] ?? 0) - divs);
}

/**
 * A levy brings men to the reserve who were in no class (Version 68; traits.ts). The warnings to the player start
 * again from where the reserve now stands, so he is told when these men too are spent.
 */
export function levyMen(g: Game, nat: string, divs: number): void {
  if (!menRules(g) || divs <= 0) return;
  ensure(g, nat);
  g.flags[kp(nat)] = menLeft(g, nat) + divs;
  if (g.flags[kl(nat)] !== undefined) g.flags[kl(nat)] = Math.min(g.flags[kl(nat)], menLevel(g, nat));
}

/** The last month in which no class comes of age, because it was called up early: 0 where none was. */
export const classGapUntil = (g: Game, nat: string): number => (menRules(g) ? g.flags[ke(nat)] ?? 0 : 0);

/** Why a nation cannot call its next class up early, or null if it can. */
export function whyNotEarly(g: Game, nat: string): string | null {
  if (!menRules(g)) return 'The reserve of men is counted in the grand campaign.';
  const n = g.nations[nat];
  if (!n?.alive) return 'There is no government left to call anyone up.';
  if (!n.atWar.length) return 'Only a country at war calls its boys up before their time.';
  const until = classGapUntil(g, nat);
  if (g.turn <= until) return `The next class is with the colours already: none comes of age until ${turnToDate(until + 1).label}.`;
  // (Room for under a thousand men is no room.)
  if (poolAtStart(g, nat) * MANPOWER.most - menLeft(g, nat) < 1000 / MEN_PER_DIV) return 'The reserve is full: there is nowhere to put them.';
  return null;
}

/**
 * The months no class comes of age after an early call that brings so many men (in divisions' worth): in step with
 * what came, so that a call which finds room in the reserve for two months of classes costs two months of them.
 */
export function earlyGap(g: Game, nat: string, come: number, callUp = 1): number {
  const E = MANPOWER.early, one = classNow(g, nat, callUp);
  return one > 0 ? Math.max(1, Math.ceil((come / one) * (E.gap / E.months) - 1e-6)) : E.gap;
}

/** What calling the next class up early would bring today, in divisions' worth. */
export function earlyClass(g: Game, nat: string, callUp = 1): number {
  if (!menRules(g)) return 0;
  return Math.max(0, Math.min(classNow(g, nat, callUp) * MANPOWER.early.months, poolAtStart(g, nat) * MANPOWER.most - menLeft(g, nat)));
}

/**
 * Calls the next class up early: months of classes join the reserve at once, and none comes of age for a while
 * after. Returns what came, in divisions' worth, or 0 where it cannot be done.
 */
export function callEarly(g: Game, nat: string, callUp = 1): number {
  if (whyNotEarly(g, nat) !== null) return 0;
  ensure(g, nat);
  const come = earlyClass(g, nat, callUp);
  if (come <= 0) return 0;
  g.flags[kp(nat)] = menLeft(g, nat) + come;
  g.flags[ke(nat)] = g.turn + earlyGap(g, nat, come, callUp);
  return come;
}

/** The month's class joins the reserve. Returns what came, in divisions' worth. */
export function classArrives(g: Game, nat: string, callUp = 1): number {
  if (!menRules(g)) return 0;
  // A class called up early is with the colours already: nobody comes of age in the months it was taken from.
  if (g.turn <= (g.flags[ke(nat)] ?? 0)) return 0;
  ensure(g, nat);
  const have = menLeft(g, nat), top = poolAtStart(g, nat) * MANPOWER.most;
  const come = Math.max(0, Math.min(classNow(g, nat, callUp), top - have));
  g.flags[kp(nat)] = have + come;
  return come;
}

/** Sets every nation's reserve as a grand campaign opens: full in September 1939, part spent in a later year. */
export function openManpower(g: Game): void {
  const years = Math.max(0, (g.turn - 1) / 12);
  for (const n of Object.values(g.nations)) {
    const gone = n.atWar.length || years >= 2 ? Math.min(MANPOWER.spentMost, MANPOWER.spentAYear * years) : 0;
    const start = tablePool(g, n.id);
    g.flags[ks(n.id)] = start;
    g.flags[kp(n.id)] = start * (1 - gone);
  }
}

export interface MenView {
  nat: string;
  /** The reserve: divisions' worth, and men. */
  divs: number;
  men: number;
  /** What it opened with, and the share of that still in hand, 0 to 1 (it can stand above 1 for a while). */
  start: number;
  share: number;
  /** The class that comes of age each month, in men, and the share of the homeland it is drawn from. */
  classMen: number;
  home: number;
  /** Men drawn since the campaign opened, and the men who have come of age and joined the reserve since then: what it opened with, and those who came, less those drawn, is what stands in it. */
  drawn: number;
  came: number;
  /** Months still to come, after this one, in which no class comes of age because one was called up early: 0 as a rule. */
  gap: number;
  /** One word for how it stands: 'Ample', 'Thinning', 'Short' or 'Spent'. */
  word: string;
  /** How it stands, in a sentence. */
  line: string;
  /** Men to one of the nation's divisions' worth, as the men above are counted: its own scale, else MEN_PER_DIV. */
  per: number;
  /** The month's class in a sentence, for a nation with a scale of its own (comesWords); null for any other, whose pages say it as before. */
  comes: string | null;
}

// (1.5.2, playtest #48: under a thousand men the figure is given whole, so a small nation's 235 a month never reads as 0.)
const round = (men: number): number => (men >= 100000 ? Math.round(men / 10000) * 10000 : men >= 1000 ? Math.round(men / 1000) * 1000 : Math.round(men));

/** The nation's own scale, men to one of its divisions' worth, where the rows lay one; else null. */
export function menScale(nat: string): number | null {
  const x = MANPOWER.scale?.[nat];
  return typeof x === 'number' && Number.isFinite(x) && x > 0 ? x : null;
}

/** Men to one of the nation's divisions' worth, for what it is shown: its own scale, else MEN_PER_DIV. */
export const menPer = (nat: string): number => menScale(nat) ?? MEN_PER_DIV;

/**
 * Men to the thousand, for a nation with a scale of its own: an estimate, said with "about" (the UI director's read of
 * 26 November 2026, 01:59Z, point 1: "a nation's men at its own scale, which are an estimate and say 'About', to the
 * thousand"). Rifles stay to the hundred.
 */
export const men1000 = (men: number): string => (Math.round(Math.max(0, men) / 1000) * 1000).toLocaleString('en-US');
/** "division's worth" for one, "divisions' worth" for any other count. */
export const worth = (d: number): string => (d === 1 ? "division's worth" : "divisions' worth");

/**
 * The reserve in a sentence, for a nation with a scale of its own (the UI director's line 14 of 25 November 2026:
 * "About 500,000 men stand in reserve: about seven divisions' worth."; the men shown rounded, not the scale, and to the
 * thousand by her read of 26 November, point 1), or null for any other, whose line stays as it was.
 */
export function reserveWords(nat: string, divs: number): string | null {
  const per = menScale(nat);
  if (per === null) return null;
  const d = Math.round(Math.max(0, divs));
  return `About ${men1000(divs * per)} men stand in reserve: about ${inWords(d)} ${worth(d)}.`;
}

/**
 * The month's class in a sentence, for a nation with a scale of its own (the UI director's line 15 of 25 November
 * 2026: "About 105,000 men come of age each month in the land you still hold."; where a page shows the class before
 * the share of the homeland held, 'whole', "with all your homeland held"; the men to the thousand by her read of 26
 * November, point 1), or null for any other.
 */
export function comesWords(nat: string, cls: number, whole = false): string | null {
  const per = menScale(nat);
  if (per === null) return null;
  return `About ${men1000(cls * per)} men come of age each month ${whole ? 'with all your homeland held' : 'in the land you still hold'}.`;
}

/**
 * How the reserve stands, in a sentence, for a nation with a scale of its own at a level below ample (1 thinning, 2
 * short, 3 spent), or null for any other nation or level. The UI director's read of 26 November 2026, point 5: "about"
 * on each figure at the nation's own scale, to the thousand, months in words ("The reserve is thinning: about 201,000
 * men are left of about 496,000."); the spent line says no class figure, since line 15 says it in the same panel.
 */
export function reserveLevelWords(nat: string, level: number, divs: number, start: number, cls: number): string | null {
  const per = menScale(nat);
  if (per === null || level < 1 || level > 3) return null;
  if (level === 1) return `The reserve is thinning: about ${men1000(divs * per)} men are left of about ${men1000(start * per)}.`;
  if (level === 2) return `The reserve is running short: about ${men1000(divs * per)} men are left. Every loss made good and every new corps is taken from it.`;
  const months = cls > 0 ? Math.ceil(3 / cls) : 0;
  return `The reserve is spent. Only the men who come of age each month reach the armies, and no new corps can be raised${months ? `: a new infantry corps is about ${months === 1 ? 'a month' : `${inWords(months)} months`} of them` : ''}.`;
}

/**
 * The arsenal's refusal for lack of men, for a nation with a scale of its own, or null for any other (whose refusal
 * stays "The reserve has not the men: this takes 45,000 and 15,000 are left. ..."). The UI director's read of 26
 * November 2026, point 6: "The reserve has not the men: this takes three divisions' worth and about one is left. About
 * 105,000 men come of age each month in the land you still hold."; under half a division's worth left, "less than half
 * of one is left"; "about one" from half up, words to ten. What is left never reads as what it takes: where rounding
 * would show the need or more, it shows one less (the same care as the store's, rifles.ts noRifles).
 */
export function noMenWords(nat: string, divs: number, left: number, cls: number): string | null {
  const per = menScale(nat);
  if (per === null) return null;
  const l = Math.max(0, left);
  const shown = l < 0.5 ? 0 : Math.min(Math.round(l), Math.ceil(divs) - 1);
  // (An order of one division's worth with half of one or more left: "about one" would read as the need, and "less than
  // half" would be untrue, so it says "less than one is left". China's engineer corps, corps:eng, is such an order: with
  // half a division's worth to just under one left, this is its live refusal.)
  const rest = shown >= 1 ? `about ${inWords(shown)} ${shown === 1 ? 'is' : 'are'} left` : l < 0.5 ? 'less than half of one is left' : 'less than one is left';
  return `The reserve has not the men: this takes ${inWords(divs)} ${worth(divs)} and ${rest}. ${comesWords(nat, cls)}`;
}

/**
 * What a new infantry corps of so many divisions takes from the reserve, for the ledger's men block. A nation with a
 * scale of its own says it in divisions' worth, the months in words (the UI director's line 16 of 25 November 2026:
 * divisions' worth on line 8 for such a nation, line 14 the bridge between the two); every other nation in men, as
 * before, byte for byte.
 */
export function corpsMenWords(v: MenView, divs: number): string {
  const months = v.classMen > 0 ? Math.ceil((divs * v.per) / v.classMen) : 0;
  if (v.comes === null) return `A new infantry corps takes ${menWords(divs * v.per)} men${months > 1 ? `, about ${months} months of the class` : ''}.`;
  return `A new infantry corps takes ${inWords(divs)} ${divs === 1 ? "division's" : "divisions'"} worth of men${months > 1 ? `, about ${inWords(months)} months of the class` : ''}.`;
}

/** Men, in words: "1.2 million", "340,000". */
export function menWords(men: number): string {
  const m = round(Math.max(0, men));
  return m >= 1000000 ? `${(m / 1000000).toFixed(m >= 10000000 ? 0 : 1).replace(/\.0$/, '')} million` : m.toLocaleString('en-US');
}

const WORDS = ['Ample', 'Thinning', 'Short', 'Spent'];

/** How far down the reserve is: 0 ample, 1 under the first warning mark, 2 under the second, 3 spent. */
export function menLevel(g: Game, nat: string): number {
  if (!menRules(g)) return 0;
  const have = menLeft(g, nat), start = poolAtStart(g, nat);
  if (have < Math.max(1, classFull(g, nat))) return 3;
  const share = start > 0 ? have / start : 0;
  return share < MANPOWER.warn[1] ? 2 : share < MANPOWER.warn[0] ? 1 : 0;
}

/** A nation's reserve of men, for a panel. Null outside the grand campaign. */
export function manpower(g: Game, nat: string | null = g.human, callUp = 1): MenView | null {
  if (!nat || !menRules(g) || !g.nations[nat]) return null;
  const divs = menLeft(g, nat), start = poolAtStart(g, nat), cls = classNow(g, nat, callUp), home = homeShare(g, nat);
  const level = menLevel(g, nat);
  const months = cls > 0 ? Math.ceil(3 / cls) : 0;
  const gap = Math.max(0, classGapUntil(g, nat) - g.turn), drawnDivs = g.flags[kx(nat)] ?? 0;
  // (Men at the nation's own scale where the rows lay one, China's; every other nation's at MEN_PER_DIV, as before.)
  const per = menPer(nat);
  // (A nation with a scale of its own says each level at that scale, with "about": reserveLevelWords.)
  const line = reserveLevelWords(nat, level, divs, start, cls) ?? (level === 3 ? `The reserve is spent. Only the ${menWords(cls * per)} men who come of age each month reach the armies, and no new corps can be raised${months ? `: a new infantry corps is about ${months === 1 ? 'a month' : `${months} months`} of them` : ''}.`
    : level === 2 ? `The reserve is running short: ${menWords(divs * per)} men are left. Every loss made good and every new corps is taken from it.`
    : level === 1 ? `The reserve is thinning: ${menWords(divs * per)} men are left of ${menWords(start * per)}.`
    : reserveWords(nat, divs) ?? `${menWords(divs * per)} men stand in reserve.`);
  return { nat, divs, men: Math.round(divs * per), start, share: start > 0 ? divs / start : 0, classMen: Math.round(cls * per), home, drawn: Math.round(drawnDivs * per), came: Math.round(Math.max(0, divs - start + drawnDivs) * per), gap, word: WORDS[level], line, per, comes: comesWords(nat, cls) };
}

/**
 * The warning the player is owed this month, if the reserve has fallen past a mark he has not been told of:
 * the level reached (1 to 3), or 0. Telling it once is the caller's business; this notes that it was told.
 */
export function menWarning(g: Game, nat: string): number {
  if (!menRules(g)) return 0;
  const level = menLevel(g, nat), told = g.flags[kl(nat)] ?? 0;
  if (level > told) { g.flags[kl(nat)] = level; return level; }
  // A reserve that has filled again can be warned of again.
  if (level < told) g.flags[kl(nat)] = level;
  return 0;
}
