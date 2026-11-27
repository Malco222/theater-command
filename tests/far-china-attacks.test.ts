// China's dated attacks of 1944-45 and Waichow's months (the design director's rulings of 07:42Z, 07:44Z, 07:46Z,
// 07:48Z, 07:55Z, 08:10Z and 08:12Z of 22 November 2026 on the second rules builder's Nanning trace; the historian's
// china-1944-guangxi-takers.md, canton-rows-1939-45.md and waichow-1938-45.md).
//
// Data only, in data/campaign/china-line.json (history.stations.attacks, appended to the Pacific's table as the file
// is laid, and the month list's Waichow). The form is stations.ts `ownBook` (far-china-book.test.ts): each row names
// its own choice, is asked in its own month as written at a stop that keeps the book, and not at a place the month
// list keeps shut. The record's takers:
//   61  Wuchow      the 104th Division (the chart's '21st Army (Canton)') and the 22nd, up the West River from Canton
//   62  Kweihsien   the same two, from Wuchow; they split at Kweiping
//   63  Kweilin     the 11th Army's rows that hold the 58th and the 40th, from Chuanhsien
//   63  Liuchow     the 11th Army's 3rd and 13th from the north, the 104th from the south-east
//   63  Nanning     the 22nd, up the Yu River
//   64  Tushan      the 3rd and 13th, from Liuchow (its only month: reached and given up in December 1944)
//   64  Lungchow    the 22nd, from Nanning
//   65  Waichow     the 104th, east of Canton (Huizhou and Haifeng: one province on the chart)
//   65  Kukong      the 40th from the north with the 23rd Army's brigades from Canton (the choice of 1945)
//   66  Kanchow     the 27th Division, from Kukong's side (the choice of 1945)
// The states are set by hand; one asking of the book is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { addCorps, atWar, createGame, declareWar, relocate } from '../src/campaign/game';
import { HISTORY, onBook, setStop } from '../src/campaign/history';
import { lineShut } from '../src/campaign/line';
import { startOpen } from '../src/campaign/staff';
import { bookAttacks, bookWatch, broughtWatch, stations, type Attack } from '../src/campaign/stations';
import type { Corps, Game } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';
import world from '../data/campaign/world.json';
import chinaLine from '../data/campaign/china-line.json';

const ICHIGO = 'JAP:ichigo', K45 = 'JAP:kukong45';
const C104 = '21st Army (Canton)', D22 = '23rd Army (22nd Division)';
const A58 = '11th Army (34th, 58th and 68th Divisions)', A40 = '11th Army (40th and 116th Divisions)', A3 = '11th Army (3rd and 13th Divisions)';
const A27 = '11th Army (27th Division)', IMB = '23rd Army (independent mixed brigades, for Kukong)';
const ROWS: [number, string, string, string[], string[], string][] = [
  [61, 'Wuchow, up the West River', 'Wuchow', [C104, D22], ['Canton'], ICHIGO],
  [62, 'Kweihsien and Kweiping', 'Kweihsien', [C104, D22], ['Wuchow'], ICHIGO],
  [63, 'Kweilin', 'Kweilin', [A58, A40], ['Chuanhsien'], ICHIGO],
  [63, 'Liuchow', 'Liuchow', [A3, C104], ['Kweilin', 'Kweihsien'], ICHIGO],
  [63, 'Nanning', 'Nanning', [D22], ['Kweihsien'], ICHIGO],
  [64, 'Tushan', 'Tushan', [A3], ['Liuchow'], ICHIGO],
  [64, 'Lungchow and the road to Indochina', 'Lungchow', [D22], ['Nanning'], ICHIGO],
  [65, 'Waichow and the coast east of Canton', 'Waichow', [C104], ['Canton', 'Tungkun'], ICHIGO],
  [65, 'Kukong', 'Kukong', [A40, IMB], ['Lingling', 'Chuanhsien', 'Canton'], K45],
  [66, 'Kanchow', 'Kanchow', [A27], ['Kukong'], K45],
];

