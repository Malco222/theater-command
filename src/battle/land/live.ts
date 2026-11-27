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
import { LandBattle, type Gate, type Hedge } from './sim';
import { drawnUp, type Formation } from './formation';
import { LAND, MOTION, type Kind } from './rules';
import { calFor } from './shells';
import type { LandUnit } from './units';
import type { Drill, MenBlock, MenOut, Round } from './drill';
import { PIECES, openSteppe, ticketOf, type SetPiece, type Steppe } from './pieces';
import { withSnipers } from './snipers';
import { AFV_PER_DIV } from './wrecks';

/** A kept point may be left out where the straight line from the last kept one misses the man by less than this (the recorder's). */
export const KEEP_M = 0.10;
export const KEEP_F = 0.05;

const r1 = (x: number): number => Math.round(x * 10) / 10;
const r2 = (x: number): number => Math.round(x * 100) / 100;
const r3 = (x: number): number => Math.round(x * 1000) / 1000;
/** Angle difference the short way round. */
const turn = (a: number, b: number): number => { let d = b - a; while (d > Math.PI) d -= 2 * Math.PI; while (d < -Math.PI) d += 2 * Math.PI; return d; };

/** A point [t, x, y, f] and a change [t, pose, cover, aimX|null, aimY|null, hidden 0|1, buttoned 0|1], as the recording
 * writes them; a man aboard adds his vehicle's k and his seat in her bed [..., seatOn, seat] (MenOut.sv). */
type Pt = number[];
type Ch = (number | string | null)[];

/** One man's track as the scene reads it (the recording's men[]). */
export interface TrackOut {
  b: number; k: number; j?: number; r: string; at: Pt[]; ch: Ch[]; s: number[]; d?: number; mk?: unknown[]; out?: number; fo?: 1;
  /** The model the 3D game draws her with where it is not her block's (release 2, looks only): a carrier of armoured infantry (absent: the lorry), a self-propelled gun, a tank destroyer. */
  ml?: string;
  /** Her model's swaps the rules pick (release 2, looks only): a motorcycle's sidecar, a staff jeep's gun. */
  sw?: Record<string, string>;
  /** A sniper team's man (release 2, looks only; MenOut.sn): 1 the sniper with his scoped rifle, 2 his spotter with his glasses. */
  sn?: 1 | 2;
  /**
   * Wounded and knocked-down men (release 2, Drill.wounded; never while it is off): the battle time he fell wounded (he
   * lies for good), and his knock-downs, each [t, round, hold], in the order sent (one beginning inside the hold before
   * it holds him to its own later end in its stead).
   */
  wd?: number; kd?: number[][];
}

/** What a delta says of one track: n its number in the order tracks were first seen; who it is only the first time. */
export interface TrackDelta {
  n: number;
  b?: number; k?: number; j?: number; r?: string; fo?: 1; ml?: string; sw?: Record<string, string>; sn?: 1 | 2;
  at?: Pt[]; ch?: Ch[]; s?: number[]; d?: number; mk?: unknown[];
  /** Battle time he left the list; -1: he is back on it. */
  out?: number;
  /** Wounded and knocked-down men (release 2, Drill.wounded; never while it is off): wd, and the knock-downs new since the last delta, each [t, round, hold] (MenOut.kd). */
  wd?: number; kd?: number[][];
}

/** What the battle did since the last delta. */
export interface LiveDelta {
  /** Battle time the rules have reached: nothing at or before it will change. */
  t: number;
  /**
   * After the break (pursue): the player who won is asked, PURSUE or END, and the rules fight nothing more until he
   * answers ({ do: 'land:pursue' } or { do: 'land:end' }). Written straight after t, as `end` is on the last delta, so
   * the game reads it off the delta's head and stops asking for more. Never with the switch off.
   */
  ask?: true;
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
  /**
   * Hedge cutters: the gaps begun since the last delta that the player may know of (his own, and the enemy's he knew as
   * it began), each once, as the 3D field reads a recording's cuts: begun at t0, open as a gateway at t1.
   */
  cuts?: { t0: number; t1: number; unit: number; x: number; y: number; yaw: number; width: number }[];
  /** The battle's end (the after card's result), once it is over, with its last battle second. */
  over?: unknown;
  end?: number;
}

/** Two points at one spot, facing one way. */
const same = (a: Pt, b: Pt): boolean => a[1] === b[1] && a[2] === b[2] && a[3] === b[3];

const keyOf = (b: number, m: MenOut, fo: boolean): string => `${b}:${fo ? 'fo' : ''}${m.k}${m.j !== undefined ? `.${m.j}` : ''}`;

interface Kept {
  n: number;
  who: { b: number; k: number; j?: number; r: string; fo?: 1; ml?: string; sw?: Record<string, string>; sn?: 1 | 2 };
  /** The last kept point (the line test's start), and the points seen since. */
  last: Pt;
  pend: Pt[];
  /** The last change, for "did anything change". */
  ch: Ch;
  shot: number | undefined;
  d?: number; mk?: unknown[]; mkKey?: string; out?: number;
  /** Wounded and knocked-down men (LiveKeeper hurt): his wd, whether it is sent, his last kd and those not yet sent. */
  wd?: number; sentWd?: boolean; kd?: number[]; newKd?: number[][];
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

  /**
   * hurt: wounded and knocked-down men (release 2, Drill.wounded) are at work (Drill.woundsOn, asked each time: the
   * bearers rule, which turns them off, is set after the keeper is made): each track carries its wd (the battle time he
   * fell wounded) and its kd (each knock-down as the drill sends it, [t, round, hold]). Off, neither is sent.
   */
  constructor(private readonly hurtOn: () => boolean = () => false) {}

  see(t: number, b: number, m: MenOut, fo: boolean): void {
    const key = keyOf(b, m, fo);
    const p = [r2(t), r1(m.x), r1(m.y), r3(m.f)];
    const ch: Ch = [r2(t), m.p, m.c, m.a ? r1(m.a[0]) : null, m.a ? r1(m.a[1]) : null, m.h ? 1 : 0, m.b ? 1 : 0];
    // (aboard a lorry, with Men in lorries on: [..., her k, his seat, 1 while he gets down] (MenOut.sv), which older
    // readers never read)
    if (m.sv) ch.push(...m.sv);
    let k = this.men.get(key);
    if (!k) {
      k = {
        n: this.order.length, who: { b, k: m.k, ...(m.j !== undefined ? { j: m.j } : {}), r: m.r, ...(fo ? { fo: 1 as const } : {}), ...(m.ml ? { ml: m.ml } : {}), ...(m.sw ? { sw: m.sw } : {}), ...(m.sn ? { sn: m.sn } : {}) },
        last: p, pend: [], ch, shot: undefined, newAt: [p], newCh: [ch], newS: [], sentWho: false, sentD: false, sentMk: false, sentOut: undefined, sentTail: null,
      };
      this.men.set(key, k);
      this.order.push(k);
    } else {
      if (k.ch.length !== ch.length || k.ch.slice(1).some((v, i) => v !== ch[i + 1])) { k.ch = ch; k.newCh.push(ch); }
      this.keep(k, p);
    }
    if (m.s !== undefined && k.shot !== m.s) { k.shot = m.s; k.newS.push(m.s); }
    if (m.d !== undefined && k.d !== m.d) { k.d = m.d; k.sentD = false; }
    if (m.mk) {
      const mk = JSON.stringify(m.mk);
      if (mk !== k.mkKey) { k.mk = m.mk; k.mkKey = mk; k.sentMk = false; }
    }
    if (this.hurtOn()) {
      if (m.wd !== undefined && k.wd !== m.wd) { k.wd = m.wd; k.sentWd = false; }
      if (m.kd && (!k.kd || k.kd.some((v, i) => v !== m.kd![i]))) { k.kd = [...m.kd]; (k.newKd ??= []).push(k.kd); }
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
      const changed = !k.sentWho || k.newAt.length || k.newCh.length || k.newS.length || !k.sentD || !k.sentMk || k.out !== k.sentOut || (k.wd !== undefined && !k.sentWd) || !!k.newKd?.length;
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
      if (k.wd !== undefined && !k.sentWd) { d.wd = k.wd; k.sentWd = true; }
      if (k.newKd?.length) { d.kd = k.newKd; k.newKd = undefined; }
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
        if (!tr) { tr = tracks[d.n] = { b: d.b!, k: d.k!, ...(d.j !== undefined ? { j: d.j } : {}), r: d.r!, at: [], ch: [], s: [], ...(d.fo ? { fo: 1 as const } : {}), ...(d.ml ? { ml: d.ml } : {}), ...(d.sw ? { sw: d.sw } : {}), ...(d.sn ? { sn: d.sn } : {}) }; }
        if (d.at) tr.at.push(...d.at);
        if (d.ch) tr.ch.push(...d.ch);
        if (d.s) tr.s.push(...d.s);
        if (d.d !== undefined) tr.d = d.d;
        if (d.mk) tr.mk = d.mk;
        if (d.wd !== undefined) tr.wd = d.wd;
        if (d.kd) (tr.kd ??= []).push(...d.kd);
        if (d.out === -1) delete tr.out; else if (d.out !== undefined) tr.out = d.out;
      }
    }
    return tracks;
  }
}

