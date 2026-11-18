# Battle models: warships and aircraft

Models for the real-time sea and air battles, made wholly by script in the cloud (Blender as a Python module, no
window, no Mac). The battle is decided by its numbers (ruling 217); these models only act the result out. Every
shape is our own: no names, art or models from any other game. Markings are each nation's plain period marking.

The sister set (infantry, tanks) is the land team's, in `/mnt/project-files/battle-models/land/`. Both sets use
one format, described here.

## Where things are

| What | Where |
| --- | --- |
| These scripts | `tools/battle-models/` in the repository |
| The models for the game | `/mnt/project-files/battle-models/sea/` and `air/`: `<Name>.json`, `.glb`, `.fbx` |
| What each model has (parts, sockets, clips) | `/mnt/project-files/battle-models/index-sea-air.json` |
| Review clips and line-up sheets | `sea/clips/`, `sea/sheets/`, `air/clips/`, `air/sheets/` |

Ships: `Battleship_UK/GER/USA/JAP/ITA/FRA`, `Battleship`, `Cruiser_UK/GER/USA/JAP/ITA`, `Cruiser`, `Destroyer`,
`Destroyer_GER/USA/JAP`, `Carrier`, `Carrier_USA/JAP`, `Submarine`, `Submarine_GER/USA` (the plain name is the
layout for any other navy). For convoys and landings (`merchants.py`): `Freighter`, `Tanker`, `Troopship`,
`LandingCraft` (an infantry boat, 36 men in her well) and `TankLandingCraft` (three tanks on her deck). Aircraft: the game's own fifteen (`Bf109`, `Spitfire`, `Zero`, `Mustang`, `Yak`, `Fw190`,
`Hurricane`, `Warhawk`, `Fighter`, `Stuka`, `Sturmovik`, `He111`, `Lancaster`, `B17`, `Bomber`), the jet `Me262`
(a battle's own, from 1944), plus `Parachute` and `Bomb`.

## The file

`<Name>.json` is the game's own model file (`points`, `facings`, `paints`, `triangles`, `blend`, `soot`, `wear`
for a fine mesh `near` and a plain one `far`, and `colours`) with four things added:

- `part` (in `near` and `far`): for every point, the number of the part it belongs to. A point belongs to one part.
- `parts`: `{name, parent, pivot, kind, axis?, limits?, slide?, slide_limits?, note?}`. `kind` is `root`, `fixed`,
  `turn` (turns about `axis` through `pivot`), `slide` (moves along `slide`), `spin` (turns without limit) or
  `free` (a piece that can break away: a clip gives it a turning and a move).
- `sockets`: `{name, part, at, facing, note?}`: named places for flash, smoke, fire, wake and so on. A socket rides
  its part.
- `clips`: `{long, loop, about, after?, tracks, events}`. A track moves one part: `turn` `[[t, degrees], ...]`,
  `slide` `[[t, metres], ...]`, `rot` `[[t, w, x, y, z], ...]`, `move` `[[t, x, y, z], ...]`, `scale`
  `[[t, s]]` or `[[t, sx, sy, sz]]`. Read keys in a straight line between times (quaternions by slerp) and hold
  them outside. `after` says what follows a clip that does not loop: `hold` its last pose, or `gone` (take the
  model away). An event is `{t, fx, socket, cue}`: start effect `fx` at `socket`, play sound `cue`.

Rules that matter when reading it:

1. Metres; x forward, **y to the right**, z up. Triangles are wound as in the game's other model files.
2. A turn of +t degrees about `axis` carries a point p (measured from the pivot) to
   `p cos t + (axis x p) sin t + axis (axis . p)(1 - cos t)`.
3. A part's own pose is applied about its pivot, then its parent's, and so on up to the root.
4. A part with both `axis` and `slide` (a gun) slides along its slide direction **as its own turn has carried it**:
   lift the gun, then it runs back along its lifted barrel.
