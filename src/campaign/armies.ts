/**
 * An army as the player reads it: what it is made of, division by division.
 *
 * A force in a province is a stack of corps, and each corps is so many divisions of one kind. This module opens the
 * stack up for both interfaces: how many divisions of each arm ("5 infantry, 4 armoured and 2 artillery divisions"),
 * and a card for every division with its number and name, its strength, and how seasoned it is. With it come the
 * corps' morale and rest (see morale.ts), each with the words for why it stands where it does.
 *
 * A corps still fights as one body. Its strength and experience are its own, and the divisions shown inside it always
 * add up to exactly those: each division has a fixed character (drawn from the corps' id, so it is the same in every
 * interface and on every reload), which makes the same division the most worn, or the most seasoned, of its corps all
 * through the war. Nothing here changes a battle.
 *
 * The numbers divisions carry are kept in the grand campaign (the corps remembers the number of its first division,
 * and each nation counts on from the last one it raised). A historical scenario numbers them as they stand.
 *
 * The first day's corps carry their divisions' real numbers of 1 September 1939 where the record has them
 * (setup-1939.json, 'nos'). Every other division is a stand-in, numbered from above the highest real number of its
 * nation's arm (divisions.json, 'record'), so no stand-in ever takes a real number and no country carries one
 * division's name twice (the design director, 18 November 2026).
 *
 * Above that top a stand-in still passes over any number whose name is another division's own (the design director,
 * 12:49Z, 25 November 2026, "B, the skip"): one a dated row of its country gives for that arm, whether or not the row
 * ever lands, or one a living named division of that arm carries. The 15th Panzer Division that lands at Tripoli keeps
 * its name to itself: Germany's stand-ins run 11th, 12th, 13th, 14th, 16th. A named division that dies does not free
 * its number where a row gave it. A war already under way keeps the names its save carries.
 */
import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import data from '../../data/campaign/divisions.json';
import setup from '../../data/campaign/setup-1939.json';
import worldRows from '../../data/campaign/world.json';
import diplomacyRows from '../../data/campaign/diplomacy.json';
import siegeRows from '../../data/campaign/siege.json';
import openRows from '../../data/campaign/open.json';
import pacificRows from '../../data/campaign/pacific.json';
import legionRows from '../../data/campaign/legions.json';
import { hashString } from '../core/rng';
import { FAR_STAGE } from './stage';
import { turnToDate } from '../core/ticket';
import { KIND, MEN_PER_DIV, type Corps, type Game, type Kind } from './types';
import { allied, atWar, men } from './game';
import { generalOf } from './commanders';
import { postWords } from './pacific';
import { grade, STORIES } from './stories';
import { SIGHT, armyBand, corpsKnown, reportSource, sightOf, sortKnown, strengthBand, type Source } from './intel';
import { armyOf } from './joint';
import { townName } from './towns';
import { whyNotTakeField } from './siege';
import { corpsMood, forceMorale, moraleWord, restWord, type Mood } from './morale';
import { withered } from './wither';

export const DIVISIONS = data as unknown as {
  kinds: Record<Kind, { word: string; name: string; short: string }>;
  names: Record<string, Partial<Record<Kind, [string, string]>>>;
  historic: Record<string, Record<string, [string, string][]>>;
  /** The divisions a formation's own name gives where the name alone does not read as its record does: by nation and formation name. */
  reads?: Record<string, Record<string, [string, string][]>>;
  order: Kind[];
  spread: { str: number; xp: number };
  hardened: number;
  record: Record<string, Partial<Record<Kind, number[]>>>;
};

// ---- Numbers ----------------------------------------------------------------------------------

const counter = (nat: string, kind: string): string => `dn:${nat}:${kind}`;

/** The first day's corps whose divisions the record numbers: their real numbers in order, null for a stand-in, by "nation:name". */
const RECORD = new Map<string, (number | null)[]>();
/** The highest real number of each nation's arm, by its counter: stand-ins are numbered from above it. */
const TOP = new Map<string, number>();
const reserve = (nat: string, kind: string, n: number): void => { const k = counter(nat, kind); TOP.set(k, Math.max(TOP.get(k) ?? 0, n)); };
for (const row of (setup as any).corps as any[]) {
  const nos = row[5]?.nos as (number | null)[] | undefined;
  if (!nos) continue;
  RECORD.set(`${row[0]}:${row[1]}`, nos);
  for (const n of nos) if (n) reserve(row[0], row[2], n);
}
for (const [nat, arms] of Object.entries(DIVISIONS.record ?? {})) for (const [kind, ns] of Object.entries(arms)) for (const n of ns ?? []) reserve(nat, kind, n);

