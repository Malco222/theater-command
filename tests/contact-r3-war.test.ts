// The contact rule, round 3, on a whole war (the far side on, the switch 'fromWait' on, nobody seated): the design
// director's verdict of 01:37Z, 27 November 2026. "Q1 YES: the telling reads leavingNow." The brief's R3-3: the telling
// and the rule take the SAME leaving set through one function (game.ts `rowLeaving`), so they cannot disagree, and a
// test proves it month by month on a war (seed 20, the plain start, 26 months: it reaches loan ends). Each time the rule builds a row's set (game.ts `leavingWatch`, null in
// play), the telling's own reader (`rowHolds`, as the cards and the headlines call it, given no set) is asked at that
// moment: the set it reads must be the rule's, corps for corps.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { setFromWait } from '../src/campaign/arrivalwaits';
import { createGame, leavingWatch, rowHolds, ruleHolds } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Corps } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';

const rows = (setup as any).expeditions as any[][];
const ids = (s: ReadonlySet<Corps>): number[] => [...s].map((c) => c.id).sort((a, b) => a - b);
afterEach(() => { leavingWatch.tell = null; });

describe("R3-3 and R3-4 on a war: the telling's leaving set is the rule's, and every hold of the rule's own writes its wait", () => {
  it('seed 20, 26 months: at every row the rule reads, rowHolds reads the very set the rule took', () => {
    const g = startOpen(createGame(20, null));
    expect(setFromWait(g, true)).toBe(true);
    let read = 0, withLoans = 0, beyondTakes = 0;
    const months = new Set<number>();
    const bad: string[] = [];
    leavingWatch.tell = (gg, nat, i, set) => {
      read++;
      months.add(gg.turn);
      const from = Array.isArray(rows[i][6]?.from) ? (rows[i][6].from as string[]) : [];
      const told = rowHolds(gg, nat, from, gg.byName[rows[i][5]]?.id, i).leaving;
      if (ids(told).join(',') !== ids(set).join(',')) bad.push(`month ${gg.turn} ${nat} row ${i} ${rows[i][2]}: rule [${ids(set)}] telling [${ids(told)}]`);
      const takes = new Set(gg.corps.filter((x) => x.nat === nat && !x.dead && from.includes(x.name)).map((x) => x.id));
      if ([...set].some((x) => !takes.has(x.id))) beyondTakes++;
      if ([...set].some((x) => rows.some((_, j) => gg.flags[`lent:${j}`] === x.id))) withLoans++;
    };
    // (And R3-4 on the same war: every hold of the rule's own, read each month through the one pure reader, has its wait
    // written and is told where it is the player's; the gate's "untold holds of the rule's own", target 0.)
    const untold: string[] = [];
    const paths: Record<string, number> = {};
    for (let m = 0; m < 26; m++) {
      const month = g.turn;
      runTurnAI(g);
      for (const h of ruleHolds(g, month)) {
        paths[h.path] = (paths[h.path] ?? 0) + 1;
        if (h.untold) untold.push(`month ${month} ${h.nat} row ${h.i} ${h.row}: ${h.path} ${h.corps.join(', ')} written ${h.written} told ${h.told}`);
      }
    }
    expect(bad).toEqual([]);
    expect(untold).toEqual([]);
    console.log(`R3-4 war: holds of the rule's own by path ${JSON.stringify(paths)}`);
    expect(Object.values(paths).reduce((s, n) => s + n, 0)).toBeGreaterThan(0);
    console.log(`R3-3 war: ${read} sets read in ${months.size} months, ${beyondTakes} beyond the row's own corps, ${withLoans} holding a lent corps`);
    expect(read).toBeGreaterThan(20);
    expect(months.size).toBeGreaterThan(10);
    // (The war reaches sets beyond a row's own `from` corps: loan ends this pass, so the check is not empty.)
    expect(beyondTakes).toBeGreaterThan(0);
    expect(withLoans).toBeGreaterThan(0);
  },
  // TIME LIMIT, not an assertion: the 900 s guards a hung run and proves nothing about the rule (the design director, 17:08Z,
  // 27 November 2026, the naval pattern). Measured, this file run ALONE (one file, --maxWorkers=1, nice -n 12, bash
  // `time` around the one vitest command), 27 November 2026 at head 441b52968, 4 cores shared with other teams:
  //   18:22Z, load 7.34: 54.1 s wall, 53.1 s user + 1.0 s sys = 54.0 s CPU;
  //   18:23Z, load 7.78: 57.5 s wall, 57.4 s user + 1.0 s sys = 58.4 s CPU.
  // (Round 3's verifier read 58 s wall at a load of 6 to 8.) The box is not quiet, so the CPU time is the figure a later
  // run is set against. WATCH LINE: 88 s of CPU (one and a half times today's larger CPU time, 58.4 s). A later quiet run
  // over it is traced (a finding for the thread), never failed, and never a reason to raise the 900 s or to cut the war.
  900_000);
});
