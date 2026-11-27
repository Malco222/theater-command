// The far side with its switch on.
//
// The switch (data/campaign/stage.json, 'far') is off in the game as it ships, and the seats' tests run with it off:
// they hold the rows as they wait. This file turns the switch on for itself and runs the same files again, so that
// their second halves run too: each seat opens on the whole world's chart, and the six tests of a client's tie hold
// (far.ts). It then holds what is nobody's seat: Japan, the old powers' garrisons, the fleets, and the rows by date.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import './far-seats-pacific.test';
import './far-seats-globe.test';

// ---- What is nobody's seat: Japan, the garrisons, the fleets, the rows by date, and the riders to the tie ------------

import { describe, expect, it } from 'vitest';
import farWorld from '../data/campaign/far-world.json';
import grandData from '../data/campaign/grand.json';
import leagueData from '../data/campaign/league.json';
import nationsData from '../data/campaign/nations.json';
import setupData from '../data/campaign/setup-1939.json';
import worldData from '../data/campaign/world.json';
import { ARSENAL, offers, yardLevel } from '../src/campaign/arsenal';
import { BUDGET, PRODUCTION, aiProduction, canRaise, colonyPurse, farColony, farGuard, farGuardRow, farShare, landFactor, landShare, lineFactories, provinceYield } from '../src/campaign/economy';
import { monthlyDiplomacy, signPeace } from '../src/campaign/diplomacy';
import { tieBars, tieWar, tiedPair } from '../src/campaign/far';
import { FAR_LAID, FAR_PATHS, FAR_SETUP } from '../src/campaign/far-rows';
import { allied, armySize, atWar, bloc, capitulate, capture, createGame, declareWar, industryHeld, rowOffers, sameSide, setWar, startPhase, takeOffer } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { small, threatened } from '../src/campaign/league';
import { MANPOWER, classNow, homeShare } from '../src/campaign/manpower';
import { NAVY, navalUpkeep } from '../src/campaign/navy';
import { RES, fieldOverseas, fieldYield, stores } from '../src/campaign/resources';
import { blocked } from '../src/campaign/rivers';
import { loadGame, saveGame } from '../src/campaign/save';
import { SIEGE, guardOf } from '../src/campaign/siege';
import { joinsOn, stalemates, startOpen } from '../src/campaign/staff';
import { prizeOf, townAt, townName, townSize, worthOf } from '../src/campaign/towns';
import { TRAITS, traitMoney } from '../src/campaign/traits';
import { runTurnAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import { FLAGS } from '../src/campaign/ui/flags';
import { FAR_FIRST, WORLD, WORLD_FIRST, WORLD_MAP, WORLD_REGIONS, enemyAtTheDoor, farOn, overseasOf, shoreOf } from '../src/campaign/world';

const FW = farWorld as any;
const WD = worldData as any;
const grand = (human: string | null = null, seed = 3): Game => startOpen(createGame(seed, human));
const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const at = (g: Game, f: Fleet): string => g.regions[f.base].name;
const SEATS = ['SAF', 'IRN', 'IRQ', 'SAU', 'AFG', 'YEM', 'NEP', 'LBR', 'CHN', 'AUS', 'NZL', 'THA', 'MAN', 'MGL'];

describe("the far side with its switch on: the rows that are nobody's seat", () => {
  it('lays the fourteen files over the data, in order, and the game reads them as its own', () => {
    expect(FAR_LAID).toEqual(['far-seats-globe', 'far-seats-pacific', 'far-world', 'far-pacific-war', 'china-line', 'china-setup', 'china-stop3', 'far-south-line', 'far-air', 'far-guards', 'far-africa', 'far-books', 'far-japan-seat', 'far-shores']);
    expect(WD.nations.JAP.name).toBe('Japan');
    expect(WD.great).toEqual(['USA', 'JAP']);
    expect(Object.keys(WD.clients)).toEqual(['MAN', 'MGL']);
    // A great power founds no League; the clients keep a small country's card, and are barred by their tie (league.ts).
    expect((leagueData as any).never).toContain('JAP');
    for (const id of ['MAN', 'MGL']) { expect((leagueData as any).never, id).not.toContain(id); expect(small(id), id).toBe(true); }
    expect(small('JAP')).toBe(false);
    // (The far world's fleet bases by date, and the Pacific war file's own: the carrier striking force's two and the
    // Asiatic Fleet's four, the design director's rulings of 03:55Z and 04:25Z, 22 November 2026.)
    expect(NAVY.moves!.length).toBe(FW.navy.moves.length + 6);
    expect(NAVY.moves!.filter((m) => m.fleet === 'Asiatic Fleet' && m.nat === 'USA').map((m) => m.base)).toEqual(['Panay', 'Soerabaja', 'Semarang', 'Perth']);
    expect(NAVY.moves!.filter((m) => m.fleet === 'Mobile Fleet' && m.nat === 'JAP').map((m) => m.base).slice(-2)).toEqual(['Etorofu', 'Hiroshima']);
  });

  it('puts Japan on the door as the seventh great power, and the fourteen seats on its second row', () => {
    const door = (grandData as any).nations as { id: string; small?: boolean; years: Record<string, string> }[];
    expect(door.filter((n) => !n.small).map((n) => n.id)).toEqual(['GER', 'UK', 'FRA', 'SOV', 'ITA', 'USA', 'POL', 'JAP']);
    const jap = door.find((n) => n.id === 'JAP')!;
    expect(Object.keys(jap.years)).toEqual(['1939']);
    expect(jap.years['1939'].split(/\s+/).length).toBeLessThanOrEqual(45);
    for (const id of SEATS) expect(door.find((n) => n.id === id)?.small, id).toBe(true);
    expect(FLAGS.JAP).toBe(FLAGS.JPN);
    // A seat that is bound says so on the door; Thailand is free and carries the door's own words.
    expect(Object.keys(FAR_PATHS).sort()).toEqual(SEATS.filter((id) => id !== 'THA').sort());
  });

  it('opens the whole world with a row of its own for every country on the chart', () => {
    const g = grand();
    expect(farOn(g)).toBe(true);
    expect(g.regions.length).toBe(WORLD_FIRST + WORLD_REGIONS);
    // (Tibet is the one bare country, by ruling: rider b632 to ruling 234 gives its five provinces the game's form
    // for a neutral's ground, an owner's code with no row. far-tibet.test.ts holds it. No other code may be bare.)
    for (const id of new Set(g.regions.flatMap((r) => [r.owner, r.ctrl]))) {
      expect(g.nations[id], id).toBeDefined();
      if (id === 'TIB') continue;
      expect((nationsData as any)[id] ?? (nationsData as any).nations?.[id] ?? WD.nations[id], `${id} has no row: it would be a bare stand-in`).toBeDefined();
    }
    const J = g.nations.JAP;
    expect(g.regions[J.capital].name).toBe('Tokyo');
    expect([J.align, bloc(J), J.quality]).toEqual(['leaning_axis', null, 1]);
    expect(J.atWar).toEqual(['CHN']);
    expect(g.order).toContain('JAP');
  });

  it('holds the ground as it stood on 1 September 1939, and stands every first-day row on it', () => {
    const g = grand();
    expect(Object.keys(FAR_SETUP.held).length).toBeGreaterThan(30);
    for (const [name, by] of Object.entries(FAR_SETUP.held)) expect(g.byName[name].ctrl, name).toBe(by);
    for (const [nat, name, , divs, place, o] of WD.corps as [string, string, string, number, string, { str?: number }][]) {
      if (!g.byName[place]) continue;
      const c = g.corps.find((x) => x.nat === nat && x.name === name);
      expect(c, `${nat} ${name}`).toBeDefined();
      expect(g.regions[c!.reg].name, name).toBe(place);
      expect(allied(g, nat, g.regions[c!.reg].ctrl), `${name} at ${place}`).toBe(true);
      expect(c!.divs, name).toBe(divs);
      if (o.str !== undefined) expect(c!.str, name).toBe(o.str);
    }
    for (const row of WD.fleets) expect(fleet(g, row.nat, row.name), `${row.nat} ${row.name}`).toBeDefined();
    // Japan's army in China stands on the ground Japan holds; nothing of anybody's stands on ground at war with it.
    for (const c of g.corps) expect(atWar(g, c.nat, g.regions[c.reg].ctrl), c.name).toBe(false);
    expect(g.fleets.filter((f) => f.nat === 'JAP').length).toBe(9);
    expect(g.fleets.filter((f) => f.nat === 'USA').map((f) => f.name).sort()).toEqual(['Asiatic Fleet', 'Atlantic Fleet', 'Pacific Carrier Fleet', 'Pacific Fleet', 'Pacific Submarine Fleet']);
  });

  it('lands the rows by date as thin as their rows say, and never fills one past its fullest', () => {
    const rows = [...FW['setup-1939'].formations].sort((a: any, b: any) => a[0] - b[0]) as [number, string, string, string, number, string, { str?: number; est?: number }][];
    expect(rows.length).toBeGreaterThan(10);
    const g = grand();
    for (const [turn, nat, name, , , place, o] of rows) {
      g.turn = turn;
      startPhase(g, nat);
      const c = g.corps.find((x) => x.nat === nat && x.name === name);
      expect(c, name).toBeDefined();
      expect(g.regions[c!.reg].name, name).toBe(place);
      // The month's own drafts may have reached it already, but never past the fullest the row gets.
      if (o.str !== undefined) expect(c!.str, name).toBeLessThanOrEqual(Math.max(o.str, o.est ?? 1) + 1e-9);
      if (o.est !== undefined && o.est < 1) expect(c!.full, name).toBe(o.est);
    }
  });

  it("holds and stays the rows of the book's own operations where the war does not follow the book (ruling b423)", () => {
    const list = (setupData as any).bookOnly as { stops: number[]; rows: [string, string][] };
    expect(list.stops).toEqual([3, 4, 5]);
    // (Seven of ruling b423's, and the 38th Division's three regimental rows, which stand in its place at these stops: far-shores.json.)
    expect(list.rows.length).toBe(10);
    // Every row named is a dated row of Japan's that the far side lays, once.
    const dated = (setupData as any).expeditions as [number, string, string, string, number, string, any][];
    for (const [nat, name] of list.rows) expect(dated.filter((r) => r[1] === nat && r[2] === name).length, name).toBe(1);
    // The 5th Division is brought to Formosa in month 2 for the landing at Pakhoi, an operation of the book alone.
    const raise = (stop: number, human: string | null) => {
      const g = grand(human);
      // (His rows are offers only where the switch 'offers' is on: stage.json, off until its screen passes, the design
      // director's 13:39Z of 25 November 2026. A campaign opened with it on carries the flag; the test sets it by hand.)
      if (human) g.flags.offers = 1;
      setStop(g, stop, true);
      g.turn = 2;
      startPhase(g, 'JAP');
      // (Ruling B8 of 25 November 2026: the row takes a corps off the board, so a player's Japan is offered it, and
      // nothing of his is taken until he says yes. Taken, it forms as the rule always formed it. This test pinned
      // the row forming of itself for the player as for the computer.)
      if (human === 'JAP') {
        expect(g.corps.some((x) => x.nat === 'JAP' && x.name === '21st Army (5th Division)')).toBe(false);
        expect(g.corps.some((x) => x.nat === 'JAP' && x.name === '12th Army (5th Division)')).toBe(true);
        const offered = rowOffers(g).map((o) => [o.id, o.name]);
        expect(offered).toEqual([[dated.findIndex((r) => r[1] === 'JAP' && r[2] === '21st Army (5th Division)'), '21st Army (5th Division)']]);
        expect(takeOffer(g, offered[0][0] as number)).toBeNull();
      }
      const c = g.corps.find((x) => x.nat === 'JAP' && x.name === '21st Army (5th Division)');
      expect(c, `stop ${stop}`).toBeDefined();
      expect(g.regions[c!.reg].name).toBe('Taihoku');
      // The row it is made from is struck off either way.
      expect(g.corps.some((x) => x.nat === 'JAP' && x.name === '12th Army (5th Division)')).toBe(false);
      return c!;
    };
    for (const stop of [3, 4, 5]) { const c = raise(stop, null); expect([c.hold, !!c.stay], `stop ${stop}`).toEqual([true, true]); }
    // By the book the landing takes it: it comes as the row is written.
    for (const stop of [1, 2]) { const c = raise(stop, null); expect([c.hold, !!c.stay], `stop ${stop}`).toEqual([false, false]); }
    // A hold binds Japan's staff, never a player's own corps.
    const own = raise(3, 'JAP');
    expect([own.hold, !!own.stay]).toEqual([false, false]);
    // A row that carried a hold already keeps it at every stop, and is stayed only where the book is not followed.
    const south = (stop: number) => {
      const g = grand();
      setStop(g, stop, true);
      g.turn = 27;
      startPhase(g, 'JAP');
      return g.corps.find((x) => x.nat === 'JAP' && x.name === '25th Army (18th Division)')!;
    };
    expect([south(3).hold, !!south(3).stay]).toEqual([true, true]);
    expect([south(1).hold, !!south(1).stay]).toEqual([true, false]);
  });

  it("gives the far side's navies their yards, so that Japan can lay a keel (ruling b445)", () => {
    // The director's ruling of 18 November 2026: "a Japan that can lay no keel is a fault, not a balance choice". The
    // arsenal's 47 yards ended at the Americas; the twelve of the planner's far-yards.md are laid with them. The
    // historian's marks (the largest ship each yard built in the war years): Vladivostok at 2 for the two cruisers
    // Dalzavod completed, Hong Kong's merchant yards at 1 for Britain, and none at Singapore, a repair base.
    const yards = FW.arsenal.yards as Record<string, number>;
    expect(yards).toEqual({ Hiroshima: 3, Yokohama: 3, Nagasaki: 3, Osaka: 3, Kyoto: 1, Sydney: 1, Melbourne: 1, Auckland: 1, Bangkok: 1, Abadan: 1, Vladivostok: 2, 'Hong Kong': 1 });
    const g = grand();
    const HOME: Record<string, string> = { Hiroshima: 'JAP', Yokohama: 'JAP', Nagasaki: 'JAP', Osaka: 'JAP', Kyoto: 'JAP', Sydney: 'AUS', Melbourne: 'AUS', Auckland: 'NZL', Bangkok: 'THA', Abadan: 'IRN', Vladivostok: 'SOV', 'Hong Kong': 'UK' };
    for (const [name, level] of Object.entries(yards)) {
      const r = g.byName[name];
      expect(r, name).toBeDefined();
      expect(r.id, name).toBeGreaterThanOrEqual(FAR_FIRST);
      expect([r.port, r.owner, r.ctrl, yardLevel(g, r)], name).toEqual([true, HOME[name], HOME[name], level]);
    }
    // The older chart's yards stand as they were.
    expect([ARSENAL.yards.Hamburg, ARSENAL.yards.Norfolk, ARSENAL.yards.Lima]).toEqual([3, 3, 1]);
    expect(Object.keys(ARSENAL.yards).length).toBe(48 + 12);
    expect(ARSENAL.yards.Singapore).toBeUndefined();
    // Main's own rule (arsenal.test.ts), on the whole world: every nation with a navy on the first day has somewhere to
    // build a destroyer. It failed on Japan, Australia, New Zealand, Thailand and Iran.
    const offer = (nat: string, key: string) => offers(g, nat).find((o) => o.key === key)!;
    const navies = [...new Set(g.fleets.filter((f) => f.base >= 0).map((f) => f.nat))];
    for (const nat of ['JAP', 'AUS', 'NZL', 'THA', 'IRN']) expect(navies, nat).toContain(nat);
    for (const nat of navies) expect(offer(nat, 'ship:dd').at, nat).toBeGreaterThanOrEqual(0);
    // Japan can lay every hull down, a carrier among them, at one of its four great yards; Australia a destroyer and
    // troopships, and no cruiser until Sydney's yard is enlarged.
    for (const hull of ['dd', 'ss', 'tp', 'ca', 'cv', 'bb']) expect(['Hiroshima', 'Yokohama', 'Nagasaki', 'Osaka'], hull).toContain(g.regions[offer('JAP', `ship:${hull}`).at]?.name);
    expect(['Sydney', 'Melbourne']).toContain(g.regions[offer('AUS', 'ship:tp').at]?.name);
    expect(offer('AUS', 'ship:ca').at).toBe(-1);
  });

  it("moves a computer navy's base in its month, where it holds the place, and leaves a player's fleet to him", () => {
    // (The book's dated base moves run at stops 1 and 2 only: Units decide audit item 42, 16:19Z of 25 November 2026.)
    const s3 = grand();
    s3.turn = 9;
    startPhase(s3, 'USA');
    expect(at(s3, fleet(s3, 'USA', 'Pacific Fleet'))).toBe('San Diego');
    const g = grand();
    setStop(g, 2, true);
    const pf = fleet(g, 'USA', 'Pacific Fleet');
    expect(at(g, pf)).toBe('San Diego');
    g.turn = 9;
    startPhase(g, 'USA');
    expect([at(g, pf), pf.port, g.seas[pf.home].key, pf.stance]).toEqual(['Pearl Harbor', 'Pearl Harbor', 'hawaii', 'port']);
    expect(g.headlines.some((h) => h.text === 'The Pacific Fleet is ordered to stay at Pearl Harbor.')).toBe(true);
    // The forward bases are the book's (stops 1 and 2), and only on ground America has taken.
    g.turn = 54;
    startPhase(g, 'USA');
    expect(at(g, pf)).toBe('Pearl Harbor');
    const b = grand();
    setStop(b, 1, true);
    b.byName['Eastern Marshalls'].ctrl = 'USA';
    b.turn = 54;
    startPhase(b, 'USA');
    expect([at(b, fleet(b, 'USA', 'Pacific Fleet')), fleet(b, 'USA', 'Pacific Fleet').port]).toEqual(['Eastern Marshalls', 'Majuro']);
    const c = grand();
    c.byName['Eastern Marshalls'].ctrl = 'USA';
    c.turn = 54;
    startPhase(c, 'USA');
    expect(at(c, fleet(c, 'USA', 'Pacific Fleet'))).toBe('San Diego');
    // Japan's battle line goes to Truk in August 1942 by the book.
    const j = grand();
    setStop(j, 2, true);
    j.turn = 36;
    startPhase(j, 'JAP');
    expect(at(j, fleet(j, 'JAP', 'Combined Fleet'))).toBe('Truk');
    // A player's fleet moves when he says.
    const h = grand('USA');
    h.turn = 9;
    startPhase(h, 'USA');
    expect(at(h, fleet(h, 'USA', 'Pacific Fleet'))).toBe('San Diego');
  });

  it('sends ships from one fleet to another by date, never more than are there, and forms a later fleet in its month', () => {
    const g = grand();
    // (The Pacific's carriers are a row of their own, ruling 234(d): every carrier comes and goes on it.)
    const pf = fleet(g, 'USA', 'Pacific Fleet'), cf = fleet(g, 'USA', 'Pacific Carrier Fleet'), af = fleet(g, 'USA', 'Atlantic Fleet');
    const before = { pb: pf.ships.bb, pc: cf.ships.cv, ab: af.ships.bb, ac: af.ships.cv };
    expect(pf.ships.cv).toBe(0);
    g.turn = 21;
    startPhase(g, 'USA');
    expect([pf.ships.bb, cf.ships.cv, af.ships.bb, af.ships.cv]).toEqual([before.pb - 3, before.pc - 1, before.ab + 3, before.ac + 1]);
    expect(pf.ships.cv).toBe(0);
    expect(g.headlines.some((h) => h.text === 'The battleships Idaho, Mississippi and New Mexico leave for the Atlantic.')).toBe(true);
    expect(g.headlines.some((h) => h.text === 'The carrier Yorktown leaves for the Atlantic.')).toBe(true);
    const few = grand();
    fleet(few, 'USA', 'Pacific Fleet').ships.bb = 1;
    few.turn = 21;
    startPhase(few, 'USA');
    expect(fleet(few, 'USA', 'Pacific Fleet').ships.bb).toBe(0);
    expect(fleet(g, 'USA', 'Escort Carrier Fleet')).toBeUndefined();
    g.turn = 51;
    startPhase(g, 'USA');
    expect(fleet(g, 'USA', 'Escort Carrier Fleet').ships.cv).toBe(3);
    // On the older chart, where a history campaign is played, that row never comes.
    const old = createGame(3, null);
    if (old.nations.USA?.alive) { old.turn = 51; startPhase(old, 'USA'); expect(old.fleets.some((f) => f.name === 'Escort Carrier Fleet')).toBe(false); }
  });

  it("brings the Eagle to the Mediterranean in May 1940 from the China Fleet, which holds her from the first day: one carrier, counted once (ruling 233, 10:02Z)", () => {
    for (const stop of [1, 3, 5]) {
      const g = grand();
      setStop(g, stop, true);
      const china = fleet(g, 'UK', 'China Fleet'), med = fleet(g, 'UK', 'Mediterranean Fleet');
      expect(china.ships.cv, `stop ${stop}`).toBe(1);
      const was: Record<number, number[]> = {};
      for (let turn = 2; turn <= 9; turn++) {
        g.turn = turn;
        navalUpkeep(g, 'UK');
        was[turn] = [china.ships.cv, med.ships.cv];
      }
      // The Glorious away from 2 (and at stop 1 back at 5 and to the Home Fleet at 8): the Mediterranean Fleet has none
      // when the Eagle comes, and the China Fleet has none after.
      expect([was[8], was[9]], `stop ${stop}`).toEqual([[1, 0], [0, 1]]);
      expect(g.headlines.filter((h) => /Eagle/.test(h.text)).map((h) => [h.turn, h.text]), `stop ${stop}`)
        .toEqual([[9, 'The carrier Eagle leaves the China Fleet for the Mediterranean Fleet.']]);
      // The row's own key, as on the older chart, so a save or a stop turned later reads it once.
      expect(g.flags['arr:9:UK:Mediterranean Fleet'], `stop ${stop}`).toBe(9);
    }
    // A China Fleet that has lost its carrier sends none, and the Mediterranean Fleet has no second Eagle from off the chart.
    const lost = grand();
    fleet(lost, 'UK', 'China Fleet').ships.cv = 0;
    for (let turn = 2; turn <= 9; turn++) { lost.turn = turn; navalUpkeep(lost, 'UK'); }
    expect(fleet(lost, 'UK', 'Mediterranean Fleet').ships.cv).toBe(0);
    expect(lost.headlines.some((h) => /Eagle/.test(h.text))).toBe(false);
  });

  it('plays two months of a war nobody leads, and a campaign saved on it loads as it was', () => {
    const g = grand(null, 11);
    runTurnAI(g);
    runTurnAI(g);
    expect(g.turn).toBe(3);
    const back = loadGame(saveGame(g))!;
    expect(back).not.toBeNull();
    expect(back.regions.length).toBe(g.regions.length);
    expect(back.regions.map((r) => r.ctrl).join()).toBe(g.regions.map((r) => r.ctrl).join());
    expect(back.corps.map((c) => `${c.nat}|${c.name}|${c.reg}`).join()).toBe(g.corps.map((c) => `${c.nat}|${c.name}|${c.reg}`).join());
    for (const id of ['JAP', 'CHN', 'MAN', 'MGL', 'IRN', 'IRQ']) { expect(back.nations[id].atWar, id).toEqual(g.nations[id].atWar); expect(back.nations[id].capital, id).toBe(g.nations[id].capital); }
    expect(tiedPair(back, 'JAP', 'MAN')).toBe(true);
    expect(back.flags['tie:MAN:CHN']).toBe(g.flags['tie:MAN:CHN']);
  });
});

describe("a client's tie: the riders (far.ts)", () => {
  it('stands the two on each other\'s ground as allies, and makes them members of no alliance together', () => {
    const g = grand();
    for (const [c, p] of [['MAN', 'JAP'], ['MGL', 'SOV']]) {
      expect(allied(g, c, p), c).toBe(true);
      expect(sameSide(g, c, p), c).toBe(false);
    }
    expect(allied(g, 'MAN', 'SOV')).toBe(false);
    expect(allied(g, 'MAN', 'MGL')).toBe(false);
    // On the older chart there is no tie to read.
    expect(tiedPair(createGame(3, null), 'JAP', 'MAN')).toBe(false);
  });

  it('answers for a client the day war is made on it, and ends that war for both when the patron makes its peace', () => {
    const g = grand();
    declareWar(g, 'UK', 'MAN');
    expect(atWar(g, 'UK', 'JAP')).toBe(true);
    expect(g.headlines.some((h) => /war on Manchukuo is war on Japan/.test(h.text))).toBe(true);
    monthlyDiplomacy(g);
    signPeace(g, 'JAP', 'UK');
    monthlyDiplomacy(g);
    expect(atWar(g, 'UK', 'MAN')).toBe(false);
    expect(atWar(g, 'UK', 'JAP')).toBe(false);
  });

  it('ends the first day\'s war with China for Manchukuo the month Japan makes peace, and never before (the third rider)', () => {
    const g = grand();
    expect(tieWar(g, 'MAN', 'CHN')).toBe(true);
    expect(tieWar(g, 'CHN', 'MAN')).toBe(true);
    expect(tieWar(g, 'JAP', 'CHN')).toBe(false);
    // The armistice of a quiet year passes the pair over: Manchukuo and China touch nowhere and never fight.
    g.turn = 40;
    for (const id of ['MAN', 'CHN']) g.nations[id].lastFight = {};
    expect(stalemates(g).some(([a, b]) => [a, b].sort().join() === 'CHN,MAN')).toBe(false);
    expect(atWar(g, 'MAN', 'CHN')).toBe(true);
    signPeace(g, 'JAP', 'CHN');
    monthlyDiplomacy(g);
    expect(atWar(g, 'MAN', 'CHN')).toBe(false);
  });

  it('lets no army strike from a client\'s ground into a war the client is not in (no sanctuary)', () => {
    const g = grand();
    const guest = g.corps.find((c) => c.nat === 'SOV' && g.regions[c.reg].owner === 'MGL' && c.kind !== 'fort')!;
    const here = g.regions[guest.reg];
    const over = g.regions.find((r) => r.owner === 'CHN' && r.ctrl === 'CHN')!;
    // The Soviet Union goes to war with China; Mongolia, whose row names Japan and Manchukuo alone, does not.
    setWar(g, 'SOV', 'CHN');
    monthlyDiplomacy(g);
    expect(atWar(g, 'MGL', 'CHN')).toBe(false);
    expect(tieBars(g, guest, over, here.id)).toBe(true);
    expect(blocked(g, guest, over, here.id)).toBe(true);
    // From its own ground the same corps is barred by nothing of the tie's, and into a war Mongolia shares it may go.
    const home = g.regions.find((r) => r.ctrl === 'SOV')!;
    expect(tieBars(g, guest, over, home.id)).toBe(false);
    setWar(g, 'SOV', 'JAP');
    monthlyDiplomacy(g);
    const manchu = g.regions.find((r) => r.ctrl === 'MAN')!;
    expect(atWar(g, 'MGL', 'MAN')).toBe(true);
    expect(tieBars(g, guest, manchu, here.id)).toBe(false);
  });

  it('parts the two when the tie breaks: the patron keeps the ground its army stands on, and the client\'s wars are its own', () => {
    const g = grand('MAN');
    const stood = g.regions.filter((r) => r.ctrl === 'MAN' && r.units.some((u) => u.nat === 'JAP'));
    expect(stood.length).toBeGreaterThan(3);
    declareWar(g, 'MAN', 'JAP');
    expect(g.nations.MAN.alive).toBe(true);
    for (const r of stood) expect(r.ctrl, r.name).toBe('JAP');
    for (const c of g.corps.filter((x) => x.nat === 'MAN')) expect(g.regions[c.reg].ctrl, c.name).toBe('MAN');
    expect(g.headlines.some((h) => /^Japan keeps the ground its army stands on in Manchukuo: /.test(h.text))).toBe(true);
    expect(tiedPair(g, 'MAN', 'JAP')).toBe(false);
    expect(Object.keys(g.flags).filter((k) => /^(tie|tied|duty):MAN/.test(k))).toEqual([]);
    // Japan's peace with China is no longer Manchukuo's.
    signPeace(g, 'JAP', 'CHN');
    monthlyDiplomacy(g);
    expect(atWar(g, 'MAN', 'CHN')).toBe(true);
  });
});

describe("the far side is not America's side of the water", () => {
  it('counts no colony on the far side as an invader at her door, so that her army and Canada\'s still sail for Europe', () => {
    const g = grand();
    expect(farOn(g)).toBe(true);
    expect([0, WORLD_FIRST - 1, WORLD_FIRST, FAR_FIRST - 1, FAR_FIRST, g.regions.length - 1].map((id) => shoreOf(id))).toEqual([0, 0, 1, 1, 2, 2]);
    for (const nat of ['USA', 'CAN', 'BRA']) expect(shoreOf(g.nations[nat].capital), nat).toBe(1);
    // Italy holds her colony on the Horn of Africa from the first day: ground beyond the old chart, held by a power
    // whose capital stands on it, and no part of the Americas.
    const horn = g.regions.filter((r) => r.ctrl === 'ITA' && r.id >= FAR_FIRST);
    expect(horn.length).toBeGreaterThan(0);
    expect(g.regions.filter((r) => r.ctrl === 'ITA' && shoreOf(r) === 1)).toEqual([]);
    for (const nat of ['USA', 'CAN', 'BRA']) setWar(g, nat, 'ITA');
    for (const nat of ['USA', 'CAN', 'BRA']) expect(enemyAtTheDoor(g, nat), nat).toBe(false);
    expect(overseasOf(g, 'USA')).toBe(WORLD.overseas.USA);
    expect(overseasOf(g, 'CAN')).toBe(WORLD.overseas.CAN);
    // An Italian force ashore in the Americas is at the door, as it always was.
    const isle = g.regions.find((r) => shoreOf(r) === 1 && r.owner === 'UK' && r.overseas && r.seas.length > 0)!;
    expect(isle).toBeDefined();
    capture(g, isle, 'ITA');
    for (const nat of ['USA', 'CAN']) expect(enemyAtTheDoor(g, nat), nat).toBe(true);
    expect(overseasOf(g, 'USA')).toBeNull();
  });
});

// ---- The whole world's data gaps (the suite with the switch on, 18 November; the planner's paper 21) -----------------

describe("the whole world's names and marks (the planner's paper 21)", () => {
  it('names no American formation with a bracket, and no town twice that was not twice before', () => {
    const rows: [string, string][] = [...FW.world.corps.map((r: any[]) => [r[0], r[1]]), ...FW['setup-1939'].expeditions.map((r: any[]) => [r[1], r[2]]), ...FW['setup-1939'].formations.map((r: any[]) => [r[1], r[2]])];
    const american = rows.filter(([nat]) => nat === 'USA').map(([, name]) => name);
    expect(american.length).toBeGreaterThan(20);
    for (const name of american) expect(name).not.toMatch(/[()]/);
    expect(new Set(american).size).toBe(american.length);
    // Tehran and Makhachkala are provinces of the far side: the old chart's Iran and Dagestan take another town's name.
    const g = grand();
    expect(g.byName['Tehran'].id).toBeGreaterThanOrEqual(FAR_FIRST);
    expect(g.byName['Makhachkala'].id).toBeGreaterThanOrEqual(FAR_FIRST);
    expect([townName(g.byName['Iran']), townName(g.byName['Dagestan'])]).toEqual(['Tabriz', 'Buynaksk']);
    expect([townName(g.byName['Tehran']), townName(g.byName['Makhachkala'])]).toEqual(['Tehran', 'Makhachkala']);
    const seen = new Map<string, number>();
    for (const r of g.regions) seen.set(townName(r), (seen.get(townName(r)) ?? 0) + 1);
    // (England's Birmingham and Alabama's stand as they did on the Americas' chart.)
    expect([...seen].filter(([, n]) => n > 1).map(([t]) => t)).toEqual(['Birmingham']);
  });

  it("marks a city for Vichy as its province is marked, Indochina too, and France's fall follows the marks (ruling 199)", () => {
    const g = grand();
    const rows = WORLD_MAP.regions as unknown as { id: number; of?: number }[];
    let cities = 0;
    for (const row of rows) {
      if (row.of === undefined) continue;
      cities++;
      expect(g.regions[row.id].vichy, g.regions[row.id].name).toBe(g.regions[row.of].vichy);
    }
    expect(cities).toBeGreaterThan(300);
    for (const name of ['Hanoi', 'Saigon', 'Haiphong', 'Hue']) expect(g.byName[name]?.vichy, name).toBe(true);
    const marked = g.regions.filter((r) => r.id >= FAR_FIRST && r.owner === 'FRA' && r.vichy).map((r) => r.id);
    const free = g.regions.filter((r) => r.id >= FAR_FIRST && r.owner === 'FRA' && !r.vichy).map((r) => r.id);
    expect([marked.length, free.length]).toEqual([75, 32]);
    for (const [a, b] of [['FRA', 'GER'], ['UK', 'GER']]) if (!atWar(g, a, b)) setWar(g, a, b);
    capitulate(g, 'FRA', 'GER');
    // The marked ground is Vichy's; the rest (Chad, Cameroon, Equatorial Africa, New Caledonia) passes into British keeping.
    for (const id of marked) expect(g.regions[id].ctrl, g.regions[id].name).toBe('VIC');
    for (const id of free) expect(g.regions[id].ctrl, g.regions[id].name).toBe('UK');
    for (const name of ['New Caledonia', 'Fort-Lamy', 'Douala', 'Brazzaville']) expect(free).toContain(g.byName[name].id);
  });
});

describe("the whole world's oil (rulings 194 and 231)", () => {
  const line = (g: Game, nat: string, key: string) => stores(g, nat)!.oil.income.find((l) => l.key === key)!;

  it('gives every oilfield on the chart a yield of its own, and stands the Caspian\'s oil at Baku', () => {
    const g = grand();
    const far = g.regions.filter((r) => r.id >= FAR_FIRST && r.oil);
    expect(far.length).toBe(17);
    expect(Object.keys(FW.resources.oil.fields).sort()).toEqual(far.map((r) => r.name).sort());
    for (const r of g.regions) if (r.oil) expect(RES.oil.fields[r.name], r.name).toBeGreaterThan(0);
    // Kirovabad stood for Baku while Baku was off the chart's edge: its mark is off, on a fresh game and on a load.
    expect(FAR_SETUP.unmark).toEqual(['Kirovabad']);
    expect([g.byName['Kirovabad'].oil, g.byName['Baku'].oil]).toEqual([false, true]);
    // (The realism audit, item 14: the Caspian 46, Grozny 6, Maikop 4; the home wells' 8 on the old chart are Guryev 2,
    // Krasnovodsk 2, Sterlitamak 3 and the one at home here.)
    expect([RES.oil.fields.Baku, RES.oil.fields.Kirovabad]).toEqual([46, 46]);
    expect(fieldYield(g, g.byName['Kirovabad'])).toBe(0);
    const again = loadGame(saveGame(g))!;
    expect([again.byName['Kirovabad'].oil, again.byName['Baku'].oil]).toEqual([false, true]);
    // The Gulf's oil reaches Britain by its route and through Iraq's 12, as on the old chart (231, 1(d)).
    expect([g.byName['Ahwaz'].oil, g.byName['Dhahran'].oil, g.byName['Iraq'].oil]).toEqual([false, false, true]);
    // The Soviet home row stood for Emba, Turkmenia and Bashkiria, which are fields on the chart now.
    expect(RES.oil.home.SOV).toBe(1);
  });

  it("leaves each of the six powers the oil it makes at home on the old chart, to the unit, save the Soviet Union's one from North Sakhalin (194(j))", () => {
    const g = grand();
    // What each power's own fields and its wells at home gave on the first day with the far side off: fields, home.
    const old: Record<string, [number, number]> = { GER: [0, 14], UK: [0, 0], FRA: [0, 1], ITA: [0, 2.5], SOV: [56, 8], USA: [100, 20] };
    // (Japan keeps stocks too with the far side on, ruling 194 point 5: tests/far-oil-book.test.ts.)
    expect([...Object.keys(old), 'JAP'].sort()).toEqual([...RES.nations].sort());
    for (const [nat, [fields, home]] of Object.entries(old)) {
      const now = (line(g, nat, 'fields')?.amount ?? 0) + (line(g, nat, 'home')?.amount ?? 0);
      expect(now, nat).toBe(fields + home + (nat === 'SOV' ? 1 : 0));
      expect(line(g, nat, 'taken')?.amount ?? 0, nat).toBe(0);
    }
    expect(line(g, 'SOV', 'fields')).toMatchObject({ amount: 64, count: 7 });
    expect(line(g, 'SOV', 'home').amount).toBe(1);
  });

  it('draws nothing at home from a field on colony ground, for its owner or for a conqueror (194, point 1)', () => {
    const g = grand();
    const colony = g.regions.filter((r) => r.oil && r.overseas);
    expect(colony.map((r) => r.name).sort()).toEqual(['Bahrain', 'Balikpapan', 'Brunei', 'Dibrugarh', 'Hurghada', 'Medan', 'Palembang', 'Rawalpindi', 'Soerabaja', 'Tarakan', 'Yenangyaung']);
    for (const r of colony) expect(fieldOverseas(r), r.name).toBe(true);
    for (const r of g.regions) if (r.id < FAR_FIRST) expect(fieldOverseas(r), r.name).toBe(false);
    // Britain owns and holds six of them, and has no oilfield at home.
    const hers = colony.filter((r) => r.owner === 'UK' && r.ctrl === 'UK');
    expect(hers.length).toBe(6);
    expect(line(g, 'UK', 'fields')?.count ?? 0).toBe(0);
    expect(line(g, 'UK', 'fields')?.amount ?? 0).toBe(0);
    // Taken, they give the conqueror nothing at home either; a field on home ground gives him his share.
    const before = stores(g, 'GER')!.oil.made;
    const brunei = g.byName['Brunei'], baku = g.byName['Baku'];
    brunei.ctrl = 'GER';
    expect(stores(g, 'GER')!.oil.made).toBe(before);
    baku.ctrl = 'GER';
    expect(stores(g, 'GER')!.oil.made).toBeCloseTo(before + 46 * RES.oil.occupied, 9);
    // The prize card promises no oil for the depots from a colony's field, and still calls it an oilfield.
    const words = (name: string): string => prizeOf(g, g.byName[name].id, 'GER')!.items.find((i) => i.key === 'oil')!.text;
    brunei.ctrl = 'UK'; baku.ctrl = 'SOV';
    setWar(g, 'GER', 'UK'); setWar(g, 'GER', 'SOV');
    expect(words('Brunei')).toBe('An oilfield');
    expect(words('Baku')).toMatch(/^An oilfield: 27\.6 a month to your depots/);
  });
});

describe('a colony on the far side (ruling 231)', () => {
  it('never makes its home country feel the war coming, as a colony of the old chart or of the Americas still does', () => {
    const g = grand();
    // One war in the world: Britain's with Italy.
    for (const n of Object.values(g.nations)) n.atWar = [];
    setWar(g, 'UK', 'ITA');
    const beside = (name: string, nat: string): boolean => g.byName[name].nb.some((j) => g.regions[j].ctrl === nat);
    const homeTouches = (id: string): boolean => g.regions.some((r) => r.owner === id && r.ctrl === id && !r.overseas && r.nb.some((j) => ['UK', 'ITA'].includes(g.regions[j].ctrl)));
    // Portugal: Goa stands beside British India, and nothing of hers at home is near the war.
    expect(g.byName['Goa']).toMatchObject({ owner: 'POR', ctrl: 'POR', overseas: true });
    expect(shoreOf(g.byName['Goa'])).toBe(2);
    expect(beside('Goa', 'UK')).toBe(true);
    expect(homeTouches('POR')).toBe(false);
    expect(threatened(g, 'POR')).toBe(false);
    // The Netherlands: the Indies beside British Borneo count for nothing, and Dutch Guiana beside British Guiana counts as it always has.
    expect(homeTouches('NED')).toBe(false);
    expect(shoreOf(g.byName['Dutch Guiana'])).toBe(1);
    expect(beside('Dutch Guiana', 'UK')).toBe(true);
    expect(threatened(g, 'NED')).toBe(true);
    for (const r of g.regions) if (r.owner === 'NED' && shoreOf(r) === 1) r.ctrl = 'UK';
    expect(g.regions.some((r) => r.owner === 'NED' && r.ctrl === 'NED' && r.overseas && shoreOf(r) === 2 && r.nb.some((j) => g.regions[j].ctrl === 'UK'))).toBe(true);
    expect(threatened(g, 'NED')).toBe(false);
    // Belgium's Congo beside Uganda and Rhodesia, the same.
    expect(homeTouches('BEL')).toBe(false);
    expect(threatened(g, 'BEL')).toBe(false);
    // A far-side country's home ground reads as any country's: Thailand beside British Burma.
    expect(threatened(g, 'THA')).toBe(true);
    // The old chart's colonies count as they always have. One war again, France's with Italy: Britain's Egypt stands
    // beside Italian Libya and her Palestine beside French Syria, and nothing of hers at home or in the Americas is near it.
    for (const n of Object.values(g.nations)) n.atWar = [];
    setWar(g, 'FRA', 'ITA');
    const touches = (side: number, colony: boolean): boolean => g.regions.some((r) => r.owner === 'UK' && r.ctrl === 'UK' && r.overseas === colony && shoreOf(r) === side
      && r.nb.some((j) => ['FRA', 'ITA'].includes(g.regions[j].ctrl)));
    expect([0, 1, 2].map((side) => touches(side, false))).toEqual([false, false, false]);
    expect([touches(0, true), touches(1, true), touches(2, true)]).toEqual([true, false, true]);
    expect(threatened(g, 'UK')).toBe(true);
    for (const r of g.regions) if (r.owner === 'UK' && r.overseas && shoreOf(r) === 0) r.ctrl = 'BEL';
    expect(touches(2, true)).toBe(true);
    expect(threatened(g, 'UK')).toBe(false);
  });

  it("is no harbour for Canada's army: the island's guard holds against the colony exception where history's dates do", () => {
    const tripoli = (g: Game): Game['regions'][number] => g.byName['Tripoli'];
    const g = grand();
    expect(tripoli(g)).toMatchObject({ owner: 'ITA', overseas: true });
    for (const [a, b] of [['UK', 'ITA'], ['CAN', 'ITA'], ['USA', 'ITA']]) if (!atWar(g, a, b)) setWar(g, a, b);
    for (const turn of [10, 28, 40]) {
      g.turn = turn;
      // Britain's harbours are Canada's base at any time; an Italian colony is none of hers, and is America's as ruling 60 gave it.
      expect(joinsOn(g, 'CAN', g.byName['Kent']), `month ${turn}`).toBe(true);
      expect(joinsOn(g, 'CAN', tripoli(g)), `month ${turn}`).toBe(false);
      expect(joinsOn(g, 'CAN', g.byName['Alexandria']), `month ${turn}`).toBe(false);
      expect(joinsOn(g, 'USA', tripoli(g)), `month ${turn}`).toBe(true);
    }
    // From 1943 she goes on to the continent behind a British or an American army. A colony across an ocean stays
    // shut to her for good (the rider, b462), on the old chart and on the far side, whoever holds it or landed there.
    g.turn = 41;
    for (const name of ['Tripoli', 'Alexandria', 'Mombasa']) {
      expect(g.byName[name].overseas, name).toBe(true);
      expect(joinsOn(g, 'CAN', g.byName[name]), name).toBe(false);
      expect(joinsOn(g, 'CAN', g.byName[name], 'UK'), name).toBe(false);
      expect(joinsOn(g, 'USA', g.byName[name]), name).toBe(true);
    }
    // Home ground out there is on her list or it is not, as anywhere: Abadan is Iran's, and no country of her list.
    // A colony on her own side of the ocean is open to her as it always was (Trinidad): no ocean is crossed for it.
    expect(g.byName['Abadan']).toMatchObject({ owner: 'IRN', overseas: false });
    expect(joinsOn(g, 'CAN', g.byName['Abadan'], 'UK')).toBe(false);
    expect(g.byName['Trinidad']).toMatchObject({ owner: 'UK', overseas: true });
    expect(shoreOf(g.byName['Trinidad'])).toBe(1);
    for (const turn of [10, 40, 41, 60]) { g.turn = turn; expect(joinsOn(g, 'CAN', g.byName['Trinidad']), `month ${turn}`).toBe(true); }
    // At the two loosest stops the staff keeps its own rules.
    for (const stop of [4, 5]) {
      const h = grand();
      expect(setStop(h, stop, true)).toBe(true);
      expect(joinsOn(h, 'CAN', tripoli(h)), `stop ${stop}`).toBe(true);
    }
  });
});

describe("a far colony's purse and its factories (the rider b462 to ruling 231)", () => {
  it('pays its own country a tenth of what it paid, a conqueror his usual share, and an older colony as ever', () => {
    const g = grand();
    const calcutta = g.byName['Calcutta'];
    expect(calcutta).toMatchObject({ owner: 'UK', ctrl: 'UK', overseas: true, fac: 3, port: true });
    expect(farColony(calcutta)).toBe(true);
    expect(BUDGET.farColony).toMatchObject({ land: 0.1, port: 0.1, oil: 0.1, fac: 0.1 });
    // Its own country: every part a tenth of a colony's pay.
    const hers = provinceYield(g, calcutta, 'UK');
    expect(hers.fac).toBeCloseTo(3 * 0.1, 9);
    expect(hers.port).toBeCloseTo(BUDGET.port * BUDGET.overseas * landFactor(calcutta) * 0.1, 9);
    expect(hers.land).toBeCloseTo(BUDGET.land[calcutta.terrain] * BUDGET.overseas * landFactor(calcutta) * landShare(calcutta) * 0.1, 9);
    expect(hers.total).toBeCloseTo(hers.fac + hers.port + hers.land, 9);
    const brunei = g.byName['Brunei'];
    expect(provinceYield(g, brunei, 'UK').oil).toBeCloseTo(BUDGET.oil * BUDGET.overseas * 0.1, 9);
    // A conqueror: what conquered ground has always paid.
    const his = provinceYield(g, calcutta, 'JAP');
    expect(his.fac).toBeCloseTo(3 * PRODUCTION.occupiedIncome, 9);
    expect(his.port).toBeCloseTo(BUDGET.port * BUDGET.occupied * landFactor(calcutta), 9);
    expect(provinceYield(g, brunei, 'JAP').oil).toBeCloseTo(BUDGET.oil * BUDGET.occupied, 9);
    // The colonies of the old chart and of the Americas, and home ground on the far side: no factor.
    for (const name of ['Alexandria', 'Trinidad']) {
      const r = g.byName[name];
      expect(r.overseas && !farColony(r), name).toBe(true);
      expect(provinceYield(g, r, 'UK').port, name).toBeCloseTo(BUDGET.port * BUDGET.overseas * landFactor(r), 9);
    }
    // (Home ground out there is no colony. The Soviet Union's beyond the old chart counts at a share of its own, below.)
    const sverdlovsk = g.regions.find((r) => r.id >= FAR_FIRST && r.owner === 'SOV' && r.fac > 0 && !r.overseas)!;
    expect(farColony(sverdlovsk)).toBe(false);
    expect(provinceYield(g, sverdlovsk, 'SOV').fac).toBeCloseTo(sverdlovsk.fac * farShare(sverdlovsk), 9);
    const tokyo = g.regions.find((r) => r.owner === 'JAP' && r.fac > 1 && !r.overseas)!;
    expect(provinceYield(g, tokyo, 'JAP').fac).toBe(tokyo.fac);
    // Britain's far colonies together: about 3.5 a month where they paid 35.4.
    let sum = 0, n = 0;
    for (const r of g.regions) if (r.owner === 'UK' && r.ctrl === 'UK' && farColony(r)) { sum += provinceYield(g, r, 'UK').total; n++; }
    expect(n).toBeGreaterThan(250);
    expect(sum).toBeGreaterThan(3.4);
    expect(sum).toBeLessThan(3.7);
  });

  it("pays Japan for Korea, Formosa and Karafuto as her home ground does (the historian's exception)", () => {
    const g = grand();
    // (The list names America's Hawaii too, ruling 234(e)3: below.)
    const named = new Set(BUDGET.farColony.home.filter((name) => g.byName[name]?.owner === 'JAP'));
    expect(named.size).toBe(12);
    // Twenty-two of Japan's provinces carry the colony mark: the Mandates' nine, Dairen, and the twelve named.
    const marked = g.regions.filter((r) => r.owner === 'JAP' && r.ctrl === 'JAP' && r.overseas);
    expect(marked.length).toBe(22);
    expect(marked.filter((r) => named.has(r.name)).map((r) => r.name).sort()).toEqual([...named].sort());
    for (const name of named) expect(g.byName[name], name).toMatchObject({ owner: 'JAP', overseas: true });
    // The named twelve pay her as home ground does: a factory its whole point, land and a harbour with no share taken.
    for (const r of marked.filter((x) => named.has(x.name))) {
      expect(colonyPurse(r) || farColony(r), r.name).toBe(false);
      const y = provinceYield(g, r, 'JAP');
      expect(y.fac, r.name).toBe(r.fac);
      expect(y.land, r.name).toBeCloseTo(BUDGET.land[r.terrain] * landFactor(r) * landShare(r), 9);
      expect(y.port, r.name).toBeCloseTo(r.port ? BUDGET.port * landFactor(r) : 0, 9);
      // The mark still says where her corps are raised: in the home islands, as before.
      expect(canRaise(g, 'JAP', r), r.name).toBe(false);
    }
    expect(g.byName['Hamhung'].fac + g.byName['Formosa'].fac).toBe(2);
    // The Mandates and Dairen are a purse: a tenth, as India is to Britain.
    const rest = marked.filter((r) => !named.has(r.name));
    expect(rest.length).toBe(10);
    for (const r of rest) {
      expect(farColony(r), r.name).toBe(true);
      const y = provinceYield(g, r, 'JAP');
      expect(y.fac, r.name).toBeCloseTo(r.fac * 0.1, 9);
      expect(y.port, r.name).toBeCloseTo(r.port ? BUDGET.port * BUDGET.overseas * landFactor(r) * 0.1 : 0, 9);
    }
    // Manchukuo is its own country and stays as it is: no province of it is Japan's, and it pays itself in full.
    const man = g.regions.filter((r) => r.owner === 'MAN');
    expect(man.length).toBeGreaterThan(0);
    for (const r of man) expect(r.overseas, r.name).toBe(false);
  });

  it("Hawaii is America's home ground, an incorporated territory and no colony (ruling 234(e)3)", () => {
    const g = grand();
    const hawaii = ['Pearl Harbor', 'Honolulu', 'Maui', 'Hawaii', 'Kauai'];
    expect(BUDGET.farColony.home.filter((name) => g.byName[name]?.owner === 'USA')).toEqual(hawaii);
    for (const name of hawaii) {
      const r = g.byName[name];
      expect(r, name).toMatchObject({ owner: 'USA', ctrl: 'USA', overseas: true });
      expect(colonyPurse(r) || farColony(r), name).toBe(false);
      const y = provinceYield(g, r, 'USA');
      // As home ground pays: land and a harbour with no share taken, and no tenth.
      expect(y.land, name).toBeCloseTo(BUDGET.land[r.terrain] * landShare(r), 9);
      expect(y.port, name).toBeCloseTo(r.port ? BUDGET.port : 0, 9);
      expect(y.fac, name).toBe(r.fac);
    }
    // Alaska, Guam, Wake, Midway, Samoa and the Philippines are a purse still: a tenth.
    for (const name of ['Anchorage', 'Guam', 'Wake', 'Midway', 'Samoa', 'Manila']) {
      const r = g.byName[name];
      expect(farColony(r), name).toBe(true);
      expect(provinceYield(g, r, 'USA').port, name).toBeCloseTo(BUDGET.port * BUDGET.overseas * 0.1, 9);
    }
  });

  it("a far post's guard is its record garrison, counted once; other far colonial towns keep a tenth of a town's guard; home ground keeps it whole (ruling 234's rider b547)", () => {
    const g = grand();
    expect(BUDGET.farColony.guard).toBe(0.1);
    // (Riders b561(d) and b563: "counted once" reaches the far colonial cities whose defenders the rows already hold.)
    // (Rider b614: Japan's island garrisons are rows at the record's worth, so her islands are posts too; three of them
    // are her own home ground. tests/far-garrisons.test.ts has the rows.)
    const HOME_POSTS = ['Marcus Island', 'Iwo Jima', 'Chichi Jima'];
    expect(BUDGET.farColony.posts).toEqual(['Guam', 'Kuching', 'Tarakan', 'Balikpapan', 'Rabaul', 'Ambon', 'Port Moresby', 'Hong Kong', 'Manila', 'Rangoon', 'Batavia', 'Koepang', 'Palembang', 'Medan', 'Wake', 'Hanoi', 'Saigon', 'Hue', 'Vientiane', 'Colombo', 'Singapore',
      'Truk', 'Palau', 'Saipan', 'Kwajalein', 'Eniwetok', 'Eastern Marshalls', 'Ulithi', 'Yap', 'Ponape', ...HOME_POSTS]);
    // A town's guard as siege.json reckons it for any town: by its size, capped by its country's army.
    const whole = (name: string): number => { const r = g.byName[name], n = g.nations[r.owner]; return Math.min(SIEGE.guard[townSize(g, r)], SIEGE.guardShare * Math.max(n.armyPeak, armySize(g, n.id))); };
    // The posts whose garrison the historian has given (ruling 234(b)1, rider b541): the record's figure is the whole
    // defence, and the town keeps no guard beside it.
    for (const name of BUDGET.farColony.posts!) {
      const r = g.byName[name];
      expect(r, name).toBeTruthy();
      expect(farColony(r), name).toBe(!HOME_POSTS.includes(name));
      expect(farGuard(r), name).toBe(0);
      expect(guardOf(g, r), name).toBe(0);
    }
    // Elsewhere on far colonial ground the far side's one number is the tenth: Malaya, the Philippines, Burma and India
    // outside the cities named, Midway beside its fort and the Pacific's islands.
    // (Since the guard table of 21 November 2026 a land with a row stands at the record's figure for the place, with no
    // tenth and no cap: far-guards.json, tests/far-guards.test.ts. The tenth, and home ground's whole guard, are the
    // reckoning of far ground under no row yet, and those are the places the guard itself is read at here.)
    const tenth: string[] = [];
    for (const name of ['Kota Bharu', 'Legaspi', 'Penang', 'Johore', 'Moulmein', 'Lingayen', 'Calcutta', 'Midway', 'Guadalcanal', 'Tarawa', 'Fiji', 'New Caledonia']) {
      const r = g.byName[name];
      expect(farColony(r), name).toBe(true);
      expect(r.ctrl, name).toBe(r.owner);
      expect(farGuard(r), name).toBe(0.1);
      expect(whole(name), name).toBeGreaterThan(0.2);
      if (farGuardRow(g, r)) continue;
      tenth.push(name);
      expect(guardOf(g, r), name).toBeCloseTo(whole(name) * 0.1, 9);
    }
    expect(tenth).toEqual(['Midway', 'Guadalcanal', 'Tarawa', 'Fiji', 'New Caledonia']);
    // Home ground keeps its guard whole: Hawaii (home by ruling 234(e)3), Japan's own and Korea and Formosa,
    // Australia's, New Zealand's, China's and Siam's; and the near side's game is untouched.
    const wholly: string[] = [];
    for (const name of ['Honolulu', 'Tokyo', 'Okinawa', 'Seoul', 'Formosa', 'Brisbane', 'Darwin', 'Chungking', 'Bangkok', 'Pearl Harbor', 'Karafuto', 'Auckland', 'Wellington', 'London', 'Berlin', 'Paris', 'Moscow']) {
      const r = g.byName[name];
      expect(farColony(r), name).toBe(false);
      expect(farGuard(r), name).toBe(1);
      expect(whole(name), name).toBeGreaterThan(0);
      if (farGuardRow(g, r)) continue;
      wholly.push(name);
      expect(guardOf(g, r), name).toBeCloseTo(whole(name), 9);
    }
    expect(wholly).toEqual(['Pearl Harbor', 'Karafuto', 'Auckland', 'Wellington', 'London', 'Berlin', 'Paris', 'Moscow']);
    // Everywhere: the share is under 1 on far colonial ground and at the three home posts, and nowhere else, the old
    // chart's colonies included.
    let near = 0;
    for (const r of g.regions) {
      expect(farGuard(r) < 1, r.name).toBe(farColony(r) || HOME_POSTS.includes(r.name));
      if (r.overseas && shoreOf(r) !== 2) { near++; expect(farGuard(r), r.name).toBe(1); }
    }
    expect(near).toBeGreaterThan(0);
  });

  it("one cut, not two: on a far colony the tenth stands in place of ruling 214's factor, which falls on the rest of that country's ground (ruling 234(e)3)", () => {
    const g = grand();
    const rows: [string, string, string][] = [['NED', 'Batavia', 'Holland'], ['POR', 'Goa', 'Lisbon'], ['AUS', 'Port Moresby', 'Sydney'], ['NZL', 'Apia', 'Wellington'], ['SAF', 'Windhoek', 'Cape Town']];
    for (const [nat, colony, home] of rows) {
      const c = g.byName[colony], h = g.byName[home];
      expect(landFactor(c), colony).toBe(BUDGET.landFactor[nat]);
      expect(landFactor(c), colony).toBeLessThan(1);
      expect([c.owner, farColony(c)], colony).toEqual([nat, true]);
      const y = provinceYield(g, c, nat);
      // The colony: the colony's half and the tenth, as Britain's Singapore pays, and no factor.
      expect(y.land, colony).toBeCloseTo(BUDGET.land[c.terrain] * BUDGET.overseas * landShare(c, true) * 0.1, 9);
      expect(y.port, colony).toBeCloseTo(c.port ? BUDGET.port * BUDGET.overseas * 0.1 : 0, 9);
      // The town card says the same.
      expect(worthOf(g, c), colony).toBe(Math.max(1, Math.round((y.land + y.port + y.oil + y.fac) * BUDGET.scale)));
      // The rest of its ground: the factor, as before.
      expect(h, home).toMatchObject({ owner: nat });
      expect(farColony(h), home).toBe(false);
      expect(provinceYield(g, h, nat).land, home).toBeCloseTo(BUDGET.land[h.terrain] * BUDGET.landFactor[nat], 9);
    }
    // A conqueror takes no tenth, and the factor goes with the ground as it did.
    const batavia = g.byName['Batavia'];
    if (!atWar(g, 'JAP', 'NED')) declareWar(g, 'JAP', 'NED');
    capture(g, batavia, 'JAP');
    expect(provinceYield(g, batavia, 'JAP', true).port).toBeCloseTo(BUDGET.port * BUDGET.occupied * BUDGET.landFactor.NED, 9);
    // Trade reads the land bare, as before: no factor and no change.
    const goa = g.byName['Goa'];
    expect(provinceYield(g, goa, 'POR', false, true).land).toBeCloseTo(BUDGET.land[goa.terrain] * BUDGET.overseas * landShare(goa, true) * 0.1, 9);
  });

  it("pays an ally that keeps it for a fallen country a tenth of a keeper's share: the Indies are no purse of London's", () => {
    const g = grand();
    const palembang = g.byName['Palembang'], batavia = g.byName['Batavia'];
    expect(palembang).toMatchObject({ owner: 'NED', ctrl: 'NED', overseas: true, oil: true });
    expect(batavia).toMatchObject({ owner: 'NED', overseas: true, port: true });
    const indies = g.regions.filter((r) => r.owner === 'NED' && farColony(r));
    const whole = indies.reduce((t, r) => t + provinceYield(g, r, 'UK', true).total, 0);
    // (One cut, not two: a keeper's tenth is of the ground's pay with no factor of ruling 214 on it. Ruling 234(e)3.)
    const unfactored = indies.reduce((t, r) => { const y = provinceYield(g, r, 'UK', true), f = landFactor(r); return t + (y.land / f) * landShare(r, true) + y.port / f + y.oil + y.fac; }, 0);
    // May 1940: the Netherlands fall to Germany, and their colonies beyond the old chart pass into Britain's keeping.
    // (Attacked, they stand with Germany's enemies: that is what makes Britain their keeper and no conqueror.)
    if (!atWar(g, 'UK', 'GER')) setWar(g, 'UK', 'GER');
    declareWar(g, 'GER', 'NED');
    capitulate(g, 'NED', 'GER');
    expect(g.nations.NED.alive).toBe(false);
    expect([palembang.ctrl, batavia.ctrl]).toEqual(['UK', 'UK']);
    expect(allied(g, 'UK', 'NED')).toBe(true);
    const kept = provinceYield(g, palembang, 'UK');
    expect(kept.oil).toBeCloseTo(BUDGET.oil * BUDGET.occupied * 0.1, 9);
    expect(kept.port).toBeCloseTo(palembang.port ? BUDGET.port * BUDGET.occupied * 0.1 : 0, 9);
    expect(provinceYield(g, batavia, 'UK').port).toBeCloseTo(BUDGET.port * BUDGET.occupied * 0.1, 9);
    // All the Indies together: under one a month to their keeper, where a conqueror's share was near seven.
    const keeping = g.regions.filter((r) => r.owner === 'NED' && r.ctrl === 'UK' && farColony(r)).reduce((t, r) => t + provinceYield(g, r, 'UK').total, 0);
    expect(whole).toBeGreaterThan(6);
    expect(keeping).toBeCloseTo(unfactored * 0.1, 6);
    expect(keeping).toBeLessThan(1.5);
    // A conqueror is paid as ever: Japan is no ally of the Netherlands.
    expect(allied(g, 'JAP', 'NED')).toBe(false);
    expect(provinceYield(g, palembang, 'JAP').oil).toBeCloseTo(BUDGET.oil * BUDGET.occupied, 9);
    // A colony of the fallen country nearer home, kept the same way, pays its keeper as it always has.
    const near = g.regions.find((r) => r.owner === 'NED' && r.overseas && shoreOf(r) === 1 && r.ctrl === 'UK')!;
    expect(near).toBeDefined();
    expect(provinceYield(g, near, 'UK').land).toBeCloseTo(BUDGET.land[near.terrain] * BUDGET.occupied * landFactor(near) * landShare(near), 9);
  });

  it('forms corps by the factories of the home country alone: a colony\'s never add a line', () => {
    const g = grand();
    const count = (colony: boolean): number => g.regions.filter((r) => r.owner === 'UK' && r.ctrl === 'UK' && !!r.overseas === colony).reduce((s, r) => s + r.fac, 0);
    expect([count(false), count(true)]).toEqual([18, 17]);
    expect(lineFactories(g, 'UK')).toBe(18);
    expect(PRODUCTION.aiLine).toBe(6);
    // Late enough that every scheduled formation is in, an empty programme and a full treasury: three lines, not five.
    g.turn = 80; g.nations.UK.queue = []; g.nations.UK.pp = 400;
    aiProduction(g, 'UK');
    const forming = g.nations.UK.queue.filter((b) => (b.what === 'corps' && b.kind !== 'eng') || b.what === 'fort').length;
    expect(forming).toBeGreaterThan(0);
    expect(forming).toBeLessThanOrEqual(3);
    // Korea's, Formosa's and Karafuto's count with Japan's own (the historian's exception): the home islands'
    // factories and the two of Hamhung and Formosa. Dairen's adds none.
    const islands = g.regions.filter((r) => r.owner === 'JAP' && r.ctrl === 'JAP' && !r.overseas).reduce((s, r) => s + r.fac, 0);
    expect(g.byName['Dairen'].fac).toBe(1);
    expect(lineFactories(g, 'JAP')).toBe(islands + 2);
    // Home ground on the far side counts with the home country's: Australia's works are her own.
    expect(lineFactories(g, 'AUS')).toBe(g.regions.filter((r) => r.owner === 'AUS' && r.ctrl === 'AUS' && !r.overseas).reduce((s, r) => s + r.fac, 0));
    expect(lineFactories(g, 'AUS')).toBeGreaterThan(0);
  });

  it("shows on a town's coins what the ground truly pays its own country, after the tenth (ruling b466)", () => {
    const g = grand();
    // Every province in its own country's hands: the worth a town shows is the pay, in money as the interface shows it.
    for (const r of g.regions) {
      if (r.ctrl !== r.owner) continue;
      const pays = provinceYield(g, r, r.owner, true).total;
      expect(worthOf(g, r), r.name).toBe(pays > 0 ? Math.max(1, Math.round(pays * BUDGET.scale)) : 0);
    }
    // Calcutta, three factories and a harbour: it paid Britain over 300 and pays her a tenth of it.
    const calcutta = g.byName['Calcutta'];
    expect(townAt(g, calcutta.id)!.worth).toBe(worthOf(g, calcutta));
    expect(worthOf(g, calcutta)).toBeGreaterThan(30);
    expect(worthOf(g, calcutta)).toBeLessThan(36);
    // Hamhung, Japan's home ground by the exception: its factory whole.
    expect(worthOf(g, g.byName['Hamhung'])).toBeGreaterThan(100);
    // An older colony reads as it always did.
    const alexandria = g.byName['Alexandria'];
    expect(worthOf(g, alexandria)).toBe(Math.max(1, Math.round((BUDGET.land[alexandria.terrain] * BUDGET.overseas * landFactor(alexandria) * landShare(alexandria) + BUDGET.port * BUDGET.overseas * landFactor(alexandria) + alexandria.fac) * BUDGET.scale)));
  });

  it("counts the Soviet Union's ground beyond the old chart at its share, for all it pays and for lines (ruling b466)", () => {
    const g = grand();
    const F = BUDGET.farHome.SOV;
    // The share is a tenth, the same tenth a far colony pays: the game has one far-side number (ruling b471).
    expect(BUDGET.farHome).toEqual({ SOV: 0.1 });
    expect(F).toBe(BUDGET.farColony.fac);
    const hers = (shore: number): typeof g.regions => g.regions.filter((r) => r.owner === 'SOV' && r.ctrl === 'SOV' && shoreOf(r) === shore);
    const sum = (rs: typeof g.regions): number => rs.reduce((t, r) => t + r.fac, 0);
    const west = hers(0), east = hers(2);
    // The old chart's fifty-seven factories already carry the Soviet war economy; the fifty-seven beyond it count at the share.
    expect([sum(west), sum(east)]).toEqual([57, 57]);
    expect(east.length).toBe(169);
    for (const r of west) { expect(farShare(r), r.name).toBe(1); expect(provinceYield(g, r, 'SOV').fac, r.name).toBe(r.fac); }
    // One factor on the whole of what the ground pays: factories, land, harbour and an oilfield's money alike.
    for (const r of east) {
      expect(farShare(r), r.name).toBe(F);
      const y = provinceYield(g, r, 'SOV');
      expect(y.fac, r.name).toBeCloseTo(r.fac * F, 9);
      expect(y.land, r.name).toBeCloseTo(BUDGET.land[r.terrain] * landFactor(r) * landShare(r) * F, 9);
      expect(y.port, r.name).toBeCloseTo(r.port ? BUDGET.port * landFactor(r) * F : 0, 9);
      expect(y.oil, r.name).toBeCloseTo(r.oil ? BUDGET.oil * F : 0, 9);
      expect(y.total, r.name).toBeCloseTo(y.fac + y.land + y.port + y.oil, 9);
    }
    expect(lineFactories(g, 'SOV')).toBeCloseTo(57 + 57 * F, 9);
    // An oilfield's oil is another matter (ruling 194): Guryev's wells give her depots what they always did.
    const field = g.byName['Guryev'];
    expect(field).toMatchObject({ owner: 'SOV', oil: true });
    expect(east).toContain(field);
    expect(fieldYield(g, field)).toBe(RES.oil.fields[field.name] ?? RES.oil.fields._default);
    // Her town's coins tell the same sum.
    const works = east.find((r) => r.fac > 0)!;
    expect(worthOf(g, works)).toBe(F > 0 ? Math.max(1, Math.round(provinceYield(g, works, 'SOV', true).total * BUDGET.scale)) : 0);
    // The share goes with the ground: a conqueror's is reckoned on the same count.
    const taken = provinceYield(g, works, 'JAP');
    expect(taken.fac).toBeCloseTo(works.fac * F * PRODUCTION.occupiedIncome, 9);
    expect(taken.land).toBeCloseTo(BUDGET.land[works.terrain] * BUDGET.occupied * landFactor(works) * landShare(works) * F, 9);
    // No other country's home ground out there takes a share.
    for (const r of g.regions) if (r.owner !== 'SOV' && shoreOf(r) === 2) expect(farShare(r), r.name).toBe(1);
    // The factories that go east ahead of the enemy count whole wherever they land: the trait's share of an old-chart
    // factory lost to Germany goes on earning. One lost beyond the old chart is worth no more gone than where it stood.
    const move = (TRAITS as unknown as { nations: Record<string, { traits: { id: string; relocate?: number }[] }> }).nations.SOV.traits.find((t) => t.id === 'urals')!.relocate!;
    expect(move).toBeGreaterThan(0);
    setWar(g, 'GER', 'SOV');
    const urals = (): number => traitMoney(g, 'SOV').lines.find((l) => l.key === 'trait:urals')?.amount ?? 0;
    const lost = west.find((r) => r.fac > 0)!;
    expect(urals()).toBe(0);
    lost.ctrl = 'GER';
    expect(urals()).toBeCloseTo(lost.fac * move, 9);
    works.ctrl = 'GER';
    expect(urals()).toBeCloseTo((lost.fac + works.fac * F) * move, 9);
  });

  it("reckons the Soviet call-up by the industry she holds with her far side at the share (ruling b475)", () => {
    const g = grand();
    const F = BUDGET.farHome.SOV;
    expect(industryHeld(g, 'SOV')).toBe(1);
    // Germany takes about twenty of the old chart's fifty-seven factories; the fifty-seven beyond it are never lost.
    let lost = 0;
    for (const r of g.regions) if (lost < 20 && r.owner === 'SOV' && shoreOf(r) === 0 && r.fac > 0 && !r.cap) { r.ctrl = 'GER'; lost += r.fac; }
    expect(lost).toBeGreaterThanOrEqual(20);
    expect(industryHeld(g, 'SOV')).toBeCloseTo((57 - lost + 57 * F) / (57 + 57 * F), 9);
    // Counted whole, the far side held a wave near its full size; at the share it reads much as the old chart did.
    expect(industryHeld(g, 'SOV')).toBeLessThan((114 - lost) / 114 - 0.1);
    expect(industryHeld(g, 'SOV') - (57 - lost) / 57).toBeLessThan(0.05);
    // A country with no home ground out there reads as it always did.
    expect(industryHeld(g, 'GER')).toBe(1);
  });

  it("counts the month's class of men by the homeland she holds with her far side at the share (ruling b483)", () => {
    const g = grand();
    const F = BUDGET.farHome.SOV;
    expect(homeShare(g, 'SOV')).toBe(1);
    const full = classNow(g, 'SOV');
    expect(full).toBe(MANPOWER.class.SOV);
    // The worth of her home ground, where her young men are: the far side outweighs the old chart, and is never lost.
    const worth = (r: Game['regions'][number]): number => 1 + r.vp + r.fac;
    let old = 0, far = 0;
    for (const r of g.regions) if (r.owner === 'SOV' && !r.overseas) { if (shoreOf(r) === 2) far += worth(r); else old += worth(r); }
    expect(far).toBeGreaterThan(old);
    // Germany takes about a third of the old chart's worth.
    let lost = 0;
    for (const r of g.regions) if (lost < old / 3 && r.owner === 'SOV' && shoreOf(r) === 0 && !r.cap) { r.ctrl = 'GER'; lost += worth(r); }
    expect(lost).toBeGreaterThanOrEqual(old / 3);
    expect(homeShare(g, 'SOV')).toBeCloseTo((old - lost + far * F) / (old + far * F), 9);
    expect(classNow(g, 'SOV')).toBeCloseTo(full * homeShare(g, 'SOV'), 9);
    // Counted whole, the far side kept the class near its full size; at the share it reads much as the old chart did.
    expect(homeShare(g, 'SOV')).toBeLessThan((old + far - lost) / (old + far) - 0.1);
    expect(homeShare(g, 'SOV') - (old - lost) / old).toBeLessThan(0.05);
    // A country with no home ground out there reads as it always did.
    const before = homeShare(g, 'GER');
    expect(before).toBe(1);
    const ruhr = g.regions.filter((r) => r.owner === 'GER' && !r.overseas).sort((x, y) => worth(y) - worth(x))[0];
    let all = 0;
    for (const r of g.regions) if (r.owner === 'GER' && !r.overseas) all += worth(r);
    ruhr.ctrl = 'FRA';
    expect(homeShare(g, 'GER')).toBeCloseTo((all - worth(ruhr)) / all, 12);
  });
});
