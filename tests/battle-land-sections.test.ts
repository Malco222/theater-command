// Each model drawn as its whole section (release 2; the 3D ground battle scene for Brian, 21 November 2026, 18:29Z:
// "the ground battle needs a lot more soldiers in it", and "the tanks should be moving"). Looks only (ruling 217): the
// rules' model is the section's key man, and the rest of his section stand round him, fight as men and fall one by one
// as the block loses strength. These tests hold that it changes nothing in the fight, that the men standing are the
// block's strength, that every man who falls has the round that took him, and that the men still move as men.
import { describe, expect, it } from 'vitest';
import { DRILL, LandBattle, sampleTicket, sectionOf, type LandUnit, type MenOut } from '../src/battle/land';
import type { FieldEvent as CoreEvent } from '../src/battle/core';

type Ground = Parameters<typeof sampleTicket>[0];
type Style = 'bold' | 'methodical' | 'mass';
const MOVING = new Set(['walk', 'run', 'advance', 'creep', 'crawl', 'flee', 'drive', 'reverse', 'towed']);
const FIRING = new Set(['fire_stand', 'fire_kneel', 'fire_prone', 'lmg_burst', 'throw_grenade', 'fire']);
const POSES = new Set([...MOVING, ...FIRING, 'idle', 'cower', 'wait', 'hide', 'observe', 'mounted', 'ride', 'horses', 'down', 'halt', 'deployed', 'parked']);
const FOOT = new Set(['officer', 'rifle', 'mg', 'at', 'sapper', 'holder', 'fo', 'leader', 'loader', 'crew']);

/** Normandy's armies on a practice field: the attack American, the defence German. */
const battle = (g: Ground, seed: number, att: Style, def: Style, tanks = false) => {
  const T = sampleTicket(g, 1.3, { seed, nat: { att: 'USA', def: 'GER' }, style: { att, def } });
  if (tanks) { T.att.forces[1].kind = 'arm'; T.def.forces[1].kind = 'arm'; }
  return new LandBattle(T);
};
const sig = (b: LandBattle, ev: CoreEvent[]): string => JSON.stringify({
  over: b.over, t: b.t,
  units: b.units.map((u: LandUnit) => [u.id, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged]),
  wrecks: b.wrecks.map((z) => [z.x, z.y, z.at]),
  said: ev.filter((e) => e.what !== 'shell').map((e) => [e.t, e.what, e.text]),
});
const fight = (b: LandBattle, each?: (k: number) => void) => {
  const ev: CoreEvent[] = [];
  for (let k = 0; !b.over && k < 30000; k++) { b.step(); ev.push(...b.view().events); each?.(k); }
  return ev;
};
const id = (blk: number, m: { k: number; j?: number }): string => `${blk}:${m.k}:${m.j ?? ''}`;

describe('drawn as sections, it is still looks only (ruling 217)', () => {
  it('a battle ends exactly the same with every section drawn as with no men drawn at all', () => {
    for (const [g, seed, att, def] of [['forest', 5, 'methodical', 'bold'], ['urban', 37, 'bold', 'mass'], ['hills', 3, 'mass', 'methodical']] as const) {
      const off = battle(g, seed, att, def), on = battle(g, seed, att, def);
      on.drillOn({ sections: true });
      const a = fight(off), c = fight(on);
      expect(sig(on, c), g).toBe(sig(off, a));
    }
  }, 120_000); // (Its own line: a heavy test, 21.6 s alone, that ran past vitest's 30 s default under load; the design director's leave, 26 Nov 2026 12:05Z.)
  it('replays man for man', () => {
    const one = battle('plains', 11, 'bold', 'methodical'), two = battle('plains', 11, 'bold', 'methodical');
    one.drillOn({ sections: true }); two.drillOn({ sections: true });
    fight(one); fight(two);
    expect(JSON.stringify(two.drill!.view())).toBe(JSON.stringify(one.drill!.view()));
    expect(JSON.stringify(two.drill!.rounds())).toBe(JSON.stringify(one.drill!.rounds()));
  });
});

