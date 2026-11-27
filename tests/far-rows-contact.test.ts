// A dated row takes no corps standing in contact (the design director's ruling of 19:47Z, 25 November 2026: "one
// sentence, both doors: a date never lifts a corps out of a fight"; her 18:47Z ruling guards the rows that draw a
// division, drawn.ts, and this is the row that takes a named corps whole, `from`).
//
// As measured (three whole-world wars nobody plays, seeds 5, 3 and 17, to month 66): 92 rows were laid by taking a
// named corps; 39 of those corps stood in contact; 2 were on a live front of their staff's plan and were held (ruling
// 234); 8 took the row's name where they stood; 29 were lifted to another province. Japan's 37th Division went from
// Loyang to Siangyang and then to the Indochina border, in contact both times; her 18th Division left the line in
// China for Malaya; Britain's 70th Division left Siam to become the Chindits.
//
// So: a row whose corps stands in contact waits, as it waits for a live front, and its half year is counted from the
// month its corps comes free; a row with a body of its own (`adds`) comes at that worth and takes nobody; a corps
// already standing where the row forms takes the row's name as before; a row the player takes himself is his order.
// In contact: an enemy at war on the corps' province or beside it, or holding either (drawn.ts `corpsInContact`). The
// states are set by hand; no war is played.
//
// One switch for both charts, 'fromWait' (stage.json, off until its gate passes; the design director's 20:08Z: "One
// switch for both charts ... one Options row, named by the UI director"; that row is not built yet). The old chart's
// side of it is tests/from-wait-r2.test.ts. Off, the far side's row takes its corps wherever it stands, as before.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { corpsInContact } from '../src/campaign/drawn';
import { addCorps, atWar, createGame, declareWar, relocate, rowOffers, startPhase, takeOffer } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { onLiveFront } from '../src/campaign/planfronts';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';
import stage from '../data/campaign/stage.json';

const D7 = '7th Division (Asahikawa)', ARMY7 = '6th Army (7th Division)';
const LESS = 'Tientsin-Pukow railway garrison (21st Division, less its Bataan detachment)', GARRISON = 'Tientsin-Pukow railway garrison (21st Division)';
const D56 = '56th Division', SAKAGUCHI = 'Sakaguchi Detachment (56th Mixed Infantry Group)';

/** A grand campaign with the far side on and the switch 'fromWait' set by hand, as a campaign opened with it on carries it. */
const open = (seed: number, seat: string | null): Game => { const g = startOpen(createGame(seed, seat)); g.flags.fromWait = 1; return g; };
const rows = (setup as any).expeditions as any[][];
const index = (name: string): number => rows.findIndex((r) => r[1] === 'JAP' && r[2] === name);
const corps = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead);
const at = (g: Game, c: Corps): string => g.regions[c.reg].name;
/**
 * (The contact rule's second form, the design director's ruling of 06:10Z, 26 November 2026: a corps beside the enemy
 * is held while it took part in the battle last resolved on its ground, or where nobody of its side keeps its province.
 * This board's corps at the front stand with others of their side, so each is set, by hand, as having fought the battle
 * last resolved where it stands, and is held as the board means. The relieved case: tests/contact-fact.test.ts.)
 */
const fought = (g: Game): void => { if (g.flags.fromWait !== 1) return; g.lastBattle = {}; for (const c of g.corps) if (!c.dead) (g.lastBattle[c.reg] ??= []).push(c.id); };
/** Read on this board: every corps as having fought where it stands (`fought`), so the test reads `beside`, as the board means. */
const inContact = (g: Game, c: Corps): boolean => { fought(g); return corpsInContact(g, c.nat, c); };
const month = (g: Game, t: number): void => { fought(g); g.turn = t; startPhase(g, 'JAP'); };
const months = (g: Game, from: number, to: number): void => { for (let t = from; t <= to; t++) month(g, t); };
const enemyHeld = (g: Game, r: Region): boolean => (!!r.ctrl && atWar(g, 'JAP', r.ctrl)) || r.units.some((u) => !u.dead && atWar(g, 'JAP', u.nat));
/** A province of Japan's with no enemy on it and an enemy at war beside it: a place in the line. */
const inTheLine = (g: Game): Region => g.regions.find((r) => r.ctrl === 'JAP' && !enemyHeld(g, r) && r.nb.some((j) => enemyHeld(g, g.regions[j])))!;
/** A province changes hands for the test: whoever stood on it is sent off, and it is the taker's. */
const lose = (g: Game, name: string, to: string, away: string): Region => {
  const r = g.byName[name];
  for (const u of [...r.units]) relocate(g, u, g.byName[away]);
  r.ctrl = to;
  return r;
};

