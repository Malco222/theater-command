// The book's line in East Africa (the design director's 13:43Z of 22 November 2026, form (ii) to his C; line.ts,
// data/campaign/far-africa.json, from the historian's iea-sahara-1940-41.md).
//
// As first laid, with no line, Italy's staff read seventeen to two in the first month of its war and went where it
// could: in war 3 at the book's first stop Kassala and Berbera fell in month 10, Port Sudan in 11 and Khartoum in 13,
// and in war 7 Britain's staff was in Addis Ababa in month 15. The record: Kassala in July 1940, British Somaliland
// in August, and no further; Addis Ababa on 6 April 1941. At the stops that keep the book the staffs there are bound
// by the record's line, as China's are: Italy's own ground to each province's last month, its gains in their months,
// and round them a ring it never held. The readings are made on a board set by hand; one war is walked to see it kept.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { HISTORY, setStop } from '../src/campaign/history';
import { lineLeaves, lineShut, lines, type Line } from '../src/campaign/line';
import { startOpen } from '../src/campaign/staff';
import { stepAI } from '../src/campaign/turn';
import { shoreOf } from '../src/campaign/world';
import type { Game, Region } from '../src/campaign/types';
import africa from '../data/campaign/far-africa.json';

type Row = [number, string, string, string, number, string, Record<string, any>];
const ROWS = (africa as any)['setup-1939'].expeditions as Row[];
const SAHARA = ['Sebha', 'Murzuk', 'Ghat', 'Waw el Kebir', 'Kufra'];
const GAINS: Record<string, [number, number]> = { Kassala: [11, 17], Gallabat: [11, 18], Kurmuk: [11, 18], Hargeisa: [12, 19], Berbera: [12, 19], Burao: [12, 19] };
const line = (): Line => lines().find((L) => L.id === 'eastafrica')!;
/** A war at a stop of the dial in a month set by hand. */
const board = (stop: number, turn: number, human: string | null = null): Game => {
  const g = startOpen(createGame(3, human));
  setStop(g, stop, true);
  g.turn = turn;
  return g;
};
const italys = (g: Game): Region[] => g.regions.filter((r) => r.owner === 'ITA' && shoreOf(r) === 2 && !SAHARA.includes(r.name));

describe('the book\'s line in East Africa: the data (the design director, 13:43Z of 22 November 2026)', () => {
  it('names every province of Italian East Africa to its last month, which is its row\'s, and the six gains in theirs', () => {
    const g = startOpen(createGame(3, null));
    const L = line();
    expect(L).toBeTruthy();
    expect([L.taker, L.land, L.ground, L.until, L.stays]).toEqual(['ITA', 'UK', 'named', 27, undefined]);
    const own = italys(g);
    expect(own.length).toBe(36);
    for (const r of own) {
      const spans = L.spans[r.name];
      expect(spans, r.name).toHaveLength(1);
      expect([spans[0].from, spans[0].kind], r.name).toEqual([1, 'own']);
      expect(L.back![r.name], r.name).toBe(spans[0].to);
    }
    // (A row's last month is its place's on the line.)
    for (const r of ROWS.filter((x) => x[1] === 'ITA' && x[6].until !== undefined && !SAHARA.includes(x[5]))) expect(L.spans[r[5]][0].to, r[2]).toBe(r[6].until);
    for (const [name, [from, to]] of Object.entries(GAINS)) {
      expect(g.byName[name].owner, name).toBe('UK');
      expect(L.spans[name], name).toEqual([{ from, to, kind: 'gain' }]);
      expect(L.back![name], name).toBe(to);
    }
    expect(Object.keys(L.spans).length).toBe(own.length + Object.keys(GAINS).length);
    expect(L.floor).toEqual({ until: 27, kinds: ['own', 'gain'] });
  });

  it('the ring is closed: every province next to ground the line gives Italy is on the line, and none of the ring is ever Italy\'s', () => {
    const g = startOpen(createGame(3, null));
    const L = line();
    const shut = new Set(L.shut);
    for (const name of Object.keys(L.spans)) {
      const r = g.byName[name];
      expect(r, name).toBeTruthy();
      for (const j of r.nb) { const nb = g.regions[j].name; expect(!!L.spans[nb] || shut.has(nb), `${name} next to ${nb}`).toBe(true); }
    }
    for (const name of shut) { expect(g.byName[name], name).toBeTruthy(); expect(L.spans[name], name).toBeUndefined(); expect(g.byName[name].owner, name).not.toBe('ITA'); }
    for (const name of ['Khartoum', 'Gedaref', 'Djibouti', 'Wajir', 'Garissa']) expect(shut.has(name), name).toBe(true);
    // (The Sahara posts and the desert are no part of it.)
    for (const name of [...SAHARA, 'Tripoli', 'Sidi Barrani', 'Mersa Matruh', 'Fort-Lamy']) expect(!!L.spans[name] || shut.has(name), name).toBe(false);
  });

  it('East Africa is a theatre of the book, closed if Italy or Britain falls before November 1941', () => {
    expect(HISTORY.theatres.eastafrica).toEqual({ name: 'East Africa', after: [] });
    const marks = HISTORY.marks.filter((m) => m.theatre === 'eastafrica');
    expect(marks.map((m) => [m.until, m.if])).toEqual([[27, ['alive:ITA']], [27, ['alive:UK']]]);
  });
});

