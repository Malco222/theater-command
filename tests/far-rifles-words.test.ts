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
import { foldCardWords, foldCorps, foldView, foldWords } from '../src/campaign/fold';
import { addCorps, createGame, setWar } from '../src/campaign/game';
import { MANPOWER, classNow, comesWords, corpsMenWords, levyMen, manpower, menPer, menScale, menWords, noMenWords, reserveLevelWords, reserveWords } from '../src/campaign/manpower';
import { RIFLES, arsenalTop, inWords, levelOf, noRifles, riflesMonth, riflesSeen, riflesTakes, riflesView, riflesWait } from '../src/campaign/rifles';
import { loadGame, saveGame } from '../src/campaign/save';
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
    // A store just short of the need never reads the need: 63,850 to 63,899.99 read 63,800 (the sceptic's S1).
    for (const r of [63850, 63899, 63899.99]) {
      store(g, r);
      expect(noRifles(g, 'CHN', 3)).toBe('Not enough rifles: this corps takes 63,900 and 63,800 are in store. 8,300 come in each month, so it can be ordered in one month.');
    }
    store(g, 63849);
    expect(noRifles(g, 'CHN', 3)).toMatch(/ and 63,800 are in store\. /);
    store(g, 63900);
    expect(noRifles(g, 'CHN', 3)).toBeNull();
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
  it('14 and 15. the reserve and the class, at 67,000 men a division\'s worth, the men to the thousand (her read of 26 November, point 1)', () => {
    const g = grand();
    expect([menScale('CHN'), menPer('CHN'), menScale('JAP'), menPer('JAP')]).toEqual([67000, 67000, null, MEN_PER_DIV]);
    const v = manpower(g, 'CHN')!;
    expect(v.divs).toBeCloseTo(7.4, 12);
    expect(v.line).toBe("About 496,000 men stand in reserve: about seven divisions' worth.");
    expect(v.comes).toBe('About 105,000 men come of age each month in the land you still hold.');
    expect([v.per, v.men, v.classMen]).toEqual([67000, 495800, 104816]);
    // Where a page shows the class before the share of the homeland held (167,500 men).
    expect(comesWords('CHN', MANPOWER.class.CHN, true)).toBe('About 168,000 men come of age each month with all your homeland held.');
    // The number rule at its edges.
    expect(reserveWords('CHN', 1)).toBe("About 67,000 men stand in reserve: about one division's worth.");
    expect(reserveWords('CHN', 10.4)).toBe("About 697,000 men stand in reserve: about ten divisions' worth.");
    expect(reserveWords('CHN', 11)).toBe("About 737,000 men stand in reserve: about 11 divisions' worth.");
  });

  it("16. the ledger's men block says a new corps in divisions' worth for a nation with its own scale", () => {
    const g = grand();
    const v = manpower(g, 'CHN')!;
    // (104,816 men a month on the first day: 201,000 men is two months of the class.)
    expect(Math.ceil((3 * v.per) / v.classMen)).toBe(2);
    expect(corpsMenWords(v, 3)).toBe("A new infantry corps takes three divisions' worth of men, about two months of the class.");
    // Every other nation as before, byte for byte (the sceptic's S5).
    const j = manpower(g, 'JAP')!;
    const months = Math.ceil((3 * MEN_PER_DIV) / j.classMen);
    expect(corpsMenWords(j, 3)).toBe(`A new infantry corps takes ${menWords(3 * MEN_PER_DIV)} men${months > 1 ? `, about ${months} months of the class` : ''}.`);
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
    // (Line 14 says the divisions' worth: the tip does not say it again in figures, the sceptic's S3.)
    expect(v.men!.tip.split('\n').slice(0, 2)).toEqual(["About 496,000 men stand in reserve: about seven divisions' worth.", 'About 105,000 men come of age each month in the land you still hold.']);
    expect(v.men!.tip.split('\n')[2]).toMatch(/^Drawn since the campaign opened: /);
    expect(v.men!.tip).not.toContain('That is enough for');
    // A reserve of 7.6 divisions' worth: line 14 says eight, and nothing beside it says seven.
    const gg = loadGame((TC().save() as { text: string }).text)!;
    gg.flags['mp:CHN'] = 7.6;
    TC().load(saveGame(gg));
    const t = (TC().view() as { men: { tip: string } }).men.tip;
    expect(t.split('\n')[0]).toBe("About 509,000 men stand in reserve: about eight divisions' worth.");
    expect(t).not.toContain('That is enough for');
  });

  it("the 3D men page says China's men to the thousand in its headline and THE COUNT, as line 14 beside them does (the sceptic's S2)", () => {
    type P = { headline: string; note: string; sections: { heading: string; text: string }[] };
    const cnt = (p: P): string => p.sections.find((x) => x.heading === 'THE COUNT')!.text;
    TC().start({ human: 'CHN', seed: 7, view: 'grand', world: true });
    const p = TC().page('men') as P;
    expect(p.headline).toBe('496,000 men in reserve  \u00B7  Ample');
    expect(p.note).toBe("About 496,000 men stand in reserve: about seven divisions' worth.");
    expect(cnt(p)).toBe(['At the start: 496,000', 'Come of age since: +0', 'Called up so far: \u22120', 'In reserve today: 496,000', 'Come of age each month: +105,000'].join('\n'));
    // Thinning, at three divisions' worth: the headline and the count say the note's figure.
    const gg = loadGame((TC().save() as { text: string }).text)!;
    gg.flags['mp:CHN'] = 3;
    TC().load(saveGame(gg));
    const q = TC().page('men') as P;
    expect(q.headline).toBe('201,000 men in reserve  \u00B7  Thinning');
    expect(q.note).toBe('The reserve is thinning: about 201,000 men are left of about 496,000.');
    expect(cnt(q).split('\n')[3]).toBe('In reserve today: 201,000');
    expect(cnt(q).split('\n')[4]).toBe('Come of age each month: +105,000');
  });

  it("a nation without a scale of its own keeps its men page's figures whole", () => {
    TC().start({ human: 'JAP', seed: 17, view: 'grand', world: true });
    const p = TC().page('men') as { headline: string; sections: { heading: string; text: string }[] };
    expect(p.headline).toBe('605,381 men in reserve  \u00B7  Ample');
    expect(p.sections.find((x) => x.heading === 'THE COUNT')!.text).toBe(['At the start: 624,000', 'Come of age since: +0', 'Called up so far: \u221218,619', 'In reserve today: 605,381', 'Come of age each month: +12,480'].join('\n'));
  });

  it("a nation without a scale of its own keeps its men tip's second line", () => {
    TC().start({ human: 'JAP', seed: 7, view: 'grand', world: true });
    const tip = (TC().view() as { men: { tip: string } }).men.tip.split('\n');
    expect(tip[1]).toMatch(/^That is enough for about \d+ divisions?\.$/);
  });
});

