// Direct command of a ground battle (release 2; Brian, 22 November 2026, 02:40Z: "I want to be able to click a unit and
// point where I want them to go so I want to control it", and 03:09Z "do 1 2 and 3"). The rules under the general's
// controls: every block of his side placed before the battle and turned his way; a block he orders is his, and his
// staff fights the rest; a run at the double; his orders by radio, a few seconds on the air, longer when a block's
// headquarters is hit. All behind switches: off, and with no order of his, a battle is the same to the last number.
import { describe, expect, it } from 'vitest';
import { LandBattle, LAND, sampleTicket, withTeams } from '../src/battle/land';
import type { LandUnit } from '../src/battle/land/units';

const sig = (b: LandBattle): string => JSON.stringify({ over: b.over, t: b.t, units: b.units.map((u) => [u.id, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged]) });
/** A practice field with the player attacking. */
const field = (seed = 5): LandBattle => new LandBattle(withTeams(sampleTicket('plains', 1.3, { seed, humanAtt: true, nat: { att: 'USA', def: 'GER' }, style: { att: 'methodical', def: 'bold' } })));
const run = (b: LandBattle, secs: number): void => { const end = b.t + secs; for (let k = 0; !b.over && b.t < end - 1e-9 && k < 100000; k++) b.step(); };
const foot = (b: LandBattle, side = 'att'): LandUnit[] => b.units.filter((u) => u.side === side && (u.kind === 'inf' || u.kind === 'mtn') && u.state !== 'gone');

describe('off, nothing changes', () => {
  it('a battle with the switches on and no order of the player\'s ends exactly as one with them off', () => {
    const off = field(), on = field();
    off.ai.att = true; on.ai.att = true;
    on.perUnit = true; on.radio = true;
    run(off, 3000); run(on, 3000);
    expect(off.over).toBeTruthy();
    expect(sig(on)).toBe(sig(off));
  }, 300_000);
});

describe('placing the whole army before the battle', () => {
  it('every block of his side may be placed, and turned his way; asked the old way only his guns and teams', () => {
    const b = field();
    expect(b.setupOn('att', true)).toBe(true);
    const z = b.setupRoom()!;
    const f = foot(b)[0];
    let spot: [number, number] | null = null;
    for (let x = z[0] + 90; x < z[2] - 90 && !spot; x += 20) for (const y of [z[3] - 40, z[3] - 120, (z[1] + z[3]) / 2]) if (!spot && b.placeWhy(f, x, y) === null) spot = [x, y];
    expect(spot).not.toBeNull();
    expect(b.command({ do: 'land:place', units: [f.id], x: spot![0], y: spot![1], face: 0.4 }, 'att')).toBe(true);
    expect(f.x).toBe(spot![0]); expect(f.y).toBe(spot![1]);
    expect(f.face).toBeCloseTo(0.4, 9);
    expect(f.keepFace).toBeCloseTo(0.4, 9);
    expect(f.home).toEqual(spot);
    // The enemy's are not his to place.
    const theirs = b.units.find((u) => u.side === 'def')!;
    expect(b.command({ do: 'land:place', units: [theirs.id], x: theirs.x, y: theirs.y - 20 }, 'att')).toBe(false);
    const old = field();
    old.setupOn('att');
    expect(old.placeWhy(foot(old)[0], foot(old)[0].x, foot(old)[0].y)).toBe('Only guns and support teams are placed.');
  });
});

describe('the blocks he orders are his; his staff fights the rest', () => {
  it('his staff leaves the block he has taken in hand alone, and takes it back when he hands it back', () => {
    const b = field();
    b.ai.att = true; b.perUnit = true;
    run(b, 4);
    const [mine, other] = foot(b);
    const to = { x: mine.x + 200, y: mine.y + 150 };
    expect(b.command({ do: 'move', units: [mine.id], x: to.x, y: to.y }, 'att')).toBe(true);
    expect(mine.taken).toBe(true);
    expect(other.taken).toBeUndefined();
    // Many staff thoughts later it is still on his errand, not the staff's.
    run(b, 3 * LAND.think + 1);
    expect(mine.order === null || (mine.order.kind === 'move' && Math.abs(mine.order.x - to.x) < 1)).toBe(true);
    expect(other.order?.kind).toBe('attack');
    // Handed back, the staff gives it its orders again.
    expect(b.command({ do: 'land:staff', units: [mine.id] }, 'att')).toBe(true);
    expect(mine.taken).toBeUndefined();
    run(b, LAND.think + 1);
    expect(mine.order?.kind).toBe('attack');
    // Handing the whole side to the computer hands every block back.
    b.command({ do: 'hold', units: [other.id] }, 'att');
    expect(other.taken).toBe(true);
    b.command({ do: 'auto', side: 'att', on: true }, 'att');
    expect(other.taken).toBeUndefined();
  });

  it('the staff\'s own orders never take a block in hand, and the computer\'s side is never his', () => {
    const b = field();
    b.ai.att = true; b.perUnit = true;
    run(b, 30);
    expect(b.units.some((u) => u.taken)).toBe(false);
    const theirs = foot(b, 'def')[0];
    expect(b.command({ do: 'hold', units: [theirs.id] }, 'def')).toBe(true);
    expect(theirs.taken).toBeUndefined();
  });
});