5. A turret's axis is given so that plus is clockwise seen from above; its bearing is its rest bearing plus its
   turn (0 ahead, 90 to starboard, 180 astern). Fore turrets rest at 0, after turrets at 180.
6. Poses from several clips are laid over each other part by part (turnings multiplied, moves added): a ship can
   ride the swell (`sail`), heel to her own broadside (`broadside.heel`) and run her guns back (`fire.A`) at once.

The `.glb` and `.fbx` carry the fine mesh skinned to a skeleton with one bone per part (same names) and every
clip that moves a part as an action. Events and sockets are only in the `.json`.

## Ships

Parts: `ship` (root), `fore` and `aft` (her two halves: everything is a child of one or the other, so she can
break in two), `turret.A/B/…` with `turret.A.gun.1…` (turn, then slide for the run-back), `director.*`, `radar.*`,
`sec.P1…/sec.S1…` (secondary mounts, port and starboard) with their guns, `tubes.N` (torpedo tubes), `screw.N`,
`rudder`, a carrier's `lift.N` and `deckpark`, a submarine's `periscope.N` and `planes.bow/stern`.

Sockets: `muzzle.turret.A.1…`, `muzzle.sec.P1.1…`, `aa.N` (light guns), `funnel.N`, `fire.N` (where fires start,
bow to stern), `hit.port.N` / `hit.starboard.N` (torpedo and shell strikes along the waterline), `wake.bow`,
`wake.stern`, `wash.N` (the screws), `ensign`, `jack`, `torpedo.*`, `depth.*`, a carrier's `launch`, `land` and
`lift.N`.

Clips: `sail` (loop), `fire.<turret>` and `fire.sec.*`, `broadside.heel`, `hit`, `list.port/starboard`,
`sink.bow`, `sink.stern`, `capsize.port/starboard`, `break` (a magazine hit: she breaks in two);
destroyers `torpedoes.port/starboard`, `depth_charges`; carriers `lift.N`; submarines `dive`, `surface`,
`periscope`, `torpedoes.bow/stern`.

The convoy's ships are ships like the rest (`ship`, `fore`, `aft`, a screw, a rudder, every ending), with the guns
such ships were given: `turret.A` in the bows and `turret.Y` on the stern (clips `fire.A`, `fire.Y`), light guns at
`aa.N`. The tanker has one clip more, `ablaze` (her cargo takes fire: oil fire at her fire sockets, burning oil on
the water at her hit sockets).

Landing craft add a `ramp` (a turn part: plus lowers it), the socket `ramp` at its lip (where men and tanks go off),
and the clips `beach` (she takes the ground), `ramp.down` and `ramp.up`. What they carry is in parts of its own so
the game can hide it or stand its own models there: `troops.1`, `troops.2` in the infantry boat (stand-in figures),
`cargo.1…3` in the tank landing craft (stand-in tanks, each with a socket of the same name facing the ramp). A
landing: `sail` while she runs in, then `beach`, then `ramp.down`; unload; `ramp.up` and she backs off.

How a battle would use them: train turrets by setting each turret's turn to (bearing to target - rest bearing)
inside its limits, lift guns by range, then play `fire.<turret>` on each salvo. Damage lights fires at `fire.N`
sockets (more fires as she is hurt). A loss is one of the endings, chosen by what sank her: gunfire `sink.bow`
or `sink.stern`, torpedoes `capsize.*`, a magazine `break`. No wake on a sinking ship. She leaves a small ragged
oil slick.

## Aircraft

All are built by the game's own sculptor (`game/build_plane.py`, copied from the `unreal` branch) and
then taken apart. Her shape, colours, camouflage blend and markings are exactly the game's. For a battle the
sculptor is run at a finer grain (`FINER` in `planes.py`: 40 points round the fuselage, a ring every 15 cm, 16
points along the chord), so that her skin is smooth with the camera a wing's length away, and `fitout.py` then
adds what is seen at that distance:

- her crew under the glass (head, helmet, goggles, mask, shoulders, life jacket), seats, a fighter's sight;
  a bomber's gunners in their turrets and the men in a glass nose;
