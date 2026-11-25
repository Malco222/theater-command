// A row that falls back to forming at home takes nobody, with the far side on (release 2, the Pacific fold on the
// map's head 1125p). The map line's "a division is laid once" (drawn.ts `heldAlready`) leaves every division a row
// names with the corps that holds it where the row has fallen back to forming at its capital, its own place lost or
// its host at peace (`home`). Since the fold, where a row is put down is the fold's `rowPlace` (game.ts), which hands
// `home` back in the two branches the map line set it in. The map line's own test of it
// (tests/price-once-r2.test.ts, 'a row that falls back to forming at home takes nobody') runs with the far side off;
// this file reads the same rule with the WHOLE WORLD ON, for a computer's Germany (where `home` alone keeps the
// divisions in place: a player's claims always stay) in the Balkans in March 1941, on real data with no row added.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { divisionsOf } from '../src/campaign/armies';
import { drawnWatch } from '../src/campaign/drawn';
import { createGame, declareWar, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { farOn } from '../src/campaign/world';
import type { Corps, Game } from '../src/campaign/types';

const MARCH_41 = 19;
const PANZERS = '2nd and 9th Panzer Divisions', INFANTRY = '50th, 72nd and 164th Infantry Divisions';

type Told = Parameters<NonNullable<typeof drawnWatch.tell>>[0];

/**
 * Nobody's seat, Italy's war on Greece begun and Germany in it. With `allies`, Bulgaria, Hungary and Romania are in
 * that war on her side; without, they are friends at peace.
 */
function balkans(allies: boolean): { g: Game; told: Told[] } {
  const g = startOpen(createGame(1, null));
  declareWar(g, 'ITA', 'GRE');
  declareWar(g, 'GER', 'GRE');
  for (const n of ['BUL', 'HUN', 'ROM']) { g.nations[n].align = 'axis'; if (allies) declareWar(g, n, 'GRE'); }
  const told: Told[] = [];
  drawnWatch.tell = (e) => told.push(e);
  return { g, told };
}
afterEach(() => { drawnWatch.tell = null; });
const month = (g: Game, turn: number): void => { g.turn = turn; startPhase(g, 'GER'); };
const corps = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'GER' && !c.dead && c.name === name);
const names = (g: Game, c: Corps): string[] => divisionsOf(g, c).map((d) => d.name);
const place = (g: Game, c: Corps): string => g.regions[c.reg].name;

/**
 * What the March rows did to XXII Corps (the 2nd and 9th Panzer), and, with `infantry` (the infantry row is put down
 * at Plovdiv, so it falls back home only where the host is at peace), to III Corps (the 50th Infantry).
 */
function read(g: Game, told: Told[], infantry: boolean): void {
  const xxii = corps(g, 'XXII Corps')!, iii = corps(g, 'III Corps')!;
  const at = place(g, xxii);
  month(g, MARCH_41);
  const e = told.find((t) => t.row === PANZERS)!;
  expect(e.home).toBe(true);
  expect(e.lands).toBe('Berlin');
  expect(e.claims.length).toBe(2);
  expect(e.claims.every((q) => q.stays)).toBe(true);
  expect(xxii.name).toBe('XXII Corps');
  expect(place(g, xxii)).toBe(at);
  expect(corps(g, PANZERS)).toBeUndefined();
  if (!infantry) return;
  expect(told.find((t) => t.row === INFANTRY)!.claims.every((q) => q.stays)).toBe(true);
  expect(iii.divs).toBe(3);
  const c = corps(g, INFANTRY)!;
  expect(g.regions[c.reg].owner).toBe('GER');
  expect(names(g, c)).toEqual(['72nd Infantry Division', '164th Infantry Division']);
}

describe('a computer\'s row that falls back to forming at home takes nobody, with the far side on', () => {
  it('its host at peace: it forms at the capital, and every division it names stays with its corps', () => {
    const { g, told } = balkans(false);
    expect(farOn(g)).toBe(true);
    read(g, told, true);
  });

  it('its own place lost to the enemy: the same', () => {
    const { g, told } = balkans(true);
    // (Sofia as if it were Germany's own ground and Greece's army stood in it: `rowPlace`'s first branch.)
    const sofia = g.byName.Sofia;
    sofia.owner = 'GER';
    sofia.ctrl = 'GRE';
    read(g, told, false);
  });

  it('where the row is put down where it was meant to be, the same corps comes itself (so the two above are read)', () => {
    const { g, told } = balkans(true);
    const xxii = corps(g, 'XXII Corps')!;
    month(g, MARCH_41);
    const e = told.find((t) => t.row === PANZERS)!;
    expect(e.home).toBe(false);
    expect(e.claims.every((q) => !q.stays)).toBe(true);
    expect(corps(g, PANZERS)).toBe(xxii);
    expect(place(g, xxii)).toBe('Sofia');
  });
});
