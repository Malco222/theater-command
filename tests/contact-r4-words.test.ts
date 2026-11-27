// The contact rule, round 4, the words (W4-1 to W4-4): the UI director's marks of 16:07Z, 27 November 2026, section 12
// of pacific-japan/ui-words-row-waits-contact-UI-MARKS.md, part B, every sentence copied from it character for character.
// The brief: /home/user/runs/contact2/R4-BRIEF.md, "What the WORDS builder lays". All under the switch 'fromWait'.
// The far side on for the whole file (TC_FAR), as tests/contact-r4-marks.test.ts: Germany's test corps on the Grenzmark,
// Poland's Army Poznan beside it in Poznan, Germany seated.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { arrivalWaitOf, loanHeldLine, setFromWait } from '../src/campaign/arrivalwaits';
import { divisionsOf } from '../src/campaign/armies';
import { recordBattle } from '../src/campaign/drawn';
import { addCorps, atWar, createGame, relocate, startPhase, theWords } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { farOn } from '../src/campaign/world';
import type { Corps, Game, Region } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';
import { asideLines } from '../unreal/TheaterCommand/Rules/offers-look';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;

const ROWS = (setup as any).expeditions as any[];
let added = 0;
const addRow = (row: any[]): number => { ROWS.push(row); added++; return ROWS.length - 1; };
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } });
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'GER'); };
const said = (g: Game, name: string): string[] => g.headlines.filter((h: any) => h.text.includes(name) && /lent to the campaign|lay down its arms/.test(h.text)).map((h: any) => h.text);

/**
 * A lent corps of Germany's, its loan over in month 2, on the Grenzmark beside Poland (the far side on), the player
 * Germany. `drawnFrom`: its divisions drawn from living corps standing beside it on the Grenzmark (so its loan's end
 * sends them back into those corps, `rowOver`), and it fought the last battle there (the one test's fight half holds
 * it, as the corps it came out of keeps it from being alone).
 */
function board(o: { on?: boolean; words?: any; awaited?: boolean; drawnFrom?: string[]; divs?: number } = {}): { g: Game; j: number; c: Corps; gz: Region; parents: Corps[] } {
  const g = startOpen(createGame(3, null));
  expect(farOn(g)).toBe(true);
  g.human = 'GER';
  g.flags.offers = 1;
  if (o.on ?? true) expect(setFromWait(g, true)).toBe(true);
  const gz = g.byName.Grenzmark;
  for (const u of [...gz.units]) relocate(g, u, g.byName.Bavaria);
  expect(atWar(g, 'GER', 'POL')).toBe(true);
  const c = addCorps(g, 'GER', 'Lent Test Corps', 'inf', o.divs ?? 1, gz.id, {});
  const j = addRow([1, 'GER', 'Lent Test Corps', 'inf', o.divs ?? 1, 'Grenzmark', { until: 2, ...(o.words ?? {}) }]);
  if (o.awaited) addRow([9, 'GER', 'Test Tank Division', 'arm', 1, 'Magdeburg', { awaits: ['Lent Test Corps'], str: 1 }]);
  g.flags[`sent:${j}`] = 1;
  g.flags[`lent:${j}`] = c.id;
  const parents: Corps[] = [];
  if (o.drawnFrom) {
    const ds = divisionsOf(g, c);
    expect(ds).toHaveLength(o.drawnFrom.length);
    for (const name of o.drawnFrom) parents.push(addCorps(g, 'GER', name, 'inf', 1, gz.id, {}));
    c.drew = ds.map((d, k) => ({ name: [d.name, d.short] as [string, string], no: d.no, from: parents[k].id }));
    recordBattle(g, gz, [c, ...g.byName.Poznan.units.filter((u) => u.nat === 'POL')]);
  }
  return { g, j, c, gz, parents };
}

// ---- W4-1 (her B1): the "goes back" headline -----------------------------------------------------------------------

