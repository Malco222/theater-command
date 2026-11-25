// Two lines' flags on one row (release 2, the Pacific fold on the map's head 1125p). The map line's "a division is
// laid once" (drawn.ts; game.ts `expeditions`) keeps `offer:<row>`, the month a row's line told the player that it
// names divisions he holds already. The fold's offers (ruling B8; game.ts `offerKey`, `offerCount`) were kept under
// the same `offer:<row>` and `offer:n:<row>` until the two lines met: with the far side and the offers on, a row whose
// line had been told was read the next month as an offer open and unanswered, and the new divisions it still had to
// form never came. The fold's keys are now `asked:<row>` and `asked:n:<row>`.
//
// This file has the WHOLE WORLD ON and the OFFERS ON, for a player's Germany in the Balkans in March and April 1941
// (the board of tests/price-once-r2.test.ts, here on real data with no row added): the 2nd and 9th Panzer
// Divisions are his already in XXII Corps, the 50th Infantry in III Corps, the 8th Panzer in XV Corps. None of these
// rows takes a corps of his off the board, so none is an offer. A player's Japan's 5th Division row, which does take
// his corps, is still offered, under the fold's own key.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { divisionsOf } from '../src/campaign/armies';
import { createGame, declareWar, offersOn, relocate, rowOffers, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { farOn } from '../src/campaign/world';
import type { Corps, Game } from '../src/campaign/types';

const MARCH_41 = 19, APRIL_41 = 20, JUNE_41 = 22;
const ROWS = (setup as any).expeditions as any[];
const rowAt = (name: string): number => ROWS.findIndex((r) => r[1] === 'GER' && r[2] === name);
const PANZERS = '2nd and 9th Panzer Divisions', MOUNTAIN = '5th and 6th Mountain Divisions', INFANTRY = '50th, 72nd and 164th Infantry Divisions', EIGHTH = '8th and 14th Panzer Divisions';
/** The corps that fill Plovdiv in March, so that the 50th's row is told its line and then finds no room. */
const FILL = ['XII Corps', 'IX Corps', 'XXV Corps', 'V Corps', 'VI Corps'];

/** A player's Germany with Italy's war on Greece begun and Germany, Bulgaria, Hungary and Romania in it. */
function balkans(offers: boolean): Game {
  const g = startOpen(createGame(1, 'GER'));
  if (offers) g.flags.offers = 1;
  declareWar(g, 'ITA', 'GRE');
  declareWar(g, 'GER', 'GRE');
  for (const n of ['BUL', 'HUN', 'ROM']) { g.nations[n].align = 'axis'; declareWar(g, n, 'GRE'); }
  return g;
}
const month = (g: Game, turn: number): void => { g.turn = turn; startPhase(g, 'GER'); };
const corps = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'GER' && !c.dead && c.name === name);
const names = (g: Game, c: Corps): string[] => divisionsOf(g, c).map((d) => d.name);
/** Germany's living corps by name, place, divisions and strength. */
const army = (g: Game): string[] => g.corps.filter((c) => c.nat === 'GER' && !c.dead)
  .map((c) => `${c.name} | ${g.regions[c.reg].name} | ${c.divs} | ${c.str.toFixed(6)} | ${names(g, c).join(', ')}`).sort();
const told = (g: Game): string[] => g.headlines.filter((h) => h.nat === 'GER' && / (is|are) due (at|in) | was due to be formed (at|in) /.test(h.text)).map((h) => h.text);
const asked = (g: Game): string[] => Object.keys(g.flags).filter((k) => k.startsWith('asked:'));

/** The walk with Plovdiv full in March and emptied in April: the same moves with the offers on and off. */
function walk(offers: boolean): { g: Game; seen: Map<number, string[]>; held: number | undefined } {
  const g = balkans(offers);
  const fill = FILL.map((nm) => corps(g, nm)!);
  const home = fill.map((c) => g.regions[c.reg]);
  for (const c of fill) relocate(g, c, g.byName.Plovdiv);
  // (A full province sends a row on to the nearest ground of its own country with room, `roomNear`: for the March
  // month Germany's own ground is in her ally Bulgaria's keeping, so that there is none, and the row finds no room.)
  const lent = g.regions.filter((r) => r.ctrl === 'GER');
  for (const r of lent) r.ctrl = 'BUL';
  const seen = new Map<number, string[]>();
  month(g, MARCH_41);
  seen.set(MARCH_41, army(g));
  const held = g.flags[`sent:${rowAt(INFANTRY)}`];
  for (const r of lent) r.ctrl = 'GER';
  fill.forEach((c, k) => relocate(g, c, home[k]));
  for (let t = APRIL_41; t <= JUNE_41; t++) { month(g, t); seen.set(t, army(g)); }
  return { g, seen, held };
}