describe('the data: the rows these tests stand on', () => {
  it("the 7th Division's row takes the 6th Army's 7th Division and waits on no war; the railway garrison's row forms where its corps stands; the 56th has a body of its own", () => {
    expect(rows[index(D7)].slice(0, 6)).toEqual([12, 'JAP', D7, 'inf', 1, 'Sapporo']);
    expect(rows[index(D7)][6]).toMatchObject({ from: [ARMY7] });
    expect(rows[index(D7)][6].war).toBeUndefined();
    expect(rows[index(LESS)].slice(0, 6)).toEqual([30, 'JAP', LESS, 'inf', 1, 'Hsuchow']);
    expect(rows[index(LESS)][6]).toMatchObject({ from: [GARRISON] });
    expect(rows[index(D56)][6]).toMatchObject({ adds: 0.67, from: [SAKAGUCHI] });
  });
});

describe('the switch', () => {
  it('is off as it comes: a new grand campaign carries no fromWait flag', () => {
    expect((stage as any).fromWait).toBe(false);
    expect(startOpen(createGame(3, null)).flags.fromWait).toBeUndefined();
  });

  it('off, the row takes its corps out of the line in its month, as before: no wait is written', () => {
    const g = startOpen(createGame(3, null));
    const src = corps(g, ARMY7)!;
    relocate(g, src, inTheLine(g));
    expect(inContact(g, src)).toBe(true);
    expect(onLiveFront(g, 'JAP', src)).toBe(false);
    month(g, 12);
    expect(at(g, corps(g, D7)!)).toBe('Sapporo');
    expect(corps(g, ARMY7)).toBeUndefined();
    expect(g.flags[`wait:${index(D7)}`]).toBeUndefined();
    expect(g.flags[`sent:${index(D7)}`]).toBe(1);
  });

  it("off, a row with a body of its own takes its corps out of the line as before: the Sakaguchi Detachment becomes the 56th Division", () => {
    const g = startOpen(createGame(3, null));
    lose(g, 'Cam Ranh', 'JAP', 'Saigon');
    month(g, 27);
    g.turn = 28;
    for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
    lose(g, 'Rangoon', 'JAP', 'Calcutta');
    const group = corps(g, SAKAGUCHI)!;
    relocate(g, group, inTheLine(g));
    expect(onLiveFront(g, 'JAP', group)).toBe(false);
    month(g, 31);
    expect(corps(g, D56)).toBeDefined();
    expect(corps(g, SAKAGUCHI)).toBeUndefined();
  });
});

describe('in contact: an enemy at war on the province or beside it', () => {
  it("a corps deep in Manchuria is clear; the same corps put into the line in China is in contact, on no live front", () => {
    const g = open(3, null);
    const src = corps(g, ARMY7)!;
    expect(atWar(g, 'JAP', 'CHN')).toBe(true);
    expect(inContact(g, src)).toBe(false);
    const line = inTheLine(g);
    expect(line).toBeDefined();
    relocate(g, src, line);
    expect(inContact(g, src)).toBe(true);
    expect(onLiveFront(g, 'JAP', src)).toBe(false);
  });

  it('an enemy who only holds the province beside, with nobody on it, is contact; a neutral beside is not', () => {
    const g = open(3, null);
    const src = corps(g, ARMY7)!;
    const here = g.regions[src.reg];
    const beside = g.regions[here.nb[0]];
    for (const u of [...beside.units]) relocate(g, u, g.byName['Tokyo']);
    beside.ctrl = 'CHN';
    expect(beside.units.filter((u) => !u.dead)).toHaveLength(0);
    expect(inContact(g, src)).toBe(true);
    beside.ctrl = 'SOV';
    expect(atWar(g, 'JAP', 'SOV')).toBe(false);
    expect(inContact(g, src)).toBe(false);
  });
});

