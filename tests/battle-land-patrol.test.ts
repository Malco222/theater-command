// Night patrol (release 2; Brian, 22 November 17:33Z: "lets do all three but make it where we can toggle it off if
// someone doesnt want it"). The design director, 18:36Z: Q6, found 0.4 and caught 0.3, knobs; Q7, a caught patrol
// leaves the defence alerted, ready not stronger: one more hidden post (a player's defence too, laid for him and named
// in his warning), a computer defence's reserve forward at once, the patrol's men lost to its block; Q8, the computer's
// attack always sends one, the player's by his order. It needs hiding: off, or with hiding off, nothing of it is read
// and nothing is drawn: the battle of ec6f8e9c to the last number. Its draws are its own stream's, never the battle's.
import { afterEach, describe, expect, it, vi } from 'vitest';
import type { FieldEvent, FieldTicket, Side } from '../src/battle/core';
import { LAND, LandBattle, MOTION, patrolSize, practiceHedges, sampleTicket, withTeams, type LandPlace, type LandUnit } from '../src/battle/land';
import { newCampaign, offers } from '../src/battle/ui/campaign';
import { orderAttack } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import { Rng, hashString } from '../src/core/rng';
import { battleCalls } from '../unreal/TheaterCommand/Rules/battle-bridge';

type T = FieldTicket<LandPlace>;
const env = (globalThis as { process?: { env: Record<string, string | undefined>; stdout?: { write(s: string): void } } }).process;
const PRINT = !!env?.env.PRINT;

/**
 * Everything a battle shows and does, every tick (the cutters and smoke test's own, with whether each block is hidden,
 * sprang its ambush or is known, and every event's extra), less the view keys in 'drop' and the events in 'skip'.
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
    rng: inside(b).rng.state, gates: b.gates,
    units: b.units.map((u) => [u.id, u.corps, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged, u.reserve ?? null, u.power0,
      u.home, u.dry ?? null, u.order, u.path, u.cal ?? null, u.crew ?? null, u.taken ?? null, u.hid ?? null, u.sprung ?? null, u.found ?? null, Object.keys(u).filter((k) => !hide.includes(k)).sort().join(',')]),
    wrecks: b.wrecks.map((z) => [z.x, z.y, z.at]),
    said: ev.map((e) => [e.t, e.what, e.unit ?? null, e.text, e.extra ?? null]),
    log: b.log.filter((e) => !skip.includes(e.what)).map((e) => [e.t, e.what, e.unit ?? null, e.text, e.extra ?? null]),
  });
}
const digest = (s: string): string => hashString(s).toString(16).padStart(8, '0') + hashString('#' + s).toString(16).padStart(8, '0');
const pin = (b: LandBattle, s: string): [number, number, string] => [Math.round(b.over!.R * 1000) / 1000, b.over!.seconds, digest(s)];

/** The rules' private workings a test reads. */
interface Inside { rng: Rng; hideStart(): void; patrolBy: number | null }
const inside = (b: LandBattle): Inside => b as unknown as Inside;
const steps = (b: LandBattle, n: number): void => { for (let k = 0; k < n && !b.over; k++) b.step(); };
const toEnd = (b: LandBattle, each?: (b: LandBattle) => void): void => { for (let k = 0; !b.over && k < 30000; k++) { b.step(); each?.(b); } };
const said = (b: LandBattle, what: string): FieldEvent[] => b.log.filter((e) => e.what === what);
const onFoot = (u: LandUnit): boolean => MOTION[u.kind] === 'foot' && !u.team && u.state !== 'gone' && u.state !== 'withdrawn';

/** The patrol's odds set by hand for one test (the knobs, LAND.patrol.found and caught), put back after. */
const ODDS = { found: LAND.patrol.found, caught: LAND.patrol.caught };
afterEach(() => { LAND.patrol.found = ODDS.found; LAND.patrol.caught = ODDS.caught; });
const odds = (found: number, caught: number): void => { LAND.patrol.found = found; LAND.patrol.caught = caught; };

