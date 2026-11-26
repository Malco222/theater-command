// A cut-off island garrison withers, and is never a free prize (ruling 173(e) and its rider (o); ruling 181(f) and (g);
// the design director's form B9 of 25 November 2026). On an island of the historian's table, on the whole-world chart:
// hunger wears a cut-off garrison to the island's floor and no lower, a quarter on an atoll and a half on an island
// large enough to feed itself; with its stores gone it only defends; it does not lay down its arms; a landing meets
// what is left; it mends when a ship of its side can reach it again. Ground that is no island of the table starves as
// it always did. (src/campaign/wither.ts; game.ts startPhase.)
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import { fightersOver } from '../src/campaign/air';
import { wrecked, whyNotField } from '../src/campaign/airversion';
import { whyNotOrder } from '../src/campaign/armies';
import { addCorps, assess, atWar, capture, computeSupply, createGame, declareWar, endPhase, enemyAshore, evacuate, moveCorps, removeCorps, resolveBattle, startPhase, whyNotEvacuate } from '../src/campaign/game';
import { islandOf } from '../src/campaign/islands';
import { corpsMood } from '../src/campaign/morale';
import { closedTo, landCorps, whyNotLand, whyNotShip } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { besieged, garrison, invest, salliers, storm, whyNotSally, whyNotStorm } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';
import { witherAt, witherStores, withered } from '../src/campaign/wither';

const LONG = 600000;
const WITHERED = 'Cut off and out of stores, it holds where it stands. It cannot attack, march or sail until stores reach it by sea.';

/** The whole world in January 1942, Japan at war with the western powers. `human` plays one of them, or nobody does. */
function war(human: string | null = null): Game {
  const g = startOpen(createGame(3, null));
  g.turn = 28;
  for (const e of ['USA', 'UK', 'NED', 'AUS']) if (!atWar(g, 'JAP', e)) declareWar(g, 'JAP', e);
  // (Japan's friends with ships are at war with America as well: a fortified port's stores are kept fresh by any
  // friend whose ships still reach it, and an Italy at peace with America would keep Truk fed for ever.)
  for (const a of ['GER', 'ITA']) if (!atWar(g, a, 'USA')) declareWar(g, a, 'USA');
  g.turn = 28;
  g.human = human;
  return g;
}
/** A place in one nation's hands for a good while, emptied of whoever stood there if it was another's. */
function hold(g: Game, place: string, nat: string): Region {
  const r = g.byName[place];
  if (r.ctrl !== nat) { for (const u of [...r.units]) removeCorps(g, u); capture(g, r, nat); r.since = 20; }
  return r;
}
/** The sea round an island closed to `to`: every fleet of `by` on patrol in it, and `to`'s own ships in harbour. */
function close(g: Game, r: Region, by = ['USA'], to = 'JAP'): void {
  let k = 0;
  for (const f of g.fleets) {
    if (by.includes(f.nat)) { f.zone = r.seas[k++ % r.seas.length]; f.stance = 'patrol'; }
    else if (f.nat === to) f.stance = 'port';
  }
  computeSupply(g);
}
/** One nation's month opens and closes with nothing ordered; its ships stay in harbour. */
function month(g: Game, nat = 'JAP'): void {
  g.turn++;
  startPhase(g, nat);
  endPhase(g, nat);
  for (const f of g.fleets) if (f.nat === nat) f.stance = 'port';
}
const state = (c: Corps): number[] => [Math.round(c.org * 1e4) / 1e4, Math.round(c.str * 1e4) / 1e4];
const withers = (g: Game): string[] => g.headlines.filter((h) => /withers on/.test(h.text)).map((h) => h.text);

