// The dial turned in mid-wait, the old chart (arrivalwaits.ts `waitedOn`, `waiting`; game.ts `expeditions`). A row of
// the player's waits at stop 3 and is told; the dial is then turned to stop 1 with the west's book open, so the contact
// rule no longer acts where the row is due (history.ts `contactRuleActs`) and the row forms next month wherever its
// corps stands. Its corps is still in the line: the formed headline is today's bare line, not section 4's "now that it
// has left the front", and neither card keeps saying it forms the month after the corps leaves the front (the marks'
// preamble: the telling speaks "only where the rule acts"). The refuter's cases-1 and interplay-1.
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
  // (Today's bare line, game.ts `expeditions`, as the build gives it for every row outside the telling: the row's name with no article.)
  bare: 'Test Army has arrived in Magdeburg.',
};

const ROWS = (setup as any).expeditions as any[];
let added = 0;
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } });
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'GER'); };
const said = (g: Game, t: number): string[] => g.headlines.filter((h) => h.turn === t && h.nat === 'GER' && /Test/.test(h.text)).map((h) => h.text);

describe('the dial turned from stop 3 to stop 1 while a told row waits, the west\'s book open', () => {
  it('the cards go quiet at once, and next month the row forms with today\'s bare line', () => {
    const g = startOpen(createGame(1, 'GER'));
    expect(setStop(g, 3, true)).toBe(true);
    g.flags.fromWait = 1;
    const c = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
    ROWS.push([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps'] }]); added++;
    const i = ROWS.length - 1;
    month(g, 2);
    expect(said(g, 2)).toEqual([WORDS.begins]);
    expect(arrivalWaitOf(g, c)?.whole).toBe(WORDS.standing);
    expect(setStop(g, 1)).toBe(true);
    expect(stopOf(g)).toBe(1);
    expect(contactRuleActs(g, g.byName.Magdeburg)).toBe(false);
    expect(corpsInContact(g, 'GER', c)).toBe(true);
    // (The rule no longer holds the row: the corps' standing line goes with it.)
    expect(arrivalWaitOf(g, c)).toBeNull();
    month(g, 3);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(said(g, 3)).toEqual([WORDS.bare]);
    expect(Object.keys(g.flags).filter((k) => /^row(Told|At|Front):/.test(k))).toEqual([]);
  });
});
