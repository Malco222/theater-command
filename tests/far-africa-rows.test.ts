// Italian East Africa and the Sahara posts by the record (the design director's C of 11:40Z and his acceptance of
// 13:09Z, 22 November 2026, on the historian's iea-sahara-1940-41.md; data/campaign/far-africa.json).
//
// Until these rows East Africa stood empty on the far chart, and so did Britain's Sudan, Kenya and Somaliland: the
// fronts there read hundreds to one. The rows are data, laid as found behind the far switch. The file's own sums are
// read here beside the record's. One war at the book's first stop is walked to the opening of Italy's month 10, June
// 1940, to see the garrisons that stood there before it and Italy's rows come at their places at the record's size.
// What the staffs then do with them is the game's own and is read in the six, not here: the same board is moved by
// hand to August 1940 and to February 1941 to see the rows grow and end with a surrender's words on the card.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { atWar, computeSupply, createGame, declareWar, estAt, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import { current, stepAI } from '../src/campaign/turn';
import { shoreOf } from '../src/campaign/world';
import type { Corps, Game } from '../src/campaign/types';
import africa from '../data/campaign/far-africa.json';
import setup from '../data/campaign/setup-1939.json';

type Row = [number, string, string, string, number, string, Record<string, any>];
const ROWS = (africa as any)['setup-1939'].expeditions as Row[];
const SAHARA = ['Kufra', 'Murzuk', 'Ghat'];
/** The six command seats, which carry the map's depot mark (the design director's 13:43Z of 22 November 2026). */
const SEATS = ['Asmara', 'Gondar', 'Addis Ababa', 'Jimma', 'Harar', 'Mogadishu'];
const ITALY = ROWS.filter((r) => r[1] === 'ITA' && !SAHARA.includes(r[5]));
const worth = (r: Row, month: number): number => r[4] * (estAt(r[6].est, month) ?? 1);
/** (A row is counted from its own month: Nasi's force comes in August 1940.) */
const sum = (rows: Row[], month: number): number => rows.filter((r) => r[0] <= month).reduce((s, r) => s + worth(r, month), 0);
/** The three forces that are rows of their own, each taken out of its command's worth (the design director's 14:36Z and 18:08Z of 22 November 2026). */
const FORCES: Record<string, string> = { 'Eritrea command': 'Kassala force', 'Amhara command': 'Gallabat force', 'Harar command': 'Nasi\'s force' };
const force = (name: string): Row => ITALY.find((r) => r[2].startsWith(name))!;
/** A command's rows, with the force taken out of it. */
const command = (name: string): Row[] => ITALY.filter((r) => r[2].startsWith(name) || (FORCES[name] !== undefined && r[2].startsWith(FORCES[name])));

