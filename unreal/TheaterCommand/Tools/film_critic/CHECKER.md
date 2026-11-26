# Film critic: the morning check (cloud side)

How the night's zips become the owner's morning page. Formats shared with the game and the Mac are in INTERFACE.md;
this file covers the tools in this folder that run in the cloud, their limits and why, and what their outputs mean.
All tools are plain Python scripts (numpy, Pillow, PyYAML; ffmpeg for a video), no network. Run anything that reads a
night with `python3 -I` (the night came from another machine; add `-B` so no `__pycache__` is left in the kit).

## The chain, in order

```
NIGHT=.../nights/2026-11-26            # the night's zips, unpacked: one folder per scene, night.json, attempts.log
WORK=.../2026-11-26-check
python3 -I -B check_night.py "$NIGHT" --out "$WORK/night-check.json"           # [--rules rules.yaml] [--shots scenes/shots.yaml]
#   -> the vision judges: one helper per entry of night-check.json "judge_list", following judge_prompt.md;
#      every FAIL then goes to a second helper following verify_prompt.md. Save their JSON answers as two lists:
#      $WORK/judge.json  [ {picture, judge, answers, other, look}, ... ]
#      $WORK/verify.json [ {picture, judge, checks}, ... ]
python3 -I -B report.py "$WORK/night-check.json" --verdicts "$WORK/judge.json" --verify "$WORK/verify.json" \
        --history history.json --out-dir "$WORK/report"
#   -> publish $WORK/report/index.html with the files listed in $WORK/report/publish-files.json;
#      send each team its part of $WORK/report/routing.json
```

`history.json` lives between mornings (keep it with the reports, not in the night folder): report.py reads it to
say NEW / STILL THERE / FIXED, then writes tonight into it (the last 60 nights are kept).

Test the whole chain on a made-up night: `bash tests/run_chain.sh [WORK_DIR]` (34 checks, then screenshots of the
page at 1440 and 390 wide, light and dark, and a check that nothing scrolls sideways).

## measure_clip.py: movement faults in a run of frames

`measure_clip.py FRAMES_DIR_OR_VIDEO --out R.json [--sheet S.jpg] [--fps 30] [--times T.json] [--expect-motion]
[--merge-repeats|--no-merge-repeats] [--stutter-x 0.5] [--stutter-floor 4] [--readings]`

How it reads motion: each frame is compared with the one before at 640 px wide by phase correlation, with a low-pass
weight (0.06 cycles a pixel) so that the 8 px block grid of JPEG / H.264 cannot make false peaks, and a sub-pixel
fit of the peak (about a tenth of a pixel). This is done for the whole picture and for 12 tiles (3 rows x 4). A tile is
read only when it has detail (mean step between neighbouring pixels, x plus y, over 2.5 grey levels: sky, haze and
calm sea are skipped) and a clear peak. The view's move is a least-squares fit of the agreeing tiles (move, zoom,
roll), refit without the tiles that disagree (the plane the camera follows moves against the ground). Pixel sizes in
the output are the source's; `*_1920` fields are the same on a 1920-wide picture, the scale of the team's jump rule.

