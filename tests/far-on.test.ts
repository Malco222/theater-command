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
import { monthlyDiplomacy, signPeace } from '../src/campaign/diplomacy';
import { tieBars, tieWar, tiedPair } from '../src/campaign/far';
import { FAR_LAID, FAR_PATHS, FAR_SETUP } from '../src/campaign/far-rows';
import { allied, atWar, bloc, createGame, declareWar, sameSide, setWar, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { small } from '../src/campaign/league';
import { NAVY } from '../src/campaign/navy';
import { blocked } from '../src/campaign/rivers';
import { loadGame, saveGame } from '../src/campaign/save';
import { stalemates, startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import { FLAGS } from '../src/campaign/ui/flags';
import { FAR_FIRST, WORLD, WORLD_FIRST, WORLD_REGIONS, farOn } from '../src/campaign/world';

const FW = farWorld as any;
const WD = worldData as any;
const grand = (human: string | null = null, seed = 3): Game => startOpen(createGame(seed, human));
const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const at = (g: Game, f: Fleet): string => g.regions[f.base].name;
const SEATS = ['SAF', 'IRN', 'IRQ', 'SAU', 'AFG', 'YEM', 'NEP', 'LBR', 'CHN', 'AUS', 'NZL', 'THA', 'MAN', 'MGL'];

describe("the far side with its switch on: the rows that are nobody's seat", () => {
  it('lays the six files over the data, in order, and the game reads them as its own', () => {
    expect(FAR_LAID).toEqual(['far-seats-globe', 'far-seats-pacific', 'far-world', 'china-line', 'china-setup', 'china-stop3']);
    expect(WD.nations.JAP.name).toBe('Japan');
    expect(WD.great).toEqual(['USA', 'JAP']);
    expect(Object.keys(WD.clients)).toEqual(['MAN', 'MGL']);
    // A great power founds no League; the clients keep a small country's card, and are barred by their tie (league.ts).
    expect((leagueData as any).never).toContain('JAP');
    for (const id of ['MAN', 'MGL']) { expect((leagueData as any).never, id).not.toContain(id); expect(small(id), id).toBe(true); }
    expect(small('JAP')).toBe(false);
    expect(NAVY.moves!.length).toBe(FW.navy.moves.length);
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
    for (const id of new Set(g.regions.flatMap((r) => [r.owner, r.ctrl]))) {
      expect(g.nations[id], id).toBeDefined();
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
    expect(g.fleets.filter((f) => f.nat === 'USA').map((f) => f.name).sort()).toEqual(['Asiatic Fleet', 'Atlantic Fleet', 'Pacific Fleet', 'Pacific Submarine Fleet']);
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
    expect(list.rows.length).toBe(7);
    // Every row named is a dated row of Japan's that the far side lays, once.
    const dated = (setupData as any).expeditions as [number, string, string, string, number, string, any][];
    for (const [nat, name] of list.rows) expect(dated.filter((r) => r[1] === nat && r[2] === name).length, name).toBe(1);
    // The 5th Division is brought to Formosa in month 2 for the landing at Pakhoi, an operation of the book alone.
    const raise = (stop: number, human: string | null) => {
      const g = grand(human);
      setStop(g, stop, true);
      g.turn = 2;
      startPhase(g, 'JAP');
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
    expect(Object.keys(ARSENAL.yards).length).toBe(47 + 12);
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
    const g = grand();
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
    const pf = fleet(g, 'USA', 'Pacific Fleet'), af = fleet(g, 'USA', 'Atlantic Fleet');
    const before = { pb: pf.ships.bb, pc: pf.ships.cv, ab: af.ships.bb, ac: af.ships.cv };
    g.turn = 21;
    startPhase(g, 'USA');
    expect([pf.ships.bb, pf.ships.cv, af.ships.bb, af.ships.cv]).toEqual([before.pb - 3, before.pc - 1, before.ab + 3, before.ac + 1]);
    expect(g.headlines.some((h) => h.text === 'The battleships Idaho, Mississippi and New Mexico and the carrier Yorktown leave for the Atlantic.')).toBe(true);
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
