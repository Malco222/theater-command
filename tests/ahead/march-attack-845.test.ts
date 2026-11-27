// AHEAD OF THE RULES: Version 84.5, MARCH UP AND ATTACK (the design director's ruling of 2026-11-17 15:50Z (a) on
// Brian's find "if an enemy army is out of a city i couldnt attack them", and sheet 34 as passed at 16:23Z with its
// one mend). A right-click on an enemy army that is not next door, with an army in hand, plans the march up and the
// attack as one order, shown before it is carried out. The game picks the approach: the province beside the enemy
// reached soonest; among equals, no river between it and the enemy; then the better ground to attack from. The card
// lists that pick (THE STAFF'S PICK) and up to three other approaches reachable this month. THE MEND: an approach is
// ground the army can stand on this month without a fight (his own, an ally's, or ground he already holds); a
// province in enemy hands is never a row. Where nothing beside the enemy can be reached this month, there is no row.
//
// The rules' reading, named by the rules builder at 17:16Z: approaches(g, corps, target) in turn.ts, the staff's pick
// first, each { region, left (moves left on arrival), river (a river between it and the enemy), ground }.
// Every test here waits, skipped, until turn.ts exports approaches; then they all run.
//
// The case: Germany, September 1939 (seed 3). III Corps at Neumark, four moves, and the Polish army in Polish Silesia,
// which is not next door. Beside it: Upper Silesia (German, 2 moves left on arrival, no river), Moravia (German, 2
// left, a river), Slovakia (an ally's, 0 left), and Lodz, Krakow and Kielce (Polish, each with a Polish army).
import { describe, expect, it } from 'vitest';
import { addCorps, allied, atWar, createGame, hostileUnits, relocate } from '../../src/campaign/game';
import { riverBetween } from '../../src/campaign/rivers';
import { startOpen } from '../../src/campaign/staff';
import * as turn from '../../src/campaign/turn';
import { beginHuman, humanUp, reach, stepAI } from '../../src/campaign/turn';
import type { Corps, Game, Region } from '../../src/campaign/types';

interface Approach { region: number; left: number; river: boolean; ground: unknown }
// Read through a loose handle, so that this file type-checks on a tree where the reading is not built yet.
const approachesOf = (turn as unknown as Record<string, unknown>).approaches as ((g: Game, corps: Corps[], target: Region) => Approach[]) | undefined;
const waiting = typeof approachesOf !== 'function';
const approaches = (g: Game, corps: Corps[], target: Region): Approach[] => approachesOf!(g, corps, target);

