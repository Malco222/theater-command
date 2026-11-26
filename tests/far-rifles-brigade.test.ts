// China's rifles: the brigade's word (the UI director, 12:48Z of 26 November 2026, on the support teams' rifles refusal,
// line 9): "the noun follows the kind. this brigade for machine-gun and mortar brigades, this corps for the rest: 'Not
// enough rifles: this brigade takes {2,100} and {800} are in store. ...'" The support teams (kinds 'mg' and 'mor',
// teams.ts, behind flags.teams) are refused by the store in the same sentence as any corps (rifles.ts noRifles); the
// noun is taken from the teams' own label. The page reads the offer's `why` (arsenal.ts offers) and the 3D game's
// arsenal passes the same `why` on (campaign-bridge.ts arsenal): both are asserted here, whole.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { offers } from '../src/campaign/arsenal';
import { levyMen } from '../src/campaign/manpower';
import { foldCorps } from '../src/campaign/fold';
import { addCorps, createGame } from '../src/campaign/game';
import { noRifles, riflesView } from '../src/campaign/rifles';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { setTeams } from '../src/campaign/teams';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

const PER = 21300;
type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
interface BridgeOffer { key: string; why: string | null; note: string }

/** China played, in its own month, the support teams on, men enough, and 21,000 rifles in store. */
function china(): Game {
  const g = startOpen(createGame(3, 'CHN'));
  let guard = 0;
  while (!humanUp(g) && guard++ < 200) stepAI(g);
  beginHuman(g);
  setTeams(g, true);
  levyMen(g, 'CHN', 30);
  g.flags['sa:CHN'] = 21000 / PER;
  return g;
}

describe("the brigade's word", () => {
  it('a machine-gun and a mortar brigade are refused as "this brigade", an infantry corps as "this corps"', () => {
    const g = china();
    const at = g.nations.CHN.capital;
    const offer = (key: string) => offers(g, 'CHN', at).find((o) => o.key === key)!;
    const inflow = riflesView(g, 'CHN')!.inflow.total;
    const hundred = (Math.round(inflow / 100) * 100).toLocaleString('en-US');
    // (On the first day about 8,300 a month: a brigade's 21,300 against 21,000 in store waits one month, a corps' 63,900 six.)
    expect(hundred).toBe('8,300');
    expect(offer('corps:mg').rifles).toBe(PER);
    expect(offer('corps:mor').rifles).toBe(PER);
    const brigade = 'Not enough rifles: this brigade takes 21,300 and 21,000 are in store. 8,300 come in each month, so it can be ordered in one month.';
    expect(offer('corps:mg').why).toBe(brigade);
    expect(offer('corps:mor').why).toBe(brigade);
    expect(offer('corps:inf').why).toBe('Not enough rifles: this corps takes 63,900 and 21,000 are in store. 8,300 come in each month, so it can be ordered in six months.');
    expect(offer('corps:mtn').why).toBe('Not enough rifles: this corps takes 42,600 and 21,000 are in store. 8,300 come in each month, so it can be ordered in three months.');
    // The rule's own word, by kind; without a kind it is the corps' sentence as before.
    expect(noRifles(g, 'CHN', 1, 'mg')).toBe(brigade);
    expect(noRifles(g, 'CHN', 1, 'mor')).toBe(brigade);
    expect(noRifles(g, 'CHN', 1, 'inf')).toBe('Not enough rifles: this corps takes 21,300 and 21,000 are in store. 8,300 come in each month, so it can be ordered in one month.');
    expect(noRifles(g, 'CHN', 1)).toBe(noRifles(g, 'CHN', 1, 'inf'));
    // Every other refusal of the store on this page still says "this corps".
    for (const o of offers(g, 'CHN', at)) {
      if (!o.why?.startsWith('Not enough rifles')) continue;
      expect(o.why, o.key).toMatch(o.key === 'corps:mg' || o.key === 'corps:mor' ? /^Not enough rifles: this brigade takes / : /^Not enough rifles: this corps takes /);
    }
  });

  it('a cadre of a machine-gun brigade, folded and waiting, is refused as "this brigade" too', () => {
    const g = china();
    const capName = g.regions[g.nations.CHN.capital].name;
    const ka = addCorps(g, 'CHN', 'Kept brigade', 'mg', 1, capName, {}), kc = addCorps(g, 'CHN', 'Folded brigade', 'mg', 1, capName, {});
    ka.str = kc.str = 0.2;
    // (Both armed to their strength, so the fold sends nothing to the store and it stays at 21,000.)
    g.flags[`rfa:${ka.id}`] = 0.2;
    g.flags[`rfa:${kc.id}`] = 0.2;
    expect(foldCorps(g, ka, kc)).toBe(true);
    const o = offers(g, 'CHN', g.nations.CHN.capital).find((x) => x.key === `cadre:${kc.id}`)!;
    expect(o.why).toBe('Not enough rifles: this brigade takes 21,300 and 21,000 are in store. 8,300 come in each month, so it can be ordered in one month.');
  });

  it('through the bridge: the 3D game\'s arsenal gives the same sentences, whole', () => {
    const g = china();
    TC().load(saveGame(g));
    const a = TC().arsenal(g.nations.CHN.capital) as { offers: BridgeOffer[] };
    const why = (key: string): string | null => a.offers.find((o) => o.key === key)!.why;
    const brigade = 'Not enough rifles: this brigade takes 21,300 and 21,000 are in store. 8,300 come in each month, so it can be ordered in one month.';
    expect(why('corps:mg')).toBe(brigade);
    expect(why('corps:mor')).toBe(brigade);
    expect(why('corps:inf')).toBe('Not enough rifles: this corps takes 63,900 and 21,000 are in store. 8,300 come in each month, so it can be ordered in six months.');
    // (The card line beside it, for the UI director: what the order takes, as the 3D game's card says it.)
    expect(a.offers.find((o) => o.key === 'corps:mg')!.note).toMatch(/Takes .* and 21,300 rifles\.$/);
  });
});

