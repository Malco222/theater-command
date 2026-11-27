// The hand-back between the campaign and the convoy battle's field (release 2, ruling 241; the production lead's
// form, naval-battles/wolfpack-r2/form.md sections 1, 2 and 4).
//
// A month on a convoy route is counted in the month of the nation the route feeds (navy.ts navalUpkeep: the hunt,
// then the convoy war), after the player's own month has closed and every computer's has run. So the screen that
// offers the player a convoy's night looks ahead: it closes the month on a copy of the game (passMonth: his month,
// every computer's, then the start of his next), with the seam (navy.ts convoyField) noting each route-month as the
// rules are about to count it. The player fights the nights he wants; then the month is closed on the game itself
// with the seam handing each fought night to the rules: they throw every die as ever and book the night's own
// counts in their place, inside each roll's run. A night the look-ahead did not see, or saw with other fleets in it,
// is settled by the dice. Only one copy is made a month: when the fleet actions' hand-back (the sea field's) joins
// this one, both seams are set around the same close.
//
// Off (a campaign without the flag 'convoyBattle'), nothing here offers anything and the seam is never asked.

import { CONVOY_BATTLE, convoyField, convoyRules, type ConvoyFought, type ConvoyMeeting } from './navy';
import { loadGame, saveGame } from './save';
import { passMonth } from './turn';
import type { Game } from './types';

/** A convoy's night the month will bring that the player can fight: the rules' own meeting, keyed. */
export interface ConvoyAhead { key: string; meeting: ConvoyMeeting }

/** The same night in the look-ahead and in the real close: the month and the route. */
export const convoyKey = (m: ConvoyMeeting): string => `${m.turn}:${m.route}`;

const ids = (xs: { id: number }[]): string => xs.map((x) => x.id).sort((a, b) => a - b).join();
const sameFleets = (a: ConvoyMeeting, b: ConvoyMeeting): boolean => ids(a.raiders) === ids(b.raiders) && ids(a.escorts) === ids(b.escorts);

/**
 * True when this player can fight this night (form section 1): submarines raid the route, and he has his own escorts
 * in its guard or his own boats among its raiders, on a route a hand may fight. Nothing of his own there, no offer.
 */
export function canFight(m: ConvoyMeeting, nat: string | null): boolean {
  if (!nat || !CONVOY_BATTLE.hand.includes(m.route) || !m.raiders.some((r) => r.ss > 0)) return false;
  return m.escorts.some((e) => e.nat === nat) || m.raiders.some((r) => r.nat === nat && r.ss > 0);
}

/** The month's close for the player's seat: his month, every computer's, then the start of his next (turn.ts passMonth). */
export const closeMonth = (g: Game): void => { passMonth(g); };

/**
 * The nights that closing the month will bring and the player can fight, found on a copy of the game. 'close' closes
 * the month on the copy it is given (closeMonth for the player's seat). Rolls nothing in g and changes nothing in it.
 */
export function convoyAhead(g: Game, close: (copy: Game) => void = closeMonth): ConvoyAhead[] {
  if (!convoyRules(g) || !g.human) return [];
  const copy = loadGame(saveGame(g));
  if (!copy) return [];
  const out: ConvoyAhead[] = [];
  const human = g.human, before = convoyField.fought;
  convoyField.fought = (m) => {
    if (canFight(m, human) && !out.some((a) => a.key === convoyKey(m))) out.push({ key: convoyKey(m), meeting: m });
    return undefined;
  };
  try { close(copy); } finally { convoyField.fought = before; }
  return out;
}

/** What the player fought, keyed by convoyKey: the night as the look-ahead saw it, and its result. */
export type ConvoyPlayed = Map<string, { meeting: ConvoyMeeting; fought: ConvoyFought }>;

/**
 * The seam's answer from what the player fought: a night is booked once, when the close reaches it with the same
 * fleets in it; every other route-month is the staff's. A screen that cannot wrap the close may leave this set as
 * convoyField.fought for the session: with nothing played it answers nothing.
 */
export function convoyPlayedHook(played: ConvoyPlayed): (m: ConvoyMeeting) => ConvoyFought | undefined {
  return (m) => {
    const k = convoyKey(m), p = played.get(k);
    if (!p) return undefined;
    played.delete(k);
    return sameFleets(p.meeting, m) ? p.fought : undefined;
  };
}

/** Closes the month on the game itself with each night the player fought booked from its result. The seam is put back as it was afterwards, whatever happens. */
export function convoyCloseWith(g: Game, played: ConvoyPlayed, close: (g: Game) => void = closeMonth): void {
  const before = convoyField.fought;
  convoyField.fought = convoyPlayedHook(played);
  try { close(g); } finally { convoyField.fought = before; }
}
