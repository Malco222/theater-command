// Where the contact rule acts on a dated row, the far side on (history.ts `rowBook`, `contactRuleActs`). The design
// director's ruling of 22:05Z, 25 November 2026: "A closed book brings the contact rule in. [...] One sentence, one
// scope: the contact rule acts where the stop is 3 to 5 OR the row's theatre's book is closed." And her 22:12Z, reading
// A: "two books by ground, behind your one function. Far-side ground reads the Pacific's book, as the stations and
// posted rows read it today, so a row and the staffs beside it read one book. The old chart reads the west's, on which
// every later book there hangs." The far side off: tests/contact-rule-acts.test.ts. No war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { bookMonth } from '../src/campaign/claims';
import { createGame, declareWar } from '../src/campaign/game';
import { bookClosed, contactRuleActs, rowBook, setStop, stopDef } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import { stations } from '../src/campaign/stations';
import { farOn, shoreOf } from '../src/campaign/world';
import type { Game, Region } from '../src/campaign/types';

const campaign = (stop: number): Game => { const g = startOpen(createGame(3, null)); expect(setStop(g, stop, true)).toBe(true); return g; };
/** A place of each kind, read by world.ts's own `shoreOf`. */
const places = (g: Game): { far: Region; old: Region; americas: Region } => ({
  far: g.byName['Sapporo'],
  old: g.byName['Magdeburg'],
  americas: g.regions.find((r) => shoreOf(r) === 1)!,
});

describe('the ground and its book (reading A)', () => {
  it('far-side ground reads the Pacific\'s book; the old chart and the Americas read the west\'s; a place not known reads none', () => {
    const g = campaign(1);
    expect(farOn(g)).toBe(true);
    const p = places(g);
    expect([shoreOf(p.far), shoreOf(p.old), shoreOf(p.americas)]).toEqual([2, 0, 1]);
    expect(rowBook(g, p.far)).toBe('pacific');
    expect(rowBook(g, p.old)).toBe('west');
    expect(rowBook(g, p.americas)).toBe('west');
    expect(rowBook(g, undefined)).toBeNull();
    // Every province of the far side reads the Pacific's, and no other province does: the same line as world.ts.
    expect(g.regions.every((r) => (rowBook(g, r) === 'pacific') === (shoreOf(r) === 2))).toBe(true);
  });
});

describe('the scope: stops 3 to 5, or the row\'s book closed', () => {
  it.each([3, 4, 5])('at stop %i it acts everywhere, the place known or not, every book open', (stop) => {
    const g = campaign(stop);
    expect(stopDef(g).book).toBeFalsy();
    const p = places(g);
    for (const r of [p.far, p.old, p.americas, undefined]) expect(contactRuleActs(g, r)).toBe(true);
  });

  it.each([1, 2])('at stop %i with every book open it acts on no known place', (stop) => {
    const g = campaign(stop);
    expect(stopDef(g).book).toBe(true);
    const p = places(g);
    for (const r of [p.far, p.old, p.americas]) expect(contactRuleActs(g, r)).toBe(false);
  });

  it.each([1, 2])('at stop %i with the Pacific\'s book closed it acts on far-side ground only', (stop) => {
    const g = campaign(stop);
    g.flags['hist:off:pacific'] = 30;
    const p = places(g);
    expect(contactRuleActs(g, p.far)).toBe(true);
    expect(contactRuleActs(g, p.old)).toBe(false);
    expect(contactRuleActs(g, p.americas)).toBe(false);
  });

  it.each([1, 2])('at stop %i with the west\'s book alone closed by hand it acts on the old chart and the Americas, not on far-side ground', (stop) => {
    const g = campaign(stop);
    g.flags['hist:off:west'] = 9;
    const p = places(g);
    expect(contactRuleActs(g, p.far)).toBe(false);
    expect(contactRuleActs(g, p.old)).toBe(true);
    expect(contactRuleActs(g, p.americas)).toBe(true);
  });

  it('the west\'s book closed by the book itself closes the Pacific\'s with it (claims.ts `closeBook`): it then acts on every known place', () => {
    const g = campaign(1);
    const p = places(g);
    expect(contactRuleActs(g, p.far)).toBe(false);
    // September 1939: the west's first mark asks that Germany be at peace with Belgium until the spring of 1940.
    g.turn = 2;
    declareWar(g, 'GER', 'BEL');
    bookMonth(g);
    expect(bookClosed(g, 'west')).toBe(2);
    expect(bookClosed(g, 'pacific')).toBe(2);
    expect(contactRuleActs(g, p.far)).toBe(true);
    expect(contactRuleActs(g, p.old)).toBe(true);
    expect(contactRuleActs(g, p.americas)).toBe(true);
  });
});

// The design director's answer 4 of 00:43Z, 26 November 2026: "A row that reads no book keeps no book: the contact rule
// acts (forces, never a book that is not there). That is one line, plus a test that the far stage always lays the table."
describe('a row that reads no book keeps no book', () => {
  it('the far stage lays the stations table, and it names the Pacific\'s theatre: far-side ground always reads a book', () => {
    const g = campaign(1);
    expect(farOn(g)).toBe(true);
    expect(stations()).not.toBeNull();
    expect(stations()!.theatre).toBe('pacific');
    expect(rowBook(g, places(g).far)).toBe('pacific');
  });

  it.each([1, 2])('at stop %i a place not known reads no book: the rule acts there, every book open or any closed', (stop) => {
    const g = campaign(stop);
    expect(rowBook(g, undefined)).toBeNull();
    expect(contactRuleActs(g, undefined)).toBe(true);
    g.flags['hist:off:pacific'] = 30;
    expect(contactRuleActs(g, undefined)).toBe(true);
  });
});
