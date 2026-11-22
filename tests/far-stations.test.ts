// The book's stations (ruling 234, rider b568; src/campaign/stations.ts).
//
// By the book the defenders play the record too. At a stop that keeps the book, while the Pacific's book is open, the
// computer's corps the historian's table names stand where the record had them month by month, with the record's
// works and no more: thin along Luzon's beaches and then on Bataan, forward and half-dug in Malaya and then on the
// island, back up the Irrawaddy in Burma. Never the player's corps, and nobody's in a war where he is the invader.
// With the far side's switch off none of it exists (tests/pacific-war-off.test.ts).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import farWorld from '../data/campaign/far-world.json';
import farShores from '../data/campaign/far-shores.json';
import chinaSetup from '../data/campaign/china-setup.json';
import { DEFENCE, addCorps, allied, createGame, declareWar, endPhase, relocate } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import { bookStations, stations, stationsBind } from '../src/campaign/stations';
import { STACK, type Corps, type Game } from '../src/campaign/types';

const war = (stop: number, human: string | null = null): Game => {
  const g = startOpen(createGame(3, human));
  if (stop !== 3) setStop(g, stop, true);
  return g;
};
const corps = (g: Game, nat: string, name: string): Corps => g.corps.find((c) => c.nat === nat && c.name === name && !c.dead)!;
const at = (g: Game, c: Corps): string => g.regions[c.reg].name;
/** A corps of the table that the war's own rows bring later, put down by hand where its row has it. */
const row = (g: Game, nat: string, name: string, place: string, divs = 1, o: object = {}): Corps => addCorps(g, nat, name, 'inf', divs, place, o);
/** Japan's blow has fallen in its month. */
const strike = (g: Game, month = 28): void => { g.flags['st:done:JAP:south'] = month; for (const e of ['USA', 'UK']) declareWar(g, 'JAP', e); };
const FULL = DEFENCE.entrench.months;
const NLF = 'North Luzon Force', D41 = '41st Division, Philippine Army', IND11 = '11th Indian Division (6th and 15th Brigades)', BDE12 = '12th Indian Infantry Brigade (from India, August 1939)';
const MALAYA = 'Malaya Command (Singapore garrison, the Malay Regiment and the Volunteers)', AUS8 = '8th Australian Division (22nd Brigade)';

