// The drawing board's blueprint (the sandbox; Brian, 19 November 2026, "yes do all 3": the drawing board laid out as
// a blueprint table, the ship's outline changing as each dial turns). A design is drawn in profile from her dials,
// bow to the right, as plain strokes and words that both games draw the same way: the 3D game on its own paper, the
// browser as lines. Turn a dial and the drawing follows: a longer hull, more and bigger turrets, a thicker belt, a
// funnel more for each step of speed, more guns for the air, a catapult or a flight deck; a submarine's length, her
// tubes, her saddle tanks for range and her hangar.
//
// Nothing here is a rule. What a design is worth, costs and needs is designCard's and subCard's (sandbox.ts); this
// file only draws. The sheet is 1000 wide and 360 high, the waterline at 215; y grows downward, as on a screen.

import type { Dials, SubDials } from './sandbox';

/** How a stroke is inked: the hull's outline, the parts on it, fine detail, the sea, the paper's own lines, a callout's leader. */
export type Ink = 'hull' | 'part' | 'fine' | 'water' | 'paper' | 'note';
/** A run of straight lines through the points (x, y, x, y, ...), closed back to its start when `shut`. */
export interface Stroke { pts: number[]; w: number; ink: Ink; shut?: boolean }
/** A word on the sheet: at (x, y), its top edge, set left, centred or right of x. `big` is the title block's heading. */
export interface Mark { x: number; y: number; text: string; align: 'l' | 'c' | 'r'; big?: boolean }
export interface Blueprint { w: number; h: number; water: number; strokes: Stroke[]; marks: Mark[] }

export const SHEET = { w: 1000, h: 360, water: 215 };
const W = SHEET.w, H = SHEET.h, WL = SHEET.water;
const r1 = (x: number): number => Math.round(x * 10) / 10;

class Pen {
  strokes: Stroke[] = [];
  marks: Mark[] = [];
  line(pts: number[], w: number, ink: Ink, shut = false): void { this.strokes.push({ pts: pts.map(r1), w, ink, ...(shut ? { shut: true } : {}) }); }
  box(x: number, y: number, w: number, h: number, wgt: number, ink: Ink): void { this.line([x, y, x + w, y, x + w, y + h, x, y + h], wgt, ink, true); }
  word(x: number, y: number, text: string, align: Mark['align'] = 'l', big = false): void { this.marks.push({ x: r1(x), y: r1(y), text, align, ...(big ? { big: true } : {}) }); }
  /** Diagonal lines across a box, `gap` apart: the hatching of armour. */
  hatch(x: number, y: number, w: number, h: number, gap: number): void {
    for (let s = x - h; s < x + w; s += gap) {
      const a = Math.max(x, s), b = Math.min(x + w, s + h);
      if (b > a) this.line([a, y + h - (a - s), b, y + h - (b - s)], 0.6, 'fine');
    }
  }
  /** A callout to place: words in the top or bottom margin and a leader from them to the part at `at`. */
  notes: { at: [number, number]; text: string; top: boolean }[] = [];
  callout(at: [number, number], text: string, top = true): void { this.notes.push({ at, text, top }); }
  /**
   * Sets the callouts out along their margin in one row each, every one as near over (or under) its part as the
   * others allow, so the leaders run short and nearly straight; the bottom row keeps clear of the title block.
   */
  lay(): void {
    for (const top of [true, false]) {
      const items = this.notes.filter((c) => c.top === top).sort((a, b) => a.at[0] - b.at[0]);
      const lo = 24, hi = top ? W - 24 : W - 280;
      const spans = items.map((c) => { const wide = c.text.length * 6.6 + 4; return { c, wide, a: Math.max(lo, Math.min(hi - wide, c.at[0] - wide / 2)) }; });
      // Push right past any overlap, then pull back left from the far edge if the row ran over.
      for (let i = 1; i < spans.length; i++) spans[i].a = Math.max(spans[i].a, spans[i - 1].a + spans[i - 1].wide + 18);
      for (let i = spans.length - 1; i >= 0; i--) {
        const limit = i === spans.length - 1 ? hi : spans[i + 1].a - 18;
        if (spans[i].a + spans[i].wide > limit) spans[i].a = limit - spans[i].wide;
      }
      const y = top ? 22 : H - 44;
      for (const { c, wide, a } of spans) {
        this.word(a, y, c.text, 'l');
        const lx = Math.max(a + 6, Math.min(a + wide - 6, c.at[0]));
        const ly = top ? y + 16 : y - 4;
        this.line([lx, ly, c.at[0], c.at[1]], 0.8, 'note');
        this.line([c.at[0] - 2.5, c.at[1] - 2.5, c.at[0] + 2.5, c.at[1] + 2.5], 0.8, 'note');
        this.line([c.at[0] - 2.5, c.at[1] + 2.5, c.at[0] + 2.5, c.at[1] - 2.5], 0.8, 'note');
      }
    }
  }
}

