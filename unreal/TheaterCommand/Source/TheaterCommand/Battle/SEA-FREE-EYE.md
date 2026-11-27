# The sea free eye: the camera he flies over a sea battle

Brian: "i want the camera angle to be like god mode ... if i click a specific unit it locks in on them and follows
them". At sea that camera is the camera thread's core (`Battle/FreeEye.h/.cpp`, `Battle/FREE-EYE.md`) driven by the
sea's glue (`Battle/SeaFreeEyeGlue.h/.cpp`). Every rule is the glue's: plain C++, no engine types, the very code the
cloud test runs (`Tools/sea_free_eye/run.sh`, `breakages.sh`). `ASeaBattle` and `SSeaBattleScreen` only move values
between Unreal and the glue. The plan it was built from: the sea free eye plan (sea-scene/free-eye/plan.md), with the
critic's marks C1 to C21 and the lead's four rulings.

It is OFF unless `-TheaterSeaFreeEye=1`, and with it off the scene is byte for byte today's: every sea show recording,
the director's shot plan (`sea_under_test` pins it), `measure_test` and every save.

## The switches

| Switch | Default | Meaning |
|---|---|---|
| `-TheaterSeaFreeEye=` | 0 | 1: he may take the camera from the director in a full battle watched live |
| `-TheaterSeaFreeEyeRide=` | 0 | with FreeEye: a lock rides her heading (the core's `bRide`) |
| `-TheaterSeaFreeEyeDrive=` | off | with FreeEye: `tour` (the Mac check's drive), or a file of timed steps |

`SeaFreeEye::Take(SeaLook, !bMonthScene && ShotFolder.IsEmpty())` in `OpenOn` (after the switches are read, before
`PlanShots()`) takes them: on only in a full battle watched live, never in the month's scene or a picture run; Ride and
Drive count only with FreeEye on; the values in force are written back, so the "Sea look:" line says 0 / off when they
were given without `-TheaterSeaFreeEye=1`. The rows are in `SeaLookSwitches.h` (after the StepDown row: `reel_test`
pins the end of the line to its two reel switches) and `Tools/sea_waves/switches_scan.py`, built because the scene reads
them.

## Each frame

1. `Tick`: `FreeDt = DeltaSeconds` before the battle's own 0.1 s clamp. The free eye flies on real seconds, unclamped
   (the core holds them to a quarter second itself), never times the pace: paused, it still flies.
2. The screen (`SSeaBattleScreen`, focusable and under GameAndUI while the switch is on) hands the camera's keys, the
   wheel, the trackpad and every mouse press to `FreeKey`, `FreeWheel`, `FreeGesture`, `FreeClick` and `FreeLetGoKeys`
   (focus lost), which put them in the glue's `Hands`. Its answer for a key is the glue's `SeaFreeEye::ScreenKey`: a
   camera key's press is Handled unless Control, Command or Option is held (a chord is the campaign's, so Command+Q
   still quits); a release is always told to the free eye, so nothing stays held there, but is Handled only for a key
   whose press the screen took, so a key the campaign saw go down it also sees come up (Ctrl+W let go never leaves W
   held on the hidden table). Every other key, L among them, returns Unhandled both ways and reaches the campaign as
   with the switch off. The wheel's notches go on with their sign. The month's scene's screen is unchanged.
3. `AimEye` runs the director exactly as today. Between its under-window line and its cut, the free block calls
   `FreeAim`, which fills a `SeaFreeEye::Scene` (the show's ships, the drawn hull lengths, his side, the real seconds,
   the window's shape, the director's eye, look point and lens in metres, the floor, the board's size, the ceiling, and
   whether the drive may start) and calls `SeaFreeEye::Step`. While the camera is his, `From`, `To` and `Lens` are the
   free eye's and none of the director's under windows is this picture's; `ShotNow` still follows the director's plan
   underneath, so C gives back the director's shot of that moment. The cut line is `SeaFreeEye::CutNow`: off, today's
   terms in today's order; free, his own cut or the core's, nothing of the director's.
4. While free, the funnels are chosen again by `SeaFreeEye::FunnelsDue` (the free eye's cut, a new hero, 1 km moved or
   4 real seconds) and the far wrecks laid by `SeaFreeEye::FarWreckDue` (a cut, half `LiveReach` moved, half a real
   second), never on the director's shot changing unseen. The weather dome repaints on 1,200 m moved, as today.
5. A give back (C, or U while free) makes the director's first frame back a cut, and his funnels are chosen again on it.

## What the glue does

- **The take.** A camera key, the wheel, the trackpad or a lock (J, Tab, double click) takes the camera; a click alone
  or a pick does not. The take's own frame is the director's shot exactly: his eye, his look point and his lens, no
  cut. His hands, and a lock that took the camera, count from the next frame (a lock's glide onto her would move the
  take's frame). A shot under the water is lifted clear (a cut, the core's); a shot over the ceiling, under a low deck,
  is let down to the ceiling still looking at what the director looked at (a cut, the glue's: the core's own let down
  keeps the angle and so sinks the pivot as far under the sea as the shot stood over the ceiling).
- **The give back.** C clears the lock, the pick, the Tab place and the division's id; its own frame is the director's
  (nothing pressed with it locks or takes), and a key still held takes the camera again from his shot on the next frame.
- **U (the lead's ruling on Q9).** U while he has the free camera gives it back first, as C does, then works exactly as
  today (`UnderKey`); through his own dive the director has the camera. While a drive plays, U does nothing.
  `SeaUnder` is not changed. The dive is aimed from the director's eye of the frame shown (`FreeDirAt`, kept in
  `FreeAim` before the free eye's picture goes into `From`), never from where the free eye was flown: so U is not a
  probe (fly beside found boat F, press U, and a dive kilometres away would have said an unfound boat lies near F).
  Off, `bFreeNow` is false and `PressU` gets `EyeAt` as today.
- **Picks and locks.** Only a found ship afloat (`MayPick`: shown, in line or limping) is ever picked, locked, stepped
  to by Tab or made part of a division, rechecked at every lock. A click picks the ship along its ray (her hull box, a
  little forgiveness that grows with distance), else a division whose line is near where the ray meets the sea, else
  nothing; a double click picks and locks. J locks the pick, or lets go, or with nothing picked or locked locks the
  found ship nearest the middle of the picture, only one that is in it (the lead's default for Q3; an unfound boat is
  never read for it, not even to be passed over). Tab steps through the found ships, his side first, by line (none
  last), piece, slot; it wraps and Shift+Tab goes back; from a division it goes to its first member. Every lock,
  however made (a click, J, Tab), moves the pick onto what it locks, so J while locked always lets go (the UI
  director's words: PRESS J AGAIN, TO LET GO), never locks an older click's pick.
- **Feeding a lock.** Her place every frame; her velocity as the scene moved her over this frame's real seconds (none on
  the first frame sent: the core measures it); her heading; her clearance, half her drawn length (half the longest
  member's for a division). A lock taken with the eye nearer her than that has its clearance grown from where the eye
  stands at 20 m a real second, so the core never throws the eye out in one frame. A ship lost from sight, sinking,
  gone or out of the show lets go that frame (no cut; the core coasts a few metres and stops). A division's id is
  renewed at every change of its found members in line, from `1 << 30` up, never otherwise.
- **Letting go holds the boom.** While locked, her clearance holds the boom off her however far his wheel has brought
  it in (the tour's low pan leaves the wheel wanting a 2 m boom; her clearance holds 57 m). On any let go (J, her loss,
  C's own frame aside) the glue hands the core a zoom want equal to the boom it has (`Eye::Want`), so the view holds
  where it is (1g). Dropped at once, the boom shrank to 2 m round a pivot still at her middle and the eye sat some 5 m
  inside her hull for about 4 to 5 s (99 to 735 frames from 24 to 144 Hz; core ask 14).
- **The floor and the ceiling.** The floor is `max(drawn, 0)` (today the flat face, so the drawn sea; the test fails
  if the swell or the gale is built before the free floor is handed the drawn sea), so every "under the water" test
  that reads Z < 0 stays right. The core keeps the eye 3 m over it (0.5 m at the hardest). The ceiling is 1,500 m, or
  under a drawn cloud deck `min(1500, max(300, base - 150))` (the lead's ruling on Q7, from `SeaWeather::DeckDrawn`;
  the art director's ruling of 07:56Z, `max(300, min(1500, base - 150))`, is the same number for every base).
  The pivot's box: the board and 3 km round it, from 20 m under the sea to the ceiling. Shift+E is fed to the core no
  further than the room left under the ceiling (for the boom it has or wants, tilted its steepest): the core's own
  ceiling takes a held rise back by shortening the boom at a speed set by the frame rate (core ask 13; after 30 s
  against a 1,500 m top with no wheel, 1 / 6 / 27 / 58 / 249 m of boom at 24 / 30 / 45 / 60 / 144 Hz), so the top of
  the drive looked and panned differently on every machine. With the glue's hold a held rise against the ceiling does
  nothing, and the tour's boom at 80 s is 818 to 821 m at every rate from 24 to 144 Hz.
- **The lens (the lead's ruling on Q8).** The director's 50 and 55 degrees ACROSS are held on every window shape: the
  core's vertical `FovNear` and `FovFar` are fitted at the take and whenever the shape changes (29.39 and 32.64 high
  at 16:9, 32.50 and 36.05 at 1.6, 33.69 and 37.35 at 1.54), and the core's vertical lens is turned back to the horizontal
  angle Unreal takes. From the director's lens it eases, never faster than 40 degrees a real second; at a cut, at once.
  The art director ruled the other way at 07:56Z, for round 2: "KEEP THE VERTICAL lens (44-49 degrees across is inside
  the director's band)" (sea-scene/free-eye/ad-rulings-1127.md). Round 1 keeps the lead's ruling; the lead to settle Q8
  between the two before round 2. If the AD's stands: `FitLens` goes (`FovNear` and `FovFar` stay the core's 29.4 and
  32.6 high at every shape), check C12 and breakage M17 are written again, and this paragraph with them.
- **The drive** (`-TheaterSeaFreeEyeDrive=tour`, the Mac check's 95 real seconds, `SeaFreeEye::TourText`): it starts at
  the bench's first counted frame (at once without a bench), puts his keys aside but C, and writes a log line for each
  step (its frames, its mean and its worst whole second) and one at its end with the height the free eye rose to. A
  frame goes to the whole second its clock ends in, with a millionth of a second's slack on the summed clock (as the
  step it belongs to), so at a steady rate every step's worst whole second is that rate: a Mac holding exactly 45 reads
  45, not 44.

## The new core

The camera thread's core after its second review (b41bc299a) is on this branch as d5cb46ea3 (core files only). One known
case is left open by the lead: a lock's clearance together with a ceiling, above 165 frames a second, can push past the
kick limit near the ceiling. The sea's cloud test plays 60, 24, hitches and jittered 8 to 33 ms; that case is noted, not
tested.

## Open questions

- Q1. What shows the pick before a lock (a ring on the water, a name label)? Nothing until the UI director says.
- Q2. Division click: the near-the-line rule, or a double click on a locked ship widening to her division?
- Q4. With the switch on, the screen takes C (capital), Tab, WASD, arrows, Q, E, the wheel and every mouse press from
  the hidden campaign table (and J, bound nowhere today); the campaign's edge slide (polled whatever has focus) is
  unchanged. Is that wanted? Not ours to change.
- Q8. Two rulings clash (the lens above): the lead's (hold 50 and 55 across, built) and the art director's of 07:56Z
  for round 2 (keep the vertical lens). The lead to settle it before round 2.

Ruled: Q3 (J with nothing: the found ship nearest the middle of the picture, only if she is in it), Q7 (the ceiling
above; the AD's 07:56Z words give the same number), Q9 (U above, the dive aimed from the director's eye).

Ruled by the art director at 07:56Z for round 2 (sea-scene/free-eye/ad-rulings-1127.md), planned, not built in round 1:

- Q5, the night dome: judged on a NIGHT run of the drive, one still at each end of the zoom with the horizon and both
  top corners in frame; no edge, seam or band at any lens. Round 2: lay the dome at each cut for the WIDEST lens the
  free eye can reach from there (55 across, or the widest the lead's Q8 ruling gives), so zoom never uncovers an edge.
- Q6, stills: yes, at the drive's marks, in a SECOND run of the same drive (never in the timed run), each at the Mac's
  own size: the low pan (one mid-pan along the own line, one just over the water toward the sun and one away from it),
  each Tab lock 2 s after it settles, the climb at about 300 m, 800 m and the top looking level (and at the top one
  looking down on the fleet), mid fast pan at the top, and the frames before and after C. She wants a card when they
  come.
- Q7's light, judged on those stills: at 3 m a level crisp horizon, near water that keeps its colour, glint a path not
  a white sheet, no facets by the lens; at the top ships dark shapes through the haze, no tiling, no hard ring where
  the far water ends. If either end fails, the cheap mend: the light fed a clamped eye (14 to 1,300 m) while the camera
  runs 3 to 1,500 m.

## For the torpedo words lane

`PlaceTorpedoWords` is not on this base. Its caption follows `EyeSubject`, set in `AimEye`. When free, `EyeSubject`
should be the glue's `Shot::Hero`: the locked ship's key, a division's leader, -1 with nothing locked (the scene keeps
it as `FreeHero`). That override goes AFTER that lane's director line, in the free block, or the director's subject
wins. Its "none under the face" test (`EyeAt.Z < 0`) stays right only because the free floor is `max(drawn, 0)`.
`EyeLens` stays the horizontal angle.

## Key clashes (reported, nothing taken beyond the switch)

- WASD, arrows, Q, E and the wheel move the hidden campaign table today (`CampaignPlayerController`); with the switch on
  the sea screen handles them.
- C goes to the capital and Tab is the campaign's Tab key: with the switch on they are the camera's. L opens Forces and
  stays the campaign's with the switch on too: the lock is J, bound nowhere today (no `EKeys::J`, no `Key=J` in Source or
  Config), so it takes nothing from the campaign.
- Every mouse press is the screen's with the switch on (today a left click there can pick a territory or send the army
  in hand on the hidden table). The edge slide is unchanged.
- U and Shift+U go to `UnderKey` from the screen (the under-water view's own read of the key is off while the switch is
  on), so they are never read twice.
- Shift is latched at a key's press (land's rule): a held W stays a pan, a held Shift+W a tilt, whatever Shift does
  meanwhile. Land reads Shift again on its repeats; the sea does not.
- Across the battles: land's L is "all fall back", Tab next/back, C film; air's C rides and its arrows step riders. The
  sea has no FILM button, so C is the only way back. LOCK ON is J in all three scenes (the UI director, 27 Nov 2026,
  06:30Z: L is land's ALL FALL BACK since 1.5, and the free camera's keys are one set everywhere); C is still the UI
  director's to rule.

## Core asks (written down; the core is not patched)

1. Held-key turn and tilt rates in `Settings` (the glue copies land's 75 and 50 degrees a second meanwhile).
2. `Begin` with a lens, easing from it (the glue eases meanwhile).
3. A horizontal lens helper in the core, so land, sea and air convert alike.
4. Tab wording: `FREE-EYE.md` says Tab steps "inside the locked group"; each scene owns its Tab order.
5. Let go coasts with her speed and brakes (about 2 m); a `bLetGoStill` flag if the AD wants a dead stop.
6. The floor is asked 12 or more times an update: say so in `FREE-EYE.md`, so no scene hands it an exact sum.
7. `AnchorId`: document -1 as none and any other value as the scene's (the sea: ship keys, and `1 << 30` up for
   divisions).
8. `Tools/free_eye/run.sh` only prints a NOTE when g++ and clang++ print differently, and still exits 0; make it fail.
   Its scratch defaults to `$TMPDIR/free_eye` (the sea runs it with `FREE_EYE_OUT`).
9. The soft clearance grows the boom faster than `ZoomFastest` when the eye stands inside 1.05 clearances of her
   (some 3.5 m in one 60 Hz frame at 54 m): the glue starts a grown clearance under that margin
   (`SeaFreeEye::ClearSoftMargin`, the core's 1.05 written again).
10. The soft ceiling lets a steep far view creep up to the ceiling: after the drive's 28 s climb the eye stands 2.7 m
    under 1,500 m (1.4 m under a deck's 850 m). The test holds "reaches the ceiling" as within 5 m.
11. The box against the other caps: the floor's lift raises the pivot with the eye, so looking up from low on a long
    boom puts the pivot far over the box's top (over 3 km in the test's random hands), and the core's let down of a shot
    over the ceiling sinks the pivot under the box's floor (the glue now lets such a take down itself). Document the
    box as the last cap, or hold the pivot to it.
12. A lock fed on `Begin`'s first update moves that frame already (its glide: 1.5 m at 60 Hz, 9.5 m at 24 Hz for a ship
    1.8 km off); the glue sends a taking lock from the next frame. Worth a line in `FREE-EYE.md`.
13. Rise held against the box top (the ceiling) shortens the boom at a speed set by the frame rate: with no glue and no
    wheel, Rise held 30 s against a 1,500 m top leaves a boom of 1.0 / 6.4 / 27 / 58 / 249 m at 24 / 30 / 45 / 60 /
    144 Hz (everywhere else the core's rates hold in real seconds: the low turn is 75.0 degrees a second at every rate).
    Ask: a ceiling rule that does not depend on the frame rate (Rise against the top does nothing, or shortens the boom
    by real seconds). Meanwhile the glue feeds Rise no further than the room left under the ceiling, so the core never
    has a rise to take back (check C2 at 24, 60 and 144 Hz; breakage M39).
14. On a let go the core drops the lock's clearance at once while its zoom want is still the wheel's, so the boom
    shrinks to the want round a pivot still at her middle (2 m in the tour, the eye in her hull). FREE-EYE.md and the
    plan say the view holds. Ask: on a let go, keep the zoom want no shorter than the boom the clearance held (or say
    in FREE-EYE.md that the scene must). Meanwhile the glue hands the core `Want(NaN, NaN, Zoom())` on the let go's
    frame (check C4 on the eye; breakage M38).

## The Release2Ship row (with step 4 of the plan, OFF)

```
{ TEXT("seafreeeye"), EKind::Launch, TEXT("Sea battle camera: fly it yourself"),
  TEXT("Move the camera anywhere over the sea battle. J locks the view on the ship picked, and so does a double click on her: the view follows her, and still moves, turns, tilts and zooms as you wish, until you give it back with C or press J again. Off: the director films it."),
  TEXT("-TheaterSeaFreeEye=1"), false,
  TEXT("waits for the Mac drive (-TheaterSeaFreeEyeDrive=tour) at 45 fps, the AD and the UI director"), false },
```

After the sea looks block of `Release2Ship.cpp`, off by default like its neighbours, launch only. Not added yet. Its
words name J, not L (LOCK ON is J in all three scenes, the UI director 27 Nov 2026 06:30Z), and wait on the UI
director like land's line.

## The Mac check

The engine builds only on the Mac. With the bench script (`Tools/sea_bench.sh`): `SEA_BENCH_TIERS=0`,
`SEA_BENCH_HELD_ARGS="-TheaterSeaFreeEye=1 -TheaterSeaFreeEyeDrive=tour"`, `SEA_BENCH_FROM` at the fight's start minus
the drop, `SEA_BENCH_SPAN` at least 100, `SEA_BENCH_BATTLES` only recordings with at least 3 found ships of his own and
the fight for it, five rounds at the least.

- Gate: every STEP of the drive at 45 fps or better, read from the drive's own log lines ("Sea free eye: drive step k
  of n, ...: F frames, R a second over the step, its worst whole second W."), not the bench's mean (at a steady rate W
  is that rate exactly, check C15 at 24, 30, 45, 60 and 144; a Mac holding 45 reads 45). The paired line
  (drive against director) is the free eye's cost. A fail counts as machine load only if an immediate re-run of that
  recording alone passes.
- Again in the heaviest weather: `SEA_BENCH_ARGS="-TheaterSeaClouds=dome -TheaterSeaCloudShade=1 -TheaterSeaRain=1"`;
  under a drawn deck the climb ends at the deck's ceiling, and the drive's last line says the height reached.
- The drive's log lines and frames to the AD.
- One run with the switch off: `-TheaterSeaFreeEyeRide=1 -TheaterSeaFreeEyeDrive=tour` WITHOUT `-TheaterSeaFreeEye=1`:
  the "Sea look:" line says 0 / off, the director films, the battle as today.

## The cloud test

`Tools/sea_free_eye/run.sh [--quick] [recording.json ...]` builds the glue, the core, the sea show and the fleet's waves
with `sea_free_eye_test.cpp` on g++ and clang++ (the Mac's warnings as errors), plays every sea recording through both
twice, and fails on any failed check or any difference between runs or compilers. It checks the scene's wiring as
text (the director's `PlanShots`, `ShotEye` and `AimEye` pinned by `director.sha`, the cut line, the free block,
`OpenOn`'s take, `Tick`'s real seconds, U, the refreshes), the glue's rules by hand-made cases, and every recording as
the scene plays it: his seeded hands at 60 and 24 frames a second, through hitches and jitter, at paces a quarter, 1 and
4, standing still, over a moving 12 m sea, under a deck at 400 m with the window's shape changed, the tour (and under
decks at 1,000 and 400 m), the real gale, and every recording again with its hidden ships deleted (bit for bit the
same). The screen's routing is read as text (OnKeyDown and OnKeyUp answer only as `ScreenKey` says, the wheel's sign)
and `ScreenKey` is played for every key both ways. `breakages.sh` breaks each rule on a copy and shows a check catches
it (M1 to M41, and the second review's V1 to V5).

`director.sha` pins the director's text on the base. A deliberate change to `PlanShots`, `ShotEye` or AimEye's
director part needs its line in `director.sha` written again, in the same commit, with the reason.
