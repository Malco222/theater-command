// The later moves of the tank regiments Japan's 1st and 2nd Tank Divisions were made of (the design director's ruling
// of 04:01Z, 22 November 2026, item 4; the historian's paper japan-tank-divisions-1942.md, section 2, and his answers
// of 04:43Z; data/campaign/far-world.json by the rider file pacific-r2/riders-tanks-1122.json).
//
// Data, at the stops that keep the book (1 and 2), in the form the 21st Division's detachment for Bataan has (rider
// b642 part 2 to ruling 234): a sister row takes the division off the board and puts it back under a name that says
// what it is less, at the smaller size, and the regiment's own row is marked 'cut' with that sister row, so that it
// comes only once the cut is made. Each regiment takes its own row's 0.25 out of its division.
//   February 1944 (54)  the 11th Tank Regiment to Shumshu in the Kuriles (the chart's Paramushiro);
//   March 1944 (55)     the 9th Tank Regiment to Saipan, and its 1st and 2nd Companies to Guam (its 0.25 split
//                       0.15 and 0.1 by the count of its tanks: nothing grows on the way);
//   April 1944 (56)     the 3rd Tank Regiment leaves for China, and stands with the 11th Army at Yochow from May (57);
//   August 1944 (60)    the 2nd Tank Division to the 14th Area Army on Luzon.
// The states are set by hand on a made-up December 1941 (far-lent-rows.test.ts); only the opening of Japan's month is
// played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { bookMonth } from '../src/campaign/claims';
import { monthlyDiplomacy } from '../src/campaign/diplomacy';
import { atWar, capitulate, createGame, declareWar, hostileUnits, removeCorps, startPhase } from '../src/campaign/game';
import setup from '../data/campaign/setup-1939.json';
import { setStop } from '../src/campaign/history';
import { navalUpkeep } from '../src/campaign/navy';
import { ops, startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

const corps = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead);
const D1 = '1st Tank Division', D2 = '2nd Tank Division';
const D1A = '1st Tank Division (less the 9th Tank Regiment)', D1B = '1st Tank Division (less the 3rd and 9th Tank Regiments)';
const D2A = '2nd Tank Division (less the 11th Tank Regiment)', D2L = '14th Area Army (2nd Tank Division)';
const R11 = '11th Tank Regiment (Shumshu)', R9S = '9th Tank Regiment (Saipan)', R9G = '9th Tank Regiment (1st and 2nd Companies, Guam)', R3 = '11th Army (3rd Tank Regiment)';
const ALL = [D1, D1A, D1B, D2, D2A, D2L, R11, R9S, R9G, R3];
const here = (g: Game, names: string[] = ALL): string[] => names.filter((nm) => !!corps(g, nm));
/** Where a row stands and at what worth: province, strength, the fullest it gets. */
const at = (g: Game, name: string): [string, number, number] => { const c = corps(g, name)!; return [g.regions[c.reg].name, +c.str.toFixed(9), c.full ?? 1]; };

/** A made-up December 1941 (far-lent-rows.test.ts): Japan's dated rows taken in. */
const december = (stop: number): Game => {
  const g = startOpen(createGame(17, null));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = 9;
  declareWar(g, 'GER', 'NED');
  capitulate(g, 'NED', 'GER');
  g.turn = 10;
  capitulate(g, 'FRA', 'GER');
  for (const t of [22, 23, 24, 25, 27]) { g.turn = t; bookMonth(g); monthlyDiplomacy(g); if (t === 23 || t === 24 || t === 27) startPhase(g, 'JAP'); }
  for (const op of ops('JAP')) if (op.id !== 'south') g.flags[`st:done:JAP:${op.id}`] = 1;
  g.turn = 28;
  bookMonth(g);
  monthlyDiplomacy(g);
  navalUpkeep(g, 'USA');
  return g;
};
/** The same with the strike south gone in December 1941, and the divisions formed in June 1942. */
const formed = (stop: number, war = true): Game => {
  const g = december(stop);
  if (war) {
    for (const e of ['USA', 'UK', 'NED', 'AUS', 'NZL']) if (!atWar(g, 'JAP', e)) declareWar(g, 'JAP', e);
    g.flags['st:done:JAP:south'] = 28;
  }
  for (const t of [32, 33, 34, 35]) month(g, t);
  return g;
};
const month = (g: Game, turn: number): void => { g.turn = turn; startPhase(g, 'JAP'); };
/** A place in Japan's hands, by hand: whoever of her enemies stood there is gone, as after a fight lost. */
const take = (g: Game, place: string): void => {
  const r = g.byName[place];
  for (const c of hostileUnits(g, r, 'JAP')) removeCorps(g, c);
  r.ctrl = 'JAP';
};

