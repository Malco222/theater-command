# Catalogue of sounds

Every sound in the set, by family: 203 sounds and 3 stings, with 7 target scenes. This file is written by `sound/tools/make_docs.mjs` from the list the generator makes, so it always matches the files.

How to read a row. **Name** is the file name without `.wav` and the name the game calls. **Kind** says mono or stereo and whether it is a loop: mono sounds are placed on the map by the game, stereo ones are played as they are. **Seconds** is its length. **LUFS** is its loudness as made (for a loop the whole of it, for anything else its loudest moment). **Small speaker** is how many dB it loses through a laptop speaker; under 6 is safe.

## Aircraft (14)

Engines by kind, fly-pasts, a dive, a dogfight, a plane going down.

| Name | Kind | Seconds | LUFS | Small speaker | What it is and when it plays |
|---|---|---:|---:|---:|---|
| `bomber_heavy_loop` | mono, loop | 11.4 | -20 | 1.7 | Four-engined bombers in formation, heard from below: the deep, slowly beating drone. Carried by a raid of heavy bombers for as long as it is in the air. |
| `bomber_medium_loop` | mono, loop | 11.4 | -20 | 1.2 | Twin-engined bombers. Their two engines run a little out of step, which gives the slow "wom-wom-wom" that people on the ground remembered. |
| `fighter_loop` | mono, loop | 8.3 | -20 | 0.6 | A single-engined fighter with an in-line engine at cruising speed: a smooth tearing note with a blower's whine on top. |
| `fighter_radial_loop` | mono, loop | 8.3 | -20 | 0.5 | A single-engined fighter with a radial engine: rounder and rougher than the in-line one. |
| `fighter_pass_01` | stereo | 4.64 | -20 | 0.2 | A fighter goes by low and fast, left to right; the note drops as it passes. |
| `fighter_pass_02` | stereo | 4.88 | -20 | 0.1 | A radial-engined fighter goes by, right to left, a little further off. |
| `fighter_pass` | stereo | 4.57 | -20 | 0.2 | The same as fighter_pass_01, under the name the 3D game already calls. |
| `bomber_drone` | stereo | 6.6 | -20 | 1 | A formation of heavy bombers passes high overhead and goes on: six seconds, swelling and fading. Played once where no loop can be carried. |
| `dive_bomber` | mono | 5.5 | -20 | 0.5 | A dive bomber comes down on its target: the engine winds up and a wind-driven siren on its undercarriage screams, then it pulls out. The bomb is a separate sound (bomb_single). |
| `aircraft_takeoff` | mono | 5.57 | -20 | 0.9 | Aircraft run up their engines on the airfield and go: played at the airfield as a raid or a patrol sets off. |
| `dogfight` | stereo | 8.21 | -20 | 0.5 | Fighters among the bombers: two engines weaving, short bursts of their guns. Played over a raid while the defender's fighters are engaged. |
| `plane_down` | mono | 7.71 | -20 | 2.5 | An aircraft falls out of the fight: its engine falters and its note slides down and away, and it hits far off. Played once for a raid that loses aircraft. |
| `bombs_falling_01` | mono | 3.63 | -20 | 0 | A stick of bombs on its way down: several whistles together, sliding down in pitch and swelling. Ends just as the first bomb would land; follow it with a bomb_stick. |
| `bombs_falling_02` | mono | 2.75 | -20 | 0 | The same, a shorter and thinner fall: a lighter raid. |

## Flak and searchlights (16)

The guns that defend a town, their shells bursting, the lamps.

