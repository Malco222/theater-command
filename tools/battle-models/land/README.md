# Land vehicles for the real-time battles

Every tank, gun, half-track and truck the battles show is built here from numbers: no outside meshes, no art from any
other game. Each is modelled to the real type's published measurements and layout, to the bar Brian set with the
battleship clip: smooth (no faceting) and detailed. Numbers decide a battle; these models only act it out (ruling 217).

It all runs in Blender as a Python module (`bpy`), with no Blender window:

    python landlook.py  PanzerIV <folder> front,rear,gear 24 1280x720      # quick stills while building
    python landshow.py  PanzerIV <out> sheet                                # six stills on the battle's ground, and a sheet
    python landshow.py  PanzerIV <out> action                               # she drives up, halts, trains her gun, fires
    python landshow.py  PanzerIV <out> brewup                               # she is struck and her ammunition goes
    python landshow.py  PanzerIV <out> faces                                # each gun and coat she wore through the war
    python export_land.py <out> PanzerIV json glb fbx                       # the game's files

## The files

| file | what it is |
| --- | --- |
| `motorpool.py` | plates, castings, lathe work, bolts and welds: every plate edge cut back, so nothing is a knife edge |
| `gear.py` | road wheels, sprockets, idlers, return rollers, and a track laid link by link round them |
| `fittings.py` | barrels, hatches, cupolas, tools, bins, silencers, grilles, spare links |
| `works.py` | the kit together (`Works`), the rig and clips every tank has, the pictures' weathering, the mesh as the game reads it |
| `tanks_ger.py` | Panzer IV (two guns, two coats, the skirts of May 1943), Panther, Tiger; the Panzer IV's running gear and the skirts along a hull (`hull_skirts`) as functions |
| `tanks_sov.py`, `tanks_usa.py`, `tanks_uk.py`, `tanks_fra.py`, `tanks_ita.py`, `tanks_jap.py`, `tanks_pol.py` | T-34 and T-34-85, Sherman (and the M4's tub, gear, deck and tail as functions), Matilda, Char B1, M13, Type 97, 7TP |
| `tanks_any.py` | the light and the medium tank of every nation without one of its own; the medium tank's hull as a function (`medium_hull`), and the assault gun built on it |
| `tank_destroyers.py` | StuG III (two coats, the box shield round her gun and the cast one of late 1943, skirts), SU-85, M10 |
| `guns.py`, `guns_at.py` | the gun kit (`GunWorks`: wheels, saddle, cradle, piece, shield, trails, the towed gun's clips), five field guns, two anti-tank guns |
| `sp_guns.py` | self-propelled artillery (`SpWorks`: the gun kit standing in a hull): M7, Sexton, Hummel, SU-76, and the self-propelled gun of every army without one (a 75 mm field gun in an open box on `medium_hull`) |
| `carriers.py` | wheels on tyres, steering, burst fire (`CarrierWorks`); the lorry, the Sd.Kfz. 251, the M3 half-track |
| `armoured_cars.py` | Sd.Kfz. 222, M8, and every other army's armoured car |
| `flak.py` | the light flak mounts, and the three wagons that carry them |
| `camo.py` | camouflage: the coat's second colour laid in soft-edged blotches over about a third of a vehicle (`Works.camouflage`, the last call of a builder) |
| `vehicles.py` | every vehicle by name, whose it is, and the faces it showed through the war |
| `ground.py` | the stage (`Battle`: the field battle's own ground and trees; `Field`: a plain meadow) and the effects (`Air`) |
| `landmarks.py` | each nation's plain period marking, laid on the skin; a crew's name or rings too, for a proof picture (`landlook.py <Name> <out> side 14 960x540 name=WORD` or `rings=3`); and the few soft marks where rain has run down her sides (`rain_marks`: pictures only) |
| `brushhand.py` | the crew's brush hand: Latin capitals (with their accents) and Russian ones as strokes, laid out unevenly as a hand does, the Soviet slogans in Russian letters, and Japanese characters one under another (`column`); written out for the games as `brush-alphabet.json` |
| `kanji_strokes.py`, `brush-kanji.json` | the Japanese characters' strokes, read from KanjiVG's open stroke-order record (CC BY-SA 3.0, Ulrich Apel, https://kanjivg.tagaini.net; the table is under the same licence and says so) |
| `names_fit.py` | which of the historian's names each tank can wear, on how many lines and how tall: writes `names-fit.json` for the games |
| `landshow.py`, `landlook.py` | sheets, clips, quick stills |
| `export_land.py` | `<Name>.json`, `.glb`, `.fbx` and `index-land.json` |
| `rigid.py`, `kit.py`, `marks.py`, `stage.py` | unchanged copies from the warships' and aircraft's folder (`tools/battle-models/` on their branch) |
| `battle_set.py`, `soldier_kit.py` | the soldier thread's preview stage and its kit, copied unchanged but for one import line |

## One of each kind for every army without its own

Brian's ask of 18 November: a generic version of every family for the smaller nations. In `vehicles.py` these are
the vehicles whose nation is `any`, each our own design and no one maker's:

| Family | The generic |
|---|---|
| tanks | `LightTank`, `MediumTank` |
| assault guns and tank destroyers | `AssaultGun` (the medium tank's hull, a fixed box, a 75 mm gun in a cast collar) |
| self-propelled guns | `SelfPropelledGun` (the medium tank's hull, an open-topped box, the 75 mm field gun) |
| armoured cars | `ArmouredCar` |
| carriers | `Truck` (the lorry carries their infantry: no generic half-track) |
| anti-aircraft wagons | `FlakTruck` |
| towed guns | `Gun75` (the field gun: France's, and every nation's without a gun of its own), `AntiTankGun` |

A generic carries no name place (b477 is for the named types) and its marking is the army's own (`tag`). The two
built on `medium_hull` share her running gear, so one track and one set of wheels serve three vehicles.

**Each small nation's own mark** (Brian, 18 November: "have the country logo show on the tanks and planes just like
the major nations"). The marks' tag is `_`: the game paints the owner's own mark there. `LightTank` carries one on
each turret side behind the pistol port (radius 0.13; her hull side is left for her number). `MediumTank` carries
four, as the Sherman does: hull sides 0.2, turret sides 0.15. `AssaultGun` and `SelfPropelledGun`: hull sides 0.2.
`Truck`: her doors. Every place is checked for a flat plate under the whole disc and a clear view from the side and
from the game's camera.

**Camouflage that fits their ground** (Brian, the same day: "make the camoflage reflect their terrian"). All nine
generics are camouflaged (`camo.py`): the second coat colour lies in uneven soft-edged blotches over about a third
of the coat, running on over the joints between hull, turret and mudguards, and the file carries it point by point
as `blend`. The game fills `upper` with a nation's `paint` and `upper2` with its `paint2` (unit-table.json): sand
with brown for a desert army, green with black-green for a jungle one. **A nation with one colour must fill
`upper2` with `upper`** (the lorry serves great powers too), or its vehicles show blotches in a darker shade of
their coat; the file says so under `camouflage.how`. Wheels, tracks, canvas, the barrel, the inside of an open body
and the hull's wall behind the wheels stay in the first colour. `Gun75` is France's own gun as well: built for
France her second colour is her first, and she stays one coat. To see one in a nation's coat and mark:
`python landshow.py LightTank <out> sheet nation=<unit-table.json>:SWE dust=0.3` reads that country's `paint`,
`paint2` and `insignia` from the game's own table (Sweden's green and brown, her three crowns); `coat=A08A5E,7A6446
mark=1A1A1A,F2EEE2,1E7A3C` gives two colours and a mark's rings by hand; `pool` takes the same for a whole line-up.

## The model file (`<Name>.json`)

The warships' format (see `rigid.py`): a fine mesh `near` and a plain one `far` (`points`, `facings`, `paints`,
`triangles`, and point by point `blend`, `soot`, `wear`, `part`), `parts`, `sockets`, `clips`. Modelling axes are x
forward, y LEFT, z up from the ground; the file is written with y to the right. Every turn is in degrees.

**Parts.** `vehicle` (the root: the game moves this over the ground) > `hull` (rocks and nods on its springs) >
`turret` (+ is to the right, clockwise from above) > `gun` (+ is muzzle up) > `barrel` (slides back in recoil).
Hatches are `hatch_commander`, `hatch_gunner`, `hatch_loader`, `hatch_driver`, `hatch_radio`; doors `door_l`, `door_r`.
The running gear hangs from the root so it stays on the ground while the hull rocks: `wheel_l1`.., `sprocket_l`,
`idler_l`, `roller_l1`.. (and `_r`), each spinning about its axle with + rolling forward; `track_l`, `track_r`, with a
part for every link (`track_l.001`..).

**`running`.** What the game needs to roll her: every wheel with its radius, each track with its links and pitch.
Turn each wheel by (distance run on its side) / radius. Play `tracks.left` and `tracks.right` at
(that side's speed) / `stride` x `long`: one loop of the clip moves every link on to where the next one stood, so the
track runs for ever and nothing is seen to jump. Run them at different speeds, or one backward, and she steers.

**`controls`.** What the game lays itself: the turret (with its rate in degrees a second), the gun, the hatches.

**`swaps`.** One model per vehicle for the whole war. A gun swap names the parts of each gun: show the chosen gun's
parts, scale the other's to nothing, and use its sockets in place of the default ones. A paint swap gives the three
colours to put in place of `upper`, `upper2` and `shade`. Another nation's paint is the same three colours changed.
A swap of fittings (the StuG III's `mantlet`, the `skirts`) names its parts the same way; `lose` names the skirt
plates of which each vehicle hides none, one or two. Where the faces changed with the months the swaps carry
`by_month`: one row per run of months (`from` and `to` both counted in) naming the choice of each swap, so the game
picks a true face for the campaign's month and never one that was not seen (the cast shield in dark grey).

**Sockets.** `muzzle`, `coax`, `hull_mg`, `exhaust.N`, `engine`, `fire.N` (where flame comes out), `ring` (the turret
ring: where the blast comes from), `hit.front/left/right/rear/turret`, `dust.left/right`, `commander`, `case`, `aerial`.

**Where the men go.** A crew socket is a point and a facing. A seated man's socket (`driver`, `rider.cab`, `rider.N`)
is the top of his seat under the middle of him: a bench is about 0.44 over the floor his boots are on, a driver's
seat 0.40; a gunner seated on his gun's mount (the flak wagons' `crew.gunner`) is the top of his seat too, and its
note gives the seat's height over the platform. Every other socket is the floor at his feet (a kneeling man's, the
M7's layer, the floor under his hips), with one exception: a tank
commander's `commander` is 0.25 m under his hatch's rim, and the crew pack's `commander_hatch` clip puts his feet
0.92 m under the socket. The three armoured cars' `commander` is the same, whether he stands under a lid (the
ArmouredCar) or in an open turret (the SdKfz 222, whose `open_up` throws both her screens open, and the M8): the
floor seen in a low turret is the hull's roof, the hull is solid, and only what is over that floor shows of him.
The ArmouredCar's lid stands on a coaming 3 cm high whose rim is the 0.25 point's measure: the roof inside it is
lower than the rim, so a man leaning in the hatch has his chest over the roof.
The leFH 18's layer stands inside her trail legs, left of the breech's path (between her left leg and her tall wheel
there is no room for a man), and her laying gear is on a narrow case beside the breech where his hands reach it.
A seat is made for a man in his belt kit: the crew pack's seated man is 0.29 deep behind his seat's point at the
hips (0.21 at the shoulder blades), 0.31 wide to the right and 0.22 to the left there, his knees 0.25 to each side,
his boots 0.68 ahead. So a seat's back is a pad at the shoulder blades over an open gap (the light flak gun, the
M16), and a seat in a turret stands where that whole man clears the walls (the M10's behind the breech ring, the
M8's well back in a turret of thin plate). The M8's pintle gun is stowed muzzle up, clear of both men's shoulders.
A man who serves a gun that turns is on the part that turns (the M3's `gunner` is on
`mg_mount`). Where his hands hold something there is a socket for each, named after his own: `grip.gunner.l` and
`grip.gunner.r` for `crew.gunner`, `grip.layer.l` and `grip.layer.r` for a towed gun's `crew.layer`. Each is the
middle of a handle; on a handwheel it is on the wheel's own part, so it goes round with the wheel (the left hand
elevates, the right traverses). A socket's note says anything else a man needs: which way a loader faces, how high
a breech is, that a place wants a pose and not a clip. Tanks carry no driver's socket: he is shut in, and what shows under an opened lid is a dark panel, not a
hole. The lorry's cab is shut too, its glass dark. The crew's clips: `../soldier/crew/` in the project's files.

**`mark_months`.** Where a nation's marking changed with the months the file carries the table (Britain and the
Commonwealth: none to 1941-10, the white-red-white recognition flash from 1941-11 to 1943-06, the white star from
1943-07) and the game paints the one that fits the campaign's month at each of the file's `marks`. A mark's `shown`
only says which of them the review pictures draw.

**`name_place`: the crew's own mark** (Brian's pick b477: a tank that has come through three battles wears a name
her crew painted). Each army as its crews really did it (`/mnt/project-files/historian/tank-names-b477.md`, with
twenty invented names to each army beside it): a name on the hull's side for American, British, French, Italian and
Polish tanks (the Matilda's on her skirting plates, the Char B1's near the front; Italy's and Poland's small and on
the left side only), a slogan in Russian letters on the turret's side for Soviet tanks (on the casemate of the SU-85
and SU-76), and for Germany no name but thin rings round the barrel (Panzer IV on either gun, Tiger, Panther, StuG
III; not the Hummel). The M10, M7, Sexton and M8 have a place too. The Type 97 has a place on her turret's side for
Japanese characters written downward, marked `script: japanese`: never Latin letters on her. Only the ten of the
historian's names whose characters are drawn (`japanese.names` in `brush-alphabet.json`: one or two characters each,
the old forms) are painted; for any other name the game paints nothing. Every other vehicle has none and the file
has no `name_place`.

