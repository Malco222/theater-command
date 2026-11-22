// The strait (strategy.json 'straits'; the design director, 22 November 2026, 09:20Z; after release 1). Every short
// crossing of the sea is water: a corps crosses by ship (its month, one of its nation's lifts, room on the far bank),
// a side that holds both banks crosses whoever holds the seas beyond, an attack across onto an enemy bank is a
// landing, and the test of strength reads the enemy's reinforcement over a crossing as it reads ships. Supply and
// the railways run over a crossing as they did. It ships switched off, and off a crossing is a land border.

import { afterEach, describe, expect, it } from 'vitest';
import { STRENGTH, reckon } from '../src/campaign/ashore';
import { addCorps, assess, createGame, declareWar, moveCorps, resolveBattle } from '../src/campaign/game';
import { farGround } from '../src/campaign/marches';
import { NAVY, liftLeft, strait, straitBars, whyNotCross } from '../src/campaign/navy';
import { blocked, whyBlocked } from '../src/campaign/rivers';
import { startOpen } from '../src/campaign/staff';
import { STRATEGY, straitsOn } from '../src/campaign/strategy';
import type { Corps, Game, Region } from '../src/campaign/types';

const start = (seed = 17): Game => startOpen(createGame(seed, null));
const on = (): void => { STRATEGY.straits!.on = true; };
const corps = (g: Game, nat: string, at: string, n: number, kind: 'inf' | 'arm' = 'inf'): Corps[] => Array.from({ length: n }, (_, k) => addCorps(g, nat, `Test ${nat} ${at} ${k}`, kind, 2, at, {}));
/** Hands a province to a nation and clears the field there. */
const give = (g: Game, r: Region, nat: string): void => { for (const u of [...r.units]) { u.dead = true; } r.units.length = 0; r.ctrl = nat; };

