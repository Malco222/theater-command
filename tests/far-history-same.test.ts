// The far side's switch changes nothing in history's campaigns.
//
// The far side's rows are laid over the game's data files as the modules load (far-rows.ts), so every game reads
// them: the grand campaign they were written for, and history's campaigns too. A row that a rule reads without asking
// which game it is in would change a campaign that was measured long ago. One did: ruling 221's refuge (troops
// lifted off a coast land at the nearest harbour their country holds there) moved four of the 1939 campaign's first
// 24 wars from May 1941 on, none of them among the four seeds the history hash plays.
//
// This file plays the 1939 campaign twice in one run, with the switch off and with it on, on two of those seeds, and
// holds the two to be the same month by month. It loads the game afresh for each (vi.resetModules), because the
// switch is read once, as the modules load.
import { afterAll, describe, expect, it, vi } from 'vitest';

const G = globalThis as { TC_FAR?: unknown };
const before = G.TC_FAR;
afterAll(() => { if (before === undefined) delete G.TC_FAR; else G.TC_FAR = before; });

async function play(far: boolean, seed: number, last: number): Promise<string[]> {
  vi.resetModules();
  G.TC_FAR = far ? 1 : 0;
  const { FAR_STAGE } = await import('../src/campaign/stage');
  expect(FAR_STAGE).toBe(far);
  const { createGame } = await import('../src/campaign/game');
  const { runTurnAI } = await import('../src/campaign/turn');
  const g = createGame(seed, null);
  const months: string[] = [];
  for (let t = 1; t < last; t++) {
    runTurnAI(g);
    months.push([
      g.corps.map((c) => `${c.nat}|${c.name}|${c.reg}|${c.str.toFixed(4)}|${c.org.toFixed(4)}`).join(';'),
      g.regions.map((r) => r.ctrl).join(''),
      g.fleets.map((f) => `${f.nat}|${f.name}|${JSON.stringify(f.ships)}`).join(';'),
      g.reports.length,
    ].join('#'));
  }
  return months;
}

describe("history's campaigns with the far side's switch on", () => {
  // Seeds 5 and 14 parted from their own history in May 1941 (the 21st month) while the refuge was read by every game.
  for (const seed of [5, 14]) {
    it(`plays the 1939 campaign month for month as it does with the switch off (seed ${seed}, to June 1941)`, async () => {
      const off = await play(false, seed, 23);
      const on = await play(true, seed, 23);
      const first = off.findIndex((m, i) => m !== on[i]);
      expect(first < 0 ? 'the same' : `parts in month ${first + 2}`).toBe('the same');
    }, 240_000);
  }
});
