// Japan's dated reinforcements on the whole world: where they form, when, and offers in place of silent removals
// (the design director's rulings B7 and B8 of 25 November 2026, design-director/pacific-japan-1125.md).
//
// As the audit found it: a player's Japan had every dated row posted on ground Japan did not OWN formed at Tokyo, 57
// rows of 86, 26 of them posted on ground in China that Japan held, against a sea lift of four corps a month; rows
// with no war of their own (the 65th Brigade for Lingayen) came at peace; and a row made from corps on the board
// (`from`: 65 of Japan's 152) struck those corps off wherever they stood, the player's as a computer's.
//
// B7, one rule for every nation under the far switch. A row whose posting its side does not hold forms at its
// country's held harbour nearest the posting in the same theatre (the posting's country's ground, joined by land),
// else at its capital. A row the record raised for an operation forms when that war has begun, never on its date.
// B8. A row that would take corps of the player's is an offer, never a silent removal: one list a month, take all or
// pick; it stands for the rest of its row's half year and lapses if a corps it needs is gone (the half year: the
// design director's ruling of 13:39Z, 25 November 2026, point 5). The computer's rows as they were.
// The states are set by hand, as tests/far-posted-ashore.test.ts sets them; the last block walks the 3D game's calls.
import { vi } from 'vitest';
// (The offers rule has its own switch, off until its screen passes the Mac: stage.ts OFFERS_STAGE. On for this file.)
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; (globalThis as any).TC_OFFERS = 1; });

import { describe, expect, it } from 'vitest';
import { createGame, declineOffer, men, postedFor, relocate, removeCorps, rowOffers, setWar, startPhase, takeOffer } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { homeland } from '../src/campaign/middleeast';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { formationOffers, orderDeclineFormation, orderTakeFormation, whyNotAnswerOffers } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';
import { chartDist } from '../src/campaign/wrap';
import setup from '../data/campaign/setup-1939.json';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

const rows = (setup as any).expeditions as any[][];
const index = (name: string): number => rows.findIndex((r) => r[1] === 'JAP' && r[2] === name);
const war = (human: string | null = 'JAP'): Game => startOpen(createGame(5, human));
const row = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead);
const at = (g: Game, name: string): string => g.regions[row(g, name)!.reg].name;
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'JAP'); };
const months = (g: Game, from: number, to: number): void => { for (let t = from; t <= to; t++) month(g, t); };
/** A province changes hands for the test: whoever stood on it is sent off, and it is the taker's. */
const lose = (g: Game, name: string, to: string, away: string): Region => {
  const r = g.byName[name];
  for (const u of [...r.units]) relocate(g, u, g.byName[away]);
  r.ctrl = to;
  return r;
};
const offer = (g: Game, name: string) => rowOffers(g).find((o) => o.name === name);

const BRIGADE = '14th Army (65th Brigade)', TANKS2 = '2nd Tank Regiment', RAIDERS = '2nd Raiding Regiment (paratroops of the 1st Raiding Group)';
const KAWAGUCHI = 'Kawaguchi Detachment (35th Brigade, of the 18th Division)', GUARDS = '25th Army (Imperial Guards Division)';
const FIFTH = '21st Army (5th Division)', FIFTH_OF = '12th Army (5th Division)', SEVENTH = '7th Division (Asahikawa)', SEVENTH_OF = '6th Army (7th Division)';
const FORTY8 = '23rd Army (48th Division)', FORMOSA = 'Formosa Army', CHEKIANG = '13th Army (Chekiang landing force)';
const BURMA = '15th Army (33rd and 55th Divisions)', BURMA_OF = '11th Army (33rd Division)';

