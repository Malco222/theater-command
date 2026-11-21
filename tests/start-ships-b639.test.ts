// A grand campaign begun after September 1939 opens with every nation's ships where the record had them in its first
// month, the player's included (rider b639 to ruling 234). The history dial's rows dated before the start, and in its
// first month, are the start position: facts of the day, not the dial moving a player's ships. From the first month on
// the player moves his own ships as before, and arrivals with no date of the dial come to him in their months.
import { describe, expect, it } from 'vitest';
import { createGame, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { navalUpkeep, startMonth } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Fleet, Game, Ships } from '../src/campaign/types';

/** A grand campaign begun in month `from` as the new-game screen begins one: the computer plays history to it, then hands over. */
const later = (from: number, stop: number, human: string | null, seed = 5): Game => {
  const g = createGame(seed, null);
  while (g.turn < from) runTurnAI(g);
  startOpen(g);
  g.human = human;
  setStop(g, stop, true);
  return g;
};
/** The same start twice, with the player at `nat`'s table and with the computer, each through its first month's opening. */
const both = (from: number, stop: number, nat: string): { me: Game; ai: Game; was: Record<string, Ships> } => {
  const me = later(from, stop, nat), ai = later(from, stop, null);
  const was = ships(me, nat);
  startPhase(me, nat);
  startPhase(ai, nat);
  return { me, ai, was };
};
const ships = (g: Game, nat: string): Record<string, Ships> => Object.fromEntries(g.fleets.filter((f) => f.nat === nat).map((f) => [f.name, { ...f.ships }]));
const where = (g: Game, nat: string): Record<string, string> => Object.fromEntries(g.fleets.filter((f) => f.nat === nat).map((f) => [f.name, `${g.regions[f.base]?.name} ${f.station}`]));
const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const MED = 'Mediterranean Fleet';

describe('a campaign begun later opens with the record\'s ships, the player\'s included (rider b639)', () => {
  it("Britain in January 1941: the player's navy opens as the computer's would, the Eagle in the Mediterranean", () => {
    for (const stop of [1, 3]) {
      const { me, ai, was } = both(17, stop, 'UK');
      expect(startMonth(me)).toBe(true);
      expect(ships(me, 'UK')).toEqual(ships(ai, 'UK'));
      expect(where(me, 'UK')).toEqual(where(ai, 'UK'));
      // The Eagle's row (turn 9, at every stop) and the Glorious's leaving (turn 2) are read for the player's navy.
      expect([me.flags['arr:9:UK:Mediterranean Fleet'], me.flags['arr:2:UK:Mediterranean Fleet']]).toEqual([17, 17]);
      if (stop === 1) {
        // At stop 1 the Glorious goes home, the Eagle and the Illustrious join: one carrier more than history's start had.
        expect(me.flags['arr:13:UK:Mediterranean Fleet<Home Fleet']).toBe(17);
        expect(fleet(me, 'UK', MED).ships.cv - was[MED].cv).toBe(1);
      }
    }
  });

  it('Force H in a campaign begun in June 1940 forms by its book in the start month, its carriers and battleships with it', () => {
    const { me, ai, was } = both(10, 1, 'UK');
    expect(was['Force H']).toBeUndefined();
    expect(me.regions[fleet(me, 'UK', 'Force H').base].name).toBe('Gibraltar');
    expect(ships(me, 'UK')).toEqual(ships(ai, 'UK'));
    // The Hood and the Valiant come from the Home Fleet, the Ark Royal leaves it: the record's Force H of 28 June.
    expect(me.flags['arr:10:UK:Force H<Home Fleet']).toBe(10);
    expect(fleet(me, 'UK', 'Force H').ships).toMatchObject({ bb: 3, cv: 1 });
  });

  it("Italy's boats in a campaign begun in June 1941 stand on the record's beats, and are the player's to move after", () => {
    const { me, ai } = both(22, 3, 'ITA');
    expect(ships(me, 'ITA')).toEqual(ships(ai, 'ITA'));
    expect(where(me, 'ITA')).toEqual(where(ai, 'ITA'));
    const atlantic = fleet(me, 'ITA', 'Atlantic Submarine Fleet'), central = fleet(me, 'ITA', 'Central Submarine Fleet');
    expect(atlantic.ships.ss).toBeGreaterThan(0);
    // The player brings the Atlantic's boats home to the central Mediterranean; next month no row sends them back.
    central.ships.ss += atlantic.ships.ss;
    atlantic.ships.ss = 0;
    const mine = ships(me, 'ITA');
    me.turn = 23;
    expect(startMonth(me)).toBe(false);
    navalUpkeep(me, 'ITA');
    expect(ships(me, 'ITA')).toEqual(mine);
    // The computer's boats are placed by the rows every month.
    const a = fleet(ai, 'ITA', 'Atlantic Submarine Fleet');
    fleet(ai, 'ITA', 'Central Submarine Fleet').ships.ss += a.ships.ss;
    a.ships.ss = 0;
    ai.turn = 23;
    navalUpkeep(ai, 'ITA');
    expect(fleet(ai, 'ITA', 'Atlantic Submarine Fleet').ships.ss).toBeGreaterThan(0);
  });

  it("after the start month the dial's rows leash the computer's navies only; an arrival with no date of the dial comes to the player in its month", () => {
    const { me, ai } = both(17, 1, 'UK');
    const med = fleet(me, 'UK', MED).ships.cv, home = { ...fleet(me, 'UK', 'Home Fleet').ships };
    // May 1941: the Eagle leaves the Mediterranean for the South Atlantic, in the computer's navy alone.
    for (const g of [me, ai]) { g.turn = 20; navalUpkeep(g, 'UK'); }
    expect([me.flags['arr:20:UK:Mediterranean Fleet'], ai.flags['arr:20:UK:Mediterranean Fleet']]).toEqual([undefined, 20]);
    expect(fleet(me, 'UK', MED).ships.cv).toBe(med);
    // June 1941: the Prince of Wales and the Victorious join the Home Fleet, the player's as the computer's.
    me.turn = 21;
    navalUpkeep(me, 'UK');
    expect([fleet(me, 'UK', 'Home Fleet').ships.bb - home.bb, fleet(me, 'UK', 'Home Fleet').ships.cv - home.cv]).toEqual([1, 1]);
  });

  it("a campaign begun on the first day has no start month: the dial's rows never move the player's ships", () => {
    const me = startOpen(createGame(5, 'UK')), ai = startOpen(createGame(5, null));
    for (const g of [me, ai]) setStop(g, 1, true);
    expect(me.flags.openFrom).toBeUndefined();
    const cv = fleet(me, 'UK', MED).ships.cv;
    for (const g of [me, ai]) { g.turn = 2; navalUpkeep(g, 'UK'); }
    expect(startMonth(me)).toBe(false);
    // October 1939: the Glorious leaves the Mediterranean for the Indian Ocean, in the computer's navy alone.
    expect([me.flags['arr:2:UK:Mediterranean Fleet'], ai.flags['arr:2:UK:Mediterranean Fleet']]).toEqual([undefined, 2]);
    expect(fleet(me, 'UK', MED).ships.cv).toBe(cv);
    expect(fleet(ai, 'UK', MED).ships.cv).toBe(cv - 1);
  });
});
