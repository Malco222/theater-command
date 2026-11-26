"""The bombers' nose art for the air battle (Battle/AirNoseArt, AirBattleNoseArt.cpp; release 2): the modeller's 200
paintings (battle-models/air/noseart, done.json; the air thread's names.json), packed for the game, and the panel each
is painted on, laid on the game's own B-17 (her near shape, Content/Models/B17.json).

Brian, 25 November 2026, 01:30Z: nose art on "american only" bombers. The paintings are the modeller's own and are
never altered: no recolour, no crop, nothing added. Each is only made smaller, the same for all: a quarter of the
modeller's size each way (1408 by 864 to 352 by 216, 1.6 pixels a centimetre on her 2.2 m by 1.35 m panel: the art is
seen from the next aircraft in the box, 30 m off and more, where 2.2 m is some 110 pixels across; and the 200 at full
size are 93 MB). Each pixel is the four by four the modeller painted under it, their colours averaged as light and
weighed by how much of each is paint, so a painting's edge keeps its own colour; a wholly clear block keeps the plain
average of the modeller's own clear colours (her olive, so that the game's smaller copies, CampaignTextures's plain
average of four, draw no dark rim round a painting seen from farther off). Lossless PNG: a palette of 256 colours moved
some of the sign painter's colours by up to 40 levels, a recolour.

Writes Content/Data/NoseArt:
  art-NNN.png   each painted number (RGBA, its clear background kept clear)
  index.json    each of the list's numbers: its name (names.json), painted or bare, its file and size; the list's
                chance of a bare nose (none: a new replacement)
  panels.json   where the painting sits on each model that wears one (only the B-17: the B-24, B-25 and B-29 are not
                modelled), port and starboard, in the model file's own frame (metres, x forward, y right, z up)

The panel: the side the air thread's names.json gives ("left side of the nose, below and behind the navigator's
windows, ahead of the cockpit"), the painting's 2.2 m from 7.2 m to 5.0 m forward of her middle (the olive between the
glass nose at 7.5 m and the pilots' windscreen above 5.0 m), its middle 23 degrees above her widest line: her olive runs
from some 19 degrees under that line (her grey belly's wavy edge) to her windscreen's foot, 63 degrees over it at 6.6 m,
and every painting lies on it with some 4 cm to spare. Laid on her skin as a grid over the part of the picture that any
of the paintings has paint in (cover, a pixel and more round all of their paint: the rest of each picture is clear, and
drawn nowhere): each column a cut across her nose, each row the same length along her skin from the painting's middle
row at every column (so the painting keeps its shape as her nose narrows); each point where a ray from her axis meets
her near shape, lifted off it along her skin's own facing far enough that no part of the grid's flat triangles dips
into her (the most a triangle sinks between her facets, and 2 mm more, as the tallies stand off her). The starboard
panel is the port one mirrored across her middle, its picture read the other way along (u = 1 - u), so the lettering
reads from left to right on both sides: seen from outside her port side her tail is on the right, from outside her
starboard side her nose. Picture places: u from the painting's left edge (0) to its right (1), v from its top (0) to
its bottom (1), as its pixels run.

Checks, and fails if one does not hold: every painting the list says is painted is here, 1408 by 864 RGBA; its outer
four pixels all round are clear (nothing of the painting is cut at the frame's edge); the packed folder is under 20 MB;
the panel lies on her olive (paint 0) everywhere, on both sides, no point of it inside her or more than 15 mm off her.
(Battle/AirNoseArt's cloud test, Tools/air_show/air_nose_art_test.cpp, checks the panel again as the game reads it.)

    python3 Tools/pack_nose_art.py <modeller's noseart folder> <air thread's names.json>

The finer B-17 (Content/Models/B17Fine.json, the Battle rules card's "Finer B-17", row b17Fine) has a panel of her own,
in a file of its own, so that panels.json stays as it is:

    python3 Tools/pack_nose_art.py --fine

writes Content/Data/NoseArt/panels-fine.json from her near shape, with the cover panels.json already has (the
paintings are not read again). Her skin differs from the older file's in three ways, each measured on her file:
- her nose is glazed further back and has windows where the older panel lay (the navigator's at 7.0-7.3 m and 5.8-6.4
  m forward, 10 to 45 degrees up), her windscreen's foot steps out of her skin aft of 5.7 m some 50 degrees up, and
  her kill tally's panel is under her window from 3.7 to 5.05 m;
- her coat carries rivet heads and seam lines of paints of their own (42 rivet, 44 seam, the coat's colour a shade
  darker), standing up to 8 mm proud of it, in rows across her, so no panel of any useful size lies on paint 0 alone
  (tried: the one placing a map of her skin showed clear of lines, 6.5 m forward and 9 degrees below her widest
  line, still lies on paint 42 when laid at full grain). Hers may lie on paints 0, 42 and 44, her coat and its lines,
  and on nothing else (never glass, her grey belly or its lines). The test checks her panel against that list, which
  the file names (skin_paints), and the older B-17's against paint 0 alone, as before. Lifted clear of the rivet
  heads, a panel stays within the 15 mm of her skin both tests ask, and faces out of her side as the test asks (its
  facing at least 0.8 across), only below her navigator's windows: the largest found is 0.45 of the older one's
  size each way (0.99 m by 0.61 m), from 6.5 m to 5.51 m forward, its middle 9 degrees below her widest line, 2.1 to
  13.6 mm off her, on paints 0 and 42. (Placed higher, between the windows and the windscreen, 55 degrees up, it
  faced 0.71 across, more up than out, and failed that check.) Whether 0.45 reads well enough there is the art
  director's call on the Mac's stills;
- she has her crew and the rooms they sit in inside her, so a ray from her axis first meets them: her skin is where
  it leaves her for the last time (within 1.8 m of her axis), not where it first meets something.

Needs numpy and Pillow.
"""
import json
import os
import sys

