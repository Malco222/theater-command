# Battle effects

Shells and bombs landing, guns firing, tracer, burning wrecks, craters and the camera's jolt, to the art director's
sheet (art-director/explosions/explosions-look.md in the project files).

- `fx.json`: the recipe. Every number lives here. Its copy for the game is
  `unreal/TheaterCommand/Content/Effects/battle-fx.json`; copy it across after each change.
- The game kit: `unreal/TheaterCommand/Source/TheaterCommand/Effects/`.
  - `TheaterFxCore.*` is plain C++ with no Unreal in it. It does all the work.
  - `BattleEffects.*` hangs the core's puffs on one procedural mesh with M_Highlight, and lights up to two shadowless lamps.
- `preview/`: draws the core's own frames in the cloud, with no Unreal.
  ```
  SRC=unreal/TheaterCommand/Source/TheaterCommand
  clang++ -O2 -std=c++20 -I $SRC tools/battle-fx/preview/fxpreview.cpp $SRC/Effects/TheaterFxCore.cpp -o fxpreview
  python3 tools/battle-fx/preview/flatten.py tools/battle-fx/fx.json > recipe.txt
  mkdir -p out && ./fxpreview recipe.txt out how105 earth 0,8,40 1920x1080
  ```
- `burst.py`, `strip.py`, `sizes.py`, `vol.py`: the Blender look-development renders. They need tools/battle-models,
  which lives on the fortifications branch.

## Putting it in the game

```cpp
#include "Effects/BattleEffects.h"
TUniquePtr<FBattleEffects> Blasts;                       // in the actor

// BeginPlay
Blasts = MakeUnique<FBattleEffects>();
Blasts->Begin(RootComponent, UnitsPerMetre, [this](const FVector& P) { return GroundUnder(P); });
if (FBattleEffects::WantsTest()) { Blasts->RunTest(SomePointOnTheGround); }

// each frame (add Jolt.Offset and Jolt.RollDegrees to the camera if wanted)
const FBattleJolt Jolt = Blasts->Tick(GetWorld()->GetTimeSeconds(), CameraLocation, CameraRotation, FieldOfView, ViewWidth, HourLight);

// a field battle's shell
Blasts->Shell(Where, TEXT("howitzer"), TEXT("earth"), /*bOnArmour*/ false);

// a machine gun's burst at a target (Ground: what the rounds kick up, earth if left out)
Blasts->Shots(Gun, Muzzle, Target, TEXT("mg"), 25, 15.0f, TEXT("snow"));

// a tank or towed gun firing on the desert (Ground: what its blast throws up, earth if left out)
Blasts->Muzzle(MuzzleSocket, Facing, TEXT("tank_gun"), TEXT("sand"));

// every frame, each moving vehicle's two dust sockets (dust.left, dust.right); Id the vehicle's number * 2 (+1 right)
Blasts->VehicleDust(Vehicle * 2, LeftSocket, Velocity, TEXT("sand"), /*bTracked*/ true);
Blasts->VehicleDust(Vehicle * 2 + 1, RightSocket, Velocity, TEXT("sand"), true);

// EndPlay
if (Blasts) { Blasts->End(); Blasts.Reset(); }
```

Lamps and day: a flash's lamp is full on the first frame drawn after the flash, about half on the next, then out
(Tick draws first, then lights; call Tick once a frame, or a lamp lasts a single frame). `HourLight` tells the kit how much it is day: by day a lamp is a quarter as strong and
warm white, and small guns light no ground. A flash shorter than a frame is still drawn once, so 30 frames a second
shows every one (and a slow frame shows a flak flash's pieces together, glow, spikes and heart, never the heart alone); a close tracer (under 100 m) is drawn whole, muzzle to strike, for one frame (`tracer.whole_path_s`).
Nothing of a tracer is drawn behind the muzzle, or behind a ricochet's strike point.
A tracer is laid along its true heading in the world, so its ends sit where they truly are; it is as wide on the
screen from end to end, and no wider than `tracer.most_px` (times `day_width` by day) in the middle of the screen, so
one passing close stays a thin whip; toward the screen's edges it draws up to about a sixth wider, as the picture
draws everything there. `most_px` 0 takes the limit off. Test: a hmg tracer passing 3 to 20 m off is long on screen (over 150 px) for 1 to 3 frames at 30 a second.

The debris shower (`burst.debris.shower`): every shell on land throws a dense spray of dark lumps, most streaming
along the crown's jets and flying on past their tips, the rest flung every way, so a hit rains earth for one to three
seconds (about 110 to 150 lumps for a 105, a bit over a hundred more puffs at its height). A ground's `shower` share
sets how much (default its `clods` share; sand and town 0.8, water none). With no `shower` in the recipe the kit draws
exactly what it did before. A water spout leaves a thin pale mist (`grounds.water.smoke_top`, `smoke_alpha`,
`haze_lingers`) and nothing smoulders on water.

The dark foot (`burst.foot`) and the spikes (`burst.crown.spikes`), from the art director's grit direction: on land a
shell's base is a dense mass of near-black lumps for its first second (the body, the dust between the jets, the earth
veil and the smoke's lower part start dark too), settling to brown-grey, and 24 to 36 thin dark rays with a lump at
each tip stand among 3 to 5 heavy jets. Test: on earth at 0.4 s, 25 m off, about a fifth of the burst's box is under
luma 60 (the art director's pass is 15%). The dust ring is 9 m, broken into 5 to 7 uneven lobes, feathered over its
outer 70% and under 1.5 m high (`shock_ring.skirt.lobes`, `feather`, `most_high_m`). With more than
`shower.halve_past` puffs live, a shell throws half the shower and half the spikes. Leave these keys out and the kit
draws as before.

The mend round (art director 03:28Z): each spike is 3 to 5 lumps of unequal width with small gaps (`spikes.pieces`,
`lump_share`, `rough`), its outer lumps drooping under gravity after about 0.3 s (`droop`), and no spike longer than
`longest_of_median` times the middle one, so a 500 lb bomb shows no lone stick. A ground may give its own colours:
`ring` (the dust ring; earth, mud and woods a touch darker than their dust), `foot` (the dark foot; sand's grey-black
explosive smoke, snow's dark earth), `stone` (the shower's lumps) and `foot_dust` (how much of its dust shows in the
foot; snow none). Water's mist is drawn out down the wind (`smoke_stretch`, 1.65 makes it about 1.5 times as wide for
its height) and thins to half by `smoke_half_by_s`. Measured at 0.4 s, 25 m off: earth 22%, woods 19%, sand 6%, snow
5% of the burst's box under luma 60 (snow's white ring covers some of its dark: 7.6% without it; the art director
accepts about 5% there). A lump flung past the lens on a close hit passes no nearer than `burst.debris.lens.miss_m`
(3 to 6 m, so it never fills a sixth of the screen), dark and torn (`lens.rough`, `lens.shade` of the ground's foot),
with no dust puff where it lands beside the eye (`lens.landing_puff` 0). A ground's `paint.scar` and `scar_rays` give
the earth thrown round its crater mark: on snow a brown-black star.

Sun and sky: `SetSun` gives the way toward the sun. Smoke's shadow side never falls below `game.sky_fill` of its own
colour in the hour's light, and looking toward the sun the fringe of a smoke cloud or trail glows warm
(`game.sun_through`). The sun's own light is full by day and none at night, read from the hour's light; the preview
takes it as FX_SUNLIGHT=r,g,b.

Names given to the kit (size, ground, calibre, gun, weapon, air gun kind) are matched in lower case, as in the recipe:
`howitzer`, `snow`, `bomb250`, `hmg`. The wrapper lowers them, so `Snow` finds snow; a name the recipe does not have
falls back (an unknown ground to earth, an unknown size to how105, an unknown air gun to hmg).

## Desert dust

From the Mediterranean set (03:35Z) and the art director (06:19Z): on a desert field everything that moves or fires
raises dust, and that dust hides things.

- Vehicles (`dust.vehicles`): `VehicleDust` every frame for each dust socket lays a puff every `spacing_m` along its
  path (no more than `most_a_second`). Pale fine dust: the ground's colour where it is thrown up (`low`; sand the
  stony gravel 170,140,106) losing colour as it climbs (`high`; sand a pale buff grey 210,196,170, so the top of the
  cloud reads as haze in the air, about saturation 0.2; other grounds their `paint.dust`, and `colour_from_ground`
  times it in the air). Low and long, drifting on the one wind (`SetWind`), lingering: dense for
  `haze_by_s`, then a thin haze to its end. Taller and longer at speed: between `least_mps` and `full_mps` the puffs
  grow from `r1_m` to `r1_fast_m`, `flatten` from 0.6 to 0.9 and `life_s` to `life_fast_s`. Half are low dense skirts
  (`low_skirt`), thick from the ground to a vehicle's height, under the soft tall billows. Tracks throw `tracked` times
  more than wheels. `by_ground` scales it: sand 1, earth 0.55, town 0.5, woods 0.25; snow, mud and water none. Standing
  still, nothing. Past three fifths of the room a trail lays half as many; past `0.85` none. The room here is the
  fuller of the whole room and the dust's own share of it (`most_of_room`, 0.4; the pre-check, 07:10Z), so a long
  column's dust never cuts a salvo behind it; a vehicle within `full_within_m` (120) of the eye keeps its whole
  trail, so from a man's eye the column in front still hides what drives behind its lead (60 tanks: 9, 2, 0% seen).
