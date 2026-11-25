// The Soviet Far East's divisions that go west (the design director's rulings of 19:45Z and 20:03Z, 22 November
// 2026; the historian's far-east-west-and-us-divisions-dec41.md, section (c), with the notes of 20:00Z and 20:03Z).
//
// The fault it mends: every row of the Soviet Far East was held and nothing read what Japan did. The record sent
// about thirteen divisions west in 1941, after Germany's attack and once Japan had turned south, and three more in
// July 1942.
//
// The rule: a wave comes due so many months after its cause (Germany's attack; the freeze; Japan's war with America
// or Britain), at the book's stops in the book's month, and is taken while Japan is at peace with the Soviet Union.
// It lifts the hold on so many divisions of a row and no more: a row that gives up part gives up a formation of that
// many, free of the hold, in the same place. Nothing is moved. A Soviet player is told, and his formations are left
// as they stand. And the 16th Army, which the old chart's own book raises at Kiev in May 1941, leaves the far
// chart's Chita row that month. Every state here is set by hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, atWar, createGame, declareWar } from '../src/campaign/game';
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
const months = (g: Game, from: number, to: number): void => { for (let t = from; t <= to; t++) month(g, t); };
/** Germany's attack on the Soviet Union in a month. */
const barbarossa = (g: Game, turn: number): void => { g.turn = turn; declareWar(g, 'GER', 'SOV'); };
/** Every division the Soviet Union has east of Lake Baikal's rows, held and free. */
const PLACES = ['Spassk', 'Vladivostok', 'Iman', 'Khabarovsk', 'Skovorodino', 'Blagoveshchensk', 'Svobodny', 'Birobidzhan', 'Chita', 'Dauria', 'Nerchinsk', 'Ulan-Ude'];
const inEast = (g: Game, free?: boolean): number =>
  g.corps.filter((c) => c.nat === 'SOV' && !c.dead && c.kind !== 'fort' && PLACES.includes(g.regions[c.reg].name) && (free === undefined || c.hold !== free)).reduce((n, c) => n + c.divs, 0);
const freed = (g: Game): [string, number, string][] => g.corps.filter((c) => c.nat === 'SOV' && !c.hold && PLACES.includes(g.regions[c.reg].name)).map((c) => [c.name, c.divs, g.regions[c.reg].name]);

