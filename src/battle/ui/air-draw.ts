// One frame of an air battle, seen from above: the ground under the camera, the target and the guns' ring round it,
// each air group as a formation of aircraft in its nation's colour (bombers in vics of three, fighters in pairs and
// fours), its height shown by its shadow on the ground and, on the high band, its condensation trails; the fights,
// the flak, aircraft going down and parachutes, the player's orders and what is selected. Everything is read from the
// sim's view: this file decides nothing.
// TODO(polish): the browser's stand-in for the 3D sky (3d-handoff.md section 2); the 3D game draws the same view with
// the models (figures, per, alt, act, speed, engaged are there for it).

import { ahead } from '../core/geometry';
import type { BattleView, FieldEvent, Side } from '../core/types';
import { SKY } from '../air/book';
import type { AirSim } from '../air/sim';
import type { AirUnit } from '../air/types';
import { toScreen, type Scene } from './draw';

const BRASS = '#e0b872';
const ALARM = '#c4553f';
const AMBER = '#f0a63a';
const PAPER = '#f1e9d4';
const SMOKE = 'rgba(70,66,60,';

/** Something in the sky that fades: an aircraft going down, parachutes, bombs on the target, a dive from above. */
export interface Mark {
  x: number; y: number; at: number;
  kind: 'explode' | 'burn' | 'spin' | 'chutes' | 'bombs' | 'bounce';
  /** Which way it was heading (radians, 0 north, clockwise), how many canopies, and the side it was on. */
  face?: number; n?: number; side?: Side;
}

/** What the frame keeps between frames. */
export interface AirScene extends Scene {
  marks: Mark[];
  /** Where the pointer is, in field metres, for the order it would give ("Attack ...", "Patrol here"). */
  at: [number, number] | null;
}