describe('on, a dated row takes no corps standing in contact', () => {
  it('clear of the enemy, the row comes in its month and takes its corps, as before', () => {
    const g = open(3, null);
    expect(inContact(g, corps(g, ARMY7)!)).toBe(false);
    month(g, 12);
    const c = corps(g, D7)!;
    expect([at(g, c), c.divs]).toEqual(['Sapporo', 1]);
    expect(corps(g, ARMY7)).toBeUndefined();
    expect(g.flags[`sent:${index(D7)}`]).toBe(1);
  });

  it('in the line, the row waits and the corps keeps its name and its place; the month after it comes clear, the row takes it', () => {
    const g = open(3, null);
    const src = corps(g, ARMY7)!, home = g.regions[src.reg], line = inTheLine(g);
    relocate(g, src, line);
    month(g, 12);
    expect(corps(g, D7)).toBeUndefined();
    expect(corps(g, ARMY7)).toBe(src);
    expect(src.reg).toBe(line.id);
    expect(g.flags[`wait:${index(D7)}`]).toBe(12);
    expect(g.flags[`sent:${index(D7)}`]).toBeUndefined();
    relocate(g, src, home);
    month(g, 13);
    expect(at(g, corps(g, D7)!)).toBe('Sapporo');
    expect(corps(g, ARMY7)).toBeUndefined();
    // It came once.
    month(g, 14);
    expect(g.corps.filter((x) => x.name === D7 && !x.dead)).toHaveLength(1);
  });

  it('its half year is counted from the month the corps comes free: ten months in the line do not strike the row', () => {
    const g = open(3, null);
    const src = corps(g, ARMY7)!, home = g.regions[src.reg];
    relocate(g, src, inTheLine(g));
    months(g, 12, 21);
    expect(corps(g, D7)).toBeUndefined();
    expect(corps(g, ARMY7)).toBe(src);
    expect(g.flags[`sent:${index(D7)}`]).toBeUndefined();
    expect(g.flags[`wait:${index(D7)}`]).toBe(21);
    relocate(g, src, home);
    month(g, 22);
    expect(at(g, corps(g, D7)!)).toBe('Sapporo');
    expect(g.flags[`sent:${index(D7)}`]).toBe(1);
  });

  it('a corps already standing where the row forms is not lifted: in contact, it takes the row\'s name in its month', () => {
    const g = open(3, null);
    const src = corps(g, GARRISON)!;
    expect(at(g, src)).toBe('Hsuchow');
    // The enemy is beside Hsuchow, whoever was: a province next to it is China's for the test.
    const beside = g.regions[g.byName['Hsuchow'].nb[0]];
    for (const u of [...beside.units]) relocate(g, u, g.byName['Tokyo']);
    beside.ctrl = 'CHN';
    expect(inContact(g, src)).toBe(true);
    month(g, 30);
    const c = corps(g, LESS)!;
    expect(at(g, c)).toBe('Hsuchow');
    expect(corps(g, GARRISON)).toBeUndefined();
    expect(g.flags[`wait:${index(LESS)}`]).toBeUndefined();
  });

  it('the same corps in contact somewhere else is not lifted to Hsuchow: the row waits', () => {
    const g = open(3, null);
    const src = corps(g, GARRISON)!;
    const line = g.regions.find((r) => r.ctrl === 'JAP' && r.name !== 'Hsuchow' && !enemyHeld(g, r) && r.nb.some((j) => enemyHeld(g, g.regions[j])))!;
    relocate(g, src, line);
    expect(inContact(g, src)).toBe(true);
    month(g, 30);
    expect(corps(g, LESS)).toBeUndefined();
    expect(corps(g, GARRISON)).toBe(src);
    expect(src.reg).toBe(line.id);
    expect(g.flags[`wait:${index(LESS)}`]).toBe(30);
  });

  it("the contact rule's second form: the same corps there, relieved by a corps of its side it did not fight beside, is lifted to Hsuchow in the row's month", () => {
    // (The design director's ruling of 06:10Z, 26 November 2026: held only where nobody of its side keeps its province,
    // unless it took part in the battle last resolved there. No battle is recorded on this board.)
    const g = open(3, null);
    const src = corps(g, GARRISON)!;
    const line = g.regions.find((r) => r.ctrl === 'JAP' && r.name !== 'Hsuchow' && !enemyHeld(g, r) && r.nb.some((j) => enemyHeld(g, g.regions[j])))!;
    relocate(g, src, line);
    addCorps(g, 'JAP', 'Relief Test Corps', 'inf', 2, line.id, {});
    expect(corpsInContact(g, 'JAP', src)).toBe(false);
    g.turn = 30; startPhase(g, 'JAP');
    expect(corps(g, GARRISON)).toBeUndefined();
    expect(at(g, corps(g, LESS)!)).toBe('Hsuchow');
    expect(g.flags[`wait:${index(LESS)}`]).toBeUndefined();
  });
});