// ---- The UI director's second read (26 November 2026, 01:59Z, on pacific-japan/ui-ask-rifles-words-2-1126.md) ----------
// Point 1: a nation's men at its own scale to the thousand, with "about" (above). Point 2: line 1 says 63,900. Point 4:
// the fold's men at the nation's own scale, the rifles as a clause on the men sent back. Point 5: the reserve's levels.
// Point 6: the refusal for lack of men. Point 7: lines 1, 2 and 12 at the top of the arsenal, page and 3D card.

describe('the second read: the store at the top of the arsenal', () => {
  const LINE1 = "Rifles in store □□□□□□□□□□ 0 of the 63,900 an infantry corps takes · +8,300 a month: Chungking's arsenals 3,000 · Changteh's arsenals 1,000 · Kweiyang's arsenals 500 · the Burma Road 2,500 · the Sinkiang road 1,300";
  const LINE2 = 'Your new corps and your replacements are armed from this store. It fills from your arsenals and from the roads you still hold.';

  it('2. line 1 says what an infantry corps takes to the hundred, as lines 8 and 9 do', () => {
    expect(riflesView(grand(), 'CHN')!.picture.line).toBe(LINE1);
  });

  it('7. lines 1 and 2 at the top of the arsenal, line 12 under them while it is true; no other nation has them', () => {
    const g = grand();
    expect(arsenalTop(g, 'CHN')).toEqual([LINE1, LINE2]);
    const c = g.corps.find((x) => x.nat === 'CHN' && !x.dead && x.kind === 'inf')!;
    c.str = levelOf(g, c) - 0.1;
    store(g, 0);
    expect(arsenalTop(g, 'CHN')).toEqual([LINE1, LINE2, "No rifles for this month's replacements: your corps wait for the store to fill."]);
    expect(arsenalTop(g, 'JAP')).toBeNull();
    expect(arsenalTop(grand('JAP', 17), 'JAP')).toBeNull();
  });
});

