// An island's room counted in men (the design director, 25 November 2026, 14:28Z: "(a) Men: yes. An island's room is
// its table figure in divisions' worth of men; a side's corps fit while their men together are within it"; "(b) A
// corps larger than the whole room does not fit: yes, for every seat"), over form B3 and rulings 173(b) and 179(s).
//
// Each island of the historian's table carries its room, now read as divisions' worth of men (15,000 a division, the
// game's own), and a corps' men are its card's own figure (game.ts `men`). Every rule that asks whether corps may
// stand in, march into, be shipped to, be raised at or land on a province asks one body (islands.ts `fits`, with
// `roomLeft` and `fitWave` over it); all other ground keeps its five corps. The player is told the reason in the UI
// director's words of 15:02Z, word for word. Nobody is moved or struck off to keep the room.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, allied, createGame, declareWar, hasRoom, men, moveCorps, placed, relocate, removeCorps, resolveBattle, roomForAll, setWar, shoreRoom, startPhase, swapCorps, whyNoRoom, whyNoSwap, whyNoTrainFor } from '../src/campaign/game';
import { engRoom, fits, fitWave, islandOf, roomLeft, roomMen, roomRefusal, roomSays } from '../src/campaign/islands';
import { menOf, setBreakUp } from '../src/campaign/breakup';
import { landingTargets, whyNotLand, whyNotShip } from '../src/campaign/navy';
import { followUp } from '../src/campaign/ashore';
import { saveGame } from '../src/campaign/save';
import { garrison, garrisonOf, whyNotGarrison, whyNotTakeField } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, orderLand, orderMove, orderShip, reach, stepAI } from '../src/campaign/turn';
import { STACK, type Corps, type Game, type Region } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

/** Every source file of the rules and the bridge, as text, by its path (vite's own glob, as the test runner gives it). */
type Glob = (pattern: string, options: { eager: true; as: 'raw' }) => Record<string, string>;
const SOURCES = {
  ...((import.meta as unknown as { glob: Glob }).glob('../src/**/*.ts', { eager: true, as: 'raw' })),
  ...((import.meta as unknown as { glob: Glob }).glob('../unreal/TheaterCommand/Rules/*.ts', { eager: true, as: 'raw' })),
};

type Call = (...a: unknown[]) => any;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;

/** A whole-world war with a player, at his first month. */
function war(human: string): Game {
  const g = startOpen(createGame(3, human));
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  beginHuman(g);
  return g;
}
/** A corps of infantry put down for the test, with its month in hand: one division at full strength unless told. */
function corps(g: Game, nat: string, name: string, at: Region, divs = 1, str = 1): Corps {
  const c = addCorps(g, nat, name, 'inf', divs, at.id, {});
  c.str = str;
  c.ap = c.apMax;
  return c;
}
const at = (g: Game, name: string): Region => g.byName[name];
/** Everybody of every side off an island, field and town, for a test that fills it by hand. */
const clear = (g: Game, r: Region): void => { for (const u of [...r.units, ...(r.garr ?? [])]) removeCorps(g, u); };

