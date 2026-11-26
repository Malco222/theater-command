// The closed book and the telling together, the old chart (arrivalwaits.ts; history.ts `contactRuleActs`). At the
// dial's stops 1 and 2 the contact rule acts where the row's book is closed (the design director's 22:05Z of 25
// November 2026, "a closed book brings the contact rule in"; the old chart's ground reads the west's book, her
// 22:12Z), and the telling reads the same there as at stops 3 to 5. With the west's book closed the player's row waits
// AND is told: the headline the month the wait begins (section 1) and the standing line on the corps (section 2). With
// it open the row takes at its date, nothing is told and none of the telling's flags is written.
//
// And the order of the month (the design director's answer 1 of 00:43Z, 26 November 2026): "A book closing at the
// opening of month T binds a row due in T that month: the rule reads the world as it stands when the row acts. Pin the
// order with a test." The month opens with the book's marks (turn.ts: the turn moves on, then `historyMonth` closes a
// book whose mark has failed), and only then does each country's month begin (`startPhase`, `expeditions`). One case
// is driven through the real month: Germany at war with Belgium in September 1939 fails the west's first mark, the
// west's book closes as October opens, and a row of Germany's due in October waits that month.
//
// The board is tests/from-wait-r2's: September 1939, Grenzmark beside Poznan, Poland at war with Germany; Bavaria
// quiet. Germany is the player's. The rows are added at the foot of the data's list and taken off after each test.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { arrivalWaitOf } from '../src/campaign/arrivalwaits';
import { addCorps, createGame, declareWar, startPhase } from '../src/campaign/game';
import { bookClosed, setStop, stopDef } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import { humanUp, passMonth } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';

/** The UI director's sentences these tests pin, in one place (her section 7 may change them). */
const WORDS = {
  begins: 'The Test Army is due in Magdeburg this month, formed from your Test Corps, which is at the front in the Grenzmark. The arrival waits, and forms the month after that corps leaves the front.',
  standing: 'An arrival due in Magdeburg since October 1939, the Test Army, is to be formed from this corps. It forms the month after this corps leaves the front.',
};

const ROWS = (setup as any).expeditions as any[];
let added = 0;
function addRow(row: any[]): number { ROWS.push(row); added++; return ROWS.length - 1; }
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } });

const month = (g: Game, turn: number): void => { g.turn = turn; startPhase(g, 'GER'); };
const living = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'GER' && !c.dead && c.name === name);
const tellings = (g: Game): string[] => g.headlines.filter((h) => /formed from|left the front|has not formed yet|without your|The arrival/.test(h.text)).map((h) => h.text);
const telFlags = (g: Game): string[] => Object.keys(g.flags).filter((k) => /^row(Told|At|Front):/.test(k));

/** Germany's grand campaign, the player's, at a stop that keeps a book, the switch on: "Test Corps" in contact at Grenzmark, a row of month 2 taking it whole to form "Test Army" at Magdeburg. */
function board(stop: number): { g: Game; i: number; held: Corps } {
  const g = startOpen(createGame(1, 'GER'));
  expect(setStop(g, stop, true)).toBe(true);
  expect(stopDef(g).book).toBe(true);
  g.flags.fromWait = 1;
  const held = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
  const i = addRow([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps'] }]);
  return { g, i, held };
}

describe("the old chart at a book stop: the west's book closed, the player's row waits and is told", () => {
  it.each([1, 2])('at stop %i: the headline once, and the standing line on the corps', (stop) => {
    const { g, i, held } = board(stop);
    g.flags['hist:off:west'] = 1;
    month(g, 2);
    expect(g.flags[`wait:${i}`]).toBe(2);
    expect(g.regions[held.reg].name).toBe('Grenzmark');
    expect(living(g, 'Test Army')).toBeUndefined();
    expect(tellings(g)).toEqual([WORDS.begins]);
    expect(arrivalWaitOf(g, held)).toEqual({ short: 'An arrival waits for it', whole: WORDS.standing });
    month(g, 3);
    expect(tellings(g)).toEqual([WORDS.begins]);
    expect(telFlags(g).sort()).toEqual([`rowAt:${i}`, `rowFront:${i}`, `rowTold:${i}`]);
  });
});

describe("the old chart at a book stop: the west's book open, the row takes at its date and nothing is told", () => {
  it.each([1, 2])('at stop %i: the corps in contact is taken in the row\'s month; no line, no flag of the telling', (stop) => {
    const { g, i, held } = board(stop);
    month(g, 2);
    expect(g.flags['hist:off:west']).toBeUndefined();
    expect(g.flags[`wait:${i}`]).toBeUndefined();
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(held.dead).toBe(true);
    expect(g.regions[living(g, 'Test Army')!.reg].name).toBe('Magdeburg');
    expect(tellings(g)).toEqual([]);
    expect(telFlags(g)).toEqual([]);
  });
});

describe("the design director's answer 1: a book closing at the opening of a row's month binds the row that month", () => {
  it('through the real month, at stop 1: the west\'s book closes as October 1939 opens, and the row due in October waits in October and is told', () => {
    const { g, i, held } = board(1);
    expect(g.turn).toBe(1);
    // (The west's first mark asks that Germany be at peace with Belgium until the spring of 1940: tests/contact-rule-acts.)
    declareWar(g, 'GER', 'BEL');
    expect(bookClosed(g, 'west')).toBe(0);
    // His month ends, every other country plays, the month turns and opens with the book's marks, and his next begins.
    while (g.turn < 2 || !humanUp(g)) expect(passMonth(g)).toBe(true);
    expect(g.turn).toBe(2);
    expect(bookClosed(g, 'west')).toBe(2);
    expect(held.dead).toBeFalsy();
    expect(g.regions[held.reg].name).toBe('Grenzmark');
    expect(living(g, 'Test Army')).toBeUndefined();
    expect(g.flags[`sent:${i}`]).toBeUndefined();
    expect(g.flags[`wait:${i}`]).toBe(2);
    expect(g.headlines.filter((h) => h.text === WORDS.begins).map((h) => h.turn)).toEqual([2]);
  });
});
