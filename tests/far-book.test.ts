// The book's dated attacks, strength by month and tank rows (ruling 234, riders b573 to b583; src/campaign/stations.ts).
//
// By the book, computer against computer, at a stop that keeps it and while the Pacific's book is open: the
// invader's staff makes each of the record's big attacks on its month; a row's strength is cut to the record's
// worth for the month and never raised; a dated attack fights the part of each side that met in the record's fight;
// Japan's tank regiments go with their divisions. Nothing about a result is written: the fight is the game's own.
// With the far side's switch off none of it exists (tests/pacific-war-off.test.ts).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { addCorps, assess, capitulate, createGame, declareWar, removeCorps, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import { bookAttacks, bookFleets, bookStations, bookWatch, ceilinged, stations, type Attack } from '../src/campaign/stations';
import { MEN_PER_DIV, type BattleReport, type Corps, type Game } from '../src/campaign/types';

const war = (stop: number, human: string | null = null): Game => {
  const g = startOpen(createGame(3, human));
  if (stop !== 3) setStop(g, stop, true);
  return g;
};
const at = (g: Game, c: Corps): string => g.regions[c.reg].name;
const row = (g: Game, nat: string, name: string, place: string, divs = 1, o: object = {}): Corps => addCorps(g, nat, name, 'inf', divs, place, o);
/** Japan's blow has fallen in its month. */
const strike = (g: Game, month = 28): void => { g.flags['st:done:JAP:south'] = month; for (const e of ['USA', 'UK']) declareWar(g, 'JAP', e); };
const worth = (c: Corps): number => c.divs * c.str;
const G5 = '25th Army (5th Division)', TANK1 = '1st Tank Regiment', IND11 = '11th Indian Division (6th and 15th Brigades)';
const GUARDS = '25th Army (Imperial Guards Division)', AUS8 = '8th Australian Division (22nd Brigade)';
const attack = (name: string): Attack => stations()!.attacks!.find((a) => a.name === name)!;
const index = (name: string): number => stations()!.attacks!.findIndex((a) => a.name === name);

/** What the book told of its attacks while a test ran. */
type Told = { name: string; what: string; why?: string; taken?: boolean; report?: BattleReport };
const told: Told[] = [];
const listen = (): void => { told.length = 0; bookWatch.tell = (e) => { told.push({ name: e.row.name, what: e.what, why: e.why, taken: e.taken, report: e.report }); }; };
afterEach(() => { bookWatch.tell = null; });
const made = (name: string): Told[] => told.filter((t) => t.name === name && t.what === 'made');

/** A corps of the table put down by hand at a strength. */
const put = (g: Game, nat: string, name: string, place: string, str: number, o: { kind?: 'inf' | 'arm'; xp?: number; divs?: number } = {}): Corps => {
  const c = addCorps(g, nat, name, o.kind ?? 'inf', o.divs ?? 1, place, { xp: o.xp ?? 0.3 });
  c.str = str;
  return c;
};
/** Jitra's field in December 1941: the 5th Division and its tank regiment at Singora, the 11th Indian at Penang. */
const jitra = (o: { japan?: number; tank?: number; india?: number } = {}): { g: Game; d5: Corps; tank: Corps; ind: Corps } => {
  const g = war(1);
  const d5 = put(g, 'JAP', G5, 'Singora', o.japan ?? 0.9, { xp: 0.6 });
  const tank = put(g, 'JAP', TANK1, 'Singora', o.tank ?? 0.25, { kind: 'arm', xp: 0.6 });
  const ind = put(g, 'UK', IND11, 'Penang', o.india ?? 1, { xp: 0.05 });
  g.turn = 28;
  strike(g);
  for (const nat of ['UK', 'JAP']) bookStations(g, nat);
  return { g, d5, tank, ind };
};
const menOf = (us: { men: number }[]): number => us.reduce((s, u) => s + u.men, 0);

const lost = (rep: BattleReport, c: Corps): number => [...rep.attackers, ...rep.defenders].find((u) => u.name === c.name)!.lost;

