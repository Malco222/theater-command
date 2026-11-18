// The land battle: a field, two armies of blocks, a clock.
//
// Each tick: the computer generals think (once a second), every block moves or holds, every block in reach of an
// enemy wears it down, nerve follows losses, runners run, and the battle ends when an army breaks, leaves, or the
// clock runs out. Deterministic: the same ticket and the same orders at the same ticks replay the same battle.

import { Rng, hashString } from '../../core/rng';
import { ONE_SHOTS, ORDER_CUES } from '../core/cues';
import { ahead, aspectOf, bearing, clamp, dist, turn } from '../core/geometry';
import { clampToDice, diceNow, fieldRatio, levelOf, RATIO } from '../core/result';
import { lineUp } from '../core/select';
import { DT, type BattleSim, type BattleView, type Command, type FieldEvent, type FieldResult, type FieldTicket, type Side } from '../core/types';
import { thinkFor } from './ai';
import { buildField, coverAt, groundAt, GROUNDS, heightAt, slopeAt, type LandField, type LandPlace } from './field';
import { LAND, MOTION, type Kind, type Motion } from './rules';
import { deploy, makeBlocks, type LandUnit } from './units';

const other = (s: Side): Side => (s === 'att' ? 'def' : 'att');
/**
 * A block's way round ground it cannot enter: how it moves, the cell it was making for, the turning points, and whether
 * the way gets there or only as near as the block can go.
 */
