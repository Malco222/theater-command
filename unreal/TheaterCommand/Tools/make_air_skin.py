#!/usr/bin/env python3
"""The worn paint's overlay for the finer B-17 (the Battle rules card's "Worn B-17 paint", row flyB17Skin).

The values are the model thread's, never this tool's: it EXTRACTS them from the model thread's skinned file of the
same shape (tools/battle-models/skinexport.py's output, e.g. issue-4/B17.json) and writes only what the game's file
lacks, beside Content/Models/B17Fine.json, which it never changes:

    python3 -I Tools/make_air_skin.py --base Content/Models/B17Fine.json --skin <model thread's B17.json> \
        --out Content/Data/AirSkin/B17Fine.json [--with-gloss-metal]

What it writes (one line, keys in a fixed order, no time stamp: the same inputs give the same bytes):
  skin_of, base_sha256, base_bytes, base_fnv64 (FNV-1a 64 of the base's bytes, for the std-only cloud test),
  source_sha256, source_bytes,
  colours        the skin's 46 paint colours, "RRGGBB"
  panel_blend    the median blend of the near paint-0 points (the nose-art panel's under-colour in the game)
  near / far     blend, soot   integers 0-100 a point (the game's reader's own scale: blend/100 toward paint 1 on
                               paint 0, 0.72 x soot/100 toward paint 11 on any paint)
                 repaint       [point, paint, point, paint, ...]: where the skin paints a point otherwise (far today:
                               160 airscrew domes from engine to hub; near: none)
                 shade         integers 0-100 a point: the skin's contact shade (0 to 1) x 100, rounded
                 gloss, metal  only with --with-gloss-metal (a later material's)
  check          points with the colour the game's law gives them (no shade, and with shade at full strength), so
                 that the C++ law is tied to this file (Tools/air_show/air_b17_skin_test.cpp)
Never marks, never anything by month or year: the game's own star and bar stays, and paint never depends on the date.

It refuses (exit 2) unless the base's sha256 begins 5e1f39002423, the two files' points, facings, triangles, part and
wear are equal in near and far, both have 46 colours and the same paint names, and every value is in range; also when
a file is not JSON, a colour is not hex, or the skin holds a dated scheme ("schemes", a key holding "month", a "from"),
which is refused rather than quietly dropped.
Stdlib only (json, hashlib).
"""
import argparse
import hashlib
import json
import sys

BASE_SHA_PREFIX = "5e1f39002423"
PAINTS = 46
SAME = ("points", "facings", "triangles", "part", "wear")
# (which paints the check points sample, by number: coat, under, glass, line, soot, engine, hub, rivets, seams, ...)
CHECK_PAINTS = (0, 1, 2, 3, 5, 6, 10, 11, 12, 17, 28, 29, 37, 42, 43, 44, 45)


class Refused(Exception):
    pass


def fnv64(data):
    h = 0xCBF29CE484222325
    for b in data:
        h ^= b
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def to_linear(v):
    c = v / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def to_srgb_byte(lin):
    lin = min(max(lin, 0.0), 1.0)
    c = lin * 12.92 if lin <= 0.0031308 else 1.055 * lin ** (1.0 / 2.4) - 0.055
    return int(min(max(c, 0.0), 1.0) * 255.0 + 0.5)


def rgb_of(hexed):
    return tuple(int(hexed[i:i + 2], 16) for i in (0, 2, 4))


def law(colours_lin, paint, blend, soot, shade=1.0):
    """CampaignMiniatures.cpp's aircraft law: paint 0 toward paint 1 by blend, toward paint 11 by 0.72 x soot, all
    linear, then (the skin's commit 3) times the shade; written as sRGB bytes."""
    c = list(colours_lin[paint])
    if paint == 0 and blend > 0:
        t = min(blend, 100) / 100.0
        c = [a + (b - a) * t for a, b in zip(colours_lin[0], colours_lin[1])]
    if soot > 0:
        t = 0.72 * min(soot, 100) / 100.0
        c = [a + (b - a) * t for a, b in zip(c, colours_lin[11])]
    c = [a * shade for a in c]
    return "".join("%02X" % to_srgb_byte(a) for a in c)


