"""An ace's name under his cockpit, his kills in rows beneath it (Brian's named aces, release 2: the air rules thread's
ask of 2026-11-22 18:03Z, "kill-mark slots and a small name panel under the cockpit" on every fighter; the spec is
air-battles/ideas-r2/3d-handoff.md, section 2).

The period's habit (looked up 2026-11-22): a pilot's name on his machine was mostly an American habit, on the port side
under the canopy rail or the sill, in plain block capitals a few centimetres high and in one colour, his kills in rows
beneath it. Other nations seldom painted one, so for every nation's ace it is the game's convention, kept small and
plain, like a stencil: no outline, no shadow, no script.

Her file (`tally.name`): one line where her first row of kills was; her rows of kills hang beneath it, lower by the
line and a gap (CAP + GAP of her skin). `line` is points along the middle of the capitals' height, every STEP from the
front of the line to its back, each with the way her skin faces there (x, y, z, nx, ny, nz in the game's axes), so
that the game can lay each letter on her curve. It reads from the front, as her kills do: the first letter at the
front. Only a fighter has one."""
import math

CAP = 0.05                   # (the capitals' height: the top of the period's range, and still read on the screen)
CAP_MIN = 0.035              # (a long name is set smaller to fit its line, never below this; longer still, it is shortened)
CAP_MAX = 0.07               # (above this it reads as a code letter or a slogan)
GAP = 0.03                   # (between the line and the first row of kills)
STEP = 0.1                   # (the line's points, this far apart along it)
ROW = {"Spitfire": 9}        # (marks to a row: her number comes forward over a tenth column, 2026-11-22)
LOWER = {"Hurricane": 0.025, "WhatIf_EscortFighter": 0.015}  # (her line this much lower down her skin: her cockpit's edge dips
                             #  below the hood's line there, to its inside (Hurricane) or to her canopy rail (the escort))
NARROW = 0.72                # (a capital's width, of its height, at most: the sign painter's narrow block hand)
SPACE = 0.16                 # (between two letters, of the capitals' height)
WORD = 0.45                  # (a space between words, of the capitals' height)
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"      # (livery.BLOCK: the block hand of a unit's sign painter)
DARK, PALE, YELLOW = "1A1A1A", "F2F0E8", "D9B43A"
LIGHT_L = 50.0               # (tallies.LIGHT_L: a finish is light when its lightness L* is at least this)
INK = ("one colour, the bombers' record rule (tallies.py): read the paint under the middle of the line as she flies "
       "(her scheme, her owner's colours, a mottle); black 1A1A1A where its lightness L* is 50 or more (bare metal, light "
       "grey, a pale mottle), else off-white F2F0E8. Yellow D9B43A on an American olive drab machine is the period's own "
       "choice: the art director's call")
LETTERS = ("plain block capitals, even stroke, about 0.7 as wide as high, no outline, shadow or script, upright on her "
           "side; the rank short as his nation wrote it, then his name (ACES.md): F/LT T. HARLAND. Fit it to `long` at "
           "`cap`, smaller down to `cap_min`; longer still, his first names to initials, then rank and surname, then the "
           "surname alone")


def line(name, x0, x1, top, side, down):
    """Her name's line: from x0 (its front, the front of her kill rows) back to x1 (their back), the capitals' tops on
    `top` (where her first row's tops were; lower by LOWER on a few). side(x, z) -> (point, facing); down(z, d) -> the
    height d lower along her skin. Returns (its entry, the height her first row of kills now hangs from)."""
    top = down(top, LOWER.get(name, 0.0))
    mid = down(top, 0.5 * CAP)
    xs, x = [], x0
    while x > x1 + 0.5 * STEP:
        xs.append(x)
        x -= STEP
    xs.append(x1)
    pts = []
    for x in xs:
        p, n = side(x, mid)
        pts.append([round(v, 3) for v in (p.x, p.y, p.z, n.x, n.y, n.z)])
    entry = {"tag": "NAME", "cap": CAP, "cap_min": CAP_MIN, "cap_max": CAP_MAX, "long": round(x0 - x1, 3),
             "at": pts[0], "line": pts, "up": [0.0, 0.0, 1.0], "ink": INK, "letters": LETTERS,
             "note": "his name, read from the front: line[k] is a point on the middle of the capitals' height and the way "
                     "her skin faces there, from the front of the line (at) to its back, every 0.1 m (the last at its "
                     "back); the first row of kills hangs below it"}
    return entry, down(top, CAP + GAP)


