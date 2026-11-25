// Folding and a corps' own establishment, with the far side's switch on (rider b594 to ruling 234). A row its data
// keeps small (`est`: a base force of a fifth of a division, a detachment of a quarter) is whole at that size: worn is
// read against it, by the computer's staff and by the player's order alike, a fold fills the kept corps no fuller than
// it, and a cadre comes back at it. No strength is made or lost by a fold. Rows without an establishment read as they
// always did (quick-picks.test.ts, rival-staffs.test.ts). The states are set by hand; no war is played here.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { foldWorn } from '../src/campaign/ai';
import { offers } from '../src/campaign/arsenal';
import { cadreTerms, deliver, orderCadre } from '../src/campaign/economy';
import { foldChoices, foldCorps, foldView, whyNotFold } from '../src/campaign/fold';
import { addCorps, createGame } from '../src/campaign/game';
import { MANPOWER, menLeft } from '../src/campaign/manpower';
import { RIVALS, startOpen } from '../src/campaign/staff';
import { MEN_PER_DIV, type Corps, type Game } from '../src/campaign/types';

const fresh = (c: Corps, str: number): Corps => { c.supplied = true; c.ap = c.apMax; c.fought = false; c.org = 1; c.str = str; return c; };
/** A row of one division that never fills past `est` of it, standing at `str`, on Japan's own ground. */
const small = (g: Game, name: string, est: number, str: number, at = 'Palau'): Corps => fresh(addCorps(g, 'JAP', name, 'inf', 1, at, { est }), str);
const whole = (g: Game, name: string, str: number, at = 'Palau'): Corps => fresh(addCorps(g, 'JAP', name, 'inf', 1, at, {}), str);
const cadres = (g: Game): string[] => (g.nations.JAP.cadres ?? []).map((c) => c.name).filter((x) => x.startsWith('Test'));
const war = (human: string | null = null): Game => startOpen(createGame(17, human));

describe("the computer's staff and a corps' own establishment", () => {
  it('leaves two small rows alone that stand at their own size (Palau: the base force and the detachment)', () => {
    const g = war();
    const base = small(g, 'Test Base Force', 0.2, 0.2), det = small(g, 'Test Detachment', 0.25, 0.25);
    // (Both are under the staff's mark for a division, and their men would fit in one row: the old reading folded them.)
    expect(base.str).toBeLessThan(RIVALS.fold.worn);
    expect(det.str + base.str).toBeLessThanOrEqual(1);
    foldWorn(g, 'JAP');
    expect([base.dead, det.dead]).toEqual([false, false]);
    expect([base.str, det.str]).toEqual([0.2, 0.25]);
    expect(cadres(g)).toEqual([]);
    // Nor when one of the two is a little under its size: it is not worn until it is under the mark of its own size.
    base.str = 0.15;
    foldWorn(g, 'JAP');
    expect([base.dead, det.dead, cadres(g).length]).toEqual([false, false, 0]);
  });

  it('folds two small rows that are worn against their own size, and fills the kept row no fuller than its size', () => {
    const g = war();
    const a = small(g, 'Test Detachment A', 0.25, 0.09), b = small(g, 'Test Detachment B', 0.25, 0.08);
    a.xp = 0.4; b.xp = 0.1;
    foldWorn(g, 'JAP');
    expect(a.dead).toBe(false);
    expect(a.str).toBeCloseTo(0.17, 6);
    expect(a.full).toBe(0.25);
    expect(cadres(g)).toEqual(['Test Detachment B']);
    // Where the men of both would not fit in the smaller row's size the bigger row is kept, whatever the smaller knows:
    // the staff folds nobody where men would be over, and no strength is made or lost.
    const c = small(g, 'Test Base Force C', 0.2, 0.079, 'Saipan'), d = small(g, 'Test Brigade D', 0.5, 0.19, 'Saipan');
    c.xp = 0.6; d.xp = 0.1;
    expect(foldView(g, c, d).back).toBeGreaterThan(0);
    expect(foldView(g, d, c).back).toBe(0);
    const before = menLeft(g, 'JAP');
    foldWorn(g, 'JAP');
    expect(d.dead).toBe(false);
    expect(d.str).toBeCloseTo(0.269, 6);
    expect(d.full).toBe(0.5);
    expect(cadres(g)).toEqual(['Test Detachment B', 'Test Base Force C']);
    expect(menLeft(g, 'JAP')).toBeCloseTo(before, 9);
  });

  it('reads a row with no establishment as it always did', () => {
    const g = war();
    const a = whole(g, 'Test Corps A', 0.3), b = whole(g, 'Test Corps B', 0.35);
    a.xp = 0.4; b.xp = 0.1;
    foldWorn(g, 'JAP');
    expect(a.str).toBeCloseTo(0.65, 6);
    expect(cadres(g)).toEqual(['Test Corps B']);
  });
});

