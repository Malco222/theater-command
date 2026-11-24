"""Soldiers to the vehicles' detail, the pilot (Brian, 23 November 2026 14:22Z: "can we make the soldiers at the same
level of artistic detail as the vehicles, tanks, and planes"; release 2; the plan: claude.ai/artifact/PcZi45SfZiBVqvPL3aNcHf).

The German rifleman made again by soldier.py with the art director's gap list worked through, as wrappers round
soldier.py's own builders (soldier.py itself is not changed, so every other army is made as before):

- cloth: the uniform thinned to about 5,600 triangles, not 2,450, so its hems, flaps and folds keep their shape; folds
  where cloth gathers (the crook of each elbow, the sleeve above the cuff, from the armpit to the Y-strap, by each
  strap's foot at the belt, the crotch, behind and above each knee) and the jackboot's ankle wrinkles; the hem eased
  to a clean line (`smooth_hem`);
- pockets: four patch pockets as pieces of their own laid on the cloth's curve (`Pocket`): a filled bag with a crisp
  rim, a box pleat, a pointed flap whose lip stands 5.8 mm off the bag with a 3 mm dark gap under it, its button;
  made in the cloth they were lost at its 3.4 mm meshing grid and read as muscles;
- Y-straps laid without teeth (`lay_strap`: a flat ribbon on an eased path, lifted clear across its width, smoothly,
  over the cloth and the pockets), over each breast pocket's outer third with its button clear, from their hooks
  behind the pouches to the back ring; their buckles and rings on the strap; the belt eased round the waist;
- pouches: stiff leather cells (`leather_cell`): flat front, rounded top, a flap with its edge, a strap and stud;
- shoulder straps as pieces of their own in the collar patches' darker paint, over the Y-straps (`shoulder_strap`);
- every small hard piece (rings, buckles, buttons) moves as one (`rigid_islands`);
- small hardware: domed buttons (front, pockets, shoulder straps), the collar patches with their two bars, the belt's
  buckle plate with its round boss, the Y-straps' chest buckles, shoulder rings and back ring, the pouches' closing
  straps and studs, the bread bag's straps;
- kit: every bag, flask, case and canister made at about three times the triangles on a finer grid; the bayonet
  in its frog over the entrenching tool; the bread bag's and water bottle's straps cut clean along their edges;
- helmet: 72 sides and twelve rings, a rolled rim, the two vent lugs and three liner rivets, the chin strap's buckle;
- rifle: the stock at 1,400 triangles with finger grooves and the butt's disc and sling slot; a stepped barrel, the
  cleaning rod, the bayonet lug, a hooded front sight, a leaf rear sight, the bolt's cocking piece and safety;
- hands at 900 triangles a hand; about 26,300 triangles in all near, the middle level of detail at 2,700 triangles
  (under the 2,048-point cap);
- the skirt: soldier.py's shell fields are exact only 3.2 cm from the shell, and the skirt flares 2.6 to 4.6 cm out,
  so its lower half was made on the coarse grid's jump, a 5 mm staircase that read as crumpling (on every army's
  tunic); here the fields are exact to 6.2 cm and the flare past 2.6 cm is halved (the silhouette the art director
  passed), and the pockets, strip and flaps are walked onto the cloth by its first change of sign (`march`).

The same skeleton and bones, so every clip plays as it is. Pictures for the art director: the game's man today beside
this one at 1 m, 4 m (front and back) and 10 m beside the Kubelwagen and the Panzer IV.

    cd Tools/battle_units
    PYTHONPATH=/home/user/work/looks/battle_units:. /home/user/bpyenv/bin/python soldier_detail.py [--fast] [--only build,pictures]
"""
import json
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import soldier as S  # noqa: E402  (bpy with it)
import crew as C  # noqa: E402
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402
import kit  # noqa: E402
from sculpt import (Ellipsoid, Layer, RoundBox, RoundCone, Sphere, Torus, Tube, frame_from, on_surface,  # noqa: E402,F401
                    rot_euler)

WORK = os.environ.get("DETAIL_WORK", "/home/user/art/detail/")
SHARED = "/mnt/project-files/battle-models/land/soldier/"
_n = S._n

# ================================================================================================ budgets
S.BUDGET["uniform"] = int(os.environ.get("DETAIL_UNIFORM", 5600))   # (7800 before the pockets were pieces of their own)
S.BUDGET["hand"] = 900
S.LOD_TARGET[1] = 2700
SCALE = {"tris": 1.0, "h": 1.0}
_mesh_from_layers = S.mesh_from_layers


def mesh_from_layers(name, layers, h, target_tris, **kw):
    """soldier.py's, with the kit's budgets scaled while the kit is made (and the uniform's hem made clean)."""
    t = int(target_tris * SCALE["tris"]) if target_tris else target_tris
    if name in ("bottle", "shovel"):          # (straps, cup and handle a clean line on the felt and leather: thinned
        kw.setdefault("cut_paints", True)     # uncut, their edges were teeth that read side-on at 1 m)
    if name == "shovel":           # (the handle from the carrier's foot, not 2 cm up inside it: thicker than the carrier,
        cone = layers[1].ops[0][1]            # its top came out through both faces, a cut stick laid on the leather)
        cone.a = cone.a + (cone.b - cone.a) / np.linalg.norm(cone.b - cone.a) * 0.026
        cone.c = (cone.a + cone.b) * 0.5
        cone.lo = np.minimum(cone.a, cone.b) - max(cone.ra, cone.rb)
        cone.hi = np.maximum(cone.a, cone.b) + max(cone.ra, cone.rb)
    ob = _mesh_from_layers(name, layers, h * SCALE["h"], t, **kw)
    if name == "uniform":
        smooth_hem(ob)
        ease_pinches(ob)
        ease_soles(ob)
    return ob


def smooth_hem(ob, it=6):
    """The tunic's hem as a clean line: where the skirt's foot meets the trousers the meshing grid and the cut along
    the paints left it ragged (its points up and down 5 mm, little teeth that read at 1 m). The line where the two
    paints meet is eased along itself, and the cloth three points either side of it (skirt and trousers) eased with
    it; nothing else moves."""
    import bmesh
    me = ob.data
    mats = [mt.name.split(".")[0] for mt in me.materials]
    if "tunic" not in mats or "trousers" not in mats:
        return
    ti, ri = mats.index("tunic"), mats.index("trousers")
    bm = bmesh.new()
    bm.from_mesh(me)
    zt = [v.co.z for f in bm.faces if f.material_index == ti for v in f.verts]
    belt = float(np.percentile(zt, 1)) + 0.08 if zt else 0.0       # (well below the belt: the skirt's foot only)
    seam = [v for v in bm.verts if v.co.z < belt and {f.material_index for f in v.link_faces} >= {ti, ri}]
    if not seam:
        bm.free()
        return
    hem = min(v.co.z for v in seam)
    sid = {v.index for v in seam}
    near, edge = set(), set(sid)
    for _ in range(3):
        edge = {e.other_vert(v).index for v in bm.verts if v.index in edge for e in v.link_edges} - sid - near
        near |= edge
    bm.verts.ensure_lookup_table()
    foot = [bm.verts[i] for i in sorted(near) if {f.material_index for f in bm.verts[i].link_faces} <= {ti, ri}
            and not bm.verts[i].is_boundary]
    along = {v.index: [e.other_vert(v) for e in v.link_edges if e.other_vert(v).index in sid] for v in seam}
    moved = 0.0
    for _ in range(it):
        new = {}
        for v in seam:
            nb = along[v.index]
            if len(nb) >= 2:
                q = sum((u.co for u in nb), v.co * 0.0) / len(nb)
                new[v] = v.co * 0.4 + q * 0.6
        for v in foot:
            nb = [e.other_vert(v) for e in v.link_edges]
            q = sum((u.co for u in nb), v.co * 0.0) / len(nb)
            new[v] = v.co * 0.5 + q * 0.5
        for v, c in new.items():
            moved = max(moved, (v.co - c).length)
            v.co = c
    bm.to_mesh(me)
    bm.free()
    me.update()
    zs = np.array([ob.data.vertices[i].co.z for i in sid])
    print(f"    {ob.name}: hem eased ({len(seam)} points on its line, {len(foot)} beside it; now {zs.min():.4f}..{zs.max():.4f})")


def ease_pinches(ob, it=3, most=0.55):
    """Single points pinched out of the cloth about the skirt's foot (a spike between the skirt pockets, dark slits on
    the side where the hem meets the trousers): each point whose offset from its neighbours' middle, along the
    surface's normal, is more than `most` of its edges' length is eased back toward the surface (along the normal
    only, so the hem's line keeps its place)."""
    import bmesh
    me = ob.data
    mats = [mt.name.split(".")[0] for mt in me.materials]
    if "tunic" not in mats or "trousers" not in mats:
        return
    ti, ri = mats.index("tunic"), mats.index("trousers")
    bm = bmesh.new()
    bm.from_mesh(me)
    zt = [v.co.z for f in bm.faces if f.material_index == ti for v in f.verts]
    hem = float(np.percentile(zt, 1))
    band = [v for v in bm.verts if hem - 0.05 < v.co.z < hem + 0.16 and not v.is_boundary and len(v.link_edges) >= 3
            and {f.material_index for f in v.link_faces} <= {ti, ri}]
    eased = set()
    for _ in range(it):
        bm.normal_update()
        moves = []
        for v in band:
            q = sum((e.other_vert(v).co for e in v.link_edges), v.co * 0.0) / len(v.link_edges)
            el = sum(e.calc_length() for e in v.link_edges) / len(v.link_edges)
            n = v.normal
            off = (v.co - q).dot(n)
            if abs(off) > most * el:
                moves.append((v, n * (off * 0.8)))
        for v, dv in moves:
            v.co = v.co - dv
            eased.add(v.index)
        if not moves:
            break
    bm.to_mesh(me)
    bm.free()
    me.update()
    print(f"    {ob.name}: {len(eased)} pinched points eased about the hem (of {len(band)})")


def ease_soles(ob, it=4, top=0.045):
    """The boots' feet below the ankle, smoothed without shrinking (Taubin's two steps): the uniform's budget left the
    soles' rims wavy, puddled at 4 m. The points on the ground keep their height."""
    import bmesh
    me = ob.data
    mats = [mt.name.split(".")[0] for mt in me.materials]
    if "boots" not in mats:
        return
    bi = mats.index("boots")
    bm = bmesh.new()
    bm.from_mesh(me)
    foot = [v for v in bm.verts if v.co.z < top and not v.is_boundary and v.link_faces
            and all(f.material_index == bi for f in v.link_faces)]
    floor = {v.index: v.co.z for v in foot if v.co.z < 0.004}
    for _ in range(it):
        for lam in (0.5, -0.53):
            new = []
            for v in foot:
                q = sum((e.other_vert(v).co for e in v.link_edges), v.co * 0.0) / len(v.link_edges)
                new.append(v.co + (q - v.co) * lam)
            for v, c in zip(foot, new):
                v.co = c
    for v in foot:
        if v.index in floor:
            v.co.z = floor[v.index]
    bm.to_mesh(me)
    bm.free()
    me.update()
    print(f"    {ob.name}: the boots' feet eased ({len(foot)} points below {top * 100:.1f} cm)")


S.mesh_from_layers = mesh_from_layers
_layer_bounds = Layer.bounds


def _padded_bounds(self):
    """A layer's box grown by half its widest smooth join (and 3 mm): a smooth union swells the shape past both its
    parts' boxes, and where it swelled past the meshing grid's edge the surface was cut open there (the rifle's stock
    had holes through it beside the receiver and in the butt, seen as black ovals)."""
    lo, hi = _layer_bounds(self)
    ks = [k for how, p, k in self.ops if how == "add"]
    pad = 0.5 * max(ks, default=0.0) + 0.003
    return lo - pad, hi + pad


Layer.bounds = _padded_bounds


# ================================================================================================ small helpers
def ridge(L, pts, r, out, blend=0.005, taper=0.8, k=None):
    """A soft fold along pts (each pulled onto the layer's surface first), standing `out` proud of it: a chain of
    round cones, thinner toward the ends."""
    P, N = S.project(L, np.asarray(pts, float))
    n = len(P)
    for i in range(n - 1):
        u0, u1 = i / (n - 1), (i + 1) / (n - 1)
        f0 = taper + (1 - taper) * math.sin(math.pi * u0)
        f1 = taper + (1 - taper) * math.sin(math.pi * u1)
        r0, r1 = r * f0, r * f1
        L.add(RoundCone(P[i] - N[i] * (r0 - out * f0), P[i + 1] - N[i + 1] * (r1 - out * f1), r0, r1), blend if k is None else k)


