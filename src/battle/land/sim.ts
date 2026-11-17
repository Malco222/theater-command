// The land battle: a field, two armies of blocks, a clock.
//
// Each tick: the computer generals think (once a second), every block moves or holds, every block in reach of an
// enemy wears it down, nerve follows losses, runners run, and the battle ends when an army breaks, leaves, or the
// clock runs out. Deterministic: the same ticket and the same orders at the same ticks replay the same battle.

import { Rng } from '../../core/rng';
import { ONE_SHOTS, ORDER_CUES } from '../core/cues';
import { ahead, aspectOf, bearing, clamp, dist, turn } from '../core/geometry';
import { clampToDice, diceNow, fieldRatio, levelOf, RATIO } from '../core/result';
import { lineUp } from '../core/select';
import { DT, type BattleSim, type BattleView, type Command, type FieldEvent, type FieldResult, type FieldTicket, type Side } from '../core/types';
import { thinkFor } from './ai';
import { buildField, groundAt, heightAt, slopeAt, type LandField, type LandPlace } from './field';
import { LAND, MOTION, type Kind } from './rules';
import { deploy, makeBlocks, type LandUnit } from './units';

const other = (s: Side): Side => (s === 'att' ? 'def' : 'att');
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
  }

  private say(e: Omit<FieldEvent, 't'>): void {
    const ev = { t: Math.round(this.t * 10) / 10, ...e } as FieldEvent;
    if (!ev.cue && ONE_SHOTS[ev.what]) ev.cue = ONE_SHOTS[ev.what];
    this.events.push(ev);
    this.log.push(ev);
  }

  unit(id: number): LandUnit | undefined { return this.units.find((u) => u.id === id); }

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
        across.forEach((u, i) => this.send(u, { kind: 'attack', x: t.x, y: t.y, target: t.id, ...(us.length > 1 ? { lane: (i - (us.length - 1) / 2) * step, axis } : {}) }));
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
    o.y = clamp(o.y, 10, this.field.h - 10);
    u.order = o;
    u.keepFace = o.face ?? null;
    u.path = this.route(u, o.x, o.y);
  }

  /** The way there: straight, unless water it cannot cross lies across it; then by the nearest bridge. */
  private route(u: LandUnit, x: number, y: number): [number, number][] {
    const m = MOTION[u.kind];
    const blocked = (gx: number, gy: number): boolean => LAND.pace[m][groundAt(this.field, gx, gy)] <= 0;
    const steps = Math.ceil(dist(u.x, u.y, x, y) / (this.field.cell / 2));
    let wet = false;
    for (let i = 1; i < steps; i++) {
      const k = i / steps, gx = u.x + (x - u.x) * k, gy = u.y + (y - u.y) * k, g = groundAt(this.field, gx, gy);
      if ((g === 'water' || g === 'blown') && blocked(gx, gy)) { wet = true; break; }
    }
    if (!wet) return [[x, y]];
    // The bridge nearest the line between here and there.
    const f = this.field;
    let best: [number, number] | null = null, bd = Infinity;
    for (let r = 0; r < f.rows; r++) for (let c = 0; c < f.cols; c++) {
      if (f.ground[r * f.cols + c] !== 7) continue; // 'bridge'
      const bx = c * f.cell + f.cell / 2, by = r * f.cell + f.cell / 2;
      const d = dist(u.x, u.y, bx, by) + dist(bx, by, x, y);
      if (d < bd) { bd = d; best = [bx, by]; }
    }
    if (!best) return [[x, y]];
    const n = Math.sign(y - u.y) || 1;
    return [[best[0], best[1] - n * 90], [best[0], best[1] + n * 90], [x, y]];
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
      if (!u.met) { u.met = true; if (u.side === 'att' || !t.met) this.say({ what: 'contact', unit: u.id, side: u.side, text: `${u.name} is in action.`, x: u.x, y: u.y }); }
      const blow = this.blow(u, t);
      const h = hurt.get(t.id) ?? { dmg: 0, nerve: 0 };
      h.dmg += blow.dmg; h.nerve += blow.nerve;
      hurt.set(t.id, h);
      if (blow.front) t.pinned = true;
      if (blow.aspect !== 'front' && this.t - t.flankedAt > 20) {
        t.flankedAt = this.t;
        if (t.side === 'att' ? this.ticket.att.human : this.ticket.def.human) this.say({ what: 'flanked', unit: t.id, side: t.side, text: `${t.name} is hit from the ${blow.aspect === 'rear' ? 'rear' : 'flank'}.`, x: t.x, y: t.y });
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
    const go = (u: LandUnit, ux: number, uy: number, push: number): void => {
      const nx = clamp(u.x + ux * push, 5, this.field.w - 5), ny = clamp(u.y + uy * push, 5, this.field.h - 5);
      if (LAND.pace[MOTION[u.kind]][groundAt(this.field, nx, ny)] > 0) { u.x = nx; u.y = ny; }
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
      const along = pl < pd;
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
    const gu = groundAt(this.field, u.x, u.y), gt = groundAt(this.field, t.x, t.y);
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
    if (t.dug && dist(t.x, t.y, t.home[0], t.home[1]) < LAND.dug.keep) take *= LAND.dug.take;
    if (t.side === 'def' && dist(t.x, t.y, t.home[0], t.home[1]) < LAND.hold.within) take *= LAND.hold.take[this.ticket.place.terrain] ?? 0.4;
    const luck = 1 + LAND.jitter * (this.rng.next() * 2 - 1);
    const dmg = p * am * take * luck * DT;
    return { dmg, nerve: nm * nerveMul, front: aspect === 'front', aspect };
  }

  private suffer(t: LandUnit, dmg: number, nerveMul: number): void {
    // Damage is in power: as a share of what the block came with, it comes off its strength meter.
    const share = dmg / Math.max(1e-9, t.power0);
    const before = t.str;
    t.str = Math.max(0, t.str - share * t.str0);
    const lost = (before - t.str) / t.str0;
    const steady = 1 - LAND.nerve.xp * t.xp - LAND.nerve.general * this.generalOf(t.side);
    t.mor = Math.max(0, t.mor - lost * LAND.nerve.perLoss * (nerveMul / Math.max(1, dmg > 0 ? 1 : 1)) * Math.max(0.3, steady));
    t.still = 0;
    const full = LAND.figures[t.kind];
    t.figures = t.str <= 0 ? 0 : Math.max(1, Math.ceil(full * t.str));
  }

  private generalOf(s: Side): number { return this.ticket[s].general?.level ?? 0; }

  private nerve(u: LandUnit): void {
    const N = LAND.nerve;
    const enemyNear = this.units.some((e) => e.side !== u.side && fighting(e) && dist(e.x, e.y, u.x, u.y) < N.clear);
    if (enemyNear) u.clear = 0;
    if (u.state === 'routing') {
      if (u.clear > N.quiet) u.mor = Math.min(1, u.mor + N.runnerRecover * DT);
      if (!this.broke[u.side] && u.clear > N.quiet && u.mor >= N.rally && u.str > N.shattered) {
        u.state = 'wavering'; u.order = null; u.path = []; u.act = 'idle';
        this.say({ what: 'rallied', unit: u.id, side: u.side, text: `${u.name} has rallied.`, x: u.x, y: u.y });
      }
      return;
    }
    if (u.still > N.still) u.mor = Math.min(1, u.mor + N.recover * DT);
    if (u.str <= N.shattered * u.str0 || u.mor < N.rout) { this.rout(u); return; }
    const was = u.state;
    u.state = u.mor < N.waver ? 'wavering' : 'steady';
    if (was === 'steady' && u.state === 'wavering') this.say({ what: 'wavering', unit: u.id, side: u.side, text: `${u.name} is wavering.`, x: u.x, y: u.y });
  }

  private rout(u: LandUnit): void {
    if (u.state === 'routing') return;
    u.state = 'routing'; u.order = null; u.path = []; u.keepFace = null; u.engaged = undefined;
    this.say({ what: 'routing', unit: u.id, side: u.side, text: `${u.name} breaks and runs.`, x: u.x, y: u.y });
    // Friends nearby see it go.
    for (const f of this.units) if (f !== u && f.side === u.side && fighting(f) && dist(f.x, f.y, u.x, u.y) < LAND.nerve.near) f.mor = Math.max(0, f.mor - LAND.nerve.friendRuns);
  }

  private move(u: LandUnit): void {
    const k = u.kind;
    let to: [number, number] | null = null, run = false;
    if (u.state === 'routing') { to = [u.x, u.side === 'att' ? -50 : this.field.h + 50]; run = true; }
    else if (u.order?.kind === 'withdraw') { to = [u.x, u.order.y]; }
    else if (u.order?.kind === 'attack') {
      const t = this.unit(u.order.target!);
      if (!t || !live(t)) { u.order = null; u.path = []; }
      else {
        // Close until it is well in reach, then stand and fight.
        const want = LAND.reach[k] * (k === 'art' ? 0.85 : 0.6);
        if (this.gap(u, t) > want) {
          const [ax, ay] = this.lanePoint(u, t, want);
          u.path = this.route(u, ax, ay); to = u.path[0];
        } else { u.path = []; to = null; }
      }
    } else if (u.order && u.path.length) {
      // A block on the march stops when an enemy is on top of it, unless it is running or pulling out.
      to = u.path[0];
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
    // Ground it cannot enter: try a little to either side, else stop.
    if (LAND.pace[m][groundAt(this.field, nx, ny)] <= 0 && !run) {
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
        this.say({ what: 'gone', unit: u.id, side: u.side, text: `${u.name} has fled the field.` });
      } else if (u.order?.kind === 'withdraw') {
        u.state = 'withdrawn'; u.act = 'down';
        this.say({ what: 'withdrawn', unit: u.id, side: u.side, text: `${u.name} has withdrawn.` });
      }
    } else if (u.order?.kind === 'withdraw' && (u.side === 'att' ? u.y <= 6 : u.y >= this.field.h - 6)) {
      u.state = 'withdrawn'; u.act = 'down';
      this.say({ what: 'withdrawn', unit: u.id, side: u.side, text: `${u.name} has withdrawn.` });
    }
  }

  /**
   * Where a block in a lane makes for: in front of its target as the attack came on, out to the side by its lane, and
   * for a lane wider than the target's front, forward round its flank. With no lane, or once it stands at that point and
   * is still out of reach, straight at the target.
   */
  private lanePoint(u: LandUnit, t: LandUnit, want: number): [number, number] {
    const o = u.order!;
    if (o.lane === undefined || o.axis === undefined) return [t.x, t.y];
    const [fx, fy] = ahead(o.axis), [rx, ry] = ahead(o.axis + Math.PI / 2);
    const lim = t.w / 2 + u.w / 2;
    const off = Math.min(Math.abs(o.lane), lim) * Math.sign(o.lane);
    const back = Math.max(0, want * 0.8 + t.d / 2 + u.d / 2 - Math.max(0, Math.abs(o.lane) - lim));
    const p: [number, number] = [t.x + rx * off - fx * back, t.y + ry * off - fy * back];
    return dist(u.x, u.y, p[0], p[1]) < 12 ? [t.x, t.y] : p;
  }

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

  private judge(): void {
    for (const s of ['att', 'def'] as Side[]) {
      const left = this.units.filter((u) => u.side === s && fighting(u));
      if (!left.length) { this.finish(other(s), this.units.some((u) => u.side === s && u.state === 'withdrawn') && !this.units.some((u) => u.side === s && u.state === 'routing') ? 'withdrew' : 'destroyed'); return; }
      if (this.brokenShare(s) >= LAND.breaks) {
        this.broke[s] = true;
        for (const u of left) this.rout(u);
        this.say({ what: 'broken', side: s, text: `The ${this.ticket[s].adj} army breaks.` });
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
      const kept = u.state === 'withdrawn' ? u.str : u.state === 'gone' ? u.str * 0.5 : u.state === 'routing' ? u.str * 0.75 : u.str;
      lost[u.side] += u.power0 * (1 - kept / u.str0);
    }
    return { att: lost.att / this.power.att, def: lost.def / this.power.def };
  }

  private finish(winner: Side, how: FieldResult['how'], R?: number): void {
    if (this.over) return;
    const lost = this.lostShares();
    const men = { att: 0, def: 0 };
    for (const u of this.units) men[u.side] += Math.round(u.men0 * (1 - u.str / u.str0));
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
    const A = this.ticket.att.adj, D = this.ticket.def.adj;
    const lines = [
      how === 'rout' ? `The ${winner === 'att' ? D : A} army broke after ${Math.round(this.t / 60)} minutes.`
        : how === 'withdrew' ? `The ${winner === 'att' ? D : A} army withdrew from the field.`
          : how === 'time' ? `The ${D} line held until the day was done.`
            : how === 'destroyed' ? `The ${winner === 'att' ? D : A} army was driven from the field.`
              : `The staff settled the rest of the attack on ${this.ticket.place.town ?? this.ticket.place.name} from where it stood: what you had won stays won.`,
      `The ${A} attack lost about ${Math.round(lost.att * 100)}% of its strength, the ${D} defence about ${Math.round(lost.def * 100)}%.`,
    ];
    this.over = {
      winner: verdict, how, seconds: Math.round(this.t), lost, men, R: Math.round(r * 1000) / 1000,
      level: levelOf(r, men.att > men.def), lines,
      forces: [...forces.values()].map((f) => ({ corps: f.corps, side: f.side, lost: f.p > 0 ? Math.round((f.lost / f.p) * 1000) / 1000 : 0 })),
    };
    if (verdict !== winner) this.over.lines.push('The day went their way, but the campaign counts it as close as the odds allowed.');
    this.say({ what: 'end', text: lines[0] });
  }

  /** Hand it to the staff (ruling 217(4), the button named in b348): the rest is rolled on the dice, at the ratio then standing. */
  toDice(): void {
    if (this.over) return;
    const k = this.ticket.reckoning;
    const st = this.standing();
    const tot = this.power.att + this.power.def;
    const share = { att: st.att / (this.power.att / tot), def: st.def / (this.power.def / tot) };
    const R = diceNow(k.odds, k.dice, share, this.rng.next());
    this.finish(R >= RATIO.carry ? 'att' : 'def', 'objective', R);
  }

  view(as?: Side): BattleView<LandUnit> {
    const ev = this.events;
    this.events = [];
    void as; // Every block on the field is in sight in the first slice. TODO(polish): fog of war on the field.
    return { t: this.t, limit: this.ticket.limit, units: this.units, balance: this.standing(), events: ev, zones: [], ai: { ...this.ai }, over: this.over };
  }
}

/** Kinds the sim knows, for anything that builds a ticket by hand. */
export const KINDS: Kind[] = ['inf', 'mot', 'arm', 'cav', 'mtn', 'fort', 'art', 'eng'];
export { slopeAt };
