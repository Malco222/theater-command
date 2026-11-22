// Hidden in the woods (release 2; Brian, 22 November 04:18Z: "if you put units in the forest then they aren't visible to
// the enemy until you get close or they start shooting. this allows you to do ambushes", and 04:19Z "the AI will not
// know they are there"). Both sides alike, behind a switch (LandBattle.hideOn): off, no block is ever hidden.
import { describe, expect, it } from 'vitest';
import { battleCalls, unitOut } from '../unreal/TheaterCommand/Rules/battle-bridge';
import { LAND, LandBattle, sampleTicket, withTeams } from '../src/battle/land';
import { thinkFor } from '../src/battle/land/ai';
import { coverAt } from '../src/battle/land/field';
import type { LandUnit } from '../src/battle/land/units';
import { newCampaign, offers } from '../src/battle/ui/campaign';
import { orderAttack } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import type { Side } from '../src/battle/core/types';

const forest = (seed = 3, humanAtt = true): LandBattle => new LandBattle(withTeams(sampleTicket('forest', 1.3, { seed, humanAtt })));
const run = (b: LandBattle, secs: number, each?: () => void): void => { const end = b.t + secs; for (let k = 0; !b.over && b.t < end - 1e-9 && k < 100000; k++) { b.step(); each?.(); } };
const foot = (b: LandBattle, side: Side): LandUnit[] => b.units.filter((u) => u.side === side && (u.kind === 'inf' || u.kind === 'mtn') && u.state !== 'gone');
const woods = (b: LandBattle, x: number, y: number): boolean => coverAt(b.field, x, y) === 'woods';
/**
 * A spot in the side's zone where a block of this size stands wholly on woods, away from every other block; with
 * 'open', open ground for that many metres before it (toward the attack), where an enemy can stand to be ambushed.
 */
function woodsSpot(b: LandBattle, side: Side, u: LandUnit, open = 0): [number, number] | null {
  const z = b.field.zones[side];
  for (let y = z[1] + 60; y < z[3] - 60; y += 20) for (let x = z[0] + u.w / 2; x < z[2] - u.w / 2; x += 20) {
    const all = [[0, 0], [-u.w / 3, -u.d / 3], [u.w / 3, -u.d / 3], [-u.w / 3, u.d / 3], [u.w / 3, u.d / 3]].every(([dx, dy]) => woods(b, x + dx, y + dy));
    if (!all || !b.units.every((o) => o === u || Math.hypot(o.x - x, o.y - y) > 350)) continue;
    let clear = true;
    for (let k = 150; k <= open && clear; k += 20) if (woods(b, x, y - k)) clear = false;
    if (clear) return [x, y];
  }
  return null;
}
/** A practice field and a woods spot for the defence's first foot block with open ground before it. */
function withWoods(open: number): { b: LandBattle; spot: [number, number] } {
  for (const terrain of ['forest', 'hills', 'plains'] as const) for (const seed of [5, 1, 2, 3, 4, 6, 7, 8]) {
    const b = new LandBattle(withTeams(sampleTicket(terrain, 1.3, { seed, humanAtt: true })));
    const spot = woodsSpot(b, 'def', foot(b, 'def')[0], open);
    if (spot) return { b, spot };
  }
  throw new Error('no woods');
}
/** Stands 'a' straight in front of 'd' (toward the attack) with this many metres between their edges. */
function inFront(b: LandBattle, d: LandUnit, a: LandUnit, gap: number): void {
  a.x = d.x; a.face = 0;
  for (let off = 1200; off > 0; off -= 1) { a.y = d.y - off; if (b.gap(a, d) <= gap) break; }
  a.home = [a.x, a.y];
}

describe('off, nothing changes', () => {
  it('no block is ever hidden, and the switch cannot be thrown once the clock runs', () => {
    const b = forest();
    b.ai.att = b.ai.def = true;
    run(b, 60, () => expect(b.units.some((u) => u.hid)).toBe(false));
    expect(b.log.some((e) => e.what === 'land:found')).toBe(false);
    expect(b.hideOn()).toBe(false);
    expect(b.hide).toBe(false);
  });
});

