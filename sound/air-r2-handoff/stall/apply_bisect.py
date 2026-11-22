#!/usr/bin/env python3
"""The gun-take bisect's mix.json edits, for the Mac (no rebuild): python3 apply_bisect.py B1|B2|B3|L1 <mix.json>

Edits only the named fields' values, line by line (the rest of the file is left byte for byte), then checks that the
file still parses and that, parsed, it differs from what it was only at the intended paths. Undo with
  git checkout -- unreal/TheaterCommand/Content/Data/Sound/mix.json
B1: the bombers' gunners silent (heavy.turret, heavy.handHeld, handHeldRifle.takes).
B2: every gun take silent (B1 + heavy.wing, cannon.take, rifle.takes).
B3: B2 + rounds passing the eye silent (air.roundsPassing.within 0).
L1: the B-17 formation loop and the ridden B-17's interior back to r2d's takes (patch 0019 undone in the mix only;
    b17_drone_loop and b17_interior_loop are still in Sounds/Real and in the game's content).
"""
import json, re, sys

EDITS = {
    # (block path, key, expected old value as written, new value as written)
    "B1": [("air.gunsByCalibre.heavy", "turret", '"gunner_burst"', '""'),
           ("air.gunsByCalibre.heavy", "handHeld", '"gunner_burst"', '""'),
           ("air.gunsByCalibre.handHeldRifle", "takes", '["mg42_burst_01", "mg42_burst_02"]', '[]')],
}
EDITS["B2"] = EDITS["B1"] + [("air.gunsByCalibre.heavy", "wing", '"plane_guns"', '""'),
                             ("air.gunsByCalibre.cannon", "take", '"flak_light_fire"', '""'),
                             ("air.gunsByCalibre.rifle", "takes", '["mg42_burst_01", "mg42_burst_02"]', '[]')]
EDITS["B3"] = EDITS["B2"] + [("air.roundsPassing", "within", "40", "0")]
EDITS["L1"] = [("air.loops", "B-17", '"b17_formation_drone_loop"', '"b17_drone_loop"'),
               ("air.closeView.bed.interior", "B-17", '["b17_interior_whiteman_loop", "b17_interior_sioux_loop"]', '"b17_interior_loop"')]


def block_span(lines, path):
    """The lines of the object at path (a.b.c): each step is a line '"step": {' one brace level inside the last.
    (Brace depth counted per line, braces inside strings included: the mix's notes hold none; checked by the parse
    and the whole-object comparison in main, which catch any wrong line.)"""
    depth_before = []
    d = 0
    for line in lines:
        depth_before.append(d)
        d += line.count("{") - line.count("}")
    start, end, inner = 0, len(lines), 1
    for step in path.split("."):
        pat = re.compile(r'^\s*"%s"\s*:\s*\{' % re.escape(step))
        found = [i for i in range(start, end) if depth_before[i] == inner and pat.match(lines[i])]
        if len(found) != 1:
            sys.exit("block %s: found %d opening lines for %s" % (path, len(found), step))
        i = found[0]
        j = i
        while j < end and not (j > i and depth_before[j] == inner):
            j += 1
        start, end, inner = i, j, inner + 1
    return start, end


def get(obj, path):
    for s in path.split("."):
        obj = obj[s]
    return obj


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in EDITS:
        sys.exit(__doc__)
    which, file = sys.argv[1], sys.argv[2]
    text = open(file, encoding="utf-8").read()
    before = json.loads(text)
    lines = text.split("\n")
    for path, key, old, new in EDITS[which]:
        s, e = block_span(lines, path)
        pat = re.compile(r'^(\s*"%s"\s*:\s*)%s(\s*,?\s*)$' % (re.escape(key), re.escape(old)))
        hits = [i for i in range(s, e) if pat.match(lines[i])]
        if len(hits) != 1:
            sys.exit("%s.%s: expected exactly one line '%s: %s' in lines %d-%d, found %d: not edited" % (path, key, key, old, s + 1, e, len(hits)))
        i = hits[0]
        lines[i] = pat.sub(lambda m: m.group(1) + new + m.group(2), lines[i])
        print("line %d: %s.%s %s -> %s" % (i + 1, path, key, old, new))
    out = "\n".join(lines)
    after = json.loads(out)  # (a parse error stops here, before anything is written)
    want = {(p + "." + k): json.loads(n) for p, k, o, n in EDITS[which]}
    for full, val in want.items():
        assert get(after, full) == val, full
    # nothing else changed: put the old values back and compare the whole object
    for p, k, o, n in EDITS[which]:
        get(after, p)[k] = json.loads(o)
    assert after == before, "something besides the intended fields changed"
    open(file, "w", encoding="utf-8").write(out)
    print("%s written: parses, and differs from before only at %s" % (file, ", ".join(sorted(want))))


main()
