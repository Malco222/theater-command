// Carriers and the air war at sea (the grand campaign). A fleet with carriers brings a wing of fighters and a wing
// of strike aircraft that go where it goes; aircraft strike a fleet at sea or ships in a harbour from the same raid
// planner as any raid; carrier fighters and ships' guns defend; and a fleet has to be found before it can be
// struck. The history campaigns keep the war at sea they always had.
import { describe, expect, it } from 'vitest';
import carriers from '../data/campaign/carriers.json';
import { AIR, CARRIERS, airFleetsOf, airPool, airRange, airUpkeep, assessAir, carrierRules, fly, forecastAir, groupAt, kmTo, kmToSea, shipHits, shipsInPort } from '../src/campaign/air';
import { airAct, airPage } from '../src/campaign/airpages';
import { airfieldCard, groupJobs, planRaid, raidTargets } from '../src/campaign/airwar';
import { capOver, deckWorth, fleetFound, fleetKnown, fleetsSeen, openCarriers, searchOf, searchOptions, searchSea, seaPicture, syncWings, wingCards, wingShare, wingSize, wingsOf } from '../src/campaign/carriers';
import { detach, fleetCard, merge, seaOptions } from '../src/campaign/fleets';
import { createGame } from '../src/campaign/game';
import { setFog } from '../src/campaign/intel';
import { atSea, fleetPower, sail, shipCount, surfaceShips, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, fleetOptions, fleetView, fleetsInSight, humanUp, myAirGroups, orderRaid, orderSearch, previewRaid, raidMenu, raidTargetsFor, runTurnAI, searchOptionsFor, seaPictureOf, stepAI, wingsOfFleet } from '../src/campaign/turn';
import type { AirFleet, Fleet, Game } from '../src/campaign/types';

const S = CARRIERS.strike;
const named = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;
const sea = (g: Game, key: string): number => zoneOf(g, key);
const wing = (g: Game, f: Fleet, kind: 'ftr' | 'bmr'): AirFleet => wingsOf(g, f.id).find((w) => w.kind === kind)!;
const aboard = (g: Game, f: Fleet): number => wingsOf(g, f.id).reduce((s, w) => s + w.ftr + w.bmr, 0);

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
  f.zone = sea(g, key); f.stance = 'patrol'; f.lane = undefined;
  expect(atSea(f)).toBe(true);
  syncWings(g);
}
/** Brings a fleet into its harbour. */
function dock(g: Game, f: Fleet): void {
  sail(g, f, f.home, 'port');
  expect(atSea(f)).toBe(false);
}
/** The Home Fleet with six carriers, and the German Battle Fleet with it in the open Atlantic, away from every coast's fighters. */
function duel(seed = 3): { g: Game; home: Fleet; foe: Fleet; strike: AirFleet; fighters: AirFleet } {
  const g = britain(seed);
  const home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet');
  home.ships.cv = 6;
  put(g, home, 'natl');
  put(g, foe, 'natl');
  return { g, home, foe, strike: wing(g, home, 'bmr'), fighters: wing(g, home, 'ftr') };
}

describe('the carrier data', () => {
  it('gives every navy its wings and explains every table', () => {
    for (const key of ['wing', 'range', 'naval', 'strike', 'harbour', 'flak', 'cap', 'sunk', 'empty', 'search', 'ai'] as const) expect((carriers._about as Record<string, string>)[key].length).toBeGreaterThan(60);
    for (const row of Object.values(carriers.wing)) { expect(row.ftr).toBeGreaterThan(0); expect(row.bmr).toBeGreaterThan(row.ftr); }
    expect(wingSize('USA').bmr).toBeGreaterThan(wingSize('UK').bmr);
    expect(wingSize('XXX')).toEqual(carriers.wing._default);
    expect(carriers.range.bmr).toBeGreaterThan(carriers.range.ftr);
    expect(carriers.range.next).toBeLessThanOrEqual(carriers.range.ftr);
  });
  it('keeps a strike within bounds: carriers are aimed at first, a harbour is the easier target, and land bombers are poor at it', () => {
    expect(S.aim.cv).toBeGreaterThan(S.aim.bb);
    expect(S.aim.bb).toBeGreaterThan(S.aim.ca);
    expect(S.aim.ss).toBe(0);
    expect(S.takes.bb).toBeGreaterThan(S.takes.cv);
    expect(S.most).toBeLessThan(0.5);
    expect(S.again).toBeLessThan(1);
    expect(S.report).toBeLessThan(1);
    expect(carriers.harbour.hit).toBeGreaterThan(1);
    expect(carriers.harbour.surprise).toBeGreaterThan(1);
    expect(carriers.harbour.unready).toBeLessThan(1);
    expect(carriers.harbour.most).toBeGreaterThan(S.most);
    for (const v of Object.values(carriers.naval.land)) expect(v).toBeLessThan(carriers.naval.carrier / 2);
    expect(carriers.search.find).toBeGreaterThan(carriers.search.report);
    expect(carriers.sunk.saved).toBeGreaterThan(0);
    expect(carriers.sunk.saved).toBeLessThan(1);
    expect(carriers.empty).toBeLessThan(1);
  });
});