/** The computer's attack on the computer's defence in the forest, America on Germany, teams, hiding on; the patrol as asked. */
function forest(seed: number, on: boolean, o: { def?: string; human?: Side } = {}): LandBattle {
  const t = withTeams(sampleTicket('forest', 1.3, { seed, nat: { att: 'USA', def: 'GER' }, style: { att: 'methodical', def: o.def ?? 'methodical' } }));
  if (o.human) t[o.human].human = true;
  const b = new LandBattle(t);
  b.ai.att = b.ai.def = true;
  b.hideOn();
  b.patrol = on;
  return b;
}

// ---- (1) off: the hide-on battles of ec6f8e9c --------------------------------------------------------------------------

/** A patrol order from his first block on foot, refused off as an unknown order is (and by the defence, always). */
const refused: boolean[] = [];
const patrolOrder = (b: LandBattle, side: Side = 'att'): void => {
  const u = b.units.find((x) => x.side === side && x.kind === 'inf' && !x.team);
  if (u) refused.push(b.command({ do: 'land:patrol', unit: u.id }, side));
};
/** Battles with hiding on, fought on ec6f8e9c (scratch pin test, this same signed()): each with its orders before each step. */
const CASES: [string, (patrol: boolean) => LandBattle, ((b: LandBattle, k: number) => void)?][] = [
  ['the computer\'s attack on the computer\'s defence, Britain on Germany, forest 1.3 seed 3, teams', (patrol) => {
    const b = new LandBattle(withTeams(sampleTicket('forest', 1.3, { seed: 3, nat: { att: 'UK', def: 'GER' } })));
    b.ai.att = b.ai.def = true; b.patrol = patrol; b.hideOn();
    return b;
  }],
  ['his attack placing his army, America on Germany, forest 1.2 seed 5', (patrol) => {
    const b = new LandBattle(sampleTicket('forest', 1.2, { seed: 5, humanAtt: true, nat: { att: 'USA', def: 'GER' } }));
    b.ai.att = b.ai.def = true; b.perUnit = true; b.patrol = patrol; b.hideOn(); b.setupOn('att', true);
    return b;
  }, (b, k) => { if (k === 0) { patrolOrder(b); b.command({ do: 'land:begin' }, 'att'); } }],
  ['his defence under fog, Germany on the Soviets, hills 1.2 seed 8', (patrol) => {
    const t = sampleTicket('hills', 1.2, { seed: 8, nat: { att: 'GER', def: 'SOV' } });
    t.def.human = true;
    const b = new LandBattle(t);
    b.ai.att = b.ai.def = true; b.fog = true; b.patrol = patrol; b.hideOn(); b.setupOn('def', true);
    return b;
  }, (b, k) => { if (k === 0) { patrolOrder(b, 'def'); b.command({ do: 'land:begin' }, 'def'); } }],
  ['a hedged field, America on Germany in August 1944, plains 1.1 seed 3', (patrol) => {
    const b = new LandBattle({ ...sampleTicket('plains', 1.1, { seed: 3, nat: { att: 'USA', def: 'GER' } }), turn: 60 });
    b.ai.att = b.ai.def = true; b.patrol = patrol;
    const H = practiceHedges(b.field.w, b.field.h);
    b.hedgesOn(H.hedges, H.gates); b.hideOn();
    return b;
  }],
];
const BEFORE: Record<string, [number, number, string]> = {
  'the computer\'s attack on the computer\'s defence, Britain on Germany, forest 1.3 seed 3, teams': [1.19, 360, '4a29939968351064'],
  'his attack placing his army, America on Germany, forest 1.2 seed 5': [1.44, 325, '074c15b3abbd5392'],
  'his defence under fog, Germany on the Soviets, hills 1.2 seed 8': [1.071, 258, '90670b1f48931900'],
  'a hedged field, America on Germany in August 1944, plains 1.1 seed 3': [1.034, 227, 'a5994e6d7248e2c6'],
};

