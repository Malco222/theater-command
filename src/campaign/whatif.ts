// The sandbox (Brian, 19 November 2026: "i want this to be a sandbox to play and make your own history, not just
// play out the real one"). The grand campaign only. Two parts, both in data/campaign/whatif.json.
//
// What-if starts: how the war begins. Five swing powers (the Soviet Union, Italy, the United States, Spain and
// Turkey) each have sides; a start is a set of them. History's side is stored as nothing, so a campaign begun with
// "The war that was" and the turning points off is the game exactly as it was, flag for flag and seed for seed.
//
// Turning points: moments where the war can go another way (Britain alone in 1940, Spain at Hendaye, Molotov in
// Berlin, Germany and America, Stalin's peace feeler, the generals' plot and their follow-ups). Each is decided by
// the player when he leads the country that decides it, and otherwise by the computer on weights the history dial
// sets: by the book always history's choice, at the middle stop now and then another, at the loose stops often.
// A choice can raise a flag that opens the next turning point, so the war branches.
//
// Everything is kept in g.flags, so a campaign saves and loads as it always has:
//   wi                 1 while the sandbox is on in this campaign
//   wiStart            the start's number ('n' in the data; 0 for history's)
//   wiTurns            1 while the turning points come
//   wi:side:NAT        the country's side, 1-based in its list (absent: history's)
//   wi:bar:KEY         a great choice ('NAT:op'), an event ('ev:id') or a deal ('deal:id') that does not happen
//   wi:nopact          the Red Army does not come into Poland, and its zone is not handed over
//   wi:f:NAME          a flag a choice raised, with its month
//   tp:ID              the choice made at a turning point, 1-based; tp:ID:at its month; tp:ID:by who made it
//   tp:ID:ask          the month the player was asked and has not yet answered; tp:ID:next the month it comes again

import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import data from '../../data/campaign/whatif.json';
import worldData from '../../data/campaign/world.json';
import { hashString, Rng } from '../core/rng';
import type { Align, Game, Nation } from './types';
import { allied, atWar, bloc, declareWar, headline, setWar, theName, warning } from './game';
import { fireRow, holds, signPeace, type DiploEvent } from './diplomacy';
import { stopOf } from './history';
import { siegeSweep } from './siege';

// ---- The data ----------------------------------------------------------------------------------------------

/** What a choice does. */
export interface Do {
  join?: Record<string, 'allies' | 'axis'>;
  leave?: string[];
  war?: [string, string][];
  peace?: [string, string][];
  truce?: [string, string][];
  armistice?: { with: string; except?: string[] };
  /** The dominions of the first of each pair do as it does. */
  dominions?: boolean;
  /** The allies of the first of each pair that are at war with the second do as it does. */
  partners?: boolean;
  opinion?: Record<string, number>;
  bar?: string[];
  unbar?: string[];
  quality?: Record<string, number>;
  wake?: string[];
  flag?: string[];
}

export interface Side {
  id: string; name: string; says: string;
  align?: Align; opinion?: number; war?: [string, string][]; wake?: boolean; nopact?: boolean;
  bar?: string[]; follow?: DiploEvent[];
  /** A line of the eve-of-war card that reads otherwise in this world: [its place, the words]. */
  eve?: Record<string, [number, string]>;
}
export interface Country { id: string; name: string; sides: Side[] }
export interface Start { id: string; n: number; name: string; tag: string; blurb: string; sides: Record<string, string>; dealt?: boolean; custom?: boolean }
type Words = string | Record<string, string>;
export interface Option {
  id: string; hist?: boolean; again?: boolean;
  label: Words; says: Words; news: string;
  /** The computer's weight at each stop of the dial, 1 to 5. */
  w: number[];
  if?: string[];
  boost?: Record<string, number>;
  do?: Do;
  /** A choice the player makes that needs the other side's yes. */
  reply?: { by: string; w: number[]; no: string };
}
export interface Turn { id: string; name: string; from: number; to?: number; again?: number; by: string[]; if?: string[]; ask: string; options: Option[] }

