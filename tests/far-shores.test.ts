// The owed shores' data (ruling 234, the design director's notes of 22:05Z and 22:10Z of 20 November 2026, on the
// second rules builder's table and the historian's rows: historian/south-landings-record-1941-42.md, sections 7, 8).
//
// The rows of the shores Japan never took in the measured wars are re-weighed to the record at the game's own count,
// 15,000 men to a division; every outpost row carries a cap; the 38th Division comes as its three regimental rows
// where no book dates Japan's rows; works and will are a row's own data where the record gives them, in place of
// the rule that every corps left alone digs in whole and of the common level of morale; and the KNIL's militia is
// Dutch until the Netherlands falls. The rows are data/campaign/far-shores.json: a hand-kept seats file whose
// amendments reach rows that the written files lay.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, capitulate, createGame, declareWar, endPhase, relocate, startPhase } from '../src/campaign/game';
import { amendRow, FAR_LAID } from '../src/campaign/far-rows';
import { setStop } from '../src/campaign/history';
import { MORALE, moraleMonth } from '../src/campaign/morale';
import { ops, startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';
import world from '../data/campaign/world.json';
import shores from '../data/campaign/far-shores.json';
import farWorld from '../data/campaign/far-world.json';
import farSeats from '../data/campaign/far-seats-pacific.json';

const R228 = '228th Infantry Regiment (the Ito Detachment)';
const R229 = "229th Infantry Regiment (with the 38th Division's headquarters)";
const R230 = '230th Infantry Regiment (the Shoji Detachment)';
const D38 = '38th Division (Canton)';
const MILITIA = "KNIL mobilisation (Java's militia and the Landstorm)";

const dated = (list: 'expeditions' | 'formations', name: string): any[] => ((setup as any)[list] as any[]).find((r) => r[2] === name)!;
const first = (name: string): any[] => ((world as any).corps as any[]).find((r) => r[1] === name)!;
const war = (stop = 3): Game => { const g = startOpen(createGame(5, null)); if (stop !== 3) setStop(g, stop, true); return g; };
const of = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.name === name && !c.dead);