describe("an island's room, by the historian's table, in men", () => {
  it("is a division's worth on an atoll, four on Okinawa, and five corps on ground that is no island of the table", () => {
    const g = startOpen(createGame(3, null));
    for (const name of ['Wake', 'Midway', 'Tarawa', 'Iwo Jima', 'Kwajalein']) expect(roomLeft(g, at(g, name), 'JAP'), name).toBe(1);
    for (const name of ['Truk', 'Saipan', 'Palau']) expect(roomLeft(g, at(g, name), 'JAP'), name).toBe(2);
    expect(roomLeft(g, at(g, 'Guadalcanal'), 'JAP')).toBe(3);
    expect(roomLeft(g, at(g, 'Okinawa'), 'JAP')).toBe(4);
    for (const name of ['Tokyo', 'Manila', 'Honolulu', 'Paris', 'London']) expect(roomLeft(g, at(g, name), at(g, name).ctrl, 0), name).toBe(STACK);
    // The room left is read in divisions' worth, not rounded: Guam's 750 men take 0.05 of its two.
    expect(roomLeft(g, at(g, 'Guam'), 'USA')).toBeCloseTo(1.95, 9);
    // The swap card's head (the UI director, 15:02Z, section 2).
    expect(roomSays(g, at(g, 'Wake'))).toBe('Wake has room for one division, about 15,000 men.');
    expect(roomSays(g, at(g, 'Okinawa'))).toBe('Okinawa has room for 4 divisions, about 60,000 men.');
    expect(roomSays(g, at(g, 'Tokyo'))).toBeNull();
    // A landing counts an island's room in men and no other ground's: two half-strength corps go ashore on an atoll
    // where one full one does, and a corps of two full divisions on none.
    const tokyo = at(g, 'Tokyo');
    const full = corps(g, 'JAP', 'Full', tokyo), half = corps(g, 'JAP', 'Half', tokyo, 1, 0.5), halfToo = corps(g, 'JAP', 'Half too', tokyo, 1, 0.5), big = corps(g, 'JAP', 'Big', tokyo, 2);
    expect(shoreRoom(g, at(g, 'Wake'), 'JAP', [full, half])).toBe(1);
    expect(shoreRoom(g, at(g, 'Wake'), 'JAP', [half, halfToo])).toBe(2);
    expect(shoreRoom(g, at(g, 'Wake'), 'JAP', [big])).toBe(0);
    // A corps too large for what is left is passed over, and the next one asked, in the plan's order.
    expect(fitWave(g, at(g, 'Wake'), 'JAP', [big, half, full, halfToo]).map((c) => c.name)).toEqual(['Half', 'Half too']);
    expect(shoreRoom(g, at(g, 'Manila'), 'JAP', [big, full])).toBe(Infinity);
    expect(fitWave(g, at(g, 'Manila'), 'JAP', [big, full])).toEqual([big, full]);
  }, 300000);

  it("counts a corps' men by its card's own measure, one measure for every nation (the design director, 21:37Z, 26 November 2026, 7)", () => {
    const g = startOpen(createGame(3, null));
    const c = corps(g, 'CHI', 'Test Army', at(g, 'Chungking'), 2, 0.6);
    // (China's corps cards read 15,000 a division as every nation's do: the design director, 21:37Z, 26 November 2026, 7.)
    expect(men(c)).toBe(18000);
    expect(menOf(c)).toBe(men(c));
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
    const said = 'Wake is full: it has room for about 15,000 men and 15,000 of yours are there.';
    expect(whyNotShip(g, second, wake)).toBe(said);
    expect(orderShip(g, second, wake.id)).toBe(false);
    expect(second.reg).toBe(honolulu.id);
    // The 3D game asks the same question through its doorway, and shows the island's own sentence.
    TC().load(saveGame(g));
    expect(TC().bySeaWhy([second.id], wake.id)).toEqual({ why: said });
    expect((TC().seaReach([second.id]) as { ports: number[] }).ports).not.toContain(wake.id);
    expect(TC().ship([second.id], wake.id).moved).toEqual([]);
    expect(TC().province(wake.id).island).toEqual({ room: 1, line: 'Wake has room for one division, about 15,000 men.' });
    expect(TC().province(honolulu.id).island).toBeNull();
  }, 300000);

  it('cannot land two full corps on Wake, and lands two half-strength ones where it landed one', () => {
    const g = war('JAP');
    const wake = at(g, 'Wake'), quay = at(g, 'Kwajalein');
    // (Two stand on the quay for the test, put there by hand: Kwajalein is an atoll too, and nobody is struck off it.)
    const a = corps(g, 'JAP', 'First', quay), b = corps(g, 'JAP', 'Second', quay);
    declareWar(g, 'JAP', 'USA');
    const said = 'Wake has room for one division, about 15,000 men: you are sending 30,000.';
    expect(whyNotLand(g, [a, b], wake)).toBe(said);
    expect(orderLand(g, [a, b], wake.id)).toMatchObject({ ok: false, why: said });
    expect(landingTargets(g, [a, b]).map((r) => r.name)).not.toContain('Wake');
    TC().load(saveGame(g));
    expect(TC().bySeaWhy([a.id, b.id], wake.id)).toEqual({ why: said });
    expect(TC().bySeaWhy([a.id], wake.id)).toEqual({ why: '' });
    // Two at half strength are 15,000 men together, and both go.
    a.str = 0.5; b.str = 0.5;
    expect(whyNotLand(g, [a, b], wake)).toBeNull();
    expect(landingTargets(g, [a, b]).map((r) => r.name)).toContain('Wake');
    a.str = 1; b.str = 1;
    const res = orderLand(g, [a], wake.id);
    expect(res).toMatchObject({ ok: true, taken: true });
    expect([wake.ctrl, a.reg]).toEqual(['JAP', wake.id]);
    // The second cannot follow it by ship to the island now his own.
    b.ap = b.apMax;
    expect(whyNotShip(g, b, wake)).toBe('Wake is full: it has room for about 15,000 men and 15,000 of yours are there.');
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
    expect(roomLeft(g, wake, 'JAP')).toBe(0);
    b.ap = b.apMax;
    // (The corps in the town is counted as there: the island is full.)
    expect(whyNotShip(g, b, wake)).toBe('Wake is full: it has room for about 15,000 men and 15,000 of yours are there.');
    // It may come out again: it asks no new place. And the enemy who would land counts only his own.
    expect(whyNotTakeField(g, a)).toBeNull();
    expect(roomLeft(g, wake, 'USA')).toBe(1);
    const us = g.corps.find((c) => c.nat === 'USA' && !c.dead && c.kind === 'inf')!;
    expect(shoreRoom(g, wake, 'USA', [us])).toBe(men(us) <= 15000 ? 1 : 0);
    // (Each corps in a town is asked for itself: each side counts its own men there and not the other's.)
    const truk = { ...at(g, 'Truk'), units: [] as Corps[] };
    expect(roomLeft(g, { ...truk, garr: [a, us] }, 'JAP')).toBe(1);
    expect(roomLeft(g, { ...truk, garr: [a, us] }, 'USA')).toBeCloseTo(2 - men(us) / 15000, 9);
    expect(roomLeft(g, { ...truk, garr: [us, a, b] }, 'JAP')).toBe(0);
  }, 300000);

  it('engineers are counted apart and in corps on an island, by ship as by march and by landing', () => {
    const g = war('USA');
    const wake = at(g, 'Wake'), honolulu = at(g, 'Honolulu');
    expect(placed(g, wake, 'USA')).toHaveLength(0);
    const sappers = addCorps(g, 'USA', 'Sappers', 'eng', 1, honolulu.id, {}), bridgers = addCorps(g, 'USA', 'Bridgers', 'eng', 1, honolulu.id, {});
    for (const e of [sappers, bridgers]) e.ap = e.apMax;
    const first = corps(g, 'USA', 'First', honolulu), second = corps(g, 'USA', 'Second', honolulu);
    // One division's worth of fighting men and one engineer corps are what an atoll takes; two engineer corps beside it are one too many.
    expect(roomForAll(g, wake, [first, sappers])).toBe(true);
    expect(roomForAll(g, wake, [first, sappers, bridgers])).toBe(false);
    expect(roomRefusal(g, wake, [first, sappers, bridgers])).toBe('Wake has room for one engineer corps as well as its troops: only one can go.');
    // Engineers alone hold the atoll: they work among the troops and take none of its room in men.
    expect(whyNotShip(g, sappers, wake)).toBeNull();
    relocate(g, sappers, wake);
    expect(hasRoom(g, wake, first)).toBe(true);
    expect(roomForAll(g, wake, [first])).toBe(true);
    expect(shoreRoom(g, wake, 'USA', [first])).toBe(1);
    expect(whyNotShip(g, first, wake)).toBeNull();
    relocate(g, first, wake);
    // The division's worth is there: a second corps is refused by ship as it would be by march or by landing.
    expect(hasRoom(g, wake, second)).toBe(false);
    expect(roomForAll(g, wake, [second])).toBe(false);
    expect(whyNotShip(g, second, wake)).toBe('Wake is full: it has room for about 15,000 men and 15,000 of yours are there.');
    // A second engineer corps does not join the first on an atoll (the design director, 13:58Z 25 November 2026),
    // and the island is not called full for it: it may still take troops (the UI director, 15:02Z, section 4).
    expect(engRoom(g, wake)).toBe(1);
    expect(hasRoom(g, wake, bridgers)).toBe(false);
    expect(roomForAll(g, wake, [bridgers])).toBe(false);
    expect(whyNotShip(g, bridgers, wake)).toBe('Wake can take no more engineers: it has room for one engineer corps as well as its troops, and yours is there.');
    // An island with room for two or more takes two, as any ground does, and says so in corps.
    const okinawa = at(g, 'Okinawa'), tokyo = at(g, 'Tokyo');
    expect([engRoom(g, okinawa), engRoom(g, at(g, 'Singapore')), engRoom(g, honolulu)]).toEqual([2, 2, 2]);
    clear(g, okinawa);
    const e = [1, 2, 3].map((k) => { const x = addCorps(g, 'USA', `Engineers ${k}`, 'eng', 1, tokyo.id, {}); x.ap = x.apMax; return x; });
    relocate(g, e[0], okinawa);
    expect(roomRefusal(g, okinawa, [e[1], e[2]])).toBe('Okinawa has room for 2 engineer corps as well as its troops and one of yours is there: only one more can go.');
    relocate(g, e[1], okinawa);
    expect(roomRefusal(g, okinawa, [e[2]])).toBe('Okinawa can take no more engineers: it has room for 2 engineer corps as well as its troops, and 2 of yours are there.');
    // (A harbour that is no island of the table counts its engineers as it always did.)
    const osaka = at(g, 'Osaka');
    expect(islandOf(g, osaka)).toBeNull();
    expect(engRoom(g, osaka)).toBe(2);
  }, 300000);

  it("marches a corps onto Singapore while its side's men there and his are within its room, refuses one they are not, and says why; the two may still change places", () => {
    const g = war('UK');
    const singapore = at(g, 'Singapore'), johore = at(g, 'Johore');
    expect(singapore.nb).toEqual([johore.id]);
    // (Read off the board: Malaya Command's men, drafted to 7,050 by the player's month. The coast guns' 15,000 are a
    // fortress crew and take no room, whatever the fortress rows say: the design director, 00:20Z, 27 November 2026, D4.)
    const all = [...placed(g, singapore, 'UK'), ...(singapore.garr ?? [])].filter((u) => u.kind !== 'eng');
    expect(all.filter((u) => u.kind === 'fort').reduce((n, u) => n + men(u), 0)).toBe(15000);
    const there = all.filter((u) => u.kind !== 'fort').reduce((n, u) => n + men(u), 0);
    expect(there).toBe(7050);
    // Singapore's room is 5 (the design director's stand-in G of 14:28Z): a third corps, which its room of 2 corps
    // refused, now goes.
    const third = corps(g, 'UK', 'Third', johore);
    expect(hasRoom(g, singapore, third)).toBe(true);
    expect(whyNoRoom(g, third, singapore)).toBeNull();
    removeCorps(g, third);
    const c = corps(g, 'UK', 'Army', johore, 5);
    expect(hasRoom(g, singapore, c)).toBe(false);
    expect(reach(g, c).has(singapore.id)).toBe(false);
    // (67,950 more fit: "more can go" is rounded down and the 7,050 there up, so the two add up to the room the line shows.)
    const said = 'Singapore has room for 5 divisions, about 75,000 men, and 8,000 of yours are there: only 67,000 more can go, and you are sending 75,000.';
    expect(whyNoRoom(g, c, singapore)).toBe(said);
    expect(orderMove(g, c, singapore.id)).toBe(0);
    expect(moveCorps(g, c, singapore)).toBe(false);
    // (Nor by rail, with the island's own sentence.)
    expect(whyNoTrainFor(g, [c], singapore.id)).toMatchObject({ code: 'room', line: said, at: singapore.id });
    // A worn army of 67,500 has the room a fresh one has not.
    c.str = 0.9;
    expect(hasRoom(g, singapore, c)).toBe(true);
    c.str = 1;
    // Ground that is no island says nothing of room here.
    expect(whyNoRoom(g, c, at(g, 'Kuala Lumpur'))).toBeNull();
    // The 3D game's march: the island's sentence, and the offer to change places with the corps that stands there: the
    // men there after the swap (75,000) are within the room (the design director, 21:37Z, 26 November 2026, 5).
    TC().load(saveGame(g));
    const tried = TC().move([c.id], singapore.id) as { moved: unknown[]; note: string };
    expect(tried.moved).toEqual([]);
    expect(tried.note).toMatch(/^Singapore has room for 5 divisions, about 75,000 men\. Order the march again and Army changes places with Malaya Command/);
    expect(TC().province(singapore.id).island).toEqual({ room: 5, line: 'Singapore has room for 5 divisions, about 75,000 men.' });
    const swapped = TC().move([c.id], singapore.id) as { moved: { id: number }[]; note: string };
    expect(swapped.moved).toHaveLength(2);
    expect(swapped.note).toMatch(/have changed places\.$/);
  }, 300000);
});

