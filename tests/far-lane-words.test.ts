// What each of Japan's lanes runs on, and the oilfields overseas on the cards (rulings 194 and 195(c); the UI
// director's sheets 24, boxes 1 and 2, and 25, box 1: builder-brief-round-1.md).
//
// The fault it mends: the Stores page listed Japan's five lanes with what each brought, and said nothing of what
// each ran on, so a player could not read that Korea's lane lives while Pusan is Japan's, or that the freeze stops
// the southern lane until Singapore is taken. The Oil page listed the southern oil lane and not the fields that feed
// it, and an oilfield overseas said "It yields 14 oil a month" on its card whoever held it and wherever its oil went.
//
// The rule: a lane's row names its key (far-books.json 'key': the place held or the seller), and its last sentence
// is read from the same checks that stop it (laneKey). "Yours" to the nation the lane feeds, by name to everyone
// else. Words only: no figure moves, and Europe's rows, which name no key, keep their words. Every state here is set
// by hand on the first day's chart; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { createGame, declareWar } from '../src/campaign/game';
import { RES, fieldCard, laneKey, stores } from '../src/campaign/resources';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const grand = (): Game => startOpen(createGame(3, null));
const SOUTH = ['Palembang', 'Balikpapan', 'Medan', 'Soerabaja', 'Brunei', 'Yenangyaung', 'Tarakan'];
const take = (g: Game, names: string[]): void => { for (const nm of names) g.byName[nm].ctrl = 'JAP'; };
const key = (g: Game, id: string, me: string | null = 'JAP'): string => laneKey(g, id, me);
/** The freeze, in August 1941. */
const freeze = (g: Game): void => { g.flags['pac:freeze'] = 24; };

describe("what Japan's lanes run on (ruling 195(c); sheet 25, box 1)", () => {
  it('five rows name a key, and no other lane of any nation does', () => {
    const keyed = RES.lanes.filter((l) => (l as { key?: string }).key !== undefined).map((l) => [l.id, (l as { key?: string }).key]);
    expect(keyed).toEqual([['america-japan', 'America'], ['korea-japan', 'Pusan'], ['formosa-japan', 'Formosa'], ['rice-japan', 'Saigon'], ['south-japan', 'Singapore']]);
    const g = grand();
    for (const l of RES.lanes) if (!keyed.some(([id]) => id === l.id)) { expect(key(g, l.id, l.to), l.id).toBe(''); expect(key(g, l.id, null), l.id).toBe(''); }
    expect(key(g, 'south-oil')).toBe('');
    expect(key(g, 'no such lane')).toBe('');
  });

  it('a running lane says what it runs on: "yours" to Japan, by its name to everyone else', () => {
    const g = grand();
    expect(key(g, 'korea-japan')).toBe('It runs while Pusan is yours.');
    expect(key(g, 'korea-japan', 'USA')).toBe("It runs while Pusan is in Japan's hands.");
    expect(key(g, 'korea-japan', null)).toBe("It runs while Pusan is in Japan's hands.");
    expect(key(g, 'formosa-japan')).toBe('It runs while Formosa is yours.');
    expect(key(g, 'rice-japan')).toBe('It runs while the holder of Saigon is no enemy of yours.');
    expect(key(g, 'rice-japan', 'UK')).toBe("It runs while the holder of Saigon is no enemy of Japan's.");
    expect(key(g, 'south-japan')).toBe('It runs while the holder of Singapore is no enemy of yours.');
    expect(key(g, 'america-japan')).toBe('It runs while America sells to you.');
    expect(key(g, 'america-japan', 'USA')).toBe('It runs while America sells to Japan.');
  });

  it('the freeze is named by its month: it stops two lanes, one of them for good, and the rice lane says it is not touched', () => {
    const g = grand();
    freeze(g);
    expect(key(g, 'america-japan')).toBe('Stopped by the freeze of August 1941. It does not run again.');
    expect(key(g, 'south-japan')).toBe('Stopped by the freeze of August 1941. It runs again when Singapore is yours.');
    expect(key(g, 'south-japan', 'UK')).toBe("Stopped by the freeze of August 1941. It runs again when Singapore is in Japan's hands.");
    expect(key(g, 'rice-japan')).toBe('It runs while the holder of Saigon is no enemy of yours. The freeze does not touch it.');
    expect(key(g, 'korea-japan')).toBe('It runs while Pusan is yours.');
    expect(key(g, 'formosa-japan')).toBe('It runs while Formosa is yours.');
    // The sentence and the lane's state are read from one check.
    const state = (id: string): string => stores(g, 'JAP')!.lanes.find((l) => l.id === id)!.state;
    expect([state('america-japan'), state('south-japan'), state('rice-japan'), state('korea-japan')]).toEqual(['cut', 'cut', 'open', 'open']);
    // Singapore taken: the southern lane sails again, on the place and no longer on a peace.
    take(g, ['Singapore']);
    expect(state('south-japan')).toBe('open');
    expect(key(g, 'south-japan')).toBe('It runs while Singapore is yours.');
    expect(key(g, 'america-japan')).toBe('Stopped by the freeze of August 1941. It does not run again.');
  });

  it('a lane stopped another way says what stopped it and what would start it again, or that nothing will', () => {
    const g = grand();
    // Its own place in another's hands with no war on: not running.
    const pusan = g.byName.Pusan;
    pusan.ctrl = 'SOV';
    expect(key(g, 'korea-japan')).toBe('The Soviet Union holds Pusan. It runs again when Pusan is yours.');
    expect(key(g, 'korea-japan', 'SOV')).toBe("The Soviet Union holds Pusan. It runs again when Pusan is in Japan's hands.");
    // And with a war on: cut, in the row's own words.
    declareWar(g, 'SOV', 'JAP');
    expect(key(g, 'korea-japan')).toBe('The Soviet Union holds Pusan and the road through it is shut. It runs again when Pusan is yours.');
    pusan.ctrl = 'JAP';
    // A narrows an enemy holds. Before the freeze a peace would open it; after it, only Singapore in Japan's own hands.
    declareWar(g, 'JAP', 'UK');
    expect(key(g, 'south-japan')).toBe('The United Kingdom holds Singapore and lets nothing past. It runs again when the holder of Singapore is no enemy of yours.');
    freeze(g);
    expect(key(g, 'south-japan')).toBe('The United Kingdom holds Singapore and lets nothing past. It runs again when Singapore is yours.');
    // The seller at war: the trade is over.
    delete g.flags['pac:freeze'];
    declareWar(g, 'JAP', 'USA');
    expect(key(g, 'america-japan')).toBe('Japan is at war with the United States, which sold the cargoes. It does not run again.');
  });
});