describe('history keeps the war at sea it had', () => {
  it('gives no wings, hides no fleet and offers no strike on ships outside the grand campaign', () => {
    const g = createGame(1939, null);
    expect(carrierRules(g)).toBe(false);
    openCarriers(g);
    expect(carrierRules(g)).toBe(false);
    for (let i = 0; i < 4; i++) runTurnAI(g);
    expect(g.airFleets.some((f) => f.ship !== undefined)).toBe(false);
    expect(g.airReports.some((r) => r.mission === 'fleet' || r.mission === 'harbour')).toBe(false);
    for (const f of g.fleets) { expect(fleetKnown(g, f, 'UK')).toBe('exact'); expect(deckWorth(g, f)).toBe(1); expect(wingCards(g, f)).toEqual([]); expect(capOver(g, f)).toBe(0); }
    const h = createGame(1939, 'UK');
    expect(fleetsSeen(h).length).toBe(h.fleets.filter((f) => shipCount(f.ships) > 0).length);
  });
});

describe('carrier wings', () => {
  it('gives every fleet with carriers a wing of fighters and a wing of strike aircraft, at full strength', () => {
    const g = startOpen(createGame(7, 'UK'));
    expect(carrierRules(g)).toBe(true);
    const decks = g.fleets.filter((f) => f.ships.cv > 0);
    expect(decks.length).toBeGreaterThan(2);
    for (const f of decks) {
      const ws = wingsOf(g, f.id);
      expect(ws.map((w) => w.kind)).toEqual(['ftr', 'bmr']);
      const size = wingSize(f.nat);
      expect(ws[0].ftr).toBe(f.ships.cv * size.ftr);
      expect(ws[0].bmr).toBe(0);
      expect(ws[1].bmr).toBe(f.ships.cv * size.bmr);
      expect(ws[1].ftr).toBe(0);
      expect(ws[0].name).toContain(f.name);
      expect(wingShare(g, f)).toBe(1);
      expect(deckWorth(g, f)).toBe(1);
      const card = fleetCard(g, f);
      expect(card.wings.map((w) => w.kind)).toEqual(['ftr', 'bmr']);
      expect(card.decks).toBe(1);
      expect(card.wings[1].label).toBe('Strike aircraft');
    }
    for (const f of g.fleets.filter((x) => x.ships.cv <= 0)) { expect(wingsOf(g, f.id)).toEqual([]); expect(fleetCard(g, f).wings).toEqual([]); }
    // Opening twice adds nothing.
    const n = g.airFleets.length;
    openCarriers(g);
    expect(g.airFleets.length).toBe(n);
  });

  it('keeps wings off the airfields: they are counted with their fleet, and cannot be moved to a field', () => {
    const g = britain();
    const home = named(g, 'Home Fleet'), w = wing(g, home, 'bmr');
    const port = airfieldCard(g, w.base, 'UK');
    expect(port?.rows.some((r) => r.id === w.id) ?? false).toBe(false);
    const row = myAirGroups(g).find((r) => r.id === w.id)!;
    expect(row.ship).toBe(home.id);
    expect(row.baseName).toBe(home.name);
    expect(row.crowd).toBe(1);
    const jobs = groupJobs(g, w.id);
    expect(jobs[0].label).toBe('Stand ready for a strike');
    expect(groupJobs(g, wing(g, home, 'ftr').id)[0].label).toBe('Cover the fleet');
  });

  it('a new carrier joins with its aircraft aboard, and a carrier sunk takes most of its aircraft with it', () => {
    const g = britain();
    const home = named(g, 'Home Fleet'), size = wingSize('UK');
    const cv = home.ships.cv;
    home.ships.cv = cv + 1;
    syncWings(g);
    expect(wing(g, home, 'ftr').ftr).toBe((cv + 1) * size.ftr);
    expect(wing(g, home, 'bmr').bmr).toBe((cv + 1) * size.bmr);
    expect(wing(g, home, 'bmr').est.bmr).toBe((cv + 1) * size.bmr);
    // One goes down.
    const pool = airPool(g, 'UK'), lost0 = pool.lostBmr, spare0 = pool.bmr;
    home.ships.cv = cv;
    syncWings(g);
    const b = wing(g, home, 'bmr');
    expect(b.est.bmr).toBe(cv * size.bmr);
    expect(b.bmr).toBeLessThanOrEqual(b.est.bmr);
    expect(pool.lostBmr - lost0).toBe(Math.round(size.bmr * (1 - CARRIERS.sunk.saved)));
    expect(b.bmr + (pool.bmr - spare0) + (pool.lostBmr - lost0)).toBe((cv + 1) * size.bmr);
    // The last carrier gone: no wing is left, and what could reach the shore is back in the reserve.
    const before = aboard(g, home);
    const have = pool.ftr + pool.bmr;
    home.ships.cv = 0;
    syncWings(g);
    expect(wingsOf(g, home.id)).toEqual([]);
    expect(pool.ftr + pool.bmr - have).toBeGreaterThan(before * CARRIERS.sunk.saved - 2);
    expect(pool.ftr + pool.bmr - have).toBeLessThan(before * CARRIERS.sunk.saved + 2);
    expect(deckWorth(g, home)).toBe(1);
  });

  it('goes with its carriers when a squadron is split off, and comes back when the fleets join again', () => {
    const g = britain();
    const home = named(g, 'Home Fleet');
    expect(home.ships.cv).toBeGreaterThanOrEqual(2);
    const all = aboard(g, home), cv = home.ships.cv;
    const sq = detach(g, home, { cv: 1, dd: 4 })!;
    expect(sq).toBeTruthy();
    expect(wingsOf(g, sq.id).map((w) => w.kind)).toEqual(['ftr', 'bmr']);
    expect(aboard(g, sq) + aboard(g, home)).toBe(all);
    expect(wing(g, sq, 'bmr').est.bmr).toBe(wingSize('UK').bmr);
    expect(wing(g, home, 'bmr').est.bmr).toBe((cv - 1) * wingSize('UK').bmr);
    expect(wing(g, sq, 'bmr').name).toContain(sq.name);
    expect(merge(g, sq, home)).toBe(true);
    expect(wingsOf(g, sq.id)).toEqual([]);
    expect(aboard(g, home)).toBe(all);
    expect(wingsOf(g, home.id).length).toBe(2);
  });

  it('is made up to strength only in harbour, and an empty deck is worth less in a fight', () => {
    const g = britain();
    const home = named(g, 'Home Fleet');
    put(g, home, 'nsea');
    const b = wing(g, home, 'bmr');
    const full = fleetPower(g, home);
    b.bmr = Math.round(b.est.bmr / 2);
    wing(g, home, 'ftr').ftr = 0;
    expect(wingShare(g, home)).toBeLessThan(0.5);
    expect(deckWorth(g, home)).toBeLessThan(1);
    expect(deckWorth(g, home)).toBeGreaterThan(CARRIERS.empty);
    expect(fleetPower(g, home)).toBeLessThan(full);
    expect(fleetCard(g, home).decks).toBeLessThan(0.5);
    expect(wingCards(g, home)[1].note).toContain('harbour');
    const pool = airPool(g, 'UK');
    pool.bmr += 200; pool.ftr += 200; pool.pilots += 200;
    const had = b.bmr;
    airUpkeep(g, 'UK');
    expect(b.bmr).toBe(had);
    dock(g, home);
    airUpkeep(g, 'UK');
    expect(b.bmr).toBeGreaterThan(had);
    expect(wing(g, home, 'ftr').ftr).toBeGreaterThan(0);
  });

  it('is kept in a saved game, and a grand campaign saved before there were wings is given them', () => {
    const g = britain();
    const home = named(g, 'Home Fleet');
    wing(g, home, 'bmr').bmr -= 7;
    const back = loadGame(saveGame(g))!;
    const h2 = named(back, 'Home Fleet');
    expect(carrierRules(back)).toBe(true);
    expect(wing(back, h2, 'bmr').bmr).toBe(wing(g, home, 'bmr').bmr);
    expect(wing(back, h2, 'bmr').ship).toBe(h2.id);
    expect(back.airFleets.length).toBe(g.airFleets.length);
    // An older save: no flag and no wings.
    const old = startOpen(createGame(7, 'UK'));
    old.airFleets = old.airFleets.filter((f) => f.ship === undefined);
    delete old.flags.carriers;
    const up = loadGame(saveGame(old))!;
    expect(carrierRules(up)).toBe(true);
    for (const f of up.fleets.filter((x) => x.ships.cv > 0)) expect(wingsOf(up, f.id).length).toBe(2);
  });
});

