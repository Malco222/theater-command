# Film critic facts in the game: build notes

The game side of INTERFACE.md sections 1 and 2.

- With `-FilmCritic`, the game writes `<the run's picture folder>/critic-facts.jsonl`, one JSON object a line.
- With `-FilmCriticStrip=<fps>,<seconds>`, it also saves JPEG film strips: 960 px wide, at most 1500 frames a run.

The work was written in a container with no Unreal engine, so none of it has been compiled against the engine. What was checked here, and what a reviewer on the Mac should look at, is at the end.

The changes are left uncommitted on `claude/project-thread-34tup9`, in the worktree at 2fdb13e5.

## 1. Without -FilmCritic

**What runs.** Every hook is one line: `if (FilmCritic::On()) { ... } // film critic: ...`.

- `On()` returns a function-local `static const bool`. It is read once from the command line with `FParse::Param(..., TEXT("FilmCritic"))`.
- `FParse::Param` needs the name to end at whitespace, so `-FilmCriticStrip=` alone does not turn it on.
- With the flag off, the only cost is one call to a function that returns a cached bool. This happens at each hook: 2 a frame (PlayerTick, and the battle's camera hook in a battle), plus one at each cut told and each still.

**What does not run.**
- Nothing is allocated, measured or written.
- Nothing touches the game's state, clocks or random streams.
- No UPROPERTY, UFUNCTION or member was added to any class.
- The one change to a class is a plain C++ const method on `AArmyPiece`, which only reads.

## 2. Files touched

### New files

**`Source/TheaterCommand/Campaign/FilmCriticLine.h`** is header-only, pure C++17 with no Unreal. It formats one JSON line.
- `AppendQuoted` escapes `"`, `\`, `\n`, `\r`, `\t` and other control characters as `\u00XX`. UTF-8 passes through unchanged.
- `AppendNumber` uses fixed decimals (0 to 9), rounds half away from zero and never writes `-0`. It writes `null` for NaN, infinity and values too large to print.
- `Line(t, k)` provides `Num`, `Whole`, `Flag`, `Nothing`, `Str`, `StrOrNull`, `Vec2`, `Vec3`, `Pairs`, `Strs`, `NamedAt` (`{"name":..,"at":[..]}`) and `Done()` (returns `}\n`).

**`Source/TheaterCommand/Campaign/FilmCritic.h`** is the interface the hooks call. It includes CoreMinimal only and forward-declares the rest. It is in `namespace FilmCritic`:
- `On`, `StripAsked`, `Folder`
- `Frame`, `Shutdown`
- `NoteCut`
- `Air`, `Land`, `Sea`
- `Still`, `StepActed`

**`Source/TheaterCommand/Campaign/FilmCritic.cpp`** (about 1300 lines) holds everything else.
- All helper names are in `namespace FilmCriticParts` with an `FCritic` prefix, so nothing can clash under `bUseUnity = false`.
- Its state is one heap object, made on first use and never freed. This avoids any order problem at static destruction.

**`Tools/film_critic/facts_line_test.cpp`** is a standalone g++ test of FilmCriticLine.h. It has 30 exact-match cases.

**This file.**

### Existing files: hooks (each a single guarded line, commented `// film critic: ...`)

