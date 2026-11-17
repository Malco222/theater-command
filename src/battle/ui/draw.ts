// One frame of the field: the painted map under the camera, the blocks in their nations' colours with their kind's
// sign, the orders the player has given, what is selected, and the fighting. Everything is read from the sim's view:
// this file decides nothing. TODO(polish): this is the browser's stand-in for the 3D field; the 3D game draws the same
// view with the models (figures, act, speed, engaged, cover are there for it).

import { DT, type Side } from '../core/types';
import type { LandBattle } from '../land/sim';
import type { LandUnit } from '../land/units';
import { PAINT, type Painted } from './paint';

export interface Camera { x: number; y: number; z: number }

export interface Scene {
  cam: Camera;
  /** The canvas's size in CSS pixels. */
  W: number;
  H: number;
  me: Side;
  sel: Set<number>;
  hover: number | null;
  /** A box being dragged out, in field metres. */
  box: [number, number, number, number] | null;
  /** A line being drawn with the right button, and where each block would stand on it. */
  line: { x0: number; y0: number; x1: number; y1: number; ghosts: { id: number; x: number; y: number; face: number }[] } | null;
  /** Real seconds, for the flicker of the fighting. */
  now: number;
  /** A brief mark where the last order went. */
  ping: { x: number; y: number; at: number; kind: 'move' | 'attack' } | null;
}

export const toScreen = (s: Scene, x: number, y: number): [number, number] => [s.W / 2 + (x - s.cam.x) * s.cam.z, s.H / 2 - (y - s.cam.y) * s.cam.z];
export const toField = (s: Scene, px: number, py: number): [number, number] => [s.cam.x + (px - s.W / 2) / s.cam.z, s.cam.y - (py - s.H / 2) / s.cam.z];

const INK = '#2a2d2f';
const PAPER = '#f1e9d4';
const BRASS = '#e0b872';
const ALARM = '#c4553f';
const AMBER = '#f0a63a';
const GAIN = '#6f9a5f';

/** The kind's sign inside a block, the campaign map's own signs: a cross for foot, an oval for tanks, a dot for guns. */
function sign(x: CanvasRenderingContext2D, kind: string, w: number, h: number): void {
  const L = -w / 2, T = -h / 2;
  x.beginPath();
  const cross = (): void => { x.moveTo(L, T); x.lineTo(L + w, T + h); x.moveTo(L, T + h); x.lineTo(L + w, T); };
  switch (kind) {
    case 'inf': cross(); break;
    case 'mot': cross(); break;
    case 'mtn': cross(); break;
    case 'cav': x.moveTo(L, T + h); x.lineTo(L + w, T); break;
    case 'arm': x.ellipse(0, 0, w * 0.33, h * 0.28, 0, 0, Math.PI * 2); break;
    case 'eng': x.moveTo(L + w * 0.22, T + h * 0.75); x.lineTo(L + w * 0.22, T + h * 0.3); x.lineTo(L + w * 0.78, T + h * 0.3); x.lineTo(L + w * 0.78, T + h * 0.75); x.moveTo(0, T + h * 0.3); x.lineTo(0, T + h * 0.62); break;
    case 'fort': x.moveTo(L + w * 0.3, T + h * 0.78); x.lineTo(L + w * 0.3, T + h * 0.28); x.lineTo(L + w * 0.42, T + h * 0.28); x.lineTo(L + w * 0.42, T + h * 0.44); x.lineTo(L + w * 0.58, T + h * 0.44); x.lineTo(L + w * 0.58, T + h * 0.28); x.lineTo(L + w * 0.7, T + h * 0.28); x.lineTo(L + w * 0.7, T + h * 0.78); x.closePath(); break;
    case 'art': break;
  }
  x.stroke();
  if (kind === 'art') { x.beginPath(); x.arc(0, 0, h * 0.26, 0, Math.PI * 2); x.fill(); }
  if (kind === 'mot') { x.beginPath(); x.arc(L + w * 0.3, T + h + h * 0.18, h * 0.1, 0, Math.PI * 2); x.arc(L + w * 0.7, T + h + h * 0.18, h * 0.1, 0, Math.PI * 2); x.fill(); }
  if (kind === 'mtn') { x.beginPath(); x.moveTo(-h * 0.22, T + h); x.lineTo(0, T + h * 0.62); x.lineTo(h * 0.22, T + h); x.closePath(); x.fill(); }
}

