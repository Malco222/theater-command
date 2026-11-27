// The far side's submarine beats (the design director's notes of 13:45Z, 13:59Z and 14:06Z, 22 November 2026; the
// historian's paper sub-patrols-japan-oil.md, sections 4 and 9).
//
// The fault it mends: on the far chart America's boats lay in harbour with the Pacific Submarine Fleet (duty port), or
// sailed with the Asiatic Fleet's cruisers, and no fleet can be sent farther than three seas from its base, so no boat
// ever reached Japan's trade and Japan's lanes were never raided. The record's boats patrolled off Japan from Pearl
// Harbor, and from Fremantle to the South China Sea.
//
// The rule, in the form Germany's Mediterranean boats already have: the rows are boats on hand by command, lent from
// the navy's own fleets while it is at war with the nation its groups hunt. A group sails from the record's base of
// the day while its side holds it, and its boats raid as far from that base as the record's boats patrolled in that
// half-year: no number of ours. At stops 1 and 2 the beat is read from the record's seas; at stops 3 to 5 the staff
// takes the sea in that reach where the enemy's shipping is thickest. Every state here is set by hand on the first
// day's chart; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { allied, atWar, createGame, declareWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { seaOptions } from '../src/campaign/fleets';
import { NAVY, baseInForce, boatsReach, fleetReach, lanesTo, ownBoatsReach, placeBoats, sail, shippingIn, zoneOf } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game } from '../src/campaign/types';

const NAVIES = ['USA', 'NED', 'UK', 'JAP'];
const grand = (stop = 1): Game => { const g = startOpen(createGame(3, null)); if (stop !== 3) setStop(g, stop, true); return g; };
const fleet = (g: Game, nat: string, name: string): Fleet | undefined => g.fleets.find((f) => f.nat === nat && f.name === name);
const boats = (g: Game, nat: string, name: string): number => fleet(g, nat, name)?.ships.ss ?? 0;
const all = (g: Game, nat: string): number => g.fleets.reduce((n, f) => n + (f.nat === nat ? f.ships.ss : 0), 0);
const sea = (g: Game, f: Fleet | undefined): string => (f ? g.seas[f.station].key : '');
const place = (g: Game, turn?: number): void => { if (turn !== undefined) g.turn = turn; for (const nat of NAVIES) placeBoats(g, nat); };
const news = (g: Game, words: RegExp): string[] => g.headlines.filter((h) => words.test(h.text)).map((h) => h.text);
/** Japan at war with America, Britain and the Dutch, who stand together; the month is December 1941 and the Asiatic Fleet has its 29 boats. */
const war = (g: Game, turn = 28): Game => {
  g.turn = turn;
  for (const foe of ['USA', 'UK', 'NED']) if (!atWar(g, 'JAP', foe)) declareWar(g, 'JAP', foe);
  for (const nat of ['USA', 'NED', 'AUS']) g.nations[nat].align = 'allies';
  if (!atWar(g, 'JAP', 'AUS')) declareWar(g, 'JAP', 'AUS');
  fleet(g, 'USA', 'Asiatic Fleet')!.ships.ss = 29;
  return g;
};
const hold = (g: Game, place: string, nat: string): void => { g.byName[place].ctrl = nat; };

