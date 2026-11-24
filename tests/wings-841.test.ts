// Wings ashore (Version 84.1, ruling 175(b); the grand campaign). A carrier's wing can be ordered ashore to a
// friendly airfield with room when its fleet lies in that field's harbour or in the sea off it. Ashore it is an air
// group like any other, with its own aircraft, pilots and fitness, and a carrier wing's reach. The same order sends
// it back aboard a fleet of its navy with deck room standing empty there. While it is ashore its carriers fight as
// empty carriers. The move costs what a move between airfields costs. It is the player's order only. And a wing
// aboard is at no airfield: nothing counts it or draws it at one (ruling 176(b)).
import { describe, expect, it } from 'vitest';
import { AIR, CARRIERS, airPool, airRange, airfieldsHold, fieldRoom } from '../src/campaign/air';
import { airAct, airPage } from '../src/campaign/airpages';
import { bareLine, bareOf, deckWorth, flyAboard, flyAshore, syncWings, whyNoDeck, whyNoShore, whyNotAboard, whyNotAshore, wingShare, wingSize, wingsAshore, wingsOf } from '../src/campaign/carriers';
import { detach, fleetCard, merge } from '../src/campaign/fleets';
import { createGame } from '../src/campaign/game';
import { atSea, sail } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { airGroupBases, airGroupDecks, airfieldAt, beginHuman, endHuman, humanUp, myAirGroups, orderAboard, orderAirBase, stepAI } from '../src/campaign/turn';
import type { AirFleet, Fleet, Game, Region } from '../src/campaign/types';

/** A grand campaign with America at the table, in her first month to give orders. */
function america(seed = 3): Game {
  const g = startOpen(createGame(seed, 'USA'));
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
  return g;
}
/** The month ends and America's next one begins. */
function nextMonth(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  beginHuman(g);
}
const carriers = (g: Game): Fleet => g.fleets.find((f) => f.nat === 'USA' && f.ships.cv > 0 && f.base >= 0 && !atSea(f))!;
const wing = (g: Game, f: Fleet, kind: 'ftr' | 'bmr'): AirFleet => wingsOf(g, f.id).find((w) => w.kind === kind)!;
const row = (g: Game, id: number) => myAirGroups(g).find((x) => x.id === id)!;
function setup(seed = 3): { g: Game; fleet: Fleet; port: Region; ftr: AirFleet; bmr: AirFleet } {
  const g = america(seed);
  const fleet = carriers(g);
  expect(fleet).toBeDefined();
  return { g, fleet, port: g.regions[fleet.base], ftr: wing(g, fleet, 'ftr'), bmr: wing(g, fleet, 'bmr') };
}

describe('a wing aboard is at no airfield', () => {
  it('its row has no base, names its fleet and its harbour, and the airfield there lists nobody', () => {
    const { g, fleet, port, ftr } = setup();
    const x = row(g, ftr.id);
    expect(x.ship).toBe(fleet.id);
    expect(x.base).toBe(-1);
    expect(x.port).toBe(port.id);
    expect(x.ashore).toBe(-1);
    expect(x.where).toBe(`Aboard the ${fleet.name}`);
    expect(x.wingMove).toEqual({ order: 'ashore', price: 'While it is ashore its carriers fight as empty carriers. The move costs it some readiness for a month.', why: null });
    expect(airfieldAt(g, port.id)!.rows.some((r) => r.id === ftr.id)).toBe(false);
    // A group that was never a wing has no such order.
    const land = myAirGroups(g).find((r) => r.ship < 0);
    if (land) expect(land.wingMove).toBeNull();
  });
});

