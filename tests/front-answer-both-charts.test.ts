// Held armies answer an attack by land with both switches on: the far side's and Units decide (the design director's
// B11 of 25 November 2026 and its ruling of 14:15Z, audit item 38, "B11's one rule on both charts, behind Units
// decide, stops 3 to 5", design-director/pacific-japan-1125.md).
//
// The far side reads the rule at every stop with its switch on, and that is unchanged by Units decide: its rows, and
// the ground each is tied to, are the same with Units decide on or off. The old chart reads it under Units decide at
// the stops that keep no book. A front with a foot on each chart is read by neither, as before, and a row is tied only
// to ground of its own front's chart. The skeptic's finding of 25 November 2026: with both switches on, the Saharan
// front against Italy, read whole, tied France's Tunisia Command at Mareth to far-side ground as far as El Fasher.
// (Whether such a front is one front to both charts is put to the design director.)
//
// Each chart frees its rows by its own trigger (the design director's ruling of 15:09Z of 25 November 2026, "Held rows
// on the old chart: four readings before the gate", point 1): the far side keeps its built one, the war itself on the
// front that faces that enemy; the old chart reads the form's cause, an enemy at war with the row's country holding
// ground of its side on the row's own province or beside it.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { answerWatch, withAnswer } from '../src/campaign/ai';
import { clientMonth } from '../src/campaign/far';
import { addCorps, allied, atWar, createGame, relocate, setWar } from '../src/campaign/game';
import { heldFirst } from '../src/campaign/far';
import { setStop, stopDef } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';
import { setUnitsDecide, unitsDecide } from '../src/campaign/units';
import { farOn, shoreOf } from '../src/campaign/world';

/** A war nobody plays, opened in September 1939 at the stop named, both switches on: Japan and Manchukuo at war with the Soviet Union, Italy with France and Britain, a German army in Champagne. */
const war = (stop: number): Game => {
  const g = startOpen(createGame(1, null));
  expect(setUnitsDecide(g, true)).toBe(true);
  if (stop !== 3) expect(setStop(g, stop, true)).toBe(true);
  expect([farOn(g), unitsDecide(g), !!stopDef(g).book]).toEqual([true, true, stop <= 2]);
  setWar(g, 'JAP', 'SOV');
  clientMonth(g, true);
  setWar(g, 'FRA', 'ITA');
  setWar(g, 'UK', 'ITA');
  const r = g.byName['Champagne'];
  for (const u of [...r.units]) if (u.nat !== 'GER') relocate(g, u, g.byName['Paris']);
  r.prev = r.ctrl; r.ctrl = 'GER'; r.since = g.turn;
  addCorps(g, 'GER', 'Test army in Champagne', 'inf', 4, r.id, {});
  return g;
};

/** Every held row that answers this month, each nation's as its staff's month reads it, with the ground it is tied to (by name). */
const read = (g: Game): Map<string, string[]> => {
  const out = new Map<string, string[]>();
  answerWatch.tell = (e) => { for (const x of e.rows) out.set(`${x.corps.nat} ${x.corps.name} @ ${g.regions[x.corps.reg].name}`, [...x.on].map((id) => g.regions[id].name).sort()); };
  try { for (const nat of Object.keys(g.nations)) if (g.nations[nat].alive) withAnswer(g, nat, () => 0); } finally { answerWatch.tell = null; }
  return out;
};
/** The same, read as if Units decide were off on the same position. */
const readOff = (g: Game): Map<string, string[]> => {
  delete g.flags.unitsDecide;
  try { return read(g); } finally { g.flags.unitsDecide = 1; }
};
const onFar = (g: Game, key: string): boolean => shoreOf(g.byName[key.slice(key.lastIndexOf(' @ ') + 3)]) === 2;
const farOnly = (g: Game, m: Map<string, string[]>): [string, string[]][] => [...m].filter(([k]) => onFar(g, k)).sort();
/** Every held row that answers this month, as `read` reads them, as corps. */
const rowsOf = (g: Game): Corps[] => {
  const out: Corps[] = [];
  answerWatch.tell = (e) => { out.push(...e.rows.map((x) => x.corps)); };
  try { for (const nat of Object.keys(g.nations)) if (g.nations[nat].alive) withAnswer(g, nat, () => 0); } finally { answerWatch.tell = null; }
  return out;
};
/** True where an enemy at war with the row's country holds (the field of) ground of its side on its province or beside it. */
const entered = (g: Game, c: Corps): boolean => [c.reg, ...g.regions[c.reg].nb].some((id) => {
  const r = g.regions[id];
  return (atWar(g, c.nat, r.ctrl) || r.units.some((u) => !u.dead && atWar(g, c.nat, u.nat))) && (allied(g, c.nat, r.owner) || allied(g, c.nat, heldFirst(r)));
});

