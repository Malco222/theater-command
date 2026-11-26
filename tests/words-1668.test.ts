// The UI director's word mends from build 1668 (ui-director/review-1441x878.md): the storm ticket names a capital as
// "Panama City, Panama's capital"; a garrison is told it does not take the train; the Arsenal says "Motorised"; and an
// airfield's own page says its crowding once, in the head, not on every group.
import { describe, expect, it } from 'vitest';
import { addCorps, assess, createGame, relocate, setWar, whyNoTrain } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { garrison } from '../src/campaign/siege';
import { groupLines } from '../src/campaign/airpages';
import { KIND } from '../src/campaign/types';
import production from '../data/campaign/production.json';
import type { GroupRow } from '../src/campaign/airwar';

describe('build 1668 words', () => {
  it("names a capital on the storm ticket as the place, then whose capital it is", () => {
    const g = startOpen(createGame(3, 'GER'));
    const warsaw = g.byName.Warsaw;
    const c = addCorps(g, 'GER', 'Ticket Corps', 'inf', 2, 'Masuria', {});
    const town = assess(g, [c], warsaw, { town: true }).lines.find((l) => l.label === 'The town')!;
    expect(town.value).toMatch(/^Warsaw, Poland's capital/);
    expect(town.value).not.toMatch(/Capital of/);
  });

  it('names any other town as the place, then what it is, and keeps its works (build 1678)', () => {
    const g = startOpen(createGame(3, 'GER'));
    const c = addCorps(g, 'GER', 'Ticket Corps', 'inf', 2, 'Masuria', {});
    const lodz = assess(g, [c], g.byName.Lodz, { town: true }).lines.find((l) => l.label === 'The town')!;
    expect(lodz.value).toMatch(/^Lodz, a (city|town)/);
    expect(lodz.value).not.toMatch(/(City|Town|Village) of/);
    const silesia = g.byName['Polish Silesia'];
    const s = assess(g, [c], silesia, { town: true }).lines.find((l) => l.label === 'The town')!;
    expect(s.value).toMatch(/^Katowice, a city, with fortress works of level \d/);
  });

  it('tells a garrison out of a siege that it does not take the train', () => {
    const g = startOpen(createGame(3, 'GER'));
    setWar(g, 'GER', 'POL');
    const lodz = g.byName.Lodz;
    for (const u of [...lodz.units]) relocate(g, u, g.byName.Warsaw);
    const c = addCorps(g, 'POL', 'Inside Corps', 'inf', 1, 'Lodz', {});
    expect(garrison(g, c)).toBe(true);
    expect(whyNoTrain(g, c)!.line).toBe('A garrison does not take the train. Out of the town, it can go by rail next month.'); // (the 1.5.3 review: a train takes the whole month, and coming out costs a move)
  });

  it('says Motorised, in one English', () => {
    expect(KIND.mot.label).toBe('Motorised');
    expect(JSON.stringify(production)).not.toMatch(/Motorized/);
  });

  it("leaves the crowding off a group's row on its airfield's page, and keeps it on the group's own card", () => {
    const x = { ftr: 40, bmr: 0, range: 600, ready: 0.9, doing: 'On patrol', crowd: 0.6 } as unknown as GroupRow;
    expect(groupLines(x, false).join(' ')).not.toMatch(/crowded/);
    expect(groupLines(x).join(' ')).toContain('The airfield is crowded: only 60% of it can fly.');
  });
});
