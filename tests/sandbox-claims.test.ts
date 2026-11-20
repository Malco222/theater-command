// The sandbox's lock sweep, finding 4 (after release 1): where anything goes (stop 5; history.json claims 'inBloc') a
// great power may march for a claim on a country that has taken a side, so long as it is not a member of its own
// alliance. At stop 4 it still marches only on a country outside both alliances (tests/history-dial.test.ts).
import { describe, expect, it } from 'vitest';
import { atWar, bloc, createGame, setWar } from '../src/campaign/game';
import { claimsMonth, moment, readable } from '../src/campaign/claims';
import { HISTORY, setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const grand = (seed: number, stop: number): Game => { const g = startOpen(createGame(seed, null)); if (stop !== 3) setStop(g, stop, true); return g; };
const amb = (id: string) => HISTORY.ambitions.find((a) => a.id === id)!;
/** A war where anything goes in which the Soviet claim on Kars is alive and its moment has come by month 40. */
const karsWar = (): Game => {
  const a = amb('kars');
  for (let s = 1; s < 60; s++) { const x = grand(s, 5); x.turn = 40; if (readable(x, a) && moment(x, a)) return x; }
  throw new Error('no war with the claim on Kars alive');
};

describe('a great power\'s claim on a country that has taken a side (stop 5)', () => {
  it('is named in the data, for stop 5 alone', () => {
    expect(HISTORY.claims.inBloc).toEqual([5]);
  });

  it('may be marched for where anything goes, and not at stop 4', () => {
    const a = amb('kars');
    const g = karsWar();
    g.nations.TUR.align = 'allies_cobelligerent';
    expect(bloc(g.nations.TUR)).toBe('allies');
    expect(readable(g, a)).toBe(true);
    expect(moment(g, a)).toBeTruthy();
    setStop(g, 4);
    expect(moment(g, a)).toBeNull();
  });

  it('is never marched for on a member of its own alliance', () => {
    const a = amb('kars');
    const g = karsWar();
    g.nations.TUR.align = 'allies';
    g.nations.SOV.align = 'allies';
    expect(moment(g, a)).toBeNull();
  });

  it('is pressed with a warning first, then marched for, and the claimant fights beside the holder\'s enemies', () => {
    const a = amb('kars');
    const g = karsWar();
    // Turkey has joined the Allies, and Germany is at war with it.
    g.nations.TUR.align = 'allies';
    setWar(g, 'GER', 'TUR');
    expect(bloc(g.nations.SOV)).toBeNull();
    claimsMonth(g);
    expect(g.flags[`amb:${a.id}`]).toBe(g.turn);
    expect(g.headlines.some((h) => /presses its claim to Kars and the eastern provinces/.test(h.text))).toBe(true);
    expect(atWar(g, 'SOV', 'TUR')).toBe(false);
    for (let k = 0; k < 36 && !atWar(g, 'SOV', 'TUR'); k++) { g.turn++; claimsMonth(g); }
    expect(atWar(g, 'SOV', 'TUR')).toBe(true);
    expect(g.turn - g.flags[`amb:${a.id}`]).toBeGreaterThanOrEqual(HISTORY.claims.notice);
    expect(g.headlines.some((h) => /marches for Kars and the eastern provinces/.test(h.text))).toBe(true);
    expect(bloc(g.nations.SOV)).toBe('axis');
  });
});
