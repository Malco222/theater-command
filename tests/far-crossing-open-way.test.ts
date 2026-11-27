// The way a landing sails (the design director's ruling of 08:53Z, 22 November 2026, on Sandakan in seed 19 at the
// first stop; src/campaign/navy.ts, `landingWay`).
//
// A crossing's way was chosen by the day's risk in each sea (`seaPath`), and its card then read what could come to
// the last of them (`seaHold`): the staff weighed by one map and fought by another. Sandakan stands on the Celebes Sea
// and the Sulu Sea. In January 1942 the way from Jolo took the Celebes Sea, with no enemy in it that day; that sea's
// card read "can be closed" (two fleets could come into it), the staff heeded it and waited two months, while the
// Sulu Sea's card, with Japan's own ships in it, read open.
//
// The form, with the far side's switch on and for a crossing that has a way through a sea of the far side: of the ways
// (the safest to each of the beach's seas) the landing takes one whose card reads open, and among those the least risk
// today. If none reads open it is the safest way as before, and a staff that heeds the card waits. A crossing wholly
// on the old chart reads as it always did, and with the switch off nothing here is read.
// The states are set by hand on the first day's chart; no war is played here.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterAll, describe, expect, it } from 'vitest';
import { addCorps, atWar, createGame, declareWar } from '../src/campaign/game';
import { NAVY, fleetsAnswer, landingPath, planLanding, seaHold, seaPath, seaRisk } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { FAR_SEAS_FIRST } from '../src/campaign/world';
import type { Fleet, Game, Region } from '../src/campaign/types';

const ALLIES = ['USA', 'UK', 'NED', 'AUS'];
const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const sea = (g: Game, key: string): number => g.seas.findIndex((z) => z.key === key);
/** A fleet put to sea in a sea of its reach. */
const toSea = (f: Fleet, z: number): void => { f.zone = z; f.stance = 'patrol'; };
/** A fleet sent to lie in a harbour far away: out of every sea these tests read. */
const away = (g: Game, f: Fleet, place: string): void => { const r = g.byName[place]; f.base = r.id; f.port = r.name; f.home = r.seas[0]; f.zone = r.seas[0]; f.station = r.seas[0]; f.stance = 'port'; };
/** The safest way as it always was, and the way a landing sails. */
const safest = (g: Game, nat: string, from: Region, to: Region): number[] => seaPath(g, nat, from, to, { max: NAVY.landing.range, surprise: false })!;
const sails = (g: Game, nat: string, from: Region, to: Region): number[] => landingPath(g, nat, from, to, false)!;
const card = (g: Game, nat: string, z: number, to: Region) => seaHold(g, nat, [z], to);
const coming = (g: Game, nat: string, z: number, to: Region): string[] => fleetsAnswer(g, nat, [z], to).come.map((f) => f.name).sort();

/**
 * January 1942 by hand: Japan at war with America, Britain, the Dutch and Australia; Jolo Japan's; no Allied aircraft
 * in these skies, and Britain's China Fleet gone from Singapore. What is left in reach of Sandakan's two seas is
 * America's Asiatic Fleet at Manila (both seas) and the Dutch East Indies Fleet at Soerabaja (the Celebes Sea).
 */
const january = (): { g: Game; jolo: Region; sandakan: Region; celebes: number; sulu: number } => {
  const g = startOpen(createGame(3, null));
  // (At war from December: January's landing is no surprise.)
  g.turn = 28;
  for (const e of ALLIES) if (!atWar(g, 'JAP', e)) declareWar(g, 'JAP', e);
  g.turn = 29;
  for (const f of g.airFleets) if (ALLIES.includes(f.nat)) { f.ftr = 0; f.bmr = 0; }
  away(g, fleet(g, 'UK', 'China Fleet'), 'Alexandria');
  const jolo = g.byName.Jolo, sandakan = g.byName.Sandakan;
  jolo.ctrl = 'JAP';
  return { g, jolo, sandakan, celebes: sea(g, 'celebes'), sulu: sea(g, 'sulu') };
};

