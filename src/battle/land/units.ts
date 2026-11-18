// From the campaign's corps to the blocks on the field, and where they stand when the battle opens.
//
// One block is one division, named as the campaign's army tray names it (Brian's division cards, Version 46). A side
// with fewer than six has some divisions split into their three regiments so there is something to handle (ruling
// 217(c)), each holding its division's frontage with a third of its models (b418); no division is ever folded into
// another (ruling 226(10)). Each side's blocks share out exactly the power the attack card counted (A and D), so the
// card's odds are the field's.

import type { CrewMarks } from './crews';
import type { Calibre, ForceTicket, Side, SideTicket, UnitBase } from '../core/types';
import { BEND, digWorks, type Bend, type LandField } from './field';
import { LAND, type Kind } from './rules';

export interface LandOrder {
  kind: 'move' | 'attack' | 'withdraw';
  x: number;
  y: number;
  /** The facing to take on arrival, from a drawn line. */
  face?: number;
  target?: number;
  /** An attack by several blocks at one: this block's place across the attack, metres right of its middle, and the attack's bearing. */
  lane?: number;
  axis?: number;
  /** Sent with others: when its block breaks or leaves, it carries the attack on to the nearest enemy still fighting. */
  press?: boolean;
}

export interface LandUnit extends UnitBase {
  kind: Kind;
  /** Its name for a card, and what it belongs to under it: "11th Infantry" over "I Corps", "1st Regiment" over "1st Infantry". */
  short: string;
  parent: string;
  /** Its share of its side's power at the start: what it strikes with and what it can take. */
  power0: number;
  /** Strength it came onto the field with (the campaign's own): its meter starts here. */
  str0: number;
  /** Morale it came onto the field with (the campaign's own): it comes back no higher than this. */
  mor0: number;
  xp: number;
  order: LandOrder | null;
  /** Where it is going next on the way to its order (a bridge, then the far bank). */
  path: [number, number][];
  /** Dug in where it began: cover while it stays put. */
  dug: boolean;
  home: [number, number];
  /** Seconds since it was last hit, and since it last had an enemy near (for a rally). */
  still: number;
  clear: number;
  /** Struck from its front this tick by someone: it holds its facing rather than turn. */
  pinned: boolean;
  /** When it was last told it was hit on the flank, so the dispatch is not repeated every tick. */
  flankedAt: number;
  /** The facing its order asked for: kept while the enemy is in front of it. */
  keepFace: number | null;
  /** In contact this battle already (for the first-contact dispatch). */
  met: boolean;
  /**
   * The player knows it (b464(2), LandBattle.known): a block of his has fought it, his guns from far back too, or it
   * has struck one of his in contact (its guns firing from far back do not count). Nothing in the fight reads it.
   */
  found?: boolean;
  /**
   * Carried off by its army's break while it still stood: it falls back in order and reads so, never Routing, which
   * belongs to the blocks that broke (the design director, after the slice 3 review). What it leaves behind on the
   * field is counted as a runner's, as before, so the field's figures and the fairness tests do not move.
   */
  fellBack?: boolean;
  /**
   * Given ground at the brink (b496(1)): worn in a small army that its fresh blocks hold at the brink, it falls back off
   * the field rather than stand to be ground down, and while it goes, or runs, it is struck only on its rearguard. Its
   * order is a withdrawal; the player may send it in again.
   */
  gives?: boolean;
  /**
   * A defence's reserve (the design director, b407): 'held' behind the line until the line is struck, then 'sent' at
   * marching pace to where it is struck hardest. A line block from a quiet stretch that closes on the fight is 'sent' too.
   * A block that is sent no longer goes back to the ground it was drawn up on.
   */
  reserve?: 'held' | 'sent';
  /** Models on its tray at full strength: a division's, or a third of them for a regiment. */
  tray: number;
  /** A tank block's crews' own marks, names or kill rings, as data for the 3D game and the hover (b477); none on the rest. */
  crew?: CrewMarks;
  /**
   * What its guns throw, as the explosions sheet sizes it (shells.ts): data for the 3D game, never read by the fight.
   * None where it has only small arms.
   */
  cal?: Calibre;
  /**
   * Looks only (shells.ts): the harm each block that struck it has done it, by the striker's id, less a model's worth
   * for each model of it the striker was given. Each model it loses goes to the striker of that tick owed most, so a gun
   * firing from far back lands its shells in its share of the harm (the design director, b507). Nothing in the fight
   * reads it.
   */
  owed?: Record<number, number>;
  /** The way it stepped this tick, if it marched: at a bridge or a pass, a friend in its way makes way (LAND.queue). */
  pressing?: number;
  /** The friend it made way for this tick (LAND.queue): next tick it waits rather than step back into that friend's way. */
  yieldTo?: number;
  /** Where its attack has it stand, as last worked out: its own stretch, which a friend coming on later stands beside (b439). */
  stand?: [number, number];
  /** What its stand was worked out for (its target, and its lane): the stand holds while it is still its own (b453). */
  standFor?: string;
  /** The way its stand was worked out facing: the friends beside it are reckoned the same way while it holds it. */
  standFace?: number;
  /** It came to its stand still out of reach of its target, which had stepped back: it closes straight on it. */
  closing?: boolean;
  /**
   * Its corps comes on from ground with no river between it and the place (ForceTicket.cross false): the card counts it
   * attacking over dry ground, so it draws up in the lanes the river bends away from (the design director, b464(5);
   * field.ts, Bend), and the river's hold is not counted against what it strikes (LAND.hold.river).
   */
  dry?: boolean;
}

