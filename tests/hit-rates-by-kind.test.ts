// Bomber hit rates on ships by kind (release 2; the design director's rulings of 22 November 2026, 18:13Z and 18:27Z,
// on the historian's counts): each bomber that released counts at its kind's rate for its navy, by whether fighters were
// over the ships, and at anchor by the surprises' rows. Its own switch, off as shipped; off, the law is as it was.
import { describe, expect, it } from 'vitest';
import { assessAir, fly, shipHits, shipHitsBy, shipLaw, type ShipLaw } from '../src/campaign/air';
import { syncWings, wingsOf } from '../src/campaign/carriers';
import { createGame } from '../src/campaign/game';
import { HITS, deckMix, hitRate, hitRatesOn, hitRatesShipped, hitsLine, kindTakesOf, landMix, mixOf, setHitRates, strikeRates, sunkByKind, takesOf } from '../src/campaign/hitrates';
import { atSea, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { AirFleet, Fleet, Game } from '../src/campaign/types';

// (turn = (year - 1939) x 12 + month - 8: May 1942 is 33, June 1942 is 34)
const MAY42 = 33, JUN42 = 34;

function britain(seed = 3): Game {
  const g = startOpen(createGame(seed, 'UK'));
  for (let m = 0; m < 12; m++) {
    let guard = 0;
    while (!humanUp(g) && guard++ < 200) stepAI(g);
    beginHuman(g);
    if (g.nations.UK.atWar.length && g.nations.GER.atWar.includes('UK')) return g;
    endHuman(g);
  }
  throw new Error('Britain never went to war');
}
const named = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;
function duel(seed = 3): { g: Game; foe: Fleet; strike: AirFleet } {
  const g = britain(seed);
  const home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet');
  home.ships.cv = 6;
  for (const f of [home, foe]) { f.zone = zoneOf(g, 'natl'); f.stance = 'patrol'; f.lane = undefined; expect(atSea(f)).toBe(true); }
  syncWings(g);
  return { g, foe, strike: wingsOf(g, home.id).find((w) => w.kind === 'bmr')! };
}

describe('the switch', () => {
  it('is off as shipped, and cannot ship on until the table is sourced (the land shares and the Mk 13\'s month)', () => {
    expect(HITS.on).toBe(false);
    expect(HITS.sourced).toBe(false);
    expect(hitRatesShipped()).toBe(false);
    const g = startOpen(createGame(3, 'UK'));
    expect(hitRatesOn(g)).toBe(false);
    setHitRates(g, true);
    expect(hitRatesOn(g)).toBe(true);
    expect(loadGame(saveGame(g))!.flags.hitKinds).toBe(1);
    const hist = createGame(3, null);
    setHitRates(hist, true);
    expect(hitRatesOn(hist)).toBe(false);
  });

  it('changes nothing while off: the law\'s single rate, naval skill and spoil stand', () => {
    const { g, foe, strike } = duel();
    const a = assessAir(g, [strike], 'fleet', foe.id);
    expect(a.rates).toBeUndefined();
    expect(shipLaw(a, 'fleet').rates).toBeUndefined();
    expect(shipHits(a, 'fleet', strike.bmr).kinds).toBeUndefined();
  });
});

describe('the table (the historian\'s rows, the design director\'s split)', () => {
  it('splits dive bombing by fighters overhead: Japan 0.55 open and 0.09 under them, America 0.15 under them', () => {
    expect(hitRate('JAP', 'dive', false, false, MAY42)).toBe(0.55);
    expect(hitRate('JAP', 'dive', true, false, MAY42)).toBe(0.09);
    expect(hitRate('USA', 'dive', true, false, JUN42)).toBe(0.15);
    // A row a navy lacks takes the pool (D): America with no fighters overhead, Italy anywhere.
    expect(hitRate('USA', 'dive', false, false, JUN42)).toBe(HITS.pool.dive.open);
    expect(hitRate('ITA', 'dive', true, false, JUN42)).toBe(HITS.pool.dive.covered);
  });

  it('gives torpedoes Japan 0.14 open (Force Z and Crace\'s cruisers) and 0.15 under fighters (the Kates), Britain 0.14 (Bismarck), America 0 until the Mk 13 (turn 61)', () => {
    expect(hitRate('JAP', 'torpedo', false, false, 28)).toBe(0.14);
    expect(hitRate('JAP', 'torpedo', true, false, MAY42)).toBe(0.15);
    expect(hitRate('UK', 'torpedo', false, false, 21)).toBe(0.14);
    expect(hitRate('UK', 'torpedo', true, false, 21)).toBe(0.14);
    // A navy without a row takes the pool: every counted case, 10.5 of 73 open and 2 of 13 covered.
    expect(hitRate('ITA', 'torpedo', false, false, JUN42)).toBe(0.14);
    expect(hitRate('ITA', 'torpedo', true, false, JUN42)).toBe(0.15);
    expect(hitRate('USA', 'torpedo', true, false, JUN42)).toBe(0);
    expect(HITS.fixed.USA.torpedo).toBe(61);
    expect(hitRate('USA', 'torpedo', true, false, 60)).toBe(0);
    expect(hitRate('USA', 'torpedo', true, false, 61)).toBe(HITS.pool.torpedo.covered);
    expect(hitRate('USA', 'torpedo', false, false, 61)).toBe(HITS.pool.torpedo.open);
  });

  it('gives level bombing at sea Japan 0.06 (Force Z and Crace\'s cruisers), America 0 (from high up), the rest the pool: 0.06 open, 0 under fighters', () => {
    expect(hitRate('JAP', 'level', false, false, 28)).toBe(0.06);
    expect(hitRate('JAP', 'level', true, false, 28)).toBe(0);
    expect(hitRate('USA', 'level', false, false, JUN42)).toBe(0);
    expect(hitRate('USA', 'level', true, false, JUN42)).toBe(0);
    expect(hitRate('GER', 'level', false, false, JUN42)).toBe(0.06);
    expect(hitRate('UK', 'level', true, false, JUN42)).toBe(0);
  });

  it('takes the senior partner\'s row for kin: Manchukuo Japan\'s, Canada Britain\'s (the pool where Britain has none)', () => {
    expect(hitRate('MAN', 'dive', true, false, MAY42)).toBe(0.09);
    expect(hitRate('CAN', 'dive', true, false, MAY42)).toBe(HITS.pool.dive.covered);
    expect(hitRate('AUS', 'torpedo', false, false, MAY42)).toBe(0.14);
  });

  it('reads ships at anchor by the surprises\' rows: torpedo 0.64, armour-piercing level 0.175, dive its row x 1.5', () => {
    expect(hitRate('JAP', 'torpedo', false, true, 28)).toBe(0.64);
    expect(hitRate('UK', 'torpedo', true, true, 15)).toBe(0.64);
    expect(hitRate('JAP', 'level', false, true, 28)).toBe(0.175);
    expect(hitRate('JAP', 'dive', false, true, 28)).toBeCloseTo(0.55 * 1.5, 10);
  });

  it('splits a carrier strike by the field\'s own deck mix', () => {
    const d = { year: 1942, month: 6 };
    const usa = mixOf('USA', true, JUN42, false);
    for (const [k, s] of deckMix('USA', d, 'fleet')) expect(usa[k]).toBeCloseTo(s, 10);
  });

  it('lays a land group\'s kind by its type where the game carries one: the B-17, He 111 and Lancaster level, the Stuka dive', () => {
    const level = { dive: 0, torpedo: 0, level: 1, mast: 0 };
    expect(landMix('USA', JUN42, 'B17')).toEqual(level);
    expect(landMix('GER', 40, 'He111')).toEqual(level);
    expect(landMix('UK', 45, 'Lancaster')).toEqual(level);
    expect(landMix('GER', 20, 'Stuka')).toEqual({ dive: 1, torpedo: 0, level: 0, mast: 0 });
    // The type wins over the nation's kinds: a German He 111 group in 1942 is level, though KG 26 carried torpedoes.
    expect(landMix('GER', 30, 'He111').torpedo).toBe(0);
  });

  it('splits Japan\'s plain land bombers by the record\'s count at Force Z (61 torpedo, 27 level), the only share found', () => {
    const jap = landMix('JAP', 28, 'Bomber:JAP');
    expect(jap.torpedo).toBeCloseTo(0.69, 10);
    expect(jap.level).toBeCloseTo(0.31, 10);
    // Manchukuo flies by Japan's, its senior partner.
    expect(landMix('MAN', 40, 'Bomber:MAN')).toEqual(jap);
  });

  it('counts a plain type by the historian\'s kinds for its nation (M), in equal shares while no share is found (D)', () => {
    // Italy 1942: torpedo, level and dive, a third each.
    const ita = landMix('ITA', JUN42, 'Bomber:ITA');
    for (const k of ['torpedo', 'level', 'dive'] as const) expect(ita[k]).toBeCloseTo(1 / 3, 10);
    expect(ita.mast).toBe(0);
    // The Soviet Union 1942: torpedo and dive (Pe-2), half each.
    expect(landMix('SOV', JUN42, 'Bomber:SOV')).toEqual({ dive: 0.5, torpedo: 0.5, level: 0, mast: 0 });
    // Britain before its heavy bombers: level, then torpedo Beauforts from autumn 1940, then torpedo alone from 1942.
    expect(landMix('UK', 10, 'Bomber:UK').level).toBe(1);
    expect(landMix('UK', 20, 'Bomber:UK')).toEqual({ dive: 0, torpedo: 0.5, level: 0.5, mast: 0 });
    expect(landMix('UK', 30, 'Bomber:UK').torpedo).toBe(1);
    // A nation with no row at all is level, as the law was.
    expect(landMix('ROM', JUN42, 'Bomber:ROM')).toEqual({ dive: 0, torpedo: 0, level: 1, mast: 0 });
  });
});

describe('the law with hit rates by kind', () => {
  const law = (r: NonNullable<ReturnType<typeof strikeRates>>, harbour = false): ShipLaw => ({
    harbour, naval: 1, spoil: 0.5, again: 1, found: 1, surprise: true, have: { bb: 40, cv: 40, ca: 80, dd: 160, ss: 0 }, fleets: [],
    rates: r.rates, mix: r.mix, land: r.land,
  });

  it('counts Midway as the record has it: dive bombers about 0.15 a bomber under the Japanese fighters, torpedoes none', () => {
    const r = strikeRates([{ nat: 'USA', bmr: 100, deck: true }], JUN42, false, true)!;
    const h = shipHitsBy(law(r), 100);
    expect(h.kinds!.dive).toBeCloseTo(75 * 0.15, 6);
    expect(h.kinds!.torpedo).toBe(0);
    expect(h.hits).toBeCloseTo(11.25, 6);
    // The spoil, naval skill and a surprise's factor are not read.
    expect(shipHitsBy({ ...law(r), spoil: 0, naval: 0.08 }, 100).hits).toBeCloseTo(h.hits, 10);
  });

  it('counts the air battle\'s drops by kind in place of the strike\'s mix', () => {
    const r = strikeRates([{ nat: 'JAP', bmr: 50, deck: true }], MAY42, false, false)!;
    const h = shipHitsBy(law(r), 30, { dive: 20, torpedo: 10 });
    expect(h.kinds!.dive).toBeCloseTo(20 * 0.55, 6);
    expect(h.kinds!.torpedo).toBeCloseTo(10 * 0.14, 6);
  });

  it('keeps no strike sinking more than so much of what it finds, and scales the hits by kind with it', () => {
    const r = strikeRates([{ nat: 'JAP', bmr: 400, deck: true }], 28, true, false)!;
    const h = shipHitsBy(law(r, true), 400);
    const sum = (h.kinds!.dive + h.kinds!.torpedo + h.kinds!.level + h.kinds!.mast);
    expect(sum).toBeCloseTo(h.hits, 6);
  });

  it('keeps the all-kind takes of 19:48Z as the reference: battleship 6.25 (7 at anchor), carrier 5.7, cruiser 8, destroyer 2.25', () => {
    expect(takesOf(false)).toEqual({ bb: 6.25, cv: 5.7, ca: 8, dd: 2.25, ss: 1 });
    expect(takesOf(true).bb).toBe(7);
  });

  it('lays the kind form\'s takes: bombs and torpedoes each against their own, from the record\'s pure cases', () => {
    const sea = kindTakesOf(false), anchor = kindTakesOf(true);
    expect(sea.bomb).toEqual({ bb: null, cv: 6.5, ca: 8, dd: 3, ss: 1 });
    expect(sea.torpedo).toEqual({ bb: 5.25, cv: 4, ca: 8, dd: 1.6667, ss: 1 });
    expect(anchor.bomb.bb).toBe(7);
    expect(anchor.torpedo.bb).toBe(6);
    expect(anchor.bomb.cv).toBe(6.5);
  });

  // The tests before code (air-battles/hitrates/form-aim-and-kind-1122.md; the design director's 19:54Z): each case's
  // bombs and torpedoes, and the ships the kind form says they sink, against the record.
  it('sinks the record\'s battleships at sea by torpedo, and none by bombs', () => {
    // Repulse 5 torpedoes and Prince of Wales 5.5, a bomb each: two sunk, by construction.
    expect(sunkByKind('bb', 1, 5, false) + sunkByKind('bb', 1, 5.5, false)).toBeCloseTo(2, 6);
    // No battleship at sea was sunk by bombs alone: bombs hit her but do not sink her.
    expect(sunkByKind('bb', 30, 0, false)).toBe(0);
  });

  it('sinks the carriers as the record has them: Yorktown afloat on 4 June, Lexington and Hornet sunk, all five together', () => {
    expect(sunkByKind('cv', 3, 2, false)).toBeCloseTo(0.96, 2); // Yorktown, 4 June: afloat
    expect(sunkByKind('cv', 3, 2, false)).toBeLessThan(1);
    expect(sunkByKind('cv', 2, 3.5, false)).toBeGreaterThan(1); // Lexington, sunk (1.18)
    expect(sunkByKind('cv', 3, 3, false)).toBeGreaterThan(1); // Hornet, sunk (1.21)
    // Taiho 1 torpedo, Shokaku 3.5, Lexington, Hornet, and Yorktown in the end (3 bombs, 4 torpedoes): 5 sunk.
    const five = [[0, 1], [0, 3.5], [2, 3.5], [3, 3], [3, 4]].reduce((s, [b, t]) => s + sunkByKind('cv', b, t, false), 0);
    expect(five).toBeCloseTo(5, 1);
    // A reading, not a fault: Taiho (0.25) and Shokaku (0.88) sank by fuel vapour and damage control, which the field does not know.
    expect(sunkByKind('cv', 0, 1, false)).toBeCloseTo(0.25, 6);
    expect(sunkByKind('cv', 0, 3.5, false)).toBeCloseTo(0.875, 6);
  });

  it('sinks battleships at anchor by both kinds: West Virginia sunk, Nevada beached, the four together over the record by a fifth', () => {
    expect(sunkByKind('bb', 7, 0, true)).toBeCloseTo(1, 6); // bombs 7 at anchor
    expect(sunkByKind('bb', 0, 6, true)).toBeCloseTo(1, 6); // torpedoes 6 (Oklahoma)
    expect(sunkByKind('bb', 2, 6.5, true)).toBeGreaterThan(1); // West Virginia (1.37)
    expect(sunkByKind('bb', 6, 1, true)).toBeCloseTo(1.02, 2); // Nevada, beached (half)
    // California (2 bombs, 2.5 torpedoes) comes to 0.70 against sunk: a reading, the four together 3.1 against 2.5.
    expect(sunkByKind('bb', 2, 2.5, true)).toBeCloseTo(0.70, 2);
    const four = [[2, 6.5], [2, 2.5], [6, 1]].reduce((s, [b, t]) => s + sunkByKind('bb', b, t, true), 0);
    expect(four).toBeCloseTo(3.1, 1);
  });

  it('sinks cruisers and destroyers by their own takes: Mikuma and Mogami one between them, Sims by 3 bombs, 3 by 5 torpedoes', () => {
    expect(sunkByKind('ca', 3, 0, false) + sunkByKind('ca', 5, 0, false)).toBeCloseTo(1, 6);
    expect(sunkByKind('ca', 0, 8, false)).toBeCloseTo(1, 6); // torpedoes on a cruiser: the all-kind take, a gap
    expect(sunkByKind('dd', 3, 0, false)).toBeCloseTo(1, 6);
    expect(sunkByKind('dd', 0, 5, false)).toBeCloseTo(3, 3);
  });

  it('keeps a strike\'s bombs and torpedoes in two jugs, each shared over the classes by the aim', () => {
    const weight = 40 * 2 + 40 * 3 + 80 * 1 + 160 * 0.5;
    // Midway: dive bombers only score under the Japanese fighters (America's torpedoes 0): no battleship at sea sinks.
    const r = strikeRates([{ nat: 'USA', bmr: 100, deck: true }], JUN42, false, true)!;
    const h = shipHitsBy(law(r), 100);
    expect(h.sunk.bb).toBe(0);
    expect(h.sunk.cv).toBeCloseTo((h.hits * 40 * 3) / weight / 6.5, 8);
    expect(h.sunk.ca).toBeCloseTo((h.hits * 80 * 1) / weight / 8, 8);
    expect(h.sunk.dd).toBeCloseTo((h.hits * 160 * 0.5) / weight / 3, 8);
    // Japan in the Coral Sea: torpedoes and dive bombs, each against its own take.
    const j = shipHitsBy(law(strikeRates([{ nat: 'JAP', bmr: 50, deck: true }], MAY42, false, false)!), 50);
    const bombs = j.kinds!.dive + j.kinds!.level + j.kinds!.mast, torps = j.kinds!.torpedo;
    expect(torps).toBeGreaterThan(0);
    expect(j.sunk.bb).toBeCloseTo((torps * 40 * 2) / weight / 5.25, 8);
    expect(j.sunk.cv).toBeCloseTo((bombs * 40 * 3) / weight / 6.5 + (torps * 40 * 3) / weight / 4, 8);
    expect(j.sunk.dd).toBeCloseTo((bombs * 160 * 0.5) / weight / 3 + (torps * 160 * 0.5) / weight / 1.6667, 8);
    // At anchor (Pearl, turn 28), bombs sink battleships too.
    const at = shipHitsBy(law(strikeRates([{ nat: 'JAP', bmr: 50, deck: true }], 28, true, false)!, true), 50);
    const ab = at.kinds!.dive + at.kinds!.level + at.kinds!.mast, atT = at.kinds!.torpedo;
    expect(at.sunk.bb).toBeCloseTo((ab * 40 * 2) / weight / 7 + (atT * 40 * 2) / weight / 6, 8);
  });

  it('reads a strike on a fleet with the switch on, and says the hits by kind on the raid card', () => {
    const { g, foe, strike } = duel();
    setHitRates(g, true);
    const a = assessAir(g, [strike], 'fleet', foe.id);
    expect(a.rates).toBeDefined();
    expect(a.rates!.covered).toBe(a.D > 0);
    const exp = shipHits(a, 'fleet', strike.bmr);
    expect(exp.kinds).toBeDefined();
    const rep = fly(g, 'UK', 'fleet', foe.id, [strike])!;
    expect(rep.result).toMatch(/Hits: (none|about \w+) from the (torpedo|dive) bombers/);
  });

  it('words the hits only for the kinds that flew', () => {
    expect(hitsLine({ dive: 7.2, torpedo: 0 }, { dive: 48, torpedo: 41 })).toBe('Hits: about 7 from the dive bombers, none from the torpedo bombers.');
    expect(hitsLine({ dive: 1.2 }, { dive: 10, level: 0 })).toBe('Hits: about one from the dive bombers.');
  });
});
