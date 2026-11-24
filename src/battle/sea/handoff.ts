// The hand-back between the campaign and the field for fleet actions.
//
// The rules fight a month's fleet actions while they close a nation's month (turn.ts closePhase, navy.ts seaBattles),
// in among its air battles and the rest of the month's routine. So a screen that offers the player their fleet actions
// first looks ahead: it closes the month on a copy of the game, with the seam (navy.ts seaField) noting each action
// as the rules are about to fight it, and writes each one out as a ticket. The player fights the ones they want; then
// the month is closed on the game itself, with the seam handing each played action's result to the rules in place of
// the dice. An action the look-ahead did not see, or saw with other fleets in it (an action played earlier in the
// month can change what follows), is settled by the dice as before. With the seam unset nothing here runs at all.
//
// TODO(polish): ships that went out of the fight burning (SeaLosses.burnt) come home afloat; the rules could take
// them in for repair (their fleet's readiness) once the design director rules on it.

import { seaField, type SeaFought, type SeaMeeting } from '../../campaign/navy';
import { loadGame, saveGame } from '../../campaign/save';
import type { Game } from '../../campaign/types';
import type { FieldResult } from '../core/types';
import type { SeaLosses } from './sim';
import { fieldFights, seaTicket } from './ticket';
import { SEA_CLASSES, type SeaShips, type SeaTicket } from './types';

/** A fleet action the month will bring: the rules' own meeting, and the ticket the field is built from. */
export interface SeaAhead { key: string; meeting: SeaMeeting; ticket: SeaTicket }

/** The same action in the look-ahead and in the real close: the month, the sea, and the nation that sought it. */
export const meetingKey = (m: SeaMeeting): string => `${m.turn}:${m.zone}:${m.nat}`;

const sameFleets = (a: number[], b: number[]): boolean => a.length === b.length && [...a].sort((x, y) => x - y).join() === [...b].sort((x, y) => x - y).join();

/**
 * The fleet actions that closing the month will bring and the field can fight, found on a copy of the game. 'close'
 * closes the month on the copy it is given (for the player's own month, (c) => endHuman(c); for a computer's,
 * (c) => stepAI(c)). Rolls no dice and changes nothing in g. An action with no gun ship on one side is left to the
 * staff: the close settles it by the dice, as every action not played is.
 */
export function seaAhead(g: Game, close: (copy: Game) => void): SeaAhead[] {
  const copy = loadGame(saveGame(g));
  if (!copy) return [];
  const out: SeaAhead[] = [];
  const before = seaField.fought;
  seaField.fought = (m) => {
    const ticket = seaTicket(copy, m);
    // One the field cannot fight (a side with no gun ship) is settled by the staff, not offered (b438).
    if (fieldFights(ticket)) out.push({ key: meetingKey(m), meeting: m, ticket });
    return undefined;
  };
  try { close(copy); } finally { seaField.fought = before; }
  return out;
}

/** A played battle's result as the rules take it: the ratio, the ships each fleet lost by class, and whether the dice finished it. */
export function foughtOf(r: FieldResult): SeaFought {
  const x = r.extra as SeaLosses | undefined;
  return { R: r.R, sunk: x?.abandoned ? lostOf(x) : x?.sunk ?? {}, toDice: x?.toDice, ...(x?.kept ? { kept: x.kept } : {}) };
}

/** The ships each fleet lost with men in the water (release 2's sailor rules): sunk, and those its own call to abandon
 *  early lost past the field's limit (form C3), which the campaign counts against the fleet as it counts a ship sunk. */
function lostOf(x: SeaLosses): Record<number, Partial<SeaShips>> {
  const out: Record<number, Partial<SeaShips>> = {};
  for (const id of new Set([...Object.keys(x.sunk), ...Object.keys(x.abandoned ?? {})].map(Number))) {
    const a = x.sunk[id], b = x.abandoned?.[id];
    out[id] = Object.fromEntries(SEA_CLASSES.map((c) => [c, (a?.[c] ?? 0) + (b?.[c] ?? 0)]));
  }
  return out;
}

/** What the player fought, keyed by meetingKey: the meeting as the look-ahead saw it, and its result. */
export type SeaPlayed = Map<string, { meeting: SeaMeeting; fought: SeaFought }>;

/**
 * Closes the month on the game itself ('close', as for seaAhead), with each action the player fought settled by its
 * result instead of the dice. The seam is put back as it was afterwards, whatever happens.
 */
export function closeWith(g: Game, played: SeaPlayed, close: (g: Game) => void): void {
  const before = seaField.fought;
  seaField.fought = playedHook(played);
  try { close(g); } finally { seaField.fought = before; }
}

/**
 * The seam's answer from what the player fought: an action is booked from the field once, when the close reaches it
 * with the same fleets in it, and every other action rolls the dice. A screen that cannot wrap the close (the 3D
 * game's month end) may leave this set as navy.ts seaField.fought for the whole session: with nothing played, it
 * answers nothing.
 */
export function playedHook(played: SeaPlayed): (m: SeaMeeting) => SeaFought | undefined {
  return (m) => {
    const k = meetingKey(m), p = played.get(k);
    if (!p) return undefined;
    played.delete(k);
    return sameFleets(p.meeting.mine, m.mine) && sameFleets(p.meeting.met, m.met) ? p.fought : undefined;
  };
}