describe('at the double', () => {
  it('foot sent at the double get there faster, and are winded for a while after', () => {
    const walk = field(), dbl = field();
    for (const b of [walk, dbl]) { b.ai.att = false; run(b, 1); }
    const a = foot(walk)[0], d = foot(dbl)[0];
    const to = { x: a.x, y: a.y + 300 };
    walk.command({ do: 'move', units: [a.id], x: to.x, y: to.y }, 'att');
    dbl.command({ do: 'move', units: [d.id], x: to.x, y: to.y, run: true }, 'att');
    expect(d.order?.run).toBe(true);
    expect(a.order?.run).toBeUndefined();
    run(walk, 20); run(dbl, 20);
    expect(d.y - (to.y - 300)).toBeGreaterThan((a.y - (to.y - 300)) * 1.3);
    expect(d.winded).toBeGreaterThan(dbl.t);
    expect(a.winded).toBeUndefined();
  });
});

describe('drawn as men (looks only)', () => {
  it('a block sent at the double is every man up and running', () => {
    const b = field();
    b.ai.att = false;
    const d = b.drillOn({ sections: true, platoons: true });
    run(b, 1);
    const [u, w] = foot(b);
    b.command({ do: 'move', units: [u.id], x: u.x, y: u.y + 400, run: true }, 'att');
    b.command({ do: 'move', units: [w.id], x: w.x, y: w.y + 400 }, 'att');
    run(b, 8);
    const poses = (id: number): string[] => d.view().find((x) => x.id === id)!.men.filter((m) => m.d === undefined && m.r !== 'lorry').map((m) => m.p);
    // (On the battle clock a block on the march goes at a man's run already: the double shows most under fire, where its
    // men keep running instead of going by fire and movement. The ground it makes is the rules' test above.)
    const ran = poses(u.id);
    expect(ran.filter((p) => p === 'run').length / ran.length).toBeGreaterThan(0.8);
  });
});

describe('orders by radio', () => {
  it('his order reaches the block a few seconds after he gives it; a later one replaces it on the air; his staff\'s go at once', () => {
    const b = field();
    b.ai.att = false; b.radio = true;
    run(b, 2);
    const [u, v] = foot(b);
    const before = u.order;
    expect(b.command({ do: 'move', units: [u.id], x: u.x + 100, y: u.y + 100 }, 'att')).toBe(true);
    expect(u.order).toBe(before);
    expect(u.orderAt).toBeCloseTo(b.t + LAND.radio.delay, 6);
    run(b, LAND.radio.delay - 1);
    expect(u.order).toBe(before);
    // A second order before the first lands: only the second is carried out, and the wait starts again.
    expect(b.command({ do: 'move', units: [u.id], x: u.x - 100, y: u.y + 100 }, 'att')).toBe(true);
    const x2 = u.x - 100;
    run(b, LAND.radio.delay + 0.5);
    expect(u.orderAt).toBeUndefined();
    expect(u.order === null || (u.order.kind === 'move' && Math.abs(u.order.x - x2) < 30)).toBe(true);
    // A block whose headquarters is hit (under half its strength) hears later.
    v.str = 0.4 * v.str0;
    b.command({ do: 'hold', units: [v.id] }, 'att');
    expect(v.orderAt).toBeCloseTo(b.t + LAND.radio.delay * LAND.radio.cut, 6);
  });

  it('the wait counts from the time he gave it as shown (at), never from before it, and never ahead of the battle', () => {
    const b = field();
    b.ai.att = false; b.radio = true;
    run(b, 10);
    const [u, v, w] = foot(b);
    // Given 2 s ago as the picture showed it: it lands 3 s from now.
    expect(b.command({ do: 'move', units: [u.id], x: u.x + 50, y: u.y + 50, at: b.t - 2 } as never, 'att')).toBe(true);
    expect(u.orderAt).toBeCloseTo(b.t - 2 + LAND.radio.delay, 6);
    // An 'at' ahead of the battle counts from now.
    b.command({ do: 'move', units: [v.id], x: v.x + 50, y: v.y + 50, at: b.t + 30 } as never, 'att');
    expect(v.orderAt).toBeCloseTo(b.t + LAND.radio.delay, 6);
    // Given longer ago than the wait: carried out at once.
    b.command({ do: 'move', units: [w.id], x: w.x + 50, y: w.y + 50, at: b.t - LAND.radio.delay - 1 } as never, 'att');
    expect(w.orderAt).toBeUndefined();
    expect(w.order?.kind).toBe('move');
  });

  it('before the clock runs (the setup step) his orders are given at once', () => {
    const b = field();
    b.ai.att = false; b.radio = true;
    const u = foot(b)[0];
    expect(b.command({ do: 'move', units: [u.id], x: u.x, y: u.y + 50 }, 'att')).toBe(true);
    expect(u.order?.kind).toBe('move');
    expect(u.orderAt).toBeUndefined();
  });

  it('the computer\'s side is never on the air', () => {
    const b = field();
    b.radio = true;
    run(b, 2);
    const theirs = foot(b, 'def')[0];
    b.command({ do: 'move', units: [theirs.id], x: theirs.x, y: theirs.y - 40 }, 'def');
    expect(theirs.orderAt).toBeUndefined();
    expect(theirs.order?.kind).toBe('move');
  });
});