describe('(1) off: the battles of ec6f8e9c, hiding on, his patrol orders refused as an unknown order is', () => {
  for (const [name, make, before] of CASES) {
    it(name, () => {
      refused.length = 0;
      const b = make(false);
      const got = pin(b, signed(b, before));
      if (PRINT) { env?.stdout?.write(`    ${JSON.stringify(name)}: ${JSON.stringify(got)},\n`); return; }
      expect(got).toEqual(BEFORE[name]);
      expect(refused.every((x) => x === false)).toBe(true);
      expect(b.nightPatrol).toBeNull();
      expect(b.log.some((e) => e.what === 'land:patrol' || e.extra?.how === 'patrol')).toBe(false);
      expect(Object.keys(b.view())).not.toContain('patrol');
      expect(b.patrolWhy(b.units.find((u) => u.side === 'att' && onFoot(u))!)).toBe('There are no night patrols in this battle.');
    }, 300_000);
  }
  it('his attack with the patrol on but no order given: no patrol goes out, and the battle is that of ec6f8e9c', () => {
    const name = CASES[1][0], b = CASES[1][1](true);
    const got = pin(b, signed(b, (x, k) => { if (k === 0) x.command({ do: 'land:begin' }, 'att'); }));
    expect(got).toEqual(BEFORE[name]);
    expect(b.nightPatrol).toBeNull();
  }, 300_000);
});

// ---- (2) on with hiding off: the same battle, nothing drawn -------------------------------------------------------------

describe('(2) on with hiding off: the patrol does nothing and draws nothing', () => {
  it('a computer\'s attack (which would always send one) fights the same battle with the same draws, every stream counted', () => {
    const run = (on: boolean): { s: string; n: number; b: LandBattle } => {
      const spy = vi.spyOn(Rng.prototype, 'next');
      try {
        const b = new LandBattle(withTeams(sampleTicket('forest', 1.3, { seed: 3, nat: { att: 'UK', def: 'GER' } })));
        b.ai.att = b.ai.def = true; b.patrol = on;
        const s = signed(b);
        return { s, n: spy.mock.calls.length, b };
      } finally { spy.mockRestore(); }
    };
    const off = run(false), on = run(true);
    expect(on.s).toBe(off.s);
    expect(on.n).toBe(off.n);
    expect(on.n).toBeGreaterThan(0);
    expect(on.b.nightPatrol).toBeNull();
    expect(Object.keys(on.b.view())).not.toContain('patrol');
  }, 300_000);
  it('his patrol order is refused, and says why', () => {
    const b = new LandBattle(sampleTicket('forest', 1.2, { seed: 5, humanAtt: true }));
    b.patrol = true; b.setupOn('att', true);
    const u = b.units.find((x) => x.side === 'att' && onFoot(x))!;
    expect(b.patrolWhy(u)).toBe('Nothing hides in this battle, so there is nothing for a patrol to find.');
    expect(b.command({ do: 'land:patrol', unit: u.id }, 'att')).toBe(false);
  });
});

// ---- (3) found -----------------------------------------------------------------------------------------------------------

