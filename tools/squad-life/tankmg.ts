// The tank machine-gun meter (Brian, 25 November 2026, 15:55Z: "i never see the machine guns on the tanks firing";
// 15:56Z: "i dont want it to be all the time as thats unrealistic with ammo"; 15:57Z: "i want everything to see like it
// has its own mind"). Plays a battle as the 3D game starts it, once with the tankmg switch off and once with it on, and
// reads every burst a tank's machine guns are drawn firing: how many, from how many tanks, how often, in what spells,
// how alike the tanks of one block are, at what, from which gun, and how her rounds run down. Looks only: it reads the
// drill, and the two battles must end the same.
//
//   npx esbuild tools/squad-life/tankmg.ts --bundle --platform=node --format=esm --outfile=dist/tankmg.mjs --log-level=warning
//   node dist/tankmg.mjs [piece] [seed] [mode]      mode: shipped | life | hatches   (life: the drill book and squad life on
//                                                    as well; hatches: each commander's own hatch on as well)
//   node dist/tankmg.mjs practice [att] [def] [seed] [month]
//                                                    a practice field (tanks in the attack, drawn as sections, as the tests
//                                                    draw it) of that month (1939-09 the first, the default) and nation
//                                                    pair, with the switch on: its bursts in each 30 s, each tank's stock at
//                                                    the end, the tanks that reached their last tenth and when, and the
//                                                    bursts fired from it. The early-war read: practice USA GER 5, and
//                                                    practice SOV ITA 3 (run again when the historian's stocks arrive).
//
// Words used. Burst: one 'mg' round from a tank of an armoured block (the 3D game draws it as a burst); the burst that
// takes a man the rules took (a kill burst) is counted apart. Spell: a tank's bursts with no gap over 8 s. Firing share:
// the spells' lengths, each and 4 s, over all the time the tanks stood. Together: the share of bursts with another tank
// of the same block bursting within half a second. Her mark: the man she holds in her sights; a hop: her mark becoming
// another man while she holds one. An echo: a tank's spell starting exactly `apart` after another tank of her block
// fired; stairs: three or more tanks of a block opening so one after another. aimWas: degrees from what a coaxial burst
// was fired at to where her aim stood a step before; turret: to a turret that had followed her aim at `turn` degrees a second.
import { livePiece, type LiveOpts } from '../../src/battle/land/live';
import { DRILL, type Drill } from '../../src/battle/land/drill';
import { LandBattle, sampleTicket, withTeams } from '../../src/battle/land';

interface M { k: number; n: number; j?: number; mgOn?: [number, number, number]; role: string; pose: string; x: number; y: number; face: number; fell?: number; aim?: [number, number]; buttoned?: boolean; cmdDown?: number; hidden?: boolean; cover: string; went?: number; ml?: string; mgAt?: [number, number]; mgLeft?: number; mgRoof?: number; mgG?: string; mgT?: number }
interface U { id: number; side: string; kind: string; nat: string; team?: string; x: number; y: number; w: number; d: number; figures: number; state: string; engaged?: number; dug: boolean; hid?: boolean }
interface Sq { u: U; esc?: string; of?: number; men: M[]; veh: M[] }
/** One burst, as it stood the moment it was fired (read inside the drill's round()). */
export interface Burst { t: number; by: number; k: number; model: string; g: string; range: number; cls: 'threat' | 'open' | 'cover' | 'rattle' | 'none'; role: string; kind: string; hull: number; coax: number | null; shut: boolean; left: number; lowBefore: boolean; lastBefore: boolean; engaged: boolean; own: boolean; hers: boolean; aimWas: number | null; tur: number }
export interface Tally {
  t: number; sig: string; tanks: number; tankSecs: number; bursts: Burst[]; kills: { t: number; by: number; k: number; g?: string }[]; mainRounds: number; shells: number;
  /**
   * Each tank ('block:k'): her model, her stock and what is left, and when she first fell under `low`; when her belts
   * first stood at their last tenth (tenthAt: the coaxial and hull guns' stock, or a turretless model's roof belts) and
   * when at nothing (noneAt), and the bursts she fired from her tenth.
   */
  stock: Map<string, { model: string; stock: number; left: number; roofLeft?: number; lowAt?: number; min: number; tenthAt?: number; noneAt?: number; fromTenth: number }>;
  /** A model that is no tank of an armoured block and fired a machine-gun round as one (must stay empty). */
  wrong: string[];
  /** Her mark becoming another man while she holds one (hops); her aim swung over 15 degrees and back within 3 s (fro). */
  hops: number; fro: number;
  /** Bursts due and not fired because her man was gone or not worth the rounds that instant: all, and the most in a row for one tank. */
  misses: number; starve: number;
}

