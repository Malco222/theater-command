// A dated landing's corps goes to its quay a month ahead (the design director's 18:08Z of 22 November 2026 to ruling
// 234, on Assab's trace; src/campaign/stations.ts `setsOut`).
//
// One rule: every dated row arrives on its date, by sea or by land. A dated attack by land already sent its corps
// out early enough to be at its door by the move rules. A landing's corps were put down on their quay in the date's
// own month: Britain's Aden force, marched inland by its staff and standing in Addis Ababa's town in June 1941, came
// out of the town and to Aden that month with its month's march spent, could not embark, and landed at Assab in July
// against the record's June. Now a corps that is not on one of the row's ports is there, in the field, by the end of
// the month before, by road where it has one and put down where its quay is over the sea, and is held to its row
// until the row is no longer owed.
//
// Read on boards set by hand with Britain's own rows (far-africa.json: Assab from Aden, June 1941, month 22).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, atWar, createGame, declareWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { joinTown } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { bookAttacks, bookStations, stations } from '../src/campaign/stations';
import type { Corps, Game } from '../src/campaign/types';

const AD = 'Aden Striking Force (two Punjab battalions)';
const ASSAB = (): number => stations()!.attacks!.findIndex((r) => r.book === 'UK:eastafrica' && r.at[0] === 'Assab');
/** Britain and Italy at war at a stop, Britain's choice gone in 17, and the Aden force standing where the case puts it. */
const board = (turn: number, at: string, stop = 1): { g: Game; c: Corps } => {
  const g = startOpen(createGame(3, null));
  setStop(g, stop, true);
  g.turn = turn;
  if (!atWar(g, 'ITA', 'UK')) declareWar(g, 'ITA', 'UK');
  g.flags['st:done:UK:eastafrica'] = 17;
  const c = addCorps(g, 'UK', AD, 'inf', 1, at, { str: 0.2 });
  g.supplyDirty = true;
  return { g, c };
};
const at = (g: Game, c: Corps): string => g.regions[c.reg].name;
const heldTo = (g: Game, c: Corps): number | undefined => g.flags[`bkl:${c.id}`];

