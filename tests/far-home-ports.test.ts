// A home port across the water feeds only while the open seas join it to the homeland (the design director's ruling
// of 03:01Z, 22 November 2026, on Luzon reading fed; src/campaign/navy.ts, openSeas).
//
// The supply picture let every home port send out its own supply ships: a port on its owner's ground was a quay on
// any sea of its own that was not closed, and nothing asked for a way back to where the country makes its arms. In
// one war (seed 19, December 1941 to January 1942) Luzon read fed with every sea behind it Japan's, the Far East Air
// Force was made up from America's reserve month after month, and the fighters it was given kept the Luzon Sea from
// closing: a loop. The record made up no aircraft on Luzon after December 1941.
//
// The form, on the far side alone and for every country: the supply ships sail from the home ports the country's own
// roads reach from its capital. A home port across the water is a quay only while the month's open seas join it to
// one of those. A country that holds no capital on the chart, or has no port its roads reach from it, has no such
// place and reads as it did. No number is new. The seas are closed by hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { DESERT, atWar, computeSupply, createGame, declareWar } from '../src/campaign/game';
import { SeaPicture, openSeas, zoneOf, type SeaState } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import type { Game, Region } from '../src/campaign/types';

/** A war nobody plays on the first day's chart, far side on. */
const war = (): Game => startOpen(createGame(3, null));
/** The same in January 1942, Japan at war with America, Britain, the Dutch and the dominions. */
const pacificWar = (): Game => {
  const g = war();
  for (const e of ['USA', 'UK', 'NED', 'AUS', 'NZL']) if (!atWar(g, 'JAP', e)) declareWar(g, 'JAP', e);
  g.turn = 29;
  return g;
};
/** The seas the ports of the Philippines stand on, and the seas round them: the ways in. */
const BESIDE = ['luzonstrait', 'philsea', 'luzonsea', 'visayan', 'sulu', 'leyte', 'palawan', 'celebes'];
const ROUND = ['ryukyu', 'echina', 'formosa', 'schina', 'wcarolines', 'marianas', 'bonin', 'indochina', 'molucca', 'natuna', 'makassar'];
const zones = (g: Game, keys: string[]): Set<number> => new Set(keys.map((k) => zoneOf(g, k)));
/**
 * Seas closed to one country by hand, on top of whatever the month's own picture closes. (A sea closed by hand is also
 * a sea whose state has changed, as it is in play: the supply picture keeps each country's sea lanes while the state
 * of every sea they asked is the same (game.ts lanesKept, navy.ts SeaPicture.state), and closed() reads nothing else.)
 */
const really = SeaPicture.prototype.closed, reallyState = SeaPicture.prototype.state;
const shutTo = (who: string, shut: () => (z: number) => boolean): void => {
  vi.spyOn(SeaPicture.prototype, 'closed').mockImplementation(function (this: SeaPicture, z: number, nat: string): boolean {
    return (nat === who && shut()(z)) || really.call(this, z, nat);
  });
  vi.spyOn(SeaPicture.prototype, 'state').mockImplementation(function (this: SeaPicture, z: number): SeaState | null {
    const s = reallyState.call(this, z);
    if (!shut()(z)) return s;
    const was = s ?? { fleets: [], coasts: [], groups: [], skies: false, wx: 1 };
    return { ...was, fleets: [...was.fleets, { nat: `shut by hand to ${who}`, power: 0 }] };
  });
};
const fed = (g: Game, nat: string, name: string): boolean => !!g.supply[nat][g.byName[name].id];
const homePorts = (g: Game, nat: string): Region[] => g.regions.filter((r) => r.port && r.owner === nat && r.ctrl === nat);

afterEach(() => { vi.restoreAllMocks(); });

