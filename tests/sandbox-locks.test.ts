// The sandbox playtest's lock sweep (playtest/sandbox/lock-sweep.md, 2026-11-19): four rules that replayed the record
// for the computer at the loose stops 4 and 5 now hold at stop 3 and the book's stops only. Stop 3 (par) and the
// book's stops are as they were.
// - Finding 1: Britain and the Soviet Union no longer partition Iran for the staffs the month both are at war with
//   Germany ('iran-1941-staffs'): Iran is left to the staffs' own choices and claims.
// - Finding P: Turkey is no longer made to declare war on Germany and Japan from February 1945 ('turkey-war-late'),
//   at the stops that promise a Turkey that may side with the Axis.
// - Finding 6: Britain's computer staff keeps no home floor by the record's calendar of the invasion fear.
// - Finding C: the Dyle Plan's armies keep no standing order for a computer staff: neither held out of its defence
//   nor jumped into Belgium the day it is attacked.
import { describe, expect, it } from 'vitest';
import { monthlyDiplomacy } from '../src/campaign/diplomacy';
import { addCorps, atWar, createGame, declareWar, dyleOrder, setWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { homeFloor, mayLeave } from '../src/campaign/middleeast';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

function grand(stop: number, human: string | null = null, seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  setStop(g, stop, true);
  return g;
}
function at(g: Game, turn: number): void {
  g.turn = turn;
  monthlyDiplomacy(g);
}
const dyle = (g: Game, nat: string): Corps[] => g.corps.filter((c) => c.nat === nat && !c.dead && !!c.dyle);

describe('Iran (finding 1): the staffs partition it at stop 3 alone', () => {
  for (const stop of [3, 4, 5]) {
    it(`stop ${stop}`, () => {
      const g = grand(stop);
      setWar(g, 'GER', 'UK');
      g.nations.SOV.align = 'allies';
      setWar(g, 'GER', 'SOV');
      at(g, 23);
      expect(g.nations.IRN.alive).toBe(stop !== 3);
      expect(!!g.flags['event:iran-1941-staffs']).toBe(stop === 3);
    });
  }
});

describe('Turkey (finding P): the late declaration is made at stop 3 alone', () => {
  for (const stop of [3, 4, 5]) {
    it(`stop ${stop}`, () => {
      const g = grand(stop);
      setWar(g, 'GER', 'UK');
      // An Allied army on Germany's home ground in February 1945.
      const rhine = g.regions.find((r) => r.owner === 'GER' && r.ctrl === 'GER' && !r.overseas && r.nb.some((n) => g.regions[n].owner === 'FRA'))!;
      for (const c of [...rhine.units]) c.dead = true;
      rhine.units = [];
      rhine.ctrl = 'UK';
      addCorps(g, 'UK', 'Test Corps', 'inf', 3, rhine.id, {});
      at(g, 66);
      expect(atWar(g, 'TUR', 'GER')).toBe(stop === 3);
    });
  }
});

describe('Britain\'s home floor (finding 6): the record\'s calendar binds at stop 3 alone', () => {
  it('a third of the field corps in July 1940 at stop 3; none at stops 4 and 5', () => {
    for (const stop of [3, 4, 5]) {
      const g = grand(stop);
      g.turn = 12;
      if (stop === 3) expect(homeFloor(g, 'UK')).toBeGreaterThan(0);
      else {
        expect(homeFloor(g, 'UK'), `stop ${stop}`).toBe(0);
        expect(mayLeave(g, 'UK'), `stop ${stop}`).toBe(Infinity);
      }
    }
  });
});

describe('the Dyle Plan (finding C): a standing order for a computer staff at stop 3 and the book\'s stops alone', () => {
  it('stop 3: France\'s four armies have their orders, and march into Belgium the day it is attacked', () => {
    const g = grand(3);
    const armies = dyle(g, 'FRA');
    expect(armies.map((c) => dyleOrder(g, c))).toEqual(['Namur', 'Brussels', 'Namur', 'Antwerp']);
    setWar(g, 'GER', 'FRA');
    declareWar(g, 'GER', 'BEL');
    expect(armies.filter((c) => c.dyle === null).length).toBeGreaterThan(0);
    for (const c of armies.filter((x) => x.dyle === null)) expect(g.regions[c.reg].owner, c.name).toBe('BEL');
  });

  for (const stop of [4, 5]) {
    it(`stop ${stop}: no order is read; the armies stay where they stand when Belgium is attacked`, () => {
      const g = grand(stop);
      const armies = dyle(g, 'FRA');
      expect(armies.length).toBe(4);
      expect(armies.map((c) => dyleOrder(g, c))).toEqual([null, null, null, null]);
      const where = armies.map((c) => c.reg);
      setWar(g, 'GER', 'FRA');
      declareWar(g, 'GER', 'BEL');
      expect(armies.map((c) => c.reg)).toEqual(where);
      // The orders stay on the corps, so a dial turned back to stop 3 finds them.
      expect(armies.every((c) => !!c.dyle)).toBe(true);
      g.flags.hist = 3;
      expect(armies.map((c) => dyleOrder(g, c))).toEqual(['Namur', 'Brussels', 'Namur', 'Antwerp']);
    });
  }

  it('a player\'s own France keeps its orders at every stop, as before (reactionMoves never moves a player\'s corps)', () => {
    const g = grand(5, 'FRA');
    expect(dyle(g, 'FRA').map((c) => dyleOrder(g, c))).toEqual(['Namur', 'Brussels', 'Namur', 'Antwerp']);
  });
});