describe('who is hidden', () => {
  it('every block standing in woods when the clock starts, either side; shown so as he places his', () => {
    const b = forest();
    expect(b.setupOn('att', true)).toBe(true);
    expect(b.hideOn()).toBe(true);
    const f = foot(b, 'att')[0];
    const z = b.setupRoom()!;
    let wood: [number, number] | null = null, open: [number, number] | null = null;
    for (let x = z[0] + 120; x < z[2] - 120 && (!wood || !open); x += 20) for (let y = z[1] + 60; y < z[3] - 60; y += 20) {
      if (b.placeWhy(f, x, y) !== null) continue;
      if (!wood && woods(b, x, y)) wood = [x, y];
      if (!open && !woods(b, x, y)) open = [x, y];
    }
    expect(wood && open).toBeTruthy();
    expect(b.command({ do: 'land:place', units: [f.id], x: wood![0], y: wood![1] }, 'att')).toBe(true);
    expect(f.hid).toBe(true);
    expect(b.command({ do: 'land:place', units: [f.id], x: open![0], y: open![1] }, 'att')).toBe(true);
    expect(f.hid).toBeUndefined();
    b.command({ do: 'land:begin' }, 'att');
    b.step();
    for (const u of b.units) {
      if (u.kind === 'fort') expect(u.hid).toBeUndefined();
      else if (!woods(b, u.x, u.y)) expect(u.hid).toBeUndefined();
    }
    expect(b.units.filter((u) => u.hid).length).toBeGreaterThan(0);
  });
});

describe('an ambush', () => {
  /** A defending foot block hidden in woods, and an attacking one standing in the open before it. */
  function ambush(gap: number, hide = true): { b: LandBattle; d: LandUnit; a: LandUnit } {
    const { b, spot } = withWoods(450);
    b.ai.att = b.ai.def = false;
    const d = foot(b, 'def')[0], a = foot(b, 'att')[0];
    [d.x, d.y] = spot;
    d.home = [d.x, d.y];
    inFront(b, d, a, gap);
    expect(woods(b, a.x, a.y)).toBe(false);
    if (hide) b.hideOn();
    return { b, d, a };
  }

  it('holds its fire till the enemy is close (in true forest, LAND.hide.forest), and nobody strikes it meanwhile', () => {
    const { b, d, a } = ambush(180);
    expect(d.hid).toBe(true);
    expect(b.gap(a, d)).toBeLessThan(LAND.reach.inf);
    expect(b.targetOf(d)).toBeNull();
    expect(b.targetOf(a)?.id).not.toBe(d.id);
    // Within the hedgerow distance but not the forest's: still holds.
    inFront(b, d, a, 120);
    expect(b.targetOf(d)).toBeNull();
    inFront(b, d, a, LAND.hide.forest - 20);
    expect(b.targetOf(d)?.id).toBe(a.id);
  });

  it('springs on the enemy at close range: it is found, says so, and its first fire strikes harder', () => {
    const on = ambush(80), off = ambush(80, false);
    on.b.step();
    expect(on.d.hid).toBeUndefined();
    expect(on.d.sprung).toBe(0);
    const ev = on.b.log.find((e) => e.what === 'land:found' && e.unit === on.d.id)!;
    expect((ev.extra as { how: string }).how).toBe('fired');
    expect(ev.text).toBe(`Enemy troops hidden in the woods open fire on your ${on.a.name}.`);
    run(on.b, 15); off.b.step(); run(off.b, 15);
    expect(on.a.mor).toBeLessThan(off.a.mor);
    expect(on.a.str).toBeLessThan(off.a.str);
  });

  it('is found when it leaves the woods', () => {
    const { b, d } = ambush(600);
    const z = b.field.zones.def;
    let to: [number, number] | null = null;
    for (let y = z[1] + 60; y < z[3] - 60 && !to; y += 20) for (let x = z[0] + 150; x < z[2] - 150 && !to; x += 20) if (!woods(b, x, y) && Math.hypot(x - d.x, y - d.y) < 900) to = [x, y];
    expect(b.command({ do: 'move', units: [d.id], x: to![0], y: to![1] }, 'def')).toBe(true);
    run(b, 400, () => { if (d.hid) expect(woods(b, d.x, d.y)).toBe(true); });
    expect(d.hid).toBeUndefined();
    expect((b.log.find((e) => e.what === 'land:found' && e.unit === d.id)!.extra as { how: string }).how).toBe('out');
  });

  it('is found by an enemy that comes close enough to see it (guns on the move, which do not fire)', () => {
    const b = forest(5);
    b.ai.att = b.ai.def = false;
    const g = b.units.find((u) => u.side === 'def' && u.kind === 'art' && !u.team)!, a = foot(b, 'att')[0];
    [g.x, g.y] = woodsSpot(b, 'def', g)!;
    g.home = [g.x, g.y];
    inFront(b, g, a, LAND.hide.sight.guns - 20);
    b.hideOn();
    expect(g.hid).toBe(true);
    // Limbered up and moving a little way through the woods.
    const to = [[30, 0], [-30, 0], [0, 30], [0, -30]].map(([dx, dy]) => [g.x + dx, g.y + dy]).find(([x, y]) => woods(b, x, y))!;
    b.command({ do: 'move', units: [g.id], x: to[0], y: to[1] }, 'def');
    b.step();
    expect(g.hid).toBeUndefined();
    expect((b.log.find((e) => e.what === 'land:found' && e.unit === g.id)!.extra as { how: string }).how).toBe('seen');
  });
});