/** The numbers a numbered corps' divisions carry, in order. */
const numbersOf = (c: Corps): number[] => c.nos ?? Array.from({ length: c.divs }, (_, i) => c.dn! + i);

/** Lays out a corps' numbers: its first, and the whole list only where they do not run on one from the next. */
export function keepNumbers(c: Corps, list: number[]): void {
  c.dn = list[0];
  if (list.some((n, i) => n !== list[0] + i)) c.nos = list;
  else delete c.nos;
}

// ---- Names that are another division's own (the design director, 12:49Z, 25 November 2026) -------------------

/** What a formation's own name makes of its divisions, kept by "nation:kind:name" (the same few names are asked often). */
const NAMED = new Map<string, [string, string][]>();
/** The same in the grand campaign, where a dated row's name is read by the names table (`reads`) before its own words: a division is laid once, drawn.ts. */
const READ = new Map<string, [string, string][]>();
/**
 * `grand` true in the grand campaign alone: there a row's name is read by the table that gives each of its divisions
 * the one name the country's other rows know it by (divisions.json `reads`). A historical scenario reads every name
 * as it always did, and its cards and battle tickets carry the names they were measured with.
 */
function ownNames(name: string, nat: string, kind: Kind, grand = false): [string, string][] {
  const k = `${nat}:${kind}:${name}`, kept = grand ? READ : NAMED;
  let got = kept.get(k);
  if (!got) kept.set(k, got = DIVISIONS.historic[nat]?.[name] ?? (grand ? DIVISIONS.reads?.[nat]?.[name] : undefined) ?? namedDivisions(name, nat, kind));
  return got;
}

/**
 * The divisions a formation's own name gives in the grand campaign, as its cards show them there (the historian's
 * table first, then the names table, then the name read): [name, short], in order. `grand` false: as a historical
 * scenario reads it.
 */
export const divisionsNamed = (name: string, nat: string, kind: Kind, grand = true): [string, string][] => ownNames(name, nat, kind, grand);

/**
 * The division names the data's rows give, by nation: every row of the campaign's data files that names a formation
 * of a country and an arm, as [nation, name, kind, ...] or dated, [month, nation, name, kind, ...] (the first day's
 * corps, the expeditions, the wider world's and the far side's rows, the garrisons). Read once, when a division is
 * first numbered: by then the far side's rows are laid over the files (far-rows.ts), where its switch is on. A row
 * that never lands keeps its names all the same, and so does one whose division has died.
 */
const ROW_NAMES: { grand: Map<string, Set<string>> | null; history: Map<string, Set<string>> | null } = { grand: null, history: null };
function rowNames(grand: boolean): Map<string, Set<string>> {
  const got = grand ? ROW_NAMES.grand : ROW_NAMES.history;
  if (got) return got;
  const out = new Map<string, Set<string>>();
  const note = (nat: string, name: string, kind: Kind): void => {
    const own = ownNames(name, nat, kind, grand);
    if (!own.length) return;
    let set = out.get(nat);
    if (!set) out.set(nat, set = new Set());
    for (const [full] of own) set.add(full);
  };
  const walk = (x: unknown): void => {
    if (Array.isArray(x)) {
      const at = typeof x[0] === 'number' ? 1 : 0;
      const nat = x[at], name = x[at + 1], kind = x[at + 2];
      if (typeof nat === 'string' && /^[A-Z]{2,3}$/.test(nat) && typeof name === 'string' && typeof kind === 'string' && kind in KIND) note(nat, name, kind as Kind);
      for (const y of x) if (y && typeof y === 'object') walk(y);
    } else if (x && typeof x === 'object') for (const y of Object.values(x)) if (y && typeof y === 'object') walk(y);
  };
  for (const file of [setup, worldRows, diplomacyRows, siegeRows, openRows, pacificRows, legionRows]) walk(file);
  // (The far side's divisions sent west are rows of another shape, [the corps it leaves, how many, the name it goes
  // by]: pacific.json `west`. Each name is its division's own all the same: the 32nd Rifle Division that leaves the
  // Far East for Moscow is no stand-in's number. Read where the far side's switch is on, as its other rows are.)
  if (FAR_STAGE) {
    const west = (pacificRows as unknown as { west?: { nat: string; waves?: { rows: [string, number, string][] }[] } }).west;
    for (const w of west?.waves ?? []) for (const row of w.rows) note(west!.nat, row[2], 'inf');
  }
  if (grand) ROW_NAMES.grand = out; else ROW_NAMES.history = out;
  return out;
}

/**
 * The next stand-in number above `from` for a nation's arm whose name is no other division's own: none a row of the
 * country gives, and none a living named division of the country carries (a corps broken up by hand keeps its
 * divisions' names on itself, and they count). The living are read once for each corps numbered. `own` a corps that is
 * being numbered now, with the numbers it keeps: the places it has yet to fill hold no number of anybody's.
 */
