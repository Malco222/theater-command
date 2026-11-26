// The vehicles' meter (Brian, 25 November 2026, 15:57Z: "i dont want the tanks and vehicles doing the exact same thing
// as each other. i want everything to see like it has its own mind"; 15:56Z: "should the tanks be moving or was that
// the real tactic?"). Plays a battle as the 3D game starts it and reads, every step and twice a battle second, what the
// tanks of every armoured block are doing: how often they all share one pose, how often the whole block changes pose
// in the same half second, whether a main-gun shot on the advance comes from a real halt, how long a whole platoon
// stands halted together while its block advances, how far a tank falls behind her place, how many shells are drawn,
// how much of the advance she spends backing (and backing with her rear to the enemy), how often a block's hulls are
// all parallel while it rolls on fast, how often a tank changes her
// pose and her direction in a battle minute, and the limits a tank must keep as a body (her turn and her step, the
// leash, never driving on the spot).
// Step two (looking, firing posts, the other vehicles and the guns), read twice a battle second: how far a standing
// tank's gun is off her hull; how alike the aim points of a halted platoon that is not fighting are; how often a
// waiting line sits frozen alike (every tank halted in one pose, hulls and guns within 6 degrees); how much of her
// time a tank sits backed down behind her firing place, and how often she shifts posts; how often a tank's aim point
// changes (each change is a row sent to the 3D game); how often a battery's gun fires within half a second of
// another; and how often a block's cars, AA vehicles, jeeps and motorcycles, its lorries and carriers, move off or
// halt all in the same half second.
// Looks only: reads the drill, changes nothing.
//
//   node dist/vehicles.mjs [piece] [seed] [upto] [mode] [block from to]
//   mode: off | on (the tankmind switch), and +life +hatches +sections (on+life ...); +mech draws the armoured
//   infantry's carriers; +camp plays the piece's ticket as a campaign battle, which alone draws armoured cars, AA
//   vehicles, jeeps and motorcycles (with carriers, tank destroyers and self-propelled guns). block, from, to: one
//   block's tanks written out every half second between two battle times (D drive, r reverse, h halt, F fire, x lost;
//   each with her hull's bearing in degrees), with the block's phase, its pace and its longest stand so far.
import { livePiece, liveTicket, type LiveOpts } from '../../src/battle/land/live';
import { PIECES, ticketOf } from '../../src/battle/land/pieces';
import { wreckOffset } from '../../src/battle/land/drill';

interface M { k: number; n: number; role: string; pose: string; x: number; y: number; face: number; fell?: number; v: number; went?: number; aim?: [number, number]; shot?: number; cyc?: string; post?: number; cover?: string; mind?: { sit?: string } }
interface U { id: number; side: string; kind: string; state: string; figures: number; figures0: number; x: number; y: number; w: number; d: number; face: number; engaged?: number; team?: string }
interface Sq { u: U; men: M[]; veh: M[]; phase: string; speed: number; net: number; wx: number; wy: number; heading: number; esc?: string; of?: number }

const [piece = 'prokhorovka-1943', seedS = '11', uptoS = '250', mode = 'off', traceS = '', fromS = '0', toS = '1e9'] = process.argv.slice(2);
const flags = new Set(mode.split('+'));
const opts: LiveOpts = { me: null };
if (flags.has('sections')) opts.platoons = false;
if (flags.has('life')) opts.life = true;
if (flags.has('hatches')) opts.hatches = true;
if (flags.has('on')) opts.tankmind = true;
if (flags.has('mech') || flags.has('camp')) opts.mech = true;
if (flags.has('camp')) Object.assign(opts, { cars: true, aa: true, light: true, td: true, spg: true });
/** The piece's ticket as a campaign battle's, every corps given three armoured cars, three AA vehicles, two jeeps and a motorcycle to a block. */
function campaign(): Parameters<typeof liveTicket>[0] {
  const t = ticketOf(PIECES.find((x) => x.key === piece)!, Number(seedS), null);
  const kit = <S extends { forces: { kit?: object }[] }>(side: S): S => ({ ...side, forces: side.forces.map((f) => ({ ...f, kit: { ...f.kit, cars: 3, aa: 3, jeeps: 2, bikes: 1 } })) });
  return { ...t, att: kit(t.att), def: kit(t.def) };
}
const L = flags.has('camp') ? liveTicket(campaign(), opts) : livePiece(piece, Number(seedS), opts);
const squads = (L.drill as unknown as { squads: Map<number, Sq> }).squads;
const wrap = (a: number): number => Math.atan2(Math.sin(a), Math.cos(a));
const HALTED = new Set(['halt', 'fire']);
const MOVING = new Set(['drive', 'reverse']);
const POSES = new Set(['drive', 'reverse', 'halt', 'fire', 'down']);
const ADVANCE = new Set(['bound', 'deploy']);
const r1 = (v: number): number => Math.round(v * 10) / 10;