const table = (): Attack[] => stations()!.attacks!;
// (The rows that name a choice of Japan's: Britain's in East Africa are rows of the same table, far-africa-attacks.test.ts.)
const mine = (): Attack[] => table().filter((a) => a.book !== undefined && a.book.startsWith('JAP:'));
const idx = (name: string): number => table().findIndex((a) => a.name === name);
afterEach(() => { bookWatch.tell = null; broughtWatch.tell = null; });

/**
 * A war at stop `stop` in month `turn`: Japan at war with China, the choice of 1944 gone in month 57 and that of 1945
 * in month 64 where `k45` says so, the Pacific's own dated attacks all made already, and of China's the ones named
 * in `done`.
 */
const war = (turn: number, o: { stop?: number; k45?: boolean; done?: string[] } = {}): Game => {
  const g = startOpen(createGame(3, null));
  if ((o.stop ?? 1) !== 3) setStop(g, o.stop ?? 1, true);
  if (!atWar(g, 'JAP', 'CHN')) declareWar(g, 'JAP', 'CHN');
  g.turn = turn;
  g.flags[`st:done:${ICHIGO}`] = 57;
  if (o.k45) g.flags[`st:done:${K45}`] = 64;
  table().forEach((a, i) => { if (a.book === undefined || o.done?.includes(a.name)) g.flags[`ba:${i}`] = 1; });
  return g;
};
/** A place in Japan's hands, with nobody of the other side in it. */
const japans = (g: Game, ...names: string[]): void => { for (const name of names) { const r = g.byName[name]; r.ctrl = 'JAP'; r.units = r.units.filter((u) => u.nat === 'JAP'); } };
/** A place with nobody in its field and no town guard: who comes walks in. */
const open = (g: Game, ...names: string[]): void => { for (const name of names) { const r = g.byName[name]; for (const c of r.units) if (c.nat !== 'JAP') c.dead = true; r.units = r.units.filter((u) => u.nat === 'JAP'); g.flags[`guard:${r.id}`] = 100; } };
/** A row of the chart's set down by hand where the case has it: the one on the board moved, or one of that name put there. */
const stand = (g: Game, name: string, place: string): Corps => {
  const c = g.corps.find((x) => x.nat === 'JAP' && !x.dead && x.name === name);
  if (!c) return addCorps(g, 'JAP', name, 'inf', 1, place, { xp: 0.3 });
  if (c.reg !== g.byName[place].id) relocate(g, c, g.byName[place]);
  return c;
};
const where = (g: Game, c: Corps): string => g.regions[c.reg].name;
const made = (g: Game, name: string): boolean => (g.flags[`ba:${idx(name)}`] ?? 0) > 0;

type Told = { name: string; what: string; why?: string; taken?: boolean };
const told: Told[] = [];
const moved: [string, string, string][] = [];
const listen = (): void => {
  told.length = 0; moved.length = 0;
  bookWatch.tell = (e) => { if (e.row.book !== undefined) told.push({ name: e.row.name, what: e.what, why: e.why, taken: e.taken }); };
  broughtWatch.tell = (e) => { moved.push([e.corps.name, e.from.name, e.to.name]); };
};
const whats = (name: string): string[] => told.filter((t) => t.name === name).map((t) => t.what);

