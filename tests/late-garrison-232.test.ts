// Ruling 232, riders b587 and b588: the late garrison. The drive-out rule (232) never lets a beaten army shelter in
// its town, so a capital stood its siege with only the two corps the staff's ordinary rule leaves there, and Paris
// fell in 1940 far more often than before the ruling. The mend is in the staff's garrison rule, for every capital and
// great city alike: with an enemy field army one march away, a computer staff at the dial's looser stops (3, 4 and 5)
// orders in, up to the town's room, its worn corps (strength against the corps' own size, or order, under 0.6): those
// standing in the province, and those in a province next door that can march in this month, the most worn first.
// Fresh corps stay in the field. Nothing changes at the book's stops or with the ruling off. (The rule is the computer staff's: a player garrisons his own towns.)
import { beforeEach, describe, expect, it, vi } from 'vitest';
vi.hoisted(() => { (globalThis as { TC_DRIVE?: unknown }).TC_DRIVE = 1; });
import { garrisons } from '../src/campaign/ai';
import { addCorps, createGame, relocate } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { DRIVE, SIEGE, driveOut, garrisonOf, greatPlace, roomIn } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

beforeEach(() => { DRIVE.on = true; });

/**
 * France in September 1939, nobody playing, with a German corps standing one march from Paris. Returns the game, Paris,
 * the province the Germans stand in, and a quiet province next to Paris that France holds.
 */
function parisThreatened(stop = 3): { g: Game; paris: Region; quiet: Region } {
  const g = startOpen(createGame(3, null));
  if (stop !== 3) expect(setStop(g, stop, true)).toBe(true);
  const paris = g.byName.Paris;
  // (Empty provinces beside Paris: a corps set down where the enemy's field army stands would be put in the town.)
  const next = paris.nb.map((j) => g.regions[j]).filter((r) => r.ctrl === 'FRA' && !r.units.length);
  expect(next.length).toBeGreaterThanOrEqual(2);
  // (Formed at home and set down beside Paris: a corps raised on ground the enemy stands on would join its town.)
  relocate(g, addCorps(g, 'GER', 'Test Panzer Corps', 'arm', 2, 'Berlin', {}), next[0]);
  return { g, paris, quiet: next[1] };
}
const corps = (g: Game, name: string, at: Region, str: number, org = 1, divs = 3): Corps => {
  const c = addCorps(g, 'FRA', name, 'inf', divs, at.id, {});
  c.str = str; c.org = org;
  return c;
};
const inside = (r: Region): string[] => garrisonOf(r).map((c) => c.name).sort();

describe('ruling 232, riders b587 and b588: the late garrison', () => {
  it('the dial says where it applies: a computer staff at stops 3, 4 and 5, worn under 0.6', () => {
    expect(SIEGE.ai.lateGarrison).toEqual({ stops: [3, 4, 5], str: 0.6, org: 0.6 });
  });

  it('Paris, an enemy army one march away: the worn corps in the province go inside, the fresh stay in the field', () => {
    const { g, paris } = parisThreatened();
    expect(driveOut(g)).toBe(true);
    expect(greatPlace(g, paris)).toBe(true);
    expect(garrisonOf(paris)).toHaveLength(0);
    const worn = corps(g, 'Worn Corps', paris, 0.4), tired = corps(g, 'Tired Corps', paris, 1, 0.5), fresh = corps(g, 'Fresh Corps', paris, 1);
    garrisons(g, 'FRA', false);
    expect(inside(paris)).toEqual(['Tired Corps', 'Worn Corps']);
    expect(worn.town).toBeDefined();
    expect(tired.town).toBeDefined();
    expect(fresh.town).toBeUndefined();
    expect(paris.units).toContain(fresh);
  });

  it('b588: a worn corps in a quiet province next door marches in; a fresh one there stays where it is', () => {
    const { g, paris, quiet } = parisThreatened();
    const worn = corps(g, 'Worn Neighbour', quiet, 0.5), fresh = corps(g, 'Fresh Neighbour', quiet, 1);
    garrisons(g, 'FRA', false);
    expect(inside(paris)).toEqual(['Worn Neighbour']);
    expect(worn.reg).toBe(paris.id);
    expect(fresh.reg).toBe(quiet.id);
    expect(fresh.town).toBeUndefined();
  });

  it('up to the town\'s room and no further, the most worn first, the province before its neighbours', () => {
    const { g, paris, quiet } = parisThreatened();
    const room = roomIn(g, paris);
    expect(room).toBe(3);
    for (let k = 0; k < 3; k++) corps(g, `Worn ${k}`, paris, 0.2 + 0.08 * k);
    corps(g, 'Worn Neighbour', quiet, 0.1);
    garrisons(g, 'FRA', false);
    // (Three worn corps stand in the province, one more next door: the province's own fill the room.)
    expect(inside(paris)).toEqual(['Worn 0', 'Worn 1', 'Worn 2']);
    expect(garrisonOf(paris)).toHaveLength(room);
  });

  it('with room for some only, the most worn go in', () => {
    const { g, paris } = parisThreatened();
    for (let k = 0; k < 5; k++) corps(g, `Worn ${k}`, paris, 0.5 - 0.08 * k);
    garrisons(g, 'FRA', false);
    expect(inside(paris)).toEqual(['Worn 2', 'Worn 3', 'Worn 4']);
  });

  it('b594: worn is read against the corps\' own size, so a row the record keeps small is not worn at its full', () => {
    const { g, paris } = parisThreatened();
    const small = corps(g, 'Brigade Group', paris, 0.5);
    small.full = 0.5;
    const thin = corps(g, 'Thinned Brigade', paris, 0.25);
    thin.full = 0.5;
    garrisons(g, 'FRA', false);
    expect(inside(paris)).toEqual(['Thinned Brigade']);
  });

  it('no enemy within a march, or a city that is no capital and has no great works: nothing goes in', () => {
    const g = startOpen(createGame(3, null));
    const paris = g.byName.Paris;
    corps(g, 'Worn Corps', paris, 0.4);
    garrisons(g, 'FRA', false);
    expect(garrisonOf(paris)).toHaveLength(0);
    // (A plain town with the enemy next door.)
    const plain = g.regions.find((r) => r.ctrl === 'FRA' && r.owner === 'FRA' && !r.overseas && !greatPlace(g, r) && r.nb.some((j) => g.regions[j].ctrl === 'FRA' && !g.regions[j].units.length))!;
    const beside = plain.nb.map((j) => g.regions[j]).find((r) => r.ctrl === 'FRA' && !r.units.length)!;
    relocate(g, addCorps(g, 'GER', 'Test Panzer Corps', 'arm', 2, 'Berlin', {}), beside);
    corps(g, 'Worn Provincial', plain, 0.4);
    garrisons(g, 'FRA', false);
    expect(garrisonOf(plain).map((c) => c.name)).not.toContain('Worn Provincial');
  });

  it('at the book\'s stops the staff keeps the record: no late garrison at stops 1 and 2', () => {
    for (const stop of [1, 2]) {
      const { g, paris } = parisThreatened(stop);
      corps(g, 'Worn Corps', paris, 0.4);
      garrisons(g, 'FRA', false);
      expect(garrisonOf(paris)).toHaveLength(0);
    }
  });

  it('with ruling 232 off there is no late garrison: a beaten army shelters in its town as it always did', () => {
    DRIVE.on = false;
    const { g, paris } = parisThreatened();
    expect(driveOut(g)).toBe(false);
    corps(g, 'Worn Corps', paris, 0.4);
    garrisons(g, 'FRA', false);
    expect(garrisonOf(paris)).toHaveLength(0);
  });
});
