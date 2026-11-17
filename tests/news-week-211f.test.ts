// The month's news in true order (ruling 211(f), the last item; display only). Each nation's month counts its weeks
// from the first, so a nation that moved after the player dated its news before his own battles of the same month:
// in Brian's January 1941 Canada's guns closing round Cleveland read "week 2", after the relief of Pittsburgh in
// week 4. Once the player's month is over, news is dated no earlier than the last week his own news or battles
// reached. g.week, the battles' weeks and the week a country fell, which the rules read, are left alone.
import { describe, expect, it } from 'vitest';
import { createGame, headline, newsWeek } from '../src/campaign/game';
import { recordFall } from '../src/campaign/liberation';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, current, endHuman, stepAI } from '../src/campaign/turn';
import type { BattleReport, Game } from '../src/campaign/types';

/** A grand campaign played as the United States, brought to the opening of the player's own month. */
function america(seed = 3): Game {
  const g = startOpen(createGame(seed, 'USA'));
  while (current(g) !== 'USA') stepAI(g);
  beginHuman(g);
  return g;
}
const last = (g: Game) => g.headlines[g.headlines.length - 1];

describe('the week news is dated', () => {
  it('keeps the player\'s own weeks, and dates what comes after his month no earlier than the last of them', () => {
    const g = america();
    const T = g.turn;
    g.week = 4; headline(g, 'battle', 'His attack, fought in week 4.', 'USA');
    expect(last(g).week).toBe(4);
    // A later order of his own month that reached only week 2 keeps its week.
    g.week = 2; headline(g, 'battle', 'His march, in week 2.', 'USA');
    expect(last(g).week).toBe(2);
    endHuman(g);
    expect(g.turn).toBe(T);
    // The nations after him: their own count says week 2, the news says week 4.
    g.week = 2; headline(g, 'battle', 'Canadian troops close round Cleveland.', 'USA');
    expect(last(g).week).toBe(4);
    expect(g.week).toBe(2);
    g.week = 4; headline(g, 'battle', 'A blow at the month\'s very end.', 'CAN');
    expect(last(g).week).toBe(4);
    // A fall on the roll keeps the week of its headline.
    g.week = 1;
    recordFall(g, 'LUX', 'GER', 'beaten');
    const p = g.passings[g.passings.length - 1];
    expect(p.week).toBe(newsWeek(g));
    expect(p.week).toBe(4);
    expect(p.weeks).toBe((T - 1) * 4 + 4);
    // The rest of the month moves: whatever the nations after him write in it is dated week 4 or later.
    const from = g.headlines.length;
    while (current(g) !== 'USA') stepAI(g);
    expect(g.headlines.slice(from).filter((h) => h.turn === T && h.week < 4)).toEqual([]);
    // The next month starts afresh: his news and the nations before him keep their own weeks.
    beginHuman(g);
    expect(g.turn).toBe(T + 1);
    g.week = 1; headline(g, 'info', 'A new month, week 1.', 'USA');
    expect(last(g).week).toBe(1);
  });

  it('reads the player\'s battles of the month when nothing of his month was noted (a save made before the mend)', () => {
    const g = america(5);
    delete g.flags.newsweek;
    g.reports.push({ turn: g.turn, week: 3, attNat: 'USA' } as unknown as BattleReport);
    endHuman(g);
    g.week = 1; headline(g, 'battle', 'After his month.', 'CAN');
    expect(last(g).week).toBe(3);
  });

  it('leaves the nations before the player, and a war nobody plays, to their own weeks', () => {
    const g = america(7);
    // Round to the next month's first nation, which moves before him.
    endHuman(g);
    while (g.phase !== 0) stepAI(g);
    expect(g.order.indexOf('USA')).toBeGreaterThan(0);
    g.week = 2; headline(g, 'battle', 'Before his month.', g.order[0]);
    expect(last(g).week).toBe(2);
    const ai = startOpen(createGame(7, null));
    ai.week = 2; headline(ai, 'battle', 'Nobody plays.', 'GER');
    expect(ai.headlines[ai.headlines.length - 1].week).toBe(2);
    expect(ai.flags.newsweek).toBeUndefined();
  });
});
