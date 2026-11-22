// Ruling 240, C3 (Brian, 21 November 2026, "yes to all 3"; release 2): Damage control, a naval research. A fleet whose
// nation holds it takes strike.dc times the hits to sink: its surface warships only, wherever warships are sunk.
// Every nation alike: the rule reads research only. It stands behind the switch research.dc in carriers.json, OFF as
// shipped; with it off, and in every history campaign whatever it says, the game is release 1 to the byte.
// No rule here adds, removes or reorders a draw: a ship saved is found from the same single draw that decides the sinking.
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { Rng, hashString } from '../src/core/rng';
import { CARRIERS, assessAir, fly, forecastAir, shipHits, type AirAssess } from '../src/campaign/air';
import { planRaid, shippingWords, shipsLose } from '../src/campaign/airwar';
import page from '../src/campaign/ui/main.ts?raw';
import bridge from '../unreal/TheaterCommand/Rules/campaign-bridge.ts?raw';
import { syncWings, wingsOf } from '../src/campaign/carriers';
import { bombard, planBombard } from '../src/campaign/fleets';
import { createGame, relocate, setWar } from '../src/campaign/game';
import { FLEETS, NAVY, afloatWatch, atSea, fleetsNamed, keptAfloat, keptAfloatPast, lossOf, navalUpkeep, sail, seaBattles, shipCount, shipList, sinkShips, torpedoRisk, toughOf, zoneOf } from '../src/campaign/navy';
import { PLANS } from '../src/campaign/plans';
import { loadGame, saveGame } from '../src/campaign/save';
import { lossAt, planSeaBattle, shares } from '../src/campaign/seaplan';
import { startOpen } from '../src/campaign/staff';
import { SHIP_RESEARCH, dcOf, techOf } from '../src/campaign/tech';
import { beginHuman, endHuman, humanUp, orderFleet, runTurnAI, stepAI } from '../src/campaign/turn';
import type { AfloatTold } from '../src/campaign/navy';
import type { AirFleet, Fleet, Game, ShipClass, Ships } from '../src/campaign/types';

// The switch is flipped in memory and put back after every test (as tests/wings-abroad.test.ts does with its own).
const R = SHIP_RESEARCH()!;
let was = { stage: false, vt: false, dc: false };
beforeEach(() => { was = { stage: R.stage.on, vt: R.vt.on, dc: R.dc.on }; });
afterEach(() => { R.stage.on = was.stage; R.vt.on = was.vt; R.dc.on = was.dc; });

const DC = CARRIERS.strike.dc;
const CLASSES: ShipClass[] = ['bb', 'cv', 'ca', 'dd', 'ss'];
const SURFACE: ShipClass[] = ['bb', 'cv', 'ca', 'dd'];
const noShips = (): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 });
const less = (a: Ships, b: Ships): Ships => { const out = noShips(); for (const c of CLASSES) out[c] = a[c] - b[c]; return out; };

/** Puts the research straight onto a nation's list, whatever the switch says (a save made with it on holds the id). */
const give = (g: Game, nat: string, id = 'damagecontrol'): void => { const s = techOf(g, nat)!; if (!s.done.includes(id)) s.done.push(id); };
const named = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;
const wing = (g: Game, f: Fleet, kind: 'ftr' | 'bmr'): AirFleet => wingsOf(g, f.id).find((w) => w.kind === kind)!;
const copy = (g: Game): Game => loadGame(saveGame(g))!;

/**
 * Runs `fn` and says, for every stream of dice it drew from (named by the state it began in), how many draws were
 * made and the state it was left in.
 */
function dice<T>(fn: () => T): { out: T; streams: Map<number, { n: number; end: number }> } {
  const next = Rng.prototype.next;
  const seen = new Map<Rng, { start: number; n: number }>();
  Rng.prototype.next = function (this: Rng): number {
    let s = seen.get(this);
    if (!s) seen.set(this, (s = { start: this.state, n: 0 }));
    s.n++;
    return next.call(this);
  };
  try {
    const out = fn();
    const streams = new Map<number, { n: number; end: number }>();
    for (const [rng, s] of seen) streams.set(s.start, { n: s.n, end: rng.state });
    return { out, streams };
  } finally { Rng.prototype.next = next; }
}
/** The state a keyed stream begins in (src/core/rng.ts). */
const startOf = (key: string): number => hashString(key) >>> 0 || 0x9e3779b9;

// ---- The fixture of tests/carriers.test.ts: Britain at the table, at war with Germany -----------------------------

/** A grand campaign with Britain at the table, played until Britain is at war with Germany and it is her month to give orders. */
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
/** Puts a fleet in a sea, on patrol, whatever its reach: the tests place the pieces. */
function put(g: Game, f: Fleet, key: string): void {
  f.zone = zoneOf(g, key); f.stance = 'patrol'; f.lane = undefined;
  expect(atSea(f)).toBe(true);
  syncWings(g);
}
/** The Home Fleet with six carriers, and the German Battle Fleet (2 battleships, 8 cruisers, 20 destroyers) with it in the open Atlantic. */
function duel(seed = 3): { g: Game; home: Fleet; foe: Fleet; strike: AirFleet } {
  const g = britain(seed);
  const home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet');
  home.ships.cv = 6;
  put(g, home, 'natl');
  put(g, foe, 'natl');
  return { g, home, foe, strike: wing(g, home, 'bmr') };
}
/** The same, with the German Battle Fleet in its harbour, Italy's 2nd Fleet beside it and the Home Fleet's carriers off it. */
function harbour(seed = 3): { g: Game; foe: Fleet; ally: Fleet; port: number; strike: AirFleet } {
  const g = britain(seed);
  const home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet'), ally = named(g, '2nd Fleet');
  home.ships.cv = 6;
  sail(g, foe, foe.home, 'port');
  expect(atSea(foe)).toBe(false);
  const port = foe.base;
  setWar(g, 'UK', ally.nat);
  ally.base = port; ally.stance = 'port';
  home.zone = g.seas.find((z) => z.coast.includes(port))!.id; home.stance = 'patrol'; syncWings(g);
  return { g, foe, ally, port, strike: wing(g, home, 'bmr') };
}

/** Release 1's figures for 120 bombers through to the German Battle Fleet at sea (worked by the code at fcfb7711). */
const R1_HITS = { hits: 4.8, sunk: { bb: 0.34909090909090906, cv: 0, ca: 1.7454545454545454, dd: 3.116883116883117, ss: 0 }, count: 5.211428571428572, value: 153.35064935064935 };

describe('ruling 240, C3: the dice', () => {
  it('rolls two expectations on one draw: the first is roll\'s own number and leaves roll\'s own state', () => {
    for (let k = 1; k <= 400; k++) {
      const x = (k * 0.137) % 7, seed = 1000 + k;
      const a = new Rng(seed), b = new Rng(seed), c = new Rng(seed);
      const plain = a.roll(x);
      // Equal expectations: the same number twice, and the stream where roll leaves it.
      expect(b.rollBoth(x, x)).toEqual([plain, plain]);
      expect(b.state).toBe(a.state);
      // The eased figure beside release 1's: the first is still what roll makes of it, on the one draw.
      const eased = new Rng(seed).roll(x / DC);
      const [n, n0] = c.rollBoth(x / DC, x);
      expect(n).toBe(eased);
      expect(n0).toBe(plain);
      expect(c.state).toBe(a.state);
      // A smaller expectation never gives the larger count.
      expect(n).toBeLessThanOrEqual(n0);
    }
  });
});