const ORD = (n: number): string => { const s = ['th', 'st', 'nd', 'rd'], v = n % 100; return n + (s[(v - 20) % 10] || s[v] || s[0]); };
const KIND_WORD: Record<Kind, string> = { inf: 'Infantry', mtn: 'Mountain', cav: 'Cavalry', eng: 'Engineers', mot: 'Motorized', arm: 'Armoured', art: 'Artillery', fort: 'Fortress' };

/** What one block carries of its force: one whole division, or one of its regiments. */
export interface BlockPart { divs: number[]; regiment?: number }

/** How each force is cut into blocks: one a division, split into regiments until the side has at least 'least'. */
export function partsFor(forces: ForceTicket[]): BlockPart[][] {
  const B = LAND.blocks;
  const parts: BlockPart[][] = forces.map((f) => Array.from({ length: Math.max(1, Math.round(f.divs)) }, (_, i) => ({ divs: [i] })));
  const total = (): number => parts.reduce((s, p) => s + p.length, 0);
  // The force whose blocks are biggest gives up a whole division to its regiments first.
  while (total() < B.least) {
    let best = -1, size = 0;
    parts.forEach((p, i) => {
      if (!p.some((x) => x.regiment === undefined && x.divs.length === 1)) return;
      const each = forces[i].share / p.length;
      if (each > size) { size = each; best = i; }
    });
    if (best < 0) break;
    const p = parts[best], j = p.findIndex((x) => x.regiment === undefined && x.divs.length === 1);
    p.splice(j, 1, ...Array.from({ length: B.regiments }, (_, r) => ({ divs: p[j].divs, regiment: r })));
  }
  return parts;
}

/** How many blocks each force gives. */
export function blocksFor(forces: ForceTicket[]): number[] {
  return partsFor(forces).map((p) => p.length);
}

