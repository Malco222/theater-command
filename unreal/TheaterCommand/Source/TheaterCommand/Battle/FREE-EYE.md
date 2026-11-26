# The free camera (FreeEye): how a scene wires it

Brian: "i want the camera angle to be like god mode ... if i click a specific unit it locks in on them and follows
them". `Battle/FreeEye.h/.cpp` is that camera's core for the AIR scene (AAirBattle) and the SEA scene (SeaBattle). Plain
C++, no engine types; `#include "Battle/FreeEye.h"`. The ground battle keeps LandGeneralEye. Its cloud test is
`Tools/free_eye/run.sh` (and `breakages.sh`, which proves the test sees each rule broken).

## Each frame

1. Fill a `FreeEye::Input` (metres, X east, Y north, Z up; headings radians, 0 north, clockwise):
   keys into `PanRight`, `PanAhead`, `Rise` (-1..1, held); this frame's two-finger slide into `SlideRight`,
   `SlideAhead` (picture widths and heights, the way the VIEW should go) with `Aspect`; wheel notches or pinch steps into
   `Zoom` (+ in); `Turn` and `Tilt` in degrees this frame (+ clockwise, + further down). `Frame` and `FrameId` every frame
   (air: the raid's middle; sea: a still (0,0,0)). Optionally `bBox` with the pivot's box.
2. Call `Pose P = Eye.Update(In, RealSeconds, &World)` on REAL seconds (not the battle clock; paused it still flies).
   `World::Floor(X, Y)` returns the ground or the drawn sea under the eye, without margin (the core adds FloorClear);
   a wave that moves is asked again each frame. Null is a flat floor at `Settings::FloorFlat`.
3. Place the camera at `P.Eye` looking at `P.Look` (build the rotation from `P.Look - P.Eye`; it is at least 10 m long
   even on the drone's 1 m boom). `P.Fov` is the VERTICAL lens: `UCameraComponent::FieldOfView` is horizontal, so set
   `2 * atan(tan(Fov / 2) * Aspect)`. Convert to centimetres and to the scene's own axes as the scene already does.
4. If `P.bCut`, tell the camera manager it is a cut (`SetGameCameraCutThisFrame`), as AimEye does for its own cuts.

## Entering and leaving

Enter with `Eye.Begin(EyeAt, LookAt, Frame)` from the director's current shot: the first frame is that shot exactly,
no jump. Leave by not calling Update any more and handing the lens back to the director (C or FILM). Begin again the
next time he takes the free camera; nothing else needs resetting.

## The lock

While locked, feed EVERY frame: `bLock = true`, `AnchorId` (a squadron, an aircraft or a ship: a stable id), `Anchor`
(its place now), `AnchorVel` in metres per REAL second as the scene moved it this frame (battle pace multiplied in,
zero when paused) with `bAnchorVel = true` (or false: the core measures it), and `AnchorHeading` (for `bRide`).
`AnchorClear` keeps the eye off it (about a wingspan for an aircraft, half a ship's length for a ship). A new
`AnchorId` is a skip (Tab, Shift+Tab); a lock further than `CutBeyond` cuts, nearer it glides. Stop sending `bLock`
(or send `bLetGo` once) to let go: the view keeps its place and speed and comes to rest in the frame. After a let go
the lock takes again only on a new rise of bLock or a new id. An anchor that is gone: just stop sending it.

## Keys, the same in all three battle kinds

W A S D or arrows pan; Q / E turn; wheel or pinch zoom; Shift+W / Shift+S tilt; Shift+Q / Shift+E down and up (Rise);
L locks on the thing picked or lets go; Tab and Shift+Tab next and previous inside the locked group; a double click on
a unit locks on; C or the FILM button gives the camera back to the director. Presets (top view, level) go through
`Eye.Want(Heading, Tilt, Zoom)`: eased, never a jump.

## Settings per scene (set `Eye.Set` before Begin)

- Sea: `FloorClear = 3` over the drawn wave (Floor returns the wave's height), `EyeMost = 1500`, `CutBeyond = 800`,
  `SteadyZ = 3` (a ship's heave does not bob the picture), `bRide` optional per his choice.
- Air: `TiltLeast = -80` (looking up at a bomber from under it), `CutBeyond` as the scene likes (3000 default),
  `AnchorClear` about a wingspan, anchor velocity per real second; `Floor` the ground under the eye.
- Both keep `bPanKeepsLock = true` (he drifts about inside the bomber stream; he sits off a ship's bow).

## What the core does NOT do

Picking (which unit is under the click), hidden or fallen units (the scene stops sending a lost anchor or skips),
drawing, input mapping, sounds, and the director's own shots. It also does not see ships or aircraft other than its
anchor: only the floor, the ceiling (`EyeMost`), the pivot's box and the anchor's clearance stop the eye.

## Notes for the owners

- The turn, tilt and zoom defaults are slower than first asked (turn 120 deg/s at 400 deg/s/s, zoom 0.25 of its range
  a second at 0.8): at the asked values the picture swings harder than the 1,200 deg/s/s it is held to. The header
  gives the arithmetic.
- Where two caps meet the floor wins, then the ceiling, then the anchor's clearance, then the box. Looking straight up
  at a ship from the water the floor and her clearance fight; the view is held by the hard rules there, not eased.
- A long boom swung under the floor (looking up from 1.4 km away) is put clear at once: no spring climbs that in time.