- Guns (`muzzle.veil`): `Muzzle(..., Ground)` on a ground in `veil.by_ground` (sand) raises a veil from a gun with a
  `dust_ring_m` (tank, anti-tank, howitzer; a towed gun's `muzzle` socket too), spread along the line of fire and
  round the gun, every other puff a low skirt. It hides the gun and its line of sight for `hide_s`, then thins to
  `haze_alpha` and lingers on the wind. The ring itself takes the ground's own paint, as thick as its
  `grounds.<g>.dust` against earth's (on earth exactly as before).
- Pass check, at a man's eye (1.7 m): a column of four tanks at 10 m/s, 30 m apart, seen from 70 m ahead and 10 m off
  its line: the lead in the clear, the three behind it 0 to 16% visible through its dust (the same with the dust blown
  across or toward the eye). A towed anti-tank gun seen 45 m down its line of fire: 1 to 8% visible for 1.5 s after a
  shot, 24% at 2 s, 85% at 5 s.
- The preview: `./fxpreview recipe.txt out drive sand 0,60 960x540 eye` (shots `eye`, `side`, `high`, `top` (the
  overview, `FX_DIST`, `FX_DOWN`), `gun`;
  `FX_SPEED`, `FX_GAP`, `FX_VEHICLES`, `FX_WHEELS=1`, `FX_FIRE=1`, `FX_EVERY`, `FX_AHEAD`, `FX_OFF`, `FX_WIND_X`,
  `FX_WIND_Y`). Each frame prints how much of each vehicle, or the gun, still shows through the dust in front of it.
- From above (`from_above`, art director 06:48Z): a player always finds his own units. With the eye more than
  `above_m` up (15 to 25) and looking steeply down (`steep`, the sine of its look down, 0.3 to 0.5), the dust within
  `near_m` (8 to 14) of a vehicle laying dust, or of the gun a veil came from, thins to `keep` of itself. The long
  trail behind a column stays: from above it is how a column is spotted. Ground and follow eyes keep the full veil.
  Measured: the overview 150 m off at 50 degrees, 400 m at 60, and 80 m at 40 show every tank 66 to 84% (from the
  man's eye still 2, 0, 0%). Unit markers and banners are drawn over the effects, never under.
- The bleached, hazy horizon of a desert day is the grade's, not the kit's.

## Gun cases on the ground

Ground guns throw out their spent cases (`ground_cases`), with no new call: `Shots` and `Muzzle` lay them when the
eye is within `within_m` (40) of the gun, and only for the guns named there.

- A machine gun (`mg`, by `Shots` with weapon `mg`): each round drops a rifle-calibre case (`case_m` 0.06 long) out of
  the bottom of the receiver (`breech_back_m` behind the muzzle, `below_m` under it), as the war's machine guns did,
  falling with a little scatter (`scatter_mps`) to build a small heap under and beside the gun where the gunner lies.
  The heap lies a minute or two (`lie_s`).
- Towed and field guns (`at_gun`, `howitzer`, by `Muzzle`): after the recoil (`after_s`, 0.8 and 1.5 s) the big case
  (0.6 and 0.4 m) drops out of the breech behind the gun (`breech_back_m`), tumbles slowly and comes to rest lying
  flat. It stays three to four minutes (`lie_s`), so a gun that has been firing sits on a growing pile of brass.
- Tank cases stay inside the tank: `tank_gun` has no row.
- Each case is a brass tube lit as metal (art director 07:08Z), square-ended and straight-sided from any side: across
  it a bright band toward the sun (`colour` 196,156,82) with a gold glint (`glint`, `glint_colour`, `shine`), down to a
  shade side and underside (`shade` 112,82,37); a lying case spans about 130 levels of luma. The open mouth (`mouth`)
  shows dark with a brass lip when that end is turned to the eye, and once it lies there a soft contact shadow
  (`contact_shadow`) sits under it. Each case takes up to three places, and is counted so in the room (the
  pre-check, 07:35Z); narrower than `detail_px` (3) on the screen it is laid as its tube alone, in one.
- Past `most_lying` (200) cases lying at once, a new one lies only a moment. Past `draw_within_m` (150) a case is
  neither counted nor drawn, fading over the last quarter. Below `least_px` on screen it is not drawn. Cases take no
  room past three fifths of it, and they use their own dice, so every other effect draws exactly as before.
- The preview: `FX_MG_VIEW=gunner ./fxpreview recipe.txt out mg earth 20,120 1280x720` (the MG from beside its
  gunner: the brass falling, then the heap); `FX_EVERY=4 FX_AHEAD=-3 FX_OFF=6 ./fxpreview recipe.txt out drive earth
  0,30 960x540 gun` (an anti-tank gun firing every 4 s, its pile building).

## Close flashes of ground guns

Art director 07:08Z: within `muzzle.hot.within_m` (50) of the eye a ground gun's flash, a machine gun's (`Shots`,
`mg`) or a field gun's (`Muzzle`), is lit from within as the air guns' close flash: a yellow-white root at the muzzle
(`root` to `root_edge`, luma 241 to 244) inside an orange body (`orange`, its rim `rim`) with dense rims, shrinking
rather than going see-through. Farther off, and in a recipe without `muzzle.hot`, as before.

Art director 13:54Z, on the game's own frames (build 1720), where that flash read as a sticker (one flat orange fill
with a cut edge and a white oval laid on it). With `muzzle.hot.tongues` in the recipe the close flash is built anew
each round (`HotFlame`): a small white-yellow root right at the muzzle (`root`), melting into `yellow`, then the
`orange` body, widest a third of the way out, which breaks into 3 to 5 ragged tongues with `red` only at their ends,
and 2 to 4 short licks thrown sideways. Its length is rolled between `long` (0.6 to 1.3) times its usual length
(`mg_flame_m` for a machine gun, 1.4 x `flame_m` for a gun), so no two flashes match. Dense inside, its edge a pixel
or two soft; a thin glow (`glow`, `glow_m` past the body, `glow_day_share` of it by day). A burst's flames are laid one
at a time as each round fires, so a long burst never holds hundreds of pieces waiting.

After a burst of 3 rounds or more, close to the eye, `mg_smoke` leaves a thin pale wisp at the muzzle (never over a
quarter opaque) drifting on the wind for a second or two, and on dry ground a low puff of the ground's own dust just
in front of the muzzle.

## What the game's camera showed (build 1720)

Four things the kit learned from the first machine gun frames out of the real game. All are behind recipe keys or
the view's new fields: an older recipe, or a caller that does not give the view's width, draws as before.

- **Pixels are drawn pixels.** The game draws the 3D map smaller than its window (1072 across on a 1600 still, about
  1080 across on a 2881 window) and scales it up. Give `Tick` the *drawn* width, so `least_px`, `detail_px` and the
  tracer's widths count pixels that are really drawn, and a thing held at 2 px is 2 px on every screen. The art
  director's pixel numbers are measured on 1600-wide stills: two thirds of them in kit pixels.
- **What is off the screen takes no place.** `View::HalfAcross` (the tangent of half the view across; the wrapper sets
  it) lets `Draw` leave out what is wholly outside the view, with room to spare for its size and the eye's shake. It is
  still counted in the room asked (`RoomAsked`), so nothing more is made than the whole field has room for; but the
  close grit goes by the room on the screen (`RoomSeen`). In build 1720 the test barrage, off the screen behind the
  eye, held the room three quarters full and the machine gun in front of the eye threw no brass at all.
  `preview/barrage.cpp` replays that barrage and prints the room and the cases thrown.
- **Brass that reads from 19 m.** There a 6 cm case is under a pixel across and its lit and shade sides average to
  the ground's own tone. `ground_cases.hold_px` (1.6): never narrower on the screen; held, its glint is brighter and
  wider and its shade side darker (`far_glint`, `far_shine`, `far_shade`), 40 luma over and under pale ground; whole
  while its true width is over `full_px`, gone under `gone_px`. `nearer_m` (0.05): laid that much nearer the eye in
  the same place on the screen, so the game's flat-faced ground never buries it. Close to, a case is as before.
- **White that lands white.** The game's tone curve holds plain white down to about 224 of 255. `picture.lit_gain`
  (1.5 then; 4 since build 1732, see the game's own picture below): the wrapper sets the material's `Brightness` to it and the kit writes every colour that much darker, so all
  looks as it did; the hottest lights (`muzzle.hot.over`, `tracer.day_over`) are written up to that much past white.

By day a tracer is now a solid pale core (`tracer.day_core`, `day_core_px` 2) inside a warm edge (`day_edge`),
`day_px` (3.4) across in all, straight-sided and at its whole opacity along its whole length: 90 luma over pale
ground where build 1720 had 25. At dusk as before.

Preview: `FX_PALE=1 FX_MG_VIEW=game FX_LENS=42 ./fxpreview recipe.txt out mg sand 0,2,4,60,120 1072x670` is the game's
own camera on the machine gun (18.75 m off, 22 degrees down, a 42 degree lens, the drawn size, pale dry ground).
`FX_NOCULL=1` lays everything in range, as before the view's width was known.

## The game's own picture (build 1732)

The game does not show a colour as the kit writes it. Every pixel goes through Unreal's film curve and the grade set
in `Campaign/CampaignCameraPawn.cpp` (a fixed exposure, saturation 1.07, contrast 1.08, warm highlights, cool shadows,
film toe 0.6). Plain white shows as about 226; grey 60 shows as 31, so dark paints sink toward black (a clod's
40,33,26 shows as 11,3,3); a strong orange loses its blue (235,140,40 shows as 234,155,0, candy). The art director's
ruling of 16:30Z: the game-wide curve and bloom stay as they are, and the kit's colours are written to land.

- `tools/battle-fx/game_curve.py` is that road in numbers (numpy), checked against build 1732's stills to about five
  levels. `python3 game_curve.py lands 235 140 40` says what a written colour shows as; `lands 250 240 200 x1.5` the
  same for a light written past white; `write 235 140 40` what to write, and how far past white, to land on it;
  `show out/mg-sand-f002.ppm.f32 1072 670 shown.png` puts a preview frame through it. `FX_FLOAT=1 ./fxpreview ...`
  saves each frame's light beside its picture (`.ppm.f32`, three floats a pixel, unclipped) for that.
- `picture.lit_gain` is now 4, so the flash's root (`muzzle.hot.root` 255,233,158 at `over` 4) lands a warm white,
  253,250,234, over a spot 5 to 9 px across on a 1600 still, where it was cream at 235,231,216. `muzzle.hot.halo`
  (0.08 then, 0 since build 1746) and `halo_m` (0.028): a soft card of the root's colour round it, in place of bloom. `yellow_over` (1.175)
  keeps the yellow as it was. `muzzle.hot.orange` 236,131,71 lands 234,141,41.
- `tracer.day_core` 255,241,151 at `day_over` 2.72 lands the core on 245,240,210 and over.
- `mg_smoke.colour` 228,232,238 and `puff_share` (0.63): the wisp reaches the whole 0.25 where its puffs overlap.
- Colours are written in 8 bits of linear light, each `lit_gain` times darker. At 4 the darkest paints keep fewer
  steps; through the game's curve a howitzer burst, a burning fall and an oil fire moved by 2 to 5 levels in their
  darks, 12 at most. Should dark smoke ever band in the game, the cure is in the material (bytes written with a
  gamma and squared there), not a lower gain.

## A gun's smoke and dust in lumps (build 1746)

In build 1746 the test's tank gun, 4 m beside the machine gun's riflemen, laid an even beige gauze over the whole west
view: 17 big soft puffs of one grey-brown along the bore, 13 m from the eye. Art director 17:52Z: a heavy gun blinding
the men beside it is right, but it has to read as dust and not as fog. Lumpy, heaviest low and near the muzzle,
thinning upward, the thick part clear in 1.5 to 2 s with low tufts drifting longer, and faded out near the camera.

- `muzzle.billows` (with `low` in the recipe, and a gun of `flame_m` 0.5 or more) builds the cloud in three layers in
  place of the old smoke. **Low**: mounds of the ground's own dust lying on the ground along the first `reach` of the
  dust ring, `low_alpha` dense; `tuft_share` of them settle to `haze_alpha` and drift for `tuft_s`. **Mid**: lumps of
  dust mixed `smoke_mix` toward the gun's smoke, `mid_up_m` above the ground, thinner and shorter-lived with height
  (`pale_up`). **Top**: the gun's smoke thrown along the bore in lumps, thinner the farther and the higher. Each lump
  has a firm heart and a rough outline (`heart`, `inner`, `rough`) and is lit as part of one cloud (`shade`), so there
  are thick parts, thin parts and gaps. The thick part is gone in `clear_s` and `top_s`. The low dust ring and the
  desert veil (`muzzle.veil`) are as they were. Counts follow the gun's size and are capped; its own dice, so a recipe
  without it draws as before, piece for piece.
- `Bit::Rest`: a lump that rests on the ground never has its middle lower than that share of its radius above the
  ground under it, so the ground never cuts it along a straight line.
- `game.near_eye_fade_m` (2, 8): see-through smoke and dust thins out as its near edge comes within 8 m of the eye
  and is gone at 2 m, so it never fills the whole frame evenly. Lights, thrown pieces, ribbons, smears and the kept
  columns (oil fires) are left alone. At the battle camera a burst's own puffs stay farther off than that (a 105's
  nearest edge 8.4 m), so the bursts look as they did; it is drifted dust and smoke over the eye that goes.
- `muzzle.hot.halo` is 0 (art director 18:05Z): the game's bloom gives the glow.
- The test barrage (`BattleEffects.cpp`) fires its tank gun at 20.0 s, after the machine gun's bursts at 15.5 and
  18.0 s, with a third burst at 20.5 s that shows the men in the gun's dust.
- Wild numbers: `dust_ring_m` is read to 30 m at most and the old smoke's count to a 4 m flame, so a recipe with a
  number that is not one cannot ask for pieces without end.

Preview: `FX_CREW=1` stands two riflemen at the machine gun; `FX_TEST_TANK=0.5` fires the test's tank gun that long
before the burst, with the test's wind (`FX_TANK_UP=1` lifts its muzzle a metre, the game's ground there;
`FX_TANK_ROW` another gun's row); `FX_GAME_FROM=208` is the builder's west view. `FX_BARRAGE=5` (any shell's shot) lands
that many in a row across the view, `FX_BARRAGE_STEP` m and `FX_BARRAGE_GAP` s apart; with the wind (`FX_WIND_X`,
`FX_WIND_Y`) toward the eye, the eye stands downwind in their dust.

## Lighter burst dirt, smoke that stands on the sea, the desert veil in lumps (after build 1746)

The art director's round of 18:28Z, on build 1746's stills.

- **A burst that reads as earth.** From 60 m, on its shadow side, a 105's cloud read as an ink blot (smoke 36,30,24).
  It was not `lit_gain`'s steps (the crops held 40 to 48 levels, and the kit gave the same dark means at gain 1.5):
  it was the paints sinking through the grade. Earth's, mud's and woods' `paint.soil` and `paint.smoke` and the
  cloud's top (`burst.smoke.colour_top`) are now written values, as the clods are, aimed with `game_curve.py write`
  to show as they were painted; `paint.heavy` stays, so a dark heart sits inside lighter brown dust. Through the
  game's curve the cloud's dense half on the shadow side goes from 44,37,28 to 61,53,44 at 2 s and from 58,48,35 to
  77,67,55 at 6 s. The burst-black share at 0.4 s (under luma 60, the art director's 15% rule), through the curve:
  earth 41% (was 46%), woods 38%, mud 38%, sand 17%. Snow keeps its old, darker soil, smoke and charcoal
  (`grounds.snow.smoke_top`), by the art director's word: black earth thrown over white snow is the strongest picture
  we have (12% there, as it was).
- **Smoke that stands on the sea.** `fire.smoke_rest` (1): a plain fire's smoke (`Burn`) rests on the ground or sea
  under it (`Bit::Rest`). Its puffs grow to two to four times the fire's width while climbing only 20 to 30 m, so a
  40 m ship fire's stood half under the sea, and from the air the sea cut each along a straight level line (Pearl
  Harbor, build 1746). A small fire's smoke already rides higher than it is wide and does not move; a crater's
  smoulder lifts a few centimetres at its foot (one level over a hundred pixels).
  `oil_fires.<kind>.pall_rest` (1.3): the same for the oil fire's pall, should a puff wander down.
- **The desert veil in lumps.** `muzzle.veil.lumps` (1): the veil's ten upright sheets become as many lumps, each
  `lump_r` of a sheet's radius across and `lump_thick` of its thickness, with the billows' firm heart and rough
  outline; every other one a mound on the ground, the rest up to `lump_up_m` over it and thinner the higher.
  `lump_tuft_share` of them thin to the haze and linger as the veil did. Twice the count and size made a pale wash
  over the lower frame.
- **The test's ground.** `RunTest(Centre, GroundKind)`: the test barrage's tank gun and machine gun raise the dust of
  the ground they stand on (pale on Idaho's dry plain, "sand"); the shells keep their own test grounds.

Each is behind its recipe key: the last recipe draws as before on this code, frame for frame. Preview:
`FX_BURN=40 FX_DIST=1462 FX_EYE_H=651 FX_LOOK_H=0 ./fxpreview recipe.txt out oil sunk 450 1440x893` is a 40 m plain
fire on the sea from the air battle's eye (`FX_BURN_UP` m its base over the sea).

## A ragged shell flash, a gun's dust on dry ground, a dark foot to a ship's smoke (after build 1748)

The art director's round of 19:57Z on build 1748's game proofs.

- **The shell's flash.** `burst.flash.tongues` and the keys after it: a ground burst's flash is no longer an even
  star of seven flames. A small heart burns near white and warm for its two frames (`heart` written `over` times
  past white; it lands about 252,248,232 and is `heart_share` of the flash's reach across), a yellow body stands
  round it (`yellow`, `yellow_over`: at least as bright as pale ground, luma about 214), and `tongues` ragged
  tongues of uneven length and lean are thrown up and out with the dirt, yellow-orange at the root and red-orange
  at the tip, with `licks` short ones low at the sides. Every edge fades out over `feather_m`; each piece is two
  fans half a step apart, so no facet shows. `reach_m` is the longest tongue for a 105, `life_s` two frames. Its
  own dice, by the place and the moment: no two rounds alike. Body and flames keep blue under 110.
- **A gun's dust on dry ground.** `muzzle.dry`: from the battle eye, build 1748's ring sheets, mounds and veil,
  some sixty soft pale pieces, were one sheet of fog over a third of the frame, and the men 4 m from the muzzle were
  grey shapes. On a ground named in `by_ground` a heavy gun now raises `lumps` dense lumps instead, one to a
  stretch of the line of fire with clear ground between, the heaviest by the muzzle, every third standing higher,
  none thinning to a haze: the far ones gone by `clear_s`, the two or three nearest holding on to `near_s`,
  thinning, as the cloud that gives the gun away. Each lump is a mound with a darker underside (`belly`) and a
  lit crown riding on it (`crown_r`, `crown_up`), of its own shade (`shade`). The gun's own smoke is `smoke` times
  as many wisps and takes `smoke_as_dust` of the dust's colour.
- **The dust's colour is the ground's.** The game hands the kit the ground's colour as its SCREEN shows it:
  `FBattleEffects::GroundColour` (a `TFunction`, red, green and blue as pixel / 255), which the kit asks once a
  shot through `Battle::SetGroundColour`. The lump's crown is that colour `lighter` times as light, written to
  land on it: `picture.to_land` is the game's curve turned round (`python3 game_curve.py table`; make it again if
  the grade changes). Where the game cannot say, the ground row's own dust paint. In the preview the dust's
  lightest lands 1.09 times a plain shown as 134,124,92 and 1.06 times one shown as 176,159,117, its undersides
  about 0.9; the box round the men changes by 5 to 8 levels at the worst (0.5 s) where build 1748 changed it by 27
  to 40.
- **A dark foot to a ship's smoke.** `fire.smoke_foot`: resting on the sea (`smoke_rest`), a big fire's old smoke
  had paled to a sunlit grey bank at the column's foot and read as steam (Pearl Harbor, build 1748). While a big
  fire's puff lies on the ground or sea it keeps the fire's own dark, after the flames too, and pales only as it
  rides `smoke_foot_clear` of its radius clear of where it rests (`Bit::FootClear`). Big: from
  `smoke_foot_from_m` across; a tank's or a house's fire smokes as before.

Each is behind its recipe key and throws its own dice: build 1748's recipe draws as before on this code, frame for
frame. Preview: `FX_SHELL_AT=s` the shell's moment, `FX_SHELL_ROUND=n` another round (n tenths of a metre aside),
`FX_LOOK_HIT=m` looks at the hit; `FX_GROUND=r,g,b` the ground as the game's screen shows it (told to the kit),
`FX_GROUND_RAW=r,g,b` what to paint so that `game_curve.py show` lands on it; `FX_GAME_BACK=60` the battle eye
from 60 m; `FX_BURN_ASIDE=m` a plain fire that far aside of the oil fire.

## A big fire's smoke as a bank of lumps down the wind (after head 71a0d19)

The art director's item of 03:30Z, 20 November, on build 1750's Pearl Harbor (Akagi, picture 06): resting on the
sea, a big fire's smoke was a smooth, soft dark oval with no lumps and no edge, a stain or a hole in the water
from the air. From straight above a 40 m fire's was a round disc some 250 m across.

- **The bank.** `fire.smoke_bank`: a fire `from_m` across or more lays its smoke as lumps of the oil column's own
  kind (`rough`, `heart`, `twin_px`), run out down the wind (`Battle::SmokeBank`, the row read once as the fire
  starts, `Battle::BankRowOf`). Each lump is born on the fire and runs down the wind at the pace that takes the
  longest lived to the bank's far end, wandering to either side (`wide`, `spread`, `long`). All live through the
  first `short` of that, piling into one dark mass at the fire; fewer and fewer live on (`thin`), each solid for
  `hold` of its life, so down the wind it thins to lumps apart with clear sea between. From straight above a
  40 m fire's bank measures 2.3 times as long as wide at 60 s and 2.9 at 90 s.
- **Its light.** Most lumps lie on the sea (`rest`) and keep the fire's dark (`smoke_foot`), each its own shade
  (`shade`); every third or so lifts (`heads`, `head_rise_mps`) and pales as it rides clear. The lumps on the
  bank's sunward side are lit toward `lit_colour` (`sun_side`), those on the far side left dark, as the oil
  fire's pall. A young lump is lit `sun_young` as much until `sun_from` of the longest life and in full by
  `sun_by` of it (`Bit::SunYoung`, `SunFrom`, `SunBy`): the dark pile at the root takes a tenth of the light,
  lands at luma about 32 and near neutral, as the foot passed before, and reads by its edge against the sea; the
  light comes in through the middle of the bank, so the far lumps keep their lit tops.
- **Held.** Every dial is clamped in `BankRowOf` and one that is not a number takes its usual value; a fire never
  has more than some three hundred lumps; `Battle::Burn` holds a fire's width to 5000 m (an endless width from
  the caller once laid points at infinity).

Behind its recipe key, with its own dice: head 71a0d19's recipe draws as before on this code, frame for frame,
fires of 3, 9, 12 and 40 m among them. A fire under `from_m` (a tank, a house) smokes as before.

## A burning ship's own column (after head 335b669)

The art director's item of 04:06Z, 20 November: a burning ship gets a column of her own, of the oil column's kind
but about half its width: black at the root, in lumps, leaning down the wind, rising a few hundred metres before
it bends flat, the bank lying on the sea beneath it.

- **What the game already lights.** Every ship lost but a submarine gets an oil fire 0.6 of her length across
  (`AAirStage`, at most `MostOilFires`), so Akagi in build 1750 had her column: the soft blur in picture 06 was
  the deck fires' old smoke discs in front of it. With the bank the column stands clear; this round changes
  nothing for a lost ship.
- **Her own column.** `fire.smoke_column` (`Battle::OwnColumn`, a `Source` of kind 3): a plain fire `from_m`
  across or more that burns `from_s` seconds or longer sends up the oil column's own puffs and pall, read from
  this row by the oil row's keys (`Battle::OilRowAt`), sizes as shares of the fire's width. A 40 m fire's column
  is 35 to 55 m across and stands 200 to 280 m before it bends. A shorter fire (a crashed aeroplane's 90 s) and a
  smaller one (a bomb hit on a destroyer) lay their smoke as before.
- **Black at the root.** `sun_young`, `sun_from_s`, `sun_by_s`: a puff has a tenth of its lit side until 4 s old
  and all of it by 16 s, so the first 40 m or so of the column is black and the lit lumps come in higher up. Any
  oil row may ask the same; none asks, so the oil fires draw as before.
- **One column to a ship.** A fire lit within `join_m` of a place a standing column rises from joins it: the
  column then rises from both fires and from between them (eight places at most), stands on their middle, and is
  `join_wide` of her burning length across, never wider than a fire `join_m` across would send up. A carrier
  ablaze from end to end (three 33 m fires 78 m apart) sends up one column of lumps 40 to 64 m in radius, about
  half her oil column's.
- **Under burning oil, none.** While an oil fire burns within `under_oil` of its reach of one of her fires, the
  column lays nothing (`Battle::UnderOil`): the oil's column is hers. The picture with her oil fire is the same
  to the byte with and without this row. A column that oil has stood over has stood down for good
  (`Source::bStoodDown`): it does not stand again when the game puts that oil out, and a fire lit under burning
  oil starts none.
- **Cost.** A column is 95 to 130 puffs, kept when the screen is full as an oil column's are; `most` (6) stand at
  once and never more, a fire past that lays its bank only. The choice is made as the fire is lit, never by the
  eye. One that has stood down is not counted, and since it never stands again the count holds (the 3D
  pre-check's scene of four ships under oil until 150 s and ten more fires: 6 at once, where 10 stood before).
- **A ship struck again and again** keeps her one column: a fire that joins after the eighth place adds none.
- **Held.** `OilRowAt` now holds every oil row dial to a sane span and gives one that is not a number its usual
  value (an oil row of 1e9 heads once ran out of memory, one of `inf` laid points at infinity), and never lets a
  row ask for more than some four hundred puffs of a kind to a fire; `OilFire` holds a reach to 5000 m. A sane
  row is not changed by it. `Burn` and `OilFire` light nothing at a place that is not a number or lies beyond
  1e9 m (such a place once laid points that are not numbers).
- **The bank's fringe.** The bank's lumps let the sun through by how high each stands over the ground or sea
  under the fire, never over a deck fire's own height, so back-lit smoke under a deck fire glows as it does by a
  fire at the waterline.

Behind its recipe key, with its own dice: head 335b669's recipe draws as before on this code, frame for frame,
the oil scenes among them. The kit cannot tell a ship's fire from one on land: a big, long fire ashore (the
atoll's tanks and hangars) sends up a column too.

`FX_BURN_ROW=n,m` lays n plain fires m metres apart, centred where `FX_BURN_ASIDE` puts the fire, turned
`FX_BURN_TURN` degrees. Near build 1750's Akagi 06: `FX_LENS=42 FX_BURN=33 FX_BURN_ROW=3,78 FX_BURN_TURN=15
FX_BURN_UP=9 FX_DIST=830 FX_EYE_H=200 FX_LOOK_H=60 ./fxpreview recipe.txt out oil sunk 3600 1072x670`; add
`FX_BURN_ASIDE=0.01 FX_REACH=156 FX_SINCE=90` for her oil fire. The lone 40 m fire in the game's air view:
`FX_BURN=40 FX_DIST=520 FX_EYE_H=330 FX_LOOK_H=60`.

## Oil fires at sea

```cpp
// a ship lost in a sea battle: one call for each smoke zone (id, x, y, r) when the scene opens or she goes down
Blasts->OilFire(Zone.Id, ZoneWorld, Zone.R, TEXT("sunk"), /*Until*/ FightEnd, /*Since*/ BattleNow - Zone.At);
Blasts->OilStop(Zone.Id); // only if it should go out before the fight ends
```

- `sunk`: oil burning on the sea where she went down. An oval slick (`slick_long`, `slick_wide` of Reach) with a
  broken line of low ragged flames along it (`flame_odds`, `flame_point`), a low dark roll as wide as the slick hugging
  the water (`base_flat`), and one fat black column standing up off it, about 350 m tall for a 100 m slick. Its top
  never closes: from where the push gives out it runs nearly level down the wind as a widening sheet two to three
  times the column's height long, fanning out and breaking into loose lumps as it thins (`pall_*`; `pall_level` rounds the elbow). `burning`: where a ship on fire left the line, a
  narrower, greyer column on a small grey roll; give WorldAt her deck's height to put the flames on her.
- The column reads round: lit as a round column by where each point sits across it on the screen, its width measured
  from its own puffs each frame. Sunward side brown-grey (`lit_colour`, `sun_side`), far side near black, a high sun
  lighting a little all round (`oil_fires.round_up`); with the sun behind, dark with a warm fringe on its own edge only.
- One wind for every column: `SetWind`. `Since` grows the column at once for a scene that opens mid-fight.
- The column is kept when the screen is full (`oil_fires.<kind>.keep`) and drawn out to `oil_fires.draw_beyond_m`
  (20 km), so 2 to 4 far columns stay on the horizon. About 260 puffs for a sunk ship's fire, 190 for a burning one.
- At night the flames light the sea round them (`lamp`, `lamp_reach`).
- The preview: `./fxpreview recipe.txt out oil sunk 45 1280x720` with `FX_SINCE=300 FX_DIST=1300 FX_LOOK_H=200` (`FX_SUN=x,y,z` the way toward the sun);
  `oil burning`; `oil horizon` (three fires 3, 6 and 9 km off from an 8 m eye).
- `-TheaterBlastTest` also lights two, already burning: a slick 900 m off and a burning ship 2.5 km off.

## Guns on aircraft

```cpp
// every frame, for each gun whose trigger is down (Gun: a number that stays the same for that gun)
Blasts->AirGun(AirNow, Gun, MuzzleWorld, AimPointWorld, AircraftVelocity, TEXT("hmg"));
// when that trigger lets go
Blasts->AirGunStop(Gun);
```

- `AirNow` is this frame's time on the clock given to `Tick`. An air scene with its own air time (slowed when the
  player slows the battle) keeps its own FBattleEffects, made with the scene's own units a metre, and gives that time
  to both `AirGun` and `Tick`, so gunfire slows with the scene.
- The air scene calls `Blasts->UseAirDistance()` once after making it: its strategy camera stands 800 to 2,000 m off
  the fight, so it draws out to `air.draw_beyond_m` (4 km), not the land battle's 1.5 km. From there every muzzle
  flash and hit is held at 2 px on its flash frame (`air_guns.flash_least_px`, `air_hit.least_px`) and a fire's
  heart at 4 px (`air_trails.fire.flame_least_px`): far off they read as twinkles, drawn from the kit's own shapes.
- The battle decides lead and convergence: each gun gets its own aim point, and wing guns converge where it says.
- `AircraftVelocity` is in world units a second. Rounds fly at the gun's speed plus the aircraft's, slow in the air
  and fall. Rate and muzzle speed come from `air_guns` in the recipe (`rifle`, `hmg`, `cannon20`); pass your own to
  override (rounds a minute, world units a second).
- Every round flickers at the muzzle; one in five is the ruled tracer. Cannon also throw licks and light the wing.
- The close view (art director 04:40Z), from the eye of the last frame drawn, so nothing to call: a heavy machine gun
  within 30 m (`air_guns.close.licks_within_m`) throws 0 to 2 short licks a round, raked aft, and a small lamp
  (`close_licks`, `close_lamp`; by night only, as every small gun's). Within 60 m (`cases_within_m`) every round of
  every gun drops its spent brass case and steel belt link from a chute about a metre behind the muzzle and under it
  (`chute_back_m`, `chute_down_m`; a turret's guns dump theirs overboard the same way). The air tears them aft at once
  and they fall, tumbling and glinting, gone in 1.2 to 2 s: a firing wing trails a thin spray of brass. Sizes are the
  row's own (`case_m`, `case_d_m`, `link_m`). They are the first thing left out when the screen is full (past three
  fifths of `game.max_puffs`). The preview: `... air hmg 20,40 1920x1080 close` (about 12 m off the right wing;
  `FX_CLOSE_OUT` metres out, or `FX_CLOSE_AFT=1` from behind and under it).
- The hot flash (art director 06:19Z): lit from within, as the fireball. A white-yellow root at the muzzle over the
  first quarter (luma 235 or more), a saturated orange body (`air_guns.flash_orange`) going to a deeper orange at its
  edges (`flash_rim`, so a rim over the sky never turns peach), a fading tip, no dark crease. It stays dense to its
  edge; the second frame is about half by shrinking, never by going see-through. The heavy machine gun's close view
  (its flicker and licks) is always hot; a cannon's whole flash is where its row sets `hot_flash` (the 20 mm does: a
  core of luma 240 or more, body saturation 0.7 or more, twice the heavy machine gun's length).
- The preview draws it: `./fxpreview recipe.txt out air hmg 15,45 960x540 side` (shots `chase`, `side`, `far`,
  `close`, `target`, `hit`, `trail`, `wake` (the whole trail from `FX_DIST` m off the beam), `map` (the strategy camera,
  `FX_DIST` m off); `FX_DUSK=1` for dusk, `FX_TURN=deg/s` for the turn, `FX_TRAIL=smoke|fire|glycol`, `FX_SPAN=m`).
- A ground machine gun's close burst: `./fxpreview recipe.txt out mg earth 0,1 1280x720`, seen from `FX_MG_VIEW=side`
  or `behind` the gun.

## Hits on aircraft and trails

```cpp
// a round striking an aircraft (Kind: the gun that hit)
Blasts->AirHit(AirNow, HitPointWorld, SkinNormal, TargetVelocity, TEXT("cannon20"));
// an engine trailing: once, then every frame, then when it ends
Blasts->TrailStart(AirNow, EngineId, EngineSocketWorld, TargetVelocity, TEXT("smoke"), /*SpanMetres*/ 10.5f);
Blasts->TrailMove(AirNow, EngineId, EngineSocketWorld, TargetVelocity);
Blasts->TrailStop(EngineId);
```

- `TrailStart` again on a running trail changes its kind, such as smoke catching fire (`fire`).
- Give `SpanMetres`, the aircraft's wingspan: a fire's flame is half the span, at most 10 m (`flame_of_span`,
  `flame_most_m`), ragged and flickering, streaming back from the root, with black smoke from its tips. Without it the
  flame keeps the row's `flame_m`.
- By day the flame must read as fire off the skin (art director 04:59Z): its tongues stand off by
  `flame_lift_of_length`, licking out above and below the fuselage, opaque (`flame_alpha` at the cowl and the tip,
  `flame_hold`, `flame_wide`, `lick_alpha`), and `tip_smoke` lays black smoke right at each tongue's tip, so flame and
  smoke join with no gap (about 27 more puffs a burning aircraft). A tongue is rooted on the skin at the fire, with the
  yellow-white heart there; only its tip licks out (`flame_lift_curve`, 05:25Z) and goes orange to `flame_tip_colour`
  (dark red) into the smoke. Put the trail's point at the fire: the engine's exhaust, or the wing root for a tank fire,
  whose tongues sweep back past the tail.
- Smoke hangs (art director 04:40Z): a fire trail lives 30 s and a straggler's smoke 20 s. To fit the room, past
  `thin_after_s` only one puff in `thin_keep_one_in` (6) hangs on, growing on to `thin_grow` times, so an old trail is
  fewer, bigger lumps: about 250 puffs for a fighter's fire trail at 150 m/s, 170 for its smoke. Each kept puff
  thickens to `thin_alpha` times its opacity by `thin_after_s`, holds full to `thin_hold` of its life and is drawn out
  over the gap to the next, so the old trail stays one dark line (`thin_ragged` makes the gaps and sizes uneven, a
  torn ribbon, never a string of beads) (04:59Z: a fire trail 110 or darker for two thirds of
  its length on a 212 sky, its oldest part no paler than 150; a straggler's head about 130). A hit's puff hangs 3 to 4 s.
- A trail is one strip (`ribbon` 1; art director 14:13Z, where the game's falling planes trailed beads on a string).
  Each piece runs from its own point back to the piece laid before it, wherever that has drifted to, and is as wide
  and as dark at that edge as that piece is: the two share the edge, with no overlap to darken a join and no gap, at
  any speed. The strip leaves each edge along the way the aircraft flew there, so it bends through a turn and a spin's
  trail is a smooth corkscrew; each piece takes most of the drift and rise of the one before, so it meanders and
  never zig-zags (`drift`: how much of the scatter it keeps; `core`: the share of its half width that is dense). One
  piece in `thin_keep_one_in` (6), and one wherever the path has turned `keep_each_turn_deg`, is also a long length
  that hangs the whole of `life_s`, coming into sight between `join_from` and `join_by` of `thin_after_s` as the
  short pieces go. From `lumps_from` of its life a length lets go of its neighbours, paling and narrowing to nothing
  at each end, and draws in to a soft lump: only the old end tears into lumps. It takes no more room: at 120 m/s a
  smoke trail 124 places and a fire trail 225. Recipes without `ribbon` draw as before.
- A ribbon ages (art director 15:22Z, where the falls read as pen strokes): `widen_by_s` is the time by which a piece
  has widened from `r0_m` to `r1_m`, by the power `grow` (over 1: thin at first, then spreading), and the long lengths
  widen with it and on to `thin_grow` times; it stays as black as laid until `pale_from_s` and pales to `pale_to` of
  its opacity by `pale_by_s`; `wide_wander` makes its width wander along its length in soft swells. Without those
  keys a ribbon draws as it did. Read a fall frame with care: 14 s into a burning fall the preview's aircraft is under
  its ground plane, so the frame holds only smoke about 2.5 to 7.5 s old; at 8 s (frame 240) the plane is in sight.
- When the room (`game.max_puffs`) is short, every light (flames, flashes, tracers) is counted first, as an oil fire's
  kept column is, and the farthest smoke goes: a dozen trailing aircraft fill about 2,900 of 3,000 slots, and past that
  the far trails shorten while no fire is lost (24 aircraft, half burning, seen from 1,200 m).
- A trail not moved for 30 s is forgotten; call `TrailStart` again to bring it back.
- Kinds live in the recipe's `air_trails`: `smoke`, `fire`, `glycol`. What a hit looks like lives in `air_hit`.
- The same clock rule applies as for `AirGun`.

## Aircraft going down

```cpp
// an aircraft lost (the battle's air:lost): once as it falls, then every frame, then when the model is gone
Blasts->AirFall(AirNow, PlaneId, PlaneWorld, PlaneVelocity, TEXT("burn"), /*SpanMetres*/ 11.0f, /*DeckZ*/ CloudTopZ);
Blasts->AirFallMove(AirNow, PlaneId, PlaneWorld, PlaneVelocity);
Blasts->AirFallStop(PlaneId);
```

- The debris row (art director 04:40Z). `explode`: blown apart, every piece at once, 30 to 50 for a fighter and 60 to
  100 for a bomber (a span of `bomber_span_m`, 20 m, or more), round a short torn fireball, never a ball: 18 to 28
  clumps of fire thrown out along two to four tears, each a hot middle darkening to a deep red edge, gone in about half
  a second, each leaving black smoke that hangs where it died while the pieces fly on. It is lit from within (art
  director 06:00Z): a white-yellow heart for about 0.1 s, broken in three along the tears and drawn over the clumps
  (`Bit::Nearer`); a saturated orange
  body, blue kept low and near opaque to its edge, cooling to deep red by `cool_by_s` and into its smoke, which rides
  with it as it dies, so it never goes peach, clay or salmon over the sky. The battle's colour grade must spare fire,
  flashes and tracers from its desaturation. Hide the model at once; no
  `AirFallMove` is needed. `burn`: going down on fire, 5 to 10 pieces shed over the fall. `spin`: out of control, two
  to five, the first sometimes a wing (`wing_odds`). Keep the fire or smoke trail on the model with the Trail calls.
- Most pieces are light torn skin plates, painted outside and bare metal inside, that tumble, flutter and glint in the
  sun as they sink slowly and drift with the wind; a few (`heavy_share`) are heavy dark pieces that drop straight.
- `DeckZ` is the world height where the pieces leave sight: the cloud deck's top on a day with one, otherwise the
  ground under the fight. Each piece fades into it over its last `enter_deck_m`, at most `most_s` after it came loose.
  Far off a piece shrinks to a speck and fades, never held up: nothing pops out of view.
- A fall not moved for 30 s, or that has shed all its pieces, is forgotten. Everything lives in the recipe's
  `air_debris` (and `air_debris.fireball`).
- The preview: `FX_FALL=explode FX_DIST=90 ./fxpreview recipe.txt out air hmg 16,19,22,30,60 960x540 fall` (the fall
  starts at frame 15; `burn` and `spin` too; FX_SPAN its span, FX_DECK metres down to the deck, 400).

## Flak at height

```cpp
// a heavy flak shell bursting near the bombers (the battle picks where and how often)
Blasts->AirFlak(AirNow, BurstWorld, TEXT("heavy"));
```

- Smoke, never a ball: a soft flash, then five to eight see-through lobes in a crooked chain down the wind, with wisps.
  It hangs where it burst, drifts with the wind, greys and stretches, and is gone in about 9 s. It is never cut off.
- The art director's brief sets where and how often: within about two wingspans of the formation, more behind than
  ahead; a new burst about every 2.5 s (light guns), 1.4 s (medium) or 0.9 s (heavy), ragged; none once they turn
  for home. Each burst is about 12 to 19 puffs; seen close, its smoke puffs take two slots each (16 to 26 slots).
- The preview: `./fxpreview recipe.txt out air hmg 6,20,95,245 960x540 burst` (one burst, close) or `... 360 960x540
  flak` (heavy guns round a flying aircraft).
  `... 6,12,35,95 960x540 above` is one burst 60 m over the fields as the campaign camera sees it: FX_DIST metres off (75),
  looking down FX_PITCH degrees (50), facing FX_YAW degrees (90); FX_SUN=x,y,z sets the sun for every air shot.
- Seen with the sun behind the eye (as from the campaign camera on a sunny day), a flak burst shows its sunlit face:
  `face_lit` takes its outer lobes and wisps toward `lit_colour`, most on the sunward side, while its young heart and
  far side stay dark. Facing the sun or at night it keeps the dark look; with the sun to the side it is partly lit. A flak
  puff at least `twin_px` across on the screen is drawn as two fans, the second turned half a step, so it takes two slots
  and its outline shows no facets. When the room is short the farthest go, counted by slots.

`-TheaterBlastTest` on the command line plays a 24-second barrage of every size and kind round the point given to
RunTest, again and again, for test pictures.

`-TheaterFxLampLog` writes a log line for each frame a lamp is lit (where it hangs, in world units; how strong, in
candelas; how far it reaches; its colour), and one for the first frame after with none. A 105 mm shell's lamp shows
once at full strength, once at half, then out.
