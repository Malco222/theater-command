// One frame of a fleet action: the open sea under the camera, each piece's ships side by side along its row in their nations'
// colours with their wakes, the guns that are firing, strikes and raids landing, ships going down, the player's orders
// and what is selected. Everything is read from the sim: this file decides nothing.
// TODO(polish): the browser's stand-in for the 3D sea (the art director's sea art list); the 3D game draws the same
// view with the models (figures, act, speed, engaged are there for it).

import { turn } from '../core/geometry';
import type { FieldEvent, Side } from '../core/types';
import { SEA } from '../sea/numbers';
import type { SeaSim } from '../sea/sim';
import type { SeaClass, SeaTicket, SeaUnit } from '../sea/types';
import { toField, toScreen, type Scene } from './draw';

const BRASS = '#e0b872';
const ALARM = '#c4553f';
const AMBER = '#f0a63a';
const FOAM = 'rgba(246,248,248,';
/** The campaign map's own sea tone (the art director: the sea as the campaign shows it, as the land page takes its paper). */
const SEA_TONE = [157, 178, 189];
const INK = '#2d241a';
const PAPER = '#f1e9d4';
/** The staff's pencil, for what stands ordered and the sea's own labels. */
const PENCIL = 'rgba(42,45,47,.6)';
/** Downwind, for smoke: toward the east-north-east, in field metres a battle second. */
const WIND: [number, number] = [5.5, 2.2];

// ---- Side colours: two fills a battle can tell apart (the art director's review, 2) ---------------------------------

const lin = (c: number): number => { const v = c / 255; return v <= 0.04045 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4; };
const unlin = (v: number): number => Math.round(255 * Math.max(0, Math.min(1, v <= 0.0031308 ? 12.92 * v : 1.055 * v ** (1 / 2.4) - 0.055)));
const fLab = (t: number): number => (t > 216 / 24389 ? Math.cbrt(t) : (24389 / 27 * t + 16) / 116);
const fInv = (t: number): number => (t ** 3 > 216 / 24389 ? t ** 3 : (116 * t - 16) / (24389 / 27));
const WHITE = [0.95047, 1, 1.08883];
const rgbOf = (hex: string): [number, number, number] => { const n = parseInt(hex.replace('#', ''), 16); return [(n >> 16) & 255, (n >> 8) & 255, n & 255]; };
function labOf([R, G, B]: [number, number, number]): [number, number, number] {
  const [r, g, b] = [lin(R), lin(G), lin(B)];
  const X = (0.4124564 * r + 0.3575761 * g + 0.1804375 * b) / WHITE[0], Y = 0.2126729 * r + 0.7151522 * g + 0.072175 * b, Z = (0.0193339 * r + 0.119192 * g + 0.9503041 * b) / WHITE[2];
  const [fx, fy, fz] = [fLab(X), fLab(Y), fLab(Z)];
  return [116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz)];
}
const toLab = (hex: string): [number, number, number] => labOf(rgbOf(hex));
function fromLab([L, a, b]: [number, number, number]): string {
  const fy = (L + 16) / 116, fx = fy + a / 500, fz = fy - b / 200;
  const [X, Y, Z] = [fInv(fx) * WHITE[0], fInv(fy), fInv(fz) * WHITE[2]];
  const r = 3.2404542 * X - 1.5371385 * Y - 0.4985314 * Z, g = -0.969266 * X + 1.8760108 * Y + 0.041556 * Z, bl = 0.0556434 * X - 0.2040259 * Y + 1.0572252 * Z;
  return `#${[unlin(r), unlin(g), unlin(bl)].map((v) => v.toString(16).padStart(2, '0')).join('')}`;
}

/** CIEDE2000, the difference between two colours as an eye sees it. */
function de2000(c1: [number, number, number], c2: [number, number, number]): number {
  const [L1, a1, b1] = labOf(c1), [L2, a2, b2] = labOf(c2);
  const rad = Math.PI / 180, deg = 180 / Math.PI;
  const Cb = (Math.hypot(a1, b1) + Math.hypot(a2, b2)) / 2;
  const G = 0.5 * (1 - Math.sqrt(Cb ** 7 / (Cb ** 7 + 25 ** 7)));
  const a1p = (1 + G) * a1, a2p = (1 + G) * a2;
  const C1p = Math.hypot(a1p, b1), C2p = Math.hypot(a2p, b2);
  const h1p = ((Math.atan2(b1, a1p) * deg) % 360 + 360) % 360, h2p = ((Math.atan2(b2, a2p) * deg) % 360 + 360) % 360;
  const dLp = L2 - L1, dCp = C2p - C1p;
  let dhp = h2p - h1p;
  if (C1p * C2p === 0) dhp = 0; else if (dhp > 180) dhp -= 360; else if (dhp < -180) dhp += 360;
  const dHp = 2 * Math.sqrt(C1p * C2p) * Math.sin((dhp / 2) * rad);
  const Lbp = (L1 + L2) / 2, Cbp = (C1p + C2p) / 2;
  const hbp = C1p * C2p === 0 ? h1p + h2p : Math.abs(h1p - h2p) <= 180 ? (h1p + h2p) / 2 : h1p + h2p < 360 ? (h1p + h2p + 360) / 2 : (h1p + h2p - 360) / 2;
  const T = 1 - 0.17 * Math.cos((hbp - 30) * rad) + 0.24 * Math.cos(2 * hbp * rad) + 0.32 * Math.cos((3 * hbp + 6) * rad) - 0.2 * Math.cos((4 * hbp - 63) * rad);
  const dth = 30 * Math.exp(-(((hbp - 275) / 25) ** 2));
  const Rc = 2 * Math.sqrt(Cbp ** 7 / (Cbp ** 7 + 25 ** 7));
  const Sl = 1 + (0.015 * (Lbp - 50) ** 2) / Math.sqrt(20 + (Lbp - 50) ** 2), Sc = 1 + 0.045 * Cbp, Sh = 1 + 0.015 * Cbp * T;
  const Rt = -Math.sin(2 * dth * rad) * Rc;
  return Math.sqrt((dLp / Sl) ** 2 + (dCp / Sc) ** 2 + (dHp / Sh) ** 2 + Rt * (dCp / Sc) * (dHp / Sh));
}

/** How a colour looks to a deuteranope or a protanope (Viénot, Brettel and Mollon). */
const CVD: Record<'deutan' | 'protan', number[][]> = {
  deutan: [[0.29275, 0.70725, 0], [0.29275, 0.70725, 0], [-0.02234, 0.02234, 1]],
  protan: [[0.11238, 0.88762, 0], [0.11238, 0.88762, 0], [0.00401, -0.00401, 1]],
};
function seenBy(rgb: [number, number, number], k: 'deutan' | 'protan'): [number, number, number] {
  const l = rgb.map(lin);
  return CVD[k].map((row) => unlin(row[0] * l[0] + row[1] * l[1] + row[2] * l[2])) as [number, number, number];
}
/** The least of the three differences: to a normal eye, a deuteranope's and a protanope's. */
const leastApart = (A: [number, number, number], B: [number, number, number]): number =>
  Math.min(de2000(A, B), de2000(seenBy(A, 'deutan'), seenBy(B, 'deutan')), de2000(seenBy(A, 'protan'), seenBy(B, 'protan')));

/**
 * The two sides' fills for one battle (the art director's side colour rule, shared with the land page): each nation's
 * own colour, unless the pair stands under 20 apart (CIEDE2000) to a normal eye, a deuteranope or a protanope; then the
 * lighter is raised in L* only, half a step at a time, until all three are 20 or more. Britain against Germany keeps
 * #b5574a and #59616b; Italy against Britain comes to #92c495; France against Germany to #5f84c7.
 */
export function sideColours(att: string, def: string): Record<Side, string> {
  const A = rgbOf(att), D = rgbOf(def);
  if (leastApart(A, D) >= 20) return { att, def };
  const attLighter = labOf(A)[0] >= labOf(D)[0];
  const [L, a, b] = labOf(attLighter ? A : D), other = attLighter ? D : A;
  for (let dL = 0.5; dL <= 40; dL += 0.5) {
    const c = fromLab([L + dL, a, b]);
    if (leastApart(rgbOf(c), other) >= 20) return attLighter ? { att: c, def } : { att, def: c };
  }
  return { att, def };
}

/** A mark on the water that fades: a ship going down, a strike landing, a raid. 'at' is real seconds, 'bt' battle seconds. */
export interface Mark { x: number; y: number; at: number; bt: number; kind: 'sunk' | 'strike' | 'air' | 'burning'; unit?: number; face?: number }

/**
 * One piece as the frame draws it, in screen pixels: its ships side by side along its row, a hull's length apart, each
 * turned to the piece's heading (the UI director, keep-together 1: line abreast heading for the enemy, line ahead
 * broadside on, on the same row either way), and the piece moved along its row, or its row moved, as far as it takes
 * for every hull to have its own water (the art director, keep-together 1). The spacing is the drawing's, not the sim's.
 */
