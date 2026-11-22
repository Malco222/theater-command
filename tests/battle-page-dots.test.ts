// The 2D field page's men as dots (the true-scale plan, GR5; the production lead's option A of 22 November 08:03Z:
// "cheap dots (men0/N per block, thinning with strength, no drill)"). One N for the page, both sides: every man both
// sides bring over LAND.scale.page, rounded up to a multiple of 5 (the Kursk salient 45). A block's dots are its men0
// over N, thinning with its strength; a fogged field gives away neither its men nor N.
import { describe, expect, it } from 'vitest';
import { LAND, LandBattle, sampleTicket, type LandUnit } from '../src/battle/land';
import { dotAt, dotsOf, pageScale } from '../src/battle/ui/dots';

const battle = (seed = 1) => new LandBattle(sampleTicket('plains', 1.3, { seed, nat: { att: 'GER', def: 'SOV' } }));
/** Sets every block's men so the battle brings `total` men in all, split evenly. */
const bring = (b: LandBattle, total: number) => { for (const u of b.units) (u as { men0: number }).men0 = total / b.units.length; };

describe('the page\'s one N (GR5)', () => {
  it('the Kursk salient (480,000 v 570,000 men) draws at 1 to 45, about 23,400 dots; a battle that fits whole, 1 to 1', () => {
    const b = battle();
    bring(b, 480_000 + 570_000);
    expect(pageScale(b, 'att').N).toBe(45);
    const n = b.units.reduce((s, u) => s + dotsOf(b, u, pageScale(b, 'att')), 0);
    expect(Math.abs(n - 1_050_000 / 45)).toBeLessThanOrEqual(b.units.length);
    bring(b, LAND.scale.page);
    expect(pageScale(b, 'att').N).toBe(1);
  });

  it('the same N both sides: twice the men, twice the dots', () => {
    const b = battle();
    const att = b.units.filter((u) => u.side === 'att'), def = b.units.filter((u) => u.side === 'def');
    for (const u of att) (u as { men0: number }).men0 = 300_000 / att.length;
    for (const u of def) (u as { men0: number }).men0 = 600_000 / def.length;
    const sc = pageScale(b, 'att');
    const side = (us: LandUnit[]) => us.reduce((s, u) => s + dotsOf(b, u, sc), 0);
    expect(side(def) / side(att)).toBeCloseTo(2, 1);
  });
});

describe('a block\'s dots', () => {
  it('thin with its strength, at least one while it stands', () => {
    const b = battle();
    bring(b, 900_000);
    const sc = pageScale(b, 'att'), u = b.units[0];
    const full = dotsOf(b, u, sc);
    expect(full).toBe(Math.round(u.men0 / sc.N));
    u.str = u.str0 / 2;
    expect(dotsOf(b, u, sc)).toBe(Math.round(full / 2));
    u.str = 0;
    expect(dotsOf(b, u, sc)).toBe(1);
  });

  it('on a fogged field: N counts every block as an average one of his own, and an enemy he has not met shows that plain count', () => {
    const b = battle();
    b.fog = true;
    b.ticket.att.human = true;
    const mine = b.units.filter((u) => u.side === 'att'), theirs = b.units.filter((u) => u.side === 'def');
    for (const u of mine) (u as { men0: number }).men0 = 15_000;
    for (const u of theirs) (u as { men0: number }).men0 = 45_000;
    const sc = pageScale(b, 'att');
    expect(sc.plain).toBe(15_000);
    expect(sc.N).toBe(b.scale(() => 15_000, LAND.scale.page));
    const foe = theirs[0];
    expect(b.known(foe)).toBe(false);
    foe.str = foe.str0 / 2;
    expect(dotsOf(b, foe, sc)).toBe(Math.max(1, Math.round(15_000 / sc.N)));
    (foe as { found?: boolean }).found = true;
    expect(dotsOf(b, foe, sc)).toBe(Math.round((45_000 / sc.N) / 2));
  });

  it('lie in the box, evenly: any first n of them spread over all four quarters', () => {
    for (const id of [0, 3, 17]) {
      const q = [0, 0, 0, 0];
      for (let i = 0; i < 40; i++) {
        const [x, y] = dotAt(id, i);
        expect(x).toBeGreaterThanOrEqual(0); expect(x).toBeLessThan(1);
        expect(y).toBeGreaterThanOrEqual(0); expect(y).toBeLessThan(1);
        q[(x < 0.5 ? 0 : 1) + (y < 0.5 ? 0 : 2)]++;
      }
      for (const k of q) expect(k).toBeGreaterThanOrEqual(6);
    }
  });
});
