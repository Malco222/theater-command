// Britain east of Suez on the whole-world chart (ruling 189(e); the historian's pacific-allied-fleet-rows.md, sections
// 2a to 2e; the audit's re-check of 25 November 2026). At stops 1 and 2 the ships that went east come out of the
// European rows on their dates, and back on theirs: Force Z in December 1941, the Eastern Fleet of 1942 and 1944, the
// British Pacific Fleet from November 1944. At stops 3 to 5 none of these rows is read (the staffs' plans decide).
// A dial turned to the book later has the record's ships: every row on or before the month is read at once.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { setStop, stopOf } from '../src/campaign/history';
import { NAVY, navalUpkeep } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game, Ships } from '../src/campaign/types';

const named = (g: Game, name: string): Fleet | undefined => g.fleets.find((f) => f.nat === 'UK' && f.name === name);
const ships = (f: Fleet | undefined): Ships => ({ ...(f?.ships ?? { bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 }) });
const sum = (g: Game, c: keyof Ships): number => g.fleets.filter((f) => f.nat === 'UK').reduce((s, f) => s + f.ships[c], 0);

/** A computer's war at a stop, with Britain's month at sea read in month `turn` (every row on or before it, once). */
function at(stop: number, turn: number): Game {
  const g = startOpen(createGame(5, null));
  if (stop !== 3) setStop(g, stop, true);
  expect(stopOf(g)).toBe(stop);
  g.turn = turn;
  navalUpkeep(g, 'UK');
  return g;
}

describe('the rows', () => {
  it('are laid for Britain at stops 1 and 2 only, and the British Pacific Fleet forms by its book alone', () => {
    const east = NAVY.arrivals.filter((r) => r[1] === 'UK' && r[5]?.stops && /China Station|Eastern Fleet|Force Z|British Pacific|Revenge|Howe|escort carriers|Indefatigable|Implacable/.test(r[4] + r[2]));
    expect(east.length).toBeGreaterThan(10);
    for (const r of east) expect(r[5]!.stops).toEqual([1, 2]);
    const bpf = NAVY.fleets.find((f) => f.nat === 'UK' && f.name === 'British Pacific Fleet')!;
    expect(bpf.book).toEqual({ stops: [1, 2], from: 63, ships: { bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 } });
    expect(bpf.from).toBeGreaterThan(80);
  });
});

describe('at stop 1', () => {
  it('in December 1941 the China Fleet is Force Z: two capital ships, three old cruisers and eight destroyers', () => {
    const g0 = startOpen(createGame(5, null));
    setStop(g0, 1, true);
    const home0 = ships(named(g0, 'Home Fleet'));
    const g = at(1, 28);
    const cf = ships(named(g, 'China Fleet'));
    expect(cf).toMatchObject({ bb: 2, cv: 0, ca: 3, dd: 8 });
    // The battleship and the battlecruiser came out of the Home Fleet: no ship is in two oceans.
    const home = ships(named(g, 'Home Fleet'));
    expect(home.bb).toBeLessThan(home0.bb + 2);
    expect(g.headlines.some((h) => /Force Z/.test(h.text))).toBe(true);
  });

  it('in November 1944 the British Pacific Fleet forms at Trincomalee with ships from the Eastern Fleet and home', () => {
    const before = at(1, 62);
    const g = at(1, 63);
    const bpf = named(g, 'British Pacific Fleet')!;
    expect(bpf).toBeDefined();
    expect(bpf.port).toBe('Trincomalee');
    expect(bpf.ships.cv).toBeGreaterThanOrEqual(2);
    expect(bpf.ships.bb).toBeGreaterThanOrEqual(1);
    expect(bpf.ships.dd).toBe(15);
    // Its carriers are the Eastern Fleet's and one new one: Britain's carriers grow by the new ones only.
    expect(sum(g, 'cv') - sum(before, 'cv')).toBeLessThanOrEqual(2);
  });
});

describe('at stop 3', () => {
  it('none of the eastern rows is read: the China Fleet keeps its first-day ships but the Eagle, and no British Pacific Fleet forms', () => {
    const g = at(3, 63);
    expect(ships(named(g, 'China Fleet'))).toMatchObject({ bb: 0, cv: 0, ca: 8, dd: 9 });
    expect(named(g, 'British Pacific Fleet')).toBeUndefined();
  });
});