function standIns(g: Game, nat: string, kind: Kind, numbers = true, own?: { c: Corps; nos: number[] }): (from: number) => number {
  const words = divisionWords(nat, kind).name;
  const grand = g.flags.open === 1;
  const rows = rowNames(grand).get(nat);
  let living: Set<string> | null = null;
  const taken = (name: string): boolean => {
    if (rows?.has(name)) return true;
    if (!living) {
      living = new Set();
      for (const o of g.corps) if (o.nat === nat && !o.dead) {
        for (const [full] of o.names ?? ownNames(o.name, o.nat, o.kind, grand)) living.add(full);
        // (`numbers`: nor any number a living division of the arm carries. A nation's own count never comes back to
        // one; a corps that came under its flag from another country's army may carry one the count has yet to reach.)
        if (numbers && o.kind === kind && o.dn !== undefined) for (const n of own?.c === o ? own.nos : numbersOf(o)) living.add(`${ordinal(n)} ${words}`);
      }
    }
    return living.has(name);
  };
  return (from) => {
    let n = from + 1;
    while (taken(`${ordinal(n)} ${words}`)) n++;
    return n;
  };
}

/** True where a stand-in of this number would carry a name that is another division's own: a row's, or a living named division's. */
export function numberSpokenFor(g: Game, nat: string, kind: Kind, n: number): boolean {
  return standIns(g, nat, kind, false)(n - 1) !== n;
}

/**
 * Gives a corps the numbers its divisions will carry (the grand campaign): the record's real ones where it has them
 * and no division of the nation's arm carries them already, and otherwise the next stand-ins its nation has free.
 */
export function numberCorps(g: Game, c: Corps, real?: (number | null)[]): void {
  if (g.flags.open !== 1 || c.dn !== undefined) return;
  const key = counter(c.nat, c.kind);
  let next = Math.max(g.flags[key] ?? 0, TOP.get(key) ?? 0);
  const held = new Set<number>();
  if (real) for (const o of g.corps) if (o !== c && !o.dead && o.nat === c.nat && o.kind === c.kind && o.dn !== undefined) for (const n of numbersOf(o)) held.add(n);
  const free = standIns(g, c.nat, c.kind);
  const list: number[] = [];
  for (let i = 0; i < c.divs; i++) {
    const n = real?.[i];
    if (n && !held.has(n)) { held.add(n); list.push(n); } else list.push(next = free(next));
  }
  if (next > (g.flags[key] ?? 0)) g.flags[key] = next;
  keepNumbers(c, list);
}

/**
 * Numbers every corps that has no numbers yet, oldest first; a first-day corps the record holds takes its real
 * numbers. Called when a grand campaign opens, and when an older save of one is loaded.
 */
export function numberDivisions(g: Game): void {
  if (g.flags.open !== 1) return;
  for (const c of [...g.corps].sort((a, b) => a.id - b.id)) numberCorps(g, c, RECORD.get(`${c.nat}:${c.name}`));
}

/**
 * A corps that takes more divisions in (an expedition row's 'joins'): the new ones carry its nation's next free
 * numbers, never those of a corps beside it; one that changes arm takes the new arm's numbers for all of them.
 */
export function growCorps(g: Game, c: Corps, divs: number, kind: Kind): void {
  const was = c.dn !== undefined && g.flags.open === 1 ? numbersOf(c) : null;
  const same = kind === c.kind;
  c.divs += divs;
  c.kind = kind;
  if (!was) return;
  const key = counter(c.nat, kind);
  let next = Math.max(g.flags[key] ?? 0, TOP.get(key) ?? 0);
  // (It has grown already: the numbers it keeps count, the places it has yet to fill and an arm it has left do not.)
  const list = same ? was : [];
  const free = standIns(g, c.nat, kind, true, { c, nos: [...list] });
  while (list.length < c.divs) list.push(next = free(next));
  g.flags[key] = next;
  keepNumbers(c, list);
}

/**
 * A corps that has passed under another flag (a fallen country's garrison taken into keeping on the far side, the
 * garrison of ground that was demanded, France's corps overseas under Vichy and back): each of its divisions that
 * reads by its number keeps that number where it is no other division's name under the new flag, a row's or a living
 * division's, and takes its new nation's next stand-in where it is. Called once the corps carries its new nation.
 * The Netherlands Indies' garrisons were numbered 4th to 11th in their own army, and so were Britain's own: in
 * keeping they read 19th and on. A division with a name of its own keeps it. The grand campaign only.
 */
