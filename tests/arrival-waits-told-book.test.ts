// The closed book and the telling together, the far side (arrivalwaits.ts; history.ts `contactRuleActs`). At the
// dial's stops 1 and 2 the contact rule acts where the row's book is closed (the design director's 22:05Z of 25
// November 2026, "a closed book brings the contact rule in"; far-side ground reads the Pacific's book, her 22:12Z),
// and the UI director's words read the same there as at stops 3 to 5 (the marks' preamble: "The telling reads the
// same at a book stop after the closing"). With the Pacific's book closed the player's row waits AND is told: the
// headline the month the wait begins (section 1), the standing line on the corps (section 2), and the month card's
// line for a row that waits on another row (section 2). With it open the row takes at its date, nothing is told and
// none of the telling's flags is written. The book is closed by hand, as the other tests close one.
//
// The board is tests/arrival-waits-told's: seed 3, Japan the player's, the 21st Army (18th Division) in the line at
// Tungkun, so the 25th Army (18th Division)'s row for Hainan (month 27) waits on it, and the 18th Division's row for
// Rangoon (month 32) waits on that row.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { arrivalWaitOf, rowsWaitingOnRows } from '../src/campaign/arrivalwaits';
import { createGame, declareWar, relocate, startPhase } from '../src/campaign/game';
import { setStop, stopDef } from '../src/campaign/history';
import { markFor, monthCard } from '../src/campaign/monthcard';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';

/** The UI director's sentences these tests pin, in one place (her section 7 may change them). */
const WORDS = {
  begins: 'The 25th Army (18th Division) is due in Hainan this month, formed from your 21st Army (18th Division), which is at the front in Tungkun. The arrival waits, and forms the month after that corps leaves the front.',
  standing: 'An arrival due in Hainan, the 25th Army (18th Division), is to be formed from this corps, and has waited for it since November 1941. It forms the month after this corps leaves the front.',
  chain: 'The 18th Division is due in Rangoon this month, formed from the 25th Army (18th Division), which has not formed yet. The arrival waits for it.',
  card: 'The 18th Division, due in Rangoon, has waited since April 1942 for the 25th Army (18th Division), which has not formed yet.',
};

const ARMY21 = '21st Army (18th Division)', ARMY25 = '25th Army (18th Division)', D18 = '18th Division';
const rows = (setup as any).expeditions as any[][];
const index = (name: string): number => rows.findIndex((r) => r[1] === 'JAP' && r[2] === name);
const corps = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead);
/**
 * (The contact rule's second form, the design director's ruling of 06:10Z, 26 November 2026: a corps beside the enemy
 * is held while it took part in the battle last resolved on its ground, or where nobody of its side keeps its province.
 * This board's corps at the front stand with others of their side, so each is set, by hand, as having fought the battle
 * last resolved where it stands, and is held as the board means. The relieved case: tests/contact-fact.test.ts.)
 */
const fought = (g: Game): void => { if (g.flags.fromWait !== 1) return; g.lastBattle = {}; for (const c of g.corps) if (!c.dead) (g.lastBattle[c.reg] ??= []).push(c.id); };
const month = (g: Game, t: number): void => { fought(g); g.turn = t; startPhase(g, 'JAP'); };
const lose = (g: Game, name: string, to: string, away: string): Region => { const r = g.byName[name]; for (const u of [...r.units]) relocate(g, u, g.byName[away]); r.ctrl = to; return r; };
const tellings = (g: Game): string[] => g.headlines.filter((h) => /formed from|left the front|has not formed yet|without your|The arrival/.test(h.text)).map((h) => h.text);
const telFlags = (g: Game): string[] => Object.keys(g.flags).filter((k) => /^row(Told|At|Front):/.test(k));

/** Seed 3, Japan the player's, at a stop that keeps a book, the switch on; the Pacific's book closed or open. */
const war = (stop: number, closed: boolean): Game => {
  const g = startOpen(createGame(3, 'JAP'));
  expect(setStop(g, stop, true)).toBe(true);
  expect(stopDef(g).book).toBe(true);
  g.flags.fromWait = 1;
  if (closed) g.flags['hist:off:pacific'] = 20;
  lose(g, 'Cam Ranh', 'JAP', 'Saigon');
  month(g, 27);
  g.turn = 28; g.flags['st:done:JAP:south'] = 28;
  for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
  lose(g, 'Rangoon', 'JAP', 'Calcutta');
  for (let t = 29; t <= 33; t++) month(g, t);
  return g;
};

describe("the far side at a book stop: the Pacific's book closed, the player's row waits and is told", () => {
  it.each([1, 2])('at stop %i: the headline, the corps\' standing line, and the month card\'s line for the row that waits on a row', (stop) => {
    const g = war(stop, true);
    const i = index(ARMY25), j = index(D18);
    expect(g.flags[`wait:${i}`]).toBe(33);
    expect(g.flags[`wait:${j}`]).toBe(33);
    expect(corps(g, ARMY25)).toBeUndefined();
    // (At a book stop more of the book's rows of Japan's are due than at stop 3, and they wait and are told as well.)
    expect(tellings(g)).toEqual(expect.arrayContaining([WORDS.begins, WORDS.chain]));
    expect(tellings(g).filter((x) => x === WORDS.begins || x === WORDS.chain)).toHaveLength(2);
    expect(g.headlines.find((h) => h.text === WORDS.begins)?.turn).toBe(27);
    expect(arrivalWaitOf(g, corps(g, ARMY21)!)).toEqual({ short: 'An arrival waits for it', whole: WORDS.standing });
    expect(rowsWaitingOnRows(g, 'JAP')).toEqual([WORDS.card]);
    expect(monthCard(g, markFor(g), 'JAP').waits).toEqual([WORDS.card]);
    expect([g.flags[`rowTold:${i}`], g.flags[`rowTold:${j}`]]).toEqual([27, 32]);
  });
});

describe("the far side at a book stop: the Pacific's book open, the row takes at its date and nothing is told", () => {
  it.each([1, 2])('at stop %i: the row takes its corps out of the line in its month; no line, no flag of the telling', (stop) => {
    const g = war(stop, false);
    const i = index(ARMY25), j = index(D18);
    expect(g.flags['hist:off:pacific']).toBeUndefined();
    expect([g.flags[`wait:${i}`], g.flags[`wait:${j}`]]).toEqual([undefined, undefined]);
    // (The 25th Army (18th Division) formed in month 27 out of the 21st's, and the 18th Division's row took it in month 32.)
    expect([g.flags[`sent:${i}`], g.flags[`sent:${j}`]]).toEqual([1, 1]);
    expect(corps(g, ARMY21)).toBeUndefined();
    expect(g.regions[corps(g, D18)!.reg].name).toBe('Rangoon');
    expect(tellings(g)).toEqual([]);
    expect(telFlags(g)).toEqual([]);
    expect(rowsWaitingOnRows(g, 'JAP')).toEqual([]);
    expect(g.corps.filter((c) => c.nat === 'JAP' && !c.dead).map((c) => arrivalWaitOf(g, c)).filter(Boolean)).toEqual([]);
  });
});