describe('how far a wing reaches', () => {
  it('is anywhere in its own sea, reaches the seas next door as a long flight, and flies from its port in harbour', () => {
    const g = britain();
    const home = named(g, 'Home Fleet');
    put(g, home, 'nsea');
    const b = wing(g, home, 'bmr'), z = g.seas[home.zone];
    expect(kmToSea(g, b, z.id)).toBe(0);
    expect(kmToSea(g, b, z.adj[0])).toBe(CARRIERS.range.next);
    const far = g.seas.find((s) => s.id !== z.id && !z.adj.includes(s.id))!;
    expect(kmToSea(g, b, far.id)).toBe(Infinity);
    for (const id of z.coast) expect(kmTo(g, b, id)).toBe(0);
    expect(groupAt(g, b)).toMatchObject({ sea: z.id, x: z.x, y: z.y, region: -1 });
    expect(airRange(b)).toEqual({ ftr: CARRIERS.range.ftr, bmr: CARRIERS.range.bmr });
    // Inland is measured from the coast of its sea, and has a limit.
    const inland = g.regions.filter((r) => !z.coast.includes(r.id)).map((r) => kmTo(g, b, r.id));
    expect(inland.some((d) => d > 0 && d <= CARRIERS.range.bmr)).toBe(true);
    expect(inland.some((d) => d > CARRIERS.range.bmr)).toBe(true);
    dock(g, home);
    expect(kmTo(g, b, home.base)).toBe(0);
    expect(groupAt(g, b)).toMatchObject({ sea: -1, region: home.base });
    expect(kmToSea(g, b, far.id)).toBeGreaterThan(0);
    expect(kmToSea(g, b, far.id)).toBeLessThan(Infinity);
  });
});

