// The Kawaguchi Detachment's split and the 2/15th Punjab's march, by the book (the design director's rulings of
// 07:16Z and 07:40Z, 22 November 2026, on the historian's second-division-and-british-borneo-1941-42.md, section 4;
// data/campaign/far-shores.json and far-pacific-war.json). It replaces "one row, one corps" (tests/far-borneo-book.test.ts).
//
// The record: the detachment took Miri, Brunei and Kuching whole in December 1941, and from January 1942 it was four
// commands. At the book's stops it is four rows from month 29, proportions of the row's ruled size (0.325 for the
// historian's 0.3):
//   the western force       0.14, from Kuching: Singkawang and Pontianak in January (overland and by sea are one
//                           row at the chart's grain);
//   the Kuching guard       0.055, stays at Kuching;
//   the North Borneo battalion  0.075, from Miri: Jesselton and Sandakan in January;
//   the Miri and Seria party    0.0325, stays at Miri.
// In April (32) the detachment leaves Borneo for Cebu, marked unsure as the historian marks it.
// The 2/15th Punjab is a row of the book's stations: Kuching in December, Singkawang's ground (the record's Sanggau
// Ledo) in December and January at about 0.05, the interior (Sintang) in February, Sampit in March, and gone with
// the Dutch capitulation in April. Beaten at Kuching by the book's fight it goes by its own next step, early.
// At the stops that keep no book the detachment is one row and the Punjabis are their staff's, as before.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { addCorps, capitulate, computeSupply, createGame, declareWar, relocate, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import { bookAttacks, bookStations, bookWatch, stations, type Attack } from '../src/campaign/stations';
import setup from '../data/campaign/setup-1939.json';
import type { Corps, Game } from '../src/campaign/types';

const KAWAGUCHI = 'Kawaguchi Detachment (35th Brigade, of the 18th Division)';
const WESTERN = 'Kawaguchi Detachment (the western force, for Pontianak)';
const GUARD = 'Kawaguchi Detachment (the Kuching guard)';
const BATTALION = 'Kawaguchi Detachment (the North Borneo battalion)';
const PARTY = 'Kawaguchi Detachment (the Miri and Seria party)';
const PARTS = [WESTERN, GUARD, BATTALION, PARTY];
const PUNJAB = '2/15th Punjab (Kuching)';

const attacks = (): Attack[] => stations()!.attacks!;
const attack = (name: string): Attack => attacks().find((a) => a.name === name)!;
const index = (name: string): number => attacks().findIndex((a) => a.name === name);
const dated = (name: string): any[] => ((setup as any).expeditions as any[]).find((r) => r[2] === name)!;

/** A book war at stop `stop` in month `turn`, Japan's blow fallen in December 1941 and the Indies in Britain's keeping. */
const war = (turn: number, stop = 1): Game => {
  const g = startOpen(createGame(3, null));
  if (stop !== 3) setStop(g, stop, true);
  declareWar(g, 'GER', 'NED');
  capitulate(g, 'NED', 'GER');
  g.turn = turn;
  g.flags['st:done:JAP:south'] = 28;
  for (const e of ['USA', 'UK']) declareWar(g, 'JAP', e);
  return g;
};
/** Every dated attack but these is made already: the test reads these alone. */
const only = (g: Game, ...names: string[]): void => { attacks().forEach((a, i) => { if (!names.includes(a.name)) g.flags[`ba:${i}`] = 1; }); };
/** A place in Japan's hands, with nobody of the other side in it. */
const japans = (g: Game, name: string): void => { const r = g.byName[name]; r.ctrl = 'JAP'; r.units = r.units.filter((u) => u.nat === 'JAP'); };
/** A place with nobody in its field and no town guard: a force that comes there walks in. */
const open = (g: Game, name: string): void => { const r = g.byName[name]; r.units = r.units.filter((u) => u.nat === 'JAP'); g.flags[`guard:${r.id}`] = 100; };
const put = (g: Game, nat: string, name: string, place: string, str: number, est?: number): Corps => {
  const c = addCorps(g, nat, name, 'inf', 1, place, est === undefined ? { xp: 0.1 } : { xp: 0.1, est });
  c.str = str;
  return c;
};
const find = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'JAP' && !c.dead && c.name === name);
const where = (g: Game, c: Corps): string => g.regions[c.reg].name;
const worth = (c: Corps): number => Math.round(c.divs * c.str * 10000) / 10000;

