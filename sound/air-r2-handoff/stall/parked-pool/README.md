# Parked: pooled sound voices for the air scene (22 Nov 2026)

**Status: parked, not held for release.** The production lead asked for this patch to be ready in case the Mac's trace
showed voice churn behind the Bremen sound-on stall. At 19:03Z the air scene passed on the Mac's result: there is no
stall in play with sound on (a1acb552, 150 s, 43-58 fps, 1,136 takes, no 170/6 ms alternation; -nosound 44-68 fps).
The 170 ms stall came only from the film recorder's fixed-step sound capture, which the Mac builder is fixing. So
the patch has no job now, and the workflow writing it was stopped before its revise pass to save usage.

`0001-Air-sound-held-one-shot-takes-on-a-pool-of-the-scene.patch`: against r2h 24b8d1b9 (AirBattle.cpp, AirBattle.h,
mix.json). One-shot takes reuse pooled voices (a generation number per voice), cut times are checked per frame instead
of a timer per take, and a cap holds far takes back a frame. With no new mix keys it behaves exactly as today (487 of
487 engine calls identical in a stand-in harness). Not built in Unreal.

**Known faults if it is ever revived** (from the three reviews in `reviews.json`; their work is in the session's
scratch folder, not kept):
1. The per-frame cut fires one frame early (the timer model in the harness was wrong); a fix was prototyped.
2. A gun take dropped by the cap is never asked for again, so a short burst can go unheard.
3. With the chosen mix values the cap and the pool never engage in Bremen, Oahu or Midway, so as set it changes
   nothing anyone hears.

Revive only if a real play run (not a film capture) shows sound costing frame time. The sound-on/sound-off gap at the
top end (58 against 68 fps) is the place to look first.