describe('a strike on a fleet at sea', () => {
  it('is planned like any raid: the odds first, then ships sunk whole, and the fleet shaken', () => {
    const { g, foe, strike } = duel();
    const ts = raidTargets(g, 'fleet', 'UK');
    expect(ts.some((t) => t.id === foe.id && t.sea && t.zone === foe.zone && t.known === 'exact')).toBe(true);
    expect(raidMenu(g).find((k) => k.key === 'fleet')!.targets).toBeGreaterThan(0);
    const p = previewRaid(g, 'fleet', foe.id, [strike.id]);
    expect(p.why).toBeNull();
    expect(p.title).toContain(foe.name);
    expect(p.bombers).toBe(strike.bmr);
    expect(p.lines.map((l) => l.label)).toEqual(expect.arrayContaining(["Ships' guns", 'Crews']));
    expect(p.lines.find((l) => l.label === 'Crews')!.value).toMatch(/Carrier crews/);
    expect(p.success).toBeGreaterThan(0.3);
    expect(p.effects.join(' ')).toMatch(/hit/);
    const before = { ...foe.ships }, ready = foe.ready, afloat = surfaceShips(foe);
    const rep = orderRaid(g, 'fleet', foe.id, [strike.id])!;
    expect(rep.mission).toBe('fleet');
    expect(rep.fleet).toBe(foe.id);
    expect(rep.zone).toBe(foe.zone);
    expect(rep.ships![0].fleet).toBe(foe.name);
    expect(rep.ships![0].ships).toEqual(before);
    const sunk = shipCount(rep.ships![0].sunk);
    expect(shipCount(before) - shipCount(foe.ships)).toBe(sunk);
    expect(sunk).toBeLessThanOrEqual(Math.ceil(S.most * afloat) + 1);
    expect(foe.ships.ss).toBe(before.ss);
    expect(foe.ready).toBeLessThan(ready);
    expect(foe.struck).toEqual([g.turn, 1, foe.zone]);
    expect(strike.flew).toBe(g.turn);
    expect(rep.att[0].at).toEqual([g.seas[foe.zone].x, g.seas[foe.zone].y]);
    // It has flown: no second strike this month, and its fleet has shown itself.
    expect(previewRaid(g, 'fleet', foe.id, [strike.id]).why).toBeTruthy();
    expect(foe.seen!.UK).toBe(g.turn);
    expect(named(g, 'Home Fleet').seen!.GER).toBe(g.turn);
  });

  it('sinks about what the ticket says over many strikes, and never more than a third of a fleet at a blow', () => {
    let hits = 0, sunk = 0, expected = 0, worst = 0;
    for (let seed = 1; seed <= 12; seed++) {
      const { g, foe, strike } = duel(seed);
      const a = assessAir(g, [strike], 'fleet', foe.id);
      expect(a.why).toBeNull();
      const fc = forecastAir(g, a, 'fleet', foe.id);
      expected += shipCount(fc.ships!);
      hits += fc.hits!;
      const afloat = surfaceShips(foe);
      const rep = fly(g, 'UK', 'fleet', foe.id, [strike])!;
      const n = shipCount(rep.ships![0].sunk);
      sunk += n;
      worst = Math.max(worst, n / afloat);
    }
    expect(hits / 12).toBeGreaterThan(1);
    expect(sunk).toBeGreaterThan(expected * 0.5);
    expect(sunk).toBeLessThan(expected * 1.6 + 3);
    expect(worst).toBeLessThanOrEqual(S.most + 0.05);
  });

  it('is broken up by carrier fighters over the fleet, and costs bombers to the ships\' guns', () => {
    const { g, foe, strike, fighters } = duel();
    const open = assessAir(g, [strike], 'fleet', foe.id);
    expect(open.spoil).toBe(0);
    expect(open.flak).toBeGreaterThan(0);
    expect(open.flak).toBeLessThanOrEqual(CARRIERS.flak.most);
    expect(open.naval).toBe(CARRIERS.naval.carrier);
    const clear = shipHits(open, 'fleet', strike.bmr).hits;
    // The enemy brings carriers of his own.
    foe.ships.cv = 3;
    syncWings(g);
    const cap = wing(g, foe, 'ftr');
    expect(cap.ftr).toBe(3 * wingSize('GER').ftr);
    const met = assessAir(g, [strike], 'fleet', foe.id);
    expect(met.def.some((d) => d.f.id === cap.id)).toBe(true);
    expect(met.spoil).toBeGreaterThan(0.2);
    expect(met.spoil).toBeLessThanOrEqual(CARRIERS.cap.spoil);
    expect(shipHits(met, 'fleet', strike.bmr).hits).toBeLessThan(clear);
    expect(forecastAir(g, met, 'fleet', foe.id).bomberLoss).toBeGreaterThan(forecastAir(g, open, 'fleet', foe.id).bomberLoss);
    const p = previewRaid(g, 'fleet', foe.id, [strike.id]);
    expect(p.meets.some((m) => m.afloat && m.baseName === foe.name)).toBe(true);
    expect(p.lines.some((l) => l.label === 'Fighter cover')).toBe(true);
    // An escort of the fleet's own fighters cuts what the cover spoils.
    const withEscort = assessAir(g, [strike, fighters], 'fleet', foe.id);
    expect(withEscort.spoil).toBeLessThan(met.spoil);
    // Carrier fighters also keep the coast's bombers off their own fleet, month by month.
    expect(capOver(g, foe)).toBeGreaterThan(0);
    expect(capOver(g, foe)).toBeLessThanOrEqual(CARRIERS.cap.most);
  });

  it('finds a scattered fleet harder to hit a second time in a month, and land bombers poor at it', () => {
    const { g, foe, strike } = duel();
    expect(assessAir(g, [strike], 'fleet', foe.id).again).toBe(1);
    foe.struck = [g.turn, 1, foe.zone];
    expect(assessAir(g, [strike], 'fleet', foe.id).again).toBe(S.again);
    foe.struck = [g.turn - 1, 2, foe.zone];
    expect(assessAir(g, [strike], 'fleet', foe.id).again).toBe(1);
    // Bombers from an airfield: put the two fleets where they can reach.
    const home = named(g, 'Home Fleet');
    put(g, foe, 'nsea'); put(g, home, 'nsea');
    const land = airFleetsOf(g, 'UK').filter((f) => f.ship === undefined && f.kind === 'bmr' && f.bmr > 0).find((f) => assessAir(g, [f], 'fleet', foe.id).why === null)!;
    expect(land).toBeTruthy();
    const a = assessAir(g, [land], 'fleet', foe.id);
    expect(a.naval).toBe(CARRIERS.naval.land.UK);
    expect(a.lines.find((l) => l.label === 'Crews')!.value).toMatch(/Land bombers/);
    // Flown together, the carrier crews and the land crews each count for what they are worth.
    const both = assessAir(g, [strike, land], 'fleet', foe.id);
    expect(both.naval).toBeGreaterThan(a.naval);
    expect(both.naval).toBeLessThan(CARRIERS.naval.carrier);
  });
});

