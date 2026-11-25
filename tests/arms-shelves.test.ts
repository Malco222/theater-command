// A nation's stock of arms (src/campaign/arms.ts, data/campaign/arms.json): one module, three shelves, every amount in
// divisions' worth. The module alone: rows are laid here through rowsFrom, seedFrom and ARMS.nations, and put back
// after each test, so this file plays the same on a line where no other rule registers anything.
//
//  1. The contract, function by function: a shelf with a figure and without, never short, nothing written for no
//     figure, take leaves 0, put has no bound, and the guards on amounts.
//  2. The month's gain: peace, war, the turn-over month by month, the share of home industry held (works not scaled),
//     works held and lost, a place the chart lacks, and armsMonth twice in a month.
//  3. Where a row comes from: a registered source over the table over the depot seed; a source that answers only in
//     some campaigns; no seed, no shelf.
//  4. Save and load keep the shelves. The module stands alone: it imports nothing of the game's rules.
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import armsText from '../src/campaign/arms.ts?raw';
import {
  ARMS, SHELVES, armsMonth, armsOf, armsView, countOn, draw, firstOf, gainOf, giveBack, has, peaceFooting, put, rowsFrom, seedFrom, stockOf, take, warFooting,
  type ArmsRows,
} from '../src/campaign/arms';
import { createGame } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const undo: (() => void)[] = [];
const tableBefore = JSON.stringify(ARMS.nations);
// Every test starts with no depot seed, whatever another module registered as it loaded, and with the table as shipped.
beforeEach(() => { undo.push(seedFrom(null)); });
afterEach(() => {
  while (undo.length) undo.pop()!();
  for (const k of Object.keys(ARMS.nations)) delete ARMS.nations[k];
  Object.assign(ARMS.nations, JSON.parse(tableBefore));
});
const lay = (nat: string, rows: ArmsRows): void => { ARMS.nations[nat] = rows; };
const game = (): Game => startOpen(createGame(11, null));
const flagsOf = (g: Game): string => JSON.stringify(Object.keys(g.flags).sort().map((k) => [k, g.flags[k]]));
const ours = (g: Game): string[] => Object.keys(g.flags).filter((k) => /^sa[a-z]?:/.test(k)).sort();
/** Two nations of the chart, one given a row in a test and one left without. */
const A = 'GER', B = 'ITA';

describe('the data file', () => {
  it('ships with its own words and no nation rows', () => {
    expect(ARMS.nations).toEqual({});
    expect(typeof (ARMS as unknown as { _about: string })._about).toBe('string');
  });
});

