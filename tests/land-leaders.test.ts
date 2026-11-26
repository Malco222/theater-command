// When the leader falls (release 2; Brian, 25 November 2026, 15:11Z: "do those 3 ideas"): "When the leader falls: the
// squad hesitates a few seconds, then the next man takes charge." Looks only (ruling 217): Drill.leaders has the men
// who were with a fallen leader stay where they are a few seconds, each till his own moment (drill.ts leaderFell and
// hesitate, DR.leaders): the men nearest him look his way, a man caught on the move drops, those firing fire on, then
// the next man takes charge and the rest go on after him a few at a time. And always Brian's rule (25 November,
// 13:40Z): "i dont want all the soldiers of a squad doing the exact same thing at the same time". These tests hold
// that it changes nothing in the fight, that off is off, that the same battle draws the same twice, that no held man
// starts a walk, a run or a rush, that the men stop one after another and never as one man (none of them in the step
// he falls), that no man is up again before his mates are down, that the hold outlasts a change of the block's task
// and ends for every man, that the squad has one leader after it, that the 3D game is never sent a new role for a man
// it has seen, and that the limits of squad life still hold.
import { describe, expect, it } from 'vitest';
import { LandBattle, platoonOf, sampleTicket, withTeams, type Drill, type Man, type MenOut } from '../src/battle/land';
import { livePiece } from '../src/battle/land/live';

const GAITS = new Set(['walk', 'run', 'advance', 'creep', 'crawl', 'flee']);
/** Getting up and going: what no held man may start. (A creep or a crawl clear of a mate is no start.) */
const UP = new Set(['walk', 'run', 'advance']);
const POSES = new Set([...GAITS, 'drive', 'reverse', 'towed', 'fire_stand', 'fire_kneel', 'fire_prone', 'lmg_burst', 'throw_grenade', 'fire', 'idle', 'cower', 'wait', 'hide', 'observe', 'mounted', 'ride', 'horses', 'down', 'halt', 'deployed', 'parked']);
const FIRES = new Set(['fire_prone', 'fire_kneel', 'fire_stand', 'lmg_burst', 'throw_grenade']);
const NOT_FOOT = new Set(['tank', 'gun', 'lorry', 'hmg', 'mortar']);
// What the 3D game draws a pose with (as tests/land-squad-life.test.ts has it).
const CLIP: Record<string, string> = { hide: 'prone', fire_prone: 'prone', wait: 'kneel', fire_kneel: 'kneel', throw_grenade: 'stand', fire_stand: 'stand' };
const seen = (m: MenOut): string => `${m.p === 'lmg_burst' ? (m.r === 'mg' ? 'gun' : 'prone') : CLIP[m.p] ?? m.p}${FIRES.has(m.p) && m.a ? '*' : ''}`;
/** The same, of a man in the drill: what he is seen doing now. */
const doing = (m: Man): string => `${m.pose === 'lmg_burst' ? (m.role === 'mg' ? 'gun' : 'prone') : CLIP[m.pose] ?? m.pose}${FIRES.has(m.pose) && m.aim ? '*' : ''}`;
/**
 * The time the men have to go down in after their leader falls (battle seconds): the last man caught on the move is
 * down within 0.8 (drill.json leaders.drop), and no man is up again for half a second more (one think of the drill).
 */
const DOWN = 0.8 + 0.5;

const ticket = () => withTeams(sampleTicket('plains', 1.3, { seed: 5, nat: { att: 'USA', def: 'GER' }, style: { att: 'methodical', def: 'bold' } }));
const sig = (b: LandBattle): string => JSON.stringify({ over: b.over, t: b.t, units: b.units.map((u) => [u.id, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged]) });
const hash = (s: string): number => { let h = 0x811c9dc5; for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 0x01000193); } return h >>> 0; };
/** The group a man is in (his section, and in a platoon which of its squads), and whether he leads it. */
const grp = (m: Man): number => (m.sec ?? m.k) * 8 + (m.sub ?? 0);
const leads = (m: Man): boolean => !!m.lead || (m.j === undefined && !!m.hasSec && m.role === 'rifle');
const turnTo = (m: Man, x: number, y: number): number => Math.abs(Math.atan2(Math.sin(Math.atan2(x - m.x, y - m.y) - m.face), Math.cos(Math.atan2(x - m.x, y - m.y) - m.face)));