| measure | counts a frame when | limit and why |
|---|---|---|
| jump | the whole view moves further than 3% of the width, and over 3 x the median move of the 10 frames around + 1% | unchanged from the first version: a 3% lurch is seen at once |
| stutter | while the view, or one part of it, is moving (mean move over the frames around, this one included, at least 1 px on 1920), this frame's move is off what the frames around lead to expect by more than **X = 50%** of the usual move **and** by more than **4 px on 1920** | see the tuning below. "Expected" is a smooth curve (quadratic, refit once without outliers) through the 5 frames each side, not a flat median, so easing in, easing out and swaying are not faults; only frame-to-frame unevenness is |
| zoom | the picture's size changes by over 8% in one frame beyond the median of the frames around | a zoom flip (the team's "flicker" in the overview camera was zoom flips of 832 m to 450 m) |
| pop | one cell of a 6 x 6 grid changes far more than in the frames around (z over 8, over 6 grey levels, at most 4 cells) | unchanged |
| flicker | the whole picture's brightness jumps over 10 levels and comes back the next frame | unchanged; a cut now needs the picture's structure to change too (with each frame's brightness taken away), so a one-frame flash is a flicker, not a cut |
| freeze | 3 near-identical frames in a row while the frames around visibly move (median difference over 1 grey level); or, with real times, a gap over 100 ms and 3 x the usual | the first version also counted chase shots that hold a plane still against haze: 2 to 42 "freezes" on the old clip depending only on the JPEG quality |
| blank | black, white or flat frame | unchanged |

Parts of the picture (tiles) are judged for stutter on their own only while they hold one motion: in 4 frames of 5
around, the next-best correlation peak is under 0.35 of the best (one motion reads 0.18-0.33; men running across
grass that pans read 0.35-0.6, and the reading flips between the two moves, which is not stutter). A stutter fault
says `where`: `whole view` or the part of the picture (`bottom centre` ...).

Repeats: a frame identical to the one before while the clip moves. A 30 fps screen recording makes these when the game
and the recorder drift apart. With `--merge-repeats` (the default for a video file; off for a folder, so off for the
game's own strips) a repeat and the next frame share their move before stutter is judged. With `--times` (check_night
passes the `strip` lines' real times), a move is judged per real second, so an uneven frame time is not taken for an
uneven camera.

### Tuning on three real recordings (30 fps screen recordings, 1440 x 878)

Readings with the limits above, run on the mp4 files (`measure_clip.py X.mp4 --fps 30 --expect-motion`):

| clip | stutter | a minute | where | pop | jump | zoom | freeze | cuts | repeats |
|---|---|---|---|---|---|---|---|---|---|
| f1-old-flying-clipA-fighters (old flying: the owner saw "jumpy fighters") | **45** | **125.2** | all on the followed fighter, bottom centre; median step 8.5 px on 1920 | 41 (114/min, same place) | 0 | 0 | 0 | 0 | 0 |
| f1-NEW-flying-d132e5d7-clipA-fighters (new flying, smooth) | **0** | 0 | | 0 | 0 | 0 | 0 | 0 | 0 |
| r2-b2-grass-wind-10s (four shots of grass, men and bushes) | 11 | 66 | 5 the whole view (one-frame lurches at the start of a move and a 2x step), 6 parts | 0 | 0 | 0 | 0 | 4 | 2 |

What the old clip shows: the chase camera holds the view still, and the fighter rocks on screen in steps of 1-4 px at
640 wide (3-12 px on 1920) every 6 frames (five times a second), still in between: its attitude is updated in steps,
not smoothly. That is 125 steps a minute, close to the "100 jumps a minute" the team measured on the old overview
camera. In the new clip the same tiles read under 0.1 px every frame.

How X and the floor were chosen (stutter counts as whole / view / parts, frames from the mp4 at ffmpeg's default JPEG
quality, repeats merged):

| X, floor (px on 1920) | old | new | grass |
|---|---|---|---|
| 0.25, 2 | 76 | 0 | 62 (49 view) |
| 0.25, 4 | 45 | 0 | 13 |
| 0.5, 2 | 76 | 0 | 16 |
| **0.5, 4** | **45** | **0** | **9** |
| 0.5, 8 | 40 | 0 | 3 |
| 1.0, 4 | 43 | 0 | 3 |

Old against new holds at every setting. What moves is grass: its steady pans read 1.4 / 2.5 / 1.4 / 2.5 px a frame
at 640 wide (the recorder's beat against the game's frame rate, about +-30%, 1.5 px on 1920). A 2 px floor counts that
beat as stutter (49 frames); 4 px does not, and still catches a one-frame 2x step (frame 101: 5.2 px where its
neighbours move 1.4-2.5). X = 50% matters only for fast pans, where 4 px is a small share of the move. Without the
repeat merge the grass reads 10 instead of 9 (2 repeats).

Uncertain: these are screen recordings, not the game's strips; the game's strips at 960 px will have their own
JPEG noise and real frame times. Re-check the limits on the first real strips (a pan the owner calls smooth should
read 0). The tile stutter needs texture: a plane against flat sky with no ground behind it is read only through the
whole-picture fallback.

## check_facts.py: checks over critic-facts.jsonl

`check_facts.py critic-facts.jsonl [--checks a,b] [--context '{"theater":"T1","year":1940,"month":8}'] [--out R.json]`

Each check returns `{status: pass|fail|not-measured, measured: {...}, worst: [moments], words: one sentence}`. A kind
of line that is missing is **not-measured**, never a pass. A moment (`worst`) carries `t` (real s), `bt` (battle s,
when given), the shot, its size, plain words, and the nearest still (`still`, `still_off_s`) and strip frame (within
2 s). A check that breaks on a bad line returns not-measured with the error, so the night's other checks still run.

| check | from | fails when | limits and why |
|---|---|---|---|
| camera-smooth | `cam` | any jump, zoom pop, stutter or eye lurch | **jump**: the view's screen move (turn as px on 1920 at the frame's lens, roll as px at the edge), or the subject's screen move when `subject` is a position, beyond the straight-on guess from the frame before (scaled by the frames' lengths) by over **20 px on 1920**: the team's rule from the Swedish jump findings. The frame after a jump is guessed from the frame before the jump, so one lurch counts once. **zoom pop**: the lens, or the distance to the subject, changes over **10%** in one frame (same source). **stutter**: as measure_clip (X 50%, 4 px), on speed per real second, so a long frame with the camera on its path is a hitch, not a stutter. **eye lurch** (no subject position): the eye's step beyond the guess over 0.5 m and 3 x the usual step. Steps are not compared across a declared `cut`, a change of scene, or a gap longer than 3 frames (the map logs once a second at rest). **Hitches** (frames over 50 ms and over 100 ms) are reported and named in the words but do not fail the camera: they are the speed detective's |
| fleet-bow-on-course | `fleet` | a fleet with over 25% of its sailing samples at dot(bow, course) under 0.9; or the wake habit | turns are brief, so a quarter of the samples are allowed off. **Wake habit**: with 3+ fleets showing a wake, the screen direction from bow to wake end (`tailpx - bowpx`) agrees across fleets (mean resultant over 0.9, 3+ within 30 degrees) while their headings relative to the camera do not (under 0.7): the "wakes always point left" bug |
| opening-shows-both-sides | `planes` | both sides in frame under 50% of the opening's samples | the opening is the shot named flyover / opening / 00-..., else the first shot. Worst: the longest stretches with one side missing |
| marks-on-every-model | `marks` | any model with `marks` 0 | named by kind, type, nation, year-month |
| men-gait-variety | `men` | over the seconds with 20+ men moving, the median of p90/p10 running speed under 1.25 or the median number of gait clips/rates under 3 | the median, so a moment of order does not fail a battle; the worst seconds are listed |
| vehicles-no-rocking | `vehicle` | a vehicle whose yaw rate changes sign (both over 2 deg/s) while under 0.5 m/s more than 3 times within 10 s | as asked |
| battle-end-not-abrupt | `end` | the side broke (`broke_at`) and the card came under 10 s of real time later with no offer to go on (`offered_continue` not true) | the break's real time is read from lines carrying both `t` and `bt`; an end by limit passes |
| look-matches-place | `look` | the look's year or month differs from the scene's (shots.yaml), or snow / bare trees in high summer (Jun-Aug north, Dec-Feb south, under 60 degrees latitude) | with no scene date or place to compare: not-measured, the look still reported for coverage |