def arc(centre, R, rx, ry, a0, a1, n, z=0.0):
    """Points round an ellipse in the frame R (columns: across x, across y, along), angles in degrees from R's x."""
    out = []
    for a in np.radians(np.linspace(a0, a1, n)):
        out.append(centre + R[:, 0] * math.cos(a) * rx + R[:, 1] * math.sin(a) * ry + R[:, 2] * z)
    return np.array(out)


def dome(g, p, nrm, r, h, paint="steel", sides=10, up=(0.0, 0.0, 1.0)):
    """A domed button or rivet head standing on a surface at p (outward normal nrm)."""
    R = frame_from(_n(nrm), up)
    x, y, z = R[:, 0], R[:, 1], R[:, 2]
    rings = [[p + x * h]]
    for rr, hh in ((0.55, 0.92), (0.85, 0.7), (1.0, 0.38), (1.0, 0.0), (0.8, -0.0015 / max(h, 1e-6))):
        ring = []
        for i in range(sides):
            t = 2 * math.pi * i / sides
            ring.append(p + x * h * hh + (y * math.cos(t) + z * math.sin(t)) * r * rr)
        rings.append(ring)
    rings.append([p - x * 0.0015])
    g.shell(rings, paint)


def ring(g, c, axis, rmaj, rmin, paint="steel", n=14, sides=6, up=None):
    """A metal ring (a D-ring or the back ring) round `axis` at c."""
    R = frame_from(_n(axis), up or (0.0, 0.0, 1.0))
    pts = [c + (R[:, 1] * math.cos(t) + R[:, 2] * math.sin(t)) * rmaj for t in np.linspace(0, 2 * math.pi, n + 1)]
    for a, b in zip(pts[:-1], pts[1:]):
        g.tube(a, b, rmin, rmin, paint, sides, caps=False)


def buckle(g, c, nrm, along, w, h, t=0.0022, paint="steel"):
    """A rectangular buckle frame with its centre bar and tongue, lying on a surface (w across the strap, h along)."""
    R = frame_from(_n(nrm), along)            # x out, y across... (frame_from: x the first, z toward the hint)
    x, y, z = R[:, 0], R[:, 1], R[:, 2]
    o = c + x * t
    for s in (1.0, -1.0):
        g.box(o + y * s * (w / 2), (t, t * 0.8, h / 2 + t * 0.8), paint, R)       # the sides
        g.box(o + z * s * (h / 2), (t, w / 2 + t * 0.8, t * 0.8), paint, R)       # top and bottom
    g.box(o, (t * 0.9, w / 2, t * 0.6), paint, R)                                  # the centre bar
    g.box(o + z * (h * 0.22) + x * 0.0008, (t * 0.7, t * 0.5, h * 0.24), paint, R)  # the tongue


# ================================================================================================ the uniform
_uniform_layers = S.uniform_layers
_ShellField = S.ShellField


FLARE_PAST = float(os.environ.get("DETAIL_FLARE", 0.8))     # (0.5 wrapped the skirt under his seat; 1.0 a bell)


class ShellField(_ShellField):
    """soldier.py's shell field, exact to 6.2 cm from the shell, not 3.2: the skirt flares 2.6 to 5 cm out from its
    shell, so below the middle of the skirt its cloth lay beyond the exact band, where the field is read from the
    coarse grid and jumps; the cloth was made at that jump, a 5 mm staircase that read as crumpling."""

    def __init__(self, ob, h=0.005, band=0.062, levels=2):
        super().__init__(ob, h, band, levels)

    def cloth(self, loose, lo=None, hi=None, default=0.0, more=None):
        # (the flare past 2.6 cm at 0.8: the staircase had held the skirt out over his seat, the silhouette the art
        # director passed; halved, the hem drew in and the skirt wrapped under the seat; the full 4.6 cm is a bell)
        if more is not None:
            m0 = more
            more = lambda P: (lambda x: np.where(x > 0.026, 0.026 + FLARE_PAST * (x - 0.026), x))(np.asarray(m0(P)))
        return super().cloth(loose, lo, hi, default, more)


S.ShellField = ShellField


def march(L, P, way, step=0.003, n=400):
    """Walk each of the points P (outside the cloth) along `way` to where the layer's field first turns negative, in
    short steps, then halve the last step until it is found to a hundredth of a millimetre. soldier.py's skirt is
    not a true distance below the belt (its shell's field is exact only within 3.2 cm of the shell, and the flared
    skirt stands further out than that, so the field jumps there): a walk that waits for the field to reach nought
    rocks about the jump, or steps through it, where the first change of sign is the cloth the mesh is made at.
    Returns (points, found)."""
    P = np.array(P, float).reshape(-1, 3)
    w = _n(np.asarray(way, float))
    d = L.at_points(P)
    out = d > 0
    lo, hi = P.copy(), P.copy()
    live = out.copy()
    for _ in range(n):
        if not live.any():
            break
        Q = P[live] + w[None, :] * np.clip(d[live], 3e-4, step)[:, None]
        dq = L.at_points(Q)
        idx = np.flatnonzero(live)
        hit = dq <= 0
        lo[idx[hit]], hi[idx[hit]] = P[idx[hit]], Q[hit]
        P[idx], d[idx] = Q, dq
        live[idx[hit]] = False
    found = out & ~live
    for _ in range(14):
        M = 0.5 * (lo + hi)
        dm = L.at_points(M)
        neg = dm <= 0
        hi[neg], lo[~neg] = M[neg], M[~neg]
    P = np.where(found[:, None], hi, P)
    return P, found


def land(L, start, way, step=0.003, n=400, span=0.02):
    """`march` from `start` along `way` onto the layer's surface; the outward normal from four more walks `span`
    to either side (the field's own slope is ragged where the skirt's field jumps). Returns (point, outward normal)."""
    w = _n(np.asarray(way, float))
    R = frame_from(-w, (0.0, 0.0, 1.0))
    a, b = R[:, 1] * span, R[:, 2] * span
    st = np.asarray(start, float)
    P, ok = march(L, np.array([st, st + a, st - a, st + b, st - b]), w, step, n)
    nrm = _n(np.cross(P[1] - P[2], P[3] - P[4]))
    if nrm @ w > 0:
        nrm = -nrm
    return P[0], nrm


class Patch(RoundBox):
    """A patch of cloth: the cloth's own surface (`base`, a layer) lifted `t` metres, inside an outline (a rounded
    box at c, of half sizes `half` (across, up) in the frame R, deep along R's x, its first column the cloth's
    normal there). The cloth under it is read once as a height map along R's x (a march straight in from outside
    at 13 x 15 places), so it follows the cloth's curve cheaply, and its edge is a crisp step."""

    def __init__(self, base, c, half, r, R, t, deep=0.08):
        super().__init__(c, (deep, half[0], half[1]), r, R)
        self.t = float(t)
        self.us = np.linspace(-half[0] - 0.012, half[0] + 0.012, 13)
        self.ws = np.linspace(-half[1] - 0.012, half[1] + 0.012, 15)
        U, W = np.meshgrid(self.us, self.ws, indexing="ij")
        loc = np.stack([np.full(U.size, 0.06), U.ravel(), W.ravel()], axis=1)
        P = self.c + loc @ self.R.T
        P, ok = march(base, P, -self.R[:, 0])
        self.h = ((P - self.c) @ self.R[:, 0]).reshape(U.shape)
        if not ok.all():                             # (any walk that found nothing takes its neighbours' mean)
            good = ok.reshape(U.shape)
            self.h[~good] = self.h[good].mean() if good.any() else 0.0
        if os.environ.get("DETAIL_PATCH_DEBUG"):
            print(f"    PATCH c={np.round(self.c, 3)} half={half} t={t}: {int((~ok).sum())} not found,"
                  f" h {self.h.min():+.4f}..{self.h.max():+.4f}")
            np.set_printoptions(precision=3, suppress=True, linewidth=200)
            print(np.round(self.h * 1000, 1))

    def height(self, y, z):
        iy = np.clip((y - self.us[0]) / (self.us[1] - self.us[0]), 0, len(self.us) - 1.001)
        iz = np.clip((z - self.ws[0]) / (self.ws[1] - self.ws[0]), 0, len(self.ws) - 1.001)
        i0, j0 = np.floor(iy).astype(int), np.floor(iz).astype(int)
        fy, fz = iy - i0, iz - j0
        h = self.h
        return ((1 - fy) * (1 - fz) * h[i0, j0] + fy * (1 - fz) * h[i0 + 1, j0]
                + (1 - fy) * fz * h[i0, j0 + 1] + fy * fz * h[i0 + 1, j0 + 1])

    def d(self, px, py, pz):
        outline = RoundBox.d(self, px, py, pz)
        x, y, z = self.local(px, py, pz)
        x, y, z = np.broadcast_arrays(x, y, z)
        return np.maximum(outline, x - (self.height(y, z) + self.t))


class ClothMap:
    """The cloth's height along R's x over a patch at c (a `march` straight in from outside at nu x nv places), read
    back bilinearly, so pieces laid on the tunic follow its curve: `at(y, z, lift)` is the world point `lift` metres
    out from the cloth at (y, z) in R's frame (x the cloth's normal at c, z up)."""

    def __init__(self, L, c, R, half, nu=15, nv=17, margin=0.016):
        self.c, self.R = np.asarray(c, float), np.asarray(R, float)
        self.us = np.linspace(-half[0] - margin, half[0] + margin, nu)
        self.ws = np.linspace(-half[1] - margin, half[1] + margin, nv)
        U, W = np.meshgrid(self.us, self.ws, indexing="ij")
        loc = np.stack([np.full(U.size, 0.06), U.ravel(), W.ravel()], axis=1)
        P, ok = march(L, self.c + loc @ self.R.T, -self.R[:, 0])
        self.h = ((P - self.c) @ self.R[:, 0]).reshape(U.shape)
        if not ok.all():
            good = ok.reshape(U.shape)
            self.h[~good] = self.h[good].mean() if good.any() else 0.0

    def height(self, y, z):
        y, z = np.asarray(y, float), np.asarray(z, float)
        iy = np.clip((y - self.us[0]) / (self.us[1] - self.us[0]), 0, len(self.us) - 1.001)
        iz = np.clip((z - self.ws[0]) / (self.ws[1] - self.ws[0]), 0, len(self.ws) - 1.001)
        i0, j0 = np.floor(iy).astype(int), np.floor(iz).astype(int)
        fy, fz = iy - i0, iz - j0
        h = self.h
        return ((1 - fy) * (1 - fz) * h[i0, j0] + fy * (1 - fz) * h[i0 + 1, j0]
                + (1 - fy) * fz * h[i0, j0 + 1] + fy * fz * h[i0 + 1, j0 + 1])

    def at(self, y, z, lift):
        y, z, lift = np.broadcast_arrays(np.asarray(y, float), np.asarray(z, float), np.asarray(lift, float))
        loc = np.stack([self.height(y, z) + lift, y, z], axis=-1)
        return self.c + loc @ self.R.T

    def local(self, P):
        return (np.asarray(P, float) - self.c) @ self.R


