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
import { addCorps, capitulate, computeSupply, corpsBase, createGame, declareWar, withWorksAside, worksKept } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { closedTo, liftLeft, onOwnShipping, useLift, whyNotLand } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { bookAttacks, bookSails, bookWatch, stations, type Attack } from '../src/campaign/stations';
import farWorld from '../data/campaign/far-world.json';
import farShores from '../data/campaign/far-shores.json';
import type { BattleReport, Corps, Game } from '../src/campaign/types';

const MAIZURU = 'Naval landing force (Maizuru 2nd, for Wake)';
const SASEBO = 'Naval landing forces (Sasebo 1st and 2nd, Yokosuka 1st)';
const SAKAGUCHI = 'Sakaguchi Detachment (56th Mixed Infantry Group)';
const ITO = 'Ito Detachment (228th Infantry and the 1st Kure landing force)';
const R229 = '229th Infantry (of the 38th Division)';
const TULAGI = 'Naval landing force (Kure 3rd, for Tulagi)';
const AIRFIELD = 'Naval landing force (the Guadalcanal airfield party)';

const attacks = (): Attack[] => stations()!.attacks!;
// (British Borneo's landings are the book's too, laid after the ring's: the design director's rulings of 03:59Z and
// 04:06Z, 22 November 2026, and with them the Kawaguchi Detachment's landing on Cebu, his of 07:16Z. They are read
// in tests/far-borneo-book.test.ts and tests/far-borneo-split.test.ts and are no rows of the ring.)
const BORNEO = ['Miri and Seria', 'Brunei', 'Kuching', 'Jesselton', 'Sandakan', 'Cebu'];
// (Japan's rows: the table is one, and a landing that names another country's choice is that country's, far-africa.json.
// As first written every landing of the table was Japan's; Britain's two in East Africa joined it on 22 November 2026.)
const ring = (): Attack[] => attacks().filter((a) => a.sea && (a.book === undefined || a.book.startsWith('JAP:')) && !a.name.startsWith('Java') && !BORNEO.includes(a.name));
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

type Told = { name: string; what: string; why?: string; taken?: boolean; report?: BattleReport };
const told: Told[] = [];
const listen = (): void => { told.length = 0; bookWatch.tell = (e) => { told.push({ name: e.row.name, what: e.what, why: e.why, taken: e.taken, report: e.report }); }; };
afterEach(() => { bookWatch.tell = null; });
const made = (name: string): Told[] => told.filter((t) => t.name === name && t.what === 'made');

