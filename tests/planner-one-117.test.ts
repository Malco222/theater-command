// The design director's ruling 117, point 6: two rows that open one card are one thing to learn twice. The planner
// lists a bombing raid once; its small line says by day or by night, and the card asks which. The 3D build keeps
// both rows (raidMenu) until its card has the two columns. Display only.
import { describe, expect, it } from 'vitest';
import { BOMB_EITHER, raidKinds, raidKindsOne } from '../src/campaign/airwar';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { raidMenu, raidMenuOne } from '../src/campaign/turn';

describe('the planner lists a bombing raid once', () => {
  it('has no row for the night, and the one row says by day or by night', () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    const two = raidMenu(g), one = raidMenuOne(g);
    expect(two.some((k) => k.key === 'night')).toBe(true);
    expect(one.some((k) => k.key === 'night')).toBe(false);
    expect(one.map((k) => k.key)).toEqual(two.filter((k) => k.key !== 'night').map((k) => k.key));
    const bomb = one.find((k) => k.key === 'bomb')!;
    expect(bomb.blurb).toBe(BOMB_EITHER);
    expect(bomb.blurb).toMatch(/by day or by night/);
    // It is offered where either way has a target, and counts the larger of the two.
    const day = two.find((k) => k.key === 'bomb')!, night = two.find((k) => k.key === 'night')!;
    expect(bomb.targets).toBe(Math.max(day.targets, night.targets));
    expect(bomb.why === null).toBe(day.targets > 0 || night.targets > 0);
    // Every other row is as it was.
    for (const k of one) if (k.key !== 'bomb') expect(k).toEqual(two.find((x) => x.key === k.key));
  });

  it('is the same list for any country, with nobody at the table too', () => {
    const g = startOpen(createGame(7, null));
    expect(raidKindsOne(g, 'GER').map((k) => k.key)).toEqual(raidKinds(g, 'GER').filter((k) => k.key !== 'night').map((k) => k.key));
  });
});
