// Attackers stay where they stop to fight (release 2, the Battle rules card's `stand`, off; sim.standRule).
//
// The art director's number of 26 November 2026, 00:50Z, for the facing rule's look: how often a block reverses its
// wheel within five seconds of its last reversal. Counted, it was high with every rule off, in the game as shipped: a
// block on an attack order closes "until it is well in reach, then stands and fights", and that was asked afresh every
// tick with no memory of the tick before. With its target at that edge the block stood for a tick and wheeled to what
// it fought, marched for a tick and wheeled back to its way, and rocked on the spot.
//
// The design director's ruling of 01:02Z: a fault, today's, for every block on an attack order; the mend on its own
// switch and its own gate. A block that has stood to fight its target stands while the target is within its hold (how
// far off its target a block may stand and still fight, sim.ts lanePoint: no new figure), and closes again only when
// the target is further off than that. `facing` and `teams` go on only with it on.
//
// The design director's ruling of 01:28Z put three more choices of the same kind on the same row and switch, one rule,
// "what it chose it keeps while that still holds", read only from the game's own facts: the way over a river (way),
// the place beside a friend (beside) and the jostled block (back); a fourth of that kind found at the rule's own gate
// is with them (step). Each has its own key in sim.standKeeps so the gate can count them alone and together.
//
// The staged tests put one attacking block before one defending block on a real field, both staffs silent, and draw
// the defender back by hand; the last ones count the rocking in whole battles, as the gate counts it.
import { describe, expect, it } from 'vitest';
import { bearing, dist, turn } from '../src/battle/core/geometry';
import { liveTicket } from '../src/battle/land/live';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import { reachOf } from '../src/battle/land/rules';
import { LandBattle } from '../src/battle/land/sim';
import type { FieldTicket } from '../src/battle/core/types';
import type { LandPlace } from '../src/battle/land/field';

type Ticket = FieldTicket<LandPlace>;
type U = {
  id: number; side: 'att' | 'def'; kind: 'inf'; x: number; y: number; w: number; d: number; face: number; speed: number; power0: number; str: number; state: string; act: string;
  path: [number, number][]; order: { kind: string; target?: number } | null; home: [number, number]; keepFace: number | null; hid?: boolean; stoodFor?: number;
  stand?: [number, number]; standBy?: { id: number; st: boolean }[]; heldAt?: [number, number]; stepsBack?: boolean;
};
type Keeps = { way: boolean; beside: boolean; back: boolean; step: boolean };
const ALL: Keeps = { way: true, beside: true, back: true, step: true };
type Inner = {
  send(u: U, o: { kind: 'attack' | 'move'; x: number; y: number; target?: number; face?: number }): void;
  gap(a: U, b: U): number;
  claimed(u: U, x: number, y: number, face: number): { id: number; x: number; y: number } | null;
};

/** Normandy's field with one infantry force a side. */
function small(): Ticket {
  const t = ticketOf(PIECES.find((x) => x.key === 'normandy-1944')!, 11, null);
  const foot = (fs: typeof t.att.forces): typeof t.att.forces => [{ ...fs.find((f) => f.kind === 'inf')!, share: 1 }];
  return { ...t, att: { ...t.att, forces: foot(t.att.forces) }, def: { ...t.def, forces: foot(t.def.forces) } };
}

const AT: [number, number] = [1500, 1000];
/** Both staged blocks strike at this share of themselves, so neither wavers while it is watched. */
const CALM = 0.2;

interface Stage { sim: LandBattle; a: U; d: U; want: number; hold: number; gap: () => number }

/**
 * The stage: one defending infantry block in the middle of the field and one attacking infantry block 400 m before it,
 * sent to attack it. The rest of both sides stand out of everyone's reach, and both staffs are silent.
 */
const put = (u: U, x: number, y: number, face: number, weight = 1): U => { u.x = x; u.y = y; u.face = face; u.home = [x, y]; u.keepFace = null; u.hid = false; u.power0 *= weight; return u; };

