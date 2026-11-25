// Japan's island garrisons by the record, with the far side's switch on (riders b593 and b614 to ruling 234; the
// historian's table by half-year, historian/island-garrisons-1941-43-b591.md). Each island of the table is a post:
// one row at the table's worth and no town guard beside it, on the Mandates' colonial ground and on Japan's own home
// islands alike, at every stop of the dial. A row whose record changes its size gives a list of month and worth, as
// the air rows do, and takes each size in its month. No island holds more than the record. The states are set by
// hand; no war is played here.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import farWorld from '../data/campaign/far-world.json';
import { BUDGET, farColony, farGuard } from '../src/campaign/economy';
import { createGame, estAt, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { guardOf } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

/** The historian's table: the island, its row, and its worth in December 1941, June and December 1942 and 1943. */
const HALVES = [28, 34, 40, 46, 52];
const TABLE: [string, string, number[]][] = [
  ['Truk', '4th Base Force (Truk)', [0.05, 0.05, 0.1, 0.1, 0.15]],
  ['Palau', '3rd Base Force (Palau)', [0.05, 0.05, 0.05, 0.07, 0.07]],
  ['Saipan', '5th Base Force (Saipan)', [0.07, 0.07, 0.07, 0.06, 0.17]],
  ['Kwajalein', '61st Guard Unit (Kwajalein and Roi)', [0.07, 0.07, 0.1, 0.12, 0.2]],
  ['Eniwetok', 'Engebi lookout station (Eniwetok)', [0.01, 0.01, 0.01, 0.01, 0.02]],
  ['Eastern Marshalls', '6th Base Force (Kwajalein and Jaluit)', [0.1, 0.1, 0.15, 0.2, 0.3]],
  ['Yap', 'Yap naval guard', [0.02, 0.02, 0.02, 0.03, 0.03]],
  ['Ponape', 'Ponape naval guard', [0.04, 0.04, 0.05, 0.05, 0.06]],
  ['Marcus Island', 'Marcus Island Guard Unit', [0.02, 0.02, 0.03, 0.03, 0.05]],
  ['Iwo Jima', 'Iwo Jima naval air station', [0.01, 0.01, 0.02, 0.03, 0.05]],
  ['Chichi Jima', 'Chichi Jima Fortress', [0.33, 0.33, 0.33, 0.33, 0.33]],
];
const HOME = ['Marcus Island', 'Iwo Jima', 'Chichi Jima'];
/** The rows the book or the staff only passes through an island: not its garrison (rider b593). */
const TRANSIT = /Detachment|landing force/i;

const war = (stop = 3): Game => { const g = startOpen(createGame(17, null)); if (stop !== 3) setStop(g, stop, true); return g; };
/** Japan's month opens: the rows by date come, and a dated size is read. */
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'JAP'); };
const row = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead);

describe('estAt: a row\'s establishment in a month', () => {
  it('reads a number as the fullest the row ever gets, and a list as the size by date', () => {
    expect(estAt(undefined, 30)).toBeUndefined();
    expect(estAt(0.25, 1)).toBe(0.25);
    expect(estAt(0.25, 99)).toBe(0.25);
    const list: [number, number][] = [[27, 0.07], [46, 0.06], [52, 0.17]];
    expect([1, 27, 45, 46, 51, 52, 80].map((t) => estAt(list, t))).toEqual([0.07, 0.07, 0.07, 0.06, 0.06, 0.17, 0.17]);
  });
});

