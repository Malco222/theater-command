// The contact rule, round 4, R4-3: the rule clears the wait markers IT wrote when the loan ends, on the ON path only
// (the design director's (4) of 16:07Z, 27 November 2026: "Not leave-and-list, because no dead data. The rule clears the
// markers IT wrote when the loan ends, on the ON path only. Off writes none, so switch-off identity holds. If that cannot
// be done without touching the base's own markers, bring me why." Finding S1-8: seed 40's Somaliland force read
// 'lent=-1 wait=10' from month 11 to month 36).
//
// Established first (rules builder 4's report): the base (bab18b248) writes `wait:<i>` only on a row not yet sent, at
// three gates (game.ts there 5886 ruling B7's wait, 5933 the far from-row's live front, 5952 `sourceAsked`; 5946 is the
// switch's own), all three on the far side alone, and never deletes one. So a marker the base wrote can stand on a lent
// row under the rule's only where the row's own words let one of those gates act on it (game.ts `baseMayMark`): on every
// other lent row, and on every row with the far side off, any `wait:<i>` a sent row carries is the rule's. There the
// rule deletes it at each point a held loan ends; on a row whose words let a base gate act, the save cannot tell the
// rule's marker from one the base wrote before the row was sent (the rule's first write lies over it), so that case is
// left as built, for the director.
// The far side on for the whole file (TC_FAR), as tests/contact-r3-far.test.ts: Germany's test corps alone on the
// Grenzmark, Poland's Army Poznan beside it in Poznan.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { setFromWait } from '../src/campaign/arrivalwaits';
import { addCorps, atWar, baseMayMark, createGame, relocate, removeCorps, rowWords, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { farOn } from '../src/campaign/world';
import type { Corps, Game } from '../src/campaign/types';

const ROWS = (setup as any).expeditions as any[];
let added = 0;
const addRow = (row: any[]): number => { ROWS.push(row); added++; return ROWS.length - 1; };
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } });
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'GER'); };
const marks = (g: Game): string[] => Object.keys(g.flags).filter((k) => k.startsWith('wait:')).sort();

/** A lent corps of Germany's, its loan over in month 2, alone on the Grenzmark beside Poland (the far side on). */
function board(o: { on?: boolean; words?: any; human?: boolean; awaited?: boolean } = {}): { g: Game; j: number; c: Corps; relieve: () => Corps } {
  const g = startOpen(createGame(3, null));
  expect(farOn(g)).toBe(true);
  if (o.human) { g.human = 'GER'; g.flags.offers = 1; }
  if (o.on ?? true) expect(setFromWait(g, true)).toBe(true);
  const gz = g.byName.Grenzmark;
  for (const u of [...gz.units]) relocate(g, u, g.byName.Bavaria);
  expect(atWar(g, 'GER', 'POL')).toBe(true);
  const before = marks(g);
  expect(before).toEqual([]);
  const c = addCorps(g, 'GER', 'Lent Test Corps', 'inf', 1, gz.id, {});
  const j = addRow([1, 'GER', 'Lent Test Corps', 'inf', 1, 'Grenzmark', { until: 2, ...(o.words ?? {}) }]);
  if (o.awaited) addRow([9, 'GER', 'Test Tank Division', 'arm', 1, 'Magdeburg', { awaits: ['Lent Test Corps'], str: 1 }]);
  g.flags[`sent:${j}`] = 1;
  g.flags[`lent:${j}`] = c.id;
  return { g, j, c, relieve: () => addCorps(g, 'GER', 'Relief Test Corps', 'inf', 2, gz.id, {}) };
}