describe('a hand-kept seats file amends rows the written files lay', () => {
  it('finds a dated row and a first-day row by country and name, lays words over the row\'s own, and can give it another country', () => {
    const targets = { a: { dated: [[3, 'UK', 'A row', 'inf', 1, 'Here', { str: 0.2, hold: 1 }], [4, 'UK', 'Another', 'inf', 1, 'There']], day: [['NED', 'A row', 'inf', 1, 'Here', { str: 0.4 }]] } };
    amendRow(targets, 'test', { file: 'a', list: 'dated', nat: 'UK', name: 'A row', src: 'x', as: 'NED', set: { str: 0.1, est: 0.1 } });
    expect(targets.a.dated[0]).toEqual([3, 'NED', 'A row', 'inf', 1, 'Here', { str: 0.1, hold: 1, est: 0.1 }]);
    amendRow(targets, 'test', { file: 'a', list: 'dated', nat: 'UK', name: 'Another', src: 'x', set: { works: ['There', 0] } });
    expect(targets.a.dated[1]).toEqual([4, 'UK', 'Another', 'inf', 1, 'There', { works: ['There', 0] }]);
    amendRow(targets, 'test', { file: 'a', list: 'day', nat: 'NED', name: 'A row', src: 'x', set: { will: 0.3 } });
    expect(targets.a.day[0]).toEqual(['NED', 'A row', 'inf', 1, 'Here', { str: 0.4, will: 0.3 }]);
  });

  it('can stand a row in another place under the same name, dated or first-day, and leaves its words alone', () => {
    const targets = { a: { dated: [[3, 'UK', 'A row', 'inf', 1, 'Here', { str: 0.2, hold: 1 }]], day: [['NED', 'A row', 'inf', 1, 'Here', { str: 0.4 }], ['NED', 'Another', 'inf', 2, 'Here']] } };
    amendRow(targets, 'test', { file: 'a', list: 'dated', nat: 'UK', name: 'A row', src: 'x', at: 'There' });
    expect(targets.a.dated[0]).toEqual([3, 'UK', 'A row', 'inf', 1, 'There', { str: 0.2, hold: 1 }]);
    amendRow(targets, 'test', { file: 'a', list: 'day', nat: 'NED', name: 'A row', src: 'x', at: 'There', set: { est: 0.4 } });
    expect(targets.a.day[0]).toEqual(['NED', 'A row', 'inf', 1, 'There', { str: 0.4, est: 0.4 }]);
    amendRow(targets, 'test', { file: 'a', list: 'day', nat: 'NED', name: 'Another', src: 'x', at: 'Elsewhere' });
    expect(targets.a.day[1]).toEqual(['NED', 'Another', 'inf', 2, 'Elsewhere']);
  });

  it('an amendment that finds no row, or names no list, is an error and never passes in silence', () => {
    const targets = { a: { dated: [[3, 'UK', 'A row', 'inf', 1, 'Here']] } };
    expect(() => amendRow(targets, 'test', { file: 'a', list: 'dated', nat: 'UK', name: 'No such row', src: 'x', set: {} })).toThrow(/no row "No such row" of UK/);
    expect(() => amendRow(targets, 'test', { file: 'a', list: 'dated', nat: 'NED', name: 'A row', src: 'x', set: {} })).toThrow(/no row/);
    expect(() => amendRow(targets, 'test', { file: 'a', list: 'nowhere', nat: 'UK', name: 'A row', src: 'x', set: {} })).toThrow(/names no list/);
    expect(() => amendRow(targets, 'test', { file: 'a', list: 'dated', nat: 'UK', name: 'A row', set: {} })).toThrow(/names no source file/);
  });

  // (The design director, 23:18Z of 20 November 2026, 1: each line names the row it amends and its source file, and a
  // line leaves when its source's tool takes the figure.)
  it("each line of the shores file names the tool-written file whose row it amends, and that file holds the row", () => {
    const SRC: Record<string, any> = { 'far-world': farWorld, 'far-seats-pacific': farSeats };
    for (const a of (shores as any).rows.amend as { file: string; list: string; nat: string; name: string; src: string }[]) {
      expect(Object.keys(SRC), a.name).toContain(a.src);
      const rows = (SRC[a.src][a.file]?.[a.list] ?? []) as any[][];
      const k = (r: any[]): number => (typeof r[0] === 'number' ? 1 : 0);
      expect(rows.some((r) => r[k(r)] === a.nat && r[k(r) + 1] === a.name), `${a.src}: ${a.name}`).toBe(true);
    }
  });

  it('the shores file is laid last, so its amendments find every row', () => {
    expect(FAR_LAID[FAR_LAID.length - 1]).toBe('far-shores');
    // (38: thirty-four of the owed shores and the Indies' record; (g) of 21 November 2026, the 21st Division's old cut marked for
    // stops 3 to 5 and its Bataan detachment naming either cut; and the two Khalkhin Gol rows that hold, far-khalkhin-rows-hold.test.ts.)
    expect((shores as any).rows.amend).toHaveLength(38);
  });
});