describe('the contract: a shelf with a figure and without', () => {
  it('a nation with a figure keeps it; one with none keeps nothing, counts 0 and may count on any amount', () => {
    const g = game();
    lay(A, { rifles: { start: 10 } });
    expect(has(g, A, 'rifles')).toBe(true);
    expect(stockOf(g, A, 'rifles')).toBe(10);
    expect(firstOf(g, A, 'rifles')).toBe(10);
    expect(countOn(g, A, 'rifles')).toBe(10);
    for (const s of ['cadres', 'guns'] as const) {
      expect(has(g, A, s)).toBe(false);
      expect(stockOf(g, A, s)).toBe(0);
      expect(countOn(g, A, s)).toBe(Infinity);
    }
    for (const s of SHELVES) {
      expect(has(g, B, s)).toBe(false);
      expect(stockOf(g, B, s)).toBe(0);
      expect(countOn(g, B, s)).toBe(Infinity);
    }
    // Reading writes nothing.
    expect(ours(g)).toEqual([]);
  });

  it('a draw gives what there is and says how much; a nation with no figure is never short and nothing is written', () => {
    const g = game();
    lay(A, { rifles: { start: 10 } });
    expect(draw(g, A, 'rifles', 4)).toBe(4);
    expect(stockOf(g, A, 'rifles')).toBe(6);
    expect(countOn(g, A, 'rifles')).toBe(6);
    expect(draw(g, A, 'rifles', 100)).toBe(6);
    expect(stockOf(g, A, 'rifles')).toBe(0);
    const empty = flagsOf(g);
    expect(draw(g, A, 'rifles', 3)).toBe(0);
    expect(flagsOf(g)).toBe(empty);
    // No figure: the whole amount, on every shelf, and not a flag written.
    const before = flagsOf(g);
    expect(draw(g, B, 'rifles', 12.5)).toBe(12.5);
    expect(draw(g, B, 'guns', 3)).toBe(3);
    expect(draw(g, A, 'cadres', 7)).toBe(7);
    expect(flagsOf(g)).toBe(before);
    expect(stockOf(g, B, 'rifles')).toBe(0);
  });

  it('a draw back and a put have no bound; a put on a shelf the row does not carry adds nothing and writes nothing', () => {
    const g = game();
    lay(A, { rifles: { start: 10 }, guns: { start: 1 } });
    expect(draw(g, A, 'rifles', 4)).toBe(4);
    expect(giveBack(g, A, 'rifles', 4)).toBe(4);
    expect(stockOf(g, A, 'rifles')).toBe(10);
    expect(giveBack(g, A, 'rifles', 5)).toBe(5);
    expect(stockOf(g, A, 'rifles')).toBe(15);
    expect(put(g, A, 'guns', 1000)).toBe(1000);
    expect(stockOf(g, A, 'guns')).toBe(1001);
    const before = flagsOf(g);
    expect(put(g, B, 'rifles', 5)).toBe(0);
    expect(put(g, A, 'cadres', 5)).toBe(0);
    expect(giveBack(g, B, 'guns', 5)).toBe(0);
    expect(flagsOf(g)).toBe(before);
    expect(has(g, B, 'rifles')).toBe(false);
  });

  it('take empties a shelf and returns what stood on it: the shelf stands at 0 after, not at its first day', () => {
    const g = game();
    lay(A, { cadres: { start: 8 } });
    expect(take(g, A, 'cadres')).toBe(8);
    expect(stockOf(g, A, 'cadres')).toBe(0);
    expect(g.flags['sac:GER']).toBe(0);
    expect(take(g, A, 'cadres')).toBe(0);
    expect(put(g, A, 'cadres', 2)).toBe(2);
    expect(take(g, A, 'cadres')).toBe(2);
    // A shelf the nation has no figure for: nothing to take and nothing written.
    const before = flagsOf(g);
    expect(take(g, B, 'cadres')).toBe(0);
    expect(take(g, A, 'rifles')).toBe(0);
    expect(flagsOf(g)).toBe(before);
  });

  it('an amount that is negative, not a number or not finite counts as nothing', () => {
    const g = game();
    lay(A, { rifles: { start: 10 } });
    const before = flagsOf(g);
    for (const x of [-1, -Infinity, NaN, Infinity, 0]) {
      expect(draw(g, A, 'rifles', x)).toBe(0);
      expect(put(g, A, 'rifles', x)).toBe(0);
      expect(giveBack(g, A, 'rifles', x)).toBe(0);
      expect(draw(g, B, 'rifles', x)).toBe(0);
    }
    expect(flagsOf(g)).toBe(before);
    expect(stockOf(g, A, 'rifles')).toBe(10);
  });

  it('a flag is written only when a shelf moves, under its own name', () => {
    const g = game();
    lay(A, { rifles: { start: 10 }, cadres: { start: 5 }, guns: { start: 2 } });
    expect(ours(g)).toEqual([]);
    draw(g, A, 'rifles', 1); draw(g, A, 'cadres', 1); draw(g, A, 'guns', 1);
    expect(ours(g)).toEqual(['sa:GER', 'sac:GER', 'sag:GER']);
  });

  it('armsOf: the rifles and guns a living corps carries at its strength, none for a dead one', () => {
    const g = game();
    const c = g.corps.find((k) => k.nat === A && !k.dead && k.divs > 1)!;
    c.str = 0.75;
    expect(armsOf(g, c)).toEqual({ rifles: c.divs * 0.75, guns: c.divs * 0.75 });
    c.dead = true;
    expect(armsOf(g, c)).toEqual({ rifles: 0, guns: 0 });
  });
});

