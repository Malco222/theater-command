// A dated attack with a book of its own (the design director's rulings of 07:42Z, 07:44Z and 07:46Z, 22 November 2026,
// on Nanning's trace; src/campaign/stations.ts, `ownBook`).
//
// By the book the month list (line.ts) gives leave and does not say who goes where: in two wars of three at the first
// stop the 23rd Army's two rows took Liuchow together in November 1944 and Nanning, open beside them the same month,
// was passed by and never fell. The record's rows are to be laid as dated attacks, as British Borneo's are. But the
// table is the Pacific's: its theatre, its great choice's calendar, and its last month, 35. A row for 1944 laid in it
// was never asked. So a dated attack may name its own choice (`book: "JAP:ichigo"`):
//   asked     at a stop that keeps the book, once that choice has gone and while the choice's theatre keeps the book;
//             the table's last month and the table's theatre do not bind it;
//   its month as written, with no slide: the month list does not slide either, so the row and the list read the
//             same months (the choice of 1944 is dated 56 and goes in 57);
//   the leave the month list stays the leave it is: the row is not asked at a place the list keeps shut that month.
//             It stays owed, and goes in when the list opens the place;
//   its end   its own `until`, or its book's close, when it lapses with nothing done;
//   held      the staff's ships leave its corps for it, as they leave the table's (`bookedAhead`).
// Rows without the field are as they were. Nothing with the far side's switch off: the table is the far side's data.
// The rows here are the tests' own, laid in the table for the test and taken out after it.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { addCorps, atWar, createGame, declareWar } from '../src/campaign/game';
import { HISTORY, onBook, setStop } from '../src/campaign/history';
import { lineShut } from '../src/campaign/line';
import { startOpen } from '../src/campaign/staff';
import { bookAttacks, bookWatch, bookedAhead, broughtWatch, stations, stationsBind, type Attack } from '../src/campaign/stations';
import type { Corps, Game } from '../src/campaign/types';

const ICHIGO = 'JAP:ichigo';
const TWENTY2 = '23rd Army (22nd Division)';
const OTHER = 'a division that is not named';

const table = (): Attack[] => stations()!.attacks!;
let before = 0;
beforeEach(() => { before = table().length; });
afterEach(() => { table().length = before; bookWatch.tell = null; broughtWatch.tell = null; });
/** A row of the test's own, laid at the table's end. */
const lay = (row: Omit<Attack, 'name'> & { name: string }): number => table().push({ ...row, name: `test: ${row.name}` }) - 1;

/**
 * A war at stop `stop` in month `turn`: Japan at war with China, the choice of 1944 gone in month `went` (0: not
 * yet), and every dated attack but the tests' own made already.
 */
const war = (turn: number, o: { stop?: number; went?: number; human?: string | null } = {}): Game => {
  const g = startOpen(createGame(3, o.human ?? null));
  if ((o.stop ?? 1) !== 3) setStop(g, o.stop ?? 1, true);
  if (!atWar(g, 'JAP', 'CHN')) declareWar(g, 'JAP', 'CHN');
  g.turn = turn;
  if (o.went !== 0) g.flags[`st:done:${ICHIGO}`] = o.went ?? 57;
  table().forEach((a, i) => { if (!a.name.startsWith('test: ')) g.flags[`ba:${i}`] = 1; });
  return g;
};
/** A place in Japan's hands, with nobody of the other side in it. */
const japans = (g: Game, name: string): void => { const r = g.byName[name]; r.ctrl = 'JAP'; r.units = r.units.filter((u) => u.nat === 'JAP'); };
/** A place with nobody in its field and no town guard: who comes walks in. */
const open = (g: Game, name: string): void => { const r = g.byName[name]; for (const c of r.units) if (c.nat !== 'JAP') c.dead = true; r.units = r.units.filter((u) => u.nat === 'JAP'); g.flags[`guard:${r.id}`] = 100; };
const put = (g: Game, name: string, place: string): Corps => addCorps(g, 'JAP', name, 'inf', 1, place, { xp: 0.3 });
const where = (g: Game, c: Corps): string => g.regions[c.reg].name;
/** The West River road as the record had it in November 1944: Wuchow and Kweihsien Japan's, Nanning open beside them. */
const westRiver = (g: Game): void => { for (const p of ['Wuchow', 'Kweihsien']) japans(g, p); open(g, 'Nanning'); };

