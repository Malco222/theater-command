// A tank's machine guns, seen firing (release 2; Brian, 25 November 2026, 15:55Z: "i never see the machine guns on the
// tanks firing"; 15:56Z: "i dont want it to be all the time as thats unrealistic with ammo"; 15:57Z: "i dont want the
// tanks and vehicles doing the exact same thing as each other. i want everything to see like it has its own mind").
// Looks only (ruling 217): Drill.tankmg gives each tank of an armoured block her own machine-gun mind (drill.ts mgThink
// and mgFire, DRILL.tankmg). These tests hold that it changes nothing in the fight; that off is off; that with it on the
// guns are seen firing as the game ships, but not all the time; that each tank fires at her own moment, at targets
// worth the rounds, from the guns her model carries, and eases off as her rounds run down; that nothing but a tank of an
// armoured block fires one; and that the hatch rule and the drill book's own path stand beside it.
//
// Words used. Burst: one 'mg' round from a tank (the 3D game draws it as a burst); the burst that takes a man the rules
// took (a kill burst) is counted apart. Spell: a tank's bursts with no gap over 8 s. Firing share: the spells' lengths,
// each and 4 s, over all the time the tanks stood. Together: the share of bursts with another tank of the same block
// bursting within half a second. Her mark: the man she holds in her sights (drill.ts Man.mgOn); a hop: her mark
// becoming another man while she holds one. The numbers in the comments are what the meter read when this was written
// (tools/squad-life/tankmg.ts; Prokhorovka and Normandy, seeds 11 and 7, as the game starts them).
import { describe, expect, it } from 'vitest';
import { LandBattle, sampleTicket, withTeams } from '../src/battle/land';
import { livePiece, liveTicket, type LiveOpts } from '../src/battle/land/live';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import { DRILL, type Drill } from '../src/battle/land/drill';
import { ARMOUR } from '../src/battle/land/armour';
import type { FieldTicket } from '../src/battle/core/types';

interface M { k: number; n: number; j?: number; role: string; pose: string; x: number; y: number; face: number; fell?: number; aim?: [number, number]; buttoned?: boolean; cmdDown?: number; hidden?: boolean; cover: string; went?: number; ml?: string; coax?: true; mgLeft?: number; mgRoof?: number; mgG?: string; mgT?: number; mgAt?: [number, number]; mgOn?: [number, number, number]; mgNext?: number; mgRun?: number }
interface U { id: number; side: string; kind: string; nat: string; team?: string; figures: number; engaged?: number; dug: boolean; hid?: boolean }
interface Sq { u: U; esc?: string; of?: number; men: M[]; veh: M[] }
/** One burst, as it stood the moment it was fired (read inside the drill's round()). */
interface Burst {
  t: number; by: number; k: number; model: string; g: string; range: number; cls: 'threat' | 'open' | 'cover' | 'rattle' | 'none'; role: string; kind: string;
  /** Degrees from her bow to what she fired at, and from where her turret was laid (none: no aim). */
  hull: number; coax: number | null;
  /** Her commander down in his hatch as she fired, and (cmd) struck there before. */
  shut: boolean; cmd: boolean;
  /** Her stock before this burst: under `low`, at her last tenth (lastBefore: drill.ts mgTenth). `own`: fired by her own mind this step (else the drill book's burst). `byBook`: the book's gate (coax) was open on her. */
  lowBefore: boolean; lastBefore: boolean; own: boolean; byBook: boolean;
  /** The round was fired at the very man her mark names. */
  hers: boolean;
  /** Degrees from what she fired at to where her aim stood as the step before ended (none: no aim then), and to a turret that had followed her aim all along at `turn` degrees a battle second. */
  aimWas: number | null; tur: number;
}
interface Tally {
  t: number; sig: string; tanks: number; tankSecs: number; bursts: Burst[];
  /** Kill bursts: the burst that took a man the rules took, with its gun, and (off) the degrees he stood off that gun's line as the step ended. */
  kills: { t: number; by: number; k: number; g?: string; off: number }[];
  /** Every 'mg' round whose shooter is a vehicle or a gun that is no tank of an armoured block (must stay empty). */
  wrong: string[];
  /** Each tank ('block:k'): her model, her own pace (as the drill draws it), her stock and what is left of it (her roof gun's own belts apart, where it has them), the least any of her belts ever held, and when she first fell under `low`. */
  stock: Map<string, { model: string; pace: number; stock: number; left: number; roofStock: number; roofLeft: number; min: number; lowAt?: number; floor: number; roofFloor: number; tenthAt?: number }>;
  /** The rest her guns took after each spell's last burst of her own mind (her next burst due less the time she fired it): at her last tenth, with her rounds low, or neither (as they stood before that burst). */
  pauses: { tenth: number[]; low: number[]; full: number[] };
  /** Her own bursts that left a spell running (more bursts due): fired from her last tenth (must stay 0), and in all. */
  runOn: { tenth: number; all: number };
  /** Roof bursts from a tank the view shows closed up after the step (as tests/land-hatches counts them). */
  roofShutAfter: number;
  /** A digest of every round and of the men's view each second, to tell two drawn battles apart. */
  drawn: string;
  /** Marks held after a think on a man a round could not be fired at (must stay 0), and marks not standing where their man stands (likewise). */
  unfit: number;
  /** Bursts that were due and not fired because her man was gone or not worth the rounds that instant: all of them, and the most in a row for one tank with no burst between. */
  misses: number; starve: number;
  /** Hops: her mark becoming another man while she holds one. */
  hops: number;
  /** A turret thrown to and fro: swings of a tank's aim over 15 degrees answered within 3 s by a swing back to within 8 degrees of where it was. */
  fro: number;
}

const T = DRILL.tankmg, deg = 180 / Math.PI;
const NONE = (): Omit<Tally, 'stock'> => ({ t: 0, sig: '', tanks: 0, tankSecs: 0, bursts: [], kills: [], wrong: [], roofShutAfter: 0, drawn: '', unfit: 0, misses: 0, starve: 0, hops: 0, fro: 0, pauses: { tenth: [], low: [], full: [] }, runOn: { tenth: 0, all: 0 } });
/** The rounds of a stock she keeps, its last tenth (drill.ts mgKept), and whether her belts stand at it (mgTenth). */
const kept = (stock: number): number => Math.ceil(stock * T.last - 1e-9);
const atTenth = (R: { stock: number; roofStock?: number }, m: { mgLeft?: number; mgRoof?: number }): boolean => (R.stock > 0 ? (m.mgLeft ?? R.stock) <= kept(R.stock) : R.roofStock ? (m.mgRoof ?? R.roofStock) <= kept(R.roofStock) : false);
const wrap = (a: number): number => Math.atan2(Math.sin(a), Math.cos(a));
const brg = (x0: number, y0: number, x1: number, y1: number): number => Math.atan2(x1 - x0, y1 - y0);
const FIRING = new Set(['fire', 'fire_stand', 'fire_kneel', 'fire_prone', 'lmg_burst']);
const sigOf = (b: LandBattle): string => JSON.stringify({ over: b.over, t: b.t, units: b.units.map((u) => [u.id, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged]) });
const hash = (h: number, s: string): number => { for (let i = 0; i < s.length; i++) h = (Math.imul(h, 31) + s.charCodeAt(i)) | 0; return h; };

