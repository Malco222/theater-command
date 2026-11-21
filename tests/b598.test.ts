// Ruling 233 rider b598: need sees fleets, a lost home frees a tie, and the Bordeaux flotilla is never parked on an
// empty sea. (1) Need counts lanes, and a lane and a fleet's weight have no common measure in the rules, so the gradual
// move by need stands and, while an enemy fleet is based on a Mediterranean sea, Italy's three Mediterranean beats keep
// at least a third of her boats between them (navy.json beats 'floor'). (2) Every tied fleet, in every campaign, loses
// its tie the month its home falls or once its beat is out of reach from the port it is in, and is then placed as any
// other fleet. (3) At stops 1 to 3 the Bordeaux flotilla's beat follows the historian's seas by half-year (riders b601
// and b603, which replace b598's interim): the sea of the list with the most British lane, or where none has any, the
// nearest sea to the list that does; its dated beat only where no sea in its reach carries one. (4) The names and the
// carriers are the rows, at every stop (rider b601): Bordeaux's and the German Mediterranean boats' commanders by date,
// and the Mediterranean Fleet's Glorious and Eagle. (5) At stops 1 and 2 each British fleet carrier's arrivals in and
// departures from the Mediterranean, 1939 to 1942, are rows, one each way (the director's notes of 08:34Z and 08:38Z;
// the historian's addendum to historian/bordeaux-med-subs-b598.md): a move between two fleets takes its ships from the
// one and gives them to the other in the same month, a departure the record owed to battle is no row, and the arrival
// that replaced her waits until play has taken a carrier from that fleet, until the record's window for her posting
// closes (the Formidable's in the East at 21, the Eagle's in the West at Pedestal: the director's notes of 10:02Z and
// 11:21Z). (6) At stops 1 and 2 Force H forms in June 1940 by its book, with the historian's ships month by month, and
// is its row's ships from September 1940 (the director's notes of 10:06Z and 11:21Z; historian/italian-boats-b562.md
// and its addendum); everywhere else it forms in July 1940 with its row's ships, as before.
import { describe, expect, it } from 'vitest';
import { createGame, declareWar } from '../src/campaign/game';
import {
  NAVY, fleetReach, fleetsAnswer, foeBasedOn, gapOf, huntBeat, lanesTo, navalAI, navalUpkeep, placeBoats, rebase, sail, seaRoutes, shippingIn, tieLapses, tiedNow, zoneOf,
} from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game } from '../src/campaign/types';

const ITALY = ['Atlantic Submarine Fleet', 'Submarine Fleet', 'Central Submarine Fleet', 'Eastern Submarine Fleet'];
const MED = ITALY.slice(1);

/** A grand campaign on its first day, at a stop of the history dial. */
function wide(stop = 3, human: string | null = null): Game {
  const g = startOpen(createGame(1, human));
  if (stop !== 3) g.flags.hist = stop;
  return g;
}
const named = (g: Game, name: string): Fleet | undefined => g.fleets.find((f) => f.name === name);
const boats = (g: Game, name: string): number => named(g, name)?.ships.ss ?? 0;
const italian = (g: Game): number[] => ITALY.map((name) => boats(g, name));
const sum = (xs: number[]): number => xs.reduce((a, b) => a + b, 0);
const news = (g: Game, turn: number): string[] => g.headlines.filter((h) => h.turn === turn).map((h) => h.text);
const key = (g: Game, z: number): string => g.seas[z].key;
/** Bordeaux is Germany's, as it was from June 1940. */
const frenchFall = (g: Game): void => { g.byName.Bordeaux.ctrl = 'GER'; };
/** Force H forms in July 1940 (turn 11), as its row has it (at stops 1 and 2 its book's window still holds then). */
function forceH(g: Game): Fleet {
  g.turn = 11;
  navalUpkeep(g, 'UK');
  return named(g, 'Force H')!;
}
/** Every fleet of a nation at war with Italy that has surface ships, struck from the list (none is left to be based on any sea). */
const noFoes = (g: Game): void => { g.fleets = g.fleets.filter((f) => !(g.nations[f.nat].atWar.includes('ITA') && f.ships.bb + f.ships.cv + f.ships.ca + f.ships.dd > 0)); };

