// Round 2's sceptic "telling", three findings mended (T2-3, T2-4, T2-5). Every sentence the telling says is true of the
// month it is said, and the place is said once:
// - T2-3: a corps of the mask renamed while the row waits (here by an earlier row that joins it, the same month) is not
//   "lost": it stands under its new name. It is known by its id, not its name; the row forms with today's bare line.
// - T2-4: a told row that joins a corps standing elsewhere: "now that it stands there" is said only of the province the
//   sentence names, the place of the corps it joins. A corps at the front anywhere else: nothing true to say, and as
//   the base nothing is said for a joins row without news.
// - T2-5: the UI director's section 8 point 5 (02:07Z, 26 November 2026), word for word: "{The 8th Tank Regiment} and
//   {the 13th Tank Regiment} in {the Grenzmark} are at the front, ... Places mixed: grouped by place, the groups set off
//   by commas: "{A} and {B} in {X}, and {C} in {Y}, are at the front"." Section 1's own form where every place differs.
//
// The board is tests/from-wait-r2's: September 1939, Grenzmark beside Poznan and the Saar beside France, at war with
// Germany; Bavaria and Magdeburg quiet. Germany is the player's, the switch on by hand.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { waitBeginsLine } from '../src/campaign/arrivalwaits';
import { corpsInContact } from '../src/campaign/drawn';
import { addCorps, createGame, relocate, startPhase, theWords } from '../src/campaign/game';
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
const said = (g: Game, t: number): string[] => g.headlines.filter((h) => h.turn === t && h.nat === 'GER' && /Test|Old Corps/.test(h.text)).map((h) => h.text);
const tel = (g: Game, i: number): string[] => Object.keys(g.flags).filter((k) => /^row[A-Za-z]+:/.test(k) && k.split(':')[1] === String(i));
const open = (): Game => { const g = startOpen(createGame(1, 'GER')); g.flags.fromWait = 1; return g; };

describe('T2-3: a corps of the mask renamed while the row waits is not lost', () => {
  it('renamed the month the row forms by an earlier row that joins it: today\'s bare line, never "which has been lost"', () => {
    const g = open();
    const held = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
    // (The earlier row, in the order the rule reads them: it joins the Test Corps in month 3 and renames it.)
    ROWS.push([3, 'GER', 'Test Corps (reinforced)', 'inf', 1, 'Bavaria', { joins: 'Test Corps' }]); added++;
    ROWS.push([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps'] }]); added++;
    const i = ROWS.length - 1;
    month(g, 2);
    expect(said(g, 2)).toEqual(['The Test Army is due in Magdeburg this month, formed from your Test Corps, which is at the front in the Grenzmark. The arrival waits, and forms the month after that corps leaves the front.']);
    relocate(g, held, g.byName.Bavaria);
    month(g, 3);
    expect(held.name).toBe('Test Corps (reinforced)');
    expect(held.dead).toBe(false);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(said(g, 3).filter((t) => /Test Army/.test(t))).toEqual(['Test Army has arrived in Magdeburg.']);
    expect(tel(g, i)).toEqual([]);
  });
});

describe('T2-4: a told row that joins a corps in another province', () => {
  it('its corps at the front in the province the row is due in, the Old Corps in Magdeburg: never "now that it stands there"', () => {
    const g = open();
    const old = addCorps(g, 'GER', 'Old Corps', 'inf', 2, g.byName.Magdeburg.id, {});
    const held = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Saar.id, {});
    ROWS.push([2, 'GER', 'Old Corps (Test Divisions)', 'inf', 2, 'Grenzmark', { from: ['Test Corps'], joins: 'Old Corps' }]); added++;
    const i = ROWS.length - 1;
    month(g, 2);
    expect(said(g, 2)).toEqual(['The Old Corps (Test Divisions) is due in the Grenzmark this month, formed from your Test Corps, which is at the front in the Saar. The arrival waits, and forms the month after that corps leaves the front.']);
    relocate(g, held, g.byName.Grenzmark);
    fought(g);
    expect(corpsInContact(g, 'GER', held)).toBe(true);
    month(g, 3);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(old.name).toBe('Old Corps (Test Divisions)');
    // (No news of its own, and nothing true to say: as the base, nothing.)
    expect(said(g, 3)).toEqual([]);
    expect(tel(g, i)).toEqual([]);
  });

  it('with news of its own: the news alone', () => {
    const g = open();
    addCorps(g, 'GER', 'Old Corps', 'inf', 2, g.byName.Magdeburg.id, {});
    const held = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Saar.id, {});
    ROWS.push([2, 'GER', 'Old Corps (Test Divisions)', 'inf', 2, 'Grenzmark', { from: ['Test Corps'], joins: 'Old Corps', news: 'Two new divisions join the Old Corps.' }]); added++;
    month(g, 2);
    relocate(g, held, g.byName.Grenzmark);
    month(g, 3);
    expect(said(g, 3)).toEqual(['Two new divisions join the Old Corps.']);
  });
});