/** The kind's sign as a small picture for the cards. */
export function signSvg(kind: string, color: string): string {
  const w = 24, h = 15;
  const X = `<path d="M0 0L${w} ${h}M0 ${h}L${w} 0"/>`;
  const body = kind === 'arm' ? `<ellipse cx="${w / 2}" cy="${h / 2}" rx="${w * 0.33}" ry="${h * 0.27}"/>`
    : kind === 'art' ? `<circle cx="${w / 2}" cy="${h / 2}" r="${h * 0.24}" fill="${PAPER}" stroke="none"/>`
      : kind === 'cav' ? `<path d="M0 ${h}L${w} 0"/>`
        : kind === 'eng' ? `<path d="M${w * 0.22} ${h * 0.72}V${h * 0.32}H${w * 0.78}V${h * 0.72}M${w / 2} ${h * 0.32}V${h * 0.6}"/>`
          : kind === 'fort' ? `<path d="M${w * 0.3} ${h * 0.75}V${h * 0.3}H${w * 0.42}V${h * 0.45}H${w * 0.58}V${h * 0.3}H${w * 0.7}V${h * 0.75}Z"/>`
            : kind === 'mtn' ? `${X}<path d="M${w / 2 - 3} ${h}L${w / 2} ${h - 4.5}L${w / 2 + 3} ${h}Z" fill="${PAPER}" stroke="none"/>`
              : X;
  const wheels = kind === 'mot' ? `<circle cx="${w * 0.3}" cy="${h + 1.7}" r="1.3" fill="${PAPER}" stroke="none"/><circle cx="${w * 0.7}" cy="${h + 1.7}" r="1.3" fill="${PAPER}" stroke="none"/>` : '';
  return `<svg class="bsign" width="34" height="24" viewBox="-1 -1 26 19" aria-hidden="true"><rect x="0" y="0" width="${w}" height="${h}" rx="1.5" fill="${color}" stroke="#15181b" stroke-width=".8"/><g fill="none" stroke="${PAPER}" stroke-width="1.1" stroke-linecap="round">${body}</g>${wheels}</svg>`;
}

