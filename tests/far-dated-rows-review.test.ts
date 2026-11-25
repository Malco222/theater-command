// Japan's dated rows on the whole world, mended after the review of 25 November 2026 (the design director's rulings B7
// and B8 of that day, design-director/pacific-japan-1125.md; the piece itself: tests/far-dated-rows-b7.test.ts).
//
// What the reviewer found, and what is pinned here:
//   the end of a loan   A regiment lent to a campaign left the player's map when its time was up, as a computer's
//                       does: Japan's 1st and 6th Tank Regiments the month Singapore was his. B8: "rows that take a
//                       corps from the player: an offer, never a silent removal". Where a division's row awaits the
//                       regiment it now stays until he lets it go, in the same list. Every other loan ends as it did
//                       (kept, its men would be counted twice): that is put to the design director, and pinned here
//                       as it stands.
//   a war that is over  An offer whose war had ended stood on the month card for good and could never be taken. It
//                       lapses, and he is told.
//   the lapse's words   "the corps ... are no longer on the map" for one corps, "one of the corps" for two of three.
//   a sister row        A detachment whose men another row releases by cutting a corps was struck after its half year
//                       while the player had not yet answered the offer to make the cut.
//   a chain             (Found in the mending.) A row made from the formation another row would make came whole and
//                       took nobody while that other row stood as an open offer, or after he had declined it: the
//                       18th Division at Rangoon beside the corps it was to be made of. It waits on his answer;
//                       declined, it never comes while he has the corps he kept.
//   who is asked        Every player on the whole world, for the rows the grand campaign sends: Britain's own are
//                       beyond the old chart. The old chart's rows made from corps on the board all stand in the
//                       book's own theatres (Italy, the West), which the grand campaign sends to nobody.
//   the 3D game's reply A reason is a sentence of its own: it follows a full stop, not a colon.
// The states are set by hand, as tests/far-dated-rows-b7.test.ts sets them.
import { vi } from 'vitest';
// (The offers rule has its own switch, off until its screen passes the Mac: stage.ts OFFERS_STAGE. On for this file.)
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; (globalThis as any).TC_OFFERS = 1; });

import { describe, expect, it } from 'vitest';
import { createGame, declineOffer, men, relocate, removeCorps, rowOffers, setWar, startPhase, takeOffer } from '../src/campaign/game';
import { homeland } from '../src/campaign/middleeast';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';
import { beyondEurope } from '../src/campaign/world';
import setup from '../data/campaign/setup-1939.json';
import theatres from '../data/campaign/theatres.json';
import '../unreal/TheaterCommand/Rules/campaign-bridge';
import { offerCalls } from '../unreal/TheaterCommand/Rules/offers-look';

const rows = (setup as any).expeditions as any[][];
const index = (name: string, nat = 'JAP'): number => rows.findIndex((r) => r[1] === nat && r[2] === name);
const war = (human: string | null = 'JAP'): Game => startOpen(createGame(5, human));
const row = (g: Game, name: string, nat = 'JAP'): Corps | undefined => g.corps.find((c) => c.nat === nat && c.name === name && !c.dead);
const month = (g: Game, t: number, nat = 'JAP'): void => { g.turn = t; startPhase(g, nat); };
const months = (g: Game, from: number, to: number, nat = 'JAP'): void => { for (let t = from; t <= to; t++) month(g, t, nat); };
/** A province changes hands for the test: whoever stood on it is sent off, and it is the taker's. */
const lose = (g: Game, name: string, to: string, away: string): Region => {
  const r = g.byName[name];
  for (const u of [...r.units]) relocate(g, u, g.byName[away]);
  r.ctrl = to;
  return r;
};
const offer = (g: Game, name: string) => rowOffers(g).find((o) => o.name === name);
/** The two countries make peace, for the test. */
const peace = (g: Game, a: string, b: string): void => {
  g.nations[a].atWar = g.nations[a].atWar.filter((x) => x !== b);
  g.nations[b].atWar = g.nations[b].atWar.filter((x) => x !== a);
};
const told = (g: Game, from: number, about: RegExp): string[] => g.headlines.slice(from).map((h) => h.text).filter((t) => about.test(t));