describe("the dial: the rule acts at stops 3 to 5, and at the book's stops the book's rows keep their book", () => {
  // The design director's ruling of 21:25Z, 25 November 2026: "at stops 1 and 2 the book's rows keep their book, as
  // for every other piece. The contact rule acts at stops 3 to 5."
  it.each([1, 2])('at stop %i, with the switch on, the row takes its corps out of the line in its month as with the switch off: no wait is written', (stop) => {
    const g = open(3, null);
    expect(setStop(g, stop, true)).toBe(true);
    const src = corps(g, ARMY7)!;
    relocate(g, src, inTheLine(g));
    expect(inContact(g, src)).toBe(true);
    expect(onLiveFront(g, 'JAP', src)).toBe(false);
    month(g, 12);
    expect(at(g, corps(g, D7)!)).toBe('Sapporo');
    expect(corps(g, ARMY7)).toBeUndefined();
    expect(g.flags[`wait:${index(D7)}`]).toBeUndefined();
    expect(g.flags[`sent:${index(D7)}`]).toBe(1);
  });

  it.each([4, 5])('at stop %i it acts as at stop 3: the row waits and the corps keeps its name and its place', (stop) => {
    const g = open(3, null);
    expect(setStop(g, stop, true)).toBe(true);
    const src = corps(g, ARMY7)!;
    const line = inTheLine(g);
    relocate(g, src, line);
    expect(inContact(g, src)).toBe(true);
    month(g, 12);
    expect(corps(g, D7)).toBeUndefined();
    expect(at(g, corps(g, ARMY7)!)).toBe(line.name);
    expect(g.flags[`wait:${index(D7)}`]).toBe(12);
    expect(g.flags[`sent:${index(D7)}`]).toBeUndefined();
  });
});

