// The design director's b544 (ruling 232's rider): a computer staff relieves its ringed capital or great city first,
// before the line, the reserves or the plan move anyone. (1) With its own corps next door that are fit to attack (moves
// left, not yet in a fight this month, order 0.6 or better, not held), it goes in if the odds reach the relief bar.
// (2) Short of the bar, once the town has eaten half its stores, those corps are held where they stand for the month,
// so the relief gathers month on month. A staff's habit at every stop; a player's corps are never held, and nothing
// happens where there are no sieges. (3), the lower bar, is held back.
import { describe, expect, it } from 'vitest';
import { reliefBar, reliefFirst } from '../src/campaign/ai';
import { addCorps, createGame, relocate } from '../src/campaign/game';
import { besieged, garrison, garrisonOf, monthsShut, storesOf, whyNotGarrison } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

/** A grand campaign in its first month, every seat the computer's (or the named one a player's). */
function grand(human: string | null = null, seed = 3): Game {
  return startOpen(createGame(seed, human));
}
/**
 * Warsaw ringed: Poland's field corps sent off to a quiet province, the German corps given standing in Warsaw's field
 * (strong: four whole corps; weak: one, all but gone), a Polish corps of three divisions inside, and one fresh Polish
 * corps next door, in a province of Poland's with no Germans in it.
 */
function ringed(g: Game, ring: 'strong' | 'weak', name = 'Warsaw'): { town: Region; door: Region; relief: Corps } {
  const town = g.byName[name];
  const quiet = g.regions.find((r) => r.ctrl === 'POL' && !town.nb.includes(r.id) && r.id !== town.id && !r.units.some((u) => u.nat !== 'POL'))!;
  for (const c of g.corps.filter((c) => c.nat === 'POL' && !c.dead && c.kind !== 'fort')) { c.town = undefined; relocate(g, c, quiet); }
  town.garr = [];
  const door = town.nb.map((j) => g.regions[j]).find((r) => r.ctrl === 'POL' && !r.units.some((u) => u.nat !== 'POL'))!;
  const inside = addCorps(g, 'POL', `${name} garrison`, 'inf', 3, town.id, {});
  if (!garrison(g, inside)) throw new Error(`no garrison for ${name}: ${whyNotGarrison(g, inside)}`);
  const ger = g.corps.filter((c) => c.nat === 'GER' && !c.dead && c.kind === 'inf' && !c.town).slice(0, ring === 'strong' ? 4 : 1);
  for (const c of ger) { relocate(g, c, town); if (ring === 'weak') c.str = 0.05; }
  const relief = addCorps(g, 'POL', 'Relief army', 'inf', ring === 'strong' ? 2 : 6, door.id, {});
  relief.org = 1; relief.ap = relief.apMax; relief.fought = false; relief.hold = false;
  if (ring === 'strong') relief.str = 0.4;
  return { town, door, relief };
}
const fights = (g: Game, town: Region, from: number): number => g.reports.slice(from).filter((r) => r.region === town.id && r.attNat === 'POL').length;

describe('relief first (b544)', () => {
  it('sets the scene: Warsaw is a ringed capital with its own corps next door', () => {
    const g = grand();
    const { town, door, relief } = ringed(g, 'weak');
    expect(besieged(g, town)).toBe(true);
    expect(garrisonOf(town).map((c) => c.name)).toEqual(['Warsaw garrison']);
    expect(door.nb).toContain(town.id);
    expect(relief.reg).toBe(door.id);
  });

  it('(1) goes in at once when the odds reach the bar, and holds nobody', () => {
    const g = grand();
    const { town, relief } = ringed(g, 'weak');
    const from = g.reports.length;
    const kept = reliefFirst(g, 'POL');
    expect(fights(g, town, from)).toBe(1);
    expect(g.reports[g.reports.length - 1].odds).toBeGreaterThanOrEqual(reliefBar(g, town) * 0.8 - 1e-9);
    expect(kept).toEqual([]);
    expect(relief.hold).toBeFalsy();
  });

  it('(2) short of the bar, with the stores still more than half full, neither goes in nor holds', () => {
    const g = grand();
    const { town, relief } = ringed(g, 'strong');
    g.flags[`sgm:${town.id}`] = Math.floor((storesOf(g, town) - 1) / 2);
    expect(monthsShut(g, town) * 2).toBeLessThan(storesOf(g, town));
    const from = g.reports.length;
    expect(reliefFirst(g, 'POL')).toEqual([]);
    expect(fights(g, town, from)).toBe(0);
    expect(relief.hold).toBeFalsy();
  });

  it('(2) short of the bar, once half the stores are eaten, holds the corps next door for the relief', () => {
    const g = grand();
    const { town, relief } = ringed(g, 'strong');
    g.flags[`sgm:${town.id}`] = Math.ceil(storesOf(g, town) / 2);
    const from = g.reports.length;
    const kept = reliefFirst(g, 'POL');
    expect(fights(g, town, from)).toBe(0);
    expect(kept).toContain(relief);
    expect(relief.hold).toBe(true);
  });

  it('counts only the corps fit to attack: one that has fought, or has no moves, or is in disorder, is neither sent nor held', () => {
    for (const unfit of [(c: Corps) => { c.fought = true; }, (c: Corps) => { c.ap = 0; }, (c: Corps) => { c.org = 0.5; }]) {
      for (const ring of ['weak', 'strong'] as const) {
        const g = grand();
        const { town, relief } = ringed(g, ring);
        g.flags[`sgm:${town.id}`] = storesOf(g, town);
        unfit(relief);
        const from = g.reports.length;
        expect(reliefFirst(g, 'POL')).toEqual([]);
        expect(fights(g, town, from)).toBe(0);
        expect(relief.hold).toBeFalsy();
      }
    }
  });

  it('leaves the ring of a town that is neither a capital nor a great city to the month\'s moves', () => {
    const g = grand();
    const { town, relief } = ringed(g, 'weak', 'Lodz');
    expect(besieged(g, town)).toBe(true);
    const from = g.reports.length;
    expect(reliefFirst(g, 'POL')).toEqual([]);
    expect(fights(g, town, from)).toBe(0);
    g.flags[`sgm:${town.id}`] = storesOf(g, town);
    relief.str = 0.4;
    expect(reliefFirst(g, 'POL')).toEqual([]);
    expect(relief.hold).toBeFalsy();
  });

  it('never moves or holds a player\'s corps', () => {
    const g = grand('POL');
    const { town, relief } = ringed(g, 'weak');
    const from = g.reports.length;
    expect(reliefFirst(g, 'POL')).toEqual([]);
    expect(fights(g, town, from)).toBe(0);
    g.flags[`sgm:${town.id}`] = storesOf(g, town);
    relief.str = 0.4;
    expect(reliefFirst(g, 'POL')).toEqual([]);
    expect(relief.hold).toBeFalsy();
  });

  it('does nothing in a historical scenario, where there are no sieges', () => {
    const g = createGame(3, null);
    const from = g.reports.length;
    expect(reliefFirst(g, 'POL')).toEqual([]);
    expect(g.reports.length).toBe(from);
  });
});
