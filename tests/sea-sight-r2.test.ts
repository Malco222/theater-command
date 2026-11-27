// Where each side last saw the other at sea (release 2, Units decide; the design director's 24 November 2026 17:49Z
// amendment to ruling 1, and his ruling of 17:54Z on naval-battles/convoy-r2/sight-store-form.md, (a) on all five
// questions): each staff counts the other side's units where its own side last saw them, never where they truly are and
// never from the player's plans, and the same sight feeds the player's own panel. The record is written and read only
// while the sea patrol is live; as shipped it is dormant, and then nothing of it is written or read. The figures in these
// tests are the tests' own, not the game's, and each that needs it lays the switch live.
import { afterEach, describe, expect, it } from 'vitest';
import { allied, createGame } from '../src/campaign/game';
import { CAUSE, NAVY, atSea, convoyMeeting, convoyWar, navalUpkeep, nearerPort, onStation, onWatch, quieterBeat, zoneOf } from '../src/campaign/navy';
import { setMission } from '../src/campaign/air';
import { groupJobs } from '../src/campaign/airwar';
import { convoyTicket } from '../src/battle/convoy/ticket';
import { boatsInView, fleetFound } from '../src/campaign/carriers';
import { patrolFigures, patrolOn, patrolSeas, patrolStaff, watchAgainst, watchFor } from '../src/campaign/seapatrol';
import { airKnown, airMetWords, boatsKnown, noteSeaSight, routeSeenWords, seaMonth, seaSeen, seaSeenWords, seenAir, seenBoats } from '../src/campaign/seasight';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { grantTech } from '../src/campaign/tech';
import { runTurnAI } from '../src/campaign/turn';
import { setUnitsDecide } from '../src/campaign/units';
import { turnToDate } from '../src/core/ticket';
import type { AirFleet, Fleet, Game, Ships } from '../src/campaign/types';

const atlantic = NAVY.convoys.find((r) => r.id === 'atlantic')!;
const UNREAD = structuredClone(CAUSE.patrol);
const COUNT = structuredClone(CAUSE.count), INCIDENT = structuredClone(CAUSE.incident);
const FIG = { live: true, reach: 'map' as 'map' | 'air', a0: 50, intensity: 0.5 as number | null, attacks: { kit: 0.6, none: 0.3 }, kill: { kit: 0.1, none: 0.04 }, lost: null as number | null };
/** A strong patrol, so that a month's dice sink boats. */
const STRONG = { ...FIG, attacks: { kit: 6, none: 6 }, kill: { kit: 1, none: 1 } };
const lay = (fig: typeof FIG = FIG): void => { Object.assign(CAUSE.patrol, structuredClone(fig)); };
afterEach(() => {
  Object.assign(CAUSE.patrol, structuredClone(UNREAD));
  Object.assign(CAUSE.count, structuredClone(COUNT));
  Object.assign(CAUSE.incident, structuredClone(INCIDENT));
});

function open(seed = 1, human: string | null = null): Game {
  const g = startOpen(createGame(seed, human));
  g.flags.unitsDecide = 1;
  delete g.flags.weather;
  for (const f of g.fleets) f.stance = 'port';
  return g;
}
const none = (): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 });
function sea(g: Game, name: string, zone: string, ships: Partial<Ships>, stance: Fleet['stance'], lane?: string, nat?: string): Fleet {
  const f = g.fleets.find((x) => x.name === name && (!nat || x.nat === nat))!;
  f.ships = { ...none(), ...ships };
  f.zone = zoneOf(g, zone);
  f.stance = stance;
  if (lane) f.lane = lane; else delete f.lane;
  f.ready = 1;
  return f;
}
const group = (g: Game, name: string): AirFleet => g.airFleets.find((f) => f.name === name)!;
/** A raider set on its beat as its staff keeps it. */
function raider(g: Game, zone: string, ss: number): Fleet {
  const f = sea(g, 'U-boat Fleet', zone, { ss }, 'raid');
  f.duty = 'raid'; f.station = f.zone; f.tied = false;
  return f;
}
/** The enemy boats on station in a sea now: the truth, which no staff reads. */
const truly = (g: Game, nat: string, z: number): number => g.fleets.filter((f) => f.zone === z && atSea(f) && f.ships.ss > 0 && g.nations[f.nat].atWar.includes(nat)).reduce((s, f) => s + f.ships.ss * onStation(g, f, f.zone), 0);
/** A planted record of boats seen. */
function plant(g: Game, nat: string, z: number, t: number, n: number): void {
  (((g.intel.seaSight ??= {})[nat] ??= {})[z] ??= {}).boats = { t, n, how: n > 0 ? ['attack'] : [] };
}
/** A planted record of the aircraft a side's boats met: the part of a boat's time kept down. */
function plantAir(g: Game, nat: string, z: number, t: number, none: number): void {
  (((g.intel.seaSight ??= {})[nat] ??= {})[z] ??= {}).air = { t, kit: 0, none, lost: 0, how: none > 0 ? ['keptDown'] : [] };
}
/** About so many boats, as the panel says it. */
const about = (n: number): string => (n < 10 ? n.toFixed(1) : String(Math.round(n)));
const sent = (g: Game, nat: string): number[] => g.airFleets.filter((f) => f.nat === nat && f.mission === 'seapatrol').map((f) => f.target);