export const WHATIF = data as unknown as {
  menu: boolean;
  countries: Country[];
  starts: Start[];
  deal: Record<string, number[]>;
  turns: Turn[];
};

const COUNTRY = new Map(WHATIF.countries.map((c) => [c.id, c]));
const TURN = new Map(WHATIF.turns.map((t) => [t.id, t]));
const WORLD = worldData as unknown as { follow?: Record<string, string>; dominions?: Record<string, { of: string[] }> };

// ---- Where it applies ----------------------------------------------------------------------------------------

/** True in a campaign begun with the sandbox on. Nothing here does anything in any other. */
export const sandboxOn = (g: Game): boolean => g.flags.open === 1 && g.flags.wi === 1;

/** True where a great choice, an event or a deal has been taken out of this war ('GER:east', 'ev:baltic', 'deal:pact'). Cheap when the sandbox is off. */
export const wiBarred = (g: Game, key: string): boolean => g.flags.wi === 1 && !!g.flags[`wi:bar:${key}`];

/** True where the Pact of 1939 was never signed in this war: the Red Army stays out of Poland. */
export const noPact = (g: Game): boolean => g.flags.wi === 1 && !!g.flags['wi:nopact'];

/** The side a swing country stands on in this war (history's where none was chosen). */
export function sideOf(g: Game, nat: string): Side | null {
  const c = COUNTRY.get(nat);
  if (!c) return null;
  const k = g.flags.wi === 1 ? g.flags[`wi:side:${nat}`] ?? 0 : 0;
  return c.sides[k] ?? c.sides[0];
}

/** The start this campaign was begun with. */
export const startOf = (g: Game): Start => WHATIF.starts.find((s) => s.n === (g.flags.wi === 1 ? g.flags.wiStart ?? 0 : 0)) ?? WHATIF.starts[0];

/** Why the sandbox cannot be set this way, or null. Only on the first day of a grand campaign: it is how the war begins. */
export function whyNotSandbox(g: Game, start: string, sides: Record<string, string> = {}): string | null {
  if (g.flags.open !== 1) return 'The sandbox belongs to the grand campaign. A historical scenario is always played as history fought it.';
  if (g.turn !== 1 || g.flags.openFrom !== undefined) return 'A world of your own is set on the first day of the war, in September 1939.';
  if (g.flags.wi !== undefined) return 'This war has its world already.';
  const S = WHATIF.starts.find((s) => s.id === start);
  if (!S) return 'There is no such start.';
  for (const [nat, id] of Object.entries(S.custom ? sides : {})) {
    const c = COUNTRY.get(nat);
    if (!c) return `There is no choice of side for ${nat}.`;
    if (!c.sides.some((s) => s.id === id)) return `${c.name} has no side called ${id}.`;
  }
  return null;
}

/** A draw between 0 and 1 from the war's seed and what is being decided, never the battle dice. */
const draw = (g: Game, ...key: (string | number)[]): number => new Rng(hashString(`${g.seed}:wi:${key.join(':')}`)).next();

// ---- How the war begins --------------------------------------------------------------------------------------

/**
 * Sets the world of this war on its first day: the start's sides, and whether turning points come. Called once,
 * after the grand campaign is set up and before anybody's first move. History's start with no turning points
 * stores nothing at all. Returns false if it cannot be set.
 */