def ints_0_100(values, what):
    out = []
    worst = 0.0
    for v in values:
        if not isinstance(v, (int, float)) or isinstance(v, bool):
            raise Refused("%s: not a number" % what)
        q = int(round(v))
        if q < 0 or q > 100:
            raise Refused("%s: %r is outside 0-100" % (what, v))
        worst = max(worst, abs(q - v))
        out.append(q)
    return out, worst


def loads(data, what):
    """The file's JSON; a file that is not JSON (cut short, or not text) is refused, not a traceback."""
    try:
        return json.loads(data)
    except (ValueError, UnicodeDecodeError) as e:
        raise Refused("the %s is not JSON (%s)" % (what, e))


def dated_key(node, path=""):
    """The first key that would date the paint: "schemes" at the top, any key holding "month", or a key "from". A key
    "year" is not one (the model thread's file says the year it was made); the game's reader refuses all of them."""
    stack = [(node, path)]
    while stack:
        x, at = stack.pop()
        if isinstance(x, dict):
            for k, v in x.items():
                low = str(k).lower()
                if (at == "" and low == "schemes") or "month" in low or low == "from":
                    return "the key %s/%s" % (at, k)
                if isinstance(v, (dict, list)):
                    stack.append((v, at + "/" + str(k)))
        elif isinstance(x, list):
            for v in x:
                if isinstance(v, (dict, list)):
                    stack.append((v, at + "[]"))
    return None