describe('(1) need, and the Mediterranean\'s third', () => {
  /** Stop 4, at war with Britain, Bordeaux German, the groups formed, and most boats already drawn to the Atlantic. */
  function drawnWest(): Game {
    const g = wide(4);
    frenchFall(g);
    declareWar(g, 'ITA', 'UK');
    g.turn = 13;
    placeBoats(g, 'ITA');
    const set = [80, 10, 6, 4];
    ITALY.forEach((name, i) => { named(g, name)!.ships.ss = set[i]; });
    g.turn = 14;
    return g;
  }

  it('need counts lanes only: an enemy fleet on a sea adds nothing to it', () => {
    const g = drawnWest();
    const emed = zoneOf(g, 'emed');
    expect(named(g, 'Mediterranean Fleet')!.home).toBe(emed);
    expect(shippingIn(g, 'ITA', emed)).toBe(0);
    expect(shippingIn(g, 'ITA', zoneOf(g, 'wapp'))).toBeGreaterThan(0);
  });

  it('the floor is in the data: the three Mediterranean beats, a third, on the Mediterranean\'s seas', () => {
    const F = NAVY.beats!.ITA.floor!;
    expect(F.groups).toEqual(MED);
    expect(F.least).toEqual([1, 3]);
    const g = wide();
    for (const k of F.seas) expect(zoneOf(g, k), k).toBeGreaterThanOrEqual(0);
    expect([...F.seas].sort()).toEqual(['adriatic', 'aegean', 'central', 'emed', 'ionian', 'libyan', 'tyrr', 'wmed']);
  });

  it('an enemy fleet with surface ships whose port is on a Mediterranean sea is what the floor looks for', () => {
    const g = drawnWest();
    const seas = NAVY.beats!.ITA.floor!.seas;
    expect(foeBasedOn(g, 'ITA', seas)).toBe(true);
    // Force H's Gibraltar opens on the Western Mediterranean as well as the Iberian Coast: it counts alone.
    const h = forceH(g);
    g.turn = 14;
    g.fleets = g.fleets.filter((f) => f === h || !(f.nat === 'UK' || f.nat === 'FRA'));
    expect(foeBasedOn(g, 'ITA', seas)).toBe(true);
    // A fleet of submarines only is no quarry for the boats.
    h.ships = { bb: 0, cv: 0, ca: 0, dd: 0, ss: 4 };
    expect(foeBasedOn(g, 'ITA', seas)).toBe(false);
    // Nor is the fleet of a nation not at war with Italy: an Italy at peace has none to look for.
    const peace = wide(4);
    expect(named(peace, 'Mediterranean Fleet')).toBeDefined();
    expect(foeBasedOn(peace, 'ITA', seas)).toBe(false);
  });

  it('while an enemy fleet is based on a Mediterranean sea, the Mediterranean\'s beats keep a third of the boats', () => {
    const g = drawnWest();
    placeBoats(g, 'ITA');
    const now = italian(g);
    expect(sum(now)).toBe(100);
    expect(sum(now.slice(1))).toBe(34);
    expect(now[0]).toBe(66);
    // Shared among the three as the month's move would share them: the western beat still the largest of them.
    expect(now.slice(1).every((x) => x > 0)).toBe(true);
    expect(now[1]).toBeGreaterThan(now[3]);
    // The next month the floor still holds, and no boat is made or lost by it.
    g.turn = 15;
    placeBoats(g, 'ITA');
    expect(sum(italian(g))).toBe(100);
    expect(sum(italian(g).slice(1))).toBeGreaterThanOrEqual(34);
  });

  it('with no enemy fleet based on a Mediterranean sea the gradual move goes on as before', () => {
    const g = drawnWest();
    noFoes(g);
    expect(foeBasedOn(g, 'ITA', NAVY.beats!.ITA.floor!.seas)).toBe(false);
    placeBoats(g, 'ITA');
    expect(sum(italian(g))).toBe(100);
    // A sixtieth of the way towards all the boats in the Atlantic, from 80: never back to the floor.
    expect(boats(g, 'Atlantic Submarine Fleet')).toBe(80);
    expect(sum(italian(g).slice(1))).toBe(20);
  });

  it('the floor never binds where the move leaves the Mediterranean its third, and keeps b564\'s pace', () => {
    const g = wide(4);
    frenchFall(g);
    declareWar(g, 'ITA', 'UK');
    for (let turn = 13; turn <= 25; turn++) {
      g.turn = turn;
      placeBoats(g, 'ITA');
    }
    // As b564 has it: about a fifth in the Atlantic in a year.
    expect(boats(g, 'Atlantic Submarine Fleet')).toBe(17);
    expect(sum(italian(g))).toBe(100);
  });

  it('at stops 1 to 3 the rows are read by date, floor or none', () => {
    const g = wide(3);
    frenchFall(g);
    declareWar(g, 'ITA', 'UK');
    g.turn = 28;
    placeBoats(g, 'ITA');
    expect(italian(g)).toEqual([18, 19, 38, 25]);
    // The record's rows never leave the Mediterranean below a third, so a row that does is laid in for the test: read by
    // date, it stands as written with an enemy fleet based at Alexandria, where by need the floor would lift it to 34.
    const rows = NAVY.beats!.ITA.rows;
    rows.push([28, 80, 5, 5, 5]);
    try {
      for (const stop of [1, 2, 3]) {
        g.flags.hist = stop;
        placeBoats(g, 'ITA');
        expect(foeBasedOn(g, 'ITA', NAVY.beats!.ITA.floor!.seas)).toBe(true);
        expect(italian(g), `stop ${stop}`).toEqual([84, 6, 5, 5]);
      }
      g.flags.hist = 4;
      placeBoats(g, 'ITA');
      expect(sum(italian(g).slice(1))).toBeGreaterThanOrEqual(34);
    } finally {
      rows.pop();
    }
  });

  it('a group counts for the floor while its beat is on a Mediterranean sea, not by its name alone', () => {
    const g = drawnWest();
    // Italy holds the Rock, so the western group can pass the strait, and a British lane is laid off the Iberian coast:
    // by need the western group's beat is that sea, outside the Mediterranean.
    g.byName.Gibraltar.ctrl = 'ITA';
    seaRoutes(g).push({ id: 'b598-iberia', name: 'a test lane', to: 'UK', zones: [zoneOf(g, 'iberia')] });
    expect(foeBasedOn(g, 'ITA', NAVY.beats!.ITA.floor!.seas)).toBe(true);
    placeBoats(g, 'ITA');
    const west = named(g, 'Submarine Fleet')!;
    expect(key(g, west.station)).toBe('iberia');
    // The third is kept by the groups still on Mediterranean seas; the western group's boats are not counted in it.
    const seas = NAVY.beats!.ITA.floor!.seas;
    const onMed = MED.filter((name) => seas.includes(key(g, named(g, name)!.station)));
    expect(onMed).not.toContain('Submarine Fleet');
    expect(onMed.length).toBeGreaterThan(0);
    expect(sum(onMed.map((name) => boats(g, name)))).toBe(34);
    expect(sum(italian(g))).toBe(100);
  });
});