- exhaust stubs or pipes with their stain down the cowling, gun ports (doped red patches on British wings),
  muzzles, cowling fasteners, wingtip and tail lamps;
- rows of rivets along frames, stringers, spars and ribs (small dark squares, the fine model only);
- a bomb bay behind two doors with the bombs on their racks (`BAYS` in `planes.py`: the bombers, and the
  torpedo bombers that carry their torpedo inside);
- the black rubber along the leading edges of the `B17`, her two open waist windows with a gunner standing at each
  gun (`WAIST` in `planes.py` cuts them out of her skin);
- her tail gunner in a closed position: three small framed panes in each side and a glazed panel over his guns
  (`PANES` in `planes.py` turns those faces of her skin to glass; `fitout.tail_gunner` frames them and lines the
  compartment dark);
- her ball turret as a steel ball in the belly's paint (`BALL` in `fitout.py`): the game's glass dome is left out,
  the ball lifts with its two guns (it is in part `turret.N.guns`) and has one round window between them with the
  gunner curled up behind it, and the yoke it hangs in trains (part `turret.N`).

The aerial wire from the mast to the fin is left off every machine (Brian, 2026-11-18); the mast stays.

`Me262` is a battle's own (the game has no jet): its numbers are `EXTRA_PLANES` in `planes.py`, in the game's form
with one new key, `jets` (the pods under the wings). Three small patches (`JETS`) let the game's sculptor leave the
airscrew off and skin the pods; `fitout.jets` adds the intake lips and bullets, the tailpipes and the cannon ports.
She has no `prop.N` parts and no flaps; her sockets add `jet.N` (each tailpipe). Her file says `year` 1944 and
`tech` "jets": by the art director's unit table Germany flies her only once jet fighters are researched (the rules'
tech `jets`), and whether a battle mixes her in among the Fw 190s is the air battle builder's call.

**The carrier planes and the big bombers** (Brian, 2026-11-18) are a battle's own too, twelve more entries in
`EXTRA_PLANES`. Paints and first years are the art director's rulings of that day; the file's `year` is the first
year a nation flies her (none: from the start), `tech` the rules' technology she needs, `tag` the model tag the
design director gave her.

| file | what she is | year, tech | crew | she carries |
| --- | --- | --- | --- | --- |
| `Hellcat` | US Navy carrier fighter, three blues over white | 1943 | 1 | a belly tank |
| `CarrierFighter` | carrier fighter for any other nation (`nation_paint`) | | 1 | |
| `Kate` | Japan's carrier torpedo bomber, pale grey | 1937 | 3 | a torpedo, or one heavy bomb for bombing from high up |
| `Avenger` | US Navy torpedo bomber, blue-grey over light grey | 1942 | 3 | a torpedo or bombs, in her bay |
| `TorpedoBomber` | carrier torpedo bomber for any other nation | | 3 | a torpedo or bombs, in her bay |
| `Val` | Japan's carrier dive bomber, fixed undercarriage | 1940 | 2 | a bomb on its crutch |
| `Dauntless` | US Navy dive bomber, holed dive flaps | 1941 | 2 | a bomb on its crutch |
| `DiveBomber` | carrier dive bomber for any other nation | | 2 | a bomb on its crutch |
| `Nell` | Japan's land-based bomber of the early war: two engines, two fins | | 7 | a torpedo or one heavy bomb, under her |
| `Betty` | Japan's land-based bomber: two engines, a glass cone in her tail | 1941 | 7 | a torpedo or bombs, in her bay |
| `Amerikabomber` | Germany's very long range heavy: four engines, two fins | `verylongrange`, tag `amerikabomber` | 8 | bombs |
| `B29` | America's very long range heavy: four engines, bare metal | `verylongrange`, tag `b29` | 11 | bombs |

What they add to the form:

