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
/** The Maginot Line's two provinces, and the German ground beside them. */
const line = (g: Game) => ['Lorraine', 'Alsace'].map((n) => g.byName[n]);
const doors = (g: Game): number[] => [...new Set(line(g).flatMap((r) => r.nb.filter((j) => g.regions[j].ctrl === 'GER')))];
/** The West Wall's garrisons are put under the field army's orders: field corps stand beside the Line. */
const free = (g: Game): void => { for (const c of g.corps) if (c.nat === 'GER' && !c.dead && c.kind !== 'fort' && doors(g).includes(c.reg)) c.hold = false; };

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
      free(g);
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

// The design director's ruling of 21 November 2026 on the trace of that ruling (the staff massed for months opposite
// the Maginot Line and never struck: ai-builder/sandbox-fixes/eff-read.txt), for every staff free of the book:
//   - a place is named on the corps that would go in, not on what the staff reckons it could mass (the Line was named
//     at 1.23 on thirteen divisions, five of them a province back, and fought at 0.95 on six);
//   - patience shortens no odds against strong works: there the staff asks its full odds.
describe('a main effort is named on the corps beside the place, and at full odds against strong works', () => {
  /** A staff whose patience has run right out: it asks 'full' odds of its own, and 'short' where patience may shorten them. */
  function worn<T>(who: Character, full: number, short: number, run: () => T): T {
    const C = RIVALS.characters[who], P = RIVALS.effort.patience!;
    const old = { ...C }, was = { ...P };
    Object.assign(C, { quiet: 0, strength: 0, local: full });
    Object.assign(P, { step: 100, strength: 0, odds: short });
    try { return run(); } finally { Object.assign(C, old); Object.assign(P, was); }
  }

  it('the Maginot Line is strong works along the whole of Germany\'s front on France', () => {
    const g = france(4);
    for (const r of line(g)) { expect(r.fort!.lv).toBeGreaterThanOrEqual(2); expect(r.fort!.vs).toContain('GER'); }
    expect(doors(g).length).toBeGreaterThan(0);
  });

  for (const stop of [3, 4, 5]) {
    it(`stop ${stop}: with only its held garrisons beside the Line the staff names no place there, whatever stands a province back`, () => {
      const g = france(stop);
      // Field corps a province behind the doors: before the ruling they were reckoned in, as far as there was room.
      expect(g.corps.some((c) => c.nat === 'GER' && !c.dead && !c.hold && c.kind !== 'fort' && doors(g).some((j) => g.regions[j].nb.includes(c.reg)))).toBe(true);
      expect(g.corps.some((c) => c.nat === 'GER' && !c.dead && !c.hold && c.kind !== 'fort' && doors(g).includes(c.reg))).toBe(false);
      month(g);
      expect(effortOf(g, 'GER')).toBeNull();
      // With field corps beside it the same staff names one.
      free(g);
      month(g);
      expect(effortOf(g, 'GER')).not.toBeNull();
    });

    it(`stop ${stop}: patience shortens no odds against strong works: the staff asks its own, and an effort named there strikes at its usual odds`, () => {
      const who = characterOf(france(stop), 'GER');
      const scene = (): Game => { const g = france(stop); free(g); g.turn = 8; return g; };
      // Patience would take any odds (0.01) and the staff's own are within reach (0.05): the Line is named, at the
      // staff's own odds, so nothing is marked as named on shortened odds.
      const g = scene();
      worn(who, 0.05, 0.01, () => decide(g, 'GER'));
      const e = effortOf(g, 'GER')!;
      expect(e).not.toBeNull();
      expect(line(g).map((r) => r.id)).toContain(e.target);
      expect(e.odds).toBeUndefined();
      // The same army, the same quiet front, the same patience (it would take 0.05, which that army reaches), but the
      // staff's own odds out of reach: against strong works it asks its own, and names nothing.
      const h = scene();
      worn(who, 50, 0.05, () => decide(h, 'GER'));
      expect(effortOf(h, 'GER')).toBeNull();
    });
  }
});