describe('held armies answer an attack by land with the far switch and Units decide both on', () => {
  it('stop 3: each chart answers by its own rule; no row is tied across the line between the charts; the far side is as with Units decide off', () => {
    const g = war(3);
    const both = read(g), off = readOff(g);
    // The old chart answers (the 2nd Army beside lost Champagne), and so does the far side (the Soviet Far East).
    expect([...both.keys()].some((k) => k.startsWith('FRA 2nd Army'))).toBe(true);
    expect(farOnly(g, both).some(([k]) => k.startsWith('SOV '))).toBe(true);
    // Every row's ground lies on the chart it stands on: Tunisia Command at Mareth is never tied to the Sahara's far
    // side, nor a far-side row to the old chart.
    for (const [k, on] of both) {
      const far = onFar(g, k);
      expect([k, on.filter((nm) => (shoreOf(g.byName[nm]) === 2) !== far)]).toEqual([k, []]);
    }
    // The far side's rows and their ground are those it has with Units decide off.
    expect(farOnly(g, both)).toEqual(farOnly(g, off));
    // With Units decide off nothing answers on the old chart.
    expect([...off.keys()].filter((k) => !onFar(g, k))).toEqual([]);
  });

  it('stop 3: the old chart reads the cause, the far side its own trigger as built', () => {
    const g = war(3);
    const rows = rowsOf(g);
    const old = rows.filter((c) => shoreOf(c.reg) !== 2), far = rows.filter((c) => shoreOf(c.reg) === 2);
    // The old chart: only rows on or beside ground of their side an enemy holds (the 2nd Army, the XVIII Corps and the
    // Paris garrison beside Champagne). Not the 3rd and 4th Armies, on the front with Germany but beside no lost ground.
    expect(old.length).toBeGreaterThan(0);
    for (const c of old) expect([c.nat, c.name, entered(g, c)]).toEqual([c.nat, c.name, true]);
    expect(old.map((c) => c.name)).toEqual(expect.arrayContaining(['2nd Army', 'XVIII Corps', 'Paris garrison']));
    expect(old.map((c) => c.name)).not.toContain('3rd Army');
    expect(old.map((c) => c.name)).not.toContain('4th Army');
    // The far side: the war itself frees the holds on the front that faces that enemy, before he has crossed. The Soviet
    // Far East's rows answer with no enemy on or beside them.
    expect(far.length).toBeGreaterThan(0);
    expect(far.filter((c) => c.nat === 'SOV' && !entered(g, c)).length).toBeGreaterThan(0);
  });

  for (const stop of [1, 2]) {
    it(`stop ${stop}, the book's: nothing answers on the old chart; the far side answers as with Units decide off`, () => {
      const g = war(stop);
      const both = read(g);
      expect([...both.keys()].filter((k) => !onFar(g, k))).toEqual([]);
      expect(farOnly(g, both).length).toBeGreaterThan(0);
      expect(farOnly(g, both)).toEqual(farOnly(g, readOff(g)));
    });
  }
});