kind `name`: a place for each side that carries one (`at`, the plate's `facing`, the writing's `along` and `up`, the
`part` it rides, the room `long`, the `lines` it has room for), the letters `high` tall (`high_two` on two lines),
`least` (9 cm: never smaller), `writing` (across or down). kind `rings`: the barrel's `part`, the first ring's middle
`at`, `toward` the breech, `step`, `wide`, `most`, and the barrel's `radius` under each ring. `ink`: for a name
off-white, or near-black on dark yellow (and for each coat she can wear); rings off-white on every coat. The place is
found on the mesh when she is built (`Works.name_place`, `Works.kill_rings`): a flat run of her own painted plate
clear of her national marking, with no hand rail, hinge or box across it or hard against its ends.

How a name goes in (the art director's rule, `brushhand.fit`, and `fit` in `brush-alphabet.json`): four to ten
letters to a line. One line at `high` if it fits the place's `long`; else two lines where the place has `lines` 2
and the name two words; else smaller on one line, never under `least`; else that name is not for this tank.
`names_fit.py` works it out for every tank and every one of the historian's names and writes
`/mnt/project-files/battle-models/land/names-fit.json` (which names each tank can wear, their lines and letter
height): choose from there. The word is painted in the brush hand of `brush-alphabet.json`, never a printed face
(`/mnt/project-files/art-director/battles/b477-battle-details-look.md`): the baseline steps about a tenth of a
letter, a letter or two run wide, no two strokes weigh the same, and a blob stays where the brush lifts. That file
has every letter the historian's names use (French, Polish, Italian and German marks are one more dab of the brush)
and his twenty Soviet lines in Russian letters (`slogans_russian`: his Latin spelling drops the soft sign, so read
them from there).

The Japanese hand (`japanese` in `brush-alphabet.json`, `brushhand.column`): each character's strokes in the order
they are written, with how the brush leaves each (`stop`: pressed down at both ends; `sweep`: drawn off to a point;
`dab`: a dot), painted with a finer brush the more strokes the fullest character has, each character a little off the
line, leaning and unlike in size. To add a name: fetch its characters' files from KanjiVG, run `kanji_strokes.py` on
the folder, add the name to `brushhand.JAPANESE`, and check `japanese-hand-check.png`-style against a printed face.

Where two strokes cross, or a blob sits on a stroke, the pieces must never lie in one plane: the overlap renders as
a dark spot. The pictures lay each piece a hair above the last (`landmarks.crew_mark`); a game paints the strokes
into one decal or does the same. Paint over paint is the same off-white, never darker.

**Rain marks** (pictures only: the games do their own dirt). `landmarks.rain_marks` looks at her sides as she stands
and lays a few soft dark strips where rain would run: from under a top edge open to the sky, or under a small thing
that stands off the plate (a handle, a hatch's rim), never under an overhang; unlike in length and spacing, up to
ten on a hull's side and four on a turret's, clear of her marking and her name. The art director's rule: six to ten
on a side, not a hundred (fine even streaks all over a plate read as planks; none at all reads as plastic). A mark
runs round a rivet's head or a weld (anything standing under 2 cm off the plate); a tall plate's top is likelier to
carry one than a scrap's; only marks 20 cm long or more count toward a side's number; a short hull side that got
fewer than six takes more, closer together (a half-track's body); and on a grey coat they are half as dark again
(`rain_strength`: the same mark read half as strongly on the grey Sd.Kfz. 251 as on the green Sexton).

**Small fittings with rules of their own** (`fittings.py`). `lamp_guard`: a brush guard is two hoops of 2.5 cm bar
crossing over the lamp in her own paint, the lamp's stem carried down to the plate (thin dark rods splayed from a
lamp read as a spider). `bucket`: a pail is dull galvanised grey with a darker inside and its wire handle hanging
(near black, seen from above, it reads as a top hat). `belt_box`: a machine gun's belt box in dark field grey with a
lid, a catch and a handle. A paint only some vehicles need is added by `own_paint` the first time it is asked for,
so the others' files do not change. What lies on an open body's floor must lie where the battle camera sees it:
the camera looks down at 30 degrees, so a wall hides the floor behind it for about 1.7 times its height (the
Sexton's cases lie forward of x = -0.2; the Sd.Kfz. 251's boxes are piled in her left after corner).

**Clips.** `idle`, `start`, `drive`, `move_off`, `stop`, `fire` (the barrel's recoil), `fire.rock` (the hull's answer,
made for a gun trained ahead: turn it about the turret's bearing), `mg_burst`, `hull_mg_burst`, `hit`, `open_up`,
`knocked_out`, `burn`, `brew_up` (the turret is thrown off its ring and lies beside her), `tracks.left`, `tracks.right`.
A clip's events carry the effect, the socket and the sound cue's name only (ruling 144).

**`wear` and `soot`.** `wear` (0 to 100) is how far a point goes toward the model's `rust` colour (no vehicle
raises it now). `soot` (toward the dark `soot` colour) is laid round each exhaust outlet, and lightly along the plate edges that boots
and hands reach: wear on an edge goes darker, never whiter (a pale line along every edge reads as chalk).

**Zimmerit.** The Tiger and the Panzer IV carry the ribbed paste of 1943-09 to 1944-09 as a swap, `zimmerit` (off by
default): real ribs, since the battle draws no textures. `Fittings.zimmerit` lays it as the trowel did (the art
director's round of 21 Nov): narrow panels about a hand wide (15 cm or under), a dark seam 8 mm wide between each and
its neighbour, level ridges about 4 cm apart and 5.5 mm proud, and no two panels' rows quite in step, of quite the
same pitch or quite as deep. A ridge is a round bead: its two faces are shaded as one over the crest and cut apart at
the furrow (each ridge has points of its own there), and the furrows are a fifth darker point by point (in `soot`, so
COLOR_0 carries the ribs in any light) so no hard light and dark bands show. They are parts of their own
(`zimmerit_hull`, `zimmerit_turret`; on the Panzer IV `zimmerit_shield` and a door each side, on her turret's shield):
a game shows them for the choice `on` and scales them to nothing otherwise, and `swaps.by_month` gives the shares by
month (ours, low on the numbers). Near mesh only: about 10,800 triangles on the Tiger, 8,100 on the Panzer IV (a scene
short of triangles can leave the parts off at its second level). The Tiger's wheels are interleaved as hers were: the
first axle's wheels outermost, the next axle's close behind.

**The two paints the ground scene gave** (the art director, 21 Nov, so a file matches the Normandy scene's own
colours): American olive drab 585139 with 46402D and shade 363123, German dark yellow 8E7E58 with 726546 and shade
584E36 (`motorpool.TONES` holds each coat's two darker tones where she gave them; any other coat's are worked out).
Ground vehicles only: the planes' swatches are their own.

**Budget.** About 50,000 triangles near and 5,000 far for a medium tank (the two with the paste run to 65,000 and
63,000 with it shown).

## How a vehicle is made to look made

* Every plate edge is cut back and the mesh carries weighted normals, so a flat stays flat and its edge catches the
  light as rolled steel does. Round things are turned with enough sides for the closest camera.
* The pictures' weathering (`works.land_look`) follows the art director's ruling: tracks are steel (dark, rust in the
  hollows, bright on the guide horns, the wheel path and the cleats); dust in the colour of the ground she stands on,
  thick on the running gear and gone by mid-hull; mud where a wheel's dish keeps it, and splashed up the lower third
  of hull skirts; grime in every recess; paint rubbed dark and worn through only where boots and hands come (an edge
  never goes paler than its paint); an exhaust that is dark mottled rust, sooted round its outlet.
* Markings are each nation's plain period marking and nothing else.
* Smoke and flame are many small soft puffs and ragged tongues, never solid balls.

## Adding a vehicle

Write a function in a `tanks_*.py` (or a new file) that builds it on `Works` (a towed gun on `GunWorks`, a gun in a
hull on `SpWorks`, anything on tyres on `CarrierWorks`), end it with `tank_clips(...)`, `gun_clips(...)` or
`car_clips(...)`, register
it in `vehicles.py`, look at it with `landlook.py`, then export it, make its sheet and clips, and send the pictures to
the art director's thread.
