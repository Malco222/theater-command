#!/usr/bin/env python3
"""The bench's table (the release 2 ocean plan, step 0, (d)): reads the logs Tools/sea_bench.sh keeps, one a run, and
tables each tier against the plain tier 0 runs interleaved with it, and the replays of release 1.5's 22 November picture
runs against 22 November's own lines.

Each run's three lines, said once a battle at its end (ASeaBattle::SayFrameTimes):
  - "Sea battle: frame times over ..."  release 1.5's line, the whole frame from 1 s of play, with the window's size and
                                         the pictures a picture run took; in the bench's own runs it is described,
                                         never counted; in a replay it is read beside 22 November's;
  - "Sea battle: threads over ..."      the game, render, RHI and graphics chip times, the most of the three (game, render,
                                         chip: the number the step 15 frame guard watches), the whole frame and the kit's
                                         ground asks, all counted over the same frames, from the bench's drop
                                         (-TheaterSeaBench=drop,span), with the battle's clock they covered;
  - "Sea look: ..."                     the tier in force and the switches asked for, as the scene applied them.
A run is labelled by the "Sea look:" line, by what was drawn, not by what the bench asked: until step 15 builds the tier,
a run asked for tier 1 says tier 0 and "asked for but not built yet", and is tabled as that (an A/A run: its difference
from plain tier 0 is the bench's own noise).

runs.tsv (sea_bench.sh's) gives each run its status, its block (the tier T that block holds to tier 0; none for a
replay), its round, its role (base: plain tier 0; held: tier T; replay: one of 22 November's picture runs again), which of
the pair ran first, its start and the machine's heat before, at most during and after it. Block 0 holds tier 0 to tier 0,
its held runs with the bench's SEA_BENCH_HELD_ARGS: an own switch's A/B (step 1(a)'s -TheaterSeaMarksPool=1, its held runs
labelled "tier 0, -TheaterSeaMarksPool=1" by their "Sea look:" line), or with none an A/A block. A run whose status is not
"done" (it overran, crashed, or left no log) is left out and said, whatever its log says.

For each battle and block, each tier T run is paired with the plain tier 0 run of its own round (one after the other,
the order turned about every round), and the differences d are taken: a difference counts only when the 99% interval
on their mean leaves 0 out, |mean d| > t(0.995, n - 1) x spread of d / root n (the spread: d's standard deviation,
n - 1), with five pairs or more. At five pairs that is about 2.06 times the spread of d (the plan's twice the spread);
with more rounds the interval narrows, so a real cost is more likely to count, not less. Each measure's interval is
printed, with the least difference its pairs resolve (the interval's half-width). Drift across the hours of a bench
(heat, the machine's own work, the charger) falls out of each d. The pooled arms are described only. Runs of different
window sizes are never pooled or paired. Last, plain tier 0 in run order with the machine's heat, and the drift across
the bench.

Release 1.5's 22 November lines (the production lead's, 05:23-05:32Z) were picture runs, not whole battles: two-lines and
light-ships water sheets (-TheaterSeaWaterSheet, 24 pictures each, 35 and 34 s of frames), samar its own shots (10
pictures, 167 s of frames), the camera on the planned shots and skipping ahead between them. No command line was kept,
so their sound is not known. They are read only beside the bench's replays of those same runs (SEA_BENCH_REPLAY), a
replay known by its frame-time line's pictures (more than 0), at 22 November's size and with its number of pictures; a
line is flagged when the replays' mean is more than twice their spread from 22 November's (the plan's check 4: tier 0
against the 22 November baseline). Picture runs are never pooled or paired with the bench's whole-battle runs.

Usage: python3 -I Tools/sea_waves/bench_table.py OUT          (OUT: sea_bench.sh's folder, runs.tsv and logs/)
       python3 -I Tools/sea_waves/bench_table.py --self-test [measure_lines.txt]
"""

import datetime
import math
import os
import random
import re
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(ROOT, "Source", "TheaterCommand")

# The game's own words (the self-test holds these to the source: Battle/SeaFrames.h and Battle/SeaBattle.cpp).
THREADS = ("Sea battle: threads over %d frames from %.0f s of play (the battle's %.0f to %.0f s): game %.2f/%.2f/%.2f ms, "
           "render %.2f/%.2f/%.2f ms, RHI %.2f/%.2f/%.2f ms%s, graphics chip %.2f/%.2f/%.2f ms, the most of the three (game, "
           "render, chip) %.2f/%.2f/%.2f ms, the whole frame %.2f/%.2f/%.2f ms (mean/95th/99th in the hundred); %d frames "
           "(%.1f%%) more than %.0f ms over their slowest thread; the kit's ground asked %.0f times a frame on the mean, %.0f "
           "at the 95th, %.0f at most%s.")
NO_THREADS = "Sea battle: threads: no frames counted from %.0f s of play."
FRAMES = ("Sea battle: frame times over %d frames (%.0f s, %dx%d): mean %.1f ms (%.0f a second), middle %.1f, 95th in the "
          "hundred %.1f, 99th %.1f, worst %.1f; %d frames apart (skips ahead, %d pictures).")
NOT_TIMED = " (the graphics chip read 0 every frame: not timed on this machine)"
RHI_NONE = " (the RHI thread read 0 every frame: its work is in the render thread's)"

# The threads' line's numbers, in its order.
THREAD_KEYS = (["frames", "from", "battle_from", "battle_to"]
               + [a + "_" + b for a in ("game", "render", "rhi", "chip", "most", "whole") for b in ("mean", "p95", "p99")]
               + ["over", "over_share", "over_ms", "asks_mean", "asks_p95", "asks_most"])

# Release 1.5 on the Mac, 22 November 05:23-05:32Z (the production lead's frame-time lines,
# /mnt/project-files/sea-b34a-ride-and-frame-times.txt): mean, 95th, 99th of the whole frame from 1 s of play, ms. Picture
# runs, not whole battles, in the game's own window (98 by 95 hundredths of Brian's desktop); their sound not known.
BEFORE = {"two-lines": (9.5, 11.4, 13.7), "light-ships": (9.2, 11.3, 15.4), "samar": (14.2, 22.8, 31.0)}
# Each one's run: (its pictures, the frames counted, over how many seconds, how it was made).
BEFORE_RUNS = {"two-lines": (24, 3647, 35, "a water sheet, -TheaterSeaWaterSheet"), "light-ships": (24, 3729, 34, "a water sheet, -TheaterSeaWaterSheet"),
               "samar": (10, 11724, 167, "its own shots")}
BEFORE_NOTE = ("samar-warm-grade (-TheaterSeaGrade, a different grade): 17.2/31.2/45.8, 9728 frames over 167 s, 10 pictures;"
               " not a second run of samar as made, and not replayed (the grade's values were not kept)")
BEFORE_SIZE = "2881x1786"

# The pairs a difference needs before it counts (the plan: at least 5 runs a tier); and the replays a 22 November line
# needs before its flag is more than a look.
LEAST = 5

