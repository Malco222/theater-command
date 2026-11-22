// Neighbours join the fight, part (b): the field draws the joiners the card priced (the design director's ruling of
// 22 November 03:15Z; the design form: "the field draws it arriving at (1 - share) of the battle"; the production
// lead's 08:03Z). The card prices a joiner; the field only draws it arriving (b415, b418): it reaches its side's line
// at (1 - joined) of the battle's own clock, never a draw, so it is held off the field until its own march from the edge
// facing the province it comes from would bring it there then, and walks on from that edge as a b407 reserve sent to
// where its side's line is struck hardest. A ticket with no joiners is today's battle to the last figure.
import { describe, expect, it } from 'vitest';
import type { FieldEvent, FieldTicket, Side } from '../src/battle/core';
import { DT } from '../src/battle/core';
import { calFor } from '../src/battle/land/shells';
import { joinEdge, LAND, LandBattle, PRACTICE_GROUNDS, practiceTicket, sampleTicket, streamFor, wetUnder, withJoiners, withTeams, type LandPlace, type LandUnit } from '../src/battle/land';
import { hashString } from '../src/core/rng';

type T = FieldTicket<LandPlace>;

/** Everything a battle shows and does, every tick: blocks, orders, ground, words, views and both scales. */
function signed(b: LandBattle, before?: (b: LandBattle, k: number) => void): string {
  const ev: FieldEvent[] = [];
  const keys = new Set<string>();
  for (let k = 0; !b.over && k < 30000; k++) {
    before?.(b, k);
    b.step();
    const v = b.view();
    for (const x of Object.keys(v)) keys.add(x);
    ev.push(...v.events);
  }
  return JSON.stringify({
    over: b.over, t: b.t, keys: [...keys].sort(), scale: b.scale(), page: b.scale(undefined, LAND.scale.page),
    units: b.units.map((u) => [u.id, u.corps, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged, u.reserve ?? null, u.power0,
      u.home, u.dry ?? null, u.order, u.path, u.cal ?? null, u.crew ?? null, u.taken ?? null]),
    wrecks: b.wrecks.map((z) => [z.x, z.y, z.at]),
    said: ev.map((e) => [e.t, e.what, e.unit ?? null, e.text]),
    log: b.log.map((e) => [e.t, e.what, e.unit ?? null, e.text]),
  });
}
const digest = (s: string): string => hashString(s).toString(16).padStart(8, '0') + hashString('#' + s).toString(16).padStart(8, '0');

/** The ticket with one corps marked as coming on over dry ground (ForceTicket.cross false). */
const dryCorps = (t: T, side: 'att' | 'def', i: number): T => ({ ...t, [side]: { ...t[side], forces: t[side].forces.map((f, k) => (k === i ? { ...f, cross: false } : f)) } });
/** Warsaw's own river through the streets, as an attack from the south meets it (b526). */
const warsaw = (t: T): T => ({ ...t, place: { ...t.place, town: 'Warsaw', townSize: 'city', stream: streamFor('Warsaw', 0) } });

/** Every switch on, the defence the player's: hidden blocks, fog, the measured staff, per-unit orders by radio, a held reserve taken in hand. */
const everySwitch = (b: LandBattle): void => { b.ticket.def.human = true; b.ai.def = true; b.measured = b.hide = b.perUnit = b.radio = b.fog = true; };
const takeReserve = (b: LandBattle, k: number): void => {
  if (k !== 600) return;
  const u = b.units.find((x) => x.side === 'def' && x.reserve === 'held');
  if (u) b.command({ do: 'move', units: [u.id], x: u.x, y: u.y - 120, face: Math.PI }, 'def');
};

