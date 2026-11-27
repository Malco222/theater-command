// The contact rule, round 3, R3-4: "A hold of the rule's own writes its wait" (the design director's verdict of 01:37Z,
// 27 November 2026: "The 3 rows with no wait written: name each by cause. A hold of the rule's own writes its wait
// (target 0). The older place test, if that is the cause, gets its own known-list row."). game.ts `ruleHolds`: ONE pure
// reader (it writes nothing) listing this month's holds of the rule's own on every path, each with whether its wait is
// written and told: the gate reads "untold holds of the rule's own" through it, target 0.
// The boards are tests/contact-fact.test.ts's and tests/contact-r3.test.ts's: seed 1, the first day, the old chart (far
// side off); Germany's corps alone in the Neumark, Poland's Army Poznan beside it in Poznan.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { setFromWait } from '../src/campaign/arrivalwaits';
import { divisionsOf } from '../src/campaign/armies';
import { addCorps, atWar, createGame, declareWar, relocate, ruleHolds, startPhase, theatreName } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
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
/** What the reader says, without the month (every hold read is of this month). */
const said = (g: Game): unknown[] => ruleHolds(g).map(({ turn, ...h }) => { expect(turn).toBe(g.turn); return h; });

/** The Neumark emptied of Germany's corps (all but III Corps where `iii`), Poland beside it; the switch on (or off). */
function board(o: { on?: boolean; iii?: boolean } = {}): { g: Game; nm: Region } {
  clear();
  const g = startOpen(createGame(1, null));
  if (o.on ?? true) expect(setFromWait(g, true)).toBe(true);
  const nm = g.byName.Neumark;
  for (const u of [...nm.units]) if (!(o.iii && u.name === 'III Corps')) relocate(g, u, g.byName.Bavaria);
  expect(nm.nb.some((j) => g.regions[j].name === 'Poznan' && g.regions[j].ctrl === 'POL')).toBe(true);
  expect(atWar(g, 'GER', 'POL')).toBe(true);
  return { g, nm };
}