export function numbersUnderFlag(g: Game, c: Corps): void {
  if (g.flags.open !== 1 || c.dn === undefined) return;
  const named = c.names ?? ownNames(c.name, c.nat, c.kind, true);
  const words = divisionWords(c.nat, c.kind).name;
  const rows = rowNames(true).get(c.nat);
  let held: Set<string> | null = null;
  const spoken = (n: number): boolean => {
    const name = `${ordinal(n)} ${words}`;
    if (rows?.has(name)) return true;
    if (!held) {
      held = new Set();
      for (const o of g.corps) if (o !== c && o.nat === c.nat && !o.dead) for (const d of divisionsOf(g, o)) held.add(d.name);
    }
    return held.has(name);
  };
  const was = numbersOf(c);
  const moves = was.map((n, i) => !named[i] && spoken(n));
  if (!moves.some(Boolean)) return;
  const key = counter(c.nat, c.kind);
  let next = Math.max(g.flags[key] ?? 0, TOP.get(key) ?? 0);
  const free = standIns(g, c.nat, c.kind);
  keepNumbers(c, was.map((n, i) => (moves[i] ? (next = free(next)) : n)));
  g.flags[key] = next;
}

/**
 * The next stand-in numbers of a nation's arm, for divisions raised new into a corps that already stands (a division
 * is laid once, drawn.ts): its nation counts on from them, as for a corps raised whole. None outside the grand campaign.
 */
export function standInNumbers(g: Game, nat: string, kind: Kind, count: number): number[] {
  if (g.flags.open !== 1 || count <= 0) return [];
  const key = counter(nat, kind);
  let next = Math.max(g.flags[key] ?? 0, TOP.get(key) ?? 0);
  const free = standIns(g, nat, kind);
  const list: number[] = [];
  while (list.length < count) list.push(next = free(next));
  g.flags[key] = next;
  return list;
}

/** A nation's count of an arm's stand-ins as it stands, and that count put back: for a corps laid with a place kept open that no new division fills (drawn.ts). */
export const standInCount = (g: Game, nat: string, kind: Kind): number | undefined => g.flags[counter(nat, kind)];
export function setStandInCount(g: Game, nat: string, kind: Kind, n: number | undefined): void {
  if (n === undefined) delete g.flags[counter(nat, kind)]; else g.flags[counter(nat, kind)] = n;
}

/** The number a corps' first division carries. Where none is kept, the divisions are numbered as the armies stand. */
function firstNumber(g: Game, c: Corps): number {
  if (c.dn !== undefined) return c.dn;
  let n = 1;
  for (const o of g.corps) if (o.nat === c.nat && o.kind === c.kind && o.id < c.id && !o.dead) n += o.divs;
  return n;
}

export function ordinal(n: number): string {
  const t = n % 100;
  if (t >= 11 && t <= 13) return `${n}th`;
  return `${n}${['th', 'st', 'nd', 'rd'][n % 10] ?? 'th'}`;
}

/** What a nation calls a division of a kind: the full name and the short one for a card. */
export function divisionWords(nat: string, kind: Kind): { name: string; short: string } {
  const own = DIVISIONS.names[nat]?.[kind];
  return own ? { name: own[0], short: own[1] } : { name: DIVISIONS.kinds[kind].name, short: DIVISIONS.kinds[kind].short };
}

// ---- Divisions --------------------------------------------------------------------------------

export interface DivisionCard {
  /** Its place in the corps, from 0. */
  slot: number;
  no: number;
  /** "7th Panzer Division". */
  name: string;
  /** "7th Panzer": for a card. */
  short: string;
  kind: Kind;
  nat: string;
  /** 0 to 1: the men and equipment it still has. */
  str: number;
  men: number;
  /** 0 to 1: its experience, with the word for it and how many stripes that word is worth (0 to 4). */
  xp: number;
  grade: string;
  stripes: number;
  /** True for a division seasoned enough to count as battle-hardened. */
  hardened: boolean;
}

/** The fixed character of each division of a corps: one number from -1 to 1 for each, adding up to nothing. */
function characters(id: number, n: number, salt: string): number[] {
  if (n <= 1) return [0];
  const raw = Array.from({ length: n }, (_, i) => (hashString(`${salt}:${id}:${i}`) % 2001) / 1000 - 1);
  const mean = raw.reduce((s, x) => s + x, 0) / n;
  const off = raw.map((x) => x - mean);
  const top = Math.max(...off.map(Math.abs));
  return top > 0 ? off.map((x) => x / top) : off;
}

const stripesOf = (xp: number): number => {
  let k = 0;
  STORIES.grades.forEach(([from], i) => { if (xp >= from) k = i; });
  return k;
};

const BARE = /^\d+(?:st|nd|rd|th)$/;

