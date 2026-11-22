// With the far side's switch off, no front keeps its own offensive (rider b646 to ruling 234, (3)): a staff remembers
// by enemy country as it always did, nothing is written on a front's ground, and nothing reads it. The grand wars'
// own pins hold the rest; this reads the staff's month directly.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { decide, fronts, offensiveWatch, ownFront, startOpen } from '../src/campaign/staff';
import { farOn } from '../src/campaign/world';

describe('the offensive held by front, far side off', () => {
  it('reads on no front and writes nothing', () => {
    const g = startOpen(createGame(3, null));
    expect(farOn(g)).toBe(false);
    const told: { own: boolean }[] = [];
    offensiveWatch.tell = (e) => { told.push(e); };
    try {
      for (g.turn = 1; g.turn <= 3; g.turn++) for (const nat of Object.keys(g.nations)) decide(g, nat);
    } finally { offensiveWatch.tell = null; }
    expect(told.length).toBeGreaterThan(0);
    expect(told.every((e) => !e.own)).toBe(true);
    for (const nat of Object.keys(g.nations)) for (const f of fronts(g, nat)) expect(ownFront(g, f)).toBe(false);
    expect(Object.keys(g.flags).filter((k) => /^st:f[ori]:/.test(k))).toEqual([]);
    // (Nor is the far-side fronts' own word by country: the design director's 11:40Z of 22 November 2026.)
    expect(Object.keys(g.flags).filter((k) => k.startsWith('st:on:') && k.endsWith(':far'))).toEqual([]);
    // Germany's offensive against Poland is remembered as it was.
    expect(g.flags['st:on:GER:POL']).toBeGreaterThan(0);
  });
});
