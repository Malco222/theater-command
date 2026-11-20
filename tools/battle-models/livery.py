"""What makes each bomber of a formation her own: her group's mark on her fin, her serial and letter, her squadron's
code, and her nose art (Brian, 2026-11-20: "have some bomber nose art on the b-17s"; "here is the images i want you
to base them off ... with the bomber group markings, windows, gun looks, ect").

    overlay("B17")  ->  the model "B17Livery"

Like her damage (`damage.py`), it is a model of its own that lies on her skin: it has every part of hers (same names,
parents and pivots, no faces of their own) and every clip of hers, so lay it on her and play on it what she plays.
Its own faces are in parts that each hold one thing a figure of a formation may wear, every one of them painted on
both her sides where the real paint was:

- `group.T`, `group.S`: the white triangle (the 1st Bombardment Division) or square (the 3rd) high on her fin, and
  `group.T.<L>`, `group.S.<L>` the black letter in it;
- `serial.<k>.<d>`: digit d (0 to 9) in place k (1 to 6) of the yellow serial under it (42-31234 is painted 231234);
- `letter.fin.<L>`: her own letter in yellow under the serial, and `letter.side.<L>` the same letter in light grey
  on her fuselage, ahead of the star;
- `code.<side>.<k>.<L>`: letter L in place k (1 or 2) of her squadron's code behind the star, on her port or
  starboard side (each letter on each side its own part, so that the tail takes its letters with it if it breaks);
- `art.port`, `art.starboard`: her nose art, a picture wrapped on her skin on each side of her nose, below the
  navigator's windows and ahead of the cockpit, with its place on the picture (UVs: u 0 to 1 the way it is read, v 0
  to 1 upward). Its material (`<model>:art`) takes a figure's own decal as its picture: art-NNN.png, number NNN of the
  air thread's list (/mnt/project-files/air-battles/nose-art/names.json; noseart.py paints them), clear where the
  picture is clear. Most figures wear it on the port side only; a few on both.

At rest none of them shows (each is scaled to nothing): give each figure the parts it wears at full size and leave
the rest, or build its paint from those parts alone. `pick()` says which parts a figure wears, from the battle's
seed and the group's id, as the air thread's brief has it (section 3), so that a machine keeps her name, letters
and serial through a battle. The file's `livery` lists every part by kind.

The paints are the art director's (2026-11-20, G3): off-white, faded chrome yellow, light grey codes; nose art in
flat sign-painter's colours with a dark edge. Lettering is our own block hand; the paintings are ours (noseart.py).
"""
import math
import os
import random
import tempfile

import bmesh
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import tessellate_polygon

import noseart
from rigid import Model, Part

X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
BLOCK = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"            # (the block hand of a unit's sign painter)
PAINTS = {"group": "DCD9CC", "group_ink": "17171A", "serial": "C99A35", "code": "B9BAB4",
          "art": "FFFFFF"}                        # (the nose art's: white, so that its picture shows as painted)
PAINTS.update(noseart.PAINTS)
DECALS = os.environ.get("NOSEART_DIR", "/mnt/project-files/battle-models/air/noseart")     # (art-NNN.png, as noseart.write makes them)
SKIN = ("upper", "upper2", "under", "nose", "boss")

# Where each thing is painted on her (her own metres, y to port as in the model; `side` marks are laid on both sides).
# fin: the group's shape (its middle, the length of its side), the serial (its middle, the height of a digit, how far
# apart), her letter (middle, height); side: her letter ahead of the star, the code behind it (middle of the word,
# height of a letter, how far apart); art: the middle of the painting on her port side, its width, its turn.
# Sized to Brian's side picture (2026-11-20): the triangle as wide as the fin allows ahead of the rudder's hinge, the
# serial close under it, her letter big and low on the fin, the code and letter on her side nearly half her height.
PLACES = {
    "B17": {"fin": {"shape": ((-10.22, 3.41), 1.6), "square": ((-10.36, 3.25), 1.12), "serial": ((-10.22, 2.74), 0.32, 0.25),
                    "letter": ((-10.2, 1.92), 0.8)},
            "side": {"letter": ((-2.0, 0.08), 0.8), "code": ((-6.98, 0.06), 0.8, 0.73)},
            "art": ((6.12, -0.42), 2.2)},
}
NARROW = 0.74                    # (the sign painter's block letters are narrower than a printer's: about two thirds as wide as high)


# ---- lettering -----------------------------------------------------------------------------------------------
_GLYPHS = {}


