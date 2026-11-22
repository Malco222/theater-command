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
import { createGame, men, torchRowsOnly } from '../src/campaign/game';
import { HISTORY, leash, setStop } from '../src/campaign/history';
import { bookLanding, startOpen } from '../src/campaign/staff';
import { STRATEGY, torchBook } from '../src/campaign/strategy';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const start = (seed: number, stop: number, human: string | null = null): Game => {
  const g = startOpen(createGame(seed, human));
  if (stop !== 3) setStop(g, stop, true);
  return g;
};
type Row = [number, string, string, string, number, string, Record<string, unknown>?];
const TORCH = (setup as unknown as { torch: { instead: string[]; rows: Row[]; vichy: { nat: string; from: number; ground: string[]; places: [string, number, string][] }; america: { nats: string[] } } }).torch;
const ROSTER = (setup as unknown as { expeditions: Row[] }).expeditions;
type Landing = { corps: string; own?: number; to: string[] };
const LANDINGS = (nat: string): Landing[] => (open as unknown as { ops: Record<string, { id: string; landings?: Landing[] }[]> }).ops[nat].find((o) => o.id === 'torch')!.landings!;

describe('Operation Torch by the book', () => {
  const was = STRATEGY.torch!.on;
  afterEach(() => { STRATEGY.torch!.on = was; STRATEGY.torch!.ceasefire = false; });

  it('ships switched off, with its data in order', () => {
    const T = STRATEGY.torch!;
    expect(T.on).toBe(false);
    // (The step struck on 22 November 2026 is behind its own key, and that key ships off.)
    expect(T.ceasefire).toBe(false);
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
    // The first shore each landing names is the record's: Algiers for the Eastern force, Casablanca and Oran for America's two.
    expect(LANDINGS('UK').map((L) => L.to[0])).toEqual(['Algiers']);
    expect(LANDINGS('USA').map((L) => L.to[0])).toEqual(['Casablanca', 'Oran']);
    // Nothing twice: a row of Torch's that carries a roster row's name stands in for it, and every name stood in for is a roster row's.
    for (const nm of TORCH.instead) expect(ROSTER.some((r) => r[2] === nm), nm).toBe(true);
    for (const row of TORCH.rows) if (ROSTER.some((r) => r[1] === row[1] && r[2] === row[2])) expect(TORCH.instead, row[2]).toContain(row[2]);
    expect(new Set(TORCH.rows.map((r) => `${r[1]}:${r[2]}`)).size).toBe(TORCH.rows.length);
    // The Axis answer is rows with a date; Pistoia and La Spezia were Libya's and do not come to Tunis again.
    expect(TORCH.instead).toContain('Pistoia and La Spezia Divisions');
    expect(TORCH.rows.some((r) => /Pistoia/.test(r[2]))).toBe(false);
    // The Axis rows hold the bridgehead they land in: every one is held and marked to stay. No Allied row is.
    for (const r of TORCH.rows) { const axis = r[1] === 'GER' || r[1] === 'ITA'; expect(!!r[6]?.hold && !!r[6]?.stay, r[2]).toBe(axis); }
    // Vichy's placing on the day: the record's commands, eight divisions as the 1939 rows have, every place on the
    // ground named and laid before the day; and America is the nation whose divisions there are the book's rows.
    const V = TORCH.vichy;
    expect(V.places.reduce((n, x) => n + x[1], 0)).toBe(8);
    expect((setup as unknown as { corps: [string, string, string, number, string][] }).corps.filter((r) => r[0] === 'FRA' && V.ground.includes(r[4])).reduce((n, r) => n + r[3], 0)).toBe(8);
    for (const [at] of V.places) expect(V.ground, at).toContain(at);
    expect(V.places.slice(0, 3).map((x) => x[0])).toEqual(['Algiers', 'Oran', 'Casablanca']);
    expect(V.from).toBeLessThan(39);
    expect(TORCH.america.nats).toEqual(['USA']);
  });

  it('Vichy stands where the record had it on the day, with no division added or struck; never with the switch off', () => {
    const where = (g: Game): string => g.corps.filter((c) => c.nat === 'VIC' && !c.dead && TORCH.vichy.ground.includes(g.regions[c.reg].name)).map((c) => `${g.regions[c.reg].name} ${c.divs}`).sort().join(', ');
    const count = (g: Game): number => g.corps.filter((c) => c.nat === 'VIC' && !c.dead && TORCH.vichy.ground.includes(g.regions[c.reg].name)).reduce((n, c) => n + men(c), 0);
    // Off: the 1939 rows where they have stood since.
    const off = start(17, 1);
    while (off.turn < 39) runTurnAI(off);
    expect(where(off)).toBe('Algiers 3, Casablanca 2, Mareth 3');
    expect(off.flags['torch:vichy']).toBeUndefined();
    // On, at stop 1: the 1939 rows until the month the data names, then the record's commands.
    STRATEGY.torch!.on = true;
    const g = start(17, 1);
    while (g.turn < TORCH.vichy.from - 1) runTurnAI(g);
    expect(where(g)).toBe('Algiers 3, Casablanca 2, Mareth 3');
    const before = count(g);
    runTurnAI(g);
    expect(g.flags['torch:vichy']).toBe(TORCH.vichy.from);
    expect(where(g)).toBe('Algiers 1, Casablanca 1, Constantine 1, Fez 2, Marrakech 1, Oran 1, Tunis 1');
    expect(count(g)).toBe(before);
    for (const c of g.corps) if (c.nat === 'VIC' && TORCH.vichy.ground.includes(g.regions[c.reg].name)) expect(c.hold, c.name).toBe(true);
    // Once: the next month it is not laid again.
    runTurnAI(g);
    expect(g.flags['torch:vichy']).toBe(TORCH.vichy.from);
    // At a stop that keeps no book, never.
    const free = start(17, 3);
    while (free.turn < 39) runTurnAI(free);
    expect(free.flags['torch:vichy']).toBeUndefined();
    // America's army across the ocean is sailed into no harbour of that ground where Torch is the book's, and only there.
    expect(torchRowsOnly(g, 'USA', g.byName.Oran)).toBe(true);
    expect(torchRowsOnly(g, 'USA', g.byName.Cornwall)).toBe(false);
    expect(torchRowsOnly(g, 'UK', g.byName.Oran)).toBe(false);
    expect(torchRowsOnly(free, 'USA', free.byName.Oran)).toBe(false);
    expect(torchRowsOnly(start(17, 1, 'USA'), 'USA', g.byName.Oran)).toBe(false);
    STRATEGY.torch!.on = false;
    expect(torchRowsOnly(g, 'USA', g.byName.Oran)).toBe(false);
  }, 300_000);

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
    // By the book a landing of Torch's goes to its first-named shore (on its day whatever the odds); with the switch
    // off, at a stop that keeps no book, or for a player, it is a landing like any other.
    for (const nat of ['UK', 'USA']) for (const L of LANDINGS(nat)) {
      expect(bookLanding(start(31, 1), nat, L)).toBe('after');
      expect(bookLanding(start(31, 3), nat, L)).toBeNull();
      expect(bookLanding(start(31, 1, nat), nat, L)).toBeNull();
    }
    STRATEGY.torch!.on = false;
    for (const nat of ['UK', 'USA']) for (const L of LANDINGS(nat)) expect(bookLanding(start(31, 1), nat, L)).toBeNull();
    STRATEGY.torch!.on = true;
    // Germany's own dated operation is untouched by any of it.
    expect(leash(start(31, 1), 'GER', 'weser').kind).toBe('dated');
  });

  it('on, at stop 1: the landings go in November 1942 at the record\'s three ports from America\'s own corps, the French turn follows that month, and nothing comes twice', () => {
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
    // The landings went where the record's did, and nowhere else: each force is on its own shore or back in a harbour
    // of its side (the day's fight threw it off), never on another shore of the list. The Center force took Oran.
    const at = (c: { reg: number } | undefined): string | undefined => (c ? g.regions[c.reg].name : undefined);
    const others = [...LANDINGS('UK'), ...LANDINGS('USA')].flatMap((L) => L.to.slice(1));
    const e = g.corps.find((c) => c.name === 'First Army: 78th Division' && !c.dead);
    const w = g.corps.find((c) => c.id === g.flags['st:own:USA:Western Task Force'] - 1 && !c.dead);
    expect(e && w).toBeTruthy();
    for (const [c, shore] of [[e!, 'Algiers'], [w!, 'Casablanca']] as const) {
      expect(others).not.toContain(at(c));
      if (at(c) !== shore) expect(g.regions[c.reg].port).toBeTruthy();
    }
    expect(at(g.corps.find((c) => c.id === g.flags['st:own:USA:Center Task Force'] - 1 && !c.dead))).toBe('Oran');
    // (The struck step's key is off: no force is noted as lying off its shore.)
    expect(Object.keys(g.flags).some((k) => k.startsWith('torch:off:'))).toBe(false);
    // America's divisions in French North Africa are the book's rows: nothing of its army stands there but the two picks.
    const picks = new Set(picked.map((k) => g.flags[k] - 1));
    for (const c of g.corps) if (c.nat === 'USA' && !c.dead && TORCH.vichy.ground.includes(g.regions[c.reg].name)) expect(picks.has(c.id), c.name).toBe(true);
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

  it('the struck step, with its own key on: a force thrown back on the day is ashore at its shore after the turn', () => {
    STRATEGY.torch!.on = true;
    STRATEGY.torch!.ceasefire = true;
    const g = start(17, 1);
    while (g.turn < 40) runTurnAI(g);
    const at = (c: { reg: number } | undefined): string | undefined => (c ? g.regions[c.reg].name : undefined);
    expect(at(g.corps.find((c) => c.name === 'First Army: 78th Division' && !c.dead))).toBe('Algiers');
    expect(at(g.corps.find((c) => c.id === g.flags['st:own:USA:Western Task Force'] - 1 && !c.dead))).toBe('Casablanca');
    expect(at(g.corps.find((c) => c.id === g.flags['st:own:USA:Center Task Force'] - 1 && !c.dead))).toBe('Oran');
    expect(Object.keys(g.flags).some((k) => k.startsWith('torch:off:'))).toBe(false);
  }, 300_000);
});
