// The shared battle core (src/battle/core): how a fought battle's result lands on the campaign's scale (ruling 217),
// the clock, and the line a group is drawn up on. Shared by the land, sea and air battles.
import { describe, expect, it } from 'vitest';
import { CLAMP, Clock, clampToDice, DROPS, diceChance, diceNow, fieldRatio, levelFor, levelOf, lineUp, LOOPS, nextGap, nextTake, ONE_SHOTS, RATIO, runOut, SALVOS, TOWN, type BattleSim, type UnitBase } from '../src/battle/core';

describe('R: where a fought battle lands on the campaign scale', () => {
  it('a field win is on or above the line, a field defeat below it', () => {
    for (const a of [0, 0.1, 0.4, 0.8]) for (const d of [0, 0.1, 0.4, 0.8]) {
      expect(fieldRatio('att', { att: a, def: d })).toBeGreaterThanOrEqual(RATIO.carry);
      expect(fieldRatio('def', { att: a, def: d })).toBeLessThan(RATIO.carry);
    }
  });
  it('the more lopsided the exchange, the further from the line', () => {
    expect(fieldRatio('att', { att: 0.1, def: 0.6 })).toBeGreaterThan(fieldRatio('att', { att: 0.3, def: 0.4 }));
    expect(fieldRatio('def', { att: 0.6, def: 0.1 })).toBeLessThan(fieldRatio('def', { att: 0.4, def: 0.3 }));
  });
  it('is held to the run of the dice, and may cross only to the line itself (217(2), (a))', () => {
    expect(CLAMP.mode).toBe('across');
    // Inside the run: untouched.
    expect(clampToDice(1.3, 1.2, [0.8, 1.2], 'att')).toBeCloseTo(1.3);
    // Bigger than the dice could roll: held to the top.
    expect(clampToDice(3.5, 1.2, [0.8, 1.2], 'att')).toBeCloseTo(1.44);
    // A field win where the dice could not give one: a bare close victory, no more.
    expect(clampToDice(2.5, 0.9, [0.8, 1.2], 'att')).toBe(RATIO.carry);
    // A field defeat where the dice could only give a win: a bare close defeat.
    expect(clampToDice(0.5, 2, [0.8, 1.2], 'def')).toBeCloseTo(RATIO.carry - 0.01);
    CLAMP.mode = 'dice';
    expect(clampToDice(2.5, 0.9, [0.8, 1.2], 'att')).toBeCloseTo(1.08);
    CLAMP.mode = 'across';
  });
  it('the dice at the ratio then standing stay inside the run (217(4))', () => {
    for (const roll of [0, 0.5, 0.999]) for (const st of [{ att: 0.1, def: 1 }, { att: 1, def: 1 }, { att: 1, def: 0.05 }]) {
      const R = diceNow(1.3, [0.8, 1.2], st, roll);
      expect(R).toBeGreaterThanOrEqual(1.3 * 0.8 - 1e-9);
      expect(R).toBeLessThanOrEqual(1.3 * 1.2 + 1e-9);
    }
  });
  it('the levels are the campaign\'s own bands, mirrored for the defender', () => {
    expect(levelOf(3.1, false)).toBe('Heroic victory');
    expect(levelOf(1.25, true)).toBe('Pyrrhic victory');
    expect(levelOf(0.7, false)).toBe('Decisive defeat');
    expect(levelFor('def', 2.2, false)).toBe('Decisive defeat');
    expect(diceChance(1.2, [0.8, 1.2])).toBeCloseTo(0.5);
  });
});

describe('the clock', () => {
  const counter = (): BattleSim & { n: number } => {
    const s = { n: 0, over: null as null | object, ai: { att: false, def: false }, step() { s.n++; } };
    return s as unknown as BattleSim & { n: number };
  };
  it('runs whole ticks for the real time given, at the speed set, and none while paused', () => {
    const c = new Clock(), sim = counter();
    c.advance(sim, 1); expect(sim.n).toBe(10);
    c.set(2); c.advance(sim, 1); expect(sim.n).toBe(30);
    c.toggle(); c.advance(sim, 1); expect(sim.n).toBe(30);
    c.toggle(); expect(c.speed).toBe(2);
    c.nudge(1); expect(c.speed).toBe(4);
  });
  it('never runs away after a stall', () => {
    const c = new Clock(), sim = counter();
    c.advance(sim, 60);
    expect(sim.n).toBe(c.maxTicks);
    runOut({ ...sim, step() { sim.n++; if (sim.n > 100) (sim as { over: object | null }).over = {}; } } as unknown as BattleSim);
  });
});

