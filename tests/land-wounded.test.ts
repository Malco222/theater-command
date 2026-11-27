// Wounded and knocked-down men (release 2; Brian, 25 November 2026, 16:14Z: "im not seeing multiple soliders being taken
// out by an explosion if they are in the blast radius or the limbs being blown off for realism"; 26 November 04:24Z: he
// still does not see "the blast taking out multiple people or knocking others down and getting back up if they were
// close"). Drill.wounded, looks only (ruling 217): a block's losses to shells wait for one burst, laid where it takes
// the men it is owed and no other man's life; the nearest lie dead and those further out wounded (with the bearers rule
// off a share of every block's fallen lie wounded for good); its misses land clear of men; and the living men round
// every high-explosive landing are knocked down or duck, each held a few seconds of his own, and get up again (kd).
//
// What must hold, on the owner's field (the rulings, section 7: his Play launch's rows from the one shared gate tool,
// gates/gatehead.ts, flattened once in the meter, armOpts; the switch and any rides by name; the attacker seated on odd
// seeds and the defender on even), on the eight (Normandy 1944 seeds 11, 3, 12, 4; Prokhorovka 1943 seeds 5, 13, 6,
// 14), two sample plains tickets that are not set pieces (seeds 5 and 6) and one campaign ticket: off, every byte the drill sends is the base's (WOUNDED_BASE: the base tree to
// compare with; without it, off is held to the switch left out); on, the battle is the same to the last figure; the men
// drawn down never run ahead of the rules' count, nor behind it longer than a shell's hold and a think; the wounded lie
// for good; a burst takes two, three or four men, with no other living man inside its lethal ring as it lands, its dead
// inside that ring and its wounded outside; a miss lands clear of men; and every man knocked down was alive, on his
// feet and inside the stun ring, holds still and fires nothing for his own hold of 2.4 s or more, and goes on after it.
//
//   WOUNDED_BASE=/path/to/base npx vitest run tests/land-wounded.test.ts --testTimeout=900000
// (on a loaded box, one battle at a time: -t "normandy-1944 11", and so on, then -t "across" and -t "salts")
import { describe, expect, it } from 'vitest';
import { LiveBattle, livePiece, type LiveOpts } from '../src/battle/land/live';
import { sampleTicket, withTeams } from '../src/battle/land/sample';
import { DRILL, type MenBlock, type MenOut, type Round } from '../src/battle/land/drill';
import type { FieldTicket } from '../src/battle/core/types';
import type { LandPlace } from '../src/battle/land/field';
import type { LandBattle } from '../src/battle/land/sim';
import drillText from '../src/battle/land/drill.ts?raw';
import { armOpts, EIGHT, PLAINS, type Seat } from '../tools/squad-life/wounded';
import { BRIAN_LAUNCH, seatOf } from '@gates/gatehead';

/** A running hash of what is fed to it (two FNV-1a lanes): enough to tell two runs apart. */
function hasher(): { update(s: string): void; digest(): string } {
  let a = 0x811c9dc5, b = 0x01000193 ^ 0x5bd1e995, n = 0;
  return {
    update(s: string) {
      for (let i = 0; i < s.length; i++) { const c = s.charCodeAt(i); a = Math.imul(a ^ c, 0x01000193); b = Math.imul(b ^ c, 0x5bd1e995) ^ (b >>> 15); }
      n += s.length;
    },
    digest: () => `${(a >>> 0).toString(16)}${(b >>> 0).toString(16)}:${n}`,
  };
}

type Battle = { name: string; seed: number; open: (opts: LiveOpts, live?: typeof livePiece, Live?: typeof LiveBattle) => LiveBattle };
/**
 * An arm of battle B on the owner's field (the rulings, section 7): his launch's rows (armOpts, from the one tool's
 * landField), the switch under test and every ride given by name in sw, seated by the seat rule unless a seat is given
 * (null: nobody, the unseated reads of the hard bars).
 */
const F = (B: Battle, sw: Record<string, boolean> = {}, seat: Seat = seatOf(B.seed)): LiveOpts => armOpts(sw, seat);
const W = DRILL.wounded;
const RING = W.ring as Record<string, [number, number, number]>;
const NOT_FOOT = new Set(['tank', 'gun', 'lorry', 'hmg', 'mortar']);
const RIDING = new Set(['ride', 'mounted', 'horses']);
const LYING = new Set(['fire_prone', 'hide', 'crawl', 'observe', 'cower', 'lmg_burst']);
const MOVING = new Set(['walk', 'run', 'advance', 'creep', 'crawl', 'flee']);
const HE = new Set(['tank', 'gun', 'mortar']);
const ARMS = new Set(['rifle', 'mg', 'grenade', 'at']);
const HOLD = Math.max(...Object.values(W.hold));

/** A campaign battle: the urban ticket of the exits tests (June 1944, Soviet against German); seated by its ticket's seed. */
const URBAN_SEED = 2 * 7919 + 13;
function sample(): FieldTicket<LandPlace> {
  const t = withTeams(sampleTicket('urban', 1.1, { seed: URBAN_SEED, nat: { att: 'SOV', def: 'GER' }, humanAtt: false, date: 'June 1944' }));
  (t as { turn: number }).turn = 58;
  return t;
}
/** A sample ticket of a ground and a seed, as the meter opens them (tools/squad-life/wounded.ts open: the seed its own). */
const sampleOf = (ground: 'forest' | 'plains', seed: number): Battle => ({ name: `sample ${ground} ${seed}`, seed, open: (o: LiveOpts, _l?: typeof livePiece, Live = LiveBattle) => new Live(withTeams(sampleTicket(ground, 1.1, { seed, nat: { att: 'RED', def: 'BLU' }, humanAtt: false, date: 'June 1944' })), null, o) });
const piece = (p: string, s: number): Battle => ({ name: `${p} ${s}`, seed: s, open: (o: LiveOpts, live = livePiece) => live(p, s, o) });
/** The eight (the rulings, 7.2: the declared battles and their mirrors at seed + 1). */
const THE_EIGHT: Battle[] = EIGHT.map(([p, s]) => piece(p, s));
/** The battles every bar is read on: the eight, the two sample plains tickets that are not set pieces (7.3), the urban campaign ticket. */
const BATTLES: Battle[] = [
  ...THE_EIGHT,
  ...PLAINS.map(([, s]) => sampleOf('plains', s)),
  { name: 'campaign urban 2', seed: URBAN_SEED, open: (o: LiveOpts, _l?: typeof livePiece, Live = LiveBattle) => new Live(sample(), null, o) },
];
/** The reviewers' own battles (27 November), where they found the faults the rulings' section 5 mends (on his field now). */
const REVIEWED = { n7: piece('normandy-1944', 7), p2: piece('prokhorovka-1943', 2), p7: piece('prokhorovka-1943', 7), p17: piece('prokhorovka-1943', 17), forest9: sampleOf('forest', 9) };

/** How far a burst finds a man, as the drill scores him (drill.ts far: his cover, how much of him shows). */
const farOf = (m: MenOut): number => {
  const c = (DRILL.rounds.cover as Record<string, number>)[m.c] ?? 1;
  const ex = LYING.has(m.p) ? DRILL.blast.lying : m.p === 'fire_kneel' || m.p === 'wait' || m.p === 'creep' ? DRILL.blast.kneeling : 1;
  return (2 - c) / ex;
};
const keyOf = (b: number, m: MenOut, fo = false): string => `${b}:${fo ? 'fo' : ''}${m.k}.${m.j ?? -1}`;
const sig = (b: LandBattle): string => JSON.stringify({ t: b.t, over: b.over, units: b.units.map((u) => [u.id, u.figures, u.str, u.state]) });

type Pairs = { booked: Record<string, number>; keys: Record<string, number>; men: Record<string, number>; crush?: string[] };

