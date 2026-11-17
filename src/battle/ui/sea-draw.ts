// One frame of a fleet action: the open sea under the camera, each piece's ships in line ahead in their nations'
// colours with their wakes, the guns that are firing, strikes and raids landing, ships going down, the player's orders
// and what is selected. Everything is read from the sim: this file decides nothing.
// TODO(polish): the browser's stand-in for the 3D sea (the art director's sea art list); the 3D game draws the same
// view with the models (figures, act, speed, engaged are there for it).

import { ahead } from '../core/geometry';
import type { FieldEvent, Side } from '../core/types';
import { SEA } from '../sea/numbers';
import type { SeaSim } from '../sea/sim';
import type { SeaClass, SeaUnit } from '../sea/types';
import { toScreen, type Scene } from './draw';

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
function toLab(hex: string): [number, number, number] {
  const n = parseInt(hex.replace('#', ''), 16);
  const [r, g, b] = [lin((n >> 16) & 255), lin((n >> 8) & 255), lin(n & 255)];
  const X = (0.4124 * r + 0.3576 * g + 0.1805 * b) / WHITE[0], Y = 0.2126 * r + 0.7152 * g + 0.0722 * b, Z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / WHITE[2];
  const [fx, fy, fz] = [fLab(X), fLab(Y), fLab(Z)];
  return [116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz)];
}
function fromLab([L, a, b]: [number, number, number]): string {
  const fy = (L + 16) / 116, fx = fy + a / 500, fz = fy - b / 200;
  const [X, Y, Z] = [fInv(fx) * WHITE[0], fInv(fy), fInv(fz) * WHITE[2]];
  const r = 3.2406 * X - 1.5372 * Y - 0.4986 * Z, g = -0.9689 * X + 1.8758 * Y + 0.0415 * Z, bl = 0.0557 * X - 0.204 * Y + 1.057 * Z;
  return `#${[unlin(r), unlin(g), unlin(bl)].map((v) => v.toString(16).padStart(2, '0')).join('')}`;
}

/**
 * The two sides' fills for one battle: each nation's own colour, except that where the two lie within 20 L* of each
 * other the lighter is raised until they stand 20 apart, so a red-green colour-blind eye still tells them apart
 * (Britain's brick and Italy's green come to #b5574a and about #86b789).
 */
export function sideColours(att: string, def: string): Record<Side, string> {
  const A = toLab(att), D = toLab(def);
  const gap = 20 - Math.abs(A[0] - D[0]);
  if (gap <= 0) return { att, def };
  if (A[0] >= D[0]) return { att: fromLab([Math.min(96, A[0] + gap), A[1], A[2]]), def };
  return { att, def: fromLab([Math.min(96, D[0] + gap), D[1], D[2]]) };
}

/** A mark on the water that fades: a ship going down, a strike landing, a raid. 'at' is real seconds, 'bt' battle seconds. */
export interface Mark { x: number; y: number; at: number; bt: number; kind: 'sunk' | 'strike' | 'air' | 'burning'; unit?: number; face?: number }

/** What the frame keeps between frames: the marks, the strikes in the air, and the sides' fills for this battle. */
export interface SeaScene extends Scene {
  marks: Mark[];
  /** Carriers' strikes in the air: the target, and when they land (battle seconds). */
  inbound: { target: number; lands: number }[];
  colours?: Record<Side, string>;
}

/** How long an oil slick lies on the water, in battle seconds (the art director: fading over a few battle minutes). */
const SLICK_FOR = 240;

