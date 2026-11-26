# Film critic: the agreed formats (read this before changing any part)

Three parts meet here: the game (writes facts and film strips in a critic run), the night run on the Mac
(night.sh, scenes/), and the morning check in the cloud (check_night.py, report.py, rules.yaml).

## 1. Command-line flags a critic run passes to the game

- `-FilmCritic` is on EVERY run night.sh starts (it also tells the Mac builder's watchdog and night.sh that the game
  window is a test run, not Brian's game). In the game it turns on the facts file (below). Without it the game
  behaves exactly as today: nothing is written, nothing is measured, no frame is slower.
- `-FilmCriticStrip=<fps>,<seconds>` (optional) asks for film strips: short runs of consecutive frames saved as
  JPEG, 960 px wide, for measuring smoothness, pops and flicker by pixel math.
  - In a step run: during every `air-wait` and `land-wait` step, and for <seconds> after every `press`/`order-sea`
    step's act: `strip-<step NNN>-<frame NNNN>.jpg`.
  - In a battle scene run (-TheaterLandBattle / -TheaterAirBattle / -TheaterSeaBattle / -TheaterLandScript with
    their Shots folders): for the first <seconds> of the opening shot (the flyover in the air), and for <seconds>
    after each named still: `strip-<still name>-<frame NNNN>.jpg`.
  - At most 1500 strip frames a run; each frame's real time is also written to the facts file (`strip` lines).

## 2. The facts file: `<the run's picture folder>/critic-facts.jsonl`

One JSON object a line. Every line has `t` (real seconds since the game began), `k` (the kind) and, in a battle,
`bt` (battle seconds). Lengths in metres, angles in degrees, screen points in pixels of the game window.

| k | when | fields |
|---|---|---|
| `run` | once at the start | `scene` (map, air, land, sea), `key` (recording or step file), `build` (if known), `window` [w,h] |
| `cam` | every frame while a scene draws (map: every frame while the camera moves, and once a second at rest) | `scene`, `eye` [x,y,z], `rot` [pitch,yaw,roll], `fov`, `dt` (this frame's length, ms), `cut` (true on a declared cut), `why` (the shot's name or reason, when known), `subject` (what the shot looks at, when known) |
| `still` | at each named still or step picture | `name`, `file`, and the same camera fields as `cam` |
| `strip` | at each strip frame | `file`, `of` (the step or still it belongs to) |
| `fleet` | each fleet piece on screen, at every picture, and every 0.5 s while one sails | `id`, `name`, `nation`, `sailing`, `progress` (0..1 while sailing), `course` [x,y] unit vector or null, `bow` [x,y] unit vector, `wake` (0 none, 1 patrol, 2 passage), `camyaw`, `bowpx` [x,y], `tailpx` [x,y] |
| `planes` | air scenes: every 0.25 s in the opening shot, and at each still | `shot`, `inframe` {side: count} with side `attack` / `defend`, `nearest_m` {side: metres} |
| `men` | land scenes: every second | `moving`, `run_p10`, `run_p50`, `run_p90` (speeds of running men, m/s), `rates` (how many distinct gait clips or rates are playing among moving men), `capped` |
| `vehicle` | land scenes: every 0.5 s per vehicle in sight | `id`, `type`, `nation`, `heading`, `speed`, `turret` |
| `marks` | once per model drawn (land vehicles, aircraft, ships; men where the game marks them) | `kind` (tank, plane, ship, man), `type`, `nation`, `year`, `month`, `marks` (how many marking decals or tags it carries) |
| `look` | once per battle | `look`, `month`, `year`, `lat`, `lon`, `snow`, `leafless`, `hour`, `weather` |
| `end` | once, when a battle ends | `how` (rout, limit, both broke, ...), `broke_at` (battle s the losing side broke, or null), `ended_at`, `card_at` (real s), `offered_continue` (true/false/null if the game has no such offer) |

A kind the game cannot give yet is simply absent: the checker treats a missing kind as "not measured", never as a
pass or a failure.

## 3. Night folder layout on the Mac

`~/Total War WW2/unreal/TheaterCommand/Saved/FilmCritic/nights/<YYYY-MM-DD>/` (the date the evening began):
- `attempts.log` (every half-hourly try, and why it did not run), `night.json` (what ran), `DONE`
- one folder and one zip per scene: `<scene>/` with the pictures, `steps.jsonl` (step runs), `critic-facts.jsonl`,
  `strip-*.jpg`, `game.log.gz` (the game's own log), `flags.txt`

## 4. Rules (`rules.yaml`) and shots (`scenes/shots.yaml`)

A rule says what must always be true, in plain words, where it came from, which shot tags it applies to, and how
it is checked: `picture` (a question a vision judge answers PASS / FAIL / CAN'T TELL from a still), `clip` (a
measure on strips: jump, pop, freeze, flicker, blank, stutter, zoom, with limits), `facts` (a named check in
check_facts.py over the facts file). Shots carry tags, so each picture is judged only against the rules that fit it.

The morning check's own formats (night-check.json, the judges' JSON, history.json, routing.json, the shots.yaml
fields) and every measure's limit are in CHECKER.md.
