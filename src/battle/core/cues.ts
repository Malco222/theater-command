// What the battle sounds like: which of the sound director's recordings answer what a unit is doing and what
// happens on the field. Names are the approved battle pack's (/mnt/project-files/sound/battle-pack/, its "The field"
// table), with the generator's list (/mnt/project-files/sound/sound-list.json) where the pack has nothing yet. The 3D
// game plays the pack's own files; the sims and the bridge only name them. TODO(polish): the sound director tunes
// this table and adds battle hooks of their own.

import type { Act, FieldEvent } from './types';

/** The loop a unit plays while it does this, by the kind of unit ('foot' for men, 'tracks' for tanks, 'wheels' for lorries, 'guns'). */
export const LOOPS: Record<'foot' | 'tracks' | 'wheels' | 'guns', Partial<Record<Act, string>>> = {
  foot: { march: 'marching_loop', run: 'marching_loop', fight: 'battle_loop' },
  tracks: { march: 'tank_loop', run: 'tank_loop', fight: 'battle_loop', turn: 'tank_loop' },
  // The pack's lorry: the engine holding its revs on the move, idling where it stands with nothing to do.
  wheels: { idle: 'lorry_idle_loop', march: 'lorry_drive_loop', run: 'lorry_drive_loop', fight: 'battle_loop' },
  // Guns in action have no loop: they fire one far salvo at a time (DROPS), as the pack asks.
  guns: {},
};

/** Sounds dropped in one at a time at a random gap, rather than looped: a take from the list every `every` seconds. */
export interface Drops { takes: readonly string[]; every: readonly [number, number] }

/** Guns in action: one far salvo every 6 to 20 s, a different take from the last (the pack's "The field" table). */
export const SALVOS: Drops = { takes: ['far_guns_01', 'far_guns_02', 'far_guns_03'], every: [6, 20] };

/** What a unit drops in while it does this, by the kind of unit, beside or instead of its loop. */
export const DROPS: Partial<Record<keyof typeof LOOPS, Partial<Record<Act, Drops>>>> = {
  guns: { fight: SALVOS },
};

/**
 * A fight in a town: the street battle in place of the open field's bed, with the town's machine guns dropped in. The gap
 * is the builder's first guess, for the sound director to tune; the pack names the sounds but not the gap.
 */
export const TOWN = { loop: 'street_battle', drops: { takes: ['mg_town_01', 'mg_town_02', 'mg_town_03'], every: [4, 12] } as Drops };

/**
 * One take from several, at random and never the one played last (the pack: "play one at random and never the same one
 * twice running"). `roll` is a number from 0 to under 1; a list of one always gives that one.
 */
export function nextTake(takes: readonly string[], last: string | null, roll: number): string {
  const pool = takes.length > 1 && last !== null && takes.includes(last) ? takes.filter((t) => t !== last) : takes;
  return pool[Math.min(pool.length - 1, Math.floor(roll * pool.length))];
}

/** The gap before the next drop, in seconds, from a roll from 0 to under 1. */
export function nextGap(d: Drops, roll: number): number {
  return d.every[0] + (d.every[1] - d.every[0]) * roll;
}

/** The one-shot that answers an event, where the event does not name its own. */
export const ONE_SHOTS: Partial<Record<FieldEvent['what'], string>> = {
  contact: 'rifles',
  flanked: 'machine_gun',
  routing: 'bugle',
  broken: 'bugle',
  end: 'battle',
};

/** The sound of an order given, as on the campaign map. */
export const ORDER_CUES = { move: 'order_march', attack: 'order_attack', hold: 'order_cancel', withdraw: 'order_cancel' } as const;
