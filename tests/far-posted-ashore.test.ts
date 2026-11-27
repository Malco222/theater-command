// A posted division is not struck for a lost posting (rider b642 part 2 to ruling 234; the design director's of
// 15:52Z and 17:10Z of 19 November 2026).
//
// A row by date is a fact of the division's raising. Under the old rule a row whose posting was not its side's
// waited six months for the place and was then struck: in twenty-four measured wars Japan lost a hundred and ten
// rows that way, the 65th Brigade and the 21st Division's detachment for Luzon in every one. Beyond the old chart a
// division raised at home now comes ashore, on its date, at its country's nearest held port to the posting, and the
// staff posts it. A local levy keeps the old rule, and so does a row that takes corps already on the board: only
// what such a row adds from home comes ashore, and nobody is counted twice.
//
// Since the design director's ruling B7 of 25 November 2026 (design-director/pacific-japan-1125.md) that is the rule of
// the rows a book dates alone (stops 1 and 2, `ashoreFor`). Everywhere else one rule reads every country's rows, the
// player's too (`postedFor`; tests/far-dated-rows-b7.test.ts): a row posted on ground its side does not hold forms at
// its country's nearest held harbour in the same theatre, else at its capital, and a row raised for an operation
// waits for its war. Three tests here pinned what that ruling changed, and say so where they stand.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { allied, ashoreFor, createGame, declareWar, relocate, setWar, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { homeland } from '../src/campaign/middleeast';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';
import { mapDist2 } from '../src/campaign/wrap';
import setup from '../data/campaign/setup-1939.json';

const war = (human: string | null = null): Game => startOpen(createGame(5, human));
const row = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead);
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'JAP'); };
const dated = (name: string): any[] => ((setup as any).expeditions as any[]).find((r) => r[1] === 'JAP' && r[2] === name)!;
/** Japan's nearest held port to a place, as the map lies. */
const nearest = (g: Game, to: Region): Region => g.regions.filter((r) => r.port && r.ctrl === 'JAP').sort((a, b) => mapDist2(g, a.x, a.y, to.x, to.y) - mapDist2(g, b.x, b.y, to.x, to.y))[0];
/** A province changes hands for the test: whoever stood on it is sent off, and it is the taker's. */
const lose = (g: Game, name: string, to: string, away: string): Region => {
  const r = g.byName[name];
  for (const u of [...r.units]) relocate(g, u, g.byName[away]);
  r.ctrl = to;
  return r;
};

// (Canton's row where no book dates Japan's rows is the 38th Division's 229th Regiment with its headquarters, 0.3: ruling 234's notes of 22:05Z and 22:10Z of 20 November 2026.)
const CANTON = "229th Infantry Regiment (with the 38th Division's headquarters)", BRIGADE = '14th Army (65th Brigade)', BODY = '56th Division', GROUP = 'Sakaguchi Detachment (56th Mixed Infantry Group)', EIGHTEENTH = '18th Division';

