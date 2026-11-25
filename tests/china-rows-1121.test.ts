// The record's rows for China and the small islands' guards (the design director's rulings of 00:40Z to 00:46Z and
// 03:44Z to 03:47Z of 21 November 2026, on the second rules builder's traces of the six China tests; the historian's
// china-coast-1941-and-field-armies.md, north-china-oob-and-island-guards.md and china-1944-45-operations.md).
//
// Data only, behind the far switch: the 1941 coast's defenders at the record's weights; a garrison row for Hangchow
// and for Nanchang, so their field rows may go out; the book's massings (Second Changsha at Yochow, the 13th Army's
// gathering at Hangchow); North China's independent mixed brigades as their own rows, and at the book's stops the
// four raised to divisions on their record months; the Hunan push's order of battle with Changsha's garrison; the
// dated choices the book lacked (Chekiang 1942, the 1944 coast, Kukong and Laohokow in 1945) with their forces; and
// the navy's guard units on the small islands. The rows are data/campaign/china-setup.json and far-shores.json; the
// choices are in china-line.json.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { createGame, declareWar, removeCorps, startPhase } from '../src/campaign/game';
import { bookDate, setStop } from '../src/campaign/history';
import { ops, startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';
import world from '../data/campaign/world.json';
import chinaSetup from '../data/campaign/china-setup.json';
import chinaLine from '../data/campaign/china-line.json';
import chinaStop3 from '../data/campaign/china-stop3.json';
import shores from '../data/campaign/far-shores.json';
import farWorld from '../data/campaign/far-world.json';
import farSeats from '../data/campaign/far-seats-pacific.json';

const MINE = (chinaSetup as any)['setup-1939'].expeditions as any[][];
const FIRST = (chinaSetup as any).world.corps as any[][];
// The island guards are the dated rows that hold their post and stay on it (the Pacific's own later rows, the
// paratroops of 1942 among them, are neither), and are no part of another row: the two parts the Kawaguchi Detachment
// leaves at Kuching and Miri hold and stay too, and are `cut` from their parent (far-borneo-split).
const GUARDS = ((shores as any)['setup-1939'].expeditions as any[][]).filter((r) => r[0] >= 29 && r[6].hold === 1 && r[6].stay === 1 && r[6].cut === undefined);
const dated = (name: string): any[] => ((setup as any).expeditions as any[]).find((r) => r[2] === name)!;
const first = (name: string): any[] => ((world as any).corps as any[]).find((r) => r[1] === name)!;
const war = (stop = 3): Game => { const g = startOpen(createGame(5, null)); if (stop !== 3) setStop(g, stop, true); return g; };
const of = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.name === name && !c.dead);
const at = (g: Game, name: string): string | undefined => { const c = of(g, name); return c ? g.regions[c.reg].name : undefined; };
const put = (g: Game, name: string, place: string): void => { const c = of(g, name)!, from = g.regions[c.reg], to = g.byName[place]; from.units.splice(from.units.indexOf(c), 1); to.units.push(c); c.reg = to.id; };
const give = (g: Game, place: string, nat: string): void => { const r = g.byName[place]; r.ctrl = nat; for (const u of [...r.units]) if (u.nat !== nat) { u.dead = true; r.units.splice(r.units.indexOf(u), 1); g.corps.splice(g.corps.indexOf(u), 1); } };

const BRIGADES: [string, string][] = [
  ['Mongolia Garrison Army (2nd Independent Mixed Brigade)', 'Kalgan'],
  ['1st Army (3rd Independent Mixed Brigade)', 'Lishih'],
  ['1st Army (9th Independent Mixed Brigade)', 'Taiyuan'],
  ['North China Area Army (1st Independent Mixed Brigade)', 'Tangshan'],
  ['North China Area Army (8th Independent Mixed Brigade)', 'Shihkiachwang'],
  ['North China Area Army (15th Independent Mixed Brigade)', 'Tientsin'],
  ['12th Army (6th Independent Mixed Brigade)', 'Chefoo'],
  ['12th Army (7th Independent Mixed Brigade, Lintsing)', 'Lintsing'],
  ['12th Army (10th Independent Mixed Brigade)', 'Tsinan'],
];
const REWEIGHED = ['1st Army (independent brigades, Taiyuan)', '1st Army (western Shansi)', 'Tsingtao garrison (independent mixed brigades)'];