describe('T2-5, section 8 point 5: the place once, grouped by place', () => {
  /** Germany's corps, each where `at` puts it and at the front there, and a row of month 2 taking them whole at Magdeburg. */
  const board = (at: string[], unformed: string[] = []): { g: Game; cs: Corps[] } => {
    const g = open();
    const names = ['Test Corps', 'Second Test Corps', 'Third Test Corps'].slice(0, at.length);
    const cs = names.map((n, k) => addCorps(g, 'GER', n, 'inf', 2, g.byName[at[k]].id, {}));
    fought(g);
    for (const c of cs) expect(corpsInContact(g, 'GER', c)).toBe(true);
    ROWS.push([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: [...names, ...unformed] }]); added++;
    return { g, cs };
  };

  it('two in one province: "The Test Corps and the Second Test Corps in the Grenzmark are at the front"', () => {
    const { g } = board(['Grenzmark', 'Grenzmark']);
    month(g, 2);
    expect(said(g, 2)).toEqual(['The Test Army is due in Magdeburg this month, formed from two of your corps. The Test Corps and the Second Test Corps in the Grenzmark are at the front, so the arrival waits, and forms the month after both leave the front.']);
  });

  it('places mixed: "{A} and {B} in {X}, and {C} in {Y}, are at the front"', () => {
    const { g } = board(['Grenzmark', 'Grenzmark', 'Saar']);
    month(g, 2);
    expect(said(g, 2)).toEqual(['The Test Army is due in Magdeburg this month, formed from three of your corps. The Test Corps and the Second Test Corps in the Grenzmark, and the Third Test Corps in the Saar, are at the front, so the arrival waits, and forms the month after all of them leave the front.']);
  });

  it('places mixed, the one of its own place named between: grouped by place, in the order the first of each is named', () => {
    const { g } = board(['Grenzmark', 'Saar', 'Grenzmark']);
    month(g, 2);
    expect(said(g, 2)).toEqual(['The Test Army is due in Magdeburg this month, formed from three of your corps. The Test Corps and the Third Test Corps in the Grenzmark, and the Second Test Corps in the Saar, are at the front, so the arrival waits, and forms the month after all of them leave the front.']);
  });

  it('every place different: section 1\'s own form, as before', () => {
    const { g } = board(['Grenzmark', 'Saar']);
    month(g, 2);
    expect(said(g, 2)).toEqual(['The Test Army is due in Magdeburg this month, formed from two of your corps. The Test Corps in the Grenzmark and the Second Test Corps in the Saar are at the front, so the arrival waits, and forms the month after both leave the front.']);
  });

  it('both causes, two at the front in one province and a row not formed (section 7 case 2\'s list, built the same way): the place once', () => {
    // (Both causes come only on the far side, where a row waits on a row: the headline's builder is read straight.)
    const { g, cs } = board(['Grenzmark', 'Grenzmark']);
    const from = ['Test Corps', 'Second Test Corps', 'Phantom Army (Test Division)'];
    expect(theWords(waitBeginsLine(g, 'GER', 'Test Army', 'Magdeburg', from, cs, ['Phantom Army (Test Division)'])!)).toBe('The Test Army is due in Magdeburg this month. The Test Corps and the Second Test Corps in the Grenzmark are at the front, and the Phantom Army (Test Division), which it is also formed from, has not formed yet, so the arrival waits for all of them.');
  });
});