describe('Okinawa takes four divisions, and other ground five corps', () => {
  it("four fresh corps are shipped to Okinawa and the fifth is told the island is full; each refusal says the room, what is there, how much more fits and what he sends", () => {
    const g = war('JAP');
    const okinawa = at(g, 'Okinawa'), tokyo = at(g, 'Tokyo');
    clear(g, okinawa);
    const cs = [1, 2, 3, 4, 5].map((k) => corps(g, 'JAP', `Corps ${k}`, tokyo));
    for (const c of cs.slice(0, 4)) {
      expect(whyNotShip(g, c, okinawa), c.name).toBeNull();
      relocate(g, c, okinawa);
    }
    expect(whyNotShip(g, cs[4], okinawa)).toBe('Okinawa is full: it has room for about 60,000 men and 60,000 of yours are there.');
    expect(hasRoom(g, okinawa, cs[4])).toBe(false);
    expect(roomForAll(g, okinawa, [cs[4]])).toBe(false);
    relocate(g, cs[3], tokyo);
    const sixth = corps(g, 'JAP', 'Corps 6', tokyo);
    expect(roomRefusal(g, okinawa, [cs[3]])).toBeNull();
    expect(roomRefusal(g, okinawa, [cs[3], cs[4]])).toBe('Okinawa has room for 4 divisions, about 60,000 men, and 45,000 of yours are there: only 15,000 more can go, and you are sending 30,000.');
    for (const c of cs.slice(0, 3)) relocate(g, c, tokyo);
    expect(roomRefusal(g, okinawa, [...cs.slice(0, 5)])).toBe('Okinawa has room for 4 divisions, about 60,000 men: you are sending 75,000.');
    expect(roomRefusal(g, okinawa, [...cs, sixth])).toBe('Okinawa has room for 4 divisions, about 60,000 men: you are sending 90,000.');
    relocate(g, cs[0], okinawa);
    expect(roomRefusal(g, okinawa, cs.slice(1))).toBe('Okinawa has room for 4 divisions, about 60,000 men, and 15,000 of yours are there: only 45,000 more can go, and you are sending 60,000.');
    // Where an ally's corps stand there too, "of yours" is dropped.
    const ally = g.corps.find((c) => !c.dead && c.nat !== 'JAP' && c.kind === 'inf' && allied(g, c.nat, 'JAP'))!;
    expect(ally).toBeDefined();
    ally.str = 1; ally.divs = 1;
    relocate(g, ally, okinawa);
    expect(roomRefusal(g, okinawa, cs.slice(1))).toBe('Okinawa has room for 4 divisions, about 60,000 men, and 30,000 are there: only 30,000 more can go, and you are sending 60,000.');
  }, 300000);

  it('rounds "more can go" down to the thousand and the men there up, what he sends to the nearest, and says a figure under 1,000 whole', () => {
    const g = war('JAP');
    const okinawa = at(g, 'Okinawa'), tokyo = at(g, 'Tokyo');
    clear(g, okinawa);
    // 45,600 there: 14,400 more fit, so "14,000" (down), and the 45,600 is "46,000" (up): they add up to the room.
    const there = corps(g, 'JAP', 'There', okinawa, 3, 45600 / 45000);
    expect(men(there)).toBe(45600);
    const going = corps(g, 'JAP', 'Going', tokyo);
    expect(roomRefusal(g, okinawa, [going])).toBe('Okinawa has room for 4 divisions, about 60,000 men, and 46,000 of yours are there: only 14,000 more can go, and you are sending 15,000.');
    // Fewer than 1,000 more fit: "full".
    there.str = 59400 / 45000;
    expect(men(there)).toBe(59400);
    expect(roomRefusal(g, okinawa, [going])).toBe('Okinawa is full: it has room for about 60,000 men and 60,000 of yours are there.');
    // Under 1,000 men the figure whole.
    expect([roomMen(750), roomMen(999), roomMen(1499), roomMen(1500), roomMen(14999, 'down'), roomMen(14001, 'up'), roomMen(999, 'up')]).toEqual(['750', '999', '1,000', '2,000', '14,000', '15,000', '999']);
    // What he sends would read at or under what can go once rounded: the line says the shortfall instead.
    there.str = 1;
    const over = corps(g, 'JAP', 'Over', tokyo, 1, 15400 / 15000);
    expect(men(over)).toBe(15400);
    expect(roomRefusal(g, okinawa, [over])).toBe('Okinawa has room for 4 divisions, about 60,000 men, and 45,000 of yours are there: you are sending 400 men more than fit.');
    relocate(g, there, tokyo);
    expect(roomRefusal(g, okinawa, [there, over])).toBe('Okinawa has room for 4 divisions, about 60,000 men: you are sending 400 men more than fit.');
    relocate(g, there, okinawa);
    there.str = 45600 / 45000;
    removeCorps(g, there);
    const guard = corps(g, 'JAP', 'Guard', okinawa, 1, 0.05);
    expect(roomRefusal(g, okinawa, [going, ...[1, 2, 3, 4].map((k) => corps(g, 'JAP', `More ${k}`, tokyo))])).toBe('Okinawa has room for 4 divisions, about 60,000 men, and 750 of yours are there: only 59,250 more can go, and you are sending 75,000.'); // (the UI director, 00:19Z, 27 November 2026, U1: both whole)
    expect(guard.dead).toBeFalsy();
  }, 300000);

  it('a province that is no island of the table still takes five corps, and says what it always said', () => {
    const g = war('JAP');
    const osaka = at(g, 'Osaka'), tokyo = at(g, 'Tokyo');
    expect(osaka.port).toBe(true);
    expect(islandOf(g, osaka)).toBeNull();
    for (const u of [...osaka.units]) removeCorps(g, u);
    // (Of any size: five armies of four divisions each.)
    const cs = [1, 2, 3, 4, 5, 6].map((k) => corps(g, 'JAP', `Corps ${k}`, tokyo, 4));
    for (const c of cs.slice(0, 5)) {
      expect(hasRoom(g, osaka, c), c.name).toBe(true);
      expect(whyNotShip(g, c, osaka), c.name).toBeNull();
      relocate(g, c, osaka);
    }
    expect(hasRoom(g, osaka, cs[5])).toBe(false);
    expect(whyNotShip(g, cs[5], osaka)).toBe('Osaka has no room for another corps.');
  }, 300000);
});