/** The off-identical cases: each ticket, how it is set up, and what is done to it as it is fought. */
const CASES: [string, () => T, ((b: LandBattle) => void)?, ((b: LandBattle, k: number) => void)?][] = [
  ['plains 1.3 seed 7', () => sampleTicket('plains', 1.3, { seed: 7 })],
  ['teams forest 1.0 seed 3', () => withTeams(sampleTicket('forest', 1.0, { seed: 3, nat: { att: 'GER', def: 'POL' } }))],
  ['bold hills river 1.2 seed 5', () => sampleTicket('hills', 1.2, { river: true, seed: 5, style: { att: 'bold', def: 'bold' } })],
  ['air practice 9 seed 2', () => practiceTicket(PRACTICE_GROUNDS[9], { seed: 2 })],
  ['city 1.1 seed 4', () => sampleTicket('urban', 1.1, { seed: 4, town: 'city' })],
  ['bend: an attack corps over dry ground, seed 6', () => dryCorps(sampleTicket('plains', 1.2, { river: true, seed: 6 }), 'att', 0)],
  ['river: a defence corps marked dry, seed 8', () => dryCorps(sampleTicket('plains', 1.1, { river: true, seed: 8 }), 'def', 0)],
  ['Warsaw streets, a defence corps marked dry, seed 10', () => warsaw(dryCorps(sampleTicket('urban', 1.0, { seed: 10, town: 'city' }), 'def', 1))],
  ['every switch, plains 1.2 seed 9', () => sampleTicket('plains', 1.2, { seed: 9 }), everySwitch, takeReserve],
];

/** Recorded on f242b7c9, before part (b), with signed() above: [R, seconds, digest]. PRINT=1 prints them afresh. */
const BEFORE: Record<string, [number, number, string]> = {
  'plains 1.3 seed 7': [1.119, 251, 'e502a09d189973f2'],
  'teams forest 1.0 seed 3': [0.999, 291, '659809f64b59c66b'],
  'bold hills river 1.2 seed 5': [1.074, 278, 'b577dd90a4f54a51'],
  'air practice 9 seed 2': [1.609, 249, 'bfd0aa192040a8bc'],
  'city 1.1 seed 4': [1.32, 314, '315f7594c4b3c28d'],
  'bend: an attack corps over dry ground, seed 6': [1.073, 236, '23ce2c16eb3404cf'],
  'river: a defence corps marked dry, seed 8': [0.983, 227, 'e470e039ab3ad486'],
  'Warsaw streets, a defence corps marked dry, seed 10': [1.19, 333, 'b822bc5ca80f91d7'],
  'every switch, plains 1.2 seed 9': [1.122, 279, 'c3330a6689bc270b'],
};

const PRINT = !!(globalThis as { process?: { env: Record<string, string | undefined> } }).process?.env.PRINT;
const fought = (name: string, also?: (b: LandBattle) => void): [number, number, string] => {
  const [, make, setup, during] = CASES.find((c) => c[0] === name)!;
  const b = new LandBattle(make());
  b.ai.att = b.ai.def = true;
  setup?.(b);
  also?.(b);
  const s = signed(b, during);
  return [Math.round(b.over!.R * 1000) / 1000, b.over!.seconds, digest(s)];
};

describe('(1) switched off: the same battle to the last figure', () => {
  for (const [name] of CASES) {
    it(name, () => {
      const got = fought(name);
      if (PRINT) { console.log(`    ${JSON.stringify(name)}: ${JSON.stringify(got)},`); return; }
      expect(got).toEqual(BEFORE[name]);
    }, 300_000);
  }
});

// Hold until relieved and the rider ahead (release 2; the design director, 13:43Z: "each switch on alone with no order
// and no joiner identical"): with nobody coming up from next door, either switched on is the same battle.
describe('(1b) hold until relieved, and the rider ahead, each switched on alone with no joiner: the same battle', () => {
  for (const [name] of CASES) {
    it(`${name}: relief on`, () => { expect(fought(name, (b) => { b.relief = true; })).toEqual(BEFORE[name]); }, 300_000);
    it(`${name}: rider on`, () => { expect(fought(name, (b) => { b.rider = true; })).toEqual(BEFORE[name]); }, 300_000);
  }
});

// ---- (2) switched on ------------------------------------------------------------------------------------------------

