// The Pacific Fleet's carriers in the month of the strike (ruling 234(d)).
//
// The carriers are a row of their own, the Pacific Carrier Fleet. At the stops of the history dial that keep the book
// the record's own move has them at sea in the month Japan strikes south, though America is at peace, and back at
// their base the month after. Where no book is kept, a fleet with a 'cruise' on its row spends a month of peace at sea
// on an even chance by the war's seed and the month, so that a strike opening a war finds it in harbour or does not,
// and nobody can read which from where a data row sits. Peace, for that month, is peace with Japan (rider b593):
// America is at war with Germany from the close of December 1941 whenever Japan strikes, and a strike that comes a
// month or half a year later must still find the carriers out or in. With the far side's switch off none of it exists
// (tests/pacific-war-off.test.ts).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import farWorld from '../data/campaign/far-world.json';
import { shipsInPort } from '../src/campaign/air';
import { createGame, declareWar } from '../src/campaign/game';
import { HISTORY, bookDate, setStop } from '../src/campaign/history';
import { setDesk } from '../src/campaign/depth';
import { NAVY, atSea, navalAI, navalUpkeep } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { stepAI } from '../src/campaign/turn';
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
    // (The third word is whose blow the month waits for: rider b593, below.)
    expect(cf.cruise).toEqual(['midway', 0.5, 'JAP']);
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

  it('America\'s own month played whole leaves them at sea: its admiral, who calls every fleet home in peace, does not call these (found in the measure of ruling 234: three carriers were sunk at anchor in every war)', () => {
    // The admiral's orders alone, after the month's upkeep.
    const g = war(1);
    months(g, 28);
    const cf = fleet(g, NAME), pf = fleet(g, 'Pacific Fleet');
    navalAI(g, 'USA');
    expect([atSea(cf), sea(g, cf), cf.stance, cf.cruised]).toEqual([true, 'midway', 'patrol', 28]);
    expect(atSea(pf)).toBe(false);
    // And the whole of America's month as the computer plays it: upkeep, staff, admiral, the month's close.
    const w = war(1);
    months(w, 27);
    w.turn = 28;
    w.phase = w.order.indexOf('USA');
    expect(stepAI(w)).toBe('USA');
    const wc = fleet(w, NAME), wp = fleet(w, 'Pacific Fleet');
    expect(w.nations.USA.atWar.length).toBe(0);
    expect([atSea(wc), sea(w, wc), wc.cruised, wc.ships.cv]).toEqual([true, 'midway', 28, 3]);
    declareWar(w, 'JAP', 'USA');
    const there = shipsInPort(w, wp.base, 'JAP').map((f) => f.name);
    expect(there).toContain('Pacific Fleet');
    expect(there).not.toContain(NAME);
    // A month of peace with no cruise: the admiral still calls every fleet home, as before.
    const q = war(1);
    months(q, 29);
    const qc = fleet(q, NAME);
    qc.stance = 'patrol';
    navalAI(q, 'USA');
    expect(atSea(qc)).toBe(false);
    // Where no book is kept, the chance's month at sea stands through the admiral's orders too.
    for (let seed = 1, seen = 0; seed <= 12 && seen < 3; seed++) {
      const c = war(3, null, seed);
      c.turn = 28;
      navalUpkeep(c, 'USA');
      const at = atSea(fleet(c, NAME));
      navalAI(c, 'USA');
      expect(atSea(fleet(c, NAME)), `seed ${seed}`).toBe(at);
      if (at) seen++;
    }
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

describe('peace is peace with Japan, for the fleet whose row says so (rider b593 to ruling 234)', () => {
  /** America at war with Germany and Italy, as it is from the close of December 1941 whenever Japan strikes. */
  const european = (g: Game): void => { declareWar(g, 'GER', 'USA'); declareWar(g, 'ITA', 'USA'); };
  /** A war with no book, America in the European war from the close of month 28 and at peace with Japan. */
  const late = (seed: number, human: string | null = null, stop = 3): Game => {
    const g = war(stop, human, seed);
    months(g, 28);
    european(g);
    expect(g.nations.USA.atWar).toEqual(expect.arrayContaining(['GER', 'ITA']));
    expect(g.nations.USA.atWar).not.toContain('JAP');
    return g;
  };

  it('where no book is kept the carriers are at sea on the even chance in a month at war with Germany and at peace with Japan', () => {
    const g = late(3);
    const cf = fleet(g, NAME), out: number[] = [];
    const others = g.fleets.filter((f) => f.nat === 'USA' && f !== cf).map((f) => [f.name, f.zone, f.stance]);
    months(g, 60, (t) => {
      if (atSea(cf)) { out.push(t); expect([sea(g, cf), cf.stance, cf.cruised]).toEqual(['midway', 'patrol', t]); }
      else expect(cf.cruised).toBeUndefined();
      // A cruise, not a patrol: it wears nothing.
      expect(cf.ready).toBe(1);
    });
    // About half of the 32 months, and the months a country at peace with everybody draws: the seed's and the month's.
    expect(out.length).toBeGreaterThan(8);
    expect(out.length).toBeLessThan(24);
    const p = war(3, null, 3), pc = fleet(p, NAME), calm: number[] = [];
    months(p, 60, (t) => { if (t > 28 && atSea(pc)) calm.push(t); });
    expect(out).toEqual(calm);
    // Nobody else's month: no other fleet of America's is moved by it (those formed since lie where they formed).
    expect(g.fleets.filter((f) => f.nat === 'USA' && f !== cf).map((f) => [f.name, f.zone, f.stance]).slice(0, others.length)).toEqual(others);
    for (const f of g.fleets) if (f.nat === 'USA' && f !== cf) expect([f.name, atSea(f), f.cruised]).toEqual([f.name, false, undefined]);
    // Over many wars the chance is still the ruling's one in two for the month of a strike that comes late.
    let at = 0;
    for (let seed = 1; seed <= 80; seed++) {
      const w = late(seed);
      w.turn = 31;
      navalUpkeep(w, 'USA');
      if (atSea(fleet(w, NAME))) at++;
    }
    expect(at).toBeGreaterThan(28);
    expect(at).toBeLessThan(52);
  });

  it('the admiral leaves a cruising fleet alone that month, the blow finds it out or in, and from the month after it is his', () => {
    let out = 0, inn = 0;
    for (let seed = 1; seed <= 24 && (out < 3 || inn < 3); seed++) {
      const g = late(seed);
      g.turn = 31;
      navalUpkeep(g, 'USA');
      const cf = fleet(g, NAME), pf = fleet(g, 'Pacific Fleet'), at = atSea(cf);
      navalAI(g, 'USA');
      if (at) { expect([atSea(cf), sea(g, cf), cf.stance, cf.cruised], `seed ${seed}`).toEqual([true, 'midway', 'patrol', 31]); out++; }
      else { expect([atSea(cf), cf.cruised], `seed ${seed}`).toEqual([false, undefined]); inn++; }
      // Japan moves last in a month: its blow finds the battle line in harbour, and the carriers there or not.
      declareWar(g, 'JAP', 'USA');
      const there = shipsInPort(g, pf.base, 'JAP').map((f) => f.name);
      expect(there, `seed ${seed}`).toContain('Pacific Fleet');
      expect(there.includes(NAME), `seed ${seed}`).toBe(!at);
      // At war with Japan the chance says nothing more: back at its base the month after, and its admiral's.
      months(g, 40, (t) => expect(cf.cruised, `seed ${seed} month ${t}`).toBeUndefined());
    }
    expect(Math.min(out, inn)).toBeGreaterThanOrEqual(3);
  });

  it("America's own month played whole, at war with Germany, leaves the cruising carriers at sea", () => {
    let seed = 1;
    for (; seed <= 24; seed++) { const d = late(seed); d.turn = 29; navalUpkeep(d, 'USA'); if (atSea(fleet(d, NAME))) break; }
    expect(seed).toBeLessThanOrEqual(24);
    const w = late(seed);
    w.turn = 29;
    w.phase = w.order.indexOf('USA');
    expect(stepAI(w)).toBe('USA');
    const wc = fleet(w, NAME);
    expect(w.nations.USA.atWar).toContain('GER');
    expect([atSea(wc), sea(w, wc), wc.stance, wc.cruised]).toEqual([true, 'midway', 'patrol', 29]);
    declareWar(w, 'JAP', 'USA');
    expect(shipsInPort(w, fleet(w, 'Pacific Fleet').base, 'JAP').map((f) => f.name)).not.toContain(NAME);
  });

  it("by the book a strike drawn a month late (stop 2) still finds them at sea, though America is in Germany's war by then", () => {
    let seen = 0;
    for (let seed = 1; seed <= 40 && seen < 2; seed++) {
      const g = war(2, null, seed);
      const strike = bookDate(g, 'JAP', 'south')!;
      if (strike !== HISTORY.dates.JAP.south + 1) continue;
      seen++;
      const cf = fleet(g, NAME);
      months(g, 28, (t) => expect(atSea(cf), `seed ${seed} month ${t}`).toBe(false));
      european(g);
      g.turn = strike;
      navalUpkeep(g, 'USA');
      expect([atSea(cf), sea(g, cf), cf.stance, cf.cruised], `seed ${seed}`).toEqual([true, 'midway', 'patrol', strike]);
      navalAI(g, 'USA');
      expect(atSea(cf), `seed ${seed}`).toBe(true);
      // And no other month: the book keeps one month at sea, the strike's.
      months(g, 36, (t) => expect(atSea(cf), `seed ${seed} month ${t}`).toBe(false));
    }
    expect(seen).toBe(2);
  });

  it("a player's fleets are his own to send out once America is at war with anybody: nothing is done with them, unless his admirals run the navy", () => {
    const h = late(3, 'USA');
    const hc = fleet(h, NAME);
    months(h, 60, (t) => { expect(atSea(hc), `month ${t}`).toBe(false); expect(hc.cruised).toBeUndefined(); });
    // (The same war with the computer's America has them out about half those months: the first test above.)
    const s = late(3, 'USA');
    expect(setDesk(s, 'navy', true)).toBe(true);
    const sc = fleet(s, NAME), out: number[] = [];
    months(s, 60, (t) => { if (atSea(sc)) out.push(t); });
    expect(out.length).toBeGreaterThan(8);
  });
});