/** The blocks of one side, not yet placed. */
export function makeBlocks(side: Side, t: SideTicket, power: number, firstId: number): LandUnit[] {
  const out: LandUnit[] = [];
  const cut = partsFor(t.forces);
  t.forces.forEach((f, i) => {
    const k = (Object.keys(LAND.speed).includes(f.kind) ? f.kind : 'inf') as Kind;
    const n = Math.max(1, Math.round(f.divs));
    // Each division as the campaign has it; where the ticket carries none, numbered as the corps' own and alike.
    const divs = Array.from({ length: n }, (_, d) => {
      const own = f.divisions?.[d];
      const no = (f.first ?? 1) + d;
      return {
        name: own?.name ?? `${ORD(no)} ${KIND_WORD[k]} Division`, short: own?.short ?? `${ORD(no)} ${KIND_WORD[k]}`,
        str: own?.str ?? f.str, xp: own?.xp ?? f.xp, men: own?.men ?? f.men / n,
      };
    });
    const weightOf = (p: BlockPart): number => p.divs.reduce((s, d) => s + Math.max(0.01, divs[d].str), 0) / (p.regiment !== undefined ? LAND.blocks.regiments : 1);
    const all = cut[i].reduce((s, p) => s + weightOf(p), 0);
    for (const p of cut[i]) {
      const ds = p.divs.map((d) => divs[d]);
      const name = p.regiment !== undefined ? `${ORD(p.regiment + 1)} Regiment, ${ds[0].short}`
        : ds.length === 1 ? ds[0].name : ds.map((d) => d.short).join(' and ');
      const short = p.regiment !== undefined ? `${ORD(p.regiment + 1)} Regiment` : ds.length > 1 ? name : ds[0].short;
      const parent = p.regiment !== undefined ? ds[0].short : f.name;
      // A regiment holds its division's frontage, thinner: a third of the models on the same tray. Cut to a third of the
      // frontage, three regiments of a small attack crowded onto one enemy block, power packed three times as thick as the
      // card counts it, and the desert's coast villages ran 18 points over their card (b418, b420).
      const [w, d] = LAND.size[k];
      const full = p.regiment !== undefined ? Math.max(1, Math.round(LAND.figures[k] / LAND.blocks.regiments)) : LAND.figures[k];
      const str0 = Math.max(0.05, Math.min(1, ds.reduce((s, x) => s + x.str, 0) / ds.length));
      const mor0 = Math.min(1, 0.45 + 0.35 * f.org + 0.2 * (f.mor ?? 0.6));
      const share = weightOf(p) / all;
      out.push({
        id: firstId + out.length, side, corps: f.corps, nat: f.nat, kind: k, name, short, parent,
        x: 0, y: 0, face: side === 'att' ? 0 : Math.PI,
        str: str0, str0, mor: mor0, mor0, xp: ds.reduce((s, x) => s + x.xp, 0) / ds.length,
        state: 'steady', men0: Math.round(ds.reduce((s, x) => s + x.men, 0) / (p.regiment !== undefined ? LAND.blocks.regiments : 1)),
        figures0: Math.max(1, Math.ceil(full * str0)), figures: Math.max(1, Math.ceil(full * str0)), tray: full,
        w, d, act: 'idle', speed: 0,
        power0: power * f.share * share,
        order: null, path: [], dug: !!f.dug, home: [0, 0], still: 99, clear: 99, pinned: false, flankedAt: -99, keepFace: null, met: false,
        ...(f.cross === false ? { dry: true } : {}),
      });
    }
  });
  return out;
}

const FRONT: Kind[] = ['inf', 'mtn', 'eng', 'fort', 'cav'];
const MOBILE: Kind[] = ['arm', 'mot'];
/** Metres between blocks drawn up side by side in a row. */
const GAP = 40;

/** A side's rows: foot in the front line, tanks and lorries next (half of them in front where there is no foot), guns. */
function rowsOf(mine: LandUnit[]): LandUnit[][] {
  const lines: LandUnit[][] = [mine.filter((u) => FRONT.includes(u.kind)), mine.filter((u) => MOBILE.includes(u.kind)), mine.filter((u) => u.kind === 'art')];
  if (!lines[0].length && lines[1].length) lines[0] = lines[1].splice(0, Math.ceil(lines[1].length / 2));
  return lines;
}
const rowWidth = (row: LandUnit[]): number => row.reduce((s, u) => s + u.w + GAP, -GAP);

/**
 * Where the river bends (b464(5)): the x of the line between the attack's lanes that do not cross ('dry' at that end
 * of the field) and those that do. Each group of corps has as much of the attacker's band as its widest row needs, the
 * room to spare shared out the same way, and BEND.clear either side of the line, where the river runs down to the
 * attacker's edge, kept clear.
 */
