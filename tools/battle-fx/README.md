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
- The preview draws it: `./fxpreview recipe.txt out air hmg 15,45 960x540 side` (shots `chase`, `side`, `far`,
  `target`, `hit`, `trail`, `wake` (the whole trail from `FX_DIST` m off the beam), `map` (the strategy camera,
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
- Smoke hangs (art director 04:40Z): a fire trail lives 30 s and a straggler's smoke 20 s. To fit the room, past
  `thin_after_s` only one puff in `thin_keep_one_in` (4) hangs on, growing on to `thin_grow` times and thinning slowly,
  so an old trail is fewer, bigger lumps: about 250 puffs for a fighter's fire trail at 150 m/s, 170 for its smoke.
  A hit's puff hangs 3 to 4 s.
- A trail not moved for 30 s is forgotten; call `TrailStart` again to bring it back.
- Kinds live in the recipe's `air_trails`: `smoke`, `fire`, `glycol`. What a hit looks like lives in `air_hit`.
- The same clock rule applies as for `AirGun`.

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