/** The paper itself: its border, the waterline across it, and the dimension line under the keel with the length. */
function paper(p: Pen, x0: number, xb: number, keel: number, metres: number): void {
  p.box(8, 8, W - 16, H - 16, 1.2, 'paper');
  p.line([24, WL, W - 24, WL], 1, 'water');
  for (let x = 40; x < W - 40; x += 46) p.line([x, WL + 6, x + 16, WL + 6], 0.6, 'water');
  const y = keel + 30, mid = (x0 + xb) / 2;
  p.line([x0, y - 7, x0, y + 7], 0.8, 'fine');
  p.line([xb, y - 7, xb, y + 7], 0.8, 'fine');
  p.line([x0, y, mid - 40, y], 0.8, 'fine');
  p.line([mid + 40, y, xb, y], 0.8, 'fine');
  p.line([x0 + 9, y - 4, x0, y, x0 + 9, y + 4], 0.8, 'fine');
  p.line([xb - 9, y - 4, xb, y, xb - 9, y + 4], 0.8, 'fine');
  p.word(mid, y - 7, `${metres} M`, 'c');
}

/** The title block in the sheet's lower right corner: what she is, and for whom and when she was drawn. */
function titleBlock(p: Pen, title: string, sub: string, foot: string): void {
  const x = W - 262, y = H - 76;
  p.box(x, y, 246, 60, 1, 'paper');
  p.line([x, y + 26, x + 246, y + 26], 0.6, 'paper');
  p.word(x + 10, y + 6, title, 'l', true);
  p.word(x + 10, y + 31, sub, 'l');
  p.word(x + 10, y + 45, foot, 'l');
}

/** A turret seen from the side, its barrels toward `dir` (1 the bow, -1 the stern), standing on the deck at y. */
function turret(p: Pen, x: number, y: number, size: number, dir: 1 | -1, raise: number): { top: [number, number] } {
  const tw = [0, 18, 26, 36, 46][size], th = [0, 8, 11, 14, 17][size], bl = [0, 20, 30, 42, 54][size];
  const base = y - raise;
  if (raise > 0) p.box(x - tw * 0.32, base, tw * 0.64, raise, 0.9, 'part');
  // The gunhouse: a sloped face toward the barrels.
  const back = x - (dir * tw) / 2, front = x + (dir * tw) / 2;
  p.line([back, base, back, base - th, front - dir * th * 0.45, base - th, front, base - th * 0.35, front, base], 1.1, 'part', true);
  const by = base - th * 0.55;
  const muzzle = front + dir * bl;
  p.line([front, by, muzzle, by - 1.5], size >= 3 ? 1.6 : 1.1, 'part');
  if (size >= 3) p.line([front, by + 3, muzzle, by + 1.5], 1.6, 'part');
  return { top: [x, base - th] };
}

/** A light anti-aircraft mount: a small tub and two barrels cocked up. */
function aaMount(p: Pen, x: number, y: number): void {
  p.box(x - 3.5, y - 4, 7, 4, 0.7, 'fine');
  p.line([x - 1, y - 4, x + 3, y - 10], 0.7, 'fine');
  p.line([x + 1.5, y - 4, x + 5.5, y - 10], 0.7, 'fine');
}

