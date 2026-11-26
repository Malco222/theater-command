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
| `LandHulk.h/.cpp` | Old wrecks (release 2, `-TheaterLandOldWrecks`): where the burnt-out armour of earlier battles lies on the field and how each looks, from the game's own vehicle models. Plain C++; `Tools/land_hulk/run.sh` tests it. |
| `LandAir.h/.cpp` | The fighters on call (release 2, `-TheaterLandAirSupport`): each leg's planes down their run, the ground's guns and the lost plane's fall, from the rules' air notes. Plain C++; drawn by `LandBattleAir.cpp`. |
| `LandWeather.h/.cpp` | Rain and snow seen falling (release 2, `-TheaterLandFalling`): what falls over a battle and how hard, from the campaign's weather word and the day's sky (no new draw), and the day's changes for it (haze, light, wet ground, sheen, rain heard). Plain C++; numbers in `Content/Effects/land-weather.json`. |
| `LandBattleWeather.cpp` | `ALandBattle`'s side of it: reads the switches and the recording's sky, lays the wet inks and the rain bed, makes and ticks the drops. |
| `../Effects/WeatherFall.h/.cpp`, `WeatherFallCore.h/.cpp`, `WeatherFallChip.inl` | The falling drops: one mesh per layer round the eye, made once, every drop placed on the graphics card each frame by `M_LandFall` (`Tools/build_content.py`, `build_land_fall`), whose arithmetic is `WeatherFallChip.inl`, shared word for word with the cloud test. |
| `../../../Tools/land_weather/` | The cloud test of what falls (the pick, the recipe file, each layer's drops, wind, fades, counts in a picture), built with both compilers' strict flags; `weather_fall_picture` lays the drops over `Tools/land_field`'s pictures (`LAND_FIELD_FALL=rain,0.8`). |
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
     They gather along the field's old lines (hedges, lanes, the village's edge): a line stepped finer than 4 m, as
     a lane's is, has its steps joined, and the lines take no more than about a hole to 3 m of their length. A field
     thin of lines (one lane across open steppe) takes knots along its lane with clear stretches between, about a
     third of the holes, and the rest in the open and at the junctions.
   - `-TheaterLandOldRuts=N`, `-TheaterLandOldRutMonths=M`, `-TheaterLandNoOldRuts` (release 2, mud memory): the ruts
     of battles fought here in mud before, as a recording's root `"oldRuts": {"count", "months"}` gives them: dried
     pale under three months, then a lusher green line fading; puddled with `-TheaterLandWeatherWord=mud`.
   - `-TheaterLandOldWrecks=N`, `-TheaterLandOldWreckMonths=M`, `-TheaterLandNoOldWrecks` (release 2, old wrecks):
     the burnt-out armour of battles fought here before (N laid, at most 20, burnt M months ago, the nations the
     battle's two sides'), standing in for a recording's root `"oldWrecks": {"count", "months", "nats", "per"}`. See
     "Old wrecks" below.
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

## Rain and snow seen falling (release 2, off until passed)

