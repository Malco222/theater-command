// The convoy night's hand-off with the campaign (ruling 241 §1, §2 and §4): the look-ahead that finds the nights the
// player can fight, each written as a field's ticket on the look-ahead's own copy of the game at the moment the rules
// note it, and the night fought handed back as the rules take it (convoyhand.ts books it in the real close).

import { canFight, closeMonth, convoyKey } from '../../campaign/convoyhand';
import { convoyField, convoyRules, type ConvoyFought, type ConvoyMeeting } from '../../campaign/navy';
import { loadGame, saveGame } from '../../campaign/save';
import type { Game } from '../../campaign/types';
import { foughtOf, ranksOf, restFrom, staffNights, type StaffNights } from './calibrate';
import { ConvoySim } from './sim';
import { convoyTicket } from './ticket';
import type { ConvoyRanks, ConvoyTicket } from './types';

/** A night the month will bring that the player can fight: the rules' meeting, keyed, and its field's ticket. */
export interface ConvoyNightAhead { key: string; meeting: ConvoyMeeting; ticket: ConvoyTicket }

/**
 * The nights closing the month will bring and the player can fight, found on a copy of the game (as convoyhand.ts
 * convoyAhead, with each night's ticket written on the copy as the rules note it). Rolls nothing in g and changes nothing in it.
 */
export function convoyNightsAhead(g: Game, close: (copy: Game) => void = closeMonth): ConvoyNightAhead[] {
  if (!convoyRules(g) || !g.human) return [];
  const copy = loadGame(saveGame(g));
  if (!copy) return [];
  const out: ConvoyNightAhead[] = [];
  const human = g.human, before = convoyField.fought;
  convoyField.fought = (m) => {
    if (canFight(m, human) && !out.some((a) => a.key === convoyKey(m))) out.push({ key: convoyKey(m), meeting: m, ticket: convoyTicket(copy, m, human) });
    return undefined;
  };
  try { close(copy); } finally { convoyField.fought = before; }
  return out;
}

/** A night ready to fight: its field, with the computer's own nights for the card to rank it against and to settle a handover. */
export function convoyNight(t: ConvoyTicket, nights = 30): { sim: ConvoySim; staff: StaffNights } {
  const staff = staffNights(t, nights);
  const sim = new ConvoySim(t);
  sim.rest = restFrom(staff, sim.limit);
  return { sim, staff };
}

/**
 * The night as the rules book it (navy.ts ConvoyFought), with its ranks for the after card. A night handed to the staff
 * before anything was fought is none: the month is the staff's, rolled exactly as the dice roll it (217(4)).
 */
export function nightBooked(sim: ConvoySim, staff: StaffNights): { fought: ConvoyFought; ranks: ConvoyRanks } | undefined {
  if (!sim.over) return undefined;
  if ((sim.over.extra as { handed?: boolean } | undefined)?.handed && sim.view().contact === null) return undefined;
  const ranks = ranksOf(sim.ticket, sim.counts, staff);
  return { fought: foughtOf(sim.ticket, ranks), ranks };
}
