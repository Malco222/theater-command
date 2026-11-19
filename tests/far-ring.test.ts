// Japan's second ring as the book's dated landings (rider b599 (i) to ruling 234; src/campaign/stations.ts, the
// historian's rows in historian/japan-second-ring-b599.md).
//
// At a stop that keeps the book, computer against computer: Wake's try of 11 December 1941 and its taking of the
// 23rd, the ring of January to March 1942 and the Solomons in May and July, each from its own port with the record's
// force and met figures, its cover as dated fleet moves. Within a month the landings go in the record's date order;
// a force that made one earlier in the month sails again on its next, from where it stands; a row whose port has not
// fallen waits a month. Nothing about a result is written. With the far side's switch off none of it exists.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { addCorps, capitulate, createGame, declareWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import { bookAttacks, bookWatch, stations, type Attack } from '../src/campaign/stations';
import farWorld from '../data/campaign/far-world.json';
import type { Corps, Game } from '../src/campaign/types';

const MAIZURU = 'Naval landing force (Maizuru 2nd, for Wake)';
const SASEBO = 'Naval landing forces (Sasebo 1st and 2nd, Yokosuka 1st)';
const SAKAGUCHI = 'Sakaguchi Detachment (56th Mixed Infantry Group)';
const ITO = 'Ito Detachment (228th Infantry and the 1st Kure landing force)';
const R229 = '229th Infantry (of the 38th Division)';
const TULAGI = 'Naval landing force (Kure 3rd, for Tulagi)';
const AIRFIELD = 'Naval landing force (the Guadalcanal airfield party)';

const attacks = (): Attack[] => stations()!.attacks!;
const attack = (name: string): Attack => attacks().find((a) => a.name === name)!;
const index = (name: string): number => attacks().findIndex((a) => a.name === name);

/** A book war at stop 1 in month `turn`, Japan's blow fallen in December 1941 and the Indies in Britain's keeping. */
const war = (turn: number): Game => {
  const g = startOpen(createGame(3, null));
  setStop(g, 1, true);
  declareWar(g, 'GER', 'NED');
  capitulate(g, 'NED', 'GER');
  g.turn = turn;
  g.flags['st:done:JAP:south'] = 28;
  for (const e of ['USA', 'UK']) declareWar(g, 'JAP', e);
  return g;
};
/** Every dated attack but these is made already: the test reads these alone. */
const only = (g: Game, ...names: string[]): void => {
  attacks().forEach((a, i) => { if (!names.includes(a.name)) g.flags[`ba:${i}`] = 1; });
};
/** A place in Japan's hands, with nobody of the other side in it. */
const japans = (g: Game, name: string): void => {
  const r = g.byName[name];
  r.ctrl = 'JAP';
  r.units = r.units.filter((u) => u.nat === 'JAP');
};
/** A place with nobody in its field and no town guard: a landing there walks in. */
const open = (g: Game, name: string): void => {
  const r = g.byName[name];
  r.units = r.units.filter((u) => u.nat === 'JAP');
  g.flags[`guard:${r.id}`] = 100;
};
const put = (g: Game, name: string, place: string, str: number): Corps => {
  const c = addCorps(g, 'JAP', name, 'inf', 1, place, { xp: 0.1 });
  c.str = str;
  return c;
};

type Told = { name: string; what: string; why?: string; taken?: boolean };
const told: Told[] = [];
const listen = (): void => { told.length = 0; bookWatch.tell = (e) => { told.push({ name: e.row.name, what: e.what, why: e.why, taken: e.taken }); }; };
afterEach(() => { bookWatch.tell = null; });
const made = (name: string): Told[] => told.filter((t) => t.name === name && t.what === 'made');