interface Fall {
  t: number; blk: number; kind: 'key' | 'squad' | 'hq' | 'acting'; by: Man;
  /** No hesitation is due: the block running or gone, or fewer than three men were with him. */
  quiet: boolean;
  /** The men with him just before he fell, and of them those the hold is for (not keeping by a weapon, not gone to tend him, not riding). */
  squad: Man[]; mates: Man[];
  /** Each held man's own moment as the drill set it when the leader fell, the latest of them, and the men still held. */
  own: Map<Man, number>; last: number; open: Set<Man>;
  /**
   * Men let go, the steps they were let go at, and when the first was. Not counted among those the hold lets go: men
   * their block's own leash had got up and running while they were held (leashed: the block gone on without them).
   */
  freed: number; instants: Set<number>; firstAt?: number; lastAt?: number; leashed: number;
  starts: number; recalled: number; early: number; late: number;
  /**
   * The men held, in the step he fell and the second after it (the time the drill gives them to look his way or to
   * drop, each at his own moment). How many of them stopped what they were at in the very step he fell (atFall: down
   * from a move, or off their sights). The most of them, as a share of those on their feet, who stopped in one same
   * step and who took up one same new thing in one same step: where three or more of them stood (stop3, same3) and
   * where four or more did (stop4, same4).
   */
  atFall: number; stop3: number; stop4: number; same3: number; same4: number;
  /** Men the hold let go before their mates can all be down (DOWN), a rout aside. */
  soon: number;
  /** The block's phase as he fell, whether it changed while men were held, and the men still held after it had. */
  phase: string; changed: boolean; through: Set<Man>;
  /** The block broke while men were held: when, and man-steps held more than a think after. */
  brokeAt?: number; routHeld: number;
  /** One second after: the men of the squad standing still, and those of them facing the fallen leader. */
  still?: number; faced?: number;
  /** The most of his mates starting to move in one same step in the six seconds after, as a share of them. */
  burst: number;
  /**
   * After the hold: its groups with a man fit to lead and no leader; its groups with two leaders, one of them a man who
   * took charge (many), or both leaders from the start (both: squads joined to another section keep theirs, as before);
   * the men who took charge, each as he is then: a plain rifleman marked to lead (plain), and one of the men who were
   * with the fallen leader (own).
   */
  led?: { none: number; many: number; both: number; acting: { plain: boolean; own: boolean }[]; left: number };
  /**
   * The time from which who leads them is judged: a think after the last man's moment; and where the man who then led
   * them has fallen in his turn before that (they hesitate again), a think after the last man's moment of that fall.
   */
  judge: number;
  closed: boolean;
}

interface Seen {
  sig: string; falls: Fall[]; hashes: number[];
  badPose: number; out: number; samples: number; alike: number;
  /** Changes of the role the 3D game is sent for a man it has seen, by kind ('loader>mg'). */
  roles: Record<string, number>;
  /** Half seconds a group had two leaders, one of them a man who took charge; leaders (light cases) back on their feet. */
  twoLeaders: number; backs: number;
}

/**
 * Plays a battle step by step and reads, for every leader who falls, what the men who were with him do; and every
 * half second what the 3D game is sent. Reads the drill only.
 */