describe("a closed book brings the contact rule in: at the book's stops it acts where the row's book is closed", () => {
  // The design director's ruling of 22:05Z, 25 November 2026: "the contact rule acts where the stop is 3 to 5 OR the
  // row's theatre's book is closed". The row is due at Sapporo, far-side ground: it reads the Pacific's book (her
  // 22:12Z, reading A; history.ts `contactRuleActs`).
  const inLine = (stop: number, shut: string[]): { g: Game; src: Corps; line: Region } => {
    const g = open(3, null);
    expect(setStop(g, stop, true)).toBe(true);
    for (const t of shut) g.flags[`hist:off:${t}`] = 9;
    const src = corps(g, ARMY7)!, line = inTheLine(g);
    relocate(g, src, line);
    expect(inContact(g, src)).toBe(true);
    expect(onLiveFront(g, 'JAP', src)).toBe(false);
    month(g, 12);
    return { g, src, line };
  };
  it.each([1, 2])('at stop %i with the Pacific\'s book closed the row waits and the corps keeps its name and its place', (stop) => {
    const { g, src, line } = inLine(stop, ['pacific']);
    expect(corps(g, D7)).toBeUndefined();
    expect(corps(g, ARMY7)).toBe(src);
    expect(src.reg).toBe(line.id);
    expect(g.flags[`wait:${index(D7)}`]).toBe(12);
    expect(g.flags[`sent:${index(D7)}`]).toBeUndefined();
  });
  it.each([1, 2])('at stop %i with the Pacific\'s book open the row takes its corps out of the line at its date', (stop) => {
    const { g } = inLine(stop, []);
    expect(g.flags['hist:off:pacific']).toBeUndefined();
    expect(at(g, corps(g, D7)!)).toBe('Sapporo');
    expect(corps(g, ARMY7)).toBeUndefined();
    expect(g.flags[`wait:${index(D7)}`]).toBeUndefined();
    expect(g.flags[`sent:${index(D7)}`]).toBe(1);
  });
  // (A state set by hand that the game cannot reach: with the far side on the Pacific's book hangs on the west's
  // (`after: ['west']`, no `stays`), so claims.ts `closeBook` closes it in the same call that closes the west's. The
  // test is kept only to pin reading A: a far row reads the Pacific's book, never the west's. logic-1.)
  it.each([1, 2])('at stop %i with only the west\'s book closed by hand, the Pacific\'s open, the far row still takes at its date', (stop) => {
    const { g } = inLine(stop, ['west']);
    expect(g.flags['hist:off:pacific']).toBeUndefined();
    expect(at(g, corps(g, D7)!)).toBe('Sapporo');
    expect(corps(g, ARMY7)).toBeUndefined();
    expect(g.flags[`wait:${index(D7)}`]).toBeUndefined();
    expect(g.flags[`sent:${index(D7)}`]).toBe(1);
  });
});

