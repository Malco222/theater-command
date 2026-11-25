// An island's room in corps (rulings 173(b) and 179, rider (s); the design director, 25 November 2026, form B3: "Islet
// room: yes, built as rulings 173 and 179 say, for the first Pacific release").
//
// Each island of the historian's table carries "its room, in corps", and "the limit of an open beach is the island's
// room of one corps and what the corps carries". Until now the one limit was the five a province holds, and in test
// wars five corps stood on Wake. On the whole-world chart every rule that asks how many corps may stand in, march
// into, be shipped to, be raised at or land on a province now reads one count (islands.ts roomAt): the island's room,
// the corps of that side inside its town counted with those outside it; all other ground keeps its five. The player
// is told the reason in the island's own words. Nobody is moved or struck off to keep the count.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, allied, createGame, declareWar, hasRoom, moveCorps, placed, relocate, removeCorps, resolveBattle, roomForAll, setWar, shoreRoom, startPhase, whyNoRoom, whyNoTrainFor } from '../src/campaign/game';
import { engRoom, islandOf, roomAt, roomRefusal, roomSays } from '../src/campaign/islands';
import { landingTargets, whyNotLand, whyNotShip } from '../src/campaign/navy';
import { followUp } from '../src/campaign/ashore';
import { saveGame } from '../src/campaign/save';
import { garrison, garrisonOf, whyNotGarrison, whyNotTakeField } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, orderLand, orderMove, orderShip, reach, stepAI } from '../src/campaign/turn';
import { STACK, type Corps, type Game, type Region } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => any;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;

/** A whole-world war with a player, at his first month. */
function war(human: string): Game {
  const g = startOpen(createGame(3, human));
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  beginHuman(g);
  return g;
}
/** A fresh corps of infantry put down for the test, with its month in hand. */
function corps(g: Game, nat: string, name: string, at: Region): Corps {
  const c = addCorps(g, nat, name, 'inf', 1, at.id, {});
  c.ap = c.apMax;
  return c;
}
const at = (g: Game, name: string): Region => g.byName[name];

describe("an island's room, by the historian's table", () => {
  it('is one corps on an atoll, four on Okinawa, and five on ground that is no island of the table', () => {
    const g = startOpen(createGame(3, null));
    for (const name of ['Wake', 'Midway', 'Tarawa', 'Iwo Jima', 'Kwajalein']) expect(roomAt(g, at(g, name), at(g, name).ctrl), name).toBe(1);
    for (const name of ['Truk', 'Saipan', 'Guam', 'Palau']) expect(roomAt(g, at(g, name), at(g, name).ctrl), name).toBe(2);
    expect(roomAt(g, at(g, 'Guadalcanal'), 'UK')).toBe(3);
    expect(roomAt(g, at(g, 'Okinawa'), 'JAP')).toBe(4);
    expect(roomAt(g, at(g, 'Rabaul'), 'AUS')).toBe(4);
    for (const name of ['Tokyo', 'Manila', 'Honolulu', 'Paris', 'London']) expect(roomAt(g, at(g, name), at(g, name).ctrl), name).toBe(STACK);
    expect(roomSays(g, at(g, 'Wake'))).toBe('Wake has room for one corps.');
    expect(roomSays(g, at(g, 'Okinawa'))).toBe('Okinawa has room for 4 corps.');
    expect(roomSays(g, at(g, 'Tokyo'))).toBeNull();
    // A landing counts an island's room and no other ground's.
    expect(shoreRoom(g, at(g, 'Wake'), 'JAP')).toBe(1);
    expect(shoreRoom(g, at(g, 'Manila'), 'JAP')).toBe(Infinity);
  }, 300000);
});

