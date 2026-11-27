// The wounded meter (Brian, 25 November 2026, 16:14Z: "im not seeing multiple soliders being taken out by an explosion
// if they are in the blast radius"; 26 November 04:24Z: "the blast taking out multiple people or knocking others down
// and getting back up if they were close"; Drill.wounded). Plays a battle as the game ships it (the Battle rules card:
// shell blasts on, the bearers off), once with the switch off and once on, and reads every step of the drill: the
// killing bursts by the men each took, the dead and the wounded by what took them, the trail (seconds a man the rules
// have taken is still drawn on his feet, by the class of what took him), the knock-downs, the most of a block's men down
// at once and the seconds down per man-minute under shelling, the bursts shrunk, and the misses moved clear of men and
// those still landing inside a living man's lethal ring. Looks only: reads the drill, changes nothing, and the two
// battles must end the same.
//
// Every arm is the owner's field (the rulings, section 7): his Play launch's rows from the one shared gate tool
// (gates/gatehead.ts, '@gates/gatehead': landField), the switch under test and any rows ridden by name, seated by the
// seat rule (seatOf: the attacker on odd seeds, the defender on even); each output begins with the heads (gateHead,
// againstLaunch for every arm, launchDiffers for every off/on pair), the tree named by GATE_TREE.
//
//   npx esbuild tools/squad-life/wounded.ts --bundle --platform=node --format=esm --outfile=dist/wounded.mjs --log-level=warning \
//     --alias:@tc=./src --alias:@gates=/mnt/project-files/gates --loader:.json=json
//   GATE_TREE=<git rev-parse --short=9 HEAD of a clean tree> GATE_SHIPS=yes GATE_RIDES_SHIP=<ride>:yes,... node dist/wounded.mjs [piece seed] ... [unseated] [preview]
// (run from the tree's root: the head's knows is bridgeRows of the working directory)
// (default: the eight, Normandy 1944 seeds 11, 3, 12, 4 and Prokhorovka 1943 seeds 5, 13, 6, 14, and sample:plains 5
// and 6; a piece 'sample:plains', 'sample:forest' and the like: a sample ticket of that ground, the seed its own;
// 'unseated': nobody seated, me null; 'preview': the preview icon's rides on by name, BRIAN_LAUNCH.land.previewRides)
import { LiveBattle, livePiece, type LiveOpts } from '../../src/battle/land/live';
import { sampleTicket, withTeams } from '../../src/battle/land/sample';
import { DRILL, type Round } from '../../src/battle/land/drill';
import { BRIAN_LAUNCH, landField, seatOf, gateHead, againstLaunch, launchDiffers, type Head } from '@gates/gatehead';
import { bridgeRows } from '@gates/rowproof';

/** The seat of an arm: by the seat rule (seatOf), or nobody (null) for the unseated reads of the hard bars. */
export type Seat = 'att' | 'def' | null;

/**
 * One arm's options for LiveBattle on the owner's field (the rulings, section 7, THE ONE TOOL): every row of his Play
 * launch (landField), setup false, then the switch under test and each ride by name in sw, and the seat. The one
 * flattening of landField in this tree: the tests import it from here.
 */
export function armOpts(sw: Record<string, boolean>, seat: Seat): LiveOpts {
  const f = landField(sw, seat);
  return { ...f.rows, setup: f.setup, platoons: f.platoons, bocage: f.bocage, me: f.me } as LiveOpts;
}

/**
 * An arm's head (gateHead), the switch under test 'wounded' and its rides by name set on it. The battle is named as the
 * tool asks (gates/README.md, "Mend round", 27 November 04:53Z): a set piece opened: 'piece' with its key as battle; a
 * sample ticket, which is neither a piece nor a campaign ticket, opened: 'alone' with its name as battle; and the rows
 * this tree's land bridge forwards by the path the battle was opened by (knows: bridgeRows of the tree root, the
 * working directory the meter is run from; gates/README.md, "Second mend round": a piece by landStart, 'piece'; a
 * ticket by landStartTicket, 'campaign', the path a ticket is opened by, which a sample ticket also is).
 */
export function armHead(sw: Record<string, boolean>, seat: Seat, seed: number, rides: string[], piece: string): Head {
  const f = landField(sw, seat);
  const cwd = (globalThis as { process?: { cwd(): string } }).process?.cwd() ?? '.';
  const opened = piece.startsWith('sample:') ? 'alone' as const : 'piece' as const;
  const h = gateHead(null, { kind: 'land', land: { ...f, ticket: [], seed, opened, battle: piece, knows: bridgeRows(cwd, opened === 'piece' ? 'piece' : 'campaign') } });
  h.switch = ['wounded']; h.rides = [...rides].sort();
  return h;
}

