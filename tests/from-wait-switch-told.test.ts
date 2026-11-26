// The switch and the telling's flags, the old chart (arrivalwaits.ts `setFromWait`, `rowDone`). Off no longer clears
// the telling's three flags (rowTold, rowAt, rowFront): they go where the row is sent or struck (`rowDone`), the design
// director's answer 3 of 00:43Z, 26 November 2026 ("Clear the telling's three flags where the row is sent or struck (no
// dead data)"). So a row told, the switch then turned off and on again inside one month, is not told a second time
// (section 1: one headline, once), and its standing line keeps the wait's first month (section 7 case 8: "keep the
// wait's month ... so it is true whatever came before"). While the switch is off nothing of the telling is shown, and a
// row that forms while it is off gets today's bare line, its flags gone. The refuter's switch-02 (its R5), cases-6.
// The far side's month card: tests/from-wait-switch-told-far.
//
// The board is tests/from-wait-r2's: September 1939, Grenzmark beside Poznan, Poland at war with Germany; Bavaria and
// Magdeburg quiet. Germany is the player's.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { arrivalWaitOf, setFromWait } from '../src/campaign/arrivalwaits';
import { addCorps, createGame, startPhase } from '../src/campaign/game';
import { markFor, monthCard } from '../src/campaign/monthcard';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

const WORDS = {
  begins: 'The Test Army is due in Magdeburg this month, formed from your Test Corps, which is at the front in the Grenzmark. The arrival waits, and forms the month after that corps leaves the front.',
  standing: 'An arrival due in Magdeburg, the Test Army, is to be formed from this corps, and has waited for it since October 1939. It forms the month after this corps leaves the front.',
  // (Today's bare line, game.ts `expeditions`: shipped words, the row's name with no article.)
  bare: 'Test Army has arrived in Magdeburg.',
};

const ROWS = (setup as any).expeditions as any[];
let added = 0;
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } });
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'GER'); };
const said = (g: Game, t?: number): string[] => g.headlines.filter((h) => (t === undefined || h.turn === t) && h.nat === 'GER' && /Test/.test(h.text)).map((h) => h.text);
const telFlags = (g: Game, i: number): Record<string, number> => Object.fromEntries(['rowTold', 'rowAt', 'rowFront'].filter((k) => g.flags[`${k}:${i}`] !== undefined).map((k) => [k, g.flags[`${k}:${i}`]]));

/** Germany's Test Corps in contact at Grenzmark, the switch on, and a row of month 2 taking it whole to form the Test Army at Magdeburg. */
function board(): { g: Game; i: number; c: Corps } {
  const g = startOpen(createGame(1, 'GER'));
  expect(setFromWait(g, true)).toBe(true);
  const c = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
  ROWS.push([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps'] }]); added++;
  return { g, i: ROWS.length - 1, c };
}

describe('off and on again inside one month, mid-wait', () => {
  it('the row is not told twice, and the standing line keeps the month the wait began (R5)', () => {
    const { g, i, c } = board();
    month(g, 2); month(g, 3);
    expect(arrivalWaitOf(g, c)?.whole).toBe(WORDS.standing);
    expect(setFromWait(g, false)).toBe(true);
    expect(setFromWait(g, true)).toBe(true);
    expect(telFlags(g, i)).toEqual({ rowTold: 2, rowAt: g.byName.Magdeburg.id, rowFront: 1 });
    month(g, 4);
    expect(g.flags[`wait:${i}`]).toBe(4);
    expect(said(g, 4)).toEqual([]);
    // (One headline, once, in the whole war.)
    expect(said(g).filter((t) => /is due in Magdeburg this month/.test(t))).toEqual([WORDS.begins]);
    expect(arrivalWaitOf(g, c)?.whole).toBe(WORDS.standing);
    expect(g.flags[`rowTold:${i}`]).toBe(2);
  });
});

describe('off mid-wait', () => {
  it('nothing of the telling shows from the moment it is off; the flags stay in the save until the row forms, next month, with today\'s bare line', () => {
    const { g, i, c } = board();
    month(g, 2); month(g, 3);
    expect(arrivalWaitOf(g, c)?.whole).toBe(WORDS.standing);
    expect(setFromWait(g, false)).toBe(true);
    // (Off: no card, no line, at once.)
    expect(arrivalWaitOf(g, c)).toBeNull();
    expect(monthCard(g, markFor(g), 'GER').waits).toBeUndefined();
    // (What already happened stays in the save: the design director's answer 2.)
    const h = loadGame(saveGame(g))!;
    expect(telFlags(h, i)).toEqual({ rowTold: 2, rowAt: g.byName.Magdeburg.id, rowFront: 1 });
    expect(h.flags[`wait:${i}`]).toBe(3);
    month(h, 4);
    expect(h.flags[`sent:${i}`]).toBe(1);
    expect(c.dead).toBeFalsy();
    expect(h.corps.find((x) => x.name === 'Test Corps' && !x.dead)).toBeUndefined();
    expect(said(h, 4)).toEqual([WORDS.bare]);
    expect(telFlags(h, i)).toEqual({});
  });

  it('off for good before the row has waited: nothing is written, nothing told', () => {
    const { g, i } = board();
    expect(setFromWait(g, false)).toBe(true);
    month(g, 2);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(said(g, 2)).toEqual([WORDS.bare]);
    expect(saveGame(g)).not.toMatch(/fromWait|rowTold|rowAt|rowFront/);
  });
});