type Told = { name: string; what: string; why?: string; taken?: boolean };
const told: Told[] = [];
const listen = (): void => { told.length = 0; bookWatch.tell = (e) => { told.push({ name: e.row.name, what: e.what, why: e.why, taken: e.taken }); }; };

describe('the two books', () => {
  it('are the data\'s: the table is the Pacific\'s and ends in 1942; China\'s choice of 1944 has a theatre of its own', () => {
    const B = stations()!;
    expect(B.theatre).toBe('pacific');
    expect(B.until).toBeLessThan(40);
    expect(HISTORY.theatre[ICHIGO]).toBe('china');
    expect(HISTORY.dates.JAP.ichigo).toBe(56);
    // The month list opens Nanning in November 1944, month 63, and keeps it shut the month before.
    const g = war(62);
    expect(onBook(g, 'china')).toBe(true);
    expect(lineShut(g, 'JAP', g.byName.Nanning)).toBe(true);
    g.turn = 63;
    expect(lineShut(g, 'JAP', g.byName.Nanning)).toBe(false);
  });
});

describe('a row that names its own choice', () => {
  it('goes in in its own month as written, long after the table\'s last: Nanning in 63 with the list open', () => {
    const i = lay({ month: 63, name: 'Nanning', at: ['Nanning'], force: [TWENTY2], from: ['Kweihsien'], book: ICHIGO });
    const g = war(63);
    westRiver(g);
    const c = put(g, TWENTY2, 'Kweihsien'), other = put(g, OTHER, 'Kweihsien');
    // The table no longer binds anyone, and the choice went a month after its date: neither moves the row's month.
    expect(stationsBind(g, 'JAP')).toBe(false);
    expect(g.flags[`st:done:${ICHIGO}`]).toBe(57);
    expect(bookedAhead(g, c)).toBe(true);
    expect(bookedAhead(g, other)).toBe(false);
    listen();
    bookAttacks(g, 'JAP');
    expect(told.map((t) => `${t.name}: ${t.what}`)).toEqual(['test: Nanning: due', 'test: Nanning: made']);
    expect(g.byName.Nanning.ctrl).toBe('JAP');
    expect(where(g, c)).toBe('Nanning');
    expect(where(g, other)).toBe('Kweihsien');
    expect(g.flags[`ba:${i}`]).toBe(63);
    // Made, it holds its corps no longer.
    expect(bookedAhead(g, c)).toBe(false);
  });

  it('is not asked before its month, though its choice has gone and the place is open', () => {
    lay({ month: 63, name: 'Kweihsien', at: ['Kweihsien'], force: [TWENTY2], from: ['Wuchow'], book: ICHIGO });
    const g = war(62);
    japans(g, 'Wuchow');
    open(g, 'Kweihsien');
    const c = put(g, TWENTY2, 'Wuchow');
    // (The list opens Kweihsien in 62: nothing but the row's own month holds it.)
    expect(lineShut(g, 'JAP', g.byName.Kweihsien)).toBe(false);
    listen();
    bookAttacks(g, 'JAP');
    expect(told).toEqual([]);
    expect(g.byName.Kweihsien.ctrl).toBe('CHN');
    expect(bookedAhead(g, c)).toBe(true);
    g.turn = 63;
    bookAttacks(g, 'JAP');
    expect(g.byName.Kweihsien.ctrl).toBe('JAP');
  });

  it('is not asked until its choice has gone, nor where the choice was never taken up', () => {
    lay({ month: 63, name: 'Nanning', at: ['Nanning'], force: [TWENTY2], from: ['Kweihsien'], book: ICHIGO });
    for (const went of [0, -1]) {
      const g = war(63, { went });
      westRiver(g);
      const c = put(g, TWENTY2, 'Kweihsien');
      listen();
      bookAttacks(g, 'JAP');
      expect(told, `went ${went}`).toEqual([]);
      expect(g.byName.Nanning.ctrl).toBe('CHN');
      expect(bookedAhead(g, c)).toBe(false);
    }
  });

  it('puts its corps down at its door from where it stands, and tells the harness of it', () => {
    lay({ month: 63, name: 'Nanning', at: ['Nanning'], force: [TWENTY2], from: ['Kweihsien'], book: ICHIGO });
    const g = war(63);
    westRiver(g);
    const c = put(g, TWENTY2, 'Canton');
    const brought: string[] = [];
    broughtWatch.tell = (e) => { brought.push(`${e.corps.name}: ${e.from.name} to ${e.to.name} for ${e.row.name}`); };
    bookAttacks(g, 'JAP');
    expect(brought).toEqual([`${TWENTY2}: Canton to Kweihsien for test: Nanning`]);
    expect(where(g, c)).toBe('Nanning');
    expect(g.byName.Nanning.ctrl).toBe('JAP');
  });

  it('is owed while its door is not its side\'s, and goes in when it is', () => {
    lay({ month: 63, name: 'Nanning', at: ['Nanning'], force: [TWENTY2], from: ['Kweihsien'], book: ICHIGO });
    const g = war(63);
    japans(g, 'Wuchow');
    open(g, 'Nanning');
    expect(g.byName.Kweihsien.ctrl).toBe('CHN');
    const c = put(g, TWENTY2, 'Wuchow');
    listen();
    bookAttacks(g, 'JAP');
    expect(told.map((t) => `${t.what}: ${t.why}`)).toEqual(['owed: not all of its corps stand next to the place']);
    expect(where(g, c)).toBe('Wuchow');
    japans(g, 'Kweihsien');
    g.turn = 64;
    bookAttacks(g, 'JAP');
    expect(g.byName.Nanning.ctrl).toBe('JAP');
  });
});

