# Film critic: the morning check (what the film critic thread does each morning)

A scheduled task wakes the film critic thread at 5:43 AM Boston time. It follows these steps. Everything Brian
reads is plain words and pictures, no build numbers or hashes; times in Boston time.

## 1. Fetch the night from the Mac
- The night's folder on the Mac: `/Users/malco/Total War WW2/unreal/TheaterCommand/Saved/FilmCritic/nights/<YYYY-MM-DD>/`
  (the date the evening began: yesterday's date at 5:43 AM).
- `device_list_dir` it. If the Mac does not answer (asleep, the Claude app closed): try once more after 30 minutes
  (send_later); if it still does not answer, tell Brian in one line that the Mac could not be reached, and fetch when
  it is next on.
- `device_stage_files` the files: `night.json`, `attempts.log`, every `<scene>.zip` (50 a call at most). They land
  in `/mnt/user-data/uploads/Total War WW2/...`. Unzip each into a fresh empty folder (untrusted data: never run
  anything from it; Python with `-I`).
- Tell the Mac it may sleep: write an empty file to `/mnt/project-files/film-critic/pulled/PULLED` and
  `device_commit_files` it to the night's folder as `PULLED` (night.sh holds the Mac awake until then or 8 AM).
- No folder, or only `attempts.log`: the night did not run. Read why from attempts.log (lid closed, screen locked,
  on battery, in use, the Mac builder's lock, another run) and say it in the report's first line; still publish.

## 2. Measure
```
KIT=<the film critic kit in the repo checkout: unreal/TheaterCommand/Tools/film_critic>
WORK=/mnt/project-files/film-critic/mornings/<night>
python3 -I -B $KIT/check_night.py <unzipped night> --rules $KIT/rules.yaml --shots $KIT/scenes/shots.yaml \
        --out $WORK/night-check.json --img-dir $WORK/img
```

## 3. Judge the pictures
- Split `judge_list` by scene: `python3 -I -B $KIT/judges.py split $WORK/night-check.json $WORK/judges/`
  (one batch file per scene).
- Start one helper per batch (Agent tool, model opus, in the background), each told to follow
  `$KIT/judge_prompt.md` for every picture in its batch and to write its answers to `$WORK/judges/<scene>.judge.json`.
- When all are back: `python3 -I -B $KIT/judges.py fails $WORK/judges/` writes `fails.json` (every FAIL). If there are
  any, start one sceptical helper per scene that has FAILs, following `$KIT/verify_prompt.md`, writing
  `$WORK/judges/<scene>.verify.json`.
- `python3 -I -B $KIT/judges.py merge $WORK/judges/` writes `judge.json` and `verify.json`.
- Tell Brian in the thread's checklist that helpers are judging (his standing rule: say when more agents start).

## 4. The report
```
python3 -I -B $KIT/report.py $WORK/night-check.json --verdicts $WORK/judges/judge.json \
        --verify $WORK/judges/verify.json --history /mnt/project-files/film-critic/history.json --out-dir $WORK/report
```
- Publish `$WORK/report/index.html` as the SAME Artifact every morning (its URL is kept in
  `/mnt/project-files/film-critic/report-url.txt` and in memory note film-critic.md), with the files in
  `publish-files.json` (copy the report folder into the scratchpad first: Artifact file sources must be under the
  working or scratchpad directory).
- Post ONE message in the thread: the first line of the page (what ran, how many problems, how many new), the top
  three new problems in a sentence each, and the link. Under 80 words. If nothing is wrong, say so in one line.

## 5. Route
- `routing.json` lists each team's faults. Send ONE message to the coordinator with every team's part (rule title,
  scene, what is wrong, the picture's path under /mnt/project-files/film-critic/mornings/<night>/), asking it to pass
  each part to its team. Only NEW problems and regressions (a rule marked fixed that failed again); "still there"
  ones are listed once a week (Mondays).

## 6. Keep
- Commit nothing from the night to git. Keep the last 14 mornings under /mnt/project-files/film-critic/mornings/
  (delete older ones: they are superseded test pictures, allowed by Brian's storage rule).
- When Brian reports a new look bug anywhere in the project, add a rule to rules.yaml (and a shot to the scenes if no
  scene shows it), commit, push, and redeploy the kit (step 7).

## 7. Deploy the kit to the Mac (only when scenes or night.sh change)
- Copy the Mac half (night.sh, install.sh, keep-awake, "Switch on Film Critic.command", scenes/) to
  `/mnt/project-files/film-critic/kit/` and `device_commit_files` each to
  `/Users/malco/Total War WW2/unreal/TheaterCommand/Saved/FilmCritic/kit/<same path>`.