import numpy as np
from PIL import Image

SHRINK = 4                       # (1408 by 864 to 352 by 216)
BORDER = 4                       # (the outer pixels that must be clear, at the modeller's size)
SLACK = 2                        # (the cover's pixels round all the paintings' paint, at the packed size)
MOST_BYTES = 20 * 1024 * 1024    # (the whole folder)
FINE = 0.45                      # (the finer B-17's panel, of the older one's size each way: see above)
FINE_MODELS = {"B17Fine": {
    "fore": 6.5, "aft": round(6.5 - 2.2 * FINE, 4),
    "middle_deg": -9.0,
    "cols": 45, "rows": 28,
    "stand": 0.002,
    "paint": 0,
    "skin_paints": [0, 42, 44],  # (her coat, its rivet heads and its seam lines)
    "outer": True,               # (her skin the last of her a ray from her axis leaves, within 1.8 m)
    "scale": FINE,
}}
MODELS = {"B17": {
    "fore": 7.2, "aft": 5.0,     # (metres forward of her middle: the panel's ends)
    "middle_deg": 23.0,          # (the painting's middle row, degrees above her widest line, z = 0)
    "cols": 45, "rows": 28,      # (its grid's points: some 5 cm apart each way)
    "stand": 0.002,              # (off her skin at the least, as the tallies stand)
    "paint": 0,                  # (her olive, upper: the only paint it may lie on)
}}


def to_light(c):
    c = c / 255.0
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def to_eye(c):
    c = np.clip(c, 0.0, 1.0)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1.0 / 2.4) - 0.055) * 255.0


