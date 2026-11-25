# The ground battle's 3D scene (release 2: the Normandy dry run)

Brian's ask of 21 November 2026: a ground battle in 3D as intense as the air battle, realistic, with a man who is hit
falling and staying on the ground dead; first a Normandy dry run of Shermans against a Tiger platoon, infantry and
artillery, recorded and filmed like the air battle's. The battle is the land rules' (`src/battle/land` on
`claude/r2-support-teams`), recorded once by `Tools/land_record/record.ts`. This scene plays the recording and draws
it, and the battle screen sits over it.

No gore until Brian rules: a man struck shows a puff of dust and a faint dark mist, then falls.

## The files

| File | What it is |
|---|---|
| `LandFeed.h/.cpp` | Reads a recording (`Content/Data/LandBattles/<key>.json`) into the cores' terms: the field, the tracks (each man, team, gun and vehicle with its path), every round fired, the events, the after card. Plain C++. |
| `LandPlay.h/.cpp` | The fire. Every round in the recording becomes a muzzle flash where it was fired and a landing where it struck: a puff in the dirt, a spark and plate mark on a tank (the plate the round struck), a blast for each shell paired with the round that killed by it. It hands on the effects, the noises and the losses. Plain C++. |
| `LandSound.h/.cpp` | The mix: which recording plays for each noise, how loud, how far it carries, the crack of a round passing near the ear, the beds under the battle, the shell shock after a near blast. Every number comes from the sound director's `mix.json`; nothing here copies them. Plain C++. |
| `LandEye.h/.cpp` | The camera: wide, shoulder, low in the grass, beside a tank, watching a man, the village. It cuts on the battle's beats, never through a hedge or wall, held a little unsteady in the hand, and never down among standing corn or tall grass. Plain C++. |
| `LandSight.h/.cpp` | What stands where, for the camera: whether a point is inside a tank, a gun, a man, a tree's trunk or crown, a hedge's shrubs, a house or a wall. The field's things are laid once in 10 m squares; the men and vehicles are placed again each frame from the cores (a man a rod from his feet to his head, a vehicle her model's hull and turret boxes and a rod to her muzzle). The cloud test lays the same field. |
| `LandCrater.h/.cpp` | The shell holes: torn earth laid on the ground for each shell, bomb and grenade that lands in it, sized by the round, kept the whole battle. Plain C++. |
| `LandDesk.h/.cpp` | The screen's words: the title, place and date, the clock, the tug-of-war bar's knot, HOW NEAR TO BREAKING OFF, the dispatches, the after card. Plain C++. |
| `LandField.h/.cpp` | The ground: the bocage's fields and hedges, lanes, the village, woods, slopes, in tiles near and far, and the small things (grass, stones, shrubs) laid round the eye. Plain C++. |
| `LandMen.h/.cpp` | The soldiers: each man's pose from the soldier files' clips (walk, run, kneel, prone, fire, reload), his fall when hit, and his body lying where it fell for the rest of the battle. Plain C++. A man is (block, k, j): j is a follower of key man k in the drill's sections (0 the key man), with the roles leader, loader and crew besides the key men's. The scene plays the crowd thrifty (`SetThrift`: a man away from the eye posed every 2nd to 4th frame by distance, his place, shots and falls every frame) and skins the men who moved over all the cores (`ParallelFor`; a body's first skin, which reads the ground, on the game thread). 698 men: about 1 ms a frame on 4 cores. |
| `LandKit.h/.cpp` | The vehicles and guns: Shermans, Tigers, Panzer IVs, the field guns and lorries from `Content/Models/Battle`, their tracks, turrets, dust, soot, and the fires and cook-offs of a kill. Plain C++. |
| `LandAir.h/.cpp` | The fighters on call (release 2, `-TheaterLandAirSupport`): each leg's planes down their run, the ground's guns and the lost plane's fall, from the rules' air notes. Plain C++; drawn by `LandBattleAir.cpp`. |
| `LandBattleState.h` | The plain state above, kept out of the actor's header so the standard library never meets Unreal's header tool. |
| `LandBattle.h/.cpp` | `ALandBattle`, the scene. It feeds the cores and draws what they say: the ground as merged meshes per tile, the men, the vehicles, the effects kit, the sky and haze, the camera, and the sound. |
| `LandBattleScreen.h/.cpp` | `SLandBattleScreen`, the screen. It draws the desk's words and takes the mouse and keys. |
| `../../../Tools/land_play/` | The cloud test of the fire, sound, camera (over the laid field), words and shell holes: `run.sh` builds it with both compilers' strict flags and plays the recording at 60 frames a second. |
| `../../../Tools/land_look/` | The ground's own detail pictures, made by `make_ground_textures.py` from numbers with a fixed seed (nothing fetched or copied): `T_LandGrain.png` (a grazed sward and a loam's clods, about 2 m a repeat), `T_LandBump.png` (which way each leans) and `T_LandPatch.png` (the broad lie of a field, about 40 m: value, warmth, trodden places, and where the tussocks gather, which the material lays on grass only, never on soil). |
| `M_LandSurface` (`Tools/build_content.py`, `build_land_surface`) | The ground tiles' material (near, far and the ring): M_MapSurface's vertex colours with that grain over them, strong close by and calm far off, only where the ground faces up. The hedges, trees, grass, houses, tanks and men stay on M_MapSurface, and the campaign map never uses it. Without it the ground falls back to M_MapSurface and the log says so once. |
| `Content/Data/LandBattles/normandy-1944.json` | The dry run: Le Mesnil, July 1944, a 3 by 2 km field; 113 tracks, 3,725 rounds; American infantry, Shermans and 105 mm howitzers against German infantry dug in, a Tiger platoon beside Panzer IVs, and leFH 18s. (Tigers in Normandy mostly faced the British; the platoon is kept small for that reason.) |
| `Sounds/Real/*.wav` (95 new) | The ground recordings of the approved battle pack: each army's rifles, machine guns and sub-machine guns, tank guns and engines, artillery and mortars, shells landing, the men's falls, orders and kit. Credits are in `credits.json` and `CREDITS.md`. |

## Running it

1. `Tools/build_sounds.sh` brings the 95 new recordings into `/Game/Campaign/Sounds/Real`. Without it the battle plays
   silent and nothing fails (the log names each missing sound once).
