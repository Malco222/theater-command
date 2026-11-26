// The telling on the old chart: the UI director's section 7 cases that need no chain (pacific-japan/
// ui-words-row-waits-contact-UI-MARKS.md, 00:44Z, 26 November 2026), each whole sentence (arrivalwaits.ts). And
// section 4's second sentence after a row's general's line or its own news, one corps and several.
//   case 3: the same name, after a general's line: "The arrival is formed from your corps of that name, ...";
//   case 4: three or more at the front, on the old chart's statement;
//   case 5: a name whose head (before any bracket) ends in "Force" takes no article ("theOf", the one function).
// The board is tests/from-wait-r2's: September 1939, Grenzmark beside Poznan, Poland at war with Germany; Bavaria
// quiet. Germany is the player's, the switch on by hand. Rows are added at the foot of the data's list and taken off.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { arrivalWaitOf, named, theOf } from '../src/campaign/arrivalwaits';
import { addCorps, createGame, relocate, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

const ROWS = (setup as any).expeditions as any[];
let added = 0;
function addRow(row: any[]): number { ROWS.push(row); added++; return ROWS.length - 1; }
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } });

const month = (g: Game, turn: number): void => { g.turn = turn; startPhase(g, 'GER'); };
const said = (g: Game, t: number): string[] => g.headlines.filter((h) => h.turn === t && h.nat === 'GER' && /Test|Hans|Special Force|Gull Force/.test(h.text)).map((h) => h.text);

/** Germany's corps `names` at Grenzmark, in contact, and a row of month 2 `rowName` taking them whole at Magdeburg. */
function board(rowName: string, names: string[], opts: Record<string, unknown> = {}): { g: Game; cs: Corps[] } {
  const g = startOpen(createGame(1, 'GER'));
  g.flags.fromWait = 1;
  const cs = names.map((n) => addCorps(g, 'GER', n, 'inf', 2, g.byName.Grenzmark.id, {}));
  addRow([2, 'GER', rowName, 'inf', 2, 'Magdeburg', { from: names, ...opts }]);
  return { g, cs };
}
const free = (g: Game, cs: Corps[]): void => { for (const c of cs) relocate(g, c, g.byName.Bavaria); };

describe("section 4's second sentence, after a general's line or the row's own news", () => {
  it("case 3: the same name after a general's line: \"your corps of that name\"", () => {
    const { g, cs } = board('Test Corps', ['Test Corps'], { gen: 'Hans Test' });
    month(g, 2);
    free(g, cs);
    month(g, 3);
    expect(said(g, 3)).toEqual(['Hans Test arrives in Magdeburg with the Test Corps. The arrival is formed from your corps of that name, now that it has left the front.']);
  });

  it("one corps, after the row's own news", () => {
    const { g, cs } = board('Test Army', ['Test Corps'], { news: 'The Test Army is gathered in Magdeburg.' });
    month(g, 2);
    free(g, cs);
    month(g, 3);
    expect(said(g, 3)).toEqual(['The Test Army is gathered in Magdeburg. The arrival is formed from your Test Corps, now that it has left the front.']);
  });

  it("several, after a general's line: \"from {two} of your corps, now that {A} and {B} have left the front\"", () => {
    const { g, cs } = board('Test Army', ['Test Corps', 'Second Test Corps'], { gen: 'Hans Test' });
    month(g, 2);
    free(g, cs);
    month(g, 3);
    expect(said(g, 3)).toEqual(['Hans Test arrives in Magdeburg with the Test Army. The arrival is formed from two of your corps, now that the Test Corps and the Second Test Corps have left the front.']);
  });

  it("several, after the row's own news", () => {
    const { g, cs } = board('Test Army', ['Test Corps', 'Second Test Corps'], { news: 'The Test Army is gathered in Magdeburg.' });
    month(g, 2);
    free(g, cs);
    month(g, 3);
    expect(said(g, 3)).toEqual(['The Test Army is gathered in Magdeburg. The arrival is formed from two of your corps, now that the Test Corps and the Second Test Corps have left the front.']);
  });
});

describe('case 4: three at the front on the old chart', () => {
  it('the standing line names the other two whole, and the formed headline all three', () => {
    const { g, cs } = board('Test Army', ['Test Corps', 'Second Test Corps', 'Third Test Corps']);
    month(g, 2);
    expect(arrivalWaitOf(g, cs[0])?.whole).toBe('An arrival due in Magdeburg, the Test Army, is to be formed from this corps, and has waited for it since October 1939. It forms the month after this corps, the Second Test Corps and the Third Test Corps leave the front.');
    free(g, cs);
    month(g, 3);
    expect(said(g, 3)).toEqual(['The Test Army has arrived in Magdeburg, formed from three of your corps, now that the Test Corps, the Second Test Corps and the Third Test Corps have left the front.']);
  });
});

describe('case 5: a name whose head ends in "Force" takes no article', () => {
  it('theOf, the one function: the head before any bracket', () => {
    expect(theOf('Special Force (3rd Indian Division, the Chindits)')).toBe('');
    expect(theOf('Western Task Force (2nd Armored and 9th Infantry Divisions)')).toBe('');
    expect(theOf('Gull Force (2/21st Battalion)')).toBe('');
    expect(theOf('W Force (New Zealand Division and 1st Armoured Brigade)')).toBe('');
    expect(theOf('Creforce')).toBe('');
    // (Only the head: a bracket that holds "force" does not count.)
    expect(theOf('13th Army (Chekiang landing force)')).toBe('the ');
    expect(theOf('Test Army')).toBe('the ');
    expect(named('Special Force (3rd Indian Division, the Chindits)', true)).toBe('Special Force (3rd Indian Division, the Chindits)');
  });

  it('"Special Force is due in ...": the headline, the standing line and the formed headline', () => {
    const name = 'Special Force (3rd Indian Division, the Chindits)';
    const { g, cs } = board(name, ['Test Corps']);
    month(g, 2);
    expect(said(g, 2)).toEqual(['Special Force (3rd Indian Division, the Chindits) is due in Magdeburg this month, formed from your Test Corps, which is at the front in the Grenzmark. The arrival waits, and forms the month after that corps leaves the front.']);
    expect(arrivalWaitOf(g, cs[0])?.whole).toBe('An arrival due in Magdeburg, Special Force (3rd Indian Division, the Chindits), is to be formed from this corps, and has waited for it since October 1939. It forms the month after this corps leaves the front.');
    free(g, cs);
    month(g, 3);
    expect(said(g, 3)).toEqual(['Special Force (3rd Indian Division, the Chindits) has arrived in Magdeburg, formed from your Test Corps, now that it has left the front.']);
  });

  it('a corps of that kind in the standing line and the several headline: "this corps and Gull Force"', () => {
    const { g, cs } = board('Test Army', ['Test Corps', 'Gull Force (2/21st Battalion)']);
    month(g, 2);
    expect(said(g, 2)).toEqual(['The Test Army is due in Magdeburg this month, formed from two of your corps. The Test Corps and Gull Force (2/21st Battalion) in the Grenzmark are at the front, so the arrival waits, and forms the month after both leave the front.']);
    expect(arrivalWaitOf(g, cs[0])?.whole).toBe('An arrival due in Magdeburg, the Test Army, is to be formed from this corps, and has waited for it since October 1939. It forms the month after this corps and Gull Force (2/21st Battalion) leave the front.');
  });
});
