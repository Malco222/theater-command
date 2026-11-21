// Indochina after the demand (the design director's rulings of 16:44Z and 16:46Z of 21 November 2026, on the
// trace of Hanoi and the historian's japan-taken-towns-1942.md, section 7; pacific.ts demand and coupMonth,
// pacific.json 'demand').
//
// 199(e)'s added line had the French garrison lay down its arms. With it gone and Japan's one division held at
// Phnom Penh, Tonkin stood empty the day of the demand, and in 12 wars of 12 China's army walked into Hanoi and
// Lang Son the month after. In the record the French army stayed at its posts all over the colony until 9 March
// 1945, with Japan's troops beside it by agreement, and no Chinese force crossed into Tonkin before 1945. So on
// the demand every corps of Vichy's on the ground answers to Japan as a fallen country's corps answers to its
// keeper (199(b)): it is marked as raised by Vichy, fights at Vichy's own figure, and holds and stays where it
// stands. Japan's own troops under the agreement come with the demand: Hanoi 0.3, Haiphong 0.1, Lao Kay 0.035,
// none at Lang Son or Vinh. The kept army's end: the record's coup at the book's stops (month 67); at stops 3 to 5
// it holds to the war's end for now. The states are set by hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { runAI } from '../src/campaign/ai';
import { atWar, capitulate, createGame, declareWar } from '../src/campaign/game';
import { raisedQuality, setStop } from '../src/campaign/history';
import { menLeft } from '../src/campaign/manpower';
import { PACIFIC, demand, pacificOpening } from '../src/campaign/pacific';
import { startOpen } from '../src/campaign/staff';
import '../src/campaign/far-rows';
import type { Corps, Game } from '../src/campaign/types';

const FRENCH = ['Tonkin Division (with the 5th Foreign Infantry Regiment)', 'Annam Brigade', 'Cochinchina and Cambodia Division', 'Laos battalions'];
const POSTS = PACIFIC!.demand.posts!;
const r3 = (x: number): number => Math.round(x * 1000) / 1000;

/** France beaten by Germany in June 1940, at a stop of the dial, moved by hand to a month of the war. */
const france = (stop: number, turn: number, human: string | null = null): Game => {
  const g = startOpen(createGame(3, human));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = 10;
  capitulate(g, 'FRA', 'GER');
  g.turn = turn;
  return g;
};
const corps = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.name === name && !c.dead);
const at = (g: Game, c: Corps): string => g.regions[c.reg].name;
const news = (g: Game, turn: number, part: string): boolean => g.headlines.some((h) => h.turn === turn && h.text.includes(part));

describe('on the demand the French army stays at its posts and answers to Japan', () => {
  it("every corps of Vichy's in Indochina keeps its post, its weight and its name; it is Japan's, raised by Vichy, and holds and stays", () => {
    const g = france(3, 21);
    const before = FRENCH.map((nm) => { const c = corps(g, nm)!; return [nm, c.nat, at(g, c), r3(c.divs * c.str)]; });
    expect(before.map((b) => b[1])).toEqual(['VIC', 'VIC', 'VIC', 'VIC']);
    expect(demand(g)).toBe(true);
    for (const [nm, , place, worth] of before) {
      const c = corps(g, nm as string)!;
      expect([c.nat, c.raised, at(g, c), r3(c.divs * c.str), c.hold, !!c.stay], nm as string).toEqual(['JAP', 'VIC', place, worth, true, true]);
    }
    expect(atWar(g, 'JAP', 'VIC')).toBe(false);
    expect(news(g, 21, 'the French garrison stays at its posts')).toBe(true);
  });

  it("it fights at Vichy's own figure, as it did the day before, and not as Japan's line", () => {
    const g = france(3, 21);
    demand(g);
    const c = corps(g, FRENCH[0])!;
    expect(raisedQuality(g, c)).toBe(g.nations.VIC.quality);
    expect(g.nations.VIC.quality).toBeLessThan(g.nations.JAP.quality);
  });

  it("Vichy's corps elsewhere are untouched: the demand is for the ground that joins Saigon by land", () => {
    const g = france(3, 21);
    const away = g.corps.filter((c) => c.nat === 'VIC' && !c.dead && !FRENCH.includes(c.name)).map((c) => c.name);
    expect(away.length).toBeGreaterThan(0);
    demand(g);
    for (const nm of away) expect(corps(g, nm)!.nat, nm).toBe('VIC');
  });

  it("the month's own moves leave them where they stand: a computer's Japan marches none of them", () => {
    const g = france(3, 21);
    demand(g);
    g.turn = 22;
    runAI(g, 'JAP');
    expect(FRENCH.map((nm) => at(g, corps(g, nm)!))).toEqual(['Hanoi', 'Hue', 'Saigon', 'Vientiane']);
  });
});

