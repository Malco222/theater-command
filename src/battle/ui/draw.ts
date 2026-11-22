// One frame of the field: the painted map under the camera, the blocks in their nations' colours with their kind's
// sign, the orders the player has given, what is selected, and the fighting. Everything is read from the sim's view:
// this file decides nothing. TODO(polish): this is the browser's stand-in for the 3D field; the 3D game draws the same
// view with the models (figures, act, speed, engaged, cover are there for it).

import { DT, type Side } from '../core/types';
import { groundAt } from '../land/field';
import { LAND } from '../land/rules';
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
  /**
   * Air support armed (F or the button): the run a strike would fly from where the pointer is, and whether it can be
   * called there (false: too close to our men). Null when not armed.
   */
  air?: { run: [number, number, number, number] | null; ok: boolean } | null;
  /** The setup step (release 2): a gun or support team being carried to where it will stand, and whether it may stand there. */
  place?: { id: number; x: number; y: number; ok: boolean } | null;
  /** The sides' colours on this field, where the page has settled them (sideColours); else the ticket's. */
  colours?: Record<Side, string> | null;
  /**
   * What stands over the field's top in canvas pixels (left, top, right, bottom): the dispatch, the tag and the zoom
   * group. A hover label keeps 8 clear of them (the UI director, b464).
   */
  avoid?: [number, number, number, number][];
}

export const toScreen = (s: Scene, x: number, y: number): [number, number] => [s.W / 2 + (x - s.cam.x) * s.cam.z, s.H / 2 - (y - s.cam.y) * s.cam.z];
export const toField = (s: Scene, px: number, py: number): [number, number] => [s.cam.x + (px - s.W / 2) / s.cam.z, s.cam.y - (py - s.H / 2) / s.cam.z];

