// A fleet the player keeps by hand on the whole-world chart (the navy thread's read of the far side's join, 24 November
// 2026). Under the admirals (Theatre command) the book's dated rows move a navy's bases: America's Pacific Fleet goes
// from San Diego to Pearl Harbor in May 1940. A fleet the player has ordered himself is his until he hands it back
// (1.5.1, Brian 24 November 18:32Z: "they dont stay where they were moved"), so no book row moves it either.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import { setDesk } from '../src/campaign/depth';
import { createGame } from '../src/campaign/game';
import { baseTargets } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, humanUp, orderRebase, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

function month(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  beginHuman(g);
}
/** America's campaign under the admirals, at the player's month before the book's move (May 1940 is month 9). */
function america(): { g: Game; pf: Fleet } {
  const g = startOpen(createGame(3, 'USA'));
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  setDesk(g, 'navy', true);
  beginHuman(g);
  while (g.turn < 8) month(g);
  return { g, pf: g.fleets.find((f) => f.nat === 'USA' && f.name === 'Pacific Fleet')! };
}
const at = (g: Game, f: Fleet): string => g.regions[f.base].name;

describe("the book's base moves and the player's own orders", () => {
  it('under the admirals, a fleet he has not ordered goes where the book sends it', () => {
    const { g, pf } = america();
    expect(at(g, pf)).toBe('San Diego');
    month(g); month(g);
    expect(at(g, pf)).toBe('Pearl Harbor');
  }, 300000);
  it('a fleet he has moved himself stays where he sent it', () => {
    const { g, pf } = america();
    const to = baseTargets(g, pf).find((r) => r.name === 'San Francisco')!;
    expect(orderRebase(g, pf.id, to.id)).toBe(true);
    month(g); month(g);
    expect(at(g, pf)).toBe('San Francisco');
    expect(g.headlines.some((h) => h.text === 'The Pacific Fleet is ordered to stay at Pearl Harbor.')).toBe(false);
  }, 300000);
});