describe("B7, where a row forms: the player's Japan", () => {
  it('a row posted on ground in China that Japan holds forms there, not at Tokyo', () => {
    const g = war();
    expect([g.byName.Canton.owner, g.byName.Canton.ctrl, g.byName.Hainan.owner, g.byName.Hainan.ctrl]).toEqual(['CHN', 'JAP', 'CHN', 'JAP']);
    months(g, 1, 2);
    expect(at(g, '21st Army (170th Infantry Regiment, of the 104th Division)')).toBe('Canton');
    expect(at(g, "229th Infantry Regiment (with the 38th Division's headquarters)")).toBe('Canton');
    month(g, 27);
    expect([at(g, '1st Tank Regiment'), at(g, '6th Tank Regiment')]).toEqual(['Hainan', 'Hainan']);
  });

  it("a row whose posting Japan does not hold forms at Japan's held harbour nearest it in the same theatre: the 65th Brigade on Luzon's own coast", () => {
    const g = war();
    setWar(g, 'JAP', 'USA');
    const lingayen = g.byName.Lingayen;
    lose(g, 'Aparri', 'JAP', 'Manila');
    lose(g, 'Davao', 'JAP', 'Manila');
    expect(lingayen.ctrl).toBe('USA');
    month(g, 29);
    const c = row(g, BRIGADE)!;
    // Aparri, the nearer of the two, and it holds nothing in place there.
    expect(chartDist(g, g.byName.Aparri, lingayen)).toBeLessThan(chartDist(g, g.byName.Davao, lingayen));
    expect([g.regions[c.reg].name, c.hold, c.str, c.full]).toEqual(['Aparri', false, 0.44, 0.44]);
    // It came once.
    month(g, 30);
    expect(g.corps.filter((x) => x.nat === 'JAP' && x.name === BRIGADE)).toHaveLength(1);
  });

  it('the theatre is the ground of the posting\'s country, joined by land: Mindanao is the Philippines, and a nearer port of Japan\'s own is not', () => {
    const g = war();
    setWar(g, 'JAP', 'USA');
    const lingayen = g.byName.Lingayen, davao = lose(g, 'Davao', 'JAP', 'Manila');
    // Japan holds ports far nearer Lingayen than Davao (Formosa's), and before the ruling the computer's row came ashore at the nearest of them.
    const nearer = g.regions.filter((r) => r.port && r.ctrl === 'JAP' && chartDist(g, r, lingayen) < chartDist(g, davao, lingayen));
    expect(nearer.length).toBeGreaterThan(0);
    expect(nearer.every((r) => r.owner !== 'USA')).toBe(true);
    month(g, 29);
    expect(at(g, BRIGADE)).toBe('Davao');
    // Read by itself: the place, and nothing taken.
    const h = war();
    setWar(h, 'JAP', 'USA');
    lose(h, 'Davao', 'JAP', 'Manila');
    expect(postedFor(h, 'JAP', rows[index(BRIGADE)], h.byName.Lingayen)).toMatchObject({ at: h.byName.Davao, adds: null, waits: false, takes: false });
  });

  it('else at the capital: with no harbour held in the theatre it forms at home, and Hong Kong is no harbour of Malaya', () => {
    const g = war();
    setWar(g, 'JAP', 'USA');
    month(g, 29);
    expect(homeland(g.regions[row(g, BRIGADE)!.reg], 'JAP')).toBe(true);
    // The 2nd Tank Regiment for Kota Bharu, with Hong Kong (Britain's too) in Japan's hands: Malaya is another stretch of Britain's ground.
    const h = war();
    month(h, 28);
    setWar(h, 'JAP', 'UK');
    lose(h, 'Hong Kong', 'JAP', 'Calcutta');
    expect([h.byName['Hong Kong'].owner, h.byName['Kota Bharu'].owner, h.byName['Hong Kong'].port]).toEqual(['UK', 'UK', true]);
    months(h, 29, 30);
    expect(homeland(h.regions[row(h, TANKS2)!.reg], 'JAP')).toBe(true);
    // With Penang held, it forms on Malaya's own coast.
    const k = war();
    month(k, 28);
    setWar(k, 'JAP', 'UK');
    lose(k, 'Penang', 'JAP', 'Singapore');
    months(k, 29, 30);
    expect(at(k, TANKS2)).toBe('Penang');
  });
});

