// The squad brain (src/battle/land/drill.ts; Brian, 20 November 2026, 20:43Z: "super realistic and meaningful and
// everyone has high AI iq"): the men, tanks and guns inside each block, second by second, for the 3D game. Looks only
// (ruling 217): a battle with the men drawn ends exactly as one without them. These tests hold that, and that the men
// fight as men: in cover, by fire and movement, never idle in a fight, never far from their block.
import { describe, expect, it } from 'vitest';
import { DRILL, LandBattle, rolesFor, sampleTicket, wreckOffset, type LandUnit, type MenBlock } from '../src/battle/land';
import type { FieldEvent as CoreEvent } from '../src/battle/core';

type Ground = Parameters<typeof sampleTicket>[0];
const STY = ['bold', 'methodical', 'mass'] as const;
const MOVING = new Set(['walk', 'run', 'advance', 'creep', 'crawl', 'flee', 'drive', 'reverse', 'towed']);
const FIRING = new Set(['fire_stand', 'fire_kneel', 'fire_prone', 'lmg_burst', 'throw_grenade', 'fire']);
const POSES = new Set([...MOVING, ...FIRING, 'idle', 'cower', 'wait', 'hide', 'observe', 'mounted', 'ride', 'horses', 'down', 'halt', 'deployed', 'parked']);

/** A battle run to its end, the men drawn or not, with every event it sent (shells too). */
const run = (g: Ground, odds: number, seed: number, att: (typeof STY)[number], def: (typeof STY)[number], drill: boolean | number, each?: (b: LandBattle, men: MenBlock[]) => void) => {
  const b = new LandBattle(sampleTicket(g, odds, { seed, style: { att, def } }));
  if (drill === true) b.drillOn();
  const ev: CoreEvent[] = [];
  for (let k = 0; !b.over && k < 30000; k++) {
    if (typeof drill === 'number' && b.t >= drill && !b.drill) b.drillOn();
    b.step();
    ev.push(...b.view().events);
    if (each && b.drill && k % 10 === 0) each(b, b.drill.view());
  }
  return { b, ev };
};
/** Everything the fight decides: who won and how, when, every block's strength, nerve, place and models, the wrecks. */
const sig = (b: LandBattle, ev: CoreEvent[]): string => JSON.stringify({
  over: b.over, t: b.t,
  units: b.units.map((u: LandUnit) => [u.id, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged]),
  wrecks: b.wrecks.map((z) => [z.x, z.y, z.at]),
  said: ev.filter((e) => e.what !== 'shell').map((e) => [e.t, e.what, e.text]),
});

describe('the squad brain is looks only (ruling 217)', () => {
  const grounds: Ground[] = ['plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert'];
  it('a battle ends exactly the same with the men drawn as without them, on every ground and style', () => {
    grounds.forEach((g, i) => {
      const att = STY[i % 3], def = STY[(i + 1) % 3], seed = 41 + i * 13, odds = i % 2 ? 1.6 : 1.15;
      const off = run(g, odds, seed, att, def, false), on = run(g, odds, seed, att, def, true);
      expect(sig(on.b, on.ev), `${g} ${att}/${def}`).toBe(sig(off.b, off.ev));
    });
  });
  it('turned on halfway through a battle, it changes nothing either', () => {
    const off = run('hills', 1.3, 7, 'bold', 'methodical', false), mid = run('hills', 1.3, 7, 'bold', 'methodical', 60);
    expect(mid.b.drill).not.toBeNull();
    expect(sig(mid.b, mid.ev)).toBe(sig(off.b, off.ev));
  });
  it('replays man for man: the same battle draws the same men in the same places', () => {
    const one = run('forest', 1.4, 19, 'mass', 'methodical', true), two = run('forest', 1.4, 19, 'mass', 'methodical', true);
    expect(JSON.stringify(two.b.drill!.view())).toBe(JSON.stringify(one.b.drill!.view()));
  });
  it('the fits and the campaign never pay for it: off unless asked for', () => {
    const b = new LandBattle(sampleTicket('plains', 1.3, { seed: 3 }));
    for (let k = 0; k < 50; k++) b.step();
    expect(b.drill).toBeNull();
  });
});