describe("the book's dated attacks (ruling 234, riders b573 to b583)", () => {
  it("fights the part of each side that met in the record's fight, and what it costs falls on that part (rider b580)", () => {
    const { g, d5, tank, ind } = jitra();
    expect(attack('Jitra').met).toEqual([0.35, 0.55]);
    listen();
    bookAttacks(g, 'JAP');
    const rep = made('Jitra')[0].report!;
    // The 5th Division and its tanks are 1.15 and go in as 0.35; the 11th Indian is 1.0 and meets them as 0.55.
    expect(Math.abs(menOf(rep.attackers) - 0.35 * MEN_PER_DIV)).toBeLessThanOrEqual(2);
    expect(Math.abs(menOf(rep.defenders) - 0.55 * MEN_PER_DIV)).toBeLessThanOrEqual(1);
    expect(rep.attackers.map((u) => u.name).sort()).toEqual([TANK1, G5].sort());
    // Every corps comes out whole in its divisions, less only the men its part lost.
    expect([d5.divs, tank.divs, ind.divs, d5.bookPart, tank.bookPart, ind.bookPart]).toEqual([1, 1, 1, undefined, undefined, undefined]);
    for (const [c, was] of [[d5, 0.9], [tank, 0.25], [ind, 1]] as [Corps, number][]) {
      expect(lost(rep, c)).toBeGreaterThan(0);
      expect(Math.abs((was - c.str) * c.divs * MEN_PER_DIV - lost(rep, c))).toBeLessThan(2);
    }
    // A side with less than the record's figure fights whole, and never as more than it is.
    const thin = jitra({ india: 0.3 });
    listen();
    bookAttacks(thin.g, 'JAP');
    expect(menOf(made('Jitra')[0].report!.defenders)).toBe(Math.round(0.3 * MEN_PER_DIV));
  });

  it("a fight at the record's figures that is lost stays owed and is asked once a month by the book alone (the staff's own odds on later tries are in far-book-owed.test.ts); one the record called off is over after its last month (rider b580)", () => {
    const { g } = jitra({ japan: 0.1, tank: 0.05 });
    const i = index('Jitra');
    listen();
    bookAttacks(g, 'JAP');
    expect(made('Jitra').map((t) => t.taken)).toEqual([false]);
    expect([g.flags[`ba:${i}`], g.flags[`bat:${i}`]]).toEqual([undefined, 28]);
    // The staff's month has a second asking (ai.ts): the lost fight is not tried again in it.
    bookAttacks(g, 'JAP');
    expect(made('Jitra').length).toBe(1);
    g.turn = 29;
    bookAttacks(g, 'JAP');
    expect(made('Jitra').length).toBe(2);
    // The Sittang's fight is asked in February and no later.
    expect(attack('The Bilin and the Sittang').until).toBe(30);
    g.turn = 31;
    listen();
    bookAttacks(g, 'JAP');
    expect(told.find((t) => t.name === 'The Bilin and the Sittang')).toMatchObject({ what: 'over', why: "the record's attack was called off" });
    expect(g.flags[`ba:${index('The Bilin and the Sittang')}`]).toBe(31);
  });

  it("won, the broken line stands where the record had it next: Kampar's prepared works after Jitra, its row's next step after Muar (rider b580)", () => {
    const { g, ind } = jitra({ india: 0.3 });
    // (Kampar is asked in the same month: it is set aside here, so that Jitra's own result is read.)
    g.flags[`ba:${index('Kampar')}`] = 28;
    listen();
    bookAttacks(g, 'JAP');
    expect(made('Jitra').map((t) => t.taken)).toEqual([true]);
    expect(g.flags[`ba:${index('Jitra')}`]).toBe(28);
    expect(attack('Jitra').back).toEqual(['Ipoh', 3]);
    expect([at(g, ind), ind.bookWorks, ind.dug, ind.dugFor, ind.hold]).toEqual(['Ipoh', 3, true, 3, true]);
    expect(g.flags[`bk:b:${ind.id}`]).toBe(g.byName.Ipoh.id + 1);
    // The month's second reading of the stations leaves it in those works; January's step is its row's own.
    bookStations(g, 'UK');
    expect([at(g, ind), ind.bookWorks]).toEqual(['Ipoh', 3]);
    g.turn = 29;
    bookStations(g, 'UK');
    expect([at(g, ind), ind.bookWorks, g.flags[`bk:b:${ind.id}`]]).toEqual(['Kuala Lumpur', 1, undefined]);

    // Muar: the Guards and their tanks break the Australians in Johore, who are on the island a month before their row has them.
    const m = war(1);
    const guards = put(m, 'JAP', GUARDS, 'Kuala Lumpur', 1, { xp: 0.9 }), t14 = put(m, 'JAP', '14th Tank Regiment', 'Kuala Lumpur', 0.2, { kind: 'arm', xp: 0.9 });
    const aus = put(m, 'UK', AUS8, 'Johore', 0.25, { xp: 0 });
    m.byName['Kuala Lumpur'].ctrl = 'JAP';
    m.turn = 28; strike(m); bookStations(m, 'UK');
    m.turn = 29; for (const nat of ['UK', 'JAP']) bookStations(m, nat);
    expect([at(m, aus), aus.bookWorks]).toEqual(['Johore', 1]);
    listen();
    bookAttacks(m, 'JAP');
    expect(made('Gemas and Muar').map((t) => t.taken)).toEqual([true]);
    expect(Math.abs(menOf(made('Gemas and Muar')[0].report!.attackers) - 0.65 * MEN_PER_DIV)).toBeLessThanOrEqual(2);
    expect([at(m, guards), at(m, t14)]).toEqual(['Johore', 'Johore']);
    expect([at(m, aus), aus.bookWorks, !!aus.dug]).toEqual(['Singapore', 0, false]);
  });

  it('marches in where nobody stands in the field, as a landing on an empty shore does (rider b580)', () => {
    const { g, d5, tank, ind } = jitra();
    removeCorps(g, ind);
    g.flags[`ba:${index('Kampar')}`] = 28;
    listen();
    bookAttacks(g, 'JAP');
    expect(made('Jitra').map((t) => [t.taken, t.report])).toEqual([[true, undefined]]);
    expect([at(g, d5), at(g, tank), g.flags[`ba:${index('Jitra')}`]]).toEqual(['Penang', 'Penang', 28]);
  });

  it("a landing with the record's figures meets its fight inland: no cut for the shore and the shore works aside; lost, it is back at its port and owed (rider b580)", () => {
    const name = 'Java: Bantam Bay and Merak', i = index(name);
    // Java in March 1942, the Indies in Britain's keeping as the book has them, West Java's rows on the month's step.
    // (`stood`: a corps the record did not have at Batavia, so that the landing's own fight can be read.)
    const java = (stood: number): { g: Game; d2: Corps; knil: Corps } => {
      const g = war(1);
      declareWar(g, 'GER', 'NED');
      capitulate(g, 'NED', 'GER');
      const d2 = put(g, 'JAP', '16th Army (2nd Division)', 'Cam Ranh', 1);
      const knil = g.corps.find((c) => c.name === 'KNIL 1st Division (West Java)')!;
      strike(g);
      g.byName['Cam Ranh'].ctrl = 'JAP';
      g.turn = 31;
      bookStations(g, 'UK');
      if (stood) put(g, 'UK', 'A corps the record did not have at Batavia', 'Batavia', stood);
      return { g, d2, knil };
    };
    // The row keeps Leuwiliang's figures by the historian's card: 0.25 of the 2nd Division against 0.3.
    expect([attack(name).sea, attack(name).met]).toEqual([true, [0.25, 0.3]]);
    const { g, d2 } = java(0.1);
    const shore = g.byName.Batavia;
    expect(shore.fort).toMatchObject({ lv: 1, vs: ['JAP'] });
    listen();
    bookAttacks(g, 'JAP');
    const t = made(name)[0], rep = t.report!;
    // What stands there is under the record's 0.3 and fights whole; the division fights as no more than the 0.25 that met.
    expect(menOf(rep.defenders)).toBe(Math.round(0.1 * MEN_PER_DIV));
    expect(Math.abs(menOf(rep.attackers) - 0.25 * MEN_PER_DIV)).toBeLessThanOrEqual(2);
    const line = (label: string): string => { const l = rep.lines.find((x) => x.label === label); return l ? `${l.value} ${l.effect}` : ''; };
    expect(line('Landing')).toContain('100%');
    expect(line('Fortifications')).toBe('');
    expect(shore.fort).toMatchObject({ lv: 1, vs: ['JAP'] });
    expect([t.taken, at(g, d2), d2.hold, g.flags[`ba:${i}`], g.flags[`bat:${i}`]]).toEqual([true, 'Batavia', false, 31, undefined]);
    // Against more than the record met there the same landing is thrown back: the division is at its quay, and the attack is owed for another month.
    const strong = java(2);
    listen();
    bookAttacks(strong.g, 'JAP');
    expect(Math.abs(menOf(made(name)[0].report!.defenders) - 0.3 * MEN_PER_DIV)).toBeLessThanOrEqual(2);
    expect([made(name)[0].taken, at(strong.g, strong.d2), strong.g.flags[`ba:${i}`], strong.g.flags[`bat:${i}`]]).toEqual([false, 'Cam Ranh', undefined, 31]);
  });

  it('Batavia is given up, not lost: West Java\'s rows are on the plateau in the month of the landing, before it is fought, and the 2nd Division lands on an empty province (rider b621)', () => {
    const name = 'Java: Bantam Bay and Merak', i = index(name);
    const west = ['KNIL 1st Division (West Java)', 'Blackforce (2/3rd Machine Gun and 2/2nd Pioneer Battalions)', '2nd Battalion, 131st Field Artillery (United States)'];
    // The table: each of the three leaves Batavia for Bandoeng in March 1942 (turn 31), in the open; the militia is there in its works already.
    expect(west.map((n) => stations()!.rows.find((r) => r.corps === n)!.steps.at(-1))).toEqual([[31, 'Bandoeng', 0], [31, 'Bandoeng', 0], [31, 'Bandoeng', 0]]);
    const g = war(1);
    declareWar(g, 'GER', 'NED');
    capitulate(g, 'NED', 'GER');
    const d2 = put(g, 'JAP', '16th Army (2nd Division)', 'Cam Ranh', 0.9);
    const stand = west.map((n, k) => g.corps.find((c) => c.name === n) ?? put(g, 'UK', n, 'Batavia', [0.5, 0.2, 0.1][k]));
    strike(g);
    g.byName['Cam Ranh'].ctrl = 'JAP';
    // February: the West Group is in Batavia still.
    g.turn = 30;
    bookStations(g, 'UK');
    expect(stand.map((c) => at(g, c))).toEqual(['Batavia', 'Batavia', 'Batavia']);
    // March: Britain's month comes before Japan's, and the three are on the plateau when the landing is made.
    g.turn = 31;
    bookStations(g, 'UK');
    expect(stand.map((c) => [at(g, c), c.hold, c.bookWorks])).toEqual([['Bandoeng', true, 0], ['Bandoeng', true, 0], ['Bandoeng', true, 0]]);
    listen();
    bookAttacks(g, 'JAP');
    // No fight is told: the division marches in, and the row is made in its month.
    expect(made(name).map((t) => [t.taken, t.report])).toEqual([[true, undefined]]);
    expect([at(g, d2), g.byName.Batavia.ctrl, g.flags[`ba:${i}`], g.flags[`bat:${i}`]]).toEqual(['Batavia', 'JAP', 31, undefined]);
    expect(stand.map((c) => [at(g, c), c.dead ?? false])).toEqual([['Bandoeng', false], ['Bandoeng', false], ['Bandoeng', false]]);
  });

  it("is made computer against computer: never on ground a player holds, and not at all for a player's Japan or where history is loose", () => {
    const mine = war(1, 'UK');
    put(mine, 'JAP', G5, 'Singora', 0.9);
    const ind = put(mine, 'UK', IND11, 'Penang', 1);
    mine.turn = 28; strike(mine);
    listen();
    bookAttacks(mine, 'JAP');
    expect(told.find((t) => t.name === 'Jitra')).toMatchObject({ what: 'owed', why: 'the player holds it' });
    expect(ind.str).toBe(1);
    for (const g of [war(1, 'JAP'), war(3)]) {
      put(g, 'JAP', G5, 'Singora', 0.9);
      put(g, 'UK', IND11, 'Penang', 1);
      g.turn = 28; strike(g);
      listen();
      bookAttacks(g, 'JAP');
      expect(told).toEqual([]);
    }
  });
});

