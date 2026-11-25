// The old chart's lanes and convoy routes all carry something laid, so the rule that a staff counts a lane only while
// it carries something (the design director's ruling of 12:39Z, 25 November 2026, Pacific batch 3, A(2)) moves no
// choice there.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { laneCounts, seaRoutes } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { attachWorld } from '../src/campaign/world';

describe('the old chart', () => {
  it('every lane and convoy route carries something laid, in Europe and with the Americas', () => {
    const g = startOpen(createGame(5, null));
    expect(seaRoutes(g).length).toBeGreaterThan(5);
    expect(seaRoutes(g).filter((r) => !laneCounts(r)).map((r) => r.id)).toEqual([]);
    const w = createGame(5, null);
    attachWorld(w);
    startOpen(w);
    expect(seaRoutes(w).filter((r) => !laneCounts(r)).map((r) => r.id)).toEqual([]);
  });
});
