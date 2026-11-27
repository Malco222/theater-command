// The chain by the maker's own state (the design director's ruling of 06:10Z, 26 November 2026, CHAIN: "The chain
// asks whether its maker is still to come, read from the maker's own state (due, unsent, not struck, no living corps
// of its name), whatever holds it. When the maker is struck by the rows' existing end, the chained row goes through
// and the division exists once. No new count. ... Test: seed 34 lays the 62nd once."). Japan's 32nd Army (62nd
// Division) row (month 60, Okinawa) takes the 12th Army (62nd Division), which the 12th Army's row (month 55, Kaifeng)
// makes. As first built the chain read only a wait the maker had written, and a maker held because its place is in
// enemy hands writes none: the Okinawa row came from nobody and laid the 62nd, and the Kaifeng row, its place
// retaken, could come after it. Now the chained row waits while its maker is due and unsent, whatever holds it.
// And her answer 7 of 12:48Z (the guard): a maker that comes due only AFTER its chained row went through lays no
// second division. Section 13 A4 of the design: the maker's own corps stands where no enemy is beside it, so the hold
// that keeps the maker is its place (no wait written). The states are set by hand; no whole war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { divisionsOf } from '../src/campaign/armies';
import { setFromWait } from '../src/campaign/arrivalwaits';
import { corpsInContact } from '../src/campaign/drawn';
import { addCorps, createGame, declareWar, postedFor, putToSea, relocate, rowComes, rowHolds, startPhase } from '../src/campaign/game';
import { setStop, stopOf } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';

const MAKER = '12th Army (62nd Division)', CHAINED = '32nd Army (62nd Division)', TAIYUAN = '1st Army (62nd Division)';
const BRIGADES = '1st Army (independent brigades, Taiyuan)';
const rows = (setup as any).expeditions as any[][];
const index = (name: string, nat = 'JAP'): number => rows.findIndex((r) => r[1] === nat && r[2] === name);
const M = index(MAKER), C = index(CHAINED), T = index(TAIYUAN);

/**
 * Seed 34, the far side on, stop 3. Kaifeng held by `kaifeng` with its own corps moved to Hsuchow (Kaifeng is on no
 * named front: no fall-back harbour); the maker's own corps (the independent brigades) moved to Tokyo, where no enemy
 * is beside it (A4); Japan at war with America (the chained row's war); the maker due from `dueM`.
 */
const board = (on: boolean, kaifeng: 'CHN' | 'JAP', dueM = 55): Game => {
  const g = startOpen(createGame(34, null));
  setStop(g, 3, true);
  if (on) setFromWait(g, true);
  const k = g.byName.Kaifeng;
  for (const u of [...k.units]) relocate(g, u, g.byName.Hsuchow);
  k.ctrl = kaifeng;
  relocate(g, brigades(g)!, g.byName.Tokyo);
  declareWar(g, 'JAP', 'USA');
  g.flags[`due:${M}`] = dueM;
  return g;
};
const brigades = (g: Game): Corps | undefined => g.corps.find((x) => x.nat === 'JAP' && !x.dead && x.name === BRIGADES);
/** The living corps of Japan, on the board or on a long passage, that hold the 62nd as a whole division. */
const holders = (g: Game): string[] => [...new Set([...g.corps, ...(g.atSea ?? []).map((v) => v.corps)])]
  .filter((c) => c.nat === 'JAP' && !c.dead && !(c.full !== undefined && c.full < 1) && divisionsOf(g, c).some((d) => /^62nd\b/.test(d.name))).map((c) => c.name).sort();
