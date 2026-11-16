# Licence log

**Every sound in this set is original.** Each was synthesised from mathematical tones and random noise by our own program (`sound/make_sounds.mjs` and the recipes beside it), written for this game in November 2026. No recording, sample, library sound, impulse response, soundfont or music from any other source is in any of them, and nothing was copied from any game, film or record.

| Question | Answer |
|---|---|
| Whose they are | The project's. They were made for this game, and no third party's material is in them. |
| Licence needed to ship them | None. There is no third-party material to license and no credit owed. |
| Third-party sounds used | None. |
| Third-party code used | None. The generator uses only what comes with Node.js (`node:fs`, `node:path`). The MP3 copies are packed with ffmpeg and LAME; that packs our audio and adds none of theirs. |
| The echoes | Made by the generator from filtered noise (`makePlace` in `kit/dsp.mjs`). No measured or bought impulse responses. |
| The tunes | The band calls are our own, built on one five-note figure (F, B flat, C, A, D) written for this game. They use notes a bugle cannot play, so none can be a real army's call; no anthem, march, hymn or film tune is quoted. The Morse in `radio` spells the game's own initials. |
| Real things imitated | Only kinds of thing (a radial engine, a steam whistle, an air raid siren's wail, a 1940s telephone bell), which nobody owns. No maker's recording of any of them was used or analysed. |
| Can they be made again | Yes, exactly. Every sound's random numbers are seeded from its name, so the same program always writes the same files. |

**Sources looked at and not used.** The brief allowed CC0 libraries. The free libraries were not reachable from the workshop (the network policy refuses them), so none was downloaded, listened to or used. If CC0 recordings are added later, each one must get a row here with its address, its author, the date fetched and the text of its licence, and it must be checked that the licence really is CC0 and not "free for non-commercial use".

**What has not been checked.** The tunes were written to be unlike any call or tune the sound director knows, but nobody has yet listened to them: a person's ear should confirm that none sounds like something famous by accident. That check is Brian's listening pass (gate 7, item 2).

## Every file

| File | Family | Source | Recipe | Third-party material | Licence |
|---|---|---|---|---|---|
| `bomber_heavy_loop.wav` | air | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `bomber_medium_loop.wav` | air | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `fighter_loop.wav` | air | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `fighter_radial_loop.wav` | air | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `fighter_pass_01.wav` | air | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `fighter_pass_02.wav` | air | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `fighter_pass.wav` | air | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `bomber_drone.wav` | air | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `dive_bomber.wav` | air | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `aircraft_takeoff.wav` | air | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `dogfight.wav` | air | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `plane_down.wav` | air | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `bombs_falling_01.wav` | air | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `bombs_falling_02.wav` | air | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `bomb_stick_01.wav` | guns | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `bomb_stick_02.wav` | guns | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `bomb_stick_03.wav` | guns | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `bomb_single_01.wav` | guns | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `bomb_single_02.wav` | guns | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `air_raid_siren.wav` | calls | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `siren.wav` | calls | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `siren_all_clear.wav` | calls | Original synthesis | `recipes/air.mjs` | none | none needed (original) |
| `flak_heavy_fire_01.wav` | flak | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `flak_heavy_fire_02.wav` | flak | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `flak_heavy_fire_03.wav` | flak | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `flak_burst_01.wav` | flak | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `flak_burst_02.wav` | flak | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `flak_burst_03.wav` | flak | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `flak_burst_04.wav` | flak | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `flak_light_01.wav` | flak | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `flak_light_02.wav` | flak | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `flak_barrage_loop.wav` | flak | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `flak_wagon_fire_01.wav` | flak | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `flak_wagon_fire_02.wav` | flak | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `searchlight_on.wav` | flak | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `searchlight_loop.wav` | flak | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `searchlight_off.wav` | flak | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `searchlight_caught.wav` | flak | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `artillery_fire_01.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `artillery_fire_02.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `artillery_fire_03.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `field_gun.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `artillery_salvo_01.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `artillery_salvo_02.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `shell_incoming_01.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `shell_incoming_02.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `shell_impact_01.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `shell_impact_02.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `shell_impact_03.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `barrage_loop.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `rifles.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `rifles_02.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `machine_gun.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `machine_gun_02.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `tank_gun.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `tank_gun_02.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `explosion.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `explosion_02.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `explosion_03.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `bridge_blown.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `battle.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `battle_loop.wav` | guns | Original synthesis | `recipes/guns.mjs` | none | none needed (original) |
| `tank_engine.wav` | ground | Original synthesis | `recipes/ground.mjs` | none | none needed (original) |
| `tank_loop.wav` | ground | Original synthesis | `recipes/ground.mjs` | none | none needed (original) |
| `truck_engine.wav` | ground | Original synthesis | `recipes/ground.mjs` | none | none needed (original) |
| `halftrack.wav` | ground | Original synthesis | `recipes/ground.mjs` | none | none needed (original) |
| `convoy_loop.wav` | ground | Original synthesis | `recipes/ground.mjs` | none | none needed (original) |
| `marching.wav` | ground | Original synthesis | `recipes/ground.mjs` | none | none needed (original) |
| `marching_loop.wav` | ground | Original synthesis | `recipes/ground.mjs` | none | none needed (original) |
| `horses.wav` | ground | Original synthesis | `recipes/ground.mjs` | none | none needed (original) |
| `horses_loop.wav` | ground | Original synthesis | `recipes/ground.mjs` | none | none needed (original) |
| `dig_in.wav` | ground | Original synthesis | `recipes/ground.mjs` | none | none needed (original) |
| `hammer.wav` | ground | Original synthesis | `recipes/ground.mjs` | none | none needed (original) |
| `build_works.wav` | ground | Original synthesis | `recipes/ground.mjs` | none | none needed (original) |
| `factory_loop.wav` | ground | Original synthesis | `recipes/ground.mjs` | none | none needed (original) |
| `parachutes.wav` | ground | Original synthesis | `recipes/ground.mjs` | none | none needed (original) |
| `train_loop.wav` | rail | Original synthesis | `recipes/rail.mjs` | none | none needed (original) |
| `train.wav` | rail | Original synthesis | `recipes/rail.mjs` | none | none needed (original) |
| `train_depart.wav` | rail | Original synthesis | `recipes/rail.mjs` | none | none needed (original) |
| `train_arrive.wav` | rail | Original synthesis | `recipes/rail.mjs` | none | none needed (original) |
| `train_turn_round.wav` | rail | Original synthesis | `recipes/rail.mjs` | none | none needed (original) |
| `train_whistle_01.wav` | rail | Original synthesis | `recipes/rail.mjs` | none | none needed (original) |
| `train_whistle_02.wav` | rail | Original synthesis | `recipes/rail.mjs` | none | none needed (original) |
| `rail_ride.wav` | rail | Original synthesis | `recipes/rail.mjs` | none | none needed (original) |
| `rail_cut.wav` | rail | Original synthesis | `recipes/rail.mjs` | none | none needed (original) |
| `rail_mended.wav` | rail | Original synthesis | `recipes/rail.mjs` | none | none needed (original) |
| `ship_horn.wav` | sea | Original synthesis | `recipes/sea.mjs` | none | none needed (original) |
| `ship_horn_02.wav` | sea | Original synthesis | `recipes/sea.mjs` | none | none needed (original) |
| `ship_bell.wav` | desk | Original synthesis | `recipes/sea.mjs` | none | none needed (original) |
| `ship_guns.wav` | sea | Original synthesis | `recipes/sea.mjs` | none | none needed (original) |
| `ship_guns_02.wav` | sea | Original synthesis | `recipes/sea.mjs` | none | none needed (original) |
| `shore_bombardment.wav` | sea | Original synthesis | `recipes/sea.mjs` | none | none needed (original) |
| `ship_loop.wav` | sea | Original synthesis | `recipes/sea.mjs` | none | none needed (original) |
| `anchor_chain.wav` | sea | Original synthesis | `recipes/sea.mjs` | none | none needed (original) |
| `sonar_ping.wav` | sea | Original synthesis | `recipes/sea.mjs` | none | none needed (original) |
| `depth_charge.wav` | sea | Original synthesis | `recipes/sea.mjs` | none | none needed (original) |
| `torpedo_hit.wav` | sea | Original synthesis | `recipes/sea.mjs` | none | none needed (original) |
| `ship_sinking.wav` | sea | Original synthesis | `recipes/sea.mjs` | none | none needed (original) |
| `submarine_dive.wav` | sea | Original synthesis | `recipes/sea.mjs` | none | none needed (original) |
| `landing_craft.wav` | sea | Original synthesis | `recipes/sea.mjs` | none | none needed (original) |
| `carrier_launch.wav` | sea | Original synthesis | `recipes/sea.mjs` | none | none needed (original) |
| `sea_battle_loop.wav` | sea | Original synthesis | `recipes/sea.mjs` | none | none needed (original) |
| `fire_small_loop.wav` | fire | Original synthesis | `recipes/fire.mjs` | none | none needed (original) |
| `fire_town_loop.wav` | fire | Original synthesis | `recipes/fire.mjs` | none | none needed (original) |
| `fire_start.wav` | fire | Original synthesis | `recipes/fire.mjs` | none | none needed (original) |
| `fire_collapse_01.wav` | fire | Original synthesis | `recipes/fire.mjs` | none | none needed (original) |
| `fire_collapse_02.wav` | fire | Original synthesis | `recipes/fire.mjs` | none | none needed (original) |
| `click.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `ui_primary.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `ui_dial.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `tick.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `ui_back.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `ui_tab.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `switch_on.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `switch_off.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `ui_error.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `ui_notify.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `card_open.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `card_close.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `page_turn.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `screen_open.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `screen_close.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `chart_unroll.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `stamp.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `stamp_low.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `stamp_light.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `pickup.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `putdown.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `order_march.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `order_attack.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `order_cancel.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `select_region.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `coins.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `coin_run.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `alert_slip.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `pen_sign.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `typewriter.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `type_key.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `teleprinter.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `telephone.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `radio.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `projector.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `month_ask.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `month_end.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `month_passing_loop.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `month_card_open.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `note_arrives.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `note_done.wav` | desk | Original synthesis | `recipes/desk.mjs` | none | none needed (original) |
| `bell.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `bugle.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `victory.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `defeat.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `drums.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `newsreel.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `campaign_start.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `war_declared.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `treaty_signed.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `city_taken.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `city_lost.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `nation_falls.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `nation_freed.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `world_won.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `campaign_lost.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `month_card_good.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `month_card_bad.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `month_card_quiet.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `month_yours.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `alert.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `research_done.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `medal.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `drum_low.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `unit_lost.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `note_closing.wav` | calls | Original synthesis | `recipes/calls.mjs` | none | none needed (original) |
| `room_day_loop.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `room_night_loop.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `land_day_loop.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `land_night_loop.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `winter_loop.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `desert_loop.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `jungle_loop.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `rain_loop.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `sea_day_loop.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `sea_night_loop.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `front_loop.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `bird_01.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `bird_02.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `bird_03.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `bird_04.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `owl_01.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `owl_02.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `gull_01.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `gull_02.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `bell_buoy.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `church_bell_far.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `far_guns_01.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `far_guns_02.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `far_guns_03.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `thunder_01.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `thunder_02.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `room_typing_01.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `room_typing_02.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `room_steps.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `room_papers.wav` | beds | Original synthesis | `recipes/beds.mjs` | none | none needed (original) |
| `scene_night_raid.wav` | target scene | A mix of the files above | `make_scenes.mjs` | none | none needed (original) |
| `scene_day_raid.wav` | target scene | A mix of the files above | `make_scenes.mjs` | none | none needed (original) |
| `scene_march_and_train.wav` | target scene | A mix of the files above | `make_scenes.mjs` | none | none needed (original) |
| `scene_month_end.wav` | target scene | A mix of the files above | `make_scenes.mjs` | none | none needed (original) |
| `scene_attack.wav` | target scene | A mix of the files above | `make_scenes.mjs` | none | none needed (original) |
| `scene_fleet.wav` | target scene | A mix of the files above | `make_scenes.mjs` | none | none needed (original) |
| `raid_day.wav` | sting | A mix of the files above | `make_scenes.mjs` | none | none needed (original) |
| `raid_night.wav` | sting | A mix of the files above | `make_scenes.mjs` | none | none needed (original) |
| `rail_move.wav` | sting | A mix of the files above | `make_scenes.mjs` | none | none needed (original) |
| `scene_first_command.wav` | target scene | A mix of the files above | `make_scenes.mjs` | none | none needed (original) |