describe('(2) a tie lapses', () => {
  it('Force H loses its tie the month Gibraltar falls, moves its base, and is placed as any other fleet', () => {
    const g = wide(1);
    const f = forceH(g);
    declareWar(g, 'ITA', 'UK');
    expect([f.tied, tieLapses(g, f)]).toEqual([true, false]);
    // A month with Gibraltar held: the tie holds.
    g.turn = 12;
    navalUpkeep(g, 'UK');
    expect(f.tied).toBe(true);
    g.byName.Gibraltar.ctrl = 'ITA';
    expect(tieLapses(g, f)).toBe(true);
    g.turn = 13;
    navalUpkeep(g, 'UK');
    expect(f.tied).toBe(false);
    expect(g.regions[f.base].name).not.toBe('Gibraltar');
    // The base's loss has its own line of news, as for any fleet: the tie goes with it, and says nothing more.
    expect(news(g, 13)).toContain(`With Gibraltar lost, Force H moves its base to ${g.regions[f.base].name}.`);
    // Once lapsed it stays so, even with Gibraltar back.
    g.byName.Gibraltar.ctrl = 'UK';
    g.turn = 14;
    navalUpkeep(g, 'UK');
    expect(f.tied).toBe(false);
  });

  it('in a history campaign too: the Western Approaches Fleet loses its tie the month Cornwall falls', () => {
    const g = createGame(1, null);
    const f = named(g, 'Western Approaches Fleet')!;
    expect(f.tied).toBe(true);
    g.turn = 2;
    navalUpkeep(g, 'UK');
    expect(f.tied).toBe(true);
    g.byName.Cornwall.ctrl = 'GER';
    g.turn = 3;
    navalUpkeep(g, 'UK');
    expect(f.tied).toBe(false);
    expect(g.regions[f.base].name).not.toBe('Cornwall');
  });

  it('a tied fleet whose beat goes out of reach from its port loses its tie, with its home still held', () => {
    const g = wide(1);
    const f = forceH(g);
    declareWar(g, 'ITA', 'UK');
    // Sent to Plymouth, Force H's beat in the Western Mediterranean is three seas off, through the Strait of Gibraltar.
    expect(rebase(g, f, g.byName.Cornwall)).toBe(true);
    const wmed = zoneOf(g, 'wmed');
    expect(key(g, f.station)).toBe('wmed');
    expect(fleetReach(g, f)).toContain(wmed);
    g.turn = 12;
    navalUpkeep(g, 'UK');
    expect(f.tied).toBe(true);
    // Italy takes the Rock: the strait is shut to British ships and the beat is out of reach. Plymouth is not lost.
    g.byName.Gibraltar.ctrl = 'ITA';
    expect(fleetReach(g, f)).not.toContain(wmed);
    expect(g.byName.Cornwall.ctrl).toBe('UK');
    g.turn = 13;
    navalUpkeep(g, 'UK');
    expect(f.tied).toBe(false);
    expect(g.regions[f.base].name).toBe('Cornwall');
    // Freed, its admiral sends it where the work is, as any other fleet: here against a German landing on the Channel
    // coast. Tied, he would have looked only at a beat it could not reach and kept it in harbour; and a tie that has
    // lapsed holds nothing even before its flag is cleared (tiedNow), so the flag set again changes nothing.
    const channel = zoneOf(g, 'channel');
    g.flags[`landed:${channel}:GER`] = g.turn;
    navalAI(g, 'UK');
    expect([key(g, f.zone), f.stance]).toEqual(['channel', 'patrol']);
    f.tied = true;
    expect(tiedNow(g, f)).toBe(false);
    sail(g, f, f.home, 'port');
    navalAI(g, 'UK');
    expect([key(g, f.zone), f.stance]).toEqual(['channel', 'patrol']);
    // With its beat back in reach (the Rock British again), the tie holds and the admiral keeps it to its beat.
    g.byName.Gibraltar.ctrl = 'UK';
    expect(tiedNow(g, f)).toBe(true);
    sail(g, f, f.home, 'port');
    navalAI(g, 'UK');
    expect(key(g, f.zone)).not.toBe('channel');
  });

  it('an untied fleet is left as it was: its base moves when it falls, and nothing else changes', () => {
    const g = createGame(1, null);
    const f = named(g, 'Mediterranean Fleet')!;
    expect(f.tied).toBe(false);
    const station = f.station, duty = f.duty;
    g.turn = 2;
    navalUpkeep(g, 'UK');
    expect([f.tied, f.station, f.duty]).toEqual([false, station, duty]);
    g.byName.Alexandria.ctrl = 'ITA';
    expect(tieLapses(g, f)).toBe(false);
    g.turn = 3;
    navalUpkeep(g, 'UK');
    expect([f.tied, f.station, f.duty]).toEqual([false, station, duty]);
    expect(g.regions[f.base].name).not.toBe('Alexandria');
  });

  it('a tied fleet with its home held and its beat in reach keeps its tie, month after month', () => {
    const g = createGame(1, null);
    const tied = g.fleets.filter((f) => f.tied).map((f) => f.name);
    expect(tied).toEqual(expect.arrayContaining(['Western Approaches Fleet', 'Dover Fleet', 'Swinemunde Fleet', 'Baltic Fleet', 'Black Sea Fleet', 'Northern Fleet']));
    for (const f of g.fleets) if (f.tied) expect(fleetReach(g, f), f.name).toContain(f.station);
    for (let turn = 2; turn <= 4; turn++) {
      g.turn = turn;
      for (const nat of ['UK', 'GER', 'SOV']) navalUpkeep(g, nat);
    }
    for (const name of ['Western Approaches Fleet', 'Dover Fleet', 'Swinemunde Fleet', 'Baltic Fleet', 'Black Sea Fleet', 'Northern Fleet']) expect(named(g, name)!.tied, name).toBe(true);
  });

  it('a home taken after its owner\'s month frees the fleet at once in the enemy\'s forecast, before its flag is cleared', () => {
    const g = wide(1);
    const f = forceH(g);
    declareWar(g, 'ITA', 'UK');
    g.turn = 12;
    navalUpkeep(g, 'UK');
    const central = zoneOf(g, 'central');
    const answering = (): string[] => { const a = fleetsAnswer(g, 'ITA', [central], g.byName.Malta); return [...a.come, ...a.kept, ...a.shy].map((x) => x.name); };
    // Tied to the Western Mediterranean, Force H does not come out against an Italian landing on Malta.
    expect(answering()).not.toContain('Force H');
    // Italy takes the Rock after Britain's month: the flag is cleared only at Britain's next, but the tie no longer holds.
    g.byName.Gibraltar.ctrl = 'ITA';
    expect([f.tied, tiedNow(g, f)]).toEqual([true, false]);
    expect(answering()).toContain('Force H');
  });

  it('a fleet moved from a fallen home while still tied, as a save from before this rule has it, is freed once loaded', () => {
    const g = wide(1);
    const f = forceH(g);
    declareWar(g, 'ITA', 'UK');
    // As b564 left it: Gibraltar lost, Force H based at Malta, still tied to the Western Mediterranean, which is in reach.
    g.byName.Gibraltar.ctrl = 'ITA';
    expect(rebase(g, f, g.byName.Malta)).toBe(true);
    f.tied = true;
    expect(fleetReach(g, f)).toContain(f.station);
    expect(tieLapses(g, f)).toBe(true);
    g.turn = 12;
    navalUpkeep(g, 'UK');
    expect(f.tied).toBe(false);
    // With the Rock British again the home it was tied to stands, and a fleet still tied away from it keeps its tie.
    g.byName.Gibraltar.ctrl = 'UK';
    f.tied = true;
    expect(tieLapses(g, f)).toBe(false);
  });
});