describe('who is in a block', () => {
  it('a full tray of foot: an officer, two machine guns, an anti-tank man and riflemen in two teams', () => {
    const r = rolesFor('inf', 12, 12);
    expect(r[0]).toEqual(['officer', 0]);
    expect(r.filter(([x]) => x === 'mg')).toHaveLength(2);
    expect(r.filter(([x]) => x === 'at')).toHaveLength(1);
    expect(r.filter(([x, t]) => x === 'rifle' && t === 1).length).toBe(r.filter(([x, t]) => x === 'rifle' && t === 2).length);
    expect(rolesFor('eng', 12, 12).filter(([x]) => x === 'sapper')).toHaveLength(2);
    expect(rolesFor('cav', 12, 12).some(([x]) => x === 'holder')).toBe(true);
    expect(rolesFor('arm', 4, 4).every(([x]) => x === 'tank')).toBe(true);
    expect(rolesFor('art', 3, 3).every(([x]) => x === 'gun')).toBe(true);
  });
  it('a tank stands near where her wreck would lie, and burns where she stood: no jump as she falls', () => {
    expect(wreckOffset(5, 0, 130, 60)).toEqual(wreckOffset(5, 0, 130, 60));
    let checked = 0, shells = 0, onHer = 0;
    // Tanks seldom lose one on the practice grounds: three battles where one does.
    for (const [g, odds, seed] of [['forest', 1.2, 1], ['forest', 1.2, 7], ['forest', 1.5, 7]] as const) {
      const b = new LandBattle(sampleTicket(g, odds, { seed, style: { att: 'bold', def: 'methodical' } }));
      const d = b.drillOn(), was = new Map<string, [number, number]>(), ev: CoreEvent[] = [];
      for (let k = 0; !b.over && k < 30000; k++) {
        for (const u of b.units) if (u.kind === 'arm') for (const m of d.squad(u.id)?.men ?? []) if (m.fell === undefined) was.set(u.id + ':' + m.k, [m.x, m.y]);
        b.step();
        ev.push(...b.view().events);
      }
      for (const u of b.units.filter((x) => x.kind === 'arm')) {
        const sq = d.squad(u.id)!, wrecks = b.wrecks.filter((z) => z.unit === u.id);
        const dead = sq.men.filter((m) => m.fell !== undefined).sort((p, q) => p.fell! - q.fell! || p.k - q.k);
        expect(dead.length).toBe(wrecks.length);
        dead.forEach((m, i) => {
          // Where she was the step before she fell, give or take a step's drive: no tank slides to her wreck.
          const before = was.get(u.id + ':' + m.k)!;
          expect(Math.hypot(m.x - before[0], m.y - before[1])).toBeLessThan(1.3);
          // Her wreck is drawn where she lies, within her block's ground and its margin from the rules' own place for it
          // (her posts are beside it; in woods she may be held back behind her own foot, 50 m and more).
          expect(d.wreckSpot(wrecks[i])).toEqual([m.x, m.y]);
          expect(Math.hypot(m.x - wrecks[i].x, m.y - wrecks[i].y)).toBeLessThan(Math.hypot(u.w, u.d) / 2 + 60);
          // A shell that knocked her out lands on her.
          for (const e of ev.filter((x) => x.what === 'shell' && x.armour && x.on === u.id && Math.abs(x.t - m.fell!) < 0.15)) {
            shells++;
            if (Math.hypot(e.x! - m.x, e.y! - m.y) < 0.2) onHer++;
          }
          checked++;
        });
      }
    }
    expect(checked).toBeGreaterThan(0);
    expect(onHer).toBe(shells);
  });
  it("a shell lands on the man it costs, where he stands", () => {
    let near = 0, all = 0;
    const { b, ev } = run('hills', 1.4, 23, 'methodical', 'methodical', true);
    for (const e of ev.filter((x) => x.what === 'shell' && x.cal !== 'bomb' && !x.armour && x.on !== undefined)) {
      const sq = b.drill!.squad(e.on!)!;
      all++;
      if (sq.men.some((m) => m.fell !== undefined && Math.abs(m.fell - e.t) < 0.15 && Math.hypot(m.x - e.x!, m.y - e.y!) < 0.2)) near++;
    }
    expect(all).toBeGreaterThan(0);
    expect(near / all).toBeGreaterThan(0.95);
  });
});

