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

import { describe, expect, it } from 'vitest';
import { corpsInContact } from '../src/campaign/drawn';
import { atWar, createGame, declareWar, relocate, rowOffers, startPhase, takeOffer } from '../src/campaign/game';
import { onLiveFront } from '../src/campaign/planfronts';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';
import stage from '../data/campaign/stage.json';
import bridge from '../unreal/TheaterCommand/Rules/campaign-bridge.ts?raw';

const D7 = '7th Division (Asahikawa)', ARMY7 = '6th Army (7th Division)';
const LESS = 'Tientsin-Pukow railway garrison (21st Division, less its Bataan detachment)', GARRISON = 'Tientsin-Pukow railway garrison (21st Division)';
const D56 = '56th Division', SAKAGUCHI = 'Sakaguchi Detachment (56th Mixed Infantry Group)';

/** A grand campaign with the far side on and the switch 'fromWait' set by hand, as a campaign opened with it on carries it. */
const open = (seed: number, seat: string | null): Game => { const g = startOpen(createGame(seed, seat)); g.flags.fromWait = 1; return g; };
const inContact = (g: Game, c: Corps): boolean => corpsInContact(g, c.nat, c);
const rows = (setup as any).expeditions as any[][];
const index = (name: string): number => rows.findIndex((r) => r[1] === 'JAP' && r[2] === name);
const corps = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead);
const at = (g: Game, c: Corps): string => g.regions[c.reg].name;
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'JAP'); };
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
  it("the switch's note promises no Options row the bridge does not have: one is owed, named by the UI director", () => {
    const note = (stage as any)._fromWait as string;
    expect(note).toMatch(/one Options row, named by the UI director/);
    if (!/fromwait/i.test(bridge)) expect(note).toMatch(/that row is not built yet/);
  });

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
