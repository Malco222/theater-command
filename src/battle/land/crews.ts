// Tank crews name their tanks (b477, Brian's pick; the design director's rulings of 12:13Z, trimmed for the browser by
// b479; b490: every armoured vehicle with its crew inside, never transport): which of an armoured block's vehicles carry
// a crew's name or the German kill rings, as data. The campaign has one armoured kind, Armor: its block is drawn with
// the tanks, tank destroyers, assault guns or self-propelled guns the 3D unit table gives it, and each takes the entry at
// its place in the block's model order. Lorries, half-tracks and towed guns stand in other kinds and carry nothing.
// The browser draws none of it: the 3D game paints the names and rings where the model has a name place
// (index-land.json crew_mark), and the hover gives the names with their English meaning.
//
// The unit is the corps: every battle it has come through counts, staff-settled or fought by hand (stories.ts
// storyBattle counts both, rec.battles). Names stay with the tank from battle to battle: the same block of the same
// corps names its tanks the same way every time.

import { CREW_NAMES, type CrewName } from './crew-names';
import type { Kind } from './rules';
import type { LandUnit } from './units';

export const CREWS = {
  /** A unit that has come through this many battles is a veteran: its crews have named their tanks. */
  veteran: 3,
  /** Japan, Italy and Poland named few: about one tank in this many of a veteran unit (b477(3)). */
  few: 0.25,
  /** Germany's kill rings: one to five a tank, more with the unit's battles, varying tank to tank (b477(4)). */
  rings: { most: 5, per: 0.5 },
} as const;

/** The Commonwealth's armies name their tanks as Britain's did (the historian). */
const COMMONWEALTH = ['CAN', 'AUS', 'NZL', 'NZ', 'SAF', 'IND'];

/** The crews' own marks on one block's tanks, in its models' order. */
export interface CrewMarks {
  /** 'name' where that army painted names, 'rings' for Germany's kill rings. */
  mark: 'name' | 'rings';
  /** Per tank: its name and English meaning, or null where it carries none (mark 'name'). */
  names?: (CrewName | null)[];
  /** Per tank: its rings, 1 to 5 (mark 'rings'). No count is ever shown on screen. */
  rings?: number[];
}

/** A number from 0 to under 1 that is always the same for the same words (FNV-1a, then mixed so close words land far apart). */
function steady(s: string): number {
  let h = 0x811c9dc5;
  for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 0x01000193); }
  h ^= h >>> 16; h = Math.imul(h, 0x85ebca6b); h ^= h >>> 13; h = Math.imul(h, 0xc2b2ae35); h ^= h >>> 16;
  return (h >>> 0) / 4294967296;
}

/** Which army's way of naming a nation's crews follow, or null for the plain number. */
function army(nat: string): keyof typeof CREW_NAMES | null {
  if (nat === 'VIC' || nat === 'FFR') return 'FRA';
  if (COMMONWEALTH.includes(nat)) return 'UK';
  return nat in CREW_NAMES ? (nat as keyof typeof CREW_NAMES) : null;
}

/**
 * The marks a block's tanks carry: nat is the block's nation, kind its arm, battles the battles its corps has come through
 * before this one, tanks the models it shows, seed what names the block (its corps and short name). Null for a block that
 * is not tanks, or whose crews paint nothing yet.
 */
export function crewMarks(nat: string, kind: Kind, battles: number, tanks: number, seed: string): CrewMarks | null {
  if (kind !== 'arm' || tanks <= 0) return null;
  const a = army(nat);
  if (!a) return null;
  const vet = battles >= CREWS.veteran;
  if (a === 'GER') {
    if (!vet) return null;
    // One ring at the third battle, another every second battle after it, each tank a ring either way of that.
    const base = 1 + (battles - CREWS.veteran) * CREWS.rings.per;
    const rings = Array.from({ length: tanks }, (_, i) => Math.max(1, Math.min(CREWS.rings.most, Math.round(base + (steady(`${seed}#r${i}`) * 3 - 1.5)))));
    return { mark: 'rings', rings };
  }
  // France: every Char B1 carried her name from the start (b477(2)); America, Britain and the Soviet Union from the third
  // battle (1); Japan, Italy and Poland about one in four of a veteran unit (3).
  const all = a === 'FRA' || ((a === 'USA' || a === 'UK' || a === 'SOV') && vet);
  const few = (a === 'JAP' || a === 'ITA' || a === 'POL') && vet;
  if (!all && !few) return null;
  const list = CREW_NAMES[a];
  const start = Math.floor(steady(seed) * list.length);
  const names = Array.from({ length: tanks }, (_, i) => (all || steady(`${seed}#n${i}`) < CREWS.few ? list[(start + i) % list.length] : null));
  return names.some((n) => n) ? { mark: 'name', names } : null;
}

/** A block's crews' marks, from the battles its corps has come through before this one; named by the block, so the same every battle. */
export function crewFor(u: Pick<LandUnit, 'nat' | 'kind' | 'corps' | 'name' | 'figures0'>, battles: number): CrewMarks | null {
  return crewMarks(u.nat, u.kind, battles, u.figures0, `${u.corps}:${u.name}`);
}

/**
 * The hover's line for a block's named tanks, in the UI director's words (b477(6)): the English in brackets where it
 * differs from the name, three names in the rules' own order and then the count, all four where there are four. Null
 * where none is named: Germany's rings are never counted on screen.
 */
export function crewLine(c: CrewMarks | null): string | null {
  const named = (c?.names ?? []).filter((n): n is CrewName => !!n);
  if (!named.length) return null;
  const say = (n: CrewName): string => (n.en === n.name ? n.name : `${n.name} (${n.en})`);
  const shown = (named.length === 4 ? named : named.slice(0, 3)).map(say);
  const more = named.length - shown.length;
  return `Tanks named by their crews: ${shown.join(', ')}${more > 0 ? ` and ${more} more` : ''}`;
}