/** A floatplane on her catapult, or parked on a submarine's deck: fuselage, wing, tail and float. */
function seaplane(p: Pen, x: number, y: number, s = 1): void {
  p.line([x - 13 * s, y - 2 * s, x + 12 * s, y - 3 * s, x + 14 * s, y - 5 * s], 0.9, 'fine');
  p.line([x - 3 * s, y - 6 * s, x + 6 * s, y - 6 * s], 1.2, 'fine');
  p.line([x - 13 * s, y - 2 * s, x - 15 * s, y - 8 * s, x - 11 * s, y - 3 * s], 0.9, 'fine');
  p.line([x - 6 * s, y + 3 * s, x + 9 * s, y + 3 * s, x + 11 * s, y + 1 * s], 0.9, 'fine');
  p.line([x - 1 * s, y - 2 * s, x - 2 * s, y + 3 * s], 0.7, 'fine');
  p.line([x + 6 * s, y - 2 * s, x + 6 * s, y + 3 * s], 0.7, 'fine');
}

/** Words for one turn of a dial on a callout. */
const up = (s: string): string => s.toUpperCase();
const TURRET_WORD = ['NO', 'ONE', 'TWO', 'THREE', 'FOUR'];

/** Labels the board's dials have, step by step (sandbox-ships.json board), for the callouts. */
export interface StepWords { hull: string[]; guns: string[]; armour: string[]; speed: string[]; aa: string[]; air: string[] }
export interface SubWords { size: string[]; tubes: string[]; range: string[]; hangar: string[] }

/**
 * A surface ship in profile from her dials. `kind` is what the board makes of her (a battleship, a carrier, a cruiser
 * or a flotilla of destroyers), `title`/`foot` go in the title block.
 */