describe('a corps larger than the whole room (b)', () => {
  it('does not fit an island for any seat, even an empty one, and is told so first and alone', () => {
    const g = war('USA');
    const wake = at(g, 'Wake'), honolulu = at(g, 'Honolulu');
    clear(g, wake);
    const big = corps(g, 'USA', 'XIV Corps', honolulu, 3, 32000 / 45000);
    expect(men(big)).toBe(32000);
    expect(fits(g, wake, 'USA', [big])).toBe(false);
    expect(fits(g, wake, 'JAP', [{ ...big, nat: 'JAP' }])).toBe(false);
    // Without the split rules: too many for the island.
    expect(whyNotShip(g, big, wake)).toBe('Wake has room for one division, about 15,000 men. The XIV Corps has 32,000: too many for the island.');
    // Before how full the island is today, the first such corps of a group named (the UI director, 21:37Z, 26 November 2026, 3).
    const small = corps(g, 'USA', 'Small', honolulu);
    relocate(g, corps(g, 'USA', 'There', honolulu, 1, 0.5), wake);
    expect(roomRefusal(g, wake, [small, big])).toBe('Wake has room for one division, about 15,000 men. The XIV Corps has 32,000: too many for the island.');
    // Where the split rules let him split it, he is told to.
    setBreakUp(g, true);
    expect(whyNotShip(g, big, wake)).toBe('Wake has room for one division, about 15,000 men. The XIV Corps has 32,000, too many to go there whole: split it and send a part.');
    // More than one too large: the first is named, and the rest counted, so he is not refused again for the next.
    setBreakUp(g, false);
    const second = corps(g, 'USA', 'XV Corps', honolulu, 2, 0.9), third = corps(g, 'USA', 'XVI Corps', honolulu, 3, 1);
    expect(roomRefusal(g, wake, [small, big, second])).toBe('Wake has room for one division, about 15,000 men. The XIV Corps has 32,000: too many for the island. One more of the corps you are sending is too large for Wake as well.');
    expect(roomRefusal(g, wake, [big, small, second, third])).toBe('Wake has room for one division, about 15,000 men. The XIV Corps has 32,000: too many for the island. Two more of the corps you are sending are too large for Wake as well.');
    // The corps' figure is its card's own, the exact men.
    big.str = 31950 / 45000;
    expect(roomRefusal(g, wake, [big])).toBe('Wake has room for one division, about 15,000 men. The XIV Corps has 31,950: too many for the island.');
    // On other ground it is a corps as any other.
    expect(whyNotShip(g, big, at(g, 'Manila')) ?? '').not.toMatch(/too many/);
  }, 300000);

  it('a computer staff passes it over in a landing wave and takes the corps of its force that fit (the design director, 21:37Z, 26 November 2026, 2)', () => {
    const g = startOpen(createGame(3, null));
    const wake = at(g, 'Wake'), quay = at(g, 'Kwajalein');
    const big = corps(g, 'JAP', 'Big', quay, 2), small = corps(g, 'JAP', 'Small', quay, 1, 0.4), next = corps(g, 'JAP', 'Next', quay, 1, 0.5);
    expect(fitWave(g, wake, 'JAP', [big, small, next])).toEqual([small, next]);
    expect(shoreRoom(g, wake, 'JAP', [big])).toBe(0);
  }, 300000);
});

