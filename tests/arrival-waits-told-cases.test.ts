// The telling, the far side: the UI director's eight more cases (section 7 of
// pacific-japan/ui-words-row-waits-contact-UI-MARKS.md, 00:44Z, 26 November 2026), each form's whole sentence against
// rows of Japan's, with Japan the player's (arrivalwaits.ts). The board is tests/arrival-waits-told's: seed 3, the far
// side on, the switch on by hand, Japan's strike south gone in month 28 and Rangoon hers. Test rows are added at the
// foot of the data's list and taken off after each test.
//   case 2: both causes with more than one of either (the headline, and the corps' standing line);
//   case 4: three or more names (the standing line, the formed headline, "which have not formed yet", "without your");
//   case 6: a row that waited on the front and then only for another row still gets section 4's headline;
//   case 7: two corps in one province name the place once (section 5).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { arrivalWaitOf, rowsWaitingOnRows } from '../src/campaign/arrivalwaits';
import { createGame, declareWar, relocate, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';

const ARMY21 = '21st Army (18th Division)', ARMY25 = '25th Army (18th Division)', ARMY7 = '6th Army (7th Division)';
const T8 = '8th Tank Regiment', T12 = '12th Tank Regiment', T13 = '13th Tank Regiment';
const SAKAGUCHI = 'Sakaguchi Detachment (56th Mixed Infantry Group)', CHEKIANG = '13th Army (Chekiang landing force)';
const PHANTOM = 'Phantom Army (Test Division)';
const rows = (setup as any).expeditions as any[][];
let added = 0;
const addRow = (row: any[]): number => { rows.push(row); added++; return rows.length - 1; };
afterEach(() => { while (added > 0) { rows.pop(); added--; } });

const corps = (g: Game, name: string): Corps => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead)!;
/**
 * (The contact rule's second form, the design director's ruling of 06:10Z, 26 November 2026: a corps beside the enemy
 * is held while it took part in the battle last resolved on its ground, or where nobody of its side keeps its province.
 * This board's corps at the front stand with others of their side, so each is set, by hand, as having fought the battle
 * last resolved where it stands, and is held as the board means. The relieved case: tests/contact-fact.test.ts.)
 */
const fought = (g: Game): void => { if (g.flags.fromWait !== 1) return; g.lastBattle = {}; for (const c of g.corps) if (!c.dead) (g.lastBattle[c.reg] ??= []).push(c.id); };
const month = (g: Game, t: number): void => { fought(g); g.turn = t; startPhase(g, 'JAP'); };
const told = (g: Game, t: number, about: string): string[] => g.headlines.filter((h) => h.turn === t && h.nat === 'JAP' && h.text.includes(about)).map((h) => h.text);
const lose = (g: Game, name: string, to: string, away: string): Region => { const r = g.byName[name]; for (const u of [...r.units]) relocate(g, u, g.byName[away]); r.ctrl = to; return r; };
const open = (): Game => { const g = startOpen(createGame(3, 'JAP')); g.flags.fromWait = 1; return g; };
const war = (): Game => {
  const g = open();
  lose(g, 'Cam Ranh', 'JAP', 'Saigon');
  month(g, 27);
  g.turn = 28; g.flags['st:done:JAP:south'] = 28;
  for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
  lose(g, 'Rangoon', 'JAP', 'Calcutta');
  return g;
};

