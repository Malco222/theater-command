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
| `LandCrater.h/.cpp` | The shell holes: torn earth laid on the ground for each shell, bomb and grenade that lands in it, sized by the round, kept the whole battle. Plain C++. |
| `LandDesk.h/.cpp` | The screen's words: the title, place and date, the clock, the tug-of-war bar's knot, HOW NEAR TO BREAKING OFF, the dispatches, the after card. Plain C++. |
| `LandField.h/.cpp` | The ground: the bocage's fields and hedges, lanes, the village, woods, slopes, in tiles near and far, and the small things (grass, stones, shrubs) laid round the eye. Plain C++. |
| `LandMen.h/.cpp` | The soldiers: each man's pose from the soldier files' clips (walk, run, kneel, prone, fire, reload), his fall when hit, and his body lying where it fell for the rest of the battle. Plain C++. |
| `LandKit.h/.cpp` | The vehicles and guns: Shermans, Tigers, Panzer IVs, the field guns and lorries from `Content/Models/Battle`, their tracks, turrets, dust, soot, and the fires and cook-offs of a kill. Plain C++. |
| `LandBattleState.h` | The plain state above, kept out of the actor's header so the standard library never meets Unreal's header tool. |
| `LandBattle.h/.cpp` | `ALandBattle`, the scene. It feeds the cores and draws what they say: the ground as merged meshes per tile, the men, the vehicles, the effects kit, the sky and haze, the camera, and the sound. |
| `LandBattleScreen.h/.cpp` | `SLandBattleScreen`, the screen. It draws the desk's words and takes the mouse and keys. |
| `../../../Tools/land_play/` | The cloud test of the fire, sound, camera, words and shell holes: `run.sh` builds it with both compilers' strict flags and plays the recording at 60 frames a second. |
| `Content/Data/LandBattles/normandy-1944.json` | The dry run: Le Mesnil, July 1944, a 3 by 2 km field; 113 tracks, 3,725 rounds; American infantry, Shermans and 105 mm howitzers against German infantry dug in, a Tiger platoon beside Panzer IVs, and leFH 18s. (Tigers in Normandy mostly faced the British; the platoon is kept small for that reason.) |
| `Sounds/Real/*.wav` (95 new) | The ground recordings of the approved battle pack: each army's rifles, machine guns and sub-machine guns, tank guns and engines, artillery and mortars, shells landing, the men's falls, orders and kit. Credits are in `credits.json` and `CREDITS.md`. |

## Running it

1. `Tools/build_sounds.sh` brings the 95 new recordings into `/Game/Campaign/Sounds/Real`. Without it the battle plays
   silent and nothing fails (the log names each missing sound once).
2. `-TheaterLandBattle=normandy-1944` on the command line plays the recorded battle once the table is up. The table is
   hidden while it plays. The clock runs at twice life until the armies are about to meet, then at life.
3. Adding `-TheaterLandBattleShots=<folder>` takes the art director's pictures and then quits: `01-opening`,
   `02-the-advance`, `03-among-the-men`, `04-in-the-grass`, `05-the-tanks`, `06-a-man-falls`, `07-a-tank-knocked-out`,
   `08-the-village`, `09-the-fallen`, `10-after-card`.
4. Other switches for trying the look: `-TheaterLandHand=0..3` (how unsteady the hand-held camera is, 1 by default),
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
  howitzer's 2.8 m and a 150's 4 m. The holes are one mesh of 24 holes a section, on the ground's own paint
  (`M_MapSurface`, unchanged). A section is sent again only when a hole is added to it. At most 2,400 holes are kept.
  The dry run makes 949 of them, about 200 points each. The grass and corn laid round the eye are left out of a hole
  and laid again half a second after a hole lands near them. The effects kit's own flat painted craters are turned off
  in this scene (`SetCraters(false)`); the rest of a shell is unchanged.
- **The camera stays out of the corn.** `LandEye`'s world now has `Crop(x, y)`. The scene builds it from the field's
  own clutter: the tallest wheat or tall grass laid within 0.7 m, the same stalks it draws. A lens near the ground (the
  low shot, the shoulder, beside a tank) rises to 0.12 m over the ears. It rises at once and comes down slowly. A
  low shot the camera chooses for itself where the crop stands over 0.45 m becomes a shoulder shot instead. A low shot
  asked for by name keeps its subject and rises over the ears. The cloud test fails if any frame of those shots has its
  lens fewer than 0.05 m over the crop.
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
