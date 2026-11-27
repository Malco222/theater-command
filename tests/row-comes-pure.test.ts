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
//
// (What the four months no longer reach, guard (b)'s struck half and the 'until' cause, is read on hand boards with no
// war months, the sceptic's S1 of 27 November 2026: every row of a fresh board, its own war set on by hand, read month by
// month from 0 to 89, reads 'ended' only for one of A5's causes, and rows whose months run out read 'due' and then
// 'ended' in their 'until' month; and a row due seven months back and struck by the month's own start reads 'ended'.)
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { setFromWait } from '../src/campaign/arrivalwaits';
import { createGame, farRow, rowComes, rowWords, startPhase, warOver } from '../src/campaign/game';
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
  for (const seed of [17]) {
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
  // quiet box; seed 34 is tests/row-comes-pure-34.test.ts. The war's 24 months are read in four steps of six, one test each, on the
  // same game, so that no step runs alone against the whole war's time: the same reads and the same assertions.)
  const seed = 17;
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

/** A row's own war set on by hand, both ways, on a board nobody plays. */
const warSet = (g: Game, w: string[] | undefined): void => {
  if (!w || w[1] === '*' || !g.nations[w[0]] || !g.nations[w[1]]) return;
  const [a, b] = w;
  if (!g.nations[a].atWar.includes(b)) g.nations[a].atWar.push(b);
  if (!g.nations[b].atWar.includes(a)) g.nations[b].atWar.push(a);
};

describe("rowComes reads 'ended' where a row is struck or its months are out, on hand boards", () => {
  it("every row, its war on, months 0 to 89: 'ended' only for one of A5's causes, and in its 'until' month", () => {
    const g = startOpen(createGame(34, null));
    expect(setFromWait(g, true)).toBe(true);
    let untilEnds = 0;
    rows.forEach((row, j) => {
      const nat = row[1] as string;
      if (!g.nations[nat]) return;
      const { o } = rowWords(g, nat, row);
      const keep = Object.fromEntries(Object.entries(g.nations).map(([k, n]) => [k, [...n.atWar]]));
      const turn = g.turn;
      warSet(g, o.war);
      const seq: string[] = [];
      for (let t = 0; t < 90; t++) {
        g.turn = t;
        const v = rowComes(g, nat, j);
        seq.push(v);
        if (v === 'ended') expect(endedBecause(g, nat, j), `month ${t}, row ${j} ${row[2]}: ended`).toBe(true);
      }
      if (o.until && seq[o.until - 1] === 'due' && seq[o.until] === 'ended') untilEnds++;
      g.turn = turn;
      for (const [k, w] of Object.entries(keep)) g.nations[k].atWar = w;
    });
    // (Not an empty reading: rows whose months run out were read due and then ended, the Somaliland force among them.)
    expect(untilEnds).toBeGreaterThan(20);
  });

  it("the Amhara command (Gondar), due seven months back, struck at the month's start: sent 2, and read 'ended'", () => {
    const g = startOpen(createGame(34, null));
    expect(setFromWait(g, true)).toBe(true);
    const j = rows.findIndex((r) => r[1] === 'ITA' && r[2] === 'Amhara command (Gondar)');
    const { o } = rowWords(g, 'ITA', rows[j]);
    warSet(g, o.war);
    g.turn = 20;
    expect(o.until).toBeGreaterThan(g.turn);
    expect(rowComes(g, 'ITA', j)).toBe('due');
    g.flags[`due:${j}`] = g.turn - 7;
    startPhase(g, 'ITA');
    expect(g.flags[`sent:${j}`]).toBe(2);
    expect(rowComes(g, 'ITA', j)).toBe('ended');
    expect(endedBecause(g, 'ITA', j)).toBe(true);
  });
});
