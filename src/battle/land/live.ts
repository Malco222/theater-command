// The live ground battle the player commands in the 3D game (release 2). Brian, 22 November 2026 02:55Z: "i want the
// ground battles to be playable. im the general who controls their deployment and who to attack and move on a map with
// tactics"; 03:09Z: the staff runs the blocks he has not ordered, orders go by radio, and he first plays it as a
// Normandy practice battle.
//
// The 3D ground scene was built to replay a recording (Tools/land_record/record.ts writes it; LandFeed reads it). A
// live battle is the same recording written as it is fought, in pieces: header() is the recording with nothing fought
// yet (the field, the blocks, the sides, the sky), and each tick() is a delta in the recording's own shapes, so the
// scene appends it and every reader keeps reading one LandFeed::Recording. The game keeps the rules a little ahead of
// what it shows (two battle seconds for the film's cut on action, less while the player commands), so a reader never
// samples past what has been fought.
//
// The men are written exactly as the recorder writes them: a man's place is kept where a straight line from the last
// kept point would miss him by more than KEEP_M (or his facing by KEEP_F), his pose and the rest whenever they change.
// A delta carries each track's points kept since the last delta, and its tail: where he stands now, a provisional last
// point the scene replaces with the next delta. A battle fought in many ticks gives the very tracks one fought in one.
//
// Looks only, like the recorder: nothing here changes the fight. The orders are the rules' own (TC.battleOrder's
// shapes, src/battle/core/types.ts), given for the player's side.

import type { Command, FieldEvent, FieldTicket, Side } from '../core/types';
import { DT } from '../core/types';
import { DROPS, LOOPS, TOWN } from '../core/cues';
import { GROUNDS, groundAt, type LandPlace } from './field';
import { LandBattle } from './sim';
import { LAND, MOTION, type Kind } from './rules';
import { calFor } from './shells';
import type { LandUnit } from './units';
import type { Drill, MenBlock, MenOut, Round } from './drill';
import { PIECES, openSteppe, ticketOf, type SetPiece, type Steppe } from './pieces';

/** A kept point may be left out where the straight line from the last kept one misses the man by less than this (the recorder's). */
export const KEEP_M = 0.10;
export const KEEP_F = 0.05;

const r1 = (x: number): number => Math.round(x * 10) / 10;
const r2 = (x: number): number => Math.round(x * 100) / 100;
const r3 = (x: number): number => Math.round(x * 1000) / 1000;
/** Angle difference the short way round. */
const turn = (a: number, b: number): number => { let d = b - a; while (d > Math.PI) d -= 2 * Math.PI; while (d < -Math.PI) d += 2 * Math.PI; return d; };

/** A point [t, x, y, f] and a change [t, pose, cover, aimX|null, aimY|null, hidden 0|1, buttoned 0|1], as the recording writes them. */
type Pt = number[];
type Ch = (number | string | null)[];

/** One man's track as the scene reads it (the recording's men[]). */
export interface TrackOut {
  b: number; k: number; j?: number; r: string; at: Pt[]; ch: Ch[]; s: number[]; d?: number; mk?: unknown[]; out?: number; fo?: 1;
}

/** What a delta says of one track: n its number in the order tracks were first seen; who it is only the first time. */
export interface TrackDelta {
  n: number;
  b?: number; k?: number; j?: number; r?: string; fo?: 1;
  at?: Pt[]; ch?: Ch[]; s?: number[]; d?: number; mk?: unknown[];
  /** Battle time he left the list; -1: he is back on it. */
  out?: number;
}

/** What the battle did since the last delta. */
export interface LiveDelta {
  /** Battle time the rules have reached: nothing at or before it will change. */
  t: number;
  /** The setup step: the men were placed again (the player moved a block): drop every track and take these. */
  reset?: true;
  men: TrackDelta[];
  /**
   * Where men stand now, at t, flat: [n, x, y, f, n, x, y, f, ...]. A provisional last point after the track's kept
   * ones: the scene lerps to it and drops it when a later kept point arrives. Only a man whose spot differs from the last
   * point the scene has for him is listed, so a man standing still costs nothing.
   */
  tails: number[];
  rounds: (Round & { n: number })[];
  events: FieldEvent[];
  views: Record<string, unknown>[];
  phases: { b: number; ph: [number, string, number | null, number | null][] }[];
  contact: number | null;
  /** The battle's end (the after card's result), once it is over, with its last battle second. */
  over?: unknown;
  end?: number;
}

