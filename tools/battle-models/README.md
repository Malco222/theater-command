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
`LandingCraft` (an infantry boat, 36 men in her well) and `TankLandingCraft` (three tanks on her deck). The aircraft a
carrier ranges on her deck are models of their own, kept with the ships: `DeckPark_CarrierFighter`, `DeckPark_Hellcat`,
`DeckPark_Zero` (see "A carrier's deck" below). Aircraft: the game's own fifteen (`Bf109`, `Spitfire`, `Zero`, `Mustang`, `Yak`, `Fw190`,
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
break in two), `tear.fore` and `tear.aft` (the deck's last strip at her break, each hinged across her: see "Where
she breaks" below), `turret.A/B/…` with `turret.A.gun.1…` (turn, then slide for the run-back), `director.*`, `radar.*`,
`sec.P1…/sec.S1…` (secondary mounts, port and starboard) with their guns, `tubes.N` (torpedo tubes), `screw.N`,
`rudder`, a carrier's `lift.N` and `deckpark`, a submarine's `periscope.N` and `planes.bow/stern`.

Sockets: `muzzle.turret.A.1…`, `muzzle.sec.P1.1…`, `aa.N` (light guns), `funnel.N`, `fire.N` (where fires start,
bow to stern), `hit.port.N` / `hit.starboard.N` (torpedo and shell strikes along the waterline), `wake.bow`,
`wake.stern`, `wash.N` (the screws), `ensign`, `jack`, `torpedo.*`, `depth.*`, `break` (where her back breaks: steam
and smoke pour from the gap), a carrier's `launch`, `land` and `lift.N`.

Clips: `sail` (loop), `fire.<turret>` and `fire.sec.*`, `broadside.heel`, `hit`, `list.port/starboard`,
`sink.bow`, `sink.stern`, `capsize.port/starboard`, `break` (a magazine hit: she breaks in two);
destroyers `torpedoes.port/starboard`, `depth_charges`; carriers `lift.N`; submarines `dive`, `surface`,
`periscope`, `torpedoes.bow/stern`.

**Where she breaks.** The first sinking showed her parting "along a straight, clean vertical line, flat plating on
both sides of a dark wedge, like a model sawn in two" (the art director, 2026-11-18: destruction must look broken,
never cut away). So her halves meet along a ragged line (`Yard.hull`, `tear`: by her beam, 0.6 to 2.6 m fore and
aft of the break; a submarine's is still 0, a clean cut). The frame at the break has its points thrown fore and aft
by turns and both skins share it: whole, she shows nothing; broken, each half ends in teeth of plating that fit the
other's. Her deck's edge is not thrown, so what stands on her deck is as it was; a scuttle or a seam near the break
belongs to the half whose plating it lies on (`Yard.half_of`, `Yard.jag_at`). Behind each ragged edge a fine model
has (`Yard.torn_ends`) the plating's thickness, a dark lining, a bulkhead a few metres in, two frames (the nearer
stands bare wherever the plating is torn back past it), her lower decks broken off short or long, and tongues of
plating bent inward: these reach past the break into the other half while she is whole and stand out of the torn
end when the halves part. Nothing that must show outside her can be hidden so: what is bent outward has to be a
part that moves. The deck's last strip on each half is therefore `tear.fore` / `tear.aft`, a turn part hinged
across her at the next frame, and the `break` clip heaves it up in its first third of a second (+ lifts the torn
edge of `tear.fore`, - that of `tear.aft`). The clip's event `steam` at the socket `break` asks for steam and smoke
from the gap until it goes under. A carrier's flight deck and hangar are still cut clean above her hull.

