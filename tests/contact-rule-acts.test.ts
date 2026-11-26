// Where the contact rule acts on a dated row, the far side off (history.ts `rowBook`, `contactRuleActs`). The design
// director's ruling of 22:05Z, 25 November 2026: "One sentence, one scope: the contact rule acts where the stop is 3 to
// 5 OR the row's theatre's book is closed." With the far side off every ground is the old chart's or the Americas',
// and reads the west's book (her 22:12Z, reading A). The far side on: tests/far-contact-rule-acts.test.ts. No war is
// played.
import { describe, expect, it } from 'vitest';
import { bookMonth } from '../src/campaign/claims';
import { createGame, declareWar } from '../src/campaign/game';
import { bookClosed, contactRuleActs, rowBook, setStop, stopDef } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import { farOn, shoreOf } from '../src/campaign/world';
import type { Game, Region } from '../src/campaign/types';

const campaign = (stop: number): Game => { const g = startOpen(createGame(1, null)); expect(setStop(g, stop, true)).toBe(true); return g; };
const places = (g: Game): Region[] => [g.byName['Magdeburg'], g.regions.find((r) => shoreOf(r) === 1)!];

describe('the far side off', () => {
  it('every province reads the west\'s book; a place not known reads none', () => {
    const g = campaign(1);
    expect(farOn(g)).toBe(false);
    expect(places(g).map((r) => shoreOf(r))).toEqual([0, 1]);
    expect(g.regions.every((r) => rowBook(g, r) === 'west')).toBe(true);
    expect(rowBook(g, undefined)).toBeNull();
  });

  it.each([3, 4, 5])('at stop %i it acts everywhere, the place known or not', (stop) => {
    const g = campaign(stop);
    expect(stopDef(g).book).toBeFalsy();
    for (const r of [...places(g), undefined]) expect(contactRuleActs(g, r)).toBe(true);
  });

  it.each([1, 2])('at stop %i it acts nowhere with the west\'s book open, even with the Pacific\'s marked closed', (stop) => {
    const g = campaign(stop);
    g.flags['hist:off:pacific'] = 30;
    for (const r of [...places(g), undefined]) expect(contactRuleActs(g, r)).toBe(false);
  });

  it.each([1, 2])('at stop %i with the west\'s book closed it acts on every known place', (stop) => {
    const g = campaign(stop);
    g.flags['hist:off:west'] = 9;
    for (const r of places(g)) expect(contactRuleActs(g, r)).toBe(true);
    expect(contactRuleActs(g, undefined)).toBe(false);
  });

  it('closed by the book itself (claims.ts `bookMonth`): Germany at war with Belgium in September 1939 closes the west\'s, and it acts', () => {
    const g = campaign(1);
    expect(contactRuleActs(g, g.byName['Magdeburg'])).toBe(false);
    g.turn = 2;
    declareWar(g, 'GER', 'BEL');
    bookMonth(g);
    expect(bookClosed(g, 'west')).toBe(2);
    expect(contactRuleActs(g, g.byName['Magdeburg'])).toBe(true);
  });
});
