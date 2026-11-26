// Units face the fire that hurts most (release 2, the Battle rules card's `facing`, off; sim.faceRule).
//
// The design director's ruling of 25 November 2026, 21:54Z, on the fairness gate's trace of two cities
// (land-battles/teams-gate2-1125.md): as the rules stood, ANY blow on a block's front held it to its facing for the
// tick, at any range, however light and whatever the block's cover, so a far-off gun or machine gun kept a block
// facing it while a line came in on its side, where blows count half as much again. Ruled a fault, and the mend by its
// cause, for every striker: a block faces what costs it most. It is held by the enemy whose blows cost it most this
// tick after cover, and turns to meet another whose blows cost it more. No new figure.
//
// The cost is weighed as the blow would land face on, without the tick's luck (sim.ts blow, faceOn): so what an enemy
// costs is the same whichever way the block faces, and two enemies of about equal weight never turn it back and forth.
// The staged tests below put three or four blocks by hand on a real field, both staffs silent, and read the facing;
// the last ones read whole battles blow by blow.
import { describe, expect, it, vi } from 'vitest';
import { bearing, turn } from '../src/battle/core/geometry';
import { liveTicket } from '../src/battle/land/live';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import { LAND } from '../src/battle/land/rules';
import { LandBattle } from '../src/battle/land/sim';
import type { FieldTicket } from '../src/battle/core/types';
import type { LandPlace } from '../src/battle/land/field';

type Ticket = FieldTicket<LandPlace>;
type U = {
  id: number; side: 'att' | 'def'; kind: string; x: number; y: number; face: number; power0: number; str: number; str0: number;
  state: string; pinned: boolean; engaged?: number; keepFace: number | null; home: [number, number]; hid?: boolean;
};
type Blow = { dmg: number; front: boolean; aspect: 'front' | 'flank' | 'rear'; faceOn: number };

/** Normandy's field with one infantry and one gun force attacking one infantry force: thirteen blocks. */
function small(): Ticket {
  const t = ticketOf(PIECES.find((x) => x.key === 'normandy-1944')!, 11, null);
  const pick = (fs: typeof t.att.forces, kinds: string[]): typeof t.att.forces => {
    const out = kinds.map((k) => fs.find((f) => f.kind === k)!), s = out.reduce((a, f) => a + f.share, 0);
    return out.map((f) => ({ ...f, share: f.share / s }));
  };
  return { ...t, att: { ...t.att, forces: pick(t.att.forces, ['inf', 'art']) }, def: { ...t.def, forces: pick(t.def.forces, ['inf']) } };
}

const AT: [number, number] = [1500, 1000];
/** Every staged attacker strikes at this share of itself, so the block they strike stands steady while it is watched. */
const CALM = 0.3;

interface Stage { sim: LandBattle; d: U; side: U | null; front: U | null; rear: U | null; guns: U[] }

/**
 * The stage: one defending infantry block in the middle of the field facing south, the attack's three gun blocks 790 m
 * to its front, and attacking infantry blocks close on its east side, to its front and behind it, each where a weight
 * is given for it. The rest of both sides stand out of everyone's reach. Both staffs are silent: nobody moves but to turn.
 */
function stage(rule: boolean, w: { guns: number; side?: number; front?: number; rear?: number }): Stage {
  const sim = new LandBattle(small());
  sim.faceRule = rule;
  sim.ai = { att: false, def: false };
  const all = sim.units as unknown as U[];
  const foot = all.filter((u) => u.side === 'att' && u.kind === 'inf'), guns = all.filter((u) => u.side === 'att' && u.kind === 'art');
  const held = all.filter((u) => u.side === 'def');
  const put = (u: U, x: number, y: number, face: number, weight = 1): U => { u.x = x; u.y = y; u.face = face; u.home = [x, y]; u.keepFace = null; u.hid = false; u.power0 *= weight; return u; };
  held.slice(1).forEach((u, i) => put(u, 500 + 380 * i, 1650, Math.PI));
  foot.forEach((u, i) => put(u, 400 + 2200 * (i % 2), 250 + 150 * i, 0));
  const d = put(held[0], AT[0], AT[1], Math.PI);
  const side = w.side !== undefined ? put(foot[0], AT[0] + 260, AT[1], -Math.PI / 2, w.side * CALM) : null;
  const front = w.front !== undefined ? put(foot[1], AT[0], AT[1] - 160, 0, w.front * CALM) : null;
  const rear = w.rear !== undefined ? put(foot[2], AT[0], AT[1] + 160, Math.PI, w.rear * CALM) : null;
  for (const g of guns) g.power0 *= w.guns * CALM;
  return { sim, d, side, front, rear, guns };
}

