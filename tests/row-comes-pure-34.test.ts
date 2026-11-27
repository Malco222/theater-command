// (Seed 34's war of tests/row-comes-pure.test.ts, one war a file: R4-4.)
// `rowComes` (game.ts), the chain's reading of a maker's own state (the design director's ruling of 06:10Z, 26 November
// 2026), writes nothing: it is read by the far side's chain while the rule runs, and by the telling, and a read that
// wrote a flag (a book's month, a row's due month) would move the war it reads. Two wars nobody plays, the far side on,
// the switch 'fromWait' on, 24 months each: before every month, every dated row read for its own country leaves the
// flags and the whole save as they were. And at the end of every month (section 13 A5 of the design): a row sent or
// struck reads 'ended', and a row that reads 'ended' is sent or struck, or its months are out, or (the far side) its
// war is over, or its country was overrun before its call-up was counted.
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

describe('rowComes writes nothing, and reads ended only where a row has ended', () => {
  // (The release 2 line's reading of this war, its first MONTHS months, as tests/row-comes-pure.test.ts gives it for seed
  // 17 and says why; kept beside R4-4's steps at the join of 27 November 2026.)
  for (const seed of [34]) {
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
  // (One war a file, R4-4: the two wars of this test, seeds 17 and 34, were one file and passed vitest's 30 s only on a
  // quiet box; seed 17 is tests/row-comes-pure.test.ts. The war's 24 months are read in four steps of six, one test each, on the
  // same game, so that no step runs alone against the whole war's time: the same reads and the same assertions.)
  const seed = 34;
  let g: Game;
  const seen = { notYet: 0, due: 0, ended: 0 };
  const step = (): void => {
    for (let m = 0; m < 6; m++) {
      const flags = JSON.stringify(g.flags), save = saveGame(g);
      for (const nat of Object.keys(g.nations)) rows.forEach((_, j) => { seen[rowComes(g, nat, j)]++; });
      expect(JSON.stringify(g.flags), `month ${g.turn}: flags`).toBe(flags);
      expect(saveGame(g) === save, `month ${g.turn}: the whole save`).toBe(true);
      runTurnAI(g);
      rows.forEach((row, j) => {
        const nat = row[1] as string;
        if (!g.nations[nat]) return;
        const v = rowComes(g, nat, j);
        if (g.flags[`sent:${j}`]) expect(v, `month ${g.turn}, row ${j} ${row[2]}: sent`).toBe('ended');
        if (v === 'ended') expect(endedBecause(g, nat, j), `month ${g.turn}, row ${j} ${row[2]}: ended`).toBe(true);
      });
    }
  };
  it(`seed ${seed}, 24 months: months 1 to 6`, () => {
    g = startOpen(createGame(seed, null));
    expect(setFromWait(g, true)).toBe(true);
    step();
  });
  it(`seed ${seed}, 24 months: months 7 to 12`, () => { step(); });
  it(`seed ${seed}, 24 months: months 13 to 18`, () => { step(); });
  it(`seed ${seed}, 24 months: months 19 to 24`, () => {
    step();
    // (Not an empty reading: rows of every kind were read.)
    expect(seen.notYet).toBeGreaterThan(0);
    expect(seen.due).toBeGreaterThan(0);
    expect(seen.ended).toBeGreaterThan(0);
  });
});