describe("the month's gain", () => {
  it('peace: the figure, scaled by the share of home industry held; put once a month, a second call adding nothing', () => {
    const g = game();
    lay(A, { guns: { start: 1, peace: 2 } });
    expect(gainOf(g, A, 'guns')).toBe(2);
    expect(gainOf(g, A, 'guns', 0.5)).toBe(1);
    expect(gainOf(g, A, 'guns', 0)).toBe(0);
    armsMonth(g, A, 0.5);
    expect(stockOf(g, A, 'guns')).toBe(2);
    armsMonth(g, A, 0.5);
    expect(stockOf(g, A, 'guns')).toBe(2);
    g.turn++;
    armsMonth(g, A);
    expect(stockOf(g, A, 'guns')).toBe(4);
  });

  it('war: nothing of it before the footing is noted, then the turn-over month by month, whole after; struck, the peace figure again', () => {
    const g = game();
    lay(A, { rifles: { start: 0, peace: 1, war: 5, turn: 4 } });
    expect(gainOf(g, A, 'rifles')).toBe(1);
    const t0 = g.turn;
    warFooting(g, A);
    expect(g.flags['saw:GER']).toBe(t0);
    const seen: number[] = [];
    for (let m = 0; m <= 6; m++) { g.turn = t0 + m; seen.push(gainOf(g, A, 'rifles')); }
    expect(seen).toEqual([1, 2, 3, 4, 5, 5, 5]);
    // Noted once: a second footing later does not start the turn-over again.
    g.turn = t0 + 2;
    warFooting(g, A);
    expect(g.flags['saw:GER']).toBe(t0);
    expect(gainOf(g, A, 'rifles', 0.5)).toBe(1.5);
    peaceFooting(g, A);
    expect(g.flags['saw:GER']).toBeUndefined();
    expect(gainOf(g, A, 'rifles')).toBe(1);
  });

  it("with no 'turn' the war figure comes at once; a missing 'war' reads as 'peace'; no war figure anywhere, no footing written", () => {
    const g = game();
    lay(A, { rifles: { peace: 1, war: 3 }, guns: { peace: 2 } });
    warFooting(g, A);
    expect(gainOf(g, A, 'rifles')).toBe(3);
    expect(gainOf(g, A, 'guns')).toBe(2);
    lay(B, { rifles: { peace: 1 }, cadres: { start: 4 } });
    const before = flagsOf(g);
    warFooting(g, B);
    expect(flagsOf(g)).toBe(before);
    expect(gainOf(g, B, 'rifles')).toBe(1);
  });

  it('works: a place held gives its gain, unscaled by industry held; lost, nothing; a place the chart lacks, nothing', () => {
    const g = game();
    const place = g.regions[g.nations[A].capital];
    lay(A, { rifles: { start: 0, peace: 2, works: [{ place: place.name, gain: 3 }, { place: 'A Place No Chart Has', gain: 7 }] } });
    expect(gainOf(g, A, 'rifles')).toBe(5);
    expect(gainOf(g, A, 'rifles', 0.5)).toBe(4);
    expect(armsView(g, A).rifles.works).toEqual([{ place: place.name, gain: 3, held: true }, { place: 'A Place No Chart Has', gain: 7, held: false }]);
    place.ctrl = B;
    expect(gainOf(g, A, 'rifles', 0.5)).toBe(1);
    expect(armsView(g, A).rifles.works[0].held).toBe(false);
  });

  it('armsMonth writes nothing where nothing is gained, and nothing for a nation that has fallen', () => {
    const g = game();
    lay(A, { rifles: { start: 3 } });
    const before = flagsOf(g);
    armsMonth(g, A);
    armsMonth(g, B);
    expect(flagsOf(g)).toBe(before);
    lay(B, { guns: { peace: 1 } });
    g.nations[B].alive = false;
    armsMonth(g, B);
    expect(flagsOf(g)).toBe(before);
  });
});

