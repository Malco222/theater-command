// A corps at the front that marched into the province the row forms in, and is still at the front there
// (arrivalwaits.ts `waitedOn`, `formedLine`). The rule lets a corps that stands where the row forms be taken (game.ts
// `expeditions`: `x.reg !== formsAt`), so the wait ends without the corps leaving the front, and "now that it has left
// the front" would be false. The UI director's section 8 point 2 (02:10Z, 26 November 2026), word for word: "{Row} has
// {arrived in / landed at} {Hainan}, formed from your {21st Army (18th Division)}, now that it stands there." Same name:
// "formed from your corps of that name, now that it stands there." Several: "now that {the 13th Tank Regiment} stands
// there". Mixed with one that left the front: "now that {A} has left the front and {B} stands there". After a general's
// line or news: "The arrival is formed from your {21st Army (18th Division)}, now that it stands there." The refuter's
// cases-3 (its R3 board).
//
// The board: September 1939, Germany the player's, the switch on by hand; the Test Corps at the front in the Saar, the
// row due in the Grenzmark (at the front too, beside Poznan); Bavaria quiet.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { corpsInContact } from '../src/campaign/drawn';
import { addCorps, createGame, relocate, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

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
const said = (g: Game, t: number): string[] => g.headlines.filter((h) => h.turn === t && h.nat === 'GER' && /Test|Hans/.test(h.text)).map((h) => h.text);

/** Germany's corps `names` at the front in the Saar, and a row of month 2 taking them whole in the Grenzmark. */
function board(rowName: string, names: string[], opts: Record<string, unknown> = {}): { g: Game; cs: Corps[]; i: number } {
  const g = startOpen(createGame(1, 'GER'));
  g.flags.fromWait = 1;
  const cs = names.map((n) => addCorps(g, 'GER', n, 'inf', 2, g.byName.Saar.id, {}));
  ROWS.push([2, 'GER', rowName, 'inf', 2, 'Grenzmark', { from: names, ...opts }]); added++;
  return { g, cs, i: ROWS.length - 1 };
}
const into = (g: Game, c: Corps): void => { relocate(g, c, g.byName.Grenzmark); fought(g); expect(corpsInContact(g, 'GER', c)).toBe(true); };

describe('section 8 point 2: the corps stands at the front in the province the row forms in', () => {
  it('one corps (R3): "formed from your Test Corps, now that it stands there"', () => {
    const { g, cs, i } = board('Test Army', ['Test Corps']);
    month(g, 2);
    expect(said(g, 2)).toEqual(['The Test Army is due in the Grenzmark this month, formed from your Test Corps, which is at the front in the Saar. The arrival waits, and forms the month after that corps leaves the front.']);
    into(g, cs[0]);
    month(g, 3);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(said(g, 3)).toEqual(['The Test Army has arrived in the Grenzmark, formed from your Test Corps, now that it stands there.']);
  });

  it('the same name: "formed from your corps of that name, now that it stands there"', () => {
    const { g, cs } = board('Test Corps', ['Test Corps']);
    month(g, 2); into(g, cs[0]); month(g, 3);
    expect(said(g, 3)).toEqual(['The Test Corps has arrived in the Grenzmark, formed from your corps of that name, now that it stands there.']);
  });

  it('several: "now that the Test Corps stands there"', () => {
    const { g, cs } = board('Test Army', ['Test Corps', 'Second Test Corps']);
    relocate(g, cs[1], g.byName.Bavaria);
    month(g, 2); into(g, cs[0]); month(g, 3);
    expect(said(g, 3)).toEqual(['The Test Army has arrived in the Grenzmark, formed from two of your corps, now that the Test Corps stands there.']);
  });

  it('mixed with one that left the front: "now that the Second Test Corps has left the front and the Test Corps stands there"', () => {
    const { g, cs } = board('Test Army', ['Test Corps', 'Second Test Corps']);
    month(g, 2);
    into(g, cs[0]);
    relocate(g, cs[1], g.byName.Bavaria);
    month(g, 3);
    expect(said(g, 3)).toEqual(['The Test Army has arrived in the Grenzmark, formed from two of your corps, now that the Second Test Corps has left the front and the Test Corps stands there.']);
  });

  it("after a general's line: \"The arrival is formed from your Test Corps, now that it stands there.\"", () => {
    const { g, cs } = board('Test Army', ['Test Corps'], { gen: 'Hans Test' });
    month(g, 2); into(g, cs[0]); month(g, 3);
    expect(said(g, 3)).toEqual(['Hans Test arrives in the Grenzmark with the Test Army. The arrival is formed from your Test Corps, now that it stands there.']);
  });

  it('a corps that marched in and is no longer at the front there has left the front: section 4 as before', () => {
    const { g, cs } = board('Test Army', ['Test Corps']);
    month(g, 2);
    relocate(g, cs[0], g.byName.Bavaria);
    month(g, 3);
    expect(said(g, 3)).toEqual(['The Test Army has arrived in the Grenzmark, formed from your Test Corps, now that it has left the front.']);
  });
});
