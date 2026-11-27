// Only the hits that land shake a unit (release 2; the design director, 26 November 2026 06:21Z, after the sniper
// teams' gate traced it; the UI director's row, 06:23Z): when several blows land on one block in a tick, each blow's
// nerve weight prices only its own harm. Off, the old way stands: the tick's weights summed and the sum laid on all its
// harm. These tests hold the rule on one tick's blows, that a blow doing no harm costs nothing with it on, that the
// bridge turns it on only when asked or with sniper teams, and that it changes a battle through nerve.
import { describe, expect, it } from 'vitest';
import { LAND, LandBattle, PRACTICE_GROUNDS, practiceTicket, withTeams } from '../src/battle/land';
import { LiveBattle } from '../src/battle/land/live';

type Tick = { dmg: number; nerve: number; own: number; n: number };
type Peek = { tickNerve(h: Tick): number };
const tick = (blows: [number, number][]): Tick =>
  blows.reduce((h, [dmg, nerve]) => ({ dmg: h.dmg + dmg, nerve: h.nerve + nerve, own: h.own + nerve * dmg, n: h.n + 1 }), { dmg: 0, nerve: 0, own: 0, n: 0 });
const ticket = (terrain = 'urban', seed = 3) => withTeams(practiceTicket(PRACTICE_GROUNDS.find((p) => p.terrain === terrain)!, { seed }));

describe('only the hits that land shake a unit', () => {
  it('off, lays the summed weights on all the harm, as before', () => {
    const b = new LandBattle(ticket()), p = b as unknown as Peek;
    expect(p.tickNerve(tick([[0.002, 2], [0.001, 1]]))).toBe(3);
    // the old sum: a blow that does no harm still multiplies the nerve of the others' harm (3 to 11)
    expect(p.tickNerve(tick([[0.002, 2], [0.001, 1], [0, 8]]))).toBe(11);
  });

  it('on, prices each weight on its own harm only, and a blow that does no harm costs nothing', () => {
    const b = new LandBattle(ticket()), p = b as unknown as Peek;
    b.hitsShake = true;
    const two = tick([[0.002, 2], [0.001, 1]]);
    expect(p.tickNerve(two) * two.dmg).toBeCloseTo(0.002 * 2 + 0.001 * 1, 12);
    const three = tick([[0.002, 2], [0.001, 1], [0, 8]]);
    expect(p.tickNerve(three) * three.dmg).toBeCloseTo(p.tickNerve(two) * two.dmg, 12);
    // one blow alone is priced as it always was
    expect(p.tickNerve(tick([[0.003, 2.5]]))).toBeCloseTo(2.5, 12);
    // no harm at all: the weights' mean (only what was held back for the air reads it)
    expect(p.tickNerve(tick([[0, 2], [0, 4]]))).toBe(3);
  });

  it('reads its refits in place of the values they name only while on (DD 26 Nov 13:36Z)', () => {
    const b = new LandBattle(ticket()), p = b as unknown as { takeOf(t: string): number };
    // off: the hold's own values, every ground (switch off = the whole save)
    for (const g of Object.keys(LAND.hold.take)) expect(p.takeOf(g)).toBe(LAND.hold.take[g]);
    b.hitsShake = true;
    for (const g of Object.keys(LAND.hold.take)) expect(p.takeOf(g)).toBe(LAND.shaken.take[g] ?? LAND.hold.take[g]);
    expect(p.takeOf('marsh')).toBe(1.23);
    expect(LAND.hold.take.marsh).toBe(1.32);
  });

  it('is turned on by the bridge only when asked, or with sniper teams', () => {
    expect(new LiveBattle(ticket(), null, {}).sim.hitsShake).toBe(false);
    expect(new LiveBattle(ticket(), null, { hitsShake: true }).sim.hitsShake).toBe(true);
    expect(new LiveBattle(ticket(), null, { snipers: true }).sim.hitsShake).toBe(true);
    expect(new LiveBattle(ticket(), null, { snipers: true, hide: false }).sim.hitsShake).toBe(false);
  });

  it('changes a battle where several blocks fire on one, and only through nerve', () => {
    const run = (own: boolean) => {
      const b = new LandBattle(ticket('plains', 5));
      b.ai.att = b.ai.def = true;
      b.hitsShake = own;
      for (let k = 0; k < 2400 && !b.over; k++) b.step();
      return b;
    };
    const off = run(false), on = run(true);
    // the same blows land at first; nerve parts the two battles once two blocks strike one in a tick
    expect(on.units.map((u) => u.mor)).not.toEqual(off.units.map((u) => u.mor));
  });
});
