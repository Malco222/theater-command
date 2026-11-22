// The watch and the guard (strategy.json 'ops.watch', 'ops.home.held', 'ops.home.reach'; the design director's
// ruling of 22 November 2026, 19:36Z; after release 1; behind 'ops.on', which ships off). As shipped a staff watched
// only its homeland's harbours, up to a share of its army, and the guard an operation left on conquered ground was
// free to march away: Germany kept one corps of three divisions in a conquered Norway where the record has 6 to 14
// (norway-columns.md), and in one war the 18 divisions that took Norway all went east through Finland within two
// months. One rule for every side: every coast a side holds is watched, each country's coast by itself, a corps a
// harbour up to the sum of the sea lift of every enemy in reach; the guard's kept corps are held; and the guard reads
// the enemy the staff can see within a month's march.
//
// The rulings of 20:15Z and 20:18Z the same day: a side's own ground overseas is watched too, each piece a coast of
// its own, from the corps standing there; a corps the staff has only glimpsed counts as the player would know it, a
// corps and no more (the computer never knows more than the player); the keeper of a foothold's harbour stands for
// its month whatever a phase releases ('ops.port.held'); and the player's map reads the same coasts and the same
// figure of his own ground (`coastsRead`).

import { afterEach, describe, expect, it } from 'vitest';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { SIGHT, sightOf } from '../src/campaign/intel';
import { strikers } from '../src/campaign/marches';
import { coastReport, gatherings } from '../src/campaign/navy';
import { coastWatch, coastsRead, footholdPorts, opArmies, startOpen, type CoastRead } from '../src/campaign/staff';
import { STRATEGY } from '../src/campaign/strategy';
import { runTurnAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

const O = STRATEGY.ops!;
const shipped = { on: O.on, watch: O.watch, held: O.home!.held, reach: O.home!.reach, port: O.port!.held };
const reset = (): void => { O.on = shipped.on; O.watch = shipped.watch; O.home!.held = shipped.held; O.home!.reach = shipped.reach; O.port!.held = shipped.port; };

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
    expect(O.port!.held).toBe(true);
    const g = start();
    for (const r of norway(g)) r.ctrl = 'GER';
    alert(g, 'GER');
    const watch = coastWatch(g, 'GER');
    expect(Object.keys(watch).length).toBe(3);
    for (const name of Object.keys(watch)) expect(g.byName[name].owner).toBe('GER');
    expect(opArmies(g, 'GER')).toEqual([]);
    // And nobody's map reads a coast.
    expect(coastsRead(g, 'GER')).toEqual([]);
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

  it('on: one rule for every side, and a side\'s own ground overseas is watched, each piece a coast of its own', () => {
    O.on = true;
    const g = start();
    alert(g, 'UK');
    // Germany's lift is four corps a month: four of Britain's twelve harbours, where the share of the army gave three.
    // Its ships reach nothing of Britain's overseas.
    const watch = coastWatch(g, 'UK');
    expect(Object.keys(watch).length).toBe(g.nations.GER.sealift);
    for (const name of Object.keys(watch)) { expect(g.byName[name].owner).toBe('UK'); expect(g.byName[name].overseas).toBeFalsy(); }
    // Italy in the war: its lift of three reaches Egypt and Palestine, Cyprus, Gibraltar and Malta. Each piece of
    // Britain's ground overseas is a coast of its own, named by its most valuable harbour.
    declareWar(g, 'ITA', 'UK');
    const asked = (): Record<string, CoastRead> => Object.fromEntries(coastsRead(g, 'UK').filter((c) => c.ask > 0).map((c) => [c.id, c]));
    const read = asked();
    expect(Object.keys(read).sort()).toEqual(['UK', 'UK/Alexandria', 'UK/Cyprus', 'UK/Gibraltar', 'UK/Malta']);
    expect(read.UK.kind).toBe('home');
    expect(read['UK/Alexandria'].kind).toBe('overseas');
    expect(read['UK/Alexandria'].harbours.map((r) => r.name).sort()).toEqual(['Alexandria', 'Palestine', 'Port Said']);
    expect(read['UK/Alexandria'].lift).toEqual({ ITA: g.nations.ITA.sealift });
    expect(read['UK/Alexandria'].ask).toBe(3);
    expect(read['UK/Alexandria'].by.Alexandria).toEqual(['ITA']);
    // Never more than the harbours held: an island of one harbour asks one corps.
    expect(read['UK/Malta'].ask).toBe(1);
    // The homeland's watch is its own: Germany's four, as before.
    expect(read.UK.lift).toEqual({ GER: g.nations.GER.sealift });
    expect(read.UK.ask).toBe(4);
    // Harbours no enemy's ships reach are on no watch: the Caribbean, Newfoundland, the Falklands.
    expect(coastsRead(g, 'UK').filter((c) => c.ask === 0).map((c) => c.id)).toContain('UK/Jamaica');
    // The computer's watch is that reading: on each coast the harbours asked, the most valuable first.
    const both = coastWatch(g, 'UK');
    expect(Object.keys(both).sort()).toEqual(Object.values(read).flatMap((c) => c.reached.slice(0, c.ask).map((r) => r.name)).sort());
    expect(Object.keys(both).length).toBe(4 + 3 + 1 + 1 + 1);
    // Italy's side of it: Libya is one coast and Rhodes another.
    expect(coastsRead(g, 'ITA').filter((c) => c.kind === 'overseas').map((c) => [c.id, c.ask])).toEqual([['ITA/Rhodes', 1], ['ITA/Tripoli', 3]]);
    // The player's map reads the same coasts and the same figure of his own ground, and reading writes nothing.
    // (His staff is not the computer's: with a player in Britain's seat the computer's watch is the homeland's, as shipped.)
    g.human = 'UK';
    const flags = JSON.stringify(g.flags);
    expect(Object.fromEntries(Object.entries(asked()).map(([id, c]) => [id, c.ask]))).toEqual(Object.fromEntries(Object.entries(read).map(([id, c]) => [id, c.ask])));
    expect(JSON.stringify(g.flags)).toBe(flags);
    expect(Object.keys(coastWatch(g, 'UK')).length).toBe(3);
    g.human = null;
    // Its own key: off, the watch is the homeland's as shipped, and no coast is read.
    O.watch = false;
    expect(Object.keys(coastWatch(g, 'UK')).length).toBe(3);
    expect(coastsRead(g, 'UK')).toEqual([]);
  });

  it('on: shipping that gathers within reach of any coast a player holds is reported to him, and kept for his map', () => {
    // Britain's seat, Italy in the war: three Italian corps stand on the quay at Sicily with shipping for them. No
    // sea of theirs is within a landing's range of Britain's own shore: it is Malta they reach.
    const gathered = (): Game => {
      const g = start();
      declareWar(g, 'ITA', 'UK');
      g.human = 'UK';
      const inland = g.regions.find((r) => r.owner === 'ITA' && !r.overseas && !r.port)!;
      for (const r of g.regions) if (r.port && r.ctrl === 'ITA') for (const u of [...r.units]) if (u.nat === 'ITA' && u.kind !== 'fort') put(g, u, inland);
      for (const c of infantry(g, 'ITA').slice(0, 3)) put(g, c, g.byName.Sicily);
      return g;
    };
    const told = (g: Game): Game['headlines'] => g.headlines.filter((h) => h.warn === 'Coast watch');
    // As shipped the coast watch reads the homeland's shore alone, and he is told nothing.
    const off = gathered();
    coastReport(off, 'ITA');
    expect(told(off)).toEqual([]);
    expect(gatherings(off, 'UK')).toEqual([]);
    O.on = true;
    const g = gathered();
    coastReport(g, 'ITA');
    const w = told(g);
    expect(w.length).toBe(1);
    expect(w[0].nat).toBe('UK');
    expect(w[0].text).toBe('The coast watch reports Italian shipping gathering at Sicily, with troops on the quays: enough to carry 3 corps. Malta is within their reach, and a landing could come as soon as next month.');
    expect(w[0].region).toBe(g.byName.Malta.id);
    // His map has the quay for as long as the shipping stands there, and for the month after it was last seen.
    expect(gatherings(g, 'UK')).toEqual([{ quay: g.byName.Sicily.id, by: 'ITA', corps: 3, coast: g.byName.Malta.id }]);
    g.turn += 1;
    expect(gatherings(g, 'UK').length).toBe(1);
    g.turn += 1;
    expect(gatherings(g, 'UK')).toEqual([]);
    // Its own key: off, the homeland's shore alone.
    O.watch = false;
    const key = gathered();
    coastReport(key, 'ITA');
    expect(told(key)).toEqual([]);
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
    // Seen: two provinces from German ground is a glimpse, a flag and a count. The staff takes each for a corps of
    // its own army's mean worth: it knows no more of them than a player would.
    expect(sightOf(g, 'GER')[g.byName.Finnmark.id]).toBe(SIGHT.glimpse);
    const own = g.corps.filter((c) => c.nat === 'GER' && !c.dead && c.kind !== 'fort' && c.kind !== 'eng' && c.town === undefined);
    const foes = british.length * (worth(own) / own.length);
    expect(2 * foes).toBeGreaterThan(O.home!.guard);
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
    // A glimpse tells nothing of their state: worn to a tenth, they are still three corps to the staff.
    O.home!.reach = true;
    for (const c of british) c.str = 0.1;
    expect(kept()).toBe(withReach);
    // Next to the ground they are in full view and count at their worth, with the reach and without it: now the guard
    // is its six divisions, and more sail.
    for (const c of british) put(g, c, g.byName.Narvik);
    expect(sightOf(g, 'GER')[g.byName.Narvik.id]).toBe(SIGHT.full);
    expect(2 * worth(british)).toBeLessThan(O.home!.guard);
    const inView = kept();
    expect(inView).toBeLessThan(withReach);
    O.home!.reach = false;
    expect(kept()).toBe(inView);
  });

  it('on: the keeper of a foothold\'s harbour stands for its month, whatever a phase releases', () => {
    // Seed 1 at stop 3, the war of the harbour keeper's first trace. Germany lands at Oslo in May 1940 and Norway
    // fights on. While Oslo is a foothold's harbour (an enemy field corps within two marches), a German corps stands
    // in its field at every month's end.
    const emptied = (): number[] => {
      const g = startOpen(createGame(1, null));
      const out: number[] = [];
      while (g.turn <= 14) {
        runTurnAI(g);
        const oslo = g.byName.Oslo;
        if (oslo.ctrl !== 'GER' || !footholdPorts(g, 'GER').includes(oslo)) continue;
        if (!oslo.units.some((c) => c.nat === 'GER' && !c.dead && c.kind !== 'fort' && c.town === undefined)) out.push(g.turn - 1);
      }
      return out;
    };
    O.on = true;
    expect(emptied()).toEqual([]);
    // Its own key: off, as before, a phase's release let the keeper go in its own month. In September 1940 (month 13)
    // the corps that kept Oslo marched out with the rest and the harbour's field stood empty beside the enemy.
    O.port!.held = false;
    expect(emptied()).toEqual([13]);
  }, 240_000);
});
