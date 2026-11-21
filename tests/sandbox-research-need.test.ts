// Research by need (the sandbox's lock sweep, finding 7; the historian's thresholds, part 6; after release 1). At
// stops 4 and 5 a computer staff's research answers the war it is in: what it has met pulls its answers, a navy with
// a battle fleet takes carriers in their turn and orders one, and the order of its branches may be dealt from the
// war's seed. The calendar's pull stays whole and each threat met cancels a year of it (the design director's sandbox
// ruling of 20 November 2026, finding A). Stops 1 to 3 and a player's own advisers are as they were.
import { describe, expect, it } from 'vitest';
import { programmeWants } from '../src/campaign/arsenal';
import { addCorps, createGame, frozen, setWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { NEED, battleFleet, needFirst, needKeeps, needOrder, needPull, needStop, needWatch, threatMet, threatsMet } from '../src/campaign/need';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { TECH, staffPick, techOf, techTick } from '../src/campaign/tech';
import type { AirReport, BattleReport, Game } from '../src/campaign/types';

const grand = (stop: number, human: string | null = null, seed = 3): Game => { const g = startOpen(createGame(seed, human)); if (stop !== 3) setStop(g, stop, true); return g; };
const needFlags = (g: Game): string[] => Object.keys(g.flags).filter((k) => k.startsWith('nd:'));
/** What the staff would take up, one after another, with everything it picks counted as done. */
const picks = (g: Game, nat: string, n: number, turn = g.turn): string[] => {
  const s = techOf(g, nat)!, kept = [...s.done], out: string[] = [];
  for (let k = 0; k < n; k++) { const id = staffPick(g, nat, turn); if (!id) break; out.push(id); s.done.push(id); }
  s.done.length = 0; s.done.push(...kept);
  return out;
};
const DEC_41 = 28, JAN_42 = 29;
/** A land battle in which Germany met a Soviet tank corps. */
const tankBattle = (g: Game): BattleReport => ({
  id: g.reports.length + 1, turn: g.turn, week: 1, region: 0, regionName: 'x', attNat: 'GER', defNat: 'SOV',
  attackers: [{ name: 'a', nat: 'GER', kind: 'inf', divs: 3, men: 1, lost: 0, fate: '' }],
  defenders: [{ name: 'd', nat: 'SOV', kind: 'arm', divs: 3, men: 1, lost: 0, fate: '' }],
  lines: [], aPower: 1, dPower: 1, odds: 1, level: 'marginal', taken: false, attLost: 0, defLost: 0, prisoners: 0,
} as unknown as BattleReport);
const raid = (g: Game, attNat: string, defNat: string, mission: string, more: Partial<AirReport> = {}): AirReport => ({
  id: g.airReports.length + 1, turn: g.turn, mission, region: 0, zone: -1, place: 'x', attNat, defNat,
  att: [{ nat: attNat, fleet: 'a', leader: null, ftr: 0, bmr: 100, lostFtr: 0, lostBmr: 0 }],
  def: [{ nat: defNat, fleet: 'd', leader: null, ftr: 50, bmr: 0, lostFtr: 0, lostBmr: 0 }],
  lines: [], odds: 1, level: 'marginal', won: true, sky: false, hold: 0, result: '', ...more,
} as unknown as AirReport);

describe('the data', () => {
  it('names stops 4 and 5, the historian\'s numbers, and answers that are real technologies', () => {
    expect(NEED!.stops).toEqual([4, 5]);
    expect([NEED!.pull, NEED!.most, NEED!.met.battles, NEED!.met.months]).toEqual([30, 60, 3, 2]);
    // A need is one year of the calendar's pull, two at the most for one answer; a winter is met in its first frozen month.
    expect(NEED!.pull).toBe(TECH.ai.early);
    expect(NEED!.most).toBe(2 * TECH.ai.early);
    expect(NEED!.met.by).toEqual({ winter: { months: 1 } });
    expect((NEED as unknown as { early?: unknown }).early).toBeUndefined();
    const ids = new Set(TECH.techs.map((t) => t.id));
    for (const [th, list] of Object.entries(NEED!.threats)) for (const id of list) expect(ids.has(id), `${th}: ${id}`).toBe(true);
    for (const id of NEED!.fleetTechs) expect(ids.has(id)).toBe(true);
    expect(TECH.ai.skip._all).toContain('airborne');
  });
});

describe('stops 1 to 3, and a player\'s own advisers', () => {
  it('read nothing and write nothing', () => {
    for (const stop of [1, 2, 3]) {
      const g = grand(stop);
      expect(needStop(g, 'GER')).toBe(false);
      g.reports.push(tankBattle(g), tankBattle(g), tankBattle(g));
      setWar(g, 'GER', 'SOV');
      needWatch(g, 'GER');
      techTick(g, 'GER');
      expect(needFlags(g)).toEqual([]);
      // Even with a threat on the books from a looser stop, nothing pulls here.
      g.flags['nd:on:GER:armour'] = 1;
      expect(needPull(g, 'GER', 'mediums')).toBe(0);
      expect(needOrder(g, 'GER', TECH.ai.order.GER)).toEqual(TECH.ai.order.GER);
      expect(needKeeps(g, 'GER', 'carriers')).toBe(false);
    }
  });

  it('pick as before at stop 3 whatever the flags hold', () => {
    const g = grand(3);
    const before = picks(g, 'GER', 12);
    for (const th of Object.keys(NEED!.threats)) g.flags[`nd:on:GER:${th}`] = 1;
    expect(picks(g, 'GER', 12)).toEqual(before);
  });

  it('leave a player\'s advisers as they were at stop 5', () => {
    const mine = grand(5, 'GER'), usual = grand(3, 'GER');
    expect(needStop(mine, 'GER')).toBe(false);
    for (const th of Object.keys(NEED!.threats)) mine.flags[`nd:on:GER:${th}`] = 1;
    expect(picks(mine, 'GER', 12)).toEqual(picks(usual, 'GER', 12));
    mine.reports.push(tankBattle(mine), tankBattle(mine), tankBattle(mine));
    needWatch(mine, 'GER');
    expect(needFlags(mine).filter((k) => !k.startsWith('nd:on:'))).toEqual([]);
    // (The computer-run nations beside him do research by need.)
    expect(needStop(mine, 'SOV')).toBe(true);
  });
});

describe('the calendar\'s pull', () => {
  it('stays whole at stops 4 and 5: the year is part of the answer there as at stop 3', () => {
    // The same staff asked in 1939 and in 1945 answers differently, wherever it is asked.
    const at = (stop: number, turn: number): string[] => picks(grand(stop), 'GER', 30, turn);
    for (const stop of [3, 4, 5]) expect(at(stop, 1)).not.toEqual(at(stop, 76));
    // In a war where the staff keeps its own order of branches and has met nothing, a year ahead weighs what it does
    // at stop 3: Germany's staff takes the warning chain (1940, its third branch) before medium tanks (1941, its first).
    const usual = TECH.ai.order.GER;
    const seed = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12].find((k) => needOrder(grand(4, null, k), 'GER', usual).join() === usual.join())!;
    const three = picks(grand(3, null, seed), 'GER', 30), four = picks(grand(4, null, seed), 'GER', 30);
    expect(three.indexOf('mediums')).toBeGreaterThan(three.indexOf('earlywarning'));
    expect(four.indexOf('mediums')).toBeGreaterThan(four.indexOf('earlywarning'));
    expect(four.indexOf('earlywarning')).toBeGreaterThanOrEqual(0);
  });

  it('is cancelled a year by each threat met: enemy tanks met bring medium tanks forward as a year\'s passing would, and no further', () => {
    const usual = TECH.ai.order.GER;
    const seed = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12].find((k) => needOrder(grand(4, null, k), 'GER', usual).join() === usual.join())!;
    const SEP_40 = 13, SEP_41 = 25;
    const calm = grand(4, null, seed), met = grand(4, null, seed);
    met.flags['nd:on:GER:armour'] = 1;
    const place = (g: Game, turn: number, id: string): number => picks(g, 'GER', 30, turn).indexOf(id);
    // In September 1940 medium tanks (1941) are a year off: unmet, the staff takes up what is due before them.
    expect(place(calm, SEP_40, 'mediums')).toBeGreaterThan(place(calm, SEP_40, 'earlywarning'));
    // Met, that year is cancelled: they come first, where they stand unmet a year later.
    expect(place(met, SEP_40, 'mediums')).toBe(0);
    expect(place(calm, SEP_41, 'mediums')).toBe(0);
    // One threat cancels one year and no more: late tanks (1944) still wait behind what is due.
    expect(place(met, SEP_40, 'latetanks')).toBeGreaterThan(place(met, SEP_40, 'escorts'));
    expect(place(met, SEP_40, 'escorts')).toBeGreaterThan(0);
  });
});