const T1 = '1st Tank Regiment', T6 = '6th Tank Regiment', T7 = '7th Tank Regiment', D1 = '1st Tank Division', D2 = '2nd Tank Division';
const GUNS = '23rd Army artillery (1st Artillery Group)';
const BURMA = '15th Army (33rd and 55th Divisions)';
const FORTY8 = '23rd Army (48th Division)', FORMOSA = 'Formosa Army';
const GARRISON = 'Tientsin-Pukow railway garrison (21st Division, less its Bataan detachment)', BATAAN = '21st Division detachment (for Bataan)';
const LEFT = /lent to the campaign/;

/** A player's Japan in December 1941 (month 28), at war with Britain and with Singapore his: the 1st and 6th Tank Regiments' loans are over. */
const singapore = (human: string | null = 'JAP'): Game => {
  const g = war(human);
  months(g, 1, 27);
  setWar(g, 'JAP', 'UK');
  lose(g, 'Singapore', 'JAP', 'Calcutta');
  return g;
};

describe('the data: three of Japan\'s lent regiments are awaited by their divisions', () => {
  it('the 1st and 6th were lent to take Singapore, the 7th to take Bataan; the tank divisions of June 1942 await them', () => {
    expect(rows[index(T1)].slice(0, 6)).toEqual([27, 'JAP', T1, 'arm', 1, 'Hainan']);
    expect(rows[index(T1)][6]).toMatchObject({ str: 0.25, until: 35, takes: 'Singapore' });
    expect(rows[index(T6)][6]).toMatchObject({ str: 0.25, until: 32, takes: 'Singapore' });
    expect(rows[index(T7)][6]).toMatchObject({ str: 0.25, until: 33, takes: 'Bataan' });
    expect(rows[index(D1)][6]).toMatchObject({ str: 0.45, est: 0.8, awaits: [T1] });
    expect(rows[index(D2)][6]).toMatchObject({ str: 0.5, est: 0.8, awaits: [T6, T7] });
    // No other country's row awaits anybody.
    expect(rows.filter((r) => r[6]?.awaits).map((r) => [r[1], r[2]])).toEqual([['JAP', D1], ['JAP', D2]]);
  });
});