describe("the China file's amendments and the rows its dated rows take", () => {
  it('each amendment names the tool-written file whose row it amends, and that file holds the row', () => {
    const SRC: Record<string, any> = { 'far-world': farWorld, 'far-seats-pacific': farSeats };
    const lines = (chinaSetup as any).rows.amend as { file: string; list: string; nat: string; name: string; src: string }[];
    // (Ten of 21 November 2026 and Canton's three of 22 November: the 104th and the 18th at the chart's full division,
    // and the 38th's last month. tests/far-canton-rows.test.ts.)
    expect(lines).toHaveLength(13);
    for (const a of lines) {
      expect(Object.keys(SRC), a.name).toContain(a.src);
      const rows = (SRC[a.src][a.file]?.[a.list] ?? []) as any[][];
      const k = (r: any[]): number => (typeof r[0] === 'number' ? 1 : 0);
      expect(rows.some((r) => r[k(r)] === a.nat && r[k(r) + 1] === a.name), `${a.src}: ${a.name}`).toBe(true);
    }
  });

  it("every row a dated row takes its men from is a row of that country's on the board or in the book", () => {
    const names = (nat: string): Set<string> => new Set<string>([...((world as any).corps as any[]).filter((r) => r[0] === nat).map((r) => r[1]), ...((setup as any).expeditions as any[]).filter((r) => r[1] === nat).map((r) => r[2])]);
    const N: Record<string, Set<string>> = { JAP: names('JAP'), CHN: names('CHN') };
    for (const r of MINE) for (const nm of (r[6]?.from ?? []) as string[]) expect(N[r[1]].has(nm), `${r[2]} takes ${nm}`).toBe(true);
  });

  // (The five guards: the director's 16:44Z of 21 November 2026. Dated at the book's stops as before; at stops 3 to 5
  // they fold into the rear garrison that comes by cause, staff.ts rearMonth, at the same figures: far-rear-garrison.)
  it("the book's own rows are marked for stops 1 and 2, and so are the five guards; the brigades and the garrisons stand at every stop", () => {
    // (Canton's formations are no rows of the book's massings: the 51st Division, the brigades of 1942 and the
    // divisions of 1945 stand at every stop, one line each. The 51st's end at the book's stops is a station of the
    // book's, at Rabaul from November 1942, and no second line: the design director's 07:55Z, 08:10Z and 08:48Z of
    // 22 November 2026, tests/far-canton-rows.test.ts.)
    const canton = (r: any[]): boolean => /^23rd Army \((51st Division|19th Independent Mixed Brigade|22nd Independent Mixed Brigade|129th Division|130th Division)\)$/.test(r[2]);
    for (const r of MINE.filter((x) => x[0] >= 24 && !canton(x))) expect(r[6].stops, r[2]).toEqual([1, 2]);
    expect(MINE.filter((x) => x[0] >= 24 && canton(x))).toHaveLength(5);
    for (const r of GUARDS) expect(r[6].stops, r[2]).toEqual([1, 2]);
    expect(GUARDS).toHaveLength(5);
  });
});

describe("the coast's group armies stand on the record's places at China's scale, each group's rows summing to its total", () => {
  // The design director's 05:00Z and 05:04Z of 21 November 2026 on the historian's coast-group-armies-and-burma-elites.md.
  const COAST: [string, string, number, boolean][] = [
    ['3rd War Area (the Chekiang coast)', 'Ningpo', 0.14, true], ['10th Army Group (Taichow)', 'Ningpo', 0.09, true],
    ['Wenchow garrison (Peace Preservation troops)', 'Wenchow', 0.05, true], ['3rd War Area: 10th Army Group', 'Chuchow', 0.27, false],
    ['Foochow garrison', 'Foochow', 0.2, true], ['25th Army Group (Fukien)', 'Nanping', 0.3, false],
  ];

  it('the 10th Group Army is 0.55 in all and the 25th 0.5; a port holds what stood in it and the main body stands inland', () => {
    expect(first('3rd War Area (the Chekiang coast)')).toEqual(['CHN', '3rd War Area (the Chekiang coast)', 'inf', 1, 'Ningpo', { xp: 0.1, str: 0.14, hold: 1, est: 0.14, works: ['Ningpo', 1] }]);
    expect(first('10th Army Group (Taichow)')).toEqual(['CHN', '10th Army Group (Taichow)', 'inf', 1, 'Ningpo', { xp: 0.1, str: 0.09, est: 0.09, hold: 1 }]);
    expect(first('Wenchow garrison (Peace Preservation troops)')).toEqual(['CHN', 'Wenchow garrison (Peace Preservation troops)', 'inf', 1, 'Wenchow', { xp: 0.1, str: 0.05, est: 0.05, hold: 1, works: ['Wenchow', 1] }]);
    expect(first('3rd War Area: 10th Army Group')).toEqual(['CHN', '3rd War Area: 10th Army Group', 'inf', 1, 'Chuchow', { xp: 0.15, str: 0.27, est: 0.27 }]);
    expect(first('Foochow garrison')).toEqual(['CHN', 'Foochow garrison', 'inf', 1, 'Foochow', { xp: 0.1, str: 0.2, hold: 1, est: 0.2, works: ['Foochow', 1] }]);
    expect(first('25th Army Group (Fukien)')).toEqual(['CHN', '25th Army Group (Fukien)', 'inf', 1, 'Nanping', { xp: 0.1, str: 0.3, est: 0.3 }]);
    const sum = (names: string[]): number => Math.round(names.reduce((s, nm) => s + COAST.find((r) => r[0] === nm)![2], 0) * 100) / 100;
    expect(sum(['3rd War Area (the Chekiang coast)', '10th Army Group (Taichow)', 'Wenchow garrison (Peace Preservation troops)', '3rd War Area: 10th Army Group'])).toBe(0.55);
    expect(sum(['Foochow garrison', '25th Army Group (Fukien)'])).toBe(0.5);
  });

  it('they stand there on the first day under their caps, the ports\' rows held', () => {
    const g = war(1);
    for (const [nm, place, w, held] of COAST) {
      const c = of(g, nm)!;
      expect([g.regions[c.reg].name, c.str, c.full, !!c.hold], nm).toEqual([place, w, w, held]);
    }
  });
});

