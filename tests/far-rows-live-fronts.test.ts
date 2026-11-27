// A dated row takes no corps standing on a live front of its staff's plan (the design director's rulings of 05:30Z
// and 05:35Z of 21 November 2026 to ruling 234; the record: the historian's far-chart-airfields.md, section 6).
//
// As measured (war 22 at stop 3, the Burma rows laid): the 18th Division's row formed at Rangoon in month 32 and took
// the 25th Army's 18th Division out of Malaya with Singapore still standing; Singapore had not fallen by month 44.
// The record's 18th crossed to Singapore, left Malaya only after the fall and landed at Rangoon in April 1942 without
// its 124th Regiment, the Kawaguchi Detachment. The 56th Division's main body needed only Rangoon's port, and the
// Sakaguchi Detachment rejoined it after Java.
//
// So: a row whose corps stand on a live front waits for that front, and its half year is counted from the month the
// front lets them go; a row with a body of its own (`adds`) comes at that worth and takes nobody. Read wherever the
// plan's fronts are read (planfronts.ts): where no book dates Japan's rows, the strike south gone, never for the
// player. The states are set by hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { createGame, declareWar, relocate, rowOffers, startPhase, takeOffer } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { frontsState, onLiveFront } from '../src/campaign/planfronts';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';

const D18 = '18th Division', MALAYA18 = '25th Army (18th Division)', KAWAGUCHI = 'Kawaguchi Detachment (35th Brigade, of the 18th Division)';
const D56 = '56th Division', SAKAGUCHI = 'Sakaguchi Detachment (56th Mixed Infantry Group)';

const rows = (setup as any).expeditions as any[][];
const index = (name: string): number => rows.findIndex((r) => r[1] === 'JAP' && r[2] === name);
const row = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead);
const at = (g: Game, c: Corps): string => g.regions[c.reg].name;
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'JAP'); };
const months = (g: Game, from: number, to: number): void => { for (let t = from; t <= to; t++) month(g, t); };
/** A province changes hands for the test: whoever stood on it is sent off, and it is the taker's. */
const lose = (g: Game, name: string, to: string, away: string): Region => {
  const r = g.byName[name];
  for (const u of [...r.units]) relocate(g, u, g.byName[away]);
  r.ctrl = to;
  return r;
};
/**
 * A war with the strike's rows on their quays (month 27) and, where `went`, the strike south gone in month 28: Japan
 * at war with America, Britain and the Netherlands, and Rangoon Japan's unless said.
 */
const war = (o: { stop?: number; human?: string | null; went?: boolean; rangoon?: boolean } = {}): Game => {
  const g = startOpen(createGame(3, o.human ?? null));
  // (A player's rows are offers only where the switch 'offers' is on: stage.json, off until its screen passes, the
  // design director's 13:39Z of 25 November 2026. A campaign opened with it on carries the flag; set by hand here.)
  if (o.human) g.flags.offers = 1;
  if (o.stop && o.stop !== 3) setStop(g, o.stop, true);
  // (Cam Ranh is Japan's, as the demand on Vichy France makes it before the strike south in a played war. Since the
  // design director's ruling B7 of 25 November 2026 a row posted on a neutral's ground waits for its war, where no
  // book dates the rows: with Cam Ranh still France's the Kawaguchi Detachment would not be on its quay in month 27.)
  lose(g, 'Cam Ranh', 'JAP', 'Saigon');
  month(g, 27);
  g.turn = 28;
  if (o.went !== false) g.flags['st:done:JAP:south'] = 28;
  for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
  if (o.rangoon !== false) lose(g, 'Rangoon', 'JAP', 'Calcutta');
  return g;
};
const live = (g: Game, id: string): boolean => !!frontsState(g, 'JAP').find((f) => f.id === id)?.live;

describe('the data: the 18th Division came to Rangoon without the Kawaguchi Detachment', () => {
  it("its row, at every stop, is made of the 25th Army's 18th Division alone, at that row's 0.65", () => {
    const r = rows[index(D18)];
    expect(r.slice(0, 6)).toEqual([32, 'JAP', D18, 'inf', 1, 'Rangoon']);
    expect(r[6]).toMatchObject({ str: 0.65, est: 0.65, from: [MALAYA18], war: ['JAP', 'UK'] });
    expect(r[6].stops).toBeUndefined();
    expect(rows[index(MALAYA18)][6]).toMatchObject({ str: 0.65, est: 0.65 });
    // The 56th's body is its own, 0.67, beside the Sakaguchi Detachment's 0.33.
    expect(rows[index(D56)][6]).toMatchObject({ adds: 0.67, from: [SAKAGUCHI] });
  });
});