- **A torpedo is a second load.** Part `torpedo` (a slide, like `bombs`), socket `torpedo`, clip `torpedo_away` (a
  bay's doors open first). A machine that can carry either says so in her file (`loads`: `["torpedo", "bombs"]`), and
  she carries one or the other, never both: play `load.torpedo` or `load.bombs` first (each scales the other load
  away) and leave it applied. Extras painted `torpedo` are her torpedo; `BAY_TORPEDO` puts one in a bay. The new
  `Torpedo` model is for the game to drop and run: clips `drop` and `run` (its wooden air fins, part `box`, are
  scaled away in the water; part `screws` turns), sockets `nose` and `wake`.
- **A coat graded down her sides** (`graded` in the numbers, patched in by `GRADED`): the Hellcat's sea blue on top,
  a paler blue down her sides and on her fin, white beneath.
- **Dive flaps full of holes** (`HOLED` in `fitout.py`): the Dauntless.
- **Blisters.** A dome off her middle line is fixed to her and never a turret; a gun in it is a `flexgun.N`. On the
  machines in `BLISTER_CREW` a man is seen at each one (the Betty's and the Nell's waist gunners; the B-29's three
  gunners, who sight through theirs for turrets they do not sit in).
- **Turrets with nobody in them** (`REMOTE` in `planes.py`): the B-29's four are low steel domes, parts `turret.N`
  and `turret.N.guns` like any other.
- **A nose of glass with no step** (`windows_back`): the windows over the pilots, behind the part that is glass all
  round, are framed like the nose. Both pilots and the bomb aimer are seen in it (`NOSE_CREW`).
- **Bare metal** (`bare` in the numbers; the file says `finish: "bare metal"`): draw her painted skin (`upper`,
  `upper2`, `under`, `nose`) as metal. The pictures use metallic 0.85, roughness 0.4.
- The B-29 has two bomb bays. The model opens the forward one; the rear one's doors are scribed in her belly
  (`SHUT_BAYS`).

**Her record, painted on her** (Brian, 2026-11-18: an ace's kills by his cockpit, a bomber's missions as rows of
small yellow bombs, a star over a mission she led). Each aircraft file carries a `tally`: lists of slots, each slot a
place on her port side and the way her skin faces there (`x, y, z, nx, ny, nz`, the game's axes, like a `marks`
entry), with the mark's `radius` (half its height) and how many stand in a row. The game paints as many marks as her
record in the campaign has earned, first slot first; the rules keep the record (kills by the loser's nation, missions
flown, which of them she led). Past the last slot nothing more is painted.

| list | where | mark |
| --- | --- | --- |
| `kills` | under the port cockpit sill, running aft from the windscreen (on a bomber: beneath her bombs) | `KILL-<nation>`: the plain marking of the nation that lost the machine, in small |
| `rudder` | the port face of the rudder (part `rudder`, so it turns with it); fighters Germany flies | `KILL-BAR`: a white bar a kill |
| `missions` | the port side under the cockpit, rows of twenty (sixteen on a medium bomber, twelve on a two-seater) | `BOMB`: a small yellow bomb standing on its nose |
| `lead` | slot k stands over mission k | `LEAD`: a small yellow star, painted if she led that mission |

Who wears what is the art director's ruling (`habit` in the file; a generic machine says "by nation"): Germany's
fighters wear `rudder` bars; the Soviet Union's wear a small red star a kill in the `kills` slots (`KILL-STAR`);
every other nation's fighters wear the victims' marks there. Bombers of Britain and America wear `missions`, `lead`
and their gunners' `kills`; other nations' bombers wear nothing (their slots are in the file all the same). A victim's
mark is never a party emblem: Germany's is the plain cross, Japan's the red disc, Italy's a small green, white and
red flag, France's her roundel, any other nation's the marking the game draws on its own aircraft. No names, nose art
or badges, and nothing copied from a real machine. A mark is about a hand high, matt and a little worn: it is read
only at the nearest camera. For the pictures `marks.record()` draws them: `airlook.py B17 <out> nose,record
record='{"kills": [["GER", 6]], "missions": 32, "lead": [12, 19]}'`; `Plane(..., record=...)` in a clip.

Paints added after the game's own: `torn` (the inside of a torn-off end), `blur` (the disc of a turning airscrew),
and the fit-out's (`inside`, `leather`, `skin`, `suit`, `vest`, `goggles`, `exhaust`, `gunmetal`, `patch`, `lens`,
`boot`, `rivet`, `rivet_under`, `blur_tip`, three lamps). **Draw `glass`, `blur` and `blur_tip` clear**: the file
gives how clear under `clear` (`glass` 0.3, `blur` 0.07, `blur_tip` 0.3), and then her crew are seen through the
hood. The disc of a turning airscrew (`prop.N.blur`) is a faint disc that leaves the spinner clear, the smear of
each blade over it, and a thin yellow ring near the rim where the blades have yellow tips (British, American and
Japanese machines); drawn as one even grey disc it reads as a plastic plate.