describe('the men on an island as they stand', () => {
  it('drafts may put an island over its room: nobody is moved or struck, and it refuses new corps of that side (the design director, 21:37Z, 26 November 2026, 4)', () => {
    const g = war('USA');
    const wake = at(g, 'Wake'), honolulu = at(g, 'Honolulu');
    clear(g, wake);
    const a = corps(g, 'USA', 'Marines', wake, 1, 0.9), b = corps(g, 'USA', 'Defense Battalion', wake, 1, 0.08);
    expect(roomLeft(g, wake, 'USA')).toBeCloseTo(0.02, 9);
    // The month's drafts bring the marines to full strength in place (the design director, 21:37Z, 26 November 2026, 4: drafts are never refused).
    a.str = 1;
    expect(roomLeft(g, wake, 'USA')).toBe(0);
    expect([a, b].every((c) => !c.dead && c.reg === wake.id)).toBe(true);
    const more = corps(g, 'USA', 'More', honolulu, 1, 0.05);
    expect(whyNotShip(g, more, wake)).toBe('Wake is full: it has room for about 15,000 men and 17,000 of yours are there.');
  }, 300000);

  it('a retreat onto an island reads its own allowance, the room and two divisions\' worth (the design director, 21:37Z, 26 November 2026, 3)', () => {
    const g = startOpen(createGame(3, null));
    const wake = at(g, 'Wake');
    clear(g, wake);
    corps(g, 'USA', 'There', wake);
    const beaten = corps(g, 'USA', 'Beaten', at(g, 'Honolulu'), 2);
    expect(fits(g, wake, 'USA', [beaten])).toBe(false);
    expect(fits(g, wake, 'USA', [beaten], { slack: 2 })).toBe(true);
    beaten.divs = 3;
    expect(fits(g, wake, 'USA', [beaten], { slack: 2 })).toBe(false);
  }, 300000);
});

describe('two corps changing places onto an island', () => {
  it("stand while the men there after the swap fit, never take on a corps larger than the whole room, and say what stays (the design director, 21:37Z, 26 November 2026, 5)", () => {
    const g = war('UK');
    const singapore = at(g, 'Singapore'), johore = at(g, 'Johore');
    clear(g, singapore);
    const stay = corps(g, 'UK', 'Stays', singapore, 1, 0.8), out = corps(g, 'UK', 'Goes Out', singapore, 1, 0.3);
    // (12,000 stay; the corps coming off frees its 4,500. Singapore's room is 5, 75,000 men.)
    const fitsIn = corps(g, 'UK', '9th Corps', johore, 5, 0.7);
    expect(whyNoSwap(g, fitsIn, out)).toBeNull();
    fitsIn.str = 0.9;
    expect(men(fitsIn)).toBe(67500);
    expect(whyNoSwap(g, fitsIn, out)).toBe('Singapore has room for 5 divisions, about 75,000 men, and 12,000 of yours stay there after the swap: only 63,000 more can go, and the 9th Corps has 67,500.');
    expect(swapCorps(g, fitsIn, out)).toBe(false);
    // A swap that brings no more men than goes out stands, however full the island (79,500 there before and after).
    out.divs = 5; out.str = 0.9;
    expect(whyNoSwap(g, fitsIn, out)).toBeNull();
    // A corps larger than the whole room never swaps on: the too-large line first.
    fitsIn.divs = 6;
    expect(whyNoSwap(g, fitsIn, out)).toBe('Singapore has room for 5 divisions, about 75,000 men. The 9th Corps has 81,000: too many for the island.');
    expect(stay.reg).toBe(singapore.id);
  }, 300000);
});