def glyph(ch, font=BLOCK, res=5, offset=0.0):
    """The flat triangles of one letter, its capitals one unit high, standing on its middle: [(tri), ...] in (u, v)."""
    key = (ch, font, res, round(offset, 4))
    if key in _GLYPHS:
        return _GLYPHS[key]
    cu = bpy.data.curves.new("glyph", "FONT")
    cu.body = ch
    cu.font = bpy.data.fonts.load(font, check_existing=True)
    cu.size = 1.0
    cu.align_x, cu.align_y = "CENTER", "CENTER"
    cu.offset = offset
    cu.resolution_u = res
    ob = bpy.data.objects.new("glyph", cu)
    bpy.context.scene.collection.objects.link(ob)
    dg = bpy.context.evaluated_depsgraph_get()
    me = ob.evaluated_get(dg).to_mesh()
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    tris = [tuple((v.co.x, v.co.y) for v in f.verts) for f in bm.faces]
    bm.free()
    ob.evaluated_get(dg).to_mesh_clear()
    bpy.data.objects.remove(ob)
    bpy.data.curves.remove(cu)
    cap = cap_height(font)
    mid = 0.5 * (min(p[1] for t in tris for p in t) + max(p[1] for t in tris for p in t)) if ch.isdigit() or ch.isupper() else 0.0
    tris = [tuple((u / cap, (v - mid) / cap) for u, v in t) for t in tris]
    _GLYPHS[key] = tris
    return tris


_CAPS = {}


def cap_height(font):
    if font not in _CAPS:
        cu = bpy.data.curves.new("cap", "FONT")
        cu.body = "H"
        cu.font = bpy.data.fonts.load(font, check_existing=True)
        cu.size = 1.0
        ob = bpy.data.objects.new("cap", cu)
        bpy.context.scene.collection.objects.link(ob)
        dg = bpy.context.evaluated_depsgraph_get()
        me = ob.evaluated_get(dg).to_mesh()
        zs = [v.co.y for v in me.vertices]
        _CAPS[font] = max(zs) - min(zs)
        ob.evaluated_get(dg).to_mesh_clear()
        bpy.data.objects.remove(ob)
        bpy.data.curves.remove(cu)
    return _CAPS[font]


def scaled(tris, size, at=(0.0, 0.0), turn=0.0, narrow=1.0):
    c, s = math.cos(turn), math.sin(turn)
    return [tuple((at[0] + size * (u * narrow * c - v * s), at[1] + size * (u * narrow * s + v * c)) for u, v in t) for t in tris]


def polygon(points):
    """The triangles of one flat outline (any shape without holes)."""
    flat = [Vector((u, v, 0.0)) for u, v in points]
    return [tuple((flat[i].x, flat[i].y) for i in t) for t in tessellate_polygon([flat])]


def triangle(side):
    """The group's triangle, point up, standing on its middle (its incircle's centre)."""
    h = side * math.sqrt(3.0) / 2.0
    lo = -h / 3.0
    return polygon([(-side / 2.0, lo), (side / 2.0, lo), (0.0, lo + h)])


def square(side):
    return polygon([(-side / 2.0, -side / 2.0), (side / 2.0, -side / 2.0), (side / 2.0, side / 2.0), (-side / 2.0, side / 2.0)])