def shrink(a, f):
    """An RGBA picture (h, w, 4 bytes) made f times smaller each way: each pixel the f by f under it, colours averaged as
    light weighed by their paint; a wholly clear block the plain average of its own colours."""
    h, w, _ = a.shape
    paint = a[:, :, 3].astype(np.float64) / 255.0
    light = to_light(a[:, :, :3].astype(np.float64))
    paint_b = paint.reshape(h // f, f, w // f, f)
    light_b = light.reshape(h // f, f, w // f, f, 3)
    held = paint_b.sum(axis=(1, 3))
    weighed = (light_b * paint_b[..., None]).sum(axis=(1, 3))
    plain = light_b.mean(axis=(1, 3))
    colour = np.where(held[..., None] > 1e-9, weighed / np.maximum(held[..., None], 1e-12), plain)
    out = np.zeros((h // f, w // f, 4))
    out[..., :3] = to_eye(colour)
    out[..., 3] = held / (f * f) * 255.0
    return np.clip(np.rint(out), 0, 255).astype(np.uint8)


def ray_hits(o, d, a, b, c, outer=False):
    """The nearest of many triangles a ray meets (Moller and Trumbore): its distance, the triangle and the two
    barycentric weights of its second and third corners; (inf, -1, 0, 0) when none. outer: the farthest it meets
    within OUTER_M instead (the finer B-17's skin, with her crew inside her)."""
    e1 = b - a
    e2 = c - a
    p = np.cross(d, e2)
    det = (e1 * p).sum(1)
    ok = np.abs(det) > 1e-12
    inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
    t0 = o - a
    u = (t0 * p).sum(1) * inv
    q = np.cross(t0, e1)
    v = (d * q).sum(1) * inv
    t = (e2 * q).sum(1) * inv
    hit = ok & (u >= -1e-9) & (v >= -1e-9) & (u + v <= 1 + 1e-9) & (t > 1e-9)
    if outer:
        hit = hit & (t < OUTER_M)
    if not hit.any():
        return np.inf, -1, 0.0, 0.0
    if outer:
        tt = np.where(hit, t, -np.inf)
        i = int(np.argmax(tt))
        return float(tt[i]), i, float(u[i]), float(v[i])
    tt = np.where(hit, t, np.inf)
    i = int(np.argmin(tt))
    return float(tt[i]), i, float(u[i]), float(v[i])


OUTER_M = 1.8                    # (the farthest from her axis a finer B-17's skin is looked for about her nose)


def unit(v):
    n = np.linalg.norm(v)
    return v / n if n > 0 else v


def lay_panel(model_path, size_m, how, cover):
    """The port panel's grid on the model's near shape, over the part of the picture any painting has paint in (cover:
    u0, v0, u1, v1): rows from the painting's top, each its points from its left (her nose, seen from outside her port
    side) [x, y, z, nx, ny, nz, u, v]; and what it lies on."""
    model = json.load(open(model_path))
    near = model["near"]
    pts = np.array(near["points"], dtype=float).reshape(-1, 3)
    facings = np.array(near["facings"], dtype=float).reshape(-1, 3)
    tris = np.array(near["triangles"], dtype=int).reshape(-1, 3)
    paints = np.array(near["paints"], dtype=int)
    part = np.array(near["part"], dtype=int) if "part" in near else np.zeros(len(pts), dtype=int)
    # (her machine's own skin about her nose, both sides: not her bay's doors or load)
    first = pts[tris[:, 0]]
    keep = (part[tris] == 0).all(1) & (np.abs(first[:, 0] - 0.5 * (how["fore"] + how["aft"])) < 2.5)
    if how.get("outer", False):
        # (and, about the finer B-17's nose, only what lies within reach of the panel: her skin, not the far side of her)
        middle = pts[tris].mean(1)
        keep = keep & (np.abs(middle[:, 0] - 0.5 * (how["fore"] + how["aft"])) < 0.5 * (how["fore"] - how["aft"]) + 0.4) & (np.hypot(middle[:, 1], middle[:, 2]) < OUTER_M)
    tris = tris[keep]
    a, b, c = pts[tris[:, 0]], pts[tris[:, 1]], pts[tris[:, 2]]
    outer = bool(how.get("outer", False))

    def cut(x, deg, hand=-1.0):
        """Where a ray from her axis at x, deg above her widest line on her port side (hand -1; 1 her starboard),
        meets her: the point, its facing (her points' own, between the triangle's corners) and its paint."""
        r = np.radians(deg)
        t, i, wu, wv = ray_hits(np.array([x, 0.0, 0.0]), np.array([0.0, hand * np.cos(r), np.sin(r)]), a, b, c, outer)
        if i < 0:
            raise SystemExit("pack_nose_art: no skin at x %.3f, %.1f degrees" % (x, deg))
        corner = tris[i]
        at = (1 - wu - wv) * pts[corner[0]] + wu * pts[corner[1]] + wv * pts[corner[2]]
        facing = unit((1 - wu - wv) * facings[corner[0]] + wu * facings[corner[1]] + wv * facings[corner[2]])
        paint = paints[corner[[0, 1, 2]][np.argmax([1 - wu - wv, wu, wv])]]
        return at, facing, paint

    cols, rows = how["cols"], how["rows"]
    wide, high = size_m
    grid = np.zeros((rows, cols, 3))
    ways = np.zeros((rows, cols, 3))
    under = np.zeros((rows, cols), dtype=int)
    under_s = np.zeros((rows, cols), dtype=int)
    steps = np.arange(-80.0, 80.01, 0.25)
    u0, v0, u1, v1 = cover
    for j in range(cols):
        u = u0 + (u1 - u0) * j / (cols - 1)
        x = how["fore"] - (how["fore"] - how["aft"]) * u
        # (the cut across her: its points, and how far along her skin each is from its foot)
        line = [cut(x, s) for s in steps]
        at = np.array([p for p, _, _ in line])
        along = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(at, axis=0), axis=1))])
        middle = np.interp(how["middle_deg"], steps, along)
        for i in range(rows):
            want = middle + high * (0.5 - (v0 + (v1 - v0) * i / (rows - 1)))
            k = int(np.clip(np.searchsorted(along, want) - 1, 0, len(steps) - 2))
            share = (want - along[k]) / max(along[k + 1] - along[k], 1e-12)
            deg = steps[k] + share * (steps[k + 1] - steps[k])
            grid[i, j], ways[i, j], under[i, j] = cut(x, deg)
            # (and the paint under the same point of her starboard side, for the mirrored panel)
            under_s[i, j] = cut(x, deg, 1.0)[2]

    def radial_gap(p):
        """How far a point is outside her (negative: inside), along the ray from her axis through it."""
        o = np.array([p[0], 0.0, 0.0])
        d = unit(p - o)
        t, i, _, _ = ray_hits(o, d, a, b, c, outer)
        return np.linalg.norm(p - o) - t

    def samples(g):
        out = []
        for i in range(rows - 1):
            for j in range(cols - 1):
                q = (g[i, j], g[i, j + 1], g[i + 1, j + 1], g[i + 1, j])
                for s in np.linspace(0.0, 1.0, 5):
                    for t in np.linspace(0.0, 1.0, 5):
                        # (both triangles of the cell, (0, 1, 2) and (0, 2, 3), as the game lays them)
                        if s + t <= 1.0:
                            out.append(q[0] + s * (q[1] - q[0]) + t * (q[2] - q[0]))
                            out.append(q[0] + s * (q[2] - q[0]) + t * (q[3] - q[0]))
        return out

    # How far her facets let the grid's flat triangles sink into her, then the grid lifted clear of that.
    sink = max(0.0, -min(radial_gap(p) for p in samples(grid)))
    lift = round(sink + how["stand"], 4)
    laid = grid + ways * lift
    # (both sides: the starboard panel is the port one mirrored, on her own starboard skin)
    gaps = [radial_gap(p) for p in samples(laid)] + [radial_gap(p) for p in samples(laid * np.array([1.0, -1.0, 1.0]))]
    points = []
    for i in range(rows):
        for j in range(cols):
            p, n = laid[i, j], ways[i, j]
            places = [u0 + (u1 - u0) * j / (cols - 1), v0 + (v1 - v0) * i / (rows - 1)]
            points.append([round(float(v), 4) for v in (p[0], p[1], p[2], n[0], n[1], n[2])] + [round(float(v), 5) for v in places])
    return model, points, np.concatenate([under.ravel(), under_s.ravel()]), lift, min(gaps), max(gaps)