export function drawShip(d: Dials, words: StepWords, title: string, foot: string): Blueprint {
  const p = new Pen();
  const hull = Math.max(0, Math.min(3, d.hull)), speed = Math.max(0, Math.min(2, d.speed)), air = Math.max(0, Math.min(3, d.air));
  const guns = d.guns > 0 ? Math.max(1, Math.min(4, d.guns)) : 0, turrets = guns ? Math.max(1, Math.min(4, d.turrets)) : 0;
  const L = [560, 700, 820, 920][hull], x0 = (W - L) / 2, xb = x0 + L;
  const f = [22, 28, 34, 38][hull], dr = [14, 20, 26, 30][hull];
  const metres = [115, 185, 245, 280][hull];
  const rake = [0.03, 0.05, 0.07][speed] * L;
  const keel = WL + dr;
  // The deck's sheer, from the stern to the stem: low aft, level amidships, rising to the bow (more for a fast ship).
  const sheer: [number, number][] = [[x0, WL - f * 0.92], [x0 + 0.2 * L, WL - f], [xb - 0.3 * L, WL - f], [xb - 0.12 * L, WL - f * (1.06 + 0.02 * speed)], [xb, WL - f * (1.2 + 0.05 * speed)]];
  const deckY = (x: number): number => {
    for (let i = 1; i < sheer.length; i++) {
      const [ax, ay] = sheer[i - 1], [bx, by] = sheer[i];
      if (x <= bx) return ay + ((by - ay) * (x - ax)) / Math.max(1e-6, bx - ax);
    }
    return sheer[sheer.length - 1][1];
  };
  paper(p, x0, xb, keel, metres);

  // The hull: transom, keel, the forefoot curving up into the stem, and the deck back aft.
  p.line([
    x0, deckY(x0), x0 + 0.012 * L, WL + dr * 0.35, x0 + 0.06 * L, keel, xb - rake - 0.08 * L, keel,
    xb - rake - 0.035 * L, WL + dr * 0.75, xb - rake - 0.012 * L, WL + dr * 0.3, xb - rake * 0.5, WL - f * 0.3, xb, deckY(xb),
    ...sheer.slice(0, -1).reverse().flat(),
  ], 1.8, 'hull', true);

  // The armour belt along the waterline: a line when light, a hatched band when medium, a double band when heavy.
  const ba = x0 + (hull === 0 ? 0.3 : 0.2) * L, bb = xb - (hull === 0 ? 0.3 : 0.2) * L;
  if (d.armour <= 0) p.line([ba, WL - 3, bb, WL - 3], 0.7, 'fine');
  else {
    const top = d.armour >= 2 ? 12 : 7, low = d.armour >= 2 ? 5 : 3;
    p.box(ba, WL - top, bb - ba, top + low, d.armour >= 2 ? 1.4 : 1, 'part');
    if (d.armour >= 2) p.box(ba + 3, WL - top + 3, bb - ba - 6, top + low - 6, 0.6, 'fine');
    p.hatch(ba, WL - top, bb - ba, top + low, d.armour >= 2 ? 7 : 12);
  }
  const belt: [number, number] = [bb - 30, WL + (d.armour >= 2 ? 5 : d.armour ? 3 : -3)];

  const flat = air === 3, half = air === 2;
  const hangar = f * 0.95;
  const fdY = WL - f - hangar;
  const aaN = [2, 5, 9][Math.max(0, Math.min(2, d.aa))];
  const aaAt: [number, number][] = [];
  let gunsAt: [number, number] = [xb - 0.16 * L, deckY(xb - 0.16 * L)];
  let stackAt: [number, number] = [0, 0];
  let airAt: [number, number] | null = null;

  if (flat) {
    // A carrier: the hangar along the hull, the flight deck over it end to end, the island to starboard with the stack.
    const ha = x0 + 0.06 * L, hb = xb - 0.1 * L;
    p.line([ha, deckY(ha), ha, fdY, hb, fdY, hb, deckY(hb)], 1.2, 'part');
    for (let x = ha + 18; x < hb - 18; x += 34) p.line([x, fdY + hangar * 0.35, x + 16, fdY + hangar * 0.35], 0.7, 'fine');
    p.box(x0 + 0.015 * L, fdY - 4, L * 0.94, 4, 1.4, 'part');
    p.line([x0 + 0.015 * L, fdY, x0 + 0.06 * L, deckY(x0 + 0.06 * L)], 0.8, 'fine');
    p.line([xb - 0.045 * L, fdY, xb - 0.1 * L, deckY(xb - 0.1 * L)], 0.8, 'fine');
    const ia = xb - 0.4 * L, iw = 0.07 * L, ih = f * 1.05;
    p.box(ia, fdY - 4 - ih * 0.55, iw, ih * 0.55, 1.1, 'part');
    p.box(ia + iw * 0.15, fdY - 4 - ih * 0.85, iw * 0.55, ih * 0.3, 1, 'part');
    p.line([ia + iw * 0.4, fdY - 4 - ih * 0.85, ia + iw * 0.4, fdY - 4 - ih * 1.5], 1, 'part');
    p.line([ia + iw * 0.25, fdY - 4 - ih * 1.3, ia + iw * 0.55, fdY - 4 - ih * 1.3], 0.8, 'fine');
    // One trunked stack, wider for every step of speed.
    const sw = 0.022 * L * (1 + 0.45 * speed), sx = ia + iw * 0.55;
    p.line([sx, fdY - 4 - ih * 0.55, sx + 3, fdY - 4 - ih * 1.15, sx + sw + 3, fdY - 4 - ih * 1.15, sx + sw, fdY - 4 - ih * 0.55], 1.1, 'part', true);
    stackAt = [sx + sw / 2 + 2, fdY - 4 - ih * 1.15];
    airAt = [x0 + 0.25 * L, fdY - 4];
    // Guns on a carrier are mounts in the galleries, two forward and two aft at most, never larger than medium.
    if (guns) {
      const at = [x0 + 0.1 * L, xb - 0.14 * L, x0 + 0.2 * L, xb - 0.22 * L].slice(0, turrets);
      for (const x of at) turret(p, x, fdY + 9, Math.min(2, guns), x > W / 2 ? 1 : -1, 0);
      gunsAt = [at[at.length > 1 ? 1 : 0], fdY + 2];
    }
    for (let i = 0; i < aaN; i++) aaAt.push([x0 + 0.08 * L + ((L * 0.82) * (i + 0.5)) / aaN, fdY - 4]);
  } else {
    // A gun ship. Where the turrets go: A and B forward (B firing over A), X and Y aft (X over Y). With half a flight
    // deck aft the guns all go forward of the superstructure, and a third over the flight deck's forward end.
    const fwd: { x: number; lvl: number; dir: 1 | -1 }[] = [{ x: xb - 0.16 * L, lvl: 0, dir: 1 }, { x: xb - 0.25 * L, lvl: 1, dir: 1 }];
    const aft: { x: number; lvl: number; dir: 1 | -1 }[] = half
      ? [{ x: x0 + 0.475 * L, lvl: 1, dir: -1 }, { x: xb - 0.075 * L, lvl: 0, dir: 1 }]
      : [{ x: x0 + 0.13 * L, lvl: 0, dir: -1 }, { x: x0 + 0.22 * L, lvl: 1, dir: -1 }];
    const order = turrets === 1 ? [fwd[0]] : turrets === 2 ? (half ? [fwd[0], fwd[1]] : [fwd[0], aft[0]]) : turrets === 3 ? [fwd[0], fwd[1], aft[0]] : [fwd[0], fwd[1], aft[0], aft[1]];
    const placed = guns ? order : [];
    for (const t of placed) {
      const r = turret(p, t.x, deckY(t.x), guns, t.dir, t.lvl * [0, 5, 7, 9, 11][guns]);
      if (t === fwd[0]) gunsAt = r.top;
    }
    if (placed.length && placed[0] !== fwd[0]) gunsAt = [placed[0].x, deckY(placed[0].x) - 10];

    // The bridge: a tower in tiers behind the forward guns, with a mast and a director on top. Bigger hulls build higher.
    const bx = half ? xb - 0.34 * L : xb - 0.335 * L, bw = (0.05 + 0.006 * hull) * L, by = deckY(bx);
    const tier = f * (0.55 + 0.08 * hull);
    p.box(bx - bw / 2, by - tier, bw, tier, 1.1, 'part');
    p.box(bx - bw * 0.36, by - tier * 1.8, bw * 0.72, tier * 0.8, 1, 'part');
    if (hull >= 2) p.box(bx - bw * 0.22, by - tier * 2.4, bw * 0.44, tier * 0.6, 1, 'part');
    const mastTop = by - tier * (hull >= 2 ? 3.6 : 2.9);
    p.line([bx, by - tier * (hull >= 2 ? 2.4 : 1.8), bx, mastTop], 1, 'part');
    p.line([bx - 9, mastTop + 9, bx + 9, mastTop + 9], 0.8, 'fine');
    p.box(bx - 6, mastTop + 12, 12, 5, 0.8, 'fine');

    // Funnels: one for each step of speed, raked aft, each with its cap.
    const n = speed + 1, fw = (hull === 0 ? 0.034 : 0.028) * L, fh = f * 1.15 + 5 * hull;
    const fx0 = bx - bw / 2 - 0.04 * L;
    for (let i = 0; i < n; i++) {
      const fx = fx0 - i * (fw + 0.03 * L), fy = deckY(fx);
      p.line([fx, fy, fx - 5, fy - fh, fx - 5 - fw, fy - fh, fx - fw, fy], 1.1, 'part', true);
      p.line([fx - 7, fy - fh + 3, fx - 3 - fw, fy - fh + 3], 0.6, 'fine');
      if (i === Math.floor((n - 1) / 2)) stackAt = [fx - 5 - fw / 2, fy - fh];
    }
    const lastFunnel = fx0 - (n - 1) * (fw + 0.03 * L) - fw;

    if (half) {
      // Half a flight deck: the hangar and its deck aft, from the stern to the after turret.
      const ha = x0 + 0.04 * L, hb = x0 + 0.44 * L;
      p.line([ha, deckY(ha), ha, fdY, hb, fdY, hb, deckY(hb)], 1.2, 'part');
      for (let x = ha + 16; x < hb - 16; x += 32) p.line([x, fdY + hangar * 0.35, x + 14, fdY + hangar * 0.35], 0.7, 'fine');
      p.box(x0 + 0.015 * L, fdY - 4, hb - x0 - 0.015 * L + 6, 4, 1.4, 'part');
      airAt = [x0 + 0.2 * L, fdY - 4];
    } else {
      // The after superstructure and mainmast; a destroyer's torpedo tubes amidships.
      const ax = Math.min(lastFunnel - 0.05 * L, x0 + 0.33 * L), aw = 0.05 * L, ay = deckY(ax);
      p.box(ax - aw / 2, ay - f * 0.5, aw, f * 0.5, 1, 'part');
      p.line([ax, ay - f * 0.5, ax, ay - f * 1.9], 0.9, 'part');
      p.line([ax - 6, ay - f * 1.6, ax + 6, ay - f * 1.6], 0.7, 'fine');
      if (hull === 0) for (const tx of [lastFunnel - 0.035 * L, ax + aw / 2 + 0.03 * L]) {
        const ty = deckY(tx);
        p.box(tx - 13, ty - 7, 26, 5, 0.8, 'fine');
        p.line([tx, ty - 2, tx, ty], 0.8, 'fine');
      }
      if (air === 1) {
        // A catapult between the funnels and the after superstructure, her floatplane on it.
        const cx = (lastFunnel + ax + aw / 2) / 2, cy = deckY(cx);
        p.line([cx - 0.045 * L, cy - 4, cx + 0.045 * L, cy - 9], 1.1, 'part');
        p.line([cx - 0.045 * L, cy, cx - 0.045 * L, cy - 4], 0.8, 'fine');
        p.line([cx + 0.045 * L, cy, cx + 0.045 * L, cy - 9], 0.8, 'fine');
        seaplane(p, cx, cy - 15);
        airAt = [cx, cy - 18];
      }
    }
    // The air defences: mounts along the superstructure, between the guns.
    const aaFrom = half ? x0 + 0.5 * L : x0 + 0.27 * L, aaTo = xb - 0.29 * L;
    for (let i = 0; i < aaN; i++) {
      const x = aaFrom + ((aaTo - aaFrom) * (i + 0.5)) / aaN;
      const y = deckY(x) - (i % 2 ? f * 0.25 : 0);
      aaAt.push([x, y]);
    }
  }
  for (const [x, y] of aaAt) aaMount(p, x, y);

  // The callouts, one for each dial, in the margins.
  if (guns) p.callout(gunsAt, `MAIN GUNS · ${TURRET_WORD[turrets]} ${flat ? (turrets === 1 ? 'MOUNT' : 'MOUNTS') : (turrets === 1 ? 'TURRET' : 'TURRETS')} · ${up(words.guns[guns])}`);
  else p.callout([xb - 0.16 * L, deckY(xb - 0.16 * L)], 'NO MAIN GUNS');
  p.callout(stackAt, `MACHINERY · ${up(words.speed[speed])} · ${speed + 1} ${flat ? (speed ? 'BOILER ROOMS' : 'BOILER ROOM') : speed ? 'FUNNELS' : 'FUNNEL'}`);
  if (aaAt.length) p.callout(aaAt[Math.floor(aaAt.length / 2)], `AIR DEFENCE · ${up(words.aa[Math.max(0, Math.min(2, d.aa))])}`);
  if (airAt) p.callout(airAt, up(words.air[air]));
  p.callout(belt, `BELT · ${up(words.armour[Math.max(0, Math.min(2, d.armour))])} ARMOUR`, false);
  p.callout([x0 + 0.06 * L, keel], `HULL · ${up(words.hull[hull])}`, false);
  p.lay();
  titleBlock(p, title, `${up(words.hull[hull])} HULL · ${metres} M`, foot);
  return { ...SHEET, strokes: p.strokes, marks: p.marks };
}

