// The Allies' landing ships in the Pacific by the record, with the far side's switch on (rider b614 to ruling 234; the
// historian's count, historian/allied-lift-pacific-1941-43-b593.md). A second count beside a country's monthly sea
// lift, read for a landing from the sea on an enemy shore of the Pacific's ground and for nothing else. What is
// counted is a country's own troops put ashore on an enemy-held shore, in anyone's ships, since the game has no
// boarding. By half-year from December 1941, and for December 1943: America 0, 1.25, 1.5, 6, 1.3; Australia 0, 0.06,
// 0, 1.2, 0; New Zealand 0.5 and Canada 0.35 from June 1943; nobody else any. It binds player and computer alike, by
// the date, and Japan's side is not counted. The states are set by hand; no war is played here.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { hop } from '../src/campaign/ai';
import { addCorps, atWar, createGame, declareWar } from '../src/campaign/game';
import { liftDates } from '../src/campaign/lift';
import { landCorps, liftLeft, whyNotLand } from '../src/campaign/navy';
import { PACIFIC, hopAims, pacificGround, shoreLift } from '../src/campaign/pacific';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

/** A war nobody plays (or `human`'s), with Japan at war with America, Britain, Australia and New Zealand, in month `turn`. */
const war = (turn: number, human: string | null = null): Game => {
  const g = startOpen(createGame(17, human));
  for (const foe of ['USA', 'UK', 'AUS', 'NZL']) if (!atWar(g, 'JAP', foe)) declareWar(g, 'JAP', foe);
  g.turn = turn;
  // (The months jumped would have read the shipping the record dates: America's third sailing from January 1942, rider b630.)
  for (const nat of Object.keys(g.nations)) liftDates(g, nat);
  return g;
};
const fresh = (c: Corps, str = 1): Corps => { c.supplied = true; c.ap = c.apMax; c.fought = false; c.org = 1; c.str = str; return c; };
const corps = (g: Game, nat: string, name: string, at: string, str = 1, est?: number): Corps => fresh(addCorps(g, nat, name, 'inf', 1, at, est === undefined ? {} : { est }), str);
/** The count as read: what is left, the stretch's worth, its first month, the month after its last, and the month more comes. */
const count = (left: number, from: number, next: number, due = next, first = from === 0) => ({ left, of: left, from, next, due, first });

describe('the rows as laid', () => {
  it("are the record's own troops put ashore, by half-year, to December 1943", () => {
    expect(PACIFIC!.lift).toEqual({
      until: 52,
      by: {
        USA: [[34, 1.25], [40, 1.5], [46, 6], [52, 1.3]],
        AUS: [[34, 0.06], [40, 0], [46, 1.2], [52, 0]],
        NZL: [[46, 0.5], [52, 0]],
        CAN: [[46, 0.35], [52, 0]],
      },
      // (Japan's own sea lift is the record's count, rider b630 to ruling 234: four forces a month from the opening,
      // eleven from October 1941.)
      own: { JAP: [[1, 4], [26, 11]] },
    });
  });
});

