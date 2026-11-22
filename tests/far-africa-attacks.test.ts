// Britain's takings in East Africa as dated attacks at the stops that keep the book (the design director's 14:17Z and
// 14:36Z of 22 November 2026: China's form, one map; data/campaign/far-africa.json, written from the historian's
// iea-sahara-1940-41.md; src/campaign/stations.ts `rowNat`).
//
// With the line laid and no attacks, Britain's staff came into each province when the line opened it and by whatever
// corps it chose, or the line handed the empty ground back a month late. The record has each place taken in its month
// by a force with a name. So Britain has a choice of its own, 'eastafrica', dated January 1941 (month 17), and each
// province Italy holds is a dated attack of that choice's in the province's month on the line, by the rows the record
// has taking it. A dated attack that names its own choice is that country's row: as built every row of the table was
// its one invader's, Japan's, and a row of Britain's could be nobody's.
//
// The data is read as laid; the mechanism is read on boards set by hand; two wars are walked.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { holds } from '../src/campaign/diplomacy';
import { addCorps, atWar, createGame, declareWar, startPhase } from '../src/campaign/game';
import { HISTORY, bookIf, leash, setStop, theatreOf } from '../src/campaign/history';
import { lines } from '../src/campaign/line';
import { ops, startOpen } from '../src/campaign/staff';
import { bookAttacks, stations, type Attack } from '../src/campaign/stations';
import { stepAI } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';
import africa from '../data/campaign/far-africa.json';

type Row = [number, string, string, string, number, string, Record<string, any>];
const ROWS = (africa as any)['setup-1939'].expeditions as Row[];
const BOOK = 'UK:eastafrica';
const ITALYS = 'ITA:eastafrica';
const KF = 'Kassala force (the Eritrea command\'s two colonial brigades and cavalry)';
const GF = 'Gallabat force (a colonial battalion of the Amhara command)';
const NF = 'Nasi\'s force (the Eastern sector: five colonial brigades, three Blackshirt battalions and bande)';
const SDF = 'Sudan Defence Force (Kassala: two motor machine-gun companies and a mounted company)';
const italys = (): Attack[] => stations()!.attacks!.filter((r) => r.book === ITALYS);
const I5 = '5th Indian Division (the Sudan and Eritrea)';
const mine = (): Attack[] => stations()!.attacks!.filter((r) => r.book === BOOK);
const board = (stop: number, turn: number, human: string | null = null): Game => {
  const g = startOpen(createGame(3, human));
  setStop(g, stop, true);
  g.turn = turn;
  return g;
};
/** Kassala Italy's and empty, Britain and Italy at war, the 5th Indian at Khartoum next door, the choice gone in 17. */
const kassala = (stop: number, turn: number, human: string | null = null): { g: Game; c: Corps } => {
  const g = board(stop, turn, human);
  if (!atWar(g, 'ITA', 'UK')) declareWar(g, 'ITA', 'UK');
  const K = g.byName['Kassala'];
  for (const u of K.units) u.dead = true;
  K.units = [];
  K.ctrl = 'ITA';
  g.flags['st:done:UK:eastafrica'] = 17;
  const c = addCorps(g, 'UK', I5, 'inf', 1, 'Khartoum', { xp: 0.3 });
  g.supplyDirty = true;
  return { g, c };
};

