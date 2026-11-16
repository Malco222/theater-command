// A campaign in the desert is not lost in Rome: the design director's ruling 103(c), completing 99 and 101(b).
//
// An event may take the player's country out of the war only where his campaign's card states it as the stake. In
// the grand campaign that is never (ruling 99). Among the history campaigns Rome's change of government and the
// armistice fire on a player's Italy only in the campaign that opens in July 1943, whose card says so; a player who
// leads Italy in the desert from October 1940, in the Balkans, in the desert of 1942 or against Torch plays on. The
// same holds for any later history campaign: an event names the seats it may end by the month they are taken.
//
// The month a seat is taken is recorded when the campaign opens (openGoals, which both builds call). A game saved
// before that has no record, and is left to history as it was.
import { describe, expect, it } from 'vitest';
import { turnToDate } from '../src/core/ticket';
import { DIPLOMACY, fireEvent, holds, monthlyDiplomacy } from '../src/campaign/diplomacy';
import { createGame, setWar } from '../src/campaign/game';
import { openGoals } from '../src/campaign/goals';
import { spared } from '../src/campaign/league';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const AUG_43 = 48, SEPT_43 = 49;
/** The seats Italy is offered in the history campaigns, by the month they are taken; the last is July 1943. */
const DESERT_40 = 13, BALKANS = 14, DESERT_42 = 33, TORCH = 39, ITALY_43 = 47;
/** A history game with a player, his seat taken in that month (or not recorded, as in a game saved before Version 83). */
const seat = (me: string, from?: number): Game => { const g = createGame(25, me); if (from !== undefined) openGoals(g, me, from); return g; };
/** Italy at war with Britain, and Sardinia in British hands. */
const lose = (g: Game): Game => { setWar(g, 'ITA', 'UK'); g.byName.Sardinia.ctrl = 'UK'; return g; };
const army = (g: Game, nat: string): number => g.corps.filter((c) => c.nat === nat && !c.dead).length;
const ground = (g: Game, nat: string): number => g.regions.filter((r) => r.ctrl === nat).length;
const play = (g: Game, from: number, to: number): Game => { for (let t = from; t <= to; t++) { g.turn = t; monthlyDiplomacy(g); } return g; };

describe('the month a seat is taken names the campaign', () => {
  it('is recorded when the campaign opens, and kept by a saved game', () => {
    expect(createGame(25, 'ITA').flags.seat).toBeUndefined();
    const g = seat('ITA', DESERT_42);
    expect(g.flags.seat).toBe(DESERT_42);
    expect(loadGame(saveGame(g))!.flags.seat).toBe(DESERT_42);
    expect(turnToDate(ITALY_43).label).toBe('July 1943');
  });

  it('the two Italian events name July 1943 as the one seat they may end', () => {
    expect(DIPLOMACY.events.find((e) => e.id === 'mussolini-falls')!.if).toContain(`computer:ITA,${ITALY_43}`);
    expect(DIPLOMACY.events.find((e) => e.id === 'italian-armistice')!.stake).toEqual([ITALY_43]);
    // No other event that deals out a country names a seat: a player's country is spared from each of them.
    for (const e of DIPLOMACY.events.filter((x) => x.partition && x.id !== 'italian-armistice')) expect(e.stake, e.id).toBeUndefined();
  });
});

describe('a player\'s Italy in a history campaign', () => {
  it('in the desert, the Balkans and against Torch plays on through the autumn of 1943 with its army and its ground', () => {
    for (const from of [DESERT_40, BALKANS, DESERT_42, TORCH]) {
      const g = lose(seat('ITA', from));
      const men = army(g, 'ITA'), held = ground(g, 'ITA');
      expect(spared(g, 'ITA', [ITALY_43]), `seat ${from}`).toBe(true);
      expect(holds(g, `computer:ITA,${ITALY_43}`)).toBe(false);
      play(g, AUG_43, SEPT_43 + 3);
      expect(g.flags['event:mussolini-falls'], `seat ${from}`).toBeUndefined();
      expect(g.flags['event:italian-armistice'], `seat ${from}`).toBeUndefined();
      expect(g.nations.ITA.alive).toBe(true);
      expect(g.nations.ITA.atWar).toContain('UK');
      expect(army(g, 'ITA')).toBeGreaterThanOrEqual(men);
      expect(ground(g, 'ITA')).toBe(held);
      expect(g.headlines.filter((h) => /voted against Mussolini|signed an armistice/.test(h.text))).toEqual([]);
      // The terms themselves refuse too, whoever calls them.
      g.flags['event:mussolini-falls'] = AUG_43;
      expect(fireEvent(g, 'italian-armistice')).toBe(false);
      expect(g.nations.ITA.alive).toBe(true);
    }
  });

  it('in the campaign of July 1943, whose card states it as the stake, loses its government in August and signs in September', () => {
    const g = lose(seat('ITA', ITALY_43));
    expect(spared(g, 'ITA', [ITALY_43])).toBe(false);
    expect(holds(g, `computer:ITA,${ITALY_43}`)).toBe(true);
    play(g, AUG_43, AUG_43);
    expect(g.flags['event:mussolini-falls']).toBe(AUG_43);
    expect(g.nations.ITA.alive).toBe(true);
    play(g, SEPT_43, SEPT_43);
    expect(g.flags['event:italian-armistice']).toBe(SEPT_43);
    expect(g.nations.ITA.alive).toBe(false);
  });

  it('with Sardinia held, the July 1943 campaign plays on: the stake is the ground, as its card says', () => {
    const g = seat('ITA', ITALY_43);
    setWar(g, 'ITA', 'UK');
    play(g, AUG_43, SEPT_43 + 2);
    expect(g.flags['event:mussolini-falls']).toBeUndefined();
    expect(g.nations.ITA.alive).toBe(true);
  });

  it('in a game saved before seats were recorded is left to history as it was', () => {
    const g = lose(seat('ITA'));
    expect(spared(g, 'ITA', [ITALY_43])).toBe(false);
    play(g, AUG_43, SEPT_43);
    expect(g.flags['event:mussolini-falls']).toBe(AUG_43);
    expect(g.flags['event:italian-armistice']).toBe(SEPT_43);
  });
});

describe('everyone else is as before', () => {
  it('the computer\'s Italy changes its government and signs, whatever seat another player took', () => {
    for (const [me, from] of [['UK', DESERT_42], ['GER', 34], ['UK', ITALY_43]] as [string, number][]) {
      const g = lose(seat(me, from));
      expect(spared(g, 'ITA', [ITALY_43])).toBe(false);
      play(g, AUG_43, SEPT_43);
      expect(g.flags['event:mussolini-falls'], me).toBe(AUG_43);
      expect(g.flags['event:italian-armistice'], me).toBe(SEPT_43);
      expect(g.nations.ITA.alive).toBe(false);
    }
  });

  it('with nobody playing nothing is spared', () => {
    const g = lose(createGame(25, null));
    expect(spared(g, 'ITA', [ITALY_43])).toBe(false);
    play(g, AUG_43, SEPT_43);
    expect(g.nations.ITA.alive).toBe(false);
  });

  it('in the grand campaign the player\'s own country is spared whatever month he took it, and nobody else\'s is', () => {
    const g = startOpen(createGame(25, 'ITA'));
    expect(spared(g, 'ITA', [ITALY_43])).toBe(true);
    openGoals(g, 'ITA', ITALY_43);
    expect(spared(g, 'ITA', [ITALY_43])).toBe(true);
    expect(holds(g, `computer:ITA,${ITALY_43}`)).toBe(false);
    expect(spared(g, 'GER', [ITALY_43])).toBe(false);
  });
});
