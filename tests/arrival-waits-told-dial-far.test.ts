// The dial turned in mid-wait, the far side (arrivalwaits.ts `waitedOn`, `waiting`; game.ts `expeditions`). The board
// is tests/arrival-waits-told's: seed 3, Japan the player's, at stop 3 the 25th Army (18th Division)'s row for Hainan
// waits and is told while the 21st Army (18th Division) is in the line at Tungkun, and the 18th Division's row for
// Rangoon waits on that row. In month 33 the dial is turned to stop 1 with the Pacific's book open: the contact rule no
// longer acts there (history.ts `contactRuleActs`), so neither card speaks for the two rows, and in month 34 the Hainan
// row forms with today's line, not section 4's "now that it has left the front" (the 21st was in the line when it was
// taken). The marks' preamble: the telling speaks "only where the rule acts". The refuter's cases-1 and interplay-1.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { arrivalWaitOf, rowsWaitingOnRows } from '../src/campaign/arrivalwaits';
import { corpsInContact } from '../src/campaign/drawn';
import { createGame, declareWar, relocate, startPhase } from '../src/campaign/game';
import { contactRuleActs, setStop, stopOf } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';

const WORDS = {
  standing: 'An arrival due in Hainan, the 25th Army (18th Division), is to be formed from this corps, and has waited for it since November 1941. It forms the month after this corps leaves the front.',
  card: 'The 18th Division, due in Rangoon, has waited since April 1942 for the 25th Army (18th Division), which has not formed yet.',
};

const ARMY21 = '21st Army (18th Division)', ARMY25 = '25th Army (18th Division)';
const rows = (setup as any).expeditions as any[][];
const index = (name: string): number => rows.findIndex((r) => r[1] === 'JAP' && r[2] === name);
const corps = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead);
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'JAP'); };
const lose = (g: Game, name: string, to: string, away: string): Region => { const r = g.byName[name]; for (const u of [...r.units]) relocate(g, u, g.byName[away]); r.ctrl = to; return r; };

describe("the dial turned from stop 3 to stop 1 while Japan's rows wait and are told, the Pacific's book open", () => {
  it('the cards go quiet at once, and the Hainan row forms next month without "now that it has left the front"', () => {
    const g = startOpen(createGame(3, 'JAP'));
    expect(stopOf(g)).toBe(3);
    g.flags.fromWait = 1;
    lose(g, 'Cam Ranh', 'JAP', 'Saigon');
    month(g, 27);
    g.turn = 28; g.flags['st:done:JAP:south'] = 28;
    for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
    lose(g, 'Rangoon', 'JAP', 'Calcutta');
    for (let t = 29; t <= 33; t++) month(g, t);
    const i = index(ARMY25), a21 = corps(g, ARMY21)!;
    expect(g.flags[`wait:${i}`]).toBe(33);
    expect(arrivalWaitOf(g, a21)?.whole).toBe(WORDS.standing);
    expect(rowsWaitingOnRows(g, 'JAP')).toEqual([WORDS.card]);
    expect(setStop(g, 1)).toBe(true);
    expect(g.flags['hist:off:pacific']).toBeUndefined();
    expect(contactRuleActs(g, g.byName.Hainan)).toBe(false);
    expect(corpsInContact(g, 'JAP', a21)).toBe(true);
    // (The rule no longer holds the rows: neither card speaks for them.)
    expect(arrivalWaitOf(g, a21)).toBeNull();
    expect(rowsWaitingOnRows(g, 'JAP')).toEqual([]);
    const h0 = g.headlines.length;
    month(g, 34);
    expect(g.flags[`sent:${i}`]).toBe(1);
    const t34 = g.headlines.slice(h0).filter((h) => h.nat === 'JAP' && /Hainan/.test(h.text)).map((h) => h.text);
    expect(t34).toEqual(['25th Army (18th Division) has landed at Hainan.']);
    expect(g.headlines.slice(h0).filter((h) => /left the front/.test(h.text))).toEqual([]);
  });
});