describe('B8, the end of a loan: a regiment its division awaits leaves only if the player lets it go', () => {
  it("with Singapore his, the player's 1st and 6th Tank Regiments stay on the map and stand in the list; a computer's leave as they did", () => {
    const g = singapore();
    const t1 = row(g, T1)!, t6 = row(g, T6)!;
    const news = g.headlines.length;
    month(g, 28);
    expect(row(g, T1)).toBe(t1);
    expect(row(g, T6)).toBe(t6);
    expect(told(g, news, LEFT)).toEqual([]);
    const o = offer(g, T1)!;
    expect(o).toMatchObject({ id: index(T1), name: T1, kind: 'arm', divs: 1, men: men(t1), since: 28, region: t1.reg, joins: null, why: null });
    expect(o.takes).toEqual([{ id: t1.id, name: T1, kind: 'arm', region: t1.reg, divs: 1, men: men(t1) }]);
    // Its division is not formed until June: let go now, its worth is remembered for it.
    expect(o.loan).toBe(`It was lent to take Singapore, and its loan is over. Let go, it goes home, and joins its division, ${D1}, when that is formed; kept, it stays under your command, and its division stays short of it.`);
    expect(offer(g, T6)!.loan).toContain(`joins its division, ${D2}, when that is formed`);
    // It stands until he answers, and the regiment with it.
    months(g, 29, 33);
    expect([row(g, T1), offer(g, T1)!.since, row(g, T6), offer(g, T6)!.since]).toEqual([t1, 28, t6, 28]);
    // The 7th was lent to take Bataan, which is not his: nothing is asked of it.
    expect([row(g, T7) !== undefined, offer(g, T7)]).toEqual([true, undefined]);

    // A computer's Japan: both leave the month Singapore is its own, as before the mending.
    const c = singapore(null);
    const was = c.headlines.length;
    month(c, 28);
    expect([row(c, T1), row(c, T6)]).toEqual([undefined, undefined]);
    expect(told(c, was, LEFT)).toEqual([`Japanese formations lent to the campaign leave for other fronts: ${T1}; ${T6}.`]);
    expect(rowOffers(c)).toEqual([]);
    expect(Object.keys(c.flags).filter((k) => /^(leave|kept|offer):/.test(k))).toEqual([]);
  });

  it('let go with its division on the map, it goes home into it one for one, as the rule always had it go', () => {
    const g = singapore();
    months(g, 28, 34);
    expect(takeOffer(g, index(D1))).toBeNull();
    const d = row(g, D1)!;
    // The division without its lent regiment: the row's 0.45, and a fullest size short of the regiment's 0.25.
    expect([d.str, d.full]).toEqual([0.45, 0.55]);
    expect(offer(g, T1)!.loan).toBe(`It was lent to take Singapore, and its loan is over. Let go, it goes home to its division, ${D1}; kept, it stays under your command, and its division stays short of it.`);
    const news = g.headlines.length;
    expect(takeOffer(g, index(T1))).toBeNull();
    expect(row(g, T1)).toBeUndefined();
    expect(d.str).toBeCloseTo(0.7, 9);
    expect(d.full).toBe(0.8);
    expect(told(g, news, LEFT)).toEqual([`Japanese formations lent to the campaign leave for other fronts: ${T1}.`]);
    expect([offer(g, T1), g.flags[`lent:${index(T1)}`], g.flags[`leave:${index(T1)}`]]).toEqual([undefined, -1, undefined]);
    expect(takeOffer(g, index(T1))).toBe('There is no such offer.');
    // It does not come back, and is not taken twice.
    months(g, 35, 37);
    expect(row(g, T1)).toBeUndefined();
    expect(d.str).toBeGreaterThanOrEqual(0.7);
  });

  it('let go before its division is formed, its worth is remembered and the division forms with it', () => {
    const g = singapore();
    month(g, 28);
    expect(takeOffer(g, index(T1))).toBeNull();
    expect(row(g, T1)).toBeUndefined();
    months(g, 29, 34);
    expect(takeOffer(g, index(D1))).toBeNull();
    const d = row(g, D1)!;
    expect(d.str).toBeCloseTo(0.7, 9);
    expect(d.full).toBe(0.8);
  });

  it('kept, it stays his for good and is never asked for again; its division stays short of it while it lives', () => {
    const g = singapore();
    month(g, 28);
    const t6 = row(g, T6)!;
    expect(declineOffer(g, index(T6))).toBe(true);
    expect(declineOffer(g, index(T6))).toBe(false);
    expect(offer(g, T6)).toBeUndefined();
    months(g, 29, 34);
    expect(takeOffer(g, index(D2))).toBeNull();
    const d = row(g, D2)!;
    // Its row counts four regiments in June 1942 and two of them are not there (the 6th kept, the 7th lent still):
    // 0.5 less two quarters, and a fullest size short of both shares. Nobody is counted twice.
    expect(d.str).toBeCloseTo(0.25, 9);
    expect(d.full).toBeCloseTo(0.3, 9);
    months(g, 35, 44);
    expect(row(g, T6)).toBe(t6);
    expect(offer(g, T6)).toBeUndefined();
    expect(d.full).toBeCloseTo(0.3, 9);
    expect([g.flags[`kept:${index(T6)}`], g.flags[`leave:${index(T6)}`]]).toEqual([1, undefined]);
    // Destroyed, it holds no share any more: the drafts may fill it, as the record replaced losses.
    removeCorps(g, t6);
    month(g, 45);
    expect(d.full).toBeCloseTo(0.55, 9);
  });

  it('destroyed while the offer stands, there is nothing left to ask about', () => {
    const g = singapore();
    month(g, 28);
    removeCorps(g, row(g, T1)!);
    expect(offer(g, T1)).toBeUndefined();
    expect(takeOffer(g, index(T1))).toBe('There is no such offer.');
    month(g, 29);
    expect([g.flags[`leave:${index(T1)}`], g.flags[`lent:${index(T1)}`]]).toEqual([undefined, -1]);
  });

  it('a saved war keeps the offers and the answers', () => {
    const g = singapore();
    month(g, 28);
    expect(declineOffer(g, index(T6))).toBe(true);
    const back = loadGame(saveGame(g))!;
    expect(rowOffers(back)).toEqual(rowOffers(g));
    expect(offer(back, T1)!.since).toBe(28);
    expect(back.flags[`kept:${index(T6)}`]).toBe(1);
    months(back, 29, 30);
    expect([row(back, T1) !== undefined, row(back, T6) !== undefined, offer(back, T6)]).toEqual([true, true, undefined]);
  });

  it("every other loan ends as it did, the player's too: the 23rd Army's guns leave the month Hong Kong is his (put to the design director)", () => {
    const g = war();
    months(g, 1, 27);
    expect(rows[index(GUNS)][6]).toMatchObject({ until: 34, takes: 'Hong Kong' });
    expect(row(g, GUNS)).toBeDefined();
    setWar(g, 'JAP', 'UK');
    lose(g, 'Hong Kong', 'JAP', 'Calcutta');
    const news = g.headlines.length;
    month(g, 28);
    expect(row(g, GUNS)).toBeUndefined();
    expect(told(g, news, LEFT)).toEqual([`Japanese formations lent to the campaign leave for other fronts: ${GUNS}.`]);
    expect(offer(g, GUNS)).toBeUndefined();
  });
});