describe('a threat met', () => {
  for (const stop of [4, 5]) {
    it(`stop ${stop}: enemy tanks in three battles pull better tanks and the guns that stop them, and the lesson stays learned`, () => {
      const g = grand(stop);
      g.turn = 23;
      setWar(g, 'GER', 'SOV');
      g.reports.push(tankBattle(g), tankBattle(g));
      needWatch(g, 'GER');
      expect(threatMet(g, 'GER', 'armour')).toBe(false);
      expect(g.flags['nd:b:GER:armour']).toBe(2);
      expect(needPull(g, 'GER', 'mediums')).toBe(0);
      g.turn = 24;
      g.reports.push(tankBattle(g));
      needWatch(g, 'GER');
      expect(threatMet(g, 'GER', 'armour')).toBe(true);
      expect(threatsMet(g, 'GER')).toEqual([{ threat: 'armour', since: 24 }]);
      expect(g.flags['nd:b:GER:armour']).toBeUndefined();
      for (const id of ['antitank', 'mediums', 'heavies', 'tankdestroyers', 'latetanks']) expect(needPull(g, 'GER', id)).toBe(30);
      expect(needPull(g, 'GER', 'jets')).toBe(0);
      // The same reports are never counted twice, and the lesson outlasts the war.
      needWatch(g, 'GER');
      expect(g.flags['nd:r:GER:land']).toBe(3);
      const s = saveGame(g);
      expect(threatMet(loadGame(s)!, 'GER', 'armour')).toBe(true);
    });

    it(`stop ${stop}: enemy tanks standing before its ground two months running are met without a battle, and one month alone is not`, () => {
      const g = grand(stop);
      g.turn = 23;
      setWar(g, 'GER', 'SOV');
      // (In 1939 Poland lies between them: a piece of it next to German ground is put in Soviet hands.)
      const home = g.regions.find((r) => r.ctrl === 'GER' && r.nb.some((j) => g.regions[j].ctrl === 'POL'))!;
      const edge = g.regions[home.nb.find((j) => g.regions[j].ctrl === 'POL')!];
      edge.ctrl = 'SOV';
      const c = addCorps(g, 'SOV', 'Test Tank Corps', 'arm', 3, edge.id, {});
      needWatch(g, 'GER');
      expect([threatMet(g, 'GER', 'armour'), g.flags['nd:m:GER:armour']]).toEqual([false, 1]);
      // (Gone for a month: the count starts again.)
      c.dead = true;
      g.turn++;
      needWatch(g, 'GER');
      expect(g.flags['nd:m:GER:armour']).toBeUndefined();
      c.dead = false;
      g.turn++; needWatch(g, 'GER');
      g.turn++; needWatch(g, 'GER');
      expect(threatMet(g, 'GER', 'armour')).toBe(true);
    });

    it(`stop ${stop}: tanks behind its own in the tree are no threat`, () => {
      const g = grand(stop);
      g.turn = 23;
      setWar(g, 'GER', 'SOV');
      techOf(g, 'GER')!.done.push('mediums');
      g.reports.push(tankBattle(g), tankBattle(g), tankBattle(g));
      needWatch(g, 'GER');
      expect(threatMet(g, 'GER', 'armour')).toBe(false);
      techOf(g, 'SOV')!.done.push('mediums');
      g.reports.push(tankBattle(g), tankBattle(g), tankBattle(g));
      needWatch(g, 'GER');
      expect(threatMet(g, 'GER', 'armour')).toBe(true);
    });

    it(`stop ${stop}: a winter in the deep east without the equipment for it is met in its first frozen month, and winter equipment is taken up next`, () => {
      const g = grand(stop);
      setWar(g, 'GER', 'SOV');
      const deep = g.regions.find((r) => r.owner === 'SOV' && !r.overseas)!;
      addCorps(g, 'GER', 'Test Corps', 'inf', 3, deep.id, {});
      g.turn = DEC_41;
      expect(frozen(g, 'GER', deep)).toBe(true);
      expect(staffPick(g, 'GER')).not.toBe('winterkit');
      needWatch(g, 'GER');
      expect(threatMet(g, 'GER', 'winter')).toBe(true);
      expect(threatsMet(g, 'GER')).toEqual([{ threat: 'winter', since: DEC_41 }]);
      expect(needPull(g, 'GER', 'winterkit')).toBe(30);
      g.turn = JAN_42;
      expect(staffPick(g, 'GER')).toBe('winterkit');
      // The Red Army, at home in it, meets no such threat.
      needWatch(g, 'SOV');
      expect(threatMet(g, 'SOV', 'winter')).toBe(false);
    });

    it(`stop ${stop}: bombers over its ground pull the warning chain, costly raids of its own pull escorts and heavier bombers, and two threats pull one answer twice`, () => {
      const g = grand(stop);
      g.turn = 14;
      setWar(g, 'GER', 'UK');
      for (let k = 0; k < 3; k++) g.airReports.push(raid(g, 'GER', 'UK', 'bomb', { att: [{ nat: 'GER', fleet: 'a', leader: null, ftr: 0, bmr: 100, lostFtr: 0, lostBmr: 8 }] } as Partial<AirReport>));
      needWatch(g, 'UK');
      needWatch(g, 'GER');
      expect(threatMet(g, 'UK', 'bombed')).toBe(true);
      expect([needPull(g, 'UK', 'earlywarning'), needPull(g, 'UK', 'gci')]).toEqual([30, 30]);
      expect(threatMet(g, 'GER', 'bombers')).toBe(true);
      // (British fighters with as many fighter technologies as Germany's brought the bombers down: that is met too.)
      expect(threatMet(g, 'GER', 'fighters')).toBe(true);
      expect(needPull(g, 'GER', 'escorts')).toBe(60);
      expect(needPull(g, 'GER', 'heavybombers')).toBe(30);
      // A raid that lost one bomber in a hundred cost nothing worth a lesson.
      const h = grand(stop);
      setWar(h, 'GER', 'UK');
      for (let k = 0; k < 5; k++) h.airReports.push(raid(h, 'GER', 'UK', 'bomb', { att: [{ nat: 'GER', fleet: 'a', leader: null, ftr: 0, bmr: 100, lostFtr: 0, lostBmr: 1 }], def: [{ nat: 'UK', fleet: 'd', leader: null, ftr: 0, bmr: 0, lostFtr: 0, lostBmr: 0 }] } as Partial<AirReport>));
      needWatch(h, 'GER');
      expect(threatsMet(h, 'GER')).toEqual([]);
    });

    it(`stop ${stop}: submarines on its convoy route two months running pull sonar and escort groups`, () => {
      const g = grand(stop);
      setWar(g, 'GER', 'UK');
      for (const t of [5, 6]) {
        g.turn = t;
        g.convoys.atlantic = { turn: t - 1, raid: 30, guard: 10, tons: 100000, total: 0, share: 0.1, loss: 0.2, subsLost: 0, escortsLost: 0 };
        needWatch(g, 'UK');
      }
      expect(threatMet(g, 'UK', 'boats')).toBe(true);
      expect([needPull(g, 'UK', 'sonar'), needPull(g, 'UK', 'escortgroups')]).toEqual([30, 30]);
    });
  }
});