describe('drawing a line', () => {
  const u = (id: number, x: number, y: number): UnitBase => ({ id, side: 'att', corps: 1, nat: 'X', kind: 'inf', name: '', x, y, face: 0, str: 1, mor: 1, state: 'steady', men0: 1, figures0: 1, figures: 1, w: 100, d: 30, act: 'idle', speed: 0 });
  it('spreads the blocks along it in their order, facing the enemy\'s edge', () => {
    const got = lineUp([u(1, 500, 0), u(2, 100, 0), u(3, 300, 0)], 'att', 0, 1000, 1000, 1000);
    expect(got.map((p) => p.id)).toEqual([2, 3, 1]);
    for (const p of got) { expect(p.y).toBeCloseTo(1000); expect(Math.cos(p.face)).toBeGreaterThan(0.99); }
    // Drawn from right to left, the attacker still faces north.
    for (const p of lineUp([u(1, 0, 0)], 'att', 1000, 1000, 0, 1000)) expect(Math.cos(p.face)).toBeGreaterThan(0.99);
    // A defender faces south.
    for (const p of lineUp([{ ...u(1, 0, 0), side: 'def' }], 'def', 0, 1000, 1000, 1000)) expect(Math.cos(p.face)).toBeLessThan(-0.99);
  });
});

describe('the field\'s sounds: the approved pack\'s table', () => {
  it('several takes play at random, never the same one twice running', () => {
    let last: string | null = null;
    const heard = new Set<string>();
    for (let i = 0; i < 300; i++) {
      const roll = ((i * 7919) % 1000) / 1000;
      const t = nextTake(SALVOS.takes, last, roll);
      expect(t).not.toBe(last);
      expect(SALVOS.takes).toContain(t);
      heard.add(t); last = t;
    }
    expect(heard.size).toBe(SALVOS.takes.length);
    // A list of one has nothing else to give.
    expect(nextTake(['only'], 'only', 0.5)).toBe('only');
  });
  it('guns in action drop one far salvo every 6 to 20 seconds, not a loop', () => {
    expect(LOOPS.guns.fight).toBeUndefined();
    expect(DROPS.guns?.fight).toBe(SALVOS);
    expect(SALVOS.takes).toEqual(['far_guns_01', 'far_guns_02', 'far_guns_03']);
    expect(nextGap(SALVOS, 0)).toBe(6);
    expect(nextGap(SALVOS, 0.999)).toBeLessThan(20);
  });
  it('lorries drive on the lorry, and a town fight is the street battle with its machine guns', () => {
    expect(LOOPS.wheels.march).toBe('lorry_drive_loop');
    expect(LOOPS.wheels.idle).toBe('lorry_idle_loop');
    expect(TOWN.loop).toBe('street_battle');
    expect(TOWN.drops.takes).toEqual(['mg_town_01', 'mg_town_02', 'mg_town_03']);
  });
  it('names only real sounds from the pack, and no block carries a fight loop of its own (the sound director, 14:59Z)', () => {
    const named = [
      ...Object.values(LOOPS).flatMap((l) => Object.values(l)),
      ...Object.values(DROPS).flatMap((d) => Object.values(d ?? {}).flatMap((x) => x?.takes ?? [])),
      TOWN.loop, ...TOWN.drops.takes, ...Object.values(ONE_SHOTS),
    ];
    // The made-up set the pack replaced, the bugles Brian cut, and the open field's bed, which is shared, not a block's.
    for (const dead of ['battle', 'bugle', 'rifles', 'machine_gun', 'battle_loop', 'order_march', 'order_attack', 'order_cancel']) expect(named).not.toContain(dead);
    expect(LOOPS.foot.fight).toBeUndefined();
    expect(LOOPS.tracks.fight).toBe(LOOPS.tracks.idle);
    expect(LOOPS.wheels.fight).toBe(LOOPS.wheels.idle);
    expect(ONE_SHOTS).toEqual({ contact: 'whistle_long' });
  });
});