describe('B8, an offer whose war is over lapses, and he is told', () => {
  it('the 15th Army for Burma, offered with Britain at war, lapses the month after peace and never comes', () => {
    const g = war();
    const id = rows.findIndex((r) => r[1] === 'JAP' && r[2] === BURMA && r[6].stops.includes(3));
    expect(rows[id][6].war).toEqual(['JAP', 'UK']);
    months(g, 28, 30);
    setWar(g, 'JAP', 'UK');
    months(g, 31, 32);
    expect(offer(g, BURMA)).toMatchObject({ id, since: 32, why: null });
    peace(g, 'JAP', 'UK');
    // Asked for before the month turns, it is refused with the reason.
    const words = `The offer to form ${BURMA} has lapsed: the war it was raised for is over.`;
    expect(offer(g, BURMA)!.why).toBe(words);
    const news = g.headlines.length;
    month(g, 33);
    expect(offer(g, BURMA)).toBeUndefined();
    expect(told(g, news, /has lapsed/)).toEqual([words]);
    expect(g.flags[`sent:${id}`]).toBe(2);
    // His corps stays as it is, and a new war does not bring the row back.
    expect(row(g, '11th Army (33rd Division)')).toBeDefined();
    setWar(g, 'JAP', 'UK');
    months(g, 34, 36);
    expect([offer(g, BURMA), row(g, BURMA)]).toEqual([undefined, undefined]);
  });

  it('taken in the month of the peace, it is refused with the same words and lapses then', () => {
    const g = war();
    months(g, 28, 30);
    setWar(g, 'JAP', 'UK');
    months(g, 31, 32);
    const id = offer(g, BURMA)!.id;
    peace(g, 'JAP', 'UK');
    expect(takeOffer(g, id)).toBe(`The offer to form ${BURMA} has lapsed: the war it was raised for is over.`);
    expect([offer(g, BURMA), row(g, BURMA), g.flags[`sent:${id}`]]).toEqual([undefined, undefined, 2]);
  });
});