describe('carriers', () => {
  it('are taken in their turn by a navy with a battle fleet at stops 4 and 5, and still left for last by one without', () => {
    for (const stop of [4, 5]) {
      const g = grand(stop);
      expect([battleFleet(g, 'GER'), battleFleet(g, 'ITA'), battleFleet(g, 'POL')]).toEqual([true, true, false]);
      expect([needKeeps(g, 'GER', 'carriers'), needKeeps(g, 'POL', 'carriers'), needKeeps(g, 'GER', 'airborne')]).toEqual([true, false, false]);
      const ger = picks(g, 'ITA', 30), usual = picks(grand(3), 'ITA', 30);
      expect(ger.indexOf('carriers')).toBeGreaterThanOrEqual(0);
      expect(ger.indexOf('carriers')).toBeLessThan(usual.indexOf('carriers') < 0 ? 99 : usual.indexOf('carriers'));
    }
  });

  it('are ordered by a navy with a battle fleet once it has the technology, at stops 4 and 5 only; carrier aircraft at its ships pull the technology', () => {
    for (const stop of [3, 4, 5]) {
      const g = grand(stop);
      setWar(g, 'ITA', 'UK');
      expect(programmeWants(g, 'ITA')).not.toContain('ship:cv');
      techOf(g, 'ITA')!.done.push('carriers');
      expect(programmeWants(g, 'ITA').includes('ship:cv')).toBe(stop >= 4);
      for (let k = 0; k < 3; k++) g.airReports.push(raid(g, 'UK', 'ITA', 'harbour', { att: [{ nat: 'UK', fleet: 'Illustrious', at: [0, 0], leader: null, ftr: 0, bmr: 20, lostFtr: 0, lostBmr: 1 }] } as Partial<AirReport>));
      needWatch(g, 'ITA');
      expect(threatMet(g, 'ITA', 'decks')).toBe(stop >= 4);
      expect(needPull(g, 'ITA', 'carriers')).toBe(stop >= 4 ? 30 : 0);
    }
  });
});