describe('the second read: the reserve of men at a nation\'s own scale', () => {
  it('5. each level, "about" on each figure, to the thousand, months in words, beside line 15', () => {
    const g = grand();
    const at = (divs: number): { word: string; line: string; comes: string | null } => { g.flags['mp:CHN'] = divs; const v = manpower(g, 'CHN')!; return { word: v.word, line: v.line, comes: v.comes }; };
    const comes = 'About 105,000 men come of age each month in the land you still hold.';
    // Thinning (under half of the 7.4 it opened with): her example.
    expect(at(3)).toEqual({ word: 'Thinning', line: 'The reserve is thinning: about 201,000 men are left of about 496,000.', comes });
    // Spent (under the class of 2.5 a month at full homeland): the class figure is left to line 15 beside it.
    expect(at(2)).toEqual({ word: 'Spent', line: 'The reserve is spent. Only the men who come of age each month reach the armies, and no new corps can be raised: a new infantry corps is about two months of them.', comes });
    // Running short: a form she gave no words for, laid by the same rules. China's class (2.5) stands above a quarter
    // of what it opened with (1.85), so on this chart China passes from thinning to spent; the sentence is pinned here.
    expect(reserveLevelWords('CHN', 2, 1.8, 7.4, 1.5644)).toBe('The reserve is running short: about 121,000 men are left. Every loss made good and every new corps is taken from it.');
    // Spent with a class of a quarter of the full (a homeland mostly lost): twelve months, in figures.
    expect(reserveLevelWords('CHN', 3, 0.1, 7.4, 0.25)).toBe('The reserve is spent. Only the men who come of age each month reach the armies, and no new corps can be raised: a new infantry corps is about 12 months of them.');
    expect(reserveLevelWords('CHN', 3, 0.1, 7.4, 0.5)).toBe('The reserve is spent. Only the men who come of age each month reach the armies, and no new corps can be raised: a new infantry corps is about six months of them.');
    // Ample: line 14, as before.
    expect(reserveLevelWords('CHN', 0, 7.4, 7.4, 1.5644)).toBeNull();
    // Every nation without a scale of its own keeps its lines.
    expect([1, 2, 3].map((lv) => reserveLevelWords('JAP', lv, 1, 10, 1))).toEqual([null, null, null]);
    const j = grand('JAP', 17);
    j.flags['mp:JAP'] = 0;
    const jv = manpower(j, 'JAP')!;
    const cls = classNow(j, 'JAP'), months = Math.ceil(3 / cls);
    expect(jv.line).toBe(`The reserve is spent. Only the ${menWords(cls * MEN_PER_DIV)} men who come of age each month reach the armies, and no new corps can be raised${months ? `: a new infantry corps is about ${months === 1 ? 'a month' : `${months} months`} of them` : ''}.`);
  });

  it("6. the refusal for lack of men: divisions' worth, about one from half up, less than half, never the need", () => {
    const g = grand();
    const cap = g.nations.CHN.capital;
    const why = (left: number, key: string): string | null => { g.flags['mp:CHN'] = left; return offers(g, 'CHN', cap).find((o) => o.key === key)!.why; };
    const CLASS = ' About 105,000 men come of age each month in the land you still hold.';
    expect(why(1.4, 'corps:inf')).toBe(`The reserve has not the men: this takes three divisions' worth and about one is left.${CLASS}`);
    expect(why(0.5, 'corps:inf')).toBe(`The reserve has not the men: this takes three divisions' worth and about one is left.${CLASS}`);
    expect(why(0.49, 'corps:inf')).toBe(`The reserve has not the men: this takes three divisions' worth and less than half of one is left.${CLASS}`);
    expect(why(0, 'corps:inf')).toBe(`The reserve has not the men: this takes three divisions' worth and less than half of one is left.${CLASS}`);
    expect(why(1.5, 'corps:inf')).toBe(`The reserve has not the men: this takes three divisions' worth and about two are left.${CLASS}`);
    // What is left never reads as what it takes: 2.5 to 2.99 of three read "about two", 1.5 to 1.99 of two "about one".
    expect(why(2.6, 'corps:inf')).toBe(`The reserve has not the men: this takes three divisions' worth and about two are left.${CLASS}`);
    expect(why(2.99, 'corps:inf')).toBe(`The reserve has not the men: this takes three divisions' worth and about two are left.${CLASS}`);
    expect(why(1.5, 'corps:mtn')).toBe(`The reserve has not the men: this takes two divisions' worth and about one is left.${CLASS}`);
    // An order of one division's worth with half of one or more left: China's engineer corps is offered, and this is its
    // live refusal (the sceptic's S3 of the second words round).
    expect(noMenWords('CHN', 1, 0.7, 1.5644171779141105)).toBe(`The reserve has not the men: this takes one division's worth and less than one is left.${CLASS}`);
    expect(noMenWords('CHN', 1, 0.2, 1.5644171779141105)).toBe(`The reserve has not the men: this takes one division's worth and less than half of one is left.${CLASS}`);
    expect(offers(g, 'CHN', cap).find((o) => o.key === 'corps:eng')!.takes).toBe("Takes one division's worth of men and 21,300 rifles.");
    expect(why(0.7, 'corps:eng')).toBe(`The reserve has not the men: this takes one division's worth and less than one is left.${CLASS}`);
    expect(why(0.5, 'corps:eng')).toBe(`The reserve has not the men: this takes one division's worth and less than one is left.${CLASS}`);
    expect(why(0.2, 'corps:eng')).toBe(`The reserve has not the men: this takes one division's worth and less than half of one is left.${CLASS}`);
    // Every nation without a scale of its own: as before.
    expect(noMenWords('JAP', 3, 1, 1)).toBeNull();
    const j = grand('JAP', 17);
    j.flags['mp:JAP'] = 1;
    expect(offers(j, 'JAP', j.nations.JAP.capital).find((o) => o.key === 'corps:inf')!.why).toMatch(/^The reserve has not the men: this takes 45,000 and 15,000 are left\. [\d,.]+( million)? come of age each month\.$/);
  });
});