describe('Hangchow and Nanchang keep a garrison row, so their field rows may go out', () => {
  it('the rows: 0.4 each, capped, held and staying, at every stop', () => {
    for (const [nm, place] of [['Hangchow garrison (13th Army garrison units)', 'Hangchow'], ['Nanchang garrison (11th Army garrison units)', 'Nanchang']]) {
      expect(first(nm)).toEqual(['JAP', nm, 'inf', 1, place, { xp: 0.2, str: 0.4, est: 0.4, hold: 1, stay: 1 }]);
      for (const stop of [1, 3]) { const g = war(stop); const c = of(g, nm)!; expect([g.regions[c.reg].name, c.str, c.hold, !!c.stay], `${nm} at stop ${stop}`).toEqual([place, 0.4, true, true]); }
    }
  });

  it('the field rows beside them are not held', () => {
    const g = war(1);
    expect(of(g, '13th Army (22nd Division, Hangchow)')!.hold).toBe(false);
    expect(of(g, '11th Army (Nanchang)')!.hold).toBe(false);
  });
});

describe('north Kiangsu and the lower Yangtze: Haichow, the 23rd Army Group, the sweep of July 1941, and the 33rd Division in Kiangsi', () => {
  // The design director's 05:08Z, 05:11Z, 05:14Z, 05:16Z, 05:18Z and 05:35Z of 21 November 2026 on the historian's
  // haichow-yencheng-and-free-share.md. Japan at 15,000 men to 1.0; China at 40,000 at a place.
  const HAICHOW = "Haichow garrison (the Hsuchow division's detachment and the naval guard)";
  const r2 = (x: number): number => Math.round(x * 100) / 100;
  const chinese = (g: Game, place: string): [string, number, number][] => g.byName[place].units.filter((u) => u.nat === 'CHN' && !u.dead).map((u) => [u.name, r2(u.str), u.full] as [string, number, number]).sort();

  it('Haichow keeps a town row of 0.175, held and staying, at every stop, so the 12th Army\'s row there is a field row', () => {
    expect(first(HAICHOW)).toEqual(['JAP', HAICHOW, 'inf', 1, 'Haichow', { xp: 0.2, str: 0.175, est: 0.175, hold: 1, stay: 1 }]);
    for (const stop of [1, 3]) {
      const g = war(stop), c = of(g, HAICHOW)!;
      expect([g.regions[c.reg].name, c.str, c.full, c.hold, !!c.stay], `stop ${stop}`).toEqual(['Haichow', 0.175, 0.175, true, true]);
      expect(of(g, '12th Army (Shantung)')!.hold, `stop ${stop}`).toBe(false);
    }
  });

  it('the 33rd Division stands at Nanchang under its own name; the written file keeps its own place and the row for Bangkok still finds it', () => {
    expect(first('11th Army (33rd Division)').slice(0, 5)).toEqual(['JAP', '11th Army (33rd Division)', 'inf', 1, 'Nanchang']);
    expect(((farWorld as any).world.corps as any[]).find((r) => r[1] === '11th Army (33rd Division)')[4]).toBe('Hankow');
    expect(at(war(), '11th Army (33rd Division)')).toBe('Nanchang');
    expect(dated('15th Army (33rd and 55th Divisions)')[6].from).toEqual(['11th Army (33rd Division)']);
  });

  it("no Chinese row is laid in Yencheng's province: the planner's north Kiangsu row stands alone there, at every stop", () => {
    // (The design director's 05:35Z: the record's New Fourth Army and Han Deqin's remnant lived off the country for
    // years; the game has no row that can, and a row laid there starves in its third month. Their weight is carried
    // by China's own trait, and the sweep of month 23 walks in as the record's did.)
    expect([...MINE.filter((r) => r[1] === 'CHN' && r[5] === 'Yencheng'), ...FIRST.filter((r) => r[0] === 'CHN' && r[4] === 'Yencheng')]).toEqual([]);
    for (const stop of [1, 3]) {
      const g = war(stop);
      expect(chinese(g, 'Yencheng').map((x) => x.slice(0, 2)), `stop ${stop}`).toEqual([['Shantung-Kiangsu War Area (north Kiangsu)', 0.6]]);
      g.turn = 17; startPhase(g, 'CHN');
      expect(chinese(g, 'Yencheng').map((x) => x[0]), `stop ${stop}`).toEqual(['Shantung-Kiangsu War Area (north Kiangsu)']);
    }
  });

  it('the 23rd Army Group is what stays at Chuchow when the New Fourth Army\'s share leaves in month 17', () => {
    const g = war(1);
    g.turn = 17; startPhase(g, 'CHN');
    expect(chinese(g, 'Chuchow')).toEqual([['3rd War Area: 10th Army Group', 0.27, 0.27], ['3rd War Area: 23rd Army Group', 0.75, 0.75]]);
    expect(of(g, '3rd War Area: 23rd Army Group and the New Fourth Army')).toBeUndefined();
  });

  it("the sweep: Japan's 1.1 lent at Nanking for months 23 and 24 at the book's stops, and Yencheng's town row of 0.2 once the place is Japan's", () => {
    const g = war(1);
    g.turn = 23; startPhase(g, 'JAP');
    const s = of(g, '13th Army (the Yencheng sweep)')!;
    expect([g.regions[s.reg].name, s.divs, s.str, s.full, s.hold]).toEqual(['Nanking', 2, 0.55, 0.55, false]);
    expect(of(g, 'Yencheng garrison (13th Army detachment)')).toBeUndefined();
    give(g, 'Yencheng', 'JAP');
    g.turn = 24; startPhase(g, 'JAP');
    const k = of(g, 'Yencheng garrison (13th Army detachment)')!;
    expect([g.regions[k.reg].name, k.str, k.full, k.hold, !!k.stay]).toEqual(['Yencheng', 0.2, 0.2, true, true]);
    g.turn = 25; startPhase(g, 'JAP');
    expect([of(g, '13th Army (the Yencheng sweep)'), at(g, 'Yencheng garrison (13th Army detachment)')]).toEqual([undefined, 'Yencheng']);
    const b = war();
    for (const t of [23, 24]) { b.turn = t; startPhase(b, 'JAP'); }
    expect([of(b, '13th Army (the Yencheng sweep)'), of(b, 'Yencheng garrison (13th Army detachment)')]).toEqual([undefined, undefined]);
  });

  it("ground held asks the record's figures by region: 13 across the north's 23 provinces and about 7 across the centre's 11, all Japan's on the first day", () => {
    const by = (chinaStop3 as any).open.garrison.by as { name: string; each: number; places: string[] }[];
    const n = by.find((x) => x.name.startsWith('North China'))!, c = by.find((x) => x.name.startsWith('central China'))!;
    expect([n.places.length, r2(n.each * n.places.length)]).toEqual([23, 13]);
    expect([c.places.length, r2(c.each * c.places.length)]).toEqual([11, 7]);
    const g = war();
    for (const p of [...n.places, ...c.places]) expect([g.byName[p]?.owner, g.byName[p]?.ctrl], p).toEqual(['CHN', 'JAP']);
  });
});