describe('the order of branches', () => {
  it('is dealt from the war\'s seed where anything goes: the same all through one war, not the same in every war, and never for the player', () => {
    const usual = TECH.ai.order.GER;
    const orders = new Set<string>();
    for (let seed = 1; seed <= 12; seed++) {
      const g = grand(5, null, seed);
      const o = needOrder(g, 'GER', usual);
      expect([...o].sort()).toEqual([...usual].sort());
      g.turn = 30;
      expect(needOrder(g, 'GER', usual)).toEqual(o);
      orders.add(o.join());
    }
    expect(orders.size).toBeGreaterThan(3);
    // At stop 4 about half the staffs keep their own order.
    let kept = 0;
    for (let seed = 1; seed <= 40; seed++) if (needOrder(grand(4, null, seed), 'GER', usual).join() === usual.join()) kept++;
    expect(kept).toBeGreaterThan(8);
    expect(kept).toBeLessThan(32);
  });

  it('puts the naval branch last for a nation with no ships, however the deal fell', () => {
    for (let seed = 1; seed <= 12; seed++) {
      const g = grand(5, null, seed);
      expect(g.fleets.some((f) => f.nat === 'HUN')).toBe(false);
      expect(needOrder(g, 'HUN', TECH.ai.order._default).at(-1)).toBe('naval');
    }
  });
});