describe('a wing ordered ashore', () => {
  it('from harbour it can go only to the airfield there, and becomes an air group on it', () => {
    const { g, fleet, port, ftr, bmr } = setup();
    const size = wingSize('USA'), n = ftr.ftr, est = ftr.est.ftr, ready = ftr.ready;
    expect(est).toBe(fleet.ships.cv * size.ftr);
    const fields = airGroupBases(g, ftr.id);
    expect(fields.map((b) => b.region)).toEqual([port.id]);
    expect(fields[0].why).toBeNull();
    const other = g.regions.find((r) => r.ctrl === 'USA' && r.id !== port.id && airfieldsHold(g, r.id) > 0)!;
    expect(whyNotAshore(g, ftr, other)).toBe(`The ${fleet.name} lies at ${fleet.port}: from harbour its wing can fly ashore only to the airfield there.`);
    expect(orderAirBase(g, ftr.id, other.id)).toBe(false);
    expect(ftr.ship).toBe(fleet.id);

    expect(orderAirBase(g, ftr.id, port.id)).toBe(true);
    expect(ftr.ship).toBeUndefined();
    expect(ftr.ashore).toBe(fleet.id);
    expect(ftr.base).toBe(port.id);
    expect(ftr.ftr).toBe(n);
    expect(ftr.est.ftr).toBe(est);
    expect(ftr.moved).toBe(g.turn);
    // The move costs what a move between airfields costs.
    expect(ftr.ready).toBeCloseTo(Math.max(AIR.ready.floor, ready - AIR.ready.move), 9);
    expect(fleet.bare).toEqual({ ftr: est, bmr: 0 });
    expect(bareOf(fleet, 'ftr')).toBe(est);
    // It is on the airfield's list like any group, named as a wing ashore with its fleet.
    expect(airfieldAt(g, port.id)!.rows.some((r) => r.id === ftr.id)).toBe(true);
    const x = row(g, ftr.id);
    expect(x.base).toBe(port.id);
    expect(x.ship).toBe(-1);
    expect(x.ashore).toBe(fleet.id);
    expect(x.fleetName).toBe(fleet.name);
    expect(x.where).toBe(`Ashore at ${port.name}`);
    expect(x.wingMove!.order).toBe('aboard');
    expect(x.wingMove!.why).toBe('It has already changed base this month.');
    // It keeps a carrier wing's reach.
    expect(airRange(ftr).ftr).toBe(CARRIERS.range.ftr);
    // Its carriers fight as carriers with that much of their decks empty, and no new wing is raised for the room.
    const share = bmr.bmr / (bmr.est.bmr + est);
    expect(wingShare(g, fleet)).toBeCloseTo(share, 9);
    expect(deckWorth(g, fleet)).toBeCloseTo(CARRIERS.empty + (1 - CARRIERS.empty) * share, 9);
    syncWings(g);
    expect(wingsOf(g, fleet.id).map((w) => w.kind)).toEqual(['bmr']);
    expect(wingsAshore(g, fleet.id).map((w) => w.id)).toEqual([ftr.id]);
  });

  it('with every wing ashore the fleet fights as empty carriers, and says so', () => {
    const { g, fleet, port, ftr, bmr } = setup();
    expect(deckWorth(g, fleet)).toBe(1);
    expect(bareLine(g, fleet)).toBe('');
    expect(flyAshore(g, ftr, port)).toBe(true);
    expect(bareLine(g, fleet)).toMatch(/^Part of its wings are ashore: its carriers fight at \d+% of their worth/);
    expect(fieldRoom(g, port.id, { ...bmr, ship: undefined } as AirFleet)).toBe(true);
    expect(flyAshore(g, bmr, port)).toBe(true);
    expect(wingsOf(g, fleet.id)).toEqual([]);
    expect(wingShare(g, fleet)).toBe(0);
    expect(deckWorth(g, fleet)).toBe(CARRIERS.empty);
    const card = fleetCard(g, fleet);
    expect(card.decks).toBe(0);
    expect(card.aboard).toBe(0);
    expect(card.deckRoom).toBe(fleet.ships.cv * (wingSize('USA').ftr + wingSize('USA').bmr));
    expect(card.bare).toBe('Its wings are ashore: its carriers fight as empty carriers.');
    expect(card.ashore.map((w) => w.id).sort()).toEqual([ftr.id, bmr.id].sort());
    expect(card.ashore[0].at).toBe(port.name);
    // A month on, nothing has raised new wings for the empty decks, and the campaign saves and loads as it is.
    nextMonth(g);
    expect(wingsOf(g, fleet.id)).toEqual([]);
    expect(fleet.bare).toEqual({ ftr: ftr.est.ftr, bmr: bmr.est.bmr });
    expect(ftr.ashore).toBe(fleet.id);
    const text = saveGame(g);
    const h = loadGame(text)!;
    expect(saveGame(h)).toBe(text);
    expect(h.fleets.find((f) => f.id === fleet.id)!.bare).toEqual(fleet.bare);
    expect(h.airFleets.find((f) => f.id === ftr.id)!.ashore).toBe(fleet.id);
  });

  it('the same order sends it back aboard, where its fleet lies', () => {
    const { g, fleet, port, ftr } = setup();
    const est = ftr.est.ftr, name = ftr.name;
    expect(flyAshore(g, ftr, port)).toBe(true);
    // Not twice in a month.
    expect(whyNotAboard(g, ftr, fleet)).toBe('It has already changed base this month.');
    expect(orderAboard(g, ftr.id, fleet.id)).toBe(false);
    nextMonth(g);
    const decks = airGroupDecks(g, ftr.id);
    expect(decks).toEqual([{ fleet: fleet.id, name: fleet.name, where: `in harbour at ${fleet.port}`, room: est, why: null }]);
    expect(row(g, ftr.id).wingMove).toEqual({ order: 'aboard', price: 'Going aboard costs it some readiness for a month, as a move between airfields does.', why: null });
    const n = ftr.ftr, ready = ftr.ready;
    expect(orderAboard(g, ftr.id, fleet.id)).toBe(true);
    expect(ftr.ship).toBe(fleet.id);
    expect(ftr.ashore).toBeUndefined();
    expect(ftr.name).toBe(name);
    expect(ftr.est).toEqual({ ftr: est, bmr: 0 });
    expect(ftr.ftr).toBe(Math.min(n, est));
    expect(ftr.ready).toBeCloseTo(Math.max(AIR.ready.floor, ready - AIR.ready.move), 9);
    expect(fleet.bare).toBeUndefined();
    expect(wingsOf(g, fleet.id).map((w) => w.kind)).toEqual(['ftr', 'bmr']);
    expect(row(g, ftr.id).base).toBe(-1);
    syncWings(g);
    expect(ftr.est.ftr).toBe(est);
    expect(ftr.ftr).toBe(Math.min(n, est));
  });

  it('from sea it goes to a friendly airfield on that sea, and back only while its fleet is off that coast', () => {
    const { g, fleet, ftr } = setup();
    sail(g, fleet, fleet.home, 'patrol');
    expect(atSea(fleet)).toBe(true);
    const coast = g.seas[fleet.zone].coast;
    const fields = airGroupBases(g, ftr.id);
    expect(fields.length).toBeGreaterThan(0);
    for (const b of fields) { expect(coast).toContain(b.region); expect(g.regions[b.region].ctrl).toBe('USA'); }
    const inland = g.regions.find((r) => r.ctrl === 'USA' && !coast.includes(r.id) && airfieldsHold(g, r.id) > 0)!;
    // Where the fleet is, as the UI director's sea words say it (13:29Z): a coast's sea is "off" it.
    expect(g.seas[fleet.zone].name).toBe('Eastern Seaboard');
    expect(whyNotAshore(g, ftr, inland)).toBe(`The ${fleet.name} is off the Eastern Seaboard: its wing can fly ashore only to an airfield on that sea.`);
    const to = fields.find((b) => !b.why)!;
    expect(orderAirBase(g, ftr.id, to.region)).toBe(true);
    expect(ftr.base).toBe(to.region);
    nextMonth(g);
    // The fleet has gone into harbour somewhere the wing is not: the wing must be where the fleet is.
    sail(g, fleet, fleet.home, 'port');
    if (fleet.base !== ftr.base) {
      expect(whyNotAboard(g, ftr, fleet)).toBe(`The ${fleet.name} lies at ${fleet.port}: the wing must be at the airfield there to go aboard.`);
      expect(whyNoDeck(g, ftr)).toBe(whyNotAboard(g, ftr, fleet));
    }
    sail(g, fleet, fleet.home, 'patrol');
    expect(whyNotAboard(g, ftr, fleet)).toBeNull();
    expect(flyAboard(g, ftr, fleet)).toBe(true);
    expect(ftr.ship).toBe(fleet.id);
  });

  it('is refused with its reason: no aircraft, flown, no room, not a wing, not the player', () => {
    const { g, fleet, port, ftr, bmr } = setup();
    // A full airfield has no room.
    const crowd: AirFleet = { ...ftr, id: g.nextAir++, ship: undefined, base: port.id, ftr: airfieldsHold(g, port.id) + 1, name: 'A crowd' };
    g.airFleets.push(crowd);
    expect(whyNotAshore(g, ftr, port)).toBe(`The airfield at ${port.name} has no room for it.`);
    expect(whyNoShore(g, ftr)).toBe(`The airfield at ${port.name} has no room for it.`);
    expect(row(g, ftr.id).wingMove!.why).toBe(`The airfield at ${port.name} has no room for it.`);
    g.airFleets.splice(g.airFleets.indexOf(crowd), 1);
    expect(whyNoShore(g, ftr)).toBeNull();
    bmr.flew = g.turn;
    expect(whyNoShore(g, bmr)).toBe('It flew its strike this month: it is back aboard next month.');
    const had = ftr.ftr;
    ftr.ftr = 0;
    expect(whyNoShore(g, ftr)).toBe('It has no aircraft to send ashore.');
    ftr.ftr = had;
    // A group on land is no wing, and another navy's wing is not the player's to order.
    expect(whyNotAboard(g, crowd, fleet)).toBe('Only a carrier wing ashore can be sent aboard.');
    const theirs = g.airFleets.find((f) => f.ship !== undefined && f.nat !== 'USA')!;
    const their = g.fleets.find((f) => f.id === theirs.ship)!;
    if (!atSea(their) && their.base >= 0) expect(orderAirBase(g, theirs.id, their.base)).toBe(false);
    expect(theirs.ship).toBe(their.id);
  });
});

