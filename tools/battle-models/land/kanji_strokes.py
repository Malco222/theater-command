"""The Japanese characters a Type 97's crew brushes on her turret, as strokes (Brian's pick b477).

    python kanji_strokes.py <folder of KanjiVG .svg files> [brush-kanji.json]

The art director's ruling (2026-11-18): never Latin letters on a Japanese tank; the historian gives the checked
characters (historian/tank-names-b477.json, JAP `chars`). A character is not ours to invent, so each one's strokes
(their order, where each begins and ends, its kind) are read from KanjiVG, the open stroke-order record of the
written characters, and kept here as short runs of points; `brushhand.column()` then paints them with a crew's
brush and a hand's unevenness, as it does the capitals. Nothing of a printed face is used.

KanjiVG: copyright (C) 2009-2011 Ulrich Apel, https://kanjivg.tagaini.net, Creative Commons Attribution-Share Alike
3.0 (http://creativecommons.org/licenses/by-sa/3.0/). `brush-kanji.json` is built upon it and is under the same
licence; it says so itself. Fetch a character's file as kanji/0<its code in hex, five places>.svg from
https://github.com/KanjiVG/kanjivg and give this tool the folder.

Stroke ends, from KanjiVG's kinds: "sweep" (the brush is drawn off to a point: the falling strokes, the rising flick,
every hook), "dab" (a dot: set down light, pressed, lifted), "stop" (pressed down at both ends: the level and upright
strokes and their corners).
"""
import json
import os
import re
import sys

SWEEPS = "㇒㇏㇀㇁㇂㇃㇆㇇㇈㇉㇋㇙㇚㇛㇟㇡㇓"
BOX = 109.0
SPAN = 92.0                                                 # a full character fills about this much of KanjiVG's box


def _path(d):
    """The points along one SVG path (KanjiVG writes M, then C/c/S/s curves; L/l for the sake of it)."""
    toks = re.findall(r"[MmCcSsLl]|-?\d*\.?\d+(?:[eE][-+]?\d+)?", d)
    out, i, cmd, cur, last_c2 = [], 0, None, (0.0, 0.0), None
    def num():
        nonlocal i
        v = float(toks[i]); i += 1
        return v
    while i < len(toks):
        if toks[i] in "MmCcSsLl":
            cmd = toks[i]; i += 1
        rel = cmd.islower()
        base = cur if rel else (0.0, 0.0)
        if cmd in "Mm":
            cur = (base[0] + num(), base[1] + num())
            out.append(cur)
            cmd = "l" if rel else "L"
            last_c2 = None
        elif cmd in "Ll":
            cur = (base[0] + num(), base[1] + num())
            out.append(cur)
            last_c2 = None
        else:
            if cmd in "Cc":
                c1 = (base[0] + num(), base[1] + num())
            else:                                           # S/s: the first handle mirrors the last curve's second
                c1 = (2 * cur[0] - last_c2[0], 2 * cur[1] - last_c2[1]) if last_c2 else cur
            c2 = (base[0] + num(), base[1] + num())
            end = (base[0] + num(), base[1] + num())
            for k in range(1, 11):
                t = k / 10.0
                a, b, c, e = (1 - t) ** 3, 3 * (1 - t) ** 2 * t, 3 * (1 - t) * t * t, t ** 3
                out.append((a * cur[0] + b * c1[0] + c * c2[0] + e * end[0], a * cur[1] + b * c1[1] + c * c2[1] + e * end[1]))
            cur, last_c2 = end, c2
    return out


def _fewer(pts, tol):
    """The same run with only the points it needs (Ramer, Douglas, Peucker)."""
    if len(pts) < 3:
        return pts
    (ax, ay), (bx, by) = pts[0], pts[-1]
    dx, dy = bx - ax, by - ay
    d = (dx * dx + dy * dy) ** 0.5 or 1e-9
    far, at = 0.0, 0
    for k in range(1, len(pts) - 1):
        off = abs((pts[k][0] - ax) * dy - (pts[k][1] - ay) * dx) / d
        if off > far:
            far, at = off, k
    if far <= tol:
        return [pts[0], pts[-1]]
    return _fewer(pts[:at + 1], tol)[:-1] + _fewer(pts[at:], tol)


def read(folder):
    table = {}
    for fn in sorted(os.listdir(folder)):
        if not fn.endswith(".svg"):
            continue
        ch = chr(int(fn[:-4], 16))
        text = open(os.path.join(folder, fn), encoding="utf-8").read()
        strokes = []
        for kind, d in re.findall(r'<path[^>]*kvg:type="([^"]*)"[^>]*\sd="([^"]*)"', text):
            pts = [((x - BOX / 2) / SPAN + 0.5, 0.5 - (y - BOX / 2) / SPAN) for (x, y) in _path(d)]
            first = kind[0]
            end = "dab" if first == "㇔" else "sweep" if first in SWEEPS else "stop"
            strokes.append({"end": end, "pts": [[round(x, 3), round(y, 3)] for (x, y) in _fewer(pts, 0.007)]})
        table[ch] = {"strokes": strokes}
    return table


if __name__ == "__main__":
    folder = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "brush-kanji.json")
    chars = read(folder)
    doc = {"about": "Japanese characters as brush strokes for a tank crew's name (b477): for each character its strokes in the order they are written, each a run of "
                    "points in a square of 1 (x from the left, y up from the foot), and how the brush leaves it: `stop` (pressed down at both ends), `sweep` "
                    "(drawn off to a point) or `dab` (a dot). Painted by brushhand.column(), one character under another.",
           "source": "Stroke order and centre lines after KanjiVG, copyright (C) 2009-2011 Ulrich Apel, https://kanjivg.tagaini.net",
           "licence": "Creative Commons Attribution-Share Alike 3.0, http://creativecommons.org/licenses/by-sa/3.0/ (this table is built upon KanjiVG and is under the same licence)",
           "characters": chars}
    with open(out, "w", encoding="utf-8") as f:
        json.dump(doc, f, ensure_ascii=False, separators=(",", ":"))
    print("wrote %s: %d characters, %d strokes, %d points" % (out, len(chars), sum(len(c["strokes"]) for c in chars.values()),
                                                          sum(len(s["pts"]) for c in chars.values() for s in c["strokes"])))
    for ch, c in chars.items():
        xs = [p[0] for s in c["strokes"] for p in s["pts"]]
        ys = [p[1] for s in c["strokes"] for p in s["pts"]]
        print(" ", ch, len(c["strokes"]), "strokes", "x %.2f..%.2f y %.2f..%.2f" % (min(xs), max(xs), min(ys), max(ys)), "".join(s["end"][0] for s in c["strokes"]))