export function bendAt(units: LandUnit[], z: [number, number, number, number], dry: 'west' | 'east'): number {
  const att = units.filter((u) => u.side === 'att');
  const need = (us: LandUnit[]): number => Math.max(0, ...rowsOf(us).map(rowWidth));
  const a = need(att.filter((u) => u.dry)), b = need(att.filter((u) => !u.dry)), room = z[2] - z[0] - 2 * BEND.clear;
  const span = a + b > 0 ? (room >= a + b ? a + ((room - a - b) * a) / (a + b) : (room * a) / (a + b)) : room / 2;
  return dry === 'west' ? z[0] + span + BEND.clear : z[2] - span - BEND.clear;
}

/**
 * Draws a side up in its zone: foot in the front line across the middle, tanks and lorries on the wings behind it,
 * guns at the back. The defender forms in front of its town where it has one, and its dug-in blocks get their works.
 * Where the river bends away from the lanes of the corps that do not cross (field.bend, b464(5)), the attack draws up
 * as two: those that do not cross at their end of the band, those that cross at the other, the river between them.
 */
export function deploy(units: LandUnit[], side: Side, f: LandField): void {
  const mine = units.filter((u) => u.side === side);
  const z = f.zones[side];
  if (side === 'att' && f.bend) {
    const B: Bend = f.bend, west = B.dry === 'west', inner = (s: -1 | 1): number => B.x + s * BEND.clear;
    const lanes: [LandUnit[], number, number][] = [
      [mine.filter((u) => u.dry), west ? 10 : inner(1), west ? inner(-1) : f.w - 10],
      [mine.filter((u) => !u.dry), west ? inner(1) : 10, west ? f.w - 10 : inner(-1)],
    ];
    for (const [us, lo, hi] of lanes) if (us.length) drawUp(us, side, f, (Math.max(lo, z[0]) + Math.min(hi, z[2])) / 2, lo, hi);
  } else drawUp(mine, side, f, side === 'def' && f.town ? Math.min(z[2] - 300, Math.max(z[0] + 300, f.town.x)) : (z[0] + z[2]) / 2);
  for (const u of mine) {
    u.x = Math.min(f.w - u.w / 2 - 10, Math.max(u.w / 2 + 10, u.x));
    u.y = Math.min(f.h - 40, Math.max(40, u.y));
    u.home = [u.x, u.y];
    if (u.dug) digWorks(f, u.x, u.y, u.w, u.d);
  }
}

/**
 * Draws blocks up in rows about cx. Between lo and hi (metres east) where they are given, the attack's lanes either side
 * of the river's bend: a row that would run past either is moved in, and a row wider than the room goes on in a further
 * row behind it, GAP kept between its blocks, so no two are drawn up on top of each other (b492(3)).
 */