describe('where a row comes from', () => {
  it('a registered source over the table over the depot seed, shelf by shelf; the seed for rifles alone', () => {
    const g = game();
    undo.push(seedFrom((nat) => (nat === A ? 7 : undefined)));
    expect(stockOf(g, A, 'rifles')).toBe(7);
    expect(has(g, A, 'cadres')).toBe(false);
    expect(has(g, B, 'rifles')).toBe(false);
    lay(A, { rifles: { start: 8 } });
    expect(stockOf(g, A, 'rifles')).toBe(8);
    const off = rowsFrom((_g, nat) => (nat === A ? { rifles: { start: 9 } } : undefined));
    undo.push(off);
    expect(stockOf(g, A, 'rifles')).toBe(9);
    // The latest registered is asked first; a source with no row for a shelf leaves it to the next.
    const off2 = rowsFrom((_g, nat) => (nat === A ? { guns: { start: 2 } } : undefined));
    undo.push(off2);
    expect(stockOf(g, A, 'rifles')).toBe(9);
    expect(stockOf(g, A, 'guns')).toBe(2);
    const off3 = rowsFrom((_g, nat) => (nat === A ? { rifles: { start: 11 } } : undefined));
    undo.push(off3);
    expect(stockOf(g, A, 'rifles')).toBe(11);
    off3(); off2();
    expect(has(g, A, 'guns')).toBe(false);
    off();
    expect(stockOf(g, A, 'rifles')).toBe(8);
    delete ARMS.nations[A];
    expect(stockOf(g, A, 'rifles')).toBe(7);
  });

  it('a registered source that answers only in the campaigns its own switch is on in', () => {
    undo.push(rowsFrom((g, nat) => (g.flags.armsTestRows === 1 && nat === A ? { cadres: { start: 6, peace: 1 } } : undefined)));
    const on = game(), off = game();
    on.flags.armsTestRows = 1;
    expect(stockOf(on, A, 'cadres')).toBe(6);
    expect(gainOf(on, A, 'cadres')).toBe(1);
    expect(has(off, A, 'cadres')).toBe(false);
    expect(draw(off, A, 'cadres', 5)).toBe(5);
    expect(ours(off)).toEqual([]);
    expect(draw(on, A, 'cadres', 5)).toBe(5);
    expect(stockOf(on, A, 'cadres')).toBe(1);
  });

  it('with no depot seed registered, a nation with no row has no shelf', () => {
    const g = game();
    for (const nat of Object.keys(g.nations)) for (const s of SHELVES) expect(has(g, nat, s)).toBe(false);
    // A seed that gives a figure of 0 is a shelf of 0, and a nation it gives none for still has none.
    undo.push(seedFrom((nat) => (nat === A ? 0 : undefined)));
    expect(has(g, A, 'rifles')).toBe(true);
    expect(stockOf(g, A, 'rifles')).toBe(0);
    expect(draw(g, A, 'rifles', 1)).toBe(0);
    expect(has(g, B, 'rifles')).toBe(false);
    // Taken away again, it is as it was.
    undo.pop()!();
    expect(has(g, A, 'rifles')).toBe(false);
  });
});

describe('the view, save and load, and standing alone', () => {
  it('the view gives, per shelf, whether it is carried, its stock, its first day, its gain and its works', () => {
    const g = game();
    lay(A, { rifles: { start: 10, peace: 1 } });
    draw(g, A, 'rifles', 3);
    const v = armsView(g, A, 0.5);
    expect(v.rifles).toEqual({ has: true, stock: 7, first: 10, gain: 0.5, works: [] });
    expect(v.guns).toEqual({ has: false, stock: 0, first: 0, gain: 0, works: [] });
  });

  it('save and load keep the shelves and the footing', () => {
    const g = game();
    lay(A, { rifles: { start: 10, peace: 1, war: 3, turn: 2 }, cadres: { start: 5 }, guns: { start: 2 } });
    draw(g, A, 'rifles', 2.25);
    take(g, A, 'cadres');
    put(g, A, 'guns', 1.5);
    warFooting(g, A);
    armsMonth(g, A);
    const back = loadGame(saveGame(g))!;
    expect(back).not.toBeNull();
    for (const s of SHELVES) expect(stockOf(back, A, s)).toBe(stockOf(g, A, s));
    expect([stockOf(back, A, 'rifles'), stockOf(back, A, 'cadres'), stockOf(back, A, 'guns')]).toEqual([8.75, 0, 3.5]);
    back.turn = g.turn + 1;
    expect(gainOf(back, A, 'rifles')).toBe(2);
    armsMonth(back, A);
    expect(stockOf(back, A, 'rifles')).toBe(10.75);
    expect(ours(back)).toEqual(ours(g));
  });

  it('the module imports nothing of the game at run time: types only, its own data file and the far rows', () => {
    const lines = (armsText as unknown as string).split('\n').filter((l) => /^\s*import\b/.test(l));
    expect(lines.length).toBeGreaterThan(0);
    for (const l of lines) expect(l).toMatch(/^import type |^import '\.\/far-rows';|^import data from '\.\.\/\.\.\/data\/campaign\/arms\.json';$/);
    // No rule reads a switch here.
    expect(armsText as unknown as string).not.toMatch(/from '\.\/stage'|_STAGE/);
  });
});