/** Fights a battle to its end, reading the tanks' machine guns as the drill fires them. */
function read(b: LandBattle, drill: Drill, drawn?: Map<number, string>): Tally {
  const d = drill as unknown as { squads: Map<number, Sq>; fired: number; shots: { g?: string }[]; round: (sq: Sq, m: M, w: string, aim: [number, number]) => void; blockAt: (side: string, p: [number, number]) => U | undefined; standing: (sq: Sq) => M[]; mgIn: (r: [number, number], ...ns: number[]) => number; mgThink: (sq: Sq) => void; mgFire: (sq: Sq, m: M) => void; mgMan: (sq: Sq, m: M, f: number) => unknown };
  const out: Tally = { ...NONE(), stock: new Map() };
  const rowOf = (sq: Sq, m: M) => { const ml = m.ml ?? drawn?.get(sq.u.id) ?? b.vehicle(sq.u as never) ?? '_'; return { ml, R: T.models[ml] ?? T.models._ }; };
  // Each tank as the steps go by: her aim as the last step ended, a turret following it, her mark's man, her aim's swings.
  const seen = new Map<M, { aim?: [number, number]; tur: number; on?: string; log: [number, number][]; run: number }>();
  const of = (m: M) => seen.get(m) ?? seen.set(m, { tur: m.face, log: [], run: 0 }).get(m)!;
  const round0 = d.round;
  d.round = function (this: unknown, sq: Sq, m: M, w: string, aim: [number, number]): void {
    // (a machine-gun round from a vehicle that is no tank of an armoured block: an escort's, a carrier, a lorry, a gun)
    const tank = m.role === 'tank' && sq.u.kind === 'arm' && !sq.esc && sq.of === undefined;
    if (w === 'mg' && !tank && (m.role === 'tank' || m.role === 'gun' || m.role === 'lorry')) out.wrong.push(`${sq.u.id}:${m.k} ${m.role} ${m.ml ?? ''} of ${sq.esc ?? sq.u.kind}`);
    if (w !== 'mg' || m.role !== 'tank') return round0.call(this, sq, m, w, aim);
    const u = sq.u, { ml, R } = rowOf(sq, m);
    const on = d.blockAt.call(d, u.side, aim), tq = on ? d.squads.get(on.id) : undefined;
    let v: M | undefined, dd = Infinity;
    for (const x of tq ? d.standing.call(d, tq) : []) { const q = Math.hypot(x.x - aim[0], x.y - aim[1]); if (q < dd) { dd = q; v = x; } }
    // (her own burst this step: its rounds are already off her stock, so her belts are read as they stood before it, in
    // mgFire below; else it is the drill book's burst)
    const own = m.mgT !== undefined && Math.abs(m.mgT - b.t) < 1e-6, was = own && pre ? pre : m;
    const before = R.stock > 0 ? (was.mgLeft ?? R.stock) / R.stock : R.roofStock ? (was.mgRoof ?? R.roofStock) / R.roofStock : 0, tenth = atTenth(R, was);
    const range = v ? Math.hypot(v.x - m.x, v.y - m.y) : -1, k = before < T.low ? 0.5 : 1;
    // What she fired at, weighed here by the rule as it is written down (not by the drill's own code).
    let cls: Burst['cls'] = 'none';
    if (v && on && !v.hidden && v.pose !== 'mounted' && !on.hid && (u.engaged !== undefined || range < T.reach.near)) {
      if (v.role === 'tank') cls = !v.buttoned && v.cmdDown === undefined && range < T.reach.rattle && before >= T.low ? 'rattle' : 'none';
      else if (v.role !== 'gun' && v.role !== 'lorry') {
        const threat = (v.role === 'at' && range < T.reach.at * k) || ((v.role === 'mg' || v.role === 'hmg') && FIRING.has(v.pose) && range < T.reach.mg * k);
        const open = (v.went ?? 0) > 0.5 || (!on.dug && (v.cover === 'none' || v.cover === 'field'));
        cls = tenth ? (threat && range < T.lastReach ? 'threat' : 'none') : threat ? 'threat' : open && range < T.reach.open * k ? 'open' : range < T.reach.cover * k ? 'cover' : 'none';
      }
    }
    const s = of(m), bv = v ? brg(m.x, m.y, v.x, v.y) : 0;
    const burst: Burst = {
      t: Math.round(b.t * 10) / 10, by: u.id, k: m.k, model: ml, g: '?', range, cls, role: v?.role ?? 'none', kind: on?.kind ?? 'none',
      hull: v ? Math.abs(wrap(bv - m.face)) * deg : 999, coax: v && m.aim ? Math.abs(wrap(bv - brg(m.x, m.y, m.aim[0], m.aim[1]))) * deg : null,
      shut: !!m.buttoned || m.cmdDown !== undefined, cmd: m.cmdDown !== undefined, lowBefore: before < T.low, lastBefore: tenth, own, byBook: !!m.coax,
      hers: !!v && !!on && !!m.mgOn && m.mgOn[0] === on.id && m.mgOn[1] === v.k && m.mgOn[2] === (v.j ?? -1),
      aimWas: v && s.aim ? Math.abs(wrap(bv - brg(m.x, m.y, s.aim[0], s.aim[1]))) * deg : null, tur: v ? Math.abs(wrap(bv - s.tur)) * deg : 999,
    };
    const n0 = d.fired;
    round0.call(this, sq, m, w, aim);
    if (d.fired !== n0) { burst.g = d.shots[d.shots.length - 1].g ?? '?'; out.bursts.push(burst); }
  };
  // After each think of a tank block: every mark is a man standing, whom a round at his place would be fired at.
  const think0 = d.mgThink;
  d.mgThink = function (this: unknown, sq: Sq): void {
    think0.call(this, sq);
    for (const m of sq.men) {
      if (!m.mgAt) continue;
      if (!m.mgOn) { out.unfit++; continue; }
      const [id, k, j] = m.mgOn, tq = d.squads.get(id), x = tq?.men.find((y) => y.k === k && (y.j ?? -1) === j);
      if (!tq || !x || x.fell !== undefined || tq.u.hid || d.blockAt.call(d, sq.u.side, [x.x, x.y])?.id !== id || x.x !== m.mgAt[0] || x.y !== m.mgAt[1]) out.unfit++;
    }
  };
  // A burst that was due and not fired because her man was not there to be fired at (the drill's own finding of him).
  let noMan = false, firing = false, pre: { mgLeft?: number; mgRoof?: number } | undefined;
  const man0 = d.mgMan, fire0 = d.mgFire;
  d.mgMan = function (this: unknown, sq: Sq, m: M, f: number): unknown { const a = man0.call(this, sq, m, f); if (firing && !a) noMan = true; return a; };
  d.mgFire = function (this: unknown, sq: Sq, m: M): void {
    const n0 = d.fired, s = of(m), R = rowOf(sq, m).R, tenth = atTenth(R, m);
    noMan = false; firing = true; pre = { mgLeft: m.mgLeft, mgRoof: m.mgRoof }; fire0.call(this, sq, m); firing = false; pre = undefined;
    if (d.fired !== n0) {
      s.run = 0;
      // (her own burst: the rest her guns take after it where it ended her spell, by her belts as it left them; else a
      // spell running on)
      const now = atTenth(R, m), g = R.stock > 0 ? m.mgLeft! / R.stock : R.roofStock ? m.mgRoof! / R.roofStock : 0;
      if (m.mgRun) { out.runOn.all++; if (tenth || now) out.runOn.tenth++; }
      else out.pauses[now ? 'tenth' : g < T.low ? 'low' : 'full'].push(m.mgNext! - b.t);
    }
    else if (noMan) { out.misses++; out.starve = Math.max(out.starve, ++s.run); }
  };
  let last = 0, h = 7;
  for (let step = 0; !b.over && step < 30000; step++) {
    b.step();
    for (const sq of d.squads.values()) {
      if (sq.u.kind !== 'arm' || sq.esc || sq.of !== undefined) continue;
      for (const m of sq.men) {
        if (m.role !== 'tank') continue;
        const key = `${sq.u.id}:${m.k}`, { ml, R } = rowOf(sq, m);
        let s = out.stock.get(key);
        if (!s) out.stock.set(key, (s = { model: ml, pace: d.mgIn.call(d, T.pace, sq.u.id, m.n, 211), stock: R.stock, left: R.stock, roofStock: R.roofStock ?? 0, roofLeft: R.roofStock ?? 0, min: 0, floor: R.stock, roofFloor: R.roofStock ?? 0 }));
        if (m.mgLeft !== undefined) {
          s.left = m.mgLeft; s.roofLeft = m.mgRoof ?? 0; s.min = Math.min(s.min, m.mgLeft, m.mgRoof ?? 0);
          // (the least each of her belts ever held, step by step, and when she first stood at her last tenth)
          s.floor = Math.min(s.floor, m.mgLeft); s.roofFloor = Math.min(s.roofFloor, m.mgRoof ?? 0);
          if (s.tenthAt === undefined && atTenth(R, m)) s.tenthAt = Math.round(b.t * 10) / 10;
          const f = R.stock > 0 ? m.mgLeft / R.stock : R.roofStock ? (m.mgRoof ?? 0) / R.roofStock : 0;
          if (f < T.low && s.lowAt === undefined) s.lowAt = Math.round(b.t * 10) / 10;
        }
        if (m.fell !== undefined || m.k >= sq.u.figures) continue;
        out.tankSecs += 0.1;
        // Her mark's man, her aim (each change of it, by its bearing) and a turret that follows her aim.
        const z = of(m), on = m.mgAt && m.mgOn ? m.mgOn.join(':') : undefined, a = m.aim, was = z.aim;
        if (on && z.on && on !== z.on) out.hops++;
        z.on = on;
        if ((a === undefined) !== (was === undefined) || (a && was && (a[0] !== was[0] || a[1] !== was[1]))) z.log.push([b.t, a ? brg(m.x, m.y, a[0], a[1]) * deg : NaN]);
        z.aim = a ? [a[0], a[1]] : undefined;
        const want = a ? brg(m.x, m.y, a[0], a[1]) : m.face, most = (T.turn * 0.1) / deg;
        z.tur = wrap(z.tur + Math.max(-most, Math.min(most, wrap(want - z.tur))));
      }
    }
    for (const r of drill.rounds()) {
      if (r.n <= last) continue;
      last = r.n;
      h = hash(h, JSON.stringify(r));
      if (r.w !== 'mg' || r.by === undefined || r.k === undefined || (!r.g && r.r !== 'kill')) continue;
      // A round that names a tank's gun, and the burst that took a man: its shooter is a model of the block's own squad
      // (what rides with a block, its escorts and its armoured infantry, are squads of their own: read in round() above).
      const sq = d.squads.get(r.by), m = sq?.men.find((x) => x.k === r.k && (x as { j?: number }).j === r.j);
      const tank = !!sq && !!m && m.role === 'tank' && sq.u.kind === 'arm' && !sq.esc && sq.of === undefined;
      if (r.g && !tank) out.wrong.push(`${r.by}:${r.k} names a tank's gun and is no tank`);
      if (!m || !tank) continue;
      if (r.r === 'kill') {
        const br = brg(m.x, m.y, r.x, r.y);
        const off = r.g === 'hull' ? Math.abs(wrap(br - m.face)) * deg : r.g === 'coax' && m.aim ? Math.abs(wrap(br - brg(m.x, m.y, m.aim[0], m.aim[1]))) * deg : 999;
        out.kills.push({ t: r.t, by: r.by, k: r.k, off, ...(r.g ? { g: r.g } : {}) });
      }
      if (r.g === 'roof' && m.buttoned) out.roofShutAfter++;
    }
    if (step % 10 === 0) h = hash(h, JSON.stringify(drill.view()));
  }
  d.round = round0; d.mgThink = think0; d.mgMan = man0; d.mgFire = fire0;
  for (const z of seen.values()) {
    const A = z.log, off = (i: number, j: number): number => Math.abs(wrap((A[i][1] - A[j][1]) / deg)) * deg;
    for (let i = 1; i < A.length; i++) {
      if (Number.isNaN(A[i][1]) || Number.isNaN(A[i - 1][1]) || off(i, i - 1) <= 15) continue;
      for (let j = i + 1; j < A.length && A[j][0] - A[i][0] <= 3; j++) if (!Number.isNaN(A[j][1]) && off(j, i - 1) < 8) { out.fro++; break; }
    }
  }
  out.t = b.t; out.tanks = out.stock.size; out.sig = sigOf(b); out.drawn = `${h}:${last}`;
  return out;
}