describe('ruling 240, C3: the line for a ship that was saved', () => {
  const home = { name: 'Home Fleet' }, force = { name: 'Force H' };
  it('is the UI director\'s sentence: a count that opens it is a word, a count inside it a figure, a count of one "a", and no ship has a pronoun', () => {
    expect(keptAfloat(home, { ...noShips(), ca: 1 })).toBe('A cruiser of the Home Fleet is badly hit, but the repair parties keep the ship afloat.');
    expect(keptAfloat(home, { ...noShips(), dd: 2 })).toBe('Two destroyers of the Home Fleet are badly hit, but the repair parties keep the ships afloat.');
    expect(keptAfloat(home, { ...noShips(), ca: 1, dd: 2 })).toBe('A cruiser and 2 destroyers of the Home Fleet are badly hit, but the repair parties keep the ships afloat.');
    // Rider 2 (22 November): a count of one is "a" anywhere.
    expect(keptAfloat(home, { ...noShips(), bb: 1, ca: 1 })).toBe('A battleship and a cruiser of the Home Fleet are badly hit, but the repair parties keep the ships afloat.');
    expect(keptAfloat(home, { bb: 1, cv: 1, ca: 3, dd: 1, ss: 0 })).toBe('A battleship, a carrier, 3 cruisers and a destroyer of the Home Fleet are badly hit, but the repair parties keep the ships afloat.');
    expect(keptAfloat(home, { ...noShips(), dd: 2, ca: 0, bb: 0, cv: 1 })).toBe('A carrier and 2 destroyers of the Home Fleet are badly hit, but the repair parties keep the ships afloat.');
    expect(keptAfloat(home, { ...noShips(), dd: 12 })).toBe('12 destroyers of the Home Fleet are badly hit, but the repair parties keep the ships afloat.');
    // A fleet that takes no article keeps none.
    expect(keptAfloat(force, { ...noShips(), bb: 1 })).toBe('A battleship of Force H is badly hit, but the repair parties keep the ship afloat.');
    // Right after a sentence that names the fleet (riders 2 and 3): the fleet is named once.
    expect(keptAfloat(home, { ...noShips(), ca: 1 }, 'its')).toBe('One of its cruisers is badly hit, but the repair parties keep the ship afloat.');
    expect(keptAfloat(home, { ...noShips(), dd: 2 }, 'its')).toBe('Two of its destroyers are badly hit, but the repair parties keep the ships afloat.');
    // With another sentence between (rider 3, 1): "the fleet's".
    expect(keptAfloat(home, { ...noShips(), ca: 1 }, "fleet's")).toBe("One of the fleet's cruisers is badly hit, but the repair parties keep the ship afloat.");
    expect(keptAfloat(home, { ...noShips(), dd: 2 }, "fleet's")).toBe("Two of the fleet's destroyers are badly hit, but the repair parties keep the ships afloat.");
    // Several classes, without the name (rider 3, 2): "of the fleet" once, not "of its" twice; a count of one stays "a".
    for (const form of ['its', "fleet's"] as const) {
      expect(keptAfloat(home, { ...noShips(), ca: 1, dd: 2 }, form)).toBe('A cruiser and 2 destroyers of the fleet are badly hit, but the repair parties keep the ships afloat.');
      expect(keptAfloat(force, { ...noShips(), bb: 1, ca: 1 }, form)).toBe('A battleship and a cruiser of the fleet are badly hit, but the repair parties keep the ships afloat.');
    }
    // Addressed to the owner alone (passes as built, rider 3, 2).
    expect(keptAfloat(home, { ...noShips(), ca: 1 }, 'yours')).toBe('One of your cruisers is badly hit, but the repair parties keep the ship afloat.');
    expect(keptAfloat(home, { ...noShips(), dd: 2 }, 'yours')).toBe('Two of your destroyers are badly hit, but the repair parties keep the ships afloat.');
    expect(keptAfloat(home, { ...noShips(), ca: 1, dd: 2 }, 'yours')).toBe('One of your cruisers and 2 of your destroyers are badly hit, but the repair parties keep the ships afloat.');
    expect(keptAfloat(home, { ...noShips(), ca: 1, dd: 1 }, 'yours')).toBe('One of your cruisers and one of your destroyers are badly hit, but the repair parties keep the ships afloat.');
    // In the past, after a sentence in the past (rider 3: the line takes the tense of the sentence before it).
    expect(keptAfloat(home, { ...noShips(), ca: 1 }, 'fleet', true)).toBe('A cruiser of the Home Fleet was badly hit, but the repair parties kept the ship afloat.');
    expect(keptAfloat(home, { ...noShips(), ca: 1, dd: 2 }, 'fleet', true)).toBe('A cruiser and 2 destroyers of the Home Fleet were badly hit, but the repair parties kept the ships afloat.');
    expect(keptAfloat(home, { ...noShips(), dd: 2 }, 'its', true)).toBe('Two of its destroyers were badly hit, but the repair parties kept the ships afloat.');
    // (The third argument as it was before the forms had names: true for the owner, false for the fleet.)
    expect(keptAfloat(home, { ...noShips(), dd: 2 }, true)).toBe(keptAfloat(home, { ...noShips(), dd: 2 }, 'yours'));
    expect(keptAfloat(home, { ...noShips(), dd: 2 }, false)).toBe(keptAfloat(home, { ...noShips(), dd: 2 }));
    // Nothing saved, nothing said.
    for (const form of ['fleet', 'its', "fleet's", 'yours'] as const) expect(keptAfloat(home, noShips(), form)).toBe('');
    // No pronoun for a ship: "its" stands only for the fleet, in "of its".
    for (const s of [keptAfloat(home, { ...noShips(), cv: 1 }), keptAfloat(home, { ...noShips(), cv: 1 }, 'yours')]) expect(s).not.toMatch(/\b(she|her|it|its)\b/i);
    for (const s of [keptAfloat(home, { ...noShips(), cv: 1 }, 'its'), keptAfloat(home, { ...noShips(), cv: 2, dd: 1 }, 'its'), keptAfloat(home, { ...noShips(), cv: 1 }, "fleet's")]) { expect(s).not.toMatch(/\b(she|her|it)\b/i); expect(s.replace(/\bof its\b/g, '')).not.toMatch(/\bits\b/i); }
  });

  it('names the fleets a strike hit, three and then "and N more" (rider 3, 3)', () => {
    const f = (...ns: string[]): { name: string }[] => ns.map((name) => ({ name }));
    expect(fleetsNamed(f('Home Fleet'))).toBe('the Home Fleet');
    expect(fleetsNamed(f('Home Fleet', 'Mediterranean Fleet'))).toBe('the Home Fleet and the Mediterranean Fleet');
    expect(fleetsNamed(f('Home Fleet', 'Force H', '2nd Fleet'))).toBe('the Home Fleet, Force H and the 2nd Fleet');
    expect(fleetsNamed(f('Home Fleet', 'Force H', '2nd Fleet', 'Atlantic Fleet'))).toBe('the Home Fleet, Force H, the 2nd Fleet and 1 more');
    expect(fleetsNamed(f('Home Fleet', 'Force H', '2nd Fleet', 'Atlantic Fleet', 'Mediterranean Fleet'))).toBe('the Home Fleet, Force H, the 2nd Fleet and 2 more');
  });

  it('in a strike\'s report that sank none, is told in the past, after release 1\'s sentence (rider 2)', () => {
    expect(keptAfloatPast({ ...noShips(), ca: 1 })).toBe('the repair parties kept a badly hit cruiser afloat');
    expect(keptAfloatPast({ ...noShips(), ca: 1, dd: 2 })).toBe('the repair parties kept a badly hit cruiser and 2 destroyers afloat');
    expect(keptAfloatPast({ ...noShips(), dd: 2 })).toBe('the repair parties kept 2 badly hit destroyers afloat');
    expect(keptAfloatPast({ ...noShips(), bb: 1, ca: 1, dd: 3 })).toBe('the repair parties kept a badly hit battleship, a cruiser and 3 destroyers afloat');
    expect(keptAfloatPast(noShips())).toBe('');
    expect(`Ships of the Home Fleet were hit, but none was sunk: ${keptAfloatPast({ ...noShips(), ca: 1 })}.`).toBe('Ships of the Home Fleet were hit, but none was sunk: the repair parties kept a badly hit cruiser afloat.');
  });
});

describe('ruling 240, C3: a strike on ships, before the dice (shipHits)', () => {
  it('nobody holding it: release 1\'s figures to the digit, and the assessment has no new key', () => {
    const { g, foe, strike } = duel();
    const look = (): { a: AirAssess; hits: ReturnType<typeof shipHits> } => { const a = assessAir(g, [strike], 'fleet', foe.id); return { a, hits: shipHits(a, 'fleet', 120) }; };
    // As shipped.
    const off = look();
    expect(off.hits).toEqual(R1_HITS);
    expect(Object.keys(off.hits)).toEqual(['hits', 'sunk', 'count', 'value']);
    expect('tough' in off.a).toBe(false);
    // The switch on and nobody holding the research.
    R.dc.on = true;
    const none = look();
    expect(none.hits).toEqual(R1_HITS);
    expect(Object.keys(none.hits)).toEqual(['hits', 'sunk', 'count', 'value']);
    expect('tough' in none.a).toBe(false);
    // The research held and the switch off (a save made with it on): release 1.
    give(g, 'GER');
    R.dc.on = false;
    const held = look();
    expect(dcOf(g, 'GER')).toBe(1);
    expect(held.hits).toEqual(R1_HITS);
    expect('tough' in held.a).toBe(false);
    // The attacker's own research is nothing to the ships he strikes.
    R.dc.on = true;
    techOf(g, 'GER')!.done = techOf(g, 'GER')!.done.filter((id) => id !== 'damagecontrol');
    give(g, 'UK');
    expect(look().hits).toEqual(R1_HITS);
  });

  it('a fleet that holds it: each surface class sinks 1/dc as often, the hits are the same, and submarines are not eased', () => {
    const { g, foe, strike } = duel();
    R.dc.on = true;
    const plain = shipHits(assessAir(g, [strike], 'fleet', foe.id), 'fleet', 120);
    give(g, 'GER');
    expect(dcOf(g, 'GER')).toBe(DC);
    const a = assessAir(g, [strike], 'fleet', foe.id);
    expect(a.tough).toEqual([DC]);
    const eased = shipHits(a, 'fleet', 120);
    expect(eased.hits).toBe(plain.hits);
    for (const c of SURFACE) expect(eased.sunk[c], c).toBeCloseTo(plain.sunk[c] / DC, 12);
    expect(eased.sunk.dd).toBeGreaterThan(0);
    expect(eased.count).toBeCloseTo(plain.count / DC, 12);
    expect(eased.value).toBeCloseTo(plain.value / DC, 9);
    expect(eased.plain).toEqual(plain.sunk);
    expect(eased.by).toHaveLength(1);
    for (const c of CLASSES) expect(eased.by![0][c], c).toBe(eased.sunk[c]);
    // Submarines: nothing aims at them from the air (strike.aim.ss is 0); were anything to, the figure is not divided.
    const aim = CARRIERS.strike.aim.ss;
    try {
      CARRIERS.strike.aim.ss = 1;
      foe.ships.ss = 10;
      const p = shipHits({ ...a, tough: undefined }, 'fleet', 120), e = shipHits(a, 'fleet', 120);
      expect(p.sunk.ss).toBeGreaterThan(0);
      expect(e.sunk.ss).toBe(p.sunk.ss);
      expect(e.sunk.dd).toBeCloseTo(p.sunk.dd / DC, 12);
    } finally { CARRIERS.strike.aim.ss = aim; foe.ships.ss = 0; }
    // The forecast and the staffs' choice of a strike read the same figures.
    const fc = forecastAir(g, a, 'fleet', foe.id);
    techOf(g, 'GER')!.done = techOf(g, 'GER')!.done.filter((id) => id !== 'damagecontrol');
    const before = forecastAir(g, assessAir(g, [strike], 'fleet', foe.id), 'fleet', foe.id);
    expect(fc.hits).toBe(before.hits);
    expect(fc.bomberLoss).toBe(before.bomberLoss);
    for (const c of SURFACE) expect(fc.ships![c], c).toBeCloseTo(before.ships![c] / DC, 12);
    expect(fc.worth).toBeLessThan(before.worth);
    expect(fc.cut).toBeCloseTo(before.cut / DC, 12);
  });

  it('a harbour of two navies, one holding it: the navy without it loses what it would have, to the last digit', () => {
    const { g, foe, ally, port, strike } = harbour();
    expect([foe.nat, ally.nat]).toEqual(['GER', 'ITA']);
    R.dc.on = true;
    const a0 = assessAir(g, [strike], 'harbour', port);
    expect(a0.ships).toEqual([foe, ally]);
    const none = shipHits(a0, 'harbour', 100);
    const have = (c: ShipClass): number => foe.ships[c] + ally.ships[c];
    const share = (x: Fleet, c: ShipClass): number => (have(c) > 0 ? none.sunk[c] * (x.ships[c] / have(c)) : 0);
    for (const holder of ['GER', 'ITA']) {
      const h = copy(g);
      give(h, holder);
      const strike2 = wing(h, named(h, 'Home Fleet'), 'bmr');
      const a = assessAir(h, [strike2], 'harbour', port);
      expect(a.tough).toEqual(holder === 'GER' ? [DC, 1] : [1, DC]);
      const e = shipHits(a, 'harbour', 100);
      expect(e.hits).toBe(none.hits);
      expect(e.plain).toEqual(none.sunk);
      const [i, o] = holder === 'GER' ? [0, 1] : [1, 0];
      const mine = [foe, ally][i], other = [foe, ally][o];
      // The neighbour: bit for bit what it was. The holder: its own share over dc.
      for (const c of CLASSES) expect(e.by![o][c], `${holder} ${c}`).toBe(share(other, c));
      for (const c of SURFACE) expect(e.by![i][c], `${holder} ${c}`).toBeCloseTo(share(mine, c) / DC, 12);
      for (const c of CLASSES) expect(e.sunk[c], c).toBeCloseTo(e.by![0][c] + e.by![1][c], 12);
      expect(e.count).toBeLessThan(none.count);
    }
  });

  it('the cap is worked first, on the count without anyone\'s damage control, and only then is the holder eased', () => {
    const { g, foe, ally, port, strike } = harbour();
    R.dc.on = true;
    const most = CARRIERS.harbour.most * (shipCount(foe.ships, SURFACE) + shipCount(ally.ships, SURFACE));
    const none = shipHits(assessAir(g, [strike], 'harbour', port), 'harbour', 2000);
    expect(none.count).toBeCloseTo(most, 9);
    give(g, 'GER');
    const a = assessAir(g, [strike], 'harbour', port);
    const e = shipHits(a, 'harbour', 2000);
    // The hits, scaled down by the cap, and the capped class totals are release 1's.
    expect(e.hits).toBe(none.hits);
    expect(e.plain).toEqual(none.sunk);
    expect(shipCount(e.plain!)).toBeCloseTo(most, 9);
    // Italy beside it sinks as before; Germany's capped share is divided by dc, so damage control still saves ships.
    const have = (c: ShipClass): number => foe.ships[c] + ally.ships[c];
    const share = (x: Fleet, c: ShipClass): number => (have(c) > 0 ? none.sunk[c] * (x.ships[c] / have(c)) : 0);
    for (const c of SURFACE) {
      expect(e.by![1][c], c).toBe(share(ally, c));
      expect(e.by![0][c], c).toBeCloseTo(share(foe, c) / DC, 12);
    }
    expect(e.by![0].dd).toBeGreaterThan(0);
    expect(e.count).toBeLessThan(most);
    // A fleet alone under a capped strike: 1/dc of the cap.
    const d = duel();
    const cap1 = shipHits(assessAir(d.g, [d.strike], 'fleet', d.foe.id), 'fleet', 2000);
    expect(cap1.count).toBeCloseTo(CARRIERS.strike.most * shipCount(d.foe.ships, SURFACE), 9);
    give(d.g, 'GER');
    const eased = shipHits(assessAir(d.g, [d.strike], 'fleet', d.foe.id), 'fleet', 2000);
    expect(eased.count).toBeCloseTo(cap1.count / DC, 9);
    expect(eased.hits).toBe(cap1.hits);
  });
});