describe('(3) the Bordeaux flotilla\'s beat at stops 1 to 3, by the record\'s seas', () => {
  const H = NAVY.beats!.ITA.groups[0].hunts!;
  const bordeaux = (g: Game): Pick<Fleet, 'nat' | 'base' | 'home'> => ({ nat: 'ITA', base: g.byName.Bordeaux.id, home: zoneOf(g, 'biscay') });
  /** A British lane through these seas, laid in for the test (each game has its own list of routes). */
  const lane = (g: Game, id: string, seas: string[]): void => { seaRoutes(g).push({ id, name: 'a test lane', to: 'UK', zones: seas.map((k) => zoneOf(g, k)) }); };

  it('the historian\'s seas by half-year are in the data, every one on the chart', () => {
    expect(H.to).toBe('UK');
    expect(H.seas.map(([turn]) => turn)).toEqual([13, 19, 25, 30, 37, 43]);
    expect(H.seas[0][1]).toEqual(['wapp', 'natl']);
    expect(H.seas[1][1]).toEqual(['azores', 'iberia', 'canary', 'sierra']);
    expect(H.seas[3][1]).toContain('narrows');
    const g = wide();
    for (const [, seas] of H.seas) for (const k of seas) expect(zoneOf(g, k), k).toBeGreaterThanOrEqual(0);
  });

  it('every month it sails, on a sea that carries a British lane, and never the list\'s first sea for its own sake', () => {
    for (const stop of [1, 2, 3]) {
      const g = wide(stop);
      frenchFall(g);
      for (let turn = 13; turn <= 46; turn++) {
        g.turn = turn;
        placeBoats(g, 'ITA');
        const atl = named(g, 'Atlantic Submarine Fleet');
        if (!atl) continue;
        // On this chart Britain's one lane runs through the North Atlantic, the Western Approaches and the Irish Sea.
        // To February 1941 the list's two seas both carry it and the first of them is taken; from March 1941 no sea of
        // the list carries it, and the Western Approaches is the nearest to the list that does.
        expect(key(g, atl.station), `stop ${stop} turn ${turn}`).toBe('wapp');
        expect(lanesTo(g, 'UK', atl.station), `stop ${stop} turn ${turn}`).toBeGreaterThan(0);
        expect(fleetReach(g, atl)).toContain(atl.station);
      }
      expect(boats(g, 'Atlantic Submarine Fleet'), `stop ${stop}`).toBeGreaterThan(0);
    }
  });

  it('takes the sea of the list with the most British lane, the first in the list between seas with as much', () => {
    const g = wide(1);
    frenchFall(g);
    g.turn = 14;
    expect(key(g, huntBeat(g, bordeaux(g), H))).toBe('wapp');
    lane(g, 'b603-natl', ['natl']);
    expect(lanesTo(g, 'UK', zoneOf(g, 'natl'))).toBe(2);
    expect(key(g, huntBeat(g, bordeaux(g), H))).toBe('natl');
    // From March 1941 the list's own seas come first once one carries a British lane: the most of them, then the first.
    g.turn = 19;
    lane(g, 'b603-canary', ['canary']);
    expect(key(g, huntBeat(g, bordeaux(g), H))).toBe('canary');
    lane(g, 'b603-iberia', ['iberia', 'azores']);
    expect(key(g, huntBeat(g, bordeaux(g), H))).toBe('azores');
    lane(g, 'b603-canary2', ['canary']);
    expect(key(g, huntBeat(g, bordeaux(g), H))).toBe('canary');
  });

  it('where none of the list carries one: the nearest sea to the list that does, within the flotilla\'s reach', () => {
    const g = wide(1);
    frenchFall(g);
    // February to August 1942, the American coasts: the Western Approaches is two seas from the Sargasso Sea, by the
    // Azores, the North Atlantic further.
    g.turn = 32;
    expect(key(g, huntBeat(g, bordeaux(g), H))).toBe('wapp');
    // A British lane through the Tropical Atlantic, next to the list, is out of the flotilla's reach from Bordeaux.
    lane(g, 'b603-tropic', ['tropic']);
    expect(fleetReach(g, bordeaux(g))).not.toContain(zoneOf(g, 'tropic'));
    expect(key(g, huntBeat(g, bordeaux(g), H))).toBe('wapp');
    // The Canary Basin is as near to the list as the Western Approaches: with one lane each, the chart's order; with
    // more, the Canary Basin.
    lane(g, 'b603-canary', ['canary']);
    expect(key(g, huntBeat(g, bordeaux(g), H))).toBe('wapp');
    lane(g, 'b603-canary2', ['canary']);
    expect(key(g, huntBeat(g, bordeaux(g), H))).toBe('canary');
    // Before the list's first half-year there is no beat by it.
    g.turn = 12;
    expect(huntBeat(g, bordeaux(g), H)).toBe(-1);
  });

  it('where no sea in its reach carries a British lane, it keeps its dated beat', () => {
    const g = wide(1);
    frenchFall(g);
    // At war with the United States, Britain's Atlantic lane carries nothing.
    declareWar(g, 'USA', 'UK');
    for (const turn of [16, 19]) {
      g.turn = turn;
      expect(huntBeat(g, bordeaux(g), H), `turn ${turn}`).toBe(-1);
      placeBoats(g, 'ITA');
      const atl = named(g, 'Atlantic Submarine Fleet')!;
      expect(key(g, atl.station), `turn ${turn}`).toBe(turn < 19 ? 'natl' : 'iberia');
    }
  });

  it('at stops 4 and 5 the beat is by need, as before', () => {
    const g = wide(4);
    frenchFall(g);
    declareWar(g, 'ITA', 'UK');
    g.turn = 13;
    placeBoats(g, 'ITA');
    const atl = named(g, 'Atlantic Submarine Fleet')!;
    // Need finds no sea thicker than the dated beat's: the North Atlantic, not the list's.
    expect(key(g, atl.station)).toBe('natl');
  });

  it('a history campaign has no flotilla to place', () => {
    const g = createGame(1, null);
    frenchFall(g);
    g.turn = 16;
    placeBoats(g, 'ITA');
    expect(named(g, 'Atlantic Submarine Fleet')).toBeUndefined();
  });
});

describe('(4) the names and the carriers are the rows, at every stop', () => {
  const admiral = (g: Game, name: string): string | undefined => named(g, name)?.admiral?.name;

  it('Bordeaux\'s commanders by date: Parona, Polacchini from turn 25, Grossi from turn 40, each with his line of news', () => {
    for (const stop of [1, 3, 4]) {
      const g = wide(stop);
      frenchFall(g);
      declareWar(g, 'ITA', 'UK');
      const seen: Record<number, string | undefined> = {};
      for (let turn = 13; turn <= 41; turn++) {
        g.turn = turn;
        placeBoats(g, 'ITA');
        seen[turn] = admiral(g, 'Atlantic Submarine Fleet');
      }
      expect([seen[13], seen[24], seen[25], seen[39], seen[40], seen[41]], `stop ${stop}`).toEqual(['Angelo Parona', 'Angelo Parona', 'Romolo Polacchini', 'Romolo Polacchini', 'Enzo Grossi', 'Enzo Grossi']);
      expect(news(g, 25), `stop ${stop}`).toContain('Romolo Polacchini takes command of the Atlantic Submarine Fleet.');
      expect(news(g, 40), `stop ${stop}`).toContain('Enzo Grossi takes command of the Atlantic Submarine Fleet.');
      expect(named(g, 'Atlantic Submarine Fleet')!.admiral!.level).toBe(1);
    }
  });

  it('a group formed after a commander\'s date forms under him, with no second line', () => {
    const g = wide(1);
    declareWar(g, 'ITA', 'UK');
    // Bordeaux is taken only in turn 30: the flotilla forms then, under Polacchini.
    g.turn = 29;
    placeBoats(g, 'ITA');
    expect(named(g, 'Atlantic Submarine Fleet')).toBeUndefined();
    frenchFall(g);
    g.turn = 30;
    placeBoats(g, 'ITA');
    expect(admiral(g, 'Atlantic Submarine Fleet')).toBe('Romolo Polacchini');
    expect(news(g, 30)).toContain('The Atlantic Submarine Fleet forms at Bordeaux.');
    expect(news(g, 30).some((t) => /takes command/.test(t))).toBe(false);
  });

  it('the U-boats in the Mediterranean: Donitz, then Oehrn from turn 27, Kreisch from 29 and Hartmann from 53', () => {
    const g = wide(1);
    const groups = NAVY.beats!.GER.groups.map((G) => G.name);
    const seen: Record<number, (string | undefined)[]> = {};
    for (let turn = 26; turn <= 54; turn++) {
      g.turn = turn;
      placeBoats(g, 'GER');
      seen[turn] = groups.map((name) => admiral(g, name));
    }
    // October 1941: six boats, all east, under the head of the U-boat arm.
    expect(seen[26]).toEqual([undefined, 'Karl Donitz']);
    expect(seen[27]).toEqual([undefined, 'Victor Oehrn']);
    expect(seen[28]).toEqual(['Victor Oehrn', 'Victor Oehrn']);
    expect(seen[29]).toEqual(['Leo Kreisch', 'Leo Kreisch']);
    expect(seen[52]).toEqual(['Leo Kreisch', 'Leo Kreisch']);
    expect(seen[53]).toEqual(['Werner Hartmann', 'Werner Hartmann']);
    expect(news(g, 29)).toContain(`Leo Kreisch takes command of the ${groups[1]}.`);
  });

  it('the Mediterranean Fleet\'s carrier: the Glorious on the first day, the Eagle from turn 9, at every stop', () => {
    for (const stop of [1, 2, 3, 4, 5]) {
      const g = wide(stop);
      const med = named(g, 'Mediterranean Fleet')!;
      const cv: Record<number, number> = {};
      for (let turn = 2; turn <= 14; turn++) {
        g.turn = turn;
        navalUpkeep(g, 'UK');
        cv[turn] = med.ships.cv;
      }
      // The Glorious leaves for the Indian Ocean in October 1939 (b601) and the Eagle comes from the China Station in
      // May 1940. (At stops 1 and 2 the Glorious is back from January to April 1940, and the Illustrious joins the
      // Eagle in September: see (5).)
      expect([cv[2], cv[9]], `stop ${stop}`).toEqual([0, 1]);
      expect(news(g, 2), `stop ${stop}`).toContain('The carrier Glorious leaves the Mediterranean Fleet for the Indian Ocean.');
      // With the far side off there is no China Fleet: she comes from off the chart at every stop, under the row's own
      // key (on the far side's chart she leaves the China Fleet: far-on.test.ts).
      expect(g.fleets.some((f) => f.name === 'China Fleet')).toBe(false);
      expect(news(g, 9), `stop ${stop}`).toContain('The carrier Eagle joins the Mediterranean Fleet.');
      expect(g.flags['arr:9:UK:Mediterranean Fleet'], `stop ${stop}`).toBe(9);
    }
  });

  it('a history campaign, and a player\'s own navy, keep the rows as they were, but for a ship coming onto the chart', () => {
    for (const g of [createGame(1, null), wide(1, 'UK')]) {
      const player = g.human === 'UK';
      const med = named(g, 'Mediterranean Fleet')!, home = named(g, 'Home Fleet')!;
      for (let turn = 2; turn <= 14; turn++) {
        g.turn = turn;
        navalUpkeep(g, 'UK');
        // The Mediterranean keeps its first-day carrier throughout, with no news of the Glorious. A player's is joined
        // by the Eagle in May 1940: she comes from off the chart, where he cannot order her (the director's note of
        // 15:52Z on rider b639).
        expect(med.ships.cv, `turn ${turn}`).toBe(player && turn >= 9 ? 2 : 1);
      }
      expect([med.ships.cv, home.ships.cv]).toEqual([player ? 2 : 1, 3]);
      expect(g.headlines.some((h) => /Glorious/.test(h.text))).toBe(false);
      expect(g.headlines.filter((h) => /Eagle/.test(h.text)).map((h) => [h.turn, h.text])).toEqual(player ? [[9, 'The carrier Eagle joins the Mediterranean Fleet.']] : []);
    }
  });
});