describe('B8, the lapse says how many of the corps are gone', () => {
  it('one corps named and gone: "the corps ... is"', () => {
    const g = war();
    months(g, 1, 19);
    expect(offer(g, FORTY8)!.takes.map((t) => t.name)).toEqual([FORMOSA]);
    removeCorps(g, row(g, FORMOSA)!);
    expect(offer(g, FORTY8)!.why).toBe(`The offer to form ${FORTY8} has lapsed: the corps it was to be formed from is no longer on the map.`);
  });

  it('three named: one of the 3, 2 of the 3, the 3', () => {
    const g = war();
    months(g, 1, 34);
    expect(offer(g, D1)!.takes.map((t) => t.name)).toEqual(['3rd Tank Regiment', '5th Tank Regiment', '9th Tank Regiment']);
    removeCorps(g, row(g, '3rd Tank Regiment')!);
    expect(offer(g, D1)!.why).toBe(`The offer to form ${D1} has lapsed: one of the 3 corps it was to be formed from is no longer on the map.`);
    removeCorps(g, row(g, '5th Tank Regiment')!);
    expect(offer(g, D1)!.why).toBe(`The offer to form ${D1} has lapsed: 2 of the 3 corps it was to be formed from are no longer on the map.`);
    removeCorps(g, row(g, '9th Tank Regiment')!);
    expect(offer(g, D1)!.why).toBe(`The offer to form ${D1} has lapsed: the 3 corps it was to be formed from are no longer on the map.`);
    const news = g.headlines.length;
    month(g, 35);
    expect(told(g, news, /has lapsed/)).toEqual([`The offer to form ${D1} has lapsed: the 3 corps it was to be formed from are no longer on the map.`]);
  });
});

describe('B8, a row that waits on a cut the player has been offered is not struck while he decides', () => {
  /**
   * A player's Japan at war with America, in the last month of the offer to cut Hsuchow's garrison, with no answer
   * given. (Six months after it was made, its half year's end: the design director's ruling of 13:39Z, 25 November
   * 2026, point 5. As first built the offer stood for good, and this was ten months after.)
   */
  const waiting = (): Game => {
    const g = war();
    months(g, 1, 30);
    expect(offer(g, GARRISON)).toMatchObject({ since: 30, last: 36, why: null });
    setWar(g, 'JAP', 'USA');
    months(g, 31, 36);
    return g;
  };

  it("the 21st Division's detachment for Bataan waits on the offer to cut Hsuchow's garrison, and comes the month after he takes it", () => {
    expect(rows[index(BATAAN)][6].cut).toContain(GARRISON);
    const g = waiting();
    // (As built it was struck in month 38, its half year out, with the offer still unanswered.)
    expect([g.flags[`sent:${index(BATAAN)}`], row(g, BATAAN), offer(g, GARRISON)!.since]).toEqual([undefined, undefined, 30]);
    expect(takeOffer(g, index(GARRISON))).toBeNull();
    month(g, 37);
    const c = row(g, BATAAN)!;
    // Lingayen is America's and Japan holds no harbour in the Philippines: it forms at home (ruling B7), at its 0.25.
    expect([homeland(g.regions[c.reg], 'JAP'), c.str]).toEqual([true, 0.25]);
  });

  it('declined, the cut is never made: the detachment has its half year from that month and is then struck', () => {
    const g = waiting();
    expect(declineOffer(g, index(GARRISON))).toBe(true);
    months(g, 37, 43);
    expect(g.flags[`sent:${index(BATAAN)}`]).toBeUndefined();
    month(g, 44);
    expect([g.flags[`sent:${index(BATAAN)}`], row(g, BATAAN)]).toEqual([2, undefined]);
  });

  it("a computer's Japan is as it was: the garrison is cut in its month and the detachment waits for nobody's answer", () => {
    const g = war(null);
    months(g, 1, 30);
    expect(row(g, GARRISON)).toBeDefined();
    expect(rowOffers(g)).toEqual([]);
  });
});