/** One block as the 3D game reads it: the bridge's unitOut for land, plus whether the player has met it. */
export function landUnitOut(u: LandUnit, t: number, known = true, mine = false, sim?: LandBattle): Record<string, unknown> {
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
    // (the real men it stands for now, for the general's plate: the historian, 22 November 05:50Z)
    men: Math.round(u.men0 * Math.min(1, u.str / Math.max(0.01, u.str0))),
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
    // Hold until relieved (release 2, sim.relief), his own blocks only: waiting for a corps coming up from next door, dug in
    // where it stands while not free; late, help never came and its men break sooner.
    ...(mine && u.waits ? { waits: { due: r1(u.waits.due), ...(u.waits.late ? { late: true } : {}), ...(u.waits.free ? { free: true } : {}) } } : {}),
    // Hedge cutters (release 2, sim.cutters): a tank block that carries them, with the share of its tanks fitted (the
    // 3D game draws the cutters on that many). Smoke on call (sim.smoke): his own blocks that may lay it, with the
    // screens they have left.
    ...(sim?.cutters && sim.cutterOf(u) ? { cutter: LAND.cutters.share } : {}),
    ...(mine && sim?.smoke ? smokeOut(sim, u) : {}),
    order: u.order ? { kind: u.order.kind, x: r1(u.order.x), y: r1(u.order.y), target: u.order.target ?? -1, path: u.path.map(([x, y]) => [r1(x), r1(y)]) } : null,
  };
}

/** A block's smoke for the 3D game (smoke on call): the screens it has left, where its army has the means. */
function smokeOut(sim: LandBattle, u: LandUnit): Record<string, unknown> {
  const m = sim.smokeOf(u);
  return m ? { smoke: u.smokeLeft ?? m.count ?? LAND.smoke.count } : {};
}

