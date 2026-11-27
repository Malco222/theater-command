// An island's room in men, round 2: the design director's rulings of 00:20Z, 27 November 2026 (D1 to D6 and the row's
// formed figure) and the UI director's answers of 00:19Z, 27 November 2026 (U1 to U8), each held by a test that fails
// without it. (src/campaign/islands.ts and its readers.)
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, createGame, removeCorps, retreatTarget, startPhase } from '../src/campaign/game';
import { fits } from '../src/campaign/islands';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';
import { chartDist } from '../src/campaign/wrap';
import setup from '../data/campaign/setup-1939.json';

const ROWS = (setup as unknown as { expeditions: any[][] }).expeditions;
/** The record's two rows for Kwajalein in January 1942 (T27): the Maizuru 2nd landing force and the 61st Guard Unit. */
const KWAJ = [171, 263];
const where = (g: Game, i: number): string[] => g.corps.filter((c) => !c.dead && c.nat === 'JAP' && c.name === ROWS[i][2]).map((c) => g.regions[c.reg].name);
/** Japan's months from T24 to `last`, Japan's phase opening each (its dated rows are read there). */
function months(g: Game, last: number, each: (t: number) => void = () => {}): void {
  for (let t = 24; t <= last; t++) { g.turn = t; startPhase(g, 'JAP'); each(t); }
}

describe('a dated row is never struck for room (the design director, 00:20Z, 27 November 2026, D1)', () => {
  it('with Kwajalein full at stop 3, its two rows land at the nearest held harbour with room by the sea\'s own distance, and neither is struck', () => {
    const g = startOpen(createGame(3, null));
    expect(KWAJ.map((i) => [ROWS[i][0], ROWS[i][5]])).toEqual([[27, 'Kwajalein'], [27, 'Kwajalein']]);
    const k = g.byName['Kwajalein'];
    expect(k.nb).toEqual([]);
    addCorps(g, 'JAP', 'A full division (a test row)', 'inf', 1, k.id, {});
    months(g, 36);
    expect(KWAJ.map((i) => g.flags[`sent:${i}`])).toEqual([1, 1]);
    expect(KWAJ.map((i) => where(g, i))).toEqual([['Eastern Marshalls'], ['Eastern Marshalls']]);
    // The nearest by chartDist (the measure nearHarbour uses) of the harbours Japan holds with room for them.
    const g2 = startOpen(createGame(3, null));
    const k2 = g2.byName['Kwajalein'];
    addCorps(g2, 'JAP', 'A full division (a test row)', 'inf', 1, k2.id, {});
    const one = { nat: 'JAP', kind: 'inf', divs: 1, str: 0.1 } as unknown as Corps;
    const near = g2.regions.filter((r) => r.port && r !== k2 && r.ctrl === 'JAP' && fits(g2, r, 'JAP', [one], { rows: r.units.filter((u) => u.kind !== 'eng').length }))
      .sort((a, b) => chartDist(g2, a, k2) - chartDist(g2, b, k2))[0];
    expect(near.name).toBe('Eastern Marshalls');
  });

  it('where no held ground anywhere has room, the rows wait past their half year unstruck, and land when room comes', () => {
    const g = startOpen(createGame(3, null));
    const k = g.byName['Kwajalein'];
    const fill = addCorps(g, 'JAP', 'A full division (a test row)', 'inf', 1, k.id, {});
    // Every other harbour Japan holds filled to its room for a corps of the rows' size.
    const one = { nat: 'JAP', kind: 'inf', divs: 1, str: 0.07 } as unknown as Corps;
    const filled = (r: Region): boolean => !fits(g, r, 'JAP', [one], { rows: r.units.filter((u) => u.kind !== 'eng').length });
    for (const r of g.regions) if (r.port && r !== k && r.ctrl === 'JAP') for (let n = 0; n < 8 && !filled(r); n++) addCorps(g, 'JAP', `Filler ${r.id}.${n} (a test row)`, 'inf', 1, r.id, {});
    const seen: string[] = [];
    months(g, 40, (t) => {
      seen.push(`T${t} ${KWAJ.map((i) => g.flags[`sent:${i}`] ?? '-').join('/')}`);
      if (t === 37) removeCorps(g, fill);
    });
    // Due T27, half a year out at T34: unstruck through T37, the months waited for room counted apart.
    expect(seen.slice(3, 14)).toEqual(['T27 -/-', 'T28 -/-', 'T29 -/-', 'T30 -/-', 'T31 -/-', 'T32 -/-', 'T33 -/-', 'T34 -/-', 'T35 -/-', 'T36 -/-', 'T37 -/-']);
    // Room comes at Kwajalein at T38: the Maizuru landing force lands there. (The 61st Guard Unit finds the room
    // taken by another row that waited, and waits on, still unstruck.)
    expect(where(g, 171)).toEqual(['Kwajalein']);
    expect(g.flags['sent:171']).toBe(1);
    expect(g.flags['sent:263']).toBeUndefined();
    expect(g.flags['roomwait:263']).toBeGreaterThan(6);
  });
});

/** Ground emptied of every corps, field and town. */
const clear = (g: Game, r: Region): void => { for (const u of [...r.units, ...(r.garr ?? [])]) removeCorps(g, u); };

describe('a beaten engineer corps falls back onto an island at room plus two (the design director, 00:20Z, 27 November 2026, D2)', () => {
  it('a British engineer corps beaten in Johore falls back onto Singapore, which holds its two engineers and a corps', () => {
    const g = startOpen(createGame(3, null));
    const sg = g.byName['Singapore'], jo = g.byName['Johore'];
    for (const r of [sg, jo]) clear(g, r);
    addCorps(g, 'UK', '1st Engineers (a test row)', 'eng', 1, sg.id, {});
    addCorps(g, 'UK', '2nd Engineers (a test row)', 'eng', 1, sg.id, {});
    addCorps(g, 'UK', 'A corps (a test row)', 'inf', 1, sg.id, {});
    const beaten = addCorps(g, 'UK', '3rd Engineers (a test row)', 'eng', 1, jo.id, {});
    const banned = new Set(jo.nb.filter((j) => j !== sg.id));
    // Ordered there, it does not fit: the island takes two engineer corps.
    expect(fits(g, sg, 'UK', [beaten])).toBe(false);
    // Beaten, it has the retreat's two places more.
    expect(fits(g, sg, 'UK', [beaten], { slack: 2 })).toBe(true);
    expect(retreatTarget(g, beaten, jo, banned)?.name).toBe('Singapore');
  });
});
