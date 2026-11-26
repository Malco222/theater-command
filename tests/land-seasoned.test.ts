// Experience you can see (release 2; Brian, 25 November 2026, 15:11Z: "do those 3 ideas": "Experience you can see:
// green squads bunch up and freeze under first fire, veterans spread out and get down fast"). Looks only (ruling 217):
// Drill.seasoned shows a block's training in how its squads move (drill.ts, DR.seasoned): the gaps its men keep, what
// each man does in the first seconds under fire, how its buddy teams rush. These tests hold that it changes nothing in
// the fight, that off is off, that a green squad and a veteran squad are told apart at a glance in the same fight (the
// grade forced to each end), that nobody is left frozen, and that the rule of squad life still holds with it on: no
// squad does one thing as one man (Brian, 25 November 2026, 13:40Z), a gun section of three or four no more than a full
// squad, and that only green men freeze: a veteran pinned on the march goes on as with squad life alone.
import { describe, expect, it } from 'vitest';
import { DRILL, LandBattle, sampleTicket, withTeams, type LandUnit, type MenOut } from '../src/battle/land';

const GAITS = new Set(['walk', 'run', 'advance', 'creep', 'crawl', 'flee']);
const POSES = new Set([...GAITS, 'drive', 'reverse', 'towed', 'fire_stand', 'fire_kneel', 'fire_prone', 'lmg_burst', 'throw_grenade', 'fire', 'idle', 'cower', 'wait', 'hide', 'observe', 'mounted', 'ride', 'horses', 'down', 'halt', 'deployed', 'parked']);
const FIRES = new Set(['fire_prone', 'fire_kneel', 'fire_stand', 'lmg_burst', 'throw_grenade']);
const NOT_FOOT = new Set(['tank', 'gun', 'lorry', 'hmg', 'mortar']);
// Down: lying, or on a knee. A man crouched where he froze is not yet down: he is frozen, and stands still (he goes
// down when his freeze is over).
const LOW = new Set(['fire_prone', 'hide', 'lmg_burst', 'crawl', 'observe', 'fire_kneel', 'wait', 'cower']);
// Frozen: stock still on his feet, or crouched where he stood.
const FROZEN = new Set(['idle', 'cower']);
// What the 3D game draws a pose with (LandMen.cpp StanceOf), as the squad-life tests read it.
const CLIP: Record<string, string> = { hide: 'prone', fire_prone: 'prone', wait: 'kneel', fire_kneel: 'kneel', throw_grenade: 'stand', fire_stand: 'stand' };
const seen = (m: MenOut): string => `${m.p === 'lmg_burst' ? (m.r === 'mg' ? 'gun' : 'prone') : CLIP[m.p] ?? m.p}${FIRES.has(m.p) && m.a ? '*' : ''}`;

const ticket = () => withTeams(sampleTicket('plains', 1.3, { seed: 5, nat: { att: 'USA', def: 'GER' }, style: { att: 'methodical', def: 'bold' } }));
// A town, seed 3: the one sample battle found where first fire pins a block still on the march (46 s in), where it
// does not go on by bounds. The same armies and generals.
const town = () => withTeams(sampleTicket('urban', 1.3, { seed: 3, nat: { att: 'USA', def: 'GER' }, style: { att: 'methodical', def: 'bold' } }));
const sig = (b: LandBattle): string => JSON.stringify({ over: b.over, t: b.t, units: b.units.map((u) => [u.id, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged]) });
const mean = (a: number[]): number => (a.length ? a.reduce((s, x) => s + x, 0) / a.length : NaN);
const part = (a: number[], p: number): number => { const s = [...a].sort((x, y) => x - y); return s.length ? s[Math.min(s.length - 1, Math.floor(p * s.length))] : NaN; };
const median = (a: number[]): number => part(a, 0.5);
const hashOf = (h: number, s: string): number => { for (let i = 0; i < s.length; i++) h = (Math.imul(h, 31) + s.charCodeAt(i)) | 0; return h; };

type Opts = NonNullable<Parameters<LandBattle['drillOn']>[0]>;
const LIFE: Opts = { sections: true, life: true }, ON: Opts = { sections: true, life: true, seasoned: true };