describe('Britain\'s takings in East Africa: the data (the design director, 14:17Z and 14:36Z of 22 November 2026)', () => {
  it('Britain has a choice of its own, dated January 1941, in East Africa\'s theatre: a side choice with no chance of its own', () => {
    expect(HISTORY.dates['UK']?.['eastafrica']).toBe(17);
    expect(theatreOf('UK', 'eastafrica')).toBe('eastafrica');
    const op = ops('UK').find((o) => o.id === 'eastafrica')!;
    expect(op).toBeTruthy();
    expect([op.side, op.chance, op.from, op.to, op.ratio, op.declare, op.landings]).toEqual([true, 0, 17, 27, undefined, undefined, undefined]);
    // (By the book it waits on the war, on Italy, and on Italy being the computer's: never against a player's Italy.)
    expect(bookIf('UK', 'eastafrica')).toEqual(['war:UK,ITA', 'alive:ITA', 'computer:ITA']);
    expect(op.if).toEqual(bookIf('UK', 'eastafrica'));
    // (Japan's choices and their dates are as they were.)
    expect(HISTORY.dates['JAP']?.['south']).toBe(28);
    expect(HISTORY.dates['JAP']?.['ichigo']).toBe(56);
  });

  it('thirty-six rows, each in its place\'s month on the line, from ground next to it, by rows of Britain\'s that stand that month', () => {
    const g = startOpen(createGame(3, null));
    const A = mine();
    const L = lines().find((x) => x.id === 'eastafrica')!;
    expect(A.length).toBe(36);
    const named = new Set<string>();
    for (const r of A) {
      expect(r.at.length, r.name).toBe(1);
      const at = g.byName[r.at[0]];
      expect(at, r.name).toBeTruthy();
      expect(named.has(at.name), r.name).toBe(false);
      named.add(at.name);
      // (Its month is the month the line opens the place to Britain: Italy's last there.)
      expect(L.back![at.name], r.name).toBe(r.month);
      expect([r.met, r.until, r.ground, r.works], r.name).toEqual([undefined, undefined, undefined, undefined]);
      expect(r.from!.length, r.name).toBeGreaterThan(0);
      for (const nm of r.from!) {
        const d = g.byName[nm];
        expect(d, `${r.name} from ${nm}`).toBeTruthy();
        if (r.sea) expect(d.port, `${r.name} from ${nm}`).toBe(true);
        else expect(at.nb, `${r.name} from ${nm}`).toContain(d.id);
      }
      for (const nm of r.force) {
        const rows = ROWS.filter((x) => x[1] === 'UK' && x[2] === nm);
        expect(rows.length, `${r.name}: ${nm}`).toBe(1);
        const [from, , , , , , words] = rows[0];
        expect(from, `${r.name}: ${nm} is not yet come`).toBeLessThanOrEqual(r.month);
        // (A row is gone in the month written as its last.)
        if (words.until !== undefined) expect(words.until, `${r.name}: ${nm} is gone`).toBeGreaterThan(r.month);
      }
    }
    // Every province of Italy's on the line has its row but three: the two the pages give no force for, and Debra
    // Tabor, which surrendered to forces the Patriots led. Of the gains, the three the record has retaken by a force.
    const own = Object.entries(L.spans).filter(([, s]) => s[0].kind === 'own').map(([n]) => n);
    expect(own.filter((n) => !named.has(n)).sort()).toEqual(['Bender Cassim', 'Debra Tabor', 'Obbia']);
    const gains = Object.entries(L.spans).filter(([, s]) => s[0].kind === 'gain').map(([n]) => n);
    expect(gains.filter((n) => named.has(n)).sort()).toEqual(['Berbera', 'Hargeisa', 'Kassala']);
    expect(A.filter((r) => r.sea).map((r) => [r.at[0], r.from])).toEqual([['Berbera', ['Aden']], ['Assab', ['Aden']]]);
    expect([...new Set(A.map((r) => r.month))]).toEqual([17, 18, 19, 20, 21, 22, 23, 27]);
  });

  it('the table is one: Italy\'s rows and then Britain\'s are laid after Japan\'s, and every other row is Japan\'s as it was', () => {
    const A = stations()!.attacks!;
    const first = A.findIndex((r) => r.book === BOOK), italy = A.findIndex((r) => r.book === ITALYS);
    expect(A.slice(first).every((r) => r.book === BOOK)).toBe(true);
    expect(A.slice(italy, first).every((r) => r.book === ITALYS)).toBe(true);
    expect(first - italy).toBe(5);
    expect(A.slice(0, italy).every((r) => r.book === undefined || r.book.startsWith('JAP:'))).toBe(true);
    expect(A.slice(0, italy).length).toBe(51);
    expect(stations()!.taker).toBe('JAP');
  });
});