| File | Where | Hook | Why |
|---|---|---|---|
| `Campaign/CampaignPlayerController.cpp` | includes | `#include "Campaign/FilmCritic.h"` | |
| | `EndPlay`, first line | `FilmCritic::Shutdown()` | Last lines flushed; strip grabber stopped (after `FlushRenderingCommands`); waits at most 3 s for JPEGs still being written |
| | `PlayerTick`, after `TheaterSpeed::NoteFrame` | `FilmCritic::Frame(*this, DeltaTime)` | Map camera, map fleets, stills that no battle camera took, strip frames, the once-a-second file write |
| `Campaign/CampaignStepPlayer.cpp` | includes | include | |
| | `FStepPlayer::Log`, after the step picture's `RequestScreenshot` | `FilmCritic::Still(FPaths::GetBaseFilename(Picture))` | The `still` line for `step-NNN`, plus fleets at the picture |
| | `FStepPlayer::Tick` phase 3, after `Act` | `FilmCritic::StepActed(Logged + 1, Step.Verb, Step.bReplay)` | Starts a step's strip: `air-wait`/`land-wait` until its picture; `press`/`order-sea` for the given seconds |
| `Battle/AirBattle.cpp` | includes | include | |
| | `AirParts::TellCut` | `FilmCritic::NoteCut()` | This frame's camera line gets `cut:true`. Every cut goes through TellCut: AimEye's shots, and the sky's "every cut the sky sees". Both run before the camera hook in `Tick`. |
| | `AAirBattle::Tick`, after the jolt block, before `if (Stage)` | `FilmCritic::Air(this, S, *Eye, UnitsPerMetre, DeltaSeconds, <overview name>)` | The final camera, with the jolt already applied; its shot and subject; planes in frame; the opening strip; the `end` line |
| | `TakeShots`, after each of 4 `RequestScreenshot` calls | `FilmCritic::Still(name)` | `still` lines (plus `planes` at each still). These run after the camera hook in the same frame, so they take this frame's camera. |
| `Battle/LandBattle.cpp` | includes | include | |
| | `ALandBattle::Tick`, just before `LandFell::Phase("tick: the pictures taken")` | `FilmCritic::Land(this, S, *Eye, UnitsPerMetre, DeltaSeconds)` | The final camera; men, vehicles, marks, look, end; the opening strip |
| | `TakeShots`, after 3 `RequestScreenshot` calls | `FilmCritic::Still(name)` | `still` lines |
| `Battle/LandBattleCommand.cpp` | includes; `ScriptStill` | `FilmCritic::Still("general-" + Name)` | The general's walk stills (`-TheaterLandScript`) |
| `Battle/SeaBattle.cpp` | includes | include | |
| | `ASeaBattle::Tick`, just before `TakePictures()` | `FilmCritic::Sea(this, *Eye, UnitsPerMetre, DeltaSeconds, S.BattleT, TicksSinceCut == 0, shot name, shot subject)` | The final camera, with the shake already applied; the opening strip |
| | `TakePictures`, after the shot's `RequestScreenshot` | `FilmCritic::Still(Shot.Name)` | `still` lines |

### Existing file: one read-only method

**`Campaign/ArmyPiece.h` / `ArmyPiece.cpp`** gain `bool AArmyPiece::FilmFleet(...) const`, a public, non-virtual, plain C++ method declared after `GetNationTag`.

- **Why it exists.** A fleet's wake sections, route leg and hull box are protected, so FilmCritic.cpp cannot read them from outside.
- **What it returns.**
  - The fleet's name.
  - Which wake is shown: section 2 passage is 2, section 1 patrol is 1, none is 0. A wake counts only if it is visible and has vertices.
  - While sailing: the progress and the current route leg's direction, worked out exactly as `Tick` works them out.
  - The bow direction: the body's +Y in world space.
  - The bow point (hull box Max.Y) and the wake's far end (wake box Min.Y), both in world space.
- **What it changes.** Nothing.

## 3. Kinds and fields

All lines start with `t` (real seconds, `FPlatformTime::Seconds() - GStartTime`), `k`, and `f` (the frame number, `GFrameCounter`). In a battle they also carry `bt`.

**Units and conventions**
- Lengths are in metres. Unreal units are divided by the battle's `UnitsPerMetre`, and by 100 on the map.
- The axes are Unreal's: eye and subject share one frame.
- Angles are in degrees. `rot` is [pitch, yaw, roll], Unreal style.
- `fov` is horizontal (UCameraComponent `FieldOfView`, or `GetFOVAngle` on the map).
- `dt` is the frame's game delta in ms. The engine caps it at 400 ms. `t` is the real clock, so a longer hitch shows in `t`.

