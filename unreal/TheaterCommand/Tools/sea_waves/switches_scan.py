#!/usr/bin/env python3
"""The release 2 sea's switches, held to the plan (the release 2 ocean plan, step 0, (f); the table is the plan's section 3).

It holds the plan's switch table and checks, against the game's own source:
  - Battle/SeaLookSwitches.h (what the "Sea look:" line says) holds the same switches, with the same today's value, the
    same tier or switch that turns each on and the same step that builds it, in the same order;
  - no name with its '=' is found inside another's on the command line, as the engine's value reader finds it
    (FParse::Value looks for the name anywhere, whatever the case; FParse::Param only as a whole word), against every
    switch the game already reads too: -TheaterSeaWaves= is the air sea's and is never taken again;
  - a switch is marked built exactly when the game reads it: its name in a string of the game's source outside
    SeaLookSwitches.h (the "Sea look:" line reads every name only to say it, which builds nothing);
  - the bench's own switch, -TheaterSeaBench= (step 0, measuring only), is read and collides with nothing;
  - the cloud's SEA_SWELL_RIDE (step 5) is read by no runner yet;
  - no word of the new lines says dice.

Usage: python3 -I Tools/sea_waves/switches_scan.py [--self-test]
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(ROOT, "Source", "TheaterCommand")
HEADER = os.path.join(SRC, "Battle", "SeaLookSwitches.h")

# The plan's section 3: (name as after its '-', values with the default first, today's value at tier 0, what turns it on,
# the step that builds it, built in this tree). A name ending '=' takes a value; one without is a flag.
TABLE = [
    ("TheaterSeaLook=", "0, 1, 2, 3", "0", "tier", "15", False),
    ("TheaterSeaMarksPool=", "0, 1 (1 after the Mac A/B)", "0", "own", "1", True),
    ("TheaterSeaFarEdge=", "15 (ship lengths, today's)", "15", "own", "1", False),
    ("TheaterSeaSwell=", "0, 1", "0", "1", "8, 9", False),
    ("TheaterSeaSwellScale=", "1 (0 keeps everything wired on a flat sea)", "1", "with Swell", "9", False),
    ("TheaterSeaRingCells=", "96, 128", "96", "with Swell", "2, 8", False),
    ("TheaterSeaPatch=", "0, 1", "0", "1", "8, 9", False),
    ("TheaterSeaPixelTrains=", "12 (0-16)", "12", "1", "2, 7", False),
    ("TheaterSeaFleetCaps=", "0, 1", "0", "1", "7", False),
    ("TheaterSeaHullWaves=", "0, 1", "0", "1", "9a", False),
    ("TheaterSeaClockCap=", "1.5 (most times real time the sea runs in fast play)", "1.5", "with Swell", "9", False),
    ("TheaterSeaWater=", "tropic, temperate, north, med (override; none by default)", "none", "1", "3, 8", False),
    ("TheaterSeaProbe", "off", "off", "review", "9", False),
    ("TheaterSeaDay=", "seed, field", "seed", "2", "11", False),
    ("TheaterSeaVisContrast=", "0.05", "0.05", "with Day", "3", False),
    ("TheaterSeaSkyCapture=", "0, 1", "0", "2", "11", False),
    ("TheaterSeaStars=", "0, or the stars' light x (1 with Day)", "0", "with Day", "11", False),
    ("TheaterSeaWakeLane=", "0, 1", "0", "2", "12", False),
    ("TheaterSeaSpray=", "0, 1", "0", "2", "6, 12", True),
    ("TheaterSeaIce=", "0, 1", "0", "2", "6", True),
    ("TheaterSeaNightFight=", "0, 1", "0", "2", "11", True),
    ("TheaterSeaClouds=", "0, dome, puffs", "0", "3", "13", True),
    ("TheaterSeaCloudShade=", "0, 1", "0", "3", "13", True),
    ("TheaterSeaRain=", "0, 1", "0", "3", "13", True),
    ("TheaterSeaSkyStandIn=", "0, 1 (Brian rules on the default)", "0", "3", "4, 13", True),
    ("TheaterSeaWeather=", "none, deck,0.8 (review pictures only)", "none", "none", "13", True),
    ("TheaterSeaCurve", "off", "off", "extra", "14", False),
    ("TheaterSeaWet", "off", "off", "extra", "14", False),
    ("TheaterSeaGale", "off", "off", "extra", "14", False),
    ("TheaterSeaEyeRide", "off", "off", "extra", "14", False),
    ("TheaterSeaIntoSun", "off", "off", "extra", "14", False),
    ("TheaterSeaCoast", "off", "off", "extra", "14", False),
    ("TheaterSeaTurrets=", "0, 1 (1 after the Mac check)", "0", "own", "16", False),
    ("TheaterSeaStepDown=", "0; 1 whenever the tier is 1 or more", "0", "1", "15", False),
    # (the ship detail plan, round 1: the ships at sea, review pictures until the art director's Mac check, and the reel's two)
    ("TheaterSeaShipLight=", "0 or off; 1 or on (the file's shade on each point, the ships' top light 0.10); or a top light from 0.0 to 0.30 (0.08, 0.12, the map's 0.26); anything else refused, off", "0", "review", "R1.3", True),
    ("TheaterSeaShipSkin=", "0, 1 (the rust ramp and the ruled salt, M_MapSurface's own wear on the sea's steel ink)", "0", "review", "R1.4", True),
    ("TheaterSeaShipShade=", "0, 1 (occlusion, 0.3 m contact shadows, shadow quality 3)", "0", "review", "R1.3", True),
    ("TheaterSeaReel=", "off, or shots by their planned names (with -TheaterSeaBattleShots=)", "off", "review", "R1.1", True),
    ("TheaterSeaRideStrip=", "off, or key,from,seconds (with -TheaterSeaBattleShots=)", "off", "review", "R1.1", True),
]
# The bench's own (step 0): measuring only, it draws nothing differently, so it is no look switch and not on the line.
MEASURE = [("TheaterSeaBench=", "drop,span (seconds; Tools/sea_bench.sh)", "0", "Step 0", True)]
# The cloud's own (environment variables, not the game's): the ride build of sea_show_test.
CLOUD = [("SEA_SWELL_RIDE", "5", False)]
# Already the air sea's: never taken again.
TAKEN = ["TheaterSeaWaves="]

ROW = re.compile(r'\{\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*(true|false)\s*\}')
FPARSE = re.compile(r'FParse::(Value|Param|Bool)\s*\([^,;]+,\s*TEXT\("([^"]+)"\)')
TEXT_VALUE = re.compile(r'TEXT\("(Theater[A-Za-z0-9]*=)"\)')
STRING = re.compile(r'"((?:[^"\\\n]|\\.)*)"')


def header_rows(text):
    """The rows of SeaLookSwitches::All, in order: (name, today, on at, step, built)."""
    start = text.find("Switch All[] = {")
    end = text.find("};", start)
    if start < 0 or end < 0:
        return None
    return [(m.group(1), m.group(2), m.group(3), m.group(4), m.group(5) == "true") for m in ROW.finditer(text[start:end])]


def source_files():
    for base, _dirs, files in os.walk(SRC):
        for name in sorted(files):
            if name.endswith((".h", ".cpp", ".inl")):
                yield os.path.join(base, name)


def read_by_game():
    """Every switch the game reads: {name: (kind, where)}; kind "value" (found anywhere, as FParse::Value finds it) or
    "flag" (a whole word, as FParse::Param); and every string in the source outside SeaLookSwitches.h."""
    names, strings = {}, {}
    for path in source_files():
        text = open(path, encoding="utf-8", errors="replace").read()
        rel = os.path.relpath(path, ROOT)
        for m in FPARSE.finditer(text):
            kind = "flag" if m.group(1) == "Param" else "value"
            names.setdefault(m.group(2), (kind, rel))
        for m in TEXT_VALUE.finditer(text):  # (a helper's switch, as BattleLook's Asked(TEXT("-TheaterAirHaze=")))
            names.setdefault(m.group(1), ("value", rel))
        if os.path.basename(path) != os.path.basename(HEADER):
            for m in STRING.finditer(text):
                strings.setdefault(m.group(1), rel)
    return names, strings


def finds(value_name, other, other_kind):
    """Whether the engine's value reader, asked for value_name, finds it inside other's switch on the command line."""
    spelled = "-" + other + ("1" if other.endswith("=") or other_kind == "value" else "")
    return value_name.lower() in spelled.lower()


def collisions(named):
    """Each pair (a, b) where a's value reader finds a inside b's switch. named: {name: kind}."""
    out = []
    for a, kind_a in named.items():
        if kind_a != "value":
            continue
        for b, kind_b in named.items():
            if a != b and finds(a, b, kind_b):
                out.append((a, b))
    return out


class Checks:
    def __init__(self):
        self.n = 0
        self.failed = 0

    def check(self, what, ok, got):
        self.n += 1
        self.failed += 0 if ok else 1
        print("  %s  %s: %s" % ("ok  " if ok else "FAIL", what, got))


def self_test():
    c = Checks()
    named = {"TheaterSeaSwell=": "value", "XTheaterSeaSwell=": "value", "TheaterSeaWet": "flag", "TheaterSeaWetter=": "value",
             "TheaterSeaFoam": "value", "TheaterSeaFoamy=": "value"}
    found = collisions(named)
    c.check("a value's name inside a longer one is found", ("TheaterSeaSwell=", "XTheaterSeaSwell=") in found, str(found))
    c.check("a flag is found only whole, never inside another", not any(a == "TheaterSeaWet" for a, _ in found), str(found))
    c.check("a value read without its '=' is found inside any longer name", ("TheaterSeaFoam", "TheaterSeaFoamy=") in found, str(found))
    c.check("whatever the case", finds("theaterseaswell=", "XTHEATERSEASWELL=", "value"), "found")
    c.check("a name with its '=' is not found inside a longer word", not finds("TheaterSeaPatch=", "TheaterSeaPatchX=", "value"), "not found")
    rows = header_rows('Switch All[] = {\n\t{ "A=", "0", "1", "2", false },\n\t{ "B", "off", "extra", "14", true },\n};')
    c.check("the header's rows read", rows == [("A=", "0", "1", "2", False), ("B", "off", "extra", "14", True)], str(rows))
    print("switches_scan self-test: %d cases, %d failed" % (c.n, c.failed))
    return c.failed == 0


def main():
    if "--self-test" in sys.argv[1:]:
        return 0 if self_test() else 1
    c = Checks()
    header = open(HEADER, encoding="utf-8").read()
    rows = header_rows(header)
    mine = [(n, t, o, s, b) for (n, _v, t, o, s, b) in TABLE]
    c.check("Battle/SeaLookSwitches.h read", rows is not None and len(rows) > 0, "%d rows" % len(rows or []))
    rows = rows or []
    differ = [("%s: plan %s, header %s" % (a[0], a[1:], b[1:]) if a[0] == b[0] else "%s against %s" % (a[0], b[0])) for a, b in zip(mine, rows) if a != b]
    c.check("the header holds the plan's section 3 switch for switch: today's value, what turns it on, its step, built or not, in order",
            len(rows) == len(mine) and not differ, ("%d of %d" % (len(rows), len(mine))) + ("; " + "; ".join(differ) if differ else ""))
    names = [n for n, *_ in TABLE] + [n for n, *_ in MEASURE]
    lowered = [n.lower().rstrip("=") for n in names]
    c.check("every name its own, whatever the case or its '='", len(set(lowered)) == len(lowered), "%d names" % len(names))

    game, strings = read_by_game()
    table_names = set(n for n, *_ in TABLE) | set(n for n, *_ in MEASURE)
    already = {n: k for n, (k, _w) in game.items() if n not in table_names}
    c.check("the game's own switches found", len(already) > 100 and "TheaterSeaWaves=" in already, "%d, -TheaterSeaWaves= among them" % len(already))
    clash = [n for n in names if n.lower().rstrip("=") in {a.lower().rstrip("=") for a in already}]
    c.check("no new name is one the game already reads (-TheaterSeaWaves= stays the air sea's)", not clash and not [t for t in TAKEN if t in table_names],
            ", ".join(clash) or "none")
    named = dict(already)
    for n in names:
        named[n] = "value" if n.endswith("=") else "flag"
    found = collisions(named)
    new = [(a, b) for a, b in found if a in table_names or b in table_names]
    old = [(a, b) for a, b in found if a not in table_names and b not in table_names]
    c.check("no new name with its '=' found inside another's, nor another's inside a new one, among the game's %d switches" % len(named),
            not new, "; ".join("-%s inside -%s" % p for p in new) or "none")
    if old:
        print("  note  the game's own names found inside one another (as made; not the new sea's to change): " + "; ".join("-%s inside -%s" % p for p in old))

    wrong = []
    for n, _v, _t, _o, _s, built in TABLE:
        read = n in strings or n in game
        if read != built:
            wrong.append("%s %s but %s" % (n, "marked built" if built else "not marked built", "read" if read else "not read"))
    c.check("a switch marked built exactly when the game reads it (outside the \"Sea look:\" line's table)", not wrong, "; ".join(wrong) or "none built, none read")
    for n, _v, _t, step, built in MEASURE:
        c.check("the bench's -%s (%s, measuring only) read by the game and marked so" % (n, step), built and n in game and game[n][0] == "value",
                game.get(n, ("not read", ""))[1])
    for env, step, built in CLOUD:
        readers = []
        for base, _dirs, files in os.walk(os.path.join(ROOT, "Tools")):
            for name in files:
                path = os.path.join(base, name)
                if os.path.abspath(path) == os.path.abspath(__file__) or not name.endswith((".sh", ".cpp", ".py", ".h")):
                    continue
                if env in open(path, encoding="utf-8", errors="replace").read():
                    readers.append(os.path.relpath(path, ROOT))
        c.check("the cloud's %s (step %s) read by a runner exactly when built" % (env, step), bool(readers) == built, ", ".join(readers) or "read by none yet")
    said = header + open(os.path.join(SRC, "Battle", "SeaFrames.h"), encoding="utf-8").read()
    c.check("no word of the new lines says dice", "dice" not in said.lower(), "SeaLookSwitches.h, SeaFrames.h")
    print("switches_scan: %d checks, %d failed" % (c.n, c.failed))
    return 0 if c.failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