## check_night.py: one night to night-check.json

`check_night.py NIGHT_DIR [--rules rules.yaml] [--shots scenes/shots.yaml] [--out WORK/night-check.json]
[--img-dir WORK/img] [--look-scores] [--only a,b]`

Scenes are the union of night.json's, the folders and shots.yaml's. For each scene:

- `ran.status`: `ran`, `crashed` (night.json says so, or the log has crash lines), `timed out`, `stopped` (the owner
  came back), `ended with an error` (exit code), `did not run` (with why: the night stopped before it, or it was not
  played). night.json with a stray quote in a note is read line by line.
- `steps`: counts of DID / SAID WHY / DEAD / MUTE / MISSING / ERROR, and every line that is not DID (or whose expect
  failed). Lines after the first MISSING `start`, `select-unit` or `press` are marked `suspect` (they may fail only
  because of it).
- `log`: from game.log.gz, each distinct line once with how many times: `LogTheater: Error`, `LogTheater: Warning`,
  failed ensures, crash lines (critical / fatal error, assertion, unhandled exception, signals); other categories'
  errors are only counted.
- `speed`: from steps.jsonl (fps_low, fps_mid, game/draw/chip ms), the log's `Speed:` lines (frames a second, worst
  frame, long frames and what bound the longest: game, draw or graphics chip), land battle and photographer lines,
  and the cam facts' frame times (hitches over 50 / 100 ms). `fps` is the lower of the log's median and the steps'.
- `strips`: strip frames grouped by their `of` (the `strip` lines; else the file name `strip-<of>-<NNNN>.jpg`),
  measured with the `strip` lines' real times, judged against every rule with `clip:` whose tags fit (`strip`, plus
  the tags of the still the strip follows). Each group gets a contact sheet (`strip-<of>-sheet.jpg`) and, when it has
  faults, a sheet readable on a phone (`strip-<of>-faults.jpg`: before / the faulty frame / after, worst three).
