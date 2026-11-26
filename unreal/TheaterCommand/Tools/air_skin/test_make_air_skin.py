#!/usr/bin/env python3
"""The test of Tools/make_air_skin.py (the worn B-17 paint's overlay; row flyB17Skin).

    python3 -I Tools/air_skin/test_make_air_skin.py [<base B17Fine.json> <model thread's skin file> <overlay>]

On small made-up files: two runs give the same bytes; blend, soot, shade and the repaint list come out as the skin
has them, with the rounding bounded; nothing keyed by month or year and no marks reach the overlay; gloss and metal
only when asked; each refusal (the base's sha256, unequal points/facings/triangles/part/wear, 45 colours, other paint
names, a value out of range). Given the three real files: the committed overlay is what the tool makes from them,
byte for byte, and its check colours are the game's law of its own numbers.
"""
import copy
import hashlib
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import make_air_skin as M  # noqa: E402

FAILS = []


def check(cond, what):
    if not cond:
        FAILS.append(what)
        print("FAIL:", what)


def level(n, seed):
    pts = [float((i * 7 + seed) % 13) / 10.0 for i in range(3 * n)]
    return {
        "points": pts, "facings": [0.0, 0.0, 1.0] * n, "paints": [i % 3 if i % 5 else 11 for i in range(n)],
        "triangles": [0, 1, 2] * (n // 3), "part": [0] * n, "wear": [i % 4 for i in range(n)],
        "blend": [(i * 13) % 85 for i in range(n)], "soot": [(i * 5) % 40 for i in range(n)],
    }


def made_up():
    names = ["p%d" % i for i in range(46)]
    base = {"kind": "aircraft", "colours": ["%02X%02X%02X" % (i * 5, i * 4, i * 3) for i in range(46)], "paint_names": names,
            "marks": [{"tag": "USA-BAR"}], "mark_months": [1, 2, 3], "near": level(12, 1), "far": level(6, 2)}
    skin = copy.deepcopy(base)
    skin["colours"][0] = "5B5A44"
    skin["colours"][11] = "241F1B"
    skin["marks"] = [{"tag": "USA"}]
    del skin["mark_months"]
    skin["schemes"] = [{"from": "1944-04", "colours": {"p0": "FFFFFF"}}]
    for name, lv in (("near", skin["near"]), ("far", skin["far"])):
        n = len(lv["paints"])
        lv["blend"] = [(i * 11) % 101 for i in range(n)]
        lv["soot"] = [(i * 17) % 101 for i in range(n)]
        lv["shade"] = [round(0.12 + (i % 9) * 0.11, 2) for i in range(n)]
        lv["gloss"] = [7 + i for i in range(n)]
        lv["metal"] = [i * 3 for i in range(n)]
    skin["far"]["paints"][4] = 37
    return base, skin


def dump(d):
    return json.dumps(d).encode()


def refused(base, skin, what, prefix=""):
    try:
        M.make(dump(base), dump(skin), base_sha_prefix=prefix)
    except M.Refused:
        return
    FAILS.append("not refused: " + what)
    print("FAIL: not refused:", what)


def made_up_tests():
    base, skin = made_up()
    t1, fig = M.make(dump(base), dump(skin), base_sha_prefix="")
    t2, _ = M.make(dump(base), dump(skin), base_sha_prefix="")
    check(t1 == t2, "two runs, the same bytes")
    d = json.loads(t1)
    check(list(d.keys()) == ["skin_of", "base_sha256", "base_bytes", "base_fnv64", "source_sha256", "source_bytes", "colours", "panel_blend", "near", "far", "check"], "top keys in their order")
    check(d["source_sha256"] == hashlib.sha256(dump(skin)).hexdigest() and d["base_sha256"] == hashlib.sha256(dump(base)).hexdigest(), "the two sha256s")
    check(d["base_fnv64"] == "%016x" % M.fnv64(dump(base)), "fnv64")
    check(M.fnv64(b"") == 0xCBF29CE484222325 and M.fnv64(b"a") == 0xAF63DC4C8601EC8C, "fnv64 of known strings")
    flat = t1.lower()
    for word in ("month", "year", "mark", "scheme", "\"from\"", "usa", "gloss", "metal"):
        check(word not in flat, "no %s in the overlay" % word)
    for name in ("near", "far"):
        lv = d[name]
        check(list(lv.keys()) == ["blend", "soot", "repaint", "shade"], name + " keys")
        check(lv["blend"] == skin[name]["blend"] and lv["soot"] == skin[name]["soot"], name + " blend and soot as the skin's")
        worst = max(abs(q / 100.0 - s) for q, s in zip(lv["shade"], skin[name]["shade"]))
        check(worst <= 0.005 + 1e-12, name + " shade within 0.005 (%.4f)" % worst)
    check(d["far"]["repaint"] == [4, 37] and d["near"]["repaint"] == [], "repaint lists")
    check(d["colours"][0] == "5B5A44" and len(d["colours"]) == 46, "colours")
    lin = [tuple(M.to_linear(v) for v in M.rgb_of(c)) for c in d["colours"]]
    for c in d["check"]:
        lv = d[c["shape"]]
        k = c["point"]
        check(c["rgb"] == M.law(lin, c["paint"], lv["blend"][k], lv["soot"][k]), "check colour %s %d" % (c["shape"], k))
    # shade quantised from a three-decimal value: bounded at half a step
    s3 = copy.deepcopy(skin)
    s3["near"]["shade"] = [0.123 + 0.0007 * i for i in range(len(s3["near"]["shade"]))]
    t3, _ = M.make(dump(base), dump(s3), base_sha_prefix="")
    q = json.loads(t3)["near"]["shade"]
    check(max(abs(a / 100.0 - b) for a, b in zip(q, s3["near"]["shade"])) <= 0.005 + 1e-12, "shade rounding bounded")
    tg, _ = M.make(dump(base), dump(skin), with_gloss_metal=True, base_sha_prefix="")
    dg = json.loads(tg)
    check(dg["near"]["gloss"] == skin["near"]["gloss"] and dg["far"]["metal"] == skin["far"]["metal"], "gloss and metal when asked")
    # refusals
    refused(base, skin, "a base whose sha256 is not the game's", prefix=M.BASE_SHA_PREFIX)
    for name in ("near", "far"):
        for key in M.SAME:
            s = copy.deepcopy(skin)
            v = s[name][key]
            v[0] = v[0] + 1 if not isinstance(v[0], float) else v[0] + 0.5
            refused(base, s, "%s %s unequal" % (name, key))
    s = copy.deepcopy(skin)
    s["colours"] = s["colours"][:45]
    refused(base, s, "45 colours")
    s = copy.deepcopy(skin)
    s["paint_names"][3] = "other"
    refused(base, s, "other paint names")
    s = copy.deepcopy(skin)
    s["near"]["blend"][2] = 101
    refused(base, s, "blend 101")
    s = copy.deepcopy(skin)
    s["far"]["soot"][1] = -1
    refused(base, s, "soot -1")
    s = copy.deepcopy(skin)
    s["far"]["shade"][1] = 1.2
    refused(base, s, "shade 1.2")
    s = copy.deepcopy(skin)
    s["near"]["blend"] = s["near"]["blend"][:-1]
    refused(base, s, "blend one short")
    s = copy.deepcopy(skin)
    del s["far"]["shade"]
    refused(base, s, "no shade")
    s = copy.deepcopy(skin)
    s["far"]["paints"][2] = 46
    refused(base, s, "a paint past the colours")


def real_tests(base_path, skin_path, overlay_path):
    with open(base_path, "rb") as f:
        base = f.read()
    with open(skin_path, "rb") as f:
        skin = f.read()
    with open(overlay_path, "rb") as f:
        overlay = f.read()
    text, _ = M.make(base, skin)
    check(text.encode("ascii") == overlay, "the committed overlay is the tool's of these files, byte for byte")
    d = json.loads(overlay)
    check(d["source_sha256"] == hashlib.sha256(skin).hexdigest(), "the overlay names this skin file")
    check(len(d["near"]["blend"]) == 143899 and len(d["far"]["blend"]) == 15534 and len(d["colours"]) == 46, "her counts")
    try:
        M.make(base[:-2] + b" \n", skin)
        FAILS.append("a base one byte changed not refused")
    except M.Refused:
        pass


def main():
    made_up_tests()
    if len(sys.argv) == 4:
        real_tests(*sys.argv[1:4])
    print("test_make_air_skin: %s" % ("FAILED %d" % len(FAILS) if FAILS else "ok"))
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