describe('dormant as shipped, and off: nothing is written or read', () => {
  it('dormant, a month at sea writes no record, and a record laid by hand is not read by the staffs or the panel', () => {
    expect(patrolFigures()).toBeNull();
    const g = open(), wapp = zoneOf(g, 'wapp');
    expect(patrolOn(g)).toBe(false);
    raider(g, 'wapp', 40);
    sea(g, 'Home Fleet', 'wapp', { dd: 20 }, 'escort', 'atlantic');
    navalUpkeep(g, 'GER');
    navalUpkeep(g, 'UK');
    noteSeaSight(g, 'UK', { attack: { [wapp]: 9 }, incident: {}, sunk: {} });
    expect(g.intel.seaSight).toBeUndefined();
    plant(g, 'UK', wapp, g.turn, 30);
    expect(seaSeen(g, 'UK', wapp)).toBeNull();
    expect(seenBoats(g, 'UK', wapp)).toBeNull();
    expect(boatsKnown(g, 'UK', wapp)).toBeNull();
    expect(routeSeenWords(g, 'UK', atlantic.zones.map((k) => zoneOf(g, k)))).toBe('');
    expect(airMetWords(g, 'GER', wapp)).toBe('');
    expect(convoyTicket(g, convoyMeeting(g, 'UK', atlantic), 'UK').place.card.sight).toBeUndefined();
  });

  it('off (without Units decide), live figures or not, nothing is written', () => {
    lay();
    const g = open(), wapp = zoneOf(g, 'wapp');
    g.flags.unitsDecide = 0;
    expect(patrolOn(g)).toBe(false);
    raider(g, 'wapp', 40);
    navalUpkeep(g, 'UK');
    noteSeaSight(g, 'GER');
    expect(g.intel.seaSight).toBeUndefined();
    expect(seenBoats(g, 'UK', wapp)).toBeNull();
  });

  it('the whole game: a year of two computer wars as shipped writes no record, and one planted at the start changes nothing else', () => {
    const months = 12;
    const play = (planted: boolean): string => {
      const g = startOpen(createGame(1, null));
      expect(setUnitsDecide(g, true)).toBe(true);
      if (planted) for (const z of g.seas) plant(g, 'UK', z.id, g.turn, 50);
      for (let t = 0; t < months; t++) runTurnAI(g);
      if (planted) delete g.intel.seaSight;
      else expect(g.intel.seaSight).toBeUndefined();
      return saveGame(g);
    };
    const bare = play(false), planted = play(true);
    expect(planted.length).toBe(bare.length);
    expect(planted === bare).toBe(true);
  });
});

