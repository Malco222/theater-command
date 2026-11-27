// The contact rule's second form on the far side's clause (game.ts `expeditions`, the row that takes named corps whole
// with the far side on): a corps that leaves in the same act is nobody's relief (the design director's ruling of 06:10Z,
// 26 November 2026; section 13 A1 of /home/user/runs/contact2/SPEC.md: ONE `leavingNow` set per row, read by the far
// clause, the old chart's clause, `heldAlready` and the theatre draw). The old chart's clause and the draw are read in
// tests/contact-fact.test.ts; this file is the far clause's, which needs the far side on for the whole file.
// Germany's two test corps of one row's `from` on the Grenzmark, Poland's Army Poznan beside it in Poznan, nobody else
// of Germany's there; no battle recorded, so the contact half alone reads. The states are set by hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { setFromWait } from '../src/campaign/arrivalwaits';
import { contactRead, corpsInContact } from '../src/campaign/drawn';
import { addCorps, atWar, createGame, relocate, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { farOn } from '../src/campaign/world';
import type { Corps, Game } from '../src/campaign/types';

const ROWS = (setup as any).expeditions as any[];
let added = 0;
const addRow = (row: any[]): number => { ROWS.push(row); added++; return ROWS.length - 1; };
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } });
const named = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'GER' && !c.dead && c.name === name);

/** The far side on, the switch on; the Grenzmark emptied of Germany's own and two test corps put there; a row of month 2 taking both whole to Magdeburg. */
function board(): { g: Game; i: number; a: Corps; b: Corps } {
  const g = startOpen(createGame(3, null));
  expect(farOn(g)).toBe(true);
  expect(setFromWait(g, true)).toBe(true);
  const gz = g.byName.Grenzmark;
  for (const u of [...gz.units]) relocate(g, u, g.byName.Bavaria);
  expect(gz.units).toEqual([]);
  expect(atWar(g, 'GER', 'POL')).toBe(true);
  const a = addCorps(g, 'GER', 'First Test Corps', 'inf', 2, gz.id, {});
  const b = addCorps(g, 'GER', 'Second Test Corps', 'inf', 2, gz.id, {});
  // (Each keeps the province for the other where nothing takes them: the read that the row's own set must undo.)
  expect(contactRead(g, 'GER', a)).toEqual({ beside: true, battle: false, kept: true });
  expect(corpsInContact(g, 'GER', a, new Set([a, b]))).toBe(true);
  const i = addRow([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['First Test Corps', 'Second Test Corps'] }]);
  return { g, i, a, b };
}
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'GER'); };

describe("the far side's clause reads the row's own `leavingNow` (section 13 A1)", () => {
  it('two corps of one row on one province keep nothing for each other: the row waits, both where they stand', () => {
    const { g, i, a, b } = board();
    month(g, 2);
    expect(g.flags[`wait:${i}`]).toBe(2);
    expect(g.flags[`sent:${i}`]).toBeUndefined();
    expect([a.dead, b.dead]).toEqual([false, false]);
    expect([g.regions[a.reg].name, g.regions[b.reg].name]).toEqual(['Grenzmark', 'Grenzmark']);
    expect(named(g, 'Test Army')).toBeUndefined();
  });

  it('a third corps the row does not take keeps the province: the row is taken in its month', () => {
    const { g, i, a, b } = board();
    addCorps(g, 'GER', 'Relief Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
    month(g, 2);
    expect(g.flags[`wait:${i}`]).toBeUndefined();
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect([a.dead, b.dead]).toEqual([true, true]);
    expect(g.regions[named(g, 'Test Army')!.reg].name).toBe('Magdeburg');
  });
});