describe("the record's strength by month (ruling 234, riders b573 and b579)", () => {
  it('cuts a row to the record\'s worth for the month and never raises it; a worth of nothing is a command that is gone', () => {
    const g = war(1);
    const nlf = put(g, 'USA', 'North Luzon Force', 'Lingayen', 0.8, { divs: 4 });
    const div = g.corps.find((c) => c.nat === 'USA' && c.name === 'Philippine Division')!;
    const b45 = put(g, 'UK', '45th Indian Infantry Brigade', 'Johore', 0.3);
    const own = worth(div);
    expect(own).toBeLessThan(0.64);
    // Before the war the table gives no figure: nothing is cut.
    g.turn = 28;
    for (const nat of ['USA', 'UK']) bookStations(g, nat);
    expect(worth(nlf)).toBeCloseTo(3.2, 6);
    strike(g);
    const read: number[][] = [];
    for (const t of [29, 30, 31, 32]) { g.turn = t; for (const nat of ['USA', 'UK']) bookStations(g, nat); read.push([+worth(nlf).toFixed(2), +worth(div).toFixed(2)]); if (t === 29) expect([b45.dead, at(g, b45)]).toEqual([false, 'Johore']); }
    // The North Luzon Force by the table; the Philippine Division stands under January's 0.64 and is given nothing.
    expect(read).toEqual([[1.82, +own.toFixed(2)], [1.36, 0.48], [0.91, 0.32], [0.45, 0.16]]);
    // The 45th Indian Brigade was destroyed at Muar: in February it is no corps.
    expect(g.corps.includes(b45)).toBe(false);
  });

  it("while the book is open the staff folds no such row into another (rider b579), and Singapore's coast guns stand in no field fight that comes round their works", () => {
    const g = war(1);
    const nlf = put(g, 'USA', 'North Luzon Force', 'Lingayen', 0.8, { divs: 4 }), other = put(g, 'USA', 'A corps the table does not name', 'Manila', 0.5);
    const d5 = put(g, 'JAP', G5, 'Johore', 0.9);
    g.byName.Johore.ctrl = 'JAP';
    g.turn = 28; strike(g);
    for (const nat of ['USA', 'UK']) bookStations(g, nat);
    const fort = g.corps.find((c) => c.nat === 'UK' && c.name === 'Singapore Fortress (the coast guns)')!;
    expect([ceilinged(g, nlf), ceilinged(g, other), fort.bookCoast]).toEqual([true, false, true]);
    const island = g.byName.Singapore;
    expect(island.fort!.vs.includes(g.byName.Johore.owner)).toBe(false);
    const out = (o: object = {}): boolean => assess(g, [d5], island, o).lines.some((l) => l.label === 'Coast defences');
    expect(out()).toBe(true);
    // From the sea the guns are the shore's own.
    expect(out({ sea: 1 })).toBe(false);
    // The book closes: the rows are the staff's again, and the fortress is a corps like any other.
    g.flags['hist:off:pacific'] = 30;
    for (const nat of ['USA', 'UK']) bookStations(g, nat);
    expect([ceilinged(g, nlf), fort.bookCoast, out()]).toEqual([false, undefined, false]);
    const loose = war(3);
    expect(ceilinged(loose, put(loose, 'USA', 'North Luzon Force', 'Lingayen', 0.8))).toBe(false);
  });
});