describe('the boats a side sees (Q2 (a)): units that act reveal themselves where they acted', () => {
  it('a pack that attacks its convoys is seen where it attacked, that month, and never more than its boats on station', () => {
    lay();
    // (The tests' own figure, many boats meeting each convoy attacked, so that the attacks and the escort's incidents
    // alone would count more boats than are on station: the count stops at the boats on station as the month opened.)
    Object.assign(CAUSE.count, { perAttack: 20 });
    const g = open(), natl = zoneOf(g, 'natl');
    raider(g, 'natl', 40);
    sea(g, 'Home Fleet', 'wapp', { dd: 20 }, 'escort', 'atlantic');
    const before = truly(g, 'UK', natl);
    navalUpkeep(g, 'UK');
    const b = seenBoats(g, 'UK', natl)!;
    expect(b.t).toBe(g.turn);
    expect(b.how).toContain('attack');
    expect(b.how).toContain('incident');
    expect(b.n).toBeCloseTo(before, 2);
    expect(b.n).toBeLessThanOrEqual(before + 0.005);
    // (Its staff then counts them there.)
    patrolStaff(g, 'UK', g.airFleets.filter((f) => f.nat === 'UK'));
    expect(sent(g, 'UK')).toContain(natl);
  });

  it('boats that lurk where the side has no eyes are not seen: no convoy, escort, hunter, patrol or search of its own there', () => {
    lay();
    const g = open(), baltic = zoneOf(g, 'baltic'), wapp = zoneOf(g, 'wapp');
    sea(g, 'U-boat Fleet', 'baltic', { ss: 30 }, 'raid');
    navalUpkeep(g, 'UK');
    expect(truly(g, 'UK', baltic)).toBeGreaterThan(0);
    expect(seenBoats(g, 'UK', baltic)).toBeNull();
    // (Where it had eyes and saw none, it says so, with the month.)
    expect(seenBoats(g, 'UK', wapp)).toEqual({ t: g.turn, n: 0, how: [] });
  });

  it('its search counts the boats of fleets it identifies, as fleetFound finds a fleet', () => {
    lay();
    const g = open(), wapp = zoneOf(g, 'wapp');
    const f = raider(g, 'wapp', 30);
    // (With no ship of its own at sea, its search does not find boats there; an escort strong in cruisers identifies them.)
    expect(fleetFound(g, f, 'UK')).toBe('none');
    sea(g, 'Home Fleet', 'wapp', { ca: 30, dd: 20 }, 'escort', 'atlantic');
    expect(fleetFound(g, f, 'UK')).toBe('exact');
    navalUpkeep(g, 'UK');
    const b = seenBoats(g, 'UK', wapp)!;
    expect(b.how).toContain('search');
    expect(b.n).toBeCloseTo(truly(g, 'UK', wapp), 2);
  });

  it('a fleet its search only reports is not counted, as the map counts no strength for it, and its sea is not read as empty', () => {
    // (carriers.ts fleetsSeen and seaPicture: a fleet only reported has no count. The Bay of Biscay carries no British
    // convoy route; a British force of cruisers searching it reports boats there, and identifies them only when stronger.)
    lay();
    const g = open(), biscay = zoneOf(g, 'biscay');
    expect(atlantic.zones.map((k) => zoneOf(g, k))).not.toContain(biscay);
    const f = sea(g, 'U-boat Fleet', 'biscay', { ss: 20 }, 'raid');
    f.duty = 'raid'; f.station = biscay; f.tied = false;
    const cruisers = sea(g, 'Dover Fleet', 'biscay', { ca: 10 }, 'patrol');
    plant(g, 'UK', biscay, g.turn - 3, 12);
    expect(fleetFound(g, f, 'UK')).toBe('report');
    expect(boatsInView(g, 'UK', biscay)).toBe(true);
    noteSeaSight(g, 'UK');
    expect(seenBoats(g, 'UK', biscay)).toEqual({ t: g.turn - 3, n: 12, how: ['attack'] });
    // (Identified, they are counted, as they stand on station.)
    cruisers.ships.ca = 40;
    expect(fleetFound(g, f, 'UK')).toBe('exact');
    noteSeaSight(g, 'UK');
    expect(seenBoats(g, 'UK', biscay)).toEqual({ t: g.turn, n: Math.round(truly(g, 'UK', biscay) * 100) / 100, how: ['search'] });
    // (And with none there, the sea in view reads empty, with this month.)
    g.turn++;
    f.stance = 'port'; f.zone = f.home;
    noteSeaSight(g, 'UK');
    expect(seenBoats(g, 'UK', biscay)).toEqual({ t: g.turn, n: 0, how: [] });
  });

  it('what its escorts sink it has seen: their kills count, the most of any source where the dice sink more than the count met', () => {
    lay();
    // (The tests' own figure: each incident with the escort sinks the boat.)
    Object.assign(CAUSE.incident, { independent: 1 });
    const g = open(1), natl = zoneOf(g, 'natl');
    raider(g, 'natl', 6);
    sea(g, 'Western Approaches Fleet', 'natl', { dd: 36 }, 'escort', 'atlantic');
    const seen = seaMonth();
    convoyWar(g, 'UK', {}, undefined, seen);
    expect(seen.sunk[natl]).toBe(g.convoys.atlantic.byEscorts);
    expect(seen.sunk[natl]).toBeGreaterThan(Math.max(seen.attack[natl], seen.incident[natl]));
    noteSeaSight(g, 'UK', seen);
    expect(seenBoats(g, 'UK', natl)).toEqual({ t: g.turn, n: seen.sunk[natl], how: ['attack', 'incident', 'loss'] });
  });

  it('its patrols sight boats in the share of the sea their own aircraft watch, and what it sinks it has seen', () => {
    lay(STRONG);
    const g = open(), natl = zoneOf(g, 'natl');
    grantTech(g, 'UK', 'asvradar');
    raider(g, 'natl', 40);
    for (const n of ['No. 1 Group', 'No. 2 Group']) expect(setMission(g, group(g, n), 'seapatrol', natl)).toBe(true);
    // (Its patrols' sighting alone, before any attack or loss: the boats on station x the share of the sea its own
    // aircraft watch.)
    noteSeaSight(g, 'UK');
    const s = watchFor(g, natl, (x) => x === 'UK')!.s;
    expect(s).toBeGreaterThan(0);
    expect(s).toBeLessThan(1);
    expect(seenBoats(g, 'UK', natl)!.how).toEqual(['sighting']);
    expect(seenBoats(g, 'UK', natl)!.n).toBeCloseTo(truly(g, 'UK', natl) * s, 2);
    navalUpkeep(g, 'UK');
    const b = seenBoats(g, 'UK', natl)!;
    expect(b.t).toBe(g.turn);
    expect(b.how).toContain('sighting');
    expect(b.how).toContain('loss');
  });
});