def uniform_layers(man, J, spec, LM):
    ls = _uniform_layers(man, J, spec, LM)
    t0 = time.time()
    tunic, trousers, boots = ls[0], ls[1], ls[2]
    X = np.array((1.0, 0.0, 0.0))
    for s, side in (("l", 1.0), ("r", -1.0)):
        sh, el, wr = (np.array(J[k + s]) for k in ("upperarm_", "lowerarm_", "hand_"))
        ua, fa = _n(el - sh), _n(wr - el)
        axis = _n(wr - sh)
        crook = _n(X - axis * (X @ axis))                     # (the elbow's inside faces forward at rest)
        across = _n(np.cross(axis, crook))
        # the crook of the elbow: three short folds fanning across it
        for k, (u, tilt, ln) in enumerate(((-0.035, 18.0, 0.026), (-0.008, -6.0, 0.03), (0.02, 14.0, 0.024))):
            c = el + axis * u + crook * 0.05
            d = _n(across * math.cos(math.radians(tilt)) + axis * math.sin(math.radians(tilt)))
            ridge(tunic, [c - d * ln, c, c + d * ln], 0.0075, 0.003, 0.0075)   # (soft: bent, sharper ones creased like cuts)
        # the sleeve gathered above the turned-back cuff: two soft slanting folds, front and back
        cc = wr - fa * 0.075
        for k, (dz, a0, a1) in enumerate(((0.0, -70.0, 60.0), (0.03, 120.0, 230.0))):
            Rk = S.ring_frame(fa) @ rot_euler(8.0 * (1 - 2 * k), 0, 0)
            rx, ry = S.ring_axes(tunic, cc + fa * dz, Rk)
            ridge(tunic, arc(cc + fa * dz, Rk, rx, ry, a0, a1, 6), 0.006, 0.0032, 0.005)
        # the front of the armpit: the cloth drawn from the armpit's hollow up to the Y-strap, two soft folds that stop
        # at the breast pocket's outer edge (they had run on across the chest, under where the pocket now lies, and
        # lumped it: with the pocket over them they read as muscles)
        Zf = LM.get("Z", lambda z: z)
        for k in range(3):
            a = np.array((sh[0] + 0.055, side * 0.196, sh[2] - 0.1 - 0.03 * k))
            b = np.array((sh[0] + 0.1, side * 0.152, sh[2] - 0.045 - 0.04 * k))
            ridge(tunic, [a, (a + b) / 2, b], 0.0088, 0.0085, 0.005)  # (8.5 mm proud and steeper: at 7 mm they read
        # the cloth drawn by the strap at its buckle: two shallow folds from the strap's outer edge up and out toward
        # the armpit, below the breast pocket (beside the buckles the wool had lain smooth, the straps painted on)
        for k in range(3):                                    # (5.5 mm proud they hardly read: 7.5 mm)
            a = np.array((0.125, side * 0.136, Zf(1.18 + 0.025 * k)))
            b = np.array((0.088, side * 0.188, Zf(1.225 + 0.032 * k)))
            ridge(tunic, [a, (a + b) / 2, b], 0.009, 0.0075, 0.005)
        # where the strap pulls on the belt: the wool bunched by the strap's foot, two short folds slanting up and out
        # from its outer edge (begun under the strap, at 11.2 cm, the first lifted the strap's edge: a wedge at port arms)
        for k in range(2):
            a = np.array((0.13, side * 0.131, Zf(1.05 + 0.03 * k)))
            b = np.array((0.11, side * 0.176, Zf(1.095 + 0.04 * k)))
            ridge(tunic, [a, (a + b) / 2, b], 0.009, 0.0055, 0.006)
        # ---- the trousers: folds from the crotch over the inner thigh, behind and above the knee
        hip, knee, ank = (np.array(J[k + s]) for k in ("thigh_", "calf_", "foot_"))
        th = _n(knee - hip)
        for k in range(2):
            a = hip + th * (0.06 + 0.05 * k) + np.array((0.05, -side * 0.06, 0.0))
            b = hip + th * (0.17 + 0.06 * k) + np.array((0.075, -side * 0.02, 0.0))
            ridge(trousers, [a, (a + b) / 2, b], 0.008, 0.0036, 0.007)
        Rl = S.ring_frame(knee - ank)
        for k, (dz, a0, a1, r_) in enumerate(((0.035, 140.0, 220.0, 0.0065), (0.0, 130.0, 230.0, 0.007), (-0.03, 145.0, 215.0, 0.006))):
            c = knee + _n(knee - ank) * dz
            rx, ry = S.ring_axes(trousers, c, Rl @ rot_euler(6.0 * (k - 1), 0, 0))
            ridge(trousers, arc(c, Rl @ rot_euler(6.0 * (k - 1), 0, 0), rx, ry, a0, a1, 5), r_ * 1.15, 0.0042, 0.006)
        for k, dz in enumerate((0.075, 0.115)):                # (the cloth slack over the knee)
            c = knee + _n(knee - ank) * dz
            rx, ry = S.ring_axes(trousers, c, Rl)
            ridge(trousers, arc(c, Rl @ rot_euler(-5.0 + 10.0 * k, 0, 0), rx, ry, -55.0, 55.0, 5), 0.0075, 0.0032, 0.007)
        # ---- the jackboots' ankle wrinkles: the leather shaft gathered over the instep, front and sides
        if spec["tall_boots"]:
            Rb = S.ring_frame(knee - ank)
            for k, dz in enumerate((0.055, 0.08, 0.108, 0.138)):
                z = LM["ankle"] + dz
                c = S.leg_point(J, s, z)
                Rk = Rb @ rot_euler(7.0 * (1 if k % 2 else -1), 0, 0)
                rx, ry = S.ring_axes(boots, c, Rk)
                ridge(boots, arc(c, Rk, rx, ry, -105.0 + 10 * k, 105.0 - 8 * k, 9), 0.0052, 0.0032, 0.004)
    # ---- the pockets and the button strip, where a tunic has them. soldier.py's march onto the cloth slid the pockets
    # toward the middle (the two breast pockets met over the buttons, the skirt's sat by the strip), and each was one
    # flat box: out at the chest's sides, where the cloth curves away, a flat box stands proud at its edges and reads
    # as a bag (and the strip stood out of the hollow of his breastbone). So they are taken out; the strip is laid
    # again as a patch of the cloth itself, 2 mm proud (`Patch`), and the pockets' places kept for `build_pockets`:
    # breast pockets 9.5 cm and skirt pockets 11.8 cm either side of the buttons
    old = LM.get("pockets", [])
    if old:
        Z = LM["Z"]
        olds = [p for p, R, half in old] + [p + R @ np.array((0.005, 0.0, half[2] - 0.013)) for p, R, half in old]
        strip = [op for op in tunic.ops if isinstance(op[1], RoundBox) and op[1].R is not None
                 and np.allclose(op[1].half[:2], (0.006, 0.014), atol=1e-9)]
        keep = [op for op in tunic.ops if not (isinstance(op[1], RoundBox) and any(np.allclose(op[1].c, q, atol=1e-9) for q in olds))
                and not any(op is q for q in strip)]
        print(f"  detail: pockets and strip taken out ({len(tunic.ops) - len(keep)} shapes) and laid again")
        tunic.ops = keep
        base = Layer("tunic_base", tunic.paint)
        base.ops = list(keep)
        if strip:
            sb = strip[0][1]
            top, bot = sb.c[2] + sb.half[2], sb.c[2] - sb.half[2]
            q, nq = land(base, (0.4, 0.0, 0.5 * (top + bot)), (-1.0, 0.0, 0.0))
            tunic.add(Patch(base, q, (0.014, 0.5 * (top - bot)), 0.004, frame_from(nq, (0.0, 0.0, 1.0)), 0.002), 0.001)
        # the four pockets themselves are made as pieces of their own, with crisp edges (`build_pockets`, with the kit):
        # cut into the cloth's own surface they were lost at its meshing grid (3.4 mm) and read as muscles, not pockets
        LM["pockets"] = []
        for side in (1.0, -1.0):
            for aim_, half in (((0.4, 0.09 * side, Z(1.305)), (0.0068, 0.049, 0.058)),
                               ((0.4, 0.118 * side, Z(0.875)), (0.0066, 0.062, 0.072))):
                c0, n0 = land(base, np.array(aim_), (-1.0, 0.0, 0.0))
                LM["pockets"].append((c0, frame_from(n0, (0.0, 0.0, 1.0)), half))
    LM["flap_points"] = []
    # the shoulder straps: taken out of the cloth and made as pieces of their own (`shoulder_strap`, with the kit)
    eps = [op for op in tunic.ops if op[0] == "add" and type(op[1]) is RoundBox and op[1].R is not None
           and np.allclose(op[1].half[1:], (0.021, 0.006), atol=1e-9)]
    if eps:
        tunic.ops = [op for op in tunic.ops if not any(op is e for e in eps)]
        LM["shoulder_straps"] = [(np.array(e[1].c, float), np.array(e[1].R, float), 2.0 * float(e[1].half[0])) for e in eps]
        print(f"  detail: {len(eps)} shoulder straps taken out of the cloth")
    print(f"  detail: folds, pleats and points laid in {time.time() - t0:.0f} s")
    return ls


S.uniform_layers = uniform_layers


# ================================================================================================ the kit
_build_kit = S.build_kit


def _wind(V, f, out):
    """A face (indices into V) wound so its normal faces `out`."""
    a, b, c = (np.asarray(V[i]) for i in f[:3])
    return tuple(f) if np.cross(b - a, c - a) @ np.asarray(out) >= 0 else tuple(reversed(f))


# the tunic's four pockets (metres): the bag `t` proud of the cloth in the middle and `edge` at its rim, sunk `sink`
# into the cloth; the flap `flap` thick, its lip `lip` over the bag, `drop` deep at its sides and pointed `point`
# lower in the middle; a box pleat `pleat` either side of the middle, `pleat_up` over the bag
POCKET = dict(edge=0.0046, sink=0.0025, flap=0.0026, lip=0.0058, drop=0.036, point=0.012, pleat=0.0105, pleat_up=0.0012)


class Pocket:
    """One patch pocket laid on the cloth as pieces of its own: a filled bag with a crisp rim, a pleat down its middle
    and a pointed flap whose lip stands off the bag (a dark gap under it), all following the cloth's curve."""

    def __init__(self, L, c, R, half):
        self.t, self.hy, self.hz = half
        self.cm = ClothMap(L, c, R, (self.hy + 0.006, self.hz + 0.006))
        self.R = self.cm.R
        P = POCKET
        self.W = self.hy + 0.003
        self.z_top = self.hz - 0.001

    def rho(self, y, z, n=6.0):
        return (np.abs(np.asarray(y) / self.hy) ** n + np.abs(np.asarray(z) / self.hz) ** n) ** (1.0 / n)

    def bag_lift(self, y, z):
        t = self.t
        return np.interp(self.rho(y, z), [0.0, 0.55, 0.9, 1.0], [t, t, 0.94 * t, POCKET["edge"]])

    def z_bot(self, u):
        return self.hz - POCKET["drop"] - POCKET["point"] * (1.0 - np.abs(u))

    def flap_lift(self, v):
        P = POCKET
        return self.t + 0.0008 + (P["lip"] - 0.0008) * np.clip(v, 0.0, 1.0) ** 1.3

    def lift(self, P_world):
        """How far the pocket stands off the cloth under each world point (0 off it): for laying straps over it."""
        loc = self.cm.local(np.atleast_2d(P_world))
        y, z = loc[:, 1], loc[:, 2]
        out = np.where(self.rho(y, z) <= 1.0, self.bag_lift(y, z), 0.0)
        pl = (np.abs(y) <= POCKET["pleat"]) & (z > -self.hz + 0.008) & (z < self.z_bot(0.0) + 0.006)
        out = np.where(pl, out + POCKET["pleat_up"], out)
        u = np.clip(y / self.W, -1.0, 1.0)
        zb = self.z_bot(u)
        v = (self.z_top - z) / np.maximum(self.z_top - zb, 1e-6)
        inf = (np.abs(y) <= self.W) & (v >= 0.0) & (v <= 1.0)
        return np.where(inf, np.maximum(out, self.flap_lift(v)), out)

    def build(self, g, paint="tunic", lite=False):
        """The pocket; `lite`: the middle level's, few faces but the same crisp flap and lip (thinned from the near
        pocket, the flap and bag melted into soft pads at 10 m)."""
        P, cm, R = POCKET, self.cm, self.R
        X, Y, Zx = R[:, 0], R[:, 1], R[:, 2]
        # ---- the bag: rings of a rounded rectangle from its sunk foot to its middle
        K = 12 if lite else 20
        th = 2 * math.pi * (np.arange(K) + 0.5) / K
        cs, sn = np.cos(th), np.sin(th)
        oy = self.hy * np.sign(cs) * np.abs(cs) ** (2 / 6.0)
        oz = self.hz * np.sign(sn) * np.abs(sn) ** (2 / 6.0)
        rings = ([(1.0, -P["sink"]), (1.0, P["edge"]), (0.8, self.t)] if lite else
                 [(1.0, -P["sink"]), (1.0, P["edge"]), (0.9, 0.94 * self.t), (0.55, self.t)])
        V, F = [], []
        idx = []
        for sc, lf in rings:
            pts = cm.at(oy * sc, oz * sc, lf)
            idx.append(list(range(len(V), len(V) + K)))
            V += list(pts)
        centre = len(V)
        V.append(cm.at(0.0, 0.0, self.t))
        for r in range(len(rings) - 1):
            a, b = idx[r], idx[r + 1]
            for k in range(K):
                k1 = (k + 1) % K
                out = X if r > 0 else (Y * oy[k] / self.hy + Zx * oz[k] / self.hz)
                F.append(_wind(V, (a[k], a[k1], b[k1], b[k]), out))
        for k in range(K):
            F.append(_wind(V, (idx[-1][k], idx[-1][(k + 1) % K], centre), X))
        g._add(V, F, paint)
        if lite:
            return self.build_flap(g, paint, 3, 2)
        # ---- the box pleat down the middle, under the flap at its top
        z0, z1 = -self.hz + 0.0025, self.z_bot(0.0) + 0.006       # (from the bag's foot, rising out of it: no tab's end)
        nz = 7
        zs = np.linspace(z0, z1, nz)
        up = P["pleat_up"] * np.clip((zs - z0) / 0.014, 0.0, 1.0)
        V, F = [], []
        cols = []
        for yy in (-P["pleat"], P["pleat"]):
            top = cm.at(np.full(nz, yy), zs, self.bag_lift(yy, zs) + up)
            foot = cm.at(np.full(nz, yy), zs, self.bag_lift(yy, zs) - 0.0015)
            cols.append((list(range(len(V), len(V) + nz)), list(range(len(V) + nz, len(V) + 2 * nz))))
            V += list(top) + list(foot)
        (tl, fl), (tr, fr) = cols
        for j in range(nz - 1):
            F.append(_wind(V, (tl[j], tr[j], tr[j + 1], tl[j + 1]), X))
            F.append(_wind(V, (fl[j], tl[j], tl[j + 1], fl[j + 1]), -Y))
            F.append(_wind(V, (tr[j], fr[j], fr[j + 1], tr[j + 1]), Y))
        F.append(_wind(V, (fl[0], fr[0], tr[0], tl[0]), -Zx))
        g._add(V, F, paint)
        return self.build_flap(g, paint, 7, 3)

    def build_flap(self, g, paint, nu, nv):
        """The flap, pointed at the bottom, its lip off the bag, and its button's place."""
        P, cm, R = POCKET, self.cm, self.R
        X, Y, Zx = R[:, 0], R[:, 1], R[:, 2]
        us, vs = np.linspace(-1.0, 1.0, nu), np.linspace(0.0, 1.0, nv)
        V, F = [], []
        T_, U_ = {}, {}
        for i, u in enumerate(us):
            for j, v in enumerate(vs):
                y = u * self.W
                z = self.z_top + (self.z_bot(u) - self.z_top) * v
                T_[i, j] = len(V)
                V.append(cm.at(y, z, self.flap_lift(v)))
                U_[i, j] = len(V)
                V.append(cm.at(y, z, self.flap_lift(v) - P["flap"]))
        for i in range(nu - 1):
            for j in range(nv - 1):
                F.append(_wind(V, (T_[i, j], T_[i + 1, j], T_[i + 1, j + 1], T_[i, j + 1]), X))
                F.append(_wind(V, (U_[i, j], U_[i, j + 1], U_[i + 1, j + 1], U_[i + 1, j]), -X))
            F.append(_wind(V, (T_[i, nv - 1], T_[i + 1, nv - 1], U_[i + 1, nv - 1], U_[i, nv - 1]), -Zx))   # the lip
            F.append(_wind(V, (T_[i, 0], U_[i, 0], U_[i + 1, 0], T_[i + 1, 0]), Zx))
        for j in range(nv - 1):
            F.append(_wind(V, (T_[0, j], T_[0, j + 1], U_[0, j + 1], U_[0, j]), -Y))
            F.append(_wind(V, (T_[nu - 1, j], U_[nu - 1, j], U_[nu - 1, j + 1], T_[nu - 1, j + 1]), Y))
        g._add(V, F, paint)
        # ---- its button, on the flap above the point
        z = self.z_bot(0.0) + 0.0105
        v = (self.z_top - z) / (self.z_top - self.z_bot(0.0))
        p0 = cm.at(0.0, z, self.flap_lift(v))
        dy = cm.at(0.003, z, self.flap_lift(v)) - cm.at(-0.003, z, self.flap_lift(v))
        vz = lambda zz: (self.z_top - zz) / (self.z_top - self.z_bot(0.0))
        dz = cm.at(0.0, z + 0.003, self.flap_lift(vz(z + 0.003))) - cm.at(0.0, z - 0.003, self.flap_lift(vz(z - 0.003)))
        n = _n(np.cross(dy, dz))
        return p0, (n if n @ X > 0 else -n)