/** The block's facing each tick for so many seconds, and how many of the ticks it was held. */
function watch(s: Stage, secs = 20): { faces: number[]; held: number; ticks: number } {
  const faces: number[] = [];
  let held = 0, ticks = 0;
  const t0 = s.sim.t;
  while (!s.sim.over && s.sim.t - t0 < secs - 1e-6) { s.sim.step(); faces.push(s.d.face); ticks++; if (s.d.pinned) held++; }
  return { faces, held, ticks };
}

const offOf = (d: U, e: U): number => Math.abs(turn(d.face, bearing(d.x, d.y, e.x, e.y)));
/** How many times the facing changed the way it was turning. */
function reversals(faces: number[], from: number): number {
  let last = 0, n = 0, prev = from;
  for (const f of faces) {
    const step = turn(prev, f);
    prev = f;
    if (Math.abs(step) < 1e-9) continue;
    if (last && Math.sign(step) !== last) n++;
    last = Math.sign(step);
  }
  return n;
}
const turned = (faces: number[], from: number): number => faces.filter((f, i) => Math.abs(turn(i ? faces[i - 1] : from, f)) > 1e-9).length;

describe('units face the fire that hurts most: the switch', () => {
  it('is off unless the card turns it on, for a set piece and a campaign battle alike', () => {
    const t = small();
    expect(new LandBattle(t).faceRule).toBe(false);
    expect(liveTicket(t, { me: null }).sim.faceRule).toBe(false);
    expect(liveTicket(t, { me: null, facing: true }).sim.faceRule).toBe(true);
    expect(liveTicket(ticketOf(PIECES[0], 11, null), { me: null, facing: true }, PIECES[0].key).sim.faceRule).toBe(true);
  });

  it('support teams in battle works only with it on: `teams` alone turns on nothing', () => {
    const t = small();
    const alone = liveTicket(t, { me: null, teams: true }).sim, both = liveTicket(t, { me: null, teams: true, facing: true }).sim;
    expect([alone.teamsRule, alone.faceRule]).toEqual([false, false]);
    expect([both.teamsRule, both.faceRule]).toEqual([true, true]);
    // (and `facing` alone is its own rule: no team rule rides on it)
    expect(liveTicket(t, { me: null, facing: true }).sim.teamsRule).toBe(false);
  });
});

describe('units face the fire that hurts most: a block, a far gun to its front and a line at its side', () => {
  it('off, the far guns hold it to its facing while the line strikes its side, as it always was', () => {
    const s = stage(false, { guns: 1, side: 1 }), r = watch(s);
    expect(s.d.state).toBe('steady');
    // (it is fighting the line at its side, and may not turn to it)
    expect(s.d.engaged).toBe(s.side!.id);
    expect(r.held).toBe(r.ticks);
    expect(turned(r.faces, Math.PI)).toBe(0);
    expect(offOf(s.d, s.side!)).toBeCloseTo(Math.PI / 2, 6);
  });

  it('on, it turns to meet the line, once, and is then held by the line; it loses less for it', () => {
    const s = stage(true, { guns: 1, side: 1 }), r = watch(s);
    expect(s.d.state).toBe('steady');
    // The line is in its front now (a block stops turning when what holds it is in its front, as before), and holds it.
    expect(offOf(s.d, s.side!)).toBeLessThanOrEqual(Math.PI / 3 + 1e-9);
    expect(s.d.pinned).toBe(true);
    expect(reversals(r.faces, Math.PI)).toBe(0);
    // One turn, over in a second or two: every tick after it the block is held.
    const moving = turned(r.faces, Math.PI);
    expect(moving).toBeGreaterThan(0);
    expect(moving).toBeLessThan(30);
    expect(r.held).toBe(r.ticks - moving);
    // What the mend is for: the line's blows land on its front, not its side.
    const was = stage(false, { guns: 1, side: 1 });
    watch(was);
    expect(s.d.str).toBeGreaterThan(was.d.str);
  });

  it('on, fire that truly hurts still holds it: guns that cost it more than the line keep its facing, tick for tick as off', () => {
    // (each gun block's blow, at the guns' share from far back, well over the line's)
    const heavy = { guns: 12, side: 1 };
    const on = stage(true, heavy), off = stage(false, heavy);
    const a = watch(on, 8), b = watch(off, 8);
    expect(a.held).toBe(a.ticks);
    expect(turned(a.faces, Math.PI)).toBe(0);
    // The rule draws no dice and moved nothing: the same fight to the last digit.
    expect(a.faces).toEqual(b.faces);
    expect(on.d.str).toBe(off.d.str);
    expect((on.sim.units as unknown as U[]).map((u) => [u.str, u.face, u.state])).toEqual((off.sim.units as unknown as U[]).map((u) => [u.str, u.face, u.state]));
  });
});

