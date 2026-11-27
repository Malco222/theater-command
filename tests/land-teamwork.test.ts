// Machine gun and mortar teams at work (release 2; Brian, 25 November 2026, 14:15Z: machine gun and mortar teams "lay
// down suppressing fire, such as the German MG 42 and the US machine gun", and 13:40Z: "i dont want all the soldiers of
// a squad doing the exact same thing at the same time"). Looks only (ruling 217): Drill.teamwork has each support team's
// guns set up on ground of their own choosing, fire in bursts and spells so the team's guns take turns, sweep their own
// shares of the target, lift as their own men close, and move one at a time while the rest cover; and each crew works
// its gun as men with jobs (drill.ts atWork, crewMan, DR.teamwork): the number two at the feed, the team leader with his
// glasses, bearers on watch, at the bombs or running ammunition up. These tests hold that it changes nothing in the
// fight, that a crew no longer does one thing as one man, that a team's guns are not all firing (nor all silent) at once,
// that each number two kneels at his own army's feed side, that a team packs up a gun at a time, that a gun never hops a
// few metres, that a gun sweeps within the arc its mount allows, that it lifts beyond the target as its own men close,
// and that the 3D game is sent nothing it cannot draw. And the three drills of that evening (Brian 18:39Z, "keep going"):
// the guns' last-ditch fire laid just in front of their own line as the enemy closes on it (the mortars' bombs between
// the two), the ammunition running short and fed again by the bearers from a dump behind the gun, and a gun's stoppage
// or a mortar's misfire now and then, cleared by the crew in seconds.
import { describe, expect, it } from 'vitest';
import { LandBattle, sampleTicket, withTeams, type MenOut } from '../src/battle/land';

const GAITS = new Set(['walk', 'run', 'advance', 'creep', 'crawl', 'flee']);
const POSES = new Set([...GAITS, 'drive', 'reverse', 'towed', 'fire_stand', 'fire_kneel', 'fire_prone', 'lmg_burst', 'throw_grenade', 'fire', 'idle', 'cower', 'wait', 'hide', 'observe', 'mounted', 'ride', 'horses', 'down', 'halt', 'deployed', 'parked']);
const FOOT = new Set(['inf', 'mtn', 'eng', 'fort', 'cav']);
const wrap = (a: number): number => Math.atan2(Math.sin(a), Math.cos(a));
/** What a team is doing inside (the drill's own record, read past its public face): each gun's work and the final line. */
interface GunWork { ammo: number; jam?: number; mis?: [number, number]; at?: [number, number]; set: number }
interface Inside { men: Array<{ n: number; k?: number; j?: number; role: string; pose: string; aim?: [number, number]; short?: number }>; tw?: { guns: Map<unknown, GunWork>; fpl?: { on: number; own: number } } }

type Ground = Parameters<typeof sampleTicket>[0];
interface Fight { g: Ground; seed: number; att: string; def: string }
const FIGHTS: Fight[] = [
  { g: 'plains', seed: 5, att: 'USA', def: 'GER' },
  { g: 'forest', seed: 23, att: 'UK', def: 'GER' },
  { g: 'hills', seed: 11, att: 'GER', def: 'SOV' },
];
const ticket = (f: Fight) => withTeams(sampleTicket(f.g, 1.3, { seed: f.seed, nat: { att: f.att, def: f.def }, style: { att: 'methodical', def: 'bold' } }));
const sig = (b: LandBattle): string => JSON.stringify({ over: b.over, t: b.t, units: b.units.map((u) => [u.id, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged]) });

interface Seen {
  sig: string; crews: number; alike: number; kinds: number; volleys: number; allFire: number; noneFire: number;
  two: Map<string, { n: number; d: number; right: number }>; packs: number[]; hops: number;
  sweeps: number; inArc: number; lifts: number; beyond: number; badPose: number; out: number;
  /** Seconds (in half-second looks) a team is in its fight; with its final line laid; its guns firing on it, laid in
   * front of the enemy towards our own line; its mortars firing on it, their bombs between the enemy and our line. */
  batt: number; finalT: number; finalFire: number; finalFront: number; fpf: number; fpfFront: number; fpfBeyond: number; fpfOwn: number;
  /** Looks at a set-up gun in its fight; firing; firing short (ammunition low); dry; the longest dry spell, seconds;
   * times a gun was fed. Stoppages and misfires begun, and the seconds guns spent stopped. */
  gunT: number; fireT: number; shortT: number; dry: number; dryRun: number; fed: number; jams: number; misfires: number; jamT: number;
}