/** A field with both staffs silent and every block set down out of everyone's reach: the attack's in the north, the defence's in the south. */
function quiet(rule: boolean, keeps: Keeps = ALL, t: Ticket = small()): { sim: LandBattle; inner: Inner; foot: U[]; held: U[] } {
  const sim = new LandBattle(t);
  sim.standRule = rule;
  sim.standKeeps = { ...keeps };
  sim.ai = { att: false, def: false };
  const all = sim.units as unknown as U[], inner = sim as unknown as Inner;
  const foot = all.filter((u) => u.side === 'att'), held = all.filter((u) => u.side === 'def');
  held.forEach((u, i) => put(u, 500 + 380 * i, 1700, Math.PI));
  foot.forEach((u, i) => put(u, 400 + 2200 * (i % 2), 150 + 120 * i, 0));
  return { sim, inner, foot, held };
}

/** (by: the side whose block is sent to attack. The defence's block comes at it from the south, a counter-attack.) */
function stage(rule: boolean, keeps: Keeps = ALL, by: 'att' | 'def' = 'att'): Stage {
  const { sim, inner, foot, held } = quiet(rule, keeps);
  const mine = by === 'att' ? foot[0] : held[0], theirs = by === 'att' ? held[0] : foot[0], sign = by === 'att' ? 1 : -1;
  const d = put(theirs, AT[0], AT[1], by === 'att' ? Math.PI : 0, CALM), a = put(mine, AT[0], AT[1] - 400 * sign, by === 'att' ? 0 : Math.PI, CALM);
  inner.send(a, { kind: 'attack', x: d.x, y: d.y, target: d.id });
  // (where it closes to, and its hold: the same figures as sim.ts closeTo and lanePoint)
  const want = reachOf(a) * 0.6, hold = want + (reachOf(a) - want) / 2;
  return { sim, a, d, want, hold, gap: () => inner.gap(a, d) };
}

/** Steps until the attacking block stands to fight (its way is empty), and says how many ticks that took. */
function close(s: Stage, most = 4000): number {
  let n = 0;
  do { s.sim.step(); n++; } while (s.a.path.length && n < most);
  return n;
}

/** Steps so many ticks, and counts those the attacking block marched in and the times it went from standing to marching. */
function watch(s: Stage, ticks: number, each: (k: number) => void = () => {}): { marched: number; starts: number } {
  let marched = 0, starts = 0, stood = !s.a.path.length;
  for (let k = 0; k < ticks; k++) {
    each(k);
    s.sim.step();
    const stands = !s.a.path.length;
    if (!stands) marched++;
    if (stood && !stands) starts++;
    stood = stands;
  }
  return { marched, starts };
}

describe('attackers stay where they stop to fight: the switch', () => {
  it('is off unless the card turns it on, for a set piece and a campaign battle alike, and is its own rule', () => {
    const t = small();
    expect(new LandBattle(t).standRule).toBe(false);
    expect(liveTicket(t, { me: null }).sim.standRule).toBe(false);
    const on = liveTicket(t, { me: null, stand: true }).sim;
    expect([on.standRule, on.faceRule, on.teamsRule]).toEqual([true, false, false]);
    expect(liveTicket(ticketOf(PIECES[0], 11, null), { me: null, stand: true }, PIECES[0].key).sim.standRule).toBe(true);
    // (the rows that work only with it on: tests/land-facing.test.ts)
    expect(liveTicket(t, { me: null, facing: true }).sim.standRule).toBe(false);
  });
});