describe('W4-1: the loan-hold headline for a corps whose divisions go back, in her B1 form', () => {
  it('the rule knows the corps it came out of: named, "is due this month to go back into {the corps}"', () => {
    const { g, c } = board({ drawnFrom: ['Parent Test Corps'] });
    month(g, 2);
    expect(c.dead).toBe(false);
    expect(said(g, 'Lent Test Corps')).toEqual(['The Lent Test Corps, lent to the campaign, is due this month to go back into the Parent Test Corps, but it is at the front in the Grenzmark. It stays, and goes back the month after it leaves the front.']);
    expect(theWords(loanHeldLine(g, c, 'back'))).toBe(said(g, 'Lent Test Corps')[0]);
  });

  it('two corps it came out of: the rule cannot name one, "is due this month to go back to the corps it came out of"', () => {
    const { g, c } = board({ drawnFrom: ['Parent Test Corps A', 'Parent Test Corps B'], divs: 2 });
    month(g, 2);
    expect(c.dead).toBe(false);
    expect(said(g, 'Lent Test Corps')).toEqual(['The Lent Test Corps, lent to the campaign, is due this month to go back to the corps it came out of, but it is at the front in the Grenzmark. It stays, and goes back the month after it leaves the front.']);
  });

  // (The other four stand as round 3 printed them: gone; home with an offer and without; down. Each on a board of its own.)
  const AT = 'but it is at the front in the Grenzmark.';
  it('leaves for other fronts: as printed', () => {
    const { g } = board();
    month(g, 2);
    expect(said(g, 'Lent Test Corps')).toEqual([`The Lent Test Corps, lent to the campaign, is due to leave for other fronts this month, ${AT} It stays, and goes the month after it leaves the front.`]);
  });
  it('the regiment whose end is an offer: as printed', () => {
    const { g } = board({ awaited: true });
    month(g, 2);
    expect(said(g, 'Lent Test Corps')).toEqual([`The Lent Test Corps, lent to the campaign, has come to the end of its loan this month, ${AT} It stays, and you are asked whether to let it go home to its division the month after it leaves the front.`]);
  });
  it('the same, offers off: as printed', () => {
    const { g } = board({ awaited: true });
    delete g.flags.offers;
    month(g, 2);
    expect(said(g, 'Lent Test Corps')).toEqual([`The Lent Test Corps, lent to the campaign, has come to the end of its loan this month, ${AT} It stays, and goes home to its division the month after it leaves the front.`]);
  });
  it('due to lay down its arms: as printed', () => {
    const { g } = board({ words: { ends: 'surrender' } });
    month(g, 2);
    expect(said(g, 'Lent Test Corps')).toEqual([`The Lent Test Corps is due to lay down its arms this month, ${AT} It fights on, and lays them down the month after it leaves the front.`]);
  });
});

// ---- W4-2 (her B2): a held loan's standing line on the corps' own cards ----------------------------------------------

describe("W4-2: a held loan carries a standing line on the corps' own cards every month it is held, built as section 2's", () => {
  // (Her B2: tray "Its loan is over", its hover the whole line; record card and 3D note by the way the loan ends.)
  const line = (whole: string): { short: string; whole: string } => ({ short: 'Its loan is over', whole });
  const relieve = (g: Game, gz: Region): Corps => addCorps(g, 'GER', 'Relief Test Corps', 'inf', 2, gz.id, {});
  const flagsOf = (g: Game): string => JSON.stringify(g.flags);

  it('leaves for other fronts: the line from the month the hold begins, every month it is held, gone the month the loan ends', () => {
    const { g, c, gz } = board();
    expect(arrivalWaitOf(g, c)).toBeNull();
    month(g, 2);
    const was = flagsOf(g);
    expect(arrivalWaitOf(g, c)).toEqual(line('This corps was due to leave for other fronts in October 1939. It goes the month after it leaves the front.'));
    // (A pure read: nothing written to the save for it.)
    expect(flagsOf(g)).toBe(was);
    month(g, 3);
    expect(arrivalWaitOf(g, c)).toEqual(line('This corps was due to leave for other fronts in October 1939. It goes the month after it leaves the front.'));
    relieve(g, gz);
    month(g, 4);
    expect(c.dead).toBe(true);
    expect(arrivalWaitOf(g, c)).toBeNull();
  });

  it('goes back into the corps it came out of: her "go back into {the corps}" line', () => {
    const { g, c } = board({ drawnFrom: ['Parent Test Corps'] });
    month(g, 2);
    expect(arrivalWaitOf(g, c)).toEqual(line('This corps was due to go back into the Parent Test Corps in October 1939. It goes back the month after it leaves the front.'));
  });

  it('home to its division, his offer to follow: "You are asked then whether to let it go."; the line stops the month the offer is made', () => {
    const { g, c, j, gz } = board({ awaited: true });
    month(g, 2);
    expect(arrivalWaitOf(g, c)).toEqual(line('This corps was due to go home to its division in October 1939. It goes the month after it leaves the front. You are asked then whether to let it go.'));
    relieve(g, gz);
    month(g, 3);
    expect([c.dead, g.flags[`leave:${j}`]]).toEqual([false, 3]);
    expect(arrivalWaitOf(g, c)).toBeNull();
  });

  it('home to its division with offers off: no offer follows', () => {
    const { g, c } = board({ awaited: true });
    delete g.flags.offers;
    month(g, 2);
    expect(arrivalWaitOf(g, c)).toEqual(line('This corps was due to go home to its division in October 1939. It goes the month after it leaves the front.'));
  });

  it('the line stops with the corps (her B3: the memorial line is enough)', () => {
    const { g, c } = board();
    month(g, 2);
    expect(arrivalWaitOf(g, c)).not.toBeNull();
    c.dead = true;
    expect(arrivalWaitOf(g, c)).toBeNull();
  });

  it('switch off: no line; and a loan never held has none', () => {
    const off = board({ on: false });
    month(off.g, 2);
    expect(arrivalWaitOf(off.g, off.c)).toBeNull();
    const { g, c, gz } = board();
    relieve(g, gz);
    month(g, 2);
    expect(arrivalWaitOf(g, c)).toBeNull();
    const held = board();
    month(held.g, 2);
    expect(setFromWait(held.g, false)).toBe(true);
    expect(arrivalWaitOf(held.g, held.c)).toBeNull();
  });

  it("the 3D corps card's muted note carries the whole line (the bridge's view, `arrivalWait`)", () => {
    const { g, c } = board();
    month(g, 2);
    g.phase = g.order.indexOf('GER');
    TC().load(saveGame(g));
    const v = TC().view() as { corps: { name: string; nat: string; arrivalWait?: string }[] };
    expect(v.corps.filter((x) => x.nat === 'GER' && x.name === c.name).map((x) => x.arrivalWait)).toEqual(['This corps was due to leave for other fronts in October 1939. It goes the month after it leaves the front.']);
  });
});