describe('the month list stays the leave it is', () => {
  it('a row at a place the list keeps shut is owed and its corps stay; it goes in the month the list opens the place', () => {
    const i = lay({ month: 62, name: 'Nanning', at: ['Nanning'], force: [TWENTY2], from: ['Kweihsien'], book: ICHIGO });
    const g = war(62);
    westRiver(g);
    const c = put(g, TWENTY2, 'Canton');
    expect(lineShut(g, 'JAP', g.byName.Nanning)).toBe(true);
    listen();
    bookAttacks(g, 'JAP');
    expect(told).toEqual([{ name: 'test: Nanning', what: 'owed', why: 'the month list keeps the place shut', taken: undefined }]);
    expect(g.byName.Nanning.ctrl).toBe('CHN');
    // (Nothing is brought to the door of a place that is shut.)
    expect(where(g, c)).toBe('Canton');
    expect(g.flags[`ba:${i}`]).toBeUndefined();
    expect(bookedAhead(g, c)).toBe(true);
    g.turn = 63;
    listen();
    bookAttacks(g, 'JAP');
    expect(told.map((t) => t.what)).toEqual(['due', 'made']);
    expect(g.byName.Nanning.ctrl).toBe('JAP');
    expect(where(g, c)).toBe('Nanning');
    expect(g.flags[`ba:${i}`]).toBe(63);
  });
});

describe('its end', () => {
  it('lapses with nothing done when its book is closed', () => {
    const i = lay({ month: 63, name: 'Nanning', at: ['Nanning'], force: [TWENTY2], from: ['Kweihsien'], book: ICHIGO });
    const g = war(63);
    westRiver(g);
    const c = put(g, TWENTY2, 'Kweihsien');
    expect(bookedAhead(g, c)).toBe(true);
    g.flags['hist:off:china'] = 63;
    expect(onBook(g, 'china')).toBe(false);
    listen();
    bookAttacks(g, 'JAP');
    expect(told).toEqual([]);
    expect(g.byName.Nanning.ctrl).toBe('CHN');
    expect(where(g, c)).toBe('Kweihsien');
    expect(g.flags[`ba:${i}`]).toBeUndefined();
    // Its corps is the staff's again.
    expect(bookedAhead(g, c)).toBe(false);
  });

  it('is over after its own last month, as a row of the table is', () => {
    const i = lay({ month: 64, until: 64, name: 'Tushan', at: ['Tushan'], force: [TWENTY2], from: ['Liuchow'], book: ICHIGO });
    const g = war(65);
    const c = put(g, TWENTY2, 'Canton');
    expect(bookedAhead(g, c)).toBe(false);
    listen();
    bookAttacks(g, 'JAP');
    expect(told.map((t) => `${t.what}: ${t.why}`)).toEqual(['over: the record\'s attack was called off']);
    expect(g.flags[`ba:${i}`]).toBe(65);
    expect(where(g, c)).toBe('Canton');
  });
});

