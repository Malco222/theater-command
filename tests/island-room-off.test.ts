// With the whole world off nothing reads the islands' table, and every province has room for five corps as it always
// had (rulings 173(b) and 179(s), form B3 of 25 November 2026: the islands' room is the whole-world chart's alone).
import { describe, expect, it } from 'vitest';
import { addCorps, createGame, hasRoom, placed, removeCorps, shoreRoom } from '../src/campaign/game';
import { islandNames, islandOf, roomAt, roomSays } from '../src/campaign/islands';
import { whyNotShip } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { STACK, type Region } from '../src/campaign/types';

describe("an island's room with the whole world off", () => {
  it('is never read: every province has room for five, and no province says an island\'s sentence', () => {
    const g = startOpen(createGame(3, null));
    expect(islandNames()).toEqual([]);
    for (const r of g.regions) {
      expect(islandOf(g, r)).toBeNull();
      expect(roomAt(g, r, r.ctrl), r.name).toBe(STACK);
      expect(roomSays(g, r)).toBeNull();
      expect(shoreRoom(g, r, 'GER')).toBe(Infinity);
    }
    // A province that bore an island's name would be no island here either: the table is not loaded at all.
    expect(roomAt(g, { ...g.regions[0], name: 'Wake' } as Region, 'USA')).toBe(STACK);
  }, 300000);

  it('a harbour takes its fifth corps and refuses a sixth in the words it always used', () => {
    const g = startOpen(createGame(3, 'UK'));
    const malta = g.byName['Malta'], london = g.byName['London'];
    expect(malta.port && london.port).toBe(true);
    for (const u of [...malta.units]) removeCorps(g, u);
    const cs = [1, 2, 3, 4, 5, 6].map((k) => { const c = addCorps(g, 'UK', `Corps ${k}`, 'inf', 1, london.id, {}); c.ap = c.apMax; return c; });
    for (const c of cs.slice(0, 5)) {
      expect(hasRoom(g, malta, c), c.name).toBe(true);
      removeCorps(g, c);
      addCorps(g, 'UK', c.name, 'inf', 1, malta.id, {});
    }
    expect(placed(g, malta, 'UK')).toHaveLength(5);
    expect(hasRoom(g, malta, cs[5])).toBe(false);
    expect(whyNotShip(g, cs[5], malta)).toBe('Malta has no room for another corps.');
  }, 300000);
});