A submarine seen close: her tower's top is an open bridge inside a thin screen with a turned-out lip (the hatch, the
sight on its post and the periscope standards stand inside it), her deck gun is an open mounting on a pedestal with no
shield (`style="open"` of `Yard.turret`: its breech lifts with the gun), her light gun a single slender one, her
casing has free-flooding slots with round ends (Germany's in two rows, America's a row of arched limber holes) and a
deck laid in slats, with bollards, a capstan, hatches and her anchor in its bed. Germany's boat has the spray
deflector round the fore end of her tower and a direction-finding loop; America's a door in each side of her
fairwater. Parts, sockets and clips are as they were; only the plain (`far`) mesh keeps a solid block for the screen.

Her tower is plated (seams round it and up it, a row of rivets beside each: `plating` in `ships.py`), with small
flooding holes above its foot, and the foot curves out to the casing. She is the most streaked ship in the fleet (the
art director, 2026-11-18): rust weeps down from her slots, her bollards and her anchor's bed, each weep a dark brown
core with a softer orange-brown edge, most of them running into the dark band at her waterline; salt dries in a pale
chalky bloom along the edge of her casing, on the lower third of her tower and in runs down from the lip of her
bridge screen; a dark, rusty band lies above her boot topping where the sea washes her. (First built at about half
this strength, she "read new from the yard" from the battle camera: judge weathering on the picture of the whole
boat and on the sheet, not on the close looks.) Two close-up lessons from the same review: a weep under every
hole of a row, all of a length and crisp, reads as a dotted line of "little flowerpots", so the weeps under her
tower's holes are narrower than the holes, taper to nothing, and vary (a few holes none, most a short run, one or two
long ones out over the curve of the tower's foot); and salt alone is a broad soft tongue, palest a quarter of the
way down its middle and thinning to nothing at its edge, never a wedge with straight sides. Her rust is her own colour, 8A4E28: her file says so in `rust`, and
a file without the key takes the usual 6E4A34. Her boot topping's top is level at 0.55 m (`boot` of `Yard.hull`: the ships' rule by the height of the deck
does not suit so low a side). Her torpedo doors lie under water (they had been drawn at the waterline: `Hull.side`
serves only above it), and the `torpedo.bow.*` and `torpedo.stern.*` sockets with them.

**Streaks: what lies on a skin.** Weather in these files is a number at each point (`wear` toward rust, `soot`
toward black, and where a model carries any, `salt` toward a pale grey: `info.salt`, D2D5CF if it is not given), so
a streak finer than the skin's own points needs points of its own. It is a ribbon lying on the skin, three points
across (`Model.band`, or `Model.ribbon` along a line): its middle points carry the rust or the salt, most at its
head, and its edges none, so it fades to either side. With `core` a ribbon is five points across (`Model.bands`
takes any number of lines): a weep of rust has a middle line and two shoulders that carry rust and darkening, and
edges that carry none. Five things keep it from showing as a panel, and the first four were each found by a picture
from 6 m:

- No two things lying on one skin may overlap: neither can show through the other, and where they share a triangle
  of the skin they fight. So the casing's salt lies on the rows above her slots, where no rust runs; a weep is made
  narrower, or left out, where it would touch the one beside it (`laid` in `streak`); the tower's bloom lies between
  the weeps from the holes at its foot.
- Its paint is the skin's own colour under the skin's name with `.on` after it (`hull.on`, `upper.on`:
  `Model.on_paint`), so that pictures and the game can treat it apart. In the pictures it is seen by the camera
  and by nothing else (`stage.ship_look`), so it stops no light: otherwise its edge draws a hairline of shadow
  and the skin seen under its near edge is dark. In the game give `.on` paints no shadow and a small depth bias.
- When the mesh is made its points are brought down onto the skin's drawn triangles and take the skin's shading
  there (`lay_on_skin` in `rigid.py`). A ribbon's own facing differs from the skin's by a degree or two, and a low
  sun shows that as a panel.
- The skin's own weather is kept broad where streaks lie (nothing finer than a few metres on a submarine's side),
  so a ribbon's edge, which carries only that, cannot be told from the skin under it.