describe('the computer does not know they are there', () => {
  it('nobody is struck, sent at, or planned against while hidden; its staff keeps its hidden line where it is', () => {
    for (const seed of [1, 2, 3]) {
      const b = new LandBattle(withTeams(sampleTicket('forest', 1.3, { seed, style: { att: 'methodical', def: 'bold' } })));
      b.ai.att = b.ai.def = true;
      b.hideOn();
      let saw = 0;
      run(b, 3000, () => {
        for (const u of b.units) {
          // Only fire on cover (reconnaissance by fire, once its attack has been ambushed) lands on a block still hidden.
          if (u.engaged !== undefined && b.unit(u.engaged)!.hid) { expect(u.search).toBeDefined(); expect(b.ambushed.has(u.side)).toBe(true); }
          if (u.order?.kind === 'attack') expect(b.unit(u.order.target!)!.hid).toBeUndefined();
          if (u.aim !== undefined) expect(b.unit(u.aim)!.hid).toBeUndefined();
          // (Broken by blind fire while still hidden, it may fall back on its own.)
          if (u.side === 'def' && u.hid && u.reserve !== 'held') { expect(u.order === null || u.order.kind === 'withdraw').toBe(true); saw++; }
        }
        for (const id of b.plan.att?.main ?? []) expect(b.unit(id)!.hid).toBeUndefined();
      });
      expect(b.over).toBeTruthy();
      expect(saw).toBeGreaterThan(0);
    }
  }, 300_000);

  it('an attack that sees nobody goes forward to find them', () => {
    const b = forest(3, false);
    for (const u of b.units) if (u.side === 'def') u.hid = true;
    thinkFor(b, 'att');
    const goers = b.units.filter((u) => u.side === 'att' && u.kind !== 'art' && u.kind !== 'fort');
    expect(goers.length).toBeGreaterThan(0);
    for (const u of goers) {
      expect(u.order?.kind).toBe('move');
      expect(u.order!.y).toBeGreaterThan(u.y + 100);
    }
    for (const u of b.units.filter((x) => x.side === 'att' && x.kind === 'art')) expect(u.order).toBeNull();
  });

  it('its defence lays a block or two in the woods on the way in before the clock starts; never the player\'s', () => {
    let laid = 0;
    for (const seed of [1, 2, 3, 4]) {
      const b = forest(seed, false);
      b.ai.att = b.ai.def = true;
      b.hideOn();
      const at = new Map(b.units.map((u) => [u.id, [u.x, u.y]]));
      b.step();
      const moved = b.units.filter((u) => u.side === 'def' && (Math.abs(at.get(u.id)![0] - u.x) > 5 || Math.abs(at.get(u.id)![1] - u.y) > 5));
      expect(moved.length).toBeLessThanOrEqual(LAND.hide.staff.blocks);
      for (const u of moved) { expect(woods(b, u.x, u.y)).toBe(true); expect(u.hid).toBe(true); expect(u.home).toEqual([u.x, u.y]); }
      laid += moved.length;
      // The player defending: his staff never moves what he drew up.
      const t = withTeams(sampleTicket('forest', 1.3, { seed }));
      t.def.human = true;
      const p = new LandBattle(t);
      p.ai.att = p.ai.def = true;
      p.hideOn();
      const pat = new Map(p.units.map((u) => [u.id, [u.x, u.y]]));
      p.step();
      expect(p.units.filter((u) => u.side === 'def' && Math.abs(pat.get(u.id)![0] - u.x) + Math.abs(pat.get(u.id)![1] - u.y) > 5).length).toBe(0);
    }
    expect(laid).toBeGreaterThan(0);
  });
});