describe('(5) the carriers\' rows at stops 1 and 2: each carrier in and out of the Mediterranean, one row each way', () => {
  const FLEETS = ['Home Fleet', 'Mediterranean Fleet', 'Force H'];
  const uk = (g: Game, name: string): Fleet | undefined => g.fleets.find((f) => f.nat === 'UK' && f.name === name);
  /** Spans [from turn, Home Fleet, Mediterranean Fleet, Force H], laid out month by month to the last turn (-1: Force H not yet formed). */
  function months(spans: number[][], to = 41): Record<number, number[]> {
    const out: Record<number, number[]> = {};
    for (let turn = 1; turn <= to; turn++) out[turn] = spans.filter((s) => s[0] <= turn).at(-1)!.slice(1);
    return out;
  }
  /** Britain's carriers by fleet each month from the first day, with no battle; 'before' plays the month's losses ahead of its upkeep. */
  function carriers(g: Game, to = 41, before?: (turn: number) => void): Record<number, number[]> {
    const out: Record<number, number[]> = {};
    for (let turn = 1; turn <= to; turn++) {
      g.turn = turn;
      before?.(turn);
      navalUpkeep(g, 'UK');
      out[turn] = FLEETS.map((name) => uk(g, name)?.ships.cv ?? -1);
    }
    return out;
  }
  /** Play sinks one of a fleet's carriers. */
  const sink = (g: Game, name: string): void => { const f = uk(g, name)!; f.ships.cv -= 1; f.lost.cv += 1; };

  // By the rows, with no battle: the Glorious out at 2, back at 5 and to the Home Fleet at 8; the Eagle in at 9 and out
  // at 20; the Illustrious to the Home Fleet at 10 (every campaign) and on to the East at 13; the Ark Royal out of the
  // Home Fleet at 10, as Force H forms with her by its book (June 1940); the Furious's ferry run at 21 and 22; Pedestal at 36 and
  // 37; Torch at 39 and the Victorious home at 40. The Formidable waits for the Illustrious until her row lapses at 21,
  // and the Eagle in the West for the Ark Royal until hers lapses at 36.
  const BOOK = [
    [1, 2, 1, -1], [2, 2, 0, -1], [5, 2, 1, -1], [8, 3, 0, -1], [9, 3, 1, -1], [10, 3, 1, 1], [13, 2, 2, 1],
    [20, 2, 1, 1], [21, 2, 1, 2], [22, 3, 1, 1], [36, 1, 1, 3], [37, 3, 1, 1], [39, 1, 1, 4], [40, 2, 1, 3],
  ];
  // Stops 3 to 5 as before, b601's two rows apart: the Illustrious at 10, the Formidable at 16, the Victorious at 21.
  const LOOSE = [[1, 2, 1, -1], [2, 2, 0, -1], [9, 2, 1, -1], [10, 3, 1, -1], [11, 3, 1, 1], [16, 3, 2, 1], [21, 4, 2, 1]];
  // A history campaign and the player's own Britain: no row of the dial at all.
  const HISTORY = [[1, 2, 1, -1], [10, 3, 1, -1], [11, 3, 1, 1], [16, 3, 2, 1], [21, 4, 2, 1]];
  // A player's own Britain at stops 1 and 2: the history campaign's, and the Eagle from off the chart in May 1940 (the
  // director's note of 15:52Z on rider b639); her leaving in May 1941 is his to make or not.
  const PLAYER = [[1, 2, 1, -1], [9, 2, 2, -1], [10, 3, 2, -1], [11, 3, 2, 1], [16, 3, 3, 1], [21, 4, 3, 1]];

  it('are in the data: one row each way, every move from a fleet on the chart, every key of its own, no record\'s sinking', () => {
    const rows = NAVY.arrivals.filter((r) => r[1] === 'UK' && r[5]?.stops);
    const keys = rows.map(([turn, who, fleet, , , d]) => `${turn}:${who}:${fleet}<${d!.from ?? ''}`);
    expect(new Set(keys).size).toBe(keys.length);
    const g = wide(1);
    forceH(g);
    for (const [, , fleet, , what, d] of rows) {
      expect(uk(g, fleet), what).toBeDefined();
      if (d!.from) expect(uk(g, d!.from), what).toBeDefined();
      expect(d!.sunk, what).toBeUndefined();
    }
    // Rows by date, so a campaign that reads them late reads each fleet's in the record's order.
    const turns = rows.map((r) => r[0]);
    expect(turns).toEqual([...turns].sort((a, b) => a - b));
    // The two that wait, each lapsing when the record's window for her posting closed: the Formidable's in the East at
    // 21 (11:21Z) and the Eagle's in the West at 36, when Pedestal's rows take over (10:02Z); and the Formidable's
    // undated row that the book's stand in for.
    expect(rows.filter((r) => r[5]!.waits).map((r) => [r[0], r[2], r[4], r[5]!.lapse])).toEqual([[19, 'Mediterranean Fleet', 'the carrier Formidable', 21], [30, 'Force H', 'the carrier Eagle', 36]]);
    // Force H forms in July 1940 by its row, everywhere but at stops 1 and 2, where its book has it in June with the
    // historian's first ships (the director's note of 10:06Z); the Ark Royal leaves the Home Fleet that month.
    const h = NAVY.fleets.find((f) => f.name === 'Force H')!;
    expect([h.from, h.ships]).toEqual([11, { bb: 2, cv: 1, ca: 2, dd: 9, ss: 0 }]);
    expect(h.book).toEqual({ stops: [1, 2], from: 10, ships: { bb: 1, cv: 1, ca: 1, dd: 9, ss: 0 } });
    expect(rows.find((r) => /Ark Royal/.test(r[4]))![0]).toBe(10);
    // The Eagle's row of turn 9 comes from the China Fleet where the far side has one, and keeps its key and its stops.
    expect(rows.find((r) => r[0] === 9)![5]).toEqual({ stops: [1, 2, 3, 4, 5], fromIf: 'China Fleet' });
    expect(NAVY.arrivals.find((r) => r[0] === 16 && r[2] === 'Mediterranean Fleet')![5]).toEqual({ not: [1, 2] });
    // Carriers on no fleet's roster have no row: the Indomitable at Pedestal, and the Argus at Gibraltar.
    expect(NAVY.arrivals.some((r) => /Indomitable|Argus/.test(r[4]))).toBe(false);
  });

  it('each fleet\'s carriers month by month at stops 1 and 2, with no battle', () => {
    for (const stop of [1, 2]) {
      const g = wide(stop);
      expect(carriers(g), `stop ${stop}`).toEqual(months(BOOK));
      // The record's shape: never more than two in the East, and one in the West from Force H's first month to Pedestal
      // (the Furious's ferry run apart).
      const seen = months(BOOK);
      for (let turn = 1; turn <= 41; turn++) expect(seen[turn][1]).toBeLessThanOrEqual(2);
      for (let turn = 10; turn <= 35; turn++) expect(seen[turn][2], `turn ${turn}`).toBe(turn === 21 ? 2 : 1);
      expect(news(g, 5)).toContain('The carrier Glorious, back from the Indian Ocean, joins the Mediterranean Fleet.');
      expect(news(g, 8)).toContain('The carrier Glorious leaves the Mediterranean Fleet for the Home Fleet.');
      expect(news(g, 10)).toEqual(expect.arrayContaining(['Force H forms at Gibraltar.', 'The carrier Ark Royal leaves the Home Fleet for Force H.']));
      expect(news(g, 13)).toContain('The carrier Illustrious leaves the Home Fleet for the Mediterranean Fleet.');
      expect(news(g, 20)).toContain('The carrier Eagle leaves the Mediterranean Fleet for the South Atlantic.');
      expect(news(g, 21)).toContain('The carrier Furious leaves the Home Fleet for Force H.');
      expect(news(g, 22)).toContain('The carrier Furious leaves Force H for the Home Fleet.');
      expect(news(g, 36)).toContain('The carriers Victorious and Furious leave the Home Fleet for Force H.');
      expect(news(g, 37)).toContain('The carriers Victorious and Furious leave Force H for the Home Fleet.');
      expect(news(g, 39)).toEqual(expect.arrayContaining(['The carrier Formidable joins Force H.', 'The carriers Victorious and Furious leave the Home Fleet for Force H.']));
      expect(news(g, 40)).toContain('The carrier Victorious leaves Force H for the Home Fleet.');
      // The Formidable never came to the East (the Illustrious never left it): her undated row of turn 16 is not read
      // here, and she comes to Torch from off the chart. The Eagle is counted once: in the East from 9 to 19, never West.
      expect(g.headlines.filter((h) => /Formidable/.test(h.text)).map((h) => h.turn)).toEqual([39]);
      expect(g.headlines.filter((h) => /Eagle/.test(h.text)).map((h) => h.turn)).toEqual([9, 20]);
    }
  });

  it('no carrier is counted twice: the fleets together hold the carriers the rows have on the chart that month', () => {
    // [carrier, on the chart from, until] by the rows: the first day's Ark Royal and Furious (Home Fleet) and Glorious
    // (Mediterranean, away from 2 to 4); the Eagle from the China Station at 9, to the South Atlantic at 20; the
    // Illustrious at 10, the Victorious at 21, the Formidable at Torch.
    const roster: [string, number, number][] = [['Ark Royal', 1, 99], ['Furious', 1, 99], ['Glorious', 1, 1], ['Glorious', 5, 99], ['Eagle', 9, 19], ['Illustrious', 10, 99], ['Victorious', 21, 99], ['Formidable', 39, 99]];
    const seen = carriers(wide(1));
    for (let turn = 1; turn <= 41; turn++) {
      const held = seen[turn].reduce((a, b) => a + Math.max(0, b), 0);
      expect(held, `turn ${turn}`).toBe(roster.filter(([, a, b]) => a <= turn && turn <= b).length);
    }
  });

  it('a Force H that play has robbed of its carrier has the Eagle, at turn 30 or the first month after with the gap', () => {
    for (const stop of [1, 2]) {
      // The Ark Royal sunk at 25: the Eagle at 30, not before.
      const g = wide(stop);
      const seen = carriers(g, 41, (turn) => { if (turn === 25) sink(g, 'Force H'); });
      expect([24, 25, 29, 30, 35, 36, 37, 39, 40].map((t) => seen[t][2])).toEqual([1, 0, 0, 1, 1, 3, 1, 4, 3]);
      expect(news(g, 30)).toContain('The carrier Eagle joins Force H.');
      expect(g.headlines.filter((h) => /Eagle joins Force H/.test(h.text)).map((h) => h.turn)).toEqual([30]);
      expect(gapOf(g, uk(g, 'Force H')!, 'cv')).toBe(0);
      // Sunk at 33: the Eagle that month, with its news.
      const late = wide(stop);
      const later = carriers(late, 41, (turn) => { if (turn === 33) sink(late, 'Force H'); });
      expect([30, 32, 33, 34].map((t) => later[t][2])).toEqual([1, 1, 1, 1]);
      expect(late.headlines.filter((h) => /Eagle joins Force H/.test(h.text)).map((h) => h.turn)).toEqual([33]);
    }
  });

  it('the Eagle comes West once: a second carrier lost brings no other', () => {
    const g = wide(1);
    const seen = carriers(g, 41, (turn) => { if (turn === 25 || turn === 31) sink(g, 'Force H'); });
    expect([25, 30, 31, 35].map((t) => seen[t][2])).toEqual([0, 1, 0, 0]);
    expect(g.headlines.filter((h) => /Eagle joins Force H/.test(h.text))).toHaveLength(1);
  });

  it('the Eagle\'s waiting row lapses at 36: a Force H robbed from Pedestal on never has her', () => {
    for (const stop of [1, 2]) {
      // Robbed at 35: the Eagle that month, the last before her row lapses.
      const g = wide(stop);
      const seen = carriers(g, 41, (turn) => { if (turn === 35) sink(g, 'Force H'); });
      expect([34, 35, 36, 37].map((t) => seen[t][2])).toEqual([1, 1, 3, 1]);
      expect(g.headlines.filter((h) => /Eagle joins Force H/.test(h.text)).map((h) => h.turn)).toEqual([35]);
      // Robbed at 36, in Pedestal's month, or at 38: the row has lapsed and nothing replaces her.
      for (const at of [36, 38]) {
        const late = wide(stop);
        const later = carriers(late, 41, (turn) => { if (turn === at) sink(late, 'Force H'); });
        expect(later[at][2], `robbed at ${at}`).toBe(at === 36 ? 2 : 0);
        expect(later[41][2], `robbed at ${at}`).toBe(2);
        expect(late.headlines.some((h) => /Eagle joins Force H/.test(h.text)), `robbed at ${at}`).toBe(false);
        expect(gapOf(late, uk(late, 'Force H')!, 'cv')).toBe(1);
      }
    }
  });

  it('a Mediterranean Fleet that play has robbed of the Illustrious has the Formidable from turn 19 until her row lapses at 21, and sends her to Torch', () => {
    for (const stop of [1, 2]) {
      // Bombed and sunk at 17 (the record's Excess, which only put her out of action): the Formidable at 19.
      const g = wide(stop);
      const seen = carriers(g, 41, (turn) => { if (turn === 17) sink(g, 'Mediterranean Fleet'); });
      expect([16, 17, 18, 19, 20, 38, 39, 40].map((t) => seen[t][1])).toEqual([2, 1, 1, 2, 1, 1, 0, 0]);
      expect(seen[39][2]).toBe(4);
      expect(news(g, 19)).toContain('The carrier Formidable joins the Mediterranean Fleet.');
      expect(news(g, 39)).toContain('The carrier Formidable leaves the Mediterranean Fleet for Force H.');
      // Lost at 20, the month the Eagle leaves and the last before the row lapses: the Formidable that month.
      const late = wide(stop);
      const later = carriers(late, 41, (turn) => { if (turn === 20) sink(late, 'Mediterranean Fleet'); });
      expect([19, 20, 21, 38, 39].map((t) => later[t][1])).toEqual([2, 1, 1, 1, 0]);
      expect(late.headlines.filter((h) => /Formidable joins/.test(h.text)).map((h) => [h.turn, h.text])).toEqual([[20, 'The carrier Formidable joins the Mediterranean Fleet.']]);
      // Lost at 21 or later (the record's window closed in May 1941, 11:21Z): no Formidable in the East, and she comes
      // to Torch from off the chart, so the East never holds more than two fleet carriers.
      for (const at of [21, 25]) {
        const lapsed = wide(stop);
        const after = carriers(lapsed, 41, (turn) => { if (turn === at) sink(lapsed, 'Mediterranean Fleet'); });
        expect([at - 1, at, 38, 39, 40].map((t) => after[t][1]), `lost at ${at}`).toEqual([1, 0, 0, 0, 0]);
        expect(after[39][2], `lost at ${at}`).toBe(4);
        expect(lapsed.headlines.filter((h) => /Formidable/.test(h.text)).map((h) => [h.turn, h.text]), `lost at ${at}`).toEqual([[39, 'The carrier Formidable joins Force H.']]);
        expect(gapOf(lapsed, uk(lapsed, 'Mediterranean Fleet')!, 'cv'), `lost at ${at}`).toBe(1);
      }
    }
  });

  it('once the Formidable has come to Torch from off the chart, her row in the East is closed', () => {
    const g = wide(1);
    carriers(g, 41);
    sink(g, 'Mediterranean Fleet');
    for (let turn = 42; turn <= 45; turn++) {
      g.turn = turn;
      navalUpkeep(g, 'UK');
    }
    expect(uk(g, 'Mediterranean Fleet')!.ships.cv).toBe(0);
    expect(g.headlines.filter((h) => /Formidable/.test(h.text)).map((h) => h.turn)).toEqual([39]);
  });

  it('a move finds the ships where they are, or brings none: no carrier comes out of a fleet that has lost hers', () => {
    const g = wide(1);
    // The Home Fleet's three carriers all sunk before the Illustrious sails for the East.
    const seen = carriers(g, 14, (turn) => { if (turn === 12) for (let k = 0; k < 3; k++) sink(g, 'Home Fleet'); });
    expect([seen[12], seen[13]]).toEqual([[0, 1, 1], [0, 1, 1]]);
    expect(news(g, 13).some((t) => /Illustrious/.test(t))).toBe(false);
  });

  it('stops 3 to 5, a history campaign and the player\'s own Britain keep what they had, b601\'s two rows apart', () => {
    for (const stop of [3, 4, 5]) {
      const g = wide(stop);
      expect(carriers(g), `stop ${stop}`).toEqual(months(LOOSE));
      expect(news(g, 16)).toContain('The carrier Formidable joins the Mediterranean Fleet.');
      expect(g.headlines.some((h) => /Ark Royal|Furious|Victorious and|leaves the Home Fleet|joins Force H/.test(h.text))).toBe(false);
    }
    for (const g of [createGame(1, null), wide(1, 'UK'), wide(2, 'UK')]) {
      const player = g.human === 'UK';
      expect(carriers(g)).toEqual(months(player ? PLAYER : HISTORY));
      expect(news(g, 16)).toContain('The carrier Formidable joins the Mediterranean Fleet.');
      expect(g.headlines.some((h) => /Glorious|Ark Royal|Furious|leaves/.test(h.text))).toBe(false);
      expect(g.headlines.filter((h) => /Eagle/.test(h.text)).map((h) => h.turn)).toEqual(player ? [9] : []);
    }
  });
});

