// A book row is made only when its place is taken (the design director's rulings of 21 November 2026 on Rangoon's
// trace, to ruling 234; src/campaign/stations.ts, `made`), read with the director's rulings of 06:53Z: a row of
// several places is made with the last of them; a row's first try at a place is the book's, the whole force and
// whatever odds; after it the staff's own reckoning; while a row is owed, the corps it landed stay held to it, and
// once it is not, however its place fell, they are the staff's again.
// Behind the far switch: the book's rows are the far side's data (tests/pacific-war-off.test.ts).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import '../src/campaign/ai';
import { addCorps, createGame, declareWar, relocate } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { besieged, fall, joinTown, townHeld } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { bookAttacks, bookReckoning, bookStations, bookWatch, stations } from '../src/campaign/stations';
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
/**
 * A garrison set in a place's town by hand. On the far chart a town's own guard is the record's figure for its land
 * and class, and a village (Prome, Menado) has none (far-guards.json): a town that is to hold has its garrison.
 */
const garrisonTown = (g: Game, nat: string, name: string, place: string, str: number): Corps => {
  const c = put(g, nat, name, place, str), r = g.byName[place];
  r.units = r.units.filter((u) => u !== c);
  joinTown(g, r, c);
  return c;
};
/** Sets the rows other than these aside, so that a test reads only its own. */
const only = (g: Game, ...names: string[]): void => {
  stations()!.attacks!.forEach((a, i) => { if (!names.includes(a.name)) g.flags[`ba:${i}`] = 1; });
};