def build_pockets(tunic, LM_):
    """The four pockets as one piece (the tunic's paint), the flaps' button places, and the pockets for the straps."""
    g = Geo()
    pockets, buttons = [], []
    for c, R, half in LM_.get("pockets", []):
        pk = Pocket(tunic, c, R, half)
        buttons.append(pk.build(g))
        pockets.append(pk)
    LM_["pocket_objs"] = pockets
    return (g.build("tunic_pockets", sharp=40.0, recalc=False) if pockets else None), buttons, pockets


def lay_strap(g, L, pts, width, thick, pockets=(), paint="straps", gap=0.0025, keep_y=None):
    """A leather strap laid on the cloth as stiff leather lies: a ribbon along an eased path, its two edges and its
    middle each lifted only straight out (along the path's normal) wherever the cloth or a pocket under it would come
    through, the lifts eased along it, so it bends a little across a hollow but never steps (soldier.py's strap_on
    pulled each edge onto the cloth along the field's slope: over a pocket's rim or the shoulder's top, teeth).
    keep_y: hold the strap in the plane y = keep_y (the back strap: the cloth's slope had walked it sideways into an
    S). Returns the path (its underside's middle), normals, tangents and its laid underside's two edges."""
    P, N = S.project(L, pts)
    if keep_y is not None:
        P[:, 1] = keep_y
        N[:, 1] = 0.0
        N /= np.linalg.norm(N, axis=1)[:, None]
    for _ in range(3):
        Q = P.copy()
        Q[1:-1] = (P[:-2] + 2 * P[1:-1] + P[2:]) / 4
        d = L.at_points(Q)
        P = Q + N * np.maximum(gap - d, 0.0)[:, None]
        if keep_y is not None:
            P[:, 1] = keep_y
    for _ in range(3):
        N[1:-1] = N[:-2] + 2 * N[1:-1] + N[2:]
        N /= np.linalg.norm(N, axis=1)[:, None]
    T = np.gradient(P, axis=0)
    T /= np.linalg.norm(T, axis=1)[:, None]
    N = N - T * (N * T).sum(axis=1)[:, None]
    N /= np.linalg.norm(N, axis=1)[:, None]
    B = np.cross(T, N)
    B /= np.linalg.norm(B, axis=1)[:, None]

    def ease(a, n=3):
        a = np.asarray(a, float).copy()
        for _ in range(n):
            a[1:-1] = (a[:-2] + 2 * a[1:-1] + a[2:]) / 4
        return a
    cols = (-1.0, 0.0, 1.0)
    lifts = []
    for c in cols:                                   # each column (an edge, the middle) clear of what lies under it
        lift = np.zeros(len(P))
        for k in (c - 1.0 / 3.0, c, c + 1.0 / 3.0):
            if abs(k) > 1.0 + 1e-9:
                continue
            C = P + B * (k * width * 0.5)
            need = np.full(len(P), gap)
            for pk in pockets:
                need = np.maximum(need, pk.lift(C) + 0.0012)
            lift = np.maximum(lift, need - L.at_points(C))
        lift = np.maximum(lift, 0.0)
        raw = lift
        lift = np.maximum.reduce([lift, np.r_[lift[1:], 0.0], np.r_[0.0, lift[:-1]]])
        lifts.append(np.maximum(ease(lift, 4), raw))
    mid = np.maximum(lifts[1], 0.5 * (lifts[0] + lifts[2]))  # (the middle never under the line between its edges)
    lifts[1] = mid
    Cs = [P + B * (c * width * 0.5) + N * lf[:, None] for c, lf in zip(cols, lifts)]
    P = Cs[1]
    verts, faces = [], []
    for i in range(len(P)):
        e1, m, e2 = Cs[2][i], Cs[1][i], Cs[0][i]
        verts += [e1 + N[i] * thick, m + N[i] * thick, e2 + N[i] * thick, e2, m, e1]
    for i in range(len(P) - 1):
        j = i + 1
        for k in range(6):
            faces.append((6 * i + k, 6 * i + (k + 1) % 6, 6 * j + (k + 1) % 6, 6 * j + k))
    e = 6 * (len(P) - 1)
    faces.append((5, 4, 3, 2, 1, 0))
    faces.append((e, e + 1, e + 2, e + 3, e + 4, e + 5))
    g._add(verts, faces, paint)
    return P, N, T, (Cs[0], Cs[2])


def on_strap(P, N, T, E, i, thick):
    """The strap's own top at its i-th point: the middle of its laid top, the true normal of its face there (across
    its two laid edges: where the cloth curves away under it the strap tilts, and a buckle laid by the path's normal
    stood off it on one side) and its way along."""
    across = E[1][i] - E[0][i]
    n = _n(np.cross(across, T[i]))
    if n @ N[i] < 0:
        n = -n
    t = _n(T[i] - n * (T[i] @ n))
    return 0.5 * (E[0][i] + E[1][i]) + n * thick, n, t


def slab(g, Top, Bot, out, paint):
    """A plate between two grids of points (nu x nv: its top and its underside), walled round its edge."""
    nu, nv = Top.shape[:2]
    V = list(Top.reshape(-1, 3)) + list(Bot.reshape(-1, 3))
    t = lambda i, j: i * nv + j                                       # noqa: E731
    b = lambda i, j: nu * nv + i * nv + j                             # noqa: E731
    F = []
    for i in range(nu - 1):
        for j in range(nv - 1):
            F.append(_wind(V, (t(i, j), t(i + 1, j), t(i + 1, j + 1), t(i, j + 1)), out))
            F.append(_wind(V, (b(i, j), b(i, j + 1), b(i + 1, j + 1), b(i + 1, j)), -out))
    rim = ([(i, 0) for i in range(nu)] + [(nu - 1, j) for j in range(1, nv)] + [(i, nv - 1) for i in range(nu - 2, -1, -1)]
           + [(0, j) for j in range(nv - 2, 0, -1)])
    centre = Top.reshape(-1, 3).mean(axis=0)
    for k in range(len(rim)):
        (i0, j0), (i1, j1) = rim[k], rim[(k + 1) % len(rim)]
        o = (Top[i0, j0] + Top[i1, j1]) / 2 - centre
        o = o - out * (o @ out)
        F.append(_wind(V, (t(i0, j0), t(i1, j1), b(i1, j1), b(i0, j0)), o))
    g._add(V, F, paint)


def shoulder_strap(g, L, c, R, length, straps=(), paint="helmet"):
    """The tunic's shoulder strap as a piece of its own (soldier.py's was a lump of the cloth, the tunic's colour, that
    the Y-strap had to climb: it pinched there, and the shoulder strap did not read at 4 m): a stiff cloth strap of the
    collar patches' darker paint, 4.2 cm wide and 2.6 mm thick, rounded at the collar end, following the shoulder's
    top and lying over the Y-strap where they cross (the Y-strap runs under it, as it was worn). c, R: its middle and
    frame (x along it from the collar, z out of the shoulder). Returns its button's place and normal."""
    X, Y, Zn = R[:, 0], R[:, 1], R[:, 2]
    hw0, th, tip, bev = 0.021, 0.0024, 0.018, 0.0012
    us = np.linspace(-length / 2, length / 2, 11)
    vs = np.linspace(-1.0, 1.0, 5)
    U, V = np.meshgrid(us, vs, indexing="ij")
    a = np.clip((us[0] + tip - U) / tip, 0.0, 1.0)
    Yl = V * hw0 * np.sqrt(np.clip(1.0 - a * a, 0.12, 1.0))
    start = c + U[..., None] * X + Yl[..., None] * Y + 0.04 * Zn
    Pc, ok = march(L, start.reshape(-1, 3), -Zn)
    if not ok.all():
        Pc[~ok] = Pc[ok].mean(axis=0) if ok.any() else c
    Q = Pc
    lift = np.full(len(Q), 0.0006)
    for SP, SN, ST, sw, sth in straps:
        k = np.argmin(((Q[:, None, :] - SP[None, :, :]) ** 2).sum(axis=2), axis=1)
        dq = Q - SP[k]
        lat = dq - ST[k] * (dq * ST[k]).sum(axis=1)[:, None]
        lat = lat - SN[k] * (lat * SN[k]).sum(axis=1)[:, None]
        top = ((SP[k] + SN[k] * sth - Q) * Zn).sum(axis=1) + 0.001
        hit = np.linalg.norm(lat, axis=1) < sw / 2 + 0.004
        lift = np.maximum(lift, np.where(hit, top, 0.0))
    lift = lift.reshape(U.shape)
    raw = lift.copy()
    for _ in range(2):                                    # (eased along it: a stiff strap bridges, it does not step)
        lift[1:-1] = np.maximum(lift[1:-1], 0.5 * (lift[:-2] + lift[2:]))
    for _ in range(2):
        lift[1:-1] = 0.25 * lift[:-2] + 0.5 * lift[1:-1] + 0.25 * lift[2:]
    lift = np.maximum(lift, raw)
    Bot = Pc.reshape(U.shape + (3,)) + lift[..., None] * Zn
    Top = Bot + th * Zn                                 # (its top drawn in from the edges: a sewn strap's rounded edge)
    Top[:, 0] += Y * bev                                # (v = -1 is the -Y side)
    Top[:, -1] -= Y * bev
    Top[0] += X * bev
    Top[-1] -= X * bev
    slab(g, Top, Bot, Zn, paint)
    q = Bot[1, 2] + th * Zn
    return q, Zn