describe('Italian East Africa and the Sahara posts: the file beside the record (the design director, 13:09Z of 22 November 2026)', () => {
  it('Italy\'s army of 1 June 1940 is 17.1 by its six commands and the headquarters, and all forces from 1 August are 24.7', () => {
    expect(sum(ITALY, 10)).toBeCloseTo(17.1, 1);
    expect(sum(ITALY, 11)).toBeCloseTo(17.1, 1);
    expect(sum(ITALY, 12)).toBeCloseTo(24.7, 1);
    const at10: [string, number][] = [['East Africa headquarters', 0.5], ['Eritrea command', 2.8], ['Amhara command', 4.0], ['Shoa command', 3.6], ['Galla-Sidamo command', 2.1], ['Harar command', 2.5], ['Somalia command', 1.6]];
    for (const [name, w] of at10) expect(sum(command(name), 10), name).toBeCloseTo(w, 1);
    expect(at10.reduce((n, [name]) => n + command(name).length, 0)).toBe(ITALY.length);
  });

  it('three forces are rows of their own, each taken out of its command\'s worth: Kassala\'s 0.4, Gallabat\'s battalion, and Nasi\'s 1.95 from August 1940', () => {
    const K = 24.7 / 17.1;
    // The Kassala force: 0.4 of Eritrea's 2.8, staged at Keren; it keeps its figure when the command grows.
    const kf = force('Kassala force');
    expect([kf[0], kf[5], kf[6].hold, kf[6].until, kf[6].mark]).toEqual([10, 'Keren', 1, 19, undefined]);
    for (const m of [10, 12, 18]) expect(worth(kf, m), `Kassala force in ${m}`).toBeCloseTo(0.4, 3);
    expect(sum(ITALY.filter((r) => r[2].startsWith('Eritrea command')), 10)).toBeCloseTo(2.4, 2);
    expect(sum(command('Eritrea command'), 12)).toBeCloseTo(2.8 * K, 1);
    // The Gallabat force: a battalion of Amhara's 4.0 (D), staged at Gondar, and of Gondar's 2.7 from May 1941.
    const gf = force('Gallabat force');
    expect([gf[0], gf[5], gf[6].hold, gf[6].until, gf[6].mark]).toEqual([10, 'Gondar', 1, 27, 'D']);
    for (const m of [10, 12, 21, 26]) expect(worth(gf, m), `Gallabat force in ${m}`).toBeCloseTo(0.05, 3);
    expect(sum(ITALY.filter((r) => r[2].startsWith('Amhara command')), 10)).toBeCloseTo(3.95, 2);
    expect(sum(command('Amhara command'), 12)).toBeCloseTo(4.0 * K, 1);
    // Nasi's force: 1.95, the middle of the pages' 24,000 and 35,000, drawn in August 1940 from Harar's worth.
    const nf = force('Nasi\'s force');
    expect([nf[0], nf[5], nf[6].hold, nf[6].until, nf[6].mark]).toEqual([12, 'Harar', 1, 19, 'L']);
    expect(worth(nf, 12)).toBeCloseTo(1.95, 3);
    expect((24000 / 15000 + 35000 / 15000) / 2).toBeCloseTo(1.95, 1);
    const harar = ITALY.find((r) => r[2] === 'Harar command')!;
    expect(worth(harar, 10)).toBeCloseTo(2.5, 2);
    expect(worth(harar, 12)).toBeCloseTo(2.5 * K - 1.95, 2);
    expect(worth(harar, 12)).toBeGreaterThan(0);
    expect(sum(command('Harar command'), 12)).toBeCloseTo(2.5 * K, 1);
    // (No row is over its divisions: a row's size is never more than it can hold.)
    for (const r of ITALY) for (const m of [10, 12, 21]) expect((estAt(r[6].est, m) ?? 1), `${r[2]} in ${m}`).toBeLessThanOrEqual(1);
  });

  it('from May 1941 the field army is Gondar\'s 2.7 and Galla-Sidamo\'s 2.5, and from July Gondar alone', () => {
    const standing = (month: number): Row[] => ITALY.filter((r) => r[6].until > month);
    // (Gondar's 2.7 with the Gallabat force, a battalion of it, which is a row of its own.)
    expect(worth(command('Amhara command (Gondar)')[0], 21) + worth(force('Gallabat force'), 21)).toBeCloseTo(2.7, 2);
    expect(sum(command('Galla-Sidamo command').filter((r) => r[6].until > 21), 21)).toBeCloseTo(2.5, 2);
    expect(standing(23).map((r) => r[2])).toEqual(['Amhara command (Gondar)', force('Gallabat force')[2]]);
    expect(sum(standing(23), 23)).toBeCloseTo(2.7, 2);
    expect(standing(27)).toEqual([]);
  });

  it('each place\'s last Italian month is the record\'s, and every such row ends in a surrender', () => {
    const until = Object.fromEntries(ROWS.filter((r) => r[1] === 'ITA' && r[6].until !== undefined).map((r) => [`${r[2]} @${r[5]}`, r[6].until]));
    const last: Record<string, number> = {
      Mogadishu: 18, Kismayu: 18, 'Belet Uen': 19, Gabredarre: 19, Obbia: 19, 'Bender Cassim': 19, Neghelli: 19, Harar: 19, Keren: 19,
      Asmara: 20, 'Debra Markos': 20, 'Addis Ababa': 20, Massawa: 20, Dessie: 20, Mekele: 21, Assab: 22, Jimma: 22, Lekemti: 23, Gondar: 27, Kufra: 19,
    };
    for (const [key, month] of Object.entries(until)) expect(month, key).toBe(last[key.split(' @')[1]]);
    expect(new Set(Object.keys(until).map((k) => k.split(' @')[1]))).toEqual(new Set(Object.keys(last)));
    for (const r of ROWS.filter((x) => x[1] === 'ITA' && x[6].until !== undefined)) { expect(r[6].ends, r[2]).toBe('surrender'); expect(r[6].holds, r[2]).toEqual([r[5]]); }
    // (The Fezzan stays Italian past the file's months, and the record is silent on Sebha and Waw el Kebir.)
    for (const r of ROWS.filter((x) => ['Murzuk', 'Ghat'].includes(x[5]))) expect(r[6].until, r[2]).toBeUndefined();
    expect(ROWS.some((r) => r[5] === 'Sebha' || r[5] === 'Waw el Kebir')).toBe(false);
  });

  it('every row stands on the first day at a place of its own side\'s, where the record raised or staged it, and none on a gain of the line\'s', () => {
    const g = startOpen(createGame(3, null));
    const gains = Object.entries((africa as any).history.lines[0].spans as Record<string, { kind: string }[]>).filter(([, v]) => v[0].kind === 'gain').map(([k]) => k);
    expect(gains.length).toBe(6);
    for (const r of ROWS) {
      const at = g.byName[r[5]];
      expect(at, r[5]).toBeTruthy();
      // (All on the far chart but the 4th Indian Division's own row, which stands in Egypt again when its loan is over.)
      expect(shoreOf(at), r[2]).toBe(r[2] === '4th Indian Division' ? 0 : 2);
      expect(at.ctrl, r[2]).toBe(r[1]);
      if (r[1] !== 'ITA') expect(gains.includes(r[5]) && r[0] > 9, r[2]).toBe(false);
    }
    // (Laid over the chart's dated rows once, as one block in the file's order; no name of theirs is another row's.)
    const laid = (setup as any).expeditions as Row[];
    const first = laid.findIndex((r) => r[1] === ROWS[0][1] && r[2] === ROWS[0][2]);
    expect(first).toBeGreaterThan(0);
    expect(laid.slice(first, first + ROWS.length).map((r) => r[2])).toEqual(ROWS.map((r) => r[2]));
    const names = new Set(ROWS.map((r) => `${r[1]}:${r[2]}`));
    expect(names.size).toBe(ROWS.length);
    expect(laid.filter((r) => names.has(`${r[1]}:${r[2]}`)).length).toBe(ROWS.length);
  });

  it('the six command seats carry the depot mark, each on Italy\'s own ground, and no other province of the file does', () => {
    const g = startOpen(createGame(3, null));
    expect((africa as any).setup.depots).toEqual(SEATS);
    for (const name of SEATS) {
      const r = g.byName[name];
      expect(r.owner, name).toBe('ITA');
      expect(r.ctrl, name).toBe('ITA');
      expect(g.flags[`depot:${r.id}`], name).toBe(1);
    }
    const places = new Set(ROWS.map((r) => r[5]));
    for (const name of places) if (!SEATS.includes(name)) expect(g.flags[`depot:${g.byName[name].id}`], name).toBeUndefined();
  });

  it('the five garrisons that stood there before Italy\'s war are laid a month before it, the Defence Force at its six posts, and every other row in its own month', () => {
    const before = ROWS.filter((r) => r[0] < 10).map((r) => r[2]);
    const posts = before.filter((nm) => nm.startsWith('Sudan Defence Force ('));
    expect(posts.length).toBe(6);
    expect(before.filter((nm) => !posts.includes(nm))).toEqual([
      'British battalions in the Sudan (West Yorkshire, Essex, Worcestershire)', 'King\'s African Rifles (Kenya)',
      'Somaliland force (the Camel Corps and Chater\'s battalions)', 'Djibouti garrison (French Somaliland)',
    ]);
    for (const r of ROWS.filter((x) => x[0] < 10)) expect(r[0], r[2]).toBe(9);
    for (const r of ROWS.filter((x) => x[1] === 'ITA')) expect(r[0], r[2]).toBe(r[2].startsWith('Nasi') ? 12 : 10);
  });

  it('the Sudan Defence Force stands at its record posts: 0.02 at Kassala, and the other 0.28 shared evenly over its five seats', () => {
    const sdf = ROWS.filter((r) => r[1] === 'UK' && r[2].startsWith('Sudan Defence Force ('));
    expect(sdf.map((r) => r[5])).toEqual(['Kassala', 'El Obeid', 'El Fasher', 'Juba', 'Atbara', 'Khartoum']);
    expect(sum(sdf, 10)).toBeCloseTo(0.3, 3);
    const [kassala, ...seats] = sdf;
    // (Kassala: 320 of its men, 500 with the police, at 15,000 to 1.0.)
    expect([worth(kassala, 10), kassala[6].mark, kassala[6].hold]).toEqual([0.02, 'L', 1]);
    expect(worth(kassala, 10)).toBeCloseTo(320 / 15000, 2);
    for (const r of seats) expect([worth(r, 10), r[6].mark, r[6].hold], r[2]).toEqual([0.056, 'D', 1]);
    // (The file's 0.6 for all troops in the Sudan stands: the Defence Force and the three British battalions.)
    expect(sum(ROWS.filter((r) => r[0] === 9 && r[1] === 'UK' && ['Kassala', 'El Obeid', 'El Fasher', 'Juba', 'Atbara', 'Khartoum'].includes(r[5])), 10)).toBeCloseTo(0.6, 3);
  });
});

