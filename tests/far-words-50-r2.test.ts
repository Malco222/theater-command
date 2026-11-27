// The far side's fleet news reads right (the 1.5.1 playtest's #50, 25 November 2026): a row that opens with a count of
// two or more takes the plural ("Two new fleet carriers join the Mobile Fleet"), a row's words carry no verb of their
// own, and no carrier is "named for the one lost" at a battle this war may never have had (sandbox law).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { NAVY, navalUpkeep } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

/** A computer's war with this nation's month at sea read in month `turn` (every row on or before it, once). */
function at(nat: string, turn: number): Game {
  const g = startOpen(createGame(5, null));
  g.turn = turn;
  navalUpkeep(g, nat);
  return g;
}

describe("the far side's fleet news", () => {
  it('takes the plural after a count: two new fleet carriers join the Mobile Fleet', () => {
    const g = at('JAP', 25);
    expect(g.headlines.some((h) => h.text === 'Two new fleet carriers join the Mobile Fleet.')).toBe(true);
    expect(g.headlines.some((h) => /carriers joins/.test(h.text))).toBe(false);
  });

  it('reads one verb: twenty-three submarines, come out to Manila, join the Asiatic Fleet', () => {
    const g = at('USA', 27);
    expect(g.headlines.some((h) => h.text === 'Twenty-three submarines, come out to Manila, join the Asiatic Fleet.')).toBe(true);
    expect(g.headlines.some((h) => /reach Manila joins/.test(h.text))).toBe(false);
  });

  it('names no carrier for a loss at a battle of the record', () => {
    const rows = NAVY.arrivals.filter((r) => r[1] === 'USA');
    expect(rows.length).toBeGreaterThan(10);
    for (const r of rows) expect(r[4]).not.toMatch(/named for the one lost/);
    expect(rows.filter((r) => r[4] === 'another new Essex-class carrier').length).toBe(4);
  });
});