/** Where each field's town name stands (field metres north of its centre), and when it was last moved (real seconds). */
const townAt = new WeakMap<LandBattle, { y: number; at: number }>();
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
  const k = 7 * Math.min(cam.z, whole);
  works(x, b, s, k);
  if (field.forts.length) {
    x.strokeStyle = INK; x.lineWidth = 0.5 * k; x.lineJoin = 'miter'; x.lineCap = 'butt';
    const under = (mx: number, my: number): boolean => b.units.some((u) => u.side === 'def' && u.kind !== 'art' && Math.abs(u.home[0] - mx) < u.w / 2 + 12 && Math.abs(u.home[1] - my) < u.d / 2 + 30);
    // At the wide zooms the row thins on the screen so no two signs stand closer than about three sign widths, and
    // closer in every one shows (the art director, slice 3): a row of fifteen at the whole field read as a wall.
    const gap = 18 * k;
    let lastX = -Infinity, lastY = -Infinity;
    for (const [mx, my] of [...field.forts].sort((p, q) => p[0] - q[0] || p[1] - q[1])) {
      // A bunker sits on the dug line in a gap or at an end, never under a block (the art director, ruling 226(12)).
      if (under(mx, my)) continue;
      const [X, Y] = toScreen(s, mx, my);
      if (Math.hypot(X - lastX, Y - lastY) < gap) continue;
      lastX = X; lastY = Y;
      if (X < -6 * k || Y < -6 * k || X > W + 6 * k || Y > H + 6 * k) continue;
      x.beginPath();
      x.moveTo(X - 3 * k, Y + 2.5 * k); x.lineTo(X - 3 * k, Y - 2.5 * k); x.lineTo(X - 1.2 * k, Y - 2.5 * k); x.lineTo(X - 1.2 * k, Y - 0.8 * k);
      x.lineTo(X + 1.2 * k, Y - 0.8 * k); x.lineTo(X + 1.2 * k, Y - 2.5 * k); x.lineTo(X + 3 * k, Y - 2.5 * k); x.lineTo(X + 3 * k, Y + 2.5 * k);
      x.stroke();
    }
  }

  // Where his army drew up: a faint ruled band, his own only, and only while the clock has not yet run (the art
  // director, slice 3: the enemy's edge stacked a third line along the works).
  if (b.t < DT) {
    for (const side of [s.me]) {
      const z = b.field.zones[side];
      const [ax, ay] = toScreen(s, z[0], z[3]), [bx, by] = toScreen(s, z[2], z[1]);
      x.setLineDash([6, 5]); x.strokeStyle = 'rgba(42,45,47,.35)'; x.lineWidth = 1;
      x.strokeRect(ax, ay, bx - ax, by - ay); x.setLineDash([]);
    }
  }

  // The town's name on the map, in the campaign map's lettering: above the town, never under a block (the UI
  // director, after the retest). Placed clear of the blocks where it can be, and moved, at most every two seconds,
  // only when a block comes to stand on it.
  if (b.field.town && b.ticket.place.town) {
    const T = b.field.town, name = b.ticket.place.town.toUpperCase();
    const size = Math.max(12, Math.min(22, 38 * cam.z));
    x.font = `italic 600 ${size}px "PT Serif", Georgia, serif`;
    const half = x.measureText(name).width / 2 + 4;
    const boxes = b.units.filter((v) => v.state !== 'gone' && v.state !== 'withdrawn').map((v) => {
      const [vx, vy] = toScreen(s, v.x, v.y), r = Math.max(v.w, v.d) * cam.z / 2 + 4;
      return [vx - r, vy - r, vx + r, vy + r];
    });
    const clear = (fy: number): boolean => {
      const [tx, ty] = toScreen(s, T.x, fy);
      return !boxes.some((q) => tx - half < q[2] && tx + half > q[0] && ty - size / 2 - 2 < q[3] && ty + size / 2 + 2 > q[1]);
    };
    const was = townAt.get(b);
    if (!was || (!clear(was.y) && s.now - was.at > 2)) {
      const step = (size + 6) / cam.z;
      const up = [0, 1, 2, 3, 4, 5, 6, 7].map((k) => T.y + T.r + step * (0.5 + k)), down = up.map((y) => 2 * T.y - y);
      townAt.set(b, { y: [...up, ...down].find(clear) ?? up[0], at: s.now });
    }
    const [tx, ty] = toScreen(s, T.x, townAt.get(b)!.y);
    x.textAlign = 'center'; x.textBaseline = 'middle';
    x.lineWidth = 4; x.strokeStyle = 'rgba(233,226,207,.85)'; x.strokeText(name, tx, ty);
    x.fillStyle = INK; x.fillText(name, tx, ty);
  }
  if (b.ticket.place.river) {
    // The river's name over the water, where the river crosses the middle of the field: where it bends away from the
    // lanes of the corps that do not cross (b464(5)), over its stretch before those that do, clear of the turn.
    const F = b.field, B = F.bend;
    const [s0, s1] = B ? (B.dry === 'west' ? [B.x + 250, F.w - 250] : [50, B.x - 450]) : [0, F.w];
    const col = B ? Math.min(F.cols - 1, Math.max(0, Math.floor(Math.max(s0, Math.min(s1, F.w * 0.18)) / F.cell))) : Math.floor(F.cols * 0.18);
    let row = -1;
    for (let r = 0; r < F.rows; r++) if (F.ground[r * F.cols + col] === 5) { row = r; break; }
    if (row >= 0) {
      const [rx, ry] = toScreen(s, col * F.cell, row * F.cell + 60);
      x.font = `italic ${Math.max(12, Math.min(18, 30 * cam.z))}px "PT Serif", Georgia, serif`;
      x.textAlign = 'left'; x.lineWidth = 3; x.strokeStyle = 'rgba(233,226,207,.75)'; x.strokeText(b.ticket.place.river, rx, ry);
      x.fillStyle = '#2c5878'; x.fillText(b.ticket.place.river, rx, ry);
    }
  }
  const St = b.field.stream;
  if (St) {
    // A city's own river (b526): its name beside its line, clear of the water and the armies: a fifth of the way along
    // where it runs across (the field's west fifth) or along a shore (its south fifth, on the water), halfway up the field
    // between the armies where it runs up it.
    const F = b.field, back = St.side === 'back', up = St.lie !== 'across' && !back, k = St.lie === 'up' ? 0.5 : back ? 0.28 : 0.22;
    const at = St.course.reduce((m, p) => (Math.abs((up ? p[1] : p[0]) - (up ? F.h : F.w) * k) < Math.abs((up ? m[1] : m[0]) - (up ? F.h : F.w) * k) ? p : m), St.course[0]);
    const west = St.lie === 'shore' ? at[0] < F.w / 2 : false;
    // A shore behind the defence: its name on its water, along the field's back edge clear of the city's own name.
    const [rx, ry] = back ? toScreen(s, at[0], (at[1] + F.h) / 2) : toScreen(s, at[0] + (St.lie === 'shore' ? (west ? -at[0] / 2 : (F.w - at[0]) / 2) : up ? St.half + 20 : 0), at[1] + (up ? 0 : St.half + 30));
    x.font = `italic ${Math.max(12, Math.min(18, 30 * cam.z))}px "PT Serif", Georgia, serif`;
    x.textAlign = St.lie === 'shore' ? 'center' : 'left'; x.lineWidth = 3; x.strokeStyle = 'rgba(233,226,207,.75)'; x.strokeText(St.name, rx, ry);
    x.fillStyle = '#2c5878'; x.fillText(St.name, rx, ry);
    // A second river meeting it (the Sava at Belgrade): its name beside its water, a third of the way in from the edge.
    const J = St.join;
    if (J) {
      const inside = J.course.filter((p) => p[0] >= 0 && p[0] <= F.w && p[1] >= 0 && p[1] <= F.h), p = inside[Math.floor(inside.length / 3)];
      if (p) {
        const [jx, jy] = toScreen(s, p[0], p[1] + J.half + 30);
        x.textAlign = 'center'; x.strokeText(J.name, jx, jy); x.fillText(J.name, jx, jy);
      }
    }
  }

  const colour: Record<Side, string> = s.colours ?? { att: b.ticket.att.color, def: b.ticket.def.color };
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

  // Wrecks (b477): a knocked-out tank stays where she stood to the end of the fight, drawn plainly as a small ink cross
  // under the blocks. The look (fire, smoke) is the 3D game's.
  if (b.wrecks.length) {
    const r = Math.max(2, Math.min(5, 4 * s.cam.z));
    x.strokeStyle = INK; x.lineWidth = 1.2;
    x.beginPath();
    for (const z of b.wrecks) { const [wx, wy] = toScreen(s, z.x, z.y); x.moveTo(wx - r, wy - r); x.lineTo(wx + r, wy + r); x.moveTo(wx + r, wy - r); x.lineTo(wx - r, wy + r); }
    x.stroke();
  }

  // The blocks: the dead and the departed are not drawn.
  const order = [...b.units].filter(live).sort((a, c) => (a.side === s.me ? 1 : 0) - (c.side === s.me ? 1 : 0));
  for (const u of order) drawBlock(x, s, u, colour[u.side], byId, b);
  const hot = s.hover !== null ? byId.get(s.hover) : undefined;
  if (hot && live(hot)) hoverLabel(x, s, hot, b);

  // Air support, plainly (b533(f): in the browser a strike is a plain mark; strafing, bombs and smoke are the 3D game's).
  // A run being aimed: a dashed band the width its guns rake, brass where it can be called, the alarm where it is too
  // close to our men. A flight: a small arrowhead in its side's colour, outlined in ink, with a faint band down its run
  // while it makes it.
  airMarks(x, b, s, colour);

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

  // The setup step: the gun or team being carried, brass where it may stand, red where it may not.
  if (s.place) {
    const u = byId.get(s.place.id);
    if (u) {
      const [gx, gy] = toScreen(s, s.place.x, s.place.y);
      x.save(); x.translate(gx, gy); x.rotate(u.face);
      x.strokeStyle = s.place.ok ? BRASS : '#b3261e'; x.setLineDash([4, 3]); x.lineWidth = 2;
      x.strokeRect((-u.w / 2) * cam.z, (-u.d / 2) * cam.z, u.w * cam.z, u.d * cam.z);
      x.restore(); x.setLineDash([]);
    }
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

function airMarks(x: CanvasRenderingContext2D, b: LandBattle, s: Scene, colour: Record<Side, string>): void {
  const band = (r: [number, number, number, number], fill: string, stroke: string | null): void => {
    const [a, c] = toScreen(s, r[0], r[1]), [d, e] = toScreen(s, r[2], r[3]);
    const len = Math.hypot(d - a, e - c) || 1, half = (LAND.air.wide / 2) * s.cam.z, nx = (-(e - c) / len) * half, ny = ((d - a) / len) * half;
    x.beginPath(); x.moveTo(a + nx, c + ny); x.lineTo(d + nx, e + ny); x.lineTo(d - nx, e - ny); x.lineTo(a - nx, c - ny); x.closePath();
    x.fillStyle = fill; x.fill();
    if (stroke) { x.strokeStyle = stroke; x.lineWidth = 2; x.setLineDash([6, 4]); x.stroke(); x.setLineDash([]); }
  };
  if (s.air?.run) band(s.air.run, s.air.ok ? 'rgba(224,184,114,.32)' : 'rgba(196,85,63,.26)', s.air.ok ? BRASS : ALARM);
  for (const f of b.flightsNow()) {
    if (f.phase === 'run') band(f.run, 'rgba(42,45,47,.10)', null);
    const [px, py] = toScreen(s, f.x, f.y), k = 7;
    x.save(); x.translate(px, py); x.rotate(f.face);
    x.beginPath(); x.moveTo(0, -k * 1.4); x.lineTo(k, k); x.lineTo(0, k * 0.4); x.lineTo(-k, k); x.closePath();
    x.fillStyle = colour[f.side]; x.fill(); x.strokeStyle = INK; x.lineWidth = 1.2; x.stroke();
    x.restore();
  }
}

function drawBlock(x: CanvasRenderingContext2D, s: Scene, u: LandUnit, color: string, byId: Map<number, LandUnit>, b: LandBattle): void {
  const z = s.cam.z;
  const [sx, sy] = toScreen(s, u.x, u.y);
  // An enemy block he has not met (b464(2)): a plain block in its colour, at the plain size, where it stands and facing
  // its way. No sign, bar, state or works mark.
  const shown = b.known(u), [bw0, bd0] = boxOf(u, b);
  const w = Math.max(8, bw0 * z), d = Math.max(4, bd0 * z);
  if (sx < -w || sy < -w || sx > s.W + w || sy > s.H + w) return;
  const mine = u.side === s.me, picked = s.sel.has(u.id), hot = s.hover === u.id;
  const routing = u.state === 'routing' && shown;

  // The fight: short flashes along the front edge, toward whoever it is fighting. TODO(polish): the 3D game's muzzle flash and smoke.
  if (u.act === 'fight' && u.engaged !== undefined) {
    const e = byId.get(u.engaged);
    if (e) {
      const [ex, ey] = toScreen(s, e.x, e.y);
      if (u.kind === 'art') {
        // Guns fire over the line: a faint arc to what they hit. Guns he has not met show only where their shells fall.
        if (shown) { x.strokeStyle = 'rgba(42,45,47,.28)'; x.lineWidth = 1; x.setLineDash([2, 6]); x.beginPath(); x.moveTo(sx, sy); x.quadraticCurveTo((sx + ex) / 2, Math.min(sy, ey) - 40, ex, ey); x.stroke(); x.setLineDash([]); }
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
  // The side cue (the art and UI directors, ruling 226(12)): the player's own carry a thin paper line inside the ink
  // outline, theirs the ink alone. Colour tells nations apart; this tells sides apart.
  if (mine && w > 10 && d > 7) { x.strokeStyle = 'rgba(241,233,212,.85)'; x.lineWidth = 1; x.strokeRect(-w / 2 + 2, -d / 2 + 2, w - 4, d - 4); x.strokeStyle = INK; }
  x.lineWidth = Math.max(2, Math.min(4, 7 * z));
  x.beginPath(); x.moveTo(-w / 2, -d / 2); x.lineTo(w / 2, -d / 2); x.stroke();
  // Its state, round the edge: brass for chosen, amber for wavering, red for running.
  const ring = picked ? BRASS : !shown ? null : u.state === 'wavering' ? AMBER : routing ? ALARM : null;
  if (ring) {
    x.strokeStyle = ring; x.lineWidth = picked ? 2.5 : 2;
    if (!picked && u.state === 'wavering') x.setLineDash([5, 3]);
    x.strokeRect(-w / 2 - 2.5, -d / 2 - 2.5, w + 5, d + 5); x.setLineDash([]);
  }
  // The sign, once the block is big enough to carry one.
  if (shown && w >= 22 && d >= 9) {
    const sw = Math.min(w * 0.5, d * 1.7), sh = Math.min(d * 0.62, sw / 1.6);
    x.strokeStyle = PAPER; x.fillStyle = PAPER; x.lineWidth = Math.max(1, Math.min(2, sh / 7)); x.lineCap = 'round';
    sign(x, u.kind, sw, sh);
  }
  // TODO(polish): its models. The 3D game stands figures0 models in ranks inside this box and fades one for each share lost (figures).
  // In its works: a short run of the dug line, three notches of its square wave in paper along the block's front edge,
  // inside it (the art director, slice 3), so it never doubles the fortress troops' own notched sign.
  if (shown && inWorks(u, b) && w >= 16) {
    const q = Math.min(1.6, Math.max(1, 3 * z)), step = 2.4 * q, up = 1.6 * q;
    const run = 6 * step, x0 = -run / 2, y0 = -d / 2 + 2 + up;
    x.strokeStyle = PAPER; x.lineWidth = 1; x.lineCap = 'butt'; x.lineJoin = 'miter';
    x.beginPath(); x.moveTo(x0, y0);
    for (let i = 0; i < 6; i++) { const nx = x0 + (i + 1) * step, hi = i % 2 === 0; x.lineTo(nx, y0 - (hi ? up : 0)); if (i < 5) x.lineTo(nx, y0 - (hi ? 0 : up)); }
    x.stroke();
  }
  // Strength at the block's back edge, a fixed gap behind it, turning with it (the UI director, 10): every block's,
  // the enemy's too, so he can size them up (ruling 226(7)), once he has met them (b464(2)).
  if (shown) {
    const bw = Math.max(18, w * 0.8), by = d / 2 + 4;
    x.globalAlpha = 1;
    x.fillStyle = 'rgba(14,11,8,.75)'; x.fillRect(-bw / 2 - 1, by - 1, bw + 2, 5);
    const k = u.str / Math.max(1e-6, u.str0);
    x.fillStyle = k < 0.35 ? ALARM : k < 0.65 ? AMBER : GAIN;
    x.fillRect(-bw / 2, by, bw * Math.max(0, Math.min(1, u.str)), 3);
  }
  x.restore();
}

/**
 * The name, for the one under the pointer (the UI director's words for ruling 226(7)): the enemy's with its country,
 * strength and what to do; his own as built. The state word only when not Steady. Drawn over every block.
 */
function hoverLabel(x: CanvasRenderingContext2D, s: Scene, u: LandUnit, b: LandBattle): void {
  const z = s.cam.z;
  const [sx, sy] = toScreen(s, u.x, u.y);
  const [bw0, bd0] = boxOf(u, b);
  const w = Math.max(8, bw0 * z), d = Math.max(4, bd0 * z);
  const mine = u.side === s.me;
  x.save();
  {
    const st = u.fellBack ? ' · Falling back' : u.state === 'steady' ? '' : ` · ${u.state[0].toUpperCase()}${u.state.slice(1)}`;
    const attack = s.sel.size ? ' · Right-click to attack' : '';
    // One he has not met (b464(2), the UI director's words): his country and "troops", never a division, a strength or a state.
    const label = mine ? `${u.name}${st}` : !b.known(u) ? `${b.ticket[u.side].adj} troops, not identified${attack}` : `${b.ticket[u.side].adj} ${u.name} · Strength ${Math.round(u.str * 100)}${st}${attack}`;
    x.font = '700 12px "Courier Prime", ui-monospace, monospace';
    const tw = x.measureText(label).width;
    // Half the stand's height on the screen as it is turned, so the label sits just clear of it (b464).
    const lx = Math.min(s.W - tw - 14, Math.max(6, sx - tw / 2 - 6)), half = (w * Math.abs(Math.sin(u.face)) + d * Math.abs(Math.cos(u.face))) / 2;
    // His own block's label below it; an enemy's on its far side from his army, clear of other blocks where it can
    // (the UI director, slice 3).
    let ly = sy + half + 10;
    if (!mine) {
      const own = b.units.filter((v) => v.side === s.me && v.state !== 'gone' && v.state !== 'withdrawn');
      const myY = own.length ? own.reduce((a, v) => a + toScreen(s, v.x, v.y)[1], 0) / own.length : sy + 1;
      const away = myY >= sy ? -1 : 1;
      const boxes = b.units.filter((v) => v !== u && v.state !== 'gone' && v.state !== 'withdrawn').map((v) => {
        const [vx, vy] = toScreen(s, v.x, v.y), r = Math.max(v.w, v.d) * z / 2 + 4;
        return [vx - r, vy - r, vx + r, vy + r];
      });
      const clear = (y: number): boolean => !boxes.some((q) => lx < q[2] && lx + tw + 12 > q[0] && y < q[3] && y + 20 > q[1]);
      const at = (k: number): number => (away < 0 ? sy - half - 30 - k * 24 : sy + half + 10 + k * 24);
      ly = at(0);
      for (let k = 0; k < 4; k++) if (clear(at(k))) { ly = at(k); break; }
      ly = Math.max(4, Math.min(s.H - 24, ly));
    }
    // Never under the dispatch, the tag or the zoom group: near the field's top it goes below the stand, 8 clear of
    // them (the UI director, b464).
    const under = (y: number): [number, number, number, number] | undefined => s.avoid?.find((q) => lx < q[2] + 8 && lx + tw + 12 > q[0] - 8 && y < q[3] + 8 && y + 20 > q[1] - 8);
    if (under(ly)) {
      ly = sy + half + 10;
      for (let q = under(ly), k = 0; q && k < 4; q = under(ly), k++) ly = q[3] + 8;
      ly = Math.min(s.H - 24, ly);
    }
    x.fillStyle = '#e2d7bc'; x.strokeStyle = '#cdbf9c'; x.lineWidth = 1;
    x.fillRect(lx, ly, tw + 12, 20); x.strokeRect(lx, ly, tw + 12, 20);
    x.fillStyle = '#2a2118'; x.textAlign = 'left'; x.textBaseline = 'middle'; x.fillText(label, lx + 6, ly + 10.5);
  }
  x.restore();
}

/**
 * The box a block is drawn in: its own, or for an enemy block he has not met the plain size every such block shares, so
 * its depth never gives its arm away (b464(2)).
 */
export function boxOf(u: LandUnit, b: LandBattle): [number, number] {
  return b.known(u) ? [u.w, u.d] : LAND.size.inf;
}

/** A block standing in works: dug in where it began, or on fieldworks or a bunker. */
function inWorks(u: LandUnit, b: LandBattle): boolean {
  if (u.state === 'routing') return false;
  const atHome = Math.hypot(u.x - u.home[0], u.y - u.home[1]);
  if (u.dug && atHome < LAND.dug.keep) return true;
  // The defenders' front row behind fortress works stands in them while it holds its place.
  if (u.side === 'def' && b.ticket.place.fort > 0 && atHome < 40) {
    const front = Math.min(...b.units.filter((x) => x.side === 'def' && x.kind !== 'art').map((x) => x.home[1]));
    if (Math.abs(u.home[1] - front) < 30) return true;
  }
  const g = groundAt(b.field, u.x, u.y);
  return g === 'works' || g === 'bunker';
}

/**
 * The defenders' works (the art director, ruling 226(12)): along the front edge of the line they drew up on, under the
 * blocks and a little past each end, the campaign's fortress notch run on as a thin ink square wave, held at its
 * whole-field size on the screen; where the works carry wire, a row of small ink crosses a short way in front. Drawn
 * where they were dug: they stay if the defenders leave.
 */
function works(x: CanvasRenderingContext2D, b: LandBattle, s: Scene, k: number): void {
  const def = b.units.filter((u) => u.side === 'def' && u.kind !== 'art');
  if (!def.length || !(b.ticket.place.fort > 0 || def.some((u) => u.dug))) return;
  const front = Math.min(...def.map((u) => u.home[1]));
  const row = def.filter((u) => Math.abs(u.home[1] - front) < 30);
  const x0 = Math.min(...row.map((u) => u.home[0] - u.w / 2)) - 40, x1 = Math.max(...row.map((u) => u.home[0] + u.w / 2)) + 40;
  const y = front - Math.max(...row.map((u) => u.d)) / 2;
  const [ax, ay] = toScreen(s, x0, y), [bx] = toScreen(s, x1, y);
  const step = 1.6 * k, up = 0.9 * k;
  x.strokeStyle = INK; x.lineWidth = Math.max(1, 0.25 * k); x.lineJoin = 'miter'; x.lineCap = 'butt';
  x.beginPath(); x.moveTo(ax, ay);
  let px = ax, hi = false;
  while (px < bx) { const nx = Math.min(bx, px + step); x.lineTo(nx, ay - (hi ? up : 0)); if (nx < bx) x.lineTo(nx, ay - (hi ? 0 : up)); hi = !hi; px = nx; }
  x.stroke();
  if (b.ticket.place.fort >= 2 || (b.ticket.place.mines ?? 0) > 0) {
    // Wire: small crosses 30 m in front of the line.
    const [, wy] = toScreen(s, x0, y - 30);
    const c = 0.45 * k;
    x.lineWidth = Math.max(1, 0.2 * k); x.beginPath();
    for (let qx = ax + step; qx < bx; qx += 2.2 * step) { x.moveTo(qx - c, wy - c); x.lineTo(qx + c, wy + c); x.moveTo(qx - c, wy + c); x.lineTo(qx + c, wy - c); }
    x.stroke();
  }
}
