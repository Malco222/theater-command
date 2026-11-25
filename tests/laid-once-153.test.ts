// A division is laid once (1.5.3, the grand campaign; src/campaign/laidonce.ts, data/campaign/laidonce.json).
//
// The design director's rulings of 13:43Z and 19:17Z, 25 November 2026, and the historian's reads of the seven rows
// (historian/named-twice-reads-1125.md). Read on a board set by hand: Germany's first wars stood still, Italy's war on
// Greece begun with Germany and the Balkan allies in it, as in the spring of 1941; Britain at war with Italy. Every
// dated row of the country but the one read is marked as come already, so that one row is read alone.
import { describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { divisionsOf } from '../src/campaign/armies';
import { addCorps, createGame, declareWar, relocate, removeCorps, startPhase } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

const ROWS = (setup as any).expeditions as any[];
const PANZERS = '2nd and 9th Panzer Divisions', INFANTRY = '50th, 72nd and 164th Infantry Divisions', EIGHTH = '8th and 14th Panzer Divisions';

/** Germany's first wars stood still; Italy and Germany at war with Greece, Bulgaria, Hungary and Romania with them. */
function balkans(human: string | null = null, open = true, still = true): Game {
  const g = open ? startOpen(createGame(1, human)) : createGame(1, human);
  if (still) for (const x of [...g.nations.GER.atWar]) { g.nations.GER.atWar = g.nations.GER.atWar.filter((y) => y !== x); g.nations[x].atWar = g.nations[x].atWar.filter((y) => y !== 'GER'); }
  declareWar(g, 'ITA', 'GRE');
  declareWar(g, 'GER', 'GRE');
  for (const n of ['BUL', 'HUN', 'ROM']) { g.nations[n].align = 'axis'; declareWar(g, n, 'GRE'); }
  return g;
}
/** Every row of the country marked as come, but those named. */
function only(g: Game, nat: string, ...names: string[]): void {
  ROWS.forEach((r, i) => { if (r[1] === nat && !names.includes(r[2])) g.flags[`sent:${i}`] = 1; });
}
const month = (g: Game, nat: string, turn: number): void => { g.turn = turn; startPhase(g, nat); };
const corps = (g: Game, nat: string, name: string): Corps | undefined => g.corps.find((c) => c.nat === nat && !c.dead && c.name === name);
const names = (g: Game, c: Corps | undefined): string[] => (c ? divisionsOf(g, c).map((d) => d.name) : []);
/** The living corps of a country that hold a division of this name. */
const holders = (g: Game, nat: string, name: string): Corps[] => g.corps.filter((c) => c.nat === nat && !c.dead && names(g, c).includes(name));

describe('Germany\'s Balkan rows draw the divisions her corps hold', () => {
  it('the 2nd and 9th Panzer come out of XXII Corps, with its experience: none is raised again', () => {
    // (The month's drafts go out after its rows and fill a corps under strength, so experience is read, at full strength.)
    const g = balkans();
    only(g, 'GER', PANZERS);
    const xxii = corps(g, 'GER', 'XXII Corps')!;
    xxii.xp = 0.9;
    month(g, 'GER', 19);
    const row = corps(g, 'GER', PANZERS)!;
    expect(xxii.dead).toBe(true);
    expect(names(g, row)).toEqual(['2nd Panzer Division', '9th Panzer Division']);
    expect(row.xp).toBeCloseTo(0.9, 9);
    expect(holders(g, 'GER', '2nd Panzer Division')).toHaveLength(1);
    expect(holders(g, 'GER', '9th Panzer Division')).toHaveLength(1);
  });

  it('they come as worn as they were in XXII Corps, not fresh', () => {
    const g = balkans();
    only(g, 'GER', PANZERS);
    corps(g, 'GER', 'XXII Corps')!.str = 0.3;
    month(g, 'GER', 19);
    expect(corps(g, 'GER', PANZERS)!.str).toBeLessThan(1);
  });

  it('III Corps gives the 50th and keeps the 218th and 207th; the 72nd and 164th are raised new', () => {
    const g = balkans();
    only(g, 'GER', INFANTRY);
    const iii = corps(g, 'GER', 'III Corps')!;
    iii.xp = 0.8;
    const fresh = (ROWS.find((r) => r[1] === 'GER' && r[2] === INFANTRY)![6].xp as number);
    month(g, 'GER', 19);
    const row = corps(g, 'GER', INFANTRY)!;
    expect(names(g, iii)).toEqual(['218th Infantry Division', '207th Infantry Division']);
    expect(iii.xp).toBe(0.8);
    expect(row.divs).toBe(3);
    expect(names(g, row)[0]).toBe('50th Infantry Division');
    expect(row.xp).toBeCloseTo((0.8 + 2 * fresh) / 3, 9);
    expect(holders(g, 'GER', '50th Infantry Division')).toHaveLength(1);
  });

  it('XV Corps gives the 8th and keeps the 7th; IV Corps gives the 4th the 14th was formed of, which takes the row\'s own number', () => {
    const g = balkans();
    only(g, 'GER', EIGHTH);
    month(g, 'GER', 20);
    const row = corps(g, 'GER', EIGHTH)!;
    expect(names(g, corps(g, 'GER', 'XV Corps'))).toEqual(['7th Panzer Division']);
    expect(names(g, corps(g, 'GER', 'IV Corps'))).toEqual(['46th Infantry Division', '27th Infantry Division']);
    expect(row.divs).toBe(2);
    expect(names(g, row)[0]).toBe('8th Panzer Division');
    expect(holders(g, 'GER', '8th Panzer Division')).toHaveLength(1);
    expect(holders(g, 'GER', '4th Infantry Division')).toHaveLength(0);
  });

  it('a holder that is gone gives nothing: the row raises its divisions as before', () => {
    const g = balkans();
    only(g, 'GER', PANZERS);
    removeCorps(g, corps(g, 'GER', 'XXII Corps')!);
    month(g, 'GER', 19);
    const row = corps(g, 'GER', PANZERS)!;
    expect(row.divs).toBe(2);
    expect(row.str).toBe(1);
  });

  it('a holder in a fight keeps its division, and the row forms without it', () => {
    // (Germany's first wars go on: Neumark, where III Corps stands, lies beside Poznan.)
    const g = balkans(null, true, false);
    only(g, 'GER', INFANTRY);
    month(g, 'GER', 19);
    expect(corps(g, 'GER', 'III Corps')!.divs).toBe(3);
    expect(corps(g, 'GER', INFANTRY)!.divs).toBe(2);
    expect(holders(g, 'GER', '50th Infantry Division')).toHaveLength(1);
  });

  it('the player\'s own corps never moves for him: the row forms without it, and he is told', () => {
    const g = balkans('GER');
    only(g, 'GER', PANZERS);
    const xxii = corps(g, 'GER', 'XXII Corps')!;
    month(g, 'GER', 19);
    expect(xxii.dead).toBeFalsy();
    expect(xxii.divs).toBe(2);
    expect(corps(g, 'GER', PANZERS)).toBeUndefined();
    expect(g.headlines.some((h) => /No division is raised twice/.test(h.text) && /XXII Corps/.test(h.text))).toBe(true);
  });

  it('history\'s campaigns keep the order of battle they were measured with', () => {
    const g = balkans(null, false);
    only(g, 'GER', PANZERS);
    month(g, 'GER', 19);
    expect(corps(g, 'GER', 'XXII Corps')!.divs).toBe(2);
    expect(corps(g, 'GER', PANZERS)!.divs).toBe(2);
  });
});

describe('at a loan\'s end only what the row raised new leaves', () => {
  it('the 2nd and 9th Panzer, their corps gone, stand on where they are, free', () => {
    const g = balkans();
    only(g, 'GER', PANZERS);
    month(g, 'GER', 19);
    const row = corps(g, 'GER', PANZERS)!;
    month(g, 'GER', 22);
    expect(row.dead).toBeFalsy();
    expect(row.divs).toBe(2);
    expect(row.hold).toBe(false);
    expect(names(g, row)).toEqual(['2nd Panzer Division', '9th Panzer Division']);
  });

  it('the 8th goes back to XV Corps where they stand together; the 14th, its IV Corps apart, stands on', () => {
    const g = balkans();
    only(g, 'GER', EIGHTH);
    month(g, 'GER', 20);
    const row = corps(g, 'GER', EIGHTH)!, xv = corps(g, 'GER', 'XV Corps')!;
    relocate(g, xv, g.regions[row.reg]);
    month(g, 'GER', 22);
    expect(names(g, xv)).toEqual(['7th Panzer Division', '8th Panzer Division']);
    expect(row.dead).toBeFalsy();
    expect(row.divs).toBe(1);
    expect(holders(g, 'GER', '8th Panzer Division')).toHaveLength(1);
  });

  it('a division the row raised new leaves; the 8th, its corps apart, stands on; the save keeps what was drawn', () => {
    const g0 = balkans();
    only(g0, 'GER', EIGHTH);
    removeCorps(g0, corps(g0, 'GER', 'IV Corps')!);
    month(g0, 'GER', 20);
    const g = loadGame(saveGame(g0))!;
    const row = corps(g, 'GER', EIGHTH)!;
    expect(row.divs).toBe(2);
    month(g, 'GER', 22);
    expect(row.dead).toBeFalsy();
    expect(names(g, row)).toEqual(['8th Panzer Division']);
    expect(names(g, corps(g, 'GER', 'XV Corps'))).toEqual(['7th Panzer Division']);
  });
});

describe('Britain\'s rows', () => {
  function desert(): Game {
    const g = startOpen(createGame(1, null));
    declareWar(g, 'UK', 'ITA');
    return g;
  }

  it('the 1st Armoured comes out of the corps at home that holds it: none is raised again', () => {
    const g = desert();
    only(g, 'UK', '1st Armoured Division');
    const home = addCorps(g, 'UK', '1st Armoured Division', 'arm', 1, 'London', {});
    const no = divisionsOf(g, home)[0].name;
    month(g, 'UK', 32);
    expect(home.dead).toBe(true);
    const row = corps(g, 'UK', '1st Armoured Division')!;
    expect(g.regions[row.reg].name).toBe('Alexandria');
    expect(names(g, row)).toEqual([no]);
  });

  it('the 1st South African, in the desert already, stays where it stands: the row brings the 50th alone', () => {
    const g = desert();
    only(g, 'UK', '1st South African and 50th Divisions');
    const sa = addCorps(g, 'UK', '1st South African Division', 'inf', 1, 'Mersa Matruh', {});
    month(g, 'UK', 31);
    expect(sa.dead).toBeFalsy();
    expect(sa.divs).toBe(1);
    expect(corps(g, 'UK', '1st South African and 50th Divisions')!.divs).toBe(1);
  });
});