describe('who is in a section', () => {
  const count = (s: ReturnType<typeof sectionOf>, role: string, pr?: boolean, scout?: boolean) =>
    s.filter(([r, p, sc]) => r === role && (pr === undefined || p === pr) && (scout === undefined || sc === scout)).length;
  it('an American rifle squad of 12: its leader, a BAR team of three, two scouts and six riflemen', () => {
    const s = sectionOf('USA', 'rifle');
    expect(s.length + 1).toBe(12);
    expect(count(s, 'mg')).toBe(1);
    expect(count(s, 'loader', true)).toBe(2);
    expect(count(s, 'rifle', false, true)).toBe(2);
    expect(count(s, 'rifle', false, false)).toBe(6);
  });
  it('a German Gruppe of 10: its leader, an MG 42 team of three and six riflemen; a British section of 10 with its Bren', () => {
    const s = sectionOf('GER', 'rifle');
    expect(s.length + 1).toBe(10);
    expect(count(s, 'mg')).toBe(1);
    expect(count(s, 'loader', true)).toBe(2);
    expect(count(s, 'rifle', false, false)).toBe(6);
    expect(sectionOf('UK', 'rifle').length + 1).toBe(10);
    // An army not named takes the default.
    expect(sectionOf('XYZ', 'rifle').length + 1).toBe(DRILL.sections.rifle.default.men);
  });
  it('a field gun has her crew of seven, a support team its bearers beside the three drawn with each weapon; a tank none', () => {
    expect(sectionOf('USA', 'gun').every(([r, p]) => r === 'crew' && p)).toBe(true);
    expect(sectionOf('USA', 'gun')).toHaveLength(7);
    expect(sectionOf('GER', 'hmg')).toHaveLength(4);
    expect(sectionOf('GER', 'mortar')).toHaveLength(5);
    expect(sectionOf('GER', 'officer')).toHaveLength(3);
    expect(sectionOf('GER', 'at')).toHaveLength(1);
    expect(sectionOf('GER', 'tank')).toHaveLength(0);
  });
  it('a block draws every section, each man known by k and j; a rifle section is led by its leader, its gun towed by her own tractor', () => {
    const b = battle('plains', 11, 'bold', 'methodical');
    const d = b.drillOn({ sections: true });
    for (const blk of d.view()) {
      const u = b.unit(blk.id)!;
      const seen = new Set(blk.men.map((m) => `${m.k}:${m.j ?? ''}`));
      expect(seen.size).toBe(blk.men.length);
      if (u.kind === 'inf') {
        const nat = u.side === 'att' ? 'USA' : 'GER';
        const leaders = blk.men.filter((m) => m.r === 'leader');
        expect(leaders.length).toBeGreaterThan(0);
        for (const l of leaders) expect(blk.men.filter((m) => m.k === l.k).length).toBe(DRILL.sections.rifle[nat].men);
        expect(blk.men.filter((m) => m.j === undefined && m.r !== 'lorry').length).toBe(u.figures);
      }
      if (u.kind === 'art' && !u.team) expect(blk.men.filter((m) => m.r === 'lorry').length).toBe(blk.men.filter((m) => m.r === 'gun').length);
    }
    expect(d.view().reduce((a, blk) => a + blk.men.length, 0)).toBeGreaterThan(800);
  });
});