describe('a row whose line tells the player he holds its divisions is not an offer, with the far side and the offers on', () => {
  it('the line is told and offer:<row> keeps the month; no asked: flag is written and the offers page lists none of it', () => {
    const g = balkans(true);
    expect([farOn(g), offersOn(g)]).toEqual([true, true]);
    const was = names(g, corps(g, 'XXII Corps')!);
    month(g, MARCH_41);
    // (a) The map line's line, and its flag.
    expect(told(g).find((t) => t.includes('2nd and 9th Panzer Divisions are due in Sofia'))).toContain('you hold them already');
    expect(g.flags[`offer:${rowAt(PANZERS)}`]).toBe(MARCH_41);
    expect(g.flags[`offer:${rowAt(INFANTRY)}`]).toBe(MARCH_41);
    expect(g.flags[`sent:${rowAt(PANZERS)}`]).toBe(1);
    // (b) Nothing of the fold's.
    expect(g.flags[`asked:${rowAt(PANZERS)}`]).toBeUndefined();
    expect(g.flags[`asked:n:${rowAt(PANZERS)}`]).toBeUndefined();
    expect(asked(g)).toEqual([]);
    expect(rowOffers(g)).toEqual([]);
    month(g, APRIL_41);
    expect(g.flags[`offer:${rowAt(EIGHTH)}`]).toBe(APRIL_41);
    expect(asked(g)).toEqual([]);
    expect(rowOffers(g)).toEqual([]);
    // His corps are never moved, and nothing is raised twice.
    expect(names(g, corps(g, 'XXII Corps')!)).toEqual(was);
    expect(corps(g, PANZERS)).toBeUndefined();
    expect(names(g, corps(g, INFANTRY)!)).toEqual(['72nd Infantry Division', '164th Infantry Division']);
  });

  it('a row told its line and then held for room forms when room is made, as it does with the offers off', () => {
    const on = walk(true), off = walk(false);
    // March: the 50th's row is told and finds Plovdiv full; nothing of it forms in either game.
    expect(on.g.flags[`offer:${rowAt(INFANTRY)}`]).toBe(MARCH_41);
    expect(off.g.flags[`offer:${rowAt(INFANTRY)}`]).toBe(MARCH_41);
    expect(on.seen.get(MARCH_41)!.some((s) => s.startsWith(`${INFANTRY} |`))).toBe(false);
    expect([on.held, off.held]).toEqual([undefined, undefined]);
    // (c) April on: what it lays is exactly what it lays with the offers off, month by month.
    for (let t = MARCH_41; t <= JUNE_41; t++) expect(on.seen.get(t), `month ${t}`).toEqual(off.seen.get(t));
    expect(on.g.flags[`sent:${rowAt(INFANTRY)}`]).toBe(1);
    expect(names(on.g, corps(on.g, INFANTRY)!)).toEqual(['72nd Infantry Division', '164th Infantry Division']);
    expect(on.g.regions[corps(on.g, INFANTRY)!.reg].name).toBe('Plovdiv');
    expect(corps(on.g, MOUNTAIN)).toBeDefined();
    // Its line was told once, in March, and not again when it formed.
    expect(told(on.g).filter((t) => t.includes('The 50th Infantry Division is due in Plovdiv'))).toHaveLength(1);
    expect(told(on.g)).toEqual(told(off.g));
    expect(asked(on.g)).toEqual([]);
    expect(rowOffers(on.g)).toEqual([]);
  });
});

describe('a row that takes his corps is still offered, under the fold\'s own key', () => {
  it('a player\'s Japan is offered the 5th Division in month 2: asked:<row> holds the month, and offer:<row> is untouched', () => {
    const g = startOpen(createGame(5, 'JAP'));
    g.flags.offers = 1;
    for (let t = 1; t <= 2; t++) { g.turn = t; startPhase(g, 'JAP'); }
    const offers = rowOffers(g);
    expect(offers.map((o) => o.name)).toEqual(['21st Army (5th Division)']);
    const i = offers[0].id;
    expect(offers[0].since).toBe(2);
    expect(g.flags[`asked:${i}`]).toBe(2);
    expect(g.flags[`asked:n:${i}`]).toBe(offers[0].takes.length);
    expect(offers[0].takes.length).toBeGreaterThan(0);
    expect(g.flags[`offer:${i}`]).toBeUndefined();
    expect(g.corps.some((c) => c.nat === 'JAP' && !c.dead && c.name === '21st Army (5th Division)')).toBe(false);
    // It stands the next month, still under its own key, and its line is not the map line's.
    g.turn = 3; startPhase(g, 'JAP');
    expect(rowOffers(g).map((o) => o.id)).toEqual([i]);
    expect(g.flags[`asked:${i}`]).toBe(2);
    expect(g.flags[`offer:${i}`]).toBeUndefined();
  });
});