describe('B8, a row made from a formation the player has been offered: nobody is counted twice', () => {
  const A = '25th Army (18th Division)', B = '18th Division', SRC = '21st Army (18th Division)';
  /** A player's Japan in November 1941 with the offer to form the 25th Army's 18th Division open, then at war with Britain and Rangoon his. */
  const rangoon = (): Game => {
    const g = war();
    months(g, 1, 27);
    expect(offer(g, A)).toMatchObject({ since: 27, why: null });
    expect(offer(g, A)!.takes.map((t) => t.name)).toEqual([SRC]);
    setWar(g, 'JAP', 'UK');
    lose(g, 'Rangoon', 'JAP', 'Calcutta');
    return g;
  };
  const eighteenths = (g: Game): string[] => [SRC, A, B].filter((nm) => row(g, nm));

  it('the data: the 18th Division for Rangoon is made from the 25th Army\'s, which is made from the 21st Army\'s', () => {
    expect(rows[index(A)].slice(0, 6)).toEqual([27, 'JAP', A, 'inf', 1, 'Hainan']);
    expect(rows[index(A)][6].from).toEqual([SRC]);
    expect(rows[index(B)].slice(0, 6)).toEqual([32, 'JAP', B, 'inf', 1, 'Rangoon']);
    expect(rows[index(B)][6].from).toEqual([A]);
  });

  it('while he has not answered, the 18th Division waits and is not struck; taken, the chain runs as the record ran it', () => {
    const g = rangoon();
    // (As built the 18th Division formed whole at Rangoon in month 32 and took nobody, with the offer still open.)
    // (He answers in the offer's last month, May 1942: its half year, the ruling of 13:39Z, 25 November 2026, point 5.)
    expect(offer(g, A)!.last).toBe(33);
    months(g, 28, 33);
    expect([row(g, B), g.flags[`sent:${index(B)}`], offer(g, B)]).toEqual([undefined, undefined, undefined]);
    expect(eighteenths(g)).toEqual([SRC]);
    expect(takeOffer(g, index(A))).toBeNull();
    expect(eighteenths(g)).toEqual([A]);
    // The month after, the 18th Division is offered out of the corps just formed, at Rangoon.
    month(g, 34);
    expect(offer(g, B)).toMatchObject({ since: 34, region: g.byName.Rangoon.id, why: null });
    expect(offer(g, B)!.takes.map((t) => t.name)).toEqual([A]);
    expect(takeOffer(g, index(B))).toBeNull();
    expect(eighteenths(g)).toEqual([B]);
    expect([g.regions[row(g, B)!.reg].name, row(g, B)!.str]).toEqual(['Rangoon', 0.65]);
  });

  it('declined, the 18th Division never comes: its men are the corps he kept, and the offer said so', () => {
    const g = rangoon();
    expect(offer(g, A)!.then).toEqual([B]);
    expect(declineOffer(g, index(A))).toBe(true);
    months(g, 28, 34);
    expect([row(g, B), g.flags[`sent:${index(B)}`], offer(g, B)]).toEqual([undefined, 2, undefined]);
    expect(eighteenths(g)).toEqual([SRC]);
  });

  it('lapsed for a corps gone, with the corps he kept gone too, the later row is as it always was', () => {
    const g = rangoon();
    removeCorps(g, row(g, SRC)!);
    months(g, 28, 32);
    // The offer lapsed in month 28, and nobody of the 18th is on the board: the row comes as the rule always had it.
    expect(g.flags[`sent:${index(A)}`]).toBe(2);
    expect(eighteenths(g)).toEqual([B]);
  });

  it('a later row that names a corps he keeps by declining is not given up: it is offered still', () => {
    const g = war();
    months(g, 1, 19);
    // The 14th Army's 48th Division (November 1941) is made from the 23rd Army's 48th Division or the Formosa Army.
    expect(rows[index('14th Army (48th Division)')][6].from).toEqual([FORTY8, FORMOSA]);
    expect(offer(g, FORTY8)!.then).toEqual([]);
    expect(declineOffer(g, index(FORTY8))).toBe(true);
    months(g, 20, 27);
    expect(offer(g, '14th Army (48th Division)')!.takes.map((t) => t.name)).toEqual([FORMOSA]);
  });
});

