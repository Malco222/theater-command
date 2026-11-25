// The far side's ship move: a month's sailing (the design director's rulings of 13:56Z and 18:23Z, 22 November 2026;
// the historian's paper troop-sailing-times.md).
//
// The fault it mends: a corps shipped from port to port went any distance in a month, so a German corps could go
// from Bordeaux to Nagoya in one. The record's troop convoys covered about one long ocean leg in a month.
//
// The rule: on the far chart a corps shipped from port to port crosses at most 'reach' seas in a month (navy.json
// 'sail'). The reach is the middle of the record's passages, each counted at its own pace for thirty days. A port
// further off is reached by ports its side or an ally holds, a month a leg: a computer staff's corps goes a leg and
// sails on next month, and the player ships his own leg by leg. The book's own dated sailings are the record's and
// are not held to it. Every state here is set by hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, allied, createGame, declareWar, sailOn, shipMove } from '../src/campaign/game';
import { NAVY, canShip, onOwnShipping, sailLeg, sailReach, seasBetween, whyNotShip } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

const grand = (seed = 3): Game => startOpen(createGame(seed, null));
const at = (g: Game, name: string): Region => g.byName[name];
/** A port put in a nation's hands by hand. */
const held = (g: Game, name: string, nat: string): Region => { const r = at(g, name); r.ctrl = nat; return r; };
/** A fresh corps of infantry in a port, with shipping to carry it. */
const corps = (g: Game, nat: string, place: string): Corps => {
  g.nations[nat].sealift = Math.max(g.nations[nat].sealift, 4);
  const c = addCorps(g, nat, `Test Corps at ${place}`, 'inf', 1, place, {});
  c.ap = c.apMax;
  return c;
};
/** The fewest seas between two ports by the chart alone: no enemy and no shut strait. */
const chart = (g: Game, a: string, b: string): number => {
  const d = new Map<number, number>(), q: number[] = [];
  for (const z of at(g, a).seas) { d.set(z, 1); q.push(z); }
  for (let i = 0; i < q.length; i++) for (const j of g.seas[q[i]].adj) if (!d.has(j)) { d.set(j, d.get(q[i])! + 1); q.push(j); }
  return Math.min(...at(g, b).seas.map((z) => d.get(z) ?? Infinity));
};
/** The next month, by hand: the corps' moves whole again and the month's shipping free. */
const nextMonth = (g: Game, c: Corps): void => { g.turn++; c.ap = c.apMax; };
/** The Mediterranean shut to Britain's ships by hand: Gibraltar in the hands of a country at war with it. */
const shutGibraltar = (g: Game): void => {
  const gate = g.gates.find((x) => /Gibraltar/.test(x.name))!;
  if (!g.nations.UK.atWar.includes('GER')) declareWar(g, 'GER', 'UK');
  g.regions[gate.region].ctrl = 'GER';
};

