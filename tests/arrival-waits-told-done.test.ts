// The telling's flags go wherever a row is sent or struck (arrivalwaits.ts `rowDone`). The design director's answer 3
// of 00:43Z, 26 November 2026: "Clear the telling's three flags where the row is sent or struck (no dead data)." A row
// of the player's that waited on the front and was told carries `rowTold:<i>`, `rowAt:<i>` and `rowFront:<i>`; every
// place game.ts sets `sent:<i>` for it clears them, the formed headline having read them first: the arrival
// (`formedLine`, tests/arrival-waits-told), a row that joins a corps already landed (`joins`), a row every division of
// which stands already, a row struck for its half year, a row that stands in for one landed already, and a stand-in
// struck when the row it stands in for lands (`standDown`).
//
// The board is tests/from-wait-r2's: September 1939, Grenzmark beside Poznan, Poland at war with Germany; Bavaria and
// Magdeburg quiet. Germany is the player's, the switch on. The rows are added at the foot of the data's list and taken
// off after each test.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { addCorps, createGame, relocate, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

const ROWS = (setup as any).expeditions as any[];
let added = 0;
function addRow(row: any[]): number { ROWS.push(row); added++; return ROWS.length - 1; }
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } });

/**
 * (The contact rule's second form, the design director's ruling of 06:10Z, 26 November 2026: a corps beside the enemy
 * is held while it took part in the battle last resolved on its ground, or where nobody of its side keeps its province.
 * This board's corps at the front stand with others of their side, so each is set, by hand, as having fought the battle
 * last resolved where it stands, and is held as the board means. The relieved case: tests/contact-fact.test.ts.)
 */
const fought = (g: Game): void => { if (g.flags.fromWait !== 1) return; g.lastBattle = {}; for (const c of g.corps) if (!c.dead) (g.lastBattle[c.reg] ??= []).push(c.id); };
const month = (g: Game, turn: number): void => { fought(g); g.turn = turn; startPhase(g, 'GER'); };
const living = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'GER' && !c.dead && c.name === name);
const telFlags = (g: Game, i: number): string[] => Object.keys(g.flags).filter((k) => /^row(Told|At|Front):/.test(k) && k.endsWith(`:${i}`)).sort();
const all = (i: number): string[] => [`rowAt:${i}`, `rowFront:${i}`, `rowTold:${i}`];

/** Germany's grand campaign, the player's, the switch on, "Test Corps" in contact at Grenzmark. */
function board(): { g: Game; held: Corps } {
  const g = startOpen(createGame(1, 'GER'));
  g.flags.fromWait = 1;
  const held = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
  return { g, held };
}
/** The row waits in month 2 and is told: its three flags are written. */
function told(g: Game, i: number): void {
  month(g, 2);
  expect(g.flags[`wait:${i}`]).toBe(2);
  expect(telFlags(g, i)).toEqual(all(i));
}

describe("the telling's flags go where the row is sent", () => {
  it('a row that joins a corps already landed (`joins`): sent, and no flag of the telling stays', () => {
    const { g, held } = board();
    const old = addCorps(g, 'GER', 'Old Corps', 'inf', 1, g.byName.Bavaria.id, {});
    const i = addRow([2, 'GER', 'Test Army', 'inf', 1, 'Magdeburg', { from: ['Test Corps'], joins: 'Old Corps' }]);
    told(g, i);
    relocate(g, held, g.byName.Bavaria);
    month(g, 3);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(held.dead).toBe(true);
    expect(old.name).toBe('Test Army');
    expect(telFlags(g, i)).toEqual([]);
  });

  it('a row every division of which stands already: sent with nothing new to form, and no flag stays', () => {
    const { g, held } = board();
    // (The 217th Infantry Division is the Konigsberg garrison's: the player's own stands where it is.)
    const i = addRow([2, 'GER', 'Test Army (217th Infantry Division)', 'inf', 1, 'Magdeburg', { from: ['Test Corps'] }]);
    told(g, i);
    relocate(g, held, g.byName.Bavaria);
    month(g, 3);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(living(g, 'Test Army (217th Infantry Division)')).toBeUndefined();
    expect(living(g, 'Konigsberg garrison')).toBeDefined();
    expect(telFlags(g, i)).toEqual([]);
  });

  it('the stand-in of a row that lands (`standDown`): struck, and no flag stays', () => {
    const { g } = board();
    // (A name the data's rows stand in for: game.ts reads the stand-ins by the names it was laid with.)
    const NAME = 'W Force (New Zealand Division and 1st Armoured Brigade)';
    expect(ROWS.some((r) => r[6]?.unless === NAME)).toBe(true);
    const lands = addRow([3, 'GER', NAME, 'inf', 1, 'Magdeburg', {}]);
    const i = addRow([2, 'GER', 'Test Stand-in', 'inf', 1, 'Bavaria', { from: ['Test Corps'], unless: NAME }]);
    told(g, i);
    month(g, 3);
    expect(g.flags[`sent:${lands}`]).toBe(1);
    expect(g.flags[`sent:${i}`]).toBe(2);
    expect(telFlags(g, i)).toEqual([]);
  });
});

describe("the telling's flags go where the row is struck", () => {
  it('struck for its half year (held, once its corps came free, for ground still not lost, `ifLost`): no flag stays', () => {
    const { g, held } = board();
    const i = addRow([2, 'GER', 'Test Army', 'inf', 1, 'Magdeburg', { from: ['Test Corps'], ifLost: ['Bavaria'] }]);
    told(g, i);
    relocate(g, held, g.byName.Bavaria);
    for (let t = 3; t <= 9; t++) month(g, t);
    expect(g.flags[`sent:${i}`]).toBeUndefined();
    expect(telFlags(g, i)).toEqual(all(i));
    month(g, 10);
    expect(g.flags[`sent:${i}`]).toBe(2);
    expect(living(g, 'Test Army')).toBeUndefined();
    expect(telFlags(g, i)).toEqual([]);
  });

  it('struck because the row it stands in for has landed (`unless`): no flag stays', () => {
    const { g, held } = board();
    const other = addRow([1, 'GER', 'Other Army', 'inf', 1, 'Bavaria', {}]);
    const i = addRow([2, 'GER', 'Test Army', 'inf', 1, 'Magdeburg', { from: ['Test Corps'], unless: 'Other Army' }]);
    g.flags[`sent:${other}`] = 1;
    told(g, i);
    relocate(g, held, g.byName.Bavaria);
    month(g, 3);
    expect(g.flags[`sent:${i}`]).toBe(2);
    expect(held.dead).toBeFalsy();
    expect(telFlags(g, i)).toEqual([]);
  });
});