describe('the men of a section', () => {
  // Three battles watched every half second: the men standing, the rounds that take them, those who keep by a weapon.
  const watch = (g: Ground, seed: number, att: Style, def: Style, tanks = false) => {
    const b = battle(g, seed, att, def, tanks), d = b.drillOn({ sections: true });
    const t = { gap: 0, fellFol: 0, fellKey: 0, killedOnce: 0, killedTwice: 0, pairs: 0, pairsFar: 0, crews: 0, crewsFar: 0, orphans: 0, badPose: 0, idleInFight: 0, strayed: 0, posts: 0, cycles: 0 };
    const kills = new Map<string, number>();
    let last = 0;
    const where = new Map<string, number[]>();
    fight(b, (k) => {
      for (const r of d.rounds()) { if (r.n <= last) continue; last = r.n; if (r.r === 'kill') kills.set(id(r.on, { k: r.ok!, j: r.oj }), (kills.get(id(r.on, { k: r.ok!, j: r.oj })) ?? 0) + 1); }
      if (k % 5) return;
      for (const u of b.units) {
        const sq = d.squad(u.id)!;
        // The men on their feet are the block's strength (its men in all times the share of strength left), to a man.
        if (u.state !== 'withdrawn' && u.state !== 'gone') {
          const up = sq.men.filter((m) => m.fell === undefined).length, want = Math.ceil((sq.men.length * u.str) / u.str0 - 1e-9);
          t.gap = Math.max(t.gap, Math.abs(up - want));
        }
        for (const m of sq.men) {
          if (m.fell !== undefined) continue;
          if (m.j !== undefined && !sq.men.some((x) => x.j === undefined && x.k === (m.sec ?? m.k) && x.fell === undefined) && u.state !== 'gone') t.orphans++;
          if (m.role === 'tank' && sq.phase === 'fight') {
            if (m.cyc) t.cycles++;
            const a = (m.x - u.x) * Math.cos(u.face) - (m.y - u.y) * Math.sin(u.face), key = `${u.id}:${m.k}`;
            const s = where.get(key) ?? [Infinity, -Infinity]; where.set(key, [Math.min(s[0], a), Math.max(s[1], a)]);
          }
          if (!m.pr || m.v > 0) continue;
          const lead = sq.men.find((x) => x !== m && !x.pr && x.fell === undefined && (x.sec ?? x.k) === (m.sec ?? m.k)
            && (m.role === 'loader' ? ['mg', 'at', 'hmg', 'mortar'].includes(x.role) : m.role === 'crew' ? x.role === 'gun' : m.role === 'holder' ? x.role === 'holder' : x.role === 'officer'));
          if (!lead || lead.v > 0) continue;
          const dd = Math.hypot(lead.x - m.x, lead.y - m.y);
          if (m.role === 'crew') { if (m.pose === 'mounted') continue; t.crews++; if (dd > 10) t.crewsFar++; } else { t.pairs++; if (dd > 8) t.pairsFar++; }
        }
      }
      for (const blk of d.view()) {
        const u = b.unit(blk.id)!;
        for (const m of blk.men) {
          if (m.d !== undefined) continue;
          if (!POSES.has(m.p)) t.badPose++;
          if (m.p === 'idle' && u.engaged !== undefined) t.idleInFight++;
          // The ring round the box and its margin (a whole section's width out at its flank, men near its corners may
          // be a stride or two past it for a moment, catching up as the box moves).
          const lim = (blk.ph === 'flee' ? 70 : 45) + (m.r === 'lorry' ? 120 : 0) + 3;
          if (Math.hypot(m.x - u.x, m.y - u.y) > Math.hypot(u.w / 2 + lim, u.d / 2 + lim)) t.strayed++;
        }
      }
    });
    for (const u of b.units) for (const m of d.squad(u.id)!.men) {
      if (m.fell === undefined) continue;
      if (m.j === undefined) t.fellKey++; else t.fellFol++;
      const n = kills.get(id(u.id, m)) ?? 0;
      if (n === 1) t.killedOnce++; else if (n > 1) t.killedTwice++;
    }
    for (const [lo, hi] of where.values()) if (hi - lo > 12) t.posts++;
    return { b, d, t };
  };
  const A = watch('forest', 5, 'methodical', 'bold'), B = watch('plains', 11, 'bold', 'methodical', true), C = watch('hills', 3, 'mass', 'mass', true);
  it('the men on their feet are the block: as it loses strength its sections thin, to a man', () => {
    for (const x of [A, B, C]) { expect(x.t.fellFol).toBeGreaterThan(50); expect(x.t.gap).toBeLessThanOrEqual(1); }
  });
  it('every man who falls has his round, one, the kill: the key man the rules took, and each of his section', () => {
    for (const x of [A, B, C]) { expect(x.t.killedOnce).toBe(x.t.fellKey + x.t.fellFol); expect(x.t.killedTwice).toBe(0); }
  });
  it("a section's key man fallen, the rest of it join another: none is left without one while his block fights", () => {
    for (const x of [A, B, C]) expect(x.t.orphans).toBe(0);
  });
  it('a loader keeps by his gun, a gun crew round their gun, the runners by their officer', () => {
    for (const x of [A, B, C]) {
      expect(x.t.pairs).toBeGreaterThan(1000);
      expect(x.t.pairsFar / x.t.pairs).toBeLessThan(0.01);
      expect(x.t.crews).toBeGreaterThan(1000);
      expect(x.t.crewsFar / x.t.crews).toBeLessThan(0.01);
    }
  });
  it('every pose is one the 3D game has; no man idle in a fight; none strays from his block', () => {
    for (const x of [A, B, C]) { expect(x.t.badPose).toBe(0); expect(x.t.idleInFight).toBe(0); expect(x.t.strayed).toBe(0); }
  });
  it('tanks halted in a fight move between firing posts (a mass excepted: it stands and fights)', () => {
    expect(B.t.cycles).toBeGreaterThan(0);
    expect(B.t.posts).toBeGreaterThan(0);
    expect(C.t.cycles).toBe(0);
  });
  it('a rifle section\'s gunner fallen, his number two takes up the gun', () => {
    let took = 0;
    for (const x of [A, B, C]) for (const u of x.b.units) for (const m of x.d.squad(u.id)!.men) if (m.role === 'mg' && m.j !== undefined && !m.pr && sectionOf(u.side === 'att' ? 'USA' : 'GER', 'rifle')[m.j - 1]?.[0] === 'loader') took++;
    expect(took).toBeGreaterThan(0);
  });
});

