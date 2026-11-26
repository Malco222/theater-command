// Tanks with minds of their own, timing and movement (release 2; Brian, 25 November 2026, 15:57Z: "i dont want the
// tanks and vehicles doing the exact same thing as each other. i want everything to see like it has its own mind";
// 15:56Z: "should the tanks be moving or was that the real tactic?": move, halt briefly to fire, move on, one of a pair
// covering while the other moves). Looks only (ruling 217): Drill.tankmind gives each tank's commander his own moment
// (drill.ts mindPhase, mindRoll, mindBound, mindStep; DR.tankmind). These tests hold that it changes nothing in the
// fight, that off is off, that it draws the same twice, and, with the switch on, that a block's tanks no longer change
// pose as one, that a round on the advance comes from a real halt, that a platoon never stands halted together while
// its block comes on, that no tank falls further behind her place than she does today, that about as many shells are
// drawn, that rolling on fast a platoon's hulls are not dead parallel, that coming on no tank backs on for long with
// her rear to the enemy, and that every limit a tank keeps as a body still holds. The same measures on the 3D game's
// own fights: tools/squad-life/vehicles.ts.
// Step two (looking, firing posts, the other vehicles and the guns: drill.ts mindLook, mindGaze, mindPost, mindSit,
// mindOut, mindOff, mindVolley): with the switch on a standing tank's hull and gun are apart, a halted platoon that is
// not fighting watches all round between its tanks, a waiting line no longer sits frozen alike, a tank whose firing
// place has cover backs down behind it between her rounds, a battery's guns fire as a ripple, and a block's cars, AA
// vehicles, jeeps, motorcycles and carriers no longer move off and halt as one.
import { describe, expect, it } from 'vitest';
import { LandBattle, sampleTicket, type LandUnit } from '../src/battle/land';
import { wreckOffset } from '../src/battle/land/drill';
import { liveTicket } from '../src/battle/land/live';
import { PIECES, ticketOf } from '../src/battle/land/pieces';

type Ground = Parameters<typeof sampleTicket>[0];
type Style = 'bold' | 'methodical' | 'mass';
interface Opts { mind?: boolean; life?: boolean; sections?: boolean; none?: boolean; xp?: number; others?: boolean }
const MOVING = new Set(['drive', 'reverse']);
const HALTED = new Set(['halt', 'fire']);
const WAITING = new Set(['hold', 'rest', 'startline']);
const DEG = 180 / Math.PI;
const POSES = new Set(['drive', 'reverse', 'halt', 'fire', 'down']);
const r1 = (v: number): number => Math.round(v * 10) / 10;
const wrap = (a: number): number => Math.atan2(Math.sin(a), Math.cos(a));

/**
 * Normandy's armies on a practice field, a tank regiment on each side: the attack American, the defence German.
 * 'others': every corps brings armoured cars, AA vehicles, jeeps and motorcycles to its blocks, and the armoured
 * blocks their infantry's carriers, half of them half-tracks and half lorries (looks only: the fight is the same).
 */
const battle = (g: Ground, seed: number, att: Style, def: Style, others = false): LandBattle => {
  const T = sampleTicket(g, 1.3, { seed, nat: { att: 'USA', def: 'GER' }, style: { att, def } });
  T.att.forces[1].kind = 'arm'; T.def.forces[1].kind = 'arm';
  if (others) for (const f of [...T.att.forces, ...T.def.forces]) f.kit = { ...f.kit, cars: 3, aa: 3, jeeps: 2, bikes: 1, halftracks: 0.5 };
  const b = new LandBattle(T);
  if (others) { b.cars = true; b.aa = true; b.light = true; b.mech = true; }
  return b;
};
const sig = (b: LandBattle): string => JSON.stringify({ over: b.over, t: b.t, units: b.units.map((u: LandUnit) => [u.id, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged]) });
/** A short mark of a long text (the whole draw of a battle, step by step, is too long to keep). */
const mark = (h: number, s: string): number => { for (let i = 0; i < s.length; i++) h = (Math.imul(h, 31) + s.charCodeAt(i)) | 0; return h; };

