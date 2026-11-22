// Each rifle model drawn as its whole platoon (release 2; Brian, 22 November 2026: "I want a lot of troops in the ground
// battles"). Looks only (ruling 217): the rules' model is the platoon's leading squad's key man; its other squads and its
// headquarters form behind him, fight as men and fall one by one as the block loses strength. These tests hold that it
// changes nothing in the fight, that every platoon is its army's, that the men still move as men and keep their room,
// and that lorried platoons ride a lorry to each squad.
import { describe, expect, it } from 'vitest';
import { DRILL, LandBattle, platoonOf, sampleTicket, sectionOf, type LandUnit, type MenOut } from '../src/battle/land';
import type { FieldEvent as CoreEvent } from '../src/battle/core';

type Ground = Parameters<typeof sampleTicket>[0];
type Style = 'bold' | 'methodical' | 'mass';
const MOVING = new Set(['walk', 'run', 'advance', 'creep', 'crawl', 'flee', 'drive', 'reverse', 'towed']);
const FIRING = new Set(['fire_stand', 'fire_kneel', 'fire_prone', 'lmg_burst', 'throw_grenade', 'fire']);
const POSES = new Set([...MOVING, ...FIRING, 'idle', 'cower', 'wait', 'hide', 'observe', 'mounted', 'ride', 'horses', 'down', 'halt', 'deployed', 'parked']);
const FOOT = new Set(['officer', 'rifle', 'mg', 'at', 'sapper', 'holder', 'fo', 'leader', 'loader', 'crew']);
const PLATOONS = { sections: true, platoons: true };

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

describe('drawn as platoons, it is still looks only (ruling 217)', () => {
  it('a battle ends exactly the same with every platoon drawn as with no men drawn at all', () => {
    for (const [g, seed, att, def] of [['forest', 5, 'methodical', 'bold'], ['urban', 37, 'bold', 'mass']] as const) {
      const off = battle(g, seed, att, def), on = battle(g, seed, att, def);
      on.drillOn(PLATOONS);
      const a = fight(off), c = fight(on);
      expect(sig(on, c), g).toBe(sig(off, a));
    }
  }, 600_000);
});

describe('who is in a platoon', () => {
  it('an American platoon of 41 (three squads of 12 and five at its head), a German Zug of 37, a British platoon of 37', () => {
    const size = (nat: string) => { const P = platoonOf(nat); return P.squads * (sectionOf(nat, 'rifle').length + 1) + P.hq; };
    expect(size('USA')).toBe(41);
    expect(size('GER')).toBe(37);
    expect(size('UK')).toBe(37);
    // Every army has one, its own or the default.
    for (const nat of ['SOV', 'JAP', 'ITA', 'FRA', 'XYZ']) expect(size(nat)).toBeGreaterThanOrEqual(30);
  });
  it('a block draws each rifle model as its platoon, each man known by k and j; the rest as sections', () => {
    const b = battle('plains', 11, 'bold', 'methodical');
    const d = b.drillOn(PLATOONS);
    let rifles = 0;
    for (const blk of d.view()) {
      const u = b.unit(blk.id)!;
      expect(new Set(blk.men.map((m) => `${m.k}:${m.j ?? ''}`)).size).toBe(blk.men.length);
      if (u.kind !== 'inf') continue;
      const nat = u.side === 'att' ? 'USA' : 'GER', P = platoonOf(nat), sec = sectionOf(nat, 'rifle').length + 1;
      for (const key of blk.men.filter((m) => m.j === undefined)) {
        const mine = blk.men.filter((m) => m.k === key.k);
        if (key.r === 'leader') {
          rifles++;
          expect(mine.length).toBe(P.squads * sec + P.hq);
          // A leader to each squad and to the headquarters, each automatic weapon with its number twos.
          expect(mine.filter((m) => m.r === 'leader').length).toBe(P.squads + 1);
          expect(mine.filter((m) => m.r === 'mg').length).toBe(P.squads);
        } else expect(mine.length).toBe(sectionOf(nat, key.r as never).length + 1);
      }
    }
    expect(rifles).toBeGreaterThan(20);
    expect(d.view().reduce((a, blk) => a + blk.men.length, 0)).toBeGreaterThan(1800);
  });
});