// ---- W4-4 (her B6): the offers page's loans line for a held loan ----------------------------------------------------

describe("W4-4: the offers page's loans line names no month that has passed: for a held loan the loan's month gives way to her words", () => {
  /** A second loan of his, away from the front, whose end is an offer (a regiment its division awaits): it opens the offers page. */
  const offer = (g: Game): number => {
    addCorps(g, 'GER', 'Lent Test Regiment', 'arm', 1, g.byName.Bavaria.id, {});
    const k = addRow([1, 'GER', 'Lent Test Regiment', 'arm', 1, 'Bavaria', { until: 2 }]);
    addRow([9, 'GER', 'Test Tank Division', 'arm', 1, 'Magdeburg', { awaits: ['Lent Test Regiment'], str: 1 }]);
    g.flags[`sent:${k}`] = 1;
    g.flags[`lent:${k}`] = g.corps.find((x) => x.name === 'Lent Test Regiment')!.id;
    return k;
  };
  const page = (g: Game): { heading: string; text: string }[] => {
    g.phase = g.order.indexOf('GER');
    TC().load(saveGame(g));
    const p = TC().page('offers') as { sections: { heading: string; text: string }[] } | null;
    return (p?.sections ?? []).filter((x) => x.heading === 'LENT TO YOU').map(({ heading, text }) => ({ heading, text }));
  };

  it('before its end the line names the month; held, the month gives way: the whole line, with its head, on the page', () => {
    const { g } = board();
    offer(g);
    month(g, 1);
    expect(asideLines(g).loans).toEqual(['Lent Test Corps leaves your command by itself in October 1939, when its loan ends.']);
    month(g, 2);
    expect(asideLines(g).loans).toEqual(['Lent Test Corps leaves your command by itself: its loan is over, but it is at the front in the Grenzmark: it goes the month after it leaves the front.']);
    expect(page(g)).toEqual([{ heading: 'LENT TO YOU', text: 'Lent Test Corps leaves your command by itself: its loan is over, but it is at the front in the Grenzmark: it goes the month after it leaves the front.' }]);
  });

  it("the switch turned off in the hold's month: the base's line, its month kept", () => {
    const { g } = board();
    offer(g);
    month(g, 2);
    expect(setFromWait(g, false)).toBe(true);
    expect(asideLines(g).loans).toEqual(['Lent Test Corps leaves your command by itself in October 1939, when its loan ends.']);
  });
});