/**
 * Bar (d), on against off, STRICT (the design director, 27 November 05:03Z: "the allowance comes out, because a bar
 * never moves after a result"): the rules' booked pairs, the drawn key-model pairs AND the drawn men's kills by pair,
 * all three exactly. Nothing is allowed for. A man drawn run over by a tank (a 'track' kill) is only listed, by block
 * and moment, where it comes in one arm and not the other, so a failure names itself: such a crush was the base's own
 * fault (drill.ts crushOf drew a man to a block the rules had not booked), mended in the base by its owner
 * (the ground thread, "a crush is drawn only where the crushing block IS the booked striker"), never by this row.
 */
function pairsDiff(on: Pairs, off: Pairs): { ok: boolean; why: string } {
  const diff = (a: Record<string, number>, b: Record<string, number>): string[] => [...new Set([...Object.keys(a), ...Object.keys(b)])].filter((k) => (a[k] ?? 0) !== (b[k] ?? 0)).map((k) => `${k} ${b[k] ?? 0}/${a[k] ?? 0}`);
  const booked = diff(on.booked, off.booked), keys = diff(on.keys, off.keys), men = diff(on.men, off.men);
  const cOn = new Set(on.crush ?? []), cOff = new Set(off.crush ?? []), lone = [...[...cOn].filter((c) => !cOff.has(c)).map((c) => 'on ' + c), ...[...cOff].filter((c) => !cOn.has(c)).map((c) => 'off ' + c)];
  const ok = !booked.length && !keys.length && !men.length;
  return { ok, why: `booked ${booked.join(' ') || 'same'}; key kills ${keys.join(' ') || 'same'}; men's kills (off/on) ${men.join(' ') || 'same'}; crushes in one arm only ${lone.join(' ') || 'none'}` };
}
/**
 * The design director's bar (d), 26 November 05:55Z: the rules book, for each model lost, the striking block (sim.ts
 * suffer: the enemy block owed most). Tallied by striker > struck: the models the rules book (sim.shell, drill.struckBy,
 * drill.fromAir), and the kill rounds drawn on key models and on the men of a section. Wraps, changes nothing. Call the
 * getter after every tick (the drill keeps a round only a few seconds).
 */
function tally(L: LiveBattle): () => Pairs {
  const add = (o: Record<string, number>, k: string): void => { o[k] = (o[k] ?? 0) + 1; };
  const P: Pairs = { booked: {}, keys: {}, men: {} }, seen = new Set<number>();
  const sim = L.sim as unknown as { shell: (by: { id: number }, t: { id: number }, k: number, w: unknown) => unknown };
  const drill = L.drill as unknown as { struckBy: (by: number, t: number, k: number) => unknown; fromAir: (t: number, k: number) => unknown; rounds(): (Round & { n: number })[] };
  const sh = sim.shell.bind(sim), sb = drill.struckBy.bind(drill), fa = drill.fromAir.bind(drill);
  sim.shell = (by, t, k, w) => { add(P.booked, `${by.id}>${t.id}`); return sh(by, t, k, w); };
  drill.struckBy = (by, t, k) => { add(P.booked, `${by}>${t}`); return sb(by, t, k); };
  drill.fromAir = (t, k) => { add(P.booked, `air>${t}`); return fa(t, k); };
  return () => {
    for (const r of drill.rounds()) if (!seen.has(r.n)) { seen.add(r.n); if (r.r === 'kill') { add(r.oj === undefined ? P.keys : P.men, `${r.by ?? 'none'}>${r.on}`); if (r.w === 'track' && r.oj !== undefined) (P.crush ??= []).push(`${r.by}>${r.on}@${r.t}`); } }
    return P;
  };
}

/**
 * The design director's bar (c): the most men of a block drawn down beyond the rules' count (key men too) at this
 * moment; and, where 'at' is given, each block's own figure kept by the moment (tenths of a second), for a block by
 * block comparison of on with off (overWorse). A key man falls as the rules lift his model, so the drawing off is
 * itself a man over at times; on must never be more over than off, block by block, at any moment.
 */
function overNow(L: LiveBattle, at?: Map<string, number>): number {
  const d = L.drill as unknown as Drilled;
  let most = 0;
  const t = Math.round(L.sim.t * 10);
  for (const [id, sq] of d.squads) {
    if (sq.esc) continue;
    const u = sq.u, want = u.str <= 0 ? 0 : Math.ceil((sq.m0 * u.str) / Math.max(1e-9, u.str0) - 1e-9);
    let up = 0;
    for (const m of sq.men) if (d.stands(sq, m) || (m.j !== undefined && m.fell === undefined)) up++;
    most = Math.max(most, want - up);
    if (at && want - up > 0) at.set(`${t}|${id}`, want - up);
  }
  return most;
}

/** The moments and blocks where on is more men over the rules' count than off was at that same moment (both kept by overNow). */
function overWorse(on: Map<string, number>, off: Map<string, number>, offTimes: Set<number>): string[] {
  const out: string[] = [];
  for (const [k, v] of on) { const t = Number(k.split('|')[0]); if (offTimes.has(t) && v > (off.get(k) ?? 0)) out.push(`${(t / 10).toFixed(1)} block ${k.split('|')[1]} over ${v} (off ${off.get(k) ?? 0})`); }
  return out;
}

/**
 * The rulings, section 5.2: each block's men down beyond the rules' count (key men too) at every half second, and
 * the seconds over in all (half a second each); with the pairs (bar d).
 */
function counted(L: LiveBattle): { overAt: Map<string, number>; times: Set<number>; secs: number; pairs: Pairs; sig: string; lt: number } {
  const overAt = new Map<string, number>(), times = new Set<number>(), pull = tally(L), seen = new Set<number>();
  let lt = 0;
  while (!L.sim.over) {
    L.tick(0.5, 50); pull(); overNow(L, overAt); times.add(Math.round(L.sim.t * 10));
    for (const r of L.drill.rounds() as (Round & { n: number })[]) if (!seen.has(r.n)) { seen.add(r.n); if (r.lt !== undefined) lt++; }
  }
  return { overAt, times, secs: [...overAt.values()].reduce((a, b) => a + b, 0) * 0.5, pairs: pull(), sig: sig(L.sim), lt };
}

/** Everything the drill and the live feed send, digested, and the battle's outcome; the pairs and the most drawn ahead. */
function digest(L: LiveBattle): { digest: string; body: string; sig: string; header: Record<string, unknown>; deltas: string; pairs: Pairs; over: number; overAt: Map<string, number>; times: Set<number> } {
  const h = hasher(), header = L.header(), dl = hasher(), body = hasher(), pull = tally(L), overAt = new Map<string, number>(), times = new Set<number>();
  let over = 0;
  h.update(JSON.stringify(header));
  for (let n = 0; !L.sim.over && n < 6000; n++) {
    const d = JSON.stringify(L.tick(1, 50));
    h.update(d); dl.update(d); body.update(d);
    for (const x of [JSON.stringify(L.drill.view()), JSON.stringify(L.drill.view('def')), JSON.stringify(L.drill.rounds())]) { h.update(x); body.update(x); }
    pull(); over = Math.max(over, overNow(L, overAt)); times.add(Math.round(L.sim.t * 10));
  }
  return { digest: h.digest(), body: body.digest(), sig: sig(L.sim), header, deltas: dl.digest(), pairs: pull(), over, overAt, times };
}

interface Sq { u: { id: number; side: string; kind: string; team?: string; str: number; str0: number; cal?: string }; men: { j?: number; k: number; fell?: number; job?: unknown }[]; m0: number; esc?: string }
interface Drilled { squads: Map<number, Sq>; stands(sq: Sq, m: unknown): boolean; ringOdds(m: Sq['men'][number], p: [number, number], cal: string, ok: boolean): number; far(m: unknown): number; hurtLog: { bursts: { t: number; x: number; y: number; n: number; cal: string; men: number; shrunk: boolean; key: boolean }[]; moved: number; stuck: number; he: number[] }; step(): void }

