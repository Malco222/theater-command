// Churned ground's record (release 2, churned.ts; the design director's rulings of 27 November 2026, 19:36Z and 21:05Z,
// on map-looks/fronts-alive/churn-healing-row-FORM.md): a record for the 3D map at the month's end, never a rule. Off it
// writes nothing; on, its months are facts, uncapped; a stand ends on its three causes and heals for the art director's
// year; the player's picture moves only on what he can know; it is saved and read back whole.
// @ts-ignore node's own module: the tree's type check loads three's types only
import { readFileSync, readdirSync, statSync } from 'node:fs';
// @ts-ignore node's own module: the tree's type check loads three's types only
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import { CHURN, churnMonth, churnedOn, churnedView, setChurned } from '../src/campaign/churned';
import { atWar, createGame } from '../src/campaign/game';
import { setFog, sightOf } from '../src/campaign/intel';
import { loadGame, saveGame } from '../src/campaign/save';
import { fieldCtrl } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { stepAI } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;

const lord = (g: Game, id: number): string => fieldCtrl(g, g.regions[id]);
const side = (g: Game, x: number, foe: string): Corps[] =>
  g.regions[x].units.filter((c) => !c.dead && c.kind !== 'fort' && g.nations[c.nat] && g.nations[foe] && atWar(g, c.nat, foe));
/** A pair of neighbouring provinces whose holders are at war, with corps on both sides; the first holder named. */
function facing(g: Game, first = 'GER', second?: string): [number, number] {
  for (const r of g.regions) {
    for (const j of r.nb) {
      const la = lord(g, r.id), lb = lord(g, j);
      if (la !== first || (second && lb !== second) || la === lb || !g.nations[lb] || !atWar(g, la, lb)) continue;
      if (side(g, r.id, lb).length && side(g, j, la).length) return [r.id, j];
    }
  }
  throw new Error('no front');
}
function dig(g: Game, x: number, foe: string, on: boolean, months = 1): void {
  for (const c of side(g, x, foe)) {
    if (on) { c.dug = true; c.dugFor = months; } else { delete c.dug; delete c.dugFor; }
  }
}
const row = (g: Game, a: number, b: number) => g.churned?.find((r) => r.a === Math.min(a, b) && r.b === Math.max(a, b));
/** One month's end as far as the record is concerned. */
function month(g: Game): void {
  churnMonth(g);
  g.turn++;
}
function war(human: string | null, seed = 17): Game {
  const g = startOpen(createGame(seed, human));
  if (human) setFog(g, false);
  return g;
}

describe('the switch', () => {
  it('off writes nothing, and the save is as it was', () => {
    const g = war('GER');
    const [a, b] = facing(g);
    dig(g, a, lord(g, b), true, 3);
    dig(g, b, lord(g, a), true, 3);
    const before = saveGame(g);
    churnMonth(g);
    expect(g.churned).toBeUndefined();
    expect(saveGame(g)).toBe(before);
  });

  it('off after on deletes the record and the switch', () => {
    const g = war('GER');
    const [a, b] = facing(g);
    dig(g, a, lord(g, b), true, 3);
    dig(g, b, lord(g, a), true, 3);
    const before = saveGame(g);
    expect(setChurned(g, true)).toBe(true);
    churnMonth(g);
    expect(g.churned!.length).toBeGreaterThan(0);
    expect(setChurned(g, false)).toBe(false);
    expect(g.churned).toBeUndefined();
    expect(g.flags.churned).toBeUndefined();
    expect(saveGame(g)).toBe(before);
  });

  it('the grand campaign only', () => {
    const g = createGame(17, 'GER');
    expect(setChurned(g, true)).toBe(false);
    expect(churnedOn(g)).toBe(false);
  });

  it('TC.churnSwitch lays it from the map switch, and the view carries the list only when on', () => {
    const g = war('GER');
    TC().load(saveGame(g));
    const wire = (v: unknown): Record<string, unknown> => JSON.parse(JSON.stringify(v)); // (as the table reads it)
    expect('churned' in wire(TC().view())).toBe(false);
    const on = TC().churnSwitch(true) as { churned?: { year: number; kept: boolean; pairs: number[] } } | null;
    expect(on?.churned).toEqual({ year: CHURN.year, kept: false, pairs: [] });
    expect(TC().churnSwitch(true)).toBeNull();
    const off = TC().churnSwitch(false) as Record<string, unknown> | null;
    expect(off).not.toBeNull();
    expect('churned' in wire(off)).toBe(false);
    expect(TC().churnSwitch(false)).toBeNull();
  });

  it('the month end keeps it (turn.ts), nobody seated', () => {
    const g = war(null);
    setChurned(g, true);
    const t0 = g.turn;
    for (let k = 0; k < 400 && g.turn === t0; k++) stepAI(g);
    expect(g.turn).toBe(t0 + 1);
    expect(Array.isArray(g.churned)).toBe(true);
  });
});