/** The heads line of an output: each arm's head and againstLaunch, and each off/on pair's launchDiffers. */
export function headsLine(arms: Record<string, Head>, pairs: [string, string][]): string {
  return JSON.stringify({
    heads: Object.fromEntries(Object.entries(arms).map(([k, h]) => [k, { head: h, againstLaunch: againstLaunch(h) }])),
    pairs: Object.fromEntries(pairs.map(([a, b]) => [`${a}|${b}`, launchDiffers(arms[a], arms[b], ['wounded', 'stunned'])])),
  });
}

interface M { face?: number; went?: number; rolled?: number; stun?: { s: number; n: number; x: number; y: number }; k: number; j?: number; n: number; role: string; pose: string; x: number; y: number; cover: string; fell?: number; wd?: number; kd?: [number, number, number]; job?: unknown; lv?: number; pr?: true; sec?: number }
interface Sq { u: { id: number; side: string; kind: string; str: number; str0: number; figures: number; state: string; cal?: string }; men: M[]; m0: number; shelledAt: number; esc?: string }
interface Drilled {
  squads: Map<number, Sq>;
  hurtLog?: { bursts: { t: number; x: number; y: number; n: number; cal: string; men: number; shrunk: boolean; key: boolean; later?: number }[]; moved: number; stuck: number; he: number[]; trail: Record<string, number[]>; stun?: { marked: number; taken: number; late: number[] } };
  takeStunned?(sq: Sq, by: { id: number }, at: number): M | null;
  far(m: M): number; stands(sq: Sq, m: M): boolean; knocked(m: M): boolean; step(): void; rounds(): unknown[];
}

const NOT_FOOT = new Set(['tank', 'gun', 'lorry', 'hmg', 'mortar']);
const RIDING = new Set(['ride', 'mounted', 'horses']);
const LYING = new Set(['fire_prone', 'hide', 'crawl', 'observe', 'cower', 'lmg_burst']);
const HE = new Set(['tank', 'gun', 'mortar']);
const ARMS = new Set(['rifle', 'mg', 'grenade', 'at']);
const RING = DRILL.wounded.ring as Record<string, [number, number, number]>;
const r1 = (v: number): number => Math.round(v * 10) / 10;
const pct = (a: number, b: number): string => (b ? `${((100 * a) / b).toFixed(1)}%` : '-');
const med = (a: number[]): number => { if (!a.length) return NaN; const s = [...a].sort((x, y) => x - y); return s[Math.floor(s.length / 2)]; };

/** How far a burst finds a man, as the drill scores him (drill.ts far, exposed). */
const farOf = (m: M): number => {
  const cover = (DRILL.rounds.cover as Record<string, number>)[m.cover] ?? 1;
  const ex = LYING.has(m.pose) ? DRILL.blast.lying : m.pose === 'fire_kneel' || m.pose === 'wait' || m.pose === 'creep' ? DRILL.blast.kneeling : 1;
  return (2 - cover) / ex;
};

export interface Read {
  sig: string; t: number;
  sizes: number[]; dead: Record<string, number>; hurt: Record<string, number>;
  trail: Record<string, number[]>; kd: number; kdMen: number; worst: { share: number; t: number; id: number; men: number }; downSecs: number; shelledManSecs: number;
  bursts: number; shrunk: number; keyBursts: number; moved: number; stuck: number; missIn: number; missInReal: number; heMisses: number; burstIn: number; laid: number; holds: number[];
  /** The design director's bar (d): booked models, drawn key-model kills and drawn men's kills by striker > struck; (c): the most of a block drawn down ahead of the rules' count (key men too), and the drill's own trail by class (on). */
  pairs: { booked: Record<string, number>; keys: Record<string, number>; men: Record<string, number> }; over: number; own: Record<string, number[]>;
  /** Stunned men (rulings section 4): marked, taken, risen, taken men whose kill round is not the stunning burst's mate from its striker in the think he falls (wrong: must be 0), seconds each was taken after the rules booked the loss (late: the base's own hold); the men each laid burst ENDED with; moments of two or more; and, data only, the shell losses no stunned man was there for while a man of the block lay knocked down by a MISS of that striker. */
  stun: { marked: number; taken: number; risen: number; wrong: number; late: number[]; ended: number[]; moments: string[]; missCould: number; noneThere: number; lt: number };
  /**
   * The rulings, section 5.2: each block's men down beyond the rules' count (key men too), by the tenth of a second
   * ('t|block'), and the seconds over by block; 5.7: each man's seconds inside a kd window in his busiest minute (the
   * most, and how many men had 20 s or more), and the largest share of a block's living section men (8 or more) inside
   * a kd window at one moment.
   */
  overAt: Map<string, number>; overSecs: Record<string, number>; busiest: { secs: number; who: string; t: number; downs: number }; men20: number; secShare: { share: number; t: number; id: number; men: number };
  /** 5.8: steps a man inside his kd window lay under a moving tank or lorry (inside her nose, kd.nose metres of her middle along her, and the rules' run-over half width across, escort.clear.crushHalf), and the men rolled clear of one. */
  under: number; underWho: string[]; rolled: number;
}

