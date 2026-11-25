// The offers rule's own switch (release 2, Japan and the Pacific; stage.ts OFFERS_STAGE, game.ts offersOn). The design
// director's ruling of 13:39Z, 25 November 2026, on ruling B8: "B8 stays OFF until its screen is built and passes the
// Mac: yes. Then ON under Brian's 01:10Z rule." As first built the offers ran wherever the whole world did, so the
// player's Japan kept its lent regiments as offers the month Singapore was his with no screen to answer them on.
//
// This file has the WHOLE WORLD ON and the OFFERS OFF: a player's Japan and a player's America are walked through
// their dated rows. Nobody is offered anything, no flag of an offer is written, the lent regiments go home as a
// computer's do, Oahu's split is made by itself, and the 3D game has no page and no row. The same walk with the flag
// set on the game is offered what ruling B8 offers, so the walk does reach the rule.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { createGame, declineOffer, loansEnding, offersOn, relocate, rowOffers, rowsAwaitingWar, setWar, startPhase, takeOffer } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { OFFERS_STAGE } from '../src/campaign/stage';
import { formationOffers, orderDeclineFormation, orderTakeFormation } from '../src/campaign/turn';
import { farOn } from '../src/campaign/world';
import type { Game } from '../src/campaign/types';
import stage from '../data/campaign/stage.json';
import '../unreal/TheaterCommand/Rules/campaign-bridge';
import { cardRowOf, offersLook } from '../unreal/TheaterCommand/Rules/offers-look';

type Call = (...a: unknown[]) => any;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
const months = (g: Game, nat: string, from: number, to: number): void => { for (let t = from; t <= to; t++) { g.turn = t; startPhase(g, nat); } };
const offerFlags = (g: Game): string[] => Object.keys(g.flags).filter((k) => /^(asked|leave|kept|unmade):/.test(k));
const names = (g: Game, nat: string): string[] => g.corps.filter((c) => c.nat === nat && !c.dead).map((c) => c.name);
/** A player's Japan at war with Britain in December 1941 (month 28) with Singapore his: the month two loans end. */
const singapore = (on: boolean): Game => {
  const g = startOpen(createGame(5, 'JAP'));
  if (on) g.flags.offers = 1;
  months(g, 'JAP', 1, 27);
  setWar(g, 'JAP', 'UK');
  const r = g.byName.Singapore;
  for (const u of [...r.units]) relocate(g, u, g.byName.Calcutta);
  r.ctrl = 'JAP';
  months(g, 'JAP', 28, 28);
  return g;
};
const TANKS = ['1st Tank Regiment', '6th Tank Regiment'];

describe('the offers rule has a switch of its own, off until its screen passes the Mac', () => {
  it('ships off, and a campaign opened on the whole world does not carry it', () => {
    expect((stage as { offers?: boolean }).offers).toBe(false);
    expect(OFFERS_STAGE).toBe(false);
    const g = startOpen(createGame(5, 'JAP'));
    expect(farOn(g)).toBe(true);
    expect(g.flags.offers).toBeUndefined();
    expect(offersOn(g)).toBe(false);
  });

  it('off: a player\'s Japan is offered nothing in thirty months, and no flag of an offer is written', () => {
    const g = startOpen(createGame(5, 'JAP'));
    for (let t = 1; t <= 30; t++) {
      months(g, 'JAP', t, t);
      expect(rowOffers(g), `month ${t}`).toEqual([]);
      // The row that takes his corps came by itself, as a computer's does: the 5th Division's, in month 2.
      if (t === 2) expect(names(g, 'JAP')).toContain('21st Army (5th Division)');
    }
    expect(offerFlags(g)).toEqual([]);
    expect([formationOffers(g), loansEnding(g), rowsAwaitingWar(g)]).toEqual([[], [], []]);
    expect([takeOffer(g, 0), declineOffer(g, 0), orderDeclineFormation(g, 0)]).toEqual(['There is no such offer.', false, false]);
    expect(orderTakeFormation(g, 0)).not.toBeNull();
  });

  it('on (the flag set on the same game): the same walk is offered the 5th Division in month 2, so the walk reaches the rule', () => {
    const g = startOpen(createGame(5, 'JAP'));
    g.flags.offers = 1;
    expect(offersOn(g)).toBe(true);
    months(g, 'JAP', 1, 2);
    expect(rowOffers(g).map((o) => o.name)).toEqual(['21st Army (5th Division)']);
    expect(names(g, 'JAP')).not.toContain('21st Army (5th Division)');
    expect(offerFlags(g).length).toBeGreaterThan(0);
  });

  it('off: the lent tank regiments go home the month Singapore is his; on: they stay as two loans ending', () => {
    const off = singapore(false);
    expect(TANKS.map((nm) => names(off, 'JAP').includes(nm))).toEqual([false, false]);
    expect(rowOffers(off)).toEqual([]);
    expect(offerFlags(off)).toEqual([]);
    const on = singapore(true);
    expect(TANKS.map((nm) => names(on, 'JAP').includes(nm))).toEqual([true, true]);
    expect(rowOffers(on).filter((o) => o.loan !== null).map((o) => o.name)).toEqual(TANKS);
  });

  it('off: Oahu\'s split is made by itself for a player\'s America in month 26, as for a computer\'s', () => {
    const g = startOpen(createGame(5, 'USA'));
    months(g, 'USA', 1, 26);
    expect(rowOffers(g)).toEqual([]);
    expect(names(g, 'USA')).toEqual(expect.arrayContaining(['24th Infantry Division', '25th Infantry Division']));
    expect(offerFlags(g)).toEqual([]);
    const on = startOpen(createGame(5, 'USA'));
    on.flags.offers = 1;
    months(on, 'USA', 1, 26);
    expect(rowOffers(on).map((o) => o.name).sort()).toEqual(['24th Infantry Division', '25th Infantry Division']);
    expect(names(on, 'USA')).not.toContain('25th Infantry Division');
  });

  it('off: the 3D game has no offers page, no row on the month card and nothing in its list', () => {
    const g = singapore(false);
    g.phase = g.order.indexOf('JAP');
    expect(TC().load(saveGame(g))).not.toBeNull();
    expect(TC().page('offers')).toEqual({ ok: false });
    expect((TC().page('reports')?.sections ?? []).map((x: { heading: string }) => x.heading)).not.toContain('FORMATIONS OFFERED');
    expect(TC().offers()).toEqual({ count: 0, head: '', note: '', can: false, why: '', rows: [] });
    expect(offersLook(g).count).toBe(0);
    expect(cardRowOf(g, formationOffers(g))).toEqual({ text: '', amber: false, act: '' });
  });
});