# Student's t at 0.995 (the 99% two-sided interval), by degrees of freedom (pairs - 1); between two listed, the smaller's
# (the wider interval).
T995 = {1: 63.657, 2: 9.925, 3: 5.841, 4: 4.604, 5: 4.032, 6: 3.707, 7: 3.499, 8: 3.355, 9: 3.250, 10: 3.169, 11: 3.106,
        12: 3.055, 13: 3.012, 14: 2.977, 15: 2.947, 16: 2.921, 17: 2.898, 18: 2.878, 19: 2.861, 20: 2.845, 21: 2.831,
        22: 2.819, 23: 2.807, 24: 2.797, 25: 2.787, 26: 2.779, 27: 2.771, 28: 2.763, 29: 2.756, 30: 2.750, 40: 2.704,
        60: 2.660, 120: 2.617}


def t995(df):
    """t(0.995, df), from T995 (nan under 1 degree of freedom)."""
    listed = [k for k in T995 if k <= df]
    return T995[max(listed)] if listed else float("nan")


def interval(d):
    """(mean of d, the half-width of the 99% interval on it): nan half-width under two values."""
    mu, sd = mean_sd(d)
    return mu, (t995(len(d) - 1) * sd / math.sqrt(len(d)) if len(d) > 1 and sd == sd else float("nan"))

# The measures counted, pair by pair, in the table's order: (key, words).
MEASURES = [
    ("most_mean", "the most of the three, mean"), ("most_p95", "the most of the three, 95th"), ("most_p99", "the most of the three, 99th"),
    ("whole_mean", "whole frame, mean"), ("whole_p95", "whole frame, 95th"), ("whole_p99", "whole frame, 99th"),
    ("game_mean", "game thread, mean"), ("game_p95", "game thread, 95th"), ("game_p99", "game thread, 99th"),
    ("render_mean", "render thread, mean"), ("render_p95", "render thread, 95th"), ("render_p99", "render thread, 99th"),
    ("rhi_mean", "RHI thread, mean"), ("rhi_p95", "RHI thread, 95th"), ("rhi_p99", "RHI thread, 99th"),
    ("chip_mean", "graphics chip, mean"), ("chip_p95", "graphics chip, 95th"), ("chip_p99", "graphics chip, 99th"),
    ("over_share", "frames over their slowest thread, %"),
    ("asks_mean", "kit's ground asks a frame, mean"), ("asks_p95", "kit's ground asks a frame, 95th"),
]
# Described only, never counted: release 1.5's line, from 1 s of play (the start's hitches in it).
DESCRIBED = [("frame_mean", "whole frame from 1 s (release 1.5's line), mean"), ("frame_p95", "whole frame from 1 s, 95th"),
             ("frame_p99", "whole frame from 1 s, 99th")]
# The measures the order between a pair's two runs is shown for.
ORDER_KEYS = ["most_mean", "most_p95", "whole_mean"]


def specs(fmt):
    """The printf conversions of a line, in order: 'd', 'f' or 's' ('%%' is a plain '%')."""
    return [m.group(0)[-1] for m in re.finditer(r"%(?:\.\d+)?[dfs]|%%", fmt) if m.group(0) != "%%"]


def pattern(fmt):
    """A printf line as a regular expression: each %d, %.Nf and %s a group, '%%' a '%'."""
    out, i = "", 0
    for m in re.finditer(r"%(?:\.\d+)?[dfs]|%%", fmt):
        out += re.escape(fmt[i:m.start()])
        out += {"d": r"(-?\d+)", "f": r"(-?(?:\d+(?:\.\d*)?|nan|inf))", "s": r"(.*?)", "%": re.escape("%")}[m.group(0)[-1]]
        i = m.end()
    return re.compile(out + re.escape(fmt[i:]))


def fill(fmt, nums, notes):
    """The line again from its numbers and its notes, by its own conversions."""
    vals, n, s = [], iter(nums), iter(notes)
    for kind in specs(fmt):
        vals.append(int(next(n)) if kind == "d" else next(s) if kind == "s" else float(next(n)))
    return fmt % tuple(vals)


THREADS_RE, NO_THREADS_RE, FRAMES_RE = pattern(THREADS), pattern(NO_THREADS), pattern(FRAMES)
THREAD_SPECS = specs(THREADS)
LOOK_RE = re.compile(r"Sea look: tier (\d+)(.*)$")


def read_threads(line):
    m = THREADS_RE.search(line)
    if not m:
        return None
    nums, notes = [], []
    for kind, got in zip(THREAD_SPECS, m.groups()):
        if kind == "s":
            notes.append(got)
        else:
            nums.append(int(got) if kind == "d" else float(got))
    out = dict(zip(THREAD_KEYS, nums))
    out["nums"], out["notes"] = nums, notes
    out["rhi_none"], out["untimed"] = notes[0] == RHI_NONE, notes[1] == NOT_TIMED
    return out


def read_frames(line):
    m = FRAMES_RE.search(line)
    if not m:
        return None
    g = m.groups()
    return {"frame_n": int(g[0]), "frame_s": float(g[1]), "size": "%sx%s" % (g[2], g[3]), "frame_mean": float(g[4]), "frame_mid": float(g[6]),
            "frame_p95": float(g[7]), "frame_p99": float(g[8]), "frame_worst": float(g[9]), "frame_apart": int(g[10]), "pictures": int(g[11])}


def read_look(line):
    """(tier, asked and built, asked and not built) from a "Sea look:" line."""
    m = LOOK_RE.search(line)
    if not m:
        return None
    rest = m.group(2).rstrip(".")
    asked = []
    k = rest.find("in force: ")
    if k >= 0:
        listed = rest[k + len("in force: "):].split("; asked for but not built yet")[0]
        asked = [p[:-len(" (asked)")] for p in listed.split(", ") if p.endswith(" (asked)")]
    not_built = []
    k = rest.find("asked for but not built yet, so nothing changes: ")
    if k >= 0:
        not_built = rest[k + len("asked for but not built yet, so nothing changes: "):].split(", ")
    return int(m.group(1)), asked, not_built


def label(look):
    tier, asked, not_built = look
    out = "tier %d" % tier
    if asked:
        out += ", " + ", ".join(asked)
    if not_built:
        out += " (asked, not built: " + ", ".join(not_built) + ")"
    return out


def read_log(path):
    """A run's numbers from its log: the last of each line (a run says them once)."""
    threads = frames = look = None
    no_threads = False
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if "Sea battle: threads" in line:
                got = read_threads(line)
                threads = got or threads
                no_threads = no_threads or (got is None and NO_THREADS_RE.search(line) is not None)
            elif "Sea battle: frame times over" in line:
                frames = read_frames(line) or frames
            elif "Sea look: tier" in line:
                look = read_look(line) or look
    if threads is None or look is None:
        return None, "no threads' line" + (" (no frames counted)" if no_threads else "") if threads is None else "no \"Sea look:\" line"
    run = dict(threads)
    run.update(frames or {"size": "?"})
    run["label"], run["tier"], run["plain"] = label(look), look[0], not look[1] and not look[2]
    return run, None