describe('B8, who is asked: every player on the whole world, for the rows the grand campaign sends', () => {
  it("a player's Britain is asked too: Hong Kong's garrison of August 1941 is an offer, and taken it is the row's", () => {
    const HK = 'China Command (Hong Kong garrison)';
    expect(rows[index(HK, 'UK')].slice(0, 6)).toEqual([24, 'UK', HK, 'inf', 1, 'Hong Kong']);
    expect(rows[index(HK, 'UK')][6]).toMatchObject({ str: 0.7, from: [HK] });
    const g = war('UK');
    const old = row(g, HK, 'UK')!;
    months(g, 1, 24, 'UK');
    expect(row(g, HK, 'UK')).toBe(old);
    const o = offer(g, HK)!;
    expect(o).toMatchObject({ id: index(HK, 'UK'), since: 24, region: g.byName['Hong Kong'].id, why: null, loan: null });
    expect(o.takes.map((t) => t.id)).toEqual([old.id]);
    expect(takeOffer(g, o.id)).toBeNull();
    const now = row(g, HK, 'UK')!;
    expect([now === old, now.str, g.regions[now.reg].name]).toEqual([false, 0.7, 'Hong Kong']);
    expect(g.corps.filter((c) => c.nat === 'UK' && c.name === HK)).toHaveLength(1);
  });

  it('the old chart\'s rows made from corps on the board all stand in the book\'s own theatres, which the grand campaign sends to nobody: no offer of one can be made', () => {
    const g = war(null);
    const scripted = new Map<string, number>();
    for (const t of Object.values(theatres as Record<string, any>)) if (typeof t !== 'string' && t.scripted) for (const nm of t.regions as string[]) scripted.set(nm, t.opens ?? 0);
    const made = rows.filter((r) => Array.isArray(r[6]?.from) && g.byName[r[5]]);
    const old = made.filter((r) => !beyondEurope(g.byName[r[5]])), beyond = made.filter((r) => beyondEurope(g.byName[r[5]]));
    // Seventeen on the old chart (Germany's eleven in the West, Britain's four and America's two in Italy), every one
    // in a theatre of the book's own, dated on or after its opening.
    expect(old.reduce((n: Record<string, number>, r) => ({ ...n, [r[1]]: (n[r[1]] ?? 0) + 1 }), {})).toEqual({ USA: 2, UK: 4, GER: 11 });
    expect(old.every((r) => scripted.has(r[5]) && r[0] >= scripted.get(r[5])!)).toBe(true);
    // Beyond it: Japan's, four of Britain's and one of China's.
    expect(beyond.reduce((n: Record<string, number>, r) => ({ ...n, [r[1]]: (n[r[1]] ?? 0) + 1 }), {})).toMatchObject({ UK: 4, CHN: 1 });
  });
});

type Call = (...a: unknown[]) => any;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;