describe('the men fight as men', () => {
  // Three battles watched second by second after contact.
  const watch = (g: Ground, att: (typeof STY)[number], seed: number) => {
    const t = { men: 0, idleInFight: 0, strayed: 0, outside: 0, outStill: 0, badPose: 0, covered: 0, fighting: 0, leapfrog: 0, hiddenAt: 0, foUp: 0, hullDown: 0, assaultForward: 0 };
    const { b } = run(g, 1.4, seed, att, 'methodical', true, (bt, view) => {
      if (bt.contactAt === null) return;
      for (const blk of view) {
        const u = bt.unit(blk.id)!;
        const up = blk.men.filter((m) => m.d === undefined);
        if (blk.fo) t.foUp++;
        let moving = 0, down = 0;
        for (const m of up) {
          t.men++;
          if (!POSES.has(m.p)) t.badPose++;
          if (m.p === 'idle' && u.engaged !== undefined) t.idleInFight++;
          // In the block's own frame: across its front, along its face.
          const dx = m.x - u.x, dy = m.y - u.y, a = dx * Math.cos(u.face) - dy * Math.sin(u.face), l = dx * Math.sin(u.face) + dy * Math.cos(u.face);
          const lim = (blk.ph === 'flee' ? 70 : 45) + (m.r === 'lorry' ? 120 : 0) + 0.5;
          // The box (and its margin) turns with the block; the ring round it does not.
          if (Math.hypot(dx, dy) > Math.hypot(u.w / 2 + lim, u.d / 2 + lim)) t.strayed++;
          if (Math.abs(a) > u.w / 2 + lim || Math.abs(l) > u.d / 2 + lim) { t.outside++; if (!MOVING.has(m.p) && m.p !== 'ride' && m.p !== 'mounted') t.outStill++; }
          if (blk.ph === 'fight' && m.r !== 'tank' && m.r !== 'gun' && m.r !== 'lorry') { t.fighting++; if (m.c !== 'none') t.covered++; }
          if (MOVING.has(m.p)) moving++; else if (FIRING.has(m.p) || m.p === 'wait') down++;
          if (m.r === 'at' && m.h) t.hiddenAt++;
          if (m.r === 'tank' && m.c === 'crest') t.hullDown++;
          if (blk.ph === 'fight' && m.r === 'rifle' && l > u.d / 2 + 5) t.assaultForward++;
        }
        if (blk.ph === 'bound' && moving >= 2 && down >= 2) t.leapfrog++;
      }
    });
    return { b, t };
  };
  const A = watch('forest', 'methodical', 31), B = watch('urban', 'bold', 37), C = watch('hills', 'mass', 43);
  it('every pose is one the 3D game has', () => { for (const x of [A, B, C]) expect(x.t.badPose).toBe(0); });
  it('no man stands idle while his block is fighting', () => { for (const x of [A, B, C]) expect(x.t.idleInFight).toBe(0); });
  it('no man strays from his block: the block is the truth', () => {
    for (const x of [A, B, C]) {
      expect(x.t.strayed).toBe(0);
      // Left outside its box (it turned, or went on), he is on his way back to it, and seldom.
      expect(x.t.outStill).toBe(0);
      expect(x.t.outside / x.t.men).toBeLessThan(0.005);
    }
  });
  it('in woods and towns, men fighting take cover', () => {
    expect(A.t.covered / A.t.fighting).toBeGreaterThan(0.5);
    expect(B.t.covered / B.t.fighting).toBeGreaterThan(0.85);
  });
  it('under fire, a block goes forward by fire and movement: some up and running, some down and firing', () => {
    for (const x of [A, B, C]) expect(x.t.leapfrog).toBeGreaterThan(0);
  });
  it("a fight's assault works forward of its block's line", () => {
    expect(A.t.assaultForward + B.t.assaultForward + C.t.assaultForward).toBeGreaterThan(0);
  });
  it('a battery firing far has its spotter up with the foot', () => {
    expect(A.t.foUp + B.t.foUp + C.t.foUp).toBeGreaterThan(0);
  });
  it("a holding block's anti-tank man waits hidden for the tanks", () => {
    expect(A.t.hiddenAt + B.t.hiddenAt + C.t.hiddenAt).toBeGreaterThan(0);
  });
  it('the battle over, every man stops where he is', () => {
    for (const x of [A, B, C]) {
      for (const blk of x.b.drill!.view()) for (const m of blk.men) if (m.d === undefined) expect(m.v).toBe(0);
    }
  });
});