describe("the shores' rows stand at the record's weights, at the game's count of 15,000 men to a division", () => {
  it('Rabaul: the South Seas Detachment 0.33 and Lark Force 0.09', () => {
    expect(dated('expeditions', 'South Seas Detachment')[6]).toMatchObject({ str: 0.33, est: 0.33, hold: 1 });
    expect(dated('formations', 'Lark Force (2/22nd Battalion)')[6]).toMatchObject({ str: 0.09, est: 0.09, hold: 1 });
  });

  it("Wake: the garrison capped at its record 0.035, the landing force at the second try's 0.10", () => {
    expect(dated('formations', 'Wake detachment, 1st Defense Battalion')[6]).toMatchObject({ str: 0.035, est: 0.035 });
    expect(dated('expeditions', 'Naval landing force (Maizuru 2nd, for Wake)')[6]).toMatchObject({ str: 0.1, est: 0.1 });
  });

  it('Midway starts at 0.06 and fills to a cap of 0.2; Samoa\'s battalion is 0.05 and the 2nd Marine Brigade its own row of January 1942', () => {
    expect(dated('formations', 'Midway defense battalion')[6]).toMatchObject({ str: 0.06, est: 0.2 });
    expect(dated('formations', '7th Defense Battalion')[6]).toMatchObject({ str: 0.05, est: 0.05 });
    expect(dated('formations', '2nd Marine Brigade')).toEqual([29, 'USA', '2nd Marine Brigade', 'inf', 1, 'Samoa', { xp: 0.1, str: 0.32, hold: 1, est: 0.32 }]);
  });

  it('every outpost row of the Pacific carries a cap: no row of a division or less, held on an island post, fills by drafts without one', () => {
    for (const nm of ['Wake detachment, 1st Defense Battalion', 'Midway defense battalion', '7th Defense Battalion', '2nd Marine Brigade', 'Lark Force (2/22nd Battalion)']) expect(dated('formations', nm)[6].est, nm).toBeLessThan(1);
  });

  it('the garrisons come at those weights and stay under their caps', () => {
    const g = war();
    const read = (nm: string): [string, number, number | undefined] => { const c = of(g, nm)!; return [g.regions[c.reg].name, c.str, c.full]; };
    g.turn = 13; startPhase(g, 'USA');
    // (It comes at 0.06, and its first month's draft has already reached it.)
    const came = read('Midway defense battalion');
    expect([came[0], came[2]]).toEqual(['Midway', 0.2]);
    expect(came[1]).toBeGreaterThanOrEqual(0.06);
    expect(came[1]).toBeLessThan(0.07);
    for (const [t, nat] of [[19, 'USA'], [20, 'AUS'], [24, 'USA'], [29, 'USA']] as [number, string][]) { g.turn = t; startPhase(g, nat); }
    // (Midway fills by the months' drafts, and never past its cap; Wake, at its cap from the day it came, is sent none.)
    const midway = of(g, 'Midway defense battalion')!;
    expect(midway.str).toBeGreaterThan(came[1]);
    expect(midway.str).toBeLessThanOrEqual(0.2);
    expect(read('Wake detachment, 1st Defense Battalion')).toEqual(['Wake', 0.035, 0.035]);
    expect(read('7th Defense Battalion')).toEqual(['Samoa', 0.05, 0.05]);
    expect(read('2nd Marine Brigade')).toEqual(['Samoa', 0.32, 0.32]);
    expect(read('Lark Force (2/22nd Battalion)')).toEqual(['Rabaul', 0.09, 0.09]);
  });
});