/** A battle as the meter plays it: a set piece by its key, or a sample ticket ('sample:' and its ground) of that seed. */
export function open(piece: string, seed: number, opts: LiveOpts): LiveBattle {
  if (piece.startsWith('sample:')) return new LiveBattle(withTeams(sampleTicket(piece.slice(7) as Parameters<typeof sampleTicket>[0], 1.1, { seed, nat: { att: 'RED', def: 'BLU' }, humanAtt: false, date: 'June 1944' })), null, opts);
  return livePiece(piece, seed, opts);
}

/** Plays one battle to its end on his field (armOpts: the seat, wounded and stunned by name, and any rides) and reads it. */
export function measure(piece: string, seed: number, wounded: boolean, stunned = true, seat: Seat = seatOf(seed), rides: Record<string, boolean> = {}): Read {
  const L = open(piece, seed, armOpts({ ...rides, wounded, ...(stunned ? {} : { stunned: false }) }, seat));
  const d = L.drill as unknown as Drilled, sim = L.sim;
  const R: Read = { under: 0, underWho: [], rolled: 0, overAt: new Map(), overSecs: {}, busiest: { secs: 0, who: '', t: 0, downs: 0 }, men20: 0, secShare: { share: 0, t: 0, id: 0, men: 0 }, stun: { marked: 0, taken: 0, risen: 0, wrong: 0, late: [], ended: [], moments: [], missCould: 0, noneThere: 0, lt: 0 }, pairs: { booked: {}, keys: {}, men: {} }, over: 0, own: {}, sig: '', t: 0, sizes: [], dead: {}, hurt: {}, trail: {}, kd: 0, kdMen: 0, worst: { share: 0, t: 0, id: 0, men: 0 }, downSecs: 0, shelledManSecs: 0, bursts: 0, shrunk: 0, keyBursts: 0, moved: 0, stuck: 0, missIn: 0, missInReal: 0, heMisses: 0, burstIn: 0, laid: 0, holds: [] };
  // Each block's losses: the battle time each was booked by the rules, oldest first, still to be drawn down.
  const owed = new Map<Sq, number[]>(), downs = new Map<Sq, number>();
  const seenKd = new Map<M, string>(), kdMen = new Set<M>();
  const pending = new Map<number, Round & { n: number }>();
  let burstsSeen = 0;
  const allRounds: (Round & { n: number })[] = [];
  const seenN = new Set<number>();
  const calOf = (r: Round): string => (HE.has(r.w) ? (r.by !== undefined ? (sim.unit(r.by)?.cal ?? (r.w === 'mortar' ? 'mortar' : 'field')) : 'field') : 'arms');
  // (the bar (d): the rules' booked models by striker > struck, as count3 reads them)
  const add = (o: Record<string, number>, k: string): void => { o[k] = (o[k] ?? 0) + 1; };
  const sm = sim as unknown as { shell: (by: { id: number }, t: { id: number }, k: number, w: unknown) => unknown };
  const dd = d as unknown as { struckBy: (by: number, t: number, k: number) => unknown; fromAir: (t: number, k: number) => unknown };
  const sh = sm.shell.bind(sm), sb = dd.struckBy.bind(dd), fa = dd.fromAir.bind(dd);
  sm.shell = (by, t, k, w) => { add(R.pairs.booked, `${by.id}>${t.id}`); return sh(by, t, k, w); };
  dd.struckBy = (by, t, k) => { add(R.pairs.booked, `${by}>${t}`); return sb(by, t, k); };
  dd.fromAir = (t, k) => { add(R.pairs.booked, `air>${t}`); return fa(t, k); };
  // (data only: a shell loss with no stunned man of its striker to take; was a man of the block lying knocked down by
  // a round of that striker that took no one?)
  const byRound = new Map<number, { by?: number; r?: string }>();
  if (d.takeStunned) {
    const ts = d.takeStunned.bind(d);
    d.takeStunned = (sq, by, at) => {
      const ok = ts(sq, by, at);
      // (his kill round, booked as he is taken: the stunning burst's mate, at its point, from its striker)
      if (ok) {
        const r = (d.rounds() as (Round & { n: number })[]).filter((x) => x.r === 'kill' && x.on === sq.u.id && x.ok === ok.k && x.oj === ok.j).pop();
        if (!r || r.by !== ok.stun!.s || r.m !== 1 || r.lt !== ok.stun!.n || r.t !== Math.round(sim.t * 10) / 10 || Math.abs(r.x - Math.round(ok.stun!.x * 10) / 10) > 0.051 || Math.abs(r.y - Math.round(ok.stun!.y * 10) / 10) > 0.051) R.stun.wrong++;
      }
      if (!ok) {
        R.stun.noneThere++;
        if (sq.men.some((m) => m.fell === undefined && m.j !== undefined && d.knocked(m) && m.kd && byRound.get(m.kd[1])?.by === by.id && byRound.get(m.kd[1])?.r === 'miss')) R.stun.missCould++;
      }
      return ok;
    };
  }
  // (each man's kd windows, [from, to, round], one window drawn out from its own start kept as one)
  const wins = new Map<M, [number, number, number][]>(), who = new Map<M, string>();
  const orig = d.step.bind(d);
  d.step = (): void => {
    orig();
    const t = sim.t;
    // (the high-explosive misses: those the drill names so with the switch on; off, every gun's, mortar's and tank's miss at a block of foot)
    const he = new Set(d.hurtLog?.he ?? []);
    for (const r of d.rounds() as (Round & { n: number })[]) if (!seenN.has(r.n)) {
      seenN.add(r.n); allRounds.push(r); byRound.set(r.n, { by: r.by, r: r.r });
      if (r.lt !== undefined) R.stun.lt++;
      if (r.r === 'kill') add(r.oj === undefined ? R.pairs.keys : R.pairs.men, `${r.by ?? 'none'}>${r.on}`);
      if (r.r === 'miss' && HE.has(r.w) && (wounded ? he.has(r.n) : r.w === 'mortar' || sim.unit(r.on)?.kind !== 'arm')) pending.set(r.n, r);
    }
    const living: [M, Sq][] = [];
    for (const sq of d.squads.values()) {
      const on = sq.u.state !== 'gone' && sq.u.state !== 'withdrawn';
      for (const m of sq.men) if (on && d.stands(sq, m) && !NOT_FOOT.has(m.role) && !RIDING.has(m.pose)) living.push([m, sq]);
    }
    // HE misses as they land: any living man inside its lethal ring then?
    for (const [n, r] of pending) {
      if (r.l > t + 1e-6) continue;
      pending.delete(n);
      R.heMisses++;
      const L0 = RING[calOf(r)]?.[0] ?? 4;
      if (living.some(([m]) => Math.hypot(m.x - r.x, m.y - r.y) * farOf(m) < L0 - 1e-6)) R.missIn++;
      if (living.some(([m]) => Math.hypot(m.x - r.x, m.y - r.y) < L0 - 1e-6)) R.missInReal++;
    }
    // Bursts laid this step (on): any living man inside the lethal ring of one not shrunk?
    const log = d.hurtLog;
    if (log) for (; burstsSeen < log.bursts.length; burstsSeen++) {
      const bu = log.bursts[burstsSeen];
      if (bu.shrunk) continue;
      R.laid++;
      if (living.some(([m]) => Math.hypot(m.x - bu.x, m.y - bu.y) * farOf(m) < RING[bu.cal][0] - 1e-6)) R.burstIn++;
    }
    // (5.8: a man inside his kd window under a moving vehicle)
    const movers: M[] = [];
    for (const vq of d.squads.values()) if (vq.u.state !== 'gone' && vq.u.state !== 'withdrawn') for (const v of vq.men) if ((v.role === 'tank' || v.role === 'lorry') && v.fell === undefined && (v.went ?? 0) > 0.3) movers.push(v);
    if (movers.length) for (const sq of d.squads.values()) for (const m of sq.men) {
      if (!m.kd || m.fell !== undefined || !d.knocked(m)) continue;
      for (const v of movers) {
        const dx = m.x - v.x, dy = m.y - v.y, f = v.face ?? 0, a = dx * Math.sin(f) + dy * Math.cos(f), l = dx * Math.cos(f) - dy * Math.sin(f);
        if (Math.hypot(dx, dy) < 4 && Math.abs(a) <= DRILL.wounded.kd.nose && Math.abs(l) <= DRILL.escort.clear.crushHalf) { R.under++; if (R.underWho.length < 4) R.underWho.push(`${sq.u.id}:${m.k}.${m.j ?? -1} t=${r1(t)}`); break; }
      }
    }
    for (const sq of d.squads.values()) {
      if (sq.esc) continue;
      const u = sq.u;
      const want = u.str <= 0 ? 0 : Math.ceil((sq.m0 * u.str) / Math.max(1e-9, u.str0) - 1e-9);
      let up = 0, live = 0, knocked = 0, sec = 0, secK = 0;
      for (const m of sq.men) {
        if (d.stands(sq, m) || (m.j !== undefined && m.fell === undefined)) up++;
        if (d.stands(sq, m) && !NOT_FOOT.has(m.role) && !RIDING.has(m.pose)) { live++; if (d.knocked(m)) knocked++; }
        if (m.j !== undefined && m.fell === undefined && d.stands(sq, m) && !NOT_FOOT.has(m.role)) { sec++; if (d.knocked(m)) secK++; }
        if (m.kd) { const w = wins.get(m) ?? []; const e = Math.min(m.kd[0] + m.kd[2], m.fell ?? Infinity); if (!w.length || w[w.length - 1][0] !== m.kd[0] || w[w.length - 1][2] !== m.kd[1]) w.push([m.kd[0], e, m.kd[1]]); else w[w.length - 1][1] = e; wins.set(m, w); who.set(m, `${u.id}:${m.k}.${m.j ?? -1}`); }
        if (m.kd) { const key = m.kd.join(','); if (seenKd.get(m) !== key) { if (!seenKd.has(m) || seenKd.get(m)!.split(',')[0] !== String(m.kd[0])) R.kd++; seenKd.set(m, key); kdMen.add(m); R.holds.push(m.kd[2]); } }
      }
      // (the trail: the rules' booked losses, each drawn down later)
      const q = owed.get(sq) ?? [], booked = sq.m0 - want, drawn = sq.m0 - up;
      const was = (downs.get(sq) ?? 0) + q.length;
      for (let i = was; i < booked; i++) q.push(t);
      let gone = downs.get(sq) ?? 0;
      while (gone < drawn && q.length) {
        const at = q.shift()!;
        gone++;
        // its class: the round that took the man drawn down now
        const r = allRounds.find((x) => x.on === u.id && x.r === 'kill' && Math.abs(x.t - t) < 0.06);
        const cls = r ? calOf(r) : 'other';
        (R.trail[cls] ??= []).push(t - at);
      }
      downs.set(sq, gone);
      owed.set(sq, q);
      R.over = Math.max(R.over, want - up);
      if (want - up > 0) { R.overAt.set(`${Math.round(t * 10)}|${u.id}`, want - up); R.overSecs[u.id] = (R.overSecs[u.id] ?? 0) + 0.1; }
      if (sec >= 8 && secK / sec > R.secShare.share) R.secShare = { share: secK / sec, t: r1(t), id: u.id, men: sec };
      if (live >= 6 && knocked / live > R.worst.share) R.worst = { share: knocked / live, t: r1(t), id: u.id, men: live };
      if (t - sq.shelledAt <= 10) { R.shelledManSecs += live * 0.1; R.downSecs += knocked * 0.1; }
    }
  };
  while (!sim.over) L.tick(1, 50);
  R.t = r1(sim.t);
  R.sig = JSON.stringify({ t: sim.t, over: sim.over, units: sim.units.map((u) => [u.id, u.figures, u.str, u.state]) });
  // Killing bursts: an HE kill round and its mates (m: 1) at the same t, x and y.
  const bursts = new Map<string, number>();
  for (const r of allRounds) if (r.r === 'kill' && HE.has(r.w)) { const k = `${r.on}|${r.x}|${r.y}`; bursts.set(k, (bursts.get(k) ?? 0) + 1); }
  R.sizes = [...bursts.values()];
  // Dead and wounded, by what took them.
  const cause = new Map<string, string>();
  for (const r of allRounds) if (r.r === 'kill') cause.set(`${r.on}:${r.ok}:${r.oj ?? -1}`, HE.has(r.w) ? 'burst' : ARMS.has(r.w) ? 'arms' : r.w);
  for (const sq of d.squads.values()) for (const m of sq.men) {
    if (m.fell === undefined || NOT_FOOT.has(m.role)) continue;
    const c = cause.get(`${sq.u.id}:${m.k}:${m.j ?? -1}`) ?? 'none';
    if (m.wd !== undefined) R.hurt[c] = (R.hurt[c] ?? 0) + 1; else R.dead[c] = (R.dead[c] ?? 0) + 1;
  }
  R.kdMen = kdMen.size;
  for (const sq of d.squads.values()) for (const m of sq.men) if (m.rolled !== undefined) R.rolled++;
  // (each man's busiest minute: the seconds inside his kd windows in the 60 s from each window's start)
  for (const [m, w0] of wins) {
    const w = [...w0].sort((a, b) => a[0] - b[0]), iv: [number, number][] = [];
    for (const [a, b] of w) { if (iv.length && a <= iv[iv.length - 1][1]) iv[iv.length - 1][1] = Math.max(iv[iv.length - 1][1], b); else iv.push([a, b]); }
    let best = 0, at = 0, downs = 0;
    for (const [s0] of w) {
      let sum = 0;
      for (const [a, b] of iv) sum += Math.max(0, Math.min(b, s0 + 60) - Math.max(a, s0));
      if (sum > best) { best = sum; at = s0; downs = w.filter((x) => x[0] >= s0 && x[0] < s0 + 60).length; }
    }
    if (best >= 20) R.men20++;
    if (best > R.busiest.secs) R.busiest = { secs: r1(best), who: who.get(m) ?? '', t: r1(at), downs };
  }
  if (d.hurtLog && wounded) R.own = d.hurtLog.trail;
  if (d.hurtLog?.stun && wounded) {
    const st = d.hurtLog.stun;
    R.stun.marked = st.marked; R.stun.taken = st.taken; R.stun.late = st.late;
    for (const sq of d.squads.values()) for (const m of sq.men) if (m.stun && m.kd && (m.fell === undefined || m.fell > m.kd[0] + m.kd[2] + 1e-6)) R.stun.risen++;
    for (const b of d.hurtLog.bursts) {
      const men = b.men + (b.later ?? 0);
      R.stun.ended.push(men);
      if (men >= 2) R.stun.moments.push(`t=${b.t} x=${b.x} y=${b.y} ${b.cal} ${b.men}+${b.later ?? 0}`);
    }
  }
  if (d.hurtLog) { R.bursts = d.hurtLog.bursts.length; R.shrunk = d.hurtLog.bursts.filter((b) => b.shrunk).length; R.keyBursts = d.hurtLog.bursts.filter((b) => b.key).length; R.moved = d.hurtLog.moved; R.stuck = d.hurtLog.stuck; }
  return R;
}