| Name | Kind | Seconds | LUFS | Small speaker | What it is and when it plays |
|---|---|---:|---:|---:|---|
| `flak_heavy_fire_01` | mono | 2.46 | -18 | 3 | A heavy anti-aircraft gun fires from the edge of the town: a hard crack and a thump, the breech clanging open after it. One of three voices; pick at random. |
| `flak_heavy_fire_02` | mono | 2.46 | -18 | 3.2 | A heavy anti-aircraft gun fires from the edge of the town: a hard crack and a thump, the breech clanging open after it. One of three voices; pick at random. |
| `flak_heavy_fire_03` | mono | 2.45 | -18 | 3.1 | A heavy anti-aircraft gun fires from the edge of the town: a hard crack and a thump, the breech clanging open after it. One of three voices; pick at random. |
| `flak_burst_01` | mono | 2.67 | -21 | 3.5 | A heavy shell bursts high in the air among the bombers: a dull "crump" with no crack to it, and the ground answering a moment later. One of four voices, each a little higher or lower. |
| `flak_burst_02` | mono | 2.62 | -21 | 3.3 | A heavy shell bursts high in the air among the bombers: a dull "crump" with no crack to it, and the ground answering a moment later. One of four voices, each a little higher or lower. |
| `flak_burst_03` | mono | 2.68 | -21 | 3.6 | A heavy shell bursts high in the air among the bombers: a dull "crump" with no crack to it, and the ground answering a moment later. One of four voices, each a little higher or lower. |
| `flak_burst_04` | mono | 2.59 | -21 | 3.5 | A heavy shell bursts high in the air among the bombers: a dull "crump" with no crack to it, and the ground answering a moment later. One of four voices, each a little higher or lower. |
| `flak_light_01` | mono | 4.53 | -20 | 1.8 | Light anti-aircraft guns: the steady "pom, pom, pom" of an automatic gun, two rounds a second, and its small shells bursting far up. |
| `flak_light_02` | mono | 4.8 | -20 | 1.7 | Light anti-aircraft guns: the steady "pom, pom, pom" of an automatic gun, two rounds a second, and its small shells bursting far up. |
| `flak_barrage_loop` | mono, loop | 12 | -22 | 3.4 | A whole town's guns firing at once, heard as one continuous barrage: for a raid on a place with heavy guns, under the single shots. |
| `flak_wagon_fire_01` | mono | 2.71 | -19 | 0.5 | The flak wagon on a guarded train opens up: four light barrels firing together in short tearing bursts, the wagon's iron floor ringing under them. |
| `flak_wagon_fire_02` | mono | 3.25 | -19 | 0.4 | The flak wagon on a guarded train opens up: four light barrels firing together in short tearing bursts, the wagon's iron floor ringing under them. |
| `searchlight_on` | mono | 2.18 | -24 | 0.6 | A searchlight is struck: the heavy switch goes over and the arc catches with a spit and settles to a buzz. Played at each lamp as its beam comes up. |
| `searchlight_loop` | mono, loop | 6.2 | -32 | 1.1 | A searchlight burning: the buzz and hiss of the arc and the putter of its generator. Very quiet; heard only close to. Carried by each lamp while it is lit. |
| `searchlight_off` | mono | 1.43 | -26 | 1.3 | A searchlight is doused: the arc fizzles down and the switch drops. |
| `searchlight_caught` | mono | 2.3 | -25 | 0 | A beam has found a bomber and holds it: a thin bright shimmer, rising. A signal more than a real sound; played once when a searchlight takes hold. |

## Guns, bombs and battle (29)

Artillery, shells, small arms, tank guns, bombs, explosions, the noise of a fight.

| Name | Kind | Seconds | LUFS | Small speaker | What it is and when it plays |
|---|---|---:|---:|---:|---|
| `bomb_stick_01` | mono | 6.15 | -15 | 3.2 | A stick of six bombs walks across the target, each a heavy thump with earth and masonry coming down after it. |
| `bomb_stick_02` | mono | 6.14 | -15 | 3.2 | A heavier stick of eight, closer together: a hard hit. |
| `bomb_stick_03` | mono | 6.02 | -15 | 3.3 | Four bombs, well spaced: a light raid or a scattered one. |
| `bomb_single_01` | mono | 4.76 | -15 | 3.1 | One heavy bomb: for a dive bomber's strike, a bridge, a single hit. |
| `bomb_single_02` | mono | 4.59 | -15 | 3.2 | One bomb, a second voice so that two in a row do not sound the same. |
| `artillery_fire_01` | mono | 4.9 | -16 | 3.3 | A field gun in the gun line fires: the bark, the thump in the ground, the shell tearing away, and the report rolling round the country. One of three voices. |
| `artillery_fire_02` | mono | 5.2 | -16 | 3.5 | A field gun in the gun line fires: the bark, the thump in the ground, the shell tearing away, and the report rolling round the country. One of three voices. |
| `artillery_fire_03` | mono | 4.77 | -16 | 3.5 | A field gun in the gun line fires: the bark, the thump in the ground, the shell tearing away, and the report rolling round the country. One of three voices. |
| `field_gun` | mono | 4.95 | -16 | 3.3 | A field gun, under the name the 3D game already calls. |
| `artillery_salvo_01` | mono | 6.56 | -15 | 3.6 | The whole gun line fires, gun after gun down the line in a second or two. For a bombardment ordered, or a battle opened by an army with artillery. |
| `artillery_salvo_02` | mono | 6.52 | -15 | 3.5 | The whole gun line fires, gun after gun down the line in a second or two. For a bombardment ordered, or a battle opened by an army with artillery. |
| `shell_incoming_01` | mono | 5.05 | -16 | 3.3 | A shell coming in: its short falling whine and the burst, with earth thrown up. |
| `shell_incoming_02` | mono | 4.86 | -16 | 3.5 | A shell coming in: its short falling whine and the burst, with earth thrown up. |
| `shell_impact_01` | mono | 5.21 | -18 | 3.2 | A shell bursts some way off: a heavy "crump" and what it threw up coming down. One of three voices, for the far end of a bombardment. |
| `shell_impact_02` | mono | 4.73 | -18 | 3.2 | A shell bursts some way off: a heavy "crump" and what it threw up coming down. One of three voices, for the far end of a bombardment. |
| `shell_impact_03` | mono | 5.3 | -18 | 3.3 | A shell bursts some way off: a heavy "crump" and what it threw up coming down. One of three voices, for the far end of a bombardment. |
| `barrage_loop` | mono, loop | 12 | -21 | 3.7 | Drumfire: a bombardment kept up, guns and bursts without a pause. Carried by a province while it is being shelled. |
| `rifles` | mono | 2.17 | -16.2 | 0.7 | Rifle fire: a ragged dozen shots along a line. |
| `rifles_02` | mono | 1.69 | -16.7 | 0.7 | Rifle fire, thinner: skirmishers, a patrol, a garrison holding on. |
| `machine_gun` | mono | 3 | -16 | 1.4 | A heavy machine gun: two deliberate bursts, slow and hard. |
| `machine_gun_02` | mono | 3.06 | -16 | 0.6 | A light machine gun: quick tearing bursts. |
| `tank_gun` | mono | 3.51 | -16 | 3.2 | A tank fires: a flat, hard crack with a boom under it, and the empty case ringing on the turret floor. |
| `tank_gun_02` | mono | 3.74 | -16 | 3.2 | A tank fires, a second voice. |
| `explosion` | mono | 5.24 | -15 | 3.3 | A big explosion with earth and wreckage coming down: an assault going in, a bridge blown, a dump hit. |
| `explosion_02` | mono | 5.23 | -15 | 3.2 | An explosion in a street: the walls throw it back. |
| `explosion_03` | mono | 6.12 | -15 | 3.3 | A store of fuel or shells goes up: one burst, then more inside it, and a long roar. |
| `bridge_blown` | mono | 5.79 | -15 | 3.2 | A bridge is blown: the charges go, the span groans and drops into the river. |
| `battle` | stereo | 7.81 | -17 | 2.9 | A fight heard from a little way off, five seconds of it: rifles, machine guns, shells bursting, a tank gun. Played once when a battle is resolved. |
| `battle_loop` | stereo, loop | 16 | -21 | 3.3 | The same fight going on and on: the bed under the face-off card and under any battle that is being watched. |

