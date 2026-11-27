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
import { addCorps, atWar, capitulate, createGame, declareWar } from '../src/campaign/game';
import { raisedQuality, setStop } from '../src/campaign/history';
import { menLeft } from '../src/campaign/manpower';
import { PACIFIC, demand, pacificOpening } from '../src/campaign/pacific';
import { startOpen } from '../src/campaign/staff';
import { corpsCard, whyNotOrder } from '../src/campaign/armies';
import { whyNotFold } from '../src/campaign/fold';
import { canOrder, current } from '../src/campaign/turn';
import '../src/campaign/far-rows';
import type { Corps, Game } from '../src/campaign/types';

const FRENCH = ['Tonkin Division (with the 5th Foreign Infantry Regiment)', 'Annam Brigade', 'Cochinchina and Cambodia Division', 'Laos battalions'];
const POSTS = PACIFIC!.demand.posts!;
const r3 = (x: number): number => Math.round(x * 1000) / 1000;

/** France beaten by Germany in June 1940, at a stop of the dial, moved by hand to a month of the war. */
const france = (stop: number, turn: number, human: string | null = null): Game => {
  const g = startOpen(createGame(3, human));
  if (stop !== 3) setStop(g, stop, true);
  // (Hanoi's post is made from the 104th Division's fourth regiment, which stands at Canton from Japan's first month
  // (china-setup.json, `_canton`; the design director's 08:48Z of 22 November 2026): put there by hand, as the month is.)
  const regiment = addCorps(g, 'JAP', '21st Army (170th Infantry Regiment, of the 104th Division)', 'inf', 1, 'Canton', { xp: 0.35, est: 0.3 });
  regiment.str = 0.3;
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
    expect(news(g, 21, 'and the French garrison keeps its posts under its own officers.')).toBe(true);
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
    // (Haiphong's 0.1 and Lao Kay's 0.035, which come from nobody. Hanoi's 0.3 is the 170th Infantry's own men, made
    // from that row and drawn from no reserve: the cases below, and the ruling of 08:48Z of 22 November 2026.)
    expect(r3(had - menLeft(g, 'JAP'))).toBe(0.135);
    expect(corps(g, '21st Army (170th Infantry Regiment, of the 104th Division)')).toBeUndefined();
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

// (The design director's ruling of 08:48Z of 22 November 2026, on Canton's rows: a post may name the corps on the
// board it is made from, as a formations row does since Oahu's split. The states are set by hand: a test regiment
// stands at Canton, and Hanoi's post is told to be made from it for the length of the case.)
describe('a post made from a corps on the board takes its strength from it', () => {
  const NAME = 'a test regiment (Canton)';
  const from = <T>(run: () => T): T => {
    const p = POSTS[0] as unknown as unknown[], was = p[3];
    p[3] = [NAME];
    try { return run(); } finally { if (was === undefined) p.length = 3; else p[3] = was; }
  };
  const regiment = (g: Game, str: number): Corps => { const c = addCorps(g, 'JAP', NAME, 'inf', 1, 'Canton', { xp: 0.35, est: Math.max(str, 0.3) }); c.str = str; return c; };

  it('the post takes its worth out of the corps wherever it stands; the corps emptied is struck off, and no men are drawn for it', () => {
    const g = france(3, 21);
    regiment(g, 0.3);
    const had = menLeft(g, 'JAP');
    from(() => demand(g));
    expect(corps(g, NAME)).toBeUndefined();
    const post = corps(g, POSTS[0][2])!;
    expect([post.nat, at(g, post), r3(post.divs * post.str), post.hold, !!post.stay]).toEqual(['JAP', 'Hanoi', 0.3, true, true]);
    // (The two posts that are made from nobody are drawn as before: 0.1 and 0.035.)
    expect(r3(had - menLeft(g, 'JAP'))).toBe(0.135);
  });

  it('never more than the corps holds: a regiment at 0.2 makes a post of 0.2, which fills to its own 0.3 and no further', () => {
    const g = france(3, 21);
    regiment(g, 0.2);
    from(() => demand(g));
    expect(corps(g, NAME)).toBeUndefined();
    const post = corps(g, POSTS[0][2])!;
    expect([r3(post.divs * post.str), post.full]).toEqual([0.2, 0.3]);
  });

  it('a corps that holds more keeps the rest where it stands', () => {
    const g = france(3, 21);
    regiment(g, 0.5);
    from(() => demand(g));
    const left = corps(g, NAME)!;
    expect([at(g, left), r3(left.divs * left.str)]).toEqual(['Canton', 0.2]);
    expect(r3(corps(g, POSTS[0][2])!.str)).toBe(0.3);
  });

  it('with the corps gone the post is not made; the others come as they did, and the French army keeps Hanoi', () => {
    const g = france(3, 21);
    from(() => demand(g));
    expect(corps(g, POSTS[0][2])).toBeUndefined();
    for (const [place, worth, name] of POSTS.slice(1)) { const c = corps(g, name)!; expect([at(g, c), r3(c.divs * c.str)], name).toEqual([place, worth]); }
    expect(g.byName.Hanoi.units.filter((u) => !u.dead).map((u) => [u.nat, u.name])).toEqual([['JAP', FRENCH[0]]]);
  });

  it("at the book's stops it is the same: the post is the corps' own men, in the demand's month", () => {
    const b = france(1, 23);
    regiment(b, 0.3);
    const book = menLeft(b, 'JAP');
    from(() => pacificOpening(b));
    expect(b.flags['pac:demand']).toBe(23);
    expect([corps(b, NAME), r3(corps(b, POSTS[0][2])!.str), menLeft(b, 'JAP')]).toEqual([undefined, 0.3, book]);
  });
});

describe("a player's Japan commands them no more than a computer's does (the director's 19:40Z)", () => {
  /** A player's Japan on its own turn, the demand made by his own declaration. */
  const mine = (stop: number): Game => {
    const g = france(stop, 23, 'JAP');
    while (current(g) !== 'JAP') g.phase = (g.phase + 1) % g.order.length;
    declareWar(g, 'JAP', 'VIC');
    return g;
  };

  it("the kept French corps take none of his orders; his own post beside them does", () => {
    const g = mine(3);
    for (const nm of FRENCH) {
      const c = corps(g, nm)!;
      expect([c.nat, c.raised, c.hold, !!c.stay], nm).toEqual(['JAP', 'VIC', true, true]);
      expect(canOrder(g, c), nm).toBe(false);
    }
    expect(canOrder(g, corps(g, POSTS[0][2])!)).toBe(true);
  });

  it("the card says why, where a corps shows its trouble: the UI director's line (19:48Z); his own post beside them shows none", () => {
    const g = mine(3);
    const LINE = 'The French garrison keeps order in the colony under its own officers, and takes no orders from you.';
    for (const nm of FRENCH) {
      expect(whyNotOrder(g, corps(g, nm)!), nm).toBe(LINE);
      expect(corpsCard(g, corps(g, nm)!).noOrders, nm).toBe(LINE);
    }
    expect(whyNotOrder(g, corps(g, POSTS[0][2])!)).toBeNull();
    // (To a computer's Japan, and to anybody else looking on, the card says nothing: the line is for the one who would give the order.)
    const c = france(3, 21);
    demand(c);
    expect(whyNotOrder(c, corps(c, FRENCH[0])!)).toBeNull();
  });

  it('nobody is folded into them, nor they into anybody: the refusal says why', () => {
    const g = mine(3);
    const french = corps(g, FRENCH[0])!, own = corps(g, POSTS[0][2])!;
    expect(french.reg).toBe(own.reg);
    const WHY = 'The French garrison of Indochina answers to its own officers, not to you.';
    expect(whyNotFold(g, own, french)).toBe(WHY);
    expect(whyNotFold(g, french, own)).toBe(WHY);
  });

  it("no coup comes for a player's Japan at the book's stops: they hold past month 67 (its end by cause is after-release item 26)", () => {
    for (const stop of [1, 2]) {
      const g = mine(stop);
      g.turn = 67;
      pacificOpening(g);
      g.turn = 70;
      pacificOpening(g);
      expect(FRENCH.every((nm) => !!corps(g, nm)), `stop ${stop}`).toBe(true);
      expect(g.flags['pac:coup']).toBeUndefined();
    }
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