describe('(3) found: the most forward hidden post is found before the first think, planned against, and never springs', () => {
  // Seeds where the post it finds falls in the attack's main effort (3, 5, 6), or in its pinning line's lanes (1).
  for (const seed of [1, 3, 5, 6]) {
    it(`forest seed ${seed}`, () => {
      odds(1, 0);
      const on = forest(seed, true), off = forest(seed, false);
      on.step(); off.step();
      // The post: the defence's most forward hidden block as the clock started (nearest the attack's start, ties by id).
      const want = off.units.filter((u) => u.side === 'def' && u.hid).sort((a, c) => a.y - c.y || a.id - c.id)[0];
      const p = on.nightPatrol!;
      expect(p.found?.unit).toBe(want.id);
      const post = on.unit(want.id)!;
      expect(post.hid).toBeUndefined();
      expect([p.found!.x, p.found!.y, p.found!.kind]).toEqual([Math.round(post.x * 10) / 10, Math.round(post.y * 10) / 10, post.kind]);
      const f = said(on, 'land:found').find((e) => e.unit === post.id)!;
      expect([f.t, f.extra?.how, f.extra?.by]).toEqual([0, 'patrol', p.unit]);
      // The attack's first plan counts it: its main effort, or a block of its line sent at it. Off, still hidden, nothing does.
      const planned = (b: LandBattle): boolean => b.plan.att!.main.includes(post.id) || b.units.some((u) => u.side === 'att' && u.order?.target === post.id);
      expect(planned(on)).toBe(true);
      expect(planned(off)).toBe(false);
      // Not caught: nothing else.
      expect([p.alerted, p.post, p.lost, p.flares.length]).toEqual([false, null, 0, 0]);
      toEnd(on);
      expect(post.sprung).toBeUndefined();
      expect(said(on, 'land:found').filter((e) => e.unit === post.id)).toHaveLength(1);
    }, 300_000);
  }
  it('the patrol goes out from the computer attack\'s most forward block of the line on foot, ties by id', () => {
    const b = forest(2, true);
    // As they stand when the clock starts.
    const att = b.units.filter((u) => u.side === 'att' && onFoot(u)).map((u) => ({ id: u.id, y: u.y }));
    b.step();
    const front = Math.max(...att.map((u) => u.y));
    expect(b.nightPatrol!.unit).toBe(Math.min(...att.filter((u) => u.y === front).map((u) => u.id)));
    expect(b.nightPatrol!.side).toBe('att');
    expect(b.nightPatrol!.men).toBe(patrolSize('USA'));
  });
  it('found with no hidden post (the desert, no cover): nothing is found, and his attack is told it came back with nothing', () => {
    odds(1, 0);
    const b = new LandBattle(sampleTicket('desert', 1.2, { seed: 4, humanAtt: true }));
    b.ai.att = b.ai.def = true; b.patrol = true; b.hideOn(); b.setupOn('att', true);
    const u = b.units.find((x) => x.side === 'att' && onFoot(x))!;
    expect(b.command({ do: 'land:patrol', unit: u.id }, 'att')).toBe(true);
    b.command({ do: 'land:begin' }, 'att');
    b.step();
    expect(b.units.some((d) => d.side === 'def' && d.hid)).toBe(false);
    expect(b.nightPatrol!.unit).toBe(u.id);
    expect(b.nightPatrol!.found).toBeNull();
    expect(said(b, 'land:found')).toHaveLength(0);
    expect(said(b, 'land:patrol').map((e) => [e.side, e.unit, e.text])).toEqual([['att', u.id, 'Our patrol came back with nothing to report.']]);
  });
});

// ---- (4) caught ----------------------------------------------------------------------------------------------------------

