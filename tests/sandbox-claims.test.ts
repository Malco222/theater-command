// The sandbox's lock sweep, finding 4 (after release 1): where anything goes (stop 5; history.json claims 'inBloc') a
// great power may march for a claim on a country that has taken a side, so long as it is not a member of its own
// alliance. At stop 4 it still marches only on a country outside both alliances (tests/history-dial.test.ts reads
// the claims' own moment). Since the great powers' claims became their dealt war aims (tests/sandbox-war-aims.test.ts)
// the rule is read where a staff weighs an aim: aims.ts, aimHolders.
import { describe, expect, it } from 'vitest';
import { aimHolders, aimMarched, dealt } from '../src/campaign/aims';
import { atWar, bloc, createGame, declareWar, setWar } from '../src/campaign/game';
import { HISTORY, setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const grand = (seed: number, stop: number): Game => { const g = startOpen(createGame(seed, null)); if (stop !== 3) setStop(g, stop, true); return g; };
/** A war where anything goes in which the Soviet Union was dealt its claim on Kars, in a month when its time has come. */
const karsWar = (): Game => {
  for (let s = 1; s < 60; s++) { const x = grand(s, 5); x.turn = 40; if (dealt(x, 'SOV').some((a) => a.key === 'kars') && aimHolders(x, 'SOV', 'kars').length) return x; }
  throw new Error('no war with the claim on Kars dealt');
};
/** France beaten: at stop 4 the moment a great power waits for. */
const fall = (g: Game, nat: string): void => { g.nations[nat].alive = false; g.nations[nat].fellOn = { turn: g.turn, to: 'GER' } as Game['nations'][string]['fellOn']; };

describe('a great power\'s claim on a country that has taken a side', () => {
  it('is named in the data, for stop 5 alone', () => {
    expect(HISTORY.claims.inBloc).toEqual([5]);
  });

  it('may be marched for where anything goes', () => {
    const g = karsWar();
    expect(aimHolders(g, 'SOV', 'kars')).toEqual(['TUR']);
    g.nations.TUR.align = 'allies_cobelligerent';
    expect(bloc(g.nations.TUR)).toBe('allies');
    expect(aimHolders(g, 'SOV', 'kars')).toEqual(['TUR']);
  });

  it('is not marched for at stop 4, where a great power goes only for a country outside both alliances', () => {
    const g = grand(3, 4);
    g.turn = 12;
    fall(g, 'FRA');
    expect(aimHolders(g, 'SOV', 'iran')).toEqual(['IRN']);
    g.nations.IRN.align = 'allies_cobelligerent';
    expect(aimHolders(g, 'SOV', 'iran')).toEqual([]);
    setStop(g, 5);
    g.turn = 40;
    if (dealt(g, 'SOV').some((a) => a.key === 'iran')) expect(aimHolders(g, 'SOV', 'iran')).toEqual(['IRN']);
  });

  it('is never marched for on a member of its own alliance', () => {
    const g = karsWar();
    g.nations.TUR.align = 'allies';
    g.nations.SOV.align = 'allies';
    expect(aimHolders(g, 'SOV', 'kars')).toEqual([]);
  });

  it('puts a claimant outside both alliances beside the holder\'s enemies once it has marched', () => {
    const g = karsWar();
    // Turkey has joined the Allies, and Germany is at war with it.
    g.nations.TUR.align = 'allies';
    setWar(g, 'GER', 'TUR');
    expect(bloc(g.nations.SOV)).toBeNull();
    declareWar(g, 'SOV', 'TUR');
    expect(atWar(g, 'SOV', 'TUR')).toBe(true);
    aimMarched(g, 'SOV', 'kars');
    expect(g.flags['amb:go:kars']).toBe(g.turn);
    expect(bloc(g.nations.SOV)).toBe('axis');
  });
});
