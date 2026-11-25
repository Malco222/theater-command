#!/usr/bin/env python3
"""The ship branch of FMiniatureMesh::Sculpted, cut out of Campaign/CampaignMiniatures.cpp for ship_paint_test (the ship
detail plan, round 1). Two trees' worth: the reference, verbatim as it stood at 55419eb6 (handed in on stdin by run.sh
from `git show`), and the tree's own, as it stands. Each is written as an include the test compiles inside its own
loop over a file's points, against the engine's stand-ins (paint_shim.h):

  old_ship_branch.inc   the reference's `if (Raw->bShip && Raw->PaintColours.Num() > 0) { ... }`, byte for byte;
  new_ship_branch.inc   the tree's own, byte for byte;
  new_ship_palette.inc  the tree's own palette set before the loop (`ShipPaintCore::Palette ShipColours;` and the
                        `if (Raw->bShip) { ... }` after it), byte for byte.

A block is found by its first line and runs to its closing brace (strings, characters and comments skipped). Each must
be found exactly once, or nothing is written and the cut fails.

Usage: git show 55419eb6:<path> | python3 -I -B Tools/ship_detail/extract_paint.py - <tree's CampaignMiniatures.cpp> <out dir>
"""

import os
import sys

HEAD = "if (Raw->bShip && Raw->PaintColours.Num() > 0)"
PALETTE = "ShipPaintCore::Palette ShipColours;"


def close_of(text, start):
    """The index just past the brace that closes the first block opened at or after start."""
    depth, seen, i, n = 0, False, start, len(text)
    while i < n:
        if text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
            continue
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
            continue
        c = text[i]
        if c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            i = j + 1
            continue
        if c == "{":
            depth += 1
            seen = True
        elif c == "}":
            depth -= 1
            if seen and depth == 0:
                return i + 1
        i += 1
    raise SystemExit("extract_paint: a block does not close")


def block(text, head, what):
    found = text.count(head)
    if found != 1:
        raise SystemExit(f"extract_paint: {what}: '{head}' found {found} times (one wanted)")
    start = text.rfind("\n", 0, text.find(head)) + 1
    return text[start:close_of(text, start)] + "\n"


def palette(text):
    found = text.count(PALETTE)
    if found != 1:
        raise SystemExit(f"extract_paint: the tree's palette: '{PALETTE}' found {found} times (one wanted)")
    start = text.rfind("\n", 0, text.find(PALETTE)) + 1
    after = text.find("if (Raw->bShip)", start)
    if after < 0 or text[start:after].count(";") != 1:
        raise SystemExit("extract_paint: the tree's palette: its `if (Raw->bShip)` does not follow it")
    return text[start:close_of(text, after)] + "\n"


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    old = sys.stdin.read() if sys.argv[1] == "-" else open(sys.argv[1], encoding="utf-8").read()
    new = open(sys.argv[2], encoding="utf-8").read()
    out = sys.argv[3]
    parts = {
        "old_ship_branch.inc": block(old, HEAD, "the reference (55419eb6)"),
        "new_ship_branch.inc": block(new, HEAD, "the tree's own"),
        "new_ship_palette.inc": palette(new),
    }
    if "ShipPaintCore" in parts["old_ship_branch.inc"] or "FMath::Lerp(Coat, Raw->Rust, 0.55f * Worn / 100.0f);" not in parts["old_ship_branch.inc"]:
        raise SystemExit("extract_paint: the reference is not the one-colour rust of 55419eb6")
    os.makedirs(out, exist_ok=True)
    for name, body in parts.items():
        with open(os.path.join(out, name), "w", encoding="utf-8") as f:
            f.write(body)
        print(f"extract_paint: {name}, {body.count(chr(10))} lines")
    return 0


if __name__ == "__main__":
    sys.exit(main())
