// Ruling 133(c)(4), the design director's b491: the raid card's chance stays within three points of what the raid
// does. By night against guns the card read the luck of the day's raid, wider than the night's (32% on the card, 7%
// flown, over the Ruhr in August 1940 behind level 3 guns). With the night guns' switch (107(b)) the card weighs the
// night's luck as the flight throws it: 78% on the card, 78% flown. With the switch off nothing changes.
import { afterEach, describe, expect, it } from 'vitest';
import { assessAir, fly, forecastAir } from '../src/campaign/air';
import { AIRV, setAirPiece } from '../src/campaign/airversion';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { AirFleet, AirMission, Game } from '../src/campaign/types';

const keep = structuredClone(AIRV);
afterEach(() => { Object.assign(AIRV, structuredClone(keep)); });

/** The RAF's bombers over the Ruhr in August 1940, unescorted, the defending fighters as many as the bombers. */
function ruhr(guns: number): { g: Game; target: number; ids: number[] } {
  const g = startOpen(createGame(7, null));
  g.turn = 12;
  const target = g.byName.Ruhr.id;
  const ids = g.airFleets.filter((f) => f.nat === 'UK' && f.bmr > 0 && f.ship === undefined).map((f) => f.id);
  const fs = (): AirFleet[] => g.airFleets.filter((f) => ids.includes(f.id));
  let a = assessAir(g, fs(), 'bomb', target);
  for (let k = 0; k < 6 && a.D < a.B && a.def.length; k++) {
    const mul = Math.min(4, (a.B / Math.max(1, a.D)) * 1.02);
    for (const x of a.def) x.f.ftr = Math.round(x.f.ftr * mul);
    a = assessAir(g, fs(), 'bomb', target);
  }
  if (guns) g.flags[`wk:aa:${target}`] = guns;
  return { g, target, ids };
}
const card = (g: Game, m: AirMission, target: number, ids: number[]): number => {
  const a = assessAir(g, g.airFleets.filter((f) => ids.includes(f.id)), m, target);
  return forecastAir(g, a, m, target).success;
};

describe('the night raid card against guns (ruling 133(c)(4), b491)', () => {
  it('with the switch off, the card reads as it always has; by day the switch changes nothing', () => {
    const off = ruhr(3), on = ruhr(3);
    setAirPiece(on.g, 'night', true);
    expect(card(on.g, 'bomb', on.target, on.ids)).toBe(card(off.g, 'bomb', off.target, off.ids));
    // Off: the day's spread, which over-promises by night (the old reading, about a third).
    expect(card(off.g, 'night', off.target, off.ids)).toBeGreaterThan(0.25);
    expect(card(off.g, 'night', off.target, off.ids)).toBeLessThan(0.4);
  });

  it('with the switch on, the card says what the raid does, flown 300 times', () => {
    const { g, target, ids } = ruhr(3);
    setAirPiece(g, 'night', true);
    const said = card(g, 'night', target, ids);
    let won = 0;
    const N = 300;
    for (let k = 0; k < N; k++) {
      const c = structuredClone(g);
      c.seed = 5000 + k;
      if (fly(c, 'UK', 'night', target, c.airFleets.filter((f) => ids.includes(f.id)), `:t133:${k}`)!.won) won++;
    }
    expect(Math.abs(said - won / N)).toBeLessThan(0.03);
  }, 120_000);

  it('more guns never make the card more hopeful', () => {
    let last = 2;
    for (const guns of [0, 1, 2, 3]) {
      const { g, target, ids } = ruhr(guns);
      setAirPiece(g, 'night', true);
      const p = card(g, 'night', target, ids);
      expect(p).toBeGreaterThanOrEqual(0);
      expect(p).toBeLessThanOrEqual(last + 1e-9);
      last = p;
    }
  });
});
