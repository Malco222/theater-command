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
CAP_OF = {"Bf109": 0.04, "Me262": 0.04,                     # (the art director, 2026-11-22 19:47Z: Germany's quieter, her score is
          "Hurricane": 0.07, "Warhawk": 0.07,               #  on her rudder; and where her skin leans up toward her canopy, taller,
          "Fighter": 0.07, "CarrierFighter": 0.07}          #  to read as high as the others' from her side)
GAP = 0.03                   # (between the line and the first row of kills)
STEP = 0.1                   # (the line's points, this far apart along it)
ROW = {"Spitfire": 9}        # (marks to a row: her number comes forward over a tenth column, 2026-11-22)
LOWER = {"Hurricane": 0.035, "WhatIf_EscortFighter": 0.015}  # (her line this much lower down her skin: her cockpit's edge dips
                             #  below the hood's line there, to its inside (Hurricane) or to her canopy rail (the escort))
NARROW = 0.72                # (a capital's width, of its height, at most: the sign painter's narrow block hand)
SPACE = 0.16                 # (between two letters, of the capitals' height)
WORD = 0.45                  # (a space between words, of the capitals' height)
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"      # (livery.BLOCK: the block hand of a unit's sign painter)
DARK, PALE, YELLOW = "1A1A1A", "F2F0E8", "D9B43A"
SKIN = ("upper", "upper2", "under", "nose", "band")      # (her paints, for the paint under her name: not a rivet, a seam or a
                                                         #  line scribed on them)
REACH = 0.02                 # (that paint is the one with the most of her skin within this of the point; the scene, 20:25Z)
LIGHT_L = 50.0               # (tallies.LIGHT_L: a finish is light when its lightness L* is at least this)
INK = ("one colour. Read the paint under the middle of the line as she flies (her scheme, her owner's colours; a "
       "second colour where it has faces of its own there; a sprayed mottle's soft blend is not read, so a line partly "
       "on one takes her base coat's ink, as the C202's does). On an American machine (her owner the USA) where that "
       "paint is olive drab (a dark brownish green: L* under 45, its red and green within a tenth of each other, both "
       "10 or more above its blue and at least 1.15 times it): yellow D9B43A (the art director, 2026-11-22). Otherwise "
       "the bombers' record rule (tallies.py): black 1A1A1A where its lightness L* is 50 or more (bare metal, light "
       "grey, sand), else off-white F2F0E8. The paint under a point is the one with the most of her skin within 2 cm of "
       "it, not the nearest face (often a rivet)")
LETTERS = ("plain block capitals, even stroke, about 0.7 as wide as high, no outline, shadow or script, upright on her "
           "side; the rank short as his nation wrote it, then his name (ACES.md): F/LT T. HARLAND. Fit it to `long` at "
           "`cap`, smaller down to `cap_min`; longer still, his first names to initials, then rank and surname, then the "
           "surname alone")


def line(name, x0, x1, top, side, down):
    """Her name's line: from x0 (its front, the front of her kill rows) back to x1 (their back), the capitals' tops on
    `top` (where her first row's tops were; lower by LOWER on a few). side(x, z) -> (point, facing); down(z, d) -> the
    height d lower along her skin. Returns (its entry, the height her first row of kills now hangs from)."""
    top = down(top, LOWER.get(name, 0.0))
    cap = CAP_OF.get(name, CAP)
    mid = down(top, 0.5 * cap)
    xs, x = [], x0
    while x > x1 + 0.5 * STEP:
        xs.append(x)
        x -= STEP
    xs.append(x1)
    pts = []
    for x in xs:
        p, n = side(x, mid)
        pts.append([round(v, 3) for v in (p.x, p.y, p.z, n.x, n.y, n.z)])
    entry = {"tag": "NAME", "cap": cap, "cap_min": CAP_MIN, "cap_max": max(CAP_MAX, cap), "long": round(x0 - x1, 3),
             "at": pts[0], "line": pts, "up": [0.0, 0.0, 1.0], "ink": INK, "letters": LETTERS,
             "note": "his name, read from the front: line[k] is a point on the middle of the capitals' height and the way "
                     "her skin faces there, from the front of the line (at) to its back, every 0.1 m (the last at its "
                     "back); the first row of kills hangs below it"}
    return entry, down(top, cap + GAP)


