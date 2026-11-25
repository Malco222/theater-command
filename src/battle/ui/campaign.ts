// The page's campaign: a real grand campaign at its first month, the attacks its player could order now, and the
// battle fought on the field handed back through the rules' own attack order. The page holds one game for as long as
// it is open, so a battle fought here changes the campaign the next one is chosen from.

import { createGame, hostileUnits } from '../../campaign/game';
import { startOpen } from '../../campaign/staff';
import { beginHuman, candidates, humanUp, previewAttack, stepAI } from '../../campaign/turn';
import type { BattleReport, Corps, Game, Region } from '../../campaign/types';
import type { FieldResult, FieldTicket } from '../core/types';
import { handBack, landTicket, oddsNow, type LandPlace } from '../land';

/** The country the slice plays, and the seed of its campaign. TODO(polish): any country and start year, from the campaign's own menu. */
export const PLAYS = { nat: 'GER', seed: 3 };

/**
 * The chance as the attack card printed it, in whole percents (b476(4)): the after card never states a figure the card
 * did not show. 'end' where the card said the staff was sure of it or gave it no chance; 'guess' where it printed a
 * best guess ("a best guess of 71%, anywhere from 40 to 95%"); else 'lo' and 'hi' ("60 to 100%", or "about 70%" where
 * they are one figure).
 */
export interface CardSaid { end: 'sure' | 'none' | null; lo: number; hi: number; guess: number | null }

export interface Offer {
  target: Region;
  /** Every corps the roads will carry, as the attack card opens. */
  corps: Corps[];
  odds: number;
  ticket: FieldTicket<LandPlace>;
  /**
   * The staff's estimate of the odds where the defence is not fully known, as the attack card showed it: the field
   * shows this band, never the card's own odds, until the fight ends (the design director, b453). None where the card
   * knew the defence. 'guess' is the card's best guess at the chance over the band: after the fight the staff names
   * that, never the chance at the true odds (b464(6)).
   */
  band?: { lo: number; hi: number; guess: number; said?: CardSaid };
}

/** A grand campaign at the player's first month. */
export function newCampaign(seed = PLAYS.seed, nat = PLAYS.nat): Game {
  const g = startOpen(createGame(seed, nat));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}

/** The attacks the player could order now, one for each enemy province in reach, with everyone the roads will carry. */
export function offers(g: Game): Offer[] {
  const me = g.human;
  if (!me || !humanUp(g)) return [];
  const seen = new Set<number>();
  const out: Offer[] = [];
  for (const c of g.corps) {
    if (c.nat !== me || c.dead || c.ap <= 0) continue;
    for (const j of g.regions[c.reg].nb) {
      if (seen.has(j)) continue;
      const r = g.regions[j];
      if (!hostileUnits(g, r, me).length) continue;
      const cs = candidates(g, r);
      const a = cs.length ? previewAttack(g, cs, r) : null;
      if (!a) continue;
      const t = landTicket(g, cs, r);
      if (!t) continue;
      seen.add(j);
      out.push({ target: r, corps: cs, odds: a.odds, ticket: t });
    }
  }
  // Nearest to an even fight first: those are the battles worth fighting by hand.
  return out.sort((a, b) => Math.abs(Math.log(a.odds / 1.3)) - Math.abs(Math.log(b.odds / 1.3)));
}

/** Hands the battle back. Null where the attack can no longer be ordered (nothing should have moved, but the rules decide). */
export function settle(g: Game, o: Offer, result: FieldResult): { report: BattleReport | null; drift: boolean } {
  const drift = Math.abs(oddsNow(g, o.corps, o.target) - o.odds) > 1e-6;
  return { report: handBack(g, o.corps, o.target, result), drift };
}
