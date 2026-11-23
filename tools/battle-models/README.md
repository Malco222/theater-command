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

- her crew under the glass (head, helmet, goggles, mask, neck and jacket collar, shoulders, life jacket), seats, a
  fighter's sight; a bomber's gunners in their turrets and the men in a glass nose. Every man's neck runs from inside
  his collar up into his head, so no light shows between them (Brian, 2026-11-21: "make sure their necks connect to
  their heads"). A gunner sits wholly inside his turret, his head clear under the dome between his guns, and has
  both hands on them (Brian, 2026-11-19: "he should be in the turrett with 2 hands on the 50 call guns inside"):
  `fitout.arms` gives every gunner upper arms in his own part and forearms and gloved hands in his guns' part, so
  that his hands stay on the grips as the guns lift or swing (turrets, waist guns, a twin tail position, the ball
  turret); close up (`m.fine`) every man has a real face (Brian, 2026-11-21: "can we make their faces more realistic?",
  "maybe use the current soldier faces as a template?"): `crewface.py` takes the battle soldier's head of his own
  country (`crew_heads.json`, made by `make_crew_heads.py` from `battle-models/land/soldier/soldier_<ARMY>.json`:
  five armies, eight faces each), chosen by his machine and seat so he is the same man every build, and fits a
  leather helmet made from his own skull, earphone cups, goggles up or down with their strap, and, where he wears
  one, a rubber oxygen mask close over his nose, mouth and chin, its valve at his chin and a ribbed hose curving down
  over his chest (`fitout.look_of`: fewer masks low down and in Japanese machines; a Japanese airman's goggles mostly
  pushed up). About 2,000 triangles a man; far off he is the plain shapes as before;
- exhaust stubs or pipes with their stain down the cowling, gun ports (doped red patches on British wings),
  muzzles, cowling fasteners, wingtip and tail lamps;
- her plating, over all her skin (`fitout.rivets`, the fine model only; Brian's benchmark of 2026-11-19, below): rows
  of rivets along her frames (every 42 to 56 cm) and fourteen stringers, along the ribs and spars of her wings, tail
  and fin, and in rings round each nacelle; and the joints of her panels, a thin dark strip beside every other frame
  and rib, along four stringers and along two spars. A rivet is a flush dot of paint (12 mm across on a bomber, 9 on
  a fighter; six-sided above, four-sided underneath), a joint 6 to 7 mm wide; paints `rivet`, `rivet_under`, `seam`,
  `seam_under`. A rivet is a faint shade under the coat it lies in, about a third as strong as a joint (the art
  director, 2026-11-19: seen close "real rivets read as a faint dotted line", and the first ones, half as big again
  and dark, read as studs); the plain model has none, so they are gone beyond close range. They cost a fighter
  about 10,000 triangles and a heavy bomber about 30,000;
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

A trimotor (the SM.79, the Ju 52) has `engines` on her wings and `nose_engine`: True as well: her nose airscrew,
spinner, cowling and exhausts are seated as a single-engined machine's are, from the same keys (`spinner`, `screw`,
`blades`, `engine`, `lip`, `nose_back`, `stubs`). Her airscrews are numbered from port: `prop.1` and `engine.1` her
port wing's, then her nose's, then her starboard wing's. Like every machine with wing engines she has no flap parts.

**The benchmark** (Brian, 2026-11-19, of a B-17G model he had found: "use this 3d model is our bench mark for all
airplanes"; "here is more inspiration of the level of detail im looking for"). His pictures are a reference for shape
and finish only: no mesh or texture of theirs is used, and their nose art, names, codes and serials are real
aircraft's and are not copied. The first to meet it is the `B17`, now a battle's own too (`EXTRA_PLANES["B17"]`
replaces the game's numbers): she is laid out again against a three-view drawing of the real B-17G (Wikimedia
Commons, "Boeing B-17G Flying Fortress 3-view line drawing", Kaboldy, CC BY-SA 3.0; measured, not traced), in her
olive drab over grey, with a soft fade in the olive. What her numbers add to the game's form:

- `cabin`: her raised cockpit and the deck behind it are one body with a roof, glass only where its windows are (the
  windscreen, the side windows, a window in the roof over each pilot, the radio room's glazed hatch): `cabin_paint`
  and `cabin_frames` in `planes.py`, patched into the sculptor by `CABIN`. It is a flat-paned greenhouse, not a
  bubble (Brian, 2026-11-19: "see how the cock pit isn't round"): `fitout.cabin_body` makes it a box with eased
  corners (`square`, coming round again over `ease` behind `back`), so that the side windows are flat panes in a
  row and the roof is nearly flat; `screen` is the post between the windscreen's two flat panes (its foot's x and z,
  its head's x and z, then how far each pane sweeps back per metre out) and `pane` the straight-edged glass in each
  (how far out its foot and its top reach, how high its top's outer end, how far its top stands under the roof);
  the rest of each flat is plated over. `skirt` (from this height, down to this one) and `fillet` fill the hollow
  between the cabin's sides and her round fuselage, as a fairing fills a wing root, and her rivet rows run on
  down it;
- `frames_paint`: the bars of her windows and turrets in her skin's paint, not the game's pale frames;
- a fourth value in a row of `glass_bands`: how far up her side that window sits; and a point on a window's edge
  now takes her coat, so no pane has a pale halo;
- `TAIL_CUT` in `planes.py`: where her tail parts from her fuselage (ahead of her long fin fillet, which is in two
  pieces there).

She has the chin turret and its fairing, a cheek gun each side of her nose, the astrodome, the upper and ball
turrets, the waist, radio room and tail guns (her gunner's cabin stands behind her rudder), and her main wheels
show half out of the inner nacelles, as the real machine's do. 91,000 triangles. Her turrets and guns are numbered
afresh: read them from her file and from `gun-arcs.json`, not from memory.

The `Stuka` is the second (Brian, 2026-11-19, of a picture of a Ju 87: "here is a stuka but i want it in the all
black"): `EXTRA_PLANES["Stuka"]`, laid out against a three-view of the Ju 87B-2 (Wikimedia Commons, "Junkers Ju 87B-2
Stuka.svg", Kaboldy, CC BY-SA 3.0; measured, not traced). She is black all over with a soft second tone, her rivets
a shade lighter so that the black never reads as a hole, plain crosses on her sides and wings and nothing on her
fin. Her cranked wing, trousered legs and spats, the siren on each leg, the radiator bath under her nose, the dive
brake under each wing, the struts under her tailplane and the bomb on its crutch are in her numbers; her gunner
faces aft with his gun (`flexgun.1`). 44,000 triangles. Three more keys of `cabin` make her long greenhouse:

- `roof`: all of the cabin above its sill is glass (a greenhouse with no plated roof);
- `hoops`: where a frame arches over it from sill to sill;
- `spine`: a bar along its top.

**The four single-seat fighters** come next (Brian, 2026-11-19: "all 4", the B-17's standard for the `Mustang`,
`Spitfire`, `Bf109` and `Zero`): `fighters.FIGHTERS`, added to `EXTRA_PLANES`, each laid out against a three-view of
the real machine (the P-51D, the Spitfire Mk I's lines, the Bf 109 E, the A6M2; each drawing is named beside her
numbers in `fighters.py`; measured, not traced). They keep the game's own colours and marks as built. Wheels are up
in flight, so a tailwheel that retracted (the Mustang's, the Zero's) is left off; the Spitfire's and the Bf 109's
stay. 27,000 to 32,000 triangles, plain 4,700 to 6,000. What their numbers add:

- `canopy`: a function of her fuselage that gives her glass as framed pieces (a windscreen, a hood, the glazing
  behind it), each a loft through half sections from the foot of its rail up and over to her middle line; where the
  points are few the panes are flat. `hoops` are frames across a piece, `rails` along it, `bars` and `struts` any
  other; `bar` is their half width, `cap` glazes a piece's after end round and `bare` names sections its rails stop
  short of. Every windscreen has a flat front pane between straight posts. `bubble` lays down a blown hood, widest at
  its rail (the Mustang, the Spitfire); `box` flat panes in square frames under a flat roof (the Bf 109); `green` a
  greenhouse of many frames with flat sides and a roof rounded in flat panes (the Zero). The rail a hood slides on
  is painted her coat.
- `opening`: her cockpit is open under the glass, from this x to that and from this many degrees round her side up
  over her top; `planes.py` cuts her skin away there and `fitout.open_cockpit` lines it: her skin again from inside,
  dark; the panel with its dials; the coaming and the gunsight, kept under the windscreen's glass; a floor; a padded
  rim. The pilot sits wholly inside, his right hand on the stick and his left on the throttle.
- `glare` (the game's own key, the dull panel ahead of her windscreen) lies on the round of her nose on a machine
  with a `canopy`, not as a flat strip standing off it (the Mustang's olive drab).
- a row of `scoops` may take a fourth item, `"one"`: that scoop is on her one side only (the Spitfire's radiator
  under her starboard wing, her oil cooler under her port one); `mast` takes a fourth value, how far its top stands
  aft of its foot (less than nothing: the Zero's, raked forward).

**The `Me262`** is laid out again to the same standard (Brian, 2026-11-19, of pictures of one: "here is the ME 262
for insipiration", and of the teeth painted on its nose: "dont do the shark nose"). Her numbers stand before
`EXTRA_PLANES` in `planes.py`, measured from a three-view (Wikimedia Commons, "Messerschmitt Me 262 Schwalbe 3d
drawing.svg", Voytek S, CC BY-SA 3.0; measured, not traced). Her nose is plain: four cannon ports and nothing
painted on it. Plain crosses on her sides and wings, nothing on her fin, no aerial wire. 43,000 triangles. What her
numbers add:

- `taper`: (how much of her width is left at her spine, the height the narrowing begins at, stations of how much of
  the taper she has along her). Her fuselage is a rounded triangle, wide low down and narrow at the spine, not an
  oval: `fitout.hull_body` gives the game's `Body` with the same calls (`at`, `out`, `w`, `h`, `z`) and three more
  (`round_of` a point, `side_at` a height, `top_at` a distance out). Her stations' half widths are still her widest.
  A marking on a sloping side lies on the slope (its `facing` in her file then has a rise).
- her `canopy` is three pieces in the fighters' form (the flat windscreen between its posts, the hinged hood, the
  long fixed glass behind it that runs down to a point on her spine) over an `opening`; the rail's foot stands down
  her side from the cockpit's rim and clear of it, so the rim's padding does not show outside the glass.
- `graded` (her coat by where a point is): splinter wedges of two greys over her wings, her tailplane and along her
  spine, in zones of straight lines (`_ME262_ZONES`: each line flips the colour, and lines next to each other lean
  opposite ways); her sloping sides, her fin and her pods' flanks in the paler grey with broad soft clouds of the
  darker, thicker toward her spine; pale blue-grey beneath.
- `grain`: how far apart her wing's and tailplane's rings stand, when the usual is too coarse (`{"wing": 0.12,
  "tail": 0.11}`; with it the rings along the chord stand closer too, `CHORDS_CLOSE`). A coat is laid point by
  point, so a paint line narrower than the mesh comes out in steps: her lines are 12 cm of sprayed edge on a 12 cm
  mesh, and run straight. Any machine may take the key.
- `lines`: (how wide her scribed lines are against the usual, how far their colour is toward her paler upper
  colour). The art director, 2026-11-19, of her first lines, black and even on pale grey: "she reads like a
  blueprint". The joints her fuselage was built in (`hull_lines`) keep the usual width.
- her soot (in `weather`, as every machine's): each pipe has scorched the tail of its pod, a soft stain runs back
  under her rear fuselage, both in streaks, and each cannon port draws a faint stain back along her nose. An entry
  of `f.stains` may end with how strong it is and how broad against an exhaust's. A jet's soot lies over her rivets
  too.

**The `Val`** (the D3A1, Japan's carrier dive bomber) is laid out again to the same standard, the first of the
carrier machines. Her numbers stand in `planes.py` where her old ones did, measured from a three-view (Wikimedia
Commons, "Aichi D3A1 Val - 3 vistas.svg", MMK-Draw, CC BY-SA 4.0; measured, not traced; nothing of the drawing is
kept but its measures). 43,000 triangles. What is hers:

- a round cowling a little broader than the fuselage behind it, the oil cooler's bath under it, the bare boss of
  her airscrew (she had no spinner); a deep fuselage with a level deck; the elliptical wing, flat out to her legs
  and rising beyond them, its trailing edge run into her side in a fillet; the long low fillet ahead of her fin (the
  first sections of `fin`, each a little higher and beginning further aft); a tailplane as rounded as her wing.
- her greenhouse (`_val_canopy`, in the fighters' `canopy` form): a windscreen with a flat pane between two posts
  and the tube of her bomb sight through it; the pilot's hood, the fixed middle with her mast, and the gunner's hood,
  each a step lower than the one before, as they slid one under the other; a glazed end that falls to his gun. One
  `opening` runs under all of it, from the pilot's panel to behind the gunner. The glazed end is kept as wide as the
  cockpit's rim until the opening ends, or the rim's padding shows outside the glass.
- her legs are fixed: each a trouser and a long spat (`extras` bodies in her coat), the tyre showing under it.
- `brakes` (`fitout.dive_brakes`; any machine may take the key): the slat under each wing that held her speed in
  the dive, (from, to out along her wing; where along its chord its middle lies; half its breadth; how far it hangs
  below her skin), on brackets.
- her pilot has his hands on stick and throttle; her gunner faces aft with both hands on his gun's grips
  (`fitout.rear_gunner`, the fighters thread's: every seat that faces aft with a free gun behind it).
- her pale grey, black cowling and plain red discs are as they were. No names, numbers or unit marks; no aerial wire.

**The `Kate`** (the B5N2, Japan's carrier torpedo bomber) follows her, measured the same way (Wikimedia Commons,
"Nakajima Kate.svg", Kaboldy, CC BY-SA 3.0; measured, not traced; nothing of the drawing is kept but its
measures). 50,000 triangles. What is hers:

- a round cowling broader than the fuselage behind it and the bare boss of her airscrew; a fuselage as deep below
  her airscrew's line as above it; a long straight-tapered wing, flat out to 2.3 m and rising beyond, its trailing
  edge run into her side in a fillet; a tall fin with a straight raked leading edge; a small tailplane on her
  airscrew's line.
- her greenhouse (`_kate_canopy`, built of the Val's sections with side panes that lean in further: `lean` of
  `_val_section`): a windscreen, five hoods each a step lower and narrower than the one before, and a glazed end
  that falls to the gunner's gun. One `opening` under all of it. Pilot, observer and gunner; the gunner faces aft
  with both hands on his gun (its pivot 0.63 m aft of his seat).
- her wheels are up. Her torpedo (`_kate_torpedo`) hangs nose down by two and a half degrees, as hers did, on two
  braces; the heavy bomb she carried instead is in `bombs` (the game shows one of the two).
- pale grey, black cowling, plain red discs, as the Val, so that one carrier's air group reads as one (the art
  director). No names, numbers or unit marks; no aerial wire.

**The `He111`** (the He 111H, Germany's medium bomber) is the first twin-engined machine laid out again, measured
the same way (Wikimedia Commons, "Heinkel He 111 H-1 3-view line drawing.svg", Kaboldy, CC BY-SA 3.0; measured, not
traced). Her numbers in `EXTRA_PLANES` stand in place of the game's own, as the B-17's do. 69,000 triangles. What
is hers:

- a fuselage that is an oval taller than it is wide, and a nose that is a bullet of glass with no step: glass all
  round ahead of her pilot's feet, and from there back to her cabin's last frame down to a sill beside him
  (`glazed_sill`: degrees round her above her middle line, on one of her skin's own rows so that the edge is clean;
  she is metal below it). The frames: a round pane at her tip, ten ribs back to the first hoop, then hoops over
  her cabin from sill to sill and frames along it (`cabin_ribs`).
- her pilot sits to port in the glass, on a seat on its frame, with both hands on the wheel at the head of his
  control column (`nose_wheel`); the man at her nose gun kneels ahead of him with both hands on its grips
  (`nose_gunner`; `fitout.hold_gun`, which serves any man with a free gun in front of him, whichever way he faces).
- her engines hang low, each in a nacelle of her own lines (`nacelle`: stations in the same frame as the game's
  nacelle for an engine in line, so that her airscrews, exhaust stubs and rivets fall where they should), deepest
  where its wheel's doors are, with its radiator bath under it (`scoops`).
- a wing whose middle is straight and thick between her fuselage and her engines, its trailing edge cut forward
  there and run into her side in a long fillet; outer panels that taper and rise; a broad oval tailplane and fin.
- the gunner on her back sits facing aft under a glazed hood that is round in front and open behind him
  (`_he111_canopy`; `opening_plain`: an open position with no instrument panel), both hands on his gun. The bath
  under her has four round windows a side and a gun out of each end; the men who lay in it are not modelled, since
  nothing of them was seen from outside.
- her coat is a splinter of two dark greens close together (343A2E, 404A36: the art director, "one very dark green, with
  the splinter showing only up close or in raking light"), parted by straight lines (`splinter`, as the Me 262's wedges: bands
  across her fuselage, wedges out along each wing and tailplane, one line across her fin) over pale blue, which
  begins low on her sides (`under_from`). Crosses on her wings and fuselage only, and a plain fin (the art
  director). No names, code letters or unit marks; no aerial wire.

**The `Lancaster`** (the B.I, Britain's heavy bomber) is laid out again the same way (Wikimedia Commons, "Avro
Lancaster 3-view line drawings.png", Emoscopes, GFDL and CC BY 3.0; measured, not traced). Her numbers in
`EXTRA_PLANES` stand in place of the game's own. 67,638 triangles; the same parts, sockets and clips as before, so
nothing that flies her changes. What is hers:

- a fuselage that is a deep slab, flat along its top from her cabin to her tail, its underside sweeping up behind
  the bay; the bomb aimer's blister under her nose turret, an astrodome behind her cabin, the turret on her back in
  its long fairing, and a tray under her tail turret.
- her cabin is a long greenhouse of flat panes (`_lanc_canopy`): a raked windscreen, a fore part as wide as she is
  whose side panes lean in to a frame at their shoulder, and an after part that narrows on a rising sill. Her skin
  is open under it from her sills up (`opening`, low on her side: the greenhouse's own metal foot stands on the edge,
  and under the after part runs up where her skin would), with her instrument panel across under the windscreen
  (`opening_cabin`). That foot twists as it climbs, and showed dark patches: a rail's dark inner face was cut
  into triangles across the other two corners from the painted face it backs, so its ridge stood through the
  other's hollow. `fighters.finish` now cuts both across the same corners, for every canopy. Her pilot sits to port
  with both hands on his wheel (`pilot_wheel`; `fitout.wheel`, which the He 111's pilot shares), the flight engineer
  beside him and the navigator behind.
- four Merlins, each in a deep nacelle of its own lines with its radiator's mouth under the spinner (`nacelles`: a
  nacelle for each engine by how far out it is; `scoops`). The inner pair are lower and further forward and run back
  past the trailing edge, where her wheels go; the outer pair end under the wing.
- a wing whose middle is straight to just outside her inner engines, with outer panels that taper and rise; a
  broad tailplane set high on her, and a tall oval fin at each tip, as much of it below the tailplane as above.
- her gunners sit inside their turrets with both hands on their guns, as the B-17's do. Her tail turret has four
  guns: a handle each side joins the upper gun's grip to the lower's, and he has a hand on each
  (`fitout.gunners`).
- her night colours: dark green and dark earth above, black from high on her sides down, black fins. The green
  and the earth are a little darker than the game's day pair and closer together (434A33 and 685A42, about 22 luma
  steps apart: the art director, so that the brown stays brown but sits down into the night scheme and does not
  read as tan). By the art director's ruling there is no rivet at all on her black (`rivets_under`: False), where the
  joints between panels carry the wear instead. Plain roundels and a fin flash; no names, code letters or serials;
  no aerial wire (the drawing shows them; the mast stands without one).

The tools that read a three-view are not in the repository (they hold nothing but arithmetic on a picture): cut
each view out and turn it to stand the usual way, rub out strokes too thin to be her outline (wires, a torpedo
drawn in), find the paper by filling in from the edge, set the scales from her real length and span, and set the
height of her airscrew's line by the middle of her cowling, not by the first ink at her nose. Where a gun stands out
of her nose, her nose is set by hand, and one scale serves the whole sheet (the He 111: her span). A small
or awkward sheet (the Lancaster's is 960 pixels wide) is enlarged four times and its faint strokes darkened first, so
that the outlines close; a plan drawn at an angle is turned upright by the line through her wing tips; a side view
drawn standing on her wheels is levelled by the slope of her back; and aerial wires, which are as heavy as her
outline, are rubbed out along their own lines, found by reading the strokes at a few columns.

**The carrier planes and the big bombers** (Brian, 2026-11-18) are a battle's own too, thirteen more entries in
`EXTRA_PLANES`. Paints and first years are the art director's rulings of that day; the file's `year` is the first
year a nation flies her (none: from the start), `tech` the rules' technology she needs, `tag` the model tag the
design director gave her.

| file | what she is | year, tech | crew | she carries |
| --- | --- | --- | --- | --- |
| `Hellcat` | US Navy carrier fighter, three blues over white | 1943 | 1 | a belly tank |
| `Wildcat` | US Navy carrier fighter before the Hellcat, blue-grey over light grey, wings that fold back | 1942 | 1 | |
| `CarrierFighter` | carrier fighter for any other nation (`nation_paint`) | | 1 | |
| `Kate` | Japan's carrier torpedo bomber, pale grey | 1937 | 3 | a torpedo, or one heavy bomb for bombing from high up |
| `Avenger` | US Navy torpedo bomber, blue-grey over light grey | 1942 | 3 | a torpedo or bombs, in her bay |
| `Devastator` | US Navy torpedo bomber of Midway: a long greenhouse over her pilot and gunner (the middle place empty, as for a torpedo attack), corrugated outer wings | 1937 | 2 | a torpedo half sunk in her belly, or one 1,000 lb bomb |
| `TorpedoBomber` | carrier torpedo bomber for any other nation | | 3 | a torpedo or bombs, in her bay |
| `Val` | Japan's carrier dive bomber, fixed undercarriage | 1940 | 2 | a bomb on its crutch |
| `Dauntless` | US Navy dive bomber, holed dive flaps | 1941 | 2 | a bomb on its crutch |
| `DiveBomber` | carrier dive bomber for any other nation | | 2 | a bomb on its crutch |
| `Nell` | Japan's land-based bomber of the early war: two engines, two fins | | 7 | a torpedo or one heavy bomb, under her |
| `Betty` | Japan's land-based bomber: two engines, a glass cone in her tail | 1941 | 7 | a torpedo or bombs, in her bay |
| `Amerikabomber` | Germany's very long range heavy: four engines, two fins | `verylongrange`, tag `amerikabomber` | 8 | bombs |
| `B29` | America's very long range heavy: four engines, bare metal | `verylongrange`, tag `b29` | 11 | bombs |
| `Wellington` | Britain's twin of 1939-41 (the Mk IC): fabric over a geodetic frame, FN5 turrets in her nose and tail, a gun at each beam hatch, night colours | 1940 | 6 | bombs, in her bay |
| `Pe2` | the Soviet Union's dive bomber of 1941-42: a glazed nose, a long greenhouse over her pilot and navigator, two inline engines, twin oval fins, slatted dive brakes under her outer wings, green and black over light blue | 1941 | 3 | bombs, in her bay |

What they add to the form:

- **A torpedo is a second load.** Part `torpedo` (a slide, like `bombs`), socket `torpedo`, clip `torpedo_away` (a
  bay's doors open first). A machine that can carry either says so in her file (`loads`: `["torpedo", "bombs"]`), and
  she carries one or the other, never both: play `load.torpedo` or `load.bombs` first (each scales the other load
  away) and leave it applied. Extras painted `torpedo` are her torpedo; `BAY_TORPEDO` puts one in a bay. The Betty's
  is the Nell's Type 91, ringed as finely (`fitout.TURNED_TORPEDO`), and her far model carries it and her bombs too,
  at the game's own grain for what hangs under a machine (`fitout.PLAIN_LOADS`, `PLAIN_GRAIN`). On a torpedo sortie
  she flew with her bay's doors off and the torpedo half out of her belly (`fitout.TORPEDO_HUNG`): it hangs so on her
  near and far models, her `load.torpedo` also scales `bay.port` and `bay.starboard` away, and her `torpedo_away`
  drops it at once (no doors to open). On a bomb sortie (`load.bombs`) her doors are on and closed, and `bombs_away`
  opens them as before. Her default, with neither load clip, shows the torpedo through her closed doors: always play
  one. The new
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
  fin; the Val has the rounded, elliptical wing; the Wildcat is short and round-bellied, her main wheels drawn up
  into wells in her sides and her wings folding back along a joint. The plain three stay plain and borrow none of these.
- **The twin-engined bombers of release 2** (Brian, 2026-11-23) are each in a module of her own, taken in by
  `planes.bomber` as `planes.carrier` takes in a carrier plane (her NAME, NUMBERS and SEATS, and any of BAY, WAIST,
  TAIL_CUT, TURRET, SEAT_DROP, NOSE_CREW and NOSE_CANNON she gives). `bomber_wellington.py`: the Wellington Mk IC,
  measured from "Vickers Wellington.svg" (Wikimedia Commons, Emoscopes, CC BY-SA 3.0 and GFDL; 40.44 px to the metre,
  set by her span; the side view levelled by her thrust line and stretched 0.9% to the plan's length; measured, not
  traced). Her own key: `geodetic` (pitch, half width, slope): her skin is doped fabric over a geodetic frame, so
  `fitout.rivets` lays no rivet or joint on her fuselage or flying surfaces but the frame's two sets of members,
  crossing at 45 degrees to her length (as many round her tail as round her widest), none within a few degrees of
  the line where her coat changes. Each is a low ridge in the fabric (the art director: "faint ridges in the fabric,
  caught by raking light"), in the very paint of the skin under it: its edges lie on the skin with its shading
  (`Model.lying`), its shoulders stand a millimetre or two off it, so that its shading leans `slope` degrees there
  (half that on her black). Only the light shows it, a few per cent lighter on the side toward the light and darker
  on the other, on every colour alike; a ridge is 3 cm across, under a pixel from about 30 m, where its two sides
  cancel. On the fine model only (her plain one is plain); a machine without the key is built as before. `bomber_pe2.py`: the Pe-2 of 1941-42, measured from "Petljakov Pe-2.svg"
  (Wikimedia Commons, Kaboldy, CC BY-SA 3.0; 113.4 px to the metre, set by her span); that sheet draws the
  radial-engined Pe-2K, so her M-105s' nacelles are from the public-domain "Petlyakov Pe-2 3-view silhouette". No key
  of her own; her tally panel is drawn at 0.65 (`tallies.SCALE`), her side being slim and round; her tactical number
  is painted in white just behind her greenhouse, not on her fins (`codes.ON_SIDE`); and her tailwheel is left off,
  folded up into her in flight.
- **The Devastator's own keys** (`carrier_devastator.py`; no other machine has them, and none changes another):
  `squares` (how square her section is, station by station: a round cowling, slab sides under her greenhouse;
  `fitout.squared_hull`); `corrugated` (ridges in the skin of her outer wings and tailplane, on the fine model only,
  bent at the rings of the surface's chord, none under a star on her upper wing, and no rivet or joint under them:
  `fitout.corrugation`); `cowl_gun` (one fixed gun in the ring of her cowling, and socket `gun.nose` at its muzzle);
  `exhaust_side` (her one exhaust pipe, to starboard). An extra rod with `"one"` among its last values is laid on its
  own side only, not mirrored (her pitot, on her starboard wing). `plain_extras` (around, step, rod sides): on her
  plain model only, the bodies and rods of her extras (her torpedo, her bomb, her wheels) at that grain, (12, 0.3, 10),
  the game's own, instead of the battle's 24 round every 12 cm and rods of 22 sides. At the fine grain her torpedo and
  bomb were 3,144 of her plain model's 9,981 triangles; now 7,349, under the Kate's 11,146 and the Val's 10,942 that
  fly with her at Midway. Her fine model is as it was. (`battle_rod_sides`, `battle_body_rings` in `planes.py`; a
  machine without the key is built as before.) `rudder_line` (([(height, x), ...], ((height, x), (height, x))), metres):
  where her rudder is cut from her fin, height by height, and the two ends of the one straight axis it turns on. No
  one share of the chord follows her hinge (behind her post under her tailplane, back behind the box over its root,
  then up the fin's own hinge line), so `battle_wings` in `planes.py` stretches each row of her fin, ahead of the
  hinge's ring and behind it, to put that ring on the line; her fin's rings, the cut, the scribed hinge line, her
  rivet rows and her rudder's nose and cove follow it, and the `rudder` part's pivot and axis are the line's axis,
  with nothing of the rudder ahead of it. `ink`: her side number's colour on her first coat (black, as the squadrons
  painted it in 1942); the file's `number_ink`. Her later coats work out their own (white). `torpedo_paints`: paints
  besides `torpedo` that ride her torpedo part (her warhead and screws, in `boss`), dropped with it and hidden on the
  bombs load.
- **Later coats of paint** (`SCHEMES` in `planes.py`; the file's `schemes`). The same machine in other colours as
  the war goes on, from the art director's unit table (`carrier_paint`). Each entry has `from` (the month it comes
  in, `YYYY-MM`), `name`, `colours` (only the paints it changes, by paint name: lay them over the file's `colours`
  by `paint_names`) and, where it changes, `number_ink`. Paint her in the last one whose month has come.

  | file | to begin with | later |
  | --- | --- | --- |
  | `Hellcat` | three blues over white | overall sea blue from 1944-07 |
  | `Dauntless`, `Avenger`, `Wildcat`, `Devastator` | blue-grey over light grey | three blues over white from 1943-02, overall sea blue from 1944-07 |
  | `Zero`, `Kate`, `Val` | pale grey | dark green over grey from 1942-07 |

  The Dauntless, the Avenger and the Wildcat are one colour on top to begin with, but their skins are laid out for
  the three blues: the band down their sides and their fins are `upper2`. `airlook.py ... scheme=1944-07` shows a machine in
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
  like the rest of her (`nose` the same colour as `upper`), in `upper` above and `under` below. In a coat laid by
  where a point is (`graded`), each flap is the colour her skin is there: `under` below her skin's own line
  (`under_from`), `upper2` where the coat lays it (only with `patch`, as on her skin), `upper` elsewhere. So a later
  coat laid on by paint name colours them with her. A thin dark line (`line`) marks the gap between each pair, so
  that they are seen without a pale collar round her neck.
- **A machine's own hinges, exhausts, guns, engine face and turret.** Each is a key of her numbers; without it she
  is built as before. `elevator` and `rudder` are (from, to along the span, the share of the chord at the hinge), by
  default (0.08, 0.95, 0.62) and (0.06, 1.0, 0.60), for example `(0.06, 1.0, 0.48)`; the share moves back to the
  next ring of `CHORDS` (`CHORDS_CLOSE` with `grain`, `CHORDS_PLAIN` on the plain model). For the radial in her nose,
  `exhaust` is her pipe each side (where along her, degrees round her, how long, how far its mouth stands off her
  skin, its radius, degrees down, degrees its mouth turns out; those left off as before: -28, 0.4, 0.075, 0.05, a
  1 cm drop, 0), for example `(2.89, -33.0, 0.2, 0.04)`. `heads` is (heads in a row, rows, their ring's radius, a
  head's radius, how far the front row stands behind `lip`; by default 1, the game's ring, 0.075, -0.015), for
  example `(9, 2, 0.41, 0.08, 0.10)`, and on the fine model her face is cut open back to them. `mouth` is how far out
  that face is dark (all of it by default) and, with `heads`, open, for example 0.50. `nose_guns` is (degrees from
  her top to each trough, the barrel's lift), by default (17.0, 0.028), for example `(23.0, -0.005)`.
  `"sight": "telescope"` leaves out the reflector sight. A `dome` may add the height of its ring, below which it is
  metal in her upper paint, and a dark window each side:
  `("dome", ((-1.635, 0.0, 1.13), (0.57, 0.60, 0.495), 1.16, (-1.39, -1.86, 0.80, 1.02)))`. A `canopy` function may
  take `fine`: `def canopy(hull, fine=True)`. An extra `rod`, `ball` or `body` whose last value is `"fine"` is
  laid on the fine model only (a thin post, rail or brace the map's model does not need).
- **The Wildcat's own keys** (each for a machine whose numbers ask for it; without it she is built as before).
  `round_below`: her belly rounder than the rest of her below her middle, as stations (x, how much of the way to a
  plain oval her section is there: 0 none, 1 all of it); only her width at a height changes. `gill_proud`: how far the
  after edge of her cowl's gill plates stands out from her (0.03 by default; hers 0.0, closed and flush, as drawn).
  `pitot` may be a share of her span instead of `true`: the pitot stands that far out under her port wing (0.86 by
  default). `lamps`: (x, y out) of each wingtip lamp, standing out of the tip itself. `tailwheel_leg`: (x, z) of the
  top of her tailwheel's leg, which runs down from there to its axle (hers raked back out of a fairing).
  `side_wheels`: main wheels drawn up into wells in her sides, (x, z of the wheel's middle, the tyre's radius, the
  hub's, how much her side view squeezes the circle, as her side leans in); on the fine model each is a
  tyre and a bare hub set into a well cut into her, its dark edge seen from the side, ahead and below, and on the
  plain model a wheel laid on her skin. `fold`: the joint of a folding wing as (x, y out) from her leading edge to her
  trailing edge, a fine dark line over it on top and beneath, on the flap where it crosses the flap so that it goes
  down with it. `skin_windows`: small windows in her skin, (from, to along her, from, to in height), each side.
  `skin_rails`: the rails a sliding hood runs back on, (from x, at height, to x, at height), each side.
  Her wings wear four stars, as in 1942; `mark_months` has no way to take the second pair off from 1943-02 (an entry
  changes a mark's tag, not whether it is there), so her later coats keep four.
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
`boot`, `rivet`, `rivet_under`, `seam`, `seam_under`, `blur_tip`, three lamps). **Draw `glass`, `blur` and `blur_tip` clear**: the file
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

### France's own machines of 1940 (release 2)

Each is in a module of her own and taken in by `planes.land` (as `planes.carrier` takes in a carrier plane): her NAME,
NUMBERS, SEATS, her coat (`coat()`, her camouflage field) and any table of her own. France's other machines stay the
plain ones (the `Fighter` is drawn as an MS.406). Measured from public three-view drawings, not traced; each module
names its drawing, its licence and its scales.

| file | what she is | year | crew | she carries |
| --- | --- | --- | --- | --- |
| `D520` | the Armée de l'Air's Dewoitine D.520 fighter, from June 1940: khaki and dark earth over light blue-grey, French roundels, the tricolour on her rudder | 1940 | 1 | a 20 mm cannon through her airscrew's hub, four 7.5 mm guns in her wings |
| `LeO451` | the Liore et Olivier LeO 451 medium bomber of 1939-40: two Gnome-Rhone 14N radials, a glass nose, twin fins; the same coat and roundels, the tricolour on both rudders | 1939 | 4 | a 20 mm cannon at the back of her cabin (`turret.1`), a 7.5 mm gun in her retractable ventral turret (`turret.2`), bombs in her bay |

- **France's marking** (`marks.py`): `FRA`, the roundel (red outside, white, blue in the middle, rings of one width),
  in France's own brighter blue (`FR_BLUE`; `KILL-FRA` is as it was). `FRA-FIN`, the tricolour on her rudder, blue
  forward: a mark of place `fin` on part `rudder` with a `quad` (as a plain machine's `*FIN`) and its own `bands`
  (`{dir: "v", bands: [blue, white, red]}`), so the game needs no table to paint it.
- **The D.520's own keys** (`france_d520.py`; without them a machine is built as before). `hub_gun`: a cannon firing
  through her airscrew's hub, its barrel standing this far out of her spinner's tip (it does not turn); socket
  `gun.nose` at its muzzle. `rudder_stripes`: (tag, colours from the rudder's leading edge back), her nation's stripes
  laid where a plain machine's flag goes. Her coat is two of the three colours of 1939-40 (the game's coat has two
  upper colours; the dark blue-grey is left out), in patches over her cowling as over the rest of her, with a tight
  sprayed edge (the art director, 2026-11-23: 10 to 15 cm, not a metre of haze). Each French module steepens its own
  coat field (`_EDGE`) and sets a finer `grain` on her wings, tail and fins (and the LeO 451's fuselage), so no other
  machine changes.
- Her parts are a single-seat fighter's (`plane`, `wing.port`/`starboard`, `tail`, `prop.1`, `prop.1.blur`, `aileron.*`,
  `flap.*`, `elevator.*`, `rudder`); sockets `engine.1`, `gun.1` to `gun.4` (her wing guns), `gun.nose` (the cannon) and
  the usual others; clips a fighter's.
- **The LeO 451** (`france_leo451.py`) needs no key of her own: a glass nose (`glazed`, `glazed_sill`, `nose_frames`),
  her pilot's canopy (`hoods`, with `opening` under it), her section drawn in toward her spine (`taper`), twin fins
  (`fins`), a turret on her back and one let down under her (`dome`, `ball`). `rudder_stripes` on twin fins lays the
  stripes on both faces of each rudder, each mark on its own rudder (`rudder.port`, `rudder.starboard`; `both` false).
  Her parts are a twin-engined bomber's (`bombs`, `bay.*`, `prop.1`/`prop.2` and their blurs, `rudder.port`,
  `rudder.starboard`, `turret.1` and `turret.2` with their `.guns`); sockets `engine.1`, `engine.2`, `turret.1`,
  `turret.2`, `bombs` and the usual others; clips a bomber's (`fire.turrets`, `bombs_away`, `doors.close`, ...). She has
  `LeO451Damage`, `LeO451EngineOut` (`damage.MANY_ENGINED`) and `LeO451Codes` (the `number` style: France is among
  "everyone else" in codes.py).

### Italy's SM.79 (release 2)

| file | what she is | year | crew | she carries |
| --- | --- | --- | --- | --- |
| `SM79` | the Savoia-Marchetti SM.79 Sparviero, three Alfa Romeo 126 radials, the hump over her cabin, the ventral gondola: sand with soft green blotches over light grey, the white band round her rear fuselage, the white cross on her rudder, bare wings | 1937 | 5 | a fixed gun over her cabin, the dorsal gun, the gondola's gun and a waist gun each side (`flexgun.1` to `flexgun.5`), five bombs stowed nose up in her bay |

- In `italy_sm79.py`, taken in by `planes.land` as France's are, measured from a public three-view (the module names
  it, its licence and its scale). Her hump is a raised cabin (`hoods` with `cabin`, as the B-17's): its windscreen two
  flat panes in a V, its side windows, then the metal hump back to the dorsal gunner's step.
- What `land` takes from a module besides France's: `UPRIGHT` (fitout.UPRIGHT: bombs stowed nose up in one row along
  her keel, (how many, the bay roof's height)), `FAR_LOADS` (fitout.PLAIN_LOADS: her far model has her `bombs` part too,
  at the far grain, where her bay is, without its doors) and `finish()` (planes.FINISH: laid on her once she is built;
  hers paints the band on her skin, puts `ITA-CROSS` on her rudder, and rings her nose engine's dark face with the
  fourteen steel cylinder heads the sculptor gives her wing engines, on the fine model).
- Her coat (the art director, 23 Nov): the blotches as shipped, with a tight sprayed edge of one breadth everywhere
  (`coat()` takes the noise as a distance from the edge, soft for `_EDGE` each side, as the Me 262's lines are) and a
  finer `grain` on her wing, tail, fin and fuselage, so that an edge is not a staircase of half-metre cells. An edge
  narrower than her mesh comes out in steps of the mesh, so `_EDGE` is about as broad as her rings stand apart. Her
  nose's cowling stays plain sand.
- Parts: `plane`, `wing.*`, `tail`, `bombs`, `bay.port`, `bay.starboard`, `prop.1` to `prop.3` (2 is her nose's) and
  their blurs, `aileron.*`, `elevator.*`, `rudder`, `flexgun.1` to `flexgun.5`; sockets `engine.1` to `engine.3`,
  `turret.1` to `turret.5`, `bombs` and the usual others; clips a bomber's (`bombs_away`, `doors.close`, ...). She has
  `SM79Damage`, `SM79EngineOut` (her nose engine is feathered, no scorch: nothing lies behind it but her cabin) and
  `SM79Codes` (the `squadron` style, "278-7", anchored on her white band, as there is no roundel on her side).
- `ITA-CROSS` (`marks.py`): the white cross over her rudder with a small plain shield at its middle; Italy's marking
  is never the fasces.

### Battle damage: bullet holes and flak holes

Brian, 2026-11-18: "if a plane gets hit show the bullet holes or flak holes". Every aircraft has a damage model of
its own, `<Name>Damage` (`damage.py`; `air/<Name>Damage.json` and `.glb`), that lies on her skin. An aircraft that
has not been hit has none, so nothing changes for a game that does not use it.

- It has every part of the aircraft (same names, parents, pivots; no faces of their own) and every clip of hers
  (without her events). Put it where she is and play on it whatever she plays: it follows her, wing off and all.
- Its faces are in parts `damage.1`, `damage.2`, ...: one hit each, hanging on the part of her it struck. A hit is
  "bullets" (a raking row of five to eight holes, unevenly spaced, each a dark hole with bare metal on one stretch
  of its rim only) or "flak" (a ragged tear 30 to 55 cm across, three to six petals of torn metal bent into it, most
  of them dark and one or two catching the light, a halo of soot round it, and a scatter of splinter holes). A
  fighter has eight or nine hits, a heavy bomber fourteen; 1,400 to 2,900 triangles. Holes are larger than life
  (8 to 10 cm) so that a battle camera sees them.
- Three clips show them, with seconds standing for hits: `damage.bullets` held at k shows her first k bullet hits,
  `damage.flak` her first k flak hits (lay one over the other), `damage` the first k of all kinds. At rest all show.
- The file's `damage` lists each hit: `part`, `kind`, `on` (her part), `at`, `facing`, `holes`.
- The faces stand 3.5 to 16 mm off her skin, as her panel lines do. Six flat paints: `hole`, `torn` (bare metal in
  the light), `lip` (bare metal showing faintly through her paint), `bent` (torn metal in the dark of the hole),
  `soot`, `soot_thin`. Three are see-through, and the file says how much under `clear` (lip 0.4, soot 0.35,
  soot_thin 0.17), as an aircraft's file does for her glass. The art director's rule: a whole bright ring round a
  hole reads as a washer on olive drab, and even bright petals round a tear as a paper flower.
- In the clips: `Plane.hurt(bullets, flak, out=(2,))` (`airshow.py`); `out` names the engines that are out (below).
- Made again with `python export.py <out> damage json glb` (six seconds an aircraft).

### An engine out: a feathered airscrew and a scorched nacelle

The air battles, 2026-11-18, for Brian's "if a bomber loses and engine it goes slower and falls out of formation".
Each aircraft with two airscrews or more (`damage.MANY_ENGINED`) has a second layer of the same kind,
`<Name>EngineOut` (`damage.overlay(name, what="engines")`; `air/<Name>EngineOut.json` and `.glb`): lay it on her and
play on it whatever she plays.

- `feather.N`: engine N's airscrew with its blades turned edge on to the wind, and still (her own blades, each turned
  about its own length until it is thinnest seen from dead ahead). On the aircraft herself scale `prop.N` and
  `prop.N.blur` away. A windmilling airscrew needs no model: her own blades, without the disc, turning slowly.
- `scorch.N`: soot laid back over nacelle N and the wing behind it, in the see-through paints `soot` and `soot_thin`
  (`clear`), so that nothing has to change the aircraft's own soot at run time. Her upper side only. Blackest over the
  nacelle, then in a fan of streaks along the air, darkest on the engine's line, the dark ones to her trailing edge
  (Brian, 2026-11-20: "a lot more dark smoke strains from that engine and directly behind it on the tail"; the art
  director's G1: streaky, never a blotch). It stops at an aileron's hinge.
- `tailsoot.N`, for the two inboard engines of a four-engined machine (engines 2 and 3 on the B-17) and for both of a
  twin's: their smoke streams straight back over the tailplane on their side, so soot lies along its leading edge
  (over the de-icer boot), two dark streaks straight behind the engine and lighter ones on its top, and a lighter veil
  low on the face of her fin toward the smoke (on twin fins, the inner face of the fin on that side), as high up it as
  her fin is tall against the B-17's. All of it is sized to her span against the B-17's. It lies a little prouder than
  her livery's marks (1.7 cm against 1.2 to 1.45), so that it darkens her letters and serial where it reaches them.
- Soot that lies on a surface that swings is a part of its own riding it, cut at the hinge: `tailsoot.N.elevator.port`
  or `.starboard`, `tailsoot.N.rudder` (`.rudder.port` or `.rudder.starboard` on twin fins). The file's `engines`
  lists them under `tailsoot_riders`.
- At rest every part shows. Clips `feather.N` and `scorch.N` touch one part each, and `tailsoot.N` touches
  `tailsoot.N` and its riders: held at 0 hidden, at 1 shown. Hold them always, at 0 for a sound engine; or scale the
  parts yourself (`tailsoot.N` together with its riders). The file's `engines` lists them. G1 has the soot build over
  20 to 40 s of smoke: fade the `soot` and `soot_thin` materials' opacity up from nothing (each part's faces are its
  own), and leave it for the rest of the battle, also after the fire is out.
- Why not in `<Name>Damage`: poses laid over each other multiply their scales (point 6 above), so a part must be
  touched by one clip only. (`damage.bullets` and `damage.flak` are safe for the same reason.)
- Made again with `python export.py <out> engines json glb`. About 3,200 (the Bomber) to 7,900 (the B-17) triangles.

### A bomber's own livery: her group's marks, serial, letters, code and nose art

Brian, 2026-11-20: "have some bomber nose art on the b-17s"; "with the bomber group markings, windows, gun looks,
ect"; "make 100 bomber names and nose art so its not always the same ones each mission" ... "lets actually make it
200". The air thread's brief is `/mnt/project-files/air-battles/nose-art/brief.md`; its list of 200 names is
`names.json` beside it (copied here as `noseart_names.json`).

`<Name>Livery` (`livery.overlay(name)`; `air/B17Livery.json`, `.glb`, `.fbx`) is a layer of the same kind as her
damage: every part of hers (same names, pivots and clips) and parts of its own, each one thing a figure may wear,
all hidden at rest (clip `none`). Show a figure's own at full size:

- `group.T` / `group.S` and `group.T.<L>`: the 1st Division's white triangle (or the 3rd's square) high on her fin,
  and its black letter; `serial.<k>.<d>`: digit d in place k (1 to 6) of her yellow serial under it (42-31280 is
  painted 231280: `pick` draws only serials that fill six places); `letter.fin.<L>`, `letter.side.<L>`: her letter
  in yellow on her fin and in light grey on her side; `code.<side>.<k>.<L>`: her squadron's two letters.
- `art.port`, `art.starboard`: her nose art, a picture 2.2 by 1.35 m wrapped on her skin on each side of her nose,
  below the navigator's windows and ahead of her inboard engines. It is a grid walked out over her skin from its
  middle, so it bends round her without stretching, and it carries UVs (TEXCOORD_0 in the .glb, `uv` in the .json:
  u 0 to 1 the way it is read, v 0 to 1 upward). Its material, `B17Livery:art`, takes the figure's own decal as its
  picture, clear where the picture is clear; in the files it holds a clear 4 by 4 placeholder.
- The decals: `air/noseart/art-NNN.png` (1408 by 864, RGBA), one for each number of the list that is painted, and
  `done.txt` / `done.json`, the numbers done. A number never changes hands: the scene draws only from the done list.
- `livery.pick(seed, group, count)` says what each figure of a group wears, as the brief's section 3 has it: one
  shuffle of the painted nose art for the group from the battle's seed and the group's id, given out in slot order
  (pass one `taken` set for all the groups of a battle, so that two groups share no name while unused ones remain);
  about one in five a new machine with a bare nose, a few with it on both sides; letters in slot order within a
  squadron of six; serials from her type's real blocks, never the four famous ones. In late 1943 a formation mixes
  the B-17F and the B-17G, about six Fs to four Gs: an F's `hide` is her own chin turret (`turret.1`,
  `turret.1.guns`), for the scene to scale away. Her `star` is the tag of her national marks by month: red-edged
  bars in July and August 1943, blue-edged from September, about one in eight still red-edged until the spring of
  1944 (the game's marks: `USA-BAR`, `USA-BAR-RED`).
- In the pictures: `Plane.wear(parts, art=n, hide=...)` and `Plane.insignia(tag)` (`airshow.py`).

`noseart.py` paints the decals: each painting is drawn by script from flat shapes (a woman of the 1940s in any of
a dozen outfits and poses, always fully dressed; beasts, machines, weather; bombs, stars, wings, bolts) on a grid,
then rastered with a brush's uneven dark edge, three or four flat colours, the name in a sign painter's script,
block or stencil hand from the list, faded house paint and chips that show her olive drab. Nothing is copied from
any real aircraft's art; no name is a real aircraft's, person's, company's or any story's character. The art
director's rules (`art-director/battles/nose-art-200-ruling.md`): one bold silhouette, no words in the picture,
nothing burning or falling, lettering that reads by lightness under the battle grade (red, orange, pale blue and
light green always with a white or yellow edge), capitals 20 to 30 cm on her skin.

```
python -c "import noseart; noseart.write('<out>/air/noseart')"   # every painting drawn: art-NNN.png, done.txt, done.json
python export.py <out> B17Livery json glb fbx                     # the layer
```

### Firing arcs, and a small nation's flag

- A turret's limits are in its file (`turret.N` trains, `turret.N.guns` lifts). By default a turret in her nose or
  tail trains 95 degrees each way and its guns lift from -45 to 60; one under her belly -85 to 10; any other -10
  to 80. A ruling for one aircraft goes in `planes.TURRET_LIMITS` and replaces the default. The Lancaster's are the
  historian's (2026-11-18): nose 95 each way, -45 to 60; mid-upper all round, 0 to 70; tail 95 each way, -60 to 60.
  The files hold no cut-out for her own fins and wings: not firing through them is the game's.
- A hand-held gun (`flexgun.N`, `kind: free`) has no limits in its file. `gunarcs.py <air folder> <out.json>
  <out.md>` reads every one off the exported files and gives each a placeholder arc by where it is and which way it
  points at rest (waist, nose cheek, nose, tail, rear cockpit, under the fuselage): `air/gun-arcs.json`. They are the
  modeller's estimates, not sourced figures.
- The five plain machines a small nation flies (Fighter, Bomber, CarrierFighter, TorpedoBomber, DiveBomber) carry a
  mark of place `fin` and tag `*FIN` on the part `rudder`, both sides (`planes.rudder_flag`): the art director, for
  Brian's "have the country logo show on the tanks and planes just like the major nations". Its `quad` is the flag's
  four corners on the rudder's side in x and z (leading-bottom, trailing-bottom, trailing-top, leading-top); `at` and
  `radius` are its middle and half its height. The game paints there the stripes the unit table gives the country
  (`fin`: dir h, bands top to bottom; dir v, from the leading edge back), as it paints the country's `insignia` where
  a mark's tag is `*`. Their second upper colour (`upper2`, the unit table's `paint2`) covers 36 to 44 percent of the
  upper surfaces in soft-edged blotches; the under surface is its own paint.
- To see one as a country flies her: `python airlook.py Fighter <out> quarter 1280x720x16
  owner=<unit-table.json>#ARG` (her coat, her marking, her rudder flag, from the art director's unit table).
  `high` shows the marks on her wings. Neighbouring layers of one colour in a country's `insignia` are laid at one
  height (Sweden's three crowns are 24 pieces; each layer stands a few millimetres prouder than the last).

## Making them again, and the pictures

```
pip install bpy==5.3.0 numpy pillow            # Blender as a Python module (Python 3.13); needs ffmpeg for clips
python export.py <out> all                     # every model: .json, .glb, .fbx, and index-sea-air.json
python look.py Freighter <out> close,aft       # quick stills while building (small craft: small,smallaft)
python show.py Battleship_UK turn <out>        # ship clips: turn, broadside, sink, capsize
python airshow.py shot_down <out>              # aircraft clips: flypast (plane=Me262 for another machine), shot_down, ways_down, b17_head_on, b17_escort, b17_flak
python airshow.py b17_flak <out> frames=none   # no pictures: only the clip's sound log, <out>/air-b17_flak-sound.json (seconds)
python soundtrack.py <log.json> <sound pack's wav folder> <mix.json> <out.wav> [<video.mp4> <out.mp4>]   # mix a clip's sound, and lay it under the picture
python airlook.py Spitfire <out> front,hood     # aircraft close up (front, hood, glass, rear, over, under, side, profile, quarter, high, tail, tailside, waist, belly, ball, record, rudder, nose, ace; clip=bombs_away@1.6; clip=load.bombs@0; record=JSON; hurt=14,6,2: bullet hits, flak hits, engines out; free with eye=x,y,z look=x,y,z: a close-up of anything, in her own metres)
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

A magazine's blast (`blast`, big) throws smoke that is coloured by how high it is thrown: fire light only reaches a
cloud's foot, so it is warm in its lower third while the fire burns, grey-brown in the middle and charcoal at the
top, and the warm foot cools in a second. Tan to its top it read as a dust cloud beside the black fire smoke. The
smoke born at the deck comes in over a third to half a second, so it does not cover the fire from the first frame.
Low at the deck is a small yellow heart (thrown at about 1.3, 0.75, 0, which the tone curve gives as 225,187,104:
blue stays low, so it is yellow and never peach), with dice and names of its own (`name="heart"`); the tongues'
tips are a dark red-brown. The blast's old dice are thrown in the order they always were, so the frames after the
blast has cleared match the ones rendered before the change (frame 62 of the sinking: mean difference 0.5 of 255).

A tongue of flame's shape (`_flame_mat`). As a plain stretched ball a tongue was a lens-shaped oval with a pale line
down its middle, and a ball of them read as orange leaves, a flower, a tulip. A tongue now keeps only the middle of
its ball as it nears the tip, so it is fat near the root and drawn out thin; broad notches and its own streaks tear
its edge; its tip is soft, because the ball's face turns slowly there; and its yellower root follows how far along
it is and its streaks, never how it faces the camera (that drew the line). Tongues are long and thin (a width of a
fifth to a third of the length) and are rooted through the middle of a ball of flame, so its heart is not a hole.
A heart is a few short yellow tongues, never a round glowing ball; the throws the old balls made are still made, so
the dice after them are as they were. An engine fire's tongues are set back into the place that burns, or their
blunt root ends show as a row of round heads. `flametest`-style check: three balls of flame against the sky at two
moments, looked at twice size, before any clip.

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
stepped clear of another's smoke), find the frames it is in by projecting it into the camera. Several renders may share
one folder with frame lists that do not overlap; after the join, look at every frame's sea (one worker once read the
wake's picture while another was writing it and gave a frame a white sea; the picture is now moved into place whole).

Two lessons of the close fly-past, kept in `airshow.py`. The camera goes nearly all the way round the machine, so
somewhere it looks up-sun, and there the sun's glare off the wings washed the camouflage out to cream: the sun now
stands so that the glare falls in the gap the camera never visits (`SUN`), and a painted skin shines a little less
(`SKIN`). A machine that only comes late in the war carries its `year` in its file, and its sheet label says
"from 1944"; one that needs a technology and has no year says "by research".

Battle scenes in the air (`b17_head_on`, `b17_escort`, `b17_flak` in `airshow.py`). Each is a few seconds of a
fight round a box of B-17s, built from the same parts: `bombers()` (the box, each with her drone and her gunners),
`Flight` (a fighter's path through keyed points, banking into its turns), `guns()`, `gunners()`, `strikes()`, `trail()`,
`fireball()`, `flak()`. Three rules were learnt making them:

- **No machine closes faster than it could fly.** The box flies at 95 m/s. A fighter creeping up astern closes at 40
  to 50 m/s, an escort coming down at about 100, a head-on pass at about 245. Keys set by eye came out over 900 km/h.
- **Tracers** (Brian, 2026-11-18: "1 out of every 5 bullets with yellow"). `TRACER` and `guns()`: every fifth round
  of each gun is a yellow streak, the four between are not drawn, and the guns' belts are out of step. A streak is
  16 m long (a film shutter's worth), a solid core with a faint glow (`_tracer_mat`), and never thinner than a few
  pixels (`puff(least=)`; beyond 300 m it is let dwindle, or a sky of far tracers is confetti). Its yellow is
  measured: radiance about 1.05, 0.93, 0 shows 213, 199, 104; the flames' 1.4, 0.85, 0 is amber, and anything
  brighter is cream. The camera must be set for a frame before `A.at(t)`, because the least thickness is worked out
  from it. Every round gives a short yellow tongue at the muzzle (a round bright ball there was a cream bead).
- **Flak** is one dense black ball of big puffs one on another, lumps round it and two or three stubby arms, left
  standing in the air while the box flies on. Three dozen small puffs thrown wide were a bunch of dots.

A ring round every puff (`_face` in `stage.py`). The layer weight's "facing" forgets the sign of the smoothed normal,
so at a puff's outline, where that normal tips away from the camera, it rose again: a faint dark ring round every puff
of smoke (black flak against the sky stood as frogspawn) and a crisp brushed edge on every tongue of flame. `_face()`
works it out from where the sight line passes the ball's middle, in the ball's own space; for a tongue, from the
normal with its sign kept. Use it in any new effect's stuff. (In Blender 5.3 the vector transform node's first socket
is not its vector: sockets are taken by name. Linked to the wrong one it fails without a word.)

A clip's sound (`Air.cue`, `Air.hear`, `Air.sound_file`; `soundtrack.py`). A scene says what is heard as it says what
is seen: the name of a recording in the project's approved sound pack, when, and what it rides on. `airshow.py` writes
that beside the frames as a log of names and numbers (where the camera was and where each sound was on every frame).
`soundtrack.py` mixes it by the sound director's rules (`mix.json`): the microphone stands between the camera and what
it looks at; every sound falls away and dulls with distance by its family's law; a blast farther than 150 m is heard
after it is seen; engine loops bend with speed toward or away; a fighter going by within 300 m plays its own fly-past
recording, set so its loudest moment falls when it is nearest; guns are their burst recording laid end to end while
they fire, six at once; a machine going down dips the rest; a limiter keeps the whole under -1 dB. This repository
holds no sound: the mixer reads the pack where it lies, and a name the pack does not hold is silent, never made up.

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
  greens and the Me 262's clouds. Short of that, `grain` in a machine's numbers halves the wing's mesh and a
  `graded` coat of straight lines then holds an edge a hand's width and a half broad (the Me 262's splinter).
- One plain mesh (`far`) per model; no third level of detail.
- The men and tanks in the landing craft are stand-ins; the land team's soldiers and tanks should stand there.
- No beach in the pictures' stage, so there is no clip of a landing yet.
