// A row limited by `stops`, told, when the dial goes to a stop it does not act at (arrivalwaits.ts `waiting`). Such a
// row returns at its stops check before it can wait or form, so while the dial stands there it never forms: the corps'
// standing line ("It forms the month after this corps leaves the front") and the month card's line would be false for
// as long as it stands there. No card speaks for it; its three flags are KEPT, so that when the dial comes back to a
// stop the row acts at, the card returns with the wait's first month and the row is not told a second time. The
// refuter's interplay-3 (its R7). Real rows of the shape: on the far side, Japan's rows with stops [3, 4, 5] (the 15th
// Army (33rd and 55th Divisions), row 379) and the many with stops [1, 2].
//
// The book is closed where the row is due (the west's), so the contact rule acts at stops 1 and 2 too (history.ts
// `contactRuleActs`) and only the row's own `stops` keep it from acting. The board is tests/from-wait-r2's: Germany the
// player's, the Test Corps in contact at Grenzmark, the row for Magdeburg.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { arrivalWaitOf } from '../src/campaign/arrivalwaits';
import { corpsInContact } from '../src/campaign/drawn';
import { addCorps, createGame, startPhase } from '../src/campaign/game';
import { contactRuleActs, setStop, stopOf } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const WORDS = {
  begins: 'The Test Army is due in Magdeburg this month, formed from your Test Corps, which is at the front in the Grenzmark. The arrival waits, and forms the month after that corps leaves the front.',
  standing: 'An arrival due in Magdeburg, the Test Army, is to be formed from this corps, and has waited for it since October 1939. It forms the month after this corps leaves the front.',
};
const ROWS = (setup as any).expeditions as any[];
let added = 0;
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } });
/**
 * (The contact rule's second form, the design director's ruling of 06:10Z, 26 November 2026: a corps beside the enemy
 * is held while it took part in the battle last resolved on its ground, or where nobody of its side keeps its province.
 * This board's corps at the front stand with others of their side, so each is set, by hand, as having fought the battle
 * last resolved where it stands, and is held as the board means. The relieved case: tests/contact-fact.test.ts.)
 */
const fought = (g: Game): void => { if (g.flags.fromWait !== 1) return; g.lastBattle = {}; for (const c of g.corps) if (!c.dead) (g.lastBattle[c.reg] ??= []).push(c.id); };
const month = (g: Game, t: number): void => { fought(g); g.turn = t; startPhase(g, 'GER'); };
const said = (g: Game): string[] => g.headlines.filter((h) => h.nat === 'GER' && /Test/.test(h.text)).map((h) => h.text);
const tel = (g: Game, i: number): number[] => ['rowTold', 'rowAt', 'rowFront'].map((k) => g.flags[`${k}:${i}`]);

function board(stop: number, stops: number[]): { g: Game; i: number; c: ReturnType<typeof addCorps> } {
  const g = startOpen(createGame(1, 'GER'));
  expect(setStop(g, stop, true)).toBe(true);
  g.flags['hist:off:west'] = 1;
  g.flags.fromWait = 1;
  const c = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
  ROWS.push([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps'], stops }]); added++;
  return { g, i: ROWS.length - 1, c };
}

describe('a row with stops [3, 4, 5], told at stop 3, the dial turned to stop 1 with the west\'s book closed', () => {
  it('no card line at stop 1 in months 3 to 5; back at stop 3 the card returns with its first month, and no second headline', () => {
    const { g, i, c } = board(3, [3, 4, 5]);
    month(g, 2);
    expect(said(g)).toEqual([WORDS.begins]);
    expect(arrivalWaitOf(g, c)?.whole).toBe(WORDS.standing);
    expect(setStop(g, 1)).toBe(true);
    expect(stopOf(g)).toBe(1);
    expect(contactRuleActs(g, g.byName.Magdeburg)).toBe(true);
    for (const t of [3, 4, 5]) {
      month(g, t);
      expect(corpsInContact(g, 'GER', c)).toBe(true);
      expect(arrivalWaitOf(g, c)).toBeNull();
      expect(g.flags[`sent:${i}`]).toBeUndefined();
      // (Kept: the dial can come back.)
      expect(tel(g, i)).toEqual([2, g.byName.Magdeburg.id, 1]);
    }
    expect(setStop(g, 3)).toBe(true);
    expect(arrivalWaitOf(g, c)?.whole).toBe(WORDS.standing);
    month(g, 6);
    expect(g.flags[`wait:${i}`]).toBe(6);
    expect(said(g)).toEqual([WORDS.begins]);
    expect(arrivalWaitOf(g, c)?.whole).toBe(WORDS.standing);
  });
});

describe('a row with stops [1, 2], told at stop 1 with the west\'s book closed, the dial turned to stop 3', () => {
  it('no card line at stop 3; back at stop 1 the card returns, and no second headline', () => {
    const { g, i, c } = board(1, [1, 2]);
    month(g, 2);
    expect(said(g)).toEqual([WORDS.begins]);
    expect(arrivalWaitOf(g, c)?.whole).toBe(WORDS.standing);
    expect(setStop(g, 3)).toBe(true);
    expect(arrivalWaitOf(g, c)).toBeNull();
    month(g, 3);
    expect(arrivalWaitOf(g, c)).toBeNull();
    expect(tel(g, i)).toEqual([2, g.byName.Magdeburg.id, 1]);
    expect(setStop(g, 1)).toBe(true);
    month(g, 4);
    expect(said(g)).toEqual([WORDS.begins]);
    expect(arrivalWaitOf(g, c)?.whole).toBe(WORDS.standing);
  });
});