describe('the winners of a fight for an island', () => {
  it('move in as many as its room in men takes, and the rest stay where they came from', () => {
    const g = startOpen(createGame(3, null));
    const singapore = at(g, 'Singapore'), johore = at(g, 'Johore');
    setWar(g, 'JAP', 'UK');
    for (const u of [...johore.units]) removeCorps(g, u);
    johore.ctrl = 'JAP';
    // (Four armies of three divisions: 45,000 men each at full strength, against Singapore's room of 75,000.)
    const force = [1, 2, 3, 4].map((k) => { const c = addCorps(g, 'JAP', `Army ${k}`, 'inf', 3, johore.id, {}); c.ap = c.apMax; c.xp = 0.8; return c; });
    const report = resolveBattle(g, force, singapore);
    expect(report.taken).toBe(true);
    const inside = placed(g, singapore, 'JAP');
    expect(inside.reduce((n, c) => n + men(c), 0)).toBeLessThanOrEqual(75000);
    expect(inside.map((c) => c.name)).toEqual(['Army 1']);
    expect(placed(g, johore, 'JAP').map((c) => c.name)).toEqual(['Army 2', 'Army 3', 'Army 4']);
    // Nobody was struck off for want of room.
    expect(force.every((c) => !c.dead)).toBe(true);
  }, 300000);
});

// The record's own hand (the design director, 14:28Z (a): "the record's hand at stops 1 and 2"). At stops 1 and 2 a
// dated row is put down at the record's place counted against five, island or not. At stops 3 to 5 the island's room
// in men binds it as it binds every placing (the design director, 21:37Z, 26 November 2026, 1: "at stops 3 to 5 the
// record's place reads the room in men like every other placing; stops 1 and 2 as built").
// The four cases B3 named as over their room in corps (Pearl Harbor, Kwajalein, Samoa, Hong Kong) are within it in
// men, and are kept below as such; the case still over room is an island filled by hand before the row comes.
describe("the record's rows and an island's room", () => {
  it("the four of B3 are within their room in men: Pearl Harbor's guard, Kwajalein's two rows, Samoa's two, Hong Kong's command", () => {
    const g = startOpen(createGame(3, 'USA'));
    const pearl = at(g, 'Pearl Harbor');
    g.turn = 14;
    startPhase(g, 'USA');
    const guard = g.corps.find((c) => c.nat === 'USA' && c.name === '298th Infantry (Hawaii National Guard)')!;
    expect(g.regions[guard.reg].name).toBe('Pearl Harbor');
    expect(roomLeft(g, pearl, 'USA')).toBeGreaterThanOrEqual(0);
    const h = startOpen(createGame(3, null));
    h.turn = 27; startPhase(h, 'JAP');
    const kwajalein = at(h, 'Kwajalein');
    const names = [...placed(h, kwajalein, 'JAP'), ...(kwajalein.garr ?? [])].map((c) => c.name);
    expect(names).toContain('61st Guard Unit (Kwajalein and Roi)');
    expect(names).toContain('Naval landing force (Maizuru 2nd, for Wake)');
    expect(roomLeft(h, kwajalein, 'JAP')).toBeGreaterThan(0);
    const k = startOpen(createGame(3, null));
    k.turn = 19; startPhase(k, 'USA');
    k.turn = 29; startPhase(k, 'USA');
    const samoa = at(k, 'Samoa');
    expect(placed(k, samoa, 'USA').map((c) => c.name).sort()).toEqual(['2nd Marine Brigade', '7th Defense Battalion']);
    expect(roomLeft(k, samoa, 'USA')).toBeGreaterThan(0);
    const m = startOpen(createGame(3, null));
    const hk = at(m, 'Hong Kong');
    for (let t = 2; t <= 24; t++) { m.turn = t; startPhase(m, 'UK'); }
    const now = placed(m, hk, 'UK');
    expect(now.map((c) => c.name)).toEqual(['China Command (Hong Kong garrison)']);
    expect(now[0].str).toBeCloseTo(0.7, 5);
  }, 300000);

  it('at stops 3 to 5 a row whose own island is full is sent on to the nearest ground with room; at stops 1 and 2 it stands there over the room (the design director, 21:37Z, 26 November 2026, 1)', () => {
    const run = (stop: number): string => {
      const g = startOpen(createGame(3, 'USA'));
      g.flags.hist = stop;
      const pearl = at(g, 'Pearl Harbor');
      // (Filled by hand before the 298th's month: two divisions of the player's at full strength beside the first day's.
      // The coast artillery takes no room, the design director, 00:20Z, 27 November 2026, D4, so one would leave 4,430.)
      corps(g, 'USA', 'Filler', pearl, 2, 1);
      expect(roomLeft(g, pearl, 'USA')).toBe(0);
      g.turn = 14;
      startPhase(g, 'USA');
      const guard = g.corps.find((c) => c.nat === 'USA' && c.name === '298th Infantry (Hawaii National Guard)' && !c.dead);
      return guard ? g.regions[guard.reg].name : 'none';
    };
    expect(run(3)).not.toBe('Pearl Harbor');
    expect(run(3)).not.toBe('none');
    expect(run(1)).toBe('Pearl Harbor');
  }, 300000);
});

describe("the design director's stand-ins of 14:28Z, flagged G (c)", () => {
  it('Singapore has room for 5, Formosa\'s south for 3, and the Kure landing forces stage at Palau, not Yap', () => {
    const g = startOpen(createGame(3, null));
    expect(roomSays(g, at(g, 'Singapore'))).toBe('Singapore has room for 5 divisions, about 75,000 men.');
    expect(roomSays(g, at(g, 'Formosa'))).toBe('Formosa has room for 3 divisions, about 45,000 men.');
    expect(roomSays(g, at(g, 'Taihoku'))).toBe('Taihoku has room for 2 divisions, about 30,000 men.');
    // The Kure row comes in its month at Palau, through the far rows' own amend.
    g.turn = 27;
    startPhase(g, 'JAP');
    const kure = g.corps.find((c) => c.nat === 'JAP' && !c.dead && c.name === 'Naval landing forces (Kure 1st and 2nd)')!;
    expect(kure).toBeDefined();
    expect(g.regions[kure.reg].name).toBe('Palau');
  }, 300000);
});

