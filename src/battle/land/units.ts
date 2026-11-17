// From the campaign's corps to the blocks on the field, and where they stand when the battle opens.
//
// One block is one division, named as the campaign's army tray names it (Brian's division cards, Version 46). A side
// with fewer than six has some divisions split into their three regiments so there is something to handle (ruling
// 217(c)); past twenty, divisions are folded two to a block. Each side's blocks share out exactly the power the
// attack card counted (A and D), so the card's odds are the field's.

import type { ForceTicket, Side, SideTicket, UnitBase } from '../core/types';
import { digWorks, type LandField } from './field';
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
}

export interface LandUnit extends UnitBase {
  kind: Kind;
  /** Its name for a card: "11th Infantry" (the army tray's short form). */
  short: string;
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
}

const ORD = (n: number): string => { const s = ['th', 'st', 'nd', 'rd'], v = n % 100; return n + (s[(v - 20) % 10] || s[v] || s[0]); };
const KIND_WORD: Record<Kind, string> = { inf: 'Infantry', mtn: 'Mountain', cav: 'Cavalry', eng: 'Engineers', mot: 'Motorized', arm: 'Armoured', art: 'Artillery', fort: 'Fortress' };

/** What one block carries of its force: whole divisions (one, or two folded together), or one regiment of a division. */
export interface BlockPart { divs: number[]; regiment?: number }

/** How each force is cut into blocks: one a division, split or folded until the side has between 'least' and 'most'. */
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
  // The force with the most blocks folds two whole divisions together first.
  while (total() > B.most) {
    let best = -1;
    parts.forEach((p, i) => { if (p.filter((x) => x.regiment === undefined && x.divs.length === 1).length >= 2 && (best < 0 || p.length > parts[best].length)) best = i; });
    if (best < 0) break;
    const p = parts[best];
    const singles = p.map((x, k) => (x.regiment === undefined && x.divs.length === 1 ? k : -1)).filter((k) => k >= 0);
    const [a, b] = singles.slice(-2);
    const merged: BlockPart = { divs: [...p[a].divs, ...p[b].divs] };
    p.splice(b, 1);
    p[a] = merged;
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
      const short = p.regiment !== undefined || ds.length > 1 ? name : ds[0].short;
      const [w, d] = LAND.size[k];
      const full = LAND.figures[k];
      const str0 = Math.max(0.05, Math.min(1, ds.reduce((s, x) => s + x.str, 0) / ds.length));
      const mor0 = Math.min(1, 0.45 + 0.35 * f.org + 0.2 * (f.mor ?? 0.6));
      const share = weightOf(p) / all;
      out.push({
        id: firstId + out.length, side, corps: f.corps, nat: f.nat, kind: k, name, short,
        x: 0, y: 0, face: side === 'att' ? 0 : Math.PI,
        str: str0, str0, mor: mor0, mor0, xp: ds.reduce((s, x) => s + x.xp, 0) / ds.length,
        state: 'steady', men0: Math.round(ds.reduce((s, x) => s + x.men, 0) / (p.regiment !== undefined ? LAND.blocks.regiments : 1)),
        figures0: Math.max(1, Math.ceil(full * str0)), figures: Math.max(1, Math.ceil(full * str0)),
        w, d, act: 'idle', speed: 0,
        power0: power * f.share * share,
        order: null, path: [], dug: !!f.dug, home: [0, 0], still: 99, clear: 99, pinned: false, flankedAt: -99, keepFace: null, met: false,
      });
    }
  });
  return out;
}

const FRONT: Kind[] = ['inf', 'mtn', 'eng', 'fort', 'cav'];
const MOBILE: Kind[] = ['arm', 'mot'];

/**
 * Draws a side up in its zone: foot in the front line across the middle, tanks and lorries on the wings behind it,
 * guns at the back. The defender forms in front of its town where it has one, and its dug-in blocks get their works.
 */
export function deploy(units: LandUnit[], side: Side, f: LandField): void {
  const mine = units.filter((u) => u.side === side);
  const z = f.zones[side];
  const dir = side === 'att' ? 1 : -1;
  const front = side === 'att' ? z[3] - 40 : z[1] + 40;
  const cx = side === 'def' && f.town ? Math.min(z[2] - 300, Math.max(z[0] + 300, f.town.x)) : (z[0] + z[2]) / 2;
  const lines: LandUnit[][] = [mine.filter((u) => FRONT.includes(u.kind)), mine.filter((u) => MOBILE.includes(u.kind)), mine.filter((u) => u.kind === 'art')];
  if (!lines[0].length && lines[1].length) lines[0] = lines[1].splice(0, Math.ceil(lines[1].length / 2));
  const place = (row: LandUnit[], y: number, wings: boolean): void => {
    const gap = 40;
    if (wings) {
      // Half on each wing, beyond the ends of the front line.
      const span = (lines[0].length * (150 + gap)) / 2;
      row.forEach((u, i) => {
        const left = i % 2 === 0, k = Math.floor(i / 2);
        u.x = cx + (left ? -1 : 1) * (span + 60 + k * (u.w + gap));
        u.y = y;
      });
      return;
    }
    const width = row.reduce((s, u) => s + u.w + gap, -gap);
    let x = cx - width / 2;
    for (const u of row) { u.x = x + u.w / 2; u.y = y; x += u.w + gap; }
  };
  place(lines[0], front, false);
  place(lines[1], front - dir * 110, true);
  place(lines[2], front - dir * 230, false);
  for (const u of mine) {
    u.x = Math.min(f.w - u.w / 2 - 10, Math.max(u.w / 2 + 10, u.x));
    u.y = Math.min(f.h - 40, Math.max(40, u.y));
    u.home = [u.x, u.y];
    if (u.dug) digWorks(f, u.x, u.y, u.w, u.d);
  }
}