describe('the rows of the design director\'s 13:43Z, 14:18Z and 14:36Z of 22 November 2026: the South Africans, the West African brigades, the 4th Indian Division\'s loan', () => {
  const row = (name: string): Row => ROWS.find((r) => r[2].startsWith(name))!;
  /** A war at a stop of the dial in a month set by hand, Italy at war with Britain. */
  const board = (stop: number, turn: number): Game => {
    const g = startOpen(createGame(3, null));
    setStop(g, stop, true);
    if (!atWar(g, 'ITA', 'UK')) declareWar(g, 'ITA', 'UK');
    g.turn = turn;
    return g;
  };
  const alive = (g: Game, name: string): Corps | undefined => g.corps.find((c) => !c.dead && c.name === name);
  const size = (c: Corps): number => c.divs * (c.full ?? 1);

  it('the file: the South Africans are Britain\'s rows, 1.8 from November 1940 by brigades as the record moved them; Kenya\'s other troops are the file\'s 3.2', () => {
    // (The design director's 14:36Z: the file has them "UK (S. Africa)", the old chart's South African division is
    // Britain's row, and a dated attack is one country's fight. The 1st Brigade, which took Dessie, is a row of its own.)
    expect(ROWS.some((r) => r[1] === 'SAF')).toBe(false);
    const One = row('1st South African Brigade'), D = row('1st South African Division'), B = row('2nd South African Brigade');
    expect([One[1], One[5], One[0], One[6].until]).toEqual(['UK', 'Wajir', 12, 22]);
    // (At Marsabit from December 1940, the historian's check of the posting: section 8.7 of the file.)
    expect([D[1], D[5], D[0], D[6].until]).toEqual(['UK', 'Marsabit', 16, 21]);
    expect([B[1], B[5], B[0], B[6].until]).toEqual(['UK', 'Mombasa', 19, 22]);
    const south = (m: number): number => [One, D, B].filter((r) => r[0] <= m && r[6].until > m).reduce((n, r) => n + worth(r, m), 0);
    // (The 1st Brigade alone from August 1940; the division's 1.8 from December; from March 1941 the 2nd Brigade a
    // row of its own, staged at Mombasa; from May the headquarters and the 5th Brigade in Egypt, where the old
    // chart's own row lands the division; gone in June.)
    for (const [m, w] of [[12, 0.6], [14, 0.6], [15, 0.6], [16, 1.8], [18, 1.8], [19, 1.8], [20, 1.8], [21, 1.2], [22, 0]] as [number, number][]) expect(south(m), `month ${m}`).toBeCloseTo(w, 2);
    expect(((setup as any).expeditions as Row[]).some((r) => r[1] === 'UK' && r[2] === '1st South African Division' && r[0] === 21 && r[5] === 'Alexandria')).toBe(true);
    const N = row('23rd Nigerian Brigade'), G = row('24th Gold Coast Brigade');
    expect([N[1], N[5], N[0], N[6].until]).toEqual(['UK', 'Garissa', 17, 23]);
    expect([G[1], G[5], G[0], G[6].until]).toEqual(['UK', 'Wajir', 15, 26]);
    expect(worth(N, 17) + worth(G, 17)).toBeCloseTo(0.6, 2);
    // (The two African divisions at 0.55 each: with the King's African Rifles and the two brigades, the file's 3.2.)
    const kenya = [row('King\'s African Rifles'), row('11th African Division'), row('12th African Division'), N, G];
    expect(kenya.map((r) => r[1])).toEqual(['UK', 'UK', 'UK', 'UK', 'UK']);
    expect(sum(kenya, 17)).toBeCloseTo(3.2, 2);
    // (Rows posted where the record staged them: the design director's 14:18Z.)
    expect([row('Gideon Force')[5], row('Aden Striking Force')[5], row('Force Publique')[5]]).toEqual(['Roseires', 'Aden', 'Malakal']);
    expect(row('Chad troops')[6].hold).toBe(1);
  });

  it('the South Africans come under Britain\'s flag at the record\'s size month by month, and are gone in June 1941', () => {
    const g = board(1, 12);
    const One = row('1st South African Brigade'), D = row('1st South African Division'), B = row('2nd South African Brigade');
    startPhase(g, 'UK');
    const one = alive(g, One[2])!;
    expect(one).toBeTruthy();
    expect([one.nat, g.regions[one.reg].name]).toEqual(['UK', 'Wajir']);
    expect(size(one)).toBeCloseTo(0.6, 2);
    expect(alive(g, D[2])).toBeUndefined();
    expect(alive(g, B[2])).toBeUndefined();
    g.turn = 15;
    startPhase(g, 'UK');
    expect(alive(g, D[2])).toBeUndefined();
    g.turn = 16;
    startPhase(g, 'UK');
    const d = alive(g, D[2])!;
    expect([d.nat, g.regions[d.reg].name]).toEqual(['UK', 'Marsabit']);
    expect(size(d)).toBeCloseTo(1.2, 2);
    g.turn = 19;
    startPhase(g, 'UK');
    expect(size(alive(g, D[2])!)).toBeCloseTo(0.6, 2);
    const b = alive(g, B[2])!;
    expect([b.nat, g.regions[b.reg].name]).toEqual(['UK', 'Mombasa']);
    expect(size(b)).toBeCloseTo(0.6, 2);
    g.turn = 21;
    startPhase(g, 'UK');
    expect(alive(g, D[2])).toBeUndefined();
    expect(size(alive(g, One[2])!)).toBeCloseTo(0.6, 2);
    g.turn = 22;
    startPhase(g, 'UK');
    expect(alive(g, One[2])).toBeUndefined();
    expect(alive(g, B[2])).toBeUndefined();
  });

  it('the 4th Indian Division at the book\'s stops: taken from the desert in January 1941, lent to the Sudan until April, and in Egypt again', () => {
    const lent = '4th Indian Division (the Sudan and Eritrea)';
    for (const stop of [1, 2]) {
      const g = board(stop, 16);
      const old = alive(g, '4th Indian Division')!;
      expect(g.regions[old.reg].name, `stop ${stop}`).toBe('Mersa Matruh');
      startPhase(g, 'UK');
      expect(alive(g, lent), `stop ${stop}, December`).toBeUndefined();
      g.turn = 17;
      startPhase(g, 'UK');
      // (One division and not two: the desert's corps is the one that went.)
      expect(alive(g, '4th Indian Division'), `stop ${stop}`).toBeUndefined();
      const c = alive(g, lent)!;
      expect(c).toBeTruthy();
      expect([c.nat, c.divs, g.regions[c.reg].name]).toEqual(['UK', 1, 'Port Sudan']);
      g.turn = 20;
      const news = g.headlines.length;
      startPhase(g, 'UK');
      expect(alive(g, lent), `stop ${stop}`).toBeUndefined();
      expect(g.headlines.slice(news).some((h) => /lent to the campaign/.test(h.text) && h.text.includes(lent))).toBe(true);
      const home = alive(g, '4th Indian Division')!;
      expect(home).toBeTruthy();
      // (At Alexandria, or beside it where the board set by hand has filled the place: game.ts, roomNear.)
      const A = g.byName['Alexandria'];
      expect(home.divs).toBe(1);
      expect([A.id, ...A.nb]).toContain(home.reg);
      expect(g.corps.filter((x) => !x.dead && /4th Indian Division/.test(x.name) && x.nat === 'UK' && !/New Zealand|Eighth Army/.test(x.name)).length).toBe(1);
    }
  });

  it('at the stops that keep no book the 4th Indian Division is its staff\'s own: no loan, and the desert\'s corps stands', () => {
    for (const stop of [3, 5]) {
      const g = board(stop, 17);
      startPhase(g, 'UK');
      g.turn = 20;
      startPhase(g, 'UK');
      expect(alive(g, '4th Indian Division (the Sudan and Eritrea)'), `stop ${stop}`).toBeUndefined();
      const old = g.corps.filter((x) => !x.dead && x.name === '4th Indian Division');
      expect(old.length, `stop ${stop}`).toBe(1);
      expect(g.regions[old[0].reg].name, `stop ${stop}`).toBe('Mersa Matruh');
    }
  });

  it('Chad\'s row comes held to Chad', () => {
    const g = board(1, 12);
    startPhase(g, 'FRA');
    const c = alive(g, 'Chad troops (Leclerc)')!;
    expect(c).toBeTruthy();
    expect([g.regions[c.reg].name, c.hold]).toEqual(['Fort-Lamy', true]);
  });
});