describe('who it binds', () => {
  it('nobody at the stops that keep no book, and nobody in a war where a player is Japan', () => {
    lay({ month: 63, name: 'Nanning', at: ['Nanning'], force: [TWENTY2], from: ['Kweihsien'], book: ICHIGO });
    for (const o of [{ stop: 3 }, { stop: 4 }, { stop: 5 }, { stop: 1, human: 'JAP' }]) {
      const g = war(63, o);
      westRiver(g);
      const c = put(g, TWENTY2, 'Kweihsien');
      listen();
      bookAttacks(g, 'JAP');
      expect(told, JSON.stringify(o)).toEqual([]);
      expect(g.byName.Nanning.ctrl).toBe('CHN');
      expect(bookedAhead(g, c)).toBe(false);
    }
  });

  it('the second stop as the first', () => {
    lay({ month: 63, name: 'Nanning', at: ['Nanning'], force: [TWENTY2], from: ['Kweihsien'], book: ICHIGO });
    const g = war(63, { stop: 2 });
    westRiver(g);
    put(g, TWENTY2, 'Kweihsien');
    bookAttacks(g, 'JAP');
    expect(g.byName.Nanning.ctrl).toBe('JAP');
  });
});

describe('the table\'s own rows', () => {
  it('are not asked past the table\'s last month because a row with its own book is', () => {
    lay({ month: 63, name: 'Nanning', at: ['Nanning'], force: [TWENTY2], from: ['Kweihsien'], book: ICHIGO });
    lay({ month: 63, name: 'Kweilin, by the table', at: ['Kweilin'], force: [OTHER], from: ['Chuanhsien'] });
    const g = war(63);
    westRiver(g);
    japans(g, 'Chuanhsien');
    put(g, TWENTY2, 'Kweihsien');
    const other = put(g, OTHER, 'Chuanhsien');
    listen();
    bookAttacks(g, 'JAP');
    expect([...new Set(told.map((t) => t.name))]).toEqual(['test: Nanning']);
    expect(g.byName.Kweilin.ctrl).toBe('CHN');
    expect(bookedAhead(g, other)).toBe(false);
  });

  it('are asked in the table\'s months as they were, and a row with its own book waits for its own', () => {
    lay({ month: 29, name: 'Nanning', at: ['Nanning'], force: [TWENTY2], from: ['Kweihsien'], book: ICHIGO });
    lay({ month: 29, name: 'Kweilin, by the table', at: ['Kweilin'], force: [OTHER], from: ['Chuanhsien'] });
    const g = war(29, { went: 0 });
    g.flags['st:done:JAP:south'] = 28;
    westRiver(g);
    japans(g, 'Chuanhsien');
    open(g, 'Kweilin');
    const c = put(g, TWENTY2, 'Kweihsien'), other = put(g, OTHER, 'Chuanhsien');
    expect(stationsBind(g, 'JAP')).toBe(true);
    expect(bookedAhead(g, other)).toBe(true);
    expect(bookedAhead(g, c)).toBe(false);
    listen();
    bookAttacks(g, 'JAP');
    expect([...new Set(told.map((t) => t.name))]).toEqual(['test: Kweilin, by the table']);
    expect(g.byName.Nanning.ctrl).toBe('CHN');
    expect(where(g, other)).toBe('Kweilin');
  });
});