function watch(b: LandBattle, d: Drill, step: () => void, view = true): Seen {
  const s: Seen = { sig: '', falls: [], hashes: [], badPose: 0, out: 0, samples: 0, alike: 0, roles: {}, twoLeaders: 0, backs: 0 };
  const was = new Map<Man, { g: number; lead: boolean; pose: string; doing: string; drop: boolean; down: boolean }>();
  const role0 = new Map<string, string>();
  const open: Fall[] = [];
  for (let k = 0; !b.over && k < 60000; k++) {
    step();
    const t = b.t;
    for (const u of b.units) {
      const sq = d.squad(u.id);
      if (!sq) continue;
      // Leaders fallen this step, each with the men who were with him.
      for (const m of sq.men) {
        const w = was.get(m);
        if (m.fell === undefined || !w || w.down) continue;
        w.down = true;
        if (!w.lead) continue;
        const squad = sq.men.filter((x) => x !== m && x.j !== undefined && x.fell === undefined && !x.job && was.get(x)?.g === w.g);
        const mates = squad.filter((x) => !x.pr && !(x.aid !== undefined && t < x.until) && x.pose !== 'mounted' && x.pose !== 'ride' && x.role !== 'crew' && x.role !== 'holder');
        const f: Fall = {
          t, blk: u.id, kind: m.j === undefined ? 'key' : m.acting ? 'acting' : (m.sub ?? 0) >= platoonOf(u.nat).squads ? 'hq' : 'squad', by: m,
          quiet: sq.phase === 'flee' || sq.phase === 'gone' || u.state === 'routing' || u.state === 'gone' || u.state === 'withdrawn' || squad.length < 3,
          squad, mates, own: new Map(), last: t, open: new Set(), freed: 0, instants: new Set(), leashed: 0, starts: 0, recalled: 0, early: 0, late: 0,
          atFall: 0, stop3: 0, stop4: 0, same3: 0, same4: 0, soon: 0,
          phase: sq.phase, changed: false, through: new Set(), routHeld: 0, burst: 0, judge: t, closed: false,
        };
        for (const x of squad) if (x.holdTo !== undefined) { f.own.set(x, x.holdTo); f.open.add(x); f.last = Math.max(f.last, x.holdTo); }
        f.judge = f.last + 0.7;
        // (men hesitating already, for the leader before this one, hesitate afresh for him: followed under this fall;
        // and who leads the men of a fall before this one is not judged till this hesitation too is over)
        for (const o of open) {
          for (const x of squad) o.open.delete(x);
          if (o.led === undefined && o.squad.some((x) => squad.includes(x))) o.judge = Math.max(o.judge, f.judge);
        }
        s.falls.push(f); open.push(f);
      }
      // No group with two leaders where one of them is a man who took charge.
      if (k % 5 === 0) {
        const by = new Map<number, Man[]>();
        for (const m of sq.men) if (m.fell === undefined && leads(m) && (m.j !== undefined || m.k < u.figures)) { const g = grp(m); by.set(g, [...(by.get(g) ?? []), m]); }
        for (const l of by.values()) if (l.length > 1 && l.some((x) => x.acting)) s.twoLeaders++;
      }
    }
    for (const f of open) {
      const u = b.unit(f.blk)!, sq = d.squad(f.blk)!;
      const broke = sq.phase === 'flee' || sq.phase === 'gone';
      if (broke && f.open.size && f.brokeAt === undefined) f.brokeAt = t;
      if (!f.quiet && f.open.size && sq.phase !== f.phase && !broke) f.changed = true;
      // The men still held: none starts a walk, a run or a rush; each is let go at his own moment, not before it.
      for (const x of [...f.open]) {
        const w = was.get(x)!;
        if (x.fell !== undefined || x.job) { f.open.delete(x); continue; }
        if (x.holdTo !== undefined) {
          if (UP.has(x.pose) && !UP.has(w.pose) && f.mates.includes(x)) { if (GAITS.has(w.pose)) f.recalled++; else f.starts++; }
          if (f.changed) f.through.add(x);
          if (f.brokeAt !== undefined && sq.phase === 'flee' && t > f.brokeAt + 0.5 + 1e-6) f.routHeld++;
          continue;
        }
        f.open.delete(x); f.freed++; f.instants.add(Math.round(t * 10)); f.lastAt = t;
        // One whom his block's leash had got up and running while he was held (the block gone on without him: up and
        // going the step before, with no drop of his own still to come) is held no more: the leash is the block's, and
        // he is not the hold's to let go.
        if (UP.has(w.pose) && !w.drop) { f.leashed++; continue; }
        if (f.firstAt === undefined) f.firstAt = t;
        // (the first of them up is the next man, before his own moment)
        else if (t < f.own.get(x)! - 1e-6 && !broke) f.early++;
        if (t > f.last + 0.5 + 1e-6) f.late++;
        // No man is up again while his mates are still dropping (a rout ends it at once for all).
        if (!broke && t < f.t + DOWN - 1e-6) f.soon++;
      }
      // The step he fell and the second after it: how many of the held men stop what they were at (down from a move,
      // or off their sights), and how many take up one same new thing, in one same step.
      if (!f.quiet && t <= f.t + 1 + 1e-9) {
        const on = [...f.own.keys()].filter((x) => x.fell === undefined && !x.job);
        const to = new Map<string, number>();
        let stop = 0;
        for (const x of on) {
          const now = doing(x);
          if (now === was.get(x)!.doing) continue;
          to.set(now, (to.get(now) ?? 0) + 1);
          if (!GAITS.has(x.pose) && !(FIRES.has(x.pose) && x.aim)) stop++;
        }
        const same = Math.max(0, ...to.values());
        if (t < f.t + 0.05) f.atFall = stop;
        if (on.length >= 3) { f.stop3 = Math.max(f.stop3, stop / on.length); f.same3 = Math.max(f.same3, same / on.length); }
        if (on.length >= 4) { f.stop4 = Math.max(f.stop4, stop / on.length); f.same4 = Math.max(f.same4, same / on.length); }
      }
      // The most of his mates starting to move in one same step, over the six seconds after.
      if (t <= f.t + 6 + 1e-9) {
        let n = 0, on = 0;
        for (const x of f.mates) { if (x.fell !== undefined || x.job) continue; on++; if (!GAITS.has(was.get(x)!.pose) && GAITS.has(x.pose)) n++; }
        if (on >= 4) f.burst = Math.max(f.burst, n / on);
      }
      // One second after: who of the squad, standing still, faces the fallen leader (within 35 degrees).
      if (f.faced === undefined && t >= f.t + 1 - 1e-9) {
        const still = f.squad.filter((x) => x.fell === undefined && !x.job && !GAITS.has(x.pose));
        f.still = still.length; f.faced = still.filter((x) => turnTo(x, f.by.x, f.by.y) < 0.61).length;
      }
      // Once all are let go (and a think has passed): who leads each group the men of the squad are in now. (Where
      // the man who led them then has fallen in his turn meanwhile, once that hesitation too is over: judge.)
      if (!f.open.size && f.led === undefined && t >= f.judge) {
        const left = f.squad.filter((x) => x.fell === undefined);
        const fit = (x: Man): boolean => x.role === 'rifle' && !x.pr && !x.scout && !x.br && !x.job && x.pose !== 'mounted' && x.pose !== 'ride' && !(x.aid !== undefined && t < x.until);
        const acting = new Set<Man>();
        let none = 0, many = 0, both = 0;
        for (const g of new Set(left.map(grp))) {
          const led = sq.men.filter((y) => y.fell === undefined && leads(y) && (y.j !== undefined || y.k < u.figures) && grp(y) === g);
          for (const y of led) if (y.acting) acting.add(y);
          if (led.length > 1) { if (led.some((y) => y.acting)) many++; else both++; }
          if (!led.length && left.some((x) => grp(x) === g && fit(x))) none++;
        }
        const plain = (x: Man): boolean => x.role === 'rifle' && x.lead === true && !x.pr && !x.br && !x.scout && x.pose !== 'mounted' && x.pose !== 'ride';
        f.led = { none, many, both, acting: [...acting].map((x) => ({ plain: plain(x), own: f.squad.includes(x) })), left: left.length };
      }
      if (sq.phase === 'gone' || (f.led && t > f.t + 6.5) || t > f.t + 30) f.closed = true;
    }
    for (let i = open.length - 1; i >= 0; i--) if (open[i].closed) open.splice(i, 1);
    for (const u of b.units) {
      const sq = d.squad(u.id);
      if (!sq) continue;
      for (const m of sq.men) {
        let w = was.get(m);
        if (!w) was.set(m, (w = { g: 0, lead: false, pose: '', doing: '', drop: false, down: false }));
        if (m.fell === undefined) { if (w.down && w.lead) s.backs++; w.g = grp(m); w.lead = leads(m); w.down = false; }
        w.pose = m.pose; w.doing = doing(m); w.drop = m.downAt !== undefined;
      }
    }
    if (k % 5 || !view) continue;
    // What the 3D game is sent: only poses it has, no man beyond his block's ring, no man's role changed once seen.
    const blocks = d.view();
    s.hashes.push(hash(JSON.stringify(blocks)), hash(JSON.stringify(d.rounds())));
    for (const blk of blocks) {
      const u = b.unit(blk.id)!;
      const groups = new Map<number, MenOut[]>();
      for (const m of blk.men) {
        if (m.d !== undefined) continue;
        if (!POSES.has(m.p)) s.badPose++;
        if (Math.hypot(m.x - u.x, m.y - u.y) > Math.hypot(u.w / 2 + 70, u.d / 2 + 70) + 130) s.out++;
        const key = `${blk.id}:${m.k}:${m.j ?? -1}`, r0 = role0.get(key);
        if (r0 !== undefined && r0 !== m.r) s.roles[`${r0}>${m.r}`] = (s.roles[`${r0}>${m.r}`] ?? 0) + 1;
        role0.set(key, m.r);
        if (u.kind !== 'inf' || NOT_FOOT.has(m.r) || m.wd !== undefined || m.br) continue;
        groups.set(m.k, [...(groups.get(m.k) ?? []), m]);
      }
      for (const men of groups.values()) {
        if (men.length < 5) continue;
        const count = new Map<string, number>();
        for (const m of men) count.set(seen(m), (count.get(seen(m)) ?? 0) + 1);
        s.samples++;
        if (Math.max(...count.values()) / men.length >= 0.9) s.alike++;
      }
    }
  }
  s.sig = sig(b);
  return s;
}

