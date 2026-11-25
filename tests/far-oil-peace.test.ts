// Japan's navy on its peacetime allowance of oil (the historian's paper sub-patrols-japan-oil.md, section 6c; the
// design director's 13:43Z of 22 November 2026: "the historian's 6c is laid as found. A peace draw of 1.25 M bbl a
// month to turn 24, war need from 25").
//
// The fault it mends: on the far chart Japan is at war with China from the first day, so its books counted the
// fleet at a war's need from the China war's second month, the battle line at sea and burning in full. The record's
// fleet did not: from 1937 to 1941 the navy built its stocks up, on about a million and a quarter barrels a month,
// and the war rate (2,900 barrels an hour, about 2.1 million a month) shows only with the mobilisation of 1941.
//
// The rule: until its fleet is mobilised Japan's ships burn 1.25 in 2.1 of what they would burn all at sea, wherever
// they lie. The clock is the freeze's: through the month America's sales stop (by the book August 1941), and a war's
// need from the month after, or once Japan is at war with America, Britain or the Dutch. The game's oil has no
// barrel, so the row holds the record's two figures and the rules read the one against the other. The depots' first
// day and the tanks are counted as before. Each state is set by hand on the first day's chart; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { CLASSES, atSea, boatsOut } from '../src/campaign/navy';
import { fleetAtPeace } from '../src/campaign/pacific';
import { RES, navyAllowance, needOf, stores, storesOf } from '../src/campaign/resources';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game } from '../src/campaign/types';

const SIX = ['GER', 'UK', 'FRA', 'ITA', 'SOV', 'USA'];
const SHARE = 1.25 / 2.1;
const grand = (stop = 3): Game => { const g = startOpen(createGame(3, null)); if (stop !== 3) setStop(g, stop, true); return g; };
const hull = (f: Fleet): number => CLASSES.reduce((s, c) => s + f.ships[c] * RES.oil.need.ships[c], 0);
/** What a nation's ships would burn in a month all at sea. */
const allAtSea = (g: Game, nat = 'JAP'): number => g.fleets.reduce((s, f) => s + (f.nat === nat ? hull(f) : 0), 0);
/**
 * What a fleet burns in a month at sea when no allowance holds: every ship in full, and of its submarines in a far
 * sea the boats out on patrol alone (the far side's one measure, navy.ts `boatsOut`; the design director's 18:04Z of
 * 22 November 2026). The rest lie at the base and burn the harbour's quarter.
 */
const sailing = (g: Game, f: Fleet): number => hull(f) - (f.ships.ss - boatsOut(g, f)) * RES.oil.need.ships.ss * (1 - RES.oil.need.port);
/** What a nation's ships burn in a month of war with every fleet at sea: the base of the peacetime allowance too (the design director's 18:23Z). */
const warAtSea = (g: Game, nat = 'JAP'): number => g.fleets.reduce((s, f) => s + (f.nat === nat ? sailing(g, f) : 0), 0);
/** What they burn as they lie: at sea as above, a quarter in harbour. */
const asTheyLie = (g: Game, nat = 'JAP'): number => g.fleets.reduce((s, f) => s + (f.nat === nat ? (atSea(f) ? sailing(g, f) : hull(f) * RES.oil.need.port) : 0), 0);
/** The ships' two lines in the month's books: at sea and in harbour. */
const shipsBurn = (g: Game, nat = 'JAP'): number => stores(g, nat)!.oil.spend.reduce((s, l) => s + (l.key === 'sea' || l.key === 'port' ? l.amount : 0), 0);
const toSea = (g: Game, nat = 'JAP'): void => { for (const f of g.fleets) if (f.nat === nat) f.stance = 'patrol'; };
const freeze = (g: Game, turn: number): void => { g.flags['pac:freeze'] = turn; };