describe("Japan's tank regiments by the book (ruling 234, riders b581 and b583)", () => {
  it("go with their divisions while the book is open, and are the staff's own rows when it closes", () => {
    const g = war(1);
    const d5 = put(g, 'JAP', G5, 'Singora', 0.9), tank = put(g, 'JAP', TANK1, 'Hainan', 0.25, { kind: 'arm' });
    expect(stations()!.rows.find((r) => r.corps === TANK1)).toMatchObject({ beside: [[28, G5], [32, '15th Army (33rd and 55th Divisions)']] });
    // The eve: it waits where it formed, held for the book.
    g.turn = 27;
    bookStations(g, 'JAP');
    expect([at(g, tank), tank.hold, ceilinged(g, tank)]).toEqual(['Hainan', true, true]);
    g.turn = 28; strike(g);
    bookStations(g, 'JAP');
    expect([at(g, tank), tank.hold, tank.stay, d5.hold]).toEqual(['Singora', true, true, false]);
    // From April 1942 the regiment is the 15th Army's, in Burma.
    const a15 = put(g, 'JAP', '15th Army (33rd and 55th Divisions)', 'Bangkok', 1, { divs: 2 });
    g.turn = 32;
    bookStations(g, 'JAP');
    expect(at(g, tank)).toBe(at(g, a15));
    g.flags['hist:off:pacific'] = 32;
    bookStations(g, 'JAP');
    expect([at(g, tank), tank.hold, tank.stay, tank.bookWorks, ceilinged(g, tank)]).toEqual(['Bangkok', false, undefined, undefined, false]);
  });

  it("are facts of the day and come at every stop (riders b591 and b604); the rows the record adds to the defence are rows of stops 1 and 2 only (rider b579, reading 4)", () => {
    const names = ['18th British Division', '45th Indian Infantry Brigade', TANK1];
    const formed = (stop: number): string[] => {
      const g = war(stop);
      g.turn = 30; strike(g);
      for (const nat of ['UK', 'JAP']) startPhase(g, nat);
      return names.filter((nm) => g.corps.some((c) => c.name === nm && !c.dead));
    };
    expect(formed(1)).toEqual(names);
    expect(formed(2)).toEqual(names);
    expect(formed(3)).toEqual([TANK1]);
  });
});

