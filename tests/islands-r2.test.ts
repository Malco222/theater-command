// With the whole world off the islands' rows are not read at all (release 2, 25 November 2026).
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { islandNames, islandOf } from '../src/campaign/islands';
import { startOpen } from '../src/campaign/staff';

describe("the islands' rows with the whole world off", () => {
  it('answer nothing', () => {
    const g = startOpen(createGame(3, null));
    expect(islandNames()).toEqual([]);
    for (const r of g.regions) expect(islandOf(g, r)).toBeNull();
  }, 300000);
});