const T = DRILL.tankmg, deg = 180 / Math.PI;
const wrap = (a: number): number => Math.atan2(Math.sin(a), Math.cos(a));
const brg = (x0: number, y0: number, x1: number, y1: number): number => Math.atan2(x1 - x0, y1 - y0);
const FIRING = new Set(['fire', 'fire_stand', 'fire_kneel', 'fire_prone', 'lmg_burst']);
/** The rounds of a stock she keeps (the last tenth, drill.ts mgKept), and whether her belts stand at it now (mgTenth). */
const kept = (stock: number): number => Math.ceil(stock * T.last - 1e-9);
const atTenth = (R: { stock: number; roofStock?: number }, m: M): boolean => (R.stock > 0 ? (m.mgLeft ?? R.stock) <= kept(R.stock) : R.roofStock ? (m.mgRoof ?? R.roofStock) <= kept(R.roofStock) : false);
const atNone = (R: { stock: number; roofStock?: number }, m: M): boolean => (R.stock > 0 ? (m.mgLeft ?? R.stock) <= 0 : R.roofStock ? (m.mgRoof ?? R.roofStock) <= 0 : false);

/** Plays the battle to its end, reading the tanks' machine guns. `opts` are added to the game's own start. */
export function play(piece: string, seed: number, opts: Record<string, unknown>): Tally {
  const L = livePiece(piece, seed, { me: null, platoons: false, ...opts } as LiveOpts);
  return read(L.sim, L.drill);
}

/**
 * A practice field as the tests draw it (sampleTicket, plains, odds 1.3, the attack methodical and the defence bold,
 * withTeams), the attacker's tanks in the attack, of the month given ('1939-09', the first, to '1945-08'), drawn as
 * sections with the options added.
 */
export function practice(att: string, def: string, seed: number, month: string, opts: Record<string, unknown>): Tally {
  const [y, mo] = month.split('-').map(Number), turn = (y - 1939) * 12 + (mo - 9) + 1;
  const date = `${['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'][mo - 1]} ${y}`;
  const t = withTeams(sampleTicket('plains', 1.3, { seed, nat: { att, def }, style: { att: 'methodical', def: 'bold' }, date }));
  t.turn = turn;
  const b = new LandBattle(t);
  return read(b, b.drillOn({ sections: true, ...opts }));
}