const q = (a: number[], p: number): number => (a.length ? a[Math.min(a.length - 1, Math.floor(a.length * p))] : NaN);

/** The numbers of a battle's bursts (kill bursts left out). */
function sum(y: Tally) {
  const byTank = new Map<string, Burst[]>(), byBlock = new Map<number, Burst[]>();
  for (const x of y.bursts) {
    const key = `${x.by}:${x.k}`;
    (byTank.get(key) ?? byTank.set(key, []).get(key)!).push(x);
    (byBlock.get(x.by) ?? byBlock.set(x.by, []).get(x.by)!).push(x);
  }
  let longest = 0, fireSecs = 0, most = 0, mostLow = 0;
  const gaps: number[] = [];
  for (const bs of byTank.values()) {
    let a = 0;
    for (let i = 1; i <= bs.length; i++) {
      if (i < bs.length && bs[i].t - bs[i - 1].t <= 8) { gaps.push(bs[i].t - bs[i - 1].t); continue; }
      longest = Math.max(longest, bs[i - 1].t - bs[a].t); fireSecs += bs[i - 1].t - bs[a].t + 4; most = Math.max(most, i - a);
      // (a spell begun with her rounds already low)
      if (bs[a].lowBefore) mostLow = Math.max(mostLow, i - a);
      a = i;
    }
  }
  let together = 0, firstSpan = Infinity, firstApart = Infinity, starts = 0, echo = 0, stairs = 0;
  for (const list of byBlock.values()) {
    for (const a of list) if (list.some((c) => c.k !== a.k && Math.abs(c.t - a.t) <= 0.5 + 1e-9)) together++;
    // A tank's spell starts (a burst after 8 s of her own silence): those that come exactly `apart` after the block's
    // burst before, another tank's (an echo), and the rows of three or more tanks opening exactly `apart` apart (stairs).
    const mine = new Map<number, number>(), st: { t: number; k: number }[] = [];
    let before: Burst | undefined;
    for (const x of list) {
      const t0 = mine.get(x.k);
      if (t0 === undefined || x.t - t0 > 8) { starts++; st.push(x); if (before && before.k !== x.k && Math.abs(x.t - before.t - T.apart) < 0.051) echo++; }
      mine.set(x.k, x.t); before = x;
    }
    for (let i = 0; i < st.length; i++) {
      let j = i;
      while (j + 1 < st.length && st[j + 1].k !== st[j].k && Math.abs(st[j + 1].t - st[j].t - T.apart) < 0.051) j++;
      if (j - i >= 2) stairs++;
      i = j;
    }
    const first = new Map<number, number>();
    for (const a of list) if (!first.has(a.k)) first.set(a.k, a.t);
    const ts = [...first.values()].sort((a, c) => a - c);
    for (let i = 1; i < ts.length; i++) firstApart = Math.min(firstApart, ts[i] - ts[i - 1]);
    if (ts.length >= 3) firstSpan = Math.min(firstSpan, ts[ts.length - 1] - ts[0]);
  }
  gaps.sort((a, c) => a - c);
  const n = y.bursts.length, stock = [...y.stock.values()], low = stock.filter((s) => s.lowAt !== undefined).sort((a, c) => a.lowAt! - c.lowAt!);
  return {
    n, firing: byTank.size, perTankMin: n / Math.max(1e-9, y.tankSecs / 60), firingShare: fireSecs / Math.max(1e-9, y.tankSecs), longest, most, mostLow,
    gap: q(gaps, 0.5), together: together / Math.max(1, n), firstSpan, firstApart, starts, echo, stairs, hopsPerTankMin: y.hops / Math.max(1e-9, y.tankSecs / 60), low, guns: (g: string, model?: string): number => y.bursts.filter((x) => x.g === g && (!model || x.model === model)).length,
  };
}

// ---- The battles, each fought once ------------------------------------------------------------------------------

const runs = new Map<string, Tally>();
/** A set piece as the game starts it (livePiece, platoons off), with the options added. */
function piece(key: 'prokhorovka-1943' | 'normandy-1944', seed: number, opts: Record<string, unknown> = {}): Tally {
  const id = `${key}:${seed}:${JSON.stringify(opts)}`;
  let y = runs.get(id);
  if (!y) { const L = livePiece(key, seed, { me: null, platoons: false, ...opts } as LiveOpts); runs.set(id, (y = read(L.sim, L.drill))); }
  return y;
}
const LIFE = { book: true, life: true };
const FOUR: ['prokhorovka-1943' | 'normandy-1944', number][] = [['prokhorovka-1943', 11], ['prokhorovka-1943', 7], ['normandy-1944', 11], ['normandy-1944', 7]];

/**
 * A practice battle (tanks in the attack), June 1944 where `late` (the armies' later tanks), drawn as asked; no drill
 * where `draw` is null. `before` is done to the drill before the first step (a test's own setting of a tank's belts).
 */
function practice(att: string, draw: Record<string, unknown> | null, late = false, seed = 5, def = 'GER', before?: { name: string; fn: (d: Drill) => void }): Tally {
  const id = `practice:${att}:${def}:${JSON.stringify(draw)}:${late}:${seed}:${before?.name ?? ''}`;
  let y = runs.get(id);
  if (y) return y;
  const t = withTeams(sampleTicket('plains', 1.3, { seed, nat: { att, def }, style: { att: 'methodical', def: 'bold' } }));
  if (late) t.turn = 58; // June 1944
  const b = new LandBattle(t);
  if (!draw) { for (let k = 0; !b.over && k < 30000; k++) b.step(); y = { ...NONE(), t: b.t, sig: sigOf(b), stock: new Map() }; }
  else { const d = b.drillOn(draw); before?.fn(d); y = read(b, d); }
  runs.set(id, y);
  return y;
}
/** The two early-war battles (September 1939: every tank a light tank, whose row gives nine drawn bursts) measured for the last tenth. */
const EARLY: [string, string, number][] = [['USA', 'GER', 5], ['SOV', 'ITA', 3]];
const early = (att: string, def: string, seed: number, before?: { name: string; fn: (d: Drill) => void }): Tally => practice(att, { sections: true, tankmg: true }, false, seed, def, before);

