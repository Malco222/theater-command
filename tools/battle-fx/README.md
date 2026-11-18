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
shows every one (and a slow frame shows a flak flash's pieces together, glow, spikes and heart, never the heart alone); a close tracer (under 100 m) is drawn whole, muzzle to strike.

Sun and sky: `SetSun` gives the way toward the sun. Smoke's shadow side never falls below `game.sky_fill` of its own
colour in the hour's light, and looking toward the sun the thin edges of smoke and the fringe of a cloud glow warm
(`game.sun_through`). The sun's own light is full by day and none at night, read from the hour's light; the preview
takes it as FX_SUNLIGHT=r,g,b.

Names given to the kit (size, ground, calibre, gun, weapon, air gun kind) are matched in lower case, as in the recipe:
`howitzer`, `snow`, `bomb250`, `hmg`. The wrapper lowers them, so `Snow` finds snow; a name the recipe does not have
falls back (an unknown ground to earth, an unknown size to how105, an unknown air gun to hmg).

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
- The battle decides lead and convergence: each gun gets its own aim point, and wing guns converge where it says.
- `AircraftVelocity` is in world units a second. Rounds fly at the gun's speed plus the aircraft's, slow in the air
  and fall. Rate and muzzle speed come from `air_guns` in the recipe (`rifle`, `hmg`, `cannon20`); pass your own to
  override (rounds a minute, world units a second).
- Every round flickers at the muzzle; one in five is the ruled tracer. Cannon also throw licks and light the wing.
- The preview draws it: `./fxpreview recipe.txt out air hmg 15,45 960x540 side` (shots `chase`, `side`, `far`,
  `target`, `hit`, `trail`; `FX_DUSK=1` for dusk, `FX_TURN=deg/s` for the turn, `FX_TRAIL=smoke|fire|glycol`).

## Hits on aircraft and trails

```cpp
// a round striking an aircraft (Kind: the gun that hit)
Blasts->AirHit(AirNow, HitPointWorld, SkinNormal, TargetVelocity, TEXT("cannon20"));
// an engine trailing: once, then every frame, then when it ends
Blasts->TrailStart(AirNow, EngineId, EngineSocketWorld, TargetVelocity, TEXT("smoke"));
Blasts->TrailMove(AirNow, EngineId, EngineSocketWorld, TargetVelocity);
Blasts->TrailStop(EngineId);
```

- `TrailStart` again on a running trail changes its kind, such as smoke catching fire (`fire`).
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
  for home. Each burst is about 12 to 19 puffs.
- The preview: `./fxpreview recipe.txt out air hmg 6,20,95,245 960x540 burst` (one burst, close) or `... 360 960x540
  flak` (heavy guns round a flying aircraft).

`-TheaterBlastTest` on the command line plays a 24-second barrage of every size and kind round the point given to
RunTest, again and again, for test pictures.