describe("the second count: the landing ships in the Pacific for a country's troops", () => {
  it("reads America's stretch by the date, whenever the war began, and nothing after December 1943", () => {
    const at = (t: number) => { const g = war(t); return shoreLift(g, 'USA', g.byName['Marcus Island']); };
    expect(at(10)).toEqual(count(0, 0, 34));
    expect(at(28)).toEqual(count(0, 0, 34));
    expect(at(33)).toEqual(count(0, 0, 34));
    expect(at(34)).toEqual(count(1.25, 34, 40));
    expect(at(39)).toEqual(count(1.25, 34, 40));
    expect(at(40)).toEqual(count(1.5, 40, 46));
    expect(at(51)).toEqual(count(6, 46, 52));
    expect(at(52)).toEqual(count(1.3, 52, 53));
    expect(at(53)).toBeNull();
  });

  it("reads Australia's two stretches with the empty half-year between them, and says when more is due", () => {
    const at = (t: number) => { const g = war(t); return shoreLift(g, 'AUS', g.byName.Truk); };
    expect(at(33)).toEqual(count(0, 0, 34));
    // Goodenough Island and Betano; nothing in the half-year after, so more is due in June 1943.
    expect(at(34)).toEqual(count(0.06, 34, 40, 46));
    expect(at(40)).toEqual(count(0, 40, 46, 46, false));
    // Lae and Finschhafen; nothing in December 1943, and the count ends with it.
    expect(at(46)).toEqual(count(1.2, 46, 52, 53));
    expect(at(52)).toEqual(count(0, 52, 53, 53, false));
    expect(at(53)).toBeNull();
  });

  it('has half a division for New Zealand and a brigade for Canada from June 1943, and nothing for Britain, China or the Dutch', () => {
    const g = war(45);
    const truk = g.byName.Truk;
    for (const nat of ['CAN', 'CHN', 'NED']) if (!atWar(g, 'JAP', nat)) declareWar(g, 'JAP', nat);
    expect(shoreLift(g, 'NZL', truk)).toEqual(count(0, 0, 46));
    expect(shoreLift(g, 'CAN', truk)).toEqual(count(0, 0, 46));
    for (const nat of ['UK', 'CHN', 'NED']) expect(shoreLift(g, nat, truk), nat).toEqual(count(0, 0, 53));
    g.turn = 46;
    expect(shoreLift(g, 'NZL', truk)).toEqual(count(0.5, 46, 52, 53));
    expect(shoreLift(g, 'CAN', truk)).toEqual(count(0.35, 46, 52, 53));
    for (const nat of ['UK', 'CHN', 'NED']) expect(shoreLift(g, nat, truk), nat).toEqual(count(0, 0, 53));
  });

  it("is read for an enemy shore of the Pacific's ground and for nothing else, and never for Japan's side", () => {
    const g = war(30);
    // An enemy shore there: Japan's own islands, and the Bay of Bengal's coast once Japan holds it.
    expect(pacificGround(g, g.byName.Truk)).toBe(true);
    expect(shoreLift(g, 'USA', g.byName.Truk)).not.toBeNull();
    const rangoon = g.byName.Rangoon;
    expect(pacificGround(g, rangoon)).toBe(true);
    expect(shoreLift(g, 'UK', rangoon)).toBeNull();
    rangoon.ctrl = 'JAP';
    expect(shoreLift(g, 'UK', rangoon)).toEqual(count(0, 0, 53));
    // One's own side's ground: the ordinary sea lift alone.
    expect(shoreLift(g, 'USA', g.byName.Honolulu)).toBeNull();
    expect(shoreLift(g, 'USA', g.byName.Brisbane)).toBeNull();
    // Europe's second front is not the Pacific's ground.
    if (!atWar(g, 'USA', 'GER')) declareWar(g, 'GER', 'USA');
    const coast = g.regions.find((r) => r.ctrl === 'GER' && r.seas.length > 0 && !pacificGround(g, r))!;
    expect(pacificGround(g, g.byName.Berlin)).toBe(false);
    expect(shoreLift(g, 'USA', coast)).toBeNull();
    // Japan has no second count: its own sea lift is the record's (rider b630 to ruling 234), four forces a month as
    // the game opens and eleven from October 1941, read as its months come (pacific.ts liftMonth; none has run here).
    expect(shoreLift(g, 'JAP', g.byName.Manila)).toBeNull();
    expect(g.nations.JAP.sealift).toBe(4);
  });
});