describe('Italian East Africa by the record: one war at the book\'s first stop, at the opening of Italy\'s June 1940', () => {
  it('the garrisons stand, Italy\'s rows come at their places at the record\'s size; by hand to August they grow, and to February 1941 Mogadishu\'s and Kismayu\'s lay down their arms', () => {
    const g = startOpen(createGame(3, null));
    setStop(g, 1, true);
    const alive = (name: string): Corps | undefined => g.corps.find((c) => !c.dead && c.name === name);
    const size = (c: Corps): number => c.divs * (c.full ?? 1);
    const at = (h: Game, c: Corps): string => h.regions[c.reg].name;
    while (!(g.turn === 10 && current(g) === 'ITA')) stepAI(g);
    // Before Italy's month: none of its rows, and the four garrisons the record keeps in place stand at theirs.
    for (const r of ITALY) expect(alive(r[2]), r[2]).toBeUndefined();
    for (const r of ROWS.filter((x) => x[0] === 9 && x[6].hold)) {
      const c = alive(r[2]);
      expect(c, r[2]).toBeTruthy();
      expect(c!.nat, r[2]).toBe(r[1]);
      expect(at(g, c!), r[2]).toBe(r[5]);
      expect(size(c!), r[2]).toBeCloseTo(worth(r, 10), 2);
    }
    expect(alive('King\'s African Rifles (Kenya)')?.nat).toBe('UK');
    // Italy's month opens: every row of its army at its place, held there, at the size of 1 June 1940.
    startPhase(g, 'ITA');
    let sum10 = 0;
    for (const r of ITALY.filter((x) => x[0] > 10)) expect(alive(r[2]), r[2]).toBeUndefined();
    for (const r of ITALY.filter((x) => x[0] <= 10)) {
      const c = alive(r[2]);
      expect(c, r[2]).toBeTruthy();
      expect(c!.nat).toBe('ITA');
      expect(at(g, c!), r[2]).toBe(r[5]);
      expect(c!.hold, r[2]).toBe(true);
      expect(size(c!), r[2]).toBeCloseTo(worth(r, 10), 2);
      sum10 += size(c!);
    }
    expect(sum10).toBeCloseTo(17.1, 1);
    for (const place of SAHARA) expect(g.byName[place].units.some((u) => u.nat === 'ITA' && !u.dead), place).toBe(true);
    // By hand to August 1940, nobody having moved: all forces, Nasi's force come at Harar out of the command's worth.
    g.turn = 12;
    startPhase(g, 'ITA');
    let sum12 = 0;
    for (const r of ITALY) { expect(size(alive(r[2])!), r[2]).toBeCloseTo(worth(r, 12), 2); sum12 += size(alive(r[2])!); }
    expect(sum12).toBeCloseTo(24.7, 1);
    expect([at(g, alive(force('Nasi\'s force')[2])!), alive(force('Nasi\'s force')[2])!.hold]).toEqual(['Harar', true]);
    // By hand to February 1941: Mogadishu's and Kismayu's rows end, with a surrender's words and never a lent row's.
    const news = g.headlines.length;
    g.turn = 18;
    startPhase(g, 'ITA');
    const ended = ['Somalia command (Mogadishu)', 'Somalia command (Kismayu)'];
    for (const nm of ended) expect(alive(nm), nm).toBeUndefined();
    const said = g.headlines.slice(news).map((h) => h.text);
    for (const nm of ended) expect(said.some((t) => /^Italian forces lay down their arms: /.test(t) && t.includes(nm)), nm).toBe(true);
    expect(said.some((t) => /lent to the campaign/.test(t) && /Somalia command/.test(t))).toBe(false);
    // (The rows whose months have not come stand on.)
    const on = ITALY.filter((x) => x[6].until > 18);
    for (const r of on) expect(alive(r[2]), r[2]).toBeTruthy();
    // Italy's war with Britain, by hand if the book has not opened it: the rows are fed from the six seats, each a
    // depot of Italy's on its own ground. Without the marks none of them is: the army as first laid starved in the
    // fourth month of its war.
    if (!atWar(g, 'ITA', 'UK')) declareWar(g, 'ITA', 'UK');
    const marks = SEATS.map((name) => `depot:${g.byName[name].id}`);
    for (const k of marks) expect(g.flags[k], k).toBe(1);
    computeSupply(g);
    for (const r of on) expect(alive(r[2])!.supplied, r[2]).toBe(true);
    for (const k of marks) delete g.flags[k];
    computeSupply(g);
    for (const r of on) expect(alive(r[2])!.supplied, r[2]).toBe(false);
  });
});