const word = (s: number): 'early' | 'midway' | 'late' => (s > 2 / 3 ? 'early' : s > 1 / 3 ? 'midway' : 'late');
const fight = (t: T): LandBattle => { const b = new LandBattle(t); b.ai.att = b.ai.def = true; return b; };
const ofCorps = (b: LandBattle, corps: number): LandUnit[] => b.allBlocks().filter((u) => u.corps === corps);
/** Runs to the step in which the block first stands on the field: the clock before that step (null if it never came). */
function firstOn(b: LandBattle, id: number, each?: (b: LandBattle) => void): number | null {
  for (let k = 0; !b.over && k < 30000; k++) {
    const t0 = b.t;
    b.step();
    if (b.unit(id)) return t0;
    each?.(b);
  }
  return null;
}
const EDGE_AT = (b: LandBattle, u: LandUnit, e: string): number => (e === 'south' ? u.y : e === 'north' ? b.field.h - u.y : e === 'west' ? u.x : b.field.w - u.x);

describe('(2) switched off by an empty list: joiners [] is the battle with none', () => {
  it('plains 1.3 seed 7, both sides joiners []', () => {
    const t = sampleTicket('plains', 1.3, { seed: 7 });
    const b = new LandBattle({ ...t, att: { ...t.att, joiners: [] }, def: { ...t.def, joiners: [] } });
    b.ai.att = b.ai.def = true;
    const s = signed(b);
    expect([Math.round(b.over!.R * 1000) / 1000, b.over!.seconds, digest(s)]).toEqual(BEFORE['plains 1.3 seed 7']);
  }, 300_000);
});

/**
 * When a corps is due on the field: it reaches its side's line at (1 - joined) of the day, so it comes on earlier by its
 * own march from its edge (the middle of its blocks' places there) to the middle of its side as the battle opens.
 */
function dueOf(b: LandBattle, corps: number, joined: number): number {
  const cs = (b.view().coming ?? []).filter((c) => c.corps === corps), u = ofCorps(b, corps)[0];
  const x = cs.reduce((a, c) => a + c.x, 0) / cs.length, y = cs.reduce((a, c) => a + c.y, 0) / cs.length;
  const own = b.units.filter((v) => v.side === u.side);
  const cx = own.reduce((a, v) => a + v.x, 0) / own.length, cy = own.reduce((a, v) => a + v.y, 0) / own.length;
  return Math.max(0, (1 - joined) * b.ticket.limit - Math.hypot(x - cx, y - cy) / LAND.speed[u.kind]);
}

describe('(3) a joiner reaches its line at (1 - joined) of the battle\'s own clock, never a draw', () => {
  for (const joined of [0.9, 0.5, 0.2]) {
    it(`joined ${joined}: on the field in the step its march from the edge would bring it to its line at ${((1 - joined) * LAND.limit).toFixed(0)} s`, () => {
      for (const seed of [3, 7, 11]) {
        const b = fight(withJoiners(sampleTicket('plains', 1.0, { seed }), { att: [{ joined }] }));
        const id = ofCorps(b, 2001)[0].id, due = dueOf(b, 2001, joined);
        expect(b.unit(id)).toBeUndefined();
        // Its time is set from the start: the view of its own side shows it, every block of the corps the same.
        expect(new Set(b.view().coming!.filter((c) => c.corps === 2001).map((c) => c.at)).size).toBe(1);
        expect(b.view().coming!.find((c) => c.id === id)!.at).toBeCloseTo(due, 2);
        // Never a draw: the same battle built again has it due at the same second.
        expect(fight(withJoiners(sampleTicket('plains', 1.0, { seed }), { att: [{ joined }] })).view().coming!.find((c) => c.id === id)!.at).toBe(b.view().coming!.find((c) => c.id === id)!.at);
        const t0 = firstOn(b, id);
        if (t0 === null) { expect(b.over).not.toBeNull(); continue; } // the day ended first: test (6)'s case
        expect(t0).toBeGreaterThanOrEqual(due - 0.01);
        expect(t0).toBeLessThan(due + DT + 0.01);
      }
    }, 120_000);
  }
  it('a city\'s longer day: the joiner\'s time scales with it', () => {
    const t = sampleTicket('urban', 1.0, { seed: 4, town: 'city' });
    const b = fight(withJoiners(t, { def: [{ joined: 0.5 }] }));
    expect(b.ticket.limit).toBeGreaterThan(LAND.limit);
    const c = (b.view().coming ?? []).find((x) => x.corps === 2101);
    expect(c?.at).toBeCloseTo(dueOf(b, 2101, 0.5), 2);
    expect(c!.at).toBeGreaterThan(0.5 * LAND.limit - 1500 / LAND.speed.inf);
  });
});

