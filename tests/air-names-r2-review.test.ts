// The second reader's checks of the air groups' earned names (release 2; the design director's rulings of 25 November
// 2026 03:18Z (d) and 03:31Z, design-director/air-names-1125.md): what the player reads in a group's diary.
import { describe, expect, it } from 'vitest';
import { AIRNICKS, airRaidFlown } from '../src/campaign/airnicks';
import { setAirNames } from '../src/campaign/airnames';
import { groupRow } from '../src/campaign/airwar';
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

describe('with the switches off, a group\'s row carries none of their words', () => {
  it('no role word beside a name given as ruling 220 gives it, and no called name', () => {
    const g = startOpen(createGame(1, null));
    // (A name of ruling 220's says the role itself in English and in German: a word beside it would say it twice,
    // and the switch is off, so nothing of the switch's words is written.)
    const jg = g.airFleets.find((f) => f.name === 'Jagdgeschwader 1')!;
    expect(groupRow(g, jg).role).toBeUndefined();
    const uk = g.airFleets.find((f) => f.name === 'No. 11 Group')!;
    expect(groupRow(g, uk).role).toBeUndefined();
    // An honour nickname the honours rule gives (it is not the earned-names rule) is not set in the air style while
    // the earned names are off.
    uk.nick = 'the Old Hands';
    expect(groupRow(g, uk).called).toBeUndefined();
    setNicknames(g, true);
    expect(groupRow(g, uk).called).toBe('No. 11 Group (the Old Hands)');
  });

  it('on, a group named under the switch still shows its role beside a name that does not say it', () => {
    const g = createGame(1, null);
    g.flags.airNames = 1;
    startOpen(g);
    // ("No. 1 Group" is named under the switch and its name does not say the role; "No. 11 Group" keeps the name its
    // command came with, so the word beside it is the switch's while the switch is on.)
    const own = g.airFleets.find((f) => f.name === 'No. 1 Group')!;
    const kept = g.airFleets.find((f) => f.name === 'No. 11 Group')!;
    expect(own.roleNamed).toBe(false);
    expect(groupRow(g, own).role).toBe('bombers');
    expect(groupRow(g, kept).role).toBe('fighters');
    // The switch turned off later does not take the word off a group whose own name was given under it.
    setAirNames(g, false);
    expect(groupRow(g, own).role).toBe('bombers');
    expect(groupRow(g, kept).role).toBeUndefined();
  });
});