- `facts`: the checks named by the rules that fit the scene, with the scene's theater, year and month.
- `pictures`: every still with its tags (shots.yaml; an unlisted picture gets its scene kind's broad tag, `map`,
  `air-battle`, `land-advance` (the rules' broad land tag) or `sea-battle`), a thumbnail at most 640 px wide.
- `rules`: per rule that fits the scene (its tags meet the scene's tags; `every-picture` fits all, `every-battle`
  every battle): `status` from its clip and facts parts (`fail` if either fails, `pass` if one passes, else
  `not-measured`; `judges` for a picture-only rule), each part's words and evidence (sheet, the nearest still when
  taken within 15 s of the moment).

`judge_list`: for every still, the rules whose `picture` question applies to its tags (`{id, title, question}`), its
scene, place, year, what the shot should show, and `score_look` (shots.yaml `look: true`, or `--look-scores`).

### shots.yaml

```yaml
scenes:
  air-raid:                 # the scene's folder name (as in scenes.txt)
    kind: air               # map | air | land | sea (else read from the facts' run line or the flags)
    theater: T1             # the Look Bible's theater code
    year: 1940
    month: 8                # optional
    about: A German raid on Hampshire, August 1940     # the scene's name on the morning page
    pictures:
      - {file: "*00-flyover-raid*", tags: [air-flyover], about: "the raid's opening flyover", look: true}
      - {file: "step-00[12].png", tags: [map, map-fleet-sailing], about: "fleets sailing"}
```

`file` is a file name or a glob (on the name, with or without its extension). `look: true` asks the judge for the
Look Bible score of that picture.

## The judges: judge_prompt.md and verify_prompt.md

The first judge answers each listed rule PASS / FAIL / CANT_TELL for one picture, with where (words, and a box
`[left, top, right, bottom]` in percent of the picture) and one plain sentence; strict on evidence (only FAIL what is
visible; when in doubt, CANT_TELL); the Look Bible's nine lines 0/1/2 only when asked. The second judge sees only the
FAILs and tries to refute each; unsure means REFUTED. report.py shows a picture FAIL only when it is CONFIRMED
(`--trust-unverified` shows unchecked ones too); the others are counted on the page as left out. Both JSON shapes are
in the prompts.

## report.py: the page, routing.json, history.json

`report.py WORK/night-check.json --verdicts judge.json --verify verify.json --history history.json --out-dir DIR
[--night-dir NIGHT_DIR] [--trust-unverified] [--no-history-write]`

Per rule and scene: **fail** if a machine part failed or a picture FAIL was confirmed; **pass** if a part passed or a
picture passed; else **not checked**. Against the last night in history.json before tonight: NEW (failing now, not
then), STILL THERE (failing both; "since" the first night of the run), FIXED (failing then, passing now); failing then
but not checked tonight is said so, never called fixed.

The page (`index.html`, title "Film Critic Report"): one sentence on how the night went; New problems (each a card:
the picture with the judge's box drawn on a copy, or the strip's faulty frames, or the nearest still; the rule's
title; one or two plain sentences; "from your find on <date>" when the rule came from Brian; "sent to <team>");
Still there; Fixed since last night; What didn't run and why (scenes, steps that did nothing, the game's errors,
checks that could not be made, last night's problems that could not be checked, FAILs left out); every scene in a
table; Look scores when asked. Inline CSS with light and dark colours, 16 px side margins, no sideways scrolling at
390 px. `publish-files.json` lists `index.html` and every image it uses (paths relative to the page).
`routing.json`: per owner key of rules.yaml, `{who, covers, night, faults: [{rule, title, scene, status, since,
weight, from, picture, box, words, machine}]}`.

## Tests

`tests/make_synthetic_night.py` makes a night with planted faults (a camera jump, zoom pop and hitch; an opening with
no defenders; a bare model; an abrupt end; a strip with a 70 px lurch and a flash; a fleet with its bow against its
course and every wake drawn left; men all running alike; a tank rocking on the spot; snow in July; a crash; a scene
that never ran). `tests/rules-test.yaml` is the four stub rules plus one rule per other facts check.
`tests/make_fake_verdicts.py` plays both judges (one FAIL refuted) and writes last night's history.
`tests/check_outputs.py` asserts what must be found; `tests/screenshot.mjs` takes the screenshots
(`NODE_PATH=/opt/node-tools/node_modules node tests/screenshot.mjs PAGE OUT_DIR`).