describe('when a record changes (Q3 (a)): replaced each month the side had eyes there, even when it saw nothing', () => {
  it('eyes and nothing seen: the old record gives way to none, with this month', () => {
    lay();
    const g = open(), wapp = zoneOf(g, 'wapp');
    plant(g, 'UK', wapp, g.turn - 3, 12);
    const mine = g.airFleets.filter((f) => f.nat === 'UK');
    patrolStaff(g, 'UK', mine);
    expect(sent(g, 'UK')).toContain(wapp);
    for (const f of mine) { f.mission = 'support'; f.target = -1; }
    navalUpkeep(g, 'UK');
    expect(seenBoats(g, 'UK', wapp)).toEqual({ t: g.turn, n: 0, how: [] });
    patrolStaff(g, 'UK', mine);
    expect(sent(g, 'UK')).not.toContain(wapp);
  });

  it('its escorts, hunting groups, patrols and search are eyes too, off its convoy routes: each replaces the old record with none', () => {
    // (The Bay of Biscay carries the French convoys and no British route, and no British ship, aircraft or search has it in
    // view as the game opens: each of these, alone, gives Britain eyes there. At peace none of them does.)
    lay();
    const eyes: { what: string; put: (g: Game) => void; search: boolean }[] = [
      { what: 'an escort on the French convoys\' lane', put: (g) => { sea(g, 'Dover Fleet', 'iberia', { dd: 10 }, 'escort', 'french'); }, search: false },
      { what: 'a hunting group', put: (g) => { sea(g, 'Dover Fleet', 'biscay', { dd: 10 }, 'hunt'); }, search: false },
      { what: 'a patrol', put: (g) => { expect(setMission(g, group(g, 'No. 1 Group'), 'seapatrol', zoneOf(g, 'biscay'))).toBe(true); }, search: false },
      { what: 'a search that would report boats there', put: (g) => { sea(g, 'Dover Fleet', 'biscay', { ca: 10 }, 'patrol'); }, search: true },
    ];
    for (const { what, put, search } of eyes) {
      const g = open(), biscay = zoneOf(g, 'biscay');
      expect(atlantic.zones.map((k) => zoneOf(g, k))).not.toContain(biscay);
      plant(g, 'UK', biscay, g.turn - 3, 12);
      expect(boatsInView(g, 'UK', biscay)).toBe(false);
      noteSeaSight(g, 'UK');
      expect(seenBoats(g, 'UK', biscay), what).toEqual({ t: g.turn - 3, n: 12, how: ['attack'] });
      put(g);
      expect(boatsInView(g, 'UK', biscay), what).toBe(search);
      // (At peace it is no eyes on the enemy's boats.)
      const war = g.nations.UK.atWar;
      g.nations.UK.atWar = [];
      noteSeaSight(g, 'UK');
      g.nations.UK.atWar = war;
      expect(seenBoats(g, 'UK', biscay), what).toEqual({ t: g.turn - 3, n: 12, how: ['attack'] });
      noteSeaSight(g, 'UK');
      expect(seenBoats(g, 'UK', biscay), what).toEqual({ t: g.turn, n: 0, how: [] });
    }
  });

  it('no eyes: the old record stands with its month, however old, and nothing decays it', () => {
    lay();
    const g = open(), baltic = zoneOf(g, 'baltic');
    plant(g, 'UK', baltic, g.turn - 6, 7);
    for (let k = 0; k < 3; k++) { navalUpkeep(g, 'UK'); g.turn++; }
    expect(seenBoats(g, 'UK', baltic)).toEqual({ t: g.turn - 9, n: 7, how: ['attack'] });
  });
});

describe('allies read each other\'s records, one rule for every alliance', () => {
  it('a side reads the newest record among its own nation and its allies; an enemy reads none of it; and its staff acts on an ally\'s', () => {
    lay();
    const g = open(), wapp = zoneOf(g, 'wapp');
    expect(allied(g, 'UK', 'FRA')).toBe(true);
    plant(g, 'FRA', wapp, g.turn, 20);
    expect(seenBoats(g, 'UK', wapp)!.n).toBe(20);
    expect(seenBoats(g, 'GER', wapp)).toBeNull();
    const mine = g.airFleets.filter((f) => f.nat === 'UK');
    patrolStaff(g, 'UK', mine);
    expect(sent(g, 'UK')).toContain(wapp);
    // (The newest wins, whoever wrote it.)
    plant(g, 'UK', wapp, g.turn - 2, 40);
    expect(seenBoats(g, 'UK', wapp)!.n).toBe(20);
    plant(g, 'UK', wapp, g.turn + 1, 3);
    expect(seenBoats(g, 'UK', wapp)!.n).toBe(3);
    expect(seenBoats(g, 'FRA', wapp)!.n).toBe(3);
  });
});