type Told = { name: string; what: string; why?: string; taken?: boolean; fought: boolean };
const told: Told[] = [];
const listen = (): void => { told.length = 0; bookWatch.tell = (e) => { told.push({ name: e.row.name, what: e.what, why: e.why, taken: e.taken, fought: !!e.report }); }; };
afterEach(() => { bookWatch.tell = null; });
const made = (name: string): Told[] => told.filter((t) => t.name === name && t.what === 'made');

/** December's work done by the book: the detachment at Kuching, and Miri, Brunei and Kuching in Japan's hands. */
const january = (stop = 1): Game => {
  const g = war(27, stop);
  // (Where no book dates the rows, Cam Ranh is Japan's, as the demand on Vichy France makes it in a played war:
  // since the design director's ruling B7 of 25 November 2026 a row posted on a neutral's ground waits for its war
  // there, and the detachment would not be on its quay in month 27. By the book it comes on its date as it did.)
  if (stop === 3) japans(g, 'Cam Ranh');
  startPhase(g, 'JAP');
  const k = find(g, KAWAGUCHI)!;
  for (const nm of ['Miri', 'Brunei', 'Kuching']) japans(g, nm);
  relocate(g, k, g.byName.Kuching);
  k.str = 0.31;
  for (const nm of ['Miri and Seria', 'Brunei', 'Kuching']) g.flags[`ba:${index(nm)}`] = 28;
  g.turn = 29;
  computeSupply(g);
  startPhase(g, 'JAP');
  return g;
};