/** One block's first fire: it left the march to go on by bounds. Its men by `section:man`. */
interface Fire {
  id: number; t0: number;
  /** The men up the step before, where each stood as the fire came, and each one's first time down, ground covered before it, and time stood still before it. */
  up: Set<string>; at: Map<string, [number, number]>; drop: Map<string, number>; ground: Map<string, number>; still: Map<string, number>;
  /** The men who froze: how, from when, and until when (absent: not yet). */
  frozen: Map<string, { pose: string; from: number; until?: number }>;
  /** Its sections of five or more: their men, when four in five of each had been down, the most of each seen frozen in one same pose at one moment, and the most of each that went down in one step. */
  secs: Map<number, string[]>; down80: Map<number, number>; onePose: Map<number, number>; oneStep: Map<number, number>;
  done: boolean;
}
/** The first drawn rounds of a spell landing at a block still on the march: what its men up then did in the next six seconds. */
interface FirstRounds { id: number; t0: number; up: Set<string>; down: Set<string>; froze: Set<string>; held: number; done: boolean; cut: boolean }
interface Look {
  sig: string; drawn: number;
  samples: number; alike: number; mass: number; mins: number; badPose: number; out: number;
  /** On the march (deploying, or coming on by bounds), each read of a section of five or more: how wide it is across its block's front, its men's mean distance to their nearest mate, its closest pair, and its closest pair of men both standing or lying still. */
  width: number[]; near: number[]; closest: number[]; closestStill: number[];
  fires: Fire[]; frozenAt4: number; rounds: FirstRounds[];
  /** On the march or in bounds while the fight lasts, each step's read of a group of three or four men on foot (a gun section with its loaders, a section thinned out; officers left out): how many reads, in how many all its men stood in one same frozen pose, and in how many all of them came out of a frozen pose in that one step. */
  small: { reads: number; frozen: number; thaw: number };
  /** Coming on by bounds: the length of each man's every rush, seconds; and when each man of a section of five or more got up to rush, by section. */
  rush: number[]; rises: Map<string, { men: number; at: number[] }>;
}

/**
 * Fights the sample battle with the drill as given, reading what each section of five or more men on foot does: every
 * step (or every `each` steps: then only the result, the sameness and the gaps are read). `grade` forces every block's
 * grade (0 green, 1 veteran) by standing in for the drill's own reading of its experience, so the same fight is seen
 * both ways.
 */