describe('(4) it comes on by the edge facing the province it comes from, never the enemy\'s back edge', () => {
  const Q = [0, Math.PI / 2, Math.PI, 1.5 * Math.PI];
  it('the edges, worked out', () => {
    expect(Q.map((q) => joinEdge('att', q))).toEqual(['west', 'east', 'south', 'west']);
    expect(Q.map((q) => joinEdge('def', q))).toEqual(['north', 'east', 'west', 'west']);
    expect([joinEdge('att', undefined), joinEdge('def', undefined)]).toEqual(['south', 'north']);
    // A dry corps of the attack on a bend field comes from the dry flank or by the south edge.
    expect([joinEdge('att', Math.PI / 2, 'west'), joinEdge('att', 1.5 * Math.PI, 'west'), joinEdge('att', Math.PI / 2, 'east')]).toEqual(['south', 'west', 'east']);
  });
  for (const side of ['att', 'def'] as Side[]) {
    for (const q of Q) {
      it(`${side === 'att' ? 'attack' : 'defence'}, toward ${(q * 180 / Math.PI).toFixed(0)} degrees: on its edge, dry, facing in, a block a division`, () => {
        const b = fight(withJoiners(sampleTicket('plains', 1.0, { river: true, seed: 5 }), { [side]: [{ joined: 0.9, toward: q, divs: 3 }] }));
        const corps = side === 'att' ? 2001 : 2101, us = ofCorps(b, corps), e = joinEdge(side, q);
        expect(us.length).toBe(3);
        expect(new Set(us.map((u) => u.name)).size).toBe(3);
        expect(b.view().coming?.filter((c) => c.corps === corps).every((c) => c.edge === e) ?? side === 'def').toBe(true);
        firstOn(b, us[0].id);
        for (const u of us) {
          expect(b.unit(u.id)).toBe(u);
          expect(EDGE_AT(b, u, e)).toBeLessThanOrEqual(Math.max(u.w, u.d) / 2 + 60 + LAND.join.inward * b.field.cell);
          expect(wetUnder(b.field, u)).toBe(false);
          // Set facing in, then turning toward its march within the same step: within a few degrees of in.
          const inn = e === 'south' ? 0 : e === 'north' ? Math.PI : e === 'west' ? Math.PI / 2 : 1.5 * Math.PI;
          expect(Math.abs(Math.atan2(Math.sin(u.face - inn), Math.cos(u.face - inn)))).toBeLessThan(0.3);
        }
      }, 60_000);
    }
  }
  it('a bend field: a dry joiner of the attack from the wet flank comes on by the south edge, in the dry lanes', () => {
    const t = withJoiners(dryCorps(sampleTicket('plains', 1.2, { river: true, seed: 6 }), 'att', 0), { att: [{ joined: 0.9, toward: Math.PI / 2, cross: false }] });
    const b = fight(t);
    const B = b.field.bend!;
    expect(B).toBeDefined();
    const wet = B.dry === 'west' ? Math.PI / 2 : 1.5 * Math.PI;
    const b2 = fight(withJoiners(dryCorps(sampleTicket('plains', 1.2, { river: true, seed: 6 }), 'att', 0), { att: [{ joined: 0.9, toward: wet, cross: false }] }));
    const us = ofCorps(b2, 2001);
    expect(us.every((u) => u.dry === true)).toBe(true);
    expect(b2.view().coming?.every((c) => c.edge === 'south')).toBe(true);
    firstOn(b2, us[0].id);
    for (const u of us) {
      expect(u.y).toBeLessThan(Math.max(u.w, u.d) / 2 + 60 + LAND.join.inward * b2.field.cell);
      expect(B.dry === 'west' ? u.x < B.x : u.x > B.x).toBe(true);
    }
  }, 60_000);
});

