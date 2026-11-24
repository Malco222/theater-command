// A fleet whose base falls makes for the nearest fed port held by its own nation or by an ally at war with the same
// enemy, and never into a sea the enemy has closed (the design director's ruling of 03:42Z, 22 November 2026, on the
// Asiatic Fleet's months; src/campaign/navy.ts, haven and newBase).
//
// A fleet whose base fell went to the nearest port its own nation still held, whatever stood in that port's sea and
// whether a ship could feed it. In one war (seed 19) the Asiatic Fleet, with Manila lost, moved to Bataan under
// Japan's bombers, then to Mindoro, then to Panay: never to an ally's harbour, so never south to the Indies, where
// the record took it in December 1941.
//
// The form, on the far side alone and for every nation, with no number: the nearest haven. A haven is a port its
// own nation reads fed, held by that nation or by an ally at war with the enemy the base fell to, whose sea is not
// closed to the fleet's nation. With no haven in its reach the fleet goes where it went. The seas are closed by
// hand with Japan's own fleets on the first day's chart; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { allied, atWar, computeSupply, createGame, declareWar } from '../src/campaign/game';
import { canSailTo, closedTo, haven, navalUpkeep, zoneOf } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game, Region } from '../src/campaign/types';

/** January 1942 on the first day's chart: Japan at war with America, Britain, the Dutch and the dominions; Germany with the Soviet Union and America. */
const war = (): Game => {
  const g = startOpen(createGame(3, null));
  for (const e of ['USA', 'UK', 'NED', 'AUS', 'NZL']) if (!atWar(g, 'JAP', e)) declareWar(g, 'JAP', e);
  for (const e of ['SOV', 'USA']) if (!atWar(g, 'GER', e)) declareWar(g, 'GER', e);
  g.turn = 30;
  computeSupply(g);
  return g;
};
const asiatic = (g: Game): Fleet => g.fleets.find((f) => f.nat === 'USA' && f.name === 'Asiatic Fleet')!;
const port = (g: Game, name: string): Region => g.byName[name];
/** One of Japan's fleets put on patrol in a sea: what closes it. */
const patrol = (g: Game, name: string, key: string): void => {
  const f = g.fleets.find((x) => x.nat === 'JAP' && x.name === name)!;
  f.zone = zoneOf(g, key);
  f.stance = 'patrol';
};
/** Japan's fleets round the Philippines: every sea an American port of the islands lies in is closed to America. */
const ring = (g: Game): void => {
  patrol(g, '2nd Fleet', 'luzonsea');
  patrol(g, '3rd Fleet', 'sulu');
  patrol(g, 'Mobile Fleet', 'visayan');
  patrol(g, 'Combined Fleet', 'luzonstrait');
  patrol(g, '4th Fleet', 'celebes');
  patrol(g, 'Southern Expeditionary Fleet', 'schina');
};
const news = (g: Game): string[] => g.headlines.filter((h) => h.turn === g.turn && h.text.includes('Asiatic Fleet')).map((h) => h.text);

