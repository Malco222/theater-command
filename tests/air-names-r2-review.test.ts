// The second reader's checks of the air groups' earned names (release 2; the design director's rulings of 25 November
// 2026 03:18Z (d) and 03:31Z, design-director/air-names-1125.md): what the player reads in a group's diary.
import { describe, expect, it } from 'vitest';
import { AIRNICKS, airRaidFlown } from '../src/campaign/airnicks';
import { createGame } from '../src/campaign/game';
import { setNicknames } from '../src/campaign/nicknames';
import { startOpen } from '../src/campaign/staff';
import { diaryText } from '../src/campaign/stories';
import type { AirFleet, AirSide } from '../src/campaign/types';

const side = (f: AirFleet, ftr: number): AirSide => ({ nat: f.nat, fleet: f.name, id: f.id, base: f.base, leader: null, ftr, bmr: 0, lostFtr: 0, lostBmr: 0 });

describe('a group made Guards, in its diary', () => {
  it('says it was made a Guards unit, not that its crews called it by a name of its own', () => {
    const g = startOpen(createGame(1, null));
    setNicknames(g, true);
    const f = g.airFleets.find((x) => x.nat === 'SOV' && x.kind === 'ftr')!;
    const city = g.regions[g.nations.SOV.capital];
    f.done = { n: AIRNICKS.need - 1, at: 0, x: 0, h: 0 };
    airRaidFlown(g, 'superiority', { odds: 1, bombers: 0, bombed: 0, E: 80, D: 60, sky: false }, [{ f, side: side(f, 80) }], [], city.id);
    // (The Soviet arm gives the Guards title, not a name: the group is renamed and carries no nickname.)
    expect(f.guards).toBeDefined();
    expect(f.nick).toBeUndefined();
    const line = f.log!.find((l) => l[1] === 'airnamed')!;
    const text = diaryText(g, f, line);
    expect(text).toMatch(/^Made a Guards unit after \d+ missions/);
    expect(text).not.toContain('a name of its own');
  });

  it('a group given a name by its crews still reads so', () => {
    const g = startOpen(createGame(1, null));
    setNicknames(g, true);
    const f = g.airFleets.find((x) => x.nat === 'GER' && x.kind === 'ftr' && !x.nick)!;
    f.done = { n: AIRNICKS.need - 1, at: 0, x: 0, h: 0 };
    airRaidFlown(g, 'superiority', { odds: 1, bombers: 0, bombed: 0, E: 80, D: 60, sky: false }, [{ f, side: side(f, 80) }], [], g.nations.POL.capital);
    expect(f.nick).toBeDefined();
    const line = f.log!.find((l) => l[1] === 'airnamed')!;
    expect(diaryText(g, f, line)).toMatch(new RegExp(`^Called ${f.nick!.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')} by its crews after \\d+ missions`));
  });
});
