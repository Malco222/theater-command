// The islands' room in men, top-up 1 (27 November 2026): the sentence an offered formation reads where an island has no
// room for it, and the rule's refusal, agree on the same island (the design director, 16:36Z, 27 November 2026: "The UI
// director's words may be copied, but any figure in them (the room, the men already there) is read from the same
// function the rule reads, called and never typed. A test pins that the sentence and the refusal agree on the same
// island.").
//
// The sentence is the UI director's U5 (00:19Z, 27 November 2026, island-room-in-men-UI-MARKS.md): "{Palau} has room for
// {2} divisions, about {30,000} men, and {27,000} of yours are there: only {3,000} more fit, and this takes {5,000}."
// Every figure in the expected sentence below is read in the test from the rule's own readers: the island's room from
// `islandOf`, the room left from `roomLeft` (the men there are the room less it), and what the row takes from the corps
// the row forms (game.ts `rowCorps`) at the corps card's own measure (`men`), the reader `roomRefusal` and `fits` use.
// Only her rounding is laid here, as her marks give it (section 1 and her answer of 21:37Z, 26 November 2026, 2; U1):
// the men there UP to the thousand, what more fits DOWN, what the row takes to the nearest thousand, under 1,000 whole.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, createGame, declareWar, men, removeCorps, rowCorps } from '../src/campaign/game';
import { fits, islandOf, roomLeft } from '../src/campaign/islands';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, formationOffers, humanUp, orderTakeFormation, stepAI } from '../src/campaign/turn';
import { MEN_PER_DIV, type Corps, type Game, type Region } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';

const rows = (setup as any).expeditions as any[][];
const ROW = 169, NAME = 'Sakaguchi Detachment (56th Mixed Infantry Group)';

/** A whole-world war with Japan's player at his first month (seed 3), at war with America, the row for Palau offered by hand in its own month as that month would offer it. */
function war(): Game {
  const g = startOpen(createGame(3, 'JAP'));
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  beginHuman(g);
  declareWar(g, 'JAP', 'USA');
  // (The row's own month, so that taking it forms it as its month would: the rule forms no row before its date.)
  g.turn = rows[ROW][0];
  g.flags.offers = 1;
  g.flags[`asked:${ROW}`] = g.turn;
  return g;
}
const standing = (g: Game, r: Region): number => r.units.filter((u) => u.kind !== 'eng').length;
/** The corps the row forms, as the rule reads its room (game.ts `rowCorps`). */
const going = (g: Game): Corps[] => rowCorps(g, 'JAP', rows[ROW], rows[ROW][6] ?? {});

/** Her rounding of a figure of men (thousands with the comma, under 1,000 whole). */
const fig = (n: number, how: 'up' | 'down' | 'near'): string => {
  const m = Math.max(0, Math.round(n));
  return (m < 1000 ? m : (how === 'up' ? Math.ceil(m / 1000) : how === 'down' ? Math.floor(m / 1000) : Math.round(m / 1000)) * 1000).toLocaleString('en-US');
};
/** U5, its figures read from the rule's readers for this island and this row, at this moment. */
function u5(g: Game, r: Region): string {
  const room = islandOf(g, r)!.room, whole = room * MEN_PER_DIV;
  const left = Math.round(roomLeft(g, r, 'JAP') * MEN_PER_DIV), there = whole - left, takes = men(going(g)[0]);
  const more = there < 1000 ? left.toLocaleString('en-US') : fig(left, 'down');
  return `${r.name} has room for ${room === 1 ? 'one division' : `${room} divisions`}, about ${whole.toLocaleString('en-US')} men, and ${fig(there, 'up')} of yours are there: only ${more} more fit, and this takes ${fig(takes, 'near')}.`;
}

describe('an offered formation refused for an island\'s room: the sentence and the refusal agree on the same island', () => {
  it('Palau: the figures the offer prints are what the rule\'s own readers answer, and the row forms there exactly when its line says it fits', () => {
    const g = war();
    const palau = g.byName.Palau;
    expect(rows[ROW][2]).toBe(NAME);
    for (const u of [...palau.units, ...(palau.garr ?? [])]) removeCorps(g, u);
    const fill = addCorps(g, 'JAP', 'Fill Palau', 'inf', 2, palau.id, {});
    // (No other ground with room for the row: a row for a full island goes by sea to the nearest held harbour with room,
    // game.ts roomNear, the design director's D1 of 00:20Z, 27 November 2026. Every other harbour of Japan's is filled
    // until `fits`, asked as roomNear asks it, refuses the row there, so that the offer's place is Palau whatever the
    // room says.)
    for (const r of g.regions) {
      if (!r.port || r === palau || r.ctrl !== 'JAP') continue;
      for (let k = 0; k < 12 && fits(g, r, 'JAP', going(g), { rows: standing(g, r) }); k++) addCorps(g, 'JAP', `Fill ${r.name} ${k}`, 'inf', 1, r.id, {});
    }
    const takes = men(going(g)[0]), whole = islandOf(g, palau)!.room * MEN_PER_DIV;
    const offer = () => formationOffers(g).find((o) => o.id === ROW)!;
    const formed = (): boolean => g.corps.some((c) => c.name === NAME && !c.dead);

    // Refused for its men: some room left, less than the row takes.
    fill.str = (whole - takes + 1) / (2 * MEN_PER_DIV);
    expect([fits(g, palau, 'JAP', going(g)), Math.round(roomLeft(g, palau, 'JAP') * MEN_PER_DIV)]).toEqual([false, takes - 1]);
    expect([offer().region, offer().why]).toEqual([palau.id, u5(g, palau)]);
    // A second board of the same kind, further from the edge: what the marks quote, read from the rule.
    fill.str = 0.9;
    expect(fits(g, palau, 'JAP', going(g))).toBe(false);
    expect(offer().why).toBe(u5(g, palau));
    // The refusal: taking it is refused with that same line, and nothing forms.
    expect(orderTakeFormation(g, ROW)).toBe(u5(g, palau));
    expect(formed()).toBe(false);

    // Just enough room for the row: the rule lets it in, the line says nothing, and taking it forms it on Palau.
    fill.str = (whole - takes) / (2 * MEN_PER_DIV);
    expect([fits(g, palau, 'JAP', going(g)), Math.round(roomLeft(g, palau, 'JAP') * MEN_PER_DIV)]).toEqual([true, takes]);
    expect([offer().region, offer().why]).toEqual([palau.id, null]);
    expect(orderTakeFormation(g, ROW)).toBeNull();
    expect(g.corps.filter((c) => c.name === NAME && !c.dead).map((c) => g.regions[c.reg].name)).toEqual(['Palau']);
  }, 300000);
});
