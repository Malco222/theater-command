// The city at a glance, release 2 (Brian, 23 November 2026 22:23Z: "When you click on a city, it should be very easy to
// see what's going on with it as well as the troops, Garrison there what anti-aircraft capabilities they have", and an
// "easier user interface of what units that city can build"): the AIR DEFENCE and CAN BUILD HERE shelves, and what each
// building gives, as the bridge hands them to the 3D game's city panel.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;

interface Air { level: number; heavy: number; light: number; stop: number; going: number; fighters: number; watch: boolean; foe: string; sky: string; add: { key: string; cost: number; months: number; why: string } | null }
interface Can { key: string; group: string; sign: string; label: string; size: string; cost: number; months: number; why: string }
interface Glance { own: boolean; air: Air | null; can: Can[]; tiles: { kind: string; sign: string; does: string }[]; orders: { name: string }[] }

function germany(): Game {
  const g = startOpen(createGame(3, 'GER'));
  let guard = 0;
  while (!humanUp(g) && guard++ < 200) stepAI(g);
  beginHuman(g);
  return g;
}
const glanceOf = (g: Game, name: string): Glance => (TC().province(g.byName[name].id) as { glance: Glance }).glance;

describe('the city at a glance: air defence, what it can build, what its buildings give', () => {
  it('his own town shows its guns, the raid share they stop, the fighters over it and the next level with its price', () => {
    const g = germany();
    TC().load(saveGame(g));
    const berlin = glanceOf(g, 'Berlin');
    expect(berlin.own).toBe(true);
    const air = berlin.air!;
    expect(air).not.toBeNull();
    expect(air.watch).toBe(true);
    expect(air.light).toBeGreaterThan(0);
    expect(air.stop).toBe(air.level * 15);
    expect(air.foe).not.toBe('');
    expect(air.fighters).toBeGreaterThanOrEqual(0);
    expect(air.add?.key).toBe('work:aa');
    expect(air.add!.cost).toBeGreaterThan(0);
  });

  it('lists the orders that would be raised or built in this very town, each with its sign, price and months', () => {
    const g = germany();
    TC().load(saveGame(g));
    const hamburg = glanceOf(g, 'Hamburg');
    expect(hamburg.can.length).toBeGreaterThan(0);
    expect(hamburg.can.every((c) => c.key !== 'work:aa' && c.sign !== '' && c.cost >= 0 && c.months >= 0)).toBe(true);
    // Only what can be ordered here now, or would be with the money: no carrier before its research, no second yard.
    expect(hamburg.can.some((c) => c.key === 'ship:cv' || c.key === 'work:yard' || c.key === 'elite')).toBe(false);
    expect(glanceOf(g, 'Berlin').can.some((c) => c.group === 'navy' || c.key === 'work:yard')).toBe(false);
    // A shipyard town lays down ships; the guns are on their own shelf.
    expect(hamburg.can.some((c) => c.group === 'navy')).toBe(true);
    const dd = hamburg.can.find((c) => c.key === 'ship:dd');
    if (dd) expect(dd.sign).toBe('hull');
    // One press orders it here, and it is then on order here.
    const free = hamburg.can.find((c) => c.why === '');
    expect(free).toBeDefined();
    const out = TC().order(free!.key, g.byName.Hamburg.id) as { ok: boolean };
    expect(out.ok).toBe(true);
    const after = glanceOf(g, 'Hamburg');
    expect(after.orders.length + after.tiles.filter((t) => t.kind === 'up').length).toBeGreaterThan(hamburg.orders.length + hamburg.tiles.filter((t) => t.kind === 'up').length);
  });

  it('a building that stands says what it gives, and wears its own line\'s sign', () => {
    const g = germany();
    const berlin = g.byName.Berlin.id;
    g.flags['cv:any'] = 1;
    g.flags[`cv:market:${berlin}`] = 1;
    g.flags[`cv:hospital:${berlin}`] = 2;
    TC().load(saveGame(g));
    const any = glanceOf(g, 'Berlin').tiles.filter((t) => t.kind === 'stands');
    expect(any.map((t) => t.sign).sort()).toEqual(['civic-hospital', 'civic-market']);
    for (const t of any) {
      expect(t.sign).toMatch(/^civic-(market|farm|school|camp|hospital|hall)$/);
      expect(t.does.length).toBeGreaterThan(0);
    }
  });

  it('an enemy town shows no fighters and offers nothing', () => {
    const g = germany();
    TC().load(saveGame(g));
    const paris = glanceOf(g, 'Paris');
    if (!paris) return;
    expect(paris.can).toEqual([]);
    if (paris.air) expect(paris.air.watch).toBe(false);
  });
});
