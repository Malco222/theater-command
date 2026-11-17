// What the battle sounds like: which of the sound director's recordings answer what a unit is doing and what
// happens on the field. Names are from /mnt/project-files/sound/sound-list.json (all original recordings). The
// screens play these; the sims only name them. TODO(polish): the sound director tunes this table and adds battle
// hooks of their own.

import type { Act, FieldEvent } from './types';

/** The loop a unit plays while it does this, by the kind of unit ('foot' for men, 'tracks' for tanks, 'wheels' for lorries, 'guns'). */
export const LOOPS: Record<'foot' | 'tracks' | 'wheels' | 'guns', Partial<Record<Act, string>>> = {
  foot: { march: 'marching_loop', run: 'marching_loop', fight: 'battle_loop' },
  tracks: { march: 'tank_loop', run: 'tank_loop', fight: 'battle_loop', turn: 'tank_loop' },
  wheels: { march: 'truck_engine', run: 'truck_engine', fight: 'battle_loop' },
  guns: { fight: 'far_guns_01' },
};

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