describe("Japan's second ring by the book (rider b599 (i) to ruling 234)", () => {
  it("lays the historian's rows: each landing's month, force, port and met figures, and the table runs to July 1942", () => {
    const rows = ring();
    expect(rows.map((a) => [a.month, a.name, a.at, a.force, a.from, a.met ?? null])).toEqual([
      [28, 'Wake: the first try', ['Wake'], [MAIZURU], ['Kwajalein'], [0.03, 0.035]],
      [28, 'Wake: the taking', ['Wake'], [MAIZURU], ['Kwajalein'], [0.1, 0.035]],
      [29, 'Menado', ['Menado'], [SASEBO], ['Davao'], [0.25, 0.1]],
      [29, 'Tarakan', ['Tarakan'], [SAKAGUCHI], ['Davao'], [0.3, 0.09]],
      [29, 'Rabaul', ['Rabaul'], ['South Seas Detachment'], ['Guam', 'Truk'], [0.35, 0.1]],
      [29, 'Kendari', ['Kendari'], [SASEBO], ['Menado'], [0.2, 0.03]],
      [29, 'Balikpapan', ['Balikpapan'], [SAKAGUCHI], ['Tarakan'], [0.45, 0.07]],
      [29, 'Ambon', ['Ambon'], [ITO], ['Davao'], [0.35, 0.26]],
      // (Palembang's paratroops sail in the regiment's lift at the book's stops too: the director's 16:18Z of 21 November 2026.)
      [30, 'Palembang', ['Palembang'], [R229, '2nd Raiding Regiment (paratroops of the 1st Raiding Group)'], ['Cam Ranh'], [0.25, 0.15]],
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
      // (Wake's cover is the 4th Fleet alone: the game holds the carrier striking force as one fleet, most of it went
      // home after Pearl Harbor, and so the whole row goes home; a part of it for Wake is no row of the game's.)
      [28, '4th Fleet', 'wake'],
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

describe("The record's own shipping, armies and works (riders b626 and b628 to ruling 234)", () => {
  const labels = (name: string): string[] => made(name)[0].report!.lines.map((l) => l.label);
  /** A defending row dug in with finished works. */
  const dug = (c: Corps): Corps => { c.dug = true; c.dugFor = 3; return c; };
  const row = (g: Game, name: string): Corps => g.corps.find((c) => c.name === name && !c.dead)!;

  it("a dated landing sails on the record's own shipping: it draws nothing from the month's lift, and sails with the lift all used", () => {
    const g = war(29);
    only(g, 'Menado', 'Kendari');
    japans(g, 'Davao');
    open(g, 'Menado');
    open(g, 'Kendari');
    put(g, SASEBO, 'Ulithi', 0.2);
    // The staff's own landings have taken the month's six.
    useLift(g, 'JAP', g.nations.JAP.sealift);
    expect(liftLeft(g, 'JAP')).toBe(0);
    listen();
    bookAttacks(g, 'JAP');
    expect([made('Menado').length, made('Kendari').length]).toEqual([1, 1]);
    expect(g.flags[`lift:JAP:${g.turn}`]).toBe(g.nations.JAP.sealift);
    // And with the lift untouched it is left whole for the staff.
    const h = war(29);
    only(h, 'Menado');
    japans(h, 'Davao');
    open(h, 'Menado');
    put(h, SASEBO, 'Ulithi', 0.2);
    bookAttacks(h, 'JAP');
    expect(h.byName.Menado.ctrl).toBe('JAP');
    expect(liftLeft(h, 'JAP')).toBe(h.nations.JAP.sealift);
  });

  it("the landings of the book's great blow sail so too, in its month and the next; a staff's own landing still wants the lift", () => {
    const g = war(28);
    expect(bookSails(g, 'JAP')).toBe(true);
    expect(bookSails(g, 'USA')).toBe(false);
    g.turn = 29;
    expect(bookSails(g, 'JAP')).toBe(true);
    g.turn = 30;
    expect(bookSails(g, 'JAP')).toBe(false);
    // The shipping itself: refused for want of lift outside the book's own, not inside; and only for the country named.
    g.turn = 29;
    japans(g, 'Davao');
    const c = put(g, SASEBO, 'Davao', 0.2);
    useLift(g, 'JAP', g.nations.JAP.sealift);
    expect(whyNotLand(g, [c], g.byName.Menado)).toBe('All the shipping is in use this month.');
    expect(onOwnShipping('JAP', () => whyNotLand(g, [c], g.byName.Menado))).toBeNull();
    expect(onOwnShipping('USA', () => whyNotLand(g, [c], g.byName.Menado))).toBe('All the shipping is in use this month.');
    expect(whyNotLand(g, [c], g.byName.Menado)).toBe('All the shipping is in use this month.');
    // At a stop that does not keep the book nothing sails on the record's shipping.
    const h = startOpen(createGame(3, null));
    h.turn = 28;
    h.flags['st:done:JAP:south'] = 28;
    expect(bookSails(h, 'JAP')).toBe(false);
  });

  it("a row fights at the figure of the army that raised it: the KNIL's under Britain's flag, Australia's battalions at Australia's", () => {
    const g = war(28);
    const share = (c: Corps): number => {
      const as = corpsBase(g, c), name = c.name, raised = c.raised;
      c.name = 'a row of the country it answers to'; delete c.raised;
      const own = corpsBase(g, c);
      c.name = name; if (raised !== undefined) c.raised = raised;
      return as / own;
    };
    const knil = row(g, 'Palembang garrison (the oil)');
    expect([knil.nat, knil.raised]).toEqual(['UK', 'NED']);
    expect(share(knil)).toBeCloseTo(0.8 / g.nations.UK.quality, 6);
    // (The Indies' record of 21 November 2026, the director's 15:16Z: a Java division in the field 0.7, the Molukken
    // Brigade at Ambon 0.85; the regulars in their posts keep the army's 0.8.)
    for (const [nm, q] of [['KNIL 1st Division (West Java)', 0.7], ['Great East garrisons (Ambon, Celebes, Timor)', 0.85], ['Borneo garrisons (Tarakan and Balikpapan)', 0.8]] as [string, number][]) expect([nm, share(row(g, nm)).toFixed(6)]).toEqual([nm, (q / g.nations.UK.quality).toFixed(6)]);
    const laid = (name: string): Corps => addCorps(g, 'UK', name, 'inf', 1, 'Batavia', { xp: 0.1 });
    expect(share(laid('Outer Islands reinforcement (Tarakan, Menado, Koepang)'))).toBeCloseTo(0.8 / g.nations.UK.quality, 6);
    expect(share(laid("KNIL mobilisation (Java's militia and the Landstorm)"))).toBeCloseTo(0.5 / g.nations.UK.quality, 6);
    for (const nm of ['Gull Force (2/21st Battalion)', 'Sparrow Force (2/40th Battalion)', 'Blackforce (2/3rd Machine Gun and 2/2nd Pioneer Battalions)']) expect(share(laid(nm))).toBeCloseTo(g.nations.AUS.quality / g.nations.UK.quality, 6);
    // The historian's list for b628, whatever the flag: the Indian Army to the end of 1942, the Burma Army, the
    // Philippine Scouts above America's line and the Philippine Army's reserve divisions well under it.
    const us = (name: string): Corps => addCorps(g, 'USA', name, 'inf', 1, 'Manila', { xp: 0.1 });
    expect(share(laid('17th Indian Division'))).toBeCloseTo(0.95 / g.nations.UK.quality, 6);
    expect(share(laid('Burma Army (the Burma Rifles; the 1st Burma Division from July 1941)'))).toBeCloseTo(0.75 / g.nations.UK.quality, 6);
    // (Rows the game holds together take one figure, weighted by their men; China's armies in Burma stand under Britain's flag.)
    expect(share(laid('Malaya Command (Singapore garrison, the Malay Regiment and the Volunteers)'))).toBeCloseTo(0.85 / g.nations.UK.quality, 6);
    expect(share(laid('Chinese 5th Army (200th, 22nd and 96th Divisions)'))).toBeCloseTo(0.65 / g.nations.UK.quality, 6);
    expect(share(laid('Chinese 6th Army (49th, 55th and 93rd Divisions)'))).toBeCloseTo(g.nations.CHN.quality / g.nations.UK.quality, 6);
    expect(share(us('Philippine Division'))).toBeCloseTo(1 / g.nations.USA.quality, 6);
    expect(share(laid('8th Australian Division (22nd Brigade)'))).toBeCloseTo(g.nations.AUS.quality / g.nations.UK.quality, 6);
    expect(share(us('26th Cavalry, Philippine Scouts'))).toBeCloseTo(1 / g.nations.USA.quality, 6);
    // (The Philippine Army by row as it stood in the open in December 1941: the design director's order for Luzon,
    // (4), of 21 November 2026 and his 22:50Z. The 0.5 the reserve divisions read before was Bataan's, works counted.)
    expect(share(us('51st Division, Philippine Army'))).toBeCloseTo(0.3 / g.nations.USA.quality, 6);
    expect(share(us('41st Division, Philippine Army'))).toBeCloseTo(0.45 / g.nations.USA.quality, 6);
    expect(share(us('North Luzon Force'))).toBeCloseTo(0.35 / g.nations.USA.quality, 6);
    expect(share(us('101st Division, Philippine Army'))).toBeCloseTo(0.35 / g.nations.USA.quality, 6);
    expect(share(us('Philippine Army'))).toBeCloseTo(0.5 / g.nations.USA.quality, 6);
    const indian = laid('11th Indian Division (6th and 15th Brigades)');
    g.turn = 41;
    expect(share(indian)).toBe(1);
    g.turn = 28;
    // Every row the table names is a row of the world, and every country it names is in the game.
    // (The Celebes' garrisons by place are first-day rows of the hand-kept shores file: the Indies' record of 21 November 2026.)
    const R = (stations() as any).raised as { armies: Record<string, number>; rows: Record<string, number | string | [number, number]> };
    const world = new Set<string>([...(farWorld as any).world.corps.map((r: any[]) => r[1]), ...(farWorld as any)['setup-1939'].expeditions.map((r: any[]) => r[2]), ...(farWorld as any)['setup-1939'].formations.map((r: any[]) => r[2]), ...(farShores as any).world.corps.map((r: any[]) => r[1])]);
    for (const [nm, of] of Object.entries(R.rows)) { expect(world.has(nm), nm).toBe(true); if (typeof of === 'string') expect(g.nations[of]?.quality, of).toBeGreaterThan(0); }
    // (Vichy's corps in Indochina answer to Japan from the demand and fight at Vichy's own figure: the director's 16:46Z of 21 November 2026, far-indochina-kept.)
    expect(R.armies).toEqual({ NED: 0.8, VIC: 0.8 });
    expect(R.armies.VIC).toBe(g.nations.VIC.quality);
    // Britain's own rows are Britain's, and Japan's Japan's.
    expect(share(laid('18th British Division'))).toBe(1);
    expect(share(put(g, SASEBO, 'Ulithi', 0.2))).toBe(1);
    // A row's quality is a fact, so it is read at every stop of the dial (rider b630 to ruling 234): at the third stop
    // too the kept rows fight at the figure of the army that raised them, not their keeper's.
    const h = startOpen(createGame(3, null));
    declareWar(h, 'GER', 'NED');
    capitulate(h, 'NED', 'GER');
    const kept = h.corps.find((c) => c.name === 'Palembang garrison (the oil)')!;
    expect([kept.nat, kept.raised]).toEqual(['UK', 'NED']);
    const as = corpsBase(h, kept);
    delete kept.raised;
    expect(as / corpsBase(h, kept)).toBeCloseTo(0.8 / h.nations.UK.quality, 6);
  });

  it("carries the historian's mark on each dated landing: Wake met the place's works, Rabaul and Ambon one row's, the rest none", () => {
    const marks = ring().map((a) => [a.name, a.works ?? null]);
    expect(marks).toEqual([
      ['Wake: the first try', true], ['Wake: the taking', true],
      ['Menado', []], ['Tarakan', []], ['Rabaul', ['Lark Force (2/22nd Battalion)']], ['Kendari', []], ['Balikpapan', []],
      ['Ambon', ['Gull Force (2/21st Battalion)']], ['Palembang', []], ['Lae and Salamaua', []], ['Medan', []],
      ['Tulagi', null], ['Guadalcanal: the airfield party', null],
    ]);
    // Java's three landings by the historian's rows J1 to J3: Blackforce's river line behind Merak, the Tjiater pass in
    // full, nothing at Kragan. (The road to Soerabaja is no landing, and is read as rider b580 built it.)
    expect(attacks().filter((a) => a.name.startsWith('Java')).map((a) => [a.name, a.works ?? null])).toEqual([
      ['Java: Bantam Bay and Merak', ['Blackforce (2/3rd Machine Gun and 2/2nd Pioneer Battalions)']],
      ['Java: Eretan Wetan', true], ['Java: Kragan', []], ['Java: the road to Soerabaja', null],
    ]);
    // Every row a mark names is a row of the world.
    const world = new Set<string>([...(farWorld as any).world.corps.map((r: any[]) => r[1]), ...(farWorld as any)['setup-1939'].expeditions.map((r: any[]) => r[2]), ...(farWorld as any)['setup-1939'].formations.map((r: any[]) => r[2])]);
    for (const a of attacks()) if (Array.isArray(a.works)) for (const nm of a.works) expect(world.has(nm) || nm.startsWith('Lark Force'), nm).toBe(true);
  });

  it("Wake's works count in full against the landing that met them", () => {
    const g = war(28);
    only(g, 'Wake: the first try');
    put(g, MAIZURU, 'Kwajalein', 0.2);
    addCorps(g, 'USA', 'Wake detachment (a test row)', 'fort', 1, 'Wake', { xp: 0.2 }).str = 0.39;
    // (A fort names who built it, `by`, since rider b624 to ruling 232: matched on its level and whom it faces.)
    expect(g.byName.Wake.fort).toMatchObject({ lv: 1, vs: ['JAP'] });
    listen();
    bookAttacks(g, 'JAP');
    expect(labels('Wake: the first try')).toContain('Fortifications');
    expect(g.byName.Wake.fort).toMatchObject({ lv: 1, vs: ['JAP'] });
  });

  it("works that were not met are no part of the fight: Palembang's fieldworks set aside, and standing unworn after it", () => {
    const g = war(30);
    only(g, 'Palembang');
    japans(g, 'Cam Ranh');
    const d = dug(row(g, 'Palembang garrison (the oil)'));
    put(g, R229, 'Cam Ranh', 0.02);
    listen();
    bookAttacks(g, 'JAP');
    expect(made('Palembang').map((t) => t.taken)).toEqual([false]);
    expect(labels('Palembang')).not.toContain('Dug in');
    expect([d.dug, d.dugFor]).toEqual([true, 3]);
  });

  it("works count only for the defenders the record has fighting from them: at Ambon Gull Force's, not the Dutch row's", () => {
    const g = war(29);
    only(g, 'Ambon');
    japans(g, 'Davao');
    const dutch = dug(row(g, 'Great East garrisons (Ambon, Celebes, Timor)'));
    const gull = dug(addCorps(g, 'UK', 'Gull Force (2/21st Battalion)', 'inf', 1, 'Ambon', { xp: 0.2 }));
    gull.str = 0.1;
    put(g, ITO, 'Davao', 0.03);
    listen();
    bookAttacks(g, 'JAP');
    expect(made('Ambon').map((t) => t.taken)).toEqual([false]);
    expect(labels('Ambon')).toContain('Dug in');
    // The attack wore the works it met, and no others.
    expect([dutch.dug, dutch.dugFor]).toEqual([true, 3]);
    expect(gull.dug).toBe(true);
    expect(gull.dugFor!).toBeLessThan(3);
  });

  it("a set-aside of works holds for the fight only, never for supply or stores: a fortified port keeps its garrison fed", () => {
    const g = war(28);
    const wake = g.byName.Wake, works = wake.fort;
    const garrison = addCorps(g, 'USA', 'Wake detachment (a test row)', 'fort', 1, 'Wake', { xp: 0.2 });
    // Its sea open, the port fills its stores this month.
    computeSupply(g);
    expect(g.flags[`stores:${wake.id}`]).toBe(28);
    // Japan's fleets close the sea: the stores feed the garrison.
    for (const f of g.fleets) if (f.nat === 'JAP' && ['Mobile Fleet', '4th Fleet'].includes(f.name)) { f.zone = wake.seas[0]; f.stance = 'patrol'; }
    expect(closedTo(g, wake.seas[0], 'USA')).toBe(true);
    computeSupply(g);
    expect(garrison.supplied).toBe(true);
    // And still do while the works are set aside for a fight.
    withWorksAside(wake, () => {
      expect(wake.fort).toBeNull();
      expect(worksKept(wake)).toBe(works);
      computeSupply(g);
      expect(garrison.supplied).toBe(true);
    });
    expect(wake.fort).toBe(works);
    expect(worksKept(g.byName.Guam)).toBe(g.byName.Guam.fort);
  });
});
