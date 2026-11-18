// The design director's b502(c), rider to ruling 232: the Winter War's lent corps stay on its front until the peace.
// History's reinforcement of the isthmus (the 13th Army of the Northwestern Front, 1940) is lent to the Red Army two
// months into a war with Finland, at Leningrad, until June 1940; while that war is the Soviet staff's only war (a war
// for a strip of frontier, the op's 'near'), the staff's reserves and its massing leave it where it stands, so it is
// not walked off to Moscow's guard. The grand campaign only: the history campaigns are as they were.
import { describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';
import { winterMonth } from '../src/campaign/winter';

const NOV_39 = 3, JAN_40 = 5, JUNE_40 = 10;
const ROW = '13th Army';
const lent = (g: Game): Corps | undefined => g.corps.find((c) => c.nat === 'SOV' && !c.dead && c.name === ROW && g.turn < 19);
/** A grand campaign with the Winter War begun in November 1939, the staff's own war (the op done). */
function begun(seed = 3): Game {
  const g = startOpen(createGame(seed, null));
  while (g.turn < NOV_39) runTurnAI(g);
  if (!atWar(g, 'SOV', 'FIN')) { declareWar(g, 'SOV', 'FIN'); winterMonth(g); g.flags['st:done:SOV:winter'] = NOV_39; }
  return g;
}

describe("the Winter War's lent corps", () => {
  it('is one row, for the grand campaign, tied to the war, lent until June 1940 and kept on the front', () => {
    const rows = (setup as any).expeditions.filter((r: any[]) => r[1] === 'SOV' && r[2] === ROW && r[6]?.stay);
    expect(rows).toHaveLength(1);
    expect(rows[0][6]).toMatchObject({ open: 1, war: ['SOV', 'FIN'], after: 2, until: JUNE_40, stay: 1 });
    expect(rows[0][5]).toBe('Leningrad');
  });

  it('comes two months into the war, stays on the Finnish front while it lasts, and goes back in June 1940', () => {
    const g = begun();
    const began = Math.min(...Object.entries(g.flags).filter(([k]) => /^war:(SOV:FIN|FIN:SOV)$/.test(k)).map(([, v]) => v as number));
    while (g.turn < began + 2) runTurnAI(g);
    expect(lent(g)).toBeUndefined();
    runTurnAI(g);
    const c = lent(g)!;
    expect(c).toBeDefined();
    expect(g.flags[`stay:${c.id}`]).toBe(1);
    // While the war with Finland lasts, it is on the isthmus front: Leningrad, or the Finnish ground next to it.
    const front = new Set(['Leningrad', ...g.byName.Leningrad.nb.map((j) => g.regions[j].name)]);
    while (g.turn < JUNE_40 && atWar(g, 'SOV', 'FIN')) {
      if (!c.dead) expect(front.has(g.regions[c.reg].name), g.regions[c.reg].name).toBe(true);
      runTurnAI(g);
    }
    while (g.turn <= JUNE_40) runTurnAI(g);
    expect(lent(g)).toBeUndefined();
    expect(g.headlines.some((h) => /lent to the campaign.*leave for other fronts: .*13th Army/.test(h.text))).toBe(true);
  });

  it('is not sent in the history campaigns', () => {
    const g = createGame(3, null);
    while (g.turn <= JAN_40 + 2) runTurnAI(g);
    expect(g.corps.some((c) => c.nat === 'SOV' && c.name === ROW && g.flags[`stay:${c.id}`])).toBe(false);
  });
});