/** Two points at one spot, facing one way. */
const same = (a: Pt, b: Pt): boolean => a[1] === b[1] && a[2] === b[2] && a[3] === b[3];

const keyOf = (b: number, m: MenOut, fo: boolean): string => `${b}:${fo ? 'fo' : ''}${m.k}${m.j !== undefined ? `.${m.j}` : ''}`;

interface Kept {
  n: number;
  who: { b: number; k: number; j?: number; r: string; fo?: 1 };
  /** The last kept point (the line test's start), and the points seen since. */
  last: Pt;
  pend: Pt[];
  /** The last change, for "did anything change". */
  ch: Ch;
  shot: number | undefined;
  d?: number; mk?: unknown[]; mkKey?: string; out?: number;
  /** Not yet sent. */
  newAt: Pt[]; newCh: Ch[]; newS: number[];
  sentWho: boolean; sentD: boolean; sentMk: boolean; sentOut: number | undefined; sentTail: Pt | null;
}

/**
 * The recorder's track keeper, keeping what it has not yet sent. Its kept points are the recorder's own (the same line
 * test, KEEP_M and KEEP_F); it only remembers each man's last kept point instead of his whole walk.
 */
export class LiveKeeper {
  private men = new Map<string, Kept>();
  private order: Kept[] = [];

  see(t: number, b: number, m: MenOut, fo: boolean): void {
    const key = keyOf(b, m, fo);
    const p = [r2(t), r1(m.x), r1(m.y), r3(m.f)];
    const ch = [r2(t), m.p, m.c, m.a ? r1(m.a[0]) : null, m.a ? r1(m.a[1]) : null, m.h ? 1 : 0, m.b ? 1 : 0];
    let k = this.men.get(key);
    if (!k) {
      k = {
        n: this.order.length, who: { b, k: m.k, ...(m.j !== undefined ? { j: m.j } : {}), r: m.r, ...(fo ? { fo: 1 as const } : {}) },
        last: p, pend: [], ch, shot: undefined, newAt: [p], newCh: [ch], newS: [], sentWho: false, sentD: false, sentMk: false, sentOut: undefined, sentTail: null,
      };
      this.men.set(key, k);
      this.order.push(k);
    } else {
      if (k.ch.slice(1).some((v, i) => v !== ch[i + 1])) { k.ch = ch; k.newCh.push(ch); }
      this.keep(k, p);
    }
    if (m.s !== undefined && k.shot !== m.s) { k.shot = m.s; k.newS.push(m.s); }
    if (m.d !== undefined && k.d !== m.d) { k.d = m.d; k.sentD = false; }
    if (m.mk) {
      const mk = JSON.stringify(m.mk);
      if (mk !== k.mkKey) { k.mk = m.mk; k.mkKey = mk; k.sentMk = false; }
    }
    k.out = undefined;
  }

  /** The recorder's line test: keep the previous point if the line from the last kept point to this one misses any point between. */
  private keep(k: Kept, p: Pt): void {
    const a = k.last;
    const off = (q: Pt): boolean => {
      const span = p[0] - a[0];
      const u = span > 0 ? (q[0] - a[0]) / span : 0;
      const x = a[1] + (p[1] - a[1]) * u, y = a[2] + (p[2] - a[2]) * u, f = a[3] + turn(a[3], p[3]) * u;
      return Math.hypot(q[1] - x, q[2] - y) > KEEP_M || Math.abs(turn(f, q[3])) > KEEP_F;
    };
    if (k.pend.some(off)) {
      const q = k.pend[k.pend.length - 1];
      k.last = q; k.newAt.push(q);
      k.pend.length = 0;
    }
    // A man standing still: of a run of points at one spot, only the run's first and last can miss a straight line by
    // the most (the miss grows evenly along the line, so it is largest at a run's ends), so the points between are let
    // go. The kept points are the recorder's still; the test no longer grows with the time he has stood there.
    const n = k.pend.length;
    if (n >= 2 && same(k.pend[n - 1], p) && same(k.pend[n - 2], p)) k.pend[n - 1] = p;
    else k.pend.push(p);
  }

  /** Men not seen this step: they left with their block (their last point kept, as the recorder does). */
  gone(t: number, seen: Set<string>): void {
    for (const [key, k] of this.men) if (!seen.has(key) && k.out === undefined) {
      if (k.pend.length) { const q = k.pend[k.pend.length - 1]; k.last = q; k.newAt.push(q); k.pend.length = 0; }
      k.out = r2(t);
    }
  }