describe('case 2: both causes at once', () => {
  it('one and one: the headline says "both", the corps at the front carries the line, and the month card has none', () => {
    const g = war();
    addRow([32, 'JAP', 'Test Army', 'inf', 1, 'Rangoon', { from: [ARMY7, ARMY25] }]);
    relocate(g, corps(g, ARMY7), g.byName['Kaifeng']);
    for (let t = 29; t <= 32; t++) month(g, t);
    expect(told(g, 32, 'Test Army')).toEqual(['The Test Army is due in Rangoon this month. The 6th Army (7th Division) is at the front in Kaifeng, and the 25th Army (18th Division), which it is also formed from, has not formed yet, so the arrival waits for both.']);
    expect(arrivalWaitOf(g, corps(g, ARMY7))).toEqual({
      short: 'An arrival waits for it',
      whole: 'An arrival due in Rangoon, the Test Army, is to be formed from this corps and from the 25th Army (18th Division), which has not formed yet, and has waited for it since April 1942. It forms once this corps leaves the front and the 25th Army (18th Division) has formed.',
    });
    expect(rowsWaitingOnRows(g, 'JAP').filter((x) => x.includes('Test Army'))).toEqual([]);
  });

  it('two at the front and one not formed: "{A} in {X} and {B} in {Y} are at the front ... waits for all of them", and "once this corps and {B} leave the front and {C} has formed"', () => {
    const g = war();
    addRow([32, 'JAP', 'Test Army', 'inf', 1, 'Rangoon', { from: [ARMY7, T12, ARMY25] }]);
    relocate(g, corps(g, ARMY7), g.byName['Kaifeng']);
    relocate(g, corps(g, T12), g.byName['Linfen']);
    for (let t = 29; t <= 32; t++) month(g, t);
    expect(told(g, 32, 'Test Army')).toEqual(['The Test Army is due in Rangoon this month. The 6th Army (7th Division) in Kaifeng and the 12th Tank Regiment in Linfen are at the front, and the 25th Army (18th Division), which it is also formed from, has not formed yet, so the arrival waits for all of them.']);
    expect(arrivalWaitOf(g, corps(g, ARMY7))?.whole).toBe('An arrival due in Rangoon, the Test Army, is to be formed from this corps and from the 25th Army (18th Division), which has not formed yet, and has waited for it since April 1942. It forms once this corps and the 12th Tank Regiment leave the front and the 25th Army (18th Division) has formed.');
  });

  it('one at the front and two not formed: "{C} and {D}, which it is also formed from, have not formed yet"', () => {
    const g = war();
    addRow([32, 'JAP', 'Test Army', 'inf', 1, 'Rangoon', { from: [ARMY7, ARMY25, CHEKIANG] }]);
    relocate(g, corps(g, ARMY7), g.byName['Kaifeng']);
    for (let t = 29; t <= 32; t++) month(g, t);
    expect(told(g, 32, 'Test Army')).toEqual(['The Test Army is due in Rangoon this month. The 6th Army (7th Division) is at the front in Kaifeng, and the 25th Army (18th Division) and the 13th Army (Chekiang landing force), which it is also formed from, have not formed yet, so the arrival waits for all of them.']);
    // (The marks give the standing line for one not formed; for two it takes the headline's "{C} and {D} ... have".)
    expect(arrivalWaitOf(g, corps(g, ARMY7))?.whole).toBe('An arrival due in Rangoon, the Test Army, is to be formed from this corps and from the 25th Army (18th Division) and the 13th Army (Chekiang landing force), which have not formed yet, and has waited for it since April 1942. It forms once this corps leaves the front and the 25th Army (18th Division) and the 13th Army (Chekiang landing force) have formed.');
  });
});

describe('case 4: three or more names, each whole with its article', () => {
  it('the Mongolia Garrison Army with three at the front: the standing line and the formed headline', () => {
    const g = open();
    relocate(g, corps(g, T8), g.byName['Kaifeng']);
    relocate(g, corps(g, T12), g.byName['Linfen']);
    month(g, 40);
    expect(arrivalWaitOf(g, corps(g, T13))?.whole).toBe('An arrival due in Paotow, the Mongolia Garrison Army (3rd Tank Division), is to be formed from this corps, and has waited for it since December 1942. It forms the month after this corps, the 8th Tank Regiment and the 12th Tank Regiment leave the front.');
    for (const n of [T8, T12, T13]) relocate(g, corps(g, n), g.byName['Mukden']);
    month(g, 41);
    expect(told(g, 41, 'Mongolia Garrison Army')).toEqual(['The Mongolia Garrison Army (3rd Tank Division) has arrived in Paotow, formed from four of your corps, now that the 8th Tank Regiment, the 12th Tank Regiment and the 13th Tank Regiment have left the front.']);
  });

  it('three not formed yet: the headline and the month card', () => {
    const g = war();
    // (A third row of Japan's that waits on the front, so its corps is not on the board: the chain.)
    relocate(g, corps(g, T12), g.byName['Linfen']);
    addRow([31, 'JAP', PHANTOM, 'inf', 1, 'Tokyo', { from: [T12] }]);
    addRow([32, 'JAP', 'Test Army', 'inf', 1, 'Rangoon', { from: [ARMY25, CHEKIANG, PHANTOM] }]);
    for (let t = 29; t <= 32; t++) month(g, t);
    expect(told(g, 32, 'Test Army')).toEqual(['The Test Army is due in Rangoon this month, formed from the 25th Army (18th Division), the 13th Army (Chekiang landing force) and the Phantom Army (Test Division), which have not formed yet. The arrival waits for them.']);
    expect(rowsWaitingOnRows(g, 'JAP').filter((x) => x.includes('Test Army'))).toEqual(['The Test Army, due in Rangoon, has waited since April 1942 for the 25th Army (18th Division), the 13th Army (Chekiang landing force) and the Phantom Army (Test Division), which have not formed yet.']);
  });
});

