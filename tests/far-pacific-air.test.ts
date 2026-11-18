// The Pacific's air forces with the far side's switch on (rulings 184 and 187; the Pacific planner's paper 10 and its
// pacific-air-rows.json, the historian's rows). The commands laid by date, on an ally's ground and stood down as the
// record has them; the record's forward bases; Japan's air industry on Europe's scale and its quality by date; and
// the air over a far sea, counted from the land groups in reach of it and no longer from who holds the coast. The
// states are set by hand on the first day's chart.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import farAir from '../data/campaign/far-air.json';
import { AIR, AIRWAR, airAI, airPool, airQuality, airRange, airUpkeep, cover, crowding, groupOverSea, groupsOver, seaKm, whyNotMission } from '../src/campaign/air';
import { ARSENAL, smallAirMonth } from '../src/campaign/arsenal';
import { monthlyDiplomacy } from '../src/campaign/diplomacy';
import { allied, atWar, capitulate, capture, createGame, declareWar, removeCorps, setWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { NAVY, SeaPicture, airOver, closedTo, openSeas, zoneOf } from '../src/campaign/navy';
import { PACIFIC, skiesOn, skiesOver } from '../src/campaign/pacific';
import { startOpen } from '../src/campaign/staff';
import { techOf } from '../src/campaign/tech';
import { beyondEurope } from '../src/campaign/world';
import type { AirFleet, Game } from '../src/campaign/types';

type Row = { nat: string; name: string; base: string; from?: number; est: [number, number, number][]; moves?: [number, string][]; posted?: boolean; ally?: boolean; arrives?: boolean; byBook?: boolean; watch?: string; quality?: number; bmrRange?: number; home?: boolean };
const ROWS = (farAir as unknown as { air: { fleets: Row[] } }).air.fleets;

/** A grand campaign on the whole world at a stop of the dial, moved by hand to a month of the war. */
const war = (stop: number, turn: number, human: string | null = null, seed = 3): Game => {
  const g = startOpen(createGame(seed, human));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = turn;
  return g;
};
/** The same, with Japan at war with America, Britain, the Dutch and the dominions as the strike south leaves it. */
const pacificWar = (stop: number, turn: number, human: string | null = null, seed = 3): Game => {
  const g = war(stop, turn, human, seed);
  for (const e of ['USA', 'UK', 'NED', 'AUS', 'NZL']) if (!atWar(g, 'JAP', e)) declareWar(g, 'JAP', e);
  g.turn = turn;
  return g;
};
/** The groups of one command, under the country that raised it. */
const command = (g: Game, nat: string, name: string): AirFleet[] => g.airFleets.filter((f) => (f.raised ?? f.nat) === nat && (f.parent ?? f.name) === name && f.ship === undefined);
const sum = (fs: AirFleet[]): [number, number] => [fs.reduce((s, f) => s + f.ftr, 0), fs.reduce((s, f) => s + f.bmr, 0)];
const bases = (g: Game, fs: AirFleet[]): string[] => [...new Set(fs.map((f) => g.regions[f.base].name))];
/** A province changes hands a month ago, by hand: whoever stood there is gone, town and field. */
const take = (g: Game, name: string, nat: string): void => {
  const r = g.byName[name];
  for (const c of g.corps.filter((c) => c.reg === r.id && !c.dead && c.nat !== nat)) removeCorps(g, c);
  capture(g, r, nat);
  r.since = g.turn - 1;
};

describe('the Pacific\'s air commands (rulings 184 and 187)', () => {
  it('lays the rows: forty-nine commands on the chart, Japan\'s air industry on Europe\'s scale, and nothing of Europe\'s moved', () => {
    expect(ROWS.length).toBe(49);
    const count = (nat: string): number => ROWS.filter((r) => r.nat === nat).length;
    expect(['JAP', 'USA', 'UK', 'NED', 'AUS', 'NZL', 'CHN', 'SOV'].map(count)).toEqual([15, 15, 4, 1, 5, 2, 2, 5]);
    const g = war(1, 1);
    for (const r of ROWS) {
      expect(AIR.fleets.filter((x) => x.nat === r.nat && x.name === r.name).length, r.name).toBe(1);
      expect(g.byName[r.base], `${r.name} base`).toBeDefined();
      for (const [turn, name] of r.moves ?? []) { expect(g.byName[name], `${r.name} move`).toBeDefined(); expect(turn, r.name).toBeGreaterThan(r.from ?? 1); }
    }
    expect(AIR.nations.JAP).toEqual({ quality: 1, ftrRange: 300, bmrRange: 1300 });
    // Ruling 187(c): history's rows until January 1942, then held where Europe's rows stop (Germany's 360 over the 1.83 between them in 1944).
    expect(AIR.production.JAP).toEqual([[1, 60, 80], [17, 90, 120], [29, 197, 160]]);
    expect(AIR.pilots.perMonth.JAP).toEqual([[1, 60], [29, 100]]);
    expect(AIR.reserve.JAP).toEqual({ ftr: 250, bmr: 250, pilots: 150 });
    expect(AIR.decline!.JAP).toEqual([[1, 1], [41, 0.9], [58, 0.75], [65, 0.65]]);
    // Europe's rows and America's nation row stand as they were.
    expect(AIR.production.GER).toEqual([[1, 180, 280], [9, 250, 300], [16, 300, 320], [22, 360, 350]]);
    expect(AIR.nations.USA).toEqual({ quality: 1, ftrRange: 300, bmrRange: 1100 });
    expect(AIR.fleets.length).toBe(36 + 49);
    // The script's marks: who may stand on an ally's ground, whose aircraft arrive with the date, the commands' own figures.
    const row = (name: string): Row => ROWS.find((r) => r.name === name)!;
    expect(row('Fifth Air Force')).toMatchObject({ base: 'Townsville', ally: true, quality: 0.9, bmrRange: 1200 });
    expect(row('Hawaiian Air Force').ally).toBe(true); // (its bombers go forward to the Ellice Islands, Britain's)
    expect(row('Far East Air Force').ally).toBeUndefined();
    expect(row('XXI Bomber Command').bmrRange).toBe(2550);
    expect(row('Far East Command').quality).toBe(0.75);
    expect(row('Army Air Arm of the Indies')).toMatchObject({ quality: 0.75, arrives: true });
    // (Ruling 234's rider b518, 7: China, New Zealand and the Dutch build none; Australia's own works keep its commands up.)
    expect(ROWS.filter((r) => r.arrives).map((r) => r.nat).filter((n, i, a) => a.indexOf(n) === i)).toEqual(['NED', 'NZL', 'CHN']);
    expect(AIR.works).toEqual(['AUS']);
    // b518, 2: America's 0.9 is to start, and the volunteer group is a row, not a country.
    expect(AIR.rise).toEqual({ USA: { from: 41, tech: 'escorts' } });
    expect(row('American Volunteer Group').quality).toBe(0.9);
    // b518, 3 (ruling 187(a)): the reach of America's land fighters by the job.
    expect(AIR.jobs).toEqual({ USA: { cover: 550, raid: [[55, 1250]] } });
    // b518, 6: every fall is the record's own move or a battle's work, but the one whose reason lies outside the game; the Soviet Far East stands apart.
    expect(ROWS.filter((r) => !r.byBook).map((r) => r.name)).toEqual(['American Volunteer Group', ...ROWS.filter((r) => r.nat === 'SOV').map((r) => r.name)]);
    expect(ROWS.filter((r) => r.watch).map((r) => `${r.nat} ${r.watch}`).filter((n, i, a) => a.indexOf(n) === i)).toEqual(['SOV JAP']);
    expect(row('Transbaikal air (12th Air Army from August 1942)')).toMatchObject({ base: 'Chita', ally: true, moves: [[71, 'Choibalsan']] });
    expect(ROWS.filter((r) => r.home).map((r) => r.name)).toEqual(['Home defence (Tokyo)', 'Home defence (Kyushu)']);
  });

  it('the first day: the commands the record has in 1939 stand on their fields, Japan\'s in Manchukuo and China too', () => {
    const g = war(1, 1);
    const stands = (nat: string, name: string, at: string, n: [number, number]): void => {
      const fs = command(g, nat, name);
      expect(sum(fs), name).toEqual(n);
      expect(bases(g, fs), name).toEqual([at]);
      // (In the air war a command stands as groups of one kind.)
      for (const f of fs) expect(f.kind, name).toBeDefined();
    };
    stands('JAP', 'Kwantung Army air (2nd Air Group)', 'Hsinking', [150, 200]);
    stands('JAP', 'Army air in China', 'Hankow', [60, 100]);
    stands('JAP', 'Home defence (Tokyo)', 'Tokyo', [100, 50]);
    stands('USA', 'Far East Air Force', 'Lingayen', [30, 15]);
    stands('USA', 'Hawaiian Air Force', 'Honolulu', [60, 30]);
    stands('UK', 'Far East Command', 'Singapore', [0, 50]);
    stands('NED', 'Army Air Arm of the Indies', 'Bandoeng', [30, 60]);
    stands('CHN', 'Chinese Air Force', 'Chungking', [150, 110]);
    stands('SOV', 'Far Eastern Front air (10th Air Army from August 1942)', 'Khabarovsk', [500, 550]);
    stands('SOV', 'Kamchatka air (128th Mixed Air Division)', 'Petropavlovsk', [20, 20]);
    expect(sum([...command(g, 'SOV', 'Primorye air (9th Air Army from August 1942)'), ...command(g, 'SOV', 'Pacific Fleet air')])).toEqual([850, 900]);
    expect(g.byName['Hsinking'].ctrl).toBe('MAN');
    // Nothing dated later stands yet, and every base on the record has its air base.
    for (const r of ROWS) {
      if ((r.from ?? 1) > 1) expect(command(g, r.nat, r.name).length, r.name).toBe(0);
      expect(g.flags[`af:${g.byName[r.base].id}`], r.base).toBe(AIRWAR.fields.seed.base);
    }
  });

  it('a command forms in its month on ground its side holds, an ally\'s where the record bases it there', () => {
    const g = pacificWar(1, 28);
    airUpkeep(g, 'JAP');
    expect(sum(command(g, 'JAP', '11th Air Fleet (21st and 23rd Air Flotillas)'))).toEqual([110, 120]);
    expect(sum(command(g, 'JAP', '5th Air Group'))).toEqual([70, 80]);
    expect(bases(g, command(g, 'JAP', '5th Air Group'))).toEqual(['Formosa']);
    expect(sum(command(g, 'JAP', '24th Air Flotilla'))).toEqual([30, 40]);
    expect(g.flags['air:5th Air Group']).toBe(28);
    expect(g.headlines.some((h) => h.nat === 'JAP' && /^5th Air Group forms in Formosa\.$/.test(h.text ?? h.head ?? ''))).toBe(true);
    // Indochina is still France's here (no demand was made): the two commands the record bases there do not form.
    expect(g.byName['Saigon'].ctrl).not.toBe('JAP');
    expect(command(g, 'JAP', '22nd Air Flotilla').length).toBe(0);
    expect(command(g, 'JAP', '3rd Air Group').length).toBe(0);
    // Nor the commands whose month has not come, nor those on ground Japan has yet to take.
    expect(command(g, 'JAP', '25th Air Flotilla').length).toBe(0);
    take(g, 'Saigon', 'JAP');
    take(g, 'Phnom Penh', 'JAP');
    g.turn = 29;
    airUpkeep(g, 'JAP');
    expect(sum(command(g, 'JAP', '22nd Air Flotilla'))).toEqual([40, 100]);
    expect(bases(g, command(g, 'JAP', '3rd Air Group'))).toEqual(['Phnom Penh']);
    // America's Fifth Air Force forms in Australia once the two stand together, and not before.
    g.turn = 31;
    expect(allied(g, 'USA', 'AUS')).toBe(false);
    airUpkeep(g, 'USA');
    expect(command(g, 'USA', 'Fifth Air Force').length).toBe(0);
    monthlyDiplomacy(g);
    expect(allied(g, 'USA', 'AUS')).toBe(true);
    g.turn = 32;
    airUpkeep(g, 'USA');
    expect(bases(g, command(g, 'USA', 'Fifth Air Force'))).toEqual(['Townsville']);
    expect(sum(command(g, 'USA', 'Fifth Air Force'))).toEqual([80, 60]);
    expect(g.byName['Townsville'].ctrl).toBe('AUS');
  });

  it('a command the record withdrew is stood down, and comes again where the record has it', () => {
    const g = pacificWar(1, 28);
    airUpkeep(g, 'JAP');
    const name = '11th Air Fleet (21st and 23rd Air Flotillas)';
    expect(command(g, 'JAP', name).length).toBeGreaterThan(0);
    g.turn = 31;
    const before = { ...g.airPool.JAP };
    airUpkeep(g, 'JAP');
    expect(command(g, 'JAP', name).length).toBe(0);
    expect(g.flags[`air:${name}`]).toBeUndefined();
    expect(g.headlines.some((h) => h.nat === 'JAP' && new RegExp('^11th Air Fleet .* is stood down').test(h.text ?? h.head ?? ''))).toBe(true);
    // Its aircraft went back to the pool for the commands that took its place (what the reserve holds at most is kept).
    expect(g.airPool.JAP.ftr).toBeGreaterThan(before.ftr);
    expect(g.airPool.JAP.ftr).toBeLessThanOrEqual(AIR.reserve.JAP.ftr);
    // It does not form again while the record keeps it at nothing.
    g.turn = 32;
    airUpkeep(g, 'JAP');
    expect(command(g, 'JAP', name).length).toBe(0);
    // The 4th Air Army: Rabaul in December 1942, stood down in April 1944, and in the Philippines from October.
    take(g, 'Rabaul', 'JAP');
    g.turn = 40;
    airUpkeep(g, 'JAP');
    expect(bases(g, command(g, 'JAP', '4th Air Army'))).toEqual(['Rabaul']);
    g.turn = 56;
    airUpkeep(g, 'JAP');
    expect(command(g, 'JAP', '4th Air Army').length).toBe(0);
    take(g, 'Manila', 'JAP');
    g.turn = 62;
    airUpkeep(g, 'JAP');
    expect(bases(g, command(g, 'JAP', '4th Air Army'))).toEqual(['Manila']);
    expect(command(g, 'JAP', '4th Air Army').reduce((s, f) => s + f.est.ftr, 0)).toBe(250);
    // A campaign's first day never raises a command the record had already withdrawn (none is dated so in the rows: the rule is openWorldAir's).
    expect(ROWS.filter((r) => (r.from ?? 1) <= 1 && r.est[0][1] + r.est[0][2] <= 0)).toEqual([]);
  });

  it('the record\'s forward bases: a computer staff takes each once, at the stops that keep the book, while its side holds the ground', () => {
    const g = pacificWar(1, 28);
    airUpkeep(g, 'JAP');
    const fifth = (): AirFleet[] => command(g, 'JAP', '5th Air Group');
    // January 1942, Lingayen not yet taken: it stays on Formosa, and the move is still open.
    g.turn = 29;
    airUpkeep(g, 'JAP');
    expect(bases(g, fifth())).toEqual(['Formosa']);
    expect(g.flags['air:mv:5th Air Group']).toBeUndefined();
    // Taken this very month: an airfield taken this month is not yet in use.
    take(g, 'Lingayen', 'JAP');
    g.byName['Lingayen'].since = 29;
    airUpkeep(g, 'JAP');
    expect(bases(g, fifth())).toEqual(['Formosa']);
    // (February's forward base is Moulmein, Britain's still: the command waits on Formosa for it. So Lingayen is tried in January.)
    g.byName['Lingayen'].since = 28;
    airUpkeep(g, 'JAP');
    expect(bases(g, fifth())).toEqual(['Lingayen']);
    expect(g.flags['air:mv:5th Air Group']).toBe(29);
    for (const f of fifth()) expect(f.moved).toBe(29);
    expect(g.headlines.some((h) => h.nat === 'JAP' && /^5th Air Group moves up to airfields in Lingayen\.$/.test(h.text ?? h.head ?? ''))).toBe(true);
    // Once taken it is not taken again: the staff may base the groups elsewhere after it.
    for (const f of fifth()) { f.base = g.byName['Formosa'].id; f.moved = 0; }
    g.turn = 29;
    airUpkeep(g, 'JAP');
    expect(bases(g, fifth())).toEqual(['Formosa']);
    // A forward base with no airfield has one laid out as the command arrives (Kota Bharu for the 3rd Air Group).
    take(g, 'Saigon', 'JAP'); take(g, 'Phnom Penh', 'JAP');
    airUpkeep(g, 'JAP');
    const kota = g.byName['Kota Bharu'];
    delete g.flags[`af:${kota.id}`];
    take(g, 'Kota Bharu', 'JAP');
    for (const f of command(g, 'JAP', '3rd Air Group')) f.moved = 0;
    airUpkeep(g, 'JAP');
    expect(bases(g, command(g, 'JAP', '3rd Air Group'))).toEqual(['Kota Bharu']);
    expect(g.flags[`af:${kota.id}`]).toBe(AIRWAR.fields.seed.base);
    // No stop that keeps no book, and no player's command, moves by the record.
    for (const [stop, human] of [[3, null], [1, 'JAP']] as [number, string | null][]) {
      const h = pacificWar(stop, 28, human);
      airUpkeep(h, 'JAP');
      take(h, 'Lingayen', 'JAP');
      h.turn = 29;
      airUpkeep(h, 'JAP');
      expect(bases(h, command(h, 'JAP', '5th Air Group')), `stop ${stop} ${human}`).toEqual(['Formosa']);
    }
  });

  it('a kind the record gives a command for the first time forms its first group of that kind (Far East Command\'s fighters of September 1941)', () => {
    const g = war(1, 24);
    airUpkeep(g, 'UK');
    const fec = (): AirFleet[] => command(g, 'UK', 'Far East Command');
    expect(fec().map((f) => f.kind)).toEqual(['bmr']);
    g.turn = 25;
    g.airPool.UK = { ftr: 500, bmr: 500, pilots: 500, lostFtr: 0, lostBmr: 0, lostPilots: 0 };
    airUpkeep(g, 'UK');
    expect(sum(fec())).toEqual([60, 90]);
    expect(fec().map((f) => f.kind).sort()).toEqual(['bmr', 'ftr']);
    expect(bases(g, fec())).toEqual(['Singapore']);
    const fighters = fec().find((f) => f.kind === 'ftr')!;
    expect(fighters.parent).toBe('Far East Command');
    expect(airQuality(fighters, g)).toBe(0.75);
    expect(g.headlines.some((h) => h.nat === 'UK' && new RegExp(`^${fighters.name} forms in Singapore, a new group of Far East Command\\.$`).test(h.text ?? h.head ?? ''))).toBe(true);
    // Once: the month after nothing forms, and a command that has lost every group of a kind raises none again.
    g.airFleets.splice(g.airFleets.indexOf(fighters), 1);
    for (const t of [26, 29]) { g.turn = t; airUpkeep(g, 'UK'); expect(fec().map((f) => f.kind), `month ${t}`).toEqual(['bmr']); }
  });

  it('posted: while the Pacific\'s book stands a computer staff leaves the record\'s commands on the record\'s fields', () => {
    // Left to itself Japan's air staff flies the commands to the China front, where its main effort and the enemy's
    // aircraft are: the south's landings then go in under no air at all.
    const months = (g: Game): string[] => {
      airUpkeep(g, 'JAP');
      const at = new Map(g.airFleets.filter((f) => f.nat === 'JAP' && f.ship === undefined).map((f) => [f, f.base]));
      for (let i = 0; i < 3; i++) { airAI(g, 'JAP'); g.turn++; for (const f of g.airFleets) f.moved = 0; }
      return [...at].filter(([f, base]) => g.airFleets.includes(f) && f.base !== base).map(([f, base]) => `${f.parent ?? f.name}: ${g.regions[base].name} to ${g.regions[f.base].name}`);
    };
    expect(ROWS.every((r) => r.posted === true)).toBe(true);
    expect(months(pacificWar(1, 28))).toEqual([]);
    expect(months(pacificWar(2, 28))).toEqual([]);
    // No book at the dial's middle stop: the staff bases them as it sees the war.
    const free = months(pacificWar(3, 28));
    expect(free.length).toBeGreaterThan(0);
    expect(free).toContain('5th Air Group: Formosa to Canton');
    // The Pacific's book closed (history has left the record there): the same.
    const closed = pacificWar(1, 28);
    closed.flags['hist:off:pacific'] = 28;
    expect(months(closed)).toEqual(free);
  });

  it('a country that builds no aircraft on these rows: what the record adds to a command arrives with the date', () => {
    const g = pacificWar(1, 39);
    airUpkeep(g, 'NZL');
    const south = (): AirFleet[] => command(g, 'NZL', 'Royal New Zealand Air Force, Solomons');
    expect(sum(south())).toEqual([0, 12]);
    expect(AIR.production.NZL).toBeUndefined();
    // Losses are not made good...
    south().find((f) => f.kind === 'bmr')!.bmr -= 5;
    g.turn = 40;
    airUpkeep(g, 'NZL');
    expect(sum(south())).toEqual([0, 7]);
    // ...but May 1943's 25 and 20 bring twenty-five fighters and eight bombers more.
    g.turn = 44;
    airUpkeep(g, 'NZL');
    expect(sum(south())).toEqual([25, 15]);
    // Japan builds its own: a rise in what a command is kept at is filled from its pool, as Germany's is.
    const j = pacificWar(1, 28);
    airUpkeep(j, 'JAP');
    j.turn = 31;
    j.airPool.JAP = { ftr: 0, bmr: 0, pilots: 0, lostFtr: 0, lostBmr: 0, lostPilots: 0 };
    for (const f of command(j, 'JAP', '11th Air Fleet (21st and 23rd Air Flotillas)')) { f.ftr = 0; f.bmr = 0; }
    airUpkeep(j, 'JAP');
    // (March 1942's 200 and 200 for the 5th Air Group: no more arrives than the month's production and pilots allow.)
    const [ftr] = sum(command(j, 'JAP', '5th Air Group'));
    expect(ftr).toBeGreaterThan(70);
    expect(ftr).toBeLessThan(200);
  });

  it('a fallen country\'s command in another\'s keeping still reads its own row', () => {
    const g = war(1, 9);
    declareWar(g, 'GER', 'NED');
    capitulate(g, 'NED', 'GER');
    const dutch = (): AirFleet[] => command(g, 'NED', 'Army Air Arm of the Indies');
    expect(dutch().length).toBeGreaterThan(0);
    for (const f of dutch()) { expect(f.nat).toBe('UK'); expect(f.raised).toBe('NED'); }
    expect(bases(g, dutch())).toEqual(['Bandoeng']);
    // Its quality and reach are its own, not Britain's.
    for (const f of dutch()) { expect(airQuality(f, g)).toBe(0.75); expect(airRange(f, g).bmr).toBe(AIR.nations._default.bmrRange); }
    // December 1941's 70 and 60: what it is kept at follows the Dutch row, and Britain's works fill it.
    g.turn = 28;
    airUpkeep(g, 'UK');
    expect(dutch().reduce((s, f) => s + f.est.ftr, 0)).toBe(70);
    expect(dutch().reduce((s, f) => s + f.est.bmr, 0)).toBe(60);
    expect(sum(dutch())[0]).toBeGreaterThan(30);
  });

  it('quality: the commands\' own figures, and Japan\'s by date at the stops that keep the book and no others', () => {
    const g = pacificWar(1, 28);
    for (const n of ['JAP', 'USA', 'UK']) airUpkeep(g, n);
    const one = (nat: string, name: string): AirFleet => command(g, nat, name)[0];
    expect(airQuality(one('USA', 'Far East Air Force'), g)).toBe(0.9);
    expect(airRange(one('USA', 'Far East Air Force'), g).bmr).toBe(1200);
    expect(airQuality(one('UK', 'Far East Command'), g)).toBe(0.75);
    expect(airQuality(one('UK', 'Air Headquarters Burma'), g)).toBe(AIR.nations.UK.quality);
    // America's commands at home keep the nation's row.
    expect(airQuality(g.airFleets.find((f) => f.nat === 'USA' && (f.parent ?? f.name) === 'First Air Force')!, g)).toBe(1);
    const jap = one('JAP', '5th Air Group');
    const by = (h: Game, turn: number): number => { const was = h.turn; h.turn = turn; const q = airQuality(jap, h); h.turn = was; return q; };
    expect([28, 40, 41, 57, 58, 65].map((t) => by(g, t))).toEqual([1, 1, 0.9, 0.9, 0.75, 0.65]);
    // Japan's fighters reach as the month allows (ruling 227's dated row), its bombers 1,300 km.
    expect(airRange(jap, g)).toEqual({ ftr: 850, bmr: 1300 });
    // Its deck aircraft with the carriers' own factor: 1.1 in December 1941, and they fall with the rest.
    const wing = g.airFleets.find((f) => f.nat === 'JAP' && f.ship !== undefined)!;
    expect(airQuality(wing, g)).toBeCloseTo(1.1, 9);
    expect((() => { g.turn = 65; const q = airQuality(wing, g); g.turn = 28; return q; })()).toBeCloseTo(0.65 * 1.1, 9);
    // No date makes Japan worse where no book is kept (ruling 187(b)).
    const loose = pacificWar(3, 28);
    expect([28, 41, 58, 65].map((t) => by(loose, t))).toEqual([1, 1, 1, 1]);
  });
});

describe('the director\'s corrections (ruling 234\'s rider b518)', () => {
  it('2: America\'s 0.9 is to start: the nation\'s from January 1943 by the book, from Long-range fighters where no book is kept', () => {
    const g = pacificWar(1, 40);
    airUpkeep(g, 'USA');
    const haw = command(g, 'USA', 'Hawaiian Air Force')[0];
    expect(airQuality(haw, g)).toBe(0.9);
    g.turn = 41;
    expect(airQuality(haw, g)).toBe(AIR.nations.USA.quality);
    // No book: the month says nothing, the research does.
    const h = pacificWar(3, 60);
    airUpkeep(h, 'USA');
    const haw3 = command(h, 'USA', 'Hawaiian Air Force')[0];
    expect(techOf(h, 'USA')!.done).not.toContain('escorts');
    expect(airQuality(haw3, h)).toBe(0.9);
    techOf(h, 'USA')!.done.push('escorts');
    expect(airQuality(haw3, h)).toBe(1);
    // The volunteer group is a row, not a country: America's Pacific figure, and no rise of China's to end it.
    const c = pacificWar(1, 28);
    airUpkeep(c, 'CHN');
    const avg = command(c, 'CHN', 'American Volunteer Group')[0];
    expect(airQuality(avg, c)).toBe(0.9);
    expect(airQuality(command(c, 'CHN', 'Chinese Air Force')[0], c)).toBe(AIR.nations._default.quality);
    // The commands in Malaya and the Indies keep theirs: nothing rises for Britain.
    c.turn = 45;
    expect(AIR.rise!.UK).toBeUndefined();
  });

  it('3: from a field out here America\'s fighters reach by the job: 550 km for cover, 1,250 with the bombers on a raid from March 1944', () => {
    const g = pacificWar(1, 60);
    airUpkeep(g, 'USA');
    const f = command(g, 'USA', 'Seventh Air Force').find((x) => x.kind === 'ftr') ?? command(g, 'USA', 'Hawaiian Air Force').find((x) => x.kind === 'ftr')!;
    expect(beyondEurope(f.base)).toBe(true);
    // Ruling 227's dated row would give every job 1,200 km in June 1944.
    expect(airRange(f, g).ftr).toBe(550);
    for (const m of ['ground', 'superiority', 'harbour', 'strike', 'fleet', 'line', 'night']) expect(airRange(f, g, m).ftr, m).toBe(550);
    expect(airRange(f, g, 'bomb').ftr).toBe(1250);
    expect(airRange(f, g, 'town').ftr).toBe(1250);
    // Before March 1944 an escort reaches as cover does, and never past what the month's aircraft allow.
    g.turn = 54;
    expect(airRange(f, g, 'bomb').ftr).toBe(550);
    g.turn = 47;
    expect(airRange(f, g, 'bomb').ftr).toBe(375);
    expect(airRange(f, g).ftr).toBe(375);
    g.turn = 60;
    // The same group on a field of Europe's chart flies as Europe's rows do: nothing moves there.
    const was = f.base;
    f.base = g.byName['London'].id;
    expect(airRange(f, g).ftr).toBe(1200);
    expect(airRange(f, g, 'bomb').ftr).toBe(1200);
    f.base = was;
    // Its bombers, and everybody else's fighters, are untouched by the job.
    expect(airRange(f, g, 'bomb').bmr).toBe(1200);
    const jap = g.airFleets.find((x) => x.nat === 'JAP' && x.ship === undefined && x.kind === 'ftr')!;
    expect(airRange(jap, g).ftr).toBe(airRange(jap, g, 'bomb').ftr);
    // Saipan does not cover Iwo Jima's beach (1,112 km), and an escort from Iwo Jima reaches Tokyo (1,218 km).
    declareWar(g, 'USA', 'JAP');
    take(g, 'Saipan', 'USA'); take(g, 'Iwo Jima', 'USA');
    f.base = g.byName['Saipan'].id; f.mission = 'support'; f.flew = undefined;
    expect(whyNotMission(g, f, 'ground', g.byName['Iwo Jima'].id)).not.toBeNull();
    f.base = g.byName['Iwo Jima'].id;
    expect(whyNotMission(g, f, 'bomb', g.byName['Tokyo'].id)).toBeNull();
    expect(whyNotMission(g, f, 'superiority', g.byName['Tokyo'].id)).toBe('It is out of range.');
  });

  it('6: a withdrawal is the record\'s at the stops that keep the book; where none is kept the command stands at the most the record gave it', () => {
    const at = (stop: number): Game => {
      const g = pacificWar(stop, 28);
      for (const n of ['JAP', 'USA', 'CHN']) airUpkeep(g, n);
      return g;
    };
    const months = (g: Game, to: number, nats: string[]): void => { for (let t = g.turn + 1; t <= to; t++) { g.turn = t; for (const n of nats) airUpkeep(g, n); } };
    // By the book: the Far East Air Force is ten fighters in January 1942 and gone in May; the navy's Formosa air is gone in March.
    const book = at(1);
    months(book, 33, ['JAP', 'USA']);
    expect(command(book, 'USA', 'Far East Air Force').length).toBe(0);
    expect(command(book, 'JAP', '11th Air Fleet (21st and 23rd Air Flotillas)').length).toBe(0);
    // No book: both stand, kept at the most the record had given them, kind by kind (the bombers of November 1941, the fighters of December).
    const free = at(3);
    months(free, 33, ['JAP', 'USA']);
    const feaf = command(free, 'USA', 'Far East Air Force');
    expect(feaf.reduce((s, f) => s + f.est.ftr, 0)).toBe(100);
    expect(feaf.reduce((s, f) => s + f.est.bmr, 0)).toBe(25);
    expect(sum(command(free, 'JAP', '11th Air Fleet (21st and 23rd Air Flotillas)'))).toEqual([110, 120]);
    // Manchuria is not drained by the date either: that is the staff's to do.
    months(free, 53, ['JAP']);
    expect(command(free, 'JAP', 'Kwantung Army air (2nd Air Group)').reduce((s, f) => s + f.est.ftr + f.est.bmr, 0)).toBe(350);
    months(book, 53, ['JAP']);
    expect(command(book, 'JAP', 'Kwantung Army air (2nd Air Group)').reduce((s, f) => s + f.est.ftr + f.est.bmr, 0)).toBe(175);
    // The volunteer group's end is a change of service, outside the game: it ends in July 1942 at every stop.
    for (const g of [at(1), at(3)]) {
      expect(sum(command(g, 'CHN', 'American Volunteer Group'))).toEqual([60, 0]);
      months(g, 35, ['CHN']);
      expect(command(g, 'CHN', 'American Volunteer Group').length).toBe(0);
    }
    // A command that comes again where no book is kept is not stood down between (the 4th Air Army of 1942 to 1945).
    const row = ROWS.find((r) => r.name === '4th Air Army')!;
    expect(row.est).toEqual([[40, 120, 80], [56, 0, 0], [62, 250, 150], [65, 0, 0]]);
    expect(row.byBook).toBe(true);
  });

  it('7: Australia builds: its works keep its commands up, from the month each opens', () => {
    const W = ARSENAL.smallAir.works.AUS;
    expect([W.bmr, W.ftr]).toEqual([21, 35]);
    const g = pacificWar(1, 20);
    const pool = (): [number, number, number] => { const p = airPool(g, 'AUS'); return [p.ftr, p.bmr, p.pilots]; };
    smallAirMonth(g);
    expect(pool()).toEqual([0, 0, 0]);
    // May 1941: bombers at the earlier rate (0.3 of a wing a month), no fighters before July 1942.
    g.turn = 21;
    smallAirMonth(g);
    expect(pool()).toEqual([0, 11, 0]);
    g.turn = 35;
    g.airPool.AUS = { ftr: 0, bmr: 0, pilots: 0, lostFtr: 0, lostBmr: 0, lostPilots: 0 };
    smallAirMonth(g);
    expect(pool()).toEqual([12, 28, 12]);
    // Its commands are made up from that pool, as a great power's are: no aircraft arrive with the date.
    const h = pacificWar(1, 31);
    airUpkeep(h, 'AUS');
    const first = (): AirFleet[] => command(h, 'AUS', 'First Tactical Air Force');
    expect(sum(first())).toEqual([25, 10]);
    h.turn = 36;
    h.airPool.AUS = { ftr: 0, bmr: 0, pilots: 0, lostFtr: 0, lostBmr: 0, lostPilots: 0 };
    airUpkeep(h, 'AUS');
    expect(first().reduce((s, f) => s + f.est.ftr, 0)).toBe(40);
    expect(sum(first())).toEqual([25, 10]);
    smallAirMonth(h);
    airUpkeep(h, 'AUS');
    expect(sum(first())).toEqual([37, 30]);
    // No other smaller power's works feed a pool, on these rows or off them.
    for (const nat of ['NZL', 'NED', 'CHN', 'CAN']) { smallAirMonth(h); expect(h.airPool[nat]?.bmr ?? 0, nat).toBe(0); }
  });

  it('9: one count: where the record\'s own rows bring a country its aircraft, the rows from abroad step aside', () => {
    const G = ARSENAL.smallAir.gifts;
    const g = war(1, 24);
    const groups = (nat: string): number => g.airFleets.filter((f) => f.nat === nat).length;
    const nz = G.findIndex((r) => r[0] === 24 && r[1] === 'NZL');
    expect(nz).toBeGreaterThanOrEqual(0);
    const before = groups('NZL');
    smallAirMonth(g);
    expect(g.flags[`gift:${nz}`]).toBe(1);
    expect(groups('NZL')).toBe(before);
    // China's three and New Zealand's other three likewise; nobody else's row is touched.
    g.turn = 70;
    const china = groups('CHN');
    smallAirMonth(g);
    expect(groups('CHN')).toBe(china);
    expect(groups('NZL')).toBe(before);
    G.forEach((r, i) => { if (r[1] === 'NZL' || r[1] === 'CHN') expect(g.flags[`gift:${i}`], `${r[0]} ${r[1]}`).toBe(1); });
    // (Thailand's and Manchukuo's from Japan still come: they have no rows of the record's.)
    const th = G.findIndex((r) => r[1] === 'THA');
    expect(g.flags[`gift:${th}`]).toBe(1);
    expect(groups('THA')).toBeGreaterThan(0);
  });

  it('the Soviet Far East stands apart: what the record adds or takes comes and goes with the date, and at peace with Japan the staff leaves it there', () => {
    const NAMES = ROWS.filter((r) => r.nat === 'SOV').map((r) => r.name);
    const far = (g: Game): AirFleet[] => NAMES.flatMap((n) => command(g, 'SOV', n));
    const west = (g: Game): AirFleet[] => g.airFleets.filter((f) => f.nat === 'SOV' && f.ship === undefined && !NAMES.includes(f.parent ?? f.name));
    const total = (fs: AirFleet[]): number => fs.reduce((s, f) => s + f.ftr + f.bmr, 0);
    const g = war(1, 1);
    expect(total(far(g))).toBe(4040);
    // June 1941's rise (600 aircraft, to the army's 4,140 and the fleet's 500) arrives: the pool and the western districts are no poorer for it.
    g.turn = 21;
    airUpkeep(g, 'SOV');
    const pool = (): number => airPool(g, 'SOV').ftr + airPool(g, 'SOV').bmr;
    const had = total(west(g)) + pool();
    g.turn = 22;
    airUpkeep(g, 'SOV');
    expect(Math.abs(total(far(g)) - 4640)).toBeLessThanOrEqual(3);
    const built = AIR.production.SOV.filter((r) => r[0] <= 22).pop()!;
    expect(total(west(g)) + pool()).toBeGreaterThanOrEqual(Math.min(had + built[1] + built[2], had));
    // December 1941: 2,066 aircraft leave for the west, where Europe's rows have them already. None reaches the pool or a western command.
    g.turn = 27;
    airUpkeep(g, 'SOV');
    for (const f of west(g)) { f.ftr = 0; f.bmr = 0; }
    g.airPool.SOV = { ftr: 0, bmr: 0, pilots: 0, lostFtr: 0, lostBmr: 0, lostPilots: 0 };
    g.turn = 28;
    airUpkeep(g, 'SOV');
    expect(Math.abs(total(far(g)) - 2574)).toBeLessThanOrEqual(3);
    const month = AIR.production.SOV.filter((r) => r[0] <= 28).pop()!;
    expect(total(west(g)) + pool()).toBeLessThanOrEqual(month[1] + month[2]);
    // Its losses are made good from the pool, as any Soviet command's.
    const one = far(g).find((f) => f.kind === 'ftr')!;
    one.ftr -= 20;
    for (const f of west(g)) { f.ftr = f.est.ftr; f.bmr = f.est.bmr; }
    g.turn = 29;
    airUpkeep(g, 'SOV');
    expect(one.ftr).toBe(one.est.ftr);
    // At war in the west and at peace with Japan the staff leaves it on the Far East's fields, at a stop with no book too.
    for (const stop of [1, 3]) {
      const h = war(stop, 22);
      declareWar(h, 'GER', 'SOV');
      h.turn = 22;
      for (let i = 0; i < 4; i++) { airUpkeep(h, 'SOV'); airAI(h, 'SOV'); h.turn++; for (const f of h.airFleets) f.moved = 0; }
      expect(far(h).length).toBeGreaterThan(0);
      for (const f of far(h)) expect(h.regions[f.base].owner === 'SOV' && beyondEurope(f.base), `stop ${stop}: ${f.name} at ${h.regions[f.base].name}`).toBe(true);
    }
  });
});

describe('the air over a far sea (rulings 184 and 187)', () => {
  const S = () => PACIFIC!.skies!;

  it('the rule names the far side\'s seas, all but the western Indian Ocean; the old chart keeps the coasts', () => {
    const g = war(1, 28);
    expect(skiesOn(g)).toBe(true);
    expect(S().per).toBe(8);
    expect(S().cover).toEqual({ USA: 550 });
    expect(S().seas.length).toBe(70);
    for (const key of S().seas) { const z = zoneOf(g, key); expect(z, key).toBeGreaterThanOrEqual(0); expect(skiesOver(g, z), key).not.toBeNull(); }
    for (const key of ['channel', g.seas[0].key, g.seas[1].key, 'arabian', 'persian', 'redsea', 'somali', 'zanzibar', 'cindian', 'sindian', 'mascarene']) {
      const z = zoneOf(g, key);
      expect(z, key).toBeGreaterThanOrEqual(0);
      expect(skiesOver(g, z), key).toBeNull();
    }
    expect(g.seas.filter((z) => skiesOver(g, z.id)).length).toBe(70);
    // Outside the grand campaign the coasts decide everywhere.
    const plain = createGame(3, null);
    expect(skiesOn(plain)).toBe(false);
  });

  it('counts the groups in reach: fighters by their distance to the nearest shore, bombers in full inside their reach, eight aircraft to a point', () => {
    const g = pacificWar(1, 28);
    for (const n of ['JAP', 'USA', 'UK']) airUpkeep(g, n);
    const hand = (f: AirFleet, z: number): number => {
      if (f.ship !== undefined) return 0;
      const d = seaKm(g, f.base, z), R = airRange(f, g);
      return (f.ftr * cover(d, f.nat === 'USA' ? Math.min(R.ftr, 550) : R.ftr) + (d <= R.bmr ? f.bmr : 0)) * crowding(g, f);
    };
    const side = (z: number, nat: string, hostile: boolean): number =>
      (g.airFleets.reduce((s, f) => s + ((hostile ? atWar(g, f.nat, nat) : allied(g, f.nat, nat)) && g.nations[f.nat].atWar.length ? hand(f, z) : 0), 0) / 8) * g.seas[z].air;
    let some = 0;
    for (const key of S().seas) {
      const z = zoneOf(g, key);
      for (const nat of ['JAP', 'USA', 'UK']) for (const hostile of [false, true]) {
        const want = side(z, nat, hostile);
        expect(airOver(g, z, nat, hostile), `${key} ${nat} ${hostile}`).toBeCloseTo(want, 9);
        expect(groupsOver(g, z, nat, hostile, S())).toBe(airOver(g, z, nat, hostile));
        if (want > 0) some++;
      }
    }
    expect(some).toBeGreaterThan(40);
    // The Far East Air Force stands on the Luzon Sea's own shore: every aircraft it has counts there, with Del Monte's
    // bombers from Mindanao, and nobody else's does.
    const luzon = zoneOf(g, 'luzonsea'), feaf = command(g, 'USA', 'Far East Air Force'), delMonte = command(g, 'USA', '19th Bomb Group (Del Monte)');
    expect(g.seas[luzon].coast).toContain(g.byName['Lingayen'].id);
    expect(sum(delMonte)).toEqual([0, 16]);
    expect(airOver(g, luzon, 'JAP', true)).toBeCloseTo(((sum(feaf)[0] + sum(feaf)[1] + 16) / 8) * g.seas[luzon].air, 9);
    // Formosa's fighters reach it at part of their strength (the planner's half effect at the end of the Zero's reach), its bombers whole.
    const zero = command(g, 'JAP', '5th Air Group').find((f) => f.kind === 'ftr')!;
    const d = seaKm(g, zero.base, luzon);
    expect(d).toBeGreaterThan(0.6 * 850);
    expect(d).toBeLessThan(850);
    expect(groupOverSea(g, zero, luzon, S())).toBeCloseTo(zero.ftr * cover(d, 850), 9);
    expect(cover(d, 850)).toBeGreaterThan(0.5);
    expect(cover(d, 850)).toBeLessThan(1);
    // Who holds a coast no longer counts for anything there: Wake's twelve fighters are all America has over its waters.
    const wake = zoneOf(g, 'wake');
    expect(airOver(g, wake, 'JAP', true)).toBeCloseTo((12 / 8) * g.seas[wake].air, 9);
    // And the seas that were closed to Japan's troopships by the coasts alone are open to them.
    for (const key of ['luzonsea', 'leyte', 'celebes', 'sulu', 'natuna', 'malacca', 'java', 'wake']) expect(closedTo(g, zoneOf(g, key), 'JAP'), key).toBe(false);
  });

  it('what does not count: a resting group, an unfed field, a carrier wing afloat, a country at peace; a lost sky leaves three tenths', () => {
    const g = pacificWar(1, 28);
    airUpkeep(g, 'USA');
    const wake = zoneOf(g, 'wake');
    const f = command(g, 'USA', 'VMF-211 (Wake)')[0];
    expect(groupOverSea(g, f, wake, S())).toBe(12);
    f.mission = 'rest';
    expect(groupOverSea(g, f, wake, S())).toBe(0);
    f.mission = 'support';
    f.unfed = 1;
    expect(groupOverSea(g, f, wake, S())).toBe(0);
    delete f.unfed;
    g.sky[f.base] = { nat: 'JAP', turn: g.turn };
    expect(groupOverSea(g, f, wake, S())).toBeCloseTo(12 * AIR.sky.seaFoe, 9);
    delete g.sky[f.base];
    // A field that serves fewer aircraft than stand on it puts up only what it serves.
    f.ftr = 4 * AIRWAR.fields.sizes[AIRWAR.fields.seed.base];
    expect(groupOverSea(g, f, wake, S())).toBeCloseTo(AIRWAR.fields.sizes[AIRWAR.fields.seed.base], 6);
    f.ftr = 12;
    // Carrier wings afloat are the fleet's own strength, never the coast's.
    const wing = g.airFleets.find((x) => x.nat === 'JAP' && x.ship !== undefined)!;
    for (const z of g.seas) expect(groupOverSea(g, wing, z.id, S())).toBe(0);
    // A country at peace puts nothing over any sea.
    const peace = war(1, 28);
    for (const key of ['hawaii', 'luzonsea', 'malacca', 'java']) for (const nat of ['JAP', 'USA']) for (const hostile of [false, true]) expect(airOver(peace, zoneOf(peace, key), nat, hostile)).toBe(0);
  });

  it('the field is fed or not as its country\'s month in the air opens', () => {
    const g = pacificWar(1, 28);
    airUpkeep(g, 'USA');
    const f = command(g, 'USA', 'VMF-211 (Wake)')[0];
    expect(f.unfed).toBeUndefined();
    // Cut off: no supply reaches Wake as the picture stands.
    const was = g.supply.USA[f.base];
    g.supply.USA[f.base] = 0;
    g.turn = 29;
    airUpkeep(g, 'USA');
    expect(f.unfed).toBe(1);
    expect(groupOverSea(g, f, zoneOf(g, 'wake'), S())).toBe(0);
    g.turn = 30;
    airUpkeep(g, 'USA');
    expect(f.unfed).toBe(2);
    g.supply.USA[f.base] = was || 1;
    g.turn = 31;
    airUpkeep(g, 'USA');
    expect(f.unfed).toBeUndefined();
  });

  it('America\'s fighters cover a sea to 550 km and no farther, whatever they reach as escorts (ruling 187(a))', () => {
    const g = pacificWar(1, 60);
    airUpkeep(g, 'USA');
    const f = command(g, 'USA', 'Hawaiian Air Force').find((x) => x.kind === 'ftr')!;
    // (From a field out here its fighters reach by the job: 550 km for all but a raid's escorts.)
    expect(airRange(f, g).ftr).toBe(550);
    expect(airRange(f, g, 'bomb').ftr).toBe(1250);
    let far = 0, near = 0;
    // (Wherever the group stands: fields by hand, on islands and coasts of the south-west Pacific.)
    for (const name of ['Honolulu', 'Guadalcanal', 'Port Moresby', 'Darwin', 'Lingayen']) {
      f.base = g.byName[name].id;
      for (const z of g.seas) {
        if (!skiesOver(g, z.id)) continue;
        const d = seaKm(g, f.base, z.id);
        if (d > 550 && d <= 1200) { far++; expect(groupOverSea(g, f, z.id, S()), z.key).toBe(0); }
        if (d <= 0.6 * 550) { near++; expect(groupOverSea(g, f, z.id, S()), z.key).toBeCloseTo(f.ftr * crowding(g, f), 9); }
      }
    }
    expect(near).toBeGreaterThan(0);
    expect(far).toBeGreaterThan(0);
  });
});

/** A small steady stream of numbers, so the stirring is the same every run. */
const stream = (seed: number) => () => ((seed = (Math.imul(seed, 1103515245) + 12345) >>> 0) / 2 ** 32);

function stirred(seed: number): Game {
  const g = pacificWar(1, 30, null, seed);
  const rnd = stream(seed);
  const nats = Object.keys(g.nations).filter((n) => g.nations[n].alive);
  for (const n of ['JAP', 'USA', 'UK', 'AUS', 'CHN']) airUpkeep(g, n);
  for (let k = 0; k < 25; k++) {
    const a = nats[Math.floor(rnd() * nats.length)], b = nats[Math.floor(rnd() * nats.length)];
    if (a !== b) setWar(g, a, b);
  }
  for (const f of g.fleets) {
    f.zone = Math.floor(rnd() * g.seas.length);
    f.stance = rnd() < 0.8 ? 'patrol' : 'port';
    if (rnd() < 0.2) f.ships = { ...f.ships, bb: f.ships.bb + 30 };
  }
  // Air groups of every size on fields all over the far side, some resting, some unfed, some under a lost sky.
  const fields = [...new Set(g.seas.filter((z) => skiesOver(g, z.id)).flatMap((z) => z.coast))];
  for (const f of g.airFleets) {
    if (f.ship !== undefined) continue;
    if (rnd() < 0.7) f.base = fields[Math.floor(rnd() * fields.length)];
    f.ftr = f.kind === 'bmr' ? 0 : Math.floor(rnd() * 900);
    f.bmr = f.kind === 'ftr' ? 0 : Math.floor(rnd() * 900);
    if (rnd() < 0.1) f.mission = 'rest';
    if (rnd() < 0.1) f.unfed = 1;
    if (rnd() < 0.15) g.sky[f.base] = { nat: nats[Math.floor(rnd() * nats.length)], turn: g.turn - Math.floor(rnd() * 3) };
  }
  for (const n of nats) g.nations[n].air = Math.floor(rnd() * 400);
  return g;
}

describe('the picture of the seas answers as closedTo does where the groups are counted', () => {
  for (const seed of [3, 11]) {
    it(`every sea, every nation, seed ${seed}`, () => {
      const g = stirred(seed);
      const pic = new SeaPicture(g);
      let closedFar = 0, closedOld = 0;
      for (const nat of Object.keys(g.nations)) {
        if (!g.nations[nat].alive) continue;
        for (const z of g.seas) {
          const want = closedTo(g, z.id, nat);
          expect(pic.closed(z.id, nat), `${z.key} ${nat}`).toBe(want);
          if (want) { if (skiesOver(g, z.id)) closedFar++; else closedOld++; }
        }
        expect([...openSeas(g, nat, pic)]).toEqual([...openSeas(g, nat)]);
      }
      // The stirring closes seas of both kinds to somebody, and some far seas by aircraft alone.
      expect(closedFar).toBeGreaterThan(0);
      expect(closedOld).toBeGreaterThan(0);
      const byAir = g.seas.filter((z) => skiesOver(g, z.id) && !g.fleets.some((f) => f.zone === z.id && f.stance !== 'port') && Object.keys(g.nations).some((n) => g.nations[n].alive && closedTo(g, z.id, n)));
      expect(byAir.length).toBeGreaterThan(0);
      expect(NAVY.closed.min).toBeGreaterThan(0);
    });
  }
});
