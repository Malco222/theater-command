// Hold until relieved, and the rider ahead (release 2; Brian, 22 November 12:48Z, "Do 2 and 3": "a unit ordered to
// wait for help digs in harder, but breaks faster if help never comes", and "a courier arrives first; clicking him shows
// the relief's strength and arrival time"). The design director's rulings of 13:43Z: the dig-in is the wrecks' own light
// cover (LAND.cover[LAND.wreck.cover]) within LAND.dug.keep; what it spares is kept and lands with a break, or is paid
// out at the pace it was spared, shared by men with the relievers; help is due by the joiners' own reckoning; relieved at
// LAND.alone.near; help never came (due passed, or its corps out) breaks at LAND.nerve.waver; the player's own order ends
// the wait without that mark; one block a joining corps a battle, everyone alike. The rider rides at the field's fastest
// pace from his corps' edge to the middle of his side, the player's own only, and changes nothing in the fight.
import { describe, expect, it } from 'vitest';
import type { FieldEvent, FieldTicket, Side } from '../src/battle/core';
import { DT } from '../src/battle/core';
import { coverAt, diggable, groundAt, LAND, LandBattle, sampleTicket, streamFor, withJoiners, withTeams, type LandPlace, type LandUnit } from '../src/battle/land';
import { thinkFor } from '../src/battle/land/ai';
import { hashString } from '../src/core/rng';
import { unitOut } from '../unreal/TheaterCommand/Rules/battle-bridge';

type T = FieldTicket<LandPlace>;
const env = (globalThis as { process?: { env: Record<string, string | undefined>; stdout?: { write(s: string): void } } }).process;
const PRINT = !!env?.env.PRINT;

