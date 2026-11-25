// The book's late rows: a row of the stations whose record runs past the table's last month names its own (`until`),
// and the book still stands that one corps on its stations to that month (the design director's ruling of 07:33Z,
// 22 November 2026, on the trace of seed 19 at the first stop; the historian's paper luzon-leyte-1944-45-2nd-tank-26th.md;
// src/campaign/stations.ts, `lateBinds`).
//
// The 14th Area Army's 2nd Tank Division and 26th Division came to Manila in August 1944 by their dated rows, with
// no hold, and their army's advance walked both off Manila to Legaspi and Leyte in the month they landed. The record:
//   the 2nd Tank Division   Manila from August 1944 (60), Lingayen's plain in January 1945 (65), the mountains of the
//                           north (Aparri) from February (66) to the end (73). Never Leyte.
//   the 26th Division       Manila from August 1944 at about 0.75 (a transport sunk on the way), by sea to Leyte in
//                           November (63) at about 0.65. What the fight on Leyte makes of it is the game's own.
//   the Tsuda Detachment    the regiment the 26th left on Luzon, about 0.125: Manila's ground from November 1944,
//                           the north from February 1945 to the end.
// Nothing else of the table binds past its last month (35): no dated attack and no other row. At the stops that keep
// no book, and with the book shut in the theatre, the rows are the staff's as before. With the far side's switch off
// none of it exists.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, createGame, declareWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import { bookStations, lateStationed, stations, stationsBind } from '../src/campaign/stations';
import setup from '../data/campaign/setup-1939.json';
import type { Corps, Game } from '../src/campaign/types';

const TANKS = '14th Area Army (2nd Tank Division)';
const D26 = '14th Area Army (26th Division)';
const TSUDA = 'Tsuda Detachment (11th Independent Infantry less a battalion, of the 26th Division)';
const LUZON = ['Manila', 'Lingayen', 'Aparri', 'Bataan', 'Legaspi', 'Leyte'];

const row = (name: string) => stations()!.rows.find((r) => r.nat === 'JAP' && r.corps === name)!;
const dated = (name: string): any[] => ((setup as any).expeditions as any[]).find((r) => r[2] === name)!;

/** A book war at stop `stop` in month `turn`: Japan's blow fallen in month `went`, and the Philippines in Japan's hands. */
const war = (turn: number, stop = 1, went = 28): Game => {
  const g = startOpen(createGame(3, null));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = turn;
  g.flags['st:done:JAP:south'] = went;
  for (const e of ['USA', 'UK']) declareWar(g, 'JAP', e);
  for (const nm of LUZON) { const r = g.byName[nm]; r.ctrl = 'JAP'; r.units = []; }
  return g;
};
const put = (g: Game, name: string, place: string, str: number, kind: 'inf' | 'arm' = 'inf'): Corps => { const c = addCorps(g, 'JAP', name, kind, 1, place, { xp: 0.15 }); c.str = str; return c; };
const where = (g: Game, c: Corps): string => g.regions[c.reg].name;
/** The book's month for Japan, month after month: where the corps stands, its worth, and whether the book holds it. */
const months = (g: Game, c: Corps, turns: number[]): [number, string, number, boolean][] => turns.map((t) => {
  g.turn = t;
  bookStations(g, 'JAP');
  return [t, c.dead ? 'gone' : where(g, c), Math.round(c.divs * c.str * 1000) / 1000, c.hold && !!c.stay];
});