describe('(4b) each block keeps to its own side of a bend, and comes on where the view said', () => {
  /** Where every joiner block stands as it comes on, and the dry side of the bend. */
  const landed = (t: T): { b: LandBattle; us: LandUnit[]; said: Map<number, [number, number]> } => {
    const b = fight(t), said = new Map((b.view().coming ?? []).map((c) => [c.id, [c.x, c.y] as [number, number]]));
    const us = b.allBlocks().filter((u) => u.joined !== undefined);
    for (const u of us) if (!b.unit(u.id)) firstOn(b, u.id);
    return { b, us, said };
  };
  it('a big dry corps on a narrow dry lane stays on the dry side, every block', () => {
    for (const q of [Math.PI / 2, Math.PI, undefined]) {
      const { b, us } = landed(withJoiners(dryCorps(sampleTicket('plains', 1.2, { river: true, seed: 1 }), 'att', 2), { att: [{ joined: 0.95, toward: q, cross: false, divs: 3 }] }));
      const B = b.field.bend!;
      expect(us.length).toBe(3);
      for (const u of us) {
        expect(b.unit(u.id)).toBe(u);
        expect(B.dry === 'west' ? u.x < B.x : u.x > B.x).toBe(true);
        expect(wetUnder(b.field, u)).toBe(false);
      }
    }
  }, 60_000);
  it('a crossing corps of the attack on a bend field comes on on the wet side, never in the dry lanes', () => {
    for (const q of [undefined, 0, Math.PI / 2, Math.PI, 1.5 * Math.PI]) {
      const { b, us } = landed(withJoiners(dryCorps(sampleTicket('plains', 1.2, { river: true, seed: 1 }), 'att', 0), { att: [{ joined: 0.95, toward: q, divs: 2 }] }));
      const B = b.field.bend!;
      for (const u of us) expect(B.dry === 'west' ? u.x > B.x : u.x < B.x).toBe(true);
    }
  }, 60_000);
  it('each block\'s own place in the view, side by side, and it comes on there on clear ground', () => {
    const { b, us, said } = landed(withJoiners(sampleTicket('plains', 1.0, { seed: 3 }), { att: [{ joined: 0.9, divs: 3 }] }));
    const xs = us.map((u) => said.get(u.id)![0]);
    expect(new Set(xs).size).toBe(3);
    for (const u of us) {
      const [x, y] = said.get(u.id)!;
      // It walks on from there and turns in the step it comes: within a step's march of its place.
      expect(Math.hypot(u.x - x, u.y - y)).toBeLessThan(b.field.cell);
    }
  }, 60_000);
  it('a way it comes from that is not a number reads as none: its own back edge, never a block at NaN', () => {
    const { b, us } = landed(withJoiners(sampleTicket('plains', 1.0, { seed: 3 }), { att: [{ joined: 0.9, toward: Number.NaN }] }));
    expect(b.view().coming ?? []).toEqual([]);
    for (const u of us) expect(Number.isFinite(u.x) && Number.isFinite(u.y)).toBe(true);
  }, 60_000);
  it('battalions added by withTeams leave the card\'s A and D as they were, joiners and all', () => {
    const t = withTeams(withJoiners(sampleTicket('plains', 1.2, { seed: 3 }), { att: [{ joined: 0.5, part: 0.2 }] }));
    const b = new LandBattle(t), k = t.reckoning;
    expect(b.standing().att).toBeCloseTo(k.A / (k.A + k.D), 9);
  });
});