/** Everything a battle shows and does, every tick (the join test's own), less the view keys in 'drop'. */
function signed(b: LandBattle, before?: (b: LandBattle, k: number) => void, drop: string[] = []): string {
  const ev: FieldEvent[] = [];
  const keys = new Set<string>();
  for (let k = 0; !b.over && k < 30000; k++) {
    before?.(b, k);
    b.step();
    const v = b.view();
    for (const x of Object.keys(v)) if (!drop.includes(x)) keys.add(x);
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
const pin = (b: LandBattle, s: string): [number, number, string] => [Math.round(b.over!.R * 1000) / 1000, b.over!.seconds, digest(s)];

/** The rules' private workings a test reads. */
interface Inside {
  suffer(t: LandUnit, dmg: number, nerveMul: number): void;
  repay: { held: number; by: number[]; left: number; nerve: number; rate: number }[];
  payWaits(): void;
  giveGround(s: Side, left: LandUnit[]): void;
  waitedFor: Set<number>;
  bar: Record<Side, number>;
  breaking(s: Side): number;
  riders: { corps: number; to: [number, number]; there: number }[];
}
const inside = (b: LandBattle): Inside => b as unknown as Inside;

/** An attack corps coming up from next door, nobody's staff thinking, the relief switch on: the player's the attack. */
function quiet(j: { joined?: number; toward?: number; divs?: number } = {}, human = true): LandBattle {
  const b = new LandBattle(withJoiners(sampleTicket('plains', 1.0, { seed: 2, humanAtt: human }), { att: [{ joined: j.joined ?? 0.5, toward: j.toward, divs: j.divs }] }));
  b.ai.att = b.ai.def = false;
  b.relief = true;
  return b;
}
/** The side's blocks on foot that may wait now, westmost first. */
const mayWait = (b: LandBattle, side: Side): LandUnit[] => b.units.filter((u) => u.side === side && b.waitWhy(u) === null).sort((a, c) => a.x - c.x || a.id - c.id);
const joiners = (b: LandBattle, corps = 2001): LandUnit[] => b.allBlocks().filter((u) => u.corps === corps);
const steps = (b: LandBattle, n: number): void => { for (let k = 0; k < n && !b.over; k++) b.step(); };
const said = (b: LandBattle, what: string): FieldEvent[] => b.log.filter((e) => e.what === what);
/** The power a block has lost: what harm took off it (harm is in power, as its strength meter counts it). */
const lostOf = (u: LandUnit): number => u.power0 * (1 - u.str / u.str0);

// ---- (1) switched off, and on with no order: the same battle -------------------------------------------------------

/**
 * Joiner battles fingerprinted on 182e1be3, before either idea, with signed() and 'riders' dropped (the rider design's
 * pins, scratch pins.ts): both switches off they are today's battle; the rider on changes nothing in the fight.
 */
const JOIN_CASES: [string, () => T, ((b: LandBattle) => void)?][] = [
  ['an attack joiner midway, the attack the player\'s, plains 1.1 seed 3', () => withJoiners(sampleTicket('plains', 1.1, { seed: 3, humanAtt: true }), { att: [{ joined: 0.5 }] })],
  ['a defence joiner late from the east, the defence the player\'s, fog, plains 1.2 seed 8', () => withJoiners(sampleTicket('plains', 1.2, { seed: 8 }), { def: [{ joined: 0.3, toward: Math.PI / 2 }] }), (b) => { b.ticket.def.human = true; b.fog = true; }],
  ['both sides, two attack corps, a river, hills 1.2 seed 5', () => withJoiners(sampleTicket('hills', 1.2, { seed: 5, river: true, humanAtt: true }), { att: [{ joined: 0.6 }, { joined: 0.25, toward: 1.5 * Math.PI, kind: 'arm', divs: 1 }], def: [{ joined: 0.4 }] })],
];
const JOIN_BEFORE: Record<string, [number, number, string]> = {
  'an attack joiner midway, the attack the player\'s, plains 1.1 seed 3': [1.024, 232, '0884eefd6575fb94'],
  'a defence joiner late from the east, the defence the player\'s, fog, plains 1.2 seed 8': [1.053, 239, '161f2cc65c907e5b'],
  'both sides, two attack corps, a river, hills 1.2 seed 5': [1.075, 268, '67f5c6398d259046'],
};
/** Whether a rider rides ahead in each: the defence's late corps from the east comes on at 40 s, before he could ride 1.5 km. */
const RIDES: Record<string, boolean> = { [JOIN_CASES[0][0]]: true, [JOIN_CASES[1][0]]: false, [JOIN_CASES[2][0]]: true };
const joinFought = (name: string, also?: (b: LandBattle) => void, drop: string[] = [], during?: (b: LandBattle, k: number) => void): { got: [number, number, string]; b: LandBattle } => {
  const [, make, setup] = JOIN_CASES.find((c) => c[0] === name)!;
  const b = new LandBattle(make());
  b.ai.att = b.ai.def = true;
  setup?.(b);
  also?.(b);
  const s = signed(b, during, drop);
  return { got: pin(b, s), b };
};

describe('(1) switched off, a joiner battle is the battle of 182e1be3; the rider on changes nothing in the fight', () => {
  for (const [name] of JOIN_CASES) {
    it(`${name}: both off`, () => {
      const { got, b } = joinFought(name);
      if (PRINT) { env?.stdout?.write(`    ${JSON.stringify(name)}: ${JSON.stringify(got)},\n`); return; }
      expect(got).toEqual(JOIN_BEFORE[name]);
      expect(b.allBlocks().some((u) => u.waits || u.waited || u.debt !== undefined)).toBe(false);
    }, 300_000);
    it(`${name}: the rider on, the same fight${RIDES[name] ? ', and he is listed' : ' (his corps beats him there: none)'}`, () => {
      let listed = 0;
      const { got } = joinFought(name, (b) => { b.rider = true; }, ['riders'], (b) => { if (b.riderView().riders?.length) listed++; });
      expect(got).toEqual(JOIN_BEFORE[name]);
      expect(listed > 0).toBe(RIDES[name]);
    }, 300_000);
  }
  it('the relief on with nobody thinking and no order given: the same battle as off', () => {
    const run = (on: boolean): string => {
      const b = new LandBattle(withJoiners(sampleTicket('plains', 1.1, { seed: 3, humanAtt: true }), { att: [{ joined: 0.5 }], def: [{ joined: 0.4 }] }));
      b.ai.att = b.ai.def = false; b.relief = on;
      // Nobody thinks: one order to march on, so the armies meet and the joiners come up.
      return signed(b, (x, k) => { if (k === 0) x.command({ do: 'attack', units: x.units.filter((u) => u.side === 'att' && u.kind !== 'art').map((u) => u.id), target: x.units.find((u) => u.side === 'def')!.id }, 'att'); });
    };
    expect(run(true)).toBe(run(false));
  }, 300_000);
});

// ---- (2) who may wait -----------------------------------------------------------------------------------------------

describe('(2) who may wait for help, and why not', () => {
  it('only with the switch on, and only where a corps of its own side is coming up', () => {
    const b = quiet();
    b.relief = false;
    const u = b.units.find((x) => x.side === 'att' && x.kind === 'inf')!;
    expect(b.waitWhy(u)).toBe('No help is coming up from next door.');
    expect(b.command({ do: 'land:wait', units: [u.id] }, 'att')).toBe(false);
    b.relief = true;
    expect(b.waitWhy(u)).toBeNull();
    const d = b.units.find((x) => x.side === 'def' && x.kind === 'inf')!;
    expect(b.waitWhy(d)).toBe('No help is coming up from next door.');
  });
  it('help of its own side only: the enemy\'s corps coming up gives the player nothing to wait for, and says nothing of it', () => {
    const b = new LandBattle(withJoiners(sampleTicket('plains', 1.0, { seed: 2, humanAtt: true }), { def: [{ joined: 0.5 }] }));
    b.ai.att = b.ai.def = false; b.relief = true;
    expect(b.hasJoiners()).toBe(true);
    expect(b.hasJoiners('def')).toBe(true);
    expect(b.hasJoiners('att')).toBe(false);
    const u = b.units.find((x) => x.side === 'att' && x.kind === 'arm') ?? b.units.find((x) => x.side === 'att')!;
    // The same words as a battle with no corps coming at all: never 'Only men on foot...', which would tell of one.
    expect(b.waitWhy(u)).toBe('No help is coming up from next door.');
    expect(b.units.filter((x) => x.side === 'att').every((x) => b.waitWhy(x) === 'No help is coming up from next door.')).toBe(true);
  });
  it('men on foot only: never tanks, lorries, guns or a team', () => {
    const b = quiet();
    for (const u of b.units.filter((x) => x.side === 'att')) {
      const foot = u.kind === 'inf' || u.kind === 'mtn' || u.kind === 'cav' || u.kind === 'eng';
      if (!foot) expect(b.waitWhy(u)).toBe('Only men on foot dig in to wait for help.');
    }
    expect(b.units.some((u) => u.side === 'att' && u.kind === 'arm')).toBe(true);
  });
  it('out of the fight', () => {
    const b = quiet();
    const u = mayWait(b, 'att')[0];
    u.state = 'routing';
    expect(b.waitWhy(u)).toBe('It is out of the fight.');
    expect(b.command({ do: 'land:wait', units: [u.id] }, 'att')).toBe(false);
  });
  it('never where no fieldworks can be dug: houses, a bridge, the river', () => {
    const b = quiet();
    const u = mayWait(b, 'att')[0], F = b.field;
    for (const g of ['bridge', 'buildings', 'water'] as const) {
      const i = F.ground.findIndex((_, k) => groundAt(F, (k % F.cols + 0.5) * F.cell, (Math.floor(k / F.cols) + 0.5) * F.cell) === g);
      if (i < 0) continue;
      const x0 = u.x, y0 = u.y;
      u.x = (i % F.cols + 0.5) * F.cell; u.y = (Math.floor(i / F.cols) + 0.5) * F.cell;
      expect(diggable(coverAt(F, u.x, u.y))).toBe(false);
      expect(b.waitWhy(u)).toBe('There is no digging in here.');
      u.x = x0; u.y = y0;
    }
    expect(b.waitWhy(u)).toBeNull();
  });
  it('help that could not reach it before the day is out', () => {
    // Late in the day, from the east flank: the westmost block is too far for it.
    const b = quiet({ joined: 0.1, toward: Math.PI / 2 });
    const west = b.units.filter((x) => x.side === 'att' && x.kind === 'inf').sort((a, c) => a.x - c.x)[0];
    expect(b.dueAt(west)!.due).toBeGreaterThan(b.ticket.limit);
    expect(b.waitWhy(west)).toBe('Help could not reach it before the day is out.');
  });
  it('one block a joining corps a battle: then each is spoken for, and a block waits once', () => {
    const b = quiet();
    const [u, v] = mayWait(b, 'att');
    expect(b.command({ do: 'land:wait', units: [u.id] }, 'att')).toBe(true);
    expect(u.waits?.corps).toBe(2001);
    expect(b.waitWhy(u)).toBe('It is waiting for help already.');
    expect(b.waitWhy(v)).toBe('Each corps coming up relieves one block, and each is spoken for.');
    expect(b.command({ do: 'land:wait', units: [v.id] }, 'att')).toBe(false);
    // Called off by his own order, it has waited once today.
    b.command({ do: 'move', units: [u.id], x: u.x, y: u.y + 50 }, 'att');
    expect(u.waits).toBeUndefined();
    expect(b.waitWhy(u)).toBe('It has waited for help once today.');
  });
  it('two corps coming: two blocks may wait, each for its own; the default is the soonest', () => {
    const b = new LandBattle(withJoiners(sampleTicket('plains', 1.0, { seed: 2, humanAtt: true }), { att: [{ joined: 0.4 }, { joined: 0.7, toward: Math.PI / 2 }] }));
    b.ai.att = b.ai.def = false; b.relief = true;
    const [u, v] = mayWait(b, 'att');
    const first = b.dueAt(u)!;
    b.command({ do: 'land:wait', units: [u.id] }, 'att');
    expect(u.waits!.corps).toBe(first.corps);
    expect(u.waits!.due).toBe(first.due);
    b.command({ do: 'land:wait', units: [v.id] }, 'att');
    expect(v.waits!.corps).not.toBe(u.waits!.corps);
    expect(mayWait(b, 'att')).toEqual([]);
  });
  it('told together, each block speaks for its corps in turn: one that cannot wait keeps its order, with the reason', () => {
    const b = quiet();
    const [u, v] = mayWait(b, 'att');
    b.command({ do: 'move', units: [v.id], x: v.x, y: v.y + 200 }, 'att');
    const o = JSON.stringify(v.order);
    expect(b.waitWhyAll([u, v])).toEqual([null, 'Each corps coming up relieves one block, and each is spoken for.']);
    expect(b.command({ do: 'land:wait', units: [u.id, v.id] }, 'att')).toBe(true);
    expect(u.waits).toBeDefined();
    expect(v.waits).toBeUndefined();
    expect(JSON.stringify(v.order)).toBe(o);
  });
  it('in the setup step a block that waits is dug in where it stands: it is not placed elsewhere', () => {
    const b = quiet();
    expect(b.setupOn('att', true)).toBe(true);
    const u = mayWait(b, 'att')[0];
    expect(b.command({ do: 'land:wait', units: [u.id] }, 'att')).toBe(true);
    expect(u.waits).toBeDefined();
    expect(b.placeWhy(u, u.x + 60, u.y)).toBe('It is dug in where it stands.');
  });
  it('help beside it already', () => {
    const b = quiet({ joined: 0.75, toward: Math.PI / 2 });
    steps(b, 1);
    const j = joiners(b).find((x) => b.unit(x.id))!;
    const u = b.units.filter((x) => x.side === 'att' && x.kind === 'inf' && x.joined === undefined).sort((a, c) => a.x - c.x)[0];
    j.x = u.x + 150; j.y = u.y;
    expect(b.waitWhy(u)).toBe('Help is beside it already.');
  });
});

// ---- (3) the order, and what the block does ------------------------------------------------------------------------

describe('(3) the order lands: it stands, dug in, and its staff never orders it', () => {
  it('it stands where it was told, with when help is due by the joiners\' own reckoning', () => {
    const b = quiet();
    const u = mayWait(b, 'att')[0];
    b.command({ do: 'move', units: [u.id], x: u.x, y: u.y + 300 }, 'att');
    expect(u.order).not.toBeNull();
    const due = b.dueAt(u)!;
    expect(b.command({ do: 'land:wait', units: [u.id] }, 'att')).toBe(true);
    expect(u.order).toBeNull();
    expect(u.path).toEqual([]);
    expect(u.waits).toEqual({ corps: 2001, due: due.due, at: [u.x, u.y], since: 0 });
    // The corps' nearest block, from its place on its edge at its time, at its own pace over open ground.
    const c = b.view().coming!.filter((x) => x.corps === 2001);
    const want = Math.min(...c.map((x) => x.at + Math.hypot(x.x - u.x, x.y - u.y) / LAND.speed.inf));
    expect(due.due).toBeCloseTo(want, 3);
    expect(said(b, 'land:waits').map((e) => e.unit)).toEqual([u.id]);
    const x0 = u.x, y0 = u.y;
    steps(b, 50);
    expect(Math.hypot(u.x - x0, u.y - y0)).toBeLessThan(1);
  });
  it('at the middle of its side, help is due when the corps reaches its line: (1 - joined) of the clock', () => {
    const b = quiet({ joined: 0.5, divs: 1 });
    const own = b.units.filter((x) => x.side === 'att');
    const mid = { side: 'att', corps: -1, x: own.reduce((a, x) => a + x.x, 0) / own.length, y: own.reduce((a, x) => a + x.y, 0) / own.length } as LandUnit;
    expect(b.dueAt(mid)!.due).toBeCloseTo(0.5 * b.ticket.limit, 2);
  });
  it('the order of a block of the side nobody plays goes as its staff\'s: no word to the player', () => {
    const b = quiet({}, false);
    const u = mayWait(b, 'att')[0];
    expect(b.command({ do: 'land:wait', units: [u.id] }, 'att')).toBe(true);
    expect(u.waits).toBeDefined();
    expect(said(b, 'land:waits')).toEqual([]);
  });
  it('by radio: the order reaches it after the delay, and is gated again then', () => {
    const b = quiet();
    b.radio = true; b.perUnit = true;
    steps(b, 1);
    const u = mayWait(b, 'att')[0];
    expect(b.command({ do: 'land:wait', units: [u.id] }, 'att')).toBe(true);
    expect(u.waits).toBeUndefined();
    expect(u.taken).toBe(true);
    const at = u.orderAt!;
    while (b.t < at - 1e-9) b.step();
    b.step();
    expect(u.waits).toBeDefined();
  });
  it('a hold keeps the wait; his move calls it off, with no mark of help that never came', () => {
    const b = quiet();
    const u = mayWait(b, 'att')[0];
    b.command({ do: 'land:wait', units: [u.id] }, 'att');
    b.command({ do: 'hold', units: [u.id] }, 'att');
    expect(u.waits).toBeDefined();
    u.debt = 0.2; u.debtN = 0.3;
    steps(b, 10);
    b.command({ do: 'move', units: [u.id], x: u.x + 100, y: u.y }, 'att');
    expect(u.waits).toBeUndefined();
    expect(u.debt).toBeUndefined();
    const p = inside(b).repay.find((x) => x.held === u.id)!;
    // Paid out at the pace it was spared, on it alone.
    expect(p.by).toEqual([]);
    expect(p.left).toBeCloseTo(0.2, 9);
    expect(p.nerve).toBeCloseTo(1.5, 9);
    expect(p.rate).toBeCloseTo(0.2 / b.t, 6);
  });
  it('its staff never orders it, and it fires on what comes into its reach', () => {
    const b = new LandBattle(withJoiners(sampleTicket('plains', 1.0, { seed: 4 }), { att: [{ joined: 0.4 }] }));
    b.ai.att = b.ai.def = true; b.relief = true;
    let id: number | null = null, ordered = 0, fired = 0;
    const was = b.command.bind(b);
    b.command = (c, by) => {
      const ok = was(c, by);
      if (ok && id !== null && 'units' in c && (c.units as number[] | undefined)?.includes(id) && b.unit(id)?.waits && !b.unit(id)!.waits!.free) ordered++;
      return ok;
    };
    for (let k = 0; !b.over && k < 30000; k++) {
      b.step();
      const w = b.units.find((u) => u.waits && !u.waits.free);
      if (id === null && w) id = w.id;
      // (Once the day is done its army falls back, waiting or not.)
      if (w && w.id === id && !b.over) { expect(w.order).toBeNull(); if (w.engaged !== undefined) fired++; }
    }
    expect(id).not.toBeNull();
    expect(ordered).toBe(0);
    expect(fired).toBeGreaterThan(0);
  }, 120_000);
});

// ---- (4) digs in harder -------------------------------------------------------------------------------------------

describe('(4) digs in harder: the wrecks\' light cover where it was told to wait; what it spares is kept', () => {
  it('on open ground it takes 0.85 of a blow, and the rest is owed with its nerve', () => {
    const b = quiet();
    const u = mayWait(b, 'att').find((x) => LAND.cover[coverAt(b.field, x.x, x.y)] > LAND.cover[LAND.wreck.cover])!;
    const g = coverAt(b.field, u.x, u.y), take = LAND.cover[g];
    expect(take).toBeGreaterThan(LAND.cover[LAND.wreck.cover]);
    b.command({ do: 'land:wait', units: [u.id] }, 'att');
    const s0 = u.str, d = 0.05 * u.power0;
    inside(b).suffer(u, d, 2);
    const f = LAND.cover[LAND.wreck.cover] / take;
    expect(lostOf(u)).toBeCloseTo((d * f / u.power0) * u.power0 + (1 - s0 / u.str0) * u.power0, 9);
    expect(u.debt).toBeCloseTo(d * (1 - f), 12);
    expect(u.debtN).toBeCloseTo(d * (1 - f) * 2, 12);
  });
  it('never better than its ground gives: in woods nothing changes; nor beyond LAND.dug.keep of its spot; late, it is still dug in; free, not', () => {
    const b = quiet();
    const u = mayWait(b, 'att')[0], F = b.field;
    b.command({ do: 'land:wait', units: [u.id] }, 'att');
    const i = F.ground.findIndex((_, k) => coverAt(F, (k % F.cols + 0.5) * F.cell, (Math.floor(k / F.cols) + 0.5) * F.cell) === 'woods');
    expect(i).toBeGreaterThanOrEqual(0);
    const x0 = u.x, y0 = u.y;
    u.x = (i % F.cols + 0.5) * F.cell; u.y = (Math.floor(i / F.cols) + 0.5) * F.cell; u.waits!.at = [u.x, u.y];
    inside(b).suffer(u, 0.01, 1);
    expect(u.debt).toBeUndefined();
    u.x = x0; u.y = y0; u.waits!.at = [x0, y0 + LAND.dug.keep + 1];
    inside(b).suffer(u, 0.01, 1);
    expect(u.debt).toBeUndefined();
    // Help late, it still waits where it dug in: the cover holds, and what it spares is kept.
    u.waits!.at = [x0, y0]; u.waits!.late = true;
    const take = LAND.cover[coverAt(F, x0, y0)];
    inside(b).suffer(u, 0.01, 1);
    if (take > LAND.cover[LAND.wreck.cover]) expect(u.debt).toBeCloseTo(0.01 * (1 - LAND.cover[LAND.wreck.cover] / take), 12);
    else expect(u.debt).toBeUndefined();
    // Moved on by his order (free), it is no longer dug in.
    const kept = u.debt;
    u.waits!.free = true;
    inside(b).suffer(u, 0.01, 1);
    expect(u.debt).toBe(kept);
  });
});

// ---- (5) relieved ---------------------------------------------------------------------------------------------------

describe('(5) relieved: what was spared is paid out at the pace it was spared, shared by men with the relievers', () => {
  it('relieved once a block of the joining corps is within LAND.alone.near; never one blow, all of it by the end', () => {
    const b = quiet({ joined: 0.75, toward: Math.PI / 2 });
    steps(b, 1);
    const u = b.units.filter((x) => x.side === 'att' && x.kind === 'inf' && x.joined === undefined && b.waitWhy(x) === null).sort((a, c) => a.x - c.x)[0];
    b.command({ do: 'land:wait', units: [u.id] }, 'att');
    steps(b, 100);
    const D = b.t - u.waits!.since;
    u.debt = 0.06 * u.power0; u.debtN = u.debt;
    const debt = u.debt;
    const j = joiners(b).find((x) => b.unit(x.id))!;
    j.x = u.x + Math.max(u.w, u.d) / 2 + LAND.alone.near - 20; j.y = u.y;
    expect(b.gap(j, u)).toBeLessThanOrEqual(LAND.alone.near);
    const before = { u: lostOf(u), j: lostOf(j) };
    b.step();
    expect(u.waits).toBeUndefined();
    expect(said(b, 'land:relieved').map((e) => e.unit)).toEqual([u.id]);
    const p = inside(b).repay.find((x) => x.held === u.id)!;
    expect(p.by).toEqual([j.id]);
    expect(p.rate).toBeCloseTo(debt / D, 6);
    // One tick's worth landed, shared by men: never the whole as one blow.
    const got = { u: lostOf(u) - before.u, j: lostOf(j) - before.j };
    expect(got.u + got.j).toBeCloseTo(Math.min(debt, p.rate * DT), 9);
    const mu = u.men0 * u.str / u.str0, mj = j.men0 * j.str / j.str0;
    expect(got.u / (got.u + got.j)).toBeCloseTo(mu / (mu + mj), 2);
    // Paid to the last of it: what was spared, no more.
    while (inside(b).repay.length && !b.over) b.step();
    expect(lostOf(u) - before.u + lostOf(j) - before.j).toBeCloseTo(debt, 9);
    expect(b.waitWhy(u)).toBe('It has waited for help once today.');
  });
  it('a block that leaves the field still pays its share, off its strength; the others never take it', () => {
    const b = quiet({ joined: 0.75, toward: Math.PI / 2 });
    steps(b, 1);
    const u = b.units.filter((x) => x.side === 'att' && x.kind === 'inf' && x.joined === undefined && b.waitWhy(x) === null).sort((a, c) => a.x - c.x)[0];
    b.command({ do: 'land:wait', units: [u.id] }, 'att');
    steps(b, 100);
    u.debt = 0.06 * u.power0; u.debtN = u.debt;
    const j = joiners(b).find((x) => b.unit(x.id))!;
    j.x = u.x + Math.max(u.w, u.d) / 2 + LAND.alone.near - 20; j.y = u.y;
    b.step();
    const p = inside(b).repay.find((x) => x.held === u.id)!;
    expect(p.by).toEqual([j.id]);
    // The held block leaves the field: its share still comes off it, and the reliever's stays its own.
    u.state = 'withdrawn';
    const mu = u.men0 * u.str / u.str0, mj = j.men0 * j.str / j.str0, before = { u: lostOf(u), j: lostOf(j) }, left = p.left;
    inside(b).payWaits();
    expect(lostOf(u) - before.u).toBeCloseTo(left * mu / (mu + mj), 9);
    expect(lostOf(j) - before.j).toBeCloseTo(left * mj / (mu + mj), 9);
  });
  it('whatever is still owed lands when the day ends, and the staff\'s chance counts it already', () => {
    const b = quiet();
    const u = mayWait(b, 'att')[0];
    b.command({ do: 'land:wait', units: [u.id] }, 'att');
    steps(b, 20);
    u.debt = 0.04 * u.power0; u.debtN = u.debt;
    const chance = b.chanceNow(), l0 = lostOf(u), d = u.debt;
    inside(b).payWaits();
    expect(u.debt).toBeUndefined();
    expect(lostOf(u) - l0).toBeCloseTo(d, 9);
    expect(b.chanceNow()).toBeCloseTo(chance, 9);
  });
  it('handed to the staff: paid before the dice', () => {
    const b = quiet();
    const u = mayWait(b, 'att')[0];
    b.command({ do: 'land:wait', units: [u.id] }, 'att');
    steps(b, 20);
    u.debt = 0.04 * u.power0; u.debtN = u.debt;
    const l0 = lostOf(u), d = u.debt;
    b.command({ do: 'dice' }, 'att');
    expect(b.over).not.toBeNull();
    expect(lostOf(u) - l0).toBeCloseTo(d, 9);
  });
});

// ---- (6) help never came ------------------------------------------------------------------------------------------

describe('(6) help never came: it breaks where a block starts to waver, until relieved', () => {
  it('past due with none beside it: late, told, and it breaks at LAND.nerve.waver', () => {
    const b = quiet();
    const [u, v] = mayWait(b, 'att');
    b.command({ do: 'land:wait', units: [u.id] }, 'att');
    steps(b, 10);
    u.debt = 0.03 * u.power0; u.debtN = u.debt;
    u.waits!.due = b.t;
    b.step();
    expect(u.waits?.late).toBe(true);
    expect(said(b, 'land:late').map((e) => e.unit)).toEqual([u.id]);
    // It still stands and still waits, dug in: what was spared is still kept (late is not one of the times it lands).
    expect(u.order).toBeNull();
    expect(u.debt).toBeGreaterThan(0);
    expect(inside(b).repay.some((x) => x.held === u.id)).toBe(false);
    expect(b.waitWhy(u)).toBe('It is waiting for help already.');
    // Below the waver mark, above the rout's: the let-down block runs, the other does not.
    u.mor = v.mor = (LAND.nerve.waver + LAND.nerve.rout) / 2;
    const l0 = lostOf(u), left = u.debt!;
    b.step();
    expect(u.state).toBe('routing');
    expect(v.state).toBe('wavering');
    expect(u.waits).toBeUndefined();
    // With the break, what was kept lands on it at once.
    expect(u.debt).toBeUndefined();
    expect(inside(b).repay.some((x) => x.held === u.id)).toBe(false);
    expect(lostOf(u) - l0).toBeGreaterThanOrEqual(left - 1e-9);
    expect(said(b, 'routing').filter((e) => e.unit === u.id).length).toBe(1);
  });
  it('its corps out of the fight: late at once', () => {
    const b = quiet({ joined: 0.75, toward: Math.PI / 2 });
    steps(b, 1);
    const u = b.units.filter((x) => x.side === 'att' && x.kind === 'inf' && x.joined === undefined && b.waitWhy(x) === null).sort((a, c) => a.x - c.x)[0];
    b.command({ do: 'land:wait', units: [u.id] }, 'att');
    for (const j of joiners(b)) j.state = 'gone';
    b.step();
    expect(u.waits?.late).toBe(true);
  });
  it('late, then his order: it moves, keeping the mark; relieved, the mark lifts', () => {
    const b = quiet({ joined: 0.75, toward: Math.PI / 2 });
    steps(b, 1);
    const u = b.units.filter((x) => x.side === 'att' && x.kind === 'inf' && x.joined === undefined && b.waitWhy(x) === null).sort((a, c) => a.x - c.x)[0];
    b.command({ do: 'land:wait', units: [u.id] }, 'att');
    u.waits!.due = b.t;
    b.step();
    expect(u.waits?.late).toBe(true);
    b.command({ do: 'move', units: [u.id], x: u.x + 50, y: u.y }, 'att');
    expect(u.waits).toMatchObject({ late: true, free: true });
    expect(u.order?.kind).toBe('move');
    const j = joiners(b).find((x) => b.unit(x.id))!;
    j.x = u.x + 100; j.y = u.y;
    b.step();
    expect(u.waits).toBeUndefined();
  });
  it('late, then relieved: what was kept is paid out from then, shared by men with the relievers', () => {
    const b = quiet({ joined: 0.75, toward: Math.PI / 2 });
    steps(b, 1);
    const u = b.units.filter((x) => x.side === 'att' && x.kind === 'inf' && x.joined === undefined && b.waitWhy(x) === null).sort((a, c) => a.x - c.x)[0];
    b.command({ do: 'land:wait', units: [u.id] }, 'att');
    steps(b, 50);
    u.debt = 0.05 * u.power0; u.debtN = u.debt;
    u.waits!.due = b.t;
    b.step();
    expect(u.waits?.late).toBe(true);
    const debt = u.debt!;
    expect(debt).toBeGreaterThanOrEqual(0.05 * u.power0);
    steps(b, 20);
    expect(inside(b).repay.length).toBe(0);
    const j = joiners(b).find((x) => b.unit(x.id))!;
    j.x = u.x + Math.max(u.w, u.d) / 2 + LAND.alone.near - 20; j.y = u.y;
    const owed = u.debt!;
    b.step();
    expect(u.waits).toBeUndefined();
    const p = inside(b).repay.find((x) => x.held === u.id)!;
    expect(p.by).toEqual([j.id]);
    expect(p.left + Math.min(owed, p.rate * DT)).toBeCloseTo(owed, 9);
  });
  it('giving ground at the brink ends the wait with no mark; men already let down stay so', () => {
    const b = quiet();
    const [u, w] = mayWait(b, 'att');
    b.command({ do: 'land:wait', units: [u.id] }, 'att');
    u.str = 0.5 * u.str0; u.met = true;
    inside(b).giveGround('att', [u]);
    expect(u.waits).toBeUndefined();
    expect(u.order?.kind).toBe('withdraw');
    // Late, then giving ground: it falls back, still let down, and breaks at the waver mark.
    const c = quiet();
    const [v] = mayWait(c, 'att');
    c.command({ do: 'land:wait', units: [v.id] }, 'att');
    v.waits!.due = c.t;
    c.step();
    expect(v.waits?.late).toBe(true);
    v.str = 0.5 * v.str0; v.met = true; v.debt = 0.02 * v.power0; v.debtN = v.debt;
    inside(c).giveGround('att', [v]);
    expect(v.order?.kind).toBe('withdraw');
    expect(v.waits).toMatchObject({ late: true, free: true });
    expect(v.debt).toBeUndefined();
    expect(inside(c).repay.find((x) => x.held === v.id)?.by).toEqual([]);
    v.mor = (LAND.nerve.waver + LAND.nerve.rout) / 2;
    c.step();
    expect(v.state).toBe('routing');
    expect(w.state).not.toBe('routing');
  });
  it('pulling the whole army out: what was kept lands before the bar it left at is taken', () => {
    const b = quiet();
    const u = mayWait(b, 'att')[0];
    b.command({ do: 'land:wait', units: [u.id] }, 'att');
    steps(b, 20);
    u.debt = 0.2 * u.power0; u.debtN = u.debt;
    const before = inside(b).breaking('att'), l0 = lostOf(u), d = u.debt;
    expect(b.command({ do: 'withdrawAll', side: 'att' }, 'att')).toBe(true);
    expect(lostOf(u) - l0).toBeCloseTo(d, 9);
    expect(inside(b).bar.att).toBeGreaterThan(before);
  });
  it('kept harm landing finds no one: a hidden block that waits stays hidden', () => {
    const b = quiet();
    const u = mayWait(b, 'att')[0];
    b.command({ do: 'land:wait', units: [u.id] }, 'att');
    b.hide = true; u.hid = true;
    u.debt = 0.02 * u.power0; u.debtN = u.debt;
    inside(b).payWaits();
    expect(u.debt).toBeUndefined();
    expect(u.hid).toBe(true);
    expect(said(b, 'land:found').length).toBe(0);
  });
});

// ---- (7) the staffs ----------------------------------------------------------------------------------------------

describe('(7) every staff, the computer\'s and the player\'s alike: one block a joining corps, and its relief sent to it first', () => {
  for (const side of ['att', 'def'] as Side[]) {
    it(`${side === 'att' ? 'an attack' : 'a defence'} corps coming: one block of its line struck waits, and the corps' nearest block goes to it`, () => {
      const b = new LandBattle(withJoiners(sampleTicket('plains', 1.1, { seed: 6 }), { [side]: [{ joined: 0.6 }] }));
      b.ai.att = b.ai.def = true; b.relief = true;
      let named: LandUnit | null = null, sent = false;
      for (let k = 0; !b.over && k < 30000; k++) {
        b.step();
        const w = b.units.filter((u) => u.waited);
        expect(w.length).toBeLessThanOrEqual(1);
        if (!named && w.length) {
          named = w[0];
          expect(named.side).toBe(side);
          expect(named.engaged).toBeDefined();
          expect(named.joined).toBeUndefined();
        }
        if (named?.waits && !sent) {
          const foe = named.engaged;
          sent = b.units.some((r) => r.corps === (side === 'att' ? 2001 : 2101) && r.order && ((r.order.kind === 'attack' && r.order.target === foe) || (r.order.kind === 'move' && Math.hypot(r.order.x - named!.x, r.order.y - named!.y) <= LAND.alone.near)));
        }
      }
      expect(named).not.toBeNull();
      expect(sent).toBe(true);
    }, 120_000);
  }
});

describe('(7b) a waiting block is never its staff\'s: not sent as help, not set to fire on cover', () => {
  it('the relief sent to a waiting block is never a block that is itself waiting', () => {
    const b = new LandBattle(withJoiners(sampleTicket('plains', 1.0, { seed: 2, humanAtt: true }), { att: [{ joined: 0.95 }, { joined: 0.95, toward: Math.PI / 2 }] }));
    b.ai.att = b.ai.def = false; b.relief = true;
    steps(b, 2);
    const corps = [...new Set(b.units.filter((x) => x.side === 'att' && x.joined !== undefined).map((x) => x.corps))].sort();
    expect(corps.length).toBe(2);
    const [B, C] = corps;
    const a = b.units.filter((x) => x.side === 'att' && x.joined === undefined && b.waitWhy(x) === null).sort((p, q) => p.x - q.x)[0];
    const bs = b.units.filter((x) => x.corps === B && x.kind !== 'art');
    expect(bs.length).toBeGreaterThanOrEqual(2);
    // A waits for corps B; B's nearest block to A waits itself, for corps C, 300 m off A (not beside it).
    a.waits = { corps: B, due: b.t + 600, at: [a.x, a.y], since: b.t }; a.waited = true;
    const r1 = bs[0];
    r1.x = a.x + Math.max(a.w, a.d) / 2 + r1.w / 2 + 300; r1.y = a.y; r1.order = null;
    for (const o of bs.slice(1)) { o.x = a.x + 2000; o.y = a.y; o.order = null; }
    r1.waits = { corps: C, due: b.t + 600, at: [r1.x, r1.y], since: b.t }; r1.waited = true;
    inside(b).waitedFor.add(B); inside(b).waitedFor.add(C);
    b.ai.att = true;
    thinkFor(b, 'att');
    expect(r1.order).toBeNull();
    // Another block of corps B goes to A instead.
    expect(bs.slice(1).some((o) => o.order && (o.order.kind === 'move' ? Math.hypot(o.order.x - a.x, o.order.y - a.y) <= LAND.alone.near : o.order.kind === 'attack'))).toBe(true);
  });
  it('an ambushed attack sweeps the cover ahead with every block but one that waits for help', () => {
    const make = (): { b: LandBattle; a: LandUnit } => {
      const b = new LandBattle(withJoiners(withTeams(sampleTicket('plains', 1.3, { seed: 5, humanAtt: true })), { att: [{ joined: 0.5 }] }));
      b.ai.att = b.ai.def = false; b.relief = true;
      const a = b.units.filter((x) => x.side === 'att' && x.kind === 'inf' && x.joined === undefined && b.waitWhy(x) === null).sort((p, q) => p.x - q.x || p.id - q.id)[0];
      expect(b.hedgesOn([{ kind: 'hedgerow', points: [[a.x - 300, a.y + 120], [a.x + 300, a.y + 120]] }])).toBe(true);
      b.hideOn();
      return { b, a };
    };
    const off = make(), on = make();
    on.b.command({ do: 'land:wait', units: [on.a.id] }, 'att');
    expect(on.a.waits).toBeDefined();
    for (const { b } of [off, on]) {
      b.ambushed.add('att');
      for (const u of b.units) if (u.side === 'att') { u.engaged = undefined; u.order = null; }
      b.ai.att = true;
      thinkFor(b, 'att');
    }
    expect(off.a.search).toBeDefined();
    expect(on.a.search).toBeUndefined();
  });
});

// ---- (8) fog, and what the 3D game is sent ------------------------------------------------------------------------

describe('(8) the player\'s own only: the enemy\'s waiting blocks give nothing away', () => {
  it('the bridge sends waits for his blocks only', () => {
    const b = quiet();
    const u = mayWait(b, 'att')[0];
    b.command({ do: 'land:wait', units: [u.id] }, 'att');
    expect(unitOut(u, b.t, 'att').waits).toEqual({ due: u.waits!.due });
    expect(unitOut(u, b.t, 'def').waits).toBeUndefined();
    expect(unitOut(u, b.t).waits).toBeUndefined();
    u.waits!.late = true;
    expect(unitOut(u, b.t, 'att').waits).toEqual({ due: u.waits!.due, late: true });
    // Re-ordered once late: no longer dug in, still let down (it breaks at the waver mark), and the 3D game is told so.
    u.waits!.free = true;
    expect(unitOut(u, b.t, 'att').waits).toEqual({ due: u.waits!.due, late: true, free: true });
    expect(unitOut(u, b.t, 'def').waits).toBeUndefined();
  });
});

// ---- (9) the rider ahead -------------------------------------------------------------------------------------------

/** The player's attack with one corps of its own coming up: the rider on. */
function riding(j: { joined: number; toward?: number; kind?: 'inf' | 'mot' | 'arm' }, seed = 3, staff = true): LandBattle {
  const b = new LandBattle(withJoiners(sampleTicket('plains', 1.1, { seed, humanAtt: true }), { att: [{ joined: j.joined, toward: j.toward, kind: j.kind }] }));
  b.ai.att = staff; b.ai.def = true; b.rider = true;
  return b;
}

describe('(9) the rider ahead: from his corps\' edge to the middle of his side, ahead of his corps, the player\'s own only', () => {
  it('never in the view with the switch off, at second 0, in the setup step, or once the day is over', () => {
    const b = riding({ joined: 0.5 });
    expect(b.view().riders).toBeUndefined();
    b.rider = false; b.step();
    expect(b.view().riders).toBeUndefined();
    const c = riding({ joined: 0.5 });
    c.setupOn('att', true);
    c.step();
    expect(c.view().riders).toBeUndefined();
    c.command({ do: 'land:begin' }, 'att');
    c.step();
    expect(c.view().riders?.length).toBe(1);
    while (!c.over) c.step();
    expect(c.view().riders).toBeUndefined();
  });
  it('rides straight in at the field\'s fastest pace, stands at the middle of his side, and goes when his corps comes up', () => {
    let some = 0;
    for (const seed of [3, 7, 11]) for (const joined of [0.6, 0.4, 0.2]) for (const toward of [undefined, Math.PI / 2]) for (const kind of ['inf', 'mot', 'arm'] as const) {
      const b = riding({ joined, toward, kind }, seed);
      const c = b.view().coming!.filter((x) => x.corps === 2001);
      const at = c[0].at, from = [c.reduce((a, x) => a + x.x, 0) / c.length, c.reduce((a, x) => a + x.y, 0) / c.length];
      const own = b.units.filter((u) => u.side === 'att'), mid = [own.reduce((a, u) => a + u.x, 0) / own.length, own.reduce((a, u) => a + u.y, 0) / own.length];
      const fast = Math.max(...Object.values(LAND.speed));
      b.step();
      const r = b.view().riders?.[0];
      if (!r) {
        // None where his corps would beat him, or comes on as the clock starts.
        expect(at === 0 || Math.hypot(from[0] - mid[0], from[1] - mid[1]) / fast > at - 30).toBe(true);
        continue;
      }
      some++;
      expect(r.comes).toBeCloseTo(at, 3);
      expect(r.there).toBeLessThanOrEqual(r.comes);
      expect(r.speed).toBe(fast);
      expect(r.line).toBeCloseTo((1 - joined) * b.ticket.limit, 3);
      let gone = false;
      for (let k = 0; !b.over && k < 30000; k++) {
        b.step();
        const q = b.view().riders?.[0];
        const still = (b.view().coming ?? []).some((x) => x.corps === 2001);
        if (gone) { expect(q).toBeUndefined(); continue; }
        if (!q) { expect(still).toBe(false); gone = true; continue; }
        expect(still).toBe(true);
        expect(q.speed).toBe(b.t < q.there ? fast : 0);
        expect(groundAt(b.field, q.x, q.y) === 'water' && b.t >= q.there).toBe(false);
      }
      expect(gone).toBe(true);
    }
    expect(some).toBeGreaterThan(10);
  }, 300_000);
  it('the player\'s own only, fog on or off; none with nobody playing', () => {
    const t = withJoiners(sampleTicket('plains', 1.2, { seed: 8, humanAtt: true }), { def: [{ joined: 0.3 }] });
    for (const fog of [false, true]) {
      const b = new LandBattle(t); b.rider = true; b.fog = fog;
      for (let k = 0; k < 300 && !b.over; k++) { b.step(); expect(b.view().riders).toBeUndefined(); }
    }
    const n = new LandBattle(withJoiners(sampleTicket('plains', 1.2, { seed: 8 }), { att: [{ joined: 0.3 }] }));
    n.rider = true;
    for (let k = 0; k < 300 && !n.over; k++) { n.step(); expect(n.view().riders).toBeUndefined(); }
    const d = new LandBattle(t); d.ticket.def.human = true; d.ticket.att.human = false; d.rider = true; d.fog = true;
    d.step();
    expect(d.view().riders?.[0]?.side).toBe('def');
  });
  it('stands on dry ground, clear of every block, where the middle of his side is the river', () => {
    for (const town of ['Kiev', 'Warsaw', 'Budapest']) {
      const t0 = sampleTicket('urban', 1.0, { seed: 4, town: 'city', humanAtt: true });
      const t = withJoiners({ ...t0, place: { ...t0.place, town, stream: streamFor(town, 0) } }, { att: [{ joined: 0.3 }] });
      const b = new LandBattle(t); b.rider = true;
      b.step();
      const r = b.view().riders?.[0];
      if (!r) continue;
      const own = b.units.filter((u) => u.side === 'att');
      while (!b.over && (b.view().riders?.[0]?.speed ?? 0) > 0) b.step();
      const s = b.view().riders?.[0];
      if (!s) continue;
      expect(['water', 'blown', 'bridge']).not.toContain(groundAt(b.field, s.x, s.y));
      for (const u of own) if (u.state !== 'gone') expect(Math.abs(u.x - s.x) >= u.w / 2 || Math.abs(u.y - s.y) >= u.d / 2).toBe(true);
    }
  });
  it('his standing spot is worked out again once the setup step is done: never inside a block placed there', () => {
    const b = riding({ joined: 0.5 });
    expect(b.setupOn('att', true)).toBe(true);
    const r = inside(b).riders[0];
    expect(r).toBeDefined();
    const to0: [number, number] = [...r.to];
    // A block of his placed over where the rider would have stood.
    let put: [LandUnit, number, number] | null = null;
    for (const x of b.units) {
      if (put || x.side !== 'att' || x.joined !== undefined) continue;
      for (let dx = -x.w / 2 + 2; dx <= x.w / 2 - 2 && !put; dx += 4) for (let dy = -x.d / 2 + 2; dy <= x.d / 2 - 2 && !put; dy += 4) {
        if (b.placeWhy(x, to0[0] + dx, to0[1] + dy) === null) put = [x, to0[0] + dx, to0[1] + dy];
      }
    }
    expect(put).not.toBeNull();
    const [u, px, py] = put!;
    expect(b.command({ do: 'land:place', units: [u.id], x: px, y: py }, 'att')).toBe(true);
    expect(Math.abs(u.x - to0[0]) < u.w / 2 && Math.abs(u.y - to0[1]) < u.d / 2).toBe(true);
    expect(b.command({ do: 'land:begin' }, 'att')).toBe(true);
    const to = inside(b).riders[0].to, c = LAND.placeClear;
    for (const v of b.units) if (v.state !== 'gone') expect(Math.abs(v.x - to[0]) >= v.w / 2 + c || Math.abs(v.y - to[1]) >= v.d / 2 + c).toBe(true);
    expect(to).not.toEqual(to0);
  });
  it('his news: the corps, its divisions, its men and strength, when and where it comes up, and the line only where its staff fights it', () => {
    const b = riding({ joined: 0.5 });
    b.step();
    const r = b.view().riders![0];
    const mmss = (t: number): string => `${Math.floor(t / 60)}:${String(Math.floor(t % 60)).padStart(2, '0')}`;
    expect(r.says).toBe(`Joining Corps 1: 2 infantry divisions (41st Infantry and 42nd Infantry), 27,000 men, strength 90. It comes up from behind your line at ${mmss(r.comes)} and reaches your line at about ${mmss(r.line!)}.`);
    expect([r.name, r.kind, r.divs, r.men, r.str]).toEqual(['Joining Corps 1', 'inf', ['41st Infantry', '42nd Infantry'], 27000, 0.9]);
    const c = riding({ joined: 0.5 }, 3, false);
    c.step();
    const q = c.view().riders![0];
    expect(q.line).toBeUndefined();
    expect(q.says).toMatch(/It comes up from behind your line at \d+:\d\d, and waits there for your orders\.$/);
  });
});

// ---- (10) the measure (LONG=1) --------------------------------------------------------------------------------------

/**
 * The field against the card with the relief on (ruling 217, b418; the design director's measure, 13:43Z): section 9 of
 * the join test's rows (each side's joiner, from behind and from a flank, early, midway and late; 40 seeds at odds 1.0
 * and 1.4) on every kind of ground and a river, each fought four ways: the relief off, every staff using it, and two
 * players' scripts on the joiner's side: digging in where cover helps most and lifting the wait just before help is due
 * (hold then lift), and digging in but never sending the relief. Each row's attack win rate against the card fought
 * whole: within a tenth for the staffs, and within a tenth in the player's favour for his scripts. RELIEF_GROUND runs
 * one ground (a measure split over machines).
 */
describe('(10) the field still lands near the card with the relief on (a measure: LONG=1)', () => {
  const LONG = !!env?.env.LONG;
  const GROUNDS: [string, (seed: number, odds: number) => T][] = [
    ['plains', (seed, odds) => sampleTicket('plains', odds, { seed })],
    ['forest', (seed, odds) => sampleTicket('forest', odds, { seed })],
    ['hills', (seed, odds) => sampleTicket('hills', odds, { seed })],
    ['city', (seed, odds) => sampleTicket('urban', odds, { seed, town: 'city' })],
    ['river', (seed, odds) => sampleTicket('plains', odds, { seed, river: true })],
  ];
  const only = env?.env.RELIEF_GROUND;
  // off: the relief switch off, both staffs fighting; staff: on, both staffs (they order waits and send the relief);
  // lift: on, the joiner's side also holding a block by its own order and lifting it just before help is due; alone: on,
  // the same hold with the joining corps taken in hand and never sent; alone0: that last with the switch off (no hold),
  // so what the hold itself does to a player who never sends his relief stands apart from what never sending it does.
  type Way = 'off' | 'staff' | 'lift' | 'alone' | 'alone0';
  /** The player's script on the joiner's side: which block waits, and what he does after. */
  const script = (way: Way, side: Side): ((b: LandBattle) => void) | undefined => {
    if (way === 'off' || way === 'staff') return undefined;
    // (alone0: the switch is off, so his wait order is refused and only the taking in hand is left.)
    return (b) => {
      // Before his staff thinks: the block in a fight on the most open ground (the cover helps it most) digs in.
      const may = b.units.filter((u) => u.side === side && u.engaged !== undefined && b.waitWhy(u) === null);
      if (may.length) {
        const best = may.reduce((m, u) => (LAND.cover[coverAt(b.field, u.x, u.y)] > LAND.cover[coverAt(b.field, m.x, m.y)] ? u : m));
        b.command({ do: 'land:wait', units: [best.id] }, side);
      }
      for (const u of b.units) {
        if (u.side !== side || !u.waits || u.waits.late || u.waits.free) continue;
        // Hold then lift: just before help is due, stand where he is by his own order, with no mark.
        if (way === 'lift' && b.t >= u.waits.due - 1) b.command({ do: 'move', units: [u.id], x: u.x, y: u.y }, side);
      }
      // Never sending the relief: the joining corps stands where it came on (he has it in hand and gives it nothing).
      if (way === 'alone' || way === 'alone0') for (const u of b.units) if (u.side === side && u.joined !== undefined) u.taken = true;
    };
  };
  const run = (b: LandBattle, each?: (b: LandBattle) => void): LandBattle => { for (let k = 0; !b.over && k < 30000; k++) { each?.(b); b.step(); } return b; };
  // The bar (the measure as put to the design director, 13:43Z: paired with the relief off, the tenth): what the hold
  // itself moves, each way against the same battles with the switch off. How far the joiners' field sits from the card
  // fought whole (off against whole) is part (b)'s own (tests/battle-land-join.test.ts), printed here, never a bar of this.
  it.runIf(LONG)('every ground, each row, each way: the hold moves the win rate within a tenth of the same battles with it off', () => {
    const rows: string[] = [], bad: string[] = [];
    for (const [ground, make] of GROUNDS) {
      if (only && only !== ground) continue;
      const whole = new Map<string, number>();
      for (let seed = 1; seed <= 40; seed++) for (const odds of [1.0, 1.4]) { const b = new LandBattle(make(seed, odds)); b.ai.att = b.ai.def = true; whole.set(`${seed}:${odds}`, run(b).over!.R); }
      for (const [from, toward] of [['its back edge', undefined], ['the east flank', Math.PI / 2]] as [string, number | undefined][]) {
        for (const side of ['att', 'def'] as Side[]) for (const joined of [0.75, 0.5, 0.25]) {
          const wins: Record<Way, number> = { off: 0, staff: 0, lift: 0, alone: 0, alone0: 0 }, lr: Record<Way, number> = { off: 0, staff: 0, lift: 0, alone: 0, alone0: 0 };
          let w0 = 0, n = 0, waited = 0, late = 0;
          for (let seed = 1; seed <= 40; seed++) for (const odds of [1.0, 1.4]) {
            const r0 = whole.get(`${seed}:${odds}`)!;
            w0 += r0 >= 1.2 ? 1 : 0; n++;
            for (const way of ['off', 'staff', 'lift', 'alone', 'alone0'] as Way[]) {
              const b = new LandBattle(withJoiners(make(seed, odds), { [side]: [{ joined, part: 0.15, toward }] }));
              b.ai.att = b.ai.def = true; b.relief = way !== 'off' && way !== 'alone0';
              const lateIds = new Set<number>();
              run(b, (x) => { script(way, side)?.(x); if (way === 'staff') for (const u of x.units) if (u.waits?.late) lateIds.add(u.id); });
              wins[way] += b.over!.R >= 1.2 ? 1 : 0;
              lr[way] += Math.log(Math.max(1e-6, b.over!.R) / Math.max(1e-6, r0));
              if (way === 'staff') { waited += b.units.filter((u) => u.waited).length; late += lateIds.size; }
            }
          }
          const gap = (w: Way): number => (wins[w] - w0) / n;
          const favour = (w: Way): number => (side === 'att' ? gap(w) : -gap(w));
          const hold = { staff: gap('staff') - gap('off'), lift: favour('lift') - favour('off'), alone: favour('alone') - favour('alone0') };
          const row = `${ground.padEnd(6)} ${side === 'att' ? 'attack ' : 'defence'} joiner from ${from}, ${joined}: whole ${w0}/${n}; off ${wins.off}, staff ${wins.staff}, lift ${wins.lift}, alone ${wins.alone}, alone0 ${wins.alone0}` +
            ` | hold: staff ${hold.staff.toFixed(3)}, lift's favour ${hold.lift.toFixed(3)}, alone's favour ${hold.alone.toFixed(3)} | part (b) off-whole ${gap('off').toFixed(3)}` +
            `; R ratio staff/off ${Math.exp((lr.staff - lr.off) / n).toFixed(3)}; staff waited ${waited}, late ${late}`;
          rows.push(row);
          if (Math.abs(hold.staff) > 0.1 || hold.lift > 0.1 || hold.alone > 0.1) bad.push(row);
        }
      }
      env?.stdout?.write(`\n${rows.filter((r) => r.startsWith(ground)).join('\n')}\n`);
    }
    expect(bad, bad.join('\n')).toEqual([]);
  }, 7_200_000);
});
