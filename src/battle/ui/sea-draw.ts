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
const FOAM = 'rgba(236,242,244,';

/** A mark on the water that fades: a ship going down, a strike landing, a raid. */
export interface Mark { x: number; y: number; at: number; kind: 'sunk' | 'strike' | 'air' | 'burning'; unit?: number }

/** What the frame keeps between frames: the marks, and when the guns of each piece last flashed. */
export interface SeaScene extends Scene {
  marks: Mark[];
  /** Carriers' strikes in the air: the target, and when they land (battle seconds). */
  inbound: { target: number; lands: number }[];
}

export const live = (u: SeaUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';

/** The marks the frame keeps from the sim's events. */
export function takeEvents(s: SeaScene, sim: SeaSim, events: FieldEvent[], realNow: number): void {
  for (const e of events) {
    if (e.what === 'sea:sunk' && e.x !== undefined) s.marks.push({ x: e.x, y: e.y!, at: realNow, kind: 'sunk' });
    if (e.what === 'sea:strike' && e.x !== undefined) s.marks.push({ x: e.x, y: e.y!, at: realNow, kind: 'strike' });
    if (e.what === 'sea:air' && e.x !== undefined) s.marks.push({ x: e.x, y: e.y!, at: realNow, kind: 'air' });
    if (e.what === 'sea:burning' && e.unit !== undefined) s.marks.push({ x: e.x ?? 0, y: e.y ?? 0, at: realNow, kind: 'burning', unit: e.unit });
    if (e.what === 'ability' && (e.extra as { key?: string } | undefined)?.key === 'strike') {
      const target = (e.extra as { target?: number }).target;
      if (target !== undefined) s.inbound.push({ target, lands: sim.t + SEA.strike.windup });
    }
  }
  s.inbound = s.inbound.filter((b) => b.lands > sim.t);
  s.marks = s.marks.filter((m) => realNow - m.at < (m.kind === 'sunk' ? 30 : 6));
}

/** The smallest a ship is drawn, in pixels, by class, so that a whole fleet reads at the widest view. */
const MIN_LEN: Record<SeaClass, number> = { bb: 16, cv: 16, ca: 12, dd: 8 };
const MIN_BEAM: Record<SeaClass, number> = { bb: 4.2, cv: 4.6, ca: 3.4, dd: 2.6 };

/** One hull, centred on the origin and pointing up the screen (north), with its class's marks. */
function hull(x: CanvasRenderingContext2D, cls: SeaClass, L: number, B: number, fill: string, edge: string): void {
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
  x.lineWidth = 1; x.strokeStyle = edge; x.stroke();
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
  // Open water, darker at the edges of the field; the ruled squares are a kilometre each, for judging the range.
  x.fillStyle = '#0e0b08'; x.fillRect(0, 0, W, H);
  const [x0, y0] = toScreen(s, 0, F.size), [x1, y1] = toScreen(s, F.size, 0);
  const g = x.createRadialGradient((x0 + x1) / 2, (y0 + y1) / 2, 0, (x0 + x1) / 2, (y0 + y1) / 2, Math.max(x1 - x0, y1 - y0) * 0.75);
  g.addColorStop(0, '#3e6075'); g.addColorStop(1, '#253f50');
  x.fillStyle = g; x.fillRect(x0, y0, x1 - x0, y1 - y0);
  x.strokeStyle = 'rgba(236,242,244,.07)'; x.lineWidth = 1;
  x.beginPath();
  for (let m = 1000; m < F.size; m += 1000) {
    const [a] = toScreen(s, m, 0), [, b] = toScreen(s, 0, m);
    x.moveTo(a, y0); x.lineTo(a, y1); x.moveTo(x0, b); x.lineTo(x1, b);
  }
  x.stroke();
  // The swell: short strokes that drift slowly, so a still sea is not a dead one.
  x.strokeStyle = 'rgba(236,242,244,.06)'; x.lineWidth = 1.2;
  x.beginPath();
  const drift = (sim.t * 2) % 1400;
  for (let gy = 700; gy < F.size; gy += 1400) for (let gx = 300 + ((gy / 1400) % 2) * 700; gx < F.size; gx += 1400) {
    const [a, b] = toScreen(s, gx + drift * 0.3, gy + drift * 0.1);
    const l = Math.max(4, 240 * cam.z);
    x.moveTo(a - l, b); x.quadraticCurveTo(a, b - l * 0.18, a + l, b);
  }
  x.stroke();
  // Each side's edge, by which its ships leave for port.
  x.font = '700 11px "Courier Prime", monospace'; x.fillStyle = 'rgba(236,242,244,.45)'; x.textAlign = 'center';
  x.fillText(`${sim.ticket.def.adj.toUpperCase()} PORTS`, (x0 + x1) / 2, Math.max(14, y0 + 14));
  x.fillText(`${sim.ticket.att.adj.toUpperCase()} PORTS`, (x0 + x1) / 2, Math.min(H - 8, y1 - 6));

  const colour: Record<Side, string> = { att: sim.ticket.att.color, def: sim.ticket.def.color };
  const byId = new Map(sim.units.map((u) => [u.id, u]));

  // Reach for what is selected: the range it hits hardest at, and the furthest it hits at all.
  for (const id of s.sel) {
    const u = byId.get(id);
    if (!u || !live(u) || u.extra.cls === 'cv') continue;
    const C = SEA.classes[u.extra.cls as SeaClass];
    const [cx, cy] = toScreen(s, u.x, u.y);
    x.strokeStyle = 'rgba(224,184,114,.35)'; x.lineWidth = 1;
    x.beginPath(); x.arc(cx, cy, C.best * cam.z, 0, Math.PI * 2); x.stroke();
    x.setLineDash([5, 6]); x.beginPath(); x.arc(cx, cy, C.reach * cam.z, 0, Math.PI * 2); x.stroke(); x.setLineDash([]);
  }

  // The player's orders: where his ships are going, in brass; whom they are going for, in red.
  for (const u of sim.units) {
    if (u.side !== s.me || !live(u)) continue;
    const o = sim.orderOf(u.id);
    if (!o || o.kind === 'free' || o.kind === 'hold') continue;
    const strong = s.sel.has(u.id);
    let end: [number, number] | null = null;
    if (o.kind === 'move') end = [o.x, o.y];
    else if (o.kind === 'attack') { const t = byId.get(o.target); if (t && live(t)) end = [t.x, t.y]; }
    if (!end) continue;
    x.strokeStyle = o.kind === 'attack' ? `rgba(196,85,63,${strong ? 0.95 : 0.5})` : `rgba(224,184,114,${strong ? 0.95 : 0.5})`;
    x.lineWidth = strong ? 2 : 1.2; x.setLineDash([7, 5]);
    const [a, b] = toScreen(s, u.x, u.y), [c, d] = toScreen(s, end[0], end[1]);
    x.beginPath(); x.moveTo(a, b); x.lineTo(c, d); x.stroke(); x.setLineDash([]);
    if (o.kind === 'attack') { x.beginPath(); x.arc(c, d, 9, 0, Math.PI * 2); x.stroke(); }
    else { x.beginPath(); x.arc(c, d, 2.5, 0, Math.PI * 2); x.fillStyle = x.strokeStyle; x.fill(); }
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
      gr.addColorStop(0, `${FOAM}.55)`); gr.addColorStop(1, `${FOAM}0)`);
      x.strokeStyle = gr; x.lineWidth = Math.max(1.5, C.beam * cam.z * 1.4);
      x.beginPath(); x.moveTo(a, b); x.lineTo(c, d); x.stroke();
    }
  }

  // The guns: a flash at the firing ship and splashes round the one it is firing on, now and then.
  for (const u of sim.units) {
    if (!live(u) || u.engaged === undefined) continue;
    const t = byId.get(u.engaged);
    if (!t) continue;
    const phase = (s.now * (u.extra.cls === 'dd' ? 2.2 : 1.1) + u.id * 0.37) % 1;
    const [a, b] = toScreen(s, u.x, u.y), [c, d] = toScreen(s, t.x, t.y);
    x.strokeStyle = `rgba(240,166,58,${0.10 + 0.12 * (1 - phase)})`; x.lineWidth = 1;
    x.beginPath(); x.moveTo(a, b); x.lineTo(c, d); x.stroke();
    if (phase < 0.18) { x.fillStyle = `rgba(255,214,140,${0.9 - phase * 4})`; x.beginPath(); x.arc(a, b, 3.5, 0, Math.PI * 2); x.fill(); }
    if (phase > 0.55 && phase < 0.85) {
      const k = (phase - 0.55) / 0.3;
      const spread = Math.max(6, 160 * cam.z);
      for (let i = 0; i < 3; i++) {
        const ang = (u.id * 1.7 + i * 2.1 + Math.floor(s.now * 1.1)) % (Math.PI * 2);
        x.strokeStyle = `${FOAM}${0.7 * (1 - k)})`; x.lineWidth = 1.2;
        x.beginPath(); x.arc(c + Math.cos(ang) * spread * 0.6, d + Math.sin(ang) * spread * 0.4, 1.5 + 4 * k, 0, Math.PI * 2); x.stroke();
      }
    }
  }

  // The hulls: the departed and the sunk are not drawn; the enemy first, the player's own on top.
  const order = sim.units.filter(live).sort((a, c) => (a.side === s.me ? 1 : 0) - (c.side === s.me ? 1 : 0));
  for (const u of order) {
    const cls = u.extra.cls as SeaClass, C = SEA.classes[cls];
    const L = Math.max(MIN_LEN[cls], C.len * cam.z), B = Math.max(MIN_BEAM[cls], C.beam * cam.z * 1.6);
    const edge = u.state === 'routing' ? ALARM : u.state === 'wavering' ? AMBER : 'rgba(14,11,8,.85)';
    for (const [sx, sy] of shipsOf(u)) {
      const [a, b] = toScreen(s, sx, sy);
      x.save(); x.translate(a, b); x.rotate(u.face);
      hull(x, cls, L, B, colour[u.side], edge);
      x.restore();
    }
    const [a, b] = toScreen(s, u.x, u.y);
    if (s.sel.has(u.id) || s.hover === u.id) {
      x.strokeStyle = s.sel.has(u.id) ? BRASS : 'rgba(241,233,212,.7)'; x.lineWidth = s.sel.has(u.id) ? 2 : 1.2;
      x.beginPath(); x.arc(a, b, L * 0.75 + 4, 0, Math.PI * 2); x.stroke();
    }
    // The hull meter under a piece that is hurt, and its name when it is chosen or under the pointer.
    if (u.str < 0.999) {
      const w = 22;
      x.fillStyle = 'rgba(14,11,8,.7)'; x.fillRect(a - w / 2, b + L * 0.6 + 3, w, 3);
      x.fillStyle = u.str < 0.35 ? ALARM : u.str < 0.65 ? AMBER : '#9cc08a'; x.fillRect(a - w / 2, b + L * 0.6 + 3, w * u.str, 3);
    }
    if (s.sel.has(u.id) || s.hover === u.id) {
      x.font = '700 12px "Courier Prime", monospace'; x.textAlign = 'center'; x.textBaseline = 'bottom';
      x.lineWidth = 3; x.strokeStyle = 'rgba(14,11,8,.85)'; x.strokeText(u.name, a, b - L * 0.6 - 6);
      x.fillStyle = '#ede6d6'; x.fillText(u.name, a, b - L * 0.6 - 6);
    }
  }

  // Strikes on their way in: a ring closing on the target as they come.
  for (const ib of s.inbound) {
    const t = byId.get(ib.target);
    if (!t || !live(t)) continue;
    const k = Math.max(0, Math.min(1, (ib.lands - sim.t) / SEA.strike.windup));
    const [a, b] = toScreen(s, t.x, t.y);
    x.strokeStyle = `rgba(224,184,114,${0.5 + 0.4 * (1 - k)})`; x.lineWidth = 1.5; x.setLineDash([3, 4]);
    x.beginPath(); x.arc(a, b, 12 + 40 * k, 0, Math.PI * 2); x.stroke(); x.setLineDash([]);
  }

  // What has happened on the water lately.
  for (const m of s.marks) {
    const age = s.now - m.at;
    let mx = m.x, my = m.y;
    if (m.kind === 'burning' && m.unit !== undefined) { const u = byId.get(m.unit); if (u) { mx = u.x; my = u.y; } }
    const [a, b] = toScreen(s, mx, my);
    if (m.kind === 'sunk') {
      // Where a ship went down: a widening ring, then a slick that fades.
      const k = Math.min(1, age / 4);
      x.strokeStyle = `${FOAM}${0.8 * (1 - k)})`; x.lineWidth = 1.5;
      x.beginPath(); x.arc(a, b, 4 + 26 * k, 0, Math.PI * 2); x.stroke();
      x.fillStyle = `rgba(20,24,26,${0.45 * (1 - age / 30)})`;
      x.beginPath(); x.ellipse(a, b, 9, 5, 0.4, 0, Math.PI * 2); x.fill();
    } else if (m.kind === 'strike' || m.kind === 'air') {
      const k = Math.min(1, age / 1.5);
      x.fillStyle = `rgba(255,214,140,${0.75 * (1 - k)})`;
      for (let i = 0; i < 5; i++) { const ang = i * 1.26 + m.at; x.beginPath(); x.arc(a + Math.cos(ang) * 10 * k, b + Math.sin(ang) * 7 * k, 3 * (1 - k) + 1, 0, Math.PI * 2); x.fill(); }
      x.strokeStyle = `${FOAM}${0.6 * (1 - k)})`; x.beginPath(); x.arc(a, b, 6 + 20 * k, 0, Math.PI * 2); x.stroke();
    } else if (m.kind === 'burning') {
      const flick = 0.5 + 0.5 * Math.sin(s.now * 9 + m.at);
      x.fillStyle = `rgba(240,120,40,${0.5 * (1 - age / 6) * flick})`;
      x.beginPath(); x.arc(a, b, 5, 0, Math.PI * 2); x.fill();
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
    x.fillStyle = 'rgba(224,184,114,.10)'; x.fillRect(a, b, c - a, d - b);
    x.strokeStyle = BRASS; x.lineWidth = 1; x.strokeRect(a, b, c - a, d - b);
  }
  if (s.ping && s.now - s.ping.at < 0.6) {
    const k = (s.now - s.ping.at) / 0.6;
    const [a, b] = toScreen(s, s.ping.x, s.ping.y);
    x.strokeStyle = s.ping.kind === 'attack' ? `rgba(196,85,63,${1 - k})` : `rgba(224,184,114,${1 - k})`;
    x.lineWidth = 2; x.beginPath(); x.arc(a, b, 6 + 16 * k, 0, Math.PI * 2); x.stroke();
  }
  x.restore();
}

/** A ship's outline for the cards, by class, in its nation's colour. */
export function shipSvg(cls: SeaClass, color: string): string {
  const body = cls === 'cv'
    ? `<path d="M2 4.5H25L29 7L25 9.5H2Z" fill="${color}" stroke="#15181b" stroke-width=".8"/><rect x="15" y="1.6" width="4" height="3" fill="${color}" stroke="#15181b" stroke-width=".6"/>`
    : `<path d="M1 7Q4 3.8 9 3.8H24Q28 4.6 31 7Q28 9.4 24 10.2H9Q4 10.2 1 7Z" fill="${color}" stroke="#15181b" stroke-width=".8"/>`;
  const marks = cls === 'bb' ? '<circle cx="9" cy="7" r="1.6"/><circle cx="13" cy="7" r="1.4"/><circle cx="23" cy="7" r="1.6"/>'
    : cls === 'ca' ? '<circle cx="10" cy="7" r="1.4"/><circle cx="22" cy="7" r="1.4"/>'
      : cls === 'dd' ? '<circle cx="12" cy="7" r="1"/>' : '';
  return `<svg class="bsign" width="34" height="16" viewBox="0 0 32 14" aria-hidden="true">${body}<g fill="#f1e9d4">${marks}</g></svg>`;
}