describe('the far side: the Soviet Far East\'s divisions that go west', () => {
  it('the waves are the record\'s, in the book\'s months, and every row named is a held row with the divisions to give', () => {
    expect(W.waves.map((w) => [w.cause, w.after, w.book])).toEqual([['foe', 1, 23], ['freeze', 1, 25], ['freeze', 2, 26], ['freeze', 3, 27], ['war', 7, 35]]);
    // Thirteen divisions are named of the record's sixteen; the rest wait for the historian, with no stand-in.
    expect(W.waves.map((w) => w.rows.reduce((n, r) => n + r[1], 0))).toEqual([2, 2, 4, 2, 3]);
    expect(W.waves.map((w) => w.needs ?? '')).toEqual(['', 'foe', 'foe', 'foe', '']);
    const g = grand(1);
    const asked: Record<string, number> = {};
    for (const w of W.waves) for (const [name, n] of w.rows) asked[name] = (asked[name] ?? 0) + n;
    for (const [name, n] of Object.entries(asked)) {
      const c = named(g, name);
      expect(c?.hold, name).toBe(true);
      expect(c!.divs, name).toBeGreaterThanOrEqual(n);
    }
    // Spassk gives all four of its divisions, and Khabarovsk both of its two.
    expect([asked['1st Red Banner Army (Primorye)'], asked['2nd Red Banner Army (Khabarovsk)']]).toEqual([4, 2]);
  });

  it('by the book: two in July 1941, two, four and two from September to November, three in July 1942, and nothing is moved', () => {
    const g = grand(1);
    const all = inEast(g);
    expect(inEast(g, true)).toBe(0);
    months(g, 21, 21);
    const east = inEast(g);
    // (The Chita row's three have left for Kiev: the 16th Army, below.)
    expect(east).toBe(all - 3);
    barbarossa(g, 22);
    month(g, 22);
    expect(inEast(g, true)).toBe(0);
    month(g, 23);
    expect(freed(g)).toEqual([['69th Motorised Division', 1, 'Blagoveshchensk'], ['59th Tank Division', 1, 'Khabarovsk']]);
    expect([named(g, '2nd Red Banner Army (the Amur)')!.divs, named(g, '2nd Red Banner Army (the Amur)')!.hold]).toEqual([1, true]);
    month(g, 24);
    expect(inEast(g, true)).toBe(2);
    month(g, 25);
    expect(inEast(g, true)).toBe(4);
    expect([named(g, '21st and 26th Rifle Divisions')!.divs, named(g, '1st Red Banner Army (Primorye)')!.divs]).toEqual([2, 2]);
    month(g, 26);
    expect(inEast(g, true)).toBe(8);
    // A row that gives up all it has is free whole, under its own name.
    expect([named(g, '2nd Red Banner Army (Birobidzhan)')!.divs, named(g, '2nd Red Banner Army (Birobidzhan)')!.hold]).toEqual([1, false]);
    month(g, 27);
    expect(inEast(g, true)).toBe(10);
    expect([named(g, '1st Red Banner Army (Primorye)')!.divs, named(g, '1st Red Banner Army (Primorye)')!.hold]).toEqual([1, false]);
    // Japan's war with America in December 1941: the seventh month after is July 1942.
    g.turn = 28;
    declareWar(g, 'JAP', 'USA');
    months(g, 28, 34);
    expect(inEast(g, true)).toBe(10);
    month(g, 35);
    expect(inEast(g, true)).toBe(13);
    expect(named(g, '422nd Rifle Division')!.divs).toBe(1);
    // A freed formation is of its row's own quality, and stands where the row stands.
    const row = named(g, '1st Red Banner Army (the Ussuri, new divisions)')!, free = named(g, '422nd Rifle Division')!;
    expect([free.kind, free.xp, free.str, free.reg, free.hold]).toEqual([row.kind, row.xp, row.str, row.reg, false]);
    // Not a division is made or lost, and none has left its place.
    expect(inEast(g)).toBe(east);
    months(g, 36, 60);
    expect(inEast(g, true)).toBe(13);
  });

  it('where no book dates them the waves are counted from their causes, and a wave waits for Germany\'s war', () => {
    const g = grand(3);
    months(g, 21, 29);
    expect(inEast(g, true)).toBe(0);
    // The freeze alone frees nobody: Germany is not at war with the Soviet Union.
    g.flags['pac:freeze'] = 26;
    months(g, 27, 29);
    expect(inEast(g, true)).toBe(0);
    // Germany attacks in month 30: the freeze's waves, long due, are taken that month, and the first two the month after.
    barbarossa(g, 30);
    month(g, 30);
    expect(inEast(g, true)).toBe(8);
    month(g, 31);
    expect(inEast(g, true)).toBe(10);
    // Japan's war with Britain in month 40: three more in month 47, and not before.
    g.turn = 40;
    declareWar(g, 'JAP', 'UK');
    months(g, 40, 46);
    expect(inEast(g, true)).toBe(10);
    month(g, 47);
    expect(inEast(g, true)).toBe(13);
  });

  it('Japan at war with the Soviet Union frees nothing, and no wave to come is taken while that war lasts', () => {
    const g = grand(3);
    barbarossa(g, 30);
    month(g, 30);
    g.turn = 31;
    declareWar(g, 'JAP', 'SOV');
    expect(atWar(g, 'JAP', 'SOV')).toBe(true);
    g.flags['pac:freeze'] = 31;
    months(g, 31, 45);
    expect(inEast(g, true)).toBe(0);
    expect(g.corps.some((c) => c.name === '69th Motorised Division')).toBe(false);
  });

  it('a Soviet player is told what the Far East can spare, and his formations are left as they stand', () => {
    const g = grand(1, 'SOV');
    const shape = (): string => JSON.stringify(g.corps.filter((c) => c.nat === 'SOV').map((c) => [c.name, c.divs, c.hold, c.reg]));
    months(g, 21, 21);
    const was = shape();
    barbarossa(g, 22);
    months(g, 22, 23);
    expect(shape()).toBe(was);
    const told = g.headlines.filter((h) => h.text.startsWith('The Far East can spare')).map((h) => h.text);
    expect(told).toEqual(['The Far East can spare 2 divisions for the war in the west: the 69th Motorised Division at Blagoveshchensk and the 59th Tank Division at Khabarovsk. Nothing has been moved. They are yours to send.']);
    months(g, 24, 27);
    expect(shape()).toBe(was);
    expect(g.headlines.filter((h) => h.text.startsWith('The Far East can spare')).length).toBe(4);
    // A computer's Soviet Union says nothing of it.
    const c = grand(1);
    barbarossa(c, 22);
    months(c, 21, 27);
    expect(c.headlines.some((h) => h.text.startsWith('The Far East can spare'))).toBe(false);
  });

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
