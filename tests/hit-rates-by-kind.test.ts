// Bomber hit rates on ships by kind (release 2; the design director's rulings of 22 November 2026, 18:13Z and 18:27Z,
// on the historian's counts): each bomber that released counts at its kind's rate for its navy, by whether fighters were
// over the ships, and at anchor by the surprises' rows. Its own switch, off as shipped; off, the law is as it was.
import { describe, expect, it } from 'vitest';
import { assessAir, fly, shipHits, shipHitsBy, shipLaw, type ShipLaw } from '../src/campaign/air';
import { syncWings, wingsOf } from '../src/campaign/carriers';
import { createGame } from '../src/campaign/game';
import { HITS, deckMix, hitRate, hitRatesOn, hitRatesShipped, hitsLine, landMix, mixOf, setHitRates, strikeRates } from '../src/campaign/hitrates';
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

  it('gives torpedoes Japan 0.15 and America 0 until the Mk 13\'s month, then the pool; level bombing at sea nothing', () => {
    expect(hitRate('JAP', 'torpedo', true, false, MAY42)).toBe(0.15);
    expect(hitRate('USA', 'torpedo', true, false, JUN42)).toBe(0);
    expect(hitRate('USA', 'torpedo', true, false, 70)).toBe(0);
    const was = HITS.fixed.USA.torpedo;
    try {
      HITS.fixed.USA.torpedo = 60;
      expect(hitRate('USA', 'torpedo', true, false, 59)).toBe(0);
      expect(hitRate('USA', 'torpedo', true, false, 60)).toBe(HITS.pool.torpedo.covered);
    } finally { HITS.fixed.USA.torpedo = was; }
    for (const nat of ['JAP', 'USA', 'UK', 'GER']) expect(hitRate(nat, 'level', false, false, JUN42)).toBe(0);
  });

  it('takes the senior partner\'s row for kin: Manchukuo Japan\'s, Canada Britain\'s (the pool, Britain having none)', () => {
    expect(hitRate('MAN', 'dive', true, false, MAY42)).toBe(0.09);
    expect(hitRate('CAN', 'dive', true, false, MAY42)).toBe(HITS.pool.dive.covered);
  });

  it('reads ships at anchor by the surprises\' rows: torpedo 0.64, armour-piercing level 0.175, dive its row x 1.5', () => {
    expect(hitRate('JAP', 'torpedo', false, true, 28)).toBe(0.64);
    expect(hitRate('UK', 'torpedo', true, true, 15)).toBe(0.64);
    expect(hitRate('JAP', 'level', false, true, 28)).toBe(0.175);
    expect(hitRate('JAP', 'dive', false, true, 28)).toBeCloseTo(0.55 * 1.5, 10);
  });

  it('splits a carrier strike by the field\'s own deck mix, and a land group as level until the historian\'s shares come', () => {
    const d = { year: 1942, month: 6 };
    const usa = mixOf('USA', true, JUN42, false);
    for (const [k, s] of deckMix('USA', d, 'fleet')) expect(usa[k]).toBeCloseTo(s, 10);
    expect(landMix('GER', 40)).toEqual({ dive: 0, torpedo: 0, level: 1, mast: 0 });
    const was = HITS.land;
    try {
      HITS.land = { GER: [[0, 0.4, 0], [30, 0.3, 0.1]] };
      expect(landMix('GER', 10)).toEqual({ dive: 0.4, torpedo: 0, level: 0.6, mast: 0 });
      expect(landMix('GER', 30).torpedo).toBe(0.1);
      expect(landMix('GER', 30).level).toBeCloseTo(0.6, 10);
    } finally { HITS.land = was; }
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
    expect(h.kinds!.torpedo).toBeCloseTo(10 * 0.15, 6);
  });

  it('keeps no strike sinking more than so much of what it finds, and scales the hits by kind with it', () => {
    const r = strikeRates([{ nat: 'JAP', bmr: 400, deck: true }], 28, true, false)!;
    const h = shipHitsBy(law(r, true), 400);
    const sum = (h.kinds!.dive + h.kinds!.torpedo + h.kinds!.level + h.kinds!.mast);
    expect(sum).toBeCloseTo(h.hits, 6);
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
