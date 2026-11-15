// A staff that will not be ringed (Version 71), the grand campaign only, and only the staffs named in rivals.json
// under `ring`. Two judgements, made before the month's operations and with no new rule of the game behind them. A
// place with few roads home left, none of them held against what stands next to it, is given up: the garrison comes
// out of the town and the field corps march out toward home. A force already ringed tries to get back: one corps
// walks through a province the enemy has left empty, or the ringed corps attack together where he is weakest on the
// road home. The capital is never given up, the player's own army is never moved, and history's scenarios use none
// of it.
import { describe, expect, it } from 'vitest';
import { ringJudgement } from '../src/campaign/ai';
import { addCorps, atWar, createGame, declareWar, removeCorps } from '../src/campaign/game';
import { fieldCtrl, garrison } from '../src/campaign/siege';
import { RIVALS, startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

const RING = RIVALS.ring!;

/** The grand campaign with Germany and Poland at war. */
const war = (human: string | null = null): Game => {
  const g = startOpen(createGame(3, human));
  if (!atWar(g, 'GER', 'POL')) declareWar(g, 'GER', 'POL');
  return g;
};

/** Marches from Berlin over ground Germany holds, with some provinces counted as lost. */
function fromBerlin(g: Game, lost: Set<number> = new Set()): Int16Array {
  const d = new Int16Array(g.regions.length).fill(-1);
  const cap = g.nations.GER.capital;
  d[cap] = 0;
  const q = [cap];
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) {
    if (d[j] >= 0 || lost.has(j) || fieldCtrl(g, g.regions[j]) !== 'GER') continue;
    d[j] = d[q[i]] + 1; q.push(j);
  }
  return d;
}

const empty = (g: Game, r: Region): void => { for (const c of [...r.units]) removeCorps(g, c); };
const put = (g: Game, nat: string, r: Region, divs: number): Corps => { const c = addCorps(g, nat, `Test Corps ${g.nextCorps}`, 'inf', divs, r.id, {}); c.ap = c.apMax; return c; };
const lose = (g: Game, ids: Iterable<number>): void => { for (const id of ids) { const r = g.regions[id]; empty(g, r); r.ctrl = 'POL'; } g.supplyDirty = true; };
const ours = (g: Game, r: Region): boolean => r.owner === 'GER' && r.ctrl === 'GER' && !r.cap && r.nb.every((j) => ['GER', 'POL'].includes(g.regions[j].ctrl) && !g.regions[j].cap);

/**
 * A German province with two roads home left and the enemy in every other province round it: the two roads stand
 * empty, each with the enemy next to it. Returns the place, its two ways out and the provinces the enemy holds.
 */
function sack(g: Game): { at: Region; ways: Region[]; foes: Region[] } {
  for (const at of g.regions) {
    if (!ours(g, at)) continue;
    const base = fromBerlin(g);
    const nearer = at.nb.filter((j) => base[j] >= 0 && base[j] < base[at.id]);
    if (nearer.length < 2) continue;
    const ways = nearer.slice(0, 2);
    const lost = new Set(at.nb.filter((j) => !ways.includes(j)));
    // Each way out needs an enemy next to it: one of the provinces already lost, or one more.
    let fits = true;
    for (const w of ways) {
      if (g.regions[w].nb.some((j) => lost.has(j))) continue;
      const more = g.regions[w].nb.find((j) => j !== at.id && !ways.includes(j) && g.regions[j].ctrl === 'GER' && !g.regions[j].cap && base[j] >= base[w]);
      if (more === undefined) { fits = false; break; }
      lost.add(more);
    }
    if (!fits) continue;
    const d = fromBerlin(g, lost);
    if (d[at.id] <= 0 || ways.some((w) => d[w] < 0 || d[w] > d[at.id])) continue;
    lose(g, lost);
    empty(g, at);
    for (const w of ways) empty(g, g.regions[w]);
    return { at, ways: ways.map((w) => g.regions[w]), foes: [...lost].map((id) => g.regions[id]) };
  }
  throw new Error('no province fits');
}