describe('ruling 240, C3: a strike flown (fly)', () => {
  it('draws the same dice with and without the research; the holder never loses more, loses about 1/dc as many, and is told once what was saved', () => {
    const { g, foe } = duel();
    R.dc.on = true;
    const snap = saveGame(g);
    const N = 60;
    let plainSunk = 0, easedSunk = 0, told = 0, sunkToo = 0, alone = 0;
    for (let k = 0; k < N; k++) {
      const salt = `:${k}`;
      const run = (holds: boolean): { sunk: Ships; result: string; news: string; ready: number; n: number; end: number; lost: number } => {
        const h = loadGame(snap)!;
        if (holds) give(h, 'GER');
        const f = named(h, 'Battle Fleet'), w = wing(h, named(h, 'Home Fleet'), 'bmr');
        const d = dice(() => fly(h, 'UK', 'fleet', f.id, [w], salt)!);
        const own = d.streams.get(startOf(`air:${h.seed}:${h.turn}:fleet:${f.id}:UK${salt}`))!;
        expect(own, 'the raid has its own stream').toBeDefined();
        return { sunk: d.out.ships![0].sunk, result: d.out.result, news: h.headlines[h.headlines.length - 1].text, ready: f.ready, n: own.n, end: own.end, lost: d.out.att[0].lostBmr };
      };
      const a = run(false), b = run(true);
      // The same draws, in number and in what they leave: nothing in the raid is thrown twice or left out.
      expect(b.n, `strike ${k}`).toBe(a.n);
      expect(b.end, `strike ${k}`).toBe(a.end);
      expect(b.lost).toBe(a.lost);
      // Hits are not changed, so the fleet is as shaken as it was.
      expect(b.ready).toBe(a.ready);
      for (const c of CLASSES) expect(b.sunk[c], `strike ${k} ${c}`).toBeLessThanOrEqual(a.sunk[c]);
      plainSunk += shipCount(a.sunk); easedSunk += shipCount(b.sunk);
      // What the same draw would have sunk and did not is what the report tells, once, in the ruled words.
      const saved = less(a.sunk, b.sunk);
      expect(a.result).not.toMatch(/repair parties/);
      if (shipCount(saved)) {
        told++;
        // Ships sunk beside them: the fleet the lead-in named is named once ("One of its cruisers is ..."). None sunk: one
        // sentence in the past (the UI director's rider 2 of 22 November).
        if (shipCount(b.sunk)) { sunkToo++; expect(b.result.endsWith(` ${keptAfloat(foe, saved, 'its')}`), `strike ${k}: ${b.result}`).toBe(true); }
        else { alone++; expect(b.result).toBe(`Ships of the Battle Fleet were hit, but none was sunk: ${keptAfloatPast(saved)}.`); }
        expect(b.result.match(/repair parties/g)).toHaveLength(1);
        expect(b.news).toContain(b.result);
        expect(b.result).not.toMatch(/little harm/);
      } else {
        expect(b.result).toBe(a.result);
        expect(b.news).toBe(a.news);
      }
    }
    // (Measured on these 60 strikes: 361 ships sunk without the research, 265 with it, and a ship saved in 58 of them.)
    expect(plainSunk).toBeGreaterThan(N * 3);
    expect(told).toBeGreaterThan(N / 2);
    expect(sunkToo).toBeGreaterThan(0);
    expect(alone + sunkToo).toBe(told);
    expect(easedSunk / plainSunk).toBeGreaterThan(1 / DC - 0.06);
    expect(easedSunk / plainSunk).toBeLessThan(1 / DC + 0.06);
  });

  it('in a harbour tells each fleet\'s saved ships in a line of its own, and never eases the navy that does not hold it', () => {
    const { g, foe, ally, port } = harbour();
    R.dc.on = true;
    g.flags['war:UK:GER'] = g.turn;
    const snap = saveGame(g);
    let both = 0, germans = 0, noneSunk = 0;
    for (let k = 0; k < 24; k++) {
      const salt = `:${k}`;
      const run = (...holders: string[]): { sunk: Ships[]; result: string; n: number; end: number } => {
        const h = loadGame(snap)!;
        for (const nat of holders) give(h, nat);
        const d = dice(() => fly(h, 'UK', 'harbour', port, [wing(h, named(h, 'Home Fleet'), 'bmr')], salt)!);
        const own = d.streams.get(startOf(`air:${h.seed}:${h.turn}:harbour:${port}:UK${salt}`))!;
        return { sunk: d.out.ships!.map((x) => x.sunk), result: d.out.result, n: own.n, end: own.end };
      };
      const none = run(), ger = run('GER'), all = run('GER', 'ITA');
      for (const x of [ger, all]) { expect(x.n).toBe(none.n); expect(x.end).toBe(none.end); }
      // Germany alone holding it: Italy's ships go down exactly as they did.
      expect(ger.sunk[1]).toEqual(none.sunk[1]);
      const savedG = less(none.sunk[0], ger.sunk[0]), savedI = less(none.sunk[1], all.sunk[1]);
      for (const c of CLASSES) { expect(savedG[c]).toBeGreaterThanOrEqual(0); expect(savedI[c]).toBeGreaterThanOrEqual(0); }
      expect(all.sunk[0]).toEqual(ger.sunk[0]);
      // Two fleets struck: with ships sunk, each fleet's saved ships in a line of its own that names it; with none sunk, one
      // sentence in the past for all of them (rider 2).
      const told = (x: { sunk: Ships[]; result: string }, kept: Ships[], fleets: Fleet[]): void => {
        const lines = fleets.map((f, i) => keptAfloat(f, kept[i])).filter(Boolean);
        if (!lines.length) return;
        const all = noShips(); for (const k of kept) for (const c of CLASSES) all[c] += k[c];
        if (x.sunk.some((s) => shipCount(s))) { expect(x.result.endsWith(` ${lines.join(' ')}`), x.result).toBe(true); expect((x.result.match(/repair parties/g) ?? []).length).toBe(lines.length); }
        // (Rider 3: the fleets by name, not "the 2 fleets".)
        else { noneSunk++; expect(x.result).toBe(`Ships of ${fleetsNamed(fleets)} were hit, but none was sunk: ${keptAfloatPast(all)}.`); }
      };
      if (shipCount(savedG)) { germans++; told(ger, [savedG, noShips()], [foe, ally]); expect(ger.result).not.toContain('2nd Fleet is'); }
      else expect(ger.result).toBe(none.result);
      if (shipCount(savedG) && shipCount(savedI)) both++;
      told(all, [savedG, savedI], [foe, ally]);
      if (!shipCount(savedG) && !shipCount(savedI)) expect(all.result).toBe(none.result);
    }
    expect(germans).toBeGreaterThan(8);
    expect(both).toBeGreaterThan(4);
  });

  it('names the fleets of a harbour strike that sank none, and leaves release 1\'s line where none was saved (rider 3, 3)', () => {
    const { g, foe, ally, port } = harbour();
    R.dc.on = true;
    g.flags['war:UK:GER'] = g.turn;
    const snap = saveGame(g);
    let named2 = 0;
    for (let n = 2; n <= 40 && named2 < 3; n += 2) for (let k = 0; k < 12 && named2 < 3; k++) {
      const run = (...holders: string[]): string => {
        const h = loadGame(snap)!;
        for (const nat of holders) give(h, nat);
        const w = wing(h, named(h, 'Home Fleet'), 'bmr');
        w.bmr = n;
        return fly(h, 'UK', 'harbour', port, [w], `:${n}:${k}`)!.result;
      };
      const plain = run(), held = run('GER', 'ITA');
      if (!/none was sunk: the repair parties kept/.test(held)) continue;
      named2++;
      expect(held, held).toMatch(new RegExp(`^Ships of ${fleetsNamed([foe, ally])} were hit, but none was sunk: the repair parties kept .* afloat\\.$`));
      expect(held.startsWith('Ships of the Battle Fleet and the 2nd Fleet were hit')).toBe(true);
      // Nobody holding it: release 1's sentence, "the 2 fleets", as it was.
      expect(plain).not.toMatch(/repair parties/);
      if (/none was sunk/.test(plain)) expect(plain).toBe('Ships of the 2 fleets were hit, but none was sunk.');
    }
    expect(named2).toBeGreaterThan(0);
  });

  it('every nation alike: the same fleet under any flag on the chart loses the same ships, by its own research or its senior partner\'s', () => {
    expect(PLANS.kin.CAN).toBe('UK');
    R.dc.on = true;
    const base = duel();
    const snap = saveGame(base.g);
    const nats = Object.keys(base.g.nations);
    expect(nats.length).toBeGreaterThan(40);
    // (Nobody begins with the research: a flag's result hangs on what the test gives it and on nothing else.)
    for (const nat of nats) expect(techOf(base.g, nat)!.done, nat).not.toContain('damagecontrol');
    // The fleet under one flag, the research held by nobody and then by `holder`: three strikes each.
    const under = (flag: string, holder: string): unknown[] => {
      const out: unknown[] = [];
      for (const holds of [false, true]) {
        for (let k = 0; k < 3; k++) {
          const h = loadGame(snap)!;
          const f = named(h, 'Battle Fleet'), w = wing(h, named(h, 'Home Fleet'), 'bmr');
          f.nat = flag;
          if (holds) give(h, holder);
          expect(dcOf(h, flag), `${flag} by ${holder} ${holds}`).toBe(holds ? DC : 1);
          const a = assessAir(h, [w], 'fleet', f.id);
          const rep = fly(h, 'UK', 'fleet', f.id, [w], `:${k}`)!;
          out.push([a.tough, shipHits(a, 'fleet', 120), rep.ships![0].sunk, rep.result]);
        }
      }
      return out;
    };
    const ger = under('GER', 'GER');
    expect(ger.some((x) => /repair parties/.test((x as [unknown, unknown, unknown, string])[3]))).toBe(true);
    // Every flag on the chart by its own research: a great power, a small one, a neutral, a dominion, the raider's own.
    for (const flag of nats) expect(under(flag, flag), flag).toEqual(ger);
    // A kin nation's ships have its senior partner's damage control as well (a dominion Britain's).
    const kin = nats.filter((k) => PLANS.kin[k] !== undefined && nats.includes(PLANS.kin[k]));
    expect(kin).toContain('CAN');
    for (const k of kin) expect(under(k, PLANS.kin[k]), `${k} by ${PLANS.kin[k]}`).toEqual(ger);
  });

  it('the switch off with the research held (a save made with it on): the strike is release 1\'s to the byte', () => {
    const { g } = duel();
    const snap = saveGame(g);
    const run = (salt: string, holds: boolean, on: boolean): string => {
      const h = loadGame(snap)!;
      R.dc.on = on;
      if (holds) give(h, 'GER');
      const f = named(h, 'Battle Fleet');
      fly(h, 'UK', 'fleet', f.id, [wing(h, named(h, 'Home Fleet'), 'bmr')], salt);
      // (The research list is the one difference the saves may have: it is taken off before they are compared.)
      techOf(h, 'GER')!.done = techOf(h, 'GER')!.done.filter((id) => id !== 'damagecontrol');
      return saveGame(h);
    };
    let moved = 0;
    for (let k = 0; k < 6; k++) {
      const r1 = run(`:${k}`, false, false);
      expect(run(`:${k}`, true, false), `strike ${k}`).toBe(r1);
      expect(run(`:${k}`, false, true), `strike ${k}`).toBe(r1);
      if (run(`:${k}`, true, true) !== r1) moved++;
    }
    // (And the rule does bite once both are there, or the three equalities above would prove nothing.)
    expect(moved).toBeGreaterThan(0);
  });
});