describe('the men move as men, step by step (the 3D check, PR 20)', () => {
  // Every step of two battles, as the 3D game reads them: no one is thrown, turns about in a tick, or goes on the spot.
  const FOOT = new Set(['officer', 'rifle', 'mg', 'at', 'sapper', 'holder', 'fo']);
  const look = (g: Ground, att: (typeof STY)[number], seed: number) => {
    const t = { steps: 0, footMax: 0, vehMax: 0, thrown: 0, turnFoot: 0, turnTank: 0, fOut: 0, onSpot: 0, foMade: 0, foLost: 0, foOut: 0, still: 0 };
    const b = new LandBattle(sampleTicket(g, 1.4, { seed, style: { att, def: 'methodical' } }));
    const d = b.drillOn();
    const was = new Map<string, { x: number; y: number; f: number; p: string; v: number; same: number }>();
    const foOf = new Map<number, string>(), at = new Map<number, [number, number]>();
    for (let k = 0; !b.over && k < 30000; k++) {
      b.step(); t.steps++;
      for (const blk of d.view()) {
        const u = b.unit(blk.id)!, o = at.get(u.id), went = o ? Math.hypot(u.x - o[0], u.y - o[1]) : 0;
        at.set(u.id, [u.x, u.y]);
        const fo = (blk.fo ?? []).map((m) => m.k).join(',');
        if (fo) { t.foOut++; if (foOf.has(blk.id) && foOf.get(blk.id) !== fo) t.foMade++; foOf.set(blk.id, fo); } else if (foOf.has(blk.id) && u.state !== 'gone' && u.state !== 'withdrawn') t.foLost++;
        for (const m of [...blk.men, ...(blk.fo ?? [])]) {
          if (m.f <= -Math.PI - 0.005 || m.f > Math.PI + 0.005) t.fOut++;
          const key = blk.id + ':' + m.k, w = was.get(key);
          let same = 0;
          if (w && m.d === undefined) {
            const dist = Math.hypot(m.x - w.x, m.y - w.y);
            let df = Math.abs(m.f - w.f) % (2 * Math.PI); if (df > Math.PI) df = 2 * Math.PI - df;
            // A man runs at most 7.5 m a battle second, a tank or a lorry 16: further only when his block itself goes
            // faster (the rules push it aside), and then only as far as it went.
            const can = FOOT.has(m.r) ? 0.9 : 1.8; // and the view's rounding to 10 cm
            if (dist > can && dist > went + 0.05) t.thrown++;
            if (FOOT.has(m.r)) { t.footMax = Math.max(t.footMax, dist - Math.max(0, went - can)); if (df > 0.8 + 0.02) t.turnFoot++; }
            else { t.vehMax = Math.max(t.vehMax, dist); if (m.r === 'tank' && df > 0.08 + 0.02) t.turnTank++; }
            // Says it goes, at a speed, and neither goes nor turns, a second on end.
            if (MOVING.has(m.p) && m.v > 0.5 && dist < 0.005 && df < 0.005) { same = w.same + 1; if (same === 10) t.onSpot++; }
            if (!MOVING.has(m.p) && m.p !== 'ride' && m.p !== 'mounted' && m.p !== 'horses' && m.v > 0) t.still++;
          }
          was.set(key, { x: m.x, y: m.y, f: m.f, p: m.p, v: m.v, same });
        }
      }
    }
    return t;
  };
  const P = look('hills', 'bold', 23), Q = look('urban', 'mass', 37);
  it('no man is thrown or carried: none goes further in a step than he can run, the spotter included', () => {
    for (const x of [P, Q]) { expect(x.thrown).toBe(0); expect(x.footMax).toBeLessThan(1.0); expect(x.vehMax).toBeLessThan(1.8); }
  });
  it('no one turns about in a step: a man turns at most 0.8 rad a step, a tank 0.08 (she pivots)', () => {
    for (const x of [P, Q]) { expect(x.turnFoot).toBe(0); expect(x.turnTank).toBe(0); }
  });
  it('no tank (nor man) drives on the spot, and a man lying, kneeling or firing says he is still', () => {
    for (const x of [P, Q]) { expect(x.onSpot).toBe(0); expect(x.still).toBe(0); }
  });
  it('facing comes in (-pi, pi]', () => { for (const x of [P, Q]) expect(x.fOut).toBe(0); });
  it("a battery's spotter is one pair for the battle: never made anew, never taken off while his guns stand", () => {
    expect(P.foOut + Q.foOut).toBeGreaterThan(0);
    for (const x of [P, Q]) { expect(x.foMade).toBe(0); expect(x.foLost).toBe(0); }
  });
  it('a battery ordered to move limbers up, and its spotter stays out where he was, watching', () => {
    const b = new LandBattle(sampleTicket('hills', 1.4, { seed: 23, style: { att: 'bold', def: 'methodical' } }));
    const d = b.drillOn();
    let gun: number | undefined;
    for (let k = 0; !b.over && k < 30000 && gun === undefined; k++) { b.step(); gun = d.view().find((x) => x.fo)?.id; }
    expect(gun).toBeDefined();
    const u = b.unit(gun!)!;
    expect(b.command({ do: 'move', units: [u.id], x: u.x, y: u.side === 'att' ? u.y - 250 : u.y + 250 }, u.side)).toBe(true);
    let limbered = 0, lost = 0, jumped = 0;
    const was = new Map<number, [number, number]>();
    for (let k = 0; !b.over && k < 400; k++) {
      b.step();
      const blk = d.view().find((x) => x.id === u.id)!;
      if (blk.ph === 'limbered') limbered++;
      if (!blk.fo) { lost++; continue; }
      for (const m of blk.fo) { const w = was.get(m.k); if (w && Math.hypot(m.x - w[0], m.y - w[1]) > 0.9) jumped++; was.set(m.k, [m.x, m.y]); }
    }
    expect(limbered).toBeGreaterThan(0);
    expect(lost).toBe(0);
    expect(jumped).toBe(0);
  });
});

