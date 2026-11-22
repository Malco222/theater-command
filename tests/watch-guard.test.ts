// The watch and the guard (strategy.json 'ops.watch', 'ops.home.held', 'ops.home.reach'; the design director's
// ruling of 22 November 2026, 19:36Z; after release 1; behind 'ops.on', which ships off). As shipped a staff watched
// only its homeland's harbours, up to a share of its army, and the guard an operation left on conquered ground was
// free to march away: Germany kept one corps of three divisions in a conquered Norway where the record has 6 to 14
// (norway-columns.md), and in one war the 18 divisions that took Norway all went east through Finland within two
// months. One rule for every side: every coast a side holds is watched, each country's coast by itself, a corps a
// harbour up to the sum of the sea lift of every enemy in reach; the guard's kept corps are held; and the guard reads
// the enemy the staff can see within a month's march.

import { afterEach, describe, expect, it } from 'vitest';
import { atWar, createGame } from '../src/campaign/game';
import { sightOf } from '../src/campaign/intel';
import { strikers } from '../src/campaign/marches';
import { coastWatch, opArmies, startOpen } from '../src/campaign/staff';
import { STRATEGY } from '../src/campaign/strategy';
import { runTurnAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

const O = STRATEGY.ops!;
const shipped = { on: O.on, watch: O.watch, held: O.home!.held, reach: O.home!.reach };
const reset = (): void => { O.on = shipped.on; O.watch = shipped.watch; O.home!.held = shipped.held; O.home!.reach = shipped.reach; };

/** November 1939 in the grand campaign, the usual game: Germany at war with Britain, France and Canada. */
const start = (): Game => {
  const g = startOpen(createGame(21, null));
  for (let t = 0; t < 2; t++) runTurnAI(g);
  return g;
};
const norway = (g: Game): Region[] => g.regions.filter((r) => r.owner === 'NOR');
const harbour = (r: Region): boolean => !!r.port && r.seas.length > 0;
/** The watch, counted by the country whose coast each harbour is. */
const byCountry = (g: Game, nat: string): Record<string, number> => {
  const out: Record<string, number> = {};
  for (const [name, k] of Object.entries(coastWatch(g, nat))) out[g.byName[name].owner] = (out[g.byName[name].owner] ?? 0) + k;
  return out;
};
/** The coast watch is in force (a staff mans its harbours from a season's quiet, and for a year after). */
const alert = (g: Game, nat: string): void => { g.flags[`st:watch:${nat}`] = g.turn; };
const put = (g: Game, c: Corps, r: Region): void => {
  g.regions[c.reg].units = g.regions[c.reg].units.filter((u) => u !== c);
  r.units.push(c);
  c.reg = r.id;
};
const infantry = (g: Game, nat: string): Corps[] => g.corps.filter((c) => c.nat === nat && !c.dead && c.kind === 'inf' && !c.hold && !c.town && c.joint === undefined).sort((a, b) => a.id - b.id);

/** Norway has fallen to a German operation that came ashore at Oslo, and six German corps stand on its ground. */
const conquered = (g: Game): Corps[] => {
  for (const r of norway(g)) {
    r.ctrl = 'GER';
    for (const u of r.units) if (u.nat === 'NOR') u.dead = true;
    r.units = r.units.filter((u) => u.nat !== 'NOR');
  }
  g.nations.NOR.alive = false;
  g.flags['st:lodged:GER'] = g.byName.Oslo.id + 1;
  g.flags['st:lodgedOp:GER'] = 1;
  const six = infantry(g, 'GER').slice(0, 6);
  ['Oslo', 'Oslo', 'Oslo', 'Bergen', 'Oppland', 'Oppland'].forEach((name, k) => put(g, six[k], g.byName[name]));
  // (At full strength, so that the month's replacements change nobody's place among the weakest.)
  for (const c of six) c.str = 1;
  return six;
};
const worth = (cs: Corps[]): number => cs.reduce((s, c) => s + c.divs * c.str, 0);

describe('the watch and the guard', () => {
  afterEach(reset);

  it('ships behind the operations\' switch, which is off: the watch is the homeland\'s, under its share of the army', () => {
    expect(O.on).toBe(false);
    expect(O.watch).toBe(true);
    expect(O.home!.held).toBe(true);
    expect(O.home!.reach).toBe(true);
    const g = start();
    for (const r of norway(g)) r.ctrl = 'GER';
    alert(g, 'GER');
    const watch = coastWatch(g, 'GER');
    expect(Object.keys(watch).length).toBe(3);
    for (const name of Object.keys(watch)) expect(g.byName[name].owner).toBe('GER');
    expect(opArmies(g, 'GER')).toEqual([]);
  });

  it('on: every coast a side holds is watched, each country\'s by itself, up to the sum of the lifts in reach and never more than the harbours held', () => {
    O.on = true;
    const g = start();
    alert(g, 'GER');
    // Britain's ships reach Germany's coast and Norway's; Canada's sail from nowhere on the map yet. So what could land is Britain's lift.
    const lift = g.nations.UK.sealift;
    expect(lift).toBe(4);
    const home = byCountry(g, 'GER');
    expect(home.GER).toBe(lift);
    expect(home.NOR).toBeUndefined();
    // While Norway still fights, the harbours taken from it are a front's: an operation's force is the operation's until its task ends.
    for (const r of norway(g)) r.ctrl = 'GER';
    expect(norway(g).filter(harbour).length).toBe(6);
    expect(byCountry(g, 'GER').NOR).toBeUndefined();
    // Norway conquered: its six harbours are a watch of their own, and the homeland's is not thinned for it.
    g.nations.NOR.alive = false;
    const both = byCountry(g, 'GER');
    expect(both.GER).toBe(lift);
    expect(both.NOR).toBe(lift);
    // The most valuable harbours first.
    expect(Object.keys(coastWatch(g, 'GER')).filter((name) => g.byName[name].owner === 'NOR').sort()).toEqual(['Bergen', 'Narvik', 'Oslo', 'Trondheim']);
    // No share of the army is set over it: as shipped the whole watch was three corps of Germany's 28.
    expect(Object.values(both).reduce((s, k) => s + k, 0)).toBeGreaterThan(3);
    // The sum of the lifts, not a number of ours: a ship more, a corps more on each coast it reaches.
    g.nations.UK.sealift = lift + 1;
    expect(byCountry(g, 'GER').NOR).toBe(lift + 1);
    g.nations.UK.sealift = 99;
    expect(byCountry(g, 'GER').NOR).toBe(6);
    g.nations.UK.sealift = lift;
    // Never more than the harbours held.
    for (const r of norway(g)) if (r.name !== 'Oslo' && r.name !== 'Bergen') r.ctrl = 'UK';
    expect(byCountry(g, 'GER').NOR).toBe(2);
    // Its own key: off, the watch is the homeland's as shipped.
    O.watch = false;
    expect(byCountry(g, 'GER')).toEqual({ GER: 3 });
  });

  it('on: one rule for every side, and a side\'s own harbours overseas are not watched', () => {
    O.on = true;
    const g = start();
    alert(g, 'UK');
    const watch = coastWatch(g, 'UK');
    // Germany's lift is four corps a month: four of Britain's twelve harbours, where the share of the army gave three.
    expect(Object.keys(watch).length).toBe(g.nations.GER.sealift);
    for (const name of Object.keys(watch)) { expect(g.byName[name].owner).toBe('UK'); expect(g.byName[name].overseas).toBeFalsy(); }
    O.watch = false;
    expect(Object.keys(coastWatch(g, 'UK')).length).toBe(3);
  });

  it('on: the guard counts the watch\'s corps as kept and sends none of them home', () => {
    O.on = true;
    const g = start();
    alert(g, 'GER');
    const six = conquered(g);
    const asked = Object.keys(coastWatch(g, 'GER')).filter((name) => g.byName[name].owner === 'NOR');
    expect(asked.length).toBe(4);
    const [A] = opArmies(g, 'GER');
    // Four corps for the four harbours asked, the ones standing in Oslo and Bergen first: ten divisions' worth, over the guard's six, so the other two sail.
    expect(A.go.length).toBe(2);
    expect(A.keep ?? []).toEqual([]);
    const posts = six.filter((c) => !A.go.includes(c));
    expect(posts.some((c) => g.regions[c.reg].name === 'Oslo')).toBe(true);
    expect(posts.some((c) => g.regions[c.reg].name === 'Bergen')).toBe(true);
    expect(worth(posts)).toBeGreaterThanOrEqual(O.home!.guard);
    // Without the watch the guard keeps its six divisions' worth of the weakest, and three corps sail.
    O.watch = false;
    const [B] = opArmies(g, 'GER');
    expect(B.go.length).toBe(3);
    expect(B.keep!.length).toBe(3);
    expect(worth(B.keep!)).toBeGreaterThanOrEqual(O.home!.guard);
    expect(worth(B.keep!.slice(0, -1))).toBeLessThan(O.home!.guard);
  });

  it('on: the guard\'s kept corps are held for the month and stand where they stood', () => {
    O.on = true;
    O.watch = false;
    const g = start();
    conquered(g);
    const [A] = opArmies(g, 'GER');
    const keep = A.keep!, was = keep.map((c) => c.reg);
    expect(keep.length).toBeGreaterThan(0);
    const go = [...A.go];
    runTurnAI(g);
    expect(keep.map((c) => c.reg)).toEqual(was);
    // The others took ship for home or marched to the harbour to wait for one.
    for (const c of go) expect(g.regions[c.reg].owner === 'GER' || g.regions[c.reg].name === 'Oslo').toBe(true);
    // The hold is the month's: nobody is left held after it.
    for (const c of keep) expect(c.hold).toBeFalsy();
    // Its own key: off, the guard only chooses who sails, as before.
    O.home!.held = false;
    expect(opArmies(g, 'GER').every((x) => x.keep === undefined)).toBe(true);
  });

  it('on: the guard reads the enemy the staff can see within a month\'s march, not only the enemy next to its ground', () => {
    O.on = true;
    O.watch = false;
    const g = start();
    const six = conquered(g);
    // The British hold the far north: Narvik, next to the German ground, and Finnmark beyond it, where three corps stand.
    g.byName.Narvik.ctrl = 'UK';
    g.byName.Finnmark.ctrl = 'UK';
    const british = infantry(g, 'UK').slice(0, 3);
    for (const c of british) put(g, c, g.byName.Finnmark);
    const foes = worth(british);
    expect(2 * foes).toBeGreaterThan(O.home!.guard);
    // Seen: two provinces from German ground is a glimpse.
    expect(sightOf(g, 'GER')[g.byName.Finnmark.id]).toBeGreaterThan(0);
    const ground = new Set(['Oslo', 'Sorlandet', 'Bergen', 'Oppland', 'Trondheim', 'Nordland'].map((name) => g.byName[name].id));
    const moves = british.map((c) => c.ap);
    const near = strikers(g, 'GER', (id) => ground.has(id));
    // The reading leaves the game as it was.
    expect(british.map((c) => c.ap)).toEqual(moves);
    expect(british.every((c) => near.includes(c))).toBe(true);
    for (const c of near) expect(atWar(g, c.nat, 'GER')).toBe(true);
    // A month's march and no more: the British army at home is not counted.
    expect(near.some((c) => g.regions[c.reg].owner === 'UK')).toBe(false);
    // Twice that enemy is asked for: more than the six divisions, so fewer sail (or none).
    const kept = (): number => { const [A] = opArmies(g, 'GER'); return worth(six.filter((c) => !(A?.go ?? []).includes(c))); };
    const withReach = kept();
    expect(withReach).toBeGreaterThanOrEqual(Math.min(2 * foes, worth(six)));
    // As before, off: the enemy two provinces away is not on the ground or next to it, and the guard is six divisions.
    O.home!.reach = false;
    const without = kept();
    expect(without).toBeLessThan(withReach);
    // Next to the ground both read him.
    for (const c of british) put(g, c, g.byName.Narvik);
    expect(kept()).toBe(withReach);
  });
});