export interface LiveOpts {
  /** The side the player commands; null: the computer fights both (a film). */
  me?: Side | null;
  /** Open the setup step for the player's side before the clock runs (release 2). */
  setup?: boolean;
  /** Each rifle figure drawn as its whole platoon (Brian: "a lot of troops"); off: sections, as the first recordings. */
  platoons?: boolean;
  /**
   * The drill book (tactics.ts; Brian, 22 November 00:23Z: "move like a platoon with covering fire and historical
   * tactics"): each army's sections fight by their school, in turns, under suppression. Looks only. Left out, it is on
   * only with platoons, as it was; true, on with sections too (Brian, 25 November 13:40Z: smarter soldiers).
   */
  book?: boolean;
  /**
   * Squad life (release 2, looks only; Brian, 25 November 13:40Z: "i dont want all the soldiers of a squad doing the exact
   * same thing at the same time ... individuals apart of a squad"; Drill.life): buddy teams rushing in turn, each man
   * taking up an order in his own time, mixed stances, each watching his own arc. The drill book comes on with it. Off
   * unless true.
   */
  life?: boolean;
  /**
   * Heads up, hatches down (release 2, looks only; Brian, 25 November 15:11Z: "do those 3 ideas"; Drill.hatches): each
   * tank's commander rides with his head out until rounds come at his own tank or the enemy is within his own nerve of
   * it, drops inside at his own moment and comes up again in his own time: green crews early and for long, veterans
   * late and briefly. Off unless true (a block's tanks then close up as one with the enemy inside 120 m).
   */
  hatches?: boolean;
  /**
   * Experience you can see (release 2, looks only; Brian, 25 November 15:11Z: "do those 3 ideas"; Drill.seasoned):
   * green squads bunch up and freeze for a moment under first fire, veterans keep wider gaps and get down fast, by the
   * block's training alone. Works only with life on. Off unless true.
   */
  seasoned?: boolean;
  /**
   * When the leader falls (release 2, looks only; Brian, 25 November 15:11Z: "do those 3 ideas"; Drill.leaders): a
   * squad whose leader is hit goes to ground and hesitates a few seconds, then the next man takes charge (marked in the
   * drill only; no man's model is swapped). Works only with life on. Off unless true.
   */
  leaders?: boolean;
  /**
   * Tanks with minds of their own (release 2, looks only; Brian, 25 November 15:57Z: "i want everything to see like it
   * has its own mind"; Drill.tankmind): each tank and vehicle takes up an order at her own moment, halts briefly at her
   * own place to fire and moves on while her partner covers, hull to the threat and turret to her own arc. Needs
   * neither life nor the drill book. Off unless true.
   */
  tankmind?: boolean;
  /**
   * A tank's machine guns, seen firing (release 2, looks only; Brian, 25 November 15:55Z: "i never see the machine
   * guns on the tanks firing", 15:56Z: "i dont want it to be all the time as thats unrealistic with ammo";
   * Drill.tankmg): each tank fires short bursts at targets worth the rounds, at her own pace and moment, with pauses,
   * and eases off as her rounds run low. Needs neither life nor the drill book. Off unless true.
   */
  tankmg?: boolean;
  /**
   * Only anti-tank weapons take a tank (release 2, a real rule; Brian, 25 November 15:58Z: "cannon fire or bazookas
   * should be the only think taking them out"; the rules' sim.pierce, LAND.pierce): foot and support teams hurt a
   * tank block only with what they carry that can pierce armour. Off unless true.
   */
  pierce?: boolean;
  /**
   * Men who leave the field are seen leaving (release 2, looks only; Brian, 25 November 17:11Z: "some soldiers freeze
   * and then disapear"; Drill.exits): routing men run on past the field's edge, and the men of a block that has left
   * the fight stay in the feed, marked `lv`, until each is past his own edge. Off unless true.
   */
  exits?: boolean;
  /**
   * Support teams at work (release 2, looks only; Brian, 25 November 14:15Z: "There should be a machine gun teens that lay
   * down suppressing fire, such as the German MG 42 and the US machine gun", 15:36Z "yes to all"; Drill.teamwork): each
   * machine gun and mortar of a support team works as a crew, its men each at his own part and the team's weapons each in
   * their own time: set up one by one where they see (mortars hidden behind a rise), the machine guns sweeping their own
   * shares of the target in spells, faster as their own foot go in, shifting and lifting as they close; moved one at a
   * time when shelled; picked up in turn, the last covering. Off unless true.
   */
  teamwork?: boolean;
  /**
   * Sniper teams (release 2; Brian, 25 November 16:07Z; the rules' LAND.snipers, withSnipers): each side's foot gives its
   * army's share of its power to a few sniper teams, hidden in cover, few kills but much nerve and tempo; drawn as sniper
   * and spotter pairs. Changes the fight. Off unless true, and only with hiding on (hide).
   */
  snipers?: boolean;
  /**
   * Wounded and knocked-down men (release 2, looks only; Brian, 25 November 16:14Z: "im not seeing multiple soliders
   * being taken out by an explosion if they are in the blast radius"; Drill.wounded): a burst takes the men nearest
   * it together, a share of the fallen lie wounded, and living men near a burst are knocked down and get up again.
   * Counts never change. Off unless true.
   */
  wounded?: boolean;
  /** Stunned men (release 2, with wounded only; the design director, 26 November 12:57Z): on with wounded unless false. */
  stunned?: boolean;
  /**
   * Every man his own pace (release 2, looks only; Brian, 25 November 17:30Z: "it looks like they are all jogging at
   * the same speed"; 16:24Z: "the soldiers of a squad keep zig zaging at the same time"; Drill.paces): each man's own
   * pace and step-off, sprints and dashes, loaded and tired men slower, and a squad that no longer swerves as one.
   * Off unless true.
   */
  paces?: boolean;
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
  /**
   * Measured armour (release 2; Brian, 22 November 04:03Z to 04:16Z: the named tanks get their real advantages, German
   * tanks far weaker at the back; the rules' LAND.armour, sim.measured): a tank's or a gun's blow on a tank weighs what its
   * gun gets through the face struck at that range, each block weighed as the model the 3D game draws it with. On unless
   * turned off.
   */
  armour?: boolean;
  /**
   * Army names (release 2; the historian, 22 November 05:50Z; the rules' sim.namesOn): a regiment block reads its army's
   * own word for a third of a division over its division's full name. On unless turned off.
   */
  names?: boolean;
  /**
   * Hold until relieved (release 2; Brian, 22 November 12:48Z; the design director's R1 to R5, 13:43Z; the rules'
   * sim.relief): a block of his on foot told to wait ('land:wait') digs in for a corps coming up from next door, and
   * breaks sooner if help never comes. On for the player unless turned off.
   */
  relief?: boolean;
  /** The rider ahead (release 2; the rules' sim.rider): his own corps coming up sends a despatch rider first (view riders). */
  rider?: boolean;
  /**
   * Hedge cutters and smoke on call (release 2; Brian, 22 November 14:19Z; ruling 242; the rules' sim.cutters and
   * sim.smoke): his tanks may cut a gap through a hedge where he picks and his men on foot lay a screen along a line he
   * drags, and both staffs use them too. On unless turned off.
   */
  cutters?: boolean;
  smoke?: boolean;
  /**
   * The bocage (ruling 242 C1-B, as amended 17:45Z; the rules' sim.bocage): on a field with hedges, a tank block without
   * cutters crosses a hedge line only at a gap. Off unless turned on (the tactics thread is still finishing it).
   */
  bocage?: boolean;
  /**
   * Field telephones, stretcher bearers and the night patrol (release 2; Brian, 22 November 17:33Z, "make it where we can
   * toggle it off", and 21:21Z, each starting on; the rules' sim.phones, sim.bearers and sim.patrol): on unless false.
   * The patrol goes out only where hiding is on (hide). Views carry phones, returns and patrol.
   */
  phones?: boolean;
  bearers?: boolean;
  /**
   * Fighting on after the break (release 2; Brian, 26 November 2026: "if the enemy retreats, i have the option to
   * continue on with the battle or end it"; the rules' sim.pursue): on only where true.
   */
  pursue?: boolean;
  /** With pursue: the pursuit's own clock, a day's length from the break at the latest (the rules' sim.pursueLate): on only where true. */
  pursueLate?: boolean;
  /**
   * Falling back round an enemy in the way (release 2, the row `wayround`; the rules' sim.roundRule): a block ordered
   * to fall back steps round the nearer end of an enemy block standing in its line. On only where true.
   */
  wayround?: boolean;
  patrol?: boolean;
  /**
   * Looks the player can switch off on the Battle rules card (release 2; Brian, 23 November 01:11Z "Do all of these"):
   * captured tanks drawn in their captor's paint (blocks[].captured), uniforms that age (sides' worn), and ground that
   * keeps the ruts of its battles fought in mud (oldRuts). On unless false; the fight reads none of them.
   */
  captured?: boolean;
  worn?: boolean;
  mud?: boolean;
  /** Scarred ground (release 2, looks only): the campaign's earlier battles here, as old craters; off, none are sent. */
  scars?: boolean;
  /**
   * Support teams in battle (release 2, the Battle rules card's "teams"; the design director's ruling of 25 November
   * 17:52Z; the rules' sim.teamsRule): a team's fire shakes by its own harm, and a defence's line is never drawn out
   * of its places by a team firing behind it. NOT looks only: it changes the fight wherever team blocks are, a set
   * piece's battalions too. (A campaign battle's own teams are put on its ticket before it comes here: own-teams.ts.)
   * Off unless true.
   */
  teams?: boolean;
  /**
   * Units face the fire that hurts most (release 2, the Battle rules card's "facing"; the design director's ruling of
   * 25 November 21:54Z; the rules' sim.faceRule): a block is held to its facing only by the enemy whose blows cost it
   * most after its cover, and turns to meet another that costs it more. NOT looks only: it changes every fight. Support
   * teams in battle works only with it on (the same ruling). It works only with "stand" on. Off unless true.
   */
  facing?: boolean;
  /**
   * A stand is kept (release 2, the Battle rules card's "stand"; the design director's ruling of 26 November 01:02Z;
   * the rules' sim.standRule): a block that has stood to fight its target stands while the target is within its hold,
   * and does not rock between its way and what it fights when the target hovers at the edge of its reach. NOT looks
   * only: it changes every fight. "facing" and "teams" work only with it on (the same ruling). Off unless true.
   */
  stand?: boolean;
  /**
   * Only the hits that land shake a unit (release 2; the design director, 26 November 06:21Z; the UI director's row,
   * 06:23Z; sim.hitsShake): each blow's nerve weighs only on its own harm. Off unless true; on whenever sniper teams are
   * (they fight only with it).
   */
  hitsShake?: boolean;
  /** Shell blasts (release 2, looks only; sim.blast): one burst takes the men near it together. Off unless true. */
  blast?: boolean;
  /**
   * Infantry with tanks (release 2; sim.escort): foot advance behind their tanks, never under them, and tanks overrun
   * foot in the open. NOT looks only: the overrun moves the tank block in the rules (sim.ts overruns, LAND.overrun), so
   * it changes how battles turn out (the design director, 27 November 2026, 01:54Z, by the sniper thread's measure:
   * alone it changes 28 of 48 battles' results). Off unless true.
   */
  escort?: boolean;
  /** Men in lorries (release 1.5, looks only; sim.trucks): seats said, and over the tailboard one at a time. Off unless true. */
  trucks?: boolean;
  /**
   * Wet mud (release 1.5, looks only): the campaign's weather word over the field (thaw mud, snow, rain, clear), for
   * the 3D game's ground and the battle's sky. Off, the word is not sent and the sky is drawn as if there were none.
   */
  wet?: boolean;
  /** Old wrecks (release 2, looks only): the armour the campaign's earlier battles here left burnt out; off, none are sent. */
  wrecks?: boolean;
  /** Armoured infantry (release 2, looks only; sim.mech): an armoured block's riflemen ride behind its tanks in their carriers. Off unless true. */
  mech?: boolean;
  /** Self-propelled guns (release 2, looks only; sim.spg): some of a battery's guns drawn self-propelled. A campaign battle's only; off unless true. */
  spg?: boolean;
  /** Tank destroyers (release 2, looks only; sim.td): one model in an armoured block's mix a tank destroyer. A campaign battle's only; off unless true. */
  td?: boolean;
  /** Armoured cars (release 2, looks only; sim.cars): out ahead of a block on its flanks till it meets the enemy. A campaign battle's only; off unless true. */
  cars?: boolean;
  /** AA vehicles (release 2, looks only; sim.aa): behind a block, moving with it. A campaign battle's only; off unless true. */
  aa?: boolean;
  /** Jeeps and motorcycles (release 2, looks only; sim.light): out ahead with a block's screen or behind it. A campaign battle's only; off unless true. */
  light?: boolean;
  /**
   * Japan's own tank and field gun (release 2, looks only; the Japan/Pacific thread's finding, 25 November: Japan
   * fought in a borrowed light tank and a German howitzer). On, a Japanese armoured block is drawn as the Type 97 and
   * a Japanese battery as the 75 mm field gun, the models the fight's own table already names for her
   * (shells.ts), and every Japanese block is marked for her own coat (blocks[].coat "japan"), which the 3D game puts
   * on the models she shares with other armies: the gun, the lorry, the light tank. A campaign battle's only; off
   * unless true (the art director's stills and a Mac reading first).
   */
  japan?: boolean;
  /**
   * Island ground (release 2, looks only; "a card, then an island battle", the Japan/Pacific thread's batch 3): on, a
   * campaign battle on ground the campaign has dressed (ticket.look, ticket.latLon) is drawn as that place. The
   * header's field.look names the 3D look ("island": beach, reef and palms; "jungle": a big island's inland fight) and
   * sky.latLon carries the place's own latitude and longitude, for its sun, its weather and the men's front (without
   * it every campaign battle is drawn at 49 N 0 E, which is Normandy). Off unless true: the island look has not passed
   * the art director or the Mac. Off, or on a ticket with neither key, the header is byte for byte what it was; a set
   * piece keeps its own, and a sky given outright with its own latLon keeps that.
   */
  island?: boolean;
  /**
   * A campaign battle's (the production lead's (a), release 2): the attack card's fog, as the campaign's battle-bridge
   * sets it for its own sim (b453, b476(3)): fog, a defence the card did not know shows only where it stands until met;
   * seen, the corps the card identified, shown as the card shows them; designYears, each nation's armour design year
   * (armourYear), by which its tanks' shells go. Left out for a set piece.
   */
  fog?: boolean;
  seen?: number[];
  designYears?: Record<string, number>;
  /**
   * A campaign battle's sky over the field (looks only), where the 3D game or the campaign knows the place on the earth:
   * the cloud, the sun's latitude and longitude and the hour. Left out, ticketSky draws one from the ticket's weather and
   * seed; a set piece keeps its own.
   */
  sky?: LiveSky;
}