describe('(5) nothing on the field sees it, strikes it or is sent at it before it comes', () => {
  it('until the first joiner is due, the battle is the same whatever is coming, and after it', () => {
    const base = sampleTicket('plains', 1.2, { seed: 9 });
    // By its own back edge, near its side (a flank's long march would bring it on at the start).
    const A = fight(withJoiners(base, { att: [{ joined: 0.5 }] }));
    const B = fight(withJoiners(base, { att: [{ joined: 0.3, toward: Math.PI, kind: 'arm' }] }));
    const due = Math.min(...[A, B].flatMap((b) => (b.view().coming ?? []).map((c) => c.at)));
    expect(due).toBeGreaterThan(30);
    const strip = (b: LandBattle): string => { const v = b.view(); return JSON.stringify({ u: v.units.map((u) => [u.id, u.x, u.y, u.str, u.mor, u.state, u.engaged ?? null]), bal: v.balance, br: v.breaking, by: v.breakingBy, ev: v.events }); };
    for (let k = 0; !A.over && A.t < due - 1e-6; k++) {
      A.step(); B.step();
      expect(strip(A)).toBe(strip(B));
    }
  }, 120_000);
  it('before it comes it is not in the battle; after, it is the last ids and its side can order it', () => {
    const t = withJoiners(sampleTicket('plains', 1.1, { seed: 3, humanAtt: true }), { att: [{ joined: 0.6 }] });
    const b = new LandBattle(t);
    b.ai.def = true;
    b.ai.att = true;
    const us = ofCorps(b, 2001), ids = new Set(us.map((u) => u.id));
    const army = b.units.map((u) => u.id);
    expect(Math.min(...ids)).toBeGreaterThan(Math.max(...army));
    expect(b.command({ do: 'move', units: [...ids], x: 500, y: 500 }, 'att')).toBe(false);
    const due = b.view().coming!.find((c) => ids.has(c.id))!.at;
    expect(due).toBeGreaterThan(30);
    while (!b.over && b.t < due - 1e-6) {
      b.step();
      for (const u of b.units) {
        expect(ids.has(u.id)).toBe(false);
        expect(u.engaged === undefined || !ids.has(u.engaged)).toBe(true);
        const tg = (u.order as { target?: number } | null)?.target;
        expect(tg === undefined || !ids.has(tg)).toBe(true);
      }
      expect(b.log.every((e) => e.unit === undefined || !ids.has(e.unit))).toBe(true);
    }
    if (b.over) return;
    b.step();
    for (const id of ids) expect(b.unit(id)).toBeDefined();
    expect(b.log.some((e) => e.what === 'land:joined' && ids.has(e.unit!))).toBe(true);
    b.ai.att = false;
    expect(b.command({ do: 'move', units: [...ids], x: b.field.w / 2, y: b.field.h / 3 }, 'att')).toBe(true);
  }, 60_000);
  it('at the start the bar and the staff\'s chance stand at the card\'s figures, the joiner counted at its part', () => {
    for (const odds of [0.9, 1.3, 2.2]) {
      const t = sampleTicket('plains', odds, { seed: 2 });
      const b0 = new LandBattle(t), b1 = new LandBattle(withJoiners(t, { att: [{ joined: 0.3, part: 0.2 }], def: [{ joined: 0.7, part: 0.15 }] }));
      const k = t.reckoning, st = b1.standing();
      expect(st.att).toBeCloseTo(k.A / (k.A + k.D), 9);
      expect(st.def).toBeCloseTo(k.D / (k.A + k.D), 9);
      expect(b1.chanceNow()).toBeCloseTo(b0.chanceNow(), 9);
      expect(b1.breaking('att')).toBeCloseTo(b0.breaking('att'), 9);
      expect(b1.breaking('def')).toBeCloseTo(b0.breaking('def'), 9);
    }
  });
  it('under fog, an enemy joiner is not told before it is seen, and the drill draws it once it comes', () => {
    const t = withJoiners(sampleTicket('plains', 1.1, { seed: 3, humanAtt: true }), { def: [{ joined: 0.8, toward: Math.PI / 2 }] });
    const b = new LandBattle(t);
    b.ai.att = b.ai.def = true;
    b.fog = true;
    expect(b.view('att').coming ?? []).toEqual([]);
    const d = b.drillOn();
    const us = ofCorps(b, 2101);
    const n0 = b.scale();
    expect(n0).toBe(new LandBattle(withJoiners(sampleTicket('plains', 1.1, { seed: 3 }), { def: [{ joined: 0.8, toward: Math.PI / 2 }] })).scale());
    let told = false;
    for (let k = 0; !b.over && k < 30000; k++) {
      b.step();
      const v = b.view('att');
      if (v.events.some((e) => e.what === 'land:joined')) told = true;
      if (b.unit(us[0].id)) break;
    }
    if (b.over) return;
    const said = b.log.find((e) => e.what === 'land:joined');
    expect(said).toBeDefined();
    expect(told).toBe(!!us[0].found || b.known(us[0]));
    b.step();
    const dv = d.view();
    for (const u of us) expect(JSON.stringify(dv).includes(`"unit":${u.id}`) || JSON.stringify(dv).includes(`"id":${u.id}`)).toBe(true);
  }, 60_000);
});