describe('case 6: waited on the front, then only for another row', () => {
  it("section 4's headline when it forms, though its last waiting month was on the chain alone", () => {
    const g = war();
    relocate(g, corps(g, ARMY7), g.byName['Kaifeng']);
    relocate(g, corps(g, T12), g.byName['Linfen']);
    const p = addRow([31, 'JAP', PHANTOM, 'inf', 1, 'Tokyo', { from: [T12] }]);
    const i = addRow([32, 'JAP', 'Test Army', 'inf', 1, 'Rangoon', { from: [ARMY7, PHANTOM] }]);
    for (let t = 29; t <= 32; t++) month(g, t);
    expect(g.flags[`wait:${p}`]).toBe(32);
    expect(told(g, 32, 'Test Army')).toEqual(['The Test Army is due in Rangoon this month. The 6th Army (7th Division) is at the front in Kaifeng, and the Phantom Army (Test Division), which it is also formed from, has not formed yet, so the arrival waits for both.']);
    // The 6th Army leaves the front: the row waits on the chain alone, and the month card carries it.
    relocate(g, corps(g, ARMY7), g.byName['Hiroshima']);
    month(g, 33);
    expect(g.flags[`wait:${i}`]).toBe(33);
    expect(rowsWaitingOnRows(g, 'JAP').filter((x) => x.includes('Test Army'))).toEqual(['The Test Army, due in Rangoon, has waited since April 1942 for the Phantom Army (Test Division), which has not formed yet.']);
    expect(g.flags[`rowFront:${i}`]).toBe(1);
    // The Phantom Army's corps leaves the front: it forms, and the Test Army with it.
    relocate(g, corps(g, T12), g.byName['Hiroshima']);
    month(g, 34);
    expect(corps(g, 'Test Army')).toBeDefined();
    expect(told(g, 34, 'Test Army')).toEqual(['The Test Army has landed at Rangoon, formed from two of your corps, now that the 6th Army (7th Division) has left the front.']);
  });
});

describe('section 5 and case 7: a row with men of its own whose corps are at the front', () => {
  const south = (): Game => {
    const g = open();
    lose(g, 'Cam Ranh', 'JAP', 'Saigon');
    month(g, 27); g.turn = 28;
    for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
    lose(g, 'Rangoon', 'JAP', 'Calcutta');
    return g;
  };

  it('two corps in one province: the place once', () => {
    const g = south();
    addRow([31, 'JAP', 'Test Division', 'inf', 1, 'Rangoon', { from: [SAKAGUCHI, T8], adds: 0.67 }]);
    relocate(g, corps(g, SAKAGUCHI), g.byName['Kaifeng']);
    relocate(g, corps(g, T8), g.byName['Kaifeng']);
    month(g, 31);
    expect(told(g, 31, 'Test Division')).toEqual(['The Test Division has landed at Rangoon without your Sakaguchi Detachment (56th Mixed Infantry Group) and your 8th Tank Regiment, which are at the front in Kaifeng and stay corps of their own.']);
  });

  it('two corps in two provinces: "in {X} and {Y}"', () => {
    const g = south();
    addRow([31, 'JAP', 'Test Division', 'inf', 1, 'Rangoon', { from: [SAKAGUCHI, T12], adds: 0.67 }]);
    relocate(g, corps(g, SAKAGUCHI), g.byName['Kaifeng']);
    relocate(g, corps(g, T12), g.byName['Linfen']);
    month(g, 31);
    expect(told(g, 31, 'Test Division')).toEqual(['The Test Division has landed at Rangoon without your Sakaguchi Detachment (56th Mixed Infantry Group) and your 12th Tank Regiment, which are at the front in Kaifeng and Linfen and stay corps of their own.']);
  });

  it('three corps (case 4), two of them in one province: every corps named, each place once', () => {
    const g = south();
    addRow([31, 'JAP', 'Test Division', 'inf', 1, 'Rangoon', { from: [SAKAGUCHI, T8, T12], adds: 0.67 }]);
    relocate(g, corps(g, SAKAGUCHI), g.byName['Kaifeng']);
    relocate(g, corps(g, T8), g.byName['Kaifeng']);
    relocate(g, corps(g, T12), g.byName['Linfen']);
    month(g, 31);
    expect(told(g, 31, 'Test Division')).toEqual(['The Test Division has landed at Rangoon without your Sakaguchi Detachment (56th Mixed Infantry Group), your 8th Tank Regiment and your 12th Tank Regiment, which are at the front in Kaifeng and Linfen and stay corps of their own.']);
  });
});
