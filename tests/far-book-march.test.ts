// A dated attack by land sets out early enough to arrive by the move rules (the design director's 11:41Z of 22
// November 2026 to ruling 234; src/campaign/stations.ts, `setsOut`, `stride`, `wayTo`).
//
// The book put a named corps down at its row's door from wherever it stood. Under the bound of 07:44Z the six wars
// read on bdc0c86a showed sixteen such moves of more than a month's march from month 56 on (the 11th Army's rows from
// Shasi, Yochow and Hankow to Chuanhsien, five to seven marches at four a month; the 27th Division from Hankow to
// Kukong, eleven). The arrival is the record's date and the march is the game's own: a corps that needs more than a
// month's march to its door leaves early enough to arrive, the last month of its road being the date's own, when it
// is put down at the door as it always was. So no corps is moved more than a month's march in a month. One still
// further off on the date marches its month, and the row is owed until it is up. A corps with no road over its
// side's ground is put down as before: its route is asked of the record. The boards are set by hand.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { addCorps, atWar, createGame, declareWar, relocate } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import { bookAttacks, bookWatch, broughtWatch, stations, type Attack } from '../src/campaign/stations';
import type { Corps, Game } from '../src/campaign/types';

const ICHIGO = 'JAP:ichigo', K45 = 'JAP:kukong45';
const A58 = '11th Army (34th, 58th and 68th Divisions)', A40 = '11th Army (40th and 116th Divisions)';
const IMB = '23rd Army (independent mixed brigades, for Kukong)';
// The road south from Hankow: five provinces to Kweilin's door.
const ROAD = ['Yochow', 'Changsha', 'Hengyang', 'Lingling', 'Chuanhsien'];

const table = (): Attack[] => stations()!.attacks!;
const idx = (name: string): number => table().findIndex((a) => a.name === name);
afterEach(() => { bookWatch.tell = null; broughtWatch.tell = null; });

/**
 * A war at stop 1 in month `turn`: Japan at war with China, the choice of 1944 gone in month 57 (and that of 1945 in
 * 64 where `k45` says so), every dated attack of the book made already but the ones named in `live`.
 */
const war = (turn: number, live: string[], k45 = false): Game => {
  const g = startOpen(createGame(3, null));
  setStop(g, 1, true);
  if (!atWar(g, 'JAP', 'CHN')) declareWar(g, 'JAP', 'CHN');
  g.turn = turn;
  g.flags[`st:done:${ICHIGO}`] = 57;
  if (k45) g.flags[`st:done:${K45}`] = 64;
  table().forEach((a, i) => { if (!live.includes(a.name)) g.flags[`ba:${i}`] = 1; });
  return g;
};
/** A place in Japan's hands, with nobody of the other side in it. */
const japans = (g: Game, ...names: string[]): void => { for (const name of names) { const r = g.byName[name]; r.ctrl = 'JAP'; for (const c of r.units) if (c.nat !== 'JAP') c.dead = true; r.units = r.units.filter((u) => u.nat === 'JAP'); } };
/** A place with nobody in its field and no town guard: who comes walks in. */
const open = (g: Game, ...names: string[]): void => { for (const name of names) { const r = g.byName[name]; for (const c of r.units) if (c.nat !== 'JAP') c.dead = true; r.units = r.units.filter((u) => u.nat === 'JAP'); g.flags[`guard:${r.id}`] = 100; } };
/** A row of the chart's set down by hand where the case has it, alone there, in the field, with a full month's moves. */
const stand = (g: Game, name: string, place: string): Corps => {
  const at = g.byName[place];
  for (const u of [...at.units]) if (u.nat === 'JAP' && u.name !== name) { u.dead = true; at.units = at.units.filter((x) => x !== u); }
  let c = g.corps.find((x) => x.nat === 'JAP' && !x.dead && x.name === name);
  if (!c) c = addCorps(g, 'JAP', name, 'inf', 1, place, { xp: 0.3 });
  else if (c.reg !== at.id) relocate(g, c, at);
  c.ap = c.apMax; c.fought = false;
  return c;
};
const where = (g: Game, c: Corps): string => g.regions[c.reg].name;
/** The month turns: every corps of Japan's has its moves again. */
const nextMonth = (g: Game): void => { g.turn++; for (const c of g.corps) if (c.nat === 'JAP' && !c.dead) { c.ap = c.apMax; c.fought = false; } };

type Moved = { corps: string; from: string; to: string; months: number | null | undefined; leg: boolean };
const moved: Moved[] = [];
const owed: string[] = [];
const listen = (): void => {
  moved.length = 0; owed.length = 0;
  broughtWatch.tell = (e) => { moved.push({ corps: e.corps.name, from: e.from.name, to: e.to.name, months: e.months, leg: !!e.leg }); };
  bookWatch.tell = (e) => { if (e.what === 'owed') owed.push(`${e.row.name}: ${e.why}`); };
};