def leather_cell(g, c, R, a, hw, b, paint="straps"):
    """A stiff leather ammunition pouch cell (half depth a, half width hw, half height b, at c in R: x out, z up): a
    flat front and back, its top rounded to the front, a flap over the top and a third of the way down the front
    standing 1.8 mm off it (its edge a line), a strap down the front from the flap to a stud."""
    X, Y, Zx = R[:, 0], R[:, 1], R[:, 2]
    rt, rb = 0.011, 0.003
    prof = []                                             # the cell's side, (x, z), round from the front's foot
    prof += [(a - rb + rb * math.cos(t), -b + rb - rb * math.sin(t)) for t in np.linspace(0.0, math.pi / 2, 2)]
    prof += [(-a + rb - rb * math.sin(t), -b + rb - rb * math.cos(t)) for t in np.linspace(0.0, math.pi / 2, 2)]
    prof += [(-a + 0.004 - 0.004 * math.cos(t), b - 0.004 + 0.004 * math.sin(t)) for t in np.linspace(0.0, math.pi / 2, 2)]
    prof += [(a - rt + rt * math.sin(t), b - rt + rt * math.cos(t)) for t in np.linspace(0.0, math.pi / 2, 4)]
    V, F = [], []
    ends = []
    for yy in (-hw, hw):
        ends.append(list(range(len(V), len(V) + len(prof))))
        V += [c + X * x + Y * yy + Zx * z for x, z in prof]
    n = len(prof)
    for k in range(n):
        k1 = (k + 1) % n
        mid = np.array(prof[k]) + np.array(prof[k1])
        out = X * mid[0] + Zx * mid[1]
        F.append(_wind(V, (ends[0][k], ends[0][k1], ends[1][k1], ends[1][k]), out))
    F.append(_wind(V, tuple(ends[0]), -Y))
    F.append(_wind(V, tuple(ends[1]), Y))
    g._add(V, F, paint)
    # the flap: over the top from the back and down the front to a third of the way
    zl = b - 0.036
    path = [(-a + 0.003, b)] + [(a - rt + rt * math.sin(t), b - rt + rt * math.cos(t)) for t in np.linspace(0.0, math.pi / 2, 4)] + [(a, zl)]
    path = np.array(path)
    tang = np.gradient(path, axis=0)
    nrm = np.stack([tang[:, 1], -tang[:, 0]], axis=1)
    nrm /= np.linalg.norm(nrm, axis=1)[:, None]
    outer, inner = path + nrm * 0.0018, path - nrm * 0.0004
    V, F = [], []
    m = len(path)
    rows = {}
    for side, yy in (("l", -hw - 0.0012), ("r", hw + 0.0012)):
        rows[side, "o"] = list(range(len(V), len(V) + m))
        V += [c + X * x + Y * yy + Zx * z for x, z in outer]
        rows[side, "i"] = list(range(len(V), len(V) + m))
        V += [c + X * x + Y * yy + Zx * z for x, z in inner]
    for k in range(m - 1):
        o = X * (nrm[k, 0] + nrm[k + 1, 0]) + Zx * (nrm[k, 1] + nrm[k + 1, 1])
        F.append(_wind(V, (rows["l", "o"][k], rows["l", "o"][k + 1], rows["r", "o"][k + 1], rows["r", "o"][k]), o))
        F.append(_wind(V, (rows["l", "i"][k], rows["l", "i"][k + 1], rows["r", "i"][k + 1], rows["r", "i"][k]), -o))
        F.append(_wind(V, (rows["l", "o"][k], rows["l", "o"][k + 1], rows["l", "i"][k + 1], rows["l", "i"][k]), -Y))
        F.append(_wind(V, (rows["r", "o"][k], rows["r", "o"][k + 1], rows["r", "i"][k + 1], rows["r", "i"][k]), Y))
    F.append(_wind(V, (rows["l", "o"][-1], rows["r", "o"][-1], rows["r", "i"][-1], rows["l", "i"][-1]), -Zx))   # its edge
    F.append(_wind(V, (rows["l", "o"][0], rows["r", "o"][0], rows["r", "i"][0], rows["l", "i"][0]), -X))
    g._add(V, F, paint)
    # the strap from under the flap's edge to the stud, and the stud
    g.box(c + X * (a + 0.0011) + Zx * (zl - 0.012), (0.0011, min(0.0055, hw * 0.35), 0.0145), paint, R)
    g.tube(c + X * (a + 0.0004) + Zx * (zl - 0.028), c + X * (a + 0.0034) + Zx * (zl - 0.028), 0.0034, 0.0026, "steel", 6, up=tuple(Zx))


def build_kit(J, spec, body_ls, Z=None):
    tunic, collar = body_ls[0], body_ls[3]
    Z = Z or (lambda z: z)
    SCALE.update(tris=3.0, h=0.72)
    try:
        out = _build_kit(J, spec, body_ls, Z)
    finally:
        SCALE.update(tris=1.0, h=1.0)
    t0 = time.time()
    for name in ("belt", "details", "pouches", "bread_bag", "straps"):
        if name in out:
            ob = out[name][0]
            me = ob.data
            bpy.data.objects.remove(ob)
            bpy.data.meshes.remove(me)
            del out[name]
    LM_ = S._DETAIL_LM
    zb = Z(1.0)
    cx = J["spine_01"][0] + 0.03
    # ---- the belt: smooth round the waist, its buckle a plate with a round boss in a ring
    g = Geo()
    ang = np.linspace(0, 2 * math.pi, 84, endpoint=False)
    start = np.stack([cx + np.cos(ang) * 0.2, np.sin(ang) * 0.2, np.full_like(ang, zb)], axis=1)
    P, N = S.project(tunic, start)
    P[:, 2] = zb
    N[:, 2] = 0.0                                  # (eased round the waist and kept clear of the cloth: crossing the
    N /= np.linalg.norm(N, axis=1)[:, None]        # button strip its edges had turned, a twist by the buckle)
    for _ in range(4):
        P[:, :2] = ((np.roll(P, 1, axis=0) + 2 * P + np.roll(P, -1, axis=0)) / 4)[:, :2]
        N = np.roll(N, 1, axis=0) + 2 * N + np.roll(N, -1, axis=0)
        N /= np.linalg.norm(N, axis=1)[:, None]
    # (each edge clear of the cloth by itself, the band tilted between them: over the hips the cloth below the belt
    # stands further out than the waist above it, and a band held upright off the lower edge stood off the waist
    # like a hoop, its inside showing)
    def clear(dz, want):
        return want - tunic.at_points(P + np.array((0.0, 0.0, dz)))
    top, mid, bot = clear(0.021, 0.0035), clear(0.0, 0.005), clear(-0.021, 0.0035)
    for _ in range(2):
        top, mid, bot = (np.maximum.reduce([a, np.roll(a, 1), np.roll(a, -1)]) for a in (top, mid, bot))
    for _ in range(3):
        top, mid, bot = ((np.roll(a, 1) + 2 * a + np.roll(a, -1)) / 4 for a in (top, mid, bot))
    off = 0.5 * (top + bot)
    slope = np.clip(0.5 * (bot - top), -0.008, 0.008)           # (half the edges' difference, at most 8 mm: 21 degrees)
    off = np.maximum(off, mid)
    off = np.maximum(off, np.maximum(top, bot) - np.abs(slope))  # (neither edge into the cloth)
    off = np.maximum(off, -0.002)
    P = P + N * off[:, None]
    tilt = np.arctan2(2.0 * slope, 0.042)                       # (the band's face turned up where its foot stands out)
    Nb = N * np.cos(tilt)[:, None] + np.array((0.0, 0.0, 1.0)) * np.sin(tilt)[:, None]
    Nb /= np.linalg.norm(Nb, axis=1)[:, None]
    print(f"    belt: off the cloth {off.min() * 1000:.1f}..{off.max() * 1000:.1f} mm, tilted up to "
          f"{np.degrees(np.abs(tilt)).max():.0f} degrees")
    g.band(P, Nb, 0.046, 0.006, "straps", closed=True)
    N = Nb
    front, n0 = P[0] + N[0] * 0.001, N[0]
    Rb = frame_from(n0, (0.0, 0.0, 1.0))
    B = Layer("buckle", "steel")
    B.add(RoundBox(front + Rb @ np.array((0.003, 0.0, 0.0)), (0.0028, 0.03, 0.025), 0.0035, Rb))
    B.add(Tube(front + Rb @ np.array((0.0062, 0.0, 0.0)), -0.0012, 0.0012, (0.0155, 0.0155), (0.0145, 0.0145), 0.001,
               Rb @ rot_euler(0, 90, 0)), 0.0006)                                   # the round boss
    B.sub(Torus(front + Rb @ np.array((0.0068, 0.0, 0.0)), 0.0185, 0.0016, Rb @ rot_euler(0, 90, 0)), 0.0005)
    B.add(Torus(front + Rb @ np.array((0.0058, 0.0, 0.0)), 0.0215, 0.0012, Rb @ rot_euler(0, 90, 0)), 0.0005)
    g.absorb(_mesh_from_layers("buckle", [B], 0.0009, 450, detail=0.004))
    out["belt"] = (g.build("belt"), None)
    # ---- the small things on the tunic: domed buttons, the collar patches with their two bars
    g = Geo()
    spots = [((0.3, 0.0, z), (-1.0, 0.0, 0.0)) for z in (Z(1.405), Z(1.325), Z(1.245), Z(1.165), Z(1.085))]
    for start, way in spots:
        p, nrm = on_surface(tunic, start, way)
        dome(g, p - nrm * 0.0004, nrm, 0.0082, 0.0042)
    pk_ob, pk_buttons, pockets = build_pockets(tunic, LM_)
    if pk_ob is not None:
        out["tunic_pockets"] = (pk_ob, None)
    for q, nq in pk_buttons:                                                            # each flap's button
        dome(g, q - nq * 0.0004, nq, 0.0072, 0.0038)
    nmid, nfwd, nrx, nry = LM_["neck_ring"]
    for side in (1.0, -1.0):
        sh = np.array(J["upperarm_" + ("l" if side > 0 else "r")])
        inner = nmid + np.array((0.0, side * (nry + 0.012), 0.0))
        outer = sh + np.array((0.0, side * 0.035, 0.0))
        at = inner + _n(outer - inner) * 0.028
        if not LM_.get("shoulder_straps"):
            p, nrm = on_surface(tunic, at + np.array((0.0, 0.0, 0.25)), (0.0, 0.0, -1.0))
            dome(g, p - nrm * 0.0004, nrm, 0.0075, 0.0038, up=tuple(_n(outer - inner)))   # the shoulder strap's button
        p, nrm = on_surface(collar, (0.3, 0.036 * side, Z(1.452)), (-1.0, -0.3 * side, 0.0))
        Rp = frame_from(nrm, (0.0, 0.0, 1.0))
        g.box(p + nrm * 0.0004, (0.0016, 0.0155, 0.0078), "helmet", Rp)                 # the patch
        for dz in (0.0033, -0.0033):                                                    # and its two bars
            g.box(p + nrm * 0.0022 + Rp[:, 2] * dz, (0.0011, 0.0135, 0.0016), "lower", Rp)
    out["details"] = (g.build("details", sharp=50.0), None)
    # ---- the ammunition pouches: three cells either side of the buckle, each flap held by a strap to a stud
    po = dict(spec.get("pouches") or {})
    cells = {3: ((0.06, 0.0195), (0.103, 0.0195), (0.143, 0.0195)), 1: ((0.095, 0.042),), 0: ()}[po.get("front", 3)]
    g = Geo()
    for side in (1.0, -1.0):
        for k, (y, hw) in enumerate(cells):
            a = math.atan2(y * side, 0.12)
            p0, n0 = S.project(tunic, [(cx + math.cos(a) * 0.2, math.sin(a) * 0.2, zb - 0.012)])
            p0, n0 = p0[0], n0[0]
            R = frame_from(n0, (0.0, 0.0, 1.0))
            leather_cell(g, p0 + n0 * 0.028, R, 0.021, hw, 0.04)
    if po.get("rear"):
        p0, n0 = S.project(tunic, [(cx - 0.2, 0.0, zb - 0.012)])
        p0, n0 = p0[0], n0[0]
        leather_cell(g, p0 + n0 * 0.032, frame_from(n0, (0.0, 0.0, 1.0)), 0.026, 0.07, 0.045)
    if cells or po.get("rear"):
        out["pouches"] = (g.build("pouches", sharp=40.0, recalc=False), None)
    # ---- the bread bag at the right hip: its flap held by two straps to buckles below
    L = Layer("bread_bag", "pack")
    flap = Layer("bread_flap", "pack")
    strap = Layer("bread_straps", "straps")
    p0, n0 = S.project(tunic, [(-0.03, -0.3, Z(0.905))])
    p0, n0 = p0[0], n0[0]
    R = frame_from(n0, (0.0, 0.0, 1.0))
    c = p0 + n0 * 0.034
    L.add(RoundBox(c, (0.032, 0.11, 0.075), 0.02, R))
    flap.add(RoundBox(c + R @ np.array((0.006, 0.0, 0.03)), (0.03, 0.112, 0.05), 0.016, R))
    for y in (0.055, -0.055):
        strap.add(RoundBox(c + R @ np.array((0.0372, y, 0.022)), (0.0016, 0.009, 0.042), 0.0012, R), 0.0)        # over the flap
        strap.add(RoundBox(c + R @ np.array((0.0333, y, -0.034)), (0.0016, 0.009, 0.016), 0.0012, R), 0.0015)    # and down the bag
        strap.add(RoundBox(c + R @ np.array((0.035, y, -0.052)), (0.0022, 0.011, 0.007), 0.0012, R), 0.001)       # the buckle tab
    out["bread_bag"] = (_mesh_from_layers("bread_bag", [L, flap, strap], 0.0018, 650, detail=0.004, cut_paints=True),
                        "pelvis")         # (the straps cut clean along their edges: thinned uncut they were dark teeth)
    if "bottle" in out:            # (the water bottle hung on the bag's flap and straps, 8 mm out: it was made against
        ob = out["bottle"][0]      #  soldier.py's plainer bag, and the flap and strap stood through its felt)
        ob.data.transform(Matrix.Translation(Vector(tuple(n0 * 0.008))))
        ob.data.update()
    # ---- the bayonet in its frog, over the entrenching tool's case at the left hip
    g = Geo()
    p0, n0 = S.project(tunic, [(-0.1, 0.3, zb - 0.085)])
    p0, n0 = p0[0], _n(n0[0] * np.array((1.0, 1.0, 0.0)))
    Rs = frame_from(n0, (0.0, 0.0, 1.0)) @ rot_euler(28.0, 0, 0)
    cs = p0 + n0 * 0.026
    top = cs + Rs @ np.array((0.024, 0.028, 0.05))
    down = -Rs[:, 2]
    g.tube(top, top + down * 0.295, 0.0125, 0.0085, "steel", 12, up=tuple(n0), squash=0.42)       # the scabbard
    g.tube(top + down * 0.29, top + down * 0.302, 0.0085, 0.0045, "steel", 12, up=tuple(n0), squash=0.42)
    g.box(top + down * 0.04, (0.009, 0.019, 0.03), "straps", Rs)                                 # the frog
    g.tube(top - down * 0.004, top - down * 0.1, 0.0115, 0.0105, "wood", 12, up=tuple(n0), squash=0.55)   # the grip
    g.tube(top - down * 0.1, top - down * 0.118, 0.0105, 0.009, "steel", 12, up=tuple(n0), squash=0.7)   # the pommel
    g.box(top, (0.006, 0.022, 0.003), "steel", Rs)                                               # the cross guard
    out["bayonet"] = (g.build("bayonet", sharp=50.0), "pelvis")
    # ---- the Y-straps' hardware: chest buckles, the rings at the shoulders' front, the ring at the back
    ystraps = []
    if spec.get("y_straps", True):
        g = Geo()                                  # (the front straps run over each breast pocket's outer third, its button
        hw_ = Geo()                                # clear, from their hooks behind the pouches; the back ends meet at the ring)
        # (the back's points found straight in from behind, not pulled onto the cloth along its field: 5 cm off the back
        # the field's slope ran up toward the nape, and the straps' ends had stopped 3 cm above the ring, the back strap
        # a hand's width below them, bare wool between)
        def on_back(y, z):
            q, nq = on_surface(tunic, (-0.3, y, z), (1.0, 0.0, 0.0))
            return tuple(q + nq * 0.001)
        pr, nr = on_surface(tunic, (-0.3, 0.0, Z(1.245)), (1.0, 0.0, 0.0))
        zr = float(pr[2])
        for side in (1.0, -1.0):
            ctrl = [(x, y * side, Z(z)) for x, y, z in ((0.13, 0.09, 0.998), (0.13, 0.1, 1.16), (0.125, 0.118, 1.27), (0.105, 0.127, 1.36),
                    (0.05, 0.126, 1.445), (-0.02, 0.118, 1.462), (-0.08, 0.104, 1.43))]
            ctrl += [on_back(0.078 * side, Z(1.36)), on_back(0.046 * side, Z(1.302)),
                     on_back(0.017 * side, zr + 0.012)]          # (its end under the ring's upper half)
            P, N, T, E = lay_strap(g, tunic, S.spline(ctrl, 40), 0.032, 0.004, pockets)
            ystraps.append((P, N, T, 0.032, 0.004))
            front = np.nonzero(P[:, 0] > cx)[0]
            i = front[np.argmin(np.abs(P[front, 2] - Z(1.2)))]
            q, nq, tq = on_strap(P, N, T, E, i, 0.004)          # (square on the strap's own face, centred on it)
            buckle(hw_, q + nq * 0.0002, nq, tq, 0.034, 0.03)
            i = front[np.argmin(np.abs(P[front, 2] - Z(1.378)))]
            q, nq, tq = on_strap(P, N, T, E, i, 0.004)
            ring(hw_, q + nq * 0.0015, nq, 0.011, 0.0018, up=tuple(tq))
        # (the back strap held straight down his spine, from the ring's middle to the belt: the cloth's slope had bowed
        # it; laid 1 mm lower than the two from the shoulders, whose ends lie over its top inside the ring)
        lay_strap(g, tunic, S.spline([on_back(0.0, zr + 0.006), on_back(0.0, Z(1.13)), on_back(0.0, Z(1.005))], 14), 0.036, 0.004,
                  keep_y=0.0, gap=0.0015)
        out["straps"] = (g.build("straps"), None)
        ring(hw_, pr + nr * 0.008, nr, 0.016, 0.0024)
        out["strap_metal"] = (hw_.build("strap_metal", sharp=50.0), None)
    # ---- the shoulder straps, over the Y-straps, each with its button by the collar
    if LM_.get("shoulder_straps"):
        g = Geo()
        for c, R, length in LM_["shoulder_straps"]:
            q, nq = shoulder_strap(g, tunic, c, R, length, ystraps)
            dome(g, q - nq * 0.0004, nq, 0.0075, 0.0038, up=tuple(R[:, 0]))
        out["shoulder_straps"] = (g.build("shoulder_straps", sharp=40.0, recalc=False), None)
    print(f"  detail: kit's hardware made in {time.time() - t0:.0f} s")
    return out


