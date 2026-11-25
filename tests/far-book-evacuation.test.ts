// The far side: the staff's evacuation leaves a corps the book stands on a station (the design director's ruling of
// 08:20Z, 22 November 2026, on the trace of seed 19 at the first stop; src/campaign/ai.ts, wantsOut).
//
// Cause: the staff reads a guest corps as broken at an order under 0.3 (or cut off) and its evacuation walks it to a
// port and sails it home; it never asked whether the book had the corps. The 2/15th Punjab, a row of the book's
// stations on its march through Dutch Borneo, stood at Sintang in February 1942 at 0.19: its staff walked it to
// Bandjermasin and sailed it to London, and in March the book stood it at Sampit again, from London. In that war,
// of every nation's held rows from December 1941 to December 1942, it was this row alone.
// Now the book acts before the staff, as it does for the corps of a dated attack still to come (Q, the hop): while
// the book stands a corps on a station its staff does not lift it off. At the stops that keep no book, and once the
// book lets go, nobody is on a station and the old line stands. With the far side's switch off nothing is read.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { wantsOut } from '../src/campaign/ai';
import { addCorps, capitulate, computeSupply, createGame, declareWar, relocate } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import { bookStations, stations } from '../src/campaign/stations';
import type { Corps, Game } from '../src/campaign/types';

const PUNJAB = '2/15th Punjab (Kuching)';

/** A book war at stop `stop` in month `turn`, Japan's blow fallen in December 1941 and the Indies in Britain's keeping. */
const war = (turn: number, stop = 1): Game => {
  const g = startOpen(createGame(3, null));
  if (stop !== 3) setStop(g, stop, true);
  declareWar(g, 'GER', 'NED');
  capitulate(g, 'NED', 'GER');
  g.turn = turn;
  g.flags['st:done:JAP:south'] = 28;
  for (const e of ['USA', 'UK']) declareWar(g, 'JAP', e);
  return g;
};
const put = (g: Game, name: string, place: string): Corps => {
  const c = addCorps(g, 'UK', name, 'inf', 1, place, { xp: 0.1, est: 0.1 });
  c.str = 0.05;
  return c;
};
/** The month's stations for Britain, then both corps broken as the march left the Punjabis: in supply, order 0.19. */
const february = (stop = 1): { g: Game; p: Corps; other: Corps } => {
  const g = war(30, stop);
  const p = put(g, PUNJAB, 'Kuching');
  const other = put(g, 'a British battalion the book names in no row', 'Sintang');
  bookStations(g, 'UK');
  g.supplyDirty = true;
  computeSupply(g);
  for (const c of [p, other]) c.org = 0.19;
  return { g, p, other };
};
const where = (g: Game, c: Corps): string => g.regions[c.reg].name;

describe("the staff's evacuation leaves a corps the book stands on a station (the director's ruling of 08:20Z, 22 November 2026)", () => {
  it('the Punjabis at Sintang by the book, broken and in supply: their staff does not lift them off', () => {
    const { g, p, other } = february();
    expect([where(g, p), p.hold, !!p.stay, p.bookWorks, p.supplied]).toEqual(['Sintang', true, true, 0, true]);
    expect(wantsOut(g, p)).toBe(false);
    // A corps beside them that is no row of the book's is its staff's to take off, as before.
    expect([where(g, other), other.bookWorks, other.supplied]).toEqual(['Sintang', undefined, true]);
    expect(wantsOut(g, other)).toBe(true);
  });

  it('cut off as well as broken, the book still has it', () => {
    const { g, p, other } = february();
    for (const c of [p, other]) c.supplied = false;
    expect([wantsOut(g, p), wantsOut(g, other)]).toEqual([false, true]);
  });

  it('at the third stop nobody is on a station, and the old line stands', () => {
    const { g, p, other } = february(3);
    // The book stands nobody: the Punjabis are where their staff has them, and no works are the book's.
    expect([where(g, p), p.hold, p.bookWorks]).toEqual(['Kuching', false, undefined]);
    // On Dutch ground, broken, they are their staff's to take off, as the corps beside them is.
    relocate(g, p, g.byName.Sintang);
    expect([wantsOut(g, p), wantsOut(g, other)]).toEqual([true, true]);
  });

  it('once the book lets go, the corps is its staff\'s again', () => {
    const { g, p } = february();
    expect(wantsOut(g, p)).toBe(false);
    // The month after the table's last: every row is let go.
    g.turn = stations()!.until + 1;
    bookStations(g, 'UK');
    expect([p.dead, p.bookWorks, p.hold]).toEqual([false, undefined, false]);
    p.org = 0.19;
    expect(wantsOut(g, p)).toBe(true);
  });
});
