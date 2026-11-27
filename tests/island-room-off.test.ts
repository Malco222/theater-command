// With the whole world off nothing reads the islands' table, and every province has room for five corps as it always
// had (rulings 173(b) and 179(s), form B3 of 25 November 2026: the islands' room is the whole-world chart's alone;
// counted in men since 14:28Z, still the whole-world chart's alone, with no new switch).
import { describe, expect, it } from 'vitest';
import { addCorps, createGame, hasRoom, placed, removeCorps, shoreRoom } from '../src/campaign/game';
import { fitWave, fits, islandNames, islandOf, roomLeft, roomRefusal, roomSays } from '../src/campaign/islands';
import { whyNotShip } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { STACK, type Region } from '../src/campaign/types';

describe("an island's room with the whole world off", () => {
  it('is never read: every province has room for five, and no province says an island\'s sentence', () => {
    const g = startOpen(createGame(3, null));
    expect(islandNames()).toEqual([]);
    for (const r of g.regions) {
      expect(islandOf(g, r)).toBeNull();
      expect(roomLeft(g, r, r.ctrl, 0), r.name).toBe(STACK);
      expect(roomSays(g, r)).toBeNull();
      expect(shoreRoom(g, r, 'GER', [])).toBe(Infinity);
    }
    // A province that bore an island's name would be no island here either: the table is not loaded at all, and a
    // corps of any size is counted as one of five, never in men.
    const wake = { ...g.regions[0], name: 'Wake', units: [] } as Region;
    expect(roomLeft(g, wake, 'USA', 0)).toBe(STACK);
    const army = addCorps(g, 'USA', 'Big Army', 'inf', 4, g.byName['Washington'].id, {});
    expect(fits(g, wake, 'USA', [army], { rows: 4 })).toBe(true);
    expect(fits(g, wake, 'USA', [army], { rows: 5 })).toBe(false);
    expect(fits(g, wake, 'USA', [army], { rows: 6, slack: 2 })).toBe(true);
    expect(fits(g, wake, 'USA', [army], { rows: 7, slack: 2 })).toBe(false);
    expect(fitWave(g, wake, 'USA', [army, army])).toEqual([army, army]);
    expect(roomRefusal(g, wake, [army])).toBeNull();
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
