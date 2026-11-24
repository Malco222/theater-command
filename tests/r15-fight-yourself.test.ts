// Release 1.5: FIGHT IT YOURSELF on release 1's campaign. The campaign half (Rules/battle-bridge.ts, ported from release 2)
// gives the card's battle as a ticket, the 3D game's own land rules fight it, and the result comes back through the
// campaign's own attack order with the field's R in place of the dice and the field's bill. Release 2's looks ride
// on the ticket only as the campaign remembers them (tests/r15-looks.test.ts). Without a fought battle the attack rolls the dice as release 1 always has.
import { describe, expect, it } from 'vitest';
import '../unreal/TheaterCommand/Rules/campaign-bridge';
import { liveTicket } from '../src/battle/land/live';
import { loadGame } from '../src/campaign/save';
import type { FieldResult } from '../src/battle/core/types';

const TC = (globalThis as unknown as { TC: Record<string, (...a: unknown[]) => any> }).TC;
type Look = { ticket: Record<string, any> | null; live?: Record<string, any>; why?: string } | null;

/** Germany, September 1939 (seed 1): XIX Corps (id 1) onto Pomorze (39), the Mac's own round trip. */
function fresh() {
  TC.start({ human: 'GER', from: 1, view: 'grand', seed: 1 });
}

function fight(look: NonNullable<Look>): FieldResult {
  const L = liveTicket(structuredClone(look.ticket) as never, { ...(look.live ?? {}), me: 'att' } as never);
  for (let guard = 0; !L.sim.over && guard < 100000; guard++) L.tick(0.5, 1000);
  expect(L.sim.over).toBeTruthy();
  return L.sim.over as FieldResult;
}

describe('release 1.5: FIGHT IT YOURSELF on release 1', () => {
  it('the card\'s battle comes as a ticket the land rules fight, with no memory of earlier battles on it', () => {
    fresh();
    const look = TC.battleLook([1], 39) as Look;
    expect(look?.ticket).toBeTruthy();
    expect(look!.ticket!.place.name).toBe('Pomorze');
    expect(look!.ticket!.att.nat).toBe('GER');
    for (const k of ['scars', 'oldRuts', 'captured']) expect(look!.ticket![k]).toBeUndefined(); // (a war's first month: no field fought over yet, nothing taken; the weather word may ride, tests/r15-looks.test.ts)
    for (const s of ['att', 'def']) expect(look!.ticket![s].worn).toBeUndefined();
    expect(look!.live?.designYears?.GER).toBeGreaterThanOrEqual(1939);
    const header = liveTicket(structuredClone(look!.ticket) as never, { ...(look!.live ?? {}), me: 'att' } as never).header() as Record<string, any>;
    for (const k of ['scars', 'oldRuts']) expect(header[k]).toBeUndefined();
    expect((header.blocks as { captured?: string }[]).some((b) => b.captured)).toBe(false);
  });

  it('the result goes back through the campaign\'s attack, once, and only for the battle that was fought', () => {
    fresh();
    const look = TC.battleLook([1], 39) as Look;
    const over = fight(look!);
    const seed = look!.ticket!.seed as number;
    expect(TC.battleSettleLand([1], 39, { over }, seed + 1)).toEqual({ why: expect.any(String) });
    const out = TC.battleSettleLand([1], 39, { over }, seed) as { report?: { level?: string; region?: number } };
    expect(out?.report?.level).toMatch(/victory|defeat/i);
    // (asked again, the same answer and nothing booked twice)
    const saved = TC.save().text;
    expect(TC.battleSettleLand([1], 39, { over }, seed)).toEqual(out);
    expect(TC.save().text).toBe(saved);
    // (the campaign booked it: the report is the month's last, at Pomorze)
    const g = loadGame(saved)!;
    expect(g.reports[g.reports.length - 1].region).toBe(39);
  });

  it('a field won holds the bill near what the field cost (ruling 226(6))', () => {
    fresh();
    const look = TC.battleLook([1], 39) as Look;
    const over = fight(look!);
    const before = loadGame(TC.save().text)!.corps.find((c) => c.id === 1)!;
    const men0 = before.str;
    TC.battleSettleLand([1], 39, { over }, look!.ticket!.seed);
    const after = loadGame(TC.save().text)!.corps.find((c) => c.id === 1);
    const lostShare = after ? 1 - after.str / men0 : 1;
    const field = (over as unknown as { lost?: { att?: number } }).lost?.att;
    if (typeof field === 'number' && field > 0) expect(Math.abs(lostShare - field)).toBeLessThan(Math.max(0.05, field * 0.3));
    expect(lostShare).toBeGreaterThanOrEqual(0);
  });
});
