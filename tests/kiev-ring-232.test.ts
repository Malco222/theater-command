// Ruling 232, riders b605, b606 and b622: Kiev's ring and Helsinki's garrison, by the record. With "beat the army,
// take the city" on, Kiev fell with no corps inside in about half its falls, in the attack's first month. The mend
// is two facts from the record (historian/city-garrisons-b605.md), not a staff's rule: the Kiev Fortified Region
// stands inside Kiev from the first day at an eighth of its two divisions, and Helsinki's garrison at 0.3 of one,
// each with the fortress mark that keeps it in; and the first time anyone declares war on the Soviet Union (the
// Winter War is the Soviet Union's own declaration, not one on her) the ring fills to its two divisions from the
// reserve, before the attacker's battles that month. On 96 wars Kiev and Helsinki were then never bare.
import { beforeEach, describe, expect, it, vi } from 'vitest';
vi.hoisted(() => { (globalThis as { TC_DRIVE?: unknown }).TC_DRIVE = 1; });
import { garrisons } from '../src/campaign/ai';
import { createGame, declareWar } from '../src/campaign/game';
import { DRIVE, SIEGE, driveOut, garrisonOf, manRings } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

beforeEach(() => { DRIVE.on = true; });

const open = (): Game => startOpen(createGame(3, null));
const inTown = (g: Game, town: string, name: string): Corps | undefined => garrisonOf(g.byName[town]).find((c) => c.name === name && !c.dead);

describe('ruling 232, rider b606: Kiev\'s ring and Helsinki\'s garrison by the record', () => {
  it('the rows are the record\'s: an eighth and three tenths, the fortress mark, one ring', () => {
    const row = (corps: string) => SIEGE.startIn!.find((s) => s.corps === corps);
    expect(row('Kiev Fortified Region')).toMatchObject({ nat: 'SOV', str: 0.125, fortress: true });
    expect(row('Helsinki garrison')).toMatchObject({ nat: 'FIN', str: 0.3, fortress: true });
    expect(SIEGE.rings).toEqual([{ nat: 'SOV', town: 'Kiev', corps: 'Kiev Fortified Region', org: 0.7 }]);
  });

  it('from the first day both stand inside their towns at the record\'s size', () => {
    const g = open();
    expect(driveOut(g)).toBe(true);
    const kiev = inTown(g, 'Kiev', 'Kiev Fortified Region')!, helsinki = inTown(g, 'Helsinki', 'Helsinki garrison')!;
    expect(kiev).toBeDefined();
    expect(kiev.divs).toBe(2);
    expect(kiev.str).toBeCloseTo(0.125, 6);
    expect(kiev.fortress).toBe(true);
    expect(helsinki).toBeDefined();
    expect(helsinki.divs).toBe(1);
    expect(helsinki.str).toBeCloseTo(0.3, 6);
    expect(helsinki.fortress).toBe(true);
  });

  it('the fortress mark keeps them in: with no enemy near, the staff sends neither back to the field', () => {
    const g = open();
    garrisons(g, 'SOV', false);
    garrisons(g, 'FIN', false);
    expect(inTown(g, 'Kiev', 'Kiev Fortified Region')).toBeDefined();
    expect(inTown(g, 'Helsinki', 'Helsinki garrison')).toBeDefined();
  });

  it('the first declaration of war on the Soviet Union mans the ring: two divisions, its order no better than 0.7', () => {
    const g = open();
    const kiev = inTown(g, 'Kiev', 'Kiev Fortified Region')!;
    expect(g.flags['ring:Kiev']).toBeUndefined();
    declareWar(g, 'GER', 'SOV');
    expect(g.flags['ring:Kiev']).toBe(g.turn);
    expect(kiev.str).toBeCloseTo(1, 6);
    expect(kiev.str * kiev.divs).toBeCloseTo(2, 6);
    expect(kiev.org).toBeLessThanOrEqual(0.7);
    expect(inTown(g, 'Kiev', 'Kiev Fortified Region')).toBe(kiev);
  });

  it('once: a second declaration, or the ring worn down later, brings no new men', () => {
    const g = open();
    const kiev = inTown(g, 'Kiev', 'Kiev Fortified Region')!;
    declareWar(g, 'GER', 'SOV');
    kiev.str = 0.5;
    manRings(g, 'SOV');
    declareWar(g, 'ROM', 'SOV');
    expect(kiev.str).toBeCloseTo(0.5, 6);
  });

  it('the Winter War is the Soviet Union\'s own declaration: the ring stays at an eighth', () => {
    const g = open();
    const kiev = inTown(g, 'Kiev', 'Kiev Fortified Region')!;
    declareWar(g, 'SOV', 'FIN');
    expect(g.flags['ring:Kiev']).toBeUndefined();
    expect(kiev.str).toBeCloseTo(0.125, 6);
  });

  it('with ruling 232 off nothing is manned', () => {
    DRIVE.on = false;
    const g = open();
    manRings(g, 'SOV');
    expect(g.flags['ring:Kiev']).toBeUndefined();
  });
});
