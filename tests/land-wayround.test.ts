// Falling back round an enemy in the way (release 2, the row `wayround`, off; sim.roundRule; sim.ts wayRound).
//
// The pursuit gate's declared read (26 November 2026): a block ordered to fall back walked the straight line to its
// own edge, stepped into the ground of an enemy block standing on that line, was eased back by spread() as fast as it
// walked, and marked time there until the battle ended. The design director's ruling of 18:32Z:
//   - a row and a switch of its own, and off is today;
//   - it lives in move(), and every block falling back acts alike, either side, broken or in order;
//   - it walks round the nearer end of that block's box, by the boxes and LAND.apart, no new figure;
//   - the end is chosen once and kept while the enemy block stands there;
//   - with both ends closed it does as the field already has it.
//
// What is tested here is that, on a field set by hand as the three traced pursuits had it. A block marks time only
// where both of these hold, and the field below has both: it walks slower than spread() eases (foot in woods: 3.6 m
// a second against LAND.spread's 4; in the open it walks 6 and shoulders its way through), and the enemy block cannot
// be eased out of its way (it stands on a river's bank, and spread() eases nobody into the river off the bank). So:
// Prokhorovka's ground with a river laid across it, nobody's staff fighting, one foot block told to fall back through
// a wood, and one enemy foot block standing in that wood on the bank between it and its own edge, as a pursuer stands.
import { describe, expect, it } from 'vitest';
import { groundAt } from '../src/battle/land/field';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import { LAND } from '../src/battle/land/rules';
import { LandBattle } from '../src/battle/land/sim';
import type { Side } from '../src/battle/core/types';
import rulesTableCpp from '../unreal/TheaterCommand/Source/TheaterCommand/Battle/LandLive.cpp?raw';

type U = {
  id: number; side: Side; kind: string; team?: string; x: number; y: number; w: number; d: number; face: number; state: string; hid?: boolean; speed: number;
  order: { kind: string; x: number; y: number; target?: number } | null; path: [number, number][]; round?: { by: number; at: [number, number]; side: 1 | -1; over: [number, number] };
};
type Staged = { sim: LandBattle; u: U; e: U; others: U[]; ground: (x: number, y: number) => string };
const units = (sim: LandBattle): U[] => sim.units as unknown as U[];
/** How far from the enemy block the block falling back is set down, along its way home. */
const BACK = 120;

/**
 * The field. `falls` is the side whose first foot block `u` falls back to its own edge (the attack's is the south, the
 * defence's the north); the other side's first foot block `e` stands on the river's bank in the wood, on the side of
 * the water `u` comes from, `lean` metres east of `u`'s line, with a pursuer's order on `u`. `pocket`: two more of the
 * enemy's foot blocks stand at `u`'s shoulders where it meets `e`, one on the ground of each way round.
 */
function stage(on: boolean, lean: number, pocket = false, falls: Side = 'att'): Staged {
  const t = ticketOf(PIECES.find((x) => x.key === 'prokhorovka-1943')!, 7, null);
  const sim = new LandBattle({ ...t, place: { ...t.place, river: 'Psel' } } as never);
  sim.ai = { att: false, def: false };
  (sim as unknown as { roundRule: boolean }).roundRule = on;
  const f = (sim as unknown as { field: { h: number } }).field;
  const ground = (x: number, y: number): string => groundAt(f as never, x, y) as string;
  const foot = (s: Side): U[] => units(sim).filter((x) => x.side === s && x.kind === 'inf' && !x.team);
  const south = falls === 'att', home = south ? 0 : f.h, X = south ? 2450 : 2375, toward = south ? -1 : 1;
  const u = foot(falls)[0], [e, ...rest] = foot(south ? 'def' : 'att');
  // The bank on u's side of the water at e's place: the last dry metre before the river, coming from u's side.
  let bank = 0;
  for (let y = south ? 1300 : 700; south ? y > 700 : y < 1300; y += toward) if (ground(X + lean, y) === 'water') { bank = y - toward; break; }
  const put = (b: U, x: number, y: number, face: number): void => { b.x = x; b.y = y; b.face = face; b.hid = false; b.path = []; b.order = null; };
  put(e, X + lean, bank - toward, south ? 0 : Math.PI);
  put(u, X, e.y - toward * BACK, south ? Math.PI : 0);
  // (where u is held against e: e's half depth, its own, and LAND.apart from e's middle)
  const held = e.y - toward * (e.d / 2 + u.d / 2 + LAND.apart);
  const others = pocket ? rest.slice(0, 2) : [];
  others.forEach((o, i) => put(o, e.x + (i ? 1 : -1) * (Math.hypot(e.w, e.d) / 2 + u.w / 2 + 40), held, e.face));
  u.order = { kind: 'withdraw', x: u.x, y: home };
  e.order = { kind: 'attack', x: u.x, y: u.y, target: u.id };
  return { sim, u, e, others, ground };
}
/** Fights `seconds` on, and gives every side the block's way round took while it had one. */
function fight(sim: LandBattle, u: U, seconds: number): number[] {
  const sides: number[] = [];
  const until = sim.t + seconds;
  while (!sim.over && sim.t < until && u.state !== 'withdrawn' && u.state !== 'gone') { sim.step(); if (u.round) sides.push(u.round.side); }
  return sides;
}
/** Past the enemy block on its way home, or home already. */
const past = (u: U, e: U, falls: Side): boolean => u.state === 'withdrawn' || (falls === 'att' ? u.y < e.y - e.d / 2 : u.y > e.y + e.d / 2);