/**
 * The divisions a corps' own name gives, in its order, as [name, short] (the design director, b464(3)): "7th Armoured
 * Division" is that one division, and "Fifth Army: VI Corps (36th and 34th Infantry Divisions)" those two. A name that
 * names no division, or a corps or a group beside its divisions, gives none. A bare number takes the words after the
 * next one only in a pair ("5th and 11th Panzer Divisions") or where they are the corps' own arm; else it is just a
 * division ("50th, 49th and 7th Armoured Divisions": the 50th and the 49th are not armour).
 */
export function namedDivisions(name: string, nat: string, kind: Kind): [string, string][] {
  const inner = /\(([^()]*Division[^()]*)\)/.exec(name)?.[1];
  const text = (inner ?? name.replace(/\s*\([^()]*\)/g, '')).replace(/^[^:]*:\s*/, '').replace(/\s+and\s+(?:the\s+)?[^,]*\b(?:Brigade|garrison|Guard)$/, '').trim();
  if (/\b(?:Corps|Brigade|Group|garrison|Command|Force|Guard)\b/.test(text)) return [];
  if (/^Division\s/.test(text) && !/\sand\s/.test(text)) return [[text, text]];
  const list = /^(.+?)\s+Divisions?$/.exec(text)?.[1];
  if (!list) return [];
  const items = list.split(/\s*,\s*(?:and\s+)?|\s+and\s+/).filter(Boolean);
  const arm = divisionWords(nat, kind).short;
  return items.map((it, i) => {
    let full = it;
    const next = items.slice(i + 1).find((x) => !BARE.test(x));
    const words = BARE.test(it) && next ? /^\d+(?:st|nd|rd|th)\s+(.+)$/.exec(next)?.[1] : undefined;
    if (words && (items.length === 2 || words === arm || words.endsWith(` ${arm}`))) full = `${it} ${words}`;
    // The short name drops "Division" where what is left still reads as a division: "7th Armoured", never "78th" or "3rd Army".
    return [`${full} Division`, BARE.test(full) || /\bArmy$/.test(full) ? `${full} Division` : full];
  });
}

/** The divisions of a corps, each with its share of the corps' strength and experience. Together they add up to exactly the corps. */
export function divisionsOf(g: Game, c: Corps): DivisionCard[] {
  const first = firstNumber(g, c);
  const words = divisionWords(c.nat, c.kind);
  const str = Math.min(1, Math.max(0, c.str)), xp = Math.min(1, Math.max(0, c.xp));
  const ds = characters(c.id, c.divs, 'ds'), dx = characters(c.id, c.divs, 'dx');
  // Each stands a little above or below the corps, within the room the corps' own figure leaves either side of it.
  const roomS = Math.min(str, 1 - str) * DIVISIONS.spread.str, roomX = Math.min(xp, 1 - xp) * DIVISIONS.spread.xp;
  const named = c.names ?? ownNames(c.name, c.nat, c.kind, g.flags.open === 1);
  return Array.from({ length: c.divs }, (_, i) => {
    const s = str + ds[i] * roomS, x = xp + dx[i] * roomX;
    const no = c.nos?.[i] ?? first + i;
    // The 1939 order of battle's own divisions, where the historian has them, or those the corps' own name gives
    // (b464(3)); else the nation's number for its kind. The number stays the nation's count either way.
    const own = named[i];
    return {
      slot: i, no, name: own?.[0] ?? `${ordinal(no)} ${words.name}`, short: own?.[1] ?? `${ordinal(no)} ${words.short}`, kind: c.kind, nat: c.nat,
      str: s, men: Math.round(s * MEN_PER_DIV), xp: x, grade: grade(x), stripes: stripesOf(x), hardened: x >= DIVISIONS.hardened,
    };
  });
}

// ---- Corps and armies ---------------------------------------------------------------------------

export interface CorpsCard {
  id: number;
  nat: string;
  name: string;
  nick: string | null;
  kind: Kind;
  /** "Infantry", "Armor": the arm, as the rest of the game calls it. */
  label: string;
  divs: number;
  men: number;
  str: number;
  xp: number;
  grade: string;
  /** True for a corps seasoned enough to count as battle-hardened, and the battles it has fought and won. */
  hardened: boolean;
  battles: number;
  won: number;
  /** Its experience and its battles in a few words: "Veteran, battle-hardened · 6 battles". */
  service: string;
  general: string | null;
  /** Morale and rest, 0 to 1, each with its word and a sentence on why. */
  mood: Mood;
  divisions: DivisionCard[];
  /** Why it takes no orders, in the rules' sentence (ruling 176(c)); null where it takes them, and for a corps that is not the player's own. */
  noOrders: string | null;
  /**
   * How the viewer knows it. 'exact': everything above is true today. 'report': a dated report gone stale, 'age' months
   * old. Its kind, divisions and experience stand; 'str' and 'men' are the middle of an estimate that runs from 'strLo'
   * to 'strHi'; its morale and rest are not known (morale null, rest 0, the words say so) and its divisions are not listed.
   */
  known: 'exact' | 'report';
  age: number;
  /** How a foreign corps was last identified: in battle, by an agent, or from the air. Null for the viewer's own side, and where there is no fog. */
  source: Source | null;
  strLo: number;
  strHi: number;
  /** The nation whose army it marches with, where that is not its own: an ally's corps in a joint army. Null otherwise. */
  joint: string | null;
}

