#!/usr/bin/env python3
"""A stand-in for the game, for the bench's dry run in the cloud (bench_dry_run.py; the release 2 ocean plan, step 0).

Tools/sea_bench.sh runs it in the engine's place: it writes down its command line (STAND_IN_CALLS) and writes the log a
sea battle's bench run leaves (STAND_IN_LOG): release 1.5's frame-time line, the threads' line and the "Sea look:" line,
in the game's own words (bench_table.py's copies, which its self-test holds to the source), the look's two lines as
measure_test made them from Battle/SeaLookSwitches (STAND_IN_LOOKS, its measure_lines.txt): a tier asked for, or step
1(a)'s -TheaterSeaMarksPool=1 (the line saying it in force, asked). Every run of a battle gives the same numbers, so an
A/A block differs by nothing. Asked for the tier in STAND_IN_HANG, it hangs and will not be
asked to stop (it ignores the polite stop), as a hung game thread does. Asked for pictures ("-TheaterSeaBattleShots=",
a replay of 22 November's picture runs), it writes them into the folder, 24 for a water sheet and 10 for a battle's own
shots, and says release 1.5's line as 22 November's was, its pictures in it.
"""

import os
import signal
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import bench_table  # noqa: E402  (beside it; python3 -I leaves the script's own folder off the path)


def value(args, name):
    for a in args:
        if a.startswith("-" + name + "="):
            return a.split("=", 1)[1]
    return None


def main():
    args = sys.argv[1:]
    with open(os.environ["STAND_IN_CALLS"], "a", encoding="utf-8") as f:
        f.write("\t".join(args[1:]) + "\n")
    tier = value(args, "TheaterSeaLook")
    if tier is not None and tier == os.environ.get("STAND_IN_HANG"):
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        time.sleep(600)
        return 0
    looks = {}
    with open(os.environ["STAND_IN_LOOKS"], encoding="utf-8") as f:
        for line in f:
            row = line.rstrip("\n").split("\t")
            if row[0] == "look":
                looks[row[1]] = row[2]
    look = looks["0||"] if tier is None else looks["0|-TheaterSeaLook=%s|" % tier]
    if tier is None and value(args, "TheaterSeaMarksPool") == "1":
        look = looks["0||-TheaterSeaMarksPool=1 (asked)"]
    battle = os.path.basename(value(args, "TheaterSeaBattle") or "?")
    shots = value(args, "TheaterSeaBattleShots")
    pictures = 0
    if shots:
        pictures = 24 if "-TheaterSeaWaterSheet" in args else 10
        os.makedirs(shots, exist_ok=True)
        for k in range(pictures):
            with open(os.path.join(shots, "%s-%02d.png" % (battle, k)), "wb") as f:
                f.write(b"\x89PNG stand-in")
    bench = (value(args, "TheaterSeaBench") or "1,end").split(",")
    start = float(value(args, "TheaterSeaBattleFrom") or 0.0)
    size = (value(args, "TheaterWindow") or "2881x1786").split("x")
    drop = float(bench[0])
    chip = 8.0 + len(battle) / 4.0
    nums = [14000, drop, start + drop + 1.0, start + 430.0, 5.0, 6.0, 7.0, 2.0, 2.4, 2.8, 1.5, 1.8, 2.1, chip, chip * 1.2, chip * 1.4,
            chip, chip * 1.2, chip * 1.4, chip + 0.6, chip * 1.2 + 0.6, chip * 1.4 + 0.6, 70, 0.5, 2.0, 30.0, 52.0, 80.0]
    stamp = "[2026.11.30-09.08.00:000][900]LogTheater: Display: "
    frames = bench_table.FRAMES % (21000, 430.0, int(size[0]), int(size[1]), chip + 3.0, 1000.0 / (chip + 3.0), chip, chip * 1.5, chip * 2.0, chip * 3.0, 0, 0)
    if pictures:
        # (a picture run: release 1.5's line as 22 November's for the battle, its frames and seconds)
        mean, p95, p99 = bench_table.BEFORE.get(battle, (chip + 3.0, chip * 1.5, chip * 2.0))
        made = bench_table.BEFORE_RUNS.get(battle, (pictures, 3000, 30, ""))
        frames = bench_table.FRAMES % (made[1], made[2], int(size[0]), int(size[1]), mean, 1000.0 / mean, mean - 0.1, p95, p99, p99 * 3.0, 25, pictures)
    lines = ["[2026.11.30-09.00.00:000][  0]LogTheater: Display: Sea battle: playing %s." % battle,
             stamp + frames,
             stamp + bench_table.fill(bench_table.THREADS, [round(x, 2) for x in nums], ["", ""]),
             stamp + look]
    log = os.environ["STAND_IN_LOG"]
    os.makedirs(os.path.dirname(log), exist_ok=True)
    with open(log, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
