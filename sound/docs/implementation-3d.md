# Sound in the 3D game: the plan for the 3D builder

Sound director, 16 November 2026. To be relayed by the coordinator. Read `sound-brief.md` first for the why; this file is the how. Brian's words: "we definitely need great sound effects". This is gate 7 of the release 1 finish line.

Everything named here was read in the `unreal` branch at c287f46: `Campaign/CampaignSounds.h` and `.cpp`, `CampaignPlayerController.cpp`, `CampaignMapActor.cpp`, `CampaignGameMode.cpp`, `Tools/make_sounds.mjs`, `Tools/build_sounds.sh`, `Tools/build_sounds.py` and the launcher. Where this plan names an Unreal class or setting, the name is from memory: check it against the 5.8 headers before leaning on it.

**The steps are in order of worth. Each one ships by itself.** Step 1 needs no C++ and upgrades every sound the game already plays.

| Step | What | Size | What the player gets |
|---|---|---|---|
| 1 | Swap the generator | Small, no C++ | All 29 old events sound new; `pickup` and `train` stop being silent |
| 2 | Set the music under the effects; one Effects volume | Small | The new sounds can be heard; gate 7's two volumes |
| 3 | Sounds placed on the map; loops; rotating voices | Medium | Left and right, near and far; nothing twice running |
| 4 | The ambience: beds, sprinkles, day and night | Medium | The war room with a window |
| 5 | The raid, cue by cue | Medium | Day told from night with the eyes shut |
| 6 | Trains, armies and fleets on the move | Medium | A march told from a train |
| 7 | The month and the cards | Small | Every month's end has its sounds |
| 8 | Fires, gun lines, sieges, great moments, First Command | Small each | The rest of the list |

## Step 1. Swap the generator

The new generator is a folder, not one file. It is on the branch `claude/project-thread-qjxyv5` under `sound/`, and a copy is in the project files at `/mnt/project-files/sound/generator/`.

1. Copy that folder to `unreal/TheaterCommand/Tools/sound/` (it holds `make_sounds.mjs`, `make_scenes.mjs`, `kit/`, `recipes/`, `tools/`). Delete the old `Tools/make_sounds.mjs`.
2. In `Tools/build_sounds.sh`, run `node "$HERE/sound/make_sounds.mjs" "$PROJECT/Saved/Sounds"`, then `node "$HERE/sound/make_scenes.mjs" "$PROJECT/Saved/Sounds" "$PROJECT/Saved/Sounds"` (this adds the three stings `raid_day`, `raid_night` and `rail_move`; it also writes seven `scene_*.wav` files, which are for listening only: delete them or skip them on import).
3. The launcher and `build_sounds.sh` both decide whether to make the sounds again from `shasum Tools/make_sounds.mjs`. Make that the hash of the whole folder: `cat Tools/sound/*.mjs Tools/sound/*/*.mjs | shasum`.
4. In `Tools/build_sounds.py`, after the import, set every sound whose name ends in `_loop` to loop (`set_editor_property("looping", True)` on the sound wave, then save the asset). Skip names that begin `scene_`.
5. Making all 203 takes about two minutes on the cloud machine and should be quicker on the Mac. It needs only Node.js, as before, and it writes the same files every time.

All 29 names the old generator made are in the new set, with the same meaning, so no call in the game changes. `pickup` and `train`, which the controller plays today and which were never made, now exist.

**One thing changes that the builder must know about: the new files are quieter, on purpose.** The old sounds were each pushed to full scale; the new ones are levelled by family (a click is far softer than a gun) and leave headroom. See step 2.

## Step 2. Loudness: the music, and an Effects volume

**The music today would drown the new sounds.** The theme measures -10 LUFS as a file (measured from `leave-pass-swing-soundtrack.m4a`), and `MusicVolume` is 0.55, so it plays near -15 LUFS. The new desk sounds are made at -27 and a click at -30. Left as it is, the music would sit 12 to 15 dB over every button.

1. Measure the three pieces: `node Tools/sound/tools/measure.mjs Saved/Music/*.wav`. It prints each file's loudness and the volume number that brings it to -27 LUFS.
2. Set the music so that it plays at about -27 LUFS under play (-30 for the quiet map-room piece). For the theme that is a volume near 0.15, not 0.55. The opening screen can keep it louder (-20).
3. In the existing calls to `CampaignSounds::Play`, the volume numbers (0.5 to 0.7) were tuned to the old files. Set them all to 1.0: the files now carry the balance between a click and a gun.
4. Add one number, `EffectsVolume` (0 to 1, default 1), to `CampaignSounds`, multiplied into every play, with `SetEffectsVolume` and a control beside the music's. Keep the N key (on and off) and the M key. This is gate 7's "effects and music each have their own volume".
5. Never play an effect above 1.0: the loudest files peak at -1 dB and would clip.

