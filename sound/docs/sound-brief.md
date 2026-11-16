# Sound brief

Sound director, 16 November 2026. For Brian, the coordinator, the design director, the art director and both builders.

**Brian's words** (project chat, 2026-11-16 18:42:45Z): "do all of those. we definitely need great sound effects".

**What this is.** The plan for how the game sounds: what the sounds are, how day differs from night, how loud each thing is, and which event each sound belongs to. The sounds themselves are made (203 of them, 3 stings and 7 target scenes) and can be heard on the [listening board](https://claude.ai/artifact/Y4PGKtarPo2J2mqs6oUuYV), where each one can be stamped Good, Fix or Cut. The lists beside this brief:

| File | What it holds |
|---|---|
| `catalogue.md` | Every sound: name, length, loudness, what it is and when it plays |
| `gate-7-sound-list.md` | The sound team's list against gate 7's test, with the state of each line |
| `implementation-3d.md` | The plan for the 3D builder |
| `implementation-browser.md` | The smaller set for the browser game and how to wire it |
| `ui-hooks.md` | The UI director's twenty interface hooks and the sound that answers each |
| `licence-log.md` | Where every file came from (all original) |
| `mp3/`, `scenes/` | Every sound and scene, to listen to |
| `browser-set/` | The browser game's 78 sounds, packed small, with a player module |
| `generator/` | The program that makes every sound (also in the repository, folder `sound/`) |

## 1. The one idea: a war room with a window

The player is a commander at a desk in a war room, looking down at a map that is alive. So the game has two worlds of sound, and they must never be confused.

**The desk** is close, dry and made of real things: paper, a pencil, a rubber stamp, wood, brass, a bakelite switch, a typewriter. Every button, card and order sounds like something on that desk. Nothing electronic, and never a buzzer: in 1939 a button was a thing you could knock on.

**The war** is far, wide and outdoors: engines, guns, weather, fire. It is heard through distance, with the echo of open country or a town's walls on it.

**Camera height is the window between them.** High above the map you are in the room: the clock, a clerk typing next door, and the war only a murmur. Bring the camera down and the window opens: the room falls away, wind and birds come up, and each thing on the map is heard where it stands.

```
 camera high  |  ROOM BED ████████████  outdoor bed ░░           world sounds: faint, dull
              |  ROOM BED ████████░░░░  outdoor bed ████         world sounds: clearer
 camera low   |  room bed ░░            OUTDOOR BED ████████████  world sounds: full, placed left and right
```

The desk sounds do not change with the camera. Your own pencil is always at your elbow.

## 2. What the game has today

The finish line says "the game has a voice today and no other sound". For the record, it is a little better and a little worse than that:

- **The 3D game** already plays 29 synthesised stand-in sounds (a click, a stamp, a horn, a fly-past and so on), three moods of music and the narrator's voice. All 29 are played flat, as if inside the player's head: none is placed on the map, none gets louder as the camera comes down. Two sounds the game asks for were never made and are silent: `pickup` (an army picked up) and `train` (an army sent by rail). The flak, the bombs, the searchlights, the little trains, the fires and day against night have no sound at all.
- **The browser game** has no sound of any kind.

Every one of the 29 old names is in the new set, so the 3D game gets the new sounds for the old events without a line of code changing.

## 3. The families

| Family | Sounds | What | Placed on the map? | Loudness as made (LUFS) |
|---|---:|---|---|---:|
| Aircraft | 14 | Heavy, medium and fighter engines; fly-pasts; a dive; a dogfight; a plane down | Yes (mono) | -20 |
| Flak and searchlights | 16 | Heavy and light guns firing, shells bursting, a flak wagon, lamps struck, held and doused | Yes (mono) | -18 |
| Guns, bombs and battle | 29 | Artillery, shells in flight and landing, rifles, machine guns, tank guns, bombs falling and bursting, a bridge blown, the noise of a fight | Yes (mono); the battle beds are stereo | -16 |
| Armies | 14 | Tanks, lorries, half-tracks, boots, horses, digging, building, a factory, parachutes | Yes (mono) | -20 |
| Railways | 10 | A train going, leaving, arriving, turning round at the line's end, whistles, the rails cut and mended | Yes (mono) | -21 |
| Ships | 15 | Sirens, engines, salvoes, shore bombardment, sonar, depth charges, torpedo, sinking, landing craft, carrier launch | Yes (mono); the sea battle bed is stereo | -19 |
| Fire | 5 | A building alight, a town burning, roofs falling in | Yes | -24 |
| The desk | 42 | Buttons, dials, tabs, cards, pages, the stamp in three weights, pieces picked up and put down, pencilled orders, coins, the alert slip, telephone, typewriter, the month's sounds, the staff notes | No (stereo) | -27 (a click is -30) |
| Calls | 28 | 25 short pieces for the band, and three sirens | Band: no. Sirens: yes | -20 |
| Ambience | 30 | 11 beds and 19 sprinkles (see section 6) | Beds: no. Sprinkles: yes | -30 |

Where a sound is heard often it comes in several voices (`flak_burst_01` to `_04`, `artillery_fire_01` to `_03`), so that the same recording is never heard twice running.

## 4. Day and night

Night must be a different picture to the ear, as it is to the eye. The test is that a raid by day and a raid by night can be told apart with the eyes shut. They differ in every layer:

| Layer | By day | By night |
|---|---|---|
| The air over land | `land_day_loop`: wind in grass, birdsong a long way off | `land_night_loop`: crickets, a little wind, no birds |
| Life | A songbird, a crow, a church bell (sprinkles) | An owl |
| The sea | `sea_day_loop` and gulls | `sea_night_loop`, slower and darker, and a bell buoy |
| The war room | `room_day_loop`: the clock and a clerk typing next door | `room_night_loop`: the clock, the lamp hissing, wind at the window, nobody else |
| A raid: the warning | The siren far off and short (`siren`), or none | The full wailing siren (`air_raid_siren`) |
| A raid: the bombers | `bomber_heavy_loop`: four engines in step, a deep steady drone, and the escort's fighters above | `bomber_medium_loop`: two engines out of step, the uneven beat that rises and falls |
| A raid: the defence | Fighters meet them: `dogfight`, `fighter_pass` | The lamps: `searchlight_on`, `searchlight_loop`, `searchlight_caught` |
| A raid: after | Smoke, and quiet | `fire_town_loop`: the town burns for as long as its glow lasts, with `fire_collapse`; then `siren_all_clear` |
| The switch itself | `switch_on` | `switch_off` |

One short sting each, `raid_day` and `raid_night`, tells the same story in eight seconds for the raid card and for the browser game.

## 5. A march against a train

The other half of the test: with the eyes shut, a march must not be mistaken for a rail move. They are built from opposite materials.

| | March | Train |
|---|---|---|
| The order | `order_march` (a pencil line), then `marching` | `train_whistle_01`, then `train_depart` |
| While it moves | `marching_loop`: boots, two paces a second, kit jingling, no pitch at all | `train_loop`: the beat of the exhaust in fours, wheels over the rail joints, a whistle now and then |
| What gives it away | Many small sounds in step | One big machine, a pitched whistle, iron on iron |
| At the end | The column halts | `train_turn_round`: brakes squeal, the buffers close up one after another, steam, a peep, and it sets off back |

Tanks (`tank_loop`: engine and squealing tracks), lorries (`convoy_loop`) and horses (`horses_loop`) are each their own sound too, so every kind of army is known by ear.

## 6. Nothing loops until it grates

This is the third clause of the test, and it is won by how the sounds are built and played, not by any single file.

**Beds and sprinkles.** Think of wallpaper and pictures. A bed is wallpaper: a long loop of pure texture (wind, crickets, the sea, the air of the room) with nothing in it the ear can pick out and wait for. A sprinkle is a picture: one event (a bird, an owl, a far salvo, steps past the door) that the game hangs at random, somewhere to the left or right, every so often, never the same one twice running. A bird baked into a loop comes round like a stuck record; a sprinkled bird never repeats.

The rules for both builders:

1. **Beds are texture only.** They run 24 to 32 seconds and start at a random point.
2. **Sprinkles are random.** One every 6 to 20 seconds, random side, pitch within 3% either way, never the same file twice running, none while a card is open.
3. **Voices rotate.** Where a sound has numbered voices, pick one at random, never the last one used, and shift its pitch within 3% and its loudness within 1.5 dB.
4. **Carried loops differ.** A loop carried by a thing on the map (an engine, a train, a fire) starts at a random point and at its own pitch within 4%, so two trains never beat in step.
5. **A loop lasts as long as its event and no longer.** It fades in over half a second and out over one second. No loop plays for a thing that is off the screen.
6. **Not too many at once.** At most two of the same loop, twelve world sounds in all; the farthest gives way first.
7. **The commonest sounds are the softest.** A click is heard hundreds of times an hour, so it is short and 3 dB under the other desk sounds. `tick` (a row chosen in a list) is 8 dB under that.
8. **The UI director's three rules** (`ui-director/ui-style-guide.md`, "Sound hooks"), adopted as they stand: one sound to an act, and the same sound never faster than eight a second; nothing sounds when the pointer merely passes over a thing; a refusal (`ui_error`) always comes with the reason in words.

## 7. Mixing rules

**Five buses** (a bus is a group with one volume knob):

| Bus | What is on it | Level against the file as made | Follows the camera? |
|---|---|---|---|
| Interface | The desk | 0 dB | No |
| Calls | The band's pieces | -2 dB | No |
| World | Everything on the map, and the sirens | 0 dB at the lowest camera, falling to -12 dB and duller at the highest | Yes |
| Ambience | Beds and sprinkles | 0 dB | The room bed and the outdoor bed cross over with camera height |
| Music | The three moods that exist today | As today (0.55) | No |

The narrator's voice sits above all of them, as it does now.

**Who gives way to whom.** When two things want the ear at once, the one that matters more is kept clear:

1. The voice speaks: the world and the ambience drop 6 dB, a call waits for it to finish.
2. A call plays: the music drops 10 dB for the length of the call. (The calls are in the key of B flat; ducking the music keeps two keys from fighting.)
3. A raid or a battle is on the screen: the music drops 6 dB and the ambience 4 dB.
4. A card is open: the world drops 4 dB and no sprinkles play, so the card can be read in peace.

**Two volume controls**, as gate 7 asks: Effects (the interface, calls, world and ambience together) and Music. The 3D game's N key (sound on or off) and M key (music level) stay.

**Distance.** A world sound is loudest when the camera is over it and low, and fades with distance across the map. Sounds far from the middle of the screen come from that side. The files are mono for exactly this: the game does the placing.

## 8. Loudness rules

Every file is levelled when it is made, so that the game's volume numbers mean the same thing for every sound. Loudness is measured the broadcasters' way (LUFS, which follows the ear, not the meter needle).

- Each family has one target (the table in section 3). Within a family every one-shot's loudest moment sits on the target; every loop's whole length does.
- No file peaks above -1 dB; no loop above -3 dB.
- Nothing below 20 Hz in any file, and nothing below 36 Hz in a loop: it would only eat loudness.
- **The laptop rule.** Most players will hear the game through a laptop, which gives almost nothing below about 120 Hz. Every sound is measured through a small-speaker filter, and any that loses much has its weight leaned upward until it survives. Far guns and depth charges would otherwise simply vanish. The catalogue shows the figure for every sound; the worst now loses 6 dB.
- The scenes show the result: a raid plays near -19 LUFS, the desk near -24. The desk is quieter than the war, as it should be.

## 9. Music

No new music was written in this round, and the music that exists stays. The 3D game has three moods (the theme "Leave Pass", the battle piece and the quiet map-room piece) and already swaps between them.

What sound adds is the band's 25 **calls**: short pieces for the moments that matter (a campaign begun, a battle won or lost, war declared, a treaty, a capital taken or lost, a country beaten or freed, the month's report, a medal, the campaign's end). All of them are built on one small figure of our own, five notes: F, B flat, C, A, D. Heard whole it opens a campaign; its last three notes rising are a victory; the same three turned downward are a defeat. Like a company's jingle, the player learns it without being told, and then knows good news from bad in the first half second.

The figure uses notes a bugle cannot play, so it cannot be any army's real call, and it quotes no anthem, march or film tune.

## 10. Which event gets which sound

The full tables, by function name for the 3D game and by button for the browser game, are in the two implementation files. The interface's own moments are named by the UI director as twenty hooks (`ui.press`, `ui.stamp.won` and so on); `ui-hooks.md` is the one table of which sound answers which hook. In short:

| Event | Sound |
|---|---|
| Any button | `click`; a screen's brass button `ui_primary`; closing or going back `ui_back`; a tab `ui_tab`; a dial `ui_dial`; a switch `switch_on`, `switch_off`; something refused `ui_error` |
| A card opens or closes | `card_open`, `card_close`; a book's page `page_turn`; a whole screen `screen_open`, `screen_close` |
| An army is picked up, ordered, put down | `pickup`, then `order_march` or `order_attack`, then `putdown`; an order withdrawn `order_cancel` |
| An army moves | Its own sound by kind: `marching`, `tank_engine`, `truck_engine`, `halftrack`, `horses`, and the matching loop while it is on the road |
| An army goes by rail | `train_whistle_01`, `train_depart`, `train_loop`, `train_arrive` |
| An attack | `drums` as the sides are drawn up, `battle`, then `victory` or `defeat` |
| Guns | `artillery_fire`, `artillery_salvo`, `shell_incoming`, `shell_impact`; a shelled province carries `barrage_loop` |
| A raid | Section 4, cue by cue in the 3D plan |
| A fleet | `ship_bell` picked up, `ship_horn` sailing, `ship_loop` at sea, `ship_guns` in action, `shore_bombardment`, `sonar_ping` and `depth_charge` hunting, `torpedo_hit`, `ship_sinking` |
| Building and wrecking | `stamp_light` and `coins` for an order paid, `hammer` and `build_works` for works, `dig_in`, `bridge_blown`, `rail_cut`, `rail_mended`, `fire_start` |
| The month ends | `month_ask` for the question, `month_end` when it ends, `month_passing_loop` while the others move, `month_yours` when it is your turn, `month_card_open` and one of `month_card_good`, `month_card_bad`, `month_card_quiet` |
| The stamps and the slips | `stamp` for WON, `stamp_low` for LOST, `stamp_light` for an order placed; `alert_slip` for a batch of alerts; `coin_run` while the treasury counts; `drum_low` for a red band with no call of its own |
| Great moments | `campaign_start`, `war_declared`, `treaty_signed`, `city_taken`, `city_lost`, `nation_falls`, `nation_freed`, `medal`, `research_done`, `unit_lost`, `alert`, `world_won`, `campaign_lost` |
| First Command | `note_arrives`, `note_done` (the stamp alone is `stamp`), `note_closing` |

## 11. Weather, when it comes

Ruling 127 brings weather. The sounds are ready and wait for it: `rain_loop`, `thunder_01` and `thunder_02`, `winter_loop` for snow, `desert_loop`, `jungle_loop`. Rain is laid over the day or night bed and the birds are thinned out.

## 12. How the sounds were made, and what has not been checked

Every sound is original. Each was synthesised from tones and noise by our own program, to our own recipes; nothing is recorded, sampled or borrowed, and nothing is modelled on any game's or film's sound. The licence log has the detail. The program is in the repository (`sound/`); run it and it writes the same files again, exactly.

**The limit that matters: the sound director cannot hear.** These sounds were built from an understanding of how the real things make their noise, and checked by measurement (loudness, peaks, clicks, joins of loops, rumble, the laptop rule) and by reading pictures of them (spectrograms). That catches faults; it cannot say whether a tank sounds like a tank. The first pass of the meter found 45 faults and they are fixed. What a meter cannot find, only an ear can, and the listening board is built for that: every sound has a Good, Fix and Cut button and a place for a note. Gate 7's last item, "it passes your ears", is Brian's.