describe("North China's independent mixed brigades are their own rows", () => {
  it('nine new rows of 0.375 at their places, and three of the planner\'s rows re-weighed to a brigade each', () => {
    for (const [nm, place] of BRIGADES) expect(first(nm), nm).toEqual(['JAP', nm, 'inf', 1, place, { xp: 0.2, str: 0.375, est: 0.375, hold: 1, stay: 1 }]);
    for (const nm of REWEIGHED) { const o = first(nm)[5]; expect([o.str, o.est, o.hold, o.stay], nm).toEqual([0.375, 0.375, 1, 1]); }
  });

  it('about 21 pieces worth 12 to 13 hold the north on the first day, the 5th Division and Hsuchow\'s garrison apart', () => {
    const NORTH = ['Peiping', 'Tientsin', 'Tangshan', 'Paoting', 'Shihkiachwang', 'Kalgan', 'Silingol', 'Suiyuan', 'Shangtu', 'Paotow', 'Tatung', 'Taiyuan', 'Linfen', 'Lishih', 'Tsinan', 'Tsingtao', 'Chefoo', 'Sintai', 'Lintsing', 'Anyang', 'Kaifeng', 'Haichow'];
    const g = war();
    // (Haichow's town row is a detachment of the Hsuchow division with the navy's guard: apart, with Hsuchow's garrison.)
    const rows = g.corps.filter((c) => c.nat === 'JAP' && !c.dead && c.kind !== 'arm' && NORTH.includes(g.regions[c.reg].name) && c.name !== '12th Army (5th Division)' && !c.name.startsWith('Haichow garrison'));
    expect(rows).toHaveLength(21);
    const worth = rows.reduce((s, c) => s + c.divs * c.str, 0);
    expect(worth).toBeGreaterThan(12);
    expect(worth).toBeLessThan(13.5);
    // (Of the provinces that stood empty, the brigades hold seven; Paoting, Sintai and Mengkiang beyond Kalgan had none of their own.)
    const empty = NORTH.filter((p) => !g.byName[p].units.some((u) => u.nat === 'JAP' && !u.dead));
    expect(empty.sort()).toEqual(['Paoting', 'Shangtu', 'Silingol', 'Sintai', 'Suiyuan']);
  });

  it("at the book's stops the four raised to divisions come on the record's months, each from its brigade", () => {
    const g = war(1);
    g.turn = 30; startPhase(g, 'JAP');
    // (A division's row carries no cap, so it takes the month's drafts as every division does: 0.9 and a little.)
    expect([at(g, '12th Army (59th Division)'), of(g, '12th Army (10th Independent Mixed Brigade)')]).toEqual(['Tsinan', undefined]);
    expect(of(g, '12th Army (59th Division)')!.str).toBeGreaterThanOrEqual(0.9);
    expect(of(g, '12th Army (59th Division)')!.str).toBeLessThan(0.95);
    expect([at(g, '1st Army (69th Division)'), of(g, '1st Army (western Shansi)')]).toEqual(['Lishih', undefined]);
    expect(at(g, '1st Army (3rd Independent Mixed Brigade)')).toBe('Lishih');
    g.turn = 46; startPhase(g, 'JAP');
    expect([at(g, '1st Army (62nd Division)'), of(g, '1st Army (independent brigades, Taiyuan)'), of(g, '12th Army (6th Independent Mixed Brigade)')]).toEqual(['Taiyuan', undefined, undefined]);
    // (The 63rd takes Peiping as the 27th leaves for Manchuria: the planner's row of the same month.)
    expect([at(g, 'North China Area Army (63rd Division)'), of(g, 'North China Area Army (15th Independent Mixed Brigade)'), at(g, 'Kwantung Army (27th Division, Chinchow)')]).toEqual(['Peiping', undefined, 'Chinchow']);
  });

  it("the planner's later rows find them: the 62nd for Kaifeng takes the division raised, and the 7th Brigade's row takes Lintsing's", () => {
    expect(dated('12th Army (62nd Division)')[6].from).toEqual(['1st Army (independent brigades, Taiyuan)', '1st Army (62nd Division)']);
    expect(dated('12th Army (7th Independent Mixed Brigade)')[6].from).toEqual(['12th Army (7th Independent Mixed Brigade, Lintsing)']);
  });

  it('where no book dates the rows, none is raised to a division and the brigades stand', () => {
    const g = war();
    for (const t of [30, 46]) { g.turn = t; startPhase(g, 'JAP'); }
    for (const nm of ['12th Army (59th Division)', '1st Army (69th Division)', '1st Army (62nd Division)', 'North China Area Army (63rd Division)']) expect(of(g, nm), nm).toBeUndefined();
    expect(at(g, '12th Army (10th Independent Mixed Brigade)')).toBe('Tsinan');
  });
});