/** Twice a second, blocks of three tanks or more: all in one pose, and every tank changing pose in the same half second. */
interface Acc { n: number; one: number; flips: number; toRev: number; mixed: number }
const acc = new Map<string, Acc>();
const A = (ph: string): Acc => { let a = acc.get(ph); if (!a) acc.set(ph, (a = { n: 0, one: 0, flips: 0, toRev: 0, mixed: 0 })); return a; };
const lastPose = new Map<string, string>();
const poseTime = new Map<string, number>();
/** Each tank, step by step: where she was, how long she has stood, her last shot stamp. */
interface Was { x: number; y: number; f: number; rx: number; ry: number; stood: number; shot?: number; same: number; go: number; pose: string; ran: number }
const was = new Map<string, Was>();
const blockAt = new Map<number, [number, number]>();
/** Main-gun shots: all, on the advance, from a halt of a second or more; the rules' own kill rounds told apart. */
const shots = { all: 0, adv: 0, advHalt1: 0, advOwn: 0, advOwnHalt1: 0, kills: 0, halted: 0, haltedHalt1: 0, moving: 0, fast: 0, slow: 0 };
/** Where tanks were caught outside their block's leash (block, battle time), the first few. */
const outAt: string[] = [];
/** Shots by block, [t, k]: how close in time a mate's shot is. */
const byBlock = new Map<number, [number, number][]>();
/** All of a block's tanks (two or more) standing together while it advances: the run now, the longest, the steps. */
const allHalt = { run: new Map<number, number>(), longest: 0, steps: 0, advSteps: 0, longestGround: 0, runG: new Map<number, number>(), stepsGround: 0, groundSteps: 0 };
/** How far behind her place (along the way her block is making) a tank is, and how far from it, while it advances and makes ground. */
const behind: number[] = [];
let behindMax = 0, offMax = 0;
/** Her halts while her block advances: how long each lasts. */
const haltLens: number[] = [];
/**
 * Backing while her block advances: tank steps advancing, those in reverse, and those backing the way her block faces
 * (her rear to the enemy it faces: a tank backing on with her block), the run of that now by tank and the longest (s);
 * and each tank's changes of pose and of direction every half second.
 */
const backing = { adv: 0, rev: 0, on: 0, run: new Map<string, number>(), longest: 0, at: '', changes: 0, turnabouts: 0, halves: 0 };
const lastWay = new Map<string, string>();
/** Rolling on fast, every tank of a block driving (three or more): samples, hulls all within 2 and 1 degrees, the longest run within 2. */
const par = { n: 0, two: 0, one: 0, run: new Map<number, number>(), longest: 0 };
/** The body's limits (tests/battle-land-drill.test.ts and battle-land-sections.test.ts), read on every tank step. */
const lim = { steps: 0, turn: 0, turnMax: 0, fast: 0, stepMax: 0, thrown: 0, onSpot: 0, still: 0, stalls: 0, strayed: 0, outside: 0, outStill: 0, badPose: 0, samples: 0, jump: 0, wreckFar: 0, fell: 0 };
let seenRound = 0, tankRounds = 0;
const killStamp = new Set<string>();

// ---- Step two: looking, firing posts, the other vehicles and the guns (read twice a battle second) ----
const DEG = 180 / Math.PI;
/** A tank with an aim: how far her gun's bearing is off her hull, standing (halt, fire) and on the move. */
const hull = { halt: 0, halt6: 0, haltSum: 0, move: 0, move6: 0 };
/** A platoon of three or more, all halted, its block not fighting and holding, resting or on its start line: its aim points. */
const idle = { n: 0, aimed: 0, same: 0, near6: 0, none: 0, spread: 0 };
/** Frozen alike, by phase: [frozen, samples] of blocks of three tanks or more. */
const frozen = new Map<string, [number, number]>();
/** Hull down: tank half seconds backed down behind her firing place; halted ones whose place has cover; shifts of post. */
const post = { halves: 0, down: 0, halted: 0, covered: 0, shifts: 0, sits: 0, fight: 0, fightDown: 0 };
const lastSit = new Map<string, boolean>();
/** Aim rows: how often a tank's aim point changes (to 10 cm, as it is sent). */
const aimRows = { halves: 0, changes: 0 };
const lastAim = new Map<string, string>(), lastPost = new Map<string, number>();
/** Batteries: each gun's shots, [t, k] by block. */
const gunShots = new Map<number, [number, number, boolean][]>(), gunShot = new Map<string, number>(), gunKill = new Set<string>();
/**
 * The other vehicles, by kind (cars, aa, light: a block's vehicle-only squads; lorries: a lorried or armoured-infantry
 * block's lorries and carriers), squads of two or more: the half seconds any of them moved off or halted, those where
 * every one did so together, and (standing, all of them) hulls within 6 degrees; cars' aim points all one.
 */
