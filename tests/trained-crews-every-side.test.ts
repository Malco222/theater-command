// The trained crews edge, one rule for every side (a mend of a fault in the owner's game: the design director's ruling
// of 27 Nov 2026 05:13Z, item 6, "Trained crews edge, player only (bombers.ts:410): one rule for all sides is broken in
// his game today ... The mend reads every side's crews by the same facts ... Its own named commit and test").
//
// bombers.ts trainedEdge reads the hours a group's crews arrived with (the roster's fifth field) the same way whoever
// runs the group. Before the mend a group the player did not run read 0 whatever its crews had. The player's own
// figures do not move by a digit: the pin below is the figure trainedEdge gave on the Play tree (61c805c10) for the
// seated war of the real case (seed 17, Germany seated, month 13, 1. Fliegerdivision, 75 crews: 53 with no hours, 7
// with 3, 15 with 8), read by the game's own function (scratch out/real-17-GER-raid.json).
import { afterEach, describe, expect, it } from 'vitest';
import { assessAir } from '../src/campaign/air';
import { BOMBERS, muster, trainedEdge } from '../src/campaign/bombers';
import { createGame } from '../src/campaign/game';
import type { AirFleet, Game } from '../src/campaign/types';

const KEEP = JSON.parse(JSON.stringify(BOMBERS)) as typeof BOMBERS;
afterEach(() => { Object.assign(BOMBERS, JSON.parse(JSON.stringify(KEEP))); });

/** The real case's roster, as the seated war had it: 53 crews with no hours, 7 with 3, 15 with 8. */
const REAL = [...Array(53).fill(0), ...Array(7).fill(3), ...Array(15).fill(8)] as number[];
/** trainedEdge on the Play tree for that roster, the player's own group (out/real-17-GER-raid.json, case.player). */
const REAL_EDGE = 0.023500000000000004;

const bombers = (g: Game, nat: string): AirFleet => {
  const f = g.airFleets.find((x) => x.nat === nat && x.bmr > 0 && x.ship === undefined && x.ashore === undefined)!;
  muster(g, f);
  return f;
};
/** A group's crews given hours, row for row (the list taken round again where the roster is longer). */
const give = (f: AirFleet, hrs: number[]): void => { f.bombers!.on.forEach((b, i) => { b[4] = hrs[i % hrs.length]; }); };
/** The real case's roster on a group whose strength is the roster's, as muster leaves it. */
const realRoster = (f: AirFleet): void => {
  const R = f.bombers!;
  while (R.on.length > REAL.length) R.on.pop();
  while (R.on.length < REAL.length) R.on.push([0, 0, 0, 0, 0, 0]);
  f.bmr = REAL.length;
  give(f, REAL);
};