export function print(piece: string, seed: number, w: boolean, R: Read): string {
  const s = R.sizes, n = (k: number): number => s.filter((x) => (k < 4 ? x === k : x >= 4)).length;
  const causes = [...new Set([...Object.keys(R.dead), ...Object.keys(R.hurt)])].sort();
  const dw = causes.map((c) => `${c} ${R.dead[c] ?? 0}/${R.hurt[c] ?? 0}`).join(', ');
  const td = Object.values(R.dead).reduce((a, b) => a + b, 0), tw = Object.values(R.hurt).reduce((a, b) => a + b, 0);
  const trail = Object.entries(R.trail).sort().map(([c, a]) => `${c} ${med(a).toFixed(1)}/${Math.max(...a).toFixed(1)} s (${a.length})`).join(', ');
  return [
    `${piece} seed ${seed} wounded ${w ? 'on ' : 'off'} (ends t=${R.t})`,
    `  killing bursts by men taken: 1: ${n(1)}, 2: ${n(2)}, 3: ${n(3)}, 4+: ${n(4)} (most ${s.length ? Math.max(...s) : 0})`,
    `  dead/wounded by cause: ${dw}; all ${td}/${tw} (${td ? (tw / td).toFixed(2) : '-'} wounded to a dead)`,
    `  trail median/longest by class (read from outside, first booked to first drawn): ${trail}`,
    ...(w ? [`  trail median/longest by class (the drill's own, booked to drawn; allowed: the class's hold ${JSON.stringify(DRILL.wounded.hold)} and a think 0.5): ${Object.entries(R.own).sort().map(([c, a]) => `${c} ${med(a).toFixed(1)}/${Math.max(...a).toFixed(1)} s (${a.length})`).join(', ')}`] : []),
    `  bar (c): most of a block drawn down ahead of the rules' count ${R.over} (a key man falls as his model is lifted); bar (d): booked ${Object.values(R.pairs.booked).reduce((a, b) => a + b, 0)}, drawn key kills ${Object.values(R.pairs.keys).reduce((a, b) => a + b, 0)}, drawn men's kills ${Object.values(R.pairs.men).reduce((a, b) => a + b, 0)}`,
    `  kd: ${R.kd} knock-downs on ${R.kdMen} men; hold median ${med(R.holds).toFixed(1)} s, least ${R.holds.length ? Math.min(...R.holds).toFixed(1) : '-'}; worst share of a block down at once ${(100 * R.worst.share).toFixed(0)}% (block ${R.worst.id}, ${R.worst.men} men, t=${R.worst.t}); seconds down per man-minute under shelling ${R.shelledManSecs ? ((60 * R.downSecs) / R.shelledManSecs).toFixed(2) : '-'}`,
    `  bursts laid ${R.bursts} (key-man events ${R.keyBursts}), shrunk ${R.shrunk} (${pct(R.shrunk, R.bursts)}); laid with a living man inside the lethal ring ${R.burstIn}/${R.laid}`,
    ...(w ? [`  stunned: marked ${R.stun.marked}, taken ${R.stun.taken}, risen ${R.stun.risen}; kill round not the stunning burst's own at his fall ${R.stun.wrong}; taken after the rules booked the loss (the base's draw) median/longest ${med(R.stun.late).toFixed(1)}/${R.stun.late.length ? Math.max(...R.stun.late).toFixed(1) : '-'} s`,
      `  men each laid burst ENDED with (at landing + stunned men taken later): 1: ${R.stun.ended.filter((x) => x === 1).length}, 2: ${R.stun.ended.filter((x) => x === 2).length}, 3: ${R.stun.ended.filter((x) => x === 3).length}, 4+: ${R.stun.ended.filter((x) => x >= 4).length}`,
      `  bursts ending with two or more: ${R.stun.moments.slice(0, 6).join(' | ') || 'none'}`,
      `  data only: shell losses with no stunned man of their striker lying ${R.stun.noneThere}, of them with a man of the block lying knocked down by a round of that striker that took no one ${R.stun.missCould} (no odds or cap applied: an upper bound)`] : []),
    ...(w ? [`  kill rounds marked a late fall of an earlier burst (lt) ${R.stun.lt}; busiest man in a minute ${R.busiest.secs} s down from ${R.busiest.downs} knock-downs (${R.busiest.who}, from t=${R.busiest.t}); men 20 s or more down in some minute ${R.men20}; largest share of a block's living section men (8 or more) in a kd window at once ${(100 * R.secShare.share).toFixed(1)}% (block ${R.secShare.id}, ${R.secShare.men} men, t=${R.secShare.t})`] : []),
    ...(w ? [`  5.8: steps a man inside his kd window lay under a moving tank or lorry ${R.under}${R.underWho.length ? ' (' + R.underWho.join(', ') + ')' : ''}; men rolled clear of one ${R.rolled}`] : []),
    `  seconds over the rules' count, all blocks (key men too): ${Object.values(R.overSecs).reduce((a, b) => a + b, 0).toFixed(1)}`,
    `  HE misses ${R.heMisses}: moved clear ${R.moved}, no clear place ${R.stuck}; landing with a living man inside its lethal ring ${R.missIn} (${pct(R.missIn, R.heMisses)}; by plain metres ${R.missInReal})`,
  ].join('\n');
}

