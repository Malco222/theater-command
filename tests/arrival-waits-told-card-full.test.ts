// The month card's waits line when the At home box is full, in both cards: the browser's (ui/monthview.ts
// `monthCardHtml`) and the bridge's (campaign-bridge.ts `shownCard`, read through TC.page('reports')). One rule for
// both: the line of a row that waits only for another row to form stands first among the home lines, after the men's
// line and before deliveries, so it is never cut, and it is never counted in the foot's "N more lines of news are in
// the Newsreel" (it is not Newsreel news). The UI director's section 8 point 4 (02:10Z, 26 November 2026): "first among
// the home lines, after the men's line, never cut and never counted." The refuter's cases-5.
//
// The waits line is a real one: tests/arrival-waits-told's board (seed 3, the far side on, Japan the player's; the
// 18th Division's row for Rangoon waits behind the 25th Army (18th Division)'s row for Hainan). The rest of the card is
// set by hand around it (`pad`), in the rules' card the bridge reads too.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
vi.mock('../src/campaign/monthcard', async (orig) => {
  const m = await orig<typeof import('../src/campaign/monthcard')>();
  return { ...m, monthCard: (...a: Parameters<typeof m.monthCard>) => { const c = m.monthCard(...a); const pad = (globalThis as any).PAD_CARD; return pad ? pad(c) : c; } };
});

import { afterEach, describe, expect, it } from 'vitest';
import { createGame, declareWar, relocate, startPhase } from '../src/campaign/game';
import { markFor, monthCard, type MonthCard } from '../src/campaign/monthcard';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { monthCardHtml } from '../src/campaign/ui/monthview';
import type { Game, Region } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
type Home = { text: string; grey: boolean; tip: string };
const LINE = 'The 18th Division, due in Rangoon, has waited since April 1942 for the 25th Army (18th Division), which has not formed yet.';

const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'JAP'); };
const lose = (g: Game, name: string, to: string, away: string): Region => { const r = g.byName[name]; for (const u of [...r.units]) relocate(g, u, g.byName[away]); r.ctrl = to; return r; };
const war = (): Game => {
  const g = startOpen(createGame(3, 'JAP'));
  g.flags.fromWait = 1;
  lose(g, 'Cam Ranh', 'JAP', 'Saigon');
  month(g, 27);
  g.turn = 28; g.flags['st:done:JAP:south'] = 28;
  for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
  lose(g, 'Rangoon', 'JAP', 'Calcutta');
  for (let t = 29; t <= 33; t++) month(g, t);
  return g;
};
/** No lead story (five lines to a box), nothing in the other boxes, no letters: `research` research lines, and `waits` copies of the real waits line. */
const padWith = (research: number, waits: number) => (c: MonthCard): MonthCard => {
  expect(c.waits).toEqual([LINE]);
  return {
    ...c, lead: null, rows: [], show: 0, more: 0, letters: [], staff: [], elsewhere: [], home: [], also: [], alsoFull: [], menOut: undefined,
    research: Array.from({ length: research }, (_, k) => `Test research ${k + 1}`),
    waits: Array.from({ length: waits }, () => LINE),
  };
};
afterEach(() => { delete (globalThis as any).PAD_CARD; });

const esc = (s: string): string => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
const browser = (c: MonthCard): { shown: number; foot: string } => {
  const html = monthCardHtml(c, { esc, flag: () => '', name: (x: string) => x, all: false, orders: true } as any);
  const box = /<div class="mc-lab">At home<\/div>([\s\S]*?)<\/div>\s*<div class="mc-box">/.exec(html)?.[1] ?? '';
  return { shown: box.split(esc(LINE)).length - 1, foot: /(\d+ more lines? of news (is|are) in the Newsreel\.|Every figure comes from the month's own battle reports\.)/.exec(html)?.[0] ?? '' };
};
const bridge = (g: Game): { home: Home[]; rest: string } => {
  g.phase = g.order.indexOf('JAP');
  TC().load(saveGame(g));
  expect((TC().openingCard() as { ok: boolean }).ok).toBe(true);
  return (TC().page('reports') as { month: { home: Home[]; rest: string } }).month;
};

describe('the browser card', () => {
  it('six home lines and one waits line: the waits line stands first and shows; the foot counts the two research lines over', () => {
    const g = war();
    const c = padWith(6, 1)(monthCard(g, markFor(g), 'JAP'));
    expect(browser(c)).toEqual({ shown: 1, foot: '2 more lines of news are in the Newsreel.' });
  });
  it('more waits lines than the box holds: every one shows, and none is counted as news', () => {
    const g = war();
    const c = padWith(2, 6)(monthCard(g, markFor(g), 'JAP'));
    expect(browser(c)).toEqual({ shown: 6, foot: '1 more line of news is in the Newsreel.' });
  });
});

describe("the bridge's card, through TC.page('reports')", () => {
  it('six home lines and one waits line: the waits line stands first and shows; the foot counts the two research lines over', () => {
    const g = war();
    (globalThis as any).PAD_CARD = padWith(6, 1);
    const m = bridge(g);
    expect(m.home.map((x) => x.text)).toEqual([LINE, 'Research complete: Test research 1', 'Research complete: Test research 2', 'Research complete: Test research 3', 'Research complete: Test research 4']);
    expect(m.home[0]).toEqual(expect.objectContaining({ grey: true, tip: LINE }));
    expect(m.rest).toBe('2 more lines of news are in the Newsreel.');
  });
  it('more waits lines than the box holds: every one shows, and none is counted as news', () => {
    const g = war();
    (globalThis as any).PAD_CARD = padWith(2, 6);
    const m = bridge(g);
    expect(m.home.filter((x) => x.text === LINE)).toHaveLength(6);
    expect(m.rest).toBe('1 more line of news is in the Newsreel.');
  });
});
