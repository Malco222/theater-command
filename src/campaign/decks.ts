// A carrier fleet's deck plan (the sandbox; Brian, 19 November 2026: "treat carriers as airfields, you can pick which
// squadrons to assign to them"). A fleet's carriers are one floating airfield: their decks hold the nation's wing
// (carriers.json 'wing'), fighters and strike aircraft, which fill themselves from the reserve while the fleet lies in
// harbour, as they always have. With the sandbox on the player may divide the decks by squadrons between fighters,
// dive bombers and torpedo planes; what is aboard follows when the plan is signed, as far as the reserve allows, and
// the rest comes as the decks are made up in port. Until he does, a navy flies its own mix.
//
// Dive bombers are surest against carriers and light ships; torpedo planes do the most harm to battleships and lose
// more to the ships' guns (sandbox-ships.json 'decks'). Half and half is about as the game always was. This file holds
// only the plan and its numbers: carriers.ts sizes the wings by it, air.ts reads it when a strike goes in.

import carriersData from '../../data/campaign/carriers.json';
import { SANDBOX_SHIPS } from './sandbox-data';
import type { Fleet, ShipClass } from './types';

const WING = (carriersData as unknown as { wing: Record<string, { ftr: number; bmr: number }> }).wing;
const D = SANDBOX_SHIPS.decks;

export interface DeckPlan { ftr: number; dive: number; torp: number }

/** The aircraft one of this navy's carriers brings: its fighters and its strike aircraft. */
export const navyWing = (nat: string): { ftr: number; bmr: number } => WING[nat] ?? WING._default;

/** The share of a navy's strike aircraft that are torpedo planes, until its player says otherwise. */
export const torpOf = (nat: string): number => D.torp[nat] ?? D.torp._default;

/** A fleet's deck plan, as shares of its decks' aircraft: the player's, or its navy's own mix. */
export function deckPlan(c: Fleet): DeckPlan {
  if (c.deckPlan) return c.deckPlan;
  const w = navyWing(c.nat), all = w.ftr + w.bmr;
  const f = all > 0 ? w.ftr / all : 0, t = torpOf(c.nat);
  return { ftr: f, dive: (1 - f) * (1 - t), torp: (1 - f) * t };
}

/** The share of a fleet's strike aircraft that are torpedo planes. */
export function torpShare(c: Fleet): number {
  const p = deckPlan(c), strike = p.dive + p.torp;
  return strike > 0 ? p.torp / strike : torpOf(c.nat);
}

/**
 * Aircraft of a kind ('ftr' fighters, 'bmr' strike aircraft) that one carrier's deck holds: its navy's wing, or with a
 * deck plan of the player's its share of the same deck.
 */
export function perDeck(c: Fleet, kind: 'ftr' | 'bmr'): number {
  const w = navyWing(c.nat);
  if (!c.deckPlan) return w[kind];
  const p = c.deckPlan, all = w.ftr + w.bmr;
  return all * (kind === 'ftr' ? p.ftr : p.dive + p.torp);
}

/** What strike aircraft with this share of torpedo planes do to ships of a kind, beside the strike as it always was. */
export const planeHurt = (t: number, cls: ShipClass): number => (1 - t) * D.planes.dive.hurt[cls] + t * D.planes.torp.hurt[cls];
/** What share of the ships' guns' toll they suffer, beside the strike as it always was. */
export const planeFlak = (t: number): number => (1 - t) * D.planes.dive.flak + t * D.planes.torp.flak;