S.build_kit = build_kit
RIGID_KIT = ("strap_metal", "details", "belt", "shoulder_straps")


def rigid_islands(ob, most=0.09):
    """Each small separate piece of a kit object (a ring, a buckle, a button) given one set of weights, its points'
    mean, so it moves as one piece and does not bend with the cloth under it (the chest's rings crumpled at port arms)."""
    import bmesh
    me = ob.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.verts.ensure_lookup_table()
    seen, comps = set(), []
    for v0 in bm.verts:
        if v0.index in seen:
            continue
        stack, comp = [v0], []
        seen.add(v0.index)
        while stack:
            u = stack.pop()
            comp.append(u.index)
            for e in u.link_edges:
                w = e.other_vert(u)
                if w.index not in seen:
                    seen.add(w.index)
                    stack.append(w)
        comps.append(comp)
    co_all = np.array([v.co[:] for v in bm.verts])
    boxes = [(co_all[c].min(axis=0) - 0.002, co_all[c].max(axis=0) + 0.002) for c in comps]
    root = list(range(len(comps)))                   # (a ring is made of short tubes, a buckle of boxes: the pieces
                                                     # that touch are taken together)

    def find(i):
        while root[i] != i:
            root[i] = root[root[i]]
            i = root[i]
        return i
    for i in range(len(comps)):
        for j in range(i + 1, len(comps)):
            if np.all(boxes[i][0] <= boxes[j][1]) and np.all(boxes[j][0] <= boxes[i][1]):
                root[find(i)] = find(j)
    groups = {}
    for i, c in enumerate(comps):
        groups.setdefault(find(i), []).extend(c)
    n = 0
    for comp in groups.values():
        co = co_all[comp]
        if np.linalg.norm(co.max(axis=0) - co.min(axis=0)) > most:
            continue
        acc = {}
        for i in comp:
            for gw in me.vertices[i].groups:
                acc[gw.group] = acc.get(gw.group, 0.0) + gw.weight
        top = sorted(acc.items(), key=lambda kv: -kv[1])[:4]
        tot = sum(w for _, w in top) or 1.0
        for i in comp:
            for gi in [gw.group for gw in me.vertices[i].groups]:
                ob.vertex_groups[gi].remove([i])
            for gi, w in top:
                ob.vertex_groups[gi].add([i], w / tot, "REPLACE")
        n += 1
    bm.free()
    print(f"    {ob.name}: {n} small pieces made rigid")


_assemble_rigid = S.assemble


def assemble(nation, *a, **kw):
    m = _assemble_rigid(nation, *a, **kw)
    for ob in m["kit"]:
        if ob.name.split(".")[0] in RIGID_KIT:
            rigid_islands(ob)
    return m


S.assemble = assemble
Geo = S.Geo
S.LOD2_LEFT_OFF = S.LOD2_LEFT_OFF + ("strap_metal", "bayonet")
MID_LEFT_OFF = ("details", "strap_metal")             # (buttons and rings: nothing at 10 m; their triangles go to the shape)
_make_lods = S.make_lods


def lite_pockets(m):
    """The middle level's pockets: made again with few faces (`Pocket.build(lite=True)`), not thinned from the near
    ones (thinned, the flaps and bags melted into soft pads at 10 m, the point and lip lost), each point taking the near
    pockets' weights where it lies."""
    pk = S._DETAIL_LM.get("pocket_objs") or []
    near = next((o for o in m["kit"] if o.name.split(".")[0] == "tunic_pockets"), None)
    if not pk or near is None:
        return None, None
    g = Geo()
    for p in pk:
        p.build(g, lite=True)
    ob = g.build("tunic_pockets_lite", sharp=40.0, recalc=False)
    S.skin_like(ob, near)
    S.attach(ob, m["arm"])
    return near, ob


def make_lods(m, lod):
    if lod != 1:
        return _make_lods(m, lod)
    keep, dec, target = S.man_objects, S._lod_decimate, S.LOD_TARGET[1]
    near, lite = lite_pockets(m)

    def objs(mm):
        out = [o for o in keep(mm) if o.name.split(".")[0] not in MID_LEFT_OFF]
        return [lite if (lite is not None and o is near) else o for o in out]
    S.man_objects = objs
    if lite is not None:                          # (the lite pockets kept whole, the rest thinned to what is left)
        n = S.tris_of(lite)
        S.LOD_TARGET[1] = target - n
        S._lod_decimate = lambda c, r: None if c.name.startswith("tunic_pockets_lite") else dec(c, r)
        print(f"    middle level: the pockets made again, {n} triangles, kept whole")
    try:
        out = _make_lods(m, lod)
    finally:
        S.man_objects, S._lod_decimate, S.LOD_TARGET[1] = keep, dec, target
    if lite is not None:
        for c in out:
            if c.name.startswith("tunic_pockets_lite"):
                c.name = "tunic_pockets_LOD1"
        me = lite.data
        bpy.data.objects.remove(lite)
        bpy.data.meshes.remove(me)
    return out


S.make_lods = make_lods

# the landmarks the kit needs from the uniform (soldier.py passes the kit only the layers and the height map)
_landmarks = S.landmarks


def landmarks(J):
    LM = _landmarks(J)
    S._DETAIL_LM = LM
    return LM


S.landmarks = landmarks