/** Fights a battle to its end, reading the tanks' machine guns as the drill fires them. */
export function read(b: LandBattle, drill: Drill): Tally {
  const d = drill as unknown as { squads: Map<number, Sq>; fired: number; shots: { g?: string }[]; round: (sq: Sq, m: M, w: string, aim: [number, number]) => void; blockAt: (side: string, p: [number, number]) => U | undefined; standing: (sq: Sq) => M[]; tankMgFire: (sq: Sq, m: M) => void; mgMan: (sq: Sq, m: M, f: number) => unknown };
  const out: Tally = { t: 0, sig: '', tanks: 0, tankSecs: 0, bursts: [], kills: [], mainRounds: 0, shells: 0, stock: new Map(), wrong: [], hops: 0, fro: 0, misses: 0, starve: 0 };
  // Each tank as the steps go by: her aim as the last step ended, a turret following it, her mark's man, her aim's swings.
  const seen = new Map<M, { aim?: [number, number]; tur: number; on?: string; log: [number, number][]; run: number }>();
  const of = (m: M) => seen.get(m) ?? seen.set(m, { tur: m.face, log: [], run: 0 }).get(m)!;
  // A burst that was due and not fired because her man was not there to be fired at (the drill's own finding of him).
  let noMan = false, firing = false, pre: M | undefined;
  const fromTenth = new Map<string, number>();
  const man0 = d.mgMan, fire0 = d.tankMgFire;
  d.mgMan = function (this: unknown, sq: Sq, m: M, f: number): unknown { const a = man0.call(this, sq, m, f); if (firing && !a) noMan = true; return a; };
  d.tankMgFire = function (this: unknown, sq: Sq, m: M): void {
    const n0 = d.fired, z = of(m), tenth = atTenth(rowOf(sq, m).R, m);
    noMan = false; firing = true; pre = { mgLeft: m.mgLeft, mgRoof: m.mgRoof } as M; fire0.call(this, sq, m); firing = false; pre = undefined;
    if (d.fired !== n0) { z.run = 0; if (tenth) fromTenth.set(`${sq.u.id}:${m.k}`, (fromTenth.get(`${sq.u.id}:${m.k}`) ?? 0) + 1); }
    else if (noMan) { out.misses++; out.starve = Math.max(out.starve, ++z.run); }
  };
  const rowOf = (sq: Sq, m: M) => { const ml = m.ml ?? b.vehicle(sq.u as never) ?? '_'; return { ml, R: T.models[ml] ?? T.models._ }; };
  // Every round the drill makes passes through round(): a tank's machine-gun burst is read there, as it is fired.
  const round0 = d.round;
  d.round = function (this: unknown, sq: Sq, m: M, w: string, aim: [number, number]): void {
    // (a machine-gun round from a vehicle that is no tank of an armoured block: an escort's, a carrier, a lorry, a gun)
    if (w === 'mg' && (m.role === 'gun' || m.role === 'lorry' || (m.role === 'tank' && (sq.u.kind !== 'arm' || !!sq.esc || sq.of !== undefined)))) out.wrong.push(`${sq.u.id}:${m.k} ${m.role} ${m.ml ?? ''}`);
    if (w !== 'mg' || m.role !== 'tank') return round0.call(this, sq, m, w, aim);
    {
      const u = sq.u, { ml, R } = rowOf(sq, m);
      const on = d.blockAt.call(d, u.side, aim), tq = on ? d.squads.get(on.id) : undefined;
      let v: M | undefined, dd = Infinity;
      for (const x of tq ? d.standing.call(d, tq) : []) { const q = Math.hypot(x.x - aim[0], x.y - aim[1]); if (q < dd) { dd = q; v = x; } }
      // (her own burst this step, by her own mind: its rounds are already off her stock, so her belts are read as they
      // stood before it; else the drill book's)
      const mine = m.mgT !== undefined && Math.abs(m.mgT - b.t) < 1e-6, was = mine && pre ? pre : m;
      const f = R.stock > 0 ? (m.mgLeft ?? R.stock) / R.stock : R.roofStock ? (m.mgRoof ?? R.roofStock) / R.roofStock : 0;
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
      const burst: Burst = {
        t: Math.round(b.t * 10) / 10, by: u.id, k: m.k, model: ml, g: '?', range, cls, role: v?.role ?? 'none', kind: on?.kind ?? 'none',
        hull: v ? Math.abs(wrap(brg(m.x, m.y, v.x, v.y) - m.face)) * deg : 999, coax: v && m.aim ? Math.abs(wrap(brg(m.x, m.y, v.x, v.y) - brg(m.x, m.y, m.aim[0], m.aim[1]))) * deg : null,
        shut: !!m.buttoned || m.cmdDown !== undefined, left: f, lowBefore: before < T.low, lastBefore: tenth, engaged: u.engaged !== undefined, own: mine,
        hers: !!v && !!on && !!m.mgOn && m.mgOn[0] === on.id && m.mgOn[1] === v.k && m.mgOn[2] === (v.j ?? -1),
        aimWas: v && of(m).aim ? Math.abs(wrap(brg(m.x, m.y, v.x, v.y) - brg(m.x, m.y, of(m).aim![0], of(m).aim![1]))) * deg : null, tur: v ? Math.abs(wrap(brg(m.x, m.y, v.x, v.y) - of(m).tur)) * deg : 999,
      };
      const n0 = d.fired;
      round0.call(this, sq, m, w, aim);
      // (the gun is read off the round the drill made, as the 3D game reads it)
      if (d.fired !== n0) { burst.g = d.shots[d.shots.length - 1].g ?? '?'; out.bursts.push(burst); }
    }
  };
  let last = 0;
  while (!b.over && b.t < 2000) {
    b.step();
    for (const sq of d.squads.values()) {
      if (sq.u.kind !== 'arm' || sq.esc || sq.of !== undefined) continue;
      for (const m of sq.men) {
        if (m.role !== 'tank') continue;
        const key = `${sq.u.id}:${m.k}`, { ml, R } = rowOf(sq, m);
        let s = out.stock.get(key);
        if (!s) out.stock.set(key, (s = { model: ml, stock: R.stock + (R.roofStock ?? 0), left: R.stock + (R.roofStock ?? 0), min: 0, fromTenth: 0 }));
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
        if (m.mgLeft !== undefined) {
          s.left = m.mgLeft + (m.mgRoof ?? 0); s.roofLeft = m.mgRoof; s.min = Math.min(s.min, m.mgLeft, m.mgRoof ?? 0);
          const f = R.stock > 0 ? m.mgLeft / R.stock : R.roofStock ? (m.mgRoof ?? 0) / R.roofStock : 0;
          if (f < T.low && s.lowAt === undefined) s.lowAt = Math.round(b.t * 10) / 10;
          if (atTenth(R, m) && s.tenthAt === undefined) s.tenthAt = Math.round(b.t * 10) / 10;
          if (atNone(R, m) && s.noneAt === undefined) s.noneAt = Math.round(b.t * 10) / 10;
        }
      }
    }
    for (const r of drill.rounds()) {
      if (r.n <= last) continue;
      last = r.n;
      const by = r.by !== undefined ? b.unit(r.by) : undefined;
      if (by?.kind !== 'arm') continue;
      if (r.w === 'tank') out.mainRounds++;
      if (r.w === 'mg' && r.r === 'kill') out.kills.push({ t: r.t, by: r.by!, k: r.k ?? -1, ...(r.g ? { g: r.g } : {}) });
    }
  }
  // (the fight's own shells, by the craters they left: the same with the switch on, as it changes nothing in the fight)
  out.shells = b.craters.length;
  d.round = round0; d.mgMan = man0; d.tankMgFire = fire0;
  for (const z of seen.values()) {
    const A = z.log, off = (i: number, j: number): number => Math.abs(wrap((A[i][1] - A[j][1]) / deg)) * deg;
    for (let i = 1; i < A.length; i++) {
      if (Number.isNaN(A[i][1]) || Number.isNaN(A[i - 1][1]) || off(i, i - 1) <= 15) continue;
      for (let j = i + 1; j < A.length && A[j][0] - A[i][0] <= 3; j++) if (!Number.isNaN(A[j][1]) && off(j, i - 1) < 8) { out.fro++; break; }
    }
  }
  for (const [key, n] of fromTenth) out.stock.get(key)!.fromTenth = n;
  out.t = b.t; out.tanks = out.stock.size;
  out.sig = JSON.stringify({ over: b.over, t: b.t, units: b.units.map((u) => [u.id, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged]) });
  return out;
}