export function setSandbox(g: Game, start: string, sides: Record<string, string> = {}, turns = true): boolean {
  if (whyNotSandbox(g, start, sides)) return false;
  const S = WHATIF.starts.find((s) => s.id === start)!;
  const picked: Record<string, string> = S.dealt ? dealt(g) : S.custom ? { ...sides } : { ...S.sides };
  const moved = Object.entries(picked).filter(([nat, id]) => id !== 'history' && COUNTRY.get(nat)?.sides.some((s) => s.id === id));
  if (S.n === 0 && !turns) return true;
  g.flags.wi = 1;
  g.flags.wiStart = S.n;
  if (turns) g.flags.wiTurns = 1;
  for (const [nat, id] of moved) g.flags[`wi:side:${nat}`] = COUNTRY.get(nat)!.sides.findIndex((s) => s.id === id);
  for (const [nat] of moved) applySide(g, nat, sideOf(g, nat)!);
  // One line for the first day: the world this war begins in.
  if (moved.length || S.dealt) {
    const lines = moved.map(([nat]) => sideOf(g, nat)!.says);
    headline(g, 'event', `${S.name}. ${lines.length ? lines.join(' ') : 'The dice have dealt history\'s own world: every country stands where it stood in 1939.'}`, moved[0]?.[0] ?? 'GER');
    g.headlines[g.headlines.length - 1].head = S.name;
  }
  g.supplyDirty = true;
  return true;
}

/** Each swing country's side in "A World Dealt Anew", from the war's seed by the weights in the data. */
function dealt(g: Game): Record<string, string> {
  const out: Record<string, string> = {};
  for (const c of WHATIF.countries) {
    const w = WHATIF.deal[c.id] ?? c.sides.map((_, k) => (k === 0 ? 1 : 0));
    const sum = w.reduce((s, x) => s + x, 0);
    let x = draw(g, 'deal', c.id) * sum, k = 0;
    while (k < w.length - 1 && x >= w[k]) { x -= w[k]; k++; }
    out[c.id] = c.sides[k].id;
  }
  return out;
}

/** Puts a swing country on its side on the first day. The wars it begins with are the world's, told in the start's line. */
function applySide(g: Game, nat: string, s: Side): void {
  const n = g.nations[nat];
  if (!n) return;
  if (s.wake && n.dormant) { n.alive = true; n.dormant = false; }
  if (!n.alive) return;
  if (s.align) setAlignOf(g, n, s.align);
  if (s.opinion !== undefined) { n.opinion = s.opinion; n.home = s.opinion; }
  for (const k of s.bar ?? []) g.flags[`wi:bar:${k}`] = 1;
  if (s.nopact) g.flags['wi:nopact'] = 1;
  for (const [a, b] of s.war ?? []) {
    const A = g.nations[a], B = g.nations[b];
    if (!A?.alive || !B?.alive || atWar(g, a, b)) continue;
    setWar(g, a, b);
    A.offStart[b] = 1;
    g.flags[`war:${a}:${b}`] = 1;
  }
}

/** Puts a country at a place on the alignment track, as a member or co-belligerent stands: its opinion goes with it. */
function setAlignOf(g: Game, n: Nation, a: Align): void {
  n.align = a;
  const s = ['allies', 'allies_cobelligerent', 'leaning_allies', 'independent', 'leaning_axis', 'axis_cobelligerent', 'axis'].indexOf(a) - 3;
  n.opinion = n.home = Math.abs(s) >= 2 ? Math.sign(s) * 100 : s * 40;
  g.flags[`step:${n.id}`] = g.turn;
  g.supplyDirty = true;
}

/** The eve-of-war card's lines for the player's country in this world: history's, with any the world changes. */
export function eveLines(g: Game, nat: string, lines: string[]): string[] {
  if (g.flags.wi !== 1) return lines;
  const out = [...lines];
  for (const c of WHATIF.countries) {
    const row = sideOf(g, c.id)?.eve?.[nat];
    if (row && row[0] < out.length) out[row[0]] = row[1];
  }
  return out;
}

// ---- Conditions ----------------------------------------------------------------------------------------------

/** One condition of a turning point or a start's event: diplomacy.json's, and the sandbox's own. */
export function wiHolds(g: Game, cond: string): boolean {
  if (cond.includes('|')) return cond.split('|').some((c) => wiHolds(g, c));
  const i = cond.indexOf(':');
  const key = cond.slice(0, i), rest = cond.slice(i + 1);
  switch (key) {
    case 'side': { const [nat, id] = rest.split(':'); return sideOf(g, nat)?.id === id; }
    case 'tp': { const [id, opt] = rest.split(':'); const t = TURN.get(id); return !!t && g.flags[`tp:${id}`] === t.options.findIndex((o) => o.id === opt) + 1; }
    case 'tpAgo': { const [id, n] = rest.split(','); const at = g.flags[`tp:${id}:at`]; return at !== undefined && g.turn - at >= Number(n); }
    case 'wi': return g.flags[`wi:f:${rest}`] !== undefined;
    default: return holds(g, cond);
  }
}