describe('the men of a platoon move as men, step by step, and keep their room (the 3D check)', () => {
  const b = battle('hills', 23, 'bold', 'methodical', true), d = b.drillOn(PLATOONS);
  const t = { thrown: 0, turnFoot: 0, onSpot: 0, still: 0, badPose: 0, strayed: 0, orphans: 0, gap: 0, fell: 0, killed: 0, men: 0 };
  const was = new Map<string, { x: number; y: number; f: number; same: number }>(), at = new Map<number, [number, number]>();
  const ov = { samples: 0, pairs: 0, longest: 0, run: new Map<string, number>() };
  const kills = new Set<string>();
  let last = 0;
  fight(b, (k) => {
    for (const r of d.rounds()) { if (r.n <= last) continue; last = r.n; if (r.r === 'kill') kills.add(id(r.on, { k: r.ok!, j: r.oj })); }
    if (k % 5 === 4) {
      const all: { key: string; x: number; y: number }[] = [];
      for (const blk of d.view()) for (const m of [...blk.men, ...(blk.fo ?? [])] as MenOut[]) {
        if (m.d === undefined && !['tank', 'gun', 'lorry'].includes(m.r) && m.p !== 'mounted' && m.p !== 'ride') all.push({ key: id(blk.id, m) + (m.r === 'fo' ? 'fo' : ''), x: m.x, y: m.y });
      }
      t.men = Math.max(t.men, all.length);
      all.sort((p, q) => p.x - q.x);
      const now = new Map<string, number>();
      let n = 0;
      for (let i = 0; i < all.length; i++) for (let q = i + 1; q < all.length && all[q].x - all[i].x < 0.7; q++) {
        if (Math.hypot(all[i].x - all[q].x, all[i].y - all[q].y) >= 0.7) continue;
        n++;
        const key = all[i].key + ' ' + all[q].key, run = (ov.run.get(key) ?? 0) + 0.5;
        now.set(key, run); ov.longest = Math.max(ov.longest, run);
      }
      ov.run = now; ov.samples++; ov.pairs += n;
      for (const u of b.units) {
        const sq = d.squad(u.id)!;
        if (u.state !== 'withdrawn' && u.state !== 'gone') {
          const up = sq.men.filter((m) => m.fell === undefined).length, want = Math.ceil((sq.men.length * u.str) / u.str0 - 1e-9);
          t.gap = Math.max(t.gap, Math.abs(up - want));
        }
        for (const m of sq.men) if (m.fell === undefined && m.j !== undefined && u.state !== 'gone' && !sq.men.some((x) => x.j === undefined && x.k === (m.sec ?? m.k) && x.fell === undefined)) t.orphans++;
      }
    }
    for (const blk of d.view()) {
      const u = b.unit(blk.id)!, o = at.get(u.id), went = o ? Math.hypot(u.x - o[0], u.y - o[1]) : 0;
      at.set(u.id, [u.x, u.y]);
      for (const m of [...blk.men, ...(blk.fo ?? [])] as MenOut[]) {
        const key = id(blk.id, m), w = was.get(key);
        let same = 0;
        if (m.d === undefined) {
          if (!POSES.has(m.p)) t.badPose++;
          const lim = (blk.ph === 'flee' ? 70 : 45) + (m.r === 'lorry' ? 120 : 0) + 3;
          if (m.r !== 'fo' && Math.hypot(m.x - u.x, m.y - u.y) > Math.hypot(u.w / 2 + lim, u.d / 2 + lim)) t.strayed++;
        }
        if (w && m.d === undefined) {
          const dist = Math.hypot(m.x - w.x, m.y - w.y);
          let df = Math.abs(m.f - w.f) % (2 * Math.PI); if (df > Math.PI) df = 2 * Math.PI - df;
          const can = FOOT.has(m.r) ? 0.9 : 1.8;
          if (dist > can && dist > went + 0.05) t.thrown++;
          if (FOOT.has(m.r) && df > 0.8 + 0.02) t.turnFoot++;
          if (MOVING.has(m.p) && m.v > 0.5 && dist < 0.005 && df < 0.005) { same = w.same + 1; if (same === 10) t.onSpot++; }
          if (!MOVING.has(m.p) && m.p !== 'ride' && m.p !== 'mounted' && m.p !== 'horses' && m.v > 0) t.still++;
        }
        was.set(key, { x: m.x, y: m.y, f: m.f, same });
      }
    }
  });
  for (const u of b.units) for (const m of d.squad(u.id)!.men) if (m.fell !== undefined) { t.fell++; if (kills.has(id(u.id, m))) t.killed++; }
  it('some 1,800 men on the field, and as a block loses strength its platoons thin to a man, each who falls by his round', () => {
    expect(t.men).toBeGreaterThan(1500);
    expect(t.gap).toBeLessThanOrEqual(1);
    expect(t.fell).toBeGreaterThan(300);
    expect(t.killed).toBe(t.fell);
    expect(t.orphans).toBe(0);
  });
  it('no man is thrown, turns about in a step, goes on the spot or strays from his block; every pose is one the 3D game has', () => {
    expect(t.thrown).toBe(0); expect(t.turnFoot).toBe(0); expect(t.onSpot).toBe(0); expect(t.still).toBe(0);
    expect(t.badPose).toBe(0); expect(t.strayed).toBe(0);
  });
  it('no two men stand inside each other: three times the men, a pair under 0.7 m apart still only in passing', () => {
    // Here (1,745 men on their feet at most): 3.5 such pairs at a time on average, none for more than 5.5 s (a loader
    // and a rifleman of the next platoon, both down, in the battle's last seconds).
    expect(ov.samples).toBeGreaterThan(300);
    expect(ov.pairs / ov.samples).toBeLessThan(4.5);
    expect(ov.longest).toBeLessThan(7);
  });
});

