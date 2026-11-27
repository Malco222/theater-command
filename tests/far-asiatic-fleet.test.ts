// The Asiatic Fleet goes south by the book (the design director's ruling of 03:55Z, 22 November 2026, on the
// historian's paper asiatic-fleet-south-1941-42.md; data/campaign/far-pacific-war.json 'navy'.moves, and
// src/campaign/navy.ts, FleetMove and orderedTo).
//
// The fleet's row had no dated move, so at the stops that keep the book it stayed on its station off Luzon until
// its base fell. The record's Hart had sent most of his surface ships south before the war (the flagship lay at
// Iloilo from 2 December); the fleet was based at Surabaya in January 1942, at Tjilatjap from 19 February and at
// Fremantle from March.
//
// Four rows, at stops 1 and 2, all named with Japan's strike south, so that they keep its month where the stop draws
// it. The first is marked 'eve' (the director's ruling of 04:25Z): Iloilo, on Panay, America's own ground, in the
// strike's own month and before the blow, on the book's word that the blow falls that month. The three after it are
// read only at war with Japan and are marked 'ally' (the harbour may be one an ally at war with Japan holds):
// Surabaya (the chart's Soerabaja) in month 29, Tjilatjap (the harbour of the chart's Semarang) in 30, Fremantle
// (the port of the chart's Perth) in 31. The wars are declared by hand on the first day's chart, and only America's
// month at sea is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { setDesk } from '../src/campaign/depth';
import { allied, atWar, createGame, declareWar } from '../src/campaign/game';
import { bookDate, setStop } from '../src/campaign/history';
import { NAVY, navalUpkeep, zoneOf } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game } from '../src/campaign/types';

const start = (stop: number, human: string | null = null, seed = 3): Game => {
  const g = startOpen(createGame(seed, human));
  if (stop !== 3) setStop(g, stop, true);
  return g;
};
/** Germany at war with America, which puts America beside Britain and the Dutch. */
const europe = (g: Game): Game => { for (const e of ['SOV', 'USA']) if (!atWar(g, 'GER', e)) declareWar(g, 'GER', e); return g; };
/** Japan's blow: at war with America, Britain, the Dutch and the dominions. */
const pacific = (g: Game): Game => { for (const e of ['USA', 'UK', 'NED', 'AUS', 'NZL']) if (!atWar(g, 'JAP', e)) declareWar(g, 'JAP', e); return europe(g); };
const asiatic = (g: Game): Fleet => g.fleets.find((f) => f.nat === 'USA' && f.name === 'Asiatic Fleet')!;
/** America's month at sea in the month named, and where the fleet is based after it: province, harbour, home sea, stance. */
const month = (g: Game, turn: number): [string, string, string, string] => {
  g.turn = turn;
  navalUpkeep(g, 'USA');
  const f = asiatic(g);
  return [g.regions[f.base].name, f.port ?? '', g.seas[f.home].key, f.stance];
};
const news = (g: Game, turn: number): string[] => g.headlines.filter((h) => h.turn === turn && h.text.includes('Asiatic Fleet')).map((h) => h.text);
const MANILA = ['Manila', 'Manila', 'luzonsea', 'port'];

