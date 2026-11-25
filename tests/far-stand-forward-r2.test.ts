// A fleet that stands forward (release 2, the far chart: the design director's ruling of 25 November 2026 18:14Z). A
// fleet whose country is at war, with no enemy sea, coast or lane in its reach, makes a voyage to the nearest friendly
// harbour from which one is, priced as any base move (voyageMonths), and never to a harbour the enemy could strike
// from next month (the withdrawal's own test, strikeThreat), so no fleet advances to a harbour it would leave the
// month after. One rule for every nation; none off the far chart. Bounded at 18:23Z and 18:28Z, by cause: no farther
// than one month's voyage; the fewest months first, then the most of the enemy in reach, then the fewer seas out. Held
// at 18:35Z behind the balance work's stand-to (joinStandTo): it acts only where that has joined and is switched on,
// and reads a power stood to against as an enemy. These tests join a stand-in for it. For the join (19:13Z): never a
// fleet its staff or the book holds to a named harbour while that order stands (the striking force's gathering, a cover
// base, the book's station at stops 1 and 2); at peace a stand-to stands a fleet forward, its one order of the month;
// a player's fleets only where his navy desk is handed to his admirals.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { setDesk } from '../src/campaign/depth';
import { addCorps, atWar, createGame, declareWar } from '../src/campaign/game';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
import { setStop } from '../src/campaign/history';
import { km } from '../src/campaign/air';
import { enemyInReach, foeInReach, foesOf, forwardHarbour, foundFleet, holdTo, inPassage, joinStandTo, navalAI, outOfStrikes, planCrossing, rebase, strikeThreat, voyageMonths } from '../src/campaign/navy';
import { gatherMonth } from '../src/campaign/readiness';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game } from '../src/campaign/types';

const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const at = (g: Game, f: Fleet): string => g.regions[f.base].name;

/** A computer world at its opening (September 1939), the dial at its free stop, America's Pacific Fleet at San Diego. */
function opening(): { g: Game; pf: Fleet } {
  const g = startOpen(createGame(3, null));
  setStop(g, 3, true);
  const pf = fleet(g, 'USA', 'Pacific Fleet');
  expect(at(g, pf)).toBe('San Diego');
  return { g, pf };
}

/** A stand-in for balance's stand-to at the join: switched on or off, and the powers each country stands to against. */
const standTo: { on: boolean; against: Record<string, string[]> } = { on: true, against: {} };
beforeAll(() => joinStandTo(() => standTo.on, (_g, nat) => standTo.against[nat] ?? []));
afterAll(() => joinStandTo(() => false, () => []));

describe('held behind the stand-to join', () => {
  it('with balance switched off, or not yet joined, no fleet stands forward', () => {
    const { g, pf } = opening();
    declareWar(g, 'JAP', 'USA');
    expect(forwardHarbour(g, pf)?.name).toBe('Bristol Bay');
    standTo.on = false;
    try {
      expect(forwardHarbour(g, pf)).toBeNull();
      const from = g.headlines.length;
      navalAI(g, 'USA');
      expect(pf.port).toBe('San Diego');
      expect(g.headlines.slice(from).some((h) => /no enemy lies within its reach/.test(h.text))).toBe(false);
    } finally { standTo.on = true; }
    joinStandTo(() => false, () => []);
    try { expect(forwardHarbour(g, pf)).toBeNull(); } finally { joinStandTo(() => standTo.on, (_g, nat) => standTo.against[nat] ?? []); }
  }, 300000);

  it('a power stood to against reads as an enemy: at war with Germany alone, standing to against Japan, the Pacific Fleet holds to the Pacific', () => {
    const { g, pf } = opening();
    if (!g.nations.USA.atWar.includes('GER')) declareWar(g, 'GER', 'USA');
    expect(atWar(g, 'USA', 'JAP')).toBe(false);
    // (Reading Germany alone, it would make for no Pacific harbour: in the 24 wars of 18:28Z, Cayenne or the Falklands.)
    expect(forwardHarbour(g, pf)?.name).not.toBe('Bristol Bay');
    standTo.against = { USA: ['JAP'] };
    try {
      expect(foesOf(g, 'USA')).toEqual(expect.arrayContaining(['GER', 'JAP']));
      expect(forwardHarbour(g, pf)?.name).toBe('Bristol Bay');
      standTo.on = false;
      expect(foesOf(g, 'USA')).not.toContain('JAP');
    } finally { standTo.against = {}; standTo.on = true; }
  }, 300000);
});