/** The battle's year from its ticket's date ("June 1944"), or 1942 where it names none. */
export function yearOf(t: FieldTicket<LandPlace>): number {
  const m = /(19[34]\d)/.exec(t.date ?? '');
  return m ? Number(m[1]) : 1942;
}

/**
 * A campaign battle's stand-in model for a tank or gun block (release 2, looks only): the 3D game has a Sherman, a Panzer
 * IV, a Tiger, a StuG, a T-34, a light tank and three guns so far, so each nation is drawn with the nearest it has, in
 * its own coat (LandKit dresses a model in its owner's colours), until the art director's models for every theatre come
 * (Brian, 22 November: "it's a world war game not just France"). The Nth tank of a side, for a mix.
 *
 * `own`: the nations whose own models the game now has are drawn with them instead of the nearest (the Battle rules
 * card's switch, off until the art director and the Mac pass them). Japan's are the first: the Type 97 medium tank
 * (1938 on; her earlier blocks keep the light tank) and the 75 mm field gun, which the fight's own table (shells.ts)
 * already names for her.
 */
export function standInModel(kind: string, nat: string, year: number, n: number, own = false): string {
  const axis = ['GER', 'ITA', 'JAP', 'HUN', 'ROM', 'BUL', 'FIN'].includes(nat);
  if (own && nat === 'JAP') {
    if (kind === 'art') return 'Gun75';
    if (year >= 1938) return 'Type97';
  }
  if (kind === 'art') return nat === 'SOV' ? 'ZiS3' : axis ? 'leFH18' : 'Howitzer105';
  if (nat === 'GER') return year >= 1943 ? ['PanzerIV', 'PanzerIV', 'Tiger', 'StuG3'][n % 4] : year >= 1941 ? ['PanzerIV', 'StuG3', 'LightTank'][n % 3] : ['LightTank', 'PanzerIV'][n % 2];
  if (nat === 'SOV') return year >= 1941 ? ['T34', 'T34', 'LightTank'][n % 3] : 'LightTank';
  if (axis) return year >= 1943 && nat !== 'JAP' ? ['StuG3', 'LightTank'][n % 2] : 'LightTank';
  return year >= 1942 ? ['Sherman', 'Sherman', 'LightTank'][n % 3] : 'LightTank';
}

/** The sky the header carries (the set piece's own shape): cloud cover 0..1 and its base, the place on the earth, the hour. */
export interface LiveSky {
  cover: number;
  base: number;
  kind: 'cumulus' | 'deck';
  wind?: [number, number];
  latLon?: [number, number];
  hour?: number;
}

/**
 * A campaign battle's sky where none is given: the campaign's weather over the place in its own words, varied a little by
 * the ticket's seed (Brian, 20 November: the weather varies battle to battle, clear to overcast). Looks only.
 */
export function ticketSky(t: FieldTicket<LandPlace>): LiveSky {
  const w = (t.weather ?? '').toLowerCase();
  const lean = ((t.seed % 21) - 10) / 100;
  const c = (x: number): number => Math.round(Math.min(1, Math.max(0, x + lean)) * 100) / 100;
  if (w === 'rain') return { cover: c(0.85), base: 600, kind: 'deck', hour: 11 };
  if (w === 'snow') return { cover: c(0.9), base: 500, kind: 'deck', hour: 11 };
  if (w === 'mud') return { cover: c(0.7), base: 800, kind: 'deck', hour: 11 };
  if (w === 'clear') return { cover: c(0.2), base: 1400, kind: 'cumulus', hour: 10 };
  return { cover: c(0.45), base: 1000, kind: 'cumulus', hour: 10 };
}

/** Drawing up (drawUp): how far to every side a formation is tried when a place of it is barred, and in what steps, metres. */
const DRAW = { reach: 400, step: 20 };

