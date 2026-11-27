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
import { addCorps, createGame, halfYearLast, men, placed, removeCorps, rowCorps, rowOffers, setWar, startPhase } from '../src/campaign/game';
import { fits, roomLeft } from '../src/campaign/islands';
import { startOpen } from '../src/campaign/staff';
import { MEN_PER_DIV, STACK, type Corps, type Game, type Region } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';
import { offerLines } from '../unreal/TheaterCommand/Rules/offers-look';

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
  /** The 21st Army's 5th Division as the rule reads its room: the corps it forms (game.ts `rowCorps`) and the corps of his it is made from (its row's `from`), which leave as it forms. */
  const fifth = (g: Game): { going: Corps[]; out: Corps[] } => {
    const o = rows[fifthId][6] ?? {};
    return { going: rowCorps(g, 'JAP', rows[fifthId], o), out: g.corps.filter((c) => c.nat === 'JAP' && !c.dead && (o.from as string[]).includes(c.name)) };
  };
  /** A ground's fighting corps of Japan's side, as the five are counted on any ground (engineers apart). */
  const standing = (g: Game, r: Region): number => placed(g, r, 'JAP').filter((u) => u.kind !== 'eng').length;
  /**
   * No room for the 21st Army's 5th Division on Formosa, nor near it, read the way the rule now reads room. Taihoku and
   * Formosa (the island's two grounds, joined by land): full in men by the rule's own reader (`fits` refuses the row's
   * own corps) with five fighting corps standing on each, so that they stay full when a place among the five is asked
   * as well (the design director, 14:13Z, 27 November 2026, (2)). Near them: since round 2 a dated row for a full
   * island of the table goes by sea to the nearest held harbour with room (game.ts roomNear, commit b180e02e4, the
   * design director's D1 of 00:20Z, 27 November 2026), where the base found none by land and the row had no place: at
   * 1084839a0 it is offered and formed at Amoy. So every other harbour of Japan's is filled too, held for its staff,
   * until `fits`, asked as roomNear asks it, refuses the row there. (Whether the row is sent on by sea for the player's
   * offer is asked of the design director, unbuilt: this board holds whatever she rules, since no harbour has room.)
   */
  const fill = (g: Game): void => {
    const { going, out } = fifth(g);
    for (const nm of ['Taihoku', 'Formosa']) {
      const r = g.byName[nm];
      for (let k = 0; standing(g, r) < STACK || fits(g, r, 'JAP', going, { out }); k++) addCorps(g, 'JAP', `Test filler ${nm} ${k}`, 'inf', 1, nm, {});
    }
    for (const r of g.regions) {
      if (!r.port || r.ctrl !== 'JAP' || r === g.byName.Taihoku || r === g.byName.Formosa) continue;
      for (let k = 0; k < 12 && fits(g, r, 'JAP', going, { rows: standing(g, r), out }); k++) addCorps(g, 'JAP', `Test harbour ${r.name} ${k}`, 'inf', 1, r.id, { hold: true });
    }
    // (The no-room case comes from the fill and is read by the rule's own readers: each ground of the island refuses the
    // row's own corps, has less room left than its men, and holds five fighting corps.)
    for (const nm of ['Taihoku', 'Formosa']) {
      const r = g.byName[nm];
      expect([nm, fits(g, r, 'JAP', going, { out }), roomLeft(g, r, 'JAP') * MEN_PER_DIV < men(going[0]), standing(g, r)]).toEqual([nm, false, true, STACK]);
    }
  };

  it("the 21st Army's 5th Division, due in October 1939: with room the player's offer is open through April 1940 and in May it is gone from the rule; with no room anywhere neither seat strikes it, and when room comes the player is offered it and the computer forms it in that month", () => {
    // The player, with room: offered in month 2, open through month 8, lapsed in month 9.
    const p = war('JAP');
    months(p, 1, 8);
    expect(offer(p, FIFTH)).toMatchObject({ id: fifthId, since: 2, last: 8, why: null });
    month(p, 9);
    expect([offer(p, FIFTH), p.flags[`sent:${fifthId}`], row(p, FIFTH), row(p, FIFTH_OF) !== undefined]).toEqual([undefined, 2, undefined, true]);

    // The player and the computer, with no room on Formosa nor at any harbour of Japan's: nothing is offered or formed,
    // and neither seat strikes it, in month 9 or after: it waits until there is room.
    // (Moved by D1, the design director's ruling of 17:53Z, 27 November 2026, 2: "D1 decides. Re-pin far-lapse to the
    // law: with no room anywhere, both seats wait unstruck." Her D1 of 00:20Z, 27 November 2026: "No row is ever struck
    // for room ... Where no held ground anywhere has room, it waits until one does." Before her ruling this asserted
    // the row struck in month 9, `sent` 2, in both seats.)
    // (Each seat on its own board and walked on its own, so that each is shown: the computer's is not left unread
    // behind the player's. Month by month: nothing sent, nothing formed, nothing offered, no word of it to anyone.)
    const waits = (human: string | null): unknown[] => {
      const g = war(human);
      fill(g);
      const news = g.headlines.length, seen: unknown[] = [];
      for (let t = 1; t <= 14; t++) {
        month(g, t);
        seen.push([t, g.flags[`sent:${fifthId}`], row(g, FIFTH), offer(g, FIFTH), row(g, FIFTH_OF) !== undefined]);
      }
      const told = g.headlines.slice(news).map((h) => h.text).filter((t) => t.includes(FIFTH) || /will not form/.test(t));
      // Room on Formosa in month 15: the player is offered it in that month, the computer forms it in that month.
      for (const c of g.corps.filter((x) => /^Test filler Taihoku/.test(x.name) && !x.dead)) removeCorps(g, c);
      month(g, 15);
      const then = human ? [offer(g, FIFTH)?.since, offer(g, FIFTH)?.why, row(g, FIFTH)] : [g.flags[`sent:${fifthId}`], g.regions[row(g, FIFTH)!.reg].name, offer(g, FIFTH)];
      return [human, seen, told, then];
    };
    const unstruck = Array.from({ length: 14 }, (_, k) => [k + 1, undefined, undefined, undefined, true]);
    expect(waits(null)).toEqual([null, unstruck, [], [1, 'Taihoku', undefined]]);
    expect(waits('JAP')).toEqual(['JAP', unstruck, [], [15, null, undefined]]);

    // The computer, with room, forms it in its month, as it always did.
    const c = war(null);
    months(c, 1, 2);
    expect([c.flags[`sent:${fifthId}`], c.regions[row(c, FIFTH)!.reg].name]).toEqual([1, 'Taihoku']);
  });

  it("an offer made late in the half year keeps the months its row waited for room: room on Formosa in month 6, after months 2 to 5 without, gives an offer open through month 12, gone in month 13", () => {
    // (Moved by D1, the design director's ruling of 17:53Z, 27 November 2026, 3: "Months waited for room come off the
    // player's offer (offerLast) exactly as off the computer's half year." Before her ruling this read `last: 8`, the
    // due month and six, with the four months waited for room lost to the offer.)
    const g = war('JAP');
    fill(g);
    months(g, 1, 5);
    for (const c of g.corps.filter((x) => /^Test filler Taihoku/.test(x.name) && !x.dead)) removeCorps(g, c);
    month(g, 6);
    // Its due month is its date, October 1939 (month 2); it waited for room in months 2 to 5, counted by the rule
    // (`roomwait`); its last month is the rule's own function of the two: the due month, six, and the months waited.
    const due = rows[fifthId][0], waited = g.flags[`roomwait:${fifthId}`];
    expect([due, waited]).toEqual([2, 6 - due]);
    const last = halfYearLast(g, fifthId, due);
    expect([last, due + 6 + waited]).toEqual([12, 12]);
    expect(offer(g, FIFTH)).toMatchObject({ since: 6, last, why: null });
    // The page prints it: "Offered in February 1940, open until August 1940."
    expect(offerLines(g, offer(g, FIFTH)!).map((l) => l.text).slice(-1)).toEqual(['Offered in February 1940, open until August 1940.']);
    months(g, 7, last);
    expect(offer(g, FIFTH)).toMatchObject({ since: 6, last });
    expect(offerLines(g, offer(g, FIFTH)!).map((l) => [l.text, l.tone]).slice(-1)).toEqual([['Offered in February 1940. This is its last month.', 'amber']]);
    // Gone in the month after, with the lapse sentence, which reads the same last month.
    const news = g.headlines.length;
    month(g, last + 1);
    expect([offer(g, FIFTH), g.flags[`sent:${fifthId}`], row(g, FIFTH)]).toEqual([undefined, 2, undefined]);
    expect(g.headlines.slice(news).map((h) => h.text).filter((t) => t.includes(FIFTH))).toEqual([`The offer to form ${FIFTH} has lapsed: its time has passed.`]);
  });

  it("the same count for both seats, from one function: each month without room is counted alike before the offer and after it, and the offer's last month is the computer's", () => {
    // (The design director, 17:53Z, 27 November 2026, 3: "The same count for both seats, called from one function."
    // A computer's row that gets room forms in that month, so the two seats cannot be walked to one last month by
    // forming. The same walk instead, each seat on its own board, filled alike: the one function, `halfYearLast`, asked
    // for both in the same months, and the months each counts.)
    const p = war('JAP'), c = war(null);
    fill(p);
    fill(c);
    const due = rows[fifthId][0], rw = `roomwait:${fifthId}`;
    for (let t = 1; t <= 5; t++) {
      month(p, t);
      month(c, t);
      // (No room anywhere: from its due month on, each month is counted, in both seats alike.)
      expect([t, p.flags[rw] ?? 0, halfYearLast(p, fifthId, due)]).toEqual([t, c.flags[rw] ?? 0, halfYearLast(c, fifthId, due)]);
      expect([t, c.flags[rw] ?? 0]).toEqual([t, Math.max(0, t - due + 1)]);
    }
    // Room on Formosa in month 6: the computer forms the row in that month; the player is offered it, open through the
    // last month the computer's own count gives it, the same four months waited.
    const last = halfYearLast(c, fifthId, due);
    for (const g of [p, c]) for (const x of g.corps.filter((y) => /^Test filler Taihoku/.test(y.name) && !y.dead)) removeCorps(g, x);
    month(p, 6);
    month(c, 6);
    expect([c.flags[`sent:${fifthId}`], c.regions[row(c, FIFTH)!.reg].name]).toEqual([1, 'Taihoku']);
    expect(offer(p, FIFTH)).toMatchObject({ since: 6, last, why: null });
    expect(offerLines(p, offer(p, FIFTH)!).map((l) => l.text).slice(-1)).toEqual(['Offered in February 1940, open until August 1940.']);
    // Taihoku full again from month 7 (and every harbour still full): FORM IT shut for room. Each such month is counted
    // for the standing offer as for a computer's row with no room, through the same count, and its last month moves on
    // with it; the month it was offered, which had room, is not.
    const { going, out } = fifth(p), tk = p.byName.Taihoku;
    for (let k = 0; standing(p, tk) < STACK || fits(p, tk, 'JAP', going, { out }); k++) addCorps(p, 'JAP', `Test refill ${k}`, 'inf', 1, 'Taihoku', {});
    for (let t = 7; t <= 10; t++) {
      month(p, t);
      expect([t, p.flags[rw], offer(p, FIFTH)?.why]).toEqual([t, 4 + (t - 6), 'There is no room for another corps at Taihoku, or near it.']);
      expect(offer(p, FIFTH)!.last).toBe(halfYearLast(p, fifthId, due));
    }
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
    g.flags[`asked:${naples}`] = rows[naples][0];
    g.flags[`asked:n:${naples}`] = 0;
    g.turn = rows[naples][0] + 10;
    const o = rowOffers(g).find((x) => x.id === naples)!;
    expect(o.last).toBeNull();
    expect(o.why ?? '').not.toMatch(/has lapsed/);
  });
});
