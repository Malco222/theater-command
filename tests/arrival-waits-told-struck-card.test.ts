// The struck line on the month cards (arrivalwaits.ts `struckOnCard`, monthcard.ts, ui/monthview.ts and the bridge's
// `shownCard`). The UI director's section 11 (a), 06:56Z, 27 November 2026 (pacific-japan/ui-words-row-waits-contact-
// UI-MARKS.md): "The struck line takes the place of the muted line it ends. It stands first among the home lines (after
// the men's line, as 8.4), is never cut and never counted in '{159} more lines of news are in the Newsreel'. The
// browser's card and the bridge's card both read it the way they read the waiting line (8.4)." "It is said once for
// each row, in the month that row is struck, not again after each arrival."
//
// Hand boards in the sceptic's shapes (round 1's S2: 'one', 'three', 'three2', 'three-staggered', 'two', 'frontplace',
// and a month with far more than three lines of his news), built here on boards of their own: seed 34, Japan seated,
// stop 3, the far side on, the switch on; the place made China's and emptied; the corps the rows are formed from
// destroyed (on the memorial wall), and the rows that make them read as sent. Each strike month is read on the rules'
// card, the browser's card (monthCardHtml) and the bridge's card (TC.endMonth's month, from a save made the month before).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { rowsWaitingOnRows, setFromWait } from '../src/campaign/arrivalwaits';
import { signPeace } from '../src/campaign/diplomacy';
import { createGame, declareWar, headline, relocate, removeCorps, startPhase } from '../src/campaign/game';import { setStop } from '../src/campaign/history';
import { markFor, monthCard, type MonthCard, type MonthMark } from '../src/campaign/monthcard';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { storyFallen } from '../src/campaign/stories';
import { monthCardHtml } from '../src/campaign/ui/monthview';
import type { Game } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
type Home = { text: string; grey: boolean; tip: string };
type Shown = { home: Home[]; rest: string; staff: { text: string }[]; elsewhere: { text: string }[] };

const rows = (setup as any).expeditions as any[][];
const index = (name: string): number => rows.findIndex((r) => r[1] === 'JAP' && r[2] === name);
const M = index('12th Army (62nd Division)');
const K3 = ['12th Army (4th Cavalry Brigade)', '12th Army (37th Division)', '12th Army (110th Division)'].map(index);
const Y1 = index('11th Army (64th Division)');
const BRIGADES = '1st Army (independent brigades, Taiyuan)';
const TAIL = 'is still not in friendly hands, and the arrival has waited as long as it may.';

// Her words, to the letter, with these boards' slots (month 61 is September 1944, 62 October 1944).
const ONE = 'The 12th Army (62nd Division), due in Kaifeng since September 1944, will not form: Kaifeng is still not in friendly hands, and the arrival has waited as long as it may.';
const THREE = 'The 12th Army (4th Cavalry Brigade), the 12th Army (37th Division) and the 12th Army (110th Division), due in Kaifeng since October 1944, will not form: Kaifeng is still not in friendly hands, and the arrival has waited as long as it may.';
const THREE2_A = 'The 12th Army (4th Cavalry Brigade) and the 12th Army (37th Division), due in Kaifeng since October 1944, will not form: Kaifeng is still not in friendly hands, and the arrival has waited as long as it may.';
const THREE2_B = 'The 12th Army (110th Division), due in Kaifeng since September 1944, will not form: Kaifeng is still not in friendly hands, and the arrival has waited as long as it may.';
const STAG_A = 'The 12th Army (4th Cavalry Brigade) and the 12th Army (37th Division), due in Kaifeng since October 1944, will not form: Kaifeng is still not in friendly hands, and the arrival has waited as long as it may.';
const STAG_B = 'The 12th Army (110th Division), due in Kaifeng since October 1944, will not form: Kaifeng is still not in friendly hands, and the arrival has waited as long as it may.';
const TWO_Y = 'The 11th Army (64th Division), due in Yochow since October 1944, will not form: Yochow is still not in friendly hands, and the arrival has waited as long as it may.';
const TWO_K = 'The 12th Army (4th Cavalry Brigade), due in Kaifeng since October 1944, will not form: Kaifeng is still not in friendly hands, and the arrival has waited as long as it may.';
// (Front at Hsuchow in month 61, held for its place from month 62 when the brigades are lost: struck at the front
// wait's half year, month 69, with the place hold's month.)
const FRONTPLACE = 'The 12th Army (62nd Division), due in Kaifeng since October 1944, will not form: Kaifeng is still not in friendly hands, and the arrival has waited as long as it may.';
const WAIT_THREE_110 = 'The 12th Army (110th Division), due in Kaifeng since October 1944, waits until Kaifeng is in friendly hands.';