const q = (a: number[], p: number): number | null => (a.length ? Math.round(a[Math.min(a.length - 1, Math.floor(a.length * p))] * 10) / 10 : null);

/** The numbers of one battle's bursts. */
export function sum(y: Tally) {
  const byTank = new Map<string, number[]>(), byBlock = new Map<number, { t: number; k: number }[]>();
  for (const x of y.bursts) {
    const key = `${x.by}:${x.k}`;
    (byTank.get(key) ?? byTank.set(key, []).get(key)!).push(x.t);
    (byBlock.get(x.by) ?? byBlock.set(x.by, []).get(x.by)!).push({ t: x.t, k: x.k });
  }
  let longest = 0, fireSecs = 0, most = 0;
  const gaps: number[] = [], spells: number[] = [];
  for (const ts of byTank.values()) {
    let a = 0;
    for (let i = 1; i <= ts.length; i++) {
      if (i < ts.length && ts[i] - ts[i - 1] <= 8) { gaps.push(ts[i] - ts[i - 1]); continue; }
      longest = Math.max(longest, ts[i - 1] - ts[a]); fireSecs += ts[i - 1] - ts[a] + 4; spells.push(i - a); most = Math.max(most, i - a); a = i;
    }
  }
  let tog = 0, starts = 0, echo = 0, stairs = 0;
  const firstSpread: number[] = [];
  for (const list of byBlock.values()) {
    for (const a of list) if (list.some((c) => c.k !== a.k && Math.abs(c.t - a.t) <= 0.5 + 1e-9)) tog++;
    const mine = new Map<number, number>(), st: { t: number; k: number }[] = [];
    let before: { t: number; k: number } | undefined;
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
    if (first.size >= 3) firstSpread.push(Math.max(...first.values()) - Math.min(...first.values()));
  }
  gaps.sort((a, c) => a - c); firstSpread.sort((a, c) => a - c);
  const n = y.bursts.length, count = (f: (x: Burst) => string): Record<string, number> => { const o: Record<string, number> = {}; for (const x of y.bursts) o[f(x)] = (o[f(x)] ?? 0) + 1; return o; };
  const ranges = y.bursts.map((x) => x.range).sort((a, c) => a - c), stock = [...y.stock.values()];
  const low = stock.filter((s) => s.lowAt !== undefined), lowAt = low.map((s) => s.lowAt!).sort((a, c) => a - c);
  const lowBy: Record<string, string> = {};
  for (const ml of new Set(stock.map((s) => s.model))) lowBy[ml] = `${low.filter((s) => s.model === ml).length}/${stock.filter((s) => s.model === ml).length}`;
  return {
    ends: Math.round(y.t), bursts: n, guns: count((x) => x.g), kills: y.kills.length, mainRounds: y.mainRounds, craters: y.shells,
    tanksFiring: `${byTank.size}/${y.tanks}`, perTankMin: +(n / Math.max(1e-9, y.tankSecs / 60)).toFixed(2), firingShare: +(fireSecs / Math.max(1e-9, y.tankSecs)).toFixed(3),
    longestSpell: Math.round(longest * 10) / 10, mostInSpell: most, gapInSpell: [q(gaps, 0.1), q(gaps, 0.5), q(gaps, 0.9)], together: +(tog / Math.max(1, n)).toFixed(3),
    firstSpread: [q(firstSpread, 0), q(firstSpread, 0.5)], spellStarts: starts, echo, stairs,
    markHopsPerTankMin: +(y.hops / Math.max(1e-9, y.tankSecs / 60)).toFixed(2), aimToAndFro: y.fro, dueNotFiredNoMan: y.misses, mostInARow: y.starve,
    coaxAimWasMost: q(y.bursts.filter((x) => x.g === 'coax').map((x) => x.aimWas ?? 999).sort((a, c) => a - c), 0.9999), coaxTurretMost: q(y.bursts.filter((x) => x.g === 'coax').map((x) => x.tur).sort((a, c) => a - c), 0.9999),
    targets: count((x) => x.cls), atKinds: count((x) => x.kind), range: [q(ranges, 0.5), q(ranges, 0.9), q(ranges, 0.999)],
    lowTanks: `${low.length}/${stock.length}`, lowAt: [q(lowAt, 0), q(lowAt, 0.5)], lowByModel: lowBy, out: stock.filter((s) => s.stock > 0 && s.left <= 0).length,
    leastLeft: +Math.min(1, ...stock.filter((s) => s.stock > 0).map((s) => s.left / s.stock)).toFixed(2), belowNone: stock.filter((s) => s.min < 0).length,
    bad: { notWorth: y.bursts.filter((x) => x.cls === 'none').length, bookPath: y.bursts.filter((x) => !x.own).length, roofShut: y.bursts.filter((x) => x.g === 'roof' && x.shut).length, hullOff: y.bursts.filter((x) => x.g === 'hull' && x.hull > T.hullArc + 1e-6).length, coaxOff: y.bursts.filter((x) => x.g === 'coax' && (x.coax === null || x.coax > T.coaxArc + 1e-6)).length, wrong: y.wrong.length, notHerMan: y.bursts.filter((x) => x.own && !x.hers).length },
  };
}