describe('the 38th Division comes as three regimental rows where no book dates Japan\'s rows', () => {
  it('the rows: the 228th 0.3, the 229th with the headquarters and its general 0.3, the 230th 0.3, at Canton from month 2, at stops 3 to 5', () => {
    expect(dated('expeditions', R228)).toEqual([2, 'JAP', R228, 'inf', 1, 'Canton', { str: 0.3, est: 0.3, xp: 0.3, stops: [3, 4, 5] }]);
    expect(dated('expeditions', R229)).toEqual([2, 'JAP', R229, 'inf', 1, 'Canton', { str: 0.3, est: 0.3, xp: 0.3, gen: 'Tadayoshi Sano', stops: [3, 4, 5] }]);
    expect(dated('expeditions', R230)).toEqual([2, 'JAP', R230, 'inf', 1, 'Canton', { str: 0.3, est: 0.3, xp: 0.3, stops: [3, 4, 5] }]);
    // The division's own row is the book's: it comes at the book's stops alone, so nobody is counted twice.
    expect(dated('expeditions', D38)[6].stops).toEqual([1, 2]);
    // All three are held for the plan, as the division was (rider b423).
    const held = ((setup as any).bookOnly.rows as [string, string][]).map((r) => r[1]);
    for (const nm of [R228, R229, R230]) expect(held).toContain(nm);
  });

  it('at stop 3 the three come and the division does not; at the book\'s stops the division comes and they do not', () => {
    const g = war();
    g.turn = 2; startPhase(g, 'JAP');
    expect([R228, R229, R230].map((nm) => { const c = of(g, nm)!; return [g.regions[c.reg].name, c.str, c.hold, !!c.stay]; })).toEqual([['Canton', 0.3, true, true], ['Canton', 0.3, true, true], ['Canton', 0.3, true, true]]);
    expect(of(g, D38)).toBeUndefined();
    expect(of(g, R229)!.general).not.toBeNull();
    const b = war(1);
    b.turn = 2; startPhase(b, 'JAP');
    expect(of(b, D38)!.str).toBe(1);
    expect([R228, R229, R230].map((nm) => of(b, nm))).toEqual([undefined, undefined, undefined]);
  });

  it('the plan: all three for Hong Kong first, then the 228th for Ambon and Timor, the 229th for Palembang, the 230th for Bandoeng\'s coast', () => {
    const south = ops('JAP').find((op) => op.id === 'south')!;
    const hk = south.fronts!.find((F) => F.id === 'hongkong')!;
    expect(hk.rows).toEqual([R229, R228, R230, '23rd Army artillery (1st Artillery Group)']);
    expect(hk.land).toEqual([{ corps: R229, with: [R228, R230, '23rd Army artillery (1st Artillery Group)'], to: 'Tungkun' }]);
    const then = south.fronts!.find((F) => F.id === 'indies')!.then!;
    const mine = (nm: string) => then.filter((f) => f.corps === nm).map((f) => [f.month, f.to, f.after]);
    expect(mine(R228)).toEqual([[1, ['Ambon'], ['Hong Kong', 'Kendari']], [2, ['Koepang'], ['Ambon']]]);
    expect(mine(R229)).toEqual([[2, ['Palembang'], ['Hong Kong', 'Johore']]]);
    expect(mine(R230)).toEqual([[3, ['Bandoeng'], ['Hong Kong', 'Palembang']]]);
    expect(JSON.stringify(south.fronts)).not.toContain(D38);
    // Eretan Wetan's ground on this chart is Bandoeng's own Java Sea coast: the regiment lands in the province.
    const g = war();
    expect(g.byName['Bandoeng'].seas.length).toBeGreaterThan(0);
  });
});

describe("works are a row's own where the record gives them, and the rule that every corps digs in whole gives way", () => {
  it('the rows: no works at the Indies\' beaches, works begun at Rabaul and Palembang, and the rule for the rest', () => {
    expect(first('KNIL 1st Division (West Java)')[5].works).toEqual(['Batavia', 0]);
    expect(first('KNIL 2nd Division (Central Java)')[5].works).toEqual(['Semarang', 0]);
    expect(first('Sumatra garrisons (Medan and Atjeh)')[5].works).toEqual(['Medan', 0]);
    expect(dated('expeditions', MILITIA)[6].works).toEqual(['Bandoeng', 0]);
    expect(first('Palembang garrison (the oil)')[5].works).toEqual(['Palembang', 1]);
    expect(dated('formations', 'Lark Force (2/22nd Battalion)')[6].works).toEqual(['Rabaul', 1]);
    for (const nm of ['KNIL 3rd Division (East Java)', 'Great East garrisons (Ambon, Celebes, Timor)', 'Borneo garrisons (Tarakan and Balikpapan)']) expect(first(nm)[5].works, nm).toBeUndefined();
    for (const nm of ['Gull Force (2/21st Battalion)', 'Blackforce (2/3rd Machine Gun and 2/2nd Pioneer Battalions)']) expect(dated('expeditions', nm)[6].works, nm).toBeUndefined();
  });

  it('a row with no works never digs in at its post, a row with works begun never passes them, and a row with none of its own digs in whole', () => {
    const g = war();
    const none = of(g, 'KNIL 1st Division (West Java)')!, plain = of(g, 'KNIL 3rd Division (East Java)')!, begun = of(g, 'Palembang garrison (the oil)')!;
    expect([none.ownWorks, begun.ownWorks, plain.ownWorks]).toEqual([[g.byName['Batavia'].id, 0], [g.byName['Palembang'].id, 1], undefined]);
    for (let t = 1; t <= 6; t++) { g.turn = t; endPhase(g, 'NED'); }
    expect([!!none.dug, none.dugFor]).toEqual([false, undefined]);
    expect([!!begun.dug, begun.dugFor]).toEqual([true, 1]);
    expect(plain.dug).toBe(true);
    expect(plain.dugFor!).toBeGreaterThanOrEqual(3);
  });

  it('the flag is the post\'s: off the ground its row names, the corps digs in as any other does', () => {
    const g = war();
    const c = of(g, 'KNIL 1st Division (West Java)')!;
    relocate(g, c, g.byName['Bandoeng']);
    for (let t = 1; t <= 6; t++) { g.turn = t; endPhase(g, 'NED'); }
    expect(c.dug).toBe(true);
    expect(c.dugFor!).toBeGreaterThanOrEqual(3);
    relocate(g, c, g.byName['Batavia']);
    for (let t = 7; t <= 9; t++) { g.turn = t; endPhase(g, 'NED'); }
    expect(!!c.dug).toBe(false);
  });
});