def side_of(points, rows, cols, starboard):
    """A side's panel: its points (the port grid, mirrored across her middle for starboard, its picture read the other
    way along), and its middle, facing, up and right (the way the picture's u runs) in the file's frame."""
    out = []
    for p in points:
        q = list(p)
        if starboard:
            q[1] = -q[1]
            q[4] = -q[4]
            q[6] = round(1.0 - q[6], 5)
        out.append(q)
    grid = np.array([q[:3] for q in out]).reshape(rows, cols, 3)
    ways = np.array([q[3:6] for q in out]).reshape(rows, cols, 3)
    places = np.array([q[6:8] for q in out]).reshape(rows, cols, 2)
    mi, mj = rows // 2, cols // 2
    middle = grid[mi, mj]
    facing = unit(ways[mi, mj])
    # (up the picture: its top row's way from its bottom row's, at its middle column; right: the way u grows)
    up = unit(grid[0, mj] - grid[rows - 1, mj])
    along = grid[mi, cols - 1] - grid[mi, 0]
    right = unit(along if places[mi, cols - 1, 0] > places[mi, 0, 0] else -along)
    r4 = lambda v: [round(float(x), 4) for x in v]
    return {"middle": r4(middle), "normal": r4(facing), "up": r4(up), "right": r4(right), "points": out}