describe('a dated landing\'s corps goes to its quay a month ahead (the design director, 18:08Z of 22 November 2026)', () => {
  it('the board: Assab is a landing from Aden in month 22, Aden has a road by land from Mukalla and none from Berbera', () => {
    const g = startOpen(createGame(3, null));
    const row = stations()!.attacks![ASSAB()];
    expect([row.month, row.sea, row.from, row.force]).toEqual([22, true, ['Aden'], [AD]]);
    const aden = g.byName['Aden'];
    expect([aden.port, aden.ctrl, g.byName['Mukalla'].ctrl, g.byName['Berbera'].ctrl, g.byName['Assab'].ctrl]).toEqual([true, 'UK', 'UK', 'UK', 'ITA']);
    expect(aden.nb).toContain(g.byName['Mukalla'].id);
    expect(aden.nb).not.toContain(g.byName['Berbera'].id);
  });

  it('with its quay over the sea it is put down there the month before, held to its row, and lands on the date', () => {
    const { g, c } = board(21, 'Berbera');
    bookAttacks(g, 'UK');
    expect([at(g, c), c.town, c.hold, c.ap, heldTo(g, c)]).toEqual(['Aden', undefined, true, 0, ASSAB() + 1]);
    // (Asked again in the same month, as the staff's month ends, nothing more is done.)
    bookAttacks(g, 'UK');
    expect([at(g, c), c.hold, heldTo(g, c)]).toEqual(['Aden', true, ASSAB() + 1]);
    // June 1941: a month's moves in hand, it sails from Aden and Assab is Britain's; the row made, it is the staff's again.
    g.turn = 22;
    c.ap = c.apMax;
    bookAttacks(g, 'UK');
    expect(g.byName['Assab'].ctrl).toBe('UK');
    expect([at(g, c), c.hold, heldTo(g, c)]).toEqual(['Assab', false, undefined]);
    expect(g.flags[`ba:${ASSAB()}`]).toBe(22);
  });

  it('not before the month before: two months ahead it stands where its staff has it', () => {
    const { g, c } = board(20, 'Berbera');
    bookAttacks(g, 'UK');
    expect([at(g, c), c.hold, heldTo(g, c)]).toEqual(['Berbera', false, undefined]);
  });

  it('one standing in a town comes out and goes to its quay the month before: Assab\'s own case', () => {
    const { g, c } = board(21, 'Berbera');
    const B = g.byName['Berbera'];
    B.units.splice(B.units.indexOf(c), 1);
    joinTown(g, B, c);
    c.town = 20;
    bookAttacks(g, 'UK');
    expect([at(g, c), c.town, c.hold, heldTo(g, c)]).toEqual(['Aden', undefined, true, ASSAB() + 1]);
    g.turn = 22;
    c.ap = c.apMax;
    bookAttacks(g, 'UK');
    expect(g.byName['Assab'].ctrl).toBe('UK');
  });

  it('with a road by land it marches to its quay by the move rules, the last month of its road the one before the date', () => {
    const { g, c } = board(21, 'Mukalla');
    c.ap = c.apMax;
    bookAttacks(g, 'UK');
    expect([at(g, c), c.hold, c.ap, heldTo(g, c)]).toEqual(['Aden', true, 0, ASSAB() + 1]);
    // (A month's march from its quay, it does not set out two months ahead.)
    const early = board(20, 'Mukalla');
    bookAttacks(early.g, 'UK');
    expect([at(early.g, early.c), early.c.hold, heldTo(early.g, early.c)]).toEqual(['Mukalla', false, undefined]);
  });

  it('one on its port already is not moved: it is the row\'s from the month before', () => {
    const { g, c } = board(21, 'Aden');
    const ap = c.ap;
    bookAttacks(g, 'UK');
    expect([at(g, c), c.town, c.hold, c.ap, heldTo(g, c)]).toEqual(['Aden', undefined, true, ap, ASSAB() + 1]);
    const early = board(20, 'Aden');
    bookAttacks(early.g, 'UK');
    expect([at(early.g, early.c), early.c.hold, heldTo(early.g, early.c)]).toEqual(['Aden', false, undefined]);
  });

  it('it waits on the book: at a stop that keeps none, and with the book closed in East Africa, the staff\'s own march stands', () => {
    for (const stop of [3, 4, 5]) {
      const { g, c } = board(21, 'Berbera', stop);
      bookAttacks(g, 'UK');
      expect([at(g, c), c.hold, heldTo(g, c)], `stop ${stop}`).toEqual(['Berbera', false, undefined]);
    }
    const { g, c } = board(21, 'Berbera');
    g.flags['hist:off:eastafrica'] = 20;
    bookAttacks(g, 'UK');
    expect([at(g, c), c.hold, heldTo(g, c)]).toEqual(['Berbera', false, undefined]);
  });

  it('held to its row by its own book: the table\'s end does not let it go, the closing of its own book does', () => {
    const { g, c } = board(21, 'Berbera');
    bookAttacks(g, 'UK');
    expect([c.hold, heldTo(g, c)]).toEqual([true, ASSAB() + 1]);
    // The table's theatre off the book: the table binds nobody, and the corps is still its own row's.
    g.flags[`hist:off:${stations()!.theatre}`] = 21;
    bookStations(g, 'UK');
    expect([at(g, c), c.hold, heldTo(g, c)]).toEqual(['Aden', true, ASSAB() + 1]);
    // Its own book closed too: it is the staff's again.
    g.flags['hist:off:eastafrica'] = 21;
    bookStations(g, 'UK');
    expect([c.hold, heldTo(g, c)]).toEqual([false, undefined]);
  });
});
