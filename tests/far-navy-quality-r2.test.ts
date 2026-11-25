// Ship quality for the Pacific's two navies (the design director's B1 of 25 November 2026, 02:35Z, on the far side's
// audit FC-05): America 0.95 and Japan 1.15 (the historian's 1939 middles), laid by far-world.json over navy.json on the whole world only, in place of
// the 0.7 default neither navy had a row against. The G mark is gone: these are the historian's middles (japan-seed-checks-1125.md, section 2).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import far from '../data/campaign/far-world.json';
import { createGame } from '../src/campaign/game';
import { NAVY, quality } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';

describe('the Pacific navies\' ship quality', () => {
  it('is laid on the whole world only, from far-world.json', () => {
    expect((far as { navy: { quality: Record<string, number> } }).navy.quality).toEqual({ USA: 0.95, JAP: 1.15 });
    expect(NAVY.quality.USA).toBe(0.95);
    expect(NAVY.quality.JAP).toBe(1.15);
  });
  it('a campaign reads it, beside Britain\'s and Germany\'s', () => {
    const g = startOpen(createGame(3, null));
    expect([quality(g, 'USA'), quality(g, 'JAP'), quality(g, 'UK')]).toEqual([0.95, 1.15, 1.1]);
  });
});
