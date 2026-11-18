// What a fleet action sounds like: the battle pack's real recordings (/mnt/project-files/sound/battle-pack/, approved by
// Brian 18 November; its README's table "The sea" follows this file) that answer what happens at sea. The sim names
// them on its events; the 3D game plays them. 'ship_sinking' passed Brian's ear at 14:49Z and now sounds. He cut
// 'ship_horn', so a fleet breaking off, and the fleet's signal, name no sound until a new horn passes his ear. The
// torpedo and depth charge sounds he passed then wait for orders the field does not have yet (the torpedo run).

import type { Act, FieldEvent } from '../core/types';

/** The one-shot that answers an event at sea. */
export const SEA_CUES: Partial<Record<FieldEvent['what'], string>> = {
  contact: 'ship_guns',
  ability: 'aircraft_takeoff',
  'sea:strike': 'dive_bomber',
  'sea:air': 'bomb_stick_01',
  'sea:sunk': 'ship_sinking',
  'sea:burning': 'fire_start',
  // A fleet breaking off: silent until a new horn passes Brian's ear (he cut 'ship_horn', 14:49Z).
  // The action ends: two bells (the README: "a fleet signals; action stations; the action ends").
  end: 'ship_bells_two',
};

/** The loop a ship plays while it does this: a warship under way is its engines (under its wash, which the ship models
 *  key for themselves, destroyers their own wash). */
export const SEA_LOOPS: Partial<Record<Act, string>> = {
  idle: 'sea_day_loop',
  march: 'ship_engine_loop',
  run: 'ship_engine_loop',
  turn: 'ship_engine_loop',
  rout: 'ship_engine_loop',
  fight: 'sea_battle_loop',
};
