// Operation Torch by the book (strategy.json 'torch'; the design director, 22 November 2026; after release 1).
// At the dial's two stops that keep to the book, Britain and America have the record's operation of November 1942
// in the form Germany's Norway has: its date in history.json, the declaration on Vichy, and the landings in Morocco
// and Algeria by named corps kept at their quays until the day. America's are picked from its own corps, never
// added; the French turn runs as the record wrote it in the landing's month; and the Axis answer in Tunisia is the
// book's dated rows, nothing twice. It ships switched off, and off it leaves no trace in a game. Nowhere is it a
// staff's own choice: at the stops that keep no book, the operation is on no menu.

import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import open from '../data/campaign/open.json';
import { createGame } from '../src/campaign/game';
import { HISTORY, leash, setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import { STRATEGY, torchBook } from '../src/campaign/strategy';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const start = (seed: number, stop: number, human: string | null = null): Game => {
  const g = startOpen(createGame(seed, human));
  if (stop !== 3) setStop(g, stop, true);
  return g;
};
type Row = [number, string, string, string, number, string, Record<string, unknown>?];
const TORCH = (setup as unknown as { torch: { instead: string[]; rows: Row[] } }).torch;
const ROSTER = (setup as unknown as { expeditions: Row[] }).expeditions;

describe('Operation Torch by the book', () => {
  const was = STRATEGY.torch!.on;
  afterEach(() => { STRATEGY.torch!.on = was; });

  it('ships switched off, with its data in order', () => {
    const T = STRATEGY.torch!;
    expect(T.on).toBe(false);
    // Only the two stops that keep to the book.
    expect(T.stops.every((s) => s < 3)).toBe(true);
    // Each operation named is on its staff's menu, dated by the book, and marked the book's alone.
    for (const key of T.ops) {
      const [nat, id] = key.split(':');
      const op = ((open as unknown as { ops: Record<string, { id: string; declare?: string[]; landings?: { corps: string; own?: number; to: string[] }[] }[]> }).ops[nat] ?? []).find((o) => o.id === id);
      expect(op, key).toBeTruthy();
      expect(op!.declare).toEqual(['VIC']);
      expect((op!.landings ?? []).length).toBeGreaterThan(0);
      expect((HISTORY.dates as Record<string, Record<string, number>>)[nat][id]).toBe(39);
      expect(HISTORY.bookOnly!.ops).toContain(key);
    }
    // America's landings are made from its own corps: every one names a worth, and none names a roster row.
    const usa = (open as unknown as { ops: Record<string, { id: string; landings?: { corps: string; own?: number }[] }[]> }).ops.USA.find((o) => o.id === 'torch')!;
    for (const L of usa.landings!) { expect(L.own).toBeGreaterThan(0); expect(ROSTER.some((r) => r[2] === L.corps)).toBe(false); }
    // Nothing twice: a row of Torch's that carries a roster row's name stands in for it, and every name stood in for is a roster row's.
    for (const nm of TORCH.instead) expect(ROSTER.some((r) => r[2] === nm), nm).toBe(true);
    for (const row of TORCH.rows) if (ROSTER.some((r) => r[1] === row[1] && r[2] === row[2])) expect(TORCH.instead, row[2]).toContain(row[2]);
    expect(new Set(TORCH.rows.map((r) => `${r[1]}:${r[2]}`)).size).toBe(TORCH.rows.length);
    // The Axis answer is rows with a date; Pistoia and La Spezia were Libya's and do not come to Tunis again.
    expect(TORCH.instead).toContain('Pistoia and La Spezia Divisions');
    expect(TORCH.rows.some((r) => /Pistoia/.test(r[2]))).toBe(false);
  });

  it('is on no menu with the switch off, nor at a stop that keeps no book, nor for a player', () => {
    for (const stop of [1, 3, 5]) {
      const g = start(31, stop);
      expect(torchBook(g)).toBe(false);
      expect(leash(g, 'UK', 'torch').kind).toBe('never');
      expect(leash(g, 'USA', 'torch').kind).toBe('never');
    }
    STRATEGY.torch!.on = true;
    for (const stop of [3, 4, 5]) {
      const g = start(31, stop);
      expect(torchBook(g)).toBe(false);
      expect(leash(g, 'UK', 'torch').kind).toBe('never');
    }
    // On, at the book's stops: dated, November 1942.
    for (const stop of [1, 2]) {
      const g = start(31, stop);
      expect(torchBook(g)).toBe(true);
      const L = leash(g, 'UK', 'torch'), A = leash(g, 'USA', 'torch');
      expect(L.kind).toBe('dated');
      expect(A.kind).toBe('dated');
      // (At stop 1 the book's own month; at stop 2 give or take its month of slack, and never two months for the one operation.)
      if (stop === 1) expect(L.date).toBe(39);
      else expect(Math.abs(L.date! - 39)).toBeLessThanOrEqual(1);
      expect(A.date).toBe(L.date);
    }
    for (let seed = 40; seed < 52; seed++) { const g = start(seed, 2); expect(leash(g, 'USA', 'torch').date).toBe(leash(g, 'UK', 'torch').date); }
    // Germany's own dated operation is untouched by any of it.
    expect(leash(start(31, 1), 'GER', 'weser').kind).toBe('dated');
  });

  it('on, at stop 1: the landings go in November 1942 from America\'s own corps, the French turn follows that month, and nothing comes twice', () => {
    STRATEGY.torch!.on = true;
    const g = start(17, 1);
    while (g.turn < 39) runTurnAI(g);
    // America's force is its own: two corps picked, standing where its convoys can reach the shore, and no row of the roster's sent.
    const picked = Object.keys(g.flags).filter((k) => k.startsWith('st:own:USA:'));
    expect(picked.length).toBe(2);
    const usaBefore = g.corps.filter((c) => c.nat === 'USA' && !c.dead).length;
    for (const k of picked) expect(g.corps.find((c) => c.id === g.flags[k] - 1 && c.nat === 'USA' && !c.dead)).toBeTruthy();
    expect(g.corps.some((c) => c.nat === 'USA' && /Task Force|II Corps \(1st Infantry/.test(c.name))).toBe(false);
    expect(g.nations.VIC.alive).toBe(true);
    expect(g.flags['event:darlan']).toBeUndefined();
    runTurnAI(g);
    // The day: both staffs went, somebody is ashore in Morocco or Algeria, and the French turned in that month.
    expect(g.flags['st:done:UK:torch']).toBe(39);
    expect(g.flags['st:done:USA:torch']).toBe(39);
    expect(g.flags['event:darlan']).toBe(39);
    expect(g.nations.VIC.alive).toBe(false);
    for (const nm of ['Casablanca', 'Oran', 'Algiers', 'Constantine']) expect(['UK', 'USA'], nm).toContain(g.byName[nm].ctrl);
    // No new American men for it: the count of its corps moved only by what its own factories and ships did (none struck, none added by Torch).
    expect(g.corps.filter((c) => c.nat === 'USA' && !c.dead && /Task Force/.test(c.name)).length).toBe(0);
    expect(g.corps.filter((c) => c.nat === 'USA' && !c.dead).length).toBeGreaterThanOrEqual(usaBefore);
    // The turn made none of its three Axis divisions: the Axis answer is the dated rows.
    expect(g.corps.some((c) => /von Broich|Saharan Group/.test(c.name))).toBe(false);
    // The 78th came once, by Torch's row, and the roster's own row for it was never sent.
    expect(g.corps.filter((c) => c.name === 'First Army: 78th Division').length).toBe(1);
    const i78 = ROSTER.findIndex((r) => r[2] === 'First Army: 78th Division');
    expect(g.flags[`sent:${i78}`]).toBeUndefined();
    // XIX Corps is made from the French army that came over, never added to it, and at the turn itself: it stands
    // on ground the French held, now the Allies', before the Axis moves again.
    expect(g.flags['torch:turned']).toBeGreaterThan(0);
    const xix = g.corps.filter((c) => c.name === 'French XIX Corps' && !c.dead);
    expect(xix.length).toBe(1);
    expect(xix[0].nat).toBe('UK');
    expect(xix[0].divs).toBeLessThanOrEqual(g.flags['torch:turned']);
    expect(['UK', 'USA']).toContain(g.regions[xix[0].reg].ctrl);
    expect(['Casablanca', 'Oran', 'Algiers', 'Constantine', 'Fez', 'Marrakech', 'Mareth', 'Sfax', 'Tunis']).toContain(g.regions[xix[0].reg].name);
    // The next month it is not made again.
    runTurnAI(g);
    expect(g.corps.filter((c) => c.name === 'French XIX Corps').length).toBe(1);
  }, 300_000);
});