/**
 * A German province with the enemy holding every province round it, one of which joins ground Germany still holds.
 * Returns the place, that way out, and the rest of the ring.
 */
function ring(g: Game): { at: Region; door: Region; wall: Region[] } {
  for (const at of g.regions) {
    if (!ours(g, at)) continue;
    const lost = new Set(at.nb);
    const d = fromBerlin(g, lost);
    if (d[at.id] >= 0) continue;
    const door = at.nb.find((j) => g.regions[j].owner === 'GER' && g.regions[j].nb.some((k) => d[k] >= 0));
    if (door === undefined) continue;
    lose(g, lost);
    empty(g, at);
    // Nobody of the army outside stands next to the ring, unless the test puts him there.
    for (const j of lost) for (const k of g.regions[j].nb) if (k !== at.id && !lost.has(k) && g.regions[k].ctrl === 'GER') empty(g, g.regions[k]);
    return { at, door: g.regions[door], wall: at.nb.filter((j) => j !== door).map((j) => g.regions[j]) };
  }
  throw new Error('no province fits');
}

describe('a staff that will not be ringed', () => {
  it('is Germany\'s judgement alone, with its numbers in the data', () => {
    expect(RING.staffs).toEqual(['GER']);
    expect(RING.exits).toBeGreaterThanOrEqual(1);
    expect(RING.safe).toBeGreaterThan(0);
    expect(RING.safe).toBeLessThanOrEqual(1);
    expect(RING.reach).toBeGreaterThanOrEqual(2);
    expect(RING.odds).toBeGreaterThanOrEqual(1);
  });

  it('gives up a place with two roads home left and neither of them held, and marches the corps out toward home', () => {
    const g = war();
    const s = sack(g);
    for (const f of s.foes) put(g, 'POL', f, 3);
    const a = put(g, 'GER', s.at, 3), b = put(g, 'GER', s.at, 3);
    const given = ringJudgement(g, 'GER');
    expect(given).toContain(s.at.id);
    expect(given).not.toContain(g.nations.GER.capital);
    const home = fromBerlin(g);
    for (const c of [a, b]) {
      expect(c.dead).toBeFalsy();
      expect(c.reg).not.toBe(s.at.id);
      expect(home[c.reg]).toBeGreaterThanOrEqual(0);
    }
    expect(s.at.units.filter((u) => u.nat === 'GER').length).toBe(0);
  });

  it('stays where one road home is well held', () => {
    const g = war();
    const s = sack(g);
    for (const f of s.foes) put(g, 'POL', f, 1);
    const a = put(g, 'GER', s.at, 3);
    // Three strong corps on one of the two roads: the door is held.
    for (let k = 0; k < 3; k++) put(g, 'GER', s.ways[0], 4);
    expect(ringJudgement(g, 'GER')).not.toContain(s.at.id);
    expect(a.reg).toBe(s.at.id);
  });

  it('brings the garrison out of the town of a place it gives up', () => {
    const g = war();
    g.turn = 5;
    const s = sack(g);
    for (const f of s.foes) put(g, 'POL', f, 3);
    const inside = put(g, 'GER', s.at, 2);
    put(g, 'GER', s.at, 3);
    expect(garrison(g, inside)).toBe(true);
    inside.town = g.turn - 1; // it went in last month: it has marching time when it comes out
    expect(ringJudgement(g, 'GER')).toContain(s.at.id);
    expect(inside.town).toBeUndefined();
    expect(inside.reg).not.toBe(s.at.id);
  });

  it('leaves the player\'s own army alone, and a staff that is not named', () => {
    const g = war('GER');
    const s = sack(g);
    for (const f of s.foes) put(g, 'POL', f, 3);
    const a = put(g, 'GER', s.at, 3);
    expect(ringJudgement(g, 'GER')).toEqual([]);
    expect(a.reg).toBe(s.at.id);
    // Poland's staff is not named in the data: it makes no such judgement, whatever its army's state.
    expect(ringJudgement(war(), 'POL')).toEqual([]);
  });

  it('does nothing in history\'s scenarios', () => {
    const g = createGame(3, null);
    const before = g.corps.map((c) => c.reg).join(',');
    expect(ringJudgement(g, 'GER')).toEqual([]);
    expect(g.corps.map((c) => c.reg).join(',')).toBe(before);
  });
});

