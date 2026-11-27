// What a fleet action sounds like: the battle pack's real recordings (/mnt/project-files/sound/battle-pack/, approved by
// Brian 18 November; its README's table "The sea" follows this file) that answer what happens at sea. The sim names
// them on its events; the 3D game plays them. 'ship_sinking' passed Brian's ear at 14:49Z and now sounds. He cut
// 'ship_horn', so a fleet breaking off, and the fleet's signal, name no sound until a new horn passes his ear. The
// torpedo sounds he passed then answer the destroyers' torpedo run (ruling 239), for the sound director to confirm.

import type { Act, FieldEvent } from '../core/types';

/** The one-shot that answers an event at sea. */
export const SEA_CUES: Partial<Record<FieldEvent['what'], string>> = {
  contact: 'ship_guns',
  ability: 'aircraft_takeoff',
  'sea:strike': 'dive_bomber',
  'sea:air': 'bomb_stick_01',
  'sea:sunk': 'ship_sinking',
  'sea:burning': 'fire_start',
  'sea:torpedo': 'torpedo_launch',
  'sea:torpedoHit': 'torpedo_hit',
  // Fighters over the fleet meet a strike or a raid; its guns open on what is left (ruling 239, item 3 of the panel).
  'sea:cap': 'fighter_pass',
  'sea:flak': 'flak_light_fire',
  // A fleet breaking off: silent until a new horn passes Brian's ear (he cut 'ship_horn', 14:49Z).
  // The action ends: two bells (the README: "a fleet signals; action stations; the action ends").
  end: 'ship_bells_two',
};

/** The loop a zone plays while it lasts: a torpedo salvo's track runs from the launch to its aim point (the zone). */
export const SEA_ZONE_LOOPS: Partial<Record<string, string>> = { torpedo: 'torpedo_run_loop' };

/** The loop a ship plays while it does this: a warship under way is its engines (under its wash, which the ship models
 *  key for themselves). A destroyer's is its own wash: seaLoop. */
export const SEA_LOOPS: Partial<Record<Act, string>> = {
  idle: 'sea_day_loop',
  march: 'ship_engine_loop',
  run: 'ship_engine_loop',
  turn: 'ship_engine_loop',
  rout: 'ship_engine_loop',
  fight: 'sea_battle_loop',
};

/**
 * The loop one piece plays (the sound director, 14:59Z): a destroyer or escort under way is its own wash,
 * 'destroyer_wash_loop'; every other warship under way its engines; at rest and in the fight the same as any ship.
 * (A transport's 'ship_steam_engine_loop' waits for transports on the field.)
 */
export function seaLoop(cls: string | undefined, act: Act): string {
  const loop = SEA_LOOPS[act] ?? '';
  return loop === 'ship_engine_loop' && cls === 'dd' ? 'destroyer_wash_loop' : loop;
}