describe('a posted division comes ashore where its posting is lost (rider b642 part 2)', () => {
  it('the 38th Division\'s headquarters regiment lands at Canton while Canton is Japan\'s, as the division always did', () => {
    const g = war();
    month(g, 2);
    expect(g.regions[row(g, CANTON)!.reg].name).toBe('Canton');
  });

  it('with Canton lost on its date it comes ashore at Japan\'s nearest held port to Canton, whole and not held', () => {
    const g = war();
    const canton = lose(g, 'Canton', 'CHN', 'Hainan');
    const port = nearest(g, canton);
    expect([port.ctrl, port.port, port.id !== canton.id]).toEqual(['JAP', true, true]);
    month(g, 1);
    expect(row(g, CANTON)).toBeUndefined();
    month(g, 2);
    const c = row(g, CANTON)!;
    expect([g.regions[c.reg].name, c.hold, !!c.stay, c.str, c.divs]).toEqual([port.name, false, false, 0.3, 1]);
    // It came once: the months after bring no second one, and Canton retaken brings nothing more.
    canton.ctrl = 'JAP';
    month(g, 3);
    expect(g.corps.filter((x) => x.nat === 'JAP' && x.name === CANTON)).toHaveLength(1);
  });

  // (Changed by ruling B7. This test read "a posting a neutral holds is the same: the 65th Brigade for Luzon comes
  // ashore on its date with America at peace", at Japan's nearest held port. A row the record raised for an operation
  // now forms when that war has begun, never on its date; and with no harbour of Japan's in the Philippines it forms
  // at home. By the book, stops 1 and 2, the brigade still comes on its date, for the dated landings that sail it.)
  it('a posting a neutral holds: the 65th Brigade for Luzon waits for the war with America, and then forms at home while Japan holds no harbour in the Philippines', () => {
    const g = war();
    const lingayen = g.byName['Lingayen'];
    expect(lingayen.ctrl).toBe('USA');
    month(g, 29);
    expect(row(g, BRIGADE)).toBeUndefined();
    // No half year runs against a row that waits for its war.
    month(g, 40);
    expect(row(g, BRIGADE)).toBeUndefined();
    setWar(g, 'JAP', 'USA');
    month(g, 41);
    const c = row(g, BRIGADE)!;
    // (6,651 men, 0.44 of a division: the director's order for Luzon, (3), of 21 November 2026.)
    expect([homeland(g.regions[c.reg], 'JAP'), g.regions[c.reg].ctrl, c.hold, c.str, c.full]).toEqual([true, 'JAP', false, 0.44, 0.44]);
    // By the book it comes on its date as it did, at a port of Japan's, with America at peace.
    const b = war();
    setStop(b, 1, true);
    month(b, 29);
    const d = row(b, BRIGADE)!;
    expect([b.regions[d.reg].port, b.regions[d.reg].ctrl, d.hold, d.str]).toEqual([true, 'JAP', false, 0.44]);
  });

  it('a row that takes corps already on the board keeps the old rule: the 18th Division is not brought to Rangoon\'s coast', () => {
    const g = war();
    setWar(g, 'JAP', 'UK');
    const rangoon = g.byName['Rangoon'];
    // (The record's 18th came to Rangoon without the Kawaguchi Detachment: its row is the 25th Army's 18th Division alone.)
    expect(dated(EIGHTEENTH)[6].from).toEqual(['25th Army (18th Division)']);
    expect(ashoreFor(g, 'JAP', dated(EIGHTEENTH), rangoon)).toBeNull();
    month(g, 32);
    expect(row(g, EIGHTEENTH)).toBeUndefined();
    // Its men stay where they stand, under their own names.
    expect(row(g, '25th Army (18th Division)')).toBeDefined();
    month(g, 39);
    expect(row(g, EIGHTEENTH)).toBeUndefined();
    expect(row(g, '25th Army (18th Division)')).toBeDefined();
  });

  it('only what such a row adds comes ashore: the 56th Division\'s body from home, and the Sakaguchi Detachment stays as it is', () => {
    const g = war();
    setWar(g, 'JAP', 'UK');
    const rangoon = g.byName['Rangoon'];
    expect([dated(BODY)[6].adds, dated(BODY)[6].from]).toEqual([0.67, [GROUP]]);
    month(g, 31);
    const body = row(g, BODY)!, group = row(g, GROUP)!;
    const at = g.regions[body.reg];
    expect([at.port, at.ctrl, at.id !== rangoon.id, body.hold, body.str, body.full, body.divs]).toEqual([true, 'JAP', true, false, 0.67, 0.67, 1]);
    // (The detachment at its record weight, about 5,000 men: far-shores.json, the design director's of 21 November 2026.)
    expect([group.str, group.full, g.regions[group.reg].name]).toEqual([0.33, 0.33, 'Palau']);
    // One division between them, as the record raised.
    expect(body.str + group.str).toBe(1);
  });

  it('with Rangoon Japan\'s on the date the 56th lands there whole and takes the detachment, as before', () => {
    const g = war();
    setWar(g, 'JAP', 'UK');
    lose(g, 'Rangoon', 'JAP', 'Calcutta');
    month(g, 31);
    const c = row(g, BODY)!;
    expect([g.regions[c.reg].name, c.str, c.full]).toEqual(['Rangoon', 1, undefined]);
    expect(row(g, GROUP)).toBeUndefined();
  });

  it('who is not brought ashore: a local levy, a fort, a row that joins a standing corps, a row made of a defeat, and anybody on the old chart', () => {
    const g = war();
    const canton = lose(g, 'Canton', 'CHN', 'Hainan');
    const post = (o: any, kind = 'inf'): any[] => [2, 'JAP', 'Test row', kind, 1, 'Canton', o];
    expect(ashoreFor(g, 'JAP', post({}), canton)?.at.name).toBe(nearest(g, canton).name);
    expect(ashoreFor(g, 'JAP', post({}), canton)?.adds).toBeNull();
    expect(ashoreFor(g, 'JAP', post({ levy: 1 }), canton)).toBeNull();
    expect(ashoreFor(g, 'JAP', post({}, 'fort'), canton)).toBeNull();
    expect(ashoreFor(g, 'JAP', post({ joins: '21st Army (Canton)' }), canton)).toBeNull();
    expect(ashoreFor(g, 'JAP', post({ join: 1 }), canton)).toBeNull();
    expect(ashoreFor(g, 'JAP', post({ ifLost: ['Canton'] }), canton)).toBeNull();
    expect(ashoreFor(g, 'JAP', post({ from: ['21st Army (Canton)'] }), canton)).toBeNull();
    expect(ashoreFor(g, 'JAP', post({ from: 'China' }), canton)).toBeNull();
    expect(ashoreFor(g, 'JAP', post({ from: ['21st Army (Canton)'], adds: 0.5 }), canton)?.adds).toBe(0.5);
    // A row that came overland from a country of its own is not its carrier's to land from the sea.
    expect(ashoreFor(g, 'JAP', post({ overland: 1 }), canton)).toBeNull();
    // A row with no home but one ground comes ashore only at a port of that ground: here Japan's own, not a port she holds in China.
    expect(nearest(g, canton).owner).not.toBe('JAP');
    expect(ashoreFor(g, 'JAP', post({ shore: 'JAP' }), canton)?.at.owner).toBe('JAP');
    expect(ashoreFor(g, 'JAP', post({ shore: 'SWI' }), canton)).toBeNull();
    // A posting that is its side's is no lost posting, and a place the row was already sent on from is not read.
    canton.ctrl = 'JAP';
    expect(ashoreFor(g, 'JAP', post({}), canton)).toBeNull();
    canton.ctrl = 'CHN';
    expect(ashoreFor(g, 'JAP', post({}), g.byName['Tokyo'])).toBeNull();
    // The old chart: Germany's rows for Africa wait for Tripoli as they did.
    setWar(g, 'GER', 'UK');
    const tripoli = g.byName['Tripoli'];
    tripoli.ctrl = 'UK';
    expect(ashoreFor(g, 'GER', [18, 'GER', 'Test light division', 'arm', 1, 'Tripoli', {}], tripoli)).toBeNull();
  });

  it("a detachment cut from a corps on the board comes only once its sister row has made the cut (the director, 17:23Z)", () => {
    const WHOLE = 'Tientsin-Pukow railway garrison (21st Division)', REST = 'Tientsin-Pukow railway garrison (21st Division, less its Bataan detachment)', PART = '21st Division detachment (for Bataan)';
    // (Since (g) of 21 November 2026 the detachment names each row that may make its cut: this one at stops 3 to 5, the 17th Division's taking over Hsuchow at the book's stops, far-21st-division.)
    expect(dated(PART)[6].cut).toEqual([REST, '13th Army (17th Division, Hsuchow)']);
    // Hsuchow held: the garrison is cut to 0.65 and the quarter for Bataan comes, at home where Lingayen is not Japan's.
    // (Changed by ruling B7: the detachment is a row for Luzon with no war of its own, so it waits for the war with
    // America, and with no harbour of Japan's in the Philippines it forms at home, not at the nearest port anywhere.
    // The three wars here are at war with America, as the record's was by January 1942.)
    const g = war();
    expect(g.regions[row(g, WHOLE)!.reg].name).toBe('Hsuchow');
    month(g, 30);
    expect([row(g, REST)!.str, row(g, PART), row(g, WHOLE)]).toEqual([0.65, undefined, undefined]);
    setWar(g, 'JAP', 'USA');
    month(g, 31);
    const rest = row(g, REST)!, part = row(g, PART)!;
    expect([rest.str, part.str, g.regions[part.reg].ctrl, homeland(g.regions[part.reg], 'JAP'), part.hold]).toEqual([0.65, 0.25, 'JAP', true, false]);
    // Hsuchow lost on the date: the garrison stays whole where it stands, and no quarter is counted twice.
    const h = war();
    setWar(h, 'JAP', 'USA');
    lose(h, 'Hsuchow', 'CHN', 'Tsinan');
    for (const t of [30, 33, 37]) {
      month(h, t);
      expect(row(h, PART), `turn ${t}`).toBeUndefined();
      expect(row(h, REST), `turn ${t}`).toBeUndefined();
      expect(row(h, WHOLE), `turn ${t}`).toBeDefined();
    }
    // Hsuchow retaken inside the half year: the cut is made and the detachment comes then.
    const k = war();
    setWar(k, 'JAP', 'USA');
    const hsuchow = lose(k, 'Hsuchow', 'CHN', 'Tsinan');
    month(k, 30);
    expect(row(k, PART)).toBeUndefined();
    hsuchow.ctrl = 'JAP';
    month(k, 32);
    expect([row(k, REST)?.str, row(k, PART)?.str, row(k, WHOLE)]).toEqual([0.65, 0.25, undefined]);
  });

  it("the historian's marks: Java's militia is a levy, China's armies for Burma came overland, and the KNIL comes ashore only in the Indies", () => {
    const all = (setup as any).expeditions as any[];
    const words = (name: string): any => all.find((r) => r[2] === name)![6];
    expect(words("KNIL mobilisation (Java's militia and the Landstorm)").levy).toBe(1);
    for (const nm of ['Chinese 6th Army (49th, 55th and 93rd Divisions)', 'Chinese 5th Army (200th, 22nd and 96th Divisions)', 'Chinese 66th Army (38th, 28th and 29th Divisions)', 'Chinese 14th and 50th Divisions']) expect(words(nm).overland, nm).toBe(1);
    expect(words('Outer Islands reinforcement (Tarakan, Menado, Koepang)').shore).toBe('NED');
    // No row of Japan's is a levy: every one was raised in her home depots.
    expect(all.filter((r) => r[1] === 'JAP' && (r[6]?.levy || r[6]?.overland || r[6]?.shore))).toEqual([]);
    // With Tarakan lost and the Indies in the war beside Britain, the reinforcement lands at an Indies port; Java's militia and China's armies are brought nowhere.
    const g = war();
    setWar(g, 'JAP', 'UK');
    declareWar(g, 'GER', 'NED');
    setWar(g, 'JAP', 'NED');
    expect(allied(g, 'UK', 'NED')).toBe(true);
    const tarakan = lose(g, 'Tarakan', 'JAP', 'Batavia');
    const there = ashoreFor(g, 'UK', all.find((r) => r[2] === 'Outer Islands reinforcement (Tarakan, Menado, Koepang)')!, tarakan);
    expect([there?.at.owner, there?.at.port, there?.at.ctrl]).toEqual(['NED', true, 'NED']);
    const bandoeng = lose(g, 'Bandoeng', 'JAP', 'Batavia');
    expect(ashoreFor(g, 'UK', all.find((r) => r[2] === "KNIL mobilisation (Java's militia and the Landstorm)")!, bandoeng)).toBeNull();
    const lashio = lose(g, 'Lashio', 'JAP', 'Calcutta');
    expect(ashoreFor(g, 'UK', all.find((r) => r[2] === 'Chinese 6th Army (49th, 55th and 93rd Divisions)')!, lashio)).toBeNull();
    // An Indian division raised in India at large is no levy of its posting: with Imphal lost it forms at Britain's nearest held port.
    const imphal = lose(g, 'Imphal', 'JAP', 'Calcutta');
    const indian = ashoreFor(g, 'UK', all.find((r) => r[2] === 'IV Corps (23rd Indian Division)')!, imphal);
    expect([indian?.at.ctrl, indian?.at.port, indian?.adds]).toEqual(['UK', true, null]);
  });

  // (Changed by ruling B7. This test read "the player's Japan is as it was: such a row forms at home and shipping it
  // is his business", and the brigade stood at Tokyo in January 1942 with America at peace. The player's rows are
  // under the one rule now: tests/far-dated-rows-b7.test.ts.)
  it("the player's Japan is under the same rule: the brigade waits for its war, and with no harbour in the Philippines it forms at home, his to ship", () => {
    const g = war('JAP');
    month(g, 29);
    expect(row(g, BRIGADE)).toBeUndefined();
    setWar(g, 'JAP', 'USA');
    month(g, 30);
    const c = row(g, BRIGADE)!;
    expect(homeland(g.regions[c.reg], 'JAP')).toBe(true);
  });
});
