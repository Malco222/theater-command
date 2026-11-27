// Five practice nights beside the campaign's own (ruling 241 Q9; 217(5): no offer on the campaign screen until the
// Playtester finds them easy). Under Units decide (the design director's rider of 24 November) each is set by what the
// two sides bring, never by a year's row: the boats and their navy's habits, the escorts and the kit their navy has
// researched. The months only set the night's length and moon. Nothing a practice night does is booked.

import type { Side } from '../core/types';
import { plainConvoyTicket, type PlainConvoy } from './ticket';
import type { ConvoyKit, ConvoyTicket } from './types';

const kit = (radar: boolean, hedgehog: boolean, groups: boolean): ConvoyKit => ({ asdic: true, radar, hedgehog, groups });
const boatsKit = kit(false, false, false);

export interface ConvoyPractice extends PlainConvoy { about: string }

export const CONVOY_PRACTICE: ConvoyPractice[] = [
  { title: 'The first pack', about: 'Six boats homed onto a convoy with four escorts that have sonar and nothing else', sea: 'natl', year: 1940, month: 9,
    boats: [{ name: 'Pack', nat: 'GER', n: 6 }], escorts: [{ name: 'Escort group', nat: 'UK', n: 4 }], kit: { UK: kit(false, false, false), GER: boatsKit } },
  { title: 'Radar on the escorts', about: 'Eight boats against six escorts whose radar finds a boat on the surface in the dark', sea: 'wapp', year: 1941, month: 8,
    boats: [{ name: 'Pack', nat: 'GER', n: 8 }], escorts: [{ name: 'Escort group', nat: 'UK', n: 6 }], kit: { UK: kit(true, false, false), GER: boatsKit } },
  { title: 'A long winter night', about: 'Twelve boats against seven escorts through a November night of fifteen and a half to nearly eighteen hours of dark', sea: 'natl', year: 1942, month: 11,
    boats: [{ name: 'Pack', nat: 'GER', n: 12 }], escorts: [{ name: 'Escort group', nat: 'UK', n: 7 }], kit: { UK: kit(true, false, false), GER: boatsKit } },
  { title: 'The hunters trained', about: 'Fifteen boats against nine escorts with radar, ahead-throwers and group training', sea: 'natl', year: 1943, month: 3,
    boats: [{ name: 'Pack', nat: 'GER', n: 15 }], escorts: [{ name: 'Escort group', nat: 'UK', n: 9 }], kit: { UK: kit(true, true, true), GER: boatsKit } },
  { title: 'Hunted', about: 'Six boats against twelve escorts with every kit there is', sea: 'wapp', year: 1944, month: 1,
    boats: [{ name: 'Pack', nat: 'GER', n: 6 }], escorts: [{ name: 'Escort group', nat: 'UK', n: 12 }], kit: { UK: kit(true, true, true), GER: boatsKit } },
];

/** A practice night's ticket: the player has the escorts unless asked for the pack. */
export function convoyPracticeTicket(p: ConvoyPractice, seed: number, human: Side | null = 'def'): ConvoyTicket {
  return plainConvoyTicket(p, seed, human);
}
