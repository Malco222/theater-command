// A division is laid once: a row that lays nothing and the row that stands in for it (release 2, the grand campaign).
//
// The design director, 16:39Z, 25 November 2026, on the joiner's reading 7: a dated row whose every division stands
// already, each with a living corps that stays, lays nothing and leaves the corps its `from` lists standing; and
// "standing its stand-in down must remove no strength". So the row that stands in for it (`unless`, ruling 229, 1c)
// never comes after it, and one that has landed already stands on as it is: no corps goes where none came. Where the
// row does lay a corps, its stand-in goes with it as it always did (tests/crete-wforce-r2.test.ts).
//
// The rows that stand in for another are read once, when the rules load: so the three rows of this test are put at the
// foot of the data's list before the rules are loaded, and taken off after.
import { afterAll, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';

const ROWS = (setup as any).expeditions as any[];
const FOOT = ROWS.length;
const NAME = '9th and 2nd Panzer Divisions';
ROWS.push([10, 'GER', '903rd Panzer Division', 'arm', 1, 'Hamburg', { open: 1, unless: NAME }]);
ROWS.push([12, 'GER', NAME, 'arm', 2, 'Berlin', { open: 1 }]);
ROWS.push([14, 'GER', '904th Panzer Division', 'arm', 1, 'Hamburg', { open: 1, unless: NAME }]);
const { createGame, relocate, startPhase } = await import('../src/campaign/game');
const { startOpen } = await import('../src/campaign/staff');
const { divisionsOf } = await import('../src/campaign/armies');
afterAll(() => { ROWS.length = FOOT; });

describe('a row that lays nothing, and the row that stands in for it (the design director, 16:39Z, 25 November 2026)', () => {
  it('the stand-in landed already stands on as it is, and the one still to come never comes', () => {
    const g = startOpen(createGame(1, null));
    const month = (turn: number): void => { g.turn = turn; startPhase(g, 'GER'); };
    const named = (name: string) => g.corps.find((c) => c.nat === 'GER' && !c.dead && c.name === name);
    const xxii = named('XXII Corps')!;
    relocate(g, xxii, g.byName['Berlin']);
    month(10);
    const s = named('903rd Panzer Division')!;
    expect(s).toBeDefined();
    month(11);
    month(12);
    // The row has landed and laid nothing: both its divisions stand with XXII Corps, where the row is for.
    expect(g.flags[`sent:${FOOT + 1}`]).toBe(1);
    expect(g.flags[`came:${FOOT + 1}`]).toBe(12);
    expect(named(NAME)).toBeUndefined();
    expect(divisionsOf(g, xxii).map((d) => d.name)).toEqual(['2nd Panzer Division', '9th Panzer Division']);
    // Its stand-in stands on: no corps goes where none came.
    expect(s.dead).toBe(false);
    expect(g.corps.includes(s)).toBe(true);
    expect(s.divs).toBe(1);
    month(13);
    month(14);
    // And the stand-in still to come never comes after it.
    expect(g.flags[`sent:${FOOT + 2}`]).toBe(2);
    expect(named('904th Panzer Division')).toBeUndefined();
    expect(s.dead).toBe(false);
  });
});