/** What a battle with the switch on showed, read step by step from the drill's own view and rounds. */
interface Seen {
  sig: string; header: Record<string, unknown>; bad: string[]; pairs: Pairs; over: number;
  ahead: number; trail: number; trailAllowed: number;
  dead: Record<string, number>; hurt: Record<string, number>;
  sizes: number[]; bursts: number; shrunk: number; laidIn: number; ringWrong: number; ringChecked: number; ringLag: number;
  heMisses: number; missIn: number; kd: number; kdHolds: number[]; sameHold: number; multi: number; resumed: number; resumable: number;
  /** Each block's men over the rules' count by moment (overNow); knock-downs that began less than the get-up clip (1.6 s) after his last ended. */
  overAt: Map<string, number>; quick: number;
  /** Men each killing burst ENDED with: at landing and its stunned men taken later (their kill rounds carry its number, lt). */
  ended: number[];
  /** 5.8: half seconds a man inside his hold lay under a moving tank or lorry (and the first few), and the moves allowed him inside it (rolled clear of one). */
  under: number; underAt: string[]; rolled: number;
}

/** Fights one battle with the switch on and watches it. */
function watch(L: LiveBattle, blast: boolean): Seen {
  const S: Seen = { sig: '', header: L.header(), bad: [], pairs: { booked: {}, keys: {}, men: {} }, over: 0, ahead: 0, trail: 0, trailAllowed: 0, dead: {}, hurt: {}, sizes: [], bursts: 0, shrunk: 0, laidIn: 0, ringWrong: 0, ringChecked: 0, ringLag: 0, heMisses: 0, missIn: 0, kd: 0, kdHolds: [], sameHold: 0, multi: 0, resumed: 0, resumable: 0, overAt: new Map(), quick: 0, ended: [], under: 0, underAt: [], rolled: 0 };
  const d = L.drill as unknown as Drilled, sim = L.sim;
  const rounds = new Map<number, Round & { n: number }>(), pendingMiss = new Map<number, Round & { n: number }>();
  let prev = new Map<string, MenOut>(), burstsSeen = 0;
  const wounded = new Map<string, { x: number; y: number; t: number }>();
  const windows: { key: string; b: number; m: MenOut; t0: number; end: number; x: number; y: number; p: string; done?: boolean; after?: boolean }[] = [];
  const lastKd = new Map<string, string>(), kdByRound = new Map<number, number[]>();
  const owed = new Map<number, number[]>(), downs = new Map<number, number>();
  // (the longest a shell may hold a block's losses, by the shells its enemies throw, and a think)
  const allowed = new Map<number, number>();
  for (const sq of d.squads.values()) {
    const cals = sim.allBlocks().filter((e) => e.side !== sq.u.side && (e.kind === 'art' || e.kind === 'arm')).map((e) => W.hold[(e.cal ?? (e.team === 'mor' ? 'mortar' : 'field')) as keyof typeof W.hold] ?? DRILL.blast.hold);
    allowed.set(sq.u.id, Math.max(DRILL.blast.hold, ...cals) + 0.5);
  }
  // (where and how each man a laid burst took stood as it was judged: the step's think may have changed his pose since
  // the row before, and his cover with it)
  const judged = new Map<unknown, number>(), ringOdds = d.ringOdds.bind(d);
  d.ringOdds = (m, p, cal, ok) => {
    if (ok) judged.set(m, Math.hypot((m as unknown as MenOut).x - p[0], (m as unknown as MenOut).y - p[1]) * d.far(m));
    return ringOdds(m, p, cal, ok);
  };
  const drill = L.drill as unknown as { view(as?: string): MenBlock[]; rounds(): (Round & { n: number })[] };
  // (the drill's own man behind a row's key, for what the wire does not carry: rolled)
  const manOf = (key: string): { rolled?: number } | undefined => {
    const [b, rest] = key.split(':'), [k, j] = rest.split('.').map(Number);
    for (const q of d.squads.values()) if (q.u.id === Number(b)) for (const m of q.men as { k: number; j?: number; rolled?: number }[]) if (m.k === k && (m.j ?? -1) === j) return m;
    return undefined;
  };
  const orig = drill.view.bind(drill);
  drill.view = (as?: string): MenBlock[] => {
    const out = orig(as), t = Math.round(sim.t * 100) / 100, over = !!sim.over;
    if (as !== undefined) return out;
    const fresh: (Round & { n: number })[] = [];
    for (const r of drill.rounds()) if (!rounds.has(r.n)) { rounds.set(r.n, r); fresh.push(r); }
    const now = new Map<string, [number, MenOut]>();
    const living: MenOut[] = [];
    for (const blk of out) for (const [list, fo] of [[blk.men, false], [blk.fo ?? [], true]] as [MenOut[], boolean][]) for (const m of list) {
      now.set(keyOf(blk.id, m, fo), [blk.id, m]);
      if (m.d === undefined && !NOT_FOOT.has(m.r) && !RIDING.has(m.p)) living.push(m);
    }
    // The wounded lie for good: never up, never moved, never firing, never fired at.
    for (const [key, [b, m]] of now) {
      if (m.wd !== undefined && !wounded.has(key)) {
        wounded.set(key, { x: m.x, y: m.y, t: m.wd });
        if (m.cy || m.back !== undefined || m.gone !== undefined) S.bad.push(`${t} ${key} wounded with a carry`);
      }
      const w = wounded.get(key);
      if (w && !over && (m.p !== 'wounded' || m.d === undefined || m.x !== w.x || m.y !== w.y)) S.bad.push(`${t} ${key} wounded man up or moved (${m.p})`);
      void b;
    }
    for (const r of fresh) {
      const by = r.by !== undefined ? `${r.by}:${r.k}.${r.j ?? -1}` : '', on = r.ok !== undefined ? `${r.on}:${r.ok}.${r.oj ?? -1}` : '';
      if (by && wounded.has(by)) S.bad.push(`${t} round ${r.n} from wounded ${by}`);
      // (a stunned man taken too: his kill round, his burst's, is booked in the think he falls, never after)
      if (on && wounded.has(on) && r.t > wounded.get(on)!.t + 0.05) S.bad.push(`${t} round ${r.n} at wounded ${on}`);
      for (const w of windows) if (!w.done && by === w.key && r.t > w.t0 + 1e-6 && r.t < w.end - 1e-6) S.bad.push(`${t} round ${r.n} from ${by} held till ${w.end}`);
      if (r.r === 'miss' && d.hurtLog.he.includes(r.n)) pendingMiss.set(r.n, r);
    }
    // A miss lands clear of men (measured: the share with a man inside its lethal ring as it lands).
    for (const [n, r] of pendingMiss) {
      if (r.l > t + 1e-6) continue;
      pendingMiss.delete(n);
      S.heMisses++;
      const cal = (r.by !== undefined ? sim.unit(r.by)?.cal : undefined) ?? (r.w === 'mortar' ? 'mortar' : 'field');
      if (living.some((m) => Math.hypot(m.x - r.x, m.y - r.y) * farOf(m) < RING[cal][0] - 1e-6)) S.missIn++;
    }
    // Each burst laid: no other living man inside its lethal ring; its dead inside it, its wounded outside.
    for (; burstsSeen < d.hurtLog.bursts.length; burstsSeen++) {
      const bu = d.hurtLog.bursts[burstsSeen];
      S.bursts++;
      if (bu.shrunk) { S.shrunk++; continue; }
      if (over) continue;
      const [L0] = RING[bu.cal];
      if (living.some((m) => Math.hypot(m.x - bu.x, m.y - bu.y) * farOf(m) < L0 - 1e-6)) S.laidIn++;
      for (const r of fresh) {
        if (r.r !== 'kill' || !HE.has(r.w) || Math.abs(r.x - bu.x) > 0.051 || Math.abs(r.y - bu.y) > 0.051 || r.t !== bu.t) continue;
        const key = `${r.on}:${r.ok}.${r.oj ?? -1}`, was = prev.get(key), row = now.get(key)?.[1];
        if (!was || !row || NOT_FOOT.has(was.r)) continue;
        const e = Math.hypot(was.x - bu.x, was.y - bu.y) * farOf(was);
        if (Math.abs(e - L0) < 0.4) continue;
        S.ringChecked++;
        // (wrong: as he stood when judged; lag: as the row before showed him, which a pose taken in the step can turn)
        // (a block's riders and its other proxies share its id: every squad of that id is looked through)
        let at: number | undefined;
        for (const q of d.squads.values()) if (q.u.id === r.on) for (const m of q.men) if (m.k === r.ok && m.j === r.oj && judged.has(m)) at = judged.get(m);
        if ((e < L0) !== (row.wd === undefined)) S.ringLag++;
        at ??= e;
        if ((at < L0) !== (row.wd === undefined)) { S.ringWrong++; S.bad.push(`${t} ${key} ${row.wd === undefined ? 'dead' : 'wounded'} at ${at.toFixed(1)} from a burst (lethal ${L0})`); }
      }
    }
    // Knocked down: alive, on foot, inside the stun ring as it lands; still and firing nothing for his hold; then on.
    for (const [key, [b, m]] of now) {
      if (!m.kd) continue;
      const k = m.kd.join(',');
      if (lastKd.get(key) === k) continue;
      lastKd.set(key, k);
      const [t0, n, s] = m.kd, r = rounds.get(n);
      S.kd++; S.kdHolds.push(s);
      if (s < W.kd.least - 1e-6) S.bad.push(`${t} ${key} held ${s} s`);
      if (m.d !== undefined && m.d <= t0) S.bad.push(`${t} ${key} knocked down fallen`);
      if (NOT_FOOT.has(m.r) || RIDING.has(m.p) || m.lv) S.bad.push(`${t} ${key} knocked down not on foot (${m.r} ${m.p})`);
      if (!r) S.bad.push(`${t} ${key} knocked down by round ${n} never sent`);
      else {
        const cal = r.by !== undefined ? sim.unit(r.by)?.cal ?? (r.w === 'mortar' ? 'mortar' : 'field') : 'field', S0 = RING[cal][2];
        // (judged where he stood, and as he stood, on the step before: going down himself changes his cover)
        const pm = prev.get(key) ?? m, e = Math.min(Math.hypot(pm.x - r.x, pm.y - r.y) * farOf(pm), Math.hypot(m.x - r.x, m.y - r.y) * farOf(m));
        if (e > S0 + 0.3) S.bad.push(`${t} ${key} knocked down ${e.toFixed(1)} from round ${n} (stun ${S0})`);
      }
      // (a fresh burst on a man inside his hold finds him as a man lying, and holds him to the later end: one kd, its
      // start the later round's; a man up again is read as any man standing, so nothing here reads how long ago he
      // got up (the rulings, 6.2). Counted, for the report only: knock-downs beginning less than the get-up clip, 1.6 s,
      // after his last hold ended)
      const last = windows.filter((w) => w.key === key).pop();
      if (last && t0 >= last.end && t0 - last.end < 1.6) S.quick++;
      // (a fresh burst on him inside his hold takes its place: the earlier window ends there)
      for (const w of windows) if (w.key === key && !w.done) w.end = Math.min(w.end, t0 + 1e-6);
      windows.push({ key, b, m, t0, end: t0 + s, x: m.x, y: m.y, p: prev.get(key)?.p ?? m.p });
      (kdByRound.get(n) ?? kdByRound.set(n, []).get(n)!).push(s);
    }
    for (const w of windows) {
      if (w.done) continue;
      const row = now.get(w.key)?.[1];
      if (!row) { w.done = true; continue; }
      if (t < w.end - 1e-6) {
        // (the one move allowed him: rolled or dragged clear of a vehicle's way, on the ground, the rulings 5.8)
        if (!over && (row.x !== w.x || row.y !== w.y) && row.d === undefined && !row.lv) {
          const dm = manOf(w.key);
          if (dm?.rolled !== undefined && dm.rolled >= w.t0 - 1e-6) { w.x = row.x; w.y = row.y; S.rolled++; } else S.bad.push(`${t} ${w.key} moved inside his hold`);
        }
        continue;
      }
      // (after it: one who was on the move goes on, within 3 s)
      if (!MOVING.has(w.p) || row.d !== undefined || row.lv || over) { w.done = true; continue; }
      if (w.after === undefined) { w.after = false; S.resumable++; }
      if (row.x !== w.x || row.y !== w.y || row.p !== w.p) { S.resumed++; w.done = true; } else if (t > w.end + 3) w.done = true;
    }
    // (5.8: never drawn under a moving tank or lorry while he lies inside his hold: inside her nose, kd.nose metres of her
    // middle along her, and the rules' run-over half width across, escort.clear.crushHalf)
    const movers: { x: number; y: number; f: number }[] = [];
    for (const blk of out) for (const m of blk.men) if ((m.r === 'tank' || m.r === 'lorry') && m.d === undefined) { const p0 = prev.get(keyOf(blk.id, m)); if (p0 && Math.hypot(p0.x - m.x, p0.y - m.y) > 0.03) movers.push({ x: m.x, y: m.y, f: m.f }); }
    if (movers.length && !over) for (const w of windows) {
      if (w.done || t >= w.end - 1e-6) continue;
      const row = now.get(w.key)?.[1];
      if (!row || row.d !== undefined) continue;
      for (const v of movers) {
        const dx = row.x - v.x, dy = row.y - v.y, a = dx * Math.sin(v.f) + dy * Math.cos(v.f), l = dx * Math.cos(v.f) - dy * Math.sin(v.f);
        if (Math.abs(a) <= W.kd.nose && Math.abs(l) <= DRILL.escort.clear.crushHalf) { S.under++; if (S.underAt.length < 4) S.underAt.push(`${t} ${w.key}`); break; }
      }
    }
    // The men down: never ahead of the rules' count, nor behind it longer than a shell's hold and a think.
    for (const sq of d.squads.values()) {
      if (sq.esc) continue;
      const u = sq.u, want = u.str <= 0 ? 0 : Math.ceil((sq.m0 * u.str) / Math.max(1e-9, u.str0) - 1e-9);
      let up = 0;
      let keys = 0;
      for (const m of sq.men) if (d.stands(sq, m) || (m.j !== undefined && m.fell === undefined)) up++; else if (m.j === undefined) keys++;
      const booked = sq.m0 - want, drawn = sq.m0 - up, key = u.id * 2 + (sq.u === sim.unit(u.id) ? 0 : 1);
      // (ahead: the section men only. A key man falls when the rules lift his model, not by the block's strength, and
      // the base draws him so, a man ahead of the count until the next loss)
      if (drawn - keys > booked) S.ahead = Math.max(S.ahead, drawn - keys - booked);
      const q = owed.get(key) ?? [];
      let gone = downs.get(key) ?? 0;
      for (let i = gone + q.length; i < booked; i++) q.push(t);
      while (gone < drawn && q.length) { const at = q.shift()!; gone++; const tr = t - at; if (tr > S.trail) { S.trail = tr; S.trailAllowed = allowed.get(u.id) ?? HOLD + 0.5; } if (tr > (allowed.get(u.id) ?? HOLD + 0.5) + 1e-6) S.bad.push(`${t} block ${u.id} drawn ${tr.toFixed(1)} s behind the rules`); }
      downs.set(key, gone); owed.set(key, q);
    }
    prev = new Map([...now].map(([k, [, m]]) => [k, m]));
    return out;
  };
  // (every half second: the bar's own beat)
  const pull = tally(L);
  while (!sim.over) { L.tick(0.5, 50); pull(); S.over = Math.max(S.over, overNow(L, S.overAt)); }
  S.pairs = pull();
  S.sig = sig(sim);
  // Killing bursts: an HE kill round and its mates at the same t, x and y.
  const bursts = new Map<string, number>(), cause = new Map<string, string>();
  for (const r of rounds.values()) {
    if (r.r !== 'kill') continue;
    cause.set(`${r.on}:${r.ok}.${r.oj ?? -1}`, HE.has(r.w) ? 'burst' : ARMS.has(r.w) ? 'arms' : r.w);
    if (HE.has(r.w)) { const k = `${r.on}|${r.x}|${r.y}`; bursts.set(k, (bursts.get(k) ?? 0) + 1); }
  }
  S.sizes = [...bursts.values()];
  // (and each burst with its stunned men taken later: a late round goes to the burst whose round it names)
  const keyOfRound = new Map<number, string>(), ended = new Map<string, number>();
  for (const r of rounds.values()) if (r.r === 'kill' && HE.has(r.w) && r.lt === undefined) { const k = `${r.on}|${r.x}|${r.y}|${r.t}`; keyOfRound.set(r.n, k); ended.set(k, (ended.get(k) ?? 0) + 1); }
  for (const r of rounds.values()) if (r.r === 'kill' && r.lt !== undefined) {
    const k = keyOfRound.get(r.lt);
    if (!k || r.m !== 1) S.bad.push(`round ${r.n} a late fall of round ${r.lt}, no killing burst of it seen`);
    else ended.set(k, (ended.get(k) ?? 0) + 1);
  }
  S.ended = [...ended.values()];
  for (const blk of orig()) for (const m of blk.men) {
    if (m.d === undefined || NOT_FOOT.has(m.r)) continue;
    const c = cause.get(`${blk.id}:${m.k}.${m.j ?? -1}`) ?? 'none';
    if (m.wd !== undefined) S.hurt[c] = (S.hurt[c] ?? 0) + 1; else S.dead[c] = (S.dead[c] ?? 0) + 1;
  }
  for (const holds of kdByRound.values()) if (holds.length >= 2) { S.multi++; if (holds.every((h) => h === holds[0])) S.sameHold++; }
  void blast;
  return S;
}