describe("Japan's islands are posts held by the record's garrison", () => {
  it('no town guard stands on an island of the table, on colonial ground or on home ground; other home ground keeps a guard', () => {
    const g = war();
    for (const name of [...TABLE.map((x) => x[0]), 'Ulithi']) {
      const r = g.byName[name];
      expect(r, name).toBeTruthy();
      expect(r.owner, name).toBe('JAP');
      expect(BUDGET.farColony.posts, name).toContain(name);
      expect(farColony(r), name).toBe(!HOME.includes(name));
      expect(farGuard(r), name).toBe(0);
      expect(guardOf(g, r), name).toBe(0);
    }
    // (Since the guard table of 21 November 2026 the guard there is the record's figure for the place, far-guards.json:
    // Formosa's seat stands in place of its small town, whose 0.02 is under the rules' measure of too small to count.)
    for (const name of ['Okinawa', 'Tokyo', 'Taihoku']) {
      expect(farGuard(g.byName[name]), name).toBe(1);
      expect(guardOf(g, g.byName[name]), name).toBeGreaterThan(0);
    }
  });

  it('each row comes in November 1941 at the worth of December, at every stop, marked to stay; Ulithi has none', () => {
    for (const stop of [1, 2, 3, 4, 5]) {
      const g = war(stop);
      month(g, 27);
      for (const [place, name, worth] of TABLE) {
        const c = row(g, name)!, r = g.byName[place];
        expect(c, `${name}, stop ${stop}`).toBeTruthy();
        expect(c.reg, name).toBe(r.id);
        expect([c.str, c.full, c.hold, !!c.stay], `${name}, stop ${stop}`).toEqual([worth[0], worth[0], true, true]);
        // Nothing is counted twice: the row is the island's whole garrison, beside what only passes through.
        const others = r.units.filter((u) => u.nat === 'JAP' && !u.dead && u !== c && !TRANSIT.test(u.name));
        expect(others.map((u) => u.name), `${place}, stop ${stop}`).toEqual([]);
      }
      expect(g.byName.Ulithi.units.filter((u) => u.nat === 'JAP' && !TRANSIT.test(u.name)).map((u) => u.name), `Ulithi, stop ${stop}`).toEqual([]);
    }
  });

  it('a row takes the table\'s size in each half-year, and the men of an increase come with it', () => {
    const g = war();
    month(g, 27);
    HALVES.forEach((t, i) => {
      month(g, t);
      for (const [, name, worth] of TABLE) {
        const c = row(g, name)!;
        expect(c.full, `${name}, turn ${t}`).toBe(worth[i]);
        expect(c.str, `${name}, turn ${t}`).toBeCloseTo(worth[i], 9);
      }
    });
    // A garrison worn in the meantime gets the newcomers and no more: the 0.05 that reach Truk in December 1942.
    const h = war();
    month(h, 27);
    const truk = row(h, '4th Base Force (Truk)')!;
    truk.str = 0.02;
    month(h, 40);
    expect(truk.full).toBe(0.1);
    expect(truk.str).toBeGreaterThanOrEqual(0.07 - 1e-9);
    expect(truk.str).toBeLessThanOrEqual(0.1);
  });

  it('a garrison that has left its island, or has fallen, is not followed', () => {
    const g = war();
    month(g, 27);
    const truk = row(g, '4th Base Force (Truk)')!, saipan = row(g, '5th Base Force (Saipan)')!;
    // (Moved by hand to Tokyo, as a player might ship it home.)
    const from = g.regions[truk.reg], to = g.byName.Tokyo;
    from.units.splice(from.units.indexOf(truk), 1); to.units.push(truk); truk.reg = to.id;
    saipan.dead = true;
    month(g, 40);
    month(g, 52);
    expect([truk.full, truk.str]).toEqual([0.05, 0.05]);
    expect(saipan.full).toBe(0.07);
  });

  it('the rows as laid never give an island more than the record', () => {
    const rows = (farWorld as any)['setup-1939'].expeditions as any[];
    for (const [place, name, worth] of TABLE) {
      const mine = rows.filter((x) => x[1] === 'JAP' && x[2] === name);
      expect(mine.length, name).toBe(1);
      const [turn, , , kind, divs, at, o] = mine[0];
      expect([turn, kind, divs, at], name).toEqual([27, 'inf', 1, place]);
      expect(o.stops, name).toBeUndefined();
      expect([o.hold, o.stay], name).toEqual([1, 1]);
      expect(o.str, name).toBe(worth[0]);
      HALVES.forEach((t, i) => expect(estAt(o.est, t), `${name}, turn ${t}`).toBe(worth[i]));
      if (Array.isArray(o.est)) {
        expect(o.est[0][0], name).toBe(27);
        for (let i = 1; i < o.est.length; i++) expect(o.est[i][0], name).toBeGreaterThan(o.est[i - 1][0]);
      }
    }
    // No other row of Japan's is laid on these islands to stay.
    const places = new Set([...TABLE.map((x) => x[0]), 'Ulithi']);
    const names = new Set(TABLE.map((x) => x[1]));
    expect(rows.filter((x) => x[1] === 'JAP' && places.has(x[5]) && x[6]?.stay && !names.has(x[2])).map((x) => x[2])).toEqual([]);
  });
});
