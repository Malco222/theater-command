// Ground that is not fed has no line (rider A of 20:45Z, 20 November 2026, to ruling 234; src/campaign/staff.ts,
// edgeLine).
//
// Ruling 221 (S) has a staff reckon the long line on China's ground in its sums of strength: for each province on a
// front's edge, the shortest line among its own provinces beside it, averaged along the edge. The table of lines
// holds a nought for ground the supply table does not feed at all, and that nought was read as the shortest line
// there is. In one war (seed 20, February 1941) a single cut-off province beside four provinces of a five-province
// edge read them at full strength; the reading leapt from 1.02 to 1.70 and opened the offensive that ended China.
// Ground that is not fed is passed over, and a stretch of the edge with no fed ground beside it reads as cut off: the
// longest line the bands know. No number is new. The state is set by hand on the first day's chart; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { EASTERN, addCorps, computeSupply, createGame } from '../src/campaign/game';
import { edgeLine, fronts, startOpen, type Front } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

/** A war nobody plays on the first day, far side on. */
const war = (): Game => { const g = startOpen(createGame(3, null)); computeSupply(g); return g; };
/** Japan's main front against China. */
const main = (g: Game): Front => fronts(g, 'JAP').find((f) => f.depth[g.byName['Chungking'].id] >= 1)!;
/** A weak Japanese column alone in Enshih, in the mountains behind China's line: nothing feeds it. */
const column = (g: Game): void => {
  const e = g.byName['Enshih'];
  e.ctrl = 'JAP'; e.since = g.turn - 3;
  addCorps(g, 'JAP', 'Cut-off column', 'inf', 1, 'Enshih', {}).str = 0.05;
  computeSupply(g);
};
/** The edge provinces Enshih touches that Japan's fed ground touches too, and those only Enshih touches. */
const SHARED = ['Siangyang', 'Changteh', 'Shasi'], ALONE = ['Wanhsien', 'Pengshui', 'Ankang', 'Ichang'];

describe('ground that is not fed has no line (rider A to ruling 234)', () => {
  it('is on the far side alone: the reckoning comes with China on the chart', () => {
    const g = war();
    expect(EASTERN.lineReckon).toBe(true);
    expect(EASTERN.lineOwners).toContain('CHN');
    expect(EASTERN.deepOwners).not.toContain('CHN');
  });

  it('passes over a cut-off province beside the edge: the fed ground there still gives the line', () => {
    const g = war();
    const ours0 = new Set(main(g).ours);
    const was = SHARED.map((n) => edgeLine(g, 'JAP', ours0, g.byName[n].id));
    for (const len of was) expect(len).toBeGreaterThan(EASTERN.stepFresh);
    column(g);
    const e = g.byName['Enshih'];
    // Nothing feeds the column, and the table of lines holds a nought for its province.
    expect(g.supply.JAP[e.id]).toBeFalsy();
    expect(g.reach.JAP[e.id]).toBe(0);
    const ours = new Set(main(g).ours);
    expect(ours.has(e.id)).toBe(true);
    expect(SHARED.map((n) => edgeLine(g, 'JAP', ours, g.byName[n].id))).toEqual(was);
  });

  it('reads a stretch with no fed ground beside it as cut off', () => {
    const g = war();
    column(g);
    const F = main(g), ours = new Set(F.ours);
    for (const n of ALONE) {
      expect(F.edge).toContain(g.byName[n].id);
      expect(edgeLine(g, 'JAP', ours, g.byName[n].id)).toBeNull();
    }
  });

  it('so a cut-off column behind the line no longer lifts the front\'s reading', () => {
    const g = war();
    const before = main(g);
    column(g);
    const after = main(g);
    // The column's own strength is next to nothing; what moves the sum is the line. Read as the shortest line there
    // is, its province lifted seven provinces of the edge to full strength and the attack rose.
    expect(after.atk).toBeLessThan(before.atk);
    expect(after.ratio).toBeLessThan(before.ratio);
  });
});
