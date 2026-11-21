// The sandbox's lock sweep, "an unused great choice silences the staff's own initiative on that front" (after
// release 1), and the design director's sandbox ruling of 20 November 2026 on it: a great choice binds nothing until
// the staff takes it up. Germany has been at war with France since September 1939, and its attack in the west is a
// great choice against France. Before the ruling that choice, untaken, spoke for the French front all the same: the
// staff named no main effort there and its first impatience was set aside, so the Phoney War was replayed at every
// stop. Now the choice speaks for the front only once it is taken up: while the army is being brought up for it, or
// while the book has dated it (stops 1 and 2, where history's plan is the staff's from the first day).
import { describe, expect, it } from 'vitest';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { RIVALS, characterOf, decide, effortOf, ops, startOpen, takenUp, type Character } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const grand = (stop: number): Game => { const g = startOpen(createGame(3, null)); if (stop !== 3) setStop(g, stop, true); return g; };
const west = () => ops('GER').find((o) => o.id === 'west')!;
const up = (g: Game): void => { g.flags['st:op:GER'] = ops('GER').indexOf(west()) + 1; g.flags['st:opAt:GER'] = g.turn; };
/** Germany at war with France alone, so that the French front is the only one its staff has to think about. */
function france(stop: number): Game {
  const g = grand(stop);
  for (const e of [...g.nations.GER.atWar]) if (e !== 'FRA') {
    g.nations.GER.atWar = g.nations.GER.atWar.filter((x) => x !== e);
    g.nations[e].atWar = g.nations[e].atWar.filter((x) => x !== 'GER');
  }
  if (!atWar(g, 'GER', 'FRA')) declareWar(g, 'GER', 'FRA');
  return g;
}
/** Runs one thing with a staff's patience and standards set aside, so that the test does not wait months for a quiet front. */
function eager<T>(who: Character, run: () => T): T {
  const C = RIVALS.characters[who];
  const old = { ...C };
  Object.assign(C, { quiet: 0, strength: 0, local: 0.05 });
  try { return run(); } finally { Object.assign(C, old); }
}
const month = (g: Game): void => eager(characterOf(g, 'GER'), () => decide(g, 'GER'));

describe('a great choice binds nothing until the staff takes it up', () => {
  it('the attack in the west is the great choice against an enemy Germany is already at war with', () => {
    expect(west().if).toContain('war:GER,FRA');
    expect(west().side).toBeFalsy();
  });

  for (const stop of [3, 4, 5]) {
    it(`stop ${stop}: on the menu it is not taken up; it is while the army is brought up for it, and no other choice is`, () => {
      const g = grand(stop);
      expect(takenUp(g, 'GER', west())).toBe(false);
      up(g);
      expect(takenUp(g, 'GER', west())).toBe(true);
      for (const op of ops('GER')) if (op !== west()) expect(takenUp(g, 'GER', op)).toBe(false);
      delete g.flags['st:op:GER'];
      expect(takenUp(g, 'GER', west())).toBe(false);
    });

    it(`stop ${stop}: with the west untaken the staff may name a main effort against France`, () => {
      const g = france(stop);
      expect(effortOf(g, 'GER')).toBeNull();
      month(g);
      const e = effortOf(g, 'GER')!;
      expect(e).not.toBeNull();
      expect(atWar(g, 'GER', g.regions[e.target].ctrl)).toBe(true);
      expect(e.enemy).toBe('FRA');
    });

    it(`stop ${stop}: once the army is being brought up for the west no main effort is named beside it`, () => {
      const g = france(stop);
      up(g);
      month(g);
      expect(effortOf(g, 'GER')).toBeNull();
    });
  }

  for (const stop of [1, 2]) {
    it(`stop ${stop}: the book has dated the west, so it is the staff's plan from the first day and the French front waits for it`, () => {
      const g = grand(stop);
      expect(takenUp(g, 'GER', west())).toBe(true);
      const h = france(stop);
      month(h);
      expect(effortOf(h, 'GER')).toBeNull();
    });

    it(`stop ${stop}: where the book is closed in the west the choice is the staff's own again, and binds nothing until taken up`, () => {
      const g = grand(stop);
      g.flags['hist:off:west'] = g.turn;
      expect(takenUp(g, 'GER', west())).toBe(false);
      up(g);
      expect(takenUp(g, 'GER', west())).toBe(true);
    });
  }
});