describe('the book\'s line in East Africa: what it shuts, on a board set by hand', () => {
  it('Italy\'s staff: a gain opens in its month and shuts after its last; the ring is always shut; Germany is bound with it', () => {
    const at = (turn: number, name: string, nat = 'ITA'): boolean => { const g = board(1, turn); return lineShut(g, nat, g.byName[name]); };
    expect([10, 11, 17, 18].map((t) => at(t, 'Kassala'))).toEqual([true, false, false, true]);
    expect([10, 11, 18, 19].map((t) => at(t, 'Gallabat'))).toEqual([true, false, false, true]);
    expect([11, 12, 19, 20].map((t) => at(t, 'Berbera'))).toEqual([true, false, false, true]);
    expect([11, 12, 19, 20].map((t) => at(t, 'Hargeisa'))).toEqual([true, false, false, true]);
    for (const name of ['Khartoum', 'Gedaref', 'Tokar', 'Djibouti', 'Wajir', 'Garissa', 'Marsabit']) for (const t of [10, 12, 20, 27]) expect(at(t, name), `${name} at ${t}`).toBe(true);
    expect(at(12, 'Khartoum', 'GER')).toBe(true);
    // Its own ground is its own to its last month, and shut to it after.
    expect([19, 20, 21].map((t) => at(t, 'Asmara'))).toEqual([false, false, true]);
    // (Ground the line does not name is as it was: the desert, and the Sahara posts.)
    for (const name of ['Sidi Barrani', 'Mersa Matruh', 'Kufra', 'Fort-Lamy']) expect(at(12, name), name).toBe(false);
  });

  it('Britain\'s staff: a province Italy holds is shut to it until the province\'s month, a gain until its last', () => {
    const at = (turn: number, name: string, nat = 'UK'): boolean => { const g = board(1, turn); g.byName[name].ctrl = 'ITA'; return lineShut(g, nat, g.byName[name]); };
    expect([17, 18].map((t) => at(t, 'Mogadishu'))).toEqual([true, false]);
    expect([19, 20].map((t) => at(t, 'Asmara'))).toEqual([true, false]);
    expect([19, 20].map((t) => at(t, 'Addis Ababa'))).toEqual([true, false]);
    expect([26, 27].map((t) => at(t, 'Gondar'))).toEqual([true, false]);
    expect([16, 17].map((t) => at(t, 'Kassala'))).toEqual([true, false]);
    expect([18, 19].map((t) => at(t, 'Berbera'))).toEqual([true, false]);
    // (South Africa fights beside Britain and is bound with it.)
    expect(at(19, 'Addis Ababa', 'SAF')).toBe(true);
    // Ground Britain holds is never shut to it.
    const g = board(1, 12);
    expect(g.byName['Kassala'].ctrl).toBe('UK');
    expect(lineShut(g, 'UK', g.byName['Kassala'])).toBe(false);
  });

  it('a gain\'s corps go back inside the line the month after its last, and nobody is sent out of ground Italy still holds by the book', () => {
    const g = board(1, 17);
    expect(lineLeaves(g, 'ITA', g.byName['Kassala'], 17)).toBe(false);
    expect(lineLeaves(g, 'ITA', g.byName['Kassala'], 18)).toBe(true);
    expect(lineLeaves(g, 'ITA', g.byName['Gondar'], 18)).toBe(false);
    expect(lineLeaves(g, 'ITA', g.byName['Tripoli'], 18)).toBe(false);
  });

  it('it binds only at the stops that keep the book, while the book is open there, to month 27, and never with the player on either side', () => {
    const shutAt = (g: Game): boolean[] => [lineShut(g, 'ITA', g.byName['Khartoum']), lineShut(g, 'ITA', g.byName['Kassala'])];
    expect(shutAt(board(1, 10))).toEqual([true, true]);
    expect(shutAt(board(2, 10))[0]).toBe(true);
    for (const stop of [3, 4, 5]) expect(shutAt(board(stop, 10)), `stop ${stop}`).toEqual([false, false]);
    expect(shutAt(board(1, 28))).toEqual([false, false]);
    const closed = board(1, 10);
    closed.flags['hist:off:eastafrica'] = 9;
    expect(shutAt(closed)).toEqual([false, false]);
    for (const human of ['UK', 'ITA']) expect(shutAt(board(1, 10, human)), human).toEqual([false, false]);
  });
});

describe('the book\'s line in East Africa: one war at the book\'s first stop, to September 1940', () => {
  it('Italy holds nothing beyond its line, Khartoum and Port Sudan are Britain\'s, and Britain holds none of Italy\'s ground before its month', () => {
    const g = startOpen(createGame(3, null));
    setStop(g, 1, true);
    const L = line();
    const start = new Map(italys(g).map((r) => [r.id, r.ctrl]));
    while (g.turn < 14) {
      stepAI(g);
      for (const r of g.regions) {
        if (shoreOf(r) !== 2 || r.owner === 'ITA' || r.ctrl !== 'ITA') continue;
        // (Ground of another's that Italy holds on the far side of Africa is a gain of the line's, in its months.)
        if (L.shut!.includes(r.name) || GAINS[r.name]) { expect(GAINS[r.name], `${r.name} in month ${g.turn}`).toBeTruthy(); expect(g.turn, r.name).toBeGreaterThanOrEqual(GAINS[r.name][0]); }
      }
      for (const [id, was] of start) if (was === 'ITA') expect(g.regions[id].ctrl, `${g.regions[id].name} in month ${g.turn}`).toBe('ITA');
    }
    for (const name of ['Khartoum', 'Port Sudan', 'Gedaref', 'Wajir']) expect(g.byName[name].ctrl, name).toBe('UK');
  });
});
