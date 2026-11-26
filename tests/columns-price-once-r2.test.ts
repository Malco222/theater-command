// The supply columns, price once (release 2; the design director, 18:20Z 26 November 2026, "18:20Z Pacific, THE ONE
// LINE OF TRACE"). The columns used to charge the treasury 0.01 a lorry and 0.004 a wagon lost on the road each month,
// outside income() and upkeepTotal(), so the money card, a staff's planning and the month's result read three
// treasuries, and the charge could take a treasury below nothing. But a nation's lorries are not a stock that losses
// draw down: they are counted afresh every month (40 a division a province of road, or with the lorry orders on 55 a
// point of income times the fuel it has), so the next month's count has already made good what was burned. The charge
// was a second price for the same loss, and it is gone: a loss costs the front what it carried, once.
import { describe, expect, it } from 'vitest';
import columnsData from '../data/campaign/columns.json';
import logisticsData from '../data/campaign/logistics.json';
import { columnsMonth, setColumns } from '../src/campaign/columns';
import { createGame } from '../src/campaign/game';
import { lorryPool, setLogistics } from '../src/campaign/logistics';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

/** A grand campaign from September 1939, nobody at the table, the columns on with the lorry orders and the wagons. */
function war(seed: number): Game {
  const g = startOpen(createGame(seed, null));
  expect(setColumns(g, true)).toBe(true);
  expect(setLogistics(g, 'lorryShift', true)).toBe(true);
  expect(setLogistics(g, 'wagons', true)).toBe(true);
  return g;
}

/** Plays whole months, every nation in turn. */
function months(g: Game, n: number): void {
  const end = g.turn + n;
  let guard = 0;
  while (g.turn < end && guard++ < 4000) stepAI(g);
}

/** The nations whose columns lose lorries or wagons when their month's columns are sent again now. */
function bleeding(g: Game): string[] {
  const out: string[] = [];
  for (const nat of Object.keys(g.nations)) {
    const t = loadGame(saveGame(g))!;
    columnsMonth(t, nat);
    if ((t.columns?.[nat] ?? []).some((r) => r.lost > 0)) out.push(nat);
  }
  return out;
}

describe('the supply columns: price once', () => {
  it('no price for a lost lorry or wagon is left in the data', () => {
    expect((columnsData as { lorries: Record<string, number> }).lorries).toEqual({ perDiv: 40 });
    expect('pp' in (logisticsData as { wagons: Record<string, unknown> }).wagons).toBe(false);
  });

  it("a month's columns never touch the treasury, however many are lost, and an empty treasury stays at nothing", () => {
    const g = war(1);
    let hit: string[] = [];
    for (let m = 0; m < 6 && !hit.length; m++) { months(g, 1); hit = bleeding(g); }
    expect(hit.length).toBeGreaterThan(0);
    for (const nat of hit) for (const pp of [g.nations[nat].pp, 0]) {
      const t = loadGame(saveGame(g))!;
      t.nations[nat].pp = pp;
      columnsMonth(t, nat);
      const runs = t.columns?.[nat] ?? [];
      expect(runs.reduce((s, r) => s + r.lost, 0)).toBeGreaterThan(0);
      expect(t.nations[nat].pp).toBe(pp);
    }
  }, 240_000);

  it("the next month's lorries are counted afresh: what was lost last month takes nothing from them", () => {
    const g = war(1);
    let hit: string[] = [];
    for (let m = 0; m < 6 && !hit.length; m++) { months(g, 1); hit = bleeding(g); }
    const nat = hit[0];
    // The same month sent twice: once after last month's columns as they were, once as if every lorry and wagon of
    // last month had been lost on the road. The pool, every column's lorries and wagons, and the treasury agree.
    const kept = loadGame(saveGame(g))!, burned = loadGame(saveGame(g))!;
    for (const runs of Object.values(burned.columns ?? {})) for (const r of runs) { r.lost = r.lorries + (r.horse ?? 0); r.share = 0; }
    expect(lorryPool(burned, nat)).toBe(lorryPool(kept, nat));
    columnsMonth(kept, nat);
    columnsMonth(burned, nat);
    const sent = (t: Game) => (t.columns?.[nat] ?? []).map((r) => [r.path.join('-'), r.need ?? r.lorries, r.lorries, r.horse ?? 0]);
    expect(sent(burned)).toEqual(sent(kept));
    expect(burned.nations[nat].pp).toBe(kept.nations[nat].pp);
  }, 240_000);
});