describe('a home port across the water feeds only while the open seas join it to the homeland (the director\'s ruling of 03:01Z, 22 November 2026)', () => {
  it('changes no reading while no sea on the way is closed: on the first day every country\'s ships sail the same seas by either form', () => {
    const g = war();
    const pic = new SeaPicture(g);
    for (const n of Object.values(g.nations)) expect([...openSeas(g, n.id, pic)], n.id).toEqual([...openSeas(g, n.id, pic, false)]);
    // The far side's own form is the one the picture reads.
    expect([...openSeas(g, 'USA', pic)]).toEqual([...openSeas(g, 'USA', pic, true)]);
  });

  it('sends the supply ships out from the homeland: the ports of the Philippines send none of their own', () => {
    const g = pacificWar();
    const beside = zones(g, BESIDE), round = zones(g, ROUND);
    // Every way in is closed to America, and the seas the islands stand on are not.
    let shut = (z: number): boolean => round.has(z);
    shutTo('USA', () => shut);
    const pic = new SeaPicture(g);
    for (const z of beside) expect(pic.closed(z, 'USA'), g.seas[z].key).toBe(false);
    const was = openSeas(g, 'USA', pic, false), now = openSeas(g, 'USA', pic);
    for (const z of beside) expect([g.seas[z].key, was[z], now[z]]).toEqual([g.seas[z].key, 1, 0]);
    // The homeland's own seas are open by either form, and so is Hawaii's: the road home from there is not cut.
    for (const key of ['calrun', 'hawaii']) expect([was[zoneOf(g, key)], now[zoneOf(g, key)]], key).toEqual([1, 1]);
    // One way in open again (the Marianas, the road from Hawaii by Guam), and the ships are back in the Luzon Sea.
    shut = (z) => round.has(z) && z !== zoneOf(g, 'marianas');
    expect(openSeas(g, 'USA', new SeaPicture(g))[zoneOf(g, 'luzonsea')]).toBe(1);
  });

  it('Luzon cut off is not fed: an island port goes hungry at once, and Bataan\'s works keep their stores the months the desert\'s ports keep theirs', () => {
    const g = pacificWar();
    const round = zones(g, ROUND);
    const LUZON = ['Aparri', 'Lingayen', 'Manila', 'Bataan', 'Legaspi'], ALONE = ['Mindoro', 'Cebu', 'Panay', 'Palawan', 'Jolo'];
    computeSupply(g);
    for (const nm of [...LUZON, ...ALONE, 'Pearl Harbor', 'Midway', 'Wake']) expect(fed(g, 'USA', nm), nm).toBe(true);
    let shut = (z: number): boolean => round.has(z);
    shutTo('USA', () => shut);
    computeSupply(g);
    // A port alone on its island, its own sea open: by the old form a quay, and now no ship reaches it.
    const sulu = zoneOf(g, 'sulu');
    expect(g.byName['Mindoro'].seas).toEqual([sulu]);
    expect(openSeas(g, 'USA', new SeaPicture(g), false)[sulu]).toBe(1);
    for (const nm of ALONE) expect(fed(g, 'USA', nm), nm).toBe(false);
    // Luzon has Bataan: a fortified port overseas keeps stores, and the roads carry them (game.ts, 'stores').
    expect(g.byName['Bataan'].fort?.lv).toBe(3);
    for (let t = 29; t <= 29 + DESERT.stores; t++) {
      g.turn = t;
      computeSupply(g);
      for (const nm of LUZON) expect(fed(g, 'USA', nm), `${nm}, month ${t}`).toBe(true);
    }
    g.turn = 29 + DESERT.stores + 1;
    computeSupply(g);
    for (const nm of LUZON) expect(fed(g, 'USA', nm), nm).toBe(false);
    // Nothing else of America's reads differently: Hawaii, Midway and Wake are joined to the West Coast still.
    for (const nm of ['Pearl Harbor', 'Midway', 'Wake']) expect(fed(g, 'USA', nm), nm).toBe(true);
    // The way in open again: the ships come, and every port is a quay as before.
    shut = (z) => round.has(z) && z !== zoneOf(g, 'marianas');
    computeSupply(g);
    for (const nm of [...LUZON, ...ALONE]) expect(fed(g, 'USA', nm), nm).toBe(true);
  });

  it('a country that holds no capital on the chart reads as it did: the Indies feed themselves with Holland lost', () => {
    const g = pacificWar();
    const java = zoneOf(g, 'java');
    expect(g.byName['Batavia'].seas).toContain(java);
    // Every sea but the Java Sea closed to the Dutch.
    shutTo('NED', () => (z) => z !== java);
    // Holland held: Batavia is a home port across the water, and no ship from Rotterdam reaches it.
    expect(g.regions[g.nations.NED.capital].ctrl).toBe('NED');
    expect([openSeas(g, 'NED', new SeaPicture(g), false)[java], openSeas(g, 'NED', new SeaPicture(g))[java]]).toEqual([1, 0]);
    // Holland lost, as it was from May 1940: there is no homeland to sail from, and the Indies' ports read as before.
    for (const nm of ['Holland', 'Rotterdam']) g.byName[nm].ctrl = 'GER';
    expect(g.regions[g.nations.NED.capital].ctrl).toBe('GER');
    expect([openSeas(g, 'NED', new SeaPicture(g), false)[java], openSeas(g, 'NED', new SeaPicture(g))[java]]).toEqual([1, 1]);
  });

  it('a country with no port its roads reach from the capital reads as it did: the chart gives Lisbon none', () => {
    const g = war();
    expect(g.regions[g.nations.POR.capital].name).toBe('Lisbon');
    expect(['Lisbon', 'Porto'].map((nm) => [g.byName[nm].owner, !!g.byName[nm].port])).toEqual([['POR', false], ['POR', false]]);
    expect(homePorts(g, 'POR').length).toBe(19);
    const azores = zoneOf(g, 'azores');
    shutTo('POR', () => (z) => z !== azores);
    expect([openSeas(g, 'POR', new SeaPicture(g), false)[azores], openSeas(g, 'POR', new SeaPicture(g))[azores]]).toEqual([1, 1]);
  });
});