function watch(opts: Opts, grade?: number, each = 1): Look {
  const b = new LandBattle(ticket()), d = b.drillOn(opts);
  if (grade !== undefined) (d as unknown as { grade: (u: LandUnit) => number }).grade = () => grade;
  const L: Look = { sig: '', drawn: 0, samples: 0, alike: 0, mass: 0, mins: 0, badPose: 0, out: 0, width: [], near: [], closest: [], closestStill: [], fires: [], frozenAt4: 0, rounds: [], small: { reads: 0, frozen: 0, thaw: 0 }, rush: [], rises: new Map() };
  const last = new Map<string, string>(), lastPh = new Map<number, string>(), since = new Map<number, number>(), before = new Map<number, Map<string, MenOut>>();
  const air: { on: number; l: number }[] = [], spellLast = new Map<number, number>(), spellAt = new Map<number, number>(), spellDone = new Set<string>(), rushFrom = new Map<string, number>();
  const quiet = DRILL.seasoned.rounds.quiet;
  let seenRound = 0;
  for (let k = 0; !b.over && k < 30000; k++) {
    b.step();
    if (k % each) continue;
    const t = b.t, view = d.view(), read = k % 5 === 0;
    if (read) L.drawn = hashOf(L.drawn, JSON.stringify(view));
    // The spells of fire, as the drill counts them: a round landing after a minute's quiet is a first round again.
    for (const r of d.rounds()) {
      if (r.n <= seenRound) continue;
      seenRound = r.n;
      if (r.by !== undefined && r.on >= 0 && r.l > r.t) air.push({ on: r.on, l: r.l });
    }
    for (let i = air.length - 1; i >= 0; i--) {
      if (air[i].l > t + 1e-6) continue;
      const { on, l } = air[i], lastAt = spellLast.get(on);
      if (lastAt === undefined || l > lastAt + quiet) spellAt.set(on, l);
      spellLast.set(on, Math.max(lastAt ?? 0, l));
      air.splice(i, 1);
    }
    for (const blk of view) {
      const u = b.unit(blk.id)!;
      if (read) for (const m of blk.men) {
        if (m.d === undefined && !POSES.has(m.p)) L.badPose++;
        // Never beyond the ring round his block's box (the block is the truth).
        if (m.d === undefined && Math.hypot(m.x - u.x, m.y - u.y) > Math.hypot(u.w / 2 + 70, u.d / 2 + 70) + 130) L.out++;
      }
      if (u.kind !== 'inf') continue;
      const groups = new Map<number, MenOut[]>(), now = new Map<string, MenOut>();
      for (const m of blk.men) {
        if (m.d !== undefined || NOT_FOOT.has(m.r) || m.wd !== undefined || m.br) continue;
        let g = groups.get(m.k);
        if (!g) groups.set(m.k, (g = []));
        g.push(m);
        now.set(`${m.k}:${m.j ?? -1}`, m);
      }
      const ph = blk.ph, was = lastPh.get(blk.id), prev = before.get(blk.id) ?? now;
      const upNow = (): Set<string> => {
        const up = new Set<string>();
        for (const [g, men] of groups) if (men.length >= 5) for (const m of men) { const key = `${g}:${m.j ?? -1}`, p = prev.get(key); if (p && !LOW.has(p.p) && m.r !== 'officer') up.add(key); }
        return up;
      };
      if (each === 1) {
        // First fire: the block leaves the march to go on by bounds. Only after a march of three seconds or more: a
        // block that changes its mind inside three seconds gives its men no new moment to take up (squad life's calm).
        if (was !== undefined && was !== ph) {
          if (ph === 'bound' && (was === 'deploy' || was === 'column') && t - (since.get(blk.id) ?? 0) >= 3 - 1e-6) {
            const f: Fire = { id: blk.id, t0: t, up: upNow(), at: new Map(), drop: new Map(), ground: new Map(), still: new Map(), frozen: new Map(), secs: new Map(), down80: new Map(), onePose: new Map(), oneStep: new Map(), done: false };
            for (const [g, men] of groups) if (men.length >= 5) f.secs.set(g, men.map((m) => `${g}:${m.j ?? -1}`));
            for (const [key, m] of now) f.at.set(key, [m.x, m.y]);
            L.fires.push(f);
          }
          since.set(blk.id, t);
        }
        // The first rounds of a spell landing at a block still on the march.
        const at = spellAt.get(blk.id);
        if (at !== undefined && !spellDone.has(`${blk.id}:${at}`) && t >= at - 1e-6) {
          spellDone.add(`${blk.id}:${at}`);
          if (ph === 'deploy') L.rounds.push({ id: blk.id, t0: t, up: upNow(), down: new Set(), froze: new Set(), held: 0, done: false, cut: false });
        }
        for (const e of L.rounds) {
          if (e.done || e.id !== blk.id) continue;
          const dt = t - e.t0;
          if (ph !== 'deploy') { e.done = true; e.cut = dt < 6; continue; }
          if (dt > 6.05) { e.done = true; continue; }
          for (const key of e.up) {
            const m = now.get(key);
            if (!m) continue;
            const down = LOW.has(m.p) && m.p !== 'cower' && m.v < 0.05;
            if (down) e.down.add(key);
            if (FROZEN.has(m.p)) e.froze.add(key);
            if (Math.abs(dt - 6) < 0.05 && (down || FROZEN.has(m.p)) && (e.down.has(key) || e.froze.has(key))) e.held++;
          }
        }
        for (const f of L.fires) {
          if (f.done || f.id !== blk.id) continue;
          const dt = t - f.t0;
          if (dt > 12) { f.done = true; continue; }
          const fresh = new Map<number, number>();
          for (const key of f.up) {
            const m = now.get(key);
            if (!m) continue;
            const fr = f.frozen.get(key);
            if (!fr && dt < 1.6 && FROZEN.has(m.p) && (!f.drop.has(key) || m.p === 'cower')) f.frozen.set(key, { pose: m.p, from: dt });
            else if (fr && fr.until === undefined && m.p !== fr.pose) fr.until = dt;
            if (f.drop.has(key)) continue;
            const crouched = m.p === 'cower' && f.frozen.has(key) && f.frozen.get(key)!.until === undefined;
            if (LOW.has(m.p) && !crouched) {
              const p = f.at.get(key)!;
              f.drop.set(key, dt); f.ground.set(key, Math.hypot(m.x - p[0], m.y - p[1]));
              fresh.set(m.k, (fresh.get(m.k) ?? 0) + 1);
            } else if (m.v < 0.05) f.still.set(key, (f.still.get(key) ?? 0) + 0.1);
          }
          if (Math.abs(dt - 4) < 0.05) for (const [key, fr] of f.frozen) if (fr.until === undefined && now.has(key)) L.frozenAt4++;
          for (const [g, keys] of f.secs) {
            const alive = keys.filter((key) => now.has(key));
            if (!alive.length) continue;
            if (!f.down80.has(g) && alive.filter((key) => !f.up.has(key) || f.drop.has(key)).length / alive.length >= 0.8) f.down80.set(g, dt);
            f.oneStep.set(g, Math.max(f.oneStep.get(g) ?? 0, (fresh.get(g) ?? 0) / alive.length));
            if (dt > 4) continue;
            for (const pose of FROZEN) f.onePose.set(g, Math.max(f.onePose.get(g) ?? 0, alive.filter((key) => now.get(key)!.p === pose).length / alive.length));
          }
        }
        // Small groups, the gun's loaders counted: three or four men all frozen in one pose, or all out of it in one step
        // (while the fight lasts: at its last step the winners stand easy).
        if (!b.over && (ph === 'deploy' || ph === 'bound')) for (const [g, all] of groups) {
          const men = all.filter((m) => m.r !== 'officer');
          if (men.length < 3 || men.length > 4) continue;
          L.small.reads++;
          if (FROZEN.has(men[0].p) && men.every((m) => m.p === men[0].p)) L.small.frozen++;
          if (men.every((m) => { const p = prev.get(`${g}:${m.j ?? -1}`); return !!p && FROZEN.has(p.p) && !FROZEN.has(m.p); })) L.small.thaw++;
        }
        // Each man's rushes while his block comes on by bounds: from his getting up to his stopping again.
        for (const [key, m] of now) {
          const id = `${blk.id}:${key}`, from = rushFrom.get(id), moving = GAITS.has(m.p) && m.p !== 'crawl';
          if (ph !== 'bound') rushFrom.delete(id);
          else if (!moving) { if (from !== undefined && from >= 0) L.rush.push(t - from); rushFrom.set(id, -1); }
          else if (from === -1) {
            rushFrom.set(id, t);
            const men = groups.get(m.k)?.length ?? 0;
            if (men >= 5) { const sk = `${blk.id}:${m.k}`; let r = L.rises.get(sk); if (!r) L.rises.set(sk, (r = { men, at: [] })); r.men = Math.max(r.men, men); r.at.push(t); }
          }
        }
        before.set(blk.id, now);
      }
      lastPh.set(blk.id, ph);
      if (!read) continue;
      const rx = Math.cos(u.face), ry = -Math.sin(u.face);
      for (const [g, men] of groups) {
        if (men.length < 5) continue;
        const count = new Map<string, number>(), to = new Map<string, number>();
        for (const m of men) {
          const what = seen(m), key = `${blk.id}:${g}:${m.j ?? -1}`, old = last.get(key);
          count.set(what, (count.get(what) ?? 0) + 1);
          if (old !== undefined && old !== what) to.set(what, (to.get(what) ?? 0) + 1);
          last.set(key, what);
        }
        L.samples++; L.mins += 0.5 / 60;
        if (Math.max(...count.values()) / men.length >= 0.9) L.alike++;
        if (to.size && Math.max(...to.values()) / men.length >= 0.6) L.mass++;
        if (ph !== 'deploy' && ph !== 'bound') continue;
        let lo = Infinity, hi = -Infinity, pair = Infinity, pairStill = Infinity, sum = 0;
        for (const m of men) {
          const across = (m.x - u.x) * rx + (m.y - u.y) * ry;
          let best = Infinity;
          for (const o of men) {
            if (o === m) continue;
            const gap = Math.hypot(o.x - m.x, o.y - m.y);
            best = Math.min(best, gap);
            if (!GAITS.has(m.p) && !GAITS.has(o.p)) pairStill = Math.min(pairStill, gap);
          }
          lo = Math.min(lo, across); hi = Math.max(hi, across); pair = Math.min(pair, best); sum += best;
        }
        L.width.push(hi - lo); L.near.push(sum / men.length); L.closest.push(pair);
        if (pairStill < Infinity) L.closestStill.push(pairStill);
      }
    }
  }
  L.sig = sig(b);
  return L;
}