describe("the far side's submarine beats (the design director, 13:45Z to 14:06Z of 22 November 2026)", () => {
  it("lays the rows for four navies: the historian's boats on hand by command, and every name a Fleet", () => {
    const B = NAVY.beats!;
    expect(Object.keys(B).sort()).toEqual(['GER', 'ITA', 'JAP', 'NED', 'UK', 'USA']);
    for (const nat of NAVIES) { expect(B[nat].far, nat).toEqual(nat === 'JAP' ? { war: 28, all: true } : { war: 28 }); for (const G of B[nat].groups) { expect(G.name, nat).toMatch(/Fleet$/); expect(G.duty, G.name).toBe('raid'); expect(G.ships, G.name).toEqual({ ss: 0 }); } }
    expect(B.USA.pools).toEqual(['Pacific Submarine Fleet', 'Asiatic Fleet']);
    expect(B.USA.groups.map((G) => G.name)).toEqual(['Central Pacific Submarine Fleet', 'Asiatic Submarine Fleet', 'South West Pacific Submarine Fleet', 'Brisbane Submarine Fleet']);
    // The paper's 9d: Pearl Harbor, the Asiatic Fleet, Fremantle, Brisbane.
    expect(B.USA.rows).toEqual([[28, 22, 29, 0, 0], [31, 27, 0, 10, 0], [32, 30, 0, 10, 12], [40, 70, 0, 26, 12], [47, 85, 0, 30, 12], [53, 100, 0, 30, 12], [60, 105, 0, 40, 12], [68, 130, 0, 35, 12], [72, 140, 0, 30, 12]]);
    expect(B.NED.rows).toEqual([[28, 15], [31, 0]]);
    // The British by the record's dates: Ceylon from August 1943, the 8th from September 1944, the 4th from April 1945.
    expect(B.UK.rows).toEqual([[48, 9, 0, 0], [53, 12, 0, 0], [60, 15, 0, 0], [61, 15, 11, 0], [68, 15, 11, 9]]);
    expect(B.JAP.rows).toEqual([[28, 63]]);
    expect([B.NED.pools, B.UK.pools, B.JAP.pools]).toEqual([['Dutch East Indies Fleet'], ['China Fleet'], ['6th Fleet']]);
  });

  it('every base is a harbour of the chart on its home sea, in date order, and every sea of the lists is on the chart', () => {
    const g = grand();
    for (const nat of NAVIES) {
      expect(NAVY.beats![nat].pools!.every((name) => !!fleet(g, nat, name)), nat).toBe(true);
      for (const G of NAVY.beats![nat].groups) {
        const dated: [number, string, string, string][] = [[0, G.base!, G.port!, G.home!], ...(G.bases ?? [])];
        dated.forEach(([from, base, , home], i) => {
          const r = g.byName[base];
          expect(r, `${G.name}: ${base}`).toBeDefined();
          expect(r.port, base).toBe(true);
          expect(r.seas.includes(zoneOf(g, home)), `${base} on ${home}`).toBe(true);
          if (i) expect(from, base).toBeGreaterThan(dated[i - 1][0]);
        });
        expect(zoneOf(g, G.zone!), G.name).toBeGreaterThanOrEqual(0);
        for (const [, seas] of G.hunts!.seas) for (const k of seas) expect(zoneOf(g, k), `${G.name}: ${k}`).toBeGreaterThanOrEqual(0);
        expect(g.nations[G.hunts!.to], G.name).toBeDefined();
      }
    }
  });

  it('at peace with the nation they hunt, the boats stay with their fleets', () => {
    const g = grand();
    const was = NAVIES.map((nat) => all(g, nat));
    for (const turn of [1, 27, 28, 40]) { place(g, turn); for (const nat of NAVIES) for (const G of NAVY.beats![nat].groups) expect(fleet(g, nat, G.name), `${G.name} at ${turn}`).toBeUndefined(); }
    expect(NAVIES.map((nat) => all(g, nat))).toEqual(was);
    expect([boats(g, 'USA', 'Pacific Submarine Fleet'), boats(g, 'JAP', '6th Fleet'), boats(g, 'UK', 'China Fleet'), boats(g, 'NED', 'Dutch East Indies Fleet')]).toEqual([22, 46, 15, 15]);
  });

  it('at war the pools lend what the row asks: 22 at Pearl Harbor and 29 at Manila in December 1941', () => {
    const g = war(grand());
    expect([allied(g, 'USA', 'AUS'), allied(g, 'USA', 'NED'), allied(g, 'UK', 'AUS')]).toEqual([true, true, true]);
    place(g);
    expect([boats(g, 'USA', 'Central Pacific Submarine Fleet'), boats(g, 'USA', 'Asiatic Submarine Fleet')]).toEqual([22, 29]);
    expect([boats(g, 'USA', 'Pacific Submarine Fleet'), boats(g, 'USA', 'Asiatic Fleet')]).toEqual([0, 0]);
    expect(all(g, 'USA')).toBe(51);
    // Only the boats move: the Asiatic Fleet keeps its cruisers and destroyers, and the emptied fleet at Pearl Harbor stands for the yards' boats.
    expect([fleet(g, 'USA', 'Asiatic Fleet')!.ships.ca, fleet(g, 'USA', 'Asiatic Fleet')!.ships.dd]).toEqual([3, 13]);
    expect(fleet(g, 'USA', 'Pacific Submarine Fleet')).toBeDefined();
    const pearl = fleet(g, 'USA', 'Central Pacific Submarine Fleet')!, manila = fleet(g, 'USA', 'Asiatic Submarine Fleet')!;
    expect([pearl.port, pearl.duty, manila.port, manila.duty]).toEqual(['Pearl Harbor', 'raid', 'Manila', 'raid']);
    expect(news(g, /Submarine Fleet forms/)).toEqual(expect.arrayContaining(['The Central Pacific Submarine Fleet forms at Pearl Harbor.', 'The Asiatic Submarine Fleet forms at Manila.', 'The East Indies Submarine Fleet forms at Soerabaja.', 'The Advance Submarine Fleet forms at Kwajalein.']));
    // The Dutch 15 and all the 6th Fleet's boats (the row's 63 is above what the campaign has: a split, not a demand).
    expect([boats(g, 'NED', 'East Indies Submarine Fleet'), boats(g, 'NED', 'Dutch East Indies Fleet')]).toEqual([15, 0]);
    expect([boats(g, 'JAP', 'Advance Submarine Fleet'), boats(g, 'JAP', '6th Fleet'), boats(g, 'JAP', '4th Fleet')]).toEqual([46, 0, 9]);
    // No British boat in the Far East until August 1943: the China Fleet keeps its own.
    expect([boats(g, 'UK', 'China Fleet'), fleet(g, 'UK', 'Ceylon Submarine Fleet')]).toEqual([15, undefined]);
  });

  it('a row above the pool is a split among the commands, and no boat is made or lost', () => {
    const g = war(grand());
    place(g);
    // December 1942: the row asks 70, 26 and 12, and the campaign has its 51 and the yards' 17.
    fleet(g, 'USA', 'Pacific Submarine Fleet')!.ships.ss += 17;
    place(g, 40);
    expect(all(g, 'USA')).toBe(68);
    expect([boats(g, 'USA', 'Central Pacific Submarine Fleet'), boats(g, 'USA', 'Asiatic Submarine Fleet'), boats(g, 'USA', 'South West Pacific Submarine Fleet'), boats(g, 'USA', 'Brisbane Submarine Fleet')]).toEqual([44, 0, 16, 8]);
    expect([boats(g, 'USA', 'Pacific Submarine Fleet'), boats(g, 'USA', 'Asiatic Fleet')]).toEqual([0, 0]);
    // With more boats than the row asks, the rest stay at home with the first pool.
    fleet(g, 'USA', 'Pacific Submarine Fleet')!.ships.ss += 100;
    place(g, 41);
    expect([boats(g, 'USA', 'Central Pacific Submarine Fleet'), boats(g, 'USA', 'South West Pacific Submarine Fleet'), boats(g, 'USA', 'Brisbane Submarine Fleet')]).toEqual([70, 26, 12]);
    expect([boats(g, 'USA', 'Pacific Submarine Fleet'), all(g, 'USA')]).toEqual([60, 168]);
  });

  it("at stops 3 to 5 a group keeps the base it holds, as any fleet does (Units decide audit item 43)", () => {
    for (const stop of [3, 5]) {
      const g = war(grand(stop));
      place(g);
      const pearl = (): Fleet => fleet(g, 'USA', 'Central Pacific Submarine Fleet')!;
      place(g, 34);
      expect(pearl().port, `stop ${stop}`).toBe('Pearl Harbor');
      place(g, 35);
      expect(pearl().port, `stop ${stop}`).toBe('Pearl Harbor');
      expect(news(g, /Central Pacific Submarine Fleet moves its base/), `stop ${stop}`).toEqual([]);
      // Its boats still go out from it in the record's shares.
      expect(boats(g, 'USA', 'Central Pacific Submarine Fleet'), `stop ${stop}`).toBeGreaterThan(0);
    }
  });

  it("a group sails from the record's base of the day, while its side holds it", () => {
    const g = war(grand());
    place(g);
    const pearl = (): Fleet => fleet(g, 'USA', 'Central Pacific Submarine Fleet')!;
    place(g, 34);
    expect(pearl().port).toBe('Pearl Harbor');
    place(g, 35);
    expect([pearl().port, g.regions[pearl().base].name, g.seas[pearl().home].key]).toEqual(['Midway', 'Midway', 'midway']);
    expect(news(g, /moves its base/)).toContain('The Central Pacific Submarine Fleet moves its base to Midway.');
    // Majuro in February 1944 only if America holds it; Saipan and Guam the same.
    place(g, 54);
    expect(pearl().port).toBe('Midway');
    hold(g, 'Eastern Marshalls', 'USA');
    place(g, 55);
    expect([pearl().port, g.regions[pearl().base].name]).toEqual(['Majuro', 'Eastern Marshalls']);
    hold(g, 'Guam', 'JAP');
    place(g, 62);
    expect(pearl().port).toBe('Majuro');
    hold(g, 'Saipan', 'USA');
    place(g, 63);
    expect(pearl().port).toBe('Saipan');
    hold(g, 'Guam', 'USA');
    place(g, 64);
    expect(pearl().port).toBe('Guam');
    // The Fremantle boats: Perth's harbour, an ally's, and Subic Bay from March 1945 while Bataan is America's.
    const fremantle = (): Fleet => fleet(g, 'USA', 'South West Pacific Submarine Fleet')!;
    expect([fremantle().port, g.regions[fremantle().base].name]).toEqual(['Fremantle', 'Perth']);
    hold(g, 'Bataan', 'JAP');
    place(g, 67);
    expect(fremantle().port).toBe('Fremantle');
    hold(g, 'Bataan', 'USA');
    place(g, 68);
    expect([fremantle().port, g.seas[fremantle().home].key]).toEqual(['Subic Bay', 'luzonsea']);
    // The record's base lost again: back to the one before it.
    hold(g, 'Bataan', 'JAP');
    place(g, 69);
    expect(fremantle().port).toBe('Fremantle');
    expect(baseInForce(g, 'USA', NAVY.beats!.USA.groups[2])).toEqual({ base: 'Perth', port: 'Fremantle', home: 'westaus' });
  });

  it("its boats raid as far as the record's boats patrolled that half-year from that base, and go no farther on any other errand", () => {
    const g = war(grand());
    place(g);
    const G = NAVY.beats!.USA.groups[0], f = fleet(g, 'USA', 'Central Pacific Submarine Fleet')!;
    const z = (k: string): number => zoneOf(g, k);
    // From Pearl Harbor in 1941-42 the farthest of the record's seas is Kyushu's, five seas off; no sea of Japan's lanes is nearer than six.
    expect([boatsReach(g, f, G, 28), f.reach]).toEqual([5, 5]);
    expect(fleetReach(g, f, false, true)).toContain(z('kyushu'));
    for (const k of ['luzonstrait', 'echina', 'tsushima']) expect(fleetReach(g, f, false, true), k).not.toContain(z(k));
    // For any other errand it has any fleet's reach, three seas.
    expect(fleetReach(g, f)).not.toContain(z('honshu'));
    expect(fleetReach(g, f).length).toBeLessThan(fleetReach(g, f, false, true).length);
    expect([sail(g, f, z('honshu'), 'patrol'), sail(g, f, z('honshu'), 'raid'), g.seas[f.zone].key, f.stance]).toEqual([false, true, 'honshu', 'raid']);
    // 1943, from Midway: Formosa, six seas off, and the Luzon Strait within it.
    place(g, 41);
    expect([f.port, f.reach, boatsReach(g, f, G, 41)]).toEqual(['Midway', 6, 6]);
    expect(fleetReach(g, f, false, true)).toContain(z('luzonstrait'));
    // The forward base shortens nothing by a number of ours: from Guam the same seas are nearer, and the reach is counted from there.
    place(g, 62);
    expect([f.port, f.reach]).toEqual(['Guam', 4]);
    expect(fleetReach(g, f, false, true)).toEqual(expect.arrayContaining([z('luzonstrait'), z('schina'), z('echina'), z('formosa')]));
    // No other fleet has such a reach: the pools and the surface fleets read as they did.
    for (const x of g.fleets) if (!NAVIES.some((nat) => x.nat === nat && NAVY.beats![nat].groups.some((H) => H.name === x.name))) expect(x.reach, x.name).toBeUndefined();
  });

  it("at stops 1 and 2 the beat is the record's sea that carries the hunted trade, or the first of the record's seas", () => {
    const g = war(grand(1));
    place(g);
    const asiatic = fleet(g, 'USA', 'Asiatic Submarine Fleet')!, pearl = fleet(g, 'USA', 'Central Pacific Submarine Fleet')!, dutch = fleet(g, 'NED', 'East Indies Submarine Fleet')!;
    // Manila's boats lie on Japan's lanes; Pearl Harbor's reach none in 1942 and keep Honshu's sea.
    expect(lanesTo(g, 'JAP', zoneOf(g, 'luzonsea'))).toBeGreaterThan(0);
    expect([sea(g, asiatic), sea(g, pearl), sea(g, dutch)]).toEqual(['luzonsea', 'honshu', 'schina']);
    expect(lanesTo(g, 'JAP', pearl.station)).toBe(0);
    // January 1943: the Luzon Strait is on the record's list and in reach from Midway.
    place(g, 41);
    expect(sea(g, pearl)).toBe('luzonstrait');
    expect(lanesTo(g, 'JAP', pearl.station)).toBeGreaterThan(0);
    // The Fremantle boats keep the Java Sea until the South China Sea is on their list, in July 1943.
    const fremantle = fleet(g, 'USA', 'South West Pacific Submarine Fleet')!;
    expect([sea(g, fremantle), fremantle.reach]).toEqual(['java', 5]);
    place(g, 47);
    expect([sea(g, fremantle), fremantle.reach]).toEqual(['schina', 6]);
    // Brisbane's boats work the Bismarck Sea, where no lane of Japan's runs.
    expect(sea(g, fleet(g, 'USA', 'Brisbane Submarine Fleet'))).toBe('bismarck');
  });

  it('at stops 3 to 5 the staff takes the sea in that reach where the enemy shipping is thickest', () => {
    const g = war(grand(3));
    place(g);
    place(g, 41);
    const pearl = fleet(g, 'USA', 'Central Pacific Submarine Fleet')!;
    const most = Math.max(...fleetReach(g, pearl, false, true).map((z) => shippingIn(g, 'USA', z)));
    expect(most).toBeGreaterThan(0);
    expect(shippingIn(g, 'USA', pearl.station)).toBe(most);
    // Where no cargo sails in its reach it keeps the first of the record's seas for the half-year.
    expect(sea(g, fleet(g, 'USA', 'Brisbane Submarine Fleet'))).toBe('bismarck');
  });

  it('the Asiatic boats go south to Soerabaja, and their group and the Dutch stand down in March 1942', () => {
    const g = war(grand());
    place(g);
    place(g, 29);
    const asiatic = fleet(g, 'USA', 'Asiatic Submarine Fleet')!;
    expect([asiatic.port, g.seas[asiatic.home].key, asiatic.ships.ss]).toEqual(['Soerabaja', 'java', 29]);
    expect(news(g, /moves its base/)).toContain('The Asiatic Submarine Fleet moves its base to Soerabaja.');
    // Soerabaja and Manila both lost before March: the group has no base and its boats go home to the pool.
    hold(g, 'Soerabaja', 'JAP'); hold(g, 'Manila', 'JAP');
    place(g, 30);
    expect([fleet(g, 'USA', 'Asiatic Submarine Fleet'), fleet(g, 'NED', 'East Indies Submarine Fleet')]).toEqual([undefined, undefined]);
    expect([boats(g, 'USA', 'Central Pacific Submarine Fleet'), boats(g, 'USA', 'Pacific Submarine Fleet'), boats(g, 'NED', 'Dutch East Indies Fleet'), all(g, 'USA')]).toEqual([22, 29, 15, 51]);
    // March 1942 by the row: Fremantle's ten.
    place(g, 31);
    expect([boats(g, 'USA', 'Central Pacific Submarine Fleet'), boats(g, 'USA', 'South West Pacific Submarine Fleet'), boats(g, 'USA', 'Pacific Submarine Fleet')]).toEqual([27, 10, 14]);
  });

  it('Britain: no boat until August 1943, then Ceylon, the 8th at Fremantle and on to Subic Bay, and the 4th', () => {
    const g = war(grand());
    place(g, 47);
    expect([boats(g, 'UK', 'China Fleet'), fleet(g, 'UK', 'Ceylon Submarine Fleet')]).toEqual([15, undefined]);
    place(g, 48);
    expect([boats(g, 'UK', 'Ceylon Submarine Fleet'), boats(g, 'UK', 'China Fleet'), fleet(g, 'UK', 'Ceylon Submarine Fleet')!.port]).toEqual([9, 6, 'Trincomalee']);
    // September 1944: the row asks 15 and 11 of the 15 the campaign has.
    place(g, 61);
    expect([boats(g, 'UK', 'Ceylon Submarine Fleet'), boats(g, 'UK', '8th Submarine Fleet'), boats(g, 'UK', 'China Fleet')]).toEqual([9, 6, 0]);
    expect(fleet(g, 'UK', '8th Submarine Fleet')!.port).toBe('Fremantle');
    place(g, 69);
    expect(fleet(g, 'UK', '8th Submarine Fleet')!.port).toBe('Subic Bay');
    expect(fleet(g, 'UK', '4th Submarine Fleet')!.port).toBe('Fremantle');
    expect(all(g, 'UK')).toBe(15 + 8);
  });

  it("Japan: the 6th Fleet's boats off Hawaii for a year, and still out in 1943 (the design director, 20:03Z)", () => {
    const g = war(grand());
    place(g);
    const f = fleet(g, 'JAP', 'Advance Submarine Fleet')!;
    // Kwajalein to Ceylon, nine seas: the farthest of the record's seas of 1942. No lane feeds America on the chart, so it keeps the first of them.
    expect([f.port, f.ships.ss, f.reach, sea(g, f), f.duty]).toEqual(['Kwajalein', 46, 9, 'hawaii', 'raid']);
    expect(sail(g, f, f.station, 'raid')).toBe(true);
    place(g, 40);
    expect(boats(g, 'JAP', 'Advance Submarine Fleet')).toBe(46);
    // (The row that sent every boat home in January 1943 is struck: the record's boats were at sea beside the landings. tests/far-japan-boats.test.ts)
    place(g, 41);
    expect([boats(g, 'JAP', 'Advance Submarine Fleet'), boats(g, 'JAP', '6th Fleet'), all(g, 'JAP')]).toEqual([46, 0, 55]);
  });

  it("a war that comes before the record's is read as the record's first month", () => {
    const g = war(grand(3), 12);
    place(g);
    expect([boats(g, 'USA', 'Central Pacific Submarine Fleet'), boats(g, 'USA', 'Asiatic Submarine Fleet'), boats(g, 'JAP', 'Advance Submarine Fleet'), boats(g, 'NED', 'East Indies Submarine Fleet')]).toEqual([22, 29, 46, 15]);
    expect(fleet(g, 'USA', 'Central Pacific Submarine Fleet')!.reach).toBe(5);
    // (Britain's first row is August 1943 in any war.)
    expect(fleet(g, 'UK', 'Ceylon Submarine Fleet')).toBeUndefined();
  });

  // The design director's ruling of 17:42Z, 22 November 2026 (on the far side's reading 9): a player never lacks a tool
  // the computer has. No staff places the player's boats after his first month, so no group carries a reach for him:
  // his boats from a base raid as far as his navy's group raids from that base in that half-year. One rule.
  it("the player's own boats raid as far from a base as his navy's group does from it that half-year, and the card says so", () => {
    const g = war(grand(1));
    g.human = 'USA';
    const pool = fleet(g, 'USA', 'Pacific Submarine Fleet')!;
    const G = NAVY.beats!.USA.groups[0];
    expect(g.regions[pool.base].name).toBe('Pearl Harbor');
    expect(pool.ships).toMatchObject({ ss: 22, bb: 0, cv: 0, ca: 0, dd: 0 });
    expect(pool.reach).toBeUndefined();
    const honshu = zoneOf(g, 'honshu');
    // December 1941, from Pearl Harbor: as far as the record's boats went off Honshu, and no farther on any other errand.
    const far = boatsReach(g, pool, G, 28);
    expect(far).toBeGreaterThan(NAVY.range);
    expect(ownBoatsReach(g, pool)).toBe(far);
    expect(fleetReach(g, pool)).not.toContain(honshu);
    expect(fleetReach(g, pool, false, true)).toContain(honshu);
    expect(sail(g, pool, honshu, 'patrol')).toBe(false);
    expect(sail(g, pool, honshu, 'raid')).toBe(true);
    expect([g.seas[pool.zone].key, pool.stance]).toEqual(['honshu', 'raid']);
    // The card lists those seas after the others, marked for raiding only.
    const seas = seaOptions(g, pool, 'USA');
    const only = seas.filter((s) => s.raidOnly).map((s) => s.zone);
    expect(only).toContain(honshu);
    expect(only.sort()).toEqual(fleetReach(g, pool, false, true).filter((z) => !fleetReach(g, pool).includes(z)).sort());
    expect(seas.filter((s) => !s.raidOnly).map((s) => s.zone)).toEqual(fleetReach(g, pool));
    // A surface ship in company: any fleet's reach. So the Asiatic Fleet's boats raid far only as a fleet of their own.
    pool.ships.ca = 1;
    expect(ownBoatsReach(g, pool)).toBe(NAVY.range);
    expect(fleetReach(g, pool, false, true)).toEqual(fleetReach(g, pool));
    expect(seaOptions(g, pool, 'USA').some((s) => s.raidOnly)).toBe(false);
    pool.ships.ca = 0;
    // From a base no group of his navy sails from: any fleet's reach.
    const sf = g.byName['San Francisco'];
    expect(sf).toBeTruthy();
    const keep = { base: pool.base, home: pool.home };
    pool.base = sf.id; pool.home = sf.seas[0];
    expect(ownBoatsReach(g, pool)).toBe(NAVY.range);
    Object.assign(pool, keep);
    // The half-year's list in force is read as the month stands: January 1943's list, and Midway only once the group has it (August 1942).
    g.turn = 41;
    expect(ownBoatsReach(g, pool)).toBe(boatsReach(g, pool, G, 41));
    const midway = g.byName.Midway;
    const at = { ...pool, base: midway.id, home: zoneOf(g, 'midway') };
    expect(ownBoatsReach(g, at)).toBe(boatsReach(g, at, G, 41));
    g.turn = 30;
    expect(ownBoatsReach(g, at)).toBe(NAVY.range);
    // A war that comes before the record's is read by the record's first month.
    g.turn = 20;
    expect(ownBoatsReach(g, pool)).toBe(boatsReach(g, pool, G, 28));
  });

  it("the computer's boats are as they were: a fleet no group's row places keeps any fleet's reach, and a group its own", () => {
    const g = war(grand(1));
    expect(g.human).toBeNull();
    const pool = fleet(g, 'USA', 'Pacific Submarine Fleet')!;
    expect(fleetReach(g, pool, false, true)).toEqual(fleetReach(g, pool));
    place(g);
    const group = fleet(g, 'USA', 'Central Pacific Submarine Fleet')!;
    expect(group.reach).toBe(boatsReach(g, group, NAVY.beats!.USA.groups[0], 28));
    expect(fleetReach(g, group, false, true)).toContain(zoneOf(g, 'honshu'));
    // The same group in the player's hands reads the same reach, though its own figure goes stale.
    g.human = 'USA';
    group.reach = 1;
    expect(fleetReach(g, group, false, true)).toContain(zoneOf(g, 'honshu'));
  });
});