/** A submarine in profile from her card: her length, the tubes in her bow and stern, saddle tanks for range, a hangar. */
export function drawSub(d: SubDials, words: SubWords, title: string, foot: string): Blueprint {
  const p = new Pen();
  const size = Math.max(0, Math.min(2, d.size)), tubes = Math.max(0, Math.min(2, d.tubes)), range = Math.max(0, Math.min(2, d.range)), hangar = Math.max(0, Math.min(1, d.hangar));
  const L = [420, 560, 720][size], x0 = (W - L) / 2, xb = x0 + L;
  const fs = [9, 11, 14][size], ds = [17, 21, 27][size];
  const metres = [62, 85, 120][size];
  const keel = WL + ds;
  paper(p, x0, xb, keel, metres);
  // The pressure hull and its casing: a long spindle, rounded at the bow and drawn out to a point at the stern.
  const deck = WL - fs;
  p.line([
    x0, WL + ds * 0.15, x0 + 0.08 * L, deck, xb - 0.12 * L, deck, xb - 0.04 * L, deck + 2, xb, WL + ds * 0.2,
    xb - 0.015 * L, WL + ds * 0.7, xb - 0.07 * L, keel, x0 + 0.22 * L, keel, x0 + 0.08 * L, WL + ds * 0.6,
  ], 1.8, 'hull', true);
  p.line([x0 - 6, WL + ds * 0.15, x0 + 2, WL + ds * 0.15], 1, 'part');
  p.line([x0 - 4, WL + ds * 0.05, x0 - 4, WL + ds * 0.3], 1, 'part');
  // Saddle tanks for range: a long blister along the side, deeper for a very long reach.
  const tankAt: [number, number] = [x0 + 0.5 * L, WL + ds * 0.55];
  if (range >= 1) {
    const t = range === 2 ? 0.5 : 0.36;
    p.line([x0 + 0.24 * L, WL + ds * 0.3, x0 + 0.3 * L, WL + ds * (0.3 - t * 0.4), xb - 0.3 * L, WL + ds * (0.3 - t * 0.4), xb - 0.24 * L, WL + ds * 0.3, xb - 0.3 * L, WL + ds * (0.3 + t), x0 + 0.3 * L, WL + ds * (0.3 + t)], 1, 'part', true);
    for (let x = x0 + 0.33 * L; x < xb - 0.33 * L; x += 40) p.line([x, WL + ds * 0.3, x, WL + ds * (0.3 + t * 0.8)], 0.5, 'fine');
    tankAt[1] = WL + ds * (0.3 + t);
  } else p.line([x0 + 0.28 * L, WL + ds * 0.42, xb - 0.28 * L, WL + ds * 0.42], 0.7, 'fine');
  // The conning tower, its bridge and periscopes.
  const cx = x0 + 0.5 * L, cw = 0.085 * L, ch = [16, 20, 26][size];
  p.line([cx - cw / 2, deck, cx - cw * 0.42, deck - ch, cx + cw * 0.3, deck - ch, cx + cw / 2, deck - ch * 0.55, cx + cw / 2, deck], 1.2, 'part', true);
  p.line([cx - cw * 0.1, deck - ch, cx - cw * 0.1, deck - ch * 2], 1, 'part');
  p.line([cx + cw * 0.12, deck - ch, cx + cw * 0.12, deck - ch * 1.7], 1, 'part');
  // Deck guns: one forward of the tower, and a second aft on a cruiser submarine.
  const gun = (x: number, dir: 1 | -1): void => {
    p.line([x, deck, x, deck - 6], 1, 'part');
    p.line([x - 4, deck - 6, x + 4, deck - 6], 1, 'part');
    p.line([x, deck - 7, x + dir * (14 + 6 * size), deck - 9], 1.1, 'part');
  };
  gun(cx + cw / 2 + (hangar ? 0.17 : 0.07) * L, 1);
  if (size === 2) gun(cx - cw / 2 - 0.08 * L, -1);
  // A hangar forward of the tower, her seaplane folded inside, and a catapult on the forecasing.
  let hangarAt: [number, number] | null = null;
  if (hangar) {
    const ha = cx + cw / 2, hb = ha + 0.11 * L, hh = ch * 0.62;
    p.line([ha, deck, ha, deck - hh, hb - hh * 0.5, deck - hh, hb, deck - hh * 0.5, hb, deck], 1.1, 'part', true);
    p.line([hb + 4, deck - 2, xb - 0.16 * L, deck - 5], 1, 'part');
    seaplane(p, (ha + hb) / 2, deck - hh * 0.5, 0.7);
    hangarAt = [(ha + hb) / 2, deck - hh];
  }
  // The tubes: doors at the bow, and in the stern when she carries six or eight.
  const bow = [4, 4, 6][tubes], stern = [0, 2, 2][tubes];
  for (let i = 0; i < bow; i++) {
    const y = WL + ds * (0.25 + (0.45 * (i % Math.ceil(bow / 2))) / Math.max(1, Math.ceil(bow / 2) - 1 || 1));
    const x = xb - 0.03 * L - (i >= Math.ceil(bow / 2) ? 10 : 0);
    p.box(x - 7, y - 2, 7, 4, 0.7, 'fine');
  }
  for (let i = 0; i < stern; i++) p.box(x0 + 0.06 * L + i * 9, WL + ds * 0.35, 7, 4, 0.7, 'fine');
  const tubeAt: [number, number] = [xb - 0.03 * L - 4, WL + ds * 0.3];

  p.callout(tubeAt, `TORPEDO TUBES · ${up(words.tubes[tubes])}`);
  p.callout([cx, deck - ch * 2], `${up(words.size[size])}`);
  if (hangarAt) p.callout(hangarAt, up(words.hangar[hangar]));
  p.callout(tankAt, `RANGE · ${up(words.range[range])}${range ? ' · SADDLE TANKS' : ''}`, false);
  p.lay();
  titleBlock(p, title, `${up(words.size[size])} · ${metres} M`, foot);
  return { ...SHEET, strokes: p.strokes, marks: p.marks };
}