describe("Japan's navy on its peacetime allowance of oil (the historian's 6c)", () => {
  it('the row holds the record\'s two figures, and Japan\'s is the only row', () => {
    const g = grand();
    expect(RES.oil.need.navy).toEqual({ JAP: { draw: 1.25, war: 2.1 } });
    expect(fleetAtPeace(g, 'JAP')).toBe(true);
    expect(navyAllowance(g, 'JAP')).toBeCloseTo(SHARE, 12);
    for (const nat of SIX) { expect(fleetAtPeace(g, nat), nat).toBe(false); expect(navyAllowance(g, nat), nat).toBe(0); }
  });

  it('at war with China alone its ships burn the allowance, in harbour or at sea', () => {
    const g = grand();
    expect(atWar(g, 'JAP', 'CHN')).toBe(true);
    expect(allAtSea(g)).toBeGreaterThan(0);
    const inHarbour = shipsBurn(g);
    expect(inHarbour).toBeCloseTo(SHARE * warAtSea(g), 9);
    // The battle line off China's coast burns no more: this is the fault the allowance mends.
    toSea(g);
    expect(asTheyLie(g)).toBeCloseTo(warAtSea(g), 9);
    expect(shipsBurn(g)).toBeCloseTo(inHarbour, 9);
    // And the month's need is the army's and the aircraft's, as they were, and the allowance.
    const rest = stores(g, 'JAP')!.oil.spend.reduce((s, l) => s + (l.key === 'sea' || l.key === 'port' ? 0 : l.amount), 0);
    expect(needOf(g, 'JAP', 'oil')).toBeCloseTo(rest + SHARE * warAtSea(g), 9);
    expect(stores(g, 'JAP')!.oil.need).toBeCloseTo(needOf(g, 'JAP', 'oil'), 9);
  });

  it('the depots\' first day and the tanks are counted as they were: what a war would ask, every ship as it lies', () => {
    const g = grand();
    const rest = needOf(g, 'JAP', 'oil') - shipsBurn(g);
    expect(needOf(g, 'JAP', 'oil', true)).toBeCloseTo(rest + asTheyLie(g), 9);
    expect(storesOf(g, 'JAP')!.oil).toBeCloseTo(34 * (rest + asTheyLie(g)), 9);
    expect(stores(g, 'JAP')!.oil.cap).toBeCloseTo(34 * (rest + asTheyLie(g)), 9);
    toSea(g);
    expect(needOf(g, 'JAP', 'oil', true)).toBeCloseTo(rest + warAtSea(g), 9);
    // (Of Japan's submarines at sea one in three is out: the rest burn the harbour's quarter, so a war asks less than every hull at sea would.)
    expect(warAtSea(g)).toBeLessThan(allAtSea(g));
  });

  it('by the book: the allowance through August 1941, the month of the freeze, and a war\'s need from September', () => {
    const g = grand(1);
    toSea(g);
    g.turn = 23;
    expect(fleetAtPeace(g, 'JAP')).toBe(true);
    g.turn = 24; freeze(g, 24);
    expect(fleetAtPeace(g, 'JAP')).toBe(true);
    expect(shipsBurn(g)).toBeCloseTo(SHARE * warAtSea(g), 9);
    g.turn = 25;
    expect(fleetAtPeace(g, 'JAP')).toBe(false);
    expect(navyAllowance(g, 'JAP')).toBe(0);
    expect(shipsBurn(g)).toBeCloseTo(warAtSea(g), 9);
    // A fleet in harbour then burns its quarter, as any navy's does.
    for (const f of g.fleets) if (f.nat === 'JAP') f.stance = 'port';
    expect(shipsBurn(g)).toBeCloseTo(RES.oil.need.port * allAtSea(g), 9);
  });

  it('where no book dates the freeze the allowance lasts until the freeze\'s own cause brings it', () => {
    const g = grand(3);
    toSea(g);
    g.turn = 40;
    expect(fleetAtPeace(g, 'JAP')).toBe(true);
    expect(shipsBurn(g)).toBeCloseTo(SHARE * warAtSea(g), 9);
    freeze(g, 40);
    expect(fleetAtPeace(g, 'JAP')).toBe(true);
    g.turn = 41;
    expect(fleetAtPeace(g, 'JAP')).toBe(false);
    expect(shipsBurn(g)).toBeCloseTo(warAtSea(g), 9);
  });

  it('a war with America, Britain or the Dutch ends it the same day, whatever the freeze', () => {
    for (const foe of ['USA', 'UK', 'NED']) {
      const g = grand(3);
      toSea(g);
      expect(g.flags['pac:freeze'], foe).toBeUndefined();
      if (!atWar(g, 'JAP', foe)) declareWar(g, 'JAP', foe);
      expect(fleetAtPeace(g, 'JAP'), foe).toBe(false);
      expect(shipsBurn(g), foe).toBeCloseTo(warAtSea(g), 9);
    }
  });

  it('no other navy has an allowance: each burns as its ships lie', () => {
    const g = grand();
    for (const nat of SIX) {
      // (A nation at peace with everyone burns its peace share of that, as it always has.)
      const k = g.nations[nat].atWar.length ? 1 : RES.oil.need.peace;
      expect(shipsBurn(g, nat), nat).toBeCloseTo(k * asTheyLie(g, nat), 9);
      toSea(g, nat);
      expect(shipsBurn(g, nat), nat).toBeCloseTo(k * warAtSea(g, nat), 9);
    }
  });
});

