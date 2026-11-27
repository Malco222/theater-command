// The telling, the far side: "your arrival waits because its corps is at the front" (arrivalwaits.ts). The UI
// director's words of 21:49Z, 25 November 2026 (pacific-japan/ui-words-row-waits-contact-UI-MARKS.md), each form's
// exact sentence against real rows of Japan's, with Japan the player's. The rule itself is game.ts `expeditions`
// (tests/far-rows-contact, far-rows-from-waits): a dated row that takes named corps whole waits, under the switch
// 'fromWait' at the dial's stops 3 to 5, while one of them is at the front, or while the row that makes one waits.
// What is told: the headline once, the month the wait begins (section 1); the standing line for the corps he must move
// and the month card's line for a row that waits only for another row (section 2); the headline the month it forms
// (section 4); the row with men of its own that comes without its corps (section 5). The states are set by hand.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { arrivalWaitOf, rowsWaitingOnRows } from '../src/campaign/arrivalwaits';
import { atWar, createGame, declareWar, relocate, startPhase } from '../src/campaign/game';
import { markFor, monthCard } from '../src/campaign/monthcard';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';

const ARMY21 = '21st Army (18th Division)', ARMY25 = '25th Army (18th Division)', D18 = '18th Division', ARMY7 = '6th Army (7th Division)';
const MONGOLIA = 'Mongolia Garrison Army (3rd Tank Division)', T4 = '4th Tank Regiment', T8 = '8th Tank Regiment', T12 = '12th Tank Regiment', T13 = '13th Tank Regiment';
const D56 = '56th Division', SAKAGUCHI = 'Sakaguchi Detachment (56th Mixed Infantry Group)', CHEKIANG = '13th Army (Chekiang landing force)';
const rows = (setup as any).expeditions as any[][];
const index = (name: string): number => rows.findIndex((r) => r[1] === 'JAP' && r[2] === name);
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
const told = (g: Game, t: number): string[] => g.headlines.filter((h) => h.turn === t && h.nat === 'JAP' && /formed from|left the front|has not formed yet|without your/.test(h.text)).map((h) => h.text);
const lose = (g: Game, name: string, to: string, away: string): Region => { const r = g.byName[name]; for (const u of [...r.units]) relocate(g, u, g.byName[away]); r.ctrl = to; return r; };
/** A grand campaign, Japan the player's, the far side on, the switch on by hand as a campaign opened with it carries it. */
const open = (): Game => { const g = startOpen(createGame(3, 'JAP')); g.flags.fromWait = 1; return g; };
/** Seed 3: the 21st Army's 18th Division in the line at Tungkun; Japan's strike south gone in month 28, Rangoon hers (tests/far-rows-from-waits). */
const war = (): Game => {
  const g = open();
  lose(g, 'Cam Ranh', 'JAP', 'Saigon');
  month(g, 27);
  g.turn = 28; g.flags['st:done:JAP:south'] = 28;
  for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
  lose(g, 'Rangoon', 'JAP', 'Calcutta');
  return g;
};

describe('one corps: the 25th Army (18th Division) for Hainan, from the 21st Army (18th Division) at Tungkun', () => {
  it('the month the wait begins: one headline, and not again while it waits', () => {
    const g = war();
    const i = index(ARMY25);
    expect(told(g, 27)).toContain('The 25th Army (18th Division) is due in Hainan this month, formed from your 21st Army (18th Division), which is at the front in Tungkun. The arrival waits, and forms the month after that corps leaves the front.');
    const h = g.headlines.find((x) => x.text.startsWith('The 25th Army (18th Division) is due in Hainan'))!;
    expect([h.kind, h.nat, h.region]).toEqual(['info', 'JAP', g.byName['Hainan'].id]);
    month(g, 29); month(g, 30); month(g, 31);
    expect(g.flags[`wait:${i}`]).toBe(31);
    expect(g.headlines.filter((x) => x.text.startsWith('The 25th Army (18th Division) is due in'))).toHaveLength(1);
    expect([g.flags[`rowTold:${i}`], g.flags[`rowAt:${i}`], g.flags[`rowFront:${i}`]]).toEqual([27, g.byName['Hainan'].id, 1]);
  });

  it('the corps carries the standing line while it waits; it goes the moment the corps leaves the front', () => {
    const g = war();
    const c = corps(g, ARMY21);
    expect(arrivalWaitOf(g, c)).toEqual({
      short: 'An arrival waits for it',
      whole: 'An arrival due in Hainan, the 25th Army (18th Division), is to be formed from this corps, and has waited for it since November 1941. It forms the month after this corps leaves the front.',
    });
    relocate(g, c, g.byName['Hiroshima']);
    expect(arrivalWaitOf(g, c)).toBeNull();
  });

  it('the month it forms: the formed headline, and the telling\'s flags are cleared', () => {
    const g = war();
    const i = index(ARMY25);
    month(g, 29);
    relocate(g, corps(g, ARMY21), g.byName['Hiroshima']);
    month(g, 30);
    expect(told(g, 30)).toContain('The 25th Army (18th Division) has landed at Hainan, formed from your 21st Army (18th Division), now that it has left the front.');
    expect(g.headlines.filter((x) => x.turn === 30 && x.text.startsWith('25th Army (18th Division) has'))).toHaveLength(0);
    expect(Object.keys(g.flags).filter((k) => k.endsWith(`:${i}`) && k.startsWith('row'))).toEqual([]);
  });
});

