// The strait (strategy.json 'straits'; the design director, 22 November 2026, 09:20Z; after release 1). Every short
// crossing of the sea is water: a corps crosses by ship (its month, one of its nation's lifts, room on the far bank),
// a side that holds both banks crosses whoever holds the seas beyond, an attack across onto an enemy bank is a
// landing, and the test of strength reads the enemy's reinforcement over a crossing as it reads ships. Supply and
// the railways run over a crossing as they did. It shipped switched off; it was switched on in release 2's rules on
// 22 November 2026 (the design director's ruling of 19:12Z once its 96 wars held the gate, Brian's choice of 20:19Z).
// Off, a crossing is a land border, as in release 1.
//
// The rulings of 18:07Z and 18:19Z the same day, with the historian's rows: a crossing's own craft carry its record's
// busiest month to a bank the side holds, with no lift (Messina 7, Kerch 17, the Oresund 3.7, in divisions' worth);
// the record's sea lift stands in for a sealift of none at the crossings (the Soviet Union 2 in the Black Sea,
// Germany 5, France 1); the test of strength counts the force's own friends over a crossing as it counts the
// enemy's; the Dardanelles are the tenth crossing and the Kattegat is open sea; the Kerch railway bridge is a land
// link in its months while the Soviet Union holds both banks.

import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { STRENGTH, reckon } from '../src/campaign/ashore';
import { addCorps, assess, computeSupply, createGame, declareWar, moveCorps, railInto, resolveBattle } from '../src/campaign/game';
import { railOpen } from '../src/campaign/rails';
import { farGround } from '../src/campaign/marches';
import { NAVY, craftOf, liftLeft, strait, straitBars, straitCut, straitLift, whyNotCross } from '../src/campaign/navy';
import { blocked, whyBlocked } from '../src/campaign/rivers';
import { startOpen } from '../src/campaign/staff';
import { STRATEGY, straitsOn } from '../src/campaign/strategy';
import type { Corps, Game, Region } from '../src/campaign/types';

const start = (seed = 17): Game => startOpen(createGame(seed, null));
const on = (): void => { STRATEGY.straits!.on = true; };
const off = (): void => { STRATEGY.straits!.on = false; };
const SHIPPED = STRATEGY.straits!.on;
const corps = (g: Game, nat: string, at: string, n: number, kind: 'inf' | 'arm' = 'inf'): Corps[] => Array.from({ length: n }, (_, k) => addCorps(g, nat, `Test ${nat} ${at} ${k}`, kind, 2, at, {}));
/** Hands a province to a nation and clears the field there. */
const give = (g: Game, r: Region, nat: string): void => { for (const u of [...r.units]) { u.dead = true; } r.units.length = 0; r.ctrl = nat; };

