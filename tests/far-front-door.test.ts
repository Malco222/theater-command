// The 3D game's front door walked from end to end on the whole-world map (release 2, Japan and the Pacific, 25
// November 2026). No test had ever picked Japan through the calls the 3D game makes: the years, the countries, the
// start, a month played, the war saved, offered again and read back. The audit's second pass also found the later
// years a trap there: history up to them is played on Europe's chart and the far side laid over it as of September
// 1939, so January 1942 opened with America at peace with Japan and the Pacific Fleet at San Diego. On the whole
// world only the first day is offered (the owner, 25 November: 1939 first, a December 1941 start next).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import { saveChart } from '../src/campaign/save';
import { CHART_REGIONS, WORLD_FIRST } from '../src/campaign/world';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => any;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;

describe('the front door on the whole-world map, as the 3D game walks it', () => {
  it('offers September 1939 alone, and says so', () => {
    const s = TC().starts() as { years: { year: string; from: number }[]; only: string; groups: { from: number; sides: { nat: string }[] }[] };
    expect(s.years).toHaveLength(1);
    expect(s.years[0]).toMatchObject({ year: '1939', from: 1 });
    expect(s.only).toBe('On the whole-world map the war begins in September 1939.');
    // History's own battles are on Europe's map and are offered as ever.
    expect(s.groups).toHaveLength(14);
  }, 300000);

  it('Japan is picked, a month is played, and the war is saved, offered again and read back', () => {
    const door = TC().grand(1, 1939, true) as { chart: string; nations: { nat: string; major: boolean }[] };
    expect(door.chart).toBe('whole');
    expect(door.nations.find((n) => n.nat === 'JAP')).toMatchObject({ major: true });

    const v = TC().start({ human: 'JAP', view: 'grand', world: true }) as { turn: number; date: string; human: string; regions: unknown[]; fleets: { nat: string }[]; nations: Record<string, { name: string; atWar: string[] }> };
    expect(v).toMatchObject({ turn: 1, date: 'September 1939', human: 'JAP' });
    expect(v.nations.JAP).toMatchObject({ name: 'Japan', atWar: ['CHN'] });
    // The whole world is on the table: every province of it, and Japan's fleets and America's Pacific ones.
    expect(v.regions.length).toBe(WORLD_FIRST + CHART_REGIONS.whole);
    expect(v.fleets.filter((f) => f.nat === 'JAP').length).toBeGreaterThanOrEqual(9);
    expect(v.fleets.filter((f) => f.nat === 'USA').length).toBeGreaterThanOrEqual(4);

    // Japan's two goals stand open on the first day (ruling 196).
    const goals = TC().page('goals') as { headline: string; sections: { heading: string }[] };
    expect(goals.headline).toBe('2 campaigns to win');
    expect(goals.sections.map((x) => x.heading).join(' | ')).toMatch(/OIL FOR THE FLEET.*THE RING HOLDS/);

    const month = TC().endMonth() as { view: { turn: number; date: string; human: string } };
    expect(month.view).toMatchObject({ turn: 2, date: 'October 1939', human: 'JAP' });

    const text = TC().save().text as string;
    expect(saveChart(text)).toBe('whole');
    expect(TC().peekChart(text)).toEqual({ chart: 'whole', now: 'whole', fits: true, why: '' });
    expect(TC().peek(text)).toEqual({ human: 'JAP', name: 'Japan', date: 'October 1939', open: true });
    const back = TC().load(text) as { turn: number; human: string; regions: unknown[] } | null;
    expect(back).not.toBeNull();
    expect(back).toMatchObject({ turn: 2, human: 'JAP' });
    expect(back!.regions.length).toBe(WORLD_FIRST + CHART_REGIONS.whole);
  }, 900000);
});