describe('attackers stay where they stop to fight: one block sent to attack another', () => {
  it('on or off, it closes until it is well in reach, and stands', () => {
    for (const rule of [false, true]) {
      const s = stage(rule);
      expect(s.gap()).toBeGreaterThan(s.hold);
      close(s);
      expect(s.a.path.length).toBe(0);
      expect(s.gap()).toBeLessThanOrEqual(s.want);
      expect(s.a.order?.target).toBe(s.d.id);
      expect([s.a.state, s.d.state]).toEqual(['steady', 'steady']);
      // The stand is marked only with the rule on.
      expect(s.a.stoodFor).toBe(rule ? s.d.id : undefined);
    }
  });

  it('its target a little beyond that and still within its hold: off it marches again, on it stands and fights', () => {
    const run = (rule: boolean): { s: Stage; moved: number; marched: number } => {
      const s = stage(rule);
      close(s);
      s.d.y += s.want + 20 - s.gap();
      expect(s.gap()).toBeGreaterThan(s.want);
      expect(s.gap()).toBeLessThan(s.hold);
      const y0 = s.a.y, r = watch(s, 50);
      return { s, moved: Math.abs(s.a.y - y0), marched: r.marched };
    };
    const off = run(false), on = run(true);
    expect(off.marched).toBeGreaterThan(0);
    expect(off.moved).toBeGreaterThan(5);
    expect(on.marched).toBe(0);
    expect(on.moved).toBe(0);
    expect(on.s.a.stoodFor).toBe(on.s.d.id);
    // (and it is fighting from where it stands: in reach, as the stand's own rule has it)
    expect(on.s.gap()).toBeLessThanOrEqual(reachOf(on.s.a));
  });

  it('on, its target beyond its hold: it closes again, and stands again where the stand\'s own rule has it stand', () => {
    const s = stage(true);
    close(s);
    s.d.y += s.hold + 20 - s.gap();
    const far = s.gap(), y0 = s.a.y;
    s.sim.step();
    expect(s.a.path.length).toBeGreaterThan(0);
    expect(s.a.stoodFor).toBeUndefined();
    close(s);
    // (at its stand, or within the few metres of it at which a block counts as there: sim.ts lanePoint)
    expect(s.gap()).toBeLessThan(far - 20);
    expect(s.gap()).toBeLessThan(s.hold);
    expect(Math.abs(s.a.y - y0)).toBeGreaterThan(20);
    expect(s.a.stoodFor).toBe(s.d.id);
    // And from there it stands again while its target is within its hold.
    s.d.y += s.hold - 5 - s.gap();
    expect(watch(s, 50).marched).toBe(0);
  });

  it('a target that edges back a step at a time: off the block starts and stops over and over, on it sets off once', () => {
    // The target gives a metre of ground every second for a minute: sixty metres, past the hold once.
    const run = (rule: boolean): { starts: number; gapMost: number; s: Stage } => {
      const s = stage(rule);
      close(s);
      let gapMost = 0;
      const r = watch(s, 600, (k) => { if (k % 10 === 0) s.d.y += 1; gapMost = Math.max(gapMost, s.gap()); });
      return { starts: r.starts, gapMost, s };
    };
    const off = run(false), on = run(true);
    expect(off.starts).toBeGreaterThan(20);
    expect(on.starts).toBeLessThanOrEqual(2);
    expect(on.starts).toBeGreaterThan(0);
    // It never stood further off than its hold, where it can still fight (and a step the target took that tick).
    expect(on.gapMost).toBeLessThanOrEqual(on.s.hold + 1 + 1e-9);
    expect(on.gapMost).toBeGreaterThan(on.s.hold - 1);
    expect(on.gapMost).toBeLessThanOrEqual(reachOf(on.s.a));
  });

  it('off, nothing of it is kept: no block is ever marked as having stood', () => {
    const sim = new LandBattle(ticketOf(PIECES.find((x) => x.key === 'normandy-1944')!, 3, null));
    const units = sim.units as unknown as U[];
    let guard = 0, marks = 0;
    while (!sim.over && guard++ < 400_000) { sim.step(); for (const u of units) if (u.stoodFor !== undefined) marks++; }
    expect(sim.over).not.toBeNull();
    expect(marks).toBe(0);
  }, 240_000);
});

describe('what it chose it keeps: the order, never the side', () => {
  it('a defending block sent to counter-attack stands exactly as an attacking block does', () => {
    const run = (rule: boolean): { s: Stage; marched: number } => {
      const s = stage(rule, ALL, 'def');
      expect(s.a.side).toBe('def');
      close(s);
      expect(s.gap()).toBeLessThanOrEqual(s.want);
      s.d.y -= s.want + 20 - s.gap();
      expect(s.gap()).toBeGreaterThan(s.want);
      expect(s.gap()).toBeLessThan(s.hold);
      return { s, marched: watch(s, 50).marched };
    };
    const off = run(false), on = run(true);
    expect(off.marched).toBeGreaterThan(0);
    expect(on.marched).toBe(0);
    expect(on.s.a.stoodFor).toBe(on.s.d.id);
  });
});