describe('the aircraft the boats\' side meets (Q2 (a)): only where its own boats were on station', () => {
  it('its boats meet a patrol once it has flown its month over them, never at its order; none elsewhere: a sea never met counts as clear', () => {
    lay();
    const g = open(), wapp = zoneOf(g, 'wapp'), natl = zoneOf(g, 'natl');
    raider(g, 'wapp', 20);
    expect(setMission(g, group(g, 'No. 1 Group'), 'seapatrol', wapp)).toBe(true);
    expect(setMission(g, group(g, 'No. 2 Group'), 'seapatrol', natl)).toBe(true);
    // (Ordered and not yet flown: the order stands over the boats, but they have met nothing.)
    expect(watchAgainst(g, wapp, 'GER')!.none).toBeGreaterThan(0);
    noteSeaSight(g, 'GER');
    expect(seenAir(g, 'GER', wapp)).toEqual({ t: g.turn, kit: 0, none: 0, lost: 0, how: [] });
    // (Britain's month at sea flies its patrols over the boats; Germany's record then holds the watch they kept.)
    navalUpkeep(g, 'UK');
    expect(g.patrolSunk!.UK.met![wapp].GER.none).toBeGreaterThan(0);
    noteSeaSight(g, 'GER');
    const a = seenAir(g, 'GER', wapp)!;
    const w = watchAgainst(g, wapp, 'GER')!;
    expect(a).toMatchObject({ t: g.turn, kit: 0, lost: 0, how: ['keptDown'] });
    expect(a.none).toBeCloseTo(w.none, 3);
    expect(watchAgainst(g, natl, 'GER')!.none).toBeGreaterThan(0);
    expect(seenAir(g, 'GER', natl)).toBeNull();
    // (The convoys' side has met no aircraft of the boats' side, and reads none.)
    expect(seenAir(g, 'UK', wapp)).toBeNull();
  });

  it('with kit the aircraft attack, and what they attacked and sank is counted at the boats\' next month, once', () => {
    lay(STRONG);
    const g = open(), wapp = zoneOf(g, 'wapp');
    grantTech(g, 'UK', 'asvradar');
    raider(g, 'wapp', 40);
    for (const n of ['No. 1 Group', 'No. 2 Group']) setMission(g, group(g, n), 'seapatrol', wapp);
    // (Germany's month comes before Britain's in the order of play: its boats have not yet met the patrol.)
    expect(g.order.indexOf('GER')).toBeLessThan(g.order.indexOf('UK'));
    noteSeaSight(g, 'GER');
    expect(seenAir(g, 'GER', wapp)).toMatchObject({ kit: 0, none: 0, lost: 0, how: [] });
    // (Britain's month after Germany's: its patrols attack and sink boats in the Western Approaches.)
    navalUpkeep(g, 'UK');
    const k = g.patrolSunk!.UK.at![wapp].GER, met = g.patrolSunk!.UK.met![wapp].GER;
    expect(k).toBeGreaterThan(0);
    expect(met.kit).toBeGreaterThan(0);
    expect(met.none).toBe(0);
    g.turn++;
    noteSeaSight(g, 'GER');
    expect(seenAir(g, 'GER', wapp)).toMatchObject({ t: g.turn, lost: k, how: ['attack', 'loss'] });
    expect(seenAir(g, 'GER', wapp)!.kit).toBeCloseTo(met.kit, 3);
    // (Its next month, with no month of Britain's patrols since: nothing more met, and no loss counted twice.)
    g.turn++;
    noteSeaSight(g, 'GER');
    expect(seenAir(g, 'GER', wapp)).toMatchObject({ t: g.turn, kit: 0, none: 0, lost: 0 });
  });

  it('where the patrol met its boats and they have since left the sea, the record still holds what they met there', () => {
    lay(STRONG);
    const g = open(), wapp = zoneOf(g, 'wapp'), natl = zoneOf(g, 'natl');
    grantTech(g, 'UK', 'asvradar');
    const f = raider(g, 'wapp', 40);
    for (const n of ['No. 1 Group', 'No. 2 Group']) setMission(g, group(g, n), 'seapatrol', wapp);
    navalUpkeep(g, 'UK');
    const k = g.patrolSunk!.UK.at![wapp].GER;
    expect(k).toBeGreaterThan(0);
    // (The survivors are on another beat by the close of Germany's month.)
    f.zone = natl; f.station = natl;
    g.turn++;
    noteSeaSight(g, 'GER');
    expect(seenAir(g, 'GER', wapp)).toMatchObject({ t: g.turn, lost: k, how: ['attack', 'loss'] });
    expect(seenAir(g, 'GER', wapp)!.kit).toBeGreaterThan(0);
  });

  it('a patrol is met once, at the boats\' first close after it flew: one rule both ways, Germany\'s patrols over British boats too', () => {
    lay();
    const g = open(), nsea = zoneOf(g, 'nsea');
    // (Germany's month comes before Britain's: a German patrol flown this month is met at Britain's close this month,
    // and one flown last month was met at Britain's close last month.)
    expect(g.order.indexOf('GER')).toBeLessThan(g.order.indexOf('UK'));
    const boats = sea(g, 'Mediterranean Fleet', 'nsea', { ss: 8 }, 'raid', undefined, 'UK');
    boats.station = nsea;
    expect(setMission(g, group(g, '1. Fliegerdivision'), 'seapatrol', nsea)).toBe(true);
    navalUpkeep(g, 'GER');
    expect(g.patrolSunk!.GER.met![nsea].UK.none).toBeGreaterThan(0);
    noteSeaSight(g, 'UK');
    expect(seenAir(g, 'UK', nsea)!.none).toBeGreaterThan(0);
    // (The next month, where Germany's patrols have flown no month since by Britain's close: nothing new is met, and the
    // patrol already met is not counted twice.)
    g.turn++;
    noteSeaSight(g, 'UK');
    expect(seenAir(g, 'UK', nsea)).toEqual({ t: g.turn, kit: 0, none: 0, lost: 0, how: [] });
  });

  it('in the order of play: a patrol the player orders after the boats\' month is met only once his next month has flown it', () => {
    lay({ ...FIG, a0: 8 });
    const g = open(1, 'UK'), wapp = zoneOf(g, 'wapp');
    expect(g.order.indexOf('GER')).toBeLessThan(g.order.indexOf('UK'));
    const f = raider(g, 'wapp', 20);
    navalUpkeep(g, 'GER');
    navalUpkeep(g, 'UK');
    // (His orders, given after his month at sea.)
    for (const n of ['No. 1 Group', 'No. 2 Group']) expect(setMission(g, group(g, n), 'seapatrol', wapp)).toBe(true);
    // (The next month Germany plays first. The patrol has not flown yet: its boats have not met it, and their staff keeps
    // the beat, though the order stands over it.)
    g.turn++;
    navalUpkeep(g, 'GER');
    expect(watchAgainst(g, wapp, 'GER')!.none).toBeGreaterThan(0);
    expect(seenAir(g, 'GER', wapp)).toMatchObject({ t: g.turn, kit: 0, none: 0 });
    expect(quieterBeat(g, f)).toBe(-1);
    // (His month flies it over the boats. At Germany's next month they have met it, and their staff moves them.)
    navalUpkeep(g, 'UK');
    g.turn++;
    navalUpkeep(g, 'GER');
    expect(seenAir(g, 'GER', wapp)!.none).toBeGreaterThan(0);
    expect(seenAir(g, 'GER', wapp)!.how).toContain('keptDown');
    expect(quieterBeat(g, f)).toBeGreaterThanOrEqual(0);
  });

  it('its boats on station and no patrol met: the old record gives way to none, with this month, and the boats keep their beat', () => {
    lay({ ...FIG, a0: 8 });
    const g = open(), wapp = zoneOf(g, 'wapp');
    const f = raider(g, 'wapp', 20);
    const groups = ['No. 1 Group', 'No. 2 Group'].map((n) => group(g, n));
    for (const x of groups) expect(setMission(g, x, 'seapatrol', wapp)).toBe(true);
    navalUpkeep(g, 'UK');
    noteSeaSight(g, 'GER');
    expect(seenAir(g, 'GER', wapp)!.none).toBeGreaterThan(0);
    expect(quieterBeat(g, f)).toBeGreaterThanOrEqual(0);
    // (Britain withdraws both groups, and its next month flies none over the boats, which stay on station.)
    for (const x of groups) { x.mission = 'support'; x.target = -1; }
    g.turn++;
    navalUpkeep(g, 'UK');
    noteSeaSight(g, 'GER');
    expect(seenAir(g, 'GER', wapp)).toEqual({ t: g.turn, kit: 0, none: 0, lost: 0, how: [] });
    expect(quieterBeat(g, f)).toBe(-1);
  });
});