/**
 * The ammunition read of one battle with the switch on: its bursts in each 30 s of battle time, each tank's stock at the
 * end (as a share of her row's), the tanks that reached their last tenth and when, those at nothing, and the bursts
 * fired from the tenth (all of them, and those in the second half of the fight at a threat within lastReach).
 */
export function ammo(y: Tally) {
  const per30: number[] = [];
  for (const x of y.bursts) { const i = Math.floor(x.t / 30); while (per30.length <= i) per30.push(0); per30[i]++; }
  const stock = [...y.stock.entries()], tenth = stock.filter(([, s]) => s.tenthAt !== undefined).sort((a, c) => a[1].tenthAt! - c[1].tenthAt!);
  const bursts = new Map<string, number>();
  for (const x of y.bursts) bursts.set(`${x.by}:${x.k}`, (bursts.get(`${x.by}:${x.k}`) ?? 0) + 1);
  return {
    ends: Math.round(y.t), bursts: y.bursts.length, kills: y.kills.length, per30,
    tanks: stock.map(([k, s]) => `${k} ${s.model} ${s.left}/${s.stock} bursts ${bursts.get(k) ?? 0}${s.tenthAt !== undefined ? ` tenth@${s.tenthAt} fromTenth ${s.fromTenth}` : ''}${s.noneAt !== undefined ? ` none@${s.noneAt}` : ''}`),
    reachedTenth: `${tenth.length}/${stock.length}`, tenthAt: tenth.map(([, s]) => s.tenthAt), atNothing: stock.filter(([, s]) => s.noneAt !== undefined).length,
    fromTenth: stock.reduce((a, [, s]) => a + s.fromTenth, 0),
    secondHalfThreatClose: y.bursts.filter((x) => x.t >= y.t / 2 && x.cls === 'threat' && x.range < T.lastReach).length,
  };
}