describe('lorried platoons and mounted ones', () => {
  // The attack's first corps on horses, its second and the defence's second in lorries.
  const ticket = () => {
    const T = sampleTicket('plains', 1.3, { seed: 7, nat: { att: 'USA', def: 'GER' }, style: { att: 'bold', def: 'methodical' } });
    T.att.forces[0].kind = 'cav'; T.att.forces[1].kind = 'mot'; T.def.forces[1].kind = 'mot';
    return T;
  };
  it('end the same as with no men drawn; a lorry to each squad and headquarters, its men on its benches', () => {
    const off = new LandBattle(ticket()), on = new LandBattle(ticket()), d = on.drillOn(PLATOONS);
    let lorries = 0, groups = 0, worstSeat = 0, aboard = 0;
    for (const blk of d.view()) {
      const u = on.unit(blk.id)!;
      if (u.kind !== 'mot') continue;
      const nat = u.side === 'att' ? 'USA' : 'GER', P = platoonOf(nat);
      lorries += blk.men.filter((m) => m.r === 'lorry').length;
      for (const key of blk.men.filter((m) => m.j === undefined && m.r !== 'lorry')) groups += key.r === 'leader' ? P.squads + 1 : 1;
      // At the start every man is aboard, in a seat within his lorry's bed.
      const vans = blk.men.filter((m) => m.r === 'lorry');
      for (const m of blk.men) {
        if (m.r === 'lorry' || m.p !== 'mounted') continue;
        aboard++;
        worstSeat = Math.min(...vans.map((v) => Math.hypot(v.x - m.x, v.y - m.y)), worstSeat || Infinity);
      }
    }
    expect(lorries).toBe(groups);
    expect(aboard).toBeGreaterThan(150);
    expect(worstSeat).toBeLessThan(6);
    expect(sig(on, fight(on))).toBe(sig(off, fight(off)));
  }, 600_000);
});

it('the platoon table names every army the sections table does', () => {
  for (const nat of Object.keys(DRILL.sections.rifle)) expect(DRILL.sections.platoon[nat], nat).toBeDefined();
});