def lay_all(root, models, size_m, cover, bad):
    """Each model's panel, laid on its near shape (a model's own scale of the painting's size where it has one), and
    what is wrong with it added to bad: it lies on paint other than its skin's, or is not 0.5 to 15 mm off her."""
    panels = {}
    for kind, how in models.items():
        scale = how.get("scale", 1.0)
        size = [round(v * scale, 4) for v in size_m] if scale != 1.0 else size_m
        model, points, under, lift, low, high = lay_panel(os.path.join(root, "Content", "Models", kind + ".json"), size, how, cover)
        allowed = set(how.get("skin_paints", [how["paint"]]))
        off = sorted(set(int(p) for p in under.ravel()) - allowed)
        if off:
            bad.append("%s's panel lies on paint %s, not only %s" % (kind, off, how["paint"] if allowed == {how["paint"]} else sorted(allowed)))
        if low < 0.0005 or high > 0.015:
            bad.append("%s's panel is %.1f to %.1f mm off her skin, not 0.5 to 15" % (kind, low * 1000, high * 1000))
        colour = model["colours"][how["paint"]]
        panels[kind] = {
            "size_m": size, "cover": cover, "cols": how["cols"], "rows": how["rows"], "lift_m": lift, "skin": colour,
            "off_mm": [round(low * 1000, 2), round(high * 1000, 2)],
            "port": side_of(points, how["rows"], how["cols"], False),
            "starboard": side_of(points, how["rows"], how["cols"], True),
        }
        if "skin_paints" in how:
            panels[kind]["skin_paints"] = sorted(allowed)
        print("%s: panel %dx%d over u %.3f to %.3f, v %.3f to %.3f, lifted %.1f mm, %.1f to %.1f mm off her skin, on paint %s" % (kind, how["cols"], how["rows"], cover[0], cover[2], cover[1], cover[3], lift * 1000, low * 1000, high * 1000, sorted(set(int(p) for p in under.ravel()))))
    return panels


def write_panels(path, panels, models):
    with open(path, "w") as f:
        f.write("{\n")
        f.write(' "about": %s,\n' % json.dumps("Where each model's nose art is painted (Tools/pack_nose_art.py): in the model file's own frame (metres, x forward, y right, z up), port and starboard. cover: the part of the picture (u0, v0, u1, v1) the panel is laid over, all of every painting's paint. points: the panel's grid, rows from the painting's top, each [x, y, z, nx, ny, nz, u, v], laid on her near shape and lifted lift_m off it along her skin's facing; u along the painting from its left edge, v down it from its top, so it reads from left to right seen from outside either side. middle, normal, up and right: at its middle point (right the way u grows). skin: the paint it lies on. off_mm: how far its triangles are off her skin, least and most."))
        f.write(' "made": %s,\n' % json.dumps("Tools/pack_nose_art.py from Content/Models/%s.json's near shape" % "/".join(sorted(models))))
        f.write(' "models": {\n')
        for m, kind in enumerate(sorted(panels)):
            p = panels[kind]
            f.write('  %s: {\n' % json.dumps(kind))
            for key in ("size_m", "cover", "cols", "rows", "lift_m", "skin", "skin_paints", "off_mm"):
                if key in p:
                    f.write('   %s: %s,\n' % (json.dumps(key), json.dumps(p[key])))
            for s, side in enumerate(("port", "starboard")):
                q = p[side]
                f.write('   %s: {\n' % json.dumps(side))
                for key in ("middle", "normal", "up", "right"):
                    f.write('    %s: %s,\n' % (json.dumps(key), json.dumps(q[key])))
                f.write('    "points": [\n')
                f.write(",\n".join("     " + json.dumps(pt) for pt in q["points"]))
                f.write('\n    ]\n   }%s\n' % ("," if s == 0 else ""))
            f.write('  }%s\n' % ("," if m + 1 < len(panels) else ""))
        f.write(' }\n}\n')


def main_fine():
    """The finer B-17's panel alone (panels-fine.json), with the cover and size panels.json already has."""
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.join(here, "..")
    to = os.path.join(root, "Content", "Data", "NoseArt")
    have = json.load(open(os.path.join(to, "panels.json")))["models"]["B17"]
    bad = []
    panels = lay_all(root, FINE_MODELS, have["size_m"], have["cover"], bad)
    write_panels(os.path.join(to, "panels-fine.json"), panels, FINE_MODELS)
    for line in bad:
        print("FAIL  " + line)
    return 1 if bad else 0


