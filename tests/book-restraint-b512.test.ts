// The design director's b509(c) and b512(g, h): at the first two stops of the history dial, the book's restraints
// before Leningrad. A computer Germany rings the town and does not storm it (Hitler's choice of September 1941: starve
// it), and a computer Finland does not go into its province (in 1941 the Finnish army halted on the Karelian Isthmus at
// the frontier of 1939). A player is never held to either, the looser stops keep the staffs' own judgement, and the
// historical scenarios are played as before.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { halted, setStop, stormHeldBy } from '../src/campaign/history';
import { blocked } from '../src/campaign/rivers';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

function grand(stop: number, human: string | null = null): Game {
  const g = startOpen(createGame(3, human));
  setStop(g, stop, true);
  return g;
}
const finn = (g: Game) => g.corps.find((c) => c.nat === 'FIN' && !c.dead && c.kind === 'inf')!;

describe("the book's restraints before Leningrad", () => {
  it('hold a computer Germany back from storming it at the first two stops', () => {
    for (const stop of [1, 2]) expect(stormHeldBy(grand(stop), 'Leningrad'), `stop ${stop}`).toBe('GER');
    for (const stop of [3, 4, 5]) expect(stormHeldBy(grand(stop), 'Leningrad'), `stop ${stop}`).toBeNull();
    expect(stormHeldBy(grand(1), 'Moscow')).toBeNull();
  });

  it('keep a computer Finland out of its province at the first two stops', () => {
    for (const stop of [1, 2]) {
      const g = grand(stop);
      expect(halted(g, 'FIN', 'Leningrad'), `stop ${stop}`).toBe(true);
      expect(blocked(g, finn(g), g.byName['Leningrad']), `stop ${stop}`).toBe(true);
      expect(blocked(g, finn(g), g.byName['Karelia']), `stop ${stop}`).toBe(false);
      expect(halted(g, 'GER', 'Leningrad')).toBe(false);
    }
    const g = grand(3);
    expect(blocked(g, finn(g), g.byName['Leningrad'])).toBe(false);
  });

  it('never bind a player', () => {
    expect(stormHeldBy(grand(1, 'GER'), 'Leningrad')).toBeNull();
    const g = grand(1, 'FIN');
    expect(halted(g, 'FIN', 'Leningrad')).toBe(false);
    expect(blocked(g, finn(g), g.byName['Leningrad'])).toBe(false);
  });

  it('are no part of a historical scenario', () => {
    const g = createGame(3, null);
    expect(stormHeldBy(g, 'Leningrad')).toBeNull();
    expect(halted(g, 'FIN', 'Leningrad')).toBe(false);
  });
});