type Month = { t: number; sentM: number | null; waitM: number | null; sentC: number | null; waitC: number | null; holders: string[] };
/** Japan's months `from` to `to`, each read at its end; `before` runs at the start of a month. */
const months = (g: Game, from: number, to: number, before?: (t: number) => void): Month[] => {
  const out: Month[] = [];
  for (let t = from; t <= to; t++) {
    before?.(t);
    g.turn = t;
    startPhase(g, 'JAP');
    out.push({ t, sentM: g.flags[`sent:${M}`] ?? null, waitM: g.flags[`wait:${M}`] ?? null, sentC: g.flags[`sent:${C}`] ?? null, waitC: g.flags[`wait:${C}`] ?? null, holders: holders(g) });
  }
  return out;
};
/** The first month a row is sent or struck, or null. */
const sentIn = (ms: Month[], key: 'sentM' | 'sentC'): number | null => ms.find((m) => m[key] !== null)?.t ?? null;
/** The invariant of every month's end: the 62nd held as a whole division by one living corps at most, and the chained row never sent before its maker unless the maker was struck. */
const invariant = (ms: Month[]): void => {
  for (const m of ms) expect(m.holders.length, `month ${m.t}: ${m.holders.join(', ')}`).toBeLessThanOrEqual(1);
  const sm = sentIn(ms, 'sentM'), sc = sentIn(ms, 'sentC');
  if (sc !== null && ms.find((m) => m.t === sm)?.sentM !== 2) expect(sm !== null && sm <= sc, `chained row sent in ${sc}, maker in ${sm}`).toBe(true);
};

describe('the data: the three rows of the 62nd', () => {
  it('the Okinawa row takes the Kaifeng row\'s corps, which is made of the brigades and of the Taiyuan row\'s corps (stops 1 and 2)', () => {
    expect(rows[M].slice(0, 6)).toEqual([55, 'JAP', MAKER, 'inf', 1, 'Kaifeng']);
    expect(rows[M][6]).toMatchObject({ from: [BRIGADES, TAIYUAN], war: ['JAP', 'CHN'] });
    expect(rows[C].slice(0, 6)).toEqual([60, 'JAP', CHAINED, 'inf', 1, 'Okinawa']);
    expect(rows[C][6]).toMatchObject({ from: [MAKER], war: ['JAP', 'USA'] });
    expect(rows[C][6].adds ?? 0).toBe(0);
    expect(rows[T][6]).toMatchObject({ stops: [1, 2] });
  });
});

describe('switch on: the chained row waits while its maker is due, whatever holds it', () => {
  it('month 60: the maker, held by its place, writes no wait and is due; the chained row waits on it', () => {
    const g = board(true, 'CHN');
    expect(stopOf(g)).toBe(3);
    // (A4: the maker's own corps is not at the front, and the Taiyuan row it is also made of never comes at stop 3.)
    expect(corpsInContact(g, 'JAP', brigades(g)!)).toBe(false);
    const ms = months(g, 60, 60);
    expect(ms[0]).toMatchObject({ sentM: null, waitM: null, sentC: null, waitC: 60 });
    expect(rowComes(g, 'JAP', M)).toBe('due');
    expect(rowComes(g, 'JAP', T)).toBe('notYet');
    expect(holders(g)).toEqual([]);
    // (Its place is what holds it: the same board with Kaifeng in Japan's hands forms it that month.)
    const h = board(true, 'JAP');
    expect(months(h, 60, 60)[0].sentM).toBe(1);
  });

  it('Kaifeng never retaken: the maker is struck in month 62 by the rows\' existing end, the chained row comes then, and the 62nd stands once', () => {
    const g = board(true, 'CHN');
    const ms = months(g, 60, 66);
    expect(ms.every((m) => m.waitM === null)).toBe(true);
    expect(ms.map((m) => m.sentM)).toEqual([null, null, 2, 2, 2, 2, 2]);
    expect(sentIn(ms, 'sentC')).toBe(62);
    expect(ms.every((m) => m.sentC === null || m.sentC === 1)).toBe(true);
    expect(ms.filter((m) => m.t < 62).map((m) => m.waitC)).toEqual([60, 61]);
    expect(ms[ms.length - 1].holders).toEqual([CHAINED]);
    invariant(ms);
  });

  it('Kaifeng given back to Japan in month 61: the maker comes first, the chained row never before it, and the 62nd stands once', () => {
    const g = board(true, 'CHN');
    const ms = months(g, 60, 66, (t) => { if (t === 61) g.byName.Kaifeng.ctrl = 'JAP'; });
    expect(sentIn(ms, 'sentM')).toBe(61);
    expect(ms.find((m) => m.t === 61)!.sentM).toBe(1);
    const sc = sentIn(ms, 'sentC');
    expect(sc === null || sc >= 61).toBe(true);
    expect(ms[ms.length - 1].holders).toHaveLength(1);
    invariant(ms);
  });

  it('at stop 3 the Taiyuan row never comes, so it holds nobody: the maker is not held by the chain', () => {
    const g = board(true, 'CHN');
    g.turn = 60;
    expect(rowComes(g, 'JAP', T)).toBe('notYet');
    expect(g.corps.some((x) => x.nat === 'JAP' && !x.dead && x.name === TAIYUAN)).toBe(false);
    expect(rowHolds(g, 'JAP', rows[M][6].from, g.byName.Kaifeng.id, M).unformed).toEqual([]);
    // (And the maker, due with no corps of its name, holds the chained row.)
    expect(rowHolds(g, 'JAP', rows[C][6].from, g.byName.Okinawa.id, C).unformed).toEqual([MAKER]);
    const ms = months(g, 60, 62);
    expect(ms.every((m) => m.waitM === null)).toBe(true);
  });
});

