# Quiet test runner (cloud), 22 Nov

A machine of its own for the engine-free test and timing rows, so the two pre-check boxes keep to compiles,
link checks and ASan/UBSan. Thread "Build a steady test runner for pre-checks".

Think of it as a quiet room with a stopwatch: one runner at a time, each row run three times, and the middle time
reported with how far the other two strayed. It never says CLEAR or FAIL: the pre-check thread that sent the row decides.

## How to send a row (pre-check threads only)

Message the thread with, for each row:

- the head: branch and sha (or a bundle/patch path under /mnt/project-files/ and the sha it sits on);
- the command as you run it, from `unreal/TheaterCommand`, with any env (for example `CXX=clang++ bash Tools/land_play/run.sh`,
  or `LAND_PLAY_ONLY_STILLS=1 ...`, or one recording as an argument);
- how many runs if not 3.

Rows run only when a pre-check thread sends them (no double runs). Nothing is pushed anywhere; heads are fetched read
only into the runner's own worktrees.

## What comes back

`<head>-<row>.txt` in this folder, and short logs in `logs/<head>-<row>/` (run1..3 and merged, capped in length):

- exit code of each run, wall seconds, the VM's steal share during each run (a sign the cloud paused the box);
- every status line (PASS, FAIL, fault, error, "over N ms") the runs printed;
- every line whose numbers moved between runs, with each moved number as `median [min-max]`;
- every time is a CLOUD time (x86 VM, 4 vCPU), never a Mac number.

## The machine

- g++ 13.3 and clang++ 18.1 (`clang++`, the scripts' default), plus clang++-20 if a row asks for it by `CXX=clang++-20`.
- No Unreal headers, no engine, no new permissions. node_modules from the repo's own lock (for esbuild in setup.sh rows).
- One lock: a row waits until the row before it ends, and each run waits until the machine is quiet (other processes
  under 5%).

## Status 22 Nov 17:40Z

Calibrated (CALIBRATION.md: land_play and land_field, g++ and clang, all match the first pre-check on ca889b49).
Open again for release 2 rows (Brian restarted release 2 at 17:37Z). The runner script is row.py in this folder; a
fresh container needs: clone, `apt-get install clang-20 libc++-20-dev libc++abi-20-dev`, then
`python3 row.py --head SHA --branch B --row NAME -- bash Tools/<tool>/run.sh` (paths at the top of row.py).