describe('the player and Wake', () => {
  it('cannot ship a second corps to Wake, and is told why', () => {
    const g = war('USA');
    const wake = at(g, 'Wake'), honolulu = at(g, 'Honolulu');
    const first = corps(g, 'USA', 'First', honolulu), second = corps(g, 'USA', 'Second', honolulu);
    expect(whyNotShip(g, first, wake)).toBeNull();
    expect(orderShip(g, first, wake.id)).toBe(true);
    expect(first.reg).toBe(wake.id);
    expect(whyNotShip(g, second, wake)).toBe('Wake is full: it has room for one corps and yours is there.');
    expect(orderShip(g, second, wake.id)).toBe(false);
    expect(second.reg).toBe(honolulu.id);
    // The 3D game asks the same question through its doorway, and shows the island's own sentence.
    TC().load(saveGame(g));
    expect(TC().bySeaWhy([second.id], wake.id)).toEqual({ why: 'Wake is full: it has room for one corps and yours is there.' });
    expect((TC().seaReach([second.id]) as { ports: number[] }).ports).not.toContain(wake.id);
    expect(TC().ship([second.id], wake.id).moved).toEqual([]);
    expect(TC().province(wake.id).island).toEqual({ room: 1, line: 'Wake has room for one corps.' });
    expect(TC().province(honolulu.id).island).toBeNull();
  }, 300000);

  it('cannot land two corps on Wake: a landing force for an atoll is one corps', () => {
    const g = war('JAP');
    const wake = at(g, 'Wake'), quay = at(g, 'Kwajalein');
    // (Two stand on the quay for the test, put there by hand: Kwajalein is an atoll too, and nobody is struck off it.)
    const a = corps(g, 'JAP', 'First', quay), b = corps(g, 'JAP', 'Second', quay);
    expect(placed(g, quay, 'JAP')).toHaveLength(2);
    declareWar(g, 'JAP', 'USA');
    expect(whyNotLand(g, [a, b], wake)).toBe('Wake has room for one corps: only one can go.');
    expect(orderLand(g, [a, b], wake.id)).toMatchObject({ ok: false, why: 'Wake has room for one corps: only one can go.' });
    expect(landingTargets(g, [a, b]).map((r) => r.name)).not.toContain('Wake');
    // One corps is the force, and it goes ashore.
    expect(whyNotLand(g, [a], wake)).toBeNull();
    expect(landingTargets(g, [a]).map((r) => r.name)).toContain('Wake');
    TC().load(saveGame(g));
    expect(TC().bySeaWhy([a.id, b.id], wake.id)).toEqual({ why: 'Wake has room for one corps: only one can go.' });
    expect(TC().bySeaWhy([a.id], wake.id)).toEqual({ why: '' });
    const res = orderLand(g, [a], wake.id);
    expect(res).toMatchObject({ ok: true, taken: true });
    expect([wake.ctrl, a.reg]).toEqual(['JAP', wake.id]);
    // The second cannot follow it: not as a second landing, not by ship to the island now his own.
    b.ap = b.apMax;
    expect(whyNotLand(g, [b], wake)).toBe('Wake is not enemy ground.');
    expect(whyNotShip(g, b, wake)).toBe('Wake is full: it has room for one corps and yours is there.');
    expect(orderShip(g, b, wake.id)).toBe(false);
    // Nor are more ships counted on to follow a wave than the island has room for.
    expect(followUp(g, 'JAP', quay, [], wake)).toBe(0);
  }, 300000);

  it("the corps inside an island's town stands on the island too: nobody more of its side comes, and the enemy counts his own", () => {
    const g = war('JAP');
    const wake = at(g, 'Wake'), quay = at(g, 'Kwajalein');
    const a = corps(g, 'JAP', 'First', quay), b = corps(g, 'JAP', 'Second', quay);
    declareWar(g, 'JAP', 'USA');
    expect(orderLand(g, [a], wake.id).taken).toBe(true);
    expect(whyNotGarrison(g, a)).toBeNull();
    expect(garrison(g, a)).toBe(true);
    expect(garrisonOf(wake)).toEqual([a]);
    expect(placed(g, wake, 'JAP')).toHaveLength(0);
    expect(roomAt(g, wake, 'JAP')).toBe(0);
    b.ap = b.apMax;
    // (The corps in the town is counted as there: the island is full.)
    expect(whyNotShip(g, b, wake)).toBe('Wake is full: it has room for one corps and yours is there.');
    // It may come out again: it asks no new place. And the enemy who would land counts only his own.
    expect(whyNotTakeField(g, a)).toBeNull();
    expect(roomAt(g, wake, 'USA')).toBe(1);
    expect(shoreRoom(g, wake, 'USA')).toBe(1);
    // (Each corps in a town is asked for itself. Were a town's list ever of two sides, each side would count its own
    // corps there and not the other's, whichever stood first on the list.)
    const us = g.corps.find((c) => c.nat === 'USA' && !c.dead && c.kind === 'inf')!;
    const truk = at(g, 'Truk');
    expect(roomAt(g, { ...truk, garr: [a, us] }, 'JAP')).toBe(1);
    expect(roomAt(g, { ...truk, garr: [a, us] }, 'USA')).toBe(1);
    expect(roomAt(g, { ...truk, garr: [us, a, b] }, 'JAP')).toBe(0);
  }, 300000);

  it('engineers are counted apart on an island by ship as by march and by landing: the three counts agree', () => {
    const g = war('USA');
    const wake = at(g, 'Wake'), honolulu = at(g, 'Honolulu');
    expect(placed(g, wake, 'USA')).toHaveLength(0);
    const sappers = addCorps(g, 'USA', 'Sappers', 'eng', 1, honolulu.id, {}), bridgers = addCorps(g, 'USA', 'Bridgers', 'eng', 1, honolulu.id, {});
    for (const e of [sappers, bridgers]) e.ap = e.apMax;
    const first = corps(g, 'USA', 'First', honolulu), second = corps(g, 'USA', 'Second', honolulu);
    // One fighting corps and one engineer corps are what an atoll takes; two engineer corps beside it are one too many.
    expect(roomForAll(g, wake, [first, sappers])).toBe(true);
    expect(roomForAll(g, wake, [first, sappers, bridgers])).toBe(false);
    expect(roomRefusal(g, wake, [first, sappers, bridgers])).toBe('Wake has room for one engineer corps beside its troops: only one can go.');
    // Engineers alone hold the atoll: they work among the troops and take none of its room of one corps.
    expect(whyNotShip(g, sappers, wake)).toBeNull();
    relocate(g, sappers, wake);
    expect(hasRoom(g, wake, first)).toBe(true);
    expect(roomForAll(g, wake, [first])).toBe(true);
    expect(shoreRoom(g, wake, 'USA')).toBe(1);
    expect(whyNotShip(g, first, wake)).toBeNull();
    relocate(g, first, wake);
    // The one corps is there: a second is refused by ship as it would be by march or by landing, and told why.
    expect(hasRoom(g, wake, second)).toBe(false);
    expect(roomForAll(g, wake, [second])).toBe(false);
    expect(whyNotShip(g, second, wake)).toBe('Wake is full: it has room for one corps and yours is there.');
    // A second engineer corps does not join the first on an atoll: engineers are counted apart up to the lesser of two
    // and the island's room, and Wake's room is one (the design director, 13:58Z 25 November 2026).
    expect(engRoom(g, wake)).toBe(1);
    expect(hasRoom(g, wake, bridgers)).toBe(false);
    expect(roomForAll(g, wake, [bridgers])).toBe(false);
    expect(whyNotShip(g, bridgers, wake)).toBe('Wake is full: it has room for one engineer corps beside its troops and yours is there.');
    // An island with room for two or more takes two, as any ground does.
    const okinawa = at(g, 'Okinawa');
    expect([engRoom(g, okinawa), engRoom(g, at(g, 'Singapore')), engRoom(g, honolulu)]).toEqual([2, 2, 2]);
    // (A harbour that is no island of the table counts every corps on its quay, as it always did.)
    const osaka = at(g, 'Osaka');
    expect(islandOf(g, osaka)).toBeNull();
    expect(engRoom(g, osaka)).toBe(2);
  }, 300000);

  it('cannot march a third corps onto Singapore, and is told why; the two may still change places', () => {
    const g = war('UK');
    const singapore = at(g, 'Singapore'), johore = at(g, 'Johore');
    expect(singapore.nb).toEqual([johore.id]);
    expect(placed(g, singapore, 'UK')).toHaveLength(2);
    const c = corps(g, 'UK', 'Third', johore);
    expect(hasRoom(g, singapore, c)).toBe(false);
    expect(reach(g, c).has(singapore.id)).toBe(false);
    expect(whyNoRoom(g, c, singapore)).toBe('Singapore is full: it has room for 2 corps and 2 of yours are there.');
    expect(orderMove(g, c, singapore.id)).toBe(0);
    expect(moveCorps(g, c, singapore)).toBe(false);
    // (Nor by rail, with the island's own sentence.)
    expect(whyNoTrainFor(g, [c], singapore.id)).toMatchObject({ code: 'room', line: 'Singapore is full: it has room for 2 corps and 2 of yours are there.', at: singapore.id });
    // Ground that is no island says nothing of room here.
    expect(whyNoRoom(g, c, at(g, 'Kuala Lumpur'))).toBeNull();
    // The 3D game's march: the island's sentence, and the offer to change places with the corps that stands there.
    TC().load(saveGame(g));
    const said = TC().move([c.id], singapore.id) as { moved: unknown[]; note: string };
    expect(said.moved).toEqual([]);
    expect(said.note).toMatch(/^Singapore has room for 2 corps\. Order the march again and Third changes places with Malaya Command/);
    expect(TC().province(singapore.id).island).toEqual({ room: 2, line: 'Singapore has room for 2 corps.' });
    const swapped = TC().move([c.id], singapore.id) as { moved: { id: number }[]; note: string };
    expect(swapped.moved).toHaveLength(2);
    expect(swapped.note).toMatch(/have changed places\.$/);
  }, 300000);
});