// (logic-5: the two doors' twins the far side on.) With the far side on a row due on the old chart's ground goes
// through the far side's clause, not the old chart's statement, and reads the west's book (her 22:12Z, reading A); the
// draw (drawn.ts `heldAlready`) reads the Pacific's for a row due on far-side ground. The design director's ruling of
// 22:05Z, 25 November 2026: "the contact rule acts where the stop is 3 to 5 OR the row's theatre's book is closed".
describe("a closed book, the far side on: an old-chart row through the far side's clause, and the draw", () => {
  let added = 0;
  const addRow = (row: any[]): number => { rows.push(row); added++; return rows.length - 1; };
  afterEach(() => { while (added > 0) { rows.pop(); added--; } });
  /** Germany's "Test Corps" in contact at Grenzmark (Poznan beside it, Poland at war), a row of month 2 taking it whole to Magdeburg. */
  const west = (stop: number, shut: string[]): { g: Game; i: number; held: Corps } => {
    const g = open(3, null);
    expect(setStop(g, stop, true)).toBe(true);
    for (const t of shut) g.flags[`hist:off:${t}`] = 1;
    const held = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
    expect(inContact(g, held)).toBe(true);
    const i = addRow([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps'] }]);
    fought(g);
    g.turn = 2; startPhase(g, 'GER');
    return { g, i, held };
  };
  const german = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'GER' && c.name === name && !c.dead);
  it.each([1, 2])("at stop %i with the west's book closed (and the Pacific's with it, as the book closes them) the old-chart row waits and its corps stays in the fight", (stop) => {
    const { g, i, held } = west(stop, ['west', 'pacific']);
    expect(g.flags[`wait:${i}`]).toBe(2);
    expect(g.flags[`sent:${i}`]).toBeUndefined();
    expect(at(g, held)).toBe('Grenzmark');
    expect(german(g, 'Test Army')).toBeUndefined();
  });
  it.each([1, 2])("at stop %i with only the Pacific's book closed, the west's open, the old-chart row takes its corps at its date", (stop) => {
    const { g, i, held } = west(stop, ['pacific']);
    expect(g.flags[`wait:${i}`]).toBeUndefined();
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(held.dead).toBe(true);
    expect(at(g, german(g, 'Test Army')!)).toBe('Magdeburg');
  });

  /** Japan's "12th Army (5th Division)" put in the line, on no live front, and a row of month 12 for Sapporo that names the 5th Division. */
  const draw = (stop: number, shut: string[]): { g: Game; i: number; src: Corps; line: Region } => {
    const g = open(3, null);
    expect(setStop(g, stop, true)).toBe(true);
    for (const t of shut) g.flags[`hist:off:${t}`] = 9;
    const src = corps(g, '12th Army (5th Division)')!, line = inTheLine(g);
    relocate(g, src, line);
    expect(inContact(g, src)).toBe(true);
    expect(onLiveFront(g, 'JAP', src)).toBe(false);
    const i = addRow([12, 'JAP', 'Test Army (5th Division)', 'inf', 1, 'Sapporo', {}]);
    month(g, 12);
    return { g, i, src, line };
  };
  it.each([1, 2])("at stop %i with the Pacific's book closed the draw leaves the 5th Division in the fight: the corps keeps its name and its place", (stop) => {
    const { g, src, line } = draw(stop, ['pacific']);
    expect(corps(g, '12th Army (5th Division)')).toBe(src);
    expect(src.reg).toBe(line.id);
    expect(corps(g, 'Test Army (5th Division)')).toBeUndefined();
  });
  it.each([1, 2])("at stop %i with the Pacific's book open the row draws the 5th Division at its date, fight or not", (stop) => {
    const { g } = draw(stop, []);
    expect(corps(g, '12th Army (5th Division)')).toBeUndefined();
    expect(at(g, corps(g, 'Test Army (5th Division)')!)).toBe('Sapporo');
  });
});

describe('a row with a body of its own', () => {
  it("comes at that worth and takes nobody: the 56th Division lands at Rangoon at 0.67 while the Sakaguchi Detachment stands in the line, on no live front", () => {
    const g = open(3, null);
    lose(g, 'Cam Ranh', 'JAP', 'Saigon');
    month(g, 27);
    g.turn = 28;
    for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
    lose(g, 'Rangoon', 'JAP', 'Calcutta');
    const group = corps(g, SAKAGUCHI)!;
    relocate(g, group, inTheLine(g));
    expect(onLiveFront(g, 'JAP', group)).toBe(false);
    expect(inContact(g, group)).toBe(true);
    month(g, 31);
    const body = corps(g, D56)!;
    expect([at(g, body), body.divs, body.str, body.full]).toEqual(['Rangoon', 1, 0.67, 0.67]);
    expect(corps(g, SAKAGUCHI)).toBe(group);
    expect([group.str, group.full]).toEqual([0.33, 0.33]);
  });
});

describe('the player', () => {
  it("where his rows are offers he is asked as before with his corps in the line, and taken it forms: his order, not a date's", () => {
    const g = open(3, 'JAP');
    g.flags.offers = 1;
    const src = corps(g, ARMY7)!;
    relocate(g, src, inTheLine(g));
    expect(inContact(g, src)).toBe(true);
    month(g, 12);
    expect(corps(g, D7)).toBeUndefined();
    expect(corps(g, ARMY7)).toBe(src);
    expect(g.flags[`wait:${index(D7)}`]).toBeUndefined();
    expect(rowOffers(g).find((o) => o.name === D7)).toMatchObject({ since: 12, why: null });
    expect(takeOffer(g, index(D7))).toBeNull();
    expect(at(g, corps(g, D7)!)).toBe('Sapporo');
    expect(corps(g, ARMY7)).toBeUndefined();
  });

  it('where his rows come by their dates (offers off), a date lifts no corps of his out of the line either: the row waits', () => {
    const g = open(3, 'JAP');
    expect(g.flags.offers).toBeUndefined();
    const src = corps(g, ARMY7)!, home = g.regions[src.reg], line = inTheLine(g);
    relocate(g, src, line);
    month(g, 12);
    expect(corps(g, D7)).toBeUndefined();
    expect(corps(g, ARMY7)).toBe(src);
    expect(src.reg).toBe(line.id);
    expect(g.flags[`wait:${index(D7)}`]).toBe(12);
    relocate(g, src, home);
    month(g, 13);
    expect(at(g, corps(g, D7)!)).toBe('Sapporo');
  });
});
