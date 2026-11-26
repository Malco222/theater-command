// After his start a player's navy reads one kind of the history dial's rows: a ship coming onto the chart from off it,
// once (the design director's note of 15:52Z on ruling 234 rider b639). A player cannot order a ship that is off the
// map, so she comes to him on her row's date at her row's fleet: the Eagle; the Enterprise and the Renown where his
// start formed Force H by its book (formed by its plain row, it has them already); the Glorious's return where his start
// sent her off the chart. A ship whose leaving falls after his start stays with him, so her return is not read for him
// and she is never counted twice. Moves, departures and rows standing in for a plain row he reads are never his. The
// computer's navies read the dial as before.
import { describe, expect, it } from 'vitest';
import { createGame, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { NAVY, navalUpkeep } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';

/** A grand campaign begun in month `from` as the new-game screen begins one, through its first month's opening. */
const later = (from: number, stop: number, human: string | null, seed = 5): Game => {
  const g = createGame(seed, null);
  while (g.turn < from) runTurnAI(g);
  startOpen(g);
  g.human = human;
  setStop(g, stop, true);
  startPhase(g, 'UK');
  return g;
};
const fleet = (g: Game, name: string): Fleet => g.fleets.find((f) => f.nat === 'UK' && f.name === name)!;
/** The navy's month alone, turn by turn up to `to`. */
const upTo = (g: Game, to: number): void => { while (g.turn < to) { g.turn++; navalUpkeep(g, 'UK'); } };
const MED = 'Mediterranean Fleet';
const FORCE_H = NAVY.fleets.find((f) => f.nat === 'UK' && f.name === 'Force H')!.ships;

describe("a player's navy after his start: ships coming onto the chart, once (the director's note of 15:52Z on b639)", () => {
  it("Britain from the first day at stop 1: the Eagle comes; Force H forms with its row's ships; no move, departure or stand-in is read", () => {
    const g = startOpen(createGame(5, 'UK'));
    setStop(g, 1, true);
    const med = fleet(g, MED).ships.cv, home = { ...fleet(g, 'Home Fleet').ships };
    // October 1939: the Glorious does not leave the player's Mediterranean Fleet, so her return of turn 5 is not his.
    upTo(g, 5);
    expect([g.flags['arr:2:UK:Mediterranean Fleet'], g.flags['arr:5:UK:Mediterranean Fleet']]).toEqual([undefined, undefined]);
    expect(fleet(g, MED).ships.cv).toBe(med);
    // The Glorious's move to the Home Fleet in turn 8 is his to make or not.
    upTo(g, 8);
    expect(g.flags['arr:8:UK:Home Fleet<Mediterranean Fleet']).toBeUndefined();
    // Turn 9, the far side off: the Eagle comes from off the chart to the Mediterranean.
    upTo(g, 9);
    expect(g.flags['arr:9:UK:Mediterranean Fleet']).toBe(9);
    expect(fleet(g, MED).ships.cv).toBe(med + 1);
    // Turn 10: the Hood and the Valiant are not moved, and the Ark Royal does not leave the Home Fleet (the Illustrious
    // joins it by her plain row, as for every navy).
    upTo(g, 10);
    expect([g.flags['arr:10:UK:Force H<Home Fleet'], g.flags['arr:10:UK:Home Fleet']]).toEqual([undefined, undefined]);
    expect(fleet(g, 'Home Fleet').ships).toMatchObject({ bb: home.bb, cv: home.cv + 1 });
    // Turns 11 and 12: Force H forms by its own row, with its row's ships, the Enterprise and the Renown among them; the
    // book's rows that bring them are not read for him, so neither is counted twice.
    upTo(g, 12);
    expect([g.flags['arr:11:UK:Force H'], g.flags['arr:12:UK:Force H'], g.flags['arr:12:UK:Home Fleet<Force H']]).toEqual([undefined, undefined, undefined]);
    expect(g.flags['bk:UK:Force H']).toBeUndefined();
    expect(fleet(g, 'Force H').ships).toMatchObject(FORCE_H);
    // Turn 19: the Formidable's dated row stands in for her plain row of turn 16, which the player read: never his,
    // even with a carrier gone from the fleet.
    upTo(g, 18);
    fleet(g, MED).lost.cv += 1;
    fleet(g, MED).ships.cv -= 1;
    const now = fleet(g, MED).ships.cv;
    upTo(g, 19);
    expect(g.flags['arr:19:UK:Mediterranean Fleet']).toBeUndefined();
    expect(fleet(g, MED).ships.cv).toBe(now);
    // Turns 20 and 30: the Eagle does not leave his Mediterranean Fleet, so her return to Force H is not his either.
    upTo(g, 30);
    expect([g.flags['arr:20:UK:Mediterranean Fleet'], g.flags['arr:30:UK:Force H']]).toEqual([undefined, undefined]);
  });

  it("begun in June 1940: Force H forms by its book at his start, so the Enterprise and the Renown come to him, each once", () => {
    const g = later(10, 1, 'UK');
    expect(g.flags['bk:UK:Force H']).toBe(10);
    // The start month: the book's Force H, with the Hood and the Valiant from the Home Fleet.
    const h = { ...fleet(g, 'Force H').ships };
    expect(h).toMatchObject({ bb: 3, cv: 1, ca: 1 });
    upTo(g, 12);
    expect([g.flags['arr:11:UK:Force H'], g.flags['arr:12:UK:Force H']]).toEqual([11, 12]);
    // The Hood and the Valiant are his to keep or send: the Renown comes beside them, no cap on her.
    expect([g.flags['arr:12:UK:Home Fleet<Force H'], g.flags['arr:13:UK:Mediterranean Fleet<Force H']]).toEqual([undefined, undefined]);
    expect(fleet(g, 'Force H').ships).toMatchObject({ bb: h.bb + 1, ca: h.ca + 1, dd: h.dd + 2 });
    upTo(g, 14);
    expect(fleet(g, 'Force H').ships).toMatchObject({ bb: h.bb + 1, ca: h.ca + 1, dd: h.dd + 2 });
  });

  it("begun in November 1939: the start sent the Glorious to the Indian Ocean, so she comes back to the player in turn 5", () => {
    const g = later(3, 1, 'UK');
    expect(g.flags['arr:2:UK:Mediterranean Fleet']).toBe(3);
    const cv = fleet(g, MED).ships.cv;
    upTo(g, 5);
    expect(g.flags['arr:5:UK:Mediterranean Fleet']).toBe(5);
    expect(fleet(g, MED).ships.cv).toBe(cv + 1);
    // Her move to the Home Fleet in turn 8 is his.
    upTo(g, 8);
    expect(g.flags['arr:8:UK:Home Fleet<Mediterranean Fleet']).toBeUndefined();
    expect(fleet(g, MED).ships.cv).toBe(cv + 1);
  });

  it("begun in July 1941: the start sent the Eagle to the South Atlantic, so she comes back to Force H in turn 30, gap or none", () => {
    const g = later(23, 1, 'UK');
    expect(g.flags['arr:20:UK:Mediterranean Fleet']).toBe(23);
    const cv = fleet(g, 'Force H').ships.cv;
    upTo(g, 29);
    expect(g.flags['arr:30:UK:Force H']).toBeUndefined();
    upTo(g, 30);
    expect(g.flags['arr:30:UK:Force H']).toBe(30);
    expect(fleet(g, 'Force H').ships.cv).toBe(cv + 1);
  });

  it("begun in January 1941: the Eagle's leaving falls after the start, so she stays with the player and her return is not his", () => {
    const g = later(17, 1, 'UK');
    const med = fleet(g, MED).ships.cv, h = fleet(g, 'Force H').ships.cv;
    upTo(g, 30);
    expect([g.flags['arr:20:UK:Mediterranean Fleet'], g.flags['arr:30:UK:Force H']]).toEqual([undefined, undefined]);
    expect([fleet(g, MED).ships.cv, fleet(g, 'Force H').ships.cv]).toEqual([med, h]);
  });

  it("the computer's navy reads the dial as before: the Glorious out and back, the Eagle's return waiting for a gap", () => {
    const g = startOpen(createGame(5, null));
    setStop(g, 1, true);
    upTo(g, 5);
    expect([g.flags['arr:2:UK:Mediterranean Fleet'], g.flags['arr:5:UK:Mediterranean Fleet']]).toEqual([2, 5]);
    upTo(g, 30);
    expect(g.flags['arr:20:UK:Mediterranean Fleet']).toBe(20);
    // No carrier of Force H's has gone in this quiet war, so the Eagle's return still waits.
    expect(g.flags['arr:30:UK:Force H']).toBeUndefined();
  });
});