/** Fights one battle with the drill as given, reading every half second what each support team's men are doing. */
function watch(f: Fight, opts: { sections?: boolean; platoons?: boolean; life?: boolean; teamwork?: boolean }): Seen {
  const b = new LandBattle(ticket(f)), d = b.drillOn(opts);
  const s: Seen = { sig: '', crews: 0, alike: 0, kinds: 0, volleys: 0, allFire: 0, noneFire: 0, two: new Map(), packs: [], hops: 0, sweeps: 0, inArc: 0, lifts: 0, beyond: 0, badPose: 0, out: 0, batt: 0, finalT: 0, finalFire: 0, finalFront: 0, fpf: 0, fpfFront: 0, fpfBeyond: 0, fpfOwn: 0, gunT: 0, fireT: 0, shortT: 0, dry: 0, dryRun: 0, fed: 0, jams: 0, misfires: 0, jamT: 0 };
  const inside = (d as unknown as { squads: Map<number, Inside> }).squads;
  const jammed = new Set<string>(), misfired = new Set<string>(), dryFor = new Map<string, number>(), had = new Map<string, number>();
  const teams = b.units.filter((u) => u.team);
  const was = new Map<number, string>(), packing = new Map<number, { t0: number; at: number[]; went: Set<number> }>(), from = new Map<string, [number, number]>();
  for (let k = 0; !b.over && k < 30000; k++) {
    b.step();
    if (k % 5) continue;
    const view = d.view();
    for (const u of teams) {
      const blk = view.find((x) => x.id === u.id);
      if (!blk) continue;
      for (const m of blk.men) {
        if (m.d !== undefined) continue;
        if (!POSES.has(m.p)) s.badPose++;
        if (Math.hypot(m.x - u.x, m.y - u.y) > Math.hypot(u.w / 2 + 70, u.d / 2 + 70) + 130) s.out++;
      }
      const men = blk.men.filter((m) => m.d === undefined && m.wd === undefined && !m.br);
      const keys = men.filter((m) => m.j === undefined && (m.r === 'hmg' || m.r === 'mortar'));
      // A pack-up: the times each gun's key man first moves, from the block's limbering.
      const ph = was.get(u.id);
      was.set(u.id, blk.ph);
      if (ph === 'battery' && blk.ph === 'limbered') packing.set(u.id, { t0: b.t, at: [], went: new Set() });
      const pk = packing.get(u.id);
      if (pk) {
        for (const m of keys) if (GAITS.has(m.p) && !pk.went.has(m.k)) { pk.went.add(m.k); pk.at.push(b.t); }
        if (pk.went.size >= keys.length || b.t - pk.t0 > 20) {
          if (pk.at.length >= 2) s.packs.push(Math.max(...pk.at) - Math.min(...pk.at));
          packing.delete(u.id);
        }
      }
      for (const key of keys) {
        // A hop: a gun picked up and set down again within a few metres, the team standing.
        const id = `${u.id}:${key.k}`;
        if (GAITS.has(key.p)) { if (!from.has(id)) from.set(id, [key.x, key.y]); }
        else if (from.has(id)) {
          const [x0, y0] = from.get(id)!;
          if (blk.ph === 'battery' && Math.hypot(key.x - x0, key.y - y0) < 5) s.hops++;
          from.delete(id);
        }
        if (blk.ph !== 'battery') continue;
        const crew = men.filter((m) => m.k === key.k);
        if (crew.length < 3) continue;
        // What the crew is doing: the gun's own job told apart from his mates' (a gunner and his loader both "firing" are
        // two jobs), the gaits as one.
        const count = new Map<string, number>();
        for (const m of crew) {
          const now = `${m.j === undefined ? 'gun:' : ''}${GAITS.has(m.p) ? 'move' : m.p}`;
          count.set(now, (count.get(now) ?? 0) + 1);
        }
        s.crews++; s.kinds += count.size;
        if (Math.max(...count.values()) / crew.length >= 0.75) s.alike++;
        // The number two: the loader nearest a gun set up and still.
        if (key.r === 'hmg' && key.v === 0 && (key.p === 'fire' || key.p === 'deployed')) {
          const near = crew.filter((m: MenOut) => m.j !== undefined && !GAITS.has(m.p)).sort((a, c) => Math.hypot(a.x - key.x, a.y - key.y) - Math.hypot(c.x - key.x, c.y - key.y))[0];
          if (near) {
            const t = s.two.get(u.nat) ?? { n: 0, d: 0, right: 0 };
            t.n++; t.d += Math.hypot(near.x - key.x, near.y - key.y);
            if ((near.x - key.x) * Math.cos(key.f) - (near.y - key.y) * Math.sin(key.f) > 0) t.right++;
            s.two.set(u.nat, t);
          }
        }
      }
      // Its guns' ammunition, stoppages and final line, from the team's own working.
      const sq = inside.get(u.id), tw = sq?.tw;
      if (sq && tw && blk.ph === 'battery') {
        if (u.engaged !== undefined) s.batt++;
        if (tw.fpl) s.finalT++;
        const fl = tw.fpl, e = fl && b.unit(fl.on), own = fl && b.unit(fl.own);
        for (const gm of sq.men) {
          const w = tw.guns.get(gm);
          if (!w) continue;
          const id = `${u.id}:${gm.n}`;
          if (w.jam !== undefined && !jammed.has(id)) s.jams++;
          if (w.jam !== undefined) { s.jamT += 0.5; jammed.add(id); } else jammed.delete(id);
          if (w.mis && !misfired.has(id)) s.misfires++;
          if (w.mis) misfired.add(id); else misfired.delete(id);
          if (w.ammo > (had.get(id) ?? Infinity) + 1) s.fed++;
          had.set(id, w.ammo);
          if (u.engaged !== undefined && w.at && w.set <= b.t) {
            s.gunT++;
            if (w.ammo < (gm.role === 'hmg' ? 0.5 : 1)) { s.dry++; dryFor.set(id, (dryFor.get(id) ?? 0) + 0.5); s.dryRun = Math.max(s.dryRun, dryFor.get(id)!); } else dryFor.set(id, 0);
          }
          if (gm.role === 'mortar' && e && own && gm.aim && gm.pose === 'fire') {
            const ux = own.x - e.x, uy = own.y - e.y, ul = Math.hypot(ux, uy) || 1, along = ((gm.aim[0] - e.x) * ux + (gm.aim[1] - e.y) * uy) / ul;
            s.fpf++;
            if (along > 0 && along < ul) s.fpfFront++;
            else if (along <= 0) s.fpfBeyond++;
            const [ax, ay] = gm.aim;
            if (b.units.some((o) => o.side === u.side && o.state !== 'gone' && o.state !== 'withdrawn' && Math.abs((ax - o.x) * Math.cos(o.face) - (ay - o.y) * Math.sin(o.face)) <= o.w / 2 && Math.abs((ax - o.x) * Math.sin(o.face) + (ay - o.y) * Math.cos(o.face)) <= o.d / 2)) s.fpfOwn++;
          }
        }
      }
      // A machine gun team in its fight: how many of its set-up guns are firing, where each aims.
      if (u.team !== 'mg' || u.engaged === undefined || blk.ph !== 'battery') continue;
      const set = keys.filter((m) => !GAITS.has(m.p) && (m.p === 'fire' || m.p === 'deployed' || m.p === 'wait'));
      if (set.length >= 2) {
        const firing = set.filter((m) => m.p === 'fire').length;
        s.volleys++;
        if (firing === set.length) s.allFire++;
        if (firing === 0) s.noneFire++;
      }
      const e = b.unit(u.engaged)!;
      let close = Infinity;
      for (const o of b.units) if (o.side === u.side && FOOT.has(o.kind) && o.state !== 'gone' && o.state !== 'routing') close = Math.min(close, b.gap(o, e));
      for (const m of keys) {
        if (m.p !== 'fire' || !m.a) continue;
        s.sweeps++;
        if (Math.abs(wrap(m.f - Math.atan2(m.a[0] - m.x, m.a[1] - m.y))) < (10 * Math.PI) / 180) s.inArc++;
        if (close < 30) { s.lifts++; if (Math.hypot(m.a[0] - e.x, m.a[1] - e.y) > Math.max(e.w, e.d) / 2) s.beyond++; }
        const gm = sq?.men.find((x) => x.k === m.k && x.j === undefined);
        s.fireT++;
        if (gm?.short !== undefined && gm.short > b.t) s.shortT++;
        const fl = tw?.fpl, own = fl && b.unit(fl.own);
        if (fl && own && fl.on === e.id && close >= 30) {
          s.finalFire++;
          if ((m.a[0] - e.x) * (own.x - e.x) + (m.a[1] - e.y) * (own.y - e.y) > 0) s.finalFront++;
        }
      }
    }
  }
  s.sig = sig(b);
  return s;
}