interface Seen {
  sig: string; draw: number; minds: number;
  /** Every half second, blocks of three tanks or more: samples, all in one pose, every tank changing pose at once, all into reverse; the same while advancing. */
  n: number; one: number; flips: number; toRev: number; advN: number; advOne: number; advFlips: number;
  /** Each tank: battle minutes watched, her changes of pose, her changes of direction (forward to backing or back). */
  minutes: number; changes: number; turnabouts: number;
  /** Main-gun shots (a new stamp on her): all; on the advance; her own on the advance (not the rules' kill rounds) and those from a halt of a second or more; with a mate's in the same step. */
  shots: number; adv: number; advHalt: number; advOwn: number; advOwnHalt: number; mates: number;
  /**
   * While her block advances: the longest all its tanks stood together (s), the same while it made ground (2 m a second
   * or more over three seconds), and the furthest any fell behind her place while it made ground (m).
   */
  stoodAny: number; stood: number; behind: number;
  /** Samples where a tank's commander is acting on another phase than his block's: the platoon's leader, and the rest. */
  lagLead: number; lagRest: number; lagN: number;
  lim: { steps: number; turn: number; fast: number; thrown: number; onSpot: number; still: number; stalls: number; strayed: number; outside: number; outStill: number; badPose: number; samples: number; fell: number; jump: number; wreckFar: number };
  /** Step two, every half second. Halted tanks with an aim: samples, and those whose gun is over 6 degrees off her hull. */
  hullN: number; hullOff: number;
  /**
   * A platoon of three or more, all halted, its block not fighting and holding, resting or on its start line: samples
   * with two aims or more, those whose aim points are all one, and those whose guns are all within 6 degrees.
   */
  idleN: number; idleSame: number; idleNear: number;
  /** Blocks of three tanks or more that hold, rest or wait on their start line: samples, and those frozen alike (all halted in one pose, hulls within 6 degrees of their middle, guns within 6 degrees). */
  waitN: number; frozen: number;
  /** Times a tank backed down behind her firing place, and tank half seconds spent there or on the way. */
  sits: number; down: number;
  /** A tank's aim point changing (each is a row sent to the 3D game): changes, read every half second. */
  aimRows: number;
  /** Batteries: their guns' shots; her own shots (not the rules' kill rounds) with another gun's own to compare, and those within half a second of one. */
  gunShots: number; gunN: number; gunNear: number;
  /**
   * The other vehicles ('others'), by kind (cars, aa, light: a block's vehicle-only squads; lorries: lorries and
   * carriers), squads of two or more: half seconds in which any moved off or halted, the share of the squad that did
   * so together summed over them, those where the whole squad did; samples with two aims or more, and those all one.
   */
  out: Record<string, { ev: number; share: number; all: number; aimN: number; aimSame: number; n: number }>;
  /**
   * Rolling on fast (advancing, 3.5 m a second or more over three seconds), blocks of three tanks or more all driving, every half
   * second: samples, those whose hulls are all within 2 degrees, and the longest run of those (s). And the longest a
   * tank backed on the way her block faces, her rear to its enemy, while it advanced (s).
   */
  parN: number; par: number; parRun: number; backRun: number;
}
interface Veh { k: number; role: string; pose: string; x: number; y: number; face: number; fell?: number; went?: number; aim?: [number, number]; shot?: number }
interface Squad { u: LandUnit; men: Veh[]; veh: Veh[]; esc?: string }

/**
 * Fights the battle, reading its tanks at every step as the 3D game does (places to 10 cm). 'none': no men drawn at
 * all; 'mind' the switch (false: given as off; absent: left out); 'xp' sets every armoured block's experience first.
 */
