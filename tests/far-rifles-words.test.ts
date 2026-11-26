// China's rifles, stage B2: the words (the UI director's read of 25 November 2026, 19:15Z and 19:16Z, on
// pacific-japan/ui-ask-rifles-words-1125.md, lines 1 to 16). Her rule for all of them: numbers one to ten in words;
// men and rifles to the hundred everywhere, so a cost and the store add up. "Replacements" is the game's one word: no
// "drafts" in a sentence a player reads. Each built sentence is pinned whole here: line 2 (the arsenal page's sentence),
// line 4 (a road cut by the seller's war), line 7 (Japan's card), line 8 (the order's note), line 9 (the refusal, at
// the number rule's edges), line 12 (replacements held back by an empty store), lines 14 and 15 (China's men at its own
// scale, 67,000 men to a division's worth, laid in far-arms.json). Every other nation keeps today's sentences.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { stockOf } from '../src/campaign/arms';
import { offers } from '../src/campaign/arsenal';
import { createGame, setWar } from '../src/campaign/game';
import { MANPOWER, comesWords, manpower, menPer, menScale, reserveWords } from '../src/campaign/manpower';
import { RIFLES, inWords, levelOf, noRifles, riflesMonth, riflesSeen, riflesTakes, riflesView, riflesWait } from '../src/campaign/rifles';
import { startOpen } from '../src/campaign/staff';
import { MEN_PER_DIV, type Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

const PER = 21300;
const undo: (() => void)[] = [];
afterEach(() => { while (undo.length) undo.pop()!(); });

const grand = (human: string | null = 'CHN', seed = 7): Game => startOpen(createGame(seed, human));
/** Sets China's rifles in store, in whole rifles. */
const store = (g: Game, rifles: number): void => { g.flags['sa:CHN'] = rifles / PER; };
const NONE = ' No rifles are coming in: it can be ordered only once a road runs again or an arsenal is back in your hands.';

describe('the number rule', () => {
  it('one to ten in words, any other in figures', () => {
    expect([1, 2, 3, 6, 7, 8, 10].map(inWords)).toEqual(['one', 'two', 'three', 'six', 'seven', 'eight', 'ten']);
    expect([0, 11, 14, 1000].map(inWords)).toEqual(['0', '11', '14', '1,000']);
  });
});

describe('the store', () => {
  it("2. the arsenal page's sentence says replacements", () => {
    const v = riflesView(grand(), 'CHN')!;
    expect(v.says).toBe('Your new corps and your replacements are armed from this store. It fills from your arsenals and from the roads you still hold.');
    expect(v.says).not.toMatch(/draft/i);
  });

  it("4. a road cut by the seller's war is told to the player's own seat as his war", () => {
    const g = grand();
    riflesMonth(g, 'CHN');
    const n = g.headlines.length;
    g.turn++;
    setWar(g, 'SOV', 'CHN');
    riflesMonth(g, 'CHN');
    expect(g.headlines.slice(n).map((h) => h.text)).toEqual(['The Sinkiang road is cut: you are at war with the Soviet Union, which sold you its rifles. 1,300 fewer rifles reach your store each month.']);
  });

  it("7. Japan's card: a new infantry corps about every eight months", () => {
    const g = grand('JAP', 17);
    expect(riflesSeen(g, 'CHN', 'JAP')!.card).toBe('China arms its new corps from its arsenals, the Burma Road and the Sinkiang road: about 8,300 rifles a month, a new infantry corps about every eight months.');
  });

  it('12. a line while the store holds back the replacements, and none otherwise', () => {
    const g = grand();
    const said = "No rifles for this month's replacements: your corps wait for the store to fill.";
    // The first day: every corps is armed to its strength, so nobody waits.
    expect(riflesWait(g, 'CHN')).toBeNull();
    const c = g.corps.find((x) => x.nat === 'CHN' && !x.dead && x.kind === 'inf')!;
    c.str = levelOf(g, c) - 0.1;
    store(g, 0);
    expect(riflesWait(g, 'CHN')).toBe(said);
    // A store that reads 0 to the hundred is empty; one that reads 100 is not.
    store(g, 49);
    expect(riflesWait(g, 'CHN')).toBe(said);
    store(g, 51);
    expect(riflesWait(g, 'CHN')).toBeNull();
    // A nation with no store says nothing.
    expect(riflesWait(g, 'JAP')).toBeNull();
  });
});

describe('the order', () => {
  it("8. the order's note: divisions' worth of men and the rifles to the hundred, a mountain corps exact", () => {
    const g = grand();
    const o = offers(g, 'CHN', g.nations.CHN.capital);
    expect(o.find((x) => x.key === 'corps:inf')!.takes).toBe("Takes three divisions' worth of men and 63,900 rifles.");
    expect(o.find((x) => x.key === 'corps:mtn')!.takes).toBe("Takes two divisions' worth of men and 42,600 rifles.");
    expect(riflesTakes(g, 'CHN', 1)).toBe("Takes one division's worth of men and 21,300 rifles.");
    // Every other nation's order keeps its note in men: no sentence of the store's.
    expect(riflesTakes(g, 'JAP', 3)).toBeNull();
    for (const x of offers(g, 'JAP', g.nations.JAP.capital)) expect(x.takes, x.key).toBeUndefined();
  });

  it('9. the refusal, at the number rule\'s edges', () => {
    const g = grand();
    // (The first day's inflow, 8,300 a month: the arsenals 4,500, the Burma Road 2,500, the Sinkiang road 1,300.)
    expect(Math.round(riflesView(g, 'CHN')!.inflow.total)).toBe(8300);
    store(g, 0);
    expect(noRifles(g, 'CHN', 3)).toBe('Not enough rifles: this corps takes 63,900 and 0 are in store. 8,300 come in each month, so it can be ordered in eight months.');
    // One month, in words and alone.
    store(g, 55700);
    expect(noRifles(g, 'CHN', 3)).toBe('Not enough rifles: this corps takes 63,900 and 55,700 are in store. 8,300 come in each month, so it can be ordered in one month.');
    // Ten in words, 11 in figures.
    store(g, 4260);
    expect(noRifles(g, 'CHN', 4)).toBe('Not enough rifles: this corps takes 85,200 and 4,300 are in store. 8,300 come in each month, so it can be ordered in ten months.');
    store(g, 2130);
    expect(noRifles(g, 'CHN', 4)).toBe('Not enough rifles: this corps takes 85,200 and 2,100 are in store. 8,300 come in each month, so it can be ordered in 11 months.');
    // The store to the hundred: 999 reads 1,000, 949 reads 900.
    store(g, 999);
    expect(noRifles(g, 'CHN', 3)).toMatch(/ and 1,000 are in store\. /);
    store(g, 949);
    expect(noRifles(g, 'CHN', 3)).toMatch(/ and 900 are in store\. /);
    // Nothing coming in: the way out.
    const kept = RIFLES.roads.splice(0);
    undo.push(() => { RIFLES.roads.splice(0, RIFLES.roads.length, ...kept); });
    for (const place of ['Chungking', 'Changteh', 'Kweiyang']) g.byName[place].ctrl = 'JAP';
    expect(riflesView(g, 'CHN')!.inflow.total).toBe(0);
    store(g, 0);
    expect(noRifles(g, 'CHN', 3)).toBe(`Not enough rifles: this corps takes 63,900 and 0 are in store.${NONE}`);
    expect(stockOf(g, 'CHN', 'rifles')).toBe(0);
  });

  it('no sentence of the store\'s says "drafts"', () => {
    const g = grand();
    const c = g.corps.find((x) => x.nat === 'CHN' && !x.dead && x.kind === 'inf')!;
    c.str = levelOf(g, c) - 0.1;
    store(g, 0);
    const v = riflesView(g, 'CHN')!, o = offers(g, 'CHN', g.nations.CHN.capital);
    const all = [v.says, v.picture.line, riflesSeen(g, 'CHN', 'JAP')!.card, riflesSeen(g, 'CHN', 'JAP')!.line, noRifles(g, 'CHN', 3)!, riflesWait(g, 'CHN')!, ...o.map((x) => x.takes ?? ''), manpower(g, 'CHN')!.line, manpower(g, 'CHN')!.comes!];
    for (const s of all) expect(s).not.toMatch(/draft/i);
  });
});

describe("China's men at its own scale", () => {
  it('14 and 15. the reserve and the class, at 67,000 men a division\'s worth, the men to the hundred', () => {
    const g = grand();
    expect([menScale('CHN'), menPer('CHN'), menScale('JAP'), menPer('JAP')]).toEqual([67000, 67000, null, MEN_PER_DIV]);
    const v = manpower(g, 'CHN')!;
    expect(v.divs).toBeCloseTo(7.4, 12);
    expect(v.line).toBe("About 495,800 men stand in reserve: about seven divisions' worth.");
    expect(v.comes).toBe('About 104,800 men come of age each month in the land you still hold.');
    expect([v.per, v.men]).toEqual([67000, 495800]);
    // Where a page shows the class before the share of the homeland held.
    expect(comesWords('CHN', MANPOWER.class.CHN, true)).toBe('About 167,500 men come of age each month with all your homeland held.');
    // The number rule at its edges.
    expect(reserveWords('CHN', 1)).toBe("About 67,000 men stand in reserve: about one division's worth.");
    expect(reserveWords('CHN', 10.4)).toBe("About 696,800 men stand in reserve: about ten divisions' worth.");
    expect(reserveWords('CHN', 11)).toBe("About 737,000 men stand in reserve: about 11 divisions' worth.");
  });

  it('every nation without a scale of its own keeps its sentences', () => {
    const g = grand();
    const v = manpower(g, 'JAP')!;
    expect(v.line).toBe(`${(Math.round((v.divs * MEN_PER_DIV) / 10000) * 10000).toLocaleString('en-US')} men stand in reserve.`);
    expect([v.comes, v.per, v.men]).toEqual([null, MEN_PER_DIV, Math.round(v.divs * MEN_PER_DIV)]);
    expect([reserveWords('JAP', 3), comesWords('JAP', 1), comesWords('JAP', 1, true)]).toEqual([null, null, null]);
  });
});

describe('the 3D game says what the page says', () => {
  type Call = (...a: unknown[]) => unknown;
  const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;

  it("8 and 15. the bridge's order note and men panel, for a played China", () => {
    TC().start({ human: 'CHN', seed: 7, view: 'grand', world: true });
    const a = TC().arsenal(-1) as { offers: { key: string; note: string }[] };
    expect(a.offers.find((o) => o.key === 'corps:inf')!.note).toContain("Takes three divisions' worth of men and 63,900 rifles.");
    expect(a.offers.find((o) => o.key === 'corps:inf')!.note).not.toContain('from the reserve');
    const v = TC().view() as { men: { tip: string } | null };
    expect(v.men!.tip.split('\n').slice(0, 3)).toEqual(["About 495,800 men stand in reserve: about seven divisions' worth.", 'That is enough for about 7 divisions.', 'About 104,800 men come of age each month in the land you still hold.']);
  });
});