/** What a draw-up did (LiveBattle.drawUp). */
export interface DrawnUp {
  ok: boolean;
  why: string;
  /** Why none was drawn up, where none was. */
  none?: 'begun' | 'nothing' | 'stay' | 'room';
  id?: number;
  who?: string;
  /** Each block placed: [id, x, y, face]. */
  placed: number[][];
  /** Those named that stay where they are, and the rules' reason. */
  left: { id: number; who: string; why: string }[];
  /** How far from the place worked out the formation was stood, metres. */
  off: number;
}

/** The live battle: a set piece (or any land ticket) fought step by step, written as the recording's pieces. */
export class LiveBattle {
  readonly sim: LandBattle;
  readonly drill: Drill;
  readonly me: Side | null;
  private keeper: LiveKeeper;
  /** A campaign battle's sky (no set piece): as given, or drawn from its ticket. */
  private readonly skyOf: LiveSky;
  private steppe: Steppe | null;
  private blocks: Record<string, unknown>[];
  /**
   * Old wrecks: the real armoured vehicles one tank model stands for in this battle (both sides' armour together, at
   * least 1, to a tenth), so the 3D game lays the old wrecks at the battle's own scale; null where no armour is drawn.
   */
  private wreckPer: number | null = null;
  /** The player's looks switches the header reads (worn uniforms, mud remembered, scarred ground, old wrecks). */
  private looks: { worn: boolean; mud: boolean; scars: boolean; wrecks: boolean; wet: boolean } = { worn: true, mud: true, scars: true, wrecks: true, wet: true };
  /** The place's own dress passed on to the 3D game (release 2, LiveOpts.island): a campaign battle's only. */
  private island = false;
  private seen = new Set<number>();
  private roundsNew: (Round & { n: number })[] = [];
  private phases = new Map<number, [number, string, number | null, number | null]>();
  private phaseNew = new Map<number, [number, string, number | null, number | null][]>();
  private events: FieldEvent[] = [];
  private views: Record<string, unknown>[] = [];
  /** The hedge cutters' gaps already sent (the rules' cuts, in the order begun). */
  private cutsSent = 0;
  private lastView = -1;
  /** The aftermath's phase at the last view ('' with none): a view goes out when it changes (after the break, pursue). */
  private lastPhase = '';
  private placed = false;
  private sentOver = false;