describe("the guard (her answer 7): a maker that comes due after its chained row went through lays no second division", () => {
  it('the maker due from month 63, Kaifeng Japan\'s: the chained row comes in month 60, the maker in 63, and the 62nd stands once', () => {
    const g = board(true, 'JAP', 63);
    g.turn = 60;
    expect(rowComes(g, 'JAP', M)).toBe('notYet');
    const ms = months(g, 60, 66);
    expect(sentIn(ms, 'sentC')).toBe(60);
    expect(ms.filter((m) => m.t < 63).map((m) => m.holders)).toEqual([[CHAINED], [CHAINED], [CHAINED]]);
    expect(sentIn(ms, 'sentM')).toBe(63);
    for (const m of ms) expect(m.holders, `month ${m.t}`).toHaveLength(1);
  });

  it('the maker due from month 63 with Kaifeng China\'s until month 64: held by its place, then come, and the 62nd stands once', () => {
    const g = board(true, 'CHN', 63);
    const ms = months(g, 60, 66, (t) => { if (t === 64) g.byName.Kaifeng.ctrl = 'JAP'; });
    expect(sentIn(ms, 'sentC')).toBe(60);
    expect(ms.every((m) => m.waitM === null)).toBe(true);
    expect(sentIn(ms, 'sentM')).toBe(64);
    for (const m of ms) expect(m.holders, `month ${m.t}`).toHaveLength(1);
  });
});

describe('a row whose own name is in its list is never held by itself', () => {
  it("Hong Kong's China Command and Formosa's 4th Tank Regiment, due with no corps of their name: the chain names nobody", () => {
    const g = startOpen(createGame(34, null));
    setFromWait(g, true);
    for (const [nat, name] of [['UK', 'China Command (Hong Kong garrison)'], ['JAP', '4th Tank Regiment']] as const) {
      const i = index(name, nat);
      expect(rows[i][6].from).toContain(name);
      g.turn = rows[i][0];
      for (const x of g.corps.filter((y) => y.nat === nat && y.name === name)) x.dead = true;
      expect(rowComes(g, nat, i)).toBe('due');
      expect(rowHolds(g, nat, [name], undefined, i).unformed).toEqual([]);
      // (Read as another row's maker, the same row holds: the exclusion is the row's own index alone.)
      expect(rowHolds(g, nat, [name], undefined, -1).unformed).toEqual([name]);
    }
  });
});