# runs.tsv's columns, as sea_bench.sh writes them (an older one's first five read as well).
COLUMNS = ["order", "battle", "asked", "log", "status", "block", "round", "role", "first", "started", "heat_before", "heat_most", "heat_after"]


def runs_in(out):
    """[run rows, as dicts of COLUMNS] from runs.tsv (sea_bench.sh's), else every logs/*.log by its name."""
    listed = os.path.join(out, "runs.tsv")
    rows = []
    if os.path.exists(listed):
        with open(listed, encoding="utf-8") as f:
            for line in f:
                bits = line.rstrip("\n").split("\t")
                if len(bits) >= 4 and not line.startswith("#"):
                    row = dict(zip(COLUMNS, bits + [""] * (len(COLUMNS) - len(bits))))
                    row["order"], row["path"] = int(row["order"]), os.path.join(out, row["log"])
                    row["status"] = row["status"] or "done"
                    row["block"] = int(row["block"]) if row["block"] != "" else None
                    row["round"] = int(row["round"]) if row["round"] != "" else None
                    for k in ("heat_before", "heat_most", "heat_after"):
                        row[k] = int(row[k]) if re.match(r"^-?\d+$", row[k]) else -1
                    rows.append(row)
        return rows
    logs = os.path.join(out, "logs")
    for name in sorted(os.listdir(logs)) if os.path.isdir(logs) else []:
        m = re.match(r"(\d+)-(.+)-t(\d+)(?:-replay)?\.log$", name)
        if m:
            rows.append({"order": int(m.group(1)), "battle": m.group(2), "asked": m.group(3), "log": "logs/" + name, "path": os.path.join(logs, name),
                         "status": "done", "block": None, "round": None, "role": "", "first": "", "started": "", "heat_before": -1, "heat_most": -1, "heat_after": -1})
    return rows


def mean_sd(values):
    n = len(values)
    if n == 0:
        return float("nan"), float("nan")
    mu = sum(values) / n
    return mu, (math.sqrt(sum((v - mu) ** 2 for v in values) / (n - 1)) if n > 1 else float("nan"))


def hours(stamp):
    try:
        return datetime.datetime.strptime(stamp, "%Y-%m-%dT%H:%M:%S").timestamp() / 3600.0
    except (TypeError, ValueError):
        return None


def slope(xs, ys):
    """The least-squares slope of ys against xs (nan with fewer than two distinct xs)."""
    if len(xs) < 2:
        return float("nan")
    mx, my = sum(xs) / len(xs), sum(ys) / len(ys)
    sxx = sum((x - mx) ** 2 for x in xs)
    return sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sxx if sxx > 0 else float("nan")


def heat_word(level):
    return "?" if level is None or level < 0 else str(level)


def is_picture_run(run):
    """A picture run: its frame-time line says it took pictures, whatever runs.tsv calls it; or runs.tsv calls it a replay
    of 22 November's (one that took none is then held to no line: another shot plan)."""
    return run.get("pictures", 0) > 0 or run.get("role") == "replay"


def replay_lines(battle, pics, sound):
    """The replays of 22 November's picture run of a battle, held to its line: (lines, flags), each flag
    (battle, measure, the replays' mean, 22 November's, twice the replays' spread, flagged, runs)."""
    lines, flags = [], []
    before, made = BEFORE.get(battle), BEFORE_RUNS.get(battle)
    lines.append("  release 1.5's 22 November picture run, replayed (%s): its frame-time line, from 1 s of play as 22 November's;"
                 " never pooled or paired with the bench's runs:" % (made[3] if made else "a picture run, but none of 22 November's"))
    groups = {}
    for r in pics:
        groups.setdefault((r["label"], r["size"], r["pictures"]), []).append(r)
    for (arm, size, pictures), rs in groups.items():
        lines.append("    %s at %s, %d pictures: %d runs, %.0f frames over %.0f s on the mean%s"
                     % (arm, size, pictures, len(rs), mean_sd([r["frame_n"] for r in rs])[0], mean_sd([r["frame_s"] for r in rs])[0],
                        "; 22 November: %d frames over %d s, %d pictures" % (made[1], made[2], made[0]) if made else ""))
        why = None
        if before is None:
            why = "no 22 November line for %s" % battle
        elif not (rs[0]["plain"] and rs[0]["tier"] == 0):
            why = "drawn as %s, not release 1.5's sea as made" % arm
        elif size != BEFORE_SIZE:
            why = "22 November's runs were at %s" % BEFORE_SIZE
        elif pictures != made[0]:
            why = "22 November's took %d pictures: another shot plan" % made[0]
        if why:
            lines.append("      (not compared with 22 November: %s)" % why)
        for k, (key, words) in enumerate(DESCRIBED):
            mu, sd = mean_sd([r[key] for r in rs])
            spread = "%.2f" % sd if sd == sd else "n/a (one run)"
            if why:
                lines.append("      %-48s %9.2f  spread %s" % (words, mu, spread))
                continue
            twice = 2.0 * sd if sd == sd else float("nan")
            beyond = twice == twice and abs(mu - before[k]) > max(twice, 0.1)  # (and more than the line's own last place)
            flagged = beyond and len(rs) >= LEAST
            flags.append((battle, key, mu, before[k], twice, flagged, len(rs)))
            lines.append("      %-48s %9.2f  spread %s; 22 November %.1f, %+.2f: %s"
                         % (words, mu, spread, before[k], mu - before[k],
                            "FLAGGED (more than twice the replays' spread from 22 November)" if flagged else
                            "more than twice the spread, but under %d runs: a look, not a measure" % LEAST if beyond else
                            "within twice the spread" if twice == twice else "one run: no spread to hold it to"))
        if not why and len(rs) > 1:
            # (22 November's is one run, with its own noise in it: the band one more run of the same falls in, 99 in 100)
            bands = [t995(len(rs) - 1) * mean_sd([r[key] for r in rs])[1] * math.sqrt(1.0 + 1.0 / len(rs)) for key in ("frame_mean", "frame_p95")]
            lines.append("      (22 November's is one run: one more run of these would fall within %.2f ms of their mean on the mean, %.2f on the"
                         " 95th, 99 times in 100)" % tuple(bands))
    lines.append("    22 November's sound is not known (no command line was kept); these replays' sound: %s" % sound)
    if battle == "samar":
        lines.append("    " + BEFORE_NOTE)
    lines.append("    in run order (the whole frame's 95th from 1 s, ms; the machine's heat before/most/after, 0 nominal to 4):")
    for r in pics:
        lines.append("      run %3d  %-19s  round %s  heat %s/%s/%s  95th %6.2f  %d pictures at %s"
                     % (r["order"], r["started"] or "?", r["round"] if r["round"] is not None else "?", heat_word(r["heat_before"]),
                        heat_word(r["heat_most"]), heat_word(r["heat_after"]), r["frame_p95"], r["pictures"], r["size"]))
    return lines, flags


