// The contact rule, round 3: the mends of the design director's verdict of 01:37Z, 27 November 2026 (her words, LAW):
//   "2 YES: standDown goes under the one test, and the row waits, told.
//    3 RULED IN: a loan's end waits while the test holds the corps (Murzuk, Burao). One test for every piece.
//    Q1 YES: the telling reads leavingNow.
//    The 3 rows with no wait written: name each by cause. A hold of the rule's own writes its wait (target 0)."
// The brief: /home/user/runs/contact2/R3-BRIEF.md, R3-1 to R3-4. The one test is drawn.ts `corpsInContact`; nothing here
// holds a copy of it. Seed 1, the first day, the old chart (far side off): Germany's III Corps alone in the Neumark,
// Poland's Army Poznan beside it in Poznan, as tests/contact-fact.test.ts sets it.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { setFromWait } from '../src/campaign/arrivalwaits';
import { addCorps, atWar, createGame, relocate, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

const ROWS = (setup as any).expeditions as any[];
let added = 0;
const addRow = (row: any[]): number => { ROWS.push(row); added++; return ROWS.length - 1; };
const clear = (): void => { while (added > 0) { ROWS.pop(); added--; } };
afterEach(() => { clear(); });

const W = 'W Force (New Zealand Division and 1st Armoured Brigade)';
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'GER'); };
const named = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'GER' && !c.dead && c.name === name);
const add = (g: Game, nat: string, name: string, kind: Corps['kind'], at: Region, divs = 1): Corps => addCorps(g, nat, name, kind, divs, at.id, {});

/** The Neumark emptied of Germany's corps, Poland beside it; the switch on (or off). */
function board(on = true): { g: Game; nm: Region } {
  clear();
  const g = startOpen(createGame(1, null));
  if (on) expect(setFromWait(g, true)).toBe(true);
  const nm = g.byName.Neumark;
  for (const u of [...nm.units]) relocate(g, u, g.byName.Bavaria);
  expect(nm.units).toEqual([]);
  expect(nm.nb.some((j) => g.regions[j].name === 'Poznan' && g.regions[j].ctrl === 'POL')).toBe(true);
  expect(atWar(g, 'GER', 'POL')).toBe(true);
  return { g, nm };
}

// ---- R3-1: the stand-in's stand-down goes under the one test ------------------------------------------------------

describe('R3-1: a row waits while the stand-in it would stand down is held by the one test', () => {
  /**
   * A stand-in (a row of Germany's whose `unless` names W Force's row, landed already) stands in the Neumark; the row
   * W Force's name lands at Magdeburg in month 2 and would stand it down. (`UNLESS` is read from the data at load, so the
   * row borrows W Force's name, as tests/contact-fact.test.ts does.)
   */
  const run = (o: { on?: boolean; relief?: boolean; battle?: boolean; at?: string; human?: boolean } = {}): { g: Game; i: number; j: number; stand: Corps } => {
    const { g, nm } = board(o.on ?? true);
    if (o.human) g.human = 'GER';
    const at = o.at ? g.byName[o.at] : nm;
    const stand = add(g, 'GER', 'Stand-in Test Corps', 'inf', at, 2);
    const j = addRow([1, 'GER', 'Stand-in Test Corps', 'inf', 2, 'Neumark', { unless: W }]);
    g.flags[`sent:${j}`] = 1;
    if (o.relief) add(g, 'GER', 'Relief Test Corps', 'inf', nm, 2);
    if (o.battle) g.lastBattle = { [nm.id]: [stand.id] };
    const i = addRow([2, 'GER', W, 'inf', 2, 'Magdeburg', {}]);
    month(g, 2);
    return { g, i, j, stand };
  };

  it('alone beside the enemy: the row waits that month, its wait written; the stand-in stays where it is', () => {
    const { g, i, stand } = run();
    expect(g.flags[`wait:${i}`]).toBe(2);
    expect(g.flags[`sent:${i}`]).toBeUndefined();
    expect(g.flags[`came:${i}`]).toBeUndefined();
    expect([stand.dead, g.regions[stand.reg].name]).toEqual([false, 'Neumark']);
  });

  it('relieved by a corps of its side: the row lands and stands it down, as before', () => {
    const { g, i, stand } = run({ relief: true });
    expect(g.flags[`wait:${i}`]).toBeUndefined();
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(stand.dead).toBe(true);
  });

  it('the fight half: relieved, but it took part in the battle last resolved on its ground: the row waits', () => {
    const { g, i, stand } = run({ relief: true, battle: true });
    expect(g.flags[`wait:${i}`]).toBe(2);
    expect(g.flags[`sent:${i}`]).toBeUndefined();
    expect(stand.dead).toBe(false);
  });

  it('a stand-in with no enemy beside it is let go (her answer 1): the row lands', () => {
    const { g, i, stand } = run({ at: 'Bavaria' });
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(stand.dead).toBe(true);
  });

  it('tried again the next month: once relieved, the row lands in that month, its half year counted from the wait', () => {
    const { g, i, stand, j } = run();
    expect(g.flags[`wait:${i}`]).toBe(2);
    expect(g.flags[`sent:${j}`]).toBe(1);
    add(g, 'GER', 'Relief Test Corps', 'inf', g.byName.Neumark, 2);
    month(g, 3);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(g.flags[`came:${i}`]).toBe(3);
    expect(stand.dead).toBe(true);
  });

  it("told, for the player's own row: which corps, and where, in the marked one-corps form; nothing told for a computer's", () => {
    const { g, i } = run({ human: true });
    expect(g.flags[`wait:${i}`]).toBe(2);
    expect(g.flags[`rowTold:${i}`]).toBe(2);
    const said = g.headlines.filter((h: any) => h.turn === 2 && /W Force/.test(h.text)).map((h: any) => h.text);
    expect(said).toEqual([`${W} is due in Magdeburg this month, formed from your Stand-in Test Corps, which is at the front in the Neumark. The arrival waits, and forms the month after that corps leaves the front.`]);
    const cpu = run();
    expect(cpu.g.flags[`rowTold:${cpu.i}`]).toBeUndefined();
  });

  it('switch off: the row lands and stands the stand-in down as before, and no wait is written', () => {
    const { g, i, stand } = run({ on: false });
    expect(g.flags[`wait:${i}`]).toBeUndefined();
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(stand.dead).toBe(true);
  });
});
