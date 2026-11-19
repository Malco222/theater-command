// The design director's b509(c) and b512(g, h): at the first two stops of the history dial, the book's restraints
// before Leningrad. A computer Germany rings the town and does not storm it (Hitler's choice of September 1941: starve
// it), and a computer Finland does not go into its province (in 1941 the Finnish army halted on the Karelian Isthmus at
// the frontier of 1939). A player is never held to either, the looser stops keep the staffs' own judgement, and the
// historical scenarios are played as before.
import { describe, expect, it } from 'vitest';
import { atWar, createGame, declareWar, relocate } from '../src/campaign/game';
import { halted, setStop, stormHeldBy } from '../src/campaign/history';
import { blocked } from '../src/campaign/rivers';
import { besieged, fed, fedByBook, monthsShut, openSieges, siegeMonth, siegeView } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import type { Game, Region } from '../src/campaign/types';

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

  it('hold only computer against computer: a player in the German, Finnish or Soviet seat lifts them all (b527)', () => {
    for (const me of ['GER', 'FIN', 'SOV']) {
      const g = grand(1, me);
      expect(stormHeldBy(g, 'Leningrad'), me).toBeNull();
      expect(halted(g, 'FIN', 'Leningrad'), me).toBe(false);
      expect(blocked(g, finn(g), g.byName['Leningrad']), me).toBe(false);
    }
    // A player on another front leaves the east's book as it is.
    for (const me of ['UK', 'USA', 'JAP']) {
      const g = grand(1, me);
      expect(stormHeldBy(g, 'Leningrad'), me).toBe('GER');
      expect(halted(g, 'FIN', 'Leningrad'), me).toBe(true);
    }
  });

  it('are no part of a historical scenario', () => {
    const g = createGame(3, null);
    expect(stormHeldBy(g, 'Leningrad')).toBeNull();
    expect(halted(g, 'FIN', 'Leningrad')).toBe(false);
  });
});

// Proposed to the design director (stop 1, 139 on and off: Leningrad starved out in most wars under a ring that never
// storms): the book feeds Leningrad across Lake Ladoga while a computer Germany rings it, at the first two stops.
describe('the book feeds Leningrad while a computer Germany rings it', () => {
  function ringed(stop: number, human: string | null = null): { g: Game; town: Region } {
    const g = grand(stop, human);
    openSieges(g);
    if (!atWar(g, 'GER', 'SOV')) declareWar(g, 'GER', 'SOV');
    const town = g.byName['Leningrad'];
    ring(g, town);
    return { g, town };
  }
  /** Clears the Soviet field out of a province and stands six German corps round its town. */
  function ring(g: Game, town: Region): void {
    const away = g.regions.find((x) => x.ctrl === 'SOV' && !x.units.length && x.id !== town.id && !x.nb.includes(town.id) && x.name !== 'Moscow' && x.name !== 'Leningrad')!;
    for (const c of [...town.units]) relocate(g, c, away);
    for (const c of g.corps.filter((c) => c.nat === 'GER' && !c.dead && !c.town && c.kind === 'inf' && c.reg !== town.id).slice(0, 6)) relocate(g, c, town);
  }

  it('keeps its stores from running down at the first two stops', () => {
    for (const stop of [1, 2]) {
      const { g, town } = ringed(stop);
      expect(besieged(g, town), `stop ${stop}`).toBe(true);
      expect(fedByBook(g, town), `stop ${stop}`).toBe(true);
      expect(fed(g, town)).toBe(true);
      for (let m = 0; m < 9; m++) siegeMonth(g, 'SOV');
      expect(monthsShut(g, town), `stop ${stop}`).toBe(0);
      expect(town.ctrl).toBe('SOV');
    }
  });

  it('starves it as any other town at the looser stops, or under a player\'s ring', () => {
    for (const stop of [3, 4, 5]) {
      const { g, town } = ringed(stop);
      expect(fedByBook(g, town), `stop ${stop}`).toBe(false);
      siegeMonth(g, 'SOV');
      expect(monthsShut(g, town), `stop ${stop}`).toBe(1);
    }
    for (const me of ['GER', 'FIN', 'SOV']) { const { g, town } = ringed(1, me); expect(fedByBook(g, town), me).toBe(false); }
    // A player's corps standing in a computer Germany's ring takes the book's food away while it stands there.
    {
      const { g, town } = ringed(1, 'ITA');
      if (!atWar(g, 'ITA', 'SOV')) declareWar(g, 'ITA', 'SOV');
      expect(fedByBook(g, town)).toBe(true);
      expect(siegeView(g, town.id)!.byBook).toBe(true);
      relocate(g, g.corps.find((c) => c.nat === 'ITA' && !c.dead && !c.town && c.kind === 'inf')!, town);
      expect(fedByBook(g, town)).toBe(false);
      expect(siegeView(g, town.id)!.byBook).toBe(false);
      siegeMonth(g, 'SOV');
      expect(monthsShut(g, town)).toBe(1);
    }
    const { g, town } = ringed(1, 'UK');
    expect(fedByBook(g, town)).toBe(true);
  });

  it('feeds no other town', () => {
    const { g } = ringed(1);
    const moscow = g.byName['Moscow'];
    ring(g, moscow);
    expect(besieged(g, moscow)).toBe(true);
    expect(fedByBook(g, moscow)).toBe(false);
  });
});
