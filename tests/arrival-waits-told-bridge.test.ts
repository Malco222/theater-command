// The telling in the 3D's month card (arrivalwaits.ts; the UI director's words of 21:49Z, 25 November 2026, section 2:
// "One set of words for both charts ... in the browser and in 3D"). A dated arrival of the player's that waits only for
// another arrival to form has no corps to carry its line, so the month's report card carries it, muted, every month it
// waits. The rules' card has it as `waits` (monthcard.ts); the bridge's own card (campaign-bridge.ts `shownCard`) hands
// it to the 3D as a grey home line. The board is tests/arrival-waits-told's: seed 3, the far side on, Japan the
// player's; the 18th Division's row for Rangoon waits behind the 25th Army (18th Division)'s row for Hainan, which waits
// while the 21st Army (18th Division) is in the line at Tungkun.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { rowsWaitingOnRows } from '../src/campaign/arrivalwaits';
import { createGame, declareWar, relocate, startPhase } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Game, Region } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
type Home = { text: string; grey: boolean; tip: string };

const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'JAP'); };
const lose = (g: Game, name: string, to: string, away: string): Region => { const r = g.byName[name]; for (const u of [...r.units]) relocate(g, u, g.byName[away]); r.ctrl = to; return r; };
/** Seed 3, Japan's months 27 to 33 alone, as tests/arrival-waits-told plays them; the switch on by hand. */
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

describe("the bridge's month card carries the line of a row that waits for another row to form", () => {
  it('a grey home line, its tip the same words', () => {
    const g = war();
    const line = 'The 18th Division, due in Rangoon, has waited since April 1942 for the 25th Army (18th Division), which has not formed yet.';
    expect(rowsWaitingOnRows(g, 'JAP')).toEqual([line]);
    // (His month: the bridge picks the war up with Japan up next, and lays his card as the reports page shows it.)
    g.phase = g.order.indexOf('JAP');
    TC().load(saveGame(g));
    expect((TC().openingCard() as { ok: boolean }).ok).toBe(true);
    const home = (TC().page('reports') as { month: { home: Home[] } }).month.home;
    expect(home.filter((x) => x.text === line)).toEqual([expect.objectContaining({ text: line, grey: true, tip: line })]);
  });
});