/** A campaign battle on a set piece's ticket, each corps carrying the kit given, with every vehicle switch on. */
function campaign(key: string, kit: Record<string, number>, opts: Record<string, unknown>): { y: Tally; squads: Sq[] } {
  const t = ticketOf(PIECES.find((x) => x.key === key)!, 11, 'att');
  const with_ = (side: typeof t.att) => ({ ...side, forces: side.forces.map((f) => ({ ...f, kit: { ...f.kit, ...kit } })) });
  const L = liveTicket({ ...t, att: with_(t.att), def: with_(t.def) } as unknown as FieldTicket<never>, { me: null, platoons: false, ...opts } as LiveOpts);
  const y = read(L.sim, L.drill);
  return { y, squads: [...(L.drill as unknown as { squads: Map<number, Sq> }).squads.values()] };
}

/**
 * A campaign battle on a set piece's ticket with nothing added: no set piece names the models, so the 3D game is told
 * to draw each tank block as the nearest model it has (the header's `model`), which is not always the model the rules
 * weigh her as. The live layer tells the drill each block's drawn model as it settles it (live.ts); the drill is told
 * it here again, from the header, as any other starter of a 3D battle would tell it (a second telling changes nothing).
 */
function campaignDrawn(key: string): { y: Tally; drawn: Map<number, string>; rules: Map<number, string | null> } {
  const t = ticketOf(PIECES.find((x) => x.key === key)!, 11, 'att');
  const L = liveTicket(t as unknown as FieldTicket<never>, { me: null, platoons: false, tankmg: true } as LiveOpts);
  const drawn = new Map<number, string>(), rules = new Map<number, string | null>();
  for (const blk of L.header().blocks as { id: number; kind: string; model: string | null }[]) {
    if (blk.kind !== 'arm' || !blk.model) continue;
    drawn.set(blk.id, blk.model);
    const u = L.sim.allBlocks().find((x) => x.id === blk.id);
    rules.set(blk.id, u ? L.sim.vehicle(u) : null);
    L.drill.drawnAs(blk.id, blk.model);
  }
  return { y: read(L.sim, L.drill, drawn), drawn, rules };
}