describe("the book's late rows (the director's ruling of 07:33Z, 22 November 2026)", () => {
  it("lays the historian's rows for Luzon and Leyte, each with its own last month", () => {
    expect(stations()!.until).toBe(35);
    expect([row(TANKS).steps, row(TANKS).until, row(TANKS).worth ?? null]).toEqual([[[60, ['Manila']], [65, ['Lingayen']], [66, ['Aparri']]], 73, null]);
    expect([row(D26).steps, row(D26).until, row(D26).worth]).toEqual([[[60, ['Manila']], [63, ['Leyte']]], 64, [[60, 0.75], [63, 0.65]]]);
    expect([row(TSUDA).steps, row(TSUDA).until, row(TSUDA).worth ?? null]).toEqual([[[63, ['Manila']], [66, ['Aparri']]], 73, null]);
    for (const nm of [TANKS, D26, TSUDA]) expect([row(nm).free ?? false, row(nm).beside ?? null]).toEqual([false, null]);
    // The rows that bring them: the two divisions' as the world had them, and the detachment's, new, at the stops that keep the book.
    expect([dated(TANKS)[0], dated(TANKS)[5], dated(D26)[0], dated(D26)[5]]).toEqual([60, 'Manila', 60, 'Manila']);
    expect([dated(TSUDA)[0], dated(TSUDA)[1], dated(TSUDA)[5], dated(TSUDA)[6].str, dated(TSUDA)[6].est, dated(TSUDA)[6].stops]).toEqual([63, 'JAP', 'Manila', 0.125, 0.125, [1, 2]]);
    // Every place is a province of the chart.
    const g = war(60);
    for (const nm of [TANKS, D26, TSUDA]) for (const s of row(nm).steps) for (const place of s[1] as string[]) expect(g.byName[place], place).toBeDefined();
  });

  it('past the table\'s last month the table binds nobody, and a late row still binds its own corps to its own last month', () => {
    const g = war(60);
    const tanks = put(g, TANKS, 'Manila', 0.55, 'arm');
    const d26 = put(g, D26, 'Manila', 0.9);
    const other = put(g, 'a division the book names in no late row', 'Manila', 1);
    expect(stationsBind(g, 'JAP')).toBe(false);
    expect([lateStationed(g, tanks), lateStationed(g, d26), lateStationed(g, other)]).toEqual([true, true, false]);
    // Each to its own month and no further.
    g.turn = 64;
    expect([lateStationed(g, tanks), lateStationed(g, d26)]).toEqual([true, true]);
    g.turn = 65;
    expect([lateStationed(g, tanks), lateStationed(g, d26)]).toEqual([true, false]);
    g.turn = 73;
    expect(lateStationed(g, tanks)).toBe(true);
    g.turn = 74;
    expect(lateStationed(g, tanks)).toBe(false);
    // While the table itself runs, its rows are the table's and not late ones.
    g.turn = 35;
    expect([stationsBind(g, 'JAP'), lateStationed(g, tanks)]).toEqual([true, false]);
  });

  it('the 2nd Tank Division: held at Manila from August 1944, Lingayen in January 1945, the north from February, let go after August', () => {
    const g = war(60);
    const c = put(g, TANKS, 'Manila', 0.55, 'arm');
    const other = put(g, 'a division the book names in no late row', 'Manila', 1);
    expect(months(g, c, [60, 61, 64, 65, 66, 70, 73])).toEqual([
      [60, 'Manila', 0.55, true], [61, 'Manila', 0.55, true], [64, 'Manila', 0.55, true],
      [65, 'Lingayen', 0.55, true], [66, 'Aparri', 0.55, true], [70, 'Aparri', 0.55, true], [73, 'Aparri', 0.55, true],
    ]);
    // Nobody else is held by it.
    expect([where(g, other), other.hold]).toEqual(['Manila', false]);
    // The month after its last, it is its staff's again.
    expect(months(g, c, [74])).toEqual([[74, 'Aparri', 0.55, false]]);
    expect(c.bookWorks).toBeUndefined();
  });

  it('the 26th Division: Manila at about 0.75, and by sea to Leyte in November 1944 at about 0.65', () => {
    const g = war(60);
    const c = put(g, D26, 'Manila', 0.9);
    expect(months(g, c, [60, 62, 63, 64])).toEqual([[60, 'Manila', 0.75, true], [62, 'Manila', 0.75, true], [63, 'Leyte', 0.65, true], [64, 'Leyte', 0.65, true]]);
    // Its last month is December 1944: after it the division is its staff's, where it stands.
    expect(months(g, c, [65])).toEqual([[65, 'Leyte', 0.65, false]]);
  });

  it('a ceiling and never a gift, and no move onto ground that is lost', () => {
    const g = war(60);
    // Worn below the record's figure by the game's own fights, it is not raised to it.
    const c = put(g, D26, 'Manila', 0.5);
    expect(months(g, c, [60])).toEqual([[60, 'Manila', 0.5, true]]);
    // Leyte the enemy's in November, with his army ashore: the division stays where it stands.
    g.byName.Leyte.ctrl = 'USA';
    addCorps(g, 'USA', 'an American corps ashore on Leyte', 'inf', 1, 'Leyte', { xp: 0.3 });
    expect(months(g, c, [63])).toEqual([[63, 'Manila', 0.5, true]]);
  });

  it('the Tsuda Detachment: Manila\'s ground from November 1944, the north from February 1945', () => {
    const g = war(63);
    const c = put(g, TSUDA, 'Manila', 0.125);
    expect(months(g, c, [63, 65, 66, 73, 74])).toEqual([[63, 'Manila', 0.125, true], [65, 'Manila', 0.125, true], [66, 'Aparri', 0.125, true], [73, 'Aparri', 0.125, true], [74, 'Aparri', 0.125, false]]);
  });

  it('the late months are the calendar\'s own: they do not slide with the month the blow fell', () => {
    // The blow two months late: the table runs to 37, and the late rows keep 60, 65 and 73.
    const g = war(60, 1, 30);
    const c = put(g, TANKS, 'Manila', 0.55, 'arm');
    expect(months(g, c, [60, 64, 65, 73, 74])).toEqual([[60, 'Manila', 0.55, true], [64, 'Manila', 0.55, true], [65, 'Lingayen', 0.55, true], [73, 'Aparri', 0.55, true], [74, 'Aparri', 0.55, false]]);
  });

  it('only where the stop keeps the book and the book is open: elsewhere the rows are the staff\'s, as before', () => {
    const third = war(60, 3);
    const a = put(third, TANKS, 'Manila', 0.55, 'arm');
    expect(lateStationed(third, a)).toBe(false);
    expect(months(third, a, [60, 65])).toEqual([[60, 'Manila', 0.55, false], [65, 'Manila', 0.55, false]]);
    // The book shut in the Pacific: nothing of it binds.
    const shut = war(60);
    shut.flags['hist:off:pacific'] = 1;
    const b = put(shut, TANKS, 'Manila', 0.55, 'arm');
    expect(lateStationed(shut, b)).toBe(false);
    expect(months(shut, b, [60, 65])).toEqual([[60, 'Manila', 0.55, false], [65, 'Manila', 0.55, false]]);
    // The player as Japan is met by the staffs alone: nobody of his is stationed.
    const mine = war(60);
    mine.human = 'JAP';
    const d = put(mine, TANKS, 'Manila', 0.55, 'arm');
    expect(lateStationed(mine, d)).toBe(false);
  });
});