/** The men up when first fire came, over every block's first fire: each one's first time down, ground before it, time stood still, and the sections' times. */
function firstFire(L: Look) {
  const drop: number[] = [], ground: number[] = [], still: number[] = [], down80: number[] = [], onePose: number[] = [], oneStep: number[] = [], frozenFor: number[] = [];
  let up = 0, frozen = 0, sections = 0, squads = 0, onePoseOnly = 0, thawAsOne = 0;
  for (const f of L.fires) {
    up += f.up.size; frozen += f.frozen.size; sections += f.secs.size;
    for (const key of f.up) { still.push(f.still.get(key) ?? 0); if (f.drop.has(key)) { drop.push(f.drop.get(key)!); ground.push(f.ground.get(key)!); } }
    for (const fr of f.frozen.values()) frozenFor.push((fr.until ?? 12) - fr.from);
    for (const [g, keys] of f.secs) {
      if (f.down80.has(g)) down80.push(f.down80.get(g)!);
      onePose.push(f.onePose.get(g) ?? 0); oneStep.push(f.oneStep.get(g) ?? 0);
      // A squad with four or more men frozen: are they all in one pose, and did they all come out of it in one same half second?
      const fr = keys.filter((key) => f.frozen.has(key)).map((key) => f.frozen.get(key)!);
      if (fr.length < 4) continue;
      squads++;
      if (new Set(fr.map((x) => x.pose)).size < 2) onePoseOnly++;
      if (new Set(fr.map((x) => Math.floor((x.until ?? 12) * 2 + 1e-6))).size < 2) thawAsOne++;
    }
  }
  return { fires: L.fires.length, up, frozen, sections, drop, ground, still, down80, onePose, oneStep, frozenFor, squads, onePoseOnly, thawAsOne };
}

/** Coming on by bounds: for each man's getting up to rush, the share of his section's men (of five or more) who got up within half a second of him, on average. */
function together(L: Look): number {
  const share: number[] = [];
  for (const { men, at } of L.rises.values()) {
    let lo = 0, hi = 0;
    for (let i = 0; i < at.length; i++) {
      while (at[lo] < at[i] - 0.5 - 1e-6) lo++;
      while (hi < at.length && at[hi] <= at[i] + 0.5 + 1e-6) hi++;
      share.push((hi - lo) / men);
    }
  }
  return mean(share);
}

