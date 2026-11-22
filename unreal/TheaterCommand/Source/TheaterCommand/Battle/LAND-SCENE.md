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
5. Other switches for trying the look: `-TheaterLandHand=0..3` (how unsteady the hand-held camera is, 1 by default),
   `-TheaterLandGrain=` (film grain, 0.25), `-TheaterLandNoFocus` (no shallow focus), `-TheaterLandSize=40..100`
   (screen percentage), `-TheaterLandTextScale=` (the screen's words, 1.25). The day's look (any number left empty
   keeps its default, so `-TheaterLandFill=0.42` changes only the first):
   - `-TheaterLandWeather=cover,deck,base`: another day than the recording's (cover 0 to 1; deck 1 for a deck of
     cloud, 0 for cumulus; the cloud base in metres). `-TheaterLandWeather=0.95,1` is an overcast morning.
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
- **A tank struck shows it.** A round that strikes leaves its mark on the plate it struck. A kill brews up, burns or is
  only knocked out, as `Content/Data/Battle/fates.json` draws it, and the hull blackens and smokes.
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

## Packaging

Stage `Content/Data` (LandBattles, Battle, Sound) and `Content/Models` (Soldiers, Battle) with the game, as
`Content/Effects` is staged. Then `Sounds/Real/credits.json` for the credits page.

## On the first build, look at

1. **Facing.** The ground, the men and the vehicles face up and out (no inside-out hedges, no men seen through). The
   field's x is east and y north; `World()` swaps them into Unreal's frame, which keeps the triangles' winding right.
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