describe('the staffs read only the sight (the ruling of 17:54Z, Q5 (a))', () => {
  it('the patrol staff sends its groups where the boats were last seen, though they have since moved; the next month\'s sighting follows them', () => {
    lay();
    const g = open(), wapp = zoneOf(g, 'wapp'), natl = zoneOf(g, 'natl');
    const f = raider(g, 'wapp', 60);
    sea(g, 'Home Fleet', 'irish', { dd: 20 }, 'escort', 'atlantic');
    navalUpkeep(g, 'UK');
    expect(seenBoats(g, 'UK', wapp)!.n).toBeGreaterThan(0);
    // (The boats slip away to the North Atlantic, unseen.)
    f.zone = natl; f.station = natl;
    const mine = g.airFleets.filter((x) => x.nat === 'UK');
    patrolStaff(g, 'UK', mine);
    expect(sent(g, 'UK').length).toBeGreaterThan(0);
    expect(sent(g, 'UK').every((z) => z === wapp)).toBe(true);
    // (Next month its convoys meet them in the North Atlantic, and its patrols over the Western Approaches see none.)
    g.turn++;
    navalUpkeep(g, 'UK');
    expect(seenBoats(g, 'UK', wapp)).toMatchObject({ t: g.turn, n: 0 });
    expect(seenBoats(g, 'UK', natl)!.n).toBeGreaterThan(0);
    for (const x of mine) { x.mission = 'support'; x.target = -1; }
    patrolStaff(g, 'UK', mine);
    expect(sent(g, 'UK')).toContain(natl);
    expect(sent(g, 'UK')).not.toContain(wapp);
  });

  it('the boats\' staff weighs the aircraft its boats met, never the patrols truly flown: cover over a sea its boats never met does not change its move', () => {
    lay({ ...FIG, a0: 8 });
    const g = open(), wapp = zoneOf(g, 'wapp'), natl = zoneOf(g, 'natl'), irish = zoneOf(g, 'irish');
    const f = raider(g, 'wapp', 20);
    for (const n of ['No. 1 Group', 'No. 2 Group']) setMission(g, group(g, n), 'seapatrol', wapp);
    // (Until its boats have met them, the staff does not know they are there: ordered is not met.)
    expect(quieterBeat(g, f)).toBe(-1);
    noteSeaSight(g, 'GER');
    expect(quieterBeat(g, f)).toBe(-1);
    // (Britain's month flies them over the boats, and Germany's next record holds what its boats met.)
    navalUpkeep(g, 'UK');
    noteSeaSight(g, 'GER');
    expect(quieterBeat(g, f)).toBe(natl);
    // (One group now patrols the North Atlantic instead, unmet: there the boats would truly be up less than in the Irish
    // Sea, which their staff also counts clear; it still moves them to the North Atlantic.)
    expect(setMission(g, group(g, 'No. 2 Group'), 'seapatrol', natl)).toBe(true);
    expect(onWatch(g, f, natl)).toBeLessThan(onWatch(g, f, irish));
    expect(seenAir(g, 'GER', natl)).toBeNull();
    expect(quieterBeat(g, f)).toBe(natl);
    // (Once that patrol has flown its month over its boats there, it knows.)
    f.zone = natl; f.station = natl;
    noteSeaSight(g, 'GER');
    expect(seenAir(g, 'GER', natl)).toMatchObject({ kit: 0, none: 0 });
    navalUpkeep(g, 'UK');
    noteSeaSight(g, 'GER');
    expect(seenAir(g, 'GER', natl)!.none).toBeCloseTo(watchAgainst(g, natl, 'GER')!.none, 3);
  });

  it('the boats\' staff moves on the watch its boats met, though the patrol has since left unseen', () => {
    lay({ ...FIG, a0: 8 });
    const g = open(), wapp = zoneOf(g, 'wapp'), natl = zoneOf(g, 'natl');
    const f = raider(g, 'wapp', 20);
    const groups = ['No. 1 Group', 'No. 2 Group'].map((n) => group(g, n));
    for (const x of groups) expect(setMission(g, x, 'seapatrol', wapp)).toBe(true);
    navalUpkeep(g, 'UK');
    noteSeaSight(g, 'GER');
    expect(quieterBeat(g, f)).toBe(natl);
    // (Britain withdraws both groups, which Germany has not seen: the Western Approaches are truly clear now, and its
    // staff still moves the boats out of the watch they last met there.)
    for (const x of groups) { x.mission = 'support'; x.target = -1; }
    expect(watchAgainst(g, wapp, 'GER')).toBeNull();
    expect(seenAir(g, 'GER', wapp)!.none).toBeGreaterThan(0);
    expect(quieterBeat(g, f)).toBe(natl);
  });

  it('the boats\' staff weighs the crossing on the aircraft its boats met, never on the patrols truly flown', () => {
    // (The U-boat Fleet of 40, based at Bordeaux, on its beat in the Western Approaches; Germany holds Bordeaux and
    // Bergen. Its boats last met aircraft keeping them down 27% of the time there; no patrol truly flies there now. The
    // Irish Sea, never met, counts clear, and the boats would raid it from Bordeaux too, a sea of passage from their beat.)
    lay({ ...FIG, a0: 8 });
    const g = open(), wapp = zoneOf(g, 'wapp'), irish = zoneOf(g, 'irish');
    for (const r of g.regions) if (['Bordeaux', 'Bergen'].includes(r.name)) { r.ctrl = 'GER'; r.since = g.turn - 1; }
    for (const a of g.airFleets) if (a.nat === 'GER') a.mission = 'rest';
    const f = raider(g, 'wapp', 40);
    const home = g.byName.Bordeaux;
    f.base = home.id; f.port = home.name; f.home = home.seas[0];
    const portFor = (z: number): string | undefined => { const keep = f.station; f.station = z; try { return nearerPort(g, f)?.name; } finally { f.station = keep; } };
    expect(portFor(wapp)).toBeUndefined();
    expect(portFor(irish)).toBeUndefined();
    plantAir(g, 'GER', wapp, g.turn, 0.27);
    expect(watchAgainst(g, wapp, 'GER')).toBeNull();
    // (The Irish Sea's gain over the beat as last met covers the crossing from it as last met, but not the crossing as
    // the world stands, with the boats truly up all their time on station.)
    const D = CAUSE.station.ss!, cross = D.passage! / 30, on = onStation(g, f, wapp);
    const gain = onStation(g, f, irish) - on * (1 - 0.27);
    expect(gain).toBeGreaterThan(on * (1 - 0.27) * cross);
    expect(gain).toBeLessThan(on * cross);
    expect(quieterBeat(g, f)).toBe(irish);
  });

  it('the patrol staff takes its own patrols\' keeping down off the boats it saw: once one group brings a sea below the next, the next group goes there', () => {
    lay({ ...FIG, a0: 8 });
    const g = open(), wapp = zoneOf(g, 'wapp'), natl = zoneOf(g, 'natl');
    g.convoys.atlantic = { ...(g.convoys.atlantic ?? {}), loss: 0.05 } as any;
    plant(g, 'UK', wapp, g.turn, 20);
    plant(g, 'UK', natl, g.turn, 18);
    const mine = g.airFleets.filter((x) => x.nat === 'UK');
    patrolStaff(g, 'UK', mine);
    const one = group(g, 'No. 1 Group'), two = group(g, 'No. 2 Group');
    expect(one.mission === 'seapatrol' && one.target === wapp).toBe(true);
    // (One group over the Western Approaches keeps a share of its 20 boats down, below the 18 seen in the North Atlantic.)
    const w = watchFor(g, wapp, (x) => allied(g, x, 'UK'))!;
    expect(w.kit).toBe(0);
    expect(20 * (1 - w.none)).toBeLessThan(18);
    expect(two.mission === 'seapatrol' && two.target === natl).toBe(true);
  });
});