def table(rows, about=()):
    """The table's text; [(battle, block, arm, measure, mean difference, the half-width of its 99% interval, counts,
    pairs)] for every comparison; the runs left out; and the replays' flags against 22 November (replay_lines)."""
    lines, verdicts, problems, replays = [], [], [], []
    sound = next((a[len("sound: "):] for a in about if a.startswith("sound: ")), "not said (no about.txt)")
    battles = {}
    for row in sorted(rows, key=lambda r: r["order"]):
        where = "%s run %d (asked tier %s)" % (row["battle"], row["order"], row["asked"])
        if row["status"] != "done":
            problems.append("%s: %s, its numbers not used" % (where, row["status"]))
            continue
        run, why = read_log(row["path"]) if os.path.exists(row["path"]) else (None, "no log")
        if run is None:
            problems.append("%s: %s" % (where, why))
            continue
        run.update(row)
        if is_picture_run(run) and "frame_mean" not in run:
            problems.append("%s: a replay with no frame-time line" % where)
            continue
        battles.setdefault(row["battle"], []).append(run)
    for battle, every in battles.items():
        lines.append("")
        lines.append("%s:" % battle)
        # (a picture run, known by its pictures, is a replay of 22 November's: it follows the planned shots and skips
        # ahead between them, so it is never pooled or paired with the bench's whole-battle runs)
        runs = [r for r in every if not is_picture_run(r)]
        pics = [r for r in every if is_picture_run(r)]
        if runs:
            lines.append("  counted from %s s of play, the battle's %.0f to %.0f s (the least and most of its runs); release 1.5's line from 1 s described, never counted"
                         % ("/".join(sorted({"%.0f" % r["from"] for r in runs})), min(r["battle_from"] for r in runs), max(r["battle_to"] for r in runs)))
            if battle in BEFORE:
                lines.append("  (22 November's lines were picture runs, not whole battles: they are read only beside their replays, below)")
        sizes = sorted({r["size"] for r in runs})
        if len(sizes) > 1:
            lines.append("  runs at %s: runs of different window sizes are never pooled or paired" % ", ".join(sizes))
        if any(r["untimed"] for r in runs):
            lines.append("  (the graphics chip was not timed on this machine in some runs: its rows read 0)")
        if any(r["rhi_none"] for r in runs):
            lines.append("  (the RHI thread read 0 in some runs: its work is in the render thread's rows)")

        # the arms, described: each label at each size, its runs pooled
        arms = {}
        for r in runs:
            arms.setdefault((r["label"], r["size"]), []).append(r)
        if arms:
            lines.append("  described (each arm's runs pooled, plain tier 0 across every block; nothing here is counted):")
        for (arm, size), rs in arms.items():
            plain = rs[0]["plain"] and rs[0]["tier"] == 0
            lines.append("    %s at %s (%d runs)%s" % (arm, size, len(rs), "  <- plain tier 0" if plain else ""))
            for key, words in MEASURES + DESCRIBED:
                mu, sd = mean_sd([r[key] for r in rs if key in r])
                lines.append("      %-48s %9.2f  spread %s" % (words, mu, "%.2f" % sd if sd == sd else "n/a (one run)"))

        # the blocks: each tier T run against the plain tier 0 run of its own round
        blocks = sorted({r["block"] for r in runs if r["block"] is not None})
        if runs and not blocks:
            lines.append("  (no blocks recorded in runs.tsv: the runs cannot be paired, and nothing is counted)")
        for block in blocks:
            rounds = {}
            for r in runs:
                if r["block"] == block and r["role"] in ("base", "held"):
                    rounds.setdefault(r["round"], {})[r["role"]] = r
            pairs, unpaired = [], []
            for rd in sorted(rounds):
                pair = rounds[rd]
                if "base" not in pair or "held" not in pair:
                    unpaired.append("round %d: one of its pair left out" % rd)
                elif pair["base"]["size"] != pair["held"]["size"]:
                    unpaired.append("round %d: its pair at %s and %s" % (rd, pair["base"]["size"], pair["held"]["size"]))
                elif not (pair["base"]["plain"] and pair["base"]["tier"] == 0):
                    unpaired.append("round %d: its tier 0 run was not plain (%s)" % (rd, pair["base"]["label"]))
                else:
                    pairs.append((rd, pair["base"], pair["held"]))
            held_labels = []
            for _, _, h in pairs:
                if h["label"] not in held_labels:
                    held_labels.append(h["label"])
            if not pairs:
                lines.append("  block %d (tier 0 against tier %d asked): no whole pairs%s" % (block, block, "; " + "; ".join(unpaired) if unpaired else ""))
            # (block 0: tier 0 held to tier 0, the held runs with the bench's SEA_BENCH_HELD_ARGS: an own switch's A/B, as
            # step 1(a)'s -TheaterSeaMarksPool=1, its runs known by their "Sea look:" line; or, with none, an A/A block)
            base_words, held_words = ("plain tier 0", "the held run") if block == 0 else ("tier 0", "tier %d" % block)
            for arm in held_labels:
                ps = [p for p in pairs if p[2]["label"] == arm]
                base_first = sum(1 for _, b, _ in ps if b["first"] == "base")
                lines.append("  block %d: tier %d asked, drawn as %s, against plain tier 0 of the same round, at %s: %d pairs (rounds %s; %s ran first in %d, %s in %d)%s"
                             % (block, block, arm, ps[0][1]["size"], len(ps), ", ".join(str(p[0]) for p in ps), base_words, base_first, held_words, len(ps) - base_first,
                                "; left out: " + "; ".join(unpaired) if unpaired else ""))
                lines.append("    %-48s %9s  %s" % ("(the difference d, %s less %s)" % (held_words, base_words), "mean d",
                                                    "its 99%% interval, +- the least difference %d pairs resolve: verdict" % len(ps)))
                for key, words in MEASURES:
                    d = [h[key] - b[key] for _, b, h in ps if key in h and key in b]
                    mu, half = interval(d)
                    beyond = half == half and abs(mu) > max(half, 0.01)  # (and more than the line's own last place)
                    counts = beyond and len(d) >= LEAST
                    verdicts.append((battle, block, arm, key, mu, half, counts, len(d)))
                    span = "%+.2f to %+.2f, +-%.2f" % (mu - half, mu + half, half) if half == half else ""
                    lines.append("    %-48s %+9.2f  %s" % (words, mu, ("%s: COUNTS (the interval leaves 0 out)" % span) if counts else
                                                         ("%s: leaves 0 out, but under %d pairs: not counted" % (span, LEAST)) if beyond else
                                                         ("%s: within" % span) if half == half else "too few pairs to tell"))
                for key in ORDER_KEYS:
                    first = [h[key] - b[key] for _, b, h in ps if b["first"] == "base"]
                    second = [h[key] - b[key] for _, b, h in ps if b["first"] == "held"]
                    if first and second:
                        a, z = mean_sd(first)[0], mean_sd(second)[0]
                        lines.append("    order, %s: d %+.2f when %s ran first, %+.2f when %s did: the second run of a pair %+.2f ms"
                                     % (dict(MEASURES)[key], a, base_words, z, held_words, (a - z) / 2.0))

        # plain tier 0 in run order, with the machine's heat: the drift across the bench, and heat against load
        base = [r for r in runs if (r["role"] == "base" if r["block"] is not None else r["plain"] and r["tier"] == 0)]
        if base:
            lines.append("  plain tier 0 in run order (the most of the three's 95th and the whole frame's 95th, ms; the machine's heat before/most/after, 0 nominal to 4):")
            for r in base:
                lines.append("    run %3d  %-19s  block %s round %s  heat %s/%s/%s  most 95th %6.2f  whole 95th %6.2f  at %s"
                             % (r["order"], r["started"] or "?", r["block"] if r["block"] is not None else "?", r["round"] if r["round"] is not None else "?",
                                heat_word(r["heat_before"]), heat_word(r["heat_most"]), heat_word(r["heat_after"]), r["most_p95"], r["whole_p95"], r["size"]))
            at = [hours(r["started"]) for r in base]
            ys = [r["most_p95"] for r in base]
            per_run = slope(list(range(len(base))), ys)
            per_hour = slope([a - at[0] for a in at], ys) if all(a is not None for a in at) else float("nan")
            lines.append("    drift: the most of the three's 95th %+.3f ms a run over %d runs%s; the first run %.2f, the last %.2f"
                         % (per_run if per_run == per_run else 0.0, len(base), ", %+.2f ms an hour" % per_hour if per_hour == per_hour else "", ys[0], ys[-1]))
            cool = [r["most_p95"] for r in base if r["heat_most"] == 0]
            warm = [r["most_p95"] for r in base if r["heat_most"] > 0]
            if cool or warm:
                lines.append("    heat: nominal throughout %s; warmer at some point %s"
                             % ("%.2f ms over %d runs" % (mean_sd(cool)[0], len(cool)) if cool else "no runs",
                                "%.2f ms over %d runs" % (mean_sd(warm)[0], len(warm)) if warm else "no runs"))
            else:
                lines.append("    heat: not told on this machine")

        # the replays of 22 November's picture runs, held to 22 November's lines
        if pics:
            said, flags = replay_lines(battle, pics, sound)
            lines += said
            replays += flags
    if problems:
        lines.append("")
        lines.append("runs left out:")
        lines += ["  " + p for p in problems]
    return "\n".join(lines).lstrip("\n"), verdicts, problems, replays