describe('what it chose it keeps: a move order sent again to the place it holds (back)', () => {
  const P: [number, number] = [1500, 700];
  /** One block carries out a move order to P facing north, is eased so far to its right, and is sent the same order again (or to a place so far north of it). */
  function again(rule: boolean, keeps: Keeps, eased: number, north = 0): { u: U; held: [number, number] | undefined; steps: boolean | undefined; swung: number; left: number } {
    const { sim, inner, foot } = quiet(rule, keeps);
    const u = put(foot[0], P[0], P[1] - 100, 0);
    inner.send(u, { kind: 'move', x: P[0], y: P[1], face: 0 });
    for (let k = 0; k < 2000 && u.order; k++) sim.step();
    expect(u.order).toBeNull();
    expect(dist(u.x, u.y, P[0], P[1])).toBeLessThanOrEqual(6.5);
    const held = u.heldAt ? [...u.heldAt] as [number, number] : undefined;
    u.x = P[0] + eased; u.y = P[1]; u.face = 0;
    inner.send(u, { kind: 'move', x: P[0], y: P[1] + north, face: 0 });
    const steps = u.stepsBack;
    let swung = 0;
    for (let k = 0; k < 8000 && u.order; k++) { sim.step(); swung = Math.max(swung, Math.abs(turn(0, u.face))); }
    expect(u.order).toBeNull();
    return { u, held, steps, swung, left: dist(u.x, u.y, P[0], P[1] + north) };
  }

  it('on, it steps back to it as it faces: no wheel to that way of a few metres and none back', () => {
    const r = again(true, ALL, 12);
    expect(r.held).toEqual(P);
    expect(r.steps).toBe(true);
    expect(r.swung).toBeLessThan(0.01);
    expect(r.left).toBeLessThanOrEqual(6.5);
    // (and carried out again, the place is held still: the next such order is the same order too)
    expect(r.u.heldAt).toEqual(P);
  });

  it('off, or with that keep off, it wheels to its way and back, as it always did', () => {
    for (const r of [again(false, ALL, 12), again(true, { ...ALL, back: false }, 12)]) {
      expect(r.steps).toBeUndefined();
      expect(r.swung).toBeGreaterThan(0.5);
      expect(r.left).toBeLessThanOrEqual(6.5);
    }
    // (off, nothing of it is kept)
    expect(again(false, ALL, 12).held).toBeUndefined();
  });

  it('on, sent anywhere else, or to that place from off it altogether, it marches as it always did, and holds that place no longer', () => {
    const other = again(true, ALL, 12, 60);
    expect(other.steps).toBeUndefined();
    expect(other.swung).toBeGreaterThan(0.1);
    const far = again(true, ALL, 300);
    expect(far.steps).toBeUndefined();
    expect(far.swung).toBeGreaterThan(0.5);
  });
});

describe('what it chose it keeps: eased off the stand of its attack (step)', () => {
  /** The block stands to fight with its target just inside its hold, and is then eased 20 m to its right, off its stand and just beyond its hold. */
  function eased(keeps: Keeps): { s: Stage; swung: number; lost: boolean; left: number } {
    const s = stage(true, keeps);
    close(s);
    const st = s.a.stand!;
    s.a.x = st[0]; s.a.y = st[1];
    s.d.y += s.hold - 0.3 - s.gap();
    expect(watch(s, 10).marched).toBe(0);
    s.a.x += 20;
    expect(s.gap()).toBeGreaterThan(s.hold);
    let swung = 0, lost = false;
    watch(s, 300, () => { swung = Math.max(swung, Math.abs(turn(s.a.face, bearing(s.a.x, s.a.y, s.d.x, s.d.y)))); if (s.a.stoodFor !== s.d.id) lost = true; });
    return { s, swung, lost, left: dist(s.a.x, s.a.y, st[0], st[1]) };
  }

  it('on, it steps back to its stand as it faces what it fights, and has stood for its target the while', () => {
    const r = eased(ALL);
    expect(r.swung).toBeLessThan(0.15);
    expect(r.lost).toBe(false);
    expect(r.left).toBeLessThanOrEqual(12);
    expect(r.s.a.path.length).toBe(0);
  });

  it('with that keep off, it wheels to that way of a few metres and back', () => {
    const r = eased({ ...ALL, step: false });
    expect(r.swung).toBeGreaterThan(0.5);
    expect(r.lost).toBe(true);
    expect(r.left).toBeLessThanOrEqual(12);
  });
});