function watch(g: Ground, seed: number, att: Style, def: Style, o: Opts = {}): Seen {
  const b = battle(g, seed, att, def, o.others);
  if (o.xp !== undefined) for (const u of b.units) if (u.kind === 'arm') u.xp = o.xp;
  const s: Seen = {
    sig: '', draw: 7, minds: 0, n: 0, one: 0, flips: 0, toRev: 0, advN: 0, advOne: 0, advFlips: 0, minutes: 0, changes: 0, turnabouts: 0,
    shots: 0, adv: 0, advHalt: 0, advOwn: 0, advOwnHalt: 0, mates: 0, stoodAny: 0, stood: 0, behind: 0, lagLead: 0, lagRest: 0, lagN: 0,
    lim: { steps: 0, turn: 0, fast: 0, thrown: 0, onSpot: 0, still: 0, stalls: 0, strayed: 0, outside: 0, outStill: 0, badPose: 0, samples: 0, fell: 0, jump: 0, wreckFar: 0 },
    hullN: 0, hullOff: 0, idleN: 0, idleSame: 0, idleNear: 0, waitN: 0, frozen: 0, sits: 0, down: 0, aimRows: 0, gunShots: 0, gunN: 0, gunNear: 0, out: {},
    parN: 0, par: 0, parRun: 0, backRun: 0,
  };
  if (o.none) { for (let k = 0; !b.over && k < 30000; k++) b.step(); s.sig = sig(b); return s; }
  const d = b.drillOn({ sections: true, ...(o.sections ? {} : { platoons: true }), ...(o.life ? { life: true } : {}), ...(o.mind === undefined ? {} : { tankmind: o.mind }) });
  const was = new Map<string, { x: number; y: number; rx: number; ry: number; f: number; stood: number; shot?: number; same: number; go: number; down?: true }>();
  const pose = new Map<string, string>(), way = new Map<string, string>();
  const trail = new Map<number, [number, number][]>(), run = new Map<number, [number, number]>();
  const parRun = new Map<number, number>(), backRun = new Map<string, number>();
  const kills = new Set<string>();
  // Step two: each tank's last aim point and whether she sat; each gun's last stamp and every battery's shots, [t, k,
  // a kill round]; whether each of the other vehicles was going.
  const aimWas = new Map<string, string>(), sat = new Map<string, boolean>(), gunWas = new Map<string, number>(), gunKills = new Set<string>();
  const gunLog = new Map<number, [number, number, boolean][]>(), going = new Map<string, boolean>();
  const squads = (d as unknown as { squads: Map<number, Squad> }).squads;
  const aimKey = (m: Veh): string => (m.aim ? `${m.aim[0].toFixed(1)},${m.aim[1].toFixed(1)}` : '');
  const brg = (m: Veh): number => Math.atan2(m.aim![0] - m.x, m.aim![1] - m.y);
  const spread = (l: number[]): number => { const mid = Math.atan2(l.reduce((a, x) => a + Math.sin(x), 0), l.reduce((a, x) => a + Math.cos(x), 0)); return Math.max(...l.map((x) => Math.abs(wrap(x - mid)))) * DEG; };
  let seen = 0;
  for (let k = 0; !b.over && k < 30000; k++) {
    b.step();
    const t = b.t, half = k % 5 === 4;
    kills.clear();
    for (const r of d.rounds()) {
      if (r.n <= seen) continue;
      seen = r.n;
      if (r.w === 'tank' && r.r === 'kill' && r.by !== undefined) kills.add(`${r.by}:${r.k}`);
      if (r.w === 'gun' && r.r === 'kill' && r.by !== undefined) gunKills.add(`${r.by}:${r.k}:${r.t.toFixed(1)}`);
    }
    if (half) s.draw = mark(s.draw, JSON.stringify(d.view()));
    // A battery's guns: every new shot stamp.
    for (const u of b.units) {
      if (u.kind !== 'art' || u.team || u.state === 'gone' || u.state === 'withdrawn') continue;
      for (const m of d.squad(u.id)?.men ?? []) {
        const id = `${u.id}:${m.k}`;
        if (m.role !== 'gun' || m.fell !== undefined || m.k >= u.figures || m.shot === undefined || gunWas.get(id) === m.shot) continue;
        gunWas.set(id, m.shot);
        const l = gunLog.get(u.id) ?? []; gunLog.set(u.id, l); l.push([m.shot, m.k, gunKills.has(`${id}:${m.shot.toFixed(1)}`)]);
      }
    }
    // The other vehicles: who moved off and who halted in this half second.
    if (half && o.others) for (const [key, sq] of squads) {
      const u = sq.u;
      if (u.state === 'gone' || u.state === 'withdrawn' || !(sq.esc || u.kind === 'mot')) continue;
      const kind = sq.esc ?? 'lorries', veh = sq.esc ? sq.men.filter((m) => m.fell === undefined && m.k < u.figures) : sq.veh.filter((m) => m.fell === undefined);
      if (veh.length < 2) continue;
      const g0 = (s.out[kind] ??= { ev: 0, share: 0, all: 0, aimN: 0, aimSame: 0, n: 0 });
      let st = 0, ha = 0;
      for (const m of veh) {
        const id = `${key}:${m.k}`, now = MOVING.has(m.pose) && (m.went ?? 0) > 0.3, w = going.get(id);
        if (w !== undefined && w !== now) { if (now) st++; else ha++; }
        going.set(id, now);
      }
      g0.n++;
      for (const c of [st, ha]) if (c) { g0.ev++; g0.share += c / veh.length; if (c === veh.length) g0.all++; }
      const aimed = veh.filter((m) => m.aim);
      if (aimed.length >= 2) { g0.aimN++; if (new Set(aimed.map(aimKey)).size === 1) g0.aimSame++; }
    }
    for (const u of b.units) {
      if (u.kind !== 'arm') continue;
      const sq = d.squad(u.id);
      if (!sq) continue;
      // The block's own step, and the way it has made over the last three seconds.
      const tr = trail.get(u.id) ?? []; trail.set(u.id, tr);
      const o0 = tr[tr.length - 1], went = o0 ? Math.hypot(u.x - o0[0], u.y - o0[1]) : 0;
      tr.push([u.x, u.y]); if (tr.length > 31) tr.shift();
      const wx = u.x - tr[0][0], wy = u.y - tr[0][1], net = tr.length > 30 ? Math.hypot(wx, wy) / 3 : 0;
      const adv = sq.phase === 'bound' || sq.phase === 'deploy', ground = adv && net >= 2;
      const on = u.state !== 'gone' && u.state !== 'withdrawn';
      for (const m of sq.men) {
        // A tank the rules took this step: no jump as she falls, and her wreck near the rules' own place for it.
        const id = `${u.id}:${m.k}`, w = was.get(id);
        if (m.role !== 'tank' || m.fell === undefined || !w || w.down) continue;
        s.lim.fell++; w.down = true;
        if (Math.hypot(m.x - w.x, m.y - w.y) >= 1.3) s.lim.jump++;
        const z = b.wrecks.filter((x) => x.unit === u.id).sort((p, q) => (q.at ?? 0) - (p.at ?? 0))[0];
        if (z && Math.hypot(m.x - z.x, m.y - z.y) >= Math.hypot(u.w, u.d) / 2 + 60) s.lim.wreckFar++;
      }
      const tanks = on ? sq.men.filter((m) => m.role === 'tank' && m.fell === undefined && m.k < u.figures) : [];
      let standing = 0;
      const fired: number[] = [];
      tanks.forEach((m, i) => {
        const id = `${u.id}:${m.k}`, w = was.get(id), rx = r1(m.x), ry = r1(m.y), mv = m.went ?? 0;
        const still = !MOVING.has(m.pose) && mv < 0.05;
        if (still) standing++;
        if (o.mind === undefined && (m as { mind?: unknown }).mind !== undefined) s.minds++;
        let stood = 0, same = 0, go = 0;
        if (w) {
          const dist = Math.hypot(rx - w.rx, ry - w.ry), df = Math.abs(wrap(m.face - w.f));
          s.lim.steps++;
          if (df > 0.08 + 0.02) s.lim.turn++;
          if (dist > 1.8 && dist > went + 0.05) s.lim.thrown++;
          if (dist > Math.max(1.2, went) + 0.15) s.lim.fast++;
          go = dist > 0.5 ? 1 : 0;
          if (go && w.go === -1) s.lim.stalls++;
          if (!go && w.go === 1 && dist < 0.05) go = -1;
          if (MOVING.has(m.pose) && r1(mv) > 0.5 && dist < 0.005 && df < 0.005) { same = w.same + 1; if (same === 10) s.lim.onSpot++; }
          if (!MOVING.has(m.pose) && m.v > 0) s.lim.still++;
          stood = still ? w.stood + 0.1 : 0;
          // Backing on with her block, her rear to its enemy.
          const onward = adv && m.pose === 'reverse' && mv > 0.5 && (m.x - w.x) * Math.sin(u.face) + (m.y - w.y) * Math.cos(u.face) > 0.5 * Math.hypot(m.x - w.x, m.y - w.y);
          const br = onward ? (backRun.get(id) ?? 0) + 0.1 : 0;
          backRun.set(id, br); s.backRun = Math.max(s.backRun, br);
          if (m.shot !== undefined && m.shot !== w.shot && m.shot > t - 0.15) {
            const kill = kills.has(id), halt = w.stood >= 1 - 1e-6;
            s.shots++; fired.push(m.k);
            if (adv) { s.adv++; if (halt) s.advHalt++; if (!kill) { s.advOwn++; if (halt) s.advOwnHalt++; } }
          }
        }
        if (!POSES.has(m.pose)) s.lim.badPose++;
        // The leash: her block's box and its margin, and the ring round it.
        const dx = m.x - u.x, dy = m.y - u.y, a = dx * Math.cos(u.face) - dy * Math.sin(u.face), l = dx * Math.sin(u.face) + dy * Math.cos(u.face);
        const edge = (sq.phase === 'flee' ? 70 : 45) + 0.5;
        s.lim.samples++;
        if (Math.hypot(dx, dy) > Math.hypot(u.w / 2 + edge, u.d / 2 + edge)) s.lim.strayed++;
        // (the battle's last step is not read for this: every man stands easy where he is, drill.ts settle)
        if (Math.abs(a) > u.w / 2 + edge || Math.abs(l) > u.d / 2 + edge) { s.lim.outside++; if (!MOVING.has(m.pose) && !b.over) s.lim.outStill++; }
        if (ground) {
          const [wa, wl] = wreckOffset(u.id, u.figures0 - 1 - m.k, u.w, u.d);
          const sx = u.x + wa * Math.cos(u.face) + wl * Math.sin(u.face), sy = u.y - wa * Math.sin(u.face) + wl * Math.cos(u.face);
          s.behind = Math.max(s.behind, ((sx - m.x) * wx + (sy - m.y) * wy) / (net * 3));
        }
        const own = (m as { mind?: { ph?: string } }).mind?.ph;
        if (own !== undefined) { s.lagN++; if (own !== sq.phase) { if (i === 0) s.lagLead++; else s.lagRest++; } }
        was.set(id, { x: m.x, y: m.y, rx, ry, f: m.face, stood, shot: m.shot, same, go });
      });
      if (fired.length > 1) s.mates += fired.length;
      // The whole platoon standing together while its block advances (two tanks or more).
      const all = tanks.length >= 2 && standing === tanks.length, r0 = run.get(u.id) ?? [0, 0];
      const r: [number, number] = [all && adv ? r0[0] + 0.1 : 0, all && ground ? r0[1] + 0.1 : 0];
      run.set(u.id, r); s.stoodAny = Math.max(s.stoodAny, r[0]); s.stood = Math.max(s.stood, r[1]);
      if (!half) continue;
      // Step two: hull against gun, aim rows, backing down behind her firing place; the platoon's aims and hulls.
      for (const m of tanks) {
        const id = `${u.id}:${m.k}`, ak = aimKey(m), la = aimWas.get(id), down = !!(m as { mind?: { sit?: string } }).mind?.sit;
        if (m.aim && HALTED.has(m.pose)) { s.hullN++; if (Math.abs(wrap(brg(m) - m.face)) * DEG > 6) s.hullOff++; }
        if (la !== undefined && la !== ak && ak !== '') s.aimRows++;
        aimWas.set(id, ak);
        if (down) { s.down++; if (sat.get(id) === false) s.sits++; }
        sat.set(id, down);
      }
      if (tanks.length >= 3 && WAITING.has(sq.phase)) {
        const allHalted = tanks.every((m) => HALTED.has(m.pose)), aimed = tanks.filter((m) => m.aim), sp = aimed.length >= 2 ? spread(aimed.map(brg)) : 0;
        s.waitN++;
        if (allHalted && new Set(tanks.map((m) => m.pose)).size === 1 && spread(tanks.map((m) => m.face)) < 6 && (aimed.length < 2 || sp < 6)) s.frozen++;
        if (allHalted && u.engaged === undefined && aimed.length >= 2) { s.idleN++; if (new Set(aimed.map(aimKey)).size === 1) s.idleSame++; if (sp < 6) s.idleNear++; }
      }
      let ch = 0, rev = 0;
      for (const m of tanks) {
        const id = `${u.id}:${m.k}`, p = pose.get(id), q = way.get(id);
        s.minutes += 0.5 / 60;
        if (p !== undefined && p !== m.pose) { ch++; s.changes++; if (m.pose === 'reverse') rev++; }
        pose.set(id, m.pose);
        if (MOVING.has(m.pose)) { if (q !== undefined && q !== m.pose) s.turnabouts++; way.set(id, m.pose); }
      }
      if (tanks.length < 3) continue;
      if (adv && net >= 3.5 && tanks.every((m) => m.pose === 'drive')) {
        const pr = spread(tanks.map((m) => m.face)) <= 1 ? (parRun.get(u.id) ?? 0) + 0.5 : 0;
        s.parN++; if (pr) s.par++;
        parRun.set(u.id, pr); s.parRun = Math.max(s.parRun, pr);
      } else parRun.set(u.id, 0);
      const one = new Set(tanks.map((m) => m.pose)).size === 1;
      s.n++; if (one) s.one++; if (ch === tanks.length) s.flips++; if (rev === tanks.length) s.toRev++;
      if (adv) { s.advN++; if (one) s.advOne++; if (ch === tanks.length) s.advFlips++; }
    }
  }
  // A battery's own shots (not the rules' kill rounds, drawn from her the moment the rules take a model): how near another gun's own.
  for (const l of gunLog.values()) for (let i = 0; i < l.length; i++) {
    s.gunShots++;
    if (l[i][2]) continue;
    let gap = Infinity;
    for (let j = 0; j < l.length; j++) if (j !== i && l[j][1] !== l[i][1] && !l[j][2]) gap = Math.min(gap, Math.abs(l[j][0] - l[i][0]));
    if (gap < Infinity) { s.gunN++; if (gap <= 0.5) s.gunNear++; }
  }
  s.draw = mark(s.draw, JSON.stringify(d.rounds()));
  s.sig = sig(b);
  return s;
}