describe("the book's massings: rows that take their men from rows on the board, or are lent for their months", () => {
  it('Second Changsha masses at Yochow in month 24: the 3rd, 6th and 40th from their rows, the 4th and four detachments lent to month 27', () => {
    const g = war(1);
    g.turn = 24; startPhase(g, 'JAP');
    // (The divisions' rows carry no cap and take the month's drafts, 0.9 and a little; the detachments are capped.)
    const there = g.byName['Yochow'].units.filter((u) => u.nat === 'JAP' && !u.dead).map((u) => [u.name, Math.round(u.str * 20) / 20]).sort();
    expect(there).toEqual([['11th Army (3rd Division)', 0.9], ['11th Army (40th Division)', 0.9], ['11th Army (4th Division)', 0.9], ['11th Army (6th Division)', 0.9], ['11th Army (the four detachments for Changsha)', 0.8]]);
    for (const nm of ['11th Army (3rd Division, Sinyang)', '11th Army (Yochow)', '11th Army (Kiukiang)']) expect(of(g, nm), nm).toBeUndefined();
    g.turn = 27; startPhase(g, 'JAP');
    expect([of(g, '11th Army (4th Division)'), of(g, '11th Army (the four detachments for Changsha)')]).toEqual([undefined, undefined]);
    expect(of(g, '11th Army (3rd Division)')).toBeDefined();
  });

  it('the 13th Army gathers about four divisions\' worth at Hangchow in month 33, and the 15th is home at Nanking in month 37 under its own name', () => {
    const g = war(1);
    g.turn = 33; startPhase(g, 'JAP');
    const worth = g.byName['Hangchow'].units.filter((u) => u.nat === 'JAP' && !u.dead && !u.hold).reduce((s, u) => s + u.divs * u.str, 0);
    expect(worth).toBeGreaterThan(3.5);
    expect(worth).toBeLessThan(4.2);
    expect([at(g, '13th Army (15th Division, for Chekiang)'), of(g, '13th Army (15th Division, Nanking)')]).toEqual(['Hangchow', undefined]);
    expect(at(g, 'Hangchow garrison (13th Army garrison units)')).toBe('Hangchow');
    g.turn = 37; startPhase(g, 'JAP');
    for (const nm of ['13th Army (15th Division, for Chekiang)', '13th Army (detachments of the 32nd and 116th Divisions)', '13th Army (mixed brigades, for Chekiang)']) expect(of(g, nm), nm).toBeUndefined();
    const home = g.corps.filter((c) => c.name === '13th Army (15th Division, Nanking)' && !c.dead);
    expect(home.map((c) => [g.regions[c.reg].name, c.hold, !!c.stay])).toEqual([['Nanking', true, true]]);
    // (Its row's 0.9 and the month's drafts, a little: read with room, not rounded to the twentieth. The drafts' share
    // stood at 0.9248 and moves by a hair with whatever else draws them that month: 0.9251 once the tank divisions
    // formed short of their lent regiments, the lent rows' commit of 22 November 2026.)
    expect(home[0].str).toBeGreaterThanOrEqual(0.9);
    expect(home[0].str).toBeLessThan(0.95);
    // (The planner's row of month 54, the 15th for Mandalay, still finds it.)
    expect(dated('15th Division')[6].from).toEqual(['13th Army (15th Division, Nanking)']);
  });

  it('no massing comes where no book dates the rows', () => {
    const g = war();
    for (const t of [24, 33]) { g.turn = t; startPhase(g, 'JAP'); }
    expect([of(g, '11th Army (3rd Division)'), of(g, '11th Army (4th Division)'), of(g, '13th Army (15th Division, for Chekiang)')]).toEqual([undefined, undefined, undefined]);
    expect(at(g, '13th Army (15th Division, Nanking)')).toBe('Nanking');
  });
});