// The UI director's marks of 15:02Z 25 November 2026 and her answers of 21:37Z 26 November 2026, every sentence as the
// 3D game reads it through the bridge (TC.province, TC.railTip, TC.bySeaWhy, TC.move), on three boards of seed 3.
describe("the island's sentences on a live board", () => {
  it("says each of the UI director's sentences whole on a live board, through the bridge", () => {
    // Board A: seed 3, Britain seated, its first month, the far side on.
    {
      const g = war('UK');
      const sg = at(g, 'Singapore'), jo = at(g, 'Johore');
      const ids = (cs: Corps[]) => cs.map((c) => c.id);
      const a1 = corps(g, 'UK', '1st Army', jo, 2), a2 = corps(g, 'UK', '2nd Army', jo, 2), a3 = corps(g, 'UK', '3rd Army', jo, 2);
      TC().load(saveGame(g));
      expect(TC().province(sg.id).island.line, "head Singapore").toBe("Singapore has room for 5 divisions, about 75,000 men.");
      // (7,050 of his there: the coast guns are a fortress crew and take no room, the design director, 00:20Z, 27
      // November 2026, D4.)
      expect(TC().railTip(ids([a1, a2, a3]), sg.id).no, "some there (Singapore 7,050 of his; three armies of 30,000 by rail from Johore)").toBe("Singapore has room for 5 divisions, about 75,000 men, and 8,000 of yours are there: only 67,000 more can go, and you are sending 90,000.");
      const a4 = corps(g, 'UK', '4th Army', jo, 2, 0.5);
      clear(g, sg);
      TC().load(saveGame(g));
      expect(TC().railTip(ids([a1, a2, a3]), sg.id).no, "none there (Singapore emptied by hand; 90,000 by rail)").toBe("Singapore has room for 5 divisions, about 75,000 men: you are sending 90,000.");
      const f = [1, 2, 3, 4, 5].map((k) => corps(g, 'UK', `Filler ${k}`, sg));
      TC().load(saveGame(g));
      expect(TC().railTip(ids([a4]), sg.id).no, "full (five fresh divisions of his on Singapore)").toBe("Singapore is full: it has room for about 75,000 men and 75,000 of yours are there.");
      relocate(g, f[4], jo); relocate(g, f[3], jo);
      const ally = g.corps.find((c) => !c.dead && c.nat !== 'UK' && c.kind === 'inf' && allied(g, c.nat, 'UK'))!;
      ally.divs = 1; ally.str = 1; relocate(g, ally, sg);
      TC().load(saveGame(g));
      expect(TC().railTip(ids([a1]), sg.id).no, 'ally there').toBe("Singapore has room for 5 divisions, about 75,000 men, and 60,000 are there: only 15,000 more can go, and you are sending 30,000.");
      relocate(g, ally, jo); relocate(g, f[2], jo);
      const over = corps(g, 'UK', 'Over', jo, 2, 30400 / 30000);
      TC().load(saveGame(g));
      expect(TC().railTip(ids([over, f[2]]), sg.id).no, "shortfall, some there (30,000 there; 45,400 sent)").toBe("Singapore has room for 5 divisions, about 75,000 men, and 30,000 of yours are there: you are sending 400 men more than fit.");
      for (const x of f.slice(0, 2)) relocate(g, x, jo);
      const big = corps(g, 'UK', 'Big Army', jo, 5, 1), extra = corps(g, 'UK', 'Over Too', jo, 1, 400 / 15000);
      TC().load(saveGame(g));
      expect(TC().railTip(ids([big, extra]), sg.id).no, "shortfall, none there (75,400 sent)").toBe("Singapore has room for 5 divisions, about 75,000 men: you are sending 400 men more than fit.");
      const huge = corps(g, 'UK', 'XIV Corps', jo, 6, 0.9), huge2 = corps(g, 'UK', 'XV Corps', jo, 6, 1), huge3 = corps(g, 'UK', 'XVI Corps', jo, 7, 1);
      TC().load(saveGame(g));
      expect(TC().railTip(ids([huge]), sg.id).no, "too large, no split").toBe("Singapore has room for 5 divisions, about 75,000 men. The XIV Corps has 81,000: too many for the island.");
      setBreakUp(g, true);
      TC().load(saveGame(g));
      expect(TC().railTip(ids([huge]), sg.id).no, "too large, split on").toBe("Singapore has room for 5 divisions, about 75,000 men. The XIV Corps has 81,000, too many to go there whole: split it and send a part.");
      setBreakUp(g, false);
      const e = [1, 2, 3].map((k) => { const x = addCorps(g, 'UK', `Engineers ${k}`, 'eng', 1, jo.id, {}); x.ap = x.apMax; return x; });
      TC().load(saveGame(g));
      expect(TC().railTip(ids(e), sg.id).no, "engineers, none there (three by rail)").toBe("Singapore has room for 2 engineer corps as well as its troops: only 2 can go.");
      relocate(g, e[0], sg);
      TC().load(saveGame(g));
      expect(TC().railTip(ids([e[1], e[2]]), sg.id).no, "engineers, one there (two by rail)").toBe("Singapore has room for 2 engineer corps as well as its troops and one of yours is there: only one more can go.");
      relocate(g, e[1], sg);
      TC().load(saveGame(g));
      expect(TC().bySeaWhy(ids([e[2]]), sg.id).why, "engineers, full (one by sea)").toBe("Singapore can take no more engineers: it has room for 2 engineer corps as well as its troops, and 2 of yours are there.");
      expect(TC().railTip(ids([e[2]]), sg.id).no, "engineers, full (one by rail)").toBe("Singapore can take no more engineers: it has room for 2 engineer corps as well as its troops, and 2 of yours are there.");
      // The swap: 12,000 stay, 4,500 go out, a corps of 67,500 marches from Johore.
      clear(g, sg);
      corps(g, 'UK', 'Stays', sg, 1, 0.8); corps(g, 'UK', 'Also Stays', sg, 1, 0.8);
      const ninth = corps(g, 'UK', '9th Corps', jo, 5, 0.9);
      TC().load(saveGame(g));
      // (A march reads the march's sentence; the swap's only where a swap is asked, for the corps he picked: the UI
      // director, 00:19Z, 27 November 2026, U3 and U4.)
      expect(TC().move([ninth.id], sg.id).note, "march refused (two of 12,000 there; a corps of 67,500 marches from Johore)").toBe("Singapore has room for 5 divisions, about 75,000 men, and 24,000 of yours are there: only 51,000 more can go, and you are sending 68,000.");
      expect(whyNoSwap(g, ninth, sg.units.find((u) => u.name === 'Stays')!), "swap asked (Stays picked)").toBe("Singapore has room for 5 divisions, about 75,000 men, and 12,000 of yours stay there after the swap: only 63,000 more can go, and the 9th Corps has 67,500.");
    }
    // Board B: seed 3, Japan seated, its first month, at war with America by hand; landings from Kwajalein.
    {
      const g = war('JAP');
      const wake = at(g, 'Wake'), q = at(g, 'Kwajalein');
      const a = corps(g, 'JAP', 'First', q), b = corps(g, 'JAP', 'Second', q);
      const e1 = addCorps(g, 'JAP', 'Engineers 1', 'eng', 1, q.id, {}), e2 = addCorps(g, 'JAP', 'Engineers 2', 'eng', 1, q.id, {});
      for (const x of [e1, e2]) x.ap = x.apMax;
      declareWar(g, 'JAP', 'USA');
      TC().load(saveGame(g));
      expect(TC().province(wake.id).island.line, "head Wake").toBe("Wake has room for one division, about 15,000 men.");
      expect(TC().province(at(g, 'Okinawa').id).island.line, "head Okinawa").toBe("Okinawa has room for 4 divisions, about 60,000 men.");
      expect(TC().bySeaWhy([a.id, b.id], wake.id).why, "landing, none there (two divisions on Wake)").toBe("Wake has room for one division, about 15,000 men: you are sending 30,000.");
      expect(TC().bySeaWhy([a.id, e1.id, e2.id], wake.id).why, "landing, engineers (one division and two engineer corps on Wake)").toBe("Wake has room for one engineer corps as well as its troops: only one can go.");
      const big1 = corps(g, 'JAP', 'South Seas Force', q, 2), big2 = corps(g, 'JAP', 'Second Force', q, 2), big3 = corps(g, 'JAP', 'Third Force', q, 3);
      TC().load(saveGame(g));
      expect(TC().bySeaWhy([big1.id, a.id, big2.id], wake.id).why, "landing, too large, one more too large").toBe("Wake has room for one division, about 15,000 men. South Seas Force has 30,000: too many for the island. One more of the corps you are sending is too large for Wake as well.");
      expect(TC().bySeaWhy([big1.id, big2.id, big3.id], wake.id).why, "landing, too large, two more too large").toBe("Wake has room for one division, about 15,000 men. South Seas Force has 30,000: too many for the island. Two more of the corps you are sending are too large for Wake as well.");
    }
    // Board C: seed 3, America seated, its first month: Wake its own.
    {
      const g = war('USA');
      const wake = at(g, 'Wake'), hon = at(g, 'Honolulu');
      const first = corps(g, 'USA', 'First', hon), second = corps(g, 'USA', 'Second', hon);
      const s1 = addCorps(g, 'USA', 'Sappers', 'eng', 1, hon.id, {}), s2 = addCorps(g, 'USA', 'Bridgers', 'eng', 1, hon.id, {});
      for (const x of [s1, s2]) x.ap = x.apMax;
      relocate(g, first, wake); relocate(g, s1, wake);
      TC().load(saveGame(g));
      expect(TC().bySeaWhy([second.id], wake.id).why, "full (one fresh division there; a second by sea)").toBe("Wake is full: it has room for about 15,000 men and 15,000 of yours are there.");
      expect(TC().bySeaWhy([s2.id], wake.id).why, "engineers full (one there; a second by sea)").toBe("Wake can take no more engineers: it has room for one engineer corps as well as its troops, and yours is there.");
    }
  }, 600000);
});

