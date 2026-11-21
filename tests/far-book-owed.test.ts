// A book row is made only when its place is taken (the design director's rulings of 21 November 2026 on Rangoon's
// trace, to ruling 234; src/campaign/stations.ts, `made`), read with the director's rulings of 06:53Z: a row of
// several places is made with the last of them; a row's first try at a place is the book's, the whole force and
// whatever odds; after it the staff's own reckoning; while a row is owed, the corps it landed stay held to it.
// Behind the far switch: the book's rows are the far side's data (tests/pacific-war-off.test.ts).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import '../src/campaign/ai';
import { addCorps, createGame, declareWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { besieged, townHeld } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { bookAttacks, bookReckoning, bookWatch, stations } from '../src/campaign/stations';
import type { Corps, Game } from '../src/campaign/types';

const war = (): Game => {
  const g = startOpen(createGame(3, null));
  setStop(g, 1, true);
  return g;
};
/** Japan's blow has fallen in its month. */
const strike = (g: Game, month = 28): void => { g.flags['st:done:JAP:south'] = month; for (const e of ['USA', 'UK']) declareWar(g, 'JAP', e); };
const index = (name: string): number => stations()!.attacks!.findIndex((a) => a.name === name);
const at = (g: Game, c: Corps): string => g.regions[c.reg].name;
/** A corps of the table put down by hand at a strength. */
const put = (g: Game, nat: string, name: string, place: string, str: number, o: { kind?: 'inf' | 'arm'; xp?: number; divs?: number } = {}): Corps => {
  const c = addCorps(g, nat, name, o.kind ?? 'inf', o.divs ?? 1, place, { xp: o.xp ?? 0.3 });
  c.str = str;
  return c;
};
/** Clears a province of everyone's field corps, and gives it to a side. */
const clear = (g: Game, place: string, ctrl?: string): void => {
  const r = g.byName[place];
  for (const c of g.corps) if (!c.dead && c.reg === r.id) { c.dead = true; }
  r.units = r.units.filter((c) => !c.dead);
  if (ctrl) r.ctrl = ctrl;
};
/** Sets the rows other than these aside, so that a test reads only its own. */
const only = (g: Game, ...names: string[]): void => {
  stations()!.attacks!.forEach((a, i) => { if (!names.includes(a.name)) g.flags[`ba:${i}`] = 1; });
};

type Told = { name: string; what: string; why?: string; to?: string; taken?: boolean; held?: boolean; force?: string[] };
const told: Told[] = [];
const listen = (): void => {
  told.length = 0;
  bookWatch.tell = (e) => { told.push({ name: e.row.name, what: e.what, why: e.why, to: e.to?.name, taken: e.taken, held: e.held, force: e.force?.map((c) => c.name) }); };
};
afterEach(() => { bookWatch.tell = null; });
const of = (name: string, what: string): Told[] => told.filter((t) => t.name === name && t.what === what);

const ARMY15 = '15th Army (33rd and 55th Divisions)';

describe('a book row made only when its place is taken (the design director, 21 November 2026)', () => {
  it('the staff sets its own reckoning of a row\'s later tries', () => {
    expect(bookReckoning.why).toBeTypeOf('function');
  });

  it('a row of several places is made with the last of them, going on to the next at its next ask', () => {
    const g = war();
    strike(g);
    g.turn = 32;
    only(g, 'Prome and Yenangyaung');
    const i = index('Prome and Yenangyaung');
    for (const p of ['Rangoon', 'Prome', 'Yenangyaung']) clear(g, p, p === 'Rangoon' ? 'JAP' : undefined);
    // (Both towns empty: each falls to the walk-in, so that the row's own steps are read.)
    for (const p of ['Prome', 'Yenangyaung']) g.flags[`guard:${g.byName[p].id}`] = 100;
    const army = put(g, 'JAP', ARMY15, 'Rangoon', 1, { divs: 2 });
    listen();
    bookAttacks(g, 'JAP');
    expect(of('Prome and Yenangyaung', 'made').map((t) => [t.to, t.taken, t.held])).toEqual([['Prome', true, false]]);
    expect([g.byName.Prome.ctrl, at(g, army), g.flags[`ba:${i}`]]).toEqual(['JAP', 'Prome', undefined]);
    // The month's second asking goes on to Yenangyaung; with it taken the row is made.
    bookAttacks(g, 'JAP');
    expect(of('Prome and Yenangyaung', 'made').map((t) => [t.to, t.taken])).toEqual([['Prome', true], ['Yenangyaung', true]]);
    expect([g.byName.Yenangyaung.ctrl, g.flags[`ba:${i}`]]).toEqual(['JAP', 32]);
  });

  it('a lost row without the record\'s figures goes in again only at the staff\'s own odds', () => {
    const g = war();
    strike(g);
    g.turn = 33;
    only(g, 'The pursuit to the Chindwin');
    const i = index('The pursuit to the Chindwin');
    clear(g, 'Mandalay', 'JAP');
    clear(g, 'Kalewa');
    const army = put(g, 'JAP', ARMY15, 'Mandalay', 0.35, { divs: 2 });
    const corps4 = put(g, 'UK', 'IV Corps (23rd Indian Division)', 'Kalewa', 1, { xp: 0.6 });
    listen();
    bookAttacks(g, 'JAP');
    // Its first try is the book's: it goes in whatever the odds, and loses.
    const first = of('The pursuit to the Chindwin', 'made');
    expect(first.map((t) => [t.to, t.taken, t.held])).toEqual([['Kalewa', false, true]]);
    expect([g.flags[`ba:${i}`], g.flags[`bat:${i}`]]).toEqual([undefined, 33]);
    // Next month it is owed and asks the staff's own odds, which it does not have.
    g.turn = 34;
    if (army.reg !== g.byName.Mandalay.id) army.reg = g.byName.Mandalay.id;
    listen();
    bookAttacks(g, 'JAP');
    expect(of('The pursuit to the Chindwin', 'made')).toEqual([]);
    expect(of('The pursuit to the Chindwin', 'owed').map((t) => t.why)).toEqual([expect.stringMatching(/^the staff's own odds, /)]);
    // With the odds on its side it goes in.
    corps4.str = 0.1;
    army.str = 1;
    army.org = 1;
    listen();
    bookAttacks(g, 'JAP');
    expect(of('The pursuit to the Chindwin', 'made').map((t) => t.to)).toEqual(['Kalewa']);
  });

  it('on its first try it goes in as the book has it, a corps of it already round the town staying there: the siege is left to the staff only after a try', () => {
    const g = war();
    strike(g);
    g.turn = 32;
    only(g, 'Prome and Yenangyaung');
    for (const p of ['Rangoon', 'Toungoo']) clear(g, p, 'JAP');
    clear(g, 'Prome');
    const prome = g.byName.Prome;
    expect(townHeld(g, prome)).toBe(true);
    // One corps of the row already stands round Prome's town, the other at Rangoon.
    const army = put(g, 'JAP', ARMY15, 'Prome', 1, { divs: 2 });
    const tank = put(g, 'JAP', '1st Tank Regiment', 'Rangoon', 0.25, { kind: 'arm' });
    expect(besieged(g, prome)).toBe(true);
    listen();
    bookAttacks(g, 'JAP');
    // The tank regiment comes up from Rangoon to its division, and the row goes in: the town opens to the whole force.
    expect(of('Prome and Yenangyaung', 'due').map((t) => t.force)).toEqual([[ARMY15, '1st Tank Regiment']]);
    expect(of('Prome and Yenangyaung', 'owed')).toEqual([]);
    expect([at(g, army), at(g, tank), g.byName.Prome.ctrl]).toEqual(['Prome', 'Prome', 'JAP']);
  });

  it('while a landing row is owed, the corps it landed stay held to it', () => {
    const g = war();
    strike(g);
    g.turn = 33;
    only(g, 'Tulagi');
    const gc = g.byName.Guadalcanal;
    clear(g, 'Guadalcanal');
    clear(g, 'Rabaul', 'JAP');
    const nlf = g.corps.find((c) => c.nat === 'JAP' && !c.dead && /Naval landing force/.test(c.name) && stations()!.attacks![index('Tulagi')].force.includes(c.name));
    const force = nlf ?? put(g, 'JAP', stations()!.attacks![index('Tulagi')].force[0], 'Rabaul', 0.2);
    force.reg = g.byName.Rabaul.id;
    if (!g.byName.Rabaul.units.includes(force)) g.byName.Rabaul.units.push(force);
    force.hold = true;
    listen();
    bookAttacks(g, 'JAP');
    // Ashore, with Tulagi's guard holding the town: the row is owed, and the landing force stays the row's.
    expect(of('Tulagi', 'made').map((t) => [t.taken, t.held])).toEqual([[true, true]]);
    expect([townHeld(g, gc), gc.ctrl === 'JAP', force.hold]).toEqual([true, false, true]);
    expect(g.flags[`ba:${index('Tulagi')}`]).toBeUndefined();
  });
});