describe('a tank\'s machine guns, seen firing (looks only)', () => {
  it('off is off: nothing is drawn differently with the switch false as with it absent', () => {
    // Every round the drill books and the men's view each second, to the battle's end (and so the end itself).
    const absent = practice('USA', { sections: true }), off = practice('USA', { sections: true, tankmg: false });
    expect(off.drawn).toBe(absent.drawn);
    expect(off.sig).toBe(absent.sig);
    for (const key of ['prokhorovka-1943', 'normandy-1944'] as const) expect(piece(key, 11, { tankmg: false }).drawn).toBe(piece(key, 11).drawn);
  }, 300_000);

  it('changes nothing in the fight, as the game ships: all four battles end the same with it on, and without it no tank fires one', () => {
    for (const [key, seed] of FOUR) expect(piece(key, seed, { tankmg: true }).sig).toBe(piece(key, seed).sig);
    // As shipped, a tank's bursts hang on the drill book, which is off: none (today 0 on all four battles).
    for (const [key, seed] of FOUR) expect(piece(key, seed).bursts.length).toBe(0);
  }, 300_000);

  it('changes nothing in the fight with the drill book and squad life on as well', () => {
    // (the book and life themselves change nothing, so it is the shipped end each must come to)
    for (const [key, seed] of FOUR) expect(piece(key, seed, { ...LIFE, tankmg: true }).sig).toBe(piece(key, seed).sig);
    for (const [key, seed] of [FOUR[0], FOUR[2]]) expect(piece(key, seed, LIFE).sig).toBe(piece(key, seed).sig);
  }, 300_000);

  it('changes nothing in the fight against no men drawn at all: as sections, with and without squad life', () => {
    const none = practice('USA', null);
    expect(practice('USA', { sections: true }).sig).toBe(none.sig);
    for (const draw of [{ sections: true, tankmg: true }, { sections: true, life: true, tankmg: true }]) {
      const y = practice('USA', draw);
      expect(y.sig).toBe(none.sig);
      expect(y.bursts.length).toBeGreaterThan(0);
    }
  }, 300_000);

  it('changes nothing in the fight against no men drawn at all: as platoons, with and without the drill book', () => {
    const none = practice('USA', null);
    for (const draw of [{ sections: true, platoons: true, tankmg: true }, { sections: true, platoons: true, book: false, tankmg: true }]) {
      const y = practice('USA', draw);
      expect(y.sig).toBe(none.sig);
      expect(y.bursts.length).toBeGreaterThan(0);
    }
  }, 300_000);

  it('the guns are seen firing as the game ships, from nearly every tank, each round naming its gun', () => {
    const p = piece('prokhorovka-1943', 11, { tankmg: true }), n = piece('normandy-1944', 11, { tankmg: true });
    // Bursts in the battle (today 314 and 172; shipped without the switch 0).
    expect(p.bursts.length).toBeGreaterThanOrEqual(250);
    expect(n.bursts.length).toBeGreaterThanOrEqual(120);
    // Tanks that fire at least once (today 35 of 40 and 15 of 16; the drill book's path 16 of 40 and 8 of 16).
    expect(sum(p).firing).toBeGreaterThanOrEqual(30);
    expect(sum(n).firing).toBeGreaterThanOrEqual(12);
    for (const [key, seed] of FOUR) {
      const y = piece(key, seed, { tankmg: true });
      expect(y.bursts.length).toBeGreaterThan(100);
      for (const x of y.bursts) { expect(['coax', 'hull', 'roof']).toContain(x.g); expect(x.own).toBe(true); }
    }
  }, 300_000);

  it('not all the time: short spells with pauses between, a small share of a tank\'s time', () => {
    for (const [key, seed] of FOUR) {
      const s = sum(piece(key, seed, { tankmg: true }));
      // Bursts a tank a minute (today 1.7 to 3.2), far from the drill book's one burst every 3.7 s for as long as its gate is open.
      expect(s.perTankMin).toBeGreaterThan(1.2);
      expect(s.perTankMin).toBeLessThan(4.5);
      // The share of her time she is firing (today 0.11 to 0.20).
      expect(s.firingShare).toBeGreaterThan(0.07);
      expect(s.firingShare).toBeLessThan(0.3);
      // The longest spell (today 14 to 16.6 s; the drill book's runs 105 to 167 s unbroken), and the most bursts in one
      // (today 4). By this file's measure of a spell, four bursts are never over three gaps of 8 s.
      expect(s.longest).toBeLessThanOrEqual((T.spell[1] - 1) * 8 + 1e-6);
      expect(s.longest).toBeLessThan(20);
      expect(s.most).toBeLessThanOrEqual(T.spell[1]);
      // The gap between bursts inside a spell, its middle value (today 3.2 to 3.7 s).
      expect(s.gap).toBeGreaterThanOrEqual(2.5);
      expect(s.gap).toBeLessThan(5.5);
    }
  }, 300_000);

  it('her own mind: a platoon never opens as one, and its tanks seldom fire in the same half second', () => {
    for (const [key, seed] of FOUR) {
      const s = sum(piece(key, seed, { tankmg: true }));
      // Bursts within half a second of a blockmate's (today 0.14 to 0.28; the drill book's path 0.53 to 0.64).
      expect(s.together).toBeLessThan(0.35);
      // In every block of three or more tanks that fire, their first bursts are spread over 2 s at least (today 3.1 to
      // 5.4 s at the least, 4.7 to 20.9 s in the middle block), and no two tanks of any block fire their first within
      // `apart` of each other.
      expect(s.firstSpan).toBeGreaterThanOrEqual(2 * T.apart - 1e-6);
      expect(s.firstApart).toBeGreaterThanOrEqual(T.apart - 1e-6);
    }
    // Each tank has her own pace: no two of a block the same, and no two bursts of hers closer than her pace allows.
    for (const [key, seed] of FOUR) {
      const y = piece(key, seed, { tankmg: true }), blocks = new Map<string, number[]>(), lastAt = new Map<string, number>();
      for (const [tank, s] of y.stock) {
        expect(s.pace).toBeGreaterThanOrEqual(T.pace[0]);
        expect(s.pace).toBeLessThanOrEqual(T.pace[1]);
        const block = tank.split(':')[0];
        (blocks.get(block) ?? blocks.set(block, []).get(block)!).push(s.pace);
      }
      for (const paces of blocks.values()) expect(new Set(paces).size).toBe(paces.length);
      for (const x of y.bursts) {
        const tank = `${x.by}:${x.k}`, before = lastAt.get(tank);
        if (before !== undefined) expect(x.t - before, `${key} ${seed} tank ${tank} t=${x.t}`).toBeGreaterThanOrEqual(T.gap[0] * y.stock.get(tank)!.pace - 0.11);
        lastAt.set(tank, x.t);
      }
    }
    // And with the book on as well the same holds (today 0.19).
    expect(sum(piece('prokhorovka-1943', 11, { ...LIFE, tankmg: true })).together).toBeLessThan(0.35);
  }, 300_000);

  it('targets worth the rounds: threats first, men in the open, men in cover only close; never a gun, a lorry or a closed-up tank', () => {
    const seen = new Set<string>();
    for (const opts of [{ tankmg: true }, { ...LIFE, tankmg: true }]) for (const [key, seed] of FOUR) {
      for (const x of piece(key, seed, opts).bursts) {
        seen.add(x.cls);
        // Worth it by the written rule, weighed as she fired: within the reach of its class from the tank herself (halved
        // with her rounds low), and until her block fights only what is near her.
        expect(x.cls, `${key} ${seed} t=${x.t} block ${x.by} tank ${x.k} at a ${x.role} ${x.range.toFixed(0)} m off`).not.toBe('none');
        expect(x.range).toBeLessThan(T.reach.open);
        expect(['gun', 'lorry', 'none']).not.toContain(x.role);
        if (x.role === 'tank') expect(x.range).toBeLessThan(T.reach.rattle);
      }
    }
    // Threats and men in the open are both fired at; a rattle on a tank's armour is the rare one.
    expect(seen.has('threat')).toBe(true);
    expect(seen.has('open')).toBe(true);
    const p = piece('prokhorovka-1943', 11, { tankmg: true });
    expect(p.bursts.filter((x) => x.cls === 'rattle').length).toBeLessThan(p.bursts.length * 0.15);
  }, 300_000);

  it('ammunition: no belt goes below nothing, the light models run low first, and a tank low on rounds eases off', () => {
    const lows: Burst[] = [];
    for (const [key, seed] of FOUR) {
      const y = piece(key, seed, { tankmg: true });
      for (const s of y.stock.values()) { expect(s.min).toBeGreaterThanOrEqual(0); expect(s.left).toBeGreaterThanOrEqual(0); }
      // Low on rounds: nothing beyond half her open reach, spells of lowSpell at most, and with her last tenth only threats close in.
      for (const x of y.bursts) {
        if (x.lowBefore) { lows.push(x); expect(x.range).toBeLessThan(T.reach.open / 2); }
        if (x.lastBefore) { expect(x.cls).toBe('threat'); expect(x.range).toBeLessThan(T.lastReach); }
      }
      expect(sum(y).mostLow).toBeLessThanOrEqual(T.lowSpell[1]);
      // Every burst is charged, the kill bursts as well: `per` times `scale` rounds off the belts of the gun that fired it,
      // down to the last tenth of them, which she keeps (the design director, 25 November 2026, 22:22Z: a burst fired
      // from her last tenth takes nothing, so a roof gun's own belts lose nothing to a burst fired once she is at hers).
      for (const [tank, s] of y.stock) {
        const R = T.models[s.model] ?? T.models._, n = R.per * T.scale, hers = y.bursts.filter((x) => `${x.by}:${x.k}` === tank);
        const roofs = R.roofStock !== undefined ? hers.filter((x) => x.g === 'roof') : [], roof = roofs.filter((x) => !x.lastBefore).length, main = hers.length - roofs.length + y.kills.filter((k) => `${k.by}:${k.k}` === tank).length;
        expect(s.stock - s.left, `${key} ${seed}: tank ${tank}, a ${s.model}`).toBe(Math.min(s.stock - kept(s.stock), n * main));
        expect(s.roofStock - s.roofLeft, `${key} ${seed}: tank ${tank}, a ${s.model}, her roof gun`).toBe(Math.min(s.roofStock - kept(s.roofStock), n * roof));
      }
    }
    const p = piece('prokhorovka-1943', 11, { tankmg: true }), low = sum(p).low;
    // Tanks that run low in Prokhorovka, seed 11 (today 12 of 40, the first at 145 s of 258; at `scale` 1 none does:
    // the least share left is then 0.81).
    expect(low.length).toBeGreaterThanOrEqual(6);
    expect(low.length).toBeLessThan(p.tanks);
    // The first to run low are the models that stow least (the assault gun's one gun, the light tank), never the Tiger.
    expect(['StuG3', 'LightTank']).toContain(low[0].model);
    expect(low.slice(0, 4).every((s) => s.stock <= 1000)).toBe(true);
    expect(low.some((s) => s.model === 'Tiger')).toBe(false);
    // And they do ease off rather than stop: bursts are still fired with rounds low.
    expect(lows.length).toBeGreaterThan(5);
  }, 300_000);

  it('every tank model in the armour data has its row, and nothing that is no tank has one', () => {
    const NOT_TANKS = ['Hummel', 'SU76', 'M7', 'Sexton', 'SelfPropelledGun', 'Hanomag', 'M3Halftrack', 'SdKfz222', 'M8', 'ArmouredCar', 'FlakHalftrack', 'M16', 'FlakTruck', 'Truck', 'Pak40', 'leFH18', 'ZiS3', 'Howitzer105', '25pdr', 'Gun75', 'AntiTankGun'];
    for (const ml of Object.keys(ARMOUR.models)) expect(ml in T.models, ml).toBe(!NOT_TANKS.includes(ml));
    expect(T.models._).toBeDefined();
    for (const [ml, R] of Object.entries(T.models)) {
      // A model with a gun has rounds for it and a burst to draw; one with none has neither.
      const guns = R.coax + R.hull + R.roof;
      expect(R.per > 0, ml).toBe(guns > 0);
      expect(R.stock + (R.roofStock ?? 0) > 0, ml).toBe(guns > 0);
      if (R.coax + R.hull > 0) expect(R.stock, ml).toBeGreaterThan(0);
    }
    // The tank destroyers the campaign may put among the tanks, and the set pieces' models.
    for (const ml of [...Object.values(DRILL.td.kit.models), DRILL.td.kit.stand, DRILL.td.kit.axisStand, ...PIECES.flatMap((p) => [...p.att.tanks, ...p.def.tanks])]) expect(ml in T.models, ml).toBe(true);
  }, 300_000);

  it('guns by her model, not her nation: only the guns in her model\'s row, whoever she fights for', () => {
    for (const [key, seed] of FOUR) for (const x of piece(key, seed, { tankmg: true }).bursts) {
      const R = T.models[x.model] ?? T.models._;
      expect(R[x.g as 'coax' | 'hull' | 'roof'], `${x.model} fired her ${x.g}`).toBe(1);
      if (x.g === 'hull') expect(x.hull).toBeLessThanOrEqual(T.hullArc + 1e-6);
      if (x.g === 'coax') { expect(x.coax).not.toBeNull(); expect(x.coax!).toBeLessThanOrEqual(T.coaxArc + 1e-6); }
      if (x.g === 'roof') expect(x.shut).toBe(false);
    }
    // Prokhorovka: German tanks on the nation list fire a roof gun only where the model has one (the Panzer IV and the
    // assault gun: today 49 roof bursts; the Tiger and every Soviet model none), and the assault gun nothing else.
    const p = sum(piece('prokhorovka-1943', 11, { tankmg: true }));
    expect(p.guns('roof', 'PanzerIV') + p.guns('roof', 'StuG3')).toBeGreaterThan(0);
    expect(p.guns('roof')).toBe(p.guns('roof', 'PanzerIV') + p.guns('roof', 'StuG3'));
    expect(p.guns('hull', 'LightTank') + p.guns('hull', 'StuG3') + p.guns('coax', 'StuG3')).toBe(0);
    // The nation swapped, the model kept. September 1939: an American and a Romanian light tank fire the same one gun
    // (the drill book's nation list would give the American a roof gun and a hull gun).
    const usa39 = practice('USA', { sections: true, tankmg: true }), rom39 = practice('ROM', { sections: true, tankmg: true });
    for (const y of [usa39, rom39]) { expect(y.bursts.length).toBeGreaterThan(0); expect(new Set(y.bursts.map((x) => `${x.model}:${x.g}`))).toEqual(new Set(['LightTank:coax'])); }
    // June 1944: a British Sherman fires her roof gun as an American one does (Britain is not on the nation list), and a
    // Soviet T-34-85 has none.
    const usa44 = practice('USA', { sections: true, tankmg: true }, true), uk44 = practice('UK', { sections: true, tankmg: true }, true), sov44 = practice('SOV', { sections: true, tankmg: true }, true);
    for (const y of [usa44, uk44]) { expect(new Set(y.bursts.map((x) => x.model))).toEqual(new Set(['Sherman'])); expect(new Set(y.bursts.map((x) => x.g))).toEqual(new Set(['coax', 'hull', 'roof'])); }
    expect(new Set(sov44.bursts.map((x) => x.model))).toEqual(new Set(['T34-85']));
    expect(sov44.bursts.some((x) => x.g === 'roof')).toBe(false);
    expect(sov44.bursts.length).toBeGreaterThan(0);
  }, 300_000);

  it('the burst that takes a man the rules gave to her machine guns comes from a tank with rounds and a gun that bears on him', () => {
    const n: number[] = [];
    for (const opts of [{ tankmg: true }, { ...LIFE, tankmg: true }]) for (const [key, seed] of FOUR) {
      const y = piece(key, seed, opts);
      n.push(y.kills.length);
      for (const k of y.kills) {
        const R = T.models[y.stock.get(`${k.by}:${k.k}`)!.model] ?? T.models._;
        // Her hull gun or her coaxial, one her model carries, and the man within its arc as the step ends.
        expect(['hull', 'coax']).toContain(k.g);
        expect(R[k.g as 'hull' | 'coax'], `${key} ${seed} t=${k.t}`).toBe(1);
        expect(k.off, `${key} ${seed} t=${k.t} her ${k.g}`).toBeLessThanOrEqual((k.g === 'hull' ? T.hullArc : T.coaxArc) + 1e-6);
        // (and no burst of her own mind in that same step)
        expect(y.bursts.some((x) => x.by === k.by && x.k === k.k && Math.abs(x.t - k.t) < 0.05)).toBe(false);
      }
    }
    // Shipped, without the switch: none (the rules took the same men either way, and they are drawn as shelled).
    for (const [key, seed] of FOUR) expect(piece(key, seed).kills.length).toBe(0);
    // Kill bursts in each battle (today 3, 1, 8 and 4; with the book and life on as well 3, 1, 9 and 4. The drill book's
    // own path, this switch off: 5, 1, 9 and 4, every one of them a burst whether or not a gun bore).
    expect(n[0]).toBeGreaterThanOrEqual(1);
    expect(n.reduce((a, c) => a + c, 0)).toBeGreaterThan(8);
  }, 300_000);

  it('armoured cars, AA vehicles, jeeps, half-tracks, lorries and self-propelled guns stay silent; a tank destroyer fires only what she carries', () => {
    const kit = { cars: 2, aa: 1, jeeps: 2, halftracks: 1, spg: 1, td: 0.5 };
    const all = { tankmg: true, cars: true, aa: true, light: true, mech: true, trucks: true, spg: true, td: true };
    for (const key of ['normandy-1944', 'prokhorovka-1943']) {
      const { y, squads } = campaign(key, kit, all);
      // The vehicles are there: escorts, carriers, self-propelled guns and tank destroyers.
      expect(squads.some((sq) => sq.esc === 'cars')).toBe(true);
      expect(squads.some((sq) => sq.esc === 'aa')).toBe(true);
      expect(squads.some((sq) => sq.of !== undefined && !sq.esc && sq.veh.some((v) => v.ml))).toBe(true);
      expect(squads.some((sq) => sq.u.kind === 'art' && sq.men.some((m) => m.role === 'gun' && m.ml))).toBe(true);
      const tds = new Set(squads.filter((sq) => sq.u.kind === 'arm' && !sq.esc && sq.of === undefined).flatMap((sq) => sq.men.filter((m) => m.ml).map((m) => m.ml!)));
      expect(tds.size).toBeGreaterThan(0);
      // No machine-gun round from any of them, and none of them holds a mark or a stock of rounds.
      expect(y.wrong).toEqual([]);
      expect(y.bursts.length).toBeGreaterThan(0);
      for (const sq of squads) for (const m of [...sq.men, ...sq.veh]) {
        const tank = m.role === 'tank' && sq.u.kind === 'arm' && !sq.esc && sq.of === undefined;
        if (!tank) { expect(m.mgLeft).toBeUndefined(); expect(m.mgT).toBeUndefined(); expect((m as { mgAt?: unknown }).mgAt).toBeUndefined(); }
      }
      // A tank destroyer among the tanks: the SU-85 has no machine gun and fires none; the M10 and the StuG only the roof gun.
      for (const x of y.bursts) {
        expect(x.model).not.toBe('SU85');
        if (x.model === 'M10' || x.model === 'StuG3' || x.model === 'Semovente') expect(x.g).toBe('roof');
      }
      expect(y.sig).toBe(campaign(key, kit, { ...all, tankmg: false }).y.sig);
    }
  }, 300_000);

  it('with hatches on as well: no roof burst from a closed tank, none after her commander is struck', () => {
    for (const [key, seed] of [['normandy-1944', 11], ['prokhorovka-1943', 11]] as const) {
      const y = piece(key, seed, { tankmg: true, hatches: true });
      expect(y.sig).toBe(piece(key, seed).sig);
      const roof = y.bursts.filter((x) => x.g === 'roof');
      // (today 25 and 46 roof bursts with hatches on; 33 and 49 with them off)
      expect(roof.length).toBeGreaterThan(5);
      expect(roof.filter((x) => x.shut || x.cmd).length).toBe(0);
      expect(y.roofShutAfter).toBe(0);
    }
    // And with hatches off: none either, on all four.
    for (const [key, seed] of FOUR) { const y = piece(key, seed, { tankmg: true }); expect(y.bursts.filter((x) => x.g === 'roof' && (x.shut || x.cmd)).length).toBe(0); expect(y.roofShutAfter).toBe(0); }
  }, 300_000);

  it('with the drill book on as well: this path takes the place of the book\'s tank bursts, and no tank fires by both', () => {
    // With the book on and this switch off, the book's own path still fires them (today 373 and 252), each by its gate.
    expect(piece('prokhorovka-1943', 11, LIFE).bursts.length).toBeGreaterThan(200);
    expect(piece('normandy-1944', 11, LIFE).bursts.length).toBeGreaterThan(150);
    expect(piece('normandy-1944', 11, LIFE).bursts.every((x) => !x.own && x.byBook)).toBe(true);
    // (and off is off beside the book as well: every round and the men's view the same with the switch false as absent)
    for (const key of ['prokhorovka-1943', 'normandy-1944'] as const) expect(piece(key, 11, { ...LIFE, tankmg: false }).drawn).toBe(piece(key, 11, LIFE).drawn);
    for (const [key, seed] of FOUR) {
      const y = piece(key, seed, { ...LIFE, tankmg: true });
      expect(y.bursts.length).toBeGreaterThan(80);
      // Every burst is her own mind's, and the book's gate is shut on her as she fires.
      expect(y.bursts.filter((x) => !x.own || x.byBook).length).toBe(0);
      const s = sum(y);
      // (the book's own path, this switch off: spells unbroken for 105 to 167 s; with it on, today 12.9 to 15.1 s: a spell
      // is four bursts at most, and where she must wait for a gun to bear its gaps stretch, by this file's measure to 8 s)
      expect(s.longest).toBeLessThanOrEqual((T.spell[1] - 1) * 8 + 1e-6);
      expect(s.most).toBeLessThanOrEqual(T.spell[1]);
    }
  }, 300_000);

  it('her mark is a man she can fire at, and her burst is fired at that very man: none thrown away again and again where two blocks overlap', () => {
    // Prokhorovka, seed 5: an armoured block stands among the enemy's foot, and a round at a man of the foot there is
    // fired at the armoured block (the drill fires at the block a point lies in, the first of two). Before the mend
    // three tanks took such a man as their mark and threw him away 44, 34 and 12 times running, silent for half a minute
    // with a firing machine gun in reach.
    for (const [key, seed] of [['prokhorovka-1943', 5], ...FOUR] as const) {
      const y = piece(key, seed, { tankmg: true });
      // After every think every mark stands where its man stands, and a round at him would be fired at him (today 0).
      expect(y.unfit, `${key} ${seed}`).toBe(0);
      expect(y.bursts.filter((x) => !x.hers).length, `${key} ${seed}`).toBe(0);
      // Bursts due and not fired for want of her man (he fell, or was not worth the rounds that instant): few, and
      // never many in a row for one tank (today 22, 15, 19, 3 and 0 in all; 3, 5, 6, 1 and 0 in a row. Before the mend
      // Prokhorovka seed 5: 107 in all, 44 in a row).
      expect(y.misses, `${key} ${seed}`).toBeLessThan(60);
      expect(y.starve, `${key} ${seed}`).toBeLessThanOrEqual(12);
    }
    // And the guns are still seen there (Prokhorovka seed 5, today 189 bursts from 30 of 40 tanks).
    const p5 = piece('prokhorovka-1943', 5, { tankmg: true });
    expect(p5.bursts.length).toBeGreaterThanOrEqual(140);
    expect(sum(p5).firing).toBeGreaterThanOrEqual(25);
    expect(p5.sig).toBe(piece('prokhorovka-1943', 5).sig);
  }, 300_000);

  it('no drum-roll: a tank held back by a blockmate\'s burst opens at a moment of her own, not exactly `apart` after it', () => {
    for (const [key, seed] of [['prokhorovka-1943', 5], ...FOUR] as const) {
      const s = sum(piece(key, seed, { tankmg: true }));
      expect(s.starts, `${key} ${seed}`).toBeGreaterThan(50);
      // Spell starts that come exactly `apart` (1.0 s) after another tank of the block fired (today 7 of 82, 1 of 133,
      // 6 of 91, 4 of 67 and 3 of 78; before the mend 32 of 93, 44 of 138, 31 of 97, 24 of 69 and 24 of 75: a third).
      expect(s.echo / s.starts, `${key} ${seed}`).toBeLessThan(0.15);
      // Rows of three or more tanks opening exactly `apart` apart, one after another (today 1, 0, 0, 0 and 0; before
      // the mend 6, 4, 6, 3 and 3).
      expect(s.stairs, `${key} ${seed}`).toBeLessThanOrEqual(2);
    }
  }, 300_000);

  it('her turret: she keeps her mark, the turret is not thrown to and fro, and no coaxial burst leaves a turret still on its way', () => {
    // (Prokhorovka, seed 17: where her mark hopped most before the mend, 7.4 times a tank-minute, and her aim swung to
    // and fro 392 times against 129 with the switch off)
    for (const [key, seed] of [['prokhorovka-1943', 17], ...FOUR] as const) {
      const y = piece(key, seed, { tankmg: true }), off = piece(key, seed), s = sum(y);
      const coax = y.bursts.filter((x) => x.g === 'coax');
      expect(coax.length, `${key} ${seed}`).toBeGreaterThan(40);
      for (const x of coax) {
        // Where her aim stood as the step before ended, the mark was already within the coaxial's arc of it (today at
        // most 5.8 degrees; before the mend up to 86: the aim was laid and the burst fired in one step) ...
        expect(x.aimWas, `${key} ${seed} t=${x.t} tank ${x.by}:${x.k}`).not.toBeNull();
        expect(x.aimWas!, `${key} ${seed} t=${x.t} tank ${x.by}:${x.k}`).toBeLessThanOrEqual(T.coaxArc + 0.5);
        // ... and a turret that had followed her aim all along at `turn` degrees a second is on the mark (today at
        // most 5.9 degrees off it; before the mend up to 86, and over 10 for 25 of 148 at Prokhorovka seed 11).
        expect(x.tur, `${key} ${seed} t=${x.t} tank ${x.by}:${x.k}`).toBeLessThanOrEqual(T.coaxArc + 3);
      }
      // Her mark becomes another man seldom (today 0.6 to 1.8 times a tank-minute; before the mend 7.4 at seed 17).
      expect(s.hopsPerTankMin, `${key} ${seed}`).toBeLessThan(3.5);
      // Her aim thrown to and fro: no more than with the switch off, give or take (today 71, 87, 27, 3 and 7 against
      // 129, 85, 40, 30 and 13 with it off; before the mend 392 and 153 on the first two).
      expect(y.fro, `${key} ${seed}`).toBeLessThanOrEqual(off.fro * 1.25 + 10);
    }
  }, 300_000);

  it('told the model a block is drawn with, her guns and her stock are that model\'s (a campaign battle\'s stand-in models)', () => {
    // A campaign battle has no set piece to name the models: the 3D game draws the nearest model it has (a third of
    // the Soviet tank blocks as light tanks, a German one in four as a Tiger and one as an assault gun), and the rules
    // never hear of it. Told so (Drill.drawnAs), the drill reads her machine guns and her rounds from the row of the
    // tank the player sees. (The live layer makes the call where it settles each block's model; this test makes it
    // again itself, from the header, and the next test holds that the live layer's call alone is enough.)
    const { y, drawn, rules } = campaignDrawn('prokhorovka-1943');
    const other = [...drawn].filter(([id, ml]) => rules.get(id) !== ml).map(([id]) => id);
    // (today four of the ten tank blocks are drawn as another model than the rules weigh: two light tanks, the Tiger
    // and the assault gun)
    expect(other.length).toBeGreaterThanOrEqual(3);
    expect(y.bursts.filter((x) => other.includes(x.by)).length).toBeGreaterThan(0);
    for (const x of y.bursts) {
      const ml = drawn.get(x.by)!, R = T.models[ml] ?? T.models._;
      expect(x.model).toBe(ml);
      expect(R[x.g as 'coax' | 'hull' | 'roof'], `block ${x.by}, drawn as a ${ml}, fired her ${x.g}`).toBe(1);
      if (x.g === 'roof') expect(x.shut).toBe(false);
    }
    // Her stock is her drawn model's: a block drawn as light tanks carries a light tank's rounds, not a T-34's.
    for (const [tank, s] of y.stock) {
      const ml = drawn.get(Number(tank.split(':')[0]))!;
      expect(s.model).toBe(ml);
      expect(s.stock).toBe((T.models[ml] ?? T.models._).stock);
    }
    expect([...drawn.values()]).toContain('LightTank');
    // Looks only: the battle ends as it does with no switch at all.
    const t = ticketOf(PIECES.find((x) => x.key === 'prokhorovka-1943')!, 11, 'att');
    const plain = liveTicket(t as unknown as FieldTicket<never>, { me: null, platoons: false } as LiveOpts);
    expect(y.sig).toBe(read(plain.sim, plain.drill).sig);
  }, 300_000);

  // ---- Her last tenth (the design director, 25 November 2026, 22:22Z: "your last tenth is accepted, as crew behaviour
  // rather than a patch: crews kept their last belts for close defence. Below a tenth she fires only at a threat close
  // by, at long pauses, and never reaches nothing"). In September 1939 every tank is a light tank, whose row gives nine
  // drawn bursts; before the ruling every one of them was at nothing, and silent, before the fight was over.

  it('her last tenth: a tank that began with rounds is never at nothing, in the two early-war battles and the four set pieces', () => {
    const all = [...EARLY.map(([a, d, s]) => [`${a} v ${d} ${s}`, early(a, d, s)] as const), ...FOUR.map(([k, s]) => [`${k} ${s}`, piece(k, s, { tankmg: true })] as const)];
    for (const [name, y] of all) for (const [tank, s] of y.stock) {
      // (step by step: the least her belts ever held, each stock apart)
      if (s.stock > 0) { expect(s.floor, `${name}: tank ${tank}, a ${s.model}`).toBeGreaterThanOrEqual(kept(s.stock)); expect(s.floor).toBeGreaterThan(0); }
      if (s.roofStock > 0) { expect(s.roofFloor, `${name}: tank ${tank}, a ${s.model}, her roof gun`).toBeGreaterThanOrEqual(kept(s.roofStock)); expect(s.roofFloor).toBeGreaterThan(0); }
      // At her tenth her belts stay there to the end: the bursts she fires from it take nothing.
      if (s.tenthAt !== undefined) expect(s.left, `${name}: tank ${tank}`).toBe(kept(s.stock));
    }
    // The early-war light tanks do reach it (today 8 of 8, at 180 to 283 s of 287, and 7 of 8, at 221 to 263 s of 299;
    // before the ruling those same tanks were then at nothing).
    for (const [a, d, s] of EARLY) {
      const y = early(a, d, s), at = [...y.stock.values()].filter((x) => x.tenthAt !== undefined);
      expect(new Set([...y.stock.values()].map((x) => x.model))).toEqual(new Set(['LightTank']));
      expect(at.length, `${a} v ${d}`).toBeGreaterThanOrEqual(5);
      expect(y.sig).toBe(practice(a, null, false, s, d).sig);
    }
  }, 300_000);

  /** Every tank of an armoured block set at her last tenth before the first step: a test's own setting, to see her there all battle long. */
  const AT_TENTH = {
    name: 'at her tenth',
    fn: (dr: Drill): void => {
      const d = dr as unknown as { squads: Map<number, Sq>; mgRow: (sq: Sq, m: M) => { stock: number; roofStock?: number } };
      for (const sq of d.squads.values()) {
        if (sq.u.kind !== 'arm' || sq.esc || sq.of !== undefined) continue;
        for (const m of sq.men) {
          if (m.role !== 'tank') continue;
          const R = d.mgRow.call(d, sq, m);
          m.mgLeft = kept(R.stock);
          if (R.roofStock !== undefined) m.mgRoof = kept(R.roofStock);
        }
      }
    },
  };

  it('from her last tenth she fires only at a threat within lastReach, one burst a spell, at pauses longer than a low tank\'s, and takes nothing from her stock', () => {
    const tenth: number[] = [], low: number[] = [];
    let n = 0;
    for (const [a, d, s] of EARLY) for (const y of [early(a, d, s), early(a, d, s, AT_TENTH)]) {
      for (const x of y.bursts.filter((z) => z.lastBefore)) {
        n++;
        expect(x.cls, `${a} v ${d} t=${x.t} tank ${x.by}:${x.k} at a ${x.role} ${x.range.toFixed(0)} m off`).toBe('threat');
        expect(x.range).toBeLessThan(T.lastReach);
      }
      // One burst a spell: no burst of hers from the tenth (nor the one that brings her to it) leaves a spell running.
      expect(y.runOn.tenth).toBe(0);
      tenth.push(...y.pauses.tenth); low.push(...y.pauses.low);
      // Her stock does not fall there.
      for (const [tank, st] of y.stock) if (st.tenthAt !== undefined) { expect(st.left, `${a} v ${d}: tank ${tank}`).toBe(kept(st.stock)); expect(st.floor).toBe(kept(st.stock)); }
    }
    for (const [key, seed] of FOUR) {
      const y = piece(key, seed, { tankmg: true });
      for (const x of y.bursts.filter((z) => z.lastBefore)) { n++; expect(x.cls).toBe('threat'); expect(x.range).toBeLessThan(T.lastReach); }
      expect(y.runOn.tenth).toBe(0);
      tenth.push(...y.pauses.tenth); low.push(...y.pauses.low);
    }
    // Set at their tenth from the first, the light tanks fire seldom but do fire (today 16 and 9 bursts, every one at a
    // threat 91 to 149 m off; from the tenth in the two battles as they are fought 2 and 1, and on the four set pieces
    // none): close defence, never at nothing.
    for (const [a, d, s] of EARLY) {
      const y = early(a, d, s, AT_TENTH);
      expect(y.bursts.length, `${a} v ${d}`).toBeGreaterThan(0);
      expect(y.bursts.every((x) => x.lastBefore)).toBe(true);
      for (const st of y.stock.values()) expect(st.left).toBe(kept(st.stock));
      expect(y.sig).toBe(practice(a, null, false, s, d).sig);
    }
    expect(n).toBeGreaterThanOrEqual(8);
    // Her pauses there: `lastPause` times her own draw, so longer than a low tank's (`lowPause` times the same draw).
    expect(T.lastPause).toBeGreaterThan(T.lowPause);
    expect(tenth.length).toBeGreaterThan(5);
    expect(low.length).toBeGreaterThan(5);
    for (const p of tenth) { expect(p).toBeGreaterThanOrEqual(T.lastPause * T.pause[0] - 1e-6); expect(p).toBeLessThanOrEqual(T.lastPause * T.pause[1] + 1e-6); }
    for (const p of low) { expect(p).toBeGreaterThanOrEqual(T.lowPause * T.pause[0] - 1e-6); expect(p).toBeLessThanOrEqual(T.lowPause * T.pause[1] + 1e-6); }
    const mid = (a: number[]): number => [...a].sort((x, y) => x - y)[Math.floor(a.length / 2)];
    expect(mid(tenth)).toBeGreaterThan(mid(low));
  }, 300_000);

  it('in the second half of the early-war fights, tanks with a threat inside lastReach fire at it', () => {
    // Bursts in the second half of the fight at a threat (an anti-tank man, a firing machine gun) within lastReach (today
    // 12 and 9, of them 2 and 1 from a tank at her last tenth; before the ruling 10 and 8, none from the tenth: those
    // tanks were at nothing).
    let fromTenth = 0;
    for (const [a, d, s] of EARLY) {
      const y = early(a, d, s), half = y.bursts.filter((x) => x.t >= y.t / 2 && x.cls === 'threat' && x.range < T.lastReach);
      expect(half.length, `${a} v ${d}`).toBeGreaterThanOrEqual(4);
      fromTenth += half.filter((x) => x.lastBefore).length;
    }
    expect(fromTenth).toBeGreaterThanOrEqual(1);
  }, 300_000);

  it('a tank above her last tenth is unchanged: on the four set pieces each tank that never reaches it bursts as she did before the ruling', () => {
    // Before the ruling, by the same build with the tenth taken away (none kept, never at it): only what a tank does at
    // or below a tenth of any of her belts differs, so every tank that never comes to one fires the very same bursts.
    for (const [key, seed] of FOUR) {
      const L = livePiece(key, seed, { me: null, platoons: false, tankmg: true } as LiveOpts), d = L.drill as unknown as { mgKept: (n: number) => number; mgTenth: () => boolean };
      d.mgKept = () => 0; d.mgTenth = () => false;
      const was = read(L.sim, L.drill), now = piece(key, seed, { tankmg: true });
      const reached = new Set<string>();
      for (const y of [was, now]) for (const [tank, s] of y.stock) if (s.tenthAt !== undefined || (s.stock > 0 && s.floor <= kept(s.stock)) || (s.roofStock > 0 && s.roofFloor <= kept(s.roofStock))) reached.add(tank);
      const line = (y: Tally, tank: string): string => [...y.bursts.filter((x) => `${x.by}:${x.k}` === tank).map((x) => `${x.t}/${x.g}/${x.range}`), ...y.kills.filter((x) => `${x.by}:${x.k}` === tank).map((x) => `kill ${x.t}/${x.g}`)].join(' ');
      let same = 0;
      for (const tank of now.stock.keys()) if (!reached.has(tank)) { expect(line(now, tank), `${key} ${seed}: tank ${tank}`).toBe(line(was, tank)); same++; }
      // (today 39 of 40, 40 of 40, 9 of 16 and 9 of 16: in Normandy the Shermans' roof guns come to the tenth of their own belts)
      expect(same, `${key} ${seed}`).toBeGreaterThanOrEqual(now.tanks / 2);
      expect(was.sig).toBe(now.sig);
    }
  }, 300_000);

  it('the live layer tells the drill each drawn model: a campaign battle started through liveTicket alone reads each tank block\'s guns and stock from it', () => {
    const t = ticketOf(PIECES.find((x) => x.key === 'prokhorovka-1943')!, 11, 'att');
    const L = liveTicket(t as unknown as FieldTicket<never>, { me: null, platoons: false, tankmg: true } as LiveOpts);
    const d = L.drill as unknown as { squads: Map<number, Sq>; mgRow: (sq: Sq, m: M) => unknown };
    const blocks = (L.header().blocks as { id: number; kind: string; model: string | null }[]).filter((x) => x.kind === 'arm' && x.model);
    const model = new Map(blocks.map((x) => [x.id, x.model!]));
    let other = 0;
    for (const blk of blocks) {
      const sq = d.squads.get(blk.id)!, u = L.sim.allBlocks().find((x) => x.id === blk.id)!;
      if (L.sim.vehicle(u) !== blk.model) other++;
      for (const m of sq.men) if (m.role === 'tank' && !m.ml) expect(d.mgRow.call(d, sq, m), `block ${blk.id} drawn as a ${blk.model}`).toBe(T.models[blk.model!] ?? T.models._);
    }
    // (today four of the ten tank blocks are drawn as another model than the rules weigh)
    expect(other).toBeGreaterThanOrEqual(3);
    const y = read(L.sim, L.drill);
    expect(y.bursts.length).toBeGreaterThan(0);
    for (const x of y.bursts) {
      const R = T.models[model.get(x.by)!] ?? T.models._;
      expect(R[x.g as 'coax' | 'hull' | 'roof'], `block ${x.by}, drawn as a ${model.get(x.by)}, fired her ${x.g}`).toBe(1);
    }
    // Her stock is her drawn model's: what her belts hold is that row's stock less whole bursts of that row, or its tenth.
    for (const sq of d.squads.values()) for (const m of sq.men) {
      if (!model.has(sq.u.id) || m.role !== 'tank' || m.ml || m.mgLeft === undefined) continue;
      const R = T.models[model.get(sq.u.id)!] ?? T.models._, spent = R.stock - m.mgLeft;
      expect(m.mgLeft, `block ${sq.u.id} drawn as a ${model.get(sq.u.id)}`).toBeLessThanOrEqual(R.stock);
      expect(spent % (R.per * T.scale) === 0 || m.mgLeft === kept(R.stock), `block ${sq.u.id}: ${m.mgLeft} of ${R.stock}`).toBe(true);
    }
    // With the switch off the drill keeps nothing it is told, and the battle ends the same.
    const off = liveTicket(t as unknown as FieldTicket<never>, { me: null, platoons: false } as LiveOpts);
    expect((off.drill as unknown as { mgDrawn: Map<number, string> }).mgDrawn.size).toBe(0);
    expect((L.drill as unknown as { mgDrawn: Map<number, string> }).mgDrawn.size).toBe(blocks.length);
    expect(y.sig).toBe(read(off.sim, off.drill).sig);
  }, 300_000);
});