Brian, 25 November: "better overall graphics". This is the look bible's G10. `-TheaterLandFalling` turns it on (the
ship list's `falling` row), and `-TheaterLandFallingSnow` with it lets snow fall too (its own row, `fallingsnow`, so
rain can ship while snow waits: the art director's round 2). Both are OFF until the Mac passes them; the art director
passed both to the Mac on 25 November, 12:21Z (`falling-weather-r2-review.md`). There is no new weather draw: what falls is
read from the battle's own weather, the campaign's word over the place and the day's sky as the ticket drew it
(`Battle/LandWeather`, numbers in `Content/Effects/land-weather.json`'s `pick`).

- `rain` (a deck of 0.75 to 0.95): it rains, from 0.45 under the lightest deck to a downpour at 0.95. The jungle's,
  the islands' and rice country's rain is heavier; the desert's and the dry steppe's lighter.
- `snow` (0.8 to 1): the snow lies; it falls only under a deck of 0.88 or more, about half the snow months' battles,
  never harder than `pick.snow.most` (1; lower it if the Mac finds the field reads only under a lighter snow), and
  only with `-TheaterLandFallingSnow`: without it none falls, the day as drawn.
- `mud` (0.6 to 0.8): a drizzle under the heaviest of its decks (0.76 and more).
- `clear` or none: nothing falls. Over snow lying (0.3 and more) what falls is snow.

What the battle does for it (all looks; nothing in any fight reads it):

- The drops: three layers of streaks (or flakes) in boxes of 8, 22 and 64 m about the eye, each a mesh made once,
  moved, wrapped and turned to the eye on the graphics chip by `M_LandFall` (`Effects/WeatherFall`,
  `Effects/WeatherFallChip.inl`). The game's thread sends a few numbers a frame. A drop stays put in the world while
  it is near; none shows within 0.6 m of the lens or at a box's edge; none is drawn under a pixel and a quarter wide.
  The drops hang still while the battle is held, and fall through the night's own shots.
- The sky: rain and snow always fall from a closed sky (a forced rain on a fair day too), the deck a darker grey lid
  under them (its brightness times 1 - 0.4 x strength in rain, 1 - 0.45 x in snow, so the grey lid sits 15 or more
  under the snow lying). The art director's R1 and S2.
- The haze: thicker (1 + 1.3 x strength squared for rain, so a light rain keeps the field's depth and a downpour closes
  it; 1 + 0.6 x for snow, so the far village still reads in heavy snow over the winter's own white haze) and drawn,
  the square root of the strength's share of the way, toward a darker rain grey (0.2, 0.21, 0.23 linear) or the snow's
  grey lid itself (0.38, 0.40, 0.45: the haze is laid over the sky at the horizon, so it must not be lighter than the
  lid, or the lid and the snow meet as one pane; round 2). The drops take the haze's colour, a little brighter (snow's
  flakes three times, light discs about L225 to 240 against the lid, a wood or a hull).
- The light: the day's lamps 0.88 to 1 of the day's (rain); snow leaves them as they were, so the snow lying stays white.
- The wet: under rain the open earth is the thaw's mud for the effects (the holes' soil, torn ruts, thrown mud, a deep
  hole's water), and LandField's `Field::Rain` soaks the ground's colour: deeper, never greyer (the paler and drier the
  darker: a lane's dust to about 55% of dry, green grass hardly darker), the corn, the stubble and the hay to about three
  quarters, their hue kept (red over green up by no more than 0.05), the far ground the same, a bright gold ground
  darkened as little as the corn standing on it, so the general's view and the eye's agree (round 2's A: from above, wet
  corn had turned burnt orange). Water stands in pools on bare ground only (lanes, gateways, ploughland, holes; none in
  grass or standing corn, where soft pools read as bruises until the ground's material carries a wet mask with a firm
  edge: round 2's B) and along the lanes' ruts, the overcast's grey it mirrors. The ground, the hedges' and the clutter's paint and the steel shine wet (Roughness and
  Specular: grass and leaves no smoother than 0.75, steel 0.5).
- The streaks: a light rain's shorter and fainter than a downpour's: its length times 0.3 + 0.7 x strength, its alpha
  times the strength to the 1.5th power (strength_alpha 0, strength_alpha_curve 1.5; the layers' alphas 0.45, 0.35,
  0.22), so a light rain's streaks lift a third less off the ground than a downpour's (round 2's C). The three layers
  lean about 5 degrees apart in the same wind (wind_share 1.25, 1, 0.75). A snow's flakes keep their alpha as it
  lightens, only fewer (snow_strength_alpha 0.7); near flakes 3 cm, middle 2.5 cm (the art director's S3).
- The sound: the mix's `beds.place.rain` (`rain_loop`), over the place's air, softer the lighter it falls.

Switches for the Mac's stills: `-TheaterLandFall=rain,0.8`, `=snow,0.5`, `=0.6` (snow where it lies, else rain) or
`=none`, whatever the day (it turns the look on too, falling snow's switch or not). `-TheaterFxSet=fall.rain.far.density=0.1` sets any recipe number
at launch. Build the material once: `TC_REBUILD=M_LandFall` (it is made by `build_content.sh` when missing). Without it
nothing falls and the log says so; the haze, the light and the wet ground are laid anyway.

The cloud test: `Tools/land_weather/run.sh` (the pick, the recipe file against the code, and the chip's own arithmetic:
periods, boxes, world anchoring, fades, strength, width; how many drops a picture holds and the pixels they cover). The
field painter paints it: `LAND_FIELD_FALL="rain,0.85"` (or `word` for the recording's own weather) with
`LAND_FIELD_PICTURES`; `LAND_FIELD_FALL_HIDE=1` paints the same day without its drops (to measure what they add),
`LAND_FIELD_FALL_SET="rain.near.alpha=0.4,..."` and `LAND_FIELD_DAY_SET="day.snow.sky_dimmer=0.5,..."` try other numbers
as the json would.

The art director's eight deciding Mac shots (round 2, `/mnt/project-files/art-director/reviews/falling-weather-r2-review.md`;
rain's 1 to 5 decide the `falling` row, snow's 6 to 8 the `fallingsnow` row):

1. Dry, `rain,0.45` and `rain,0.9` from one eye and one hour: R1's and R2's ratios to dry hold under the real sky.
2. A tank and a section of men at 300 m and 600 m in the downpour: 25 or more against their ground, and 12 or more.
3. The general's high view over corn, dry and in the downpour: wet corn 70% to 85% of dry, red over green up 0.05 at most.
4. A slow pan at both strengths: no streak held still on the screen, none repeating as a tile.
5. The frame rate in a downpour against the 45 fps gate. If it fails, the far streak layer goes first.
6. `snow,0.8`: the sky's lower third 15 or more under the near and middle snow.
7. A tank and a section of men at 300 m and 600 m at `snow,0.8`: 35 or more, and 18 or more.
8. Flakes against a wood or a hull: near ones 5 to 9 px (of a 675 px picture), L225 to 240, drifting, never a dash.

If rain passes and snow does not, rain ships (`falling` on) and falling snow stays off (`fallingsnow` off), or ships
capped at the strength where 6 and 7 hold (`pick.snow.most`).

Round 1's fuller list, each with and without the switch (`falling-weather-r1-review.md`):

- One eye and one hour, three frames: dry, `-TheaterLandFall=rain,0.45`, `rain,0.9` (the ratios to dry).
- Normandy's low eye in the corn, and a hedge close by in rain (the streaks against the dark leaves).
- A tank and a section of men at 300 m and at 600 m, in the downpour and in `snow,0.8` (Readable).
- The lane and its puddles at the general's slanting view, about 100 m.
- Five-second clips: a slow pan in rain at both strengths, and snow drifting (no streak held still on the screen, none
  repeating as a tile).
- One night frame in rain beside something burning.
- The steppe under `rain,0.7` with tanks through the mud, and a winter field under `snow,0.8`.
- The frame rate in a downpour against the 45 fps gate, with `stat gpu`. If it fails, the far streak layer goes first
  (`-TheaterFxSet=fall.rain.far.off=1`).

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
  in its own infantry figure, rifle hidden, his helmet on: her second note, "a man with a pack is a far smaller fault
  than no one getting out". Effects (R2): his pack and webbing are left off where his file marks them
  (`Crowd::CrewDress`, `AddRider`'s `bNoPack`; the soldiers' `pack_tris`, see *Men out of their machines without their
  packs* below); a file without `pack_tris` (the game's Content today) still draws him with it.
  The log says so once per army ("rifle hidden, pack off", or "pack on (the file marks none)"), until the
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
  smoke grenade thrown from her front, and the man on fire). A crewman of an army with no tankers' file without his
  pack: `Tools/land_show/pack_picture.sh <soldiers folder with pack_tris>` (`pack_off.png`, with the downed pilots).
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
own test with the wait. Its first run also makes the crowd's man of each crew (`Crowd::CrewDress`, `AddRider`, with
`Battle/LandMen`): USA and GER in the tankers' files as they are; SOV and UK in their own infantry file, helmet on,
at every level his file's triangles less the pack's run and nothing in its place, every kept point as the whole
man's; FIN none drawn (no file of its own). `LAND_BAIL_SOLDIERS` reads another soldiers folder (the soldiers thread's
files before they come into Content).

The art director's Mac shots, with `-TheaterLandBailOut` against without: a brew-up from the hit to 15 s after (the
crew getting out, the blast), from 30 m and from 150 m; an engine fire's whole crew out; and with
`-TheaterLandFilmFire` as well. Also the frame rate in a battle with eight kills or more, against the 45 fps gate.
With `-TheaterLandBailSmoke` too: a tank knocked out with her enemy close (her commander's throw from 30 m, the screen
coming up between her and the enemy from 150 m, with some wind so the plume shows), and the frame rate with four or more
screens up. The plume's own picture: `Tools/land_look/smoke_screen_picture.cpp` (the kit's three kinds from 120 m across
the wind; the grenade's also at 30 s from 40 m square across its plume at a 1.5 m eye and from the game's camera 25 m
up, with its shadow alone, and measured down its plume: how solid, how high, its top's and underside's own colour;
`/mnt/project-files/battle-fx/bail-out/smoke-plume-r2.jpg`, round 2, against round 1's `smoke-plume-r1.jpg`).
Round 2 passed to the Mac by the art director (24 November 12:51Z). Its Mac shots: the plume from 150 m with some wind,
the 40 m side view at a man's eye, one from the game camera with men running through it (the screen against figures),
a still at 2 s close to (the jet), and the frame rate with four screens up (each now at most about 150 pieces, round
1's 58). Its torn wisps are judged in the Mac clip: if its tail reads as cotton, a feathered edge is the first mend.

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
  deaths thread's gore falls (`deaths_gore[stance][kind]`: `die_blast_*`, thrown with a limb gone; `die_arm_off_*`;
  `die_leg_off_*`; in the twins of 24 November `a` to `f`, `a` to `m` and `a` to `e`, for standing and running,
  kneeling and in cover, and lying), whose cut bone flies and lands on its own move track. `ALandBattle` reads
  the twin of each file it read, when the switch is on, and gives it to the crowd (`LandMen::Crowd::AddGore`), which keeps
  the twin's mesh, faces and helmets with the file's own clips and sockets, the gore falls after them, so a man swapped
  to it mid-battle keeps every clip number he had. A twin with no gore falls is passed over, and said in the log.
- **Who.** A man killed by a field gun's or a howitzer's shell, a tank's gun, a mortar's bomb or a grenade
  (`KillWeapon`), by how far he stood from its burst (the round's x, y, as the drill writes a shell's). The art
  director's round 1 (23 Nov 17:52Z): rarer and stronger, about 1 shell kill in 8 to 10. Within 2 m of the burst 0.55 of
  them for a gun's shell, 0.35 for a tank's or a mortar's, 0.25 for a grenade's, falling away by e every 1.5 m past 2 m
  to none past 8 m (gun), 6 m (tank, mortar) or 2 m (grenade). The rules lay most bursts on the man they kill (8 in 10
  shell kills read 0 m), which says nothing of how close the shell truly fell: such a man 0.2 (gun) or 0.12. A man lying
  or in cover half that. None whose stance's gore falls hold none he may take (`GoreSide`, below): with the deaths
  thread's lying falls of 24 November a man lying is gored by any shell, as any man is; with twins whose only prone fall
  is the throw from under him (those before), only by a field gun's or a howitzer's shell bursting on him. He falls as
  any man, asked before his dice. Measured on the rules' Normandy and Prokhorovka, shell blasts off and on, while a man
  lying took any shell: 1 shell kill in 7.6 to 11. The recordings as they are (45 and 48 shell kills, all by a gun's, a
  tank's or a mortar's round, every one within 1 m of the burst): with the twins of 24 November 7 and 7 gored, 1 in 6.4
  and 1 in 6.9 (1 in 6.6 together), 8 of the 14 lying and killed by a mortar's or a tank's shell, all 14 struck within
  0.3 m of the burst (13 within 0.2 m; under, below). That count is these two seeded battles' dice, not the odds: the
  odds summed over the same 93 kills expect 4.3 and 5.6 gored (1 in 10.5 and 1 in 8.6; 9.8 together, 1 in 9.5, inside
  the art director's 1 in 8 to 10), and 14 is about 1.5 standard deviations above that. b470998e gores the same 14 men
  with these twins: the rise from 6 is the twins' lying falls, not the rule of the burst under him. With twins whose
  only lying fall is the throw, 2 and 4 (1 in 22.5 and 1 in 12; 2.8 and 5.1 expected). The odds are not changed here.
  His own dice, so the same battle gores the same men. On the frame he is struck the crowd swaps him to his file's twin
  (`Impl::ToTwin`: his mesh at each level, his face, no bank), then picks his fall from the twin's gore falls. A replay
  from the start gives him his own file back.
- **Which fall: away from the burst** (the art director, 23 Nov 17:52Z: "the limb flies AWAY from the blast", the kind
  by the burst against his facing, `die_blast_*` a field gun's or a howitzer's; the deaths thread, 20:1xZ: weigh only
  the kind matching the burst; and 24 Nov: the falls from under, and a man lying's from each side). The twin's
  `gore_from` (clip: side; read as `Soldier::GoreFrom`, a gore fall it does not name by the clip's own `blast_from`)
  says where each fall's burst was: front, behind, left, right, or under (a lower leg off, of kind `under`:
  `die_leg_off_d` standing or running, `die_leg_off_e` kneeling or in cover, and in the twins of 25 November (26 gore
  falls each) their mirrors, `die_leg_off_f` and `die_leg_off_g`; and the prone throw, `die_blast_d`). Where
  the burst was against him, and what he is thrown away from (`Impl::BlastOf`, read by `GoreSide` and kept as
  `ManInfo::GoreFrom`), where he stood and his heading read from his track at the round (`Dead` less his lag, as
  `GoreTwinFor` reads them, so the same whatever frame his fall is first played on): a burst 1 m or more from him, front
  or behind within 45 degrees of his nose or his back, left or right between (his right clockwise of his heading), away
  from the burst. One within 1 m (the rules' 8 in 10), whatever his stance: under. He is then given the falls from
  under he may take: where the gun that fired it is known (below), only those whose cut limb does not move toward the
  gun, read as a limb is against a burst (`LimbsAway`, the half metre's own measure below: along the line from the
  gun's place to the limb's joint as he was struck, from the fall's first frame and its last), or where every one does,
  those that move it least so (never none: a stance with one fall from under, the twins of 24 November or lying, is
  given it whatever the gun's side, as before); where the gun is not known every one, either leg (a burst under him is
  no point to be thrown away from). And from a shell that may throw him (below) the throw his gun's side would give
  him: the gun as it fired (the round's `by`, `k`, `j`: its track at the round, as `FromWhere` reads it; its side the
  45-degree quarter, as for a burst), the falls from that side as for a burst there, of them the throws, so from that
  side alone and away from the gun. Until the mirrors nothing held the falls from under away from anything: read
  against his gun the one fall from under each stance moved his lower leg toward it about half the time (the fine grid
  below, his gun known: 82,010 of 171,360 cells standing or running, 84,302 kneeling or in cover; his body as often),
  and in the rules' battles 3 of the 5 men who fell from under moved it 0.84 to 1.64 m toward the gun, where under
  b470998e, read by the gun's side, the same 5 moved every limb 1.06 to 4.92 m away from it (one thrown). The mirrors
  as measured (the test's table: each of the 17 twins of 25 November, his gun 400 m off in the middle of a side; the
  move along the line toward the gun, less than 0 away): standing or running `die_leg_off_d` flings the right calf back
  and to his right (a gun ahead -1.06 to -0.68 m, at his left -2.55 to -1.74, at his right +1.74 to +2.55, behind
  +0.68 to +1.06) and `die_leg_off_f` the left calf to his left and back, but in SOV_snow 0.07 m forward (ahead
  -1.62 to +0.07, left +1.68 to +2.42, right -2.42 to -1.68, behind -0.07 to +1.62); kneeling or in cover
  `die_leg_off_e` the left shin to his left and a little back (ahead -0.79 to -0.26, left +1.89 to +2.12, right -2.12
  to -1.89, behind +0.26 to +0.79) and `die_leg_off_g` the right shin to his right and a little back (ahead -0.65 to
  -0.17, left -2.40 to -1.91, right +1.91 to +2.40, behind +0.17 to +0.64). So with his gun at his left a man on his
  feet loses his right calf and a man on a knee his right shin, at his right the left; ahead both (but in the one twin
  whose `die_leg_off_f` moves the calf 0.07 m forward); behind him, where both fly back toward the gun (but in that
  twin), the one that flies least so. So too, standing or running, with his gun a few metres off on his right just
  short of his back (1 to 3 m off, 133 to 135 degrees round, on a 1-degree grid): both legs move toward it there.
  Since the rules lay nearly every shell kill on its man, the falls from under now take most gored men on their feet.
  Where his stance's gore falls hold none from under he may take (a man lying killed by any other round; any man of
  twins before 24 November), by the gun's side as for a burst there, away from the gun (a man lying so since a424a37e:
  under b470998e he was under, not measured, and in the rules' battles 5 of the 8 lying gored flung a limb 0.80 to
  2.55 m toward the gun, where now every limb moves 0.90 to 3.05 m away); where the gun is not known or stood within 1
  m of him, all he may take but a throw (a throw only from his side, and from under only the prone one; a424a37e and
  b470998e still threw a man on his feet so struck from any side, with twins that hold no fall from under for him),
  none measured. His `FallFrom` reads front, back or side from the side his falls are read by (the burst's, or his
  gun's quarter); for a burst under him from the gun as for any man (`FromWhere`: front within 70 degrees of his nose,
  back past 110, the side between), not by its quarter, and it weighs neither his falls from under nor the throw. A
  gun's shell may throw him (`Impl::BigThrow`) when it is a field gun's, a howitzer's or a heavy gun's: the round's `w`
  `gun` (the rules write a gun crew's shell so, whatever its calibre) from a block whose `cal` is not `light` (a block
  with no `cal`: a howitzer's, as the Fates table reads it, `LandKit`'s `WeaponWord`); a light gun's, a tank's (`tank`),
  a mortar's (`mortar`) or a grenade's (`grenade`) never. He may take (`MayTake`) any of his stance's gore falls that is
  not a throw (kind `thrown`, or any `die_blast_*`) nor from under; a throw only from a shell that may throw him and
  only one from his side (where the burst was under him and no gun is known, only the prone throw, from under); a fall
  from under only where the burst was under him. `ChooseFall` weighs only the falls `LandMen::GoreFalls` gives him (from
  the twin and how the round struck him, `GoreStrike`: his stance, his side, the point in his own axes, his gun's side,
  whether the shell may throw him; the test asks it too): of those he may take, the ones from his side (a burst in
  front: those from the front, of kind `back`; behind: `forward`; left or right: the side falls from that side), or
  where his stance's gore falls hold none from there he may take (in twins before 24 November: kneeling or in cover none
  from the right, and from the left only `die_blast_c`, a throw; lying only the throw from under), all he may take. Of
  those, where he is thrown away from a point, the falls whose every limb moves at least half a metre away from it,
  along the line from it to the limb's joint as he was struck (`((B - A) . (A - P)) / |A - P|`: a limb flung across the
  burst, or through it to land beyond, does not count as away), read from the fall's first frame and its last, in his
  own axes (his own pose as struck is within 0.35 m of the fall's first at a cut). Where none from his side does so, the
  same of all he may take but those from the side opposite his (front and behind, left and right), never a throw from
  another side (the verifier of 35ffc951: a side is a quarter round, and the twins' standing and running falls from the
  front fling a left arm or leg back and to his left, so `die_arm_off_b` and `die_leg_off_a` flung it toward a burst or
  a gun 33 to 44 degrees round ahead on his left); never one from the side opposite (the verifier of f6f8b75c, whose
  widening took them: such a fall throws him toward the burst, so CHN and ITA_desert men standing or running with it 40
  to 44.5 degrees round ahead on their left were given `die_leg_off_b`, from behind, their pelvis 1.1 to 1.7 m toward
  it); and never one from under (b470998e's widening took `die_leg_off_d` from the twins of 24 November, a standing or
  running man's body as much as 0.23 m toward a burst or a gun 45 degrees round ahead on his left, in 720 and 721 cells
  of the fine grid below). Widened so, he keeps his side's own falls. Failing any, the one whose limbs move farthest so.
  Each kind keeps its weight (by his stance, the shell, `FallKindWanted`), shared among its falls taken; where the burst
  was under him the kind `under` three times, as the kind matching the burst is (back for one in front, forward behind,
  side at his side; his `FallFrom` weighs neither it nor the throw). So where a field gun's or a howitzer's shell
  bursting on him is offered a throw from his gun's side, the throw weighs 8 (0.2, 40 times) to the fall from under's 3,
  8 times in 11 whatever his stance and his gun's side, before the Fates rule and the section rule: as often as when he
  was read by his gun's side (b470998e) for a man standing with his gun ahead or behind, or running with it ahead;
  somewhat more often otherwise, where the matching kind weighed more (standing or running with it at his side 3.9, 8 in
  11.9; running with it behind 6, 8 in 14; kneeling with it on his left 5.85, 8 in 13.85). In the test's band's battle
  73 men were offered both and 43 thrown (the Fates rule, never the last fall again, sways a choice of two). Every other
  round kills him in a fall from under. With the twins of 24 November each stance has one, so every man on his feet so
  struck falls alike: `die_leg_off_d`, his right calf, standing or running, and `die_leg_off_e`, his left, kneeling or
  in cover (all 5 in the rules' battles). With the mirrors of 25 November, the leg away from his gun as above, or
  either where it is not known. The Fates rule and the section rule as for any man. A man not gored falls exactly as
  before: the test's digests of the switch off, and of the men not gored where the same men are gored, are the same
  before the change and after (the code of 3f85e988 and 3e64f668 against this; the men not gored against that code with
  only this change to who is gored, the men lying; and 35ffc951, f6f8b75c and b470998e against this, each changing no
  dice and not who is gored, only a gored man's fall: in the band's battle below its 2232 men not gored the same and its
  1338 gored the same men, and in the rules' Normandy and Prokhorovka every man not gored the same, the switch off and
  on, and the same men gored; against b470998e so with the twins of 24 November and with 7caf9453's, and with 7caf9453's
  every gored man's fall the same too but where the burst was under a man with no gun known, who is no longer thrown:
  b470998e's own test built against this prints its output byte for byte but for its timings and those men, its first
  battle's thrown 8 of 22 where 13, its sides' battle's 11 of 52 where 19. Against a424a37e, whose reading of a man with
  no gun known this round changed, every fall with the twins of 24 November the same, the test's battles and the rules'
  Normandy and Prokhorovka). What holds, read from the falls' first frames and last (the test's sweep, below: every
  twin, every 3 degrees all round; and a finer sweep beside it, every half degree, the burst or the gun 1 to 400 m off
  at 17 distances, 416,160 cells for each stance, and the burst under him, his gun 12 to 400 m off all round or not
  known, 97,954 for each stance): where a fall from his side he may take moves every limb half a metre away from the
  burst or the gun, he is given only such falls from his side; where none from his side does but another he may take not
  from the side opposite does, such falls; else the one of those whose limbs move farthest away; so never a fall whose
  limbs move less far away than another he may take not from the side opposite would move them, up to half a metre; and
  with the twins of 24 November in no cell of the fine sweep a fall from the side opposite where he may take one from
  his side, a fall from under but where the burst was under him, a throw but a field gun's or a howitzer's, or a throw
  from a side not his (or his gun's), its own reading of the rule giving the same falls in every cell; with 7caf9453's
  the same (where the burst was under a man on his feet and his gun is not known, 17 cells of each stance, all he may
  take but a throw, where a424a37e and b470998e gave him a throw from any side). With the twins of 24 November, on the
  fine grid: a man standing or running, no limb toward the burst or the gun, the least 0.36 m away, in 151 cells of each
  stance under half a metre, the burst or the gun 25.5 to 37.5 degrees round ahead on his left, where no fall he may
  take does better (CHN, FRA, ITA, ITA_desert and SOV, as with the twins before), and his body never toward it (the
  least 0.18 m away); kneeling or in cover, no limb toward it and none under half a metre (the least 0.50 m), his body
  toward it in 858 cells of each, as much as 0.36 m (SOV_snow, a burst 1 m off behind on his left, a fall from behind),
  806 of them where another fall he may take moves it away; lying, no limb toward it and none under half a metre (the
  least 0.50 m), his body toward it in 6824 cells, as much as 0.04 m (172 where another moves it away); the burst under
  him, a throw from his gun's side given standing in 47,437 cells, running in 22,821, kneeling or in cover in 12,308
  (`die_blast_c`, the gun on his left), its every limb at least 0.50 m and his body at least 1.46 m away from the gun;
  lying, a field gun's or a howitzer's shell the throw from under him, any other by his gun's side (his body toward the
  gun in 824 of those 48,960 cells, as much as 0.02 m). With the twins before (7caf9453's), as under b470998e: standing
  or running the same; kneeling or in cover a limb toward it in 4932 cells of each (as much as 0.73 m: USA in cover, his
  gun 400 m off 44.5 degrees round ahead on his right, `die_arm_off_e`), 24,083 under half a metre, his body toward it
  in 63,859 (as much as 1.02 m: a burst behind on his left from a shell that may not throw him, so no fall from the left
  he may take, and `die_arm_off_e` falls back). His limb leaves from his own pose as struck, not the fall's first frame,
  so it moves a little more or less than that. In the test's four battles (below), with the twins of 24 November, no
  limb toward the burst or the gun of a man read by a side or thrown (standing 439 limbs, running 351, kneeling 372,
  lying 16; the least 0.38 m away, 10 under half a metre, all in the band's battle), and no body of a man standing or
  running; the falls from under are not among them (not measured), and read against the gun the test prints them: in the
  guns' battle 37 of 82 lower legs toward it (as much as 2.12 m) and the 2 lying thrown from under 2 of their 4 limbs
  (4.72 m), in the band's 240 of 433 (1.32 m); in its first two battles no gun is known; with 7caf9453's, as under
  b470998e, standing and running the same and kneeling 39 of 587 limbs toward (as much as 0.50 m). In the test's first
  three battles with the twins of 24 November (GER, USA and POL): of 241 gored men, 105 were read by a side (6 by the
  gun's side: lying, killed by a light gun's, a tank's or a mortar's shell bursting on them) and 129 the burst was
  under, of them 123 in a fall from under and 6 thrown from their gun's side; every one standing or running read had a
  fall from his side he may take, and fell in one; 23 lying gored, 18 in falls by side (not throws) and 5 thrown from
  under by a field gun's or a howitzer's shell bursting on them. No throw but a field gun's or a howitzer's (none of a
  light gun's gored men), and none thrown from a side toward the burst or the gun (the prone throw from under is not
  measured: the guns' battle's 2 so thrown flung 2 of their 4 limbs toward the gun, as much as 4.72 m). In the rules'
  Normandy and Prokhorovka with the twins of 24 November the 14 gored men were all struck within 0.3 m of the burst (13
  within 0.2 m): of the 6 standing, running or kneeling, 5 fell from under (not measured: against the gun 3 of them
  moved the lower leg 0.84 to 1.64 m toward it, above) and 1 (a howitzer's shell) was thrown from his gun's side, away
  from it; the 8 lying (a mortar's or a tank's shell) fell by their gun's side, every limb 0.90 to 3.05 m away from the
  gun, the body at most 0.01 m toward it. With 7caf9453's twins the 6 gored men as under b470998e (2 thrown, both by a
  howitzer's or a field gun's shell), all by the gun's side.
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
- **Test**: `Tools/land_gore/run.sh [twins folder] [out folder]`: the off, on, same and replay checks above (with rifle
  never, none past reach, nearer more often, the man a burst was laid on less often than one within 2 m, the burst's
  side), no whole joint of a gored man open on its caps (his twin's seam copies within 2 mm of the points they copy,
  skinned; without the joined skin 225 mm), a note for each limb resting under 1 m from its joint or flying over 3 m
  high (the art director's asks; the clips' own to mend), and `gore_falls.ppm`, six gored men from the hit to rest, no
  army's clip twice. The sides, in its battle, in a second of 384 men standing, running, kneeling and lying, killed by a
  gun, a tank or a mortar 0.5 to 3.2 m from the burst all round, in a third of 240 so killed by a shell bursting 0.6 m
  from them, fired by a gun of their own 400 m off all round (a howitzer's, a light gun's, a tank's or a mortar's), and
  in a fourth, the band's, of 210 men of each twin in the twins folder (3570 of 17) standing, running or kneeling,
  killed by a howitzer's, a light gun's, a tank's, a mortar's or a grenade's round bursting 1.6 to 2.4 m off or 0.6 m
  from them (their gun 400 m off, a grenade's thrower 20 m), 31 to 49 degrees round ahead on their left or their right.
  Each gored man read by a side (1.5 m or more from the burst, or within 1 m where his stance's gore falls hold none
  from under he may take, by his gun's side; from his track at the round as the crowd reads it) in a fall from his side
  where one from there he may take moves every limb half a metre away, in none whose limbs move less far away than
  another he may take would, up to half a metre (but where he may take one from his side, none from the side opposite),
  and never in one from the side opposite where he may take one from his side (the falls read from their first frames
  and last), every one standing or running with a fall from his side; each the burst was under (within 1 m) whose
  stance's gore falls hold one from under he may take in one, or by a field gun's or a howitzer's shell in the throw his
  gun's side gives him (every limb as far from the gun as that side's falls set); his gun known, none in a fall from
  under that moves its limb toward the gun where another from under he may take does not, nor where all do in one that
  moves it more than the least of them (read from the falls' first frames and last, 0.01 m allowed: the test reads his
  place and heading in its own sums; asked of the guns' battle wherever the twins hold any fall from under, and there
  of at least one man so, so that it cannot pass on none, and of the band's only with a man so; not of the first
  battle nor the sides', whose rounds have no gun known), and printed, his fall from under read against his gun as
  played, by stance, and the men such a shell offered both a fall from under and a throw, with how many were thrown
  (no check of the kind's weight: the Fates rule sways a choice of two too far, the band's 43 of 73 thrown against 8
  in 11 before it); no limb off a man standing or running moving toward the burst or the gun as it
  moved (the least and those under half a metre counted), nor off a man of a stance whose gore falls hold one from each
  side that is not a throw (with the twins of 24 November kneeling, in cover and lying too; in those before a note), nor
  his body (his pelvis), standing or running, more than 0.1 m where another fall he may take moves it away (kneeling, in
  cover or lying a note); no man thrown from a side with a limb or his body moving toward it; only a field gun's or a
  howitzer's shell throwing a gored man (a light gun's goring some, throwing none); a man lying thrown only from under
  him, by such a shell bursting on him; where the twins hold lying falls by side, men lying gored in them by a tank's or
  a mortar's shell or one off to their side; where they hold falls from under, men the burst was under falling in them
  and some thrown from their gun's side; counted by side and by stance; and digests of the falls and skins of the men
  not gored and of the switch off, to be byte-identical before and after a change that does not change who is gored.
  Then the sweep: `GoreFalls` and `GoreSide` for each twin in the twins folder, standing, running, kneeling, in cover
  and lying, a shell that may throw him and one that may not, the burst 1, 1.5, 2, 2.5 or 3 m off or his gun 25 or 400 m
  off (read by its side), every 3 degrees all round (142,800 cells), and the burst under him, his gun 25 or 400 m off
  every 3 degrees all round or not known (40,970), each fall given read by the test from its first frame and last: every
  fall given one he may take (none from under but where the burst was under him); the side read as the test reads it;
  only falls from his side wherever one from there moves every limb half a metre away; none moving a limb less far away
  than another he may take (but where he may take one from his side, none from the side opposite), up to half a metre;
  none from the side opposite where he may take one from his side; no limb off a man standing or running moving toward
  the burst or the gun, naming the least move and counting the cells under half a metre, the band's (30 to 50 degrees
  round ahead on either side) apart, nor off a man of a stance whose falls hold one from each side; no fall given a man
  standing or running moving his body toward it more than 0.01 m where another he may take moves it away (kneeling, in
  cover or lying a note, the cells toward counted); the burst under him, the falls from under the rule gives (above)
  and else only such a shell's throw from his gun's side, where none from under what his gun's side gives, and no throw
  moving a limb or his body toward the gun; of the falls from under of a man standing, running, kneeling or in cover,
  his gun known, none given that moves his limb toward it where one he may take does not (the cells counted), where
  every one does those that move it least so (the cells counted and printed as gaps, by stance and side with where his
  gun was and how far toward it, not failures), his gun not known every one (that none toward it and this asking for
  cells only where some twin's falls on his feet or a knee hold one from under: 7caf9453's hold none), and any stance
  with one from under (the twins of 24 November, or lying) given it whatever his gun's side; and a table, for each
  stance on his feet or a knee and his gun on each side or not known, of which falls from under are given (of the
  twins, his gun 400 m off in the side's middle, and of the sweep's cells) and how far each moves its limb along the
  line toward the gun. Built against
  35ffc951's rule it failed six of its then 52 checks: falls given where another he may take moves every limb farther
  (the sides' battle 1, the guns' 3, the band's 136, the sweep 926 cells); the band's battle 60 of its 1006 standing and
  running limbs toward the burst or the gun (as much as 0.58 m, 80 under half a metre away); the sweep 290 cells toward,
  all in the band (756 of its 6664 under half a metre; as much as 0.63 m: SOV_winter standing, a burst 1 m off 42
  degrees round ahead on his left that may not throw him, `die_arm_off_b`). Built against f6f8b75c's rule, whose
  widening took falls from the side opposite, it failed six of its then 62: a fall from the side opposite (the sides'
  battle 1 man, the guns' 2, the band's 4, all kneeling but the band's, and the sweep 164 cells), and a standing or
  running man's body toward the burst or the gun (the band's battle 4, as much as 1.65 m: CHN standing, his gun 43
  degrees round ahead on his left, `die_leg_off_b`; the sweep 28 cells, as much as 1.70 m). b470998e's own test and
  code, given the twins of 24 November, fail four of its 62 checks: men lying gored in falls that are not throws (the
  sides' battle 12, the guns' 6: the rule it held them to), and a standing or running man's body toward the burst or the
  gun where another fall he may take moves it away (the band's battle 2, as much as 0.11 m; the sweep 95 cells, as much
  as 0.23 m: USA_nco standing, his gun 45 degrees round ahead on his left, `die_leg_off_d`, from under, widened to);
  built against this rule it fails eleven, reading a man the burst was under by his gun's side, so that his fall from
  under counts as flinging a limb toward the gun. With this rule, given the twins of 24 November, all 76 checks pass: no
  note at all; no limb toward the burst or the gun in any battle, at any stance, of the men read by a side or thrown,
  and no body of a man standing or running (the falls from under, not measured, read against the gun: in the guns'
  battle 37 of 82 lower legs toward it and 2 of the 4 limbs of the lying thrown from under, in the band's 240 of 433; on
  the sweep's grid, his gun known, standing or running 4074 of 8160 cells with the leg toward it and 4080 with the body,
  kneeling or in cover 4076 and 4082, lying, the throw from under, 3992 of 4080 with a limb toward it); on the sweep's
  grid standing and running each 10 cells under half a metre (the least 0.36 m), kneeling and in cover none (the least
  0.51 m), lying none (0.50 m), the band's 18 (746 given a fall from another side); a body toward it kneeling or in
  cover in 58 cells each (as much as 0.30 m), lying in 454 (0.04 m); the burst under him, 36,873 cells with falls from
  under he may take, in 7928 of them a throw from the gun's side (none toward it), 4080 with none from under read by the
  gun's side, 17 with the gun not known (lying, by a smaller round: all his lying falls). Given 7caf9453's twins all 74
  passed (the lying and the under battles' two checks need their falls), as under b470998e but that the burst under a man
  with no gun known throws him no more (the first battle's thrown 8 of 22 where a424a37e had 13, the sides' battle's 11
  of 52 where 19; the same men gored, the men not gored and the switch off the same): kneeling and in cover a note, 650
  of the sweep's cells with a limb toward it (as much as 0.59 m), 8878 with the body toward it (as much as 1.02 m).
  With the rule of the falls from under against the gun, given the twins of 25 November (17 twins, 26 gore falls each:
  the mirrors), all 82 checks pass, no note: the switch off, the men not gored and who is gored the same as under
  3efa6a0c with the same twins (every battle's digests); in the band's battle 433 men in a fall from under with the gun
  known, none moving the leg toward it where the other leg would not (as played none at all; under 3efa6a0c 186 of 434
  toward, as much as 1.50 m), 43 of its 73 offered both a fall from under and a throw thrown where 42 (the Fates rule,
  never the last fall again, now has one fall from under to weigh where it had two); in the guns' battle 84, as played
  9 of 82 lower legs toward the gun, as much as 0.78 m, every one with the gun behind him (under 3efa6a0c 42, 2.46 m).
  On the sweep's grid, the burst under a man on his feet or a knee and his gun known: 29,496 cells where one of his
  falls from under does not move his leg toward the gun, none given one that does; 3144 where both do, every one with
  the gun behind him (standing and running 1022 cells each, kneeling and in cover 550: Owed), given the one that moves
  it least so; his gun not known, both legs in all 136 cells. Read against the gun the falls from under given move the
  leg toward it in 1022 of the 8160 cells standing or running (7132 under 3efa6a0c) and 550 kneeling or in cover
  (7602), the body in 2008 and 1058 (7638 and 7400; not held). Given the twins of 24 November (one fall from under each
  stance) every fall is as before: all 82 pass, the output 3efa6a0c's but for the new lines and the reworded labels
  (every digest and every count the same, the band's gored men's too), its gaps printed (standing or running 1518 cells
  behind him, 448 ahead on his right, 2108 at his right; kneeling or in cover 1296 behind, 672 ahead on his left, 2108
  at his left). Given 7caf9453's twins (14 gore falls each, none from under on his feet or a knee) all 79 pass, their
  own 43 notes as under 3efa6a0c, the output 3efa6a0c's but for the new lines and the reworded labels (every digest
  and every count the same): the sweep's checks of the falls from under on his feet or a knee find no cells, asked for
  cells only where some twin holds one, and of the battles' checks of the falls from under against his gun only the
  guns' battle's is asked, its 2 men lying thrown from under, by `die_blast_d` (asked there wherever the twins hold
  any fall from under, of at least one man so read: it fails rather than passes where the battle has none). With every
  set the first battle and the sides' print instead that none there is in a fall from under with his gun known (their
  rounds have no gun known). The repo's Content holds no twins: there the test stops at its first line, no USA or GER
  twin, as under 3efa6a0c. The same output from g++ and clang++; 9 to 12 s here. Pictures:
  `/mnt/project-files/battle-fx/gore/` (r3: the joined skin, fresh twins 29ad7cfe).
- **Owed.** The Mac shots behind the switch, and gore twins for every soldier file (the soldiers folder's `GER_panzer`
  and `USA_tanker` have none: they fall as before). The deaths thread's re-runs of the falls that rested a limb under 1
  m from its joint or threw one over 3 m up (USA and GER `die_leg_off_a`, GER `die_arm_off_b`; USA and GER
  `die_blast_a`): the test's first battle notes none with the twins of 24 November, nor with 7caf9453's, but it reads
  only the falls its gored men take. From the deaths thread, for the sides: a standing or running fall from the front
  that flings a limb back and to his right (today each flings one back and to his left, so for a burst or a gun ahead on
  his left he is often given a fall from another side (746 of the sweep's 6664 band cells), and in CHN, FRA, ITA,
  ITA_desert and SOV, 25.5 to 37.5 degrees round on the fine grid (27 to 36 on the test's), none he may take moves every
  limb half a metre away: 0.36 m at least). From the deaths thread, for the burst under him: the mirrored falls from
  under came on 25 November (`die_leg_off_f`, `die_leg_off_g`), and with his gun known a man on his feet or a knee is
  given the leg that flies away from it; but with the gun behind him both legs fly back toward it (the sweep's gaps,
  above: standing or running the gun within 42 degrees of his back on his right or 27 on his left, the leg as much as
  1.19 m toward it, and on a 1-degree grid a gun 1 to 3 m off within 47 on his right, so beyond his back's quarter;
  kneeling or in cover within 18 and 15 degrees, 0.60 m), so a fall from under that flings a lower leg forward is owed
  for those. The prone throw from under (`die_blast_d`, a limb toward the gun in 75,810 of the fine grid's 85,680
  cells, 3992 of the test's 4080) is kept as it was, not held away from the gun. Nor is a man's body in a fall from
  under (toward the gun in 2008 of the sweep's 8160 cells standing, as much as 0.71 m; 1058 kneeling, 0.62 m).
  Kneeling, in cover or lying the falls' own bodies may move toward the burst or the gun: kneeling or in cover as much
  as 0.36 m, a burst behind on his left, in a fall from behind
  (806 cells of each stance's on the fine grid where another he may take moves it away; in nearly all of them the falls
  given already hold one that does, `die_arm_off_m` beside `die_arm_off_f`, so choosing among those by the body too
  would make the test's note a check: a follow-up), lying as much as 0.04 m; the test notes them. The rate: in the
  rules' battles 14 of 93 shell kills gored (1 in 6.6) where the odds expect 9.8 (1 in 9.5, inside the art director's 1
  in 8 to 10; the 14 about 1.5 standard deviations above it, those battles' dice); the odds are not changed here, hers
  to rule, with both figures before her. The half metre is the falls' own, read from their first frames: his limb leaves
  from his pose as struck, a little less or more, and nothing holds it so (in the band's battle 10 of its 1053 limbs
  under half a metre away, none toward).

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
  45 fps reading pass; Brian, 23 November 20:09Z; redone to the art director's notes of 24 November, 02:26Z, and his
  canopy's shape to her verdict of 24 November, 14:34Z: (c) its skirt, (b) how it lies; her (a) waits on the soldier
  files). The
  rules say whether a pilot got out (air:lost's chutes: half the burning and spinning planes', never a plane blown
  apart's); `LandAir::Sky::Chutes` says how. He goes over the side 1 to 2 s after the hit (out of a spin 0.5 to 0.9 s),
  never in her last 0.8 s, and where she is under 60 m by then at the highest she is after the hit. (`Sky::Build` plans
  them whenever `-TheaterLandAirSupport` is on, on dice of their own, so the planes fly the same either way; only
  `FlyOver` reads `-TheaterLandChutes`, and draws nothing of them without it.)
  - *In the air* (her first and second notes, and her answers). He leaves her with her way across but none of her
    climb, and falls free on gravity alone, so he is never above where he left her and always at least half g t
    squared under it. His way through the air (across, the wind's taken off) dies as a falling man's does (his drag at
    the 55 m/s a man falls at) and then on his canopy as it fills; the wind is not a push from rest, he is in it from
    the first. He pulls after 1 to 1.6 s (sooner out of a plane already diving: once he has fallen as far as a man from
    rest would have), and his canopy fills over the next 45 to 50 m of his fall, so that he drops about 50 m before the
    wind takes him (within 1 m/s of it). He comes down at 5 to 6.5 m/s (one man in five heavy: 6 to 7), drifting with
    the battle's wind, swinging under it about 4 s a swing, from 14 degrees settling to a gentle 5 that never quite
    dies. Out under 60 m (her fifth) he pulls at once and his canopy is still filling as he lands: a hard landing (his
    way still spent: 8 m/s through the air at most).
  - *His canopy* (her sixth, and her answers): 5 to 5.5 m across in the air (7 m is only its size lying flat) and
    about half as deep, on 24 to 28 gores, her off-white for every nation (0.86, 0.85, 0.80, each chute about 4%
    warmer or cooler). Side-on (her verdict's (c): its skirt had dropped straight, "like a jelly mould") it is
    widest 0.38 of its depth under its crown (`LandAir::WidestDown`; about a third, the test a quarter to a half)
    and its skirt curves in from there all the way down to a hem 0.82 as wide (`HemIn`; her 0.8 to 0.9), by the 1.5th
    power of the way down (`Curl`), its silk leaning in more ring by ring (10, 17, 21, then 24 degrees at the hem,
    toward its lines at 29 to 31 degrees in to him, so that they run on from it); its crown a round dome over its
    widest (`Crown`). A review of 24 November found it still dropping straight from 0.41 to 0.66 of its depth under a
    flat-topped crown: its silhouette in the picture is now 0.977 of its widest half-way down its depth and 0.953 at
    0.6 (it was 0.988 and 0.977), within 2.3% of its widest for 0.18 m under it (0.46 m), and its crown 0.34 of its
    widest 3% under its top and 0.63 11% under (0.44 and 0.70). Its gores billow a little between the seams as
    before. `LandAir::CanopyShape` and `HeapShape` make it (plain C++, both its faces): the seams faint
    darker lines and the hem a little darker, in the points' colours, so they cost nothing a frame; a line from each
    seam to his four risers. Its glow against the sun is one number a frame: `LandAir::Glow` (a little seen from under
    it, most against the sun, none with the sun behind the eye or at night), by the key light against a clear day's,
    given as `FigureFill` (and `FigureSun`, the pieces' rim) to the silk's own copy of the things' ink (M_MapSurface: no
    new material). The mesh is made once for each make it has (him falling free; him, his lines and silk; down, him
    going over; once the crowd's man has him, his lines and silk) and otherwise moved in place where it can be
    (`UpdateMeshSection`: its points, facings and colours, no new proxy; its makings' storage kept from frame to frame),
    never a frame for its look (`LandAir::SilkMoved`, `ManMoved`). In the air the silk is moved in place as it fills
    (by 0.02 of its open share): a canopy filling never folds a triangle over. Down it is made again each 0.05 of its
    falling in: the scene winds each triangle by where its points are, and as it tips, falls in and lies in its folds a
    few in a thousand turn over within a step, which kept for good would show their inside's shading. Between two
    makes it is moved in place every frame toward the next step's shape (`LandAir::SilkStep`: each make is the step it
    is in, the last make's next, and the next one; `At` moves its points and facings between them, and it is made
    again as its slump reaches the next, `Due`), and its triangles are wound by where their points are half-way
    through the step (`Wound`), so a triangle turning over shows its inside for at most half a step (0.12% of its
    triangles' frames here). A second review of 24 November found it held still between makes and jumping up to 1.3 m
    at a make some eleven times a second, and a smooth torpedo half fallen in. His part (him going over, by 0.05, and
    his lines) is moved in place with it every frame. At 45 fps a pilot's mesh is now made 24 times (21 of them as his
    canopy falls in) and moved in place 151 to 167; its silk moves 0.22 m in a frame at most (10 m/s), as much at a
    make, and is drawn within 0.07 m of its true shape. Its cost: lying, his silk is 2,706 points and 5,200 triangles
    on 26 gores, 2,914 and 5,600 on 28 (in the air 1,874 and 3,536, 2,018 and 3,808; it had been 3,138 and 6,048 on
    14 rings, then 2,466 and 4,704 on 11: it is now on 13, for its folds across it); a make (one `HeapShape`, its next
    step's: the step it is in is the last make's) 0.15 ms on a flat field (0.16 before its lines were laid across its
    body and its far end broken up: the exact pass that lays its lines over its silk, about 0.12 ms, runs once, at the
    make it lies still; 0.13 on 11 rings, and on 13 before its look from above was reworked; its instructions a fifth
    more) and about 0.3 ms on hummocks on the cloud box, its ground
    read once on a grid (not twice a point). The log tells each pilot's count
    once his canopy lies flat, for the 45 fps reading: his mesh's makes and moves in place, his silk's points and
    triangles lying, its makes as it fell in and their time (ms a make, and the longest), and the most its points
    moved in a frame (metres).
  - *Down* (her third, fourth and fifth notes). Seconds are real seconds from his landing. His chute's mesh draws him
    going over flat (0.5 s; 0.35 after a hard landing) under his falling canopy; 0.4 s later, as he lies there, the
    scene swaps him for a man of the crowd: in his army's tank crews' file where it has one (`USA_tanker`,
    `GER_panzer`, with their own heads), else its infantry's (`RiderFileOf`) bareheaded (`AddRider`'s new `bBareHead`:
    his file's helmet left off, no finish drawn) and without his pack (`bNoPack`, where his file marks it:
    `Crowd::PilotDress`, below), unarmed. So that nobody sees him pop: the crowd's man is a rider, in
    `go_prone` held at its end with its end offset not taken back (`StepRider`), so his hips lie 0.59 m forward of his
    feet, 0.14 m to his right and 0.14 m up (every soldier file's `end_offset` and pelvis, within a centimetre); the
    mesh's man pitches 84 degrees about his feet and is moved so that flat his hips lie there (`LandAir::OverShift`,
    by his own file's end offset once it is known). He had been moved back 0.9 m and lay 0.7 m short, and the crowd's
    man jumped forward in plain view (a review of 24 November). And the mesh keeps him until the crowd's man has been
    posed with his ride: the crowd is posed before `FlyOver`, so a ride set there shows the frame after, and he had gone
    for a frame; where no man of the crowd can be made he stays lying in the mesh. In a wind over 4 m/s his canopy
    drags him 1 to 2 m downwind first (at 1 to 1.5 m/s). He lies 1.5 to 3.5 s (after a hard landing 3 to 5 s), gets up
    onto his knees (`go_prone` played back from 1.9 s to 0.95 s, over 1 s; 1.4 s after a hard landing), works his
    harness free kneeling, his hands at his chest, for 1 to 1.5 s (`kneel_down` held at its end, 0.5 s, which is
    `stand_up`'s first pose), stands (`stand_up`, turning to his way) and runs 20 to 40 m from the nearest enemy block
    (`run`), and goes to ground there (`go_prone`, held). After a hard landing he limps: there is no limp clip, so `run`
    slowed to 0.65 of its pace and hurried off his right leg (a hitch of 0.45 of the loop's pace either way), his feet
    kept to its stride, and only 0.6 as far (whole half-loops). `Chute::Rate` says how fast each clip goes (less than
    nothing played back) for the scene's blends, and `Chute::Blend` over how long: 0.25 s, but 0.5 s into his harness's
    kneel, `go_prone` on his knees being far from `kneel_down`'s last pose (his pelvis 55 degrees off it, his right
    thigh 68 to 72, his pelvis 0.18 m on; no time of `go_prone` nearer than 45 degrees), so that he settles into it
    rather than lurching. His canopy falls in over 3.2 s on an even clock (each part of it easing in and out on its
    own; on an eased 2.5 s its crown had swept down at 17 m/s at its fastest), its lines back to his harness where he
    lay (from the half of its skirt behind its middle, the same lines all the while: `LandAir::LineBack`; they had
    changed halves as it fell past him), taut while it is up and a little slack once it lies (each line in two
    lengths, its middle let down toward its lower end's height and bowed to one side, every other line to the
    other).
  - *How his canopy lies* (her verdict's (b): it had lain a flat oval in even scallops, "a doily", shrinking in place
    as it fell in). `LandAir::HeapShape`: as it loses its air it tips forward on its lines about his harness and
    blows downwind, its side toward him blown in over the other (a crescent), its far half going slack first (its
    round section flattening, its side toward him coming down onto the other, its silk crumpling), flapping and
    billowing along it until each part of it is down (its near end and its front first), and streaming out along the
    wind ahead of coming down, so that its middle only ever moves away from him and its top only ever comes down (half
    fallen in it is already 1.3 to 3.7 m longer along the wind than as he landed, its middle 6.3 to 7.4 m on). It lies
    stretched along the wind, 2.2 to 2.8 times as long as it is wide (6.3 to 8.5 m long, a stronger wind stretching
    it), its near end gathered 2.1 to 3.5 m past his harness, where its lines run back to him across the grass (2.1 to
    3.5 m of his 4.1 m lines, a little slack; a review of 24 November found 0.6 to 1.2 m stubs), its far end standing
    0.37 to 0.72 m up on the air it still holds (more in a stronger wind) and swelling a little there, and coming round
    and down to the ground over its last fifth: its last ring (0.975 along it) about half as high as its far quarter
    and its crown, its tip, on the ground (0.03 m up; the review found a beak hanging 0.23 m off it; at 40 m in the
    picture its top is 0.47 m up 0.9 along it, 0.35 at 0.95, 0.22 at 0.98 and 0.04 at its tip, falling faster toward
    it). Along it run four or five long loose folds, one big and the rest smaller in no order, each wandering across
    as it goes; its edges waver and its far end lies a little to one side.
    The gaps between the folds are graded (the widest three times the narrowest, 2.6 times with five), the wider going
    where two folds need more room to stand apart (a big one beside a small one: 1.4 times their widths, and the big
    one's flank half-way between them under a third of the small one's height), and all of them spread together until
    each two stand apart, so that the grading holds (the review found the narrowest held up to a floor alone, which
    left half of them alike). All of that is drawn from the pilot's own number (`Chute::Folds`, on its own dice,
    `0xC47E5`) and the wind (`Chute::Wind`), so no two pilots' canopies lie alike. `Chute::Heap` (its middle) lies
    where it does: half its length on from its gathered near end (`LandAir::HeapOff`, 6.8 to 8.1 m downwind here).
    The scene gives `HeapShape` the ground as it is now, as a wire is laid (`WireAt`): the field's (`HeightAt`: its
    terrain, its lanes, its banks and its floors) raised to a hole's face or a breach's floor where one is torn under
    it (`Holes.TopAt`). `HeapShape` reads it once a make on a grid a quarter of a metre square under the silk (coarser
    over one still high in the air: 2,500 heights at most, 390 to 1,550 here, 750 on average, against 1,457 points a face), lays each point on it,
    and keeps every edge of the silk over it (a bank's, a ditch's side, a crest's or a crater's rim between two points
    lifts them, the nearer the more: each edge's shortfall worked out from its points as laid and shared between its
    ends, the least lift that clears it, and each point lifted once, by the most any of its edges asks; but where that
    would lift an end more than 0.1 m over its room, how much higher the ground stands within 0.6 m of it than under
    it, shared so that both ends stand as little over their rooms as can be, the end by the bank taking the more), the
    grid raised by how sharply the ground bends about each cell (a step narrower than a cell), but for the silk never
    more than its cell's top: the highest a crest can stand along any side of the cell, the slopes coming in either side
    carried on to where they meet (a ridge with straight sides exactly), no less than an eighth of the bend over its
    highest corner and no more than half. The review found its
    points on the ground but its triangles cutting 0.2 m into a bank, a ditch's sides, a sharp crest and a crater's rim,
    and the scene's ground blind to holes. A fourth review found each edge lifting both its ends by all its shortfall,
    edge after edge: its crown's tip, an end of every edge of its last ring's fan, was lifted once for each, and lying
    along a bank under one edge it stood up to 6.7 m over the ground like a tent pole (4.8 m over the rest of its fan,
    on 91 of 300 pilots more than 0.5 m); and the grid's whole bend, 0.74 m over a hedgerow bank 1 m high and 0.8 m over
    the bank along it, had floated its silk up to 2.4 m over the ground round a hedgerow bank. A fifth found half the
    bend over the highest corner still reading a round crest 0.4 m high, and the silk a metre clear of anything within
    0.6 m of it beside the banks (its edges there lifted at their far ends as much as by the bank). Now its tip stands at
    or under the rest of its fan everywhere, with the bank along it or moved 0.5 m, 1 m or 1.5 m either way; its silk
    at most 1.32 m over the ground round the hedgerow bank and 1.46 m along the bank; and over the highest ground within
    0.6 m of it, beyond where it lies on flat ground, 0.56 m round the hedgerow bank and 0.30 m or less on the step, the
    ditch, the sharp crest and the crater (0.70 to 1.94 m before), but still 0.81 to 0.90 m with the bank along it under
    its far end (4.6 to 6.3 before): its edges there run up to 2 m from its last rings' sides to where they gather, and
    one across the whole bank, its ends both off it, is kept over the crest only by lifting both. Capping those lifts
    would let the edges cut up to 0.55 m into the bank (0.29 m on the verdict's four, which the test checks), and the
    points stay where they lie.
  - *How it looks from above* (the art director's note of 24 November 17:40Z: from above both pilots' silk read as "a
    smooth cocoon or a gourd", an even outline and only lengthwise ridges). `HeapShape` lies on 13 rings now (it was
    11), a little over half a metre apart where it folds across, and on dice of their own (the pilot's number with
    `0xC47E7` above it, so that all its other draws fall as they did and no pilot's heap is another's mirrored).
    - *Folds across it*: three or four, where it bunched as it fell in, each on a ring of its own from 0.15 to 0.64
      along it (no two side by side), graded in height (12 to 23 cm, lower toward its gathered near end and lower still
      when its far end holds little air, so that its near quarter stays under its far end) and dealt out in no order;
      each highest somewhere across it and fading either way (most across part of its width, not all of it), slanted
      and bowed across it, and bunched up toward the ring before or after it (away from a fold two rings on, so that
      two do not run into one): its crest rounded over onto that ring (side-on a hump, not a tent's peak) and the ring
      on its other side let down a little, so that one flank is steep. Near them, toward its near end, its long folds
      stand a little lower. The two creases across it it had are gone (their dice are still drawn).
    - *Its outline*: one side wider than the other, 7 to 11% of its half-width, and that side the outer side of its
      axis's bend (bowed 0.12 m at the least), so that more of its silk lies on one side of the wind line through where
      its lines gather; its far end slumped the other way, so that it is uneven about the line from there to its tip too;
      each ring's edge jagged a few centimetres either side (each like its neighbours a little, so it wavers rather than
      zigzags); one or two gores flipped over and lying flat out 0.3 to 0.45 m beyond its wide side, and one tucked 0.22
      to 0.32 m under its other side, about its fourth ring to its eighth (none on its far end). Only the outermost eighth
      of its half-width moves for them, so its long folds lie where they were laid; bent across the wind or with gores
      flipped out it has no more silk, so its widths are cut back (a quarter at most) until it lies no wider than its
      length over 2.12. Its gores wander across it as they go, so its seams and its long folds' crests do not run
      straight and even end to end like a melon's ribs.
    - *Its hem*: its near end tapers as before; two or three of its gores flipped back over its top, their fold pulled
      back past its hem toward him (as far as its gathering lets it: no silk more than 0.46 m nearer him than its seams'
      middle, the near-end check's 0.5) and laid on the ground there along their lines, a roll of silk under them, and
      their hem drawn in on its silk keeping some of its spread, so that they lie as a flap; the lines to them and some
      beside them run over it (0.2 to 0.3 m of each past its seam's last 10 cm, never under it: where a line lies over
      the silk the silk under it is pressed down under the line, never through its under layer). A gore or two on the
      other side of its top's middle are tucked under a little, and the rest of its hem lies back toward him in some
      gores and on in others, in a wave round its skirt (5 to 9 cm). Its lines' seams stay behind the middle of its
      hem's, so that its lines run back to him from the same half of its skirt all the while it falls in.
    - *Its far end*: slumped to one side (away from its wider side): its air's crest 0.42 to 0.55 of its half-width off
      its middle, its top leaning over that way, its round wider and later on that side and sooner on the other, its tip
      that way, and its lifted top buckled across on the side it slumps from.
    - *After a second look* (her note again, (3) and (2): its lines had crossed only the flap at its near end, by 0.2
      to 0.3 m, all running to one point; and its far end had stayed a neat dome, its seams running cleanly to a point
      at its tip):
      - *Lines across its body*: it slid and slewed as it fell in, and one to four of the lines to its top's gores (two
        to four on dice of their own, `0x57AE` over the pilot's number, so that all its other draws fall as they did; as
        many of those as will lie so, two on most) were thrown over its near part. Each runs from his harness across the
        grass, bowed out to one side of the rest, onto its top either over the edge of its near part on that side (its
        first ring to its third) or over its near end aside of where its lines gather (0.2 m or more from it), and
        across it at a slant over its near third (28 to 36 degrees off its wind line at the least as laid, 45 on the
        mean; in a loose curve: bowed along it, curled round it, a little wiggle), down into its folds and over their
        crests, to an edge and over it to the ground (on under there, hidden, back to its hem). As laid, 1 to 2.35 m of
        each on its silk (to 2.55 m in later tries should fewer lie so; 3 m in a last try should none), 0.35 m or more
        of that past 0.15 of its length (its body), none farther along than 0.45 of its length, none over a flipped or
        tucked gore's near end nor under its flap, its grass clear of its near part, no two from seams side by side, no
        more than six over its silk in all with the flap's, and from his harness to where it goes down off its far edge
        no longer than a real line (`RealLines`, 1.33 times its spread across: a T-5's 22 ft lines and 3 ft of riser
        under a canopy about 19 ft across in the air; `LinesLong`, the air's hang, draws them short). Each is laid out
        on its silk's own rings and columns, so that it stays on its silk as that falls in. The rest run to its gathered
        hem as they did.
      - *Lying on it, never in it*: each make, each such line is drawn along its path over its silk as it is then
        (finely from 0.75 of its fall): on the top of its silk there and 12 mm over it (the cord's half-thickness and 6
        mm), the highest of its triangles facing up within 3 mm of the point (none round a steep one, whose height run
        on past its edge had floated lines over its gathered hem), found on a grid of only the squares its lines lie
        over; draped between its folds' crests where the straight would not dip into its silk (tried every 2 cm, 1 cm
        near where it comes onto its silk, 4 cm earlier in its fall), its points kept where it turns most; never under
        the ground, from his harness on. While its lines are slack only in part (`LineSlack`: taut to a slump of 0.3,
        slack from 0.8) its grass is drawn between the straight and where it lies, lifted where that would dip under the
        ground: the straight from his harness where `DownShape` draws it (in the mesh's own space, his feet at its
        origin and his man flat and rigid; a second review found that, lifted onto the ground under it as this had
        lifted it, the lines from the harness drawn cut up to 0.23 m into hummocks), each point lifted by its share of
        slack but by no less than 0.01 of it (held to 0.05, a make at a slump of 0.35, 0.028 slack, had left them in
        the ground). A third review found them flung up like poles off a bank, 5 to 7.7 m up, and 1.9 m off hummocks:
        the scene draws him flat at his feet's height, so that there his harness lies up to 0.37 m in the ground, and the
        straight from it ran under the ground most of the way to its silk, the grass next to it lifted five times that and
        more to clear it. So the lines across its silk run taut from the ground over his harness where he lies sunk into
        it (`SilkMesh::HarnessLow`, which `LineLies` draws them from; his harness itself is where the scene draws it, and
        the straight from it comes up out of the ground by him, as those to its gathered hem always had), and those lifts
        (the straight from his harness, and the grass while not yet slack) are made over the ground as it is, read where
        they are, but never more than 0.1 m under the quarter-metre grid's (whose allowance for a crest between its points
        stands a bank 1 m wide 0.6 to 0.8 m higher than it is, and a ditch's lip 0.4 m: lifted over that, they had still
        been flung up to 4 m off the bank; the 0.1 m keeps them clear between makes as that allowance had, where 0.05
        had left one 3 cm in a ditch's lip): some 70 more ground reads a make, 960 on the mean over a fall. Over its
        edge where its top there is not found (a steep skirt as it falls in: `TopAt` passes over steep triangles) a
        line still comes down 3 cm out, not 0.6 m out over the grass. Which seams' lines are thrown
        over it is picked from those nearest its top's middle first by whole half-seams round from it, mirror seams in
        the order glibc's sines had put them for 24, 26 and 28 gores (by the sine's size they had tied to the last bit,
        so that the libm chose which line lay where, a Mac's apart from a PC's); and LandAir.cpp is built with no
        a\*b+c fused into one rounding (`#pragma clang fp contract(off)`, as Unreal's Mac build and run.sh's
        `-ffp-contract=off` have it: clang fuses them on the Mac's arm64 by default, and the lines across it, routed
        through many near ties, came out up to 0.1 m elsewhere lying and nearly 1 m falling in). LandBattleAir's
        `DownShape` draws each line through `LandAir::LineLies`' points (31 for a line across it, three for the rest, a
        rod from each to the next), the same points all the while it falls in, as slack as `LandAir::LineSlack` says
        (the slack `HeapShape` lifts them for; it had had its own `FMath::SmoothStep`). His lines and
        risers are olive drab cord, (0.13, 0.12, 0.07), darker than her off-white silk so that those across it read on
        it (they had been near her silk's own colour), and chutes.py draws them only where the silk does not hide them,
        as the scene's depth test does.
      - *Its far end broken up* (on dice of their own, `0xFA2D` over the pilot's number), from its tenth ring on, its
        ninth and those before it as they were (so its folds across it and its length are its own): its last two rings
        and its vent (its tip, where its seams meet) drawn over toward the side it slumps to, 0.22 to 0.38 of its
        half-width (its width cap read without it); a crease across its dome on its eleventh ring, its air there let
        down by up to 55 to 70% (most on its narrow side, the side it slumps from; 0.6 of that on the other; it had been
        80 to 95%, a sawtooth at eye height with its inside showing along it), slanted 0.22 to 0.38 m along it edge to
        edge either way, the rings either side drawn in 5 cm toward it (10 had folded its silk over there); over its
        last two rings the edge of its narrow side folded back under it, from a half to two thirds of its half-width out
        (slanting a little along it), flat on the ground under the rest; and its seams kinked ring to ring (4.5 to 7 cm
        aside, in two waves across it, both its layers alike so that they keep together at its edges). Its last ring
        never farther on than its tip.
    - Each point's facing, where a fold across it is, is the way its silk runs on from it to the next ring (from the
      ring before to the ring after shades a fold on one ring flat), so that a fold across it is a darker band and a
      lighter one; chutes.py lights each pixel of the silk by its facing there, as the scene does.
    - land_air_test measures it from above (`FromAbove`: its silk drawn straight down on a 2 cm grid and lit as chutes.py
      lights it; its luma across its middle reads within 2 of chutes.py's for his and the second pilot's) and checks it
      over the verdict's four pilots and 300 others. Over the 300, against the code before her note and this round's
      first answer to it: (a) its silk more on one side of its wind line 0.09 to 0.74 (0.33 on the mean; it was 0.00 to
      0.62, 0.21, then 0.00 to 0.63, 0.22), and about the line to its tip 0.09 at the least (0.31 on the mean; it was 0.00
      and 0.12, then 0.00 and 0.15); (b) its edges 4.1 cm off a smooth line on the mean (2.6, then 4.1) and its hem 0.28 m
      off a smooth curve at the least (0.37 on the mean; 0.11 and 0.22, then 0.19 and 0.36); (c) three or four folds
      across it for 244 (2 for 2, then 293), two light bands or more for 216 (none, then 217); (d) its far end's lift 0.18
      to 0.41 m to one side (0.29 on the mean; 0 to 0.04, then 0.05 to 0.20); (e) one to four of its lines across its silk
      (2.3 on the mean; none, then 0 to 5, 1.9), none under it by more than 6 mm (the first answer's went 7 cm in); and
      its luma across its middle 116 to 231 at the most. His: 0.38 more on one side of the wind line (0.29, then 0.09),
      three folds across and two light bands (one and none, then three and one), his far end 0.29 m to one side (0.03,
      then 0.08), three lines across (none, then two, one of them 1.6 cm under the silk), luma 160 to 217 in chutes.py
      (126 to 221, then 129 to 222: his middle now falls on a fold's light band; 0.9 m either side of it its troughs
      read 119 and 123, a pixel's own). The second pilot's: 0.19 to one side (0.04, then 0.11), three folds and two bands (one and none, then four
      and two), its far end 0.34 m over (0.01, then 0.17), two lines across (none, then three), luma 124 to 217 in
      chutes.py (127 to 217, then 121 to 213).
    - After her second look it measures as well (e) its lines across its body: how long each lies on its silk past 0.15
      of its length on from where its lines gather, how many 0.3 m or more; for each laid across it how long it lies on
      its silk, how steeply it crosses its wind line, how far along it reaches, how near it passes where the rest gather
      on its way, how far from his harness it comes onto it, how high its middle stands over its silk's top (the exact
      top, each triangle facing up) where it runs over it, and how long it is drawn against a real line (`RealLines`);
      and (f) its far end's breakup: its vent off the middle of its far fifth (a share of its half-width), a crease
      across its dome (the deepest its top dips along it, 16 cm or more in from its edges), its last two rings folded
      back under at an edge (how much farther out its top layer reaches than its edge's seam), and its seams bending
      ring to ring over its last four rings. Each pilot (and each of the verdict's four) has one to four lines 0.3 m or
      more across its body (his and the second pilot's two at least), each laid across it 1 to 3 m on its silk, 18
      degrees or more off its wind line, none past half its length, none within 0.1 m of where the rest gather, coming
      onto it within his lines' length of his harness, its middle within 2.6 cm of its top on the mean, more than 4 cm
      over it for an eighth of it at most and 25 cm at the most, and none drawn longer than a real line; and its far end
      folded under 0.1 m or more, creased 5 cm or more, and its vent 0.15 of its half-width off its middle or its seams
      bending 0.1 m. Over the 300: 1.8 lines across its body on the mean and none with none, the longest on its silk 1.3
      to 2.4 m on the mean and over 2.6 m on a tenth at most, their least slant 35 degrees on the mean, their middle
      within 2 cm of its top on the mean; its vent off 0.22 on the mean (no more than 20 under 0.1), creased 9 cm (the
      neat dome before had measured 4.8), folded under 0.2 m and its seams bending 0.16 m on the mean. And on 60 of
      them, as it settles (0.8 to 0.95 of its fall), none of his lines under its silk's top by 8 mm; and on the 11
      fields, falling in and lying, none of his lines under the ground. Over the 300, against the code before her second
      look and this round's first answer to it: (e) one to three of its lines across its body 0.3 m or more (1.92 on the
      mean; none, then 2.19), 1.61 to 2.93 m of each on its silk (its longest 2.34 on the mean, 25 over 2.6 m; first
      1.21 to 2.95, 2.04), 20 degrees off its wind line at the least (46 on the mean), their middle 1.5 cm over its silk
      on the mean (2.1 at most on one pilot's mean; first 4.4) and 16 cm at the most (20), drawn 1.00 of a real line at
      most (first, 1.62 of its own length, some 5.5 m); its old measure's lines across its silk 2 to 6 (4.2 on the mean;
      1 to 4, then 3 to 6), none under it; (f) its vent 0.27 of its half-width off its middle on the mean (0.12, then
      0.29; 18 under 0.1, 109 and 14), a crease 0.066 m deep at the least and 0.132 on the mean (0.004 and 0.048, then
      0.060 and 0.114), folded under 0.15 m at the least and 0.34 on the mean (0.00 and 0.01), its seams bending 0.071 m
      at the least and 0.214 on the mean (0.025 and 0.107); (a) to (d) as the first answer had them. His: two lines
      across its body, 2.14 and 2.36 m on its silk (first, 2.38 m the longest), 42 degrees off its wind line at the
      least, their middle 1.5 cm over it on the mean and 4.3 cm at most (3.3 and 10), drawn 0.89 of a real line; its
      vent 0.46 off (0.22, then 0.47), a crease 0.121 m (0.030, then 0.100), folded under 0.44 m, its seams bending
      0.250 m; luma 160 to 217 in chutes.py as before, his lines now darker than his silk and hidden where it hides
      them. The second pilot's: two lines across, 2.41 and 2.43 m (2.16), 43 degrees, 1.8 cm on the mean and 5.1 at most
      (2.8 and 8), drawn 0.82; its vent 0.25 off (0.27), a crease 0.147 m (0.113), folded under 0.35 m, its seams
      bending 0.200 m; luma 123 to 217 in chutes.py (as the first answer). At eye height his far end's top stands 0.31 m
      0.9 along it, 0.31 at 0.95 and 0.20 at 0.98 (0.44, 0.34 and 0.23 before her second look; 0.27, 0.31 and 0.18 at
      the first answer), and by the time it lies 62 of its 5200 triangles are folded over (46, then 80).
  - Its own dice (`0xC47E1` as before; the notes' draws on `0xC47E3`; its lying folds on `0xC47E5`, drawn after all
    the rest; its look from above on `0xC47E7`, the pilot's number's own; its lines across it on `0x57AE` and its far
    end's breakup on `0xFA2D`, over the pilot's number): the planes fly as before, frame for frame.
- **Not yet.** The engines' and guns' sound (the sound director's), the bombs seen falling, light flak's puffs (the kit
  has only its heavy row).
- **Test**: `Tools/land_air/run.sh [out folder]` (`LAND_AIR_OUT`; built as the Mac builds it, `-ffp-contract=off`, clang
  by default, `CXX=g++` the other; its chute_silk.txt and air_paths.txt the same byte for byte either way, and with
  clang's `-mfma -ffp-contract=on`): the feed, the machines, the plan, the flight (drawn
  from before the run, at a fighter's speed, never under 5 m over the ground, smooth, the Stuka steep and the Il-2 low
  while firing, the art director's dive angles, bottoms and gun ranges), the guns (each lane from the strip's start to
  its end, aimed on the ground, each plane 6 to 12 s after the one before, a lane 150 m past its own side's front), the losses (each falls as its note says, burns where it struck, no other falls), the ground's fire (only the
  other side's blocks, each stream ending in the air near a plane), the same (built twice, grown note by note, rounds
  the same however the frames fall) and no NaN; `air_paths.png`, three strikes side-on and from above
  (`paths.py`); the chutes (a pilot out of the burning and the spinning plane, none out of the one blown apart; out
  after the hit and before she strikes, his canopy full well up, down at 5 to 6.5 m/s, never under the ground; flat,
  up onto his knees, his harness, up, running from the enemy, flat; his canopy fallen in downwind, its middle 4.5 to
  9.5 m off; the same twice). The art
  director's notes, on that field's pilots and on a second, windy one's (5.7 m/s, an Il-2 losing her pilot 35 m up):
  falling free never above where he left her and at least half g t squared under it (first); with the air (within
  1 m/s of the wind) within 3 s of his pull, his way only dying away against the air, with the wind still 3 s after
  his canopy is full, which is 40 to 75 m under where he left her (second);
  drawn in his chute's mesh till he lies flat under his falling canopy, then the crowd's man (third); up onto his
  knees, kneeling 1 to 1.5 s, then `stand_up`, and in a wind over 4 m/s dragged 1 to 2 m downwind first, none in a
  light one (fourth); out under 60 m a hard landing, his canopy still filling, lying 3 to 5 s and limping off slower,
  the rest soft (fifth); his swing about 4 s, 14 degrees settling to about 5; his canopy 5 to 5.5 m across, about half
  as deep, 24 to 28 gores, faint seams, a line at each, both faces, her off-white within 4%, and its glow (sixth). And
  a review's of 24 November: the scene's mesh at 45 fps, made at most 30 times and never in the air once his canopy is
  out; at the swap he is drawn every frame, in the mesh or as the crowd's man, both (in place) one frame at most, the
  mesh keeping him till the crowd has drawn its man (whose mesh waits when a frame's `MenRebuildsMax` is spent; held
  at most `LandAir::RiderHoldFrames`, 30, and tested with the crowd 0, 3 and 25 frames late); the silk moved in place
  only from one shape in the air to another (a heap's windings never carried into the air); a
  canopy filling never folds a triangle over, and his lines run back from the same half of its skirt; flat in the
  mesh his hips lie where the crowd's man's do in every soldier file that has `go_prone`'s end offset (18 of 19; read
  from `Content/Models/Soldiers`, `LAND_AIR_SOLDIERS`); each clip blended into over 0.25 s, the harness's kneel 0.5 s;
  he lies the way he faced as he landed. And her verdict of 24 November, 14:34Z, on every test pilot: side-on his
  canopy widest a quarter to a half of its depth under its crown and its hem 0.8 to 0.9 of that, its silk leaning in
  toward its lines ((c)); lying 2 to 3 times as long as wide, its far end up 0.3 to 0.8 m, its near end 2 to 3.6 m
  past his harness and its lines running back to him; in a few long folds along it (most of them going on along it, 2
  to 7 across a ring), their heights uneven and their spacing uneven (spread over mean 0.2 or more, each on its own
  bar); another pilot's number lying apart (0.08 m or more, the root of the mean square); falling in, its middle never
  back toward him and its top never rising, half-way longer along the wind (by 0.5 m or more) and its middle 1.5 m or
  more on ((b)); and over 300 pilots' numbers none off. And a second review's of 24 November: side-on its skirt
  curving in all the way down from its widest (its rings leaning in more one by one, its silhouette under 0.985 of its
  widest 0.6 of its depth down) and its crown round (under 0.65 of its widest 11% under its top); lying, its lines
  none longer than they are, its far end coming round and down (its last ring a quarter to three quarters of its far
  end's height, its tip within 0.08 m of the ground); falling in at 45 fps as the scene draws it (`SilkStep`), its silk
  moving under 0.3 m a frame and at a make, within 0.1 m of its true shape, under one in two hundred of its
  triangles' frames showing their inside, and its makes' points, triangles and time printed (and on hummocks); none of
  it under the ground, falling in or lying, its points nor its triangles' edges (seven places along each against the
  ground itself), on a slope of 1 in 4 along the wind or across it, on hummocks, across a hedgerow bank 1 m high, a
  step 0.4 m up, a ditch 0.6 m deep, a sharp crest, a crater's bowl and rim, and along a bank under one edge; and
  lying on the slopes its underside on the ground (each ring's lowest point within 0.08 m of it). On the same fields
  none of his lines under the ground, drawn from his harness where the scene draws it (not lifted onto the ground under
  it, which had hidden lines 0.23 m into hummocks), for the verdict's four and, each 0.05 of its fall, 300 pilots'
  numbers (their canopies' points too; on those fields and with the bank along it moved 0.5 m, 1 m and 1.5 m either
  way): the lines across its silk 0.015 m over it at the lowest (before this round's
  second review, 0.23 m under it); and for the 300, made as the scene makes them (`SilkStep`) and looked at each 0.0125
  of the fall, between makes as well: none of his lines' middles more than 3 mm under the ground there (those across
  its silk 0.0064 m over it at the lowest (0.0074 after the fourth review's mend, across the ditch's lip as one ran down
  into it falling in, and 0.0148 before it, its silk laid differently; before the third review 0.0004 m under it on
  hummocks, 0.0007 at 45 fps). With the silk laid lower round the bank's crest after the fifth review, one had gone
  4.7 mm into it between makes, its points sliding 0.7 m along it across the crest: each make keeps them over the ground, so `HeapShape`
  is handed the make before's (`SilkMesh::LyingKept::OverWas`, by `SilkStep`) and lifts its own where the straight from
  those would pass under the ground the silk lies on; and lines across the silk running out past the grid's inner
  points are kept over the ground as read there, where the grid reads a crest at its rim as flat (one 1.2 mm in the
  bank moved 1.5 m left),
  and the points his lines across its silk go through across the grass over both the taut straight from his harness
  and the ground, at makes and between, and once slack over the ground alone, no higher than each field's own bound, a
  few centimetres over what it gives (over both: now 0.39 to 0.54 m at most on the slopes, the step, the sharp crest
  and the crater, 0.50 on hummocks, 0.70 across the ditch, 0.71 over the hedgerow bank and 1.13 along the bank, where
  they come up onto its silk, 1.01 to 1.19 with it moved 1 m or 1.5 m; before the third review 7.0 m along the bank and
  1.9 on hummocks, 4,400 looks over 1.5 m, and one bound of 1.75 m for all had let a cord stand 1.7 m up where they lie
  under 0.85 m (a fourth review); over the ground alone 0.31 to 0.42 m, 0.54 and 0.65 on the sharp crest and the
  crater, 0.94 across the ditch, 1.55 over the hedgerow bank and 1.01 to 1.44 along the bank). With the bank moved
  0.5 m either way, where his harness lies on its shoulder, they still stand 1.88 and 2.07 m over both (1.91 and 2.19
  over the ground once slack), as before this round: bounded there, not cured. And lying (0.8, 0.9 and 1.0 of its
  fall) its crown's tip no more than 0.05 m over the other corners of its fan, on every field and with the bank along
  it moved (now at or under them; before the fourth review up to 5.1 m, 75 to 98 pilots of 300 over 0.3 m with the bank
  along it), and its silk no higher over the ground, nor over the highest ground within 0.6 m of it beyond where it
  lies on flat ground, than each field's bound (over the ground now 0.71 m on the slopes and hummocks, its far end's
  lift, 0.87 to 0.95 on the step, the ditch and the crater, 1.32 over the hedgerow bank, 1.30 to 1.46 along the bank;
  before, 2.39, and 4.9 to 7.1 along the bank). At 45 fps his
  lines as drawn move up to 0.52 m a frame between makes and 0.43 m at one (their run across the grass swinging as
  where they come onto its silk moves, at a slump of about 0.4): printed, not checked, being over the silk's 0.3 m. For
  the 300 (measured, not in the test) 0.56 m on the flat field, up to 0.63 m on the other fields but the bank's, and
  0.91 m along the bank, as its taut lines follow its silk falling in (1.74 m before the third review). The planes'
  hash is printed against release 1's (`e258eaf3320235ff`). `chutes.py` draws each pilot's first seconds, his way through
  the air and his seconds on the field, and the canopy as the scene draws it: side-on full through a long lens with
  his lines to him, from under with its glow and without, falling in across the wind, lying from 40 m at a man's eye
  height, and lying from above (his and a second pilot's, his lines a little slack as the scene draws them),
  measuring each from its pixels (its widest and its depth, its hem, its silhouette's width 0.5 and 0.6 of its depth
  down and its crown's 3% and 11% under its top; its middle falling in; its length, its far and near quarters'
  heights, its far end's top over its last twentieth and its silk's sRGB luma at 40 m; its length and width and its
  folds' light ridges from above).
  Pictures: `/mnt/project-files/battle-fx/air-support/` (`chutes_r1.png`, `chutes_r2.png`). The bareheaded rider:
  `Tools/land_show/run.sh` (every infantry file's rider without his helmet at each level, none drawn in its place).
  The crowd's man each pilot becomes (`Crowd::PilotDress`, `AddRider`; with `Battle/LandMen`): a USA or GER pilot in
  the tankers' file as it is, a SOV or UK pilot in his infantry file bareheaded and without his pack, at every level his
  file's triangles less the helmet's and the pack's runs (a file without `pack_tris` keeps his pack, and the line says
  so); `LAND_AIR_SOLDIERS` reads another folder. Run on the soldiers thread's files (fc61734e), only the swap's line
  changes: "36 soldier files read" for 19, its gore twins being in that folder (none has `go_prone`'s end offset).
- **Owed.** The Mac shots behind the switch, the art director's look, and the 45 fps gate with a strike overhead.
  With `-TheaterLandChutes`: a pilot under his canopy from 100 m and from 300 m, his landing and run, and his canopy
  lying on the field; and for the notes of 24 November: one from below at 100 m with the sun behind the canopy (its
  glow and its seams), a pilot falling free and his canopy filling, a hard landing out of a low Il-2, the swap from his
  chute's mesh to the crowd's man (shot 7: that the man lying prone in `go_prone` lies where the mesh's man went over,
  no jump and no frame without him; the numbers are tested, the look is not), his harness kneeling (shot 8: that he
  settles into his kneel off `go_prone` without a lurch) and the drag in a wind over 4 m/s, the limp, and an infantry
  file's rider bareheaded (its scalp under the helmet is the file's own skin). The 45 fps reading: the log, once each
  pilot's canopy lies flat, tells his mesh's makes and moves in place (24 and 151 to 167 here), his silk's points and
  triangles lying (2,290 and 4,368 on 26 gores here), its makes as it fell in (21) and their time (ms a make, and the
  longest: about 0.13 here on a flat field, 0.3 on hummocks, on the cloud box; the Mac's own on a real field is
  owed), and the most its points moved in a frame (0.22 m here; over 0.3 m would show it jumping). For her verdict of
  24 November, 14:34Z: a pilot's canopy side-on from about 200 m on a long lens (its skirt curving in from its widest
  to its hem, its crown round, its lines running on from it to him), his canopy lying from 40 m at a man's eye height
  and from above (stretched along the wind in loose uneven folds, its far end lifted and coming round and down to the
  ground, its near end gathered 2 to 3.5 m past him and his lines a little slack across the grass, two pilots' side
  by side to see they differ; and for her note of 17:40Z, an uneven outline and hem, folds across it, a few of his
  lines lying on top across its body and sagging into its folds, none sunk in it, and its far end broken up, its vent
  off its middle, a crease across it and one edge folded under), its falling in after he lands at 45 fps (tipping and blowing downwind, going slack and
  crumpling, flapping until it is down, moving every frame with no jump at a make, not shrinking in place nor a
  smooth torpedo half-way), one lying on a slope (none of it under the ground, its silk not floating either), and
  one coming down across a hedgerow bank and one into a shell hole (its silk over the bank's crest and the hole's rim
  and lying down into its bowl, none of it through them: the test's fields are its own, the scene's ground and holes
  are not).
  - *Her second note, as read* (her words: "Pilot 11 drops about 50 m straight down before the wind takes him. Once
    his own way is spent, he drifts with the air within 2-3 s. The wind isn't a push that builds from rest."). Its
    first sentence is release 1's fault, as her first note's first sentence is (his way died against the ground, so he
    fell straight down and the wind then pushed him from rest). As built his way dies against the air from the first,
    so he never falls straight down: the test's pilots are with the air 2.2 to 3.0 s after their pull (2.5 to 4.3 s
    after leaving her, carried 50 to 240 m across first), and the test holds that to 3 s. The 40 to 75 m to a full
    canopy is its opening's loss, kept. Her Mac shot of a pilot falling free and filling judges it.
  - *Her third note's "no pack"* (round 2 had it waiting on the soldiers' thread): done in the code. The soldiers'
    thread marked each infantry file's pack and webbing as one run of triangles, `pack_tris` (claude/project-thread-
    labtrr fc61734e: the 19 men and their gore twins; the game's Content has 34 of them, not yet brought in here: the
    ground thread merges both). Until those files come in, the game's Content has no `pack_tris` and the infantry
    fallback still wears his pack, exactly as before; the log says "his pack on (his file marks none)".

## Men out of their machines without their packs (release 2, behind the chutes' and the bail-out's switches)

The art director's notes: downed pilots (24 November 02:26Z, note 3: "On the ground: no steel helmet, no rifle, no
pack. Use the tanker files where they exist (USA, GER) ... Other armies get infantry with the helmet hidden"; round 2,
14:34Z: "The pack waits on the soldiers' thread") and tank crews bailing out (23 November, note 1: "other armies own
men, no rifle/pack, pistol at most", built as infantry with the rifle hidden and the pack on, for want of a range).

- **The files' run.** The soldiers' thread (fc61734e) marks his pack and all his webbing at each level as `pack_tris`
  `{near: [first, count], mid: [...], far: [...]}`, in `helmet_tris`'s shape, each ending where his helmet's run
  begins: pack, belt, Y-straps or braces, pouches, bags, canteen, shovel, bayonet, gas-mask can and rolls; the radio
  and its handset; the loader's rocket bag and rocket. Buttons, badges, the medic's armband, the chin strap and the
  weapon stay drawn. "Leave that run out and draw nothing in its place, because the cloth under it is whole." Only the
  order of the triangles between his hands and his helmet changed (the weapon's before the kit's); points,
  `helmet_tris`, clips, deaths and the gore cut points are as they were. `Soldier::PackFirst`, `PackCount` read it,
  0 where a file has none, or its run is not two whole numbers lying within the level's triangles, or it reaches into
  his helmet's run (none of fc61734e's does; were one to, a crewman with his helmet on would lose part of it, so such a
  run is left alone and he is drawn with his pack and all his helmet).
- **The mesh.** A man's mesh at each level is a variant of his file's (`Crowd`'s `VariantOf`, `FaceRestOf`), now keyed
  by the runs it leaves out (his own helmet, his pack, both) instead of by his own helmet alone: each run's triangles
  are cut, and only the points nothing kept uses are dropped (as the helmet's cut always did), the rest in the file's
  order. A pack cut where the file marks none is no cut: he shares the whole man's mesh. The whole and the helmet-only
  variants come out as before to the byte (the crowd's men and the riders, whole and bareheaded, hashed against the
  code before, on Content's files and on fc61734e's). Nothing else of the crowd indexes his triangles: the faces and
  their tints, a gore twin's joined skin and its caps go by points (unchanged), the rifle is hidden by its bones
  (`bUnarmed`), and the far men's bank and the vertex-animation sets are only ever the crowd's men, who keep their
  packs. A gore twin's pack run cuts as its man's (its caps whole: no cap point is a pack's alone; of its joined skin's
  points, a few with the pack, skipped as the helmet's are).
- **Who goes without.** `AddRider`'s `bNoPack` (beside `bBareHead`, false by default). `Crowd::PilotDress`: a downed
  pilot in the tankers' file where his army has one here (as it is: those files carry no pack range), else his army's
  infantry file (`RiderFileOf`), bareheaded and without his pack (`ALandBattle`'s chutes, LandBattleAir.cpp).
  `Crowd::CrewDress`: a tank's crewman bailing out in the tankers' file, else his army's own infantry file, his helmet
  on and without his pack (`ALandBattle::BailOut`). The tank commanders in their hatches and the light cars' riders
  are as before. With both switches off nothing changes, and a file without `pack_tris` draws every man as before.
- **Test**: `Tools/land_show/run.sh` builds `land_pack_test` (with `-Wconversion`, g++ and clang) and runs it on the
  folder it reads (`LAND_SHOW_SOLDIERS`, else Content's), against Content's own files as ones without the run. In every
  file that marks its pack, at every level, four riders of one seed and ride (whole, bareheaded, no pack, both): each
  look the file's triangles without its runs to the last index, the pack's not drawn and nothing in its place, only
  points no kept triangle uses dropped, every kept point skinned and coloured as the whole man's to the bit, the helmet
  and pack cut the two runs cut at once, the whole and bareheaded men as before; a gore twin's caps whole; a file's run
  reaching into his helmet's failed; made-up runs not whole, out of range or reaching into the helmet's ignored (one
  ending where it begins, or just after it, read); Content's files without `pack_tris` drawing a rider with no pack the
  same to the byte; and each army's `PilotDress` and `CrewDress`. It also reports, without failing, the triangles of
  each file's near run in the helmet's paint at his collar and shoulders (`CollarInRun`: the neck, head, collarbones,
  upper arms or the chest's front), which should be none (see the picture's *lost with the pack* below; the files' to
  mend). On fc61734e's 36 files: 34 mark their pack (17 gore twins), 102 levels, every run ending where the helmet
  begins, none reaching into it, 136 riders; at the collar CHN, FRA, GER, JAP, POL and UK 24 each, ITA 72, SOV_snow
  268, and their gore twins alike. On Content's own files every line of the land show
  tests is as before but the timings (and the new test's). On fc61734e's folder, on both fields, only these differ, and
  none for the pack: each file's size and clip count (GER's 93 clips to 118, USA's 96 to 121, ...); the seat check's
  count (621 poses of 207 seat clips to 1188 of 396) and its 108 failures, the soldiers' thread's new clips for getting
  out (`out_tail_a`/`_b`, `out_doors_a`/`_b`, `out_door_a`/`_b`, `bail_climb_out`), which leave his seat as they
  should but which that check reads as clips he sits through (the ground thread's to mend when it brings the files
  in). Its parent's files give the same lines but each file's size (508 or 509 bytes less: the `pack_tris` key). The
  bank, the vertex-animation sets, the frames and the play are as on Content's. `Tools/land_gore/run.sh` with that
  folder's twins fails one line, as it does with the code before this and with the parent's files: a gored man's whole
  joint opens 2.2 mm on its caps (the twins', not the cut's). `Tools/land_air/run.sh` and `Tools/land_bail/run.sh`
  check the crowd's man a pilot and a crewman become (above); on fc61734e's folder `land_air` reads 36 files for the
  swap's lying pose (19 in Content), its other lines as before.
- **Picture**: `Tools/land_show/pack_picture.sh <soldiers folder with pack_tris> [out folder]` (`pack_off.png`): the
  Russian and the British pilot down (bareheaded) and crewman out (helmet on), rifle and pack off, near from the front
  and the same as before with his pack, to compare his front (the review's, 24 November), near from behind, mid from
  behind running, and near from behind as before with his pack, to compare his back. Seen there on
  fc61734e's files: the baked shade keeps the pack's marks on his cloth, as the soldiers' thread said: on the British
  back a darker yoke across the shoulder blades where the pack's top sat, a darker band at the waist where the belt
  ran and a darker patch over the small of his back, and on his front dark blotches on his chest beside the buttons
  and round his hips where the braces and pouches lay; on the Russian a faint dark diagonal from his left shoulder to
  his right hip where the greatcoat roll lay and a band at the waist, his front nearly clean; at mid only the British
  back's yoke, faint. Under the British pack a small dark notch shows below his collar (cloth folded in
  where the pack hid it). Not new, and as before for a bareheaded man: his helmet's chin strap is drawn (not in either
  run), two thin straps standing up beside his ears, and his scalp is bare skin.
  *Lost with the pack that should not be* (the review of 24 November; a fault of fc61734e's run, not of the cut, which
  leaves out exactly the run the file gives): `pack_range.py` takes a piece more than 2.6 cm across for kit, so some
  pieces in the helmet's paint (paint 5) that are badges or cloth went into the run, against the soldiers' own note
  that badges stay drawn. The collar tabs, two small dark-green pieces at the front of his collar on the neck and chest
  bones (UK, POL and FRA near triangles 7283-7306, GER 6929-6952, CHN 6736-6759, JAP 6716-6739: 24 at near and 8 at
  mid each, CHN 10, their gore twins alike): a British, Polish, French, Chinese or Japanese pilot or crewman without
  his pack loses them, and so does a pilot whose army borrows one of those files (FIN's, the German's). The British
  crewman's front pair on the sheet shows it, small at this size; a close-up from the front, pack off and on, shows it
  plainly. Of the same kind, found with it: the Italian's collar patches on his lapels (ITA 8534-8605,
  72 at near, 14 at mid, nearly his tunic's colour: his stars stay, the patches at the top of his lapels go), and the front hem of
  the snow suit's hood on his shoulders (SOV_snow 8531-8798, 268 at near, 40 at mid, 8 at far; the hood's piece that
  runs on into the helmet's was kept, these separate ones were not), which no man out of his machine wears today (a
  Russian pilot in winter is drawn from SOV_winter, a crewman from SOV). The helmet's paint lower down is kit and
  rightly in the run (the German's gas-mask can on his hip, the pieces on the American's back). Left as the
  files give it here, until the soldiers' thread keeps those pieces out of the run (or puts them after it) in
  `pack_range.py`: no code changes then, and `land_pack_test`'s collar line reads none.
- **Owed.** From the soldiers' thread, the collar tabs, the Italian's collar patches and the snow hood's hem kept out
  of `pack_tris` (above: against their own note, so theirs to mend before the art director's look). The soldiers'
  files in Content (the ground thread's merge); then the art director's look on the Mac with
  `-TheaterLandChutes -TheaterLandAirSupport` and `-TheaterLandBailOut`; the shade's smudges and the chin strap are the
  soldiers' thread's to mend if she asks.

## Old wrecks on a field fought over before (release 2, off unless asked)

Brian, 23 November 20:09Z: the burnt-out wrecks of a battle stay on the field for the next battle there. Looks only,
as the scars and the old ruts are: nothing the rules fight reads them, no man takes cover behind one, and they are
kept off every way the recording's men and vehicles go and every place its rounds land, so no round of a recorded
battle strikes one and nothing the battle shows is moved by them. A live battle's ways and rounds are not known at its
open: its rounds land where its rules send them, on a wreck too (the burst is drawn there, no hole dug under her), and
its wrecks are kept off the ground its men and machines are likely to cross, and one any of them is sent near all the
same is cleared away before he gets there, the moment the eye cannot see her go where it can ("In a live battle"
below, with what the guard does not read).

- **What asks for them.** The campaign writes the recording's root `"oldWrecks": {"count": n, "months": m, "nats":
  {"GER": n1, "SOV": n2, ...}, "per": p}`: the real armoured vehicles burnt out here before (60 at most), the months
  since the last of those battles, whose they were, and this battle's scale (the real armoured vehicles each tank
  model drawn in it stands for, to a tenth; left out where no tank is drawn). It leaves the key out where none burnt
  here, or where the player's Battle rules row "Old wrecks" is off (sgfei1 b0a47250, src/battle/land/wrecks.ts).
  `LandFeed` reads it with any part missing or odd (a count over 60 is 60, below nothing is none, a nation's count
  must be a number over 0, a per missing, not a number or under 1 is 1 and over 1000 is 1000;
  `Recording::OldWreckCount`, `OldWreckMonths`, `OldWreckNats`, `OldWreckPer`). On the
  Mac `-TheaterLandOldWrecks=N` and `-TheaterLandOldWreckMonths=M` stand in for it on any field (the nations then the
  battle's two sides', the attacker's the odd one), and `-TheaterLandNoOldWrecks` lays none. The stand-in's N is how
  many are laid (at most 20; `Ask::bAsLaid`), not how many burnt out: its months age their look and the scrap men take
  none away, so the Mac's off/on shots show N at any age (the key's count is the burnt-out armour, salvaged as below;
  the switch was first written to read N as the key's count, and lay 6 of `=40` at two years). **With neither the key
  nor the stand-in nothing is made, read or said**: `LayOldWrecks` returns before anything, the grass's `Covers` and
  `ShowOldWrecks` find an empty yard, and no die of the battle, the scars or the ruts is moved (the wrecks throw their
  own, seeded on their own constants). A live battle (every battle the campaign plays) lays them too, read from its
  header as below; the campaign's live header carries the key from the TS side's 8ddab648 and 7ec111e2 (live.ts,
  on `claude/project-thread-pi6nal`), which this branch's `src` (07bf753a) does not have yet, so here a live battle
  lays them on the stand-in alone till the two meet. (Before 25 November a live battle laid none and said so.)
- **How many** (`LandHulk::Models`, `Left`, `Kept`). First the battle's scale (Brian's rule: one model stands for as
  many real things in every part of a battle): the count's real machines are drawn `per` to a wreck, `Models(count,
  per)` = count / per rounded, at least one where any burnt (an armoured division's 150 drawn as 20 tanks is 7.5 to a
  tank, so the 60 a campaign can carry are 8 wrecks; 3 burnt out at 10 to a tank are still one). The stand-in's N is
  laid as it is and reads no scale. Then the recovery companies took what could be mended within weeks and the scrap
  men the rest over a year or two: all of them lie there within three months, half at a year, 15% at two years, 6% from
  four years on, in straight lines between. At most 20 on a field (each a static mesh of some 70,000 to 95,000 points
  at its near level, so twenty are about as many points as the dry run's own armour; the Mac's 45 fps gate judges it).
- **Which machines** (`ModelFor`). Each nation's tanks as the month they burnt in had them, by live.ts's `standInModel`
  mix (and the Panther from 1944, as shells.ts draws it): the German Panzer IV, StuG III, Tiger and Panther, the
  American Sherman, the Soviet T-34, the light tank for the early war and for others; one in five a half-track (the
  German Sd.Kfz. 251, the American M3) or a lorry. The battle's own models where it has them and the wrecks burnt in
  its own month (they are dressed for its coats and kit); else each read as `MakeArmour` reads them and dressed for the
  month she burnt in (a German wreck of the spring in a summer battle wears her spring coat under the soot). A model not
  at hand is drawn as the nearest that is, and said in the log line.
- **Where** (`Yard::Lay`, on `LandHulk::FieldOf`: the scene and the cloud test read the field the same way). In small
  groups, a platoon's one to four 13 to 28 m apart, one group a fight: along the field's old lines (a run of hedges,
  walls and lanes across the fought-over ground in one heading, as the scars' fronts, 12 to 34 m out on the side the
  attacker came from, most of the time), at the lanes' junctions and the village's edge, along the battle's own axis
  (the way its machines went, through their middle), a few strewn. A side's wrecks face the other side's ground, some
  turned as they were hit, a few going back. A field with no old lines (the open steppe) gives their share to the axis.
  Each is kept off the lanes (6 m from a lane's middle: the engineers cleared them), the banks, walls and woods' edges
  (3 m), and never lies in a hedge, a house, a yard or garden, a wood, water, marsh, standing corn, an old shell hole,
  an old rut or across a cattle's path; 8 m from every recorded vehicle's way at any time in the battle (her middle),
  1.5 m from every man's, 4 m from where any shell, bomb or grenade of this battle lands (its hole would open under
  her), 1 m from a field telephone's wire (the recording's, or `-TheaterLandPhones`'s made from the battle: a lineman
  lays his round her), 2 m from another wreck. Her scorched patch, which reaches some 2.4 m past her outline, lies on
  open ground too, every point of it (the steppe's first lay had three patches over a trench's works and one over a
  garden).
- **In a live battle** (25 November, and its review's mend the same day; `LandHulk::Field::Likely`, `FromCore`,
  `FromLikely`, `Yard::Reached`, `Clear`, `InSight`, `Cleared` and `Over`, `LandHulk::Sight`;
  `ALandBattle::GuardOldWrecks` and `ShowOldWrecks`). A live battle opens with its header alone: the two zones, the
  blocks (each one's kind, frontage and depth, side, and whether it joins later), the town, the setup step's room; no
  way, round or blast yet. And the rules' drill has no way round a still machine (it drives this battle's own wrecks
  through), so the wrecks are kept off the ground its men and machines are likely to cross, and any one a live piece
  sends someone near all the same is cleared away before he gets there. `FieldOf` reads that ground from the header in
  two parts. Its core: each side's blocks where the rules draw them up (units.ts `deploy`'s plain `drawUp`, modelled
  here: the foot in a row 40 m in from its zone's edge facing the enemy, 40 m between blocks (`GAP`), the attacker's
  about its zone's middle, the defender's about its town kept 300 m inside its zone's ends; the tanks and lorries 110 m
  behind, half on each wing past the row's ends where they fit on the field, else centred; a defence of four blocks or
  more in its line holding a quarter back, 110 m behind or 170 where its tanks stand centred; the guns 230 m behind;
  with no foot, half the tanks in front; each kept on the field as `drawUp` keeps it; a corps from next door left out),
  each block's box grown 60 m (`StartClear`); where the rules draw a side up otherwise (a river's bend or lanes, a
  second river, a city's streets: a city's, a capital's or an urban field, or one with water, a bridge or a blown one,
  suburbs, industry or parks) its whole zone grown 60 m instead; the setup room grown 60 m and its widest block's half;
  the ground between the two fronts as drawn up (each from its westmost block to its eastmost, wings and all) and 100 m
  round it (`AdvanceMargin`); where a block joins later, a 300 m strip along every edge of the field (`JoinClear`;
  sim.ts `joinEdge` and `walkOn`: the corps from next door walks on some 200 m by the edge facing where it comes from or
  by its own back edge, never the enemy's back edge, so every edge is someone's); and 60 m in from the field's edges
  (`EdgeClear`). The rest of it, round the core: each side's whole zone grown 60 m (its blocks shift in it), and the
  ground behind each side's front and the advance to its own back edge, 60 m wider than its front (sim.ts: a block that
  falls back goes straight back to its own edge, one broken runs straight back off it). Against where the recordings'
  units stood at their start (the test's case 9): every block's middle (the mean of its men's and machines' first
  points) inside its modelled box, or 2.2 m out of it at the most (Prokhorovka's attacker's); its men and machines out
  of their boxes by at most 30.0 and 36.4 m on Normandy, the attacker's and the defender's, and 30.0 and 36.2 m on
  Prokhorovka (a battery's lorries behind its guns, its observers forward with the line, its men in their formation),
  all inside the 60 m kept round them. The battles as fought against it: 153 of Normandy's 230,868 kept points of its
  men's and machines' ways off the core (38.3 m at the most) and 25 off all of it (9.4 m); 207 of Prokhorovka's 251,622
  off the core (99.5 m), none off all of it. (Round 1 kept each whole start area grown 60 m and drew each front across
  its zone's middle, its blocks' frontages side by side with no gaps or wings; its "within 18 m" of the ways had no test
  to print it.) The ground between the grown start areas off the core, every 25 m, stands for the ground fought over
  (the box the groups are laid in). They are laid as on a recorded field, but: the groups along the old lines, at the
  junctions and at the village's edge off the core only, so on the rest of it too (the village and the lanes behind a
  defence's foot lie there); the axis's and the strewn groups off all of it; each wreck's every ring off that by her own
  reach and half a metre; 15 m from a lane's middle, not 6 (`LiveLaneClear`: the live battle's machines drive its
  lanes); their groups at least 100 m apart (`LiveGroupApart`; the first lay on Prokhorovka put all of them in one patch
  some 100 by 260 m); and the axis's groups out on its flanks, either side, 15 to 250 m past the likely ground's edge,
  still facing the other side's ground. So they read as an earlier battle's: along the hedges and walls beside the
  advance's path and behind the lines, at the junctions and the village's edge, out on the flanks, facing the other
  side.
  The guard: `GuardOldWrecks` (from `PumpLive`, after `MapLive`) marks, `ShowOldWrecks` (each frame) clears. Each live
  piece is read as it comes, before it is drawn (the screen keeps `LiveMargin`, a second or more, behind the rules):
  each track's new points from where it was last read, as the straight lines the screen draws between them; a
  knocked-out machine where she stands from her fall on: her place at her fall, as `LandKit`'s `PlaceAt` holds her (a
  live battle's machine is never pinned: the live drill stops her where she is hit, "she burns where she stood", so her
  track never jumps and `LandFeed`'s pin never comes), and her pin, from its time, where a recorded battle's reading
  pinned her; where the crews bail out (`-TheaterLandBailOut`, `S.Bails.bOn`; on in release 2's ship list), a
  knocked-out tank's place and pin out to 60 m from her fall (`BailReach`: `LandBail`'s men run 8 to 12 m in her lee,
  then 18 to 38 m on, some 54 m at the farthest; a tank, an armoured car or a self-propelled gun, the rules' role "tank"; a
  lorry's men are the crowd's own already); a tank drawn rolling smoothly (`-TheaterLandSmoothTanks`, on in release
  2's ship list: `LandKit` draws her place each step as the recording's blended over a bell of 0.4 s either way, so
  she cuts a corner, or runs ahead of her kept points or behind them as her speed changes), her reach `BlendLead` (0.8)
  times the bell's spread times her fastest within its reach more along each piece of her way (2.6 m at 8 m/s; the bell
  leads by 0.73 to 0.78 of its spread at its 0.05 s steps; her way read again back over the bell's reach of each
  piece's new points; `GuardOldWrecks` reads the switch from the command line as the armour does); and a man who left
  the field with his block, 7 m more round where he left (`LeaveDrift`: `LandMen` draws him on at his last speed for a
  second as he fades). A wreck any of them comes within 8.5 m of (her rings, from a vehicle's middle: `VehicleReach`,
  the recorded battle's 8 m and half a metre) or 1.9 m (from a man's, `ManReach`) is marked, due at the battle time he
  first comes that near; one log line, "Land battle (live): old wreck N
  (the GER Tiger) to be cleared away: a live man or machine comes near her at T s (the rules at R s, the screen at S
  s)". One a man or machine moving as the piece ends is heading for (his last way straight on for 8 s, 120 m at the
  most: `SoonAhead`, `SoonFar`; his reach and 2 m round it, `SoonWiden`) is marked as coming, till the next piece.
  `Yard::Clear` then reads the frame's own eye, the camera as it is posed (`LandHulk::SightAlong`: where `Eye` stands
  and looks once `AimEye` has turned it by the player's drag aside, `LookAside`, up to 50 degrees round and 30 up or
  down, and the effects kit has jolted it; its lens, Unreal's across the picture, by the viewport's shape and lines; a
  cut from `S.Pose`. The shot's pose alone missed the drag: a wreck on the screen could be judged out of it and cleared
  in plain view): a marked wreck, or one coming, goes the first frame the eye cannot see her go: on a cut, out of
  its picture (her reach round her middle, her rings or her patch's ragged edge, against the picture's edges a tenth
  wider for an eye that turns: `InSight`), or under 2 of its lines (`SightPx`); one due, the eye on her all along, half
  a second before the screen shows him coming near her (`ClearLead`). One coming and not yet due stays while the eye is
  on her (he may turn away). Her near and far levels and her patch hidden, the grass laid again where she lay, the
  camera no longer kept out of her, no shell hole kept off her; one log line, "Land battle (live): old wreck N (the GER
  Tiger) cleared away at T s, out of the eye's sight: a live man or machine comes near her at D s" ("on a cut", "in the
  eye's sight, her time come"; "a man or machine heading for her"). A man's provisional last point is read again with
  the next piece; when the men are placed again (the setup step) every track is read afresh, and a wreck once cleared
  stays so. So no man or machine the rules move passes through one, nor a crew bailing out of a tank or a man leaving
  the field with his block, a tank rolled smoothly too (a late kept point's ease under the screen, a tenth of a metre
  on the joined build, lies within `VehicleReach`'s half metre). What the guard does not read can: a downed airman
  under his chute and walking off (`-TheaterLandChutes` with `-TheaterLandAirSupport`), what a blast throws (the dead
  and the men thrown, `-TheaterLandGore`; earth; a turret of this battle's), a man drifting out farther than 7 m (one
  leaving at 7 m/s or more), smoke and dust; the chutes, the aircraft and the gore are all on in release 2's ship list
  (`Release2Ship.cpp` on `claude/project-thread-pi6nal`), so in Brian's play an airman may yet come down on one. Where
  the eye watches her from the moment someone turns for her to half a second before he reaches her, she goes from view
  in its sight (her steel and patch are the vehicles' and the ground's shared `SteelInk` and `GroundInk`, so she cannot
  fade alone). A live battle's shells land where its rules send them: none is dug under a standing wreck or her patch, a
  metre round (`Yard::Over`, wrapped round `S.Holes.Dig`); the round bursts there all the same and its shallow torn
  earth (the form a lane or a yard keeps where `Dig` says no) is laid under her, her patch over it. A recorded battle
  lays and digs as before, bit for bit.
  Measured (the test's case 9: each recording opened as a live battle, its tracks, rounds, events and the rest taken
  out, its setup room its attacker's zone; forty burnt out a month before, at most 20 laid): Normandy 20 laid in 13
  groups (9 along the old lines, 1 at a junction, 3 on the axis's flanks; round 1's 3 on the old lines, 7 on the flanks,
  1 strewn of 11), 5 of the old lines' and the junction's on the rest of the likely ground, every point 1.5 m and more
  off its core, the flanks' 26.7 m off all of it, 78.4 m from where any unit stood at its start; Prokhorovka 18 in 11 (4
  at a junction or the village's edge, 7 on the flanks; round 1's 12, none at either, 11 in one western pocket; 2 found
  no room), from west to east across the field, 10.3 and 27.2 m off, 50.3 m from the start. Prokhorovka's 5 old lines
  (its 5 lanes: the steppe has no hedge, bank or wall) give no group, as on its recorded lay (0 along the old lines
  there too, 6 at a junction or the village's edge); its fronts are 1,690 and 1,160 m wide as drawn up. With a corps
  from next door coming up: 20 and 13 laid, every point of them 457 and 364 m in from the field's edges. The battles as
  fought, read piece by piece as the guard reads them (30 s pieces, each man's last point provisional) and read whole,
  the same marks at the same times: none on Normandy; on Prokhorovka its junction's two behind the defence's foot,
  wrecks 11 and 12, due at 272.7 and 275.1 s, a man or machine heading for both first; every other at least 30 m from a
  vehicle's way and 10 m from a man's (the measure's reach). Read again as release 2 plays a live battle (no machine
  pinned, the crews bailing out to 60 m of the 3 and 5 tanks knocked out, the tanks rolled smoothly over 0.4 s): the
  same marks at the same times, piece by piece as whole (none on Normandy, 11 and 12 on Prokhorovka at 272.7 and 275.1
  s), every wreck left standing more than 100 m from where a knocked-out tank stands. The guard's cost at a live
  piece's cadence (every track's last two kept points read against every wreck, and each moving one's way on): 0.44 ms
  a piece on Normandy and 0.40 on Prokhorovka on the average, 0.71 and 0.43 at the most (the shared machine's clock:
  the runs before it 0.33 to 0.53 on the average, 0.44 to 1.05 at the most). Laid in some 0.4 to 0.5 s here.
- **How each looks** (`Yard::Dress`; the art director's to judge, round 2 below). Her own model, knocked out and burnt
  through: her turret turned (mostly within 50 degrees of ahead) and her gun drooped to its stop, or, one in five, her
  turret blown off and lying 1 to 4 m off her side, tail or bow (clear of her hull's corners, its gun turned away from
  her hull, and off the side her track ran off), on its roof, its side or tilted back on its rear edge with its gun in
  the air, its own hatches and doors shut (open, one on its side stood on a door a metre over the grass), and her open
  ring a black hole (her turret's base less 9 cm across its middle, from the file's own turret, `Kit::RingR`) inside a
  lip of bare or rusted steel 8 cm wide (round 2 laid a disc the width of her whole turret, tagged as soot); her hatches
  and doors open; three in ten tanks (one in five half-tracks) with a track run off beside and behind her, link by link
  from her own track (never under her thrown turret): flat on the ground (or on her patch, 2 cm over it), snaking from
  where it leaves her (`LieTrack`: 14 to 22 cm either way, a wave of 2.6 to 4.2 m; beside her it waves outward only,
  its inner edge kept 3 cm off her footprint, where round 1's strip ran up to 20 cm under her outer road wheels; round
  2's snaked only past her tail, and the half-track's short strip lay straight), and folded back on itself once, its
  last quarter or third turned over and doubled back on top, upside down, the doubled end swung off the strip under it
  by up to 0.4 of its width (less where that would touch her), so the fold shows from above (round 2's lay exactly on
  top); a lorry's canvas tilt, leather, rope and cloth burnt away to the tilt's bare hoops (every nation's: by the
  file's paint names, `LandKit::Mesh::Paints` and `Model::PaintNames`, each by its stem, the name up to its first dot,
  so the M3's `canvas.seat`, the T-34's logs' `wood.end` and `wood.split` are canvas and wood; round 2 matched whole
  names and missed them; round 1 burnt none), her bed's floor (`BedOf`: the biggest flat of her wood facing up, a square
  metre or more, with its underside; round 2 took the benches) laid again as planks along her, charred, a quarter to
  two fifths of them burnt through and gone by her own dice and some burnt short (`LayPlanks`; her benches and the rest
  of her wood kept), her glass gone and her tyres burnt off to the rims (the half-tracks' front tyres too); a tank's
  road wheels' rubber charred; her aerial and roof gun gone. Her coat is a burn, laid by where the fire was (the art
  director's last round, 24 November: round 2's patchwork read as camouflage, hard black blotches over pale chalky
  grey a week on and pale blotches on rust after, every boundary a cut line, the noise outweighing the fire; round 1
  read as one even terracotta coat). Each point's fate (`FateOf`) reads two smooth fields over her, the noise only
  roughening their edges: her heat, from how near the fire's places (her engine deck's grilles, the hatches it came
  out of, her turret's ring), each place's reach leaning a little downwind (the wind of her burning, from her own
  dice) and falling off fast below it, then how high on her and a little how much it faces up (a lorry's and a
  half-track's thin plates burn hotter); and her soot, densest round the openings the smoke came out of (her ring
  only when her turret was thrown), stretched downwind and up and streaked along the wind on her deck and up her
  sides, kept short on the outside of a plate below an opening that faces away from it (the smoke rose past it: the
  first pass of this round laid a straight-edged black band along an M3's body side, a painted stripe), burnt off the
  hottest steel. Each coat blends into the next across a band of its field, so none meets another at a cut line. A thrown
  turret burnt from its own hatches only, blown off her as the fire began (the first pass of this round heated it
  from the ring under where it sat: wholly bare grey a week on, wholly orange a month on). A week on: soot black
  (25 to 40) over about half of what faces the sky round her decks, grilles and hatches, feathered grey at its edges
  and grained blacker and greyer along the wind (not one flat black cap); round it, by her heat, bare grey steel (85
  to 95), smudged darker, in the hottest centres, the paint scorched to a chalky grey-buff (100 to 110) round that, a
  narrow fringe of it heat-tinted brown, and her own paint low on her cooler sides; a fresh orange (178,90,43 before
  the light) only along the plates' edges where the paint burnt off, a few in 100 (not on a lorry's hoops, thin tube
  creased all over, that went wholly orange; by a month the edge is the flash rust's own). A month on: the bare steel
  flash-rusted (some 120,69,36 in the picture), a dull orange-brown mottled at a hand orange and a browner ochre near
  its value and darker in brown patches (not one sheet of orange paint; even at a hand, not as uneven as the steel
  under it), its brightest in the sun, on a top or a side, no brighter than some 150,85,45 (the art director's carry
  of 24 November 21:01Z: round 3's read hot, almost glowing, on a sunlit turret top, its brightest some 188,94,42
  there; the fix round's first try held it down on her tops but its sunlit sides went to 158, and mottled it only 162
  to 158 before the light, one even orange-brown), about 15 to 25 in 100 of her, coming on across a soft edge and
  staining the paint round it rust-brown (not a bright orange against the pale grey-buff; never more than half over
  it, so the paint still shows as the coat it is, easing to under a third by ten months), in runs soft at their edges
  and fading as they go, and a softer wash between them, down her sides under
  the rusted steel above them: her own plate sampled as itself up to her top there, then her top over the edge
  (`Tops`, her height over each spot of her at rest; the first pass sampled every point above a side as a deck). Each
  wreck's thresholds are set by her own file (`ThresholdsFor`: what faces up on her upper works, her deck and
  turret, sampled by area a month on, a thrown turret not counted; the heat at which paint burns to bare steel
  moved up so no more than 22 in 100 of them are bare and unsooted, and where fewer than 15 her heat's lie drawn
  out, up to 2.5 times, till they are, the soot over her hottest steel burnt off as it goes; the heat at which it
  scorches set so some 34 to 42 in 100 lie between). The first pass of this round moved only the bare threshold,
  0.15 down at most, and kept the scorched one over 0.2: a third of the field's wrecks stopped there and the field's
  scorched paint on what faces up fell from some 21 in 100 to 10; drawn out, every wreck reaches 15, the fire's
  places still deciding where on her rather than the noise. Her heat's lie says where on her each coat lies, her
  thresholds how much of her. A thrown track's links and the lip of an open turret ring rust a fifth by a month and
  all by half a year, each across a soft edge and none before its time (the lip dull, not a drawn orange ring). Six
  months to a year and a half: the rust old, darker and duller (some 84,53,39: red at most 2.2 times blue), mottled
  deep brown and lighter at a stride and streaked darker down her sides, darkening a little as the months go; the
  soot washed off what faces the sky (kept on her sides and under her; a dark stain of it fading where it went); the
  scorched paint flaking from the hottest (the steel's edge eating half the way back into it by a year and a half),
  what is left of it weathered duller and darker within half a year (some 100,88,75 at six months to 97,83,72 by a
  year and a half; the first pass left it pale, 105,97,83, at six months: a cow pattern) and stained by the rust
  round it, so it reads as faded paint on the rust; her own paint flaking in flecks to the rust, some one in eight
  of it by a year and a half. And, from some three months (the art director's second carry of 24 November 21:01Z:
  from half a year her old rust read as one even brown), a little of her paint as the fire left it, her own or
  scorched, kept from flaking where the weather reaches least (`ShelterOf`: a little on an open side, more low on
  her, wholly under 0.5 m over her tracks and none from 1.4 m, and under her track guards or whatever of her stands
  out a hand or so over the face, wholly on what turns down), in soft patches a stride or so across that her runs
  of rust pass by, lighter than the rust round it, and none where the fire burnt her bare (the fix round's first try
  laid her own paint back over the burnt steel and on a fifth of every open side, high as much as low: it read as
  grime); and a few darker runs (`WeepFrom`) weeping from the seam where her top meets her side, from under a ledge
  of her and from under the hatches and grilles the flames came out of, on her upright faces only, in some stretches
  of her and none in others, a hand wide, each narrowing as it fades within about a metre, long and short (the fix
  round's first try ran them evenly spaced and all one length, stripes over her paint left, and blots on her sloped
  faces and her thrown turrets; none on a thrown turret now, and none where her map of her tops has no top). Her
  coat is halved where the file's plates are bigger (`Halve`, as LandKit halves a
  camouflaged one's), so her coats' soft edges lie where her heat puts them on a lorry's, a half-track's or a
  Sherman's big plates too (round 1's M3
  showed one colour smeared from corner to corner): the near level down to 0.35 m (`NearHalve`; round 2's quarter metre
  cost too much), the far level to 0.7 m (`FarHalve`; round 2 never halved the far level, and with the plates' edges'
  orange laid on its few big creases it turned one orange coat again beyond 140 m). The edges' orange is the near
  level's only (`EdgesOf`): on the far level's coarse mesh every crease is a plate's edge. Low on her the field's earth splashed
  up and, after half a year, a green stain where the weeds stand. Settled 4 to 17 cm into the ground (a lorry less). No
  fire and no smoke. Under her a scorched patch (`Yard::Patch`), not a soft round shadow: a week on ragged, ash
  grey-black with lighter drifts at her engine's end (some 64,63,55 in the picture), burnt grass dark brown round it
  (some 67,55,36), a yellow-brown rim at its ragged edge (some 94,79,50), and her fuel's stain beside her, a cool
  near-black pool with a crisp lobed edge (some 38,38,31; round 2's three came within 5 of one another, about 41,35,26,
  and read as her shadow); its mesh three spokes to each of round 1's and 16 rings (48,720 triangles for twenty, round
  2's 87,360, round 1's 6,160); it browns within a few months and then greens lusher than the field round it (the halo as round 1's); the grass
  and corn stand to her sides but not under her (her hull's corners too, which her rings leave), her thrown turret or
  her track, nor on the patch while it is fresh (`Covers`, in the carpet's `Stands`). On a field with snow lying
  (`LandField::Ground::SnowLying`, white from 0.45 as `LandSnow` whitens a roof) the snow fallen since she burnt lies
  over her patch as over the field round it (only a fresh one, under a month, shows black through, its fire having
  melted the snow round her), over the earth splashed on her low parts and the weeds' stain, and on what faces up on
  her, the steel showing through where it lies thin.
- **The scene** (`ALandBattle::LayOldWrecks`, after `LayOldRuts` as the battle opens; `ShowOldWrecks` each frame after
  `DrawArmour`). Each wreck's near and far levels are static meshes of their own (`LandParts::GroundMesh`) on the
  vehicles' `SteelInk`, casting shadows, the near one shown within 140 m of the eye (`LandHulk::NearTo`, the vehicles'
  hand-over), the far beyond; her patch on `GroundInk`, 2 cm over the drawn ground. The camera keeps out of them as it
  keeps out of the battle's own (`PlaceMovers` adds each to `LandSight` as a still machine, and her thrown turret as
  another: its box as it lies and its gun's muzzle, `Hulk::TurretBoxLo`, `TurretBoxHi`, `TurretMuzzle`; on its side
  it stands as tall as a man, and a tilted one holds its gun up two metres). One log line: "Land battle:
  old wrecks: 20 laid of 40 the scrap men left of 40 burnt out here, 1 month since the last ...: 10 GER (...); 10 USA
  (...); 2 turrets thrown, 1 track run off; 5 groups along the old lines, ...; N meshes, N points, made in N s" (the
  stand-in's "20 laid of 40 asked (the stand-in: none taken by the scrap men), 6 months since they burnt ..."; a live
  battle's ends "laid for a live battle, clear of where its blocks draw up, the ground between its fronts and its lanes
  (N groups along the old lines or at the village's edge or a junction on the rest of its start areas or the ground it
  may fall back over)", and where some found no room, "N found no room clear of the lanes, hedges and the ground the
  live battle is likely to cross"). In a live battle each wreck's patch is kept too (`OldWreckPatch`), for
  `ShowOldWrecks` to hide with her near and far levels when it clears her (`Yard::Clear` on the frame's `S.Pose` and
  the viewport's size, `GEngine->GameViewport->GetViewportSize`, on the marks `GuardOldWrecks` makes); it and
  `PlaceMovers` pass over one cleared (`Yard::Cleared`), the grass's `Covers` and the holes' `Over` no longer read her,
  and `FLandBattleState::OldWreckSeen` holds how far each track was read.
- **Test**: `Tools/land_hulk/run.sh [recording.json ...]` (`LAND_HULK_OUT`). Built with `-Wconversion` by both
  compilers; the clang build must lay every wreck where the gcc build laid it. On every recording, laid as the scene
  lays them: the same twice (every wreck, her pose and her near, far and patch meshes, hashed); every point of every
  wreck read where it lies, none on a lane, a bank, a house, a wall, a yard or garden, a wood, water, marsh or standing
  corn, none over a cattle's path; 8 m from every vehicle's way, 1.5 m from every man's; no half-metre square held by
  two; every wreck and thrown turret settled on the ground, not floating; fewer as the months go (twenty burnt out:
  20, 17, 10, 3 and 1 at 1, 6, 12, 24 and 60 months); the nats' shares (GER 30, USA 10: 15 and 5 of twenty); the cap;
  none with nothing asked; the key read ten ways; the meshes finite and whole; the rust
  spreading over the same twenty wrecks as they age; and on an old field (two battles' scars and old ruts laid first)
  none on an old hole or rut. Since the review of 24 November: every point of every scorched patch read where it lies,
  as her own are; the grass kept out of every hull's footprint, every 10 cm; each thrown turret clear of her own hull
  and run-off track, its gun not through her hull, and its box for the camera on the ground; the battle's own models
  drawn only for wrecks of its own month; the stand-in laying 12 of 12 at 18 months (the key's reading 4) and 20 of
  40; with the field telephones made from the battle (six wires), none within 0.5 m of a wire (on the steppe, laid
  without them, one lay over one); and the same field with snow lying 0.9, six months on: the patch the field's own
  colour (off by 0, against 12 and 17 in summer), what faces up on her white. It paints `placement_<recording>` (the
  field from above, the wrecks drawn to scale from their own meshes, ringed by nation, four close looks) and
  `look_<recording>` (four wrecks a week, a month, six months, a year and a year and a half on). Pictures:
  `/mnt/project-files/battle-fx/old-wrecks/` (`placement_r1.png`, `placement_r1_prokhorovka.png`, `look_r1.png`,
  `look_r1_prokhorovka.png`), with the look's rules in `look_rules_r1.txt`.
  Since round 2 of the look (the art director's notes of 24 November 14:34Z): the look sheet draws each wreck at its
  column's age (round 1 drew an earlier battle's wreck, 30 in 100 of them, months older than her column: its "a week
  on" Tiger and Sherman were some 7 and 4 months old), and
  **measures it as she does, in the picture's own sRGB**: each pixel tagged with what the dressed point nearest it shows
  (`Dressing::Coats`: soot, scorched, bare, flash rust, old rust, her paint, charred rubber, wood or glass, a track's
  links, snow; her patch's ash, burnt grass, rim, fuel, brown, lush), and for each wreck at each age, and all four
  pooled against her numbers, the share of her lit faces (the sun's light on them 0.75 and over) each takes and its
  median colour, and her patch's (a table in the log and `look_<recording>.csv`). It fails where a week's soot is off
  a third to two thirds of her lit faces, orange more than a tenth of them a week on, rust less than a tenth a month on,
  or old rust redder than 2.25 times its blue. Every nation's lorry and half-track (twenty laid of each, GBR, SOV, ITA,
  JAP, GER, USA) dressed and counted against her own model: every point of canvas, leather, rope, cloth, torn or skin
  burnt away, every point of glass gone, every tyre to its rim, the tilt's hoops still standing, the lorry's bed's
  planks laid and some but not all burnt through. Each run-off track folded back on itself once, snaking (off its own
  smooth line, the mean of 1.5 m either side, at least 4 cm, or turning from one way to the other), its links' inner
  edges off her footprint by half her own track's width (her road wheels), and none of its points through the ground.
  It paints `track_<recording>` too: each run-off track of the fresh field from above and from low on its side, a week
  on. Pictures: `/tmp/claude-0/ad-round/wrecks/` (`look_r2.png`, `look_r2_prokhorovka.png`, `placement_r2.png`,
  `placement_r2_prokhorovka.png`, `track_r2.png`, `track_r2_prokhorovka.png`).
  Since the review of round 2: each pixel also by its colour alone (`Hue`: dark, orange, brown rust, pale grey-buff,
  grey, other, the review's own classes) beside its tag, and three look sheets, not one: `look_` (her near level, the
  sun from the north-west), `look_far_` (her far level, as the game shows her beyond 140 m) and `look_side_` (her near
  level with the sun from the south-east, on the sides the camera sees, where the first sheet shows only shade). It
  fails, besides round 2's marks, where a week on more than 8 in 100 of her lit faces are orange by colour, or more than
  a tenth flash rust by tag; where the far level's flash, orange, scorched paint or month's rust strays from the near
  level's in the picture (5, 5, 6 and 8 in 100), or, over all twenty by area facing up, its soot or scorched-and-orange
  by more than 6 (round 2's far level: a third flash orange beside the near's 3 in 100); where her sides in the sun
  show less than a tenth her own paint a week on or less than 2 in 100 rust runs a month on; and where her patch's ash,
  burnt grass and fuel come within 15 of one another, measured from straight above on all twenty (`scorch_<recording>`,
  each with her hull over her patch: the look sheets' low camera hides the fuel behind her when it lies on her far
  side). It prints each wreck's flash rust a month on by tag and by colour, and every run-off track's doubled end must
  lie off the strip under it by 15 in 100 of its width or more (round 2's lay on top, not seen from above), the
  half-track's short strip turning at least once. Every nation's wreck read by her paints' stems: the M3's canvas seats
  burnt away, the T-34's logs among her charred wood, 95 in 100 of a tank's wood points dressed as wood; each thrown
  turret's open ring a hole within her turret's own base (0.8 to 1.0 of its reach); and each lorry's floor found by the
  test itself (her wood's biggest flat facing up): her bed and her planks' tops on it within 2 cm, at least 0.8 of it
  taken off and laid again, 0.9 of her other wood facing up (her benches) kept, a fifth to 0.45 of her planks gone. It
  paints `lorry_<recording>` (the first Soviet lorry of a stand-in yard of twenty, a week on, from above and from the
  south-east). Pictures: `/tmp/claude-0/ad-round/wrecks/final/` (`look_`, `look_far_`, `look_side_`, `scorch_`,
  `lorry_`, `track_` and `placement_` of `normandy-1944` and `prokhorovka-1943`, PNG, the measures in `gcc.log` and
  `look_<recording>.csv`).
  Since the art director's last round (round 2 read as camouflage): her lit faces' hard edges, each pair of side-by-side
  pixels on them (across and down, both lit) whose sRGB luma (Rec. 709's weights) steps by more than 45, as a share
  of all such pairs, and the mean step, for each wreck at each age and all four pooled (the log and
  `look_<recording>.csv`'s "lit edges" rows). A plate's own outline against its shade counts too, so it is read
  against another build's, not against none: all four on Normandy near, a week on to a year and a half, round 2's
  0.9, 0.9, 0.9, 0.5 and 0.5 in 100 (mean step 5.7, 5.8, 5.7, 4.8, 4.5), after it 0.3, 0.3, 0.2, 0.0 and 0.0 (3.6,
  3.5, 2.6, 2.1, 2.0) and since that round's reviews 0.2, 0.2, 0.2, 0.0 and 0.0 (3.7, 3.7, 2.4, 2.1, 2.0: the soot's
  grain and the rusts' mottles step a little more, softly); on Prokhorovka round 2's 0.9, 0.8, 0.7, 0.4, 0.3 (4.8,
  4.8, 5.0, 4.1, 3.8), after it 0.6, 0.5, 0.2, 0.1, 0.0 (3.1, 3.1, 2.0, 1.7, 1.5) and now 0.6, 0.4, 0.2, 0.1, 0.0
  (3.5, 3.4, 2.0, 1.7, 1.7). Pictures: `/tmp/claude-0/ad-round/wrecks/final2/` (the round before in
  `/tmp/claude-0/ad-round/wrecks/soft1/`, round 2's with the same measure in `/tmp/claude-0/ad-round/wrecks/before_soft/`).
  Since the art director's pass (24 November 21:01Z) and its two carries, each measured (the log and
  `look_<recording>.csv`), and since its fix round (the reviews of the first try: its sunlit sides unchecked, its
  mottle lost, its paint left high on her open plates and over her burnt steel, its darker runs stripes): a month
  on, her flash rust lit, by its brightest (the 2 in 100 of its pixels brightest by value, their median colour;
  each channel's own 98th in 100 of the pixels that are the flash rust alone, every corner of their triangle so, not
  its blends at her plates' edges; and the red's and the green's 98th of all its pixels, its blends too, their blue
  the pale paint's beside them), on her sunlit tops on the first sheet near and far and on her lit faces on the
  side sheet, within 5 of 150,85,45 in each channel for each of the four with 30 such pixels and more and for all
  four: Normandy's all four near, far and side 189,93,42, 183,92,41 and 189,94,42 at 1e56500d, 148,78,37, 145,76,37
  and 153,81,39 at the first try (its Tiger's side 158,83,40), now 145,77,36, 142,75,36 and 150,80,39 (the Tiger's
  side 154,81,39 the most); Prokhorovka's 188,94,42, 175,88,41 and 192,96,42, then 150,79,38, 146,78,37 and
  150,79,38, now 148,78,37, 141,75,36 and 147,78,37. Its median lit near, all four, within 118 to 142, 62 to 78 and
  30 to 46 (1e56500d's 131,73,39 and 126,70,38, now 119,69,36 and 120,68,35: the price of holding its brightest
  down), and mottled: as painted (before the light, the flash rust alone) its red's 90th in 100 at least 11 over its
  median (1e56500d's 35 and 40, the first try's 9 and 7, now 14 and 13). A week on the plates' edges by colour some
  138,78,45 and 145,82,47 (1e56500d's 144,81,46 and 144,82,47; the first try's 124,72,42 and 133,77,45, dull). And
  from half a year, on the side sheet, every pixel of hers lit or in shade, by where the weather reaches her (her sky
  faces, facing up 0.5 and more; low on her, her lower half; her open sides, the rest): her paint left (her own or
  scorched) low on her kept to 0.8 of its share a month on and 15 in 100 and more over her open sides', her open
  sides' no more than 8 in 100 over theirs a month on, her own paint on her sky faces no more than 2 in 100 over its
  share a month on, lit low on her her paint left 12 and more in luma over her old rust there, and her runs on her
  lit sides whose luma lies 10 and more under her old rust's median there 1.5 in 100 of her lit sides and more; all
  four, at six months, a year and a year and a half. All four on Normandy: her paint left low on her 69 in 100 a
  month on, then 67, 59 and 56 (1e56500d's 70, 68, 55 and 50; the first try's 70, 68, 58 and 55), on her open sides
  36, then 31, 21 and 20, her own paint 4 in 100 of them (1e56500d's 30, 19 and 17, her own 5; the first try's 40, 31
  and 30, her own 20), lit low on her some 94,81,61 (luma 82, 81 and 81) against her old rust's 80,51,37 (56, 57 and
  57); her darker runs 3.5, 4.2 and 4.3 in 100 (1e56500d's 0.0, 0.1 and 0.1; the first try's 2.4, 2.9 and 2.7). On
  Prokhorovka low on her 70, then 70, 61 and 58 (1e56500d's 71, 68, 55 and 51), her open sides 30, 20 and 18
  (1e56500d's 30, 18 and 15; the first try's 41, 32 and 31), lit low on her luma 77, 74 and 74 against 58, 55 and
  54; her darker runs 2.0, 2.0 and 1.9 (1e56500d's 0.1, 0.0 and 0.0). Some wrecks have few darker runs (the Soviet
  lorry 0.4 to 0.5 in 100, her hoops standing as her top over her bed's sides, so no seam there; a Panzer IV on each
  field 0.7 to 1.1). The hard edges within 0.1 in 100 of 1e56500d's for every wreck at every age (the most, 0.07 more, the
  Sherman near at six months): all four on Normandy near 0.23, 0.19, 0.18, 0.05 and 0.04 in 100 (1e56500d's 0.23,
  0.22, 0.16, 0.05 and 0.03), on its side sheet 1.02, 0.90, 1.10, 0.71 and 0.59 (0.99, 0.95, 1.12, 0.67 and 0.56);
  Prokhorovka near 0.54, 0.37, 0.19, 0.04 and 0.03 (0.55, 0.43, 0.23, 0.06 and 0.03), side 2.03, 1.52, 0.60, 0.14
  and 0.08 (2.01, 1.80, 0.62, 0.15 and 0.09). Pictures: `/tmp/claude-0/ad-round/wrecks/final3/` (the first try's in
  `/tmp/claude-0/ad-round/wrecks/carries1/`, 1e56500d's in `/tmp/claude-0/ad-round/wrecks/before_carries/`).
  Since 25 November, each recording opened as a live battle too (case 9: its header alone, as above): none laid with
  none asked; laid as the scene lays them, every point of every wreck read as a recorded field's are (open ground, no
  cattle's path, settled, no square shared, her patch, her thrown turret and run-off track as above) and 15 m from a
  lane's middle; where the rules draw it up against where its units stood at its start (each block's middle, the mean
  of its men's and machines' first points, within 5 m of its modelled box; each of them alone 15 m and more inside the
  60 m kept round the boxes; every one in its own side's zone); every point off the likely ground's core, and the
  axis's and strewn groups' off all of it, the groups on its outer part named; every point clear of where its units
  stood at its start by 60 m less the model's miss; the battle as fought against that ground (its ways' points off the
  core and off all of it, and the farthest); the same twice; every point of every wreck and her patch read by `Over`
  (no hole dug there) and none of the battle's own shells, bombs and grenades landing on one; with a corps from next
  door coming up, every point 300 m and more in from every edge; the battle as fought read as the guard reads its
  pieces (30 s pieces, each moving man's last point provisional, a pin only from its time, the men leaving with their
  blocks) and read whole: the same wrecks marked, once each, each due at the same time, no way within its reach of one
  before her time, every one left standing 8 m from a vehicle's way and 1.5 m from a man's (as drawn: every kept point
  to the next, every 0.25 m, and the pins), and those a man or machine was heading for at a piece's end all marked; the
  same battle read again as release 2 plays a live one (no machine pinned, her points from the pin on standing there as
  a live battle's do; the crews bailing out, `BailReach`; the tanks rolled smoothly over 0.4 s): the same piece by
  piece as whole, every wreck the first read marks marked too and none later, and every one left standing 60 m and more
  (her rings) from where a knocked-out tank stands; the
  guard's cost a piece at a live piece's cadence (each track's last two kept points); each marked one cleared with the
  eye looking away, no longer keeping the grass or a hole off; and the guard made to work (the battles as fought send
  few near one): a tank's pieces up to 30 m short of a wreck's reach leave her and its next piece, through her, read
  from its provisional last point again, marks her, due as it reaches her; the eye on her 5 s before keeps her, and at
  her time less half a second clears her ("in sight"); the eye looking away 20 s before clears her at once ("out of
  sight"), a cut too, and 6 km off she is too small to see go; a man 0.3 m outside his reach of another leaves her and
  through her marks her; a lorry pinned on a third marks her from her fall, far off leaves her; the men placed again
  (fewer tracks) read afresh, marking none twice; a tank coming on at one 60 m short of her reach is heading for her,
  not due, kept while the eye is on her, no longer heading once turned away, cleared with the eye away; a tank knocked
  out 40 m off one marks her where the crews bail out (`BailReach`), not where they stay in, nor for a lorry; the same
  tank as a live battle knocks her out (no pin, her fall between two kept points 100 m either side of the wreck) marks
  her from her place at her fall where the crews bail out, and not where they stay in; a man leaving the field with his
  block 5 m outside his reach of one marks her as he leaves (`LeaveDrift`), and not where he stays; a tank creeping by
  1.5 m outside her reach of one, then off at 8 m/s, leaves her on her kept points' line and, rolled smoothly over 0.4
  s, marks her before she sets off, read whole or in two pieces (the first, ending as she sets off, leaves her; the
  second reads her creep again); and the shot's pose looking 50 degrees off a wreck on a 40 degree lens, the camera
  dragged back onto her: in the camera's picture (`SightAlong`, its lens across the picture turned back to 40 degrees
  top to bottom), not in the pose's. The clang build lays them where the gcc build does (their lines are among those compared). It paints
  `placement_live_<recording>` (the whole field: the likely ground's core tinted red and the rest of it orange, the
  start zones, its blocks as the rules draw them up in cyan, the ground between the fronts, the battle's ways as it was
  fought, where its units stood at its start, the wrecks ringed by nation, a red cross on any the guard marked, four
  close looks). Pictures: `/tmp/claude-0/livewrecks/run3/` (`placement_live_normandy-1944.png`,
  `placement_live_prokhorovka-1943.png`, the measures in `gcc.log`; the wrecks laid where round 2's `run2/` laid
  them; round 1's in `/tmp/claude-0/livewrecks/run/`).
  `LAND_HULK_JOBS=N` holds `run.sh` to N compiles at once (all at once by default).
- **Not yet.** In a live battle: a way round a wreck for the rules' drill (the guard clears her away instead: where the
  eye watches her from the moment someone turns for her, she goes from view in its sight, half a second before he is
  shown near her; a fade would need her own materials, not the shared `SteelInk` and `GroundInk`); the guard reading a
  downed airman's chute and walk (`-TheaterLandChutes`), what a blast throws (`-TheaterLandGore`'s men), smoke and dust
  (the chutes and the gore on in release 2's ship list); the rules' drawing up
  modelled on a river's bend, a stream, a second river or a city's streets (there each whole zone is kept, so fewer
  groups find room); the scars and the old ruts (they read the battle's tracks, none at a live battle's open, so its
  wrecks lie on ground without them: `Field::Likely` could stand for their fought-over ground too); Prokhorovka's 18 of
  20 (its fronts cross most of the field), none along its old lines (its lanes; none on its recorded lay either). The
  guns' and soft vehicles' own wrecks beyond the half-tracks and lorries; a tank burnt out on her side or nose-down in a
  ditch.
- **Owed.** The Mac compile (the names to check: `LayOldWrecks`, `ShowOldWrecks`, `OldWreckNear`, `OldWreckFar`,
  `OldWreckShownNear`, `FLandBattleState::OldWrecks` and `OldRutWays`, with `LandParts::GroundMesh`,
  `UStaticMeshComponent`'s `SetRelativeLocation` and `SetVisibility`, `FParse::Value` and `Param`; and since the
  review of 24 November, `LandHulk::Ask::bAsLaid`, `LandHulk::Hulk`'s `TurretYaw`, `TurretBoxLo`, `TurretBoxHi`,
  `TurretMuzzle` and `bTurretGun`, and `LandSight::Machine`'s `Root`, `Heading`, `HullLo`, `HullHi`, `bTurret`,
  `bMuzzle` and `Muzzle` for the thrown turret; and `LandHulk::Ask::Per` with `Recording::OldWreckPer` for the battle's
  scale; and since 25 November, a live battle's: `ALandBattle::GuardOldWrecks(bool)` called from `PumpLive` with
  `Grown::bReset`, `OldWreckPatch` (a `UPROPERTY` `TArray<TObjectPtr<UStaticMeshComponent>>`, filled beside
  `OldWreckNear` and `OldWreckFar` from the patch's mesh, `Levels[2]`), `FLandBattleState::OldWreckSeen`
  (`std::vector<size_t>`), `LandHulk::Yard::Reached`, `Cleared` and `Over`, `LandCarpet::Squares::Mark`, `LandParts::Wide`,
  `TObjectPtr::Get` and `TArray::IsValidIndex`, and `S.Holes.Dig`, a `std::function<bool(double, double)>`, wrapped
  in a lambda that captures the old one by value and a `const LandHulk::Yard*`; and since its review, the clearing
  moved into `ShowOldWrecks` (after `DrawArmour`, so on the frame's own eye): `FLandBattleState& S = *State`,
  `LandHulk::Sight` filled from `S.Pose` (a `LandEye::Pose`: `Eye` and `Look`, `LandPlay::Vec`'s `X`, `Y`, `Z`;
  `Fov`; `bCut`), `GEngine->GameViewport->GetViewportSize(FVector2D&)` as `DrawMen` reads it,
  `LandHulk::Yard::Clear(double, const Sight&, std::vector<int>*)` with `S.Battle` and `Yard::DueOf`, `FMath::Clamp`,
  `FString::Printf` and `FString(TEXT(...))` for its log line; and in `GuardOldWrecks`, which now only marks,
  `Yard::Reached(S.Rec, S.OldWreckSeen, S.Bails.bOn ? LandHulk::BailReach : 0.0, Blend)` (`FLandBattleState::Bails`, a
  `LandBail::Book`) and `S.Rec.Fought`; and since its second review, the eye read from the camera as posed:
  `Eye->GetCameraComponent()` (as the effects kit's `TickBegin` call reads it) and its `FieldOfView`,
  `Eye->GetActorLocation()`, `Eye->GetActorRotation().Vector()` (`FRotator::Vector`, as `AirBattle.cpp` uses it) and
  `LandHulk::SightAlong`, the pose's `Sight` kept where there is no `Eye`; and in `GuardOldWrecks`, `Blend` read from
  `-TheaterLandSmoothTanks=` (`FParse::Value` into an `FString`, `FCString::Atod`, `FMath::Clamp` to 0.45) or
  `-TheaterLandSmoothTanks` (`FParse::Param`, 0.4), as the joined build's `LandBattle.cpp` reads it for
  `Armour->SmoothTanks`. On merging with `claude/project-thread-pi6nal` (b6e80cee; `git merge-tree` finds two
  conflicts): `LandBattleLive.cpp`, where `PumpLive` times its steps, so `GuardOldWrecks(G.bReset)` goes after
  `MapLive` and before `Lap(MsMap)`; and `Tools/land_hulk/run.sh`, where the joined build adds `LandHouse` to
  `CORES` (after `LandField`) beside this branch's `LAND_HULK_JOBS` and its build loop: keep this branch's file with
  the joined build's `CORES` line. `LayOldWrecks`' return for a live battle (`S.Rec.bLive`) is dropped by the
  merge's own reading of `LandBattle.cpp` (it auto-merges); check it is gone. After the merge `S.Armour->SmoothTanks`
  holds the same number as `GuardOldWrecks`' own reading of the switch, and may stand for it. And the art
  director's look of her two carries (the look passed at 1e56500d, 24 November 21:01Z), and the 45 fps
  gate with twenty wrecks
  laid. The Mac shots, with `-TheaterLandOldWrecks=20 -TheaterLandOldWreckMonths=1` against without on Normandy, then
  `=6` and `=18` months (the same twenty, aged): from 300 m up over the old lines; a group from 40 m and from a man's
  height; a thrown turret and a run-off track close to; a wreck as the eye crosses 140 m (her near and far levels
  should not jump); a half-track's burnt tyres; and the patch's edge against the ground's far tile (it is laid 2 cm
  over the near ground's height, as a hole's edge is; a far tile's coarser mesh may differ). Since the review: a
  winter field with snow lying (a recording whose field has snow, 0.5 and over) with `-TheaterLandOldWrecks=20
  -TheaterLandOldWreckMonths=6` (the patches lost under the snow, snow on the decks) and `=0.2` months (a fresh one
  black through it); the grass round a wreck's corners from a man's height (it should stand round her and not through
  her); a thrown turret on its side, from the camera at a man's height (the camera kept out of it and its gun); and
  with `-TheaterLandPhones` on, the wires laid round the wrecks. And once the campaign's row is on (sgfei1 b0a47250,
  "Old wrecks" off by default), one campaign battle on a field fought over before, its log line saying how many were
  drawn for how many burnt out at the battle's scale ("one for every 7.5"). Since 25 November, a live battle with
  `-TheaterLandOldWrecks=20 -TheaterLandOldWreckMonths=1` (or, once this branch meets the TS side's 8ddab648 and
  7ec111e2, the campaign's own key in the live header): from 300 m up as it opens (the wrecks off the start areas and
  the advance, along the old lines and out on the flanks, facing the other side), and the battle played out with the
  eye on a group by the advance's edge and on one by the village: no man or machine through one; any the guard marks
  and clears, its two log lines, and how the moment she goes from view reads (out of sight, on a cut or, the eye on her
  all along, in sight: none marked on Normandy as fought here, two on Prokhorovka at 273 and 275 s); with
  `-TheaterLandBailOut`, a tank knocked out by one (the crew's run clear of her or her gone first); with
  `-TheaterLandSmoothTanks`, a tank turning by one; the film view dragged aside (a left drag) onto a marked wreck or
  one a man is heading for (she stays while she is on the screen); a shell landing on a wreck (its burst and torn
  earth by her, no hole under her). Since round 2 of the look: the Mac compile
  of `LandKit::Mesh::Paints` and `LandKit::Model::PaintNames` (filled in `Model::Load`; the scene's own calls,
  `Yard::Dress(K, bNear, M)` and `Yard::Patch(K, M)`, are as they were, the new `Dressing` note left out); the cost
  against the 45 fps gate and the battle's open. Measured here (the test's twenty, the cloud machine, one core): round 1
  dressed a wreck's near level in some 22 to 27 ms (1.24 to 1.31 million triangles for twenty), round 2 in some 66 to
  90 ms (1.96 to 2.06 million, its patches 87,360 triangles for twenty, round 1's 6,160); since its review some 48 to
  56 ms (1.57 to 1.60 million, 20 to 27 in 100 over round 1; the coat halved to 0.35 m, and the fire's noises read
  only where they decide anything), since the last round some 55 to 60 ms (the same runs back to back, 49 to 52
  before it, 10 to 17 in 100 over: the fire's places' reaches read for each point, the soot's streaks and grain, the
  flaking's rag, the rust's mottles and runs, her height map's tops read once a cell), and since the art director's
  carries and their fix round some 5 in 100 over 1e56500d on Normandy and 3 on Prokhorovka by the instructions her
  dressing runs (callgrind, both levels for the twenty: 66.8 billion against 63.7, and 60.7 against 58.8), the clock
  as noisy as the shared machine's other work made it (sixteen runs back to back, fastest and middle: 64 to 72 ms on
  Normandy against 1e56500d's 63 to 70, 57 to 66 on Prokhorovka against 58 to 69; the first try's quieter runs 57 to
  59 and 52.5 to 54 against 54 to 56 and 51 to 52): the rust's stain round the steel, her paint left in her shelter
  and the darker runs, each read only where it could show), the far level some 7.5 to 9.5 ms
  (7 to 8 before it; 170,000 to 176,000 triangles for twenty, round 2's
  107,000 to 113,000: it is halved to 0.7 m now) and the patches 48,720 triangles. So `LayOldWrecks` with twenty
  makes their meshes in some 1.2 to 1.4 s here (round 1 0.5, round 2 1.5 to 2.1): the Mac to time it at the battle's
  open (its log line's "made in N s") and the frame time with twenty near levels in view (`stat unit`, against
  round 1's). And the shots, with
  `-TheaterLandOldWrecks=20` at `-TheaterLandOldWreckMonths=0.25`, `=1`, `=6` and `=18` on Normandy: a group from 40 m
  and from a man's height (the patchwork: soot, chalky grey-buff, bare grey steel, orange only on the edges a week
  on; the flash rust a month on; the old brown rust and the flaking paint at six months and a year and a half); a
  lorry close to (the Soviet or British lorry on Prokhorovka: bare hoops, charred planks with gaps, no glass, rims);
  a run-off track close to from above and from a man's height (snaking, folded back once, flat on the ground and the
  patch, clear of her road wheels); and the patch a week on (ragged, ash black at her engine's end, the brown rim, the
  fuel's stain) against six months and a year and a half on (the green halo). Since the review of round 2: a group
  from beyond 140 m a week and a month on (her far level: soot, grey-buff and bare grey, not one orange coat; the
  edges' orange is gone there); a wreck's sunlit side a month on (`-TheaterLandOldWreckMonths=1`, the sun on the side
  the camera sees: her own paint low on it, the flash rust in runs down it); the Soviet lorry on Prokhorovka from
  straight above a week on (her floor's planks with gaps showing the ground under her, her benches kept); a thrown
  turret's open ring close to (a black hole inside a steel lip, smaller than her turret's base); a half-track's
  thrown track from above (snaking, its fold showing); and the patch from above a week on (the ash, the burnt grass,
  the rim and the fuel's dark pool told apart, as `scorch_<recording>` shows them).

## A town or a city fought over (release 2, off until passed)

The look bible's gap **T4** and the art director's sheet `art-director/battles/city-battle-look.md`: where the rules
lay a battle in a town's or a capital's streets, the ground is its ruins, not the country round it. Behind the trial
word **"city"** (the game's `-TheaterLandTrial=city`, the cloud test's `LAND_FIELD_TRIAL=city`). Off, the field takes
the climate's look exactly as it did, byte for byte.

- **When it is picked** (`LandScape::Pick`): the word is on trial *and* the rules laid a city here
  (`LandScape::IsCityField`: any suburb, industry or park cell, or enough built cells). A recording may also name it
  outright (`field.look` "city", "ruins", "urban" or "town"). Its snow and its bare trees are the country's round it.
- **What lays it** (`LandCity.cpp`, a `LandScape::Region` like the desert's or the jungle's; plain C++, no engine, so
  the cloud test lays exactly what the game draws). Three street grids over the field (a core, an older quarter turned
  against it, the suburbs), each block walked round its four sides; terraces built wall to wall along the streets with
  `Host::Row` (as `Host::House` but with no gap kept round it), the suburbs' villas set back in their gardens, the
  works' sheds, chimneys and water tower on the industry, and **one landmark** by the heart (a spire, a dome or a
  station hall) seen from everywhere.
- **Its kit is built here, not read** (51 slots, every one with a name of its own, `City...`, so no two instanced kinds
  share an Unreal name): terraces of two to five storeys in five states (holed, gutted, half fallen, a stump, and now
  and then one untouched, its shutters closed and its glass in, where the city is least broken), a third of them
  brick-faced and a fifth in older, darker plaster; villas of two plans (long and gabled, square under a pyramid roof,
  never the same house in the same state twice running down a street), sheds, a
  warehouse, a church with its broken spire, a domed hall, a station, heaps, scatter, lone walls, shelled trees, lamp
  posts; and the streets' war: barricades of rubble and setts with sandbags and rails across them at the crossings,
  barbed wire before them, steel hedgehogs, tram lines' poles (one main street in three, some knocked over, their wire
  down), burnt-out cars, lorries and trams, and shells' strikes in the setts. Each at three levels (near; the middle
  from 90 m; plain from 300 m) or two (near, then plain).
- **The damage rule** is the breach clip rule: broken, never cut away. Every wall's top steps by courses (75 mm) and,
  near, brick by brick (0.24 m: toothed, a stair where it falls steeply, as torn brickwork is), holes
  are blown through it in rows (near, a hole's edge is cut a course at a time in brick-wide columns, the wall's brick
  shows in a scorched ring round it, and what it blew out lies spilt at the wall's foot), a half-fallen block's floors
  run front to back in strips, each broken back its own depth and its end sagging or snapped down at its own angle,
  rubble and a charred beam along the edge, stopping wherever a wall falls below them; a burnt shell's joists stub
  out, the rooms inside show plaster off to the brick and soot up from the windows that burnt.
- **Its ground:** dark setts in the core dusted with masonry, pale dust in the suburbs, ash and trodden earth in the
  courtyards, worn grass in the parks, gardens and open ground gone wild (tussocks in clumps, bare between), the city's
  dust over all of it; beyond the field the roofs run on under the haze,
  so the city never ends in a hard edge in the middle of a field. Its air is smoke and brick dust (sight some 3,300 m).
- **Trying it in the cloud:** `LAND_FIELD_TRIAL=city Tools/land_field/run.sh <a city field>.json`, with
  `LAND_FIELD_PICTURES=<folder>` for the pictures and `LAND_FIELD_EYE="x y angle [height]"` to stand the camera in a
  street (a place inside a building is moved to the nearest ground with a view). `Tools/land_city/city_fields.ts`
  writes test fields from the rules' own city builder.

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
  and `gun` (tips), and on a belt-fed gun `box` (her belt's box and the fold of belt in it, swung with her about the
  pin and never tipped); a mortar's `mortar` and `tube`. Sockets: `seat` (the key man's own root), `seat.loader`, `muzzle`,
  `feed`, the grips, `box` or `rounds`, the Maxim's `pull`. Every cartridge is drawn on the near mesh (Brian on the
  first sheet, 25 November: "make them more detailed with bullets feeding into the machine gun and more realistic
  angles and shapes"): a belt hangs from the feed down into an open box standing end-on to the gun, or a strip of
  rounds goes in from the left. The fed end of a belt is part of `gun`, so its rounds go in under the feed cover
  however she is swung and laid; the box and the fold of belt in it are the part `box`, which the game swings with
  her so the belt runs into its mouth wherever she points (the art director, 25 November: past ten degrees of swing
  the belt left a box that stood with the mount). A strip gun's box of strips stands with `mount`; the Type 92's and
  the Hotchkiss's strip comes out bare (their fired cases are thrown clear), the Breda 37's with its cases put back.
  Far off a belt is one band of brass (`rounds`, 7E643B). Slots, holes and bores are
  dark marks a millimetre proud of their face. A machine gun is 3,300 to 5,200 triangles near and 600 to 1,100 far;
  a mortar 2,300 and 1,000. The MG 42 is the section gunner's own gun (the soldiers' `soldier_GER_lmg`) to the same
  measures. The picture sheet: `LAND_KIT_SUPPORT=1` to the painter (`Tools/land_kit/land_picture.cpp`; the battle's
  daylight, each weapon whole beside a post banded every 25 cm and then close on her feed; `LAND_KIT_SCALE=2` paints
  each picture twice the size), laid out by `LAND_SHEET_ACROSS=4 LAND_SHEET_WIDE=3200 land_sheet.py`.
- **Which file, where it stands** (`Battle/LandTeamGun`, plain C++): `FileFor` by the block's army, the role and the
  month (the MG 34 until May 1942, the MG 42 after; an army with none of its own takes the nearest shape). `Gun::Step`
  each frame: set up with the model's seat on the key man where the crowd draws him, swung and laid on his aim (45
  and 20 degrees a second, stopped at the model's limits; a mortar turned whole); on his right shoulder while the team
  moves (the Maxim rolled behind him by her trail); left where she stood when he falls; not drawn while he is hidden,
  riding or mounted.
- **In the scene** (`ALandBattle::MakeTeamGuns`, after the men are posed and before the fire is stepped): three
  instanced meshes a file, the mount, what turns, and a belt-fed gun's box (`LandParts::BodiesMesh`: the fine level
  with every round, the plain one from 90 m; gone past 460 m; a file with no `box` part has an empty third row),
  placed by `LandParts::BankedAt` as a man is. The box is drawn only while she is set up or left where she stood
  (`Gun::SwungShown`, `Gun::SwungFrame`: her mount's frame swung by her traverse about her pin): on the move it is in
  the number two's hands, not drawn. `PlayWorld.Muzzle` gives a team's rounds her own muzzle and
  tells her of each burst (she shakes 6 mm back along her bore a round); `PlayWorld.GunLook` gives her own burst
  (`LandTeamGun::BurstOf`: the MG 42's 18 rounds in 0.9 s, where a section's gun fires 8; the Maxim 8 at 10 a second
  and the Vickers 6 at 8, as the sound check's recordings run), which `LandPlay` hands to the sound as well, and
  `PlayWorld.TeamGunFile` gives her file's name, which the round's noise carries as its `Mount` so her burst is heard
  on her own gun's row (`land.teamMg`, the sound check's).
- **The gunner** (`LandMen::Crowd::Serve`): his rifle laid down, lying behind a low gun (`fire_prone`, whose hands
  reach her grips) or kneeling at a high one or at his tube (`fire_kneel`), until his file brings `hmg_fire_prone`,
  `hmg_fire_sit` or `mortar_sight`, which are used by name as soon as it has them.
- **Checked in the cloud**: `Tools/land_kit/run.sh` (the section THE SUPPORT TEAMS' WEAPONS: every army's file reads;
  seat, swing, lay, limits, bursts, carried, rolled, left, not drawn, a belt's box swung with her about her pin and
  upright; `LAND_KIT_ONLY_TEAMGUNS=1` runs it alone) and
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
Halted in the open (the rules' `wait`, or `fire_kneel`, with no works, building, woods, rock or wreck for his cover)
he lies behind his gun on its bipod, `fire_prone`, and kneels only where low cover stands before him; on his feet he
fires from the hip as before (Brian, 25 November 16:06Z: a gunner never shoulders the gun like a rifle).

**The tank commander's field glasses** (trial word `glasses`, off; `-TheaterLandNoGlasses` off whatever the word
says): the tankers' files carry a `props.binoculars` track on `commander_hatch` and the two hatch deaths. `LandMen`
reads it (`Crowd::PropFrame`, the prop's place and turn in the world for a rider too) and `RideTanks` draws
`Content/Models/Battle/Binoculars.json` there as instances, once he is over six tenths out of his hatch; nothing is
drawn at the roof gun, where the track says put away. The men's test checks them in his hands at every moment.

**Men who leave with their block** (trial word `leave`, off; `-TheaterLandNoLeave` off whatever the word says;
Brian, 25 November 17:11Z: "some soldiers freeze and then disapear"). A block that falls back off the field or
breaks takes its men out of the recording at `out`, and the record's track ends there: the scene read speed 0 past
its last point, so a running man stopped dead in mid-stride, stood half a second and was cut. On, `LandMen`
(`Crowd::SetLeaveWalk`) carries him on from his last kept point: along his last stretch at its speed where he was
moving (run over 3 m/s, else walk); a man who was still, lying or kneeling gets up first and walks at 1.4 m/s toward
his own side's edge (the block's `side`). He stays whole for 3 s after `out` and goes over half a second, and the
scene hides him for good the first frame he is out of the picture (`Crowd::Leaving`, `Crowd::LeftUnseen`), so where
the camera allows he is never seen to go. Dead men, riders and vehicles are as before. With it, a man out of the
picture whose body was last drawn where the picture now looks is drawn again where he is now, so no body stands at
a place its man has left (the frame log counts them). The men's test plays a running, a cowering and a lying man out,
with the switch off and on (`Tools/land_show`). The bench window (`LandKit::BenchFor`'s `bFarAft`, on the same word `leave`; off, as before): a man seated by his place alone is taken to 8 m aft of his lorry's root (it was 5 m: the rules stand an overfull lorry's last men a row each 1.1 m, to 7.2 m aft, and those had no seat), still by the nearest carrier, so a lorry close behind keeps her own men; the men past her benches kneel down the bed's middle in the order they stand, fitted between her first bench and her last (the old rule put a man 0.55 m further aft for each row past them, which is behind her tail from about the third). A man whose vehicle and seat the rules say is seated as before. `Tools/land_live/seat.sh` checks it (sixteen men by place: 4 on benches, 12 kneeling at least 0.39 m apart, none outside her benches' length; a man 9 m aft on no bed; off, the last four with no seat and the benched men where they sat). Not done here: keeping a man's last seat for half a second when his carrier is not found for a frame; that belongs beside the release 1.5 code that hides him under her canvas, which this branch does not carry.

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