describe('units face the fire that hurts most: it does not turn back and forth', () => {
  // An enemy to its front and one behind it, of about equal weight: struck from behind, a blow counts double, so a rule
  // that weighed blows as they land would turn the block about, find the other enemy behind it at double, and turn again.
  it('the heavier in front: it never turns, though the blows behind it land at double', () => {
    const s = stage(true, { guns: 0.2, front: 1.02, rear: 1 }), r = watch(s);
    expect(s.d.state).toBe('steady');
    expect(turned(r.faces, Math.PI)).toBe(0);
    expect(r.held).toBe(r.ticks);
  });

  it('the heavier behind: it turns about once, and stays', () => {
    const s = stage(true, { guns: 0.2, front: 1, rear: 1.02 }), r = watch(s);
    expect(s.d.state).toBe('steady');
    expect(offOf(s.d, s.rear!)).toBeLessThanOrEqual(Math.PI / 3 + 1e-9);
    expect(reversals(r.faces, Math.PI)).toBe(0);
    const moving = turned(r.faces, Math.PI);
    expect(moving).toBeGreaterThan(0);
    expect(moving).toBeLessThan(60);
    expect(r.held).toBe(r.ticks - moving);
    // (the last ten seconds: not a turn in them)
    expect(turned(r.faces.slice(-100), r.faces[r.faces.length - 101])).toBe(0);
  });

  it('off, the one in front holds it whatever stands behind it, three times its weight too', () => {
    const s = stage(false, { guns: 0.2, front: 1, rear: 3 }), r = watch(s, 6);
    expect(turned(r.faces, Math.PI)).toBe(0);
    expect(r.held).toBe(r.ticks);
  });

  it("what an enemy costs is the same whichever way the block faces, and carries none of the tick's luck", () => {
    const s = stage(true, { guns: 1, side: 1, front: 1, rear: 1 });
    const blow = (s.sim as unknown as { blow(u: U, t: U): Blow }).blow.bind(s.sim);
    for (const e of [s.side!, s.front!, s.rear!, s.guns[0]]) {
      const costs: number[] = [], aspects = new Set<string>();
      for (const face of [Math.PI, Math.PI / 2, 0, -Math.PI / 2]) {
        s.d.face = face;
        for (let k = 0; k < 3; k++) {
          const b = blow(e, s.d), weight = LAND.aspect[b.aspect][0];
          costs.push(b.faceOn);
          aspects.add(b.aspect);
          // (the blow as it lands is that cost at its aspect's weight, within the tick's luck)
          expect(b.dmg / (b.faceOn * weight)).toBeGreaterThanOrEqual(1 - LAND.jitter - 1e-9);
          expect(b.dmg / (b.faceOn * weight)).toBeLessThanOrEqual(1 + LAND.jitter + 1e-9);
        }
      }
      expect(aspects.size).toBe(3);
      expect(Math.max(...costs)).toBeCloseTo(Math.min(...costs), 12);
      expect(costs[0]).toBeGreaterThan(0);
    }
    // Off, it is not reckoned at all.
    const was = stage(false, { guns: 1, side: 1 });
    expect((was.sim as unknown as { blow(u: U, t: U): Blow }).blow(was.side! as U, was.d).faceOn).toBe(0);
  });
});