describe('R4-3: the rule deletes the wait marker it wrote at each point a held loan ends (ON path only)', () => {
  it('held, then relieved: the loan ends the next month as it would have, and the save holds no wait:<i> for the row', () => {
    const { g, j, c, relieve } = board();
    month(g, 2);
    expect([c.dead, g.flags[`lent:${j}`], g.flags[`wait:${j}`]]).toEqual([false, c.id, 2]);
    relieve();
    month(g, 3);
    expect([c.dead, g.flags[`lent:${j}`]]).toEqual([true, -1]);
    expect(g.flags[`wait:${j}`]).toBeUndefined();
    expect(marks(g)).toEqual([]);
  });

  it('held, then the corps lost: the loan ends, and its marker goes with it', () => {
    const { g, j, c } = board();
    month(g, 2);
    expect(g.flags[`wait:${j}`]).toBe(2);
    removeCorps(g, c);
    month(g, 3);
    expect(g.flags[`lent:${j}`]).toBe(-1);
    expect(g.flags[`wait:${j}`]).toBeUndefined();
  });

  it('held for two months: the marker moves with the hold, and is gone the month the loan ends', () => {
    const { g, j, relieve } = board();
    month(g, 2);
    month(g, 3);
    expect(g.flags[`wait:${j}`]).toBe(3);
    relieve();
    month(g, 4);
    expect([g.flags[`lent:${j}`], g.flags[`wait:${j}`]]).toEqual([-1, undefined]);
  });

  it("the player's own loan whose end is his offer: the marker goes the month the hold ends and the offer is made", () => {
    const { g, j, c, relieve } = board({ human: true, awaited: true });
    month(g, 2);
    expect([g.flags[`wait:${j}`], g.flags[`leave:${j}`]]).toEqual([2, undefined]);
    relieve();
    month(g, 3);
    expect([g.flags[`leave:${j}`], c.dead, g.flags[`lent:${j}`]]).toEqual([3, false, c.id]);
    expect(g.flags[`wait:${j}`]).toBeUndefined();
  });

  it('a loan never held writes no marker and clears none (relieved from the first)', () => {
    const { g, j, relieve } = board();
    relieve();
    month(g, 2);
    expect([g.flags[`lent:${j}`], g.flags[`wait:${j}`]]).toEqual([-1, undefined]);
  });

  it('switch off: the base, nothing written; and turned off mid-war with a hold standing, off writes and clears nothing', () => {
    const off = board({ on: false });
    month(off.g, 2);
    expect([off.g.flags[`lent:${off.j}`], marks(off.g)]).toEqual([-1, []]);
    const { g, j } = board();
    month(g, 2);
    expect(g.flags[`wait:${j}`]).toBe(2);
    expect(setFromWait(g, false)).toBe(true);
    month(g, 3);
    expect(g.flags[`lent:${j}`]).toBe(-1);
    expect(g.flags[`wait:${j}`]).toBe(2);
  });

  it("a lent row whose own words let a gate of the base write a marker before it was sent: left as built, for the director", () => {
    // (A `from` with no body of its own: the far from-row's live front and `sourceAsked` can each write `wait:<i>` on it
    // before it is sent; the save cannot tell that marker from the rule's once the rule has written over it.)
    const { g, j, relieve } = board({ words: { from: ['No Such Test Corps'] } });
    expect(baseMayMark(g, 'GER', ROWS[j], rowWords(g, 'GER', ROWS[j]).o)).toBe(true);
    month(g, 2);
    expect(g.flags[`wait:${j}`]).toBe(2);
    relieve();
    month(g, 3);
    expect([g.flags[`lent:${j}`], g.flags[`wait:${j}`]]).toEqual([-1, 2]);
  });
});

describe("R4-3 establishing: on which of the data's lent rows a base marker can stand (`baseMayMark`, the base's three gates by the row's words)", () => {
  const g = startOpen(createGame(3, null));
  const at = (nat: string, name: string): boolean => {
    const row = ROWS.find((r) => r[1] === nat && r[2] === name);
    expect(row).toBeDefined();
    return baseMayMark(g, nat, row, rowWords(g, nat, row).o);
  };
  it("none on the brief's loans held in the gate's wars: the Somaliland force, the Kufra garrison, the Eritrea command at Keren", () => {
    expect(at('UK', "Somaliland force (the Camel Corps and Chater's battalions)")).toBe(false);
    expect(at('ITA', 'Kufra garrison (El Tag fort and its Saharan company)')).toBe(false);
    expect(at('ITA', 'Eritrea command (Keren)')).toBe(false);
  });
  it("can, by the words: the 5th Indian Division (sourceAsked, the player's row), the 38th Division at Canton (ruling B7's wait)", () => {
    expect(at('UK', '5th Indian Division (the Sudan and Eritrea)')).toBe(true);
    expect(at('JAP', '38th Division (Canton)')).toBe(true);
  });
});
