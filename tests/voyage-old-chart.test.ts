// On the old chart a move of base is made the month it is ordered, wherever it goes, and a strike at anchor moves no
// fleet (release 2's voyage and withdrawal are the far chart's: the design director's ruling of 04:33Z, 25 November 2026).
import { describe, expect, it } from 'vitest';
import { createGame, declareWar } from '../src/campaign/game';
import { anchorLoss, baseTargets, inPassage, moveCost, navalAI, rebase, struckLine, voyageMonths } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { farOn } from '../src/campaign/world';

describe('the old chart', () => {
  it('has no voyage: every move of base is made that month', () => {
    const g = startOpen(createGame(3, null));
    expect(farOn(g)).toBe(false);
    for (const f of g.fleets.filter((x) => x.base >= 0 && x.stance === 'port').slice(0, 12)) {
      const ts = baseTargets(g, f);
      for (const r of ts) expect([voyageMonths(g, f, r), moveCost(g, f, r)]).toEqual([0, 1]);
      const far = ts[ts.length - 1];
      if (!far) continue;
      expect(rebase(g, f, far)).toBe(true);
      expect([f.stance, f.base, f.zone, inPassage(f)]).toEqual(['port', far.id, f.home, false]);
      expect(f.passage).toBeUndefined();
    }
  });

  it('reads no strike at anchor', () => {
    const g = startOpen(createGame(3, null));
    declareWar(g, 'ITA', 'UK');
    const f = g.fleets.find((x) => x.nat === 'ITA' && x.stance === 'port' && x.base >= 0)!;
    const from = f.base;
    f.anchor = [{ t: g.turn, at: f.base, n: 3, lost: 0.5, w: 300, from: [{ sea: f.home, field: -1, km: 600 }] }];
    expect(anchorLoss(g, f)).toBe(0);
    expect(struckLine(g, f)).toBe('');
    navalAI(g, 'ITA');
    expect(f.base).toBe(from);
  });
});
