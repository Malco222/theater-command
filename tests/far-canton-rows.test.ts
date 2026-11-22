// Canton's rows as the record had them (the design director's rulings of 07:52Z, 07:55Z, 08:10Z and 08:48Z of
// 22 November 2026 on the second rules builder's Nanning trace; the historian's canton-rows-1939-45.md).
//
// Data only, behind the far switch, in data/campaign/china-setup.json. The chart's '21st Army (Canton)' is the 104th
// Division: it keeps its name and stands at 1.0, the chart's full division, as the 18th Division does at Tungkun. The
// 104th's fourth regiment, the 170th, is a row of its own (0.3) that stands at Canton at every stop until Japan's
// demand on Indochina, when Hanoi's post is made from it (pacific.json, 'posts'): its men are counted once. The 38th
// Division ends at the book's stops in February 1942, as its regiments' rows sail.
// The 51st Division stands at Canton from September 1941, one row at every stop: at the book's stops the book stands
// it at Rabaul from November 1942 (a station of the book's later months, china-line.json; the design director's
// 08:48Z, 2), and at the others it is its staff's. The 19th and 22nd Independent Mixed Brigades come in January 1942,
// and the 129th and 130th Divisions in April 1945, the 130th raised from the 19th brigade.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, capitulate, createGame, declareWar, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { demand, pacificOpening } from '../src/campaign/pacific';
import { startOpen } from '../src/campaign/staff';
import { bookStations, stations, stationsBind } from '../src/campaign/stations';
import type { Corps, Game } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';
import world from '../data/campaign/world.json';
import chinaSetup from '../data/campaign/china-setup.json';

const MINE = (chinaSetup as any)['setup-1939'].expeditions as any[][];
const AMEND = (chinaSetup as any).rows.amend as { file: string; list: string; nat: string; name: string; src: string; set?: any }[];
const first = (name: string): any[] => ((world as any).corps as any[]).find((r) => r[1] === name)!;
const dated = (name: string): any[][] => ((setup as any).expeditions as any[][]).filter((r) => r[2] === name);
const war = (stop: number): Game => { const g = startOpen(createGame(5, null)); if (stop !== 3) setStop(g, stop, true); return g; };
const of = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead);
const at = (g: Game, name: string): string | undefined => { const c = of(g, name); return c ? g.regions[c.reg].name : undefined; };
/** Canton with Tungkun, the province between it and Hong Kong: a row formed where Canton is full is put down on the nearest ground with room, the game's rule for every dated row. */
const AREA = ['Canton', 'Tungkun'];
/** Japan's month opens, for each month given in turn: its dated rows come and go. */
const months = (g: Game, ...ts: number[]): Game => { for (const t of ts) { g.turn = t; startPhase(g, 'JAP'); } return g; };

const R104 = '21st Army (Canton)', R18 = '21st Army (18th Division)', R38 = '38th Division (Canton)';
const R170 = '21st Army (170th Infantry Regiment, of the 104th Division)', R51 = '23rd Army (51st Division)';
const B19 = '23rd Army (19th Independent Mixed Brigade)', B22 = '23rd Army (22nd Independent Mixed Brigade)';
const D129 = '23rd Army (129th Division)', D130 = '23rd Army (130th Division)';
const CANTON = [R170, R51, B19, B22, D129, D130];