describe('a landing must fit both counts', () => {
  it('refuses America an enemy island before June 1942 in plain words, player and computer alike', () => {
    for (const human of [null, 'USA']) {
      const g = war(30, human);
      const c = corps(g, 'USA', 'Test Division', 'Honolulu');
      expect(liftLeft(g, 'USA')).toBeGreaterThan(0);
      expect(whyNotLand(g, [c], g.byName['Marcus Island'])).toBe('There are no landing ships in these waters for American troops yet. The first are due in June 1942.');
      expect(landCorps(g, [c], g.byName['Marcus Island']).ok).toBe(false);
      expect(c.reg).toBe(g.byName.Honolulu.id);
    }
  });

  it("carries a division and a quarter's worth between June and November 1942, once, and says what is left", () => {
    const g = war(34);
    const marcus = g.byName['Marcus Island'], kwajalein = g.byName.Kwajalein;
    const a = corps(g, 'USA', 'Test Division A', 'Honolulu', 0.75), b = corps(g, 'USA', 'Test Division B', 'Honolulu', 0.9), c = corps(g, 'USA', 'Test Division C', 'Honolulu', 0.92);
    const small = corps(g, 'USA', 'Test Battalion', 'Honolulu', 0.05, 0.05);
    // Two divisions at once are more than the record put ashore.
    expect(whyNotLand(g, [b, c], marcus)).toBe('This force has 27,300 men, and the landing ships in these waters can carry only 18,800 more American troops before December 1942.');
    expect(whyNotLand(g, [a], marcus)).toBeNull();
    expect(landCorps(g, [a], marcus, undefined, true).ok).toBe(true);
    expect(shoreLift(g, 'USA', kwajalein)!.left).toBeCloseTo(0.5, 9);
    // What is left carries a battalion and not a second division, this month or any month to November.
    for (const t of [34, 37, 39]) {
      g.turn = t; fresh(b, 0.9); fresh(small, 0.05);
      expect(liftLeft(g, 'USA'), `turn ${t}`).toBeGreaterThan(0);
      expect(whyNotLand(g, [b], kwajalein), `turn ${t}`).toBe('This force has 13,500 men, and the landing ships in these waters can carry only 7,500 more American troops before December 1942.');
    }
    expect(whyNotLand(g, [small], kwajalein)).toBeNull();
    // December 1942 opens a new stretch: 1.5.
    g.turn = 40; fresh(b, 0.9);
    expect(shoreLift(g, 'USA', kwajalein)).toEqual(count(1.5, 40, 46));
    expect(whyNotLand(g, [b], kwajalein)).toBeNull();
  });

  it('says so when a stretch is spent, and counts a landing whether it holds or is thrown back', () => {
    const g = war(35);
    const a = corps(g, 'USA', 'Test Division A', 'Honolulu'), q = corps(g, 'USA', 'Test Regiment', 'Honolulu', 0.25, 0.25), b = corps(g, 'USA', 'Test Division B', 'Honolulu', 0.9);
    expect(landCorps(g, [a], g.byName['Marcus Island'], undefined, true).ok).toBe(true);
    expect(landCorps(g, [q], g.byName.Kwajalein, undefined, true).ok).toBe(true);
    expect(shoreLift(g, 'USA', g.byName.Eniwetok)!.left).toBeCloseTo(0, 9);
    expect(whyNotLand(g, [b], g.byName.Eniwetok)).toBe('The landing ships in these waters have carried all the American troops they can for now. More are due in December 1942.');
  });

  it('moves to ground of one\'s own side stay on the ordinary lift', () => {
    const g = war(30);
    const c = corps(g, 'USA', 'Test Division', 'Honolulu');
    const midway = g.byName.Midway;
    expect(midway.ctrl).toBe('USA');
    expect(shoreLift(g, 'USA', midway)).toBeNull();
    const why = whyNotLand(g, [c], midway);
    expect(why === null || !/landing ships/.test(why)).toBe(true);
  });

  it("tells Australia and New Zealand when the ships for their troops come, and Britain that none do before 1944", () => {
    const g = war(40);
    const truk = g.byName.Truk;
    const aus = corps(g, 'AUS', 'Test Brigade', 'Brisbane', 0.3, 0.3), nzl = corps(g, 'NZL', 'Test Brigade', 'Auckland', 0.3, 0.3), uk = corps(g, 'UK', 'Test Brigade', 'Brisbane', 0.3, 0.3);
    expect(whyNotLand(g, [aus], truk)).toBe('There are no landing ships in these waters for Australian troops now. More are due in June 1943.');
    expect(whyNotLand(g, [nzl], truk)).toBe('There are no landing ships in these waters for New Zealand troops yet. The first are due in June 1943.');
    expect(whyNotLand(g, [uk], truk)).toBe('There are no landing ships in these waters for British troops yet. The first are due in January 1944.');
    // June 1943: the 9th Division's worth and no more.
    g.turn = 46; fresh(aus, 0.3);
    expect(shoreLift(g, 'AUS', truk)).toEqual(count(1.2, 46, 52, 53));
    const why = whyNotLand(g, [aus], truk);
    expect(why === null || !/landing ships/.test(why)).toBe(true);
  });
});

describe("the computer's own landings (the hop staff, ruling 200)", () => {
  const usa = (g: Game): Corps[] => g.corps.filter((c) => c.nat === 'USA' && !c.dead);
  const landed = (g: Game): string[] => Object.keys(g.flags).filter((k) => k.startsWith('landedAt:') && k.endsWith(':USA'));

  it('names no shore, lands nobody and brings nobody up while there are no ships', () => {
    const g = war(30);
    for (const nm of ['A', 'B', 'C']) corps(g, 'USA', `Test Division ${nm}`, 'Honolulu', 0.9);
    for (const c of usa(g)) fresh(c, c.str);
    const before = new Map(usa(g).map((c) => [c.id, c.reg]));
    hop(g, 'USA', null);
    expect(hopAims(g, 'USA')).toEqual([]);
    expect(landed(g)).toEqual([]);
    for (const c of usa(g)) expect(c.reg, c.name).toBe(before.get(c.id));
    expect(usa(g).some((c) => g.flags[`st:hopUp:${c.id}`] !== undefined)).toBe(false);
  });

  it("puts ashore no more than the half-year's worth, month after month", () => {
    const g = war(34);
    for (const nm of ['A', 'B', 'C']) corps(g, 'USA', `Test Division ${nm}`, 'Honolulu', 0.9);
    for (let t = 34; t <= 39; t++) {
      g.turn = t;
      for (const c of usa(g)) fresh(c, c.str);
      hop(g, 'USA', null);
    }
    const used = (g.flags['shorelift:USA:34'] ?? 0) / 1000;
    expect(landed(g).length).toBeGreaterThan(0);
    expect(used).toBeGreaterThan(0);
    expect(used).toBeLessThanOrEqual(1.25 + 1e-9);
  });
});