// (The historian's kwangsi-landing-and-garrison-totals.md, section 1, accepted as data by the design director at 23:21Z
// of 20 November 2026. The record's landing in Qinzhou Bay of 15 November 1939 met a few thousand provincial troops
// and militia, about 0.1 to 0.2 with no works of note; the row stood at 0.4 and dug in whole by the rule.)
describe("Pakhoi's garrison stands at the record's weight with no works", () => {
  const PAKHOI = 'Southern Kwangtung garrison';
  it('the row: 0.15 with a cap, held, and no works at Pakhoi', () => {
    expect(first(PAKHOI).slice(0, 5)).toEqual(['CHN', PAKHOI, 'inf', 1, 'Pakhoi']);
    expect(first(PAKHOI)[5]).toMatchObject({ str: 0.15, est: 0.15, hold: 1, works: ['Pakhoi', 0] });
  });

  it('it comes at that weight, never passes it and never digs in at its post', () => {
    const g = war();
    const c = of(g, PAKHOI)!;
    expect([g.regions[c.reg].name, c.str, c.ownWorks]).toEqual(['Pakhoi', 0.15, [g.byName['Pakhoi'].id, 0]]);
    for (let t = 1; t <= 6; t++) { g.turn = t; startPhase(g, 'CHN'); endPhase(g, 'CHN'); }
    expect(c.str).toBeLessThanOrEqual(0.15);
    expect([!!c.dug, c.dugFor]).toEqual([false, undefined]);
    // (A plain garrison row of China's digs in whole in the same months, as the rule has it.)
    const plain = of(g, 'East River garrison (Waichow)')!;
    expect([plain.ownWorks, plain.dug]).toEqual([undefined, true]);
  });
});