/** The sheet as an SVG drawing, for the browser and for checking the drawing by eye. */
export function blueprintSvg(b: Blueprint, scale = 1): string {
  const ink: Record<Ink, string> = { hull: '#f2f6ff', part: '#dfe8fb', fine: '#b8c8ea', water: '#7f9bd0', paper: '#8aa6dd', note: '#ffe9a8' };
  const out: string[] = [`<svg xmlns="http://www.w3.org/2000/svg" width="${b.w * scale}" height="${b.h * scale}" viewBox="0 0 ${b.w} ${b.h}">`, `<rect width="${b.w}" height="${b.h}" fill="#1d3f7a"/>`];
  for (let x = 20; x < b.w; x += 20) out.push(`<line x1="${x}" y1="0" x2="${x}" y2="${b.h}" stroke="#2a4f8f" stroke-width="${x % 100 ? 0.4 : 0.9}"/>`);
  for (let y = 20; y < b.h; y += 20) out.push(`<line x1="0" y1="${y}" x2="${b.w}" y2="${y}" stroke="#2a4f8f" stroke-width="${y % 100 ? 0.4 : 0.9}"/>`);
  for (const s of b.strokes) {
    const pts: string[] = [];
    for (let i = 0; i + 1 < s.pts.length; i += 2) pts.push(`${s.pts[i]},${s.pts[i + 1]}`);
    out.push(`<${s.shut ? 'polygon' : 'polyline'} points="${pts.join(' ')}" fill="none" stroke="${ink[s.ink]}" stroke-width="${s.w}" stroke-linejoin="round"/>`);
  }
  for (const m of b.marks) {
    const anchor = m.align === 'l' ? 'start' : m.align === 'r' ? 'end' : 'middle';
    out.push(`<text x="${m.x}" y="${m.y + (m.big ? 15 : 11)}" fill="${m.big ? '#ffffff' : '#e6eeff'}" font-family="Courier New, monospace" font-size="${m.big ? 15 : 11}" font-weight="${m.big ? 700 : 400}" text-anchor="${anchor}">${m.text.replace(/&/g, '&amp;').replace(/</g, '&lt;')}</text>`);
  }
  out.push('</svg>');
  return out.join('');
}