describe('a raid on a harbour', () => {
  it('goes for the warships lying there, hits harder than at sea, and catches a harbour unready in the month war is declared', () => {
    const g = britain();
    const home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet');
    home.ships.cv = 6;
    dock(g, foe);
    const port = foe.base;
    const z = g.seas.find((s) => s.coast.includes(port))!;
    home.zone = z.id; home.stance = 'patrol'; syncWings(g);
    const strike = wing(g, home, 'bmr');
    expect(shipsInPort(g, port, 'UK')).toContain(foe);
    expect(raidTargets(g, 'harbour', 'UK').some((t) => t.id === port && !t.sea)).toBe(true);
    const a = assessAir(g, [strike], 'harbour', port);
    expect(a.why).toBeNull();
    expect(a.ships).toContain(foe);
    expect(a.surprise).toBe(false);
    expect(a.lines.some((l) => l.label === 'In harbour')).toBe(true);
    // The same bombers through do more to ships at anchor than to ships under way.
    const atAnchor = shipHits(a, 'harbour', 100), under = shipHits(a, 'fleet', 100);
    expect(atAnchor.hits).toBeGreaterThan(under.hits);
    expect(atAnchor.hits / under.hits).toBeCloseTo(CARRIERS.harbour.hit, 1);
    // War declared this month.
    g.flags['war:UK:GER'] = g.turn;
    const s = assessAir(g, [strike], 'harbour', port);
    expect(s.surprise).toBe(true);
    expect(s.lines.some((l) => l.label === 'Surprise')).toBe(true);
    expect(s.D).toBeLessThan(a.D + 0.001);
    expect(shipHits(s, 'harbour', 100).hits).toBeGreaterThan(atAnchor.hits);
    const p = planRaid(g, 'harbour', port, [strike.id], 'UK');
    expect(p.why).toBeNull();
    expect(p.title).toMatch(/harbour/i);
    const before = shipCount(foe.ships);
    const rep = orderRaid(g, 'harbour', port, [strike.id])!;
    expect(rep.mission).toBe('harbour');
    expect(rep.region).toBe(port);
    expect(rep.surprise).toBe(true);
    expect(rep.ships!.some((x) => x.fleet === foe.name)).toBe(true);
    expect(before - shipCount(foe.ships)).toBe(shipCount(rep.ships!.find((x) => x.fleet === foe.name)!.sunk));
    expect(foe.struck![2]).toBe(-1);
    // A harbour with no warships in it is no target.
    put(g, foe, 'natl');
    expect(raidTargets(g, 'harbour', 'UK').some((t) => t.id === port)).toBe(false);
  });
});

