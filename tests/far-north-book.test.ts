// The Soviet Union's war with Japan, where two rules name it (the design director's ruling of 07:18Z, 22 November
// 2026, to rulings 111, 115 and 197). The finish rule (finish.ts) gives notice the month Germany falls and brings the
// war three months after, and asked no stop of the history dial; the Pacific's own tie (pacific.json 'north') holds
// the war, at the stops that keep the book, to the book's month, August 1945. The finish rule spoke first, so the
// tie's month was never heard: read on twelve wars by the book, the war came in months 58 to 68 in eleven. Ruled:
// at stops 1 and 2 the notice is given the month Germany falls, as before, and the war comes at the later of three
// months after the fall and the book's month; at stops 3 to 5 the finish rule's months alone, as before. No new
// number, and behind the far switch (with it off: tests/pacific-war-off.test.ts and tests/finish.test.ts).
// The states are set by hand on the first day's chart and the month is turned as the game turns it: no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { turnToDate } from '../src/core/ticket';
import lib from '../data/campaign/liberation.json';
import pacific from '../data/campaign/pacific.json';
import { closeDiplomacy, monthlyDiplomacy } from '../src/campaign/diplomacy';
import { finishMonthEnd, finishMonthOpen, warNotices } from '../src/campaign/finish';
import type { WarNotice } from '../src/campaign/finish';
import { atWar, capitulate, createGame, setWar } from '../src/campaign/game';
import { setStop, stopDef } from '../src/campaign/history';
import { owedNotBefore } from '../src/campaign/pacific';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const BOOK = pacific.north.book, NOTICE = lib.finish.notice;

/**
 * A grand campaign on the whole world at a stop of the dial, moved by hand to a month of the war: the Soviet Union an
 * ally at war with Germany, and Japan at war with America and Britain.
 */
const war = (stop: number, turn: number, human: string | null = null): Game => {
  const g = startOpen(createGame(3, human));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = turn;
  g.nations.SOV.align = 'allies';
  setWar(g, 'SOV', 'GER');
  setWar(g, 'UK', 'GER');
  setWar(g, 'JAP', 'USA');
  setWar(g, 'JAP', 'UK');
  return g;
};
/** The month turns by hand, in the game's own order: its close, the notices, the calendar, the wars now due. */
const turnMonth = (g: Game): void => { closeDiplomacy(g); finishMonthEnd(g); g.turn++; monthlyDiplomacy(g); finishMonthOpen(g); };
/** Germany is beaten this month, and the month closes on it. */
const germanyFalls = (g: Game): void => { capitulate(g, 'GER', 'SOV'); finishMonthEnd(g); };
const notice = (g: Game): WarNotice | undefined => warNotices(g).find((n) => n.who === 'SOV' && n.on === 'JAP');
const said = (g: Game, turn: number, part: string): boolean => g.headlines.some((h) => h.turn === turn && h.text.includes(part));

describe('the two months the record held', () => {
  it('are the data\'s own: three months of notice, and August 1945 by the book', () => {
    expect(NOTICE).toBe(3);
    expect(pacific.north.months).toBe(NOTICE);
    expect(turnToDate(BOOK).label).toBe('August 1945');
    expect([1, 2, 3, 4, 5].map((n) => !!stopDef(war(n, 57)).book)).toEqual([true, true, false, false, false]);
  });
});