## Armies on the move and at work (14)

Tanks, lorries, half-tracks, boots, horses, digging, building, a factory.

| Name | Kind | Seconds | LUFS | Small speaker | What it is and when it plays |
|---|---|---:|---:|---:|---|
| `tank_engine` | mono | 4.88 | -20 | 2.7 | An armoured corps sets off: the engines take up the load and the tracks begin to clatter. Played when armour is picked up or ordered to move. |
| `tank_loop` | mono, loop | 8 | -22 | 1.2 | Tanks on the move: engines and tracks, going round for ever. Carried by an armoured army while it marches across the map. |
| `truck_engine` | mono | 4.29 | -20 | 4 | Lorries pull out: a motorised corps sets off, the gears whining as they pick up speed. |
| `halftrack` | mono | 4.52 | -20 | 4.2 | Half-tracks pull out: a lorry's engine with tracks under it. For German and American motorised corps. |
| `convoy_loop` | mono, loop | 8 | -23 | 4.1 | A column of lorries on the road. Carried by a motorised army while it marches. |
| `marching` | mono | 4.33 | -20 | 3.5 | Infantry steps off: eight paces of a column on a road, boots together and the rattle of kit. Played when infantry is picked up or ordered to march. |
| `marching_loop` | mono, loop | 8 | -25 | 2.9 | A column marching, going round for ever at the same pace. Carried by an infantry army while it marches across the map. |
| `horses` | mono | 4.01 | -20 | 3.4 | Cavalry moves off at a canter: hooves on turf and the jingle of harness. |
| `horses_loop` | mono, loop | 7.4 | -23 | 3.5 | Cavalry on the move, going round for ever. Carried by a cavalry army while it marches. |
| `dig_in` | mono | 3.39 | -22 | 2 | An army digs in: shovels going into earth, a pick, spoil thrown on the parapet. For the order to fortify or to hold. |
| `hammer` | mono | 2.15 | -22 | 0 | Something is being built: three blows of a hammer on iron in a yard. Played when works are ordered from the arsenal or begun in a town. |
| `build_works` | mono | 4.14 | -22 | 0 | A building going up: a saw, hammering on timber, a winch's ratchet. For a town building ordered or finished. |
| `factory_loop` | mono, loop | 8 | -30 | 3.2 | A factory town at work: the thud of presses, belts running, a hiss of steam. Very quiet; heard only when the eye is close to a town whose works are busy. |
| `parachutes` | mono | 5.83 | -22 | 1.6 | Airborne troops go out over their target: a transport overhead and the canopies cracking open one after another. |

## Railways (10)

A steam train from every side: going, stopping, turning round at the line's end, the rails cut and mended.

