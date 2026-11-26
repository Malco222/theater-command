// A told row that joins a corps already landed (`joins`), or that every division of which already stands, the month
// its wait ends (game.ts `expeditions`; arrivalwaits.ts `joinedLine`, `notFormedLine`). He was told "The arrival
// waits", so the month the wait ends he is told how. Section 4: "A row whose headline today is its general's line or its
// own news: that prints as it is, and then, for the player's own row: The arrival is formed from your {corps}, now that
// it has left the front." The UI director's section 8 point 3 (02:10Z, 26 November 2026), word for word, for a row that
// joins with no news of its own: "{Row} has joined {the Old Corps} in {place}, formed from your {Test Corps}, now that
// it has left the front." (with points 1's and 2's endings where those are the true case); and where every division of
// the row already stands: "{The 18th Division}, due in {Rangoon}, will not form: every division of it already stands
// with your forces." The refuter's cases-4 (its R4 board). A real row of the shape: row 85, the USA's "Fifth Army: II
// Corps (34th, 36th, 85th and 88th Infantry Divisions)", month 56, joins, news and a from list.
//
// The board is tests/from-wait-r2's: September 1939, Grenzmark beside Poznan, Poland at war with Germany; Bavaria and
// Magdeburg quiet. Germany is the player's, the switch on.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { setFromWait } from '../src/campaign/arrivalwaits';
import { corpsInContact } from '../src/campaign/drawn';
import { addCorps, createGame, relocate, removeCorps, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { storyFallen } from '../src/campaign/stories';
import type { Corps, Game } from '../src/campaign/types';

const ROWS = (setup as any).expeditions as any[];
let added = 0;
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } });
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'GER'); };
const said = (g: Game, t: number): string[] => g.headlines.filter((h) => h.turn === t && h.nat === 'GER' && /Test|Old Corps/.test(h.text)).map((h) => h.text);
const tel = (g: Game, i: number): string[] => Object.keys(g.flags).filter((k) => k.endsWith(`:${i}`) && /^row(Told|At|Front):/.test(k));

/** Germany's Test Corps at the front at Grenzmark, the Old Corps at Magdeburg, and a row of month 2 that joins the Old Corps, taking the Test Corps whole. */
function board(opts: Record<string, unknown> = {}): { g: Game; held: Corps; old: Corps; i: number } {
  const g = startOpen(createGame(1, 'GER'));
  g.flags.fromWait = 1;
  const old = addCorps(g, 'GER', 'Old Corps', 'inf', 2, g.byName.Magdeburg.id, {});
  const held = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
  ROWS.push([2, 'GER', 'Old Corps (Test Divisions)', 'inf', 2, 'Magdeburg', { from: ['Test Corps'], joins: 'Old Corps', ...opts }]); added++;
  return { g, held, old, i: ROWS.length - 1 };
}
const BEGINS = 'The Old Corps (Test Divisions) is due in Magdeburg this month, formed from your Test Corps, which is at the front in the Grenzmark. The arrival waits, and forms the month after that corps leaves the front.';

describe('a told row that joins a standing corps (`joins`)', () => {
  it('with news of its own: the news, then section 4\'s sentence', () => {
    const { g, held, old, i } = board({ news: 'Two new divisions join the Old Corps.' });
    month(g, 2);
    expect(said(g, 2)).toEqual([BEGINS]);
    relocate(g, held, g.byName.Bavaria);
    month(g, 3);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(old.name).toBe('Old Corps (Test Divisions)');
    expect(said(g, 3)).toEqual(['Two new divisions join the Old Corps. The arrival is formed from your Test Corps, now that it has left the front.']);
    expect(tel(g, i)).toEqual([]);
  });

  it('with no news of its own: section 8 point 3, "has joined the Old Corps in Magdeburg"', () => {
    const { g, held, i } = board();
    month(g, 2);
    relocate(g, held, g.byName.Bavaria);
    month(g, 3);
    expect(said(g, 3)).toEqual(['The Old Corps (Test Divisions) has joined the Old Corps in Magdeburg, formed from your Test Corps, now that it has left the front.']);
    expect(tel(g, i)).toEqual([]);
  });

  it('with no news, its corps lost while it waited: point 1\'s ending', () => {
    const { g, held } = board();
    month(g, 2);
    // (Destroyed as game.ts destroys one in a battle: on the memorial wall, then off the board. "Lost" is said only of a
    // corps on the wall, round 3.)
    storyFallen(g, held, 'destroyed', 'POL', held.reg);
    removeCorps(g, held);
    month(g, 3);
    expect(said(g, 3)).toEqual(['The Old Corps (Test Divisions) has joined the Old Corps in Magdeburg, formed without your Test Corps, which has been lost.']);
  });

  it('with no news, its corps at the front where the row forms: point 2\'s ending', () => {
    // (Here the row and the corps it joins are in the Grenzmark, at the front beside Poznan; the Test Corps at the
    // front in the Saar marches into the Grenzmark, and is at the front there.)
    const g = startOpen(createGame(1, 'GER'));
    g.flags.fromWait = 1;
    addCorps(g, 'GER', 'Old Corps', 'inf', 2, g.byName.Grenzmark.id, {});
    const held = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Saar.id, {});
    ROWS.push([2, 'GER', 'Old Corps (Test Divisions)', 'inf', 2, 'Grenzmark', { from: ['Test Corps'], joins: 'Old Corps' }]); added++;
    month(g, 2);
    expect(said(g, 2)).toEqual(['The Old Corps (Test Divisions) is due in the Grenzmark this month, formed from your Test Corps, which is at the front in the Saar. The arrival waits, and forms the month after that corps leaves the front.']);
    relocate(g, held, g.byName.Grenzmark);
    expect(corpsInContact(g, 'GER', held)).toBe(true);
    month(g, 3);
    expect(said(g, 3)).toEqual(['The Old Corps (Test Divisions) has joined the Old Corps in the Grenzmark, formed from your Test Corps, now that it stands there.']);
  });

  it('with the switch off: as the base, nothing is said where there is no news', () => {
    const { g, held, i } = board();
    month(g, 2);
    expect(setFromWait(g, false)).toBe(true);
    relocate(g, held, g.byName.Bavaria);
    month(g, 3);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(said(g, 3)).toEqual([]);
    expect(tel(g, i)).toEqual([]);
  });
});

describe('a told row every division of which already stands', () => {
  it('section 8 point 3: "will not form: every division of it already stands with your forces"', () => {
    const g = startOpen(createGame(1, 'GER'));
    g.flags.fromWait = 1;
    const held = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
    // (The 217th Infantry Division is the Konigsberg garrison's: the player's own stands where it is.)
    ROWS.push([2, 'GER', 'Test Army (217th Infantry Division)', 'inf', 1, 'Magdeburg', { from: ['Test Corps'] }]); added++;
    const i = ROWS.length - 1;
    month(g, 2);
    expect(said(g, 2).filter((t) => /Test Army/.test(t))).toEqual(['The Test Army (217th Infantry Division) is due in Magdeburg this month, formed from your Test Corps, which is at the front in the Grenzmark. The arrival waits, and forms the month after that corps leaves the front.']);
    relocate(g, held, g.byName.Bavaria);
    month(g, 3);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(said(g, 3).filter((t) => /Test Army/.test(t))).toEqual(['The Test Army (217th Infantry Division), due in Magdeburg, will not form: every division of it already stands with your forces.']);
    expect(tel(g, i)).toEqual([]);
  });
});