describe('the men of a section move as men, step by step (the 3D check)', () => {
  // Every step of a battle, as the 3D game reads it: no one is thrown, turns about in a tick, or goes on the spot.
  const b = battle('hills', 23, 'bold', 'methodical', true), d = b.drillOn({ sections: true });
  const t = { steps: 0, footMax: 0, vehMax: 0, thrown: 0, turnFoot: 0, turnTank: 0, onSpot: 0, still: 0, tankSteps: 0, tankFast: 0, stalls: 0 };
  const was = new Map<string, { x: number; y: number; f: number; same: number; go: number }>(), at = new Map<number, [number, number]>();
  // Men inside each other (the 3D scene, 21 November 21:17Z): every half second, pairs of living men on foot under 0.7 m
  // apart, and how long each pair stays so.
  const ov = { samples: 0, pairs: 0, most: 0, longest: 0, run: new Map<string, number>() };
  fight(b, (k) => {
    t.steps++;
    if (k % 5 === 4) {
      const all: { key: string; x: number; y: number }[] = [];
      for (const blk of d.view()) for (const m of [...blk.men, ...(blk.fo ?? [])] as MenOut[]) {
        if (m.d === undefined && !['tank', 'gun', 'lorry'].includes(m.r) && m.p !== 'mounted' && m.p !== 'ride') all.push({ key: id(blk.id, m) + (m.r === 'fo' ? 'fo' : ''), x: m.x, y: m.y });
      }
      all.sort((p, q) => p.x - q.x);
      const now = new Map<string, number>();
      let n = 0;
      for (let i = 0; i < all.length; i++) for (let q = i + 1; q < all.length && all[q].x - all[i].x < 0.7; q++) {
        if (Math.hypot(all[i].x - all[q].x, all[i].y - all[q].y) >= 0.7) continue;
        n++;
        const key = all[i].key + ' ' + all[q].key, run = (ov.run.get(key) ?? 0) + 0.5;
        now.set(key, run); ov.longest = Math.max(ov.longest, run);
      }
      ov.run = now; ov.samples++; ov.pairs += n; ov.most = Math.max(ov.most, n);
    }
    for (const blk of d.view()) {
      const u = b.unit(blk.id)!, o = at.get(u.id), went = o ? Math.hypot(u.x - o[0], u.y - o[1]) : 0;
      at.set(u.id, [u.x, u.y]);
      for (const m of [...blk.men, ...(blk.fo ?? [])] as MenOut[]) {
        const key = id(blk.id, m), w = was.get(key);
        let same = 0, go = 0;
        if (w && m.d === undefined) {
          const dist = Math.hypot(m.x - w.x, m.y - w.y);
          let df = Math.abs(m.f - w.f) % (2 * Math.PI); if (df > Math.PI) df = 2 * Math.PI - df;
          const can = FOOT.has(m.r) ? 0.9 : 1.8;
          if (dist > can && dist > went + 0.05) t.thrown++;
          if (FOOT.has(m.r)) { t.footMax = Math.max(t.footMax, dist - Math.max(0, went - can)); if (df > 0.8 + 0.02) t.turnFoot++; }
          else { t.vehMax = Math.max(t.vehMax, dist); if (m.r === 'tank' && df > 0.08 + 0.02) t.turnTank++; }
          if (m.r === 'tank') {
            // No faster than a tank goes (12 m a battle second; her block's own pace where the rules drive it faster), and
            // no stop-start while she drives on: a step standing between two driving is a stall.
            t.tankSteps++;
            if (dist > Math.max(12 * 0.1, went) + 0.15) t.tankFast++;
            // A stall: she went nowhere for a step (at most turned), between two steps she drove on at a pace (a step over 0.5 m).
            go = dist > 0.5 ? 1 : 0;
            if (go && w.go === -1) t.stalls++;
            if (!go && w.go === 1 && dist < 0.05) go = -1;
          }
          if (MOVING.has(m.p) && m.v > 0.5 && dist < 0.005 && df < 0.005) { same = w.same + 1; if (same === 10) t.onSpot++; }
          if (!MOVING.has(m.p) && m.p !== 'ride' && m.p !== 'mounted' && m.p !== 'horses' && m.v > 0) t.still++;
        }
        was.set(key, { x: m.x, y: m.y, f: m.f, same, go });
      }
    }
  });
  it('no man is thrown or carried: none goes further in a step than he can run', () => {
    expect(t.thrown).toBe(0); expect(t.footMax).toBeLessThan(1.0); expect(t.vehMax).toBeLessThan(1.8);
  });
  it('no one turns about in a step, and no one goes on the spot; a man still says he is still', () => {
    expect(t.turnFoot).toBe(0); expect(t.turnTank).toBe(0); expect(t.onSpot).toBe(0); expect(t.still).toBe(0);
  });
  it('no two men stand inside each other: a pair under 0.7 m apart only in passing', () => {
    // Before 21 November's mend (Normandy, every half second): 15 such pairs at once on average, up to 36, one pair for
    // 218 s (a gunner creeping back and forth from his own loaders, a runner run into his officer, two batteries'
    // spotters on one place). Now under one at a time here, and none for more than a couple of seconds.
    expect(ov.samples).toBeGreaterThan(300);
    expect(ov.pairs / ov.samples).toBeLessThan(1.5);
    expect(ov.longest).toBeLessThan(5);
  });
  it('a tank drives no faster than a tank can (12 m a second, or her block\'s pace), and seldom stop-starts', () => {
    // A step over the pace is a wheeling block's ring sweeping her round (20 in 92,000 here; 2,620 before 21 November's
    // mend). A stall is a step lost to two orders at odds (56 here; 146 before).
    expect(t.tankSteps).toBeGreaterThan(50000);
    expect(t.tankFast).toBeLessThan(t.tankSteps / 1000);
    expect(t.stalls).toBeLessThan(t.tankSteps / 1000);
  });
});
