// A raid on troops whose fighters have strafed is not booked as broken off when they are sent home (release 2; the
// seam review of 26 November 2026, its pre-existing item 1; the design director, 26 Nov 02:20Z: "A group that has made a
// strafing pass is exempt from the break-off booking exactly as a bomber that has dropped is: it delivered its blow
// and then went home, so it did not break off, and 'its bombs jettisoned' is false of it. One rule. It acts only with
// lane (a) on, so switch-off identity holds and 1.5.2 alone is untouched."). 1.5's break-off rule (markQuit, quitter;
// tests/air-breakoff.test.ts) is otherwise as it was: a fighter group sent home before its first pass and before it met
// an enemy still gives the battle up. France's fighters over the Saar in September 1939, as tests/air-troops-r2.test.ts.
import { afterEach, describe, expect, it } from 'vitest';
import { planRaid, raidTargets } from '../src/campaign/airwar';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { AirSim } from '../src/battle/air/sim';
import { airTicket, flyKinds } from '../src/battle/air/ticket';

afterEach(() => { flyKinds([]); });

/** France's fighters alone on the German army in the Saar, the k-th battle's own luck. */
function strafers(k: number): AirSim {
  const g = startOpen(createGame(7, 'FRA'));
  setWar(g, 'FRA', 'GER');
  g.seed = (7 + 7919 * k) >>> 0;
  g.flags.airBattles = k;
  const target = raidTargets(g, 'ground', 'FRA').find((x) => g.regions[x.id].name === 'Saar')!.id;
  const ftr = planRaid(g, 'ground', target, undefined, 'FRA').seats.filter((x) => x.kind === 'ftr' && x.ship < 0 && !x.why).map((x) => x.id);
  expect(ftr.length).toBeGreaterThan(0);
  flyKinds(['ground']);
  const { ticket, why } = airTicket(g, 'ground', target, ftr, 'FRA');
  if (!ticket) throw new Error(why ?? 'no ticket');
  const s = new AirSim(ticket);
  s.ai.att = true; s.ai.def = true;
  return s;
}
// eslint-disable-next-line @typescript-eslint/no-explicit-any
const inside = (s: AirSim): { strafing: Map<number, { done: number }>; met: Set<number> } => s as any;
const raid = (s: AirSim) => s.units.filter((u) => u.side === 'att');
const toEnd = (s: AirSim): void => { let n = 0; while (!s.over && n++ < 20_000) s.step(); expect(s.over).toBeTruthy(); };

describe('strafers sent home after their passes: not broken off (release 2, lane (a))', () => {
  it('every fighter group has made a pass and none met an enemy, then all sent home: no side gave the battle up', () => {
    // (Over the Saar the defence meets the strafers some 255 seconds in, before a first pass is done, in every seed of 0
    // to 7: so their passes are counted as made, as a nearer field would make them, ten seconds in.)
    const s = strafers(0), { strafing, met } = inside(s);
    expect(s.units.some((u) => u.side === 'def' && u.air === 'ftr')).toBe(true);
    for (let n = 0; n < 100; n++) s.step();
    for (const u of raid(s)) {
      expect(met.has(u.id)).toBe(false);
      const pass = strafing.get(u.id);
      expect(pass).toBeDefined();
      pass!.done = 1;
    }
    expect(s.command({ do: 'withdraw', units: raid(s).map((u) => u.id) }, 'att')).toBe(true);
    toEnd(s);
    expect(s.outcome().gaveUp).toBeUndefined();
  });

  it('a fighter group sent home before its first pass and before any contact still gives the battle up (1.5 as it was)', () => {
    const s = strafers(0);
    s.step();
    expect(raid(s).every((u) => (inside(s).strafing.get(u.id)?.done ?? 0) === 0 && !inside(s).met.has(u.id))).toBe(true);
    expect(s.command({ do: 'withdraw', units: raid(s).map((u) => u.id) }, 'att')).toBe(true);
    toEnd(s);
    expect(s.outcome().gaveUp).toBe('att');
  });
});
