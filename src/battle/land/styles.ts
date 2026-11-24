// How a computer general fights (the tactics panel, item 1; Brian, 19 Nov: "more general themes", "a sandbox to play and
// make your own history"). Every army fights on one plan (ai.ts); a style is only the numbers on it: how long the guns
// fire before the foot goes in, how much weight goes at the main effort and how wide, when the tanks leave the foot, how
// far a defence steps out. An attack holds nothing back, and every defence holds the fitted quarter back: the card
// prices the whole army and the fit's reserve, so a style moves where the weight falls, never how much of it strikes
// (the design director, ruling 239's riders, 21:28Z and 21:57Z). data/battle/land-styles.json holds them.
//
// Read only from the ticket, and never drawn from the dice: the same ticket fights in the same style every time, so a
// battle replays. A side the player commands has none: the dial leashes the computer, never the player.

import data from '../../../data/battle/land-styles.json';
import leanData from '../../../data/battle/lean.json';
import type { FieldTicket, Side, SideTicket } from '../core/types';
import { streetsOf, type LandPlace } from './field';

export type StyleId = 'bold' | 'methodical' | 'mass';

export interface Style {
  id: StyleId;
  /** False: no main effort, every block its own lane at once, tanks with the line (mass). */
  plan: boolean;
  /** Seconds the main effort waits at its start line under its guns. */
  wait: number;
  /** The share of the side's non-gun power at the main effort. */
  main: number;
  /** Enemy line blocks in the main effort. */
  stretch: number;
  /** The tanks leave the foot once the main target's nerve is under this share of its start; null, never. */
  hook: number | null;
  /** The hook goes behind the target, or onto guns close behind it; else onto its flank. */
  deep: boolean;
  /** Metres a defending block goes from its home (never past LAND.hold.within), and the gap at which it finishes a wavering attacker. */
  defLeash: number;
  defFinish: number;
  /** While its line is unstruck, its held block walks behind the line toward an attack on the march within this many metres (0: never). */
  defShift: number;
}

type Row = Omit<Style, 'id'>;
const D = data as unknown as {
  styles: Record<StyleId, Row>;
  shared: { startLine: number; latest: number; lead: number; raw: number; near: number; deepBehind: number };
  path: Record<string, StyleId[]>;
  grow: { xp: number[]; tech: string[] };
  pick: { attOdds: number; defOdds: number; armour: number; open: string[] };
  dial: { absent: number; pickFrom: number };
};

/** The styles by name, and the numbers every style shares. */
export const STYLES: Record<StyleId, Style> = Object.fromEntries(
  (Object.keys(D.styles) as StyleId[]).map((id) => [id, { id, ...D.styles[id] }]),
) as Record<StyleId, Style>;
export const STYLE = D.shared;
export const PATH = D.path;

/** A nation's leaning in one arm's column: a style, or one for each side where it attacks and defends differently. */
type Leaning = string | { att: string; def: string };
const L = leanData as unknown as { lean: Record<string, Record<'land' | 'sea' | 'air', Leaning>>; standIn: Record<string, string> };
/** The land column of the one lean table (data/battle/lean.json): each nation's own leaning, '_' every other. */
export const LEAN: Record<string, Leaning> = Object.fromEntries(Object.entries(L.lean).map(([n, row]) => [n, row.land]));

/** A nation's leaning on land for this side, a leaning not built yet (night, last-stand: item 6) fought as its stand-in. */
export function leaningOf(nat: string, side: Side): StyleId | null {
  const row = LEAN[nat];
  if (row === undefined) return null;
  const s = typeof row === 'string' ? row : row[side];
  const id = isStyle(s) ? s : L.standIn[s];
  return isStyle(id) ? id : 'methodical';
}

const isStyle = (s: string | undefined): s is StyleId => !!s && s in STYLES;

/** A side's experience: its corps' xp weighted by their share of its power. */
export function sideXp(s: SideTicket): number {
  const tot = s.forces.reduce((a, f) => a + (f.share ?? 0), 0);
  if (tot > 0) return s.forces.reduce((a, f) => a + (f.share ?? 0) * f.xp, 0) / tot;
  return s.forces.length ? s.forces.reduce((a, f) => a + f.xp, 0) / s.forces.length : 0;
}

/** The share of a side's power in these kinds of corps. */
function kindShare(s: SideTicket, kinds: string[]): number {
  const tot = s.forces.reduce((a, f) => a + (f.share ?? 0), 0) || 1;
  return s.forces.filter((f) => kinds.includes(f.kind)).reduce((a, f) => a + (f.share ?? 0), 0) / tot;
}

/**
 * How many steps a nation with a path has grown along it, by its army's experience or its research, never by date (the
 * design director, ruling 239(2) and (8): no dated event is a trigger): one at the first experience mark or the first
 * doctrine, two at the second mark or the second doctrine.
 */
export function stepsOf(s: SideTicket): number {
  const xp = sideXp(s), G = D.grow, has = (id: string): boolean => !!s.doctrine?.includes(id);
  let n = 0;
  if (xp >= G.xp[0] || has(G.tech[0])) n = 1;
  if (xp >= G.xp[1] || has(G.tech[1])) n = 2;
  return n;
}

/** Stops 1 to 3: the nation's own leaning, grown along its path; any other nation the table's '_', or mass while raw. */
function leanFor(s: SideTicket, side: Side): StyleId {
  const path = PATH[s.nat];
  if (path) return path[Math.min(path.length - 1, stepsOf(s))];
  const own = s.nat !== '_' ? leaningOf(s.nat, side) : null;
  if (own) return own;
  return sideXp(s) < STYLE.raw ? 'mass' : leaningOf('_', side) ?? 'methodical';
}

/**
 * Stops 4 and 5: the situation picks, on the ticket's facts only, never the nation. The attack: raw troops go in as a
 * mass; works, streets, an armoured river crossing or short odds call for a methodical attack; tanks and lorries a
 * quarter of the army on open ground, a bold one. The defence: raw troops stand as a mass; a methodical defence
 * against tanks on open ground; a bold one at close odds.
 */
export function pickFor(t: FieldTicket<LandPlace>, side: Side): StyleId {
  const s = t[side], P = D.pick, p = t.place, odds = t.reckoning.odds;
  if (sideXp(s) < STYLE.raw) return 'mass';
  const open = P.open.includes(p.terrain);
  if (side === 'att') {
    const armouredCrossing = !!p.river && s.forces.some((f) => (f.kind === 'arm' || f.kind === 'mot') && f.cross !== false);
    if (p.fort >= 1 || streetsOf(p) || armouredCrossing || odds < P.attOdds) return 'methodical';
    if (open && kindShare(s, ['arm', 'mot']) >= P.armour) return 'bold';
    return 'methodical';
  }
  if (open && kindShare(t.att, ['arm']) >= P.armour) return 'methodical';
  if (odds <= P.defOdds) return 'bold';
  return 'methodical';
}

/** The history dial's stop a ticket is fought at. */
export const stopOfTicket = (t: FieldTicket): number => t.hist ?? D.dial.absent;

/**
 * A computer general's style on this side, or null where the player commands it. A style fixed on the ticket by hand
 * (tests, a sandbox's invented pairing) comes first; else the dial: its nation's own at stops 1 to 3, the situation's
 * at 4 and 5.
 */
export function styleFor(t: FieldTicket<LandPlace>, side: Side): Style | null {
  const s = t[side];
  if (s.human) return null;
  if (isStyle(s.style)) return STYLES[s.style];
  return STYLES[stopOfTicket(t) >= D.dial.pickFrom ? pickFor(t, side) : leanFor(s, side)];
}