### Implemented

| k | Written | Fields (extras are additions, not in the spec) |
|---|---|---|
| `run` | Once, at the first fact (the facts file is truncated first) | `scene`: map for a step run; air/land/sea from `-TheaterAirBattle=`/`-TheaterLandBattle=`/`-TheaterSeaBattle=`; land for `-TheaterLandScript`; otherwise map. `key`: the step file's base name or the recording key, else null. `window` [w,h] from the game viewport, else `-TheaterWindow=WxH`, else null. Extras: `strip` [fps,seconds] or null, `steps`, `v`:1. |
| `cam`, air | Every frame | `scene`, `eye`, `rot`, `fov`, `dt`, `cut`. `why`: `flyover-raid` / `flyover-target` / `flyover-guard` (by the flyover clock against `AirDesk::Flyover::RaidLong` and `TargetLong`), `glide`, `close`, or the strategy view's `overview` / `overview-ships` / `overview-guns` / `overview-chutes`. `subject`: `{"name":"group N","at":[..]}` in the flyover, `target`, or `{"name":"plane N","at":[..]}` when riding a plane. Extras: `curtain`, `bars` (the flyover's letterbox share). |
| `cam`, land | Every frame | The same camera fields. `why`: `night`, `general` (the general's own view), or the director's shot name (`LandEye::NameOf`). Extra `special`: the special shot's name. `subject`: `{"name":"track N","at":[..]}`, the recording track the shot follows, placed where it stands now at ground height +1.0 m for a man or +1.5 m otherwise. `cut` comes from `S.Pose.bCut`. Extra `left`: on a cut, the reason the director gives for leaving the last shot (`LastWhy`). |
| `cam`, sea | Every frame | The same camera fields. `why`: the shot's name, else `overview`. `subject`: `ship N` (the shot's Subject index). `cut`: `TicksSinceCut == 0`. |
| `cam`, map | While the camera moves (any change over 0.0005), and once a second at rest | The camera manager's view, i.e. the drawn camera, with `scene`:`map`. It is read at the next frame's PlayerTick, so `t`, `f` and `dt` are the previous frame's, the frame actually drawn. `cut` is true if a cut was told on that frame. No `why` or `subject`. |
| `still` | At each named still and step picture | `name`, `file` (the screenshot request's file name, else `<name>.png`), and the same camera fields as `cam`. The camera is the one in force when the picture was asked for: this frame's battle camera if it had already been written, otherwise the next one written. |
| `strip` | At each strip frame read back | `file` (`strip-<tag>-<NNNN>.jpg`), `of`. `t`, `f` and `bt` are those of the frame that was asked for and drawn. Extras: `w`, `h`, and `dropped` (only once a frame has been dropped). |
| `fleet` | At every still or step picture outside a battle, and every 0.5 s on the map while any fleet sails. Only fleets on screen (actor centre projects inside the viewport, piece not hidden). | `id` (force id), `name`, `nation`, `sailing`, `progress` (0..1 while sailing, else null). `course` [x,y]: the current leg's unit vector while sailing, else null; extra `last_course` holds the last course seen. `bow` [x,y] unit vector, `wake` 0/1/2, `camyaw`, `bowpx` [x,y] and `tailpx` [x,y] (bow point and wake end, projected; null off screen or with no wake). Extra `at`: `sail`, or the still's name. |
| `planes` | Air: every 0.25 s while the flyover and its glide play (after the black lifts), and at each air still | `shot` (the cam `why`). `inframe` {attack, defend}: shown aircraft not on the ground whose position is inside the camera frustum. `nearest_m` {attack, defend}: among those in frame, null if none. Extras: `nearest_all_m` (any shown aircraft, in frame or not), `still` (the still's name, at a still). Side 0 is attack (the raid), side 1 is defend. |
| `men` | Land: once a second | `moving` (men in sight going over 0.3 m/s life speed). `run_p10`/`run_p50`/`run_p90`: nearest-rank percentiles of moving gait men at 2.2 m/s or more, null if there are none. `rates`: distinct (gait clip, rate to 0.05) pairs among moving men. `capped`: the count of moving men whose clip rate is capped. Extras: `insight`, `all_moving` (in sight or not), `runners`, `pace` (the battle speed), `clips` (at most 16 names). "In sight": visible, fade at least 0.5, and chest height (+0.9 m) inside the frustum. |
| `vehicle` | Land: every 0.5 s, for each vehicle in sight (fade above 0, root +1.5 m inside the frustum) | `id`, `type` (model name), `nation`, `heading` (degrees from north, 0..360), `speed` (m/s of life time), `turret` (degrees). Extras: `maker` (the model's own nation), `kind`, `moving`, `dead`. |
| `marks` | Land vehicles: once per model, nation and year-month a run, when first in sight | `kind`, `type`, `nation`, `year`, `month`, `marks` (how many painted markings the model carries). Extra `on`: the battle's markings switch. |
| `look` | Land: once per battle, on its first frame | `look` (the ground's look word, null if none), `month` (the field's month as the game lays it), `year` (the armour's year, else null), `lat`, `lon`, `snow` (snow lying 0..1), `leafless` (`LandScape::LeaflessOf`, the game's own rule), `hour`, `weather`. Extras: `terrain`, `date`. |
| `end`, land | Once, at the first frame of the result card | `how`, extra `broke`. `broke_at`: null if no side broke, else the recording result's seconds (see "not exact" below). `ended_at` (the recording's end), `card_at` (real s, on the frame the card is first up), `offered_continue`: null. Extras: `winner`, `level`, `result_s`. |
| `end`, air | Once, at the first frame of the card | `how`: the card's level. `broke_at`: null. `ended_at`: battle s. `card_at`, `offered_continue`: null. Extra `title`. |

### Strips (-FilmCriticStrip=fps,seconds; fps capped at 120, seconds at 600)

**When strips run**
- **Step run.** After the act of each `press` and `order-sea` step, for the given seconds, tagged `NNN` (the step's picture number), with `of` = `step-NNN`. During each `air-wait` and `land-wait` step, until that step's picture is asked for. The `seconds` limit still applies.
- **Any other run** (battle scene runs). The opening: in the air, from the first frame after the black lifts once the flyover has begun; on land, from the first frame that is not night; at sea, from the first frame. Also after each named still. Tags are `opening` and the still's name; `of` is the same.

**How a frame is captured**
- One `FFrameGrabber` is used, on `UGameEngine::SceneViewport`, the same way as the land battle's dark reading (`StartDarkRead`/`ReadDark`).
  - Its size is 960 × (960 × H/W, rounded to even), BGRA, with 6 surfaces.
  - It is made 5 s into the run, or at the first strip, whichever comes first.
- A frame is asked for on the game thread with `CaptureThisFrame`, in PlayerTick, so the frame read back is that frame.
- Asks are paired first in, first out with what `GetCapturedFrames` returns 1 to 3 frames later.
- Each JPEG is written with `FImageUtils::SaveImageByExtension` on a thread-pool task. The pixels are moved into the task, not copied.

**Limits and pacing**
- At most 16 JPEGs are written at once. A frame over that limit, or a bad buffer, is dropped and counted in `dropped`.
- No more than 64 asks are kept waiting. 1500 asks a run is the hard cap.
- Pacing: the next frame is due one fps interval later (`NextAt += 1/fps`, 4 ms slack). After a long frame the schedule restarts rather than bursting to catch up.
- A new strip takes the place of one still running.

### Not implemented, and why

- **`run.build`**: the game has no build identifier it can read cheaply, and night.sh already knows it, so it is left out rather than invented.
- **`marks` for aircraft, ships and men**: only land vehicles carry a cheap count of their markings (`Model.Painted`). The air and sea models' markings are baked into their meshes or materials, and counting them would mean walking components. The checker treats this as "not measured".
- **`look` for air and sea; `end` for sea; `men`/`vehicle` outside land**: not in the cheap state the hooks see. Air `look` would need the sky and the season worked out again. The sea battle's end has no single result frame in the hook's reach.
- **`end.offered_continue`**: the game shows no such offer at the card, so it is always null, as the spec allows.
- **`end.broke_at` for air**: null. The air result has no "broke" moment.
- **Land `broke_at` is not exact.** It is `Result.Seconds`: the battle second at which the recording's result came. For a side that broke, this is its break or a little after.
- **`still` lines are missing for some stills**: the photographer's own pictures, the walk-pan and lorry-pan stills, and the hidden-man stills have no hook. They are diagnostics, not named stills, and adding hooks would touch more files.
- **Map `why` and `subject`**: the map camera has no shot names.
- **`planes.inframe` ignores the flyover's letterbox**: an aircraft hidden under the bars still counts as in frame. The `bars` share is on the cam line so the checker can allow for it.
- **Late cuts can be missed.** A cut told after a scene's camera hook in the same frame would not be flagged. No such call was found: in the air, AimEye and DrawSky both run before the hook. In the next frame such a cut would show only as a jump.
- **Map fleet timing is slightly off.** Map `fleet` lines use the previous frame's camera, but fleet positions as they stand at the next PlayerTick. Projected points can be one frame out for a fleet moving fast on screen.

## 4. Cost when on (estimated, not measured: nothing could be run here)

**Bytes written**

| Source | Rate |
|---|---|
| A `cam` line | About 230 to 320 bytes, so about 15 to 20 KB/s at 60 fps |
| Air `planes` | 4 a second, about 0.8 KB/s |
| Land `men` | 1 a second, about 0.4 KB/s |
| Land `vehicle` | About 200 B per vehicle in sight, 2 a second (10 in sight is about 4 KB/s) |
| Map at rest | Under 0.5 KB/s |

- In a battle, expect roughly 20 to 30 KB/s. A 25-minute scene comes to about 30 to 45 MB of facts.
- The file is written at most once a second: one append, with the buffered text converted UTF-8 to TCHAR to UTF-8.

**Frame cost**

| Work | Cost |
|---|---|
| Camera line, every frame | A few microseconds |
| Air planes in frame, every frame | A loop over the shown aircraft with a frustum test, well under 0.05 ms |
| Land men, once a second | One pass over the crowd (`PlayingOf`, `PlaceAsPosed`, frustum test), estimated 0.2 to 1 ms for a few thousand men |
| Land vehicles, every 0.5 s | Microseconds |
| Look | Once, a copy of the field grid |
| Map fleets, every 0.5 s while sailing | An actor iterator plus 3 projections a fleet on screen |
| File flush, once a second | Estimated under 0.5 ms |

**Strips**
- Each strip frame costs the grabber's GPU scale-and-copy and a readback of about 2 MB (960×540 BGRA). The grabber holds 6 staging surfaces, about 12 MB.
- JPEG encoding runs on pool threads: about 5 to 15 ms of a worker per frame, so about 0.2 to 0.4 of a core at 30 fps.
- Up to 16 frames are held in memory while they are written, about 33 MB at most.
- The grabber's per-frame callback returns at once when nothing is asked.

## 5. Checked here

- **facts_line_test.** `g++ -std=c++17 -Wall -Wextra -Wshadow -Wconversion -Werror -I Source/TheaterCommand/Campaign Tools/film_critic/facts_line_test.cpp`, run from `unreal/TheaterCommand`, gives "30 passed, 0 failed". clang++ with `-Wshadow-all -Wsign-conversion` is also clean. Every line it prints parses as JSON with Python's `json.loads`.
- **Syntax and types of FilmCritic.cpp and the FilmFleet body.** They were checked with clang++ and g++ (`-std=c++20 -fsyntax-only -Wall -Wextra -Wshadow-all`) against a small stand-in for the Unreal API, written for this check and not committed. The game's own battle-state headers (AirBattleState.h, LandBattleState.h, LandScape.h and those they include) were real. The result is clean. A deliberate error was seen to be reported.
- **What the stand-in cannot prove.** It checks that names, members and types used from the game's own code exist and match. The Unreal signatures in it were copied from memory, so the engine calls must still be checked against the real engine (section 6).

## 6. What a reviewer should check when compiling on the Mac

1. **Module dependencies: none added.** FilmCritic.cpp uses Engine, Core, ImageCore (`FImageView`), MovieSceneCapture (`FFrameGrabber`), RenderCore (`FlushRenderingCommands`) and Slate (`FSceneViewport`). All are already in `TheaterCommand.Build.cs`. ImageWrapper is loaded at run time by `FModuleManager::Get().LoadModule(TEXT("ImageWrapper"))` on the game thread, not linked. CampaignGameMode.cpp already saves images with the same `FImageUtils::SaveImageByExtension(..., FImageView(...))` call.
2. **No `#if PLATFORM_MAC`** anywhere. Nothing is platform-specific.
3. **Includes in FilmCritic.cpp to confirm:**
   - `Async/Async.h`
   - `Camera/CameraActor.h`, `Camera/CameraComponent.h`, `Camera/PlayerCameraManager.h`
   - `CoreGlobals.h` (`GFrameCounter`, `GStartTime`)
   - `Engine/Engine.h`, `Engine/GameEngine.h`, `Engine/GameViewportClient.h`, `Engine/World.h`, `EngineUtils.h`
   - `FrameGrabber.h`, `GameFramework/PlayerController.h`
   - `HAL/FileManager.h`, `HAL/PlatformProcess.h`, `HAL/PlatformTime.h`
   - `ImageUtils.h`
   - `Misc/CommandLine.h`, `Misc/FileHelper.h`, `Misc/Parse.h`, `Misc/Paths.h`
   - `Modules/ModuleManager.h`, `RenderingThread.h`, `Slate/SceneViewport.h`
   - `UnrealClient.h` (`FScreenshotRequest`)

   Order matters in two places: `Battle/AirBattle.h` comes before `Battle/AirBattleState.h`, and `Battle/LandBattle.h` before `Battle/LandBattleState.h`. The other files' order is as the battle files already use them.
4. **APIs I was unsure of.** Each matches its use elsewhere in the codebase where there is one.
   - `FFrameGrabber(TSharedRef<FSceneViewport>, FIntPoint, EPixelFormat, uint32 NumSurfaces)` with 6 surfaces. `StartCapturingFrames`, `IsCapturingFrames`, `CaptureThisFrame(FFramePayloadPtr())`, `GetCapturedFrames`, `StopCapturingFrames`, `Shutdown`. All are as in LandBattle.cpp's DarkGrab.
   - **Two grabbers on the same viewport at once** (DarkGrab in a land battle, plus the strips' grabber). Each registers its own Slate callback, so I expect this to work, but I have not seen it done.
   - `FCapturedFrameData::ColorBuffer` / `BufferSize`, as in ReadDark.
   - `FImageUtils::SaveImageByExtension(const TCHAR*, FImageView(FColor*, W, H))` with a `.jpg` path, **on a thread-pool thread**. ImageWrapper is loaded on the game thread first. The lambda is `mutable` so that `Pixels.GetData()` is non-const, matching CampaignGameMode.cpp's call. `(void)Async(EAsyncExecution::ThreadPool, ...)` discards the TFuture, which does not block.
   - `FlushRenderingCommands()` before the grabber is stopped and destroyed in EndPlay. AirBattle.cpp also calls it.
   - `FScreenshotRequest::GetFilename()` (static), read just after `RequestScreenshot`.
   - `GStartTime` (CoreGlobals.h, a double).
   - `APlayerCameraManager::GetCameraLocation`, `GetCameraRotation`, `GetFOVAngle`. `APlayerController::ProjectWorldLocationToScreen(FVector, FVector2D&, bool)` and `GetViewportSize(int32&, int32&)`.
   - `UGameViewportClient::GetViewportSize(FVector2D&)`.
   - `TActorIterator<AArmyPiece>`, `AActor::IsHidden()`.
   - `TWeakObjectPtr<const AActor>`: assigned from `const AActor*` and compared with `.Get()`.
   - `FMath::RoundToInt32`.
   - `*Eye` on `TObjectPtr<ACameraActor>` in the three battle hooks. If it gives any trouble, `*Eye.Get()` is the same.
   - In `AArmyPiece::FilmFleet` (a const method): `Body->GetNumSections()`, and `Body->GetProcMeshSection(i)`, which is non-const and is called through the `TObjectPtr` member. Also `FProcMeshSection::bSectionVisible`, `ProcVertexBuffer` and `SectionLocalBox` (an FBox), and `GetComponentTransform().TransformVectorNoScale` / `TransformPosition`.
   - `TCHAR_TO_UTF8` / `UTF8_TO_TCHAR` used inside one expression only, never kept.
5. **Warnings.** There are no shadowed names in my files under clang's `-Wshadow-all`, and the helper locals were renamed so they shadow no member. Every new name is in `FilmCriticParts` or `FilmCritic`. Nothing is file-static at global scope, so unity and non-unity builds both stay clean.
6. **Changes since an earlier draft.** The strip capture once went through `ENQUEUE_RENDER_COMMAND`, a macro nothing else in the codebase uses. It is now a plain game-thread `CaptureThisFrame`, as ReadDark does it. `RHICommandList.h` is no longer included.
7. **A quick run on the Mac.**
   - Start any step file or battle scene with `-FilmCritic -FilmCriticStrip=30,3`.
   - Check that `critic-facts.jsonl` appears in the picture folder with a `run` line first.
   - Check that `cam` lines come once a frame in battles.
   - Check that `strip-*.jpg` files are 960 px wide.
   - Then run without the flags and confirm nothing is written: no `critic-facts.jsonl`, no `Saved/FilmCritic`.

## 7. Diff summary

```
 Battle/AirBattle.cpp                    |  8 +++   include, NoteCut in TellCut, Air() after the jolt, 4 Still()
 Battle/LandBattle.cpp                   |  6 +++   include, Land() before the pictures, 3 Still()
 Battle/LandBattleCommand.cpp            |  2 +    include, Still() in ScriptStill
 Battle/SeaBattle.cpp                    |  4 ++   include, Sea() before TakePictures, 1 Still()
 Campaign/ArmyPiece.h                    |  7 +++   FilmFleet declaration (doc comment)
 Campaign/ArmyPiece.cpp                  | 60 ++++  FilmFleet definition (read only)
 Campaign/CampaignPlayerController.cpp   |  3 +    include, Shutdown() in EndPlay, Frame() in PlayerTick
 Campaign/CampaignStepPlayer.cpp         |  3 +    include, Still() at the step picture, StepActed() after the act
 8 files changed, 93 insertions(+)       (no deletions, no changed lines)
 new: Campaign/FilmCritic.h, Campaign/FilmCritic.cpp, Campaign/FilmCriticLine.h,
      Tools/film_critic/facts_line_test.cpp, Tools/film_critic/FACTS-BUILD-NOTES.md
```

All paths are under `unreal/TheaterCommand/Source/TheaterCommand/` except the Tools files.