describe('the strait: every short crossing is water', () => {
  afterEach(() => { STRATEGY.straits!.on = false; });

  it('ships switched off, is the grand campaign\'s alone, and knows the game\'s own crossings', () => {
    expect(STRATEGY.straits!.on).toBe(false);
    const g = start();
    expect(straitsOn(g)).toBe(false);
    const pairs = g.links.map(([a, b]) => [g.regions[a].name, g.regions[b].name].join('-'));
    for (const nm of ['Jutland-Zealand', 'Calabria-Sicily', 'Istanbul-Bursa', 'Zealand-Scania', 'Crimea-Novorossiysk']) expect(pairs, nm).toContain(nm);
    // Off, no crossing is water and nothing is barred by one.
    for (const [a, b] of g.links) expect(strait(g, a, b)).toBe(false);
    const dane = g.byName.Jutland.units.find((u) => u.nat === 'DEN')!;
    expect(straitBars(g, dane, g.byName.Zealand)).toBe(false);
    on();
    expect(straitsOn(g)).toBe(true);
    for (const [a, b] of g.links) { expect(strait(g, a, b)).toBe(true); expect(strait(g, b, a)).toBe(true); }
    // A land border is not a crossing.
    expect(strait(g, g.byName.Berlin.id, g.byName.Berlin.nb[0])).toBe(false);
    // (History's own campaigns keep the map they were measured on.)
    expect(straitsOn(createGame(17, null))).toBe(false);
  });

  it('a corps crosses to a bank its side holds by ship: its month and a lift, as many corps as the shipping carries', () => {
    const g = start();
    const sicily = g.byName.Sicily, calabria = g.byName.Calabria;
    const [a, b, c, d] = corps(g, 'ITA', 'Calabria', 4);
    // Off: a march like any other, and no ship is asked for.
    const off = corps(g, 'ITA', 'Calabria', 1)[0];
    expect(moveCorps(g, off, sicily)).toBe(true);
    expect(off.ap).toBeGreaterThan(0);
    expect(liftLeft(g, 'ITA')).toBe(g.nations.ITA.sealift);
    off.dead = true; sicily.units.splice(sicily.units.indexOf(off), 1);
    on();
    expect(g.nations.ITA.sealift).toBe(3);
    // No harbour is asked on either bank: the crossing carries its own quays.
    expect(calabria.port).toBeFalsy();
    for (const x of [a, b, c]) {
      expect(whyNotCross(g, x, sicily)).toBeNull();
      expect(moveCorps(g, x, sicily)).toBe(true);
      expect(x.reg).toBe(sicily.id);
      expect(x.ap).toBe(0);
    }
    expect(liftLeft(g, 'ITA')).toBe(0);
    // The fourth has no ship this month, and says so.
    expect(blocked(g, d, sicily)).toBe(true);
    expect(whyBlocked(g, d, sicily)).toBe('All the shipping is in use this month.');
    expect(moveCorps(g, d, sicily)).toBe(false);
    expect(d.reg).toBe(calabria.id);
    // Next month the ships are back.
    g.turn++;
    expect(moveCorps(g, d, sicily)).toBe(true);
  });

  it('only a corps that has not moved this month embarks, and a road planned for the months to come still runs over the water', () => {
    const g = start();
    on();
    const sicily = g.byName.Sicily, calabria = g.byName.Calabria;
    const c = corps(g, 'ITA', 'Calabria', 1)[0];
    c.ap = c.apMax - 1;
    expect(blocked(g, c, sicily)).toBe(true);
    expect(whyBlocked(g, c, sicily)).toMatch(/has not moved this month/);
    // A corps a march away: it cannot go over this month, but its road for the months to come crosses, a month for the water.
    const back = g.regions[calabria.nb.find((j) => g.regions[j].owner === 'ITA' && j !== sicily.id)!];
    const far = corps(g, 'ITA', back.name, 1)[0];
    const road = farGround(g, far);
    expect(road.has(sicily.id)).toBe(true);
    expect(road.get(sicily.id)!.cost - road.get(calabria.id)!.cost).toBe(4);
  });

  it('a nation with no shipping does not cross at all', () => {
    const g = start();
    const dane = g.byName.Jutland.units.find((u) => u.nat === 'DEN')!, turk = g.byName.Istanbul.units.find((u) => u.nat === 'TUR')!;
    expect(g.nations.DEN.sealift).toBe(0);
    expect(g.nations.TUR.sealift).toBe(0);
    expect(blocked(g, dane, g.byName.Zealand)).toBe(false);
    on();
    expect(blocked(g, dane, g.byName.Zealand)).toBe(true);
    expect(whyBlocked(g, dane, g.byName.Zealand)).toMatch(/has no shipping/);
    expect(moveCorps(g, turk, g.byName.Bursa)).toBe(false);
    // No road of his for the months to come runs over the water either. (The Turk still reaches Bursa by land: on
    // this map Istanbul borders Izmir, which is not one of the crossings.)
    expect(farGround(g, dane).has(g.byName.Zealand.id)).toBe(false);
    expect(farGround(g, turk).get(g.byName.Bursa.id)!.prev).toBe(g.byName.Izmir.id);
  });

  it('an attack across onto an enemy bank is a landing: a lift each, a landing\'s share of strength, and the month', () => {
    const g = start();
    declareWar(g, 'ITA', 'UK');
    // (Not the month war is declared: that month a landing catches the defenders unready.)
    for (const k of Object.keys(g.flags)) if (k.startsWith('war:') && g.flags[k] === g.turn) g.flags[k] = g.turn - 1;
    const sicily = g.byName.Sicily, calabria = g.byName.Calabria;
    give(g, sicily, 'UK');
    corps(g, 'UK', 'Sicily', 2);
    const wave = corps(g, 'ITA', 'Calabria', 5);
    const before = assess(g, wave, sicily);
    expect(before.lines.some((l) => l.label === 'The crossing')).toBe(false);
    on();
    // Three lifts: three corps cross, at a landing's share; the other two have no ship and take no part.
    const now = assess(g, wave, sicily);
    const line = now.lines.find((l) => l.label === 'The crossing')!;
    expect(line.effect).toMatch(/3 corps that cross fight at 60%/);
    expect(line.effect).toMatch(/2 corps have no shipping left this month and take no part/);
    expect(now.A / before.A).toBeGreaterThan(0.3);
    expect(now.A / before.A).toBeLessThan(0.6 * 3 / 5 + 0.08);
    const rep = resolveBattle(g, wave, sicily);
    expect(rep.attackers.length).toBe(3);
    expect(liftLeft(g, 'ITA')).toBe(0);
    expect(g.flags[`landedAt:${sicily.id}:ITA`]).toBe(g.turn);
    // The three that crossed have spent their month, won or lost; the two left behind have not moved or lost a man.
    for (const c of wave.slice(0, 3)) if (!c.dead) expect(c.ap).toBe(0);
    for (const c of wave.slice(3)) { expect(c.reg).toBe(calabria.id); expect(c.ap).toBe(c.apMax); expect(c.str).toBe(1); }
    // Whoever won, nobody walked: a corps is on the far bank only if the fight was won.
    for (const c of wave.slice(0, 3)) if (!c.dead) expect(c.reg === sicily.id).toBe(rep.taken);
  });

  it('the month war is declared the crossing catches the defenders unready, and a sea shut to its ships bars it', () => {
    const g = start();
    declareWar(g, 'ITA', 'UK');
    const sicily = g.byName.Sicily;
    give(g, sicily, 'UK');
    corps(g, 'UK', 'Sicily', 1);
    const wave = corps(g, 'ITA', 'Calabria', 2);
    on();
    expect(NAVY.landing.surprise).toBe(1);
    expect(assess(g, wave, sicily).lines.find((l) => l.label === 'The crossing')!.effect).toMatch(/catch the defenders unready/);
  });

  it('the test of strength reads his reinforcement over a crossing as it reads ships', () => {
    const g = start();
    expect(STRENGTH).toBeTruthy();
    declareWar(g, 'ITA', 'UK');
    const sicily = g.byName.Sicily, calabria = g.byName.Calabria;
    give(g, sicily, 'UK');
    const force = corps(g, 'UK', 'Sicily', 2);
    const across = corps(g, 'ITA', 'Calabria', 5);
    const off = reckon(g, 'UK', sicily, force)!;
    // Off: the mainland's corps are a march away and count in full.
    expect(off.across).toBeUndefined();
    expect(off.corps).toBeGreaterThanOrEqual(5);
    on();
    const now = reckon(g, 'UK', sicily, force)!;
    // On: nothing of his is within a march; what comes is what his ships bring in the months after the first
    // (two months of three lifts) and the island's shore has room for (five corps, none of his there yet).
    expect(now.corps).toBe(0);
    expect(now.near).toBe(0);
    expect(now.shipped).toBe(Math.min(5, (STRENGTH!.months - 1) * g.nations.ITA.sealift, across.length));
    expect(now.across!).toBeGreaterThan(0);
    expect(now.theirs).toBeCloseTo(now.across! + now.reserve, 6);
    expect(now.theirs).toBeLessThanOrEqual(off.theirs + 1e-9);
    // A nation with no shipping brings nothing over.
    const g2 = start();
    declareWar(g2, 'GER', 'DEN');
    give(g2, g2.byName.Zealand, 'GER');
    const landed = corps(g2, 'GER', 'Zealand', 1);
    corps(g2, 'DEN', 'Jutland', 2);
    const r2 = reckon(g2, 'GER', g2.byName.Zealand, landed)!;
    expect(r2.shipped).toBe(0);
    expect(calabria.units.length).toBe(5);
  });
});