export interface DrawnPiece {
  id: number;
  /** A hull's length and beam on the screen, and the heading every ship of the piece is turned to. */
  L: number;
  B: number;
  face: number;
  /** Each ship's centre on the screen. */
  ships: [number, number][];
  /** The piece's centre on the screen, and half its extent along its row and across it (its hulls, not the water round them). */
  x: number;
  y: number;
  ha: number;
  hb: number;
}

/** The frame's drawing of the pieces: what the hulls, the picking and the framing all read. */
export interface SeaLayout {
  /** The rows' direction on the screen, and across them toward the defender's fleet (unit vectors). */
  ra: [number, number];
  rb: [number, number];
  pieces: Map<number, DrawnPiece>;
}

/** What the frame keeps between frames: the marks, the strikes in the air, and the sides' fills for this battle. */
export interface SeaScene extends Scene {
  marks: Mark[];
  /** Carriers' strikes in the air: the target, and when they land (battle seconds). */
  inbound: { target: number; lands: number }[];
  colours?: Record<Side, string>;
  /** The foot of what stands over the top of the sea (the dispatch, the warning), in pixels from its top. */
  clearTop?: number;
  /** The last frame's drawing of the pieces. */
  layout?: SeaLayout;
  /** What the drawing keeps from frame to frame: the rows' angle, and each piece's drawn offset from where it truly is
   *  (now and as last worked out), so a piece that has to change places slides there instead of jumping. */
  keep?: { angle: number; off: Map<number, { cur: [number, number]; tgt: [number, number] }>; z: number; at: number };
}

/** How long an oil slick lies on the water, in battle seconds (the art director: fading over a few battle minutes). */
const SLICK_FOR = 240;