export interface ArmyMix { kind: Kind; word: string; divs: number; men: number }

export interface ArmyCard {
  region: number;
  nat: string;
  /** "German army at Paris". */
  name: string;
  /**
   * How much the viewer knows. 'full': every corps is identified and what follows is true today. 'partial': the army is
   * in view, but some corps are not identified or are known only from a stale report; 'corps' lists the identified ones,
   * 'unidentified' counts the rest, and the figures are of the identified corps alone. 'count': only the flag and how
   * many corps stand under it.
   */
  known: 'full' | 'partial' | 'count';
  corpsCount: number;
  /** Corps under the flag that the viewer has not identified: their kind and strength are not known. */
  unidentified: number;
  /** The staff's estimate of the whole army's strength, counted in full divisions, as a band, where it has one; and the age in months of the oldest report used. */
  est: { lo: number; hi: number } | null;
  oldest: number;
  /** The rest is empty where only the count is known. */
  divs: number;
  men: number;
  /** The army's strength as a share of its full establishment, its morale (null where none is kept) and its rest. */
  str: number;
  morale: number | null;
  moraleWord: string;
  rest: number;
  restWord: string;
  /** The arms it is made of, in the order they are listed. */
  mix: ArmyMix[];
  /** "5 infantry, 4 armoured and 2 artillery divisions". */
  line: string;
  /** The allies whose corps march in this army beside its own (a joint army), most divisions first. Empty for an army of one nation. */
  allies: string[];
  corps: CorpsCard[];
}


/** One corps opened up: its record, its mood and its divisions. */
/** The sentence for fortress troops, wherever a unit that takes no orders says why (ruling 176(c)). */
export const FORT_NO_ORDERS = 'Fortress troops man their works and do not move.';
/**
 * Why a corps of the player's own takes no orders, in the rules' sentence, for its card and for the first time it
 * is picked up (ruling 176(c), on Brian's words of 2026-11-17: "there are troops you start with in panama but you
 * cant move them"). Null where it takes orders, and for a corps that is not his.
 */
export function whyNotOrder(g: Game, c: Corps): string | null {
  if (c.dead || g.human === null || c.nat !== g.human) return null;
  // (The far side: Vichy's corps that kept their posts in Indochina on the demand answer to their own officers, pacific.ts atItsPost.)
  const post = postWords(c, 'card');
  if (post) return post;
  if (c.kind === 'fort') return FORT_NO_ORDERS;
  if (c.leave !== undefined) return `It is at home, out of the line, and takes no orders until ${turnToDate(c.leave).label}.`;
  if (c.school) return 'It is at the battle schools this month, and takes no orders until it is back. Call it back to march or fight now; it will have had no term.';
  // (Ruling 173(e), the whole-world chart: a withered island garrison only defends.)
  if (withered(g, c)) return 'Cut off and out of stores, it holds where it stands. It cannot attack, march or sail until stores reach it by sea.';
  if (c.town !== undefined) return garrisonLine(g, [c]);
  return null;
}
/**
 * The sentence for corps of one side that are inside a town, the same in both games, wherever they are asked to march
 * or to attack (Version 84.2.4, on Brian's words of 2026-11-17: "if an enemy army is out of a city i couldnt attack
 * them"). A garrison takes no marching orders, and the sentence names the cure: TAKE THE FIELD, or a sortie where an
 * enemy army stands round the town. One corps is named; several are "They". The corps are of one province.
 * (The 1.5.3 patch; the 1.5.1 playtest's find 18.) Corps that all went in this month come out with no move
 * (apOutOfTown), so taking the field is no way to march: they are told when they can. (The 1.5.3 review.) On the eve
 * of war no month passes, and a corps brought out then has no move until the war opens, when it has its whole month
 * as every corps in the field has: Warsaw's Defence Command is told to take the field now, and that it marches when
 * the war opens. And where the field outside has no room for another corps, TAKE THE FIELD is refused: the sentence
 * gives the rules' reason (whyNotTakeField), not that order.
 */