# ---- for the pictures: his name painted on her ------------------------------------------------------------------
def lightness(hexcol):
    def lin(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = (lin(int(hexcol[i:i + 2], 16)) for i in (0, 2, 4))
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    return 116.0 * (y ** (1.0 / 3.0) if y > 216.0 / 24389.0 else (24389.0 / 27.0 * y + 16.0) / 116.0) - 16.0


def olive_drab(hexcol):
    """An American olive drab: a dark brownish green (as INK says)."""
    r, g, b = (int(hexcol[i:i + 2], 16) for i in (0, 2, 4))
    # (10 and 1.15 times, not 15: the game's matte recoat 555A47 is only 14 over its blue; the scene thread, 20:25Z)
    return (lightness(hexcol) < 45.0 and abs(r - g) <= 0.1 * max(r, g) and min(r, g) - b >= 10
            and min(r, g) >= 1.15 * b)


def ink_on(hexcol, nation=None):
    """The ink for a name on this paint, as INK says."""
    if nation == "USA" and hexcol and olive_drab(hexcol):
        return YELLOW
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
         "COL.", "CDR.", "FW.", "OFW.", "UFFZ.", "OBLT.", "HPTM.", "SERG.", "MAGG.", "M.LLO", "S.TEN.", "TEN.", "CAP.", "ML.",
         "ST.", "KAPT.", "S/LT", "LT", "CDR"}               # (a rank of two words is two of these: ST. LT., LT. COL., SERG. MAGG.,
                                                           #  the Fleet Air Arm's LT CDR)


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


def paint_under(model, at, facing, reach=None):
    """The name of her paint under `at` (the model's own axes): the one that covers the most of her skin within `reach`
    of it, as seen straight in along `facing`. Her skin is a few large triangles, her rivets and seams many small
    ones: the nearest face to a point is often a rivet, so this counts area instead (64 sights spread evenly over the
    disc, each cast in from just outside her), and a rivet wins only its own small share. None if no paint of SKIN is
    hit."""
    from mathutils import Vector
    from mathutils.bvhtree import BVHTree
    reach = REACH if reach is None else reach
    tree = BVHTree.FromPolygons([tuple(v) for v in model.verts], [tuple(f) for f in model.faces])
    n = Vector(facing).normalized()
    a = Vector((1, 0, 0)) if abs(n.x) < 0.9 else Vector((0, 0, 1))
    u = (a - n * a.dot(n)).normalized()
    v = n.cross(u)
    names = list(model.paints)
    area = {}
    for k in range(64):                                   # (a sunflower: each sight stands for the same area)
        r = reach * math.sqrt((k + 0.5) / 64.0)
        t = k * 2.399963
        o = Vector(at) + u * (r * math.cos(t)) + v * (r * math.sin(t))
        hit = tree.ray_cast(o + n * reach, -n, 2.0 * reach)
        if hit[2] is not None:
            nm = names[model.fpaint[hit[2]]]
            area[nm] = area.get(nm, 0) + 1
    skin = {nm: c for nm, c in area.items() if nm in SKIN}
    return max(skin, key=lambda nm: (skin[nm], -SKIN.index(nm))) if skin else None


def ink_under(model, entry, nation=None):
    """For the pictures: the ink for the paint under the middle of her line (her faces' paint there, as her paints
    give it as she flies; a sprayed mottle's blend is not read; see paint_under), as INK says."""
    pts = entry["line"]
    mid = pts[len(pts) // 2]
    nm = paint_under(model, (mid[0], -mid[1], mid[2]), (mid[3], -mid[4], mid[5]))   # (the model's y is to port)
    colour = model.paints.get(nm) if nm else None
    return ink_on(colour or model.paints.get("upper"), nation)


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
