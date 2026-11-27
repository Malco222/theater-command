// `rowComes` (game.ts), the chain's reading of a maker's own state (the design director's ruling of 06:10Z, 26 November
// 2026), writes nothing: it is read by the far side's chain while the rule runs, and by the telling, and a read that
// wrote a flag (a book's month, a row's due month) would move the war it reads. Two wars nobody plays, the far side on,
// the switch 'fromWait' on, their first MONTHS months each: before every month, every dated row read for its own country
// leaves the flags and the whole save as they were. And at the end of every month (section 13 A5 of the design): a row
// sent or struck reads 'ended', and a row that reads 'ended' is sent or struck, or its months are out, or (the far side)
// its war is over, or its country was overrun before its call-up was counted.
//
// (Why MONTHS and not the 24 these wars once ran: measured by the builder of 27 November 2026 at a load of 8, the war's
// own months, `runTurnAI`, took 34.9 s of seed 17's 36.2 s; the two whole saves a month 0.7 s, the flags 0.01 s, and
// `rowComes` read for every nation and every row before the month and for every row after it 0.17 s. So only fewer
// months make it fast; every check is kept every month. In the first months every kind of reading is seen ('notYet',
// 'due' read before months 1 and 2, 'ended') and rows are sent and read 'ended' (one after month 1, five after month 2).
// Lost with months 5 to 24: the spring 1940 rows (some forty sent in months 9 to 11), the rows whose months run out
// (`until`, from month 12) and seed 17's one row struck at its half year (month 20). In neither 24-month war did any row
// read 'ended' for a cause other than being sent or struck.)
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { setFromWait } from '../src/campaign/arrivalwaits';
import { createGame, farRow, rowComes, rowWords, warOver } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';

const rows = (setup as any).expeditions as any[][];
const MONTHS = 4;

/** Section 13 A5: why a row may read 'ended'. */
const endedBecause = (g: Game, nat: string, j: number): boolean => {
  const row = rows[j];
  const { o } = rowWords(g, nat, row);
  const fromWar = g.flags.open === 1 && !!o.war && o.after !== undefined;
  return !!g.flags[`sent:${j}`] || (!!o.until && g.turn >= o.until) || (!!o.war && farRow(g, nat, row) && warOver(g, j, o.war))
    || (fromWar && g.flags[`due:${j}`] === undefined && !!g.nations[nat].fellOn);
};

describe('rowComes keeps no month a row is first due', () => {
  it("the 12th Army (62nd Division)'s row, its war on and its date past, never yet read: due, and no `due:` kept", () => {
    const g = startOpen(createGame(34, null));
    setFromWait(g, true);
    const j = rows.findIndex((r) => r[1] === 'JAP' && r[2] === '12th Army (62nd Division)');
    expect(rows[j][6].war).toEqual(['JAP', 'CHN']);
    g.turn = 60;
    expect(g.flags[`due:${j}`]).toBeUndefined();
    const flags = JSON.stringify(g.flags), save = saveGame(g);
    expect(rowComes(g, 'JAP', j)).toBe('due');
    expect(g.flags[`due:${j}`]).toBeUndefined();
    expect(JSON.stringify(g.flags)).toBe(flags);
    expect(saveGame(g) === save).toBe(true);
  });
});

describe('rowComes writes nothing, and reads ended only where a row has ended', () => {
  for (const seed of [17, 34]) {
    it(`seed ${seed}, ${MONTHS} months`, () => {
      const g = startOpen(createGame(seed, null));
      expect(setFromWait(g, true)).toBe(true);
      const seen = { notYet: 0, due: 0, ended: 0 };
      let sentRead = 0;
      for (let m = 0; m < MONTHS; m++) {
        const flags = JSON.stringify(g.flags), save = saveGame(g);
        for (const nat of Object.keys(g.nations)) rows.forEach((_, j) => { seen[rowComes(g, nat, j)]++; });
        expect(JSON.stringify(g.flags), `month ${g.turn}: flags`).toBe(flags);
        expect(saveGame(g) === save, `month ${g.turn}: the whole save`).toBe(true);
        runTurnAI(g);
        rows.forEach((row, j) => {
          const nat = row[1] as string;
          if (!g.nations[nat]) return;
          const v = rowComes(g, nat, j);
          if (g.flags[`sent:${j}`]) { expect(v, `month ${g.turn}, row ${j} ${row[2]}: sent`).toBe('ended'); sentRead++; }
          if (v === 'ended') expect(endedBecause(g, nat, j), `month ${g.turn}, row ${j} ${row[2]}: ended`).toBe(true);
        });
      }
      // (Not an empty reading: rows of every kind were read.)
      expect(seen.notYet).toBeGreaterThan(0);
      expect(seen.due).toBeGreaterThan(0);
      expect(seen.ended).toBeGreaterThan(0);
      // (And the rows that end in these months still end: rows sent in the war were read at its months' ends.)
      expect(sentRead).toBeGreaterThan(0);
    });
  }
});
