// The design director's b502 (rider to ruling 232): the Red Army crossed into Poland on 17 September 1939 while Poland
// still fought. If Poland falls before the march (as it can once a beaten army gives up its city), the pact hands
// the Soviet zone over and the Red Army marches into it all the same: the same corps to the same six places, so the
// zone is never handed over empty. Nothing else in the event changes.
import { describe, expect, it } from 'vitest';
import { capitulate, createGame, sovietEvent } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const SIX = ['Wilno', 'Brest', 'Lwow', 'Volhynia', 'Bialystok', 'Polesie'];
const sovietIn = (g: Game, name: string): number => g.byName[name].units.filter((u) => u.nat === 'SOV' && !u.dead).length;

describe('the Red Army marches into its zone of Poland', () => {
  it('when Poland has fallen first, the zone is handed over and the Red Army marches in', () => {
    const g = startOpen(createGame(3, null));
    capitulate(g, 'POL', 'GER');
    expect(g.nations.POL.alive).toBe(false);
    const zone = SIX.filter((n) => g.byName[n].ctrl === 'SOV');
    expect(zone.length).toBeGreaterThan(0);
    for (const n of zone) expect(sovietIn(g, n)).toBe(0);
    sovietEvent(g);
    expect(g.flags.sovietMove).toBe(1);
    for (const n of zone) expect(sovietIn(g, n)).toBeGreaterThan(0);
    expect(g.headlines.some((h) => /Red Army crosses into eastern Poland/.test(h.text))).toBe(true);
    // Once only.
    const at = g.corps.filter((c) => c.nat === 'SOV').map((c) => c.reg).join();
    sovietEvent(g);
    expect(g.corps.filter((c) => c.nat === 'SOV').map((c) => c.reg).join()).toBe(at);
  });

  it('while Poland stands and its front holds, nothing moves', () => {
    const g = startOpen(createGame(3, null));
    sovietEvent(g);
    expect(g.flags.sovietMove).toBeUndefined();
    for (const n of SIX) expect(g.byName[n].ctrl).toBe('POL');
  });

  it('a Poland that fell to a Germany at war with the Soviet Union hands nothing over, and nobody marches', () => {
    const g = startOpen(createGame(3, null));
    g.nations.GER.atWar.push('SOV');
    g.nations.SOV.atWar.push('GER');
    capitulate(g, 'POL', 'GER');
    sovietEvent(g);
    expect(g.flags.sovietMove).toBeUndefined();
    for (const n of SIX) expect(g.byName[n].ctrl).not.toBe('SOV');
  });
});

describe('the historical scenarios', () => {
  it('are left as they were: a Poland that has fallen sees no march', () => {
    const g = createGame(3, null);
    capitulate(g, 'POL', 'GER');
    sovietEvent(g);
    expect(g.flags.sovietMove).toBeUndefined();
  });
});
