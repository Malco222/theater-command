// The telling of a row held because its place is not in friendly hands (arrivalwaits.ts `tellPlace`, `placeHeldLine`,
// `rowsWaitingOnRows`, `formedLine`). The UI director's section 9, 12:48Z, 26 November 2026
// (pacific-japan/ui-words-row-waits-contact-UI-MARKS.md), each line to the letter: the headline once, in the month the
// hold begins; the month's report card's muted line while it waits; the arrival headline's clause the month it forms.
// The design director, 06:10Z: "Tell the maker's hold for its place like any wait; if the telling reads only written
// waits, the hold writes one." The telling reads its own flags, so the hold writes NO `wait:<i>` (spec 5.4). Nothing is
// told with the switch off, nor for a computer's row. A lost place together with another cause in one month is not
// worded (her section 9): the other cause is told as today.
//
// The board is seed 34's (tests/chain-maker-state): the 12th Army (62nd Division) due at Kaifeng from month 55, made
// of the 1st Army (independent brigades, Taiyuan); Kaifeng China's with its own corps gone to Hsuchow; Japan the
// player's, the far side on, stop 3, the switch on. With his corps on the board his row for a lost posting is formed at
// the nearest harbour his side holds (game.ts `postedFor`, ruling B7: here Haichow) and never waits for its place; so
// the brigades are gone from the board (destroyed), and the row, with nobody of his to take, waits for Kaifeng itself.
// The states are set by hand.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { arrivalWaitOf, nationDone, rowsWaitingOnRows, setFromWait } from '../src/campaign/arrivalwaits';
import { corpsInContact } from '../src/campaign/drawn';
import { createGame, declareWar, relocate, removeCorps, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { markFor, monthCard } from '../src/campaign/monthcard';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { storyFallen } from '../src/campaign/stories';
import { monthCardHtml } from '../src/campaign/ui/monthview';
import type { Corps, Game } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
type Home = { text: string; grey: boolean; tip: string };

const MAKER = '12th Army (62nd Division)', CHAINED = '32nd Army (62nd Division)', BRIGADES = '1st Army (independent brigades, Taiyuan)';
const rows = (setup as any).expeditions as any[][];
const index = (name: string): number => rows.findIndex((r) => r[1] === 'JAP' && r[2] === name);
const M = index(MAKER), C = index(CHAINED);

// Her three lines, to the letter, with this board's slots (month 60 is August 1944).
const HEADLINE = 'The 12th Army (62nd Division) is due in Kaifeng this month, but China holds it, so the arrival waits until it is in friendly hands.';
const CARD = 'The 12th Army (62nd Division), due in Kaifeng since August 1944, waits until Kaifeng is in friendly hands.';
// (A row first told it waits for its corps at the front in month 60, held for its place from month 61, September 1944.)
const CARD_LATER = 'The 12th Army (62nd Division), due in Kaifeng since September 1944, waits until Kaifeng is in friendly hands.';
const FORMED = 'The 12th Army (62nd Division) has arrived in Kaifeng, now in friendly hands.';
const FORMED_LOST = 'The 12th Army (62nd Division) has arrived in Kaifeng, now in friendly hands, formed without your 1st Army (independent brigades, Taiyuan), which has been lost.';
// Today's lines, for the other cause in the same month (not worded) and for the switch off. His row, its corps at the
// front, is placed at Haichow (`postedFor`) and told the front there, as today.
const FRONT = 'The 12th Army (62nd Division) is due in Haichow this month, formed from your 1st Army (independent brigades, Taiyuan), which is at the front in Hsuchow. The arrival waits, and forms the month after that corps leaves the front.';

const brigades = (g: Game): Corps | undefined => g.corps.find((x) => x.nat === 'JAP' && !x.dead && x.name === BRIGADES);
/** The brigades destroyed, as a battle destroys a corps (on the memorial wall), and struck off the board. */
const destroy = (g: Game): void => { const b = brigades(g)!; storyFallen(g, b, 'destroyed', 'CHN', b.reg); removeCorps(g, b); };
/** Seed 34, `human` seated, stop 3, the switch `on`; Kaifeng China's and emptied; the brigades gone unless `keep`. */
const board = (on: boolean, human: string | null = 'JAP', keep = false): Game => {
  const g = startOpen(createGame(34, human));
  setStop(g, 3, true);
  if (on) setFromWait(g, true);
  const k = g.byName.Kaifeng;
  for (const u of [...k.units]) relocate(g, u, g.byName.Hsuchow);
  k.ctrl = 'CHN';
  if (keep) relocate(g, brigades(g)!, g.byName.Tokyo); else destroy(g);
  declareWar(g, 'JAP', 'USA');
  g.flags[`due:${M}`] = 55;
  return g;
};
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'JAP'); };
/** The brigades at Hsuchow, beside China's Kaifeng, having fought the battle last resolved there: at the front. */
const toFront = (g: Game): void => {
  const b = brigades(g)!;
  relocate(g, b, g.byName.Hsuchow);
  g.lastBattle = { [b.reg]: [b.id] };
  expect(corpsInContact(g, 'JAP', b)).toBe(true);
};
/** The headlines whose subject is the Kaifeng row (the Okinawa row's lines name it too, after their own subject). */
const about = (g: Game, t?: number): string[] => g.headlines.filter((h) => (t === undefined || h.turn === t) && /^(The )?12th Army \(62nd Division\)/.test(h.text)).map((h) => h.text);
const flags = (g: Game): Record<string, number | undefined> => ({ told: g.flags[`rowTold:${M}`], at: g.flags[`rowAt:${M}`], place: g.flags[`rowPlace:${M}`], wait: g.flags[`wait:${M}`], sent: g.flags[`sent:${M}`] });