describe("Japan's second ring by the book (rider b599 (i) to ruling 234)", () => {
  it("lays the historian's rows: each landing's month, force, port and met figures, and the table runs to July 1942", () => {
    const rows = attacks().filter((a) => a.sea && !a.name.startsWith('Java'));
    expect(rows.map((a) => [a.month, a.name, a.at, a.force, a.from, a.met ?? null])).toEqual([
      [28, 'Wake: the first try', ['Wake'], [MAIZURU], ['Kwajalein'], [0.03, 0.035]],
      [28, 'Wake: the taking', ['Wake'], [MAIZURU], ['Kwajalein'], [0.1, 0.035]],
      [29, 'Menado', ['Menado'], [SASEBO], ['Davao'], [0.25, 0.1]],
      [29, 'Tarakan', ['Tarakan'], [SAKAGUCHI], ['Davao'], [0.3, 0.09]],
      [29, 'Rabaul', ['Rabaul'], ['South Seas Detachment'], ['Guam', 'Truk'], [0.35, 0.1]],
      [29, 'Kendari', ['Kendari'], [SASEBO], ['Menado'], [0.2, 0.03]],
      [29, 'Balikpapan', ['Balikpapan'], [SAKAGUCHI], ['Tarakan'], [0.45, 0.07]],
      [29, 'Ambon', ['Ambon'], [ITO], ['Davao'], [0.35, 0.26]],
      [30, 'Palembang', ['Palembang'], [R229], ['Cam Ranh'], [0.25, 0.15]],
      [31, 'Lae and Salamaua', ['Lae'], ['South Seas Detachment'], ['Rabaul'], [0.2, 0.01]],
      [31, 'Medan', ['Medan'], ['25th Army (Imperial Guards Division)'], ['Singapore'], [1.2, 0.15]],
      // (Tulagi is the chief place of the game's Guadalcanal: the May landing and July's airfield party go to the same province.)
      [33, 'Tulagi', ['Guadalcanal'], [TULAGI], ['Rabaul'], null],
      [35, 'Guadalcanal: the airfield party', ['Guadalcanal'], [AIRFIELD], ['Rabaul', 'Truk'], null],
    ]);
    // Wake's try is the record's of 11 December alone; the taking is asked while the table runs.
    expect([attack('Wake: the first try').until, attack('Wake: the taking').until]).toEqual([28, undefined]);
    expect(stations()!.until).toBe(35);
    // (A saved game counts the rows by their place in the list: the ring's rows follow the rows that were there.)
    expect(index('Wake: the first try')).toBe(index('The pursuit to the Chindwin') + 1);
  });

  it("gives each landing its cover as dated fleet moves, and a fleet's months in order so it is home before its next", () => {
    const F = stations()!.fleets!;
    expect(F.filter((f) => (f.nat ?? 'JAP') === 'JAP').map((f) => [f.month, f.fleet, f.sea])).toEqual([
      [28, '4th Fleet', 'wake'], [28, 'Mobile Fleet', 'wake'],
      [29, 'Mobile Fleet', 'bismarck'], [29, '4th Fleet', 'bismarck'], [29, '3rd Fleet', 'molucca'], [29, '2nd Fleet', 'makassar'],
      [30, '3rd Fleet', 'java'], [30, 'Southern Expeditionary Fleet', 'java'],
      [31, '4th Fleet', 'solomon'], [33, '4th Fleet', 'eastsol'], [35, '4th Fleet', 'eastsol'],
    ]);
    for (let i = 1; i < F.length; i++) expect(F[i].month).toBeGreaterThanOrEqual(F[i - 1].month);
  });

  it("brings the ring's forces the world had not, at the stops that keep the book, held for their landings", () => {
    const rows = (farWorld as any)['setup-1939'].expeditions.filter((r: any[]) => [ITO, R229, TULAGI, AIRFIELD].includes(r[2]));
    expect(rows.map((r: any[]) => [r[0], r[2], r[5], r[6].str, r[6].est, r[6].hold, r[6].stops])).toEqual([
      [29, ITO, 'Canton', 0.35, 0.35, 1, [1, 2]],
      [30, R229, 'Cam Ranh', 0.25, 0.25, 1, [1, 2]],
      [33, TULAGI, 'Truk', 0.03, 0.03, 1, [1, 2]],
      [35, AIRFIELD, 'Truk', 0.03, 0.03, 1, [1, 2]],
    ]);
  });

  it("a force that took its port this month sails on from it: Menado and then Kendari in January", () => {
    const g = war(29);
    only(g, 'Menado', 'Kendari');
    japans(g, 'Davao');
    open(g, 'Menado');
    open(g, 'Kendari');
    const s = put(g, SASEBO, 'Ulithi', 0.2);
    listen();
    bookAttacks(g, 'JAP');
    expect(made('Menado').map((t) => t.taken)).toEqual([true]);
    expect(made('Kendari').map((t) => t.taken)).toEqual([true]);
    expect([g.byName.Menado.ctrl, g.byName.Kendari.ctrl, g.regions[s.reg].name]).toEqual(['JAP', 'JAP', 'Kendari']);
    expect([g.flags[`ba:${index('Menado')}`], g.flags[`ba:${index('Kendari')}`]]).toEqual([29, 29]);
    // It has had its month: no third sailing.
    expect(s.ap).toBe(0);
  });

  it("a row whose port has not fallen waits for it: Kendari is owed while Menado is not Japan's", () => {
    const g = war(29);
    only(g, 'Kendari');
    open(g, 'Kendari');
    const s = put(g, SASEBO, 'Ulithi', 0.2);
    listen();
    bookAttacks(g, 'JAP');
    expect(made('Kendari')).toEqual([]);
    expect(told.filter((t) => t.name === 'Kendari').map((t) => [t.what, t.why])).toEqual([['owed', "none of its ports is its side's"]]);
    expect(g.regions[s.reg].name).toBe('Ulithi');
    // Taken the next month, the row sails from it.
    japans(g, 'Menado');
    g.turn = 30;
    listen();
    bookAttacks(g, 'JAP');
    expect(made('Kendari').map((t) => t.taken)).toEqual([true]);
  });

  it("an owed row of an earlier month goes before the month's own, and a force that landed elsewhere this month is not taken from there", () => {
    // March 1942: Tarakan, owed since January, goes first; Kragan, the same detachment's March landing, waits for it.
    const g = war(31);
    only(g, 'Tarakan', 'Java: Kragan');
    japans(g, 'Davao');
    japans(g, 'Jolo');
    open(g, 'Tarakan');
    const k = put(g, SAKAGUCHI, 'Davao', 0.25);
    listen();
    bookAttacks(g, 'JAP');
    expect(made('Tarakan').map((t) => t.taken)).toEqual([true]);
    expect(g.regions[k.reg].name).toBe('Tarakan');
    expect(made('Java: Kragan')).toEqual([]);
    expect(told.filter((t) => t.name === 'Java: Kragan').map((t) => t.why)).toEqual(['a corps of it has made its landing elsewhere this month']);
  });

  it("Wake: the try thrown back, the taking sails the same month from Kwajalein; the try taking Wake, the taking lapses", () => {
    // The try fails: the detachment is back at Kwajalein and goes in again on the 23rd.
    const g = war(28);
    only(g, 'Wake: the first try', 'Wake: the taking');
    const m = put(g, MAIZURU, 'Kwajalein', 0.004);
    addCorps(g, 'USA', 'Wake detachment (a test row)', 'inf', 1, 'Wake', { xp: 0.2 }).str = 0.39;
    listen();
    bookAttacks(g, 'JAP');
    expect(made('Wake: the first try').map((t) => t.taken)).toEqual([false]);
    expect(made('Wake: the taking').length).toBe(1);
    if (!m.dead) expect(g.regions[m.reg].name).toBe(made('Wake: the taking')[0].taken ? 'Wake' : 'Kwajalein');
    // Asked again in January the try is over; the taking, lost, is owed.
    g.turn = 29;
    listen();
    bookAttacks(g, 'JAP');
    expect(told.find((t) => t.name === 'Wake: the first try')).toMatchObject({ what: 'over', why: "the record's attack was called off" });
    // The try takes Wake: the taking is over the same month.
    const h = war(28);
    only(h, 'Wake: the first try', 'Wake: the taking');
    open(h, 'Wake');
    put(h, MAIZURU, 'Kwajalein', 0.2);
    listen();
    bookAttacks(h, 'JAP');
    expect(made('Wake: the first try').map((t) => t.taken)).toEqual([true]);
    expect(told.filter((t) => t.name === 'Wake: the taking').map((t) => [t.what, t.why])).toEqual([['over', "the place is its side's"]]);
  });
});