describe('ruling 240, C3: the older strike at shipping in a sea', () => {
  it('eases the fleet that holds it and not the fleet beside it, on the same draws, and says what was saved', () => {
    const { g, home, foe } = duel();
    const ally = named(g, '2nd Fleet');
    setWar(g, 'UK', ally.nat);
    put(g, ally, 'natl');
    R.dc.on = true;
    const zone = home.zone;
    const snap = saveGame(g);
    let plainSunk = 0, easedSunk = 0, told = 0;
    for (let k = 0; k < 120; k++) {
      const salt = `:${k}`;
      const run = (holds: boolean): { ger: Ships; ita: Ships; result: string; n: number; end: number } => {
        const h = loadGame(snap)!;
        if (holds) give(h, 'GER');
        const f = named(h, 'Battle Fleet'), i = named(h, '2nd Fleet');
        const before = [{ ...f.ships }, { ...i.ships }];
        const d = dice(() => fly(h, 'UK', 'strike', zone, [wing(h, named(h, 'Home Fleet'), 'bmr')], salt)!);
        const own = d.streams.get(startOf(`air:${h.seed}:${h.turn}:strike:${zone}:UK${salt}`))!;
        return { ger: less(before[0], f.ships), ita: less(before[1], i.ships), result: d.out.result, n: own.n, end: own.end };
      };
      const a = run(false), b = run(true);
      expect(b.n).toBe(a.n);
      expect(b.end).toBe(a.end);
      expect(b.ita).toEqual(a.ita);
      for (const c of CLASSES) expect(b.ger[c], `${k} ${c}`).toBeLessThanOrEqual(a.ger[c]);
      plainSunk += shipCount(a.ger); easedSunk += shipCount(b.ger);
      const saved = less(a.ger, b.ger);
      expect(a.result).not.toMatch(/repair parties/);
      if (shipCount(saved)) { told++; expect(b.result).toContain(keptAfloat(foe, saved)); expect(b.result.match(/repair parties/g)).toHaveLength(1); }
      else expect(b.result).toBe(a.result);
    }
    // (Measured on these 120 strikes: 58 ships sunk without the research, 36 with it, and a ship saved in 21 of them.)
    expect(plainSunk).toBeGreaterThan(40);
    expect(told).toBeGreaterThan(10);
    expect(easedSunk / plainSunk).toBeGreaterThan(1 / DC - 0.2);
    expect(easedSunk / plainSunk).toBeLessThan(1 / DC + 0.2);
  });
});

