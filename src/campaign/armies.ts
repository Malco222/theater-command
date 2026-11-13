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
 */
import data from '../../data/campaign/divisions.json';
import { hashString } from '../core/rng';
import { KIND, MEN_PER_DIV, type Corps, type Game, type Kind } from './types';
import { allied, men } from './game';
import { generalOf } from './commanders';
import { grade, STORIES } from './stories';
import { SIGHT, sightOf } from './intel';
import { corpsMood, forceMorale, moraleWord, restWord, type Mood } from './morale';

export const DIVISIONS = data as unknown as {
  kinds: Record<Kind, { word: string; name: string; short: string }>;
  names: Record<string, Partial<Record<Kind, [string, string]>>>;
  order: Kind[];
  spread: { str: number; xp: number };
  hardened: number;
};

// ---- Numbers ----------------------------------------------------------------------------------

const counter = (nat: string, kind: Kind): string => `dn:${nat}:${kind}`;

/** Gives a corps the numbers its divisions will carry: the next ones its nation has free for that arm (the grand campaign). */
export function numberCorps(g: Game, c: Corps): void {
  if (g.flags.open !== 1 || c.dn !== undefined) return;
  const key = counter(c.nat, c.kind);
  c.dn = (g.flags[key] ?? 0) + 1;
  g.flags[key] = c.dn + c.divs - 1;
}

/** Numbers every corps that has no numbers yet, oldest first. Called when a grand campaign opens, and when an older save of one is loaded. */
export function numberDivisions(g: Game): void {
  if (g.flags.open !== 1) return;
  for (const c of [...g.corps].sort((a, b) => a.id - b.id)) numberCorps(g, c);
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

/** The divisions of a corps, each with its share of the corps' strength and experience. Together they add up to exactly the corps. */
export function divisionsOf(g: Game, c: Corps): DivisionCard[] {
  const first = firstNumber(g, c);
  const words = divisionWords(c.nat, c.kind);
  const str = Math.min(1, Math.max(0, c.str)), xp = Math.min(1, Math.max(0, c.xp));
  const ds = characters(c.id, c.divs, 'ds'), dx = characters(c.id, c.divs, 'dx');
  // Each stands a little above or below the corps, within the room the corps' own figure leaves either side of it.
  const roomS = Math.min(str, 1 - str) * DIVISIONS.spread.str, roomX = Math.min(xp, 1 - xp) * DIVISIONS.spread.xp;
  return Array.from({ length: c.divs }, (_, i) => {
    const s = str + ds[i] * roomS, x = xp + dx[i] * roomX;
    const no = first + i;
    return {
      slot: i, no, name: `${ordinal(no)} ${words.name}`, short: `${ordinal(no)} ${words.short}`, kind: c.kind, nat: c.nat,
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
}

export interface ArmyMix { kind: Kind; word: string; divs: number; men: number }

export interface ArmyCard {
  region: number;
  nat: string;
  /** "German army at Paris". */
  name: string;
  /** How much the viewer knows: everything, or only the flag and how many corps stand under it. */
  known: 'full' | 'count';
  corpsCount: number;
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
  corps: CorpsCard[];
}


/** One corps opened up: its record, its mood and its divisions. */
export function corpsCard(g: Game, c: Corps): CorpsCard {
  const battles = c.rec?.battles ?? 0, won = c.rec?.won ?? 0;
  const hardened = c.xp >= DIVISIONS.hardened;
  const service = `${grade(c.xp)}${hardened ? ', battle-hardened' : ''} · ${battles ? `${battles} battle${battles === 1 ? '' : 's'}` : 'untested'}`;
  return {
    id: c.id, nat: c.nat, name: c.name, nick: c.nick ?? null, kind: c.kind, label: KIND[c.kind].label, divs: c.divs, men: men(c),
    str: c.str, xp: c.xp, grade: grade(c.xp), hardened, battles, won, service, general: generalOf(g, c)?.name ?? null,
    mood: corpsMood(g, c), divisions: divisionsOf(g, c),
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
 * One nation's army in a province, as the viewer knows it. The viewer's own armies and their allies' are always
 * known in full; anyone else's as far as the fog allows, and not at all (null) on ground the viewer cannot see.
 */
export function armyCard(g: Game, region: number, nat: string, viewer: string | null = g.human): ArmyCard | null {
  const r = g.regions[region];
  if (!r) return null;
  const cs = r.units.filter((c) => c.nat === nat && !c.dead);
  if (!cs.length) return null;
  const mine = viewer === null || allied(g, nat, viewer);
  const sight = mine ? SIGHT.full : sightOf(g, viewer)[region];
  if (sight === SIGHT.none) return null;
  const name = `${g.nations[nat].adj} army at ${r.name}`;
  if (sight !== SIGHT.full) {
    return { region, nat, name, known: 'count', corpsCount: cs.length, divs: 0, men: 0, str: 0, morale: null, moraleWord: '', rest: 0, restWord: '', mix: [], line: '', corps: [] };
  }
  const last = (c: Corps): number => (c.kind === 'fort' ? 1 : 0);
  const order = [...cs].sort((a, b) => last(a) - last(b) || DIVISIONS.order.indexOf(a.kind) - DIVISIONS.order.indexOf(b.kind) || b.divs * b.str - a.divs * a.str || a.id - b.id);
  const divs = cs.reduce((s, c) => s + c.divs, 0);
  const mix = mixOf(cs);
  const morale = forceMorale(cs);
  const rest = divs > 0 ? cs.reduce((s, c) => s + c.org * c.divs, 0) / divs : 0;
  return {
    region, nat, name, known: 'full', corpsCount: cs.length, divs, men: cs.reduce((s, c) => s + men(c), 0),
    str: divs > 0 ? cs.reduce((s, c) => s + c.divs * c.str, 0) / divs : 0,
    morale, moraleWord: morale === null ? '' : moraleWord(morale), rest, restWord: restWord(rest),
    mix, line: mixLine(mix), corps: order.map((c) => corpsCard(g, c)),
  };
}