describe('the corps it is formed from has not formed yet: the 18th Division at Rangoon behind the 25th Army (18th Division)', () => {
  it('one: the headline, the month card\'s muted line while it waits, and the general\'s line with the second sentence when it forms after waiting on the front', () => {
    const g = war();
    const i = index(D18);
    for (let t = 29; t <= 33; t++) month(g, t);
    expect(told(g, 32)).toEqual(['The 18th Division is due in Rangoon this month, formed from the 25th Army (18th Division), which has not formed yet. The arrival waits for it.']);
    expect(told(g, 33)).toEqual([]);
    expect(g.flags[`rowFront:${i}`]).toBeUndefined();
    const line = 'The 18th Division, due in Rangoon, has waited since April 1942 for the 25th Army (18th Division), which has not formed yet.';
    expect(rowsWaitingOnRows(g, 'JAP')).toContain(line);
    expect(monthCard(g, markFor(g), 'JAP').waits).toContain(line);
    // Its corps comes, at Hainan in the line: the Rangoon row waits on it there, and the corps carries the line now.
    relocate(g, corps(g, ARMY21), g.byName['Hiroshima']);
    lose(g, 'Singapore', 'JAP', 'Calcutta');
    // (It forms there alone, nobody of its side to keep Hainan: the contact rule's second form holds it where it forms.)
    for (const u of [...g.byName['Hainan'].units]) relocate(g, u, g.byName['Hiroshima']);
    // (R4-1: an enemy its own orders bind to its place is not the enemy beside for the alone half; those beside Hainan
    // are set free to come on here, as this board means.)
    for (const j of g.byName['Hainan'].nb) for (const u of g.regions[j].units) if (atWar(g, 'JAP', u.nat)) u.hold = false;
    month(g, 34); month(g, 35);
    const a = corps(g, ARMY25);
    expect(g.regions[a.reg].name).toBe('Hainan');
    expect(rowsWaitingOnRows(g, 'JAP')).not.toContain(line);
    expect(arrivalWaitOf(g, a)?.whole).toBe('An arrival due in Rangoon, the 18th Division, is to be formed from this corps, and has waited for it since April 1942. It forms the month after this corps leaves the front.');
    expect(g.flags[`rowFront:${i}`]).toBe(1);
    // No second headline for the same wait.
    expect(g.headlines.filter((x) => x.text.startsWith('The 18th Division is due in'))).toHaveLength(1);
    relocate(g, a, g.byName['Hiroshima']);
    month(g, 36);
    expect(told(g, 36)).toEqual(['Renya Mutaguchi lands at Rangoon with the 18th Division. The arrival is formed from your 25th Army (18th Division), now that it has left the front.']);
  });

  it('two: "which have not formed yet. The arrival waits for them."', () => {
    const g = war();
    addRow([32, 'JAP', 'Test Army', 'inf', 1, 'Rangoon', { from: [ARMY25, CHEKIANG] }]);
    for (let t = 29; t <= 32; t++) month(g, t);
    expect(told(g, 32)).toContain('The Test Army is due in Rangoon this month, formed from the 25th Army (18th Division) and the 13th Army (Chekiang landing force), which have not formed yet. The arrival waits for them.');
    expect(rowsWaitingOnRows(g, 'JAP')).toContain('The Test Army, due in Rangoon, has waited since April 1942 for the 25th Army (18th Division) and the 13th Army (Chekiang landing force), which have not formed yet.');
  });

  it('both causes at once: one at the front, one not formed yet', () => {
    const g = war();
    addRow([32, 'JAP', 'Test Army', 'inf', 1, 'Rangoon', { from: [ARMY7, ARMY25] }]);
    relocate(g, corps(g, ARMY7), g.byName['Kaifeng']);
    for (let t = 29; t <= 32; t++) month(g, t);
    expect(told(g, 32)).toContain('The Test Army is due in Rangoon this month. The 6th Army (7th Division) is at the front in Kaifeng, and the 25th Army (18th Division), which it is also formed from, has not formed yet, so the arrival waits for both.');
  });
});