describe('a cut-off island garrison withers and is never a free prize (ruling 173(e), rider (o))', () => {
  it('on an atoll a Japanese garrison is worn to a quarter and no lower, is still there after eight months, and only defends', () => {
    const g = war('JAP');
    const r = g.byName['Kwajalein'];
    expect(islandOf(g, r)).toMatchObject({ floor: 0.25 });
    const c = addCorps(g, 'JAP', 'Kwajalein garrison (a test row)', 'inf', 1, 'Kwajalein', {});
    close(g, r);
    expect(closedTo(g, r.seas[0], 'JAP')).toBe(true);
    expect(c.supplied).toBe(false);
    // Its card counts its stores, the three months the game has always given a fresh corps, and says what follows.
    expect(corpsMood(g, c).restLine).toBe('Cut off from supply. It gets no rest, and its stores last 3 months more. After that it withers to a quarter of its strength: it holds on but cannot attack.');
    // The first hungry month is the one it always was. It keeps its moves while its stores last.
    month(g);
    expect(state(c)).toEqual([0.6, 0.85]);
    expect([withered(g, c), witherStores(g, c), c.ap]).toEqual([false, 2, 4]);
    expect(whyNotOrder(g, c)).toBeNull();
    // The second would have taken its order to two tenths: the island's floor holds it at a quarter.
    month(g);
    expect(state(c)).toEqual([0.25, 0.7225]);
    expect([withered(g, c), witherStores(g, c), c.ap]).toEqual([false, 1, 4]);
    expect(corpsMood(g, c).restLine).toBe('Cut off from supply. It gets no rest, and its stores run out this month. Then it withers to a quarter of its strength: it holds on but cannot attack.');
    expect(withers(g)).toEqual([]);
    // The third, the month it used to lay down its arms: its stores are gone, and it stands at the floor.
    month(g);
    expect(c.dead).toBe(false);
    expect(state(c)).toEqual([0.25, 0.25]);
    expect([withered(g, c), witherStores(g, c), c.ap]).toEqual([true, 0, 0]);
    expect(g.flags[`wither:${c.id}`]).toBe(0);
    expect(withers(g)).toContain('Cut off and out of stores, Kwajalein garrison (a test row) withers on Kwajalein. It holds on at a quarter of its strength and will not lay down its arms, but it can no longer attack. It recovers once stores reach it by sea.');
    const told = withers(g).length;
    // And so it stays, month after month: eight months cut off, and the island is still Japan's and still held.
    for (let m = 4; m <= 8; m++) {
      month(g);
      expect(c.dead, `month ${m}`).toBe(false);
      expect(state(c), `month ${m}`).toEqual([0.25, 0.25]);
      expect([withered(g, c), c.ap], `month ${m}`).toEqual([true, 0]);
    }
    expect(r.ctrl).toBe('JAP');
    expect(r.units).toContain(c);
    // It is told once, in the month it withers, and no headline says it laid down its arms.
    expect(withers(g).filter((t) => t.includes('Kwajalein garrison'))).toHaveLength(1);
    expect(withers(g).length).toBeGreaterThanOrEqual(told);
    expect(g.headlines.some((h) => /lays down its arms/.test(h.text) && h.region === r.id)).toBe(false);
    // It cannot attack, march or sail, and no ship lifts it off: the card and the refused order say why.
    expect(whyNotOrder(g, c)).toBe(WITHERED);
    expect(corpsMood(g, c).restLine).toBe('Cut off and out of stores. It has withered to a quarter of its strength and holds on there. It gets no rest and no drafts, and cannot attack, until stores reach it by sea.');
    expect(whyNotShip(g, c, g.byName['Truk'])).toBe('Only a corps that has not moved this month can embark.');
    expect(evacuate(g, c)).toBe(false);
    expect(whyNotEvacuate(g, c)).toBe('Kwajalein garrison (a test row) is cut off and out of stores on Kwajalein: it cannot be lifted off until stores have reached it by sea.');
    // It stood where it was, and its works are its own still: a garrison with no moves left is not one that marched.
    // They are the two months' works it dug while its stores lasted, and no more: starving men dig nothing new.
    expect([c.dug, c.dugFor]).toEqual([true, 2]);
    // The count is a number in the game's flags, and a saved game keeps it.
    const back = loadGame(saveGame(g))!;
    const same = back.corps.find((x) => x.id === c.id)!;
    expect([withered(back, same), witherStores(back, same), state(same)]).toEqual([true, 0, [0.25, 0.25]]);
  }, LONG);

  it('a landing meets what is left: the atoll a fresh garrison holds against one division falls to it once the garrison has withered', () => {
    const g = war();
    g.turn = 36;
    const r = g.byName['Kwajalein'];
    const c = addCorps(g, 'JAP', 'Kwajalein garrison (a test row)', 'inf', 1, 'Kwajalein', {});
    hold(g, 'Eniwetok', 'USA');
    const marines = [addCorps(g, 'USA', 'I Marine Corps (a test row)', 'inf', 1, 'Eniwetok', { xp: 0.4 })];
    close(g, r);
    const fresh = assess(g, marines, r, {}).odds;
    expect(fresh).toBeLessThan(2);
    for (let m = 1; m <= 8; m++) month(g);
    expect([c.dead, withered(g, c), state(c)]).toEqual([false, true, [0.25, 0.25]]);
    startPhase(g, 'USA');
    expect(whyNotLand(g, marines, r)).toBeNull();
    // A quarter of its men at a quarter of its order: the same division now comes ashore at long odds on.
    expect(assess(g, marines, r, {}).odds).toBeGreaterThan(3 * fresh);
    const landing = landCorps(g, marines, r);
    expect(landing.ok).toBe(true);
    expect(landing.taken).toBe(true);
    expect(r.ctrl).toBe('USA');
    expect(c.dead).toBe(true);
    expect(g.headlines.some((h) => h.text === 'Kwajalein falls to American forces.')).toBe(true);
    // Its count goes with it: nothing of a dead garrison is left in the game's flags.
    expect(g.flags[`wither:${c.id}`]).toBeUndefined();
  }, LONG);

  it('an enemy who steps ashore beside a withered atoll garrison does not starve it out: it holds at the floor, the town\'s stores do not rest it, and it falls only to an assault', () => {
    const g = war();
    g.turn = 36;
    const r = g.byName['Kwajalein'];
    const c = addCorps(g, 'JAP', 'Kwajalein garrison (a test row)', 'inf', 1, 'Kwajalein', {});
    // The garrison keeps to the atoll's town, so a landing finds the beach empty and has no battle to fight for it.
    expect(garrison(g, c)).toBe(true);
    hold(g, 'Eniwetok', 'USA');
    const marines = [addCorps(g, 'USA', 'I Marine Corps (a test row)', 'inf', 1, 'Eniwetok', { xp: 0.4 })];
    close(g, r);
    for (let m = 1; m <= 5; m++) month(g);
    expect([c.dead, withered(g, c), state(c)]).toEqual([false, true, [0.25, 0.25]]);
    // The Americans land on the empty beach: no battle, the town under siege, an enemy ashore beside the garrison.
    startPhase(g, 'USA');
    const landing = landCorps(g, marines, r);
    expect([landing.ok, landing.report ?? null, besieged(g, r), enemyAshore(g, c), r.ctrl]).toEqual([true, null, true, true, 'JAP']);
    endPhase(g, 'USA');
    close(g, r);
    // The rule still answers for it: it has no stores left for a siege to eat.
    expect(witherAt(g, c, true)).toMatchObject({ floor: 0.25 });
    // Six months of siege. The first is the month a town's own stores feed its garrison: they are no ship, and neither
    // mend it nor rest it. In the rest it used to lay down its arms unfought. It stands at the floor throughout.
    const fed: boolean[] = [];
    for (let m = 1; m <= 6; m++) {
      month(g);
      fed.push(c.supplied);
      expect(c.dead, `month ${m}`).toBe(false);
      expect(state(c), `month ${m}`).toEqual([0.25, 0.25]);
      expect([withered(g, c), witherStores(g, c), c.ap, r.ctrl, besieged(g, r)], `month ${m}`).toEqual([true, 0, 0, 'JAP', true]);
    }
    expect(fed).toEqual([true, false, false, false, false, false]);
    expect(corpsMood(g, c).restLine).toBe('Cut off and out of stores. It has withered to a quarter of its strength and holds on there. It gets no rest and no drafts, and cannot attack, until stores reach it by sea.');
    expect(salliers(g, r, 'JAP')).toEqual([]);
    expect(g.headlines.some((h) => h.region === r.id && /lays down its arms|Starved out/.test(h.text))).toBe(false);
    // It gives in under assault, as any garrison with no stores does: the division round the town storms it.
    g.turn++;
    startPhase(g, 'USA');
    expect(whyNotStorm(g, r, 'USA')).toBeNull();
    const assault = storm(g, marines, r);
    expect(assault?.taken).toBe(true);
    expect([r.ctrl, c.dead]).toEqual(['USA', true]);
    expect(g.flags[`wither:${c.id}`]).toBeUndefined();
  }, LONG);

  it('on a large island the floor is a half: still there after eight months, and it falls to a landing', () => {
    const g = war();
    g.turn = 36;
    const r = hold(g, 'Guadalcanal', 'JAP');
    expect(islandOf(g, r)).toMatchObject({ floor: 0.5 });
    const c = addCorps(g, 'JAP', 'Guadalcanal garrison (a test row)', 'inf', 1, 'Guadalcanal', {});
    hold(g, 'Espiritu Santo', 'USA');
    const marines = [addCorps(g, 'USA', 'I Marine Corps (a test row)', 'inf', 1, 'Espiritu Santo', { xp: 0.4 })];
    close(g, r);
    expect(c.supplied).toBe(false);
    const fresh = assess(g, marines, r, {}).odds;
    month(g);
    expect(state(c)).toEqual([0.6, 0.85]);
    month(g);
    expect(state(c)).toEqual([0.5, 0.7225]);
    expect([withered(g, c), c.ap]).toEqual([false, 4]);
    month(g);
    expect(state(c)).toEqual([0.5, 0.5]);
    expect([withered(g, c), c.ap]).toEqual([true, 0]);
    for (let m = 4; m <= 8; m++) month(g);
    expect([c.dead, withered(g, c), state(c), r.ctrl]).toEqual([false, true, [0.5, 0.5], 'JAP']);
    startPhase(g, 'USA');
    expect(whyNotLand(g, marines, r)).toBeNull();
    expect(assess(g, marines, r, {}).odds).toBeGreaterThan(2 * fresh);
    const landing = landCorps(g, marines, r);
    expect([landing.ok, landing.taken]).toEqual([true, true]);
    expect(c.dead).toBe(true);
    expect(r.ctrl).not.toBe('JAP');
  }, LONG);

  it('on a large island it may attack on its own island while its stores last, and only defends once they are gone (rider (o)(3))', () => {
    const g = war('JAP');
    const r = hold(g, 'Rabaul', 'JAP');
    const gloucester = g.byName['Cape Gloucester'];
    expect(r.nb).toContain(gloucester.id);
    const a = addCorps(g, 'JAP', 'Rabaul force A (a test row)', 'inf', 1, 'Rabaul', {});
    const b = addCorps(g, 'JAP', 'Rabaul force B (a test row)', 'inf', 1, 'Rabaul', {});
    hold(g, 'Cape Gloucester', 'AUS');
    addCorps(g, 'AUS', 'Cape Gloucester force (a test row)', 'inf', 1, 'Cape Gloucester', {});
    close(g, r, ['USA', 'UK', 'AUS', 'NED']);
    expect(r.seas.map((z) => closedTo(g, z, 'JAP'))).toEqual(r.seas.map(() => true));
    expect([a.supplied, b.supplied]).toEqual([false, false]);
    month(g);
    month(g);
    // Two hungry months in, its stores not yet gone: it takes orders, has its moves, and its attack next door is fought.
    expect([withered(g, a), a.ap, whyNotOrder(g, a)]).toEqual([false, 4, null]);
    const fight = resolveBattle(g, [a], gloucester);
    expect(fight.level).toBeTruthy();
    expect(a.fought).toBe(true);
    // The month its stores are gone both stand at the floor, and what the battle cost the one that fought is not given back.
    month(g);
    expect([withered(g, a), withered(g, b)]).toEqual([true, true]);
    expect(state(b)).toEqual([0.5, 0.5]);
    expect(a.org).toBeLessThanOrEqual(0.5);
    expect(a.str).toBeLessThanOrEqual(0.5);
    for (const c of [a, b]) {
      expect(c.ap).toBe(0);
      expect(whyNotOrder(g, c)).toBe(WITHERED);
      expect(moveCorps(g, c, gloucester)).toBe(false);
    }
    // A count does not travel. Set down by hand on ground of the same land mass that the table does not carry, still
    // cut off, the corps has its count struck off as its month opens and starves there as a corps always did.
    for (const u of [...gloucester.units]) removeCorps(g, u);
    capture(g, gloucester, 'JAP');
    r.units.splice(r.units.indexOf(b), 1);
    b.reg = gloucester.id;
    gloucester.units.push(b);
    computeSupply(g);
    expect([islandOf(g, gloucester), b.supplied, withered(g, b)]).toEqual([null, false, false]);
    month(g);
    expect([b.dead, witherStores(g, b), state(b), b.ap]).toEqual([false, undefined, [0.1, 0.425], 4]);
    month(g);
    expect(b.dead).toBe(true);
  }, LONG);

  it("a fortified port's stores come first: Truk feeds its garrison two months, and then it withers to a half as any other", () => {
    const g = war();
    const r = g.byName['Truk'];
    expect(r.fort).toBeTruthy();
    for (const u of [...r.units]) removeCorps(g, u);
    const c = addCorps(g, 'JAP', 'Truk garrison (a test row)', 'inf', 1, 'Truk', {});
    // (A month of the war with the sea open: the port has its stores in.)
    computeSupply(g);
    close(g, r);
    const seen: string[] = [];
    for (let m = 1; m <= 9; m++) { month(g); seen.push(`${state(c).join('/')}${withered(g, c) ? ' withered' : ''}`); }
    expect(seen).toEqual(['1/1', '1/1', '0.6/0.85', '0.5/0.7225', '0.5/0.5 withered', '0.5/0.5 withered', '0.5/0.5 withered', '0.5/0.5 withered', '0.5/0.5 withered']);
    expect(c.dead).toBe(false);
  }, LONG);

  it('it mends when a ship of its side can reach it again: the count is struck off, and rest and drafts come as to any corps', () => {
    const g = war('JAP');
    const r = g.byName['Kwajalein'];
    const c = addCorps(g, 'JAP', 'Kwajalein garrison (a test row)', 'inf', 1, 'Kwajalein', {});
    close(g, r);
    for (let m = 1; m <= 4; m++) month(g);
    expect([withered(g, c), state(c), c.ap]).toEqual([true, [0.25, 0.25], 0]);
    // The American ships go home.
    const pearl = g.byName['Pearl Harbor'];
    for (const f of g.fleets) if (f.nat === 'USA') { f.zone = pearl.seas[0]; f.stance = 'port'; }
    month(g);
    expect(closedTo(g, r.seas[0], 'JAP')).toBe(false);
    expect(c.supplied).toBe(true);
    expect([withered(g, c), witherStores(g, c)]).toEqual([false, undefined]);
    expect(g.flags[`wither:${c.id}`]).toBeUndefined();
    expect(c.ap).toBe(c.apMax);
    expect(whyNotOrder(g, c)).toBeNull();
    expect(c.org).toBeGreaterThan(0.25);
    expect(c.str).toBeGreaterThan(0.25);
    const then = c.str;
    month(g);
    expect(c.str).toBeGreaterThan(then);
    // Cut off a second time, its stores are counted afresh from what it is by then.
    close(g, r);
    month(g);
    expect(withered(g, c)).toBe(false);
    expect(witherStores(g, c)).toBeGreaterThanOrEqual(1);
  }, LONG);

  it('with the enemy ashore: on an atoll hunger is the siege\'s, as it always was; on a large island the garrison withers and holds', () => {
    const story = (place: string, from: string, str = 1) => {
      const g = war();
      const r = hold(g, place, 'JAP');
      const c = addCorps(g, 'JAP', `${place} garrison (a test row)`, 'inf', 1, place, {});
      c.str = str;
      expect(garrison(g, c)).toBe(true);
      // An American division ashore, set there by hand: the field is its own and the town the garrison's.
      const base = hold(g, from, 'USA');
      const foe = addCorps(g, 'USA', 'A division ashore (a test row)', 'inf', 1, from, {});
      base.units.splice(base.units.indexOf(foe), 1);
      foe.reg = r.id;
      r.units.push(foe);
      invest(g, r, 'USA');
      close(g, r);
      expect([besieged(g, r), enemyAshore(g, c)]).toEqual([true, true]);
      return { g, r, c };
    };
    // Kwajalein: the rule does not answer, and the siege runs as it did: the town's stores, three hungry months, the fall.
    // (The test row stands at 0.8, 12,000 men: since an island's room is counted in men (the design director, 14:28Z
    // 25 November 2026), a test row of a full division would fill the atoll, and the record's own two rows for it in
    // January 1942 (the 61st Guard Unit and the Maizuru landing force, 2,550 men) would wait for room at this stop.)
    const atoll = story('Kwajalein', 'Eniwetok', 0.8);
    expect(witherAt(atoll.g, atoll.c, true)).toBeNull();
    expect(witherAt(atoll.g, atoll.c, false)).not.toBeNull();
    const seen: string[] = [];
    for (let m = 1; m <= 4; m++) { month(atoll.g); seen.push(atoll.c.dead ? 'gone' : state(atoll.c).join('/')); }
    expect(seen).toEqual(['1/0.8', '0.6/0.68', '0.2/0.578', 'gone']);
    expect(atoll.r.ctrl).toBe('USA');
    expect(Object.keys(atoll.g.flags).filter((k) => k === `wither:${atoll.c.id}`)).toEqual([]);
    // Guadalcanal, large enough to feed itself: rider (o)(3) carries the rule to it with the enemy ashore.
    const large = story('Guadalcanal', 'Espiritu Santo');
    expect(witherAt(large.g, large.c, true)).toMatchObject({ floor: 0.5 });
    for (let m = 1; m <= 9; m++) month(large.g);
    expect([large.c.dead, withered(large.g, large.c), state(large.c), large.r.ctrl]).toEqual([false, true, [0.5, 0.5], 'JAP']);
    expect(besieged(large.g, large.r)).toBe(true);
    // Withered, it holds the town and makes no sortie.
    expect(salliers(large.g, large.r, 'JAP')).toEqual([]);
    large.g.human = 'JAP';
    expect(whyNotSally(large.g, large.r, 'JAP')).toBe('The garrison is cut off and out of stores. It holds the town and makes no sortie.');
  }, LONG);

  it('ground that is no island of the table starves as it always did: an Italian garrison cut off on Rhodes lays down its arms in its third month', () => {
    const g = startOpen(createGame(3, null));
    g.turn = 28;
    if (!atWar(g, 'ITA', 'UK')) declareWar(g, 'ITA', 'UK');
    g.turn = 28;
    const r = hold(g, 'Rhodes', 'ITA');
    expect(islandOf(g, r)).toBeNull();
    const c = addCorps(g, 'ITA', 'Rhodes garrison (a test row)', 'inf', 1, 'Rhodes', {});
    close(g, r, ['UK'], 'ITA');
    expect(c.supplied).toBe(false);
    expect(witherAt(g, c, false)).toBeNull();
    const seen: string[] = [];
    for (let m = 1; m <= 3; m++) { month(g, 'ITA'); seen.push(c.dead ? 'gone' : `${state(c).join('/')} ap ${c.ap}`); expect(withered(g, c)).toBe(false); }
    expect(seen).toEqual(['0.6/0.85 ap 4', '0.2/0.7225 ap 4', 'gone']);
    expect(Object.keys(g.flags).filter((k) => k.startsWith('wither:'))).toEqual([]);
  }, LONG);

  it('rider (o)(2): the aircraft on its field fly nothing and count for nothing from the month after the sea closes, while the men last by their stores', () => {
    const g = war();
    const r = g.byName['Kwajalein'];
    const c = addCorps(g, 'JAP', 'Kwajalein garrison (a test row)', 'inf', 1, 'Kwajalein', {});
    const land = g.airFleets.filter((f) => f.nat === 'JAP' && f.ship === undefined && f.ftr > 0);
    const f = land[0], other = land.find((x) => x !== f && islandOf(g, g.regions[x.base]) === null)!;
    f.base = r.id;
    close(g, r);
    expect([f.unfed, wrecked(g, f)]).toEqual([undefined, false]);
    expect(fightersOver(g, r.id, 'USA')).toBeGreaterThan(0);
    month(g);
    expect(f.unfed).toBe(1);
    expect(wrecked(g, f)).toBe(true);
    expect(whyNotField(g, f)).toBe('Grounded. Kwajalein is cut off, and no fuel or bombs reach its field. It flies again once they reach the island by sea.');
    expect(fightersOver(g, r.id, 'USA')).toBe(0);
    // The men have their stores yet.
    expect([withered(g, c), witherStores(g, c)]).toEqual([false, 2]);
    // An unfed field on ground that is no island of the table is what ruling 184 made it, and no more: not grounded.
    other.unfed = 1;
    expect(wrecked(g, other)).toBe(false);
    delete other.unfed;
    // The sea opens: the field is fed as its country's next month in the air opens, and the group flies again.
    const pearl = g.byName['Pearl Harbor'];
    for (const x of g.fleets) if (x.nat === 'USA') { x.zone = pearl.seas[0]; x.stance = 'port'; }
    month(g);
    expect([f.unfed, wrecked(g, f)]).toEqual([undefined, false]);
    expect(fightersOver(g, r.id, 'USA')).toBeGreaterThan(0);
  }, LONG);
});
