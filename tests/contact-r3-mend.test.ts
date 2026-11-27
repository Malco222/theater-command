// The contact rule, round 3, the mender's cases: the sceptics' findings that needed no ruling (reports
// /home/user/runs/contact2/r3/reports/r2-rule-sceptic3.json and r3-telling-sceptic3.json). Seed 1, the first day, the
// old chart: the Neumark emptied of Germany's corps, Poland's Army Poznan beside it in Poznan, as tests/contact-r3.test.ts
// sets it. Each case was seen to fail under the sceptic's own case or mutation, then to pass.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { setFromWait } from '../src/campaign/arrivalwaits';
import { divisionsOf } from '../src/campaign/armies';
import { addCorps, atWar, createGame, declareWar, leavingWatch, relocate, rowLeaving, ruleHolds, startPhase, theatreName } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

const ROWS = (setup as any).expeditions as any[];
let added = 0;
const addRow = (row: any[]): number => { ROWS.push(row); added++; return ROWS.length - 1; };
const clear = (): void => { while (added > 0) { ROWS.pop(); added--; } };
afterEach(() => { clear(); });

const W = 'W Force (New Zealand Division and 1st Armoured Brigade)';
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'GER'); };
const add = (g: Game, name: string, at: Region, divs = 1): Corps => addCorps(g, 'GER', name, 'inf', divs, at.id, {});

function board(on = true): { g: Game; nm: Region } {
  clear();
  const g = startOpen(createGame(1, null));
  if (on) expect(setFromWait(g, true)).toBe(true);
  const nm = g.byName.Neumark;
  for (const u of [...nm.units]) relocate(g, u, g.byName.Bavaria);
  expect(nm.units).toEqual([]);
  expect(atWar(g, 'GER', 'POL')).toBe(true);
  return { g, nm };
}

/** A lent corps of Germany's whose loan is over in month 2, in the Neumark (alone, beside Poland) or elsewhere. */
const lend = (g: Game, name: string, at: Region, divs = 1): { c: Corps; j: number } => {
  const c = add(g, name, at, divs);
  const j = addRow([1, 'GER', name, 'inf', divs, 'Neumark', { until: 2 }]);
  g.flags[`sent:${j}`] = 1;
  g.flags[`lent:${j}`] = c.id;
  return { c, j };
};

// ---- S1-1: a held lent corps holds its divisions for a row of the same pass -----------------------------------------

describe("S1-1: a lent corps the one test holds is no corps 'on its way out' to a row that names one of its divisions", () => {
  const D = '77th Infantry Division';
  const run = (on: boolean, alone = true): { g: Game; lent: Corps; j: number; i: number; holders: Corps[] } => {
    const { g, nm } = board(on);
    const { c: lent, j } = lend(g, `Lent Test Group (${D})`, alone ? nm : g.byName.Bavaria);
    expect(divisionsOf(g, lent).map((d) => d.name)).toEqual([D]);
    const i = addRow([2, 'GER', D, 'inf', 1, 'Magdeburg', {}]);
    month(g, 2);
    const holders = g.corps.filter((c) => !c.dead && c.nat === 'GER' && divisionsOf(g, c).some((d) => d.name === D));
    return { g, lent, j, i, holders };
  };

  it('switch on, the lent corps held alone beside the enemy: the division is laid ONCE, and stays with the held corps', () => {
    const { g, lent, j, holders } = run(true);
    expect([lent.dead, g.flags[`lent:${j}`], g.flags[`wait:${j}`]]).toEqual([false, lent.id, 2]);
    expect(holders).toEqual([lent]);
    // (The held corps keeps it for contact: the row's claim of it is a hold of the rule's own, on the claim path.)
    expect(ruleHolds(g).filter((h) => h.path === 'claim').map((h) => h.corps)).toEqual([[lent.name]]);
  });

  it('switch on, the lent corps not beside the enemy: its loan ends and the row raises the division, once', () => {
    const { lent, holders } = run(true, false);
    expect(lent.dead).toBe(true);
    expect(holders).toHaveLength(1);
  });

  it('switch off: as the base, the loan ends and the row raises the division, once', () => {
    const { lent, holders } = run(false);
    expect(lent.dead).toBe(true);
    expect(holders).toHaveLength(1);
  });
});

// ---- T3-1: the reader counts the player's own hold that was not told ------------------------------------------------