describe('a ringed force tries to get back', () => {
  it('sends one corps through a province the enemy has left empty, and the road is open again', () => {
    const g = war();
    const r = ring(g);
    for (const w of r.wall) put(g, 'POL', w, 3);
    const a = put(g, 'GER', r.at, 3), b = put(g, 'GER', r.at, 3);
    expect(fromBerlin(g)[r.at.id]).toBe(-1);
    ringJudgement(g, 'GER');
    expect(fieldCtrl(g, r.door)).toBe('GER');
    expect(r.door.units.filter((u) => u.nat === 'GER').length).toBeGreaterThanOrEqual(1);
    const home = fromBerlin(g);
    for (const c of [a, b]) expect(home[c.reg]).toBeGreaterThanOrEqual(0);
  });

  it('attacks the weakest place on the road home when the enemy stands on every way, and goes through', () => {
    const g = war();
    const r = ring(g);
    for (const w of r.wall) for (let k = 0; k < 2; k++) put(g, 'POL', w, 4);
    put(g, 'POL', r.door, 1);
    const a = put(g, 'GER', r.at, 4), b = put(g, 'GER', r.at, 4);
    ringJudgement(g, 'GER');
    const fought = g.reports.filter((b) => b.attNat === 'GER' && b.region === r.door.id);
    expect(fought.length).toBe(1);
    expect(fought[0].taken).toBe(true);
    expect(fieldCtrl(g, r.door)).toBe('GER');
    expect([a, b].some((c) => c.reg === r.door.id)).toBe(true);
  });

  it('is helped by the army outside: whoever stands next to the place attacked goes in with the ringed corps', () => {
    const g = war();
    const r = ring(g);
    for (const w of r.wall) for (let k = 0; k < 2; k++) put(g, 'POL', w, 4);
    put(g, 'POL', r.door, 3);
    // One weak corps inside could never do it alone.
    const inside = put(g, 'GER', r.at, 1);
    const home = fromBerlin(g);
    const outside = g.regions[r.door.nb.find((j) => home[j] >= 0)!];
    const relief = [put(g, 'GER', outside, 4), put(g, 'GER', outside, 4), put(g, 'GER', outside, 4)];
    ringJudgement(g, 'GER');
    const fought = g.reports.filter((b) => b.attNat === 'GER' && b.region === r.door.id);
    expect(fought.length).toBe(1);
    expect(fought[0].attackers.length).toBeGreaterThan(1);
    expect(fought[0].taken).toBe(true);
    expect(fieldCtrl(g, r.door)).toBe('GER');
    expect(fromBerlin(g)[inside.reg]).toBeGreaterThanOrEqual(0);
    expect(relief.every((c) => !c.dead)).toBe(true);
  });

  it('stands where the ring is too strong to break', () => {
    const g = war();
    const r = ring(g);
    for (const w of [...r.wall, r.door]) for (let k = 0; k < 3; k++) put(g, 'POL', w, 4);
    const a = put(g, 'GER', r.at, 1);
    ringJudgement(g, 'GER');
    const round = new Set(r.at.nb);
    expect(g.reports.filter((b) => b.attNat === 'GER' && round.has(b.region)).length).toBe(0);
    expect(a.reg).toBe(r.at.id);
    expect(fieldCtrl(g, r.door)).toBe('POL');
  });
});