- It must not lie across a ridge of the skin deeper than its lift (8 mm on her side, 3 mm on her tower, under the
  seams): a streak down her side has its points at the heights of her side's own rows, a run along her side is laid
  on those rows and frames themselves, and a run down her tower keeps within one flat of its wall.

A file has the list `salt` (0 to 100 at each point, like `wear` and `soot`) only where a model carries any; the
.glb and .fbx carry it already in their point colours.

The convoy's ships are ships like the rest (`ship`, `fore`, `aft`, a screw, a rudder, every ending), with the guns
such ships were given: `turret.A` in the bows and `turret.Y` on the stern (clips `fire.A`, `fire.Y`), light guns at
`aa.N`. The tanker has one clip more, `ablaze` (her cargo takes fire: oil fire at her fire sockets, burning oil on
the water at her hit sockets).

Landing craft add a `ramp` (a turn part: plus lowers it), the socket `ramp` at its lip (where men and tanks go off),
and the clips `beach` (she takes the ground), `ramp.down` and `ramp.up`. What they carry is in parts of its own so
the game can hide it or stand its own models there: `troops.1`, `troops.2` in the infantry boat (stand-in figures),
`cargo.1…3` in the tank landing craft (stand-in tanks, each with a socket of the same name facing the ramp). A
landing: `sail` while she runs in, then `beach`, then `ramp.down`; unload; `ramp.up` and she backs off.

### A carrier's deck

