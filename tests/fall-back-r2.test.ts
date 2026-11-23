// A full rear is not an encirclement (Units decide, audit #7; the design director's ruling of 23 November 2026 10:20Z).
// Today a corps beaten in the field with no ground of its side next to it that has room (game.ts retreatTarget skips a
// neighbour holding the stack and two) lays down its arms as if surrounded: at Kursk in July 1943 all seven Soviet armies
// were taken because Voronezh behind them was full, and about a quarter of the grand campaign's field prisoners are
// taken that way. With Units decide on, the beaten corps falls back through the full ground to the next with room, as
// one retreat; failing that, it stands in the full ground over the stack for the month. Only with no ground of its
// side next to the field at all is it taken whole (ruling 232). Off, nothing changes.
import { describe, expect, it } from 'vitest';
import { addCorps, capture, createGame, placed, relocate, removeCorps, resolveBattle } from '../src/campaign/game';
import { garrisonOf } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { STACK, type Corps, type Game, type Region } from '../src/campaign/types';

/** Hands a province to a nation for a test: whoever stood in it is taken off the map. */
function hand(g: Game, r: Region, to: string): void {
  for (const u of [...r.units, ...garrisonOf(r)]) removeCorps(g, u);
  capture(g, r, to);
}

interface Pocket { g: Game; target: Region; rear: Region; beyond: Region[]; def: Corps[]; att: Corps[] }
/**
 * September 1939: a Polish stack at Lodz with German infantry on every side but one. The one side, the Polish rear, is
 * full of Polish corps; the ground beyond it is Polish and empty unless 'beyond' says otherwise.
 */
function pocket(units: boolean, beyond: 'room' | 'full' | 'enemy'): Pocket {
  const g = startOpen(createGame(3, null));
  if (units) g.flags.unitsDecide = 1;
  const target = g.byName['Lodz'];
  const nb = target.nb.map((j) => g.regions[j]);
  const from = nb.find((r) => r.ctrl === 'GER')!;
  // The rear: the Polish neighbour with the most Polish ground behind it.
  const polish = (r: Region): boolean => r.ctrl === 'POL' && r.owner === 'POL';
  const rear = nb.filter((r) => polish(r) && r.id !== from.id).sort((a, b) => b.nb.filter((k) => polish(g.regions[k])).length - a.nb.filter((k) => polish(g.regions[k])).length)[0];
  for (const r of nb) if (r.id !== rear.id && r.id !== from.id) hand(g, r, 'GER');
  for (const u of [...target.units]) removeCorps(g, u);
  for (const u of [...from.units]) if (u.nat === 'GER' && u.kind !== 'fort') relocate(g, u, g.byName['Berlin']);
  const def = Array.from({ length: 2 }, (_, i) => addCorps(g, 'POL', `Line ${i + 1}`, 'inf', 3, target.id, {}));
  const att = Array.from({ length: 4 }, (_, i) => addCorps(g, 'GER', `Storm ${i + 1}`, 'inf', 3, from.id, {}));
  for (const c of [...def, ...att]) { c.supplied = true; c.ap = c.apMax; }
  const fill = (r: Region, tag: string): void => { for (let k = placed(g, r, 'POL').length; k < STACK + 2; k++) addCorps(g, 'POL', `${tag} ${k + 1}`, 'inf', 2, r.id, {}); };
  for (const u of [...rear.units]) removeCorps(g, u);
  fill(rear, 'Reserve');
  const others = rear.nb.map((j) => g.regions[j]).filter((r) => r.id !== target.id);
  for (const r of others) {
    if (beyond === 'enemy' || !polish(r)) { if (r.ctrl !== 'GER') hand(g, r, 'GER'); continue; }
    for (const u of [...r.units]) removeCorps(g, u);
    if (beyond === 'full') fill(r, `Depot ${r.name}`);
  }
  return { g, target, rear, beyond: others.filter((r) => r.ctrl === 'POL'), def, att };
}

const fight = (p: Pocket) => resolveBattle(p.g, p.att, p.target, { fought: 3 });

describe('a beaten corps whose rear is full', () => {
  it('off: is taken whole, as today', () => {
    const p = pocket(false, 'room');
    const rep = fight(p);
    expect(rep.taken).toBe(true);
    expect(rep.defenders.filter((u) => !u.guard).map((u) => u.fate)).toEqual(['surrendered', 'surrendered']);
    expect(rep.prisoners).toBeGreaterThan(0);
  });

  it('on: falls back through the full ground to the next ground of its side with room', () => {
    const p = pocket(true, 'room');
    expect(p.beyond.length).toBeGreaterThan(0);
    const rep = fight(p);
    expect(rep.taken).toBe(true);
    expect(rep.defenders.filter((u) => !u.guard).map((u) => u.fate)).toEqual(['retreated', 'retreated']);
    for (const c of p.def) {
      expect(c.dead).toBeFalsy();
      expect(p.beyond.map((r) => r.id)).toContain(c.reg);
      expect(c.ap).toBe(0);
    }
  });

  it('on: with no room beyond either, stands in the full ground over the stack for the month', () => {
    const p = pocket(true, 'full');
    const rep = fight(p);
    expect(rep.defenders.filter((u) => !u.guard).map((u) => u.fate)).toEqual(['retreated', 'retreated']);
    for (const c of p.def) expect(c.reg).toBe(p.rear.id);
    expect(placed(p.g, p.rear, 'POL').length).toBe(STACK + 4);
  });

  it('on: with the ground beyond the enemy\'s, still falls back into the full rear: its side holds it', () => {
    const p = pocket(true, 'enemy');
    const rep = fight(p);
    expect(rep.defenders.filter((u) => !u.guard).map((u) => u.fate)).toEqual(['retreated', 'retreated']);
    for (const c of p.def) expect(c.reg).toBe(p.rear.id);
  });

  // Ruling 232 stays as it is, on and off: with no ground of its side next to the field, the fall back has nowhere to go.
  for (const on of [false, true]) {
    it(`${on ? 'on' : 'off'}: with no ground of its side next to the field at all, is taken whole (ruling 232)`, () => {
      const p = pocket(on, 'room');
      hand(p.g, p.rear, 'GER');
      expect(p.target.nb.every((j) => p.g.regions[j].ctrl === 'GER')).toBe(true);
      const rep = fight(p);
      expect(rep.taken).toBe(true);
      expect(rep.defenders.filter((u) => !u.guard).map((u) => u.fate)).toEqual(['surrendered', 'surrendered']);
      for (const c of p.def) expect(c.dead).toBe(true);
      expect(rep.prisoners).toBeGreaterThan(0);
    });
  }
});