describe("the Hunan push of 1944 is the 11th Army's seven divisions on a broad front", () => {
  it('in month 57 a western group and a centre at Yochow and an eastern group at Nanchang, formed from the rows on the board', () => {
    const g = war(1);
    // (As a war runs it: the massing of month 24 comes and its lent rows go home in month 27, and the staff sends
    // the three divisions back to the line their first rows held, Shasi, Sinyang and Hankow, so Yochow has room in
    // month 57. A full province would send a group on to the nearest ground with room before its own rows are struck.)
    g.turn = 24; startPhase(g, 'JAP');
    g.turn = 27; startPhase(g, 'JAP');
    put(g, '11th Army (3rd Division)', 'Shasi'); put(g, '11th Army (6th Division)', 'Sinyang'); put(g, '11th Army (40th Division)', 'Hankow');
    g.turn = 57; startPhase(g, 'JAP');
    expect([at(g, '11th Army (40th and 116th Divisions)'), of(g, '11th Army (40th and 116th Divisions)')!.divs]).toEqual(['Yochow', 2]);
    expect([at(g, '11th Army (34th, 58th and 68th Divisions)'), of(g, '11th Army (34th, 58th and 68th Divisions)')!.divs]).toEqual(['Yochow', 3]);
    expect([at(g, '11th Army (3rd and 13th Divisions)'), of(g, '11th Army (3rd and 13th Divisions)')!.divs]).toEqual(['Nanchang', 2]);
    // (Nobody is counted twice: the rows they are formed from are gone, under their first names or the massing's.)
    for (const nm of ['11th Army (Kiukiang)', '11th Army (40th Division)', '11th Army (Yochow)', '11th Army (6th Division)', '11th Army (Nanchang)', '11th Army (3rd Division, Sinyang)', '11th Army (3rd Division)', '11th Army (Wuhan)']) expect(of(g, nm), nm).toBeUndefined();
    expect(at(g, 'Nanchang garrison (11th Army garrison units)')).toBe('Nanchang');
  });

  it('the 39th stays at Ichang where Ichang is Japan\'s, and Changsha\'s garrison waits for the town', () => {
    const g = war(1);
    give(g, 'Ichang', 'JAP');
    g.turn = 57; startPhase(g, 'JAP');
    const c = of(g, '11th Army (39th Division, Ichang)')!;
    expect([g.regions[c.reg].name, c.hold, !!c.stay]).toEqual(['Ichang', true, true]);
    g.turn = 58; startPhase(g, 'JAP');
    expect(of(g, 'Changsha garrison (11th Army rear units)')).toBeUndefined();
    give(g, 'Changsha', 'JAP');
    g.turn = 59; startPhase(g, 'JAP');
    const k = of(g, 'Changsha garrison (11th Army rear units)')!;
    expect([g.regions[k.reg].name, k.str, k.full, k.hold, !!k.stay]).toEqual(['Changsha', 0.4, 0.4, true, true]);
  });

  it('with Ichang China\'s the 39th waits for it and is put ashore nowhere', () => {
    const g = war(1);
    expect(g.byName['Ichang'].ctrl).toBe('CHN');
    g.turn = 57; startPhase(g, 'JAP');
    expect(of(g, '11th Army (39th Division, Ichang)')).toBeUndefined();
  });
});