describe("China's dated attacks in the line file", () => {
  it('ten rows, after the Pacific\'s in the one table, each naming its own choice', () => {
    expect(mine().map((a) => [a.month, a.name, a.at.join('+'), a.force, a.from, a.book])).toEqual(ROWS.map((r) => [r[0], r[1], r[2], r[3], r[4], r[5]]));
    // (One row has a last month: Tushan's, the month it was reached and given up. The design director's 11:41Z of 22 November 2026.)
    expect(mine().filter((a) => a.until !== undefined).map((a) => [a.name, a.until])).toEqual([['Tushan', 64]]);
    // (Laid as the file is: its own block holds the ten, and they stand at the end of Japan's rows in the table.
    // Britain's rows in East Africa are laid after them: far-africa-attacks.test.ts.)
    const own = (chinaLine as any).history.stations.attacks as Attack[];
    expect(own).toHaveLength(10);
    const japans = table().filter((a) => a.book === undefined || a.book.startsWith('JAP:'));
    expect(japans.slice(-10).map((a) => a.name)).toEqual(ROWS.map((r) => r[1]));
    expect(table().slice(0, japans.length)).toEqual(japans);
    // (None by sea, none with a record's fight; one with a last month, Tushan's: see the case above.)
    for (const a of mine()) expect([a.sea, a.met, a.until, HISTORY.theatre[a.book!]], a.name).toEqual([undefined, undefined, a.name === 'Tushan' ? 64 : undefined, 'china']);
    expect([HISTORY.dates.JAP.ichigo, HISTORY.dates.JAP.kukong45]).toEqual([56, 64]);
  });

  it('each place opens in the month list in its row\'s month, and each door is a neighbour the list holds open that month', () => {
    for (const [month, name, place, , doors] of ROWS) {
      const g = war(month);
      expect(onBook(g, 'china')).toBe(true);
      const to = g.byName[place];
      expect(lineShut(g, 'JAP', to), `${name}: ${place} in ${month}`).toBe(false);
      g.turn = month - 1;
      expect(lineShut(g, 'JAP', to), `${name}: ${place} in ${month - 1}`).toBe(true);
      g.turn = month;
      for (const d of doors) {
        expect(to.nb.includes(g.byName[d].id), `${name}: ${d} beside ${place}`).toBe(true);
        expect(lineShut(g, 'JAP', g.byName[d]), `${name}: ${d} in ${month}`).toBe(false);
      }
    }
  });

  it('every corps named is a row of Japan\'s that the chart has', () => {
    const first = ((world as any).corps as any[]).filter((r) => r[0] === 'JAP').map((r) => r[1] as string);
    const dated = ((setup as any).expeditions as any[]).filter((r) => r[1] === 'JAP').map((r) => r[2] as string);
    for (const [, name, , force] of ROWS) for (const nm of force) expect(first.includes(nm) || dated.includes(nm), `${name}: ${nm}`).toBe(true);
    expect(first).toContain(C104);
  });

  it('Waichow is in the month list: one span, Japan\'s from January 1945 to the end, and China\'s before it', () => {
    const L = (chinaLine as any).history.lines[0];
    expect(L.spans.Waichow).toEqual([{ from: 65, to: 72, kind: 'ichigo' }]);
    expect(L.back.Waichow).toBe(74);
    const g = war(64);
    expect([g.byName.Waichow.ctrl, lineShut(g, 'JAP', g.byName.Waichow)]).toEqual(['CHN', true]);
    g.turn = 65;
    expect(lineShut(g, 'JAP', g.byName.Waichow)).toBe(false);
  });
});