const live = (u: LandUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';

export function drawFrame(x: CanvasRenderingContext2D, b: LandBattle, s: Scene, field: Painted): void {
  const { cam, W, H } = s;
  x.save();
  x.fillStyle = '#0e0b08';
  x.fillRect(0, 0, W, H);
  // The painted field: its top-left is the field's north-west corner.
  const [fx, fy] = toScreen(s, 0, b.field.h);
  const fw = (field.base.width / PAINT) * cam.z, fh = (field.base.height / PAINT) * cam.z;
  x.imageSmoothingEnabled = true;
  x.drawImage(field.base, fx, fy, fw, fh);
  // The furrows and single crowns come in from the closer zooms; the whole-field view stays quiet (the art director).
  const whole = Math.min(W / b.field.w, H / b.field.h);
  const near = Math.max(0, Math.min(1, (cam.z / whole - 1.4) / 0.8));
  if (near > 0) { x.globalAlpha = near; x.drawImage(field.detail, fx, fy, fw, fh); x.globalAlpha = 1; }
  // Bunkers: the campaign map's notched block in ink, no fill, held at its whole-field size from there inward with
  // the same thin stroke, as a label is (the art director, slice 2).
  if (field.forts.length) {
    const k = 7 * Math.min(cam.z, whole);
    x.strokeStyle = INK; x.lineWidth = 0.5 * k; x.lineJoin = 'miter'; x.lineCap = 'butt';
    for (const [mx, my] of field.forts) {
      const [X, Y] = toScreen(s, mx, my);
      if (X < -6 * k || Y < -6 * k || X > W + 6 * k || Y > H + 6 * k) continue;
      x.beginPath();
      x.moveTo(X - 3 * k, Y + 2.5 * k); x.lineTo(X - 3 * k, Y - 2.5 * k); x.lineTo(X - 1.2 * k, Y - 2.5 * k); x.lineTo(X - 1.2 * k, Y - 0.8 * k);
      x.lineTo(X + 1.2 * k, Y - 0.8 * k); x.lineTo(X + 1.2 * k, Y - 2.5 * k); x.lineTo(X + 3 * k, Y - 2.5 * k); x.lineTo(X + 3 * k, Y + 2.5 * k);
      x.stroke();
    }
  }

  // Where each side drew up: a faint ruled band, only while the battle has not started.
  if (b.t < DT) {
    for (const side of ['att', 'def'] as Side[]) {
      const z = b.field.zones[side];
      const [ax, ay] = toScreen(s, z[0], z[3]), [bx, by] = toScreen(s, z[2], z[1]);
      x.setLineDash([6, 5]); x.strokeStyle = 'rgba(42,45,47,.35)'; x.lineWidth = 1;
      x.strokeRect(ax, ay, bx - ax, by - ay); x.setLineDash([]);
    }
  }

  // The town's name on the map, in the campaign map's lettering.
  if (b.field.town && b.ticket.place.town) {
    const [tx, ty] = toScreen(s, b.field.town.x, b.field.town.y + b.field.town.r * 0.75);
    x.font = `italic 600 ${Math.max(11, Math.min(22, 38 * cam.z))}px "PT Serif", Georgia, serif`;
    x.textAlign = 'center'; x.textBaseline = 'middle';
    x.lineWidth = 4; x.strokeStyle = 'rgba(233,226,207,.85)'; x.strokeText(b.ticket.place.town.toUpperCase(), tx, ty);
    x.fillStyle = INK; x.fillText(b.ticket.place.town.toUpperCase(), tx, ty);
  }
  if (b.ticket.place.river) {
    // The river's name over the water, where the river crosses the middle of the field.
    const col = Math.floor(b.field.cols * 0.18);
    let row = -1;
    for (let r = 0; r < b.field.rows; r++) if (b.field.ground[r * b.field.cols + col] === 5) { row = r; break; }
    if (row >= 0) {
      const [rx, ry] = toScreen(s, col * b.field.cell, row * b.field.cell + 60);
      x.font = `italic ${Math.max(10, Math.min(18, 30 * cam.z))}px "PT Serif", Georgia, serif`;
      x.textAlign = 'left'; x.lineWidth = 3; x.strokeStyle = 'rgba(233,226,207,.75)'; x.strokeText(b.ticket.place.river, rx, ry);
      x.fillStyle = '#2c5878'; x.fillText(b.ticket.place.river, rx, ry);
    }
  }

  const colour: Record<Side, string> = { att: b.ticket.att.color, def: b.ticket.def.color };
  const byId = new Map(b.units.map((u) => [u.id, u]));

  // Orders of the player's own blocks (the UI director, 6): the lines of those in hand in the selection gold; once put
  // down, the staff's pencil, ink and thinner. Gold is what you hold, pencil is what stands ordered; never red. An
  // attack ends in a small head at its target, a march in a dot.
  for (const u of b.units) {
    if (u.side !== s.me || !live(u) || !u.order) continue;
    const pts: [number, number][] = [[u.x, u.y], ...u.path];
    const tgt = u.order.target !== undefined ? byId.get(u.order.target) : undefined;
    const end: [number, number] = tgt && live(tgt) ? [tgt.x, tgt.y] : [u.order.x, u.order.y];
    pts.push(end);
    const held = s.sel.has(u.id);
    const ink = held ? BRASS : 'rgba(42,45,47,.6)';
    x.strokeStyle = ink; x.fillStyle = ink;
    x.lineWidth = held ? 2.2 : 1.2; x.setLineDash(held ? [7, 5] : [5, 4]);
    const sp = pts.map(([px, py]) => toScreen(s, px, py));
    x.beginPath();
    sp.forEach(([a, c], i) => { if (i) x.lineTo(a, c); else x.moveTo(a, c); });
    x.stroke(); x.setLineDash([]);
    const [ex, ey] = sp[sp.length - 1], [qx, qy] = sp[Math.max(0, sp.length - 2)];
    if (u.order.kind === 'attack' && Math.hypot(ex - qx, ey - qy) > 1) {
      const a = Math.atan2(ey - qy, ex - qx), L = held ? 11 : 9, Wd = held ? 5 : 4;
      x.beginPath();
      x.moveTo(ex, ey); x.lineTo(ex - L * Math.cos(a) + Wd * Math.sin(a), ey - L * Math.sin(a) - Wd * Math.cos(a));
      x.lineTo(ex - L * Math.cos(a) - Wd * Math.sin(a), ey - L * Math.sin(a) + Wd * Math.cos(a)); x.closePath(); x.fill();
    } else { x.beginPath(); x.arc(ex, ey, held ? 2.8 : 2.2, 0, Math.PI * 2); x.fill(); }
  }

  // The blocks: the dead and the departed are not drawn.
  const order = [...b.units].filter(live).sort((a, c) => (a.side === s.me ? 1 : 0) - (c.side === s.me ? 1 : 0));
  for (const u of order) drawBlock(x, s, u, colour[u.side], byId);

  // A line being drawn: the ghosts of where the blocks will stand.
  if (s.line) {
    const [a, c] = toScreen(s, s.line.x0, s.line.y0), [d, e] = toScreen(s, s.line.x1, s.line.y1);
    x.strokeStyle = BRASS; x.lineWidth = 2; x.beginPath(); x.moveTo(a, c); x.lineTo(d, e); x.stroke();
    for (const gh of s.line.ghosts) {
      const u = byId.get(gh.id);
      if (!u) continue;
      const [gx, gy] = toScreen(s, gh.x, gh.y);
      x.save(); x.translate(gx, gy); x.rotate(gh.face);
      x.strokeStyle = BRASS; x.setLineDash([4, 3]); x.lineWidth = 1.5;
      x.strokeRect((-u.w / 2) * cam.z, (-u.d / 2) * cam.z, u.w * cam.z, u.d * cam.z);
      x.restore();
    }
    x.setLineDash([]);
  }

  // A box being dragged out.
  if (s.box) {
    const [a, c] = toScreen(s, s.box[0], s.box[1]), [d, e] = toScreen(s, s.box[2], s.box[3]);
    x.fillStyle = 'rgba(224,184,114,.10)'; x.fillRect(a, c, d - a, e - c);
    x.strokeStyle = BRASS; x.lineWidth = 1; x.strokeRect(a, c, d - a, e - c);
  }

  // Where the last order went, for a moment.
  if (s.ping && s.now - s.ping.at < 0.6) {
    const k = (s.now - s.ping.at) / 0.6;
    const [a, c] = toScreen(s, s.ping.x, s.ping.y);
    x.strokeStyle = `rgba(224,184,114,${1 - k})`;
    x.lineWidth = 2; x.beginPath(); x.arc(a, c, 6 + 16 * k, 0, Math.PI * 2); x.stroke();
  }
  x.restore();
}

function drawBlock(x: CanvasRenderingContext2D, s: Scene, u: LandUnit, color: string, byId: Map<number, LandUnit>): void {
  const z = s.cam.z;
  const [sx, sy] = toScreen(s, u.x, u.y);
  const w = Math.max(8, u.w * z), d = Math.max(4, u.d * z);
  if (sx < -w || sy < -w || sx > s.W + w || sy > s.H + w) return;
  const mine = u.side === s.me, picked = s.sel.has(u.id), hot = s.hover === u.id;
  const routing = u.state === 'routing';

  // The fight: short flashes along the front edge, toward whoever it is fighting. TODO(polish): the 3D game's muzzle flash and smoke.
  if (u.act === 'fight' && u.engaged !== undefined) {
    const e = byId.get(u.engaged);
    if (e) {
      const [ex, ey] = toScreen(s, e.x, e.y);
      if (u.kind === 'art') {
        // Guns fire over the line: a faint arc to what they hit.
        x.strokeStyle = 'rgba(42,45,47,.28)'; x.lineWidth = 1; x.setLineDash([2, 6]);
        x.beginPath(); x.moveTo(sx, sy); x.quadraticCurveTo((sx + ex) / 2, Math.min(sy, ey) - 40, ex, ey); x.stroke(); x.setLineDash([]);
        const k = (s.now * 1.3 + u.id * 0.37) % 1;
        if (k < 0.25) { x.fillStyle = `rgba(90,80,70,${0.5 - k * 2})`; x.beginPath(); x.arc(ex + Math.sin(u.id * 7.1 + s.now) * 18 * z * 3, ey + Math.cos(u.id * 3.3 + s.now) * 10, 4 + k * 30, 0, Math.PI * 2); x.fill(); }
      } else {
        const mx = sx + (ex - sx) * 0.5, my = sy + (ey - sy) * 0.5;
        for (let i = 0; i < 3; i++) {
          const k = (s.now * 2.1 + u.id * 0.29 + i * 0.33) % 1;
          if (k > 0.45) continue;
          const ox = Math.sin(u.id * 5.3 + i * 2.1 + Math.floor(s.now * 2.1)) * w * 0.4, oy = Math.cos(u.id * 1.7 + i * 3.9 + Math.floor(s.now * 2.1)) * 6;
          x.fillStyle = `rgba(250,236,190,${0.9 - k * 2})`; x.beginPath(); x.arc(mx + ox, my + oy, 1.5 + k * 4, 0, Math.PI * 2); x.fill();
          x.fillStyle = `rgba(110,100,90,${0.35 - k * 0.7})`; x.beginPath(); x.arc(mx + ox, my + oy - 3, 3 + k * 9, 0, Math.PI * 2); x.fill();
        }
      }
    }
  }

  x.save();
  x.translate(sx, sy);
  x.rotate(u.face);
  if (picked || hot) {
    x.shadowColor = picked ? 'rgba(224,184,114,.9)' : 'rgba(255,246,216,.7)';
    x.shadowBlur = 10;
  }
  x.globalAlpha = routing ? 0.55 + 0.25 * Math.sin(s.now * 8) : 1;
  x.fillStyle = color;
  x.fillRect(-w / 2, -d / 2, w, d);
  x.shadowBlur = 0;
  // The front edge, heavier: which way it faces.
  x.strokeStyle = INK; x.lineWidth = 1;
  x.strokeRect(-w / 2, -d / 2, w, d);
  x.lineWidth = Math.max(2, Math.min(4, 7 * z));
  x.beginPath(); x.moveTo(-w / 2, -d / 2); x.lineTo(w / 2, -d / 2); x.stroke();
  // Its state, round the edge: brass for chosen, amber for wavering, red for running.
  const ring = picked ? BRASS : u.state === 'wavering' ? AMBER : routing ? ALARM : null;
  if (ring) {
    x.strokeStyle = ring; x.lineWidth = picked ? 2.5 : 2;
    if (!picked && u.state === 'wavering') x.setLineDash([5, 3]);
    x.strokeRect(-w / 2 - 2.5, -d / 2 - 2.5, w + 5, d + 5); x.setLineDash([]);
  }
  // The sign, once the block is big enough to carry one.
  if (w >= 22 && d >= 9) {
    const sw = Math.min(w * 0.5, d * 1.7), sh = Math.min(d * 0.62, sw / 1.6);
    x.strokeStyle = PAPER; x.fillStyle = PAPER; x.lineWidth = Math.max(1, Math.min(2, sh / 7)); x.lineCap = 'round';
    sign(x, u.kind, sw, sh);
  }
  // TODO(polish): its models. The 3D game stands figures0 models in ranks inside this box and fades one for each share lost (figures).
  // Strength at the block's back edge, a fixed gap behind it, turning with it (the UI director, 10): the player's own
  // and anything under the pointer.
  if (mine || hot || picked) {
    const bw = Math.max(18, w * 0.8), by = d / 2 + 4;
    x.globalAlpha = 1;
    x.fillStyle = 'rgba(14,11,8,.75)'; x.fillRect(-bw / 2 - 1, by - 1, bw + 2, 5);
    const k = u.str / Math.max(1e-6, u.str0);
    x.fillStyle = k < 0.35 ? ALARM : k < 0.65 ? AMBER : GAIN;
    x.fillRect(-bw / 2, by, bw * Math.max(0, Math.min(1, u.str)), 3);
  }
  x.restore();
  // The name, for the one under the pointer.
  if (hot) {
    const label = `${u.name}${u.state === 'steady' ? '' : ` · ${u.state}`}`;
    x.font = '700 12px "Courier Prime", ui-monospace, monospace';
    const tw = x.measureText(label).width;
    const lx = Math.min(s.W - tw - 14, Math.max(6, sx - tw / 2 - 6)), ly = sy + Math.max(w, d) / 2 + 10;
    x.fillStyle = '#e2d7bc'; x.strokeStyle = '#cdbf9c'; x.lineWidth = 1;
    x.fillRect(lx, ly, tw + 12, 20); x.strokeRect(lx, ly, tw + 12, 20);
    x.fillStyle = '#2a2118'; x.textAlign = 'left'; x.textBaseline = 'middle'; x.fillText(label, lx + 6, ly + 10.5);
  }
}