describe('falling back round an enemy in the way (wayround)', () => {
  it('the field is as the test needs it: a wood on the bank, the enemy block against the water', () => {
    for (const [falls, lean] of [['att', 20], ['att', -20], ['def', 0]] as [Side, number][]) {
      const { u, e, ground } = stage(false, lean, false, falls);
      const toward = falls === 'att' ? -1 : 1;
      expect(ground(e.x, e.y)).toBe('woods');
      // (the water is at e's back: spread() eases nobody into the river off its bank)
      expect(ground(e.x, e.y + 2 * toward)).toBe('water');
      // (u meets e in the wood, where foot walks slower than spread() eases)
      expect(ground(u.x, e.y - toward * (e.d / 2 + u.d / 2 + LAND.apart))).toBe('woods');
      expect(LAND.pace.foot.woods * LAND.speed.inf).toBeLessThan(LAND.spread);
      // (the attack's block has dry ground at both ends of the enemy's box; the defence's at one at least)
      const dry = [-1, 1].filter((s) => ground(e.x + s * 190, e.y - toward * 70) !== 'water').length;
      expect(dry).toBeGreaterThanOrEqual(falls === 'att' ? 2 : 1);
    }
  });

  it('off, a block falling back with an enemy block in its line marks time against it, as today', () => {
    const { sim, u, e } = stage(false, 20);
    fight(sim, u, 60);
    const y60 = u.y;
    fight(sim, u, 180);
    expect(u.state === 'steady' || u.state === 'wavering').toBe(true);
    expect(u.round).toBeUndefined();
    // (it never got past: still north of the enemy block's own ground, and where it was three minutes before)
    expect(past(u, e, 'att')).toBe(false);
    expect(Math.abs(u.y - y60)).toBeLessThan(1);
    expect(u.x).toBeCloseTo(2450, 6);
    // (and off, the row writes nothing on any block)
    for (const b of units(sim)) { expect('barred' in b).toBe(false); expect('round' in b).toBe(false); }
  });

  it('on, it steps round the nearer end and goes on to its own edge', () => {
    for (const lean of [20, -20]) {
      const { sim, u, e } = stage(true, lean);
      const x0 = u.x;
      const sides = fight(sim, u, 240);
      expect(sides.length).toBeGreaterThan(0);
      // (the nearer end: the enemy leans east of its line, so it goes round by the west, and the other way about;
      // across its way south, the side +1 is the west)
      expect(new Set(sides)).toEqual(new Set([lean > 0 ? 1 : -1]));
      expect(past(u, e, 'att')).toBe(true);
      if (u.state !== 'withdrawn') expect(lean > 0 ? u.x < x0 : u.x > x0).toBe(true);
    }
  });

  it('the end it chose is kept while that enemy block stands there', () => {
    // (the enemy all but square on its line: the nearer end is nearer by ten metres, and it never changes its mind)
    const { sim, u, e } = stage(true, 5);
    const sides = fight(sim, u, 240);
    expect(sides.length).toBeGreaterThan(0);
    expect(sides.every((s) => s === sides[0])).toBe(true);
    expect(past(u, e, 'att')).toBe(true);
  });

  it('with both ends closed by other enemy blocks it does as before, to the last figure', () => {
    const was = stage(false, 0, true), now = stage(true, 0, true);
    expect(was.others.length).toBe(2);
    const all = (s: LandBattle): unknown[] => units(s).map((x) => [x.id, x.x, x.y, x.face, x.state, (x as unknown as { str: number }).str]);
    // (45 seconds: held against the enemy block, a block at each shoulder; 180: whatever the field then does with it)
    for (const seconds of [45, 135]) {
      fight(was.sim, was.u, seconds);
      const sides = fight(now.sim, now.u, seconds);
      expect(sides.length).toBe(0);
      expect(all(now.sim)).toEqual(all(was.sim));
      if (seconds === 45) expect(past(now.u, now.e, 'att')).toBe(false);
    }
  });

  it('a block of the defence falling back north acts the same', () => {
    const off = stage(false, 0, false, 'def');
    fight(off.sim, off.u, 240);
    expect(off.u.state === 'steady' || off.u.state === 'wavering').toBe(true);
    expect(past(off.u, off.e, 'def')).toBe(false);
    const on = stage(true, 0, false, 'def');
    const sides = fight(on.sim, on.u, 240);
    expect(sides.length).toBeGreaterThan(0);
    expect(new Set(sides).size).toBe(1);
    expect(past(on.u, on.e, 'def')).toBe(true);
  });

  it('a way round is its falling back\'s own: another order, or a rout, and it is forgotten', () => {
    for (const how of ['order', 'rout']) {
      const { sim, u } = stage(true, 20);
      while (!u.round && sim.t < 60) sim.step();
      expect(u.round).toBeDefined();
      if (how === 'order') u.order = null; else u.state = 'routing';
      sim.step();
      expect(u.round).toBeUndefined();
    }
  });

  // The choice itself, on the open steppe set by hand and never stepped, so nothing turns or is eased while it is read:
  // the attack's first foot block `u` falling back south from (ux, uy), the defence's foot blocks where told
  // ([x, y, facing]); `ask` is move()'s own question, "this enemy's ground undid my step: which way round?".
  function choice(ux: number, uy: number, foes: [number, number, number][]): { u: U; them: U[]; clear: number; ask: (barred?: U) => [number, number] | null } {
    const sim = new LandBattle(ticketOf(PIECES.find((x) => x.key === 'prokhorovka-1943')!, 7, null));
    sim.ai = { att: false, def: false };
    const foot = (sd: Side): U[] => units(sim).filter((x) => x.side === sd && x.kind === 'inf' && !x.team);
    const u = foot('att')[0], them = foot('def').slice(0, foes.length);
    const put = (b: U, x: number, y: number, face: number): void => { b.x = x; b.y = y; b.face = face; b.hid = false; b.path = []; b.order = null; };
    put(u, ux, uy, Math.PI);
    u.order = { kind: 'withdraw', x: ux, y: 0 };
    them.forEach((b, i) => put(b, foes[i][0], foes[i][1], foes[i][2]));
    // (how far from an enemy block's middle its lane must pass: the boxes and LAND.apart, however either is turned)
    const e = them[0], half = (b: U): number => Math.hypot(b.w, b.d) / 2;
    const clear = Math.max(Math.hypot(u.w / 2 + half(e), u.d / 2 + half(e) + LAND.apart), Math.hypot(e.w / 2 + half(u), e.d / 2 + half(u) + LAND.apart));
    const q = sim as unknown as { wayRound(b: U, to: [number, number], barred: number | undefined): [number, number] | null };
    return { u, them, clear, ask: (barred) => q.wayRound(u, [u.x, 0], barred?.id) };
  }
  const NORTH = 0, END_ON = Math.PI / 2;

  it('the choice: the nearer end, by the side of the enemy\'s middle it stands on', () => {
    for (const lean of [20, -20]) {
      const { u, them: [e], clear, ask } = choice(300, 1000, [[300 + lean, 940, NORTH]]);
      const by = ask(e)!;
      // (the enemy leans east: round by the west, its lane `clear` and a metre from that block's middle; and the other way)
      expect(by[0]).toBeCloseTo(e.x - Math.sign(lean) * (clear + 1), 6);
      expect(by[1]).toBeCloseTo(1000, 6);
      expect(u.round).toMatchObject({ by: e.id, at: [e.x, e.y], side: lean > 0 ? 1 : -1 });
    }
  });

  it('the choice: the nearer end off the field, it takes the further one where the enemy stands wholly ahead', () => {
    const { u, them: [e], clear, ask } = choice(150, 1000, [[170, 940, NORTH]]);
    expect(e.x - (clear + 1)).toBeLessThan(5);
    expect(ask(e)![0]).toBeCloseTo(e.x + clear + 1, 6);
    expect(u.round!.side).toBe(-1);
  });

  it('the choice: level with the enemy block, it is not sent through that block to the further end', () => {
    // (the enemy stands end on at its shoulder, 53 m east and 3 m ahead; the nearer end, the west, is off the field)
    const { u, them: [e], ask } = choice(100, 1000, [[153, 997, -END_ON]]);
    expect(ask(e)).toBeNull();
    expect(u.round).toBeUndefined();
  });

  it('the choice: an end is closed where the step across ends in, or leads through, another enemy block\'s ground', () => {
    // (the nearer end is off the field in each; what differs is where a second enemy block stands)
    const far = (second: [number, number, number]): ReturnType<typeof choice> => choice(100, 1000, [[170, 940, NORTH], second]);
    const open = far([229, 1300, END_ON]);
    const end = open.them[0].x + open.clear + 1;
    // (the second block far up the field: the further end is open)
    expect(open.ask(open.them[0])![0]).toBeCloseTo(end, 6);
    // (standing where the step would end)
    const ends = far([end, 1000, NORTH]);
    expect(ends.ask(ends.them[0])).toBeNull();
    expect(ends.u.round).toBeUndefined();
    // (standing end on in the step across: clear of this block where it is, and clear of where the step would end)
    const through = far([229, 1000, END_ON]);
    expect(Math.abs(229 - through.u.x)).toBeGreaterThan(through.u.w / 2 + 20 + LAND.apart);
    expect(Math.abs(end - 229)).toBeGreaterThan(through.u.w / 2 + 20 + LAND.apart);
    expect(through.ask(through.them[0])).toBeNull();
    expect(through.u.round).toBeUndefined();
  });

  it('the choice reads its own sight: an enemy block still hidden closes no end, and one that bars it unseen gives no way round', () => {
    // (the nearer end is off the field; a second enemy block stands where the step to the further end would end)
    const at = (hidden: boolean): ReturnType<typeof choice> => {
      const c = choice(100, 1000, [[170, 940, NORTH], [0, 1000, NORTH]]);
      c.them[1].x = c.them[0].x + c.clear + 1;
      if (hidden) c.them[1].hid = true;
      return c;
    };
    // (seen, it closes that end, as above)
    const seen = at(false);
    expect(seen.ask(seen.them[0])).toBeNull();
    // (still hidden, it is not known to this block: the end is open, and it steps that way)
    const unseen = at(true);
    expect(unseen.ask(unseen.them[0])![0]).toBeCloseTo(unseen.them[0].x + unseen.clear + 1, 6);
    expect(unseen.u.round).toMatchObject({ by: unseen.them[0].id, side: -1 });
    // (and an enemy block that undoes its step while still hidden is no block it knows a way round)
    for (const lean of [20, -20]) {
      const { u, them: [e], ask } = choice(300, 1000, [[300 + lean, 940, NORTH]]);
      e.hid = true;
      expect(ask(e)).toBeNull();
      expect(u.round).toBeUndefined();
    }
  });

  it('the choice is kept: the same end though the other is now nearer, until that enemy block moves or is gone', () => {
    const { u, them: [e], clear, ask } = choice(300, 1000, [[320, 940, NORTH]]);
    expect(ask(e)![0]).toBeCloseTo(320 - (clear + 1), 6);
    // (set down east of the enemy's middle, where the east end is the nearer: asked again, it keeps the west)
    u.x = 345;
    expect(ask(e)![0]).toBeCloseTo(320 - (clear + 1), 6);
    expect(u.round).toMatchObject({ by: e.id, at: [320, 940], side: 1 });
    // (asked without a new bar, it goes on by the end it chose)
    expect(ask()![0]).toBeCloseTo(320 - (clear + 1), 6);
    // (its lane clear of that block by that end: straight on, the choice still its own until it is past)
    u.x = 320 - (clear + 2);
    expect(ask()).toBeNull();
    expect(u.round).toBeDefined();
    // (the enemy block moves off more than LAND.apart: it chooses afresh, by where the two now stand)
    u.x = 345; e.x = 320 - 2 * LAND.apart;
    expect(ask(e)![0]).toBeCloseTo(e.x + clear + 1, 6);
    expect(u.round).toMatchObject({ by: e.id, at: [e.x, 940], side: -1 });
    // (past it along its way: done)
    u.y = e.y - (clear + 1);
    expect(ask()).toBeNull();
    expect(u.round).toBeUndefined();
    // (and where the block it was going round is gone, there is no way round to keep)
    u.y = 1000;
    expect(ask(e)).not.toBeNull();
    e.state = 'gone';
    expect(ask()).toBeNull();
    expect(u.round).toBeUndefined();
  });

  it('the row: a key and a switch of its own, off, beside pursue', () => {
    const table = rulesTableCpp as string;
    expect(table).toMatch(/\{ TEXT\("wayround"\), TEXT\("Units falling back go round an enemy in their way"\), .*TEXT\("TheaterLandNoWayRound"\), false, TEXT\("TheaterLandRuleWayRound"\) \}/);
    expect(table.indexOf('{ TEXT("wayround")')).toBeGreaterThan(table.indexOf('{ TEXT("pursue")'));
    // (a battle opens with it off)
    const sim = new LandBattle(ticketOf(PIECES.find((x) => x.key === 'prokhorovka-1943')!, 7, null));
    expect((sim as unknown as { roundRule: boolean }).roundRule).toBe(false);
  });
});