describe('the dated choices the book lacked', () => {
  const list = ops('JAP');
  const op = (id: string): any => list.find((o) => o.id === id)!;

  it('Chekiang 1942, the 1944 coast, Kukong and Laohokow are on the book with their months', () => {
    const g = war(1);
    expect(['chekiang42', 'coast44', 'kukong45', 'laohokow45'].map((id) => bookDate(g, 'JAP', id))).toEqual([33, 61, 64, 66]);
    for (const id of ['chekiang42', 'kukong45', 'laohokow45']) expect([op(id).side, op(id).ratio, op(id).prep], id).toEqual([undefined, 0.8, 0]);
    expect([op('chekiang42').push, op('kukong45').push, op('laohokow45').push]).toEqual([3, 3, 2]);
  });

  it('each is dated the month before its first span opens, as the raids of 1939 to 1941 are; the landings in their span\'s month', () => {
    const spans = (chinaLine as any).history.lines[0].spans as Record<string, { from: number; to: number }[]>;
    const opens = (p: string, after: number): number => spans[p].map((s) => s.from).filter((m) => m >= after)[0];
    expect([opens('Chuchow', 30), opens('Shangjao', 30), opens('Linchwan', 30)]).toEqual([34, 34, 34]);
    expect([opens('Kukong', 60), opens('Kanchow', 60)]).toEqual([65, 66]);
    expect(opens('Siangyang', 60)).toBe(67);
    expect([opens('Wenchow', 60), opens('Foochow', 60)]).toEqual([61, 62]);
  });

  it('the coast landings of 1944 are a side-show with two landings from Japan\'s own ports, by forces of about a brigade', () => {
    const c = op('coast44');
    expect([c.side, c.ratio, c.push]).toEqual([true, undefined, undefined]);
    expect(c.landings).toEqual([{ corps: '13th Army (Wenchow force, 1944)', to: ['Wenchow'] }, { corps: 'Foochow landing force (1944, with naval landing troops)', to: ['Foochow'] }]);
    const g = war(1);
    for (const t of [60, 61]) { g.turn = t; startPhase(g, 'JAP'); }
    const w = of(g, '13th Army (Wenchow force, 1944)')!, f = of(g, 'Foochow landing force (1944, with naval landing troops)')!;
    expect([w.str, w.full, f.str, f.full]).toEqual([0.55, 0.55, 0.5, 0.5]);
    for (const x of [w, f]) { const r = g.regions[x.reg]; expect([r.owner, r.port], x.name).toEqual(['JAP', true]); }
  });

  it('the coast raid of 1941 is a side-show too, dated to its month, with its three landings (the director, 13:39Z of 21 November 2026)', () => {
    // As a great choice it waited while the staff was busy, and at stop 2 its forces never sailed in two wars of the
    // six: Ningpo had fallen by land the month before the choice was due. A side-show goes beside whatever the army does.
    const c = op('coast41');
    expect([c.side, c.ratio, c.prep, c.push]).toEqual([true, undefined, undefined, undefined]);
    expect(bookDate(war(1), 'JAP', 'coast41')).toBe(20);
    expect(c.landings.map((l: { corps: string }) => l.corps)).toEqual(['23rd Army (48th Division)', '13th Army (Chekiang landing force)', '13th Army (Wenchow detachment)']);
    // The raids that open the China front are great choices still.
    for (const id of ['nanning', 'wuyuan', 'ichang', 'yencheng', 'changsha41']) expect([op(id).side, op(id).ratio], id).toEqual([undefined, 0.8]);
  });

  it("the 1945 forces: the 23rd Army's brigades at Canton, and the 12th Army formed up at Sinyang from its own rows", () => {
    const g = war(1);
    for (const t of [56, 59, 64, 66]) {
      g.turn = t; startPhase(g, 'JAP');
      // (By the book the 51st Division stands at Rabaul from November 1942: tests/far-canton-rows.test.ts. This board
      // steps from the first day to 1944 with no month between played, so the division's row, raised for the war in
      // China and first seen here, comes late and at Canton, the fifth corps there. It is taken off by hand, the
      // months that took it to Rabaul not being played; the case reads where the brigades of 1945 are put down.)
      const d51 = of(g, '23rd Army (51st Division)');
      if (d51) removeCorps(g, d51);
    }
    expect(at(g, '23rd Army (independent mixed brigades, for Kukong)')).toBe('Canton');
    const there = g.byName['Sinyang'].units.filter((u) => u.nat === 'JAP' && !u.dead).map((u) => [u.name, u.kind, u.divs]).sort();
    expect(there).toEqual([['12th Army (110th and 115th Divisions)', 'inf', 2], ['12th Army (3rd Tank Division, for Laohokow)', 'arm', 1], ['12th Army (4th Cavalry Brigade, for Laohokow)', 'cav', 1]]);
    for (const nm of ['12th Army (110th Division)', '12th Army (115th Division)', '12th Army (7th Independent Mixed Brigade)', '12th Army (3rd Tank Division)', '12th Army (4th Cavalry Brigade)']) expect(of(g, nm), nm).toBeUndefined();
  });
});

