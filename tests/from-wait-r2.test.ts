// The old chart's "from" rows wait on contact (release 2, the grand campaign; game.ts `expeditions`, drawn.ts
// `corpsInContact`; switch 'fromWait', off until its gate passes).
//
// The design director's ruling of 19:47Z, 25 November 2026 (pacific-japan/dd-form-from-rows-contact-1125.md): her
// 18:47Z test, "a date never lifts a corps out of a fight", is read on the second door too. A dated row that takes a
// named corps whole waits while that corps stands in contact, an enemy at war on its province or beside it, and its
// half year runs from the month the corps comes free. Where the corps already stands at the place the row forms, the
// row goes ahead: a new name, nothing lifted.
//
// Read on a board set by hand, in September 1939: Grenzmark lies beside Poznan, Poland's and at war with Germany;
// Bavaria and Magdeburg are quiet. The rows are added at the foot of the data's list and taken off after each test.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import stage from '../data/campaign/stage.json';
import { corpsInContact } from '../src/campaign/drawn';
import { addCorps, createGame, relocate, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

const ROWS = (setup as any).expeditions as any[];
let added = 0;
function addRow(row: any[]): number { ROWS.push(row); added++; return ROWS.length - 1; }
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } });

const month = (g: Game, turn: number): void => { g.turn = turn; startPhase(g, 'GER'); };
const living = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'GER' && !c.dead && c.name === name);
const at = (g: Game, c: Corps | undefined): string | undefined => (c ? g.regions[c.reg].name : undefined);

/** A grand campaign with a German corps named "Test Corps" at `where`, and a row of month 2 that takes it whole to form "Test Army" at `place`. */
function board(where: string, place = 'Magdeburg', on = true, more: Record<string, unknown> = {}): { g: Game; i: number; held: Corps } {
  const g = startOpen(createGame(1, null));
  if (on) g.flags.fromWait = 1;
  const held = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName[where].id, {});
  const i = addRow([2, 'GER', 'Test Army', 'inf', 2, place, { from: ['Test Corps'], ...more }]);
  return { g, i, held };
}

describe('the board the tests read', () => {
  it('Grenzmark is in contact (Poznan beside it, Poland at war); Bavaria and Magdeburg are not', () => {
    const g = startOpen(createGame(1, null));
    const on = (name: string): boolean => corpsInContact(g, 'GER', { reg: g.byName[name].id } as Corps);
    expect(on('Grenzmark')).toBe(true);
    expect(on('Bavaria')).toBe(false);
    expect(on('Magdeburg')).toBe(false);
  });

  it('the switch is off as it comes: a new grand campaign carries no fromWait flag', () => {
    expect((stage as any).fromWait).toBe(false);
    expect(startOpen(createGame(1, null)).flags.fromWait).toBeUndefined();
  });
});

describe('off, a row takes its corps wherever it stands (today)', () => {
  it('the corps in contact is lifted out of Grenzmark and the row forms at Magdeburg', () => {
    const { g, i, held } = board('Grenzmark', 'Magdeburg', false);
    month(g, 2);
    expect(held.dead).toBe(true);
    expect(at(g, living(g, 'Test Army'))).toBe('Magdeburg');
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(g.flags[`wait:${i}`]).toBeUndefined();
  });
});

describe('on, a row waits while the corps it takes is in contact', () => {
  it('the corps stays where it fights and the row does not form; the month is kept', () => {
    const { g, i, held } = board('Grenzmark');
    month(g, 2);
    expect(held.dead).toBeFalsy();
    expect(at(g, held)).toBe('Grenzmark');
    expect(living(g, 'Test Army')).toBeUndefined();
    expect(g.flags[`sent:${i}`]).toBeUndefined();
    expect(g.flags[`wait:${i}`]).toBe(2);
  });

  it('one corps of two in contact holds the whole row', () => {
    const { g, i, held } = board('Grenzmark');
    const quiet = addCorps(g, 'GER', 'Quiet Corps', 'inf', 1, g.byName.Bavaria.id, {});
    ROWS[i][6].from.push('Quiet Corps');
    month(g, 2);
    expect(held.dead).toBeFalsy();
    expect(quiet.dead).toBeFalsy();
    expect(living(g, 'Test Army')).toBeUndefined();
    expect(g.flags[`wait:${i}`]).toBe(2);
  });

  it('its half year runs from the month the corps comes free: eight months in contact, then the row comes, not struck', () => {
    const { g, i, held } = board('Grenzmark');
    for (let t = 2; t <= 9; t++) month(g, t);
    expect(g.flags[`wait:${i}`]).toBe(9);
    expect(g.flags[`sent:${i}`]).toBeUndefined();
    relocate(g, held, g.byName.Bavaria);
    month(g, 10);
    expect(held.dead).toBe(true);
    expect(at(g, living(g, 'Test Army'))).toBe('Magdeburg');
    expect(g.flags[`sent:${i}`]).toBe(1);
  });

  it('a corps out of contact is taken as today', () => {
    const { g, i, held } = board('Bavaria');
    month(g, 2);
    expect(held.dead).toBe(true);
    expect(at(g, living(g, 'Test Army'))).toBe('Magdeburg');
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(g.flags[`wait:${i}`]).toBeUndefined();
  });

  it('where the corps already stands at the place the row forms, the row goes ahead in contact: a new name, nothing lifted', () => {
    const { g, i, held } = board('Grenzmark', 'Grenzmark');
    month(g, 2);
    expect(held.dead).toBe(true);
    expect(at(g, living(g, 'Test Army'))).toBe('Grenzmark');
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(g.flags[`wait:${i}`]).toBeUndefined();
  });

  it('the switch rides in the save', () => {
    const { g, i } = board('Grenzmark');
    const back = loadGame(saveGame(g))!;
    month(back, 2);
    expect(back.flags.fromWait).toBe(1);
    expect(back.flags[`wait:${i}`]).toBe(2);
    expect(back.corps.some((c) => c.nat === 'GER' && !c.dead && c.name === 'Test Corps')).toBe(true);
  });
});

describe("the dial: the rule acts at stops 3 to 5, and at the book's stops the book's rows keep their book", () => {
  // The design director's ruling of 21:25Z, 25 November 2026, for the far side's rows and read on this door alike: "at
  // stops 1 and 2 the book's rows keep their book, as for every other piece. The contact rule acts at stops 3 to 5."
  it.each([1, 2])('at stop %i, with the switch on, the corps in contact is lifted and the row forms in its month, as with the switch off', (stop) => {
    const { g, i, held } = board('Grenzmark');
    expect(setStop(g, stop, true)).toBe(true);
    expect(corpsInContact(g, 'GER', held)).toBe(true);
    month(g, 2);
    expect(living(g, 'Test Corps')).toBeUndefined();
    expect(at(g, living(g, 'Test Army'))).toBe('Magdeburg');
    expect(g.flags[`wait:${i}`]).toBeUndefined();
  });
  it.each([4, 5])('at stop %i it acts as at stop 3: the row waits and the corps stays where it fights', (stop) => {
    const { g, i, held } = board('Grenzmark');
    expect(setStop(g, stop, true)).toBe(true);
    month(g, 2);
    expect(at(g, held)).toBe('Grenzmark');
    expect(living(g, 'Test Army')).toBeUndefined();
    expect(g.flags[`wait:${i}`]).toBe(2);
  });
});
