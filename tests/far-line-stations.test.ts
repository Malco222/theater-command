// A line's stations (the design director's seam c of 14:18Z and his 18:28Z of 22 November 2026 to ruling 234: a
// book-placed row holds its stations, one rule for every line; src/campaign/line.ts `lineKeeps`, ai.ts `release` and
// `lineHome`).
//
// In East Africa Italy's staff opened an offensive of its own in June 1940, and a live front lets go every held row
// on its ground: fourteen of Italy's twenty-two rows were loose that month and marched as the staff liked. Gondar's
// row went to Kurmuk in July 1940 and stood at Keren in March 1941, where it held the 4th and 5th Indian Divisions
// a month past the record. By the book, while a line binds, a held corps of the invader's own on ground the line
// gives him that month keeps its station: the staff's release does not let it go. The book's dated attacks still
// move their forces, and a held corps sent back inside the line is held again where it comes to rest. At the stops
// that keep no book, with the book closed, or with a player on either side, the staff decides as before.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { lineHome, release } from '../src/campaign/ai';
import { addCorps, atWar, createGame, declareWar, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { inLine, lineBinds, lineKeeps, lines } from '../src/campaign/line';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

const HARAR = 'Harar command';
const KF = 'Kassala force (the Eritrea command\'s two colonial brigades and cavalry)';
/** Italy's June 1940 by hand at a stop: its rows come at their places, held, and its war with Britain is open. */
const board = (stop: number, turn = 10, human: string | null = null): Game => {
  const g = startOpen(createGame(3, human));
  setStop(g, stop, true);
  g.turn = turn;
  if (!atWar(g, 'ITA', 'UK')) declareWar(g, 'ITA', 'UK');
  startPhase(g, 'ITA');
  return g;
};
const corps = (g: Game, name: string): Corps => g.corps.find((c) => !c.dead && c.name === name)!;
const at = (g: Game, c: Corps): string => g.regions[c.reg].name;

describe('a line\'s stations: a held corps of the invader\'s on its line\'s ground is not let go by its staff\'s release', () => {
  it('East Africa at the book\'s stops: Harar\'s row keeps its hold when a live front names its ground', () => {
    for (const stop of [1, 2]) {
      const g = board(stop);
      const c = corps(g, HARAR);
      expect([at(g, c), c.hold, c.stay], `stop ${stop}`).toEqual(['Harar', true, undefined]);
      expect(lineKeeps(g, c), `stop ${stop}`).toBe(true);
      release(g, 'ITA', ['Harar']);
      expect(c.hold, `stop ${stop}`).toBe(true);
    }
  });

  it('where no line binds the staff decides as before: the looser stops, the book closed there, a player\'s Britain', () => {
    for (const stop of [3, 4, 5]) {
      const g = board(stop);
      const c = corps(g, HARAR);
      expect(lineKeeps(g, c), `stop ${stop}`).toBe(false);
      release(g, 'ITA', ['Harar']);
      expect(c.hold, `stop ${stop}`).toBe(false);
    }
    const closed = board(1);
    closed.flags['hist:off:eastafrica'] = 9;
    release(closed, 'ITA', ['Harar']);
    expect(corps(closed, HARAR).hold).toBe(false);
    const player = board(1, 10, 'UK');
    expect(lineKeeps(player, corps(player, HARAR))).toBe(false);
    release(player, 'ITA', ['Harar']);
    expect(corps(player, HARAR).hold).toBe(false);
    // (Never a player's own corps.)
    const own = board(1, 10, 'ITA');
    expect(lineKeeps(own, corps(own, HARAR))).toBe(false);
  });

  it('only on ground the line gives him that month: a held corps on a gain whose months are over is let go', () => {
    const g = board(1, 16);
    const K = g.byName['Kassala'];
    K.ctrl = 'ITA';
    const c = addCorps(g, 'ITA', 'A held battalion', 'inf', 1, 'Kassala', { str: 0.1, hold: 1 });
    // (Kassala is Italy's on the line from July 1940 to January 1941, months 11 to 17.)
    expect(lineKeeps(g, c)).toBe(true);
    release(g, 'ITA', ['Kassala']);
    expect(c.hold).toBe(true);
    g.turn = 18;
    expect(lineKeeps(g, c)).toBe(false);
    release(g, 'ITA', ['Kassala']);
    expect(c.hold).toBe(false);
  });

  it('a corps that is not held, and another country\'s, are as they were', () => {
    const g = board(1);
    const free = addCorps(g, 'ITA', 'A free battalion', 'inf', 1, 'Harar', { str: 0.1 });
    release(g, 'ITA', ['Harar']);
    expect(free.hold).toBe(false);
    // (Britain's held rows on the line's ground are no part of it: the line gives the ground to Italy.)
    const uk = addCorps(g, 'UK', 'A British battalion', 'inf', 1, 'Harar', { str: 0.1, hold: 1 });
    expect(lineKeeps(g, uk)).toBe(false);
  });

  it('one rule for every line: on each line the book draws, a held corps of its taker\'s on ground of that month is kept at the book\'s stops and at no other', () => {
    const ids: string[] = [];
    for (const L of lines()) {
      // (A month and a province of the line's own: the first span that is open in a month the line binds.)
      const pick = (g: Game): { name: string; month: number } | null => {
        for (const [name, spans] of Object.entries(L.spans)) for (const s of spans) {
          const month = Math.max(s.from, 2);
          if (g.byName[name] && month <= s.to && month <= L.until) return { name, month };
        }
        return null;
      };
      const g = startOpen(createGame(3, null));
      setStop(g, 1, true);
      const p = pick(g);
      if (!p) continue;
      ids.push(L.id);
      for (const stop of [1, 2, 3, 4, 5]) {
        const h = startOpen(createGame(3, null));
        setStop(h, stop, true);
        h.turn = p.month;
        const r = h.byName[p.name];
        const c = addCorps(h, L.taker, `A held corps (${L.id})`, 'inf', 1, p.name, { hold: 1 });
        const binds = lineBinds(h, L) && inLine(h, L, r, h.turn);
        if (stop >= 3) expect(binds, `${L.id} stop ${stop}`).toBe(false);
        expect(lineKeeps(h, c), `${L.id} at ${p.name} in ${p.month}, stop ${stop}`).toBe(binds);
        release(h, L.taker, [p.name]);
        expect(c.hold, `${L.id} at ${p.name} in ${p.month}, stop ${stop}`).toBe(binds);
      }
    }
    // (China's line, the south's and East Africa's, as the far side lays them.)
    expect(ids.length).toBeGreaterThanOrEqual(3);
    expect(ids).toContain('eastafrica');
  });
});

describe('a held corps sent back inside the line is held again where it comes to rest', () => {
  /** January 1941 by hand: Kassala Italy's, the Kassala force standing on it, held, as its dated attack left it. */
  const gain = (): { g: Game; c: Corps } => {
    // (The rows come in June 1940; the board is then moved to January 1941 by hand.)
    const g = board(1);
    g.turn = 17;
    const K = g.byName['Kassala'];
    for (const u of K.units) u.dead = true;
    K.units = [];
    K.ctrl = 'ITA';
    const c = corps(g, KF);
    const from = g.regions[c.reg];
    from.units.splice(from.units.indexOf(c), 1);
    K.units.push(c);
    c.reg = K.id;
    c.ap = c.apMax;
    g.supplyDirty = true;
    return { g, c };
  };

  it('the Kassala force: in the gain\'s last month it stands, and as the month ends it marches to Keren and is held there', () => {
    const { g, c } = gain();
    lineHome(g, 'ITA', 17);
    expect([at(g, c), c.hold]).toEqual(['Kassala', true]);
    lineHome(g, 'ITA', 18);
    expect([at(g, c), c.hold, g.flags[`lnh:${c.id}`]]).toEqual(['Keren', true, undefined]);
  });

  it('one that cannot march this month stands unheld beyond the line, and is held when it comes in', () => {
    const { g, c } = gain();
    c.ap = 0;
    lineHome(g, 'ITA', 18);
    expect([at(g, c), c.hold, g.flags[`lnh:${c.id}`]]).toEqual(['Kassala', false, 1]);
    g.turn = 18;
    c.ap = c.apMax;
    lineHome(g, 'ITA', 18);
    expect([at(g, c), c.hold, g.flags[`lnh:${c.id}`]]).toEqual(['Keren', true, undefined]);
  });

  it('one that was not held when it set out comes back the staff\'s own', () => {
    const { g, c } = gain();
    c.hold = false;
    lineHome(g, 'ITA', 18);
    expect([at(g, c), c.hold, g.flags[`lnh:${c.id}`]]).toEqual(['Keren', false, undefined]);
  });
});
