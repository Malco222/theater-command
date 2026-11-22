// What a fleet action sounds like: the sound director's recordings (/mnt/project-files/sound/sound-list.json, all
// original) that answer what happens at sea. The sim names them on its events; the screens and the 3D game play them.
// TODO(polish): the sound director tunes this table (a torpedo_hit waits for the torpedo run).

import type { Act, FieldEvent } from '../core/types';

/** The one-shot that answers an event at sea. */
export const SEA_CUES: Partial<Record<FieldEvent['what'], string>> = {
  contact: 'ship_guns',
  ability: 'aircraft_takeoff',
  'sea:strike': 'dive_bomber',
  'sea:air': 'bomb_stick_01',
  'sea:sunk': 'ship_sinking',
  'sea:burning': 'fire_start',
  routing: 'ship_horn',
  broken: 'ship_horn',
  end: 'ship_bell',
};

/** The loop a ship plays while it does this. */
export const SEA_LOOPS: Partial<Record<Act, string>> = {
  idle: 'sea_day_loop',
  march: 'ship_loop',
  run: 'ship_loop',
  turn: 'ship_loop',
  rout: 'ship_loop',
  fight: 'sea_battle_loop',
};
