// The Soviet Far East's rows that leave the far chart (the design director's ruling of 20:03Z, 22 November 2026).
//
// The fault it mends: the 16th Army was counted twice. The old chart's own book raises it at Kiev in May 1941, three
// divisions, and the far chart's Transbaikal row at Chita held the same three through the war. The Chita row's three
// now leave in the month the book raises the army in the west. Every state here is set by hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, createGame } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { PACIFIC, pacificOpening } from '../src/campaign/pacific';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';

const W = PACIFIC!.west!;
/** A campaign at a stop, with the far chart's two dated Soviet rows of 1941 stood up by hand (no month is played here). */
const grand = (stop: number, human: string | null = null): Game => {
  const g = startOpen(createGame(3, human));
  if (stop !== 3) setStop(g, stop, true);
  addCorps(g, 'SOV', 'Transbaikal Military District (Chita, new divisions)', 'inf', 3, 'Chita', { hold: 1 });
  addCorps(g, 'SOV', '1st Red Banner Army (the Ussuri, new divisions)', 'inf', 2, 'Iman', { hold: 1 });
  return g;
};
const named = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'SOV' && !c.dead && c.name === name);
const month = (g: Game, turn: number): void => { g.turn = turn; pacificOpening(g); };

describe('the far side: the Soviet Far East\'s rows that leave it', () => {
  it('the 16th Army leaves the Chita row in May 1941, the month the old chart\'s book raises it at Kiev', () => {
    expect((setup as any).reinforcements.some((r: any[]) => r[0] === 21 && r[1] === 'SOV' && r[2] === '16th Army' && r[4] === 3 && r[5] === 'Kiev')).toBe(true);
    expect(W.leaves).toEqual([[21, 'Transbaikal Military District (Chita)', 'The 16th Army entrains at Chita for Kiev.']]);
    const g = grand(1, 'SOV');
    expect(named(g, 'Transbaikal Military District (Chita)')!.divs).toBe(3);
    month(g, 20);
    expect(named(g, 'Transbaikal Military District (Chita)')).toBeDefined();
    month(g, 21);
    expect(named(g, 'Transbaikal Military District (Chita)')).toBeUndefined();
    expect(g.headlines.filter((h) => h.text === 'The 16th Army entrains at Chita for Kiev.').length).toBe(1);
    // The Transbaikal's rows are then eight: the new divisions' three, Dauria's two and its tanks, Nerchinsk and the reserve.
    const transbaikal = g.corps.filter((c) => c.nat === 'SOV' && !c.dead && ['Chita', 'Dauria', 'Nerchinsk', 'Ulan-Ude'].includes(g.regions[c.reg].name)).reduce((n, c) => n + c.divs, 0);
    expect(transbaikal).toBe(8);
    // A campaign that opens too late for the book's own row to form keeps the far chart's.
    const late = grand(1);
    month(late, 30);
    expect(named(late, 'Transbaikal Military District (Chita)')).toBeDefined();
    month(late, 31);
    expect(named(late, 'Transbaikal Military District (Chita)')).toBeDefined();
  });
});