describe("the small islands' guards after capture are the navy's guard units, at the record's weight", () => {
  it('the rows: Guam 0.03, Kendari 0.04, Menado 0.02, Biak 0.02, Guadalcanal 0.02, capped, held and staying; Morotai has none', () => {
    expect(GUARDS.map((r) => [r[0], r[5], r[6].str, r[6].est, r[6].hold, r[6].stay])).toEqual([[29, 'Guam', 0.03, 0.03, 1, 1], [30, 'Kendari', 0.04, 0.04, 1, 1], [30, 'Menado', 0.02, 0.02, 1, 1], [34, 'Biak', 0.02, 0.02, 1, 1], [35, 'Guadalcanal', 0.02, 0.02, 1, 1]]);
    expect(((setup as any).expeditions as any[]).some((r) => r[1] === 'JAP' && r[5] === 'Morotai')).toBe(false);
  });

  it('at the book\'s stops: where the island is Japan\'s on the date the guard stands in it; where it is not, it comes ashore at a port Japan holds and the staff posts it', () => {
    const g = war(1);
    declareWar(g, 'JAP', 'USA');
    give(g, 'Guam', 'JAP');
    g.turn = 29; startPhase(g, 'JAP');
    const c = of(g, '54th Guard Unit (Guam)')!;
    expect([g.regions[c.reg].name, c.str, c.full, c.hold, !!c.stay]).toEqual(['Guam', 0.03, 0.03, true, true]);
    const b = war(1);
    declareWar(b, 'JAP', 'USA');
    b.turn = 29; startPhase(b, 'JAP');
    const x = of(b, '54th Guard Unit (Guam)')!;
    const r = b.regions[x.reg];
    expect([r.name !== 'Guam', r.ctrl, r.port, x.hold, x.str]).toEqual([true, 'JAP', true, false, 0.03]);
  });

  it('at stops 3 to 5 no guard comes on its date: it comes by cause, the month after its port is taken (far-rear-garrison)', () => {
    const g = war();
    declareWar(g, 'JAP', 'USA');
    give(g, 'Guam', 'JAP');
    g.turn = 29; startPhase(g, 'JAP');
    expect(of(g, '54th Guard Unit (Guam)')).toBeUndefined();
  });

  it('no guard comes in a war that has not begun', () => {
    const g = war();
    g.turn = 30; startPhase(g, 'JAP');
    for (const r of GUARDS) expect(of(g, r[2]), r[2]).toBeUndefined();
  });
});