describe("ruling 240, C3: the older strike's forecast and the staffs' worth (the design director, 22 November 03:57Z; rider 4, 11:12Z)", () => {
  /** The Home Fleet's strike wing over the German Battle Fleet and Italy's 2nd Fleet in the open Atlantic. */
  function sea(): { g: Game; zone: number } {
    const { g, home } = duel();
    const ally = named(g, '2nd Fleet');
    setWar(g, 'UK', ally.nat);
    put(g, ally, 'natl');
    return { g, zone: home.zone };
  }
  const strikeWing = (h: Game): AirFleet => wing(h, named(h, 'Home Fleet'), 'bmr');
  const forecast = (h: Game, zone: number): ReturnType<typeof forecastAir> => forecastAir(h, assessAir(h, [strikeWing(h)], 'strike', zone), 'strike', zone);
  const KEYS = ['odds', 'bomberLoss', 'cut', 'nation', 'ftrLoss', 'bmrLoss', 'foeLoss', 'foeGuns', 'success', 'worth'] as const;
  const surface = (f: Fleet): number => SURFACE.reduce((n, c) => n + f.ships[c], 0);

  it("with the switch off is release 1's to the bit, whoever holds the research", () => {
    const { g, zone } = sea();
    const snap = saveGame(g);
    const run = (on: boolean, ...holders: string[]): ReturnType<typeof forecastAir> => {
      const h = loadGame(snap)!;
      R.dc.on = on;
      for (const nat of holders) give(h, nat);
      return forecast(h, zone);
    };
    const r1 = run(false);
    expect(r1.cut).toBeGreaterThan(0);
    for (const x of [run(false, 'GER', 'ITA'), run(false, 'UK')]) for (const k of KEYS) expect(Object.is(x[k], r1[k]), k).toBe(true);
    // (Release 1's expression: the staff's cut x every surface ship struck x 30, and the trade's hurt.)
    const h = loadGame(snap)!;
    const afloat = surface(named(h, 'Battle Fleet')) + surface(named(h, '2nd Fleet'));
    expect(afloat).toBeGreaterThan(20);
    expect(r1.worth - r1.cut * afloat * 30).toBeGreaterThanOrEqual(0);
  });

  it("counts each struck fleet's ships at the fight's own figure, class by class (rider 4), in the staffs' worth and in the card's share", () => {
    const { g, zone } = sea();
    const snap = saveGame(g);
    const h0 = loadGame(snap)!;
    const gf = named(h0, 'Battle Fleet'), it2 = named(h0, '2nd Fleet');
    const afloat = surface(gf) + surface(it2);
    // Release 1's forecast (the switch off): its share and the trade's part of the worth.
    const r1 = forecast(h0, zone);
    const trade = r1.worth - r1.cut * afloat * 30;
    // navy.json's tough for each class, over each fleet's own damage control: the figure the fight sinks by (dueAt).
    const T = (f: Fleet, dc: number): number => SURFACE.reduce((n, c) => n + (f.ships[c] * NAVY.classes[c].tough) / dc, 0);
    // (The German fleet holds battleships, weighed at 0.45, and destroyers, at 1.1: the weighed count is not the head count.)
    expect(gf.ships.bb).toBeGreaterThan(0);
    expect(T(gf, 1)).not.toBeCloseTo(surface(gf), 3);
    const run = (...holders: string[]): { h: Game; x: ReturnType<typeof forecastAir> } => {
      const h = loadGame(snap)!;
      R.dc.on = true;
      for (const nat of holders) give(h, nat);
      return { h, x: forecast(h, zone) };
    };
    for (const holders of [[], ['GER'], ['ITA'], ['GER', 'ITA']]) {
      const { h, x } = run(...holders);
      const fought = T(gf, holders.includes('GER') ? DC : 1) + T(it2, holders.includes('ITA') ? DC : 1);
      expect(x.worth, holders.join()).toBeCloseTo(r1.cut * fought * 30 + trade, 9);
      expect(x.cut, holders.join()).toBeCloseTo(r1.cut * (fought / afloat), 12);
      // The staff weighs what it fights: the forecast's ships are the fight's own expected loss at that share (lossOf,
      // the figure sinkShips rolls about), summed over the fleets and their surface classes.
      const due = [named(h, 'Battle Fleet'), named(h, '2nd Fleet')].reduce((n, f) => n + SURFACE.reduce((m, c) => m + lossOf(h, f, c, r1.cut), 0), 0);
      expect(x.cut * afloat, holders.join()).toBeCloseTo(due, 12);
      // Nothing else on the forecast moves but the chance, which weighs the worth.
      for (const k of ['odds', 'bomberLoss', 'ftrLoss', 'bmrLoss', 'foeLoss', 'foeGuns'] as const) expect(x[k], k).toBe(r1[k]);
    }
    // (A fleet of many destroyers can weigh more than its head count, one of battleships less: the worth is not always lower
    // than release 1's.) Damage control eases each holder's own fleet by its own figure.
    expect(run('GER', 'ITA').x.cut).toBeCloseTo(run().x.cut / DC, 12);
    // The card the player reads says the same share, in the UI director's words of rider 4 (22 November 12:53Z) with the
    // switch on: the forecast's own voice, an expected loss. Off, release 1's line to the letter.
    const card = (h: Game): string => planRaid(h, 'strike', zone, [strikeWing(h).id], 'UK').effects.find((e) => /^Warships caught at sea there /.test(e))!;
    const on = run('GER', 'ITA');
    expect(Math.round(on.x.cut * 100)).toBeGreaterThan(0);
    expect(card(on.h)).toBe(`Warships caught at sea there should lose about ${Math.round(on.x.cut * 100)}% of their number, and merchant ships in those waters will be sunk.`);
    expect(Math.round(on.x.cut * 100)).toBeLessThan(Math.round(r1.cut * 100));
    R.dc.on = false;
    expect(card(loadGame(snap)!)).toBe(`Warships caught at sea there could lose up to ${Math.round(r1.cut * 100)}% of their number, and merchant ships in those waters will be sunk.`);
  });

  it("says it in the UI director's words of rider 4 with the switch on, on every card, and release 1's with it off", () => {
    const { g } = sea();
    const line = (cut: number, waters?: 'those' | 'these'): string => shippingWords(g, cut, waters);
    R.dc.on = false;
    // Release 1's lines as each card had them: the raid card's "those waters", the air order's card's "these waters".
    expect(line(0.034)).toBe('Warships caught at sea there could lose up to 3% of their number, and merchant ships in those waters will be sunk.');
    expect(line(0.034, 'these')).toBe('Warships caught at sea there could lose up to 3% of their number, and merchant ships in these waters will be sunk.');
    expect(line(0.004)).toBe('Warships caught at sea there could lose up to 0% of their number, and merchant ships in those waters will be sunk.');
    expect(shipsLose(g, 0.034)).toBe('could lose up to 3%');
    R.dc.on = true;
    // The 3D game's air order page says the share in its own shorter line, with the same verb (campaign-bridge.ts raidPage).
    expect(shipsLose(g, 0.034)).toBe('should lose about 3%');
    expect(shipsLose(g, 0.004)).toBe('should lose almost none');
    expect(bridge).toContain('Warships caught at sea there ${shipsLose(s, fc.cut)} of their number.');
    expect(line(0.034)).toBe('Warships caught at sea there should lose about 3% of their number, and merchant ships in those waters will be sunk.');
    // "those waters" in both places, as the sentence has already said "there".
    expect(line(0.034, 'these')).toBe(line(0.034));
    // A share that rounds to nothing.
    expect(line(0.004)).toBe('Warships caught at sea there should lose almost none of their number, and merchant ships in those waters will be sunk.');
    expect(line(0.005)).toBe('Warships caught at sea there should lose about 1% of their number, and merchant ships in those waters will be sunk.');
    // The air order's card puts it after its bombers' line ("About 12% of the bombers are likely to be lost. ...").
    expect(page).toContain("About ${lossPct}% of the bombers are likely to be lost. ${shippingWords(g, fc.cut, 'these')}</div>");
    expect(page).not.toContain('could lose up to');
  });

  it("reads the fight's own figure: each fleet's easing is 1 / dcOf of its nation, as the fight's, under every flag on the chart", () => {
    const { g, zone } = sea();
    R.dc.on = true;
    const snap = saveGame(g);
    const nats = Object.keys(g.nations).filter((n) => n !== 'UK' && PLANS.kin[n] !== 'UK');
    expect(nats.length).toBeGreaterThan(40);
    let flags = 0;
    // The German fleet under one flag, at war with Britain; the research held by nobody and then by `holder`.
    const under = (flag: string, holder: string): [number, number] | null => {
      const out: ReturnType<typeof forecastAir>[] = [];
      for (const holds of [false, true]) {
        const h = loadGame(snap)!;
        const bf = named(h, 'Battle Fleet');
        // (The German fleet alone in that sea, so that no other fleet of the flag's is counted beside it.)
        for (const f of h.fleets) if (f.zone === zone && f !== bf && f.nat !== 'UK') f.stance = 'port';
        bf.nat = flag;
        setWar(h, 'UK', flag);
        if (!g.nations[flag] || !h.nations.UK.atWar.includes(flag)) return null;
        if (holds) give(h, holder);
        expect(dcOf(h, flag), `${flag} by ${holder}`).toBe(holds ? DC : 1);
        out.push(forecast(h, zone));
      }
      return [out[1].cut / out[0].cut, out[0].worth - out[1].worth];
    };
    const base = under('GER', 'GER')!;
    expect(base[1]).toBeGreaterThan(0);
    for (const flag of nats) {
      const x = under(flag, flag);
      if (!x) continue;
      flags++;
      expect(x[0], flag).toBeCloseTo(base[0], 12);
      expect(x[1], flag).toBeCloseTo(base[1], 9);
    }
    expect(flags).toBeGreaterThan(30);
    // A kin nation's fleet, by its senior partner's research (Vichy and Free France by France's).
    for (const k of Object.keys(PLANS.kin).filter((n) => PLANS.kin[n] !== 'UK' && g.nations[n])) {
      const x = under(k, PLANS.kin[k]);
      if (x) { expect(x[0], k).toBeCloseTo(base[0], 12); expect(x[1], k).toBeCloseTo(base[1], 9); }
    }
  });
});

// ---- The sea half: fleet actions, the forecast, the bombers' toll, shore batteries, torpedoes ---------------------

/** A grand campaign with nobody at the table, Britain at war with Germany, and the Home Fleet and the German Battle Fleet on patrol in the Norwegian Sea. */
function meet(seed: number, ...holders: string[]): { g: Game; home: Fleet; battle: Fleet; z: number } {
  const g = startOpen(createGame(seed, null));
  setWar(g, 'UK', 'GER');
  const home = named(g, 'Home Fleet'), battle = named(g, 'Battle Fleet');
  const z = zoneOf(g, 'norw');
  for (const f of [home, battle]) { f.zone = z; f.stance = 'patrol'; f.lane = undefined; }
  for (const nat of holders) give(g, nat);
  return { g, home, battle, z };
}
/** The news of the fleet action just fought. */
const actionNews = (g: Game): string => [...g.headlines].reverse().find((h) => /\bdefeats?\b/.test(h.text))!.text;
/** The stream of a fleet action begun by Britain in that sea. */
const battleKey = (g: Game, z: number): number => startOf(`sea:${g.seed}:${g.turn}:battle:${z}:UK`);

describe('ruling 240, C3: no history campaign is touched, whatever the switch says', () => {
  it('reads exactly 1 for every nation and navy.json\'s own toughness for every fleet', () => {
    R.dc.on = true;
    for (const seed of [1, 5]) {
      const g = createGame(seed, null);
      const before = saveGame(g);
      for (const nat of Object.keys(g.nations)) expect(dcOf(g, nat), nat).toBe(1);
      for (const f of g.fleets) for (const c of CLASSES) expect(toughOf(g, f, c), `${f.name} ${c}`).toBe(NAVY.classes[c].tough);
      // (And asking opens no tree: nothing is written.)
      expect(saveGame(g) === before).toBe(true);
    }
  });

  it('a history fleet action and its forecast are the same to the byte', () => {
    const fight = (on: boolean): string => {
      R.dc.on = on;
      const g = createGame(5, null);
      const home = named(g, 'Home Fleet'), battle = named(g, 'Battle Fleet'), z = zoneOf(g, 'norw');
      expect(sail(g, home, z, 'patrol')).toBe(true);
      expect(sail(g, battle, z, 'patrol')).toBe(true);
      const plan = JSON.stringify(planSeaBattle(g, home.id, z, null));
      const reps = seaBattles(g, 'UK');
      expect(reps).toHaveLength(1);
      expect(shipCount(reps[0].def[0].sunk)).toBeGreaterThan(0);
      expect(actionNews(g)).not.toMatch(/repair parties/);
      return plan + JSON.stringify(reps) + saveGame(g);
    };
    expect(fight(true) === fight(false)).toBe(true);
  });

  it('a history year is the same to the byte, month by month, and so is the bombers\' toll of a fleet under an enemy coast', () => {
    const play = (on: boolean): string[] => {
      R.dc.on = on;
      const g = createGame(2, null);
      const out: string[] = [];
      for (let m = 0; m < 12; m++) { runTurnAI(g); out.push(saveGame(g)); }
      return out;
    };
    const off = play(false), on = play(true);
    for (let m = 0; m < off.length; m++) expect(on[m] === off[m], `month ${m + 1}`).toBe(true);
    // The Home Fleet in the Skagerrak, under the German air force: the monthly toll runs in history campaigns too.
    const toll = (dc: boolean): { text: string; sunk: number } => {
      R.dc.on = dc;
      const g = createGame(1, null);
      const home = named(g, 'Home Fleet');
      home.zone = zoneOf(g, 'skag'); home.stance = 'patrol';
      let sunk = 0;
      for (let m = 0; m < 6; m++) { const had = shipCount(home.ships); navalUpkeep(g, 'UK'); sunk += had - shipCount(home.ships); g.turn++; }
      expect(g.headlines.some((h) => /repair parties/.test(h.text))).toBe(false);
      return { text: saveGame(g), sunk };
    };
    const a = toll(false), b = toll(true);
    expect(a.sunk).toBeGreaterThan(0);
    expect(b.text === a.text).toBe(true);
  });
});