  /** Ends every track: its last point kept (the recorder's close, at the battle's end). */
  close(): void {
    for (const k of this.order) if (k.pend.length) { const q = k.pend[k.pend.length - 1]; k.last = q; k.newAt.push(q); k.pend.length = 0; }
  }

  /** What changed since the last call, and marks it sent: the tracks' news, and where the men stand now (tails). */
  drain(): { men: TrackDelta[]; tails: number[] } {
    const out: TrackDelta[] = [], tails: number[] = [];
    for (const k of this.order) {
      // Where the scene has him last once this drain's kept points are in: its tail if that is later, else his last kept point.
      const shown = k.sentTail && k.sentTail[0] > k.last[0] ? k.sentTail : k.last;
      const tail = k.pend.length ? k.pend[k.pend.length - 1] : null;
      if (tail && tail[0] > k.last[0] && !same(tail, shown)) { tails.push(k.n, tail[1], tail[2], tail[3]); k.sentTail = tail; }
      const changed = !k.sentWho || k.newAt.length || k.newCh.length || k.newS.length || !k.sentD || !k.sentMk || k.out !== k.sentOut;
      if (!changed) continue;
      const d: TrackDelta = { n: k.n };
      if (!k.sentWho) { Object.assign(d, k.who); k.sentWho = true; }
      if (k.newAt.length) { d.at = k.newAt; k.newAt = []; }
      if (k.newCh.length) { d.ch = k.newCh; k.newCh = []; }
      if (k.newS.length) { d.s = k.newS; k.newS = []; }
      if (k.d !== undefined && !k.sentD) { d.d = k.d; }
      k.sentD = true;
      if (k.mk && !k.sentMk) { d.mk = k.mk; }
      k.sentMk = true;
      // out -1: he is back on the list (the recorder deletes his out).
      if (k.out !== k.sentOut) { d.out = k.out ?? -1; k.sentOut = k.out; }
      out.push(d);
    }
    return { men: out, tails };
  }

  /** Every track as the recorder writes it, from the deltas drained so far (for the tests). */
  static assemble(deltas: LiveDelta[]): TrackOut[] {
    const tracks: TrackOut[] = [];
    for (const dl of deltas) {
      if (dl.reset) tracks.length = 0;
      for (const d of dl.men) {
        let tr = tracks[d.n];
        if (!tr) { tr = tracks[d.n] = { b: d.b!, k: d.k!, ...(d.j !== undefined ? { j: d.j } : {}), r: d.r!, at: [], ch: [], s: [], ...(d.fo ? { fo: 1 as const } : {}) }; }
        if (d.at) tr.at.push(...d.at);
        if (d.ch) tr.ch.push(...d.ch);
        if (d.s) tr.s.push(...d.s);
        if (d.d !== undefined) tr.d = d.d;
        if (d.mk) tr.mk = d.mk;
        if (d.out === -1) delete tr.out; else if (d.out !== undefined) tr.out = d.out;
      }
    }
    return tracks;
  }
}

