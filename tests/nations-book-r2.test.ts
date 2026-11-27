// The 3D table's list of countries on the charts of today (release 2, 24 November 2026): TC.nationsBook() with the
// far switch off. And the sea lookup on the Americas chart, which does not go round the world, moves no point.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { seaAt } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { WORLD_WIDTH } from '../src/campaign/wrap';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
interface Row { tag: string; name: string; adjective: string; colour: string; alignment: string }

describe('the nations book with the far switch off', () => {
  it('Europe alone, and Europe with the Americas', () => {
    const europe = TC().nationsBook() as Row[];
    const wide = TC().nationsBook(true) as Row[];
    expect(europe.map((n) => n.tag).sort()).toEqual(Object.keys(createGame(1, null).nations).sort());
    expect(wide.map((n) => n.tag).sort()).toEqual(Object.keys(startOpen(createGame(1, null)).nations).sort());
    expect(wide.length).toBeGreaterThan(europe.length);
    expect(wide.some((n) => n.tag === 'MEX')).toBe(true);
    expect(wide.some((n) => n.tag === 'JAP')).toBe(false);
    for (const n of wide) expect(n.colour, n.tag).toMatch(/^#[0-9a-fA-F]{6}$/);
    expect(europe.find((n) => n.tag === 'GER')).toMatchObject({ name: 'Germany', adjective: 'German', alignment: 'axis' });
  }, 300000);
});

describe('the sea under a point on the Americas chart', () => {
  it('a point a whole turn away is off the chart, as it always was', () => {
    const g = startOpen(createGame(1, null));
    const z = g.seas.find((s) => seaAt(s.x, s.y, g) >= 0)!;
    expect(seaAt(z.x + WORLD_WIDTH, z.y, g)).toBe(-1);
    expect(seaAt(z.x - WORLD_WIDTH, z.y, g)).toBe(-1);
  }, 300000);
});