| Name | Kind | Seconds | LUFS | Small speaker | What it is and when it plays |
|---|---|---:|---:|---:|---|
| `train_loop` | mono, loop | 9.6 | -24 | 2.7 | A steam train running at an easy speed: the quick beat of the exhaust, wheels over the rail joints, the rumble of wagons. Carried by each little train on the map; heard only close to. |
| `train` | mono | 5.49 | -21 | 0 | A train pulls out: a short whistle and the first heavy beats of the exhaust quickening. Under the name the 3D game already calls (it has been silent until now). |
| `train_depart` | mono | 6.96 | -21 | 0.5 | A heavy train starts away: the couplings draw out one by one, the wheels slip once, and the beat settles and quickens. |
| `train_arrive` | mono | 7.04 | -21 | 0.5 | A train runs in and stops: the beat slows, the brakes squeal, the buffers close up down the train and steam is let go. |
| `train_turn_round` | mono | 9.35 | -21 | 0.6 | A train reaches the end of its line and turns back: it brakes to a stand, the buffers close, steam blows off, one short whistle, and it pulls away the other way. Played at the end of a line as a little train turns round, in place of train_loop for these seconds. |
| `train_whistle_01` | mono | 4.2 | -22 | 0 | An engine's whistle, high and clear: one short and one long. |
| `train_whistle_02` | mono | 5.18 | -22 | 0 | A deep three-note whistle, long and lonely: the big engines of the Americas and the steppe. |
| `rail_ride` | mono | 6.68 | -21 | 0 | An army is sent by rail: the guard's whistle, carriage doors slammed down the platform, the engine's answer and the first beats away. Played when the order to ride is given: a train, never boots. |
| `rail_cut` | mono | 5.45 | -16 | 3.1 | A railway is cut: the charge or the bomb, rails wrenched and ringing, ballast coming down. |
| `rail_mended` | mono | 4.46 | -22 | 0.2 | A line is open again: the last spikes driven home and an engine's whistle to try the road. |

## Ships and the sea war (15)

Sirens, engines, big guns, sonar, depth charges, torpedoes, a ship going down, landing craft, carriers.

| Name | Kind | Seconds | LUFS | Small speaker | What it is and when it plays |
|---|---|---:|---:|---:|---|
| `ship_horn` | mono | 4.51 | -19 | 2.2 | A big ship's horn: deep, slow to speak, carrying over the water. Played when a fleet is picked up, sails or changes its orders. |
| `ship_horn_02` | mono | 4.24 | -19 | 0 | A destroyer's siren: three rising whoops. For escorts and submarine hunters. |
| `ship_guns` | mono | 7.37 | -15 | 3.3 | A salvo from a turret of heavy guns: three reports close together and the long roll back across the water. |
| `ship_guns_02` | mono | 8.47 | -15 | 3.2 | A full broadside: two turrets, one after the other. |
| `shore_bombardment` | mono | 8.57 | -16 | 3.1 | A fleet shells the coast: the salvo, the shells rumbling away overhead, and the bursts inland a few seconds later. |
| `ship_loop` | mono, loop | 8.8 | -25 | 1.9 | A fleet under way: the slow thrum of engines and the sea along the hull. Carried by each fleet at sea; heard only close to. |
| `anchor_chain` | mono | 4.4 | -22 | 0.5 | A fleet comes to anchor: the cable roaring out through the hawse and the splash. For a fleet sent to harbour. |
| `sonar_ping` | mono | 2.62 | -24 | 0 | The submarine hunter's ping going out, and its faint echo coming back off something. For destroyers hunting and for a submarine found. |
| `depth_charge` | mono | 5.96 | -16 | 2.2 | A depth charge: the splash, the deep muffled blow underneath, and the sea heaving up and falling back. |
| `torpedo_hit` | mono | 5.66 | -15 | 3.1 | A torpedo strikes: a blow against the hull, a column of water, and plates ringing. |
| `ship_sinking` | mono | 10.09 | -19 | 0 | A ship goes down: her plates groan, the sea pours in, air and steam come roaring up, and she is gone. Played with the picture of a ship sinking. |
| `submarine_dive` | mono | 5.11 | -21 | 0 | A submarine dives: two blasts of her horn, the vents roaring, the sea closing over. |
| `landing_craft` | mono | 6.7 | -20 | 3.9 | Troops go ashore: the craft's engines, the surf, the ramp chains running and the ramp down on the beach. For a landing ordered and a landing made. |
| `carrier_launch` | mono | 6.49 | -20 | 0.6 | An aircraft leaves a carrier's deck: the engine opened right up, the run, and away over the bow. |
| `sea_battle_loop` | stereo, loop | 16 | -23 | 3.3 | A fleet action heard across the water: salvoes far and near, shells falling in the sea. The bed under a sea battle that is being watched. |

## Fire (5)

One building alight, a town burning at night, a roof falling in.

| Name | Kind | Seconds | LUFS | Small speaker | What it is and when it plays |
|---|---|---:|---:|---:|---|
| `fire_small_loop` | mono, loop | 9.4 | -29 | 0.3 | One building alight: a steady crackle with a small roar under it. Carried by a siege fire, a burning depot, a works that has been hit. |
| `fire_town_loop` | stereo, loop | 17.8 | -26 | 1 | A town burning by night: the roar of many fires drawing air, the crackle on top, timbers going now and then. Carried by a bombed town for as long as its smoke and its red glow last. |
| `fire_start` | mono | 3.52 | -24 | 0 | Something is set alight: the whoosh as it catches and the first crackle. For the order to burn stores, bridges or a town's works before a retreat. |
| `fire_collapse_01` | mono | 4.02 | -19 | 3.4 | A burning roof falls in: timbers crack, the fall, a rush of sparks. |
| `fire_collapse_02` | mono | 4.1 | -19 | 3.5 | A wall comes down in a burning street, a second voice. |