/** The first rounds of a spell landing at blocks still on the march (those that stayed on the march six seconds more): the share of their men up who went down, and who stopped frozen. */
function firstRounds(L: Look) {
  const es = L.rounds.filter((e) => !e.cut);
  const up = es.reduce((n, e) => n + e.up.size, 0);
  return { blocks: es.length, up, down: es.reduce((n, e) => n + e.down.size, 0) / Math.max(1, up), froze: es.reduce((n, e) => n + e.froze.size, 0) / Math.max(1, up), held: es.reduce((n, e) => n + e.held, 0) };
}

/**
 * First fire that stops a block on the march where it is (pinned, or halted to fight: not going on by bounds), in the
 * town battle's first 60 seconds. Of its men up and walking as the fire came (officers left out): how many, how many
 * of them stood in a frozen pose before they were first down, each one's time stood still before it, and each one's
 * first time down. `grade` as in watch.
 */
function halted(opts: Opts, grade?: number) {
  const b = new LandBattle(town()), d = b.drillOn(opts);
  if (grade !== undefined) (d as unknown as { grade: (u: LandUnit) => number }).grade = () => grade;
  const lastPh = new Map<number, string>(), since = new Map<number, number>(), before = new Map<number, Map<string, MenOut>>();
  const fires: { id: number; t0: number; men: Map<string, { froze: boolean; still: number; drop?: number }> }[] = [];
  for (let k = 0; !b.over && b.t < 60; k++) {
    b.step();
    const t = b.t;
    for (const blk of d.view()) {
      if (b.unit(blk.id)!.kind !== 'inf') continue;
      const now = new Map<string, MenOut>();
      for (const m of blk.men) if (m.d === undefined && !NOT_FOOT.has(m.r) && m.r !== 'officer' && m.wd === undefined && !m.br) now.set(`${m.k}:${m.j ?? -1}`, m);
      const ph = blk.ph, was = lastPh.get(blk.id), prev = before.get(blk.id);
      if (was !== undefined && was !== ph) {
        if (prev && (ph === 'pinned' || ph === 'fight') && (was === 'deploy' || was === 'column') && t - (since.get(blk.id) ?? 0) >= 3 - 1e-6) {
          const men = new Map<string, { froze: boolean; still: number; drop?: number }>();
          for (const [key, p] of prev) if (GAITS.has(p.p) && p.p !== 'crawl' && now.has(key)) men.set(key, { froze: false, still: 0 });
          fires.push({ id: blk.id, t0: t, men });
        }
        since.set(blk.id, t);
      }
      lastPh.set(blk.id, ph); before.set(blk.id, now);
      for (const f of fires) {
        if (f.id !== blk.id || t - f.t0 > 12) continue;
        for (const [key, r] of f.men) {
          const m = now.get(key);
          if (!m || r.drop !== undefined) continue;
          if (FROZEN.has(m.p)) r.froze = true;
          if (!GAITS.has(m.p) && !FROZEN.has(m.p)) r.drop = t - f.t0;
          else if (m.v < 0.05) r.still += 0.1;
        }
      }
    }
  }
  const men = fires.flatMap((f) => [...f.men.values()]);
  return { fires: fires.length, up: men.length, froze: men.filter((r) => r.froze).length, still: men.map((r) => r.still), drop: men.filter((r) => r.drop !== undefined).map((r) => r.drop!) };
}

/** Fights the sample battle twice side by side, the drill as `a` and as `c`: how many of the reads (every fifth step: what is drawn, and the rounds) differ. */
function differ(a: Opts, c: Opts): { reads: number; differ: number } {
  const ba = new LandBattle(ticket()), da = ba.drillOn(a), bc = new LandBattle(ticket()), dc = bc.drillOn(c);
  let reads = 0, n = 0;
  for (let k = 0; !ba.over && k < 30000; k++) {
    ba.step(); bc.step();
    if (k % 5) continue;
    reads++;
    if (JSON.stringify(da.view()) !== JSON.stringify(dc.view()) || JSON.stringify(da.rounds()) !== JSON.stringify(dc.rounds())) n++;
  }
  if (sig(ba) !== sig(bc)) n++;
  return { reads, differ: n };
}