describe('Okinawa takes four, and other ground five', () => {
  it('four corps are shipped to Okinawa and the fifth is told the island is full', () => {
    const g = war('JAP');
    const okinawa = at(g, 'Okinawa'), tokyo = at(g, 'Tokyo');
    expect(placed(g, okinawa, 'JAP')).toHaveLength(0);
    const cs = [1, 2, 3, 4, 5].map((k) => corps(g, 'JAP', `Corps ${k}`, tokyo));
    for (const c of cs.slice(0, 4)) {
      expect(whyNotShip(g, c, okinawa), c.name).toBeNull();
      relocate(g, c, okinawa);
    }
    expect(placed(g, okinawa, 'JAP')).toHaveLength(4);
    expect(whyNotShip(g, cs[4], okinawa)).toBe('Okinawa is full: it has room for 4 corps and 4 of yours are there.');
    expect(hasRoom(g, okinawa, cs[4])).toBe(false);
    expect(roomForAll(g, okinawa, [cs[4]])).toBe(false);
    // A refusal says how many more can go: the number the order must be changed to (the UI director, 14:00Z 25 November 2026).
    relocate(g, cs[3], tokyo);
    const sixth = corps(g, 'JAP', 'Corps 6', tokyo);
    expect(roomRefusal(g, okinawa, [cs[3]])).toBeNull();
    expect(roomRefusal(g, okinawa, [cs[3], cs[4]])).toBe('Okinawa has room for 4 corps and 3 of yours are there: only one more can go.');
    for (const c of cs.slice(0, 3)) relocate(g, c, tokyo);
    expect(roomRefusal(g, okinawa, [...cs, sixth])).toBe('Okinawa has room for 4 corps: only 4 can go.');
    relocate(g, cs[0], okinawa);
    expect(roomRefusal(g, okinawa, cs.slice(1))).toBe('Okinawa has room for 4 corps and one of yours is there: only 3 more can go.');
    // Where an ally's corps fill the room too, "of yours" is dropped.
    const ally = g.corps.find((c) => !c.dead && c.nat !== 'JAP' && c.kind === 'inf' && allied(g, c.nat, 'JAP'))!;
    expect(ally).toBeDefined();
    relocate(g, ally, okinawa);
    expect(roomRefusal(g, okinawa, cs.slice(1))).toBe('Okinawa has room for 4 corps and 2 are there: only 2 more can go.');
  }, 300000);

  it('a province that is no island of the table still takes five, and says what it always said', () => {
    const g = war('JAP');
    const osaka = at(g, 'Osaka'), tokyo = at(g, 'Tokyo');
    expect(osaka.port).toBe(true);
    expect(islandOf(g, osaka)).toBeNull();
    for (const u of [...osaka.units]) removeCorps(g, u);
    const cs = [1, 2, 3, 4, 5, 6].map((k) => corps(g, 'JAP', `Corps ${k}`, tokyo));
    for (const c of cs.slice(0, 5)) {
      expect(hasRoom(g, osaka, c), c.name).toBe(true);
      expect(whyNotShip(g, c, osaka), c.name).toBeNull();
      relocate(g, c, osaka);
    }
    expect(hasRoom(g, osaka, cs[5])).toBe(false);
    expect(whyNotShip(g, cs[5], osaka)).toBe('Osaka has no room for another corps.');
  }, 300000);
});