interface Grp { n: number; starts: number; startsAll: number; startsShare: number; halts: number; haltsAll: number; haltsShare: number; stood: number; hullAlike: number; aimed: number; aimSame: number }
const groups = new Map<string, Grp>(), going = new Map<string, boolean>();
const G = (k: string): Grp => { let g = groups.get(k); if (!g) groups.set(k, (g = { n: 0, starts: 0, startsAll: 0, startsShare: 0, halts: 0, haltsAll: 0, haltsShare: 0, stood: 0, hullAlike: 0, aimed: 0, aimSame: 0 })); return g; };
const alike = (ms: M[]): boolean => {
  const mid = Math.atan2(ms.reduce((a, m) => a + Math.sin(m.face), 0), ms.reduce((a, m) => a + Math.cos(m.face), 0));
  return ms.every((m) => Math.abs(wrap(m.face - mid)) < 0.105);
};
const aimKey = (m: M): string => (m.aim ? `${m.aim[0].toFixed(1)},${m.aim[1].toFixed(1)}` : '');
const brg = (m: M): number => Math.atan2(m.aim![0] - m.x, m.aim![1] - m.y);
function group(kind: string, key: number, veh: M[]): void {
  if (veh.length < 2) return;
  const g = G(kind);
  let st = 0, ha = 0;
  for (const m of veh) {
    const id = `${key}:${m.k}`, now = MOVING.has(m.pose) && (m.went ?? 0) > 0.3, was = going.get(id);
    if (was !== undefined && was !== now) { if (now) st++; else ha++; }
    going.set(id, now);
  }
  g.n++;
  if (st) { g.starts++; g.startsShare += st / veh.length; if (st === veh.length) g.startsAll++; }
  if (ha) { g.halts++; g.haltsShare += ha / veh.length; if (ha === veh.length) g.haltsAll++; }
  if (veh.every((m) => !MOVING.has(m.pose))) { g.stood++; if (alike(veh)) g.hullAlike++; }
  const aimed = veh.filter((m) => m.aim);
  if (aimed.length >= 2) { g.aimed++; if (new Set(aimed.map(aimKey)).size === 1) g.aimSame++; }
}
function stepTwo(): void {
  for (const [key, sq] of squads) {
    const u = sq.u;
    if (u.state === 'gone' || u.state === 'withdrawn') continue;
    if (sq.esc) { group(sq.esc, key, sq.men.filter((m) => m.fell === undefined && m.k < u.figures)); continue; }
    if (u.kind === 'mot') group('lorries', key, sq.veh.filter((m) => m.fell === undefined));
    if (key < 0 || sq.of !== undefined || u.kind !== 'arm') continue;
    const veh = sq.men.filter((m) => m.role === 'tank' && m.fell === undefined && m.k < u.figures);
    for (const m of veh) {
      const id = `${u.id}:${m.k}`, ak = aimKey(m), la = lastAim.get(id), lp = lastPost.get(id);
      if (m.aim) { const off = Math.abs(wrap(brg(m) - m.face)) * DEG; if (HALTED.has(m.pose)) { hull.halt++; hull.haltSum += off; if (off > 6) hull.halt6++; } else { hull.move++; if (off > 6) hull.move6++; } }
      aimRows.halves++; if (la !== undefined && la !== ak && ak !== '') aimRows.changes++;
      lastAim.set(id, ak);
      post.halves++; if (m.mind?.sit) post.down++;
      if (m.mind?.sit && lastSit.get(id) === false) post.sits++;
      lastSit.set(id, !!m.mind?.sit);
      if (u.engaged !== undefined && !['bound', 'deploy', 'column', 'flee', 'fallback'].includes(sq.phase)) { post.fight++; if (m.mind?.sit) post.fightDown++; }
      if (HALTED.has(m.pose)) { post.halted++; if (['crest', 'woods', 'building', 'rock', 'works'].includes(m.cover ?? '')) post.covered++; }
      if (lp !== undefined && (m.post ?? 0) > lp) post.shifts++;
      lastPost.set(id, m.post ?? 0);
    }
    if (veh.length < 3) continue;
    const allHalted = veh.every((m) => HALTED.has(m.pose)), aimed = veh.filter((m) => m.aim);
    let spread = 0;
    if (aimed.length >= 2) { const bs = aimed.map(brg), mid = Math.atan2(bs.reduce((a, b) => a + Math.sin(b), 0), bs.reduce((a, b) => a + Math.cos(b), 0)); spread = Math.max(...bs.map((b) => Math.abs(wrap(b - mid)))) * DEG; }
    const fz = frozen.get(sq.phase) ?? [0, 0]; fz[1]++;
    if (allHalted && new Set(veh.map((m) => m.pose)).size === 1 && alike(veh) && (aimed.length < 2 || spread < 6)) fz[0]++;
    frozen.set(sq.phase, fz);
    if (allHalted && u.engaged === undefined && ['hold', 'rest', 'startline'].includes(sq.phase)) {
      idle.n++;
      if (!aimed.length) idle.none++;
      if (aimed.length >= 2) { idle.aimed++; idle.spread += spread; if (new Set(aimed.map(aimKey)).size === 1) idle.same++; if (spread < 6) idle.near6++; }
    }
  }
}