if (/tankmg\.mjs$/.test(process.argv[1] ?? '') && process.argv[2] === 'practice') {
  const [, att = 'USA', def = 'GER', seedS = '5', month = '1939-09'] = process.argv.slice(2);
  const y = practice(att, def, Number(seedS), month, { tankmg: true });
  console.log(`practice ${att} v ${def}, seed ${seedS}, ${month}: tankmg on, drawn as sections`);
  console.log(JSON.stringify(sum(y)));
  const a = ammo(y);
  console.log(JSON.stringify({ ...a, tanks: undefined }));
  for (const line of a.tanks) console.log(`  ${line}`);
  console.log(`the battle ends the same with the switch off: ${practice(att, def, Number(seedS), month, {}).sig === y.sig}`);
} else if (/tankmg\.mjs$/.test(process.argv[1] ?? '')) {
  const [piece = 'prokhorovka-1943', seedS = '11', mode = 'shipped'] = process.argv.slice(2);
  const life = mode === 'life' ? { book: true, life: true } : mode === 'hatches' ? { hatches: true } : {};
  const off = play(piece, Number(seedS), life), on = play(piece, Number(seedS), { ...life, tankmg: true });
  console.log(`${piece} seed ${seedS}, ${mode}: tankmg off, then on`);
  console.log(JSON.stringify({ switch: 'off', ...sum(off) }));
  console.log(JSON.stringify({ switch: 'on', ...sum(on) }));
  console.log(`the battle ends the same with the switch on: ${on.sig === off.sig}`);
}