interface Way { m: Motion; goal: number; pts: [number, number][]; whole: boolean }
const live = (u: LandUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';
const fighting = (u: LandUnit): boolean => live(u) && u.state !== 'routing';

export class LandBattle implements BattleSim<LandUnit> {
  readonly domain = 'land' as const;
  readonly ticket: FieldTicket<LandPlace>;
  readonly field: LandField;
  t = 0;
  units: LandUnit[];
  over: FieldResult | null = null;
  ai: Record<Side, boolean>;
  private rng: Rng;
  private events: FieldEvent[] = [];
  /** Everything said this battle, for the after-action card. */
  readonly log: FieldEvent[] = [];
  private power: Record<Side, number>;
  private nextThink = 0;
  private broke: Record<Side, boolean> = { att: false, def: false };
  /** When the first blow fell between the lines (guns firing from far back do not count): null until then. */
  contactAt: number | null = null;
  /** The player has been warned that their army is near breaking (ruling 226(3)). */
  private warned: Record<Side, boolean> = { att: false, def: false };

  constructor(ticket: FieldTicket<LandPlace>) {
    this.ticket = ticket;
    this.rng = new Rng(ticket.seed);
    this.field = buildField(ticket.place, ticket.seed);
    // Each side's blocks share out exactly the power the card counted, so the odds on the field are the card's.
    const A = ticket.reckoning.A, D = ticket.reckoning.A / Math.max(1e-6, ticket.reckoning.odds);
    this.power = { att: A, def: D };
    const att = makeBlocks('att', ticket.att, A, 1);
    const def = makeBlocks('def', ticket.def, D, att.length + 1);
    this.units = [...att, ...def];
    deploy(this.units, 'att', this.field);
    deploy(this.units, 'def', this.field);
    for (const u of this.units) u.cover = groundAt(this.field, u.x, u.y);
    this.ai = { att: !ticket.att.human, def: !ticket.def.human };
    // The day's luck, from the card's own run (ruling 226(4)): the attack's blows land that much harder or softer, and
    // the defence's the other way, so the field spreads its results as the card's chance does. Drawn apart from the
    // blow-by-blow luck, so nothing else in the fight moves with it. A second attack on the same place this month
    // (ticket.attempt) draws afresh.
    const [d0, d1] = ticket.reckoning.dice;
    const roll = d0 + (d1 - d0) * new Rng(hashString(ticket.attempt ? `fortune:${ticket.seed}:${ticket.attempt}` : `fortune:${ticket.seed}`)).next();
    this.fortune = 1 + LAND.fortune * (roll - 1);
  }

  /** The day's luck: the attack's blows are multiplied by it, the defence's divided. */
  readonly fortune: number;

  /**
   * What a defender standing on its own ground takes of a blow: set per ground, and for a river or fortress works,
   * so that the field counts the ground once, as the card's odds already have (ruling 226(4)).
   */
  private get holdTake(): number {
    const p = this.ticket.place, H = LAND.hold;
    return (H.take[p.terrain] ?? 0.4) * (p.river ? H.river : 1) * (p.fort > 0 ? H.works[Math.min(3, p.fort)] : 1);
  }

  private say(e: Omit<FieldEvent, 't'>): void {
    const ev = { t: Math.round(this.t * 10) / 10, ...e } as FieldEvent;
    if (!ev.cue && ONE_SHOTS[ev.what]) ev.cue = ONE_SHOTS[ev.what];
    this.events.push(ev);
    this.log.push(ev);
  }

  unit(id: number): LandUnit | undefined { return this.units.find((u) => u.id === id); }

  /** A block's name in a dispatch: the side nobody plays carries its country, so it never reads like the player's own (ruling 226(7)). */
  named(u: LandUnit): string { return this.ticket[u.side].human ? u.name : `${this.ticket[u.side].adj} ${u.name}`; }

  /** A practice ground is no place in the campaign: its sides are the player's and theirs, not compass words. */
  private get practice(): boolean { return this.ticket.place.region < 0; }
  /** "The Polish army", or on a practice ground "Your army" or "Their army" (the UI director, after the retest). */
  private army(s: Side): string { return this.practice ? (this.ticket[s].human ? 'Your army' : 'Their army') : `The ${this.ticket[s].adj} army`; }
  /** "the Polish defence", or on a practice ground "your defence" or "the defence". */
  private sideOf(s: Side, noun: string): string { return this.practice ? `${this.ticket[s].human ? 'your' : 'the'} ${noun}` : `the ${this.ticket[s].adj} ${noun}`; }

  // ---- Orders -----------------------------------------------------------------------------------------------------

  command(c: Command, by: Side): boolean {
    if (this.over) return false;
    const mine = (ids: number[]): LandUnit[] => ids.map((i) => this.unit(i)).filter((u): u is LandUnit => !!u && u.side === by && fighting(u) && u.kind !== 'fort');
    switch (c.do) {
      case 'move': {
        const us = mine(c.units);
        if (!us.length) return false;
        // A group keeps its shape: each block goes to the point plus its place in the group.
        const cx = us.reduce((s, u) => s + u.x, 0) / us.length, cy = us.reduce((s, u) => s + u.y, 0) / us.length;
        for (const u of us) this.send(u, { kind: 'move', x: c.x + (us.length > 1 ? u.x - cx : 0), y: c.y + (us.length > 1 ? u.y - cy : 0), face: c.face });
        this.say({ what: 'contact', side: by, text: '', cue: ORDER_CUES.move });
        this.events.pop(); this.log.pop();
        return true;
      }
      case 'line': {
        const us = mine(c.units);
        if (!us.length) return false;
        for (const p of lineUp(us, by, c.x0, c.y0, c.x1, c.y1, 150)) {
          const u = this.unit(p.id)!;
          this.send(u, { kind: 'move', x: p.x, y: p.y, face: p.face });
        }
        return true;
      }
      case 'attack': {
        const t = this.unit(c.target);
        const us = mine(c.units);
        if (!t || t.side === by || !live(t) || !us.length) return false;
        // Several blocks sent at one: they come on abreast, each in its own lane, the outer ones round the target's
        // flanks, instead of all making for its middle. One block alone goes straight for it (the computer general's way).
        const cx = us.reduce((s, u) => s + u.x, 0) / us.length, cy = us.reduce((s, u) => s + u.y, 0) / us.length;
        const axis = bearing(cx, cy, t.x, t.y);
        const [rx, ry] = ahead(axis + Math.PI / 2);
        const across = [...us].sort((a, b) => (a.x - cx) * rx + (a.y - cy) * ry - ((b.x - cx) * rx + (b.y - cy) * ry));
        const step = across.reduce((s, u) => s + u.w, 0) / across.length + 30;
        // A lane that runs past the target's flank takes the enemy block nearest it in that lane instead, so a whole
        // army sent at one block falls on their line there, block against block (the computer general's own way),
        // and never crowds one block while its neighbours take the attack in the flank (ruling 226(4)).
        const foes = this.units.filter((e) => e.side !== by && fighting(e));
        across.forEach((u, i) => {
          const lane = (i - (us.length - 1) / 2) * step;
          if (us.length > 1 && Math.abs(lane) > t.w / 2 + u.w / 2) {
            const px = t.x + rx * lane, py = t.y + ry * lane;
            const e = foes.reduce((m, f) => (dist(f.x, f.y, px, py) < dist(m.x, m.y, px, py) ? f : m), t);
            // Still in its own lane, now before the block it falls on: each block its own stretch of their front (and
            // of the far bank, over a river), never every block on one enemy's middle (the art director, b439).
            if (e !== t) { this.send(u, { kind: 'attack', x: e.x, y: e.y, target: e.id, press: true, lane: lane - ((e.x - t.x) * rx + (e.y - t.y) * ry), axis }); return; }
          }
          this.send(u, { kind: 'attack', x: t.x, y: t.y, target: t.id, press: us.length > 1, ...(us.length > 1 ? { lane, axis } : {}) });
        });
        return true;
      }
      case 'hold': {
        const us = mine(c.units);
        for (const u of us) { u.order = null; u.path = []; u.keepFace = null; }
        return us.length > 0;
      }
      case 'withdraw': {
        const us = mine(c.units);
        for (const u of us) this.send(u, { kind: 'withdraw', x: u.x, y: by === 'att' ? 0 : this.field.h });
        return us.length > 0;
      }
      case 'withdrawAll': {
        if (c.side !== by) return false;
        for (const u of this.units) if (u.side === by && fighting(u)) { u.state = 'withdrawn'; u.act = 'down'; }
        this.finish(other(by), 'withdrew');
        return true;
      }
      case 'auto': {
        if (c.side !== by && !this.ai[c.side]) return false;
        this.ai[c.side] = c.on;
        return true;
      }
      case 'dice': {
        this.toDice();
        return true;
      }
      default:
        return false;
    }
  }

  private send(u: LandUnit, o: LandUnit['order'] & object): void {
    o.x = clamp(o.x, u.w / 2, this.field.w - u.w / 2);
    // A block falling back makes for its own edge of the field itself, where it leaves (b418: it stopped 10 m short).
    if (o.kind !== 'withdraw') o.y = clamp(o.y, 10, this.field.h - 10);
    // Sent to stand in the river: it stands on the nearest bank instead (a bridge it may hold).
    if (o.kind === 'move' && this.wet(u, o.x, o.y, false) && groundAt(this.field, o.x, o.y) !== 'bridge') {
      search: for (let r = 8; r <= 240; r += 8) for (let a = 0; a < 16; a++) {
        const x = clamp(o.x + Math.sin((a * Math.PI) / 8) * r, u.w / 2, this.field.w - u.w / 2), y = clamp(o.y + Math.cos((a * Math.PI) / 8) * r, 10, this.field.h - 10);
        if (!this.wet(u, x, y, false)) { o.x = x; o.y = y; break search; }
      }
    }
    u.order = o;
    u.keepFace = o.face ?? null;
    // An attack's way is the one it will walk: to its place in the lane before its target, as the march works it out.
    const t = o.kind === 'attack' && o.target !== undefined ? this.unit(o.target) : undefined;
    const at = t ? this.lanePoint(u, t, this.closeTo(u)) : [o.x, o.y];
    u.path = at ? this.route(u, at[0], at[1]) : [];
  }

  /** How near a block closes on what it attacks before it stands and fights: well inside its reach. */
  private closeTo(u: LandUnit): number {
    return LAND.reach[u.kind] * (u.kind === 'art' ? 0.85 : 0.6);
  }

  /**
   * The way there: straight, unless ground it cannot enter lies across it (a river away from its bridges, rock, marsh
   * for a lorry, a bunker). Then round it, the quickest way over the ground (a road is quicker than a field), cut to as
   * few turns as can be walked straight between: over a bridge, round a bend of the river, round an outcrop. Where it
   * cannot get there at all it goes as near as it can, and stands. A block set down in ground it cannot enter (a tank
   * among rock) first walks out of it the shortest way. An attack's way is asked for afresh every tick, so the way found
   * is kept while the block makes for much the same place, and the block takes the furthest turning point it can walk
   * straight to (b418: tanks and lorries never crossed rivers, and stood for good at rock and marsh).
   */
  private ways = new Map<number, Way>();
  /** By how it moves: the cells within a few of ground it can hardly cross or not at all (a river, rock, marsh). */
  private hardNear: Partial<Record<Motion, Uint8Array>> = {};
  private route(u: LandUnit, x: number, y: number): [number, number][] {
    const m = MOTION[u.kind], f = this.field;
    if (m === 'fixed') return [[x, y]];
    const out = this.exit(m, u.x, u.y);
    if (out) return [...out, [x, y]];
    if (this.dry(m, u.x, u.y, x, y)) { this.ways.delete(u.id); return [[x, y]]; }
    const goal = this.cellAt(x, y);
    const near = (a: number, b: number): boolean => Math.abs((a % f.cols) - (b % f.cols)) <= 2 && Math.abs(Math.floor(a / f.cols) - Math.floor(b / f.cols)) <= 2;
    let w = this.ways.get(u.id), fresh = false;
    for (;;) {
      if (!w || w.m !== m || !near(w.goal, goal)) {
        w = { m, goal, ...this.findWay(m, u.x, u.y, x, y) };
        this.ways.set(u.id, w);
        fresh = true;
      }
      const pts = w.pts, end = pts[pts.length - 1];
      let k = pts.length - 1;
      while (k >= 0 && !this.dry(m, u.x, u.y, pts[k][0], pts[k][1])) k--;
      // Pushed off its way, or at the end of a way kept from a little earlier with the place now out of sight: look again.
      const lost = k < 0 || (w.whole && k === pts.length - 1 && dist(u.x, u.y, end[0], end[1]) < 10 && !this.dry(m, end[0], end[1], x, y));
      if (lost && !fresh) { w = undefined; continue; }
      // Nowhere nearer to go: it stays where it is.
      if (k < 0) return [[u.x, u.y]];
      const way = pts.slice(k);
      if (w.whole && this.dry(m, end[0], end[1], x, y) && (end[0] !== x || end[1] !== y)) way.push([x, y]);
      return way;
    }
  }

  private cellAt(x: number, y: number): number {
    const f = this.field;
    return clamp(Math.floor(y / f.cell), 0, f.rows - 1) * f.cols + clamp(Math.floor(x / f.cell), 0, f.cols - 1);
  }

  /**
   * The way a block standing in ground it cannot enter walks out: cell by cell, side to side, over that same kind of
   * ground (never into water it cannot cross unless it stands in water already) to the nearest cell it can enter.
   * Nothing where it stands on ground it can enter.
   */
  private exit(m: Motion, x: number, y: number): [number, number][] | null {
    const f = this.field, C = f.cols, pace = LAND.pace[m], s = this.cellAt(x, y);
    const go = (i: number): boolean => pace[GROUNDS[f.ground[i]]] > 0;
    if (go(s)) return null;
    const wet = (i: number): boolean => GROUNDS[f.ground[i]] === 'water' || GROUNDS[f.ground[i]] === 'blown';
    const inWet = wet(s), back = new Map<number, number>([[s, -1]]);
    const mid = (i: number): [number, number] => [(i % C) * f.cell + f.cell / 2, Math.floor(i / C) * f.cell + f.cell / 2];
    let ring = [s];
    while (ring.length) {
      const next: number[] = [];
      let best = -1, bd = Infinity;
      for (const i of ring) for (const [dc, dr] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
        const c = (i % C) + dc, r = Math.floor(i / C) + dr, j = r * C + c;
        if (c < 0 || c >= C || r < 0 || r >= f.rows || back.has(j)) continue;
        back.set(j, i);
        if (go(j)) {
          const d = dist(x, y, ...mid(j));
          if (d < bd || (d === bd && j < best)) { bd = d; best = j; }
        } else if (inWet || !wet(j)) next.push(j);
      }
      if (best >= 0) {
        const way: [number, number][] = [];
        for (let i = best; i !== s; i = back.get(i)!) way.unshift(mid(i));
        return way;
      }
      ring = next;
    }
    return null;
  }

  /**
   * The quickest way over the field's cells from one point to another for a block that moves this way, side to side or
   * corner to corner (never across the corner of ground it cannot enter), then cut to the fewest turning points it can
   * walk straight between. Where the place cannot be reached, the way ends at the nearest cell that can, and says so.
   */
  private findWay(m: Motion, sx: number, sy: number, x: number, y: number): { pts: [number, number][]; whole: boolean } {
    const f = this.field, C = f.cols, n = f.ground.length, pace = LAND.pace[m];
    const top = Math.max(...Object.values(pace)), s = this.cellAt(sx, sy), g = this.cellAt(x, y), gc = g % C, gr = Math.floor(g / C);
    const go = (i: number): number => pace[GROUNDS[f.ground[i]]];
    const h = (i: number): number => {
      const dc = Math.abs((i % C) - gc), dr = Math.abs(Math.floor(i / C) - gr);
      return (Math.max(dc, dr) + (Math.SQRT2 - 1) * Math.min(dc, dr)) / top;
    };
    const cost = new Float64Array(n).fill(Infinity), back = new Int32Array(n).fill(-1), done = new Uint8Array(n);
    // A small heap of cells by cost so far plus the least cost left, each entry keeping the key it went in with; ties
    // go to the lower cell, so the way is always the same.
    const hk: number[] = [], hv: number[] = [];
    const less = (a: number, b: number): boolean => hk[a] < hk[b] || (hk[a] === hk[b] && hv[a] < hv[b]);
    const swap = (a: number, b: number): void => { [hk[a], hk[b]] = [hk[b], hk[a]]; [hv[a], hv[b]] = [hv[b], hv[a]]; };
    const push = (i: number, key: number): void => {
      hk.push(key); hv.push(i);
      for (let k = hk.length - 1; k > 0;) { const p = (k - 1) >> 1; if (!less(k, p)) break; swap(k, p); k = p; }
    };
    const pop = (): number => {
      const top0 = hv[0], lk = hk.pop()!, lv = hv.pop()!;
      if (hk.length) {
        hk[0] = lk; hv[0] = lv;
        for (let k = 0; ;) {
          const l = 2 * k + 1, r = l + 1;
          let b = k;
          if (l < hk.length && less(l, b)) b = l;
          if (r < hk.length && less(r, b)) b = r;
          if (b === k) break;
          swap(k, b); k = b;
        }
      }
      return top0;
    };
    cost[s] = 0; push(s, h(s));
    let best = s, bh = h(s);
    while (hk.length) {
      const i = pop();
      if (done[i]) continue;
      done[i] = 1;
      if (h(i) < bh) { bh = h(i); best = i; }
      if (i === g) break;
      const c = i % C, r = Math.floor(i / C);
      for (let dr = -1; dr <= 1; dr++) for (let dc = -1; dc <= 1; dc++) {
        if ((!dr && !dc) || c + dc < 0 || c + dc >= C || r + dr < 0 || r + dr >= f.rows) continue;
        const j = i + dr * C + dc;
        if (done[j] || go(j) <= 0) continue;
        if (dr && dc && (go(i + dc) <= 0 || go(i + dr * C) <= 0)) continue;
        const c2 = cost[i] + (dr && dc ? Math.SQRT2 : 1) / Math.max(0.12, go(j));
        if (c2 < cost[j]) { cost[j] = c2; back[j] = i; push(j, c2 + h(j)); }
      }
    }
    const cs = f.cell, mid = (i: number): [number, number] => [(i % C) * cs + cs / 2, Math.floor(i / C) * cs + cs / 2];
    const raw: [number, number][] = [];
    for (let i = best; i !== -1 && i !== s; i = back[i]) raw.unshift(mid(i));
    if (best === g && raw.length) raw[raw.length - 1] = [x, y];
    // Already in its own cell: the place itself, where that is the cell; else nowhere nearer to go.
    if (!raw.length) return best === g ? { pts: [[x, y]], whole: true } : { pts: [[sx, sy]], whole: false };
    // Cut the corners: from each turning point, straight on to the last cell it can still walk to.
    const pts: [number, number][] = [];
    let ax = sx, ay = sy;
    for (let j = 1; j < raw.length; j++) {
      if (!this.dry(m, ax, ay, raw[j][0], raw[j][1])) { [ax, ay] = raw[j - 1]; pts.push(raw[j - 1]); }
    }
    pts.push(raw[raw.length - 1]);
    return { pts, whole: best === g };
  }

  /**
   * Whether a block that moves this way can go straight from one point to the other over nothing it cannot enter:
   * every cell the line passes through after the one it starts in, not samples along it, so the line never clips the
   * corner of a water or rock cell. A line through the very corner of two cells goes from one to the other across it.
   */
  private dry(m: Motion, x0: number, y0: number, x1: number, y1: number): boolean {
    const f = this.field, cs = f.cell;
    const cOf = (v: number): number => clamp(Math.floor(v / cs), 0, f.cols - 1), rOf = (v: number): number => clamp(Math.floor(v / cs), 0, f.rows - 1);
    let c = cOf(x0), r = rOf(y0);
    const c1 = cOf(x1), r1 = rOf(y1), dx = x1 - x0, dy = y1 - y0, sx = Math.sign(dx), sy = Math.sign(dy);
    const stepX = dx ? cs / Math.abs(dx) : Infinity, stepY = dy ? cs / Math.abs(dy) : Infinity;
    let tx = dx ? (sx > 0 ? (c + 1) * cs - x0 : x0 - c * cs) / Math.abs(dx) : Infinity;
    let ty = dy ? (sy > 0 ? (r + 1) * cs - y0 : y0 - r * cs) / Math.abs(dy) : Infinity;
    const pace = LAND.pace[m];
    for (let n = Math.abs(c1 - c) + Math.abs(r1 - r); n > 0 && (c !== c1 || r !== r1); n--) {
      if (Math.abs(tx - ty) < 1e-9) { tx += stepX; ty += stepY; c += sx; r += sy; n--; }
      else if (tx < ty) { tx += stepX; c += sx; }
      else { ty += stepY; r += sy; }
      if (c < 0 || r < 0 || c >= f.cols || r >= f.rows) return true;
      if (pace[GROUNDS[f.ground[r * f.cols + c]]] <= 0) return false;
    }
    return true;
  }

  // ---- The tick -----------------------------------------------------------------------------------------------------

  step(): void {
    if (this.over) return;
    if (this.t >= this.nextThink) {
      this.nextThink = this.t + LAND.think;
      for (const s of ['att', 'def'] as Side[]) if (this.ai[s]) thinkFor(this, s);
    }
    for (const u of this.units) { u.pinned = false; if (live(u)) { u.still += DT; u.clear += DT; } }
    // Blows first, from where everyone stands, then nerve, then movement: no block gets to strike first by its place in the list.
    const hurt = new Map<number, { dmg: number; nerve: number }>();
    for (const u of this.units) {
      if (!fighting(u)) continue;
      const t = this.targetOf(u);
      u.engaged = t?.id;
      if (!t) continue;
      if (this.contactAt === null && u.kind !== 'art') this.contactAt = this.t;
      if (!u.met) { u.met = true; if (u.side === 'att' || !t.met) this.say({ what: 'contact', unit: u.id, side: u.side, text: `${this.named(u)} is in action.`, x: u.x, y: u.y }); }
      const blow = this.blow(u, t);
      const h = hurt.get(t.id) ?? { dmg: 0, nerve: 0 };
      h.dmg += blow.dmg; h.nerve += blow.nerve;
      hurt.set(t.id, h);
      if (blow.front) t.pinned = true;
      if (blow.aspect !== 'front' && this.t - t.flankedAt > 20) {
        t.flankedAt = this.t;
        if (t.side === 'att' ? this.ticket.att.human : this.ticket.def.human) this.say({ what: 'flanked', unit: t.id, side: t.side, text: `${this.named(t)} is hit from the ${blow.aspect === 'rear' ? 'rear' : 'flank'}.`, x: t.x, y: t.y });
      }
    }
    for (const [id, h] of hurt) this.suffer(this.unit(id)!, h.dmg, h.nerve);
    for (const u of this.units) if (live(u)) this.nerve(u);
    for (const u of this.units) if (live(u)) this.move(u);
    this.spread();
    this.t = Math.round((this.t + DT) * 1000) / 1000;
    this.judge();
  }

  /**
   * Nobody stands in anybody else: where two blocks' ground overlaps they are eased apart along the shallower way, a
   * few metres a second, onto ground they can stand on. Blocks side by side in a line, or one behind the other, are
   * left alone; enemies keep a few metres between their fronts. For the screens and the models as much as for the
   * fight: two divisions never share one patch of ground.
   */
  private spread(): void {
    const us = this.units.filter((u) => fighting(u) && u.kind !== 'fort');
    // Never eased onto ground it can hardly cross (into the river off a bridge, tanks into a marsh), unless it stands on such already.
    const can = (u: LandUnit, ux: number, uy: number, push: number): boolean => {
      const x = clamp(u.x + ux * push, 5, this.field.w - 5), y = clamp(u.y + uy * push, 5, this.field.h - 5), pace = LAND.pace[MOTION[u.kind]];
      const to = pace[groundAt(this.field, x, y)];
      return to > LAND.queue.hard || (to > 0 && to >= pace[groundAt(this.field, u.x, u.y)]);
    };
    const go = (u: LandUnit, ux: number, uy: number, push: number): void => {
      if (can(u, ux, uy, push)) { u.x = clamp(u.x + ux * push, 5, this.field.w - 5); u.y = clamp(u.y + uy * push, 5, this.field.h - 5); }
    };
    for (let i = 0; i < us.length; i++) for (let j = i + 1; j < us.length; j++) {
      const a = us[i], b = us[j];
      const dx = b.x - a.x, dy = b.y - a.y;
      if (Math.abs(dx) > 300 || Math.abs(dy) > 300) continue;
      // In a's own frame: across its front, and along its depth. b's box as it lies in that frame.
      const [fx, fy] = ahead(a.face), [rx, ry] = ahead(a.face + Math.PI / 2);
      const lat = dx * rx + dy * ry, dep = dx * fx + dy * fy;
      const c = Math.abs(Math.cos(b.face - a.face)), s = Math.abs(Math.sin(b.face - a.face));
      const pl = a.w / 2 + (b.w / 2) * c + (b.d / 2) * s - Math.abs(lat);
      const pd = a.d / 2 + (b.w / 2) * s + (b.d / 2) * c + (a.side === b.side ? 0 : LAND.apart) - Math.abs(dep);
      if (pl <= 0 || pd <= 0) continue;
      // A queue, not a pile (the art director, b418, b439): a block marching into a friend's ground has it make way,
      // away from it by whichever way the ground lets it, so the one goes on and the other gives ground and comes on
      // behind, each on its own ground; at a bridge or a pass as everywhere. Two marching into each other: the one with
      // less to go goes on. A friend that cannot give way at all, the marching block steps round.
      if (a.side === b.side && (this.narrows(a, a.x, a.y) || this.narrows(b, b.x, b.y))) {
        const into = (u: LandUnit, ox: number, oy: number): boolean => u.pressing !== undefined && ox * Math.sin(u.pressing) + oy * Math.cos(u.pressing) > 0;
        const pa = into(a, dx, dy), pb = into(b, -dx, -dy);
        if (pa || pb) {
          const goA = pa && (!pb || this.toGo(a) < this.toGo(b) || (this.toGo(a) === this.toGo(b) && a.id < b.id));
          const mover = goA ? a : b, y = goA ? b : a, sy = goA ? 1 : -1;
          const p = Math.min(Math.min(pl, pd), (mover.speed + LAND.spread) * DT);
          const sl = Math.sign(lat) || 1, sd = Math.sign(dep) || 1, d1 = Math.hypot(dx, dy) || 1;
          const ways: [number, number][] = pl < pd ? [[rx * sl, ry * sl], [fx * sd, fy * sd]] : [[fx * sd, fy * sd], [rx * sl, ry * sl]];
          ways.push([dx / d1, dy / d1]);
          const w = ways.find(([wx, wy]) => can(y, wx * sy, wy * sy, p));
          if (w) { go(y, w[0] * sy, w[1] * sy, p); y.yieldTo = mover.id; continue; }
          if (can(mover, -rx * sl * sy, -ry * sl * sy, p)) { go(mover, -rx * sl * sy, -ry * sl * sy, p); continue; }
        }
      }
      // In the open, a block on the march slides a friend in its way aside as fast as it comes on, and slides aside
      // itself, so the two pass side by side and no box is drawn on another (the art director, b439).
      if (a.side === b.side && (a.pressing !== undefined || b.pressing !== undefined) && !this.narrows(a, a.x, a.y) && !this.narrows(b, b.x, b.y)) {
        const sl = Math.sign(lat) || (a.id < b.id ? 1 : -1), p = Math.min(pl / 2, (Math.max(a.speed, b.speed) + LAND.spread) * DT);
        if (can(a, -rx * sl, -ry * sl, p) && can(b, rx * sl, ry * sl, p)) {
          go(a, -rx * sl, -ry * sl, p); go(b, rx * sl, ry * sl, p);
          // Two on the march into each other: the one with further to go waits a moment for the other to pass.
          if (a.pressing !== undefined && b.pressing !== undefined) { const wa = this.toGo(a) > this.toGo(b) || (this.toGo(a) === this.toGo(b) && a.id > b.id); (wa ? a : b).yieldTo = (wa ? b : a).id; }
          continue;
        }
      }
      // Side by side where neither can step aside (on a bridge, in a gap): one behind the other instead.
      let along = pl < pd;
      if (along) { const sl = Math.sign(lat) || (a.id < b.id ? 1 : -1), p = Math.min(pl / 2, LAND.spread * DT); if (!can(a, -rx * sl, -ry * sl, p) && !can(b, rx * sl, ry * sl, p)) along = false; }
      const sg = Math.sign(along ? lat : dep) || (a.id < b.id ? 1 : -1);
      const ux = (along ? rx : fx) * sg, uy = (along ? ry : fy) * sg;
      const push = Math.min((along ? pl : pd) / 2, LAND.spread * DT);
      go(a, -ux, -uy, push); go(b, ux, uy, push);
    }
  }

  /** Who this block strikes: its ordered target if in reach, else the enemy in reach most in front of it. */
  targetOf(u: LandUnit): LandUnit | null {
    const marching = u.order && u.order.kind !== 'attack' && u.path.length > 0;
    if (u.kind === 'art' && marching) return null;
    if (u.order?.kind === 'withdraw') return null;
    const reach = LAND.reach[u.kind];
    let best: LandUnit | null = null, score = Infinity;
    for (const e of this.units) {
      if (e.side === u.side || !live(e)) continue;
      const d = this.gap(u, e);
      if (d > reach) continue;
      if (u.order?.kind === 'attack' && u.order.target === e.id) return e;
      const off = Math.abs(turn(u.face, bearing(u.x, u.y, e.x, e.y)));
      const s = d * (1 + off) * (e.state === 'routing' ? 2 : 1);
      if (s < score) { score = s; best = e; }
    }
    return best;
  }

  /** Metres between two blocks' edges, roughly. */
  gap(a: LandUnit, b: LandUnit): number {
    return Math.max(0, dist(a.x, a.y, b.x, b.y) - (Math.max(a.w, a.d) + Math.max(b.w, b.d)) / 4);
  }

  private blow(u: LandUnit, t: LandUnit): { dmg: number; nerve: number; front: boolean; aspect: 'front' | 'flank' | 'rear' } {
    const gu = coverAt(this.field, u.x, u.y), gt = coverAt(this.field, t.x, t.y);
    let p = u.power0 * (0.35 + 0.65 * (u.str / u.str0)) * LAND.rate;
    if (u.state === 'wavering') p *= LAND.wavering;
    if (u.order && u.path.length && u.speed > 0.5) p *= LAND.moving;
    let nerveMul = 1;
    if (u.kind === 'art') {
      const close = this.gap(u, t) < LAND.guns.closeAt;
      p *= close ? LAND.guns.close : LAND.guns.punch;
      if (!close) nerveMul *= LAND.guns.nerve;
    }
    if (u.kind === 'arm' && LAND.tanksClose.ground.includes(gu)) p *= LAND.tanksClose.punch;
    const rise = heightAt(this.field, u.x, u.y) - heightAt(this.field, t.x, t.y);
    if (rise > LAND.height.over) p *= LAND.height.up; else if (rise < -LAND.height.over) p *= LAND.height.down;
    const aspect = aspectOf(t.face, t.x, t.y, u.x, u.y);
    const [am, nm] = LAND.aspect[aspect];
    let take = LAND.cover[gt];
    if (t.kind === 'arm' && LAND.tanksClose.ground.includes(gt)) take *= LAND.tanksClose.take;
    if (t.side === 'def' && dist(t.x, t.y, t.home[0], t.home[1]) < LAND.hold.within) take *= this.holdTake;
    const luck = 1 + LAND.jitter * (this.rng.next() * 2 - 1);
    const dmg = p * am * take * luck * (u.side === 'att' ? this.fortune : 1 / this.fortune) * DT;
    return { dmg, nerve: nm * nerveMul, front: aspect === 'front', aspect };
  }

  private suffer(t: LandUnit, dmg: number, nerveMul: number): void {
    // Damage is in power: as a share of what the block came with, it comes off its strength meter.
    const share = dmg / Math.max(1e-9, t.power0);
    const before = t.str;
    t.str = Math.max(0, t.str - share * t.str0);
    const lost = (before - t.str) / t.str0;
    const steady = 1 - LAND.nerve.xp * t.xp - LAND.nerve.general * this.generalOf(t.side);
    t.mor = Math.max(0, t.mor - lost * LAND.nerve.perLoss[t.side] * nerveMul * Math.max(0.3, steady));
    t.still = 0;
    t.figures = t.str <= 0 ? 0 : Math.max(1, Math.ceil(t.tray * t.str));
  }

  private generalOf(s: Side): number { return this.ticket[s].general?.level ?? 0; }

  private nerve(u: LandUnit): void {
    const N = LAND.nerve;
    const enemyNear = this.units.some((e) => e.side !== u.side && fighting(e) && dist(e.x, e.y, u.x, u.y) < N.clear);
    if (enemyNear) u.clear = 0;
    if (u.state === 'routing') {
      if (u.clear > N.quiet) u.mor = Math.min(u.mor0, u.mor + N.runnerRecover * DT);
      if (!this.broke[u.side] && u.clear > N.quiet && u.mor >= N.rally && u.str > N.shattered) {
        u.state = 'wavering'; u.order = null; u.path = []; u.act = 'idle';
        this.say({ what: 'rallied', unit: u.id, side: u.side, text: `${this.named(u)} has rallied.`, x: u.x, y: u.y });
      }
      return;
    }
    if (u.still > N.still) u.mor = Math.min(u.mor0, u.mor + N.recover * DT);
    if (u.str <= N.shattered * u.str0 || u.mor < N.rout) { this.rout(u); return; }
    const was = u.state;
    u.state = u.mor < N.waver ? 'wavering' : 'steady';
    if (was === 'steady' && u.state === 'wavering') this.say({ what: 'wavering', unit: u.id, side: u.side, text: `${this.named(u)} is wavering.`, x: u.x, y: u.y });
  }

  private rout(u: LandUnit): void {
    if (u.state === 'routing') return;
    u.state = 'routing'; u.order = null; u.path = []; u.keepFace = null; u.engaged = undefined;
    this.say({ what: 'routing', unit: u.id, side: u.side, text: `${this.named(u)} breaks and runs.`, x: u.x, y: u.y });
    // Friends nearby see it go.
    for (const f of this.units) if (f !== u && f.side === u.side && fighting(f) && dist(f.x, f.y, u.x, u.y) < LAND.nerve.near) f.mor = Math.max(0, f.mor - LAND.nerve.friendRuns);
  }

  private move(u: LandUnit): void {
    const k = u.kind;
    u.pressing = undefined;
    const gave = u.yieldTo !== undefined ? this.unit(u.yieldTo) : undefined;
    u.yieldTo = undefined;
    let to: [number, number] | null = null, run = false;
    if (u.state === 'routing') { to = [u.x, u.side === 'att' ? -50 : this.field.h + 50]; run = true; }
    else if (u.order?.kind === 'withdraw') {
      // Straight back to its own edge, or back round by its bridge where a river or rock lies behind it.
      u.path = this.dry(MOTION[k], u.x, u.y, u.x, u.order.y) && !this.exit(MOTION[k], u.x, u.y) ? [[u.x, u.order.y]] : this.route(u, u.order.x, u.order.y);
      to = u.path[0];
    }
    else if (u.order?.kind === 'attack') {
      let t = this.unit(u.order.target!);
      // An attack pressed by several blocks carries on: when its block breaks or leaves, it takes the nearest enemy
      // still fighting within reach of the march, as the computer general would (ruling 226(4)).
      if (u.order.press && t && !fighting(t) && !this.ai[u.side]) {
        const next = this.units.filter((e) => e.side !== u.side && fighting(e) && dist(e.x, e.y, u.x, u.y) < LAND.press)
          .reduce<LandUnit | null>((m, e) => (!m || dist(e.x, e.y, u.x, u.y) < dist(m.x, m.y, u.x, u.y) ? e : m), null);
        if (next) {
          // Where it stands across the attack's front, it stays: the next block is taken in front of it, not in its middle.
          const ax = u.order.axis, lane = ax === undefined ? undefined : (u.x - next.x) * Math.sin(ax + Math.PI / 2) + (u.y - next.y) * Math.cos(ax + Math.PI / 2);
          this.send(u, { kind: 'attack', x: next.x, y: next.y, target: next.id, press: true, ...(ax === undefined ? {} : { lane, axis: ax }) }); t = next;
        }
      }
      if (!t || !live(t)) { u.order = null; u.path = []; }
      else {
        // Close until it is well in reach, then stand and fight.
        // Never stands in the water to fight, or on the bridge: it presses on to its own ground on the far bank.
        const want = this.closeTo(u);
        const at = this.gap(u, t) > want || this.wet(u, u.x, u.y) ? this.lanePoint(u, t, want) : null;
        if (at) { u.path = this.route(u, at[0], at[1]); to = u.path[0]; }
        else { u.path = []; to = null; }
      }
    } else if (u.order && u.path.length) {
      // A block on the march stops when an enemy is on top of it, unless it is running or pulling out. One that can no
      // longer walk straight to its next turning point (it came at it a little to one side) looks again for its way.
      to = u.path[0];
      if (u.order.kind === 'move' && !this.dry(MOTION[k], u.x, u.y, to[0], to[1])) { u.path = this.route(u, u.order.x, u.order.y); to = u.path[0]; }
    }
    // Already at a turning point of its way: on to the next, from the point itself. At the end of its way (as near as
    // it can get): nothing to walk, so it stands, and faces as it stands.
    if (to && dist(u.x, u.y, to[0], to[1]) < 0.5 && !run) {
      if (u.path.length > 1 && u.path[0] === to) { u.x = to[0]; u.y = to[1]; u.path.shift(); to = u.path[0]; }
      else if (u.order?.kind === 'move') { u.path.shift(); to = u.path[0] ?? null; }
      else to = null;
    }
    if (u.engaged !== undefined && u.order?.kind === 'move' && u.kind !== 'art') {
      const e = this.unit(u.engaged);
      if (e && this.gap(u, e) < 60) to = null;
    }
    if (!to) {
      u.speed = 0;
      // Face what it is fighting, unless held by an enemy in front or by the facing its line was given.
      const e = u.engaged !== undefined ? this.unit(u.engaged) : undefined;
      let want = u.keepFace;
      if (e && !u.pinned) {
        const off = Math.abs(turn(u.keepFace ?? u.face, bearing(u.x, u.y, e.x, e.y)));
        if (u.keepFace === null || off > Math.PI / 3) want = bearing(u.x, u.y, e.x, e.y);
      }
      if (u.order?.kind === 'move' && !u.path.length) { u.order = null; }
      if (want !== null && Math.abs(turn(u.face, want)) > 0.02) { this.turnTo(u, want); u.act = 'turn'; }
      else u.act = e ? 'fight' : 'idle';
      if (u.state === 'routing') u.act = 'rout';
      return;
    }
    const head = bearing(u.x, u.y, to[0], to[1]);
    // It made way for a friend with the road last tick: it waits, rather than step straight back into that friend's way
    // (never in the river: there it presses on to the bank). At a bridge's end it waits too: that is where the queue
    // forms, and two blocks that shoved each other there without waiting stood locked till the battle ended (b453).
    if (gave && !run && !this.wet(u, u.x, u.y, false) && Math.sin(head) * (gave.x - u.x) + Math.cos(head) * (gave.y - u.y) > 0) { u.speed = 0; u.act = 'idle'; return; }
    const off = turn(u.face, head);
    // Blocks wheel before they march; a runner just runs.
    if (!run && Math.abs(off) > 0.6) { this.turnTo(u, head); u.speed = 0; u.act = 'turn'; return; }
    if (run) u.face = head; else this.turnTo(u, head);
    const m = MOTION[k];
    const g = groundAt(this.field, u.x, u.y);
    let pace = LAND.pace[m][g];
    if (k === 'mtn' && (g === 'rough' || g === 'rock')) pace = Math.min(1, pace * LAND.mountainFoot);
    const up = heightAt(this.field, u.x + Math.sin(head) * 20, u.y + Math.cos(head) * 20) - heightAt(this.field, u.x, u.y);
    pace *= up > 0 ? Math.max(0.4, 1 - (up / 20) * 1.5) : 1;
    let v = LAND.speed[k] * Math.max(0.12, pace) * (run ? 1.15 : 1) * (u.state === 'wavering' ? 0.85 : 1);
    const d = dist(u.x, u.y, to[0], to[1]);
    const stepLen = Math.min(d, v * DT);
    const [ax, ay] = ahead(head);
    let nx = u.x + ax * stepLen, ny = u.y + ay * stepLen;
    // Ground it cannot enter: try a little to either side, else stop. Not while it is still walking out of such ground.
    if (LAND.pace[m][groundAt(this.field, nx, ny)] <= 0 && LAND.pace[m][g] > 0 && !run) {
      let moved = false;
      for (const sw of [0.5, -0.5, 1, -1]) {
        const [bx, by] = ahead(head + sw);
        const tx = u.x + bx * stepLen, ty = u.y + by * stepLen;
        if (LAND.pace[m][groundAt(this.field, tx, ty)] > 0) { nx = tx; ny = ty; moved = true; break; }
      }
      if (!moved) { u.speed = 0; u.act = 'idle'; return; }
    }
    u.x = clamp(nx, 5, this.field.w - 5);
    u.y = run ? ny : clamp(ny, 5, this.field.h - 5);
    u.speed = stepLen / DT;
    if (!run) u.pressing = head;
    u.act = run ? 'rout' : v > LAND.speed[k] * 1.05 ? 'run' : 'march';
    u.cover = groundAt(this.field, u.x, u.y);
    if (d <= Math.max(6, stepLen + 0.01)) {
      u.path.shift();
      if (!u.path.length && u.order?.kind === 'move') { u.order = null; u.speed = 0; }
    }
    // Off the field.
    if (u.y < 0 || u.y > this.field.h) {
      if (u.state === 'routing') {
        u.state = 'gone'; u.act = 'down'; u.figures = 0;
        this.say({ what: 'gone', unit: u.id, side: u.side, text: `${this.named(u)} has fled the field.` });
      } else if (u.order?.kind === 'withdraw') {
        u.state = 'withdrawn'; u.act = 'down';
        this.say({ what: 'withdrawn', unit: u.id, side: u.side, text: `${this.named(u)} has fallen back off the field.` });
      }
    } else if (u.order?.kind === 'withdraw' && (u.side === 'att' ? u.y <= 6 : u.y >= this.field.h - 6)) {
      u.state = 'withdrawn'; u.act = 'down';
      this.say({ what: 'withdrawn', unit: u.id, side: u.side, text: `${this.named(u)} has fallen back off the field.` });
    }
  }

  /** Within LAND.queue.near metres of ground this block can hardly cross or not at all: at a bridge, a pass, a gap. */
  private narrows(u: LandUnit, x: number, y: number): boolean {
    const m = MOTION[u.kind], f = this.field, C = f.cols, R = f.rows;
    let near = this.hardNear[m];
    if (!near) {
      near = this.hardNear[m] = new Uint8Array(C * R);
      const n = Math.round(LAND.queue.near / f.cell), pace = LAND.pace[m];
      for (let i = 0; i < C * R; i++) {
        if (pace[GROUNDS[f.ground[i]]] > LAND.queue.hard) continue;
        const c0 = i % C, r0 = Math.floor(i / C);
        for (let r = Math.max(0, r0 - n); r <= Math.min(R - 1, r0 + n); r++) for (let c = Math.max(0, c0 - n); c <= Math.min(C - 1, c0 + n); c++) near[r * C + c] = 1;
      }
    }
    return near[this.cellAt(x, y)] === 1;
  }

  /** How far a block has still to go on its way. */
  private toGo(u: LandUnit): number {
    let d = 0, x = u.x, y = u.y;
    for (const [px, py] of u.path) { d += dist(x, y, px, py); x = px; y = py; }
    return d;
  }

  /**
   * Where a block in a lane makes for: in front of its target as the attack came on, out to the side by its lane, and
   * for a lane wider than the target's front, forward round its flank. With no lane, or once it stands at that point and
   * is still out of reach, straight at the target. Null once it stands at that point within reach: it stands and fights.
   */
  private lanePoint(u: LandUnit, t: LandUnit, want: number): [number, number] | null {
    const o = u.order!;
    const lone = o.lane === undefined || o.axis === undefined;
    // A block with no lane makes straight for its target, to stand before it on the way it comes.
    const axis = lone ? bearing(u.x, u.y, t.x, t.y) : o.axis!, lane = lone ? 0 : o.lane!;
    const key = lone ? `${t.id}` : `${t.id}/${lane}/${axis}`;
    // How far off its target a block may stand, at its own stretch, and still fight: half way from where it closes to
    // the end of its reach. A stand pushed further out by its friends, it never fires from (the review, b453).
    const hold = want + (LAND.reach[u.kind] - want) / 2;
    const off = (x: number, y: number): number => dist(x, y, t.x, t.y) - (Math.max(u.w, u.d) + Math.max(t.w, t.d)) / 4;
    const there = (s: [number, number]): [number, number] | null => {
      if (dist(u.x, u.y, s[0], s[1]) >= 12) return s;
      // At its stand: in reach, it stands; still out of reach (its target has stepped back), it closes on the target from
      // here on, and does not turn back for the stand a step later.
      if (this.gap(u, t) <= hold && !this.wet(u, u.x, u.y)) return null;
      u.closing = true;
      return [t.x, t.y];
    };
    // The stand it worked out holds while it is still its own, dry and in reach: a step to one side, or a friend first in
    // the list, does not send it off to another stretch (it flipped between two, and wheeled in place: the review, b453).
    const old = u.stand && u.standFor === key ? u.stand : null;
    if (old && u.closing) return [t.x, t.y];
    const kept = !!old && off(old[0], old[1]) <= hold && !this.wet(u, old[0], old[1]);
    if (old && kept && !this.claimed(u, old[0], old[1], u.standFace ?? bearing(old[0], old[1], t.x, t.y))) return there(old);
    const [fx, fy] = ahead(axis), [rx, ry] = ahead(axis + Math.PI / 2);
    const lim = t.w / 2 + u.w / 2;
    const side0 = Math.min(Math.abs(lane), lim) * Math.sign(lane);
    const back = Math.max(0, want * 0.8 + t.d / 2 + u.d / 2 - Math.max(0, Math.abs(lane) - lim));
    let p: [number, number] = [t.x + rx * side0 - fx * back, t.y + ry * side0 - fy * back];
    // Its own ground to stand on, never the river or the bridge: on into the far bank, short of the block it attacks;
    // else back to the nearer bank.
    if (this.wet(u, p[0], p[1])) {
      const room = Math.max(0, back - (t.d / 2 + u.d / 2 + LAND.apart));
      let dry: [number, number] | null = null;
      for (let k = 8; k <= Math.max(room, 8) && !dry; k += 8) { const q: [number, number] = [p[0] + fx * Math.min(k, room), p[1] + fy * Math.min(k, room)]; if (!this.wet(u, q[0], q[1])) dry = q; }
      for (let k = 8; k <= 240 && !dry; k += 8) { const q: [number, number] = [p[0] - fx * k, p[1] - fy * k]; if (!this.wet(u, q[0], q[1])) dry = q; }
      if (dry) p = dry;
    }
    // Its own stretch: where a friend already stands there, it stands beside it, on the side it comes from, a little
    // room between the two (the art director, b439: no box drawn on another), never in the river, and never so far out
    // that it cannot fight from there: then the friend's other side, else as near as it can.
    const face = bearing(p[0], p[1], t.x, t.y), [sx, sy] = ahead(face + Math.PI / 2);
    const fit = (q: [number, number]): boolean => off(q[0], q[1]) <= hold && !this.wet(u, q[0], q[1])
      && !this.units.some((e) => e.side !== u.side && live(e) && dist(e.x, e.y, q[0], q[1]) < (Math.max(e.w, e.d) + Math.max(u.w, u.d)) / 2);
    for (let k = 0; k < 4; k++) {
      const f = this.claimed(u, p[0], p[1], face);
      if (!f) break;
      const lat = (p[0] - f.x) * sx + (p[1] - f.y) * sy, mine = (u.x - f.x) * sx + (u.y - f.y) * sy;
      const side = Math.abs(lat) >= 1 ? Math.sign(lat) : Math.sign(mine) || Math.sign(lane) || (u.id < f.id ? -1 : 1);
      const by = (u.w + f.w) / 2 + LAND.room;
      const near: [number, number] = [p[0] + sx * side * (by - Math.abs(lat)), p[1] + sy * side * (by - Math.abs(lat))];
      const far: [number, number] = [p[0] - sx * side * (by + Math.abs(lat)), p[1] - sy * side * (by + Math.abs(lat))];
      const q = fit(near) ? near : fit(far) ? far : null;
      if (!q) break;
      p = q;
    }
    // Nowhere free in reach: it keeps the stand it had, rather than hop each tick between two that are taken.
    if (old && kept && this.claimed(u, p[0], p[1], face)) return there(old);
    u.stand = p; u.standFor = key; u.standFace = face; u.closing = false;
    return there(p);
  }

  /**
   * The friend whose ground this block's box would stand on at x, y facing so (with LAND.room between), or null: one
   * standing there, or one whose attack has it stand there and is nearer to it than this block is to x, y.
   */
  private claimed(u: LandUnit, x: number, y: number, face: number): { id: number; x: number; y: number; w: number } | null {
    const [fx, fy] = ahead(face), [rx, ry] = ahead(face + Math.PI / 2), mine = dist(u.x, u.y, x, y);
    for (const f of this.units) {
      if (f === u || f.side !== u.side || !fighting(f)) continue;
      let at: [number, number] | null = f.speed <= 0.5 ? [f.x, f.y] : null;
      if (!at && f.order?.kind === 'attack' && f.stand) {
        const theirs = dist(f.x, f.y, f.stand[0], f.stand[1]);
        if (theirs < mine || (theirs === mine && f.id < u.id)) at = f.stand;
      }
      if (!at) continue;
      const dx = at[0] - x, dy = at[1] - y;
      if (Math.abs(dx) > 300 || Math.abs(dy) > 300) continue;
      const c = Math.abs(Math.cos(f.face - face)), s = Math.abs(Math.sin(f.face - face));
      const pl = u.w / 2 + (f.w / 2) * c + (f.d / 2) * s + LAND.room / 2 - Math.abs(dx * rx + dy * ry);
      const pd = u.d / 2 + (f.w / 2) * s + (f.d / 2) * c - Math.abs(dx * fx + dy * fy);
      if (pl > 0 && pd > 0) return { id: f.id, x: at[0], y: at[1], w: f.w };
    }
    return null;
  }

  /**
   * Ground a block may not stand on: the river, a bridge and the road off either end of it (kept clear for those
   * coming over), or ground it can hardly cross (LAND.queue.hard). A block sent to stand there by its own order
   * (mouth false) may stand at a bridge's end.
   */
  private wet(u: LandUnit, x: number, y: number, mouth = true): boolean {
    if (MOTION[u.kind] === 'fixed') return false;
    const g = groundAt(this.field, clamp(x, 5, this.field.w - 5), clamp(y, 5, this.field.h - 5));
    if (g === 'bridge' || g === 'water' || g === 'blown' || LAND.pace[MOTION[u.kind]][g] <= LAND.queue.hard) return true;
    if (!mouth) return false;
    if (!this.mouths) {
      const f = this.field, C = f.cols, R = f.rows, n = Math.round(LAND.mouth / f.cell);
      const m = this.mouths = new Uint8Array(C * R);
      for (let i = 0; i < C * R; i++) {
        const k = GROUNDS[f.ground[i]];
        if (k !== 'bridge' && k !== 'blown') continue;
        const c0 = i % C, r0 = Math.floor(i / C);
        for (let r = Math.max(0, r0 - n); r <= Math.min(R - 1, r0 + n); r++) for (let c = Math.max(0, c0 - n); c <= Math.min(C - 1, c0 + n); c++) m[r * C + c] = 1;
      }
    }
    return this.mouths[this.cellAt(x, y)] === 1;
  }
  /** The cells within LAND.mouth of a bridge, worked out once. */
  private mouths: Uint8Array | null = null;

  private turnTo(u: LandUnit, want: number): void {
    const r = LAND.turn[u.kind] * DT;
    const d = turn(u.face, want);
    u.face = Math.abs(d) <= r ? want : u.face + Math.sign(d) * r;
  }

  // ---- The end --------------------------------------------------------------------------------------------------

  /** Each side's standing power as a share of both sides' at the start: the balance bar. */
  standing(): Record<Side, number> {
    const tot = this.power.att + this.power.def;
    const s = { att: 0, def: 0 };
    for (const u of this.units) {
      if (!live(u)) continue;
      const k = u.state === 'routing' ? 0.3 : 1;
      s[u.side] += (u.power0 * (u.str / u.str0) * k) / tot;
    }
    return s;
  }

  private brokenShare(s: Side): number {
    let gone = 0;
    for (const u of this.units) if (u.side === s && (u.state === 'routing' || u.state === 'gone')) gone += u.power0;
    return gone / this.power[s];
  }

  /** The share of a side's power worn away on the field (strength lost, before anyone runs). */
  wornShare(s: Side): number {
    let worn = 0;
    for (const u of this.units) if (u.side === s) worn += u.power0 * (1 - u.str / u.str0);
    return worn / this.power[s];
  }

  /**
   * How near an army is to breaking, 0 to 1: its whole worn state weighted by strength against what an army of its
   * side will stand (LAND.spent), or half its power running, whichever is nearer. One block running counts only its
   * own share, so it never takes the army with it while the rest is fresh (ruling 226(2)). The army bar shows this.
   */
  breaking(s: Side): number {
    const worn = this.wornShare(s) / LAND.spent[s];
    // Worn past what it will stand, it still holds while most of it has not been in the fight: the bar waits at the brink.
    const held = worn >= 1 && this.foughtShare(s) < LAND.fought ? 0.99 : 1;
    return Math.min(held, Math.max(this.brokenShare(s) / LAND.breaks, worn));
  }

  /** The share of a side's power that has been in the fight: struck a blow, or been worn by one. */
  private foughtShare(s: Side): number {
    let n = 0;
    for (const u of this.units) if (u.side === s && (u.met || u.str < u.str0 * 0.97 || !fighting(u))) n += u.power0;
    return n / this.power[s];
  }

  private judge(): void {
    for (const s of ['att', 'def'] as Side[]) {
      const left = this.units.filter((u) => u.side === s && fighting(u));
      if (!left.length) { this.finish(other(s), this.units.some((u) => u.side === s && u.state === 'withdrawn') && !this.units.some((u) => u.side === s && u.state === 'routing') ? 'withdrew' : 'destroyed'); return; }
      const near = this.breaking(s);
      // At three quarters, a line each army's way (the UI director's words for ruling 226(3)): the player's own in amber
      // on the page, the other's one dispatch. With nobody playing, nothing is said.
      if (!this.warned[s] && near >= 0.75 && (this.ticket.att.human || this.ticket.def.human)) {
        this.warned[s] = true;
        this.say({ what: 'nearBreak', side: s, text: this.ticket[s].human ? 'Your army is near breaking: pull back the worn, or hand it to the staff.' : `${this.army(s)} is near breaking.` });
      }
      // An army breaks when half its power is running or gone, or when it has bled as much as an army of its side
      // will stand (LAND.spent): so a battle fought out costs about what the campaign bills for its result (b356).
      if (near >= 1) {
        this.broke[s] = true;
        // The blocks that broke are already running; the rest fall back in order with their army.
        for (const u of left) { u.fellBack = true; u.order = { kind: 'withdraw', x: u.x, y: s === 'att' ? 0 : this.field.h }; u.path = []; u.keepFace = null; u.engaged = undefined; }
        this.say({ what: 'broken', side: s, text: `${this.army(s)} breaks.` });
        this.finish(other(s), 'rout');
        return;
      }
    }
    if (this.t >= this.ticket.limit) this.finish('def', 'time');
  }

  /** Each side's share of its starting power lost: strength worn away, and what runners and the fled leave behind them. */
  lostShares(): Record<Side, number> {
    const lost = { att: 0, def: 0 };
    for (const u of this.units) {
      const kept = u.state === 'gone' ? u.str * (1 - LAND.count.gone[u.side]) : u.state === 'routing' || u.fellBack ? u.str * (1 - LAND.count.routing[u.side]) : u.str;
      lost[u.side] += u.power0 * (1 - kept / u.str0);
    }
    return { att: lost.att / this.power.att, def: lost.def / this.power.def };
  }

  private finish(winner: Side, how: FieldResult['how'], R?: number): void {
    if (this.over) return;
    const lost = this.lostShares();
    // Men counted as the shares are, with what the runners and the fled leave behind, so the two never tell two stories.
    const men = { att: 0, def: 0 };
    for (const u of this.units) {
      const kept = u.state === 'gone' ? u.str * (1 - LAND.count.gone[u.side]) : u.state === 'routing' || u.fellBack ? u.str * (1 - LAND.count.routing[u.side]) : u.str;
      men[u.side] += Math.round(u.men0 * (1 - kept / u.str0));
    }
    const k = this.ticket.reckoning;
    const r = R ?? clampToDice(fieldRatio(winner, lost), k.odds, k.dice, winner);
    // The campaign's verdict follows R: under the dice's clamp it can differ from who held the field.
    const verdict: Side = r >= RATIO.carry ? 'att' : 'def';
    const forces = new Map<number, { corps: number; side: Side; lost: number; p: number }>();
    for (const u of this.units) {
      const f = forces.get(u.corps) ?? { corps: u.corps, side: u.side, lost: 0, p: 0 };
      f.lost += u.power0 * (1 - u.str / u.str0); f.p += u.power0;
      forces.set(u.corps, f);
    }
    const loser = other(winner);
    // "about 4%", or under one in a hundred "less than 1%", never "about 0%" (the UI director's words, b358).
    // Where nothing was lost at all, it says "none" (b408).
    const pct = (p: number): string => (p < 1e-4 ? 'none' : p < 0.005 ? 'less than 1%' : `about ${Math.round(p * 100)}%`);
    const cap = (t: string): string => t[0].toUpperCase() + t.slice(1);
    const line = this.practice && this.ticket.def.human ? 'Your line' : this.practice ? 'Their line' : `The ${this.ticket.def.adj} line`;
    const lines = [
      how === 'rout' ? this.brokeAfter(loser)
        : how === 'withdrew' ? `${this.army(loser)} withdrew from the field.`
          : how === 'time' ? `${line} held until the day was done.`
            : how === 'destroyed' ? `${this.army(loser)} was driven from the field.`
              : this.staffLine(),
    ];
    // Handed over, the field's figure is only the part before the staff took it: it says so, as it reads beside the
    // campaign's bill for the whole day (the design director, b356). Handed over before a shot, the staff's line says so
    // already, and there is no figure to give (the UI director, after the retest).
    if (how !== 'objective') lines.push(lost.att < 1e-4 && lost.def < 1e-4 ? 'Neither side lost any of its strength.' : `${cap(this.sideOf('att', 'attack'))} lost ${pct(lost.att)} of its strength, ${this.sideOf('def', 'defence')} ${pct(lost.def)}.`);
    else if (this.contactAt !== null) lines.push(`Before you handed the battle to the staff, ${this.sideOf('att', 'attack')} had lost ${pct(lost.att)} of its strength on the field, and ${this.sideOf('def', 'defence')} ${pct(lost.def)}.`);
    // The battle is over: nothing marches on (the blocks' cards and the 3D models alike).
    for (const u of this.units) if (live(u) && u.state !== 'routing') { u.act = 'idle'; u.speed = 0; u.engaged = undefined; }
    this.over = {
      winner: verdict, how, seconds: Math.round(this.t), lost, men, R: Math.round(r * 1000) / 1000,
      level: levelOf(r, men.att > men.def), lines,
      forces: [...forces.values()].map((f) => ({ corps: f.corps, side: f.side, lost: f.p > 0 ? Math.round((f.lost / f.p) * 1000) / 1000 : 0 })),
    };
    if (verdict !== winner) this.over.lines.push('The day went their way, but the campaign counts it as close as the odds allowed.');
    this.say({ what: 'end', text: lines[0] });
  }

  /**
   * The staff's line, in its three forms (the UI director's words for ruling 226(5)): before any contact, the whole
   * attack; after it, the rest, and "what you had won stays won" only where the field had moved the player's way from
   * where the card put it, by five points of the balance or more.
   */
  private staffLine(): string {
    // A practice ground is not a place: the staff settle the battle there, not the attack on it (b408).
    const what = this.practice ? 'battle' : `attack on ${this.ticket.place.name}`;
    // Before the armies met: true whether or not the guns had opened (b409, the UI director's words).
    if (this.contactAt === null) return `The staff settled the whole ${what}: the armies had not met.`;
    const me: Side = this.ticket.def.human && !this.ticket.att.human ? 'def' : 'att';
    const bal = this.standing(), tot = this.power.att + this.power.def;
    // Moved his way by five points of the balance's share or more (the design director, 20:51): a point's drift is not a gain.
    const moved = (bal[me] / Math.max(1e-9, bal.att + bal.def)) >= this.power[me] / tot + 0.05;
    return `The staff settled the rest of the ${what} from where it stood${moved ? ': what you had won stays won' : ''}.`;
  }

  /** How long it broke after, counted from the first contact (ruling 226(6), the UI director's words). */
  private brokeAfter(s: Side): string {
    const m = Math.round((this.t - (this.contactAt ?? this.t)) / 60);
    const who = this.army(s);
    return m < 1 ? `${who} broke within a minute of the first shots.` : m === 1 ? `${who} broke after a minute of fighting.` : `${who} broke after ${m} minutes of fighting.`;
  }

  /** Each side's standing power against what it brought, as the staff reckons the day now. */
  private standingShares(): Record<Side, number> {
    const st = this.standing(), tot = this.power.att + this.power.def;
    return { att: st.att / (this.power.att / tot), def: st.def / (this.power.def / tot) };
  }

  /**
   * The staff's chance now that the attack carries the ground, were the battle handed to them this moment: the dice's
   * own chance at the ratio standing, held to their run at the card's odds (what toDice would roll). For HAND IT TO THE
   * STAFF's question (ruling 226(5)).
   */
  chanceNow(): number {
    const k = this.ticket.reckoning, sh = this.standingShares();
    if (k.odds * k.dice[0] >= RATIO.carry) return 1;
    if (k.odds * k.dice[1] < RATIO.carry) return 0;
    const now = k.odds * (Math.max(1e-6, sh.att) / Math.max(1e-6, sh.def));
    return Math.max(0, Math.min(1, (k.dice[1] - RATIO.carry / Math.max(1e-6, now)) / (k.dice[1] - k.dice[0])));
  }

  /** Hand it to the staff (ruling 217(4), the button named in b348): the rest is rolled on the dice, at the ratio then standing. */
  toDice(): void {
    if (this.over) return;
    const k = this.ticket.reckoning;
    const R = diceNow(k.odds, k.dice, this.standingShares(), this.rng.next());
    this.finish(R >= RATIO.carry ? 'att' : 'def', 'objective', R);
  }

  view(as?: Side): BattleView<LandUnit> {
    const ev = this.events;
    this.events = [];
    void as; // Every block on the field is in sight in the first slice. TODO(polish): fog of war on the field.
    return { t: this.t, limit: this.ticket.limit, units: this.units, balance: this.standing(), events: ev, zones: [], ai: { ...this.ai }, over: this.over, breaking: { att: this.breaking('att'), def: this.breaking('def') }, contact: this.contactAt };
  }
}

/** Kinds the sim knows, for anything that builds a ticket by hand. */
export const KINDS: Kind[] = ['inf', 'mot', 'arm', 'cav', 'mtn', 'fort', 'art', 'eng'];
export { slopeAt };