## The desk (interface) (42)

Every button, card, order and page: the sounds of a staff officer's desk.

| Name | Kind | Seconds | LUFS | Small speaker | What it is and when it plays |
|---|---|---:|---:|---:|---|
| `ship_bell` | mono | 4.19 | -24 | 0 | A ship's bell: two pairs of strokes. For the fleet's cards and orders, as the stamp is for the army's. |
| `click` | stereo | 0.22 | -30 | 0 | Any button pressed: a small firm click, like a bakelite switch. The commonest sound in the game, so it is short and soft. |
| `ui_primary` | stereo | 0.28 | -27 | 0.3 | The one brass button of a screen pressed (END THE MONTH, Attack, Send): a heavier click with brass in it, and its release. |
| `ui_dial` | stereo | 0.2 | -32 | 0 | A dial moved one stop (the history dial, the difficulty, a volume): the small sharp click of a detent. |
| `tick` | stereo | 0.19 | -38 | 0 | The smallest sound on the desk, hardly more than a breath: a row chosen in a list, a card turned in the newsreel. Never played for the pointer merely passing over something. |
| `ui_back` | stereo | 0.22 | -32 | 0.2 | Close, cancel, step back: the same click, lower and softer, so that going back sounds like going back. |
| `ui_tab` | stereo | 0.27 | -32 | 0 | A tab chosen: a card flicked forward in an index. |
| `switch_on` | stereo | 0.24 | -29 | 0.1 | A setting turned on, the lamp lit, night to day: a heavy toggle thrown over. |
| `switch_off` | stereo | 0.23 | -30 | 0.4 | A setting turned off, the lamp put out, day to night: the toggle thrown back, a shade lower. |
| `ui_error` | stereo | 0.45 | -28 | 0.7 | That cannot be done: two dull knocks on the desk. Never a buzzer. |
| `ui_notify` | stereo | 1.6 | -27 | 0 | Something wants attention: one touch on a desk bell. |
| `card_open` | stereo | 0.4 | -29 | 0.4 | A card laid on the table: it slides in and lands. For every card that opens (an army's, a town's, a report). |
| `card_close` | stereo | 0.32 | -31 | 0 | A card taken up again: the same slide, backwards and lighter. |
| `page_turn` | stereo | 0.63 | -29 | 0.3 | A page of a heavy book turned: for the ledger, the roll of honour, the guide, and moving from one report to the next. |
| `screen_open` | stereo | 0.52 | -27 | 3 | A whole room of the game opened (the arsenal, research, diplomacy, the ledger): a deep drawer pulled out and a book set down. |
| `screen_close` | stereo | 0.64 | -28 | 3.3 | That room closed: the book shut and the drawer pushed home. |
| `chart_unroll` | stereo | 0.92 | -28 | 0.3 | A chart unrolled across the table: for the raid planner, the sea chart, the map views. |
| `stamp` | stereo | 0.49 | -24 | 3.6 | An order confirmed, and the WON stamp: a rubber stamp brought down firmly on paper. The sound of "it is done". Fire it as the stamp lands, not as it starts to fall. |
| `stamp_low` | stereo | 0.56 | -24 | 3.9 | The LOST stamp: the same stamp as "stamp", lower and heavier, so that good and bad are told apart before the word is read. |
| `stamp_light` | stereo | 0.45 | -29 | 2.6 | Something bought or ordered: the same stamp, lightly, as a clerk marks a chit. For the everyday order; "stamp" is for the ones that matter. |
| `pickup` | stereo | 0.29 | -30 | 0 | A piece lifted off the table: an army or a fleet taken in the hand. (The 3D game already calls this name; it has been silent until now.) |
| `putdown` | stereo | 0.26 | -28 | 0.3 | A piece set down on the table: wood on wood, and it settles. |
| `order_march` | stereo | 0.43 | -28 | 0 | An order to march drawn on the map: one stroke of a wax pencil. |
| `order_attack` | stereo | 0.73 | -26 | 0 | An attack ordered: two hard strokes of the red pencil, an arrowhead, and the pencil rapped on the map. |
| `order_cancel` | stereo | 0.55 | -30 | 0 | An order taken back: the mark rubbed out. |
| `select_region` | stereo | 0.22 | -34 | 0.1 | A province chosen on the map: a fingertip on paper. |
| `coins` | stereo | 1.24 | -27 | 0 | Money spent: a few coins counted down onto the desk. |
| `coin_run` | stereo | 0.54 | -28 | 0 | The treasury figure counts to its new value: one short run of coins, done inside the 400 ms the figure takes. |
| `alert_slip` | stereo | 0.57 | -28 | 0 | An alert slip arrives: a telegraph sounder rattles out three strokes. Dry and short, so that several alerts in a month do not nag. |
| `pen_sign` | stereo | 0.85 | -29 | 0 | A paper signed: a steel nib across it and a full stop. For treaties, pacts, promotions, a save made. |
| `typewriter` | stereo | 1.95 | -27 | 0 | A line typed and the carriage thrown back: for a report opened, a dispatch written. |
| `type_key` | stereo | 0.23 | -33 | 0.1 | One typewriter key: for each line of the month card as it is set out, and for figures that count up. |
| `teleprinter` | stereo | 2.04 | -28 | 0.4 | The teleprinter wakes and rattles out a message: news from outside coming in (dispatches, another country's move, an intercept). |
| `telephone` | stereo | 1.65 | -26 | 0 | The telephone on the desk rings twice: another government wants a word. For an offer, an ultimatum, a cabinet matter. |
| `radio` | stereo | 3.03 | -28 | 0 | A wireless message through the static: dots and dashes (they spell the game's initials). For intelligence, spies, a ship's report. |
| `projector` | stereo | 6.1 | -30 | 0.8 | A film projector running: the newsreel's own noise, under its pictures. |
| `month_ask` | stereo | 2.43 | -25 | 0 | The question before the month ends ("armies at the front have no orders"): a clock's single soft chime, and two ticks. A pause, not an alarm. |
| `month_end` | stereo | 2.81 | -23 | 0.9 | The month is ended: the calendar page torn off, the day book shut, one stroke of the clock. Played once when END THE MONTH is pressed. |
| `month_passing_loop` | stereo, loop | 4.4 | -30 | 0 | The other countries moving, very fast: a clock running quick and a teleprinter chattering somewhere. Goes round under the playback of everyone else's month. |
| `month_card_open` | stereo | 0.91 | -25 | 1.1 | The month's report laid before you: a dispatch case unlatched and the card drawn out onto the table. |
| `note_arrives` | stereo | 0.63 | -27 | 0 | A staff note arrives: a slip of paper slid onto the desk and a knuckle's tap to say "read me". For each lesson of the first hour. |
| `note_done` | stereo | 1.4 | -24 | 2.9 | A staff note stamped DONE: the stamp, and a small bright touch of the desk bell after it. The stamp alone is "stamp". |

## Calls (28)

The band's short pieces for the great moments, and the sirens.

| Name | Kind | Seconds | LUFS | Small speaker | What it is and when it plays |
|---|---|---:|---:|---:|---|
| `air_raid_siren` | mono | 10.28 | -21 | 0 | The air raid warning over a town: the wail winds up, wavers twice and runs down. Played from the place as a raid comes within sight of it. |
| `siren` | mono | 6.97 | -21 | 0 | The same warning, shorter, under the name the 3D game already calls. |
| `siren_all_clear` | mono | 7.46 | -22 | 0 | The all clear: one steady note held and let run down. Played from the place when the last raid of the night has gone. |
| `bell` | stereo | 4.53 | -23 | 0.1 | One stroke on a deep bell: a decision taken that cannot be taken back (a road of history chosen, a treaty put to the table). |
| `bugle` | stereo | 4.95 | -20 | 0 | The campaign begins: the command figure, once, on a single cornet. Our own five notes; a bugle could not play them. |
| `victory` | stereo | 3.55 | -20 | 1.4 | A battle won, a town taken: the last three notes of the command figure, rising, with the band under them. Short, because it is heard often. |
| `defeat` | stereo | 5.35 | -21 | 2.6 | A battle lost, a town fallen: the same three notes turned down instead of up, low brass and strings, a muffled drum. |
| `drums` | stereo | 3.74 | -21 | 1.3 | The two sides are drawn up: a roll on the side drum and four strokes, the bass drum under them. Before a battle is joined. |
| `newsreel` | stereo | 6.07 | -20 | 0.5 | The band at the head of the newsreel: two bars of our own march, brass and side drum, ending open so that the news can follow. |
| `campaign_start` | stereo | 6.43 | -19 | 1.7 | You take command: a kettledrum roll and the whole command figure from the full band. For the moment a country and a year are chosen. |
| `war_declared` | stereo | 6.43 | -19 | 2.8 | War is declared: a kettledrum roll that swells, a dark chord from the low brass, and one heavy stroke. For you, on you, or between two others. |
| `treaty_signed` | stereo | 6.06 | -22 | 0.7 | A treaty signed, an alliance made, a peace: two warm chords from the brass and a bell. Calm after "war_declared". |
| `city_taken` | stereo | 6.12 | -20 | 1 | A great city is yours: three tower bells and the rising figure over them. For a capital or a prize city, not for every town. |
| `city_lost` | stereo | 6.26 | -22 | 2.5 | A great city is lost: one bell tolls twice, low strings under it. |
| `nation_falls` | stereo | 8.28 | -20 | 0.8 | A country is beaten and leaves the war: the command figure broken off and falling, muffled drums, a last low note. |
| `nation_freed` | stereo | 7.08 | -20 | 1.2 | A country is set free and comes back into the war: bells from every tower and the figure whole again. |
| `world_won` | stereo | 11.75 | -19 | 2.2 | The campaign is won: the whole of our march, four bars, drums and all. Heard once, at the very end. |
| `campaign_lost` | stereo | 11.88 | -20 | 2.4 | The campaign is lost: the figure once more, slow and in the dark, the drum with a cloth on it, and silence after. |
| `month_card_good` | stereo | 3.64 | -23 | 0 | The month went your way: two soft rising chords, a light touch of the bell. Under the month card when the news is good. |
| `month_card_bad` | stereo | 4.16 | -23 | 1.6 | The month went against you: two chords that sink, low strings, a muffled stroke. Under the month card when the news is bad. |
| `month_card_quiet` | stereo | 3.58 | -25 | 1.2 | A month in which nothing was settled: one held chord, neither up nor down. Under the month card when the news is mixed or thin. |
| `month_yours` | stereo | 2.54 | -24 | 0 | The others have moved and the month is yours: two notes, like a hand on the shoulder. Never loud. |
| `alert` | stereo | 2.13 | -21 | 0 | Something needs you now (a landing on your shore, an attack on you, a government in danger): two quick notes and a drum tap. |
| `research_done` | stereo | 2.08 | -24 | 0 | The workshops have something new: three small bells climbing, like a good idea arriving. |
| `medal` | stereo | 2.91 | -22 | 0 | A medal pinned on, a general promoted: a ruffle on the drum and two bright notes. |
| `drum_low` | stereo | 2.58 | -23 | 5.7 | A red band crosses the screen (war, a country that may fall): one low drum stroke in the hall. When the moment has its own call ("war_declared", "nation_falls"), play the call and not this. |
| `unit_lost` | stereo | 4.05 | -23 | 3.7 | A division is destroyed, a ship goes down with her name: one muffled stroke and a low note falling. |
| `note_closing` | stereo | 5.94 | -22 | 0.6 | The last staff note of the first hour ("the command is yours"): the command figure, gently, with a bell on its last note. Heard once in a player's life. |

## Ambience (30)

Beds (long loops of pure texture) and sprinkles (single events dropped in at random).

| Name | Kind | Seconds | LUFS | Small speaker | What it is and when it plays |
|---|---|---:|---:|---:|---|
| `room_day_loop` | stereo, loop | 24 | -33 | -0.7 | The war room by day: the air of a big panelled room, the wall clock, somebody typing two rooms away. Heard when the camera is high above the map, and under every card. |
| `room_night_loop` | stereo, loop | 24 | -34 | 0.2 | The war room at night: the staff gone home, the clock, the lamp hissing, wind at the window. The same room, emptier. |
| `land_day_loop` | stereo, loop | 28 | -30 | 0.1 | Open country by day: wind in grass and trees, and birdsong a long way off, too far to pick one bird from another. Heard when the camera comes down over land in daylight. |
| `land_night_loop` | stereo, loop | 28 | -31 | 0.1 | Open country by night: crickets, and a little wind. No birds. The change from the day bed is the plainest sign that night has come. |
| `winter_loop` | stereo, loop | 28 | -30 | 0.1 | Winter: a hard wind that finds every gap, and fine snow driven on it. No life in it at all. For snow-covered country, by day or night. |
| `desert_loop` | stereo, loop | 28 | -30 | 0.1 | Desert: a dry steady wind, and sand hissing along the ground. For North Africa and the like. |
| `jungle_loop` | stereo, loop | 28 | -30 | 0 | Jungle: a wall of insects, frogs, water dripping from leaves. For Burma, the islands and the Pacific shore. |
| `rain_loop` | stereo, loop | 24 | -30 | 0 | Steady rain on open ground. For bad weather over land; lay it over the day or night bed, and thin the birds out. |
| `sea_day_loop` | stereo, loop | 30 | -30 | 0.8 | The open sea by day: the swell lifting and falling away, and wind. Heard when the camera comes down over water. |
| `sea_night_loop` | stereo, loop | 30 | -31 | 1.8 | The sea at night: the same swell, slower and darker, with less wind. The gulls are gone. |
| `front_loop` | stereo, loop | 32 | -31 | 5 | A front line a long way off: the guns muttering on the horizon, never still and never near. Laid under the land bed when the camera is over a province where armies face each other. |
| `bird_01` | mono | 1.64 | -28 | 0 | Sprinkle, land by day: a small songbird, one phrase, near. |
| `bird_02` | mono | 2.55 | -28 | 0 | Sprinkle, land by day: a two-note call, twice. |
| `bird_03` | mono | 2.08 | -28 | 0 | Sprinkle, land by day: a quick trill and an answer. |
| `bird_04` | mono | 2.07 | -28 | 0 | Sprinkle, land by day: a crow, three times, a field away. Good over farmland and over a battlefield after the fighting. |
| `owl_01` | mono | 2.93 | -28 | 0 | Sprinkle, land by night: an owl. One hoot, then three. |
| `owl_02` | mono | 2.08 | -28 | 0 | Sprinkle, land by night: another owl, one long shaking note. |
| `gull_01` | mono | 3.05 | -28 | 0 | Sprinkle, sea and harbour by day: a gull, three cries. |
| `gull_02` | mono | 3.3 | -28 | 0 | Sprinkle, sea and harbour by day: a gull, one long cry and two short. |
| `bell_buoy` | mono | 6.44 | -28 | 0 | Sprinkle, sea by night and harbours: a bell buoy rocking, three uneven strokes. |
| `church_bell_far` | mono | 8.15 | -28 | 0.5 | Sprinkle, land by day in country that is at peace: a church bell across the fields, three strokes. |
| `far_guns_01` | mono | 6.17 | -26 | 5.6 | Sprinkle, near a front: a far salvo, four guns, and its roll. |
| `far_guns_02` | mono | 6.92 | -26 | 5.8 | Sprinkle, near a front: two heavy shells landing far off. |
| `far_guns_03` | mono | 7.95 | -26 | 5.3 | Sprinkle, near a front: a long ragged ripple of fire, a battery firing in turn. |
| `thunder_01` | mono | 11.04 | -25 | 1.8 | Sprinkle, storms: thunder a long way off, rolling. |
| `thunder_02` | mono | 9.28 | -25 | 3.7 | Sprinkle, storms: thunder nearer, with a dull crack before the roll. |
| `room_typing_01` | stereo | 3.26 | -34 | 0 | Sprinkle, war room by day: a clerk typing a line next door. |
| `room_typing_02` | stereo | 3.68 | -34 | 0 | Sprinkle, war room by day: a line typed, the bell, and the carriage sent back. |
| `room_steps` | stereo | 3.85 | -34 | 3.9 | Sprinkle, war room: somebody walks past the door on a wooden floor. |
| `room_papers` | stereo | 1.74 | -34 | 0.1 | Sprinkle, war room: papers gathered and squared on a desk. |

## Stings (3)

Short mixes of the sounds above that are themselves sounds, for a card that reports something it cannot show.

| Name | Seconds | Made from | What it is and when it plays |
|---|---:|---|---|
| `raid_day` | 7.5 | `bomber_heavy_loop`, `fighter_pass_01`, `flak_burst_02`, `flak_burst_01`, `flak_burst_03`, `bombs_falling_01`, `bomb_stick_03` | For the raid card (and the browser game) when the raid was flown by day: the steady drone of heavy bombers, a fighter going through, flak, the bombs. No siren, no lamps. |
| `raid_night` | 9 | `air_raid_siren`, `searchlight_on`, `bomber_medium_loop`, `flak_heavy_fire_01`, `flak_burst_01`, `flak_burst_04`, `bombs_falling_02`, `bomb_stick_01`, `fire_town_loop` | For the raid card (and the browser game) when the raid was flown by night: the siren, a lamp struck, the uneven beat of the night bombers, flak, the bombs, and fire after. |
| `rail_move` | 5.5 | `train_whistle_01`, `train_depart` | For the browser game when an army is sent by rail: the whistle and the engine pulling away. Set against "marching", it is how a rail move and a march are told apart with the eyes shut. |

## Target scenes (7)

Not for the game: these are what the game should sound like when the sounds are laid together. Each is a list of cues in `scene-list.json` (when, which sound, how loud in dB against the file, where from left to right).

| Scene | Seconds | What happens in it |
|---|---:|---|
| `scene_night_raid` | 44 | A town with heavy guns is bombed at night. Crickets and quiet; the siren; the lamps strike; the bombers come over with their uneven beat; the guns open and the shells burst; a lamp catches one; the bombs come down; the town burns; the bombers go home; the all clear; the fire is still burning when the crickets come back. |
| `scene_day_raid` | 38 | The same town bombed by day. Birds and wind; the deep steady drone of the four-engined bombers, far steadier than the night bombers; their escort weaving above; the defending fighters meet them and there is a dogfight; flak; the bombs; one bomber goes down; they go home. No siren wail in the dark, no lamps, no crickets: a different picture altogether. |
| `scene_march_and_train` | 46 | Two ways to move an army, told apart with the eyes shut. First a column steps off and marches: boots, in time. Then a train: the whistle, the engine pulling away, the beat of the exhaust and the wheels over the joints; a flak wagon on it fires at something overhead; at the end of the line it brakes, the buffers close up, and it sets off back the way it came. |
| `scene_month_end` | 30 | The war room at the end of a month. A few orders given at the desk; END THE MONTH; the question ("armies at the front have no orders"); the answer; the month ends; the other countries move, quickly; the month is yours again; the month card is laid in front of you with good news; you turn its page and close it. |
| `scene_attack` | 32 | A gun line and an assault. The drums as the two sides are drawn up; the guns open; shells go over and land; tanks start and roll; rifles and machine guns; the noise of the fight; and it is won. |
| `scene_fleet` | 34 | The sea war. Gulls and the swell; a fleet puts out with a blast on the siren; the engines; destroyers hunt a submarine (the ping, then the charges); the big guns fire a salvo and the shells land; a torpedo strikes. |
| `scene_first_command` | 16 | The first hour's lessons. A staff note arrives on the desk; you do what it asks (a piece picked up, an order pencilled, the piece put down); the note is stamped DONE; and, at the very end of the hour, the closing note: the command is yours. |