describe("the Kawaguchi Detachment's split (the director's rulings of 07:16Z and 07:40Z, 22 November 2026)", () => {
  it("lays the four rows of January 1942 as proportions of the row's ruled size, at the book's stops alone", () => {
    expect(dated(KAWAGUCHI)[6].str).toBe(0.325);
    const rows = PARTS.map(dated);
    expect(rows.map((r) => [r[0], r[1], r[5], r[6].str, r[6].est, !!r[6].hold, !!r[6].stay, r[6].stops])).toEqual([
      [29, 'JAP', 'Kuching', 0.14, 0.14, false, false, [1, 2]],
      [29, 'JAP', 'Kuching', 0.055, 0.055, true, true, [1, 2]],
      [29, 'JAP', 'Miri', 0.075, 0.075, false, false, [1, 2]],
      [29, 'JAP', 'Miri', 0.0325, 0.0325, true, true, [1, 2]],
    ]);
    // The western force comes of the detachment's row, and the other three only with it: nothing is counted twice.
    expect([rows[0][6].from, rows[0][6].cut ?? null]).toEqual([[KAWAGUCHI], null]);
    expect(rows.slice(1).map((r) => [r[6].from ?? null, r[6].cut])).toEqual([[null, WESTERN], [null, WESTERN], [null, WESTERN]]);
    // The historian's shares (0.13, 0.05, 0.07, 0.03 of 0.3) at the row's 0.325: what the record left of the main
    // body's 0.2 after Kuching was 0.18.
    const sum = rows.reduce((s, r) => s + r[6].str, 0);
    expect(Math.round(sum * 10000) / 10000).toBe(0.3025);
    for (const [r, share] of [[rows[0], 0.13], [rows[1], 0.05], [rows[2], 0.07], [rows[3], 0.03]] as [any[], number][]) expect(Math.abs(r[6].str - (share * 0.325) / 0.3)).toBeLessThan(0.003);
  });

  it("lays the dated attacks after Borneo's: Singkawang and Pontianak in January, Cebu in April, and each landing names its part", () => {
    expect(['Singkawang', 'Pontianak', 'Cebu'].map(index)).toEqual([38, 39, 40]);
    expect(['Singkawang', 'Pontianak', 'Cebu'].map(attack).map((a) => [a.month, !!a.sea, a.at, a.force, a.from, a.met ?? null])).toEqual([
      // (The record's fight for Singkawang II airfield: the western force against the Punjabis and the KNIL's detachment.)
      [29, false, ['Singkawang'], [WESTERN], ['Kuching'], [0.13, 0.1]],
      [29, false, ['Pontianak'], [WESTERN], ['Singkawang', 'Kuching'], null],
      [32, true, ['Cebu'], PARTS, ['Jesselton', 'Sandakan', 'Brunei', 'Kuching'], null],
    ]);
    expect([attack('Kuching').force, attack('Jesselton').force, attack('Sandakan').force]).toEqual([[KAWAGUCHI, WESTERN], [KAWAGUCHI, BATTALION], [KAWAGUCHI, BATTALION]]);
    const g = war(29);
    for (const nm of ['Singkawang', 'Pontianak', 'Cebu']) for (const place of [...attack(nm).at, ...(attack(nm).from ?? [])]) expect(g.byName[place], place).toBeDefined();
  });

  it('January 1942 at a stop of the book: the one row is four, each where the record had it', () => {
    const g = january();
    expect(find(g, KAWAGUCHI)).toBeUndefined();
    expect(PARTS.map((nm) => { const c = find(g, nm)!; return [where(g, c), worth(c), c.full, c.hold, !!c.stay]; })).toEqual([
      ['Kuching', 0.14, 0.14, false, false],
      ['Kuching', 0.055, 0.055, true, true],
      ['Miri', 0.075, 0.075, false, false],
      ['Miri', 0.0325, 0.0325, true, true],
    ]);
  });

  it('at the third stop the detachment stays one row', () => {
    const g = january(3);
    expect(find(g, KAWAGUCHI)).toBeDefined();
    expect(PARTS.map((nm) => find(g, nm))).toEqual([undefined, undefined, undefined, undefined]);
  });

  it('January 1942: the battalion makes Jesselton and Sandakan, the western force Singkawang and Pontianak', () => {
    const g = january();
    only(g, 'Jesselton', 'Sandakan', 'Singkawang', 'Pontianak');
    for (const nm of ['Jesselton', 'Sandakan', 'Singkawang', 'Pontianak']) open(g, nm);
    listen();
    bookAttacks(g, 'JAP');
    expect(['Jesselton', 'Sandakan', 'Singkawang', 'Pontianak'].map((nm) => made(nm).map((t) => [t.taken, t.fought]))).toEqual([[[true, false]], [[true, false]], [[true, false]], [[true, false]]]);
    expect(['Jesselton', 'Sandakan', 'Singkawang', 'Pontianak'].map((nm) => g.byName[nm].ctrl)).toEqual(['JAP', 'JAP', 'JAP', 'JAP']);
    expect([where(g, find(g, BATTALION)!), where(g, find(g, WESTERN)!)]).toEqual(['Sandakan', 'Pontianak']);
    // The two that stay have not moved, and are held where they stand.
    expect([GUARD, PARTY].map((nm) => { const c = find(g, nm)!; return [where(g, c), c.hold]; })).toEqual([['Kuching', true], ['Miri', true]]);
  });

  it("Singkawang's fight is the game's own, at the record's figures, against the line that stands there", () => {
    const g = january();
    only(g, 'Singkawang');
    g.byName.Singkawang.units = [];
    put(g, 'UK', PUNJAB, 'Singkawang', 0.05, 0.1);
    put(g, 'UK', 'the KNIL\'s West Borneo detachment', 'Singkawang', 0.05, 0.05);
    const w = find(g, WESTERN)!;
    listen();
    bookAttacks(g, 'JAP');
    expect(made('Singkawang').map((t) => t.fought)).toEqual([true]);
    // What it cost fell on the part that fought: 0.13 of the force's 0.14.
    expect(w.str).toBeLessThan(0.14);
    expect(w.str).toBeGreaterThan(0.14 - 0.13);
    // Nothing about its result is written: won, the place is Japan's and the force in it; lost, it is owed.
    const won = made('Singkawang')[0].taken;
    expect([g.byName.Singkawang.ctrl === 'JAP', where(g, w) === 'Singkawang', g.flags[`ba:${index('Singkawang')}`] !== undefined]).toEqual([won, won, won]);
  });

  it('April 1942: the four parts leave Borneo together for Cebu', () => {
    const g = january();
    only(g, 'Cebu');
    for (const nm of ['Jesselton', 'Sandakan']) japans(g, nm);
    g.turn = 32;
    open(g, 'Cebu');
    computeSupply(g);
    listen();
    bookAttacks(g, 'JAP');
    expect(made('Cebu').map((t) => t.taken)).toEqual([true]);
    expect(g.byName.Cebu.ctrl).toBe('JAP');
    expect(PARTS.map((nm) => where(g, find(g, nm)!))).toEqual(['Cebu', 'Cebu', 'Cebu', 'Cebu']);
    // Not before its month.
    const early = january();
    only(early, 'Cebu');
    open(early, 'Cebu');
    listen();
    bookAttacks(early, 'JAP');
    expect(told.filter((t) => t.name === 'Cebu')).toEqual([]);
  });
});