/** The fights watched, switch off and on: an attack over open ground, a slower one over hills, a mass against the bold. */
const FIGHTS: [Ground, number, Style, Style][] = [['plains', 5, 'methodical', 'bold'], ['hills', 23, 'bold', 'methodical'], ['plains', 11, 'mass', 'bold']];
const sum = (l: Seen[], f: (s: Seen) => number): number => l.reduce((a, s) => a + f(s), 0);

const off = FIGHTS.map((f) => watch(...f)), on = FIGHTS.map((f) => watch(...f, { mind: true }));

describe('tanks with minds of their own: timing and movement (looks only)', () => {

  it('changes nothing in the fight: no men drawn, the drill as shipped, the switch on, by sections or platoons, with squad life or without', () => {
    FIGHTS.forEach((f, i) => {
      const none = watch(...f, { none: true }).sig;
      expect(off[i].sig, f[0]).toBe(none);
      expect(on[i].sig, f[0]).toBe(none);
    });
    const f = FIGHTS[0], none = watch(...f, { none: true }).sig;
    expect(watch(...f, { mind: true, life: true }).sig).toBe(none);
    expect(watch(...f, { mind: true, sections: true }).sig).toBe(none);
    expect(watch(...f, { mind: true, sections: true, life: true }).sig).toBe(none);
  }, 300_000);

  it('off is off: the switch given as off draws as the switch left out, and no tank is given a mind', () => {
    // (That the drill with the switch off draws as the tree before this build did, every model's own fields at every
    // step, was checked by hand when this was built: a test cannot hold the old tree.)
    const f = FIGHTS[0], no = watch(...f, { mind: false });
    expect(no.draw).toBe(off[0].draw);
    expect(no.sig).toBe(off[0].sig);
    for (const s of off) { expect(s.minds).toBe(0); expect(s.lagN).toBe(0); }
  }, 300_000);

  it('draws the same twice', () => {
    const f = FIGHTS[1], again = watch(...f, { mind: true });
    expect(again.draw).toBe(on[1].draw);
    expect(JSON.stringify(again.lim)).toBe(JSON.stringify(on[1].lim));
  }, 300_000);

  it('each commander in his own time: a block\'s tanks no longer change pose as one, nor go into reverse as one', () => {
    FIGHTS.forEach((f, i) => {
      const a = off[i], c = on[i], perMin = (s: Seen): number => s.flips / (s.n / 120);
      expect(c.n, f[0]).toBeGreaterThan(2000);
      // Every tank of a block changing pose in the same half second: today 1.8, 2.0 and 3.3 times a battle minute in
      // these fights; with the switch on 0.4, 0.2 and 0.2.
      expect(perMin(c), f[0]).toBeLessThan(0.9);
      expect(perMin(c) * 3, f[0]).toBeLessThan(perMin(a));
      // The whole block into reverse in one half second: today 13, 8 and 17 times; with the switch on never.
      expect(a.toRev, f[0]).toBeGreaterThan(4);
      expect(c.toRev, f[0]).toBeLessThanOrEqual(1);
      // The share of the time all a block's tanks show one pose: today 59%, 63% and 68% (on the advance 69%, 70%, 69%);
      // with the switch on 55%, 59% and 63% (67%, 65%, 62%). Most of what is left is a platoon all driving with no enemy
      // in reach, or all standing in a held line.
      expect(c.one / c.n, f[0]).toBeLessThan(a.one / a.n - 0.01);
      expect(c.advOne / c.advN, f[0]).toBeLessThan(a.advOne / a.advN);
    });
  }, 300_000);

  it('calms the flick: a tank changes pose less often, and direction (forward to backing or back) half as often or less', () => {
    FIGHTS.forEach((f, i) => {
      const a = off[i], c = on[i];
      // Changes of pose a tank a battle minute: today 9.8, 9.6 and 11.6; with the switch on 9.2, 8.5 and 9.1.
      expect(c.changes / c.minutes, f[0]).toBeLessThan(a.changes / a.minutes);
      // Changes of direction: today 2.7, 3.1 and 4.5; with the switch on 1.6, 1.7 and 1.5.
      expect(c.turnabouts / c.minutes, f[0]).toBeLessThan(0.7 * (a.turnabouts / a.minutes));
    });
  }, 300_000);

  it('move, halt briefly to fire, move on: on the advance her round comes from a real halt, a battle second or more', () => {
    FIGHTS.forEach((f, i) => {
      const a = off[i], c = on[i];
      expect(c.advOwn, f[0]).toBeGreaterThan(50);
      // Her own rounds (the rules' kill rounds, drawn from whichever tank the rules name, left out): today 14%, 18% and
      // 5% come from a tank that has stood a second or more; with the switch on 98%, 100% and 100%.
      expect(a.advOwnHalt / a.advOwn, f[0]).toBeLessThan(0.3);
      expect(c.advOwnHalt / c.advOwn, f[0]).toBeGreaterThan(0.93);
      // Every main-gun shot on the advance, the kill rounds too: today 14%, 15% and 6%; with the switch on 90%, 86%, 73%.
      expect(c.advHalt / c.adv, f[0]).toBeGreaterThan(0.6);
    });
  }, 300_000);

  it('no volley as one: two tanks of a block seldom fire in the same step', () => {
    FIGHTS.forEach((f, i) => {
      // Shots with a mate's in the same step: today 63 of 332, 44 of 340 and 27 of 203; with the switch on 6, 8 and 0.
      expect(off[i].mates / off[i].shots, f[0]).toBeGreaterThan(0.1);
      expect(on[i].mates / on[i].shots, f[0]).toBeLessThan(0.07);
    });
  }, 300_000);

  it('about as many shells are drawn as today', () => {
    // Main-gun shots: today 332, 340 and 203; with the switch on 303, 319 and 218.
    FIGHTS.forEach((f, i) => { expect(Math.abs(on[i].shots - off[i].shots) / off[i].shots, f[0]).toBeLessThan(0.15); });
    expect(Math.abs(sum(on, (s) => s.shots) - sum(off, (s) => s.shots)) / sum(off, (s) => s.shots)).toBeLessThan(0.08);
  }, 300_000);

  it('a platoon does not stand halted together while its block comes on, and no tank falls further behind her place than today', () => {
    FIGHTS.forEach((f, i) => {
      const a = off[i], c = on[i];
      // The longest all a block's tanks stand together while it makes ground: today 11.0, 6.5 and 1.5 s (while it
      // advances at all, the same); with the switch on 0.2, 0.4 and 0.1 s (0.4, 0.4 and 0.1). Waiting on the fast
      // advance for her place to come up to her (her block keeps its tanks behind its foot, or has checked), the pairs
      // take turns to roll on slowly (mindWait): before that mend 10.1, 3.0 and 0.1 s.
      expect(c.stood, f[0]).toBeLessThan(1);
      expect(c.stoodAny, f[0]).toBeLessThan(1);
      expect(c.stood, f[0]).toBeLessThanOrEqual(a.stood);
      // The furthest a tank is behind her place along her block's way while it makes ground: today 90, 123 and 112 m
      // (a tank turning about, or kept behind her foot: not her halts); with the switch on 90, 109 and 94 m.
      expect(c.behind, f[0]).toBeLessThan(a.behind + 10);
    });
  }, 300_000);

  it('rolling on fast, a platoon\'s hulls are not dead parallel: each tank steers a little her own way', () => {
    FIGHTS.forEach((f, i) => {
      const a = off[i], c = on[i];
      // Blocks of three tanks or more rolling on at 3.5 m a second or more, all driving: every hull within 2 degrees of
      // the others today 61.1%, 40.9% and 41.2% of 1,215 to 1,480 half seconds, the longest run 47.5, 57.5 and 44.5 s;
      // with the switch on 3.6%, 5.2% and 8.1%, the longest 3.0, 4.5 and 4.0 s (before mindWeave 58.5%, 44.7% and
      // 46.7%, the longest 46, 58.5 and 50 s).
      expect(c.parN, f[0]).toBeGreaterThan(500);
      expect(c.par / c.parN, f[0]).toBeLessThan(0.15);
      expect(c.par / c.parN, f[0]).toBeLessThan(0.3 * (a.par / a.parN));
      expect(c.parRun, f[0]).toBeLessThan(10);
    });
  }, 300_000);

  it('coming on, a tank does not back on with her block, her rear to its enemy, for long', () => {
    // Longest run a tank backs the way her block faces while it advances: today 21.5, 15.3 and 14.0 s; with the switch
    // on 1.7, 3.5 and 2.6 s.
    FIGHTS.forEach((f, i) => { expect(on[i].backRun, f[0]).toBeLessThan(5); expect(on[i].backRun, f[0]).toBeLessThan(off[i].backRun); });
    // A block slowed to 4 m a second after a sharp turn, its place kept a few metres behind one tank's tail as she backs
    // (Prokhorovka's ticket, seed 3, as a campaign battle with its cars, AA vehicles and carriers; block 5, from 90 s):
    // she turns about once she has backed DR.tankmind.back metres, however near the place she backs to. Before that
    // mend she backed 31.0 s; today (switch off) 5.8 s over the whole battle; now under 5 s.
    const t = ticketOf(PIECES.find((x) => x.key === 'prokhorovka-1943')!, 3, null);
    const kit = <S extends { forces: { kit?: object }[] }>(side: S): S => ({ ...side, forces: side.forces.map((g) => ({ ...g, kit: { ...g.kit, cars: 3, aa: 3, jeeps: 2, bikes: 1 } })) });
    const L = liveTicket({ ...t, att: kit(t.att), def: kit(t.def) }, { me: null, tankmind: true, mech: true, cars: true, aa: true, light: true, td: true, spg: true });
    const squads = (L.drill as unknown as { squads: Map<number, Squad & { phase: string }> }).squads;
    const was = new Map<string, [number, number, number]>();
    let longest = 0;
    while (!L.sim.over && L.sim.t < 130 - 1e-6) {
      L.to(L.sim.t + 0.1, 100000);
      for (const sq of squads.values()) {
        const u = sq.u, adv = sq.phase === 'bound' || sq.phase === 'deploy';
        if (u.kind !== 'arm') continue;
        for (const m of sq.men) {
          if (m.role !== 'tank' || m.fell !== undefined) continue;
          const id = `${u.id}:${m.k}`, w = was.get(id);
          const d = w ? Math.hypot(m.x - w[0], m.y - w[1]) : 0;
          const onward = !!w && adv && m.pose === 'reverse' && (m.went ?? 0) > 0.5 && (m.x - w[0]) * Math.sin(u.face) + (m.y - w[1]) * Math.cos(u.face) > 0.5 * d;
          const r = onward ? w![2] + 0.1 : 0;
          was.set(id, [m.x, m.y, r]); longest = Math.max(longest, r);
        }
      }
    }
    expect(longest).toBeLessThan(5);
  }, 300_000);

  it('the platoon\'s leader acts at once, the others at their own moments, a green crew later than a veteran one', () => {
    for (const s of on) {
      expect(s.lagN).toBeGreaterThan(50000);
      expect(s.lagLead).toBe(0);
      // Tank steps on which her commander is still acting on his block's last phase: 1,329, 1,231 and 1,052 here.
      expect(s.lagRest).toBeGreaterThan(300);
    }
    // The same share for green crews and for veterans (the block's experience set, so the fights differ a little):
    // 2.1% against 1.3% in the third fight.
    const f = FIGHTS[2], raw = watch(...f, { mind: true, xp: 0.05 }), vet = watch(...f, { mind: true, xp: 0.9 });
    expect(raw.lagRest / raw.lagN).toBeGreaterThan(1.2 * (vet.lagRest / vet.lagN));
  }, 300_000);

  it('every limit a tank keeps as a body still holds with the switch on', () => {
    FIGHTS.forEach((f, i) => {
      const a = off[i].lim, c = on[i].lim;
      expect(c.steps, f[0]).toBeGreaterThan(50000);
      // Only the poses the 3D game knows; no turn over 0.08 rad a step; never thrown, never driving on the spot, never
      // standing with a speed; a tank lost falls where she stood, her wreck near the rules' own place for it.
      expect(c.badPose, f[0]).toBe(0); expect(c.turn, f[0]).toBe(0); expect(c.thrown, f[0]).toBe(0);
      expect(c.onSpot, f[0]).toBe(0); expect(c.still, f[0]).toBe(0);
      expect(c.fell, f[0]).toBe(a.fell); expect(c.jump, f[0]).toBe(0); expect(c.wreckFar, f[0]).toBe(0);
      // A step over the pace (1.2 m, or her block's own step, and 0.15 m): today none; with the switch on 3, 8 and 5
      // of 70,000 to 90,000 (closing up after a halt she may go 12.5 m a second; places are read to 10 cm). A stall
      // (a step lost between two she drives): today 0, 25 and 4; with the switch on 1, 0 and 0. Under 1 in 1,000.
      expect(c.fast, f[0]).toBeLessThan(c.steps / 1000);
      expect(c.stalls, f[0]).toBeLessThan(c.steps / 1000);
      // The leash: never beyond the ring round her block (today 0, 5 and 0 steps; with the switch on none), never
      // standing outside its box and margin, and outside it on the move no oftener than today (0, 1,240 and 227 steps
      // today, the hills fight's tanks kept far behind their foot; with the switch on 0, 681 and 99).
      expect(c.strayed, f[0]).toBeLessThanOrEqual(a.strayed);
      expect(c.outStill, f[0]).toBe(0);
      expect(c.outside, f[0]).toBeLessThanOrEqual(a.outside + c.samples / 1000);
    });
  }, 300_000);
});