describe('the winners of a fight for an island', () => {
  it('move in as many as it has room for, and the rest stay where they came from', () => {
    const g = startOpen(createGame(3, null));
    const singapore = at(g, 'Singapore'), johore = at(g, 'Johore');
    setWar(g, 'JAP', 'UK');
    for (const u of [...johore.units]) removeCorps(g, u);
    johore.ctrl = 'JAP';
    const force = [1, 2, 3, 4].map((k) => { const c = addCorps(g, 'JAP', `Army ${k}`, 'inf', 3, johore.id, {}); c.ap = c.apMax; c.xp = 0.8; return c; });
    const report = resolveBattle(g, force, singapore);
    expect(report.taken).toBe(true);
    expect(placed(g, singapore, 'JAP').map((c) => c.name)).toEqual(['Army 1', 'Army 2']);
    expect(placed(g, johore, 'JAP').map((c) => c.name)).toEqual(['Army 3', 'Army 4']);
    // Nobody was struck off for want of room.
    expect(force.every((c) => !c.dead)).toBe(true);
  }, 300000);
});

// The record's own hand, as built. Form B3, (c), says both that rows which put more corps on an island than its room
// are left and listed for the historian, and that a row arriving on a full island follows what today's code does for
// a full province. As built, the record's place is counted against five as it always was, so its rows are put down
// over an island's room and nobody else may join them. The other reading (the row sent next door or kept waiting at
// the island's own room) was built and measured and is not in the rules: it is the design director's to rule, and
// these tests say what the rules do until he has.
describe("the record's rows and an island's room, as built", () => {
  it('a row the record puts on an island is put down there as before, over its room, and nobody else may join it', () => {
    const g = startOpen(createGame(3, 'USA'));
    // Pearl Harbor has room for two, and two stand there on the first day. The 298th Infantry's row (October 1940)
    // is the record's: it forms where the record has it, as it did before the table, and is not sent next door.
    const pearl = at(g, 'Pearl Harbor'), honolulu = at(g, 'Honolulu');
    expect(placed(g, pearl, 'USA')).toHaveLength(2);
    g.turn = 14;
    startPhase(g, 'USA');
    const guard = g.corps.find((c) => c.nat === 'USA' && c.name === '298th Infantry (Hawaii National Guard)')!;
    expect(g.regions[guard.reg].name).toBe('Pearl Harbor');
    expect(placed(g, pearl, 'USA')).toHaveLength(3);
    expect(placed(g, pearl, 'USA').length).toBeGreaterThan(roomAt(g, pearl, 'USA'));
    // The island is over its room by the record's hand alone: the player's own corps is still refused, and told why.
    const more = corps(g, 'USA', 'Test Corps', honolulu);
    expect(hasRoom(g, pearl, more)).toBe(false);
    expect(whyNoRoom(g, more, pearl)).toBe('Pearl Harbor is full: it has room for 2 corps and 3 of yours are there.');
    expect(shoreRoom(g, pearl, 'USA')).toBe(0);
  }, 300000);

  it('a garrison and a landing force the record stands on one atoll both come: no row waits or is struck for the table', () => {
    const g = startOpen(createGame(3, null));
    // Kwajalein has room for one. In November 1941 the record has the 61st Guard Unit hold it and the Maizuru landing
    // force wait there for Wake: both rows come in their month (the historian mends the one or the other).
    const kwajalein = at(g, 'Kwajalein');
    g.turn = 27;
    startPhase(g, 'JAP');
    const names = [...placed(g, kwajalein, 'JAP'), ...(kwajalein.garr ?? [])].map((c) => c.name);
    expect(names).toContain('61st Guard Unit (Kwajalein and Roi)');
    expect(names).toContain('Naval landing force (Maizuru 2nd, for Wake)');
  }, 300000);

  it('rows the record puts on an island over its room stay: nobody is moved or struck off to keep the count', () => {
    const g = startOpen(createGame(3, null));
    // Samoa has room for one. The 7th Defense Battalion (March 1941) and the 2nd Marine Brigade (January 1942) both
    // come to it, and are still there the month after: no rule clears an island.
    const samoa = at(g, 'Samoa');
    g.turn = 19; startPhase(g, 'USA');
    g.turn = 29; startPhase(g, 'USA');
    expect(placed(g, samoa, 'USA').map((c) => c.name).sort()).toEqual(['2nd Marine Brigade', '7th Defense Battalion']);
    expect(placed(g, samoa, 'USA').length).toBeGreaterThan(roomAt(g, samoa, 'USA'));
    g.turn = 30; startPhase(g, 'USA');
    expect(placed(g, samoa, 'USA')).toHaveLength(2);
  }, 300000);

  it('a row made from the corps that holds an island takes its place there, and is not sent away for want of room', () => {
    const g = startOpen(createGame(3, null));
    // Hong Kong has room for one, and China Command holds it. Its row of August 1941 is made from that corps,
    // which is struck off as the row forms: the stronger command stands where the weaker stood, on the island.
    const hk = at(g, 'Hong Kong');
    const was = placed(g, hk, 'UK');
    expect(was.map((c) => [c.name, c.str])).toEqual([['China Command (Hong Kong garrison)', 0.35]]);
    for (let t = 2; t <= 24; t++) { g.turn = t; startPhase(g, 'UK'); }
    const now = placed(g, hk, 'UK');
    expect(now.map((c) => c.name)).toEqual(['China Command (Hong Kong garrison)']);
    expect(now[0].id).not.toBe(was[0].id);
    expect(now[0].str).toBeCloseTo(0.7, 5);
    expect(g.corps.filter((c) => c.nat === 'UK' && !c.dead && c.name === 'China Command (Hong Kong garrison)')).toHaveLength(1);
  }, 300000);
});
