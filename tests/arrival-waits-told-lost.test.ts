// A corps at the front lost while its row waits (arrivalwaits.ts `waitedOn`, `formedLine`). The row was told it waits
// because that corps is at the front; the corps is then destroyed, and the row forms. It used to say "formed from your
// {other corps}, now that it has left the front" of a corps never at the front. The UI director's section 8 point 1
// (02:10Z, 26 November 2026), word for word: "{The 25th Army (18th Division)} has {landed at / arrived in} {Hainan},
// formed without your {21st Army (18th Division)}, which has been lost." Same name: "formed without your corps of that
// name, which has been lost." Several: "{Row} has arrived in {Paotow}, formed from {three} of your corps, without {the
// 13th Tank Regiment}, which has been lost." After a general's line or news: "The arrival is formed without your {21st
// Army (18th Division)}, which has been lost." The refuter's cases-2 (its R2 and R2b boards).
//
// The board is tests/from-wait-r2's: September 1939, Grenzmark beside Poznan, Poland at war with Germany; Bavaria
// quiet. Germany is the player's, the switch on by hand.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { addCorps, createGame, relocate, removeCorps, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { storyFallen } from '../src/campaign/stories';
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
const tel = (g: Game, i: number): string[] => Object.keys(g.flags).filter((k) => k.endsWith(`:${i}`) && /^row(Told|At|Front):/.test(k));

/** Germany's corps `names`, the first at Grenzmark in contact and the rest in quiet Bavaria, and a row of month 2 taking them whole at Magdeburg. */
function board(rowName: string, names: string[], opts: Record<string, unknown> = {}): { g: Game; cs: Corps[]; i: number } {
  const g = startOpen(createGame(1, 'GER'));
  g.flags.fromWait = 1;
  const cs = names.map((n, k) => addCorps(g, 'GER', n, 'inf', 2, g.byName[k === 0 ? 'Grenzmark' : 'Bavaria'].id, {}));
  ROWS.push([2, 'GER', rowName, 'inf', 2, 'Magdeburg', { from: names, ...opts }]); added++;
  return { g, cs, i: ROWS.length - 1 };
}
/**
 * Told in month 2; the corps at the front destroyed, as game.ts destroys one in a battle (`storyFallen`, which writes it
 * on the memorial wall, then `removeCorps`: "lost" is said only of a corps on the wall, round 3); month 3, the row forms.
 */
const lose = (g: Game, c: Corps): void => { month(g, 2); expect(said(g, 2)).toHaveLength(1); storyFallen(g, c, 'destroyed', 'POL', c.reg); removeCorps(g, c); month(g, 3); };

describe('section 8 point 1: the corps at the front is lost while the row waits', () => {
  it('one corps: "formed without your Test Corps, which has been lost"', () => {
    const { g, cs, i } = board('Test Army', ['Test Corps']);
    lose(g, cs[0]);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(said(g, 3)).toEqual(['The Test Army has arrived in Magdeburg, formed without your Test Corps, which has been lost.']);
    expect(tel(g, i)).toEqual([]);
  });

  it('the same name: "formed without your corps of that name, which has been lost"', () => {
    const { g, cs } = board('Test Corps', ['Test Corps']);
    lose(g, cs[0]);
    expect(said(g, 3)).toEqual(['The Test Corps has arrived in Magdeburg, formed without your corps of that name, which has been lost.']);
  });

  it('several, two left (R2b): "formed from two of your corps, without the Test Corps, which has been lost"', () => {
    const { g, cs } = board('Test Army', ['Test Corps', 'Second Test Corps', 'Third Test Corps']);
    lose(g, cs[0]);
    expect(said(g, 3)).toEqual(['The Test Army has arrived in Magdeburg, formed from two of your corps, without the Test Corps, which has been lost.']);
  });

  it('two, one left (R2): never "formed from your Second Test Corps, now that it has left the front"', () => {
    const { g, cs } = board('Test Army', ['Test Corps', 'Second Test Corps']);
    lose(g, cs[0]);
    expect(said(g, 3)).toEqual(['The Test Army has arrived in Magdeburg, formed from your Second Test Corps, without the Test Corps, which has been lost.']);
  });

  it("after a general's line: \"The arrival is formed without your Test Corps, which has been lost.\"", () => {
    const { g, cs } = board('Test Army', ['Test Corps'], { gen: 'Hans Test' });
    lose(g, cs[0]);
    expect(said(g, 3)).toEqual(['Hans Test arrives in Magdeburg with the Test Army. The arrival is formed without your Test Corps, which has been lost.']);
  });

  it('after its own news, several: "The arrival is formed from two of your corps, without the Test Corps, which has been lost."', () => {
    const { g, cs } = board('Test Army', ['Test Corps', 'Second Test Corps', 'Third Test Corps'], { news: 'The Test Army is gathered in Magdeburg.' });
    lose(g, cs[0]);
    expect(said(g, 3)).toEqual(['The Test Army is gathered in Magdeburg. The arrival is formed from two of your corps, without the Test Corps, which has been lost.']);
  });

  it('a corps that left the front and lives: section 4 as before', () => {
    const { g, cs } = board('Test Army', ['Test Corps', 'Second Test Corps']);
    month(g, 2);
    relocate(g, cs[0], g.byName.Bavaria);
    month(g, 3);
    expect(said(g, 3)).toEqual(['The Test Army has arrived in Magdeburg, formed from two of your corps, now that the Test Corps has left the front.']);
  });
});