describe('several corps: the Mongolia Garrison Army (3rd Tank Division) at Paotow, from four', () => {
  const at40 = (): Game => { const g = open(); return g; };
  it('one at the front: the 13th Tank Regiment at Hankow; and the month it forms', () => {
    const g = at40();
    month(g, 40);
    expect(told(g, 40)).toContain('The Mongolia Garrison Army (3rd Tank Division) is due in Paotow this month, formed from four of your corps. The 13th Tank Regiment is at the front in Hankow, so the arrival waits, and forms the month after that corps leaves the front.');
    expect(arrivalWaitOf(g, corps(g, T13))?.whole).toBe('An arrival due in Paotow, the Mongolia Garrison Army (3rd Tank Division), is to be formed from this corps, and has waited for it since December 1942. It forms the month after this corps leaves the front.');
    expect(arrivalWaitOf(g, corps(g, T8))).toBeNull();
    relocate(g, corps(g, T13), g.byName['Mukden']);
    month(g, 41);
    expect(told(g, 41)).toContain('The Mongolia Garrison Army (3rd Tank Division) has arrived in Paotow, formed from four of your corps, now that the 13th Tank Regiment has left the front.');
  });

  it('two at the front: "after both leave the front", the card names the other, and the formed headline names both', () => {
    const g = at40();
    relocate(g, corps(g, T8), g.byName['Kaifeng']);
    month(g, 40);
    expect(told(g, 40)).toContain('The Mongolia Garrison Army (3rd Tank Division) is due in Paotow this month, formed from four of your corps. The 8th Tank Regiment in Kaifeng and the 13th Tank Regiment in Hankow are at the front, so the arrival waits, and forms the month after both leave the front.');
    expect(arrivalWaitOf(g, corps(g, T13))?.whole).toBe('An arrival due in Paotow, the Mongolia Garrison Army (3rd Tank Division), is to be formed from this corps, and has waited for it since December 1942. It forms the month after this corps and the 8th Tank Regiment leave the front.');
    relocate(g, corps(g, T8), g.byName['Mukden']);
    relocate(g, corps(g, T13), g.byName['Mukden']);
    month(g, 41);
    expect(told(g, 41)).toContain('The Mongolia Garrison Army (3rd Tank Division) has arrived in Paotow, formed from four of your corps, now that the 8th Tank Regiment and the 13th Tank Regiment have left the front.');
  });

  it('three at the front: "after all of them leave the front"', () => {
    const g = at40();
    relocate(g, corps(g, T8), g.byName['Kaifeng']);
    relocate(g, corps(g, T12), g.byName['Linfen']);
    month(g, 40);
    expect(told(g, 40)).toContain('The Mongolia Garrison Army (3rd Tank Division) is due in Paotow this month, formed from four of your corps. The 8th Tank Regiment in Kaifeng, the 12th Tank Regiment in Linfen and the 13th Tank Regiment in Hankow are at the front, so the arrival waits, and forms the month after all of them leave the front.');
  });
});

describe('a row and a corps that share a name: the 4th Tank Regiment for Formosa', () => {
  it('the wait names the corps, and the formed headline says "your corps of that name"', () => {
    const g = open();
    relocate(g, corps(g, T4), g.byName['Amoy']);
    month(g, 27);
    expect(told(g, 27)).toContain('The 4th Tank Regiment is due in Formosa this month, formed from your 4th Tank Regiment, which is at the front in Amoy. The arrival waits, and forms the month after that corps leaves the front.');
    relocate(g, corps(g, T4), g.byName['Tokyo']);
    // (Formosa is full: the 7th Tank Regiment that landed there in month 27 makes room for the row.)
    relocate(g, corps(g, '7th Tank Regiment'), g.byName['Tokyo']);
    month(g, 28);
    expect(told(g, 28)).toContain('The 4th Tank Regiment has landed at Formosa, formed from your corps of that name, now that it has left the front.');
  });
});

describe('a row with men of its own: the 56th Division, the Sakaguchi Detachment at the front', () => {
  it('comes anyway, and its headline says it comes without the corps', () => {
    const g = open();
    lose(g, 'Cam Ranh', 'JAP', 'Saigon');
    month(g, 27);
    g.turn = 28;
    for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
    lose(g, 'Rangoon', 'JAP', 'Calcutta');
    relocate(g, corps(g, SAKAGUCHI), g.byName['Kaifeng']);
    month(g, 31);
    expect(corps(g, D56)).toBeDefined();
    expect(told(g, 31)).toContain('The 56th Division has landed at Rangoon without your Sakaguchi Detachment (56th Mixed Infantry Group), which is at the front in Kaifeng and stays a corps of its own.');
    expect(g.headlines.filter((x) => x.turn === 31 && x.text === '56th Division has landed at Rangoon.')).toHaveLength(0);
    expect(g.flags[`wait:${index(D56)}`]).toBeUndefined();
  });
});