describe('his orders', () => {
  it('cannot send a block or lay guns on what his side has not found', () => {
    const b = forest(5);
    b.ai.att = b.ai.def = false;
    const d = foot(b, 'def')[0];
    [d.x, d.y] = woodsSpot(b, 'def', d)!;
    b.hideOn();
    b.step();
    expect(d.hid).toBe(true);
    const mine = foot(b, 'att')[0], gun = b.units.find((u) => u.side === 'att' && u.kind === 'art')!;
    expect(b.command({ do: 'attack', units: [mine.id], target: d.id }, 'att')).toBe(false);
    expect(b.command({ do: 'land:fire', units: [gun.id], target: d.id }, 'att')).toBe(false);
    expect(b.sees('att', d)).toBe(false);
    expect(b.sees('def', d)).toBe(true);
    delete d.hid;
    expect(b.command({ do: 'attack', units: [mine.id], target: d.id }, 'att')).toBe(true);
  });
});

describe('the 3D view', () => {
  it('a hidden block carries hid: 1, whoever sees it; battleStart opens with it on request', () => {
    const b = forest(5);
    const d = foot(b, 'def')[0];
    [d.x, d.y] = woodsSpot(b, 'def', d)!;
    b.hideOn();
    expect(unitOut(d).hid).toBe(1);
    for (const u of b.units.filter((x) => !x.hid)) expect(unitOut(u).hid).toBeUndefined();
    // Through the bridge, from the campaign.
    const g: Game = newCampaign();
    const TC = battleCalls({ game: () => g, attack: (ids, to, f) => ({ report: orderAttack(g, ids.map((i) => g.corps.find((c) => c.id === i)!), g.regions[to], f) }) });
    const o = offers(g)[0];
    const st = TC.battleStart(o.corps.map((c) => c.id), o.target.id, { hide: true })!;
    const f = st.field as unknown as { cols: number; cell: number; ground: number[]; grounds: string[] };
    type U = { kind: string; x: number; y: number; hid?: number; state: string };
    const inWoods = (u: U): boolean => f.grounds[f.ground[Math.floor(u.y / f.cell) * f.cols + Math.floor(u.x / f.cell)]] === 'woods';
    for (const u of (st.view as { units: U[] }).units) if (u.kind !== 'fort' && u.kind !== undefined) expect(u.hid === 1).toBe(inWoods(u));
  });
});