describe("Canton's rows in the China file", () => {
  it('the 104th and the 18th are amended to 1.0, the chart\'s full division, and the 38th to end in month 30', () => {
    const line = (name: string) => AMEND.find((a) => a.name === name)!;
    expect(line(R104)).toEqual({ file: 'world', list: 'corps', nat: 'JAP', name: R104, src: 'far-world', set: { str: 1 } });
    expect(line(R18)).toEqual({ file: 'world', list: 'corps', nat: 'JAP', name: R18, src: 'far-world', set: { str: 1 } });
    expect(line(R38)).toEqual({ file: 'setup-1939', list: 'expeditions', nat: 'JAP', name: R38, src: 'far-world', set: { until: 30 } });
    expect(first(R104).slice(0, 5)).toEqual(['JAP', R104, 'inf', 1, 'Canton']);
    expect(first(R104)[5].str).toBe(1);
    expect(first(R18).slice(0, 5)).toEqual(['JAP', R18, 'inf', 1, 'Tungkun']);
    expect(first(R18)[5].str).toBe(1);
    // (The 38th stands at the book's stops alone, far-shores.json; both amendments lie on the one row.)
    expect(dated(R38)).toHaveLength(1);
    expect([dated(R38)[0][0], dated(R38)[0][5], dated(R38)[0][6].stops, dated(R38)[0][6].until]).toEqual([2, 'Canton', [1, 2], 30]);
  });

  it('the new rows: six lines for six formations, each at every stop with no end written, no row above a division\'s worth, each at Canton', () => {
    const rows = MINE.filter((r) => CANTON.includes(r[2]));
    expect(rows.map((r) => [r[0], r[2], r[4], r[5], r[6].str, r[6].stops ?? 'every stop', r[6].until ?? null])).toEqual([
      [1, R170, 1, 'Canton', 0.3, 'every stop', null],
      [25, R51, 1, 'Canton', 1, 'every stop', null],
      [29, B19, 1, 'Canton', 0.35, 'every stop', null], [29, B22, 1, 'Canton', 0.35, 'every stop', null],
      [68, D129, 1, 'Canton', 0.6, 'every stop', null], [68, D130, 1, 'Canton', 0.6, 'every stop', null],
    ]);
    for (const r of rows) expect(r[1] === 'JAP' && r[3] === 'inf' && r[6].str <= 1, r[2]).toBe(true);
    // (A row under a division's worth never fills past its own size.)
    for (const r of rows.filter((x) => x[6].str < 1)) expect(r[6].est, r[2]).toBe(r[6].str);
    // (One row becomes the other: the 130th is raised from the 19th brigade, and nothing else here takes a row.)
    expect(rows.filter((r) => r[6].from).map((r) => [r[2], r[6].from])).toEqual([[D130, [B19]]]);
  });
});

describe('the 104th Division, the 18th and the 170th Infantry', () => {
  it('on the first day the 104th stands at Canton and the 18th at Tungkun, each at 1.0, at every stop', () => {
    for (const stop of [1, 3]) {
      const g = war(stop);
      expect([at(g, R104), of(g, R104)!.str, of(g, R104)!.full], `stop ${stop}`).toEqual(['Canton', 1, undefined]);
      expect([at(g, R18), of(g, R18)!.str, of(g, R18)!.full], `stop ${stop}`).toEqual(['Tungkun', 1, undefined]);
    }
  });

  it('the 170th stands at Canton from the first month at 0.3, a row that never fills past it', () => {
    for (const stop of [1, 3]) {
      const g = months(war(stop), 1);
      const c = of(g, R170)!;
      expect([at(g, R170), c.str, c.full, c.hold, !!c.stay], `stop ${stop}`).toEqual(['Canton', 0.3, 0.3, false, false]);
    }
  });

  it('it stands at Canton at every stop until the demand, January 1941 and after', () => {
    for (const stop of [1, 3]) {
      const g = months(war(stop), 1, 16, 17, 22);
      expect([at(g, R170), of(g, R170)!.str], `stop ${stop}`).toEqual(['Canton', 0.3]);
    }
  });

  it("on Japan's demand Hanoi's post is made from it: the regiment is gone from Canton and its men stand at Hanoi, once", () => {
    const POST = 'Indochina Expeditionary Force (Hanoi and Gia Lam)';
    // (France beaten in June 1940, by hand, as the Indochina tests have it: far-indochina-kept.)
    const fallen = (stop: number, turn: number): Game => { const g = months(war(stop), 1); g.turn = 10; capitulate(g, 'FRA', 'GER'); g.turn = turn; return g; };
    // By the book, July 1941.
    const b = fallen(1, 22);
    expect([at(b, R170), of(b, POST)]).toEqual(['Canton', undefined]);
    b.turn = 23;
    pacificOpening(b);
    expect(b.flags['pac:demand']).toBe(23);
    expect([of(b, R170), at(b, POST), of(b, POST)!.str, of(b, POST)!.full]).toEqual([undefined, 'Hanoi', 0.3, 0.3]);
    // Where the demand comes by cause, in its own month.
    const g = fallen(3, 21);
    expect(demand(g)).toBe(true);
    expect([of(g, R170), at(g, POST), of(g, POST)!.str]).toEqual([undefined, 'Hanoi', 0.3]);
    // (The 104th itself stays.)
    for (const x of [b, g]) expect(at(x, R104)).toBe('Canton');
  });

  it("where no demand comes it stands on and is its staff's", () => {
    const g = months(war(3), 1, 17, 30);
    expect(at(g, R170)).toBe('Canton');
    expect(g.flags['pac:demand']).toBeUndefined();
  });
});