describe('wings ashore and the carriers they left', () => {
  /** Two carriers, fighters ashore. */
  function two(): { g: Game; fleet: Fleet; port: Region; ftr: AirFleet; bmr: AirFleet; size: { ftr: number; bmr: number } } {
    const s = setup();
    s.fleet.ships.cv = 2;
    syncWings(s.g);
    const size = wingSize('USA');
    expect(s.ftr.est.ftr).toBe(2 * size.ftr);
    expect(flyAshore(s.g, s.ftr, s.port)).toBe(true);
    expect(s.fleet.bare).toEqual({ ftr: 2 * size.ftr, bmr: 0 });
    return { ...s, size };
  }

  it('a squadron split off takes the carriers with aircraft aboard first, and a fleet joined brings its empty decks', () => {
    const { g, fleet, ftr, bmr, size } = two();
    const strike = bmr.bmr;
    const sq = detach(g, fleet, { cv: 1, dd: Math.min(1, fleet.ships.dd) })!;
    expect(sq).not.toBeNull();
    // One empty fighter deck goes with the carrier that left; the strike aircraft divide with the carriers.
    expect(fleet.bare).toEqual({ ftr: size.ftr, bmr: 0 });
    expect(sq.bare).toEqual({ ftr: size.ftr, bmr: 0 });
    expect(wingsOf(g, fleet.id).map((w) => w.kind)).toEqual(['bmr']);
    expect(wingsOf(g, sq.id).map((w) => w.kind)).toEqual(['bmr']);
    expect(wingsOf(g, fleet.id)[0].bmr + wingsOf(g, sq.id)[0].bmr).toBe(strike);
    expect(wingsOf(g, fleet.id)[0].est.bmr).toBe(size.bmr);
    expect(ftr.ashore).toBe(fleet.id);
    expect(merge(g, sq, fleet)).toBe(true);
    expect(fleet.bare).toEqual({ ftr: 2 * size.ftr, bmr: 0 });
    expect(wingsOf(g, fleet.id).map((w) => w.kind)).toEqual(['bmr']);
    expect(wingsOf(g, fleet.id)[0].bmr).toBe(strike);
    expect(wingsOf(g, fleet.id)[0].est.bmr).toBe(2 * size.bmr);
    // Joined the other way, the wing ashore belongs to the fleet its carriers now sail with.
    const sq2 = detach(g, fleet, { cv: 1 })!;
    expect(merge(g, fleet, sq2)).toBe(true);
    expect(ftr.ashore).toBe(sq2.id);
    expect(sq2.bare).toEqual({ ftr: 2 * size.ftr, bmr: 0 });
    expect(g.fleets.includes(fleet)).toBe(false);
  });

  it('a carrier lost while its wing is ashore loses no aircraft, only the deck to come back to', () => {
    const { g, fleet, ftr, size } = two();
    const n = ftr.ftr, lost = ftr.lost.ftr;
    fleet.ships.cv = 1;
    syncWings(g);
    expect(ftr.ftr).toBe(n);
    expect(ftr.lost.ftr).toBe(lost);
    expect(fleet.bare).toEqual({ ftr: size.ftr, bmr: 0 });
    expect(wingsOf(g, fleet.id).map((w) => w.kind)).toEqual(['bmr']);
    nextMonth(g);
    // Back aboard, only what the deck that is left holds goes: the rest stay ashore in the reserve.
    const pool = airPool(g, 'USA').ftr, aircraft = ftr.ftr;
    expect(flyAboard(g, ftr, fleet)).toBe(true);
    expect(ftr.est.ftr).toBe(size.ftr);
    expect(ftr.ftr).toBe(Math.min(aircraft, size.ftr));
    expect(airPool(g, 'USA').ftr).toBe(pool + Math.max(0, aircraft - size.ftr));
    expect(fleet.bare).toBeUndefined();
  });

  it('with no carrier left the wing stays an air group ashore', () => {
    const { g, fleet, ftr } = two();
    const n = ftr.ftr;
    fleet.ships.cv = 0;
    syncWings(g);
    expect(fleet.bare).toBeUndefined();
    expect(ftr.ftr).toBe(n);
    expect(g.airFleets.includes(ftr)).toBe(true);
    expect(airGroupDecks(g, ftr.id)).toEqual([]);
    expect(whyNoDeck(g, ftr)).toBe('No carrier of its navy has deck room standing empty for it.');
  });

  it('a new carrier joins with its own aircraft, and the wing that comes back is one wing with them', () => {
    const { g, fleet, ftr, size } = two();
    fleet.ships.cv = 3;
    syncWings(g);
    const fresh = wingsOf(g, fleet.id).find((w) => w.kind === 'ftr')!;
    expect(fresh.est.ftr).toBe(size.ftr);
    expect(fresh.ftr).toBe(size.ftr);
    expect(fleet.bare).toEqual({ ftr: 2 * size.ftr, bmr: 0 });
    nextMonth(g);
    const a = ftr.ftr, b = fresh.ftr;
    expect(flyAboard(g, ftr, fleet)).toBe(true);
    expect(g.airFleets.includes(ftr)).toBe(false);
    expect(fresh.est.ftr).toBe(3 * size.ftr);
    expect(fresh.ftr).toBe(a + b);
    expect(fleet.bare).toBeUndefined();
    syncWings(g);
    expect(fresh.ftr).toBe(a + b);
  });
});