function drawUp(mine: LandUnit[], side: Side, f: LandField, cx: number, lo?: number, hi?: number): void {
  const z = f.zones[side];
  const dir = side === 'att' ? 1 : -1;
  const front = side === 'att' ? z[3] - 40 : z[1] + 40;
  const lines = rowsOf(mine);
  const x0 = lo ?? 10, x1 = hi ?? f.w - 10;
  /**
   * Where a row stands on the wings: half on each, beyond the ends of the front line. None where a wing would run off
   * the field or past its lane (a big army's long front): the row stands centred behind the line instead, so no two blocks are drawn up
   * on top of each other.
   */
  const wingsAt = (row: LandUnit[]): number[] | null => {
    const span = (lines[0].length * (150 + GAP)) / 2;
    const at = row.map((u, i) => { const left = i % 2 === 0, k = Math.floor(i / 2); return cx + (left ? -1 : 1) * (span + 60 + k * (u.w + GAP)); });
    return row.every((u, i) => at[i] - u.w / 2 >= x0 && at[i] + u.w / 2 <= x1) ? at : null;
  };
  /** Draws a row up at depth y; says whether it stood on the wings (false: centred). */
  const place = (row: LandUnit[], y: number, wings: boolean): boolean => {
    const at = wings ? wingsAt(row) : null;
    if (at) {
      row.forEach((u, i) => { u.x = at[i]; u.y = y; });
      return true;
    }
    const width = rowWidth(row);
    let x = cx - width / 2;
    for (const u of row) { u.x = x + u.w / 2; u.y = y; x += u.w + GAP; }
    if (lo !== undefined && hi !== undefined && row.length > 0) {
      // Kept to its own lanes: moved in off the river's bend.
      const a = row[0].x - row[0].w / 2, b = row[row.length - 1].x + row[row.length - 1].w / 2;
      const shift = a < x0 ? x0 - a : b > x1 ? x1 - b : 0;
      for (const u of row) u.x += shift;
    }
    return false;
  };
  // A defence of four blocks or more in its line keeps a quarter of them back as a reserve, centred between its tanks
  // on the wings (b407). Each is the last division of whichever corps has most in the line, so every corps' first
  // divisions stand in the line. Where the tanks could not stand on the wings and are centred behind the line, the
  // reserve stands a row further back.
  const held: LandUnit[] = [];
  if (side === 'def' && lines[0].length >= 4) {
    for (let n = Math.max(1, Math.floor(lines[0].length / 4)); n > 0; n--) {
      const count = (c: number): number => lines[0].filter((u) => u.corps === c).length;
      let at = -1;
      for (let i = lines[0].length - 1; i >= 0; i--) if (at < 0 || count(lines[0][i].corps) > count(lines[0][at].corps)) at = i;
      held.unshift(...lines[0].splice(at, 1));
    }
  }
  if (lo === undefined || hi === undefined) {
    place(lines[0], front, false);
    const wide = place(lines[1], front - dir * 110, true);
    if (held.length) { place(held, front - dir * (wide || !lines[1].length ? 110 : 170), false); for (const u of held) u.reserve = 'held'; }
    place(lines[2], front - dir * 230, false);
    return;
  }
  // In its lanes (b492(3)): each line cut into rows no wider than the room, the front ones full and the rest behind.
  // A line's first row stands where it would with room enough (the tanks 110 m behind the front line, the guns 230 m),
  // or further back to keep GAP behind the row before it, as each row after it does. Where the last row would stand
  // past the back of the zone, the gaps between rows close up, 5 m at a time to no less than 10 m; past that the rows
  // run on back toward the attacker's edge of the field.
  const rows: { us: LandUnit[]; at: number; first: boolean; wings: boolean }[] = [];
  const cut = (line: LandUnit[], at: number): void => line.forEach((u, i) => {
    const r = rows[rows.length - 1];
    if (i > 0 && rowWidth([...r.us, u]) <= x1 - x0) r.us.push(u);
    else rows.push({ us: [u], at, first: i === 0, wings: false });
  });
  cut(lines[0], 0);
  // The tanks stand on the wings only beside a front line that stands in one row.
  const wide = rows.length <= 1 && lines[1].length > 0 && wingsAt(lines[1]) !== null;
  if (wide) rows.push({ us: lines[1], at: 110, first: true, wings: true });
  else cut(lines[1], 110);
  cut(held, wide || !lines[1].length ? 110 : 170);
  for (const u of held) u.reserve = 'held';
  cut(lines[2], 230);
  const deep = (us: LandUnit[]): number => Math.max(...us.map((u) => u.d));
  const steps = (gap: number): number[] => {
    const ys: number[] = [];
    rows.forEach((r, i) => {
      const behind = i ? ys[i - 1] - dir * (deep(rows[i - 1].us) / 2 + gap + deep(r.us) / 2) : front;
      ys.push(!r.first ? behind : dir > 0 ? Math.min(front - r.at, behind) : Math.max(front + r.at, behind));
    });
    return ys;
  };
  const back = dir > 0 ? z[1] : z[3], n = rows.length - 1;
  const past = (ys: number[]): boolean => n >= 0 && dir * (ys[n] - back) < deep(rows[n].us) / 2;
  let ys = steps(GAP);
  for (let gap = GAP - 5; gap >= 10 && past(ys); gap -= 5) ys = steps(gap);
  rows.forEach((r, i) => place(r.us, ys[i], r.wings));
}
