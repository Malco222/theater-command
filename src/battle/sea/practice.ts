// The practice actions offered beside the campaign's own: fleets the campaign has, in seas it names. Their odds are the
// campaign's own reckoning of these fleets and that water, never set by hand (the design director, b402), so a newcomer's
// first actions keep the card's promise; tests/battle-sea-obvious.test.ts plays each as a newcomer would.
import type { Side } from '../core/types';
import { plainTicket, type PlainFleet } from './ticket';
import type { SeaTicket } from './types';

export interface Practice { name: string; about: string; sea: string; att: PlainFleet[]; def: PlainFleet[]; air?: Partial<Record<Side, number>> }
// Nations and fleets the campaign has, in seas it names, so a practice action reads as the campaign's own would. The
// odds are the campaign's own reckoning of these fleets and that water, not set by hand (the design director, b402):
// the fleets are chosen to come out at about 1.00, 1.09, 1.16, 0.86 and 1.05 to 1. The last is many light ships
// against a few heavy ones, the make-up the field finds hardest to honour (the design director, b438).
export const PRACTICE: Practice[] = [
  { name: 'Two battle lines', about: 'Battleships, cruisers and destroyers on both sides, at even odds', sea: 'North Sea',
    att: [{ name: 'Home Fleet', nat: 'UK', ships: { bb: 3, ca: 4, dd: 8 }, admiral: 1 }], def: [{ name: 'Battle Fleet', nat: 'GER', ships: { bb: 2, ca: 5, dd: 15 }, admiral: 1 }] },
  { name: 'Carriers against a battle line', about: 'Your carriers strike from behind their screen; theirs is all guns', sea: 'Ionian Sea',
    att: [{ name: 'Mediterranean Fleet', nat: 'UK', ships: { cv: 3, ca: 5, dd: 12 } }], def: [{ name: '1st Fleet', nat: 'ITA', ships: { bb: 3, ca: 2, dd: 8 } }] },
  { name: 'Under the enemy\'s aircraft', about: 'Their coast is near and its aircraft raid your ships all through the action', sea: 'Tyrrhenian Sea', air: { def: 15 },
    att: [{ name: 'Mediterranean Fleet', nat: 'UK', ships: { bb: 3, cv: 1, ca: 5, dd: 12 } }], def: [{ name: '2nd Fleet', nat: 'ITA', ships: { bb: 2, ca: 4, dd: 8 } }] },
  { name: 'Outgunned', about: 'They are the stronger: break off well, or make them pay', sea: 'North Sea',
    att: [{ name: 'Home Fleet', nat: 'UK', ships: { bb: 2, ca: 4, dd: 10 } }], def: [{ name: 'Battle Fleet', nat: 'GER', ships: { bb: 3, ca: 3, dd: 10 } }] },
  { name: 'Light ships against battleships', about: 'Many cruisers and destroyers against a few battleships: close in and wear them down', sea: 'Central Mediterranean',
    att: [{ name: 'Force H', nat: 'UK', ships: { ca: 6, dd: 22 } }], def: [{ name: '1st Fleet', nat: 'ITA', ships: { bb: 3, ca: 2, dd: 2 } }] },
];

/**
 * A practice action's ticket, with no game behind it: the field is headed by the card's own name ("Two battle lines"),
 * as the land's practice field is, not by the sea's (b458, 5); the sea it is fought in stays in the opening dispatch,
 * and the facts line under the title says "Practice" and the fleets. The player has the attacking side unless asked.
 */
export function practiceTicket(p: Practice, seed: number, human: Side | null = 'att'): SeaTicket {
  const t = plainTicket({ att: p.att, def: p.def, air: p.air, seed, human, zoneName: p.sea, title: p.name });
  t.date = 'Practice'; // Not a month of the war: these fleets never met like this.
  return t;
}
