"""Where her skin's seams and rivets lie under her nose art (the art director, 2026-11-20 17:44Z: "The skin shows
through. Rivets and seams press through the paint ... and the paint takes the skin's sheen and dirt").

    python3 noseart_skin.py [name]   -> noseart_skin.json beside this file

The nose art is a picture wrapped on her skin (livery.py, art.port), lifted a little off it, so her own panel lines and
rivets are hidden under it. This finds them: every seam, panel line and rivet of her model (fitout.py) that lies under
the port picture, as the picture's own u, v (0 to 1: u the way it is read, from her nose back; v up). noseart.weather
then presses them through the paint, and wears the paint first along them.
"""
import json
import math
import os
import sys

import bpy  # noqa: F401
from mathutils import Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import livery  # noqa: E402
import planes  # noqa: E402

SEAMS = ("seam", "seam_under", "line")
RIVETS = ("rivet", "rivet_under")


def main(name="B17"):
    m, _ = planes.BUILDERS[name](True)
    d = livery.overlay(name, True, model=m)
    art = d.part_at["art.port"]
    tris = [f for k, f in enumerate(d.faces) if d.fpart[k] == art]
    verts = [d.verts[i] for f in tris for i in f]
    idx = [tuple(range(3 * k, 3 * k + 3)) for k in range(len(tris))]
    uvs = [d.uv[i] for f in tris for i in f]
    tree = BVHTree.FromPolygons([tuple(v) for v in verts], idx)

    def uv_of(p):
        """The picture's u, v over her skin's point p, or None if it is not under the picture."""
        hit = tree.find_nearest(p, 0.08)
        if hit[0] is None:
            return None
        loc, _, k, _ = hit
        a, b, c = (verts[i] for i in idx[k])
        v0, v1, v2 = b - a, c - a, loc - a
        d00, d01, d11, d20, d21 = v0.dot(v0), v0.dot(v1), v1.dot(v1), v2.dot(v0), v2.dot(v1)
        den = d00 * d11 - d01 * d01
        if abs(den) < 1e-12:
            return None
        w1 = (d11 * d20 - d01 * d21) / den
        w2 = (d00 * d21 - d01 * d20) / den
        w0 = 1.0 - w1 - w2
        ua, ub, uc = (uvs[i] for i in idx[k])
        u = w0 * ua[0] + w1 * ub[0] + w2 * uc[0]
        v = w0 * ua[1] + w1 * ub[1] + w2 * uc[1]
        if -0.002 <= u <= 1.002 and -0.002 <= v <= 1.002:
            return round(u, 5), round(v, 5)
        return None

    names = list(m.paints)
    seams, rivets = [], []
    for k, f in enumerate(m.faces):
        paint = names[m.fpaint[k]]
        if paint not in SEAMS and paint not in RIVETS:
            continue
        pts = [m.verts[i] for i in f]
        c = sum(pts, Vector()) / len(pts)
        if c.y < 0.0 or not (4.6 < c.x < 7.7 and -1.4 < c.z < 0.6):
            continue
        if paint in RIVETS:
            q = uv_of(c)
            if q:
                rivets.append(q)
            continue
        if len(pts) != 4:
            continue
        # (a strip's quad: the middle of each of its short ends)
        e = [(pts[i] - pts[(i + 1) % 4]).length for i in range(4)]
        j = 0 if e[0] + e[2] < e[1] + e[3] else 1
        a = (pts[j] + pts[j + 1]) / 2
        b = (pts[(j + 2) % 4] + pts[(j + 3) % 4]) / 2
        qa, qb = uv_of(a), uv_of(b)
        if qa and qb:
            seams.append([qa[0], qa[1], qb[0], qb[1]])
    out = {"for": name, "picture": "art.port", "size_m": list(d.info["file"]["art"]["size_m"]),
           "about": "Her seams and panel lines (u0, v0, u1, v1) and rivets (u, v) under her port nose art, in the "
                    "picture's own u, v (u from her nose back, v up): noseart.weather presses them through the paint.",
           "seams": seams, "rivets": rivets}
    path = os.path.join(HERE, "noseart_skin.json")
    with open(path, "w") as fh:
        json.dump(out, fh, separators=(",", ":"))
    print("seams", len(seams), "rivets", len(rivets), "->", path)


if __name__ == "__main__":
    main(*(sys.argv[1:2] or ["B17"]))