/** One block as the 3D game reads it: the bridge's unitOut for land, plus whether the player has met it. */
export function landUnitOut(u: LandUnit, t: number, known = true): Record<string, unknown> {
  const motion = MOTION[u.kind as Kind] as string | undefined;
  const loops = motion && motion !== 'fixed' ? LOOPS[motion as keyof typeof LOOPS] : undefined;
  const town = u.act === 'fight' && (u.cover === 'buildings' || u.cover === 'suburb' || u.cover === 'industry') && !!loops && motion !== 'guns';
  const drops = town ? TOWN.drops : motion && motion !== 'fixed' ? DROPS[motion as keyof typeof LOOPS]?.[u.act] : undefined;
  if (!known) {
    // An enemy block the player has not met: where it stands and whether it moves, nothing that names, counts or arms it (b464(2)).
    const off = u.state === 'gone' || u.state === 'withdrawn';
    const [w, d] = LAND.size.inf;
    return { id: u.id, side: u.side, known: false, x: r1(u.x), y: r1(u.y), face: r3(u.face), w, d, act: off ? 'down' : u.speed > 0.05 ? 'march' : 'idle', speed: r1(u.speed), cover: u.cover ?? '', state: off ? u.state : 'unknown', ...(u.hid ? { hid: 1 } : {}) };
  }
  return {
    id: u.id, side: u.side, kind: u.kind, name: u.name, x: r1(u.x), y: r1(u.y), face: r3(u.face),
    str: r3(u.str), mor: r3(u.mor), state: u.state, figures0: u.figures0, figures: u.figures,
    w: u.w, d: u.d, act: u.act, speed: r1(u.speed), engaged: u.engaged ?? -1, cover: u.cover ?? '',
    loop: town ? TOWN.loop : loops?.[u.act] ?? '',
    ...(drops ? { drops: { takes: drops.takes, every: drops.every } } : {}),
    ...(u.short ? { short: u.short, parent: u.parent } : {}),
    ...(u.crew ? { crew: u.crew } : {}),
    ...(u.cal ? { cal: u.cal } : {}),
    ...(u.aloneAt !== undefined && t - u.aloneAt <= 1 ? { alone: true } : {}),
    ...(u.aim !== undefined ? { aim: u.aim } : {}),
    ...(u.team ? { team: u.team } : {}),
    // Direct command (release 2), as the bridge writes it: a block the player has taken in hand, and the seconds till his
    // order on the air reaches it.
    ...(u.taken ? { taken: true } : {}),
    // Hidden in the woods from the other side (release 2, LAND.hide): his own show the forest mark, the enemy's are not drawn.
    ...(u.hid ? { hid: 1 } : {}),
    ...(u.orderAt !== undefined ? { orderIn: Math.max(0, r1(u.orderAt - t)) } : {}),
    order: u.order ? { kind: u.order.kind, x: r1(u.order.x), y: r1(u.order.y), target: u.order.target ?? -1, path: u.path.map(([x, y]) => [r1(x), r1(y)]) } : null,
  };
}

export interface LiveOpts {
  /** The side the player commands; null: the computer fights both (a film). */
  me?: Side | null;
  /** Open the setup step for the player's side before the clock runs (release 2). */
  setup?: boolean;
  /** Each rifle figure drawn as its whole platoon (Brian: "a lot of troops"); off: sections, as the first recordings. */
  platoons?: boolean;
  /**
   * Direct command (Brian, 03:09Z "do 1 2 and 3"; the rules' switches, sim.ts perUnit and radio). staff: a block he
   * orders is his, and his staff fights the rest of his side. radio: his orders land a few seconds later, longer where
   * the block is hit. Both on for the player unless turned off.
   */
  staff?: boolean;
  radio?: boolean;
  /**
   * Ambushes (Brian, 22 November 04:18Z: "if you put units in the forest then they aren't visible to the enemy until you
   * get close or they start shooting"; the rules' LAND.hide, sim.hideOn): a block in woods when the clock starts is hidden
   * from the other side till found, for both sides alike. On unless turned off.
   */
  hide?: boolean;
}

/** The live battle: a set piece (or any land ticket) fought step by step, written as the recording's pieces. */
export class LiveBattle {
  readonly sim: LandBattle;
  readonly drill: Drill;
  readonly me: Side | null;
  private keeper = new LiveKeeper();
  private steppe: Steppe | null;
  private blocks: Record<string, unknown>[];
  private seen = new Set<number>();
  private roundsNew: (Round & { n: number })[] = [];
  private phases = new Map<number, [number, string, number | null, number | null]>();
  private phaseNew = new Map<number, [number, string, number | null, number | null][]>();
  private events: FieldEvent[] = [];
  private views: Record<string, unknown>[] = [];
  private lastView = -1;
  private placed = false;
  private sentOver = false;