export const live = (u: SeaUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';

/** The marks the frame keeps from the sim's events. */
export function takeEvents(s: SeaScene, sim: SeaSim, events: FieldEvent[], realNow: number): void {
  const bt = sim.t;
  for (const e of events) {
    const face = e.unit !== undefined ? sim.units.find((u) => u.id === e.unit)?.face : undefined;
    if (e.what === 'sea:sunk' && e.x !== undefined) s.marks.push({ x: e.x, y: e.y!, at: realNow, bt, kind: 'sunk', face });
    if (e.what === 'sea:strike' && e.x !== undefined) s.marks.push({ x: e.x, y: e.y!, at: realNow, bt, kind: 'strike' });
    if (e.what === 'sea:air' && e.x !== undefined) s.marks.push({ x: e.x, y: e.y!, at: realNow, bt, kind: 'air' });
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
  x.fillStyle = fill; x.fill();
  // Every hull keeps a dark ink rim, so a pale navy never sinks into the pale sea; ours carry a pale line inside it
  // (amber when wavering, red when breaking off), theirs the ink alone, so the sides read without colour.
  x.strokeStyle = INK;
  if (inner) { x.lineWidth = 2.6; x.stroke(); x.lineWidth = 0.9; x.strokeStyle = inner; x.stroke(); } else { x.lineWidth = 1.3; x.stroke(); }
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

/** Where each ship of a piece is: the leader at the piece's place, the rest in line ahead astern of it. */
export function shipsOf(u: SeaUnit): [number, number][] {
  const C = SEA.classes[u.extra.cls as SeaClass];
  const [ax, ay] = ahead(u.face);
  const out: [number, number][] = [];
  for (let i = 0; i < u.extra.ships; i++) out.push([u.x - ax * i * 3 * C.len, u.y - ay * i * 3 * C.len]);
  return out;
}

export function drawSea(x: CanvasRenderingContext2D, sim: SeaSim, s: SeaScene): void {
  const { cam, W, H } = s;
  const F = SEA.field;
  x.save();
  // Open water in the campaign map's sea tone, a shade deeper toward the edges; the ruled squares are a kilometre each,
  // in ink at low strength, for judging the range.
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
  // The swell: short strokes that drift slowly, so a still sea is not a dead one.
  x.strokeStyle = 'rgba(246,248,248,.22)'; x.lineWidth = 1.2;
  x.beginPath();
  const drift = (sim.t * 2) % 1400;
  for (let gy = 700; gy < F.size; gy += 1400) for (let gx = 300 + ((gy / 1400) % 2) * 700; gx < F.size; gx += 1400) {
    const [a, b] = toScreen(s, gx + drift * 0.3, gy + drift * 0.1);
    const l = Math.max(4, 240 * cam.z);
    x.moveTo(a - l, b); x.quadraticCurveTo(a, b - l * 0.18, a + l, b);
  }
  x.stroke();
  // Each side's edge, by which its ships leave for port: at the left end, clear of the dispatch (the UI director, 14).
  x.font = '700 12px "Courier Prime", monospace'; x.fillStyle = PENCIL; x.textAlign = 'left'; x.textBaseline = 'alphabetic';
  // The far edge's name sits below the dispatch strip, whose foot is about 50 pixels down.
  x.fillText(`${sim.ticket.def.adj.toUpperCase()} PORTS`, Math.max(12, x0 + 10), Math.max(66, y0 + 18));
  x.fillText(`${sim.ticket.att.adj.toUpperCase()} PORTS`, Math.max(12, x0 + 10), Math.min(H - 8, y1 - 8));

  const colour: Record<Side, string> = s.colours ?? { att: sim.ticket.att.color, def: sim.ticket.def.color };
  const byId = new Map(sim.units.map((u) => [u.id, u]));

  // Reach for what is in hand: the range it hits hardest at (solid) and the furthest it hits at all (dashed), each
  // named at its top edge (the UI director, 10).
  for (const id of s.sel) {
    const u = byId.get(id);
    if (!u || !live(u) || u.extra.cls === 'cv') continue;
    const C = SEA.classes[u.extra.cls as SeaClass];
    const [cx, cy] = toScreen(s, u.x, u.y);
    x.strokeStyle = 'rgba(138,107,59,.75)'; x.lineWidth = 1.2;
    x.beginPath(); x.arc(cx, cy, C.best * cam.z, 0, Math.PI * 2); x.stroke();
    x.setLineDash([5, 6]); x.beginPath(); x.arc(cx, cy, C.reach * cam.z, 0, Math.PI * 2); x.stroke(); x.setLineDash([]);
    ringWord(x, 'BEST', cx, cy - C.best * cam.z);
    ringWord(x, 'REACH', cx, cy - C.reach * cam.z);
  }

  // Orders of the player's own ships, as on the land page: the lines of those in hand in the selection gold, the rest
  // in the staff's pencil; never red. An attack ends in a small head at its target, a course in a dot. A ship in hand
  // that is firing on its own choice shows its target the same way; no other ship draws a standing line.
  for (const u of sim.units) {
    if (u.side !== s.me || !live(u)) continue;
    const o = sim.orderOf(u.id);
    const held = s.sel.has(u.id);
    let end: [number, number] | null = null, attack = false;
    if (o?.kind === 'move') end = [o.x, o.y];
    else if (o?.kind === 'attack') { const t = byId.get(o.target); if (t && live(t)) { end = [t.x, t.y]; attack = true; } }
    if (!end && held && u.engaged !== undefined) { const t = byId.get(u.engaged); if (t && live(t)) { end = [t.x, t.y]; attack = true; } }
    if (!end) continue;
    const ink = held ? BRASS : PENCIL;
    x.strokeStyle = ink; x.fillStyle = ink;
    x.lineWidth = held ? 2.2 : 1.2; x.setLineDash(held ? [7, 5] : [5, 4]);
    const [a, b] = toScreen(s, u.x, u.y), [ex, ey] = toScreen(s, end[0], end[1]);
    x.beginPath(); x.moveTo(a, b); x.lineTo(ex, ey); x.stroke(); x.setLineDash([]);
    if (attack && Math.hypot(ex - a, ey - b) > 1) {
      const an = Math.atan2(ey - b, ex - a), L = held ? 11 : 9, Wd = held ? 5 : 4;
      x.beginPath();
      x.moveTo(ex, ey); x.lineTo(ex - L * Math.cos(an) + Wd * Math.sin(an), ey - L * Math.sin(an) - Wd * Math.cos(an));
      x.lineTo(ex - L * Math.cos(an) - Wd * Math.sin(an), ey - L * Math.sin(an) + Wd * Math.cos(an)); x.closePath(); x.fill();
    } else { x.beginPath(); x.arc(ex, ey, held ? 2.8 : 2.2, 0, Math.PI * 2); x.fill(); }
  }

  // Oil where ships went down: a small ragged dark slick about a hull long, fading over a few battle minutes.
  for (const m of s.marks) {
    if (m.kind !== 'sunk') continue;
    const k = (sim.t - m.bt) / SLICK_FOR;
    const [a, b] = toScreen(s, m.x, m.y);
    const len = Math.max(14, 200 * cam.z);
    x.fillStyle = `rgba(30,32,34,${0.32 * (1 - k)})`;
    x.save(); x.translate(a, b); x.rotate((m.face ?? 0) + 0.3);
    x.beginPath();
    for (let i = 0; i < 9; i++) {
      const an = (i / 9) * Math.PI * 2, wob = 0.75 + 0.25 * Math.sin(i * 2.7 + m.bt);
      const px = Math.sin(an) * len * 0.22 * wob, py = -Math.cos(an) * len * 0.5 * wob;
      if (i) x.lineTo(px, py); else x.moveTo(px, py);
    }
    x.closePath(); x.fill(); x.restore();
  }

  // Wakes first, under every hull.
  for (const u of sim.units) {
    if (!live(u) || u.extra.way < 0.5) continue;
    const C = SEA.classes[u.extra.cls as SeaClass];
    const [ax, ay] = ahead(u.face);
    const len = Math.max(C.len * 2.5, 18 / cam.z) * Math.min(1.6, u.extra.way / (C.speed * SEA.speeds.cruise));
    for (const [sx, sy] of shipsOf(u)) {
      const [a, b] = toScreen(s, sx, sy), [c, d] = toScreen(s, sx - ax * len, sy - ay * len);
      const gr = x.createLinearGradient(a, b, c, d);
      gr.addColorStop(0, `${FOAM}.75)`); gr.addColorStop(1, `${FOAM}0)`);
      x.strokeStyle = gr; x.lineWidth = Math.max(1.5, C.beam * cam.z * 1.4);
      x.beginPath(); x.moveTo(a, b); x.lineTo(c, d); x.stroke();
    }
  }

  // The guns, salvo by salvo: a flash at the guns, a short pale streak flying to the fall of shot and gone within a
  // second, then the splashes round the target. No standing line from every ship (the art director, 1).
  for (const u of sim.units) {
    if (!live(u) || u.engaged === undefined) continue;
    const t = byId.get(u.engaged);
    if (!t) continue;
    const every = u.extra.cls === 'dd' ? 1.6 : u.extra.cls === 'ca' ? 2.4 : 3.2;
    const phase = ((s.now + u.id * 0.37) % every) / every;
    const [a, b] = toScreen(s, u.x, u.y);
    const salvo = Math.floor((s.now + u.id * 0.37) / every);
    const spread = Math.max(6, 160 * cam.z);
    const fx = Math.sin(salvo * 2.3 + u.id) * spread * 0.5, fy = Math.cos(salvo * 1.7 + u.id) * spread * 0.35;
    const [c0, d0] = toScreen(s, t.x, t.y);
    const c = c0 + fx, d = d0 + fy;
    const flight = 0.9 / every;
    if (phase < 0.08) { x.fillStyle = `rgba(255,226,160,${0.95 - phase * 10})`; x.beginPath(); x.arc(a, b, 3.5, 0, Math.PI * 2); x.fill(); }
    if (phase < flight) {
      const k = phase / flight, k0 = Math.max(0, k - 0.18);
      x.strokeStyle = `rgba(250,246,236,${0.75 * (1 - k * 0.5)})`; x.lineWidth = 1.4;
      x.beginPath(); x.moveTo(a + (c - a) * k0, b + (d - b) * k0); x.lineTo(a + (c - a) * k, b + (d - b) * k); x.stroke();
    } else if (phase < flight + 0.3) {
      const k = (phase - flight) / 0.3;
      for (let i = 0; i < 3; i++) {
        const an = u.id * 1.7 + i * 2.1 + salvo;
        x.strokeStyle = `${FOAM}${0.9 * (1 - k)})`; x.lineWidth = 1.3;
        x.beginPath(); x.arc(c + Math.cos(an) * spread * 0.25, d + Math.sin(an) * spread * 0.18, 1.5 + 4 * k, 0, Math.PI * 2); x.stroke();
      }
    }
  }

  // Smoke from a ship burning or badly hurt: soft, see-through puffs drifting downwind, never a solid blob.
  const smoke = (wx: number, wy: number, seed: number, strength: number): void => {
    for (let i = 0; i < 6; i++) {
      const age = ((s.now * 0.5 + i / 6 + seed * 0.13) % 1);
      const px = wx + WIND[0] * age * 60, py = wy + WIND[1] * age * 60;
      const [a, b] = toScreen(s, px, py);
      const rr = Math.max(3, 60 * cam.z) * (0.5 + age * 1.6);
      x.fillStyle = `rgba(70,68,64,${strength * 0.22 * (1 - age)})`;
      x.beginPath(); x.arc(a, b, rr, 0, Math.PI * 2); x.fill();
    }
  };
  for (const m of s.marks) {
    if (m.kind !== 'burning' || m.unit === undefined) continue;
    const u = byId.get(m.unit);
    if (u && live(u)) smoke(u.x, u.y, m.unit, 1 - (sim.t - m.bt) / 120);
  }
  for (const u of sim.units) if (live(u) && u.str < 0.5) smoke(u.x, u.y, u.id + 7, 0.6 * (1 - u.str));

  // The hulls: the departed and the sunk are not drawn; the enemy first, the player's own on top.
  const order = sim.units.filter(live).sort((a, c) => (a.side === s.me ? 1 : 0) - (c.side === s.me ? 1 : 0));
  for (const u of order) {
    const cls = u.extra.cls as SeaClass, C = SEA.classes[cls];
    const L = Math.max(MIN_LEN[cls], C.len * cam.z), B = Math.max(MIN_BEAM[cls], C.beam * cam.z * 1.6);
    const inner = u.side !== s.me ? null : u.state === 'routing' ? ALARM : u.state === 'wavering' ? AMBER : PAPER;
    for (const [sx, sy] of shipsOf(u)) {
      const [a, b] = toScreen(s, sx, sy);
      x.save(); x.translate(a, b); x.rotate(u.face);
      hull(x, cls, L, B, colour[u.side], inner);
      x.restore();
    }
    const [a, b] = toScreen(s, u.x, u.y);
    const held = s.sel.has(u.id), over = s.hover === u.id;
    if (held || over) {
      x.strokeStyle = held ? BRASS : 'rgba(45,36,26,.7)'; x.lineWidth = held ? 2 : 1.2;
      x.beginPath(); x.arc(a, b, L * 0.75 + 4, 0, Math.PI * 2); x.stroke();
    }
    // A hull's bar only while it is in hand or under the pointer; an enemy's only under the pointer (the UI director, 13).
    if ((held && u.side === s.me) || over) {
      const w = 24;
      x.fillStyle = 'rgba(14,11,8,.7)'; x.fillRect(a - w / 2, b + L * 0.6 + 3, w, 3);
      const shown = Math.round(u.str * 100);
      x.fillStyle = shown < 35 ? ALARM : shown < 65 ? AMBER : '#5f9a52'; x.fillRect(a - w / 2, b + L * 0.6 + 3, (w * shown) / 100, 3);
      x.font = '700 12px "Courier Prime", monospace'; x.textAlign = 'center'; x.textBaseline = 'bottom';
      x.lineWidth = 3; x.strokeStyle = 'rgba(14,11,8,.85)'; x.strokeText(u.name, a, b - L * 0.6 - 6);
      x.fillStyle = '#ede6d6'; x.fillText(u.name, a, b - L * 0.6 - 6);
    }
  }

  // Strikes on their way in: a ring closing on the target as they come, named.
  for (const ib of s.inbound) {
    const t = byId.get(ib.target);
    if (!t || !live(t)) continue;
    const k = Math.max(0, Math.min(1, (ib.lands - sim.t) / SEA.strike.windup));
    const [a, b] = toScreen(s, t.x, t.y);
    const rr = 12 + 40 * k;
    x.strokeStyle = `rgba(138,107,59,${0.6 + 0.4 * (1 - k)})`; x.lineWidth = 1.5; x.setLineDash([3, 4]);
    x.beginPath(); x.arc(a, b, rr, 0, Math.PI * 2); x.stroke(); x.setLineDash([]);
    ringWord(x, 'STRIKE', a, b - rr);
  }

  // What has just happened on the water: the moment a ship goes down (a ring that spreads and fades), strikes and raids.
  for (const m of s.marks) {
    const age = s.now - m.at;
    const [a, b] = toScreen(s, m.x, m.y);
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