describe('the headline, once, in the month the hold begins', () => {
  it('month 60: her sentence, the holder named and the place said once; no wait is written', () => {
    const g = board(true);
    month(g, 60);
    expect(about(g, 60)).toEqual([HEADLINE]);
    const h = g.headlines.find((x) => x.text === HEADLINE)!;
    expect([h.kind, h.nat, h.region]).toEqual(['info', 'JAP', g.byName.Kaifeng.id]);
    expect(flags(g)).toEqual({ told: 60, at: g.byName.Kaifeng.id, place: g.byName.Kaifeng.id, wait: undefined, sent: undefined });
  });

  it('not again while it waits', () => {
    const g = board(true);
    month(g, 60); month(g, 61);
    expect(g.headlines.filter((x) => x.text === HEADLINE)).toHaveLength(1);
    expect(about(g, 61)).toEqual([]);
    expect(flags(g)).toMatchObject({ told: 60, place: g.byName.Kaifeng.id, wait: undefined });
  });
});

describe("while it waits: the month's report card carries it, muted, from one maker on the page, the month card and the bridge", () => {
  it('the rules\' line, beside the line of the row made from it', () => {
    const g = board(true);
    month(g, 60); month(g, 61);
    // (The North China Area Army (27th Division)'s row, also due at Kaifeng, waits for it too, and the 11th Army (27th
    // Division)'s for that row: their lines stand beside these.)
    const lines = rowsWaitingOnRows(g, 'JAP');
    expect(lines).toContain(CARD);
    expect(lines).toContain('The 32nd Army (62nd Division), due in Okinawa, has waited since August 1944 for the 12th Army (62nd Division), which has not formed yet.');
    expect(lines.filter((x) => x === CARD)).toHaveLength(1);
    expect(g.flags[`rowTold:${C}`]).toBe(60);
  });

  it('the month card, the browser\'s card and the bridge\'s card show the same sentence', () => {
    const g = board(true);
    month(g, 60);
    const card = monthCard(g, markFor(g), 'JAP');
    expect(card.waits).toContain(CARD);
    const esc = (s: string): string => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
    expect(monthCardHtml(card, { esc, flag: () => '', name: (x: string) => x, all: false, orders: true } as any)).toContain(esc(CARD));
    g.phase = g.order.indexOf('JAP');
    TC().load(saveGame(g));
    expect((TC().openingCard() as { ok: boolean }).ok).toBe(true);
    const home = (TC().page('reports') as { month: { home: Home[] } }).month.home;
    expect(home.filter((x) => x.text === CARD)).toEqual([expect.objectContaining({ text: CARD, grey: true, tip: CARD })]);
  });

  it('read from the board: the line goes once the place is in friendly hands', () => {
    const g = board(true);
    month(g, 60);
    g.byName.Kaifeng.ctrl = 'JAP';
    expect(rowsWaitingOnRows(g, 'JAP')).not.toContain(CARD);
  });
});