describe('the record: facts', () => {
  it('counts each side\'s months, uncapped, the first from the works months already kept', () => {
    const g = war('GER');
    setChurned(g, true);
    const [a, b] = facing(g);
    const la = lord(g, a), lb = lord(g, b);
    dig(g, a, lb, true, 4);
    dig(g, b, la, true, 2);
    month(g);
    expect(row(g, a, b)).toMatchObject({ ma: a < b ? 4 : 2, mb: a < b ? 2 : 4, pm: 2 });
    for (let k = 0; k < 9; k++) month(g);
    const r = row(g, a, b)!;
    expect(Math.min(r.ma, r.mb)).toBe(11);
    expect(r.pm).toBe(11);
  });

  it('a stand ends where a side stops digging, and its months become a scar', () => {
    const g = war('GER');
    setChurned(g, true);
    const [a, b] = facing(g);
    dig(g, a, lord(g, b), true, 1);
    dig(g, b, lord(g, a), true, 1);
    for (let k = 0; k < 5; k++) month(g);
    expect(row(g, a, b)!.pm).toBe(5);
    dig(g, b, lord(g, a), false);
    const left = g.turn;
    month(g);
    const r = row(g, a, b)!;
    expect(r.pm).toBe(0);
    expect(r.scars).toEqual([{ m: 5, left }]);
  });

  it('a stand ends at peace', () => {
    const g = war('GER');
    setChurned(g, true);
    const [a, b] = facing(g);
    const la = lord(g, a), lb = lord(g, b);
    dig(g, a, lb, true, 1);
    dig(g, b, la, true, 1);
    for (let k = 0; k < 3; k++) month(g);
    g.nations[la].atWar = g.nations[la].atWar.filter((n) => n !== lb);
    g.nations[lb].atWar = g.nations[lb].atWar.filter((n) => n !== la);
    month(g);
    const r = row(g, a, b)!;
    expect([r.ma, r.mb, r.pm]).toEqual([0, 0, 0]);
    expect(r.scars).toHaveLength(1);
  });

  it('a stand ends when the front moves on (a holder changes), and its counts start again', () => {
    const g = war('GER');
    setChurned(g, true);
    const [a, b] = facing(g);
    dig(g, a, lord(g, b), true, 1);
    dig(g, b, lord(g, a), true, 1);
    for (let k = 0; k < 4; k++) month(g);
    const r0 = row(g, a, b)!;
    r0.lb = r0.lb === 'ITA' ? 'HUN' : 'ITA'; // (the holder at the last month's end was another)
    month(g);
    const r = row(g, a, b)!;
    expect([r.ma, r.mb, r.pm]).toEqual([1, 1, 1]);
    expect(r.scars).toHaveLength(1);
    expect(r.scars![0].m).toBe(4);
  });

  it('a scar heals for the art director\'s year, then it is dropped; the list says how long since', () => {
    const g = war('GER');
    setChurned(g, true);
    const [a, b] = facing(g);
    dig(g, a, lord(g, b), true, 1);
    dig(g, b, lord(g, a), true, 1);
    for (let k = 0; k < 6; k++) month(g);
    dig(g, a, lord(g, b), false);
    dig(g, b, lord(g, a), false);
    month(g);
    const seen = sightOf(g, g.human);
    const at = (): number[] => {
      const v = churnedView(g, seen)!;
      const out: number[] = [];
      for (let k = 0; k < v.pairs.length; k += 4) if (v.pairs[k] === Math.min(a, b) && v.pairs[k + 1] === Math.max(a, b)) out.push(v.pairs[k + 2], v.pairs[k + 3]);
      return out;
    };
    expect(at()).toEqual([6, 1]);
    for (let k = 0; k < CHURN.year - 2; k++) month(g);
    expect(at()).toEqual([6, CHURN.year - 1]);
    month(g);
    expect(at()).toEqual([]);
    expect(row(g, a, b)).toBeUndefined();
    expect(churnedView(g, seen)!.year).toBe(12);
  });

  it('is saved and read back whole', () => {
    const g = war('GER');
    setChurned(g, true);
    const [a, b] = facing(g);
    dig(g, a, lord(g, b), true, 2);
    dig(g, b, lord(g, a), true, 3);
    for (let k = 0; k < 4; k++) month(g);
    dig(g, a, lord(g, b), false);
    month(g);
    const text = saveGame(g);
    const h = loadGame(text)!;
    expect(h.churned).toEqual(g.churned);
    expect(saveGame(h)).toBe(text);
    // (An empty record is kept too: it says the record has been kept.)
    const e = war('GER');
    setChurned(e, true);
    e.churned = [];
    expect(loadGame(saveGame(e))!.churned).toEqual([]);
  });

  it('on, the save is equal less the record and its switch (every reader writes nothing)', () => {
    const g = war('GER');
    const [a, b] = facing(g);
    dig(g, a, lord(g, b), true, 2);
    dig(g, b, lord(g, a), true, 3);
    const off = JSON.parse(saveGame(g));
    setChurned(g, true);
    churnMonth(g);
    const on = JSON.parse(saveGame(g));
    expect(on.churned.length).toBeGreaterThan(0);
    delete on.churned;
    delete on.flags.churned;
    expect(on).toEqual(off);
  });
});