describe('the second read: the fold', () => {
  /** Two corps of two divisions at the capital, at these strengths, with the reserve full enough for anything. */
  const two = (g: Game, nat: string, sa: number, sb: number) => {
    levyMen(g, nat, 30);
    const at = g.regions[g.nations[nat].capital].name;
    const a = addCorps(g, nat, 'Kept corps', 'inf', 2, at, {}), b = addCorps(g, nat, 'Folded corps', 'inf', 2, at, {});
    a.str = sa; b.str = sb;
    return { a, b, at };
  };
  const last = (g: Game): string => g.headlines[g.headlines.length - 1].text;

  it("11. China's men at her own scale with \"about\", and the rifles as a clause on the men sent back", () => {
    const g = grand();
    const { a, b } = two(g, 'CHN', 0.6, 0.6);
    expect(foldWords(g, 'CHN', foldView(g, a, b))).toEqual({ men: 'about 134,000 men', back: 'about 27,000 sent back to the reserve, and their rifles to the store', over: 'About 27,000 men over go back to the reserve, and their rifles to the store.' });
    expect(foldCorps(g, a, b)).toBe(true);
    expect(last(g)).toBe('Folded corps is folded into Kept corps at Chungking: about 134,000 men in one corps, about 27,000 sent back to the reserve, and their rifles to the store. Its cadre has gone home and can be raised again from the arsenal.');
  });

  it('11. (the reading) no men sent back: nothing is said of rifles, though the moved men\'s pass the kept corps\' full', () => {
    const g = grand();
    // The kept corps is armed to its full (no flag) at strength 0.3: the 0.6 moved in bring rifles it already holds.
    const { a, b } = two(g, 'CHN', 0.3, 0.3);
    const s = stockOf(g, 'CHN', 'rifles');
    expect(foldCorps(g, a, b)).toBe(true);
    expect(stockOf(g, 'CHN', 'rifles') - s).toBeCloseTo(0.6, 12);
    expect(last(g)).toBe('Folded corps is folded into Kept corps at Chungking: about 80,000 men in one corps. Its cadre has gone home and can be raised again from the arsenal.');
    // Men sent back that round to no thousand (67 men): said as none.
    const h = grand();
    const p = two(h, 'CHN', 0.6, 0.4005);
    expect(foldView(h, p.a, p.b).back).toBeGreaterThan(0);
    expect(foldCorps(h, p.a, p.b)).toBe(true);
    expect(last(h)).toBe('Folded corps is folded into Kept corps at Chungking: about 134,000 men in one corps. Its cadre has gone home and can be raised again from the arsenal.');
  });

  it('11. a nation without a store or a scale of its own: as built, byte for byte', () => {
    const g = grand('JAP', 17);
    const { a, b, at } = two(g, 'JAP', 0.6, 0.6);
    expect(foldWords(g, 'JAP', foldView(g, a, b))).toEqual({ men: '30,000 men', back: '6,000 sent back to the reserve', over: '6,000 men over go back to the reserve.' });
    expect(foldCorps(g, a, b)).toBe(true);
    expect(last(g)).toBe(`Folded corps is folded into Kept corps at ${at}: 30,000 men in one corps, 6,000 sent back to the reserve. Its cadre has gone home and can be raised again from the arsenal.`);
    const h = grand('JAP', 17);
    const p = two(h, 'JAP', 0.3, 0.3);
    expect(foldCorps(h, p.a, p.b)).toBe(true);
    expect(last(h)).toBe(`Folded corps is folded into Kept corps at ${p.at}: 18,000 men in one corps. Its cadre has gone home and can be raised again from the arsenal.`);
  });
});