function germany1939(): Game {
  const g = startOpen(createGame(3, 'GER'));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const corps = (g: Game, name: string): Corps => g.corps.find((c) => c.name === name && c.nat === 'GER' && !c.dead)!;
const named = (g: Game, rows: Approach[]): string[] => rows.map((a) => g.regions[a.region].name);
/** Moves left on arriving there this month for every corps in hand (the army moves as one, so the slowest counts). */
const leftOn = (g: Game, cs: Corps[], region: number): number => Math.min(...cs.map((c) => c.ap - reach(g, c).get(region)!.cost));

describe.skipIf(waiting)('84.5 march up and attack (ruling of 15:50Z (a), sheet 34): the approaches and the staff\'s pick', () => {
  it('the case stands as described: the target is not next door, and three provinces beside it can be reached this month', () => {
    const g = germany1939();
    const c = corps(g, 'III Corps');
    const T = g.byName['Polish Silesia'];
    expect(g.regions[c.reg].name).toBe('Neumark');
    expect(T.nb.includes(c.reg)).toBe(false);
    expect(hostileUnits(g, T, 'GER').length).toBeGreaterThan(0);
  });

  it('the staff\'s pick is reached soonest, and among equals has no river between it and the enemy', () => {
    const g = germany1939();
    const c = corps(g, 'III Corps');
    const T = g.byName['Polish Silesia'];
    const rows = approaches(g, [c], T);
    // Upper Silesia and Moravia are both reached with 2 moves left; Moravia has a river before the enemy.
    expect(named(g, rows).slice(0, 2)).toEqual(['Upper Silesia', 'Moravia']);
    expect(rows[0]).toMatchObject({ left: 2, river: false });
    expect(rows[1]).toMatchObject({ left: 2, river: true });
    // Slovakia, an ally's ground, is reached this month with nothing left: a row, where the attack waits for next month.
    expect(named(g, rows)).toContain('Slovakia');
    expect(rows.find((a) => g.regions[a.region].name === 'Slovakia')!.left).toBe(0);
  });

  it('every row is beside the enemy, reached this month, ground he can stand on without a fight, and says what is left and the river truly', () => {
    const g = germany1939();
    const c = corps(g, 'III Corps');
    const T = g.byName['Polish Silesia'];
    const rows = approaches(g, [c], T);
    expect(rows.length).toBeGreaterThan(0);
    // The pick and up to three others.
    expect(rows.length).toBeLessThanOrEqual(4);
    const seen = new Set<number>();
    for (const a of rows) {
      const r = g.regions[a.region];
      expect(seen.has(a.region), r.name).toBe(false);
      seen.add(a.region);
      expect(T.nb, r.name).toContain(a.region);
      expect(allied(g, 'GER', r.ctrl), r.name).toBe(true);
      expect(hostileUnits(g, r, 'GER').length, r.name).toBe(0);
      const at = reach(g, c).get(a.region);
      expect(at, r.name).toBeDefined();
      expect(at!.attack, r.name).toBe(false);
      expect(a.left, r.name).toBe(c.ap - at!.cost);
      expect(a.left, r.name).toBeGreaterThanOrEqual(0);
      expect(a.river, r.name).toBe(riverBetween(g, a.region, T.id) !== null);
    }
    // Reached soonest first, and no river before a river where the moves left are the same.
    for (let k = 1; k < rows.length; k++) {
      expect(rows[k].left).toBeLessThanOrEqual(rows[k - 1].left);
      if (rows[k].left === rows[k - 1].left) expect(Number(rows[k].river)).toBeGreaterThanOrEqual(Number(rows[k - 1].river));
    }
  });

  it('THE MEND: ground in enemy hands is never a row, even where he could march onto it this month with moves to spare', () => {
    const g = germany1939();
    const c = corps(g, 'III Corps');
    const T = g.byName['Polish Silesia'];
    // The Polish army leaves Lodz, beside the enemy: it is Polish ground with nobody in its field, and III Corps could
    // march onto it this month with a move left (marching there is an attack of its own).
    const L = g.byName['Lodz'];
    const away = g.regions.find((x) => x.ctrl === 'POL' && x.id !== T.id && !T.nb.includes(x.id) && !x.nb.some((j) => T.nb.includes(j)))!;
    for (const u of [...L.units]) relocate(g, u, away);
    expect(hostileUnits(g, L, 'GER').length).toBe(0);
    expect(atWar(g, 'GER', L.ctrl)).toBe(true);
    const there = reach(g, c).get(L.id);
    expect(there).toBeDefined();
    expect(c.ap - there!.cost).toBeGreaterThan(0);
    const rows = approaches(g, [c], T);
    expect(named(g, rows)).not.toContain('Lodz');
    for (const a of rows) expect(atWar(g, 'GER', g.regions[a.region].ctrl)).toBe(false);
    expect(named(g, rows)[0]).toBe('Upper Silesia');
  });

  it('an army of several corps: the moves left are the slowest corps\', and every row is reached by all of them', () => {
    const g = germany1939();
    const c = corps(g, 'III Corps');
    // Two more corps of other kinds join III Corps at Neumark, so the army in hand moves at more than one pace.
    addCorps(g, 'GER', 'Test panzer corps', 'arm', 2, c.reg, {});
    addCorps(g, 'GER', 'Test mountain corps', 'mtn', 2, c.reg, {});
    const here = g.regions[c.reg].units.filter((u) => u.nat === 'GER' && !u.dead && u.kind !== 'fort' && u.ap > 0 && u.town === undefined);
    expect(here.length).toBeGreaterThanOrEqual(3);
    const T = g.byName['Polish Silesia'];
    const rows = approaches(g, here, T);
    expect(rows.length).toBeGreaterThan(0);
    for (const a of rows) {
      for (const u of here) expect(reach(g, u).has(a.region), `${u.name} to ${g.regions[a.region].name}`).toBe(true);
      expect(a.left).toBe(leftOn(g, here, a.region));
    }
  });

  it('where nothing beside the enemy can be reached this month there is no row', () => {
    const g = germany1939();
    const c = corps(g, 'III Corps');
    const rc = reach(g, c);
    // A Polish army deep in Poland: no province beside it is in reach of III Corps this month.
    const far = g.regions.find((T) => hostileUnits(g, T, 'GER').length > 0 && !T.nb.includes(c.reg) && T.nb.every((j) => !rc.has(j)))!;
    expect(far).toBeDefined();
    expect(approaches(g, [c], far)).toEqual([]);
  });

  it('the plan is shown before it is carried out: reading it moves nothing and spends nothing', () => {
    const g = germany1939();
    const c = corps(g, 'III Corps');
    const T = g.byName['Polish Silesia'];
    const before = JSON.stringify(g.corps.map((u) => [u.id, u.reg, u.ap, u.dead]));
    const reports = g.reports.length, flags = JSON.stringify(g.flags);
    approaches(g, [c], T);
    expect(JSON.stringify(g.corps.map((u) => [u.id, u.reg, u.ap, u.dead]))).toBe(before);
    expect(g.reports.length).toBe(reports);
    expect(JSON.stringify(g.flags)).toBe(flags);
  });
});