type Count = { [K in keyof Seen]: Seen[K] extends number ? K : never }[keyof Seen];
const sum = (xs: Seen[], k: Count): number => xs.reduce((n, s) => n + s[k], 0);

describe('machine gun and mortar teams at work (looks only, ruling 217)', () => {
  const old = FIGHTS.map((f) => watch(f, { sections: true })), work = FIGHTS.map((f) => watch(f, { sections: true, teamwork: true }));

  it('a battle ends exactly the same with the teams at work as with no men drawn at all (as sections, as platoons, with squad life)', () => {
    FIGHTS.forEach((f, i) => {
      const off = new LandBattle(ticket(f));
      for (let k = 0; !off.over && k < 30000; k++) off.step();
      expect(work[i].sig).toBe(sig(off));
      expect(old[i].sig).toBe(sig(off));
      if (i) return;
      const big = new LandBattle(ticket(f));
      big.drillOn({ sections: true, platoons: true, life: true, teamwork: true });
      for (let k = 0; !big.over && k < 30000; k++) big.step();
      expect(sig(big)).toBe(sig(off));
    });
  }, 900_000);

  it('is off unless asked for', () => {
    expect(new LandBattle(ticket(FIGHTS[0])).drillOn({ sections: true }).teamwork).toBe(false);
    expect(new LandBattle(ticket(FIGHTS[0])).drillOn({ sections: true, teamwork: true }).teamwork).toBe(true);
  });

  it('a crew no longer does one thing as one man: three in four of it alike rarely, where the old crews nearly always were', () => {
    expect(sum(work, 'crews')).toBeGreaterThan(1000);
    const was = sum(old, 'alike') / sum(old, 'crews'), now = sum(work, 'alike') / sum(work, 'crews');
    expect(was).toBeGreaterThan(0.7);
    expect(now).toBeLessThan(0.15);
    expect(now).toBeLessThan(was / 4);
    expect(sum(work, 'kinds') / sum(work, 'crews')).toBeGreaterThan(sum(old, 'kinds') / sum(old, 'crews') + 1);
  });

  it('a team\'s guns take turns: not all firing at once most of the time, and all quiet only rarely', () => {
    expect(sum(work, 'volleys')).toBeGreaterThan(300);
    expect(sum(old, 'allFire') / Math.max(1, sum(old, 'volleys'))).toBeGreaterThan(0.9);
    expect(sum(work, 'allFire') / sum(work, 'volleys')).toBeLessThan(0.7);
    expect(sum(work, 'noneFire') / sum(work, 'volleys')).toBeLessThan(0.1);
  });

  it('each number two kneels close by his gun on his own army\'s feed side: the German and American guns\' left, the British and Soviet right', () => {
    const two = new Map<string, { n: number; d: number; right: number }>();
    for (const s of work) for (const [nat, t] of s.two) {
      const o = two.get(nat) ?? { n: 0, d: 0, right: 0 };
      two.set(nat, { n: o.n + t.n, d: o.d + t.d, right: o.right + t.right });
    }
    for (const nat of ['USA', 'GER', 'UK', 'SOV']) {
      const t = two.get(nat);
      expect(t, nat).toBeDefined();
      expect(t!.n, nat).toBeGreaterThan(30);
      expect(t!.d / t!.n, nat).toBeLessThan(1.6);
      expect(t!.d / t!.n, nat).toBeGreaterThan(0.7);
      if (nat === 'UK' || nat === 'SOV') expect(t!.right / t!.n, nat).toBeGreaterThan(0.85);
      else expect(t!.right / t!.n, nat).toBeLessThan(0.15);
    }
  });

  it('a team packs up a gun at a time, and never picks a gun up to set it down again a few metres off', () => {
    const packs = work.flatMap((s) => s.packs);
    expect(packs.length).toBeGreaterThanOrEqual(2);
    expect(Math.min(...packs)).toBeGreaterThanOrEqual(1);
    expect(sum(work, 'hops')).toBe(0);
  });

  it('a gun sweeps its share of the target within the arc its mount allows, and lifts beyond it as its own men close', () => {
    expect(sum(work, 'sweeps')).toBeGreaterThan(300);
    expect(sum(work, 'inArc') / sum(work, 'sweeps')).toBeGreaterThan(0.97);
    expect(sum(work, 'lifts')).toBeGreaterThan(20);
    expect(sum(work, 'beyond') / sum(work, 'lifts')).toBeGreaterThan(0.9);
  });

  it('as the enemy closes on our own line the guns lay their last-ditch fire just in front of it, and the mortars drop their bombs between', () => {
    expect(sum(work, 'finalT')).toBeGreaterThan(100);
    expect(sum(work, 'finalT') / sum(work, 'batt')).toBeLessThan(0.35);
    expect(sum(work, 'finalFire')).toBeGreaterThan(200);
    expect(sum(work, 'finalFront') / sum(work, 'finalFire')).toBeGreaterThan(0.7);
    expect(sum(work, 'fpf')).toBeGreaterThan(50);
    // A bomb falls between the lines when there is room for a band there; when the enemy is already too close to our
    // line for one, the tube shifts beyond him instead (lifted fire), and no bomb is ever aimed on our own men.
    expect(sum(work, 'fpfFront') / sum(work, 'fpf')).toBeGreaterThan(0.7);
    expect((sum(work, 'fpfFront') + sum(work, 'fpfBeyond')) / sum(work, 'fpf')).toBeGreaterThan(0.97);
    expect(sum(work, 'fpfOwn')).toBe(0);
  });

  it('the guns run short now and then and are fed again by their bearers: short bursts some of the time, dry rarely and briefly', () => {
    const short = sum(work, 'shortT') / sum(work, 'fireT');
    expect(short).toBeGreaterThan(0.02);
    expect(short).toBeLessThan(0.25);
    expect(sum(work, 'dry') / sum(work, 'gunT')).toBeLessThan(0.05);
    expect(Math.max(...work.map((s) => s.dryRun))).toBeLessThan(40);
    expect(sum(work, 'fed')).toBeGreaterThan(20);
  });

  it('a gun stops and a mortar misfires now and then, and the crew clears it in seconds', () => {
    expect(sum(work, 'jams')).toBeGreaterThanOrEqual(3);
    expect(sum(work, 'misfires')).toBeGreaterThanOrEqual(2);
    for (const s of work) expect(s.jams + s.misfires).toBeLessThan(20);
    expect(sum(work, 'jamT') / sum(work, 'jams')).toBeLessThan(10);
  });

  it('only poses the 3D game has, and every man within reach of his block', () => {
    expect(sum(work, 'badPose')).toBe(0);
    expect(sum(work, 'out')).toBe(0);
  });
});