# ================================================================================================ the helmet
def build_helmet(kind, J, sides=None):
    """soldier.py's helmet at 72 sides and twelve rings, its flare in five steps to a rolled rim, with the vent lugs, the
    liner's rivets and the chin strap's buckle (the MakeHuman man only)."""
    k = dict(S.HELMETS[kind])
    sides = 60
    dome_rings = 12
    C0 = np.array((J["skull_x"] - 0.004 + k["dx"], 0.0, J["crown"] - 0.108))
    k["az"] += 0.013
    k["ay"] *= (J["skull_half"][1] + 0.025) / 0.108
    k["ax"] *= (J["skull_half"][0] + 0.023) / 0.122
    th = 0.0045 if not k.get("cloth") else 0.006
    nexp = k.get("n", 2.0)
    outer, inner = [], []
    for i in range(sides):
        f = 2 * math.pi * i / sides
        rho = 1.0 / math.sqrt((math.cos(f) / k["ax"]) ** 2 + (math.sin(f) / k["ay"]) ** 2)
        zE = k["edge"](f)
        a_end = math.asin(max(min(zE / k["az"], 0.999), 0.0) ** (nexp / 2.0)) if zE > 0 else 0.0
        prof = []
        for r in range(1, dome_rings + 1):
            u = r / dome_rings
            a = math.pi / 2 - (math.pi / 2 - a_end) * (u ** 0.85)
            prof.append((rho * math.cos(a) ** (2.0 / nexp), k["az"] * math.sin(a) ** (2.0 / nexp)))
        if zE < 0:
            prof.append((rho, zE * 0.5))
            prof.append((rho, zE))
        else:
            prof.append((prof[-1][0], prof[-1][1] - 5e-5))
            prof.append((prof[-1][0], prof[-1][1] - 5e-5))
        rE, zE2 = prof[-1]
        fl, dr = k["flare"](f), k["drop"](f)
        if kind == "stahl" or not k.get("cloth"):
            for u in (0.12, 0.32, 0.56, 0.8):
                prof.append((rE + fl * u, zE2 - dr * (u ** 0.8)))
            prof.append((rE + fl, zE2 - dr))
            prof.append((rE + fl + 0.0014, zE2 - dr - 0.0022))                  # the rolled rim
            ins = [(rE + fl - 0.0012, zE2 - dr - 0.0034), (rE + fl - th * 0.7, zE2 - dr + th * 0.6), (rE - th, zE2 + th * 0.5)]
        else:
            prof.append((rE + fl * 0.45, zE2 - dr * 0.6))
            prof.append((rE + fl, zE2 - dr))
            ins = [(rE + fl - th * 0.6, zE2 - dr + th * 0.9), (rE - th, zE2 + th * 0.5)]
        for r in (dome_rings, dome_rings * 2 // 3, dome_rings // 3):
            rr, zz = prof[r - 1]
            ins.append((max(rr - th, 0.0), zz - th))
        outer.append(prof)
        inner.append(ins)
    P_, Q_ = len(outer[0]), len(inner[0])

    def ring_of(lst, j):
        out = []
        for i in range(sides):
            f = 2 * math.pi * i / sides
            r, z = lst[i][j]
            out.append(C0 + np.array((math.cos(f) * r, math.sin(f) * r, z)))
        return out

    def on_shell(f_deg, j, lift=0.0):
        """A point on the outside at bearing f (0 front, 90 his left) on profile row j, and the outward normal there."""
        i = int(round(f_deg / 360.0 * sides)) % sides
        f = 2 * math.pi * i / sides
        r, z = outer[i][j]
        r2, z2 = outer[i][j + 1]
        p = C0 + np.array((math.cos(f) * r, math.sin(f) * r, z))
        t = np.array((math.cos(f) * (r2 - r), math.sin(f) * (r2 - r), z2 - z))
        side = np.array((-math.sin(f), math.cos(f), 0.0))
        nrm = _n(np.cross(side, t))
        if nrm @ np.array((math.cos(f), math.sin(f), 0.0)) < 0:
            nrm = -nrm
        return p + nrm * lift, nrm
    g = Geo()
    paint = "helmet"
    rings = [[C0 + np.array((0.0, 0.0, k["az"]))]]
    rings += [ring_of(outer, j) for j in range(P_)]
    rings += [ring_of(inner, j) for j in range(Q_)]
    rings += [[C0 + np.array((0.0, 0.0, k["az"] - th))]]
    g.shell(rings, paint)
    if not k.get("cloth"):
        if kind == "stahl":
            for side in (1.0, -1.0):
                p, nrm = on_shell(90.0 * side + 12.0 * side, dome_rings - 3)          # the vent lug
                dome(g, p, nrm, 0.0062, 0.0042, paint, sides=10)
                g.tube(p + nrm * 0.0036, p + nrm * 0.0046, 0.0026, 0.0024, "steel", 8)
                p, nrm = on_shell(90.0 * side - 25.0 * side, dome_rings)            # a liner rivet
                dome(g, p, nrm, 0.0042, 0.0022, paint, sides=8)
            p, nrm = on_shell(180.0, dome_rings)
            dome(g, p, nrm, 0.0042, 0.0022, paint, sides=8)
        for side in (1.0, -1.0):
            i = sides // 4 if side > 0 else 3 * sides // 4
            r, z = outer[i][-4 if kind == "stahl" else -2]        # (from inside the flare)
            chin, ex = np.array(J["chin"]), J["eye_x"]
            ctrl = [C0 + np.array((0.0, side * (r - 0.012), z + 0.004)), np.array((ex - 0.072, side * 0.066, chin[2] + 0.05)),
                    chin + np.array((-0.045, side * 0.042, -0.003)), chin + np.array((-0.016, 0.0, -0.007))]
            centre = np.array((J["skull_x"], 0.0, chin[2] + 0.07))
            pts = S.spline(ctrl, 16)
            g.cord(pts, [_n(q - centre) for q in pts], 0.011, 0.0035, "straps", sides=8)
            if side > 0:                                                                # the strap's buckle, on his left
                q = pts[5]
                buckle(g, q + _n(q - centre) * 0.0018, _n(q - centre), tuple(_n(pts[6] - pts[4])), 0.014, 0.012, t=0.0012)
    return g.build("helmet_" + kind, sharp=55.0)


S.build_helmet = build_helmet


# ================================================================================================ the rifle
def build_rifle():
    """soldier.py's rifle, at the detail of the vehicles' guns: the stock's finger grooves, sling slot and butt disc; a
    stepped barrel with its crown, the cleaning rod and bayonet lug, a hooded front sight and a leaf rear sight on its
    base, the bands with their swivel, a fuller trigger guard; the bolt with its cocking piece and safety."""
    wood = Layer("stock", "wood")
    wood.add(RoundBox((-0.232, 0.0, -0.026), (0.09, 0.0205, 0.057), 0.016, rot_euler(0, -4, 0)))       # butt
    wood.add(RoundBox((-0.12, 0.0, -0.01), (0.06, 0.0185, 0.034), 0.015), 0.035)                        # into the wrist
    wood.add(RoundCone((-0.09, 0.0, -0.002), (0.035, 0.0, 0.012), 0.0185, 0.0175), 0.02)               # the wrist
    wood.add(Ellipsoid((-0.022, 0.0, -0.028), (0.026, 0.0165, 0.03)), 0.02)                             # half-pistol grip
    wood.add(RoundBox((0.085, 0.0, 0.012), (0.08, 0.02, 0.03), 0.015), 0.02)                             # round the action
    wood.add(RoundBox((0.395, 0.0, 0.026), (0.245, 0.0172, 0.02), 0.0155), 0.03)                        # fore-end, into the cap
    wood.add(RoundBox((-0.035, 0.0, 0.021), (0.035, 0.0115, 0.008), 0.006), 0.012)                       # up under the receiver
    wood.add(RoundCone((0.19, 0.0, 0.0515), (0.604, 0.0, 0.0462), 0.0122, 0.0104), 0.011)               # upper handguard
    # (from under the rear sight's base to under the front band: its rounded ends had left wedges of dark barrel
    # showing, read as cracks in the wood; joined to the fore-end over 11 mm, not 6: its sides overhang the fore-end's
    # rounded top, and the thinning had made the narrow valley under them a black slit by the front band)
    for side in (1.0, -1.0):
        wood.sub(RoundCone((0.2, side * 0.0212, 0.031), (0.37, side * 0.0198, 0.033), 0.0036, 0.0034), 0.0025)   # finger grooves
    for side in (1.0, -1.0):       # the sling slot, a rounded recess each side (cut through, the thinned stock tore
        wood.sub(RoundBox((-0.245, side * 0.0205, -0.05), (0.0055, 0.0075, 0.0165), 0.004, rot_euler(0, -4, 0)), 0.0015)
        #                              round it: a ragged hole that read as a split butt at 1 m)
    rifle = _mesh_from_layers("rifle_stock", [wood], 0.0015, 1100, detail=0.004)
    g = Geo()
    st = "steel"
    bz = 0.042
    g.box((-0.3225, 0.0, -0.026), (0.003, 0.0215, 0.058), st, rot_euler(0, -4, 0))                     # butt plate
    g.tube((-0.226, -0.0195, -0.02), (-0.226, -0.0228, -0.02), 0.0125, 0.0118, st, 18, up=(1.0, 0.0, 0.0))   # the butt's disc
    g.tube((-0.06, 0.0, 0.04), (0.13, 0.0, bz), 0.0165, 0.0158, st, 24)                                 # receiver
    g.tube((0.13, 0.0, bz), (0.17, 0.0, bz), 0.0178, 0.0178, st, 24)                                    # its front ring
    g.tube((0.17, 0.0, bz), (0.4, 0.0, bz), 0.0118, 0.0106, st, 20)                                     # barrel, stepped
    g.tube((0.4, 0.0, bz), (0.408, 0.0, bz), 0.0106, 0.0096, st, 20, caps=False)
    g.tube((0.408, 0.0, bz), (0.776, 0.0, bz), 0.0096, 0.0086, st, 20)
    g.tube((0.776, 0.0, bz), (0.782, 0.0, bz), 0.0092, 0.0084, st, 20)                                  # the crown
    for x0, x1, r, z in ((0.38, 0.394, 0.026, 0.031), (0.6, 0.618, 0.023, 0.034)):
        g.tube((x0, 0.0, z), (x1, 0.0, z), r, r, st, 24, squash=1.1 if x0 < 0.5 else 1.0)              # the bands
    g.tube((0.612, 0.0, 0.0305), (0.648, 0.0, 0.029), 0.0228, 0.0196, st, 20, squash=0.95)            # the nose cap, round the barrel
    for x in (0.387, 0.609):                                                                           # swivels
        loop = [(x + 0.009 * math.cos(t), 0.0, 0.004 - 0.01 * math.sin(t)) for t in np.linspace(0.0, math.pi, 7)]
        for a, b in zip(loop[:-1], loop[1:]):
            g.tube(a, b, 0.0018, 0.0018, st, 6, caps=False)
    g.tube((0.6, 0.0, 0.016), (0.775, 0.0, 0.024), 0.0031, 0.0031, st, 8)                              # cleaning rod
    g.tube((0.775, 0.0, 0.024), (0.782, 0.0, 0.024), 0.0046, 0.0046, st, 10)                           # its head
    g.box((0.66, 0.0, 0.02), (0.024, 0.0042, 0.0045), st)                                              # bayonet lug
    g.tube((0.748, 0.0, 0.05), (0.778, 0.0, 0.05), 0.0108, 0.0108, st, 12)                             # front sight base
    g.box((0.764, 0.0, 0.061), (0.006, 0.0014, 0.007), st, taper=0.6)                                  # its blade
    g.tube((0.75, 0.0, 0.059), (0.776, 0.0, 0.059), 0.0122, 0.0118, st, 20, caps=False)                # its hood
    g.box((0.212, 0.0, 0.0548), (0.03, 0.012, 0.0055), st, taper=0.9)                                  # rear sight base, sat into the wood
    g.box((0.21, 0.0, 0.0625), (0.026, 0.0085, 0.0024), st)                                            # its leaf
    g.box((0.198, 0.0, 0.066), (0.006, 0.011, 0.0035), st)                                             # the slider
    g.box((0.234, 0.0, 0.068), (0.003, 0.0085, 0.006), st)                                             # the notch plate
    g.box((0.085, 0.0, -0.0195), (0.044, 0.015, 0.0035), st)                                           # floorplate
    loop = [(0.0 + 0.026 * math.cos(t), 0.0, -0.016 - 0.018 * math.sin(t)) for t in np.linspace(0.1, math.pi - 0.1, 13)]
    for a, b in zip(loop[:-1], loop[1:]):
        g.tube(a, b, 0.0034, 0.0034, st, 8, caps=False)                                                # trigger guard
    g.tube((0.012, 0.0, -0.012), (0.016, 0.0, -0.02), 0.0024, 0.0022, st, 8)                           # trigger
    g.tube((0.016, 0.0, -0.02), (0.012, 0.0, -0.029), 0.0022, 0.0018, st, 8)
    sl = S.spline([(0.387, 0.0, -0.004), (0.2, 0.0, -0.02), (-0.02, 0.0, -0.068), (-0.2, 0.0, -0.087), (-0.245, 0.0, -0.06)], 26)
    g.band(sl, [(0.0, 1.0, 0.0)] * len(sl), 0.022, 0.003, "straps")                                    # sling
    g.box(tuple(sl[16]), (0.009, 0.0025, 0.013), st, frame_from(_n(sl[17] - sl[15]), (0.0, 1.0, 0.0)))   # its buckle
    g.box(tuple(sl[9]), (0.006, 0.0032, 0.0135), "straps", frame_from(_n(sl[10] - sl[8]), (0.0, 1.0, 0.0)))  # its keeper
    g.absorb(rifle)
    rifle = g.build("rifle", sharp=45.0)
    b = Geo()
    b.tube((-0.105, 0.0, 0.042), (-0.055, 0.0, 0.042), 0.012, 0.0125, st, 20)                          # bolt
    b.tube((-0.124, 0.0, 0.042), (-0.105, 0.0, 0.042), 0.0098, 0.0112, st, 16)                         # cocking piece
    b.box((-0.118, 0.0, 0.056), (0.004, 0.0015, 0.0055), st)                                           # safety flag
    b.tube((-0.05, -0.006, 0.042), (-0.062, -0.052, 0.024), 0.0042, 0.0042, st, 10)                    # handle
    b.tube((-0.06, -0.05, 0.025), (-0.066, -0.064, 0.016), 0.0088, 0.0072, st, 16)                     # knob
    b.tube((-0.066, -0.064, 0.016), (-0.067, -0.0665, 0.0145), 0.0072, 0.004, st, 16)
    bolt = b.build("rifle_bolt", sharp=45.0)
    return rifle, bolt


S.build_rifle = build_rifle


# ================================================================================================ build
def build(fast=False):
    """The German rifleman with the detail pass; his triangles by part and level."""
    kit.fresh(S.FPS)
    t0 = time.time()
    m = S.assemble("GER")
    lods = {0: S.man_objects(m), 1: S.make_lods(m, 1), 2: S.make_lods(m, 2)}
    n = {lod: sum(S.tris_of(o) for o in lods[lod]) for lod in lods}
    parts = {o.name.split(".")[0]: S.tris_of(o) for o in lods[0]}
    print(f"built in {time.time() - t0:.0f} s: {n}")
    print(json.dumps(parts, indent=1))
    m["lods"] = lods
    m["tris"], m["parts"] = n, parts
    os.makedirs(WORK, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(WORK, "pilot.blend"))
    return m


# ================================================================================================ pictures
def load_json_lod(path, lod, name, at=(0.0, 0.0, 0.0)):
    """A soldier file's level as a Blender mesh in the game's colours by paint and the game's own facings."""
    d = json.load(open(path))
    M = d[lod]
    pts = np.array(M["points"], float).reshape(-1, 3) * np.array((1.0, -1.0, 1.0)) + np.array(at)
    fac = np.array(M["facings"], float).reshape(-1, 3) * np.array((1.0, -1.0, 1.0))
    tri = np.array(M["triangles"]).reshape(-1, 3)              # (with y turned, these face along the facings)
    paints = np.array(M["paints"])
    me = bpy.data.meshes.new(name)
    me.from_pydata(pts.tolist(), [], tri.tolist())
    by_index = {v: k for k, v in S.FIGURE_PAINTS.items()}
    used = sorted(set(paints.tolist()))
    for u in used:
        pn = by_index.get(u, "skin" if u in (46, 47) else "tunic")
        me.materials.append(S.paint_material(pn) if pn in S.LOOK_NOW else S.paint_material("tunic"))
    face_paint = paints[tri[:, 0]]
    me.polygons.foreach_set("material_index", np.array([used.index(p) for p in face_paint], np.int32))
    me.polygons.foreach_set("use_smooth", np.ones(len(tri), bool))
    me.update()
    me.normals_split_custom_set_from_vertices([tuple(v) for v in fac])
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def game_smooth(objs):
    """Smooth shading on the new man as the game draws him (its facings per point), no subdivision."""
    for ob in objs:
        if ob.type == "MESH":
            ob.data.polygons.foreach_set("use_smooth", np.ones(len(ob.data.polygons), bool))
            ob.data.update()


def show(objs, on):
    for o in objs:
        o.hide_render = not on
        o.hide_viewport = not on


def pictures(m, fast=False):
    """Today's man (the game's file) and the new one: 1 m (chest and kit), 4 m front and back, 10 m beside the
    Kubelwagen and the Panzer IV at the middle level of detail."""
    from PIL import Image, ImageDraw, ImageFont
    out = os.path.join(WORK, "pictures")
    os.makedirs(out, exist_ok=True)
    w, h = (640, 640) if fast else (1000, 1000)
    samples = 12 if fast else 28
    near0, mid0 = m["lods"][0], m["lods"][1]
    lod2 = m["lods"][2]
    show(lod2, False)
    # (no studio dust on the new man: today's, read from the game's file, has none, and the game draws none; the dust
    # had lit his boots grey-brown beside today's dark ones)
    for ob in near0 + mid0:
        if ob.type == "MESH" and "dust" in ob.data.attributes:
            a = ob.data.attributes["dust"]
            a.data.foreach_set("value", np.zeros(len(a.data), np.float32))
    game_smooth(near0 + mid0)
    old_near = load_json_lod(SHARED + "soldier_GER.json", "near", "old_near")
    old_mid = load_json_lod(SHARED + "soldier_GER.json", "mid", "old_mid")
    clear_studio()
    kit.studio(sun_elev=40, sun_turn=-40, strength=3.4)
    kit.backdrop("B5AC96")
    cam = kit.camera(lens=50)
    S.shared_render(w, h, samples)
    shots = []
    # the men side by side: old on the left (his right), new on the right, 1.4 m apart, both at rest
    old_near.location = old_mid.location = (0.0, 0.7, 0.0)
    m["arm"].location = (0.0, -0.7, 0.0)
    views = [("1 m: chest and belt", (0.0, 0.0, 1.2), 1.0, 8.0, 25.0, 50),
             ("4 m: front", (0.0, 0.0, 0.92), 4.0, 6.0, 20.0, 50),
             ("4 m: back", (0.0, 0.0, 0.92), 4.0, 6.0, 160.0, 50)]
    for title, tgt, dist, pitch, yaw, lens in views:
        for who in ("today", "new"):
            cam.data.lens = lens
            show(near0, who == "new")
            show(mid0, False)
            old_near.hide_render = who != "today"
            old_mid.hide_render = True
            centre = (0.0, 0.7 if who == "today" else -0.7, 0.0)
            t = tuple(np.array(tgt) + np.array(centre))
            kit.orbit(cam, t, dist, pitch, yaw)
            p = os.path.join(out, f"{title.split(':')[0].replace(' ', '')}_{title.split(': ')[1].split()[0]}_{who}.png")
            kit.still(p)
            shots.append((p, f"{title}, {'today' if who == 'today' else 'new'}"))
    # the new man at port arms (his elbows bent: the folds at the crook), front at 4 m and his chest at 1 m, and his
    # chest square on at 0.8 m
    old_near.hide_render = True
    show(near0, True)
    m["arm"].location = (0.0, 0.0, 0.0)
    kit.orbit(cam, (0.0, 0.0, 1.15), 0.8, 2.0, 0.0)
    p = os.path.join(out, "close_square_new.png")
    kit.still(p)
    shots.append((p, "0.8 m square on, new (at rest)"))
    rig = m["rig"]
    P = S.base_stand()
    rig.key(rig.solve(P), 1)
    bpy.context.scene.frame_set(1)
    for title, tgt, dist, pitch, yaw in (("4 m, new at port arms", (0.0, 0.0, 0.92), 4.0, 6.0, 25.0),
                                         ("1 m, new at port arms", (0.0, 0.0, 1.15), 1.0, 8.0, 35.0),
                                         ("4 m, new at port arms, his left", (0.0, 0.0, 0.92), 4.0, 6.0, 95.0)):
        kit.orbit(cam, tgt, dist, pitch, yaw)
        p = os.path.join(out, title.replace(" ", "_").replace(",", "") + ".png")
        kit.still(p)
        shots.append((p, title))
    # 10 m: the vehicles, both men at the middle level
    show(near0, False)
    old_near.hide_render = True
    show(mid0, True)
    old_mid.hide_render = False
    objs = C.load_piece("vehicle", "Kubelwagen", at=(1.2, 3.6, 0.0), yaw=-20.0)
    objs += C.load_piece("vehicle", "PanzerIV", at=(-4.5, -1.5, 0.0), yaw=15.0)
    old_mid.location = (0.0, 1.2, 0.0)
    m["arm"].location = (0.0, 0.0, 0.0)
    cam.data.lens = 35
    kit.orbit(cam, (-1.0, 0.8, 1.0), 10.0, 7.0, 24.0)
    bpy.context.scene.render.resolution_x = w * 2
    p = os.path.join(out, "10m_vehicles.png")
    kit.still(p)
    # the sheet
    ims = [Image.open(q).convert("RGB") for q, _ in shots]
    font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", max(16, h // 34))
    big = Image.open(p).convert("RGB")
    W = w * 2
    rows = (len(ims) + 1) // 2
    sheet = Image.new("RGB", (W, h * rows + big.size[1] + 40), (232, 226, 214))
    for k, (im, (_, lab)) in enumerate(zip(ims, shots)):
        x, y = (k % 2) * w, (k // 2) * h
        sheet.paste(im, (x, y))
        d = ImageDraw.Draw(sheet)
        d.rectangle((x, y, x + w, y + h // 22), fill=(232, 226, 214))
        d.text((x + 10, y + 4), lab, fill=(20, 20, 20), font=font)
    sheet.paste(big, (0, h * rows + 40))
    d = ImageDraw.Draw(sheet)
    d.text((10, h * rows + 8), "10 m, middle detail (what the game draws past about 9 m): today's man nearer the car, the new one "
           "in front of the Panzer IV", fill=(20, 20, 20), font=font)
    path = os.path.join(out, "soldier_detail_pilot.jpg")
    sheet.save(path, quality=90)
    print("sheet", path)
    return path


def clear_studio():
    """Take away an earlier studio's sun, backdrop and camera (a second set over the first rendered the backdrop
    black and lit the men twice)."""
    for ob in list(bpy.data.objects):
        if ob.type in ("LIGHT", "CAMERA") or ob.name.split(".")[0] == "backdrop":
            bpy.data.objects.remove(ob)


def chest_check(m):
    """A quick look while working: the new man's chest at 1 m and square on, 640 px (pictures/check_*.png)."""
    out = os.path.join(WORK, "pictures")
    os.makedirs(out, exist_ok=True)
    near0 = m["lods"][0]
    show(m["lods"][1], False)
    show(m["lods"][2], False)
    game_smooth(near0)
    clear_studio()
    kit.studio(sun_elev=40, sun_turn=-40, strength=3.4)
    kit.backdrop("B5AC96")
    cam = kit.camera(lens=50)
    S.shared_render(640, 640, 12)
    paths = []
    for name, tgt, dist, pitch, yaw in (("check_chest", (0.0, 0.0, 1.2), 1.0, 8.0, 25.0),
                                        ("check_square", (0.0, 0.0, 1.15), 0.8, 2.0, 0.0),
                                        ("check_side", (0.0, 0.0, 1.1), 0.9, 4.0, 70.0)):
        kit.orbit(cam, tgt, dist, pitch, yaw)
        paths.append(os.path.join(out, name + ".png"))
        kit.still(paths[-1])
    print("check", paths)
    return paths


def main():
    a = sys.argv
    fast = "--fast" in a
    steps = a[a.index("--only") + 1].split(",") if "--only" in a else ["build", "pictures"]
    m = build(fast)
    os.makedirs(WORK, exist_ok=True)
    with open(os.path.join(WORK, "tris.json"), "w") as f:
        json.dump({"tris": m["tris"], "parts": m["parts"]}, f, indent=1)
    if "chest" in steps:
        chest_check(m)
    if "pictures" in steps:
        pictures(m, fast)


if __name__ == "__main__":
    main()