describe('(6) Force H\'s first months at stops 1 and 2: June 1940, with the historian\'s ships', () => {
  const FLEETS = ['Home Fleet', 'Mediterranean Fleet', 'Force H'];
  const uk = (g: Game, name: string): Fleet | undefined => g.fleets.find((f) => f.nat === 'UK' && f.name === name);
  /** Each fleet's [capital ships, carriers, cruisers, destroyers] month by month, with no battle (null: not formed). */
  function strength(g: Game, to = 14, first = 1, before?: (turn: number) => void): Record<number, (number[] | null)[]> {
    const out: Record<number, (number[] | null)[]> = {};
    for (let turn = first; turn <= to; turn++) {
      g.turn = turn;
      before?.(turn);
      navalUpkeep(g, 'UK');
      out[turn] = FLEETS.map((name) => { const f = uk(g, name); return f ? [f.ships.bb, f.ships.cv, f.ships.ca, f.ships.dd] : null; });
    }
    return out;
  }
  const head = (x: number[] | null): number[] => x!.slice(0, 3);
  /** The Home Fleet's and the Mediterranean Fleet's [bb, cv, ca], and Force H whole, for turns 9 to 14 (or from 'first'). */
  const shape = (seen: Record<number, (number[] | null)[]>, first = 9): unknown[] =>
    [9, 10, 11, 12, 13, 14].filter((t) => t >= first).map((t) => [head(seen[t][0]), head(seen[t][1]), seen[t][2]]);
  // The Resolution, the Ark Royal, the Arethusa and nine destroyers at 10, with the Hood and the Valiant from the Home
  // Fleet; the Enterprise and two destroyers at 11; the Hood home and the Renown in at 12; the Valiant east at 13 (Hats),
  // with the Illustrious, and two destroyers out (home and to Dakar: 11:21Z). The Hood counts as a capital ship, as
  // every battlecruiser does.
  const BOOK = [
    [[7, 3, 12], [3, 1, 7], null],
    [[5, 3, 12], [3, 1, 7], [3, 1, 1, 9]],
    [[5, 3, 12], [3, 1, 7], [3, 1, 2, 11]],
    [[6, 3, 12], [3, 1, 7], [3, 1, 2, 11]],
    [[6, 2, 12], [4, 2, 7], [2, 1, 2, 9]],
    [[6, 2, 12], [4, 2, 7], [2, 1, 2, 9]],
  ];
  // Stops 3 to 5, a history campaign and the player's own Britain: Force H at 11 with its row's ships, and no capital
  // ship moves (the carriers as (5) has them: the Illustrious to the Home Fleet at 10).
  const ROW = [
    [[7, 2, 12], [3, 1, 7], null],
    [[7, 3, 12], [3, 1, 7], null],
    [[7, 3, 12], [3, 1, 7], [2, 1, 2, 9]],
    [[7, 3, 12], [3, 1, 7], [2, 1, 2, 9]],
    [[7, 3, 12], [3, 1, 7], [2, 1, 2, 9]],
    [[7, 3, 12], [3, 1, 7], [2, 1, 2, 9]],
  ];

  it('forms at Gibraltar in June 1940 with the Resolution, the Ark Royal and the Arethusa, the Hood and the Valiant from the Home Fleet', () => {
    for (const stop of [1, 2]) {
      const g = wide(stop);
      const seen = strength(g);
      expect(shape(seen), `stop ${stop}`).toEqual(BOOK);
      // No destroyer leaves either of the others: theirs are as at stop 3, month by month.
      const row = strength(wide(3));
      for (let t = 9; t <= 14; t++) for (const i of [0, 1]) expect(seen[t][i]![3], `turn ${t}`).toBe(row[t][i]![3]);
      // Every capital ship counted once: from the Renown's month the three fleets hold as many as at stop 3, where Force
      // H's two come with it from off the chart (the Resolution and the Renown are on no first-day roster).
      const capital = (x: (number[] | null)[]): number => sum(x.map((f) => f?.[0] ?? 0));
      for (let t = 12; t <= 14; t++) expect(capital(seen[t]), `turn ${t}`).toBe(capital(row[t]));
      expect(news(g, 10)).toEqual(expect.arrayContaining([
        'Force H forms at Gibraltar.', 'The carrier Ark Royal leaves the Home Fleet for Force H.', 'The battlecruiser Hood and the battleship Valiant leave the Home Fleet for Force H.',
      ]));
      expect(news(g, 11)).toContain('The cruiser Enterprise and two destroyers join Force H.');
      expect(news(g, 12)).toEqual(expect.arrayContaining(['The battlecruiser Hood leaves Force H for the Home Fleet.', 'The battlecruiser Renown joins Force H.']));
      expect(news(g, 13)).toEqual(expect.arrayContaining([
        'The battleship Valiant leaves Force H for the Mediterranean Fleet.', 'Two destroyers leave Force H for home waters and Dakar.',
      ]));
      // The Resolution's torpedo at Dakar is no row.
      expect(g.headlines.some((h) => /Resolution/.test(h.text))).toBe(false);
    }
  });

  it('is its row\'s ships from September 1940 to late 1941, as at stops 3 to 5', () => {
    // November 1941 (turn 27), the month the record's Ark Royal was sunk, which only play decides here.
    const row = NAVY.fleets.find((f) => f.name === 'Force H')!.ships;
    for (const stop of [1, 2, 3]) {
      const g = wide(stop);
      const seen = strength(g, 27);
      // (Its carriers as (5) has them: the Furious's ferry run at 21 apart, one.)
      for (let t = 13; t <= 27; t++) expect(seen[t][2], `stop ${stop}, turn ${t}`).toEqual([row.bb, stop < 3 && t === 21 ? 2 : row.cv, row.ca, row.dd]);
      expect(uk(g, 'Force H')!.ships, `stop ${stop}`).toEqual(row);
    }
  });

  it('at stops 3 to 5, in a history campaign and under a player, forms in July 1940 with its row\'s ships, as before', () => {
    for (const g of [wide(3), wide(4), wide(5), createGame(1, null), wide(1, 'UK'), wide(2, 'UK')]) {
      // (A player's Mediterranean Fleet has the Eagle from off the chart besides: (4).)
      expect(shape(strength(g))).toEqual(g.human === 'UK' ? ROW.map(([h, m, f]) => [h, [m![0], m![1] + 1, m![2]], f]) : ROW);
      expect(news(g, 11)).toContain('Force H forms at Gibraltar.');
      expect(g.headlines.some((h) => /Hood|Valiant|Renown|Enterprise|destroyers leave Force H/.test(h.text))).toBe(false);
    }
  });

  it('a dial turned to stop 1 late brings Force H to the same ships, none of them twice', () => {
    // Turned in July 1940 before Force H has formed by its row: it forms by its book that month.
    const july = wide(3);
    strength(july, 10);
    july.flags.hist = 1;
    expect(shape(strength(july, 14, 11), 11)).toEqual(BOOK.slice(2));
    // Turned after Force H has formed with its row's ships, at 12 or at 13: the Hood and the Valiant pass through it,
    // the Renown and the Enterprise are already there, and the months gone by are not news.
    for (const at of [12, 13]) {
      const g = wide(3);
      const seen = strength(g, 14, 1, (turn) => { if (turn === at) g.flags.hist = 1; });
      expect(shape(seen, 13), `turned at ${at}`).toEqual(BOOK.slice(4));
      expect(g.headlines.some((h) => /Renown|Enterprise/.test(h.text)), `turned at ${at}`).toBe(false);
      // (A row of the month it is turned in is that month's news: the Hood home at 12.)
      expect(g.headlines.filter((h) => /Hood/.test(h.text)).map((h) => [h.turn, h.text]), `turned at ${at}`)
        .toEqual(at === 12 ? [[12, 'The battlecruiser Hood leaves Force H for the Home Fleet.']] : []);
      expect(news(g, 13)).toContain('The battleship Valiant leaves Force H for the Mediterranean Fleet.');
    }
  });

  it('a campaign saved when the Ark Royal\'s leaving row was dated 11 loads with her gone once, and takes its Force H to the same ships', () => {
    // As the build before the book left a stop 1 campaign in August 1940: Force H formed by its row, the Ark Royal's
    // row read under its old date, none of the book's rows read.
    const g = wide(1);
    strength(g, 12);
    for (const k of Object.keys(g.flags)) if (/^arr:1[0-2]:UK:(Force H|Home Fleet)/.test(k)) delete g.flags[k];
    g.flags['arr:11:UK:Home Fleet'] = 11;
    Object.assign(uk(g, 'Force H')!.ships, NAVY.fleets.find((f) => f.name === 'Force H')!.ships);
    uk(g, 'Home Fleet')!.ships.bb = 7;
    const back = loadGame(saveGame(g))!;
    expect([back.flags['arr:10:UK:Home Fleet'], back.flags['arr:11:UK:Home Fleet']]).toEqual([11, undefined]);
    const seen = strength(back, 14, 13);
    expect(shape(seen, 13)).toEqual(BOOK.slice(4));
  });
});