describe("Japan's own troops under the agreement come with the demand", () => {
  it('the rows: Hanoi 0.3, Haiphong 0.1, Lao Kay 0.035, and none at Lang Son or Vinh', () => {
    expect(POSTS.map((p) => [p[0], p[1]])).toEqual([['Hanoi', 0.3], ['Haiphong', 0.1], ['Lao Kay', 0.035]]);
    const g = france(3, 21);
    demand(g);
    for (const [place, worth, name] of POSTS) {
      const c = corps(g, name)!;
      expect([c.nat, at(g, c), c.kind, r3(c.divs * c.str), c.hold, !!c.stay, c.raised], name).toEqual(['JAP', place, 'inf', worth, true, true, undefined]);
    }
    for (const nm of ['Lang Son', 'Vinh']) expect(g.byName[nm].units.filter((u) => !u.dead), nm).toEqual([]);
  });

  it("where the demand comes by cause their weight is drawn from Japan's reserve of men; at the book's stops they are dated rows and not paid for", () => {
    const g = france(3, 21);
    const had = menLeft(g, 'JAP');
    demand(g);
    expect(r3(had - menLeft(g, 'JAP'))).toBe(0.435);
    const b = france(1, 23);
    const book = menLeft(b, 'JAP');
    pacificOpening(b);
    expect(b.flags['pac:demand']).toBe(23);
    expect(corps(b, POSTS[0][2])).toBeTruthy();
    expect(menLeft(b, 'JAP')).toBe(book);
  });

  it('Hanoi the day after: the Tonkin Division and Japan\'s force stand in it, so nobody walks into an empty town', () => {
    const g = france(3, 21);
    demand(g);
    const there = g.byName.Hanoi.units.filter((u) => !u.dead).map((u) => [u.nat, u.name, r3(u.divs * u.str)]);
    expect(there).toEqual([['JAP', FRENCH[0], 0.7], ['JAP', POSTS[0][2], 0.3]]);
    expect(g.byName.Hanoi.ctrl).toBe('JAP');
  });

  it("a player's Japan gets the same on his own demand: the French rows are his, and his troops come", () => {
    const g = france(3, 23, 'JAP');
    declareWar(g, 'JAP', 'VIC');
    expect(g.flags['pac:demand']).toBe(23);
    expect(corps(g, FRENCH[0])!.nat).toBe('JAP');
    expect(corps(g, POSTS[0][2])).toBeTruthy();
  });
});

describe("the kept army's end", () => {
  it("at the book's stops: the record's coup in month 67, and the corps Vichy raised are struck off; Japan's own stay", () => {
    for (const stop of [1, 2]) {
      const g = france(stop, 23);
      pacificOpening(g);
      g.turn = 66;
      pacificOpening(g);
      expect(FRENCH.every((nm) => !!corps(g, nm)), `stop ${stop}`).toBe(true);
      g.turn = 67;
      pacificOpening(g);
      expect(FRENCH.some((nm) => !!corps(g, nm)), `stop ${stop}`).toBe(false);
      expect(g.flags['pac:coup']).toBe(67);
      expect(news(g, 67, 'disarms it')).toBe(true);
      expect(corps(g, POSTS[0][2])).toBeTruthy();
    }
  });

  it('at stops 3 to 5 they hold to the war\'s end: no coup comes by the date', () => {
    const g = france(3, 21);
    demand(g);
    g.turn = 67;
    pacificOpening(g);
    g.turn = 72;
    pacificOpening(g);
    expect(FRENCH.every((nm) => !!corps(g, nm))).toBe(true);
    expect(g.flags['pac:coup']).toBeUndefined();
  });
});