Parts: `plane` (root: the fuselage), `wing.port`, `wing.starboard` (each wing outboard of the fuselage with its
engines and guns), `tail` (rear fuselage, tailplane, fins), `prop.N` (blades, numbered from the port side),
`prop.N.blur`, `bombs` where something hangs under her or in her bay, and `torpedo` where she can carry one. Both ends of every break are closed.
Moving surfaces, each a `turn` part with limits: `aileron.port/starboard`, `flap.port/starboard` (machines with
no engines on the wing), `elevator.port/starboard`, `rudder` (or `rudder.port/starboard`), `bay.port/starboard`
(bomb doors, plus opens). Plus on an aileron, flap or elevator is trailing edge down; plus on a rudder is
trailing edge to starboard. Guns: `turret.N` (plus trains it clockwise seen from above) with its child
`turret.N.guns` (plus lifts them), and `flexgun.N` (a gun on a free mounting, keyed by rotation).

Sockets: `engine.N`, `jet.N` (a jet's tailpipe), `gun.N` and `gun.nose` (fixed guns), `turret.N` (gunners' muzzles, riding their guns),
`bombs`, `torpedo`, `hatch`, `hatch.belly`, `wingtip.*`, `tank.*`, `stub.*`, `break.wing.*`, `break.tail`, `fuel`, `trail`.

Clips: `fly` (loop: discs turning, a little life, the controls working), `idle`, `stopped`,
`bank.port/starboard` (ailerons over, elevators up), `peel_off` (fighters), `flaps.down`, `fire`, `fire.turrets`,
`watch` (loop: gunners sweeping the sky), `bombs_away` (the doors open first, then the bombs go), `torpedo_away`,
`load.torpedo` and `load.bombs` (which of two loads she is seen with), `doors.close`,
`hit`, `smoking`, and the three ways down: `fall_burn`, `fall_spin` (the port wing shot off), `fall_explode`
(wings and tail fly off); `bail_out` (one `chute` event for each of the crew who get out: spawn a `Parachute`
there and play its `open`, then `drift`).

**The game flies the machine.** Clips only tip, roll and break her about her own middle. In a fall the game
carries the wreck down: for `fall_burn` along her nose, gaining speed; for `fall_spin` and `fall_explode` as a
falling body that loses its way (see `Plane.fly_on` and `Plane.fall` in `airshow.py` for paths that look right).
A piece that has broken away is still a child of the fuselage in the file; its track already allows for the
fuselage's own turning, so it flutters away rightly if the tracks are simply played.

`Parachute`: parts `chute` (root, the harness point), `canopy` (scaled from a streamer to full), `airman`
(swings). Clips `open`, `drift` (loop), `collapse`. `Bomb`: clip `fall`. `Torpedo`: clips `drop`, `run`.

## Making them again, and the pictures