describe('the player\'s picture: only what he can know (fronts\' sight rule)', () => {
  /** A war seated as `human` with the fog on, the two provinces photographed this month (seen whole). */
  function fogged(human: string): { g: Game; a: number; b: number } {
    const g = startOpen(createGame(17, human));
    setFog(g, true);
    setChurned(g, true);
    const [a, b] = facing(g, 'GER', 'POL');
    return { g, a, b };
  }
  const watch = (g: Game, ...ids: number[]): void => {
    g.intel.scout = { ...(g.intel.scout ?? {}) };
    for (const id of ids) g.intel.scout[id] = g.turn;
  };
  const know = (g: Game, cs: Corps[], on: boolean): void => {
    g.intel.known = { ...(g.intel.known ?? {}) };
    for (const c of cs) { if (on) g.intel.known[c.id] = g.turn; else delete g.intel.known[c.id]; }
  };
  /** What crosses to the table, and the picture's months, after a month's end on a copy. */
  // (The picture's months for every pair, and its scars: the record's facts, ma and mb, may hold what he cannot see.)
  function sent(g: Game, change: (h: Game) => void): { pairs: number[]; pm: string[] } {
    const h = loadGame(saveGame(g))!;
    change(h);
    churnMonth(h);
    const pm = (h.churned ?? []).filter((r) => r.pm > 0 || r.scars).map((r) => `${r.a}-${r.b}:${r.pm}:${JSON.stringify(r.scars ?? [])}`);
    return { pairs: churnedView(h, sightOf(h, h.human))!.pairs, pm };
  }

  it('his own front: a hidden enemy\'s dig never moves a sent number; his own side\'s months are sent', () => {
    const { g, a, b } = fogged('GER');
    expect(g.flags.nofog).toBeUndefined();
    const pol = side(g, b, 'GER');
    dig(g, a, 'POL', true, 3);
    watch(g, a, b);
    know(g, pol, false);
    const dugIn = sent(g, (h) => dig(h, b, 'GER', true, 1));
    const notDug = sent(g, (h) => dig(h, b, 'GER', false));
    const dugLong = sent(g, (h) => dig(h, b, 'GER', true, 9));
    expect(dugIn).toEqual(notDug);
    expect(dugLong).toEqual(notDug);
    const v = notDug.pairs;
    const k = v.findIndex((_, i) => i % 4 === 0 && v[i] === Math.min(a, b) && v[i + 1] === Math.max(a, b));
    expect(v.slice(k + 2, k + 4)).toEqual([3, -1]);
  });

  it('his own front: an enemy he knows dug in sends the lesser months; known not dug ends it', () => {
    const { g, a, b } = fogged('GER');
    const pol = side(g, b, 'GER');
    dig(g, a, 'POL', true, 5);
    watch(g, a, b);
    know(g, pol, true);
    const known = sent(g, (h) => dig(h, b, 'GER', true, 2));
    const r = known.pairs;
    const k = r.findIndex((_, i) => i % 4 === 0 && r[i] === Math.min(a, b) && r[i + 1] === Math.max(a, b));
    expect(r.slice(k + 2, k + 4)).toEqual([2, -1]);
    const none = sent(g, (h) => dig(h, b, 'GER', false));
    expect(none.pairs.some((_, i) => i % 4 === 0 && none.pairs[i] === Math.min(a, b) && none.pairs[i + 1] === Math.max(a, b))).toBe(false);
  });

  it('two other countries: only the months he has watched (mv); a hidden side moves nothing', () => {
    const { g, a, b } = fogged('USA');
    const ger = side(g, a, 'POL'), pol = side(g, b, 'GER');
    dig(g, a, 'POL', true, 7);
    dig(g, b, 'GER', true, 7);
    // Watched and known on both sides for three months: mv 1, 2, 3 (from the works his cards show, then counted).
    for (let k = 0; k < 3; k++) {
      watch(g, a, b);
      know(g, [...ger, ...pol], true);
      month(g);
    }
    expect(row(g, a, b)!.pm).toBe(9);
    // A month he cannot see the Polish side: whatever it does, his picture neither grows nor ends.
    watch(g, a, b);
    know(g, ger, true);
    know(g, pol, false);
    const still = sent(g, (h) => dig(h, b, 'GER', true, 7));
    const stopped = sent(g, (h) => dig(h, b, 'GER', false));
    expect(stopped).toEqual(still);
    expect(still.pm).toContain(`${Math.min(a, b)}-${Math.max(a, b)}:9:[]`);
  });

  it('nothing is sent for a pair he cannot see', () => {
    const { g, a, b } = fogged('USA');
    dig(g, a, 'POL', true, 7);
    dig(g, b, 'GER', true, 7);
    churnMonth(g);
    const seen = new Uint8Array(g.regions.length);
    expect(churnedView(g, seen)!.pairs).toEqual([]);
  });
});

describe('a record, never a rule', () => {
  it('no file under src/ but churned.ts and save.ts reads or writes g.churned', () => {
    const files: string[] = [];
    const walk = (d: string): void => {
      for (const f of readdirSync(d)) {
        const p = join(d, f);
        if (statSync(p).isDirectory()) walk(p);
        else if (p.endsWith('.ts')) files.push(p);
      }
    };
    walk(decodeURIComponent(new URL('../src', import.meta.url).pathname));
    const touch = files.filter((p) => /\.churned\b/.test(readFileSync(p, 'utf8'))).map((p) => p.replace(/^.*\/src\//, 'src/')).sort();
    expect(touch).toEqual(['src/campaign/churned.ts', 'src/campaign/save.ts']);
  });
});