describe('up the West River in 1944', () => {
  it('September, month 61: the 104th and the 22nd take Wuchow from Canton, and nothing else of the book is asked yet', () => {
    const g = war(61);
    const c104 = stand(g, C104, 'Canton'), d22 = stand(g, D22, 'Canton');
    open(g, 'Wuchow');
    listen();
    bookAttacks(g, 'JAP');
    expect(g.byName.Wuchow.ctrl).toBe('JAP');
    expect([where(g, c104), where(g, d22)]).toEqual(['Wuchow', 'Wuchow']);
    expect(told.map((t) => [t.name, t.what])).toEqual([[ROWS[0][1], 'due'], [ROWS[0][1], 'made']]);
    expect(made(g, ROWS[0][1])).toBe(true);
    // (Not a month sooner.)
    const early = war(60);
    stand(early, C104, 'Canton'); stand(early, D22, 'Canton'); open(early, 'Wuchow');
    listen();
    bookAttacks(early, 'JAP');
    expect([told, early.byName.Wuchow.ctrl]).toEqual([[], 'CHN']);
  });

  it('October, month 62: the same two go on from Wuchow to Kweihsien', () => {
    const g = war(62, { done: [ROWS[0][1]] });
    japans(g, 'Wuchow');
    const c104 = stand(g, C104, 'Wuchow'), d22 = stand(g, D22, 'Wuchow');
    open(g, 'Kweihsien');
    listen();
    bookAttacks(g, 'JAP');
    expect([g.byName.Kweihsien.ctrl, where(g, c104), where(g, d22)]).toEqual(['JAP', 'Kweihsien', 'Kweihsien']);
    expect(whats(ROWS[1][1])).toEqual(['due', 'made']);
  });

  it('November, month 63: they split. The 22nd takes Nanning alone; the 104th goes north to Liuchow with the 3rd and 13th', () => {
    const g = war(63, { done: [ROWS[0][1], ROWS[1][1]] });
    japans(g, 'Wuchow', 'Kweihsien', 'Chuanhsien', 'Kweilin');
    const c104 = stand(g, C104, 'Kweihsien'), d22 = stand(g, D22, 'Kweihsien'), a3 = stand(g, A3, 'Kweilin');
    open(g, 'Liuchow', 'Nanning');
    listen();
    bookAttacks(g, 'JAP');
    expect([g.byName.Liuchow.ctrl, g.byName.Nanning.ctrl]).toEqual(['JAP', 'JAP']);
    expect([where(g, d22), where(g, c104), where(g, a3)]).toEqual(['Nanning', 'Liuchow', 'Liuchow']);
    // (Kweilin is its side's already here: its row is over, with nothing done.)
    expect([whats(ROWS[2][1]), whats(ROWS[3][1]), whats(ROWS[4][1])]).toEqual([['over'], ['due', 'made'], ['due', 'made']]);
    // (Nobody had to be carried: each stood at its door.)
    expect(moved).toEqual([]);
  });

  it('November, month 63: Kweilin falls to the 11th Army\'s rows from Chuanhsien', () => {
    const g = war(63, { done: [ROWS[0][1], ROWS[1][1], ROWS[3][1], ROWS[4][1]] });
    japans(g, 'Chuanhsien');
    const a58 = stand(g, A58, 'Chuanhsien'), a40 = stand(g, A40, 'Chuanhsien');
    open(g, 'Kweilin');
    listen();
    bookAttacks(g, 'JAP');
    expect([g.byName.Kweilin.ctrl, where(g, a58), where(g, a40)]).toEqual(['JAP', 'Kweilin', 'Kweilin']);
  });

  it('December, month 64: the 3rd and 13th go on to Tushan and the 22nd to Lungchow; neither of the West River\'s two goes to Tushan', () => {
    const g = war(64, { done: ROWS.slice(0, 5).map((r) => r[1]) });
    japans(g, 'Wuchow', 'Kweihsien', 'Kweilin', 'Liuchow', 'Nanning');
    const c104 = stand(g, C104, 'Liuchow'), a3 = stand(g, A3, 'Liuchow'), d22 = stand(g, D22, 'Nanning');
    open(g, 'Tushan', 'Lungchow');
    listen();
    bookAttacks(g, 'JAP');
    expect([g.byName.Tushan.ctrl, g.byName.Lungchow.ctrl]).toEqual(['JAP', 'JAP']);
    expect([where(g, a3), where(g, d22), where(g, c104)]).toEqual(['Tushan', 'Lungchow', 'Liuchow']);
  });

  // (Re-pointed by the design director's 11:41Z of 22 November 2026: the row ends in 64. Its purpose is kept: Tushan
  // is not attacked in January. Before, the row stayed owed and the month list refused it every month to 72.)
  it('Tushan not taken in its month is over: the record gave it up within days, and the row is not asked in January', () => {
    const g = war(65, { done: ROWS.filter((r) => r[1] !== 'Tushan').map((r) => r[1]) });
    japans(g, 'Liuchow');
    const a3 = stand(g, A3, 'Liuchow');
    open(g, 'Tushan');
    listen();
    bookAttacks(g, 'JAP');
    expect(told.map((t) => [t.name, t.what, t.why])).toEqual([['Tushan', 'over', 'the record\'s attack was called off']]);
    expect([g.byName.Tushan.ctrl, where(g, a3), made(g, 'Tushan')]).toEqual(['CHN', 'Liuchow', true]);
    // (And nothing is told of it the month after.)
    g.turn = 66;
    listen();
    bookAttacks(g, 'JAP');
    expect(told.filter((t) => t.name === 'Tushan')).toEqual([]);
    expect(where(g, a3)).toBe('Liuchow');
  });
});

