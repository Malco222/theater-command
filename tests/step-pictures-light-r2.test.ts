// (Speed, the far map, 25 November 2026.) A month's steps are only ever laid on the 3D table while the month is
// watched (ShowWar: the holders, the armies, the fleets, the agents and the table's marks); the panels, cards and town
// plates read the month's own picture. So a step's picture leaves out what only they read, and the month's own picture
// keeps all of it. These tests hold the two apart: if a step's picture is ever read for one of the panel fields, it has
// to be drawn whole again (campaign-bridge.ts, drawView's panel()). Drawing a picture must not change the war, so a
// panel field whose drawing writes to the game (the chart, which keeps the admiral's sightings) is still drawn in a
// step's picture and only its answer left out; the month hashes (production/map-speed-1122/far/hashset.sh, the steps'
// panel fields dropped) hold the wars the same.
import { describe, expect, it } from 'vitest';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type View = Record<string, unknown>;
type Month = { steps: { view?: View }[]; view: View };
const TC = (globalThis as unknown as { TC: { start: (o: object) => unknown; endMonth: () => Month } }).TC;

/** What only the panels, cards and plates read: left out of a step's picture. */
const PANEL = ['joins', 'folds', 'cityOrders', 'glance', 'command', 'treasury', 'declare', 'gates', 'round', 'alerts', 'todo', 'alarms', 'alone', 'arms', 'dial', 'lift', 'chart', 'stores', 'men', 'idle'];
/** Some of what the table reads while a step is shown: always there. */
const TABLE = ['turn', 'human', 'regions', 'corps', 'fleets', 'air', 'airfields', 'prizes', 'wars', 'fronts', 'cold', 'going', 'wrecked', 'worksLeft', 'unseen', 'glimpsed', 'lanes', 'pages', 'designs', 'league', 'siegePaint'];

describe("a month's step pictures", () => {
  it('leave out the panel fields, keep the table fields, and the month picture keeps both', () => {
    TC.start({ view: 'grand', human: 'GER', seed: 7 });
    let month: Month | null = null;
    for (let i = 0; i < 3 && !month?.steps.length; i++) month = TC.endMonth();
    expect(month!.steps.length).toBeGreaterThan(0);
    for (const step of month!.steps) {
      const v = step.view!;
      for (const k of PANEL) expect(v[k], `step picture has ${k}`).toBeUndefined();
      for (const k of TABLE) expect(k in v, `step picture lacks ${k}`).toBe(true);
    }
    for (const k of [...PANEL, ...TABLE]) expect(k in month!.view, `month picture lacks ${k}`).toBe(true);
    // (a step's page list holds the towns page alone, where it is open: all the table reads of it; the month's is whole)
    for (const step of month!.steps) for (const p of step.view!.pages as { id: string }[]) expect(p.id).toBe('towns');
    expect((month!.view.pages as { id: string }[]).some((p) => p.id !== 'towns')).toBe(true);
    // (and a step's picture, sent as the 3D game is sent it, has none of the panel fields at all)
    const sent = JSON.parse(JSON.stringify(month!.steps[0].view)) as View;
    for (const k of PANEL) expect(k in sent).toBe(false);
  });
});