2. `Tools/build_content.sh` (with the editor closed) makes `/Game/Campaign/Materials/M_LandSurface` and brings in its
   three pictures from `Tools/land_look/` under `/Game/Campaign/Textures/Land` (`T_LandGrain`, `T_LandBump`,
   `T_LandPatch`). It leaves the other materials alone, M_MapSurface among them. Commit the four new `.uasset` files
   (`Content/Campaign/Materials/M_LandSurface.uasset`, `Content/Campaign/Textures/Land/*.uasset`) as the others are.
   If the pictures are painted again (`python3 Tools/land_look/make_ground_textures.py`), or M_LandSurface was built
   before from an earlier commit (the tussocks moved to `T_LandPatch`'s alpha after the first one), run it as
   `TC_REBUILD=M_LandSurface Tools/build_content.sh` to bring them in again. Should anything in it fail, the log says
   so and the rest of `build_content.py` goes on; the battle then draws its ground on M_MapSurface.
3. `-TheaterLandBattle=normandy-1944` on the command line plays the recorded battle once the table is up. The table is
   hidden while it plays. The clock runs at twice life until the armies are about to meet, then at life.
4. Adding `-TheaterLandBattleShots=<folder>` takes the art director's pictures and then quits: `01-opening`,
   `02-the-advance`, `03-among-the-men`, `04-in-the-grass`, `05-the-tanks`, `06-a-man-falls`, `07-a-tank-knocked-out`,
   `08-the-village`, `09-the-fallen`, `10-after-card`.
   `-TheaterLandWalkPan=<battle s>[,<seconds>]` (20 s by default) is the photographer for the Mac's look at the
   crowd's walk (LandWalkPan): from that battle second the eye stands still some 70 m off the flank of the block
   walking most steadily (45 to 100 m, a man's height up, or 3.5 m where a hedge is in the way) and turns with the
   block's middle as it walks by, a still each real second (`<key>-walkpan-NN.png` in the stills folder, or
   Saved/LandWalkPan). No block walking then: asked again 10 s on, up to six times. The same recording gives the same
   block and place, so the base and `-TheaterLandNoVat` show the same men; the cloud test checks one is found.
   `-TheaterLandTankPan=<battle s>[,<seconds>]` is the same photographer for the far tanks' motion vectors: the tank
   driving most steadily (2 to 9 m/s), the eye 170 to 240 m off her flank (past the 140 m hand-over, so she is drawn
   by her model's far pieces) 2.5, 6 or 10 m up, on a 12 degree lens, a still each real second (`<key>-tankpan-NN.png`,
   or Saved/LandTankPan). With `-TheaterLandCrowdNoPrev` it is the A/B: the same tank and place. The cloud test finds
   one in Normandy at 50 s (the hedges leave few open lines) and Prokhorovka at 30 s.
5. Other switches for trying the look: `-TheaterLandHand=0..3` (how unsteady the hand-held camera is, 1 by default),
   `-TheaterLandGrain=` (film grain, 0.25), `-TheaterLandNoFocus` (no shallow focus), `-TheaterLandSize=40..100`
   (screen percentage), `-TheaterLandTextScale=` (the screen's words, 1.25). The day's look (any number left empty
   keeps its default, so `-TheaterLandFill=0.42` changes only the first):
   - `-TheaterLandWeather=cover,deck,base`: another day than the recording's (cover 0 to 1; deck 1 for a deck of
     cloud, 0 for cumulus; the cloud base in metres). `-TheaterLandWeather=0.95,1` is an overcast morning.
   - `-TheaterLandWeatherWord=mud` (release 2): the campaign's word for the ground, as a recording's root `"weather"`
     gives it (clear, rain, snow, mud). `mud` turns open earth to the thaw's mud (the holes' soil, torn ruts, mud
     thrown). It is not the sky's `-TheaterLandWeather=` above: given that one as `mud`, the sky read a clear day and
     the ground stayed dry (23 November). The two may sit on one line.
   - `-TheaterLandScars=N`, `-TheaterLandScarMonths=M`, `-TheaterLandNoScars` (release 2): old shell holes from
     earlier battles over the same field, as a recording's root `"scars": {"count", "months"}` gives them.
   - `-TheaterLandOldRuts=N`, `-TheaterLandOldRutMonths=M`, `-TheaterLandNoOldRuts` (release 2, mud memory): the ruts
     of battles fought here in mud before, as a recording's root `"oldRuts": {"count", "months"}` gives them: dried
     pale under three months, then a lusher green line fading; puddled with `-TheaterLandWeatherWord=mud`.
   - `-TheaterLandCraterOldEdge`: the shell holes' edge as before the art director's 23 November seam (no feather);
     `-TheaterLandMenFieldFrame`: the near men's meshes left at the field's corner (no motion vector), for the A/B.
   - `-TheaterLandHour=`: another hour (local sun time) for the sun.
   - `-TheaterLandFill=shade,overcast shade,kelvin,height` (0.36, 0.62, 7500, 60): shaded open ground's share of
     sunlit in light on a fair day and on an overcast one, the sky fill's warmth, and how high it stands in degrees.
   - `-TheaterLandHaze=start,density,falloff,most` (0, 0.0069, 0.1, 0.98): the density is the one at the field's
     ground; the falloff is the engine's (0.1 halves it every 100 m up).
   - `-TheaterLandSky=bright,air,dust,hazeover` (1.3, 1.0, 0.6, 1.0): the sky's brightness, its blue and its dust
     against the engine's numbers, and how much of the haze lies over the sky.
   - `-TheaterLandNoClouds` (no clouds), and the air battle's own `-TheaterCloudBright=` (1.3), `-TheaterBattleGrade=`
     and `-TheaterNoBattleGrade`.
   - The shade (the art director's cut 4 fixes 1 to 3): `-TheaterLandContact=` (0.3: the sun's contact shadow in
     metres of world; 0 keeps the old 0.05 of the screen), `-TheaterLandSkyShare=` (0.6: the shade's share carried by
     the sky light, the rest by the fill), `-TheaterLandSkyBelow=` (0.3: the sky's lower half the field's colour times
     this; 0 black), `-TheaterLandFootShade=` (1: the footprint shade under tanks, guns and men, M_LandShade; 0 none),
     `-TheaterLandTopLight=` (0.08) and `-TheaterLandMapLight`.
   - `-TheaterLandNoPoseBank`: every man skinned, as before the pose bank (SO1).
   - `-TheaterLandNoVat` (the men's crowd off: far men a mesh a pose, mid men skinned, as before SO1's draw),
     `-TheaterLandNoVatMid` (the crowd for far men only; mid men skinned), `-TheaterLandMassMinPx=` (2: in the wide
     mass shot a far man is drawn no smaller than this many of the picture's lines; 0 none).

Keys on the battle screen: Space holds the clock or lets it run, 1 to 4 the speeds (half, life, twice, four times), C
cuts to a new shot, H holds this shot, Enter CONTINUE on the after card. A drag looks a little aside; let go and the
camera comes back to its shot.

## What the scene draws

- **Every shot is seen and heard.** Each round in the recording flashes at its muzzle and lands where it struck. Most
  rounds miss (the land rules' odds), so most landings are dust in the grass or a chip off a wall. A shot on screen is
  always heard; only far and off-screen shots give way when the mix is full.
- **A man hit falls and stays.** His fall is one of the soldier file's deaths for his stance (standing, running,
  kneeling, prone, in cover), picked so the same fall does not repeat nearby. He lies where he fell, on the slope of
  the ground, his rifle beside him, for the rest of the battle; the camera's last picture walks past the fallen.
- **Far men are drawn from the pose bank (the true-scale plan's SO1).** The soldiers thread's
  `Content/Models/Soldiers/pose_bank.json` is read before the men are made: each soldier file's far mesh is baked in
  about 70 poses (and each again with the rifle laid down). A man seen 120 m off or more shows the bank's pose nearest
  to where his clip has got. His play is unchanged; only his look between poses is. Men alike out there (the same far
  variant, helmet finish and army colours) share one mesh a pose, made the first time it is needed, four a frame at
  most, and each man is an instance of it, moved only when his frame changes. Nothing of his is skinned or sent: a
  far man costs the Mac a place in an instance list, not a draw and a mesh of his own. A man whose mesh is not made
  yet is skinned as before for those frames. A mesh holding no one is hidden. The log gives the bank's sets and bake
  time, and the meshes' cost at 50, 200 and 500 made. The instances turn his own axes to Unreal's with a plain turn:
  his frame mirrors (left-handed to the field's right-handed), and the field-to-Unreal swap mirrors back.
- **Mid and far men as one crowd (SO1's draw, `LandVat`, release 2).** With `M_LandCrowd` (built by
  `Tools/build_content.sh`) the men past the near level (some 8.6 m as seen, GS3) are not skinned at all. Each body set
  (men alike out to 25 m but for helmet and face: Normandy has 7, Prokhorovka 2) is one mesh at rest and one instanced
  component at each of the mid and far levels; every pose of the bank is in one picture made at load (a column a
  point, two rows a pose: each point's way from its rest and its facing, half floats; 37.6 MB on the card for
  Normandy, 9.7 for Prokhorovka). Each man is one instance: his place and nine numbers, the two poses his clip is
  between and the share of the second (so he goes smoothly between them, not pose to pose), the same a frame before
  (for the anti-aliasing), and his face's tone. The card moves every point. His helmet is an instance of its own, one
  component for each set, finish and level, riding his head. Mid men cast their shadow, far men none, as before. In
  the wide mass shot a far man is grown about his feet to stand at least 2 of the picture's lines, so the whole army
  reads as men. The packing is checked in the cloud by `Tools/land_show/land_vat_pack_test.cpp` (every set, level,
  pose and point: the material's lookup lands on its own texel; the halfs' rounding at worst 1.7 mm, 0.14 of a line at
  the nearest mid man; facings within 0.02 degrees; every pose inside the grown bounds). Without `M_LandCrowd`, or
  with `-TheaterLandNoVat`, the far men are drawn as above and mid men skinned. A man given back (near again, fallen,
  out of sight) has his place filled by his component's last man, and the places left past the last are removed at
  the frame's end, so a component draws only men (GS7). A man's move from frame to frame has its motion vector for
  the anti-aliasing, as his limbs' have: each component keeps every instance's place a frame before
  (`SetHasPerInstancePrevTransforms`, set before it is registered), and each move sends the new place with the one he
  stood at the frame before. The engine's own instance update never sets that place (pre-check, UE 5.8: without it a
  man moving inside his component reads as still). A man who stops, or is left where he was out of the picture, has the
  two made one at the next frame's end, so he does not smear along his last step. A place new to him is sent with no
  move, and a man moved into a given-back place keeps his own place a frame before, not the given-back man's.
- **A tank struck shows it.** A round that strikes leaves its mark on the plate it struck. A kill brews up, burns or is
  only knocked out, as `Content/Data/Battle/fates.json` draws it, and the hull blackens and smokes.
- **Fuel going up (release 2).** As a tank brews up (her ammunition's blast), and as a lorry is knocked out (its fire
  starting), the effects kit's `FuelBall` rolls a ball of burning fuel off her engine deck: about 8 m across for a tank
  (the art director's ruling), 6 for a lorry, once a vehicle, carried on as she was moving, a second or so, black smoke
  boiling off its top. Her own fire burns on after it as before. Tanks only knocked out (no brew-up) get none.
- **Tank commanders up in their hatches (release 2).** Each American Sherman and German Panzer IV and Tiger has her
  commander drawn: the soldiers thread's `soldier_USA_tanker` and `soldier_GER_panzer` (Content/Models/Soldiers), a
  man of the crowd with no track of his own (`Crowd::AddRider`), seated each frame on her `commander` socket, moved by
  his clip's place for her model (`root_offset`: forward, right and up of the socket; the turret's facing, the hull's
  up). He rises out of the hatch as the lid swings open (from 40% open) and sinks as it shuts; down inside he is not
  drawn. Up there he looks about with his glasses; from some seconds before her roof gun's bursts to some after he
  is at the gun, and fires each burst (`MakeRiders` reads each burst's gun from the replay's own `TankMg`). Killed
  with her while he was up (the rules' hatch open at her end), he slumps over the rim or drops inside, by her own draw;
  thrown with her turret when she blows up; a round that struck him up there (the rules' `cm`) drops him inside and
  she fights on buttoned up. His rifle is put away (its bones drawn to a point). No shade on the ground, no place in the
  camera's movers (she stands for him). A burst due while her lid is still swinging open (over 15% open) brings him
  straight up, so no roof gun fires unmanned. Tanks that join a live battle later (`Extend`) get no commander yet:
  `MakeRiders` runs once, at the start. `-TheaterLandNoCommanders` leaves them out.
- **Roof guns as the models draw them.** A German tank's MG 34 on the cupola's rail (`roof_mg_cradle`) is drawn only
  while her commander mans it; the Sherman's .50 stays on its post. A tank whose model draws no roof gun this month or
  for her nation (an early Tiger; a British Sherman, `swaps.roof_mg.by_nation`, now read) never fires one: the replay
  asks the scene (`World::RoofGun`) and gives her coaxial instead.
- **A/B switches for the production lead.** `-TheaterLandNoMassWide` turns off the camera's whole-army wide (the
  dearest shot until the far men are instanced).
- **The men's detail by their size in the picture (the battle-scale plan's GS3).** A man's level goes by his seen
  distance (the usual 50 deg lens's, so a long lens's far men count as near): Near while a 1.8 m man stands over 150
  of 665 lines (within 8.6 m), Mid to 80 m, Far beyond, each crossed back with a twentieth's give. His shadow goes with
  the level: on at Near and Mid, off at Far, switched only when his mesh is made again for a new level (a shadow
  switched alone rebuilds his draw as dear as the mesh). A man under 2 lines (past some 640 m) is not drawn, except in
  the camera's wide of the whole army (`MassShot`), where the far men are its crowd. At most 50 men's meshes are made
  again a frame; the rest keep their old level a frame or two longer, and a man not yet drawn waits. Switches:
  `-TheaterLandNearPx=150`, `-TheaterLandShadowTo=80`, `-TheaterLandCullPx=2` (0 draws every man),
  `-TheaterLandRebuilds=50`. Every man within 25 m is still posed every frame whatever his level.
- **The field's own draw by distance (the battle-scale plan's GS9).** The hedge shrubs and haycocks are drawn plain
  (their far level) from some 129 m, their shadows end at 250 m, and none is drawn past 500 m. The trees are drawn
  plain from some 300 m. From 400 m each tree is a 42-triangle crown (`LandField::CrownOf`: a three-sided trunk and a
  six-sided ball, sized and coloured from the tree's far level, darker beneath) with no shadow, and none is drawn past
  1.2 km. The plain levels' distances are what the Mac build already drew: the old switch was worked out for a 90 deg
  lens, so at the usual 50 deg it fell at 2.14 times the field's FarFrom. A level with no shadow has its sections'
  shadows off (`FStaticMeshSection::bCastShadow`). Switches, in metres (0 leaves that step out):
  `-TheaterLandShrubFarFrom=`, `-TheaterLandTreeFarFrom=` (the plan's 80 for the A/B), `-TheaterLandShrubShadowTo=250`,
  `-TheaterLandCrownFrom=400`, `-TheaterLandShrubsTo=500`, `-TheaterLandTreesTo=1200`. The log says them at load.
  The corn's bundles cast their shadow only near the eye: each corn mesh has a shadowed twin, and the clutter's 16 m
  squares whose nearest point is within 25 m of the eye lay their corn in the twin, the rest in the plain one, laid
  again as the eye crosses the line by 4 m (`LandCarpet`'s `ShadowTo`, `Twin`). Every bundle within 21 m casts its
  shadow and none past some 52 m; at Prokhorovka's wheat a quarter of the corn laid now casts one, against all of it
  before (the land field test, an eye at a tank's pace). `-TheaterLandCornShadowTo=` (25; 0: everywhere, as before).
  The clutter's meshes are kept packed (GS7): a place a square gives back is filled with its mesh's last thing and the
  last place removed, and a square laid again (a hole, a track, the eye nearer) moves only what changed. Before, a
  given-back place stayed, shrunk to nothing where it stood, and was still drawn: in the land field test's walk some
  3,300 such places a frame at Normandy (up to 17,000) and 2,400 at Prokhorovka (up to 9,600); now none. The 10 s log
  says the clutter's places against the things laid. `-TheaterLandClutterShrunk` keeps the old way.
  The ground's tiles are static meshes (GS8): each tile's near, far and built level is made once at load as a mesh
  of its own, so the engine draws it from its cached draw commands. As procedural meshes, some 450 of them were
  gathered again on the draw thread every frame, in every pass, shadows included. The 170 tiles of the far ring, 1 km
  and more off, cast no shadow. The points, normals and colours are the same bytes as before. The log says how many
  were made and how long it took. `-TheaterLandTilesProcedural` draws them as procedural meshes, as before.
  The torn ground settles into static meshes too (GS8): a section of a patch's holes or tracks, or a part of a dug
  tile's near ground, is sent into its procedural mesh as it changes, as before. Once it has stood unchanged 3 s it is
  baked into a static mesh of its own and its procedural section emptied, within 1 ms a frame. A patch's procedural
  mesh then holds only the pieces still changing, so a new hole remakes only those. A hole torn in a baked piece makes
  it procedural again until it settles. The engine cannot change a procedural section's triangles in place (its update
  takes points only), and a hole's cut changes them, so this is how the torn ground stays mostly static. The 10 s log
  says the sections sent, the pieces baked, those still procedural and the slowest frame's baking.
  `-TheaterLandTornProcedural` keeps them all procedural; `-TheaterLandTornSettle=` (3 s) and `-TheaterLandTornBakeMs=`
  (1 ms) tune it. The effects kit's own painted crater marks stay off in this scene (the holes are the torn earth's);
  `-TheaterLandKitScars` brings them back for an A/B.
- **The burnt coat (release 2, on trial: `-TheaterLandTrial=burntcoat`, off; `-TheaterLandNoBurntCoat` off whatever
  the trial).** A burnt-out wreck goes to three colours in place of one sooty black (the art director's brief of 25
  November, for every army). `LandKit` gives every point of every mesh its own burnt colour at load (`Mesh::Burnt`,
  read with `Mesh::BurntOf`; `Model::bBurntCoat` says so): soot above and round what the smoke poured from (her
  `engine`, `fire.*` and `ring` sockets; it climbs), under overhangs, in stains, and over what the fire left coolest,
  where her paint charred and keeps a little of its tone; rust, from a dark scale to a red-brown with a thin soot
  film, in streaks down upright plates, on her tracks and running gear; and pale ash in flakes on what looks up and on
  the thin band of rubber round a tank's road wheel, never on a plate's edge. A lorry's, a half-track's or a
  motorcycle's tyre (a wheel whose rubber is more than about a quarter of its radius) chars, with its ash lying on it
  in flakes: a clean pale ring reads as a white-walled tyre. A vent's reach goes with her width, is halved on what is
  no longer than a motorcycle and its sidecar, and is counted from her nearest skin where a fire's place hangs in the
  air of an open body (a half-track's). The near mesh is cut along the ash's line (`CutAlong`, long edges only:
  some 3 to 6% more near triangles, the far mesh untouched), so a flake ends at an edge with a soft shoulder and does
  not fade across a 25 cm plate. Her live paint is as it was: the coat changes only what a point burns to and how far.
  Near wrecks take it in `LandParts::FromBody`; far ones carry it in their instanced pieces' texture coordinates
  (`LandCrowd`'s `PieceMesh`: red beside the soot weight, green and blue in a second coordinate) and `M_LandSteel`
  reads it where its `BurntByPoint` is 1. **`M_LandSteel` must be rebuilt for the far wrecks**
  (`TC_REBUILD=M_LandSteel`); with the old one they stay the one black until they come near. The cloud's sheet:
  `LAND_KIT_BURNT=1` (three hulls before and after, and one burnt out among live ones from 150 m), `=more` (twelve
  other kinds) or `=bikes` (the motorcycles) with `Tools/land_kit/land_picture.cpp`, her marks the game's own so they
  burn with her skin; `LAND_KIT_MASK=1` writes each picture's vehicle and sunlit pixels beside it for measuring. The
  vehicles' suite has its own section (`THE BURNT COAT`; `LAND_KIT_ONLY_BURNT=1` runs it alone): off as before, her
  own points and paint untouched under the cut, and soot, rust and ash together on what does not look down. Not yet:
  the half minute of burning takes her paint straight to these colours; black first and rust after would be truer.
- **Far vehicles as instances, links only in the picture (TK1, TK3; the tank modeller's, merged).** Past 140 m a
  tank, lorry or gun that fits her pieces within a couple of pixels is drawn as at most two instances (hull, turret),
  merged once a model at load (`LandCrowd`, `LandKit::SplitFar`), her soot as per-instance data (`M_LandSteel`, built
  by `Tools/build_content.sh`; without it the far wrecks show unburnt paint). A German roof gun's cradle is kept out of
  the pieces, so it still shows only while manned. A near tank's links are posed only while she is in the picture.
  The wrecks' soot steps are spread over frames (40,000 points a frame). Each piece's instance list is kept packed, as
  the men's crowd's is (GS7): a vehicle no longer drawn by her pieces gives her place to the piece's last vehicle, and
  the places past the last are removed once a frame, so the log's instance count is what is drawn. Each piece keeps
  its instances' places a frame before (LandVat's pattern for the men), so a far tank driving, or one the lens follows,
  has her own motion vector for the anti-aliasing and motion blur instead of being drawn as still (smeared, or blurred
  with the ground behind her): a move sends where she is and where her instance was the frame before, one not moved
  again is made still the next frame, and one newly drawn by a piece (the hand-over, the lid-up copy) takes her
  anchor's place the frame before. The log's crowd line counts the places sent moving and made still a frame.
  `-TheaterLandNoCrowd`, `-TheaterLandLinksAlways` and `-TheaterLandCrowdNoPrev` turn them off.
- **The speed log.** Every 10 s, after the cores' line, a second line: what a frame's drawing cost (the men's meshes,
  the tanks', the effects kit's tick, the sound), the card's own frame time (for A/Bs of the field's and the men's
  draw), the men in the picture a frame and the most in one frame, the meshes made again or held at the old level a
  frame, and the men's crowd (SO1): men drawn as its instances a frame, mid and far, and in the mass shot how many far
  men were grown to the least size and the most, times his size; and the most smoke puffs the effects kit drew in a
  frame, with the room asked then.
- **The effects kit's frame in two halves (FX2, the effects thread's; merged up to 5833912a).** A frame now plays the
  fire first (the shells, guns and smoke rounds told the kit), then digs and settles the holes and tracks, then calls
  the kit's `TickBegin`: the kit's drawing starts on a worker and the hand-held eye gets its jolt at once. The men and
  the tanks are drawn meanwhile, the tanks' and men's own beats (cook-offs, wreck fires) played, and `TickEnd` waits for
  the worker and sends the smoke. In between, the worker asks the ground under a point of the field's `At`, which is
  read-only: nothing in the scene changes it. What the scene asks of the kit in between (a tank's dust, a cook-off) is
  held and done at `TickEnd`, first drawn the next frame. The log's effects time is the two halves' own time on the game
  thread (`TickEnd`'s wait for the worker included).
- **Smoke at the lens.** The effects kit lays each puff flat to the lens. The land camera stands among the bursts, so a
  shell's smoke whose middle was level with the camera a few metres to one side was laid across the whole picture as
  one dark disc. The land scene now calls `SetLensFade(1, 5, 0.3)` on the effects: smoke and dust thin out as their near
  side comes within 5 m in front of the lens and are gone within 1 m. Nothing is laid whose middle is behind the lens or
  within 0.3 m of it. Flashes, sparks, tracers, thrown clods and smears are left alone. The air and sea scenes never
  call it, so they draw what they drew before. The smoke's darkness is the recipe's earth row for a fresh 105 mm burst,
  and is left as it is.
- **Shell holes (`LandCrater`).** Each shell, bomb and grenade that lands in open ground leaves a hole for the rest of
  the battle: a ragged ring of thrown earth standing over the ground, a dark damp hollow, a fan of spoil thinning out
  with patches of torn turf, and clods scattered round it. All of it is in the soil's browns: earth, mud, sand, woods,
  town or snow, as the ground under it says. None is made on armour, in a house, on a wall's line or in water. The size
  goes by the round: a grenade's scuff about 0.5 m across, a mortar's 1.3 m, a 75 or 88 mm gun's 1.5 to 1.8 m, a 105 mm
  howitzer's 2.8 m and a 150's 4 m. The holes are drawn on the ground's own paint (`M_MapSurface`, unchanged) and
  grouped by place: one mesh for each 64 m square of the field that has holes in it, with 24 holes to a section. When
  any section of a mesh changes, the engine copies and uploads all of that mesh's sections again. It also redraws the
  shadows cached under the mesh's bounds (the scene uses virtual shadow maps). So a new hole rebuilds only its own
  square's mesh, about 70 m across with the thrown soil, never the whole field's. At most 2,400 holes are kept. The
  dry run makes 949 of them, about 200 points each, in 80 squares. In the heaviest frame of the dry run (a barrage),
  5 meshes and about 50,000 points are sent again. If one mesh held them all, that would be all 200,000 points and
  the shadows of the whole field. The cloud test checks that no mesh sent again is wider than its square and the soil
  thrown round it. On the Mac, watch the log's field time (`FieldMs`, which now counts the holes' meshes too) and
  the slowest frame during the barrage. The grass and corn laid round the eye are left out of a hole
  and laid again when a hole lands within 20 m of the eye or 15 m of what it looks at, at most every 2 s. The effects kit's own flat painted craters are turned off
  in this scene (`SetCraters(false)`); the rest of a shell is unchanged.
- **Holes and tracks lit as the ground they lie on (cut 4, shot 07).** In a low sun a hole's thrown soil, lit by its
  own slope down from the lip, drew a ruled polygon round the hole, a tenth brighter on one side and darker on the
  other; a tank's tracks read as flat stripes on a mat. Now the thrown soil leans to the near ground's own normal toward
  its edge (`Ground::DrawnUp`: the tile's points' normals mixed across the triangle the place is in, as the mesh is
  lit), wholly at the thin edge, and each track strip is lit by it outright. Each strip is six points across instead
  of four: the ground's colour at the edge, a feathered band part paint and part ground, the pressed strip mottled; its
  widths and tones change at every 1.2 m piece by a hash of the place, so the edges run ragged and one piece's end
  meets the next one's start. The flattened corn between a Tiger's tracks is darker than the open ground, and the grass
  and corn crushed under the tracks are cut to a ragged edge, each thing by its own place. The field test checks the
  edges against the drawn ground's lighting (95% within a degree). The holes' and tracks' heights now come from
  `HeightAt`, a fifth of `At`'s cost: a track piece costs less than before for all its extra points.
- **The camera stays out of the corn.** `LandEye`'s world now has `Crop(x, y)`. The scene builds it from the field's
  own clutter: the tallest wheat or tall grass laid within 0.7 m, the same stalks it draws. A lens near the ground (the
  low shot, the shoulder, beside a tank) rises to 0.12 m over the ears. It rises at once and comes down slowly. A
  low shot the camera chooses for itself where the crop stands over 0.45 m becomes a shoulder shot instead. A low shot
  asked for by name keeps its subject and rises over the ears. The cloud test fails if any frame of those shots has its
  lens fewer than 0.05 m over the crop.
- **Near the lens the corn is stalks, not bundles (the art director's cut 4, fix 4).** While the eye is under 2.5 m
  over the ground (until it rises past 3 m), the wheat's bundles within 3.5 m of it along the ground are taken away
  and each clump is drawn as its own stalks (`LandField`'s `Stalks`: 48 thin stems of two strips, two or three
  drooping leaves, an ear of crossed zigzag strips, the stem's foot taking the soil's colour; the poppies have cups of
  four petals and nodding buds). They are on `M_LandStalks` (`build_content.py`): lit as foliage, faded in by dither
  from 0.6 to 0.9 m of depth so no stalk stands across the lens, and out from 3.5 to 5 m along the ground where the
  bundles take over, with whole shadows. From higher, the stalks are hidden and the bundles are back (a clearing seen
  from above would be a bald ring in the corn). The grass squares (`LandCarpet::Clear`) hide and give back the bundles
  a thing at a time as the eye moves, rises or jumps; the cloud test checks that no bundle is laid within 3.5 m of a
  low eye and that they come back under a risen one. Without `M_LandStalks` the corn keeps its bundles to the lens,
  said once in the log.
- **The camera keeps the art director's rules (her notes on the first film).** A fifth of that film was over 40 per
  cent black: the lens pressed on a German turret for 17 s, under a tree's crown, on a dark hedge, under the ground,
  inside a GI. Now:
  - No lens within 1.5 m of a thing or 0.9 m of a man, and no picture more than 30 per cent filled by a thing within
    3.5 m of the lens (a man within 1.5 m, the ground within 0.8 m). `LandEye` asks `World::Solid` (`LandSight`) in
    28 rays across the picture before it takes a shot, trying the other side, farther off or higher, and other shots,
    up to 16 tries a cut. During a shot it looks round every quarter second; found bad twice running, it cuts at once
    (a "rescue", the only cut sooner than 3 s).
  - Cut on action. A tank about to be knocked out, or a man about to fall (three in four, never two within 6 s), a
    second or two ahead and not in the picture: the camera cuts to it just before. One already in the picture holds the
    shot for it. Each cut weighs what lands or falls in the next two seconds two and a half times over the next eight.
    Once the armies are near, a shot in which nothing has fired, landed or fallen for 4 s is left.
  - A fall is kept in the picture to the ground. The eye stops where it was and the lens follows his chest down (the
    crowd's own bones, `World::Body`), never nearer him than 2 m even if he is thrown toward it, and holds 4 s from the
    hit (his fall and a moment where he lies). A fall gone to is seen from his side 10 to 18 m off at a man's height,
    or over the shoulder of a man beside him.
  - Tanks: a ride 3 to 5 m behind the turret and 4 to 5 m up, looking down her gun; and a tracking shot from 20 to 40 m
    off her side as she drives and fires, a long lens, kept alongside as she goes (`track` in the log). The tanks take
    turns with the men: never two tank shots running unless one is about to be knocked out. With the drill's full
    sections (some 600 men) the men would win every cut, so once the armies are near every third shot is the tanks'
    turn: only tanks are weighed, of both sides, a moving one first, one not shown in the last 45 s and the other side's
    before the last one's. A tank on the move is the action of its own shot (not left as "nothing happening"), and a
    man's fall does not take the camera off a tank before 6 s. A tank about to be knocked out comes before any man's
    fall, even out of a fall being held. The dry run: about a third of the film on tanks, 14 tank shots on 9 tanks of
    both sides, all 3 tanks knocked out in the picture.
  - A knocked-out vehicle is laid where it was hit: the recording's last point jumps 14 to 22 m in a tenth of a second
    at the moment of death (its formation place, not its own); `LandFeed` and `LandKit` both keep the wreck where it
    stood.
  - The watching shot is now 28 to 55 m off (it was 70 to 140, men 50 px tall) and 3 to 7 m up, so men fill the near
    third. One aerial opens the battle and one closes it; none in the middle.
  - The cloud test plays the dry run over the same field with the men and vehicles placed from the recording: the
    picture filled near the lens 0.2 per cent of the quarter-second looks, the lens too near something 0.2 per cent,
    no aerial in the middle (it fails over 3 per cent, or on any middle aerial).
- **The battle waits for its materials.** The first time the game is run on a machine Unreal compiles a new
  material's shaders while drawing a stand-in grey: the second cut's opening picture (M_LandSurface's first run) was of
  flat grey-brown fields. The battle's clock now holds, the scene drawn, until no shader jobs are left (at most 240 s;
  the log says how long). A packaged game has none to wait for.
- **The bar.** The tug-of-war bar is the flags' bar of every battle: the player's flag on the left, brass from the
  left meeting slate from the right at the knot, a pale mark at the middle, no ticks and no words on it. HOW NEAR TO
  BREAKING OFF sits under it.

## The light, the sky, the haze and the grade (the art director's notes on cut 1)

All of it is in `LandBattle.cpp` (`HideTheTable`, `LightTheBattle`, `LaySky`, `DrawSky`, `KeepTheLook`), worked out
once from the recording's sky block (cover, cloud base, cumulus or deck, place and hour) and held in `FLandSky`. The
log says it all in its `Land battle look:` lines.

1. **The grade.** By the code, the grade was already on the camera the picture is drawn through: `BattleLook::Grade`,
   the air battle's, written into the eye with every override at weight 1; the eye is the view target; the shallow
   focus writes only its own fields (and depth of field is off game-wide). The stills disagree in part (dark frames
   stay dark, as a fixed exposure keeps them, but the middle tones are no greyer than the paint), so the grade is now
   made sure of: the eye is put back as the view target every frame if anything takes it (with a warning, said once),
   the same grade also lies over the whole world in an unbound post-process volume while the battle stands, and three
   seconds in, the log says what the picture is drawn through and every number its lens carries. If that line says
   the grade is on and the colour is still strong, the cause is the paint: the field's grass and wheat are much more
   saturated than the air battle's sea and land, and the grade's 0.72 in the middle tones cannot bring them to 0.08
   to 0.18 (that is the field's palette, `LandField`, not this work).
2. **The shade.** The only fill was the table's fill lamp (0.75 from a fixed side of the room, about 0.48 on flat
   ground) and a dim sky light, against a sun of 5.9: shade was about a tenth of sunlit in light and crushed to black
   through the film curve. Now the fill lamp is the sky's light: it stands 60 degrees up on the side away from the sun,
   cool (7500 K), throws no shadow, and is as strong as makes shaded open ground 0.36 of sunlit in light (about 0.45 of
   its brightness after the grade); the sun is taken down to keep sunlit ground where it was. On the dry run's morning
   (sun 49 degrees up, cover 0.45): sun 4.18, fill 1.92, sky light 0.83; a crown's shaded side is about 0.37 of its
   lit side in light. Overcast: sun 2.0, fill 2.7, shade 0.62 of sunlit, the shadows soft (6 degrees) and no disc.
   `M_MapSurface`'s TopLight (0.26) brightens up-facing faces in sun and shade alike, so it does not change the
   ground's ratio, but on the crowns it lifts their sunlit tops over their sides, working a little against "half";
   if the crowns' shade is too dark, lower `TopLight` on the land's thing ink per instance (the field's builder's
   `LoadInks`) rather than in the material. Nothing in `M_MapSurface` is changed.
3. **The sky.** There were no clouds at all, and the sky was the battle look's under a thick, high haze that hid its
   blue; the camera seldom looks above 25 degrees. Now the clouds are the air battle's own (`AirSky`): cumulus at the
   recording's base (900 m on the dry run), 1.1 km deep, or a 600 m deck when the day is overcast, laid once round the
   field and turned to the eye as it moves (a millisecond or so a turn, every few metres the eye moves), carried by
   the wind. The air battle's clearing over its target is put 35 km away. No cloud shadows fall on the field. The sky
   is the battle look's atmosphere set for the ground: the low haze laid over it (pale at the horizon, the blue kept
   overhead), less dust than the air's, and duller under cloud.
4. **The haze.** It began at 250 m and lifted only 4 per cent at 400 m and 17 at a kilometre. Now it is a low layer
   from the eye: about 17 per cent at 400 m, 38 at a kilometre, 76 at 3 km, 91 at 5 km where the far ground ends, at
   most 0.98; thin straight up (about 7 per cent). Its colour greys under overcast. The opening aerial (170 m up)
   sees about 25 per cent at a kilometre and 57 at three, a little more than cut 1's 15 and 45.

### On the next Mac run, look at

1. The `Land battle look:` lines in the log: the picture is drawn through the battle's eye, its lens carries the
   battle grade, the grade over the world stands, and no warning that the eye was taken.
2. `07-a-tank-knocked-out` and `05-the-tanks`: shaded ground 0.4 to 0.55 of sunlit, slightly blue; the crowns' shade
   about half their lit side. Too dark or too flat: `-TheaterLandFill=0.42` or `-TheaterLandFill=0.30`.
3. The sky: a deeper blue overhead fading to a pale horizon, the cumulus at about 900 m, white on the sun's side.
   Too pale or too dark: `-TheaterLandSky=1.1` or `1.5`; the clouds' brightness `-TheaterCloudBright=`.
4. The haze: the tree line 400 m off a little lifted, a kilometre off clearly soft, the skyline pale. The opening
   aerial should keep its look.
5. An overcast try: `-TheaterLandWeather=0.95,1`: soft shadows, flatter light, a grey deck, greyer haze.
6. The `Land battle sky` lines every 30 s: how often the clouds turned and what a turn cost (about 1.4 ms on average
   in the cloud test).

## The wind, the clouds' shadows and the sun's shafts (release 2, off until passed)

Brian, 23 November: "can we make the grass, trees, and sky more visually realistic and epic?" There are three switches,
each off until the art director and the Mac's 45 fps pass it; `-TheaterLandEpic` turns on all three. The art director's
round 1 notes are in. Every battlefield is treated alike (the bocage, the steppe, the desert, Italy, the jungle, rice,
the farms, the plains): nothing is set per place. Build the materials once on the Mac:
`TC_REBUILD=M_LandBlades,M_LandCloudShade,M_LandSurface,M_LandLeaves,M_LandStalks`. They read
`Tools/land_look/T_LandCloudWind.png`, made by `make_sky_textures.py`: red is how far inside a cloud's shadow, blue is
the cloud's order, green is the gusts.

### The wind

`-TheaterLandWind`, or `=strength`: 1 is as worked out from the recording's wind, 0 is off.

- A light breeze, not a gale (Brian, 25 November: the first wind "blows to much and is distracting"). Every sway is
  0.6 of the art director's 23 November pass and stops growing at 5.5 m a second; a lull still leans 0.45 of a gust;
  each thing's own swing is 0.6 of its share at 0.7 of its pace; leaves shake at 0.8 of their pace. At the summer
  breeze (2.9 m a second) the grass sweeps some 45% as far as it did, two thirds as quickly.
- Gusts of 20 to 75 m run downwind over the field at about 0.9 times the wind's speed (at least 1.8 m a second). A calm
  day, under a metre a second, stands near still.
- The grass, the flowers and a region's blades lean from the root, about 5 degrees in a gust at a fresh breeze (it was 9).
- The ripe wheat bends toward the ear and nods back over about 2.4 s. The corn's and the gaoliang's stalks do the same,
  taller.
- A region's low leafy plants and the bushes lean less.
- An oak, an elm or a shrub bends toward its top, some 20 cm at 18 m, slowly, and each leaf card shakes 2 to 3 cm.
- Poplars, birches, willows and bamboo move two to three times as much, with quicker leaves.
- Palms' fronds lift and toss.
- Every copy swings on its own beat (the materials' PerInstanceRandom).
- Each gust runs over the grass and the corn as a light patch, and each lull as a darker one, moving with the lean.
  This sheen is 6 per cent on grass and 11 on ripe grain (wheat, and the ground's gold fields by their colour). There
  is none on earth, sand or snow.

The new material `M_LandBlades` is M_MapSurface with the sway and the sheen; the campaign's M_MapSurface is untouched.
M_LandLeaves and M_LandStalks also carry the sway, but stay still unless the C++ sets it. Each kind of tree's leaves get
their own copy of the leaf ink, so they sway with their wood. Things bend only within 250 m of the eye
(`-TheaterLandSwayTo=`; the clutter within 120 m). Each swaying copy's shadow is kept as cached (VSM `Rigid`), so the
shadows are not redrawn for every leaf. With the wind off, the leaves' and stalks' copies are told not to move.

### The clouds' shadows

`-TheaterLandCloudShadows`, or `=darkness`: the sun's share left under a cloud, 0.2. That leaves a shadowed field about
0.49 of sunlit in game: the sky's light of 0.36, plus 0.2 of the sun's remaining 0.64. A tree's cast shadow stays
darker, at 0.36, and cast shadows fade under a cloud as they should.

- The light function `M_LandCloudShade` on the battle's sun lays the fair-weather cumulus seen from below. The clouds
  are lobed, cauliflower outlines 150 to 600 m across with a 30 m soft edge, with sun between.
- The day's cover decides how many clouds are drawn, not how big they are: four fifths of the cover, never more than
  0.4 of the field.
- The picture's clear disc (280 m) is laid at the start over what the opening shot looks at (`Anchor`, from the
  director's first pose; the field's middle if it has none). Six clouds ring the disc, first in the day's order, so the
  opening's first frame already shows a few patches on its flanks and far fields while its middle stands in sun. A
  simulation of the opening (170 m up, FOV 48) gives 17 to 40 per cent of the visible ground shaded at full cover, and
  7 to 33 at a thin day's. The shadows come over the middle as the clouds drift: DrawSky sets the drift every frame,
  the same drift as the clouds drawn.
- It takes the sun off everything under a cloud (the ground, the hedges, the tanks, the men), never the paint.
- There are none on an overcast day, a clear one, or with `-TheaterLandNoClouds`.
- The table's sun is put back as it was when the battle ends.

### The sun's shafts

`-TheaterLandSunShafts`, or `=bloom`: 0.2. This is light-shaft occlusion and bloom on the sun, only on a fair day with
the sun under 25 degrees. When the sun stands before the lens, rays come through the crowns, the hedges, the smoke and
the dust. The bloom starts only above the engine's threshold of 8, so it comes from the sun's disc, fires and flashes,
never as a milky film.

### Checking it

The log reports each one at load ("the wind ON ...", "the clouds' shadows ON ...", "the sun's shafts ON ..."). The
field painter shows the clouds' shadows and the gusts' sheen in its stills with `LAND_FIELD_CLOUD_SHADE=0.2` and
`LAND_FIELD_SHEEN=0.06`. Only the game shows the wind's motion and the shafts.

The art director's Mac shots:

- The opening wide and a low eye in the corn, with `-TheaterLandEpic` against without (the A/B).
- The wide on an overcast day.
- 30 s of shadows crossing, from the wide.
- 20 s of wind with wheat, a hedge, poplars and palms, plus 10 s at top wind and 10 s on a calm day.
- A low-sun shaft shot, plus a midday frame.
- The frame rate for each look alone and for all three, against the 45 fps gate.

## A burning tank as a film shows it (release 2, off until passed)

Brian, 23 November: "the tank on fire doesnt look realistic". Before this, every kill lit the same single bonfire, about
3 m across, that roared unchanged until the battle's end. `-TheaterLandFilmFire` gives each wreck its own story instead
(`LandBurn`), chosen by how the fates table says she ended, her size, and her own dice:

- **A brew-up** (or her turret thrown): flame jets out of her hatches for half a second and the fuel ball rolls into
  black smoke. Then the open turret ring (or the commander's hatch) roars like a blowtorch for one to two minutes. Her
  rounds cook off in pops for minutes, one or two of them jetting 4 to 6 m. Her deck burns under a black column to the
  battle's end: at its height for six to twelve minutes, then low, with small flames in her hatches and a grey stream
  of smoke from them.
- **An engine fire:** the deck catches at once, or after a minute of thickening smoke if she "burns later". The flames
  reach her hatches a minute or two on, and some rounds cook off. It goes out after 10 to 20 minutes and smoulders. A
  table that picked no fate burns this way.
- **A small fire put out:** low flames for a minute or two, then grey smoke.
- **Not burning** (her crew bailed out or were killed, a track off): grey smoke from a hatch or the engine for one to
  three minutes, or none.
- **Lorries and cars:** they burn only if the table says so (`soft.burns`, or flipped half the time); otherwise they
  smoke. **Guns:** a gun whose ready rounds were hit pops them off.

The kit gains `Vent`: flame forced out of an opening, which never melts into the deck's fire and lays no column of its
own. There are two new steam rows, `smoulder` and `smoke`. The switch also lays `Content/Effects/battle-fx-film.json`
over the recipe for that battle (`FBattleEffects::LayOver`): the art director's round 1 for the deck's flames, the
column, the burn-down (`fire.late`), the lamp's flicker, the fuel ball and the cook-offs. With the switch off the
recipe is as it was. Keep `tools/battle-fx/fx-film.json` and that file the same. Effects (R2) blocks in `PlayArmourEvents` play each plan
and leave out the clip's own fire and smoke loops; the clip's brew-up jets, magazine blast and hatch blown open stay.
The log names each wreck's story ("Land battle: Tiger's wreck (tank.brew_up, heavy): ..."). The effects preview plays
the same plan: `FX_SCRIPT=film FX_FATE=tank.engine_fire FX_CLASS=heavy`, with `flatten.py fx-film.json` appended to
its recipe.

Not yet: the scorch spreading from the vents (her soot still blackens every kill over 30 s), heat shimmer, embers.

The art director's Mac shots, with `-TheaterLandFilmFire` against without: a brew-up at 1 s, 90 s, 5 min and 15 min,
from 50 m and from 300 m, plus a frame at dusk; an engine fire at the same times. Also the frame rate in a battle with
eight wrecks or more, against the 45 fps gate.

## A knocked-out tank's crew getting out (release 2, off until passed)

Brian, 23 November: "if a tank gets hit people try and get out before it explosdes". Before this, nobody left a
knocked-out tank: her commander, if he was up, died in his hatch, and the rest of her crew were never seen; and a
brew-up blew at the instant of the kill, so there was no time to get out. `-TheaterLandBailOut` plays the fates table's
own `crew_out` steps (how many, when, running or crawling, to her rear, to cover or to a crater) and its `crew_in`
(a crew that bails out and climbs back in), with `LandBail`:

- **The wait.** A brew-up's ammunition now goes 5.5 to 10 life seconds after the hit (the table's 5 to 10), by her own
  dice (`LandKit::Battle::BrewWaitLeast/Most`, each vehicle's `BrewWait`). Until then she plays `knocked_out` (hatch
  blown, smoke, bail-out); then `brew_up`. Her fire, column and cook-offs start at the blast. With the switch off both
  are 0 and the kill is as it was.
- **Getting out.** Each man climbs out of his hatch (the commander's, the driver's, then the loader's and hull gunner's
  beside them), steps to the hull's edge on the side away from the enemy, drops to the ground and goes. First he runs
  straight back in her lee, 8 to 12 m (a crawler 3 to 5), past her rear as the enemy sees her and into the shadow her
  hull casts, never through it. Then he breaks for her rear (18 to 38 m further), for cover, or crawls to a crater. A
  man staying by her crouches in her lee.
- **Who doesn't make it.** A man still climbing out when the ammunition goes is thrown 2 to 5 m and lies dead; a man
  who had not yet started is lost inside. Everyone on the ground within 15 m of her flinches flat for a second or two
  as it goes, then carries on. While the fight goes on round her, a runner in the open is hit about one time in six
  when her nearest living enemy is within 150 m, falling to one in sixty at 600 m or with none on the field (a crawler
  half that; a little over half of those hit are killed, the rest left crawling). A man crouching in her lee is never
  hit. In at most about one burning wreck in eight, the first man out is on fire, his clothes alight in patches with
  smoke trailing from him. He runs a few weaving steps and falls; his flames die to a smoulder over 10 to 15 s. About
  one in three rolls them out in a few seconds and lives, lying wounded and smoking.
- **The men.** Tankers where the army has them (the art director: "riflemen climbing out of a Tiger would break it at
  once"). They are the crowd's riders, drawn from `USA_tanker` and `GER_panzer` with no rifle (`Crowd::Ride::bUnarmed`:
  those files' infantry clips carry one). An army without a tankers' file (UK, SOV, ITA, JAP, FRA for now) bails out
  in its own infantry figure, rifle hidden, pack still on (no file carries the pack on a bone of its own): her second
  note, "a man with a pack is a far smaller fault than no one getting out". The log says so once per army, until the
  soldiers' thread makes its tankers' file (own uniform, no pack; the Soviets' padded tank helmet, the British beret
  or crash helmet). They are posed by `LandBail::PoseAt` with clips the files already have (`stand_up` for the climb, `run`, `advance`, `crawl`,
  `go_prone`, `cower`, `flee`, `die_forward`, `die_back`). Clips for the climb out, the jump down and a burning man are
  asked of the soldiers' thread; until they come these stand in.
- **The soldiers' bail clips** (labtrr 42bb946f, in all 19 files with a `bail_clips` block). Where a crew's file has all
  four (`LandBail::Wreck::bClips`), he climbs out in `bail_climb_out` (1.5 s, his boots 0.92 m under the hatch's point,
  facing so the clip's step to his right is the side he drops from; her roof's own height over the clip's 0.95 m, a
  Panzer IV 0.82, a Tiger 0.97, a Sherman 1.07, made up from 0.8 s). Then (the soldiers' 18:52Z, ea211cc5) he turns out
  and scrambles, crouched and quick (the art director, 19:33Z: the ground battle's `advance` played at 2.25 m a second,
  never an upright walk on a burning tank), over her turret's roof to its edge (`Wreck::TurretHalf`, else
  a third of her width), steps down onto her deck in `bail_step_down` (0.73 s, off at 0.10 s, 0.42 m out and 0.95 m
  down, her own drop made up between 0.10 and 0.55 s), scrambles to her hull's edge and drops in `bail_jump_down` (1.60 s,
  off at 0.44 s, 1.71 m out and 1.5 m down, her own height made up between 0.44 and 1.09 s). The step is only where
  her roof stands 0.45 m or more over her deck and his file has the clip (`Wreck::bStep`); a man out of a hatch on her
  deck scrambles straight to its edge. He goes off the side away from her enemy (the art director); with her enemy
  about dead ahead or behind, the side toward where he runs. Where that side is blocked (`Wreck::Blocked`: the scene
  marks a side, her back or her bow blocked where another vehicle, living or a wreck, stands within 4.5 m of her hull
  there), the other side if her enemy is ahead or behind, else off the end of her nearest his hatch (over her turret's
  back onto her engine deck) so long as that end does not face her enemy and she is not burning (never over the
  grilles of a burning engine), else the other side. A man down on the side her enemy sees runs straight along her
  flank to past her end first, then breaks for his cover. Where her deck beside her turret is narrower than the step he jumps from where he
  lands (at most 0.05 m past where the jump stands), and steps from nearer her middle if he must. His way over her is
  laid by `LandBail::LayWayOff`. A man on fire falls before the end of his way (a crawler's short way, run). A man on
  fire runs in `burning_run` (its stride, 7.35 m a loop, by the ground he covers) and falls in `burning_fall_roll`,
  living or not. The clips' roots move him: his seat is moved on where each ends, `Crowd::Ride::bOwnRoot` keeps the
  root the new clip's through the blend, and `Pose::Root` is where his body is (a burning man's flames go with it,
  each clip's root as the soldiers' tracks run). The clips take longer to the ground (3.6 to 5.2 s against 2.05 s:
  `Man::ClipsS`), so each man starts up that much sooner (not before 0.3 s after her kill) and every man goes as he did
  without them (the test, run both ways: the same tallies). Without the clips, the same crew to the last number.
  Picture: `Tools/land_bail/sheet.sh <soldiers folder>` (`bail_clips.ppm`;
  `/mnt/project-files/battle-fx/bail-out/bail-clips-r6.jpg`: three-quarters on, from her front, off her back, the
  smoke grenade thrown from her front, and the man on fire).
- **Smoke to cover them** (`-TheaterLandBailSmoke`, with `-TheaterLandBailOut`; release 2, off until the art director
  and a 45 fps reading pass; Brian, 23 November 20:09Z). With her enemy close and in sight (`Wreck::Threat` 0.5 or
  more), the fight going on and her ammunition not about to go (no brew-up), six crews in ten have the first man out,
  her commander, stand in his hatch and throw a smoke grenade toward her enemy (`throw_grenade`: it leaves his hand at
  0.95 s, he holds its follow-through 0.25 s, turns to the way he climbs over 0.25 s and ducks into his climb, 1.45 s
  up in his hatch in all; the art director, 24 November 02:04Z: he is the most exposed man on the field). It lands 9 to
  14 m off toward her enemy, past her hull, and 1 to 3 m upwind of that line (`Wreck::Wind`, the battle's own wind), so
  its drift lies between her enemy and her crew's way back, about 0.9 s later (`Crew::SmokeAt`), and gushes grey-white
  from the ground (the kit's `SmokeScreen`, Kind "grenade", `smoke_screen.grenade`, `Battle::GrenadeLumps`; round 2
  by the art director's notes of 24 November 02:18Z: one short jet off the canister, 0.5 to 1 m down the wind, growing
  into a dense rolling billow there, 3 to 5 m across within 2 s and 10 to 15 m by 10 s; then a plume leaning up down
  the wind, its middle climbing about 1.3 m in 10 m, widening, with torn wisps off its top; solid for its first 15 m,
  then thinning steadily, 0.4 thick at 40 m and 0.1 to 0.2 at 50 to 60 m; a sunlit grey-white top, 212-232 in the
  cloud picture, a cool grey underside and lee, 164-185; a soft shadow on the ground under its dense part, laid away
  from the sun; laid until its last 15 s, then thinning) for 60 to 90 s. A man on the
  ground 2.5 s or more after it lands is hit half as often (the test: 0.078 against 0.175 in the open). Its own dice
  (`Wreck::Seed` with its own salt): with the switch off every crew goes to the last number as before. Without the
  bail clips the grenade still lands; he is unseen in her until he climbs out.

None of it changes who lives and dies in the battle's numbers (the fates table's rule: looks only). Its dice are its
own, by the wreck's seed, so the same battle bails out the same way each time. The log names each crew ("Land battle:
Sherman's crew (tank.brew_up): 3 out: 2 clear, 1 caught by the blast"). `Tools/land_bail/run.sh` plays every land way
with crew_out for 300 seeds and checks each man's pose every 0.1 s for 180 s, then again with the bail clips, then with
the clips and smoke (`LAND_BAIL_SMOKE=1`: thrown only where it may be, by her first man, toward her enemy, about six
crews in ten, half the hits under it); `LAND_BREW_WAIT=5.5,10` runs the kit's
own test with the wait.

The art director's Mac shots, with `-TheaterLandBailOut` against without: a brew-up from the hit to 15 s after (the
crew getting out, the blast), from 30 m and from 150 m; an engine fire's whole crew out; and with
`-TheaterLandFilmFire` as well. Also the frame rate in a battle with eight kills or more, against the 45 fps gate.
With `-TheaterLandBailSmoke` too: a tank knocked out with her enemy close (her commander's throw from 30 m, the screen
coming up between her and the enemy from 150 m, with some wind so the plume shows), and the frame rate with four or more
screens up. The plume's own picture: `Tools/land_look/smoke_screen_picture.cpp` (the kit's three kinds from 120 m across
the wind; the grenade's also at 30 s from 40 m square across its plume at a 1.5 m eye and from the game's camera 25 m
up, with its shadow alone, and measured down its plume: how solid, how high, its top's and underside's own colour;
`/mnt/project-files/battle-fx/bail-out/smoke-plume-r2.jpg`, round 2, against round 1's `smoke-plume-r1.jpg`).
For the Mac shot of round 2: the plume from the side at 40 m and from 150 m, a still at 2 s close to (the jet), and
the frame rate with four screens up (each now at most about 150 pieces, round 1's 58).

## Shell holes each their own (release 2, off until passed)

Brian, 23 November: "make the explosion craters different varations so its not always the same". Before this, every
round of a kind left the same hole: every 105's 2.8 m across and 0.7 m deep, round, its soil thrown evenly all round,
only its ragged outline its own. `-TheaterLandCraterVariety` (`LandCrater::Field::bVariety`) gives each round its own:

- **Its size:** a quarter bigger or smaller, and one in ten a big one (up to half as big again); its depth, rim and
  clods with it.
- **Its fuze:** most burst at once on the ground. One in eight is a delay that bursts under the ground: deeper,
  narrower, a high rim, big clods, little soot. One in ten grazes: a shallow, wide, sooty scoop, longer.
- **Its slant, along the way its round came** (the scene passes `LandPlay::Effect::Way`). A tank's or anti-tank gun's
  high explosive comes in nearly flat and leaves a long hole (1.35 to 1.8 times as long as wide), most of its soil
  thrown forward and out in two wings to either side, little behind. A field gun's or howitzer's shell leaves a
  slightly oval hole with some wings. A mortar bomb falls nearly straight down and leaves a round one. A round of no
  known way (a blast in the record with no round) is turned by its own dice.
- **The burst's soot:** four to nine soft-sided rays of blackened soil over the thrown soil, each its own width,
  length and darkness, the longest thrown downrange by a tank's slanting burst. The thin edge itself always stays the
  ground's own colour (soot on it drew a grey halo with straight sides on the snow).
- **The subsoil:** a deep hole throws lighter soil from under the topsoil onto its crest in patches. Normandy's
  yellow-brown clay, the steppe's loess, paler sand, a town's plaster dust.
- **The ground:** mud slumps wide and low with its soot smothered, sand sprays wide and shallow, and on snow the dark
  earth reaches further.
- **Overlapping holes:** a new hole's thrown soil stops at an older hole's rim and steps down into it, rather than
  lying across the older bowl as a sheet. Where it reaches over an older hole's thrown soil it lies on it, in that
  soil's colour and light at its thin edge (`Field::SoilAt`), so the newer edge never shows as a white sheet over
  the older soil.

The art director's round 1 (23 November) mended four things, all inside the switch:

- **No straight edges:** each hole has 40% more points round it, and the thrown soil's outer band is broken from side
  to side, thick where a ray or wing threw it and thinning early elsewhere, so the dark soil runs out in tongues of
  uneven length.
- **The spray:** specks of soil and torn turf thrown past the thick soil and beyond the thin edge, thinning as they
  go, most along the rays and into the wings; on snow and sand some are blackened. They are laid as clods are, so a
  body lies on them.
- **A smooth bowl:** the bowl's points a quarter as jittered and its tone smoothed from side to side, so the sand's
  bowls no longer fan into pie slices.
- **Puddles in mud:** about half the deep holes in mud hold water, a flat disc at a quarter to a half of the bowl's
  depth, the grey of the sky in its middle, darker at its edge, under a dark wet ring on the bowl's side. A man wades
  in it: `TopAt` reads the bowl under it, not the water.

Cost: a varied hole has about twice the points and 1.7 times the triangles (a 105's about 580 points and 750
triangles, a mortar's 360 and 490). Laying 500 holes takes half a second in the cloud with the switch on, against a
fifth of a second off, so the Mac's speed gate must check it.

It is all drawn from each hole's own dice, so the same battle lays the same holes. It applies to scarred ground's old
holes too. With the switch off every hole is laid as before to the byte: checked on 500 holes of every name and
ground, including old ones, against the code before it. `Tools/land_crater/run.sh` checks it (each hole its own
size, the long ones along their rounds' way, a body still lies on every hole's face but a puddle's water, no hole
through the world) and
paints a barrage's holes on earth, sand, snow, mud and black earth, switch off and on, from a man's height and from
above.

The art director's Mac shots, with `-TheaterLandCraterVariety` against without: a barrage's field from 40 m up and
from a man's height, on the bocage and on snow or sand. A field after tanks have shot high explosive at a hedge line
should show the long holes pointing back at the guns.

## Each gun its own flash (release 2, off until passed)

Brian, 23 November: "how does the gun fire look when they shoot? can we make the mussel flash realistic?" Behind
`-TheaterLandGunFlash`. Off: every gun draws its row's flash as before (the kit's preview draws the same frames, byte
for byte, with the switch off).

- **Who has which.** `LandPlay::World::GunLook` (set in `ALandBattle::Open` only when on) names each shot's look by the
  shooter: `star`, the German MG 42 or MG 34 (squad and heavy); `cone`, a flash hider's narrow cone (the Bren, the DP,
  the BAR, the Type 96 and 99); `open`, a long tongue (heavy guns: the M1919, the Maxim, the Vickers; the Italian and
  French squad guns); `tank`, a tank's machine guns; `smg`, a squad leader drawn with a sub-machine gun (his figure's
  `_smg` or `_nco` file): a burst of 4 (the PPSh 5) at 9 to 15 a second, no tracer, as his clip's cue sounds it;
  `rifle`, every other rifleman; `brake`, a main gun with a muzzle brake by her model (Tiger, Panther, Panzer IV, StuG,
  ZiS-3, SU-76, Sexton). The kit is told "mg:star": the row `mg` as ever, with the look's pieces laid after it.
- **The looks** are `muzzle.look.<look>` in `Content/Effects/battle-fx-guns.json` (`tools/battle-fx/fx-guns.json`),
  laid over the recipe for that battle only. Each can narrow or lengthen the flame, add a star of spikes round the bore,
  a brake's two side jets and the dust they blow aside, a pale haze that hangs, a kick of the ground's dust under a
  muzzle lower than `prone_under_m`, and a small lamp on the firer's face at dusk and night (`bNightOnly`).
- **The art director's round 1** (23 November, 17:06Z; the cone, tongue, SMG, tank MG and brake passed): the MG 42's
  star is 4 to 6 short straight tapered spikes, yellow-white, leaning 70 degrees off the bore, rolled anew each round
  (one star a round: a burst's first round is the Shots', so `Muzzle` drops its own); by day a look is drawn at `day_k`
  of its size and `day_pale` toward pale yellow-white (`View.SmokeLight`); the rifle's haze is a thin puff gone in 1 to
  2 s; the prone kick is tan on dry ground (half to full by the ground's dust share), white on snow, none on mud, water
  or marsh; rifles and sub-machine guns draw no tracer (`tracer 0`); machine-gun tracer is a short travelling streak even
  close in (`streak 1`); the face lamp is 2 to 3 m and lit on the flash's own frame alone (`Flash::bOneFrame`). Round 2
  (17:19Z) PASSED, with two carries made: the kick off snow a bright bluish-white (`snow_kick`), and each spike white at
  its root running out to a yellow tip (`Bit::Hub`), so the star sits with the flash at dusk.
- **Cost.** A shot lays at most 2 haze puffs, 3 dust puffs, 6 spikes or 4 jets, 10 brake dust puffs and one lamp; a
  burst re-kicks its dust every fourth round and lights its lamp each round. The lamps compete for the kit's two.
- **Pictures**: `/mnt/project-files/battle-fx/muzzle/` (cloud preview: `FX_MG_VIEW=flash`, `FX_MG_GUN=mg:star`,
  `FX_DIM` for dusk, `FX_FLASH_AHEAD=2.2 FX_FLASH_OFF=2.2` for the star from ahead, `FX_GUN_ROW=at_gun:brake`).

## Shell and tank hits take limbs off (release 2, off until passed)

Brian, 22 November: "make more graphic animations if a guy gets shot or blown up", "with limbs coming off"; and the
effects brief of 23 November: shell and tank hits throw men and take limbs off. Behind `-TheaterLandGore`. Off: no twin
is read and every man falls as before (the cloud test plays its battle with the twins added and the switch off, fall
for fall and skin for skin the same as with none).

- **The twins.** The soldiers thread's gore twins, `soldier_<ARMY>_gore.json` beside each file in
  `Content/Models/Soldiers`: the same bones, the skin split at the eight cuts (shoulder, elbow, hip, knee, left and
  right) with the stump and limb caps hidden inside the body till a limb goes, the paints wound, bone and blood, and the
  deaths thread's gore falls (`deaths_gore[stance][kind]`: `die_blast_a` to `d`, thrown with a limb gone;
  `die_arm_off_a`, `b`; `die_leg_off_a`, `b`), whose cut bone flies and lands on its own move track. `ALandBattle` reads
  the twin of each file it read, when the switch is on, and gives it to the crowd (`LandMen::Crowd::AddGore`), which keeps
  the twin's mesh, faces and helmets with the file's own clips and sockets, the gore falls after them, so a man swapped
  to it mid-battle keeps every clip number he had. A twin with no gore falls is passed over, and said in the log.
- **Who.** A man killed by a field gun's or a howitzer's shell, a tank's gun, a mortar's bomb or a grenade
  (`KillWeapon`), by how far he stood from its burst (the round's x, y, as the drill writes a shell's). The art
  director's round 1 (23 Nov 17:52Z): rarer and stronger, about 1 shell kill in 8 to 10. Within 2 m of the burst 0.55
  of them for a gun's shell, 0.35 for a tank's or a mortar's, 0.25 for a grenade's, falling away by e every 1.5 m past
  2 m to none past 8 m (gun), 6 m (tank, mortar) or 2 m (grenade). The rules lay most bursts on the man they kill (8 in
  10 shell kills read 0 m), which says nothing of how close the shell truly fell: such a man 0.2 (gun) or 0.12. A man
  lying or in cover half that. Measured on the rules' Normandy and Prokhorovka, shell blasts off and on: 1 shell kill
  in 7.6 to 11. His own dice, so the same battle gores the same men. On the frame he is struck the crowd swaps him to
  his file's twin (`Impl::ToTwin`: his mesh at each level, his face, no bank), then picks his fall from the twin's gore
  falls. Which side: where the burst was 1.5 m or more off, by its side against his facing (front, back, side, so a
  fall of kind `back` throws him away from a burst in front of him); nearer, by the gun's. The big throw (`thrown`,
  `die_blast_*`) is a field gun's or a howitzer's; a tank's, a mortar's or a grenade's mostly knocks him down an arm or
  a leg short. A replay from the start gives him his own file back.
- **Whole joints stay whole** (the art director's PASS, 18:34Z: the caps hidden till his limb goes). The twin's skin is
  split hard at the eight cuts, which opens a crack of up to 20 cm at a bent shoulder or hip with its red caps inside.
  Its `gore.joined` lists, per level, the points at each joint with the man's own weights and the cuts they sit at:
  `Crowd::Skin` skins those so while none of their cuts is off (a cut is off once its bone has left the joint its
  parent holds by over a centimetre), the twin's hard weights after. The twin's `gore.cuts` are read as
  `Soldier::Cuts` (each cut's bone, its caps' points per level), `gore.joined` as `Soldier::Joins`; a joined list with a
  point or a bone out of range is dropped for that level, and he falls cracked as before.
- **The scene.** His mesh's number changes at the swap (`Crowd::MeshNo`); `DrawMen` then makes his triangles and
  colours again, as for a new level (`FManDrawn::MeshNo`). His fall's `limb_off` events come with the rest of the men's
  (`Crowd::Events`, at the stump's socket); no blood is drawn from them yet (Brian has not ruled on blood).
- **Cost.** A gored man is posed and skinned as any falling man; at rest he is frozen as any body. The twins are read
  only with the switch on (about 6 MB of json each).
- **Test**: `Tools/land_gore/run.sh [twins folder] [out folder]`: the off, on, same and replay checks above (with
  rifle never, none past reach, nearer more often, the man a burst was laid on less often than one within 2 m, the
  burst's side), no whole joint of a gored man open on its caps (his twin's seam copies within 2 mm of the points they
  copy, skinned; without the joined skin 225 mm), a note for each limb resting under 1 m from its joint or flying over
  3 m high (the art director's asks; the clips' own to mend), and `gore_falls.ppm`, six gored men from the hit to rest,
  no army's clip twice. Pictures: `/mnt/project-files/battle-fx/gore/` (r3: the joined skin, fresh twins 29ad7cfe).
- **Owed.** The deaths thread's re-runs of the falls (USA and GER `die_leg_off_a`, GER `die_arm_off_b` rest under 1 m
  from the joint; USA and GER `die_blast_a` throw a limb 3.6 to 3.7 m up), gore falls in every army's twin, and the Mac
  shots behind the switch.

## The fighters on call over the field (release 2, off until passed)

The effects brief of 23 November (Brian's ask through the coordinator): the ground-attack fighters on call "fired on"
from the ground, "rarely shot down", and they "crash". Behind `-TheaterLandAirSupport`. Off: nothing of them is read
past the feed or drawn (the feed keeps the notes in `Recording::Air` either way; nothing else reads them).

- **The rules decide.** Which side calls a strike and when, each leg's run down a strip (`land:air-in`, then
  `land:strafe`: from, to, width, its time in battle seconds), its bombs (`land:bomb-drop`: each burst a shell of
  calibre bomb, drawn by the scene as ever) and the planes it cost (`air:lost`: explode, burn or spin) as the run
  ends. `LandFeed::AirNote` reads the four; nothing here changes any of them.
- **The plan** (`LandAir.h/.cpp`, plain C++). A leg's planes come in one behind another down the run's line, high, push
  over into a dive aimed at their lane's middle, their guns opening at the machine's range from the lane's start and
  walking it to its end, pull out, climb away and break left and right in turn, each 6 to 12 s behind the one before
  (each dive its own beat), at a real machine's speed along its way (105 to 140 m/s). The shapes are the art director's
  (23 November, 19:08Z): a fighter from 700 to 800 m in a 25 degree dive, guns from 900 m to 250 m slant, out at 45 m;
  a Stuka from 1,800 m rolled into a 75 degree dive, its two guns a short burst (1,100 to 700 m), out at 300 m; an Il-2
  from 160 m at 8 degrees, guns from 700 to 200 m, out at 15 m. A lane starts at least 150 m past its own side's front
  (the rules' strip is unchanged; only where the rounds are seen to strike moves on). The machine by nation and year
  (`MachineOf`): Mustang or Warhawk, Stuka (to 1943) or Fw 190 (1944 on), Spitfire or Hurricane, the Il-2, Zero, else a plain fighter in the
  nation's paint. The ground's guns: the other side's four blocks nearest the run, two guns a block, ragged bursts,
  aimed short of the lead so that the tracers fall behind. A plane lost is the last of its leg not yet lost: blown apart
  in its pass, or hit climbing away and going down on fire in a long shallow dive, or spinning in (about 3 s from 150 m);
  it burns where it struck the rest of the battle.
- **The scene** (`LandBattleAir.cpp`, `ALandBattle::FlyOver`, each frame after `PlayEffects`). Each plane a mesh of its
  model's far form (`FMiniatureMesh`; the plain fighter sized to its span where there is none), on `SteelInk`. Its wing
  guns on the kit's `AirGun` (flash, smoke, the nation's tracer, carried with the plane), and `Shots`' dust spurts over
  the last 6 m where the rounds go in (no tracer of its own seen). The ground's guns on `AirGun` from where they stand
  (the defender's tracer; `hmg` for American guns, `rifle` for the rest). A hit: `AirHit`, `AirFall` (explode, burn,
  spin) and a `fire` or `smoke` trail moved with it; into the ground: `FuelBall` 18 m, `ShellOfSize` `gun75` (an
  exploded one's pieces `mortar`) and a fire (`Burn` 5 m, 2.5 for the pieces) for the rest of the battle. Ids: the
  planes' guns 90000 on, the ground's 95000 on (`GunIdOf`, `GunnerIdOf`), a plane's fall, trail and fire `1 << 25` on.
  A live battle's notes plan it again as they come (the legs flown the same); a loss whose note comes after its plane
  was hit begins where it is. A replay from the start tells it all again.
- **Downed pilots** (`-TheaterLandChutes`, with `-TheaterLandAirSupport`; release 2, off until the art director and a
  45 fps reading pass; Brian, 23 November 20:09Z). The rules say whether a pilot got out (air:lost's chutes: half the
  burning and spinning planes', never a plane blown apart's); `LandAir::Sky::Chutes` says how. He goes over the side 1
  to 2 s after the hit (out of a spin 0.5 to 0.9 s), never in her last 0.8 s, and where she is under 60 m by then at the
  highest she is after the hit; falls free 1 to 1.6 s (pulling at once under 70 m), his way off her dying fast; his
  canopy blossoms over 1 s and he comes down at 5 to 6.5 m/s, drifting with the battle's wind, swinging under it (14
  degrees, 3.2 s a swing, dying away). In the air he and his canopy are one small mesh (`LandBattleAir.cpp`,
  `ChuteShape`: a 7 m canopy, eight lines, a man in his harness 4.1 m under it, its pivot the canopy so he swings),
  built again only as it blossoms. Down, he goes flat as he lands (`go_prone`), lies 1.5 to 3.5 s, gets up
  (`stand_up`, turning to his way) and runs 20 to 40 m from the nearest enemy block at 4.2 to 5 m/s (`run`), and goes to
  ground there (`go_prone`, held): a man of the crowd in his army's infantry clips with no rifle (`RiderFileOf`). His
  canopy falls in over 2.5 s and lies flat 4 m downwind of where he landed, its lines back to there (`HeapShape`).
  Its own dice: the planes fly as before.
- **Not yet.** The engines' and guns' sound (the sound director's), the bombs seen falling, light flak's puffs (the kit
  has only its heavy row).
- **Test**: `Tools/land_air/run.sh [out folder]` (`LAND_AIR_OUT`): the feed, the machines, the plan, the flight (drawn
  from before the run, at a fighter's speed, never under 5 m over the ground, smooth, the Stuka steep and the Il-2 low
  while firing, the art director's dive angles, bottoms and gun ranges), the guns (each lane from the strip's start to
  its end, aimed on the ground, each plane 6 to 12 s after the one before, a lane 150 m past its own side's front), the losses (each falls as its note says, burns where it struck, no other falls), the ground's fire (only the
  other side's blocks, each stream ending in the air near a plane), the same (built twice, grown note by note, rounds
  the same however the frames fall) and no NaN; `air_paths.png`, three strikes side-on and from above
  (`paths.py`); the chutes (a pilot out of the burning and the spinning plane, none out of the one blown apart; out
  after the hit and before she strikes, his canopy full well up, down at 5 to 6.5 m/s, never under the ground; flat,
  up, running from the enemy, flat; his canopy 4 m downwind; the same twice), `chutes.py` drawing them side-on and
  down from above. Pictures: `/mnt/project-files/battle-fx/air-support/` (`chutes_r1.png`).
- **Owed.** The Mac shots behind the switch, the art director's look, and the 45 fps gate with a strike overhead.
  With `-TheaterLandChutes`: a pilot under his canopy from 100 m and from 300 m, his landing and run, and his canopy
  lying on the field.

## The support teams' machine guns and mortars (release 2, off until passed)

Brian, 25 November 2026: "Where are the machine gun teams and mortar teams", "There should be machine gun teams that
lay down suppressing fire, such as the German MG 42 and the US machine gun". The rules already field them (a block of
team `mg` or `mor`: three key men of role `hmg` or `mortar`, each with his loaders); until now each was drawn as a
rifleman. On trial `-TheaterLandTrial=teamguns` (off; `-TheaterLandNoTeamGuns` off whatever the word says, for a paired
run) each key man has his weapon:

- **The models** (`Content/Models/Battle`, made by `tools/battle-models/land/support_guns.py` on the tank modeller's
  kit): `HmgMG42`, `HmgMG34`, `HmgM1919`, `HmgM1917`, `HmgVickers`, `HmgMaxim`, `HmgType92`, `HmgHotchkiss`,
  `HmgBreda37`; `MortarGER`, `MortarUSA`, `MortarUK`, `MortarSOV`, `MortarJAP`, `MortarITA`, `MortarFRA`. A machine
  gun's parts are `mount` (the tripod or wheeled carriage, its origin on the ground under the pin), `cradle` (swings)
  and `gun` (tips); a mortar's `mortar` and `tube`. Sockets: `seat` (the key man's own root), `seat.loader`, `muzzle`,
  `feed`, the grips, `box` or `rounds`, the Maxim's `pull`. The picture sheet: `LAND_KIT_SUPPORT=1` to the painter
  (`Tools/land_kit/land_picture.cpp`).
- **Which file, where it stands** (`Battle/LandTeamGun`, plain C++): `FileFor` by the block's army, the role and the
  month (the MG 34 until May 1942, the MG 42 after; an army with none of its own takes the nearest shape). `Gun::Step`
  each frame: set up with the model's seat on the key man where the crowd draws him, swung and laid on his aim (45
  and 20 degrees a second, stopped at the model's limits; a mortar turned whole); on his right shoulder while the team
  moves (the Maxim rolled behind him by her trail); left where she stood when he falls; not drawn while he is hidden,
  riding or mounted.
- **In the scene** (`ALandBattle::MakeTeamGuns`, after the men are posed and before the fire is stepped): two instanced
  meshes a file, the mount and what turns (`LandParts::BodiesMesh`: the fine level, the plain one from 90 m; gone past
  460 m), placed by `LandParts::BankedAt` as a man is. `PlayWorld.Muzzle` gives a team's rounds her own muzzle and
  tells her of each burst (she shakes 6 mm back along her bore a round); `PlayWorld.GunLook` gives her own burst
  (`LandTeamGun::BurstOf`: the MG 42's 18 rounds in 0.9 s, where a section's gun fires 8), which `LandPlay` hands to
  the sound as well.
- **The gunner** (`LandMen::Crowd::Serve`): his rifle laid down, lying behind a low gun (`fire_prone`, whose hands
  reach her grips) or kneeling at a high one or at his tube (`fire_kneel`), until his file brings `hmg_fire_prone`,
  `hmg_fire_sit` or `mortar_sight`, which are used by name as soon as it has them.
- **Checked in the cloud**: `Tools/land_kit/run.sh` (the section THE SUPPORT TEAMS' WEAPONS: every army's file reads;
  seat, swing, lay, limits, bursts, carried, rolled, left, not drawn; `LAND_KIT_ONLY_TEAMGUNS=1` runs it alone) and
  `Tools/land_show/run.sh` (the support teams' key men). The engine's part (`MakeTeamGuns`, `BodiesMesh`) has not been
  compiled or seen: the Mac's first build and stills are owed.
- **Not in this**: the loaders are where the drill puts them (the model's `seat.loader` is not read yet), no belt
  runs through the gun, a mortar's bomb is not seen dropped. Suppression itself is the rules' (the smarter-fighting
  thread's), not the scene's.

**The sections' own machine gunners** (trial word `gunners`, off): where the soldiers thread has made an army's
section gunner with his gun, `soldier_<ARMY>_lmg.json` (the German MG 42) and the earlier gun's man, `_lmg34` (his
MG 34), the scene reads those files (`MakeMen`'s list) and `LandMen` draws a section's `mg` man from them: `_lmg34`
before July 1942 where it is made, else `_lmg`, else the American's `_bar` as before. Off, the files are not read and he
is the rifleman he was. The men's test plays one of each date (`Tools/land_show`).

Two exposure words came with it (`LandScape::AirOf`): `desertstop` takes the desert's fields half a stop down and
`islandstop` an island's a third, for sand that washed pale on the Mac. Both off until a before and after still.

## Packaging

Stage `Content/Data` (LandBattles, Battle, Sound) and `Content/Models` (Soldiers, Battle) with the game, as
`Content/Effects` is staged. Then `Sounds/Real/credits.json` for the credits page.

## On the first build, look at

1. **Facing.** The ground, the men and the vehicles face up and out (no inside-out hedges, no men seen through). The
   field's x is east and y north; `World()` swaps them into Unreal's frame, which keeps the triangles' winding right.
   The vehicles' and props' meshes (LandKit's, model axes straight into Unreal's) are turned as they are handed over
   (`LandParts::KitFront`, FromBody, PropMesh, LandCrowd's PieceMesh): the files wind them the other way round, and
   unturned every tank was drawn inside out (Brian, 23 November: the Tiger's commander seen whole through her turret).
2. **A tank's turret.** If a turret aims away from what it fires at, the sign of `TurretDeg` is wrong in
   `DrawArmour`.
3. **The falls.** A man hit should fall the way he was facing or struck, and lie still from then on, flat on the slope.
4. **The sound.** The crack of a round passing near, the shots by army, the tanks' engines moving with them, the
   blasts arriving after their flash by the distance.
5. **The room.** The effects kit's busiest moment against its room of 3,000 pieces, and the log's slowest frame.
6. **The holes.** Picture 07 and any shot after the guns open: the holes should sit on the ground, not float or sink.
   They are laid 3 cm over the ground's height, and a far tile's coarser mesh may differ from that by more. Their
   browns should read as earth beside the field's grass under the scene's light. Look also at picture 03 at the same
   moment as before: no dark disc, the 105 mm smoke beside the camera thinning as it passes the lens.
