// The contact rule, round 4, the words (W4-1 to W4-4): the UI director's marks of 16:07Z, 27 November 2026, section 12
// of pacific-japan/ui-words-row-waits-contact-UI-MARKS.md, part B, every sentence copied from it character for character.
// The brief: /home/user/runs/contact2/R4-BRIEF.md, "What the WORDS builder lays". All under the switch 'fromWait'.
// The far side on for the whole file (TC_FAR), as tests/contact-r4-marks.test.ts: Germany's test corps on the Grenzmark,
// Poland's Army Poznan beside it in Poznan, Germany seated.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { loanHeldLine, setFromWait } from '../src/campaign/arrivalwaits';
import { divisionsOf } from '../src/campaign/armies';
import { recordBattle } from '../src/campaign/drawn';
import { addCorps, atWar, createGame, relocate, startPhase, theWords } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { farOn } from '../src/campaign/world';
import type { Corps, Game, Region } from '../src/campaign/types';

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