describe('(4) caught: one more hidden post, the computer\'s reserve forward at once, the patrol\'s men lost', () => {
  it('forest seed 1, both the computer\'s: the defence lays one more post than off', () => {
    odds(0, 1);
    const on = forest(1, true), off = forest(1, false);
    on.step(); off.step();
    const p = on.nightPatrol!;
    expect(p.alerted).toBe(true);
    expect(p.found).toBeNull();
    const hidden = (b: LandBattle): number[] => b.units.filter((u) => u.side === 'def' && u.hid).map((u) => u.id);
    expect(hidden(on).length).toBe(hidden(off).length + 1);
    expect(p.post).not.toBeNull();
    const post = on.unit(p.post!)!, was = off.unit(p.post!)!;
    expect(hidden(off)).not.toContain(post.id);
    expect(post.hid).toBe(true);
    expect(post.home).toEqual([post.x, post.y]);
    expect(Math.hypot(post.x - was.x, post.y - was.y)).toBeGreaterThan(5);
    // Every other block of the defence stands where it does off.
    for (const u of on.units.filter((x) => x.side === 'def' && x.id !== post.id)) expect([u.x, u.y, !!u.hid]).toEqual([off.unit(u.id)!.x, off.unit(u.id)!.y, !!off.unit(u.id)!.hid]);
    // Nobody is told: neither side is the player's.
    expect(said(on, 'land:patrol')).toHaveLength(0);
  });
  it('the patrol\'s men come off its block\'s strength, never its nerve; three flares over where it was caught', () => {
    odds(0, 1);
    const on = forest(1, true), off = forest(1, false);
    on.step(); off.step();
    const p = on.nightPatrol!, u = on.unit(p.unit)!, u0 = off.unit(p.unit)!;
    const lost = (patrolSize('USA') * u.str0) / u.men0;
    expect(p.men).toBe(4);
    expect(p.lost).toBeCloseTo(lost, 12);
    expect(u0.str - u.str).toBeCloseTo(lost, 12);
    expect(u.mor).toBe(u0.mor);
    expect(p.flares).toHaveLength(3);
    for (const [t, x, y] of p.flares) {
      expect(t).toBeGreaterThanOrEqual(LAND.patrol.night.t0);
      expect(t).toBeLessThanOrEqual(LAND.patrol.night.t1);
      expect(Math.abs(x - p.to[0])).toBeLessThanOrEqual(120);
      expect(Math.abs(y - p.to[1])).toBeLessThanOrEqual(60);
    }
    expect(p.flares.map((f) => f[0])).toEqual([...p.flares.map((f) => f[0])].sort((a, c) => a - c));
    // Counted as lost in the day's tally, before a shot is fired.
    const a = forest(1, true), z = forest(1, false);
    inside(a).hideStart(); inside(z).hideStart();
    const blk = a.unit(a.nightPatrol!.unit)!;
    const all = a.units.filter((x) => x.side === 'att').reduce((t, x) => t + x.power0, 0);
    expect(a.lostShares().att - z.lostShares().att).toBeCloseTo((blk.power0 * a.nightPatrol!.lost) / blk.str0 / all, 12);
    expect(a.lostShares().def).toBe(z.lostShares().def);
  });
  it('a methodical computer defence\'s held reserve walks toward the attack before its line is struck, as a bold one does; off, it stands', () => {
    odds(0, 1);
    const walked = (on: boolean): { moved: boolean; shift: boolean } => {
      const b = forest(1, on);
      let moved = false, shift = false;
      const held = b.units.filter((u) => u.side === 'def' && u.reserve === 'held').map((u) => u.id);
      expect(held.length).toBeGreaterThan(0);
      for (let k = 0; k < 900 && !b.over; k++) {
        b.step();
        const struck = b.units.some((u) => u.side === 'def' && u.kind !== 'art' && u.reserve !== 'held' && u.engaged !== undefined);
        if (struck) break;
        if (held.some((id) => b.shifted.has(id))) moved = true;
        if (b.view().plan?.def?.phase === 'shift') shift = true;
      }
      return { moved, shift };
    };
    expect(walked(true)).toEqual({ moved: true, shift: true });
    expect(walked(false)).toEqual({ moved: false, shift: false });
  }, 300_000);
});

// ---- (5) his attack: only by his order -----------------------------------------------------------------------------------

function rig(): { g: Game; TC: ReturnType<typeof battleCalls> } {
  const g = newCampaign();
  const TC = battleCalls({ game: () => g, attack: (ids, to, f) => ({ report: orderAttack(g, ids.map((i) => g.corps.find((c) => c.id === i)!), g.regions[to], f) }) });
  return { g, TC };
}
type V = { units: { id: number; side: string; kind: string; team?: string }[]; patrol?: { side: string; unit?: number; men: number; t0: number; t1: number; found: unknown; alerted: boolean; flares: unknown[] }; events: FieldEvent[] };

