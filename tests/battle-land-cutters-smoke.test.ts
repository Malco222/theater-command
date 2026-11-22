// Hedge cutters and smoke on call (release 2; Brian, 22 November 14:19Z: "Let's do 1 and 2": bladed Shermans punch a
// gap through a hedgerow where the player picks; a section lays a smoke line to cover a crossing, when and where the
// player orders it). The design director's ruling 242, as amended 17:45Z from the historian: cutters (C1-A, C2-A, C3,
// C4-A) are looks only, every army's tanks from July 1944 on a hedged field, the player's where he orders and his staff's
// wherever their way crosses a hedge with no gap, a gap 3.2 m wide open 6 battle seconds after the cut begins; smoke
// (S1-A to S6) halves a direct blow across a live screen (never on top of the vague share; fire from far back untouched),
// one screen a block at a time, the army's count a battle, laid within its army's reach of its front and at most its
// front long, both staffs alike as their foot goes in, the drill drawing each screen as its rounds in place of its own
// salvoes. Off, nothing of either is read: the battle of 3bda466e to the last number. The bocage (C1-B, built as the
// 17:45Z amendment asks: "unfitted tanks cross only at gaps"), under its own switch: a tank block without cutters crosses
// a hedge only at a gap, a gateway or a cut once open; measured against off (LONG=1).
import { describe, expect, it } from 'vitest';
import type { FieldEvent, FieldTicket, Side } from '../src/battle/core';
import { LAND, LandBattle, PRACTICE_GROUNDS, fieldHedges, kitchenTicket, practiceHedges, practiceTicket, sampleTicket, smokeMeans, withTeams, type Cut, type LandPlace, type LandUnit, type Screen } from '../src/battle/land';
import { newCampaign, offers } from '../src/battle/ui/campaign';
import { orderAttack } from '../src/campaign/turn';
import { Rng, hashString } from '../src/core/rng';
import { battleCalls, unitOut } from '../unreal/TheaterCommand/Rules/battle-bridge';

type T = FieldTicket<LandPlace>;
const env = (globalThis as { process?: { env: Record<string, string | undefined>; stdout?: { write(s: string): void } } }).process;