describe("T3-1: ruleHolds counts the player's own hold that was never told as untold", () => {
  it("his own theatre draw passes over a corps the test holds: no sentence exists, so it is told false, and untold", () => {
    clear();
    const g = startOpen(createGame(1, null));
    setFromWait(g, true);
    g.human = 'GER';
    declareWar(g, 'GER', 'GRE');
    const balkans = g.regions.filter((r) => theatreName(r) === 'the Balkans');
    const greek = (r: Region): boolean => r.ctrl === 'GRE' || r.units.some((u) => u.nat === 'GRE');
    const line = balkans.find((r) => !greek(r) && !atWar(g, 'GER', r.ctrl) && r.nb.some((j) => greek(g.regions[j])))!;
    // (R4-1: the Greek armies beside it start under their orders to hold where they stand, and an enemy so bound is not the
    // enemy beside for the alone half; their orders are lifted here, so that they are free to come on, as this board means.)
    for (const j of line.nb) for (const u of g.regions[j].units) if (u.nat === 'GRE') u.hold = false;
    for (const u of [...line.units]) if (u.nat === 'GER') relocate(g, u, g.byName.Bavaria);
    const held = add(g, 'Held Test Corps', line, 3);
    const i = addRow([2, 'GER', 'Test Theatre Army', 'inf', 1, 'Magdeburg', { from: 'the Balkans' }]);
    month(g, 2);
    expect(held.divs).toBe(3);
    expect(ruleHolds(g).filter((h) => h.path === 'draw').map(({ turn, ...h }) => h)).toEqual([{ nat: 'GER', i, row: 'Test Theatre Army', path: 'draw', corps: ['Held Test Corps'], waits: false, written: null, told: false, untold: true }]);
  });
});

// ---- T3-2: two loans ending in one pass are no relief to each other -------------------------------------------------

describe('T3-2: the lent corps whose loans end in the same pass keep nothing for each other', () => {
  // (Round 4, R4-2: the design director's (2) of 16:07Z, 27 November 2026, "read in row order, since that is a fixed
  // order of the data and never a throw. A loan already held this pass is not leaving for the rows after it." Finding
  // S1-2. So the first in row order stays, held, and every one read after it goes.)
  it('two lent corps alone together beside the enemy, both loans over this month: EXACTLY the first in row order stays', () => {
    const { g, nm } = board();
    const a = lend(g, 'Lent Test Corps A', nm);
    const b = lend(g, 'Lent Test Corps B', nm);
    expect(a.j).toBeLessThan(b.j);
    month(g, 2);
    expect([a.c.dead, a.c.reg, g.flags[`lent:${a.j}`], g.flags[`wait:${a.j}`]]).toEqual([false, nm.id, a.c.id, 2]);
    expect([b.c.dead, g.flags[`lent:${b.j}`]]).toEqual([true, -1]);
    expect(ruleHolds(g).filter((h) => h.path.startsWith('loan')).map((h) => [h.i, h.corps])).toEqual([[a.j, [a.c.name]]]);
  });

  it('three lent corps alone together beside the enemy: the first stays, the other two go', () => {
    const { g, nm } = board();
    const ls = ['A', 'B', 'C'].map((k) => lend(g, `Lent Test Corps ${k}`, nm));
    month(g, 2);
    expect(ls.map((l) => [l.c.dead, g.flags[`lent:${l.j}`]])).toEqual([[false, ls[0].c.id], [true, -1], [true, -1]]);
    expect(ruleHolds(g).filter((h) => h.path.startsWith('loan')).map((h) => h.i)).toEqual([ls[0].j]);
  });

  it('switch off: all three go, as the base', () => {
    const { g, nm } = board(false);
    const ls = ['A', 'B', 'C'].map((k) => lend(g, `Lent Test Corps ${k}`, nm));
    month(g, 2);
    expect(ls.map((l) => [l.c.dead, g.flags[`lent:${l.j}`]])).toEqual([[true, -1], [true, -1], [true, -1]]);
  });

  it('a row read after a held loan finds the held corps no leaver: the telling reads the same set as the rule', () => {
    const { g, nm } = board();
    const a = lend(g, 'Lent Test Corps A', nm);
    const b = lend(g, 'Lent Test Corps B', nm);
    const seen: [number, string[]][] = [];
    leavingWatch.tell = (g2, nat, i, leaving) => {
      if (nat !== 'GER') return;
      const tell = rowLeaving(g2, nat, i);
      expect([...tell].map((x) => x.name).sort()).toEqual([...leaving].map((x) => x.name).sort());
      seen.push([i, [...leaving].map((x) => x.name).filter((n) => n.startsWith('Lent Test')).sort()]);
    };
    const r = addRow([2, 'GER', 'Late Test Row', 'inf', 1, 'Magdeburg', {}]);
    try { month(g, 2); } finally { leavingWatch.tell = null; }
    expect(a.c.dead).toBe(false);
    expect(b.c.dead).toBe(true);
    // (The row read after both: A was held, so it is not leaving; B went, so it is.)
    const late = seen.find(([i]) => i === r);
    expect(late?.[1]).toEqual(['Lent Test Corps B']);
  });
});