describe('finding ships at sea', () => {
  it('knows its own side, reports ships in harbour, and sees a fleet at sea only as well as that sea is searched', () => {
    const g = britain();
    const home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet');
    expect(fleetFound(g, home, 'UK')).toBe('exact');
    expect(fleetFound(g, named(g, 'Mediterranean Fleet'), 'FRA')).toBe('exact');
    dock(g, foe); delete foe.seen;
    expect(['report', 'exact']).toContain(fleetFound(g, foe, 'UK'));
    // A country at peace with you hides nothing in its harbours.
    const neutral = g.fleets.find((f) => !atSea(f) && shipCount(f.ships) > 0 && !g.nations[f.nat].atWar.length && f.nat !== 'UK')!;
    expect(fleetFound(g, neutral, 'UK')).toBe('exact');
    // In a far gulf with nobody looking: no British ship there, no carrier next door, no British coast.
    dock(g, home);
    put(g, foe, 'bothnia');
    const z = foe.zone;
    const p = seaPicture(g, z, 'UK');
    expect(p.needFind).toBeGreaterThan(p.needReport);
    expect(p.needFind).toBeGreaterThan(CARRIERS.search.find);
    expect(searchOf(g, 'UK', z)).toBeLessThan(p.needReport);
    {
      expect(fleetFound(g, foe, 'UK')).toBe('none');
      expect(fleetsSeen(g, 'UK').some((s) => s.id === foe.id)).toBe(false);
      expect(fleetsInSight(g).some((s) => s.id === foe.id)).toBe(false);
      expect(fleetView(g, foe.id)).toBeNull();
      expect(raidTargets(g, 'fleet', 'UK').some((t) => t.id === foe.id)).toBe(false);
      expect(planRaid(g, 'fleet', foe.id, undefined, 'UK').why).toMatch(/found/);
      expect(seaPicture(g, z, 'UK').word).toMatch(/Not searched/);
    }
    // The Home Fleet arrives: ships and carriers in a sea search it.
    home.ships.cv = 4;
    put(g, home, 'bothnia');
    expect(searchOf(g, 'UK', z)).toBeGreaterThanOrEqual(p.needFind);
    expect(fleetFound(g, foe, 'UK')).toBe('exact');
    const s = fleetsInSight(g).find((x) => x.id === foe.id)!;
    expect(s).toMatchObject({ known: 'exact', side: 'foe', atSea: true, zone: z, name: foe.name });
    expect(s.ships).toEqual(foe.ships);
    expect(fleetView(g, foe.id)).not.toBeNull();
    expect(seaPictureOf(g, z)!.theirs).toBeGreaterThan(0);
  });

  it('gives a report, not an identification, where the search is thin: a flag and a rough size', () => {
    const g = britain();
    const home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet');
    dock(g, home);
    for (const f of g.fleets) if (f !== foe && f.nat !== 'GER' && atSea(f)) dock(g, f);
    put(g, foe, 'natl');
    delete foe.seen;
    const z = foe.zone, need = seaPicture(g, z, 'UK');
    // Met last month: reported this month even where nobody is searching.
    foe.seen = { UK: g.turn - 1 };
    expect(searchOf(g, 'UK', z)).toBeLessThan(need.needFind);
    {
      expect(fleetFound(g, foe, 'UK')).toBe('report');
      const s = fleetsInSight(g).find((x) => x.id === foe.id)!;
      expect(s.known).toBe('report');
      expect(s.ships).toBeNull();
      expect(s.count).toBe(-1);
      expect(s.carriers).toBe(-1);
      expect(s.name).toBe('German ships reported');
      expect(s.size.length).toBeGreaterThan(3);
      expect(fleetView(g, foe.id)).toBeNull();
      expect(wingsOfFleet(g, foe.id)).toEqual([]);
      expect(seaPictureOf(g, z)!.reported).toBe(1);
      // It can be struck, but part of the strike never finds it.
      const t = raidTargets(g, 'fleet', 'UK').find((x) => x.id === foe.id);
      if (t) expect(t.known).toBe('report');
      expect(t === undefined ? 'no group reaches it' : t.known).toBeTruthy();
    }
    // Met this month: identified.
    foe.seen = { UK: g.turn };
    expect(fleetFound(g, foe, 'UK')).toBe('exact');
    // Nobody at the table, or the fog lifted: everything is known.
    delete foe.seen;
    expect(fleetKnown(g, foe, null)).toBe('exact');
    setFog(g, false);
    expect(fleetKnown(g, foe)).toBe('exact');
    expect(fleetsInSight(g).length).toBe(g.fleets.filter((f) => shipCount(f.ships) > 0).length);
  });

  it('lets a group fly a search of one sea a month, which adds to what the ships and the coasts see', () => {
    const g = britain();
    const home = named(g, 'Home Fleet');
    put(g, home, 'nsea');
    const ftr = wing(g, home, 'ftr');
    const opts = searchOptionsFor(g, ftr.id);
    expect(opts[0]).toMatchObject({ zone: home.zone, km: 0 });
    expect(opts.every((o) => o.zone === home.zone || g.seas[home.zone].adj.includes(o.zone))).toBe(true);
    expect(searchOptions(g, ftr.id, 'GER')).toEqual([]);
    const z = opts.find((o) => o.zone !== home.zone)!.zone;
    const before = searchOf(g, 'UK', z), ready = ftr.ready;
    const res = orderSearch(g, ftr.id, z);
    expect(res.ok).toBe(true);
    expect(res.text).toContain(g.seas[z].name);
    expect(searchOf(g, 'UK', z)).toBeCloseTo(before + CARRIERS.search.flight, 5);
    expect(ftr.scouted).toBe(g.turn);
    expect(ftr.ready).toBeLessThan(ready);
    expect(searchOptionsFor(g, ftr.id)).toEqual([]);
    expect(orderSearch(g, ftr.id, z).ok).toBe(false);
    expect(searchSea(g, ftr.id, 9999).ok).toBe(false);
    expect(searchOptionsFor(g, wing(g, home, 'bmr').id).find((o) => o.zone === z)!.searched).toBe(true);
    // A search can turn nothing into a report, or a report into a name.
    const foe = named(g, 'Battle Fleet');
    delete foe.seen;
    foe.zone = z; foe.stance = 'patrol';
    const k = fleetFound(g, foe, 'UK');
    g.intel.sea = {};
    const rank = { none: 0, report: 1, exact: 2 } as const;
    expect(rank[k]).toBeGreaterThanOrEqual(rank[fleetFound(g, foe, 'UK')]);
  });

  it('shows the player\'s fleet only the enemy it has found when it looks at the seas in reach', () => {
    const g = britain();
    const home = named(g, 'Home Fleet');
    const o = fleetOptions(g, home.id)!;
    expect(o.seas.length).toBeGreaterThan(2);
    for (const s of o.seas) { expect(typeof s.search).toBe('string'); expect(s.reported).toBeGreaterThanOrEqual(0); }
    // With no viewer the staff counts everything; a viewer counts what he has found.
    const all = seaOptions(g, home), seen = seaOptions(g, home, 'UK');
    expect(all.length).toBe(seen.length);
    for (let i = 0; i < all.length; i++) expect(seen[i].surface).toBeLessThanOrEqual(all[i].surface);
  });
});

