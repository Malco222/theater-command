// The Pacific Fleet's carriers in the month of the strike (ruling 234(d)).
//
// The carriers are a row of their own, the Pacific Carrier Fleet. At the stops of the history dial that keep the book
// the record's own move has them at sea in the month Japan strikes south, though America is at peace, and back at
// their base the month after. Where no book is kept, a fleet with a 'cruise' on its row spends a month of peace at sea
// on an even chance by the war's seed and the month, so that a strike opening a war finds it in harbour or does not,
// and nobody can read which from where a data row sits. With the far side's switch off none of it exists
// (tests/pacific-war-off.test.ts).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import farWorld from '../data/campaign/far-world.json';
import { shipsInPort } from '../src/campaign/air';
import { createGame, declareWar } from '../src/campaign/game';
import { HISTORY, bookDate, setStop } from '../src/campaign/history';
import { NAVY, atSea, navalUpkeep } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game } from '../src/campaign/types';

const NAME = 'Pacific Carrier Fleet';
const war = (stop: number, human: string | null = null, seed = 3): Game => {
  const g = startOpen(createGame(seed, human));
  if (stop !== 3) setStop(g, stop, true);
  return g;
};
const fleet = (g: Game, name: string): Fleet => g.fleets.find((f) => f.nat === 'USA' && f.name === name)!;
/** America's months at sea, one after another, from the month after the game's own to `to`. */
const months = (g: Game, to: number, each?: (turn: number) => void): void => {
  for (let t = g.turn + 1; t <= to; t++) { g.turn = t; navalUpkeep(g, 'USA'); each?.(t); }
};
const sea = (g: Game, f: Fleet): string => g.seas[f.zone].key;