export const live = (u: SeaUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';

/** A carrier's aircraft when its wings are full, from the ticket: its fleet's aircraft shared over its fleet's carriers,
 *  each carrier sailing alone (as the sim counts a strike). 0 for any other ship, or a carrier with no wing aboard. */
export function aircraftOf(t: SeaTicket, u: SeaUnit): number {
  if (u.extra.cls !== 'cv') return 0;
  const f = t.place.fleets.find((x) => x.id === u.corps);
  return f && f.ships.cv > 0 ? Math.round(f.bombers / f.ships.cv) : 0;
}

/** A carrier's aircraft still able to fly, once worn; null while they are whole, or for a ship that carries none. Counted
 *  as its strike counts the bombers it flies off (sim.ts strike: its aircraft at full strength times its output over its
 *  whole weight), so the card and the dispatch never give two numbers: what the sim's wear has left of its aircraft
 *  (extra.planes, worn with every blow on its fleet) and, once the carrier itself is struck, of its deck (its hull, down
 *  to fire.damaged for a wreck). "None" only when none at all are left. */
export function aircraftLeft(t: SeaTicket, u: SeaUnit): { left: number; full: number } | null {
  const full = aircraftOf(t, u);
  if (full <= 0) return null;
  const n0 = Math.max(1, u.extra.ships0), afloat = u.extra.ships, deck = u.extra.planes ?? 1, D = SEA.fire.damaged;
  const share = afloat <= 0 ? 0 : (deck * (afloat * D + (1 - D) * u.str * n0)) / n0;
  const left = deck <= 0 || share <= 0 ? 0 : Math.max(1, Math.min(full, Math.round(full * share)));
  return left < full ? { left, full } : null;
}

/** A piece's name as every place shows it, numbered in kind, with what it has lost: "Cruiser 2", "Destroyers 4 · 3 of 4",
 *  and a carrier's aircraft once they are worn, counted as ships are: "Carrier 1 · 18 aircraft", "Carrier 1 · no aircraft".
 *  The count shows only while some ships of a piece of more than one are still afloat: a piece with none left stands
 *  bare, "Cruiser 1", "Destroyers 2", and its state says "Lost" (the UI director, 2b1f0e3), never "0 of 1". */
export const shownName = (u: SeaUnit, t?: SeaTicket): string => {
  if (u.extra.ships <= 0) return u.name;
  const air = t ? aircraftLeft(t, u) : null;
  return `${u.name}${u.extra.ships0 > 1 && u.extra.ships < u.extra.ships0 ? ` · ${u.extra.ships} of ${u.extra.ships0}` : ''}${air ? ` · ${air.left ? `${air.left} aircraft` : 'no aircraft'}` : ''}`;
};

/** What a carrier's worn aircraft mean, for the hover on its name (the UI director, keep-together). */
export function aircraftSaid(t: SeaTicket, u: SeaUnit): string {
  const air = aircraftLeft(t, u);
  if (!air) return '';
  return air.left ? `${air.left} of its ${air.full} aircraft can still fly.` : 'None of its aircraft can fly: it strikes no more in this action.';
}

const STATE_SAID: Record<string, string> = { wavering: 'Wavering', routing: 'Breaking off' };
/** The hover over a ship, in the land's form: whose, what, its fleet, its hull, and for an enemy what a right-click does
 *  when ships are in hand (the UI director): "German Cruiser 2, Battle Fleet · Hull 99 · Right-click to engage". With
 *  nothing in hand a right-click does nothing, so the hover does not offer it. */
export function hoverText(sim: SeaSim, u: SeaUnit, me: Side, inHand = 0): string {
  const bits = [`${u.side === me ? '' : `${sim.ticket[u.side].adj} `}${shownName(u, sim.ticket)}, ${u.extra.fleet}`, `Hull ${Math.round(u.str * 100)}`];
  if (STATE_SAID[u.state]) bits.push(STATE_SAID[u.state]);
  if (inHand > 0 && u.side !== me && u.state !== 'routing') bits.push('Right-click to engage');
  return bits.join(' · ');
}

/** Where a piece is drawn, in field metres: where a mark made for it now should lie. Its true place if it is not drawn. */
function drawnAt(s: SeaScene, id: number | undefined, x: number, y: number): [number, number] {
  const p = id !== undefined ? s.layout?.pieces.get(id) : undefined;
  return p ? toField(s, p.x, p.y) : [x, y];
}

/** The marks the frame keeps from the sim's events. */
export function takeEvents(s: SeaScene, sim: SeaSim, events: FieldEvent[], realNow: number): void {
  const bt = sim.t;
  for (const e of events) {
    const face = e.unit !== undefined ? sim.units.find((u) => u.id === e.unit)?.face : undefined;
    // A slick lies where the ship was drawn going down, and stays there as the fleet sails on.
    if (e.what === 'sea:sunk' && e.x !== undefined) { const [mx, my] = drawnAt(s, e.unit, e.x, e.y!); s.marks.push({ x: mx, y: my, at: realNow, bt, kind: 'sunk', face }); }
    if (e.what === 'sea:strike' && e.x !== undefined) s.marks.push({ x: e.x, y: e.y!, at: realNow, bt, kind: 'strike', unit: e.unit });
    if (e.what === 'sea:air' && e.x !== undefined) s.marks.push({ x: e.x, y: e.y!, at: realNow, bt, kind: 'air', unit: e.unit });
    if (e.what === 'sea:burning' && e.unit !== undefined) s.marks.push({ x: e.x ?? 0, y: e.y ?? 0, at: realNow, bt, kind: 'burning', unit: e.unit });
    if (e.what === 'ability' && (e.extra as { key?: string } | undefined)?.key === 'strike') {
      const target = (e.extra as { target?: number }).target;
      if (target !== undefined) s.inbound.push({ target, lands: sim.t + SEA.strike.windup });
    }
  }
  s.inbound = s.inbound.filter((b) => b.lands > sim.t);
  s.marks = s.marks.filter((m) => (m.kind === 'sunk' ? bt - m.bt < SLICK_FOR : m.kind === 'burning' ? bt - m.bt < 120 : realNow - m.at < 6));
}

/** The smallest a ship is drawn, in pixels, by class, so that a whole fleet reads at the widest view (the art director: a sign before it is a size). */
const MIN_LEN: Record<SeaClass, number> = { bb: 20, cv: 20, ca: 16, dd: 12 };
const MIN_BEAM: Record<SeaClass, number> = { bb: 5.4, cv: 5.4, ca: 4.2, dd: 3.2 };
/** Each class's row in the line, counted back from the front, as the fleets draw up (pieces.ts): while a fleet holds its
 *  line each class is drawn in its own band, the lighter toward the enemy (the UI director, keep-together 1). */
const RANK: Record<SeaClass, number> = { dd: 0, ca: 1, bb: 2, cv: 3 };
/** How far round a piece picking reaches, and its outline is drawn, in screen pixels (the UI director, keep-together 2). */
export const PICK = 6;
/** The hull bar shown with the outline: its thickness, and its gap from the hulls, inside the outline. */
const BAR = 3, BAR_GAP = 1.5;
/** The water left between two rows' hulls, in screen pixels: room for a piece's outline on each side, its hull bar and 3
 *  clear, so a held or hovered piece's outline, bar and words never touch the next row (the UI director, keep-together
 *  review). Between a piece's own ships and along a row the gap is a hull's length. LEAST_CLEAR is the least ever drawn
 *  while a piece eases to a new place: 3 clear of the rims once they are drawn (the art director). */
const ROW_CLEAR = 2 * PICK + BAR + 3;
const LEAST_CLEAR = 6;
/** Pieces of one side closer than this in depth, one to the next, in metres, hold one line (about two rows' spacing):
 *  its classes are drawn in bands. A piece further off (a carrier holding off, a ship sent away) is a line of its own. */
const LINE_GAP = 2.5 * SEA.field.row;

/**
 * Pieces sorted by where they want to be along a line, kept in that order with at least sep[i] between the i-th and the
 * next, each moved as little as it can be (least squares): a row drawn longer than it truly is only where its hulls
 * would touch, as the land page spaces its blocks (the art director, keep-together 1).
 */
function spread(want: number[], sep: number[]): number[] {
  const n = want.length, off = new Array<number>(n).fill(0);
  const blocks: { i0: number; i1: number; sum: number; at: number }[] = [];
  for (let i = 0; i < n; i++) {
    let b = { i0: i, i1: i, sum: want[i], at: want[i] };
    while (blocks.length) {
      const p = blocks[blocks.length - 1];
      if (p.at + off[p.i1] + sep[p.i1] <= b.at + 1e-9) break;
      const shift = off[p.i1] + sep[p.i1];
      for (let k = b.i0; k <= b.i1; k++) { off[k] += shift; p.sum += want[k] - off[k]; }
      p.i1 = b.i1;
      p.at = p.sum / (p.i1 - p.i0 + 1);
      blocks.pop();
      b = p;
    }
    blocks.push(b);
  }
  const out = new Array<number>(n);
  for (const b of blocks) for (let k = b.i0; k <= b.i1; k++) out[k] = b.at + off[k];
  return out;
}

/**
 * Lays the pieces out on the screen for this frame. Every fleet's rows run along the fleets' own lines, or square to the
 * line between the two fleets' middles where they are not drawn out in lines. A piece's ships stand side by side along its row, a hull's length apart, so it is never deeper than one hull.
 * While a fleet holds its line (no side has broken off, the piece is not breaking off or leaving), each class of it has
 * its own band of depth, in order toward the enemy: destroyers, cruisers, battleships, carriers, each band a clear row
 * from the next whether or not its pieces overlap along the row, as near its pieces' true depth as that allows. So the
 * line reads as its rows (the UI director, keep-together 1 and its review). In the chase, and for a piece out of the
 * line, pieces of one class at about one depth are a row at their true depth. Along a row the pieces keep their order
 * and are pushed apart, as little as will do, until a hull's length of water lies between them; rows that would still
 * touch are drawn apart across their line.
 */
export function layoutSea(sim: SeaSim, s: SeaScene): SeaLayout {
  const z = s.cam.z;
  const us = sim.units.filter(live);
  const keep = s.keep ?? (s.keep = { angle: NaN, off: new Map(), z, at: s.now });
  const dt = Math.max(0, Math.min(0.25, s.now - keep.at));
  const mid = (side: Side): [number, number] | null => {
    const mine = us.filter((u) => u.side === side);
    if (!mine.length) return null;
    const [x, y] = mine.reduce((a, u) => [a[0] + u.x, a[1] + u.y], [0, 0]);
    return toScreen(s, x / mine.length, y / mine.length);
  };
  const A = mid('att'), D = mid('def');
  let want = keep.angle;
  if (A && D && Math.hypot(D[0] - A[0], D[1] - A[1]) > 1) {
    want = Math.atan2(D[1] - A[1], D[0] - A[0]);
    // The rows lie along the fleets' own lines where they stand in lines: the long axis of the two fleets' spreads about
    // their own middles, taken together. A line of four kilometres drawn square to the line between the middles instead
    // would slant across the screen whenever one fleet stands a little to one side of the other, and its flanks would
    // fall level with the rows behind. Where the fleets are not drawn out in lines, or their lines do not stand between
    // them, the rows run square to the line between the middles.
    let cxx = 0, cxy = 0, cyy = 0;
    for (const u of us) {
      const [mx, my] = u.side === 'att' ? A : D, [X, Y] = toScreen(s, u.x, u.y);
      cxx += (X - mx) ** 2; cxy += (X - mx) * (Y - my); cyy += (Y - my) ** 2;
    }
    const disc = Math.sqrt(((cxx - cyy) / 2) ** 2 + cxy ** 2), l1 = (cxx + cyy) / 2 + disc, l2 = (cxx + cyy) / 2 - disc;
    if (l1 > 3 * Math.max(l2, 1e-6)) {
      let across = 0.5 * Math.atan2(2 * cxy, cxx - cyy) + Math.PI / 2;
      if (Math.cos(across - want) < 0) across += Math.PI;
      if (Math.cos(across - want) >= Math.SQRT1_2) want = across;
    }
  } else if (!Number.isFinite(want)) want = -Math.PI / 2;
  const angle = Number.isFinite(keep.angle) ? keep.angle + turn(keep.angle, want) * (1 - Math.exp(-dt / 0.8)) : want;
  const rb: [number, number] = [Math.cos(angle), Math.sin(angle)], ra: [number, number] = [-rb[1], rb[0]];

  // base: the depth a piece is drawn at before rows are pushed apart: its band's in a line, else its own true depth.
  interface Item { u: SeaUnit; L: number; B: number; ea: number; n: number; ha: number; hb: number; X: number; Y: number; ta: number; tb: number; pa: number; base: number }
  const items: Item[] = us.map((u) => {
    const cls = u.extra.cls as SeaClass, C = SEA.classes[cls];
    const L = Math.max(MIN_LEN[cls], C.len * z), B = Math.max(MIN_BEAM[cls], C.beam * z * 1.6);
    // A hull's extent along the row and across it, as it is turned.
    const c = Math.abs(Math.sin(u.face) * ra[0] - Math.cos(u.face) * ra[1]), q = Math.sqrt(Math.max(0, 1 - c * c));
    const ea = L * c + B * q, eb = L * q + B * c;
    const n = Math.max(1, u.extra.ships);
    const [X, Y] = toScreen(s, u.x, u.y);
    const ta = X * ra[0] + Y * ra[1], tb = X * rb[0] + Y * rb[1];
    return { u, L, B, ea, n, ha: (n * ea + (n - 1) * L) / 2, hb: eb / 2, X, Y, ta, tb, pa: ta, base: tb };
  });

  // Rows. line: a band of a fleet holding its line, drawn straight at one depth.
  interface Row { side: Side; rank: number; items: Item[]; tb: number; db: number; key: number; line: boolean }
  const rows: Row[] = [];
  const near = (SEA.field.row / 2) * z;
  const chase = sim.brokenSide !== null;
  const inLine = (u: SeaUnit): boolean => !chase && u.state !== 'routing' && sim.orderOf(u.id)?.kind !== 'leave';
  for (const side of ['att', 'def'] as Side[]) {
    // The bands of each line: toward the enemy is up the rb axis for the attacker, down it for the defender.
    const toward = side === 'att' ? 1 : -1;
    const lined = items.filter((it) => it.u.side === side && inLine(it.u)).sort((p, q) => p.tb - q.tb);
    const lines: Item[][] = [];
    for (const it of lined) {
      const last = lines[lines.length - 1];
      if (last && it.tb - last[last.length - 1].tb <= LINE_GAP * z) last.push(it); else lines.push([it]);
    }
    for (const line of lines) {
      // Heaviest band first along rb for the attacker (its destroyers nearest the enemy, at the largest depth), lightest
      // first for the defender.
      const ranks = [...new Set(line.map((it) => RANK[it.u.extra.cls as SeaClass]))].sort((a, b) => toward * (b - a));
      const bands = ranks.map((k) => line.filter((it) => RANK[it.u.extra.cls as SeaClass] === k));
      const want = bands.map((b) => b.reduce((a, it) => a + it.tb, 0) / b.length);
      const deep = bands.map((b) => Math.max(...b.map((it) => it.hb)));
      const at = spread(want, bands.map((_, i) => (i + 1 < bands.length ? deep[i] + deep[i + 1] + ROW_CLEAR : 0)));
      bands.forEach((b, i) => {
        for (const it of b) it.base = at[i];
        rows.push({ side, rank: ranks[i], items: b, tb: at[i], db: 0, key: rows.length, line: true });
      });
    }
    // Out of the line: a fleet's pieces of one class at about one depth (closer than half the rows' spacing to the next).
    for (const cls of Object.keys(RANK) as SeaClass[]) {
      const mine = items.filter((it) => it.u.side === side && !inLine(it.u) && it.u.extra.cls === cls).sort((p, q) => p.tb - q.tb);
      let row: Item[] = [];
      const close = (): void => { if (row.length) rows.push({ side, rank: RANK[cls], items: row, tb: row.reduce((a, it) => a + it.tb, 0) / row.length, db: 0, key: rows.length, line: false }); row = []; };
      for (const it of mine) { if (row.length && it.tb - row[row.length - 1].tb > near) close(); row.push(it); }
      close();
    }
  }
  // Along each row: a hull's length of water between neighbours, the same as between a piece's own ships. Neighbours
  // keep the order they were drawn in until they have truly passed each other by half the room it took to part them,
  // so two pieces abreast of each other do not trade places back and forth.
  const lastOff = (it: Item): number => keep.off.get(it.u.id)?.cur[0] ?? 0;
  for (const r of rows) {
    const byA = [...r.items].sort((p, q) => p.ta + lastOff(p) / 2 - (q.ta + lastOff(q) / 2) || p.u.id - q.u.id);
    const at = spread(byA.map((it) => it.ta), byA.map((it, i) => (i + 1 < byA.length ? it.ha + byA[i + 1].ha + Math.max(it.L, byA[i + 1].L) : 0)));
    byA.forEach((it, i) => { it.pa = at[i]; });
  }
  // Across the rows: where two rows' hulls would still touch, they are drawn apart, each half the way, keeping one order
  // across the screen. A line's bands are in order already; a row out of the line keeps its own place, except that
  // within about a row's spacing a fleet's lighter rows come first toward the enemy (the defender's toward the foot of
  // the screen, the attacker's toward its top, as they drew up). Two rows of one fleet that lie alongside each other
  // keep clear depths, so no piece sits in between another row's ships.
  const lean = (R: Row): number => (R.line ? 0 : (R.side === 'att' ? 1 : -1) * (3 - R.rank) * SEA.field.row * z * 0.5);
  rows.sort((R, Q) => R.tb + lean(R) - (Q.tb + lean(Q)) || R.key - Q.key);
  const span = rows.map((R) => [Math.min(...R.items.map((it) => it.pa - it.ha)), Math.max(...R.items.map((it) => it.pa + it.ha))]);
  const banded = rows.map((R, i) => rows.map((Q, j) => R.side === Q.side && span[i][0] < span[j][1] + ROW_CLEAR && span[j][0] < span[i][1] + ROW_CLEAR));
  for (let pass = 0; pass < 120; pass++) {
    let worst = 0;
    // Row i stands before row j (i < j) along rb.
    for (let i = 0; i < rows.length; i++) for (let j = i + 1; j < rows.length; j++) {
      const R = rows[i], Q = rows[j];
      let need = 0;
      for (const p of R.items) for (const q of Q.items) {
        if (!banded[i][j] && Math.abs(p.pa - q.pa) >= p.ha + q.ha + ROW_CLEAR) continue;
        need = Math.max(need, p.hb + q.hb + ROW_CLEAR - (q.base + Q.db - (p.base + R.db)));
      }
      if (need > 0.01) { R.db -= need / 2; Q.db += need / 2; worst = Math.max(worst, need); }
    }
    if (worst < 0.05) break;
  }

  // Each piece's offset from where it truly is. A change of place (a ship lost, a piece joining a row) is eased in over
  // a moment; the rest follows the sea exactly, and a new zoom is drawn at once. Easing never puts one hull on another:
  // a piece that would touch another on its way is drawn at its new place at once (two pieces trading places in a row
  // cannot slide past each other without it).
  const snap = Math.abs(z / keep.z - 1) > 0.03;
  const ease = Math.exp(-dt / 0.2);
  const all = rows.flatMap((r) => r.items.map((it) => {
    const tgt: [number, number] = [it.pa - it.ta, it.base - it.tb + r.db];
    const was = keep.off.get(it.u.id);
    let cur: [number, number] = tgt;
    if (was && !snap) {
      // How far the drawing stands from where it should: after a jump, from the new place; else what is left of the last.
      const from = Math.hypot(tgt[0] - was.tgt[0], tgt[1] - was.tgt[1]) > 4 ? tgt : was.tgt;
      cur = [tgt[0] + (was.cur[0] - from[0]) * ease, tgt[1] + (was.cur[1] - from[1]) * ease];
    }
    return { it, tgt, cur, a: 0, b: 0 };
  }));
  const place = (e: (typeof all)[number]): void => { e.a = e.it.ta + e.cur[0]; e.b = e.it.tb + e.cur[1]; };
  all.forEach(place);
  for (let found = true; found;) {
    found = false;
    for (let i = 0; i < all.length; i++) for (let j = i + 1; j < all.length; j++) {
      const p = all[i], q = all[j];
      if (Math.abs(p.a - q.a) >= p.it.ha + q.it.ha + LEAST_CLEAR || Math.abs(p.b - q.b) >= p.it.hb + q.it.hb + LEAST_CLEAR) continue;
      for (const e of [p, q]) if (e.cur !== e.tgt) { e.cur = e.tgt; place(e); found = true; }
    }
  }
  const pieces = new Map<number, DrawnPiece>();
  const seen = new Set<number>();
  for (const { it, tgt, cur } of all) {
    keep.off.set(it.u.id, { cur, tgt });
    seen.add(it.u.id);
    const x = it.X + ra[0] * cur[0] + rb[0] * cur[1], y = it.Y + ra[1] * cur[0] + rb[1] * cur[1];
    const ships: [number, number][] = [];
    for (let k = 0; k < it.n; k++) { const o = (k - (it.n - 1) / 2) * (it.ea + it.L); ships.push([x + ra[0] * o, y + ra[1] * o]); }
    pieces.set(it.u.id, { id: it.u.id, L: it.L, B: it.B, face: it.u.face, ships, x, y, ha: it.ha, hb: it.hb });
  }
  for (const id of [...keep.off.keys()]) if (!seen.has(id)) keep.off.delete(id);
  keep.angle = angle; keep.z = z; keep.at = s.now;
  return { ra, rb, pieces };
}

/** How far a point is from a hull's outline, in screen pixels (0 inside it). */
function fromHull(dx: number, dy: number, face: number, L: number, B: number): number {
  const along = dx * Math.sin(face) - dy * Math.cos(face), across = dx * Math.cos(face) + dy * Math.sin(face);
  return Math.hypot(Math.max(0, Math.abs(along) - L / 2), Math.max(0, Math.abs(across) - B / 2));
}

/**
 * The piece under a point on the screen: every hull it draws and the water between them, plus PICK pixels each way, as
 * its outline shows (the UI director, keep-together 2). Where two pieces' areas overlap, the one whose hull is nearer wins.
 */
export function pieceAt(s: SeaScene, px: number, py: number, ok: (id: number) => boolean): number | null {
  const lay = s.layout;
  if (!lay) return null;
  let best: number | null = null, bd = Infinity;
  for (const p of lay.pieces.values()) {
    if (!ok(p.id)) continue;
    const dx = px - p.x, dy = py - p.y;
    if (Math.abs(dx * lay.ra[0] + dy * lay.ra[1]) > p.ha + PICK || Math.abs(dx * lay.rb[0] + dy * lay.rb[1]) > p.hb + PICK) continue;
    let d = Infinity;
    for (const [sx, sy] of p.ships) d = Math.min(d, fromHull(px - sx, py - sy, p.face, p.L, p.B) + 1e-3 * Math.hypot(px - sx, py - sy));
    if (d < bd) { bd = d; best = p.id; }
  }
  return best;
}

/** The pieces with a ship inside a box on the screen. */
export function piecesIn(s: SeaScene, x0: number, y0: number, x1: number, y1: number, ok: (id: number) => boolean): number[] {
  const [lx, hx] = x0 < x1 ? [x0, x1] : [x1, x0], [ly, hy] = y0 < y1 ? [y0, y1] : [y1, y0];
  return [...(s.layout?.pieces.values() ?? [])].filter((p) => ok(p.id) && p.ships.some(([sx, sy]) => sx >= lx && sx <= hx && sy >= ly && sy <= hy)).map((p) => p.id);
}

/** Half a drawn piece's width and height on the screen, as it lies (its row may run at a slant). */
const halfBox = (lay: SeaLayout, p: DrawnPiece, pad = 0): [number, number] =>
  [Math.abs(lay.ra[0]) * (p.ha + pad) + Math.abs(lay.rb[0]) * (p.hb + pad), Math.abs(lay.ra[1]) * (p.ha + pad) + Math.abs(lay.rb[1]) * (p.hb + pad)];

/** The guns' rhythm is the class's flavour and nothing more: battleships fire slow, heavy salvos with a big flash and tall splashes,
 *  destroyers quick, light ones; every class does the same harm per unit of weight each minute (the design
 *  director, b415). Look only: the reckoning never reads this table. */
const SALVO: Record<string, { every: number; flash: number; streak: number; splashes: number; rise: number }> = {
  bb: { every: 3.2, flash: 5.5, streak: 2.1, splashes: 4, rise: 6.5 },
  ca: { every: 2.4, flash: 3.5, streak: 1.4, splashes: 3, rise: 4 },
  dd: { every: 1.6, flash: 2.4, streak: 1, splashes: 2, rise: 2.8 },
};
/** The width of our ships' ink rim and of the paper line inside it, each, in pixels. */
const RIM = 0.75;

/** One hull, centred on the origin and pointing up the screen (north), with its class's marks. */
function hull(x: CanvasRenderingContext2D, cls: SeaClass, L: number, B: number, fill: string, inner: string | null): void {
  x.beginPath();
  if (cls === 'cv') {
    // A flat deck: square at the stern, a short rake at the bow.
    x.moveTo(-B / 2, L / 2); x.lineTo(-B / 2, -L * 0.36); x.lineTo(0, -L / 2); x.lineTo(B / 2, -L * 0.36); x.lineTo(B / 2, L / 2); x.closePath();
  } else {
    x.moveTo(0, -L / 2);
    x.quadraticCurveTo(B / 2, -L * 0.18, B / 2, L * 0.28);
    x.lineTo(B * 0.32, L / 2); x.lineTo(-B * 0.32, L / 2); x.lineTo(-B / 2, L * 0.28);
    x.quadraticCurveTo(-B / 2, -L * 0.18, 0, -L / 2);
  }
  // Every hull keeps a dark ink rim, so a pale navy never sinks into the pale sea; ours carry a pale line inside it
  // (amber when wavering, red when breaking off), theirs the ink alone, so the sides read without colour. Ours are
  // drawn outside the hull, each line as thin as the other, and the fill laid over their inner halves, so the nation's
  // colour keeps the hull's whole beam: at least half the width a destroyer shows (the art director, round 2).
  x.strokeStyle = INK;
  if (inner) {
    x.lineWidth = 2 * (RIM + RIM); x.stroke();
    x.lineWidth = 2 * RIM; x.strokeStyle = inner; x.stroke();
    x.fillStyle = fill; x.fill();
  } else { x.fillStyle = fill; x.fill(); x.lineWidth = 1.3; x.stroke(); }
  // The class's marks: a battleship's turrets fore and aft, a cruiser's, a carrier's island and deck line.
  x.fillStyle = 'rgba(241,233,212,.85)';
  const dot = (yy: number, r: number): void => { x.beginPath(); x.arc(0, yy, r, 0, Math.PI * 2); x.fill(); };
  if (cls === 'bb') { dot(-L * 0.22, B * 0.24); dot(-L * 0.08, B * 0.2); dot(L * 0.24, B * 0.24); }
  else if (cls === 'ca') { dot(-L * 0.2, B * 0.22); dot(L * 0.22, B * 0.22); }
  else if (cls === 'cv') {
    x.fillRect(B * 0.18, -L * 0.06, B * 0.22, L * 0.18);
    x.strokeStyle = 'rgba(241,233,212,.55)'; x.lineWidth = 0.8; x.setLineDash([2, 2]);
    x.beginPath(); x.moveTo(-B * 0.1, -L * 0.3); x.lineTo(-B * 0.1, L * 0.42); x.stroke(); x.setLineDash([]);
  }
}

/** A drawn piece's outline: its hulls and the water between them, PICK pixels each way, along its row. */
function outline(x: CanvasRenderingContext2D, lay: SeaLayout, p: DrawnPiece): void {
  const a = p.ha + PICK, b = p.hb + PICK, r = Math.min(4, b);
  x.save(); x.translate(p.x, p.y); x.transform(lay.ra[0], lay.ra[1], lay.rb[0], lay.rb[1], 0, 0);
  x.beginPath();
  x.moveTo(-a + r, -b); x.lineTo(a - r, -b); x.arcTo(a, -b, a, -b + r, r); x.lineTo(a, b - r); x.arcTo(a, b, a - r, b, r);
  x.lineTo(-a + r, b); x.arcTo(-a, b, -a, b - r, r); x.lineTo(-a, -b + r); x.arcTo(-a, -b, -a + r, -b, r); x.closePath();
  x.restore();
  x.stroke();
}

/** A piece's hull bar: a short meter along its row, just past its hulls on the side lower on the screen, inside its
 *  outline and as wide as the outline allows up to 24 (the UI director, keep-together review: on the piece, never under
 *  the next row's ships). */
function hullBar(x: CanvasRenderingContext2D, lay: SeaLayout, p: DrawnPiece, str: number): void {
  const sa = lay.ra[0] < 0 ? -1 : 1, sb = lay.rb[1] < 0 ? -1 : 1;
  const w = Math.min(24, 2 * (p.ha + PICK) - 4), shown = Math.round(str * 100);
  x.save(); x.translate(p.x, p.y); x.transform(sa * lay.ra[0], sa * lay.ra[1], sb * lay.rb[0], sb * lay.rb[1], 0, 0);
  x.fillStyle = 'rgba(14,11,8,.7)'; x.fillRect(-w / 2, p.hb + BAR_GAP, w, BAR);
  x.fillStyle = shown < 35 ? ALARM : shown < 65 ? AMBER : '#5f9a52'; x.fillRect(-w / 2, p.hb + BAR_GAP, (w * shown) / 100, BAR);
  x.restore();
}

/**
 * Where a line of words w by h (x its middle, y its foot) stands over open water: at (cx, foot) if that is clear of every
 * block, else the nearest clear place above it, below the piece (whose half box is hw by hh round cx, its middle
 * foot + 3 + hh below), or past either end of it, kept on the sea between top and the foot of the screen.
 */
function openWater(cx: number, foot: number, w: number, h: number, hw: number, hh: number, blocks: [number, number, number, number][], top: number, W: number, H: number, hard: [number, number, number, number][] = []): [number, number] {
  const fitX = (v: number): number => Math.max(w / 2 + 6, Math.min(W - w / 2 - 6, v));
  const fitY = (v: number): number => Math.max(top + h, Math.min(H - 4, v));
  const off = (bs: [number, number, number, number][], lx: number, ly: number): boolean => !bs.some(([a, b, c, d]) => lx - w / 2 < c && lx + w / 2 > a && ly - h < d && ly > b);
  const clear = (lx: number, ly: number): boolean => off(blocks, lx, ly);
  const mid = foot + 3 + hh;
  const tries: [number, number, number][] = [];
  for (let d = 0; d <= 240; d += 3) {
    tries.push([cx, foot - d, d]);
    tries.push([cx, mid + hh + 3 + h + d, 2 * hh + h + 6 + d]);
  }
  for (let d = 0; d <= 120; d += 3) for (const k of [-1, 1]) tries.push([cx + k * (hw + 4 + w / 2 + d), mid + h / 2, hh + h / 2 + hw + w / 2 + d]);
  tries.sort((p, q) => p[2] - q[2]);
  for (const [tx, ty] of tries) { const lx = fitX(tx), ly = fitY(ty); if (clear(lx, ly)) return [lx, ly]; }
  // Where no open water is near (a crowded sea, the view far out), the nearest place off the 'hard' blocks at least: a
  // STRIKE chip and the other line of words are never written over (b458, 6), though a hull may be.
  for (const [tx, ty] of tries) { const lx = fitX(tx), ly = fitY(ty); if (off(hard, lx, ly)) return [lx, ly]; }
  return [fitX(cx), fitY(foot)];
}

export function drawSea(x: CanvasRenderingContext2D, sim: SeaSim, s: SeaScene): void {
  const { cam, W, H } = s;
  const F = SEA.field;
  const lay = s.layout = layoutSea(sim, s);
  const at = (id: number): DrawnPiece | undefined => lay.pieces.get(id);
  x.save();
  // Open water in the campaign map's sea tone, a shade deeper toward the edges, on the dark desk as the land page's
  // field is (the art director's correction, keep-together, 03:16); the ruled squares are a kilometre each, in ink at
  // low strength, for judging the range.
  x.fillStyle = '#0e0b08'; x.fillRect(0, 0, W, H);
  const [x0, y0] = toScreen(s, 0, F.size), [x1, y1] = toScreen(s, F.size, 0);
  const [r, g0, b0] = SEA_TONE;
  const g = x.createRadialGradient((x0 + x1) / 2, (y0 + y1) / 2, 0, (x0 + x1) / 2, (y0 + y1) / 2, Math.max(x1 - x0, y1 - y0) * 0.75);
  g.addColorStop(0, `rgb(${r + 6},${g0 + 6},${b0 + 5})`); g.addColorStop(1, `rgb(${r - 14},${g0 - 14},${b0 - 12})`);
  x.fillStyle = g; x.fillRect(x0, y0, x1 - x0, y1 - y0);
  x.strokeStyle = 'rgba(45,36,26,.09)'; x.lineWidth = 1;
  x.beginPath();
  for (let m = 1000; m < F.size; m += 1000) {
    const [a] = toScreen(s, m, 0), [, b] = toScreen(s, 0, m);
    x.moveTo(a, y0); x.lineTo(a, y1); x.moveTo(x0, b); x.lineTo(x1, b);
  }
  x.stroke();
  // The swell: short strokes that drift slowly, so a still sea is not a dead one; kept to the board.
  x.save();
  x.beginPath(); x.rect(x0, y0, x1 - x0, y1 - y0); x.clip();
  x.strokeStyle = 'rgba(246,248,248,.22)'; x.lineWidth = 1.2;
  x.beginPath();
  const drift = (sim.t * 2) % 1400;
  for (let gy = 700; gy < F.size; gy += 1400) for (let gx = 300 + ((gy / 1400) % 2) * 700; gx < F.size; gx += 1400) {
    const [a, b] = toScreen(s, gx + drift * 0.3, gy + drift * 0.1);
    const l = Math.max(4, 240 * cam.z);
    if (a < -l || a > W + l || b < -l || b > H + l) continue;
    x.moveTo(a - l, b); x.quadraticCurveTo(a, b - l * 0.18, a + l, b);
  }
  x.stroke();
  x.restore();
  // Each side's edge, by which its ships leave for port: at the left end, clear of the dispatch (the UI director, 14).
  x.font = '700 12px "Courier Prime", monospace'; x.fillStyle = PENCIL; x.textAlign = 'left'; x.textBaseline = 'alphabetic';
  // The far edge's name sits below the dispatch strip, nothing over it (the UI director, sea-226 mend 2).
  x.fillText(`${sim.ticket.def.adj.toUpperCase()} PORTS`, Math.max(12, x0 + 10), Math.max((s.clearTop ?? 50) + 16, y0 + 18));
  x.fillText(`${sim.ticket.att.adj.toUpperCase()} PORTS`, Math.max(12, x0 + 10), Math.min(H - 8, y1 - 8));

  const colour: Record<Side, string> = s.colours ?? { att: sim.ticket.att.color, def: sim.ticket.def.color };
  const byId = new Map(sim.units.map((u) => [u.id, u]));

  // Reach for one ship only, so a fleet in hand does not bury the sea (the UI director, sea newcomer 1): the one alone
  // in hand, or the one of those in hand under the pointer. One ring, the furthest its guns hit at all, named once at
  // its top edge: the guns hit as hard anywhere within it (one reach, b415), so there is no nearer ring to show.
  const ringId = s.sel.size === 1 ? [...s.sel][0] : s.hover !== null && s.hover !== undefined && s.sel.has(s.hover) ? s.hover : null;
  const ringOn = ringId !== null ? byId.get(ringId) : undefined;
  const ringAt = ringOn ? at(ringOn.id) : undefined;
  if (ringOn && ringAt && live(ringOn) && ringOn.extra.cls !== 'cv') {
    const C = SEA.classes[ringOn.extra.cls as SeaClass];
    x.strokeStyle = 'rgba(138,107,59,.75)'; x.lineWidth = 1.2;
    x.setLineDash([5, 6]); x.beginPath(); x.arc(ringAt.x, ringAt.y, C.reach * cam.z, 0, Math.PI * 2); x.stroke(); x.setLineDash([]);
    ringWord(x, 'REACH', ringAt.x, ringAt.y - C.reach * cam.z);
  }

  // Orders of the player's own ships, as on the land page: the lines of those in hand in the selection gold, the rest
  // in the staff's pencil; never red. An attack ends in a small head at its target, a course in a dot. A ship in hand
  // that is firing on its own choice shows its target the same way; no other ship draws a standing line.
  for (const u of sim.units) {
    const p = at(u.id);
    if (u.side !== s.me || !live(u) || !p) continue;
    const o = sim.orderOf(u.id);
    const held = s.sel.has(u.id);
    let end: [number, number] | null = null, attack = false;
    const drawnTarget = (id: number): [number, number] | null => { const t = byId.get(id), q = at(id); return t && live(t) && q ? [q.x, q.y] : null; };
    if (o?.kind === 'move') end = toScreen(s, o.x, o.y);
    else if (o?.kind === 'attack') { end = drawnTarget(o.target); attack = !!end; }
    if (!end && held && u.engaged !== undefined) { end = drawnTarget(u.engaged); attack = !!end; }
    if (!end) continue;
    const ink = held ? BRASS : PENCIL;
    x.strokeStyle = ink; x.fillStyle = ink;
    x.lineWidth = held ? 2.2 : 1.2; x.setLineDash(held ? [7, 5] : [5, 4]);
    const [a, b] = [p.x, p.y], [ex, ey] = end;
    x.beginPath(); x.moveTo(a, b); x.lineTo(ex, ey); x.stroke(); x.setLineDash([]);
    if (attack && Math.hypot(ex - a, ey - b) > 1) {
      const an = Math.atan2(ey - b, ex - a), L = held ? 11 : 9, Wd = held ? 5 : 4;
      x.beginPath();
      x.moveTo(ex, ey); x.lineTo(ex - L * Math.cos(an) + Wd * Math.sin(an), ey - L * Math.sin(an) - Wd * Math.cos(an));
      x.lineTo(ex - L * Math.cos(an) - Wd * Math.sin(an), ey - L * Math.sin(an) + Wd * Math.cos(an)); x.closePath(); x.fill();
    } else { x.beginPath(); x.arc(ex, ey, held ? 2.8 : 2.2, 0, Math.PI * 2); x.fill(); }
  }

  // Oil where ships went down: an irregular blot about a hull long, a few soft round shapes overlapping along the ship's
  // last course, no point, each a little different, dark at low strength, fading over a few battle minutes (the art
  // director, round 2: one pointed shape read as a fin, or another ship).
  for (const m of s.marks) {
    if (m.kind !== 'sunk') continue;
    const k = (sim.t - m.bt) / SLICK_FOR;
    const [a, b] = toScreen(s, m.x, m.y);
    const len = Math.max(14, 200 * cam.z);
    const seed = m.x * 0.0137 + m.y * 0.0071 + m.bt * 0.37;
    const rnd = (i: number): number => { const v = Math.sin(seed * 12.9898 + i * 78.233) * 43758.5453; return v - Math.floor(v); };
    const n = 3 + Math.floor(rnd(0) * 3);
    const alpha = 0.2 * (1 - k);
    x.save(); x.translate(a, b); x.rotate(m.face ?? 0);
    for (let i = 0; i < n; i++) {
      const along = ((i + 0.5) / n - 0.5) * len * 0.6 + (rnd(i + 1) - 0.5) * len * 0.1;
      const across = (rnd(i + 11) - 0.5) * len * 0.16;
      const rr = len * (0.24 + 0.12 * rnd(i + 21));
      const gr = x.createRadialGradient(across, along, 0, across, along, rr);
      gr.addColorStop(0, `rgba(30,32,34,${alpha})`);
      gr.addColorStop(0.55, `rgba(30,32,34,${alpha * 0.75})`);
      gr.addColorStop(1, 'rgba(30,32,34,0)');
      x.fillStyle = gr; x.beginPath(); x.arc(across, along, rr, 0, Math.PI * 2); x.fill();
    }
    x.restore();
  }

  // Wakes first, under every hull.
  for (const u of sim.units) {
    const p = at(u.id);
    if (!live(u) || !p || u.extra.way < 0.5) continue;
    const C = SEA.classes[u.extra.cls as SeaClass];
    const hx = Math.sin(u.face), hy = -Math.cos(u.face);
    const len = Math.max(C.len * 2.5 * cam.z, 18) * Math.min(1.6, u.extra.way / (C.speed * SEA.speeds.cruise));
    for (const [a, b] of p.ships) {
      const c = a - hx * len, d = b - hy * len;
      const gr = x.createLinearGradient(a, b, c, d);
      gr.addColorStop(0, `${FOAM}.75)`); gr.addColorStop(1, `${FOAM}0)`);
      x.strokeStyle = gr; x.lineWidth = Math.max(1.5, C.beam * cam.z * 1.4);
      x.beginPath(); x.moveTo(a, b); x.lineTo(c, d); x.stroke();
    }
  }

  // The guns, salvo by salvo: a flash at the guns, a short pale streak flying to the fall of shot and gone within a
  // second, then the splashes round the target. No standing line from every ship (the art director, 1). Each class's rhythm is in SALVO.
  for (const u of sim.units) {
    if (!live(u) || u.engaged === undefined) continue;
    const p = at(u.id), t = at(u.engaged);
    if (!p || !t) continue;
    const look = SALVO[u.extra.cls as string] ?? SALVO.ca, every = look.every;
    const phase = ((s.now + u.id * 0.37) % every) / every;
    const salvo = Math.floor((s.now + u.id * 0.37) / every);
    // The guns of the ship nearest its target speak for the piece.
    let [a, b] = p.ships[0];
    for (const [sx, sy] of p.ships) if (Math.hypot(sx - t.x, sy - t.y) < Math.hypot(a - t.x, b - t.y)) [a, b] = [sx, sy];
    const spread = Math.max(6, 160 * cam.z);
    const fx = Math.sin(salvo * 2.3 + u.id) * spread * 0.5, fy = Math.cos(salvo * 1.7 + u.id) * spread * 0.35;
    const c = t.x + fx, d = t.y + fy;
    const flight = 0.9 / every;
    if (phase < 0.08) { x.fillStyle = `rgba(255,226,160,${0.95 - phase * 10})`; x.beginPath(); x.arc(a, b, look.flash, 0, Math.PI * 2); x.fill(); }
    if (phase < flight) {
      const k = phase / flight, k0 = Math.max(0, k - 0.18);
      x.strokeStyle = `rgba(250,246,236,${0.75 * (1 - k * 0.5)})`; x.lineWidth = look.streak;
      x.beginPath(); x.moveTo(a + (c - a) * k0, b + (d - b) * k0); x.lineTo(a + (c - a) * k, b + (d - b) * k); x.stroke();
    } else if (phase < flight + 0.3) {
      const k = (phase - flight) / 0.3;
      for (let i = 0; i < look.splashes; i++) {
        const an = u.id * 1.7 + i * 2.1 + salvo;
        x.strokeStyle = `${FOAM}${0.9 * (1 - k)})`; x.lineWidth = look.streak * 0.9;
        x.beginPath(); x.arc(c + Math.cos(an) * spread * 0.25, d + Math.sin(an) * spread * 0.18, 1.5 + look.rise * k, 0, Math.PI * 2); x.stroke();
      }
    }
  }

  // Smoke from a ship burning or badly hurt: soft, see-through puffs drifting downwind, never a solid blob.
  const smoke = (p: DrawnPiece, seed: number, strength: number): void => {
    for (let i = 0; i < 6; i++) {
      const age = ((s.now * 0.5 + i / 6 + seed * 0.13) % 1);
      const a = p.x + WIND[0] * age * 60 * cam.z, b = p.y - WIND[1] * age * 60 * cam.z;
      const rr = Math.max(3, 60 * cam.z) * (0.5 + age * 1.6);
      x.fillStyle = `rgba(70,68,64,${strength * 0.22 * (1 - age)})`;
      x.beginPath(); x.arc(a, b, rr, 0, Math.PI * 2); x.fill();
    }
  };
  for (const m of s.marks) {
    if (m.kind !== 'burning' || m.unit === undefined) continue;
    const u = byId.get(m.unit), p = at(m.unit);
    if (u && p && live(u)) smoke(p, m.unit, 1 - (sim.t - m.bt) / 120);
  }
  for (const u of sim.units) { const p = at(u.id); if (p && live(u) && u.str < 0.5) smoke(p, u.id + 7, 0.6 * (1 - u.str)); }

  // The hulls: the departed and the sunk are not drawn; the enemy first, the player's own on top. Every hull has its
  // own water (layoutSea), so none is drawn on another.
  const order = sim.units.filter(live).sort((a, c) => (a.side === s.me ? 1 : 0) - (c.side === s.me ? 1 : 0));
  const labels: { text: string; p: DrawnPiece; first: boolean }[] = [];
  const framed: DrawnPiece[] = [];
  for (const u of order) {
    const p = at(u.id);
    if (!p) continue;
    const cls = u.extra.cls as SeaClass;
    const inner = u.side !== s.me ? null : u.state === 'routing' ? ALARM : u.state === 'wavering' ? AMBER : PAPER;
    for (const [a, b] of p.ships) {
      x.save(); x.translate(a, b); x.rotate(u.face);
      hull(x, cls, p.L, p.B, colour[u.side], inner);
      x.restore();
    }
    const held = s.sel.has(u.id), over = s.hover === u.id;
    // The piece in hand, or under the pointer: an outline round the whole of it, the same as picking takes, and its
    // hull bar inside the outline, under its own hulls (an enemy's only under the pointer: the UI director, 13). Both
    // stay on the piece, in the water ROW_CLEAR keeps round it, so neither lies on another row.
    if (held || over) {
      x.strokeStyle = held ? BRASS : 'rgba(45,36,26,.7)'; x.lineWidth = held ? 2 : 1.2;
      outline(x, lay, p);
      framed.push(p);
    }
    if ((held && u.side === s.me) || over) hullBar(x, lay, p, u.str);
    // Words over a ship only for the one under the pointer (the hover) and the one alone in hand (its name): never for
    // every ship in hand (the UI director, sea newcomer 1).
    if (over) labels.push({ text: hoverText(sim, u, s.me, s.sel.size), p, first: true });
    else if (held && s.sel.size === 1) labels.push({ text: shownName(u, sim.ticket), p, first: false });
  }
  // Strikes on their way in: a ring closing on the target as they come, round the whole of the piece, its word on a
  // small paper chip outside the ring, on the side away from the ships, never over a hull (the art director,
  // keep-together 3). The chips are placed before any words, so the words keep off them (b458, 6).
  const rings: { p: DrawnPiece; k: number; rr: number; chip: Chip }[] = [];
  for (const ib of s.inbound) {
    const t = byId.get(ib.target), p = at(ib.target);
    if (!t || !p || !live(t)) continue;
    const k = Math.max(0, Math.min(1, (ib.lands - sim.t) / SEA.strike.windup));
    const rr = Math.hypot(...halfBox(lay, p)) + 6 + 40 * k;
    rings.push({ p, k, rr, chip: placeStrikeChip(x, lay, p, rr, W, H, (s.clearTop ?? 50) + 4) });
  }
  // At most two lines of words, each over open water: above its piece's outline where that is clear, else the nearest
  // place that is, up or down or past the row's end, never across a hull, a STRIKE chip or the other line of words,
  // kept on the sea below the dispatch strip (the UI director, keep-together review). The name is placed first, the
  // hover after it.
  x.font = '700 12px "Courier Prime", monospace'; x.textAlign = 'center'; x.textBaseline = 'bottom';
  const top = (s.clearTop ?? 50) + 16;
  const blocks: [number, number, number, number][] = [];
  for (const q of lay.pieces.values()) for (const [a, b] of q.ships) {
    const ex = (Math.abs(Math.sin(q.face)) * q.L + Math.abs(Math.cos(q.face)) * q.B) / 2, ey = (Math.abs(Math.cos(q.face)) * q.L + Math.abs(Math.sin(q.face)) * q.B) / 2;
    blocks.push([a - ex - 2, b - ey - 2, a + ex + 2, b + ey + 2]);
  }
  for (const q of framed) { const [hw, hh] = halfBox(lay, q, PICK + 1); blocks.push([q.x - hw, q.y - hh, q.x + hw, q.y + hh]); }
  // The chips and the words already placed are never written over, even where every place near is taken (openWater).
  const hard: [number, number, number, number][] = rings.map(({ chip: c }) => [c.cx - c.w / 2 - 3, c.cy - c.h / 2 - 3, c.cx + c.w / 2 + 3, c.cy + c.h / 2 + 3]);
  blocks.push(...hard);
  for (const lb of labels.sort((p, q) => Number(p.first) - Number(q.first))) {
    const w = x.measureText(lb.text).width, h = 15;
    const [hw, hh] = halfBox(lay, lb.p, PICK + 1);
    const [lx, ly] = openWater(lb.p.x, lb.p.y - hh - 3, w, h, hw, hh, blocks, top, W, H, hard);
    blocks.push([lx - w / 2 - 2, ly - h - 2, lx + w / 2 + 2, ly + 2]);
    hard.push([lx - w / 2 - 2, ly - h - 2, lx + w / 2 + 2, ly + 2]);
    x.lineWidth = 3; x.strokeStyle = 'rgba(14,11,8,.85)'; x.strokeText(lb.text, lx, ly);
    x.fillStyle = '#ede6d6'; x.fillText(lb.text, lx, ly);
  }

  // The strikes' rings and chips, where they were placed above.
  for (const { p, k, rr, chip } of rings) {
    x.strokeStyle = `rgba(138,107,59,${0.6 + 0.4 * (1 - k)})`; x.lineWidth = 1.5; x.setLineDash([3, 4]);
    x.beginPath(); x.arc(p.x, p.y, rr, 0, Math.PI * 2); x.stroke(); x.setLineDash([]);
    drawStrikeChip(x, chip);
  }

  // What has just happened on the water: the moment a ship goes down (a ring that spreads and fades), strikes and raids.
  for (const m of s.marks) {
    const age = s.now - m.at;
    const q = m.unit !== undefined ? at(m.unit) : undefined;
    const [a, b] = q ? [q.x, q.y] : toScreen(s, m.x, m.y);
    if (m.kind === 'sunk' && age < 1.6) {
      const k = age / 1.6;
      x.strokeStyle = `${FOAM}${0.85 * (1 - k)})`; x.lineWidth = 1.5;
      x.beginPath(); x.arc(a, b, 4 + 22 * k, 0, Math.PI * 2); x.stroke();
    } else if ((m.kind === 'strike' || m.kind === 'air') && age < 1.5) {
      const k = age / 1.5;
      x.fillStyle = `rgba(255,214,140,${0.8 * (1 - k)})`;
      for (let i = 0; i < 5; i++) { const ang = i * 1.26 + m.at; x.beginPath(); x.arc(a + Math.cos(ang) * 10 * k, b + Math.sin(ang) * 7 * k, 3 * (1 - k) + 1, 0, Math.PI * 2); x.fill(); }
      x.strokeStyle = `${FOAM}${0.7 * (1 - k)})`; x.beginPath(); x.arc(a, b, 6 + 20 * k, 0, Math.PI * 2); x.stroke();
    }
  }

  // A line being drawn: where each piece would take station.
  if (s.line) {
    const [a, b] = toScreen(s, s.line.x0, s.line.y0), [c, d] = toScreen(s, s.line.x1, s.line.y1);
    x.strokeStyle = BRASS; x.lineWidth = 2; x.beginPath(); x.moveTo(a, b); x.lineTo(c, d); x.stroke();
    for (const gh of s.line.ghosts) { const [gx, gy] = toScreen(s, gh.x, gh.y); x.strokeStyle = BRASS; x.setLineDash([3, 3]); x.beginPath(); x.arc(gx, gy, 7, 0, Math.PI * 2); x.stroke(); }
    x.setLineDash([]);
  }
  if (s.box) {
    const [a, b] = toScreen(s, s.box[0], s.box[1]), [c, d] = toScreen(s, s.box[2], s.box[3]);
    x.fillStyle = 'rgba(224,184,114,.14)'; x.fillRect(a, b, c - a, d - b);
    x.strokeStyle = BRASS; x.lineWidth = 1; x.strokeRect(a, b, c - a, d - b);
  }
  // Where the last order went, for a moment, in gold.
  if (s.ping && s.now - s.ping.at < 0.6) {
    const k = (s.now - s.ping.at) / 0.6;
    const [a, b] = toScreen(s, s.ping.x, s.ping.y);
    x.strokeStyle = `rgba(224,184,114,${1 - k})`;
    x.lineWidth = 2; x.beginPath(); x.arc(a, b, 6 + 16 * k, 0, Math.PI * 2); x.stroke();
  }
  x.restore();
}

/** A ring's one word, at its top edge, in the typed face (the UI director, 10). */
function ringWord(x: CanvasRenderingContext2D, word: string, cx: number, top: number): void {
  x.font = '700 12px "Courier Prime", monospace'; x.textAlign = 'center'; x.textBaseline = 'middle';
  x.lineWidth = 3; x.strokeStyle = 'rgba(157,178,189,.9)'; x.strokeText(word, cx, top);
  x.fillStyle = '#5c4626'; x.fillText(word, cx, top);
}

/** A STRIKE chip's word, middle and size, in screen pixels. */
interface Chip { word: string; cx: number; cy: number; w: number; h: number }

/**
 * Where STRIKE stands on its small paper chip: just outside the strike's ring, on the side away from the ships round it,
 * and clear of every hull: tried first straight away from them, then turning a twelfth of the way round each side at a
 * time.
 */
function placeStrikeChip(x: CanvasRenderingContext2D, lay: SeaLayout, p: DrawnPiece, rr: number, W: number, H: number, top: number): Chip {
  x.font = '700 12px "Courier Prime", monospace';
  const word = 'STRIKE', w = Math.ceil(x.measureText(word).width) + 12, h = 18;
  // Away from the ships near the ring: the target's own and any others.
  let sx = 0, sy = 0;
  for (const q of lay.pieces.values()) for (const [a, b] of q.ships) {
    const d = Math.hypot(a - p.x, b - p.y);
    if (d > 0.5 && d < rr + 160) { sx += (a - p.x) / d; sy += (b - p.y) / d; }
  }
  const away = Math.hypot(sx, sy) > 0.3 ? Math.atan2(-sy, -sx) : -Math.PI / 2;
  const hulls: [number, number, number, number][] = [];
  for (const q of lay.pieces.values()) for (const [a, b] of q.ships) {
    const ex = (Math.abs(Math.sin(q.face)) * q.L + Math.abs(Math.cos(q.face)) * q.B) / 2, ey = (Math.abs(Math.cos(q.face)) * q.L + Math.abs(Math.sin(q.face)) * q.B) / 2;
    hulls.push([a - ex - 3, b - ey - 3, a + ex + 3, b + ey + 3]);
  }
  let cx = p.x, cy = p.y - rr - h / 2 - 3;
  for (let i = 0; i < 12; i++) {
    const an = away + (i % 2 ? 1 : -1) * Math.ceil(i / 2) * (Math.PI / 6);
    const dx = Math.cos(an), dy = Math.sin(an);
    const reach = rr + 4 + Math.abs(dx) * w / 2 + Math.abs(dy) * h / 2;
    const tx = p.x + dx * reach, ty = p.y + dy * reach;
    const clear = !hulls.some(([a, b, c, d]) => tx - w / 2 < c && tx + w / 2 > a && ty - h / 2 < d && ty + h / 2 > b);
    const onSea = tx - w / 2 >= 4 && tx + w / 2 <= W - 4 && ty - h / 2 >= top && ty + h / 2 <= H - 4;
    if (i === 0) { cx = tx; cy = ty; }
    if (clear && onSea) { cx = tx; cy = ty; break; }
  }
  return { word, cx, cy, w, h };
}

/** STRIKE on its chip, where placeStrikeChip put it. */
function drawStrikeChip(x: CanvasRenderingContext2D, c: Chip): void {
  const lx = Math.round(c.cx - c.w / 2) + 0.5, ly = Math.round(c.cy - c.h / 2) + 0.5;
  x.font = '700 12px "Courier Prime", monospace';
  x.fillStyle = '#e2d7bc'; x.fillRect(lx, ly, c.w, c.h);
  x.strokeStyle = 'rgba(92,70,38,.55)'; x.lineWidth = 1; x.strokeRect(lx, ly, c.w, c.h);
  x.fillStyle = '#5c4626'; x.textAlign = 'center'; x.textBaseline = 'middle'; x.fillText(c.word, c.cx, c.cy + 1);
}

/**
 * A ship's outline for the cards, from above as on the board, bow to the right, in its side's fill with the ink rim:
 * the battleship and the carrier full length, the cruiser three quarters, the destroyer half; the carrier a flat deck
 * with its island a small block to one side (the art director's review, 4).
 */
export function shipSvg(cls: SeaClass, color: string): string {
  const len = cls === 'dd' ? 16 : cls === 'ca' ? 24 : 32, beam = cls === 'bb' ? 7 : cls === 'cv' ? 7.4 : cls === 'ca' ? 5.4 : 4;
  const x0 = (32 - len) / 2, x1 = x0 + len, top = 7 - beam / 2, bot = 7 + beam / 2;
  const body = cls === 'cv'
    ? `<path d="M${x0} ${top}H${x1 - 3}L${x1} 7L${x1 - 3} ${bot}H${x0}Z" fill="${color}" stroke="${INK}" stroke-width=".9"/><rect x="${x0 + len * 0.55}" y="${top - 0.2}" width="${len * 0.14}" height="2.2" fill="${INK}"/><path d="M${x0 + 2} 7.6H${x1 - 5}" stroke="${PAPER}" stroke-width=".6" stroke-dasharray="1.6 1.4"/>`
    : `<path d="M${x0} 7Q${x0 + len * 0.1} ${top} ${x0 + len * 0.28} ${top}H${x1 - len * 0.3}Q${x1 - len * 0.08} ${top + 0.4} ${x1} 7Q${x1 - len * 0.08} ${bot - 0.4} ${x1 - len * 0.3} ${bot}H${x0 + len * 0.28}Q${x0 + len * 0.1} ${bot} ${x0} 7Z" fill="${color}" stroke="${INK}" stroke-width=".9"/>`;
  const at = (k: number): number => x0 + len * k;
  const marks = cls === 'bb' ? `<circle cx="${at(0.28)}" cy="7" r="1.6"/><circle cx="${at(0.4)}" cy="7" r="1.4"/><circle cx="${at(0.74)}" cy="7" r="1.6"/>`
    : cls === 'ca' ? `<circle cx="${at(0.3)}" cy="7" r="1.3"/><circle cx="${at(0.72)}" cy="7" r="1.3"/>`
      : cls === 'dd' ? `<circle cx="${at(0.66)}" cy="7" r="1"/>` : '';
  return `<svg class="bsign" width="34" height="16" viewBox="0 0 32 14" aria-hidden="true">${body}<g fill="${PAPER}">${marks}</g></svg>`;
}