describe('the strait: every short crossing is water', () => {
  const CRAFT = STRATEGY.straits!.craft;
  // Each test starts with the switch off (a crossing is a land border, as in release 1) and switches it on where it
  // reads the crossings: that is how each was written and measured, and it reads both sides of the key.
  beforeEach(off);
  afterEach(() => { STRATEGY.straits!.on = SHIPPED; STRATEGY.straits!.craft = CRAFT; });
  /** Reads a war with no craft counted at any crossing: every corps that crosses takes one of its nation's lifts. */
  const noCraft = (): void => { STRATEGY.straits!.craft = []; };

  it('ships switched on in release 2\'s rules, is the grand campaign\'s alone, and knows the game\'s own crossings', () => {
    expect(SHIPPED).toBe(true);
    const g = start();
    STRATEGY.straits!.on = SHIPPED;
    expect(straitsOn(g)).toBe(true);
    off();
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
    noCraft();
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

  it('a nation with no shipping crosses only where the crossing has craft of its own on the record', () => {
    const g = start();
    const dane = g.byName.Jutland.units.find((u) => u.nat === 'DEN')!, turk = g.byName.Istanbul.units.find((u) => u.nat === 'TUR')!;
    expect(g.nations.DEN.sealift).toBe(0);
    expect(g.nations.TUR.sealift).toBe(0);
    expect(blocked(g, dane, g.byName.Zealand)).toBe(false);
    expect(blocked(g, turk, g.byName.Bursa)).toBe(false);
    on();
    // The Turk has no ship, and neither the Bosporus nor the Dardanelles has a month of ferries on the record.
    expect(blocked(g, turk, g.byName.Bursa)).toBe(true);
    expect(whyBlocked(g, turk, g.byName.Bursa)).toMatch(/has no shipping/);
    expect(moveCorps(g, turk, g.byName.Bursa)).toBe(false);
    // No road of his for the months to come runs over the water either.
    expect(farGround(g, turk).has(g.byName.Bursa.id)).toBe(false);
    expect(farGround(g, turk).has(g.byName.Izmir.id)).toBe(false);
    // The Dane has no ship either, but the Great Belt has its train ferries: he crosses to his own far bank by them.
    expect(dane.divs * dane.str).toBeLessThanOrEqual(2.75);
    expect(blocked(g, dane, g.byName.Zealand)).toBe(false);
    expect(farGround(g, dane).has(g.byName.Zealand.id)).toBe(true);
    // With no craft counted there he does not cross at all.
    noCraft();
    const g2 = start();
    const dane2 = g2.byName.Jutland.units.find((u) => u.nat === 'DEN')!;
    expect(blocked(g2, dane2, g2.byName.Zealand)).toBe(true);
    expect(whyBlocked(g2, dane2, g2.byName.Zealand)).toMatch(/has no shipping/);
    expect(farGround(g2, dane2).has(g2.byName.Zealand.id)).toBe(false);
  });

  it('the Dardanelles are the tenth crossing and the Kattegat is open sea, and off the chart is as it was', () => {
    const g = start();
    const ist = g.byName.Istanbul, izmir = g.byName.Izmir, jut = g.byName.Jutland, got = g.byName.Gothenburg;
    const turk = ist.units.find((u) => u.nat === 'TUR')!;
    // Off: both are land borders like any other.
    expect(ist.nb).toContain(izmir.id);
    expect(jut.nb).toContain(got.id);
    expect(strait(g, ist.id, izmir.id)).toBe(false);
    expect(straitCut(g, jut.id, got.id)).toBe(false);
    expect(farGround(g, turk).has(izmir.id)).toBe(true);
    on();
    expect(strait(g, ist.id, izmir.id)).toBe(true);
    expect(strait(g, izmir.id, ist.id)).toBe(true);
    expect(straitCut(g, jut.id, got.id)).toBe(true);
    expect(straitCut(g, got.id, jut.id)).toBe(true);
    // (The Kattegat is not a crossing: no ferry and no lift takes a corps over it. It is a landing or nothing.)
    expect(strait(g, jut.id, got.id)).toBe(false);
    declareWar(g, 'GER', 'DEN');
    declareWar(g, 'GER', 'SWE');
    give(g, jut, 'GER');
    const wave = corps(g, 'GER', 'Jutland', 2);
    expect(blocked(g, wave[0], got)).toBe(true);
    expect(whyBlocked(g, wave[0], got)).toMatch(/Open sea lies between Jutland and Gothenburg/);
    expect(farGround(g, wave[0]).get(got.id)?.prev).not.toBe(jut.id);
    const line = assess(g, wave, got).lines.find((l) => l.label === 'Open sea')!;
    expect(line.effect).toMatch(/2 corps cannot come over it and take no part/);
  });

  it('cut means cut: no supply and no railway runs over the Kattegat, and both still run over a crossing', () => {
    const g = start();
    const jut = g.byName.Jutland, got = g.byName.Gothenburg, zea = g.byName.Zealand;
    declareWar(g, 'GER', 'DEN');
    declareWar(g, 'GER', 'SWE');
    give(g, jut, 'GER');
    give(g, zea, 'GER');
    give(g, got, 'GER');
    // (Gothenburg alone of Sweden is Germany's, and it is no harbour: its only way home by land is Jutland.)
    expect(got.port).toBeFalsy();
    expect(got.nb.filter((j) => g.regions[j].ctrl === 'GER').map((j) => g.regions[j].name)).toEqual(['Jutland']);
    // Off: the chart's border carries the stores and the trains, as it always did.
    computeSupply(g);
    expect(g.supply.GER[got.id]).toBeTruthy();
    expect(railOpen(g, 'GER', jut.id, got.id)).toBe(true);
    expect(railInto(g, 'GER', [jut.id])[got.id]).toBe(1);
    on();
    computeSupply(g);
    expect(g.supply.GER[jut.id]).toBeTruthy();
    expect(g.supply.GER[got.id]).toBeFalsy();
    expect(railOpen(g, 'GER', jut.id, got.id)).toBe(false);
    expect(railOpen(g, 'GER', got.id, jut.id)).toBe(false);
    expect(railInto(g, 'GER', [jut.id])[got.id]).toBe(0);
    // A crossing is not cut: the Great Belt carries supply and the train ferry as before.
    expect(g.supply.GER[zea.id]).toBeTruthy();
    expect(railOpen(g, 'GER', jut.id, zea.id)).toBe(true);
    expect(railInto(g, 'GER', [jut.id])[zea.id]).toBe(1);
  });

  it('a crossing\'s own craft carry its record\'s month to a bank the side holds, with no lift; the rest goes by ship', () => {
    const g = start();
    on();
    const sicily = g.byName.Sicily, calabria = g.byName.Calabria;
    // The record's busiest months, in divisions' worth; no month is on the record for the Bosporus.
    expect(craftOf(g, calabria.id, sicily.id)).toBe(7);
    expect(craftOf(g, g.byName.Crimea.id, g.byName.Novorossiysk.id)).toBe(17);
    expect(craftOf(g, g.byName.Zealand.id, g.byName.Scania.id)).toBe(3.7);
    expect(craftOf(g, g.byName.Jutland.id, g.byName.Zealand.id)).toBe(2.75);
    expect(craftOf(g, g.byName.Istanbul.id, g.byName.Bursa.id)).toBeUndefined();
    // Messina: three corps of two divisions go over by the ferries (six of the seven), and no lift is taken.
    const [a, b, c, d] = corps(g, 'ITA', 'Calabria', 4);
    for (const x of [a, b, c]) { expect(moveCorps(g, x, sicily)).toBe(true); expect(x.ap).toBe(0); }
    expect(liftLeft(g, 'ITA')).toBe(g.nations.ITA.sealift);
    // The ferries have no room for the fourth this month: it goes by one of Italy's ships.
    expect(moveCorps(g, d, sicily)).toBe(true);
    expect(liftLeft(g, 'ITA')).toBe(g.nations.ITA.sealift - 1);
    // A nation with no ships crosses by the craft alone: the Red Army over the Kerch strait to its own far bank.
    const red = corps(g, 'SOV', 'Novorossiysk', 1)[0];
    expect(g.nations.SOV.sealift).toBe(0);
    expect(moveCorps(g, red, g.byName.Crimea)).toBe(true);
    expect(red.reg).toBe(g.byName.Crimea.id);
    // Onto an enemy bank the craft carry nobody: that is a landing, and takes a lift.
    declareWar(g, 'ITA', 'UK');
    for (const k of Object.keys(g.flags)) if (k.startsWith('war:') && g.flags[k] === g.turn) g.flags[k] = g.turn - 1;
    g.turn++;
    give(g, sicily, 'UK');
    corps(g, 'UK', 'Sicily', 1);
    const wave = corps(g, 'ITA', 'Calabria', 2);
    resolveBattle(g, wave, sicily);
    expect(liftLeft(g, 'ITA')).toBe(g.nations.ITA.sealift - 2);
  });

  it('the record\'s sea lift stands in for a sealift of none at the crossings, on the seas it names', () => {
    const g = start();
    const crimea = g.byName.Crimea, novo = g.byName.Novorossiysk, jut = g.byName.Jutland, zea = g.byName.Zealand;
    declareWar(g, 'GER', 'SOV');
    for (const k of Object.keys(g.flags)) if (k.startsWith('war:') && g.flags[k] === g.turn) g.flags[k] = g.turn - 1;
    give(g, crimea, 'GER');
    corps(g, 'GER', 'Crimea', 1);
    const wave = corps(g, 'SOV', 'Novorossiysk', 4);
    on();
    // The Soviet Union: none by its sealift, two a month in the Black Sea by the record; none on the Baltic.
    expect(g.nations.SOV.sealift).toBe(0);
    expect(straitLift(g, 'SOV', novo.id, crimea.id)).toBe(2);
    expect(straitLift(g, 'SOV', zea.id, g.byName.Scania.id)).toBe(0);
    expect(straitLift(g, 'FRA', novo.id, crimea.id)).toBe(1);
    expect(straitLift(g, 'TUR', g.byName.Istanbul.id, g.byName.Bursa.id)).toBe(0);
    expect(straitLift(g, 'DEN', jut.id, zea.id)).toBe(0);
    // Two corps of the four attack over the Kerch strait; the other two have no ship.
    const line = assess(g, wave, crimea).lines.find((l) => l.label === 'The crossing')!;
    expect(line.effect).toMatch(/2 corps that cross/);
    expect(line.effect).toMatch(/2 corps have no shipping left this month and take no part/);
    expect(resolveBattle(g, wave, crimea).attackers.length).toBe(2);
    expect(whyNotCross(g, wave[3], crimea)).toBe('All the shipping is in use this month.');
    // Germany: five by the record against a sealift of four. The one over is the crossings' own. Over the Great Belt
    // to a bank it holds, the first corps goes by the train ferries (two of their 2.75 divisions' worth), the second
    // by the record's fifth lift, which leaves the four for Norway whole, and only the third takes one of the four.
    expect(straitLift(g, 'GER', jut.id, zea.id)).toBe(5);
    declareWar(g, 'GER', 'DEN');
    give(g, jut, 'GER');
    give(g, zea, 'GER');
    const [first, second, third] = corps(g, 'GER', 'Jutland', 3);
    expect(moveCorps(g, first, zea)).toBe(true);
    expect(moveCorps(g, second, zea)).toBe(true);
    expect(liftLeft(g, 'GER')).toBe(g.nations.GER.sealift);
    expect(moveCorps(g, third, zea)).toBe(true);
    expect(liftLeft(g, 'GER')).toBe(g.nations.GER.sealift - 1);
  });

  it('the Kerch railway bridge is a land link in its months, while the Soviet Union has held both banks since the spring', () => {
    const g = start();
    on();
    const crimea = g.byName.Crimea, novo = g.byName.Novorossiysk;
    const BR = STRATEGY.straits!.bridges!.find((B) => B.pair.includes('Crimea'))!;
    expect([BR.from, BR.to, BR.holds, BR.since]).toEqual([63, 66, 'SOV', 57]);
    expect(strait(g, crimea.id, novo.id)).toBe(true);
    const turn = g.turn;
    g.turn = 63;
    expect(strait(g, crimea.id, novo.id)).toBe(false);
    const red = corps(g, 'SOV', 'Novorossiysk', 1)[0];
    expect(straitBars(g, red, crimea)).toBe(false);
    g.turn = 66;
    expect(strait(g, novo.id, crimea.id)).toBe(false);
    // The ice takes it in February 1945: water again from March.
    g.turn = 67;
    expect(strait(g, crimea.id, novo.id)).toBe(true);
    // No bridge for anyone else, and none for a bank won back after the spring of 1944.
    g.turn = 64;
    crimea.since = 60;
    expect(strait(g, crimea.id, novo.id)).toBe(true);
    crimea.since = 0;
    crimea.ctrl = 'GER';
    expect(strait(g, crimea.id, novo.id)).toBe(true);
    crimea.ctrl = 'SOV';
    expect(strait(g, crimea.id, novo.id)).toBe(false);
    g.turn = turn;
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

  it('the test of strength counts the force\'s own friends over a crossing as it counts the enemy\'s', () => {
    const g = start();
    declareWar(g, 'ITA', 'UK');
    const sicily = g.byName.Sicily, calabria = g.byName.Calabria;
    give(g, sicily, 'UK');
    give(g, calabria, 'UK');
    const force = corps(g, 'UK', 'Sicily', 2);
    const friends = corps(g, 'UK', 'Calabria', 4);
    const off = reckon(g, 'UK', sicily, force)!;
    expect(off.over).toBeUndefined();
    on();
    const now = reckon(g, 'UK', sicily, force)!;
    // On: the corps beyond the water are not a march away. They come by the Messina ferries in the months after the
    // first (two months of seven divisions' worth carry all four), as far as the island has room beside the force.
    expect(now.ferried).toBe(Math.min(friends.length, 5 - force.length));
    expect(now.over!).toBeGreaterThan(0);
    expect(now.ours).toBeCloseTo(now.force + now.friends + now.follow + now.over!, 6);
    expect(now.ours).toBeLessThanOrEqual(off.ours + 1e-9);
  });
});