describe('Britain\'s takings in East Africa: a dated attack that names its own choice is that country\'s row', () => {
  it('the dial: dated in 17 at the book\'s first stop, within a month of it at the second, the staff\'s own at the others', () => {
    expect(leash(board(1, 10), 'UK', 'eastafrica')).toMatchObject({ kind: 'dated', date: 17 });
    const second = leash(board(2, 10), 'UK', 'eastafrica');
    expect(second.kind).toBe('dated');
    expect([16, 17, 18]).toContain(second.date);
    for (const stop of [3, 4, 5]) expect(leash(board(stop, 10), 'UK', 'eastafrica').kind, `stop ${stop}`).toBe('free');
    // (With the book closed in East Africa it is the staff's own at the first stop too.)
    const closed = board(1, 10);
    closed.flags['hist:off:eastafrica'] = 9;
    expect(leash(closed, 'UK', 'eastafrica').kind).toBe('free');
    // (Never against a player's Italy: the book's own condition.)
    expect(holds(board(1, 17), 'computer:ITA')).toBe(true);
    expect(holds(board(1, 17, 'GER'), 'computer:ITA')).toBe(true);
    expect(holds(board(1, 17, 'ITA'), 'computer:ITA')).toBe(false);
  });

  it('Kassala in 17: with the choice gone the 5th Indian walks in from Khartoum, and not a month before', () => {
    const { g, c } = kassala(1, 17);
    bookAttacks(g, 'UK');
    expect(g.byName['Kassala'].ctrl).toBe('UK');
    expect(g.regions[c.reg].name).toBe('Kassala');
    const early = kassala(1, 16);
    bookAttacks(early.g, 'UK');
    expect(early.g.byName['Kassala'].ctrl).toBe('ITA');
    expect(early.g.regions[early.c.reg].name).toBe('Khartoum');
  });

  it('it waits on its choice, on a stop that keeps the book, and on the book in East Africa', () => {
    const a = kassala(1, 17);
    delete a.g.flags['st:done:UK:eastafrica'];
    bookAttacks(a.g, 'UK');
    expect(a.g.byName['Kassala'].ctrl).toBe('ITA');
    for (const stop of [3, 4, 5]) {
      const b = kassala(stop, 17);
      bookAttacks(b.g, 'UK');
      expect(b.g.byName['Kassala'].ctrl, `stop ${stop}`).toBe('ITA');
    }
    const c = kassala(1, 17);
    c.g.flags['hist:off:eastafrica'] = 16;
    bookAttacks(c.g, 'UK');
    expect(c.g.byName['Kassala'].ctrl).toBe('ITA');
  });

  it('never a player\'s row, and never against ground a player holds', () => {
    const uk = kassala(1, 17, 'UK');
    bookAttacks(uk.g, 'UK');
    expect(uk.g.byName['Kassala'].ctrl).toBe('ITA');
    const ita = kassala(1, 17, 'ITA');
    bookAttacks(ita.g, 'UK');
    expect(ita.g.byName['Kassala'].ctrl).toBe('ITA');
    expect(ita.g.regions[ita.c.reg].name).toBe('Khartoum');
    // (With the player a third country it is computer against computer there, and the row is asked.)
    const ger = kassala(1, 17, 'GER');
    bookAttacks(ger.g, 'UK');
    expect(ger.g.byName['Kassala'].ctrl).toBe('UK');
  });

  it('each country its own rows: Britain\'s asking marks none of Japan\'s, Italy\'s and Japan\'s none of Britain\'s', () => {
    const { g } = kassala(1, 17);
    const A = stations()!.attacks!;
    const marks = (): number[] => A.map((_, i) => i).filter((i) => g.flags[`ba:${i}`] || g.flags[`bat:${i}`] || g.flags[`bt:${i}`]);
    bookAttacks(g, 'ITA');
    bookAttacks(g, 'JAP');
    expect(marks()).toEqual([]);
    expect(g.byName['Kassala'].ctrl).toBe('ITA');
    bookAttacks(g, 'UK');
    expect(g.byName['Kassala'].ctrl).toBe('UK');
    expect(marks().every((i) => A[i].book === BOOK)).toBe(true);
    expect(marks().length).toBeGreaterThan(0);
  });
});