describe('the pages both games draw', () => {
  it('a wing aboard has FLY ASHORE, a wing ashore BACK ABOARD, and the fleet says where its aircraft are', () => {
    const { g, fleet, port, ftr } = setup();
    const top = (id: number) => airPage(g, `group:${id}`)!.top;
    const ashore = top(ftr.id).find((b) => b.label === 'FLY ASHORE')!;
    expect(ashore.ok).toBe(true);
    expect(ashore.act).toBe(`airbase:${ftr.id}:${port.id}`);
    expect(ashore.tip).toBe(`To the airfield at ${port.name}. While it is ashore its carriers fight as empty carriers. The move costs it some readiness for a month.`);
    expect(top(ftr.id).some((b) => b.label === 'MOVE TO ANOTHER AIRFIELD')).toBe(false);
    expect(airPage(g, `rebase:${ftr.id}`)!.sections[0].rows.map((r) => r.title)).toEqual([port.name]);
    const done = airAct(g, ashore.act)!;
    expect(done.ok).toBe(true);
    expect(done.note).toBe(`The ${ftr.name} flies ashore to ${port.name}.`);
    const page = airPage(g, `group:${ftr.id}`)!;
    expect(page.headline).toMatch(new RegExp(`ashore at ${port.name}$`));
    expect(page.note).toContain(`A carrier wing of the ${fleet.name}, ordered ashore`);
    const back = page.top.find((b) => b.label === 'BACK ABOARD')!;
    expect(back.ok).toBe(false);
    expect(back.tip).toBe('It has already changed base this month.');
    const wings = airPage(g, `wings:${fleet.id}`)!;
    expect(wings.headline).toContain(' aircraft aboard. Part of its wings are ashore');
    expect(wings.sections.map((s) => s.heading)).toEqual(['THE WINGS', 'ASHORE']);
    expect(wings.sections[1].rows.map((r) => r.title)).toEqual([ftr.name]);
    nextMonth(g);
    const again = top(ftr.id).find((b) => b.label === 'BACK ABOARD')!;
    expect(again.ok).toBe(true);
    expect(again.act).toBe(`aboard:${ftr.id}:${fleet.id}`);
    expect(airPage(g, `aboard:${ftr.id}`)!.sections[0].rows.map((r) => r.title)).toEqual([fleet.name]);
    const home = airAct(g, again.act)!;
    expect(home.ok).toBe(true);
    expect(home.note).toBe(`The ${ftr.name} is back aboard the ${fleet.name}.`);
    expect(home.open).toBe(`group:${ftr.id}`);
    expect(airPage(g, `wings:${fleet.id}`)!.sections.map((s) => s.heading)).toEqual(['THE WINGS']);
  });
});