describe('a dated row takes no corps standing on a live front', () => {
  it("the 18th Division waits while Singapore stands, and comes the month after it falls: the 25th Army's 18th is taken then, and the Kawaguchi Detachment stays its own row", () => {
    const g = war();
    const src = row(g, MALAYA18)!;
    expect(live(g, 'malaya')).toBe(true);
    expect(onLiveFront(g, 'JAP', src)).toBe(true);
    months(g, 32, 34);
    expect(row(g, D18)).toBeUndefined();
    expect(row(g, MALAYA18)).toBe(src);
    // Singapore falls in month 34; the row comes as Japan's next month opens.
    lose(g, 'Singapore', 'JAP', 'Calcutta');
    expect(onLiveFront(g, 'JAP', src)).toBe(false);
    month(g, 35);
    const c = row(g, D18)!;
    expect([at(g, c), c.divs, c.str, c.full]).toEqual(['Rangoon', 1, 0.65, 0.65]);
    expect(c.general).not.toBeNull();
    expect(row(g, MALAYA18)).toBeUndefined();
    expect(row(g, KAWAGUCHI)).toBeDefined();
    // It came once.
    month(g, 36);
    expect(g.corps.filter((x) => x.name === D18 && !x.dead)).toHaveLength(1);
  });

  it('its half year is counted from the month the front lets go: a Singapore that stands ten months does not strike the row', () => {
    const g = war();
    months(g, 32, 41);
    expect(row(g, D18)).toBeUndefined();
    expect(g.flags[`sent:${index(D18)}`]).toBeUndefined();
    lose(g, 'Singapore', 'JAP', 'Calcutta');
    month(g, 42);
    expect(at(g, row(g, D18)!)).toBe('Rangoon');
  });

  it("let go, it still waits its half year for Rangoon and is then struck, and its corps stays as it is", () => {
    const g = war({ rangoon: false });
    months(g, 32, 34);
    lose(g, 'Singapore', 'JAP', 'Calcutta');
    months(g, 35, 41);
    expect(row(g, D18)).toBeUndefined();
    expect(g.flags[`sent:${index(D18)}`]).toBeUndefined();
    month(g, 42);
    expect(g.flags[`sent:${index(D18)}`]).toBe(2);
    expect(row(g, D18)).toBeUndefined();
    expect(row(g, MALAYA18)).toBeDefined();
  });

  it("a row with a body of its own comes at that worth and takes nobody: the 56th Division lands at Rangoon at 0.67 with the Sakaguchi Detachment on the Indies' front", () => {
    const g = war();
    const group = row(g, SAKAGUCHI)!;
    expect(live(g, 'indies')).toBe(true);
    expect(onLiveFront(g, 'JAP', group)).toBe(true);
    month(g, 31);
    const body = row(g, D56)!;
    expect([at(g, body), body.divs, body.str, body.full]).toEqual(['Rangoon', 1, 0.67, 0.67]);
    expect(row(g, SAKAGUCHI)).toBe(group);
    expect([group.str, group.full]).toEqual([0.33, 0.33]);
    // One division between them, as the record raised.
    expect(body.str + group.str).toBe(1);
    // It came once: the Indies' front over later brings no second body and takes no detachment.
    lose(g, 'Batavia', 'JAP', 'Soerabaja');
    month(g, 32);
    expect(g.corps.filter((x) => x.name === D56 && !x.dead)).toHaveLength(1);
    expect(row(g, SAKAGUCHI)).toBe(group);
  });

  it("with the Indies' front over on its date the 56th lands whole and takes the detachment, as before", () => {
    const g = war();
    lose(g, 'Batavia', 'JAP', 'Soerabaja');
    expect(live(g, 'indies')).toBe(false);
    month(g, 31);
    const body = row(g, D56)!;
    expect([at(g, body), body.str, body.full]).toEqual(['Rangoon', 1, undefined]);
    expect(row(g, SAKAGUCHI)).toBeUndefined();
  });
});

describe("only where the plan's fronts are read", () => {
  it('with the strike south not gone there is no front, and the rows come as they did', () => {
    const g = war({ went: false });
    expect(frontsState(g, 'JAP')).toEqual([]);
    month(g, 32);
    expect(at(g, row(g, D18)!)).toBe('Rangoon');
    expect(row(g, MALAYA18)).toBeUndefined();
    expect(row(g, KAWAGUCHI)).toBeDefined();
    expect(row(g, D56)!.str).toBe(1);
    expect(row(g, SAKAGUCHI)).toBeUndefined();
  });

  it("at the book's stops the book dates Japan's rows: the 18th comes in its month, made of the 25th Army's 18th alone", () => {
    for (const stop of [1, 2]) {
      const g = war({ stop });
      expect(frontsState(g, 'JAP'), `stop ${stop}`).toEqual([]);
      month(g, 32);
      const c = row(g, D18)!;
      expect([at(g, c), c.str, c.full], `stop ${stop}`).toEqual(['Rangoon', 0.65, 0.65]);
      expect(row(g, MALAYA18), `stop ${stop}`).toBeUndefined();
      expect(row(g, KAWAGUCHI), `stop ${stop}`).toBeDefined();
    }
  });

  it("never for the player: no front is read for his Japan, and his rows are offered by their dates", () => {
    const g = war({ human: 'JAP' });
    expect(frontsState(g, 'JAP')).toEqual([]);
    // (Since the design director's ruling B8 of 25 November 2026 a row that would take corps of the player's is an
    // offer. The 25th Army's 18th Division is one, of month 27; while it stood unanswered the 18th Division for
    // Rangoon came whole in month 32 and took nobody, the same men twice, and this test passed on it. It waits on
    // his answer now: tests/far-dated-rows-review.test.ts.)
    month(g, 32);
    expect([row(g, D18), row(g, MALAYA18)]).toEqual([undefined, undefined]);
    expect(takeOffer(g, index(MALAYA18))).toBeNull();
    expect(row(g, MALAYA18)).toBeDefined();
    // No front holds his row back: with Singapore still standing it is offered the month after, and taken it forms.
    month(g, 33);
    expect(rowOffers(g).find((o) => o.name === D18)).toMatchObject({ since: 33, why: null });
    expect(takeOffer(g, index(D18))).toBeNull();
    expect(at(g, row(g, D18)!)).toBe('Rangoon');
    expect(row(g, MALAYA18)).toBeUndefined();
  });
});
