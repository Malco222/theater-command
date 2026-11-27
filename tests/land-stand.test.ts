// Units keep their stand (release 2, the Battle rules card's `stand`, off; sim.standRule).
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
// The staged tests put one attacking block before one defending block on a real field, both staffs silent, and draw
// the defender back by hand; the last ones count the rocking in whole battles, as the gate counts it.
import { describe, expect, it } from 'vitest';
import { turn } from '../src/battle/core/geometry';
import { liveTicket } from '../src/battle/land/live';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import { reachOf } from '../src/battle/land/rules';
import { LandBattle } from '../src/battle/land/sim';
import type { FieldTicket } from '../src/battle/core/types';
import type { LandPlace } from '../src/battle/land/field';

type Ticket = FieldTicket<LandPlace>;
type U = {
  id: number; side: 'att' | 'def'; kind: 'inf'; x: number; y: number; face: number; power0: number; str: number; state: string; act: string;
  path: [number, number][]; order: { kind: string; target?: number } | null; home: [number, number]; keepFace: number | null; hid?: boolean; stoodFor?: number;
};
type Inner = { send(u: U, o: { kind: 'attack'; x: number; y: number; target: number }): void; gap(a: U, b: U): number };

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
function stage(rule: boolean): Stage {
  const sim = new LandBattle(small());
  sim.standRule = rule;
  sim.ai = { att: false, def: false };
  const all = sim.units as unknown as U[], inner = sim as unknown as Inner;
  const foot = all.filter((u) => u.side === 'att'), held = all.filter((u) => u.side === 'def');
  const put = (u: U, x: number, y: number, face: number, weight = 1): U => { u.x = x; u.y = y; u.face = face; u.home = [x, y]; u.keepFace = null; u.hid = false; u.power0 *= weight; return u; };
  held.slice(1).forEach((u, i) => put(u, 500 + 380 * i, 1700, Math.PI));
  foot.slice(1).forEach((u, i) => put(u, 400 + 2200 * (i % 2), 150 + 120 * i, 0));
  const d = put(held[0], AT[0], AT[1], Math.PI, CALM), a = put(foot[0], AT[0], AT[1] - 400, 0, CALM);
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

describe('units keep their stand: the switch', () => {
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

describe('units keep their stand: one block sent to attack another', () => {
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

describe('units keep their stand: whole battles, the rocking counted', () => {
  const DEG = Math.PI / 180;
  /**
   * A whole battle, and the count the gate prints (the design director's standing rule of 26 November 01:02Z): a
   * reversal is a block turning back after a swing of two degrees or more, while it is steady or wavering; counted are
   * the reversals within five seconds of the block's last one, and of those the ones within one second (a rock).
   */
  function fought(key: string, seed: number, rule: boolean): { R: number; t: number; in5: number; rocks: number; blocks: number } {
    const sim = new LandBattle(ticketOf(PIECES.find((x) => x.key === key)!, seed, null));
    sim.standRule = rule;
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
    const end = (rule: boolean): [number, number, number] => { const r = fought('normandy-1944', 3, rule); return [r.R, r.t, r.in5]; };
    const on = end(true), off = end(false);
    expect(on).not.toEqual(off);
    expect(end(true)).toEqual(on);
    expect(end(false)).toEqual(off);
  }, 480_000);
});