describe('a fleet with no enemy in its reach stands forward', () => {
  it('at peace, standing to against Japan, the Pacific Fleet stands forward: its one order of the month, and the next month it lies in harbour there', () => {
    const { g, pf } = opening();
    expect(g.nations.USA.atWar.length).toBe(0);
    standTo.against = { USA: ['JAP'] };
    try {
      const from = g.headlines.length;
      navalAI(g, 'USA');
      expect(pf.port).toBe('Bristol Bay');
      expect(g.headlines.slice(from).some((h) => h.text === 'The Pacific Fleet moves its base from San Diego to Bristol Bay: no enemy lies within its reach from San Diego.')).toBe(true);
      g.turn++;
      navalAI(g, 'USA');
      expect([pf.port, pf.stance]).toEqual(['Bristol Bay', 'port']);
      // (With the stand-to ended, at peace it lies in harbour where it is: never sent back by this rule.)
      standTo.against = {};
      g.turn++;
      navalAI(g, 'USA');
      expect(pf.port).toBe('Bristol Bay');
    } finally { standTo.against = {}; }
  }, 300000);

  it('at peace, a fleet stays where it is', () => {
    const { g, pf } = opening();
    expect(atWar(g, 'USA', 'JAP')).toBe(false);
    expect(forwardHarbour(g, pf)).toBeNull();
  }, 300000);

  it("at war with Japan, the Pacific Fleet moves its base from San Diego to the harbour nearest Japan within a month's voyage, with news", () => {
    const { g, pf } = opening();
    declareWar(g, 'JAP', 'USA');
    expect(foeInReach(g, pf, g.byName['San Diego'])).toBe(false);
    const to = forwardHarbour(g, pf)!;
    // (Bristol Bay, on the Gulf of Alaska and the Aleutian Waters, has four Japanese seas in its reach: the Kuriles, the
    // Sea of Okhotsk, the Honshu coast and the North-West Pacific. Pearl Harbor, as near in months, has three.)
    expect(to.name).toBe('Bristol Bay');
    const pearl = g.byName['Pearl Harbor'];
    expect(voyageMonths(g, pf, to)).toBe(voyageMonths(g, pf, pearl));
    expect([enemyInReach(g, pf, to).seas, enemyInReach(g, pf, pearl).seas]).toEqual([4, 3]);
    // (Wake, with more of Japan in reach, is a month's voyage: a month's passage is paid only when nothing nearer reaches the war.)
    const wake = g.byName['Wake'];
    expect(enemyInReach(g, pf, wake).seas).toBeGreaterThan(4);
    expect(voyageMonths(g, pf, wake)).toBeGreaterThan(voyageMonths(g, pf, to));
    expect(foeInReach(g, pf, to)).toBe(true);
    expect(outOfStrikes(g, to, strikeThreat(g, pf))).toBe(true);
    const from = g.headlines.length;
    navalAI(g, 'USA');
    expect(pf.port).toBe('Bristol Bay');
    expect(g.headlines.slice(from).some((h) => h.text === 'The Pacific Fleet moves its base from San Diego to Bristol Bay: no enemy lies within its reach from San Diego.')).toBe(true);
    // (There, with Japan in its reach, it stands forward no further the next month.)
    g.turn++;
    expect(forwardHarbour(g, pf)).toBeNull();
  }, 300000);

  it("no farther than a month's voyage: the Australian Fleet, with no German in reach of Sydney, stays", () => {
    const { g } = opening();
    const au = fleet(g, 'AUS', 'Australian Fleet');
    expect(at(g, au)).toBe('Sydney');
    expect(g.nations.AUS.atWar).toContain('GER');
    expect(foeInReach(g, au, g.byName['Sydney'])).toBe(false);
    // (A harbour with Germans in reach there is, two months' voyage away and more: beyond the bound.)
    const gib = g.byName['Gibraltar'];
    expect(enemyInReach(g, au, gib).seas).toBeGreaterThan(0);
    expect(voyageMonths(g, au, gib)).toBeGreaterThan(1);
    expect(forwardHarbour(g, au)).toBeNull();
  }, 300000);

  it('a fleet with an enemy in its reach stays', () => {
    const { g, pf } = opening();
    declareWar(g, 'JAP', 'USA');
    expect(rebase(g, pf, g.byName['Pearl Harbor'], true)).toBe(true);
    expect(foeInReach(g, pf, g.byName['Pearl Harbor'])).toBe(true);
    expect(forwardHarbour(g, pf)).toBeNull();
  }, 300000);

  it("never to a harbour the enemy could strike from next month", () => {
    const { g, pf } = opening();
    declareWar(g, 'JAP', 'USA');
    // (Struck at anchor at San Diego from a Japanese field whose bombers reach Bristol Bay, and not Pearl Harbor.)
    const kushiro = g.byName['Kushiro'], bay = g.byName['Bristol Bay'], pearl = g.byName['Pearl Harbor'];
    expect(kushiro.ctrl).toBe('JAP');
    const R = km(g, kushiro.id, bay.id) + 50;
    expect(km(g, kushiro.id, pearl.id)).toBeGreaterThan(R);
    pf.anchor = [{ t: g.turn, at: pf.base, n: 1, lost: 0.05, w: 100, from: [{ sea: -1, field: kushiro.id, km: R, nat: 'JAP' }] }];
    const T = strikeThreat(g, pf);
    expect(outOfStrikes(g, bay, T)).toBe(false);
    const to = forwardHarbour(g, pf)!;
    expect(to).not.toBeNull();
    expect(to.name).not.toBe('Bristol Bay');
    expect(outOfStrikes(g, to, T)).toBe(true);
  }, 300000);

  it('one rule for every nation: where any fleet at war stands forward, its harbour has an enemy in reach and is out of strikes', () => {
    const { g } = opening();
    declareWar(g, 'JAP', 'USA');
    let moves = 0;
    const nations = new Set<string>();
    for (const f of g.fleets) {
      if (!g.nations[f.nat]?.atWar.length || f.base < 0) continue;
      const to = forwardHarbour(g, f);
      if (!to) continue;
      moves++;
      nations.add(f.nat);
      expect(foeInReach(g, f, g.regions[f.base])).toBe(false);
      expect(foeInReach(g, f, to)).toBe(true);
      expect(outOfStrikes(g, to, strikeThreat(g, f))).toBe(true);
      expect(voyageMonths(g, f, to)).toBeLessThanOrEqual(1);
    }
    expect(moves).toBeGreaterThan(0);
    expect(nations.size).toBeGreaterThan(1);
  }, 300000);

  it("a month's passage is paid where nothing nearer reaches the war: from New Orleans the Pacific Fleet sails for Hawaii", () => {
    const { g, pf } = opening();
    declareWar(g, 'JAP', 'USA');
    expect(rebase(g, pf, g.byName['New Orleans'], true)).toBe(true);
    const to = forwardHarbour(g, pf)!;
    expect(to.name).toBe('Hawaii');
    expect(voyageMonths(g, pf, to)).toBe(1);
    const from = g.headlines.length;
    navalAI(g, 'USA');
    expect([pf.port, inPassage(pf)]).toEqual(['Hawaii', true]);
    expect(g.headlines.slice(from).some((h) => h.text === 'The Pacific Fleet moves its base from New Orleans to Hawaii: no enemy lies within its reach from New Orleans.')).toBe(true);
    // (In passage it stands forward no further.)
    g.turn++;
    expect(forwardHarbour(g, pf)).toBeNull();
  }, 300000);

  it('of harbours as near with as much of the enemy in reach, the one fewer seas out: Italy, standing to against the Soviet Union', () => {
    const { g } = opening();
    const it2 = fleet(g, 'ITA', '2nd Fleet');
    expect(at(g, it2)).toBe('Naples');
    standTo.against = { ITA: ['SOV'] };
    try {
      const rhodes = g.byName['Rhodes'], apulia = g.byName['Apulia'];
      expect(voyageMonths(g, it2, rhodes)).toBe(voyageMonths(g, it2, apulia));
      expect(enemyInReach(g, it2, rhodes).seas).toBe(enemyInReach(g, it2, apulia).seas);
      expect([enemyInReach(g, it2, rhodes).out, enemyInReach(g, it2, apulia).out]).toEqual([2, 3]);
      expect(forwardHarbour(g, it2)?.name).toBe('Rhodes');
    } finally { standTo.against = {}; }
  }, 300000);

  it('an enemy fleet its side knows of is the enemy in reach: the French Mediterranean Fleet at Marseille, and the German fleets gone', () => {
    const { g } = opening();
    const fr = fleet(g, 'FRA', 'Mediterranean Fleet'), home = g.regions[fr.base];
    expect(home.name).toBe('Marseille');
    expect(foeInReach(g, fr, home)).toBe(true);
    expect(forwardHarbour(g, fr)).toBeNull();
    for (const x of g.fleets) if (x.nat === 'GER') x.zone = -1;
    expect(foeInReach(g, fr, home)).toBe(false);
    expect(forwardHarbour(g, fr)).not.toBeNull();
  }, 300000);

  it("at stops 1 and 2 the book's station is the leash: a fleet the book stations stays, one it stations nowhere stands forward", () => {
    const { g, pf } = opening();
    declareWar(g, 'JAP', 'USA');
    const sq = foundFleet(g, 'USA', g.byName['San Diego'], 'Test Squadron');
    expect(forwardHarbour(g, pf)?.name).toBe('Bristol Bay');
    for (const stop of [1, 2]) {
      setStop(g, stop, true);
      expect(forwardHarbour(g, pf), `stop ${stop}`).toBeNull();
      expect(forwardHarbour(g, sq)?.name, `stop ${stop}`).toBe('Bristol Bay');
    }
  }, 300000);

  it('off the far chart, no fleet stands forward', () => {
    const { g, pf } = opening();
    declareWar(g, 'JAP', 'USA');
    expect(forwardHarbour(g, pf)).not.toBeNull();
    g.flags.world = 0;
    expect(forwardHarbour(g, pf)).toBeNull();
  }, 300000);
});

