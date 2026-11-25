#!/usr/bin/env python3
"""The bench's dry run (the release 2 ocean plan, step 0): Tools/sea_bench.sh itself, run in the cloud with a stand-in in
the engine's place (stand_in_game.py), and held to what the Mac run is to do:
  - a battle whose recording cannot be read stops the bench before its first run;
  - the warm-up first; then the replays of release 1.5's 22 November picture runs, five rounds of the three in turn, each
    as it was made (two-lines and light-ships "-TheaterSeaWaterSheet -TheaterSeaBattleShots=OUT/shots/<order>", samar
    "-TheaterSeaBattleShots=OUT/shots/<order>"), in the game's own window whatever THEATER_WINDOW says, with no
    -TheaterSeaBench, tier, start or SEA_BENCH_ARGS, their pictures not kept; SEA_BENCH_REPLAY=0 for none;
  - then each round's pair, tier 0 and the tier, the order turned about every round, each run with its block, round, role,
    which ran first, its start and the heat in runs.tsv;
  - by default the game's own window and sound (no -TheaterWindow, no -nosound) and the run to the battle's end
    ("-TheaterSeaBench=60,end"); SEA_BENCH_FROM starts a battle later;
  - the table pairs each tier run with its own round's tier 0, counts nothing in an A/A block that differs by nothing,
    and reads 22 November's lines only beside their replays (known by their pictures), never beside the bench's runs;
  - a run that hangs and will not stop is killed, said "overran, killed", and left out of the table, and the bench goes on;
  - step 1(a)'s A/B, SEA_BENCH_TIERS=0 SEA_BENCH_HELD_ARGS=-TheaterSeaMarksPool=1: the switch asked of the held runs
    alone, never of the plain tier 0 runs, and the table pairs each held run, labelled by its "Sea look:" line, with the
    plain tier 0 run of its own round.

Usage: python3 -I -B Tools/sea_waves/bench_dry_run.py MEASURE_LINES OUT
       (MEASURE_LINES: measure_test's measure_lines.txt, for the "Sea look:" lines; OUT: a folder to work in)
"""

import os
import shutil
import stat
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
BENCH = os.path.join(ROOT, "Tools", "sea_bench.sh")


