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
   (air: the raid's middle; sea: a still (0,0,0)). Give a NEW `FrameId` whenever the frame shifts rather than moves (a
   group joins the raid or leaves it and its middle steps): the core carries the view across a new id. A step with the
   same id it rides as the frame's own movement where it fits the frame's speed last seen (kept through a pause; a
   change of pace up to `PaceJump` times it ahead fits), and carries one further than `TeleportBeyond` beyond that;
   a smaller jump is ridden, a jump in the picture. Optionally `bBox` with the pivot's box.
2. Call `Pose P = Free.Update(In, RealSeconds, &World)` (the scene's member `FreeEye::Eye Free;`) on REAL seconds (not the battle clock; paused it still flies).
   `World::Floor(X, Y)` returns the ground or the drawn sea under the eye, without margin (the core adds FloorClear);
   a wave that moves is asked again each frame. Null is a flat floor at `Settings::FloorFlat`.
3. Place the camera at `P.Eye` looking at `P.Look` (build the rotation from `P.Look - P.Eye`; it is at least 10 m long
   even on the drone's 1 m boom). `P.Fov` is the VERTICAL lens: `UCameraComponent::FieldOfView` is horizontal, so set
   `2 * atan(tan(Fov / 2 * pi / 180) * Aspect) * 180 / pi` degrees. Convert to centimetres and to the scene's own axes as the scene already does.
4. If `P.bCut`, tell the camera manager it is a cut (`SetGameCameraCutThisFrame`), as AimEye does for its own cuts.

## Entering and leaving

Enter with `Free.Begin(EyeAt, LookAt, Frame)` from the director's current shot, `Frame` the same reference the first
Update will feed: the first frame is that shot exactly, no jump (a shot straight down or up included: the tilt eases
into its range from the next frame). Two shots are moved on that first frame, which then says `bCut`: one under
`FloorHard` over the floor (lifted clear) and one over `EyeMost` (let down under it). Leave by not calling Update any more and handing the lens back to the director (C or FILM). Begin again the
next time he takes the free camera; nothing else needs resetting.

## The lock

While locked, feed EVERY frame: `bLock = true`, `AnchorId` (a squadron, an aircraft or a ship: a stable id), `Anchor`
(its place now), `AnchorVel` in metres per REAL second as the scene moved it this frame (battle pace multiplied in,
zero when paused) with `bAnchorVel = true` (or false: the core measures it), and `AnchorHeading` (for `bRide`).
`In.AnchorClear` (an Input, not a Setting) keeps the eye off it: about a wingspan for an aircraft, half a ship's
length for a ship. A new `AnchorId` is a skip (Tab, Shift+Tab); give a new id too when the anchor itself changes (a
squadron's middle when a member joins or falls), since the core rides any step of the same id within `TeleportBeyond`
of its own movement rigidly. A lock glides, unless its glide would take the pivot further than `CutBeyond`, last longer than
`GlideLongest` (an aircraft some hundreds of m/s faster than the view, or 3 km off on the drone's short boom), or run
the eye through the anchor's clearance (met head on, passing through the eye): then it cuts. Stop sending `bLock` (or
send `bLetGo` once) to let go: the view keeps its place and speed and comes to rest in the frame, braked at the
glide's push, never turning back. After a let go
the lock takes again only on a new rise of bLock or a new id. An anchor that is gone: just stop sending it.

## Keys, the same in all three battle kinds

W A S D or arrows pan; Q / E turn; wheel or pinch zoom; Shift+W / Shift+S tilt; Shift+Q / Shift+E down and up (Rise);
L locks on the thing picked or lets go; Tab and Shift+Tab next and previous inside the locked group; a double click on
a unit locks on; C or the FILM button gives the camera back to the director. Presets (top view, level) go through
`Free.Want(Heading, Tilt, Zoom)`: eased, never a jump.

## Settings per scene (set `Free.Set` before Begin)

- Sea: `FloorClear = 3` over the drawn wave (Floor returns the wave's height), `EyeMost = 1500`, `CutBeyond = 800`,
  `SteadyZ = 3` (a ship's heave does not bob the picture: a first-order low-pass, so of an 8 s heave 39% still
  passes; 6 s passes 21%, 10 s 13%), `bRide` optional per his choice, `FovNear = 29.4`, `FovFar = 32.6` (the
  scene's 50 and 55 horizontal at 16:9).
- Air: `TiltLeast = -80` (looking up at a bomber from under it), `CutBeyond` as the scene likes (3000 default),
  anchor velocity per real second; `Floor` the ground under the eye; `FovNear = FovFar = 32.6` (the strategy view's
  55 horizontal at 16:9). The defaults (44 and 50 vertical) are some 45% wider than either scene's lens.
- Both keep `bPanKeepsLock = true` (he drifts about inside the bomber stream; he sits off a ship's bow).
- `FovNear` and `FovFar` are VERTICAL degrees. A scene whose lens is given horizontally (Unreal's FieldOfView) turns it
  first: 55 horizontal at 16:9 is `2 * atan(tan(27.5 deg) / 1.778)` = 32.6 vertical.
- `GlideLongest` (15 s, added to the specification's numbers) bounds how long a glide may take before a lock cuts.

## What the core does NOT do

Picking (which unit is under the click), hidden or fallen units (the scene stops sending a lost anchor or skips),
drawing, input mapping, sounds, and the director's own shots. It also does not see ships or aircraft other than its
anchor: only the floor, the ceiling (`EyeMost`), the pivot's box and the anchor's clearance stop the eye.

## Notes for the owners

- The turn keeps the asked 140 deg/s at 500 deg/s/s. The zoom is slower than first asked (0.25 of its range a second
  at 0.8, not 0.5): at the asked values the boom's growth flings the eye harder than the 1,200 deg/s/s it is held to.
  The header gives the arithmetic.
- Where two caps meet the floor wins, then the ceiling, then the anchor's clearance, then the box. Looking straight up
  at a ship from the water the floor and her clearance fight and the hard rules act on some frames (counted: some
  hundreds in twenty seconds); the picture is still held to the kick.
- A long boom swung under the floor (looking up from 1.4 km away, or a far view tilted up or zoomed out low over the
  ground) is put clear at once: no spring climbs that in time. His own descent (Rise) stops softly at FloorClear.