describe('the second read: the 3D game says what the page says', () => {
  type Call = (...a: unknown[]) => unknown;
  const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
  type Row = { lines: string[]; buttons: { act: string }[] };
  /** The 3D fold card's row for these two corps, laid at the capital of the seat being played. */
  const foldRow = (nat: string, sa: number, sb: number): Row => {
    const gg = loadGame((TC().save() as { text: string }).text)!;
    const cap = gg.nations[nat].capital, at = gg.regions[cap].name;
    const a = addCorps(gg, nat, 'Kept corps', 'inf', 2, at, {}), b = addCorps(gg, nat, 'Folded corps', 'inf', 2, at, {});
    a.str = sa; b.str = sb;
    TC().load(saveGame(gg));
    const p = TC().page(`fold:${cap}`) as { sections: { rows: Row[] }[] };
    return p.sections.flatMap((x) => x.rows).find((r) => r.buttons[0].act === `fold:${a.id}:${b.id}`)!;
  };

  it("7. lines 1 and 2 at the top of the 3D arsenal for China, and nothing new for Japan", () => {
    TC().start({ human: 'CHN', seed: 7, view: 'grand', world: true });
    const a = TC().arsenal(-1) as { notes?: string[] };
    expect(a.notes).toEqual([
      "Rifles in store ■□□□□□□□□□ 8,300 of the 63,900 an infantry corps takes · +8,300 a month: Chungking's arsenals 3,000 · Changteh's arsenals 1,000 · Kweiyang's arsenals 500 · the Burma Road 2,500 · the Sinkiang road 1,300",
      'Your new corps and your replacements are armed from this store. It fills from your arsenals and from the roads you still hold.',
    ]);
    TC().start({ human: 'JAP', seed: 17, view: 'grand', world: true });
    expect('notes' in (TC().arsenal(-1) as object)).toBe(false);
  });

  it("11. the 3D fold card speaks China's men at her scale with the rifles' clause, and Japan's as built", () => {
    TC().start({ human: 'CHN', seed: 7, view: 'grand', world: true });
    const c = foldRow('CHN', 0.6, 0.6);
    expect(c.lines[0]).toMatch(/^About 134,000 men in one corps at 100% of its strength, [a-z]+\.$/);
    expect(c.lines).toContain('About 27,000 men over go back to the reserve, and their rifles to the store.');
    TC().start({ human: 'JAP', seed: 17, view: 'grand', world: true });
    const j = foldRow('JAP', 0.6, 0.6);
    expect(j.lines[0]).toMatch(/^30,000 men in one corps at 100% of its strength, [a-z]+\.$/);
    expect(j.lines).toContain('6,000 men over go back to the reserve.');
  });
});