/** The base tree, where one is given: its live battle, opened as this tree's is. */
const BASE = (globalThis as { process?: { env: Record<string, string | undefined> } }).process?.env.WOUNDED_BASE;
const base = BASE ? ((await import(BASE + '/src/battle/land/live.ts')) as typeof import('../src/battle/land/live')) : null;

const all: { name: string; seat: Seat; on: Seen; plain: Seen }[] = [];

/**
 * The stunned: false reference digests, by battle, set and seat: read on the tree with its one stunned line struck out.
 * Recorded again on the pass-2 line (27 November 13:52Z: this tree, next-al 0dc2b5b75 + the row, with stun() in
 * burstThin struck out; all 16 keys read, and stunned: false on the tree as it stands gave the same 16). Fifteen are
 * what the row's old line recorded. One moved: Prokhorovka 13, bearers on, nobody seated. With the bearers rule on
 * this row stands aside (woundsOn false), so that key is the base's own drawing, and the pass-2 line draws that battle
 * otherwise than the old line did from 306.5 s on (the pass-2 line with the bearers alone gives the same d511d769).
 * Recorded once more where the row is carried onto ground's crush mend (27 November 15:52Z: next-bc d57656614 + the
 * row, the same way: all 16 keys read with stun() struck out, and stunned: false gave the same 16). Fifteen stand. The
 * same one moved, to 40a2cce8: a tank of a block that did not strike a man no longer crushes him (crushOf's third
 * argument), which changes the base's own drawing of that battle; next-bc itself with the bearers alone gives 40a2cce8.
 */