describe('Italy\'s gains in East Africa as its own dated attacks (the design director, 14:36Z, seam c, and 18:08Z of 22 November 2026)', () => {
  /** Italy's month by hand at a stop: the garrisons and Italy's rows come at their places, the war open, the choice gone in 11. */
  const gains = (stop: number, turn: number, human: string | null = null): Game => {
    const g = board(stop, turn, human);
    if (!atWar(g, 'ITA', 'UK')) declareWar(g, 'ITA', 'UK');
    startPhase(g, 'UK');
    startPhase(g, 'ITA');
    g.flags['st:done:ITA:eastafrica'] = 11;
    g.supplyDirty = true;
    return g;
  };
  const alive = (g: Game, name: string): Corps | undefined => g.corps.find((c) => !c.dead && c.name === name);
  const at = (g: Game, name: string): string | undefined => { const c = alive(g, name); return c ? g.regions[c.reg].name : undefined; };

  it('Italy has a choice of its own, dated July 1940: a side choice with no chance of its own, never against a player\'s Britain', () => {
    expect(HISTORY.dates['ITA']?.['eastafrica']).toBe(11);
    expect(theatreOf('ITA', 'eastafrica')).toBe('eastafrica');
    const op = ops('ITA').find((o) => o.id === 'eastafrica')!;
    expect(op).toBeTruthy();
    expect([op.side, op.chance, op.from, op.to, op.ratio, op.declare, op.landings]).toEqual([true, 0, 11, 16, undefined, undefined, undefined]);
    expect(bookIf('ITA', 'eastafrica')).toEqual(['war:ITA,UK', 'alive:UK', 'computer:UK']);
    expect(op.if).toEqual(bookIf('ITA', 'eastafrica'));
    expect(leash(board(1, 10), 'ITA', 'eastafrica')).toMatchObject({ kind: 'dated', date: 11 });
    for (const stop of [3, 4, 5]) expect(leash(board(stop, 10), 'ITA', 'eastafrica').kind, `stop ${stop}`).toBe('free');
    expect(holds(board(1, 11, 'UK'), 'computer:UK')).toBe(false);
    expect(holds(board(1, 11, 'GER'), 'computer:UK')).toBe(true);
  });

  it('five rows, each at a gain in the gain\'s first month, from ground next to it, by a force of Italy\'s staged at its door', () => {
    const g = startOpen(createGame(3, null));
    const A = italys();
    const L = lines().find((x) => x.id === 'eastafrica')!;
    expect(A.map((r) => [r.month, r.at[0], r.force[0], r.from])).toEqual([
      [11, 'Kassala', KF, ['Keren']], [11, 'Gallabat', GF, ['Gondar']],
      [12, 'Hargeisa', NF, ['Harar']], [12, 'Berbera', NF, ['Hargeisa']], [12, 'Burao', NF, ['Hargeisa', 'Berbera']],
    ]);
    for (const r of A) {
      expect([r.at.length, r.force.length, r.sea, r.met, r.until, r.ground, r.works], r.name).toEqual([1, 1, undefined, undefined, undefined, undefined, undefined]);
      const to = g.byName[r.at[0]];
      // (Its month is the first of the gain's on the line, and the gain ends in the month Britain's row comes back.)
      expect(L.spans[to.name][0], r.name).toMatchObject({ from: r.month, kind: 'gain' });
      for (const nm of r.from!) expect(to.nb, `${r.name} from ${nm}`).toContain(g.byName[nm].id);
      const rows = ROWS.filter((x) => x[1] === 'ITA' && x[2] === r.force[0]);
      expect(rows.length, r.name).toBe(1);
      expect(rows[0][0], r.name).toBeLessThanOrEqual(r.month);
      expect(rows[0][6].until, r.name).toBeGreaterThan(r.month);
    }
    // (Each force is staged at the door of its first row: a row is posted where the record staged it, never on a gain.)
    expect([KF, GF, NF].map((nm) => ROWS.find((x) => x[2] === nm)![5])).toEqual(['Keren', 'Gondar', 'Harar']);
    // (Kurmuk is no dated attack: the pages give its battalion no command.)
    expect(Object.entries(L.spans).filter(([, s]) => s[0].kind === 'gain').map(([n]) => n).filter((n) => !A.some((r) => r.at[0] === n))).toEqual(['Kurmuk']);
  });

  it('Kassala and Gallabat in July 1940: the Kassala force fights the Defence Force\'s companies at the rows\' own sizes, the battalion walks into Gallabat, and neither a month before', () => {
    const g = gains(1, 11);
    const kf = alive(g, KF)!, sdf = alive(g, SDF)!;
    expect([at(g, KF), at(g, GF), at(g, SDF)]).toEqual(['Keren', 'Gondar', 'Kassala']);
    expect([kf.divs * kf.str, sdf.divs * sdf.str].map((x) => Math.round(x * 100) / 100)).toEqual([0.4, 0.02]);
    bookAttacks(g, 'ITA');
    expect([g.byName['Kassala'].ctrl, g.byName['Gallabat'].ctrl]).toEqual(['ITA', 'ITA']);
    expect([at(g, KF), at(g, GF)]).toEqual(['Kassala', 'Gallabat']);
    // (The companies no longer stand at Kassala: the record's withdrew.)
    expect(at(g, SDF)).not.toBe('Kassala');
    const early = gains(1, 10);
    bookAttacks(early, 'ITA');
    expect([early.byName['Kassala'].ctrl, early.byName['Gallabat'].ctrl]).toEqual(['UK', 'UK']);
    expect([at(early, KF), at(early, GF)]).toEqual(['Keren', 'Gondar']);
  });

  it('British Somaliland in August 1940: Nasi\'s force goes from Harar by Hargeisa to Berbera, and Burao with them', () => {
    const g = gains(1, 12);
    expect(at(g, NF)).toBe('Harar');
    const nf = alive(g, NF)!;
    expect(nf.divs * nf.str).toBeCloseTo(1.95, 2);
    bookAttacks(g, 'ITA');
    expect(['Hargeisa', 'Berbera', 'Burao'].map((nm) => g.byName[nm].ctrl)).toEqual(['ITA', 'ITA', 'ITA']);
    expect(['Hargeisa', 'Berbera', 'Burao']).toContain(at(g, NF));
  });

  it('it waits on its choice and on a stop that keeps the book, and is never a player\'s row nor asked against ground a player holds', () => {
    const a = gains(1, 11);
    delete a.flags['st:done:ITA:eastafrica'];
    bookAttacks(a, 'ITA');
    expect(a.byName['Kassala'].ctrl).toBe('UK');
    for (const stop of [3, 4, 5]) {
      const b = gains(stop, 11);
      bookAttacks(b, 'ITA');
      expect(b.byName['Kassala'].ctrl, `stop ${stop}`).toBe('UK');
    }
    const ita = gains(1, 11, 'ITA');
    bookAttacks(ita, 'ITA');
    expect(ita.byName['Kassala'].ctrl).toBe('UK');
    const uk = gains(1, 11, 'UK');
    bookAttacks(uk, 'ITA');
    expect(uk.byName['Kassala'].ctrl).toBe('UK');
    expect(at(uk, KF)).toBe('Keren');
    // (Britain's asking makes none of Italy's rows.)
    const c = gains(1, 11);
    bookAttacks(c, 'UK');
    bookAttacks(c, 'JAP');
    expect(c.byName['Kassala'].ctrl).toBe('UK');
  });
});

