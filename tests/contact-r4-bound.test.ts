// The contact rule, round 4, R4-1: an enemy its own orders bind to its place is not "the enemy beside" for the ALONE half
// of the one test (the design director's (1) of 16:07Z, 27 November 2026: "Lean taken. An enemy its own orders bind to its
// place is not "the enemy beside" for the alone half. Read it from the mover's own function, on that month's orders. No
// timer." Finding S1-3: seed 29, the 2nd New Zealand Division alone at Jarabub beside Italy's Kufra garrison, held from
// month 20 to month 36, and W Force waiting with it). The FIGHT half is untouched: a corps that took part in the battle
// last resolved on its ground with an enemy beside is held whoever that enemy is.
// "Bound", as the mover reads it: the corps' own order to hold where it stands (`hold`), which every picker of the
// general's (ai.ts) reads before it moves a corps, and which only the general's own orders lift; never a corps of the
// player's (the general never moves one: his own orders are his). Seed 1, the first day, the old chart: Germany's III
// Corps moved out of the Neumark, a test corps alone there, Poland's Army Poznan and the Wielkopolska Cavalry beside it in
// Poznan (Poland's), as tests/contact-r3.test.ts sets the board.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { setFromWait } from '../src/campaign/arrivalwaits';
import { contactRead, corpsInContact, recordBattle } from '../src/campaign/drawn';
import { addCorps, atWar, createGame, relocate, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

const ROWS = (setup as any).expeditions as any[];
let added = 0;
const addRow = (row: any[]): number => { ROWS.push(row); added++; return ROWS.length - 1; };
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } });
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'GER'); };

function board(o: { on?: boolean; bind?: ('army' | 'cav')[]; human?: string } = {}): { g: Game; nm: Region; pz: Region; c: Corps; army: Corps; cav: Corps } {
  const g = startOpen(createGame(1, o.human ?? null));
  if (o.on ?? true) expect(setFromWait(g, true)).toBe(true);
  const nm = g.byName.Neumark, pz = g.byName.Poznan;
  for (const u of [...nm.units]) relocate(g, u, g.byName.Bavaria);
  expect(atWar(g, 'GER', 'POL')).toBe(true);
  // (The Neumark's only neighbour at war with Germany is Poznan, Poland's: its two corps, free as the data has them.)
  expect(nm.nb.map((j) => g.regions[j]).filter((r) => atWar(g, 'GER', r.ctrl) || r.units.some((u) => atWar(g, 'GER', u.nat))).map((r) => r.name)).toEqual(['Poznan']);
  const army = pz.units.find((u) => u.name === 'Army Poznan')!, cav = pz.units.find((u) => u.name === 'Wielkopolska Cavalry')!;
  expect([army.hold, cav.hold, pz.units.length]).toEqual([false, false, 2]);
  for (const k of o.bind ?? []) (k === 'army' ? army : cav).hold = true;
  const c = addCorps(g, 'GER', 'Lone Test Corps', 'inf', 1, nm.id, {});
  return { g, nm, pz, c, army, cav };
}

describe('R4-1: an enemy bound to its place by its own orders is not the enemy beside, for the alone half only', () => {
  it('every enemy corps beside bound by its own orders, and its province: the corps alone is NOT held', () => {
    const { g, c } = board({ bind: ['army', 'cav'] });
    expect(contactRead(g, 'GER', c)).toEqual({ beside: true, battle: false, kept: true });
    expect(corpsInContact(g, 'GER', c)).toBe(false);
  });

  it('the mirror: the same enemy with its orders lifted (free, as the data has it): held, as today', () => {
    const { g, c } = board();
    expect(contactRead(g, 'GER', c)).toEqual({ beside: true, battle: false, kept: false });
    expect(corpsInContact(g, 'GER', c)).toBe(true);
  });

  it('two enemies beside, one bound and one free: held', () => {
    const { g, c } = board({ bind: ['army'] });
    expect(contactRead(g, 'GER', c)).toEqual({ beside: true, battle: false, kept: false });
    expect(corpsInContact(g, 'GER', c)).toBe(true);
  });

  it('the fight half untouched: a bound enemy that fought it in the battle last resolved on its ground: held', () => {
    const { g, nm, c, army, cav } = board({ bind: ['army', 'cav'] });
    recordBattle(g, nm, [c, army, cav]);
    expect(contactRead(g, 'GER', c)).toEqual({ beside: true, battle: true, kept: true });
    expect(corpsInContact(g, 'GER', c)).toBe(true);
  });

  it("the player's own corps under an order to hold: the general never moves his corps, so it binds nothing: held", () => {
    const { g, c } = board({ bind: ['army', 'cav'], human: 'POL' });
    expect(corpsInContact(g, 'GER', c)).toBe(true);
  });

  it('switch off: the base test, an enemy beside, bound or not', () => {
    const { g, c } = board({ on: false, bind: ['army', 'cav'] });
    expect(corpsInContact(g, 'GER', c)).toBe(true);
  });

  it("on the rule's own path: a lent corps alone beside bound enemies only is let go when its loan is over", () => {
    const { g, c } = board({ bind: ['army', 'cav'] });
    const j = addRow([1, 'GER', c.name, 'inf', 1, 'Neumark', { until: 2 }]);
    g.flags[`sent:${j}`] = 1;
    g.flags[`lent:${j}`] = c.id;
    month(g, 2);
    expect([c.dead, g.flags[`lent:${j}`], g.flags[`wait:${j}`]]).toEqual([true, -1, undefined]);
  });

  it('and with the enemy free, the same loan is held, its wait written', () => {
    const { g, c } = board();
    const j = addRow([1, 'GER', c.name, 'inf', 1, 'Neumark', { until: 2 }]);
    g.flags[`sent:${j}`] = 1;
    g.flags[`lent:${j}`] = c.id;
    month(g, 2);
    expect([c.dead, g.flags[`lent:${j}`], g.flags[`wait:${j}`]]).toEqual([false, c.id, 2]);
  });
});
