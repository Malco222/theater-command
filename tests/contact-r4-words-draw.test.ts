// The contact rule, round 4, W4-3: the player's own theatre draw that passes over a corps at the front is TOLD, in
// section 5's form (the UI director's marks of 16:07Z, 27 November 2026, section 12 B5 of
// pacific-japan/ui-words-row-waits-contact-UI-MARKS.md: "Pass your lean, section 5's form: {Row} has {arrived in /
// landed at} {place} without your {corps}, which is at the front in {X} and stays a corps of its own. After a general's
// line or news: The arrival is formed without your {corps}, which is at the front in {X} and stays a corps of its own.
// (the tail kept, as section 5)."), and the rule's reader reports that hold told. Round 3's T3-1 board
// (tests/contact-r3-mend.test.ts): seed 1, Germany the player's, at war with Greece; a German corps alone on the Balkans'
// line beside free Greek armies; a dated row of Germany's drawn from the Balkans, due in Magdeburg.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { setFromWait } from '../src/campaign/arrivalwaits';
import { addCorps, atWar, createGame, declareWar, relocate, ruleHolds, startPhase, theatreName } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

const ROWS = (setup as any).expeditions as any[];
let added = 0;
const addRow = (row: any[]): number => { ROWS.push(row); added++; return ROWS.length - 1; };
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } });
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'GER'); };

function board(o: { on?: boolean; human?: boolean; words?: any; corps?: string[]; divs?: number } = {}): { g: Game; i: number; line: Region; lines: Region[]; held: Corps[] } {
  const g = startOpen(createGame(1, null));
  if (o.on ?? true) setFromWait(g, true);
  if (o.human ?? true) g.human = 'GER';
  declareWar(g, 'GER', 'GRE');
  const balkans = g.regions.filter((r) => theatreName(r) === 'the Balkans');
  const greek = (r: Region): boolean => r.ctrl === 'GRE' || r.units.some((u) => u.nat === 'GRE');
  // (Each corps alone on a province of the line of its own: two corps in one province are not alone, and neither is held.)
  const lines = balkans.filter((r) => !greek(r) && !atWar(g, 'GER', r.ctrl) && r.nb.some((j) => greek(g.regions[j])));
  const names = o.corps ?? ['Held Test Corps'];
  expect(lines.length).toBeGreaterThanOrEqual(names.length);
  // (As round 3's T3-1 board after R4-1: the Greek armies beside it free of their orders to hold, so they are the enemy beside.)
  const held = names.map((name, k) => {
    const line = lines[k];
    for (const j of line.nb) for (const u of g.regions[j].units) if (u.nat === 'GRE') u.hold = false;
    for (const u of [...line.units]) if (u.nat === 'GER') relocate(g, u, g.byName.Bavaria);
    return addCorps(g, 'GER', name, 'inf', 3, line.id, {});
  });
  const i = addRow([2, 'GER', 'Test Theatre Army', 'inf', o.divs ?? 1, 'Magdeburg', { from: 'the Balkans', ...(o.words ?? {}) }]);
  return { g, i, line: lines[0], lines, held };
}
const lines = (g: Game): string[] => g.headlines.filter((h: any) => h.turn === 2 && /Test Theatre Army|The arrival is formed/.test(h.text)).map((h: any) => h.text);
const draws = (g: Game): unknown[] => ruleHolds(g).filter((h) => h.path === 'draw').map(({ turn, i, nat, row, path, waits, written, ...h }) => h);

describe("W4-3: the player's own theatre draw that passes over a corps at the front is told, in section 5's form", () => {
  it('one corps: "{Row} has arrived in {place} without your {corps}, which is at the front in {X} and stays a corps of its own."; the reader: told', () => {
    const { g, line, held } = board();
    month(g, 2);
    expect(held[0].divs).toBe(3);
    expect(lines(g)).toEqual([`The Test Theatre Army has arrived in Magdeburg without your Held Test Corps, which is at the front in ${line.name} and stays a corps of its own.`]);
    expect(draws(g)).toEqual([{ corps: ['Held Test Corps'], told: true, untold: false }]);
  });

  it("after a general's line: \"The arrival is formed without your {corps}, which is at the front in {X} and stays a corps of its own.\" (the tail kept)", () => {
    const { g, line } = board({ words: { gen: 'General Test' } });
    month(g, 2);
    expect(lines(g)).toEqual([`General Test arrives in Magdeburg with the Test Theatre Army. The arrival is formed without your Held Test Corps, which is at the front in ${line.name} and stays a corps of its own.`]);
    expect(draws(g)).toEqual([{ corps: ['Held Test Corps'], told: true, untold: false }]);
  });

  it("after the row's own news: the same sentence follows it", () => {
    const { g, line } = board({ words: { news: 'The Test Theatre Army is formed.' } });
    month(g, 2);
    expect(lines(g)).toEqual([`The Test Theatre Army is formed. The arrival is formed without your Held Test Corps, which is at the front in ${line.name} and stays a corps of its own.`]);
    expect(draws(g)).toEqual([{ corps: ['Held Test Corps'], told: true, untold: false }]);
  });

  it("two corps: section 5's two form, \"without your {A} and your {B}, which are at the front in {X} and {Y} and stay corps of their own.\"", () => {
    const { g, lines: at } = board({ corps: ['Held Test Corps A', 'Held Test Corps B'], divs: 60 });
    month(g, 2);
    expect(lines(g)).toEqual([`The Test Theatre Army has arrived in Magdeburg without your Held Test Corps A and your Held Test Corps B, which are at the front in ${at[0].name} and ${at[1].name} and stay corps of their own.`]);
    expect(draws(g)).toEqual([{ corps: ['Held Test Corps A', 'Held Test Corps B'], told: true, untold: false }]);
  });

  it("two corps after a general's line: a form section 12 does not give, so nothing is added, and the reader says untold", () => {
    const { g } = board({ corps: ['Held Test Corps A', 'Held Test Corps B'], divs: 60, words: { gen: 'General Test' } });
    month(g, 2);
    expect(lines(g)).toEqual(['General Test arrives in Magdeburg with the Test Theatre Army.']);
    expect(draws(g)).toEqual([{ corps: ['Held Test Corps A', 'Held Test Corps B'], told: false, untold: true }]);
  });

  it("a computer's row: nothing said, told null", () => {
    const ai = board({ human: false });
    month(ai.g, 2);
    expect(lines(ai.g)).toEqual([]);
    expect(draws(ai.g)).toEqual([{ corps: ['Held Test Corps'], told: null, untold: false }]);
  });

  it("switch off: the base's line, nothing noted", () => {
    const off = board({ on: false });
    month(off.g, 2);
    expect(lines(off.g)).toEqual(['Test Theatre Army has arrived in Magdeburg.']);
    expect(ruleHolds(off.g)).toEqual([]);
  });
});