describe("the maker's own state, read as expeditions reads it", () => {
  let added = 0;
  const addRow = (row: any[]): number => { rows.push(row); added++; return rows.length - 1; };
  afterEach(() => { while (added > 0) { rows.pop(); added--; } });

  it("ruling B7: a maker posted on ground of a country Japan is at peace with waits for that war: 'notYet', and it holds nobody; the same ground Japan's, it is due and holds", () => {
    const g = board(true, 'CHN');
    g.turn = 60;
    // (A province beyond Europe held by a country Japan is neither allied with nor at war with, which the maker's row,
    // taking nobody and keyed to no war, is posted to: `postedFor` says it waits for that war.)
    const maker = (at: string): any[] => [50, 'JAP', 'Test Maker Corps', 'inf', 1, at, {}];
    const place = g.regions.find((r) => {
      if (!r.ctrl || r.ctrl === 'JAP' || postedFor(g, 'JAP', maker(r.name), r)?.waits !== true) return false;
      // (Nothing else of the maker's gates keeps it: with the same ground Japan's it is due.)
      const was = r.ctrl;
      r.ctrl = 'JAP';
      const j = addRow(maker(r.name));
      const due = rowComes(g, 'JAP', j) === 'due';
      rows.pop(); added--;
      r.ctrl = was;
      return due;
    })!;
    expect(place).toBeDefined();
    const j = addRow(maker(place.name));
    const i = addRow([50, 'JAP', 'Test Chained Army', 'inf', 1, 'Tokyo', { from: ['Test Maker Corps'] }]);
    expect(rowComes(g, 'JAP', j)).toBe('notYet');
    expect(rowHolds(g, 'JAP', ['Test Maker Corps'], g.byName.Tokyo.id, i).unformed).toEqual([]);
    place.ctrl = 'JAP';
    expect(rowComes(g, 'JAP', j)).toBe('due');
    expect(rowHolds(g, 'JAP', ['Test Maker Corps'], g.byName.Tokyo.id, i).unformed).toEqual(['Test Maker Corps']);
  });

  it('a corps of the maker\'s name on a long passage is living: the chained row is not held by the maker', () => {
    const g = board(true, 'CHN');
    g.turn = 60;
    expect(rowComes(g, 'JAP', M)).toBe('due');
    expect(rowHolds(g, 'JAP', rows[C][6].from, g.byName.Okinawa.id, C).unformed).toEqual([MAKER]);
    const x = addCorps(g, 'JAP', MAKER, 'inf', 1, g.byName.Tokyo.id, {});
    putToSea(g, x, g.byName.Okinawa, 3);
    expect(g.corps.includes(x)).toBe(false);
    expect(g.atSea?.some((v) => v.corps === x)).toBe(true);
    expect(rowComes(g, 'JAP', M)).toBe('due');
    expect(rowHolds(g, 'JAP', rows[C][6].from, g.byName.Okinawa.id, C).unformed).toEqual([]);
  });
});

describe('switch off: the same boards as 2a7564b7b gives', () => {
  // (Recorded once, 26 November 2026, by the same three boards on 2a7564b7b's own tree: [month, sent:M, wait:M, sent:C,
  // wait:C, holders]. With the switch off the chain is not read, and the Okinawa row comes in month 60.)
  const HEAD: Record<string, unknown[]> = {
    N: [[60, null, null, 1, null, [CHAINED]], [61, null, null, 1, null, [CHAINED]], [62, 2, null, 1, null, [CHAINED]], [63, 2, null, 1, null, [CHAINED]], [64, 2, null, 1, null, [CHAINED]], [65, 2, null, 1, null, [CHAINED]], [66, 2, null, 1, null, [CHAINED]]],
    R: [[60, null, null, 1, null, [CHAINED]], [61, 1, null, 1, null, [MAKER]], [62, 1, null, 1, null, [MAKER]], [63, 1, null, 1, null, [MAKER]], [64, 1, null, 1, null, [MAKER]], [65, 1, null, 1, null, [MAKER]], [66, 1, null, 1, null, [MAKER]]],
    late: [[60, null, null, 1, null, [CHAINED]], [61, null, null, 1, null, [CHAINED]], [62, null, null, 1, null, [CHAINED]], [63, 1, null, 1, null, [MAKER]], [64, 1, null, 1, null, [MAKER]], [65, 1, null, 1, null, [MAKER]], [66, 1, null, 1, null, [MAKER]]],
  };
  const flat = (ms: Month[]): unknown[] => ms.map((m) => [m.t, m.sentM, m.waitM, m.sentC, m.waitC, m.holders]);
  it('Kaifeng never retaken', () => { const g = board(false, 'CHN'); expect(flat(months(g, 60, 66))).toEqual(HEAD.N); });
  it('Kaifeng given back in month 61', () => { const g = board(false, 'CHN'); expect(flat(months(g, 60, 66, (t) => { if (t === 61) g.byName.Kaifeng.ctrl = 'JAP'; }))).toEqual(HEAD.R); });
  it('the maker due from month 63', () => { const g = board(false, 'JAP', 63); expect(flat(months(g, 60, 66))).toEqual(HEAD.late); });
});
