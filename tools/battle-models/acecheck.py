"""Every record slot of a fighter checked on her mesh (game axes: x ahead, y right, port y<0; the JSON's own).
For each kill slot (its disc), each point of the name line (its letters' top, middle and foot) and each rudder bar:
  skin   - every sample, cast in along her facing from 0.3 m out, first meets her own skin (not glass, a frame, a crew
           man, a wing, an exhaust) within 2 cm of where the slot says
  abeam  - the slot's middle seen level from 4 m out to port (and from 20 degrees above) is her skin there, not
           something standing in front of it (a wing root, an exhaust, a propeller)
  marks  - no overlap with a painted mark on that side (true size of the number: x at-0.745H..at+0.635H, z +-0.505H;
           insignia discs; the barred star at its widest +-2.05r; a unit's emblem)
usage: python3 acecheck.py <air dir> Name,Name,..."""
import json, math, sys
import numpy as np
import bpy  # noqa
from mathutils import Vector
from mathutils.bvhtree import BVHTree
AIR, names = sys.argv[1], sys.argv[2].split(",")
SKIN = {"upper", "upper2", "under", "nose", "boss", "rivet", "rivet_under", "seam", "seam_under", "line", "patch", "stain"}
total_bad = 0
for name in names:
    d = json.load(open(f"{AIR}/{name}.json"))
    nm = d["paint_names"]; n = d["near"]
    P = np.array(n["points"]).reshape(-1, 3); T = np.array(n["triangles"]).reshape(-1, 3)
    pa = np.array(n["paints"]); part = np.array(n["part"]); pn = [p["name"] for p in d["parts"]]
    tree = BVHTree.FromPolygons([tuple(v) for v in P], [tuple(t) for t in T])
    t = d["tally"]

    def first(o, dv, far):
        loc, nrm, fi, dist = tree.ray_cast(Vector(o), Vector(dv).normalized(), far)
        if loc is None:
            return None, None, None
        v = T[fi][0]
        return loc, nm[pa[v]], pn[part[v]]

    def on_skin(p, nrm, parts=("plane",)):
        p = Vector(p); nv = Vector(nrm).normalized()
        loc, paint, prt = first(p + nv * 0.3, -nv, 0.6)
        if loc is None:
            return "nothing"
        if paint not in SKIN or not any(prt.startswith(q) for q in parts):
            return f"{prt}/{paint}"
        if (loc - p).length > 0.02:
            return f"off {((loc - p).length):.3f}"
        return None

    def seen(p, parts=("plane",)):
        out = []
        for lift in (0.0, math.radians(20)):
            dv = Vector((0.0, math.cos(lift), -math.sin(lift)))
            o = Vector(p) - dv * 4.0
            loc, paint, prt = first(o, dv, 8.0)
            if loc is None or (loc - Vector(p)).length > 0.03 or paint not in SKIN:
                out.append(f"{'level' if lift == 0 else 'above'}:{prt}/{paint}")
        return out

    # her painted marks on the port side, as boxes in (x, z)
    boxes = []
    for mk in d.get("marks", []):
        if mk.get("place") not in ("side", "code"):
            continue
        if not (mk["at"][1] < 0 or mk.get("both")):
            continue
        x, z, r = mk["at"][0], mk["at"][2], mk["radius"]
        if mk["tag"] == "#":
            boxes.append(("#", x - 0.745 * r, x + 0.635 * r, z - 0.505 * r, z + 0.505 * r))
        elif "BAR" in mk["tag"] or mk["tag"] in ("USA",):
            boxes.append((mk["tag"], x - 2.05 * r, x + 2.05 * r, z - r, z + r))
        else:
            boxes.append((mk["tag"], x - r, x + r, z - r, z + r))
    for em in (d.get("unit_marks") or {}).get("emblems", []):
        if em["at"][1] < 0:
            x, z, r = em["at"][0], em["at"][2], em["radius"]
            boxes.append(("emblem", x - r, x + r, z - r, z + r))

    def hits(x0, x1, z0, z1):
        return [b[0] for b in boxes if x0 < b[2] and x1 > b[1] and z0 < b[4] and z1 > b[3]]

    bad = []
    k = t.get("kills")
    if k:
        r = k["radius"]
        for i, s in enumerate(k["slots"]):
            p, nv = Vector(s[:3]), Vector(s[3:]).normalized()
            up = Vector((0, 0, 1)); v = (up - nv * up.dot(nv)).normalized(); u = v.cross(nv)
            samples = [p] + [p + (u * math.cos(a) + v * math.sin(a)) * r * 0.95 for a in np.linspace(0, 2 * math.pi, 8, endpoint=False)]
            why = [w for w in (on_skin(q, nv) for q in samples) if w]
            why += seen(p)
            why += ["mark " + m for m in hits(p.x - r, p.x + r, p.z - r, p.z + r)]
            if why:
                bad.append(("kill", i + 1, round(p.x, 2), round(p.z, 3), sorted(set(why))))
    nmL = t.get("name")
    if nmL:
        c = nmL["cap"]
        for i, s in enumerate(nmL["line"]):
            p, nv = Vector(s[:3]), Vector(s[3:]).normalized()
            up = Vector((0, 0, 1)); v = (up - nv * up.dot(nv)).normalized()
            samples = [p, p + v * 0.5 * c, p - v * 0.5 * c]
            why = [w for w in (on_skin(q, nv) for q in samples) if w]
            why += seen(p)
            why += ["mark " + m for m in hits(p.x - 0.05, p.x + 0.05, p.z - c / 2, p.z + c / 2)]
            if why:
                bad.append(("name", i, round(p.x, 2), round(p.z, 3), sorted(set(why))))
        # the name line clear of the first row of kills
        if k:
            gapz = min(s[2] for s in nmL["line"]) - 0.5 * c * abs(nmL["line"][0][4]) - max(s[2] + k["radius"] * abs(s[4]) for s in k["slots"][:k["per_row"]])
            if gapz < 0.0:
                bad.append(("name", "-", 0, round(gapz, 3), ["overlaps row 1"]))
    rd = t.get("rudder")
    if rd:
        for i, s in enumerate(rd["slots"]):
            p = Vector(s[:3]); nv = Vector(s[3:]).normalized()
            w = on_skin(p, nv, parts=("rudder", "tail"))
            if w:
                bad.append(("bar", i + 1, round(p.x, 2), round(p.z, 3), [w]))
    nk = len(k["slots"]) if k else 0
    print(f"{name:22s} kills {nk:2d}, name pts {len(nmL['line']) if nmL else 0:2d}, bars {len(rd['slots']) if rd else 0:2d}: {'CLEAR' if not bad else str(len(bad)) + ' faults'}")
    for b in bad[:12]:
        print("    ", b)
    if len(bad) > 12:
        print("     ...", len(bad) - 12, "more")
    total_bad += len(bad)
print("TOTAL faults", total_bad)
