// The telling on the old chart (arrivalwaits.ts; the UI director's words of 21:49Z, 25 November 2026: one set of words
// for both charts). The old chart's statement in game.ts `expeditions` holds a row that takes a named corps whole
// while that corps is in contact, under the switch 'fromWait' at stops 3 to 5 (tests/from-wait-r2); it tells the
// player as the far side's clause does. The board is tests/from-wait-r2's: September 1939, Grenzmark beside Poznan,
// Poland at war with Germany; Bavaria quiet. Germany is the player's. The rows are added at the foot of the data's
// list and taken off after each test.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { arrivalWaitOf } from '../src/campaign/arrivalwaits';
import { addCorps, createGame, relocate, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

const ROWS = (setup as any).expeditions as any[];
let added = 0;
function addRow(row: any[]): number { ROWS.push(row); added++; return ROWS.length - 1; }
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } });

/**
 * (The contact rule's second form, the design director's ruling of 06:10Z, 26 November 2026: a corps beside the enemy
 * is held while it took part in the battle last resolved on its ground, or where nobody of its side keeps its province.
 * This board's corps at the front stand with others of their side, so each is set, by hand, as having fought the battle
 * last resolved where it stands, and is held as the board means. The relieved case: tests/contact-fact.test.ts.)
 */
const fought = (g: Game): void => { if (g.flags.fromWait !== 1) return; g.lastBattle = {}; for (const c of g.corps) if (!c.dead) (g.lastBattle[c.reg] ??= []).push(c.id); };
const month = (g: Game, turn: number): void => { fought(g); g.turn = turn; startPhase(g, 'GER'); };
const tellings = (g: Game): string[] => g.headlines.filter((h) => /formed from|left the front|has not formed yet|without your|The arrival/.test(h.text)).map((h) => h.text);
const telFlags = (g: Game): string[] => Object.keys(g.flags).filter((k) => /^row(Told|At|Front):/.test(k));

/** A grand campaign with a German corps "Test Corps" at `where`, and a row of month 2 that takes it whole to form "Test Army" at Magdeburg. */
function board(where: string, seat: string | null = 'GER', on = true): { g: Game; i: number; held: Corps } {
  const g = startOpen(createGame(1, seat));
  if (on) g.flags.fromWait = 1;
  const held = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName[where].id, {});
  const i = addRow([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps'] }]);
  return { g, i, held };
}

describe('the old chart tells as the far side does', () => {
  it('the headline once, the standing line on the corps, and the formed headline', () => {
    const { g, i, held } = board('Grenzmark');
    month(g, 2);
    expect(g.flags[`wait:${i}`]).toBe(2);
    // (The Grenzmark takes "the" as every headline gives it: data/campaign/places.json, game.ts `theWords`.)
    expect(tellings(g)).toEqual(['The Test Army is due in Magdeburg this month, formed from your Test Corps, which is at the front in the Grenzmark. The arrival waits, and forms the month after that corps leaves the front.']);
    expect(arrivalWaitOf(g, held)).toEqual({ short: 'An arrival waits for it', whole: 'An arrival due in Magdeburg, the Test Army, is to be formed from this corps, and has waited for it since October 1939. It forms the month after this corps leaves the front.' });
    month(g, 3);
    expect(tellings(g)).toHaveLength(1);
    relocate(g, held, g.byName.Bavaria);
    expect(arrivalWaitOf(g, held)).toBeNull();
    month(g, 4);
    expect(tellings(g).slice(1)).toEqual(['The Test Army has arrived in Magdeburg, formed from your Test Corps, now that it has left the front.']);
    expect(telFlags(g)).toEqual([]);
  });

  it('the switch off: the corps is taken where it stands, nothing told, no flag', () => {
    const { g, i } = board('Grenzmark', 'GER', false);
    month(g, 2);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(tellings(g)).toEqual([]);
    expect(telFlags(g)).toEqual([]);
  });

  it("a computer's row: the row waits, nothing told, no flag", () => {
    const { g, i } = board('Grenzmark', null);
    month(g, 2);
    expect(g.flags[`wait:${i}`]).toBe(2);
    expect(tellings(g)).toEqual([]);
    expect(telFlags(g)).toEqual([]);
  });

  it.each([1, 2])('at stop %i: nothing told, no flag', (stop) => {
    const { g } = board('Grenzmark');
    expect(setStop(g, stop, true)).toBe(true);
    month(g, 2);
    expect(tellings(g)).toEqual([]);
    expect(telFlags(g)).toEqual([]);
  });
});