describe('experience you can see: green squads bunch up and freeze, veterans spread out and get down fast (looks only, ruling 217)', () => {
  const memo = new Map<string, Look>();
  const look = (name: string, make: () => Look): Look => { let L = memo.get(name); if (!L) memo.set(name, (L = make())); return L; };
  const life = () => look('life', () => watch(LIFE)), on = () => look('on', () => watch(ON));
  const green = () => look('green', () => watch(ON, 0)), vet = () => look('vet', () => watch(ON, 1));

  it('a battle ends exactly the same with it on as with squad life alone and as with no men drawn at all (drawn as sections, and as platoons)', () => {
    const off = new LandBattle(ticket());
    for (let k = 0; !off.over && k < 30000; k++) off.step();
    expect(life().sig).toBe(sig(off));
    expect(on().sig).toBe(sig(off));
    expect(green().sig).toBe(sig(off));
    expect(vet().sig).toBe(sig(off));
    for (const seasoned of [false, true]) {
      const big = new LandBattle(ticket());
      big.drillOn({ sections: true, platoons: true, life: true, seasoned });
      for (let k = 0; !big.over && k < 30000; k++) big.step();
      expect(sig(big)).toBe(sig(off));
    }
  }, 300_000);

  it('off is off: nothing drawn differs unless it is asked for, and it is only on with squad life', () => {
    const b = new LandBattle(ticket());
    expect(b.drillOn({ sections: true, life: true }).seasoned).toBe(false);
    expect(new LandBattle(ticket()).drillOn({ sections: true, seasoned: true }).seasoned).toBe(false);
    expect(new LandBattle(ticket()).drillOn(ON).seasoned).toBe(true);
    // Squad life, and squad life with the switch passed as off: man for man the same at every read.
    const a = differ(LIFE, { sections: true, life: true, seasoned: false });
    expect(a.reads).toBeGreaterThan(500);
    expect(a.differ).toBe(0);
    // Asked for without squad life: the same as not asked for.
    expect(differ({ sections: true }, { sections: true, seasoned: true }).differ).toBe(0);
  }, 300_000);

  it('the same fight draws the same twice over, however often it is read', () => {
    // Read every step, and read every fifth step only: what is drawn at each fifth step is the same.
    const again = watch(ON, undefined, 5);
    expect(again.drawn).toBe(on().drawn);
    expect(again.sig).toBe(on().sig);
    // And it does draw something else than squad life alone.
    expect(on().drawn).not.toBe(life().drawn);
  }, 300_000);

  it('on the march a green section is bunched and a veteran section spread out', () => {
    const g = green(), v = vet();
    expect(g.width.length).toBeGreaterThan(2000);
    expect(v.width.length).toBeGreaterThan(2000);
    // How wide a section is across its block's front, metres.
    expect(mean(g.width)).toBeLessThanOrEqual(24);
    expect(mean(v.width)).toBeGreaterThanOrEqual(38);
    expect(mean(v.width) / mean(g.width)).toBeGreaterThanOrEqual(1.6);
    // Each man's distance to his nearest mate, metres.
    expect(mean(g.near)).toBeLessThanOrEqual(4.0);
    expect(mean(v.near)).toBeGreaterThanOrEqual(5.0);
    // Squad life alone lies between the two.
    expect(mean(life().width)).toBeGreaterThan(mean(g.width));
    expect(mean(life().width)).toBeLessThan(mean(v.width));
  }, 300_000);

  it('bunched is not piled up: green men keep the room squad life gives them', () => {
    // No two men ever closer than 1.5 m cannot be held of any squad: with squad life alone men on the move pass within
    // 0.2 m of each other, and two men stopped are now and then 1.3 m apart (one read in 200). So what is held is that a
    // green section does no worse than that: its closest two stopped men as far apart as squad life alone has them, in
    // 99 reads of 100 at least 1.45 m (positions are read to 0.1 m), and its closest two men 1.5 m apart on average.
    const l = life(), g = green(), v = vet();
    for (const L of [g, v]) {
      expect(Math.min(...L.closestStill)).toBeGreaterThanOrEqual(Math.min(...l.closestStill) - 0.1);
      expect(part(L.closestStill, 0.01)).toBeGreaterThanOrEqual(1.45);
      expect(mean(L.closest)).toBeGreaterThanOrEqual(1.5);
    }
  }, 300_000);

  it('first fire: green men stand frozen a moment and go to ground late, veterans are down at once and near where they were', () => {
    const g = firstFire(green()), v = firstFire(vet()), l = firstFire(life());
    expect(g.fires).toBeGreaterThanOrEqual(4);
    expect(g.up).toBeGreaterThan(300);
    expect(v.up).toBeGreaterThan(300);
    // Green: each man up stands still half a second or more on average, and the middle man is down a second or more after the veterans' middle man.
    expect(mean(g.still)).toBeGreaterThanOrEqual(0.5);
    expect(median(g.drop)).toBeGreaterThanOrEqual(median(v.drop) + 1.0);
    expect(g.frozen).toBeGreaterThan(g.up * 0.5);
    // Veterans: the middle man down inside a second, four in five of every section down inside two, and within 8 m of where the fire found them.
    expect(median(v.drop)).toBeLessThanOrEqual(1.0);
    expect(v.down80.length).toBe(v.sections);
    expect(Math.max(...v.down80)).toBeLessThanOrEqual(2.0);
    expect(mean(v.ground)).toBeLessThanOrEqual(8);
    expect(v.frozen).toBe(0);
    // With squad life alone nobody freezes, and the men are down later and further on than the veterans.
    expect(l.frozen).toBe(0);
    expect(median(l.drop)).toBeGreaterThan(median(v.drop) + 1.0);
    expect(mean(l.ground)).toBeGreaterThan(mean(v.ground) + 5);
  }, 300_000);

  it('the first rounds at a squad still on the march: veterans duck and go on, green men stop where they are, and nobody is held', () => {
    const g = firstRounds(green()), v = firstRounds(vet()), l = firstRounds(life());
    expect(v.blocks).toBeGreaterThanOrEqual(2);
    expect(v.up).toBeGreaterThan(100);
    expect(v.down).toBeGreaterThanOrEqual(0.8);
    expect(g.froze).toBeGreaterThanOrEqual(0.6);
    expect(g.down).toBeLessThan(0.3);
    expect(l.down + l.froze).toBeLessThan(0.05);
    // Six seconds on every one of them is up and going again.
    expect(v.held).toBe(0);
    expect(g.held).toBe(0);
  }, 300_000);

  it('squad life still holds at both ends: no squad does one thing as one man, nobody is left frozen, nothing the 3D game cannot draw', () => {
    for (const L of [green(), vet(), on()]) {
      expect(L.samples).toBeGreaterThan(2000);
      expect(L.alike / L.samples).toBeLessThan(0.08);
      expect(L.mass / L.mins).toBeLessThan(0.5);
      expect(L.badPose).toBe(0);
      expect(L.out).toBe(0);
      // No man who froze under first fire is still frozen four seconds after it.
      expect(L.frozenAt4).toBe(0);
    }
    const g = firstFire(green()), v = firstFire(vet());
    expect(g.squads).toBeGreaterThan(20);
    // Men freeze each for his own length, 1 to 3 s.
    expect(Math.max(...g.frozenFor)).toBeLessThanOrEqual(3.2);
    // A green squad never comes out of its freeze as one man, and never stands all in one same frozen pose.
    expect(g.thawAsOne).toBe(0);
    expect(g.onePoseOnly).toBe(0);
    expect(Math.max(...g.onePose)).toBeLessThan(0.9);
    // A veteran squad is never down in one movement: at most three in five of it go down in any one step.
    expect(Math.max(...v.oneStep)).toBeLessThanOrEqual(0.6);
  }, 300_000);

  it('a gun section too: no group of three or four stands frozen as one man, or comes out of it as one', () => {
    // A section whose key man is its gun (he, his two loaders and a rifleman), or one thinned out to three or four: the
    // checks above read sections of five or more. Its key man is never frozen, whatever he carries (his loaders take
    // his pose), so on the march and in bounds such a group is no oftener all in one frozen pose, or all out of one in
    // one step, than with squad life alone (when this was written: never). With the gun frozen too it was over a
    // hundred reads of a green fight, and three such steps.
    const l = life().small;
    for (const L of [green(), on(), vet()]) {
      expect(L.small.reads).toBeGreaterThan(2000);
      expect(L.small.frozen).toBeLessThanOrEqual(l.frozen);
      expect(L.small.thaw).toBeLessThanOrEqual(l.thaw);
    }
  }, 300_000);

  it('however few are left: two or more men of a group who can freeze are never all given one frozen pose', () => {
    // A frozen man crouches or stands by his place in his squad, one place in so many crouching. A squad thinned out can
    // have none of those places left, or only those: its men then take it by their order among those left. Read from
    // the drill itself, every five seconds of a green fight: each group's men who can freeze (not its leader, nor those
    // who keep by its gun), and the pose each would freeze in.
    type M = { j?: number; lead?: true };
    type Sq = { u: LandUnit };
    const b = new LandBattle(ticket());
    const d = b.drillOn(ON) as unknown as { squads: Map<number, Sq>; memo(sq: Sq): { bySec: Map<number, M[]> }; stock(sq: Sq, m: M): string; grade: (u: LandUnit) => number };
    d.grade = () => 0;
    let groups = 0, thinned = 0, alike = 0;
    for (let k = 0; !b.over && k < 30000; k++) {
      b.step();
      if (k % 50) continue;
      for (const sq of d.squads.values()) {
        if (sq.u.kind !== 'inf') continue;
        for (const all of d.memo(sq).bySec.values()) {
          const men = all.filter((m) => !m.lead);
          if (men.length < 2) continue;
          groups++;
          // (by their places alone these would all crouch, or all stand)
          if (new Set(men.map((m) => (m.j ?? 0) % DRILL.seasoned.cower === 1)).size < 2) thinned++;
          if (new Set(men.map((m) => d.stock(sq, m))).size < 2) alike++;
        }
      }
    }
    expect(groups).toBeGreaterThan(2000);
    expect(thinned).toBeGreaterThan(0);
    expect(alike).toBe(0);
  }, 300_000);

  it('first fire that pins a block on the march: only green men freeze, veterans go on as with squad life alone', () => {
    // Where first fire stops the block (pinned, or halted to fight) a veteran keeps squad life's own moment to take it
    // up, and goes on as he was through it: he is not seen to freeze. (With squad life alone a man in fifty is read as
    // frozen here too, so the veterans are held to that; before this was mended one veteran in four froze.) A green man
    // stands frozen as anywhere.
    const l = halted(LIFE), v = halted(ON, 1), g = halted(ON, 0);
    for (const x of [l, v, g]) {
      expect(x.fires).toBeGreaterThanOrEqual(1);
      expect(x.up).toBeGreaterThan(40);
    }
    expect(v.froze / v.up).toBeLessThanOrEqual(l.froze / l.up + 0.03);
    expect(mean(v.still)).toBeLessThanOrEqual(mean(l.still) + 0.05);
    // Down no later than with squad life alone (to the half second the drill thinks in). Not sooner either: pinned,
    // a veteran takes it up in squad life's own moment.
    expect(median(v.drop)).toBeLessThanOrEqual(median(l.drop) + 0.5);
    expect(g.froze / g.up).toBeGreaterThan(0.3);
    expect(mean(g.still)).toBeGreaterThan(mean(v.still) + 0.3);
  }, 300_000);

  it('coming on by bounds a green squad gets up more nearly together and rushes longer, a veteran squad spreads its rises out', () => {
    // When each man gets up: the share of his section who got up within half a second of him. A green squad's buddy
    // teams rise closer together, a veteran squad's further apart; never near a whole section at once in either.
    // (On the rolling advance squad life already spreads a squad's teams over its whole turn, so a veteran squad's are
    // spread about as squad life alone has them.)
    const tg = together(green()), tv = together(vet()), tl = together(life());
    expect(tg).toBeGreaterThan(tv + 0.03);
    expect(tg).toBeGreaterThan(tl + 0.03);
    expect(tg).toBeLessThan(0.6);
    // Each rush from a man's getting up to his stopping again, the long runs of men catching up left out. A man runs to
    // his next place at a pace of at least 2 m/s, so his rush mostly ends where the ground does: a green man's rushes
    // are longer, but by a few tenths of a second.
    const g = green().rush.filter((x) => x <= 6), v = vet().rush.filter((x) => x <= 6);
    expect(g.length).toBeGreaterThan(1000);
    expect(v.length).toBeGreaterThan(1000);
    expect(median(g)).toBeGreaterThanOrEqual(median(v));
    expect(mean(g)).toBeGreaterThan(mean(v) + 0.15);
  }, 300_000);

  it('the numbers (data/battle/drill.json, seasoned)', () => {
    const E = DRILL.seasoned;
    // Green at the first end of the grade, veteran at the second.
    expect(E.grade[0]).toBeLessThan(E.grade[1]);
    // Green gaps closer than the book's, veteran gaps wider; never under the room two men keep (1.6 m).
    expect(E.step[0]).toBeLessThan(1);
    expect(E.step[1]).toBeGreaterThan(1);
    expect(E.least).toBeGreaterThanOrEqual(1.6);
    expect(E.least).toBeLessThanOrEqual(DRILL.sections.space);
    expect(E.inside).toBeGreaterThan(0);
    // A green man takes longer to go to ground than a veteran, who is never held (under a quarter of a second).
    expect(E.freeze[0]).toBeGreaterThan(E.quick);
    expect(E.freeze[0]).toBeLessThanOrEqual(E.freeze[1]);
    expect(E.quick).toBeLessThan(0.25);
    // One frozen man in so many crouches, the others stand: two at the least, so a squad's men are never all given one frozen pose.
    expect(Number.isInteger(E.cower)).toBe(true);
    expect(E.cower).toBeGreaterThanOrEqual(2);
    expect(E.knee).toBeGreaterThan(0);
    expect(E.knee).toBeLessThan(1);
    // The first rounds: every spell of it ends by itself.
    for (const span of [E.rounds.after, E.rounds.duck, E.rounds.low]) expect(span[0]).toBeLessThanOrEqual(span[1]);
    expect(E.rounds.quiet).toBeGreaterThan(E.rounds.after[1] + E.rounds.duck[1] + E.rounds.low[1]);
    expect(E.rounds.bent).toBeGreaterThan(0);
    expect(E.rounds.bent).toBeLessThan(1);
    // Green teams get up closer together and rush longer, veteran teams further apart and shorter.
    expect(E.rush.close[0]).toBeLessThan(1);
    expect(E.rush.close[1]).toBeGreaterThan(1);
    expect(E.rush.long).toBeGreaterThan(0);
    expect(E.rush.long).toBeLessThan(1);
  });
});