describe('(5) his attack sends a patrol only by his order while he places his army (TC.battlePatrolWhy)', () => {
  it('through the bridge: the order taken, one a night, refused in plain words, and it goes out as the clock starts', () => {
    const { g, TC } = rig();
    const o = offers(g)[0], ids = o.corps.map((c) => c.id);
    const st = TC.battleStart(ids, o.target.id, { setup: true, hide: true, patrol: true })!;
    const v = st.view as unknown as V;
    const mine = v.units.filter((u) => u.side === 'att'), foe = v.units.find((u) => u.side === 'def')!;
    const foot = mine.filter((u) => !u.team && MOTION[u.kind as keyof typeof MOTION] === 'foot');
    const other = mine.find((u) => u.team || MOTION[u.kind as keyof typeof MOTION] !== 'foot');
    expect(foot.length).toBeGreaterThan(1);
    expect(TC.battlePatrolWhy(foe.id).why).toBe('Only your own troops take your orders.');
    expect(TC.battlePatrolWhy(99999).why).toBe('Nothing to order.');
    if (other) expect(TC.battlePatrolWhy(other.id).why).toBe('Only men on foot go out on patrol.');
    expect(TC.battlePatrolWhy(foot[0].id).why).toBeNull();
    expect(TC.battleOrder({ do: 'land:patrol', unit: foot[0].id }).ok).toBe(true);
    expect(TC.battlePatrolWhy(foot[1].id).why).toBe('One patrol a night: one is already going out.');
    expect(TC.battleOrder({ do: 'land:patrol', unit: foot[1].id }).ok).toBe(false);
    TC.battleBegin();
    expect(TC.battlePatrolWhy(foot[1].id).why).toBe('A patrol is sent while you place your army, before the battle begins.');
    const v1 = TC.battleTick(1) as unknown as V;
    expect(TC.battlePatrolWhy(foot[1].id).why).toBe('The battle has begun: a patrol goes out the night before.');
    expect(v1.patrol).toBeDefined();
    expect([v1.patrol!.side, v1.patrol!.unit, v1.patrol!.t0, v1.patrol!.t1]).toEqual(['att', foot[0].id, LAND.patrol.night.t0, LAND.patrol.night.t1]);
    // His attack is told what came of it (found: the post's own land:found).
    const told = v1.events.filter((e) => e.what === 'land:patrol' || (e.what === 'land:found' && e.extra?.how === 'patrol')).map((e) => e.text);
    expect(told.length).toBeGreaterThan(0);
    for (const t of told) expect(['Our night patrol found a hidden post.', 'Our patrol was caught in the night. They know we\'re coming.', 'Our patrol came back with nothing to report.']).toContain(t);
  });
  it('through the bridge with no order: none goes out', () => {
    const { g, TC } = rig();
    const o = offers(g)[0];
    TC.battleStart(o.corps.map((c) => c.id), o.target.id, { setup: true, hide: true, patrol: true });
    TC.battleBegin();
    const v = TC.battleTick(1) as unknown as V;
    expect(v.patrol).toBeUndefined();
    expect(v.events.some((e) => e.what === 'land:patrol')).toBe(false);
  });
  it('his staff fighting his whole attack sends none either; every refusal in words', () => {
    const b = new LandBattle(withTeams(sampleTicket('forest', 1.2, { seed: 5, humanAtt: true })));
    b.ai.att = b.ai.def = true;
    const u = b.units.find((x) => x.side === 'att' && onFoot(x))!, d = b.units.find((x) => x.side === 'def' && onFoot(x))!;
    const gun = b.units.find((x) => x.side === 'att' && x.kind === 'art')!, tank = b.units.find((x) => x.side === 'att' && x.kind === 'arm')!;
    expect(gun.team).toBeDefined();
    expect(b.patrolWhy(u)).toBe('There are no night patrols in this battle.');
    b.patrol = true;
    expect(b.patrolWhy(u)).toBe('Nothing hides in this battle, so there is nothing for a patrol to find.');
    b.hideOn();
    expect(b.patrolWhy(d)).toBe('Only the attacker sends out a patrol.');
    expect(b.patrolWhy(u)).toBe('A patrol is sent while you place your army, before the battle begins.');
    expect(b.command({ do: 'land:patrol', unit: u.id }, 'att')).toBe(false);
    b.setupOn('att', true);
    expect(b.patrolWhy(gun)).toBe('Only men on foot go out on patrol.');
    expect(b.patrolWhy(tank)).toBe('Only men on foot go out on patrol.');
    expect(b.command({ do: 'land:patrol', unit: gun.id }, 'att')).toBe(false);
    expect(b.command({ do: 'land:patrol', unit: u.id }, 'def')).toBe(false);
    expect(b.patrolWhy(u)).toBeNull();
    b.command({ do: 'land:begin' }, 'att');
    steps(b, 20);
    expect(b.nightPatrol).toBeNull();
    expect(b.patrolWhy(u)).toBe('The battle has begun: a patrol goes out the night before.');
  });
  it('his attack is told each outcome in plain words', () => {
    const told = (found: number, caught: number): string[] => {
      odds(found, caught);
      const b = forest(3, true, { human: 'att' });
      b.setupOn('att', true);
      expect(b.command({ do: 'land:patrol', units: [b.units.find((x) => x.side === 'att' && onFoot(x))!.id] }, 'att')).toBe(true);
      b.command({ do: 'land:begin' }, 'att');
      b.step();
      return b.log.filter((e) => e.what === 'land:patrol' || (e.what === 'land:found' && e.extra?.how === 'patrol')).map((e) => e.text);
    };
    expect(told(1, 0)).toEqual(['Our night patrol found a hidden post.']);
    expect(told(0, 1)).toEqual(['Our patrol was caught in the night. They know we\'re coming.']);
    expect(told(1, 1)).toEqual(['Our night patrol found a hidden post.', 'Our patrol was caught in the night. They know we\'re coming.']);
    expect(told(0, 0)).toEqual(['Our patrol came back with nothing to report.']);
  });
});