describe("B7, when a row forms: the player's Japan", () => {
  it('a row raised for an operation waits for its war: the 65th Brigade does not come with America at peace, is not struck for waiting, and comes the month the war has begun', () => {
    const g = war();
    months(g, 29, 40);
    expect(row(g, BRIGADE)).toBeUndefined();
    expect(g.flags[`sent:${index(BRIGADE)}`]).toBeUndefined();
    expect(postedFor(g, 'JAP', rows[index(BRIGADE)], g.byName.Lingayen)).toMatchObject({ waits: true, at: null });
    setWar(g, 'JAP', 'USA');
    month(g, 41);
    expect(row(g, BRIGADE)).toBeDefined();
  });

  it('a row that names its war keeps it: the paratroops for Palembang form when the war with Britain has begun, though Phnom Penh is France\'s and France at peace', () => {
    const g = war();
    expect(rows[index(RAIDERS)].slice(0, 6)).toEqual([29, 'JAP', RAIDERS, 'inf', 1, 'Phnom Penh']);
    expect(rows[index(RAIDERS)][6]).toMatchObject({ war: ['JAP', 'UK'], after: 1 });
    months(g, 29, 31);
    expect(row(g, RAIDERS)).toBeUndefined();
    setWar(g, 'JAP', 'UK');
    months(g, 32, 33);
    expect([g.byName['Phnom Penh'].ctrl, homeland(g.regions[row(g, RAIDERS)!.reg], 'JAP')]).toEqual(['FRA', true]);
  });

  it("a row with no war of its own, posted on a neutral's ground, waits for war with its holder or for the ground: the Kawaguchi Detachment at Cam Ranh", () => {
    const g = war();
    expect(rows[index(KAWAGUCHI)][6].war).toBeUndefined();
    months(g, 27, 30);
    expect(row(g, KAWAGUCHI)).toBeUndefined();
    // A war with somebody else is not its war.
    setWar(g, 'JAP', 'UK');
    month(g, 31);
    expect(row(g, KAWAGUCHI)).toBeUndefined();
    // Cam Ranh in Japan's hands (the demand on Vichy France makes it so): it forms there, on its quay.
    lose(g, 'Cam Ranh', 'JAP', 'Saigon');
    month(g, 32);
    expect(at(g, KAWAGUCHI)).toBe('Cam Ranh');
  });
});

