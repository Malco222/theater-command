// The far side: troops lifted off a coast are landed only on ground their own side holds (the design director's
// ruling B1 of 07:22Z, 22 November 2026, on the playtester's far pass of the same day: Japanese corps were set down
// inside a Tokyo the United States held in 9 of 31 wars).
//
// refuge() tries a theatre's havens, then the nearest harbour its country holds on the enemy's ground, and last the
// capital. The first two were always asked whose they are; the capital was not. With the far side on:
//   - a beaten corps with its back to the sea and no ground of its side to be landed on is not lifted: it lays down
//     its arms as one cut off (ruling 232(b));
//   - the order to lift a corps off is refused, with its sentence;
//   - a corps left on ground no longer its own to stand on when a country falls goes home only while its side holds
//     the capital: otherwise to the nearest ground its side holds, or with none it is struck off.
// Every nation. With the far side's switch off the old line stands (the 96 old-chart wars read it).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, capitulate, computeSupply, createGame, declareWar, evacuate, refuge, refugeHeld, resolveBattle, whyNotEvacuate } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

/** The grand campaign with Japan at war with Britain and the United States. */
const war = (): Game => {
  const g = startOpen(createGame(3, null));
  for (const e of ['USA', 'UK']) declareWar(g, 'JAP', e);
  return g;
};
const tokyo = (g: Game): Region => g.regions[g.nations.JAP.capital];
const put = (g: Game, nat: string, name: string, place: string, str = 1): Corps => { const c = addCorps(g, nat, name, 'inf', 1, place, { xp: 0.1 }); c.str = str; return c; };
const where = (g: Game, c: Corps): string => g.regions[c.reg].name;
/**
 * Kota Bharu in Japan's hands with one Japanese corps in it, and three British corps at Kuantan beside it. Every
 * province round it is Britain's (Thailand's Singora too, for the test), so a beaten corps has no ground to fall back on.
 */
const beach = (g: Game): { c: Corps; att: Corps[]; at: Region } => {
  const at = g.byName['Kota Bharu'];
  at.ctrl = 'JAP';
  at.units = [];
  for (const j of at.nb) g.regions[j].ctrl = 'UK';
  const c = put(g, 'JAP', 'A corps with its back to the sea', 'Kota Bharu', 0.8);
  const att = [1, 2, 3].map((k) => put(g, 'UK', `British corps ${k}`, 'Kuantan'));
  g.supplyDirty = true;
  computeSupply(g);
  return { c, att, at };
};

describe("troops are lifted off only to ground their own side holds (the director's ruling B1 of 07:22Z, 22 November 2026)", () => {
  it('the capital is a refuge while its own side holds it, and none once the enemy does', () => {
    const g = war();
    const { c, at } = beach(g);
    // No haven and no harbour of Japan's on British ground: the last place tried is the capital.
    expect([refuge(g, c, at).name, refugeHeld(g, c, at)?.name]).toEqual(['Tokyo', 'Tokyo']);
    expect(whyNotEvacuate(g, c)).toBeNull();
    tokyo(g).ctrl = 'USA';
    // (The old line still names the capital: it is the far side's reading that asks whose it is.)
    expect([refuge(g, c, at).name, refugeHeld(g, c, at)]).toEqual(['Tokyo', null]);
    expect(whyNotEvacuate(g, c)).toMatch(/Tokyo is in enemy hands.*nowhere to be lifted off to/);
    // An ally's hold of it is its own side's.
    tokyo(g).ctrl = 'JAP';
    expect(refugeHeld(g, c, at)?.name).toBe('Tokyo');
  });

  it('the order to lift a corps off is refused with the capital in enemy hands, and nothing moves', () => {
    const g = war();
    const { c } = beach(g);
    tokyo(g).ctrl = 'USA';
    const was = c.str;
    expect(evacuate(g, c)).toBe(false);
    expect([where(g, c), c.str, c.dead]).toEqual(['Kota Bharu', was, false]);
    tokyo(g).ctrl = 'JAP';
    expect(evacuate(g, c)).toBe(true);
    expect(where(g, c)).toBe('Tokyo');
  });

  it('beaten with its back to the sea, a corps is lifted home while its side holds the capital', () => {
    const g = war();
    const { c, att, at } = beach(g);
    const rep = resolveBattle(g, att, at);
    expect(rep.taken).toBe(true);
    expect(rep.defenders.map((u) => u.fate)).toEqual(['evacuated']);
    expect([where(g, c), c.dead]).toEqual(['Tokyo', false]);
  });

  it('and with the capital the enemy\'s it lays down its arms as one cut off: nobody is set down in an enemy town', () => {
    const g = war();
    const { c, att, at } = beach(g);
    tokyo(g).ctrl = 'USA';
    const inTokyo = tokyo(g).units.length;
    const rep = resolveBattle(g, att, at);
    expect(rep.taken).toBe(true);
    expect(rep.defenders.map((u) => [u.fate, !!u.cut])).toEqual([['surrendered', true]]);
    expect(c.dead).toBe(true);
    expect(g.corps.includes(c)).toBe(false);
    expect(tokyo(g).units.length).toBe(inTokyo);
    expect(rep.prisoners).toBeGreaterThan(0);
  });

  it('a country falls: a corps on ground no longer its own goes home only while its side holds the capital', () => {
    const fall = (held: boolean): { g: Game; c: Corps } => {
      const g = startOpen(createGame(3, null));
      declareWar(g, 'JAP', 'USA');
      // A Japanese corps on Dutch ground, at peace with the Dutch and with Britain, when the Netherlands fall and
      // Britain takes the Indies into its keeping.
      const c = put(g, 'JAP', 'A corps on ground not its own', 'Pontianak');
      if (!held) tokyo(g).ctrl = 'USA';
      declareWar(g, 'GER', 'NED');
      capitulate(g, 'NED', 'GER');
      return { g, c };
    };
    const home = fall(true);
    expect(home.g.byName.Pontianak.ctrl).toBe('UK');
    expect([where(home.g, home.c), home.c.dead]).toEqual(['Tokyo', false]);
    // With Tokyo the enemy's: Borneo has no ground of Japan's side to walk to, and the corps is struck off.
    const lost = fall(false);
    expect(lost.c.dead).toBe(true);
    expect(tokyo(lost.g).units.some((u) => u.nat === 'JAP' && u.name === lost.c.name)).toBe(false);
  });
});