describe("the 3D game's calls: a loan's end in the one list", () => {
  it('TC.offers() says what each answer does; let go and kept are told in their own words', () => {
    const g = singapore();
    month(g, 28);
    g.phase = g.order.indexOf('JAP');
    for (const id of rowOffers(g).filter((o) => o.loan === null).map((o) => o.id)) declineOffer(g, id);
    const t1 = row(g, T1)!;
    expect(TC().load(saveGame(g))).not.toBeNull();
    const list = TC().offers() as { count: number; head: string; note: string; can: boolean; rows: { id: number; name: string; what: string; where: string; since: string; takes: { name: string; where: string }[]; line: string; can: boolean; loan: boolean }[] };
    // (Head, note and the sentences said: the UI director's marks of 25 November 2026, 13:39Z, sections 3 and 5.)
    expect(list).toMatchObject({ count: 2, head: '2 loans ending', can: true });
    expect(list.note).toBe("A loan's end: send the regiment home into its division, or keep it for good.");
    expect(list.rows.map((r) => [r.id, r.name, r.loan])).toEqual([[index(T1), T1, true], [index(T6), T6, true]]);
    // (The kind in the division words, "Armoured": the UI director's marks, section 2, read by the skeptic's review of 25 November 2026 for TC.offers() too.)
    expect(list.rows[0]).toMatchObject({ what: `Armoured, ${men(t1).toLocaleString('en-US')} men`, where: 'Hainan', since: 'Its loan ended in December 1941', can: true });
    expect(list.rows[0].takes.map((t) => [t.name, t.where])).toEqual([[T1, 'Hainan']]);
    expect(list.rows[0].line).toBe(`It was lent to take Singapore, and its loan is over. Let go, it goes home, and joins its division, ${D1}, when that is formed; kept, it stays under your command, and its division stays short of it.`);

    const go = TC().offerTake([index(T1)]) as { ok: boolean; taken: number[]; said: string; offers: { count: number; head: string; note: string } };
    expect(go).toMatchObject({ ok: true, taken: [index(T1)], said: `${T1} is sent home.` });
    expect(go.offers).toMatchObject({ count: 1, head: '1 loan ending', note: "A loan's end: send the regiment home into its division, or keep it for good." });
    const keep = TC().offerDecline('all') as { ok: boolean; declined: number[]; said: string; offers: { count: number } };
    expect(keep).toMatchObject({ ok: true, declined: [index(T6)], said: `${T6} stays yours for good.` });
    expect(keep.offers.count).toBe(0);
  });

  it('a formation and a loan in one list: the heading counts each', () => {
    const g = singapore();
    month(g, 28);
    g.phase = g.order.indexOf('JAP');
    const calls = offerCalls(() => g, () => null);
    const list = calls.offers();
    const forms = list.rows.filter((r) => !r.loan).length;
    expect(forms).toBeGreaterThan(1);
    expect(list.head).toBe(`${forms} formations offered, 2 loans ending`);
    expect(list.note).toBe("Each would be formed out of corps you have, as history formed it. Each waits for your answer until its last month, or until a corps it needs is gone. A loan's end: send the regiment home into its division, or keep it for good.");
  });

  it('a row says what declining it also gives up', () => {
    const g = war();
    months(g, 1, 27);
    const list = offerCalls(() => g, () => null).offers();
    // (The UI director's marks, section 2.3: "which would later be formed out of it", for TC.offers() as for the page.)
    expect(list.rows.find((r) => r.name === '25th Army (18th Division)')!.after).toBe('Declining it also gives up 18th Division, which would later be formed out of it.');
    expect(list.rows.find((r) => r.name === '14th Army (48th Division)')!.after).toBe('');
  });

  it('a reason is a sentence of its own: it follows a full stop, never a colon', () => {
    const g = war();
    months(g, 1, 2);
    const FIFTH = '21st Army (5th Division)';
    const calls = offerCalls(() => g, () => null);
    // Not his month.
    g.phase = g.order.indexOf('GER');
    expect(calls.offerTake([index(FIFTH)])).toMatchObject({ ok: false, taken: [], said: `${FIFTH} is not formed. It is not your month.` });
    // No such offer: the bare reason.
    g.phase = g.order.indexOf('JAP');
    expect(calls.offerTake([9999])).toMatchObject({ ok: false, said: 'There is no such offer.' });
    // A loan he cannot let go out of his month stays.
    const s = singapore();
    month(s, 28);
    s.phase = s.order.indexOf('GER');
    expect(offerCalls(() => s, () => null).offerTake([index(T1)])).toMatchObject({ ok: false, said: `${T1} stays. It is not your month.` });
  });
});
