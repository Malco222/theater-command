# The Bremen sound-on stall (r2f, 22 Nov): what the sound director found, and what the Mac should run

The builder's report (via the coordinator, 11:37Z): the Bremen r2f **sound** run stalls the game thread, "one ~170 ms
step in two once the battle starts", only with sound on. It came in with r2e/r2f.

## What was checked in the cloud (two workflows, each with a skeptic)

- **No effect flood.** The flight core's effects per step, a1acb552 against 2b177386, at 60 fps, 30 fps and
  alternating pacing: about the same or fewer. The core step is 0.17 ms on average (0.69 ms at 30 fps, worst 28 ms).
- **Hits make no sound.** Every Hit in Bremen is Text "air"; `air.roundsHit.takes` is `[]`.
- **Gun takes begun.** At 60 fps r2f begins 23-30 gun takes a second 20-60 s after contact, against r2d's 16-18. But
  at the film's own fixed 1/30 s step the two are about equal (30-60 s: 17.8 against 18.7 a second), and r2g (with
  0021, a twin turret heard as one take) is under r2d (10.8). So gun voices are a suspect, not a finding.
- **Onset does not fit the guns.** No gun fires until about 16 s after the formations meet (contact is about 32 s into
  the x10 run-up, later with the flyover). If the stall begins "once the battle starts", i.e. before contact, gun
  takes cannot be its cause.
- **r2d had long frames too.** Its film has 70 audio stretches of 52-204 ms (mostly 167 ms) in the first 25.6 s, the
  flyover and approach, with no gun takes laid (sound/air-r2/bremen-r2d-film-measured.md). What r2d lacked was a
  sustained stall in the fight.
- **Not auditable from here:** the film-sound recorder (THEATER_SOUND=record, -UseFixedTimeStep -fps=30). It is only
  active with sound on and is not in the repo. It fits "sound on only", and a hold-and-catch-up step can make
  long/short pairs.

## What the Mac should send or run, in order

**Screen lock (production lead, 14:48Z):** the Mac runs its queue with the screen locked until about 16:45Z. A
locked screen can stretch frame times by itself, so any stall reading taken locked is provisional. The control run,
the trace and the bisect below want a lit screen, after about 16:45Z. Each reading should say "screen locked" or
"screen lit".

1. **The exact facts:** r2f's film command line; when the first long frame comes, in film seconds and against the log
   lines for the flyover's end and the formations meeting; and two successive 20 s "takes laid on" tally lines
   (the tally is cumulative: send the difference).
2. **The trace** (already in BUILDER-NOTES run 3): sound on, `-trace=default,counters,audio,audiomixer
   -statnamedevents` (or `-trace=default,counters`). Where does the ~165 ms sit in a long frame: the end-of-frame audio
   update or a fence wait under it; AirBattle_PlayEffects > AirBattle_Lay; AirBattle_EngineSounds or _AirLevels;
   CollectGarbage; or the recorder's own work?
3. **Two controls:** one sound-on run **without the recorder and without -UseFixedTimeStep**; and, if the stall
   survives that, the mix-only bisect below.

## The bisect (no rebuild: the game reads mix.json from Content at battle open, AirBattle.cpp:697 in r2g)

`python3 apply_bisect.py <variant> unreal/TheaterCommand/Content/Data/Sound/mix.json`, one variant per run; restore
with `git checkout -- unreal/TheaterCommand/Content/Data/Sound/mix.json` between runs and before any picture run or
pre-check. Never commit. The script edits only the named values, checks the file still parses and that nothing else
changed, and refuses a file already edited. Tested on a1acb552's, r2g's and today's mix.json.

| Run | What it silences or changes | If the stall goes |
|---|---|---|
| B0 | nothing (the baseline) | |
| L1 | the B-17 formation loop and the ridden B-17's interior back to r2d's takes (0019 undone in the mix) | 0019's new loops; send me the trace |
| B1 | the bombers' gunners' takes (heavy.turret, heavy.handHeld, handHeldRifle) | gunner voices; the held fix below |
| B2 | every gun take | gun voices in general |
| B3 | B2 and the rounds passing the eye | the cracks |

Every run: the log must have "Air battle sound: master limiter on" and must not have "mix.json is missing" (a broken
file silences the battle and looks like a cure). If the stall begins before contact, run L1 first; gun takes cannot
be heard then.

## Held: `gunners-one-take-HELD.diff` (against r2g ab5ca046; applies with git apply)

Each aircraft's gunners of one calibre heard as one take (0021's twin-mount rule widened to the whole aircraft), and a
playing take moved once a frame by its nearest gun. Modelled: about 28-40% fewer gun takes begun. Heard: each bomber's
gunners as one burst at their combined loudness, from the nearest firing gun; a gunner who joins mid-burst is heard at
the next lay (within 0.83 s). Bookkeeping checked on Bremen, Oahu and Midway (no take cut while a gun fires, none left
playing). Not compiled in Unreal. **Not to be taken in unless the trace or B1 points at gunner voices**, or r2h's
150-aircraft raids push voices past the 128 the engine plays.