describe('a dated attack by land sets out early enough to arrive by the move rules (the design director, 11:41Z of 22 November 2026)', () => {
  it('two months of march from Kweilin\'s door: the row leaves the month before its date, a month\'s march and no more, and is put down on the date', () => {
    const g = war(61, ['Kweilin']);
    japans(g, ...ROAD);
    stand(g, A58, 'Chuanhsien');
    const c = stand(g, A40, 'Hankow');
    listen();
    // Two months before its date: nothing. Its road is two months, and the second of them is the date's own.
    bookAttacks(g, 'JAP');
    expect(where(g, c)).toBe('Hankow');
    expect(moved).toEqual([]);
    // The month before: it marches its month along the road, by the move rules, and has no moves left.
    nextMonth(g);
    expect(g.turn).toBe(62);
    bookAttacks(g, 'JAP');
    expect(where(g, c)).toBe('Lingling');
    expect(c.ap).toBe(0);
    expect(moved).toEqual([{ corps: A40, from: 'Hankow', to: 'Lingling', months: 2, leg: true }]);
    // (Asked again in the month, as the staff's month closes: it has made its march.)
    c.ap = c.apMax;
    bookAttacks(g, 'JAP');
    expect(where(g, c)).toBe('Lingling');
    expect(moved.length).toBe(1);
    expect(g.flags[`ba:${idx('Kweilin')}`]).toBeUndefined();
    // Its date: within a month of the door, it is put down there as it always was, and the row goes in.
    nextMonth(g);
    open(g, 'Kweilin');
    bookAttacks(g, 'JAP');
    expect(moved[1]).toEqual({ corps: A40, from: 'Lingling', to: 'Chuanhsien', months: 1, leg: false });
    expect(g.byName['Kweilin'].ctrl).toBe('JAP');
    expect(g.flags[`ba:${idx('Kweilin')}`]).toBe(63);
  });

  it('still two months off on its date: it is not put down at the door; it marches its month and the row is owed until it is up', () => {
    const g = war(63, ['Kweilin']);
    japans(g, ...ROAD);
    open(g, 'Kweilin');
    const lead = stand(g, A58, 'Chuanhsien');
    const c = stand(g, A40, 'Hankow');
    listen();
    bookAttacks(g, 'JAP');
    expect(where(g, c)).toBe('Lingling');
    expect(where(g, lead)).toBe('Chuanhsien');
    expect(moved).toEqual([{ corps: A40, from: 'Hankow', to: 'Lingling', months: 2, leg: true }]);
    expect(owed).toEqual(['Kweilin: not all of its corps stand next to the place']);
    expect(g.byName['Kweilin'].ctrl).toBe('CHN');
    // (Nor in the same month's second asking: a march and a put-down are not made in one month.)
    c.ap = c.apMax;
    bookAttacks(g, 'JAP');
    expect(where(g, c)).toBe('Lingling');
    expect(g.byName['Kweilin'].ctrl).toBe('CHN');
    // The month after, it is up, and the row goes in.
    nextMonth(g);
    bookAttacks(g, 'JAP');
    expect(moved[1]).toEqual({ corps: A40, from: 'Lingling', to: 'Chuanhsien', months: 1, leg: false });
    expect(g.byName['Kweilin'].ctrl).toBe('JAP');
  });

  it('within a month\'s march of its door nothing is changed: no setting out the month before, and the put-down on the date', () => {
    const g = war(62, ['Kweilin']);
    japans(g, ...ROAD);
    stand(g, A58, 'Chuanhsien');
    const c = stand(g, A40, 'Yochow');
    listen();
    bookAttacks(g, 'JAP');
    expect(where(g, c)).toBe('Yochow');
    expect(moved).toEqual([]);
    nextMonth(g);
    open(g, 'Kweilin');
    bookAttacks(g, 'JAP');
    expect(moved).toEqual([{ corps: A40, from: 'Yochow', to: 'Chuanhsien', months: 1, leg: false }]);
    expect(g.byName['Kweilin'].ctrl).toBe('JAP');
  });

  it('with no road over its side\'s ground it is put down as before, on the date and not sooner', () => {
    const g = war(62, ['Kweilin']);
    // (Only the door is Japan's: nothing joins it to Hankow.)
    japans(g, 'Chuanhsien');
    stand(g, A58, 'Chuanhsien');
    const c = stand(g, A40, 'Hankow');
    listen();
    bookAttacks(g, 'JAP');
    expect(where(g, c)).toBe('Hankow');
    nextMonth(g);
    open(g, 'Kweilin');
    bookAttacks(g, 'JAP');
    expect(moved).toEqual([{ corps: A40, from: 'Hankow', to: 'Chuanhsien', months: null, leg: false }]);
    expect(g.byName['Kweilin'].ctrl).toBe('JAP');
  });

  it('a corps an earlier row of the book still names does not set out for a later one', () => {
    // Month 64. Kweilin's row (63) is owed: its door is China's still. Kukong's (65) names the 40th and 116th too,
    // and its first door open is Canton, two months of march from Hankow round by Kanchow and the coast.
    const board = (): { g: Game; c: Corps } => {
      const g = war(64, ['Kweilin', 'Kukong'], true);
      japans(g, 'Yochow', 'Changsha', 'Hengyang', 'Kanchow', 'Swatow', 'Waichow');
      stand(g, A58, 'Hengyang');
      stand(g, IMB, 'Canton');
      return { g, c: stand(g, A40, 'Hankow') };
    };
    const a = board();
    listen();
    bookAttacks(a.g, 'JAP');
    expect(where(a.g, a.c)).toBe('Hankow');
    expect(moved).toEqual([]);
    expect(owed).toContain('Kweilin: not all of its corps stand next to the place');
    // With Kweilin's row made, the same board: it sets out for Kukong's door.
    const b = board();
    b.g.flags[`ba:${idx('Kweilin')}`] = 63;
    listen();
    bookAttacks(b.g, 'JAP');
    expect(moved.length).toBe(1);
    expect(moved[0]).toMatchObject({ corps: A40, from: 'Hankow', months: 2, leg: true });
    expect(where(b.g, b.c)).not.toBe('Hankow');
    expect(b.c.ap).toBe(0);
  });
});