/** Wake's Marines, the record's detachment, in their works. */
const garrisonWake = (g: Game): void => {
  const nm = 'Wake detachment, 1st Defense Battalion';
  const c = g.corps.find((x) => x.nat === 'USA' && !x.dead && x.name === nm) ?? addCorps(g, 'USA', nm, 'fort', 1, 'Wake', { xp: 0.5 });
  if (c.reg !== g.byName.Wake.id) relocate(g, c, g.byName.Wake);
  c.str = 0.35;
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

  it('a lost row without `until` goes in again only at the staff\'s own odds', () => {
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

  it("a lost row with `until` keeps the record's own months of trying, whatever the odds; one without goes in again only at the staff's own odds (the director's of 11:17Z)", () => {
    // Jitra has the record's figures and no `until`: lost, it waits on the staff's own odds.
    const g = war();
    put(g, 'JAP', '25th Army (5th Division)', 'Singora', 0.1, { xp: 0.6 });
    put(g, 'JAP', '1st Tank Regiment', 'Singora', 0.05, { kind: 'arm', xp: 0.6 });
    put(g, 'UK', '11th Indian Division (6th and 15th Brigades)', 'Penang', 1, { xp: 0.05 });
    g.turn = 28;
    strike(g);
    only(g, 'Jitra');
    for (const nat of ['UK', 'JAP']) bookStations(g, nat);
    expect(stations()!.attacks![index('Jitra')].until).toBeUndefined();
    listen();
    bookAttacks(g, 'JAP');
    expect(of('Jitra', 'made').map((t) => t.taken)).toEqual([false]);
    g.turn = 29;
    listen();
    bookAttacks(g, 'JAP');
    expect(of('Jitra', 'made')).toEqual([]);
    expect(of('Jitra', 'owed').map((t) => t.why)).toEqual([expect.stringMatching(/^the staff's own odds, /)]);

    // Bataan in January has `until` February: lost, it goes in again in February at whatever odds, and is over after.
    const h = war();
    strike(h);
    h.turn = 29;
    only(h, 'Bataan, January');
    const i = index('Bataan, January');
    expect(stations()!.attacks![i].until).toBe(30);
    clear(h, 'Manila', 'JAP');
    clear(h, 'Bataan', 'USA');
    const japan = ['14th Army (65th Brigade)', '14th Army (16th Division)'].map((nm) => put(h, 'JAP', nm, 'Manila', 0.1));
    put(h, 'USA', 'Philippine Army', 'Bataan', 1, { xp: 0.3 });
    listen();
    bookAttacks(h, 'JAP');
    expect(of('Bataan, January', 'made').map((t) => [t.to, t.taken])).toEqual([['Bataan', false]]);
    h.turn = 30;
    for (const c of japan) { c.ap = c.apMax; if (c.reg !== h.byName.Manila.id) relocate(h, c, h.byName.Manila); }
    listen();
    bookAttacks(h, 'JAP');
    expect(of('Bataan, January', 'owed')).toEqual([]);
    expect(of('Bataan, January', 'made').map((t) => [t.to, t.taken])).toEqual([['Bataan', false]]);
    h.turn = 31;
    listen();
    bookAttacks(h, 'JAP');
    expect(of('Bataan, January', 'over').map((t) => t.why)).toEqual(['the record\'s attack was called off']);
    expect(h.flags[`ba:${i}`]).toBe(31);
  });

  it('on its first try it goes in as the book has it, a corps of it already round the town staying there: the siege is left to the staff only after a try', () => {
    const g = war();
    strike(g);
    g.turn = 32;
    only(g, 'Prome and Yenangyaung');
    for (const p of ['Rangoon', 'Toungoo']) clear(g, p, 'JAP');
    clear(g, 'Prome');
    const prome = g.byName.Prome;
    // (A village of Burma has no guard of its own on the far chart: a garrison holds Prome's town.)
    expect(townHeld(g, prome)).toBe(false);
    garrisonTown(g, 'UK', 'Prome garrison (a company of the Burma Rifles)', 'Prome', 0.04);
    expect(townHeld(g, prome)).toBe(true);
    // One corps of the row already stands round Prome's town, the other at Rangoon.
    const army = put(g, 'JAP', ARMY15, 'Prome', 1, { divs: 2 });
    const tank = put(g, 'JAP', '1st Tank Regiment', 'Rangoon', 0.25, { kind: 'arm' });
    expect(besieged(g, prome)).toBe(true);
    listen();
    bookAttacks(g, 'JAP');
    // The tank regiment comes up from Rangoon to its division, and the row goes in with the whole force. The garrison
    // holds the town against it (a guard would open to that force at the summons; a garrison is a fight of its own).
    expect(of('Prome and Yenangyaung', 'due').map((t) => t.force)).toEqual([[ARMY15, '1st Tank Regiment']]);
    expect(of('Prome and Yenangyaung', 'owed')).toEqual([]);
    expect(of('Prome and Yenangyaung', 'made').map((t) => [t.taken, t.held])).toEqual([[false, true]]);
    expect([at(g, army), at(g, tank), g.byName.Prome.ctrl]).toEqual(['Prome', 'Prome', 'UK']);
    // The try made, the siege is the staff's: the next month the row is owed, its corps round the town.
    g.turn = 33;
    listen();
    bookAttacks(g, 'JAP');
    expect(of('Prome and Yenangyaung', 'due')).toEqual([]);
    expect(of('Prome and Yenangyaung', 'owed').map((t) => t.why)).toEqual(['its corps stand round the town, which holds']);
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

  it('once a landing row is not owed, however its place fell, the corps it landed are the staff\'s again', () => {
    const g = war();
    strike(g);
    g.turn = 33;
    only(g, 'Tulagi');
    const gc = g.byName.Guadalcanal;
    clear(g, 'Guadalcanal');
    clear(g, 'Rabaul', 'JAP');
    const name = stations()!.attacks![index('Tulagi')].force[0];
    const force = g.corps.find((c) => c.nat === 'JAP' && !c.dead && c.name === name) ?? put(g, 'JAP', name, 'Rabaul', 0.2);
    relocate(g, force, g.byName.Rabaul);
    force.hold = true;
    bookAttacks(g, 'JAP');
    expect([force.reg, townHeld(g, gc), force.hold]).toEqual([gc.id, true, true]);
    // The staff storms the town in its own month: the row is not owed any more, and the landing force is let go.
    fall(g, gc, 'JAP', 'storm');
    g.turn = 34;
    listen();
    bookAttacks(g, 'JAP');
    expect(of('Tulagi', 'over').map((t) => t.why)).toEqual(['the place is its side\'s']);
    expect([force.hold, g.flags[`bkl:${force.id}`]]).toEqual([false, undefined]);
  });

  it('a place it has tried and that is lost again is not tried as a first time', () => {
    const g = war();
    strike(g);
    g.turn = 32;
    only(g, 'Prome and Yenangyaung');
    for (const p of ['Rangoon', 'Prome', 'Yenangyaung']) clear(g, p, p === 'Rangoon' ? 'JAP' : undefined);
    g.flags[`guard:${g.byName.Prome.id}`] = 100;
    const army = put(g, 'JAP', ARMY15, 'Rangoon', 1, { divs: 2 });
    put(g, 'UK', 'IV Corps (17th Indian Division)', 'Yenangyaung', 1, { divs: 3, xp: 0.6 });
    listen();
    // Prome is walked into; Yenangyaung's first try is the book's, and is lost.
    bookAttacks(g, 'JAP');
    bookAttacks(g, 'JAP');
    expect(of('Prome and Yenangyaung', 'made').map((t) => [t.to, t.taken])).toEqual([['Prome', true], ['Yenangyaung', false]]);
    // Britain takes Prome back, and the army stands at Rangoon again.
    expect(army.dead).toBeFalsy();
    relocate(g, army, g.byName.Rangoon);
    g.byName.Prome.ctrl = 'UK';
    put(g, 'UK', '1st Burma Division', 'Prome', 1, { divs: 3, xp: 0.6 });
    army.str = 0.5;
    g.turn = 33;
    listen();
    bookAttacks(g, 'JAP');
    // Prome was tried once: it goes in again only at the staff's own odds.
    expect(of('Prome and Yenangyaung', 'made')).toEqual([]);
    expect(of('Prome and Yenangyaung', 'owed').map((t) => [t.to, t.why])).toEqual([['Prome', expect.stringMatching(/^the staff's own odds, /)]]);
  });

  it('a landing row thrown back goes in again only at the staff\'s own landing odds', () => {
    const g = war();
    strike(g);
    g.turn = 33;
    only(g, 'Tulagi');
    const gc = g.byName.Guadalcanal;
    clear(g, 'Guadalcanal');
    clear(g, 'Rabaul', 'JAP');
    const name = stations()!.attacks![index('Tulagi')].force[0];
    const force = g.corps.find((c) => c.nat === 'JAP' && !c.dead && c.name === name) ?? put(g, 'JAP', name, 'Rabaul', 0.2);
    relocate(g, force, g.byName.Rabaul);
    force.str = 0.2;
    force.hold = false;
    put(g, 'UK', '1st Fiji Infantry Regiment', 'Guadalcanal', 1, { xp: 0.6 });
    listen();
    bookAttacks(g, 'JAP');
    // Its first try is the book's: it lands whatever the odds, and is thrown back; the row is owed and holds its force.
    expect(of('Tulagi', 'made').map((t) => [t.taken, t.held])).toEqual([[false, true]]);
    expect([gc.ctrl === 'JAP', force.hold, g.flags[`ba:${index('Tulagi')}`]]).toEqual([false, true, undefined]);
    // Next month it asks the staff's landing odds where anything fights, which it does not have.
    g.turn = 34;
    if (force.reg !== g.byName.Rabaul.id) relocate(g, force, g.byName.Rabaul);
    force.ap = force.apMax;
    // (The plan lets its landing forces go in its own month, staff.ts `freed`: the owed row holds them again.)
    force.hold = false;
    listen();
    bookAttacks(g, 'JAP');
    expect(of('Tulagi', 'made')).toEqual([]);
    expect(of('Tulagi', 'owed').map((t) => t.why)).toEqual([expect.stringMatching(/^the staff's own odds for a landing, /)]);
    expect(force.hold).toBe(true);
    // When the book no longer binds, the staff has its landing force again.
    g.turn = 45;
    bookStations(g, 'JAP');
    expect([force.hold, g.flags[`bkl:${force.id}`]]).toEqual([false, undefined]);
  });

  it('a landed corps a later row goes with stays held to that row, and is let go when that row is not owed', () => {
    const g = war();
    strike(g);
    g.turn = 29;
    declareWar(g, 'JAP', 'NED');
    only(g, 'Menado', 'Kendari');
    const [menado, kendari] = [g.byName.Menado, g.byName.Kendari];
    clear(g, 'Davao', 'JAP');
    clear(g, 'Menado');
    clear(g, 'Kendari');
    // Menado's town holds: a village of the Indies has no guard of its own on the far chart, so by a garrison.
    expect(townHeld(g, menado)).toBe(false);
    garrisonTown(g, 'UK', 'Menado garrison (a company of the KNIL)', 'Menado', 0.04);
    expect(townHeld(g, menado)).toBe(true);
    const name = stations()!.attacks![index('Menado')].force[0];
    const force = g.corps.find((c) => c.nat === 'JAP' && !c.dead && c.name === name) ?? put(g, 'JAP', name, 'Davao', 0.2);
    relocate(g, force, g.byName.Davao);
    force.hold = false;
    listen();
    bookAttacks(g, 'JAP');
    expect(of('Menado', 'made').map((t) => [t.taken, t.held])).toEqual([[true, true]]);
    expect([force.hold, g.flags[`bkl:${force.id}`]]).toEqual([true, index('Menado') + 1]);
    // The staff storms Menado's town; Kendari's landing is not asked this month. Menado's row is over, and its
    // landing force stays held, now to Kendari's row.
    fall(g, menado, 'JAP', 'storm');
    g.turn = 30;
    g.flags[`bat:${index('Kendari')}`] = 30;
    listen();
    bookAttacks(g, 'JAP');
    expect(of('Menado', 'over').map((t) => t.why)).toEqual(['the place is its side\'s']);
    expect([force.hold, g.flags[`bkl:${force.id}`]]).toEqual([true, index('Kendari') + 1]);
    // Kendari falls some other way: its row is over, and the force is the staff's again.
    kendari.ctrl = 'JAP';
    g.turn = 31;
    listen();
    bookAttacks(g, 'JAP');
    expect(of('Kendari', 'over').map((t) => t.why)).toEqual(['the place is its side\'s']);
    expect([force.hold, g.flags[`bkl:${force.id}`]]).toEqual([false, undefined]);
  });

  it('Wake: a force the first try left held is let go when the taking makes the row', () => {
    const g = war();
    strike(g);
    g.turn = 28;
    only(g, 'Wake: the first try', 'Wake: the taking');
    const [first, taking] = [index('Wake: the first try'), index('Wake: the taking')];
    const name = stations()!.attacks![first].force[0];
    const force = g.corps.find((c) => c.nat === 'JAP' && !c.dead && c.name === name) ?? put(g, 'JAP', name, 'Kwajalein', 0.1);
    relocate(g, force, g.byName.Kwajalein);
    force.hold = true;
    garrisonWake(g);
    listen();
    bookAttacks(g, 'JAP');
    expect(of('Wake: the first try', 'made').map((t) => t.taken)).toEqual([false]);
    expect(of('Wake: the taking', 'made').map((t) => t.taken)).toEqual([true]);
    expect([g.byName.Wake.ctrl, g.flags[`ba:${taking}`], force.hold, g.flags[`bkl:${force.id}`]]).toEqual(['JAP', 28, false, undefined]);
  });

  it('Wake: when the first try is called off, the force it left held is held to the taking', () => {
    const g = war();
    strike(g);
    g.turn = 28;
    only(g, 'Wake: the first try', 'Wake: the taking');
    const [first, taking] = [index('Wake: the first try'), index('Wake: the taking')];
    const name = stations()!.attacks![first].force[0];
    const force = g.corps.find((c) => c.nat === 'JAP' && !c.dead && c.name === name) ?? put(g, 'JAP', name, 'Kwajalein', 0.1);
    relocate(g, force, g.byName.Kwajalein);
    force.hold = true;
    garrisonWake(g);
    g.flags[`bat:${taking}`] = 28;
    listen();
    bookAttacks(g, 'JAP');
    expect(of('Wake: the first try', 'made').map((t) => t.taken)).toEqual([false]);
    expect([force.hold, g.flags[`bkl:${force.id}`]]).toEqual([true, first + 1]);
    // The record called the first try off after its month: its force is the taking's now, and stays held.
    g.turn = 29;
    g.flags[`bat:${taking}`] = 29;
    force.hold = false;
    listen();
    bookAttacks(g, 'JAP');
    expect(of('Wake: the first try', 'over').map((t) => t.why)).toEqual(['the record\'s attack was called off']);
    expect([force.hold, g.flags[`bkl:${force.id}`]]).toEqual([true, taking + 1]);
  });
});