describe('B8, a row that would take corps of the player\'s is an offer', () => {
  it("nothing is taken: the 21st Army's 5th Division is offered in October 1939, where it would form and what it would take", () => {
    const g = war();
    const src = row(g, FIFTH_OF)!;
    months(g, 1, 2);
    expect(row(g, FIFTH)).toBeUndefined();
    expect(row(g, FIFTH_OF)).toBe(src);
    const o = offer(g, FIFTH)!;
    expect(o).toMatchObject({ id: index(FIFTH), name: FIFTH, kind: 'inf', divs: 1, men: 13500, since: 2, region: g.byName.Taihoku.id, joins: null, why: null });
    expect(o.takes).toEqual([{ id: src.id, name: FIFTH_OF, kind: 'inf', region: src.reg, divs: 1, men: men(src) }]);
    expect(rowOffers(g)).toHaveLength(1);
  });

  // (Moved by the design director's ruling of 13:39Z, 25 November 2026, on the batch 1 readings, point 5 of the dated
  // rows: "the player's offer stays in the list for the same half year". As first built it stood until he answered,
  // and this test took it ten months on.)
  it("it stands through the rest of its row's half year, the month's offers one list, oldest first; then it lapses and he is told", () => {
    const g = war();
    months(g, 1, 8);
    expect(g.flags[`sent:${index(FIFTH)}`]).toBeUndefined();
    // Made in October 1939 (month 2), the month its row came due: open through April 1940 (month 8).
    expect(rowOffers(g).map((o) => [o.name, o.since, o.last])).toEqual([[FIFTH, 2, 8]]);
    // Taken in its last month, it forms as the rule always formed it: the corps it names struck off, the row in its place.
    expect(takeOffer(g, index(FIFTH))).toBeNull();
    expect(row(g, FIFTH_OF)).toBeUndefined();
    const c = row(g, FIFTH)!;
    expect([g.regions[c.reg].name, c.divs, c.str]).toEqual(['Taihoku', 1, 0.9]);
    expect(g.flags[`sent:${index(FIFTH)}`]).toBe(1);
    // It came once.
    months(g, 9, 12);
    expect(g.corps.filter((x) => x.nat === 'JAP' && x.name === FIFTH)).toHaveLength(1);
    expect(rowOffers(g).map((o) => [o.name, o.since, o.last])).toEqual([[SEVENTH, 12, 18]]);

    // Left unanswered, it lapses the month after its last, and he is told; his corps stays as it is.
    const h = war();
    months(h, 1, 8);
    const src = row(h, FIFTH_OF)!;
    const news = h.headlines.length;
    month(h, 9);
    expect(offer(h, FIFTH)).toBeUndefined();
    expect(h.headlines.slice(news).map((x) => x.text).filter((t) => /has lapsed/.test(t))).toEqual([`The offer to form ${FIFTH} has lapsed: its time has passed.`]);
    expect([h.flags[`sent:${index(FIFTH)}`], row(h, FIFTH_OF), row(h, FIFTH)]).toEqual([2, src, undefined]);
  });

  it('declined, the row never comes and his corps stays as it is', () => {
    const g = war();
    months(g, 1, 12);
    const src = row(g, SEVENTH_OF)!;
    expect(declineOffer(g, index(SEVENTH))).toBe(true);
    expect(declineOffer(g, index(SEVENTH))).toBe(false);
    months(g, 13, 20);
    expect(row(g, SEVENTH)).toBeUndefined();
    expect(row(g, SEVENTH_OF)).toBe(src);
    expect(offer(g, SEVENTH)).toBeUndefined();
    expect(g.flags[`sent:${index(SEVENTH)}`]).toBe(2);
  });

  // (The offers of October 1939 and August 1940 have lapsed by April 1941, their half years out: the ruling of 13:39Z,
  // 25 November 2026, point 5. As first built they stood in this list too.)
  it('take some, leave some: the two offers of April 1941, one taken and one left open', () => {
    const g = war();
    months(g, 1, 19);
    expect(rowOffers(g).map((o) => o.name)).toEqual([FORTY8, CHEKIANG]);
    expect(takeOffer(g, index(FORTY8))).toBeNull();
    expect([row(g, FORMOSA), at(g, FORTY8)]).toEqual([undefined, 'Formosa']);
    expect(rowOffers(g).map((o) => o.name)).toEqual([CHEKIANG]);
  });

  it('it lapses if a corps it needs is gone, and he is told', () => {
    const g = war();
    months(g, 1, 19);
    removeCorps(g, row(g, FORMOSA)!);
    // Asked for before the month turns, it is refused with the reason, and lapses.
    expect(offer(g, FORTY8)!.why).toMatch(/has lapsed: the corps it was to be formed from is no longer on the map/);
    month(g, 20);
    expect(offer(g, FORTY8)).toBeUndefined();
    expect(g.flags[`sent:${index(FORTY8)}`]).toBe(2);
    expect(g.headlines.some((h) => h.text === `The offer to form ${FORTY8} has lapsed: the corps it was to be formed from is no longer on the map.`)).toBe(true);
    months(g, 21, 24);
    expect(row(g, FORTY8)).toBeUndefined();
  });

  it('an offer forms by ruling B7 where its posting is not held: the 15th Army for Burma is offered at home once its war has begun', () => {
    const g = war();
    // The row of the free stops is posted at Raheng, in Thailand, and names its war (Japan with Britain).
    const burma = rows.find((r) => r[1] === 'JAP' && r[2] === BURMA && r[6].stops.includes(3))!;
    expect([burma[5], g.byName.Raheng.ctrl, burma[6].war]).toEqual(['Raheng', 'THA', ['JAP', 'UK']]);
    months(g, 28, 30);
    expect(offer(g, BURMA)).toBeUndefined();
    setWar(g, 'JAP', 'UK');
    months(g, 31, 32);
    const o = offer(g, BURMA)!;
    expect([homeland(g.regions[o.region], 'JAP'), o.why, o.takes.map((t) => t.name)]).toEqual([true, null, [BURMA_OF]]);
    expect(takeOffer(g, o.id)).toBeNull();
    const c = row(g, BURMA)!;
    expect([homeland(g.regions[c.reg], 'JAP'), c.divs, c.hold, row(g, BURMA_OF)]).toEqual([true, 2, false, undefined]);
  });

  it('and waits for its war as any row does: the Imperial Guards for Phnom Penh are not offered while France holds it at peace', () => {
    const g = war();
    months(g, 24, 27);
    expect(offer(g, GUARDS)).toBeUndefined();
    expect(row(g, 'Imperial Guards Division')).toBeDefined();
    lose(g, 'Phnom Penh', 'JAP', 'Saigon');
    month(g, 28);
    expect(offer(g, GUARDS)).toMatchObject({ region: g.byName['Phnom Penh'].id, why: null });
  });

  it('he answers in his own month, and a saved war keeps its offers', () => {
    const g = war();
    months(g, 1, 2);
    const id = index(FIFTH);
    g.phase = g.order.indexOf('GER');
    expect(whyNotAnswerOffers(g)).toBe('It is not your month.');
    expect(orderTakeFormation(g, id)).toBe('It is not your month.');
    expect(orderDeclineFormation(g, id)).toBe(false);
    g.phase = g.order.indexOf('JAP');
    const back = loadGame(saveGame(g))!;
    expect(formationOffers(back)).toEqual(formationOffers(g));
    expect(formationOffers(back).map((o) => o.name)).toEqual([FIFTH]);
    expect(orderTakeFormation(back, id)).toBeNull();
    expect(row(back, FIFTH)).toBeDefined();
    expect(orderTakeFormation(back, id)).toBe('There is no such offer.');
  });
});