for (const stop of [1, 2]) {
  describe(`stop ${stop}, which keeps the book`, () => {
    it('gives notice the month Germany falls, and names the book\'s month for the war', () => {
      const g = war(stop, 57);
      germanyFalls(g);
      expect(notice(g)).toEqual({ who: 'SOV', on: 'JAP', given: 57, due: BOOK });
      expect(said(g, 57, 'notice that it will be at war with it in August 1945')).toBe(true);
    });

    it('keeps the peace until the book\'s month, and is at war in it, Mongolia and Manchukuo with it', () => {
      const g = war(stop, 57);
      germanyFalls(g);
      g.turn++;
      monthlyDiplomacy(g);
      finishMonthOpen(g);
      expect(atWar(g, 'SOV', 'JAP'), 'month 58').toBe(false);
      while (g.turn < BOOK - 1) { turnMonth(g); expect(atWar(g, 'SOV', 'JAP'), `month ${g.turn}`).toBe(false); }
      expect(notice(g)?.due).toBe(BOOK);
      turnMonth(g);
      expect(g.turn).toBe(BOOK);
      expect(atWar(g, 'SOV', 'JAP')).toBe(true);
      expect(g.flags['war:SOV:JAP']).toBe(BOOK);
      expect(atWar(g, 'MGL', 'JAP')).toBe(true);
      expect(atWar(g, 'MAN', 'SOV')).toBe(true);
      expect(notice(g)).toBeUndefined();
    });

    it('waits its three months where Germany falls late: the later of the two', () => {
      const g = war(stop, BOOK - 1);
      germanyFalls(g);
      expect(notice(g)).toEqual({ who: 'SOV', on: 'JAP', given: BOOK - 1, due: BOOK - 1 + NOTICE });
      g.turn++;
      monthlyDiplomacy(g);
      finishMonthOpen(g);
      expect(g.turn).toBe(BOOK);
      expect(atWar(g, 'SOV', 'JAP')).toBe(false);
      turnMonth(g);
      expect(atWar(g, 'SOV', 'JAP')).toBe(false);
      turnMonth(g);
      expect(g.turn).toBe(BOOK - 1 + NOTICE);
      expect(atWar(g, 'SOV', 'JAP')).toBe(true);
    });

    it('holds no other war to a month: the Soviet Union\'s with Japan alone', () => {
      const g = war(stop, 57);
      expect(owedNotBefore(g, 'SOV', 'JAP')).toBe(BOOK);
      for (const [who, on] of [['JAP', 'SOV'], ['SOV', 'ITA'], ['SOV', 'MAN'], ['USA', 'JAP'], ['FRA', 'JAP'], ['UK', 'ITA']]) expect(owedNotBefore(g, who, on), `${who} on ${on}`).toBe(0);
      // Nor in the last act, where every power fights for itself and the ties are not read.
      g.flags.dom = 1;
      expect(owedNotBefore(g, 'SOV', 'JAP')).toBe(0);
    });
  });
}

for (const stop of [3, 4, 5]) {
  describe(`stop ${stop}, which does not`, () => {
    it('goes by the finish rule alone: notice the month Germany falls, war three months after', () => {
      const g = war(stop, 57);
      expect(owedNotBefore(g, 'SOV', 'JAP')).toBe(0);
      germanyFalls(g);
      expect(notice(g)).toEqual({ who: 'SOV', on: 'JAP', given: 57, due: 57 + NOTICE });
      expect(said(g, 57, `notice that it will be at war with it in ${turnToDate(57 + NOTICE).label}`)).toBe(true);
      g.turn++;
      monthlyDiplomacy(g);
      finishMonthOpen(g);
      expect(atWar(g, 'SOV', 'JAP'), 'month 58').toBe(false);
      turnMonth(g);
      expect(atWar(g, 'SOV', 'JAP'), 'month 59').toBe(false);
      turnMonth(g);
      expect(g.turn).toBe(57 + NOTICE);
      expect(atWar(g, 'SOV', 'JAP')).toBe(true);
      expect(g.flags['war:SOV:JAP']).toBe(57 + NOTICE);
      expect(atWar(g, 'MGL', 'JAP')).toBe(true);
      expect(atWar(g, 'MAN', 'SOV')).toBe(true);
    });
  });
}

describe('a player\'s Japan', () => {
  it('is warned of the month the war will come, by the book and off it', () => {
    for (const [stop, due] of [[1, BOOK], [3, 57 + NOTICE]]) {
      const g = war(stop, 57, 'JAP');
      germanyFalls(g);
      // (France, at war with Germany on this chart, gives its own notice by the finish rule: its three months, at every stop.)
      const card = g.headlines.find((h) => h.warn === 'War is coming' && h.nat === 'JAP' && h.about === 'SOV');
      const french = g.headlines.find((h) => h.warn === 'War is coming' && h.nat === 'JAP' && h.about === 'FRA');
      expect(french?.text, `stop ${stop}`).toContain(`it will declare war on you in ${turnToDate(57 + NOTICE).label}`);
      expect(card?.text, `stop ${stop}`).toContain(`it will declare war on you in ${turnToDate(due).label}`);
      expect(notice(g)?.due).toBe(due);
    }
  });
});