describe("the player's fold order and a corps' own establishment", () => {
  it('is refused in plain words for a row at its own size, and reads worn against that size', () => {
    const g = war();
    const base = small(g, 'Test Base Force', 0.2, 0.2), det = small(g, 'Test Detachment', 0.25, 0.25);
    const why = whyNotFold(g, det, base)!;
    expect(why).toBe('Test Detachment is already at full strength for its size.');
    expect(whyNotFold(g, base, det)).toBe('Test Base Force is already at full strength for its size.');
    expect(why).not.toMatch(/\best\b|establishment/i);
    expect(foldChoices(g, base.reg, 'JAP').filter((v) => [base.id, det.id].includes(v.keep))).toEqual([]);
    expect(foldCorps(g, det, base)).toBe(false);
    // Under its size, but not under the mark of its size: the old words, read against its own size.
    const worn = MANPOWER.fold.worn;
    base.str = 0.2 * (worn + 0.05); det.str = 0.25 * (worn - 0.2);
    expect(whyNotFold(g, det, base)).toMatch(new RegExp(`under ${Math.round(worn * 100)}% of their strength`));
    // Both under the mark of their own size: the order is open, both ways round.
    base.str = 0.2 * (worn - 0.1);
    expect(whyNotFold(g, det, base)).toBeNull();
    expect(whyNotFold(g, base, det)).toBeNull();
  });

  it('fills the kept row no fuller than its size, and makes or loses no strength', () => {
    const g = war();
    const keep = small(g, 'Test Detachment', 0.25, 0.15), gone = small(g, 'Test Base Force', 0.2, 0.13);
    const v = foldView(g, keep, gone);
    expect(v.why).toBeNull();
    expect(v.str).toBeCloseTo(0.25, 6);
    expect(v.men).toBe(Math.round(0.25 * MEN_PER_DIV));
    expect(v.back).toBe(Math.round(0.03 * MEN_PER_DIV));
    // The cadre comes back at its own size, for that share of the price and of the men.
    const one = cadreTerms(g, 'JAP', whole(g, 'Test Corps', 1, 'Saipan'));
    expect(v.cadreMen).toBeCloseTo(0.2 * MEN_PER_DIV, 6);
    expect(Math.abs(v.cadreCost - one.cost * 0.2)).toBeLessThan(0.11);
    const before = menLeft(g, 'JAP');
    expect(foldCorps(g, keep, gone)).toBe(true);
    expect(keep.str).toBeCloseTo(0.25, 6);
    // (What did not fit went back to the reserve the cadre is filled from: 0.15 + 0.13 = 0.25 + 0.03.)
    expect(menLeft(g, 'JAP') - before).toBeCloseTo(0.03, 6);
    expect(cadres(g)).toEqual(['Test Base Force']);
  });

  it('raises a small cadre again at its own size, never at a whole division', () => {
    const g = war();
    const keep = small(g, 'Test Detachment', 0.25, 0.1), gone = small(g, 'Test Base Force', 0.2, 0.08);
    expect(foldCorps(g, keep, gone)).toBe(true);
    const n = g.nations.JAP;
    const offer = offers(g, 'JAP', n.capital).find((o) => o.key === `cadre:${gone.id}`)!;
    expect(offer.men).toBeCloseTo(0.2 * MEN_PER_DIV, 6);
    n.pp = 5000;
    const before = menLeft(g, 'JAP');
    const b = orderCadre(g, 'JAP', gone.id, n.capital)!;
    expect(b).not.toBeNull();
    expect(before - menLeft(g, 'JAP')).toBeCloseTo(0.2, 6);
    g.turn = b.eta;
    deliver(g, 'JAP');
    expect(gone.dead).toBe(false);
    expect(gone.str).toBe(0.2);
    expect(gone.full).toBe(0.2);
  });
});