describe('lorried foot and cavalry (the practice armies have none: a corps of each made so)', () => {
  // The attack's first corps on horses, its second and the defence's second in lorries.
  const ticket = (g: Ground, seed: number, att: (typeof STY)[number], def: (typeof STY)[number]) => {
    const T = sampleTicket(g, 1.3, { seed, style: { att, def } });
    T.att.forces[0].kind = 'cav'; T.att.forces[1].kind = 'mot'; T.def.forces[1].kind = 'mot';
    return T;
  };
  const go = (g: Ground, seed: number, att: (typeof STY)[number], def: (typeof STY)[number], drill: boolean) => {
    const b = new LandBattle(ticket(g, seed, att, def));
    const d = drill ? b.drillOn() : null;
    const t = { poses: new Set<string>(), strayed: 0, thrown: 0, turned: 0, aboardAtFirst: 0, men: 0, piled: 0, footFast: 0, footMost: 0, reboarded: 0, resaddled: 0, waited: 0, climbMoving: 0, stuck: 0 };
    const still = new Map<string, number>();
    const was2 = new Map<string, string>();
    const pileFrom = new Map<number, number>();
    const was = new Map<string, [number, number, number]>(), at = new Map<number, [number, number]>();
    const ev: CoreEvent[] = [];
    for (let k = 0; !b.over && k < 30000; k++) {
      b.step(); ev.push(...b.view().events);
      if (!d || b.over) continue;
      for (const blk of d.view()) {
        const u = b.unit(blk.id)!, o = at.get(u.id), went = o ? Math.hypot(u.x - o[0], u.y - o[1]) : 0;
        at.set(u.id, [u.x, u.y]);
        // Three men or more on one point (off their lorries or horses) for over five seconds: a knot, not a squad.
        const foot = blk.men.filter((m) => m.d === undefined && m.r !== 'lorry' && m.p !== 'mounted' && m.p !== 'ride' && m.p !== 'horses');
        if (foot.some((m) => foot.filter((q) => Math.hypot(q.x - m.x, q.y - m.y) < 1).length >= 3)) { const s0 = pileFrom.get(blk.id) ?? b.t; pileFrom.set(blk.id, s0); if (b.t - s0 > 5) t.piled++; } else pileFrom.delete(blk.id);
        // Where each lorry and horse holder stood last step (before this step's men are booked).
        const before = new Map(blk.men.filter((c) => c.r === 'lorry' || c.r === 'holder').map((c) => [c, was.get(blk.id + ':' + c.k)] as const));
        for (const m of blk.men) {
          if (m.d !== undefined) continue;
          if (u.kind === 'mot' || u.kind === 'cav') { t.poses.add(`${u.kind} ${m.r === 'lorry' ? 'lorry' : 'man'} ${m.p}`); if (k === 0 && u.kind === 'mot' && m.r !== 'lorry' && m.p === 'mounted') t.aboardAtFirst++; }
          // Lorried men and troopers may be further out, waiting for their lorry or running to their horse (drill: ring).
          const ex = m.r === 'lorry' || u.kind === 'mot' || u.kind === 'cav' ? 120 : 0;
          if (Math.hypot(m.x - u.x, m.y - u.y) > Math.hypot(u.w / 2 + 70.5 + ex, u.d / 2 + 70.5 + ex)) t.strayed++;
          const w = was.get(blk.id + ':' + m.k);
          if (w) {
            const dist = Math.hypot(m.x - w[0], m.y - w[1]), can = m.r === 'lorry' || m.r === 'tank' || m.r === 'gun' || m.p === 'mounted' ? 1.8 : m.p === 'ride' || m.p === 'horses' ? 1.6 : 0.9;
            if (dist > can && dist > went + 0.3) t.thrown++;
            // On his feet (not in a lorry or a saddle, nor getting in or out), a man goes no faster than a man runs,
            // however fast his block goes (the 3D check, 21 November 00:33Z): 7.5 m a second, and the view's rounding.
            const p0 = was2.get(blk.id + ':' + m.k), carried = (p: string | undefined): boolean => p === 'mounted' || p === 'ride' || p === 'horses';
            if (m.r !== 'lorry' && m.r !== 'tank' && m.r !== 'gun' && !carried(m.p) && !carried(p0)) { t.footMost = Math.max(t.footMost, dist); if (dist > 0.9) t.footFast++; }
            if (p0 !== undefined && !carried(p0) && p0 !== 'down') { if (u.kind === 'mot' && m.p === 'mounted') t.reboarded++; if (u.kind === 'cav' && m.p === 'ride') t.resaddled++; }
            // Into his lorry, or into the saddle, only once it has halted for him (the review of 21 November).
            if (p0 !== undefined && !carried(p0) && m.r !== 'holder' && (m.p === 'mounted' || m.p === 'ride')) {
              let c: (typeof blk.men)[number] | undefined, cd = Infinity;
              for (const q of before.keys()) if (q.r === (u.kind === 'mot' ? 'lorry' : 'holder') && q.d === undefined) { const dd = Math.hypot(q.x - m.x, q.y - m.y); if (dd < cd) { cd = dd; c = q; } }
              const cw = c && before.get(c);
              if (c && cw && Math.hypot(c.x - cw[0], c.y - cw[1]) > 0.3) t.climbMoving++;
            }
            // Nobody goes in a moving pose on one spot, facing one way, for a second (a lorry at the field's edge halts).
            const key = blk.id + ':' + m.k, n0 = p0 === m.p && dist < 0.005 && Math.abs(m.f - w[2]) < 0.005 && /^(walk|run|advance|creep|crawl|flee|drive|reverse)$/.test(m.p) ? (still.get(key) ?? 0) + 1 : 0;
            still.set(key, n0); if (n0 === 10) t.stuck++;
            if (u.kind === 'mot' && m.p === 'wait' && blk.ph !== 'hold' && blk.ph !== 'fight' && blk.ph !== 'startline' && blk.ph !== 'pinned') t.waited++;
            let df = Math.abs(m.f - w[2]) % (2 * Math.PI); if (df > Math.PI) df = 2 * Math.PI - df;
            if (df > 0.82) t.turned++;
          }
          was.set(blk.id + ':' + m.k, [m.x, m.y, m.f]);
          was2.set(blk.id + ':' + m.k, m.p);
          t.men++;
        }
      }
    }
    return { b, ev, t };
  };
  const runs = [['plains', 504, 'bold', 'methodical'], ['hills', 538, 'methodical', 'mass']] as const;
  const on = runs.map(([g, seed, a, d]) => go(g, seed, a, d, true)), off = runs.map(([g, seed, a, d]) => go(g, seed, a, d, false));
  it('the men drawn change nothing here either', () => { on.forEach((x, i) => expect(sig(x.b, x.ev)).toBe(sig(off[i].b, off[i].ev))); });
  it('lorried men start aboard and ride in their lorries, then get down and fight; the lorries park behind', () => {
    const P = new Set(on.flatMap((x) => [...x.t.poses]));
    expect(on[0].t.aboardAtFirst).toBeGreaterThan(0);
    for (const p of ['mot man mounted', 'mot lorry drive', 'mot lorry parked', 'mot man advance']) expect(P.has(p), p).toBe(true);
    expect([...P].some((p) => /^mot man (fire_|lmg_burst)/.test(p))).toBe(true);
  });
  it('cavalry ride up, get off their horses to fight, and the horse holders lead the horses back', () => {
    const P = new Set(on.flatMap((x) => [...x.t.poses]));
    for (const p of ['cav man ride', 'cav man horses']) expect(P.has(p), p).toBe(true);
    expect([...P].some((p) => /^cav man (fire_|lmg_burst)/.test(p))).toBe(true);
  });
  it('none strays, is thrown or turns about in a step, mounted or not', () => {
    for (const x of on) { expect(x.t.men).toBeGreaterThan(0); expect(x.t.strayed).toBe(0); expect(x.t.thrown).toBe(0); expect(x.t.turned).toBe(0); }
  });
  it('off their lorries and horses, the men spread to their places: no knot of three on one point for long', () => {
    for (const x of on) expect(x.t.piled).toBe(0);
  });
  it('on foot, no man goes faster than a man can run, however fast the rules drive his block', () => {
    for (const x of on) { expect(x.t.footFast).toBe(0); expect(x.t.footMost).toBeLessThan(0.9); }
  });
  it('down, and their block goes on at lorry or horse pace: they mount again (into their lorries, into the saddle)', () => {
    expect(on.reduce((a, x) => a + x.t.reboarded, 0)).toBeGreaterThan(0);
    expect(on.reduce((a, x) => a + x.t.resaddled, 0)).toBeGreaterThan(0);
  });
  it('they climb into a halted lorry and mount a horse held still, never one going by; nobody marches on the spot', () => {
    for (const x of on) { expect(x.t.climbMoving).toBe(0); expect(x.t.stuck).toBe(0); }
  });
  it('a lorried block holding from the start is down and in its places, not aboard', () => {
    const T = ticket('hills', 538, 'methodical', 'mass');
    const b = new LandBattle(T), d = b.drillOn();
    b.step();
    const held = d.view().filter((blk) => b.unit(blk.id)!.kind === 'mot' && blk.ph === 'hold');
    expect(held.length).toBeGreaterThan(0);
    for (const blk of held) expect(blk.men.filter((m) => m.r !== 'lorry' && m.p === 'mounted')).toHaveLength(0);
  });
});

