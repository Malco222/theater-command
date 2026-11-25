// One lapse rule for both seats, with the far side's switch on (the design director's ruling of 13:39Z, 25 November
// 2026, on the batch 1 readings, point 5 of the dated rows, design-director/pacific-japan-1125.md):
//
// "A war that is over: one rule for both sides. A row whose war is over lapses at once, the player's offer and the
// computer's row alike. The half-year wait is only for a war not yet begun, and holds for both: the player's offer
// stays in the list for the same half year. If the two must differ, name the cause."
//
// As built a player's offer whose war was over lapsed at once and for good, while a computer's row in the same case
// waited for its war to come again (and was then struck for its half year, or came); and a player's offer stood with
// no half year against it, where the same row of a computer's not formed by the end of its half year is struck.
// The same row, the same months, each seat. The old chart: tests/far-dated-rows-off and the identity check.
import { vi } from 'vitest';
// (The offers rule has its own switch, off until its screen passes the Mac: stage.ts OFFERS_STAGE. On for this file.)
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; (globalThis as any).TC_OFFERS = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, createGame, placed, removeCorps, rowOffers, setWar, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { STACK, type Corps, type Game } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';

const rows = (setup as any).expeditions as any[][];
const war = (human: string | null): Game => startOpen(createGame(5, human));
const row = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead);
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'JAP'); };
const months = (g: Game, from: number, to: number): void => { for (let t = from; t <= to; t++) month(g, t); };
const offer = (g: Game, name: string) => rowOffers(g).find((o) => o.name === name);
const peace = (g: Game, a: string, b: string): void => {
  g.nations[a].atWar = g.nations[a].atWar.filter((x) => x !== b);
  g.nations[b].atWar = g.nations[b].atWar.filter((x) => x !== a);
};
const lapsed = (g: Game, from: number): string[] => g.headlines.slice(from).map((h) => h.text).filter((t) => /has lapsed/.test(t));

const BURMA = '15th Army (33rd and 55th Divisions)', BURMA_OF = '11th Army (33rd Division)';
const FIFTH = '21st Army (5th Division)', FIFTH_OF = '12th Army (5th Division)';
const burmaId = rows.findIndex((r) => r[1] === 'JAP' && r[2] === BURMA && r[6].stops.includes(3));
const fifthId = rows.findIndex((r) => r[1] === 'JAP' && r[2] === FIFTH);

describe('a row whose war is over lapses at once, for the player and the computer alike', () => {
  /** Japan goes to war with Britain at the end of September 1941 (month 30) and makes peace a month later, before the 15th Army for Burma has come. */
  const shortWar = (human: string | null): Game => {
    const g = war(human);
    months(g, 28, 30);
    setWar(g, 'JAP', 'UK');
    month(g, 31);
    // (The row has seen its war: its month is counted, and it comes in month 32.)
    expect([g.flags[`due:${burmaId}`], g.flags[`sent:${burmaId}`], row(g, BURMA)]).toEqual([32, undefined, undefined]);
    peace(g, 'JAP', 'UK');
    return g;
  };

  it('the 15th Army for Burma: struck in the month after the peace, in both seats, and it never comes when the war is taken up again', () => {
    for (const human of ['JAP', null]) {
      const g = shortWar(human);
      month(g, 32);
      // (As built both waited here for the war to come again: the player's row would then have been offered and the
      // computer's formed.)
      expect([human, g.flags[`sent:${burmaId}`], row(g, BURMA)]).toEqual([human, 2, undefined]);
      setWar(g, 'JAP', 'UK');
      months(g, 33, 36);
      expect([human, row(g, BURMA), offer(g, BURMA), row(g, BURMA_OF) !== undefined]).toEqual([human, undefined, undefined, true]);
    }
  });

  it("the player's offer of it, made, lapses in the month after the peace with the words he is told", () => {
    const g = war('JAP');
    months(g, 28, 30);
    setWar(g, 'JAP', 'UK');
    months(g, 31, 32);
    expect(offer(g, BURMA)).toMatchObject({ id: burmaId, since: 32, last: 38 });
    peace(g, 'JAP', 'UK');
    const news = g.headlines.length;
    month(g, 33);
    expect([offer(g, BURMA), g.flags[`sent:${burmaId}`]]).toEqual([undefined, 2]);
    expect(lapsed(g, news)).toEqual([`The offer to form ${BURMA} has lapsed: the war it was raised for is over.`]);
  });

  it('a war not yet begun is waited for, as before, by both: the row comes when it begins', () => {
    for (const human of [null, 'JAP']) {
      const g = war(human);
      months(g, 28, 34);
      expect([human, g.flags[`sent:${burmaId}`], g.flags[`due:${burmaId}`]]).toEqual([human, undefined, undefined]);
      setWar(g, 'JAP', 'UK');
      months(g, 35, 36);
      // (Both have their half year from the war's own month. The player's is offered at once; the computer's waits for
      // its posting in Thailand, which a country at peace with Japan holds, as it always did: B7's point 1 reading.)
      expect([human, g.flags[`due:${burmaId}`], g.flags[`sent:${burmaId}`]]).toEqual([human, 36, undefined]);
      if (human) expect(offer(g, BURMA)).toMatchObject({ since: 36, last: 42 });
    }
  });
});