describe('the Asiatic Fleet goes south by the book (the director\'s ruling of 03:55Z, 22 November 2026)', () => {
  it('the book has four rows for it, at stops 1 and 2, named with the strike south: the eve\'s, and three marked for an ally\'s harbour', () => {
    const rows = (NAVY.moves ?? []).filter((m) => m.nat === 'USA' && m.fleet === 'Asiatic Fleet');
    expect(rows.map((m) => [m.turn, m.base, m.port, m.home, m.stops, m.with, m.eve ?? false, m.ally ?? false])).toEqual([
      [28, 'Panay', 'Iloilo', 'sulu', [1, 2], 'JAP:south', true, false],
      [29, 'Soerabaja', 'Surabaya', 'java', [1, 2], 'JAP:south', false, true],
      [30, 'Semarang', 'Tjilatjap', 'eindian', [1, 2], 'JAP:south', false, true],
      [31, 'Perth', 'Fremantle', 'westaus', [1, 2], 'JAP:south', false, true],
    ]);
    // Each harbour is a port of the chart on the sea its row names.
    const g = start(1);
    for (const m of rows) expect([m.base, !!g.byName[m.base!]?.port, g.byName[m.base!].seas.includes(zoneOf(g, m.home!))]).toEqual([m.base, true, true]);
  });

  it('by the book: Iloilo before the blow, then Surabaya in January 1942, Tjilatjap in February, Fremantle in March', () => {
    // December 1941, America's month, at peace with Japan: the book lays the blow for this month, and the fleet goes.
    const g = start(1);
    expect(atWar(g, 'USA', 'JAP')).toBe(false);
    expect(month(g, 27)).toEqual(MANILA);
    expect(month(g, 28)).toEqual(['Panay', 'Iloilo', 'sulu', 'port']);
    expect(news(g, 28)).toEqual(['The Asiatic Fleet leaves Manila for Iloilo, on Panay.']);
    // Japan's blow, later in the month.
    pacific(g);
    expect([allied(g, 'USA', 'NED'), atWar(g, 'NED', 'JAP'), allied(g, 'USA', 'AUS'), atWar(g, 'AUS', 'JAP')]).toEqual([true, true, true, true]);
    expect(month(g, 29)).toEqual(['Soerabaja', 'Surabaya', 'java', 'port']);
    expect(news(g, 29)).toEqual(['The Asiatic Fleet goes south to Surabaya.']);
    // It lies in harbour in the Java Sea, and that sea is its station from now.
    const f = asiatic(g);
    expect([g.seas[f.zone].key, g.seas[f.station].key]).toEqual(['java', 'java']);
    expect(month(g, 30)).toEqual(['Semarang', 'Tjilatjap', 'eindian', 'port']);
    expect(news(g, 30)).toEqual(['The Asiatic Fleet moves to Tjilatjap, on Java\'s south coast.']);
    expect(month(g, 31)).toEqual(['Perth', 'Fremantle', 'westaus', 'port']);
    expect(news(g, 31)).toEqual(['The Asiatic Fleet falls back to Fremantle.']);
  });

  it('the eve is the book\'s: where the book lays no blow nothing is read before the war, and the three after it wait for the war', () => {
    // The book shut in the Pacific, and America fighting Germany beside the Dutch: the fleet goes nowhere.
    const g = europe(start(1));
    declareWar(g, 'GER', 'NED');
    g.flags['hist:off:pacific'] = 27;
    expect([allied(g, 'USA', 'NED'), atWar(g, 'USA', 'JAP')]).toEqual([true, false]);
    for (const t of [28, 29, 30, 31]) expect(month(g, t)).toEqual(MANILA);
    // A player's Japan: the book lays no blow for him, and the surprise is his. After his blow the rows read the war.
    const p = start(1, 'JAP');
    expect(month(p, 28)).toEqual(MANILA);
    pacific(p);
    expect(month(p, 29)[0]).toBe('Soerabaja');
    // By the book with the blow never falling: the fleet lies at Iloilo, and no later row takes it on with no war.
    const q = europe(start(1));
    declareWar(q, 'GER', 'NED');
    expect(month(q, 28)[0]).toBe('Panay');
    for (const t of [29, 30, 31]) expect(month(q, t)[0]).toBe('Panay');
    // The eve is read before the blow alone: a war with Japan already on, and the fleet waits at Manila for January's row.
    const w = pacific(start(1));
    expect(month(w, 28)).toEqual(MANILA);
    expect(month(w, 29)[0]).toBe('Soerabaja');
  });

  it('a harbour that is no ally\'s at war with Japan is passed by, and the next row is read in its own month', () => {
    // Surabaya fallen to Japan: the fleet stays where it is, and goes to Tjilatjap in its month.
    const g = pacific(start(1));
    g.byName['Soerabaja'].ctrl = 'JAP';
    expect(month(g, 29)).toEqual(MANILA);
    expect(month(g, 30)[0]).toBe('Semarang');
    // Surabaya in the hands of an ally that is not at war with Japan: no harbour for it either.
    const h = pacific(start(1));
    h.byName['Soerabaja'].ctrl = 'FRA';
    expect([allied(h, 'USA', 'FRA'), atWar(h, 'FRA', 'JAP')]).toEqual([true, false]);
    expect(month(h, 29)).toEqual(MANILA);
  });

  it('only where the stop keeps the book: at stop 3 the fleet is the staff\'s', () => {
    const g = start(3);
    expect(month(g, 28)).toEqual(MANILA);
    pacific(g);
    for (const t of [29, 30, 31]) expect(month(g, t)[0]).toBe('Manila');
  });

  it('at stop 2 the rows keep their distance from the strike, whose month the stop draws', () => {
    // A war whose strike south is drawn a month late (January 1942): Iloilo in January, before the blow, and Surabaya
    // in February.
    let seed = 1;
    while (seed < 200 && bookDate(start(2, null, seed), 'JAP', 'south') !== 29) seed++;
    const g = start(2, null, seed);
    expect(bookDate(g, 'JAP', 'south')).toBe(29);
    expect(month(g, 28)).toEqual(MANILA);
    expect(month(g, 29)[0]).toBe('Panay');
    pacific(g);
    expect(month(g, 30)[0]).toBe('Soerabaja');
    expect(month(g, 31)[0]).toBe('Semarang');
    expect(month(g, 32)[0]).toBe('Perth');
  });

  it('a player\'s America: his own to move where he runs the navy himself, and the staff keeps the dates where he has handed it over', () => {
    const g = pacific(start(1, 'USA'));
    expect(month(g, 29)).toEqual(MANILA);
    const h = pacific(start(1, 'USA'));
    setDesk(h, 'navy', true);
    expect(month(h, 29)[0]).toBe('Soerabaja');
  });
});
