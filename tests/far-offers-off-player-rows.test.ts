// A player's Japan with the offers switch OFF (as shipped, stage.json `offers: false`): his dated rows that take a
// named corps whole come by their dates and take his corps, as a computer's do, and no hold binds his own corps.
// (The three tests of 03c39f400 set `g.flags.offers = 1` and read the offer; this file reads the other side of the
// switch for the same rows: far-on's b423 player case and far-seventh-division's player case.)
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { createGame, offersOn, rowOffers, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

const row = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead);
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'JAP'); };

describe("a player's Japan with the offers switch off", () => {
  it('the 21st Army row of month 2 forms by itself at Taihoku and takes his 12th Army (5th Division); no hold binds it', () => {
    for (const stop of [3, 1]) {
      const g = startOpen(createGame(3, 'JAP'));
      if (stop !== 3) setStop(g, stop, true);
      expect(offersOn(g)).toBe(false);
      expect(row(g, '12th Army (5th Division)'), `stop ${stop}`).toBeDefined();
      month(g, 2);
      expect(rowOffers(g), `stop ${stop}`).toEqual([]);
      const c = row(g, '21st Army (5th Division)')!;
      expect(c, `stop ${stop}`).toBeDefined();
      expect(g.regions[c.reg].name, `stop ${stop}`).toBe('Taihoku');
      expect(row(g, '12th Army (5th Division)'), `stop ${stop}`).toBeUndefined();
      expect([c.hold, !!c.stay], `stop ${stop}`).toEqual([false, false]);
    }
  });

  it("the 7th Division's row of month 12 forms by itself at Sapporo and takes his 6th Army (7th Division)", () => {
    const g = startOpen(createGame(5, 'JAP'));
    expect(offersOn(g)).toBe(false);
    month(g, 11);
    expect(row(g, '6th Army (7th Division)')).toBeDefined();
    month(g, 12);
    expect(rowOffers(g)).toEqual([]);
    expect(g.regions[row(g, '7th Division (Asahikawa)')!.reg].name).toBe('Sapporo');
    expect(row(g, '6th Army (7th Division)')).toBeUndefined();
  });
});