describe("R3-4: one pure reader of this month's holds of the rule's own, each with its wait written and told", () => {
  it('a row that takes a named corps whole, held alone beside the enemy: listed, its wait written', () => {
    const { g, nm } = board();
    add(g, 'GER', 'Row Test Corps', 'inf', nm, 2);
    const i = addRow([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Row Test Corps'] }]);
    month(g, 2);
    expect(said(g)).toEqual([{ nat: 'GER', i, row: 'Test Army', path: 'from', corps: ['Row Test Corps'], waits: true, written: true, told: null, untold: false }]);
  });

  it("the stand-in a row would stand down (R3-1): listed, written; for the player's own row, told", () => {
    for (const human of [false, true]) {
      const { g, nm } = board();
      if (human) g.human = 'GER';
      add(g, 'GER', 'Stand-in Test Corps', 'inf', nm, 2);
      const j = addRow([1, 'GER', 'Stand-in Test Corps', 'inf', 2, 'Neumark', { unless: W }]);
      g.flags[`sent:${j}`] = 1;
      const i = addRow([2, 'GER', W, 'inf', 2, 'Magdeburg', {}]);
      month(g, 2);
      expect(said(g)).toEqual([{ nat: 'GER', i, row: W, path: 'standDown', corps: ['Stand-in Test Corps'], waits: true, written: true, told: human ? true : null, untold: false }]);
    }
  });

  it("a loan's end (R3-2): listed by the way it would have ended, written; told for the player's own", () => {
    for (const human of [false, true]) {
      const { g, nm } = board();
      if (human) g.human = 'GER';
      const lent = add(g, 'GER', 'Lent Test Corps', 'inf', nm, 2);
      const j = addRow([1, 'GER', 'Lent Test Corps', 'inf', 2, 'Neumark', { until: 2 }]);
      g.flags[`sent:${j}`] = 1;
      g.flags[`lent:${j}`] = lent.id;
      month(g, 2);
      expect(said(g)).toEqual([{ nat: 'GER', i: j, row: 'Lent Test Corps', path: 'loanGone', corps: ['Lent Test Corps'], waits: true, written: true, told: human ? true : null, untold: false }]);
    }
  });

  it('a claim the test keeps where it stands: listed as a piece the row forms without (no wait: the row is sent)', () => {
    const { g, nm } = board({ iii: true });
    expect(divisionsOf(g, named(g, 'III Corps')!).map((d) => d.name)).toContain('50th Infantry Division');
    add(g, 'GER', 'From Test Corps', 'inf', nm, 2);
    const i = addRow([2, 'GER', 'Test Army (50th Infantry Division)', 'inf', 1, 'Magdeburg', { from: ['From Test Corps'] }]);
    month(g, 2);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(said(g)).toEqual([{ nat: 'GER', i, row: 'Test Army (50th Infantry Division)', path: 'claim', corps: ['III Corps'], waits: false, written: null, told: null, untold: false }]);
  });

  it('the theatre draw skips a corps the test holds: listed as a piece the row forms without', () => {
    clear();
    const g = startOpen(createGame(1, null));
    setFromWait(g, true);
    declareWar(g, 'GER', 'GRE');
    const balkans = g.regions.filter((r) => theatreName(r) === 'the Balkans');
    const greek = (r: Region): boolean => r.ctrl === 'GRE' || r.units.some((u) => u.nat === 'GRE');
    const line = balkans.find((r) => !greek(r) && !atWar(g, 'GER', r.ctrl) && r.nb.some((j) => greek(g.regions[j])))!;
    for (const u of [...line.units]) if (u.nat === 'GER') relocate(g, u, g.byName.Bavaria);
    const held = add(g, 'GER', 'Held Test Corps', 'inf', line, 3);
    const i = addRow([2, 'GER', 'Test Theatre Army', 'inf', 1, 'Magdeburg', { from: 'the Balkans' }]);
    month(g, 2);
    expect(held.divs).toBe(3);
    expect(said(g).filter((h: any) => h.path === 'draw')).toEqual([{ nat: 'GER', i, row: 'Test Theatre Army', path: 'draw', corps: ['Held Test Corps'], waits: false, written: null, told: null, untold: false }]);
  });

  it('a row held by its place (the older place test, its own known-list row): not a hold of the rule\'s own, and no wait', () => {
    const { g } = board();
    add(g, 'GER', 'Row Test Corps', 'inf', g.byName.Bavaria, 2);
    const i = addRow([2, 'GER', 'Test Army', 'inf', 2, 'Poznan', { from: ['Row Test Corps'] }]);
    month(g, 2);
    expect(g.flags[`sent:${i}`]).toBeUndefined();
    expect(g.flags[`wait:${i}`]).toBeUndefined();
    expect(said(g)).toEqual([]);
  });

  it('the reader writes nothing: the whole save, and what it reads, are the same before and after', () => {
    const { g, nm } = board();
    add(g, 'GER', 'Row Test Corps', 'inf', nm, 2);
    addRow([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Row Test Corps'] }]);
    month(g, 2);
    const before = saveGame(g);
    const a = ruleHolds(g);
    // (What it hands back is a copy: changing it changes neither the game nor the next reading.)
    a[0].told = false; a[0].corps.push('Nobody'); (a[0] as any).written = false;
    const b = ruleHolds(g);
    expect(saveGame(g)).toBe(before);
    expect([b[0].written, b[0].told, b[0].corps, b[0].untold]).toEqual([true, null, ['Row Test Corps'], false]);
    expect(ruleHolds(g, 1)).toEqual([]);
  });

  it('switch off: nothing is a hold of the rule\'s own, and nothing is listed', () => {
    const { g, nm } = board({ on: false });
    add(g, 'GER', 'Row Test Corps', 'inf', nm, 2);
    addRow([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Row Test Corps'] }]);
    month(g, 2);
    expect(said(g)).toEqual([]);
  });
});