  constructor(readonly ticket: FieldTicket<LandPlace>, readonly piece: SetPiece | null, opts: LiveOpts = {}) {
    this.me = opts.me ?? null;
    this.skyOf = opts.sky ?? ticketSky(opts.wet === false ? { ...ticket, weather: undefined } : ticket);
    // (sniper teams need hiding on, as the night patrol does: with it off no team is made and the field is as it was)
    const sim = (this.sim = new LandBattle(opts.snipers === true && opts.hide !== false ? withSnipers(ticket) : ticket));
    this.steppe = piece?.open === 'steppe' ? openSteppe(sim.field, ticket.seed) : null;
    if (this.steppe) for (const u of sim.units) u.cover = groundAt(sim.field, u.x, u.y);
    // (every block, those of corps still coming up from next door too: neighbours join the fight, release 2)
    for (const u of sim.allBlocks()) if (u.kind === 'arm') { const c = calFor('arm', u.nat, ticket.turn, opts.designYears?.[u.nat]); if (c) u.cal = c; }
    if (opts.fog) { sim.fog = true; sim.seen = new Set(opts.seen ?? []); }
    // The player's side is still thought for by its staff (the rules thread keeps the blocks he takes his own); the other
    // side's general fights as ever.
    for (const s of ['att', 'def'] as Side[]) sim.command({ do: 'auto', side: s, on: true }, s);
    if (this.me && opts.staff !== false) sim.perUnit = true;
    if (this.me && opts.radio !== false) sim.radio = true;
    // (armoured infantry: before the drill is made, which draws them with their blocks)
    if (opts.mech === true) sim.mech = true;
    // (self-propelled guns and tank destroyers: a campaign battle's, set pieces stay as the historian drew them)
    if (opts.spg === true && !piece) sim.spg = true;
    if (opts.td === true && !piece) sim.td = true;
    if (opts.cars === true && !piece) sim.cars = true;
    if (opts.aa === true && !piece) sim.aa = true;
    if (opts.light === true && !piece) sim.light = true;
    this.keeper = new LiveKeeper(() => this.drill.woundsOn);
    this.drill = sim.drillOn({ sections: true, ...(opts.platoons === false ? {} : { platoons: true }), ...(opts.book !== undefined ? { book: opts.book } : {}), ...(opts.life === true ? { life: true } : {}), ...(opts.hatches === true ? { hatches: true } : {}), ...(opts.seasoned === true ? { seasoned: true } : {}), ...(opts.leaders === true ? { leaders: true } : {}), ...(opts.tankmind === true ? { tankmind: true } : {}), ...(opts.tankmg === true ? { tankmg: true } : {}), ...(opts.exits === true ? { exits: true } : {}), ...(opts.wounded === true ? { wounded: true } : {}), ...(opts.paces === true ? { paces: true } : {}), ...(opts.teamwork === true ? { teamwork: true } : {}), ...(opts.wounded === true && opts.stunned === false ? { stunned: false } : {}) });
    // Every block of his side placeable (the rules' setupOn all), not only his guns and teams.
    if (opts.setup && this.me) sim.setupOn(this.me, true);
    if (opts.hide !== false) sim.hideOn();
    if (opts.armour !== false) sim.measured = true;
    if (opts.names !== false) sim.namesOn();
    if (this.me && opts.relief !== false) sim.relief = true;
    if (this.me && opts.rider !== false) sim.rider = true;
    if (opts.cutters !== false) sim.cutters = true;
    if (opts.smoke !== false) sim.smoke = true;
    if (opts.bocage) sim.bocage = true;
    if (opts.phones !== false) sim.phones = true;
    if (opts.bearers !== false) sim.bearers = true;
    if (opts.pursue === true) sim.pursue = true;
    if (opts.pursue === true && opts.pursueLate === true) sim.pursueLate = true;
    if (opts.wayround === true) sim.roundRule = true;
    if (opts.stand === true) sim.standRule = true;
    // ("facing" works only with "stand" on: the design director, 26 November 01:02Z.)
    if (opts.facing === true && opts.stand === true) sim.faceRule = true;
    // (Support teams in battle works only with "facing" on, and so with "stand": 25 November 21:54Z and 26 November 01:02Z.)
    if (opts.teams === true && opts.facing === true && opts.stand === true) sim.teamsRule = true;
    if (opts.hitsShake === true || (opts.snipers === true && opts.hide !== false)) sim.hitsShake = true;
    if (opts.blast === true) sim.blast = true;
    if (opts.escort === true) sim.escort = true;
    if (opts.pierce === true) sim.pierce = true;
    if (opts.trucks === true) sim.trucks = true;
    if (opts.patrol !== false && opts.hide !== false) sim.patrol = true;
    const tanksSeen: Record<string, number> = { att: 0, def: 0 };
    this.looks = { worn: opts.worn !== false, mud: opts.mud !== false, scars: opts.scars !== false, wrecks: opts.wrecks !== false, wet: opts.wet !== false };
    this.island = !piece && opts.island === true;
    // (old wrecks: the armour's real vehicles over the tank models it is drawn with; a block's worth is its divisions by
    // their strength, a regiment a third)
    const armour = sim.allBlocks().filter((u) => u.kind === 'arm' && u.figures0 > 0);
    const models = armour.reduce((s, u) => s + u.figures0, 0);
    const real = armour.reduce((s, u) => s + (AFV_PER_DIV.arm ?? 0) * (u.worth ?? u.figures0 / Math.max(1, u.tray)), 0);
    this.wreckPer = models > 0 && real > 0 ? Math.max(1, Math.round((real / models) * 10) / 10) : null;
    // (captured kit, a campaign battle's only: the first few tank blocks of a side that lately beat an army with tanks
    // are drawn as the loser's model, marked with whose they were, in their captor's paint; looks only)
    const caught = !piece && opts.captured !== false && ticket.captured ? ticket.captured : null;
    const caughtLeft: Record<string, number> = { att: caught?.att?.n ?? 0, def: caught?.def?.n ?? 0 };
    // (a corps coming up from next door is in the header from the start, its words and model known, and walks on later:
    // 'joins' says so; the views carry it once it is on the field and, till then, in 'coming')
    this.blocks = sim.allBlocks().map((u) => {
      const a = piece ? (u.side === 'att' ? piece.att : piece.def) : null;
      let model: string | null = null;
      if (a && u.kind === 'arm') model = a.tanks[Math.min(a.tanks.length - 1, tanksSeen[u.side]++)];
      else if (a && u.kind === 'art' && !u.team) model = a.gun;
      // (the rules weigh a block as the model the set piece draws it with, so a Tiger on the screen fights as a Tiger)
      if (model) sim.vehicleIs(u.id, model);
      // A campaign battle (no piece): the nearest model the 3D game has for the nation and year, looks only, so the
      // battle stays the one the campaign priced (the fight never reads it).
      else if (!piece && (u.kind === 'arm' || (u.kind === 'art' && !u.team))) model = standInModel(u.kind, u.nat, opts.designYears?.[u.nat] ?? yearOf(ticket), tanksSeen[u.side]++, opts.japan === true);
      const took = caught && u.kind === 'arm' && caughtLeft[u.side] > 0 ? caught[u.side as 'att' | 'def'] : undefined;
      if (took) {
        caughtLeft[u.side]--;
        model = standInModel('arm', took.from, opts.designYears?.[took.from] ?? yearOf(ticket), 0, opts.japan === true);
      }
      // (the drill reads a tank's machine guns and her rounds from the model the player sees: looks only, and kept only
      // with its tankmg switch on)
      if (u.kind === 'arm') this.drill.drawnAs(u.id, model);
      return {
        id: u.id, side: u.side, nat: u.nat, kind: u.kind, name: u.name, short: u.short, parent: u.parent, corps: u.corps,
        figures0: u.figures0, men0: u.men0, tray: u.tray, w: u.w, d: u.d, ...(u.cal ? { cal: u.cal } : {}), ...(u.team ? { team: u.team } : {}),
        ...(u.crew ? { crew: u.crew } : {}), model, ...(a ? { army: a.army, ...(a.standIn ? { standIn: a.standIn } : {}) } : {}),
        ...(u.joined !== undefined ? { joins: true } : {}), ...(took ? { captured: took.from } : {}),
        // (release 2, the "japan" row: a Japanese block's vehicles in her own coat where the model is one she shares with
        // other armies and its file has no Japanese paint, the field gun, the lorry, the light tank; the 3D game keeps
        // the coat by this name. Looks only, a campaign battle's only: a set piece is as the historian drew it.)
        ...(!piece && opts.japan === true && u.nat === 'JAP' ? { coat: 'japan' } : {}),
        // (release 2: the half-tracks, self-propelled guns and tank destroyers its men are drawn with, for the 3D game to
        // read before the battle opens)
        ...((mls) => (mls.length ? { mls } : {}))(this.drill.modelsFor(u)),
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
      // (wounded and knocked-down men, release 2: the drill's own wounded and knock-downs are in this battle, so the 3D
      // game leaves its own stand-in for them off for the whole of it; absent while the switch is off, and with the
      // bearers rule on, where the switch changes nothing: the rulings, sections 2 and 5.6)
      ...(this.drill.woundsOn ? { wounded: true } : {}),
      // (the campaign's weather word, for the ground under it, and scarred ground: looks only, release 2; the 3D game
      // lays thaw mud from "mud", weathered old craters from the scars, old ruts from the battles fought in mud, and the
      // burnt-out armour of the battles here from oldWrecks)
      ...(t.weather && this.looks.wet ? { weather: t.weather } : {}),
      ...(t.scars && t.scars.count > 0 && this.looks.scars ? { scars: { count: t.scars.count, months: t.scars.months } } : {}),
      ...(t.oldRuts && t.oldRuts.count > 0 && this.looks.mud ? { oldRuts: { count: t.oldRuts.count, months: t.oldRuts.months } } : {}),
      ...(t.oldWrecks && t.oldWrecks.count > 0 && this.looks.wrecks ? { oldWrecks: { count: t.oldWrecks.count, months: t.oldWrecks.months, nats: { ...t.oldWrecks.nats }, ...(this.wreckPer ? { per: this.wreckPer } : {}) } } : {}),
      // (the place's own latitude and longitude, release 2 on its "island" option: only where the sky has none)
      ...(p ? { sky: { ...p.sky, latLon: p.latLon, hour: p.hour } }
        : { sky: this.island && t.latLon && !this.skyOf.latLon ? { ...this.skyOf, latLon: t.latLon } : this.skyOf }),
      sides: {
        att: { nat: t.att.nat, adj: t.att.adj, color: t.att.color, ...(p ? { army: p.att.army, ...(p.att.standIn ? { standIn: p.att.standIn } : {}) } : {}), ...(t.att.worn !== undefined && this.looks.worn ? { worn: t.att.worn } : {}) },
        def: { nat: t.def.nat, adj: t.def.adj, color: t.def.color, ...(p ? { army: p.def.army, ...(p.def.standIn ? { standIn: p.def.standIn } : {}) } : {}), ...(t.def.worn !== undefined && this.looks.worn ? { worn: t.def.worn } : {}) },
      },
      field: {
        w: f.w, h: f.h, cell: f.cell, cols: f.cols, rows: f.rows, grounds: GROUNDS, ground: f.ground, height: f.height.map((x: number) => r1(x)),
        zones: f.zones, town: f.town, place: t.place,
        ...(f.storeys ? { storeys: f.storeys } : {}), ...(f.landmark ? { landmark: f.landmark } : {}), ...(f.stream ? { stream: f.stream } : {}),
        ...(this.steppe ? { steppe: this.steppe } : {}),
        // (the 3D look the campaign names for this ground, release 2 on its "island" option: LandScape reads it first)
        ...(this.island && t.look ? { look: t.look } : {}),
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
    // (after the break, pursue: a view the moment the winner is asked, and the moment the pursuit begins)
    const phase = v.after?.phase ?? '';
    if (Math.floor(t) !== this.lastView || v.over || phase !== this.lastPhase) {
      this.lastView = Math.floor(t);
      this.lastPhase = phase;
      this.views.push(this.row(v, t));
    }
  }

  /** One view as the recording keeps it: the blocks as the player may know them, and what the release 2 rows add. */
  private row(v: ReturnType<LandBattle['view']>, t: number): Record<string, unknown> {
    return {
      t: r2(t), units: v.units.map((u) => landUnitOut(u as LandUnit, v.t, !this.me || this.sim.known(u as LandUnit), !!this.me && u.side === this.me, this.sim)), balance: v.balance, zones: v.zones,
      // (the bars as the player sees them: under the field spread the enemy's as his staff reads it, the mask; with
      // nobody playing, or no spread, the true ones, as the view above read them)
      ...(v.breaking ? { breaking: { att: r3(this.sim.shownBar('att', this.me)), def: r3(this.sim.shownBar('def', this.me)) } } : {}),
      ...(v.contact !== undefined ? { contact: v.contact } : {}),
      ...(v.plan ? { plan: Object.fromEntries(Object.entries(v.plan).filter(([, p]) => p?.phase).map(([s, p]) => [s, { phase: p!.phase, main: p!.main ?? [], startY: p!.startY ?? null }])) } : {}),
      ...(v.setup ? { setup: v.setup } : {}),
      // (the corps still coming up from next door that the player may know of: where on its edge and when)
      ...(v.coming?.length ? { coming: v.coming.map((c) => ({ id: c.id, side: c.side, corps: c.corps, kind: c.kind, at: r1(c.at), edge: c.edge, x: r1(c.x), y: r1(c.y), ...(c.arrive ? { arrive: c.arrive } : {}) })) } : {}),
      // (the rider ahead: his own corps' despatch riders, riding in before it)
      ...(v.riders?.length ? { riders: v.riders } : {}),
      // (smoke on call: the screens standing now, the enemy's block named only where he knows it; the smoke rounds
      // carry how long each hangs, for the 3D field. The hedge cutters' gaps go once each, in the delta's cuts)
      ...this.sim.screenView(this.me ?? undefined),
      // (field telephones: the wires he may see; stretcher bearers: the light cases back in the line, each shown once
      // by its t and unit; the night patrol: the attack's own route, and where it was caught)
      ...this.sim.phonesView(this.me ?? undefined),
      ...this.sim.bearerView(this.me ?? undefined),
      ...this.sim.patrolView(this.me ?? undefined),
      // (after the break, pursue: who broke, who chooses or pursues, and the phase)
      ...(v.after ? { after: v.after } : {}),
      ...(v.over ? { over: v.over } : {}),
    };
  }

  /**
   * Fights on for up to `seconds` of battle time (none while the setup step is open) and says what happened since the
   * last delta. At most maxSteps steps a call, so the game hears back often.
   */
  tick(seconds: number, maxSteps = 50): LiveDelta {
    const steps = Math.min(maxSteps, Math.max(0, Math.round(seconds / DT)));
    // (after the break, pursue: no step while the player who won has not chosen)
    for (let i = 0; i < steps && !this.sim.over && !this.sim.setup && this.sim.aftermath?.phase !== 'ask'; i++) { this.sim.step(); this.look(); }
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
      ...(last ? { end: r2(this.sim.t) } : this.sim.aftermath?.phase === 'ask' ? { ask: true as const } : {}),
      ...(this.placed ? { reset: true as const } : {}),
      ...this.keeper.drain(),
      rounds: this.roundsNew.sort((a, b) => a.n - b.n), events: this.events, views: this.views,
      phases: [...this.phaseNew.entries()].map(([b, ph]) => ({ b, ph })),
      contact: this.sim.contactAt,
    };
    // (the gaps begun since the last delta, each once, those of the enemy only where he knew the block as it began)
    const cuts = this.sim.cuts.slice(this.cutsSent).filter((c) => !this.me || c.side === this.me || c.known);
    this.cutsSent = this.sim.cuts.length;
    if (cuts.length) d.cuts = cuts.map((c) => ({ t0: r2(c.t0), t1: r2(c.t1), unit: c.unit, x: r1(c.x), y: r1(c.y), yaw: r3(c.yaw), width: r2(c.width) }));
    if (last) { d.over = over; this.sentOver = true; }
    this.events = []; this.views = []; this.roundsNew = []; this.phaseNew.clear(); this.placed = false;
    return d;
  }

  /** One of the player's orders, in the rules' own shapes. False where the rules refuse it. */
  order(c: Command): boolean {
    if (!this.me) return false;
    const was = this.sim.aftermath?.phase, ok = this.sim.command(c, this.me);
    // After the break (pursue): END answered while the battle stood still ends it between steps, so its last words and
    // its result are looked at here, as a step's look would have.
    if (ok && was === 'ask' && this.sim.over) {
      const v = this.sim.view();
      for (const e of v.events) this.events.push(e);
      this.views.push(this.row(v, this.sim.t));
    }
    return ok;
  }

  /** After the break (pursue): why an order of his was not taken, where the aftermath is why (sim.afterWhy); else null. */
  afterWhy(): string | null {
    return this.me ? this.sim.afterWhy(this.me) : null;
  }

  /**
   * Hold until relieved: why one of his blocks may not wait for help (null where it may), and the battle second help
   * could reach it there (null where it may not wait).
   */
  waitWhy(id: number): { why: string | null; due: number | null } {
    const u = this.sim.unit(id);
    if (!u) return { why: 'No such block.', due: null };
    if (!this.me || u.side !== this.me) return { why: 'Only your own troops take your orders.', due: null };
    const why = this.sim.waitWhy(u);
    return { why, due: why === null ? this.sim.dueAt(u)?.due ?? null : null };
  }

  /**
   * Hedge cutters: why one of his tank blocks may not cut a gap through the hedge at (x, y) (null where it may); with no
   * point, whether it may cut at all.
   */
  cutWhy(id: number, x?: number, y?: number): string | null {
    const u = this.sim.unit(id);
    if (!u) return 'Nothing to order.';
    if (!this.me || u.side !== this.me) return 'Only your own troops take your orders.';
    return this.sim.cutWhy(u, x, y);
  }

  /**
   * Smoke on call: why one of his blocks may not lay a screen along that line (x0, y0 to x1, y1, or only its middle; null
   * where it may); with no line, whether it may lay one at all.
   */
  smokeWhy(id: number, x0?: number, y0?: number, x1?: number, y1?: number): string | null {
    const u = this.sim.unit(id);
    if (!u) return 'Nothing to order.';
    if (!this.me || u.side !== this.me) return 'Only your own troops take your orders.';
    return this.sim.smokeWhy(u, x0, y0, x1, y1);
  }

  /** All fall back (release 1.5): whether his army has been ordered back (it takes no new orders). */
  fallingBack(): boolean {
    return !!this.me && this.sim.fallingBack(this.me);
  }

  /** All fall back (release 1.5): why his army may not fall back now, or null where it may. */
  fallBackWhy(): string | null {
    if (!this.me) return 'Only your own troops take your orders.';
    return this.sim.fallBackWhy(this.me);
  }

  /** Night patrol: why one of his blocks may not go out on patrol tonight, or null where it may. */
  patrolWhy(id: number): string | null {
    const u = this.sim.unit(id);
    if (!u) return 'Nothing to order.';
    if (!this.me || u.side !== this.me) return 'Only your own troops take your orders.';
    return this.sim.patrolWhy(u);
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

  /**
   * The 3D field's hedges and their gates, before the clock runs (the rules' sim.hedgesOn; the historian's check, 22
   * November: a block on a hedge line hides as in woods, and the men on foot go through the gates). False once the clock
   * has run. The men stand again (a reset piece), as their hiding may have changed.
   */
  hedges(hedges: Hedge[], gates: Gate[]): boolean {
    const ok = this.sim.hedgesOn(hedges, gates);
    if (ok) this.replace();
    return ok;
  }

  /**
   * Why several of his blocks, each [id, x, y, face?], may not be placed as one act in the setup step: the block that
   * cannot go (its id and name) and why, or null where they all may.
   */
  canPlaceMany(list: readonly (readonly number[])[]): { id: number; who: string; why: string } | null {
    if (!this.me || !this.sim.setup) return { id: -1, who: '', why: 'Not now: the battle has begun.' };
    if (!list.length) return { id: -1, who: '', why: 'Nothing to place.' };
    const us = list.map((p) => this.sim.units.find((v) => v.id === Number(p[0])));
    const lost = us.findIndex((u) => !u || u.side !== this.me);
    if (lost >= 0) return { id: Number(list[lost][0]), who: '', why: us[lost] ? 'Only your own troops take your orders.' : 'No such block.' };
    const no = this.sim.placeManyWhy(us as LandUnit[], list.map((p) => p.slice(1)));
    return no ? { ...no, who: this.whoIs(no.id) } : null;
  }

  /**
   * The ground that bars a block from standing at a place in the setup step, for the screen's sentence when the rules
   * refuse it there (sim.standWhy's water line): a bridge under any of it, else the water (a blown bridge is in the
   * river), else the ground under its middle, which it can hardly cross (a marsh, bare rock or a bunker, to wheels,
   * tracks and guns). Null where there is no such block or place. Words only: no rule reads it.
   */
  barredBy(id: number, x: number, y: number): 'bridge' | 'water' | 'marsh' | 'rock' | 'bunker' | null {
    const u = this.sim.units.find((v) => v.id === id), f = this.sim.field;
    if (!u || !Number.isFinite(x) || !Number.isFinite(y)) return null;
    // (the block's box as the rules walk it, units.ts wetUnder: every half cell)
    const hw = u.w / 2, hd = u.d / 2, nx = Math.max(1, Math.ceil((2 * hw) / (f.cell / 2))), ny = Math.max(1, Math.ceil((2 * hd) / (f.cell / 2)));
    let wet = false;
    for (let i = 0; i <= nx; i++) for (let j = 0; j <= ny; j++) {
      const c = Math.floor((x - hw + (2 * hw * i) / nx) / f.cell), r = Math.floor((y - hd + (2 * hd * j) / ny) / f.cell);
      if (c < 0 || r < 0 || c >= f.cols || r >= f.rows) continue;
      const k = GROUNDS[f.ground[r * f.cols + c]];
      if (k === 'bridge') return 'bridge';
      wet = wet || k === 'water' || k === 'blown';
    }
    const g = groundAt(f, Math.min(Math.max(x, 5), f.w - 5), Math.min(Math.max(y, 5), f.h - 5));
    if (g === 'bridge') return 'bridge';
    if (wet || g === 'water' || g === 'blown') return 'water';
    return g === 'marsh' || g === 'rock' || g === 'bunker' ? g : null;
  }

  /** Places several of the player's blocks as one act in the setup step, each [id, x, y, face?]: all of them or none. */
  placeMany(list: readonly (readonly number[])[]): boolean {
    if (this.canPlaceMany(list) !== null) return false;
    const ok = this.sim.command({ do: 'land:place', units: list.map((p) => Number(p[0])), places: list.map((p) => p.slice(1)) } as unknown as Command, this.me!);
    if (ok) this.replace();
    return ok;
  }

  /**
   * Draws his blocks up in the setup step (formation.ts; Brian, 26 November 2026): those named, or with none named
   * every block of his that may be placed, in a line, two lines, a wedge or a column about the middle of where they
   * stand, facing the enemy, guns and support teams behind. Where a place of it is barred (water, a block that stays, a
   * fort), the whole formation is tried a little to every side, the nearest first, and stands on the first ground that
   * takes all of it: never outside his own ground. Says where each was placed and how far off the place worked out
   * (off, metres), and those named that stay where they are (left: dug in, each with its name); or why none was:
   * 'begun', 'nothing' (none of his to draw up), 'stay' (every one named stays where it is: id, who and why the
   * first's), 'room' (his ground cannot hold them so).
   */
  drawUp(kind: Formation, ids: readonly number[] = []): DrawnUp {
    const none = (no: DrawnUp['none'], why: string, id?: number): DrawnUp => ({ ok: false, why, none: no, ...(id !== undefined && id >= 0 ? { id, who: this.whoIs(id) } : {}), placed: [], left: [], off: 0 });
    const room = this.sim.setupRoom();
    if (!this.me || !this.sim.setup || !room) return none('begun', 'Not now: the battle has begun.');
    const me = this.me, mine = this.sim.units.filter((u) => u.side === me);
    const named = ids.length ? mine.filter((u) => ids.includes(u.id)) : mine;
    const us = named.filter((u) => this.sim.placeable(u) === null);
    // (those he named that stay: said by name; with none named, his army's dug-in blocks are simply no part of it)
    const left = ids.length ? named.filter((u) => !us.includes(u)).map((u) => ({ id: u.id, who: this.sim.named(u), why: this.sim.placeable(u)! })) : [];
    if (!us.length) return left.length ? { ...none('stay', left[0].why, left[0].id), left } : none('nothing', 'Nothing to draw up.');
    const dir = me === 'att' ? 1 : -1, face = me === 'att' ? 0 : Math.PI;
    const slots = drawnUp(kind, us.map((u) => ({ id: u.id, x: u.x, y: u.y, w: u.w, d: u.d, rear: u.kind === 'art' })), dir, room);
    if (!slots) return none('room', 'Your ground has no room to draw them up so.');
    const order = slots.map((s) => us.find((u) => u.id === s.id)!);
    // The formation as worked out, then a little to every side of it, the nearest first.
    const shifts: [number, number][] = [];
    for (let dx = -DRAW.reach; dx <= DRAW.reach; dx += DRAW.step) for (let dy = -DRAW.reach; dy <= DRAW.reach; dy += DRAW.step) shifts.push([dx, dy]);
    shifts.sort((a, b) => Math.hypot(a[0], a[1]) - Math.hypot(b[0], b[1]) || a[1] - b[1] || a[0] - b[0]);
    for (const [dx, dy] of shifts) {
      const at = slots.map((s) => [s.x + dx, s.y + dy, face]);
      if (this.sim.placeManyWhy(order, at)) continue;
      const list = slots.map((s, i) => [s.id, at[i][0], at[i][1], face]);
      return this.placeMany(list) ? { ok: true, why: '', placed: list, left, off: Math.hypot(dx, dy) } : none('begun', 'Not now: the battle has begun.');
    }
    return none('room', 'Your ground has no room to draw them up so.');
  }

  /** A block's name as the player reads it ('' for none). */
  private whoIs(id: number): string {
    const u = this.sim.units.find((v) => v.id === id);
    return u ? this.sim.named(u) : '';
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
    this.keeper = new LiveKeeper(() => this.drill.woundsOn);
    this.phases.clear(); this.phaseNew.clear();
    this.lastView = -1;
    this.placed = true;
    this.look();
  }
}

/**
 * Opens a campaign battle live from its ticket (the campaign's battleLook): no set piece, so no piece's sky, models or
 * army kit; the scene dresses the field by the ticket's place. A ticket that is a set piece's own may name its piece.
 */
export function liveTicket(ticket: FieldTicket<LandPlace>, opts: LiveOpts = {}, pieceKey?: string): LiveBattle {
  const p = pieceKey ? PIECES.find((x) => x.key === pieceKey) ?? null : null;
  return new LiveBattle(ticket, p, opts);
}

/** Opens a set piece live: the piece by key, its first seed unless one is given. */
export function livePiece(key: string, seed?: number, opts: LiveOpts = {}): LiveBattle {
  const p = PIECES.find((x) => x.key === key);
  if (!p) throw new Error(`no set piece ${key}`);
  const s = seed ?? p.seeds[0];
  return new LiveBattle(ticketOf(p, s, opts.me ?? null), p, opts);
}