describe('ruling 240, C3: a fleet action', () => {
  it('same seed, with and without the research on one side: the same battle, the other side\'s losses to the ship, and the holder\'s never more', () => {
    R.dc.on = true;
    const N = 40;
    for (const holder of ['GER', 'UK']) {
      let plain = 0, eased = 0, told = 0;
      for (let seed = 1; seed <= N; seed++) {
        const a = meet(seed), b = meet(seed, holder);
        // Submarines sail with both fleets: they are sunk as they always were.
        for (const x of [a, b]) { x.battle.ships.ss = 12; x.home.ships.ss = 12; }
        const da = dice(() => seaBattles(a.g, 'UK')), db = dice(() => seaBattles(b.g, 'UK'));
        const ra = da.out[0], rb = db.out[0];
        expect(ra, `seed ${seed}`).toBeDefined();
        // Who won, the strengths, the odds after the dice and the level: the same.
        expect([rb.won, rb.odds, rb.level, rb.aPower, rb.dPower]).toEqual([ra.won, ra.odds, ra.level, ra.aPower, ra.dPower]);
        // Readiness, and who sails home.
        for (const [x, y] of [[a.home, b.home], [a.battle, b.battle]]) expect([y.ready, y.stance, y.zone, y.fought]).toEqual([x.ready, x.stance, x.zone, x.fought]);
        // The battle's own dice: as many draws, left in the same state.
        expect(da.streams.get(battleKey(a.g, a.z))).toBeDefined();
        expect(db.streams.get(battleKey(b.g, b.z)), `seed ${seed}`).toEqual(da.streams.get(battleKey(a.g, a.z)));
        const [mineA, mineB, otherA, otherB, fleet] = holder === 'GER' ? [ra.def[0], rb.def[0], ra.att[0], rb.att[0], b.battle] : [ra.att[0], rb.att[0], ra.def[0], rb.def[0], b.home];
        expect(otherB.sunk, `seed ${seed}`).toEqual(otherA.sunk);
        for (const c of SURFACE) expect(mineB.sunk[c], `seed ${seed} ${c}`).toBeLessThanOrEqual(mineA.sunk[c]);
        expect(mineB.sunk.ss, `seed ${seed}`).toBe(mineA.sunk.ss);
        plain += shipCount(mineA.sunk, SURFACE); eased += shipCount(mineB.sunk, SURFACE);
        // What the same draws would have sunk and did not is told once, after the battle's own line of news.
        const saved = less(mineA.sunk, mineB.sunk);
        expect(actionNews(a.g)).not.toMatch(/repair parties/);
        if (shipCount(saved)) {
          told++;
          expect(actionNews(b.g), `seed ${seed}`).toMatch(/ sunk( for the loss of [^.]+)?\. [A-Z0-9]/);
          expect(actionNews(b.g).endsWith(` ${keptAfloat(fleet, saved)}`), actionNews(b.g)).toBe(true);
          expect(actionNews(b.g).match(/repair parties/g)).toHaveLength(1);
        } else expect(actionNews(b.g)).toBe(actionNews(a.g));
        // The report's ticket has no new line. (Its 'Forces' line has always given the strengths of the ships still
        // afloat once the losses are taken, so that one figure follows the ships saved; every other line is the same.)
        expect(rb.lines.map((l) => l.label)).toEqual(ra.lines.map((l) => l.label));
        expect(rb.lines.filter((l) => l.label !== 'Forces')).toEqual(ra.lines.filter((l) => l.label !== 'Forces'));
        expect(rb.lines[0].value).toBe(ra.lines[0].value);
      }
      // (Measured: Germany 401 ships sunk without it and 307 with it, told in all 40 battles; Britain 109, 79 and 25.)
      expect(plain, holder).toBeGreaterThan(N);
      expect(told, holder).toBeGreaterThan(N / 4);
      expect(eased / plain, holder).toBeGreaterThan(1 / DC - 0.08);
      expect(eased / plain, holder).toBeLessThan(1 / DC + 0.08);
    }
  });

  it('leaves the dice of the fleets beside it alone: an ally without the research loses the same ships and the same admiral', () => {
    R.dc.on = true;
    let spared = 0, burnt = 0;
    for (let seed = 1; seed <= 40; seed++) {
      const run = (holds: boolean): { g: Game; z: number; ger: Fleet; ita: Fleet; rep: ReturnType<typeof seaBattles>[number]; own: unknown } => {
        const { g, home, battle, z } = meet(seed, ...(holds ? ['GER'] : []));
        const ita = named(g, '2nd Fleet');
        setWar(g, 'UK', ita.nat);
        ita.zone = z; ita.stance = 'patrol'; ita.lane = undefined;
        // A battle line heavy enough to beat them both, and capital ships enough on the German side for some to be hit.
        home.ships.bb = 30; battle.ships.bb = 8; battle.ships.cv = 4;
        const d = dice(() => seaBattles(g, 'UK'));
        expect(d.out).toHaveLength(1);
        expect(d.out[0].won).toBe(true);
        expect(d.out[0].def.map((x) => x.nat)).toEqual(['GER', 'ITA']);
        return { g, z, ger: battle, ita, rep: d.out[0], own: d.streams.get(battleKey(g, z)) };
      };
      const a = run(false), b = run(true);
      expect(b.own, `seed ${seed}`).toEqual(a.own);
      // Italy, which fights beside the holder and after it in the battle's order: the same to the ship and to the man.
      expect(b.rep.def[1].sunk, `seed ${seed}`).toEqual(a.rep.def[1].sunk);
      expect(b.ita.admiral, `seed ${seed}`).toEqual(a.ita.admiral);
      expect(b.rep.att[0].sunk).toEqual(a.rep.att[0].sunk);
      const capA = a.rep.def[0].sunk.bb + a.rep.def[0].sunk.cv, capB = b.rep.def[0].sunk.bb + b.rep.def[0].sunk.cv;
      expect(capB).toBeLessThanOrEqual(capA);
      // A German flagship that would have gone down and was kept afloat: the admiral lives, whatever the dice said.
      if (capA > 0 && capB === 0) {
        spared++;
        expect(b.ger.admiral!.name).toBe('Wilhelm Marschall');
        if (a.ger.admiral!.name !== 'Wilhelm Marschall') burnt++;
      }
      // With a capital ship sunk in both, the same dice decide the same fate.
      if (capB > 0) expect(b.ger.admiral!.name).toBe(a.ger.admiral!.name);
    }
    // (Both cases arise in these forty battles, or the test would prove nothing. Measured: the flagships spared in 3,
    // and in 2 of those the dice had the admiral lost.)
    expect(spared).toBeGreaterThan(0);
    expect(burnt).toBeGreaterThan(0);
  });

  it('every nation alike: the same ships under any flag on the chart are as hard to sink, by its own research or its senior partner\'s', () => {
    R.dc.on = true;
    const { g, battle } = meet(3);
    battle.ships.ss = 9;
    const nats = Object.keys(g.nations);
    expect(nats.length).toBeGreaterThan(40);
    for (const nat of nats) expect(techOf(g, nat)!.done, nat).not.toContain('damagecontrol');
    // The fleet under one flag, the research held by nobody and then by `holder`: its toughness by class, the figure
    // the fight and the forecast work, and what six rolls of the same dice sink and save.
    const under = (flag: string, holder: string): unknown[] => {
      const out: unknown[] = [];
      battle.nat = flag;
      for (const holds of [false, true]) {
        if (holds) give(g, holder);
        expect(dcOf(g, flag), `${flag} by ${holder} ${holds}`).toBe(holds ? DC : 1);
        out.push(CLASSES.map((c) => toughOf(g, battle, c)));
        for (let k = 1; k <= 6; k++) {
          const f: Fleet = { ...battle, ships: { ...battle.ships }, lost: { ...battle.lost } };
          const rng = new Rng(100 + k), kept = noShips();
          out.push([CLASSES.map((c) => lossOf(g, f, c, 0.1 * k)), sinkShips(f, 0.1 * k, rng, undefined, dcOf(g, flag), kept), kept, rng.state]);
        }
      }
      techOf(g, holder)!.done = techOf(g, holder)!.done.filter((id) => id !== 'damagecontrol');
      return out;
    };
    const ger = under('GER', 'GER');
    expect(ger[0]).toEqual(CLASSES.map((c) => NAVY.classes[c].tough));
    expect(ger[7]).toEqual(CLASSES.map((c) => (c === 'ss' ? NAVY.classes[c].tough : NAVY.classes[c].tough / DC)));
    // Every flag on the chart by its own research, and a kin nation's ships by its senior partner's (a dominion Britain's).
    for (const flag of nats) expect(under(flag, flag), flag).toEqual(ger);
    const kin = nats.filter((k) => PLANS.kin[k] !== undefined && nats.includes(PLANS.kin[k]));
    expect(kin).toContain('CAN');
    for (const k of kin) expect(under(k, PLANS.kin[k]), `${k} by ${PLANS.kin[k]}`).toEqual(ger);
  });

  it('the forecast reads the figure the fight works: with a holder on either side the fight ends inside its band, and his likely losses fall', () => {
    R.dc.on = true;
    const N = 40;
    const bare = meet(1);
    const plain = planSeaBattle(bare.g, bare.home.id, bare.z, null)!;
    for (const holder of ['UK', 'GER']) {
      let ours = 0, theirs = 0, first = null as ReturnType<typeof planSeaBattle>;
      for (let seed = 1; seed <= N; seed++) {
        const { g, home, z } = meet(seed, holder);
        const before = saveGame(g);
        const plan = planSeaBattle(g, home.id, z, null)!;
        expect(saveGame(g) === before, 'the forecast writes nothing').toBe(true);
        first ??= plan;
        expect(plan.why).toBeNull();
        expect(plan.offers).toBe(true);
        const [r] = seaBattles(g, 'UK');
        expect(r, `seed ${seed}`).toBeDefined();
        expect(plan.ours.total).toBeCloseTo(r.aPower, 9);
        expect(plan.theirs.total).toBeCloseTo(r.dPower, 9);
        expect(r.odds).toBeGreaterThanOrEqual(plan.odds!.low - 1e-9);
        expect(r.odds).toBeLessThanOrEqual(plan.odds!.high + 1e-9);
        // Every class on each side goes down within the forecast's fewest and most.
        for (const c of CLASSES) {
          expect(r.att[0].sunk[c], `${holder} seed ${seed} ours ${c}`).toBeGreaterThanOrEqual(plan.range.ours[0][c]);
          expect(r.att[0].sunk[c], `${holder} seed ${seed} ours ${c}`).toBeLessThanOrEqual(plan.range.ours[1][c]);
          expect(r.def[0].sunk[c], `${holder} seed ${seed} theirs ${c}`).toBeGreaterThanOrEqual(plan.range.theirs[0][c]);
          expect(r.def[0].sunk[c], `${holder} seed ${seed} theirs ${c}`).toBeLessThanOrEqual(plan.range.theirs[1][c]);
        }
        ours += shipCount(r.att[0].sunk); theirs += shipCount(r.def[0].sunk);
      }
      // The likely losses are what forty fights lose on average.
      expect(Math.abs(ours / N - shipCount(first!.likely.ours)), holder).toBeLessThan(1);
      expect(Math.abs(theirs / N - shipCount(first!.likely.theirs)), holder).toBeLessThan(1.5);
      // Beside the same forecast with nobody holding it: the odds are the same, the holder's likely losses fall, the
      // other side's stand, and the forecast has no new line.
      expect(first!.odds).toEqual(plain.odds);
      expect(first!.win).toBe(plain.win);
      const [mine, other] = holder === 'UK' ? (['ours', 'theirs'] as const) : (['theirs', 'ours'] as const);
      expect(first!.likely[other]).toEqual(plain.likely[other]);
      expect(first!.range[other]).toEqual(plain.range[other]);
      for (const c of CLASSES) expect(first!.likely[mine][c], `${holder} ${c}`).toBeLessThanOrEqual(plain.likely[mine][c]);
      expect(shipCount(first!.likely[mine]), holder).toBeLessThan(shipCount(plain.likely[mine]));
      expect(shipCount(first!.range[mine][1]), holder).toBeLessThan(shipCount(plain.range[mine][1]));
      expect(first!.lines.map((l) => l.label)).toEqual(plain.lines.map((l) => l.label));
      expect(first!.lines).toEqual(plain.lines);
    }
  });

  it('the forecast and the fight work one figure, to the last bit: every fleet and every class, whoever holds the research', () => {
    R.dc.on = true;
    // What the fight asks its dice for, in the order it asks: for each fleet, attackers first, each class in turn,
    // [the ships due to sink, the ships due without damage control].
    const fought = (g: Game): { rep: ReturnType<typeof seaBattles>[number]; asked: [number, number][] } => {
      const real = Rng.prototype.rollBoth, asked: [number, number][] = [];
      Rng.prototype.rollBoth = function (this: Rng, x: number, y: number): [number, number] { asked.push([x, y]); return real.call(this, x, y); };
      try {
        const reps = seaBattles(g, 'UK');
        expect(reps).toHaveLength(1);
        return { rep: reps[0], asked };
      } finally { Rng.prototype.rollBoth = real; }
    };
    let figures = 0, eased = 0, won = 0, lost = 0;
    const hold = (g: Game, before: Game, holders: string[]): void => {
      const { rep, asked } = fought(g);
      if (rep.won) won++; else lost++;
      // The forecast's own shares at the odds the dice gave this fight.
      const s = shares(rep.odds);
      const sides = [...rep.att.map((x) => ({ x, share: s.ours })), ...rep.def.map((x) => ({ x, share: s.theirs }))];
      expect(asked).toHaveLength(sides.length * CLASSES.length);
      sides.forEach(({ x, share }, i) => {
        // The fleet as it stood before the battle, in a copy of the game the fight never touched.
        const f = before.fleets.find((y) => y.nat === x.nat && y.name === x.fleet)!;
        expect(f.ships).toEqual(x.ships);
        const forecast = lossAt(before, [f], share);
        CLASSES.forEach((c, k) => {
          const [due, plain] = asked[i * CLASSES.length + k];
          const what = `${holders.join('+') || 'nobody'} seed ${g.seed} ${x.fleet} ${c}`;
          // The fight's figure is navy.ts lossOf, and the forecast's is the same number to the last bit.
          expect(Object.is(due, lossOf(before, f, c, share)), what).toBe(true);
          expect(Object.is(forecast[c], Math.min(f.ships[c], due)), what).toBe(true);
          // Without damage control it is release 1's own expression, and so is every submarine's and every navy's without the research.
          expect(Object.is(plain, f.ships[c] * share * NAVY.classes[c].tough), what).toBe(true);
          const holds = holders.includes(x.nat) && c !== 'ss';
          if (!holds) expect(Object.is(due, plain), what).toBe(true);
          else { expect(due, what).toBeCloseTo(plain / DC, 12); if (due !== plain) eased++; }
          figures++;
        });
      });
    };
    for (const holders of [[], ['UK'], ['GER'], ['UK', 'GER']]) {
      for (let seed = 1; seed <= 8; seed++) {
        const { g, home, battle } = meet(seed, ...holders);
        home.ships.ss = 7; battle.ships.ss = 9;
        hold(g, copy(g), holders);
      }
      // A fight the attacker can lose: the German fleet reinforced until the odds stand just over what the British
      // admiral wants before he offers battle, so that the dice can turn it.
      for (let seed = 1; seed <= 10; seed++) {
        const { g, home, battle, z } = meet(seed, ...holders);
        while (planSeaBattle(g, home.id, z, null)!.odds!.mid > NAVY.battle.seek + 0.02) battle.ships.dd++;
        expect(planSeaBattle(g, home.id, z, null)!.offers).toBe(true);
        hold(g, copy(g), holders);
      }
    }
    // Three fleets in one action: Italy's beside Germany's, each navy with its own research or none.
    for (const holders of [['ITA'], ['GER'], ['UK', 'ITA']]) {
      for (let seed = 1; seed <= 6; seed++) {
        const { g, home, battle, z } = meet(seed, ...holders);
        const ita = named(g, '2nd Fleet');
        setWar(g, 'UK', ita.nat);
        ita.zone = z; ita.stance = 'patrol'; ita.lane = undefined;
        home.ships.bb = 30; battle.ships.bb = 8; battle.ships.cv = 4;
        hold(g, copy(g), holders);
      }
    }
    // (Both ends of the fight, and figures enough that are eased, or the test would hold nothing. Measured: 86 fights
    // won and 4 lost, 990 figures, 324 of them eased.)
    expect(won).toBeGreaterThan(0);
    expect(lost).toBeGreaterThan(0);
    expect(figures).toBeGreaterThan(600);
    expect(eased).toBeGreaterThan(150);
  });
});