/** The eight (the rulings, 7.2: each declared battle and its mirror at seed + 1) and the two sample tickets that are not set pieces (7.3). */
export const EIGHT: [string, number][] = [['normandy-1944', 11], ['normandy-1944', 3], ['normandy-1944', 12], ['normandy-1944', 4], ['prokhorovka-1943', 5], ['prokhorovka-1943', 13], ['prokhorovka-1943', 6], ['prokhorovka-1943', 14]];
export const PLAINS: [string, number][] = [['sample:plains', 5], ['sample:plains', 6]];

/** Run straight (node dist/wounded.mjs ...), not when a test imports this file. */
const argv: string[] = (globalThis as { process?: { argv: string[] } }).process?.argv ?? [];
if (/wounded\.(mjs|ts|js)$/.test(argv[1] ?? '')) {
  const args = argv.slice(2).filter((a) => a !== 'unseated' && a !== 'preview');
  const unseated = argv.includes('unseated'), preview = argv.includes('preview');
  const rides: Record<string, boolean> = preview ? Object.fromEntries(BRIAN_LAUNCH.land.previewRides.map((k) => [k, true])) : {};
  const rideNames = Object.keys(rides);
  const runs: [string, number][] = [];
  for (let i = 0; i + 1 < args.length; i += 2) runs.push([args[i], Number(args[i + 1])]);
  if (!runs.length) runs.push(...EIGHT, ...PLAINS);
  for (const [p, s] of runs) {
    const seat: Seat = unseated ? null : seatOf(s);
    console.log(headsLine({
      off: armHead({ ...rides, wounded: false }, seat, s, rideNames, p), on: armHead({ ...rides, wounded: true }, seat, s, rideNames, p),
      offS: armHead({ ...rides, wounded: false, stunned: false }, seat, s, [...rideNames, 'stunned'], p), sf: armHead({ ...rides, wounded: true, stunned: false }, seat, s, [...rideNames, 'stunned'], p),
    }, [['off', 'on'], ['offS', 'sf']]));
    console.log(`${p} seed ${s}: seat ${seat ?? 'nobody'}${rideNames.length ? `, rides ${rideNames.join('+')}` : ''} (offS: the off arm read with stunned: false by name; stunned acts only with wounded on, so it is the off run)`);
    const off = measure(p, s, false, true, seat, rides), sf = measure(p, s, true, false, seat, rides), on = measure(p, s, true, true, seat, rides);
    console.log(print(p, s, false, off));
    console.log(print(p, s, true, sf).replace('wounded on ', 'wounded on, stunned: false '));
    console.log(print(p, s, true, on));
    const same3 = (k: 'booked' | 'keys' | 'men'): string => JSON.stringify(Object.entries(sf.pairs[k]).sort()) === JSON.stringify(Object.entries(off.pairs[k]).sort()) ? 'yes' : 'NO';
    console.log(`  stunned: false against off: the same battle ${off.sig === sf.sig ? 'yes' : 'NO'}; pairs booked ${same3('booked')}, key kills ${same3('keys')}, men's kills ${same3('men')}; drawn ahead ${sf.over} <= ${off.over}`);
    console.log(`  the same battle on and off: ${off.sig === on.sig ? 'yes' : 'NO'}`);
    // (the rulings, 5.2: block by block, never more men down beyond the count on than off at the same moment)
    for (const [nm, x] of [['stunned: false', sf], ['on', on]] as const) {
      const worse = [...x.overAt].filter(([k, v]) => v > (off.overAt.get(k) ?? 0)), secs = Object.values(x.overSecs).reduce((a, b) => a + b, 0) - Object.values(off.overSecs).reduce((a, b) => a + b, 0);
      console.log(`  count (5.2), ${nm} against off: seconds over the count on minus off ${secs.toFixed(1)}; tenths of a second with a block more over than off ${worse.length}${worse.length ? ': ' + worse.slice(0, 4).map(([k, v]) => `${k} ${v}`).join(' ') : ''}`);
    }
    const same = (k: 'booked' | 'keys' | 'men'): string => { const a = off.pairs[k], b = on.pairs[k], diff = [...new Set([...Object.keys(a), ...Object.keys(b)])].filter((x) => (a[x] ?? 0) !== (b[x] ?? 0)).map((x) => `${x} ${a[x] ?? 0}/${b[x] ?? 0}`); return diff.length ? `NO (${diff.join(' ')})` : 'yes'; };
    console.log(`  bar (d) the same pairs on and off: booked ${same('booked')}, key kills ${same('keys')}, men's kills ${same('men')}; bar (c) drawn ahead on ${on.over} <= off ${off.over}: ${on.over <= off.over ? 'yes' : 'NO'}`);
  }
}
