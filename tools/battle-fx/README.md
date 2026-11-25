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

// the same burst striking a tank's plate at Target, Normal its outward way: it bounces off her (release 2, see
// "A burst off armour: ricochets"); never a dust spurt
Blasts->ShotsOnArmour(Gun, Muzzle, Target, Normal, TEXT("mg"), 25, 15.0f);

// a tank or towed gun firing on the desert (Ground: what its blast throws up, earth if left out)
Blasts->Muzzle(MuzzleSocket, Facing, TEXT("tank_gun"), TEXT("sand"));

// every frame, each moving vehicle's two dust sockets (dust.left, dust.right); Id the vehicle's number * 2 (+1 right)
Blasts->VehicleDust(Vehicle * 2, LeftSocket, Velocity, TEXT("sand"), /*bTracked*/ true);
Blasts->VehicleDust(Vehicle * 2 + 1, RightSocket, Velocity, TEXT("sand"), true);

// an air scene's stick of bombs: 11 bombs of 500 lb landing along the ground from First to Last (see Bomb carpets far off)
Blasts->BombStick(AirNow, StickId, FirstWorld, LastWorld, 11, TEXT("bomb500"), TEXT("earth"));

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
  columns (oil fires) are left alone (an aircraft's trail has its own, `game.trail_near_eye_fade_m`, below). At the battle camera a burst's own puffs stay farther off than that (a 105's
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
- **The bank's fringe.** The bank's lumps let the sun through by how high each stands over the fire's own
  height, a deck fire's too. Head d7c5c30 counted from the sea under a deck fire; the pre-check then measured that
  deck fire's bank glowing about a seventh more against a low sun than the same fire at the waterline, where
  before the two glowed alike. So it is back as build 1758 had it, the look the art director passed.

Behind its recipe key, with its own dice: head 335b669's recipe draws as before on this code, frame for frame,
the oil scenes among them. The kit cannot tell a ship's fire from one on land: a big, long fire ashore (the
atoll's tanks and hangars) sends up a column too.

`FX_BURN_ROW=n,m` lays n plain fires m metres apart, centred where `FX_BURN_ASIDE` puts the fire, turned
`FX_BURN_TURN` degrees. Near build 1750's Akagi 06: `FX_LENS=42 FX_BURN=33 FX_BURN_ROW=3,78 FX_BURN_TURN=15
FX_BURN_UP=9 FX_DIST=830 FX_EYE_H=200 FX_LOOK_H=60 ./fxpreview recipe.txt out oil sunk 3600 1072x670`; add
`FX_BURN_ASIDE=0.01 FX_REACH=156 FX_SINCE=90` for her oil fire. The lone 40 m fire in the game's air view:
`FX_BURN=40 FX_DIST=520 FX_EYE_H=330 FX_LOOK_H=60`.

## Gun dust on dry ground as a few dense lumps, a softer flash with a yellow root (after head d7c5c30)

The art director's verdict of 06:08Z, 20 November, on build 1758's game stills.

- **A gun's dust on dry ground.** `muzzle.dry.tall` and the keys after it. In build 1758's west still on sand the
  dry lumps and the gun's smoke wisps were still one soft cloud half a second after the shot, and two riflemen
  4 m off the line of fire, seen across it, almost vanished (their box's span 137 of 170). The blast now raises
  four to six big dense lumps and nothing thin. `tall` of them stand where the blast strikes the ground hardest,
  at the muzzle's two flanks and just ahead of it, never behind it: each `tall_r` of the ring across with two
  shoulders, about man-high for a tank gun. `low` of them are heaps of two lobes farther down the line of fire,
  one to a stretch with clear ground between, `low_r` of the ring across. The gun's own smoke is `puffs` dense
  puffs at the muzzle in place of the wisps along the bore. Every piece is `alpha` thick out to `inner` of its
  radius and lit as a thing with form (`light`, the strength of `Bit::ShadeK`): on a plain shown as 176,159,117
  its lit top lands 1.08 to 1.10 times the ground and its shadow side about two thirds (0.66 at the fifth
  percentile). The heaps are gone by `clear_s`; the tall ones hold to `near_s`, thinning. Build 1758's lumps
  (`lumps` and its keys) are still drawn by a recipe that has them and no `tall`.
- **Why the men vanished.** From the test's west eye the tank's line of fire lies between the eye and the two
  men, and the eye's line to them crosses it 3 to 4 m ahead of the muzzle, 1.5 to 2.7 m up: just where the old
  lumps stood tallest and the smoke wisps flew. Now only low heaps lie there. In the preview the men's box keeps
  its whole span at 20.5 s and 21.5 s over ten throws of the dice, and under 2% of the men's own pixels change.
- **The flash's edge in pixels.** `burst.flash.feather_px`: from 18 m `feather_m` is about one pixel and the
  tongues read as cut paper. Every feathered edge now fades over at least that many pixels on the screen whatever
  the distance (`Bit::FeatherPx`: the heart ring is brought in at draw time).
- **The tongue's yellow root.** `burst.flash.root_reach`, `root_yellow`: in the game the tongues read as one flat
  orange (median green over red 0.50). Each tongue's root is now yellow-orange (lands about 243,202,63, green
  over red 0.8) from the body out to `root_reach` of what shows of the tongue, then turns orange and reddens to
  its tip. In the preview the flames' green over red runs 0.37 to 0.84.
- **The burst on sand, measured and left.** The burst-black share at 0.4 s on sand (of the pixels the burst
  changes, those under luma 60 in the game's picture): 15.6 to 35.0% from 18 m and 12.9 to 20.4% from 60 m over
  four rounds, means 24 and 18%. Over the art director's 15% on the whole, so the sand throw is as it was.

Each is behind its recipe key and throws its own dice (the flash throws none new): head d7c5c30's recipe draws
as before on this code but for the bank's fringe above, where it draws as 5203b6c's. Preview: the test's west
eye as build 1758's still has it is `FX_MG_VIEW=game FX_GAME_FROM=208 FX_GAME_BACK=13.1 FX_GAME_LOOK=3.6,0`
with `FX_CREW=1 FX_TEST_TANK=0.5 FX_LENS=42`, scene `mg sand`, frames 0 and 30 (the game's 20.5 and 21.5 s: the
preview runs 30 frames a second). With build 1758's recipe it gives the men's box 137 and 149 of 170 where the
game gave 137 and 147. `FX_GAME_LOOK=toward,right` moves the point the eye looks at; `FX_TANK_NORTH`,
`FX_TANK_EAST` move the tank's muzzle, which throws its dice anew.

## A flash whose yellow melts into its orange, gun dust born low (after head 3d00b85)

The art director's verdict of 08:03Z, 20 November, on build 1762's game stills. The gun dust passed (the men's
box 169 of 170 against the frame before the shot) and so did the flash from 60 m. One small round followed.

- **The tongue's root, melted.** `burst.flash.root_melt`, `root_from`. From 18 m the root of `root_reach` showed
  as a flat yellow petal with a firm line round it inside each orange tongue, and the flash read as a flower:
  the root was an oval of its own, seven tenths as wide as the tongue, and its edge faded over a pixel. Now a
  tongue's fan is drawn from the point where it leaves the body (`Bit::Hub`, `root_from` of what the body hides
  of the tongue): orange there, running evenly out to the red-orange of its edge. Its root is the tongue's own
  shape laid over it, outline for outline, yellow and whole at that same point and thinning evenly to nothing at
  the tongue's rim, half gone `root_melt` of the way out and turning orange as it thins. So nothing of the root
  can show outside the tongue, the yellow melts into the orange along the tongue and across it with no line, the
  hottest yellow runs down the centre line, and the edges are orange to red-orange. Heart, body, sizes, timing
  and dice are as they were. In the preview the flames' median green over red is 0.57 to 0.61.
- **Dust born low.** `muzzle.dry.low_s`, `low_flat`, `low_soft` (`Bit::LowBy` and its fields). From 60 m, a fifth
  of a second after the shot, the young lumps read as small pale balls. Now, until `low_s.0` seconds old, each
  lump and heap lies along the ground `low_flat` times as high as it is wide (as wide again as keeps its bulk),
  drawn as its outline from where the eye is, in its plain middle tone with no lit top and no shadow side, its
  dense part reaching only `low_soft` of the way out it will: a low soft skirt, as blast dust first is. By
  `low_s.1` it has risen round, firmed and earned its light, so half a second after the shot it is the lump
  the art director passed in build 1762. The gun's smoke puffs, off the ground, keep their round shape and only
  firm and come into their light as late.
- **Each lump of its own shade.** `muzzle.dry.shade` now runs six parts in a hundred either side of its middle
  (0.77 to 0.87; its top as before, so the lit tops stay at 1.10 times the ground or under).
- **The old lumps' block is skipped under `tall`** (the pre-check, 07:31Z): with `muzzle.dry.tall` none of build
  1758's lumps is laid, so none of their numbers is read and none of their dice thrown. The pictures are the
  same and a heavy shot on sand costs 4 to 5 millionths of a second less to make.

Each is behind its recipe key and throws no new dice: head 3d00b85's recipe draws the same on this code, frame
for frame. Preview: the game's wider view of the west test is `FX_GAME_BACK=52 FX_GAME_LOOK=0,0`; with
`FX_TEST_TANK=0.1` frames 0, 3, 6, 9 and 12 are 0.1 to 0.5 s after the shot.

## Young dust that shows on the game's sand, fans that cannot fold (after head 61387c7)

Build 1764's game stills (09:28Z, 20 November) showed two things the kit's own stills had not.

- **The young dust was lost on the game's ground.** From 60 m, for a third of a second after the shot, nothing
  marked the gun: the low skirt of head 61387c7, soft-edged and in one flat tone a little under the ground's,
  showed as a dark stain on the preview's flat ground and as almost nothing on the game's rippled sand.
  `muzzle.dry.low_lit` (`Bit::LowLit`) is the share of its light and shade a mound is born with; `low_soft` 1
  gives it its firm edge from birth. At `low_lit` 0.5 a young mound's top sits at about the ground's own tone
  (the top 2% of it 1.03 to 1.04 times the game's sand, where a lit lump of build 1762 was 1.09) and its foot is
  darker, so it reads as a low heap of dust and never as a pale ball. From `low_s.1` on it is as it was: the
  frames at 20.5 and 21.5 s of the west test are head 61387c7's, byte for byte. No `low_lit`: 0, as before.
- **A fan drawn from a point off its middle folded over itself** (`Bit::Hub`; the pre-check's reading, 09:08Z):
  where a tongue's rough outline turned its back on the hub, that sector was wound the other way and drawn over
  its neighbour, a wedge of a few pixels inked twice, in 360 of 7,000 fans. `Battle::Lay` now brings each rim
  point that would lie behind the last one in sight onto that one's line from the hub, as far out as it was, and
  its heart point with it, so the sector is a line and covers nothing. By the pre-check's counter (hub57): 0 of
  7,000. A fan whose outline is wholly in sight of its hub is left as it was, to the bit. A fan more than half
  of whose rim would be moved is put back as it was laid (the pre-check's guard, 10:45Z): that is a hub outside
  its whole outline, which no hub the recipe can make is (a tongue's is held to -0.5 to 0.6), and without the
  guard such a tongue would be drawn as a line, which is to say not at all.

**Judge a ground effect on the game's own ground.** `over_game.py` lays the kit's picture over one of the game's
stills: the same frame drawn on two grounds gives, pixel by pixel, how much the effect covers and the light it
adds; that is laid in linear light over the still (taken back through the game's curve) and shown through the
curve again. Laid so, head 3d00b85's young lumps and head 61387c7's skirt come out as builds 1762 and 1764
showed them. The wider view's eye is not quite the builder's (the dust lies 19 pixels left and 158 up of where
the preview has it; the last argument moves it), and by eye the game's shot is about a tenth of a second later
than the still's name says.

## A big column's puffs carry lumps of their own (after head d96d6be)

Build 1766's Midway ships picture (art director, 10:57Z 20 November): Akagi's column, a kilometre off, was the
right shape and colour and read as a soft brown smear.

- **It was the kit's own picture, not Unreal's upscaler.** The kit's picture of the same framing, drawn at the
  1072 pixels the air scene draws and filled out 2.7 times with plain scaling, shows the same smear. A column
  puff there is 60 to 90 m across, 300 to 400 of the window's pixels, and one fan whose edge fades over two
  fifths of its radius, lit as part of one round column: at that size no lump has an outline. Drawing the scene
  bigger did not help, as the fade is a share of the puff and not of the pixel. (Epic's own page on the
  upscaler says it does not use a material's responsive flag for translucency, so there was no setting to try.)
- **Lumps on a lump** (`Bit::Lumps`, the rows' `lumps` keys on `oil_fires.sunk`, `oil_fires.burning` and
  `fire.smoke_column`). A column puff at least `lumps_px` pixels across on the screen carries `lumps` smaller
  fans on its face. Their sizes are dealt from `lumps_r` (shares of the puff's radius), one step of it to each
  lump, so every puff has a big one, small ones and the sizes between; each is set `lumps_out` of the way from
  the puff's middle to where it would touch the puff's edge, so none stands free of it. Each takes `lumps_light`
  of its light from its own sunward side, under the same sun as the column, and keeps `lumps_rim` of it at its
  rim, so the dark between two lumps lies on their shadow sides; `lumps_inner` is its own heart ring, a firmer
  edge than the puff's. Place, size and shade come from the puff's own seed: no dice are thrown, so nothing else
  is laid differently, and no two puffs carry the same pattern.
- **Nothing pops** (the art director's guard, 11:29Z). The lumps come in between `lumps_px` and a quarter as
  wide again, and melt into the puff over the last quarter of `hold` of its life, before it begins to thin.
  Measured by telling the kit a pixel 4% smaller at each of 30 steps through the mark, the picture otherwise
  unchanged: the most any pixel's share of the lumps moved in one step was 16 levels of the 72 they come to.
- **What they cost, and to whom.** Each takes a place in `game.max_puffs` only while its puff is that big and
  that young: at `lumps_px` 260 the carrier of the Midway picture takes about 220 places more, a ship at the
  harbour picture's distance about 15, and her picture is next to unchanged. The pall aloft and the black root
  carry none. And they take only places nothing else wants (`Battle::LumpsRoom`, set in `Draw`): they are not in
  the room asked (`RoomAsked`), so no gun smoke is shed for them, nor in the count that drops the farthest
  pieces. They are whole while a tenth of the room would still be free with them all laid, thinner as that
  goes, and not laid once it is full. The Midway framing with the room cut down: whole at 1,100 places, about
  half at 1,000, none at 950 or 900, where the picture is the one the old recipe draws, to the bit.
- **The room is judged by the most asked of late** (the pre-check's finding and cure, 12:56Z; `LumpsMost`,
  `LumpsHeld`, `LumpsShown` in `Draw`). Judged by one frame's count, every lump on the screen flickered once
  the room was in its last tenth, as a fire's flames come and go by ten or twenty places a frame: two ships
  800 m off stepped the lumps' ink by over a fiftieth in a frame on 230 to 761 of 2,100 frames, and a salvo
  took it all in one. Now a new most is taken at once, stands while the count stays within a hundredth of the
  room of it, and left untouched for two seconds comes down a hundredth of the room a second; what is shown
  falls by no more than a half a second. On the same scenes: 0 or 1 such step a run. The limits: the frame in
  which a salvo fills the room still takes every lump in that frame (there are no places; a guard that lays
  what there is room for was tried and is a known follow-up if it is ever seen), and after a crowd the lumps
  take up to 12 s to come back whole.
- **A cut clears that memory** (the pre-check's second diff, 13:07Z; `LumpsZoom`). Cut from a harbour view with
  the room in its last tenth to a close-up, the columns stood bare for over 2 s and were whole only 10 s
  later; the Midway ships picture is itself reached by a cut. A cut is the eye over 50 m from where it was a
  frame ago, turned by over 0.6, or its view's pixels a radian changed by over a quarter: on that frame and the
  next the room is that frame's count alone. With a still or flying eye nothing is drawn differently.
- **`game.max_puffs` is held** to 100 to 20,000 in `Battle::Setup`, and a value that is not a number gives
  3,000: turned into a whole number unchecked, a broken recipe's nan was a fault and its 1e9 asked for hundreds
  of gigabytes (the pre-check's fault finder, 12:57Z).
- **Old recipes.** With no `lumps` key a row draws as it did: head d96d6be's recipe gives the same 65 frames,
  byte for byte, on its own code and on this.
- **Their cost held** (after head 525ceec; `game.lumps_most`, `lumps_px_most`, `lumps_least_ink`). The builder's
  rested reading of build 1770 had Pearl Harbor at 55.8 frames a second against 72.7 on 1766 and Midway at 59.0
  against 67.8 once the ships burned, with the kit's own time up by half a millisecond: every lump is a fan to
  work out, send and paint. Two burning ships 800 m off laid 87 puffs with lumps, 436 fans more, and the kit's
  draw took 2.2 ms against 1.4 without them. Three limits, each from the art director's 14:50Z call:
  - `lumps_most` (48): no more puffs than this carry lumps at once. With more wide enough, the width a puff
    must have rises to that of the first one left out (`Battle::LumpsTight` times its own `lumps_px`, found in
    `Draw`), and each puff's lumps come in over the next quarter of that, as before. It follows the count by
    half its size a second at most, and a cut clears it as it clears the room's memory. The same two ships:
    46 puffs, 230 fans more, 1.7 ms.
  - `lumps_px_most` (1200 to 1800 window pixels): a puff wider than the first number counts for less the wider
    it is (`LumpsWorth`) and has lost its lumps a little short of the second. A column the eye is in or under
    fills the screen with such pieces; their lumps are murk and the dearest of all to paint. From 30 m off the
    same ships: none laid, against 436 fans.
  - `lumps_least_ink` (0.02): lumps thinner than this are not laid.
  - `lumps_soft_s` (0.35, after head 7d83869): the pre-check followed every lump frame to frame on 7d83869 and
    found none popping, but at the cap all the lumps in the fade band stepped TOGETHER about once a second: 6 to
    13% of all their ink in one frame with the eye still, 31 of 360 frames over 5% and two blinks on a slow
    approach. The width asked drops a little each time a wide puff ends its lumps' time; a drop within the
    clamp's reach passed whole in one frame and every puff in the band moved by four times it. With the key
    `LumpsTight` goes toward what is asked by a share of the way left each frame, most of it closed in that many
    seconds and the same at any frame rate, still by half its size a second at most; a cut sets it at once. Six
    ships at 800 m: the biggest frame change of all the lumps' ink 2.1% from 12.3%, a followed lump steps by 3 of
    255 from 18; the approach, no frame over 5% and no blink. The price: on an approach the count runs over the
    cap for a while, 285 lumps at most against 240. 0 or no key: as 7d83869 (8 of 8 scenes the same in every
    number and the picture's hash). Under the cap nothing changes.
  - `lumps_band` (not in the recipe; 0.25 without it): the share of the width asked over which a puff's lumps
    come in. With many puffs alike in size most of the 48 kept lie inside that quarter and show at under half
    strength (the pre-check's note). 0.15 gives them a third more ink at the cap for the same count, but it
    also quickens every young puff's lumps under the cap, so the Midway ships framing would no longer be the
    picture the art director passed. Left as a dial for a day the cap's look is judged in the game.
  - **What is the same.** The Midway ships framing below has 40 to 44 puffs with lumps and is the same picture
    byte for byte with and without the three keys, so its cost there is as it was. At `lumps_most` 20 the lower
    half of that column goes smooth again. Without the keys every lump is laid as in build 1770, and head
    525ceec's recipe gives the same 65 frames on its own code and on this.

**Judge a big effect at the window's own pixels.** The air scene tells the kit the WINDOW's width in pixels
(2881 on Brian's screen) while it draws about 1072 of them and Unreal fills the rest out; the campaign map tells
it the width it draws. So `twin_px`, `lumps_px` and every other pixel rule are window pixels in the air scene.
The preview's `FX_KIT_PX=2.6875` tells the kit the same (the oil scene), and the picture then has to be looked
at filled out 2.7 times, not at the size it is drawn. The Midway ships framing: `FX_FLOAT=1 FX_BURN=33
FX_BURN_ROW=3,78 FX_BURN_TURN=15 FX_BURN_UP=9 FX_BURN_ASIDE=0 FX_REACH=156 FX_DIST=620 FX_EYE_H=128
FX_LOOK_H=75 FX_KIT_PX=2.6875 ... oil sunk 3600 1072x670`.

## Hits that play out in many ways: steam, cook-off, a fire put out, a ricochet (after head 031d6c8)

Brian, 20 November: hits on ships and tanks should play out in many ways, as the aircraft's do. The outcomes
thread picks what happens (`Battle/Fates`); these are the pictures. Each look does nothing without its recipe row
and throws dice of its own, so the last recipe draws as before (the same 65 frames, byte for byte). The art
director's limits are in `art-director/explosions/outcome-looks-brief.md`; passed 16:41Z after one mend round.
Gouges and scorch that stay are the MODEL's to show.

- **`Steam(Id, WorldAt, Way, AcrossMetres, Seconds, Kind)`, `SteamStop(Id)`** (row `steam`, kinds `vent` and
  `funnel`). White that rises fast and vanishes, beside black that stays. A narrow cone, clear for `gap_m` at
  the mouth, then rags that evaporate within `life_s`; it roars for `roar` of its time, then its length and speed
  fall to a wavering wisp. Each rag has a cool shadow side (`shadow`, `shadow_k`), and the side of the plume away
  from the sun is greyer (`side_shade`). The funnel plume's top third broadens and leans off down the wind
  (`cap`, `cap_wide`, `cap_from`, `cap_life_s`): bigger, slower rags born where the climb is spent.
- **`CookOff(WorldAt, Up, AcrossMetres, Pops, OverSeconds, JetSeconds, JetFromSeconds)`** (row `cook_off`). Pops:
  a small flash first, a grey-black puff, embers thrown unevenly (`ember_uneven`), now and then a tumbling
  fragment; never a glowing round going skyward. The jet, 3 to 6 m from a tank's hatch (`jet_tall_m`, by the root
  of the hatch's width): a near-white yellow root (`jet_root`) as its brightest point, tongues torn off its upper
  half (`jet_torn`), a pulse that only takes from its height (`jet_pulse`), a lean with the wind (`jet_lean`),
  sparks up in it (`jet_sparks`), black smoke off its sides and top by the clock (`jet_smoke_every_s`) so the
  column above is one torn piece, and an orange lamp on hull and ground (`jet_lamp`, `jet_lamp_day`).
- **`Burn(WorldAt, AcrossMetres, Seconds, Id, bOnShip)`.** `bOnShip` true: a fire on a ship's deck (anything
  afloat). A small, low, still fire on land takes the burning tank's look (`fire.land_tongues`, `fire.land_column`,
  the land lamp) and joins a fire beside it (`fire.join`); a scene's ground under a ship is her deck, so a deck
  fire would too. With the flag it never does, and burns as a ship's fire (the pre-check of 22 November: a ship's
  fire 2.5 m across at rest drew 426 pieces with a column and a lamp four times as strong; with the flag 100, as
  at 07d21fff). False (the default): as before. The sea scene and the air stage pass it for every fire on a ship.
- **`Burn(WorldAt, AcrossMetres, Seconds, Id)`, `BurnOut(Id, OverSeconds, How)`** (row `fire.put_out`). The
  flames are eaten back from the edges to the seat, shrinking steadily: half their height by two fifths of the
  time, fewer tongues as it goes, never all gone and back. `How` "water": a surge of the vent's own steam rags
  lifting off the seat for `surge_s` (row `steam.surge`). "extinguisher": a brief pale puff. The smoke pales to
  grey-white by `pale_s` and is gone by `gone_s`. Where it burned a dull patch (`glow`, `glow_alpha`) lies on the
  seat and small specks flicker over it (`embers` alight at once at first, fewer to the end) for `embers_s`
  after the last flame. Hose streams are left for a later call that knows where the nozzle is.
- **`Glance(WorldAt, Incoming, Normal, CalibreMm, Ground)`** (row `glance`; the calibre is a NUMBER in mm). A
  two-frame flash, sparks uneven in length (`spark_uneven`), a tiny puff, and the shot going off: held to the
  1/30 s streak rule (`streak_s`), a glowing head, a hot neck and a tail cooling to red, slowing, dropping and
  kinking as it tumbles (`wobble_m`, `tumble_hz`). Each ricochet's dice are its own (place, moment, the kit's
  seed, how many came before). No more than `most_shots` (12) are followed at once.
- **Why the first cut's shot was a laser line.** `Unseen()` keeps a light shorter than a frame that no frame has
  laid and lays it whole on the next: right for one flash, but a chain of short lights laid every step since the
  last frame at once. A chain's pieces are born with `bShown` set.
- **A small fire's column in one piece** (`fire.smoke_column.tight_m`, `overlap`, `least_px`, `least_keep`). The
  art director: at 300 m a ship fire's smoke broke into separate flakes. A puff's size goes by the fire's width
  but the scatter of the row's windage and push does not, so over a 12 m fire the puffs were a fifth as wide as
  over a 60 m one and thrown as far apart. A fire narrower than `tight_m` (50) keeps only its share of that
  scatter, each puff is at least `overlap` (1.5) times as wide as the way to the next, far off it is never
  under `least_px`, and its pall thins away. At 50 m or wider nothing changes: three fires along Akagi make a
  column 55 m wide, laid as passed. Of the 65 proof frames only lone fires of 9, 12 and 40 m differ.
- **Wild numbers.** Every number of the four rows and the column's four keys (192) set to nan, inf, -inf, 1e9,
  -1e9, 0 and 1e-6, alone and all together, and the calls' own arguments the same, under ASAN and UBSAN: 1,876
  runs, no report. It found one fault on the way: a direction of (inf, 0, 0) was checked for a number only and
  came out as not a number in every part (`WayOf` now).
- **Preview.** `fxpreview <recipe> <dir> fate steam|out|cook|glance <frames> WxH`; the dials are listed at the
  top of `fxpreview.cpp`. Stills: `battle-fx/stills/outcome2-*.png`.

## Tracers by nation, hits that sparkle, a dead engine that always smokes (after head de54166)

Brian, 20 November: "more real istic chaser fire", "if a engine is out there should be a lot more dark smoke
strains from that engine and directly behind it on the tail", and many ways for an aircraft to go down. The art
director's rulings are in `art-director/battles/air-fates-look.md` (D tracers, C hits survived, G1 a dead engine,
G2 colours). This head is the tracers, the hits and the engine smoke; the ways down (the ragged explosion, the
flamer `fuel`, the wing-root `mist`, the splash and ditching, the belly landing) are the next. Everything here is
behind a recipe row or key and throws no dice an old recipe's path throws: the last head's recipe draws the same
115 frames, byte for byte, on this code.

- **Tracers** (row `air_guns.tracers`; without it the ruled yellow streak, as before). Alight `life_s` (1.2), a
  streak `streak_s` of its flight long that shortens as the round slows, a hot core in a soft glow three cores
  wide (`body_px`, `core_px`), full until `dim_from` of its life, dimming to `dim_to`, out over `out_s`. The core
  is white-hot within `white_within_m` of the eye and its nation's colour from `white_beyond_m`. A cannon (a row
  with `cannon` 1) is `tracer_wide` times as wide and `tracer_life_k` as long alight.
- **Whose guns.** `AirGun`'s ninth argument, `Nation`: `us`, `gb`, `de`, `it`, `jp`, `su`; empty or unknown takes
  `default`. Each of `tracers.nations` has `body` and `core`, written for the game's picture (`game_curve.py
  write`) with how far past white each burns (`body_over`, `core_over`), and may carry a `cannon` of its own: the
  Soviet small arms are green and the Soviet cannon red as the Allies'. Ground guns keep the ruled yellow.
- **The pixels the air scene tells the kit are its WINDOW's.** The air scene draws 1072 across and tells the kit
  2881, so `least_px` and the day widths in the air rows are window pixels: 2.69 of them to a drawn pixel there,
  1.80 on a 1600 still. The ruled 3.4 came out at 1.3 drawn pixels, a hairline the Pearl Harbor film lost; 9.6 and
  3.2 are 5.3 and 1.8 px on a 1600 still. Previews render at twice the size and are boxed down (`FX_KIT_PX`).
- **How many.** No more than `most` (1,500) alight; nearing it the guns farthest from the eye go without first,
  those within `always_within_m` never. A gun farther than `one_fan_beyond_m` (1,200) fires tracers of one fan,
  a solid dash in the core's colour (`one_fan_px`). A tracer is two fans alight 1.2 s (it was 2.5 s): a firing
  .50 holds about 6 fans where it held 13.
- **Hits survived** (rows `air_hit.sparkle`, `air_hit.burst`; without them a hit is as it was). A rifle-calibre or
  .50 round: `points` bright sparkles of one or two frames on the skin and nothing more. A 20 mm shell: the small
  flash, `sparks`, `fragments` scraps of skin and one black-grey puff `puff_m` across left hanging `puff_s`.
- **A dead engine always smokes** (rows of `air_trails`; the caller names the kind in `TrailStart` at the dead
  engine's socket). `fire`: the densest black, 1.5 m at the nacelle and 5 to 6 m past the tail. `oil`, out and
  windmilling: dark grey, lumpy, one torn column, a gout `gout_wide` times as wide and `gout_alpha` as dense for
  `gout_s` every `gout_every_s`. `streak`, the fire out and the airscrew feathered: thin, dark, never clearing.
  `doused`: the extinguisher's white-grey puff for `for_s`, the tongues shrinking back to the nacelle over
  `flame_shrink_s`, then the streak by itself (`then_streak`). `glycol`: white, thinning to `fade_to` of itself
  over `fade_over_s`, then the streak. `tail_wide_m` is how wide the smoke is `tail_m` behind its source: the
  aircraft's own wake spreads it that fast. A strip thins to nothing at its sides, so the key is set wider than
  the ruled width, which is where the smoke is half as dark.
- **A change of kind is `TrailStart` again with the same number, never `TrailStop`.** A kind with `blend_s` comes
  out of the kind before it over that long in colour, density, width, shadow and how it pales, and the old kind's
  hanging lengths close with one last bar that thins to nothing: black, greying, the white puff, grey, the thin
  dark streak, with no cut anywhere. An unknown kind trails as `smoke`.
- **Soft strips** (`soft` 1). A ribbon's link had one colour and was lit by its place round its ring, so pale smoke
  showed every link as a panel, bright at one end and dark at the other. A soft strip's colour runs along each
  link to the colour of the piece behind, it is lit across only (top and underside, the same all along), and its
  density swells with its width's slow wander, never by each piece's dice. `shadow` and `shadow_k` give pale smoke
  its underside. `wisps`: the share of pieces that throw a soft shred off the column's edge.
- **A soft strip's pieces are as dark as they will be from the moment they are laid.** A ribbon's piece fades in
  over 10 ms, and its near end was pale for a frame under the dark end of the trail's young end: a notch behind
  the engine each time a piece was laid. A piece laid at the very moment of a frame was left out whole, and the
  trail began a piece's length (8 m) behind its engine for that frame. Old kinds without `soft` are as they were.
- **A ricochet's cost is its call** (`glance.most_a_frame`, 3; the pre-check on de54166). No more than that many
  shots begin to be followed at one moment; one past it keeps its flash, sparks and puff. Time going back by more
  than half a second forgets the shots still counted.
- **Guards.** `TrailStart` and `TrailMove` do nothing for a place, a speed or a time that is not a real number
  (`RealPlace`), and every number of a trail row is held between wide bounds as it is read. Wild numbers: 263 keys
  and the calls' arguments set to nan, inf, -inf, 1e9, -1e9, 0 and 1e-6 under ASAN and UBSAN, 3,871 runs. The
  first run found 210 faults, every one in a trail row's OLDER keys or a trail call's place or speed: a new row
  wants its old keys tried too.
- **Preview.** `fxpreview <recipe> <dir> air <gun row> <frames> WxH <shot>`; shots `far`, `chase`, `close`, `hit`,
  and for an engine's trail `beam` and `quarter` with `FX_TRAIL` (and `FX_TRAIL2`, `FX_TRAIL2_AT` for a change of
  kind), `FX_ENGINE=x,y,z`, `FX_PLANE`, `FX_SPEED`, `FX_SPAN`; `FX_NATION` whose guns. Stills:
  `battle-fx/stills/air-*.png`. For measuring a trail: `FX_FLAT_BACK=r,g,b` paints the sky one flat colour (run it
  over 0,0,0 and 255,255,255 and the two pictures give the trail's opacity and its own colour), and `FX_WAKE_UP`
  (250 m) is how high the `wake` shot's eye rides over the trail (0: level with it, seen side-on).

## The art director's mends to the air look (after head ea8c1ba)

The art director's verdict on ea8c1ba, 19:52Z 20 November: six mends, each behind a new key, so ea8c1ba's recipe
and every older one draw as they did.

- **A tracer is a soft glow round a bright core, never a tube** (`air_guns.tracers.glow_soft`). Solid across by
  day, the glow was a bar darker than a pale sky round a sky-pale core: a hollow tube, and end-on a ring. With the
  key the glow is strongest beside the core and fades smoothly to nothing at its own edge, with no rim, and a
  streak short on the screen (a round coming at the eye) has less of it. The cores are hotter: `us` lands
  255,236,212, 25 and more over a pale sky, the red only in its glow. `one_fan_px`: a far gun's tracer is a solid
  dash that wide in the core's colour, never an outlined oval.
- **A trail is a cone with a torn outline, never a tube.** `tail_widen`: past the tail it keeps widening, as its
  age over the time to the tail to this power (0.22 is half as wide again by 100 m). `edge_torn`: its two sides
  in and out by that much more. `fold`: a lighter fold along inside it, the strip up to that much thinner on one
  side, the side wandering along it; it never makes the strip denser. `knots`: the share of its pieces that
  carry a soft lump of denser smoke, `knot_r` of the strip's half width across, `knot_off` of it off the middle,
  `knot_alpha` as dense and `knot_dark` as dark, for `knot_s`; one past 0.6 off the middle is the column's own
  colour and lumps its outline. Three times as many in a gout, and none when the room is four fifths full.
- **An oil gout is a lump.** `gout_s` is a quarter to half a second, so a gout is two or three pieces long, darker
  and wider, and is seen running back down the trail.
- **The put-out streak shows on a pale sky.** The game blends in linear light, where a dark thing over a pale sky
  loses about a third of its darkness on the picture: `alpha` 0.75 reads 0.3 to 0.4 at the nacelle. Dark from the
  nacelle, paling to 0.6 of that by 60 m (0.15 on the picture) and no further.
- **Glycol shows on a pale sky.** A cool grey underside 10 below the sky (`shadow`), the top 10 above it; and
  `spurt_m`: a denser, wider spurt that long behind the radiator, `spurt_wide` times the source's width and
  `spurt_alpha` as dense, riding with the aircraft and thinning as the coolant runs out.
- **Nacelle flames are thin rooted tongues** (`flame_tongues`, `flame_over`). The one wide tongue a step read as
  flat orange shards lying on the skin. With the key, that many thin tongues a step, each rooted on the nacelle's
  skin at its own place round it, `flame_over` times past white at its root, orange along it and dark red at its
  tip, soft at its edge, licking back and a little off the skin (`flame_lift_of_length`); the first is the long
  one, and the tip smoke, the torn-off lick and the heart follow it.
- **A 20 mm puff is a ragged ball** (`air_hit.burst.puff_lobes`): the main puff and that many smaller ones
  crowding it, starting where it does. One rough puff read as a bird.
- **From the pre-check on ea8c1ba** (its tested cures): a put-out flame's shrink is held to 1, so it cannot grow
  should time go back; the tracer count forgets no end under 40 s ahead (a recipe's `life_s` times
  `tracer_life_k` could pass 20 s and switch the cap off).
- **Proofs.** de54166's recipe draws its 180 frames and ea8c1ba's its 44, byte for byte, on this code; both hashes
  unchanged; no warnings (g++ and clang, -Wshadow); wild numbers on 315 keys and the calls' arguments, 5,054 runs
  under ASAN and UBSAN, no faults. A whole trail at 80 m/s: fire 225 fans (it was 163), oil 138 (110), streak 89,
  glycol 23. 24 burning aircraft at once: 2,952 of 3,000 at most, never a frame without flame.
- **Still to come:** ground guns and flak by nation (the art director's ruling: one nation, one colour) want a
  `Nation` on `Shots` and `AirFlak`, with the ways down.

## White water: a hit in the sea (after head b84ea90)

The art director's note on build 1778, 17:54Z 20 November: a torpedo's, a near miss's and a flooding ship's water
columns were soft white billows that hung like steam or cotton. Wanted: white water, a fast spout to about mast
height with a hard ragged crown and streamers, falling back as a curtain within 3 to 5 s, a dark wet base, then a
foam ring and a thin drifting mist.

- **The row** `grounds.water.spout`. With it, `Shell` on water calls `Spout` in place of the skirt, the crown and the
  smoke: white water and nothing else. Without it, and on every other ground, a shell draws as it did, its dice in
  the same order.
- **What it draws.** Every piece is water thrown from the hit to a place of its own and brought down again by a
  gravity set from `fall_s`, so the whole column is back on the sea by then and nothing hangs. The body stands to
  three quarters of the size's `crown_m` (`wide` of the height at its stalk, `crown_wide` at its head; dark wet
  water, `base_colour`, for its lowest `base_to`, breaking white above). The top is fingers that rise inside the
  body and bend out of its head (`fingers`, `finger_lean_deg`, `finger_reach`), each torn to a point, with streamers
  of spray leaving their heads and the column's sides (`streamers`, `streamer_pieces`, `streamer_out`, `side_spray`;
  see the mend below). Going up the
  water is jets (`jet_stretch`); coming down each piece is drawn out longer, a curtain. Round the foot: a low dark
  heave (`foot`), a ring of spray racing out over the sea (`surge`), a soft mottled patch of foam that lasts
  (`foam`, `foam_reach`, `foam_alpha`, `foam_s`) and a thin mist down the wind (`mist`, `mist_alpha`, `mist_s`). The
  side away from the sun is shaded (`side_shade`, toward `shadow`).
- **Timing** for a 500 lb bomb: full height by about 1 s, the top back on the sea by about 3.5 s, foam for 10 to
  16 s. A small splash is quicker by the root of its height.
- **Sizes.** Any size may set its own numbers under `sizes.<size>.spout`. Two new rows: `torpedo` (the tallest and
  narrowest column, 55 to 70 m, dark for its lower third) and `churn` (the white water round a ship that is
  settling, going down or rolling over: a low boil of domes 3 to 6 m high and about five times as wide, slow, with
  foam that stays; see the mend below). `sizes.<size>.quiet: 1` means water moved by no explosion: no flash and no jolt (`churn` has it). A kit
  without these rows draws an unknown size as a 105's burst, so a scene passes them only with this recipe.
- **Cost.** The most pieces a column has at once, over six rolls of the dice, least to most: a 500 lb bomb's 296
  to 446 (the old column 1,078), a torpedo's 349 to 442, a 250 lb 236 to 314, a 105's 83 to 103, a light splash 36
  to 49, a churn 33 to 51. (The first notes gave one roll, 318, as the bomb's count.) A small splash is fewer,
  coarser pieces by its height. So is a hit far off: so far that a 50 m column would stand under `full_px` tall in
  the picture (the scene's window pixels) it has that share of its pieces, never under `far_least`, each as much
  bigger, and it keeps more of its fingers than of its body (a 500 lb column with the game's lens: all of them at
  470 m, 170 to 253 at 1 km, 102 to 134 at 1.5 km, 73 to 100 at 2 km). Past `halve_past` pieces asked for on the
  screen a new column is half as many, and with the room four fifths full a quarter: a salvo's last columns are
  coarse, never missing. Water already thrown and not yet born counts as asked for, since a column's spray and foam
  come a moment after it. A salvo of 12 500 lb columns raised within 0.6 s, at 470 m: 2,468 to 2,535 at most of a
  room of 3,000; at 1 km about 2,070; at 1.5 km about 1,415; at 2 km about 1,010. Sixteen at 470 m: 2,699 to 2,844.
- **For the preview:** `FX_KIT_PX=k` on a shell's shot too (the kit is told a picture k times as wide as the one
  drawn, as a scene tells it its window's pixels); a small test picture without it draws thinned columns.
  `FX_BARRAGE=12 FX_BARRAGE_GAP=0.05` is a salvo.
- **Hardening.** A size's `scale`, `crown_m`, `crater_m`, `shake_within_m` and its push are held to what a shell can
  be, and `SetWind` takes still air for any part of the wind that is not a number or is past any gale. Sane values
  draw as before.
- **Dark water stays dark.** The froth is shaded first (the sun's side, and one piece in five in the shade of the
  rest) and the base's dark laid over that, on the body and on the fingers' roots, so `base_to` shows on every side.
- **The art director's mend** (22:03Z 20 November, on head 17d5075: the column passed; its spray was "long,
  dead-straight hairlines that fan out like a firework", later hanging "alone above the column like rain or
  scratches"; the churn was "a white hedgehog mound, taller than wide"). A streamer is now `streamer_pieces` pieces
  thrown one after another from one place on the column (a finger's head on its way up, or the column's side) out
  to `streamer_out` of the height and a little up, under the weight of real water: at any moment an arc from its
  root that bends over, a line of dashes thicker at the root that breaks into beads at its tip; falling fast its
  dashes no longer reach each other, and it is gone before the column's top is down. A finger's tip goes before
  its root, so nothing stands alone over the falling column. The churn is `domes` under its own row: no column at
  all, but that many domes of pale water heaving up and sinking back over the patch, no two at the same moment,
  froth breaking round the foot of each, the patch between them dark troubled water, and one to three vents of
  spray. `streamer_reach` is gone from the row.
- **Proofs** (of the mended head). b84ea90's recipe on b84ea90's code and on this: 117 frames (sea and land 23, the
  air look 44 and 50), byte for byte. de54166's recipe: 115 frames (65 and 50), byte for byte. Both hashes
  unchanged. No warnings (g++ and clang, -Wshadow). Wild numbers on the 96 keys of the sea's rows, alone and all
  together, with wild winds and screen scales and a salvo of 16: 2,143 runs under ASAN and UBSAN, no faults.
- **Still to come:** the ways down. (Ship fires as ragged tongues: the next section.)

## Ship fires as ragged tongues (after head 3e3c4ef)

The art director's second note on build 1778, 17:54Z 20 November: the ships' fires were "rows of pointed orange
cones like birthday candles". Wanted: ragged tongues of uneven height, roots on the skin with a yellow-white heart,
tips tearing off and sweeping downwind.

- **The row** `fire.tongues`. With it (its `every_s`), a burning thing's flames (`Burn`, on a ship, a plane's wreck
  or the ground) are laid by `FireTongues`. Without it they are the one or two upright cones a step they were, by
  the same dice in the same order. The fire's smoke, its lamp and the steam of its putting out are untouched.
- **What it draws.** A fire burns at seats of its own (`seats`), fixed where it was lit and standing apart round
  its middle. Each seat throws tongues `tall` of the fire's width high, each seat its own share of that: one the
  tallest, more of them short, none the same (a fire over `tall_wide_m` across is less tall for its width). A
  seat's height surges and sinks by `pulse` several times a second, no two seats together. A tongue is `wide` of
  the width at its root, lives `life_s` and dies by being eaten away from below; it leans `lean` for each metre a
  second of wind (`lean_most` at the most) and bends further over toward its tip. `forks` of them have a thinner
  tongue out of the same root leaning aside, and `tips` of them a thin tip above the body, so a flame's top is
  torn and never one point. Under them each seat keeps a bed that never goes out while the fire burns: a broad
  yellow foot where the flame leaves the skin, and along it the fire's bright heart (`root`; see the mend).
  `torn`: the chance a step of a scrap torn off a tall tongue's top.
- **The art director's mend** (23:19Z 20 November, on head 3f7a8e9: the shape passed, "no more candles"; "each
  flame is one flat orange from root to tip"; the round root "reads as a lamp"; "taller far off"; the torn tips
  were "pointed darts flying across the smoke, which read as sparks").
  - *Light along a tongue.* A piece's fan is drawn from its root (`Bit::Hub`), its own colour there and its edge's
    at its rim and its tip: a tongue is `body` at its root, orange up its middle and reddening toward `tip`, a
    dull deep red, at its edge and its top; it reddens as it dies; its thin tip is that red, going darker, and goes
    by shrinking, never as a pale ghost on the sky.
  - *The heart is a band.* Along each seat's foot, across the eye's line, two short blunt licks of `root`,
    white-yellow, burning `root_over` past white under the tallest seat and less under the rest
    (`picture.lit_gain`), melting into the foot's yellow: a ragged bright band, never a disc.
  - *Taller far off.* `tall_wide_m` is 36, so a 33 m fire is as tall for its width as a small one, and a fire
    under `far_px` wide in the picture stands up to `far_tall` taller, so from a ship's distance it reads as fire
    before smoke.
  - *Torn tips.* A fifth of the steps, not half; each a ragged blunt scrap, dull red going to the smoke's dark,
    thrown up a little faster than the tongue climbs and slowed at once, gone in 0.2 to 0.3 s within a tongue's
    height.
  - Measured as she measured the first sheet (the mean lightness of the flames' top fifth, their middle and their
    lowest seventh, which gave her 123, 127 and 153 and me 120, 120 and 154): now 113 to 150 at the tips (150
    where a tip stands against the sky; 109 from 300 m), 143 to 149 in the middle, 182 to 199 at the foot, where
    the licks themselves are 223 to 234.
  - Passed on that sheet (23:51Z), with one touch asked: "soften the foot licks a little so they don't read as cut
    shapes up close". `foot_soft` 0.55: the share of a lick, from its root to its rim, over which it fades away
    (0.4 without the key, which is the sheet she passed). The licks' brightest tenth reads 214 to 230 near, where
    it read 223 to 234; 233 from 300 m. No piece more or less.
- **Seen through its own smoke.** A fire's smoke rolls off its middle, and from a ship's distance it blacked out
  every seat on the far side: three 33 m fires from 830 m showed one dim tongue each. `over_smoke`: the flames are
  sorted that share of the fire's width nearer the eye, so the far seats show through the smoke as fire does.
- **Put out** (`BurnOut`): the seats are drawn in toward the fire's middle, steadily lower, at fewer of them, each
  tongue as dense as ever; a fit only lowers them a little for its third of a second. Torn tips thin out with it.
- **Cost** (of the mended head). Pieces in the frame, smoke and all, mean while it burns (frames 150 to 300, four
  rolls of the dice): a 12 m fire from 48 m 48 to 61 against the cones' 38 to 39 (the roll sets the seats, 3 to 5;
  a 400 px picture, the kit told no other width). With the game's lens, the kit told a 2,881 px window: six 33 m
  fires from 830 m 310, at most 369, against 196 and 244; twenty-four 20 m fires from 830 m 436, at most 526,
  against 349 and 419; from 1,500 m 418 and 494 against 348 and 417. A fire under `full_px` wide in the picture
  (the scene's window pixels) burns at that share of its seats, never under `seats_least`, each as much wider,
  with as much fewer forks, tips and torn tips. As the room fills a fire keeps its bed and its tongues' bodies and
  loses the forks, tips and torn tips, by degrees: all of them with the room seven tenths full, none at seventeen
  twentieths (every fire reads the same room in the same step, so at one line they would all drop them and take
  them back together). Bodies and bed alone: the 12 m fire 43 to 51, the six 33 m fires 254.
- **Three guards from the pre-check.** Its notes E and F on 3e3c4ef: `Shell` at a place or moment that is no
  number, or beyond any field or battle, is no hit (it put points that were no number into the picture); the
  flash's dice hold the moment before the cast. Its note on 3f7a8e9, an old fault: `Run` at a moment that is no
  number, or past a thousand million seconds either way, lays nothing (a fire's loop steps by a twentieth of a
  second, and at 1e300 a step no longer moves the moment, so one endless fire and one such Draw laid pieces until
  memory was gone). Ordinary hits and ordinary moments draw as before.
- **For the preview:** a plain fire: `FX_AT=9999 FX_BURN=12 FX_DIST=48 FX_EYE_H=14 FX_LOOK_H=9 ... fate out 60 800x600`
  (`FX_AT=2 FX_OVER=6` puts it out; `FX_LAND=1` on land). A ship ablaze: `FX_BURN=33 FX_BURN_ROW=3,78 ... oil sunk`.
- **The churn's domes eased in** (the pre-check's note C on 3e3c4ef, the art director 23:19Z).
  `sizes.churn.spout.dome_in_s` 0.3: a dome's water comes up out of the sea over that long. Before, a dome born
  while the boil stood lifted the picture's ink by up to 10.4% in one frame over its neighbours (one churn from
  150 m); now 2.5% at most. Without the key a dome is born as it was.
- **Proofs** (of the mended head). 3e3c4ef's recipe on 3e3c4ef's code and on this: 189 frames (sea and land 23,
  the air look 50 and 44, guns, shells and fires 65, a churn and a torpedo 7), byte for byte. b84ea90's recipe:
  182. de54166's recipe: 138 (23, 50 and 65). Both hashes unchanged. No warnings (g++ and clang, -Wshadow). Wild
  numbers on the 31 keys of the row, alone and all together, on a near fire, a far one, a ship ablaze, one being
  put out and one on land, with wild winds, screen scales and fires of no size, a tiny size and a vast one: 976
  runs under ASAN and UBSAN, no faults. The sea's rows again with the new key, 97 keys: 2,157 runs, no faults.
  3f7a8e9's own recipe draws differently on this code (the mend is not behind keys; that head was never built).
  All of it run again over the `Run` guard: the sheet's frames the same to the byte. And again with `foot_soft`
  (32 keys, 1,004 runs, no faults): without the key the head before it draws the same to the byte (7 frames).
- **Still to come:** the ways down's ends (the sea, the ground); ground guns and flak by nation.

## Ways down in the air: a flamer's stream, fuel mist, the ruled explosion (after head efc7498)

The art director's ruling of 15:02Z 20 November (`air-fates-look`, B and E), for Brian's ask that aircraft go
down in many ways and never the same way twice. The battle's event says how (`how`) and which of four looks
(`look`, 0 to 3); the kit throws no dice for a look.

```cpp
// a flamer: fire streaming from the wing root past the tail, by the event's look
Blasts->TrailStart(AirNow, RootId, RootWorld, PlaneVelocity, TEXT("fuel"), SpanMetres, Event.look);
// a wing torn off: white fuel from the root; TrailStart "fuel" on the same number if it catches
Blasts->TrailStart(AirNow, RootId, RootWorld, PlaneVelocity, TEXT("mist"), SpanMetres);
// how it starts to go down: the burst at the break, then AirFallMove each frame as before
Blasts->AirFall(AirNow, PlaneId, BreakWorld, PlaneVelocity, TEXT("wing"), SpanMetres, DeckZ, Event.look);
// when it blows up (the event's boom), with the same number
Blasts->AirFall(AirNow, PlaneId, PlaneWorld, PlaneVelocity, TEXT("explode"), SpanMetres, DeckZ, Event.look, bBombsAboard);
```

- **A flamer** (`air_trails.fuel`, a `fire` row with `stream_lengths`). "It streams as long torn tongues 1 to 3
  aircraft lengths behind, bright at the source and orange to sooty further back, turning to thick black smoke.
  It is never a smooth sheet on the skin." The stream is `stream_lengths` by the look (1, 1.5, 2, 3) times
  `length_of_span` of the span long, at most `stream_most_m`. Each sixtieth or so it throws `stream_strands`
  tongues and two pale ones at the root, left behind in the air at `stream_carried` of the aircraft's speed, so
  each is torn back along the path; a tongue dies anywhere along the stream, the long-lived ones bigger, and its
  colour goes by how far back it is: `stream_hot` at the root, `stream_body`, then `stream_deep` mixed toward
  `stream_soot` by `stream_sooty` for the look. `stream_wide_m` is a tongue's half width on an 11 m fighter (it
  grows with the root of the span), `stream_ragged` how far tongues wander from the line, in half widths. The
  black smoke starts `stream_smoke_from` of the way down the stream. The flames are sorted over their own smoke.
  With the room 85% full it throws one strand and no pale ones. A row without `stream_lengths` is as it was.
- **Fuel mist** (`air_trails.mist`): a white torn spray about 170 m long that pales and is gone in under two
  seconds, never smoke. It takes `blend_s` to become `fuel` when the root catches.
- **The explosion** (`air_debris.explode`; without the row `explode` is the short torn fireball it was, by the
  same dice). "A ragged orange fire cloud of burning fuel, never a clean sphere. About 1 to 1.5 wingspans across
  for a fighter and 1.5 to 2 for a bomber, bigger and whiter-hearted with bombs aboard. Black smoke boils out of
  it within 0.3 s; the fire is gone in 0.5 to 1 s and the black cloud hangs 5 to 20 s." By the look:
  `across_fighter` or `across_bomber` spans across, `hang_s`, `pieces` times the debris row's count, `streaks`
  clots of burning fuel. With bombs aboard (`AirFall`'s last argument) it is `bombs_across` times as wide, its
  white heart `bombs_heart` times as long, in more knots further out along the tears (never one flat white
  shape), and it throws `bombs_streaks` streaks at least. The clumps burn up to `fire_s`, the shortest out and
  smoking within a third of a second, cooling from orange to a dark `ember` by one part in `cool` of their time;
  a clump stays whole until its smoke has it and is gone in a frame or two, because a long see-through fade over
  the bare sky is pink. The smoke (`smoke_grow`, its edge `smoke_rough` ragged) is thick through half of `hang_s`.
- **Fuel streaks** (`air_debris.explode.streak`): "2 to 5 burning fuel streaks curve down, orange heads with
  black tails." A clot is heavier than the cloud's gas: thrown `throw_mps` (harder from a bigger cloud) and
  carried on at `carry` of the aircraft's speed, slowed by `drag`, it comes out of the cloud a moment after the
  flash and curves the whole of its `burns_s` toward falling at `sink_mps`. Its head shrinks as it burns, flame
  is torn off it along its path, and its black smoke is left along the whole path in `steps` lengths for
  `smoke_s`. A bomber's clots are bigger, burn longer and fall faster.
- **The sparkle** (`air_debris.sparkle`): "a sparkle of small metal bits catches the sun for 1 to 3 s": `sparkle`
  winks, each a point of light for `life_s`, in the cloud of small metal as it is carried on, spreads and sinks.
- **The other ways** (`air_debris.ways.<how>`): `burst` small torn panels at once where it breaks (half as many
  again for a bomber), a `sparkle`, `sparks` where two aircraft meet, then `sheds` more panels over the fall;
  burst and sparkle are 0.7 to 1.3 times that by the look. `pilot` and `glide` throw nothing: "its silence is its
  read". `flamer` and `burn` shed as a burning fall always has. The big pieces are the model's own.
- **Far off and crowded** (ruling E). A fall whose span is under `air_debris.far_px` pixels on the screen (24: a
  fighter past about 1.7 km at the game's lens on a 2,881 pixel window), or one that starts with the room four
  fifths full, throws no pieces, streaks or sparkle; an explosion keeps its flash, its fire and its black cloud.
- **Guards.** `AirFall` and `AirFallMove` do nothing for a place, a speed or a time that is no number or without
  end; a span is held to 200 m and a deck that is no number is one 3 km down. A look over 3 is 3.
- **Counts** (pieces in the frame, mean and most over seeds 1, 7 and 99; a 640 pixel picture told as a 2,880
  pixel window, lens as the preview's). A fighter's explosion, frames 16 to 200, by the look: 29 to 38 and 75;
  57 to 62 and 78; 74 to 82 and 102; 106 to 110 and 138. A bomber's, look 1: 113 to 126 and 159; with bombs,
  look 3: 185 to 203 and 237. The far version: 23 to 32 and 52. A collision's burst, look 3: at most 75. A
  fighter's flamer with all its smoke, frames 100 to 300: look 0 154 to 158 and 165, look 3 194 to 198 and 215
  (the `fire` trail it stands beside: 161 to 165 and 175); a bomber's, look 3: 214 to 221 and 236. Mist: 74, 76.
- **Proofs.** efc7498's, 3e3c4ef's and b84ea90's recipes on their own code and on this: 210 frames each (sea
  and land 23, the air look 50 and 44, guns, shells and fires 65, falls of every old kind and the old trails
  28), byte for byte. de54166's: 156 against its own code, and its 28 fall and trail frames against efc7498's
  code (its trails were mended at ea8c1ba). Both hashes unchanged; the ring test as it was (12 aircraft at most
  2,745, 24 at most 2,935). No warnings (g++ and clang, -Wshadow). Wild numbers: the 276 new keys alone and all
  together, and wild spans, speeds, looks, decks, winds and screen scales on a flamer, mist, three explosions
  and every way: 1,889 runs under ASAN and UBSAN, no faults. (A span without end found a cast of no number in
  the preview's own drawing of the aircraft, mended there; the kit was sound.)
- **For the preview:** `FX_TRAIL=fuel FX_LOOK=3 FX_SPAN=11 ... air hmg 150 1600x1000 beam`;
  `FX_FALL=explode FX_LOOK=3 FX_BOMBS=1 FX_STAY=1 ... air hmg 16,24,60 640x400 fall` (FX_STAY: the eye stays on the
  cloud); `FX_FALL=wing FX_FALL_TRAIL=mist ... fall` (FX_FALL_TRAIL: the trail a fall is seen with, `none` for none).

### The art director's three settings (after head 3355503)

The art director passed 3355503 (04:00Z 21 November) with three settings. Each is behind recipe keys: without
them the head draws as 3355503 did, byte for byte.

- **The dying fire** was "a red flower". `air_debris.explode.ember`, `clump_edge` and `cool` now take a ruled
  clump from orange through a dark ember brown into its soot, with no red. The colours are written to LAND
  brown in the game's picture (`game_curve.py write 66 42 24` gives 88, 68, 53): a dark colour written as it
  looks lands near black-red, since the game's film curve crushes the green and blue of a dark colour first.
  Measured over a bomber's cloud with bombs in the game's curve, half a second after the burst: 3355503 had
  11,655 of its 17,379 fire pixels red (a hue under 14 degrees, the middle one 7); this has 3 of 16,347, the
  middle one 26. From 0.3 s to 0.9 s the middle hue runs 30 down to 23 and never more than 19 pixels are red.
- **The bombs' heart** was "a flat pale plate". With `heart_edge` a ruled heart is one hot core: a heart is
  whiter, bigger and drawn nearer the closer it lies to the cloud's middle (`heart_fall`), each falls off to
  `heart_edge` (by `heart_edge_in` at four tenths of its radius) at a ragged edge (`heart_rough`). Its 0.2 s
  is kept. The dice are thrown as before, so the cloud round it is the same cloud.
- **The mist** takes the glycol's cool grey underside (`air_trails.mist.shadow`, `shadow_k` 1, `core` 0.45):
  under the white the row reads 204 and its darkest 201 on a 211 sky, the top as it was (221).
- The flamer keeps its 1.2 s to full length.
- **Counts** are unchanged: no piece is added. **Proofs.** 3355503's recipe on its own code and on this: the
  210 standing frames and 81 of its own (the ruled explosion at each look, with bombs, far and by other seeds;
  the flamer at looks 0 and 3 and a bomber's; the mist; five other ways), byte for byte. efc7498's, 3e3c4ef's
  and b84ea90's 210 each and de54166's 156 as before. Both hashes unchanged, the ring test as it was, no
  warnings. Wild numbers on 285 keys: 2,015 runs under ASAN and UBSAN, no faults.

## Fires that ride with a ship, and the sea's distance (after head 1fc2904)

Brian, 21 November: "when they do hit a plane or something with gun fire than it shows up on the receiving
object". The sea battle scene found two gaps on the way: a hit's fire stayed on the water where the ship was
struck while she steamed on at 25 knots, and the enemy line, 5 to 15 km from the eye, showed no flash and no
column at all (the kit drew to an air scene's 4 km, and beyond it a gun's flash is under a pixel).

```cpp
// every frame, for each fire, steam leak and cook-off on a ship under way (zero velocity once when she stops)
Effects->BurnMove(FireId, WorldAt, Velocity);
Effects->SteamMove(LeakId, WorldAt, Way, Velocity);
Effects->CookOff(At, Up, Across, Pops, Over, Jet, JetFrom, CookId);   // (an id, so it can be moved)
Effects->CookOffMove(CookId, WorldAt, Up, Velocity);
// once, after the kit is made, in place of UseAirDistance
Effects->UseSeaDistance();
```

- **What rides.** A piece with no motion of its own (an ember, a glow on the deck) is moved with the seat. A
  new piece is born where the seat is at its own moment. A burning piece keeps the share of the ship's way the
  air has not got of it for 2 s (a root nearly all, a torn tip none), so the fire stays on her and its tongues
  lean aft by the wind over her deck; a thrown piece keeps all of it until it lands; smoke and steam are left
  to the air at once and stream astern. A vent's rag and a hatch's jet are drawn out along their flight as seen
  from her. A big fire's own column moves with its fires, and its puffs are widened so they still overlap.
- **Rules of the calls.** An id no fire has, or a move before the fire is lit: nothing. A place that is no
  place: that move is dropped. A velocity that is no number or over 150 m/s counts as none. Without a move
  call a fire is as it was: every passed head draws the same.
- **The sea's distance** is the recipe's `sea` row. `draw_beyond_m` (16 km) is how far the kit draws.
  `flash`: where a gun's or a hit's whole flash stands under `glow_px` (12) across it is far: its flame is
  never under `least_px` (4), turns `hot` of the way to `colour` and burns `over` times white, and a soft glow
  of the flame's own colour is laid over it for `glow_s` longer. `column`: a shell column's jets and fingers
  are never under `least_px` (6) long, a small splash less by the root of its height, so the heavy shell's
  stays the tallest. `gone_px`: beyond the air's 4 km a piece of smoke, dust or water under 2 pixels across
  takes no place. `hit`: beyond 4 km a hit's smoke is fewer, bigger puffs the smaller it stands (all where
  its crown is `full_px` tall, never under `least`, coming in over `over_m`), and under half it has no skirt.
- **Inside 4 km** only two things differ from an air scene: a flash under 12 pixels across (the howitzer
  row's from between 800 m and 1 km at 1,920 wide) and a column's jets under their floor. To 800 m no frame
  differs; at 1 km only the flash frames; at 2.5 km those and the column's. Stills: `stills/sea/`.
- **Counts,** the most in any frame of 10 s of the preview's sea shot (four turrets firing, three shells in
  the sea, two hits on a ship, a 20 m fire; two seeds), at 1,920 and at 2,880 pixels wide: 3 km 1,772 and
  1,780 (as an air scene); 4.5 km 852 and 1,121; 5 km 583 and 808; 7 km 234 and 476; 10 km 168 and 327;
  15 km 107 and 167. Of the 1,920 figures the two hits are 653, 430, 173, 103 and 45 from 4.5 to 15 km, the
  three columns about 70, the fire about 55, four turrets firing twice a second 117 at 5 km and 13 at 10 km.
  A fire that moves costs what a still one does. Inside 4 km a hit is the 1,700 pieces it always was.
- **Proofs.** 1fc2904's recipe on its own code and on this, and this recipe on this: 291 frames each, byte for
  byte. 3355503's 291, efc7498's, 3e3c4ef's and b84ea90's 210 each and de54166's 138 as before. Both hashes
  unchanged, the ring test as it was, no warnings (g++ and clang, -Wshadow). Wild numbers under ASAN and UBSAN
  with float-divide-by-zero: 951 direct cases on the move calls (every argument no number, endless, a leap,
  a clock that jumps, ids no fire has, one number for three kinds, forty fires, wild eyes and lenses) and 832
  preview runs on the `sea` keys: no fault in the kit.
- **Two of the pre-check's notes on 1fc2904 taken:** a piece of no life is never laid (it divided nought by
  nought), and an explosion's piece count is held to 400 after the recipe's multiplier, not before.
- **Not here:** a ship gun's own flash and cordite cloud (the scene's guns are the howitzer's row still)
  and columns by calibre, each to the art director's ruling. Funnel smoke is the next section.
- **For the preview:** `FX_DIST=10000 ... sea far 30,45,75,150 2880x1620` (the top of `fxpreview.cpp` has the
  dials); `FX_SHIP=12.86,0,0 FX_SHIP_STOP=11 ... fate out 450 1280x800`.

## Funnel smoke (after head 78f6edf)

The sea battle's first Mac frames (21 November) borrowed the air `smoke` trail for funnels, and it drew a
picket fence of tall dark strips astern of each ship: that trail lays a piece every 9 m of the path and keeps
one in six, three times wider, which is 800 m behind an aircraft and 50 m behind a ship. The art director ruled
a look of its own (`art-director/explosions/funnel-smoke-ruling.md`, 05:30Z): the trail kind `funnel`.

```cpp
// once a funnel: At its top, the ship's velocity, the funnel's WIDTH AT ITS TOP in metres
Effects->TrailStart(Now, FunnelId, FunnelTop, ShipVelocity, TEXT("funnel"), FunnelWidthMetres);
// every frame, also when she lies stopped: Power is the share of her top speed she is making, 0 to 1
Effects->TrailMove(Now, FunnelId, FunnelTop, ShipVelocity, Power);
```

- **Laid by time, not by distance.** A puff is let go into the air the ship steams through and keeps none of
  her speed: it drifts with the kit's wind and climbs, its height `rise_k` x root(funnel) x age^(2/3) over
  the cube root of the wind she feels (never under `felt_least_mps`). So a fast ship's plume lies low and
  long astern, a slow one's stands higher, and a stopped ship smokes on, straight up. A distance-laid trail
  would stop when she does.
- **Round puffs that always overlap.** A puff is `lip_of_funnel` funnel widths across at the lip and widens
  by `wide_k` x funnel^`wide_by_funnel` x (metres of felt wind gone by)^(2/3). One is laid every 1/`overlap`
  (3.5) of a puff's width. `size` and `shade`: each puff that much bigger or smaller, lighter or darker; its
  climb and its drift across the wind wander over about three seconds.
- **Laid evenly** (`even`, after head 1ec1f4c). The art director measured the first sheets, 14:13Z 21
  November: "sky between" the puffs in the first 100 m astern, a dark knot at the lip, a gap at 10 m. The
  first law thinned the plume by an even spread (puff n lived by the n-th number of it), which leaves the
  ones that hang on 2, 2 and 4 places apart, and a puff is thick only about its middle: every third gap was
  half as deep. Now every other puff goes at once, each time the plume has doubled its width, so the ones
  left always stand evenly, one place, then two, then four apart. A puff thins as the plume deepens in them
  (the same smoke over more puffs; worked as the light it lets through, so a dense puff keeps its smoke),
  and over the last `even` (0.3) of each such stretch the ones that go thin to nothing while those between
  take their smoke up: nothing pops. The first `even_near` (2) halvings come at each doubling, about 8 and
  40 m astern of a destroyer; the next `even_far` (2) sooner, each when the plume is `even_step` (1.6)
  times wider, about 100 and 215 m (far astern it is faint and soft and needs fewer puffs deep); the rest
  at each doubling again. So the plume is 3.5 to 7 puffs deep over its first 100 m and 2.2 to 4.5 at its
  old end. `rough` is a puff's lumpy rim, and `heart_round` 1 keeps its heart ring a true round under that
  rim: the outline billows and the middle of the plume stays as thick whichever way a puff is turned. A
  row without `even` is thinned as before.
- **The art director's measure of beads:** the darkest level in each 6 px column along the plume, at true
  size (1,600 wide from 600 m), over the first 100 m astern; between two knots it never climbs more than a
  third of the way from the nearer knot toward the sky. Over 35 frames a second apart, a destroyer through
  a surge: the worst climb 0.17 on average, 0.32 at most, none over a third. Steady full power: 0.21 and
  0.32, none. A capital ship: 0.15 and 0.34 through a surge, 0.19 and 0.38 steady, two frames of 35 a hair
  over in each. The measure is in shown levels, and dense black smoke on a bright sky is where it bites: a
  puff's worth of smoke more or less moves the level by 30. Sheet: `stills/funnel/funnel-even.jpg`.
- **It rolls** (`roll_lift`, `roll_shade`, `roll_run`, after head 1dd07e8). The art director passed the spacing,
  16:03Z 21 November, and found the plume now "evener than the ruling", "a smooth tube": put the lumps back
  (puff size 0.3, rim 0.25), with the bead measure "still never over 0.33". Measured, those dice break the
  measure: over a third in 10 frames of 35 through a surge and in 14 steady, 0.43 and 0.48 at most. So did
  every way of making a stretch of the plume denser or its puffs bigger that was tried (twenty of them): the
  measure allows the thinnest place two thirds of the smoke of the thickest, and the evenly laid plume stood
  at 0.32 already. What the measure does not mind is where the puffs sit and how dark they are. So the plume
  rolls: a slow wave along it by the puff's number, one value every `roll_run` (4) puffs at the lip and
  twice as far apart each time the plume has doubled its width, so a roll is always about as long as the
  plume is wide and goes with the smoke. Where the wave is high the puffs ride `roll_lift` (0.35) of their
  radius higher and are `roll_shade` (0.1) darker; where it is low, as much the other way. No lift at the
  lip, in full once the plume is near three times as wide (at the lip a lifted puff was a hook at her
  funnel). The wave is never over 1: no dice, no tails. `shade`, the dice, came down to 0.1 so the two
  together stay about the ruling's 15 in a hundred. Bead measure with the roll: a destroyer through a surge
  0.17 on average and 0.32 at most, steady 0.21 and 0.32, over a third in none of 35 either way.
- **A wide funnel's puffs are laid closer** (`overlap_wide` 4 at `wide_funnel_m` 8 and wider, in proportion
  between `funnel_m` and that: a cruiser's 5 m funnel 3.7). The art director of the capital ship's two
  frames in 35 a hair over: "Lay hers at a quarter of a width. The extra pieces are worth it, because beads
  read at any range." Each puff holds as much less smoke (by the light it lets through), so the plume is as
  dense as before: lip 0.78, 0.46 at 100 m, 0.13 at 300 m. Bead measure: through a surge 0.13 on average
  and 0.31 at most, none over; steady 0.14 and 0.36, one frame of 35 over (4.5 deep did no better: 0.38).
- **Power.** `full` is the look from `full_from` (0.75) of her power, `cruise` the look at `cruise_to` (0.4)
  and under, blended between: `alpha` a puff's opacity at the lip, fallen by e at `fade_s` seconds old (as
  the power `fade_pow` of its age), `age_s` how long the oldest hang, `colour` going to `colour_old` by
  `old_by_s`. Power left out (under 0): her speed over `top_speed_mps`. **The surge:** when the share
  stands `surge.rise` (0.08) over where it was about `surge.within_s` (5) before, she has rung up, and for
  `surge.for_s` (15 to 25 s) the smoke is the black `surge` look, in over 2 s and out over 5.
- **Far off.** `lip_least_px`: a puff is never under 3 pixels at the lip in the last frame's picture (at
  most `lip_most` 8 times its size: at 4 a destroyer's first puffs at 15 km stood under the sea's
  `gone_px` and left a gap of two or three pixels at her funnel), so a far plume is fewer, bigger puffs.
  `overlap_far` and `far_px` can thin it more (where the true lip is `far_px.0` pixels or under the puffs
  overlap only `overlap_far` deep, each as much denser); it stands at 3, a little under `overlap`, because at 2
  a plume at 10 to 15 km broke into dots at its root.
- **Measured** (no wind, the same frames on black and on white, five frames 3 s apart; width where the
  opacity is over a fifth of its peak), against the ruling in brackets. Destroyer, 3 m funnel, 25 knots,
  full power: at 100 m astern 16 m wide (15 to 20), opacity 0.45 (0.45), centre +12 m (10 to 15); at 300 m
  33 m (30 to 40), 0.13 (0.15), +30 m (25 to 30); lip 0.77 (0.75), lit 66,62,59 (55 to 75), shade 46 (35 to
  50); lit 112 at 300 m (110 to 140); 0.01 at 450 m and gone by 550 m (450 to 600). Capital ship, 8 m: 30 m
  (25 to 30) and 55 m (50 to 60), 0.45 and 0.12, +23 and +44 m (20, 40 to 50); lip 0.78. Cruising at 12
  knots: lip 0.24 (0.25), 0.10 at 60 m (0.10), gone by 150 m, lit 152,150,146 (130 to 160). The surge (the
  art director on the first sheet, 14:09Z: at 64,61,59 it "doesn't read as black"; now 56,52,48): at the
  lip lit 35,32,28 (25 to 40, red over blue by 4 to 10), shade 24,22,19 (20 to 30), opacity 0.88 (0.9);
  lit 44 at 30 m astern (with the roll; before it 32,28,25 and 20,17,15). Stills: `stills/funnel/`.
- **Counts,** a funnel (the mean of three ships over five frames), at 1,920 and at 2,880 pixels wide with
  a 55 degree lens, 25 knots at full power, no wind, nothing left out for being off the screen; the last
  head (1ec1f4c) by the same count in brackets. A destroyer's: 63 at 600 m (56), 57 and 62 at 1.5 km (56),
  54 and 55 at 3 km (49, 56), 50 and 52 at 5 km (40, 48), 38 and 45 at 10 km (31, 37), 29 and 38 at 15 km
  (24, 31). A capital ship's, laid 4 deep: 44 at 600 m and 42 at 3 km (30), 34 and 41 at 5 km (30), 29 and
  33 at 10 km (24, 29), 25 and 29 at 15 km (19, 24). At 30 knots and all her power: 69 (63) and 50 (34).
  Cruising at 12 knots: 28 (19) and 16 (9). Through a surge: 58 at most (50) and 40 (26). A cruiser's 5 m
  funnel: 53. The game's own following wind: the same. So an evenly laid plume costs a destroyer's funnel
  an eighth more than the first head's and a capital ship's, near, nearly half as much again. Sixty
  destroyer funnels started together near the eye, 14 s on: 2,755, inside the room. The most a scene that
  smokes its 8 nearest ships can ask: all destroyers, three funnels each, all near and at 30 knots, 1,656,
  a little over half the room. A plume a funnel on the three nearest ships (two funnels each) and one a
  ship on the five beyond: about 760 for destroyers, 540 for capital ships. The roll adds no piece.
- **Rules of the calls.** The kit holds the width between 1 and 14 m. A plume throws dice of its own, from
  the trail's id and the puff's number: no other effect is thrown differently because funnels smoke, and a
  funnel's plume is the same in every play. TrailMove's Power is a last argument with a default: every
  earlier call compiles as it was. A recipe without the row trails "funnel" as smoke, as before. The first
  power a trail is given is where she stands: she surges only on a rise after that. A trail stopped with
  TrailStop and started again under the same id starts its plume afresh (the sea scene's ships leave its
  set and come back): no surge from the power she had before, and none left over.
- **Proofs** (the evenly laid head). 1ec1f4c's recipe on its own code and on this: 377 frames byte for
  byte, 35 of them funnel shots (side views, cruising, the surge, stopped, wind, astern, the far line). This
  recipe on 1ec1f4c's code and on this: 233 ship, ways, sea, air and fall frames the same but the funnel's
  own 2. The recipe gains 5 keys and changes 9 values, all under `air_trails.funnel.plume`. 78f6edf's
  recipe 342 frames, 1fc2904's and 3355503's 291 each, efc7498's, 3e3c4ef's and b84ea90's 210 each,
  de54166's 138, as before. Both hashes unchanged, the ring test as it was, no warnings (g++ and clang,
  -Wshadow). Wild numbers under ASAN and UBSAN with float-divide-by-zero: 2,060 direct cases (every argument
  of both calls no number or endless, a clock that jumps, a leap, a kind that changes and back, a trail
  stopped and started again, sixty funnels, a wind, an eye and a lens that are no number, and each of the
  row's 56 numbers made wild in turn) and 20 preview runs: none bad. A funnel stopped and started again
  under her id, back after a frame to 5 s at more power: no surge (1ec1f4c surged every time); never
  stopped, or ringing up after she is back: she surges.
- **Proofs** (the rolling head). 1dd07e8's recipe on its own code and on this: 377 frames byte for byte (35
  funnel, 51 ship, 81 ways, 23 sea, 50 air, 44, 65, 28). The seven older heads' sets as before. The recipe
  gains 5 keys (`roll_lift`, `roll_shade`, `roll_run`, `overlap_wide`, `wide_funnel_m`) and changes one
  number (`shade`), all under `air_trails.funnel.plume`. Both hashes unchanged, no warnings (g++ and clang,
  -Wshadow). Wild numbers under ASAN and UBSAN with float-divide-by-zero: 2,190 direct cases (calls 404, the
  eye 200, each of the row's 61 numbers made wild in turn 1,586) and 17 preview runs: none bad.
- **For the preview:** `FX_FUNNEL=funnel FX_FUNNEL_SPAN=3 FX_POWER=0.85 FX_FOLLOW=1 FX_DIST=600
  FX_LOOK_X=-190 FX_BURN=0 FX_NO_HIT=1 FX_FIRE_AT=1e9 FX_FALL_AT=1e9 ... sea side 1200 1600x700`;
  `FX_BG=0` and `FX_BG=1` give the pair to measure opacity from.

## Smoke that goes slowly into the air (after head b15af20)

Brian, 21 November: "have the smoke effects gradually fade away into the air not disappear right away". The art
director's ruling (`art-director/explosions/smoke-burst-fire-ruling-r2.md`): a cloud is as thick as it was until its
thick time, then widens, lifts, drifts and thins on a long tail. It is at half its thick strength a third of the way
through the tail and at a tenth by two thirds, the last fifth stays under a twentieth so nothing pops, and it is two
to three times as wide by the end. Old smoke pales to the haze behind it, never to white and never pink.

The recipe's `linger` row holds it, a kind at a time. Seconds after the burst:

| Kind (row) | Thick until | Gone by | Before |
|---|---|---|---|
| grenade (`shell.by.grenade`) | 1 | 8 | 3.5 |
| light, mortar | 1.5, 2 | 11, 15 | 8 to 10 |
| 75 mm, 105 mm, 150 mm | 3, 4, 5 | 22, 30, 38 | 14, 17, 38 |
| 250 lb, 500 lb bomb | 6, 8 | 45, 60 or its old life if later | 45 to 90, 60 to 120 |
| a tank's, anti-tank or field gun's smoke and dust (`gun.by.*`) | 1 | 10 | 4 |
| flak burst (`flak`) | 8 | 25 | 9 |
| hit puff on an aircraft (`hit`) | 0.5 | 6 | 2.4 |
| aircraft explosion cloud (`burst`) | 4 | 30 | 12 |
| cook-off smoke, each pop and the jet (`cook`) | 2 | 15 | 4 |

Rifle and machine-gun smoke, fire columns, funnel smoke, steam and spray go as they did. Smoke trails after their
source stops come with the trail head. Take a kind's row or its times out of the recipe and it goes as before.

- **The curve.** `left_third` and `left_two_thirds` are what is left of the cloud's smoke a third and two thirds of
  the way through the tail; the kit runs smoothly between, on to four fifths, and from there to nothing. They are
  not the opacity: a deep cloud must lose most of its smoke before the eye sees it thin, so they are tuned in the
  preview's pictures until the cloud reads half as strong and then a tenth against the empty frame.
- **Fewer puffs.** `keep` of a cloud's puffs stay to the end and take over the smoke of the rest, which retire over
  the first `go_by` of the tail. Flak, hit puffs and the aircraft cloud keep all of theirs (so few, they read as
  separate balls with half gone).
- **Wider, higher.** By the end a puff is `wide` wider again and stands `spread` further again from the cloud's own
  middle (found from the puffs, carried by the wind), and it climbs `lift_mps` more.
- **Its old looks.** Everything else about a puff (its own widening, colours, heart, lumps) keeps the clock it was
  laid with, so a burst is unchanged up to its thick time. A puff in its tail carries no lumps.
- **Colour.** From `pale_from` to `pale_by` of the tail the puff goes `pale` of the way to `sky_share` (0.9) of the
  sky or haze the scene gave, or to `to` by the hour's light where no scene has said.
- **Room.** With over `spare_from` of the room (game.max_puffs) asked for, a new cloud gets less of its `keep` and
  `go_by`, down to `spare_least` of them at `spare_by`. The cloud goes by the same law with fewer pieces.
- **Never sooner.** A cloud is gone no sooner than its longest puff was before the rule: its tail runs from its
  thick time to the later of `gone_s` and that, by the same curve. A bomb's smoke lived 45 to 90 s (250 lb) and 60 to
  120 s (500 lb) by its size's `linger_s`, 1.6 times that on sand, and `gone_s` alone cut it short (the pre-check on
  86d71f2). Shells on earth are under their `gone_s` and go as the table says.
- Any of a kind's numbers may be given for one size or gun under `by.<name>`.

```cpp
// once the scene's sky is set, and again when the hour or the weather changes it: the haze at the horizon for a
// ground or sea battle, the sky at the fight's height for an air one, as the SCREEN shows it (pixel / 255)
Blasts->SetSky(FLinearColor(209.f / 255.f, 212.f / 255.f, 215.f / 255.f));
```

Measured in the preview's pictures in the game's curve (a third / two thirds / four fifths of the tail, as a share
of the cloud when thick): grenade 0.52 / 0.08 / 0.05, light 0.48 / 0.08 / 0.02, mortar 0.50 / 0.09 / 0.03, 75 mm
0.50 / 0.09 / 0.02, 105 mm 0.52 / 0.08 / 0.02, 150 mm 0.52 / 0.11 / 0.03, 250 lb 0.50 / 0.10 / 0.04, 500 lb 0.51 /
0.11 / 0.04, an anti-tank gun's smoke 0.52 / 0.09 / 0.03, flak 0.48 / 0.10 / 0.04, the aircraft cloud 0.55 / 0.11 /
0.03. Flak against a sky of 209,212,215: red minus blue -6.3 and -7.0 against the sky's -6, the worst pixel 3 over.

Pieces (1,600 px wide, a 55 degree lens; held at most, the rule off in brackets): one 105 1,034 (1,034), its tail
130 puffs falling to 65; 105s every 3 s for two minutes 2,465 (2,141); 105s one a second 4,122 (4,596); 75s one a
second 3,225 (3,077); mortars two a second 4,070 (4,444); eight tank guns each firing every 6 s 639 (221); flak two
bursts a second 554 (205), six a second 1,610 (586); an aircraft exploding every 4 s 1,876 (1,722).

The preview: `FX_SKY=r,g,b` tells the kit the sky as the game's screen shows it. For the burst shot `FX_FOLLOW=1`
carries the eye with the wind so an old burst stays in the picture, and `FX_FAR=2` and so on stands it that many
times 160 m off. `FX_FLOAT=1` and `game_curve.py show` give the frame as the game shows it.

## Bigger bursts: a column of earth, a flash by its size, a skirt that hangs (after head 2a20087)

Brian, 21 November: "the explosions should be bigger and more more realistic as well as the fire". The art director's
ruling (`art-director/explosions/smoke-burst-fire-ruling-r2.md`, section 2): more bulk, a denser core and a longer
hang, never a ball of soft smoke lifting off the ground and never a film fireball. Measured at the head before, a
105's dense cloud was 8 m wide at two seconds and had left the ground.

A shell or bomb on land now has (`burst.bulk`, `Battle::Bulk`; a hit in the sea is as it was):

- **A column of the ground's own dark earth** standing on the hit (`burst.bulk.column`): many puffs at every height
  from its foot, opaque through its first two seconds. A share of them (`hang`) hang on with the smoke, the likelier
  the lower (`linger.shell.stay_low`), so the base holds the ground and thins last; the rest are earth and are down
  by `life_thick` of the size's thick time (never under `life_least_s`).
- **Lighter grey-brown smoke and dust round it** (`burst.bulk.fringe`), beginning at the column's flank
  (`from_column`) and going from the column's dark and thick to light and thin by degrees (`near_alpha`, `near_dark`,
  `near_heart`), widest at the ground. `reach` is set so the cloud MEASURES `sizes.<size>.cloud_m` across at two
  seconds: where a quarter of what is behind it is hidden, above the skirt. A size may have its own `cloud_reach`,
  `cloud_many` (that many times the puffs) and `cloud_puff` (its smoke puffs that many times as big: a bomb's cloud
  has so many, each so small a share of it, that its edge was a field of spots).
- **A flash by its size** (`sizes.<size>.flash_k`, `flash_s`) and what it leaves low in the earth: a dull orange glow
  no wider than a third of the flash, dark within 0.15 to 0.3 s (`burst.flash.ember`).
- **A ground skirt of dust** rolling out to 1.5 times the cloud's width (`burst.shock_ring.of_cloud`) that stays low
  and hangs with the cloud (`skirt.hangs`; `hang_keep` of its pieces stay).
- **Jets of earth that end as dust** (`burst.crown.to_dust`, `jet_hold`), clods back on the ground within
  `sizes.<size>.clods_back_s`, a few trailing a thin line of dust (`burst.debris.dust_trails`).
- **A throw that is earth, not a black shape** (`burst.throw`; the art director's mends of 22 November, below).

| Size | Flash, m and s | Jets, m | Cloud at 2 s, m (`cloud_m`) | Measured at 2 s |
|---|---|---|---|---|
| grenade | 1.5, 0.05 | 3 to 4 | 5 | 5.0 to 5.3 (core 1.0 at half a second, 0.70 to 0.85 at 2 s, past its thick time) |
| light | 2.2, 0.05 | 5 to 7 | 8 | 8.2, core 0.76 to 0.90 |
| mortar | 3.0, 0.06 | 10 to 12 | 12 | 11.8 to 13.8, core 0.96 to 0.97 |
| 75 mm | 4.5, 0.08 | 15 to 19 | 20 | 16.4 to 19.7, core 0.86 to 0.99 |
| 105 mm | 5.9, 0.10 | 20 to 25 | 30 | 25.8 to 31.5, core 0.89 to 0.98, skirt 37 to 41 |
| 150 mm | 8.0, 0.12 | 28 to 35 | 38 | 33.3 to 43.9, core 0.97 to 0.98 |
| 250 lb | 10.9, 0.13 | 45 to 60 | 48 | 43.9 to 50.5, core 0.99 |
| 500 lb | 13.9, 0.15 | 60 to 80 | 60 | 58.8 to 63.1, core 0.98 to 0.99 |

(Measured over three throws of the dice each, `FX_SEED` 1 to 3, on the head as committed. A cloud's width moves by
a fifth from throw to throw: measure over several, never one.)

A 105 on other grounds at two seconds: sand 29 m and core 0.93, town 27 and 0.99, snow 29 and 0.98, mud 27 and
0.97, woods 29 and 0.96; the skirt 40 to 43 m on all.

**The art director's three mends** (22 November, on the first sheets: "a black comic-book splat with an orange
dot", "cardboard slabs", and a column lit to 105 against the ruling's 70 to 90):

- `burst.throw`: what is thrown at the flash is dark earth brown, lit by the flash. `body` is the WRITTEN value a
  ground's heavy earth is raised to in the jets, never by more than `lighter` times (sand and rubble, lighter
  already, are left alone); `mottle` makes one piece that much lighter or darker than the next, by its own shape's
  number and no dice; `floor` is a colour no thrown piece is drawn darker than (written 69,62,55 lands about
  40,33,26: "floor about 15" of 100); `flash_lit` lights the faces toward the flash with `flash_colour` for
  `flash_lit_s`, falling off over `flash_lit_reach` flash widths. A recipe's colours are WRITTEN values and the
  game's curve darkens the darks: `python3 game_curve.py write R G B` says what to write for a colour to land.
- `burst.crown`: more heavy jets, each thinner (`heavy_jets`, `heavy_thick`), their pieces drawn long and stepped
  as far apart (`heavy_stretch`), widening little (`heavy_swell`) and less toward the tip (`tip_swell`), the tip's
  pieces soft and short-lived (`tip_heart`, `tip_alpha`, `tip_inner`, `tip_life_s`, `heart_end`) and falling faster
  than the root's (`heavy_droop`), so the tips are back down by about a second and a half. All on the dice the jets
  threw before. The thin jets are paid for in pieces: fewer clumps on a jet's side, fewer clods, spikes, fill jets
  and foot lumps.
- `burst.bulk.fringe.shade` and `light`, `burst.bulk.column.light`: the cloud's own colour and how far its sunlit
  side is lifted, so the column's lit top sits inside 70 to 90.

Measured (a 105 on earth, six throws, in the game's curve, 0 to 255): the lit top 75 to 85 at two seconds (its
brightest 3%: 79 to 88) and 80 to 88 at four; the shade 42 to 47; the throw's darkest quarter 66 to 71 at 0.03 s,
53 to 63 at 0.07 s, 46 to 51 at 0.13 s, 42 to 48 at 0.27 s (it was 13 to 18). `own_colour.py` measures them: draw
the same frame on black and on white (`FX_BACK=0,0,0` and `FX_BACK=255,255,255` with `FX_FLOAT=1`), and it finds
what the cloud hides and its own colour where it hides nine tenths: `python3 own_colour.py on_black.f32
on_white.f32 W H [own.png]`.

The slow fade (the section above) has its own smoke-left numbers for these clouds, `linger.shell.by.<size>.
bulk_left_third` and `bulk_left_two_thirds`, set in pictures as the row's were: many thick puffs one behind another
hold their look far longer than a thin cloud's. Read in the picture at a third, two thirds and four fifths of the
tail, the mean of four throws: grenade 0.46 / 0.12 / 0.03, light 0.53 / 0.05 / 0.02, mortar 0.53 / 0.11 / 0.03,
75 mm 0.55 / 0.10 / 0.03, 105 mm 0.49 / 0.10 / 0.03, 150 mm 0.48 / 0.10 / 0.03, 250 lb 0.51 / 0.11 / 0.03, 500 lb
0.53 / 0.11 / 0.03 (the bombs over their true tails, to the later of `gone_s` and the burst's own `linger_s`). One
throw reads a tenth either side of its size's mean, so set these on several. The two-thirds number is never taken
above the third's, which is why the light shell's reads low.

Pieces (1,600 px wide, held at most; head 2a20087 in brackets): one 105 1,104 (1,034); 105s every 3 s 2,374
(2,497); 105s one a second 4,059 (4,118); 75s one a second 3,456 (3,270); mortars two a second 4,339 (4,046); a
stick of twelve 500 lb 15,053 (18,401). Guns, flak and aircraft are untouched: eight tank guns 639, flak six a
second 1,601, aircraft explosions one every 4 s 1,871.

Remove `burst.bulk.column.puffs` and every burst is the one before; each other part falls back alone the same way
(`sizes.<size>.cloud_m`, `flash_k`, `burst.flash.ember.life_s`, `burst.shock_ring.of_cloud`, `skirt.hangs`,
`burst.crown.to_dust`, `burst.debris.dust_trails`, the `burst.throw` row, each `burst.crown` key of the mends).
With head 2a20087's recipe this head draws every shell size on every ground, and the eight same-picture sets,
exactly as that head did.

## An aircraft's trail near the camera (after head 877e898)

Art director, 22 November, on the air scene's ride and chase cameras (build 2b177386): a trail passing the lens was
a wide soft grey band across the frame or a black ribbon, and a strip through the camera's own plane a hard-edged
sheet. "The fix is a near fade: smoke and trail opacity goes to zero within about 30 m of the camera, ramping from
about 60 m." Passed at 04:43Z with the size rule below kept and a nearer ramp asked for, "20 to 45 m", so that the
60 m rear quarter film shot's fire trail does not thin toward its corner.

`game.trail_near_eye_fade_m` (20, 38): by its distance, nothing of an aircraft's smoke trail within the first of
these of the eye and all of it from the second on, fading smoothly between. It goes by how far the trail's line is
from the eye, each end of a strip by its own distance, so a strip fades along its length and two pieces agree at the
edge they share. The second number is 38 and not 45 because the quarter shot's trail passes 39 m from its camera: at
45 that shot still thinned by up to 54 of 255, at 38 it is as it was (below).

By the distance alone an aircraft the camera rides with would show no smoke at all: its engine is 12 to 25 m from
the lens. So two more numbers:

- `game.trail_near_eye_looms` (0.06, 0.2): nearer than 38 m a trail still shows while it is small in the picture.
  All of it while its half width is under 0.06 of its distance from the eye, none of it from 0.2. A thin young trail
  at the ridden aircraft's engine shows, and goes as it widens in the wake and comes back toward the lens; another
  aircraft's wide old trail goes as it comes up to the lens. Without the key the distance alone decides.
- `game.trail_near_eye_least_m` (5, 10): however small, none of a trail within 5 m of the eye, and no more than the
  distance leaves until 10 m. Nothing is drawn at the lens or through the eye's own plane.

Flames are lights and stay, so a ridden aircraft on fire always shows its fire. A ship's funnel smoke, a burst's
smoke and gun smoke go by `game.near_eye_fade_m` as before. Nothing is laid differently and no dice are thrown: a
piece inside the fade is simply not drawn, so the counts can only fall. Without `game.trail_near_eye_fade_m` every
trail draws as it did.

The preview's `pass` shot shows it: `FX_TRAIL=oil FX_SPEED=80 FX_PASS_OFF=3 fxp rec.txt out air hmg 60,120,180
640x360 pass` hangs the eye still beside the path, `FX_PASS_OFF` metres off it, where the aircraft was at
`FX_PASS_AT` seconds, looking after it, so its trail runs past the lens; `FX_PASS_RIDE=1` rides `FX_PASS_BACK`
metres behind the aircraft instead. Seen so: at 3 m off the band and the sheets are gone; at 15 m off the trail is
whole from 38 m out and thins to nothing as it comes toward the eye; ridden from 22 m a burning aircraft keeps black
smoke round it that thins toward the lens, and from 14 m it shows its flames and a trace of smoke.

Proofs: head 877e898's recipe draws its eight same-picture sets (377 frames) exactly as that head did. With this
recipe 375 of them are the same and two change, both the `quarter` shot from 60 m at 2.5 s: the fire trail by at
most 4 of 255 in 1,569 of its 57,600 pixels, the streak by at most 1 in 53. 527 runs with broken values in the six
numbers and with odd eyes (on the trail, behind it, at a place that is not a number), built with
float-divide-by-zero: none bad. No warnings (g++ and clang `-Wshadow`, clang `-Wunreachable-code-aggressive`).

## Trails that thin, pale and go: the Bremen mend (after head f7bf0b2)

Art director, 22 November, mend 2 on the air scene's Mac frames (a1acb552): at Bremen the trails were ropes, a core of
78 against a sky of 226, one width all along, crossing the whole frame. Wanted: 1-2 m at the source, 6-12 m by 10 s;
opacity 0.9, 0.3 by 15 s and gone by 30 s; colour lifting from about 45 to 100-120 with age; edges breaking into
puffs; no trail longer than about 2 km; an engine hit grey-brown, only burning fuel black and only near the aircraft.

- **The rows** `air_trails.smoke`, `fire`, `fuel` and `oil` (the kinds the flight core passes are unchanged). Each
  widens to `r1_m` by `widen_by_s` 10 and on by `thin_grow` after; pales to `pale_to` 0.35 from 2 to 15 s; fades from
  `thin_hold` 0.5 of `life_s` 30; cools to a light grey `colour_old` (smoke: grey-brown from the start, never black);
  draws into lumps only from `lumps_from` 0.8 of its life, so its old end never breaks into a row of beads; and its
  edges are torn (`soft`, `edge_torn`, `wisps`). Fire and fuel heavier, below. The streak stays as it was, with the length rule below.
- **`longest_m`** (2400): the most sky a trail is laid over in its life. A faster aircraft's puffs live that much
  shorter, and its whole age curve (widening, paling, thinning, lumps and knots) is squeezed by the same share, so it
  is the same trail, only shorter: at 80 m/s as above, at 150 m/s gone by 16 s. Without the key, as before.
- **Measured** side-on at 80 m/s over flat black and white (`FX_FLAT_BACK`, `FX_WAKE_UP=0`, the wake shot at 31 s):

  | Age | Engine hit (smoke): opacity, colour, width at half | Fire: opacity, colour, width at half |
  |---|---|---|
  | 0.5 s | 0.94, 79, 1.3 m | 0.95, 39, 4.7 m |
  | 5 s | 0.88, 93, 5.4 m | 0.96, 63, 11.4 m |
  | 10 s | 0.51, 109, 8.7 m | 0.74, 87, 13.4 m |
  | 15 s | 0.33, 112, 12.7 m | 0.41, 101, 15.4 m |
  | 20 s | 0.18, 127 | 0.23, 109 |
  | 25 s | 0.06, 135 | 0.08, 125 |
  | 30 s | none | none |

- **Fire out-reads a hit** (art director 08:54Z: the engine hit passed; a burning aircraft must out-read a damaged
  one). Fire and fuel are one step heavier: `r1_m` 7 and 7.5, `thin_grow` 1.5, `pale_to` 0.56 from 5 to 15 s,
  `thin_hold` 0.4, and the wake past the tail kept at 5 to 6 m (`tail_wide_m`, her ruling of 20 November; 4.0 to
  5.4 m measured at 0.2 s). "1-2 m at the source" is the engine hit's.

## The kit at scale: the mesh in sections, and its Draw on a worker (FX1, FX2, after head 13388a4)

The production lead's battle-scale plan (/mnt/project-files/production/battle-scale-1122/plan.md), items FX1 and FX2.

- **FX1, the mesh made once.** The kit's room (3,000 puffs) is made at the first frame with something to draw as
  sections of 300 puffs, and never rebuilt. Each frame sends only the sections that hold puffs, points and colours
  only; a section that empties is folded flat once and hidden (`SetMeshSectionVisible`). The kit lays its puffs far
  to near and section k holds puffs k x 300 on, so the far-to-near order holds across sections.
- **Readouts.** `FBattleEffects::PuffsDrawn()` and `PuffsAsked()`: the last frame's puffs drawn and room asked.
  `-TheaterFxRoomLog`: a line every 10 s of the kit's clock (Tick's Now): the most drawn, when, the room asked then,
  the room and the sections in use.
- **FX2, old pieces go cheaply.** A piece (`Bit`) is about 1.8 kB. Prune used to close each gap by moving every piece
  after it; now the last piece fills each gap. Two pieces equally deep are drawn in the order they were made
  (`Bit::Serial`, numbered at the Draw after they are made), so the picture does not depend on where they lie: the
  96 old frames byte for byte, and 23 of 24 heavy frames (6 pixels one level apart in the raid shot, an exact tie).
  A barrage of 6 shells a second seen from a hill (16,500 asked of the room): Draw 6.8 ms against 7.3 (median of 5).
- **FX2, Draw on a worker.** `TickBegin(...)` takes what `Tick` takes, does on the game thread what must be done there
  (the test barrage, the camera's jolt, the view), starts the kit's Draw on a worker (`UE::Tasks`) and returns the
  jolt. `TickEnd()` waits for it, sends the mesh and lights the lamps, then does what was asked of the kit in
  between, in order. Between the two:
  - every call on the kit is held, its arguments copied, and done at `TickEnd` with this frame's time, as a call
    made after a `Tick`: first seen at the next frame. So call `TickBegin` after the frame's shells and fires where
    you can, and only what comes after (a vehicle's dust) waits;
  - `GroundUnder` and `GroundColour` are called from the worker: nothing may change what they read (the field's
    heights, craters, ruts) until `TickEnd`;
  - `PuffsDrawn`/`PuffsAsked` give the last frame's; `Clear` and `End` wait for the worker first; a `TickBegin`
    with no `TickEnd` is ended by the next `TickBegin` or `Tick`.
  `Tick` is both at once on the game thread, as before (the air and sea scenes). The kit keeps no shared state: two
  kits drawing at once on two threads ran clean under the thread sanitiser and drew what each drew alone.
- **The box of a moving battle** (the pre-check on FX1). A hidden section's box still counts in the mesh's bounds,
  and the translucent sort goes by the bounds' middle, so sections folded where the battle was stretched the box as
  the battle moved on (a bomber stream most). When the farthest puff is more than 500 m from where the idle sections
  were folded, they are sent once more, folded onto it and still hidden: one send of the idle sections each 500 m.

## A trail that speeds up keeps no hole (after head 9cc20a7)

The pre-check on 13388a4f: an aircraft that speeds up from 80 to 220 m/s squeezes its row's life (`longest_m`), so
the first piece laid fast ended before the slow pieces laid just behind it: a hole in the trail, kilometres wide,
for over 10 s. Now each piece of a trail (and of its kind: a kind change keeps its own rows) laid before the squeeze
rose is cut to end with that first fast piece, never later than its own end, and fades out over the last
`game.trail_cut_fade_s` (3 s) before it. The trail dies from its old end, and lives no longer than it did.

Measured with a probe on the kit (80 m/s for 10 s, up to 220 over 8 s or 2 s, held to 30 s, back down), the widest
stretch with no living piece:

| Trail | Before (8 s / 2 s) | Now (8 s / 2 s) | Longest now |
|---|---|---|---|
| smoke | 3,644 m / 4,032 m | 73 m / 72 m | 4,369 m / 3,484 m |
| fire, fuel | 3,600 m / 4,008 m | 65 m / 65 m | 4,375 m / 3,489 m |
| oil | 3,616 m / 4,000 m | 72 m / 73 m | 4,370 m / 3,485 m |
| streak, doused | 2,080 m / 2,352 m | 72 m / 72 m | 3,818 m / 3,421 m |
| glycol, mist | no hole | unchanged | |

About 70 m is the spacing of the pieces themselves. Longest: the slow stretch laid before the speed-up lives on
until the first fast piece goes, then fades (it was 5,200 m before, with the hole). At a steady speed nothing is
cut: the 96 old frames and the 24 heavy frames are byte for byte as before.

The cut is now laid in the pass the kit already makes over every piece each frame (Prune), not in a pass of its
own: CutTrails only sorts the frame's cuts and mends the trails' own records, and each piece of a cut trail looks
its cut up as Prune walks past it (Battle::CutOf, a search of the sorted cuts). The pre-check's cost note on head
5833912: the own pass walked all the pieces again in any frame with a cut, and with many trails speeding up and
slowing, that was most frames. With 300 trails swinging between slow and fast (60,000 pieces), five runs each,
taken in turn: the pictures' fingerprints are the same to the last byte, before and after (and with 300 at a
steady speed), and the draw's middle run is 12.1 ms against 14.0 before, lower in four of the five pairs. The
96 old frames and the 24 heavy frames are again byte for byte as before.

## Far bursts: fewer, bigger pieces (FX3, after head 5833912)

The production lead's FX3 (/mnt/project-files/production/battle-scale-1122/plan.md): on the ground scene's whole-army
wide shot the bursts asked for six times the room, and the room leaves out the farthest first, so the far half of
the front showed only dots. Recipe row `burst.far` (no row: as before, byte for byte):

- **The share kept** (Shell, a hit on land outside a sea battle). FarK: the crown's height in the picture (`crown_m`
  at the scene's own window pixels) against `full_px` 110, all of it at that or taller, never under `least` 0.08. Times
  BusyK: with more than `halve_past` 1,500 pieces asked for on the screen a half, with the room four fifths full a
  quarter, with more asked than the room `over_busy` 0.125; a hit whole in the picture only `near_busy` 0.5, and only
  with the room four fifths full; between, by how small it stands. Whole at 1,080 px wide: a grenade within 33 m, a
  mortar 104 m, a 75 mm 160 m, a 105 mm 212 m, a 150 mm 297 m, a 500 lb bomb 660 m.
- **What it keeps.** The flash always. The crown keeps every heavy jet, each of fewer pieces up to three times as
  long; the square root of the share of the dust between the jets, the body and the foot (each bigger); the share of
  the spikes, clumps and thrown clods. The skirt keeps the share to `skirt_pow` 0.75 and the smoke to `smoke_pow` 0.75,
  each piece as much wider, so a cloud seen after a cut to a nearer eye is softer but whole.
- **Too small to see.** A piece of a land hit past `gone_beyond_m` 300 that stands under `gone_px` 2 across at its
  biggest takes no place and is not laid; so is a crater's mark. The marks are laid first and stay all battle, up to
  2,400 of them: at 45 s of the wide shot they held a third of the room. Thrown dirt is left alone unless `gone_dirt` 1.
- **Readout.** `-TheaterFxRoomLog` adds the land hits so far, how many kept fewer pieces, and the mean share kept.

Measured with a probe on the kit (the wide shot: 1,080 x 670, a 1.5 km front 500 m deep, the eye 810 m back and
470 m up; shells of 60 mm to 250 lb mixed; the kit's crater marks off, as the ground scene has them; how many 0.3-8 s
old bursts change at least 6 pixels of the picture at 15, 30, 45 and 59 s; with `alpha_pow`, below):

| Wide shot | Room asked (mean) | Pieces held | Draw (median of 3) | Bursts that show |
|---|---|---|---|---|
| 6 shells a second, before | 18,436 | 19,530 | 7.9 ms | 35 of 101 |
| 6 shells a second, now | 3,161 | 3,767 | 3.5 ms | 96 of 101 |
| 7.6 shells a second (Prokhorovka's busiest), before | 22,941 | 24,286 | 10.9 ms | 37 of 153 |
| 7.6 shells a second, now | 3,704 | 4,434 | 4.7 ms | 147 of 153 |

(First measured with the kit's marks on, which the ground scene never lays: 25 of 74 before and 70 of 74 after. The
marks were never what emptied the far half; the bursts' own pieces asked for six times the room.)

From a hill 350 m off at 6 a second (1,600 px wide): Draw 5.7 ms before and 4.0 now, asked at the end 15,811 and 4,643. A
single burst at the preview's own distance draws as before, byte for byte; the 96 old frames and 24 heavy frames
(the raid, the fleet, the wrecks) too, with the row and without it. Pictures: battle-fx/stills/fx3/.

## Dark smoke keeps its hue: sRGB bytes and M_BattlePuffs (after head c86e3ec)

The art director passed FX3 at 12:33Z 22 November with one fix: at the medium shot a big far piece read olive over
grass (55,59,37, ground 87,86,70), and the share of dark smoke with green above red went from 7% to 15%. The cause was
not the grass alone. The kit writes each colour into a byte a channel, as light, `picture.lit_gain` (4) times darker
so its hottest flames can burn past white. A charcoal or dark earth smoke is then one to four steps a channel, and its
red, green and blue round apart: a dark brown of 2.3, 2.0 and 1.5 steps goes in as 2, 2, 1, which is olive. Many small
pieces each rounded a little differently and the errors evened out; one big piece shows its own plainly. The same
rounding is the dusk specks (red and blue blotches in near-black smoke).

- **The cure.** The kit can write its colours raised to 1/2.2 first (`Battle::SetSrgbBytes(true)`), and the new
  material `M_BattlePuffs` (Tools/build_content.py) raises them back to 2.2 before its Brightness, as M_MapSurface and
  M_LandLeaves read theirs. A dark smoke then has 30 to 50 steps a channel instead of 1 to 4. `FBattleEffects::Begin`
  uses M_BattlePuffs when it is there and says so in its ready line ("colours as sRGB bytes"); without it (the Mac's
  content not built since), M_Highlight and plain bytes, as before. **On the Mac: run Tools/build_content.sh once**;
  it builds M_BattlePuffs when it is missing and leaves everything else alone.
- **Thicker far pieces.** `burst.far.alpha_pow` 2 and `alpha_most` 0.95: a see-through piece of a thinned land hit
  (skirt, smoke, the cloud's lighter fringe) laid Bigger times as wide is as thick as Bigger to alpha_pow of them laid
  one over another, never over alpha_most, so less grass shows through the fewer layers. Sea hits are untouched.
- **The preview.** `FX_BYTES=srgb` draws as the game will with M_BattlePuffs; unset, plain bytes as before, so every
  older check holds. Byte checks need clang builds: the 96 old frames were made with clang, and g++ rounds a few
  sums differently.

Measured on the medium shot (150 m back, the kit's crater marks off as the ground scene has them; frames at 20 and 30 s):

| | Dark smoke with green above red (20 s / 30 s) | The big low piece | Dark smoke area at 30 s |
|---|---|---|---|
| Before FX3 | 3.0% / 12.8% | 77,68,60 (many small pieces) | 50,753 px |
| FX3 as passed | 2.4% / 22.7% | 54,59,37 | 35,702 px |
| FX3 with the cure | 2.4% / 14.8% | 58,57,43 | 36,908 px |

At dusk, over the burning wreck (960 x 540, frame 450), dark pixels with red six or more above green fell from 10,371 to
2,697, and with blue above red from 852 to 560; the smoke reads as its own brown-grey, not grey with purple blotches.
By day every older test frame moves by 1.7 levels on average where it moves at all (92 of 96 frames, mostly the smoke's
blue up a level or two, at most 6 to 8 levels; 21 at the darkest heart of a 75's cloud on mud). The cost is a table
look-up a channel: Draw 3.66 ms against 3.70 on the wide shot (median of 3). Pictures:
battle-fx/stills/fx3-colour/ (medium-colour-fix.png, dusk-specks.png).

## A rifle's round off armour: its flash and its sparks (FX4, after head f027cdf)

The ground battle sends a glance for every rifle and machine-gun round that strikes a tank's plate and does not go in,
at 8 mm: 1,541 of them in the Normandy battle. Each laid a shell's whole ricochet: a lamp, a puff of paint, and a
glowing shot followed off for about a second, some 470 pieces of its 500. With glance.shot_from_mm at 20, a glance
from anything smaller is its two-frame ragged flash and its sparks only. Its sparks are as before for its calibre
(read at 20 mm at the least: 7 to 13, about half a 75's in count, speed and size). It takes no lamp and no place
among the shots followed (most_shots, most_a_frame), so a tank's own gun glancing nearby is still followed. A
calibre that is no number counts as a shell's, and without the key every glance is as before. Release 2: with the
ground battle's ricochets on (the default; -TheaterLandNoRicochet off), a rifle's or machine gun's round on armour
sends no glance. Its burst goes off her plate instead (`ShotsOnArmour`, "A burst off armour: ricochets" below). A
towed gun's shield still glances.

| | One glance | Ten a second along a tank's side, 30 s | Standing at the most |
|---|---|---|---|
| 8 mm before | 507 pieces, a lamp, a shot followed | 123,917 pieces | 3,023 |
| 8 mm now (and 12.7 mm) | 32 pieces | 10,562 pieces | 95 |
| 20 mm and 75 mm | as before | as before | as before |

## A burst off armour: ricochets (release 2, after head 388ba015)

Brian, 25 November 15:58Z: "are bullets taking out german tanks? cannon fire or bazookas should be the only thing
taking them out ... bullets should be bouncing off it". The rules thread settles what a round does. This is how it
looks: the war film's machine gun raking a tank. `ShotsOnArmour(Now, Gun, From, Plate, Weapon, Rounds, PerSecond,
Ground)` is the same burst as `Shots` (its flicker, its brass, its tracers by the gun's own count). Its rounds strike
her `Battle::ArmourPlate`: At, the aim on her plate, and Normal, its outward way; RoomMid, RoomA and RoomB, how far
the plate runs flat about the aim; Carry, Pivot and Spin, how she drives and turns; Floor, her own height under the
plate. The row is `bullet_ricochet`, and every key is read with its default:

- Each round strikes about the aim, spread over the plate (the burst's own spread, held to `spread_m` across and up
  it). It lands inside the plate's room, `edge_m` (3 cm) short of its edges, so none is drawn in the air past her
  edge, on a curve that leaves the plate, or inside her. With no room, every round strikes at the aim. Each strikes
  where her plate is at that moment, carried and turned as she drives and turns. Its flash, sparks and paint ride with
  her, so none is left in her or hangs behind her. It never kicks up a dust spurt.
- Each strike has a two-frame near-white flash (`flash_m`, `flash_s`, `flash_over` 2.0) in a faint warm halo (`halo_m`
  0.2, `halo_alpha` 0.35). The flash is never under `flash_least_px` (3), so at 150 to 600 m by day each strike still
  twinkles white.
- Each strike throws 3 to 8 sparks (`sparks`) in a cone about the mirror of its way in (`cone_deg`), opened more the
  squarer it struck (`square_open_deg`), never nearer the plate than `least_off_deg`. Their speed is skewed slow
  (`spark_mps`, `spark_uneven`): most are short and a few long. They burn `spark_over` past white, fall, cool
  white-yellow to orange (`spark_old` 255, 180, 80) by their end, and are gone in `spark_life_s` (under 0.3 s). Each
  is laid as its own streak along the way it is seen to go (its flight plus her drive), with its tail on the plate.
  - A spark that would fall back onto a roof, a glacis, her floor (her deck under her turret's side, her track guard
    under her hull's side) or into the ground goes out first.
  - One whose streak would reach her floor at its birth is thrown as shallowly up instead, or drawn shorter.
- Now and then (`chip_chance`) a strike leaves a faint grey puff of paint (no wind: the wind would carry it into the
  hull).
- A tracer that strikes is drawn to the plate and never past it. `tracer_chance` of them go off again as a ricochet:
  - It leaves `tracer.ricochet.deflect_deg` off the plate the way the round was going, up to `tracer_yaw_deg` aside,
    at `tracer.ricochet.slower`, falling (`tracer_drop`).
  - It tumbles: `kinks` kinks of `kink_deg`, `kink_slow` slower at each. It is gone in `tracer_life_s`, and is drawn
    at `tracer_alpha` (0.7), dimmer than the tracer coming in.
  - A way headed down the plate is turned up it. A way headed down to the ground is turned as shallowly up, as a skip
    off the ground would be. A ricochet is drawn only to where it meets the ground.
- Far off, where a metre on the plate is under `full_px` on the screen, it throws that share of sparks (never under
  `far_least`, never under one a round). Each spark is the root of it bigger and drawn out to `spark_long_px` (never
  a bead). On a busy screen (the room over `halve_past` full) it throws half as many, and a quarter past four fifths
  full.
- It has its own dice: `Shots` into the ground throws exactly what it did before, and the kit's every other output is
  byte-identical (the before/after below).

The game: LandPlay hands a rifle's or machine gun's burst at armour that the rules do not call a miss (glance, worn
or kill) to `ShotsOnArmour`. The plate comes from the first of these that answers:
1. Her drawn skin (`World::Plate`: LandKit's `Vehicle::PlateNow`). It gives the strike (where the scar's cast passes
   through a road wheel, a sprocket, an idler or a track before her hull, the round strikes that), the room and her
   floor.
   - The room comes from a 33 by 33 lattice 3 cm apart about the strike. A point is on it where a round coming in its
     way, and a ray square into it, each meet her first within 3 cm of the plane, on a face turned within 25 degrees
     of it, with nothing of her standing in front within the point's square (3 m out along the round's way).
   - The room is the biggest rectangle of such points about the strike (`Vehicle::PlateAbout`).
2. Her skin's strike alone (`World::Skin`), with every round at its point.
3. A plain box on the face the rules name.

LandPlay fits her drive and turn over the burst from her track (least squares). It hands the kit the plate as it will
be when the burst lands, carried back to the frame the burst is laid. Armour (`LandPlay::ArmouredVehicle`) is a tank,
an assault gun or a tank destroyer, an armoured car, a half-track (the Sd.Kfz. 251, the M3, the flak half-tracks and
the M16) or a battery's self-propelled gun. A lorry, a jeep, a field car, a motorcycle and a towed gun are as before.
It is on by default. `-TheaterLandNoRicochet` gives the game as before, and the log says which.

Test: `preview/ricochet_test.cpp` (kit only). A machine gun 120 m off fires 25 rounds at 15 a second at a stand-in
tank:
- into her side, 45 degrees off square, with the eye 30, 150 and 600 m off, and once from a rise into her turret's roof;
- at her front plate 3 cm under its top edge, 0.8 m to either side;
- at her side 0.1 m under its top edge;
- at her turret's foot 5 cm over her deck, and her turret's side 0.42 m over it, with the deck handed over as her floor;
- at her side low over her tracks, 0.9 m up;
- at her front as she drives at the gun at 5 m/s, backs away from it at 5 m/s, and pivots at 0.5 rad/s.

All checks pass on g++ and clang++, and the two lay the same pieces to the digit (one digest of every piece laid). In
every case all 25 rounds strike and flash, and:
- every flash is on her face, inside its room less `edge_m`;
- the farthest strike is 0.0000 m off where her plate is at its moment;
- there are 0 dust spurts and 0 dust by her;
- nothing is inside her hull or under the ground.

| case | sparks a round | ricochets of 5 tracers | what else |
|---|---|---|---|
| side, 30 m | 5.2 | 4, 24 to 45 degrees off the plate | |
| side, 150 m | 2.2 | 5, 19 to 49 degrees | |
| side, 600 m | 1.1 | 5, 24 to 41 degrees | |
| roof, 30 m | 6.0 | 4, 13 to 43 degrees | |
| front, 3 cm under its top edge, 0.8 m to one side, then the other | 5.4, 5.5 | 4 and 3, 27 to 48 degrees | |
| side, 0.1 m under its top edge | 5.2 | 4, 24 to 45 degrees | |
| turret's foot, 5 cm over her deck | 5.9 | 3, 13 to 29 degrees | |
| turret's side, 0.42 m over her deck | 5.9 | 3, 13 to 29 degrees | handed no floor, 15 pieces go through her deck |
| side low, 0.9 m up | 5.2 | 4, 24 to 45 degrees | as `Shots` into the same place, 10 dust spurts |
| front, driving at the gun, 5 m/s | 5.4 | 3, 34 to 43 degrees | her plate moved 8.7 m while it drew |
| front, backing away, 5 m/s | 5.2 | 4, 22 to 49 degrees | 8.9 m |
| front, pivoting, 0.5 rad/s | 5.6 | 3, 17 to 42 degrees | 2.1 m |

Every incoming tracer ends on the plate. No spark starts off the plate's face, is thrown into it, lives 0.3 s, or is
drawn shorter than 1.5 times its width (the shortest is 3.9 px, backing away). A ray from a front strike to its spark
may cross the gun barrel standing out in front of her; the test counts those apart (0 to 4 a case). Each check fails
with its mend taken out:
- with no dust counter, the low side's spurts show;
- with no floor, seven cases show pieces in her hull;
- with no carry, the moving cases' strikes are left behind;
- with no room, the edge cases strike past the edge.

Her real plates are checked in `Tools/land_kit` ("SMALL ARMS OFF ARMOUR"). The Tiger's and the Panzer IV's plates,
at rest and trained on a slope, take rounds over a 7 by 7 grid of each face, and from inside her, behind her running
gear. 81 places over each room are tried:

| | plates | with no room | room on average | every place on her (5 cm) | nothing of her in front (5 cm) | a plate costs |
|---|---|---|---|---|---|---|
| Tiger | 350 | 23 | 0.33 by 0.16 m | worst 2.8 cm | worst 2.8 cm | 0.8 ms (5 ms at most) |
| Panzer IV | 359 | 47 | 0.31 by 0.26 m | worst 3.0 cm | worst 2.2 cm | 0.7 ms (5 ms at most) |

The costs are on a shared machine lightly loaded. With it four times over-full they were 1.8 to 3.5 ms on average and
about 30 ms at most. g++ and clang++ give the same plates to the digit, and the rest of the test is as before but for
its timings.

A strike cast behind her running gear is moved onto what stands in front (as far as 0.8 m). Her side aimed 0.1 m
under its top edge is never struck over it. Taking out the lattice's check of what stands in front fails the test (a
part of her 0.58 m over the Panzer IV's deck stands in front of a room on it). A gap narrower than the lattice's 3 cm
may still lie inside a room.

Cost: one burst lays 144 to 261 pieces on the plate (45 to 61 into the ground) and draws at most 9 to 24 puffs in a
frame (5 to 15 into the ground). The call takes about 0.4 to 0.8 ms on a shared machine, against 0.1 to 0.2 ms into
the ground. Most of that is stepping the sparks that could fall back onto a roof, her floor or the ground. Her plate
costs the battle about 0.8 ms more per burst at armour (once a burst, not each frame).

Preview: `fxpreview <recipe> <dir> ricochet <30|150|600> <frames> [WxH]`. The options:
- `FX_RICO_CASE`: side (the default), edge, turret or driving;
- `FX_RICO_BEFORE=1`: the same burst as `Shots` into her middle;
- `FX_DUSK`, `FX_FPS`.

Pictures and notes: battle-fx/ricochet/ in the project files.

## Fire under a scene's grade: the fire's spread (after head 2811047)

The land battle grades its picture for its fields: it takes most of the colour out of the darks and the middles
(0.45 and 0.48 of it) and keeps the brights. A burning wreck's flame is mostly middles, and translucent flame laid
over dark smoke falls into them, so under that grade the flame went salmon and peach (the art director's read of
cut 5; fire median 174,116,89 where the game's own curve gives 221,129,36). No colour the kit writes can mend it:
a deeper orange lands peach all the same, a brighter one goes yellow-white, and the tonemapper takes the grade from a
table read by the scene's colour, which no colour under zero can reach (a spread before it, clamped, gets 205,101,79
at best).

So it is mended after the grade, on the picture as the screen shows it. M_BattlePuffs marks the kit's pixels in the
custom depth (translucent custom depth writes, wherever a piece is a tenth seen or more), and M_FireSpread, a
post-process after tonemapping (Tools/build_content.py), finds the marked pixels the opaque world does not hide and,
of those, only two sorts: fire's colour (hue between orange-red and yellow, and deep colour, or bright and a little
coloured; a red tracer is in it), and a bright bluish-green light (a Soviet tracer, which the grade leaves grey-green,
at about 124 degrees; a field's greens are yellow-green, under 95, and blue and violet are the sky's: all left alone). The whitest are left alone, so a white-hot core stays as ruled. Their colour
is spread from their own grey by Spread (2.0, the art director's) and the green of fire's orange raised by Warm (1.2;
on yellow it went lime, and on a sky seen through smoke cyan, the pre-check's find),
feathered by that "heat" so there is no seam at a flame's edge. Smoke, dust, earth and the field seen through a thin
flame stay as the grade left them. A scene asks for it with FBattleEffects::SetFireSpread(its eye's lens), after
Begin; it costs the custom depth pass for the kit's mesh. Without the materials it says so and does nothing. The mark
cannot say which kind of piece made a pixel (the mesh carries places and colours only), so a tracer is known by being
bright and coloured, not by its kind.

Modelled on the wreck at 14 m through the land battle's grade (land_grade.py here: lands_land is the grade,
fire_spread the same sums as the material's code; the picture is stills/hot-fire/fire-spread-model-2.png):

| | Fire median | Flame tips | Bright core | Rims | Field behind thin flame |
|---|---|---|---|---|---|
| game's own curve (the aim) | 221,129,36 | about 67,26,24 | 254,208,101 | 174,189,203 | |
| ground grade today | 174,116,89 | | 240,210,188 | 164,174,184 | 192,194,168 |
| ground grade + spread | 221,125,51 | 108,55,21 | 255,246,160 | as graded | as graded (not lime) |

The kit's other colours, written as the recipe has them, on the game's curve, through the ground grade, and after:

| | Game's curve | Ground grade | + spread |
|---|---|---|---|
| US and British tracer glow | 231,85,46 | 175,87,78 | 244,83,51 |
| Soviet tracer glow | 119,225,121 | 167,210,170 | 135,222,143 |
| ground tracer (the ruled yellow) | 214,207,103 | 202,197,162 | 204,197,148 |
| white-hot cores (tracer, muzzle root) | as ruled | as ruled | unchanged |
| muzzle flash orange | 234,156,0 | 201,146,102 | 248,164,50 |
| sand dust | 206,193,156 | 190,181,163 | unchanged |
| clod earth (written 118,99,72) | 119,87,45 | 72,59,45 | 77,64,37 |
| grey and pale smoke | | | unchanged |

The mark is every piece a tenth seen or more, smoke and dust too, so the colour alone decides: a field seen through
thin dust whose colour on the screen is deep orange-brown or gold (ripe wheat at 215,180,95 would go 249,179,9) is
spread in the piece's shape. Under the land battle's grade a field lands far paler than that, but if dust over wheat
shows a patch on the Mac, M_BattlePuffs' opacity_mask_clip_value (0.1) is the lever: at 0.3 the wreck's fire is the
same in the model, and thin edges no longer mark.

The ground's yellow tracer is only half brought back: pale khaki and a wheat field seen through thin smoke are the same
colour on the screen, and the field must stay as graded.

## Clods and tracer under the land battle's grade (after head 95d4019)

The land battle's grade is steep: a clod a tenth darker lands ten levels darker. Earth's clod paint, written to show
40,33,26 on the game's own curve, landed 17,13,10 under it, and with its dark face (shade 2.5) and the shower's
backlit undersides (0.55) the clods were flat black silhouettes, core L 9 (the art director's 06 of cut 5); a fast
one, drawn along its flight, was a long dark slab across the men. The scene now says it is under that grade
(FBattleEffects::SetLandGrade, after Begin) and the recipe's land_grade row stands in for what it names
(Battle::Picture, and GradeNum and GradeRange for the clods' shade, tint and the shower's shade). Without the call, or
without the row, nothing changes.

- Clods, written with land_grade.py write: earth 118,103,61 (lands 72,62,42, the art director's lit earth; 118,99,72
  for 70,58,45 leaned rose-brown on the lit faces), mud 110,97,59, snow 125,114,76, woods 138,118,67; sand's stones
  107,93,53. Their shade 0.5 and tint 0.95 to 1.05, the shower's shade 0.9 to 1.0, so a clod holds within about L 40
  to 80 with a lit face and a dark one.
- Soft edges (art director 14:09Z, in every scene): the burst's own dark pieces were hard straight-cut polygons, the
  cut-card look, solid to a rim with straight sides between the ring's 16 points. They keep their dark hearts and fade
  at their very edge: the foot's body to burst.foot.rim 0.1 (0.3 before), a jet root's side clumps to
  burst.crown.side_clump_rim 0 (0.55), and the lumps thrown past the lens to burst.debris.lens.rim 0.1 from
  lens.inner 0.6 (0.6 and 0.5). The burst's mean level is the same to a tenth.
- burst.debris.clods.most_long 3 (in every scene, not only under the grade): a thrown lump that is not a light (clod,
  shower, lens piece, a gun's kicked dirt) is drawn no longer than 3 times its width. Sparks, embers and tracers keep
  their streaks, and the crown's jets their length.
- The tracer: colour 232,208,0 lands 213,206,154 and, with the fire's spread, 220,207,118 (the art director's
  214,207,103; plain it was pale khaki 202,197,162). By day its edge 255,178,0 lands 233,193,110 with the spread. Its
  white-hot core is as it was.

A 105 on earth at 14 m, 1.5 s (stills/clods/clods-ground-grade-2.png, through the land grade): the clods that changed
were median 28,26,21 (L 26), now 67,60,43 (L 60, nine in ten between about 40 and 80); the lit ones 73,66,48, their
edges blended with the smoke behind. With the old recipe all 96 check frames are the same to the byte; with the new
recipe and the grade off only the 500 lb bomb's frames change (the 3:1 and the soft edges), of 24 heavy frames.

## Soft contact: flames meet the hull softly (after head 5dc2e51)

The art director's cut 5, still 07: a burning wreck's flame cards met the hull along a hard straight line. The
mesh carries only places and colours, so the material cannot tell a flame from a case lying in the dirt; the art
director ruled (14:16Z, 22 November) option B: fade every piece where it meets the ground or a hull over 0.1 m, and
keep the lying pieces whole.

- The scene: `FBattleEffects::SetSoftContact(0.1f)` after Begin, on the game thread (the land battle only; the air
  and sea scenes never call it and pay nothing). The kit's mesh takes `M_BattlePuffsSoft`, `M_BattlePuffs` with its
  opacity times `saturate((SceneDepth - PixelDepth) / FadeCm)`. Build it once on the Mac:
  `TC_REBUILD=M_BattlePuffsSoft Tools/build_content.sh` (built anyway when missing). Without it: a log line, and the
  kit draws as before.
- The kit (`Battle::SetSoftContact`): a piece lying on the ground (a thrown clod or fragment come to rest, a crater's
  mark) is laid `game.soft_contact.lie_nearer` (1.5) times that depth nearer the eye along its own line of sight and
  smaller to match: the same picture, every point of it that far in front of what it lies on, from any height of eye.
  A spent case already did this (`ground_cases.nearer_m`); it now takes the larger of the two. A flat piece already
  higher than that (a fire's glow, 0.2 m up) is left where it is; pulled the whole way, a glow by the wreck showed a
  strip more of itself under the hull's foot.
- Why along the line of sight and not straight up: the fade is by depth. Lifted 3 cm straight up, a case seen from
  45 degrees above is 4 cm in front of the ground, and keeps 40% of itself; pulled 15 cm along the line of sight it is
  15 cm in front from any angle and keeps all of it. What changes on the screen: a resting clod shows the bottom few
  centimetres of itself the ground had hidden (about one pixel on a 105's clods at 5 s).
- Preview: `FX_SOFT_CONTACT=0.1` (the kit's call, and the material's fade in the preview's own depth test).
- Measured (960 x 540, the land grade on): the wreck at 10 s, from the side and the battle camera, about 2,000
  pixels change by more than 8 of 255, all of them flame where it meets the turret and the deck; with the lying
  pieces not pulled, a 500 lb bomb's clods at 5 and 10 s faded on 134 and 255 pixels (mean 9 and 6 levels), pulled
  16 and 4 (mean under 1). Draw on the wide heavy shot (6 a second): 3.3 to 3.5 ms either way. The GPU's one depth
  read a pixel is the Mac's to measure.
- Stills: `battle-fx/stills/soft-contact/wreck-soft-contact.png` (now on the left, soft contact on the right).
- Checked first (the art director: only if free): the colour's alpha cannot gate it. A thrown solid's middle is at
  full alpha in 24% of its frames (it fades in and out, and its rim is soft since 5dc2e51), and a flame's in 18 to 42%.

## Bomb carpets far off (after head b0f46c5)

Brian's rule: the numbers on the screen follow the units each side brings, so every bomb of a raid lands where it can
be seen. The air scene's wide view looks at its bomb carpets from 6 to 11 km (the eye about 4.7 km up), and the kit
drew nothing beyond its 4 km draw range; drawn near, one stick of eleven 500 lb bombs is about 13,600 puffs.

```cpp
// a bomber's stick, once, when it lets go (on the AirGun clock): Count bombs from First to Last along the ground
Blasts->BombStick(AirNow, BomberId, FirstWorld, LastWorld, 11, TEXT("bomb500"), TEXT("earth"));
// on the sea or a deck: TEXT("water") or TEXT("town"); OverSeconds (last, -1) when the stick's own time is known
```

Each bomb lands at its own moment along the line (the line's length at `far.bombs.walk_mps`, at most 4 s, unless the
caller gives `OverSeconds`), laid by the next Tick on or after it. Within the draw range it is the ordinary burst,
as `ShellOfSize`. Beyond it, with the recipe's `far` row, it is the far form (`TheaterFxCore.h`, `FarLay`): a flash
held to `flash.least_px` with a soft glow, so a stick ripples along its line; a short dark crown of thrown earth; a
dense dark body of earth standing on the ground; a lighter veil that lifts and spreads; and a low skirt of dust,
each one large soft piece a share of the size's `cloud_m`, in the ground's own colours (the spout's white water, foam
and mist on the sea), never under its `least_px` on the screen. The body, veil and skirt go as the near cloud does
(`linger.shell` and the size's `linger_s`: thick 8 s, then thinning over a minute or more, lifting and paling to the
sky), and look half as thick a third of the way through their tail and a tenth at two thirds (`left_third`,
`left_two_thirds`: how thick a piece looks, the art director's curve, turned into the smoke it must keep). When a
stick's bombs stand closer on the screen than `merge_px` the veil and skirt are laid once for several of them, drawn
out along the stick as its bombs land; every bomb keeps its own flash, crown and body. Far pieces are drawn out to
`far.draw_beyond_m` (20 km) and kept first when the screen is full (`far.keep`).

A bomb laid near carries its far form too, drawn only once its strike point is beyond the range; the two cross over
the last `far.hand_over` of the range (3.6 to 4 km), so a carpet never goes as the eye draws back. A bomb laid far keeps
its far form if the eye comes in. The worst raid asked for, 22 bombers each dropping twelve 500 lb bombs within a
minute, holds at most 929 pieces on earth or town (all 264 bombs within 12 s; 842 spread over 40 s) and 572 on the
sea, of the 3,000 (`game.max_puffs`).

Without the `far` row a stick is its bombs as plain `Shell` calls at their moments and places, frame for frame.

The preview's `raid` shot: `FX_KIT_PX=0.90031 FX_DIST=11000 ./fxpreview recipe.txt out raid earth 120,483,1233
3200x1800` (see the top of fxpreview.cpp: FX_STICKS, FX_BOMBS, FX_BOX_GAP, FX_EYE_H, FX_BACK_MPS for the hand-over,
FX_REF=1 for the same bombs as Shell calls).

## Fuel going up: the fuel ball (head D)

The art director's ruling of 21 November, section 2: a shell or bomb burst is earth and smoke, never a petrol
fireball; rolling fireballs come only from fuel, a vehicle's 6 to 10 m for 1 to 1.5 s, an aircraft's crash or a fuel
dump 15 to 25 m for 1.5 to 2 s, with black smoke from its top after half a second.

- **The call** `FuelBall(Now, At, Across, Velocity)` (the game's `FBattleEffects::FuelBall`). Call it at the moment
  the fuel goes; a fire that burns on after it is the caller's `Burn`, called with it. Velocity is what it was moving
  at (a crashing aeroplane's, a truck's); the fire keeps a little of it. Without the recipe's `fuel_ball` row:
  nothing.
- **What it draws.** Its time comes from its width (`small_s` at `small_m`, `big_s` at `big_m`). Its lobes are thrown
  out and up from a seat just above the ground and slowed by the air. The whole ball climbs `rise_of_across` of its
  width while it burns, faster as it goes, and each lobe rolls as it climbs. The fire is lit from within: white-hot
  knots (`hearts`) for its first moment, then a yellow-orange middle (`heart_in`, its lobes sorted `core_nearer`
  toward the eye so the middle shows from any side) inside orange billows, some deeper toward the outside
  (`deep_mix`). Each holds its colour for `cool_from` of its life, then cools to an orange `ember`, with no dark rim.
  No lobe goes before its smoke can take it, and with `soot_in_s` a lobe goes the moment its smoke is whole, so none
  fades pink over the sky. The dark comes in on top first (`soot_life`): from `smoke_from_s` black smoke boils off
  the top (`top_puffs_per_m`), and lobes lower down turn to smoke later.
- **The cloud after it** (art director 08:08Z 22 November). By `cap_s` the smoke spreads out (`cap_spread`) under a
  flat top (`cap_top`, `cap_flat`, `cap_raise`), about twice the ball across, leaning `lean_deg` down the wind, on a
  dark stem from the ground (`stem_puffs_per_m` up to `stem_s`). It thins as `linger.fuel` says (thick 2.5 s, gone
  by 20 s, four in ten of its puffs retiring early). A lamp of `lamp` times the root of its width lights the ground
  while it burns, rising with it. A camera inside it dims and never goes black (`lens_near_of_across`, the
  `game.lens_cap` backstop); the camera itself keeps twice the ball's width off it.
- **Preview.** `FX_SCRIPT=fuel` on the wreck shot: her fuel goes up at 0.5 s, `FX_FUEL_M` across (8), at the engine
  deck, and she burns on. `FX_SCRIPT=crash`: an aeroplane's fuel, 20 m at the ground, coming in at 60 m/s, with no tank.
  `FX_SCRIPT=dump`: the same standing still. With crash and dump the wreck or the spilt fuel burns on from 1.5 s (a
  `Burn` 0.3 of its width across, as a scene would light it; `FX_GROUND_FIRE=0` for none). For example
  `FX_SCRIPT=crash FX_DIST=110 FX_LOOK_H=14 FX_LOOK_X=0 fxpreview recipe.txt out wreck far 18,36,54 960x540`.

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

## Smoke screens at sea, and shell columns by calibre and dye (after head b0f46c5)

```cpp
// an escort laying smoke: every frame while she lays it, at her stern, with her velocity (world units a second)
Blasts->ScreenLay(ScreenId, SternWorld, ShipVelocity, /*bFunnel*/ false); // true: black from her funnels
Blasts->ScreenLayStop(ScreenId); // she stops laying: what is laid hangs and thins on its own
// a shell falling in the sea, by its calibre in mm and its ship's dye ("pink" Nagato, "red" Kongo, "yellow" Haruna; empty: none)
Blasts->SeaSplash(FallWorld, 406.0f, TEXT("pink"));
```

- **Screens** (`sea_screen`; not the land's `SmokeScreen`, a smoke round's wall, which draws as before). Laid by
  time as a funnel's plume is, into the air she steams through: `white` from her stern generators, `black` from her
  funnels (born `from_up_m` higher and `from_ahead_m` forward). It rests on the water (`rest`), climbs only a little,
  widens as its age to the two thirds (`wide_k`) and thins by halves (`even`), so its old end costs few pieces; it
  fades over minutes (`fade_s`, gone by `age_s`), the black greying to mid grey (`colour_old`). Heaped (`billow`) and
  lumpy, with gaps (`gaps`, `gap_thin`) along it. Far off a puff is never under `least_px` across: fewer, bigger.
  Five screens laying at once from 1 km: about 800 puffs once they are five minutes old (game.max_puffs 3,000).
- **Columns by calibre** (`sea_splash`): the passed white water (`grounds.water.spout`) built to the art director's
  table by calibre (`height_m`, `rise_s`, `hang_s`, `fall_s`, `foam_m`, `foam_s`, `crown_wide`, each between the two
  calibres either side): a 16-inch shell's column about 60 m, up in about 2 s, standing 3.5 s, then down over 5 s,
  its top the last down, and a 35 m foam patch for 25 s; its foot breaks white as it stands (`foot_white`). Dyed
  (`dyes.<name>`: `colour` lit, `shade`, `shift` toward which it reads against the sun or with the sun low, `strength`),
  its crown whiter (`dye_top`) and paling as it falls (`dye_pale`), its foam patch stained (`stain`). Far off
  (`far_px`) its white is written past white (`far_over`) to stand over a pale sky and its foot is never a dark puff
  (`far_foot`). A salvo's spacing is the caller's (0.1 to 0.5 s). Without the row: `ShellOfSize` on water as before.
- The preview: `./fxpreview recipe.txt out seafx screen 300,1800 1600x1000` (`FX_SCREENS`, `FX_BLACK=1|alt`, `FX_DIST`,
  `FX_KNOTS`, `FX_STOP`, `FX_LENS`) and `./fxpreview recipe.txt out seafx splash 45,90,180 1600x1000` (`FX_CAL`, `FX_DYE`,
  `FX_SPLASHES`, `FX_EVERY`, `FX_DIST`; `FX_OLD=bomb500` the call before). Render with `FX_KIT_PX=1.8` as the game window.

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
  It hangs where it burst, drifts with the wind, greys and stretches to 8 s, then thins into the air and is gone by 25 s
  (the recipe's `linger.flak`; about 9 s without it). It is never cut off.
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

## Smoke seen from above: a lit top and its shadow on the ground (game.from_above, OFF)

The art director's read of the general's stills (`art-director/battles/general-cf671cb9/general-stills-read.md`, item
5, 22 November): from the general's height, 300 m and up, smoke was a flat dark smudge. She asked for a lit top, a pale
side toward the sun, and its shadow laid on the ground downwind, and liked the smoke at ground level as it is.

- The scene: `FBattleEffects::SetFromAbove(true)` after Begin (the land battle only), and `game.from_above.on` 1 in
  the recipe. Either missing: nothing changes. Only as the eye climbs from `eye_m` 100 to 220 m over the ground and
  looks down from `steep` 0.4 to 0.7 (the sine of the angle down): a low eye is byte for byte as before.
- Lit: a smoke or dust puff's face toward the sun goes up to `lit` (0.45) of the way to `lit_colour`, most on its own
  sunward side (as flak's FaceLit) and on its cloud's; none against the sun, at night, or where it would darken.
- Black smoke (the art director's 19:59Z mend): smoke whose own colour's light is under `dark_lum` (0.015, going
  over by 0.04) goes only toward `dark_lit_colour` (120,108,95), a warm brown-grey, as black oil smoke never goes pale
  in the sun. Every burning wreck's or oil fire's column counts as black: its top takes `dark_lit` (0.45) toward that
  colour by the sun's height (all of it from 45 degrees up), all of it on its sunward side and a little over half on
  the far side, fading with each puff's edge; its young smoke over the fire takes as little as of the sun's side, so
  the root stays black. In the side-sun general's still the column measures 0.14 of the way there in the middle, 0.22
  at its brightest; the barrage's earth smoke is as before.
- Shadow: one puff in `shadow_every` (2) of the smoke at least `shadow_least_r_m` (1.5) across and `shadow_least_m`
  (3) over the ground, a burning wreck's column too, lays a flat soft piece where the sun's line through it meets the
  ground: `shadow_colour` at `shadow` (0.2) of its own opacity, `shadow_wide` (1.5) as wide, drawn out along the sun's
  way by the sun's height (at most `shadow_stretch_most`). It is the puff itself laid again (`Bit::bShadeTwin`), so it
  drifts, grows and thins as the puff does. The higher its puff the softer and a fifth wider its edge, fully so by
  `shadow_soft_m` (60). Where shadows lie over each other the ground keeps at least 1 - `shadow_darkest` (0.55) of
  its light, kept by patches a little smaller than each size of shadow, so a wide plume never stacks into a dark
  puddle: in the side-sun still the ground under the column keeps 0.83 near the wreck, 0.73 mid-run, 0.63 at the
  widest (it was 0.50). None with the sun under about 9 degrees. Laid with the craters, under all
  the smoke, pulled nearer the eye as a lying piece (soft contact).
- Room: shadows never take smoke's own. They use what the rest left in the last frame, less a twentieth, at most an
  eighth of the room and `shadow_most` (300), eased 25 a frame; when short, the oldest smoke keeps its shadow and the
  last 20 laid fade, so none blinks. At the 300 m bench (12 burning tanks, 6 shells a second, room full) the most
  drawn is the same with it on or off.
- Preview: `FX_FROM_ABOVE=1` with a recipe whose `game.from_above.on` is 1; the wreck shot now reads `FX_SUN` too.
  General's eye: `FX_DIST=150 FX_EYE_H=300 FX_LOOK_H=0 FX_LOOK_X=0 ... wreck tall`, or `... how105 earth F WxH 150 300`
  with `FX_LOOK_HIT=0 FX_BARRAGE=8 FX_BARRAGE_STEP=25`.
- Stills: `battle-fx/stills/from-above/from-above-sheet.png` (today left, on right).

## Rock ground: a low pale spray, skittering chips, a pale bruise (grounds.rock, drawn only when the scene passes "rock")

The production lead's theatre ask (17:46Z 22 November), with the environments thread: Italy's hills, the desert's
escarpments and stony steppe were drawn as earth (the game's own ground table maps its rock cells to earth today).
The art director's 20:25Z read of the first look gave rock its own shape: "earth throws a tall dark brown fountain,
rock a low pale grey spray".

- Shape: rock can't be dug, so the blast vents sideways. `crown_low` (0.6) and `crown_wide` (1.35), new for any ground
  (none written: 1, byte for byte as before), squash the crown's fan: each jet's way and reach, the spikes, the body,
  the foot, the lumps and the shower, so the fan stands 0.6 as tall and spreads wider. Its jets pale grey (`soil`
  150, `heavy` 132, `foot` 150 with `foot_dust` 0.6), granular (`grain` 0.75), dark lumps a minority (`clods` 0.25).
  The darkest tenth of the burst measures 112,103,95 at 0.1 s and 107,103,97 at 0.7 s (earth 77,64,56 and 71,62,54).
- Chips (`grounds.rock.chips`, new): 24 to 36 stone splinters 10 to 25 cm across, flung 3 to 12 degrees above level
  out to 30 to 60 m, their fresh faces catching the sun as they turn (`glint`, Bit::bChip); 3 in 10 skitter on along
  the ground keeping 0.4 to 0.6 of their speed, kicking up a little dust where they strike and come down again; they
  lie 1.5 to 3 s. Their own dice; fewer far off (burst.far).
- Scar (`paint.bruise`, new): a pale powder splash (175,170,160) with 4 to 6 broad, short, soft lobes, a small dark
  middle 0.4 of its radius (`paint.scar`), and pale chips scattered 3 to 5 m out. `crater_k` (new, any ground)
  scales the size's crater; rock's is 1.
- Cloud (`smoke_of_ground`, new): the ground's own colour where the game can say (SetGroundColour), pulled `grey`
  (0.4) of the way to its own grey, `shade` (0.9) times as light and its top `top_shade` (0.78) times, written to
  land on that. One rule, no table: pale limestone 182,174,156 gives a warm pale cloud (162,158,148), a tan
  escarpment 168,142,108 a tan one (156,143,127), grey steppe stone 124,122,118 a neutral one (143,142,140). Where
  the game cannot say, `paint.smoke`.
- Switch: the word itself. Until the scene passes "rock" (the land field's FxWords table), nothing changes.
- Preview: `FX_GROUND=138,132,122 ./fxpreview recipe out how105 rock 3,21,300 960x540`.
- Stills: `battle-fx/stills/rock/rock-mend-*.png`.