// ---- (6) his defence alerted ---------------------------------------------------------------------------------------------

describe('(6) his defence, alerted: its extra post laid for him and named in the warning; his reserve his own to move', () => {
  it('forest seed 1, his staff fighting it', () => {
    odds(0, 1);
    const b = forest(1, true, { human: 'def' }), off = forest(1, false, { human: 'def' });
    b.step(); off.step();
    const p = b.nightPatrol!;
    expect(p.alerted).toBe(true);
    expect(p.post).not.toBeNull();
    const post = b.unit(p.post!)!;
    expect(post.hid).toBe(true);
    expect(post.side).toBe('def');
    expect(off.unit(post.id)!.hid).toBeUndefined();
    const w = said(b, 'land:patrol');
    expect(w).toHaveLength(1);
    expect([w[0].side, w[0].unit, w[0].t]).toEqual(['def', post.id, 0]);
    expect(w[0].text).toBe(`We caught an enemy patrol in the night. An attack is coming. ${post.name} has taken a hidden post in the woods.`);
    // The patrol, as his defence sees it: where it caught it, and the flares; never the block it came from.
    const seen = b.view('def').patrol!;
    expect(seen).toEqual({ side: 'att', t0: p.t0, t1: p.t1, men: p.men, to: p.to, found: null, alerted: true, flares: p.flares });
    expect(b.view('att').patrol!.unit).toBe(p.unit);
    expect(b.view().patrol!.unit).toBe(p.unit);
    // His reserve is his to move: no walk by the rule, whatever his staff does.
    expect(b.alertedStaff('def')).toBe(false);
    for (let k = 0; k < 600 && !b.over; k++) {
      b.step();
      if (b.units.some((u) => u.side === 'def' && u.kind !== 'art' && u.reserve !== 'held' && u.engaged !== undefined)) break;
      expect(b.units.some((u) => u.side === 'def' && u.reserve === 'held' && b.shifted.has(u.id))).toBe(false);
    }
  });
  it('not caught: his defence sees nothing of it', () => {
    odds(1, 0);
    const b = forest(1, true, { human: 'def' });
    b.step();
    expect(b.nightPatrol!.alerted).toBe(false);
    expect(b.view('def').patrol).toBeUndefined();
    expect(b.view('att').patrol).toBeDefined();
    expect(said(b, 'land:patrol')).toHaveLength(0);
    // His post found: told to him as it is found.
    const f = said(b, 'land:found').find((e) => e.extra?.how === 'patrol')!;
    expect(f.text).toBe(`An enemy patrol found ${b.unit(f.unit!)!.name} in the woods in the night.`);
  });
});

// ---- (7) replays exactly per seed ----------------------------------------------------------------------------------------