def make(base_bytes, skin_bytes, with_gloss_metal=False, base_sha_prefix=BASE_SHA_PREFIX):
    """The overlay's text (and a few figures for the log) from the two files' bytes; raises Refused. (base_sha_prefix:
    the tool's test alone passes another, for its small made-up files.)"""
    base_sha = hashlib.sha256(base_bytes).hexdigest()
    if not base_sha.startswith(base_sha_prefix):
        raise Refused("the base is not the game's B17Fine.json (sha256 %s, wanted %s...)" % (base_sha[:16], base_sha_prefix))
    base = loads(base_bytes, "base")
    skin = loads(skin_bytes, "skin")
    dated = dated_key(skin)
    if dated:
        raise Refused("the skin holds %s: paint never depends on the date, so a dated scheme is refused, not dropped" % dated)
    for name, d in (("base", base), ("skin", skin)):
        if len(d.get("colours") or []) != PAINTS:
            raise Refused("the %s has %d colours, not %d" % (name, len(d.get("colours") or []), PAINTS))
    if base.get("paint_names") != skin.get("paint_names"):
        raise Refused("the two files name their paints differently")
    colours = []
    for c in skin["colours"]:
        if not isinstance(c, str) or len(c.lstrip("#")) != 6:
            raise Refused("a colour that is not RRGGBB: %r" % (c,))
        c = c.lstrip("#").upper()
        if any(ch not in "0123456789ABCDEF" for ch in c):
            raise Refused("a colour that is not RRGGBB: %r" % (c,))
        colours.append(c)
    figures = {}
    levels = {}
    for level in ("near", "far"):
        b = base.get(level)
        s = skin.get(level)
        if not isinstance(b, dict) or not isinstance(s, dict):
            raise Refused("no %s mesh in both files" % level)
        for key in SAME:
            if b.get(key) != s.get(key):
                raise Refused("%s %s differ between the base and the skin" % (level, key))
        n = len(b["paints"])
        if len(s.get("paints") or []) != n:
            raise Refused("%s paints: %d against %d points" % (level, len(s.get("paints") or []), n))
        out = {}
        worst = 0.0
        for key in ("blend", "soot"):
            vals = s.get(key)
            if not isinstance(vals, list) or len(vals) != n:
                raise Refused("%s %s: not one a point" % (level, key))
            out[key], w = ints_0_100(vals, level + " " + key)
            worst = max(worst, w)
        repaint = []
        for k, (pb, ps) in enumerate(zip(b["paints"], s["paints"])):
            if pb != ps:
                if not isinstance(ps, int) or ps < 0 or ps >= PAINTS:
                    raise Refused("%s paint %r at %d" % (level, ps, k))
                repaint += [k, ps]
        out["repaint"] = repaint
        shade = s.get("shade")
        if not isinstance(shade, list) or len(shade) != n:
            raise Refused("%s shade: not one a point" % level)
        if any((not isinstance(v, (int, float))) or v < 0.0 or v > 1.0 for v in shade):
            raise Refused("%s shade outside 0 to 1" % level)
        out["shade"], wshade = ints_0_100([v * 100.0 for v in shade], level + " shade")
        if with_gloss_metal:
            for key in ("gloss", "metal"):
                vals = s.get(key)
                if not isinstance(vals, list) or len(vals) != n:
                    raise Refused("%s %s: not one a point" % (level, key))
                out[key], _ = ints_0_100(vals, level + " " + key)
        levels[level] = out
        figures[level] = {"points": n, "repaint": len(repaint) // 2, "blend_err": worst, "shade_err": wshade / 100.0,
                          "blend_changed": sum(1 for x, y in zip(b.get("blend") or [], out["blend"]) if x != y),
                          "soot_changed": sum(1 for x, y in zip(b.get("soot") or [], out["soot"]) if x != y)}
    # The check points: for each level and each sampled paint the point of it most mixed (soot, then blend; the
    # lowest number on a tie), the point of paint 0 furthest into paint 1, and the first point repainted.
    lin = [tuple(to_linear(v) for v in rgb_of(c)) for c in colours]
    check = []
    for level in ("near", "far"):
        o = levels[level]
        paints = list(skin[level]["paints"])
        best = {}
        bestblend = None
        for k, p in enumerate(paints):
            score = o["soot"][k] * 101 + o["blend"][k]
            if p in CHECK_PAINTS and (p not in best or score > best[p][0]):
                best[p] = (score, k)
            if p == 0 and (bestblend is None or o["blend"][k] > bestblend[0]):
                bestblend = (o["blend"][k], k)
        picked = [best[p][1] for p in CHECK_PAINTS if p in best]
        if bestblend is not None and bestblend[1] not in picked:
            picked.append(bestblend[1])
        if o["repaint"] and o["repaint"][0] not in picked:
            picked.append(o["repaint"][0])
        for k in picked:
            p = paints[k]
            check.append({"shape": level, "point": k, "paint": p,
                          "rgb": law(lin, p, o["blend"][k], o["soot"][k]),
                          "shaded": law(lin, p, o["blend"][k], o["soot"][k], o["shade"][k] / 100.0)})
    # (the nose-art panel's under-colour under the skin: paint 0 at the median blend of the near paint-0 points)
    zero = sorted(levels["near"]["blend"][k] for k, p in enumerate(skin["near"]["paints"]) if p == 0)
    panel_blend = zero[len(zero) // 2] if zero else 0
    doc = {
        "skin_of": "B17Fine",
        "base_sha256": base_sha,
        "base_bytes": len(base_bytes),
        "base_fnv64": "%016x" % fnv64(base_bytes),
        "source_sha256": hashlib.sha256(skin_bytes).hexdigest(),
        "source_bytes": len(skin_bytes),
        "colours": colours,
        "panel_blend": panel_blend,
        "near": levels["near"],
        "far": levels["far"],
        "check": check,
    }
    text = json.dumps(doc, separators=(",", ":"), ensure_ascii=True) + "\n"
    return text, figures


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--base", required=True, help="the game's Content/Models/B17Fine.json")
    ap.add_argument("--skin", required=True, help="the model thread's skinned B-17 file of the same shape")
    ap.add_argument("--out", required=True, help="Content/Data/AirSkin/B17Fine.json")
    ap.add_argument("--with-gloss-metal", action="store_true", help="carry the skin's gloss and metal too (a later material's)")
    a = ap.parse_args(argv)
    with open(a.base, "rb") as f:
        base_bytes = f.read()
    with open(a.skin, "rb") as f:
        skin_bytes = f.read()
    try:
        text, figures = make(base_bytes, skin_bytes, a.with_gloss_metal)
    except Refused as e:
        print("make_air_skin: refused: %s" % e, file=sys.stderr)
        return 2
    with open(a.out, "w", encoding="ascii", newline="\n") as f:
        f.write(text)
    for level, fig in figures.items():
        print("make_air_skin: %s: %d points, %d repainted, blend changed at %d, soot at %d, worst rounding: blend/soot %.3f, shade %.4f"
              % (level, fig["points"], fig["repaint"], fig["blend_changed"], fig["soot_changed"], fig["blend_err"], fig["shade_err"]))
    print("make_air_skin: wrote %s, %d bytes, sha256 %s" % (a.out, len(text), hashlib.sha256(text.encode("ascii")).hexdigest()))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