  constructor(readonly ticket: FieldTicket<LandPlace>, readonly piece: SetPiece | null, opts: LiveOpts = {}) {
    this.me = opts.me ?? null;
    const sim = (this.sim = new LandBattle(ticket));
    this.steppe = piece?.open === 'steppe' ? openSteppe(sim.field, ticket.seed) : null;
    if (this.steppe) for (const u of sim.units) u.cover = groundAt(sim.field, u.x, u.y);
    for (const u of sim.units) if (u.kind === 'arm') { const c = calFor('arm', u.nat, ticket.turn); if (c) u.cal = c; }
    // The player's side is still thought for by its staff (the rules thread keeps the blocks he takes his own); the other
    // side's general fights as ever.
    for (const s of ['att', 'def'] as Side[]) sim.command({ do: 'auto', side: s, on: true }, s);
    if (this.me && opts.staff !== false) sim.perUnit = true;
    if (this.me && opts.radio !== false) sim.radio = true;
    this.drill = sim.drillOn(opts.platoons === false ? { sections: true } : { sections: true, platoons: true });
    // Every block of his side placeable (the rules' setupOn all), not only his guns and teams.
    if (opts.setup && this.me) sim.setupOn(this.me, true);
    if (opts.hide !== false) sim.hideOn();
    const tanksSeen: Record<string, number> = { att: 0, def: 0 };
    this.blocks = sim.units.map((u) => {
      const a = piece ? (u.side === 'att' ? piece.att : piece.def) : null;
      let model: string | null = null;
      if (a && u.kind === 'arm') model = a.tanks[Math.min(a.tanks.length - 1, tanksSeen[u.side]++)];
      else if (a && u.kind === 'art' && !u.team) model = a.gun;
      return {
        id: u.id, side: u.side, nat: u.nat, kind: u.kind, name: u.name, short: u.short, parent: u.parent, corps: u.corps,
        figures0: u.figures0, tray: u.tray, w: u.w, d: u.d, ...(u.cal ? { cal: u.cal } : {}), ...(u.team ? { team: u.team } : {}),
        ...(u.crew ? { crew: u.crew } : {}), model, ...(a ? { army: a.army, ...(a.standIn ? { standIn: a.standIn } : {}) } : {}),
      };
    });
    this.look();
  }

  /** The recording as the scene reads it, with nothing fought yet: what LandFeed reads before the first delta. */
  header(): Record<string, unknown> {
    const p = this.piece, f = this.sim.field, t = this.ticket;
    const room = this.sim.setupRoom();
    return {
      what: 'LIVE: a land battle fought as the 3D game shows it (src/battle/land/live.ts). The recording\'s shapes; the men, rounds, events, views and phases come in the deltas.',
      recording: true, live: true, key: p?.key ?? 'field', seed: t.seed, dt: DT, title: t.title, date: t.date, turn: t.turn,
      ...(p ? { sky: { ...p.sky, latLon: p.latLon, hour: p.hour } } : {}),
      sides: {
        att: { nat: t.att.nat, adj: t.att.adj, color: t.att.color, ...(p ? { army: p.att.army, ...(p.att.standIn ? { standIn: p.att.standIn } : {}) } : {}) },
        def: { nat: t.def.nat, adj: t.def.adj, color: t.def.color, ...(p ? { army: p.def.army, ...(p.def.standIn ? { standIn: p.def.standIn } : {}) } : {}) },
      },
      field: {
        w: f.w, h: f.h, cell: f.cell, cols: f.cols, rows: f.rows, grounds: GROUNDS, ground: f.ground, height: f.height.map((x: number) => r1(x)),
        zones: f.zones, town: f.town, place: t.place,
        ...(f.storeys ? { storeys: f.storeys } : {}), ...(f.landmark ? { landmark: f.landmark } : {}), ...(f.stream ? { stream: f.stream } : {}),
        ...(this.steppe ? { steppe: this.steppe } : {}),
      },
      limit: LAND.limit, contact: null, end: 0,
      blocks: this.blocks,
      phases: [], views: [], events: [], rounds: [], men: [], over: null,
      me: this.me, ...(room ? { setup: { side: this.me, room } } : {}),
    };
  }

  /** Watches one step's men, rounds, phases, events and view, as the recorder's look() does. */
  private look(): void {
    const t = this.sim.t;
    const men = this.drill.view() as MenBlock[];
    const seen = new Set<string>();
    for (const b of men) {
      for (const m of b.men) { this.keeper.see(t, b.id, m, false); seen.add(keyOf(b.id, m, false)); }
      for (const m of b.fo ?? []) { this.keeper.see(t, b.id, m, true); seen.add(keyOf(b.id, m, true)); }
      const last = this.phases.get(b.id);
      const now: [number, string, number | null, number | null] = [r2(t), b.ph, b.foWith ?? null, b.foOn ?? null];
      if (!last || last[1] !== now[1] || last[2] !== now[2] || last[3] !== now[3]) {
        this.phases.set(b.id, now);
        const ph = this.phaseNew.get(b.id) ?? [];
        ph.push(now);
        this.phaseNew.set(b.id, ph);
      }
    }
    this.keeper.gone(t, seen);
    // The drill keeps its rounds a short while only (data/battle/drill.json rounds.keep): taken every step, as the recorder does.
    for (const r of this.drill.rounds() as (Round & { n: number })[]) if (!this.seen.has(r.n)) { this.seen.add(r.n); this.roundsNew.push(r); }
    const v = this.sim.view();
    for (const e of v.events) this.events.push(e);
    if (Math.floor(t) !== this.lastView || v.over) {
      this.lastView = Math.floor(t);
      this.views.push({
        t: r2(t), units: v.units.map((u) => landUnitOut(u as LandUnit, v.t, !this.me || this.sim.known(u as LandUnit))), balance: v.balance, zones: v.zones,
        ...(v.breaking ? { breaking: { att: r3(v.breaking.att), def: r3(v.breaking.def) } } : {}),
        ...(v.contact !== undefined ? { contact: v.contact } : {}),
        ...(v.plan ? { plan: Object.fromEntries(Object.entries(v.plan).filter(([, p]) => p?.phase).map(([s, p]) => [s, { phase: p!.phase, main: p!.main ?? [], startY: p!.startY ?? null }])) } : {}),
        ...(v.setup ? { setup: v.setup } : {}),
        ...(v.over ? { over: v.over } : {}),
      });
    }
  }