describe('what it chose it keeps: the place beside a friend (beside)', () => {
  /**
   * A block marches north into a friend standing dead ahead, a hair to its right: the two slide apart, the friend to
   * the right. Then the friend lies a metre to its left, as a waver of the block's own heading has it: which way next?
   */
  function slide(keeps: Keeps): { first: number; second: number } {
    const { sim, inner, foot } = quiet(true, keeps, ticketOf(PIECES.find((x) => x.key === 'prokhorovka-1943')!, 11, null) as Ticket);
    const a = put(foot[0], 1500, 800, 0), b = put(foot[1], 1500.01, 800, 0);
    b.y = a.y + (a.d + b.d) / 2 - 5;
    inner.send(a, { kind: 'move', x: 1500, y: 1500 });
    sim.step();
    const first = b.x - a.x;
    a.x = b.x + 1;
    sim.step();
    return { first, second: b.x - a.x };
  }

  it('on, the side the two slid apart the tick before is the side they slide now; with that keep off, it flips', () => {
    const kept = slide(ALL), flips = slide({ ...ALL, beside: false });
    expect(kept.first).toBeGreaterThan(0.2);
    expect(flips.first).toBeGreaterThan(0.2);
    expect(kept.second).toBeGreaterThan(0.2);
    expect(flips.second).toBeLessThan(-0.2);
  });

  it('a friend it stood aside for holds the same ground, whether a tick finds it halted or stepping', () => {
    const at = (keeps: Keeps, speed: number, st: boolean): { id: number; x: number; y: number } | null => {
      const { inner, foot } = quiet(true, keeps);
      const u = put(foot[0], 1500, 600, 0), f = put(foot[1], 1500, 900, 0);
      f.speed = speed;
      if (st) { f.order = { kind: 'attack', target: -1 }; f.stand = [1700, 950]; }
      u.standBy = [{ id: f.id, st }];
      return inner.claimed(u, st ? 1700 : 1500, st ? 950 : 900, 0);
    };
    // Where it stands: halted, it holds its ground either way; stepping, only with the keep.
    expect(at(ALL, 0, false)?.x).toBe(1500);
    expect(at({ ...ALL, beside: false }, 0, false)?.x).toBe(1500);
    expect(at(ALL, 6, false)?.x).toBe(1500);
    expect(at({ ...ALL, beside: false }, 6, false)).toBeNull();
    // The stand its own attack has it make for: stepping (and nearer to it), it holds that either way; halted, only with the keep.
    expect(at(ALL, 0, true)?.x).toBe(1700);
    expect(at({ ...ALL, beside: false }, 0, true)).toBeNull();
  });
});