const live = (u: LandUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';

/**
 * Everything a battle shows and does, every tick (the relief test's own, with the dice's place, the gates and every
 * key a block carries), less the view keys in 'drop', the events in 'skip' and the block keys in 'hide'. Gates a cut
 * opened are left out: only the drill reads gates.
 */
function signed(b: LandBattle, before?: (b: LandBattle, k: number) => void, o: { drop?: string[]; skip?: string[]; hide?: string[] } = {}): string {
  const drop = o.drop ?? [], skip = o.skip ?? [], hide = o.hide ?? [];
  const ev: FieldEvent[] = [];
  const keys = new Set<string>();
  for (let k = 0; !b.over && k < 30000; k++) {
    before?.(b, k);
    b.step();
    const v = b.view();
    for (const x of Object.keys(v)) if (!drop.includes(x)) keys.add(x);
    ev.push(...v.events.filter((e) => !skip.includes(e.what)));
  }
  return JSON.stringify({
    over: b.over, t: b.t, keys: [...keys].sort(), scale: b.scale(), page: b.scale(undefined, LAND.scale.page),
    rng: inside(b).rng.state, gates: b.gates.filter((g) => g.kind !== 'cut'),
    units: b.units.map((u) => [u.id, u.corps, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged, u.reserve ?? null, u.power0,
      u.home, u.dry ?? null, u.order, u.path, u.cal ?? null, u.crew ?? null, u.taken ?? null, Object.keys(u).filter((k) => !hide.includes(k)).sort().join(',')]),
    wrecks: b.wrecks.map((z) => [z.x, z.y, z.at]),
    said: ev.map((e) => [e.t, e.what, e.unit ?? null, e.text]),
    log: b.log.filter((e) => !skip.includes(e.what)).map((e) => [e.t, e.what, e.unit ?? null, e.text]),
  });
}
const digest = (s: string): string => hashString(s).toString(16).padStart(8, '0') + hashString('#' + s).toString(16).padStart(8, '0');

/** The rules' private workings a test reads. */
interface Inside {
  rng: Rng;
  blow(u: LandUnit, t: LandUnit): { dmg: number };
  spotHidden(): void;
  cutBegin(u: LandUnit, x: number, y: number, yaw: number): void;
}
const inside = (b: LandBattle): Inside => b as unknown as Inside;
const steps = (b: LandBattle, n: number): void => { for (let k = 0; k < n && !b.over; k++) b.step(); };

/**
 * The hedged practice field (the pins below were fought on 3bda466e with these very hedges): an American attack on a
 * German defence in August 1944, the player's the attack with his staff fighting every block he has not taken in hand,
 * three hedge lines with two gateways each across the middle of the field.
 */
const hedgedTicket = (): T => ({ ...sampleTicket('plains', 1.1, { seed: 3, humanAtt: true, nat: { att: 'USA', def: 'GER' } }), turn: 60 });
function field(): LandBattle {
  const b = new LandBattle(hedgedTicket());
  b.ai.att = b.ai.def = true; b.perUnit = true;
  const H = practiceHedges(b.field.w, b.field.h);
  b.hedgesOn(H.hedges, H.gates);
  return b;
}
const hedgeY = (b: LandBattle): number => Math.round(b.field.h * 0.42);
const tankOf = (b: LandBattle, side: Side = 'att'): LandUnit => b.units.find((u) => u.side === side && u.kind === 'arm' && live(u))!;
const footOf = (b: LandBattle, side: Side = 'att'): LandUnit => b.units.find((u) => u.side === side && u.kind === 'inf' && live(u))!;
/** The cut and smoke orders the off pins were fought with (refused there, as an unknown order is). */
const offOrders = (said: boolean[]) => (b: LandBattle, k: number): void => {
  if (k !== 0 && k !== 600) return;
  const tank = b.units.find((u) => u.side === 'att' && u.kind === 'arm' && live(u)), foot = b.units.find((u) => u.side === 'att' && u.kind === 'inf' && live(u));
  if (tank) said.push(b.command({ do: 'land:cut', units: [tank.id], x: tank.x, y: hedgeY(b) }, 'att'));
  if (foot) said.push(b.command({ do: 'land:smoke', units: [foot.id], x0: foot.x - 80, y0: foot.y + 100, x1: foot.x + 80, y1: foot.y + 100 }, 'att'));
};
/** Fought on 3bda466e (scratch pin.test.ts, the same signed() and hedges): [R, seconds, digest]; the drill's rounds [count, smoke, digest]. */
const OFF_PIN: [number, number, string] = [1.084, 233, '1ce517f19b02d822'];
const OFF_DRILL: [number, number, string] = [69283, 30, 'd7185ed7412bad04'];

/** Two foot blocks squared up 200 m apart across open ground, nobody's staff thinking, the smoke switch on. */
function faced(nat: { att?: string; def?: string } = { att: 'USA', def: 'GER' }, teams = false): { b: LandBattle; u: LandUnit; t: LandUnit } {
  const base = sampleTicket('plains', 1.0, { seed: 2, nat });
  const b = new LandBattle(teams ? withTeams(base) : base);
  b.ai.att = b.ai.def = false; b.smoke = true;
  const u = footOf(b, 'att'), t = footOf(b, 'def');
  u.x = 1500; u.y = 900; u.face = 0; t.x = 1500; t.y = 1100; t.face = Math.PI;
  return { b, u, t };
}
/** A screen laid by hand across the field from (x0, y0) to (x1, y1), standing 30 s from now. */
const screenAt = (b: LandBattle, by: LandUnit, x0: number, y0: number, x1: number, y1: number, hang = 30): Screen => {
  const s: Screen = { id: b.screens.length + 1, side: by.side, unit: by.id, x0, y0, x1, y1, t: b.t, until: b.t + hang };
  b.screens.push(s);
  return s;
};
/** A blow's weight with the dice fixed (the same luck each time). */
const weigh = (b: LandBattle, u: LandUnit, t: LandUnit): number => { inside(b).rng = new Rng(9); return inside(b).blow(u, t).dmg; };

// ---- (1) both off: the battle of 3bda466e -------------------------------------------------------------------------

describe('(1) both switches off: the battle of 3bda466e, the orders refused as an unknown order is', () => {
  it('a hedged practice field with cut and smoke orders given while off: the pin of 3bda466e, and nothing of either on any block', () => {
    const said: boolean[] = [];
    const b = field();
    const s = signed(b, offOrders(said));
    expect([Math.round(b.over!.R * 1000) / 1000, b.over!.seconds, digest(s)]).toEqual(OFF_PIN);
    expect(said.length).toBeGreaterThan(0);
    expect(said.every((x) => x === false)).toBe(true);
    expect(b.cuts.length + b.screens.length).toBe(0);
    expect(b.allBlocks().some((u) => u.cut !== undefined || u.smokeLeft !== undefined)).toBe(false);
    expect(b.log.some((e) => e.what === 'land:cut' || e.what === 'land:smoke')).toBe(false);
    expect(Object.keys(b.view())).not.toContain('cuts');
    expect(Object.keys(b.view())).not.toContain('screens');
    expect(b.cutterOf(tankOf(b))).toBe(false);
  }, 300_000);
});

// ---- (2) cutters on: the same fight -------------------------------------------------------------------------------

describe('(2) hedge cutters on: the fight is the same, and the gaps are cut', () => {
  // On: his tank block told to cut through the first hedge line straight ahead of it; his staff's and the computer's
  // tanks cut where their way crosses one. Off: the same block sent by a plain march where the cut order sends it.
  const on = (() => {
    const b = field();
    b.cutters = true;
    let ordered = -1, ok = false, at: [number, number] = [0, 0];
    const seen: { t: number; kind: string | null; t1: number }[] = [];
    const s = signed(b, (x, k) => {
      if (k === 0) { const u = tankOf(x); ordered = u.id; at = [u.x, hedgeY(x)]; ok = x.command({ do: 'land:cut', units: [u.id], x: u.x, y: hedgeY(x) }, 'att'); }
      // A drawn man's straight way across the hedge at the ordered cut: the gate the drill sends him through (gateOn).
      const c = x.cuts.find((q) => q.unit === ordered);
      if (c) seen.push({ t: x.t, kind: x.gateOn(c.x, c.y - 20, c.x, c.y + 20)?.kind ?? null, t1: c.t1 });
    }, { drop: ['cuts'], skip: ['land:cut'], hide: ['cut'] });
    return { b, s, ordered, ok, at, seen };
  })();
  const off = (() => {
    const b = field();
    const s = signed(b, (x, k) => {
      if (k !== 0) return;
      // Where cutGo sends it: the hedge point, then its half depth and LAND.cutters.near beyond, the same sums.
      const u = tankOf(x), p = x.hedgeNear(u.x, hedgeY(x), LAND.cutters.near)!;
      const n = Math.hypot(p.x - u.x, p.y - u.y), ux = (p.x - u.x) / n, uy = (p.y - u.y) / n, d = u.d / 2 + LAND.cutters.near;
      x.command({ do: 'move', units: [u.id], x: p.x + ux * d, y: p.y + uy * d }, 'att');
    }, { drop: ['cuts'], skip: ['land:cut'], hide: ['cut'] });
    return { b, s };
  })();

  it('the same units, strengths, result and place of the dice: only the cuts, their events and their gates differ', () => {
    expect(on.ok).toBe(true);
    expect(on.s).toBe(off.s);
    expect(off.b.cuts.length).toBe(0);
  });
  it('his ordered cut where he put it, and his staff\'s and the enemy\'s wherever their tanks crossed a hedge with no gap', () => {
    const b = on.b, mine = b.cuts.filter((c) => c.unit === on.ordered);
    expect(mine.length).toBe(1);
    expect(mine[0].x).toBeCloseTo(on.at[0], 0);
    expect(mine[0].y).toBe(on.at[1]);
    expect(mine[0].yaw).toBe(0);
    const staff = b.cuts.filter((c) => c.unit !== on.ordered);
    expect(staff.length).toBeGreaterThan(0);
    for (const c of staff) { const u = b.unit(c.unit)!; expect(u.kind).toBe('arm'); expect(u.taken).toBeFalsy(); }
    for (const c of b.cuts) {
      expect(c.t1).toBeCloseTo(c.t0 + LAND.cutters.secs, 6);
      expect(c.width).toBe(LAND.cutters.width);
      expect(c.open).toBe(true);
      expect(b.gates.some((g) => g.kind === 'cut' && g.x === c.x && g.y === c.y && g.width === c.width)).toBe(true);
    }
    // No two cuts within a gap's width of each other, nor of a gateway.
    for (const c of staff) expect(b.gates.filter((g) => g.kind !== 'cut').every((g) => Math.hypot(g.x - c.x, g.y - c.y) > LAND.cutters.width)).toBe(true);
    const told = b.log.filter((e) => e.what === 'land:cut');
    expect(told.length).toBe(b.cuts.length);
    expect(told.find((e) => e.unit === on.ordered)!.text).toMatch(/cuts a gap through the hedge\.$/);
    expect((told[0].extra as { t1: number }).t1).toBe(b.cuts[0].t1);
  });
  it('the gap is a gate of kind cut from t1, and the drawn men go through it (gateOn, the drill\'s way through a hedge)', () => {
    const before = on.seen.filter((x) => x.t < x.t1 - 1e-9), after = on.seen.filter((x) => x.t >= x.t1 - 1e-9);
    expect(before.length).toBeGreaterThan(0);
    expect(after.length).toBeGreaterThan(0);
    expect(before.every((x) => x.kind !== 'cut')).toBe(true);
    expect(after.every((x) => x.kind === 'cut')).toBe(true);
  });
  it('the view lists the cuts only with the switch on', () => {
    expect(on.b.cutView('att').cuts!.length).toBe(on.b.cuts.length);
    expect(Object.keys(on.b.view())).toContain('cuts');
    expect(Object.keys(off.b.view())).not.toContain('cuts');
  });
});

describe('(2b) cut orders: a group keeps its shape along the hedge, one gap is never cut twice, a shove is no way of its own', () => {
  /** His two tank blocks on the hedged field, nobody else near, the switch on; nobody's staff thinking. */
  const two = (): { b: LandBattle; a: LandUnit; c: LandUnit; y: number } => {
    const b = field();
    b.cutters = true; b.ai.att = b.ai.def = false;
    const y = hedgeY(b), tanks = b.units.filter((u) => u.side === 'att' && u.kind === 'arm');
    const [a, c] = tanks;
    for (const x of b.units) if (x !== a && x !== c) { x.x = x.side === 'att' ? 60 + x.id * 10 : b.field.w - 60 - x.id * 10; x.y = x.side === 'att' ? 60 : b.field.h - 60; x.order = null; x.path = []; }
    a.x = 1000; c.x = 2000; a.y = c.y = y - 150; a.face = c.face = 0;
    return { b, a, c, y };
  };
  it('two blocks told to cut at one point: each cuts its own gap at its place in the group, as a move keeps its shape', () => {
    const { b, a, c, y } = two();
    expect(b.command({ do: 'land:cut', units: [a.id, c.id], x: 1500, y }, 'att')).toBe(true);
    steps(b, 400);
    const cuts = b.cuts.map((q) => [q.unit, Math.round(q.x), q.y]).sort((p, q) => p[1] - q[1]);
    expect(cuts).toEqual([[a.id, 1000, y], [c.id, 2000, y]]);
  });
  it('a block sent to cut where a gap stands already (a gateway) goes through it: no cut, no event, no second gate', () => {
    const { b, a, y } = two();
    const g = b.gates.find((q) => Math.abs(q.y - y) < 1e-6)!;
    a.x = g.x;
    const gates = b.gates.length;
    expect(b.command({ do: 'land:cut', units: [a.id], x: g.x, y }, 'att')).toBe(true);
    steps(b, 400);
    expect(a.y).toBeGreaterThan(y);
    expect(b.cuts.length).toBe(0);
    expect(b.log.some((e) => e.what === 'land:cut')).toBe(false);
    expect(b.gates.length).toBe(gates);
  });
  it('a staff tank standing still, shoved over a hedge line by a friend easing past it, cuts nothing', () => {
    const { b, a, c, y } = two();
    b.ai.att = true;
    (b as unknown as { nextThink: number }).nextThink = Infinity;
    a.y = y - 0.3; a.speed = 0;
    // A friend on the same ground just behind it: the two are eased apart along their depth, the tank over the hedge.
    c.x = a.x; c.y = a.y - (a.d + c.d) / 2 + 3; c.face = 0; c.speed = 0;
    steps(b, 5);
    expect(a.y).toBeGreaterThan(y);
    expect(a.order).toBeNull();
    expect(b.cuts.length).toBe(0);
  });
});

// ---- (3) why not, in his words --------------------------------------------------------------------------------------

describe('(3) every refusal says why, in the player\'s words (129(d))', () => {
  const hedged = (turn = 60, nat: { att?: string; def?: string } = { att: 'USA', def: 'GER' }, hedges = true): LandBattle => {
    const b = new LandBattle({ ...withTeams(sampleTicket('plains', 1.0, { seed: 2, humanAtt: true, nat })), turn });
    b.ai.att = b.ai.def = false;
    if (hedges) { const H = practiceHedges(b.field.w, b.field.h); b.hedgesOn(H.hedges, H.gates); }
    return b;
  };
  it('the cut', () => {
    const b = hedged(), tank = tankOf(b), foot = footOf(b), y = hedgeY(b);
    expect(b.cutWhy(tank)).toBe('Your tanks carry no hedge cutters.');
    expect(b.command({ do: 'land:cut', units: [tank.id], x: tank.x, y }, 'att')).toBe(false);
    b.cutters = true;
    expect(b.cutWhy(tank)).toBeNull();
    expect(b.cutWhy(tank, tank.x, y)).toBeNull();
    expect(b.cutWhy(tank, tank.x, y + LAND.cutters.near - 0.5)).toBeNull();
    expect(b.cutWhy(tank, tank.x, y + 50)).toBe('There is no hedge there.');
    expect(b.cutWhy(tank, -5, y)).toBe('Off the field.');
    expect(b.cutWhy(foot)).toBe('Only tanks carry hedge cutters.');
    expect(b.cutWhy(b.units.find((u) => u.side === 'att' && u.team === 'mg')!)).toBe('Only tanks carry hedge cutters.');
    tank.state = 'routing';
    expect(b.cutWhy(tank, tank.x, y)).toBe('It is out of the fight.');
    tank.state = 'steady';
    const early = hedged(LAND.cutters.from - 1); early.cutters = true;
    expect(early.cutWhy(tankOf(early))).toBe('Hedge cutters come in July 1944.');
    const bare = hedged(60, undefined, false); bare.cutters = true;
    expect(bare.cutWhy(tankOf(bare))).toBe('There are no hedges on this field.');
    // Refused for every block: no order; for some: only those that may.
    expect(b.command({ do: 'land:cut', units: [foot.id], x: tank.x, y }, 'att')).toBe(false);
    expect(b.command({ do: 'land:cut', units: [tank.id, foot.id], x: tank.x, y }, 'att')).toBe(true);
    expect(tank.cut).toBeDefined(); expect(foot.cut).toBeUndefined();
    // Not his own: the enemy's blocks never take his orders.
    expect(b.command({ do: 'land:cut', units: [tank.id], x: tank.x, y }, 'def')).toBe(false);
    // Any other order calls it off.
    b.command({ do: 'hold', units: [tank.id] }, 'att');
    expect(tank.cut).toBeUndefined();
  });
  it('the smoke', () => {
    const b = hedged(), foot = footOf(b), tank = tankOf(b);
    const [fx, fy] = [Math.sin(foot.face), Math.cos(foot.face)], ahead = (m: number): [number, number] => [foot.x + fx * (foot.d / 2 + m), foot.y + fy * (foot.d / 2 + m)];
    expect(b.smokeWhy(foot)).toBe('There is no smoke to lay in this battle.');
    expect(b.command({ do: 'land:smoke', units: [foot.id], x0: foot.x, y0: foot.y + 100 }, 'att')).toBe(false);
    b.smoke = true;
    expect(b.smokeWhy(foot)).toBeNull();
    expect(b.smokeWhy(foot, ...ahead(190))).toBeNull();
    expect(b.smokeWhy(foot, ...ahead(200))).toBe('They can lay smoke up to 195 m ahead.');
    expect(b.smokeWhy(foot, NaN, 3)).toBe('Off the field.');
    expect(b.smokeWhy(tank)).toBe('Only troops on foot lay smoke.');
    expect(b.smokeWhy(b.units.find((u) => u.side === 'att' && u.team === 'mor')!)).toBe('Only troops on foot lay smoke.');
    expect(b.smokeWhy(b.units.find((u) => u.side === 'att' && u.team === 'mg')!)).toBe('Only troops on foot lay smoke.');
    foot.state = 'routing';
    expect(b.smokeWhy(foot)).toBe('It is out of the fight.');
    foot.state = 'steady';
    expect(b.command({ do: 'land:smoke', units: [foot.id], x0: ahead(100)[0] - 80, y0: ahead(100)[1], x1: ahead(100)[0] + 80, y1: ahead(100)[1] }, 'att')).toBe(true);
    expect(b.smokeWhy(foot)).toBe('Their last screen still stands.');
    b.screens[0].until = b.t;
    expect(b.smokeWhy(foot)).toBeNull();
    foot.smokeLeft = 0;
    expect(b.smokeWhy(foot)).toBe('They have no smoke left.');
    expect(b.command({ do: 'land:smoke', units: [foot.id], x0: foot.x, y0: foot.y + 100 }, 'att')).toBe(false);
    const fr = hedged(60, { att: 'FRA', def: 'GER' }); fr.smoke = true;
    expect(fr.smokeWhy(footOf(fr))).toBe('Your army has no smoke to lay.');
    // Each army's reach in the words.
    for (const [nat, reach] of [['UK', 457], ['GER', 30], ['SOV', 30], ['JAP', 188], ['CAN', 457]] as [string, number][]) {
      const x = hedged(60, { att: nat, def: 'GER' }); x.smoke = true;
      const f = footOf(x);
      expect(x.smokeWhy(f, f.x, f.y + f.d / 2 + reach + 5)).toBe(`They can lay smoke up to ${reach} m ahead.`);
    }
  });
});

// ---- (4) by radio ---------------------------------------------------------------------------------------------------

describe('(4) by radio: the order waits on the air, and is checked again as it lands', () => {
  const radio = (): LandBattle => {
    const b = new LandBattle({ ...sampleTicket('plains', 1.0, { seed: 2, humanAtt: true, nat: { att: 'USA', def: 'GER' } }), turn: 60 });
    b.ai.att = b.ai.def = false; b.radio = true; b.cutters = true; b.smoke = true;
    const H = practiceHedges(b.field.w, b.field.h); b.hedgesOn(H.hedges, H.gates);
    steps(b, 10);
    return b;
  };
  const landed = (b: LandBattle, u: LandUnit): void => { for (let k = 0; k < 400 && u.orderAt !== undefined; k++) b.step(); };
  it('a cut: nothing until it lands, then the block drives for the hedge with the cut pending', () => {
    const b = radio(), u = tankOf(b), y = hedgeY(b);
    expect(b.command({ do: 'land:cut', units: [u.id], x: u.x, y }, 'att')).toBe(true);
    expect(u.orderAt).toBeGreaterThan(b.t);
    expect(u.cut).toBeUndefined();
    landed(b, u);
    expect(u.cut).toEqual({ x: u.cut!.x, y, yaw: 0 });
    expect(u.order?.kind).toBe('move');
    expect(u.order!.y).toBeGreaterThan(y);
  });
  it('a cut that may no longer be made as it lands: the block holds (as wait does); one that broke meanwhile does nothing', () => {
    const b = radio(), u = tankOf(b), y = hedgeY(b);
    b.command({ do: 'land:cut', units: [u.id], x: u.x, y }, 'att');
    u.order = { kind: 'move', x: u.x + 100, y: u.y }; u.path = [[u.x + 100, u.y]];
    b.cutters = false;
    landed(b, u);
    expect(u.order).toBeNull();
    expect(u.cut).toBeUndefined();
    b.cutters = true;
    b.command({ do: 'land:cut', units: [u.id], x: u.x, y }, 'att');
    u.state = 'gone';
    landed(b, u);
    expect(u.cut).toBeUndefined();
  });
  // A smoke order has its own place on the air (4b), and leaves orderAt to the block's move or attack: on the air while
  // the rules hold it pending.
  const onAir = (b: LandBattle, u: LandUnit): boolean => (b as unknown as { pending: { id: number }[] }).pending.some((p) => p.id === u.id);
  const aired = (b: LandBattle, u: LandUnit): void => { for (let k = 0; k < 400 && onAir(b, u); k++) b.step(); };
  it('smoke: laid as it lands, not before; not laid where one of theirs went up meanwhile', () => {
    const b = radio(), u = footOf(b);
    expect(b.command({ do: 'land:smoke', units: [u.id], x0: u.x - 80, y0: u.y + 100, x1: u.x + 80, y1: u.y + 100 }, 'att')).toBe(true);
    expect(b.screens.length).toBe(0);
    expect(onAir(b, u)).toBe(true);
    aired(b, u);
    expect(b.screens.length).toBe(1);
    expect(b.screens[0].t).toBeGreaterThan(0);
    const v = b.units.filter((x) => x.side === 'att' && x.kind === 'inf')[1];
    b.command({ do: 'land:smoke', units: [v.id], x0: v.x - 80, y0: v.y + 100, x1: v.x + 80, y1: v.y + 100 }, 'att');
    screenAt(b, v, v.x - 50, v.y + 60, v.x + 50, v.y + 60);
    aired(b, v);
    expect(onAir(b, v)).toBe(false);
    expect(b.screens.filter((s) => s.unit === v.id).length).toBe(1);
    expect(v.smokeLeft).toBeUndefined();
  });
});

describe('(4b) by radio, smoke has its own place on the air: it and his move or attack both land, in either order', () => {
  const radio = (): LandBattle => {
    const b = new LandBattle({ ...sampleTicket('plains', 1.0, { seed: 2, humanAtt: true, nat: { att: 'USA', def: 'GER' } }), turn: 60 });
    b.ai.att = b.ai.def = false; b.radio = true; b.perUnit = true; b.smoke = true;
    steps(b, 10);
    return b;
  };
  const line = (u: LandUnit): { x0: number; y0: number; x1: number; y1: number } => ({ x0: u.x - 80, y0: u.y + u.d / 2 + 100, x1: u.x + 80, y1: u.y + u.d / 2 + 100 });
  const both = (b: LandBattle, u: LandUnit): void => { for (let k = 0; k < 600 && (u.orderAt !== undefined || !b.screens.some((s) => s.unit === u.id)); k++) b.step(); };
  it('a move, then smoke to cover it: the move lands and the screen is laid', () => {
    const b = radio(), u = footOf(b), y0 = u.y;
    expect(b.command({ do: 'move', units: [u.id], x: u.x, y: u.y + 300 }, 'att')).toBe(true);
    const due = u.orderAt!;
    expect(b.command({ do: 'land:smoke', units: [u.id], ...line(u) }, 'att')).toBe(true);
    // The smoke leaves the move's time on the air alone.
    expect(u.orderAt).toBe(due);
    both(b, u);
    expect(b.screens.filter((s) => s.unit === u.id).length).toBe(1);
    expect(u.order?.kind).toBe('move');
    steps(b, 300);
    expect(u.y - y0).toBeGreaterThan(100);
  });
  it('smoke, then a move: the screen is laid and the move lands', () => {
    const b = radio(), u = footOf(b), y0 = u.y;
    expect(b.command({ do: 'land:smoke', units: [u.id], ...line(u) }, 'att')).toBe(true);
    expect(u.orderAt).toBeUndefined();
    expect(b.command({ do: 'move', units: [u.id], x: u.x, y: u.y + 300 }, 'att')).toBe(true);
    both(b, u);
    expect(b.screens.filter((s) => s.unit === u.id).length).toBe(1);
    expect(u.order?.kind).toBe('move');
    steps(b, 300);
    expect(u.y - y0).toBeGreaterThan(100);
  });
  it('a second smoke order on the air replaces the first (one screen), and a second move replaces the first', () => {
    const b = radio(), u = footOf(b), [x0, y0] = [u.x, u.y];
    b.command({ do: 'land:smoke', units: [u.id], ...line(u) }, 'att');
    b.command({ do: 'move', units: [u.id], x: x0 + 200, y: y0 }, 'att');
    b.command({ do: 'land:smoke', units: [u.id], ...line(u) }, 'att');
    b.command({ do: 'move', units: [u.id], x: x0, y: y0 + 300 }, 'att');
    both(b, u);
    steps(b, 50);
    expect(b.screens.filter((s) => s.unit === u.id).length).toBe(1);
    expect(b.log.filter((e) => e.what === 'land:smoke').length).toBe(1);
    expect(u.order?.kind).toBe('move');
    expect([u.order!.x, u.order!.y]).toEqual([x0, y0 + 300]);
  });
});

// ---- (5) the screen ------------------------------------------------------------------------------------------------

describe('(5) a screen halves a direct blow across it, and nothing else', () => {
  it('across it: half; beside it: whole; either side\'s screen', () => {
    const { b, u, t } = faced();
    const plain = weigh(b, u, t), back = weigh(b, t, u);
    screenAt(b, t, 1300, 1000, 1700, 1000);
    expect(weigh(b, u, t) / plain).toBeCloseTo(LAND.smoke.share, 12);
    expect(weigh(b, t, u) / back).toBeCloseTo(LAND.smoke.share, 12);
    b.screens[0].x0 = 1800; b.screens[0].x1 = 2000;
    expect(weigh(b, u, t)).toBe(plain);
    expect(weigh(b, t, u)).toBe(back);
    expect(LAND.smoke.share).toBe(LAND.hide.locate.fire);
  });
  it('fire from far back is not touched: guns beyond their close range, mortar teams; a machine-gun team and a gun over open sights are', () => {
    const { b, u } = faced({ att: 'USA', def: 'GER' }, true);
    const gun = b.units.find((x) => x.side === 'def' && x.kind === 'art' && !x.team)!, mor = b.units.find((x) => x.side === 'def' && x.team === 'mor')!, mg = b.units.find((x) => x.side === 'def' && x.team === 'mg')!;
    for (const g of [gun, mor, mg]) { g.x = 1500; g.y = 1600; g.face = Math.PI; }
    const far = [gun, mor, mg].map((g) => weigh(b, g, u));
    screenAt(b, u, 1300, 1000, 1700, 1000);
    expect(weigh(b, gun, u)).toBe(far[0]);
    expect(weigh(b, mor, u)).toBe(far[1]);
    expect(weigh(b, mg, u) / far[2]).toBeCloseTo(LAND.smoke.share, 12);
    gun.y = u.y + u.d / 2 + gun.d / 2 + 100;
    b.screens[0].y0 = b.screens[0].y1 = u.y + u.d / 2 + 50;
    expect(b.gap(gun, u)).toBeLessThan(LAND.guns.closeAt);
    const near = (() => { b.screens[0].until = b.t; const w = weigh(b, gun, u); b.screens[0].until = b.t + 30; return w; })();
    expect(weigh(b, gun, u) / near).toBeCloseTo(LAND.smoke.share, 12);
    mor.y = gun.y;
    const m0 = (() => { b.screens[0].until = b.t; const w = weigh(b, mor, u); b.screens[0].until = b.t + 30; return w; })();
    expect(weigh(b, mor, u)).toBe(m0);
  });
  it('never on top of the vague share; gone once it has stood its time', () => {
    const { b, u, t } = faced();
    t.unloc = b.t + 10;
    const vague = weigh(b, u, t);
    const s = screenAt(b, t, 1300, 1000, 1700, 1000);
    expect(weigh(b, u, t)).toBe(vague);
    delete t.unloc;
    const plain = (() => { s.until = b.t; return weigh(b, u, t); })();
    expect(b.screenView('att').screens).toBeUndefined();
    s.until = b.t + 30;
    expect(weigh(b, u, t) / plain).toBeCloseTo(LAND.smoke.share, 12);
  });
  it('never on top of fire on cover at a block still hidden (blind, already fire at a place)', () => {
    const { b, u, t } = faced();
    t.hid = true;
    const blind = weigh(b, u, t);
    screenAt(b, t, 1300, 1000, 1700, 1000);
    expect(weigh(b, u, t)).toBe(blind);
    delete t.hid;
    expect(weigh(b, u, t) / blind).toBeCloseTo(LAND.smoke.share, 12);
  });
  it('the air is not touched: the share held back for air strikes is the same with a screen across the blow or not', () => {
    const run = (screened: boolean): { bank: number; withheld: number; str: number } => {
      const b = new LandBattle(practiceTicket(PRACTICE_GROUNDS.find((p) => p.air)!, { seed: 2, nat: { att: 'USA', def: 'GER' } }));
      b.ai.att = b.ai.def = false; b.smoke = true;
      const u = footOf(b, 'att'), t = footOf(b, 'def');
      for (const x of b.units) if (x !== u && x !== t) { x.x = x.side === 'att' ? 100 : b.field.w - 100; x.y = x.side === 'att' ? 60 : b.field.h - 60; }
      u.x = 1500; u.y = 900; u.face = 0; t.x = 1500; t.y = 1100; t.face = Math.PI;
      if (screened) screenAt(b, u, 1300, 1000, 1700, 1000, 600);
      const s0 = t.str;
      steps(b, 1);
      const A = b as unknown as { airBank: Record<Side, Map<number, number>>; airLedger: Record<Side, { withheld: number }> };
      return { bank: A.airBank.att.get(t.id) ?? 0, withheld: A.airLedger.att.withheld, str: s0 - t.str };
    };
    const open = run(false), smoke = run(true);
    expect(open.withheld).toBeGreaterThan(0);
    expect(smoke.bank).toBeCloseTo(open.bank, 12);
    expect(smoke.withheld).toBeCloseTo(open.withheld, 12);
    // The ground's own part is the part the screen spoils.
    expect(smoke.str).toBeLessThan(open.str * 0.75);
  });
  it('with hiding on, a hidden block is not seen through it', () => {
    const b = new LandBattle(sampleTicket('plains', 1.0, { seed: 2, nat: { att: 'USA', def: 'GER' } }));
    b.ai.att = b.ai.def = false; b.smoke = true;
    const H = practiceHedges(b.field.w, b.field.h); b.hedgesOn(H.hedges, H.gates);
    b.hide = true;
    const h = footOf(b, 'def'), e = footOf(b, 'att'), y = hedgeY(b);
    h.x = 1500; h.y = y; h.face = Math.PI; h.hid = true; h.speed = 0;
    e.x = 1500; e.y = y - (h.d / 2 + e.d / 2 + 40); e.face = 0; e.speed = 0;
    for (const x of b.units) if (x !== h && x !== e) x.y = x.side === 'att' ? 100 : b.field.h - 100;
    const s = screenAt(b, e, 1400, y - h.d / 2 - 20, 1600, y - h.d / 2 - 20);
    inside(b).spotHidden();
    expect(h.hid).toBe(true);
    s.until = b.t;
    inside(b).spotHidden();
    expect(h.hid).toBeUndefined();
  });
  it('one at a time, the army\'s count a battle, its words; laid at once, its hang by army', () => {
    const { b, u } = faced();
    const at = (v: LandUnit, k: number): number[] => [v.x - 80, v.y + v.d / 2 + k, v.x + 80, v.y + v.d / 2 + k];
    for (let i = 0; i < 2; i++) {
      const [x0, y0, x1, y1] = at(u, 100);
      expect(b.command({ do: 'land:smoke', units: [u.id], x0, y0, x1, y1 }, 'att')).toBe(true);
      expect(b.command({ do: 'land:smoke', units: [u.id], x0, y0, x1, y1 }, 'att')).toBe(false);
      const s = b.screens[b.screens.length - 1];
      expect(s.until - s.t).toBeCloseTo(30, 6);
      expect(b.log.filter((e) => e.what === 'land:smoke').length).toBe(i + 1);
      s.until = b.t;
    }
    expect(u.smokeLeft).toBe(0);
    expect(b.smokeWhy(u)).toBe('They have no smoke left.');
    const uk = faced({ att: 'UK', def: 'GER' });
    for (let i = 0; i < 3; i++) { const [x0, y0, x1, y1] = at(uk.u, 100); expect(uk.b.command({ do: 'land:smoke', units: [uk.u.id], x0, y0, x1, y1 }, 'att')).toBe(true); uk.b.screens[i].until = uk.b.t; }
    expect(uk.b.smokeWhy(uk.u)).toBe('They have no smoke left.');
  });
  it('each army\'s means: reach, hang and count as sourced; its kin take its senior partner\'s; no stand-in', () => {
    const means: [string, number, number, number][] = [['USA', 195, 30, 2], ['UK', 457, 30, 3], ['GER', 30, 22, 2], ['SOV', 30, 22, 2], ['JAP', 188, 22, 2], ['ITA', 0, 19, 2]];
    for (const [nat, reach, hang, count] of means) { const m = smokeMeans(nat)!; expect([m.reach, m.hang, m.count ?? LAND.smoke.count]).toEqual([reach, hang, count]); }
    for (const kin of ['CAN', 'AUS', 'NZL', 'SAF', 'IND']) expect(smokeMeans(kin)).toBe(smokeMeans('UK'));
    for (const none of ['FRA', 'VIC', 'FFR', 'RED', 'BLU', 'POL']) expect(smokeMeans(none)).toBeNull();
    // The 240 form (plans.kin on the air branch's arsenal.json): Vichy and Free France both follow France.
    expect(LAND.smoke.kin).toEqual({ CAN: 'UK', AUS: 'UK', NZL: 'UK', SAF: 'UK', IND: 'UK', VIC: 'FRA', FFR: 'FRA' });
  });
  it('the line is clipped to the block\'s front about its middle; an army whose means is set down at its own front lays it there', () => {
    const { b, u } = faced();
    const my = u.y + u.d / 2 + 100;
    b.command({ do: 'land:smoke', units: [u.id], x0: u.x - 300, y0: my, x1: u.x + 300, y1: my }, 'att');
    const s = b.screens[0];
    expect(Math.hypot(s.x1 - s.x0, s.y1 - s.y0)).toBeCloseTo(u.w, 0);
    expect((s.x0 + s.x1) / 2).toBeCloseTo(u.x, 0);
    expect((s.y0 + s.y1) / 2).toBeCloseTo(my, 0);
    const ita = faced({ att: 'ITA', def: 'GER' }), iu = ita.u, front = iu.y + iu.d / 2;
    // A line drawn far off is refused, saying why (129(d)): Italy's pot is set down along its own front.
    expect(ita.b.smokeWhy(iu, iu.x + 400, iu.y + 400, iu.x + 500, iu.y + 300)).toBe('They set their smoke down along their own front.');
    expect(ita.b.smokeWhy(iu, iu.x, front + LAND.smoke.front + 5)).toBe('They set their smoke down along their own front.');
    expect(ita.b.command({ do: 'land:smoke', units: [iu.id], x0: iu.x + 400, y0: iu.y + 400, x1: iu.x + 500, y1: iu.y + 300 }, 'att')).toBe(false);
    expect(ita.b.screens.length).toBe(0);
    // On its own ground, or within a throw of its front: laid along its front.
    expect(ita.b.smokeWhy(iu, iu.x, iu.y)).toBeNull();
    expect(ita.b.smokeWhy(iu, iu.x + 20, front + LAND.smoke.front - 5)).toBeNull();
    expect(ita.b.command({ do: 'land:smoke', units: [iu.id], x0: iu.x - 60, y0: front + 20, x1: iu.x + 100, y1: front + 25 }, 'att')).toBe(true);
    const q = ita.b.screens[0];
    expect((q.y0 + q.y1) / 2).toBeCloseTo(ita.u.y + ita.u.d / 2, 0);
    expect((q.x0 + q.x1) / 2).toBeCloseTo(ita.u.x, 0);
    expect(Math.hypot(q.x1 - q.x0, q.y1 - q.y0)).toBeCloseTo(ita.u.w, 0);
    expect(q.until - q.t).toBeCloseTo(19, 6);
  });
});

describe('(5b) the 2D test kitchen: ?cutters alone leaves the field\'s date, ?july44 moves it', () => {
  it('kitchenTicket', () => {
    const t0 = practiceTicket(PRACTICE_GROUNDS[0], { humanAtt: true, seed: 5 });
    expect(kitchenTicket(t0, (f) => f === 'cutters' || f === 'smoke' || f === 'hedges')).toBe(t0);
    const t = kitchenTicket(t0, (f) => f === 'july44');
    expect([t.turn, t.date]).toEqual([LAND.cutters.from, 'July 1944']);
    expect(t0.turn).toBeLessThan(LAND.cutters.from);
  });
});

// ---- (6) both staffs ------------------------------------------------------------------------------------------------

describe('(6) both staffs lay screens as their foot goes in, never from their works', () => {
  type Laid = { s: Screen; planned: boolean; went: number | null; home: boolean; sent: boolean; target: boolean; kind: string };
  const fought = (t: T, human = false): { b: LandBattle; laid: Laid[] } => {
    const b = new LandBattle(t);
    b.ai.att = b.ai.def = true; b.smoke = true;
    if (human) b.perUnit = true;
    const laid: Laid[] = [];
    for (let k = 0; !b.over && k < 30000; k++) {
      const n = b.screens.length;
      b.step();
      for (const s of b.screens.slice(n)) {
        const u = b.unit(s.unit)!, P = b.plan[s.side];
        laid.push({ s, planned: !!P, went: P?.went ?? null, home: u.side === 'def' && u.reserve !== 'sent' && Math.hypot(u.x - u.home[0], u.y - u.home[1]) < LAND.hold.within, sent: u.reserve === 'sent', target: u.order?.kind === 'attack', kind: u.kind });
      }
    }
    return { b, laid };
  };
  const ai = fought(sampleTicket('plains', 1.1, { seed: 3, nat: { att: 'USA', def: 'GER' } }));
  const mine = fought(sampleTicket('plains', 1.0, { seed: 2, humanAtt: true, nat: { att: 'GER', def: 'UK' } }), true);
  it('the attack\'s in its assault (once its main effort has gone in), the defence\'s reserves sent forward; at their targets, foot only', () => {
    for (const { laid } of [ai, mine]) {
      expect(laid.filter((x) => x.s.side === 'att').length).toBeGreaterThan(0);
      expect(laid.filter((x) => x.s.side === 'def').length).toBeGreaterThan(0);
      for (const x of laid) {
        expect(x.kind).toBe('inf');
        expect(x.target).toBe(true);
        // An attack with a plan lays none before its main effort has gone in.
        if (x.s.side === 'att' && x.planned) { expect(x.went).not.toBeNull(); expect(x.s.t).toBeGreaterThanOrEqual(x.went!); }
        expect(x.home).toBe(false);
        // On the defence, only a reserve sent forward: a line block never leaves its works (its leash is LAND.hold.within).
        if (x.s.side === 'def') expect(x.sent).toBe(true);
      }
    }
    // The computer's attack fights to a plan (its main effort waits at its start line, then goes in).
    expect(ai.laid.some((x) => x.s.side === 'att' && x.planned)).toBe(true);
  });
  it('his own staff lays them too (the player\'s blocks he has not taken in hand)', () => {
    expect(mine.b.ticket.att.human).toBe(true);
    expect(mine.laid.some((x) => x.s.side === 'att')).toBe(true);
  });
});

// ---- (7) the drill --------------------------------------------------------------------------------------------------

describe('(7) the drill: screens drawn as their rounds in place of its own salvoes', () => {
  it('switched off: the rounds of 3bda466e, the drill\'s own smoke among them', () => {
    const b = field(), d = b.drillOn({ sections: true, platoons: true }), shots: string[] = [], said: boolean[] = [];
    let last = 0, smoke = 0;
    for (let k = 0; !b.over && k < 30000; k++) {
      offOrders(said)(b, k);
      b.step();
      for (const r of d.rounds()) { if (r.n <= last) continue; last = r.n; shots.push(JSON.stringify(r)); if (r.w === 'smoke') { smoke++; expect(r.hang).toBeUndefined(); } }
    }
    expect([shots.length, smoke, digest(shots.join('\n'))]).toEqual(OFF_DRILL);
  }, 600_000);
  it('switched on: no salvo of its own; each screen\'s rounds along its line, a round each 15 m (12 at most), each hanging to its end', () => {
    const b = field(), d = b.drillOn({ sections: true, platoons: true });
    b.smoke = true;
    const rounds: { by?: number; on: number; t: number; l: number; x: number; y: number; hang?: number }[] = [];
    let last = 0;
    for (let k = 0; !b.over && k < 2000; k++) {
      b.step();
      for (const r of d.rounds()) { if (r.n <= last) continue; last = r.n; if (r.w === 'smoke') rounds.push(r); }
    }
    expect(b.screens.length).toBeGreaterThan(0);
    expect(rounds.length).toBeGreaterThan(0);
    for (const r of rounds) {
      expect(r.on).toBe(-1);
      expect(r.hang).toBeDefined();
      const s = b.screens.find((q) => q.unit === r.by && r.t >= Math.round(q.t * 10) / 10 - 1e-6 && r.t <= q.until)!;
      expect(s).toBeDefined();
      expect(r.hang).toBeCloseTo(Math.max(0, Math.round((s.until - r.l) * 10) / 10), 6);
      expect(r.l).toBeGreaterThanOrEqual(r.t);
    }
    for (const s of b.screens) {
      const n = Math.min(BOOK_MOST, Math.max(1, Math.ceil(Math.hypot(s.x1 - s.x0, s.y1 - s.y0) / BOOK_PER)));
      expect(rounds.filter((r) => r.by === s.unit && r.t >= Math.round(s.t * 10) / 10 - 1e-6 && r.t <= s.until).length).toBeLessThanOrEqual(n);
    }
  }, 600_000);
});
const BOOK_PER = 15, BOOK_MOST = 12;

// ---- (8) the bridge -------------------------------------------------------------------------------------------------

describe('(8) the bridge: the switches, the why calls, the flags and the fog', () => {
  const rig = () => {
    const g = newCampaign();
    const TC = battleCalls({ game: () => g, attack: (ids, to, f) => ({ report: orderAttack(g, ids.map((i) => g.corps.find((c) => c.id === i)!), g.regions[to], f) }) });
    return { g, TC };
  };
  type U = { id: number; side: string; kind: string; team?: string; state: string; smoke?: number; cutter?: number; known?: boolean };
  it('off: no flags, no keys, the orders refused, the why says so', () => {
    const { g, TC } = rig();
    const o = offers(g)[0];
    const st = TC.battleStart(o.corps.map((c) => c.id), o.target.id)!;
    const units = st.view.units as U[], me = st.view.me as string, foot = units.find((u) => u.side === me && ['inf', 'mtn', 'cav', 'eng'].includes(u.kind) && !u.team)!;
    expect(units.some((u) => u.smoke !== undefined || u.cutter !== undefined)).toBe(false);
    expect(TC.battleSmokeWhy(foot.id).why).toBe('There is no smoke to lay in this battle.');
    expect(TC.battleCutWhy(foot.id, 0, 0).why).toBe('Your tanks carry no hedge cutters.');
    expect(TC.battleOrder({ do: 'land:smoke', units: [foot.id], x0: 1, y0: 1, x1: 2, y1: 2 } as never).ok).toBe(false);
    const v = TC.battleView() as Record<string, unknown>;
    expect(v.screens).toBeUndefined(); expect(v.cuts).toBeUndefined();
  });
  it('on: his own foot carry smoke, the enemy\'s never; why calls; a screen laid shows in the view', () => {
    const { g, TC } = rig();
    const o = offers(g)[0];
    const st = TC.battleStart(o.corps.map((c) => c.id), o.target.id, { cutters: true, smoke: true })!;
    const units = st.view.units as U[], me = st.view.me as string;
    const foot = units.find((u) => u.side === me && ['inf', 'mtn', 'cav', 'eng'].includes(u.kind) && !u.team)!, foe = units.find((u) => u.side !== me)!;
    // The player's Germany: section smoke grenades and candles, 30 m.
    const m = smokeMeans(g.human!);
    expect(g.human).toBe('GER');
    expect(units.filter((u) => u.side !== me).every((u) => u.smoke === undefined)).toBe(true);
    if (m) expect(foot.smoke).toBe(m.count ?? LAND.smoke.count);
    expect(TC.battleSmokeWhy(foe.id).why).toBe('Only your own troops take your orders.');
    expect(TC.battleSmokeWhy(99999).why).toBe('Nothing to order.');
    expect(TC.battleCutWhy(foot.id, 0, 0).why).toBe('Only tanks carry hedge cutters.');
    // The campaign's 1939: no cutters on any tank of it.
    expect(units.some((u) => u.cutter !== undefined)).toBe(false);
    if (!m) { expect(TC.battleSmokeWhy(foot.id).why).toBe('Your army has no smoke to lay.'); return; }
    expect(TC.battleSmokeWhy(foot.id).why).toBeNull();
    const fu = units.find((u) => u.id === foot.id)! as U & { x: number; y: number; face: number; d: number };
    const [fx, fy] = [Math.sin(fu.face), Math.cos(fu.face)], k = fu.d / 2 + Math.max(0, m.reach - 5);
    const mx = fu.x + fx * k, my = fu.y + fy * k;
    expect(TC.battleSmokeWhy(foot.id, mx + fx * 50, my + fy * 50).why).toBe(m.reach > 0 ? `They can lay smoke up to ${m.reach} m ahead.` : null);
    expect(TC.battleOrder({ do: 'land:smoke', units: [foot.id], x0: mx - fy * 80, y0: my + fx * 80, x1: mx + fy * 80, y1: my - fx * 80 } as never).ok).toBe(true);
    const v = TC.battleView() as { screens?: { unit?: number; side: string }[]; units: U[] };
    expect(v.screens!.length).toBe(1);
    expect(v.screens![0].unit).toBe(foot.id);
    expect(v.units.find((u) => u.id === foot.id)!.smoke).toBe((m.count ?? LAND.smoke.count) - 1);
  });
  it('unitOut: cutter on a tank carrying them, either side\'s, never on an enemy hidden from him; smoke his own only', () => {
    const b = field();
    b.cutters = true; b.smoke = true;
    const tank = tankOf(b), foot = footOf(b), gun = b.units.find((u) => u.side === 'def' && u.kind === 'art')!;
    expect(unitOut(tank, 0, 'att', b).cutter).toBe(LAND.cutters.share);
    expect(unitOut(tank, 0, 'def', b).cutter).toBe(LAND.cutters.share);
    tank.hid = true;
    expect(unitOut(tank, 0, 'def', b).cutter).toBeUndefined();
    expect(unitOut(tank, 0, 'att', b).cutter).toBe(LAND.cutters.share);
    delete tank.hid;
    expect(unitOut(foot, 0, 'att', b).cutter).toBeUndefined();
    expect(unitOut(foot, 0, 'att', b).smoke).toBe(2);
    expect(unitOut(foot, 0, 'def', b).smoke).toBeUndefined();
    expect(unitOut(gun, 0, 'def', b).smoke).toBeUndefined();
    expect(unitOut(tank, 0, 'att').cutter).toBeUndefined();
    b.cutters = false; b.smoke = false;
    expect(unitOut(tank, 0, 'att', b).cutter).toBeUndefined();
    expect(unitOut(foot, 0, 'att', b).smoke).toBeUndefined();
  });
  it('fog: his own cuts and screens; the enemy\'s cuts only of blocks he knew as they cut; an enemy screen without its block where he has not met it', () => {
    const b = field();
    b.cutters = true; b.smoke = true; b.fog = true;
    const mine = tankOf(b), theirs = footOf(b, 'def');
    expect(b.known(theirs)).toBe(false);
    inside(b).cutBegin(mine, 1000, hedgeY(b), 0);
    inside(b).cutBegin(theirs, 1200, hedgeY(b), 0);
    theirs.found = true;
    inside(b).cutBegin(theirs, 1400, hedgeY(b), 0);
    delete theirs.found;
    const seen = (as?: Side): number[] => (b.view(as).cuts ?? []).map((c) => c.x);
    expect(seen('att')).toEqual([1000, 1400]);
    expect(seen()).toEqual([1000, 1200, 1400]);
    expect(b.cuts.map((c: Cut) => c.known)).toEqual([true, false, true]);
    screenAt(b, mine, 900, 800, 1000, 800);
    screenAt(b, theirs, 900, 1200, 1000, 1200);
    const sv = b.view('att').screens!;
    expect(sv.map((s) => s.unit)).toEqual([mine.id, undefined]);
    expect(b.view().screens!.map((s) => s.unit)).toEqual([mine.id, theirs.id]);
    // The enemy's cut, told: with no words for the player, and only in the log while he has not met it.
    expect(b.log.filter((e) => e.what === 'land:cut' && e.unit === theirs.id).every((e) => e.text === '')).toBe(true);
  });
});

// ---- (9) the measure (LONG=1) -----------------------------------------------------------------------------------------

/**
 * The field against itself with smoke on (ruling 242 S5-A: both staffs alike, measured paired on against off within the
 * tenth, b418): on every kind of ground, an army with smoke attacking an army with none and defending against one, at odds
 * 1.0 and 1.4, SMOKE_SEEDS seeds (80), both staffs, no player orders. Each row's attack win rate with smoke on against
 * the same battles with it off: within a tenth. SMOKE_GROUND runs one ground (a measure split over machines);
 * SMOKE_NAT the army with smoke (UK, the longest reach and the most screens, where not said).
 */
describe('(9) the field with smoke on against off (a measure: LONG=1)', () => {
  const LONG = !!env?.env.LONG;
  const GROUNDS: [string, (seed: number, odds: number, nat: { att: string; def: string }) => T][] = [
    ['plains', (seed, odds, nat) => sampleTicket('plains', odds, { seed, nat })],
    ['forest', (seed, odds, nat) => sampleTicket('forest', odds, { seed, nat })],
    ['hills', (seed, odds, nat) => sampleTicket('hills', odds, { seed, nat })],
    ['city', (seed, odds, nat) => sampleTicket('urban', odds, { seed, town: 'city', nat })],
    ['river', (seed, odds, nat) => sampleTicket('plains', odds, { seed, river: true, nat })],
  ];
  const only = env?.env.SMOKE_GROUND, seeds = Number(env?.env.SMOKE_SEEDS ?? 80), army = env?.env.SMOKE_NAT ?? 'UK';
  it.runIf(LONG)('every ground, each side with smoke, each odds: smoke on moves the attack\'s win rate within a tenth of off', () => {
    const rows: string[] = [], bad: string[] = [];
    for (const [ground, make] of GROUNDS) {
      if (only && only !== ground) continue;
      for (const side of ['att', 'def'] as Side[]) for (const odds of [1.0, 1.4]) {
        const nat = side === 'att' ? { att: army, def: 'BLU' } : { att: 'RED', def: army };
        let w0 = 0, w1 = 0, n = 0, laid = 0, lr = 0;
        for (let seed = 1; seed <= seeds; seed++) {
          const R: number[] = [];
          for (const on of [false, true]) {
            const b = new LandBattle(make(seed, odds, nat));
            b.ai.att = b.ai.def = true; b.smoke = on;
            for (let k = 0; !b.over && k < 30000; k++) b.step();
            R.push(b.over!.R);
            const win = b.over!.R >= b.ticket.reckoning.even ? 1 : 0;
            if (on) { w1 += win; laid += b.screens.length; } else w0 += win;
          }
          lr += Math.log(Math.max(1e-6, R[1]) / Math.max(1e-6, R[0]));
          n++;
        }
        const gap = (w1 - w0) / n;
        const row = `${ground.padEnd(6)} ${army} ${side === 'att' ? 'attacking' : 'defending'} odds ${odds.toFixed(1)}: off ${w0}/${n}, on ${w1}/${n}, on-off ${gap.toFixed(3)}; R ratio on/off ${Math.exp(lr / n).toFixed(3)}; screens ${(laid / n).toFixed(1)} a battle`;
        rows.push(row);
        env?.stdout?.write(`${row}\n`);
        if (Math.abs(gap) > 0.1) bad.push(row);
      }
    }
    expect(rows.length).toBeGreaterThan(0);
    expect(bad, bad.join('\n')).toEqual([]);
  }, 7_200_000);
});

// ---- (10) the bocage (C1-B) -------------------------------------------------------------------------------------------

/**
 * Ruling 242 C1-B, built as its 17:45Z amendment asks ("unfitted tanks cross only at gaps"), under its own switch
 * (LandBattle.bocage). Off, the pins above (the battle of 3bda466e) hold. On, a tank block without cutters crosses a
 * hedge line only at a gap in it, a gateway or a cut once open, within a cell or so of it (the field's grain), and goes
 * round to one; a tank block with cutters goes on as before, so with every tank fitted the fight is the very same.
 */
describe('(10) the bocage: tanks without cutters cross a hedge only at a gap', () => {
  const CELL = 25, NEAR = 1.6 * CELL;
  /** His one tank block south of the three practice hedges, nobody else near, nobody's staff thinking. */
  const lone = (turn: number, o: { bocage?: boolean; cutters?: boolean } = {}): { b: LandBattle; u: LandUnit } => {
    const b = new LandBattle({ ...sampleTicket('plains', 1.0, { seed: 2, humanAtt: true, nat: { att: 'USA', def: 'GER' } }), turn });
    b.ai.att = b.ai.def = false; b.bocage = !!o.bocage; b.cutters = !!o.cutters;
    const H = practiceHedges(b.field.w, b.field.h); b.hedgesOn(H.hedges, H.gates);
    const u = tankOf(b);
    for (const x of b.units) if (x !== u) { x.x = x.side === 'att' ? 60 + x.id * 10 : b.field.w - 60 - x.id * 10; x.y = x.side === 'att' ? 60 : b.field.h - 60; x.order = null; x.path = []; }
    u.x = 1500; u.y = 700; u.face = 0;
    return { b, u };
  };
  /** Where its middle crossed each hedge line (y), driving north to (1500, 1300). */
  const drive = (b: LandBattle, u: LandUnit, n = 3000): { y: number; x: number }[] => {
    const lines = [0.42, 0.5, 0.58].map((f) => Math.round(b.field.h * f)), out: { y: number; x: number }[] = [];
    expect(b.command({ do: 'move', units: [u.id], x: 1500, y: 1300 }, 'att')).toBe(true);
    for (let k = 0; k < n && u.order; k++) {
      const [x0, y0] = [u.x, u.y];
      b.step();
      for (const y of lines) if ((y0 - y) * (u.y - y) < 0) out.push({ y, x: x0 + ((u.x - x0) * (y - y0)) / (u.y - y0) });
    }
    return out;
  };
  it('off: straight over the hedges; on, without cutters: over each only at a gateway, and there all the same', () => {
    const off = lone(50), on = lone(50, { bocage: true });
    expect(on.b.heldBack(on.u)).toBe(true);
    expect(off.b.heldBack(off.u)).toBe(false);
    const a = drive(off.b, off.u), c = drive(on.b, on.u);
    expect(a.length).toBe(3);
    for (const p of a) expect(Math.abs(p.x - 1500)).toBeLessThan(1);
    expect(c.map((p) => p.y)).toEqual(a.map((p) => p.y));
    for (const p of c) expect(Math.min(...on.b.gates.filter((g) => g.y === p.y).map((g) => Math.abs(g.x - p.x)))).toBeLessThan(NEAR);
    expect(Math.hypot(on.u.x - 1500, on.u.y - 1300)).toBeLessThan(8);
    // The way round costs it time.
    expect(on.b.t).toBeGreaterThan(off.b.t + 10);
  });
  it('a gap cut and open serves any tank: one without cutters goes through it', () => {
    const { b, u } = lone(50, { bocage: true, cutters: true });
    expect(b.cutterOf(u)).toBe(false);
    // A cutter tank's gap, begun now (an enemy's: the cut is the hedge's, whoever cut it), open from t1.
    const y = hedgeY(b), foe = footOf(b, 'def');
    inside(b).cutBegin(foe, 1500, y, 0);
    for (let k = 0; k < 400 && b.t < b.cuts[0].t1 + 1; k++) b.step();
    expect(b.gates.some((g) => g.kind === 'cut' && g.x === 1500)).toBe(true);
    const c = drive(b, u);
    expect(Math.abs(c[0].x - 1500)).toBeLessThan(NEAR);
  });
  it('never eased over a hedge but at a gap: a friend shoving it at the hedge moves it along, not over', () => {
    const { b, u } = lone(50, { bocage: true });
    const y = hedgeY(b), f = b.units.filter((x) => x.side === 'att' && x.kind === 'arm')[1];
    u.y = y - 0.3; u.speed = 0;
    f.x = u.x; f.y = u.y - (u.d + f.d) / 2 + 3; f.face = 0; f.speed = 0;
    steps(b, 5);
    expect(u.y).toBeLessThan(y);
    // Off, the same shove puts it over.
    const o = lone(50), fo = o.b.units.filter((x) => x.side === 'att' && x.kind === 'arm')[1];
    o.u.y = y - 0.3; o.u.speed = 0; fo.x = o.u.x; fo.y = o.u.y - (o.u.d + fo.d) / 2 + 3; fo.face = 0; fo.speed = 0;
    steps(o.b, 5);
    expect(o.u.y).toBeGreaterThan(y);
  });
  it('a tank block standing at a hedge walks off it on its own side', () => {
    const { b, u } = lone(50, { bocage: true });
    const y = hedgeY(b);
    u.y = y - 0.3;
    expect(b.command({ do: 'move', units: [u.id], x: 1500, y: 600 }, 'att')).toBe(true);
    let over = false;
    for (let k = 0; k < 400 && u.order; k++) { b.step(); if (u.y > y) over = true; }
    expect(over).toBe(false);
    expect(Math.abs(u.y - 600)).toBeLessThan(8);
  });
  it('with every tank fitted (cutters, July 1944), the bocage changes nothing: the very same fight', () => {
    const run = (bocage: boolean): string => {
      const b = field();
      b.cutters = true; b.bocage = bocage;
      expect(b.units.filter((u) => u.kind === 'arm').every((u) => !b.heldBack(u))).toBe(true);
      return signed(b);
    };
    expect(run(true)).toBe(run(false));
  }, 300_000);
});

/**
 * The bocage against off (a measure: LONG=1, ruling 242 C1-B "measured against the card"): on the hedged practice field
 * (three hedge lines across the middle, two gateways each, and one wherever a line crosses a road or a bridge: fieldHedges,
 * as no hedge closes a lane; laid straight over a river's bridges, the river's attack lost its tanks for want of any way
 * over), before July 1944 (no tank carries cutters), both staffs, no
 * player orders, BOCAGE_SEEDS seeds (80) at odds 1.0 and 1.4, the attack's win rate with the bocage on against the same
 * battles with it off: within a tenth (b418). BOCAGE_GROUND runs one ground.
 */
describe('(10b) the bocage on against off (a measure: LONG=1)', () => {
  const LONG = !!env?.env.LONG;
  const GROUNDS: [string, (seed: number, odds: number) => T][] = [
    ['plains', (seed, odds) => sampleTicket('plains', odds, { seed })],
    ['forest', (seed, odds) => sampleTicket('forest', odds, { seed })],
    ['hills', (seed, odds) => sampleTicket('hills', odds, { seed })],
    ['city', (seed, odds) => sampleTicket('urban', odds, { seed, town: 'city' })],
    ['river', (seed, odds) => sampleTicket('plains', odds, { seed, river: true })],
  ];
  const only = env?.env.BOCAGE_GROUND, seeds = Number(env?.env.BOCAGE_SEEDS ?? 80);
  it.runIf(LONG)('every ground, each odds: the bocage moves the attack\'s win rate within a tenth of off', () => {
    const rows: string[] = [], bad: string[] = [];
    for (const [ground, make] of GROUNDS) {
      if (only && only !== ground) continue;
      for (const odds of [1.0, 1.4]) {
        let w0 = 0, w1 = 0, n = 0, lr = 0;
        for (let seed = 1; seed <= seeds; seed++) {
          const R: number[] = [];
          for (const on of [false, true]) {
            const b = new LandBattle({ ...make(seed, odds), turn: LAND.cutters.from - 1 });
            b.ai.att = b.ai.def = true; b.bocage = on;
            const H = fieldHedges(b.field); b.hedgesOn(H.hedges, H.gates);
            for (let k = 0; !b.over && k < 30000; k++) b.step();
            R.push(b.over!.R);
            const win = b.over!.R >= b.ticket.reckoning.even ? 1 : 0;
            if (on) w1 += win; else w0 += win;
          }
          lr += Math.log(Math.max(1e-6, R[1]) / Math.max(1e-6, R[0]));
          n++;
        }
        const gap = (w1 - w0) / n;
        const row = `${ground.padEnd(6)} bocage odds ${odds.toFixed(1)}: off ${w0}/${n}, on ${w1}/${n}, on-off ${gap.toFixed(3)}; R ratio on/off ${Math.exp(lr / n).toFixed(3)}`;
        rows.push(row);
        env?.stdout?.write(`${row}\n`);
        if (Math.abs(gap) > 0.1) bad.push(row);
      }
    }
    expect(rows.length).toBeGreaterThan(0);
    expect(bad, bad.join('\n')).toEqual([]);
  }, 7_200_000);
});
