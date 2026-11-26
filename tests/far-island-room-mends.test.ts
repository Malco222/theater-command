// The mender's tests for the island's room in men (26 November 2026): each finding of the sceptics that needed no
// ruling, held by a test that fails without its mend.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { frontWatch, runAI } from '../src/campaign/ai';
import { addCorps, createGame, declareWar } from '../src/campaign/game';
import { followKey, frontsClear, sailedKey } from '../src/campaign/planfronts';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

const at = (g: Game, name: string): Region => g.byName[name];
type Told = { what: string; force: string[]; to: string; why?: string };
const told: Told[] = [];
const listen = (): void => { told.length = 0; frontWatch.tell = (e) => { told.push({ what: e.what, force: e.force.map((c) => c.name), to: e.to.name, why: e.why }); }; };
afterEach(() => { frontWatch.tell = null; frontsClear(); });

describe("a computer staff's shore that takes none of its force (the design director, 21:37Z, 26 November 2026, 2)", () => {
  it("is read as full, and the staff goes on to the plan's next shore instead of waiting on it every month", () => {
    const g = startOpen(createGame(3, null));
    g.turn = 29;
    g.flags['st:done:JAP:south'] = 28;
    for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
    const guam = at(g, 'Guam');
    guam.ctrl = 'JAP'; guam.units = guam.units.filter((u) => u.nat === 'JAP');
    const rabaul = at(g, 'Rabaul');
    rabaul.units = rabaul.units.filter((u) => u.nat === 'JAP'); g.flags[`guard:${rabaul.id}`] = 100;
    // (The South Seas Detachment made five divisions for the test: 75,000 men against Rabaul's room of four, 60,000.)
    const ss: Corps = addCorps(g, 'JAP', 'South Seas Detachment', 'inf', 5, guam.id, { xp: 0.3, hold: true });
    g.flags[sailedKey(ss)] = 28;
    listen();
    runAI(g, 'JAP');
    expect(told.filter((t) => t.force[0] === 'South Seas Detachment').map((t) => [t.what, t.to, t.why])).toEqual([['wait', 'Rabaul', 'no room on the shore: Rabaul is full']]);
    // The shore is ended, not made: the row is free of it, and no hop behind it reads Rabaul as taken.
    expect(g.flags[followKey('JAP', 5, 0)]).toBe(-29);
    expect([g.regions[ss.reg].name, rabaul.ctrl === 'JAP']).toEqual(['Guam', false]);
    // The next month it does not wait on Rabaul again.
    g.turn = 30;
    listen();
    runAI(g, 'JAP');
    expect(told.filter((t) => t.force[0] === 'South Seas Detachment' && t.to === 'Rabaul')).toEqual([]);
  }, 300000);
});
