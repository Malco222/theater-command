// Japan's seat file with the far switch off (release 2, 24 November 2026): nothing of it is laid, so the goals of
// today's game stand as they were. America's long goal is Germany's fall alone, and there is no Japan to play.
import { describe, expect, it } from 'vitest';
import { FAR_LAID } from '../src/campaign/far-rows';
import { GOALS } from '../src/campaign/goals';

describe("Japan's seat file with the switch off", () => {
  it('is not laid: no goals for Japan, and America keeps Victory in Europe', () => {
    expect(FAR_LAID).toEqual([]);
    expect(GOALS.JAP).toBeUndefined();
    expect(GOALS.USA.long).toMatchObject({ title: 'Victory in Europe', beaten: ['GER'], by: 76 });
    expect(Object.keys(GOALS).sort()).toEqual(['FRA', 'GER', 'ITA', 'POL', 'SOV', 'UK', 'USA']);
  });
});