describe('the tank regiments\' later moves (the director\'s ruling of 04:01Z, 22 November 2026, item 4)', () => {
  it('eight rows at stops 1 and 2: each division\'s sister row, and each regiment cut from it', () => {
    const rows = ((setup as any).expeditions as any[]).filter((r) => r[1] === 'JAP' && ALL.includes(r[2]) && r[2] !== D1 && r[2] !== D2);
    expect(rows.map((r) => [r[0], r[2], r[5], r[6].str, r[6].est, r[6].from ?? null, r[6].cut ?? null, r[6].war, r[6].stops])).toEqual([
      [54, D2A, 'Mutankiang', 0.55, 0.55, [D2], null, ['JAP', 'USA'], [1, 2]],
      [54, R11, 'Paramushiro', 0.25, 0.25, null, D2A, ['JAP', 'USA'], [1, 2]],
      [55, D1A, 'Mutankiang', 0.55, 0.55, [D1], null, ['JAP', 'USA'], [1, 2]],
      [55, R9S, 'Saipan', 0.15, 0.15, null, D1A, ['JAP', 'USA'], [1, 2]],
      [55, R9G, 'Guam', 0.1, 0.1, null, D1A, ['JAP', 'USA'], [1, 2]],
      [56, D1B, 'Mutankiang', 0.3, 0.3, [D1A], D1A, ['JAP', 'CHN'], [1, 2]],
      [57, R3, 'Yochow', 0.25, 0.25, null, D1B, ['JAP', 'CHN'], [1, 2]],
      [60, D2L, 'Manila', 0.55, 0.55, [D2A], D2A, ['JAP', 'USA'], [1, 2]],
    ]);
    // Every place is a province of the chart.
    const g = december(1);
    expect(rows.map((r) => !!g.byName[r[5]])).toEqual(rows.map(() => true));
  });

  it('by the book: the 11th to the Kuriles in February 1944, the 9th to Saipan and Guam in March, the 3rd to China in April and May, the 2nd Tank Division to Luzon in August', () => {
    const g = formed(1);
    for (const p of ['Guam', 'Manila']) take(g, p);
    expect(here(g)).toEqual([D1, D2]);
    // February 1944: the 2nd Tank Division gives up the 11th's quarter, and the regiment stands in the Kuriles.
    month(g, 54);
    expect(here(g)).toEqual([D1, D2A, R11]);
    expect(at(g, D2A)).toEqual(['Mutankiang', 0.55, 0.55]);
    expect(at(g, R11)).toEqual(['Paramushiro', 0.25, 0.25]);
    // March: the 1st gives up the 9th's, and the regiment stands on Saipan with two companies on Guam.
    month(g, 55);
    expect(here(g)).toEqual([D1A, D2A, R11, R9S, R9G]);
    expect(at(g, D1A)).toEqual(['Mutankiang', 0.55, 0.55]);
    expect([at(g, R9S), at(g, R9G)]).toEqual([['Saipan', 0.15, 0.15], ['Guam', 0.1, 0.1]]);
    // April: the 3rd leaves the 1st. May: it stands with the 11th Army at Yochow.
    month(g, 56);
    expect(here(g)).toEqual([D1B, D2A, R11, R9S, R9G]);
    expect(at(g, D1B)).toEqual(['Mutankiang', 0.3, 0.3]);
    // (Room is made at Yochow by hand: in this made-up war nobody marches, and a full province sends a row on to the
    // nearest ground with room, as it does every row.)
    for (const c of [...g.byName['Yochow'].units]) if (c.nat === 'JAP') removeCorps(g, c);
    month(g, 57);
    expect(here(g)).toEqual([D1B, D2A, R11, R9S, R9G, R3]);
    expect(at(g, R3)).toEqual(['Yochow', 0.25, 0.25]);
    // August: the 2nd Tank Division, less the 11th, is the 14th Area Army's on Luzon.
    month(g, 60);
    expect(here(g)).toEqual([D1B, D2L, R11, R9S, R9G, R3]);
    expect(at(g, D2L)).toEqual(['Manila', 0.55, 0.55]);
    // The regiments on their islands and the two divisions' sister rows are held where the record had them; the two
    // that went to a campaign are their staff's.
    expect([D1B, R11, R9S, R9G, R3, D2L].map((nm) => !!corps(g, nm)!.hold)).toEqual([true, true, true, true, false, false]);
  });

  it('only where the stop keeps the book: at stop 3 the divisions stand whole', () => {
    const g = formed(3);
    for (const p of ['Guam', 'Manila', 'Singapore', 'Bataan']) take(g, p);
    for (const t of [36, 54, 55, 56, 57, 60]) month(g, t);
    expect(here(g)).toEqual([D1, D2]);
  });

  it('a regiment comes only once its division is cut, and a second cut only after the first', () => {
    // No war with America: the rows of February and March 1944 wait for it, and with them April's cut of the 1st
    // (named after March's) and the 3rd's own row, though Japan is at war with China.
    const g = formed(1, false);
    expect(atWar(g, 'JAP', 'CHN')).toBe(true);
    for (const t of [54, 55, 56, 57, 60]) month(g, t);
    expect(here(g)).toEqual([D1, D2]);
  });
});