// ---- Turning points ------------------------------------------------------------------------------------------

/** Who decides a turning point this month: the player if he leads one of its countries, else the first of them still standing on the computer. */
function deciderOf(g: Game, t: Turn): { nat: string; player: boolean } | null {
  const mine = t.by.find((id) => id === g.human && g.nations[id]?.alive);
  if (mine) return { nat: mine, player: true };
  const first = t.by.find((id) => id !== g.human && g.nations[id]?.alive && !g.nations[id].dormant);
  return first ? { nat: first, player: false } : null;
}

const wordsFor = (w: Words, nat: string): string | null => (typeof w === 'string' ? w : w[nat] ?? null);

/** The choices open to a decider this month: those with words for it whose own conditions hold. */
function optionsFor(g: Game, t: Turn, nat: string): Option[] {
  return t.options.filter((o) => wordsFor(o.label, nat) !== null && (o.if ?? []).every((c) => wiHolds(g, c)));
}

/** True where a turning point comes this month. */
function due(g: Game, t: Turn): boolean {
  if (g.flags[`tp:${t.id}`] !== undefined || g.flags[`tp:${t.id}:ask`] !== undefined) return false;
  if (g.turn < t.from || g.turn > (t.to ?? 999) || g.turn < (g.flags[`tp:${t.id}:next`] ?? 0)) return false;
  return (t.if ?? []).every((c) => wiHolds(g, c));
}

/** The computer's weight for a choice at this campaign's stop of the dial. */
function weightOf(g: Game, o: Option): number {
  let w = o.w[stopOf(g) - 1] ?? 0;
  for (const [c, k] of Object.entries(o.boost ?? {})) if (wiHolds(g, c)) w *= k;
  return Math.max(0, w);
}

/** The computer's choice: by the dial's weights, drawn from the war's seed and the month. History's where nothing else weighs. */
function computerPick(g: Game, t: Turn, list: Option[]): Option {
  const w = list.map((o) => weightOf(g, o));
  const sum = w.reduce((s, x) => s + x, 0);
  if (sum <= 0) return list.find((o) => o.hist) ?? list[0];
  let x = draw(g, 'tp', t.id, g.turn) * sum;
  for (let k = 0; k < list.length; k++) { if (x < w[k]) return list[k]; x -= w[k]; }
  return list[list.length - 1];
}

/** History's choice at a turning point, taken when the player gives no answer. */
const histOf = (t: Turn, list: Option[]): Option => list.find((o) => o.hist) ?? list[0];

/** The countries a pair's 'dominions' and 'partners' bring in beside its first: never the player's, unless his was the choice. */
function beside(g: Game, a: string, b: string, d: Do, mine: boolean): string[] {
  const out = new Set<string>([a]);
  const ok = (x: string): boolean => !!g.nations[x]?.alive && (x !== g.human || mine);
  if (d.dominions) {
    for (const x of WORLD.dominions?.[a]?.of ?? []) if (ok(x)) out.add(x);
    for (const [x, of] of Object.entries(WORLD.follow ?? {})) if (of === a && ok(x)) out.add(x);
  }
  if (d.partners) for (const x of Object.keys(g.nations)) if (x !== a && ok(x) && allied(g, x, a) && atWar(g, x, b)) out.add(x);
  return [...out];
}

/** Two at war stop where they stand: each keeps what its armies hold, as a staff's armistice does. */
function standDown(g: Game, a: string, b: string): boolean {
  const A = g.nations[a], B = g.nations[b];
  if (!A?.alive || !B?.alive || !atWar(g, a, b)) return false;
  A.atWar = A.atWar.filter((x) => x !== b);
  B.atWar = B.atWar.filter((x) => x !== a);
  g.flags[`arm:${a}:${b}`] = g.turn;
  return true;
}

