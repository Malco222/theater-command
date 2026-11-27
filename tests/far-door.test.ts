// What the 3D game's country picker is told on the whole-world map (release 2, Japan and the Pacific, 25 November
// 2026). Japan stands among the great powers in 1939; a far seat brings its own words (a client's tie, a dominion's
// place); Tibet, whose ground no army enters, is nobody's seat; and a great power that cannot be led from the year
// chosen is named with the years it can be, so the door can grey it instead of dropping it.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => any;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
interface Seat { nat: string; name: string; major: boolean; small: boolean; blurb: string; door: string; corps: number; regions: number }

describe('the door on the whole-world map', () => {
  it('September 1939: Japan among the great powers, the far seats with their own words, no Tibet', () => {
    const d = TC().grand(1, 1939, true) as { chart: string; beyond: string; nations: Seat[]; absent: unknown[] };
    expect(d.chart).toBe('whole');
    expect(d.beyond).toBe('');
    expect(d.absent).toEqual([]);
    const jap = d.nations.find((n) => n.nat === 'JAP')!;
    expect(jap).toMatchObject({ name: 'Japan', major: true, small: false, door: '' });
    expect(jap.blurb).toMatch(/China/);
    expect(d.nations.filter((n) => n.major).map((n) => n.nat)).toContain('USA');
    expect(d.nations.some((n) => n.nat === 'TIB')).toBe(false);
    expect(d.nations).toHaveLength(64);
    expect(d.nations.find((n) => n.nat === 'MAN')!.door).toMatch(/^Japan's client\./);
    expect(d.nations.find((n) => n.nat === 'AUS')!.door).toMatch(/^A dominion/);
    expect(d.nations.find((n) => n.nat === 'CHN')!.door).toMatch(/At war with Japan from the first day/);
    // A seat of the old chart has no door words: its blurb says it all, as it always did.
    for (const nat of ['GER', 'UK', 'USA', 'SWE', 'BRA']) expect(d.nations.find((n) => n.nat === nat)!.door, nat).toBe('');
  }, 600000);

  it('January 1940: Japan and Poland are named as seats of another year, not dropped without a word', () => {
    const d = TC().grand(5, 1939, true) as { nations: Seat[]; absent: { nat: string; name: string; years: number[]; line: string }[] };
    expect(d.nations.some((n) => n.nat === 'JAP')).toBe(false);
    expect(d.absent.find((a) => a.nat === 'JAP')).toEqual({ nat: 'JAP', name: 'Japan', years: [1939], line: 'Japan can be led from 1939.' });
    expect(d.absent.find((a) => a.nat === 'POL')).toMatchObject({ years: [1939], line: 'Poland can be led from 1939.' });
    for (const a of d.absent) expect(d.nations.some((n) => n.nat === a.nat), a.nat).toBe(false);
    // A small seat of the far side is still offered, as every small country is in any year it stands.
    expect(d.nations.find((n) => n.nat === 'AUS')).toMatchObject({ small: true });
  }, 900000);
});