describe('the way a landing sails on the far side (the director\'s ruling of 08:53Z, 22 November 2026)', () => {
  it('the chart: Sandakan and Jolo each stand on the Celebes Sea and the Sulu Sea, both seas of the far side', () => {
    const { g, jolo, sandakan, celebes, sulu } = january();
    expect([g.seas[celebes].name, g.seas[sulu].name]).toEqual(['Celebes Sea', 'Sulu Sea']);
    expect(sandakan.seas).toEqual([celebes, sulu]);
    expect([...jolo.seas].sort()).toEqual([sulu, celebes].sort());
    expect([celebes >= FAR_SEAS_FIRST, sulu >= FAR_SEAS_FIRST, FAR_SEAS_FIRST]).toEqual([true, true, 67]);
  });

  it('takes the way whose card reads open, where the safest way\'s card reads "can be closed" (Sandakan from Jolo)', () => {
    const { g, jolo, sandakan, celebes, sulu } = january();
    // The Asiatic Fleet is at sea in the Sulu Sea, and Japan's 3rd Fleet stands in that sea against it.
    toSea(fleet(g, 'USA', 'Asiatic Fleet'), sulu);
    toSea(fleet(g, 'JAP', '3rd Fleet'), sulu);
    // The day's risk: none in the Celebes Sea, with nobody in it; some in the Sulu Sea. The safest way is the Celebes Sea.
    expect(seaRisk(g, celebes, 'JAP')).toBe(0);
    expect(seaRisk(g, sulu, 'JAP')).toBeGreaterThan(0.1);
    expect(safest(g, 'JAP', jolo, sandakan)).toEqual([celebes]);
    // Its card: two fleets could come into it and Japan has nothing there. It can be closed.
    expect(coming(g, 'JAP', celebes, sandakan)).toEqual(['Asiatic Fleet', 'Dutch East Indies Fleet']);
    const closed = card(g, 'JAP', celebes, sandakan);
    expect([closed.open, closed.own.total, closed.foe >= NAVY.closed.min]).toEqual([false, 0, true]);
    // The Sulu Sea's card: the enemy's fleet there is less than twice Japan's. It stays open.
    const open = card(g, 'JAP', sulu, sandakan);
    expect([open.open, open.there > 0, open.foe < NAVY.closed.ratio * open.own.total]).toEqual([true, true, true]);
    // The landing sails by the Sulu Sea, and its plan carries that sea's card.
    expect(sails(g, 'JAP', jolo, sandakan)).toEqual([sulu]);
    const c = addCorps(g, 'JAP', 'A division for Sandakan', 'inf', 1, jolo.id, {});
    const plan = planLanding(g, [c], sandakan);
    expect([plan.why, plan.surprise, plan.path, plan.hold?.sea, plan.hold?.open]).toEqual([null, false, [sulu], sulu, true]);
    expect(plan.risk).toBeGreaterThan(0);
  });

  it('takes the safest way as before where no way\'s card reads open: the staff that heeds the card waits, as it did', () => {
    const { g, jolo, sandakan, celebes, sulu } = january();
    // The Asiatic Fleet at sea in the Sulu Sea and nothing of Japan's in either.
    toSea(fleet(g, 'USA', 'Asiatic Fleet'), sulu);
    expect([card(g, 'JAP', celebes, sandakan).open, card(g, 'JAP', sulu, sandakan).open]).toEqual([false, false]);
    expect(sails(g, 'JAP', jolo, sandakan)).toEqual(safest(g, 'JAP', jolo, sandakan));
    expect(sails(g, 'JAP', jolo, sandakan)).toEqual([celebes]);
    const c = addCorps(g, 'JAP', 'A division for Sandakan', 'inf', 1, jolo.id, {});
    const plan = planLanding(g, [c], sandakan);
    expect([plan.path, plan.hold?.sea, plan.hold?.open]).toEqual([[celebes], celebes, false]);
  });

  it('keeps the safest way where its own card reads open, though another way\'s reads open too', () => {
    const { g, jolo, sandakan, celebes, sulu } = january();
    // No enemy fleet in reach of either sea, and Japan's 3rd Fleet at sea in the Sulu Sea.
    away(g, fleet(g, 'USA', 'Asiatic Fleet'), 'Alexandria');
    away(g, fleet(g, 'NED', 'Dutch East Indies Fleet'), 'Alexandria');
    toSea(fleet(g, 'JAP', '3rd Fleet'), sulu);
    expect([card(g, 'JAP', celebes, sandakan).open, card(g, 'JAP', sulu, sandakan).open]).toEqual([true, true]);
    expect(safest(g, 'JAP', jolo, sandakan)).toEqual([celebes]);
    expect(sails(g, 'JAP', jolo, sandakan)).toEqual([celebes]);
  });

  it('among the ways whose cards read open takes the least risk today, not the first on the chart (Aparri from Formosa)', () => {
    const { g } = january();
    const formosa = g.byName.Formosa, aparri = g.byName.Aparri;
    const strait = sea(g, 'luzonstrait'), phil = sea(g, 'philsea'), luzon = sea(g, 'luzonsea');
    // Aparri stands on three seas: the Luzon Strait (one sea from Formosa), then the Philippine Sea and the Luzon Sea (two).
    expect(aparri.seas).toEqual([strait, phil, luzon]);
    // The Asiatic Fleet at sea in the Philippine Sea, Japan's 2nd Fleet against it there, and its 3rd in the Luzon Sea.
    toSea(fleet(g, 'USA', 'Asiatic Fleet'), phil);
    toSea(fleet(g, 'JAP', '2nd Fleet'), phil);
    toSea(fleet(g, 'JAP', '3rd Fleet'), luzon);
    // The safest way is the strait alone, and the Asiatic Fleet could come into it: its card can be closed.
    expect(safest(g, 'JAP', formosa, aparri)).toEqual([strait]);
    expect(card(g, 'JAP', strait, aparri).open).toBe(false);
    // Both the others read open: the Philippine Sea with the enemy at sea in it (a risk today), the Luzon Sea with none.
    expect([card(g, 'JAP', phil, aparri).open, card(g, 'JAP', luzon, aparri).open]).toEqual([true, true]);
    expect(seaRisk(g, phil, 'JAP')).toBeGreaterThan(0.1);
    expect(seaRisk(g, luzon, 'JAP')).toBe(0);
    // The Philippine Sea comes first on the chart; the landing sails by the Luzon Sea, the less risk of the two.
    const way = sails(g, 'JAP', formosa, aparri);
    expect(way[way.length - 1]).toBe(luzon);
    expect(way).toEqual([strait, luzon]);
  });
});