/**
 * Carries out a choice. `mine` is true where the player made it: only then may it take his own country out of a war.
 * Returns true if anything changed.
 */
export function carryOut(g: Game, d: Do, mine = false): boolean {
  let did = false;
  for (const id of d.wake ?? []) { const n = g.nations[id]; if (n?.dormant) { n.alive = true; n.dormant = false; did = true; } }
  for (const [id, side] of Object.entries(d.join ?? {})) {
    const n = g.nations[id];
    if (!n?.alive || n.league) continue;
    setAlignOf(g, n, side);
    did = true;
  }
  for (const id of d.leave ?? []) { const n = g.nations[id]; if (n?.alive && bloc(n)) { setAlignOf(g, n, 'independent'); did = true; } }
  for (const k of d.bar ?? []) { g.flags[`wi:bar:${k}`] = 1; did = true; }
  for (const k of d.unbar ?? []) { if (g.flags[`wi:bar:${k}`]) { delete g.flags[`wi:bar:${k}`]; did = true; } }
  const off = (x: string, y: string): boolean => (x !== g.human && y !== g.human) || mine;
  for (const [a, b] of d.peace ?? []) for (const x of beside(g, a, b, d, mine)) if (off(x, b) && signPeace(g, x, b)) did = true;
  for (const [a, b] of d.truce ?? []) for (const x of beside(g, a, b, d, mine)) if (off(x, b) && standDown(g, x, b)) did = true;
  if (d.armistice) {
    const X = d.armistice.with;
    const foes = [...(g.nations[X]?.atWar ?? [])].filter((y) => !(d.armistice!.except ?? []).includes(y) && (y !== g.human || mine));
    for (const y of foes) for (const x of beside(g, X, y, { partners: d.partners }, mine)) if (standDown(g, x, y)) did = true;
  }
  if (d.peace || d.truce || d.armistice) { siegeSweep(g); g.supplyDirty = true; }
  // The first war is declared whatever stands; the rest only on a country already at war with the declarer's side.
  let first = true;
  for (const [a, b] of d.war ?? []) {
    const A = g.nations[a], B = g.nations[b];
    if (!A?.alive || !B?.alive || A.dormant || B.dormant || atWar(g, a, b)) continue;
    const ok = first || B.atWar.some((x) => x !== a && allied(g, x, a));
    first = false;
    if (!ok) continue;
    for (const x of beside(g, a, b, d, mine || a === g.human)) if (!atWar(g, x, b) && g.nations[x].alive) { declareWar(g, x, b); did = true; }
  }
  for (const [id, k] of Object.entries(d.opinion ?? {})) {
    const n = g.nations[id];
    if (!n?.alive) continue;
    n.opinion = Math.max(-100, Math.min(100, n.opinion + k));
    n.home = Math.max(-100, Math.min(100, n.home + k));
    did = true;
  }
  for (const [id, k] of Object.entries(d.quality ?? {})) { const n = g.nations[id]; if (n?.alive) { n.quality *= k; did = true; } }
  for (const f of d.flag ?? []) { g.flags[`wi:f:${f}`] = g.turn; did = true; }
  return did;
}

/** Who made a choice, as the record keeps it. */
export type MadeBy = 'player' | 'computer' | 'unanswered' | 'refused';
const BY: MadeBy[] = ['player', 'computer', 'unanswered', 'refused'];

