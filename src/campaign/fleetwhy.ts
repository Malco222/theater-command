// The admirals' reasons (release 2; Brian, 24 November 2026 18:32Z: "the navy needs a lot of work and attention", and
// the plan sent to him that evening: "the admirals don't explain themselves"). Behind its own switch (stage.json
// 'fleetWhy', the flag 'fleetWhy'): off, nothing is written and nothing is shown.
//
// Where the player's admirals run the navy for him (depth.ts), each fleet they order this month carries one sentence
// saying why it is where it is: the sea and the reason that weighed most with them (navy.ts navalAI writes it, word for
// word as the choice was made). Like a note pinned to each ship's berth on the operations table. It changes nothing in
// the war; it only says what was already decided.

import type { Fleet, Game } from './types';

/** True where the admirals give their reasons: the grand campaign, with the switch on. */
export const fleetWhyOn = (g: Game): boolean => g.flags.open === 1 && g.flags.fleetWhy === 1;

/** Turns the admirals' reasons on or off in a campaign under way. */
export function setFleetWhy(g: Game, on: boolean): void {
  if (on) g.flags.fleetWhy = 1;
  else {
    delete g.flags.fleetWhy;
    for (const f of g.fleets) delete f.why;
  }
}

/** The admirals' reason for one of the player's fleets this month, or '' (none written, or his own orders rule it). */
export function fleetWhy(g: Game, f: Fleet): string {
  if (!fleetWhyOn(g) || f.nat !== g.human || f.ordered || !f.why || f.why[0] !== g.turn) return '';
  // (The UI director's 24 November note: no capital after the colon, so the sentence reads as the fleet's, not the
  // admirals'. The small-caps YOUR ADMIRALS label above the sentence, its fuller form, waits on the 3D panel.)
  const why = f.why[1];
  return `Your admirals: ${why.charAt(0).toLowerCase()}${why.slice(1)}`;
}
