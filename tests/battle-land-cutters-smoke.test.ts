// Hedge cutters and smoke on call (release 2; Brian, 22 November 14:19Z: "Let's do 1 and 2": bladed Shermans punch a
// gap through a hedgerow where the player picks; a section lays a smoke line to cover a crossing, when and where the
// player orders it). The design director's ruling 242, as amended 17:45Z from the historian: cutters (C1-A, C2-A, C3,
// C4-A) are looks only, every army's tanks from July 1944 on a hedged field, the player's where he orders and his staff's
// wherever their way crosses a hedge with no gap, a gap 3.2 m wide open 6 battle seconds after the cut begins; smoke
// (S1-A to S6) halves a direct blow across a live screen (never on top of the vague share; fire from far back untouched),
// one screen a block at a time, the army's count a battle, laid within its army's reach of its front and at most its
// front long, both staffs alike as their foot goes in, the drill drawing each screen as its rounds in place of its own
// salvoes. Off, nothing of either is read: the battle of 3bda466e to the last number.
import { describe, expect, it } from 'vitest';
import type { FieldEvent, FieldTicket, Side } from '../src/battle/core';
import { LAND, LandBattle, practiceHedges, sampleTicket, smokeMeans, withTeams, type Cut, type LandPlace, type LandUnit, type Screen } from '../src/battle/land';
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
  it('smoke: laid as it lands, not before; not laid where one of theirs went up meanwhile', () => {
    const b = radio(), u = footOf(b);
    expect(b.command({ do: 'land:smoke', units: [u.id], x0: u.x - 80, y0: u.y + 100, x1: u.x + 80, y1: u.y + 100 }, 'att')).toBe(true);
    expect(b.screens.length).toBe(0);
    landed(b, u);
    expect(b.screens.length).toBe(1);
    expect(b.screens[0].t).toBeGreaterThan(0);
    const v = b.units.filter((x) => x.side === 'att' && x.kind === 'inf')[1];
    b.command({ do: 'land:smoke', units: [v.id], x0: v.x - 80, y0: v.y + 100, x1: v.x + 80, y1: v.y + 100 }, 'att');
    screenAt(b, v, v.x - 50, v.y + 60, v.x + 50, v.y + 60);
    landed(b, v);
    expect(b.screens.filter((s) => s.unit === v.id).length).toBe(1);
    expect(v.smokeLeft).toBeUndefined();
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
    for (const none of ['FRA', 'VIC', 'RED', 'BLU', 'POL']) expect(smokeMeans(none)).toBeNull();
  });
  it('the line is clipped to the block\'s front about its middle; an army whose means is set down at its own front lays it there', () => {
    const { b, u } = faced();
    const my = u.y + u.d / 2 + 100;
    b.command({ do: 'land:smoke', units: [u.id], x0: u.x - 300, y0: my, x1: u.x + 300, y1: my }, 'att');
    const s = b.screens[0];
    expect(Math.hypot(s.x1 - s.x0, s.y1 - s.y0)).toBeCloseTo(u.w, 0);
    expect((s.x0 + s.x1) / 2).toBeCloseTo(u.x, 0);
    expect((s.y0 + s.y1) / 2).toBeCloseTo(my, 0);
    const ita = faced({ att: 'ITA', def: 'GER' });
    expect(ita.b.command({ do: 'land:smoke', units: [ita.u.id], x0: ita.u.x + 400, y0: ita.u.y + 400, x1: ita.u.x + 500, y1: ita.u.y + 300 }, 'att')).toBe(true);
    const q = ita.b.screens[0];
    expect((q.y0 + q.y1) / 2).toBeCloseTo(ita.u.y + ita.u.d / 2, 0);
    expect((q.x0 + q.x1) / 2).toBeCloseTo(ita.u.x, 0);
    expect(Math.hypot(q.x1 - q.x0, q.y1 - q.y0)).toBeCloseTo(ita.u.w, 0);
    expect(q.until - q.t).toBeCloseTo(19, 6);
  });
});

// ---- (6) both staffs ------------------------------------------------------------------------------------------------

describe('(6) both staffs lay screens as their foot goes in, never from their works', () => {
  const fought = (t: T, human = false): { b: LandBattle; laid: { s: Screen; went: number | null; home: boolean; target: boolean; kind: string }[] } => {
    const b = new LandBattle(t);
    b.ai.att = b.ai.def = true; b.smoke = true;
    if (human) b.perUnit = true;
    const laid: { s: Screen; went: number | null; home: boolean; target: boolean; kind: string }[] = [];
    for (let k = 0; !b.over && k < 30000; k++) {
      const n = b.screens.length;
      b.step();
      for (const s of b.screens.slice(n)) {
        const u = b.unit(s.unit)!;
        laid.push({ s, went: b.plan[s.side]?.went ?? null, home: u.side === 'def' && u.reserve !== 'sent' && Math.hypot(u.x - u.home[0], u.y - u.home[1]) < LAND.hold.within, target: u.order?.kind === 'attack', kind: u.kind });
      }
    }
    return { b, laid };
  };
  const ai = fought(sampleTicket('plains', 1.1, { seed: 3, nat: { att: 'USA', def: 'GER' } }));
  const mine = fought(sampleTicket('plains', 1.0, { seed: 2, humanAtt: true, nat: { att: 'GER', def: 'UK' } }), true);
  it('the attack\'s in its assault, the defence\'s in counter-attacks; at their targets, foot only', () => {
    for (const { laid } of [ai, mine]) {
      expect(laid.filter((x) => x.s.side === 'att').length).toBeGreaterThan(0);
      expect(laid.filter((x) => x.s.side === 'def').length).toBeGreaterThan(0);
      for (const x of laid) {
        expect(x.kind).toBe('inf');
        expect(x.target).toBe(true);
        if (x.s.side === 'att' && x.went !== null) expect(x.s.t).toBeGreaterThanOrEqual(x.went);
        expect(x.home).toBe(false);
      }
    }
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
    expect(unitOut(tank, 0, 'att', b).cutter).toBe(1);
    expect(unitOut(tank, 0, 'def', b).cutter).toBe(1);
    tank.hid = true;
    expect(unitOut(tank, 0, 'def', b).cutter).toBeUndefined();
    expect(unitOut(tank, 0, 'att', b).cutter).toBe(1);
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