// The design director's 14:05Z of 22 November 2026: "the oil book counts a corps by kind and divisions, not by what it
// holds, so a quarter-division tank regiment burns a tank division's fuel. A corps burns rate times divs times c.full,
// the establishment cadreTerms already reads. One map: the corps burns what it holds, as it fights with what it holds."
// Behind the far switch, every nation and every kind.
describe('a corps burns oil at its establishment (the far side)', () => {
  const line = (g: Game, key: string, nat = 'JAP'): { amount: number; count: number } => {
    const l = stores(g, nat)!.oil.spend.find((x) => x.key === key);
    return { amount: l?.amount ?? 0, count: l?.count ?? 0 };
  };

  it('a tank regiment of a quarter of a division burns a quarter of a tank division\'s oil', () => {
    const g = grand();
    const tanks = g.corps.filter((c) => c.nat === 'JAP' && !c.dead && c.kind === 'arm');
    // The first day's six tank regiments, each a row of one division kept at a quarter (far-world.json).
    expect(tanks.map((c) => c.name).sort()).toEqual(['12th Tank Regiment', '13th Tank Regiment', '3rd Tank Regiment', '4th Tank Regiment', '5th Tank Regiment', '8th Tank Regiment']);
    for (const c of tanks) { expect(c.divs, c.name).toBe(1); expect(c.full, c.name).toBe(0.25); }
    expect(line(g, 'fast').amount).toBeCloseTo(6 * 0.25 * RES.oil.need.corps.arm, 9);
    expect(line(g, 'fast').count).toBeCloseTo(1.5, 9);
    // Made a full division, one of them burns a division's fuel: four times what it did.
    const before = line(g, 'fast').amount;
    delete tanks[0].full;
    expect(line(g, 'fast').amount - before).toBeCloseTo(0.75 * RES.oil.need.corps.arm, 9);
  });

  it('and a brigade a brigade\'s: every kind, by what the row holds', () => {
    const g = grand();
    const foot = g.corps.filter((c) => c.nat === 'JAP' && !c.dead && c.kind !== 'arm' && c.kind !== 'mot');
    const small = foot.filter((c) => c.full !== undefined);
    expect(small.length).toBeGreaterThan(0);
    const sum = foot.reduce((s, c) => s + (RES.oil.need.corps[c.kind] ?? RES.oil.need.corps._default) * c.divs * (c.full ?? 1), 0);
    expect(line(g, 'foot').amount).toBeCloseTo(sum, 9);
    expect(line(g, 'foot').count).toBeCloseTo(foot.reduce((s, c) => s + c.divs * (c.full ?? 1), 0), 9);
    // A brigade of a third of a division, made up to a division, burns the other two thirds more.
    const c = small[0], rate = RES.oil.need.corps[c.kind] ?? RES.oil.need.corps._default, was = line(g, 'foot').amount, held = c.full!;
    delete c.full;
    expect(line(g, 'foot').amount - was).toBeCloseTo(rate * c.divs * (1 - held), 9);
  });

  it('the first day\'s depots and the tanks are counted by the same measure', () => {
    const g = grand();
    const army = g.corps.reduce((s, c) => s + (c.nat === 'JAP' && !c.dead ? (RES.oil.need.corps[c.kind] ?? RES.oil.need.corps._default) * c.divs * (c.full ?? 1) : 0), 0);
    const planes = g.airFleets.reduce((s, f) => s + (f.nat === 'JAP' ? (f.ftr + f.bmr) * RES.oil.need.air : 0), 0);
    expect(needOf(g, 'JAP', 'oil', true)).toBeCloseTo(army + planes + asTheyLie(g), 9);
    expect(storesOf(g, 'JAP')!.oil).toBeCloseTo(34 * (army + planes + asTheyLie(g)), 9);
  });
});
