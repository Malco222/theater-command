// British Borneo and the 2nd Division's road to Java by the book (the design director's rulings of 03:59Z and 04:06Z,
// 22 November 2026, on the trace of seed 19 at the first stop; the historian's paper
// second-division-and-british-borneo-1941-42.md; src/campaign/stations.ts and ai.ts, hop).
//
// At a stop that keeps the book the plan's fronts are not read, so nothing carried the Kawaguchi Detachment on from
// Brunei, and the staff's ships took the 16th Army's 2nd Division from Sendai in December 1941 for a shore of their
// own (it landed at Kuching in February 1942, the month before its landing in Java). Now:
//   the rule   the staff's hop leaves a corps the book names in a dated attack still to come (`bookedAhead`);
//   the data   the 2nd Division stands where the record had it, held there: Tokyo (Narashino) in December 1941,
//              Formosa (Takao) in January, Cam Ranh in February, and lands in Java in March as the book had it;
//              the Kawaguchi Detachment takes Miri from Cam Ranh, Brunei overland and Kuching by sea in December
//              (about 0.2 of it against the Punjabis and Sarawak's forces, 0.1), Jesselton and Sandakan in January.
// At the stops that keep no book the corps and Borneo's shores are the staff's, as before. With the far side's switch
// off none of it exists.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { hop, hopWatch } from '../src/campaign/ai';
import { addCorps, atWar, capitulate, computeSupply, createGame, declareWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import { bookAttacks, bookStations, bookWatch, bookedAhead, stations, type Attack } from '../src/campaign/stations';
import setup from '../data/campaign/setup-1939.json';
import type { Corps, Game } from '../src/campaign/types';

const KAWAGUCHI = 'Kawaguchi Detachment (35th Brigade, of the 18th Division)';
const WESTERN = 'Kawaguchi Detachment (the western force, for Pontianak)';
const BATTALION = 'Kawaguchi Detachment (the North Borneo battalion)';
const SECOND = '16th Army (2nd Division)';
const TROOPS = '16th Army troops (landing with the 2nd Division)';
const PUNJAB = '2/15th Punjab (Kuching)';
const BORNEO = ['Miri and Seria', 'Brunei', 'Kuching', 'Jesselton', 'Sandakan'];
const JAVA = 'Java: Bantam Bay and Merak';

const attacks = (): Attack[] => stations()!.attacks!;
const index = (name: string): number => attacks().findIndex((a) => a.name === name);
const dated = (name: string): any[] => ((setup as any).expeditions as any[]).find((r) => r[2] === name)!;

/** A book war at stop `stop` in month `turn`, Japan's blow fallen in December 1941 and the Indies in Britain's keeping (tests/far-ring.test.ts). */
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
/** A place with nobody in its field and no town guard: a landing there walks in. */
const open = (g: Game, name: string): void => { const r = g.byName[name]; r.units = r.units.filter((u) => u.nat === 'JAP'); g.flags[`guard:${r.id}`] = 100; };
const put = (g: Game, nat: string, name: string, place: string, str: number): Corps => { const c = addCorps(g, nat, name, 'inf', 1, place, { xp: 0.1 }); c.str = str; return c; };
const where = (g: Game, c: Corps): string => g.regions[c.reg].name;

type Told = { name: string; what: string; why?: string; taken?: boolean; fought: boolean };
const told: Told[] = [];
const listen = (): void => { told.length = 0; bookWatch.tell = (e) => { told.push({ name: e.row.name, what: e.what, why: e.why, taken: e.taken, fought: !!e.report }); }; };
afterEach(() => { bookWatch.tell = null; });
const made = (name: string): Told[] => told.filter((t) => t.name === name && t.what === 'made');

describe("British Borneo by the book (the director's rulings of 03:59Z and 04:06Z, 22 November 2026)", () => {
  it("lays the historian's rows after the table's: each landing's month, place, force, port and met figures", () => {
    const rows = attacks().filter((a) => BORNEO.includes(a.name));
    // (A row names every corps that may make it: all of them that live go in together. At the book's stops the
    // detachment is four rows from January 1942, the director's ruling of 07:16Z, and the part whose landing it was
    // is named beside the whole: tests/far-borneo-split.test.ts.)
    expect(rows.map((a) => [a.month, a.name, !!a.sea, a.at, a.force, a.from, a.met ?? null])).toEqual([
      [28, 'Miri and Seria', true, ['Miri'], [KAWAGUCHI], ['Cam Ranh'], null],
      // (Brunei was reached overland from Miri and Seria: no landing.)
      [28, 'Brunei', false, ['Brunei'], [KAWAGUCHI], ['Miri'], null],
      // (The record's fight for Kuching: about 0.2 of the detachment against the Punjabis and Sarawak's forces.)
      [28, 'Kuching', true, ['Kuching'], [KAWAGUCHI, WESTERN], ['Miri', 'Brunei'], [0.2, 0.1]],
      [29, 'Jesselton', true, ['Jesselton'], [KAWAGUCHI, BATTALION], ['Brunei', 'Miri'], null],
      [29, 'Sandakan', true, ['Sandakan'], [KAWAGUCHI, BATTALION], ['Jesselton'], null],
    ]);
    // (A saved game counts the rows by their place in the list: Borneo's follow the rows that were there.)
    expect(BORNEO.map(index)).toEqual([33, 34, 35, 36, 37]);
    // Every place is a province of the chart, and the months are inside the record's line there.
    const g = war(28);
    for (const a of rows) for (const nm of [...a.at, ...(a.from ?? [])]) expect(g.byName[nm], nm).toBeDefined();
    // The forces are the rows the world already had: the detachment at about 0.3, the defenders at 0.1.
    expect([dated(KAWAGUCHI)[6].str, dated(PUNJAB)[6].str]).toEqual([0.325, 0.1]);
  });

  it('December 1941: Miri from Cam Ranh, Brunei overland, and Kuching by sea at the record\'s figures', () => {
    const g = war(28);
    only(g, ...BORNEO);
    japans(g, 'Cam Ranh');
    open(g, 'Miri');
    open(g, 'Brunei');
    const k = put(g, 'JAP', KAWAGUCHI, 'Cam Ranh', 0.325);
    g.byName.Kuching.units = [];
    const p = put(g, 'UK', PUNJAB, 'Kuching', 0.1);
    listen();
    bookAttacks(g, 'JAP');
    expect(made('Miri and Seria').map((t) => [t.taken, t.fought])).toEqual([[true, false]]);
    expect(made('Brunei').map((t) => [t.taken, t.fought])).toEqual([[true, false]]);
    expect(made('Kuching').map((t) => t.fought)).toEqual([true]);
    expect([g.byName.Miri.ctrl, g.byName.Brunei.ctrl]).toEqual(['JAP', 'JAP']);
    // The fight is the game's own, at the record's figures: about 0.2 of the detachment met 0.1, and what it cost
    // fell on the part that fought. Won, the detachment stands in Kuching.
    expect(made('Kuching')[0].taken).toBe(true);
    expect([g.byName.Kuching.ctrl, where(g, k)]).toEqual(['JAP', 'Kuching']);
    expect(k.str).toBeGreaterThan(0.325 - 0.2);
    expect(k.str).toBeLessThan(0.325);
    expect(p.dead || where(g, p) !== 'Kuching').toBe(true);
    // The landings of January are not due yet.
    expect([made('Jesselton'), made('Sandakan')]).toEqual([[], []]);
  });

  it('January 1942: Jesselton from Brunei Bay and then Sandakan, each from the place just taken', () => {
    const g = war(29);
    only(g, ...BORNEO);
    for (const nm of ['Cam Ranh', 'Miri', 'Brunei', 'Kuching']) japans(g, nm);
    for (const nm of ['Miri and Seria', 'Brunei', 'Kuching']) g.flags[`ba:${index(nm)}`] = 28;
    open(g, 'Jesselton');
    open(g, 'Sandakan');
    const k = put(g, 'JAP', KAWAGUCHI, 'Kuching', 0.325);
    listen();
    bookAttacks(g, 'JAP');
    expect(made('Jesselton').map((t) => t.taken)).toEqual([true]);
    expect(made('Sandakan').map((t) => t.taken)).toEqual([true]);
    expect([g.byName.Jesselton.ctrl, g.byName.Sandakan.ctrl, where(g, k)]).toEqual(['JAP', 'JAP', 'Sandakan']);
    expect([g.flags[`ba:${index('Jesselton')}`], g.flags[`ba:${index('Sandakan')}`]]).toEqual([29, 29]);
    // Its landings made, the detachment is its staff's again.
    expect([k.hold, bookedAhead(g, k)]).toEqual([false, false]);
  });

  it('only where the stop keeps the book: at the third stop no row of it goes in', () => {
    const g = war(28, 3);
    only(g, ...BORNEO);
    japans(g, 'Cam Ranh');
    open(g, 'Miri');
    const k = put(g, 'JAP', KAWAGUCHI, 'Cam Ranh', 0.325);
    listen();
    bookAttacks(g, 'JAP');
    expect(told).toEqual([]);
    expect([where(g, k), g.byName.Miri.ctrl]).toEqual(['Cam Ranh', 'UK']);
  });
});

describe("the 2nd Division's road to Java (the director's rulings of 03:59Z and 04:06Z, 22 November 2026)", () => {
  it('its row of the stations: Tokyo in December 1941, Formosa in January, Cam Ranh in February, held there by the book', () => {
    const row = stations()!.rows.find((r) => r.nat === 'JAP' && r.corps === SECOND)!;
    expect([row.steps, row.free ?? false]).toEqual([[[28, ['Tokyo']], [29, ['Formosa']], [30, ['Cam Ranh']]], false]);
    // The division is 1.0 and its army troops 0.1, a row of their own that goes with it: the record's 1.1. The 2nd
    // Tank Regiment is its own row too, with the 25th Army in Malaya: nothing is counted twice.
    expect([dated(SECOND)[6].str, dated(TROOPS)[6].str, dated('2nd Tank Regiment')[6].str, dated('2nd Tank Regiment')[5]]).toEqual([1, 0.1, 0.25, 'Kota Bharu']);
    expect(attacks()[index(JAVA)].force).toEqual([SECOND, TROOPS]);

    const g = war(28);
    japans(g, 'Cam Ranh');
    const c = put(g, 'JAP', SECOND, 'Sendai', 1);
    const seen: [number, string, boolean, boolean][] = [];
    for (const t of [28, 29, 30]) { g.turn = t; bookStations(g, 'JAP'); seen.push([t, where(g, c), c.hold, !!c.stay]); }
    expect(seen).toEqual([[28, 'Tokyo', true, true], [29, 'Formosa', true, true], [30, 'Cam Ranh', true, true]]);
  });

  it('January and February wait for the war: without the blow the division stands at Tokyo', () => {
    const g = startOpen(createGame(3, null));
    setStop(g, 1, true);
    const c = put(g, 'JAP', SECOND, 'Sendai', 1);
    for (const t of [28, 29, 30]) { g.turn = t; bookStations(g, 'JAP'); }
    expect(where(g, c)).toBe('Tokyo');
  });

  it('a corps the book names in a dated attack still to come is left by the staff\'s ships, at the stops that keep the book', () => {
    const g = war(28);
    const c = put(g, 'JAP', SECOND, 'Sendai', 1);
    const k = put(g, 'JAP', KAWAGUCHI, 'Cam Ranh', 0.325);
    const other = put(g, 'JAP', 'a division the book names in no attack', 'Sendai', 1);
    expect([bookedAhead(g, c), bookedAhead(g, k), bookedAhead(g, other)]).toEqual([true, true, false]);
    // Its landing made (or its place its side's some other way), it is the staff's to judge again.
    g.flags[`ba:${index(JAVA)}`] = 31;
    expect(bookedAhead(g, c)).toBe(false);
    // Never another country's corps, and never past the table's last month.
    const uk = put(g, 'UK', SECOND, 'Singapore', 1);
    expect(bookedAhead(g, uk)).toBe(false);
    g.turn = stations()!.until + 1;
    expect(bookedAhead(g, k)).toBe(false);
  });

  it('the staff\'s ships leave the division at Sendai while Java is still to come, and take it once its landing is made', () => {
    // A made-up December 1941: the division fit and fed at Sendai, shipping in hand, shores open to the staff.
    const month = (stop: number, javaMade: boolean): string => {
      const g = war(28, stop);
      for (const e of ['AUS', 'NZL']) if (!atWar(g, 'JAP', e)) declareWar(g, 'JAP', e);
      const c = put(g, 'JAP', SECOND, 'Sendai', 1);
      if (javaMade) g.flags[`ba:${index(JAVA)}`] = 28;
      computeSupply(g);
      const took: string[] = [];
      hopWatch.tell = (e) => { if (e.force.includes(c) && (e.kind === 'land' || e.kind === 'bring')) took.push(e.kind); };
      try { hop(g, 'JAP', null); } finally { hopWatch.tell = null; }
      return `${where(g, c) === 'Sendai' ? 'left at Sendai' : 'taken'}${took.length ? ' (' + took.join(', ') + ')' : ''}`;
    };
    expect(month(1, false)).toBe('left at Sendai');
    expect(month(1, true)).toBe('taken (land)');
    // At the third stop the hop is the staff's judgment, the book's table or not.
    expect([month(3, false), month(3, true)]).toEqual(['taken (land)', 'taken (land)']);
  });

  it('at the stops that keep no book the corps is the staff\'s to judge, as before', () => {
    const g = war(28, 3);
    const c = put(g, 'JAP', SECOND, 'Sendai', 1);
    expect(bookedAhead(g, c)).toBe(false);
    bookStations(g, 'JAP');
    expect([where(g, c), c.hold]).toEqual(['Sendai', false]);
  });
});