// The UI director's sixth read (26 November 2026, 18:55Z), point 2: "the noun follows the kind" covers every sentence
// that names this unit or this order, so a team reads "brigade" in the cadre's blurb, the cadre's note (the cadre's own
// kind) and the fold's news (the kept unit's kind), all by rifles.ts orderNoun. With the teams off no brigade exists,
// so every line reads as before.
describe("the sixth read, point 2: the cadre's offer and the fold's news follow the kind", () => {
  const BLURB = (noun: string): string => `The cadre of a ${noun} whose men were folded into another: its staff, its name, its record and its honours. Raised again it is the same ${noun} filled with new men, and it keeps 75% of what it knew.`;
  /** Two worn units of this kind at China's capital, both armed to their strength (the fold sends nothing to the store), folded through the 3D game's own action. */
  const folded = (kind: string, teams: boolean) => {
    const g = china();
    if (!teams) setTeams(g, false);
    const capName = g.regions[g.nations.CHN.capital].name;
    const ka = addCorps(g, 'CHN', 'Kept unit', kind as never, 1, capName, {}), kc = addCorps(g, 'CHN', 'Folded unit', kind as never, 1, capName, {});
    ka.str = kc.str = 0.2;
    g.flags[`rfa:${ka.id}`] = 0.2;
    g.flags[`rfa:${kc.id}`] = 0.2;
    TC().load(saveGame(g));
    const h0 = g.headlines.length;
    const r = TC().act(`fold:${ka.id}:${kc.id}`) as { ok: boolean };
    expect(r.ok).toBe(true);
    const after = loadGame((TC().save() as { text: string }).text)!;
    const a = TC().arsenal(after.nations.CHN.capital) as { offers: (BridgeOffer & { blurb: string })[] };
    const cadre = a.offers.find((o) => o.key === `cadre:${kc.id}`)!;
    const page = offers(after, 'CHN', after.nations.CHN.capital).find((o) => o.key === `cadre:${kc.id}`)!;
    return { news: after.headlines.slice(h0).map((h) => h.text), cadre, page, capName };
  };

  it('a machine-gun brigade folded into another: the news, the blurb and the note say "brigade", whole', () => {
    const { news, cadre, page, capName } = folded('mg', true);
    expect(capName).toBe('Chungking');
    expect(news).toEqual(['Folded unit is folded into Kept unit at Chungking: about 27,000 men in one brigade. Its cadre has gone home and can be raised again from the arsenal.']);
    expect(page.blurb).toBe(BLURB('brigade'));
    expect(page.note).toBe('Forms up at Chungking, for 50% of the price of a new brigade.');
    expect(cadre.blurb).toBe(BLURB('brigade'));
    expect(cadre.note).toBe("Forms up at Chungking, for 50% of the price of a new brigade. Takes one division's worth of men and 21,300 rifles.");
  });

  it('a mortar brigade the same', () => {
    const { news, cadre } = folded('mor', true);
    expect(news).toEqual(['Folded unit is folded into Kept unit at Chungking: about 27,000 men in one brigade. Its cadre has gone home and can be raised again from the arsenal.']);
    expect(cadre.blurb).toBe(BLURB('brigade'));
    expect(cadre.note).toBe("Forms up at Chungking, for 50% of the price of a new brigade. Takes one division's worth of men and 21,300 rifles.");
  });

  it('an infantry corps, teams on and teams off: "corps", as before', () => {
    for (const teams of [true, false]) {
      const { news, cadre, page } = folded('inf', teams);
      expect(news).toEqual(['Folded unit is folded into Kept unit at Chungking: about 27,000 men in one corps. Its cadre has gone home and can be raised again from the arsenal.']);
      expect(page.blurb).toBe(BLURB('corps'));
      expect(page.note).toBe('Forms up at Chungking, for 50% of the price of a new corps.');
      expect(cadre.blurb).toBe(BLURB('corps'));
      expect(cadre.note).toBe("Forms up at Chungking, for 50% of the price of a new corps. Takes one division's worth of men and 21,300 rifles.");
    }
  });
});
