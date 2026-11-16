# Gate 7: the sound team's list

Sound director, 16 November 2026. Written for the design director's finish line (ruling 128), against gate 7's test.

**The test.** "Every order you give and every month that ends has a sound. With your eyes shut you can tell a march from a train, and a raid by day from a raid by night. Nothing loops until it grates." Effects and music each have their own volume.

**Three states.** A line of this list passes through three states, in order:

- **MADE**: the sound exists, is measured and is on the [listening board](https://claude.ai/artifact/Y4PGKtarPo2J2mqs6oUuYV).
- **WIRED**: the game plays it at the right moment. This is the builders' work, from the two implementation files.
- **HEARD**: Brian has listened and it passes.

**Where things stand on 16 November.** All 206 sounds are MADE (203 and three stings). None of the new ones is WIRED. None is HEARD. In the 3D game the 29 old names will play the new sounds as soon as the builder swaps the generator, with no other change.

## A. Every order you give has a sound

| # | Order | Sound | Made | Wired 3D | Wired browser | Heard |
|---|---|---|---|---|---|---|
| A1 | Press any button | `click` | yes | old name, on swap | no | no |
| A2 | Close, cancel, go back | `ui_back`, `card_close` | yes | no | no | no |
| A3 | Choose a tab | `ui_tab` | yes | no | no | no |
| A4 | Something that cannot be done | `ui_error` | yes | no | no | no |
| A5 | Pick a province | `select_region` | yes | no | no | no |
| A6 | Pick up an army | `pickup`, then the army's own sound by kind | yes | `pickup` is silent today; on swap | no | no |
| A7 | Order a march | `order_march`, `putdown`, `marching` (or tanks, lorries, horses) | yes | old names, on swap | no | no |
| A8 | Order an attack | `order_attack`, `drums` | yes | `drums` on swap | no | no |
| A9 | Send an army by rail | `train_whistle_01`, `train_depart` (browser: `rail_move`) | yes | `train` is silent today; on swap | no | no |
| A10 | Stand fast, stop a march, withdraw an order | `putdown`, `order_cancel` | yes | no | no | no |
| A11 | Dig in, fortify | `dig_in` | yes | no | no | no |
| A12 | Build or buy (arsenal, works, a town's job) | `stamp_light`, `coins`, `hammer`; `build_works` when finished | yes | old names, on swap | no | no |
| A13 | Blow a bridge, wreck, burn, cut the rails | `bridge_blown`, `explosion`, `fire_start`, `rail_cut` | yes | no | no | no |
| A14 | Plan and send a raid | `click` through the planner, `stamp` to send; the raid itself is section D | yes | on swap | no | no |
| A15 | Patrol, escort, scout, move an air group | `fighter_pass`, `aircraft_takeoff` | yes | `fighter_pass` on swap | no | no |
| A16 | Pick up a fleet, sail, return to port | `ship_bell`, `ship_horn`, `anchor_chain` | yes | `ship_horn` on swap | no | no |
| A17 | Shell a shore | `shore_bombardment` | yes | no | no | no |
| A18 | Land an army from the sea | `landing_craft` | yes | no | no | no |
| A19 | Drop airborne troops | `parachutes` | yes | no | no | no |
| A20 | Declare war | `war_declared` | yes | no | no | no |
| A21 | Sign a treaty, an alliance, a trade | `pen_sign`, `treaty_signed` | yes | no | no | no |
| A22 | Take a road of history, a cabinet decision | `stamp`, `bell` | yes | old names, on swap | no | no |
| A23 | Appoint, promote, decorate | `medal` | yes | no | no | no |
| A24 | Send a spy, read an intercept | `radio` | yes | old name, on swap | no | no |
| A25 | Start research | `pen_sign`; `research_done` when it lands | yes | no | no | no |
| A26 | Turn night on or off, turn a setting on or off | `switch_off`, `switch_on` | yes | no | no | no |
| A27 | Open a ledger, a book, the guide | `page_turn`, `screen_open` | yes | no | no | no |
| A28 | Press a screen's brass button (end the month, declare, confirm) | `ui_primary` | yes | no | no | no |
| A29 | Turn a dial one stop (history, level, depth) | `ui_dial` | yes | no | no | no |
| A30 | The WON or LOST stamp lands on a report | `stamp`, `stamp_low` | yes | `stamp` on swap | no | no |
| A31 | Alerts arrive; the treasury counts; a red band appears | `alert_slip`, `coin_run`, `drum_low` | yes | no | no | no |

## B. Every month that ends has a sound

| # | Moment | Sound | Made | Wired 3D | Wired browser | Heard |
|---|---|---|---|---|---|---|
| B1 | The end-of-month question | `month_ask` | yes | no | no | no |
| B2 | The month ends | `month_end` | yes | no | no | no |
| B3 | The other countries move | `month_passing_loop` | yes | no | no | no |
| B4 | The month is yours again | `month_yours` | yes | no | no | no |
| B5 | The month card is laid down | `month_card_open` | yes | no | no | no |
| B6 | The card's news: good, bad or mixed | `month_card_good`, `month_card_bad`, `month_card_quiet` | yes | no | no | no |
| B7 | What happened in the month: battles, cities, countries | `victory`, `defeat`, `city_taken`, `city_lost`, `nation_falls`, `nation_freed`, `unit_lost` | yes | `victory` and `defeat` on swap | no | no |
| B8 | The newsreel | `newsreel`, `projector` | yes | old names, on swap | not in the browser game | no |

## C. Eyes shut: a march from a train

| # | What must be true | How | Made | Wired 3D | Wired browser | Heard |
|---|---|---|---|---|---|---|
| C1 | A march sounds like boots | `marching`, `marching_loop` | yes | `marching` on swap | no | no |
| C2 | A train sounds like a train | `train_depart`, `train_loop`, whistles | yes | no | no | no |
| C3 | A train turns round at the end of its line | `train_turn_round` | yes | no | not in the browser game | no |
| C4 | A flak wagon on a guarded line fires | `flak_wagon_fire_01`, `_02` | yes | no | not in the browser game | no |
| C5 | Tanks, lorries and horses are each their own sound | `tank_loop`, `convoy_loop`, `horses_loop` | yes | no | no | no |
| C6 | The two can be compared side by side | Scene `scene_march_and_train`; on the board, the first "eyes shut" pair | yes | n/a | n/a | no |

## D. Eyes shut: a raid by day from a raid by night

| # | What must be true | How | Made | Wired 3D | Wired browser | Heard |
|---|---|---|---|---|---|---|
| D1 | The air itself differs | `land_day_loop` against `land_night_loop` | yes | no | not in the browser game | no |
| D2 | The bombers differ | `bomber_heavy_loop` by day, `bomber_medium_loop` by night | yes | no | no | no |
| D3 | The defence differs | Fighters by day (`dogfight`, `fighter_pass`); lamps by night (`searchlight_on`, `_loop`, `_caught`, `_off`) | yes | no | no | no |
| D4 | The warning differs | `siren` by day, `air_raid_siren` and `siren_all_clear` by night | yes | `siren` on swap | no | no |
| D5 | Flak fires and its shells burst | `flak_heavy_fire`, `flak_burst`, `flak_light`, `flak_barrage_loop` | yes | no | no | no |
| D6 | Bombs fall and burst | `bombs_falling`, `bomb_stick`, `bomb_single` | yes | no | no | no |
| D7 | A town burns at night | `fire_start`, `fire_town_loop`, `fire_collapse` | yes | no | no | no |
| D8 | The raid card tells day from night in one sound | `raid_day`, `raid_night` | yes | no | no | no |
| D9 | The two can be compared side by side | Scenes `scene_day_raid` and `scene_night_raid`; on the board, the second "eyes shut" pair | yes | n/a | n/a | no |

## E. Nothing loops until it grates

| # | What must be true | How | Made | Wired 3D | Wired browser | Heard |
|---|---|---|---|---|---|---|
| E1 | Beds are pure texture, with nothing to wait for | 11 beds, 24 to 32 seconds, built without events | yes | no | one bed | no |
| E2 | Life comes from sprinkles, at random | 19 sprinkles; the rules are in the brief, section 6 | yes | no | no | no |
| E3 | No loop has a click at its join | Measured: every loop's join is within its own normal step | yes | check on import | handled by the packing | no |
| E4 | No sound is heard twice running | Numbered voices, random pitch and loudness (brief, section 6) | yes | no | no | no |
| E5 | The commonest sound is the softest, and nothing sounds on pointer-over | `click` at -30 LUFS, `tick` at -38; the same sound never faster than eight a second | yes | on swap | no | no |
| E6 | An hour's play does not tire the ear | Brian's listening pass, and the Playtester's hour | n/a | n/a | n/a | no |

## F. Effects and music each have their own volume

| # | What must be true | State |
|---|---|---|
| F1 | 3D game: a volume for effects and one for music | The music level exists (M key). Effects have only on and off (N key). A volume for effects is in the 3D plan, step 2. |
| F2 | Browser game: a volume for effects | In the browser plan. The browser game has no music, so there is nothing for a second control to govern yet. |

## G. First Command (ruling 129)

| # | Moment | Sound | Made | Wired 3D | Wired browser | Heard |
|---|---|---|---|---|---|---|
| G1 | A staff note arrives | `note_arrives` | yes | no | no | no |
| G2 | The note is stamped DONE | `note_done` (the stamp alone is `stamp`) | yes | no | no | no |
| G3 | The closing note | `note_closing` | yes | no | no | no |

## H. It passes your ears

Only Brian can close this. The listening board is built for it: every sound has Good, Fix and Cut and room for a note, and the sound team reads the marks and works from them. The sound director cannot hear; everything above was checked by meter and by picture, which finds faults but cannot say whether a thing sounds right.

## What would make this gate fail, in order of risk

1. **The sounds are made but not wired.** Until the builders hook them up, the player hears only the old 29 events.
2. **A synthesised sound that measures well may still sound wrong.** Engines, voices of animals and brass are the hardest to make from nothing. The ear pass will find these; each one marked Fix is rebuilt.
3. **Too much at once.** A big night raid can ask for forty sounds in ten seconds. The limits in the brief (section 6, rule 6) must be built in, or the mix will turn to mud.