export function garrisonLine(g: Game, cs: Corps[]): string {
  const r = g.regions[cs[0].reg], town = townName(r), one = cs.length === 1;
  const who = one ? `${cs[0].name} is ${(r.garr?.length ?? 0) > 1 ? 'in ' : ''}the garrison of ${town}` : `They are the garrison of ${town}`;
  const shut = g.flags.sieges === 1 && r.units.some((u) => !u.dead && atWar(g, u.nat, r.ctrl));
  if (shut) return `${who}, shut in by the army round it. Order a sortie against that army.`;
  // (room outside as siege.ts whyNotTakeField counts it, and its sentence: asked of it, so that release 2's room, an
  // island's and the fortress rows' that take none, is counted as TAKE THE FIELD counts it. With those off, as before.)
  const full = whyNotTakeField(g, cs[0]);
  if (full?.startsWith('There is no room in the field')) return `${who}. ${full}`;
  if (cs.every((c) => c.town === g.turn)) return g.flags.eve === 1 ? `${who}. Order ${one ? 'it' : 'them'} to take the field now, and ${one ? 'it' : 'they'} can march when the war opens.` : `${who}. ${one ? 'It' : 'They'} went in this month, and can march out next month.`;
  return `${who}. Order ${one ? 'it' : 'them'} to take the field first.`;
}
/** Why the player's corps inside the town of a province take no marching orders, in the rules' sentence; null where he has none inside. */
export function whyNotOrderGarrison(g: Game, region: number): string | null {
  const r = g.regions[region];
  const cs = g.human === null || !r ? [] : (r.garr ?? []).filter((c) => c.nat === g.human && !c.dead);
  return cs.length ? garrisonLine(g, cs) : null;
}

export function corpsCard(g: Game, c: Corps): CorpsCard {
  const battles = c.rec?.battles ?? 0, won = c.rec?.won ?? 0;
  const hardened = c.xp >= DIVISIONS.hardened;
  const service = `${grade(c.xp)}${hardened ? ', battle-hardened' : ''} · ${battles ? `${battles} battle${battles === 1 ? '' : 's'}` : 'untested'}`;
  return {
    id: c.id, nat: c.nat, name: c.name, nick: c.nick ?? null, kind: c.kind, label: KIND[c.kind].label, divs: c.divs, men: men(c),
    str: c.str, xp: c.xp, grade: grade(c.xp), hardened, battles, won, service, general: generalOf(g, c)?.name ?? null,
    mood: corpsMood(g, c), divisions: divisionsOf(g, c), known: 'exact', age: 0, source: null, strLo: c.str, strHi: c.str, joint: c.joint ?? null,
    noOrders: whyNotOrder(g, c),
  };
}

const listed = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);

/** What a set of corps is made of, arm by arm. */
export function mixOf(cs: Corps[]): ArmyMix[] {
  const out: ArmyMix[] = [];
  for (const kind of DIVISIONS.order) {
    const of = cs.filter((c) => c.kind === kind);
    if (!of.length) continue;
    out.push({ kind, word: DIVISIONS.kinds[kind].word, divs: of.reduce((s, c) => s + c.divs, 0), men: of.reduce((s, c) => s + men(c), 0) });
  }
  return out;
}

/** "5 infantry, 4 armoured and 2 artillery divisions". */
export function mixLine(mix: ArmyMix[]): string {
  if (!mix.length) return 'No troops';
  const total = mix.reduce((s, m) => s + m.divs, 0);
  const parts = mix.map((m) => `${m.divs} ${m.word}`);
  // An engineer formation is a brigade, not a division: an army of nothing else is counted in brigades.
  const unit = mix.every((m) => m.kind === 'eng') ? (total === 1 ? 'brigade' : 'brigades') : total === 1 ? 'division' : 'divisions';
  return `${listed(parts)} ${unit}`;
}

/**
 * A corps as the viewer knows it: as it is where it is identified this month, a stale report where it was identified
 * some months ago, and null where it is not identified at all. The viewer's own corps and its allies' are always known.
 */
export function corpsSeen(g: Game, c: Corps, viewer: string | null = g.human): CorpsCard | null {
  const k = corpsKnown(g, c, viewer);
  if (k.level === 'none') return null;
  const card = { ...corpsCard(g, c), source: reportSource(g, c, viewer) };
  if (k.level === 'exact') return card;
  const band = strengthBand(g, c, k.age);
  const mid = (band.lo + band.hi) / 2;
  const since = `Not known since the report of ${k.age === 1 ? 'last month' : `${k.age} months ago`}.`;
  return {
    ...card, known: 'report', age: k.age, str: mid, strLo: band.lo, strHi: band.hi, men: Math.round(c.divs * mid * MEN_PER_DIV), divisions: [],
    mood: { morale: null, moraleWord: 'Not known', trend: 'steady', rest: 0, restWord: 'Not known', line: since, restLine: since, why: [], resting: false },
  };
}

