// With the far side off the lanes are the old seven, word for word (the design director's rulings of 04:33Z and
// 12:39Z, 25 November 2026: "the rows ride the far switch"; the Pacific planner's brief batch3-lanes-brief.md, part 5,
// test 8).
//
// The Pacific sea lanes (America to Hawaii, America to Australia, Britain round the Cape to India) are rows of
// data/campaign/far-books.json, laid over the lanes list only while the whole world is on the chart
// (src/campaign/far-rows.ts). This file plays with the switch as the game ships it, off, and pins that nothing of
// them is read there: the lanes list is the one data/campaign/resources.json carries on disk, the convoy routes are
// navy.json's, and no route of the three ids can be escorted or raided. The two figures are the sha256 of each list
// as JSON.stringify writes it (counted on 93cdfb78 and again on 35631517, the same), so that a change to either list
// on the old chart is seen here by whoever makes it.
//
// The words are pinned too. The batch gives the stores page a line for a lane that carries nothing ("Not counted")
// and a lane cut at the harbours its ships land at its own sentence; neither may reach a lane of the old chart. The
// third figure is the sha256 of every sentence the old chart's lanes print (oldLaneLines below: the stores page's
// rows and their book lines on the first day, and each row's harbours, narrows and fields in a neutral's hands and
// then an enemy's). The same body, run as a script on 35631517, the tree before the words, gave the same figure.
//
// The far side's own tests of the rows: tests/far-lanes-laid.test.ts.
import { describe, expect, it } from 'vitest';
// (The files as they lie on disk, read as text.)
import navyDisk from '../data/campaign/navy.json?raw';
import resourcesDisk from '../data/campaign/resources.json?raw';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { NAVY, seaRoutes } from '../src/campaign/navy';
import { RES, laneKey, laneNotCounted, laneStopped, stores } from '../src/campaign/resources';
import { FAR_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { farOn } from '../src/campaign/world';

const NEW = ['america-hawaii', 'america-australia', 'britain-india'];
const sha256 = async (text: string): Promise<string> => [...new Uint8Array(await crypto.subtle.digest('SHA-256', new TextEncoder().encode(text)))].map((b) => b.toString(16).padStart(2, '0')).join('');

/** Every sentence the old chart's lanes print, one line each. */
function oldLaneLines(): string[] {
  const out: string[] = [];
  const say = (tag: string, v: unknown): void => { out.push(`${tag}: ${JSON.stringify(v)}`); };
  const book = (g: any, tag: string): void => {
    for (const nat of RES.nations) {
      const v = stores(g, nat);
      if (!v) continue;
      for (const l of v.lanes) say(`${tag} ${nat} ${l.id}`, [l.name, l.from, l.text, l.state, l.why, laneKey(g, l.id, null), laneKey(g, l.id, nat), laneNotCounted(g, l)]);
      for (const b of [v.oil, v.sup]) for (const i of b.income) if (i.lane !== undefined) say(`${tag} ${nat} ${b.good} ${i.key}`, [i.label, i.note ?? null]);
    }
  };
  book(startOpen(createGame(3, null)), 'day one');
  for (const row of RES.lanes) {
    const places = [...(row.held ?? []), ...(row.ports ?? []), ...(row.past ?? []), ...(row.field ? [row.field] : [])];
    const foe = row.to === 'GER' ? 'UK' : 'GER';
    for (const pick of [...places.map((p) => [p]), places]) {
      if (!pick.length) continue;
      const g: any = startOpen(createGame(3, null));
      // In a neutral's hands, and then in an enemy's.
      for (const p of pick) g.byName[p].ctrl = 'SWE';
      say(`${row.id} ${pick.join('+')} neutral`, laneStopped(g, row.id));
      if (!atWar(g, foe, row.to)) declareWar(g, foe, row.to);
      for (const p of pick) g.byName[p].ctrl = foe;
      say(`${row.id} ${pick.join('+')} enemy`, [laneStopped(g, row.id), laneKey(g, row.id, null), laneKey(g, row.id, row.to)]);
      book(g, `${row.id} ${pick.join('+')}`);
    }
  }
  return out;
}

describe('the old chart keeps its own lanes (the far switch off)', () => {
  it('with the far side off the lanes are the old seven, word for word', async () => {
    // (Run with the switch as shipped: a run given TC_FAR=1 in its environment is not this test's.)
    expect(FAR_STAGE).toBe(false);
    const lanes = JSON.parse(resourcesDisk).lanes, convoys = JSON.parse(navyDisk).convoys;
    expect(RES.lanes.map((l) => l.id)).toEqual(['atlantic', 'gulf', 'french', 'italian', 'german', 'arctic', 'persia']);
    expect(RES.lanes).toEqual(lanes);
    expect(NAVY.convoys).toEqual(convoys);
    expect(await sha256(JSON.stringify(lanes))).toBe('31da7245591c4a3777e34760c2ca207d0b8ecb5ff34c113161767ef3e06fa41f');
    expect(await sha256(JSON.stringify(convoys))).toBe('f2fb3e7a09db4a06c9fbd92223e447dd659d16c7875f1eae08b2a1f2568e4ba0');
    // And as the rules read them: the same two lists, to the letter.
    expect(await sha256(JSON.stringify(RES.lanes))).toBe('31da7245591c4a3777e34760c2ca207d0b8ecb5ff34c113161767ef3e06fa41f');
    expect(await sha256(JSON.stringify(NAVY.convoys))).toBe('f2fb3e7a09db4a06c9fbd92223e447dd659d16c7875f1eae08b2a1f2568e4ba0');
    // No route of the three can be escorted or raided on the old chart.
    const g = startOpen(createGame(3, null));
    expect(farOn(g)).toBe(false);
    const routes = seaRoutes(g).map((r) => r.id);
    for (const id of NEW) expect(routes, id).not.toContain(id);
    expect(routes).toEqual(['atlantic', 'french', 'italian', 'german', 'arctic', 'libya']);
  });

  it("no sentence an old chart's lane prints has changed", async () => {
    expect(FAR_STAGE).toBe(false);
    const lines = oldLaneLines();
    expect(lines).toHaveLength(246);
    // Nothing of the old chart is "not counted", and none is cut at a harbour its ships land at.
    expect(lines.filter((l) => /Not counted|no port left to land at|lands at/.test(l))).toEqual([]);
    expect(await sha256(lines.join('\n'))).toBe('4732c0887baccf470c351d6082d2b35797696fbb8e037876abe05d232e830ab1');
  });
});
