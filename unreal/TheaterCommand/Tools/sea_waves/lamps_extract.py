"""Cuts the sea battle's sea-lamps detection out of the game's own source (SeaParts::RuleOf, HasSeaLamps, AskSeaLamps
and NavalRow in Battle/SeaBattle.cpp; the release 2 ocean plan, step 11) for sea_lamps_test, so the test runs the
game's lines and never a copy. Each end is found once, at the head of a line.
Usage: python3 -I -B lamps_extract.py <Source/TheaterCommand> <out folder>"""
import os
import sys

FIRST = "\ttemplate <typename TSetter>\n\tstruct RuleOf\n"
LAST = "\tstatic const TCHAR* NavalRow(double CalibreMm, const std::string& What)\n"


def main(argv):
    if len(argv) != 2:
        raise SystemExit(__doc__)
    src, out = argv
    with open(os.path.join(src, "Battle", "SeaBattle.cpp"), encoding="utf-8") as f:
        text = f.read()
    for mark in (FIRST, LAST):
        if text.count(mark) != 1:
            raise SystemExit(f"lamps_extract: {mark.strip()!r} found {text.count(mark)} times in SeaBattle.cpp, not once")
    start = text.index(FIRST)
    tail = text.index(LAST)
    end = text.index("\n\t}\n", tail) + len("\n\t}\n")
    if not start < tail < end:
        raise SystemExit("lamps_extract: RuleOf no longer comes before NavalRow")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "lamps_cut.inc"), "w", encoding="utf-8") as f:
        f.write("// Cut from Battle/SeaBattle.cpp by lamps_extract.py: do not edit.\n" + text[start:end])
    print(f"lamps_extract: SeaParts' sea lamps cut, lines {text.count(chr(10), 0, start) + 1}-{text.count(chr(10), 0, end)}")


if __name__ == "__main__":
    main(sys.argv[1:])