describe('a fleet whose base falls makes for the nearest fed port of its own nation or of an ally in the same war, and into no closed sea (the director\'s ruling of 03:42Z, 22 November 2026)', () => {
  it('with no sea closed and every port fed it goes where it went: the nearest port of its own', () => {
    const g = war();
    const f = asiatic(g);
    expect(g.regions[f.base].name).toBe('Manila');
    port(g, 'Manila').ctrl = 'JAP';
    expect(haven(g, f, port(g, 'Bataan'))).toBe(true);
    navalUpkeep(g, 'USA');
    expect(g.regions[f.base].name).toBe('Bataan');
    expect(news(g)).toContain('With Manila lost, the Asiatic Fleet moves its base to Bataan.');
  });

  it('it goes into no sea the enemy has closed: with Japan\'s fleets round the islands it makes for an ally\'s harbour, Brunei', () => {
    const g = war();
    const f = asiatic(g);
    port(g, 'Manila').ctrl = 'JAP';
    ring(g);
    for (const key of ['luzonsea', 'sulu', 'visayan', 'luzonstrait', 'celebes', 'schina']) expect(closedTo(g, zoneOf(g, key), 'USA'), key).toBe(true);
    // America's own ports of the Philippines are fed, and each lies in a closed sea: none is a haven.
    const own = g.regions.filter((r) => r.port && r.ctrl === 'USA' && r.owner === 'USA' && ['Bataan', 'Lingayen', 'Mindoro', 'Legaspi', 'Aparri', 'Panay', 'Cebu', 'Davao', 'Jolo'].includes(r.name));
    expect(own.length).toBe(9);
    for (const r of own) expect([r.name, !!g.supply.USA[r.id], haven(g, f, r)]).toEqual([r.name, true, false]);
    // Hong Kong and Britain's and the Dutch ports on the Celebes Sea are shut in with them.
    for (const nm of ['Hong Kong', 'Sandakan', 'Tarakan']) expect(haven(g, f, port(g, nm)), nm).toBe(false);
    // Brunei on the Palawan Passage: Britain's, at war with Japan, fed, its sea open, and nearer than any other.
    const brunei = port(g, 'Brunei');
    expect([brunei.ctrl, atWar(g, 'UK', 'JAP'), g.seas[brunei.seas[0]].key, closedTo(g, brunei.seas[0], 'USA'), haven(g, f, brunei)]).toEqual(['UK', true, 'palawan', false, true]);
    navalUpkeep(g, 'USA');
    expect(g.regions[f.base].name).toBe('Brunei');
    expect(news(g)).toContain('With Manila lost, the Asiatic Fleet moves its base to Brunei.');
  });

  it('an ally not at war with the enemy the base fell to is no haven: Indochina\'s harbours, nearer than Brunei, and the Soviet Union\'s', () => {
    const g = war();
    const f = asiatic(g);
    port(g, 'Manila').ctrl = 'JAP';
    ring(g);
    // French Indochina: a friend's ports, fed, in an open sea, in the fleet's reach, and France is not at war with Japan.
    for (const nm of ['Qui Nhon', 'Cam Ranh']) {
      const r = port(g, nm);
      expect([nm, allied(g, 'USA', r.ctrl), atWar(g, r.ctrl, 'JAP'), !!g.supply.USA[r.id], closedTo(g, r.seas[0], 'USA'), canSailTo(g, f, r), haven(g, f, r)]).toEqual([nm, true, false, true, false, true, false]);
    }
    // Petropavlovsk: the Soviet Union is America's ally against Germany and at peace with Japan.
    const pet = port(g, 'Petropavlovsk');
    expect([allied(g, 'USA', 'SOV'), atWar(g, 'SOV', 'JAP'), !!g.supply.USA[pet.id], closedTo(g, pet.seas[0], 'USA'), haven(g, f, pet)]).toEqual([true, false, true, false, false]);
    // The Soviet Union at war with Japan: it is one.
    declareWar(g, 'SOV', 'JAP');
    expect(haven(g, f, pet)).toBe(true);
  });

  it('an unfed port is no haven: with Brunei not fed the fleet makes for the next, Miri', () => {
    const g = war();
    const f = asiatic(g);
    port(g, 'Manila').ctrl = 'JAP';
    ring(g);
    const brunei = port(g, 'Brunei');
    // (On a copy: the picture computeSupply hands out may be kept for its next call, and is never written into.)
    g.supply.USA = Uint8Array.from(g.supply.USA);
    g.supply.USA[brunei.id] = 0;
    expect(haven(g, f, brunei)).toBe(false);
    navalUpkeep(g, 'USA');
    expect([g.regions[f.base].name, g.regions[f.base].ctrl]).toEqual(['Miri', 'UK']);
  });

  it('with no haven in its reach it goes where it went: the nearest port its own nation holds, closed sea or not', () => {
    const g = war();
    const f = asiatic(g);
    port(g, 'Manila').ctrl = 'JAP';
    ring(g);
    // No port anywhere reads fed for America.
    // (On a copy: the picture computeSupply hands out may be kept for its next call, and is never written into.)
    g.supply.USA = new Uint8Array(g.supply.USA.length);
    expect(g.regions.some((r) => haven(g, f, r))).toBe(false);
    navalUpkeep(g, 'USA');
    expect(g.regions[f.base].name).toBe('Bataan');
    expect(g.fleets).toContain(f);
  });

  it('a base that has not fallen to an enemy gives no ally\'s harbour: only its own nation\'s ports are havens then', () => {
    const g = war();
    const f = asiatic(g);
    // Manila in the hands of a country America is not at war with.
    port(g, 'Manila').ctrl = 'THA';
    expect(atWar(g, 'USA', 'THA')).toBe(false);
    expect([haven(g, f, port(g, 'Bataan')), haven(g, f, port(g, 'Brunei'))]).toEqual([true, false]);
  });
});