// ---- T3-3: a stand-in that stands where the row forms is not held -------------------------------------------------

describe('T3-3: a stand-in in contact in the province the row forms in is not held: the row lands there', () => {
  it('the stand-in alone in the Neumark beside Poland, the row forming in the Neumark: it lands that month, no hold noted', () => {
    const { g, nm } = board();
    const stand = add(g, 'Stand-in Test Corps', nm, 2);
    const j = addRow([1, 'GER', 'Stand-in Test Corps', 'inf', 2, 'Neumark', { unless: W }]);
    g.flags[`sent:${j}`] = 1;
    const i = addRow([2, 'GER', W, 'inf', 2, 'Neumark', {}]);
    month(g, 2);
    expect(g.flags[`wait:${i}`]).toBeUndefined();
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(stand.dead).toBe(true);
    expect(ruleHolds(g).filter((h) => h.path === 'standDown')).toEqual([]);
  });
});

// ---- S1-9 (a): a lent corps that would stand on at its loan's end is not held ---------------------------------------

describe("S1-9 (a): a lent corps whose loan's end would leave it standing (`rowOver`) is not held, and is told nothing", () => {
  // (Its one division drawn from a corps no longer on the board: at the loan's end it stands on where it is, as the
  // base has it, so nothing leaves the Neumark and no hold is needed; holding it would tell him it goes back.)
  const run = (on: boolean): { g: Game; c: Corps; j: number } => {
    const { g, nm } = board(on);
    const { c, j } = lend(g, 'Lent Test Stand Corps', nm);
    const [d0] = divisionsOf(g, c);
    c.drew = [{ name: [d0.name, d0.short], no: d0.no, from: 987654 }];
    month(g, 2);
    return { g, c, j };
  };

  it('switch on, alone beside the enemy: the loan ends as the base ends it, the corps stands on, and no hold is noted or told', () => {
    const { g, c, j } = run(true);
    expect([c.dead, c.reg, g.flags[`lent:${j}`], g.flags[`wait:${j}`], g.flags[`rowTold:${j}`]]).toEqual([false, g.byName.Neumark.id, -1, undefined, undefined]);
    expect(ruleHolds(g).filter((h) => h.path.startsWith('loan'))).toEqual([]);
    expect(g.headlines.filter((h: any) => /Lent Test Stand Corps/.test(h.text))).toEqual([]);
  });

  it('switch off: the same, as the base', () => {
    const { g, c, j } = run(false);
    expect([c.dead, c.reg, g.flags[`lent:${j}`], g.flags[`wait:${j}`]]).toEqual([false, g.byName.Neumark.id, -1, undefined]);
  });

  it("a row lent for a war that is not on (Germany and the United States): the corps stands on, as the base has it, and is not held", () => {
    for (const on of [true, false]) {
      const { g, nm } = board(on);
      const c = add(g, 'Lent Test War Corps', nm);
      const j = addRow([1, 'GER', 'Lent Test War Corps', 'inf', 1, 'Neumark', { war: ['GER', 'USA'] }]);
      g.flags[`sent:${j}`] = 1;
      g.flags[`lent:${j}`] = c.id;
      const [d0] = divisionsOf(g, c);
      c.drew = [{ name: [d0.name, d0.short], no: d0.no, from: 987654 }];
      month(g, 2);
      expect([on, c.dead, c.reg, g.flags[`lent:${j}`], g.flags[`wait:${j}`]]).toEqual([on, false, nm.id, -1, undefined]);
      expect(ruleHolds(g)).toEqual([]);
      clear();
    }
  });
});