/** Takes a choice at a turning point: carries it out, tells it and records it. */
function take(g: Game, t: Turn, o: Option, nat: string, by: MadeBy): void {
  delete g.flags[`tp:${t.id}:ask`];
  const k = t.options.indexOf(o);
  g.flags[`tp:${t.id}:at`] = g.turn;
  g.flags[`tp:${t.id}:by`] = BY.indexOf(by) + 1;
  // The player's offer the other side turns down: its answer is the news, and nothing changes.
  if (by === 'player' && o.reply && o.reply.by !== nat) {
    const yes = draw(g, 'reply', t.id, g.turn) < (o.reply.w[stopOf(g) - 1] ?? 0);
    if (!yes) {
      const fall = histOf(t, t.options);
      g.flags[`tp:${t.id}`] = t.options.indexOf(fall) + 1;
      g.flags[`tp:${t.id}:by`] = BY.indexOf('refused') + 1;
      headline(g, 'event', o.reply.no, o.reply.by);
      return;
    }
  }
  if (o.again && t.again) {
    // Not yet: the turning point comes round again.
    delete g.flags[`tp:${t.id}`];
    g.flags[`tp:${t.id}:next`] = g.turn + t.again;
    g.flags[`tp:${t.id}:waits`] = (g.flags[`tp:${t.id}:waits`] ?? 0) + 1;
  } else g.flags[`tp:${t.id}`] = k + 1;
  if (o.do) carryOut(g, o.do, by === 'player');
  // (A choice whose news is told by the war it declares has no line of its own; a choice that changes nothing says nothing.)
  if (o.news) {
    headline(g, 'event', o.news, nat);
    g.headlines[g.headlines.length - 1].head = t.name;
  }
}

/**
 * The sandbox's month, at its opening: answers the player left unsaid are history's, the start's own events are
 * checked, and the turning points due are decided, by the computer or put to the player.
 */
export function whatIfMonth(g: Game): void {
  if (!sandboxOn(g) || (g.flags.dom ?? 0) > 0) return;
  // A question the player has not answered by the month's end has history's answer.
  for (const t of WHATIF.turns) {
    const asked = g.flags[`tp:${t.id}:ask`];
    if (asked === undefined || asked >= g.turn) continue;
    const who = deciderOf(g, t);
    const list = who ? optionsFor(g, t, who.nat) : [];
    if (!who || !list.length) { delete g.flags[`tp:${t.id}:ask`]; continue; }
    take(g, t, histOf(t, list), who.nat, 'unanswered');
  }
  // The start's own events: a side's follow-ups, in diplomacy.json's shape.
  for (const c of WHATIF.countries) {
    for (const e of sideOf(g, c.id)?.follow ?? []) {
      if (g.flags[`event:${e.id}`] || g.turn < e.from || g.turn > e.to) continue;
      if (!(e.if ?? []).every((x) => wiHolds(g, x))) continue;
      if (fireRow(g, e)) g.flags[`event:${e.id}`] = g.turn;
    }
  }
  if (g.flags.wiTurns !== 1) return;
  for (const t of WHATIF.turns) {
    if (!due(g, t)) continue;
    const who = deciderOf(g, t);
    if (!who) continue;
    const list = optionsFor(g, t, who.nat);
    if (!list.length) continue;
    if (who.player) {
      g.flags[`tp:${t.id}:ask`] = g.turn;
      warning(g, t.name, `${t.ask} Your answer is wanted this month. If you give none, history's choice stands.`, who.nat);
      continue;
    }
    take(g, t, computerPick(g, t, list), who.nat, 'computer');
  }
}

// ---- For both interfaces -------------------------------------------------------------------------------------

export interface SideView { id: string; name: string; says: string }
export interface CountryView { nat: string; name: string; sides: SideView[] }
export interface StartView { id: string; name: string; tag: string; blurb: string; sides: Record<string, string>; dealt: boolean; custom: boolean }

/** True where the new-campaign card offers the sandbox (switched on after release 1). */
export const sandboxMenu = (): boolean => WHATIF.menu;

export const sandboxStarts = (): StartView[] =>
  WHATIF.starts.map((s) => ({ id: s.id, name: s.name, tag: s.tag, blurb: s.blurb, sides: { ...s.sides }, dealt: !!s.dealt, custom: !!s.custom }));
export const sandboxCountries = (): CountryView[] =>
  WHATIF.countries.map((c) => ({ nat: c.id, name: c.name, sides: c.sides.map((s) => ({ id: s.id, name: s.name, says: s.says })) }));