describe("the computer's rows are as they were", () => {
  it("a computer's Japan is offered nothing: its row takes the corps it names in its month", () => {
    const g = war(null);
    months(g, 1, 2);
    expect([row(g, FIFTH_OF), at(g, FIFTH)]).toEqual([undefined, 'Taihoku']);
    expect(rowOffers(g)).toEqual([]);
    months(g, 3, 19);
    expect([row(g, FORMOSA), at(g, FORTY8)]).toEqual([undefined, 'Formosa']);
    expect(Object.keys(g.flags).filter((k) => k.startsWith('offer:'))).toEqual([]);
  });

  it("by the book the record's book stays the book: at stops 1 and 2 a computer's 65th Brigade comes on its date at Japan's nearest held port, with America at peace", () => {
    for (const stop of [1, 2]) {
      const g = war(null);
      setStop(g, stop, true);
      expect(postedFor(g, 'JAP', rows[index(BRIGADE)], g.byName.Lingayen), `stop ${stop}`).toBeNull();
      month(g, 29);
      const c = row(g, BRIGADE)!;
      // As before the ruling (the rider to ruling 2c): Formosa, the port Japan holds nearest Lingayen, wherever it lies.
      expect([g.regions[c.reg].name, g.regions[c.reg].port, g.regions[c.reg].ctrl, c.str], `stop ${stop}`).toEqual(['Formosa', true, 'JAP', 0.44]);
    }
    // Where no book dates the rows the one rule reads a computer's too: it waits for the war.
    const h = war(null);
    month(h, 29);
    expect(row(h, BRIGADE)).toBeUndefined();
  });
});

type Call = (...a: unknown[]) => any;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;

describe('the 3D game\'s calls: one list on the month card, take all or pick', () => {
  it("Japan's first offer is on October 1939's card; taken, it is formed; read back from the save and declined, it never comes", () => {
    TC().start({ human: 'JAP', view: 'grand', world: true });
    expect(TC().offers()).toEqual({ count: 0, head: '', note: '', can: false, why: '', rows: [] });
    const end = TC().endMonth() as { month: { offers: { count: number; head: string; rows: { id: number; name: string }[] } }; view: { turn: number } };
    expect(end.view.turn).toBe(2);
    // The month card carries the list.
    expect(end.month.offers).toMatchObject({ count: 1, head: '1 formation offered' });
    expect(end.month.offers.rows.map((r) => r.name)).toEqual([FIFTH]);
    const list = TC().offers() as { count: number; note: string; can: boolean; rows: { id: number; name: string; what: string; where: string; region: number; since: string; takes: { name: string; where: string; size: string; men: number }[]; line: string; can: boolean; why: string }[] };
    expect(list).toEqual(end.month.offers);
    // (The UI director's marks of 25 November 2026, 13:39Z, section 5: "an offer stands until you answer" was untrue, and
    // "the record" is our word; the player's is "history".)
    expect(list.note).toBe('It would be formed out of corps you have, as history formed it. It waits for your answer until its last month, or until a corps it needs is gone.');
    const r = list.rows[0];
    expect(r).toMatchObject({ id: index(FIFTH), name: FIFTH, what: 'Infantry, 13,500 men', where: 'Taihoku', since: 'Offered in October 1939', can: true, why: '' });
    expect(r.takes).toHaveLength(1);
    expect(r.takes[0].name).toBe(FIFTH_OF);
    expect(r.line).toBe(`Forms at Taihoku (infantry, 13,500 men), taking ${FIFTH_OF} at ${r.takes[0].where} (${r.takes[0].size}).`);
    const text = TC().save().text as string;

    const took = TC().offerTake('all') as { ok: boolean; taken: number[]; left: unknown[]; said: string; offers: { count: number }; view: { turn: number } };
    expect(took).toMatchObject({ ok: true, taken: [index(FIFTH)], left: [], said: `${FIFTH} is formed.` });
    expect([took.offers.count, took.view.turn]).toEqual([0, 2]);
    expect(TC().offerTake([index(FIFTH)])).toMatchObject({ ok: false, taken: [], left: [{ id: index(FIFTH), why: 'There is no such offer.' }] });

    // The saved war has the offer still open.
    expect(TC().load(text)).not.toBeNull();
    expect((TC().offers() as { count: number }).count).toBe(1);
    const no = TC().offerDecline([index(FIFTH)]) as { ok: boolean; declined: number[]; said: string; offers: { count: number } };
    expect(no).toMatchObject({ ok: true, declined: [index(FIFTH)], said: `${FIFTH} is not formed: your corps stay as they are.` });
    expect(no.offers.count).toBe(0);
    expect(TC().offerDecline('all')).toMatchObject({ ok: false, declined: [] });
  }, 900000);
});
