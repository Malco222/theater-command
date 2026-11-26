// The general's line keeps the base's article, the old chart (game.ts `generalsThe`, read by `expeditions`). The UI
// director's section 7 case 5 ("a name whose head, before any bracket, ends in Force takes no article") is for the
// telling's own sentences (arrivalwaits.ts `theOf`), which exist only with the switch on. The general's line exists with
// the switch off too, so it keeps exactly the rule of 70d6f6a1f: none where the name opens with "the", is an "X Force" or
// is Creforce. With the switch off every sentence is the base's, byte for byte (the bundle's first law; the refuter's
// switch-01). Where the general's line is followed by section 4's second sentence, the first is the base's and the
// second the telling's. The far side's rows: tests/general-line-article-far.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { theOf } from '../src/campaign/arrivalwaits';
import { addCorps, createGame, generalsThe, relocate, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

/** The base's rule, as 70d6f6a1f writes it in game.ts's general's line. */
const BASE = (name: string): string => (/^(the |[A-Z] Force|Creforce)/i.test(name) ? '' : 'the ');

const ROWS = (setup as any).expeditions as any[];
let added = 0;
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } });
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'GER'); };
const said = (g: Game, t: number): string[] => g.headlines.filter((h) => h.turn === t && h.nat === 'GER' && /Hans Test|Test Corps/.test(h.text)).map((h) => h.text);

describe("the general's line's article is the base's, for every dated row of the old chart", () => {
  it('a table over setup-1939.json\'s rows: no later widening slips through', () => {
    expect(ROWS.length).toBeGreaterThan(100);
    const differ = ROWS.filter((r) => generalsThe(r[2]) !== BASE(r[2])).map((r) => r[2]);
    expect(differ).toEqual([]);
    // (The one old-chart name the telling's wider rule reads otherwise: the general's line keeps the base's.)
    const wtf = ROWS.find((r) => r[1] === 'USA' && /^Western Task Force/.test(r[2]));
    expect(wtf?.[6]?.gen).toBe('George S. Patton');
    expect(generalsThe(wtf![2])).toBe('the ');
    expect(theOf(wtf![2])).toBe('');
  });
});

describe("the general's line on a built board, Germany seated, the old chart", () => {
  it('switch off: "Hans Test arrives in Magdeburg with the Western Task Force (Test Division)."', () => {
    const g = startOpen(createGame(1, 'GER'));
    expect(g.flags.fromWait).toBeUndefined();
    ROWS.push([2, 'GER', 'Western Task Force (Test Division)', 'inf', 1, 'Magdeburg', { gen: 'Hans Test' }]); added++;
    month(g, 2);
    expect(said(g, 2)).toEqual(['Hans Test arrives in Magdeburg with the Western Task Force (Test Division).']);
  });

  it('switch on, after a wait on the front: the general\'s line is the base\'s, the second sentence the telling\'s', () => {
    const g = startOpen(createGame(1, 'GER'));
    g.flags.fromWait = 1;
    const c = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
    ROWS.push([2, 'GER', 'Special Force (Test Division)', 'inf', 2, 'Magdeburg', { from: ['Test Corps'], gen: 'Hans Test' }]); added++;
    month(g, 2);
    // (The telling's own sentence: case 5, no article.)
    expect(said(g, 2)).toEqual(['Special Force (Test Division) is due in Magdeburg this month, formed from your Test Corps, which is at the front in the Grenzmark. The arrival waits, and forms the month after that corps leaves the front.']);
    relocate(g, c, g.byName.Bavaria);
    month(g, 3);
    expect(said(g, 3)).toEqual(['Hans Test arrives in Magdeburg with the Special Force (Test Division). The arrival is formed from your Test Corps, now that it has left the front.']);
  });
});