export interface ChoiceView { id: string; label: string; says: string; history: boolean }
export interface TurningPointView { id: string; name: string; ask: string; nat: string; asked: number; choices: ChoiceView[] }

/** The turning points put to the player this month, waiting for his answer. */
export function turningPointsOf(g: Game): TurningPointView[] {
  if (!sandboxOn(g) || !g.human) return [];
  const out: TurningPointView[] = [];
  for (const t of WHATIF.turns) {
    const asked = g.flags[`tp:${t.id}:ask`];
    if (asked === undefined) continue;
    const who = deciderOf(g, t);
    if (!who?.player) continue;
    out.push({
      id: t.id, name: t.name, ask: t.ask, nat: who.nat, asked,
      choices: optionsFor(g, t, who.nat).map((o) => ({ id: o.id, label: wordsFor(o.label, who.nat)!, says: wordsFor(o.says, who.nat) ?? '', history: !!o.hist })),
    });
  }
  return out;
}

/** Why the player cannot give this answer, or null. */
export function whyNotTurningPoint(g: Game, id: string, choice: string): string | null {
  const t = TURN.get(id);
  if (!t || !sandboxOn(g)) return 'There is no such turning point in this war.';
  if (g.flags[`tp:${id}:ask`] === undefined) return 'That question is not open.';
  const who = deciderOf(g, t);
  if (!who?.player) return 'That choice is not yours to make.';
  if (!optionsFor(g, t, who.nat).some((o) => o.id === choice)) return 'That answer is not open to you now.';
  return null;
}

/** The player's answer at a turning point: it is carried out at once. */
export function answerTurningPoint(g: Game, id: string, choice: string): boolean {
  if (whyNotTurningPoint(g, id, choice)) return false;
  const t = TURN.get(id)!;
  const who = deciderOf(g, t)!;
  take(g, t, optionsFor(g, t, who.nat).find((o) => o.id === choice)!, who.nat, 'player');
  return true;
}

export interface TurningRecord { id: string; name: string; turn: number; choice: string; history: boolean; by: MadeBy; waits: number }
/** The war's turning points so far, oldest first: what was chosen, when, and by whom. The campaign's own what-if book. */
export function turningRecord(g: Game): TurningRecord[] {
  if (!sandboxOn(g)) return [];
  const out: TurningRecord[] = [];
  for (const t of WHATIF.turns) {
    const k = g.flags[`tp:${t.id}`];
    if (k === undefined) continue;
    const o = t.options[k - 1];
    const nat = deciderOf(g, t)?.nat ?? t.by[0];
    out.push({ id: t.id, name: t.name, turn: g.flags[`tp:${t.id}:at`] ?? 0, choice: wordsFor(o.label, nat) ?? wordsFor(o.label, t.by[0]) ?? o.id, history: !!o.hist, by: BY[(g.flags[`tp:${t.id}:by`] ?? 2) - 1], waits: g.flags[`tp:${t.id}:waits`] ?? 0 });
  }
  return out.sort((a, b) => a.turn - b.turn);
}

export interface SandboxView { start: StartView; sides: { nat: string; name: string; side: SideView; history: boolean }[]; turns: boolean; record: TurningRecord[]; departures: number }
/** The world this war began in, for the campaign settings and the Powers tab. Null outside the sandbox. */
export function sandboxOf(g: Game): SandboxView | null {
  if (!sandboxOn(g)) return null;
  const S = startOf(g);
  const record = turningRecord(g);
  return {
    start: sandboxStarts().find((s) => s.id === S.id)!,
    sides: WHATIF.countries.map((c) => { const s = sideOf(g, c.id)!; return { nat: c.id, name: c.name, side: { id: s.id, name: s.name, says: s.says }, history: s.id === 'history' }; }),
    turns: g.flags.wiTurns === 1,
    record,
    departures: record.filter((r) => !r.history).length,
  };
}

/** The bare news name of a country for the views: "the Soviet Union". */
export const countryName = (g: Game, nat: string): string => (g.nations[nat] ? theName(g.nations[nat]) : COUNTRY.get(nat)?.name ?? nat);