describe('the computer at sea', () => {
  it('strikes fleets it has found and ships in harbour, and keeps every wing in order through a war', () => {
    const g = startOpen(createGame(7, null));
    while (g.turn <= 30) runTurnAI(g);
    const strikes = g.airReports.filter((r) => r.mission === 'fleet' || r.mission === 'harbour');
    expect(strikes.length).toBeGreaterThan(2);
    expect(strikes.every((r) => (r.ships?.length ?? 0) > 0)).toBe(true);
    expect(strikes.some((r) => r.ships!.some((x) => shipCount(x.sunk) > 0))).toBe(true);
    for (const w of g.airFleets.filter((f) => f.ship !== undefined)) {
      const c = g.fleets.find((f) => f.id === w.ship)!;
      expect(c).toBeTruthy();
      expect(c.ships.cv).toBeGreaterThan(0);
      expect(w.ftr).toBeGreaterThanOrEqual(0);
      expect(w.bmr).toBeGreaterThanOrEqual(0);
      expect(w.ftr).toBeLessThanOrEqual(w.est.ftr);
      expect(w.bmr).toBeLessThanOrEqual(w.est.bmr);
      expect(w.est.ftr + w.est.bmr).toBe(c.ships.cv * (w.kind === 'ftr' ? wingSize(c.nat).ftr : wingSize(c.nat).bmr));
    }
    for (const c of g.fleets.filter((f) => f.ships.cv > 0 && g.nations[f.nat].alive)) expect(wingsOf(g, c.id).length).toBe(2);
  }, 120000);
});

