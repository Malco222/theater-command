// What the 3D game's country picker is told with the whole world off (release 2, 25 November 2026): today's seats,
// each as it was, and one line more for the door, which answers the owner's question of 24 November ("why can't you
// play as Japan?"): Japan is on the whole-world map.
import { describe, expect, it } from 'vitest';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => any;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
interface Seat { nat: string; name: string; major: boolean; small: boolean; door: string }

describe('the door with the whole world off', () => {
  it('Europe and the Americas: the 51 seats, no Japan, and the line that says where Japan is', () => {
    const d = TC().grand(1, 1939, true) as { chart: string; beyond: string; nations: Seat[]; absent: unknown[] };
    expect(d.chart).toBe('americas');
    expect(d.beyond).toBe('Japan, China and the Pacific are on the whole-world map. Switch the whole world on under OPTIONS to lead them.');
    expect(d.nations).toHaveLength(51);
    expect(d.nations.some((n) => n.nat === 'JAP')).toBe(false);
    expect(d.nations.every((n) => n.door === '')).toBe(true);
    expect(d.absent).toEqual([]);
  }, 600000);

  it('Europe alone: no line about another map', () => {
    const d = TC().grand(1, 1939) as { chart: string; beyond: string; nations: Seat[] };
    expect(d.chart).toBe('europe');
    expect(d.beyond).toBe('');
    expect(d.nations.some((n) => n.nat === 'GER' && n.major)).toBe(true);
  }, 600000);

  it('every start year is offered, as ever', () => {
    const s = TC().starts() as { years: { year: string; from: number }[]; only: string };
    expect(s.years.map((y) => [y.year, y.from])).toEqual([['1939', 1], ['1940', 5], ['1941', 17], ['1942', 29]]);
    expect(s.only).toBe('');
  }, 300000);

  it('January 1940: Poland is a seat of 1939, and says so', () => {
    const d = TC().grand(5, 1939, true) as { nations: Seat[]; absent: { nat: string; line: string }[] };
    expect(d.absent).toEqual([{ nat: 'POL', name: 'Poland', years: [1939], line: 'Poland can be led from 1939.' }]);
  }, 900000);
});