describe('the Pacific Fleet\'s carriers (ruling 234(d))', () => {
  it('are a row of their own: the carriers and the screen that sailed with them, off the Pacific Fleet\'s row', () => {
    const g = war(1);
    const cf = fleet(g, NAME), pf = fleet(g, 'Pacific Fleet');
    expect(cf.ships).toMatchObject({ bb: 0, cv: 4, ca: 6, dd: 14 });
    expect(pf.ships.cv).toBe(0);
    expect(pf.ships.bb).toBe(12);
    expect(cf.admiral?.name).toBe('William Halsey');
    expect(g.regions[cf.base].name).toBe('San Diego');
    // The row's own mark for the stops with no book, and the record's month at sea with the choice whose month it keeps.
    expect(cf.cruise).toEqual(['midway', 0.5]);
    const moves = (farWorld as unknown as { navy: { moves: { fleet: string; turn: number; sea?: string; with?: string; stops?: number[] }[] } }).navy.moves;
    expect(moves.filter((m) => m.sea !== undefined)).toEqual([expect.objectContaining({ fleet: NAME, turn: 28, sea: 'midway', with: 'JAP:south', stops: [1, 2] })]);
    expect(NAVY.moves!.some((m) => m.fleet === NAME && m.sea === 'midway')).toBe(true);
    // Nobody else's fleet cruises.
    expect(g.fleets.filter((f) => f.cruise).map((f) => f.name)).toEqual([NAME]);
    // Every carrier the Pacific gains or loses by date comes and goes on its row.
    for (const a of NAVY.arrivals) if (a[1] === 'USA' && a[2] === 'Pacific Fleet') expect(a[3].cv ?? 0, a[4]).toBe(0);
  });

  it('by the book they are at sea in the month of the strike, though America is at peace, and the battle line is not', () => {
    const g = war(1);
    months(g, 27);
    const cf = fleet(g, NAME), pf = fleet(g, 'Pacific Fleet');
    // (They went to Pearl Harbor with the fleet in May 1940.)
    expect([g.regions[cf.base].name, cf.port, cf.stance]).toEqual(['Pearl Harbor', 'Pearl Harbor', 'port']);
    expect(g.regions[pf.base].name).toBe('Pearl Harbor');
    expect(cf.ready).toBe(1);
    g.turn = 28;
    navalUpkeep(g, 'USA');
    expect(g.nations.USA.atWar.length).toBe(0);
    expect([atSea(cf), sea(g, cf), cf.stance, cf.cruised]).toEqual([true, 'midway', 'patrol', 28]);
    expect(atSea(pf)).toBe(false);
    // A cruise, not a patrol: it wears nothing.
    expect(cf.ready).toBe(1);
    expect(g.headlines.some((h) => h.nat === 'USA' && (h.text ?? '') === 'The Pacific Fleet\'s carriers are at sea, flying fighters to Wake and Midway.')).toBe(true);
    // Japan's blow that month finds the battleships in harbour and no carrier.
    declareWar(g, 'JAP', 'USA');
    const there = shipsInPort(g, pf.base, 'JAP').map((f) => f.name);
    expect(there).toContain('Pacific Fleet');
    expect(there).not.toContain(NAME);
    // The month after they are back at their base, and from there their admiral's.
    g.turn = 29;
    navalUpkeep(g, 'USA');
    expect([atSea(cf), sea(g, cf), cf.cruised]).toEqual([false, 'hawaii', undefined]);
  });

  it('no strike, no war: the month after they lie in harbour again, as every fleet of a country at peace', () => {
    const g = war(1);
    months(g, 28);
    const cf = fleet(g, NAME);
    expect(atSea(cf)).toBe(true);
    months(g, 40, (t) => expect(atSea(cf), `month ${t}`).toBe(false));
    expect(cf.cruised).toBeUndefined();
  });

  it('where the strike\'s month is drawn (stop 2) the month at sea is the strike\'s, whenever the draw puts it', () => {
    const seen = new Set<number>();
    for (let seed = 1; seed <= 12; seed++) {
      const g = war(2, null, seed);
      const strike = bookDate(g, 'JAP', 'south')!;
      seen.add(strike - HISTORY.dates.JAP.south);
      const out: number[] = [];
      const cf = fleet(g, NAME);
      months(g, 34, (t) => { if (atSea(cf)) out.push(t); });
      expect(out, `seed ${seed}`).toEqual([strike]);
    }
    // (The draw does move the month in some of these wars: the test is not of December alone.)
    expect(seen.size).toBeGreaterThan(1);
  });

  it('where no book is kept they are at sea in a month of peace on an even chance, by the seed and the month', () => {
    const run = (seed: number, stop = 3): number[] => {
      const g = war(stop, null, seed);
      const cf = fleet(g, NAME), out: number[] = [];
      months(g, 60, (t) => {
        if (atSea(cf)) { out.push(t); expect(sea(g, cf)).toBe('midway'); expect(cf.ready).toBe(1); }
        // Nobody else's fleet leaves harbour in a month of peace.
        for (const f of g.fleets) if (f.nat === 'USA' && f !== cf) expect(atSea(f), `${f.name} ${t}`).toBe(false);
      });
      return out;
    };
    const a = run(3);
    // About half the months, and no month known beforehand.
    expect(a.length).toBeGreaterThan(18);
    expect(a.length).toBeLessThan(42);
    expect(run(3)).toEqual(a);
    expect(run(4)).not.toEqual(a);
    for (const stop of [4, 5]) { const n = run(3, stop).length; expect(n).toBeGreaterThan(18); expect(n).toBeLessThan(42); }
    // Over many wars the chance is the ruling's one in two for the month of a strike.
    let at = 0;
    for (let seed = 1; seed <= 80; seed++) {
      const g = war(3, null, seed);
      g.turn = 28;
      navalUpkeep(g, 'USA');
      if (atSea(fleet(g, NAME))) at++;
    }
    expect(at).toBeGreaterThan(28);
    expect(at).toBeLessThan(52);
    // At war the admiral decides: the chance says nothing more.
    const w = war(3, null, 3);
    declareWar(w, 'JAP', 'USA');
    const cf = fleet(w, NAME);
    months(w, 20, () => expect(cf.cruised).toBeUndefined());
  });

  it('a player\'s carriers too: at peace nobody\'s fleet is his to send out, so the record and the chance stand for him as well', () => {
    const g = war(1, 'USA');
    // (His bases are his own to move: the carriers still lie at San Diego.)
    months(g, 28);
    const cf = fleet(g, NAME);
    expect(g.regions[cf.base].name).toBe('San Diego');
    expect([atSea(cf), sea(g, cf)]).toEqual([true, 'midway']);
    const h = war(3, 'USA', 3);
    const out: number[] = [];
    months(h, 40, (t) => { if (atSea(fleet(h, NAME))) out.push(t); });
    expect(out.length).toBeGreaterThan(10);
  });
});