The three carriers are told apart by their islands first. `Carrier_USA` has the tall American island (the art
director, 2026-11-18: built low, 8 m to the bridge roof with a 12 m funnel, it "reads like a Japanese island"; the
historian's sizes, his own reckoning from plans and photographs, scaled from a 266 m ship): four bridge levels to
about 11 m over the flight deck with a director box and its aerial on top; a stout mast to 28 m with the big square
air-search aerial on a platform; a big flat-sided upright funnel in the after half, its top at 17.5 m (`flat=True`
of `Yard.funnel`); a second director at the after end and a smaller aerial on a pole by the funnel; and, the surest
sign of all, four twin gun houses on the flight deck (`style="house"` of `Yard.turret`), two before the island and
two abaft it, the inner of each pair on a deckhouse, firing over the outer. They are parts `sec.S1` to `sec.S4`,
counted from forward; her port pair, `sec.P1` and `sec.P2`, stay on their sponsons under the deck's edge.
`Carrier_JAP` keeps a small island well forward with her smoke led out over the side; `Carrier` a middling one
with the funnel in it.

A wall the size of that funnel, left bare, reads as a plain slab (the art director again), so it shows its work:
soot over its top third, fading down in uneven tongues and further down the after side (laid point by point on the
wall's own rings, since the general weathering fades over the top half of a three-ring wall and hardly shows);
three steam pipes up its after face, each to its own height, with a dark lip; a railed gallery round its sides and
after end two thirds of the way up, on brackets; a railed searchlight platform on each side; and ladders, one up
each side to the gallery and another from there to the top. `railing` and `ladder` in `ships.py` serve any other
wall or platform that wants them.

A carrier's own mesh has no aircraft on it. Her part `deckpark` is empty: its pivot is the deck under the foremost
row of her range, about a quarter of her length abaft her middle. Her file says which range stands there, by the
battle's month:

```
"deck_park": [{"model": "DeckPark_CarrierFighter"}, {"from": "1943-01", "model": "DeckPark_Hellcat"}]
```

Take the last entry whose month has come (the first has no `from`). `Carrier_USA` wears the generic carrier fighter
until 1943 and then the Hellcat, `Carrier_JAP` the Zero, `Carrier` (Britain and every other navy) the generic
carrier fighter. Stand the range's origin at the pivot of `deckpark`, x forward and z up as the ship's are, and let
it ride that part, so it rolls and goes down with her. When her air group is up, hide it.

A range (`deckpark.py`) is eleven machines, two abreast and one between, noses forward. Each is that aircraft's own
plain mesh (the `far` mesh of her own file: the same shape, paints, camouflage blend and marks as the one that
flies) with nobody in her, standing on her undercarriage with her tail down and a wooden chock (`chock`) before and
behind each main wheel. Her airscrew is stopped, each machine's at its own angle (eleven stopped alike read as one
aircraft copied eleven times), and the last hand's breadth of each blade is a paint of its own, `blade_tip`: yellow
where her nation's airscrews wear yellow tips (America's and Britain's). On the generic fighter it is the blade's
black, and the file's `blade_tip_for` gives the yellow and the owners whose machines wear it. A Zero has no such
paint: Japan's navy airscrews wore no yellow (see "Japan's navy airscrews" under Aircraft). A Hellcat is ranged without the tank under her middle. Parts:
`park` (root) and `plane.1` … `plane.11` (fixed, pivot on the deck under the machine), so the game can take them
off the deck one at a time. In the file: `machine` (the aircraft it is made of), `places` (where each stands),
`pitch` (how far her nose is up, degrees), and the aircraft's own `schemes`, `mark_months`, `nation_paint`,
`number_ink` and `finish`, so a range is painted by the month exactly as the aircraft is: the Hellcats go from the
three blues to gloss sea blue in 1944-07, the Zeros from pale grey to dark green over grey in 1942-07, and the
American star changes with the rest. The generic fighter is painted for her owner as the flying one is
(`nation_paint`): on the American carrier in 1942, blue-grey over light grey with the plain star.

Two keys on a range's `marks` that an aircraft's do not have: `part` (the machine the mark is on) and `up` (which
way is up for the mark, since she stands nose-high). The range's plain mesh (`far`) is a simple stand-in shape per
machine, about 2,000 triangles for the eleven; the fine one is 37,000 to 42,000.

### Decks by the month

A ship's file may carry `schemes`, read exactly as an aircraft's are: a list of `{from: YYYY-MM, name, colours}`
where `colours` holds only the paints that change, and she is painted in the last one whose month has come. So far
there is one: American wooden decks (`planks` on `Battleship_USA`, `Cruiser_USA`, `Carrier_USA`) are stained dark
blue from 1941-12 (`DECK_STAIN` in `ships.py`); pale wood was the peacetime deck. Japan's decks stay wood and
Britain's armoured flight deck stays grey. The pictures show a ship in her last coat unless a month is given.

### Using them in a battle

How a battle would use them: train turrets by setting each turret's turn to (bearing to target - rest bearing)
inside its limits, lift guns by range, then play `fire.<turret>` on each salvo. Damage lights fires at `fire.N`
sockets (more fires as she is hurt). A loss is one of the endings, chosen by what sank her: gunfire `sink.bow`
or `sink.stern`, torpedoes `capsize.*`, a magazine `break`. No wake on a sinking ship. Where she goes under the
sea boils white (heaviest where the last of her went down, lighter along her side where the sea is climbing it),
and a dark sheen of oil wells up there and spreads.

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
  The bars of such a nose are her skin's own finish and thin (`nose_frames_paint`, `nose_bar` their thickness beside
  an ordinary canopy's), never pale: pale, thick bars make a white birdcage of her. The B-29's is a smooth dome of a
  few large panes (`nose_cap`: where the ribs begin, so no knot of bars at the tip); the German heavy's is a finer
  lattice. The two differ, as they did.
- **Each own model has her own shape.** A carrier plane that shares a body with the plain ones reads as the same
  plane in other paint. The Hellcat is deep and portly with a chin intake (`scoops`), a short hood with metal
  behind and square-cut tips; the Avenger a deep barrel with a belly that steps up to her lower gun and a squared
  fin; the Val has the rounded, elliptical wing. The plain three stay plain and borrow none of these.
- **Later coats of paint** (`SCHEMES` in `planes.py`; the file's `schemes`). The same machine in other colours as
  the war goes on, from the art director's unit table (`carrier_paint`). Each entry has `from` (the month it comes
  in, `YYYY-MM`), `name`, `colours` (only the paints it changes, by paint name: lay them over the file's `colours`
  by `paint_names`) and, where it changes, `number_ink`. Paint her in the last one whose month has come.

  | file | to begin with | later |
  | --- | --- | --- |
  | `Hellcat` | three blues over white | overall sea blue from 1944-07 |
  | `Dauntless`, `Avenger` | blue-grey over light grey | three blues over white from 1943-02, overall sea blue from 1944-07 |
  | `Zero`, `Kate`, `Val` | pale grey | dark green over grey from 1942-07 |

  The Dauntless and the Avenger are one colour on top to begin with, but their skins are laid out for the three
  blues: the band down their sides and their fins are `upper2`. `airlook.py ... scheme=1944-07` shows a machine in
  a later coat.

  Overall sea blue was the war's one gloss coat, so that entry alone says `finish: "gloss"`: give it a modest sheen
  (the pictures use roughness 0.36, shine 0.4; `GLOSS` in `airshow.py`), never a mirror. Every other coat is matt.
- **The American star by month** (`MARK_MONTHS` in `planes.py`; the file's `mark_months`). The marking changes with
  the calendar on every American aircraft, whatever her paint: `USA-RED` (the star with a red centre) to begin
  with, `USA` (the plain white star) from 1942-06, `USA-BAR` (the star with white bars and a blue surround) from
  1943-09. A file whose `marks` are American carries `mark_months`: a list of `{from, tag}`, the first with no
  `from`. Swap the tag of each of her marks for the last one whose month has come. The red surround of the summer
  of 1943 is left out. `marks.py` draws all three.
- **Japan's navy disc by month** (the same `mark_months`). The five navy types (Zero, Kate, Val, Nell, Betty) wear
  the plain red disc, `JAP-PLAIN`. From 1942-10 to 1943-12 the disc on the FUSELAGE alone has a thin white ring
  round it, `JAP-RING` (the ring a tenth of the disc's width, outside it, in the markings' matt off-white); from
  1944-01 it is plain again. The wings' discs stay plain throughout. So an entry of `mark_months` may carry `on`,
  and then it holds only for the marks whose `place` is that word. Every aircraft's mark says its `place`: `side`
  (her fuselage), `wing`, `fin` or `code` (her number). An entry with no `on` holds for all her marks.
- **Cowl gills** (behind a radial engine; a mesh of their own in the sculptor, `gills`). In the file the ring of
  small flaps behind the cowl is faces of the fuselage in the cowl's paint (`nose`), or, where her cowl is painted
  like the rest of her (`nose` the same colour as `upper`), in `upper` above and `under` below. So a later coat
  laid on by paint name colours them with her. A thin dark line (`line`) marks the gap between each pair, so that
  they are seen without a pale collar round her neck.
- **The Val's spats** (`SPATS` in `planes.py`): the outer face of each spat is in her upper paint and the inner
  face in her under paint, following the line down her side, so that a green Val has green spats, not white socks.
- **The yellow tips of a turning airscrew** (`blur_tip`; the file's `clear.blur_tip` is 0.035) are three soft rings
  laid over each other, widest and faintest outermost, so that the tip ring is as blurred as the blades and about
  a third as strong as it was: a faint warm haze at the rim, not a gold wire.
- **Japan's navy airscrews** (`NAVY_AIRSCREW`, `NAVY_BANDS`, `navy_airscrew` in `planes.py`; the Zero, Kate, Val,
  Nell and Betty, flying and ranged on a deck). They wore no yellow tips. The front of each blade is bare metal
  (`blade`), its back red-brown (`blade_back`), with two thin red bands across it near the tip (`blade_band`, cut
  into the blade at 0.80 to 0.835 and 0.885 to 0.92 of its length), and the spinner is bare metal. Her turning disc
  is pale metal (`blur`) with a faint red ring (`blur_tip`) where the bands lie, as clear as the yellow one is.
- **Matt paint, and less light from below** (`SKIN` and `BACK_LIGHT` in `airshow.py`, the pictures only). The war's
  paints were matt: roughness 0.6, shine 0.25. And the sunlit cloud and haze under her light her belly with half
  their brightness, or a white belly is as bright as the sky behind it and a pale wing seen from just below is lost
  against it (a wing with dihedral shows her under paint to a camera level with her).
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
gives how clear under `clear` (`glass` 0.3, `blur` 0.07, `blur_tip` 0.035), and then her crew are seen through the
hood. The disc of a turning airscrew (`prop.N.blur`) is a faint disc that leaves the spinner clear, the smear of
each blade over it, and a thin yellow ring near the rim where the blades have yellow tips (British and American
machines; a Japanese navy machine has a faint red ring where her blades' red bands lie); drawn as one even grey disc
it reads as a plastic plate.

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
python export.py <out> parks                   # the three deck ranges alone (they are also in `ships` and `all`)
python show.py Carrier_USA turn <out> month=1942-06   # a carrier in a month: her range and its paint are that month's
python shipcheck.py loose all                  # every piece of every ship that touches nothing else of her, and points in no face
python shipcheck.py boxes Battleship_JAP       # her largest plain boxes and bars, to see that none is out of scale
```

Run both checks after a change to `ships.py` or `shipyard.py`. A picture shows one corner of a ship; these look at
all of her. `boxes` is the one that finds a fitting built far too large (the aerial over the Japanese towers was once
a solid sheet 20 m wide, because a loop had reused the name of the tower's size: never name a loop's counter after a
size that is wanted after the loop).

Rendering is Cycles on the CPU (EEVEE cannot start without a display). A 960x540 frame at 12 samples takes 10 to
25 seconds on four cores. `stage.py` holds the sea (swell, whitecaps, haze, a painted wake), the sky, a cloud
deck, and the effects (soft puffs: smoke, spray, flame, splashes, a magazine blast). `marks.py` draws national
markings for the pictures only; the game paints its own from the model's `marks`.

The wake (`paint_wake` and `Sea.wake` in `stage.py`) is a picture painted once to the ship's measures: red the white
water, green the churned green under it, blue the crests of her bow wave. Two lessons (the art director, 2026-11-18,
of a carrier's clip). The crests are laid on the sea as painted and never multiplied by the streaks that go by with
the water: cut across by those, each arm of the bow wave became "a row of bright white dashes of equal size and
spacing, like road paint". An arm is a faint crest that comes and goes in lengths of every size, wanders, swells and
thins, soft at its ends. And the foam astern is one pale streak without a break, whitest at the stern, that widens
and fades in uneven patches: a hard cut to white-or-nothing made it a fan of white stripes.

Flame and the light of a flash (`flames`, `light`, `gun`, `burst`, `fire`, `blast` in `stage.py`). Flame is ragged
tongues thrown out from the place that burns, deep orange, gone in a frame or two, breaking at once into smoke: a
gun's flash drawn as smooth glowing balls in a row read as a rocket's exhaust, and a magazine drawn as big soft balls
read as a balloon. The light a flash throws is weak and short: only what stands within a few lengths of the gun is
lit, to about half as bright again as the day, for a frame or two, one light for a turret's salvo, and it hardly
shines in the water. At a hundred times that the whole ship went near-white and a glittering road of light ran down
the sea, like a low sun.

Flame's colour is measured, not judged by eye (the art director, 2026-11-18: the bridge fire, the magazine and the
gun flash had all passed by eye and measured peach, about 230,177,140, where deep orange is 195 to 230 / 110 to 125 /
40 to 55). Two causes. The tone curve (AgX) bleaches a bright flame toward white whatever its hue: a flat patch
thrown at 2.5, 0.77, 0.08 comes out 248,191,151 and one at 0.95, 0.18, 0.03 comes out 225,118,58, so a tongue is
thrown at about that (its colour is a seventh of it, the stuff's strength being 7), redder and dimmer at its tip,
with a small paler heart. And a tongue that dies by going thin all over lets the pale sky through, which is peach
again: it dies by being eaten away and stays solid where it is left. `flamemeasure`-style check: take the warm pixels
of a frame (red well over blue) and read their middle.

Smoke seen later and from near by (`trail` in `airshow.py`, `_place` in `stage.py`). A trail's puffs of one age
all live as long as each other and drift little, or those that die early leave the rest standing as a chain of
separate ovals; and a puff thins away as the camera comes up to it, or its rim shows like a flat glass plate. Bullet
strikes are a spit of small orange tongues and a dark scrap, never a round glowing bead.

White water at a sinking (`froth`, `boil`, `steam`, `slick` in `stage.py`; `sink` in `show.py`). Three lessons.
Foam laid about the middle of her deck is under her and none of it shows: it is laid outside the line of her side,
at the places where the sea has climbed more than half way to her deck's edge (`_goes_under` finds them from her
pose). From a camera low over the sea a flat patch is a sliver a few pixels deep, so white water needs height:
spouts against her side, and where the last of her went down domes of air bursting up several metres. And the
heaviest of it belongs in the middle, where the broken ends went down, with the oil welling up there and spreading
outward over the seconds; each half's own line is lighter. The oil is seen edge on from that camera too, so it starts
as soon as she breaks, spreads fast at first and reaches well clear of her side (85 m in the battleship's clip), and
it lies under the foam, never over it.

Foam on the sea (`foam` and the kind "foam" in `stage.py`; a splash's foot and the froth where a ship goes under).
One broad flat patch, however ragged its stuff, reads as a white plate laid on the sea. Churned water is a patch at
the heart and a scatter of smaller ones about it, each drawn out into a streak, each with its own life, drifting a
little apart; the stuff has holes and lace all through it and thins to nothing at its rim, pale green where it is
thick and white where thin (faint: pushed harder it reads as paint), and as a patch dies its holes open rather than
the whole growing paler. Kept off pure white (about 0.84, 0.9, 0.88 in the sun). The oil slick keeps the plain
"flat" stuff: dark and matt but for a tenth of dull shine, and how far a patch reaches differs all the way round it,
so its rim is bays and tongues (a round rim showed from the battle camera as a smooth curve).

A shell's splash (`splash` and the kinds "jet" and "ring" in `stage.py`). It goes up fast and narrow. Built of soft
puffs thrown up under gravity alone, its first half second was "a soft white cotton-wool mound swelling at the
waterline, like steam from a kettle" (the art director, 2026-11-18). A column is jets of solid water, each a ball
drawn out tall and stood upright, thrown to its own height against a strong drag, so it is at four fifths of its
height in two frames: a dense core that narrows upward, thin streaks close in along its sides, a feathered head of
thin jets each at its own height and lean, drops falling out of it, and a dark ragged ring of disturbed water
spreading at its foot. The soft puffs come in slowly after it and are only the mist at its base and the curtain that
falls; `settle=(a, b)` brings that curtain down, spread and gone, between two ages of the splash (left alone it
hangs four seconds, and in the sinking it stood "as a white rope by her bow" in front of the blast). Three things
that went wrong on the way: jets out of the top that stand parallel read as a fork; thin jets
that are fat and splayed read as petals; and stuff that tears only at its very rim reads as a wax candle (it tears in
from the outer quarter now, and most toward its upper end).

Mending a clip by frames. A clip need not be rendered again for one fault: `show.py` and `airshow.py` take
`frames=43,44,45`, `stage.film` skips a frame that is already on disk and still steps every effect through it, and
`kit.encode` joins the folder. Render the mended frames from a copy of the code the clip was made with, changed only
where the mend is. For a thing added (the splash's column), give it dice of its own and names of its own (`rng` and
`name` of `puff`): the other puffs then keep their throws and their names, and a puff's name decides its pattern, so
the first frame after the mend comes out as it was. Check that by subtracting the two. For a thing moved (a bomber
stepped clear of another's smoke), find the frames it is in by projecting it into the camera.

Two lessons of the close fly-past, kept in `airshow.py`. The camera goes nearly all the way round the machine, so
somewhere it looks up-sun, and there the sun's glare off the wings washed the camouflage out to cream: the sun now
stands so that the glare falls in the gap the camera never visits (`SUN`), and a painted skin shines a little less
(`SKIN`). A machine that only comes late in the war carries its `year` in its file, and its sheet label says
"from 1944"; one that needs a technology and has no year says "by research".

## Not done yet

- Sound: events are keyed to the sound director's names where a real recording exists (`ship_gun_heavy`,
  `ship_gun_medium`, `ship_guns`, `shell_impact_near`, `magazine_explosion`, `fire_loop`, `fire_small_loop` (an
  aircraft's engine burning), `depth_charge`, and under way two loops together by her size: `ship_wash_loop` or, for
  destroyers, surfaced submarines and landing craft, `destroyer_wash_loop`, over `ship_engine_loop` or, for merchants
  and the liner, `ship_steam_engine_loop`; `plane_guns`, `gunner_burst`, `ricochet_01`, `plane_down`, `plane_explode`,
  `fighter_engine_loop`, `aero_engine_idle_loop`, `b17_drone_loop`, `b24_drone_loop`, `bomber_medium_loop`,
  `stuka_drone_loop`, `jet_engine_loop`; list in `/mnt/project-files/sound/battle-pack/sound-list.json`). Brian approved the
  pack on 2026-11-18 (ruling 144 is met), so the game may now play these: MP3s with `mp3-list.json` for the browser game,
  `wav/` for the 3D game, credits owed in the pack's README. Every keyed name with a recording is in the pack (checked against the 59 ship
  and aircraft files). A model file carries names only; no sound file is added by these tools. Names with no recording
  yet, which stay silent: `bomb_release`,
  `bomb_doors`, `aircraft_break_up`, `parachute_open`, `ship_sinking`, `torpedo_launch`,
  `torpedo_run_loop`,
  `depth_charge_roll`, `depth_charge_thrower`, `dive_alarm`, `surfacing`, `lift_motor`, `craft_grounds`,
  `ramp_drop`, `ramp_winch`.
- Two exports of the same code write ten aircraft (those with gunners: the heavy and medium bombers, the Stuka, the
  Val, the Avenger) with the same points in another order over part of the mesh. Each file is whole and right in
  itself; do not match one file's points to another's by their numbers. The cause is not found yet.
- Effects are named in events but not built for the game; the pictures use Blender-only effects from `stage.py`.
- Aircraft that fly have no undercarriage that lowers (the machines of a deck range stand on fixed legs of their
  own). No wings that fold and no arrester hook that drops, so a range is spread wings and all.
- A deck range is fighters only. The torpedo and dive bombers of an air group are not ranged yet.
- Camouflage patches are blended from point to point of the skin, so their edges are as soft as the skin's grain is
  coarse. The art director would like the 1940 British and German schemes tight-edged (a hand's width); that needs
  the skin cut along the patch edges, or the patches drawn in the game's shader. Soft is right for the Soviet
  greens and the Me 262's mottle.
- One plain mesh (`far`) per model; no third level of detail.
- The men and tanks in the landing craft are stand-ins; the land team's soldiers and tanks should stand there.
- No beach in the pictures' stage, so there is no clip of a landing yet.