describe('the pages the 3D game reads', () => {
  it('lays out a fleet\'s wings, a search of the seas, and the way between them', () => {
    const g = britain();
    const home = named(g, 'Home Fleet');
    put(g, home, 'nsea');
    const b = wing(g, home, 'bmr'), f = wing(g, home, 'ftr');
    const wings = airPage(g, `wings:${home.id}`)!;
    expect(wings.title).toContain('HOME FLEET');
    expect(wings.sections[0].rows.map((r) => r.title)).toEqual([f.name, b.name]);
    expect(wings.sections[0].rows[0].buttons[0].act).toBe(`open:group:${f.id}`);
    const group = airPage(g, `group:${b.id}`)!;
    const labels = group.top.map((t) => t.label);
    expect(labels).toContain('BACK TO THE FLEET');
    expect(labels).toContain('SEARCH A SEA');
    expect(labels.some((l) => /MOVE/.test(l))).toBe(false);
    const search = airPage(g, `search:${f.id}`)!;
    expect(search.sections[0].rows.length).toBeGreaterThan(1);
    const act = search.sections[0].rows[1].buttons[0].act;
    expect(act).toMatch(new RegExp(`^search:${f.id}:\\d+$`));
    const res = airAct(g, act)!;
    expect(res.ok).toBe(true);
    expect(f.scouted).toBe(g.turn);
    expect(airPage(g, `search:${f.id}`)!.sections[0].rows.length).toBe(0);
    // A fleet that is not identified has no page of wings.
    const foe = named(g, 'Battle Fleet');
    foe.ships.cv = 1; syncWings(g);
    for (const x of g.fleets) if (x.nat !== 'GER' && atSea(x)) sail(g, x, x.home, 'port');
    foe.zone = sea(g, 'natl'); foe.stance = 'patrol'; delete foe.seen; g.intel.sea = {};
    expect(fleetFound(g, foe, 'UK')).not.toBe('exact');
    expect(airPage(g, `wings:${foe.id}`)).toBeNull();
    // A strike flown from a deck takes off from the fleet, and the planner comes back to the fleet's wings.
    put(g, home, 'nsea');
    foe.zone = home.zone;
    const flown = airAct(g, `raid:fleet:${foe.id}:${b.id}`);
    expect(flown?.ok).toBe(true);
    {
      expect(flown!.open).toBe(`wings:${home.id}`);
      expect(flown!.fly).toMatchObject({ fromX: g.seas[home.zone].x, fromY: g.seas[home.zone].y });
    }
  });
});