# ---- for the pictures: his name painted on her ------------------------------------------------------------------
def lightness(hexcol):
    def lin(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = (lin(int(hexcol[i:i + 2], 16)) for i in (0, 2, 4))
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    return 116.0 * (y ** (1.0 / 3.0) if y > 216.0 / 24389.0 else (24389.0 / 27.0 * y + 16.0) / 116.0) - 16.0


def ink_on(hexcol):
    return DARK if hexcol and lightness(hexcol) >= LIGHT_L else PALE


_GLYPHS = {}
_CAP = []


def glyph(ch):
    """One capital's flat triangles, one unit high, standing on its middle: [(tri), ...] in (u, v)."""
    if ch in _GLYPHS:
        return _GLYPHS[ch]
    import bmesh
    import bpy
    font = bpy.data.fonts.load(FONT, check_existing=True)

    def mesh_of(body):
        cu = bpy.data.curves.new("acepanel", "FONT")
        cu.body, cu.font, cu.size = body, font, 1.0
        cu.align_x, cu.align_y = "CENTER", "CENTER"
        cu.resolution_u = 4
        ob = bpy.data.objects.new("acepanel", cu)
        bpy.context.scene.collection.objects.link(ob)
        dg = bpy.context.evaluated_depsgraph_get()
        me = ob.evaluated_get(dg).to_mesh()
        b = bmesh.new()
        b.from_mesh(me)
        bmesh.ops.triangulate(b, faces=b.faces)
        tris = [tuple((v.co.x, v.co.y) for v in f.verts) for f in b.faces]
        b.free()
        ob.evaluated_get(dg).to_mesh_clear()
        bpy.data.objects.remove(ob)
        bpy.data.curves.remove(cu)
        return tris

    if not _CAP:                                          # (a capital H's height and middle in the font's own units)
        vs = [p[1] for t in mesh_of("H") for p in t]
        _CAP.extend((max(vs) - min(vs), 0.5 * (max(vs) + min(vs))))
    cap, mid = _CAP
    tris = mesh_of(ch)                                    # (every character on the capitals' line: a stop sits on it)
    tris = [tuple((u / cap, (v - mid) / cap) for u, v in t) for t in tris]
    _GLYPHS[ch] = tris
    return tris


def wide(ch):
    """A character's width in capital heights, as painted (narrowed), and its triangles."""
    if ch == " ":
        return WORD, []
    tris = glyph(ch)
    if not tris:
        return WORD, []
    us = [p[0] for t in tris for p in t]
    w = max(us) - min(us)
    k = min(1.0, NARROW / max(w, 1e-6)) if ch not in "IJ1/." else 1.0
    mid = 0.5 * (max(us) + min(us))
    return w * k, [tuple(((u - mid) * k, v) for u, v in t) for t in tris]


RANKS = {"SGT", "F/SGT", "P/O", "F/O", "F/LT", "S/LDR", "W/CDR", "ENS.", "LT.", "LT.(JG)", "LCDR.", "CAPT.", "MAJ.",
         "FW.", "OFW.", "UFFZ.", "OBLT.", "HPTM.", "SERG.", "M.LLO", "S.TEN.", "TEN.", "CAP.", "ML.", "ST.", "KAPT."}


def fitted(entry, text):
    """The text as it will be painted on this line, and its capitals' height: shortened as LETTERS says if it will
    not go in at `cap_min` (his rank is the words in front that are ranks, ST. LT. two of them)."""
    words = text.upper().split()
    k = 0
    while k < len(words) - 1 and words[k] in RANKS:
        k += 1
    rank, names = words[:k], words[k:]

    def run(ws):
        cells = [wide(c)[0] for c in " ".join(ws)]
        return sum(cells) + SPACE * max(0, len(cells) - 1)

    tries = [words]
    if len(names) > 1:                                    # (his first names to initials, then his rank and surname)
        tries.append(rank + [w if w.endswith(".") else w[0] + "." for w in names[:-1]] + names[-1:])
        tries.append(rank + names[-1:])
    tries.append(names[-1:])                              # (and his surname alone)
    for ws in tries:
        unit = run(ws)
        cap = min(entry["cap"], entry["long"] / max(unit, 1e-6))
        if cap >= entry["cap_min"]:
            return " ".join(ws), cap
    ws = tries[-1]
    return " ".join(ws), entry["long"] / max(run(ws), 1e-6)


def ink_under(model, entry):
    """For the pictures: the ink for the paint under the middle of her line (her skin's paint there, as her paints
    give it as she flies: a mottle's own colour where the line lies on it), as INK says."""
    from mathutils import Vector
    from mathutils.bvhtree import BVHTree
    pts = entry["line"]
    mid = pts[len(pts) // 2]
    tree = BVHTree.FromPolygons([tuple(v) for v in model.verts], [tuple(f) for f in model.faces])
    hit = tree.find_nearest(Vector((mid[0], -mid[1], mid[2])), 0.1)          # (the model's y is to port)
    names = list(model.paints)
    colour = None
    if hit[2] is not None:
        colour = model.paints.get(names[model.fpaint[hit[2]]])
    if not colour or names[model.fpaint[hit[2]]] not in ("upper", "upper2", "under", "nose"):
        colour = model.paints.get("upper")
    return ink_on(colour)


def marks(entry, text, ink):
    """His name's letters as marks (entries like the model's own marks), each laid on her line where it falls."""
    text, cap = fitted(entry, text)
    pts = entry["line"]
    out, s = [], 0.0
    for c in text:
        w, tris = wide(c)
        centre = s + 0.5 * w * cap
        s += (w + SPACE) * cap
        if not tris:
            continue
        x = pts[0][0] - centre
        k = 0
        while k + 2 < len(pts) and pts[k + 1][0] > x:
            k += 1
        a, b = pts[k], pts[k + 1]
        t = 0.0 if abs(a[0] - b[0]) < 1e-9 else max(0.0, min(1.0, (a[0] - x) / (a[0] - b[0])))
        at = [a[i] + (b[i] - a[i]) * t for i in range(3)]
        n = [a[i] + (b[i] - a[i]) * t for i in range(3, 6)]
        ln = math.sqrt(sum(v * v for v in n)) or 1.0
        out.append({"tag": "NAME", "at": at, "facing": [v / ln for v in n], "radius": cap, "small": True,
                    "up": entry.get("up", [0.0, 0.0, 1.0]), "drawn": [[(ink, list(tri)) for tri in tris]]})
    return out