describe("will is a row's own where the record gives it: where its morale settles, in place of the common level", () => {
  it('the rows: the KNIL one band of the game\'s words below the common level, Sumatra and Lark Force a band and a half, the steady rows none', () => {
    expect(MORALE.level).toBe(0.5);
    for (const nm of ['KNIL 1st Division (West Java)', 'KNIL 2nd Division (Central Java)', 'KNIL 3rd Division (East Java)', 'Palembang garrison (the oil)', 'Borneo garrisons (Tarakan and Balikpapan)', 'Great East garrisons (Ambon, Celebes, Timor)']) expect(first(nm)[5].will, nm).toBe(0.3);
    expect(dated('expeditions', MILITIA)[6].will).toBe(0.3);
    expect(first('Sumatra garrisons (Medan and Atjeh)')[5].will).toBe(0.2);
    expect(dated('formations', 'Lark Force (2/22nd Battalion)')[6].will).toBe(0.2);
    for (const nm of ['Gull Force (2/21st Battalion)', 'Blackforce (2/3rd Machine Gun and 2/2nd Pioneer Battalions)', '2nd Battalion, 131st Field Artillery (United States)', 'Sparrow Force (2/40th Battalion)']) expect(dated('expeditions', nm)[6].will, nm).toBeUndefined();
    expect(dated('formations', 'Wake detachment, 1st Defense Battalion')[6].will).toBeUndefined();
  });

  it('a row with a will starts as far above it as any fresh corps is above the common level, and settles as far below a plain row as its will is', () => {
    const g = war();
    const low = addCorps(g, 'UK', 'A row with a will of its own', 'inf', 1, 'Singapore', { will: 0.3 }), plain = addCorps(g, 'UK', 'A plain row', 'inf', 1, 'Singapore', {});
    expect([low.will, low.mor, plain.will, plain.mor]).toEqual([0.3, 0.4, undefined, MORALE.start]);
    for (let t = 2; t <= 30; t++) { g.turn = t; for (const c of [low, plain]) moraleMonth(g, c, { owed: 0, cold: false }); }
    // (The months' own gains are the same for both: they stand together.)
    expect(plain.mor! - low.mor!).toBeCloseTo(0.2, 2);
  });
});

describe('the KNIL\'s militia is Dutch, and changes flag with the rest on the fall of the Netherlands, never on a date', () => {
  it('the row is the Netherlands\' own, a levy of its place, in the month it always came', () => {
    const row = dated('expeditions', MILITIA);
    expect([row[0], row[1], row[5], row[6].levy, row[6].hold, row[6].str, row[6].est]).toEqual([10, 'NED', 'Bandoeng', 1, 1, 0.4, 0.4]);
  });

  it('while the Netherlands stands it comes as Dutch, Britain raises none, and at the fall it passes into keeping with the rest', () => {
    const g = war();
    g.turn = 10; startPhase(g, 'UK'); startPhase(g, 'NED');
    const c = of(g, MILITIA)!;
    expect([c.nat, c.raised, g.regions[c.reg].name, c.hold]).toEqual(['NED', undefined, 'Bandoeng', true]);
    expect(g.corps.filter((x) => x.name === MILITIA)).toHaveLength(1);
    g.turn = 14;
    declareWar(g, 'GER', 'NED');
    capitulate(g, 'NED', 'GER');
    expect([c.dead, c.nat, c.raised]).toEqual([false, 'UK', 'NED']);
    expect(of(g, 'KNIL 1st Division (West Java)')!.nat).toBe('UK');
    // It came once: the keeper is sent no second one.
    g.turn = 15; startPhase(g, 'UK');
    expect(g.corps.filter((x) => x.name === MILITIA && !x.dead)).toHaveLength(1);
  });

  it('where the Netherlands has fallen before its month, it comes under the flag Java is kept under, marked Dutch-raised, with its own works and will', () => {
    const g = war();
    g.turn = 9;
    declareWar(g, 'GER', 'NED');
    capitulate(g, 'NED', 'GER');
    expect([g.nations.NED.alive, g.byName['Bandoeng'].ctrl]).toEqual([false, 'UK']);
    startPhase(g, 'UK');
    expect(of(g, MILITIA)).toBeUndefined();
    g.turn = 10; startPhase(g, 'UK');
    const c = of(g, MILITIA)!;
    expect([c.nat, c.raised, g.regions[c.reg].name, c.hold, c.str, c.full, c.will]).toEqual(['UK', 'NED', 'Bandoeng', true, 0.4, 0.4, 0.3]);
    expect(c.ownWorks).toEqual([g.byName['Bandoeng'].id, 0]);
    g.turn = 11; startPhase(g, 'UK');
    expect(g.corps.filter((x) => x.name === MILITIA && !x.dead)).toHaveLength(1);
  });
});