describe('the far side: a month\'s sailing for a corps shipped from port to port', () => {
  it('the reach is the middle of the record\'s passages, each counted at its own pace for thirty days', () => {
    const g = grand();
    const S = NAVY.sail!;
    const pace = S.passages.map((p) => {
      let seas = 0;
      for (let i = 1; i < p.ports.length; i++) seas += chart(g, p.ports[i - 1], p.ports[i]);
      return (seas * 30) / ((p.days[0] + p.days[1]) / 2);
    });
    expect(pace.map((x) => Math.round(x * 10) / 10)).toEqual([8.1, 6.5, 10.9, 9.5, 9.5]);
    const middle = [...pace].sort((a, b) => a - b)[2];
    expect(Math.round(middle * 10) / 10).toBe(9.5);
    // A sea is crossed whole.
    expect(S.reach).toBe(Math.floor(middle));
    expect(sailReach(g)).toBe(9);
  });

  it('San Francisco to Australia, and to Manila, is one month', () => {
    const g = grand();
    const c = corps(g, 'USA', 'San Francisco');
    const brisbane = held(g, 'Brisbane', 'USA'), manila = held(g, 'Manila', 'USA');
    expect(seasBetween(g, 'USA', at(g, 'San Francisco'), brisbane)).toBe(5);
    expect(seasBetween(g, 'USA', at(g, 'San Francisco'), manila)).toBe(9);
    expect(whyNotShip(g, c, manila)).toBeNull();
    expect(whyNotShip(g, c, brisbane)).toBeNull();
    expect(shipMove(g, c, brisbane)).toBe(true);
    expect(c.reg).toBe(brisbane.id);
    expect(c.sail).toBeUndefined();
  });

  it('the Clyde to Cape Town is one month, and to the Red Sea round the Cape two: a staff\'s corps sails on a leg a month', () => {
    const g = grand();
    shutGibraltar(g);
    const c = corps(g, 'UK', 'Clydeside');
    const clyde = at(g, 'Clydeside'), cape = held(g, 'Cape Town', 'UK'), sudan = held(g, 'Port Sudan', 'UK');
    expect(seasBetween(g, 'UK', clyde, cape)).toBe(8);
    expect(canShip(g, c, cape)).toBe(true);
    // Round the Cape the Red Sea is further off than a month's sailing.
    const off = seasBetween(g, 'UK', clyde, sudan);
    expect(off).toBe(13);
    expect(canShip(g, c, sudan)).toBe(false);
    const leg = sailLeg(g, c, sudan)!;
    expect(leg.port && allied(g, 'UK', leg.ctrl)).toBe(true);
    expect(seasBetween(g, 'UK', clyde, leg)).toBeLessThanOrEqual(9);
    expect(seasBetween(g, 'UK', leg, sudan)).toBeLessThan(off);
    expect(seasBetween(g, 'UK', leg, sudan)).toBeLessThanOrEqual(9);
    expect(whyNotShip(g, c, sudan)).toBe(`Port Sudan is 13 seas away, and a troop convoy sails 9 seas in a month. Ship them to ${leg.name} first, and on from there next month.`);
    // A computer staff's corps goes the leg, and the order stands.
    expect(shipMove(g, c, sudan)).toBe(false);
    expect(c.reg).toBe(leg.id);
    expect(c.sail).toBe(sudan.id);
    expect(c.ap).toBe(0);
    // Next month it sails on before its staff moves anyone, and is there: two months in all.
    nextMonth(g, c);
    sailOn(g, 'UK');
    expect(c.reg).toBe(sudan.id);
    expect(c.sail).toBeUndefined();
  });

  it('the player ships his own corps leg by leg: an order beyond a month\'s sailing is refused and nothing moves', () => {
    const g = grand();
    shutGibraltar(g);
    g.human = 'UK';
    const c = corps(g, 'UK', 'Clydeside');
    held(g, 'Cape Town', 'UK');
    const sudan = held(g, 'Port Sudan', 'UK');
    expect(shipMove(g, c, sudan)).toBe(false);
    expect(c.reg).toBe(at(g, 'Clydeside').id);
    expect(c.sail).toBeUndefined();
    expect(c.ap).toBe(c.apMax);
  });

  it('an order that cannot go on lapses, and the corps is its staff\'s where it stands', () => {
    const g = grand();
    shutGibraltar(g);
    const c = corps(g, 'UK', 'Clydeside');
    held(g, 'Cape Town', 'UK');
    const sudan = held(g, 'Port Sudan', 'UK');
    shipMove(g, c, sudan);
    const leg = c.reg;
    expect(c.sail).toBe(sudan.id);
    // The port it was bound for falls before it sails on.
    sudan.ctrl = 'GER';
    nextMonth(g, c);
    sailOn(g, 'UK');
    expect(c.reg).toBe(leg);
    expect(c.sail).toBeUndefined();
  });

  it('Bordeaux to Nagoya is shut: twelve seas, and no port of its side on the way', () => {
    const g = grand();
    held(g, 'Bordeaux', 'GER');
    const c = corps(g, 'GER', 'Bordeaux');
    const nagoya = held(g, 'Nagoya', 'GER');
    expect(chart(g, 'Bordeaux', 'Nagoya')).toBe(12);
    expect(canShip(g, c, nagoya)).toBe(false);
    expect(sailLeg(g, c, nagoya)).toBeNull();
    expect(shipMove(g, c, nagoya)).toBe(false);
    expect(c.reg).toBe(at(g, 'Bordeaux').id);
    expect(c.sail).toBeUndefined();
  });

  it('the book\'s own dated sailings are the record\'s: they are not held to the month\'s reach', () => {
    const g = grand();
    shutGibraltar(g);
    const c = corps(g, 'UK', 'Clydeside');
    const sudan = held(g, 'Port Sudan', 'UK');
    expect(whyNotShip(g, c, sudan)).not.toBeNull();
    expect(onOwnShipping('UK', () => sailReach(g, 'UK'))).toBe(0);
    expect(onOwnShipping('UK', () => whyNotShip(g, c, sudan))).toBeNull();
  });
});
