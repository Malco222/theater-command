// Italy's boats on the record's seas, and Force H at Gibraltar (ruling 233, rider b564; the historian's rows in
// historian/italian-boats-b562.md). In the grand campaign Italy's submarines are shared out among four beats by the
// historian's rows, as shares of the boats the fleet has after the game's own losses: the Atlantic from Bordeaux (only
// while the Axis holds it), and the Mediterranean's west, centre and east. At stops 4 and 5 the staff places them by
// need. Germany sends U-boats into the Mediterranean from the U-boat Fleet, by date at stops 1 and 2 and at stops 3
// to 5 only while an Axis army fights in North Africa. Force H is at home in Gibraltar with the Western Mediterranean
// as its beat, and at stops 1 and 2 its carriers come and go as the record has them. The history campaigns keep the
// war at sea they always had.
import { describe, expect, it } from 'vitest';
import navy from '../data/campaign/navy.json';
import { capitulate, createGame, declareWar, relocate } from '../src/campaign/game';
import { openEscorts } from '../src/campaign/fleets';
import { NAVY, armyInAfrica, closedTo, fleetReach, navalAI, navalUpkeep, placeBoats, shippingIn, wholeShares, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import brianSave from './ahead/saves/brian-usa-1940-12.json?raw';
import type { Fleet, Game } from '../src/campaign/types';

const ITALY = ['Atlantic Submarine Fleet', 'Submarine Fleet', 'Central Submarine Fleet', 'Eastern Submarine Fleet'];
const MED_UBOATS = ['Western Mediterranean U-boat Fleet', 'Eastern Mediterranean U-boat Fleet'];

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
/** Bordeaux is Germany's, as it was from June 1940. (France itself is left in the war: only the port is wanted here.) */
const frenchFall = (g: Game): void => { g.byName.Bordeaux.ctrl = 'GER'; };
/** Force H forms in July 1940 (turn 11), as its row has it. */
function forceH(g: Game): Fleet {
  g.turn = 11;
  navalUpkeep(g, 'UK');
  return named(g, 'Force H')!;
}

describe('the rows', () => {
  it('are the historian\'s, and every group is a fleet the chart can carry', () => {
    const B = NAVY.beats!;
    expect(B.ITA.rows.map((r) => r[0])).toEqual([10, 13, 14, 15, 19, 21, 24, 29, 39, 41, 45, 47]);
    expect(B.ITA.rows.find((r) => r[0] === 24)).toEqual([24, 14, 15, 30, 20]);
    expect(B.GER.rows).toEqual([[26, 0, 6], [28, 5, 15], [39, 10, 10], [53, 10, 3], [61, 0, 0]]);
    const g = wide();
    for (const nat of ['ITA', 'GER']) {
      for (const G of B[nat].groups) {
        expect(G.name, G.name).toMatch(/ Fleet$/);
        if (G.base) expect(g.byName[G.base], G.base).toBeDefined();
        if (G.home) expect(zoneOf(g, G.home), G.home).toBeGreaterThanOrEqual(0);
        if (G.zone) expect(zoneOf(g, G.zone), G.zone).toBeGreaterThanOrEqual(0);
      }
    }
    // The existing Italian fleet is the western beat, under the name saves and tests already know.
    expect(B.ITA.groups[1]).toEqual({ name: 'Submarine Fleet' });
    expect(B.GER.from).toBe('U-boat Fleet');
    for (const name of B.GER.cause!.ground) expect(g.byName[name], name).toBeDefined();
    // Force H's row is the old one, with the grand campaign's own beat beside it.
    const row = navy.fleets.find((f) => f.name === 'Force H')!;
    expect(row.home).toBe('iberia');
    expect((row as any).open).toEqual({ home: 'wmed', zone: 'wmed', tied: true });
  });

  it('share whole boats by largest remainder and keep the total exactly', () => {
    expect(wholeShares(100, [14, 15, 30, 20])).toEqual([18, 19, 38, 25]);
    expect(wholeShares(79, [14, 15, 30, 20])).toEqual([14, 15, 30, 20]);
    expect(wholeShares(60, [14, 15, 30, 20])).toEqual([11, 11, 23, 15]);
    expect(wholeShares(7, [0, 0])).toEqual([0, 0]);
    for (let n = 0; n < 120; n++) expect(sum(wholeShares(n, [11, 15, 30, 20]))).toBe(n);
  });
});

describe('Italy\'s boats by the historian\'s rows (stops 1 to 3)', () => {
  it('in August 1941 each beat takes its share of the boats the fleet has left after its losses', () => {
    const g = wide();
    frenchFall(g);
    g.turn = 24;
    named(g, 'Submarine Fleet')!.ships.dd = 2; // a transfer moves only submarines
    placeBoats(g, 'ITA');
    expect(italian(g)).toEqual([18, 19, 38, 25]);
    // The game's own losses stand: of 60 boats left, the same shares, and never a boat more.
    for (const name of ITALY) named(g, name)!.ships.ss = 0;
    named(g, 'Submarine Fleet')!.ships.ss = 60;
    placeBoats(g, 'ITA');
    expect(italian(g)).toEqual([11, 11, 23, 15]);
    expect(named(g, 'Submarine Fleet')!.ships.dd).toBe(2);
    // December 1941 is still the August row.
    g.turn = 28;
    placeBoats(g, 'ITA');
    expect(italian(g)).toEqual([11, 11, 23, 15]);
  });

  it('the groups form when they first have boats, with a line of news, and sail for their own beats', () => {
    const g = wide();
    g.turn = 9;
    placeBoats(g, 'ITA');
    expect(italian(g)).toEqual([0, 100, 0, 0]);
    g.turn = 10;
    placeBoats(g, 'ITA');
    expect(italian(g)).toEqual([0, 30, 40, 30]);
    expect(news(g, 10)).toContain('The Central Submarine Fleet forms at Augusta.');
    expect(news(g, 10)).toContain('The Eastern Submarine Fleet forms at Leros.');
    const centre = named(g, 'Central Submarine Fleet')!, east = named(g, 'Eastern Submarine Fleet')!;
    expect([g.regions[centre.base].name, g.seas[centre.station].key, centre.duty]).toEqual(['Sicily', 'central', 'raid']);
    expect([g.regions[east.base].name, g.seas[east.station].key, east.duty]).toEqual(['Rhodes', 'libyan', 'raid']);
    expect(fleetReach(g, centre)).toContain(centre.station);
    expect(fleetReach(g, east)).toContain(east.station);
    // A second month says nothing more.
    g.turn = 11;
    placeBoats(g, 'ITA');
    expect(news(g, 11).filter((t) => /Submarine Fleet forms/.test(t))).toEqual([]);
  });

  it('the Atlantic flotilla sails only while the Axis holds Bordeaux, and its boats come home when it is lost', () => {
    const g = wide();
    g.turn = 15;
    placeBoats(g, 'ITA');
    // Bordeaux is French: the Atlantic's count is left out and the Mediterranean shares everything.
    expect(named(g, 'Atlantic Submarine Fleet')).toBeUndefined();
    expect(italian(g)).toEqual([0, 25, 42, 33]);
    frenchFall(g);
    g.turn = 16;
    placeBoats(g, 'ITA');
    expect(news(g, 16)).toContain('The Atlantic Submarine Fleet forms at Bordeaux.');
    const atl = named(g, 'Atlantic Submarine Fleet')!;
    expect(g.regions[atl.base].name).toBe('Bordeaux');
    expect(g.seas[atl.home].key).toBe('biscay');
    // Its beat is read from the record's seas by half-year (riders b601 and b603): to February 1941 the one of the
    // Western Approaches and the North Atlantic with the most British lane, the first of them where both carry one;
    // from March 1941, when the record's boats went south where the chart carries no British lane, the nearest sea to
    // those that does. The Western Approaches both times, never the Iberian Coast its dated beat would give.
    expect(g.seas[atl.station].key).toBe('wapp');
    expect(italian(g)).toEqual([30, 18, 29, 23]);
    g.turn = 19;
    placeBoats(g, 'ITA');
    expect(g.seas[atl.station].key).toBe('wapp');
    expect(fleetReach(g, atl)).toContain(atl.station);
    // Bordeaux falls to the Allies: the flotilla stands down and the Mediterranean has its boats again.
    g.byName.Bordeaux.ctrl = 'UK';
    g.turn = 20;
    placeBoats(g, 'ITA');
    expect(named(g, 'Atlantic Submarine Fleet')).toBeUndefined();
    expect(sum(italian(g))).toBe(100);
    expect(italian(g)).toEqual([0, 25, 42, 33]);
    // Retaken, it forms again.
    frenchFall(g);
    g.turn = 21;
    placeBoats(g, 'ITA');
    expect(boats(g, 'Atlantic Submarine Fleet')).toBe(31);
    expect(news(g, 21)).toContain('The Atlantic Submarine Fleet forms at Bordeaux.');
  });

  it('after the last row its shares hold, and the Atlantic has none', () => {
    const g = wide();
    frenchFall(g);
    g.turn = 60;
    placeBoats(g, 'ITA');
    expect(italian(g)).toEqual([0, 30, 60, 10]);
  });
});

describe('Italy\'s boats by need (stops 4 and 5)', () => {
  it('with no enemy shipping in reach, the boats are placed as the war opened', () => {
    for (const stop of [4, 5]) {
      const g = wide(stop);
      frenchFall(g);
      g.turn = 28;
      placeBoats(g, 'ITA');
      expect(italian(g), `stop ${stop}`).toEqual([0, 30, 40, 30]);
    }
  });

  it('at war with Britain, the boats move towards where her cargoes sail, the Atlantic while the Axis holds Bordeaux, about a fifth of the way in a year', () => {
    const g = wide(4);
    frenchFall(g);
    declareWar(g, 'ITA', 'UK');
    g.turn = 13;
    placeBoats(g, 'ITA');
    // The first month starts from where the war opened them and goes a sixtieth of the way.
    expect(italian(g)).toEqual([2, 30, 39, 29]);
    const atl = named(g, 'Atlantic Submarine Fleet')!;
    expect(shippingIn(g, 'ITA', atl.station)).toBeGreaterThan(0);
    expect(fleetReach(g, atl)).toContain(atl.station);
    // The chart's Mediterranean carries no enemy lane: the beats there have none of the enemy's cargoes to hunt.
    for (const key of ['wmed', 'central', 'libyan']) expect(shippingIn(g, 'ITA', zoneOf(g, key)), key).toBe(0);
    // A year on, about a fifth of the boats are in the Atlantic and the Mediterranean's beats keep the rest: never
    // a hundred boats on one beat.
    for (let turn = 14; turn <= 25; turn++) {
      g.turn = turn;
      placeBoats(g, 'ITA');
    }
    expect(sum(italian(g))).toBe(100);
    expect(boats(g, 'Atlantic Submarine Fleet')).toBe(17);
    for (const name of ITALY.slice(1)) expect(boats(g, name), name).toBeGreaterThan(20);
    // Without Bordeaux there is no enemy shipping in Italy's reach: the flotilla's boats go to the Mediterranean at
    // once, and the beats there move back towards the opening shares.
    g.byName.Bordeaux.ctrl = 'UK';
    g.turn = 26;
    placeBoats(g, 'ITA');
    expect(named(g, 'Atlantic Submarine Fleet')).toBeUndefined();
    expect(sum(italian(g))).toBe(100);
  });

  it('a staff at war before June 1940 places its boats at once, and only by need', () => {
    for (const stop of [4, 5]) {
      const g = wide(stop);
      declareWar(g, 'ITA', 'UK');
      g.turn = 6;
      placeBoats(g, 'ITA');
      expect(sum(italian(g)), `stop ${stop}`).toBe(100);
      expect(boats(g, 'Central Submarine Fleet'), `stop ${stop}`).toBeGreaterThan(30);
      expect(boats(g, 'Eastern Submarine Fleet'), `stop ${stop}`).toBeGreaterThan(20);
    }
    // By the record the rows begin in June 1940, whatever the war; at peace no staff moves them before then.
    for (const [stop, war] of [[3, true], [4, false]] as const) {
      const g = wide(stop);
      if (war) declareWar(g, 'ITA', 'UK');
      g.turn = 6;
      placeBoats(g, 'ITA');
      expect(italian(g), `stop ${stop}`).toEqual([0, 100, 0, 0]);
    }
  });

  it('at stop 3 the same war keeps to the historian\'s rows', () => {
    const g = wide(3);
    frenchFall(g);
    declareWar(g, 'ITA', 'UK');
    g.turn = 28;
    placeBoats(g, 'ITA');
    expect(italian(g)).toEqual([18, 19, 38, 25]);
  });
});

describe('Germany\'s U-boats in the Mediterranean', () => {
  it('at stops 1 and 2 they go by date, taken from the U-boat Fleet up to as many as it has, and come back', () => {
    for (const stop of [1, 2]) {
      const g = wide(stop);
      const pool = named(g, 'U-boat Fleet')!;
      pool.ships.ss = 40;
      const med = (): number[] => MED_UBOATS.map((name) => boats(g, name));
      g.turn = 25;
      placeBoats(g, 'GER');
      expect(med()).toEqual([0, 0]);
      g.turn = 26;
      placeBoats(g, 'GER');
      expect(med()).toEqual([0, 6]);
      expect(news(g, 26)).toContain('The Eastern Mediterranean U-boat Fleet forms at Leros.');
      g.turn = 28;
      placeBoats(g, 'GER');
      expect(med()).toEqual([5, 15]);
      expect(pool.ships.ss).toBe(20);
      expect(news(g, 28)).toContain('The Western Mediterranean U-boat Fleet forms at La Spezia.');
      const west = named(g, MED_UBOATS[0])!;
      expect([g.regions[west.base].name, g.seas[west.station].key]).toEqual(['Liguria', 'wmed']);
      g.turn = 39;
      placeBoats(g, 'GER');
      expect(med()).toEqual([10, 10]);
      // Play's losses are its own: what the Mediterranean lost is not made good from the Atlantic beyond the row.
      g.turn = 53;
      named(g, MED_UBOATS[1])!.ships.ss = 1;
      placeBoats(g, 'GER');
      expect(med()).toEqual([10, 3]);
      expect(pool.ships.ss + sum(med())).toBe(31);
      g.turn = 61;
      placeBoats(g, 'GER');
      expect(med()).toEqual([0, 0]);
      expect(pool.ships.ss).toBe(31);
      // With none left in the Mediterranean the two groups stand down.
      expect(g.fleets.some((f) => MED_UBOATS.includes(f.name))).toBe(false);
    }
  });

  it('lent boats whose Italian base is lost go home to the U-boat Fleet, and none is lost by it', () => {
    const g = wide(1);
    named(g, 'U-boat Fleet')!.ships.ss = 100;
    g.turn = 28;
    placeBoats(g, 'GER');
    const german = (): number => g.fleets.filter((f) => f.nat === 'GER').reduce((n, f) => n + f.ships.ss, 0);
    const before = german();
    // Rhodes falls: Germany holds no port of its own in the Mediterranean to send the eastern group to.
    g.byName.Rhodes.ctrl = 'UK';
    g.turn = 29;
    navalUpkeep(g, 'GER');
    expect(named(g, MED_UBOATS[1])).toBeUndefined();
    expect(boats(g, MED_UBOATS[0])).toBe(5);
    expect(news(g, 29).some((t) => /scuttled/.test(t))).toBe(false);
    // (New boats join the U-boat Fleet every month: the count can only grow.)
    expect(german()).toBeGreaterThanOrEqual(before);
    // Retaken, the group forms there again.
    g.byName.Rhodes.ctrl = 'ITA';
    g.turn = 30;
    placeBoats(g, 'GER');
    expect(boats(g, MED_UBOATS[1])).toBe(15);
    expect(news(g, 30)).toContain('The Eastern Mediterranean U-boat Fleet forms at Leros.');
  });

  it('a short U-boat Fleet sends what it has, in the row\'s proportion', () => {
    const g = wide(1);
    named(g, 'U-boat Fleet')!.ships.ss = 8;
    g.turn = 28;
    placeBoats(g, 'GER');
    expect(MED_UBOATS.map((name) => boats(g, name))).toEqual([2, 6]);
    expect(boats(g, 'U-boat Fleet')).toBe(0);
  });

  it('at stops 3 to 5 they go only while an Axis army fights in North Africa', () => {
    for (const stop of [3, 4, 5]) {
      const g = wide(stop);
      const cause = NAVY.beats!.GER.cause!;
      g.turn = 26;
      // Italy's army stands in Libya, but Italy is not at war: no cause.
      expect(armyInAfrica(g, cause)).toBe(false);
      placeBoats(g, 'GER');
      expect(boats(g, MED_UBOATS[1])).toBe(0);
      declareWar(g, 'ITA', 'UK');
      expect(armyInAfrica(g, cause)).toBe(true);
      placeBoats(g, 'GER');
      expect(boats(g, MED_UBOATS[1])).toBe(6);
      // Before October 1941 the cause does not hold, whatever the army does.
      g.turn = 25;
      expect(armyInAfrica(g, cause)).toBe(false);
      // The Axis armies leave Africa: the U-boats go back to the U-boat Fleet.
      g.turn = 28;
      const rome = g.byName.Rome;
      const ground = new Set(cause.ground.map((name) => g.byName[name].id));
      for (const c of g.corps) if (!c.dead && cause.by.includes(c.nat) && ground.has(c.reg)) relocate(g, c, rome);
      expect(armyInAfrica(g, cause)).toBe(false);
      const before = boats(g, 'U-boat Fleet');
      placeBoats(g, 'GER');
      expect(MED_UBOATS.map((name) => boats(g, name)), `stop ${stop}`).toEqual([0, 0]);
      expect(boats(g, 'U-boat Fleet'), `stop ${stop}`).toBe(before + 6);
    }
  });
});

describe('Force H', () => {
  it('in the grand campaign is at home in Gibraltar with the Western Mediterranean as its beat, and keeps to it', () => {
    const g = wide();
    const f = forceH(g);
    expect([g.regions[f.base].name, g.seas[f.home].key, g.seas[f.station].key, f.tied, f.duty]).toEqual(['Gibraltar', 'wmed', 'wmed', true, 'patrol']);
    expect(f.ships).toEqual({ bb: 2, cv: 1, ca: 2, dd: 9, ss: 0 });
    navalAI(g, 'UK');
    expect([g.seas[f.zone].key, f.stance]).toEqual(['wmed', 'patrol']);
  });

  it('in a history campaign keeps the row it always had', () => {
    const g = createGame(1, null);
    const f = forceH(g);
    expect([g.regions[f.base].name, g.seas[f.home].key, g.seas[f.station].key, f.tied]).toEqual(['Gibraltar', 'iberia', 'iberia', false]);
  });

  it('a grand campaign begun later takes the beat over from the history it was played on', () => {
    const g = createGame(1, null);
    const f = forceH(g);
    g.flags.open = 1;
    openEscorts(g);
    expect([g.seas[f.home].key, g.seas[f.zone].key, g.seas[f.station].key, f.tied]).toEqual(['wmed', 'wmed', 'wmed', true]);
  });

  it('a grand campaign saved before the rule takes the beat over when it is loaded, once', () => {
    // A save from before the rule: the United States, December 1940, with Force H on the Iberian Coast.
    const old = loadGame(brianSave)!;
    const f = named(old, 'Force H')!;
    expect([old.seas[f.home].key, old.seas[f.station].key, f.tied]).toEqual(['wmed', 'wmed', true]);
    // Sent to another beat since, it keeps that beat when the game is loaded again.
    f.station = zoneOf(old, 'iberia');
    const again = loadGame(saveGame(old))!;
    expect(again.seas[named(again, 'Force H')!.station].key).toBe('iberia');
    // A history campaign's save is left as it was.
    const hist = createGame(1, null);
    forceH(hist);
    expect(hist.seas[named(loadGame(saveGame(hist))!, 'Force H')!.station].key).toBe('iberia');
  });

  it('at stops 1 and 2 keeps the Ark Royal until play sinks her: the Eagle waits for that, and no row sinks a carrier', () => {
    // (The record's Ark Royal of November 1941 and Eagle of August 1942 were sunk in battle, and the battle decides:
    // the director's note of 08:38Z. b598.test.ts (5) has the carriers' rows month by month.)
    for (const stop of [1, 2]) {
      const g = wide(stop);
      const f = forceH(g);
      const cv: Record<number, number> = {};
      for (let turn = 12; turn <= 37; turn++) {
        g.turn = turn;
        navalUpkeep(g, 'UK');
        cv[turn] = f.ships.cv;
      }
      expect([cv[28], cv[31], cv[36], cv[37]]).toEqual([1, 1, 3, 1]);
      expect(f.lost.cv).toBe(0);
      expect(g.headlines.some((h) => /sunk|Eagle joins Force H/.test(h.text))).toBe(false);
      // Its row's capital ships, cruisers and destroyers: the two destroyers its book brings in July 1940 leave in
      // September (the director's notes of 10:06Z and 11:21Z; b598.test.ts (6) has its first months).
      expect([f.ships.bb, f.ships.ca, f.ships.dd]).toEqual([2, 2, 9]);
    }
  });

  it('in a grand campaign begun in January 1942 at stop 1 reads the rows of before without news, and keeps the Ark Royal', () => {
    const g = createGame(1, null);
    // History is played to the month the grand campaign begins, and the record's rows were not read in it.
    for (let turn = 2; turn <= 28; turn++) {
      g.turn = turn;
      navalUpkeep(g, 'UK');
    }
    const f = named(g, 'Force H')!;
    g.turn = 29;
    startOpen(g);
    g.flags.hist = 1;
    navalUpkeep(g, 'UK');
    // The Furious's ferry run of May and June 1941 has come and gone: Force H has its one carrier, and lost none.
    expect([f.ships.cv, f.lost.cv]).toEqual([1, 0]);
    expect(named(g, 'Home Fleet')!.ships.cv).toBe(3);
    // Their months are past: they are not news.
    expect(news(g, 29).some((t) => /carrier/.test(t))).toBe(false);
    g.turn = 30;
    navalUpkeep(g, 'UK');
    expect([f.ships.cv, f.lost.cv]).toEqual([1, 0]);
    g.turn = 36;
    navalUpkeep(g, 'UK');
    expect(f.ships.cv).toBe(3);
    expect(news(g, 36)).toContain('The carriers Victorious and Furious leave the Home Fleet for Force H.');
  });

  it('a dial turned to the book later brings Force H to the record\'s ships of that month, once', () => {
    const g = wide(3);
    const f = forceH(g);
    for (let turn = 12; turn <= 33; turn++) {
      g.turn = turn;
      navalUpkeep(g, 'UK');
    }
    expect(f.ships.cv).toBe(1);
    g.flags.hist = 1;
    g.turn = 34;
    navalUpkeep(g, 'UK');
    // The ferry run come and gone without news: one carrier, and none lost.
    expect([f.ships.cv, f.lost.cv]).toEqual([1, 0]);
    expect(news(g, 34).some((t) => /carrier/.test(t))).toBe(false);
    g.turn = 35;
    navalUpkeep(g, 'UK');
    expect(f.ships.cv).toBe(1);
    g.turn = 36;
    navalUpkeep(g, 'UK');
    expect(f.ships.cv).toBe(3);
    g.turn = 37;
    navalUpkeep(g, 'UK');
    expect([f.ships.cv, f.lost.cv]).toEqual([1, 0]);
  });

  it('at stops 3 to 5, in a history campaign and under a player, its carrier goes only if play sinks it', () => {
    const games = [wide(3), wide(4), wide(5), createGame(1, null), wide(1, 'UK')];
    for (const g of games) {
      const f = forceH(g);
      for (const turn of [28, 31, 36]) {
        g.turn = turn;
        navalUpkeep(g, 'UK');
        expect(f.ships.cv).toBe(1);
      }
      expect(f.lost.cv).toBe(0);
      expect(g.headlines.some((h) => /Ark Royal|Eagle/.test(h.text))).toBe(false);
    }
  });

  it('a carrier play has sunk is made good by the Eagle in March 1942, once', () => {
    const g = wide(1);
    const f = forceH(g);
    f.ships.cv = 0;
    f.lost.cv = 1;
    for (const [turn, cv] of [[28, 0], [29, 0], [30, 1], [31, 1]]) {
      g.turn = turn;
      navalUpkeep(g, 'UK');
      expect(f.ships.cv, `turn ${turn}`).toBe(cv);
    }
    expect(news(g, 30)).toContain('The carrier Eagle joins Force H.');
    expect(f.lost.cv).toBe(1);
  });
});

describe('what is left alone', () => {
  it('a history campaign\'s boats stay where they always were', () => {
    const g = createGame(1, null);
    frenchFall(g);
    for (const turn of [10, 15, 28, 40]) {
      g.turn = turn;
      placeBoats(g, 'ITA');
      placeBoats(g, 'GER');
    }
    expect(italian(g)).toEqual([0, 100, 0, 0]);
    expect(boats(g, 'U-boat Fleet')).toBe(27);
    expect(g.fleets.some((f) => MED_UBOATS.includes(f.name))).toBe(false);
  });

  it('the player\'s own navy is his to place', () => {
    const ita = wide(1, 'ITA');
    frenchFall(ita);
    ita.turn = 28;
    placeBoats(ita, 'ITA');
    expect(italian(ita)).toEqual([0, 100, 0, 0]);
    const ger = wide(1, 'GER');
    ger.turn = 28;
    placeBoats(ger, 'GER');
    expect(boats(ger, 'U-boat Fleet')).toBe(27);
    expect(ger.fleets.some((f) => MED_UBOATS.includes(f.name))).toBe(false);
  });
});

describe('the director\'s check: December 1941', () => {
  /** December 1941 at stop 1: France has fallen, Italy is at war with Britain, and the U-boat Fleet has boats to spare. */
  function december(): { g: Game; f: Fleet } {
    const g = wide(1);
    const f = forceH(g);
    capitulate(g, 'FRA', 'GER');
    declareWar(g, 'ITA', 'UK');
    g.turn = 28;
    named(g, 'U-boat Fleet')!.ships.ss = 100;
    return { g, f };
  }

  it('the Western Mediterranean holds about 15 Italian and 5 German boats with Force H on it, and stays open to Britain', () => {
    const { g, f } = december();
    expect(g.byName.Bordeaux.ctrl).toBe('GER');
    expect(g.fleets.some((x) => x.nat === 'FRA')).toBe(false);
    // The record's own total for Italy at that date (79 boats, net of its losses).
    named(g, 'Submarine Fleet')!.ships.ss = 79;
    placeBoats(g, 'ITA');
    placeBoats(g, 'GER');
    for (const nat of ['ITA', 'GER', 'UK']) navalAI(g, nat);
    const wmed = zoneOf(g, 'wmed');
    const inWmed = (nat: string): number => g.fleets.filter((x) => x.nat === nat && x.zone === wmed && x.stance !== 'port').reduce((n, x) => n + x.ships.ss, 0);
    expect(inWmed('ITA')).toBe(15);
    expect(inWmed('GER')).toBe(5);
    expect([f.zone, f.stance]).toEqual([wmed, 'patrol']);
    expect(closedTo(g, wmed, 'UK')).toBe(false);
    // The same month on the old single beat, every Italian boat on the Western Mediterranean and Force H on the
    // Iberian Coast's row, shuts the sea to Britain: it is the rule that keeps it open, not France's fleet.
    const old = december();
    old.f.home = old.f.zone = old.f.station = zoneOf(old.g, 'iberia');
    old.f.tied = false;
    named(old.g, 'Submarine Fleet')!.ships.ss = 100;
    for (const nat of ['ITA', 'GER', 'UK']) navalAI(old.g, nat);
    expect(closedTo(old.g, wmed, 'UK')).toBe(true);
    // Of a full fleet of 100 the western beat has 19, still near the record's 15.
    for (const name of ITALY) named(g, name)!.ships.ss = name === 'Submarine Fleet' ? 100 : 0;
    placeBoats(g, 'ITA');
    expect(boats(g, 'Submarine Fleet')).toBe(19);
  });
});