def main(art_folder, names_path):
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.join(here, "..")
    to = os.path.join(root, "Content", "Data", "NoseArt")
    os.makedirs(to, exist_ok=True)
    names = json.load(open(names_path))
    done = json.load(open(os.path.join(art_folder, "done.json")))
    painted = set(int(n) for n in done["done"])
    size_m = [float(v) for v in done.get("size_m", [2.2, 1.35])]
    big = [int(v) for v in done.get("px", [1408, 864])]
    small = [big[0] // SHRINK, big[1] // SHRINK]
    bad = []
    index = []
    for entry in sorted(names["art"], key=lambda e: int(e["id"])):
        number = int(entry["id"])
        row = {"number": number, "name": entry["name"], "painted": False}
        if number in painted:
            src = os.path.join(art_folder, "art-%03d.png" % number)
            if not os.path.exists(src):
                bad.append("art-%03d.png is listed as painted but missing" % number)
                index.append(row)
                continue
            im = Image.open(src)
            if im.mode != "RGBA" or list(im.size) != big:
                bad.append("art-%03d.png is %s %dx%d, not RGBA %dx%d" % (number, im.mode, im.size[0], im.size[1], big[0], big[1]))
            a = np.asarray(im.convert("RGBA"))
            edge = np.concatenate([a[:BORDER, :, 3].ravel(), a[-BORDER:, :, 3].ravel(), a[:, :BORDER, 3].ravel(), a[:, -BORDER:, 3].ravel()])
            if edge.max() > 0:
                bad.append("art-%03d.png has paint in its outer %d pixels (most %d): it would be cut at the panel's edge" % (number, BORDER, int(edge.max())))
            out = shrink(a, SHRINK)
            name = "art-%03d.png" % number
            Image.fromarray(out, "RGBA").save(os.path.join(to, name), optimize=True)
            row.update({"painted": True, "file": name, "px": small, "size_m": size_m})
        index.append(row)
    n_painted = sum(1 for r in index if r["painted"])
    # The part of the picture any painting has any paint in, and a little more: the panel is laid over that alone.
    held = None
    for row in index:
        if row["painted"]:
            a = np.asarray(Image.open(os.path.join(to, row["file"])))[:, :, 3] > 0
            held = a if held is None else (held | a)
    if held is None:
        raise SystemExit("pack_nose_art: no paintings")
    ys = np.nonzero(held.any(1))[0]
    xs = np.nonzero(held.any(0))[0]
    cover = [max(0, int(xs.min()) - SLACK) / small[0], max(0, int(ys.min()) - SLACK) / small[1],
             min(small[0], int(xs.max()) + 1 + SLACK) / small[0], min(small[1], int(ys.max()) + 1 + SLACK) / small[1]]
    cover = [round(v, 5) for v in cover]
    with open(os.path.join(to, "index.json"), "w") as f:
        json.dump({
            "about": "The bombers' nose art (Battle/AirNoseArt, AirBattleNoseArt.cpp): each number of the air thread's list (names.json), its name, whether it is painted and, if so, its picture: RGBA, its clear background kept clear, covering size_m of her skin (the painting fitted inside). A number not painted is worn as a bare nose. none: the chance a bomber has no name (a new replacement) where the battle does not say who flies her.",
            "made": "Tools/pack_nose_art.py from the modeller's noseart folder (done.json: %d painted, %dx%d) and the air thread's names.json, each a quarter of the modeller's size each way" % (len(painted), big[0], big[1]),
            "count": len(index), "painted": n_painted, "none": float(names.get("none", 0.2)),
            "size_m": size_m, "px": small,
            "art": index,
        }, f, indent=1, ensure_ascii=False)
        f.write("\n")
    panels = lay_all(root, MODELS, size_m, cover, bad)
    write_panels(os.path.join(to, "panels.json"), panels, MODELS)
    total = sum(os.path.getsize(os.path.join(to, n)) for n in os.listdir(to))
    if total >= MOST_BYTES:
        bad.append("the folder is %.1f MB, not under %d MB" % (total / 1048576.0, MOST_BYTES // 1048576))
    print("nose art: %d of %d painted, %dx%d each, folder %.2f MB" % (n_painted, len(index), small[0], small[1], total / 1048576.0))
    for line in bad:
        print("FAIL  " + line)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main_fine() if sys.argv[1:] == ["--fine"] else main(sys.argv[1], sys.argv[2]))