describe('ruling 240, C3: the other ways a warship is sunk', () => {
  it('the bombers\' monthly toll: eased for a fleet that holds it, on the same draws, and unchanged by the bombers\' own research', () => {
    const g = britain();
    const home0 = named(g, 'Home Fleet');
    put(g, home0, 'skag');
    R.dc.on = true;
    const snap = saveGame(g);
    let plain = 0, eased = 0, told = 0, alone = 0;
    for (let k = 0; k < 120; k++) {
      const run = (holder: string | null): { lost: Ships; news: string[]; own: unknown; ready: number } => {
        const h = loadGame(snap)!;
        h.seed = 7000 + k;
        if (holder) give(h, holder);
        const home = named(h, 'Home Fleet');
        const before = { ...home.ships }, mark = h.headlines.length;
        const d = dice(() => navalUpkeep(h, 'UK'));
        return { lost: less(before, home.ships), news: h.headlines.slice(mark).map((x) => x.text).filter((t) => /^Bombers /.test(t)), own: d.streams.get(startOf(`sea:${h.seed}:${h.turn}:air:${home.id}`)), ready: home.ready };
      };
      const a = run(null), b = run('UK'), c = run('GER');
      expect(a.own, 'the toll has its own stream').toBeDefined();
      expect(b.own).toEqual(a.own);
      expect(b.ready).toBe(a.ready);
      // Germany's research is nothing to British ships.
      expect([c.lost, c.news, c.own]).toEqual([a.lost, a.news, a.own]);
      for (const cls of CLASSES) expect(b.lost[cls], `month ${k} ${cls}`).toBeLessThanOrEqual(a.lost[cls]);
      expect(b.lost.ss).toBe(0);
      plain += shipCount(a.lost); eased += shipCount(b.lost);
      const saved = less(a.lost, b.lost);
      expect(a.news.join(' ')).not.toMatch(/repair parties/);
      if (!shipCount(saved)) { expect(b.news).toEqual(a.news); continue; }
      told++;
      // (Rider 2 of 22 November: the lead-in names the fleet, and the line names it once: "One of its cruisers is ...".)
      if (shipCount(b.lost)) expect(b.news).toEqual([`${a.news[0].replace(/^Bombers sink .*? of the Home Fleet/, `Bombers sink ${shipList(b.lost)} of the Home Fleet`)} ${keptAfloat(home0, saved, 'its')}`]);
      else { alone++; expect(b.news).toEqual([`Bombers attack the Home Fleet in the Skagerrak. ${keptAfloat(home0, saved, 'its')}`]); }
    }
    // (Measured: 137 ships sunk without it and 106 with it; told in 29 months, in 15 of them with nothing sunk.)
    expect(plain).toBeGreaterThan(80);
    expect(told).toBeGreaterThan(10);
    expect(alone).toBeGreaterThan(3);
    expect(eased / plain).toBeGreaterThan(1 / DC - 0.1);
    expect(eased / plain).toBeLessThan(1 / DC + 0.1);
  });

  it('shore batteries answering a bombardment: the forecast and the toll are eased together, on the same draws', () => {
    const g = britain();
    const home0 = named(g, 'Home Fleet');
    const z = zoneOf(g, 'nsea');
    expect(orderFleet(g, home0.id, z, 'patrol')).toBe(true);
    const r0 = g.seas[z].coast.map((id) => g.regions[id]).find((x) => x.ctrl === 'GER')!;
    const corps = g.corps.find((x) => x.nat === 'GER' && !x.dead && x.kind === 'inf')!;
    relocate(g, corps, r0);
    corps.org = 0.9; corps.dug = false;
    r0.fort = { lv: 4, vs: [] };
    home0.ships.ca = 20; home0.ships.dd = 40;
    R.dc.on = true;
    const snap = saveGame(g);
    // The forecast: the share of the light ships likely to be sunk, and the words that say it.
    const raw = FLEETS.bombard.reply * 4;
    const p0 = planBombard(g, home0, r0, 'troops');
    expect(p0.reply).toBeCloseTo(raw, 12);
    const words = (p: typeof p0): string => p.lines.find((l) => l.label === 'Shore batteries')!.effect!;
    expect(words(p0)).toBe(`About ${Math.round(raw * 100)}% of the cruisers and destroyers that stand in are likely to be sunk`);
    {
      const h = loadGame(snap)!;
      give(h, 'UK');
      const p = planBombard(h, named(h, 'Home Fleet'), h.regions[r0.id], 'troops');
      expect(p.reply).toBeCloseTo(raw / DC, 12);
      expect(words(p)).toBe(`About ${Math.round((raw / DC) * 100)}% of the cruisers and destroyers that stand in are likely to be sunk`);
      expect(p.lines.map((l) => l.label)).toEqual(p0.lines.map((l) => l.label));
      // The coast's own research does nothing for the ships that shell it.
      const o = loadGame(snap)!;
      give(o, 'GER');
      expect(planBombard(o, named(o, 'Home Fleet'), o.regions[r0.id], 'troops').reply).toBe(p0.reply);
    }
    let plain = 0, eased = 0, told = 0, afterSunk = 0;
    for (let k = 0; k < 40; k++) {
      const run = (holder: string | null): { sunk: Ships; result: string; news: string; own: unknown; cut: number } => {
        const h = loadGame(snap)!;
        h.seed = 8000 + k;
        if (holder) give(h, holder);
        const home = named(h, 'Home Fleet'), r = h.regions[r0.id];
        const d = dice(() => bombard(h, home, r, 'troops')!);
        return { sunk: d.out.sunk, result: d.out.result, news: h.headlines[h.headlines.length - 1].text, own: d.streams.get(startOf(`sea:${h.seed}:${h.turn}:shell:${home.id}:${r.id}`)), cut: d.out.cut };
      };
      const a = run(null), b = run('UK'), c = run('GER');
      expect(a.own, 'the shelling has its own stream').toBeDefined();
      expect(b.own).toEqual(a.own);
      expect(b.cut).toBe(a.cut);
      expect([c.sunk, c.result, c.news, c.own]).toEqual([a.sunk, a.result, a.news, a.own]);
      expect(a.sunk.bb + a.sunk.cv + a.sunk.ss).toBe(0);
      for (const cls of CLASSES) expect(b.sunk[cls], `${k} ${cls}`).toBeLessThanOrEqual(a.sunk[cls]);
      plain += shipCount(a.sunk); eased += shipCount(b.sunk);
      const saved = less(a.sunk, b.sunk);
      expect(a.result).not.toMatch(/repair parties/);
      if (!shipCount(saved)) { expect(b.result).toBe(a.result); expect(b.news).toBe(a.news); continue; }
      told++;
      // The report is past, so its line is past, in the fleet form (rider 3, 4); the news names the fleet once: "its" right
      // after the lead-in, "the fleet's" after the batteries' line (rider 3, 1).
      expect(b.result.endsWith(` ${keptAfloat(home0, saved, 'fleet', true)}`), b.result).toBe(true);
      expect(b.result).toMatch(/ (was|were) badly hit, but the repair parties kept the ships? afloat\.$/);
      if (shipCount(b.sunk)) afterSunk++;
      expect(b.news.endsWith(` ${keptAfloat(home0, saved, shipCount(b.sunk) ? "fleet's" : 'its')}`), b.news).toBe(true);
      expect(b.result.match(/repair parties/g)).toHaveLength(1);
    }
    // (Measured: 122 ships sunk without it and 98 with it, told after 22 of the 40 shellings, each of them after the
    // batteries' line, so the news says "the fleet's".)
    expect(afterSunk).toBeGreaterThan(10);
    expect(plain).toBeGreaterThan(80);
    expect(told).toBeGreaterThan(10);
    expect(eased / plain).toBeGreaterThan(1 / DC - 0.06);
    expect(eased / plain).toBeLessThan(1 / DC + 0.06);
  });

  it('a submarine\'s torpedo: the ship is hit as ever, sinks 1/dc as often, and the stream is spent the same way', () => {
    const g = britain();
    const subs = named(g, 'U-boat Fleet'), home0 = named(g, 'Home Fleet');
    subs.ships.ss = 80; subs.stance = 'raid'; subs.zone = zoneOf(g, 'natl');
    home0.ships.dd = 0;
    put(g, home0, 'natl');
    const p = torpedoRisk(g, home0, home0.zone);
    expect(p).toBeGreaterThan(0.2);
    R.dc.on = true;
    const snap = saveGame(g);
    const SUNK = /^A submarine torpedoes and sinks an? (battleship|carrier|cruiser) of the Home Fleet in the North Atlantic: the fleet has too few destroyers to screen its heavy ships\.$/;
    let hits = 0, sunk = 0, savedShips = 0;
    const N = 120;
    for (let k = 0; k < N; k++) {
      const run = (holder: string | null): { lost: Ships; news: string[]; own: { n: number; end: number } | undefined; ready: number } => {
        const h = loadGame(snap)!;
        h.seed = 9000 + k;
        if (holder) give(h, holder);
        const home = named(h, 'Home Fleet');
        const before = { ...home.ships }, mark = h.headlines.length;
        const d = dice(() => navalUpkeep(h, 'UK'));
        return { lost: less(before, home.ships), news: h.headlines.slice(mark).map((x) => x.text).filter((t) => /^A submarine /.test(t)), own: d.streams.get(startOf(`sea:${h.seed}:${h.turn}:torpedo:${home.id}`)), ready: home.ready };
      };
      const a = run(null), b = run('UK'), c = run('GER');
      expect(a.own, 'the torpedo has its own stream').toBeDefined();
      // One draw for the hit and, only when a ship is hit, one for its class: with and without the research.
      expect(b.own).toEqual(a.own);
      expect(b.ready).toBe(a.ready);
      // The submarines' own research is nothing to the ships they hit.
      expect([c.lost, c.news, c.own]).toEqual([a.lost, a.news, a.own]);
      if (!a.news.length) { expect(a.own!.n).toBe(1); expect(shipCount(a.lost)).toBe(0); expect([b.lost, b.news]).toEqual([a.lost, a.news]); continue; }
      hits++;
      expect(a.own!.n).toBe(2);
      const cls = SUNK.exec(a.news[0])![1];
      expect(shipCount(a.lost)).toBe(1);
      if (shipCount(b.lost)) { sunk++; expect([b.lost, b.news]).toEqual([a.lost, a.news]); continue; }
      // Hit and saved: the same class of ship, afloat, and the ruled line.
      savedShips++;
      // (The UI director's rider 2 of 22 November: the fleet is named once.)
      expect(b.news).toEqual([`A submarine attacks the Home Fleet in the North Atlantic: the fleet has too few destroyers to screen its heavy ships. One of its ${NAVY.classes[cls === 'battleship' ? 'bb' : cls === 'carrier' ? 'cv' : 'ca'].many} is badly hit, but the repair parties keep the ship afloat.`]);
      // (The month card reads these words for a sinking: the news of a ship saved has none of them.)
      expect(b.news[0]).not.toMatch(/\b(sinks?|sunk|torpedoes|scuttled)\b/);
    }
    // About p of the months a ship is hit, and of the ships hit about 1/dc go down. (Measured: p 0.4; 41 ships hit in
    // the 120 months, 35 of them sunk and 6 kept afloat.)
    expect(hits / N).toBeGreaterThan(p - 0.12);
    expect(hits / N).toBeLessThan(p + 0.12);
    expect(savedShips).toBeGreaterThan(3);
    expect(sunk / hits).toBeGreaterThan(1 / DC - 0.15);
    expect(sunk / hits).toBeLessThan(1 / DC + 0.15);
  });
});