describe('(7) each seed replays exactly: its own stream, two draws, found then caught', () => {
  it('the same battle twice is the same battle', () => {
    const a = forest(4, true), b = forest(4, true);
    expect(signed(a)).toBe(signed(b));
  }, 300_000);
  it('found and caught are the first and second draws of patrol:<seed>[:attempt], whatever the battle', () => {
    let found = 0, caught = 0;
    const N = 60;
    for (let seed = 1; seed <= N; seed++) for (const attempt of [undefined, 2]) {
      const t = withTeams(sampleTicket('forest', 1.3, { seed, nat: { att: 'USA', def: 'GER' } }));
      if (attempt) t.attempt = attempt;
      const b = new LandBattle(t);
      b.ai.att = b.ai.def = true; b.hideOn(); b.patrol = true;
      inside(b).hideStart();
      const r = new Rng(hashString(attempt ? `patrol:${seed}:${attempt}` : `patrol:${seed}`));
      const f = r.next() < LAND.patrol.found, c = r.next() < LAND.patrol.caught;
      const p = b.nightPatrol!;
      expect(p.alerted).toBe(c);
      // Found: the most forward hidden post, where there is one (the forest's defence always has one here).
      expect(b.units.some((u) => u.side === 'def' && u.hid) || p.found !== null).toBe(true);
      expect(p.found !== null).toBe(f);
      if (!attempt) { found += f ? 1 : 0; caught += c ? 1 : 0; }
    }
    expect(found / N).toBeGreaterThan(0.2); expect(found / N).toBeLessThan(0.6);
    expect(caught / N).toBeGreaterThan(0.12); expect(caught / N).toBeLessThan(0.5);
  });
});

// ---- (8) never the battle's dice -----------------------------------------------------------------------------------------

describe('(8) nothing draws on the battle\'s dice', () => {
  it('found and caught both, settled as the clock starts: the battle\'s stream untouched', () => {
    odds(1, 1);
    for (const seed of [1, 2, 3]) {
      const b = forest(seed, true);
      const r = inside(b).rng, s0 = r.state;
      let n = 0;
      const next = r.next.bind(r);
      r.next = (): number => { n++; return next(); };
      inside(b).hideStart();
      expect(b.nightPatrol!.alerted).toBe(true);
      expect(b.nightPatrol!.found).not.toBeNull();
      expect(n).toBe(0);
      expect(r.state).toBe(s0);
    }
  });
});

// ---- (9) by army ---------------------------------------------------------------------------------------------------------

describe('(9) the patrol\'s men by army, every playable nation named', () => {
  it('as the readings give them, kin with Britain, Vichy with France, every other army four', () => {
    const want: Record<string, number> = { USA: 4, UK: 3, CAN: 3, AUS: 3, NZL: 3, SAF: 3, IND: 3, SOV: 4, JAP: 10, GER: 10, FRA: 2, VIC: 2, ITA: 4 };
    for (const [nat, n] of Object.entries(want)) { expect(patrolSize(nat)).toBe(n); expect(LAND.patrol.size[nat]).toBe(n); }
    for (const nat of ['POL', 'RED', 'BLU', 'FFR']) expect(patrolSize(nat)).toBe(4);
    expect([LAND.patrol.found, LAND.patrol.caught, LAND.patrol.night.t0, LAND.patrol.night.t1]).toEqual([0.4, 0.3, -2400, -600]);
  });
  it('a Japanese patrol caught costs its block a squad', () => {
    odds(0, 1);
    const t = withTeams(sampleTicket('forest', 1.3, { seed: 1, nat: { att: 'JAP', def: 'USA' } }));
    const b = new LandBattle(t);
    b.ai.att = b.ai.def = true; b.hideOn(); b.patrol = true;
    const u0 = b.units.map((u) => [u.id, u.str] as const);
    b.step();
    const p = b.nightPatrol!, u = b.unit(p.unit)!;
    expect(p.men).toBe(10);
    expect(u0.find(([id]) => id === u.id)![1] - u.str).toBeCloseTo((10 * u.str0) / u.men0, 12);
  });
});