Then the ducking, in `CampaignGameMode` beside the existing `DuckMusic`:

| When | What gives way | By how much |
|---|---|---|
| The narrator speaks | World and ambience | -6 dB; a call waits until the voice has finished |
| A call plays (any sound of the Calls family) | Music | -10 dB for the call's length, half a second in and one second out |
| A raid or a battle is on screen | Music -6 dB, ambience -4 dB | While it lasts |
| A card is open | World -4 dB; no sprinkles | While it is open |

## Step 3. Sounds on the map

Today every sound goes through `UGameplayStatics::PlaySound2D`: flat, in the middle of the head. World sounds should come from where they happen.

**Add to `CampaignSounds`:**

```cpp
// A sound at a place on the map. Reach says how far it carries: 1 for small things, 1.6 for bombs, big guns and sirens.
void PlayAt(const UObject* World, const TCHAR* Name, const FVector& Where, float Volume = 1.0f, float Pitch = 1.0f, float Reach = 1.0f);

// A sound that goes round, carried by something (a flight, a train, a fire). Returns a handle.
int32 StartLoop(const UObject* World, const TCHAR* Name, const FVector& Where, float Volume = 1.0f, float FadeIn = 0.5f, float Reach = 1.0f);
void MoveLoop(int32 Handle, const FVector& Where);
void SetLoopVolume(int32 Handle, float Volume, float Fade = 0.3f);
void StopLoop(int32 Handle, float FadeOut = 1.0f);

// "flak_burst", 4 -> "flak_burst_03": one of the numbered voices, never the one used last time.
const TCHAR* Voice(const TCHAR* Base, int32 Count);

// Called once a frame with where the camera looks and how far away it is.
void SetEar(const FVector& CameraFocus, float CameraDistance, float CameraYaw);
```

**How loud a placed sound is.** Do the arithmetic in our own code and let the engine do only the left and right. Unreal's own distance falloff measures from the camera, which is far above the map, and would make everything faint.

```
Zoom     = 1 - smoothstep(4000, 16000, CameraDistance)        // 1 low over the map, 0 high in the room
WorldBus = lerp(0.25, 1.0, Zoom)                              // -12 dB high, 0 dB low
r        = Dist2D(Where, CameraFocus) / max(CameraDistance, 2600) / Reach
Falloff  = 1 - smoothstep(0.25, 1.6, r)                       // full near the middle of the screen, gone well off it
Gain     = Volume * EffectsVolume * WorldBus * Falloff * Duck
```

- If `Gain` is under 0.02, do not play the sound at all.
- For left and right, spawn it with `UGameplayStatics::SpawnSoundAtLocation` and an attenuation that spatialises but does not attenuate, and put the listener on the map at the camera's focus with the camera's yaw (`APlayerController::SetAudioListenerOverride`). Then a thing on the left of the screen sounds from the left.
- For a loop, keep the audio component, and update its place and its gain every frame from the same formula.
- The world files are mono so that this works. The stereo ones (`battle`, `battle_loop`, `sea_battle_loop`, `bomber_drone`, `dogfight`, `fire_town_loop`) can be placed too, or played flat with the gain above.

**Rotation, so nothing is heard twice running** (brief, section 6):

