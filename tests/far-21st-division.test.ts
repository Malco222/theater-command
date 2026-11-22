// The 21st Division at the book's stops (the design director's 19:40Z (g), 19:56Z and 20:12Z of 21 November 2026;
// the historian's japan-taken-towns-1942.md, sections 9 and 10; far-shores.json, '_21st').
//
// The record: the division passed to the Southern Army in November 1941, left the Tientsin-Pukow railway in month
// 27 and landed in Tonkin in month 30, less the detachment it sent to Bataan. Its stretch at Hsuchow was taken by
// the 17th Division, which the chart has from the first day inside the 13th Army's row at Pengpu: so no new
// division is laid, the weight is shifted, Hsuchow about 0.5 and Pengpu about 0.4, the men of the two old rows and
// no more. In September 1943 the 17th sailed for Rabaul and the 65th Division held Hsuchow to 1945. All of it is
// at stops 1 and 2 alone: at stops 3 to 5 nothing moves by a date, and the old month-30 cut for Bataan stands.
// The states are set by hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { capitulate, createGame, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { pacificOpening } from '../src/campaign/pacific';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';

const WHOLE = 'Tientsin-Pukow railway garrison (21st Division)', HUAI = '13th Army (the Huai river)';
const SEVENTEENTH = '13th Army (17th Division, Hsuchow)', TONKIN = '21st Division (Tonkin, less its Bataan detachment)';
const BATAAN = '21st Division detachment (for Bataan)', OLD_CUT = 'Tientsin-Pukow railway garrison (21st Division, less its Bataan detachment)';
const SIXTY_FIFTH = '65th Division (Hsuchow)', RABAUL = '17th Division (Rabaul)';
const r3 = (x: number): number => Math.round(x * 1000) / 1000;
const war = (stop: number): Game => { const g = startOpen(createGame(5, null)); if (stop !== 3) setStop(g, stop, true); return g; };
const all = (g: Game, name: string): Corps[] => g.corps.filter((c) => c.nat === 'JAP' && c.name === name && !c.dead);
const one = (g: Game, name: string): Corps | undefined => all(g, name)[0];
const fig = (g: Game, name: string): [string, number] | null => { const c = one(g, name); return c ? [g.regions[c.reg].name, r3(c.divs * c.str)] : null; };
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'JAP'); };
const dated = (name: string): any[][] => ((setup as any).expeditions as any[][]).filter((r) => r[1] === 'JAP' && r[2] === name);
/** Where a row stands, or null: its weight is read apart, in the month it lands (a corps on the board takes its drafts month by month). */
const at = (g: Game, name: string): string | null => { const c = one(g, name); return c ? g.regions[c.reg].name : null; };
/** France beaten in June 1940 and the book's demand on Indochina made in its month, so that Tonkin is Japan's (pacific.ts). */
const tonkin = (g: Game): void => { g.turn = 10; capitulate(g, 'FRA', 'GER'); g.turn = 23; pacificOpening(g); };
/** A place put in Japan's hands by hand, emptied of anybody else's corps. */
const give = (g: Game, place: string): void => {
  const r = g.byName[place];
  for (const u of [...r.units]) if (u.nat !== 'JAP') { u.dead = true; r.units.splice(r.units.indexOf(u), 1); }
  r.ctrl = 'JAP';
};
/** The men of the rows this file is about, wherever they stand. */
const men = (g: Game): number => r3([WHOLE, HUAI, SEVENTEENTH, TONKIN, BATAAN, OLD_CUT].flatMap((nm) => all(g, nm)).reduce((n, c) => n + c.divs * c.str, 0));

describe('the rows as laid', () => {
  it("five rows, each for the book's stops alone; the old month-30 cut is for stops 3 to 5; Bataan's detachment names either cut", () => {
    const rows = [[SEVENTEENTH, 27, 'Hsuchow', 0.5], [HUAI, 27, 'Pengpu', 0.4], [TONKIN, 30, 'Hanoi', 0.65], [SIXTY_FIFTH, 49, 'Hsuchow', 0.55], [RABAUL, 50, 'Rabaul', 0.65]] as const;
    for (const [nm, m, place, worth] of rows) {
      const [r, ...more] = dated(nm);
      expect(more, nm).toEqual([]);
      expect([r[0], r[5], r[4] * r[6].str, r[6].est, r[6].stops], nm).toEqual([m, place, worth, worth, [1, 2]]);
    }
    expect(dated(SEVENTEENTH)[0][6].from).toEqual([WHOLE, HUAI]);
    expect([dated(HUAI)[0][6].cut, dated(TONKIN)[0][6].cut, dated(TONKIN)[0][6].hold]).toEqual([SEVENTEENTH, SEVENTEENTH, 1]);
    expect([dated(SIXTY_FIFTH)[0][6].from, dated(RABAUL)[0][6].cut]).toEqual([[SEVENTEENTH], SIXTY_FIFTH]);
    expect(dated(OLD_CUT)[0][6].stops).toEqual([3, 4, 5]);
    expect(dated(BATAAN)[0][6].cut).toEqual([OLD_CUT, SEVENTEENTH]);
    expect(dated(BATAAN)[0][6].stops).toBeUndefined();
  });
});