def main_table(out):
    rows = runs_in(out)
    if not rows:
        print("bench_table: no runs in %s" % out)
        return 1
    about = []
    if os.path.exists(os.path.join(out, "about.txt")):
        with open(os.path.join(out, "about.txt"), encoding="utf-8", errors="replace") as f:
            about = [l.rstrip("\n") for l in f if l.strip()]
    text, verdicts, problems, replays = table(rows, about)
    text = "\n".join(["how the runs were played (about.txt):"] + ["  " + a for a in about] + [""]) + text
    with open(os.path.join(out, "bench_table.txt"), "w", encoding="utf-8") as f:
        f.write(text + "\n")
    print(text)
    print("\nbench_table: %d runs, %d left out; %d differences count (the 99%% interval on the paired differences' mean leaves 0 out);"
          " %d of %d of release 1.5's 22 November lines flagged against their replays (more than twice the replays' spread); table in %s"
          % (len(rows), len(problems), sum(1 for v in verdicts if v[6]), sum(1 for f in replays if f[5]), len(replays), os.path.join(out, "bench_table.txt")))
    return 1 if problems else 0


# ---------------------------------------------------------------------------------------------------------------
# The self-test: the reader held to the game's writers.

def c_literals_after(text, anchor):
    """The adjacent C string literals that follow anchor, joined."""
    k = text.find(anchor)
    if k < 0:
        return None
    k += len(anchor)
    out = ""
    m = re.compile(r'\s*"((?:[^"\\]|\\.)*)"')
    while True:
        lit = m.match(text, k)
        if not lit:
            return out
        out += lit.group(1)
        k = lit.end()


