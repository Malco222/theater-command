// An island's room in men, round 2's mends: the mended sentences as a player reads them on live boards through the 3D
// game's bridge (TC), each asserted whole (words sceptic 2's findings on head 4c863d9c4).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, createGame, declareWar, placed, removeCorps } from '../src/campaign/game';
import { fits } from '../src/campaign/islands';
import { saveGame } from '../src/campaign/save';
import { garrison } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => any;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
/** A whole-world war with a player, at his first month (seed 3). */
function war(human: string): Game {
  const g = startOpen(createGame(3, human));
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  beginHuman(g);
  return g;
}
/** A corps of infantry put down for the test, with its month in hand. */
function corps(g: Game, nat: string, name: string, at: Region, divs = 1, str = 1): Corps {
  const c = addCorps(g, nat, name, 'inf', divs, at.id, {});
  c.str = str;
  c.ap = c.apMax;
  return c;
}
const clear = (g: Game, r: Region): void => { for (const u of [...r.units, ...(r.garr ?? [])]) removeCorps(g, u); };
/** What the board printed, for the report: one line a sentence. */
const say = (board: string, line: string): void => { (globalThis as unknown as { process: { stderr: { write: (s: string) => void } } }).process.stderr.write(`\nLIVE ${board} | ${line}`); };
describe("round 2's mended sentences on live boards through the bridge", () => {
  it('W2-4: the Defense Battalion of 9,400 taking the field on Wake beside 6,000 Marines reads its card\'s figure', () => {
    const g = war('USA');
    const wake = g.byName['Wake'];
    clear(g, wake);
    const inside = corps(g, 'USA', 'Defense Battalion', wake, 1, 9400 / 15000);
    expect(garrison(g, inside)).toBe(true);
    corps(g, 'USA', 'Marines', wake, 1, 0.4);
    TC().load(saveGame(g));
    const w = TC().work(`field:${inside.id}`, wake.id) as { ok: boolean; note: string };
    say('W2-4 take the field', w.note);
    expect(w.ok).toBe(false);
    expect(w.note).toBe('Wake has room for one division, about 15,000 men, and 6,000 of yours are there: only 9,000 more can go, and the Defense Battalion has 9,400.');
  }, 300000);
});