describe('the 38th Division ends as its regiments sail', () => {
  it('at the book\'s stops it comes in month 2, stands in 29 and is gone in 30', () => {
    for (const stop of [1, 2]) {
      const g = months(war(stop), 2);
      expect([at(g, R38), of(g, R38)!.str], `stop ${stop}`).toEqual(['Canton', 1]);
      months(g, 29);
      expect(of(g, R38), `stop ${stop}, month 29`).toBeDefined();
      months(g, 30);
      expect(of(g, R38), `stop ${stop}, month 30`).toBeUndefined();
    }
  });

  it('at stops 3 to 5 it comes as its three regiments and nothing ends by date', () => {
    const g = months(war(3), 2, 30, 40);
    expect(of(g, R38)).toBeUndefined();
    for (const nm of ['228th Infantry Regiment (the Ito Detachment)', "229th Infantry Regiment (with the 38th Division's headquarters)", '230th Infantry Regiment (the Shoji Detachment)']) expect(of(g, nm), nm).toBeDefined();
  });
});

describe('the 51st Division at Canton', () => {
  it('comes in September 1941, month 25, at 1.0, at every stop: one row, with no end written', () => {
    const rows = MINE.filter((r) => r[2] === R51);
    expect(rows).toHaveLength(1);
    expect([rows[0][0], rows[0][5], rows[0][6].str, rows[0][6].until ?? null, rows[0][6].stops ?? null]).toEqual([25, 'Canton', 1, null, null]);
    for (const stop of [1, 3]) {
      const g = months(war(stop), 24);
      expect(of(g, R51), `stop ${stop}, month 24`).toBeUndefined();
      months(g, 25);
      expect([at(g, R51), of(g, R51)!.str, of(g, R51)!.hold], `stop ${stop}`).toEqual(['Canton', 1, false]);
      expect(g.corps.filter((c) => c.name === R51 && !c.dead), `stop ${stop}`).toHaveLength(1);
    }
  });

  /** Japan's blow fallen by the book in December 1941, and Rabaul in its hands, as the record had both by November 1942. */
  const south = (g: Game): Game => {
    g.flags['st:done:JAP:south'] = 28;
    for (const e of ['USA', 'UK']) declareWar(g, 'JAP', e);
    const rabaul = g.byName['Rabaul'];
    rabaul.ctrl = 'JAP'; rabaul.units = [];
    return g;
  };
  /** The book's month for Japan. */
  const book = (g: Game, t: number): Game => { g.turn = t; bookStations(g, 'JAP'); return g; };

  it("the station: a row of the book's table with its own last month, Rabaul from month 39", () => {
    const row = stations()!.rows.find((r) => r.nat === 'JAP' && r.corps === R51)!;
    expect([row.steps, row.until, row.worth ?? null, row.free ?? false, row.beside ?? null]).toEqual([[[39, ['Rabaul']]], 73, null, false, null]);
    expect(war(1).byName['Rabaul']).toBeDefined();
    // None of the other new rows has a station: the regiment, the brigades and the divisions of 1945 are their staff's.
    for (const nm of [R170, B19, B22, D129, D130]) expect(stations()!.rows.some((r) => r.corps === nm), nm).toBe(false);
  });

  it("at the book's stops the book stands it at Rabaul in November 1942, month 39: the same corps, at its worth, nobody struck off", () => {
    for (const stop of [1, 2]) {
      const g = south(months(war(stop), 25));
      const c = of(g, R51)!;
      // To October 1942 it is its staff's at Canton, the table's last month past or not.
      for (const t of [35, 36, 38]) { book(g, t); expect([at(g, R51), c.hold, c.bookWorks ?? null], `stop ${stop}, month ${t}`).toEqual(['Canton', false, null]); }
      expect(stationsBind(g, 'JAP')).toBe(false);
      startPhase(book(g, 39), 'JAP');
      expect(of(g, R51), `stop ${stop}`).toBe(c);
      expect([at(g, R51), c.str, c.hold, !!c.stay], `stop ${stop}, month 39`).toEqual(['Rabaul', 1, true, true]);
      expect(g.byName['Canton'].units.includes(c)).toBe(false);
      // It stands there while its row runs, and is its staff's the month after.
      book(g, 60);
      expect([at(g, R51), c.hold], `stop ${stop}, month 60`).toEqual(['Rabaul', true]);
      book(g, 73);
      expect([at(g, R51), c.hold], `stop ${stop}, month 73`).toEqual(['Rabaul', true]);
      book(g, 74);
      expect([at(g, R51), c.hold, c.bookWorks ?? null], `stop ${stop}, month 74`).toEqual(['Rabaul', false, null]);
    }
  });

  it("with Rabaul not Japan's, or an enemy in its field, the division stays where it stands, its staff's; it goes the month Rabaul is open", () => {
    const g = south(months(war(1), 25));
    const c = of(g, R51)!;
    g.byName['Rabaul'].ctrl = 'UK';
    book(g, 39);
    expect([at(g, R51), c.hold]).toEqual(['Canton', false]);
    g.byName['Rabaul'].ctrl = 'JAP';
    const foe = addCorps(g, 'USA', 'an American corps ashore at Rabaul', 'inf', 1, 'Rabaul', { xp: 0.3 });
    book(g, 40);
    expect([at(g, R51), c.hold]).toEqual(['Canton', false]);
    g.byName['Rabaul'].units.splice(g.byName['Rabaul'].units.indexOf(foe), 1);
    book(g, 41);
    expect([at(g, R51), c.hold]).toEqual(['Rabaul', true]);
  });

  it("at stops 3 to 5 no station binds it: it stands at Canton as its staff's", () => {
    for (const stop of [3, 5]) {
      const g = south(months(war(stop), 25));
      const c = of(g, R51)!;
      for (const t of [39, 50]) { startPhase(book(g, t), 'JAP'); expect([of(g, R51), at(g, R51), c.hold], `stop ${stop}, month ${t}`).toEqual([c, 'Canton', false]); }
    }
  });
});