# ---- laying flat things on her skin ---------------------------------------------------------------------------------
class Skin:
    """Her painted skin, for laying things on it."""

    def __init__(self, m):
        names = list(m.paints)
        faces = [f for k, f in enumerate(m.faces) if names[m.fpaint[k]] in SKIN or (k < len(m.fcoat) and m.fcoat[k])]
        self.tree = BVHTree.FromPolygons([tuple(v) for v in m.verts], [tuple(f) for f in faces])
        self.m = m

    def lay(self, tris, at, n, up, lift=0.012, cell=0.14, flip=False):
        """Flat triangles (u, v in metres; u along the way she is read, v up) laid on her skin round `at`, facing `n`.
        Returns (points, faces), cut small first: a big flat piece dips under a round skin in its middle."""
        n = Vector(n).normalized()
        v_ = (Vector(up) - n * Vector(up).dot(n)).normalized()
        u_ = v_.cross(n)
        if flip:
            u_ = -u_
        pts, faces = [], []
        for tri in tris:
            flat = [Vector((a, b, 0.0)) for a, b in tri]
            longest = max((flat[i] - flat[(i + 1) % 3]).length for i in range(3))
            pieces = [flat]
            for _ in range(min(4, max(0, math.ceil(math.log2(max(longest / cell, 1.0)))))):
                pieces = [q for (a, b, c) in pieces for ab, bc, ca in [((a + b) / 2, (b + c) / 2, (c + a) / 2)]
                          for q in ((a, ab, ca), (ab, b, bc), (ca, bc, c), (ab, bc, ca))]
            for piece in pieces:
                idx = []
                for q in piece:
                    p = Vector(at) + u_ * q.x + v_ * q.y
                    hit = self.tree.ray_cast(p + n * 0.6, -n, 1.4)
                    if hit[0] is not None and hit[1].dot(n) > 0.25:
                        p = hit[0] + hit[1].normalized() * lift
                    else:
                        p = p + n * lift
                    idx.append(len(pts))
                    pts.append(p)
                a, b, c = (pts[i] for i in idx)
                if (b - a).cross(c - a).dot(n) < 0.0:
                    idx.reverse()
                faces.append(tuple(idx))
        return pts, faces

    def wrap(self, size, at, n, up, lift=0.014, cells=(24, 15)):
        """A picture `size` (width, height: metres) wrapped on her skin round `at` (where it faces `n`), read along
        her side with `up` up: a grid of points walked out over her skin from its middle, row and column, so that it
        bends round her without stretching. Returns (points, faces, uvs): u 0..1 the way it is read, v 0..1 upward."""
        W, H = size
        nu, nv = cells
        up = Vector(up)

        def snap(p, n0):
            loc, nor, _, _ = self.tree.find_nearest(p, 1.0)
            if loc is None:
                return p, n0
            nor = nor.normalized()
            return loc, (nor if nor.dot(n0) > 0.0 else -nor)

        def axes(nor):
            v_ = (up - nor * up.dot(nor)).normalized()
            return v_.cross(nor), v_

        def walk(p, nor, way, dist, steps=4):
            for _ in range(steps):
                u_, v_ = axes(nor)
                p, nor = snap(p + (u_ if way == "u" else v_) * dist / steps, nor)
            return p, nor

        p0, n0 = snap(Vector(at), Vector(n).normalized())
        row = {nu // 2: (p0, n0)}
        for sgn in (1, -1):
            p, nor = p0, n0
            for i in range(1, nu // 2 + 1):
                p, nor = walk(p, nor, "u", sgn * W / nu)
                row[nu // 2 + sgn * i] = (p, nor)
        grid = {}
        for i, (pc, nc) in row.items():
            grid[(i, nv // 2)] = (pc, nc)
            for sgn in (1, -1):
                p, nor = pc, nc
                for j in range(1, nv // 2 + 1 + nv % 2):
                    p, nor = walk(p, nor, "v", sgn * H / nv)
                    jj = nv // 2 + sgn * j
                    if 0 <= jj <= nv:
                        grid[(i, jj)] = (p, nor)
        pts, uvs, at_ = [], [], {}
        for j in range(nv + 1):
            for i in range(nu + 1):
                p, nor = grid[(i, j)]
                at_[(i, j)] = len(pts)
                pts.append(p + nor * lift)
                uvs.append((i / nu, j / nv))
        faces = [(at_[(i, j)], at_[(i + 1, j)], at_[(i + 1, j + 1)], at_[(i, j + 1)]) for j in range(nv) for i in range(nu)]
        return pts, faces, uvs

    def find(self, x, z, way):
        """The point of her skin at (x, z) seen from `way` (+Y her port side, -Y her starboard), and its facing."""
        hit = self.tree.ray_cast(Vector((x, way * 6.0, z)), Vector((0.0, -way, 0.0)), 12.0)
        if hit[0] is None:
            return Vector((x, 0.0, z)), Vector((0.0, way, 0.0))
        return hit[0], hit[1].normalized()


# ---- the layer -------------------------------------------------------------------------------------------------------
def overlay(name, fine=True, model=None):
    """The livery layer for the aircraft `name` (`model`: hers, if it is already built)."""
    import planes
    if name not in PLACES:
        return None
    m = model if model is not None else planes.BUILDERS[name](fine)[0]
    at = PLACES[name]
    d = Model(name + "Livery", m.title + ": her group's marks, serial, letters, code and nose art", "damage", dict(PAINTS), fine)
    d.parts = [Part(p.name, p.parent, p.pivot, axis=p.axis, limits=p.limits, slide=p.slide, slide_limits=p.slide_limits, kind=p.kind, note=p.note) for p in m.parts]
    d.part_at = dict(m.part_at)
    d.clips = {k: dict(c, events=[]) for k, c in m.clips.items()}
    skin = Skin(m)
    x_cut = m.parts[m.part_at["tail"]].pivot.x
    kinds = {"group": [], "serial": [], "letter.fin": [], "letter.side": [], "code": [], "art": []}
    cell = 0.14 if fine else 0.5

    def on_of(x):
        return "tail" if x < x_cut else "plane"

    def put(part, on, pieces, note):
        """pieces: [(tris, x, z, way, paint, lift)]: each laid on her side `way` round (x, z)."""
        d.part(part, on, pivot=(pieces[0][1], 0.0, pieces[0][2]), kind="fixed", note=note)
        d.use(part)
        for tris, x, z, way, paint, lift in pieces:
            p, n = skin.find(x, z, way)
            pts, faces = skin.lay(tris, p, n, Z, lift=lift, cell=cell)
            d.add(pts, faces, paint, smooth=False)

    # (her fin: the group's shape, its letter, the serial, her letter; the same on both sides, read the right way on each)
    (tx, tz), side = at["fin"]["shape"]
    (qx, qz), qside = at["fin"]["square"]
    for shape, tris, sx, sz, high in (("T", triangle(side), tx, tz, side * 0.42), ("S", square(qside), qx, qz, qside * 0.6)):
        put(f"group.{shape}", "tail", [(tris, sx, sz, w, "group", 0.012) for w in (1, -1)],
            f"the {'triangle' if shape == 'T' else 'square'} of a group of the {'1st' if shape == 'T' else '3rd'} Bombardment Division")
        kinds["group"].append(f"group.{shape}")
        for ch in LETTERS:
            g = scaled(glyph(ch), high, narrow=0.86)
            put(f"group.{shape}.{ch}", "tail", [(g, sx, sz, w, "group_ink", 0.0145) for w in (1, -1)], f"the group's letter {ch} in its {'triangle' if shape == 'T' else 'square'}")
            kinds["group"].append(f"group.{shape}.{ch}")
    (ex, ez), high, apart = at["fin"]["serial"]
    for k in range(6):
        for dgt in "0123456789":
            pieces = []
            for w in (1, -1):
                # (read from her nose back on her port side, from her tail forward on her starboard)
                off = (k - 2.5) * apart
                pieces.append((scaled(glyph(dgt), high, narrow=NARROW), ex - off * w, ez, w, "serial", 0.012))
            put(f"serial.{k + 1}.{dgt}", "tail", pieces, f"digit {dgt} in place {k + 1} of her serial")
            kinds["serial"].append(f"serial.{k + 1}.{dgt}")
    (lx, lz), lh = at["fin"]["letter"]
    for ch in LETTERS:
        put(f"letter.fin.{ch}", "tail", [(scaled(glyph(ch), lh, narrow=NARROW), lx, lz, w, "serial", 0.012) for w in (1, -1)], f"her letter {ch} on her fin")
        kinds["letter.fin"].append(f"letter.fin.{ch}")
    (fx, fz), fh = at["side"]["letter"]
    for ch in LETTERS:
        put(f"letter.side.{ch}", on_of(fx), [(scaled(glyph(ch), fh, narrow=NARROW), fx, fz, w, "code", 0.012) for w in (1, -1)], f"her letter {ch} on her side, ahead of the star")
        kinds["letter.side"].append(f"letter.side.{ch}")
    (cx, cz), ch_, apart = at["side"]["code"]
    for w, side_name in ((1, "port"), (-1, "starboard")):
        for k in range(2):
            off = (k - 0.5) * apart
            x = cx - off * w
            for ch in LETTERS:
                put(f"code.{side_name}.{k + 1}.{ch}", on_of(x), [(scaled(glyph(ch), ch_, narrow=NARROW), x, cz, w, "code", 0.012)],
                    f"letter {ch} in place {k + 1} of her squadron's code, on her {side_name} side")
                kinds["code"].append(f"code.{side_name}.{k + 1}.{ch}")

    # (her nose art: a picture wrapped on her skin on each side of her nose; a figure's own decal is its picture)
    (ax, az), wide = at["art"]
    if os.environ.get("NOSEART_PLACE"):                  # (x,z,width: to try her nose art elsewhere)
        ax, az, wide = (float(v) for v in os.environ["NOSEART_PLACE"].split(","))
    size = (wide, wide * noseart.SIZE_M[1] / noseart.SIZE_M[0])
    for w, side_name in ((1, "port"), (-1, "starboard")):
        part = f"art.{side_name}"
        d.part(part, "plane", pivot=(ax, 0.0, az), kind="fixed",
               note=f"her nose art on her {side_name} side: a picture {size[0]:.2f} by {size[1]:.2f} m wrapped on her skin, UVs 0 to 1 "
                    f"(u the way it is read, v up); its material takes a figure's own decal (art-NNN.png) as its picture")
        d.use(part)
        p, n = skin.find(ax, az, w)
        pts, faces, uvs = skin.wrap(size, p, n, Z, lift=0.014, cells=(24, 15) if fine else (8, 5))
        d.add(pts, faces, "art", smooth=True, uv=uvs)
        kinds["art"].append(part)

    every = [p for v in kinds.values() for p in v]
    d.clips["none"] = {"long": 1.0, "loop": False, "after": "hold", "tracks": [{"part": p, "scale": [[0.0, 0.001]]} for p in every], "events": [],
                       "about": "Every part of the livery hidden: play it, then show the parts a figure wears (scale 1)."}
    d.info = {"file": {"for": name, "livery": kinds, "letters": LETTERS,
                       "art": {"parts": kinds["art"], "material": name + "Livery:art", "size_m": list(size), "decals": "noseart/art-NNN.png",
                               "about": "A figure with nose art shows art.port (a few also art.starboard), its material's picture "
                                        "the decal of its number; the picture's clear parts are clear (alpha)."},
                       "about": "Show a figure's own parts at full size, the rest at nothing (see `pick` in livery.py for how a figure's are chosen)."}}
    d.kinds = kinds
    return d


# ---- who wears what -----------------------------------------------------------------------------------------------
# The air thread's brief (/mnt/project-files/air-battles/nose-art/brief.md, sections 2 and 3), as the historian
# checked it: groups and their squadrons' codes, and the blocks the serials are drawn from.
GROUPS = {"303": {"shape": "T", "letter": "C", "codes": ["VK", "BN", "PU", "GN"]}}
SERIALS = {"B-17F": [(41, 24340, 24639), (42, 5050, 5484), (42, 29467, 31031), (42, 2964, 3562), (42, 5705, 6204)],
           "B-17G": [(42, 31032, 32116), (42, 37714, 38213), (42, 39758, 40057)]}
KEEP_OFF = {"41-24577", "41-24605", "41-24485", "42-31909"}           # (the famous ones: never painted)
NO_LETTER = "IO"


def painted(serial):
    """42-31234 is painted 231234 on her fin: the year's last figure and the number (her fin has six places: `pick`
    draws only serials that fill them; 42-5249 would be painted 25249)."""
    year, number = serial.split("-")
    return year[-1] + number


def pick(seed, group="303", count=18, kind=None, none=0.2, done=None, taken=None, month="1943-12", g_share=0.4, both=0.12):
    """What each of `count` figures of a group wears: [{"parts", "serial", "letter", "code", "art", "type", "star",
    "hide", "group"}, ...], as the air thread's brief has it (section 3), so that a machine keeps it all through a
    battle and through a film's cuts:
    - one shuffle of the painted nose art (`done`: noseart.DONE) for the group, from the battle's seed and the group's
      id, given out in slot order; a battle's other groups' (`taken`, a set this adds to) go to the back, so that two
      groups do not share a name while unused ones remain; about one in five (`none`) a new machine with a bare nose;
      a few (`both`) wear it on both sides;
    - letters in slot order within a squadron of six; serials from the same dice, from her type's blocks;
    - her type: `kind` ("B-17F" or "B-17G") for all, or else a late-1943 formation's mix, about six Fs to four Gs
      (`g_share`); an F hides the G's chin turret (`hide`: her own parts, for the scene to hide);
    - her star (the tag of her national marks) by `month`: red-edged bars in July and August 1943, blue-edged from
      September, about one in eight still red-edged until the spring of 1944."""
    g = GROUPS.get(group, {"shape": "T", "letter": LETTERS[sum(map(ord, group)) % 26], "codes": ["VK", "BN", "PU"]})
    rng = random.Random(f"{seed}:{group}")
    order = list(done if done is not None else noseart.DONE)
    rng.shuffle(order)
    if taken:
        order = [n for n in order if n not in taken] + [n for n in order if n in taken]
    letters = [c for c in LETTERS if c not in NO_LETTER]
    out, used, given = [], set(), 0
    for k in range(count):
        squadron = k // 6
        code = g["codes"][squadron % len(g["codes"])]
        letter = letters[(k % 6) + 6 * (squadron // len(g["codes"]))]
        typ = kind or ("B-17G" if rng.random() < g_share else "B-17F")
        while True:
            year, lo, hi = rng.choice(SERIALS[typ])
            serial = f"{year}-{rng.randint(lo, hi)}"
            if serial not in KEEP_OFF and serial not in used and len(painted(serial)) == 6:
                used.add(serial)
                break
        art = None
        if order and rng.random() >= none:
            art = order[given % len(order)]
            given += 1
            if taken is not None:
                taken.add(art)
        two = rng.random() < both
        if month < "1943-07":
            star = "USA"
        elif month < "1943-09":
            star = "USA-BAR-RED"
        elif month < "1944-03":
            star = "USA-BAR-RED" if rng.random() < 0.125 else "USA-BAR"
        else:
            star = "USA-BAR"
        digits = painted(serial)
        parts = [f"group.{g['shape']}", f"group.{g['shape']}.{g['letter']}", f"letter.fin.{letter}", f"letter.side.{letter}"]
        parts += [f"serial.{i + 1}.{c}" for i, c in enumerate(digits)]
        parts += [f"code.{side}.{i + 1}.{c}" for side in ("port", "starboard") for i, c in enumerate(code)]
        if art:
            parts += ["art.port"] + (["art.starboard"] if two else [])
        hide = ["turret.1", "turret.1.guns"] if typ == "B-17F" else []
        out.append({"parts": parts, "serial": serial, "letter": letter, "code": code, "art": art, "type": typ, "star": star,
                    "hide": hide, "group": group})
    return out


def decal(n):
    """The path of nose art n's decal (art-NNN.png): in DECALS, or drawn now into a cache if it is not there."""
    p = os.path.join(DECALS, f"art-{int(n):03d}.png")
    if os.path.exists(p):
        return p
    cache = os.path.join(tempfile.gettempdir(), "noseart-cache")
    os.makedirs(cache, exist_ok=True)
    p = os.path.join(cache, f"art-{int(n):03d}.png")
    if not os.path.exists(p):
        im = noseart.decal(int(n))
        if im is None:
            return None
        im.save(p)
    return p


def blank():
    """A clear picture, the nose art's until a figure's own decal is set."""
    im = bpy.data.images.get("noseart-none")
    if im is None:
        im = bpy.data.images.new("noseart-none", 4, 4, alpha=True)
        im.pixels = [0.0] * 64
    return im


def look(paint, mat, nt, bsdf, mix, **kw):
    """For the pictures: matt paint, a little worn. The nose art's: see art_look."""
    bsdf.inputs["Roughness"].default_value = 0.85
    bsdf.inputs["Specular IOR Level"].default_value = 0.2
    art_look(paint, mat, nt, bsdf, mix)


def art_look(paint, mat, nt, bsdf, mix, **kw):
    """The nose art's material (in the pictures and the files): its picture (a clear one until a figure's own decal is
    set) times the wear and soot its points carry, clear where the picture is clear. Every other paint as it is."""
    if paint == "art":
        tex = nt.nodes.new("ShaderNodeTexImage")
        tex.name = tex.label = "decal"
        tex.interpolation = "Linear"
        tex.image = blank()
        uvn = nt.nodes.new("ShaderNodeUVMap")
        uvn.uv_map = "UVMap"
        nt.links.new(uvn.outputs["UV"], tex.inputs["Vector"])
        nt.links.new(tex.outputs["Color"], mix.inputs["A"])
        nt.links.new(tex.outputs["Alpha"], bsdf.inputs["Alpha"])
        for attr, val in (("surface_render_method", "BLENDED"), ("blend_method", "BLEND")):
            try:
                setattr(mat, attr, val)
            except (AttributeError, TypeError):
                pass


def set_art(ob, n):
    """Give the livery object `ob` (one figure's own) nose art n: its own copy of the art material, the decal its picture."""
    path = decal(n) if n else None
    for slot in ob.material_slots:
        m = slot.material
        if m is None or not m.name.split(".")[0].endswith(":art"):
            continue
        mine = m.copy()
        mine.name = f"{m.name.split('.')[0]}.{int(n or 0):03d}"
        tex = mine.node_tree.nodes.get("decal")
        if tex is not None:
            tex.image = bpy.data.images.load(path, check_existing=True) if path else blank()
        slot.link = "OBJECT"
        slot.material = mine