describe("at the book's stops the 21st leaves the railway in month 27 and is in Tonkin in month 30", () => {
  for (const stop of [1, 2]) {
    it(`stop ${stop}: month 27, the 17th Division takes over Hsuchow at 0.5 and the Huai stands at 0.4; the 21st is off the board`, () => {
      const g = war(stop);
      expect([fig(g, WHOLE), fig(g, HUAI), fig(g, SEVENTEENTH)]).toEqual([['Hsuchow', 0.9], ['Pengpu', 0.9], null]);
      month(g, 27);
      expect([fig(g, WHOLE), fig(g, HUAI), fig(g, SEVENTEENTH)]).toEqual([null, ['Pengpu', 0.4], ['Hsuchow', 0.5]]);
      expect(all(g, HUAI).length).toBe(1);
      expect(g.byName.Hsuchow.units.filter((u) => !u.dead && u.nat === 'JAP').map((u) => u.name)).toEqual([SEVENTEENTH]);
      // (Months 27 to 29 the 21st is at sea and on the road: 0.9 of the 1.8 stands on the board.)
      expect(men(g)).toBe(0.9);
      expect([fig(g, TONKIN), fig(g, BATAAN)]).toEqual([null, null]);
    });

    it(`stop ${stop}: month 30, it stands in Hanoi at 0.65 and holds, its detachment for Bataan comes at 0.25, and no man is counted twice`, () => {
      const g = war(stop);
      // (Tonkin is Japan's by the demand at the book's stops, month 23: pacific.ts; Lingayen by the landing of month 28.)
      tonkin(g);
      expect(g.flags['pac:demand']).toBe(23);
      month(g, 27);
      give(g, 'Lingayen');
      month(g, 30);
      const t = one(g, TONKIN)!, b = one(g, BATAAN)!;
      expect([g.regions[t.reg].name, r3(t.str), t.hold]).toEqual(['Hanoi', 0.65, true]);
      expect([g.regions[b.reg].name, r3(b.str)]).toEqual(['Lingayen', 0.25]);
      expect(one(g, OLD_CUT)).toBeUndefined();
      expect(men(g)).toBe(1.8);
      // (And the months after bring nothing more of it.)
      for (const m of [31, 32, 36]) month(g, m);
      expect(men(g)).toBe(1.8);
      expect([all(g, TONKIN).length, all(g, BATAAN).length, all(g, SEVENTEENTH).length, all(g, HUAI).length]).toEqual([1, 1, 1, 1]);
    });

    it(`stop ${stop}: month 49 the 65th Division takes Hsuchow at 0.55 as the 17th sails, and month 50 the 17th is ashore at Rabaul at 0.65`, () => {
      const g = war(stop);
      month(g, 27);
      give(g, 'Rabaul');
      month(g, 48);
      expect([at(g, SEVENTEENTH), at(g, SIXTY_FIFTH), at(g, RABAUL)]).toEqual(['Hsuchow', null, null]);
      month(g, 49);
      expect([fig(g, SEVENTEENTH), fig(g, SIXTY_FIFTH), fig(g, RABAUL)]).toEqual([null, ['Hsuchow', 0.55], null]);
      month(g, 50);
      expect([at(g, SIXTY_FIFTH), fig(g, RABAUL), at(g, HUAI)]).toEqual(['Hsuchow', ['Rabaul', 0.65], 'Pengpu']);
    });
  }

  it("Hsuchow lost before month 27: the 21st stays whole where it stands, the Huai row is not cut, and nothing of it comes to Tonkin or Bataan", () => {
    const g = war(1);
    const h = g.byName.Hsuchow, c = one(g, WHOLE)!, to = g.byName.Tsinan;
    h.units.splice(h.units.indexOf(c), 1); to.units.push(c); c.reg = to.id;
    h.ctrl = 'CHN';
    tonkin(g);
    give(g, 'Lingayen');
    for (const m of [27, 30, 33]) {
      month(g, m);
      expect([at(g, WHOLE), at(g, HUAI), at(g, SEVENTEENTH), at(g, TONKIN), at(g, BATAAN)], `month ${m}`).toEqual(['Tsinan', 'Pengpu', null, null, null]);
      expect(all(g, HUAI).length).toBe(1);
    }
  });
});

describe('at stops 3 to 5 nothing of it moves by a date', () => {
  for (const stop of [3, 4, 5]) {
    it(`stop ${stop}: the division stays at Hsuchow, cut for Bataan in month 30 as before; no 17th, 65th or Tonkin row ever comes`, () => {
      const g = war(stop);
      give(g, 'Hanoi'); give(g, 'Rabaul'); give(g, 'Lingayen');
      for (const m of [27, 28, 29]) month(g, m);
      expect([at(g, WHOLE), at(g, HUAI), at(g, SEVENTEENTH)]).toEqual(['Hsuchow', 'Pengpu', null]);
      expect(one(g, HUAI)!.str).toBeGreaterThanOrEqual(0.9);
      month(g, 30);
      expect([at(g, WHOLE), fig(g, OLD_CUT), at(g, HUAI)]).toEqual([null, ['Hsuchow', 0.65], 'Pengpu']);
      expect(r3(one(g, BATAAN)!.str)).toBe(0.25);
      for (const m of [31, 49, 50, 51]) month(g, m);
      for (const nm of [SEVENTEENTH, TONKIN, SIXTY_FIFTH, RABAUL]) expect(one(g, nm), nm).toBeUndefined();
      expect([at(g, OLD_CUT), at(g, HUAI), all(g, HUAI).length]).toEqual(['Hsuchow', 'Pengpu', 1]);
      expect(one(g, HUAI)!.str).toBeGreaterThanOrEqual(0.9);
    });
  }
});