describe('the month it forms', () => {
  it('Kaifeng back in month 61: today\'s arrival headline with the place\'s clause after the place; the flags go', () => {
    const g = board(true);
    month(g, 60);
    g.byName.Kaifeng.ctrl = 'JAP';
    month(g, 61);
    expect(about(g, 61)).toEqual([FORMED]);
    expect(flags(g)).toEqual({ told: undefined, at: undefined, place: undefined, wait: undefined, sent: 1 });
    expect(rowsWaitingOnRows(g, 'JAP')).not.toContain(CARD);
  });

  it('a row that waited on the front, its corps then lost: told each once, and formed with the "formed ..." clause following as now', () => {
    const g = board(true, 'JAP', true);
    toFront(g);
    month(g, 60);
    expect(about(g, 60)).toEqual([FRONT]);
    destroy(g);
    month(g, 61);
    expect(about(g, 61)).toEqual([HEADLINE]);
    // (The card's month is the month the place hold began, the month that headline was said: the UI director's section
    // 10, Q3, 00:39Z, 27 November 2026. The row's own month, the front wait's, stays for every other reader of it:
    // `rowTold` is still 60, so the memorial wall's "since the row was told" below still finds the brigades.)
    expect(rowsWaitingOnRows(g, 'JAP')).toContain(CARD_LATER);
    expect(rowsWaitingOnRows(g, 'JAP')).not.toContain(CARD);
    expect([g.flags[`rowTold:${M}`], g.flags[`rowPlaceSince:${M}`]]).toEqual([60, 61]);
    g.byName.Kaifeng.ctrl = 'JAP';
    month(g, 62);
    expect(about(g, 62)).toEqual([FORMED_LOST]);
    expect(Object.keys(g.flags).filter((k) => /^row[A-Z]\w*:/.test(k) && k.split(':')[1] === String(M))).toEqual([]);
  });
});

describe("the place hold's month (Q3): its own flag, `rowPlaceSince`", () => {
  it('a row held only for its place: the hold\'s month is the month it was told, and every reader reads it', () => {
    const g = board(true);
    month(g, 60);
    expect([g.flags[`rowTold:${M}`], g.flags[`rowPlaceSince:${M}`]]).toEqual([60, 60]);
    month(g, 61);
    expect(g.flags[`rowPlaceSince:${M}`]).toBe(60);
    expect(rowsWaitingOnRows(g, 'JAP')).toContain(CARD);
  });

  it('a save made before the flag (acf6ac794), its place hold already told: the card reads the row\'s month, nothing is told twice, and the flags go', () => {
    const g = board(true);
    month(g, 60);
    delete g.flags[`rowPlaceSince:${M}`];
    const text = saveGame(g);
    const h = loadGame(text)!;
    expect(rowsWaitingOnRows(h, 'JAP')).toContain(CARD);
    month(h, 61);
    expect(h.headlines.filter((x) => x.text === HEADLINE)).toHaveLength(1);
    expect(about(h, 61)).toEqual([]);
    expect(h.flags[`rowPlaceSince:${M}`]).toBeUndefined();
    expect(rowsWaitingOnRows(h, 'JAP')).toContain(CARD);
    // (Kaifeng back the next month: today's formed headline with the place's clause, and every flag of the row goes.)
    const k = loadGame(text)!;
    k.byName.Kaifeng.ctrl = 'JAP';
    month(k, 61);
    expect(about(k, 61)).toEqual([FORMED]);
    expect(Object.keys(k.flags).filter((x) => /^row[A-Z]\w*:/.test(x) && x.split(':')[1] === String(M))).toEqual([]);
  });

  it('his nation leaves the war: the flag goes with the others (`nationDone`)', () => {
    const g = board(true);
    month(g, 60);
    nationDone(g, 'JAP');
    expect(Object.keys(g.flags).filter((k) => /^row[A-Z]\w*:/.test(k) && k.split(':')[1] === String(M))).toEqual([]);
  });
});

