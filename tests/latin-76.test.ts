// Version 76, the grand campaign only.
// 1. The landing card's two amber words (the design director's rulings 36 and 37). The sea behind a beach has
//    three words now: "stays open" when the invader has there at least all the enemy could bring within a month,
//    "contested" when he has at least half of it and less than all, and "can be closed" under half. A landing that
//    takes a whole island where the sea can be closed is told what it is: a force on its own, which holds out for
//    about three months on what it carries. A staff goes on heeding the red warning only.
import { describe, expect, it } from 'vitest';
import { atWar, createGame, monthsUnfed } from '../src/campaign/game';
import { NAVY, crossingsOf, numberWord, sail, seaHold, seaSide, surfaceShips, type CrossingOutlook } from '../src/campaign/navy';
import { heedsCard, startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

describe('the sea behind the beach has three words (rulings 36 and 37)', () => {
  /** A war the computer plays on every side, as it stands once Poland has fallen, with every enemy fleet of Germany's in harbour. */
  const opened = (): Game => {
    const g = startOpen(createGame(3, null));
    for (let k = 0; k < 8 && (k < 2 || g.nations.POL.alive); k++) runTurnAI(g);
    for (const f of g.fleets) if (atWar(g, f.nat, 'GER')) sail(g, f, f.home, 'port');
    return g;
  };
  const toBritain = (g: Game): CrossingOutlook => crossingsOf(g, 'GER').find((x) => x.owner === 'UK' && x.home)!;
  const word = (h: ReturnType<typeof seaHold>): { value: string; effect: string } => {
    const l = h.lines.find((x) => x.label === 'The sea behind the beach')!;
    return { value: String(l.value), effect: l.effect ?? '' };
  };
  /** A German fleet stood in that sea and grown until Germany has `want` there. */
  const grow = (g: Game, sea: number, want: () => number): void => {
    const f = g.fleets.find((k) => k.nat === 'GER' && surfaceShips(k) > 0)!;
    f.base = -1; f.home = sea; f.zone = sea; f.stance = 'patrol';
    for (let n = 0; n < 2000 && seaSide(g, sea, 'GER', false).total <= want(); n++) f.ships.bb += 2;
  };

  it('says "can be closed" under half of what the enemy could bring', () => {
    const g = opened();
    const x = toBritain(g);
    const h = x.account.hold;
    expect(h.foe).toBeGreaterThanOrEqual(NAVY.closed.min);
    expect(h.own.total).toBeLessThan(h.foe / 2);
    expect(h.open).toBe(false);
    expect(h.contested).toBe(false);
    expect(h.island).toBe(false);
    expect(word(h)).toEqual({ value: 'can be closed', effect: 'the force ashore would be cut off from its supplies' });
  });

  it('says "contested" with at least half and less than all of it, and the sea still counts as open', () => {
    const g = opened();
    const x = toBritain(g);
    const before = x.account.hold;
    grow(g, before.sea, () => before.need * 1.1);
    const h = seaHold(g, 'GER', x.path, x.to);
    expect(h.own.total).toBeGreaterThan(h.foe / 2);
    expect(h.own.total).toBeLessThan(h.foe);
    expect(h.open).toBe(true);
    expect(h.contested).toBe(true);
    expect(word(h)).toEqual({ value: 'contested', effect: 'yours today, but he can bring enough to fight you for it' });
    // The line above it says both figures: what keeps it open today, and what he can bring.
    const mine = h.lines.find((l) => l.label.startsWith('Your ships and aircraft'))!;
    expect(mine.effect).toContain('keep it open today');
    expect(mine.effect).toContain('he can bring');
  });

  it('says "stays open" once the invader has there all the enemy could bring', () => {
    const g = opened();
    const x = toBritain(g);
    const sea = x.account.hold.sea;
    grow(g, sea, () => seaHold(g, 'GER', x.path, x.to).foe * 1.02);
    const h = seaHold(g, 'GER', x.path, x.to);
    expect(h.own.total).toBeGreaterThanOrEqual(h.foe);
    expect(h.open).toBe(true);
    expect(h.contested).toBe(false);
    expect(word(h)).toEqual({ value: 'stays open', effect: 'the supply ships reach the force' });
  });

  it('is never contested where the enemy has too little in reach to close any sea', () => {
    const g = opened();
    // Sweden's coast across the Baltic: no enemy of Germany's has a fleet that could come there.
    const baltic = g.byName['Stockholm'].seas[0];
    const h = seaHold(g, 'GER', [baltic], g.byName['Stockholm']);
    expect(h.foe).toBeLessThan(NAVY.closed.min);
    expect(h.need).toBe(0);
    expect(h.open).toBe(true);
    expect(h.contested).toBe(false);
  });

  it('tells a landing that takes a whole island how long it can hold out', () => {
    const g = opened();
    const malta = g.byName['Malta'];
    // An island: a province with no land frontier.
    expect(malta.nb.length).toBe(0);
    const h = seaHold(g, 'GER', [malta.seas[0]], malta);
    expect(h.island).toBe(true);
    expect(h.months).toBe(3);
    expect(h.foe).toBeGreaterThanOrEqual(NAVY.closed.min);
    expect(h.open).toBe(false);
    expect(word(h)).toEqual({ value: 'can be closed', effect: 'cut off, the force can hold out for about three months on what it carries' });
    // A beach on the mainland is no island.
    expect(seaHold(g, 'GER', toBritain(g).path, toBritain(g).to).island).toBe(false);
  });

  it('counts the months from the rule for a corps cut off from its supplies', () => {
    // A fresh corps loses four tenths of its order each month unfed, and lays down its arms when none is left.
    expect(monthsUnfed()).toBe(3);
    expect(monthsUnfed(0.8)).toBe(2);
    expect(monthsUnfed(0.3)).toBe(1);
    expect(numberWord(3)).toBe('three');
    expect(numberWord(12)).toBe('12');
  });

  it('leaves what a staff heeds as it was: red on the mainland only', () => {
    const g = opened();
    expect(heedsCard(toBritain(g).to)).toBe(true);
    expect(heedsCard(g.byName['Malta'])).toBe(false);
  });
});
