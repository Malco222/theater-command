#!/usr/bin/env python3
"""DrawMarks' own source, cut out of the game for the cloud (the release 2 ocean plan, step 1(a): pay first).

marks_pool_test.cpp plays the game's own ASeaBattle::DrawMarks, today's way and under -TheaterSeaMarksPool=1, over every
recorded action and stand-in. It is not a copy: this cuts the game's text out of Battle/SeaBattle.cpp and .h as it stands
(each piece found exactly once, by its first line, to its closing brace, comments and strings read as such), and
marks_shim.h stands in for the engine names it uses. Each piece carries a #line back to the game's file, so a compile error
in it points at the game's own line. Three files are written into OUT:
  - marks_parts.inc    SeaParts' helpers DrawMarks and LayWakes call (inside the test's namespace SeaParts);
  - marks_structs.inc  ASeaBattle's own structs they use (inside the test's class ASeaBattle);
  - marks_bodies.inc   the member functions themselves: World, BoardOf, LayWakes, DrawMarks and step 1(a)'s
                       DrawMarksPooled and PoolMarks.

Usage: python3 -I -B Tools/sea_waves/marks_extract.py SRC OUT   (SRC: Source/TheaterCommand)
"""

import os
import sys

# (SeaBattle.cpp, SeaParts: whole functions by their first line, and single lines by their start)
PART_FUNCTIONS = [
    "static FVector2D Ahead(double HeadingDegrees)",
    "static FVector2D Starboard(double HeadingDegrees)",
    "static double WakeRun(double Long)",
    "static double WakeWhole(double Lengths, double Run)",
    "static double WakeHalf(double Beam, double Lengths, double Run)",
    "static double Hash01(double X)",
]
PART_LINES = [
    "static constexpr double WakeLife = ",
    "static constexpr int32 MostFoamRings = ",
    "static const FColor Foam(",
]
# (SeaBattle.h: ASeaBattle's own structs)
STRUCTS = ["struct FTrackLine", "struct FWakePoint", "struct FBreakUp"]
# (SeaBattle.cpp: the member functions, by their first line)
BODIES = [
    "FVector ASeaBattle::World(double X, double Y, double Z) const",
    "FVector ASeaBattle::BoardOf(const FVector& WorldAt) const",
    "void ASeaBattle::LayWakes()",
    "void ASeaBattle::DrawMarks()",
    "void ASeaBattle::DrawMarksPooled(",
    "void ASeaBattle::PoolMarks(",
]


class Missing(Exception):
    pass


def skip_quoted(text, at):
    """The index just past the string or character literal starting at at."""
    quote = text[at]
    k = at + 1
    while k < len(text):
        if text[k] == "\\":
            k += 2
            continue
        if text[k] == quote:
            return k + 1
        k += 1
    raise Missing("an unclosed quote at %d" % at)


def closing(text, open_at):
    """The index of the brace closing the one at open_at, comments, strings and characters read as such."""
    depth = 0
    k = open_at
    while k < len(text):
        c = text[k]
        if text.startswith("//", k):
            k = text.find("\n", k)
            k = len(text) if k < 0 else k
            continue
        if text.startswith("/*", k):
            end = text.find("*/", k + 2)
            if end < 0:
                raise Missing("an unclosed comment at %d" % k)
            k = end + 2
            continue
        if c in "\"'":
            k = skip_quoted(text, k)
            continue
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return k
        k += 1
    raise Missing("no closing brace for the one at %d" % open_at)


def line_start(text, at):
    return text.rfind("\n", 0, at) + 1


def once(text, head, where, at_line_start=True):
    """Where head is found, exactly once (at a line's start, after its indent, unless told)."""
    found = []
    k = text.find(head)
    while k >= 0:
        if not at_line_start or text[line_start(text, k):k].strip() == "":
            found.append(k)
        k = text.find(head, k + 1)
    if len(found) != 1:
        raise Missing("%s: %r found %d times, not once" % (where, head, len(found)))
    return found[0]


def block(text, head, where, tail=""):
    """The text from head's line to its closing brace (and tail after it, as a struct's ';'), and its first line."""
    at = once(text, head, where)
    start = line_start(text, at)
    brace = text.find("{", at)
    if brace < 0:
        raise Missing("%s: %r has no body" % (where, head))
    end = closing(text, brace) + 1
    if tail:
        if not text.startswith(tail, end):
            raise Missing("%s: %r not followed by %r" % (where, head, tail))
        end += len(tail)
    return text[start:end], text.count("\n", 0, start) + 1


def single(text, head, where):
    at = once(text, head, where)
    start = line_start(text, at)
    end = text.find("\n", at)
    return text[start:end], text.count("\n", 0, start) + 1


def write(path, pieces, origin):
    with open(path, "w", encoding="utf-8") as f:
        f.write("// Cut out of %s by Tools/sea_waves/marks_extract.py: the game's own text, not a copy to keep.\n" % origin)
        for text, first in pieces:
            f.write('#line %d "%s"\n%s\n' % (first, origin, text))


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    src, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    cpp_path = os.path.abspath(os.path.join(src, "Battle", "SeaBattle.cpp"))
    h_path = os.path.abspath(os.path.join(src, "Battle", "SeaBattle.h"))
    cpp = open(cpp_path, encoding="utf-8").read()
    h = open(h_path, encoding="utf-8").read()
    try:
        parts = [block(cpp, head, "SeaBattle.cpp") for head in PART_FUNCTIONS] + [single(cpp, head, "SeaBattle.cpp") for head in PART_LINES]
        structs = [block(h, head, "SeaBattle.h", ";") for head in STRUCTS]
        bodies = [block(cpp, head, "SeaBattle.cpp") for head in BODIES]
    except Missing as e:
        print("marks_extract: the game's source no longer reads as this cuts it: %s" % e)
        return 1
    write(os.path.join(out, "marks_parts.inc"), parts, cpp_path)
    write(os.path.join(out, "marks_structs.inc"), structs, h_path)
    write(os.path.join(out, "marks_bodies.inc"), bodies, cpp_path)
    for name, (text, first) in zip(PART_FUNCTIONS + PART_LINES + STRUCTS + BODIES, parts + structs + bodies):
        print("  cut  %s: lines %d-%d" % (name, first, first + text.count("\n")))
    print("marks_extract: %d pieces of the game's source cut out into %s" % (len(parts) + len(structs) + len(bodies), out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
