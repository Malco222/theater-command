// Ships by class and navy (release 2): with the switch on (stage.json 'shipRows', forced here by TC_SHIPROWS, as the
// far side's is by TC_FAR), a campaign opened carries the flag from its first day and every fleet lays its hulls' rows
// as it forms, in a history campaign and on the wider world alike.
import { describe, expect, it, vi } from 'vitest';
vi.hoisted(() => { (globalThis as { TC_SHIPROWS?: unknown }).TC_SHIPROWS = 1; });
import { createGame } from '../src/campaign/game';
import { fleetPower } from '../src/campaign/navy';
import { listOf } from '../src/campaign/shiprows';
import { startOpen } from '../src/campaign/staff';
import { SHIPROWS_STAGE } from '../src/campaign/stage';

describe('the switch on', () => {
  it('a campaign opened carries the flag from its first day, and its fleets their rows as they form', () => {
    expect(SHIPROWS_STAGE).toBe(true);
    const g = createGame(1, null);
    expect(g.flags.shipRows).toBe(1);
    expect(listOf(g.fleets.find((f) => f.name === 'Battle Fleet')!, 'bb')).toEqual(['ger-scharnhorst', 'ger-scharnhorst']);
    const h = startOpen(createGame(2, null));
    expect(h.flags.shipRows).toBe(1);
    // The fleets of the wider world lay theirs as they join it (here the Americas: no row yet for their old hulls, Q5).
    const usa = h.fleets.find((f) => f.nat === 'USA' && f.name === 'Atlantic Fleet')!;
    expect(usa.ships.bb).toBeGreaterThan(0);
    expect(listOf(usa, 'bb').every((x) => x === null)).toBe(true);
    for (const f of h.fleets) expect(Number.isFinite(fleetPower(h, f))).toBe(true);
  });
});