describe("the book's stations (ruling 234, rider b568)", () => {
  it('names only rows the far side has and places on its chart, in the order of the months', () => {
    const B = stations()!, g = war(1);
    // (The far side's rows: the tool-written world, and the hand-kept shores file beside it; and China's hand-kept
    // setup, whose 51st Division the book stands at Rabaul from November 1942: tests/far-canton-rows.test.ts.)
    const rows = JSON.stringify(farWorld) + JSON.stringify(farShores) + JSON.stringify(chinaSetup);
    expect(B.rows.length).toBeGreaterThan(15);
    for (const r of B.rows) {
      expect(rows.includes(JSON.stringify(r.corps)), r.corps).toBe(true);
      expect(r.steps.map((s) => s[0])).toEqual([...r.steps.map((s) => s[0])].sort((a, b) => a - b));
      for (const s of r.steps) {
        for (const place of Array.isArray(s[1]) ? s[1] : [s[1]]) expect(g.byName[place], place).toBeDefined();
        expect([0, 1, FULL]).toContain(s[2] ?? 0);
        if (s[3]?.joins) expect(B.rows.some((x) => x.nat === r.nat && x.corps === s[3]!.joins), s[3].joins).toBe(true);
      }
    }
  });

  it('gives the eve its posture by the calendar: thin works on the beaches, none in the open, and the Burma division in the Shan States', () => {
    const g = war(1);
    const nlf = row(g, 'USA', NLF, 'Lingayen', 4), ind = row(g, 'UK', IND11, 'Penang');
    for (const c of [nlf, ind]) { c.dug = true; c.dugFor = FULL; }
    const div = corps(g, 'USA', 'Philippine Division'), bde = corps(g, 'UK', BDE12), burma = corps(g, 'UK', 'Burma Army (the Burma Rifles; the 1st Burma Division from July 1941)');
    div.dug = true; div.dugFor = FULL;
    // A month before the blow nothing is the book's.
    g.turn = 27;
    for (const nat of ['USA', 'UK']) bookStations(g, nat);
    expect([nlf.bookWorks, nlf.dugFor, nlf.hold, at(g, burma)]).toEqual([undefined, FULL, false, 'Rangoon']);
    // In its month America and Britain move before Japan: the posture is the eve's, in peace.
    g.turn = 28;
    for (const nat of ['USA', 'UK']) bookStations(g, nat);
    expect([at(g, nlf), nlf.dug, nlf.dugFor, nlf.hold, nlf.bookWorks]).toEqual(['Lingayen', true, 1, true, 1]);
    // (Rider b581: Jitra's works count none, half-dug and waterlogged.)
    expect([at(g, ind), !!ind.dug, ind.dugFor, ind.hold, ind.bookWorks]).toEqual(['Penang', false, undefined, true, 0]);
    expect([at(g, div), !!div.dug, div.hold, div.bookWorks]).toEqual(['Manila', false, true, 0]);
    expect([at(g, bde), !!bde.dug, bde.hold, bde.bookWorks]).toEqual(['Kuala Lumpur', false, true, 0]);
    expect([at(g, burma), !!burma.dug, burma.bookWorks]).toEqual(['Taunggyi', false, 0]);
  });

  it('keeps the record\'s works and no more at the month\'s end, where a corps beside it digs as it likes', () => {
    const g = war(1);
    const nlf = row(g, 'USA', NLF, 'Lingayen', 4), other = row(g, 'USA', 'A corps the table does not name', 'Manila');
    const div = corps(g, 'USA', 'Philippine Division');
    g.turn = 28;
    bookStations(g, 'USA');
    for (let k = 0; k < FULL + 1; k++) endPhase(g, 'USA');
    expect([nlf.dug, nlf.dugFor]).toEqual([true, 1]);
    expect([div.dug, div.dugFor, div.digging]).toEqual([undefined, undefined, undefined]);
    expect([other.dug, other.dugFor! >= FULL, other.bookWorks]).toEqual([true, true, undefined]);
  });

  it('falls back on its month once the war is on, into prepared works, and brigades that fell back into one command are one corps', () => {
    const g = war(1);
    const nlf = row(g, 'USA', NLF, 'Lingayen', 4, { est: 0.4 }), d91 = row(g, 'USA', '91st Division, Philippine Army', 'Lingayen', 1, { est: 0.4 });
    const d41 = row(g, 'USA', D41, 'Manila', 1, { est: 0.4 });
    for (const c of [nlf, d91, d41]) c.str = 0.4;
    const div = corps(g, 'USA', 'Philippine Division'), army = corps(g, 'USA', 'Philippine Army');
    const men = d41.str * d41.divs + army.str * army.divs;
    g.turn = 28;
    bookStations(g, 'USA');
    // January without a war: nobody goes.
    g.turn = 29;
    bookStations(g, 'USA');
    expect([at(g, nlf), at(g, div)]).toEqual(['Lingayen', 'Manila']);
    strike(g);
    bookStations(g, 'USA');
    expect([at(g, nlf), nlf.dug, nlf.dugFor, nlf.bookWorks, nlf.hold]).toEqual(['Bataan', true, FULL, FULL, true]);
    expect([at(g, div), div.dugFor, at(g, d41), d41.dugFor]).toEqual(['Bataan', FULL, 'Bataan', FULL]);
    // The 91st is the North Luzon Force's fifth division; the Philippine Army's row is in the 41st's corps, man for man.
    expect([d91.dead, nlf.divs, nlf.full]).toEqual([true, 5, 0.4]);
    expect([army.dead, d41.divs]).toEqual([true, 2]);
    expect(d41.str * d41.divs).toBeCloseTo(men, 6);
    expect(g.corps.includes(d91) || g.corps.includes(army)).toBe(false);
    // A retreat's room, and no more: the peninsula holds its fort, its marines and the corps that fell back.
    expect(g.byName.Bataan.units.filter((u) => u.nat === 'USA').length).toBeLessThanOrEqual(STACK + 2);
    // The step is taken once: a second reading of the month moves nobody.
    const where = g.corps.filter((c) => c.nat === 'USA').map((c) => c.reg).join();
    bookStations(g, 'USA');
    expect(g.corps.filter((c) => c.nat === 'USA').map((c) => c.reg).join()).toBe(where);
  });

  it('falls back to the next place on its road where its place is lost, and a corps driven off its station stands in the open', () => {
    const g = war(1);
    const ind = row(g, 'UK', IND11, 'Penang'), aus = row(g, 'UK', AUS8, 'Johore');
    g.turn = 28;
    bookStations(g, 'UK');
    expect([at(g, aus), aus.dugFor, aus.bookWorks]).toEqual(['Johore', 1, 1]);
    strike(g);
    g.byName['Kuala Lumpur'].ctrl = 'JAP';
    relocate(g, aus, g.byName.Kuantan);
    aus.dug = true; aus.dugFor = 1;
    g.turn = 29;
    bookStations(g, 'UK');
    // The Slim River is gone: the 11th Indian is on the island a month early, where nothing is dug.
    expect([at(g, ind), !!ind.dug, ind.bookWorks, ind.hold]).toEqual(['Singapore', false, 0, true]);
    // The Australians were driven out of Johore and have no step this month.
    expect([at(g, aus), !!aus.dug, aus.bookWorks]).toEqual(['Kuantan', false, 0]);
    g.turn = 30;
    bookStations(g, 'UK');
    expect([at(g, aus), !!aus.dug, aus.bookWorks]).toEqual(['Singapore', false, 0]);
  });

  it('puts a free row down in its place and leaves it to the staff: the Guards in northern Malaya in January', () => {
    const g = war(1);
    const guards = row(g, 'JAP', '25th Army (Imperial Guards Division)', 'Phnom Penh');
    g.turn = 29;
    // The blow has not fallen and Siam's ground is nobody's road: the Guards wait where they are.
    bookStations(g, 'JAP');
    expect(at(g, guards)).toBe('Phnom Penh');
    // It falls, and Siam stands with Japan (far-pacific-war.json, 'siam-beside-japan'): the invader's own rows go by the blow alone.
    strike(g);
    expect(allied(g, 'JAP', g.byName.Singora.ctrl)).toBe(true);
    bookStations(g, 'JAP');
    expect([at(g, guards), guards.hold, guards.bookWorks, !!guards.stay]).toEqual(['Singora', false, undefined, false]);
    // The 38th and the 23rd Army's guns stand on Hong Kong's frontier on the eve, by the calendar alone (rider b572).
    const eve = war(1);
    const d38 = row(eve, 'JAP', '38th Division (Canton)', 'Canton'), guns = addCorps(eve, 'JAP', '23rd Army artillery (1st Artillery Group)', 'art', 1, 'Canton', { hold: 1 });
    eve.turn = 27;
    bookStations(eve, 'JAP');
    expect([at(eve, d38), at(eve, guns)]).toEqual(['Canton', 'Canton']);
    eve.turn = 28;
    bookStations(eve, 'JAP');
    expect([at(eve, d38), at(eve, guns), d38.bookWorks]).toEqual(['Tungkun', 'Tungkun', undefined]);
    expect(eve.byName['Hong Kong'].nb).toContain(eve.byName.Tungkun.id);
  });

  it('binds computer against computer at the stops that keep the book, and lets every corps go when the book does', () => {
    // Never the player's own corps; nobody's where he is the invader; nobody's where history is loose.
    const mine = war(1, 'UK');
    mine.turn = 28;
    bookStations(mine, 'UK');
    expect([stationsBind(mine, 'UK'), stationsBind(mine, 'USA'), corps(mine, 'UK', BDE12).bookWorks]).toEqual([false, true, undefined]);
    const invader = war(1, 'JAP');
    invader.turn = 28;
    expect([stationsBind(invader, 'UK'), stationsBind(invader, 'USA')]).toEqual([false, false]);
    const loose = war(3);
    loose.turn = 28;
    bookStations(loose, 'UK');
    expect([stationsBind(loose, 'UK'), corps(loose, 'UK', BDE12).bookWorks]).toEqual([false, undefined]);
    expect(stationsBind(war(2), 'UK')).toBe(true);
    // The book closes in the Pacific: the brigade is the staff's again, and a garrison row keeps the hold its own row gave it.
    const g = war(1);
    g.turn = 28;
    bookStations(g, 'UK');
    const bde = corps(g, 'UK', BDE12), garrison = corps(g, 'UK', MALAYA);
    expect([bde.hold, bde.stay, bde.bookWorks, garrison.hold, garrison.bookWorks]).toEqual([true, true, 0, true, 0]);
    g.flags['hist:off:pacific'] = 30;
    bookStations(g, 'UK');
    expect([bde.hold, bde.stay, bde.bookWorks, garrison.hold, garrison.bookWorks]).toEqual([false, undefined, undefined, true, undefined]);
    // And after the table's last month.
    const late = war(1);
    late.turn = 28;
    bookStations(late, 'UK');
    late.turn = stations()!.until + 1;
    expect(stationsBind(late, 'UK')).toBe(false);
    bookStations(late, 'UK');
    expect(corps(late, 'UK', BDE12).bookWorks).toBeUndefined();
  });
});