describe("the record's covering fleets (ruling 234, riders b575 and b577)", () => {
  it('stand in the Java Sea for their months from a port that reaches it and go home the month after; the China Fleet goes to Ceylon once', () => {
    const g = war(1);
    const f3 = g.fleets.find((f) => f.nat === 'JAP' && f.name === '3rd Fleet')!, china = g.fleets.find((f) => f.nat === 'UK' && f.name === 'China Fleet')!;
    const home = g.regions[f3.base].name;
    // (Rider b637: the China Fleet's first-day carrier is the Eagle, who leaves for the Mediterranean in May 1940, ruling
    // 233 (f). Indomitable is in no row before March 1942, so the fleet has no carrier in between: tests/far-on.test.ts.)
    expect([g.regions[china.base].name, china.ships.cv]).toEqual(['Singapore', 1]);
    for (const nm of ['Balikpapan', 'Cam Ranh']) g.byName[nm].ctrl = 'JAP';
    // Before the blow the book gives no admiral an order.
    g.turn = 30;
    expect(bookFleets(g, 'JAP')).toEqual([]);
    strike(g);
    // January is the second ring's month (rider b599): the carriers and the 4th Fleet cover Rabaul from Truk. The Java
    // Sea's cover is not ordered before its own month, and the Molucca and Makassar rows wait on Davao or Jolo.
    g.turn = 29;
    const ring = [{ fleet: 'Mobile Fleet', zone: 'bismarck', stance: 'patrol' }, { fleet: '4th Fleet', zone: 'bismarck', stance: 'patrol' }];
    expect(bookFleets(g, 'JAP')).toEqual(ring);
    g.turn = 30;
    const cover = [{ fleet: '3rd Fleet', zone: 'java', stance: 'patrol' }, { fleet: 'Southern Expeditionary Fleet', zone: 'java', stance: 'patrol' }];
    expect(bookFleets(g, 'JAP')).toEqual(cover);
    expect([g.regions[f3.base].name, !!f3.bookBase]).toEqual(['Balikpapan', true]);
    expect(bookFleets(g, 'UK')).toEqual([]);
    expect(g.regions[china.base].name).toBe('Colombo');
    // March: the Java Sea's cover stands a second month, and the 4th Fleet covers Lae and Salamaua on the Solomon Sea.
    g.turn = 31;
    expect(bookFleets(g, 'JAP')).toEqual([...cover, { fleet: '4th Fleet', zone: 'solomon', stance: 'patrol' }]);
    g.turn = 32;
    expect(bookFleets(g, 'JAP')).toEqual([]);
    expect([g.regions[f3.base].name, f3.bookBase]).toEqual([home, undefined]);
    // The move to Ceylon is made once: brought back by its admiral, the fleet is not sent again.
    china.base = g.byName.Singapore.id;
    bookFleets(g, 'UK');
    expect(g.regions[china.base].name).toBe('Singapore');
  });
});
