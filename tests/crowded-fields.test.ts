// The realism version, its first piece: crowded airfields (the design director's b437 and b448).
// 1. A computer staff leaves no field of its own crowded at its month's end: groups go to the nearest fields with room
//    that still reach their stretch of front, and the field they leave stays full.
// 2. A group too big for any field with room is split, and the command is kept at what it was.
// 3. The player's groups are never moved; history's campaigns, with no air war, are untouched.
import { describe, expect, it } from 'vitest';
import { AIR, airRange, airWar, crowding, fieldServes, km, spreadAir } from '../src/campaign/air';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { AirFleet, Game } from '../src/campaign/types';

const land = (g: Game, nat: string): AirFleet[] => g.airFleets.filter((f) => f.nat === nat && f.ship === undefined && f.ashore === undefined && f.base >= 0);
const based = (g: Game, id: number): number => g.airFleets.reduce((s, f) => s + (f.ship === undefined && f.base === id ? f.ftr + f.bmr : 0), 0);
const kept = (g: Game, id: number): number => g.airFleets.reduce((s, f) => s + (f.ship === undefined && f.base === id ? Math.max(f.ftr + f.bmr, f.est.ftr + f.est.bmr) : 0), 0);
/** A grand campaign's first month, with every one of a nation's groups but those already there piled onto one of its fields. */
function piled(nat: string, seed = 3): { g: Game; F: number } {
  const g = startOpen(createGame(seed, null));
  const mine = land(g, nat);
  const F = mine[0].base;
  // (Groups within a month's flying of it, as a staff might have gathered them.)
  for (const f of mine) if (km(g, f.base, F) <= AIR.ferry) f.base = F;
  expect(kept(g, F)).toBeGreaterThan(fieldServes(g, F));
  return { g, F };
}

