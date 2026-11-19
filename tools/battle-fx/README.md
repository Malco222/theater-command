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
  (1.5): the wrapper sets the material's `Brightness` to it and the kit writes every colour that much darker, so all
  looks as it did; the hottest lights (`muzzle.hot.over`, `tracer.day_over`) are written up to that much past white.

By day a tracer is now a solid pale core (`tracer.day_core`, `day_core_px` 2) inside a warm edge (`day_edge`),
`day_px` (3.4) across in all, straight-sided and at its whole opacity along its whole length: 90 luma over pale
ground where build 1720 had 25. At dusk as before.

Preview: `FX_PALE=1 FX_MG_VIEW=game FX_LENS=42 ./fxpreview recipe.txt out mg sand 0,2,4,60,120 1072x670` is the game's
own camera on the machine gun (18.75 m off, 22 degrees down, a 42 degree lens, the drawn size, pale dry ground).
`FX_NOCULL=1` lays everything in range, as before the view's width was known.

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
  `thin_after_s` only one puff in `thin_keep_one_in` (4) hangs on, growing on to `thin_grow` times, so an old trail is
  fewer, bigger lumps: about 250 puffs for a fighter's fire trail at 150 m/s, 170 for its smoke. Each kept puff
  thickens to `thin_alpha` times its opacity by `thin_after_s`, holds full to `thin_hold` of its life and is drawn out
  over the gap to the next, so the old trail stays one dark line (`thin_ragged` makes the gaps and sizes uneven, a
  torn ribbon, never a string of beads) (04:59Z: a fire trail 110 or darker for two thirds of
  its length on a 212 sky, its oldest part no paler than 150; a straggler's head about 130). A hit's puff hangs 3 to 4 s.
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
