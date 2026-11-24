// The sea patrol in the Unreal bridge (release 2, Units decide; the design director's ruling of 24 November 2026 15:14Z
// on naval-battles/convoy-r2/sea-patrol-form.md). A group on patrol is aimed at a sea, as a strike on shipping is: its
// orders name the sea, never the province that happens to share the sea's number. The figures here are the tests' own,
// with the switch laid live (as shipped it is off: the design director's ruling of 24 November 2026 17:49Z, 5 (a)).
import { afterEach, describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { CAUSE, zoneOf } from '../src/campaign/navy';
import { setMission } from '../src/campaign/air';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const TC = (globalThis as Record<string, unknown>).TC as any;
const UNREAD = structuredClone(CAUSE.patrol);
afterEach(() => { Object.assign(CAUSE.patrol, structuredClone(UNREAD)); });

describe('a group on patrol in the bridge', () => {
  it('its orders name the sea it patrols: on its panel, and in the words when the order is given', () => {
    Object.assign(CAUSE.patrol, { live: true, reach: 'map', a0: 50, intensity: 0.5, attacks: { kit: 0.6, none: 0.3 }, kill: { kit: 0.1, none: 0.04 }, lost: null });
    const g = startOpen(createGame(1, 'UK'));
    g.flags.unitsDecide = 1;
    const wapp = zoneOf(g, 'wapp'), grp = g.airFleets.find((f) => f.name === 'No. 1 Group')!;
    // (A province shares the sea's number: the words must not be its name.)
    expect(g.regions[wapp]?.name).toBeTruthy();
    expect(TC.load(saveGame(g))).not.toBeNull();
    // (The words when the order is given: the bridge's airPlace.)
    const given = TC.act(`air:${grp.id}:seapatrol:${wapp}`);
    expect(given.ok).toBe(true);
    expect(given.note).toBe('No. 1 Group will fly against the Western Approaches when the month ends.');
    // (The group's panel, its orders now: the bridge's force panel.)
    expect(setMission(g, grp, 'seapatrol', wapp)).toBe(true);
    expect(TC.load(saveGame(g))).not.toBeNull();
    expect(TC.force('air', grp.id).lines).toContain('Patrol a sea against submarines: the Western Approaches.');
  });
});