const kill = (g: Game, nm: string): void => { const c = g.corps.find((x) => x.nat === 'JAP' && !x.dead && x.name === nm); if (c) { storyFallen(g, c, 'destroyed', 'CHN', c.reg); removeCorps(g, c); } };
/** The corps row `j` is formed from destroyed, and the rows that would make them read as sent: it waits for its place alone. */
const empty = (g: Game, j: number): void => {
  for (const nm of rows[j][6].from as string[]) {
    kill(g, nm);
    rows.forEach((r, k) => { if (r[1] === 'JAP' && r[2] === nm && !g.flags[`sent:${k}`]) g.flags[`sent:${k}`] = 1; });
  }
};
const lose = (g: Game, place: string): void => { const k = g.byName[place]; for (const u of [...k.units]) relocate(g, u, g.byName.Hsuchow); k.ctrl = 'CHN'; };
const make = (on = true, human = 'JAP'): Game => { const g = startOpen(createGame(34, human)); setStop(g, 3, true); if (on) setFromWait(g, true); declareWar(g, 'JAP', 'USA'); return g; };
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'JAP'); };
const esc = (s: string): string => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
const unesc = (s: string): string => s.replace(/&quot;/g, '"').replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&amp;/g, '&');
const html = (card: MonthCard): string => monthCardHtml(card, { esc, flag: () => '', name: (x: string) => x, all: false, orders: true } as any);
/** The browser card's "At home" box, line by line, as the player reads it. */
const homeOf = (h: string): string[] => {
  const box = /<div class="mc-lab">At home<\/div>([\s\S]*?)<\/div>\s*<div class="mc-box">/.exec(h)?.[1] ?? '';
  return [...box.matchAll(/<div class="mc-it[^"]*">([\s\S]*?)<\/div>/g)].map((m) => unesc(m[1].replace(/<button[\s\S]*?<\/button>/g, '').replace(/<[^>]+>/g, '')).trim());
};
const struckOf = (g: Game, t: number): string[] => g.headlines.filter((h) => h.turn === t && h.text.endsWith(TAIL)).map((h) => h.text);
/** `n` lines of his own news in the month (a month like the verifier's case F, with far more than three). */
const pad = (g: Game, n: number): void => { for (let k = 1; k <= n; k++) headline(g, 'info', `A line of his news, number ${k}.`, 'JAP'); };

/**
 * The rules' card and the browser's card in the strike month: the struck lines first among the home lines (after the
 * men's line), each once, never among the card's news lines, and not counted in "more": the card without the struck
 * headline counts as many lines over as the card with it.
 */
function onBrowserCard(g: Game, mark: MonthMark, lines: string[], waitsAfter: string[] = []): void {
  const card = monthCard(g, mark, 'JAP');
  const men = card.menOut ? 1 : 0;
  expect((card.waits ?? []).slice(0, lines.length)).toEqual(lines);
  expect((card.waits ?? []).slice(lines.length)).toEqual(waitsAfter);
  const home = homeOf(html(card));
  expect(home.slice(men, men + lines.length)).toEqual(lines);
  for (const x of lines) {
    expect(home.filter((y) => y === x)).toHaveLength(1);
    expect(card.also).not.toContain(x);
    expect(card.alsoFull.join(' ')).not.toContain(x);
  }
  const without: Game = { ...g, headlines: g.headlines.filter((h) => !lines.includes(h.text)) };
  expect(monthCard(without, mark, 'JAP').more).toBe(card.more);
}

/**
 * The bridge's card in the strike month, from a save made the month before (his month open), played on through
 * TC.endMonth: month.home has each struck line first, muted, once; it is in none of the card's other lines; and the
 * rules' card the bridge reads does not count it in "more".
 */
function onBridgeCard(g: Game, said: string[], padBefore = 0): void {
  g.phase = g.order.indexOf('JAP');
  pad(g, padBefore);
  TC().load(saveGame(g));
  const t0 = g.turn;
  // (A card opened on a save tells the whole of the save's month, `markBackTo`. A struck line said earlier in that
  // month, before the save, is NOT on it and NOT counted: it stays in the Newsreel. The UI director's section 12, A1,
  // 16:07Z, 27 November 2026: "Only the strike month's own card puts the struck line in the waiting line's place. A
  // loaded game's first card neither shows nor counts an older struck line; it stays in the Newsreel." Round 2 read
  // `[...struckOf(g, t0), ...said]` here: changed by her A1.)
  const older = struckOf(g, t0);
  const lines = said;
  const res = TC().endMonth() as { headlines: { text: string; turn: number }[]; month: Shown };
  expect(res.headlines.filter((h) => h.text.endsWith(TAIL)).map((h) => h.text)).toEqual(said);
  const home = res.month.home;
  expect(home.slice(0, lines.length).map((x) => [x.text, x.grey, x.tip])).toEqual(lines.map((x) => [x, true, x]));
  for (const x of lines) {
    expect(home.filter((y) => y.text === x || y.tip === x)).toHaveLength(1);
    expect([...res.month.staff, ...res.month.elsewhere].some((y) => y.text === x)).toBe(false);
  }
  for (const x of older) {
    expect(home.some((y) => y.text === x || y.tip === x)).toBe(false);
    expect([...res.month.staff, ...res.month.elsewhere].some((y) => y.text === x)).toBe(false);
  }
  // (The rules' card the bridge read: from the month the save opened, the whole of it, as `markBackTo` reads it.)
  const after = loadGame((TC().save() as { text: string }).text)!;
  const start = after.headlines.findIndex((h) => h.turn >= t0);
  const mark: MonthMark = { ...markFor(after), turn: t0, h: start < 0 ? after.headlines.length : start, orders: [] } as MonthMark;
  const card = monthCard(after, mark, 'JAP', { whole: true });
  expect((card.waits ?? []).slice(0, lines.length)).toEqual(lines);
  for (const x of older) expect(card.waits ?? []).not.toContain(x);
  // (Neither the strike month's own lines nor an older one are counted in "more"; the older one is still in the Newsreel.)
  const without: Game = { ...after, headlines: after.headlines.filter((h) => !lines.includes(h.text) && !older.includes(h.text)) };
  expect(monthCard(without, mark, 'JAP', { whole: true }).more).toBe(card.more);
  for (const x of older) expect(after.headlines.filter((h) => h.text === x)).toHaveLength(1);
}

/**
 * The loaded game's first card on the bridge (her A1): a save made in the strike month, after the struck line was said,
 * loaded and played on through TC.endMonth. No struck line of that month is on month.home or anywhere on the card; the
 * rules' card the bridge read neither shows it in `waits` nor counts it in "more"; and the Newsreel holds it once.
 * Returns the bridge card's "rest" line for the caller to weigh.
 */
function notOnLoadedCard(g: Game, older: string[]): { rest: string; home: Home[] } {
  g.phase = g.order.indexOf('JAP');
  TC().load(saveGame(g));
  const t0 = g.turn;
  expect(older.length).toBeGreaterThan(0);
  const res = TC().endMonth() as { headlines: { text: string; turn: number }[]; month: Shown };
  expect(res.headlines.some((h) => older.includes(h.text))).toBe(false);
  for (const x of older) {
    expect(res.month.home.some((y) => y.text === x || y.tip === x)).toBe(false);
    expect([...res.month.staff, ...res.month.elsewhere].some((y) => y.text === x)).toBe(false);
  }
  const after = loadGame((TC().save() as { text: string }).text)!;
  const start = after.headlines.findIndex((h) => h.turn >= t0);
  const mark: MonthMark = { ...markFor(after), turn: t0, h: start < 0 ? after.headlines.length : start, orders: [] } as MonthMark;
  // (The card does read the whole of the save's month, as `markBackTo` does for every line of news: the struck
  // headline is among the lines it reads, and is claimed by the card without a word.)
  expect(after.headlines.slice(mark.h).some((h) => older.includes(h.text))).toBe(true);
  const card = monthCard(after, mark, 'JAP', { whole: true });
  for (const x of older) {
    expect(card.waits ?? []).not.toContain(x);
    expect(card.also).not.toContain(x);
    expect(card.alsoFull.join(' ')).not.toContain(x);
  }
  const without: Game = { ...after, headlines: after.headlines.filter((h) => !older.includes(h.text)) };
  expect(monthCard(without, mark, 'JAP', { whole: true }).more).toBe(card.more);
  for (const x of older) expect(after.headlines.filter((h) => h.text === x)).toHaveLength(1);
  // (And the bridge's own "rest": the same save with the struck headline taken out of its news reads the same count.)
  TC().load(saveGame({ ...g, headlines: g.headlines.filter((h) => !older.includes(h.text)) }));
  const bare = TC().endMonth() as { month: Shown };
  expect(res.month.rest).toBe(bare.month.rest);
  return { rest: res.month.rest, home: res.month.home };
}

describe("the struck line on both month cards, in the muted line's place (the UI director's section 11 (a))", () => {
  it("'one': the row's muted line in October's card, the struck line in November's, first among the home lines", () => {
    const g = make(); lose(g, 'Kaifeng'); kill(g, BRIGADES); g.flags[`due:${M}`] = 55;
    month(g, 61);
    const wait = 'The 12th Army (62nd Division), due in Kaifeng since September 1944, waits until Kaifeng is in friendly hands.';
    expect(rowsWaitingOnRows(g, 'JAP')).toContain(wait);
    const saved = saveGame(g);
    const mark = markFor(g);
    month(g, 62);
    expect(struckOf(g, 62)).toEqual([ONE]);
    onBrowserCard(g, mark, [ONE], rowsWaitingOnRows(g, 'JAP'));
    expect(rowsWaitingOnRows(g, 'JAP')).not.toContain(wait);
    onBridgeCard(loadGame(saved)!, [ONE]);
  });

  it("a month with far more than three lines of his news: the struck line is still first and whole, and not counted", () => {
    const g = make(); lose(g, 'Kaifeng'); kill(g, BRIGADES); g.flags[`due:${M}`] = 55;
    month(g, 61);
    const saved = saveGame(g);
    const mark = markFor(g);
    pad(g, 9);
    month(g, 62);
    const card = monthCard(g, mark, 'JAP');
    expect(card.more).toBeGreaterThan(3);
    onBrowserCard(g, mark, [ONE], rowsWaitingOnRows(g, 'JAP'));
    onBridgeCard(loadGame(saved)!, [ONE], 9);
  });

  it("'three': several rows of one place and one month read the ONE grouped line", () => {
    const g = make(); lose(g, 'Kaifeng'); for (const j of K3) { empty(g, j); g.flags[`due:${j}`] = 56; }
    month(g, 62);
    const saved = saveGame(g);
    const mark = markFor(g);
    month(g, 63);
    expect(struckOf(g, 63)).toEqual([THREE]);
    onBrowserCard(g, mark, [THREE], rowsWaitingOnRows(g, 'JAP'));
    onBridgeCard(loadGame(saved)!, [THREE]);
  });

  it("'three2': rows whose place holds began in different months read their own lines, each among the home lines", () => {
    const g = make(); lose(g, 'Kaifeng'); for (const j of K3) { empty(g, j); g.flags[`due:${j}`] = 56; }
    month(g, 62);
    g.flags[`rowPlaceSince:${K3[2]}`] = 61;
    const saved = saveGame(g);
    const mark = markFor(g);
    month(g, 63);
    expect(struckOf(g, 63)).toEqual([THREE2_A, THREE2_B]);
    onBrowserCard(g, mark, [THREE2_A, THREE2_B], rowsWaitingOnRows(g, 'JAP'));
    onBridgeCard(loadGame(saved)!, [THREE2_A, THREE2_B]);
  });

  it("'three-staggered': struck a month apart, each in its own month; the row still waiting keeps its muted line after the struck one", () => {
    const g = make(); lose(g, 'Kaifeng'); K3.forEach((j, k) => { empty(g, j); g.flags[`due:${j}`] = k === 2 ? 57 : 56; });
    month(g, 62);
    const saved = saveGame(g);
    let mark = markFor(g);
    month(g, 63);
    expect(struckOf(g, 63)).toEqual([STAG_A]);
    expect(rowsWaitingOnRows(g, 'JAP')).toContain(WAIT_THREE_110);
    onBrowserCard(g, mark, [STAG_A], rowsWaitingOnRows(g, 'JAP'));
    onBridgeCard(loadGame(saved)!, [STAG_A]);
    mark = markFor(g);
    month(g, 64);
    expect(struckOf(g, 64)).toEqual([STAG_B]);
    onBrowserCard(g, mark, [STAG_B], rowsWaitingOnRows(g, 'JAP'));
    // (November's line is not said again in December, on any surface.)
    expect(monthCard(g, mark, 'JAP').waits ?? []).not.toContain(STAG_A);
  });

  it("'two': rows of two places read their own lines, each among the home lines", () => {
    const g = make(); lose(g, 'Kaifeng'); lose(g, 'Yochow'); for (const j of [K3[0], Y1]) { empty(g, j); g.flags[`due:${j}`] = 56; }
    month(g, 62);
    const saved = saveGame(g);
    const mark = markFor(g);
    month(g, 63);
    const lines = struckOf(g, 63);
    expect([...lines].sort()).toEqual([TWO_K, TWO_Y].sort());
    onBrowserCard(g, mark, lines, rowsWaitingOnRows(g, 'JAP'));
    onBridgeCard(loadGame(saved)!, lines);
  });

  it("'frontplace': a row first held at the front, then for its place: the struck line with the place hold's month, first among the home lines", () => {
    const g = make(); lose(g, 'Kaifeng'); g.flags[`due:${M}`] = 55;
    const b = g.corps.find((x) => x.nat === 'JAP' && !x.dead && x.name === BRIGADES)!;
    relocate(g, b, g.byName.Hsuchow); g.lastBattle = { [b.reg]: [b.id] };
    month(g, 61);
    kill(g, BRIGADES);
    for (let t = 62; t <= 68; t++) month(g, t);
    expect([g.flags[`rowTold:${M}`], g.flags[`rowPlaceSince:${M}`]]).toEqual([61, 62]);
    // (The North China Area Army (27th Division)'s row, made from this one and told in month 61, is struck at its own
    // half year in month 68: its line is on the card the bridge opens on this save.)
    expect(struckOf(g, 68)).toEqual(['The North China Area Army (27th Division), due in Kaifeng since September 1944, will not form: Kaifeng is still not in friendly hands, and the arrival has waited as long as it may.']);
    const saved = saveGame(g);
    const mark = markFor(g);
    month(g, 69);
    expect(struckOf(g, 69)).toEqual([FRONTPLACE]);
    onBrowserCard(g, mark, [FRONTPLACE], rowsWaitingOnRows(g, 'JAP'));
    onBridgeCard(loadGame(saved)!, [FRONTPLACE]);
  });
});

describe("every struck line first, then the rows still waiting (the UI director's section 12, A2)", () => {
  // (Her A2, 16:07Z, 27 November 2026: "Pass: every struck line first, then the rows still waiting. 'Takes the place
  // of' in 11 (a) meant its rank, first among the home lines." The round 2 verifier's V2: 'frontplace', month 68.)
  const NCAA_WAIT = 'The North China Area Army (27th Division), due in Kaifeng since September 1944, waits until Kaifeng is in friendly hands.';
  const NCAA_STRUCK = 'The North China Area Army (27th Division), due in Kaifeng since September 1944, will not form: Kaifeng is still not in friendly hands, and the arrival has waited as long as it may.';
  const M_WAIT = 'The 12th Army (62nd Division), due in Kaifeng since October 1944, waits until Kaifeng is in friendly hands.';

  it("'frontplace', month 68: a struck line whose muted line stood second goes ahead of the line that stood first, on both cards", () => {
    const g = make(); lose(g, 'Kaifeng'); g.flags[`due:${M}`] = 55;
    const b = g.corps.find((x) => x.nat === 'JAP' && !x.dead && x.name === BRIGADES)!;
    relocate(g, b, g.byName.Hsuchow); g.lastBattle = { [b.reg]: [b.id] };
    month(g, 61);
    kill(g, BRIGADES);
    for (let t = 62; t <= 67; t++) month(g, t);
    // (The month before: the 12th Army's line first, the North China Area Army's after it.)
    const before = rowsWaitingOnRows(g, 'JAP');
    expect(before.indexOf(M_WAIT)).toBeGreaterThanOrEqual(0);
    expect(before.indexOf(M_WAIT)).toBeLessThan(before.indexOf(NCAA_WAIT));
    const saved = saveGame({ ...g, phase: g.order.indexOf('JAP') });
    const mark = markFor(g);
    month(g, 68);
    expect(struckOf(g, 68)).toEqual([NCAA_STRUCK]);
    const waits = monthCard(g, mark, 'JAP').waits ?? [];
    expect(waits.slice(0, 2)).toEqual([NCAA_STRUCK, M_WAIT]);
    TC().load(saved);
    const res = TC().endMonth() as { month: Shown };
    expect(res.month.home.slice(0, 2).map((x) => [x.text, x.grey])).toEqual([[NCAA_STRUCK, true], [M_WAIT, true]]);
  });
});

describe('said once, and nothing where nothing was told', () => {
  it('the month after the strike reads no struck line on either card', () => {
    const g = make(); lose(g, 'Kaifeng'); kill(g, BRIGADES); g.flags[`due:${M}`] = 55;
    month(g, 61); month(g, 62);
    const mark = markFor(g);
    month(g, 63);
    const card = monthCard(g, mark, 'JAP');
    expect((card.waits ?? []).some((x) => x.endsWith(TAIL))).toBe(false);
    expect(homeOf(html(card)).some((x) => x.endsWith(TAIL))).toBe(false);
    expect(g.headlines.filter((h) => h.text === ONE)).toHaveLength(1);
  });

  it('told, then its place retaken: it forms, and no struck line is on any card', () => {
    const g = make(); lose(g, 'Kaifeng'); kill(g, BRIGADES); g.flags[`due:${M}`] = 55;
    month(g, 61);
    g.byName.Kaifeng.ctrl = 'JAP';
    const mark = markFor(g);
    month(g, 62);
    expect(g.headlines.some((h) => h.text.endsWith(TAIL))).toBe(false);
    const card = monthCard(g, mark, 'JAP');
    expect((card.waits ?? []).some((x) => x.endsWith(TAIL))).toBe(false);
    expect(homeOf(html(card)).some((x) => x.endsWith(TAIL))).toBe(false);
  });

  it('the switch off and a computer\'s row: struck with no line, and no card says a word of it, as today', () => {
    for (const g of [(() => { const x = make(false); lose(x, 'Kaifeng'); kill(x, BRIGADES); x.flags[`due:${M}`] = 55; return x; })(),
      (() => { const x = make(true, 'CHN'); lose(x, 'Kaifeng'); kill(x, BRIGADES); x.flags[`due:${M}`] = 55; return x; })()]) {
      month(g, 61);
      const mark = markFor(g);
      month(g, 62);
      expect(g.flags[`sent:${M}`]).toBe(2);
      expect(g.headlines.some((h) => /friendly hands/.test(h.text))).toBe(false);
      const card = monthCard(g, mark, g.human);
      expect((card.waits ?? []).some((x) => /friendly hands/.test(x))).toBe(false);
      expect(homeOf(html(card)).some((x) => /friendly hands/.test(x))).toBe(false);
    }
  });
});

describe("once: only the strike month's own card (the UI director's section 12, A1)", () => {
  // (Her A1, 16:07Z, 27 November 2026: "Only the strike month's own card puts the struck line in the waiting line's
  // place. A loaded game's first card neither shows nor counts an older struck line; it stays in the Newsreel.")
  const one = (): Game => { const g = make(); lose(g, 'Kaifeng'); kill(g, BRIGADES); g.flags[`due:${M}`] = 55; return g; };

  it("the sceptic's case: saved in the strike month and loaded, the next card has no struck line, does not count it, and the Newsreel holds it", () => {
    const g = one();
    month(g, 61);
    const saved = saveGame({ ...g, phase: g.order.indexOf('JAP') });
    month(g, 62);
    expect(struckOf(g, 62)).toEqual([ONE]);
    // (Unbroken, on the bridge: the strike month's own card has it first, and the card after, played on, has none.)
    onBridgeCard(loadGame(saved)!, [ONE]);
    const next = TC().endMonth() as { month: Shown };
    expect(next.month.home.some((x) => x.text.endsWith(TAIL))).toBe(false);
    // (Loaded in the strike month, on the bridge: the loaded game's first card.)
    notOnLoadedCard(g, [ONE]);
    // (Loaded in the strike month, in the browser: ui/main.ts takes the mark as the game is loaded, `markFor`.)
    const b = loadGame(saveGame(g))!;
    const mark = markFor(b);
    month(b, 63);
    const card = monthCard(b, mark, 'JAP');
    expect((card.waits ?? []).some((x) => x.endsWith(TAIL))).toBe(false);
    expect(homeOf(html(card)).some((x) => x.endsWith(TAIL))).toBe(false);
    expect(b.headlines.filter((h) => h.text === ONE)).toHaveLength(1);
  });

  it('saved a month after the strike and loaded: no struck line on the next card, and the Newsreel holds it', () => {
    const g = one();
    month(g, 61); month(g, 62); month(g, 63);
    g.phase = g.order.indexOf('JAP');
    TC().load(saveGame(g));
    const res = TC().endMonth() as { month: Shown };
    expect(res.month.home.some((x) => x.text.endsWith(TAIL) || x.tip.endsWith(TAIL))).toBe(false);
    const after = loadGame((TC().save() as { text: string }).text)!;
    expect(after.headlines.filter((h) => h.text === ONE)).toHaveLength(1);
  });

  it("'three': saved in the strike month and loaded, the grouped line is not on the next card and not counted", () => {
    const g = make(); lose(g, 'Kaifeng'); for (const j of K3) { empty(g, j); g.flags[`due:${j}`] = 56; }
    month(g, 62); month(g, 63);
    expect(struckOf(g, 63)).toEqual([THREE]);
    notOnLoadedCard(g, [THREE]);
  });

  it("'frontplace': saved in the strike month (69) and loaded, neither 68's line nor 69's is on the next card", () => {
    const g = make(); lose(g, 'Kaifeng'); g.flags[`due:${M}`] = 55;
    const b = g.corps.find((x) => x.nat === 'JAP' && !x.dead && x.name === BRIGADES)!;
    relocate(g, b, g.byName.Hsuchow); g.lastBattle = { [b.reg]: [b.id] };
    month(g, 61);
    kill(g, BRIGADES);
    for (let t = 62; t <= 69; t++) month(g, t);
    expect(struckOf(g, 69)).toEqual([FRONTPLACE]);
    notOnLoadedCard(g, [FRONTPLACE]);
  });
});

describe("S6: a told place hold the row can no longer form from: its muted line does not stand on (the UI director's section 11, point 3 of the thread's ruling)", () => {
  // (No row of the setup that takes named corps whole has an 'until', and none of Japan's has a war of its own: the
  // row's words are set by hand here, and set back after.)
  const WAIT = 'The 12th Army (62nd Division), due in Kaifeng since March 1944, waits until Kaifeng is in friendly hands.';
  const withWords = (words: Record<string, unknown>, f: () => void): void => {
    const was = rows[M][6];
    rows[M][6] = { ...was, ...words };
    try { f(); } finally { rows[M][6] = was; }
  };
  const kaifeng = (): Game => { const g = make(); lose(g, 'Kaifeng'); kill(g, BRIGADES); g.flags[`due:${M}`] = 55; return g; };

  it("its 'until' passes mid-hold (month 58): the muted line goes that month, on the rules' card and the browser's; the strike at the half year is as before", () => {
    withWords({ until: 58 }, () => {
      const g = kaifeng();
      month(g, 55); month(g, 56); month(g, 57);
      expect(rowsWaitingOnRows(g, 'JAP')).toContain(WAIT);
      const mark = markFor(g);
      month(g, 58);
      expect(g.flags[`rowPlace:${M}`]).toBe(g.byName.Kaifeng.id);
      expect(rowsWaitingOnRows(g, 'JAP')).not.toContain(WAIT);
      const card = monthCard(g, mark, 'JAP');
      expect(card.waits ?? []).not.toContain(WAIT);
      expect(homeOf(html(card))).not.toContain(WAIT);
      for (let t = 59; t <= 61; t++) { month(g, t); expect(rowsWaitingOnRows(g, 'JAP')).not.toContain(WAIT); }
      month(g, 62);
      expect(g.flags[`sent:${M}`]).toBe(2);
      expect(struckOf(g, 62)).toEqual(['The 12th Army (62nd Division), due in Kaifeng since March 1944, will not form: Kaifeng is still not in friendly hands, and the arrival has waited as long as it may.']);
    });
  });

  it('its war off while its flags stand: the reader skips it, and writes nothing', () => {
    const g = kaifeng();
    month(g, 55);
    expect(rowsWaitingOnRows(g, 'JAP')).toContain(WAIT);
    withWords({ war: ['JAP', 'SOV'] }, () => {
      expect(g.nations.JAP.atWar.includes('SOV')).toBe(false);
      const before = JSON.stringify(g.flags);
      expect(rowsWaitingOnRows(g, 'JAP')).not.toContain(WAIT);
      expect(JSON.stringify(g.flags)).toBe(before);
    });
    expect(rowsWaitingOnRows(g, 'JAP')).toContain(WAIT);
  });

  it('its war (Japan and China, set by hand) ends in peace mid-hold: struck at once with its flags, and no line stands on, as before', () => {
    withWords({ war: ['JAP', 'CHN'] }, () => {
      const g = kaifeng();
      month(g, 55); month(g, 56);
      expect(rowsWaitingOnRows(g, 'JAP')).toContain(WAIT);
      expect(signPeace(g, 'JAP', 'CHN')).toBe(true);
      month(g, 57);
      expect(g.flags[`sent:${M}`]).toBe(2);
      expect(rowsWaitingOnRows(g, 'JAP')).not.toContain(WAIT);
      expect(g.headlines.some((h) => h.text.endsWith(TAIL))).toBe(false);
    });
  });
});