/**
 * The spring of 1940 by hand on the old chart: Germany at war with Britain and France. Britain's Home Fleet and
 * Western Approaches Fleet and France's Atlantic Fleet are sent away; the Dover Fleet is at sea in the Channel,
 * released from its station, and Germany's Battle Fleet is at sea in the Channel against it. From Bremen to East
 * Anglia, which stands on the Channel and the North Sea: the safest way is the North Sea alone, whose card can be
 * closed (the Dover Fleet could come into it), and the Channel's card reads open.
 */
const spring = (mk: { createGame: typeof createGame; startOpen: typeof startOpen; atWar: typeof atWar; declareWar: typeof declareWar }): { g: Game; bremen: Region; anglia: Region; channel: number; north: number } => {
  const g = mk.startOpen(mk.createGame(3, null));
  g.turn = 9;
  for (const e of ['UK', 'FRA']) if (!mk.atWar(g, 'GER', e)) mk.declareWar(g, 'GER', e);
  g.turn = 10;
  away(g, fleet(g, 'UK', 'Home Fleet'), 'Alexandria');
  away(g, fleet(g, 'UK', 'Western Approaches Fleet'), 'Alexandria');
  away(g, fleet(g, 'FRA', 'Atlantic Fleet'), 'Marseille');
  const channel = sea(g, 'channel'), north = sea(g, 'nsea');
  const dover = fleet(g, 'UK', 'Dover Fleet');
  dover.tied = false;
  toSea(dover, channel);
  toSea(fleet(g, 'GER', 'Battle Fleet'), channel);
  return { g, bremen: g.byName.Bremen, anglia: g.byName['East Anglia'], channel, north };
};

describe('a crossing wholly on the old chart', () => {
  it('sails the safest way as it always did, with the far side on, though that way\'s card can be closed and another\'s reads open', () => {
    const { g, bremen, anglia, channel, north } = spring({ createGame, startOpen, atWar, declareWar });
    expect(anglia.seas).toEqual([channel, north]);
    expect([channel < FAR_SEAS_FIRST, north < FAR_SEAS_FIRST]).toEqual([true, true]);
    expect(safest(g, 'GER', bremen, anglia)).toEqual([north]);
    expect(coming(g, 'GER', north, anglia)).toEqual(['Dover Fleet']);
    expect([card(g, 'GER', north, anglia).open, card(g, 'GER', channel, anglia).open]).toEqual([false, true]);
    // The far side's form would take the Channel. The old chart's crossing reads as today.
    expect(sails(g, 'GER', bremen, anglia)).toEqual([north]);
  });
});

describe("with the far side's switch off", () => {
  const G = globalThis as { TC_FAR?: unknown };
  afterAll(() => { G.TC_FAR = 1; });

  it('a landing sails the safest way, whatever the cards read', async () => {
    vi.resetModules();
    G.TC_FAR = 0;
    const { FAR_STAGE } = await import('../src/campaign/stage');
    expect(FAR_STAGE).toBe(false);
    const game = await import('../src/campaign/game');
    const staff = await import('../src/campaign/staff');
    const navy = await import('../src/campaign/navy');
    const { g, bremen, anglia, channel, north } = spring({ createGame: game.createGame, startOpen: staff.startOpen, atWar: game.atWar, declareWar: game.declareWar });
    expect(g.seas.length).toBe(67);
    const way = navy.seaPath(g, 'GER', bremen, anglia, { max: navy.NAVY.landing.range, surprise: false });
    expect(way).toEqual([north]);
    expect([navy.seaHold(g, 'GER', [north], anglia).open, navy.seaHold(g, 'GER', [channel], anglia).open]).toEqual([false, true]);
    expect(navy.landingPath(g, 'GER', bremen, anglia, false)).toEqual(way);
  });
});