describe('one body', () => {
  it("no file but islands.ts reads an island's room or counts corps against it by a path of its own", () => {
    expect(Object.keys(SOURCES).length).toBeGreaterThan(100);
    const bad: string[] = [];
    for (const [f, src] of Object.entries(SOURCES)) {
      if (f.endsWith('/campaign/islands.ts')) continue;
      if (/\broomAt\b/.test(src)) bad.push(`${f}: roomAt`);
      if (/islandOf\([^)]*\)\??\.room\b/.test(src)) bad.push(`${f}: islandOf(...).room`);
      // A count of corps (`.length`) compared with STACK is a room check by a path of its own, unless it is the
      // record's hand at stops 1 and 2 (marked so on its line) or the size of a landing (navy.ts, not a room).
      // (The record's hand is exempt only where it is laid, the dated rows and the book's stations, game.ts and stations.ts.)
      const hand = /\/campaign\/(game|stations)\.ts$/.test(f);
      src.split('\n').forEach((line: string, i: number) => {
        if (/\.length\s*(>=|<|<=|>)\s*STACK\b/.test(line) && !(hand && /record's hand/.test(line)) && !/No more than \$\{STACK\} corps can go ashore/.test(line)) bad.push(`${f}:${i + 1}: ${line.trim()}`);
        // A count of corps compared with the room left (in divisions' worth on an island) is a second count of the room:
        // the sceptic's mutation of 26 November 2026, `placed(...).length >= Math.floor(roomLeft(...))`, passed the old scan.
        if (/\.length\s*(>=|<|<=|>)\s*(Math\.(floor|ceil|round)\()?\s*roomLeft\(/.test(line) || /roomLeft\([^;]*\)\)?\s*(>=|<|<=|>)\s*[\w.()[\]]*\.length\b/.test(line)) bad.push(`${f}:${i + 1}: ${line.trim()}`);
      });
    }
    expect(bad).toEqual([]);
  });
});