// The UI director's second read of 26 November 2026, 03:16Z, point 1: the fold card at one scale. The heading of each
// pair says each corps' men at the nation's own scale with "about", to the thousand; the cadre line says the men in
// divisions' worth as line 8 says them, the treasury clause dropped where it is nothing. The months stay as each card
// says them (the lead's reading). One helper, fold.ts foldCardWords, read by both cards. Japan as built, byte for byte.
describe('the third read: the fold card at one scale', () => {
  type Call = (...a: unknown[]) => unknown;
  const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
  type Row = { lines: string[]; buttons: { act: string }[] };
  /** The 3D fold card's pair for two corps of this kind laid at the capital of the seat being played: its text, and the row that keeps the first. */
  const pair = (nat: string, seed: number, kind: string): { text: string; row: Row } => {
    TC().start({ human: nat, seed, view: 'grand', world: true });
    const gg = loadGame((TC().save() as { text: string }).text)!;
    const cap = gg.nations[nat].capital, at = gg.regions[cap].name;
    const a = addCorps(gg, nat, 'Central reserve', kind as 'inf', 2, at, {}), b = addCorps(gg, nat, `${at} garrison`, kind as 'inf', 2, at, {});
    a.str = 0.6; b.str = 0.6;
    TC().load(saveGame(gg));
    const p = TC().page(`fold:${cap}`) as { sections: { text: string; rows: Row[] }[] };
    const s = p.sections.find((x) => x.rows.some((r) => r.buttons[0].act === `fold:${a.id}:${b.id}`))!;
    return { text: s.text, row: s.rows.find((r) => r.buttons[0].act === `fold:${a.id}:${b.id}`)! };
  };
  /** Two corps of this many divisions at the capital, worn to 0.6, and the view of folding the second into the first. */
  const view = (g: Game, nat: string, divs: number, kind = 'inf') => {
    levyMen(g, nat, 30);
    const at = g.regions[g.nations[nat].capital].name;
    const a = addCorps(g, nat, 'Central reserve', kind as 'inf', divs, at, {}), b = addCorps(g, nat, `${at} garrison`, kind as 'inf', divs, at, {});
    a.str = 0.6; b.str = 0.6;
    return { a, b, v: foldView(g, a, b) };
  };

  it("1. China's 3D card: the pair's heading at her scale, and the cadre line in divisions' worth with no treasury clause", () => {
    const { text, row } = pair('CHN', 7, 'inf');
    expect(text).toBe('Central reserve: about 80,000 men, 60% of its strength. Chungking garrison: about 80,000 men, 60%.');
    expect(row.lines).toContain("Chungking garrison goes home as a cadre: it can be raised again from the arsenal for two divisions' worth of men from the reserve, in 5 months.");
    expect(row.lines.join('\n')).not.toMatch(/15,000|18,000|30,000|from the treasury/);
  });

  it('2. a cadre whose treasury cost is not nothing (an artillery corps of China\'s): the treasury clause kept, before the men', () => {
    const { text, row } = pair('CHN', 7, 'art');
    expect(text).toBe('Central reserve: about 80,000 men, 60% of its strength. Chungking garrison: about 80,000 men, 60%.');
    expect(row.lines).toContain("Chungking garrison goes home as a cadre: it can be raised again from the arsenal for 1,700 from the treasury and two divisions' worth of men from the reserve, in 5 months.");
  });

  it("3. a cadre of one division and of three: \"one division's worth\", \"three divisions' worth\"", () => {
    const w = foldCardWords('CHN');
    const one = view(grand(), 'CHN', 1), three = view(grand(), 'CHN', 3);
    expect(w.men(one.a)).toBe('about 40,000 men');
    expect(w.cadre(one.v, '0')).toBe("one division's worth of men from the reserve");
    expect(w.men(three.a)).toBe('about 121,000 men');
    expect(w.cadre(three.v, '0')).toBe("three divisions' worth of men from the reserve");
    // (A corps its row keeps at half a division comes back as half of one: said in men at her scale, with "about".)
    const g = grand();
    levyMen(g, 'CHN', 30);
    const at = g.regions[g.nations.CHN.capital].name;
    const a = addCorps(g, 'CHN', 'Small a', 'inf', 1, at, {}), b = addCorps(g, 'CHN', 'Small b', 'inf', 1, at, {});
    a.full = b.full = 0.5; a.str = b.str = 0.3;
    expect(w.cadre(foldView(g, a, b), '0')).toBe('about 34,000 men from the reserve');
  });

  it("4. Japan's 3D card: the pair's heading and the cadre line byte for byte as built", () => {
    const { text, row } = pair('JAP', 17, 'inf');
    expect(text).toBe('Central reserve: 18,000 men, 60% of its strength. Tokyo garrison: 18,000 men, 60%.');
    expect(row.lines).toContain('Tokyo garrison goes home as a cadre: it can be raised again from the arsenal for 2,400 from the treasury and 30,000 men from the reserve, in 5 months.');
  });

  it("5. the page's forms from the same helper (the page's wiring in ui/main.ts foldCard is read, not run)", () => {
    const c = view(grand(), 'CHN', 2), art = view(grand(), 'CHN', 2, 'art');
    const w = foldCardWords('CHN');
    // The page's heading: "Central reserve (about 80,000 men, 60%) and Chungking garrison (about 80,000 men, 60%)".
    expect(`${c.a.name} (${w.men(c.a)}, 60%) and ${c.b.name} (${w.men(c.b)}, 60%)`).toBe('Central reserve (about 80,000 men, 60%) and Chungking garrison (about 80,000 men, 60%)');
    // The page's money() floors the treasury's figure: 17 points are 1,700, 0 is 0.
    expect(c.v.cadreCost).toBe(0);
    expect(w.cadre(c.v, '0')).toBe("two divisions' worth of men from the reserve");
    expect(art.v.cadreCost).toBe(17);
    expect(w.cadre(art.v, '1,700')).toBe("1,700 from the treasury and two divisions' worth of men from the reserve");
    const j = view(grand('JAP', 17), 'JAP', 2);
    const wj = foldCardWords('JAP');
    expect(wj.men(j.a)).toBe('18,000 men');
    expect(wj.cadre(j.v, '2,400')).toBe('2,400 from the treasury and 30,000 men from the reserve');
  });
});