describe('the oilfields overseas on the Oil page and on their own cards (ruling 194; sheet 24, boxes 1 and 2)', () => {
  it('the fields stand directly above the lane that carries their oil, with no figure of their own', () => {
    const g = grand();
    const oil = () => stores(g, 'JAP')!.oil;
    // Japan holds none: no row.
    expect(oil().income.some((l) => l.bySea)).toBe(false);
    const before = oil().made;
    take(g, SOUTH);
    const lines = oil().income, i = lines.findIndex((l) => l.key === 'overseas:south-oil');
    expect(lines[i]).toEqual({ key: 'overseas:south-oil', label: 'Oilfields overseas', amount: 0, count: 7, bySea: true, note: '7 fields, feeding the southern oil lane · 19.8 a month at the wells' });
    expect(lines[i + 1]).toMatchObject({ key: 'lane:south-oil', lane: 'south-oil' });
    expect(lines[i + 1].amount).toBeCloseTo(19.8, 9);
    // The oil is counted once, on the lane.
    expect(oil().made).toBeCloseTo(before + 19.8, 9);
    expect(lines.filter((l) => l.bySea)).toHaveLength(1);
    // One field: the small line counts one.
    for (const nm of SOUTH.slice(1)) g.byName[nm].ctrl = g.byName[nm].owner;
    expect(oil().income.find((l) => l.bySea)!.note).toBe('1 field, feeding the southern oil lane · 8.4 a month at the wells');
    // No other power's book has such a row.
    for (const nat of RES.nations) if (nat !== 'JAP') expect(stores(g, nat)!.oil.income.some((l) => l.bySea), nat).toBe(false);
  });

  it("a field's card says what it yields at the wells, where its oil goes, and what it would be in the reader's hands", () => {
    const g = grand();
    const palembang = g.byName.Palembang;
    // In its owner's hands: the Netherlands keeps no stocks, so its oil reaches no one at home.
    expect(fieldCard(g, palembang, 'JAP')).toEqual({ wells: '14 oil a month at the wells.', goes: 'Its oil reaches no one at home.' });
    expect(fieldCard(g, palembang, null)).toEqual({ wells: '14 oil a month at the wells.', goes: 'Its oil reaches no one at home.' });
    // To an enemy the card says what the field would be in his hands: Japan's lane would carry it, America has none.
    declareWar(g, 'JAP', 'NED');
    expect(fieldCard(g, palembang, 'JAP')).toEqual({ wells: '14 oil a month at the wells.', goes: 'Its oil reaches no one at home.', yours: 'its oil would go home by sea, on the southern oil lane.' });
    // A field at home keeps its words.
    expect(fieldCard(g, g.byName.Akita, 'JAP')).toBeNull();
  });

  it("in Japan's hands it yields six tenths, and its line ends with the lane's own state", () => {
    const g = grand();
    const palembang = g.byName.Palembang;
    take(g, ['Palembang']);
    expect(fieldCard(g, palembang, 'JAP')).toEqual({ wells: '8.4 oil a month at the wells, six tenths of what its owner drew.', goes: 'Its oil goes home by sea, on the southern oil lane: 100% getting through.' });
    // Japan's enemy reads who it feeds, and that taking it would fill no tank of his own.
    declareWar(g, 'JAP', 'USA');
    const seen = fieldCard(g, palembang, 'USA')!;
    expect(seen.wells).toBe('8.4 oil a month at the wells, six tenths of what its owner drew.');
    expect(seen.goes).toMatch(/^Its oil goes home to Japan by sea, on the southern oil lane(: \d+% getting through|, which is cut|, which is not running)\.$/);
    expect(seen.yours).toBe('its oil would reach no one at home.');
    // A neutral reads the first two lines and no third.
    expect(fieldCard(g, palembang, 'SWE')!.yours).toBeUndefined();
  });
});