describe('attackers stay where they stop to fight: whole battles, the rocking counted', () => {
  const DEG = Math.PI / 180;
  /**
   * A whole battle, and the count the gate prints (the design director's standing rule of 26 November 01:02Z): a
   * reversal is a block turning back after a swing of two degrees or more, while it is steady or wavering; counted are
   * the reversals within five seconds of the block's last one, and of those the ones within one second (a rock).
   */
  function fought(key: string, seed: number, rule: boolean, keeps: Keeps = ALL, river = false): { R: number; t: number; in5: number; rocks: number; blocks: number } {
    let t = ticketOf(PIECES.find((x) => x.key === key)!, seed, null) as Ticket;
    // (the gate's river line: the same forces without their own battalions, a river across the field, at odds of 1.2)
    if (river) {
      const side = (s: Ticket['att']): Ticket['att'] => ({ ...s, forces: s.forces.filter((f) => f.kind !== 'mg' && f.kind !== 'mor').map((f) => ({ ...f, share: f.share / 0.92 })) });
      t = { ...t, att: side(t.att), def: side(t.def), place: { ...t.place, name: 'A river line', river: 'Vire' }, reckoning: { ...t.reckoning, odds: 1.2, D: t.reckoning.A / 1.2 } };
    }
    const sim = new LandBattle(t);
    sim.standRule = rule;
    sim.standKeeps = { ...keeps };
    sim.ai.att = true; sim.ai.def = true;
    const us = sim.units as unknown as U[];
    const face = us.map((u) => u.face), sum = us.map(() => 0), dir = us.map(() => 0), ext = us.map(() => 0), extT = us.map(() => 0), last = us.map(() => -1);
    let in5 = 0, rocks = 0, guard = 0;
    while (!sim.over && guard++ < 400_000) {
      sim.step();
      for (let i = 0; i < us.length; i++) {
        const u = us[i], step = turn(face[i], u.face);
        face[i] = u.face;
        if (u.state !== 'steady' && u.state !== 'wavering') continue;
        sum[i] += step;
        const a = sum[i];
        if (dir[i] === 0) { if (Math.abs(a - ext[i]) >= 2 * DEG) { dir[i] = Math.sign(a - ext[i]); ext[i] = a; extT[i] = sim.t; } }
        else if ((a - ext[i]) * dir[i] >= -1e-9) { ext[i] = a; extT[i] = sim.t; }
        else if ((ext[i] - a) * dir[i] >= 2 * DEG) {
          // (it has turned back two degrees from the end of its swing: a reversal, at the time the swing ended)
          if (last[i] >= 0 && extT[i] - last[i] < 5) { in5++; if (extT[i] - last[i] < 1) rocks++; }
          last[i] = extT[i]; dir[i] = -dir[i]; ext[i] = a; extT[i] = sim.t;
        }
      }
    }
    return { R: sim.over!.R, t: sim.t, in5, rocks, blocks: us.length };
  }

  it('off, blocks rock, as the game shipped; on, a small part of it is left', () => {
    let off = 0, on = 0, offRocks = 0, onRocks = 0, blocks = 0;
    for (const [key, seed] of [['normandy-1944', 3], ['normandy-1944', 11], ['prokhorovka-1943', 7]] as [string, number][]) {
      const a = fought(key, seed, false), b = fought(key, seed, true);
      off += a.in5; on += b.in5; offRocks += a.rocks; onRocks += b.rocks; blocks += a.blocks;
    }
    // (the fault is there to be mended: more than a reversal within five seconds for every block on the field)
    expect(off).toBeGreaterThan(blocks);
    expect(on).toBeLessThan(off / 4);
    expect(onRocks).toBeLessThan(offRocks / 4);
  }, 480_000);

  it('is read: on and off are different battles, and each is the same battle fought twice', () => {
    const end = (rule: boolean, keeps: Keeps = ALL): [number, number, number] => { const r = fought('normandy-1944', 3, rule, keeps); return [r.R, r.t, r.in5]; };
    const on = end(true), off = end(false);
    expect(on).not.toEqual(off);
    expect(end(true)).toEqual(on);
    expect(end(false)).toEqual(off);
    // Off, none of its keeps is read: the same battle whichever way their keys stand.
    expect(end(false, { way: false, beside: false, back: false, step: false })).toEqual(off);
  }, 480_000);

  it('the way over a river: a tank block shoved at the bridge\'s end turned to and fro between two turning points; kept, it does not', () => {
    // (the gate's river line, seed 5: one tank block rocked for 37 s at the bridge)
    const without = fought('normandy-1944', 5, true, { ...ALL, way: false }, true), kept = fought('normandy-1944', 5, true, ALL, true);
    expect(without.in5).toBeGreaterThan(60);
    expect(kept.in5).toBeLessThan(without.in5 / 4);
  }, 480_000);
});
