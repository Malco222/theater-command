// The sandbox's lock sweep, finding 6 (after release 1): where neutrals have ambitions (stops 4 and 5) the Winter
// War's peace is drawn from the map when the computer fights it on both sides. No last month by the calendar: the
// peace comes the month after the Red Army holds the isthmus it went to war for. Its terms are the victor's war aim,
// not all the ground it holds: Karelia is ceded and the rest goes back, as the peace of Moscow handed Petsamo back
// (the design director's sandbox ruling of 20 November 2026, finding F). Stops 1 to 3, and a player on either side,
// are as ruling 228 left them (tests/winter-war-228.test.ts).
import { describe, expect, it } from 'vitest';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';
import { winterMonth, winterPeace, winterSince } from '../src/campaign/winter';

const NOV_39 = 3, MAY_40 = 9;
const grand = (stop: number, human: string | null = null): Game => { const g = startOpen(createGame(3, human)); if (stop !== 3) setStop(g, stop, true); return g; };
const begun = (g: Game, at = NOV_39): Game => { g.turn = at; declareWar(g, 'SOV', 'FIN'); winterMonth(g); return g; };

describe('the Winter War\'s peace drawn from the map (stops 4 and 5)', () => {
  for (const stop of [4, 5]) {
    it(`stop ${stop}: when the isthmus has fallen the Red Army gets what it went to war for, Karelia, and hands the rest back`, () => {
      const g = begun(grand(stop));
      expect(winterSince(g)).toBe(NOV_39);
      g.turn = NOV_39 + 1;
      g.byName.Karelia.ctrl = 'SOV';
      g.byName.Petsamo.ctrl = 'SOV';
      winterMonth(g);
      expect(atWar(g, 'SOV', 'FIN')).toBe(false);
      expect([g.byName.Karelia.owner, g.byName.Karelia.ctrl, g.byName.Karelia.fort]).toEqual(['SOV', 'SOV', null]);
      expect([g.byName.Petsamo.owner, g.byName.Petsamo.ctrl]).toEqual(['FIN', 'FIN']);
      expect(g.corps.some((c) => !c.dead && c.nat === 'FIN' && c.reg === g.byName.Karelia.id)).toBe(false);
      expect(g.corps.some((c) => !c.dead && c.nat === 'SOV' && c.reg === g.byName.Petsamo.id)).toBe(false);
      const text = g.headlines.at(-1)!.text;
      expect(text).toBe('Finland has signed the peace of Moscow. It gives up Karelia, which the Red Army holds, and most of the province\'s people have left their homes rather than stay. The Red Army hands back Petsamo.');
      expect(winterSince(g)).toBe(0);
    });

    it(`stop ${stop}: with the isthmus alone in its hands the words are ruling 228's`, () => {
      const g = begun(grand(stop));
      g.turn = NOV_39 + 1;
      g.byName.Karelia.ctrl = 'SOV';
      winterMonth(g);
      expect(atWar(g, 'SOV', 'FIN')).toBe(false);
      expect(g.headlines.at(-1)!.text).toContain('It gives up Karelia, which the Red Army holds');
      expect(g.headlines.at(-1)!.text).not.toContain('hands back');
    });

    it(`stop ${stop}: no peace by the calendar: with Viipuri still Finnish the war goes on past May 1940`, () => {
      const g = begun(grand(stop));
      for (g.turn = NOV_39 + 1; g.turn <= MAY_40 + 6; g.turn++) {
        winterMonth(g);
        expect(atWar(g, 'SOV', 'FIN')).toBe(true);
      }
      // Other ground alone does not end it either: the isthmus is what Moscow went to war for.
      g.byName.Petsamo.ctrl = 'SOV';
      winterMonth(g);
      expect(atWar(g, 'SOV', 'FIN')).toBe(true);
      expect(g.byName.Petsamo.owner).toBe('FIN');
      // The month after it falls, the peace.
      g.byName.Karelia.ctrl = 'SOV';
      g.turn++;
      winterMonth(g);
      expect(atWar(g, 'SOV', 'FIN')).toBe(false);
      expect([g.byName.Karelia.owner, g.byName.Petsamo.owner, g.byName.Petsamo.ctrl]).toEqual(['SOV', 'FIN', 'FIN']);
    });

    it(`stop ${stop}: a war the great war swallows is still the great war's`, () => {
      const g = begun(grand(stop));
      g.turn = 22;
      declareWar(g, 'GER', 'SOV');
      g.byName.Karelia.ctrl = 'SOV';
      winterMonth(g);
      expect(atWar(g, 'SOV', 'FIN')).toBe(true);
      expect(g.byName.Karelia.owner).toBe('FIN');
      expect(winterSince(g)).toBe(0);
    });
  }

  it('stop 3 is unmoved: Karelia goes, Petsamo goes back, and May 1940 ends a war that took nothing (the calendar\'s peace, which stops 4 and 5 do not have)', () => {
    const g = begun(grand(3));
    g.turn = NOV_39 + 1;
    g.byName.Karelia.ctrl = 'SOV';
    g.byName.Petsamo.ctrl = 'SOV';
    winterMonth(g);
    expect(atWar(g, 'SOV', 'FIN')).toBe(false);
    expect([g.byName.Karelia.owner, g.byName.Petsamo.owner, g.byName.Petsamo.ctrl]).toEqual(['SOV', 'FIN', 'FIN']);
    expect(g.headlines.at(-1)!.text).toMatch(/The Red Army hands back Petsamo\.$/);
    const h = begun(grand(3));
    h.turn = MAY_40;
    winterMonth(h);
    expect(atWar(h, 'SOV', 'FIN')).toBe(false);
  });

  it('a player on either side keeps ruling 228\'s offer at stops 4 and 5: Karelia for peace, the rest handed back, never signed for him', () => {
    for (const human of ['SOV', 'FIN']) {
      const g = begun(grand(5, human));
      g.turn = NOV_39 + 1;
      g.byName.Karelia.ctrl = 'SOV';
      g.byName.Petsamo.ctrl = 'SOV';
      winterMonth(g);
      expect(atWar(g, 'SOV', 'FIN')).toBe(true);
      const wp = winterPeace(g)!;
      expect(wp.cede).toEqual(['Karelia']);
      expect(wp.back).toEqual(['Petsamo']);
    }
  });
});