/** The test battle with the drill as given. */
function fight(opts: { sections?: boolean; platoons?: boolean; life?: boolean; leaders?: boolean }): Seen {
  const b = new LandBattle(ticket());
  return watch(b, b.drillOn(opts), () => b.step());
}
/** Worked out once, when first asked for. */
const once = <T>(f: () => T): (() => T) => { let v: T | undefined; return () => (v ??= f()); };
const sum = (falls: Fall[], f: (x: Fall) => number): number => falls.reduce((n, x) => n + f(x), 0);

describe('when the leader falls: the squad hesitates, then the next man takes charge (looks only, ruling 217)', () => {
  const off = once(() => { const b = new LandBattle(ticket()); for (let k = 0; !b.over && k < 60000; k++) b.step(); return sig(b); });
  const life = once(() => fight({ sections: true, life: true }));
  const on = once(() => fight({ sections: true, life: true, leaders: true }));
  const big = once(() => fight({ sections: true, platoons: true, life: true, leaders: true }));
  // Normandy as the 3D game plays it (platoons, stretcher bearers on), the switch on.
  const live = once(() => { const L = livePiece('normandy-1944', 11, { me: 'att', life: true, leaders: true }); return watch(L.sim, L.drill, () => { L.tick(0.1, 1); }); });
  /** The falls a hesitation is due at: the block not running or gone, three or more men with him. */
  const due = (s: Seen): Fall[] => s.falls.filter((f) => !f.quiet);

  it('a battle ends exactly the same with the switch on as with no men drawn at all (as sections, as platoons, and Normandy live)', () => {
    expect(on().sig).toBe(off());
    expect(big().sig).toBe(off());
    const L = livePiece('normandy-1944', 11, { me: 'att', life: true });
    for (let k = 0; !L.sim.over && k < 60000; k++) L.tick(0.1, 1);
    expect(L.sim.over).toBeTruthy();
    expect(live().sig).toBe(sig(L.sim));
  }, 300_000);

  it('off is off: not asked for, asked for as false, or asked for with squad life off, every man is sent exactly as before', () => {
    const none = new LandBattle(ticket());
    expect(none.drillOn({ sections: true, life: true }).leaders).toBe(false);
    expect(new LandBattle(ticket()).drillOn({ sections: true, leaders: true }).leaders).toBe(false);
    expect(new LandBattle(ticket()).drillOn({ sections: true, life: true, leaders: true }).leaders).toBe(true);
    // The men and the rounds the 3D game is sent, every half second of the battle.
    expect(life().hashes.length).toBeGreaterThan(1000);
    expect(fight({ sections: true, life: true, leaders: false }).hashes).toEqual(life().hashes);
    expect(fight({ sections: true, leaders: true }).hashes).toEqual(fight({ sections: true }).hashes);
    // And on, it is not the same picture.
    expect(on().hashes).not.toEqual(life().hashes);
  }, 300_000);

  it('the same battle draws the same twice', () => {
    const again = fight({ sections: true, life: true, leaders: true });
    expect(again.hashes).toEqual(on().hashes);
    expect(again.sig).toBe(on().sig);
  }, 300_000);

  it('leaders enough fall to judge by: section leaders drawn as sections, squad and headquarters leaders drawn as platoons', () => {
    const kinds = (s: Seen, k: string): number => s.falls.filter((f) => f.kind === k).length;
    console.log(`leaders fallen: sections ${on().falls.length} (key ${kinds(on(), 'key')}), ${due(on()).length} with a hesitation due; platoons ${big().falls.length} (key ${kinds(big(), 'key')}, squad ${kinds(big(), 'squad')}, headquarters ${kinds(big(), 'hq')}, men who had taken charge ${kinds(big(), 'acting')}), ${due(big()).length} with a hesitation due`);
    expect(kinds(on(), 'key')).toBeGreaterThanOrEqual(15);
    expect(kinds(big(), 'squad')).toBeGreaterThanOrEqual(10);
    expect(kinds(big(), 'hq')).toBeGreaterThanOrEqual(10);
    expect(due(on()).length).toBeGreaterThanOrEqual(12);
    expect(due(big()).length).toBeGreaterThanOrEqual(30);
  }, 300_000);

  it('the hesitation: every man who was with him is held, and none of them gets up from still to walk, run or rush while he is', () => {
    for (const s of [on(), big()]) {
      const falls = due(s);
      // Every mate (not keeping by a weapon, not gone to tend him, not on a carry) has his own moment set, half a second on at the least.
      for (const f of falls) {
        expect(f.own.size).toBeGreaterThanOrEqual(f.mates.length);
        for (const x of f.mates) expect(f.own.get(x)!).toBeGreaterThanOrEqual(f.t + 0.5 - 1e-6);
      }
      const held = sum(falls, (f) => f.own.size);
      console.log(`the hesitation (${s === on() ? 'sections' : 'platoons'}): ${falls.length} falls, ${held} men held; starts from still while held ${sum(falls, (f) => f.starts)}; men crawling whom their block's leash got up and running ${sum(falls, (f) => f.recalled)}; held no more for their block's leash ${sum(falls, (f) => f.leashed)}; let go before their moment ${sum(falls, (f) => f.early)}`);
      expect(sum(falls, (f) => f.starts)).toBe(0);
      expect(sum(falls, (f) => f.early)).toBe(0);
      // (a man already crawling whom his block has left behind is recalled at a run, held or not: the leash is the block's. Few.)
      expect(sum(falls, (f) => f.recalled)).toBeLessThanOrEqual(held * 0.02);
      // Where no hesitation is due (a rout, two men left) no man is held.
      for (const f of s.falls) if (f.quiet && f.squad.length < 3) expect(f.own.size).toBe(0);
    }
  }, 300_000);

  it('never as one man: no held man is made to stop in the step he falls, they look and drop one after another, and none is up again before his mates are down', () => {
    for (const s of [on(), big()]) {
      const falls = due(s).filter((f) => f.own.size > 0);
      const held = sum(falls, (f) => f.own.size);
      const half = falls.filter((f) => f.stop4 >= 0.5 - 1e-9).length;
      console.log(`never as one man (${s === on() ? 'sections' : 'platoons'}): ${falls.length} holds, ${held} men held; stopping in the very step he fell ${sum(falls, (f) => f.atFall)} men at ${falls.filter((f) => f.atFall > 0).length} falls; the most of the held men stopping in one same step of the second after ${Math.max(...falls.map((f) => f.stop3)).toFixed(2)} (half or more of four or more men at ${half} falls); the most of them taking up one same new thing in one same step ${Math.max(...falls.map((f) => f.same3)).toFixed(2)}; men let go before their mates can be down ${sum(falls, (f) => f.soon)}`);
      // The step he falls: the drill stops no man in it. (A man may break off of himself in that very step, for a
      // fresh clip or as his own stride ends: few.)
      expect(sum(falls, (f) => f.atFall)).toBeLessThanOrEqual(held * 0.05);
      for (const f of falls) {
        // Never every held man going down or off his sights in one same step, nor three quarters of four or more; and
        // never so many taking up one same new thing in one same step.
        expect(f.stop3).toBeLessThan(1);
        expect(f.same3).toBeLessThan(1);
        expect(f.stop4).toBeLessThan(0.75);
        expect(f.same4).toBeLessThan(0.75);
      }
      // Half of them in one same step is rare.
      expect(half).toBeLessThanOrEqual(falls.length * 0.1);
      // No man is let go while his mates may still be dropping: the squad is down before a man of it is up again.
      expect(sum(falls, (f) => f.soon)).toBe(0);
    }
  }, 300_000);

  it('the hold outlasts a change of the block\'s task, and it ends: each man at his own moment, at once in a rout, never the squad in one instant', () => {
    for (const s of [on(), big()]) {
      const falls = due(s).filter((f) => f.own.size > 0);
      const through = falls.filter((f) => f.changed && f.through.size > 0);
      const broke = s.falls.filter((f) => f.brokeAt !== undefined);
      const first = sum(falls, (f) => (f.firstAt ?? f.t) - f.t) / falls.length, last = sum(falls, (f) => (f.lastAt ?? f.t) - f.t) / falls.length;
      console.log(`the hold (${s === on() ? 'sections' : 'platoons'}): ${falls.length} holds; the block's task changed during ${falls.filter((f) => f.changed).length}, men still held after at ${through.length}; first man up ${first.toFixed(2)} s after the fall on average, last ${last.toFixed(2)} s; blocks that broke with men held ${broke.length}; half or more of a squad starting to move in one step at ${falls.filter((f) => f.burst >= 0.5).length}, three in five at ${falls.filter((f) => f.burst >= 0.6).length}`);
      expect(through.length).toBeGreaterThanOrEqual(1);
      // Every held man is let go, none later than half a second (one think of the drill) after the last man's moment.
      for (const f of falls) expect(f.open.size).toBe(0);
      expect(sum(falls, (f) => f.late)).toBe(0);
      expect(sum(s.falls, (f) => f.routHeld)).toBe(0);
      // The hold is a few seconds: the next man up a second or so after the fall, the last man within six.
      expect(first).toBeGreaterThan(0.7);
      expect(last).toBeGreaterThan(first + 0.5);
      for (const f of falls) expect(f.last - f.t).toBeLessThan(6);
      // Never the squad in one instant: two or more men let go are let go at two moments or more (a rout aside),
      // and no more than a quarter of a squad (one man at least; the next man apart) in the same half second.
      for (const f of falls) if (f.freed >= 2 && f.brokeAt === undefined) expect(f.instants.size).toBeGreaterThanOrEqual(2);
      for (const f of falls) {
        const at = new Map<number, number>();
        for (const v of f.own.values()) at.set(Math.round(v * 100), (at.get(Math.round(v * 100)) ?? 0) + 1);
        expect(Math.max(...at.values())).toBeLessThanOrEqual(Math.max(1, Math.round(f.own.size * 0.25)));
      }
    }
  }, 300_000);

  it('the men nearest him look his way: a second after, at most falls a man standing still faces the fallen leader, and more do than with the switch off', () => {
    const share = (falls: Fall[]): number => sum(falls, (f) => f.faced ?? 0) / Math.max(1, sum(falls, (f) => f.still ?? 0));
    for (const s of [on(), big()]) {
      const falls = due(s).filter((f) => f.still !== undefined);
      const some = falls.filter((f) => f.faced! > 0).length;
      console.log(`the glance (${s === on() ? 'sections' : 'platoons'}): a man facing the fallen leader a second after at ${some} of ${falls.length} falls; ${(100 * share(falls)).toFixed(1)}% of the men standing still${s === on() ? ` (switch off: ${(100 * share(due(life()))).toFixed(1)}%, at ${due(life()).filter((f) => (f.faced ?? 0) > 0).length} of ${due(life()).length})` : ''}`);
      expect(some / falls.length).toBeGreaterThan(0.75);
      // But never the whole squad turning to stare.
      expect(share(falls)).toBeLessThan(0.75);
    }
    expect(share(due(on()))).toBeGreaterThan(share(due(life())) + 0.1);
  }, 300_000);

  it('one next man: after it every group the men are in has one leader and one only, and a man who took charge was a plain rifleman of the squad', () => {
    for (const s of [on(), big()]) {
      const judged = s.falls.filter((f) => f.led !== undefined && f.led.left > 0);
      const took = judged.filter((f) => f.led!.acting.length > 0);
      console.log(`the next man (${s === on() ? 'sections' : 'platoons'}): ${judged.length} falls judged; a man who took charge leads after ${took.length}; groups left with no leader though a man fit to lead stood ${sum(judged, (f) => f.led!.none)}; groups with two leaders, one a man who took charge ${sum(judged, (f) => f.led!.many)}; with two leaders from the start (squads joined to another section, as before) ${sum(judged, (f) => f.led!.both)}`);
      expect(judged.length).toBeGreaterThanOrEqual(15);
      expect(sum(judged, (f) => f.led!.none)).toBe(0);
      expect(sum(judged, (f) => f.led!.many)).toBe(0);
      expect(s.twoLeaders).toBe(0);
      expect(took.length).toBeGreaterThanOrEqual(5);
      // A plain rifleman: never one who keeps by a weapon, a scout, a stretcher bearer, a man riding.
      for (const f of took) for (const x of f.led!.acting) expect(x.plain).toBe(true);
      // Drawn as platoons a squad keeps to itself: the man who takes charge is one of the men who were with the fallen
      // leader. (Drawn as sections he may have taken charge earlier, of the section they have since joined.)
      if (s === big()) for (const f of took) if (f.kind !== 'key') expect(f.led!.acting.some((x) => x.own)).toBe(true);
    }
    // With the switch off no one takes charge.
    expect(life().falls.some((f) => (f.led?.acting.length ?? 0) > 0)).toBe(false);
  }, 300_000);

  it('the soldier the 3D game draws is never swapped: no man becomes a leader, or stops being one, once it has seen him', () => {
    for (const s of [on(), big(), live()]) for (const c of Object.keys(s.roles)) expect(c.endsWith('>leader')).toBe(false);
    for (const s of [on(), big()]) for (const c of Object.keys(s.roles)) expect(c.startsWith('leader>')).toBe(false);
    // The only roles that change at all are the ones the old drill changes itself (a gun's number two takes the gun
    // over, or a rifle up): the switch adds no kind of change.
    console.log(`roles changed once seen: switch off ${JSON.stringify(life().roles)}; on, sections ${JSON.stringify(on().roles)}, platoons ${JSON.stringify(big().roles)}, Normandy live ${JSON.stringify(live().roles)}`);
    for (const s of [on(), big()]) for (const c of Object.keys(s.roles)) expect(Object.keys(life().roles)).toContain(c);
  }, 300_000);

  it('only poses the 3D game has, every man within reach of his block, and no section doing one thing as one man', () => {
    for (const s of [on(), big()]) {
      expect(s.badPose).toBe(0);
      expect(s.out).toBe(0);
      expect(s.samples).toBeGreaterThan(2000);
      expect(s.alike / s.samples).toBeLessThan(0.08);
    }
    // No worse than squad life alone (a little either way: the men stand elsewhere).
    expect(on().alike / on().samples).toBeLessThan(life().alike / life().samples + 0.01);
  }, 300_000);

  it('a leader back on his feet (a light case, the stretcher bearers on) takes his squad back: no second hesitation, never two leaders in one group', () => {
    // Normandy live, seed 3: two leaders are back on their feet in it. (In seed 7, used here at first, one was; with the
    // men looking and dropping each at his own step the drill picks other men to fall there, and none of its leaders
    // is a light case any more.)
    const L = livePiece('normandy-1944', 3, { me: null, life: true, leaders: true });
    const s = watch(L.sim, L.drill, () => { L.tick(0.1, 1); });
    const judged = s.falls.filter((f) => f.led !== undefined && f.led.left > 0);
    console.log(`Normandy live, seed 3: ${s.falls.length} leaders fallen, ${s.backs} back on their feet; a man who took charge leads after ${judged.filter((f) => f.led!.acting.length > 0).length} falls; half seconds a group had two leaders, one a man who took charge: ${s.twoLeaders} (seed 11: ${live().falls.length} fallen, ${live().backs} back, ${live().twoLeaders})`);
    expect(s.falls.length).toBeGreaterThanOrEqual(15);
    expect(s.backs).toBeGreaterThanOrEqual(1);
    expect(s.twoLeaders).toBe(0);
    expect(live().twoLeaders).toBe(0);
    expect(sum(judged, (f) => f.led!.many)).toBe(0);

    // And the case itself, made to happen (a leader a light case is rare): in the test battle, a squad whose leader lies
    // fallen and whose next man has taken charge. The leader is made a light case whose dressing is done, as the
    // stretcher bearers' drill has it (hurt), and the drill's own rise puts him on his feet.
    const b = new LandBattle(ticket()), d = b.drillOn({ sections: true, platoons: true, life: true, leaders: true });
    const inner = d as unknown as { squads: Map<number, { dressing?: Man[] }>; rise(sq: unknown, n: number): void };
    let done = 0;
    for (let k = 0; !b.over && k < 60000 && done < 3; k++) {
      b.step();
      if (k % 5) continue;
      for (const u of b.units) {
        if (u.state === 'routing' || u.state === 'gone' || u.state === 'withdrawn') continue;
        const men = d.squad(u.id)?.men ?? [];
        const heir = men.find((x) => x.acting && x.lead && x.fell === undefined && men.some((y) => y.fell !== undefined && y.lead && !y.acting && y.wd === undefined && grp(y) === grp(x)));
        if (!heir) continue;
        const old = men.find((y) => y.fell !== undefined && y.lead && !y.acting && y.wd === undefined && grp(y) === grp(heir))!;
        const group = men.filter((x) => x.fell === undefined && grp(x) === grp(heir));
        const holds = group.map((x) => x.holdTo);
        old.wd = b.t - 1000; old.light = true; old.pose = 'wounded';
        const sq = inner.squads.get(u.id)!;
        (sq.dressing ??= []).push(old);
        inner.rise(sq, 1);
        // He is up and leads; the man who took charge is a plain rifleman again; no one hesitates for it.
        expect(old.fell).toBeUndefined();
        expect(old.lead).toBe(true);
        expect(heir.acting).toBeUndefined();
        expect(heir.lead).toBeUndefined();
        expect(men.filter((x) => x.fell === undefined && leads(x) && grp(x) === grp(old)).length).toBe(1);
        expect(group.map((x) => x.holdTo)).toEqual(holds);
        // What the 3D game is sent for them is what it was sent before: the leader a leader, the next man a rifleman.
        const sent = d.view().find((x) => x.id === u.id)!.men;
        expect(sent.find((x) => x.k === old.k && x.j === old.j)!.r).toBe('leader');
        expect(sent.find((x) => x.k === heir.k && x.j === heir.j)!.r).toBe('rifle');
        done++;
        break;
      }
    }
    expect(done).toBe(3);
  }, 300_000);
});