describe('(6) the day can end before it comes', () => {
  it('a corps due at the very end of a lopsided day never comes: lost nothing, arrived null', () => {
    const t = sampleTicket('plains', 3, { seed: 7 });
    const b = fight(withJoiners(t, { att: [{ joined: 0.02 }] }));
    signed(b);
    expect(b.over).not.toBeNull();
    expect(b.over!.seconds).toBeLessThan(0.98 * b.ticket.limit);
    expect(b.units.some((u) => u.corps === 2001)).toBe(false);
    expect(b.over!.forces!.find((f) => f.corps === 2001)).toEqual({ corps: 2001, side: 'att', lost: 0, arrived: null });
    const c = fight(withJoiners(t, { att: [{ joined: 0.01, kind: 'arm' }] }));
    signed(c);
    expect([c.over!.R, c.over!.lost, c.over!.seconds]).toEqual([b.over!.R, b.over!.lost, b.over!.seconds]);
  }, 120_000);
});

describe('(7) both sides, several nations: the blocks and the sums', () => {
  it('GER, SVK and POL: a block a division, its guns its own, the card\'s A and D to the last figure', () => {
    const t = withJoiners(sampleTicket('forest', 1.2, { seed: 3, nat: { att: 'GER', def: 'POL' } }),
      { att: [{ joined: 0.6, nat: 'SVK', divs: 2, part: 0.08 }, { joined: 0.3, nat: 'GER', kind: 'arm', divs: 1, part: 0.12 }], def: [{ joined: 0.5, nat: 'POL', divs: 3, kind: 'inf', part: 0.15 }] });
    const b = new LandBattle(t);
    // In id order: numbered after both armies, the attack's corps and then the defence's (they come on by their time).
    const js = b.allBlocks().filter((u) => u.joined !== undefined).sort((a, c) => a.id - c.id);
    expect(js[0].id).toBe(b.units.length + 1);
    const byCorps = (c: number): LandUnit[] => js.filter((u) => u.corps === c);
    expect(byCorps(2001).length).toBe(2);
    expect(byCorps(2002).length).toBe(1);
    expect(byCorps(2101).length).toBe(3);
    expect(js.map((u) => u.corps)).toEqual([2001, 2001, 2002, 2101, 2101, 2101]);
    for (const u of js) {
      expect(u.cal ?? undefined).toEqual(calFor(u.kind, u.nat, t.turn) ?? undefined);
      expect(u.power0).toBeCloseTo(u.priced! / u.joined!, 9);
      expect(u.dug).toBe(false);
    }
    for (const side of ['att', 'def'] as Side[]) {
      const main = b.units.filter((u) => u.side === side).reduce((a, u) => a + u.power0, 0);
      const priced = js.filter((u) => u.side === side).reduce((a, u) => a + u.priced!, 0);
      const want = side === 'att' ? t.reckoning.A : t.reckoning.D;
      expect(Math.abs(main + priced - want) / want).toBeLessThan(1e-9);
    }
  });
});

describe('(8) it walks on as a reserve already sent (b407), to where its side is struck', () => {
  for (const side of ['att', 'def'] as Side[]) {
    it(`${side === 'att' ? 'the attack' : 'the defence'}'s joiner: sent, never taken in hand, and only at a foe its line is fighting`, () => {
      const t = withJoiners(sampleTicket('plains', 1.1, { seed: 8 }), { [side]: [{ joined: 0.7, toward: Math.PI / 2 }] });
      const b = fight(t);
      const ids = new Set(ofCorps(b, side === 'att' ? 2001 : 2101).map((u) => u.id));
      const bad: string[] = [];
      const cmd = b.command.bind(b);
      b.command = (c, by) => {
        if (c.do === 'attack' && by === side && c.units.some((i) => ids.has(i))) {
          const line = b.units.filter((u) => u.side === side && !ids.has(u.id) && u.engaged !== undefined).map((u) => u.engaged);
          if (!line.includes(c.target)) bad.push(`${b.t.toFixed(1)}: ${c.units.join(',')} at ${c.target}`);
        }
        return cmd(c, by);
      };
      signed(b);
      expect(bad).toEqual([]);
      for (const id of ids) {
        const u = b.allBlocks().find((x) => x.id === id)!;
        if (!b.unit(id)) continue;
        expect(u.reserve === 'sent' || u.kind === 'art').toBe(true);
        expect(u.taken ?? false).toBe(false);
      }
    }, 120_000);
  }
});