describe('ruling 240, C3: the switch off with the research held (a save made with it on)', () => {
  it('reads as release 1 everywhere at sea: the fight, its forecast, the shore guns\' forecast, the toll and the torpedo', () => {
    const fight = (on: boolean, holds: boolean): string => {
      R.dc.on = on;
      const { g, home, battle, z } = meet(5, ...(holds ? ['UK', 'GER'] : []));
      const out: string[] = [];
      out.push(JSON.stringify([dcOf(g, 'UK'), dcOf(g, 'GER'), CLASSES.map((c) => [toughOf(g, home, c), toughOf(g, battle, c)])]));
      out.push(JSON.stringify(planSeaBattle(g, home.id, z, null)));
      out.push(JSON.stringify(seaBattles(g, 'UK')));
      // What the guns of a fortified German coast would do to its cruisers and destroyers.
      const shore = g.seas[zoneOf(g, 'nsea')].coast.map((id) => g.regions[id]).find((x) => x.ctrl === 'GER')!;
      shore.fort = { lv: 4, vs: [] };
      const shell = planBombard(g, home, shore, 'troops');
      out.push(JSON.stringify([shell.fort, shell.reply, shell.lines]));
      // The month at sea, with the Home Fleet under the German air force and without a screen.
      home.ships.dd = 0;
      put(g, home, 'skag');
      const subs = named(g, 'U-boat Fleet');
      subs.ships.ss = 80; subs.stance = 'raid'; subs.zone = home.zone;
      for (let m = 0; m < 4; m++) { navalUpkeep(g, 'UK'); g.turn++; }
      out.push(JSON.stringify(g.headlines.map((h) => h.text)));
      for (const nat of ['UK', 'GER']) techOf(g, nat)!.done = techOf(g, nat)!.done.filter((id) => id !== 'damagecontrol');
      out.push(saveGame(g));
      return out.join('\n');
    };
    const r1 = fight(false, false);
    expect(fight(false, true) === r1).toBe(true);
    expect(fight(true, false) === r1).toBe(true);
    // (And with both the switch and the research, the same months do differ.)
    expect(fight(true, true) === r1).toBe(false);
  });
});

describe("ruling 240: the war runner's watcher (navy.ts afloatWatch) hears and moves nothing", () => {
  afterEach(() => { afloatWatch.tell = null; afloatWatch.strike = null; });
  it('a strike on a fleet, the older strike at shipping and a month at sea: the same saves with it set, and it hears what was sunk and saved', () => {
    const { g, home } = duel();
    const ally = named(g, '2nd Fleet');
    setWar(g, 'UK', ally.nat);
    put(g, ally, 'natl');
    R.dc.on = true;
    give(g, 'GER');
    const snap = saveGame(g);
    const play = (watch: boolean): { save: string; told: AfloatTold[]; strikes: number; rep: { fleet: Ships[]; shipping: string } } => {
      const told: AfloatTold[] = [];
      let strikes = 0;
      if (watch) { afloatWatch.tell = (e) => told.push({ ...e, sunk: { ...e.sunk }, saved: { ...e.saved }, fleet: undefined }); afloatWatch.strike = () => { strikes++; }; }
      const h1 = loadGame(snap)!, h2 = loadGame(snap)!;
      const a = fly(h1, 'UK', 'fleet', named(h1, 'Battle Fleet').id, [wing(h1, named(h1, 'Home Fleet'), 'bmr')], ':w1')!;
      const b = fly(h2, 'UK', 'strike', home.zone, [wing(h2, named(h2, 'Home Fleet'), 'bmr')], ':w2')!;
      afloatWatch.tell = null; afloatWatch.strike = null;
      return { save: saveGame(h1) + saveGame(h2), told, strikes, rep: { fleet: a.ships!.map((x) => x.sunk), shipping: b.result } };
    };
    const off = play(false), on = play(true);
    expect(on.save).toBe(off.save);
    expect(on.rep).toEqual(off.rep);
    expect(on.strikes).toBe(1);
    const air = on.told.filter((e) => e.route === 'air'), sea = on.told.filter((e) => e.route === 'shipping');
    expect(air.map((e) => e.nat)).toEqual(['GER']);
    expect(air[0].sunk).toEqual(off.rep.fleet[0]);
    expect(sea.map((e) => e.nat).sort()).toEqual(['GER', 'ITA']);
    for (const e of sea) { expect(e.dc).toBe(e.nat === 'GER' ? DC : 1); expect(e.frac).toBeGreaterThan(0); if (e.nat === 'ITA') expect(shipCount(e.saved)).toBe(0); }
  });
});
