# Release 2 sound: handoff (22 Nov 2026, 17:30Z)

Stopped here because Brian wants his remaining usage spent on a playable release 1. Everything below is release 2.
Nothing in this folder is waiting on Brian except his stamps on the listening page.

## Where things stand

- **Air sound patches 0001-0022** (`part2/`, README there): all sent to the air scene thread ("Build Pearl Harbor and
  Midway 3D battles"). 0001-0018 are on its pushed branch (2bdb2m-unreal 2b177386). 0019-0022 are in its r2h round
  (24b8d1b9), which was in code check when work stopped. The shared mix is `/mnt/project-files/sound/mix/mix.json` (= 0022).
- **Bremen sound-on stall** (`stall/README.md`): not found in our code. Suspects: gun voices, and the film-sound
  recorder (frame-locked capture). The Mac runs are lined up in order in that README: the film command line and
  onset, the trace, a run without the recorder, then the mix-only bisect (`apply_bisect.py` L1/B1/B2/B3). They need a
  lit screen (a locked screen stretches frame times by itself). No results came in before the stop.
- **Held fix:** `stall/gunners-one-take-HELD.diff` (one take per aircraft's gunners). Release it only if the trace
  or B1 points at gunner voices.
- **Not started:** the pooled-voice patch (reuse voices, cap takes begun per frame). The production lead asked for
  it to be prepared and held. Both attempts were cut off by the usage limit before any code was written. Brief:
  pool one-shot takes in a UPROPERTY array with a generation number per voice; check cut times per frame instead of a
  timer per take; cap takes begun per frame, never own guns, anything within 300 m, or big moments; defaults off.

## Listening page (https://claude.ai/artifact/EjLienLHJN29HKCPve1W4U)

Waiting on Brian's stamps (checked 17:27Z, none new):
- `r9_far_bed` (far air battle bed without the studio-library bomber): if Good, send as an air patch to the scene thread.
- `r10_*` (ground squad under fire): tell the ground sound thread which cards are Good or Cut.
- `r11_*` (bullets riddling a metal plane, two cards): if Good, copy the WAVs to Sounds/Real, list them in
  `air.roundsHit.takes` with credits, pick uniformly with no repeat, and skip Hit Text "ground"/"water".
  Notes: `round11/README.md`.

## Studio-library replacements (`wanted/`)

Brian's rule: each old studio-library sound stays until a real recording he marks Good replaces it. B-17s are done
(0019). Still to replace: Stuka engine, torpedo launch, torpedo run, ship sinking, depth-charge thrower, bomb release,
plane break-up. Stuka leads found 22 Nov (`wanted/stuka-leads-2026-11-22.json`, none checked yet): no free, real
Jumo 211 recording exists that we could find; the best stand-ins are Fight2FlyPhoto's Bf 109 (DB 601) recordings on
Freesound (142898 run-up, said to be CC BY 3.0; 242179 start-up; 142902 fly-by). Freesound and Commons pages must be
read on the Mac for licence before any fetch. The other sounds were not searched.

## Rules that carry over

Real recordings only, never synthesized; nothing from films, series, games or studio/effects libraries; licences
CC0, public domain, CC BY, CC BY-NC with credits; Brian's ear judges, so new sounds go on the listening page first.
