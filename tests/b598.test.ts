// Ruling 233 rider b598: need sees fleets, a lost home frees a tie, and the Bordeaux flotilla is never parked on an
// empty sea. (1) Need counts lanes, and a lane and a fleet's weight have no common measure in the rules, so the gradual
// move by need stands and, while an enemy fleet is based on a Mediterranean sea, Italy's three Mediterranean beats keep
// at least a third of her boats between them (navy.json beats 'floor'). (2) Every tied fleet, in every campaign, loses
// its tie the month its home falls or once its beat is out of reach from the port it is in, and is then placed as any
// other fleet. (3) At stops 1 to 3 the Bordeaux flotilla's beat is the sea nearest Bordeaux that carries a British
// lane that month, its dated beat only where none in its reach does.
import { describe, expect, it } from 'vitest';
import { createGame, declareWar } from '../src/campaign/game';
import {
  NAVY, fleetReach, foeBasedOn, laneBeat, lanesTo, navalAI, navalUpkeep, placeBoats, rebase, seaRoutes, shippingIn, tieLapses, zoneOf,
} from '../src/campaign/navy';
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
/** Force H forms in July 1940 (turn 11), as its row has it. */
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
    // Tied, its admiral looked only at a beat it could not reach and kept it in harbour. Freed, he sends it where the
    // work is, as any other fleet: here against a German landing on the Channel coast.
    const channel = zoneOf(g, 'channel');
    const tied = (): void => { f.tied = true; };
    g.flags[`landed:${channel}:GER`] = g.turn;
    tied();
    navalAI(g, 'UK');
    expect(f.stance).toBe('port');
    f.tied = false;
    navalAI(g, 'UK');
    expect([key(g, f.zone), f.stance]).toEqual(['channel', 'patrol']);
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
});

describe('(3) the Bordeaux flotilla\'s beat at stops 1 to 3', () => {
  it('is the sea nearest Bordeaux that carries a British lane, every month it sails, and never an empty sea', () => {
    for (const stop of [1, 2, 3]) {
      const g = wide(stop);
      frenchFall(g);
      for (let turn = 13; turn <= 46; turn++) {
        g.turn = turn;
        placeBoats(g, 'ITA');
        const atl = named(g, 'Atlantic Submarine Fleet');
        if (!atl) continue;
        // The Bay of Biscay carries only France's lane; the Western Approaches, two seas out, Britain's.
        expect(key(g, atl.station), `stop ${stop} turn ${turn}`).toBe('wapp');
        expect(lanesTo(g, 'UK', atl.station), `stop ${stop} turn ${turn}`).toBeGreaterThan(0);
        expect(fleetReach(g, atl)).toContain(atl.station);
      }
      expect(boats(g, 'Atlantic Submarine Fleet'), `stop ${stop}`).toBeGreaterThan(0);
    }
  });

  it('counts from the seas Bordeaux opens on, and the Bay of Biscay\'s own lane is not Britain\'s', () => {
    const g = wide(1);
    frenchFall(g);
    expect(g.byName.Bordeaux.seas.map((z) => key(g, z))).toEqual(['biscay']);
    expect(lanesTo(g, 'UK', zoneOf(g, 'biscay'))).toBe(0);
    expect(lanesTo(g, 'FRA', zoneOf(g, 'biscay'))).toBe(1);
    const from = { nat: 'ITA', base: g.byName.Bordeaux.id, home: zoneOf(g, 'biscay') };
    expect(key(g, laneBeat(g, from, 'UK'))).toBe('wapp');
  });

  it('where no sea in its reach carries a British lane, it keeps its dated beat', () => {
    const g = wide(1);
    frenchFall(g);
    // At war with the United States, Britain's Atlantic lane carries nothing.
    declareWar(g, 'USA', 'UK');
    for (const turn of [16, 19]) {
      g.turn = turn;
      placeBoats(g, 'ITA');
      const atl = named(g, 'Atlantic Submarine Fleet')!;
      expect(key(g, atl.station), `turn ${turn}`).toBe(turn < 19 ? 'natl' : 'iberia');
    }
  });

  it('between seas as near: the one with more such lanes, then the first in the chart\'s order', () => {
    const g = wide(1);
    const sicily = { nat: 'GER', base: g.byName.Sicily.id, home: zoneOf(g, 'tyrr') };
    // Sicily opens on the Tyrrhenian, the Central Mediterranean and the Ionian: Italy's Gibraltar lane runs through the
    // first, her Libya convoys through the second, one each. The chart's order decides.
    expect(g.byName.Sicily.seas.map((z) => key(g, z))).toEqual(['tyrr', 'central', 'ionian']);
    expect(key(g, laneBeat(g, sicily, 'ITA'))).toBe('tyrr');
    // A second lane through the Central Mediterranean makes it the thicker.
    seaRoutes(g).push({ id: 'b598-test', name: 'a test lane', to: 'ITA', zones: [zoneOf(g, 'central')] });
    expect(lanesTo(g, 'ITA', zoneOf(g, 'central'))).toBe(2);
    expect(key(g, laneBeat(g, sicily, 'ITA'))).toBe('central');
    // A nearer sea wins over a thicker one further off.
    const naples = { nat: 'GER', base: g.byName.Naples.id, home: zoneOf(g, 'tyrr') };
    expect(g.byName.Naples.seas.map((z) => key(g, z))).toEqual(['tyrr']);
    expect(key(g, laneBeat(g, naples, 'ITA'))).toBe('tyrr');
    // With none in reach: -1.
    expect(laneBeat(g, sicily, 'SOV')).toBe(-1);
  });

  it('at stops 4 and 5 the beat is by need, as before', () => {
    const g = wide(4);
    frenchFall(g);
    declareWar(g, 'ITA', 'UK');
    g.turn = 13;
    placeBoats(g, 'ITA');
    const atl = named(g, 'Atlantic Submarine Fleet')!;
    // Need finds no sea thicker than the dated beat's: the North Atlantic, not the nearest.
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