def main(lines_path, out):
    n, failed = 0, 0

    def check(what, ok, got):
        nonlocal n, failed
        n += 1
        failed += 0 if ok else 1
        print("  %s  %s: %s" % ("ok  " if ok else "FAIL", what, got))

    work = os.path.join(out, "bench_dry_run")
    shutil.rmtree(work, ignore_errors=True)
    engine = os.path.join(work, "UE")
    binary = os.path.join(engine, "Engine", "Binaries", "Mac", "UnrealEditor.app", "Contents", "MacOS", "UnrealEditor")
    os.makedirs(os.path.dirname(binary))
    with open(binary, "w", encoding="utf-8") as f:
        f.write('#!/bin/bash\nexec python3 -I -B "%s" "$@"\n' % os.path.join(HERE, "stand_in_game.py"))
    os.chmod(binary, os.stat(binary).st_mode | stat.S_IXUSR)
    home = os.path.join(work, "home")
    os.makedirs(home)

    def bench(name, extra):
        calls = os.path.join(work, name + "-calls.txt")
        env = {k: v for k, v in os.environ.items() if not k.startswith(("SEA_BENCH_", "THEATER_"))}
        env.update({"UE_ROOT": engine, "HOME": home, "STAND_IN_CALLS": calls, "STAND_IN_LOOKS": lines_path,
                    "STAND_IN_LOG": os.path.join(home, "Library", "Logs", "TheaterCommand", "TheaterCommand.log")})
        env.update(extra)
        folder = os.path.join(work, name)
        done = subprocess.run(["bash", BENCH, folder], env=env, cwd=ROOT, capture_output=True, text=True, timeout=300)
        said = [l.split("\t") for l in open(calls, encoding="utf-8").read().splitlines()] if os.path.exists(calls) else []
        return done, folder, said

    def rows_of(folder):
        path = os.path.join(folder, "runs.tsv")
        return [l.split("\t") for l in open(path, encoding="utf-8").read().splitlines() if not l.startswith("#")] if os.path.exists(path) else []

    # a battle with no recording: nothing run
    done, folder, said = bench("missing", {"SEA_BENCH_BATTLES": "two-lines no-such-battle", "SEA_BENCH_WARM": "0"})
    check("a battle whose recording cannot be read stops the bench before its first run", done.returncode == 2 and not said and "no-such-battle: no recording" in done.stdout,
          "%d, %d runs: %s" % (done.returncode, len(said), done.stdout.strip()[:120]))

    # the bench as the Mac runs it, two battles (samar started later in its battle), five rounds, a short warm-up, and
    # 22 November's picture runs replayed (five rounds unless told)
    done, folder, said = bench("main", {"SEA_BENCH_BATTLES": "two-lines samar", "SEA_BENCH_WARM": "2", "SEA_BENCH_FROM": "samar=150"})
    rows = rows_of(folder)
    check("the bench ran whole: a warm-up, fifteen replays and twenty runs, each done, its table written", done.returncode == 0 and len(said) == 36 and len(rows) == 35
          and all(r[4] == "done" for r in rows) and os.path.exists(os.path.join(folder, "bench_table.txt")), "%d, %d calls, %d rows" % (done.returncode, len(said), len(rows)))
    warm = said[0] if said else []
    check("the warm-up first: samar, counted from 1 s for its seconds, from where samar is told to start", "-TheaterSeaBattle=samar" in warm and "-TheaterSeaBench=1,2" in warm and "-TheaterSeaBattleFrom=150" in warm,
          " ".join(warm[2:]))
    replays, counted = said[1:16], said[16:]
    made = {"two-lines": ["-TheaterSeaWaterSheet"], "light-ships": ["-TheaterSeaWaterSheet"], "samar": []}
    want = [["-game", "-windowed", "-unattended", "-nosplash", "-TheaterSeaBattle=" + b] + made[b] + ["-TheaterSeaBattleShots=%s/shots/%03d" % (folder, k)]
            for k, b in enumerate(["two-lines", "light-ships", "samar"] * 5)]
    check("then 22 November's picture runs replayed as made, five rounds of the three in turn: the sheets -TheaterSeaWaterSheet -TheaterSeaBattleShots, samar its shots alone;"
          " the game's own window, no -TheaterSeaBench, tier, start or speed",
          replays == want, " ".join(replays[2][2:]) if len(replays) > 2 else "none")
    check("the replays' pictures written and not kept, the scratch folder gone", not os.path.exists(os.path.join(folder, "shots"))
          and done.stdout.count("24 pictures written, not kept") == 10 and done.stdout.count("10 pictures written, not kept") == 5, "%d sheets" % done.stdout.count("24 pictures written"))
    check("every bench run in the game's own window and with sound (the Mac's output muted), at speed 1, played to the battle's end after 60 s",
          counted and all(not any(a.startswith("-TheaterWindow") or a == "-nosound" or a.startswith("-TheaterSeaBattleShots") for a in c)
                          and "-TheaterSeaBench=60,end" in c and "-TheaterSeaBattleSpeed=1" in c for c in counted),
          " ".join(counted[0][2:]) if counted else "none")
    check("samar started where SEA_BENCH_FROM says, two-lines at its start",
          counted and all(("-TheaterSeaBattleFrom=150" in c) == ("-TheaterSeaBattle=samar" in c) for c in counted), "%d runs" % len(counted))
    check("runs.tsv: thirteen columns a run, the heat not told here (-1)", rows and all(len(r) == 13 and r[10:] == ["-1", "-1", "-1"] for r in rows), "%d rows" % len(rows))
    check("runs.tsv: the replays tier 0, no block, their round, role replay",
          [(r[1], r[2], r[5], r[6], r[7], r[8]) for r in rows[:15]] == [(b, "0", "", str(k // 3 + 1), "replay", "") for k, b in enumerate(["two-lines", "light-ships", "samar"] * 5)]
          and all(r[3].endswith("-t0-replay.log") for r in rows[:15]), "%s" % [(r[1], r[6], r[7]) for r in rows[:3]])
    bench_rows = rows[15:]
    abba = len(bench_rows) == 20
    for k in range(0, len(bench_rows) - 1, 2):
        a, b = bench_rows[k], bench_rows[k + 1]
        rnd = int(a[6])
        want_roles = ("base", "held") if rnd % 2 == 1 else ("held", "base")
        abba = abba and a[1] == b[1] and a[5] == b[5] == "1" and a[6] == b[6] and (a[7], b[7]) == want_roles and a[8] == b[8] == want_roles[0] \
            and (a[2], b[2]) == (("0", "1") if want_roles[0] == "base" else ("1", "0"))
    check("each round a pair a battle, tier 0 first in odd rounds and the tier first in even ones, each run's block, round, role and order said",
          abba, "%s" % [(r[1], r[6], r[7]) for r in bench_rows[:4]])
    asked = [("-TheaterSeaLook=1" in c) for c in said[1:]]
    check("tier 1 asked of the held runs only, tier 0 by asking nothing", asked == [r[2] == "1" for r in rows], "%d asked" % sum(asked))
    about = open(os.path.join(folder, "about.txt"), encoding="utf-8").read() if os.path.exists(os.path.join(folder, "about.txt")) else ""
    check("about.txt says the window, the sound, the replays and that the output could not be muted here", "window: the game's own" in about and "sound: muted" in about
          and "could not be muted" in about and "samar: 435 s of battle, played from 150 s" in about and "replays: 5 rounds" in about and "22 November's sound not known" in about,
          about.replace("\n", " | ")[:200])
    text = open(os.path.join(folder, "bench_table.txt"), encoding="utf-8").read() if os.path.exists(os.path.join(folder, "bench_table.txt")) else ""
    check("the table pairs each round, five pairs a battle, and counts nothing in an A/A block that differs by nothing",
          text.count("against plain tier 0 of the same round, at 2881x1786: 5 pairs (rounds 1, 2, 3, 4, 5; tier 0 ran first in 3, tier 1 in 2)") == 2 and "COUNTS" not in text,
          "%d blocks" % text.count("5 pairs (rounds"))
    check("22 November's lines read beside their replays only, five each at 2881x1786 with 22 November's pictures, none flagged; never beside the bench's runs",
          text.count("tier 0 at 2881x1786, 24 pictures: 5 runs") == 2 and text.count("tier 0 at 2881x1786, 10 pictures: 5 runs") == 1
          and "22 November 9.5, +0.00: within twice the spread" in text and "22 November 9.2, +0.00: within twice the spread" in text
          and "22 November 14.2, +0.00: within twice the spread" in text and "0 of 9 of release 1.5's 22 November lines flagged" in done.stdout
          and "release 1.5 on 22 November" not in text and text.count("(22 November's lines were picture runs, not whole battles") == 2, "two-lines, light-ships, samar")

    # SEA_BENCH_REPLAY=0: no replays
    done, folder, said = bench("noreplay", {"SEA_BENCH_BATTLES": "light-ships", "SEA_BENCH_WARM": "0", "SEA_BENCH_RUNS": "1", "SEA_BENCH_REPLAY": "0"})
    rows = rows_of(folder)
    check("SEA_BENCH_REPLAY=0: no replays, the bench's pair alone", len(said) == 2 and len(rows) == 2 and not any(any(a.startswith("-TheaterSeaBattleShots") for a in c) for c in said)
          and "replays:" not in open(os.path.join(folder, "about.txt"), encoding="utf-8").read(), "%d calls" % len(said))

    # a run that hangs: stopped, killed when it will not stop, said, left out; the bench goes on to its table. One replay
    # round beside it: the game's own window though THEATER_WINDOW says another, and the bench's sound (off)
    done, folder, said = bench("hang", {"SEA_BENCH_BATTLES": "light-ships", "SEA_BENCH_WARM": "0", "SEA_BENCH_RUNS": "1", "SEA_BENCH_DROP": "0",
                                        "SEA_BENCH_SPAN": "1", "SEA_BENCH_SLACK": "0", "STAND_IN_HANG": "1", "THEATER_WINDOW": "1600x1000",
                                        "SEA_BENCH_SOUND": "off", "SEA_BENCH_REPLAY": "1"})
    rows = rows_of(folder)
    check("a run that hangs and will not stop is killed, said so, and the bench goes on to its table",
          len(rows) == 5 and all(r[4] == "done" for r in rows[:4]) and rows[4][4].startswith("overran, killed") and "runs left out:" in done.stdout
          and "light-ships run 4 (asked tier 1): overran, killed" in done.stdout
          and done.returncode == 1, "%s" % [r[4] for r in rows])
    check("THEATER_WINDOW and SEA_BENCH_SOUND=off asked for: the bench's runs in the window given and the replays in the game's own, all without sound, said in about.txt",
          len(said) == 5 and all("-TheaterWindow=1600x1000" in c and "-nosound" in c for c in said[3:])
          and all(not any(a.startswith("-TheaterWindow") for a in c) and "-nosound" in c and any(a.startswith("-TheaterSeaBattleShots=") for a in c) for c in said[:3])
          and "sound: off" in open(os.path.join(folder, "about.txt"), encoding="utf-8").read(),
          " ".join(said[0][2:]) if said else "none")
    check("one replay round: under five said, 22 November's line beside it with no spread to hold it to, 22 November's sound not known and the replays' said",
          "1 rounds of 22 November's picture runs; the table holds 22 November to 5 at the least" in done.stdout and "one run: no spread to hold it to" in done.stdout
          and "22 November's sound is not known (no command line was kept); these replays' sound: off" in done.stdout and "not shown beside runs at" not in done.stdout, "light-ships")

    # step 1(a)'s A/B on the Mac: block 0, plain tier 0 against tier 0 with -TheaterSeaMarksPool=1 (the held runs' own
    # switch, SEA_BENCH_HELD_ARGS), two-lines and samar, five rounds, no replays
    done, folder, said = bench("marks", {"SEA_BENCH_BATTLES": "two-lines samar", "SEA_BENCH_WARM": "0", "SEA_BENCH_REPLAY": "0", "SEA_BENCH_TIERS": "0",
                                         "SEA_BENCH_HELD_ARGS": "-TheaterSeaMarksPool=1"})
    rows = rows_of(folder)
    held = [("-TheaterSeaMarksPool=1" in c) for c in said]
    check("SEA_BENCH_TIERS=0 with SEA_BENCH_HELD_ARGS: twenty runs, the switch asked of the held runs alone, no tier asked of any, block 0 said",
          done.returncode == 0 and len(said) == 20 and len(rows) == 20 and held == [r[7] == "held" for r in rows] and sum(held) == 10
          and not any(any(a.startswith("-TheaterSeaLook") for a in c) for c in said) and all(r[2] == "0" and r[5] == "0" for r in rows)
          and "the held runs alone also: -TheaterSeaMarksPool=1" in open(os.path.join(folder, "about.txt"), encoding="utf-8").read()
          and "block 0, tier 0 against tier 0 with -TheaterSeaMarksPool=1, 5 rounds" in done.stdout,
          "%d, %d calls, %d with the switch" % (done.returncode, len(said), sum(held)))
    text = open(os.path.join(folder, "bench_table.txt"), encoding="utf-8").read() if os.path.exists(os.path.join(folder, "bench_table.txt")) else ""
    check("the table pairs each held run, labelled by its \"Sea look:\" line, with plain tier 0 of its own round, five pairs a battle, nothing counted where nothing differs",
          text.count("block 0: tier 0 asked, drawn as tier 0, -TheaterSeaMarksPool=1, against plain tier 0 of the same round, at 2881x1786: 5 pairs (rounds 1, 2, 3, 4, 5; "
                     "plain tier 0 ran first in 3, the held run in 2)") == 2 and text.count("(the difference d, the held run less plain tier 0)") == 2
          and "COUNTS" not in text and "no whole pairs" not in text and text.count("tier 0, -TheaterSeaMarksPool=1 at 2881x1786 (5 runs)") == 2,
          "%d blocks" % text.count("block 0: tier 0 asked"))
    print("bench_dry_run: %d checks, %d failed" % (n, failed))
    return failed == 0


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(2)
    sys.exit(0 if main(sys.argv[1], sys.argv[2]) else 1)