describe("the 2/15th Punjab's march (the director's rulings of 07:16Z and 07:40Z, 22 November 2026)", () => {
  const row = () => stations()!.rows.find((r) => r.nat === 'UK' && r.corps === PUNJAB)!;

  it("lays its row of the stations: Kuching, Singkawang's ground, the interior, Sampit, and gone with the Dutch capitulation", () => {
    expect([row().steps, row().worth, row().free ?? false, row().until ?? null]).toEqual([
      [[28, 'Kuching', 0], [29, 'Singkawang', 1], [30, 'Sintang', 0], [31, 'Sampit', 0]],
      [[29, 0.05], [32, 0]],
      false, null,
    ]);
    const g = war(28);
    for (const s of row().steps) expect(g.byName[s[1] as string], s[1] as string).toBeDefined();
    // Singkawang and Pontianak touch Kuching; Sintang touches Kuching and Pontianak; Sampit touches Sintang.
    const nb = (a: string, b: string): boolean => g.byName[a].nb.includes(g.byName[b].id);
    expect([nb('Kuching', 'Singkawang'), nb('Kuching', 'Sintang'), nb('Sintang', 'Sampit')]).toEqual([true, true, true]);
  });

  it('month by month the book stands it on its road, at the record\'s worth, and it is gone in April 1942', () => {
    const g = war(28);
    const p = put(g, 'UK', PUNJAB, 'Kuching', 0.1, 0.1);
    const seen: [number, string, number, boolean, number | undefined][] = [];
    for (const t of [28, 29, 30, 31]) { g.turn = t; bookStations(g, 'UK'); seen.push([t, where(g, p), worth(p), p.hold && !!p.stay, p.bookWorks]); }
    expect(seen).toEqual([
      [28, 'Kuching', 0.1, true, 0],
      [29, 'Singkawang', 0.05, true, 1],
      [30, 'Sintang', 0.05, true, 0],
      [31, 'Sampit', 0.05, true, 0],
    ]);
    g.turn = 32;
    bookStations(g, 'UK');
    expect([p.dead, g.corps.includes(p)]).toEqual([true, false]);
  });

  it('beaten at Kuching by the book\'s fight, it goes by its own next step the same month: to Singkawang\'s ground, half-dug', () => {
    const g = war(28);
    only(g, 'Kuching');
    for (const nm of ['Cam Ranh', 'Miri', 'Brunei']) japans(g, nm);
    for (const nm of ['Miri and Seria', 'Brunei']) g.flags[`ba:${index(nm)}`] = 28;
    put(g, 'JAP', KAWAGUCHI, 'Miri', 0.325);
    g.byName.Kuching.units = [];
    const p = put(g, 'UK', PUNJAB, 'Kuching', 0.1, 0.1);
    bookStations(g, 'UK');
    g.supplyDirty = true;
    computeSupply(g);
    listen();
    bookAttacks(g, 'JAP');
    expect(made('Kuching').map((t) => [t.taken, t.fought])).toEqual([[true, true]]);
    expect(g.byName.Kuching.ctrl).toBe('JAP');
    expect([p.dead, where(g, p), p.hold, p.bookWorks]).toEqual([false, 'Singkawang', true, 1]);
    expect(p.str).toBeLessThan(0.1);
  });

  it('at the third stop it is its staff\'s, as before', () => {
    const g = war(29, 3);
    const p = put(g, 'UK', PUNJAB, 'Kuching', 0.1, 0.1);
    bookStations(g, 'UK');
    expect([where(g, p), p.hold, p.bookWorks]).toEqual(['Kuching', false, undefined]);
  });
});