describe('January and February 1945', () => {
  it('month 65: the 104th is called east of Canton to Waichow, from wherever its staff has left it, and not a month sooner', () => {
    const g = war(64, { done: ROWS.slice(0, 7).map((r) => r[1]) });
    japans(g, 'Wuchow', 'Kweihsien', 'Liuchow');
    const c104 = stand(g, C104, 'Liuchow');
    open(g, 'Waichow');
    listen();
    bookAttacks(g, 'JAP');
    expect([told, where(g, c104), g.byName.Waichow.ctrl]).toEqual([[], 'Liuchow', 'CHN']);
    g.turn = 65;
    bookAttacks(g, 'JAP');
    expect([g.byName.Waichow.ctrl, where(g, c104)]).toEqual(['JAP', 'Waichow']);
    expect(whats(ROWS[7][1])).toEqual(['due', 'made']);
    // (The book put it down at Canton's door: told to whoever reads the month's-march report.)
    expect(moved).toEqual([[C104, 'Liuchow', 'Canton']]);
  });

  it('Kukong and Kanchow wait for the choice of 1945: with it gone the 40th and the brigades take Kukong, and the 27th Kanchow the month after', () => {
    const done = ROWS.slice(0, 8).map((r) => r[1]);
    const set = (g: Game): Corps[] => { japans(g, 'Lingling', 'Chuanhsien'); open(g, 'Kukong'); return [stand(g, A40, 'Lingling'), stand(g, IMB, 'Canton')]; };
    // The choice of 1944 alone does not ask them.
    const early = war(65, { done });
    set(early);
    listen();
    bookAttacks(early, 'JAP');
    expect([told, early.byName.Kukong.ctrl]).toEqual([[], 'CHN']);
    // With the choice of 1945 gone.
    const g = war(65, { done, k45: true });
    const [a40, imb] = set(g);
    listen();
    bookAttacks(g, 'JAP');
    expect([g.byName.Kukong.ctrl, where(g, a40), where(g, imb)]).toEqual(['JAP', 'Kukong', 'Kukong']);
    expect(whats('Kukong')).toEqual(['due', 'made']);
    expect(whats('Kanchow')).toEqual([]);
    // February: the 27th is put down at Kukong and goes on to Kanchow.
    g.turn = 66;
    const a27 = stand(g, A27, 'Lingling');
    open(g, 'Kanchow');
    listen();
    bookAttacks(g, 'JAP');
    expect([g.byName.Kanchow.ctrl, where(g, a27)]).toEqual(['JAP', 'Kanchow']);
    expect(moved).toEqual([[A27, 'Lingling', 'Kukong']]);
  });
});

describe('where the book is not kept', () => {
  it('at stops 3 to 5 none of the ten is asked: the staff chooses as it did', () => {
    for (const stop of [3, 5]) {
      const g = war(63, { stop, k45: true });
      japans(g, 'Wuchow', 'Kweihsien', 'Chuanhsien', 'Kweilin');
      const c104 = stand(g, C104, 'Kweihsien'), d22 = stand(g, D22, 'Kweihsien');
      open(g, 'Liuchow', 'Nanning');
      listen();
      bookAttacks(g, 'JAP');
      expect([told, where(g, c104), where(g, d22), g.byName.Nanning.ctrl], `stop ${stop}`).toEqual([[], 'Kweihsien', 'Kweihsien', 'CHN']);
    }
  });
});