const trace: string[] = [];
/** Where the longest stand of a whole platoon was (block, battle time). */
let longestAt = '';
const UPTO = Number(uptoS);
let step = 0;
while (!L.sim.over && L.sim.t < UPTO - 1e-6) {
  L.to(L.sim.t + 0.1, 100000);
  step++;
  const t = L.sim.t, half = step % 5 === 0;
  if (half) stepTwo();
  killStamp.clear();
  for (const r of L.drill.rounds()) {
    if (r.n <= seenRound) continue;
    seenRound = r.n;
    if (r.w === 'gun' && r.r === 'kill' && r.by !== undefined) gunKill.add(`${r.by}:${r.k}:${r.t.toFixed(1)}`);
    if (r.w !== 'tank' || r.by === undefined) continue;
    tankRounds++;
    if (r.r === 'kill') killStamp.add(`${r.by}:${r.k}`);
  }
  for (const [key, sq] of squads) {
    const u = sq.u;
    // (a battery's guns: every new shot stamp, each step)
    if (key >= 0 && u.kind === 'art' && !u.team && u.state !== 'gone' && u.state !== 'withdrawn') for (const m of sq.men) {
      if (m.role !== 'gun' || m.fell !== undefined || m.k >= u.figures || m.shot === undefined || gunShot.get(`${u.id}:${m.k}`) === m.shot) continue;
      gunShot.set(`${u.id}:${m.k}`, m.shot);
      let l = gunShots.get(u.id); if (!l) gunShots.set(u.id, (l = [])); l.push([m.shot, m.k, gunKill.has(`${u.id}:${m.k}:${m.shot.toFixed(1)}`)]);
    }
    if (key < 0 || sq.esc || sq.of !== undefined || u.kind !== 'arm') continue;
    const o = blockAt.get(u.id), bwent = o ? Math.hypot(u.x - o[0], u.y - o[1]) : 0;
    blockAt.set(u.id, [u.x, u.y]);
    const on = u.state !== 'gone' && u.state !== 'withdrawn';
    const veh = on ? sq.men.filter((m) => m.role === 'tank' && m.fell === undefined && m.k < u.figures) : [];
    const adv = ADVANCE.has(sq.phase), ground = adv && sq.net >= 2;
    const hx = sq.net > 0 ? sq.wx / sq.net : 0, hy = sq.net > 0 ? sq.wy / sq.net : 0;
    // A tank the rules took this step: no jump as she falls, and her wreck near the rules' own place for it.
    for (const m of sq.men) {
      const id = `${u.id}:${m.k}`, w = was.get(id);
      if (m.role !== 'tank' || m.fell === undefined || !w || w.pose === 'down') continue;
      lim.fell++;
      if (Math.hypot(m.x - w.x, m.y - w.y) >= 1.3) lim.jump++;
      const z = L.sim.wrecks.filter((x) => x.unit === u.id).sort((p, q) => q.at - p.at)[0];
      if (z && Math.hypot(m.x - z.x, m.y - z.y) >= Math.hypot(u.w, u.d) / 2 + 60) lim.wreckFar++;
      w.pose = 'down';
    }
    let standing = 0;
    for (const m of veh) {
      const id = `${u.id}:${m.k}`, w = was.get(id), rx = r1(m.x), ry = r1(m.y), went = m.went ?? 0;
      const still = HALTED.has(m.pose) && went < 0.05;
      if (still) standing++;
      let stood = 0, same = 0, go = 0, ran = 0;
      if (w) {
        // (the limits as the 3D game's checks read them: places rounded to 10 cm, facings as they are)
        const dist = Math.hypot(rx - w.rx, ry - w.ry), df = Math.abs(wrap(m.face - w.f));
        lim.steps++;
        lim.turnMax = Math.max(lim.turnMax, df); if (df > 0.08 + 0.02) lim.turn++;
        lim.stepMax = Math.max(lim.stepMax, dist);
        if (dist > 1.8 && dist > bwent + 0.05) lim.thrown++;
        if (dist > Math.max(1.2, bwent) + 0.15) lim.fast++;
        go = dist > 0.5 ? 1 : 0;
        if (go && w.go === -1) lim.stalls++;
        if (!go && w.go === 1 && dist < 0.05) go = -1;
        if (MOVING.has(m.pose) && r1(went) > 0.5 && dist < 0.005 && df < 0.005) { same = w.same + 1; if (same === 10) lim.onSpot++; }
        if (!MOVING.has(m.pose) && m.v > 0) lim.still++;
        stood = still ? w.stood + 0.1 : 0;
        // Backing while her block advances (a tank going back with her front to the enemy is real; one backing on with
        // her block, her rear to him, is not).
        if (adv) {
          const rev = m.pose === 'reverse' && went > 0.5, onward = rev && (m.x - w.x) * Math.sin(u.face) + (m.y - w.y) * Math.cos(u.face) > 0.5 * Math.hypot(m.x - w.x, m.y - w.y);
          const r = onward ? (backing.run.get(id) ?? 0) + 0.1 : 0;
          backing.adv++; if (rev) backing.rev++; if (onward) backing.on++;
          backing.run.set(id, r); if (r > backing.longest) { backing.longest = r; backing.at = `${id} at ${t.toFixed(1)} s`; }
        } else backing.run.set(id, 0);
        // Her halts while her block advances.
        ran = adv && still ? w.ran + 0.1 : 0;
        if (!ran && w.ran > 0) haltLens.push(w.ran);
        // A new shot stamp: her main gun (or the rules' kill round, drawn from her).
        if (m.shot !== undefined && m.shot !== w.shot && m.shot > t - 0.15) {
          const kill = killStamp.has(id), from1 = w.stood >= 1 - 1e-6;
          shots.all++;
          if (kill) shots.kills++;
          if (adv) { shots.adv++; if (sq.speed >= 3.5) shots.fast++; else shots.slow++; if (from1) shots.advHalt1++; if (!kill) { shots.advOwn++; if (from1) shots.advOwnHalt1++; } }
          if (w.stood > 0 || still) { shots.halted++; if (from1) shots.haltedHalt1++; } else shots.moving++;
          let l = byBlock.get(u.id); if (!l) byBlock.set(u.id, (l = [])); l.push([t, m.k]);
        }
      }
      if (!POSES.has(m.pose)) lim.badPose++;
      // The leash: the block's box and its margin (it turns with the block), and the ring round it (which does not).
      const dx = m.x - u.x, dy = m.y - u.y, a = dx * Math.cos(u.face) - dy * Math.sin(u.face), l = dx * Math.sin(u.face) + dy * Math.cos(u.face);
      const edge = (sq.phase === 'flee' ? 70 : 45) + 0.5;
      lim.samples++;
      if (Math.hypot(dx, dy) > Math.hypot(u.w / 2 + edge, u.d / 2 + edge)) lim.strayed++;
      if (Math.abs(a) > u.w / 2 + edge || Math.abs(l) > u.d / 2 + edge) { lim.outside++; if (!MOVING.has(m.pose)) lim.outStill++; if (outAt.length < 12 && !outAt.some((x) => x.startsWith(`${u.id}:${m.k} `))) outAt.push(`${u.id}:${m.k} at ${t.toFixed(1)} s (${sq.phase})`); }
      // Behind her place along her block's way, while it advances and makes ground.
      if (ground) {
        const [wa, wl] = wreckOffset(u.id, u.figures0 - 1 - m.k, u.w, u.d);
        const sx = u.x + wa * Math.cos(u.face) + wl * Math.sin(u.face), sy = u.y - wa * Math.sin(u.face) + wl * Math.cos(u.face);
        const b = (sx - m.x) * hx + (sy - m.y) * hy;
        if (half) behind.push(b);
        behindMax = Math.max(behindMax, b); offMax = Math.max(offMax, Math.hypot(sx - m.x, sy - m.y));
      }
      was.set(id, { x: m.x, y: m.y, f: m.face, rx, ry, stood, shot: m.shot, same, go, pose: m.pose, ran });
    }
    // The whole platoon standing together while its block advances (two tanks or more).
    if (veh.length >= 2 && adv) {
      allHalt.advSteps++;
      const run = standing === veh.length ? (allHalt.run.get(u.id) ?? 0) + 0.1 : 0;
      allHalt.run.set(u.id, run); if (run > 0) allHalt.steps++;
      if (run > allHalt.longest) { allHalt.longest = run; longestAt = `block ${u.id} at ${t.toFixed(1)} s, ${veh.length} tanks`; }
    } else allHalt.run.set(u.id, 0);
    if (veh.length >= 2 && ground) {
      allHalt.groundSteps++;
      const run = standing === veh.length ? (allHalt.runG.get(u.id) ?? 0) + 0.1 : 0;
      allHalt.runG.set(u.id, run); if (run > 0) allHalt.stepsGround++;
      allHalt.longestGround = Math.max(allHalt.longestGround, run);
    } else allHalt.runG.set(u.id, 0);
    if (!half) continue;
    if (traceS !== '' && u.id === Number(traceS) && t >= Number(fromS) && t <= Number(toS)) {
      const code: Record<string, string> = { drive: 'D', reverse: 'r', halt: 'h', fire: 'F' };
      trace.push(`${t.toFixed(1).padStart(6)} ${sq.phase.padEnd(8)} v${sq.speed.toFixed(1).padStart(5)} net${sq.net.toFixed(1).padStart(5)}  ` + sq.men.filter((m) => m.role === 'tank').map((m) => (m.fell !== undefined || m.k >= u.figures ? 'x    ' : (code[m.pose] ?? '?') + String(Math.round(wrap(m.face) * 180 / Math.PI)).padStart(4)) + (m.shot !== undefined && t - m.shot < 0.5 ? '*' : ' ')).join(' | ') + `   all standing ${(allHalt.run.get(u.id) ?? 0).toFixed(1)} s`);
    }
    for (const m of veh) {
      const id = `${u.id}:${m.k}`, p = lastPose.get(id), q = lastWay.get(id);
      poseTime.set(m.pose, (poseTime.get(m.pose) ?? 0) + 0.5);
      backing.halves++; if (p !== undefined && p !== m.pose) backing.changes++;
      if (MOVING.has(m.pose)) { if (q !== undefined && q !== m.pose) backing.turnabouts++; lastWay.set(id, m.pose); }
    }
    if (veh.length < 3) { for (const m of veh) lastPose.set(`${u.id}:${m.k}`, m.pose); continue; }
    // Rolling on fast, every tank driving: how often their hulls are all within 2 degrees (1 degree), and the longest run.
    if (adv && sq.speed >= 3.5 && veh.every((m) => m.pose === 'drive')) {
      const fs = veh.map((m) => m.face), mid = Math.atan2(fs.reduce((a, x) => a + Math.sin(x), 0), fs.reduce((a, x) => a + Math.cos(x), 0));
      const sp = Math.max(...fs.map((x) => Math.abs(wrap(x - mid)))) * 2 * 180 / Math.PI, r = sp <= 2 ? (par.run.get(u.id) ?? 0) + 0.5 : 0;
      par.n++; if (sp <= 2) par.two++; if (sp <= 1) par.one++;
      par.run.set(u.id, r); par.longest = Math.max(par.longest, r);
    } else par.run.set(u.id, 0);
    const kinds = new Set(veh.map((m) => m.pose));
    let ch = 0, rev = 0;
    for (const m of veh) {
      const id = `${u.id}:${m.k}`, w = lastPose.get(id);
      if (w !== undefined && w !== m.pose) { ch++; if (m.pose === 'reverse') rev++; }
      lastPose.set(id, m.pose);
    }
    const halted = veh.filter((m) => HALTED.has(m.pose)).length;
    for (const x of [A(sq.phase), A('ALL'), ...(adv ? [A('advance')] : [])]) {
      x.n++;
      if (kinds.size === 1) x.one++;
      if (ch === veh.length) x.flips++;
      if (rev === veh.length) x.toRev++;
      if (halted > 0 && halted < veh.length) x.mixed++;
    }
  }
}
const pct = (a: number, b: number): string => (b ? (100 * a / b).toFixed(1) : '-') + '%';
console.log(`${piece} seed ${seedS} to ${L.sim.t.toFixed(0)} s, mode ${mode}: armoured blocks, read every step and twice a battle second`);
console.log('phase       samples  one-pose  whole-block flips/min  all to reverse/min  some halted some moving');
const rows = [...acc.entries()].sort((a, c) => (a[0] === 'ALL' ? 1 : c[0] === 'ALL' ? -1 : c[1].n - a[1].n));
for (const [ph, a] of rows) console.log(`${ph.padEnd(11)} ${String(a.n).padStart(7)}  ${pct(a.one, a.n).padStart(8)}  ${(a.flips / (a.n / 120)).toFixed(2).padStart(21)}  ${(a.toRev / (a.n / 120)).toFixed(2).padStart(18)}  ${pct(a.mixed, a.n).padStart(23)}`);
const pt = [...poseTime.entries()].sort((p, q) => q[1] - p[1]), tot = pt.reduce((s, [, v]) => s + v, 0);
console.log('pose share of tank time: ' + pt.map(([p, v]) => `${p} ${pct(v, tot)}`).join(', '));
console.log(`main-gun shots: ${shots.all} (rounds drawn ${tankRounds}; the rules' kill rounds ${shots.kills}); from a tank standing ${shots.halted}, moving ${shots.moving}`);
console.log(`  on the advance: ${shots.adv} (her block going 3.5 m a second or more ${shots.fast}, slower ${shots.slow}), from a halt of a second or more ${pct(shots.advHalt1, shots.adv)}; her own shots (not kill rounds) ${shots.advOwn}, from such a halt ${pct(shots.advOwnHalt1, shots.advOwn)}`);
let near = 0, same = 0, n = 0;
for (const l of byBlock.values()) for (let i = 0; i < l.length; i++) {
  let g = Infinity;
  for (let j = 0; j < l.length; j++) if (j !== i && l[j][1] !== l[i][1]) g = Math.min(g, Math.abs(l[j][0] - l[i][0]));
  if (g === Infinity) continue;
  n++; if (g < 0.05) same++; if (g <= 0.5) near++;
}
console.log(`  a mate of her block fires in the same step ${pct(same, n)}, within half a second ${pct(near, n)}`);
haltLens.sort((a, b) => a - b);
const q = (l: number[], p: number): number => (l.length ? l[Math.floor((l.length - 1) * p)] : 0);
console.log(`halts while her block advances: ${haltLens.length}; median ${q(haltLens, 0.5).toFixed(1)} s, nine in ten under ${q(haltLens, 0.9).toFixed(1)} s`);
console.log(`backing while her block advances: ${pct(backing.rev, backing.adv)} of tank steps; backing the way her block faces, her rear to its enemy: ${pct(backing.on, backing.adv)}, longest ${backing.longest.toFixed(1)} s (${backing.at}); each tank a battle minute: ${(backing.changes / Math.max(1e-9, backing.halves / 120)).toFixed(1)} changes of pose, ${(backing.turnabouts / Math.max(1e-9, backing.halves / 120)).toFixed(2)} changes of direction (forward to backing or back)`);
console.log(`rolling on fast, every tank driving: hulls all within 2 degrees ${pct(par.two, par.n)} (within 1 degree ${pct(par.one, par.n)}) of ${par.n} half seconds, longest run ${par.longest.toFixed(1)} s`);
console.log(`whole platoon standing together while its block advances: longest ${allHalt.longest.toFixed(1)} s (${longestAt}; ${pct(allHalt.steps, allHalt.advSteps)} of advancing steps); while it makes 2 m a second or more: longest ${allHalt.longestGround.toFixed(1)} s (${pct(allHalt.stepsGround, allHalt.groundSteps)})`);
behind.sort((a, b) => a - b);
console.log(`behind her place while her block advances (making 2 m a second or more): furthest ${behindMax.toFixed(1)} m; 99 in 100 under ${q(behind, 0.99).toFixed(1)} m, median ${q(behind, 0.5).toFixed(1)} m; furthest from it any way ${offMax.toFixed(1)} m`);
console.log(`limits, ${lim.steps} tank steps: turned over 0.1 rad ${lim.turn} (most ${lim.turnMax.toFixed(3)}), step over the pace ${lim.fast} (${pct(lim.fast, lim.steps)}; longest ${lim.stepMax.toFixed(2)} m), thrown ${lim.thrown}, driving on the spot ${lim.onSpot}, still with a speed ${lim.still}, stalls ${lim.stalls} (${pct(lim.stalls, lim.steps)}), beyond the ring ${lim.strayed}, outside the leash ${lim.outside} (${pct(lim.outside, lim.samples)}; standing there ${lim.outStill}), poses unknown ${lim.badPose}, tanks lost ${lim.fell} (jumped ${lim.jump}, far from the rules' wreck ${lim.wreckFar})`);
const all = acc.get('ALL') ?? { n: 0, one: 0, flips: 0, toRev: 0, mixed: 0 }, ad = acc.get('advance') ?? { n: 0, one: 0, flips: 0, toRev: 0, mixed: 0 };
if (outAt.length) console.log('outside the leash, first at: ' + outAt.join(', '));
// Step two.
const WAIT = ['hold', 'rest', 'startline'], fzOf = (phs: string[]): [number, number] => phs.reduce<[number, number]>((a, ph) => { const f = frozen.get(ph) ?? [0, 0]; return [a[0] + f[0], a[1] + f[1]]; }, [0, 0]);
const fzWait = fzOf(WAIT), fzHold = fzOf(['hold']), fzFight = fzOf(['fight']);
console.log(`step two. gun off her hull by more than 6 degrees: standing ${pct(hull.halt6, hull.halt)} of ${hull.halt} tank half seconds with an aim (mean ${(hull.haltSum / Math.max(1, hull.halt)).toFixed(1)} degrees), on the move ${pct(hull.move6, hull.move)} of ${hull.move}`);
console.log(`  a halted platoon not fighting (hold, rest, start line; ${idle.n} samples): its aim points all one ${pct(idle.same, idle.aimed)}, its guns' bearings within 6 degrees ${pct(idle.near6, idle.aimed)} (mean spread ${(idle.spread / Math.max(1, idle.aimed)).toFixed(1)} degrees; of ${idle.aimed} with two aims or more; none aimed ${pct(idle.none, idle.n)})`);
console.log(`  frozen alike (all halted in one pose, hulls and guns within 6 degrees): waiting (hold, rest, start line) ${pct(fzWait[0], fzWait[1])} of ${fzWait[1]}; hold ${pct(fzHold[0], fzHold[1])} of ${fzHold[1]}; fight ${pct(fzFight[0], fzFight[1])} of ${fzFight[1]}`);
console.log(`  hull down: backed down behind her firing place ${post.sits} times, ${pct(post.down, post.halves)} of tank time (${pct(post.fightDown, post.fight)} of her time halted in a fight); halted tanks whose place has cover ${pct(post.covered, post.halted)}; shifts of post ${post.shifts} (${(post.shifts / Math.max(1e-9, post.halves / 120)).toFixed(2)} a tank minute); aim points changed ${(aimRows.changes / Math.max(1e-9, aimRows.halves / 120)).toFixed(1)} times a tank minute`);
// (her own shots apart from the rules' kill rounds, which are drawn from her the moment the rules take a model)
let gNear = 0, gSame = 0, gN = 0, gAll = 0, gKills = 0, oNear = 0, oN = 0;
for (const l of gunShots.values()) for (let i = 0; i < l.length; i++) {
  gAll++; if (l[i][2]) gKills++;
  let g = Infinity, o = Infinity;
  for (let j = 0; j < l.length; j++) if (j !== i && l[j][1] !== l[i][1]) { const d = Math.abs(l[j][0] - l[i][0]); g = Math.min(g, d); if (!l[j][2]) o = Math.min(o, d); }
  if (g === Infinity) continue;
  gN++; if (g < 0.05) gSame++; if (g <= 0.5) gNear++;
  if (!l[i][2] && o < Infinity) { oN++; if (o <= 0.5) oNear++; }
}
console.log(`  batteries: ${gAll} shots (the rules' kill rounds ${gKills}); another gun of her battery fires in the same step ${pct(gSame, gN)}, within half a second ${pct(gNear, gN)}; her own shots within half a second of another's own ${pct(oNear, oN)}`);
// (as one: of the half seconds in which any of a squad moved off or halted, the share of the squad that did so together)
const asOne = (g: Grp | undefined): number => (g ? +(100 * (g.startsShare + g.haltsShare) / Math.max(1, g.starts + g.halts)).toFixed(1) : -1);
for (const k of ['cars', 'aa', 'light', 'lorries']) {
  const g = groups.get(k);
  if (!g) { console.log(`  ${k}: none drawn`); continue; }
  console.log(`  ${k} (squads of two or more, ${g.n} samples): moved off in ${g.starts} half seconds, the whole squad in ${pct(g.startsAll, g.starts)} of them (${pct(g.startsShare, g.starts)} of it on average); halted in ${g.halts}, the whole squad in ${pct(g.haltsAll, g.halts)} (${pct(g.haltsShare, g.halts)} of it); all standing, hulls within 6 degrees ${pct(g.hullAlike, g.stood)}; aim points all one ${pct(g.aimSame, g.aimed)} of ${g.aimed}`);
}
for (const l of trace) console.log(l);
console.log(JSON.stringify({
  piece, seed: Number(seedS), mode, t: Math.round(L.sim.t), samples: all.n,
  onePose: +(100 * all.one / Math.max(1, all.n)).toFixed(1), flipsPerMin: +(all.flips / Math.max(1e-9, all.n / 120)).toFixed(2),
  advFlipsPerMin: +(ad.flips / Math.max(1e-9, ad.n / 120)).toFixed(2), toReversePerMin: +(all.toRev / Math.max(1e-9, all.n / 120)).toFixed(2),
  reverse: +(100 * (poseTime.get('reverse') ?? 0) / Math.max(1, tot)).toFixed(1),
  shots: shots.all, rounds: tankRounds, advShots: shots.adv, advFromHalt: +(100 * shots.advHalt1 / Math.max(1, shots.adv)).toFixed(1), advOwnFromHalt: +(100 * shots.advOwnHalt1 / Math.max(1, shots.advOwn)).toFixed(1),
  advBacking: +(100 * backing.rev / Math.max(1, backing.adv)).toFixed(1), advBackingOn: +(100 * backing.on / Math.max(1, backing.adv)).toFixed(1), longestBackingOn: +backing.longest.toFixed(1), parallel2: +(100 * par.two / Math.max(1, par.n)).toFixed(1), parallel1: +(100 * par.one / Math.max(1, par.n)).toFixed(1), parallelRun: par.longest, changesPerTankMin: +(backing.changes / Math.max(1e-9, backing.halves / 120)).toFixed(1), turnaboutsPerTankMin: +(backing.turnabouts / Math.max(1e-9, backing.halves / 120)).toFixed(2),
  allHalted: +allHalt.longest.toFixed(1), allHaltedMakingGround: +allHalt.longestGround.toFixed(1), behind: +behindMax.toFixed(1), behind99: +q(behind, 0.99).toFixed(1), limits: lim,
  hullOff6: +(100 * hull.halt6 / Math.max(1, hull.halt)).toFixed(1), hullOff6Moving: +(100 * hull.move6 / Math.max(1, hull.move)).toFixed(1),
  idleAimSame: +(100 * idle.same / Math.max(1, idle.aimed)).toFixed(1), idleAimNear6: +(100 * idle.near6 / Math.max(1, idle.aimed)).toFixed(1), idleSamples: idle.n,
  frozenWaiting: +(100 * fzWait[0] / Math.max(1, fzWait[1])).toFixed(1), frozenHold: +(100 * fzHold[0] / Math.max(1, fzHold[1])).toFixed(1), frozenFight: +(100 * fzFight[0] / Math.max(1, fzFight[1])).toFixed(1),
  sits: post.sits, sitDown: +(100 * post.down / Math.max(1, post.halves)).toFixed(1), sitDownInFight: +(100 * post.fightDown / Math.max(1, post.fight)).toFixed(1), covered: +(100 * post.covered / Math.max(1, post.halted)).toFixed(1), postShifts: post.shifts, aimRowsPerTankMin: +(aimRows.changes / Math.max(1e-9, aimRows.halves / 120)).toFixed(1),
  gunShots: gAll, gunsWithinHalf: +(100 * gNear / Math.max(1, gN)).toFixed(1), gunsSameStep: +(100 * gSame / Math.max(1, gN)).toFixed(1), gunsOwnWithinHalf: +(100 * oNear / Math.max(1, oN)).toFixed(1),
  asOne: { cars: asOne(groups.get('cars')), aa: asOne(groups.get('aa')), light: asOne(groups.get('light')), lorries: asOne(groups.get('lorries')) },
  units: L.sim.units.map((u) => [u.id, +u.str.toFixed(6), u.figures, u.state, +u.x.toFixed(3), +u.y.toFixed(3)]),
}));
