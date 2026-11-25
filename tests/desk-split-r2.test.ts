// The building desk split in two (release 2; the design director's ruling on seamless play, seamless-r2-1125.md part
// A, for Brian's 25 November ask: "easy to manage the cities, recruitment, ect"). Recruitment keeps the old desk's id
// and flag ('build': troopships for the named crossing, new corps, support teams, aircraft when there are no men);
// towns and trade is new ('towns': trade agreements, repairs to towns and airfields, the guns round bombed industry,
// building in the towns). Both draw on the one allowance, in the one desk's order. The gate:
// 1. an old save with the desk handed over plays as it did: both halves, no new flag, one line a month;
// 2. both halves handed over through the switches are the one desk exactly: the same flag, the same war;
// 3. each half alone: the other half's routines do not run (one test per routine);
// 4. the towns staff does not wait for guns while the player keeps raising the forces himself;
// 5. a war nobody plays takes no notice of either half.
// (The same months against the build before the split, byte for byte, are proved outside the suite: see the notes
// sent with this work.)
import { afterEach, describe, expect, it, vi } from 'vitest';
import * as arsenal from '../src/campaign/arsenal';
import * as buildings from '../src/campaign/buildings';
import { DEPTH, DESKS, setDesk, staffRuns } from '../src/campaign/depth';
import { commandView, handOver, staffMonth } from '../src/campaign/desks';
import * as economy from '../src/campaign/economy';
import * as fieldstate from '../src/campaign/fieldstate';
import { createGame } from '../src/campaign/game';
import * as navy from '../src/campaign/navy';
import * as repairs from '../src/campaign/repairs';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import * as trade from '../src/campaign/trade';
import { beginHuman, endHuman, humanUp, orderDesk, runTurnAI, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

/** A grand campaign brought to the player's first month, which is not yet opened. */
function ready(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  return g;
}
/** Ends the player's month without an order of his own and opens the next. */
function month(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
}
const dk = (g: Game): Record<string, number> => Object.fromEntries(Object.entries(g.flags).filter(([k]) => k === 'dk:build' || k === 'dk:towns'));
const tagged = (g: Game, turn = g.turn) => g.headlines.filter((h) => h.turn === turn && (h.desk === 'build' || h.desk === 'towns'));
/** Flags only the split writes: none of them is ever written while the two halves go together. */
const SPLIT_ONLY = ['dk:towns', 'dk:ran:only', 'dk:ran:towns', 'dk:said:towns', 'dk:saidAt:towns'];

afterEach(() => { vi.restoreAllMocks(); });

describe('two halves, one switch each', () => {
  it('the desks are listed recruitment first, then towns and trade, and Field command hands over both', () => {
    expect(DESKS.slice(0, 3)).toEqual(['research', 'build', 'towns']);
    expect(DEPTH.desks.find((d) => d.id === 'build')!.name).toBe('Recruitment');
    expect(DEPTH.desks.find((d) => d.id === 'towns')!.name).toBe('Towns and trade');
    expect(DEPTH.depths.find((d) => d.id === 'field')!.staff).toEqual(expect.arrayContaining(['build', 'towns']));
    for (const id of ['theatre', 'supreme']) expect(DEPTH.depths.find((d) => d.id === id)!.staff).not.toContain('towns');
  });

  it('every way between the four states writes towns and trade\'s own flag only while the halves stand apart', () => {
    const g = ready();
    const at = (forces: boolean, towns: boolean): void => {
      expect(staffRuns(g, 'GER', 'build')).toBe(forces);
      expect(staffRuns(g, 'GER', 'towns')).toBe(towns);
      expect(dk(g)).toEqual(forces && towns ? { 'dk:build': 1 } : forces ? { 'dk:build': 1, 'dk:towns': 0 } : towns ? { 'dk:towns': 1 } : {});
    };
    at(false, false);
    expect(setDesk(g, 'towns', true)).toBe(true); at(false, true);
    expect(setDesk(g, 'towns', true)).toBe(false);
    expect(setDesk(g, 'build', true)).toBe(true); at(true, true);
    expect(setDesk(g, 'towns', false)).toBe(true); at(true, false);
    expect(setDesk(g, 'build', false)).toBe(true); at(false, false);
    expect(setDesk(g, 'build', true)).toBe(true); at(true, false);
    expect(setDesk(g, 'towns', true)).toBe(true); at(true, true);
    expect(setDesk(g, 'build', false)).toBe(true); at(false, true);
    expect(setDesk(g, 'towns', false)).toBe(true); at(false, false);
  });
});

describe('the gate', () => {
  it('1. an old save with the desk handed over plays its months as the one desk did: both halves, no new flag, one line', () => {
    const g0 = ready();
    g0.flags['dk:build'] = 1;
    const g = loadGame(saveGame(g0))!;
    expect(staffRuns(g, 'GER', 'build')).toBe(true);
    expect(staffRuns(g, 'GER', 'towns')).toBe(true);
    beginHuman(g);
    for (let k = 0; k < 24 && g.nations.GER.alive; k++) {
      const lines = tagged(g);
      expect(lines.length).toBeLessThanOrEqual(1);
      for (const h of lines) { expect(h.desk).toBe('build'); expect(h.text.startsWith('Your staff ')).toBe(true); }
      for (const f of SPLIT_ONLY) expect(g.flags[f]).toBeUndefined();
      month(g);
    }
  }, 240_000);

  it('2. both halves handed over through the switches, in either order, are the one desk: the same flag and the same war', () => {
    const play = (how: 'flag' | 'forces-first' | 'towns-first'): string => {
      const g = ready('SOV', 5);
      if (how === 'flag') g.flags['dk:build'] = 1;
      else if (how === 'forces-first') { setDesk(g, 'build', true); setDesk(g, 'towns', true); } else { setDesk(g, 'towns', true); setDesk(g, 'build', true); }
      expect(dk(g)).toEqual({ 'dk:build': 1 });
      beginHuman(g);
      for (let k = 0; k < 12; k++) month(g);
      return saveGame(g);
    };
    const one = play('flag');
    expect(play('forces-first')).toBe(one);
    expect(play('towns-first')).toBe(one);
  }, 240_000);

  describe('3. each half alone: the other half\'s routines do not run', { timeout: 180_000 }, () => {
    /** The staff's month for GER at war with money to spend, with the halves handed over as asked; what each spy saw. */
    function staffed(forces: boolean, towns: boolean, spy: () => { mock: { calls: unknown[][] } }): unknown[][] {
      const g = ready('GER', 3);
      beginHuman(g);
      month(g);
      for (const k of DESKS) setDesk(g, k, false);
      if (forces) setDesk(g, 'build', true);
      if (towns) setDesk(g, 'towns', true);
      delete g.flags['dk:ran:build']; delete g.flags['dk:ran:towns']; delete g.flags['dk:ran:only']; delete g.flags['dk:alwt'];
      g.nations.GER.pp = 4000;
      g.nations.GER.pc = 50;
      const s = spy();
      const before = s.mock.calls.length;
      staffMonth(g);
      return s.mock.calls.slice(before);
    }
    const counts = (spy: () => { mock: { calls: unknown[][] } }): number[] => [[false, false], [true, false], [false, true], [true, true]].map(([f, t]) => staffed(f, t, spy).length);

    it('new corps (aiProduction) run with recruitment only', () => {
      expect(counts(() => vi.spyOn(economy, 'aiProduction'))).toEqual([0, 1, 0, 1]);
    });
    it('support teams (aiTeams) run with recruitment only', () => {
      expect(counts(() => vi.spyOn(economy, 'aiTeams'))).toEqual([0, 1, 0, 1]);
    });
    it('troopships for the named crossing (shipping) run with recruitment only', () => {
      // (The crossing is asked for elsewhere in the month too, by the month's report on it: what counts is the one more.)
      const [none, forces, towns, both] = counts(() => vi.spyOn(navy, 'plannedCrossing'));
      expect([forces - none, towns - none, both - none]).toEqual([1, 0, 1]);
    });
    it('aircraft when there are no men (machines) run with recruitment only', () => {
      const M = DEPTH.build!.machines!;
      let read = 0;
      Object.defineProperty(DEPTH.build!, 'machines', { configurable: true, enumerable: true, get: () => { read++; return M; } });
      try {
        const seen = [[false, false], [true, false], [false, true], [true, true]].map(([f, t]) => { read = 0; staffed(f, t, () => vi.spyOn(economy, 'income')); return read; });
        expect(seen[0]).toBe(0);
        expect(seen[1]).toBeGreaterThan(0);
        expect(seen[2]).toBe(0);
        expect(seen[3]).toBeGreaterThan(0);
      } finally {
        Object.defineProperty(DEPTH.build!, 'machines', { configurable: true, enumerable: true, writable: true, value: M });
      }
    });
    it('trade agreements run with towns and trade only', () => {
      const [none, forces, towns, both] = counts(() => vi.spyOn(trade, 'dealsOf'));
      expect(forces - none).toBe(0);
      expect(towns - none).toBeGreaterThan(0);
      expect(both - none).toBe(towns - none);
    });
    it('repairs to towns (aiRepair) and to airfields (aiFieldRepair) run with towns and trade only', () => {
      expect(counts(() => vi.spyOn(repairs, 'aiRepair'))).toEqual([0, 0, 1, 1]);
      expect(counts(() => vi.spyOn(fieldstate, 'aiFieldRepair'))).toEqual([0, 0, 1, 1]);
    });
    it('building in the towns (civicAI) runs with towns and trade only', () => {
      expect(counts(() => vi.spyOn(buildings, 'civicAI'))).toEqual([0, 0, 1, 1]);
    });
    it('the arsenal is asked for the guns round bombed industry by towns and trade, and for airfields by recruitment', () => {
      const asked = (forces: boolean, towns: boolean) => staffed(forces, towns, () => vi.spyOn(arsenal, 'aiArsenal')).map((c) => [c[2], c[3]]);
      expect(asked(false, false)).toEqual([]);
      expect(asked(true, false)).toEqual([[false, true]]);
      expect(asked(false, true)).toEqual([[true, false]]);
      expect(asked(true, true)).toEqual([[true, true]]);
    });
    it('a half handed over later in the month works then, once, and adds what it spends to the month\'s account', () => {
      const g = ready('GER', 3);
      setDesk(g, 'build', true);
      beginHuman(g);
      month(g);
      expect(g.flags['dk:ran:only']).toBe(g.turn);
      const spent = g.flags['dk:spent'] ?? 0;
      const civic = vi.spyOn(buildings, 'civicAI');
      expect(orderDesk(g, 'towns', true)).toBe(true);
      expect(civic.mock.calls.length).toBe(1);
      expect(g.flags['dk:spent']).toBeGreaterThanOrEqual(spent);
      expect(commandView(g)!.spent).toBe(g.flags['dk:spent']);
      // Taken back and handed over again in the same month, it does not work twice.
      handOver(g, 'towns', false, true);
      handOver(g, 'towns', true, true);
      expect(civic.mock.calls.length).toBe(1);
    });
  });

  it('4. the towns staff does not wait for guns while the player keeps raising the forces himself; with both, it waits as before', () => {
    vi.spyOn(economy, 'gunsWanted').mockReturnValue(true);
    const wait = (forces: boolean): unknown => {
      const g = ready('GER', 3);
      setDesk(g, 'towns', true);
      if (forces) setDesk(g, 'build', true);
      const civic = vi.spyOn(buildings, 'civicAI');
      beginHuman(g);
      const call = civic.mock.calls.at(-1)!;
      civic.mockRestore();
      return call[4];
    };
    expect(wait(false)).toBe(false);
    expect(wait(true)).toBe(true);
  });

  it('5. a war nobody plays takes no notice of either half, set together or apart', () => {
    const play = (flags: Record<string, number>): string => {
      const g = startOpen(createGame(9, null));
      Object.assign(g.flags, flags);
      for (let k = 0; k < 24; k++) runTurnAI(g);
      const s = JSON.parse(saveGame(g));
      for (const k of Object.keys(s.flags)) if (k.startsWith('dk:')) delete s.flags[k];
      return JSON.stringify(s);
    };
    const plain = play({});
    expect(play({ 'dk:build': 1 })).toBe(plain);
    expect(play({ 'dk:towns': 1 })).toBe(plain);
    expect(play({ 'dk:build': 1, 'dk:towns': 0 })).toBe(plain);
  }, 400_000);
});

describe('what the player sees', () => {
  type Call = (...a: unknown[]) => unknown;
  const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
  interface Half { name: string; on: boolean; act: string; tip: string }
  interface Strip { on: boolean; line: string; head: string; desks: Half[] }
  const strip = (): Strip => (TC().homeFront() as { staff: Strip }).staff;
  const act = (a: string): { ok: boolean; note: string } => TC().act(a) as { ok: boolean; note: string };

  it('THE HOME FRONT\'s staff strip carries both halves, each with its own switch, and says who gives the orders', () => {
    TC().start({ human: 'GER', seed: 3, view: 'grand' });
    let s = strip();
    expect(s.head).toBe('YOU GIVE THESE ORDERS YOURSELF');
    expect(s.desks.map((d) => [d.name, d.on, d.act])).toEqual([['RECRUITMENT', false, 'desk:build:staff'], ['TOWNS AND TRADE', false, 'desk:towns:staff']]);
    expect(act('desk:towns:staff').ok).toBe(true);
    s = strip();
    expect(s.head).toBe('YOU AND YOUR STAFF SHARE THESE ORDERS');
    expect(s.line).toMatch(/^Your ministers run the towns and trade; raising the forces is yours\. /);
    expect(s.desks.map((d) => [d.on, d.act])).toEqual([[false, 'desk:build:staff'], [true, 'desk:towns:own']]);
    expect(act('desk:build:staff').ok).toBe(true);
    s = strip();
    expect(s.head).toBe('YOUR STAFF GIVES THESE ORDERS');
    expect(s.line).toMatch(/^Your staff raises the forces and your ministers run the towns and trade\. /);
  });

  it('the month card\'s line that told both halves\' month takes both back', () => {
    TC().start({ human: 'GER', seed: 3, view: 'grand' });
    act('desk:build:staff');
    act('desk:towns:staff');
    const back = act('desk:build+towns:own');
    expect(back.ok).toBe(true);
    expect(back.note).toBe('Recruitment, and towns and trade, are on your own desk again. What your staff ordered this month stands.');
    expect(strip().desks.every((d) => !d.on)).toBe(true);
    expect(act('desk:build+towns:own').ok).toBe(false);
  });
});