describe("the player's offer stays in the list for the same half year as the computer's row", () => {
  /** Formosa full: five corps of Japan's at Taihoku and at Formosa, so that the 21st Army's 5th Division has nowhere to form on the island. */
  const fill = (g: Game): void => {
    for (const nm of ['Taihoku', 'Formosa']) {
      const r = g.byName[nm];
      for (let k = 0; placed(g, r, 'JAP').filter((u) => u.kind !== 'eng').length < STACK; k++) addCorps(g, 'JAP', `Test filler ${nm} ${k}`, 'inf', 1, nm, {});
    }
  };

  it('the 21st Army\'s 5th Division, due in October 1939: its last month is April 1940 in every seat, and in May it is gone from the rule', () => {
    // The player, with room: offered in month 2, open through month 8, lapsed in month 9.
    const p = war('JAP');
    months(p, 1, 8);
    expect(offer(p, FIFTH)).toMatchObject({ id: fifthId, since: 2, last: 8, why: null });
    month(p, 9);
    expect([offer(p, FIFTH), p.flags[`sent:${fifthId}`], row(p, FIFTH), row(p, FIFTH_OF) !== undefined]).toEqual([undefined, 2, undefined, true]);

    // The player and the computer, with no room on Formosa: nothing is offered or formed, and the row is struck in
    // month 9, its half year out.
    for (const human of ['JAP', null]) {
      const g = war(human);
      fill(g);
      months(g, 1, 8);
      expect([human, g.flags[`sent:${fifthId}`], row(g, FIFTH), offer(g, FIFTH)]).toEqual([human, undefined, undefined, undefined]);
      month(g, 9);
      expect([human, g.flags[`sent:${fifthId}`], row(g, FIFTH)]).toEqual([human, 2, undefined]);
    }

    // The computer, with room, forms it in its month, as it always did.
    const c = war(null);
    months(c, 1, 2);
    expect([c.flags[`sent:${fifthId}`], c.regions[row(c, FIFTH)!.reg].name]).toEqual([1, 'Taihoku']);
  });

  it("an offer made late in the half year has only what is left of it: room on Formosa in month 6 gives an offer open through month 8", () => {
    const g = war('JAP');
    fill(g);
    months(g, 1, 5);
    for (const c of g.corps.filter((x) => /^Test filler Taihoku/.test(x.name) && !x.dead)) removeCorps(g, c);
    month(g, 6);
    expect(offer(g, FIFTH)).toMatchObject({ since: 6, last: 8 });
  });
});

// (The skeptic's review of 25 November 2026: the half year reached offers of rows on the old chart too, beyond ruling
// B7's reach, which the ruling is built for. An offer of a row posted on the old chart keeps what it had: its row's
// own `until` where it has one, and otherwise it stands until he answers.)
describe('the half year reaches no offer of a row posted on the old chart', () => {
  it("a player's America offered the Fifth Army's II Corps at Naples: it has no last month and stands ten months on", () => {
    const g = startOpen(createGame(5, 'USA'));
    const naples = rows.findIndex((r) => r[1] === 'USA' && r[2] === 'Fifth Army: II Corps (34th and 36th Infantry Divisions)');
    expect(rows[naples][6].until).toBeUndefined();
    setWar(g, 'USA', 'GER');
    g.flags[`offer:${naples}`] = rows[naples][0];
    g.flags[`offer:n:${naples}`] = 0;
    g.turn = rows[naples][0] + 10;
    const o = rowOffers(g).find((x) => x.id === naples)!;
    expect(o.last).toBeNull();
    expect(o.why ?? '').not.toMatch(/has lapsed/);
  });
});
