// Ruling 240, C3 (Brian, 21 November 2026, "yes to all 3"; release 2): Damage control, a naval research. A fleet whose
// nation holds it takes strike.dc times the hits to sink: its surface warships only, wherever warships are sunk.
// Every nation alike: the rule reads research only. It stands behind the switch research.dc in carriers.json, OFF as
// shipped; with it off, and in every history campaign whatever it says, the game is release 1 to the byte.
// No rule here adds, removes or reorders a draw: a ship saved is found from the same single draw that decides the sinking.
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { Rng, hashString } from '../src/core/rng';
import { CARRIERS, assessAir, fly, forecastAir, shipHits, type AirAssess } from '../src/campaign/air';
import { syncWings, wingsOf } from '../src/campaign/carriers';
import { createGame, setWar } from '../src/campaign/game';
import { atSea, keptAfloat, sail, shipCount, zoneOf } from '../src/campaign/navy';
import { PLANS } from '../src/campaign/plans';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { SHIP_RESEARCH, dcOf, techOf } from '../src/campaign/tech';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
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
  it('is the UI director\'s sentence: a count that opens it is a word, a count inside it a figure, and no ship has a pronoun', () => {
    expect(keptAfloat(home, { ...noShips(), ca: 1 })).toBe('A cruiser of the Home Fleet is badly hit, but the repair parties keep the ship afloat.');
    expect(keptAfloat(home, { ...noShips(), dd: 2 })).toBe('Two destroyers of the Home Fleet are badly hit, but the repair parties keep the ships afloat.');
    expect(keptAfloat(home, { ...noShips(), ca: 1, dd: 2 })).toBe('A cruiser and 2 destroyers of the Home Fleet are badly hit, but the repair parties keep the ships afloat.');
    expect(keptAfloat(home, { bb: 1, cv: 1, ca: 3, dd: 1, ss: 0 })).toBe('A battleship, 1 carrier, 3 cruisers and 1 destroyer of the Home Fleet are badly hit, but the repair parties keep the ships afloat.');
    expect(keptAfloat(home, { ...noShips(), dd: 12 })).toBe('12 destroyers of the Home Fleet are badly hit, but the repair parties keep the ships afloat.');
    // A fleet that takes no article keeps none.
    expect(keptAfloat(force, { ...noShips(), bb: 1 })).toBe('A battleship of Force H is badly hit, but the repair parties keep the ship afloat.');
    // Addressed to the owner alone.
    expect(keptAfloat(home, { ...noShips(), ca: 1 }, true)).toBe('One of your cruisers is badly hit, but the repair parties keep the ship afloat.');
    expect(keptAfloat(home, { ...noShips(), dd: 2 }, true)).toBe('Two of your destroyers are badly hit, but the repair parties keep the ships afloat.');
    // Nothing saved, nothing said.
    expect(keptAfloat(home, noShips())).toBe('');
    for (const s of [keptAfloat(home, { ...noShips(), cv: 1 }), keptAfloat(home, { ...noShips(), cv: 1 }, true)]) expect(s).not.toMatch(/\b(she|her|it|its)\b/i);
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
    let plainSunk = 0, easedSunk = 0, told = 0;
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
      const line = keptAfloat(foe, saved);
      expect(a.result).not.toMatch(/repair parties/);
      if (shipCount(saved)) {
        told++;
        expect(b.result.endsWith(` ${line}`), `strike ${k}: ${b.result}`).toBe(true);
        expect(b.result.match(/repair parties/g)).toHaveLength(1);
        expect(b.news).toContain(line);
        expect(b.result).not.toMatch(/little harm/);
      } else {
        expect(b.result).toBe(a.result);
        expect(b.news).toBe(a.news);
      }
    }
    // (Measured on these 60 strikes: 361 ships sunk without the research, 265 with it, and a ship saved in 58 of them.)
    expect(plainSunk).toBeGreaterThan(N * 3);
    expect(told).toBeGreaterThan(N / 2);
    expect(easedSunk / plainSunk).toBeGreaterThan(1 / DC - 0.06);
    expect(easedSunk / plainSunk).toBeLessThan(1 / DC + 0.06);
  });

  it('in a harbour tells each fleet\'s saved ships in a line of its own, and never eases the navy that does not hold it', () => {
    const { g, foe, ally, port } = harbour();
    R.dc.on = true;
    g.flags['war:UK:GER'] = g.turn;
    const snap = saveGame(g);
    let both = 0, germans = 0;
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
      if (shipCount(savedG)) { germans++; expect(ger.result.endsWith(` ${keptAfloat(foe, savedG)}`), ger.result).toBe(true); expect(ger.result).not.toContain('2nd Fleet is'); }
      else expect(ger.result).toBe(none.result);
      const lines = [keptAfloat(foe, savedG), keptAfloat(ally, savedI)].filter(Boolean);
      if (lines.length === 2) both++;
      if (lines.length) expect(all.result.endsWith(` ${lines.join(' ')}`), all.result).toBe(true);
      expect((all.result.match(/repair parties/g) ?? []).length).toBe(lines.length);
    }
    expect(germans).toBeGreaterThan(8);
    expect(both).toBeGreaterThan(4);
  });

  it('every nation alike: the same fleet under a great power\'s flag, a small power\'s and a dominion\'s loses the same ships', () => {
    expect(PLANS.kin.CAN).toBe('UK');
    R.dc.on = true;
    const base = duel();
    const snap = saveGame(base.g);
    // [the flag the fleet flies, the nation whose research it reads]: a dominion's ships have Britain's damage control.
    const seen = ([['GER', 'GER'], ['NOR', 'NOR'], ['CAN', 'UK']] as const).map(([flag, holder]) => {
      const out: unknown[] = [];
      for (const holds of [false, true]) {
        for (let k = 0; k < 6; k++) {
          const h = loadGame(snap)!;
          const f = named(h, 'Battle Fleet'), w = wing(h, named(h, 'Home Fleet'), 'bmr');
          f.nat = flag;
          if (holds) give(h, holder);
          expect(dcOf(h, flag), `${flag} ${holds}`).toBe(holds ? DC : 1);
          const a = assessAir(h, [w], 'fleet', f.id);
          const rep = fly(h, 'UK', 'fleet', f.id, [w], `:${k}`)!;
          out.push([a.tough, shipHits(a, 'fleet', 120), rep.ships![0].sunk, rep.result]);
        }
      }
      return out;
    });
    expect(seen[0].some((x) => /repair parties/.test((x as [unknown, unknown, unknown, string])[3]))).toBe(true);
    expect(seen[1]).toEqual(seen[0]);
    expect(seen[2]).toEqual(seen[0]);
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