export const live = (u: AirUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';

/** The marks the frame keeps from the sim's events. */
export function takeEvents(s: AirScene, sim: AirSim, events: FieldEvent[], realNow: number): void {
  for (const e of events) {
    if (e.x === undefined || e.y === undefined) continue;
    const ex = (e.extra ?? {}) as { kind?: string; chutes?: number };
    const u = e.unit !== undefined ? sim.units.find((v) => v.id === e.unit) : undefined;
    if (e.what === 'air:lost') {
      const kind = ex.kind === 'explode' || ex.kind === 'burn' || ex.kind === 'spin' ? ex.kind : 'burn';
      s.marks.push({ x: e.x, y: e.y, at: realNow, kind, face: u?.face ?? 0, side: e.side });
      if (ex.chutes) s.marks.push({ x: e.x, y: e.y, at: realNow + 0.8, kind: 'chutes', n: ex.chutes, face: u?.face ?? 0 });
    }
    if (e.what === 'air:bombs') s.marks.push({ x: SKY.target.x + (Math.random() - 0.5) * SKY.target.r, y: SKY.target.y + (Math.random() - 0.5) * SKY.target.r, at: realNow, kind: 'bombs' });
    if (e.what === 'air:bounce') s.marks.push({ x: e.x, y: e.y, at: realNow, kind: 'bounce' });
  }
  const life: Record<Mark['kind'], number> = { explode: 2, burn: 7, spin: 4, chutes: 14, bombs: 1e9, bounce: 2 };
  s.marks = s.marks.filter((m) => realNow - m.at < life[m.kind]);
}

/** Where each model of a group flies, in metres from the group's place, before turning to its heading (north up). */
export function slots(u: AirUnit, gapM: number): [number, number][] {
  const out: [number, number][] = [];
  const n = Math.max(0, u.figures);
  if (u.air === 'bmr') {
    // Vics of three, stepped back; the vics side by side in rows of three, each row behind the last.
    for (let k = 0; k < n; k++) {
      const v = Math.floor(k / 3), j = k % 3, col = v % 3, row = Math.floor(v / 3);
      const vx = (col - 1) * gapM * 3.2, vy = -row * gapM * 2.6 - (col === 1 ? 0 : gapM * 0.6);
      const ox = j === 0 ? 0 : j === 1 ? -gapM : gapM, oy = j === 0 ? 0 : -gapM * 0.9;
      out.push([vx + ox, vy + oy]);
    }
  } else {
    // Loose pairs, two pairs to a four, the fours side by side.
    for (let k = 0; k < n; k++) {
      const four = Math.floor(k / 4), p = Math.floor((k % 4) / 2), j = k % 2;
      const fx = (four % 3 - 1) * gapM * 4.2 + (p ? gapM * 1.8 : 0), fy = -Math.floor(four / 3) * gapM * 3 - (p ? gapM * 0.7 : 0);
      out.push([fx + (j ? gapM * 0.9 : 0), fy - (j ? gapM * 0.8 : 0)]);
    }
  }
  return out;
}

/** One aircraft from above, nose up the screen: a fighter's slim cross, a bomber's broad wing and its engines. */
function plane(x: CanvasRenderingContext2D, bmr: boolean, S: number, fill: string, edge: string): void {
  x.fillStyle = fill; x.strokeStyle = edge; x.lineWidth = 0.8;
  x.beginPath();
  if (bmr) {
    const span = S, len = S * 0.72, chord = S * 0.16;
    x.moveTo(0, -len / 2); x.lineTo(chord * 0.35, -len * 0.2);
    x.lineTo(span / 2, -len * 0.08); x.lineTo(span / 2, chord - len * 0.08); x.lineTo(chord * 0.35, len * 0.04);
    x.lineTo(chord * 0.3, len * 0.38); x.lineTo(span * 0.2, len * 0.45); x.lineTo(span * 0.2, len / 2); x.lineTo(-span * 0.2, len / 2); x.lineTo(-span * 0.2, len * 0.45); x.lineTo(-chord * 0.3, len * 0.38);
    x.lineTo(-chord * 0.35, len * 0.04); x.lineTo(-span / 2, chord - len * 0.08); x.lineTo(-span / 2, -len * 0.08); x.lineTo(-chord * 0.35, -len * 0.2);
  } else {
    const span = S, len = S * 0.85, chord = S * 0.2;
    x.moveTo(0, -len / 2); x.lineTo(chord * 0.3, -len * 0.18);
    x.lineTo(span / 2, -len * 0.06); x.lineTo(span / 2, chord - len * 0.06); x.lineTo(chord * 0.28, len * 0.1);
    x.lineTo(chord * 0.22, len * 0.36); x.lineTo(span * 0.18, len * 0.44); x.lineTo(span * 0.18, len / 2); x.lineTo(-span * 0.18, len / 2); x.lineTo(-span * 0.18, len * 0.44); x.lineTo(-chord * 0.22, len * 0.36);
    x.lineTo(-chord * 0.28, len * 0.1); x.lineTo(-span / 2, chord - len * 0.06); x.lineTo(-span / 2, -len * 0.06); x.lineTo(-chord * 0.3, -len * 0.18);
  }
  x.closePath(); x.fill(); x.stroke();
  if (bmr && S >= 9) {
    x.fillStyle = 'rgba(14,11,8,.55)';
    for (const ex of [-0.3, -0.16, 0.16, 0.3]) { x.beginPath(); x.arc(ex * S, -S * 0.1, Math.max(0.6, S * 0.035), 0, Math.PI * 2); x.fill(); }
  }
}

/** The ground's woods and fields: fixed for every battle, so the same sky always lies over the same country. */
const GROUND: { x: number; y: number; r: number; turn: number; a: number; wood: boolean }[] = (() => {
  const out: { x: number; y: number; r: number; turn: number; a: number; wood: boolean }[] = [];
  let seed = 7;
  const rnd = (): number => { seed = (seed * 16807) % 2147483647; return seed / 2147483647; };
  for (let i = 0; i < 420; i++) out.push({ x: rnd() * SKY.w, y: rnd() * SKY.h, r: 600 + rnd() * 2_400, turn: rnd() * Math.PI, a: 0.18 + rnd() * 0.22, wood: rnd() < 0.55 });
  return out;
})();

/** The smallest an aircraft is drawn, and the least gap between them, in pixels, so a formation reads at the widest view. */
const MIN = { ftr: { size: 8, gap: 7 }, bmr: { size: 12, gap: 10 } };
/** How far a shadow falls from its aircraft on each band, in pixels at the widest view and growing as the camera closes. */
const SHADOW = [3, 7, 12];

export function drawAir(x: CanvasRenderingContext2D, sim: AirSim, v: BattleView<AirUnit>, s: AirScene): void {
  const { cam, W, H } = s;
  x.save();
  // The ground far below at dusk: fields and woods in the dark, the ruled squares ten kilometres each.
  x.fillStyle = '#0e0b08'; x.fillRect(0, 0, W, H);
  const [x0, y0] = toScreen(s, 0, SKY.h), [x1, y1] = toScreen(s, SKY.w, 0);
  const g = x.createRadialGradient((x0 + x1) / 2, (y0 + y1) / 2, 0, (x0 + x1) / 2, (y0 + y1) / 2, Math.max(x1 - x0, y1 - y0) * 0.75);
  g.addColorStop(0, '#4a4a35'); g.addColorStop(1, '#2e2d22');
  x.fillStyle = g; x.fillRect(x0, y0, x1 - x0, y1 - y0);
  // Woods and open fields, fixed to the ground so they slide under the aircraft as the camera moves, and a river.
  for (const p of GROUND) {
    const [a, b] = toScreen(s, p.x, p.y);
    const rx = p.r * cam.z, ry = p.r * 0.62 * cam.z;
    if (a + rx < 0 || a - rx > W || b + rx < 0 || b - rx > H) continue;
    x.fillStyle = p.wood ? `rgba(24,34,20,${p.a})` : `rgba(120,108,72,${p.a * 0.6})`;
    x.beginPath(); x.ellipse(a, b, rx, ry, p.turn, 0, Math.PI * 2); x.fill();
  }
  x.strokeStyle = 'rgba(120,150,160,.28)'; x.lineWidth = Math.max(1.5, 400 * cam.z);
  x.beginPath();
  for (let i = 0; i <= 60; i++) {
    const fy = (i / 60) * SKY.h, fx = 78_000 + Math.sin(fy * 0.00009) * 6_000 + Math.sin(fy * 0.00031) * 1_800;
    const [a, b] = toScreen(s, fx, fy);
    if (i) x.lineTo(a, b); else x.moveTo(a, b);
  }
  x.stroke();
  x.strokeStyle = 'rgba(241,233,212,.06)'; x.lineWidth = 1;
  x.beginPath();
  for (let m = 10_000; m < SKY.w; m += 10_000) {
    const [a] = toScreen(s, m, 0), [, b] = toScreen(s, 0, m);
    x.moveTo(a, y0); x.lineTo(a, y1); x.moveTo(x0, b); x.lineTo(x1, b);
  }
  x.stroke();
  // By night the ground goes under a deep blue wash, and the aircraft cast no shadows on it.
  const night = sim.place.night;
  if (night) { x.fillStyle = 'rgba(6,10,26,.62)'; x.fillRect(x0, y0, x1 - x0, y1 - y0); }
  // Under the board's left corner, clear of the raid, which comes in at the middle of the lower edge.
  x.font = '700 11px "Courier Prime", monospace'; x.fillStyle = 'rgba(241,233,212,.45)'; x.textAlign = 'left'; x.textBaseline = 'alphabetic';
  x.fillText(`FROM ${sim.ticket.att.adj.toUpperCase()} AIRFIELDS ↑`, Math.max(x0, 8) + 2, y1 + 16 <= H - 4 ? y1 + 16 : y1 - 6);

  const T = SKY.target;
  const [tx, ty] = toScreen(s, T.x, T.y);
  // The defenders' airfields: a short runway where each of their groups is based.
  const fields = new Set<string>();
  for (const u of sim.units) {
    if (u.side !== 'def') continue;
    const key = `${Math.round(u.homeX / 1000)}:${Math.round(u.homeY / 1000)}`;
    if (fields.has(key)) continue;
    fields.add(key);
    const [a, b] = toScreen(s, u.homeX, u.homeY);
    x.strokeStyle = 'rgba(241,233,212,.35)'; x.lineWidth = 3; x.beginPath(); x.moveTo(a - 6, b + 2); x.lineTo(a + 6, b - 2); x.stroke();
  }
  // The town under the target: a few blocks, and the black smoke of what has hit it.
  x.fillStyle = 'rgba(160,150,130,.55)';
  for (let i = 0; i < 9; i++) { const a = tx + Math.cos(i * 2.4) * (2 + i * 1.6), b = ty + Math.sin(i * 2.4) * (2 + i * 1.3); x.fillRect(a - 2, b - 2, 4, 4); }
  x.strokeStyle = 'rgba(241,233,212,.55)'; x.lineWidth = 1;
  x.beginPath(); x.arc(tx, ty, T.r * cam.z, 0, Math.PI * 2); x.stroke();
  x.font = '700 11px "Courier Prime", monospace'; x.fillStyle = 'rgba(241,233,212,.75)';
  x.fillText(sim.place.name.toUpperCase(), tx, ty - T.r * cam.z - 6);
  // The guns' ring: the one dashed red ring on the board (the art director).
  x.strokeStyle = 'rgba(196,85,63,.6)'; x.setLineDash([6, 6]);
  x.beginPath(); x.arc(tx, ty, SKY.flakR * cam.z, 0, Math.PI * 2); x.stroke(); x.setLineDash([]);

  const colour: Record<Side, string> = { att: sim.ticket.att.color, def: sim.ticket.def.color };
  const units = v.units as AirUnit[];
  const byId = new Map(units.map((u) => [u.id, u]));
  const flyingNow = (u: AirUnit): boolean => live(u) && u.act !== 'idle';

  // The player's orders: whom his fighters are going for, in red; where they hold the sky, in brass.
  for (const u of units) {
    if (u.side !== s.me || !flyingNow(u)) continue;
    const strong = s.sel.has(u.id);
    let end: [number, number] | null = null, red = false;
    if (u.job.do === 'intercept') { const t = byId.get(u.job.on); if (t && live(t)) { end = [t.x, t.y]; red = true; } }
    else if (u.job.do === 'patrol' && strong) end = [u.job.x, u.job.y];
    else if (u.job.do === 'route' && strong) end = [T.x + u.lane, T.y];
    if (!end) continue;
    x.strokeStyle = red ? `rgba(196,85,63,${strong ? 0.95 : 0.5})` : `rgba(224,184,114,${strong ? 0.95 : 0.5})`;
    x.lineWidth = strong ? 2 : 1.2; x.setLineDash([7, 5]);
    const [a, b] = toScreen(s, u.x, u.y), [c, d] = toScreen(s, end[0], end[1]);
    x.beginPath(); x.moveTo(a, b); x.lineTo(c, d); x.stroke(); x.setLineDash([]);
    if (red) { x.beginPath(); x.arc(c, d, 10, 0, Math.PI * 2); x.stroke(); }
    else { x.beginPath(); x.arc(c, d, 2.5, 0, Math.PI * 2); x.fillStyle = x.strokeStyle; x.fill(); }
  }

  // The fights: a thin pale ring where groups are locked together, and the tracer between them now and then.
  const rung = new Set<string>();
  for (const u of units) {
    if (!flyingNow(u) || u.engaged === undefined) continue;
    const t = byId.get(u.engaged);
    if (!t) continue;
    const [a, b] = toScreen(s, u.x, u.y), [c, d] = toScreen(s, t.x, t.y);
    const key = [u.id, t.id].sort().join(':');
    if (!rung.has(key)) {
      rung.add(key);
      const r = Math.max(16, Math.hypot(c - a, d - b) / 2 + 10);
      x.strokeStyle = 'rgba(241,233,212,.28)'; x.lineWidth = 1;
      x.beginPath(); x.arc((a + c) / 2, (b + d) / 2, r, 0, Math.PI * 2); x.stroke();
    }
    const phase = (s.now * 3.1 + u.id * 0.37) % 1;
    if (phase < 0.35) {
      x.strokeStyle = `rgba(255,214,140,${0.6 * (1 - phase / 0.35)})`; x.lineWidth = 1;
      const k0 = phase, k1 = phase + 0.25;
      x.beginPath(); x.moveTo(a + (c - a) * k0, b + (d - b) * k0); x.lineTo(a + (c - a) * k1, b + (d - b) * k1); x.stroke();
    }
  }

  // The flak: bursts of the hanging smoke round the boxes inside the guns' ring.
  for (const u of units) {
    if (!flyingNow(u) || u.side !== 'att' || u.air !== 'bmr') continue;
    if (Math.hypot(u.x - T.x, u.y - T.y) > SKY.flakR) continue;
    const [a, b] = toScreen(s, u.x, u.y);
    for (let i = 0; i < 5; i++) {
      const ph = (s.now * 0.7 + i * 0.21 + u.id * 0.13) % 1;
      const ang = i * 2.39 + Math.floor(s.now * 0.7 + i * 0.21 + u.id * 0.13) * 1.7;
      const rr = 10 + 22 * ((i * 37) % 10) / 10;
      x.fillStyle = `${SMOKE}${0.55 * (1 - ph)})`;
      x.beginPath(); x.arc(a + Math.cos(ang) * rr, b + Math.sin(ang) * rr * 0.8, 2 + 5 * ph, 0, Math.PI * 2); x.fill();
      if (ph < 0.08) { x.fillStyle = 'rgba(255,190,110,.9)'; x.beginPath(); x.arc(a + Math.cos(ang) * rr, b + Math.sin(ang) * rr * 0.8, 1.6, 0, Math.PI * 2); x.fill(); }
    }
  }

  // The formations: shadows on the ground first, then contrails, then the aircraft. Bombers go down first, fighters in
  // a fight last, so the dogfight is never hidden under a bomber box; within each, the enemy under the player's own.
  const fighting = (u: AirUnit) => (u.air === 'ftr' && u.act === 'fight' ? 1 : 0);
  const order = units.filter(live).sort((a, c) =>
    fighting(a) - fighting(c) || (a.side === s.me ? 1 : 0) - (c.side === s.me ? 1 : 0) || a.alt - c.alt);
  const placed = new Map<number, { pts: [number, number, number][]; S: number }>();
  for (const u of order) {
    const k = u.air === 'bmr' ? MIN.bmr : MIN.ftr;
    const S = Math.max(k.size, (u.air === 'bmr' ? 30 : 11) * cam.z);
    const gapPx = Math.max(k.gap, SKY.look.gap[u.air] * cam.z);
    let pts: [number, number, number][];
    if (u.air === 'ftr' && u.act === 'fight') {
      // Fighters in a fight leave their formation and swirl round it, each side turning its own way, so the two
      // colours mix where they are locked together (3d-handoff.md: "swirl its models round the fight's centre").
      const n = Math.max(1, u.figures), dir = u.side === 'att' ? 1 : -1, w = 0.9 * dir;
      pts = Array.from({ length: n }, (_, i) => {
        const th = (2 * Math.PI * i) / n + s.now * w + u.id;
        // The two sides turn on different rings, so neither colour sits exactly on top of the other.
        const r = ((dir > 0 ? 14 : 24) + 14 * (((i * 7) % 5) / 5)) / cam.z;
        return [u.x + Math.cos(th) * r, u.y + Math.sin(th) * r, dir > 0 ? -th : Math.PI - th] as [number, number, number];
      });
    } else {
      pts = slots(u, gapPx / cam.z).map(([ox, oy]) => {
        // Turn the formation to its heading: x east, y north, face clockwise from north.
        const c = Math.cos(u.face), sn = Math.sin(u.face);
        return [u.x + ox * c + oy * sn, u.y - ox * sn + oy * c, u.face] as [number, number, number];
      });
    }
    placed.set(u.id, { pts, S });
    if (u.act === 'idle' || night) continue;
    const off = SHADOW[u.band] * Math.max(1, cam.z / 0.02);
    x.save();
    x.globalAlpha = 0.28;
    for (const [px, py, f] of pts) {
      const [a, b] = toScreen(s, px, py);
      x.save(); x.translate(a + off, b + off * 0.8); x.rotate(f); plane(x, u.air === 'bmr', S, '#0b0906', 'rgba(0,0,0,0)'); x.restore();
    }
    x.restore();
  }
  // Condensation trails, on the high band only (the art director).
  for (const u of order) {
    if (u.band !== 2 || !flyingNow(u) || u.act === 'fight') continue;
    const [ax, ay] = ahead(u.face);
    const len = Math.max(30, 2_500 * cam.z) * Math.min(1.5, u.speed / SKY.speed.bmr);
    for (const [px, py] of placed.get(u.id)!.pts) {
      const [a, b] = toScreen(s, px, py), [c, d] = toScreen(s, px - ax * len / cam.z, py - ay * len / cam.z);
      const gr = x.createLinearGradient(a, b, c, d);
      gr.addColorStop(0, 'rgba(241,240,236,.5)'); gr.addColorStop(1, 'rgba(241,240,236,0)');
      x.strokeStyle = gr; x.lineWidth = 1.4;
      x.beginPath(); x.moveTo(a, b); x.lineTo(c, d); x.stroke();
    }
  }
  for (const u of order) {
    const { pts, S } = placed.get(u.id)!;
    // A pale edge, so a dark nation's colour still reads against the ground at dusk; amber wavering, red breaking.
    const edge = u.state === 'routing' ? ALARM : u.state === 'wavering' ? AMBER : 'rgba(236,230,214,.85)';
    for (const [px, py, f] of pts) {
      const [a, b] = toScreen(s, px, py);
      x.save(); x.translate(a, b); x.rotate(f);
      x.globalAlpha = u.act === 'idle' ? 0.55 : 1;
      plane(x, u.air === 'bmr', S, colour[u.side], edge);
      x.restore();
    }
    const [a, b] = toScreen(s, u.x, u.y);
    const rad = Math.max(...pts.map(([px, py]) => { const [c, d] = toScreen(s, px, py); return Math.hypot(c - a, d - b); }), 0) + S * 0.7 + 4;
    if (s.sel.has(u.id) || s.hover === u.id) {
      x.strokeStyle = s.sel.has(u.id) ? BRASS : 'rgba(241,233,212,.7)'; x.lineWidth = s.sel.has(u.id) ? 2 : 1.2;
      x.beginPath(); x.arc(a, b, rad, 0, Math.PI * 2); x.stroke();
    }
    // The strength meter under a group that has lost aircraft, and its name when it is chosen or under the pointer.
    if (u.n < u.n0) {
      const w = 24;
      x.fillStyle = 'rgba(14,11,8,.7)'; x.fillRect(a - w / 2, b + rad + 3, w, 3);
      x.fillStyle = u.str < 0.35 ? ALARM : u.str < 0.65 ? AMBER : '#9cc08a'; x.fillRect(a - w / 2, b + rad + 3, w * Math.max(0, u.n / u.n0), 3);
    }
    if (s.sel.has(u.id) || s.hover === u.id) {
      const label = `${u.name} · ${u.n} ${u.air === 'bmr' ? 'bombers' : 'fighters'} · ${BAND_WORD[u.band]}`;
      x.font = '700 12px "Courier Prime", monospace'; x.textAlign = 'center'; x.textBaseline = 'bottom';
      x.lineWidth = 3; x.strokeStyle = 'rgba(14,11,8,.85)'; x.strokeText(label, a, b - rad - 4);
      x.fillStyle = '#ede6d6'; x.fillText(label, a, b - rad - 4);
    }
  }

  // What has happened in the sky lately.
  for (const m of s.marks) {
    const age = s.now - m.at;
    if (age < 0) continue;
    const [a, b] = toScreen(s, m.x, m.y);
    const [hx, hy] = ahead(m.face ?? 0);
    if (m.kind === 'explode') {
      const k = Math.min(1, age / 2);
      x.fillStyle = `rgba(255,214,140,${0.95 * (1 - k)})`; x.beginPath(); x.arc(a, b, 3 + 9 * k, 0, Math.PI * 2); x.fill();
      x.fillStyle = `rgba(30,26,22,${0.8 * (1 - k)})`;
      for (let i = 0; i < 6; i++) { const ang = i * 1.05 + m.at; x.fillRect(a + Math.cos(ang) * 16 * k, b + Math.sin(ang) * 16 * k, 2, 2); }
    } else if (m.kind === 'burn') {
      // Engine fire: it flies on a little, trailing the soft smoke, and goes down.
      const k = Math.min(1, age / 7), run = 60 * k;
      for (let i = 0; i < 10; i++) {
        const f = i / 10, px = a + hx * run * f, py = b - hy * run * f + 18 * k * f * f;
        x.fillStyle = `${SMOKE}${0.5 * (1 - k) * (1 - f * 0.5)})`;
        x.beginPath(); x.arc(px, py, 2 + 4 * (1 - f), 0, Math.PI * 2); x.fill();
      }
      const px = a + hx * run, py = b - hy * run + 18 * k;
      x.fillStyle = `rgba(240,120,40,${0.9 * (1 - k)})`; x.beginPath(); x.arc(px, py, 2.4, 0, Math.PI * 2); x.fill();
    } else if (m.kind === 'spin') {
      const k = Math.min(1, age / 4);
      x.strokeStyle = `rgba(241,233,212,${0.6 * (1 - k)})`; x.lineWidth = 1;
      x.beginPath();
      for (let i = 0; i <= 24; i++) { const f = i / 24 * k, ang = f * 18, r = 3 + 10 * f; const px = a + Math.cos(ang) * r, py = b + Math.sin(ang) * r + 20 * f; if (i) x.lineTo(px, py); else x.moveTo(px, py); }
      x.stroke();
    } else if (m.kind === 'chutes') {
      // White canopies drifting down where the crew got out.
      const k = Math.min(1, age / 14);
      for (let i = 0; i < (m.n ?? 1); i++) {
        const px = a + (i - ((m.n ?? 1) - 1) / 2) * 7 + Math.sin(s.now * 0.8 + i) * 2, py = b + 26 * k + i * 2;
        x.fillStyle = `rgba(250,250,246,${0.95 * (1 - k * 0.7)})`;
        x.beginPath(); x.arc(px, py, 3, Math.PI, 0); x.fill();
        x.strokeStyle = `rgba(250,250,246,${0.6 * (1 - k * 0.7)})`; x.lineWidth = 0.6;
        x.beginPath(); x.moveTo(px - 3, py); x.lineTo(px, py + 4); x.lineTo(px + 3, py); x.stroke();
      }
    } else if (m.kind === 'bombs') {
      // The black column over what was hit, leaning with the wind, for the rest of the battle.
      const k = Math.min(1, age / 6);
      for (let i = 0; i < 6; i++) {
        x.fillStyle = `rgba(18,16,14,${0.35 * k * (1 - i / 7)})`;
        x.beginPath(); x.arc(a + i * 3, b - i * 5, 3 + i * 1.6, 0, Math.PI * 2); x.fill();
      }
      if (age < 1.2) { x.fillStyle = `rgba(255,190,110,${0.9 * (1 - age / 1.2)})`; x.beginPath(); x.arc(a, b, 3 + 6 * age, 0, Math.PI * 2); x.fill(); }
    } else if (m.kind === 'bounce') {
      const k = Math.min(1, age / 2);
      x.strokeStyle = `rgba(224,184,114,${0.8 * (1 - k)})`; x.lineWidth = 1.5;
      x.beginPath(); x.arc(a, b, 8 + 22 * k, 0, Math.PI * 2); x.stroke();
    }
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

/** The height bands as the player reads them (the UI director: LOW, MIDDLE, HIGH). */
export const BAND_WORD = ['LOW', 'MIDDLE', 'HIGH'] as const;

/** An aircraft's outline for the cards, a fighter or a bomber, in its nation's colour. */
export function planeSvg(bmr: boolean, color: string): string {
  const path = bmr
    ? 'M16 1L17.4 5L31 6.4V8.6L17.4 9L17 13.6L21.5 14.6V16H10.5V14.6L15 13.6L14.6 9L1 8.6V6.4L14.6 5Z'
    : 'M16 1.5L17.2 5.4L29 6.6V8.4L17 9L16.8 12.6L20 13.6V15H12V13.6L15.2 12.6L15 9L3 8.4V6.6L14.8 5.4Z';
  return `<svg class="bsign" width="34" height="18" viewBox="0 0 32 17" aria-hidden="true"><path d="${path}" fill="${color}" stroke="#15181b" stroke-width=".8" stroke-linejoin="round"/>${bmr ? `<g fill="${PAPER}" opacity=".7"><circle cx="9" cy="6.9" r=".9"/><circle cx="12.5" cy="6.6" r=".9"/><circle cx="19.5" cy="6.6" r=".9"/><circle cx="23" cy="6.9" r=".9"/></g>` : ''}</svg>`;
}