/** The other nations whose corps march in an army, most divisions first. */
function alliesIn(cs: Corps[], nat: string): string[] {
  const by = new Map<string, number>();
  for (const c of cs) if (c.nat !== nat) by.set(c.nat, (by.get(c.nat) ?? 0) + c.divs);
  return [...by].sort((a, b) => b[1] - a[1] || (a[0] < b[0] ? -1 : 1)).map(([n]) => n);
}

/**
 * One nation's army in a province, as the viewer knows it. The viewer's own armies and their allies' are always
 * known in full. Anyone else's is a flag and a count until its corps are identified: in battle, by an agent, or
 * from the air. On ground the viewer cannot see there is nothing (null).
 */
export function armyCard(g: Game, region: number, nat: string, viewer: string | null = g.human, town = false, seen?: Uint8Array): ArmyCard | null {
  const r = g.regions[region];
  if (!r) return null;
  // An army is every corps that marches under its flag: the nation's own, and an ally's that have joined it.
  // With `town`, the card is of the garrison inside the province's town, not of the army in the field (the grand campaign).
  const cs = (town ? r.garr ?? [] : r.units).filter((c) => armyOf(c) === nat && !c.dead);
  if (!cs.length) return null;
  const allies = alliesIn(cs, nat);
  const mine = viewer === null || allied(g, nat, viewer);
  // (`seen`: the viewer's sightOf, worked out already by a caller that asks of many armies while nothing moves.)
  const sight = mine ? SIGHT.full : (seen ?? sightOf(g, viewer))[region];
  if (sight === SIGHT.none) return null;
  const name = town ? `${g.nations[nat].adj} garrison of ${townName(r)}` : `${g.nations[nat].adj} army at ${r.name}`;
  const last = (c: Corps): number => (c.kind === 'fort' ? 1 : 0);
  const sorted = (xs: Corps[]): Corps[] => [...xs].sort((a, b) => last(a) - last(b) || DIVISIONS.order.indexOf(a.kind) - DIVISIONS.order.indexOf(b.kind) || b.divs * b.str - a.divs * a.str || a.id - b.id);
  const k = mine ? null : sortKnown(g, cs, viewer);
  if (k && (k.report.length || k.none.length)) {
    const seen = sorted([...k.exact, ...k.report]);
    if (!seen.length && sight !== SIGHT.full) {
      return { region, nat, name, known: 'count', corpsCount: cs.length, unidentified: cs.length, est: null, oldest: 0, divs: 0, men: 0, str: 0, morale: null, moraleWord: '', rest: 0, restWord: '', mix: [], line: '', allies: [], corps: [] };
    }
    // Part of the army is known: the identified corps as cards, the rest as a count, and the staff's estimate of the whole.
    const cards = seen.map((c) => corpsSeen(g, c, viewer)!);
    const dv = cards.reduce((s, c) => s + c.divs, 0);
    const mix = mixOf(seen);
    const est = armyBand(g, region, nat, cs, viewer, sight === SIGHT.full);
    const words = [seen.length ? `${mixLine(mix)} identified` : '', k.none.length ? `${k.none.length} corps not identified` : ''].filter(Boolean).join(', ');
    return {
      region, nat, name, known: 'partial', corpsCount: cs.length, unidentified: k.none.length, est, oldest: cards.reduce((m, c) => Math.max(m, c.age), 0),
      divs: dv, men: cards.reduce((s, c) => s + c.men, 0), str: dv > 0 ? cards.reduce((s, c) => s + c.divs * c.str, 0) / dv : 0,
      morale: null, moraleWord: '', rest: 0, restWord: '', mix, line: `${words[0].toUpperCase()}${words.slice(1)}`, allies: [], corps: cards,
    };
  }
  const order = sorted(cs);
  const divs = cs.reduce((s, c) => s + c.divs, 0);
  const mix = mixOf(cs);
  const morale = forceMorale(cs);
  const rest = divs > 0 ? cs.reduce((s, c) => s + c.org * c.divs, 0) / divs : 0;
  return {
    region, nat, name, known: 'full', corpsCount: cs.length, unidentified: 0, est: { lo: cs.reduce((s, c) => s + c.divs * c.str, 0), hi: cs.reduce((s, c) => s + c.divs * c.str, 0) }, oldest: 0,
    divs, men: cs.reduce((s, c) => s + men(c), 0),
    str: divs > 0 ? cs.reduce((s, c) => s + c.divs * c.str, 0) / divs : 0,
    morale, moraleWord: morale === null ? '' : moraleWord(morale), rest, restWord: restWord(rest),
    mix, line: mixLine(mix), allies, corps: order.map((c) => (mine ? null : corpsSeen(g, c, viewer)) ?? corpsCard(g, c)),
  };
}