describe('a fleet its staff holds to a named harbour stays there', () => {
  it('held this month, the Pacific Fleet does not stand forward; the next month it does', () => {
    const { g, pf } = opening();
    declareWar(g, 'JAP', 'USA');
    holdTo(g, pf);
    expect(forwardHarbour(g, pf)).toBeNull();
    g.turn++;
    expect(forwardHarbour(g, pf)?.name).toBe('Bristol Bay');
  }, 300000);

  it("the striking force his admirals gather at its anchorage is not taken away by standing forward (Units decide audit item 44)", () => {
    const g = startOpen(createGame(17, 'JAP'));
    setDesk(g, 'navy', true);
    g.turn = 25;
    g.flags['pac:freeze'] = 23;
    // (The crossing he has named, and a corps of his on its quay: his admirals gather the striking force.)
    let named = false;
    for (const from of g.regions.filter((r) => r.port && r.ctrl === 'JAP' && r.owner === 'JAP')) {
      for (const to of g.regions.filter((r) => r.seas.length > 0 && atWar(g, 'JAP', r.ctrl))) {
        if (!planCrossing(g, 'JAP', from, to)) continue;
        addCorps(g, 'JAP', 'Quay Corps', 'inf', 1, from.id, {});
        named = true;
        break;
      }
      if (named) break;
    }
    expect(named).toBe(true);
    const mf = fleet(g, 'JAP', 'Mobile Fleet');
    gatherMonth(g, 'JAP');
    const anchorage = g.regions[mf.base].name;
    expect(anchorage).toBe('Etorofu');
    // (Japan at war with China alone: no enemy lies in its reach at the anchorage, and unheld it would stand forward.)
    expect(foeInReach(g, mf, g.regions[mf.base])).toBe(false);
    expect(forwardHarbour(g, mf)).toBeNull();
    navalAI(g, 'JAP');
    expect(g.regions[mf.base].name).toBe('Etorofu');
    g.flags[`held:${mf.id}`] = g.turn - 1;
    expect(forwardHarbour(g, mf)).not.toBeNull();
  }, 300000);
});

describe("a player's fleets", () => {
  /** Japan's seat at the opening, at war with China, the 4th Fleet at Truk; his navy desk handed to his admirals or his own. */
  function japan(handed: boolean): Game {
    const g = startOpen(createGame(3, 'JAP'));
    setStop(g, 3, true);
    for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
    setDesk(g, 'navy', handed);
    beginHuman(g);
    return g;
  }
  const month = (g: Game): void => { endHuman(g); for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g); beginHuman(g); };
  it('stand forward only where his navy desk is handed to his admirals', () => {
    const own = japan(false), f = fleet(own, 'JAP', '4th Fleet');
    expect(at(own, f)).toBe('Truk');
    expect(forwardHarbour(own, f)?.name).toBe('Iwo Jima');
    month(own);
    expect(at(own, f)).toBe('Truk');
    const handed = japan(true), h = fleet(handed, 'JAP', '4th Fleet');
    month(handed);
    expect(at(handed, h)).toBe('Iwo Jima');
  }, 300000);
});