describe('the numbers (data/battle/drill.json)', () => {
  it('a mass goes in waves, close; bold rushes longer and halts shorter than methodical', () => {
    const S = DRILL.style;
    expect(S.mass.wave).toBe(true);
    expect(S.mass.spacing).toBeLessThan(S.plain.spacing);
    expect(S.bold.rush).toBeGreaterThan(S.methodical.rush);
    expect(S.bold.halt).toBeLessThan(S.methodical.halt);
    expect(S.bold.scoot).toBeLessThan(S.methodical.scoot);
  });
  it('raw men bunch and stay down longer; veterans spread and look further for cover', () => {
    expect(DRILL.xp.bunch).toBeLessThan(1);
    expect(DRILL.xp.spread).toBeGreaterThan(1);
    expect(DRILL.xp.seek[0]).toBeLessThan(DRILL.xp.seek[2]);
    expect(DRILL.xp.pinned[0]).toBeGreaterThan(DRILL.xp.pinned[2]);
  });
});

describe('every round: real odds, and a hit shows on what it struck (Brian, 21 November 04:04Z)', () => {
  // A plain practice battle, and one with tanks on both sides (each side's second corps made armour) for tank duels.
  // flee: a block made to break at 80 s, run off the field and leave it (the review of 21 November).
  const go = (g: Ground, seed: number, att: (typeof STY)[number], def: (typeof STY)[number], tanks: boolean, flee?: number) => {
    const T = sampleTicket(g, 1.3, { seed, style: { att, def } });
    if (tanks) { T.att.forces[1].kind = 'arm'; T.def.forces[1].kind = 'arm'; }
    const b = new LandBattle(T), d = b.drillOn();
    const t = {
      rounds: 0, misses: 0, kills: 0, fell: 0, killOk: 0, byOk: 0, hitOnMan: 0, missFar: 0, unmarked: 0, tankHits: 0, tankShots: 0, order: 0, smallOnTankWorn: 0,
      aimedHits: 0, landed: 0, landedOnVehicle: 0, hitsLanded: 0, hitsOff: 0, unfallen: 0, fledAt: -1, fledKills: 0, fledFell: 0,
    };
    const roles = new Map<string, string>();
    let last = 0;
    const fell = new Set<string>(), killed = new Map<string, number>(), flying: (ReturnType<typeof d.rounds>[number])[] = [];
    for (let k = 0; !b.over && k < 30000; k++) {
      const runner = flee !== undefined ? b.unit(flee)! : undefined;
      if (runner && b.t >= 80 && runner.state !== 'gone') runner.mor = 0;
      const was = runner?.state;
      b.step();
      if (runner && was !== 'gone' && runner.state === 'gone') t.fledAt = b.t;
      for (const r of d.rounds()) {
        if (r.n <= last) continue;
        if (r.n !== last + 1) t.order++;
        last = r.n; t.rounds++;
        const sq = d.squad(r.on)!, all = [...sq.men, ...sq.veh];
        const v = r.ok !== undefined ? all.find((m) => m.k === r.ok) : undefined;
        if (r.w === 'tank') { t.tankShots++; if (v?.role === 'tank' && (r.r === 'glance' || r.r === 'worn')) t.tankHits++; }
        if ((r.r === 'glance' || r.r === 'worn') && r.fc !== 'top') t.aimedHits++;
        if (flee !== undefined && r.on === flee && r.r === 'kill' && t.fledAt >= 0) t.fledKills++;
        flying.push(r);
        if (r.r === 'miss') {
          t.misses++;
          const u = b.unit(r.on)!;
          if (Math.hypot(u.x - r.x, u.y - r.y) > Math.max(u.w, u.d) / 2 + 150) t.missFar++;
        } else if (r.r === 'kill') {
          t.kills++; killed.set(r.on + ':' + r.ok, (killed.get(r.on + ':' + r.ok) ?? 0) + 1);
          if (r.by !== undefined) t.byOk++;
        } else {
          // A round that strikes short of a kill strikes a vehicle, and leaves its scar there (small arms only rattle a tank).
          if (!v || (v.role !== 'tank' && v.role !== 'gun' && v.role !== 'lorry')) t.hitOnMan++;
          else if (!(v.role === 'tank' && (r.w === 'rifle' || r.w === 'mg')) && !(v.marks ?? []).some((mk) => mk[0] === r.fc && mk[1] === r.u && mk[2] === r.h) && (v.marks?.length ?? 0) < DRILL.rounds.marks) t.unmarked++;
          if (v?.role === 'tank' && (r.w === 'rifle' || r.w === 'mg') && r.r === 'worn') t.smallOnTankWorn++;
        }
      }
      // Each round where it lands, when it lands (at l): a miss on no vehicle of any block nor a wreck, a hit on what it
      // struck (where that will be is foreseen from how it goes: a vehicle that turns or stops in flight is a metre or
      // two off).
      for (let i = flying.length - 1; i >= 0; i--) {
        const r = flying[i];
        if (b.t < r.l) continue;
        flying.splice(i, 1);
        if (r.r === 'miss') {
          t.landed++;
          let near = Infinity;
          for (const u of b.units) for (const m of [...(d.squad(u.id)?.men ?? []), ...(d.squad(u.id)?.veh ?? [])]) {
            if (m.fell === undefined && (m.k >= 100 || m.k < u.figures) && (m.role === 'tank' || m.role === 'gun' || m.role === 'lorry')) near = Math.min(near, Math.hypot(m.x - r.x, m.y - r.y));
          }
          for (const z of b.wrecks) { const [zx, zy] = d.wreckSpot(z); near = Math.min(near, Math.hypot(zx - r.x, zy - r.y)); }
          if (near < 1.5) t.landedOnVehicle++;
        } else if (r.r !== 'kill' && r.ok !== undefined) {
          const sq = d.squad(r.on)!, v = [...sq.men, ...sq.veh].find((m) => m.k === r.ok)!;
          t.hitsLanded++;
          if (Math.hypot(v.x - r.x, v.y - r.y) > 4) t.hitsOff++;
        }
      }
      for (const id of b.units.map((u) => u.id)) {
        const sq = d.squad(id);
        for (const m of sq?.men ?? []) { roles.set(id + ':' + m.k, m.role); if (m.fell !== undefined) fell.add(id + ':' + m.k); }
      }
    }
    for (const key of fell) if (killed.get(key) === 1) t.killOk++;
    t.fell = fell.size;
    // Every model the rules took has fallen by the end, those of the last step too; the men of a block that ran off the
    // field never fall.
    for (const u of b.units) {
      const ran = u.state === 'gone' && (u.y < 0 || u.y > b.field.h);
      for (const m of d.squad(u.id)?.men ?? []) if (m.fell === undefined && m.k >= u.figures && u.state !== 'withdrawn' && !ran) t.unfallen++;
      if (u.id === flee) t.fledFell = d.squad(u.id)!.men.filter((m) => m.fell !== undefined && m.fell >= t.fledAt - 0.05).length;
    }
    return { b, d, t };
  };
  const plain = go('plains', 504, 'bold', 'methodical', false), duel = go('hills', 538, 'methodical', 'bold', true);
  it('every man and tank the rules take has his round, one, the kill, from the block that took him', () => {
    for (const x of [plain, duel]) { expect(x.t.fell).toBeGreaterThan(0); expect(x.t.killOk).toBe(x.t.fell); expect(x.t.kills).toBe(x.t.fell); expect(x.t.byOk).toBe(x.t.kills); expect(x.t.unfallen).toBe(0); }
  });
  it('the men of a block that runs off the field go with it: none falls there, and no round is fired at them for it', () => {
    const fled = go('plains', 504, 'bold', 'methodical', false, 1);
    expect(fled.t.fledAt).toBeGreaterThan(0);
    expect(fled.t.fledKills).toBe(0);
    expect(fled.t.fledFell).toBe(0);
    expect(fled.t.killOk).toBe(fled.t.fell);
    expect(fled.t.kills).toBe(fled.t.fell);
    expect(fled.t.unfallen).toBe(0);
  });
  it('each round lands where it lands when it lands: a miss on no vehicle or wreck of any block, a hit on what it struck', () => {
    for (const x of [plain, duel]) {
      expect(x.t.landed).toBeGreaterThan(500);
      expect(x.t.landedOnVehicle).toBeLessThanOrEqual(x.t.landed * 0.005);
      expect(x.t.hitsLanded).toBeGreaterThan(0);
      expect(x.t.hitsOff).toBeLessThanOrEqual(Math.max(1, x.t.hitsLanded * 0.02));
    }
  });
  it('most rounds miss, landing near what they were fired at (never on a vehicle: below, where it lands)', () => {
    for (const x of [plain, duel]) {
      expect(x.t.misses / x.t.rounds).toBeGreaterThan(0.8);
      expect(x.t.missFar).toBe(0);
    }
  });
  it('a round strikes a man only where the rules take him; one short of a kill strikes a tank, a gun or a lorry, and scars it', () => {
    for (const x of [plain, duel]) { expect(x.t.hitOnMan).toBe(0); expect(x.t.unmarked).toBe(0); expect(x.t.smallOnTankWorn).toBe(0); expect(x.t.order).toBe(0); }
  });
  it('tanks duelling hit some rounds and miss others, and keep their scars to the end (the view sends them)', () => {
    // Aimed rounds that struck short of a kill: the odds at work (not the kills, which the rules decide).
    expect(plain.t.aimedHits).toBeGreaterThan(0);
    expect(duel.t.aimedHits).toBeGreaterThan(20);
    expect(duel.t.tankHits).toBeGreaterThan(0);
    expect(duel.t.tankHits).toBeLessThan(duel.t.tankShots);
    const scarred = duel.d.view().flatMap((blk) => blk.men).filter((m) => m.r === 'tank' && m.mk?.length);
    expect(scarred.length).toBeGreaterThan(0);
    for (const m of scarred) expect(m.mk!.length).toBeLessThanOrEqual(DRILL.rounds.marks);
  });
});