  /**
   * Fights on for up to `seconds` of battle time (none while the setup step is open) and says what happened since the
   * last delta. At most maxSteps steps a call, so the game hears back often.
   */
  tick(seconds: number, maxSteps = 50): LiveDelta {
    const steps = Math.min(maxSteps, Math.max(0, Math.round(seconds / DT)));
    for (let i = 0; i < steps && !this.sim.over && !this.sim.setup; i++) { this.sim.step(); this.look(); }
    return this.delta();
  }

  /** Fights on until battle time `t` (or the end), at most maxSteps steps. */
  to(t: number, maxSteps = 50): LiveDelta {
    return this.tick(Math.max(0, t - this.sim.t + 1e-6), maxSteps);
  }

  private delta(): LiveDelta {
    const over = this.sim.over;
    if (over && !this.sentOver) this.keeper.close();
    // (a delta opens with t, and with end at once after it when it is the battle's last: the game reads both off its head)
    const last = !!over && !this.sentOver;
    const d: LiveDelta = {
      t: r2(this.sim.t),
      ...(last ? { end: r2(this.sim.t) } : {}),
      ...(this.placed ? { reset: true as const } : {}),
      ...this.keeper.drain(),
      rounds: this.roundsNew.sort((a, b) => a.n - b.n), events: this.events, views: this.views,
      phases: [...this.phaseNew.entries()].map(([b, ph]) => ({ b, ph })),
      contact: this.sim.contactAt,
    };
    if (last) { d.over = over; this.sentOver = true; }
    this.events = []; this.views = []; this.roundsNew = []; this.phaseNew.clear(); this.placed = false;
    return d;
  }

  /** One of the player's orders, in the rules' own shapes. False where the rules refuse it. */
  order(c: Command): boolean {
    if (!this.me) return false;
    return this.sim.command(c, this.me);
  }

  /** Why a block may not stand there in the setup step, or null where it may. */
  canPlace(id: number, x: number, y: number): string | null {
    const u = this.sim.units.find((v) => v.id === id);
    if (!u) return 'No such block.';
    return this.sim.placeWhy(u, x, y);
  }

  /** Places one of the player's blocks in the setup step (and, where the rules take it, turns it to face). */
  place(id: number, x: number, y: number, face?: number): boolean {
    if (!this.me || !this.sim.setup) return false;
    const ok = this.sim.command({ do: 'land:place', units: [id], x, y, ...(face !== undefined ? { face } : {}) }, this.me);
    if (ok) this.replace();
    return ok;
  }

  /** Ends the setup step: the clock may run. */
  begin(): boolean {
    if (!this.me || !this.sim.setup) return false;
    const ok = this.sim.command({ do: 'land:begin' }, this.me);
    if (ok) this.replace();
    return ok;
  }

  /** The men stand somewhere new before the clock has run: every track starts again from where they stand now. */
  private replace(): void {
    this.keeper = new LiveKeeper();
    this.phases.clear(); this.phaseNew.clear();
    this.lastView = -1;
    this.placed = true;
    this.look();
  }
}

/** Opens a set piece live: the piece by key, its first seed unless one is given. */
export function livePiece(key: string, seed?: number, opts: LiveOpts = {}): LiveBattle {
  const p = PIECES.find((x) => x.key === key);
  if (!p) throw new Error(`no set piece ${key}`);
  const s = seed ?? p.seeds[0];
  return new LiveBattle(ticketOf(p, s, opts.me ?? null), p, opts);
}