const REF_TABLE: Record<string, string> = {
  'normandy-1944-11-bearers-seated': 'd58c2279',
  'normandy-1944-11-bearers-unseated': '9f7f03ce',
  'normandy-1944-11-shipped-seated': 'e7b63c77',
  'normandy-1944-11-shipped-unseated': 'bdf6c290',
  'normandy-1944-3-bearers-seated': 'c27cc76',
  'normandy-1944-3-bearers-unseated': '1bfbbd29',
  'normandy-1944-3-shipped-seated': '546ac04f',
  'normandy-1944-3-shipped-unseated': 'f7a25506',
  'prokhorovka-1943-13-bearers-seated': '3017872a',
  'prokhorovka-1943-13-bearers-unseated': '40a2cce8',
  'prokhorovka-1943-13-shipped-seated': '810e8ae2',
  'prokhorovka-1943-13-shipped-unseated': '489cc4e',
  'prokhorovka-1943-5-bearers-seated': 'cc7c7404',
  'prokhorovka-1943-5-bearers-unseated': '1b6e1a3b',
  'prokhorovka-1943-5-shipped-seated': '8e9f09a8',
  'prokhorovka-1943-5-shipped-unseated': 'b717ae0d',
};

describe('wounded and knocked-down men', () => {
  for (const B of BATTLES) {
    it(`${B.name}: off is the base's, on fights the same battle, and the men act it out`, () => {
      // Off: every byte the drill and the feed send, with shell blasts on and off.
      for (const blast of [true, false]) {
        // (blast is a row of his field, on; off it is a ride by name)
        const sw: Record<string, boolean> = blast ? {} : { blast: false };
        const off = digest(B.open(F(B, sw)));
        const offF = digest(B.open(F(B, { ...sw, wounded: false })));
        expect(offF.digest).toBe(off.digest);
        expect('wounded' in off.header).toBe(false);
        if (base) expect(digest(B.open(F(B, sw), base.livePiece, base.LiveBattle)).digest).toBe(off.digest);
        // On: the very same battle, to the last figure.
        const on = watch(B.open(F(B, { ...sw, wounded: true })), blast);
        expect(on.sig).toBe(off.sig);
        // (the bar (d): each fall to the block the rules booked, the same on as off; (c): never more drawn down ahead of
        // the rules' count than off, where a key man falls as his model is lifted)
        { const pd = pairsDiff(on.pairs, off.pairs); if (!pd.ok || pd.why.includes('one arm only on') || pd.why.includes('one arm only off')) console.log(`${B.name} blast ${blast ? 'on' : 'off'} pairs: ${pd.why}`); expect(pd.ok, pd.why).toBe(true); }
        expect(on.over).toBeLessThanOrEqual(off.over);
        // (block by block, at every moment both saw: never more men down beyond the rules' count than off, key men too)
        const worse = overWorse(on.overAt, off.overAt, off.times);
        expect(worse, worse.slice(0, 5).join(' | ')).toEqual([]);
        expect(on.header.wounded).toBe(true);
        console.log(`${B.name} blast ${blast ? 'on ' : 'off'}: bursts ${[1, 2, 3, 4].map((k) => `${k}${k === 4 ? '+' : ''}: ${on.sizes.filter((s) => (k < 4 ? s === k : s >= 4)).length}`).join(', ')}; dead/wounded ${Object.entries(on.dead).map(([c, n]) => `${c} ${n}/${on.hurt[c] ?? 0}`).join(', ')}; laid ${on.bursts} shrunk ${on.shrunk}, laid on a living man ${on.laidIn}, ring wrong ${on.ringWrong}/${on.ringChecked} (by the row before ${on.ringLag}); HE misses ${on.heMisses}, on a man ${on.missIn}; kd ${on.kd} (bursts with two or more ${on.multi}, all one hold ${on.sameHold}), resumed ${on.resumed}/${on.resumable}, knocked down again less than 1.6 s after getting up ${on.quick}; under a moving vehicle inside his hold ${on.under}${on.underAt.length ? ' (' + on.underAt.join(', ') + ')' : ''}, rolled clear ${on.rolled}; pairs booked ${Object.values(on.pairs.booked).reduce((a, b) => a + b, 0)} keys ${Object.values(on.pairs.keys).reduce((a, b) => a + b, 0)} men ${Object.values(on.pairs.men).reduce((a, b) => a + b, 0)} same as off ${(['booked', 'keys', 'men'] as const).every((k) => { const x = on.pairs[k], y = off.pairs[k]; return Object.keys({ ...x, ...y }).every((q) => x[q] === y[q]); }) ? 'yes' : 'NO (' + pairsDiff(on.pairs, off.pairs).why + ')'}; over ${on.over} (off ${off.over}); ahead ${on.ahead}, trail ${on.trail.toFixed(1)} s (allowed ${on.trailAllowed.toFixed(1)}); faults ${on.bad.length}${on.bad.length ? ': ' + on.bad.slice(0, 5).join(' | ') : ''}`);
        expect(on.bad).toEqual([]);
        expect(on.ahead).toBe(0);
        // (the rulings, 5.5: no booked man drawn standing longer than the drawing off holds him, and a think)
        expect(on.trail).toBeLessThanOrEqual(DRILL.blast.hold + 0.5 + 1e-6);
        // (the rulings, 5.8: never drawn under a moving tank or lorry while he lies inside his hold)
        expect(on.under, on.underAt.join(' | ')).toBe(0);
        expect(on.ringWrong).toBe(0);
        if (blast) expect(on.laidIn).toBe(0);
        expect(on.missIn).toBeLessThanOrEqual(Math.ceil(on.heMisses * 0.015));
        if (on.multi >= 3) expect(on.sameHold).toBeLessThan(on.multi);
        if (on.resumable >= 5) expect(on.resumed / on.resumable).toBeGreaterThan(0.8);
        if (blast) all.push({ name: B.name, seat: seatOf(B.seed), on, plain: on });
      }
    });
  }

  // The rulings, 7.2: the hard bars run unseated (me null) on all eight as well: off byte identical (to the switch left
  // out, and to the base where one is given), the rules' units and result identical on against off, the count block by
  // block at every half second, the cause pairs exact, and with the bearers rule on wounded changes nothing.
  for (const B of THE_EIGHT) {
    it(`${B.name} unseated: the hard bars`, () => {
      const off = digest(B.open(F(B, {}, null))), offF = digest(B.open(F(B, { wounded: false }, null)));
      expect(offF.digest).toBe(off.digest);
      if (base) expect(digest(B.open(F(B, {}, null), base.livePiece, base.LiveBattle)).digest).toBe(off.digest);
      const on = counted(B.open(F(B, { wounded: true }, null)));
      const worse = overWorse(on.overAt, off.overAt, off.times);
      const bOff = digest(B.open(F(B, { bearers: true }, null))), bOn = digest(B.open(F(B, { bearers: true, wounded: true }, null)));
      console.log(`${B.name} unseated: off ${off.digest}; on the same battle ${on.sig === off.sig ? 'yes' : 'NO'}, pairs ${pairsDiff(on.pairs, off.pairs).why}, seconds over the count on ${on.secs.toFixed(1)} (off, read each whole second, ${[...off.overAt.values()].reduce((a, b) => a + b, 0).toFixed(1)}), half seconds a block more over than off ${worse.length}; bearers on: wounded the same ${bOn.digest === bOff.digest ? 'yes' : 'NO'}`);
      expect(on.sig).toBe(off.sig);
      { const pd = pairsDiff(on.pairs, off.pairs); expect(pd.ok, pd.why).toBe(true); }
      expect(worse, worse.slice(0, 5).join(' | ')).toEqual([]);
      expect(bOn.digest).toBe(bOff.digest);
      expect(bOn.pairs).toEqual(bOff.pairs);
    });
  }

  // The rulings, 7.4: the preview icon's read (squad life, hatches and the tanks' machine guns on, by name): the hard bars
  // and the group sizes, wounded on against off, Normandy 1944 seed 11 (the attacker seated) and Prokhorovka 1943 seed 6
  // (the defender seated).
  it('the preview read: life, hatches and tankmg ridden by name, wounded on against off', () => {
    const rides = Object.fromEntries(BRIAN_LAUNCH.land.previewRides.map((k) => [k, true]));
    for (const B of [piece('normandy-1944', 11), piece('prokhorovka-1943', 6)]) {
      const off = digest(B.open(F(B, rides))), offF = digest(B.open(F(B, { ...rides, wounded: false })));
      expect(offF.digest).toBe(off.digest);
      if (base) expect(digest(B.open(F(B, rides), base.livePiece, base.LiveBattle)).digest).toBe(off.digest);
      const on = watch(B.open(F(B, { ...rides, wounded: true })), true);
      const worse = overWorse(on.overAt, off.overAt, off.times), e = on.ended;
      console.log(`${B.name} preview (seat ${seatOf(B.seed)}, rides ${Object.keys(rides).join('+')}): the same battle ${on.sig === off.sig ? 'yes' : 'NO'}; pairs ${pairsDiff(on.pairs, off.pairs).why}; half seconds a block more over than off ${worse.length}; killing bursts by the men each ended with 1: ${e.filter((x) => x === 1).length}, 2: ${e.filter((x) => x === 2).length}, 3: ${e.filter((x) => x === 3).length}, 4+: ${e.filter((x) => x >= 4).length}; kd ${on.kd}; faults ${on.bad.length}${on.bad.length ? ': ' + on.bad.slice(0, 5).join(' | ') : ''}`);
      expect(on.sig).toBe(off.sig);
      { const pd = pairsDiff(on.pairs, off.pairs); expect(pd.ok, pd.why).toBe(true); }
      expect(worse, worse.slice(0, 5).join(' | ')).toEqual([]);
      expect(on.bad).toEqual([]);
      expect(on.ahead).toBe(0);
      expect(on.header.wounded).toBe(true);
    }
  });

  it('with the switches it meets on as well (exits, leaders, seasoned, tankmg): the same battle and pairs, no fault', () => {
    for (const B of BATTLES.slice(0, 1)) {
      const with4 = { exits: true, leaders: true, seasoned: true, tankmg: true };
      const off = digest(B.open(F(B, with4)));
      const on = watch(B.open(F(B, { ...with4, wounded: true })), true);
      console.log(`${B.name} with exits, leaders, seasoned, tankmg: kd ${on.kd}, resumed ${on.resumed}/${on.resumable}, over ${on.over} (off ${off.over}), trail ${on.trail.toFixed(1)} s; faults ${on.bad.length}${on.bad.length ? ': ' + on.bad.slice(0, 5).join(' | ') : ''}`);
      expect(on.sig).toBe(off.sig);
      { const pd = pairsDiff(on.pairs, off.pairs); expect(pd.ok, pd.why).toBe(true); }
      expect(on.over).toBeLessThanOrEqual(off.over);
      expect(overWorse(on.overAt, off.overAt, off.times)).toEqual([]);
      expect(on.bad).toEqual([]);
      expect(on.ahead).toBe(0);
    }
  });

  // The rulings, sections 2 and 5.6: with the bearers rule on, wounded changes nothing (no knock-down, no miss moved,
  // no key man's burst laid by the rings, no stunned men: the bearers' own thin). Every byte, the header among them, is
  // the drawing off: the header's key and the deltas' wd and kd are sent only while the switch's looks are at work. On
  // two battles of the tests and the reviewer's three (Normandy 7, Prokhorovka 17, sample forest 9).
  it('with the bearers rule on, wounded changes nothing, not even the header', () => {
    for (const B of [BATTLES[0], BATTLES[4], BATTLES[7], REVIEWED.n7, REVIEWED.p17, REVIEWED.forest9]) {
      const off = digest(B.open(F(B, { bearers: true }))), on = digest(B.open(F(B, { bearers: true, wounded: true })));
      console.log(`${B.name} bearers: off ${off.digest}, on ${on.digest}; pairs same ${JSON.stringify(on.pairs) === JSON.stringify(off.pairs) ? 'yes' : 'NO'}`);
      expect(on.digest, B.name).toBe(off.digest);
      { const pd = pairsDiff(on.pairs, off.pairs); expect(pd.ok, pd.why).toBe(true); }
      expect(on.sig).toBe(off.sig);
      expect('wounded' in on.header).toBe(false);
    }
  });

  // The rulings, section 5.2: with stunned men, a block's men down (key men too) are never more beyond the rules'
  // count than with the switch off, block by block, at any half second, nor more seconds over in all. On the battles
  // where the reviewer found it so (Prokhorovka 2, blocks 8 and 9; Prokhorovka 17, blocks 7 and 8; sample forest 9,
  // block 5) and one of the tests'.
  it('the count: never more men down beyond the rules\' count than off, block by block, at every half second', () => {
    for (const B of [REVIEWED.p2, REVIEWED.p17, REVIEWED.forest9, BATTLES[4], BATTLES[6]]) {
      const off = counted(B.open(F(B))), on = counted(B.open(F(B, { wounded: true })));
      const worse = overWorse(on.overAt, off.overAt, off.times);
      console.log(`${B.name} count: seconds over the rules' count on ${on.secs.toFixed(1)}, off ${off.secs.toFixed(1)}; half seconds with a block more over than off ${worse.length}${worse.length ? ': ' + worse.slice(0, 4).join(' | ') : ''}; late falls of a stunned man ${on.lt}`);
      expect(worse, worse.slice(0, 5).join(' | ')).toEqual([]);
      expect(on.secs).toBeLessThanOrEqual(off.secs + 1e-6);
      expect(on.sig).toBe(off.sig);
      { const pd = pairsDiff(on.pairs, off.pairs); expect(pd.ok, pd.why).toBe(true); }
      expect(off.lt).toBe(0);
    }
  });

  // Stunned men (rulings section 4, the design director, 26 November 12:57Z). The reference: wounded on with no stunned
  // men, every view row, the rounds and the delta at every half second, as shipped and with the bearers on; stunned:
  // false must reproduce it. Recorded again (the rulings, 5.11) after the holds went back to 6 s (5.5), the knock-down
  // came to read only what a man is now and its chance to fall evenly by the burst's reach (6.2, 6.3), a man held was
  // rolled clear of a vehicle (5.8) and the battles came onto the owner's field (7): from this tree with its one
  // stunned line (stun() in burstThin) struck out (scratchpad wnd/mend3/nostun, refd3.ts), seated by the seat rule
  // and unseated (me null), both kept.
  const REF_DIGESTS: Record<string, string> = REF_TABLE;
  const fnv = (s: string): string => { let a = 0x811c9dc5; for (let i = 0; i < s.length; i++) a = Math.imul(a ^ s.charCodeAt(i), 0x01000193); return (a >>> 0).toString(16); };
  for (const seated of [true, false]) it(`stunned false is the reference (wounded on with no stunned men), as shipped and with the bearers on, ${seated ? 'seated' : 'unseated'}`, () => {
    for (const B of [piece('normandy-1944', 11), piece('normandy-1944', 3), piece('prokhorovka-1943', 5), piece('prokhorovka-1943', 13)]) for (const set of ['shipped', 'bearers']) {
      const [key, seed] = B.name.split(' ');
      const L = B.open(F(B, { wounded: true, stunned: false, ...(set === 'bearers' ? { bearers: true } : {}) }, seated ? seatOf(B.seed) : null));
      const lines: string[] = [JSON.stringify(L.header()).length + ' ' + fnv(JSON.stringify(L.header()))];
      while (!L.sim.over) {
        const d = L.tick(0.5, 1000);
        lines.push(`${L.sim.t.toFixed(2)} ${fnv(JSON.stringify(d))} ${fnv(JSON.stringify(L.drill.view()))} ${fnv(JSON.stringify(L.drill.view('def')))} ${fnv(JSON.stringify(L.drill.rounds()))}`);
      }
      lines.push('end ' + JSON.stringify({ t: L.sim.t, units: L.sim.units.map((u) => [u.id, u.figures, u.str, u.state]) }));
      const k = `${key}-${seed}-${set}-${seated ? 'seated' : 'unseated'}`;
      console.log(`reference ${k} ${fnv(lines.join('\n'))}`);
      expect(fnv(lines.join('\n')), k).toBe(REF_DIGESTS[k]);
    }
  });

  it('stunned men: the cap; taken only from the losses of their own striker, as its burst, never up again; the rest get up', () => {
    // (the reviewers' stale takes were on Prokhorovka 5, 13 and 7 and the urban ticket: a man up and knocked down again
    // by another round taken as the old burst's, up to 137 m from it)
    for (const B of [...BATTLES, REVIEWED.p7, REVIEWED.p2]) {
      // On: every mark within the cap; every man taken from a shell loss of his own striker that the drawing off draws
      // then, his kill round that burst's (a mate at its point, from its striker, in the think he falls), having lain
      // since it without a break; never up again, no round from him, no later knock-down; the rest up after their window.
      const L = B.open(F(B, { wounded: true }));
      type SM = { k: number; j?: number; fell?: number; wd?: number; kd?: [number, number, number]; stun?: { s: number; n: number; x: number; y: number; e: number } };
      const d = L.drill as unknown as { squads: Map<number, { u: { id: number }; men: SM[] }>; hurtLog: { stun: { marked: number; taken: number; late: number[] } }; takeStunned(sq: unknown, by: unknown, at: number): SM | null };
      const took = new Set<SM>(), ts = d.takeStunned.bind(d);
      d.takeStunned = (sq, by, at) => { const m = ts(sq, by, at); if (m) took.add(m); return m; };
      const takenAt = new Map<string, { d: number; kd: string }>(), bad: string[] = [];
      const seen = new Set<number>(), kds = new Map<string, [number, number, number][]>(), kills = new Map<string, Round & { n: number }>();
      let lts = 0;
      while (!L.sim.over) {
        L.tick(0.5, 50);
        for (const r of L.drill.rounds() as (Round & { n: number })[]) if (!seen.has(r.n)) {
          seen.add(r.n);
          if (r.r === 'kill' && r.oj !== undefined) kills.set(`${r.on}:${r.ok}.${r.oj}`, r);
          if (r.lt !== undefined) { lts++; if (r.r !== 'kill' || r.m !== 1) bad.push(`${r.t} round ${r.n} marked late (lt ${r.lt}) not a burst's mate`); }
          if (r.by !== undefined && r.k !== undefined) { const k2 = `${r.by}:${r.k}.${r.j ?? -1}`, w = takenAt.get(k2); if (w && r.t > w.d + 0.05) bad.push(`${r.t} round ${r.n} from ${k2}`); }
        }
        for (const blk of L.drill.view()) for (const m of blk.men) {
          const key2 = `${blk.id}:${m.k}.${m.j ?? -1}`, was = takenAt.get(key2);
          if (m.kd) { const l = kds.get(key2) ?? []; if (!l.length || l[l.length - 1].join() !== m.kd.join()) l.push([...m.kd]); kds.set(key2, l); }
          if (was && (m.d === undefined || m.kd?.join(',') !== was.kd)) bad.push(`${L.sim.t} ${key2} up again or knocked down again`);
          if (was || m.d === undefined || m.j === undefined) continue;
          const sm = [...d.squads.values()].filter((q) => q.u.id === blk.id).flatMap((q) => q.men).find((x) => x.k === m.k && x.j === m.j);
          if (!sm || !took.has(sm)) continue;
          takenAt.set(key2, { d: m.d, kd: m.kd?.join(',') ?? '' });
          // (one of his stunning burst's: lying since it without a break, not up and thrown down again by another round,
          // and inside his stunned lie)
          const l = kds.get(key2) ?? [], i = sm.stun ? l.findIndex((w) => w[1] === sm.stun!.n) : -1;
          const cal = (L.sim.unit(sm.stun?.s ?? -1)?.cal ?? 'field') as string;
          if (sm.stun && Math.hypot(m.x - sm.stun.x, m.y - sm.stun.y) > RING[cal][2] + 0.3) bad.push(`${L.sim.t} ${key2} taken stunned ${Math.hypot(m.x - sm.stun.x, m.y - sm.stun.y).toFixed(1)} m from his burst (stun ${RING[cal][2]})`);
          if (!sm.stun || !m.kd || !(m.kd[0] < m.d && m.d < m.kd[0] + m.kd[2]) || sm.stun.e <= m.d || i < 0 || !l.slice(i).every((w, q, a) => q === 0 || w[0] < a[q - 1][0] + a[q - 1][2] + 1e-6)) bad.push(`${L.sim.t} ${key2} taken stunned not lying from his burst: ${JSON.stringify(l)} stun ${JSON.stringify(sm.stun)} d ${m.d}`);
          else {
            // (his kill round: the burst's, booked as he falls)
            const r = kills.get(key2);
            if (!r || r.m !== 1 || r.lt !== sm.stun.n || r.by !== sm.stun.s || Math.abs(r.t - m.d) > 0.051 || Math.abs(r.x - Math.round(sm.stun.x * 10) / 10) > 0.051 || Math.abs(r.y - Math.round(sm.stun.y * 10) / 10) > 0.051) bad.push(`${L.sim.t} ${key2} taken stunned: kill round ${r ? JSON.stringify({ t: r.t, m: r.m, by: r.by, x: r.x, y: r.y }) : 'none'}, his burst's from ${sm.stun.s} at ${sm.stun.x.toFixed(1)},${sm.stun.y.toFixed(1)}`);
          }
        }
      }
      const perBurst = new Map<number, number>();
      let stunned = 0, risen = 0;
      for (const sq of d.squads.values()) for (const m of sq.men) if (m.stun) {
        stunned++;
        perBurst.set(m.stun.n, (perBurst.get(m.stun.n) ?? 0) + 1);
        if (m.fell === undefined || m.fell > m.kd![0] + m.kd![2] + 1e-6) risen++;
      }
      const st = d.hurtLog.stun, late = [...st.late].sort((a, b) => a - b);
      console.log(`${B.name} stunned: marked ${st.marked}, taken ${st.taken} (seen ${takenAt.size}), risen ${risen}, most a burst ${Math.max(0, ...perBurst.values())}, taken after the booking median ${late[Math.floor(late.length / 2)] ?? '-'} s, longest ${late[late.length - 1] ?? '-'} s; faults ${bad.length}${bad.length ? ': ' + bad.slice(0, 4).join(' | ') : ''}`);
      expect(bad).toEqual([]);
      expect(Math.max(0, ...perBurst.values())).toBeLessThanOrEqual(W.stunned.cap);
      expect(st.marked).toBeGreaterThanOrEqual(stunned);
      expect(st.taken + risen).toBeLessThanOrEqual(stunned);
      // (every man taken is one seen taken here: as his burst's, inside his stunned lie, lying since it)
      expect(takenAt.size).toBe(st.taken);
      expect(took.size).toBe(st.taken);
      // (the wire, 5.4: every taken man's kill round, and no other, carries the earlier burst's number)
      expect(lts).toBe(st.taken);
      // (when: as the drawing off draws that loss, so never later than its own hold and a think after the booking)
      for (const x of late) expect(x).toBeLessThanOrEqual(DRILL.blast.hold + 0.5 + 1e-6);
    }
  });

  it('across the battles: shares near their figures, bursts of two to four, few shrunk', () => {
    // (a battle not fought above in this run, as when one is picked out with -t: fought here, switch on, blasts on)
    for (const B of BATTLES) if (!all.some((r) => r.name === B.name)) { const on = watch(B.open(F(B, { wounded: true })), true); all.push({ name: B.name, seat: seatOf(B.seed), on, plain: on }); }
    expect(all.length).toBe(BATTLES.length);
    const sum = (f: (s: Seen) => number): number => all.reduce((a, r) => a + f(r.on), 0);
    const armsW = sum((s) => s.hurt.arms ?? 0), armsD = sum((s) => s.dead.arms ?? 0);
    const burstW = sum((s) => s.hurt.burst ?? 0), burstD = sum((s) => s.dead.burst ?? 0);
    const allW = sum((s) => Object.values(s.hurt).reduce((a, b) => a + b, 0)), allD = sum((s) => Object.values(s.dead).reduce((a, b) => a + b, 0));
    const sizes = all.flatMap((r) => r.on.sizes), bursts = sum((s) => s.bursts), shrunk = sum((s) => s.shrunk);
    console.log(`all: small arms wounded ${armsW} of ${armsW + armsD} (${((100 * armsW) / (armsW + armsD)).toFixed(0)}%), bursts ${burstW} of ${burstW + burstD}; ${(allW / allD).toFixed(2)} wounded to a dead; killing bursts 2: ${sizes.filter((s) => s === 2).length}, 3: ${sizes.filter((s) => s === 3).length}, 4+: ${sizes.filter((s) => s >= 4).length}; shrunk ${shrunk} of ${bursts} (${((100 * shrunk) / Math.max(1, bursts)).toFixed(1)}%)`);
    // Small arms: two in three wounded, within a fair band for a few hundred men.
    expect(armsW / (armsW + armsD)).toBeGreaterThan(0.5);
    expect(armsW / (armsW + armsD)).toBeLessThan(0.8);
    // Over all: about two wounded to each dead.
    expect(allW / allD).toBeGreaterThan(1.2);
    expect(allW / allD).toBeLessThan(3.2);
    // (killing bursts of two and three men, as each ENDS, its stunned men taken later with it: the rulings, section 4)
    const ended = all.flatMap((r) => r.on.ended);
    // (read by seat and together: the rulings, 7.2)
    for (const seat of ['att', 'def'] as const) {
      const e = all.filter((r) => r.seat === seat && THE_EIGHT.some((b) => b.name === r.name)).flatMap((r) => r.on.ended);
      console.log(`the eight, ${seat} seated: killing bursts by the men each ended with 1: ${e.filter((s) => s === 1).length}, 2: ${e.filter((s) => s === 2).length}, 3: ${e.filter((s) => s === 3).length}, 4+: ${e.filter((s) => s >= 4).length}`);
    }
    console.log(`all: killing bursts by the men each ended with 1: ${ended.filter((s) => s === 1).length}, 2: ${ended.filter((s) => s === 2).length}, 3: ${ended.filter((s) => s === 3).length}, 4+: ${ended.filter((s) => s >= 4).length}`);
    for (const k of [2, 3]) expect(ended.filter((s) => s === k).length).toBeGreaterThan(0);
    expect(shrunk / Math.max(1, bursts)).toBeLessThanOrEqual(0.05);
    // Holds never under the least, and not one length for all.
    const holds = all.flatMap((r) => r.on.kdHolds);
    expect(Math.min(...holds)).toBeGreaterThanOrEqual(W.kd.least - 1e-6);
    expect(new Set(holds).size).toBeGreaterThan(10);
  });

  it('only salts 381 to 399 are new', async () => {
    const salts = (src: string): Set<number> => new Set([...src.matchAll(/roll\(this\.seed,[^;]*?, (\d+)\)/g)].map((m) => Number(m[1])));
    const now = salts(drillText);
    if (BASE) {
      const was = salts(((await import(/* @vite-ignore */ BASE + '/src/battle/land/drill.ts?raw')) as { default: string }).default);
      const fresh = [...now].filter((s) => !was.has(s));
      console.log(`new salts: ${fresh.sort((a, b) => a - b).join(', ')}`);
      expect(fresh.length).toBeGreaterThan(0);
      for (const s of fresh) { expect(s).toBeGreaterThanOrEqual(381); expect(s).toBeLessThanOrEqual(399); }
    } else for (const s of now) if (s >= 381 && s <= 399) expect(s).toBeLessThanOrEqual(399);
  });
});