describe('a lost place together with another cause (not worded): the other cause is told as today', () => {
  it('the brigades at the front and Kaifeng China\'s in one month: the front\'s headline and the corps\' line; no place line', () => {
    const g = board(true, 'JAP', true);
    toFront(g);
    month(g, 60);
    expect(about(g, 60)).toEqual([FRONT]);
    expect(g.flags[`rowPlace:${M}`]).toBeUndefined();
    expect(g.flags[`wait:${M}`]).toBe(60);
    expect(arrivalWaitOf(g, brigades(g)!)?.whole).toBe('An arrival due in Haichow, the 12th Army (62nd Division), is to be formed from this corps, and has waited for it since August 1944. It forms the month after this corps leaves the front.');
    expect(rowsWaitingOnRows(g, 'JAP').some((x) => x.startsWith('The 12th Army (62nd Division)'))).toBe(false);
  });

  it('a row that waits for another row to form, its own place lost too: the row not formed is told as today; no place line', () => {
    const g = board(true);
    const ok = g.byName.Okinawa;
    for (const u of [...ok.units]) relocate(g, u, g.byName.Tokyo);
    ok.ctrl = 'USA';
    month(g, 60);
    expect(g.headlines.filter((h) => h.turn === 60 && h.text.includes('32nd Army (62nd Division)')).map((h) => h.text)).toEqual([
      // (Okinawa lost, his row with none of his corps to take forms at home (`postedFor`): the chain holds it there.)
      'The 32nd Army (62nd Division) is due in Tokyo this month, formed from the 12th Army (62nd Division), which has not formed yet. The arrival waits for it.',
    ]);
    expect(g.flags[`rowPlace:${C}`]).toBeUndefined();
    expect(rowsWaitingOnRows(g, 'JAP')).toContain('The 32nd Army (62nd Division), due in Tokyo, has waited since August 1944 for the 12th Army (62nd Division), which has not formed yet.');
  });
});

describe('nothing told outside the telling\'s scope', () => {
  it('switch off: no headline, no flag, no card line; the month it comes is the base\'s', () => {
    const g = board(false);
    month(g, 60);
    expect(about(g)).toEqual([]);
    expect(flags(g)).toEqual({ told: undefined, at: undefined, place: undefined, wait: undefined, sent: undefined });
    expect(g.headlines.some((h) => /friendly hands/.test(h.text))).toBe(false);
    expect(Object.keys(g.flags).filter((k) => /^row(Told|At|Front|Id|Place|PlaceSince):/.test(k))).toEqual([]);
    expect(rowsWaitingOnRows(g, 'JAP')).toEqual([]);
    g.byName.Kaifeng.ctrl = 'JAP';
    month(g, 61);
    // (With the switch off the chain does not hold the Okinawa row: it came in month 60 and laid the 62nd, so the
    // Kaifeng row's month is the base's "laid once" line, and nothing of the telling.)
    expect(flags(g)).toEqual({ told: undefined, at: undefined, place: undefined, wait: undefined, sent: 1 });
    expect(g.headlines.filter((h) => h.turn === 61 && h.text.includes('Kaifeng')).map((h) => h.text)).toContain('The 62nd Division is due in Kaifeng this month, but you hold it already, in Okinawa. No division is raised twice: it goes to Kaifeng only if you send it.');
    expect(g.headlines.some((h) => /friendly hands/.test(h.text))).toBe(false);
  });

  it('a computer\'s row (China the player\'s): nothing is told and no telling flag is written', () => {
    const g = board(true, 'CHN');
    month(g, 60); month(g, 61);
    expect(about(g)).toEqual([]);
    expect(flags(g)).toEqual({ told: undefined, at: undefined, place: undefined, wait: undefined, sent: undefined });
  });
});