// The words sceptic of the third round, F2 and F3, on China's live board (seed 7): a corps its row keeps small, the
// 10th Army Group (its own size 0.27 of a division), worn to 60% of that size beside the 23rd Army Group at Chuchow.
// F2: its cadre is part of a division's worth, so the order's note and the refusal say its men as the fold card does
// ("about 18,000 men"), never "0 divisions' worth" (manpower.ts menWorth, one form for all three). F3: at a nation's
// own scale a corps' strength on the fold card is read against its own size, as whyNotFold reads it, so the corps that
// is "already at full strength for its size" never reads "27% of its strength". Japan as built, byte for byte.
describe('a corps kept small: its cadre and its strength at one scale', () => {
  type Call = (...a: unknown[]) => unknown;
  const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
  type Row = { lines: string[] };
  const KEEP = '3rd War Area: 23rd Army Group and the New Fourth Army', GONE = '3rd War Area: 10th Army Group';
  const CLASS = ' About 105,000 men come of age each month in the land you still hold.';
  /** The Chuchow pair worn to 60% of its own size: the 3D fold card's section, then (folded, the reserve at 'mp' if given) the cadre's offer. */
  const chuchow = (mp: number | null): { text: string; rows: Row[]; note: string; why: string | null } => {
    TC().start({ human: 'CHN', seed: 7, view: 'grand', world: true });
    const g = loadGame((TC().save() as { text: string }).text)!;
    const k = g.corps.find((c) => c.name === KEEP && c.nat === 'CHN')!, x = g.corps.find((c) => c.name === GONE && c.nat === 'CHN')!;
    for (const c of [k, x]) { c.str = 0.6 * (c.full ?? 1); c.supplied = true; }
    TC().load(saveGame(g));
    const s = (TC().page(`fold:${k.reg}`) as { sections: { text: string; rows: Row[] }[] }).sections[0];
    expect(foldCorps(g, k, x)).toBe(true);
    if (mp !== null) g.flags['mp:CHN'] = mp;
    TC().load(saveGame(g));
    const o = (TC().arsenal(-1) as { offers: { key: string; note: string; why: string | null }[] }).offers.find((y) => y.key === `cadre:${x.id}`)!;
    return { text: s.text, rows: s.rows, note: o.note, why: o.why };
  };

  it("F2 and F3, the 3D card and the arsenal: the cadre's men as the fold card says them, each corps' strength against its own size", () => {
    const c = chuchow(null);
    expect(c.text).toBe(`${GONE}: about 11,000 men, 60% of its strength. ${KEEP}: about 40,000 men, 60%.`);
    expect(c.rows.map((r) => r.lines[0])).toEqual(['About 18,000 men in one corps at 100% of its strength, green.', 'About 51,000 men in one corps at 76% of its strength, green.']);
    expect(c.rows[1].lines).toContain(`${GONE} goes home as a cadre: it can be raised again from the arsenal for about 18,000 men from the reserve, in 5 months.`);
    expect(c.note).toBe('Forms up at Chungking, for 50% of the price of a new corps. Takes about 18,000 men and 5,800 rifles.');
    expect(c.why).toBeNull();
    const short = chuchow(0.1);
    expect(short.why).toBe(`The reserve has not the men: this takes about 18,000 men and less than half of a division's worth is left.${CLASS}`);
  });

  it('F2, the helpers: part of a division\'s worth in men at the scale, whole counts as line 8 says them', () => {
    const g = grand();
    expect(riflesTakes(g, 'CHN', 0.5)).toBe('Takes about 34,000 men and 10,700 rifles.');
    expect(riflesTakes(g, 'CHN', 3)).toBe("Takes three divisions' worth of men and 63,900 rifles.");
    expect(noMenWords('CHN', 0.5, 0.2, 1.5644171779141105)).toBe(`The reserve has not the men: this takes about 34,000 men and less than half of a division's worth is left.${CLASS}`);
    expect(noMenWords('CHN', 1.5, 1.2, 1.5644171779141105)).toBe(`The reserve has not the men: this takes about 101,000 men and about one division's worth is left.${CLASS}`);
    expect(noMenWords('CHN', 3, 0.2, 1.5644171779141105)).toBe(`The reserve has not the men: this takes three divisions' worth and less than half of one is left.${CLASS}`);
  });

  it('F3, Japan: a strength is shown as it stands, as built', () => {
    const w = foldCardWords('JAP');
    expect(w.share(0.27, { full: 0.27 } as never)).toBe(0.27);
    expect(foldCardWords('CHN').share(0.162, { full: 0.27 } as never)).toBeCloseTo(0.6, 10);
  });
});