describe('tanks with minds of their own: looking, firing posts, the other vehicles and the guns (looks only)', () => {
  const share = (a: number, n: number): number => a / Math.max(1, n);

  it('hull and gun apart: a standing tank turns her front to the threat and lays her gun where she looks', () => {
    FIGHTS.forEach((f, i) => {
      const a = off[i], c = on[i];
      expect(c.hullN, f[0]).toBeGreaterThan(3000);
      // Halted tanks whose gun is over 6 degrees off her hull: today 7.8%, 9.5% and 10.2% of their half seconds (her
      // hull is turned to her aim); with the switch on 86.3%, 86.6% and 86.1%.
      expect(share(a.hullOff, a.hullN), f[0]).toBeLessThan(0.2);
      expect(share(c.hullOff, c.hullN), f[0]).toBeGreaterThan(0.7);
    });
  }, 300_000);

  it('turrets watch their own arcs: a halted platoon that is not fighting never has all its guns on one point', () => {
    FIGHTS.forEach((f, i) => {
      const a = off[i], c = on[i];
      expect(c.idleN, f[0]).toBeGreaterThan(300);
      // All its aim points one: today 77.9%, 83.7% and 90.7% of the half seconds; with the switch on never.
      expect(share(a.idleSame, a.idleN), f[0]).toBeGreaterThan(0.6);
      expect(share(c.idleSame, c.idleN), f[0]).toBeLessThan(0.03);
      // All its guns within 6 degrees of their middle: today 90.7%, 81.4% and 79.6%; with the switch on never.
      expect(share(a.idleNear, a.idleN), f[0]).toBeGreaterThan(0.6);
      expect(share(c.idleNear, c.idleN), f[0]).toBeLessThan(0.05);
    });
  }, 300_000);

  it('a waiting line is not frozen alike: holding, resting or on its start line, its tanks no longer stand as copies', () => {
    FIGHTS.forEach((f, i) => {
      const a = off[i], c = on[i];
      expect(c.waitN, f[0]).toBeGreaterThan(500);
      // All halted in one pose, hulls within 6 degrees, guns within 6 degrees: today 76.5%, 69.6% and 65.7% of the
      // half seconds a block of three tanks or more waits; with the switch on never.
      expect(share(a.frozen, a.waitN), f[0]).toBeGreaterThan(0.5);
      expect(share(c.frozen, c.waitN), f[0]).toBeLessThan(0.08);
    });
  }, 300_000);

  it('a look is held some seconds: no more aim points are sent to the 3D game than today', () => {
    FIGHTS.forEach((f, i) => {
      const a = off[i], c = on[i];
      // A tank's aim point changing (each a row sent): today 42.3, 45.0 and 53.0 a tank a battle minute; with the
      // switch on 27.7, 28.0 and 31.4.
      expect(c.aimRows / c.minutes, f[0]).toBeLessThan(a.aimRows / a.minutes);
      expect(c.aimRows / c.minutes, f[0]).toBeLessThan(40);
    });
  }, 300_000);

  it('hull down: where her firing place has cover a tank backs down behind it and comes up to fire', () => {
    // Times a tank backed down behind her firing place: today never; with the switch on 8, 24 and 1 (the hills have
    // the crests; on the plains few places have anything to sit behind), and she is down there or on her way 87, 790
    // and 72 tank half seconds.
    expect(sum(off, (s) => s.sits + s.down)).toBe(0);
    expect(on[1].sits).toBeGreaterThan(10);
    expect(on[1].down).toBeGreaterThan(300);
    expect(sum(on, (s) => s.sits)).toBeGreaterThan(20);
    // (never the most of her time: under a tenth of the tank half seconds watched, 0.6%, 4.3% and 0.5% here)
    for (const s of on) expect(s.down / (s.minutes * 120)).toBeLessThan(0.1);
  }, 300_000);

  it('a battery fires as a ripple: no gun fires within half a second of another, and as many shells are drawn', () => {
    FIGHTS.forEach((f, i) => {
      const a = off[i], c = on[i];
      expect(c.gunN, f[0]).toBeGreaterThan(50);
      // A gun's own shot (the rules' kill rounds, drawn the moment the rules take a model, left out) within half a
      // second of another gun's of her battery: today 80.8% of 78, 94.0% of 100 and 62.8% of 121; with the switch
      // on none of 78, 100 and 119.
      expect(share(a.gunNear, a.gunN), f[0]).toBeGreaterThan(0.5);
      expect(share(c.gunNear, c.gunN), f[0]).toBeLessThan(0.03);
      // The guns' shots drawn: today 104, 142 and 138; with the switch on 104, 142 and 136.
      expect(Math.abs(c.gunShots - a.gunShots) / a.gunShots, f[0]).toBeLessThan(0.1);
    });
  }, 300_000);

  it('the other vehicles each in her own time: cars, AA vehicles, jeeps and motorcycles, carriers and lorries', () => {
    const f = FIGHTS[0], none = watch(...f, { none: true, others: true }).sig;
    const a = watch(...f, { others: true }), c = watch(...f, { others: true, mind: true });
    // (they are looks only too: the fight with them drawn is the fight with no men drawn)
    expect(a.sig).toBe(none);
    expect(c.sig).toBe(none);
    for (const kind of ['cars', 'aa', 'light', 'lorries']) {
      const x = a.out[kind], y = c.out[kind];
      expect(y.n, kind).toBeGreaterThan(3000);
      expect(y.n, kind).toBe(x.n);
      // The share of a squad that moves off or halts together, when any does: today 53.6% (cars), 56.9% (AA), 66.1%
      // (jeeps and motorcycles) and 56.4% (carriers); with the switch on 40.7%, 44.0%, 55.1% and 50.3% (one of three
      // is 33%).
      expect(share(y.share, y.ev), kind).toBeLessThan(share(x.share, x.ev) - 0.04);
    }
    // Half seconds in which a whole squad moved off, or halted, together. Today: cars 419 (of 2,287 in which any did),
    // AA vehicles 592 (of 2,734), jeeps and motorcycles 657 (of 2,044). With the switch on 15 (of 468), 22 (of 308)
    // and 27 (of 265).
    for (const kind of ['cars', 'aa', 'light']) {
      expect(a.out[kind].all, kind).toBeGreaterThan(300);
      expect(c.out[kind].all, kind).toBeLessThan(a.out[kind].all / 8);
      expect(share(c.out[kind].all, c.out[kind].ev), kind).toBeLessThan(0.16);
    }
    // Carriers: today 80 (of 301); with the switch on 47 (of 295). What is left is not this build's: the instant they
    // all stop for their men to get down, and the step they all turn about when their block's place leaps.
    expect(c.out.lorries.all).toBeLessThan(a.out.lorries.all * 0.75);
    // A squad's cars all laid on one point: today always (7,275 of 7,275 half seconds); with the switch on never, each
    // on her own point of the enemy or watching her own flank. AA vehicles: no aim today; now each her own arc.
    expect(a.out.cars.aimSame).toBe(a.out.cars.aimN);
    expect(c.out.cars.aimN).toBeGreaterThan(5000);
    expect(share(c.out.cars.aimSame, c.out.cars.aimN)).toBeLessThan(0.02);
    expect(a.out.aa.aimN).toBe(0);
    expect(c.out.aa.aimN).toBeGreaterThan(5000);
    expect(share(c.out.aa.aimSame, c.out.aa.aimN)).toBeLessThan(0.02);
  }, 300_000);
});