describe('Britain\'s takings in East Africa: two wars walked', () => {
  it('the book\'s first stop: Italy\'s choice goes in July 1940 and its five gains are made in their months, its rows keeping their stations; Britain\'s goes in January 1941, and Kassala, Kismayu, Mogadishu, Berbera, Keren and Harar are Britain\'s in theirs', () => {
    const g = startOpen(createGame(3, null));
    setStop(g, 1, true);
    const L = lines().find((x) => x.id === 'eastafrica')!;
    const first: Record<string, number> = {}, gained: Record<string, number> = {};
    // (Keren with them: before Italy's rows kept their stations, line.ts `lineKeeps`, a command row its staff had let
    // go stood there in March 1941 and held the 4th and 5th Indian Divisions a month past the record.)
    const watch = ['Kassala', 'Kismayu', 'Mogadishu', 'Berbera', 'Hargeisa', 'Keren', 'Harar'];
    const italy = ROWS.filter((r) => r[1] === 'ITA' && !['Kufra', 'Murzuk', 'Ghat'].includes(r[5]));
    const went: Record<string, string[]> = { [KF]: ['Kassala'], [GF]: ['Gallabat'], [NF]: ['Hargeisa', 'Berbera', 'Burao'] };
    const gains = ['Kassala', 'Gallabat', 'Hargeisa', 'Berbera', 'Burao'];
    while (g.turn < 20) {
      const t = g.turn;
      stepAI(g);
      if (g.turn === t) continue;
      // (As each month ends.)
      for (const nm of gains) if (gained[nm] === undefined && g.byName[nm].ctrl === 'ITA') gained[nm] = t;
      // Through 1940 each of Italy's rows stands held on its station: its own place, or a force on a gain its dated attack took.
      if (t >= 10 && t <= 16) for (const r of italy) {
        const c = g.corps.find((x) => !x.dead && x.nat === 'ITA' && x.name === r[2]);
        if (!c) continue;
        expect(c.hold, `${r[2]} in month ${t}`).toBe(true);
        expect([r[5], ...(went[r[2]] ?? [])], `${r[2]} in month ${t}`).toContain(g.regions[c.reg].name);
      }
      for (const nm of watch) if (first[nm] === undefined && g.byName[nm].ctrl === 'UK' && (!gains.includes(nm) || gained[nm] !== undefined)) first[nm] = t;
      // Britain holds none of Italy's own ground before the province's month.
      for (const [nm, s] of Object.entries(L.spans)) if (s[0].kind === 'own' && g.byName[nm].ctrl === 'UK' && nm !== 'Gambela') expect(t, `${nm} in month ${t}`).toBeGreaterThanOrEqual(L.back![nm]);
    }
    expect(g.flags['st:done:ITA:eastafrica']).toBe(11);
    expect(gained).toEqual({ Kassala: 11, Gallabat: 11, Hargeisa: 12, Berbera: 12, Burao: 12 });
    expect(g.flags['st:done:UK:eastafrica']).toBe(17);
    expect(first).toEqual({ Kassala: 17, Kismayu: 18, Mogadishu: 18, Berbera: 19, Hargeisa: 19, Keren: 19, Harar: 19 });
  }, 600000);

  it('a stop that keeps no book: neither choice is ever taken, and no row of Britain\'s or Italy\'s is marked', () => {
    const g = startOpen(createGame(3, null));
    setStop(g, 3, true);
    while (g.turn < 19) stepAI(g);
    expect(g.flags['st:done:UK:eastafrica']).toBeUndefined();
    const A = stations()!.attacks!;
    expect(g.flags['st:done:ITA:eastafrica']).toBeUndefined();
    expect(A.map((_, i) => i).filter((i) => (A[i].book === BOOK || A[i].book === ITALYS) && (g.flags[`ba:${i}`] || g.flags[`bt:${i}`]))).toEqual([]);
  }, 600000);
});