describe('the brigades of 1942 and the divisions of 1945', () => {
  it('the 19th and 22nd brigades come in January 1942, month 29, at 0.35 each, at every stop, neither held', () => {
    for (const stop of [1, 3]) {
      const g = months(war(stop), 28);
      expect([of(g, B19), of(g, B22)], `stop ${stop}, month 28`).toEqual([undefined, undefined]);
      months(g, 29);
      for (const nm of [B19, B22]) { const c = of(g, nm)!; expect(AREA, `${nm} at stop ${stop}`).toContain(at(g, nm)); expect([c.str, c.full, c.hold, !!c.stay], `${nm} at stop ${stop}`).toEqual([0.35, 0.35, false, false]); }
    }
  });

  it('in April 1945, month 68, the 129th comes and the 19th brigade becomes the 130th: one row, not both', () => {
    for (const stop of [1, 3]) {
      const g = months(war(stop), 29, 67);
      expect([of(g, D129), of(g, D130)], `stop ${stop}, month 67`).toEqual([undefined, undefined]);
      months(g, 68);
      for (const nm of [D129, D130]) { const c = of(g, nm)!; expect(AREA, `${nm} at stop ${stop}`).toContain(at(g, nm)); expect([c.str, c.full], `${nm} at stop ${stop}`).toEqual([0.6, 0.6]); }
      expect(of(g, B19), `stop ${stop}`).toBeUndefined();
      expect(AREA, `stop ${stop}`).toContain(at(g, B22));
    }
  });

  it('the 130th comes at its own worth where the 19th brigade has been lost', () => {
    const g = months(war(1), 29);
    const c = of(g, B19)!, r = g.regions[c.reg];
    c.dead = true; r.units.splice(r.units.indexOf(c), 1);
    months(g, 68);
    expect(AREA).toContain(at(g, D130));
    expect(of(g, D130)!.str).toBe(0.6);
  });
});