```
pip install bpy==5.3.0 numpy pillow            # Blender as a Python module (Python 3.13); needs ffmpeg for clips
python export.py <out> all                     # every model: .json, .glb, .fbx, and index-sea-air.json
python look.py Freighter <out> close,aft       # quick stills while building (small craft: small,smallaft)
python show.py Battleship_UK turn <out>        # ship clips: turn, broadside, sink, capsize
python airshow.py shot_down <out>              # aircraft clips: flypast (plane=Me262 for another machine), shot_down, ways_down
python airlook.py Spitfire <out> front,hood     # aircraft close up (front, hood, glass, rear, over, under, side, tail, tailside, waist, belly, ball, record, rudder, nose, ace; clip=bombs_away@1.6; clip=load.bombs@0; record=JSON)
python sheet.py <out.png> bow+beam <names> 2   # a line-up sheet of ships, each from the bow and on the broadside;  python airshow.py sheet <out.png>
```

Rendering is Cycles on the CPU (EEVEE cannot start without a display). A 960x540 frame at 12 samples takes 10 to
25 seconds on four cores. `stage.py` holds the sea (swell, whitecaps, haze, a painted wake), the sky, a cloud
deck, and the effects (soft puffs: smoke, spray, flame, splashes, a magazine blast). `marks.py` draws national
markings for the pictures only; the game paints its own from the model's `marks`.

Two lessons of the close fly-past, kept in `airshow.py`. The camera goes nearly all the way round the machine, so
somewhere it looks up-sun, and there the sun's glare off the wings washed the camouflage out to cream: the sun now
stands so that the glare falls in the gap the camera never visits (`SUN`), and a painted skin shines a little less
(`SKIN`). A machine that only comes late in the war carries its `year` in its file, and its sheet label says
"from 1944"; one that needs a technology and has no year says "by research".

## Not done yet

- Sound: events are keyed to the sound director's names where a real recording exists (`ship_gun_heavy`,
  `ship_gun_medium`, `ship_guns`, `shell_impact_near`, `magazine_explosion`, `fire_loop`, `depth_charge`,
  `ship_wash_loop`, `ship_engine_loop`, `plane_guns`, `gunner_burst`, `ricochet_01`, `plane_down`, `plane_explode`,
  `fighter_engine_loop`, `aero_engine_idle_loop`, `b17_drone_loop`, `b24_drone_loop`, `bomber_medium_loop`,
  `stuka_drone_loop`; list in `/mnt/project-files/sound/battle-pack/sound-list.json`). No sound file goes into the
  game until Brian has judged it (ruling 144). Names with no recording yet, which stay silent: `bomb_release`,
  `bomb_doors`, `jet_engine_loop`, `aircraft_fire`, `aircraft_break_up`, `parachute_open`, `ship_sinking`, `torpedo_launch`,
  `torpedo_run_loop`,
  `depth_charge_roll`, `depth_charge_thrower`, `dive_alarm`, `surfacing`, `lift_motor`, `craft_grounds`,
  `ramp_drop`, `ramp_winch`.
- Effects are named in events but not built for the game; the pictures use Blender-only effects from `stage.py`.
- Aircraft have no undercarriage that lowers; a carrier's deck park uses the plain models. No wings that fold and
  no arrester hook that drops.
- The carrier planes and the Japanese bombers are each in the scheme of their first year. The art director's
  `carrier_paint` table gives the later schemes by year (the US Navy's three blues from 1943 and sea blue from
  mid-1944, Japan's dark green over grey from 1942-43): the game can repaint by `colours`, or the models can be
  written out again per scheme.
- Camouflage patches are blended from point to point of the skin, so their edges are as soft as the skin's grain is
  coarse. The art director would like the 1940 British and German schemes tight-edged (a hand's width); that needs
  the skin cut along the patch edges, or the patches drawn in the game's shader. Soft is right for the Soviet
  greens and the Me 262's mottle.
- One plain mesh (`far`) per model; no third level of detail.
- The men and tanks in the landing craft are stand-ins; the land team's soldiers and tanks should stand there.
- No beach in the pictures' stage, so there is no clip of a landing yet.