describe('units face the fire that hurts most: whole battles, blow by blow', () => {
  /** A whole battle with every blow of every tick read: per tick, what each struck block's costliest enemy was. */
  function fought(key: string, seed: number, rule: boolean, each: (tick: Map<number, { front: boolean; any: boolean; faulty: boolean }>, units: U[]) => void): LandBattle {
    const proto = LandBattle.prototype as unknown as { blow: (u: U, t: U) => Blow };
    const real = proto.blow;
    const log: { t: U; b: Blow; struck: number }[] = [];
    const spy = vi.spyOn(proto, 'blow').mockImplementation(function (this: unknown, u: U, t: U): Blow {
      const b = real.call(this, u, t);
      log.push({ t, b, struck: b.dmg });
      return b;
    });
    try {
      const sim = new LandBattle(ticketOf(PIECES.find((x) => x.key === key)!, seed, null));
      sim.faceRule = rule;
      const units = sim.units as unknown as U[];
      let guard = 0;
      while (!sim.over && guard++ < 400_000) {
        log.length = 0;
        sim.step();
        const tick = new Map<number, { front: boolean; any: boolean; faulty: boolean; cost: number; best: number; worst: number }>();
        for (const { t, b, struck } of log) {
          // On: the rule's own cost. Off (none reckoned): the blow at its aspect's weight, luck and all, for the count below.
          const cost = rule ? (struck > 0 ? b.faceOn * (b.dmg / struck) : 0) : struck / LAND.aspect[b.aspect][0];
          const top = tick.get(t.id);
          if (!top) { tick.set(t.id, { front: b.front, any: b.front, faulty: false, cost, best: b.front ? cost : 0, worst: b.front ? 0 : cost }); continue; }
          if (cost > top.cost) { top.cost = cost; top.front = b.front; }
          if (b.front) { top.any = true; top.best = Math.max(top.best, cost); } else top.worst = Math.max(top.worst, cost);
        }
        // (held by a front blow while an enemy at its side or behind cost it over twice as much: far beyond luck)
        for (const top of tick.values()) top.faulty = top.any && top.worst > 2 * top.best;
        each(tick, units);
      }
      return sim;
    } finally { spy.mockRestore(); }
  }

  it('on, a block is held exactly when the enemy whose blow cost it most that tick is in its front', () => {
    let struck = 0, held = 0, free = 0;
    for (const [key, seed] of [['normandy-1944', 3], ['prokhorovka-1943', 7]] as [string, number][]) {
      fought(key, seed, true, (tick, units) => {
        for (const u of units) {
          const top = tick.get(u.id);
          if (!top) { expect(u.pinned).toBe(false); continue; }
          struck++;
          if (top.front) held++; else free++;
          if (u.pinned !== top.front) throw new Error(`${key} block ${u.id}: held ${u.pinned}, its costliest enemy in front ${top.front}`);
        }
      });
    }
    // (the battles did put blocks under a costliest enemy at their side or behind, or this proves nothing)
    expect(struck).toBeGreaterThan(1000);
    expect(held).toBeGreaterThan(0);
    expect(free).toBeGreaterThan(0);
  }, 240_000);

  it('off, a block is held by any blow on its front, and the fault is there to be mended; on, it never is', () => {
    const faulty = (rule: boolean): number => {
      let n = 0;
      for (const [key, seed] of [['normandy-1944', 3], ['prokhorovka-1943', 7]] as [string, number][]) {
        fought(key, seed, rule, (tick, units) => {
          for (const u of units) {
            const top = tick.get(u.id);
            if (!rule && u.pinned !== !!top?.any) throw new Error(`${key} block ${u.id}: held ${u.pinned}, struck in front ${!!top?.any}`);
            // On, the count is of blocks HELD while far more is costing them from the side: the rule's own cost reads none.
            if (top?.faulty && u.pinned) n++;
          }
        });
      }
      return n;
    };
    expect(faulty(false)).toBeGreaterThan(0);
    expect(faulty(true)).toBe(0);
  }, 240_000);

  it('is read: on and off are different battles, and each is the same battle fought twice', () => {
    const end = (rule: boolean): [number, number] => { const s = fought('normandy-1944', 3, rule, () => {}); return [s.over!.R, s.t]; };
    const on = end(true), off = end(false);
    expect(on).not.toEqual(off);
    expect(end(true)).toEqual(on);
    expect(end(false)).toEqual(off);
  }, 240_000);
});
