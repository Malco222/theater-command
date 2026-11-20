// Armies earn their strength (the sandbox playtest's lock sweep, finding 3; stops 4 and 5 only): a nation's later
// mobilisation rows, its leave to build and the four reforms come on its own war's clock, not the record's calendar.
// Stops 1 to 3 are as they were.
import { describe, expect, it } from 'vitest';
import { monthlyDiplomacy } from '../src/campaign/diplomacy';
import { canBuildYet } from '../src/campaign/economy';
import { createGame, setWar, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';
import { greatWarOf } from '../src/campaign/warclock';

function grand(stop: number, seed = 3): Game {
  const g = startOpen(createGame(seed, null));
  setStop(g, stop, true);
  return g;
}
/** Runs the Soviet Union's month openings from 'from' to 'to', and returns the month each named corps came. */
function sovietMonths(g: Game, from: number, to: number, war?: number): Record<string, number> {
  const came: Record<string, number> = {};
  for (let t = from; t <= to; t++) {
    g.turn = t;
    if (t === war) {
      setWar(g, 'GER', 'SOV');
      g.flags['war:GER:SOV'] = t;
      g.flags['lb:GER:SOV'] = t;
    }
    const before = new Set(g.corps.map((c) => c.id));
    startPhase(g, 'SOV');
    for (const c of g.corps) if (!before.has(c.id) && c.nat === 'SOV') came[c.name] = t;
  }
  return came;
}

describe('the Soviet Union\'s later armies (27-28 and the Kursk fronts at 46)', () => {
  it('stop 3: they come on the record\'s months, at war or not, and the Kursk fronts come held', () => {
    const g = grand(3);
    const came = sovietMonths(g, 23, 46);
    expect(came['1st Shock Army']).toBe(27);
    expect(came['Siberian rifle divisions']).toBe(28);
    expect(came['Central Front']).toBe(46);
    expect(g.corps.find((c) => c.name === 'Central Front')!.hold).toBe(true);
    expect(greatWarOf(g, 'SOV')).toBeUndefined();
  });

  it('stop 5: none come while the Soviet Union is at peace with every great power', () => {
    const g = grand(5);
    const came = sovietMonths(g, 23, 50);
    expect(came['1st Shock Army']).toBeUndefined();
    expect(came['Central Front']).toBeUndefined();
  });

  it('stop 5: a German war begun in month 30 brings them 5, 6 and 24 months into it, the Kursk fronts held', () => {
    const g = grand(5);
    const came = sovietMonths(g, 23, 54, 30);
    expect(greatWarOf(g, 'SOV')).toBe(30);
    expect(came['1st Shock Army']).toBe(35);
    expect(came['Siberian rifle divisions']).toBe(36);
    expect(came['Central Front']).toBe(54);
    // They keep their hold until their own cause (the hold rule: sandbox-hold.test.ts).
    expect(g.corps.find((c) => c.name === 'Central Front')!.hold).toBe(true);
  });

  it('stop 4: a German war on the record\'s month brings them on the record\'s months', () => {
    const g = grand(4);
    const came = sovietMonths(g, 22, 46, 22);
    expect(came['1st Shock Army']).toBe(27);
    expect(came['Far East rifle divisions']).toBe(28);
    expect(came['Steppe Front']).toBe(46);
  });
});

describe('a computer nation\'s leave to build', () => {
  it('stop 3: the Soviet Union builds from month 29 once at war, whatever war it is', () => {
    const g = grand(3);
    setWar(g, 'SOV', 'FIN');
    g.turn = 28;
    expect(canBuildYet(g, 'SOV')).toBe(false);
    g.turn = 29;
    expect(canBuildYet(g, 'SOV')).toBe(true);
  });

  it('stop 5: in a small war only, after its rows by date (month 21); in a great war, once its last row on the clock is due', () => {
    const g = grand(5);
    setWar(g, 'SOV', 'FIN');
    g.turn = 21;
    expect(canBuildYet(g, 'SOV')).toBe(false);
    g.turn = 22;
    expect(canBuildYet(g, 'SOV')).toBe(true);
    sovietMonths(g, 23, 30, 30);
    g.turn = 36;
    expect(canBuildYet(g, 'SOV')).toBe(false);
    g.turn = 37;
    expect(canBuildYet(g, 'SOV')).toBe(true);
  });

  it('Germany, at war from the first day, builds from month 21 at every stop', () => {
    for (const stop of [3, 5]) {
      const g = grand(stop);
      if (stop === 5) startPhase(g, 'GER');
      g.turn = 20;
      expect(canBuildYet(g, 'GER'), `stop ${stop}`).toBe(false);
      g.turn = 21;
      expect(canBuildYet(g, 'GER'), `stop ${stop}`).toBe(true);
    }
  });
});

describe('the four reforms', () => {
  const at = (g: Game, turn: number): void => {
    g.turn = turn;
    monthlyDiplomacy(g);
  };
  it('stop 3: the Red Army of 1943 comes in June 1943 to a German war begun in month 30', () => {
    const g = grand(3);
    setWar(g, 'GER', 'SOV');
    g.flags['war:GER:SOV'] = 30;
    at(g, 46);
    expect(g.flags['event:red-army-1943']).toBe(46);
  });

  it('stop 5: they never come by date, nor by months at war (the one rule: sandbox-reforms.test.ts)', () => {
    const g = grand(5);
    setWar(g, 'GER', 'SOV');
    setWar(g, 'GER', 'USA');
    g.flags['war:GER:SOV'] = 30;
    g.flags['war:GER:USA'] = 15;
    for (const t of [46, 54, 57, 70]) at(g, t);
    for (const id of ['red-army-1943', 'luftwaffe-1944', 'allied-armies-1944', 'german-army-1944']) expect(g.flags[`event:${id}`]).toBeUndefined();
  });

  it('stop 3: the same war brings them on the record\'s months', () => {
    const g = grand(3);
    setWar(g, 'GER', 'USA');
    g.flags['war:GER:USA'] = 15;
    at(g, 44);
    expect(g.flags['event:allied-armies-1944']).toBeUndefined();
    at(g, 57);
    expect(g.flags['event:allied-armies-1944']).toBe(57);
  });
});

describe('where a clock starts (the historian, H)', () => {
  it('stop 5: a declaration with no fighting starts nothing; the first land battle does', () => {
    const g = grand(5);
    // Britain and France declare on the Soviet Union over Turkey in month 11 and never fight it.
    setWar(g, 'UK', 'SOV');
    g.flags['war:UK:SOV'] = 11;
    const came = sovietMonths(g, 12, 21);
    expect(greatWarOf(g, 'SOV')).toBeUndefined();
    expect(came['1st Shock Army']).toBeUndefined();
    // Germany attacks in month 22, and the first land battle is fought that month.
    setWar(g, 'GER', 'SOV');
    g.flags['war:GER:SOV'] = 22;
    g.flags['lb:GER:SOV'] = 22;
    const later = sovietMonths(g, 23, 27);
    expect(greatWarOf(g, 'SOV')).toBe(22);
    expect(later['1st Shock Army']).toBe(27);
  });

  it('stop 5: Romania\'s and Hungary\'s May 1941 call-ups come on their date, at war or not', () => {
    const g = grand(5);
    const came: Record<string, number> = {};
    for (let t = 20; t <= 22; t++) {
      g.turn = t;
      for (const nat of ['ROM', 'HUN']) {
        const before = new Set(g.corps.map((c) => c.id));
        startPhase(g, nat);
        for (const c of g.corps) if (!before.has(c.id)) came[`${c.nat} ${c.name}`] = t;
      }
    }
    expect(came['ROM II Corps']).toBe(21);
    expect(came['HUN Mobile Corps']).toBe(21);
  });
});