- `Voice()` for the numbered families: `flak_heavy_fire` 3, `flak_burst` 4, `flak_light` 2, `flak_wagon_fire` 2, `artillery_fire` 3, `artillery_salvo` 2, `shell_incoming` 2, `shell_impact` 3, `bombs_falling` 2, `bomb_stick` 3, `bomb_single` 2, `fighter_pass` 2 (plus the plain `fighter_pass`), `train_whistle` 2, `fire_collapse` 2.
- Every `PlayAt`: pitch times a random 0.97 to 1.03, volume times a random 0.85 to 1.0.
- Every `StartLoop`: begin at a random point of the file, pitch times a random 0.96 to 1.04.
- At most 2 of the same loop and 12 placed sounds at once. When full, the one with the lowest gain gives way.
- The same one-shot is not started twice within 125 ms (the UI director's rule: never faster than eight a second).

**A startup check.** `Find()` already logs a sound that has not been made, but only when it is first asked for. Add a list of every name the code uses and check them all when the campaign opens, so a misspelt name shows in the log at once and not in the middle of a raid.

## Step 4. The ambience

A small manager, ticked from the game mode, holding two beds and a sprinkle timer.

1. **The room bed.** `room_day_loop`, or `room_night_loop` when night is on. Gain `cos(Zoom * 90 degrees)`: full when the camera is high.
2. **The outdoor bed.** Chosen by what is under `CameraFocus`. Gain `sin(Zoom * 90 degrees)`: full when the camera is low. Cross over in two seconds when the choice changes.

| Under the camera | By day | By night |
|---|---|---|
| Sea | `sea_day_loop` | `sea_night_loop` |
| Land under snow (`SetSeason` winter, in the north) | `winter_loop` | `winter_loop` |
| Desert | `desert_loop` | `desert_loop` at -4 dB |
| Jungle (Burma, the islands) | `jungle_loop` | `jungle_loop` |
| Any other land | `land_day_loop` | `land_night_loop` |

3. **The front.** If the province under the camera, or one beside it, has armies of two sides facing each other, lay `front_loop` over the outdoor bed at -3 dB.
4. **Night.** Night is on when `ToggleDayNight` says so (the same value that drives `SetNight` and `GetMoonlight`). Changing it plays `switch_off` (to night) or `switch_on` (to day) and crosses the beds over in two seconds.
5. **Sprinkles.** Every 6 to 20 seconds (random), when no card is open and `Zoom` is over 0.3, play one sprinkle from the list for the bed that is playing, at a random place within the screen, never the one played last:

| Bed | Sprinkles |
|---|---|
| `land_day_loop` | `bird_01`, `bird_02`, `bird_03`, `bird_04`; `church_bell_far` rarely, and only in a country at peace |
| `land_night_loop` | `owl_01`, `owl_02` |
| `sea_day_loop` | `gull_01`, `gull_02` |
| `sea_night_loop` | `bell_buoy` |
| `front_loop` | `far_guns_01`, `far_guns_02`, `far_guns_03` |
| The room bed (when `Zoom` is under 0.3), by day | `room_typing_01`, `room_typing_02`, `room_steps`, `room_papers` |
| The room bed, by night | `room_steps` rarely |

6. **Weather, later** (ruling 127): `rain_loop` over the outdoor bed, `thunder_01` and `thunder_02` as its sprinkles, and no birds in the rain.

## Step 5. The raid

`FlySortie` starts a flight that lasts 11 seconds; `UpdateSorties` moves it. The flight's share runs from 0 to 1: out until 0.44, over the target from 0.44 to 0.56 (where `bStruck` is set and `AddBattleSmoke` fires), then home. `UpdateSmoke` makes the flak bursts while the share is between 0.30 and 0.64; `UpdateSearchlights` runs the lamps on a night raid. The sortie carries `bBombers`, `bEscort`, `Flak` (0 to 3), `bNight`, `bEngaged`, `Loss` and `bFromSea`.

| When | Where | By day | By night |
|---|---|---|---|
| Share 0 | The base | `aircraft_takeoff` (`carrier_launch` if `bFromSea`) | The same |
| Share 0 to 1 | Carried by the flight | `StartLoop`: `bomber_heavy_loop` if `bBombers`, else `fighter_loop`. If `bEscort`, a second loop `fighter_loop` at -6 dB | `bomber_medium_loop` |
| Share 0.10 | The target town | `siren` at -6 dB, only if the town has guns | `air_raid_siren` |
| Share 0.25 | The target | Nothing | `searchlight_on`, one for each of the first three lamps, 0.2 s apart; `StartLoop` `searchlight_loop` until the lamps go out |
| Share 0.30 to 0.64, each burst `UpdateSmoke` makes | The burst | `Voice("flak_burst", 4)`, Reach 1.6 | The same |
| The same moment, for every second burst | The town's guns | `Voice("flak_heavy_fire", 3)`; if `Flak` is 1, `Voice("flak_light", 2)` | The same |
| `Flak` is 3 | The town | `StartLoop` `flak_barrage_loop` at -6 dB from 0.30 to 0.64, under the single shots | The same |
| `bEngaged` turns true | Where they meet | `dogfight` once, then `Voice("fighter_pass", 2)` every 2 to 3 s while it stays true | Rare at night; the same if it happens |
| A lamp first holds a bomber | The lamp | Nothing | `searchlight_caught`, once a raid |
| Share 0.38 | The target | `Voice("bombs_falling", 2)`: the whistle lasts 2 to 3 s and ends as they land | The same |
| `bStruck` turns true | The target | `Voice("bomb_stick", 3)` for bombers, `Voice("bomb_single", 2)` for fighter-bombers; Reach 1.6 | The same, then `fire_start` |
| `AddBattleSmoke` | The target | Nothing more | `StartLoop` `fire_town_loop` for as long as the town's glow lasts; `Voice("fire_collapse", 2)` once, 5 to 9 s in |
| `Loss` is over 0, at share 0.6 | A little past the target | `plane_down` | The same |
| Share 1 | | `StopLoop` on the flight's loops (they will already be faint) | The same; `searchlight_off` and `StopLoop` on the lamp |
| 2 s after the last flight on a town this month | The town | Nothing | `siren_all_clear` at -4 dB |

A dive-bomber attack on troops or ships (not a town) uses `dive_bomber` in place of the falling whistle, and `bomb_single`.

**The raid card.** When the card that reports a raid opens, play `raid_day` or `raid_night` (the eight-second stings) if the raid itself was not watched.

**The limit matters most here.** A level 3 flak town fires a burst every 0.45 s. With the 12-voice limit and the eight-a-second rule from step 3 this stays clear; without them it will turn to mud. The target to compare against is `scenes/scene_night_raid.mp3` and `scenes/scene_day_raid.mp3`; their cue lists are in `scene-list.json`.

## Step 6. Things that move

**Trains** (`UpdateTrains`: the little trains that run when `CameraDistance` is under 9000, 11 seconds a link, up to seven of them).

| When | Sound |
|---|---|
| A train appears | `StartLoop` `train_loop`, carried by it; `MoveLoop` every frame |
| Every 15 to 40 s, for one train in view | `Voice("train_whistle", 2)` at the train |
| It reaches the end of its line and turns round | `StopLoop` (0.3 s), then `train_turn_round` at the place (9 s: brakes, buffers, steam, a peep, the start back). Start the loop again when the train moves off; if the turn on screen is quicker than the sound, start the loop 4 s in and let the two overlap |
| A flak wagon on a guarded line fires (ruling 125(f)) | `Voice("flak_wagon_fire", 2)` at the wagon |
| An army is placed by rail (`PlaceByPlan`, which plays `train` today) | Keep `train`; or `train_whistle_01` then `train_depart` at the start and `train_arrive` at the end |
| `ToggleRail` | `ui_tab`, and `train_whistle_02` at -8 dB |
| The rails are cut, or mended | `rail_cut`, `rail_mended` |

**Armies.** `Hold` already plays `CampaignSounds::OfArmy` when an army is picked up. Keep it, and add:

| When | Sound |
|---|---|
| An army is picked up (`Hold`) | `pickup` (already called, now made), then `OfArmy` as today |
| An order is drawn (`OrderTo`) | `order_march`, or `order_attack` if it is an attack |
| The army is put down (`DropArmy`) | `putdown` |
| An order is withdrawn | `order_cancel` |
| While an army's piece moves across the map | `StartLoop` by kind, carried by the piece: `marching_loop`, `tank_loop`, `convoy_loop` (lorries and half-tracks), `horses_loop`. `StopLoop` when it arrives |
| `ProvinceAction`: dig in, blow a bridge, burn, cut rails | `dig_in`, `bridge_blown`, `fire_start`, `rail_cut` |
| Airborne troops dropped | `parachutes` |

**Fleets.**

| When | Sound |
|---|---|
| `PickUpFleet` | `pickup`, `ship_bell` |
| `OrderFleetTo` | `ship_horn` (today), or `ship_horn_02` every other time |
| While a fleet's piece moves | `StartLoop` `ship_loop` |
| It reaches port | `anchor_chain` |
| `AddSeaBattle` | `StartLoop` `sea_battle_loop` at the place while it is shown; `ship_guns` and `ship_guns_02` as today |
| Destroyers hunt a submarine | `sonar_ping`, then `depth_charge` |
| A torpedo strikes; `AddSinking` | `torpedo_hit`; `ship_sinking` |
| A shore is shelled | `shore_bombardment` |
| A landing | `landing_craft` |
| A submarine goes to sea | `submarine_dive` |

## Step 7. The month and the cards

**The interface's hooks.** The UI director's kit names twenty moments of the interface (`ui.press`, `ui.primary`, `ui.stamp.won` and so on; `ui-director/ui-style-guide.md`, "Sound hooks"). Where the kit's widgets raise a hook, play the sound that `ui-hooks.md` gives for it; that file is the one table both builders work from. Three of its rules need care in code: the stamp's sound is played when the stamp lands (220 ms after it starts to fall), not when it starts; one `alert_slip` for a batch of alerts, not one a slip; and when a red band's moment has its own call (`war_declared`, `nation_falls`), play the call and skip `drum_low`. Nothing plays when the pointer merely passes over a thing.

| When | Sound |
|---|---|
| Any button | `click`, as today. The brass button of a screen: `ui_primary`. Close and back buttons: `ui_back`. Tabs: `ui_tab`. A dial moved one stop: `ui_dial`. A switch: `switch_on`, `switch_off`. A refused order: `ui_error`, with the reason in words |
| A card opens, closes | `card_open`, `card_close` |
| A book or the ledger opens; its pages | `screen_open`, `page_turn`, `screen_close` |
| `ShowEndAsk` (the end-of-month question) | `month_ask` |
| `AnswerEndAsk` | `click` |
| `EndMonth` | `month_end`, and `StartLoop` `month_passing_loop` (flat, not placed) |
| `WatchNextStep` | Leave the loop running; the things shown make their own sounds |
| `FinishMonth` | `StopLoop`, then `month_yours` |
| The month card opens | `month_card_open`, then 0.6 s later one of `month_card_good`, `month_card_bad`, `month_card_quiet`, by the card's balance of good and bad rows |
| `OpenFaceOff` | `drums` (today), and `StartLoop` `battle_loop` at -8 dB under the card |
| `ConfirmAttack` | `order_attack`, then `battle` (today) |
| The battle's result | `StopLoop`; `victory` or `defeat` (today) |
| `OrderFromArsenal` | `stamp_light` (the old `stamp` is now kept for the WON stamp and for orders that cannot be undone), `coins`, and `OfOrder` as today |
| The treasury figure counts | `coin_run`, once, inside the 400 ms count |
| The WON stamp, the LOST stamp | `stamp`, `stamp_low`, as the stamp lands |
| New alerts slide in | `alert_slip`, once for the batch |
| `PlayNewsreel` | `newsreel`, `projector` (today) |

## Step 8. The rest

**Fires and guns that stand on the map.**

| What | Sound |
|---|---|
| `SiegeFires`, `PrizeFires` (`bBurning`), `FrontFires` | `StartLoop` `fire_small_loop` on each, Reach 0.7, at most three in view; by night at 0 dB, by day at -6 dB |
| A bombed town's glow (when it is built) | `fire_town_loop`, as in step 5 |
| `BuildGunLines`: a gun line that fires | `Voice("artillery_fire", 3)` at a gun, then `Voice("shell_incoming", 2)` and `Voice("shell_impact", 3)` at the target |
| A province under bombardment or siege | `StartLoop` `barrage_loop` at -6 dB while it is in view |
| A factory town, camera low | `StartLoop` `factory_loop`, Reach 0.5 |

**Great moments.** The controller plays `victory` or `defeat` by `Announcements[0].bGood`. Make it choose by what the announcement is:

| Announcement | Sound |
|---|---|
| War is declared (`ShowWar`) | `war_declared` |
| A treaty, an alliance, a peace | `treaty_signed` |
| A capital or a prize city taken, lost | `city_taken`, `city_lost` |
| A country beaten | `nation_falls` |
| A country freed | `nation_freed` |
| A medal, a promotion | `medal` |
| Research finished | `research_done` |
| A division or a named ship lost | `unit_lost` |
| Something needs the player now (a landing on his shore, his government in danger) | `alert` |
| A campaign starts | `campaign_start` in place of `bugle` (or keep `bugle`: it is the same figure on one cornet) |
| The campaign is won, lost | `world_won`, `campaign_lost` |
| Anything else, good or bad | `victory`, `defeat`, as today |

**First Command** (ruling 129): `note_arrives` when a staff note appears, `note_done` when it is stamped DONE, `note_closing` with the closing note. The narrator reads the note after `note_arrives` has finished (0.6 s).

## How to know it works

Nobody on the building side can hear the game any more than the sound director can, so check by log and leave the ear to Brian.

1. `-TheaterQuiet` stays as it is.
2. Log one line for each sound played when a new flag `-TheaterSoundLog` is set: name, place, gain. A watched night raid on a level 3 town should log the table in step 5 in order, and never more than 12 placed sounds at once.
3. The startup check from step 3 logs no missing names.
4. Report to the coordinator which steps are in, by number, so the sound list (`gate-7-sound-list.md`) can be marked WIRED line by line.