def self_test(lines_path):
    n, failed = 0, 0

    def check(what, ok, got):
        nonlocal n, failed
        n += 1
        failed += 0 if ok else 1
        print("  %s  %s: %s" % ("ok  " if ok else "FAIL", what, got))

    frames_h = open(os.path.join(SRC, "Battle", "SeaFrames.h"), encoding="utf-8").read()
    battle_cpp = open(os.path.join(SRC, "Battle", "SeaBattle.cpp"), encoding="utf-8").read()
    src_threads = c_literals_after(frames_h, "std::snprintf(Text, sizeof(Text),\n")
    check("the threads' line read as Battle/SeaFrames.h writes it", src_threads == THREADS, src_threads or "not found")
    src_none = c_literals_after(frames_h, "std::snprintf(Text, sizeof(Text), ")
    check("and its no-frames line", src_none == NO_THREADS, src_none or "not found")
    check("and its notes, the chip not timed and the RHI thread at 0", ('"' + NOT_TIMED + '"') in frames_h and ('"' + RHI_NONE + '"') in frames_h, NOT_TIMED + ";" + RHI_NONE)
    say = battle_cpp.find("void ASeaBattle::SayFrameTimes()")
    src_frames = c_literals_after(battle_cpp[say:], "UE_LOG(LogTheater, Display, TEXT(") if say >= 0 else None
    check("the frame-time line read as SeaBattle.cpp's SayFrameTimes writes it", src_frames == FRAMES, src_frames or "not found")

    # the game's own lines, made by measure_test from the game's writers
    made = []
    if lines_path:
        with open(lines_path, encoding="utf-8") as f:
            made = [l.rstrip("\n").split("\t") for l in f if not l.startswith("#")]
        threads_ok, look_ok, threads_n, look_n, notes_seen = True, True, 0, 0, set()
        for row in made:
            if row[0] == "threads":
                threads_n += 1
                want = [float(x) for x in row[1].split()]
                got = read_threads("[2026.11.24-10.00.00:000][  0]LogTheater: Display: " + row[2])
                threads_ok = threads_ok and got is not None and len(want) == len(THREAD_KEYS) and all(abs(got[k] - w) < 1e-9 for k, w in zip(THREAD_KEYS, want))
                # and the same numbers through this reader's own copy of the words give the same line, letter for letter
                threads_ok = threads_ok and got is not None and fill(THREADS, want, got["notes"]) == row[2]
                notes_seen |= {k for k in ("rhi_none", "untimed") if got and got[k]}
            elif row[0] == "nothreads":
                threads_ok = threads_ok and (NO_THREADS % float(row[1])) == row[2] and read_threads(row[2]) is None
            elif row[0] == "look":
                look_n += 1
                tier, not_built, built = row[1].split("|")
                got = read_look(row[2])
                want_asked = [p[:-len(" (asked)")] for p in built.split(", ") if p.endswith(" (asked)")] if built else []
                look_ok = look_ok and got == (int(tier), want_asked, not_built.split(", ") if not_built else [])
        check("measure_test's threads' lines read back to the numbers they were made of, and made again letter for letter, notes and all",
              threads_ok and threads_n >= 2 and notes_seen == {"rhi_none", "untimed"}, "%d lines" % threads_n)
        check("measure_test's \"Sea look:\" lines read back to their tier, the switches asked and those not built", look_ok and look_n >= 4, "%d lines" % look_n)
    looks = {r[1]: r[2] for r in made if r[0] == "look"}
    plain_look = looks.get("0||", "Sea look: tier 0 (release 1.5's sea, as made); none of the release 2 sea's 31 switches is built yet, so every one is at today's value.")
    aa_look = looks.get("0|-TheaterSeaLook=1|", plain_look[:-1] + "; asked for but not built yet, so nothing changes: -TheaterSeaLook=1.")
    built_look = "Sea look: tier 2; 1 of the release 2 sea's 31 switches built, in force: -TheaterSeaLook=2 (asked)."
    check("a run asked for tier 1 before step 15 is tabled as tier 0, asked and not built", label(read_look(aa_look)) == "tier 0 (asked, not built: -TheaterSeaLook=1)", label(read_look(aa_look)))

    # A made-up bench. samar in Brian's window: block 1 an A/A (tier 1 asked, not built), block 2 a built tier 2 half a ms
    # slower on the chip; the machine 2 ms slower in block 2 (heat), which a pooled tier 0 would read as the tier's cost;
    # the second run of every pair 0.03 ms slower (the order), the order turned about every round. Beside it: an overrun
    # run whose log has every line, a run with no threads' line, a run in a smaller window; and two-lines at 1600x1000.
    # Before them, the replays of 22 November's picture runs: samar's as 22 November's (and one more that runs.tsv calls
    # a base run of block 1: a picture run all the same, by its pictures), light-ships' 1.2 ms slower than 22 November,
    # two-lines' under five, under another shot plan, and at another size.
    folder = tempfile.mkdtemp(prefix="bench_table_")
    os.makedirs(os.path.join(folder, "logs"))
    tsv = []
    clock = [datetime.datetime(2026, 11, 30, 9, 0, 0)]

    def run(battle, asked, look, chip, game=5.0, size=(2881, 1786), status="done", threads=True, block=1, rnd=1, role="base", first="base", heat=(0, 0, 0),
            pictures=0, frame=None, frames=True):
        order = len(tsv)
        name = "logs/%03d-%s-t%d%s.log" % (order, battle, asked, "-replay" if role == "replay" else "")
        most = max(game, chip, 2.0)
        whole = most + 0.6
        nums = [14000, 60.0, 61.0, 434.0, game, game * 1.2, game * 1.4, 2.0, 2.4, 2.8, 1.5, 1.8, 2.1, chip, chip * 1.2, chip * 1.4,
                most, most * 1.2, most * 1.4, whole, whole * 1.2, whole * 1.4, 70, 0.5, 2.0, 30.0, 52.0, 80.0]
        # (release 1.5's line: a whole battle's from 1 s, or a picture run's, frame = its mean, 95th and 99th)
        fm, f95, f99 = frame if frame else (whole + 3, whole * 1.5, whole * 2)
        line = FRAMES % (21000, 430.0, size[0], size[1], fm, 1000.0 / fm, whole, f95, f99, whole * 3, 0, 0) if not pictures else \
            FRAMES % (int(BEFORE_RUNS.get(battle, (0, 0, 167))[2] * 1000 / fm), BEFORE_RUNS.get(battle, (0, 0, 167))[2], size[0], size[1], fm, 1000.0 / fm, fm - 0.1, f95, f99, f99 * 3, 25, pictures)
        body = ["[2026.11.30-09.00.00:000][  0]LogTheater: Display: Sea battle: playing %s." % battle]
        if frames:
            body.append("[2026.11.30-09.08.00:000][900]LogTheater: Display: " + line)
        if threads:
            body.append("[2026.11.30-09.08.00:000][900]LogTheater: Display: " + fill(THREADS, [round(x, 2) for x in nums], ["", ""]))
        body.append("[2026.11.30-09.08.00:000][900]LogTheater: Display: " + look)
        with open(os.path.join(folder, name), "w", encoding="utf-8") as f:
            f.write("\n".join(body) + "\n")
        tsv.append("\t".join(str(x) for x in [order, battle, asked, name, status, block, rnd, role, first, clock[0].strftime("%Y-%m-%dT%H:%M:%S")] + list(heat)))
        clock[0] += datetime.timedelta(minutes=9)

    for rnd, (j, k) in enumerate(((-0.2, 0.1), (0.2, -0.05), (0.1, 0.0), (-0.1, 0.05), (0.0, -0.1)), 1):
        run("samar", 0, plain_look, 11.0, block="", rnd=rnd, role="replay", first="", pictures=10, frame=(14.2 + j, 22.8 + 1.5 * j, 31.0 + 2 * j))
        run("light-ships", 0, plain_look, 9.0, block="", rnd=rnd, role="replay", first="", pictures=24, frame=(10.4 + k, 12.5 + k, 16.6 + k))
        if rnd <= 3:
            run("two-lines", 0, plain_look, 8.0, block="", rnd=rnd, role="replay", first="", pictures=24, frame=(12.0 + k, 14.0 + k, 16.0 + k))
    run("two-lines", 0, plain_look, 8.0, block="", rnd=4, role="replay", first="", pictures=22, frame=(9.5, 11.4, 13.7))
    run("two-lines", 0, plain_look, 8.0, size=(1600, 1000), block="", rnd=5, role="replay", first="", pictures=24, frame=(9.5, 11.4, 13.7))
    run("samar", 0, plain_look, 11.0, block=1, rnd=9, role="base", first="base", pictures=10, frame=BEFORE["samar"])
    jitter = (-0.1, 0.05, 0.0, 0.1, -0.05)
    for block, drift, cost, look, heat in ((1, 0.0, 0.0, aa_look, (0, 0, 0)), (2, 2.0, 0.5, built_look, (0, 1, 1))):
        for rnd in range(1, 6):
            j = jitter[rnd - 1]
            first = "base" if rnd % 2 == 1 else "held"
            base_chip = 11.0 + drift + j + (0.03 if first == "held" else 0.0)
            held_chip = 11.0 + drift + j + cost + (0.03 if first == "base" else 0.0)
            pair = [("base", 0, plain_look, base_chip), ("held", block, look, held_chip)]
            for role, asked, lk, chip in (pair if first == "base" else pair[::-1]):
                run("samar", asked, lk, chip, block=block, rnd=rnd, role=role, first=first, heat=heat)
    run("samar", 0, plain_look, 11.0, block=1, rnd=6, role="base", first="base")
    run("samar", 1, aa_look, 11.0, size=(1600, 1000), block=1, rnd=6, role="held", first="base")
    run("samar", 1, aa_look, 30.0, status="overran", block=1, rnd=7, role="held", first="base")
    run("samar", 1, aa_look, 11.0, threads=False, block=1, rnd=8, role="held", first="base")
    for rnd in (1, 2):
        run("two-lines", 0, plain_look, 8.0, size=(1600, 1000), block=1, rnd=rnd, role="base", first="base")
        run("two-lines", 1, aa_look, 8.0, size=(1600, 1000), block=1, rnd=rnd, role="held", first="base")
    with open(os.path.join(folder, "runs.tsv"), "w", encoding="utf-8") as f:
        f.write("# " + "\t".join(COLUMNS) + "\n" + "\n".join(tsv) + "\n")
    text, verdicts, problems, replays = table(runs_in(folder), ["sound: muted (played with sound, the Mac's output muted)"])
    by = {(v[0], v[1], v[3]): v for v in verdicts}
    aa = [v for v in verdicts if v[0] == "samar" and v[1] == 1]
    check("the A/A block (tier 1 asked before it is built), paired round by round: no difference counts", aa and not any(v[6] for v in aa) and all(v[7] == 5 for v in aa),
          "%d measures, %s pairs" % (len(aa), sorted({v[7] for v in aa})))
    chip = by.get(("samar", 2, "chip_mean"))
    check("a tier half a ms slower on the chip, held to its own block's tier 0 (2 ms warmer than block 1's): counts, at 0.5 and not 2.5",
          chip is not None and chip[6] and abs(chip[4] - 0.5) < 0.01 and chip[2] == "tier 2, -TheaterSeaLook=2", str(chip))
    game = by.get(("samar", 2, "game_mean"))
    check("the same tier's game thread, no slower: does not count", game is not None and not game[6], str(game))
    check("the whole frame counted from the threads' line, over the same frames; release 1.5's 1 s line never counted",
          ("samar", 2, "whole_mean") in by and by[("samar", 2, "whole_mean")][6] and not any(v[3].startswith("frame_") for v in verdicts), str(by.get(("samar", 2, "whole_mean"))))
    check("the order of a pair shown: the second run 0.03 ms slower",
          "order, the most of the three, mean: d +0.53 when tier 0 ran first, +0.47 when tier 2 did: the second run of a pair +0.03 ms" in text, "samar, block 2")
    check("an overrun run is left out, though its log has every line; a run with no threads' line too; each said",
          len(problems) == 2 and "overran, its numbers not used" in problems[0] and "no threads' line" in problems[1], "; ".join(problems))
    check("a run in another window is neither pooled nor paired: an arm of its own, and its round left out of the pairs",
          "tier 0 (asked, not built: -TheaterSeaLook=1) at 1600x1000 (1 runs)" in text and "round 6: its pair at 2881x1786 and 1600x1000" in text and "runs of different window sizes are never pooled or paired" in text,
          "samar round 6")
    # (22 November's lines were picture runs: never beside the bench's whole-battle runs, only beside their replays)
    sections, beside_bench = {}, []
    for line in text.splitlines():
        if re.match(r"^[a-z-]+:$", line):
            sections[line[:-1]] = []
        elif sections:
            sections[list(sections)[-1]].append(line)
    for battle, said in sections.items():
        at = next((i for i, l in enumerate(said) if "release 1.5's 22 November picture run, replayed" in l), len(said))
        beside_bench += ["%s: %s" % (battle, l.strip()[:60]) for l in said[:at] if re.search(r"22 November \d|22 November: \d", l)]
    check("release 1.5's 22 November lines never beside the bench's whole-battle runs (its arms, blocks or run order), whatever their size",
          sections.keys() >= {"samar", "two-lines", "light-ships"} and not beside_bench and "release 1.5 on 22 November" not in text, "; ".join(beside_bench) or "none")
    sam = [f for f in replays if f[0] == "samar"]
    check("samar's replays (picture runs, known by their pictures, one of them called a base run in runs.tsv) held to 22 November's at 2881x1786 and 10 pictures: within twice their spread",
          len(sam) == 3 and all(f[6] == 6 and not f[5] and abs(f[2] - f[3]) < 0.01 for f in sam) and "22 November 14.2, +0.00: within twice the spread" in text, str([(f[1], round(f[2], 2), f[5], f[6]) for f in sam]))
    light = {f[1]: f for f in replays if f[0] == "light-ships"}
    check("light-ships' replays 1.2 ms slower than 22 November, their spread 0.08: FLAGGED", light.get("frame_mean") is not None and light["frame_mean"][5]
          and light["frame_mean"][6] == 5 and abs(light["frame_mean"][2] - light["frame_mean"][3] - 1.2) < 0.01, str(light.get("frame_mean")))
    check("two-lines' replays: three 2.5 ms off but under five runs, a look; one under another shot plan and one at 1600x1000 not compared",
          not any(f[5] for f in replays if f[0] == "two-lines") and "under 5 runs: a look, not a measure" in text
          and "(not compared with 22 November: 22 November's took 24 pictures: another shot plan)" in text
          and "(not compared with 22 November: 22 November's runs were at 2881x1786)" in text, "two-lines")
    check("the picture runs kept out of the bench's arms and pairs: samar's plain tier 0 11 runs, round 9 (a picture run) not paired",
          "    tier 0 at 2881x1786 (11 runs)  <- plain tier 0" in "\n".join(sections.get("samar", [])) and "round 9:" not in text
          and "against plain tier 0 of the same round, at 2881x1786: 5 pairs (rounds 1, 2, 3, 4, 5;" in text, "samar")
    check("22 November's sound said not known; samar-warm-grade said a different grade's run, not samar's second",
          text.count("22 November's sound is not known") == 3 and BEFORE_NOTE in text and "second run that day" not in text, "samar")
    check("two-lines' two pairs: too few to count anything", not any(v[6] for v in verdicts if v[0] == "two-lines") and any(v[0] == "two-lines" for v in verdicts), "2 pairs")
    drift = re.search(r"drift: the most of the three's 95th ([+-]\d+\.\d+) ms a run over (\d+) runs, ([+-]\d+\.\d+) ms an hour", text)
    check("plain tier 0 in run order, with its heat, and the drift across the bench (2 ms warmer in block 2)",
          drift is not None and float(drift.group(1)) > 0 and drift.group(2) == "11" and "heat 0/1/1" in text, drift.group(0) if drift else "no drift line")
    check("heat against load: nominal runs against warmer ones", re.search(r"heat: nominal throughout 13\.\d\d ms over 6 runs; warmer at some point 15\.\d\d ms over 5 runs", text) is not None,
          (re.search(r"heat: .*", text) or [""])[0])
    check("no word of the table says dice", "dice" not in text.lower(), "%d lines" % len(text.splitlines()))
    shutil.rmtree(folder, ignore_errors=True)

    # Each run with noise of its own (a run's noise is not its pair's): the chip sigma 0.4 ms a run, a fixed seed (5: its
    # samples near the noise's own, d's spread 0.66 and 0.58 against 0.57), 20 rounds. Block 1 an A/A; block 2 a tier
    # 0.6 ms slower on the chip (inside the plan's tier 1 net, +0.55 to 0.9 ms). Twice the spread of d (about 1.1 ms here)
    # never narrows with rounds, and would count such a cost about 1 time in 100 at 20 pairs.
    folder = tempfile.mkdtemp(prefix="bench_table_noise_")
    os.makedirs(os.path.join(folder, "logs"))
    tsv = []
    noise = random.Random(5)
    for block, cost, look in ((1, 0.0, aa_look), (2, 0.6, built_look)):
        for rnd in range(1, 21):
            first = "base" if rnd % 2 == 1 else "held"
            pair = [("base", 0, plain_look, 11.0 + noise.gauss(0.0, 0.4)), ("held", block, look, 11.0 + cost + noise.gauss(0.0, 0.4))]
            for role, asked, lk, chip in (pair if first == "base" else pair[::-1]):
                run("samar", asked, lk, chip, block=block, rnd=rnd, role=role, first=first)
    with open(os.path.join(folder, "runs.tsv"), "w", encoding="utf-8") as f:
        f.write("# " + "\t".join(COLUMNS) + "\n" + "\n".join(tsv) + "\n")
    text, verdicts, problems, replays = table(runs_in(folder), [])
    by = {(v[0], v[1], v[3]): v for v in verdicts}
    chip = by.get(("samar", 2, "chip_mean"))
    rows = runs_in(folder)
    d = []
    for rnd in range(1, 21):
        pair = {r["role"]: read_log(r["path"])[0]["chip_mean"] for r in rows if r["block"] == 2 and r["round"] == rnd}
        d.append(pair["held"] - pair["base"])
    check("noise of each run's own, sigma 0.4 ms: a tier 0.6 ms slower on the chip counts at 20 pairs (the 99% interval on mean d leaves 0 out)",
          chip is not None and chip[6] and chip[7] == 20 and 0.3 < chip[4] < 0.9,
          "mean d %+.2f, +-%.2f; twice the spread of d, the old rule, %.2f" % (chip[4], chip[5], 2.0 * mean_sd(d)[1]) if chip else "none")
    check("which twice the spread of d (the rule before, which never narrows) would not have counted",
          chip is not None and len(d) == 20 and abs(mean_sd(d)[0] - chip[4]) < 0.01 and abs(chip[4]) <= 2.0 * mean_sd(d)[1], "%+.2f against %.2f" % (mean_sd(d)[0], 2.0 * mean_sd(d)[1]))
    aa = [v for v in verdicts if v[1] == 1]
    check("and its A/A block, the same noise: nothing counts", aa and not any(v[6] for v in aa) and all(v[7] == 20 for v in aa),
          "chip mean d %+.2f, +-%.2f" % by[("samar", 1, "chip_mean")][4:6] if ("samar", 1, "chip_mean") in by else "none")
    check("the interval narrows with rounds: t(0.995, n - 1) / root n is 2.06 at 5 pairs (the plan's twice the spread), 0.64 at 20",
          abs(t995(4) / math.sqrt(5) - 2.059) < 0.001 and abs(t995(19) / math.sqrt(20) - 0.640) < 0.001 and t995(35) == T995[30] and t995(500) == T995[120] and t995(0) != t995(0),
          "%.3f, %.3f" % (t995(4) / math.sqrt(5), t995(19) / math.sqrt(20)))
    check("each measure's 99% interval and the least difference its pairs resolve printed",
          re.search(r"graphics chip, mean +[+-]\d+\.\d\d  [+-]\d+\.\d\d to [+-]\d+\.\d\d, \+-\d+\.\d\d: COUNTS \(the interval leaves 0 out\)", text) is not None
          and "the least difference 20 pairs resolve" in text, (re.search(r"graphics chip, mean .*COUNTS.*", text) or [""])[0].strip())
    shutil.rmtree(folder, ignore_errors=True)

    # a replay that took no pictures (runs.tsv's word for it alone) and one with no frame-time line: neither among the
    # bench's runs; the first held to no 22 November line, the second left out and said
    folder = tempfile.mkdtemp(prefix="bench_table_replay_")
    os.makedirs(os.path.join(folder, "logs"))
    tsv = []
    run("samar", 0, plain_look, 11.0, block="", rnd=1, role="replay", first="", frame=(15.0, 24.0, 33.0))
    run("samar", 0, plain_look, 11.0, block=1, rnd=1, role="base", first="base")
    run("samar", 1, aa_look, 11.0, block=1, rnd=1, role="held", first="base")
    run("samar", 0, plain_look, 11.0, block="", rnd=2, role="replay", first="", pictures=10, frames=False)
    with open(os.path.join(folder, "runs.tsv"), "w", encoding="utf-8") as f:
        f.write("# " + "\t".join(COLUMNS) + "\n" + "\n".join(tsv) + "\n")
    text, verdicts, problems, replays = table(runs_in(folder), [])
    check("a replay that took no pictures kept out of the bench's runs and held to no line; one with no frame-time line left out and said",
          "(not compared with 22 November: 22 November's took 10 pictures: another shot plan)" in text and "    tier 0 at 2881x1786 (1 runs)  <- plain tier 0" in text
          and not replays and problems == ["samar run 3 (asked tier 0): a replay with no frame-time line"], "; ".join(problems))
    shutil.rmtree(folder, ignore_errors=True)

    # an older runs.tsv (five columns): its status read, its runs described, nothing paired
    folder = tempfile.mkdtemp(prefix="bench_table_old_")
    os.makedirs(os.path.join(folder, "logs"))
    tsv = []
    run("light-ships", 0, plain_look, 9.0)
    with open(os.path.join(folder, "runs.tsv"), "w", encoding="utf-8") as f:
        f.write("# order\tbattle\tasked tier\tlog\tstatus\n" + "\t".join(tsv[0].split("\t")[:4]) + "\toverran\n")
    old = runs_in(folder)
    check("an older runs.tsv read, its status kept", len(old) == 1 and old[0]["status"] == "overran" and old[0]["block"] is None, str(old[0]["status"] if old else None))
    shutil.rmtree(folder, ignore_errors=True)
    print("bench_table self-test: %d cases, %d failed" % (n, failed))
    return failed == 0


if __name__ == "__main__":
    args = sys.argv[1:]
    if args[:1] == ["--self-test"]:
        sys.exit(0 if self_test(args[1] if len(args) > 1 else None) else 1)
    if len(args) != 1:
        print(__doc__)
        sys.exit(2)
    sys.exit(main_table(args[0]))