describe('the player\'s panel (Q4 (a)): the same sight, with the month', () => {
  it('the convoy route, the night\'s card and the patrol\'s order say what his side last saw and when', () => {
    lay();
    const g = open(1, 'UK'), wapp = zoneOf(g, 'wapp'), natl = zoneOf(g, 'natl');
    // (October 1940.)
    g.turn = 14;
    const zs = atlantic.zones.map((k) => zoneOf(g, k));
    expect(routeSeenWords(g, 'UK', zs)).toBe('No boats seen on its seas');
    expect(seaSeenWords(g, 'UK', wapp)).toBe('No boats seen there');
    raider(g, 'wapp', 30);
    navalUpkeep(g, 'UK');
    const n = seenBoats(g, 'UK', wapp)!.n, month = turnToDate(g.turn).label;
    expect(month).toBe('October 1940');
    expect(routeSeenWords(g, 'UK', zs)).toBe(`Boats last seen on its seas: about ${about(n)} (October 1940)`);
    expect(seaSeenWords(g, 'UK', wapp)).toBe(`Boats last seen there: about ${about(n)} (October 1940)`);
    // (Seen in two months, the words name both.)
    plant(g, 'UK', natl, 7, 2);
    expect(routeSeenWords(g, 'UK', zs)).toBe(`Boats last seen on its seas: about ${about(n + 2)} (March to October 1940)`);
    plant(g, 'UK', natl, 1, 2);
    expect(routeSeenWords(g, 'UK', zs)).toBe(`Boats last seen on its seas: about ${about(n + 2)} (September 1939 to October 1940)`);
    plant(g, 'UK', natl, 0, 0);
    // (The night's card on the convoys' side, and the patrol's order.)
    expect(convoyTicket(g, convoyMeeting(g, 'UK', atlantic), 'UK').place.card.sight).toEqual([routeSeenWords(g, 'UK', zs)]);
    const grp = group(g, 'No. 1 Group');
    const job = groupJobs(g, grp.id, 'UK').find((j) => j.key === `sea:${wapp}`)!;
    expect(job.forecast).toContain(`Boats last seen there: about ${about(n)} (October 1940).`);
  });

  it('where he holds boats: the aircraft his boats last met over each sea, or none met', () => {
    lay();
    const g = open(1, 'GER'), wapp = zoneOf(g, 'wapp'), natl = zoneOf(g, 'natl');
    raider(g, 'wapp', 20);
    setMission(g, group(g, 'No. 1 Group'), 'seapatrol', wapp);
    expect(airMetWords(g, 'GER', wapp)).toBe('No aircraft met over the Western Approaches');
    // (His boats on station, the patrol ordered and not yet flown: none met, with the month.)
    noteSeaSight(g, 'GER');
    expect(airMetWords(g, 'GER', wapp)).toBe(`Aircraft last met over the Western Approaches: none (${turnToDate(g.turn).label})`);
    // (Once it has flown its month over them.)
    navalUpkeep(g, 'UK');
    noteSeaSight(g, 'GER');
    const a = seenAir(g, 'GER', wapp)!, month = turnToDate(g.turn).label;
    expect(airMetWords(g, 'GER', wapp)).toBe(`Aircraft last met over the Western Approaches: keeping the boats down about ${Math.round(a.none * 100)}% of the time (${month})`);
    expect(airMetWords(g, 'GER', natl)).toMatch(/^No aircraft met over /);
    expect(convoyTicket(g, convoyMeeting(g, 'UK', atlantic), 'GER').place.card.sight).toEqual([airMetWords(g, 'GER', g.seas.find((z) => z.key === convoyTicket(g, convoyMeeting(g, 'UK', atlantic), 'GER').place.sea)!.id)]);
  });

  it('with his fog of war lifted, his panel shows the truth, and only his: the staffs still read the sight', () => {
    lay();
    const g = open(1, 'UK'), natl = zoneOf(g, 'natl'), wapp = zoneOf(g, 'wapp');
    const f = raider(g, 'natl', 40);
    g.flags.nofog = 1;
    expect(boatsKnown(g, 'UK', natl)).toEqual({ n: truly(g, 'UK', natl), t: g.turn });
    expect(seenBoats(g, 'UK', natl)).toBeNull();
    expect(boatsKnown(g, 'FRA', natl)).toBeNull();
    patrolStaff(g, 'UK', g.airFleets.filter((x) => x.nat === 'UK'));
    expect(sent(g, 'UK')).toEqual([]);
    // (The boats' side's player, his fog lifted: the watch over his boats now, though they never met it; their staff reads none.)
    g.human = 'GER';
    f.zone = wapp; f.station = wapp;
    setMission(g, group(g, 'No. 1 Group'), 'seapatrol', wapp);
    expect(airKnown(g, 'GER', wapp)!.none).toBeCloseTo(watchAgainst(g, wapp, 'GER')!.none, 9);
    expect(seenAir(g, 'GER', wapp)).toBeNull();
    expect(quieterBeat(g, f)).toBe(-1);
  });
});

