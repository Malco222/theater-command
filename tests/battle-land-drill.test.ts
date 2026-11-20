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
  it('a tank stands where her wreck would lie, and burns where she stood', () => {
    expect(wreckOffset(5, 0, 130, 60)).toEqual(wreckOffset(5, 0, 130, 60));
    let checked = 0;
    // Tanks seldom lose one on the practice grounds: three battles where one does.
    for (const [g, odds, seed] of [['forest', 1.2, 1], ['forest', 1.2, 7], ['forest', 1.5, 7]] as const) {
      const { b } = run(g, odds, seed, 'bold', 'methodical', true);
      for (const u of b.units.filter((x) => x.kind === 'arm')) {
        const sq = b.drill!.squad(u.id)!, wrecks = b.wrecks.filter((z) => z.unit === u.id);
        const dead = sq.men.filter((m) => m.fell !== undefined).sort((p, q) => p.fell! - q.fell! || p.k - q.k);
        expect(dead.length).toBe(wrecks.length);
        dead.forEach((m, i) => { expect(Math.hypot(m.x - wrecks[i].x, m.y - wrecks[i].y)).toBeLessThan(0.01); checked++; });
      }
    }
    expect(checked).toBeGreaterThan(0);
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
    const t = { men: 0, idleInFight: 0, strayed: 0, badPose: 0, covered: 0, fighting: 0, leapfrog: 0, hiddenAt: 0, foUp: 0, hullDown: 0, assaultForward: 0 };
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
          if (Math.abs(a) > u.w / 2 + lim || Math.abs(l) > u.d / 2 + lim) t.strayed++;
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
  it('no man strays from his block: the block is the truth', () => { for (const x of [A, B, C]) expect(x.t.strayed).toBe(0); });
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