// The measure the design director asked for (D1, 12:23Z): on ruling 1's whole-week shares (0.75, 0.5, 0.25), at odds
// 1.0 and 1.4 over 40 seeds, each against the same card fought whole, a joiner priced at 0.15 coming from its own back
// edge and from the field's east flank. Each row also counts the battles it came up in and those in which it fought.
// LONG=1 runs it; the table is printed whether it passes or not. On da5e15b1 all twelve rows were within the tenth
// (0.000 to 0.100). The attack's joiner fought in 59, 20 and 1 of 80 from behind (early, midway, late) and in 11 of 80
// from the flank, where its march to the middle of its side (about 230 s on foot) is longer than its share leaves it.
describe('(9) the field still lands near the card with a joiner priced on it (a measure: LONG=1)', () => {
  const env = (globalThis as { process?: { env: Record<string, string | undefined>; stdout?: { write(s: string): void } } }).process;
  const LONG = !!env?.env.LONG;
  const run = (b: LandBattle, each?: (b: LandBattle) => void): LandBattle => { for (let k = 0; !b.over && k < 30000; k++) { b.step(); each?.(b); } return b; };
  it.runIf(LONG)('each share, each side, from behind and from a flank: the win rate within a tenth of the same card fought whole', () => {
    const rows: string[] = [], gaps: number[] = [];
    const whole = new Map<string, number>();
    for (let seed = 1; seed <= 40; seed++) for (const odds of [1.0, 1.4]) whole.set(`${seed}:${odds}`, run(fight(sampleTicket('plains', odds, { seed }))).over!.R);
    for (const [from, toward] of [['its back edge', undefined], ['the east flank', Math.PI / 2]] as [string, number | undefined][]) {
      for (const side of ['att', 'def'] as Side[]) for (const joined of [0.75, 0.5, 0.25]) {
        let w0 = 0, w1 = 0, lr = 0, n = 0, came = 0, fought = 0;
        for (let seed = 1; seed <= 40; seed++) for (const odds of [1.0, 1.4]) {
          const r0 = whole.get(`${seed}:${odds}`)!;
          let hit = false;
          const c = run(fight(withJoiners(sampleTicket('plains', odds, { seed }), { [side]: [{ joined, part: 0.15, toward }] })), (b) => { if (!hit && b.units.some((u) => u.joined !== undefined && u.engaged !== undefined)) hit = true; });
          w0 += r0 >= 1.2 ? 1 : 0; w1 += c.over!.R >= 1.2 ? 1 : 0;
          lr += Math.log(Math.max(1e-6, c.over!.R) / Math.max(1e-6, r0)); n++;
          if (c.over!.forces!.some((f) => f.corps >= 2000 && f.arrived !== null && f.arrived !== undefined)) came++;
          if (hit) fought++;
        }
        gaps.push(Math.abs(w1 - w0) / n);
        rows.push(`${side === 'att' ? 'attack ' : 'defence'} joiner from ${from}, ${word(joined)} (${joined}): attack wins ${w0}/${n} whole, ${w1}/${n} with it (gap ${(Math.abs(w1 - w0) / n).toFixed(3)}); R ratio ${Math.exp(lr / n).toFixed(3)}; came up in ${came}/${n}, fought in ${fought}/${n}`);
      }
    }
    env?.stdout?.write(`\n${rows.join('\n')}\n`);
    // Every row within the field's own net of a tenth; the message carries the whole table.
    expect(gaps.every((g) => g <= 0.1), rows.join('\n')).toBe(true);
  }, 3_600_000);
});
