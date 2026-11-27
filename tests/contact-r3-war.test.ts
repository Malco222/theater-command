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
import { createGame, leavingWatch, rowHolds } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Corps } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';

const rows = (setup as any).expeditions as any[][];
const ids = (s: ReadonlySet<Corps>): number[] => [...s].map((c) => c.id).sort((a, b) => a - b);
afterEach(() => { leavingWatch.tell = null; });

describe("R3-3: the telling's leaving set is the rule's, month by month on a war", () => {
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
    for (let m = 0; m < 26; m++) runTurnAI(g);
    expect(bad).toEqual([]);
    console.log(`R3-3 war: ${read} sets read in ${months.size} months, ${beyondTakes} beyond the row's own corps, ${withLoans} holding a lent corps`);
    expect(read).toBeGreaterThan(20);
    expect(months.size).toBeGreaterThan(10);
    // (The war reaches sets beyond a row's own `from` corps: loan ends this pass, so the check is not empty.)
    expect(beyondTakes).toBeGreaterThan(0);
    expect(withLoans).toBeGreaterThan(0);
  }, 900_000);
});