describe('the patrol order, his fog of war lifted', () => {
  it('says what his panel says, the truth over every sea, though his boats were never there; with the fog on, nothing of aircraft', () => {
    lay();
    const g = open(1, 'GER'), nsea = zoneOf(g, 'nsea');
    const kg = group(g, 'Kampfgeschwader 1');
    expect(patrolSeas(g, kg)).toContain(nsea);
    // (A British group over the North Sea, laid by hand, where no German boat has been.)
    const uk = group(g, 'No. 1 Group');
    uk.mission = 'seapatrol'; uk.target = nsea;
    expect(watchAgainst(g, nsea, 'GER')!.none).toBeGreaterThan(0);
    const order = (): string => groupJobs(g, kg.id, 'GER').find((j) => j.key === `sea:${nsea}`)!.forecast;
    expect(order()).not.toContain('Aircraft last met');
    g.flags.nofog = 1;
    expect(seenAir(g, 'GER', nsea)).toBeNull();
    const words = airMetWords(g, 'GER', nsea);
    expect(words).toMatch(/^Aircraft last met over the North Sea: keeping the boats down about \d+% of the time \(/);
    expect(order()).toContain(` ${words}.`);
  });
});

describe('the save', () => {
  it('keeps each side\'s record and the month\'s kills from the air by sea', () => {
    lay(STRONG);
    const g = open(), wapp = zoneOf(g, 'wapp');
    grantTech(g, 'UK', 'asvradar');
    raider(g, 'wapp', 40);
    for (const n of ['No. 1 Group', 'No. 2 Group']) setMission(g, group(g, n), 'seapatrol', wapp);
    navalUpkeep(g, 'UK');
    noteSeaSight(g, 'GER');
    expect(g.intel.seaSight!.UK[wapp].boats).toBeDefined();
    expect(g.intel.seaSight!.GER[wapp].air!.kit).toBeGreaterThan(0);
    expect(g.patrolSunk!.UK.at).toBeDefined();
    expect(g.patrolSunk!.UK.met![wapp].GER.kit).toBeGreaterThan(0);
    const h = loadGame(saveGame(g))!;
    expect(h.intel.seaSight).toEqual(g.intel.seaSight);
    expect(h.patrolSunk).toEqual(g.patrolSunk);
    expect(seaSeen(h, 'UK', wapp)).toEqual(seaSeen(g, 'UK', wapp));
  });
});
