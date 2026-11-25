// The design director's rulings B7 and B8 of 25 November 2026 with the far switch off (release 2,
// design-director/pacific-japan-1125.md): "With far off nothing moves." No row is read by the new placing or the new
// waiting, nobody is ever offered a formation, no flag of an offer is written, and the 3D game's calls answer a list
// of none. A player's Britain and a player's America are walked through their dated rows on the old chart.
import { describe, expect, it } from 'vitest';
import { createGame, declineOffer, postedFor, rowOffers, startPhase, takeOffer } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { FAR_STAGE } from '../src/campaign/stage';
import { formationOffers, orderDeclineFormation, orderTakeFormation } from '../src/campaign/turn';
import { farOn } from '../src/campaign/world';
import setup from '../data/campaign/setup-1939.json';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

const rows = (setup as any).expeditions as any[][];
const NONE = { count: 0, head: '', note: '', can: false, why: '', rows: [] };

describe('rulings B7 and B8 with the far switch off', () => {
  it('no row is read and nobody is offered anything: three years of a player\'s Britain and a player\'s America', () => {
    expect(FAR_STAGE).toBe(false);
    for (const human of ['UK', 'USA']) {
      const g = startOpen(createGame(5, human));
      expect(farOn(g)).toBe(false);
      for (let t = 1; t <= 36; t++) {
        g.turn = t;
        for (const nat of g.order) if (g.nations[nat].alive) startPhase(g, nat);
        expect(rowOffers(g), `${human} month ${t}`).toEqual([]);
      }
      // (Nor a flag of a loan's end or of a formation never made: the mending after the review of 25 November 2026.)
      expect(Object.keys(g.flags).filter((k) => /^(offer|leave|kept|unmade):/.test(k))).toEqual([]);
      // The new rule answers for no row, wherever it is posted and whoever holds the place.
      for (const row of rows) expect(postedFor(g, row[1], row, g.byName[row[5]]), row[2]).toBeNull();
      expect(formationOffers(g)).toEqual([]);
      expect([takeOffer(g, 0), declineOffer(g, 0), orderDeclineFormation(g, 0)]).toEqual(['There is no such offer.', false, false]);
      expect(orderTakeFormation(g, 0)).not.toBeNull();
    }
  });

  it('the 3D game is answered a list of none, on the call and on the month card', () => {
    const TC = (globalThis as unknown as { TC: Record<string, (...a: unknown[]) => any> }).TC;
    TC.start({ human: 'UK', view: 'grand' });
    expect(TC.offers()).toEqual(NONE);
    expect(TC.offerTake('all')).toMatchObject({ ok: false, taken: [], left: [], said: '', offers: NONE });
    expect(TC.offerDecline('all')).toMatchObject({ ok: false, declined: [], said: '', offers: NONE });
    expect(TC.offerTake([3])).toMatchObject({ ok: false, taken: [] });
    const end = TC.endMonth() as { month: { offers: unknown } };
    expect(end.month.offers).toEqual(NONE);
    // And on the card as it is read again from the month's report.
    expect(TC.page('reports').month.offers).toEqual(NONE);
    // (The offers page and the card's row for it, on the UI director's marks of 25 November 2026: none, and empty.)
    expect(TC.page('offers')).toEqual({ ok: false });
    expect((end.month as { offersRow?: unknown }).offersRow).toEqual({ text: '', amber: false, act: '' });
    expect((TC.page('reports').sections as { heading: string }[]).map((x) => x.heading)).not.toContain('FORMATIONS OFFERED');
  }, 900000);
});