// The design director's sandbox ruling of 21 November 2026, finding B. Traced in 48 wars, a staff that had met a
// winter gave its next free team to something else in 29 of 45: 57 of the 60 answers that went first were second or
// third answers to a lesson it had already answered. A real staff with nothing against a threat it has met reaches
// for that first. It is a ranking for every staff and every lesson, and sets no number.
describe('a lesson with nothing yet to show for it', () => {
  /** Germany in January 1942 with every team free: tanks and bombing met long ago and answered, the winter just met. */
  const lessons = (stop: number, human: string | null = null): Game => {
    const g = grand(stop, human);
    g.turn = JAN_42;
    const s = techOf(g, 'GER')!;
    s.work.length = 0;
    for (const id of ['antitank', 'institutes', 'earlywarning', 'mediums', 'heavyguns', 'smg', 'sonar', 'synthetics', 'carriers']) if (!s.done.includes(id)) s.done.push(id);
    for (const th of ['armour', 'bombed', 'winter']) g.flags[`nd:on:GER:${th}`] = DEC_41;
    return g;
  };

  for (const stop of [4, 5]) {
    it(`stop ${stop}: is told from the laboratories: a threat met, none of whose answers is in service or at work`, () => {
      const g = lessons(stop), s = techOf(g, 'GER')!;
      expect(needFirst(g, 'GER', 'winterkit')).toBe(true);
      // Tanks and bombing have their first answers in service: a further answer is no such lesson.
      for (const id of ['heavies', 'tankdestroyers', 'latetanks', 'gci']) expect(needFirst(g, 'GER', id)).toBe(false);
      // A threat not met is no lesson at all, and nor is a technology that answers none.
      for (const id of ['escorts', 'heavybombers', 'labs', 'hydrogenation']) expect(needFirst(g, 'GER', id)).toBe(false);
      // Once the answer is at work, or in service, the lesson has something to show for it.
      s.work.push({ id: 'winterkit', left: 11, of: 11 });
      expect(needFirst(g, 'GER', 'winterkit')).toBe(false);
      s.work.length = 0;
      s.done.push('winterkit');
      expect(needFirst(g, 'GER', 'winterkit')).toBe(false);
    });

    it(`stop ${stop}: takes the next free team ahead of a further answer to a lesson already answered, and the score decides the rest`, () => {
      const g = lessons(stop), s = techOf(g, 'GER')!;
      // Heavy tanks and fighter control answer lessons too, but those lessons have answers in service.
      const order = picks(g, 'GER', 12);
      expect(order[0]).toBe('winterkit');
      expect(order.indexOf('heavies')).toBeGreaterThan(0);
      expect(order.indexOf('gci')).toBeGreaterThan(0);
      // With the winter unmet the same staff takes its further answers first: the kit waits its turn.
      delete g.flags['nd:on:GER:winter'];
      const calm = picks(g, 'GER', 12);
      expect(calm.slice(0, 2)).toEqual(['heavies', 'gci']);
      expect(calm).not.toContain('winterkit');
      // And once the kit is at work the staff picks as it would have without the lesson, further answers first.
      g.flags['nd:on:GER:winter'] = DEC_41;
      s.work.push({ id: 'winterkit', left: 11, of: 11 });
      expect(picks(g, 'GER', 12)).toEqual(calm);
      expect(order).toEqual(['winterkit', ...calm.slice(0, 11)]);
    });

    it(`stop ${stop}: lifts nothing the staff's own list leaves for last: carriers, for a navy that has lost its battle fleet`, () => {
      const g = lessons(stop), s = techOf(g, 'GER')!;
      s.done.splice(s.done.indexOf('carriers'), 1);
      s.done.push('winterkit');
      g.flags['nd:on:GER:decks'] = DEC_41;
      // With its battle fleet the navy takes carriers in their turn, and the lesson of the enemy's decks comes first.
      expect(battleFleet(g, 'GER')).toBe(true);
      expect(needFirst(g, 'GER', 'carriers')).toBe(true);
      const afloat = picks(g, 'GER', 40);
      expect(afloat[0]).toBe('carriers');
      // Without one its list leaves carriers for last, and no lesson lifts them: its yards would lay none down.
      for (const f of g.fleets) if (f.nat === 'GER') { f.ships.bb = 0; f.ships.ca = 0; f.ships.cv = 0; }
      expect(battleFleet(g, 'GER')).toBe(false);
      expect(needFirst(g, 'GER', 'carriers')).toBe(true);
      const sunk = picks(g, 'GER', 40);
      expect(sunk.indexOf('carriers')).toBeGreaterThan(11);
      // The rest of its choice is what it would have been with the lesson answered.
      expect(sunk.filter((id) => id !== 'carriers').slice(0, 10)).toEqual(afloat.filter((id) => id !== 'carriers').slice(0, 10));
    });

    it(`stop ${stop}: only where the threats met cancel every year it would be hurried: beyond that it takes its turn`, () => {
      const g = lessons(stop);
      // December 1941: the kit (1942) is a year off, and the one threat met cancels that year.
      expect(staffPick(g, 'GER', DEC_41)).toBe('winterkit');
      // December 1940: two years off, one cancelled. The calendar's price stands for the other, and what is due comes first.
      const DEC_40 = DEC_41 - 12;
      expect(staffPick(g, 'GER', DEC_40)).not.toBe('winterkit');
    });
  }

  it('is not read at stop 3, nor for a player\'s own advisers where anything goes', () => {
    const g = lessons(3);
    expect(needFirst(g, 'GER', 'winterkit')).toBe(false);
    expect(picks(g, 'GER', 12)[0]).not.toBe('winterkit');
    const h = lessons(5, 'GER');
    expect(needFirst(h, 'GER', 'winterkit')).toBe(false);
  });
});