describe('a computer staff at its month\'s end (b437)', () => {
  it('leaves no field of its own crowded, and the field it leaves full', () => {
    for (const nat of ['SOV', 'GER']) {
      const { g, F } = piled(nat);
      const before = land(g, nat).reduce((s, f) => s + f.ftr + f.bmr, 0);
      const moved = spreadAir(g, nat);
      expect(moved.length, nat).toBeGreaterThan(0);
      for (const id of new Set(land(g, nat).map((f) => f.base))) {
        expect(kept(g, id), `${nat} ${g.regions[id].name}`).toBeLessThanOrEqual(fieldServes(g, id));
        expect(based(g, id)).toBeLessThanOrEqual(fieldServes(g, id));
      }
      // Every group that is left can fly whole.
      for (const f of land(g, nat)) expect(crowding(g, f)).toBe(1);
      // The field they left is still as full as one more group would make it too full.
      const smallest = Math.min(...moved.map((f) => Math.max(f.ftr + f.bmr, f.est.ftr + f.est.bmr)));
      expect(kept(g, F)).toBeGreaterThan(fieldServes(g, F) - smallest);
      // No aircraft are lost or made on the way, and every group moved went a month's flying at most, with its move's cost.
      expect(land(g, nat).reduce((s, f) => s + f.ftr + f.bmr, 0)).toBe(before);
      for (const f of moved) { expect(f.moved).toBe(g.turn); expect(km(g, F, f.base)).toBeLessThanOrEqual(AIR.ferry); }
    }
  });

  it('keeps a group that covers the front within reach of it where a field with room does', () => {
    // Germany in September 1939, over Poland.
    const { g, F } = piled('GER');
    const front = g.regions.filter((r) => r.ctrl === 'POL' && r.units.some((u) => !u.dead && u.nat === 'POL')).map((r) => r.id);
    const covers = (f: AirFleet, at: number): boolean => front.some((t) => km(g, at, t) <= (f.kind === 'bmr' ? airRange(f).bmr : airRange(f).ftr));
    const was = new Map(land(g, 'GER').map((f) => [f.id, covers(f, F)]));
    const moved = spreadAir(g, 'GER');
    const coverers = moved.filter((f) => was.get(f.id));
    expect(coverers.length).toBeGreaterThan(0);
    // (A group that covered the front from the crowded field covers it still, unless every field that would was full.)
    const still = coverers.filter((f) => covers(f, f.base)).length;
    expect(still).toBeGreaterThan(0);
  });

  it('splits a group too big for any field with room, and keeps the command at what it was', () => {
    const { g, F } = piled('SOV');
    // Every field near enough has room for a part of a group, and none for the largest whole.
    const big = land(g, 'SOV').filter((f) => f.base === F).sort((a, b) => b.ftr + b.bmr - (a.ftr + a.bmr))[0];
    for (const f of land(g, 'SOV')) if (f !== big) f.base = F;
    // (Fill the other Soviet fields within reach to leave each 100 aircraft of room: a group of the size of `big` fits none.)
    const others = g.regions.filter((r) => r.id !== F && fieldServes(g, r.id) > 0 && km(g, F, r.id) <= AIR.ferry);
    const stub = (id: number, n: number): AirFleet => ({ id: g.nextAir++, nat: 'POL', name: `Stub ${id}`, base: id, ftr: n, bmr: 0, est: { ftr: n, bmr: 0 }, mission: 'support', target: -1, ready: 1, leader: null, moved: 0, lost: { ftr: 0, bmr: 0 }, kind: 'ftr' });
    for (const r of others) { const n = fieldServes(g, r.id) - kept(g, r.id) - 100; if (n > 0) g.airFleets.push(stub(r.id, n)); }
    const est = (fs: AirFleet[]): number => fs.reduce((s, f) => s + f.est.ftr + f.est.bmr, 0);
    /** Each command's shares of each kind of aircraft. */
    const shares = (fs: AirFleet[]): Record<string, number> => { const o: Record<string, number> = {}; for (const f of fs) o[`${f.parent}:${f.kind}`] = (o[`${f.parent}:${f.kind}`] ?? 0) + (f.share ?? 1); return o; };
    const before = { n: land(g, 'SOV').length, est: est(land(g, 'SOV')), shares: shares(land(g, 'SOV')), names: new Map(land(g, 'SOV').map((f) => [f.id, f.name])) };
    expect(big.ftr + big.bmr).toBeGreaterThan(100);
    const firstNew = g.nextAir;
    spreadAir(g, 'SOV');
    const after = land(g, 'SOV');
    expect(after.length).toBeGreaterThan(before.n);
    // (The parts split off: new groups, each on a field of its own with room for it.)
    const parts = after.filter((f) => f.id >= firstNew);
    expect(parts.length).toBeGreaterThan(0);
    for (const p of parts) {
      expect(p.base).not.toBe(F);
      // (Split from a group of the same command and kind, which keeps its name; the part has a name of its own and no commander.)
      expect(after.some((q) => q.id < firstNew && q.parent === p.parent && q.kind === p.kind)).toBe(true);
      expect(after.filter((q) => q.name === p.name)).toHaveLength(1);
      expect(p.leader).toBeNull();
    }
    for (const f of after) if (f.id < firstNew) expect(f.name).toBe(before.names.get(f.id));
    // Every command's establishment and shares are as they were, to the rounding of a group.
    expect(Math.abs(est(after) - before.est)).toBeLessThanOrEqual(after.length - before.n);
    const now = shares(after);
    for (const [k, v] of Object.entries(before.shares)) expect(now[k], k).toBeCloseTo(v, 9);
  });

  it('never moves the player\'s groups, and leaves history\'s campaigns alone', () => {
    const { g } = piled('SOV');
    g.human = 'SOV';
    expect(spreadAir(g, 'SOV')).toEqual([]);
    const h = createGame(3, null);
    expect(airWar(h)).toBe(false);
    for (const nat of Object.keys(h.nations)) expect(spreadAir(h, nat)).toEqual([]);
  });

  it('runs at every computer nation\'s month\'s end: no Soviet field is crowded as the months go by', () => {
    const g = startOpen(createGame(17, null));
    for (let m = 0; m < 24; m++) {
      runTurnAI(g);
      for (const id of new Set(land(g, 'SOV').map((f) => f.base))) expect(based(g, id), `${g.regions[id].name} month ${g.turn}`).toBeLessThanOrEqual(fieldServes(g, id));
    }
  }, 240_000);
});