describe('the trained crews edge reads every side by the same facts', () => {
  it("the player's own group: the edge is the Play tree's to the digit", () => {
    const g = createGame(1, 'GER');
    const f = bombers(g, 'GER');
    realRoster(f);
    expect(trainedEdge(g, f)).toBe(REAL_EDGE);
    give(f, [BOMBERS.school.most]);
    expect(trainedEdge(g, f)).toBe(BOMBERS.school.edge);
    give(f, [0]);
    expect(trainedEdge(g, f)).toBe(0);
  });

  it('a computer side\'s group with the same crews\' hours carries the same edge', () => {
    const g = createGame(1, 'GER');
    const mine = bombers(g, 'GER'), theirs = bombers(g, 'UK');
    realRoster(mine); realRoster(theirs);
    expect(theirs.nat).not.toBe(g.human);
    expect(trainedEdge(g, theirs)).toBe(REAL_EDGE);
    expect(trainedEdge(g, theirs)).toBe(trainedEdge(g, mine));
    // Crews with no hours: none, on either side.
    give(theirs, [0]);
    expect(trainedEdge(g, theirs)).toBe(0);
  });

  it('the same group and the same raid: the same figures whoever runs it, or nobody', () => {
    const g = createGame(1, 'GER');
    const f = bombers(g, 'GER');
    const t = g.byName['Warsaw'].id;
    realRoster(f);
    const seated = assessAir(g, [f], 'bomb', t);
    expect(seated.why).toBeNull();
    expect(seated.drilled).toBe(REAL_EDGE);
    for (const who of [null, 'UK', 'USA']) {
      g.human = who;
      expect(trainedEdge(g, f)).toBe(REAL_EDGE);
      const a = assessAir(g, [f], 'bomb', t);
      expect(a.drilled).toBe(seated.drilled);
      expect(a.bomberLoss).toBe(seated.bomberLoss);
      expect(a.gunLoss).toBe(seated.gunLoss);
    }
  });

  it('the edge takes the fighters\' share only, and the guns\' share is as it was', () => {
    const g = createGame(1, null);
    const f = bombers(g, 'GER');
    const t = g.byName['Warsaw'].id;
    give(f, [0]);
    const plain = assessAir(g, [f], 'bomb', t);
    expect(plain.why).toBeNull();
    expect(plain.drilled).toBe(0);
    expect(plain.bomberLoss).toBeGreaterThan(plain.gunLoss);
    give(f, [BOMBERS.school.most]);
    const drilled = assessAir(g, [f], 'bomb', t);
    expect(drilled.drilled).toBe(BOMBERS.school.edge);
    expect(drilled.gunLoss).toBe(plain.gunLoss);
    expect(drilled.bomberLoss - drilled.gunLoss).toBeCloseTo((plain.bomberLoss - plain.gunLoss) * (1 - BOMBERS.school.edge), 9);
  });

  it('a raid of two groups weighs each group\'s edge by its bombers', () => {
    const g = createGame(1, null);
    const all = g.airFleets.filter((x) => x.nat === 'GER' && x.bmr > 0 && x.ship === undefined && x.ashore === undefined);
    for (const f of all) muster(g, f);
    // The first pair of groups and the first province the game lets both of them bomb together.
    let pick: { two: AirFleet[]; t: number } | null = null;
    for (let i = 0; i < all.length && !pick; i++) for (let j = i + 1; j < all.length && !pick; j++) {
      for (const r of g.regions) {
        const a = assessAir(g, [all[i], all[j]], 'bomb', r.id);
        if (!a.why && a.att.length === 2 && a.att[0].bmr !== a.att[1].bmr) { pick = { two: [all[i], all[j]], t: r.id }; break; }
      }
    }
    expect(pick).not.toBeNull();
    const { two, t } = pick!;
    give(two[0], [BOMBERS.school.most]);
    give(two[1], [BOMBERS.school.most / 2]);
    const a = assessAir(g, two, 'bomb', t);
    expect(a.att).toHaveLength(2);
    const want = a.att.reduce((s, x) => s + x.bmr * trainedEdge(g, x.f), 0) / a.B;
    expect(want).toBeLessThan(BOMBERS.school.edge);
    expect(want).toBeGreaterThan(BOMBERS.school.edge / 2);
    expect(a.drilled).toBeCloseTo(want, 12);
    // The same raid with the seat taken by the groups' own side: the same figure.
    g.human = 'GER';
    expect(assessAir(g, two, 'bomb', t).drilled).toBe(a.drilled);
  });

  it('a roster longer than its group\'s strength (bombers lost, not yet mustered off) is read whole, on every side', () => {
    // The group loses bombers in a fight (f.bmr falls) and its roster is made up again only at the next muster, so for
    // a while the roster holds more rows than the group has bombers. The edge reads every row on it. The figure is the
    // one trainedEdge gives on the untouched base 8410c3e2d for the player's own group with this roster and f.bmr 53,
    // 60 and 74 (scratch out/c7-base.txt): the same as at full strength. The schooled rows are at the tail, so a
    // reading of only the first f.bmr rows would give 0 at 53. The same rows the other way round (the schooled rows at
    // the head) read the same on the base too (out/c11-base.txt), so a reading of only the last f.bmr rows is caught.
    const g = createGame(1, 'GER');
    const mine = bombers(g, 'GER'), theirs = bombers(g, 'UK');
    for (const hrs of [REAL, [...REAL].reverse()]) {
      realRoster(mine); realRoster(theirs);
      give(mine, hrs); give(theirs, hrs);
      for (const bmr of [74, 60, 53]) {
        mine.bmr = bmr; theirs.bmr = bmr;
        expect(mine.bombers!.on).toHaveLength(REAL.length);
        expect(trainedEdge(g, mine)).toBe(REAL_EDGE);
        expect(trainedEdge(g, theirs)).toBe(REAL_EDGE);
      }
    }
  });

  it('no roster, or an empty one, carries no edge on any side', () => {
    const g = createGame(1, 'GER');
    const f = g.airFleets.find((x) => x.nat === 'UK' && x.bmr > 0 && x.ship === undefined && x.ashore === undefined)!;
    expect(f.bombers).toBeUndefined();
    expect(trainedEdge(g, f)).toBe(0);
    muster(g, f);
    f.bombers!.on = [];
    expect(trainedEdge(g, f)).toBe(0);
  });
});
