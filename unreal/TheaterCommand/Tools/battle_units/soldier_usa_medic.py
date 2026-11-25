"""The American company aid man (the medic) of the Normandy battles, summer 1944, made on soldier_usa.py's rifleman
and soldier.py (both stay as they are).

    python soldier_usa_medic.py                     build, rig, animate and export him, then his picture
    python soldier_usa_medic.py --only export       (steps: export, picture; --fast for a small test picture;
                                                     --keep: stage only, nothing copied to the shared folder)

Run it beside soldier_usa.py (a checkout of claude/project-thread-8egity's Tools/battle_units, with soldier.py,
human.py, mocap.py, kit.py and MakeHuman's base files), with the bpy module (Blender 5.0.1).

He is soldier_usa.py's man in body, uniform, skeleton (the same 32 bones), sockets and clip names, with his own face
(MakeHuman's mix 140) and these differences, laid on by wrapping the makers before `assemble` calls them:

- unarmed: no rifle mesh. The weapon bone stays (and its bolt and bipod bones); in every clip it rides his right hand
  where a rifle's grip would lie, so a prop could hang on socket_weapon;
- his kit: the pistol belt without cartridge pockets, two medical pouches at the front of the hips (on the hinged
  pouch bones) hung from the medical yoke (soldier.py's Y-straps, over his shoulders to the belt), the canteen at the
  right hip and the first-aid pouch at the left. No haversack and no entrenching tool: an aid man's load was his two
  pouches (the yoke took their weight), and his back is left clear to carry and drag a man;
- no red cross or crescent anywhere (the project's rule, the art director 23 November 2026: the emblem is
  protected; RED_CROSS below stays False): he is known by a plain white brassard round the left upper arm, skinned
  with the sleeve, and by his kit and empty hands. The helmet's four discs and crosses and the brassard's cross are
  still made (in their own paints, so the levels of detail find them) and then taken off by plain_marks, relief and
  all: a plain shell and a plain white band;
- his clips: the motion capture plays with the capture's own arms (no rifle carried, none dropped; go_prone's hands
  kept off the ground, and its end up on his elbows as fire_prone begins); every keyed clip that held a rifle is keyed
  again with free hands (watching instead of firing, a signal instead of a grenade; fire_prone and lmg_burst up on
  his elbows, the forearms flat); and his own clip, `tend`: on his knees by a man lying across before him, pressing a
  dressing;
- his eyeballs' faces in one order (MakeHuman's is not the same from run to run): the one the delivered file was made
  with, so the .json's points keep their numbers.

Files go to battle-models/land/soldier/: soldier_USA_medic.glb, _LOD1.glb, _LOD2.glb and .json, and
pictures/soldier_USA_medic.png.
"""

import json
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import soldier_usa as U  # noqa: E402  (imports soldier.py and lays the rifleman on it)
S = U.S
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402
import kit  # noqa: E402
from sculpt import Layer, RoundBox, frame_from  # noqa: E402

S.SCRATCH = "/home/user/art/medic/"
S.STAGE = os.path.join(S.SCRATCH, "out")

NATION = "USA"
BASE = "soldier_USA_medic"
SEED = 140                 # (his face: MakeHuman's mix 140; the rifleman is 85, the squad leader 192)
ON = True                  # (False while the rifleman is made, as the casualty in the picture)

# ---- his own paints: the red cross's white and red (the game colours by number; the json carries the colours)
MARKS = ("medic_white", "medic_red")
S.FIGURE_PAINTS.update({"medic_white": 15, "medic_red": 16})
S.LOOK[NATION].update({"medic_white": "E4E1D3", "medic_red": "A3221C"})
HELMET_PAINTS = [S.FIGURE_PAINTS[p] for p in ("helmet",) + MARKS]
RED_CROSS = False          # (the project's rule: no red cross or crescent anywhere. False: the helmet's discs and
                           # crosses and the brassard's cross are taken off; see plain_marks)


def plain_marks(objs):
    """With RED_CROSS False: the helmet's discs and crosses and the brassard's cross taken off altogether (their faces
    and the points only they used), so neither paint nor relief leaves a cross: the helmet is the plain shell, the
    brassard a plain white band. Done on finished objects, after the levels of detail are made (make_lods finds the
    marks by their paints). Returns the faces taken off."""
    if RED_CROSS:
        return 0
    import bmesh
    n = 0
    for ob in objs:
        if ob.type != "MESH":
            continue
        base = ob.name.split(".")[0]
        if base.startswith("helmet"):
            which = MARKS
        elif base.startswith("brassard"):
            which = ("medic_red",)
        else:
            continue
        names = [sl.material.name.split(".")[0] if sl.material else "" for sl in ob.material_slots]
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        gone = [f for f in bm.faces if names[f.material_index] in which]
        if gone:
            bmesh.ops.delete(bm, geom=gone, context="FACES")
            bm.to_mesh(ob.data)
            ob.data.update()
        bm.free()
        n += len(gone)
    return n


def _n(v):
    v = np.asarray(v, float)
    return v / np.linalg.norm(v)


def _medic(m_or_rig):
    return bool(m_or_rig.get("medic")) if isinstance(m_or_rig, dict) else bool(getattr(m_or_rig, "medic", False))


# ================================================================================================ slabs on a surface
def lattice(mapping, cells, top, bot, walls, period=None, split=False):
    """A slab over cells (i, j) of a lattice laid on a surface: mapping(i, j, off) -> the point `off` off the surface.
    The top face at `top`; with walls, its open edges taken down to `bot`. Faces wind outward (counter-clockwise seen
    from outside, as the lattice's i, j and the surface's outward facing make a right-handed frame). With split, each
    top cell is two triangles parted along its (i, j)-(i+1, j+1) diagonal, so a mark laid on it can follow it."""
    key = (lambda i, j: (i % period, j)) if period else (lambda i, j: (i, j))
    idx, V, F, edges = {}, [], [], {}

    def vert(i, j, off, tag):
        k = (key(i, j), tag)
        if k not in idx:
            idx[k] = len(V)
            V.append(mapping(*key(i, j), off))
        return idx[k]
    for (i, j) in sorted(cells):
        cs = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
        t = [vert(a, b, top, "t") for a, b in cs]
        F.extend([(t[0], t[1], t[2]), (t[0], t[2], t[3])] if split else [tuple(t)])
        for A, B in zip(cs, cs[1:] + cs[:1]):
            edges[(key(*A), key(*B))] = (A, B)
    if walls:
        for (kA, kB), (A, B) in edges.items():
            if (kB, kA) not in edges:
                F.append((vert(*A, bot, "b"), vert(*B, bot, "b"), vert(*B, top, "t"), vert(*A, top, "t")))
    return V, F


def disc(mapping, R, segs, rings, top, bot, walls):
    """A round slab: mapping(x, y, off) on a surface with (x, y, outward) right-handed; centre fan and rings."""
    V = [mapping(0.0, 0.0, top)]
    F = []
    ring = lambda ri, k: 1 + (ri - 1) * segs + k % segs
    for ri in range(1, rings + 1):
        r = R * ri / rings
        for k in range(segs):
            a = 2.0 * math.pi * k / segs
            V.append(mapping(r * math.cos(a), r * math.sin(a), top))
    for k in range(segs):
        F.append((0, ring(1, k), ring(1, k + 1)))
    for ri in range(2, rings + 1):
        for k in range(segs):
            F.append((ring(ri - 1, k), ring(ri, k), ring(ri, k + 1), ring(ri - 1, k + 1)))
    if walls:
        b0 = len(V)
        for k in range(segs):
            a = 2.0 * math.pi * k / segs
            V.append(mapping(R * math.cos(a), R * math.sin(a), bot))
        for k in range(segs):
            F.append((b0 + k, b0 + (k + 1) % segs, ring(rings, k + 1), ring(rings, k)))
    return V, F


def plus_cells(sub):
    """The cells of a Greek cross of five equal squares, each square cut sub x sub, on a lattice of 3 sub x 3 sub
    cells: point (i, j) lies at ((i / sub - 1.5), (j / sub - 1.5)) arm widths from the cross's centre."""
    out = set()
    for ci in range(3 * sub):
        for cj in range(3 * sub):
            x, y = (ci + 0.5) / sub - 1.5, (cj + 0.5) / sub - 1.5        # (the cell's middle, in arm widths)
            if (abs(x) < 1.5 and abs(y) < 0.5) or (abs(x) < 0.5 and abs(y) < 1.5):
                out.add((ci, cj))
    return out


# ---- how fine each level of detail makes them: the helmet's disc (segments, rings, walls, top, bottom), its cross
# (cuts per square, walls, top, bottom), and the brassard (sides, axial cuts, walls, lift, cross cuts or 0)
MARK_LOD = {
    0: dict(segs=20, rings=2, disc_walls=True, disc_top=0.0013, disc_bot=-0.0008, sub=2, cross_walls=True,
            cross_top=0.0021, cross_bot=0.0010),
    1: dict(segs=12, rings=1, disc_walls=False, disc_top=0.0025, disc_bot=0.0, sub=1, cross_walls=False,
            cross_top=0.0040, cross_bot=0.0),
    2: dict(segs=8, rings=1, disc_walls=False, disc_top=0.0030, disc_bot=0.0, sub=1, cross_walls=False,
            cross_top=0.0050, cross_bot=0.0),
}
BRASSARD_LOD = {
    0: dict(sides=28, cuts=3, walls=True, lift=0.0026, cross_sub=2, cross_up=0.0011),
    1: dict(sides=12, cuts=1, walls=False, lift=0.0035, cross_sub=1, cross_up=0.0016),
    2: dict(sides=10, cuts=1, walls=False, lift=0.0045, cross_sub=0, cross_up=0.0),
}


# ================================================================================================ the helmet's crosses
MARK_R = 0.042             # the white disc's radius (m): about 8.4 cm across, as painted on the M1's four sides
CROSS_W = 0.021            # the red cross's arm width (m): five equal squares, 6.3 cm over its arms
MARK_MARGIN = 0.008        # (m) the disc's lowest point kept this far above the shell's edge, clear of the rim
MARK_LEAST = {0: 34.0, 90: 22.0, 180: 22.0, 270: 22.0}   # (degrees up from the shell's middle, at the least)
SHELL = {}                 # the plain shell the marks are laid on (kept for the levels of detail)


class Shell:
    """The M1's outside as build_helmet made it ("pot"): its middle, its half-axes, a BVH of the mesh."""

    def __init__(self, ob, J):
        k = S.HELMETS["pot"]
        self.C = np.array((J["skull_x"] - 0.004 + k["dx"], 0.0, J["crown"] - 0.108))
        self.az = k["az"] + 0.013
        self.ay = k["ay"] * (J["skull_half"][1] + 0.025) / 0.108
        self.ax = k["ax"] * (J["skull_half"][0] + 0.023) / 0.122
        self.edge = k["edge"]
        me = ob.data
        Mw = ob.matrix_world
        self.V = [tuple(Mw @ v.co) for v in me.vertices]
        self.F = [tuple(p.vertices) for p in me.polygons]
        self.bvh = BVHTree.FromPolygons(self.V, self.F)
        top = max(v[2] for v in self.V)
        self.top_err = top - (self.C[2] + self.az)            # (should be about 0: the formula is build_helmet's)

    def normal(self, p):
        d = np.asarray(p, float) - self.C
        return _n((d[0] / self.ax ** 2, d[1] / self.ay ** 2, max(d[2], 0.0) / self.az ** 2))

    def onto(self, q, n0, off):
        hit, _, _, _ = self.bvh.ray_cast(Vector(np.asarray(q) + n0 * 0.06), Vector(-n0), 0.2)
        if hit is None:
            raise RuntimeError("a mark's point missed the helmet")
        h = np.array(hit)
        return h + self.normal(h) * off

    def frame_at(self, bearing):
        """The mark's centre on the shell at a bearing (0 ahead, 90 his left), as low as keeps its disc clear of the
        rim, and its frame (centre, outward, across, up the shell)."""
        f = math.radians(bearing)
        rim = self.C[2] + self.edge(f) + MARK_MARGIN
        for e in np.arange(MARK_LEAST[bearing], 75.0, 0.5):
            el = math.radians(e)
            d = np.array((math.cos(f) * math.cos(el), math.sin(f) * math.cos(el), math.sin(el)))
            hit, _, _, _ = self.bvh.ray_cast(Vector(self.C + d * 0.4), Vector(-d), 0.5)
            if hit is None:
                continue
            c0 = np.array(hit)
            n0 = self.normal(c0)
            t1 = _n(np.cross((0.0, 0.0, 1.0), n0))
            t2 = np.cross(n0, t1)
            low = min(self.onto(c0 + MARK_R * (math.cos(a) * t1 + math.sin(a) * t2), n0, 0.0)[2]
                      for a in np.linspace(0.0, 2 * math.pi, 36, endpoint=False))
            if low >= rim:
                return c0, n0, t1, t2, float(e)
        raise RuntimeError(f"no room for a mark at bearing {bearing}")


def helmet_marks(lod):
    """The four red crosses on white discs, at one level of detail, as one mesh in the rest pose (paints medic_white
    and medic_red), laid on the plain shell kept in SHELL."""
    sh, frames = SHELL["shell"], SHELL["frames"]
    q = MARK_LOD[lod]
    g = S.Geo()
    for c0, n0, t1, t2, _ in frames:
        on = lambda x, y, off, c0=c0, n0=n0, t1=t1, t2=t2: sh.onto(c0 + x * t1 + y * t2, n0, off)
        V, F = disc(on, MARK_R, q["segs"], q["rings"], q["disc_top"], q["disc_bot"], q["disc_walls"])
        g._add(V, F, "medic_white")
        k = q["sub"]
        onc = lambda i, j, off, on=on, k=k: on((i / k - 1.5) * CROSS_W, (j / k - 1.5) * CROSS_W, off)
        V, F = lattice(onc, plus_cells(q["sub"]), q["cross_top"], q["cross_bot"], q["cross_walls"])
        g._add(V, F, "medic_red")
    return g.build(f"medic_marks_{lod}", sharp=55.0, recalc=False)


_build_helmet = S.build_helmet


def build_helmet(kind, J, sides=None):
    ob = _build_helmet(kind, J, sides)
    if not (ON and kind == "pot" and "crown" in J):
        return ob
    sh = Shell(ob, J)
    SHELL.clear()
    SHELL.update(shell=sh, frames=[sh.frame_at(b) for b in (0, 90, 180, 270)])
    print(f"  medic: the helmet's crosses (shell's top off the formula {1000 * sh.top_err:.1f} mm); centres "
          + ", ".join(f"{b}: {e:.1f} deg up" for b, (*_, e) in zip((0, 90, 180, 270), SHELL["frames"])))
    marks = helmet_marks(0)
    return S.join_meshes([ob, marks], ob.name.split(".")[0])


S.build_helmet = build_helmet


# ================================================================================================ the kit
MED_POUCH_HALF = (0.023, 0.048, 0.06)      # each medical pouch (half: deep, wide, tall): about 12 x 10 x 5 cm
MED_POUCH_AT = 0.11                        # its middle round the waist from the buckle (m along the cloth)
HANG_CLEAR = 0.012                         # (m) the canteen and the first-aid pouch swung out at the foot to clear his
                                           # hips by this much (the field's distance), at most HANG_MOST degrees
HANG_MOST = 12.0
BRASSARD_AT = 0.58                         # the brassard's middle, of the way from the shoulder's joint to the elbow
BRASSARD_W = 0.085                         # its width down the arm (m)
BRASSARD = {}                              # the sleeve's shape under it (kept for the levels of detail)
_usa_kit = S.build_kit


def sleeve_shape(tunic, J):
    """The left sleeve's radius round the upper arm's axis, sampled over the brassard's width: (s, phi) -> r."""
    a, b = np.array(J["upperarm_l"], float), np.array(J["lowerarm_l"], float)
    u = _n(b - a)
    L = float(np.linalg.norm(b - a))
    up = np.array((0.0, 0.0, 1.0))
    o = _n(up - u * (u @ up))                   # (the arm's outer side: up and out, with the arm held out at rest)
    w = np.cross(u, o)
    c = a + u * L * BRASSARD_AT
    Ss = np.linspace(-BRASSARD_W / 2 - 0.006, BRASSARD_W / 2 + 0.006, 9)
    PH = np.linspace(0.0, 2 * math.pi, 120, endpoint=False)
    rho = np.arange(0.012, 0.14, 0.0015)
    D = np.cos(PH)[:, None] * o + np.sin(PH)[:, None] * w
    pts = c[None, None, None, :] + Ss[:, None, None, None] * u + D[None, :, None, :] * rho[None, None, :, None]
    F = tunic.at_points(pts.reshape(-1, 3)).reshape(len(Ss), len(PH), len(rho))
    if (F[:, :, 0] >= 0).any():
        raise RuntimeError("the arm's axis is not inside the sleeve")
    k = np.argmax(F > 0, axis=2)
    found = (F > 0).any(axis=2)
    k = np.maximum(k, 1)
    lo, hi = rho[k - 1], rho[k]
    for _ in range(12):                          # (each first way out of the cloth, halved down)
        mid = (lo + hi) / 2
        p = c[None, None, :] + Ss[:, None, None] * u + D[None, :, :] * mid[:, :, None]
        inside = tunic.at_points(p.reshape(-1, 3)).reshape(mid.shape) < 0
        lo, hi = np.where(inside, mid, lo), np.where(inside, hi, mid)
    r = (lo + hi) / 2
    # (toward his side the sleeve can run into the jacket's body, where no way out is found or it is found far off:
    # there the brassard goes round the arm as the rest of the sleeve does, the radius carried across from either side)
    med = np.nanmedian(np.where(found, r, np.nan), axis=1, keepdims=True)
    good = found & (r < 1.35 * med)
    bad = int((~good).sum())
    for i in range(len(Ss)):
        idx = np.where(good[i])[0]
        x = np.concatenate([idx - len(PH), idx, idx + len(PH)])
        r[i] = np.interp(np.arange(len(PH)), x, np.tile(r[i, idx], 3))
    r = np.maximum(r, (np.roll(r, 1, 1) + 2 * r + np.roll(r, -1, 1)) / 4)      # (a fold's crease bridged, never cut)
    print(f"  medic: the sleeve under the brassard: {bad} of {r.size} ways carried across from their neighbours")
    return dict(c=c, u=u, o=o, w=w, S=Ss, PH=PH, r=r, L=L)


def sleeve_r(B, s, phi):
    """The sleeve's radius at s along the arm (from the brassard's middle) and phi round it (0 the outer side)."""
    Ss, r = B["S"], B["r"]
    n = len(B["PH"])
    x = (phi % (2 * math.pi)) / (2 * math.pi) * n
    j0 = int(math.floor(x)) % n
    fj = x - math.floor(x)
    i = int(np.clip(np.searchsorted(Ss, s) - 1, 0, len(Ss) - 2))
    fi = float(np.clip((s - Ss[i]) / (Ss[i + 1] - Ss[i]), 0.0, 1.0))
    row = lambda ii: r[ii, j0] * (1 - fj) + r[ii, (j0 + 1) % n] * fj
    return float(row(i) * (1 - fi) + row(i + 1) * fi)


BRASSARD_TURN = -30.0      # (degrees round the arm from its outer side at rest to the cross's middle: a hanging arm
                           # turns in, and the cross comes to face out from his side)


def band_nodes(q):
    """The band's points' radii, (cuts + 1) x sides: each `lift` off the highest of the sleeve over its own stretch."""
    B, N, cuts = BRASSARD, q["sides"], q["cuts"]
    ds = BRASSARD_W / cuts
    return np.array([[max(sleeve_r(B, -BRASSARD_W / 2 + ds * j + e, 2 * math.pi * i / N) for e in np.linspace(-ds / 2, ds / 2, 5))
                      for i in range(N)] for j in range(cuts + 1)]) + q["lift"]


def band_place(q, phi, s):
    """Where (phi round the arm, s along it) falls on the band's lattice: its cell's corners (i, j) and their shares,
    on the cell's triangle as lattice(split) makes it."""
    N, cuts = q["sides"], q["cuts"]
    ds = BRASSARD_W / cuts
    x = (phi % (2 * math.pi)) / (2 * math.pi) * N
    i0, fi = int(math.floor(x)) % N, x - math.floor(x)
    y = float(np.clip((s + BRASSARD_W / 2) / ds, 0.0, cuts - 1e-9))
    j0, fj = int(math.floor(y)), y - math.floor(y)
    i1 = (i0 + 1) % N
    if fi >= fj:
        return [((i0, j0), 1.0 - fi), ((i1, j0), fi - fj), ((i1, j0 + 1), fj)]
    return [((i0, j0), 1.0 - fj), ((i0, j0 + 1), fj - fi), ((i1, j0 + 1), fi)]


def brassard_mesh(lod):
    """The white brassard round the left upper arm and its red cross on the outer side, at one level of detail. The
    band's points stand `lift` off the highest of the sleeve near them; the cross follows the band's own triangles."""
    B, q = BRASSARD, BRASSARD_LOD[lod]
    c, u, o, w = B["c"], B["u"], B["o"], B["w"]
    N, cuts = q["sides"], q["cuts"]
    ds = BRASSARD_W / cuts
    node = band_nodes(q)

    def at(phi, s, rad):
        return c + u * s + (math.cos(phi) * o + math.sin(phi) * w) * rad

    def band_r(phi, s):                           # (on the band's own triangles)
        return float(sum(node[j, i] * f for (i, j), f in band_place(q, phi, s)))
    g = S.Geo()
    band = lambda i, j, off: at(2 * math.pi * i / N, -BRASSARD_W / 2 + ds * j, node[j, i % N] - q["lift"] + off)
    V, F = lattice(band, {(i, j) for i in range(N) for j in range(cuts)}, q["lift"], -0.001, q["walls"], period=N, split=True)
    g._add(V, F, "medic_white")
    if q["cross_sub"]:
        f0 = math.radians(BRASSARD_TURN)
        r0 = band_r(f0, 0.0)
        k, wc = q["cross_sub"], 0.019             # (the arm width: 5.7 cm over the arms, on a 8.5 cm band)

        def cross(i, j, off):
            phi, s = f0 + (i / k - 1.5) * wc / r0, (j / k - 1.5) * wc
            return at(phi, s, max(band_r(phi, s), sleeve_r(B, s, phi) + q["lift"]) + off)
        V, F = lattice(cross, plus_cells(k), q["cross_up"], 0.0, False)
        g._add(V, F, "medic_red")
    return g.build("brassard" if lod == 0 else f"brassard_LOD{lod}", sharp=55.0, recalc=False)


def hang_clear(ob, layers, centre, name):
    """Swing a piece hung from the belt out at its foot, about a level line through its top, the least that leaves
    HANG_CLEAR between it and the cloth under it (at most HANG_MOST degrees)."""
    me = ob.data
    Mw = np.array(ob.matrix_world)
    V = np.array([tuple(v.co) for v in me.vertices]) @ Mw[:3, :3].T + Mw[:3, 3]
    gap = lambda P: float(np.min([L.at_points(P) for L in layers]))
    top = V[V[:, 2] > V[:, 2].max() - 0.01].mean(0)
    n = V.mean(0) - centre
    n[2] = 0.0
    n = _n(n)
    k = np.cross(n, (0.0, 0.0, 1.0))
    g0, best = gap(V), None
    for a in np.arange(0.0, HANG_MOST + 0.01, 0.5):
        R = S.R_axis(k, a)
        W = (V - top) @ R.T + top
        if gap(W) >= HANG_CLEAR or a >= HANG_MOST:
            best = (a, W, gap(W))
            break
    a, W, g1 = best
    Wl = (W - Mw[:3, 3]) @ np.linalg.inv(Mw[:3, :3]).T
    for v, co in zip(me.vertices, Wl):
        v.co = tuple(co)
    me.update()
    print(f"  medic: {name} swung out {a:.1f} deg at its foot (its least gap to the cloth {100 * g0:.1f} -> {100 * g1:.1f} cm)")


def build_kit(J, spec, body_ls, Z=None):
    out = _usa_kit(J, spec, body_ls, Z)
    if not (ON and U._ours(spec)):
        return out
    Z = Z or (lambda z: z)
    tunic = body_ls[0]
    for k in ("pouches", "belt_pockets", "pack", "shovel"):        # (no cartridges, no haversack, no shovel)
        if k in out:
            bpy.data.objects.remove(out.pop(k)[0])
    zb = Z(1.0)
    cx = J["spine_01"][0] + 0.03 if "crown" in J else 0.0
    rings, _ = U._taut({side: U._waist_ring(tunic, cx, zb, side) for side in (1.0, -1.0)}, cx)
    # ---- the two medical pouches, hooked to the belt at the front of each hip (the yoke's straps come down to them):
    # canvas, a flap over the top closed by two snaps
    top = zb + U.BELT_W / 2 + 0.004
    hx, hy, hz = MED_POUCH_HALF
    for k_ in [k_ for k_ in J if k_.startswith("pouch_pivot_")]:
        del J[k_]
    layers, g = [], S.Geo()
    for side in (1.0, -1.0):
        Pr, Nr, sr = rings[side]
        p0, n0 = U._at_length(Pr, Nr, sr, MED_POUCH_AT)
        R = frame_from(n0, (0.0, 0.0, 1.0))
        c = np.array((p0[0], p0[1], top - hz)) + n0 * (0.011 + hx)
        L = Layer("med_pouch", "pack")
        L.add(RoundBox(c, (hx, hy, hz), 0.009, R), 0.0)
        L.add(RoundBox(c + R @ np.array((0.004, 0.0, hz - 0.024)), (hx + 0.002, hy + 0.0025, 0.026), 0.007, R), 0.002)
        layers.append(L)
        for dy in (-0.027, 0.027):
            p = c + R @ np.array((hx + 0.0055, dy, hz - 0.04))
            g.tube(p - n0 * 0.002, p + n0 * 0.0022, 0.0042, 0.0036, "steel", 8)
        J["pouch_pivot_" + ("l" if side > 0 else "r")] = tuple(float(v) for v in np.array((p0[0], p0[1], top - 0.012)) + n0 * 0.012)
    J["waist_centre"] = (float(cx), 0.0, float(zb))
    g.absorb(S.mesh_from_layers("med_pouches", layers, 0.002, 520))
    out["pouches"] = (g.build("pouches"), None)
    # ---- the canteen and the first-aid pouch hang from the belt over his hips: swung out at the foot to clear them
    for k in ("bottle", "aid_pouch"):
        if k in out:
            hang_clear(out[k][0], body_ls, np.array((cx, 0.0, zb)), k)
    # ---- the brassard on the left upper arm
    BRASSARD.clear()
    BRASSARD.update(sleeve_shape(tunic, J))
    out["brassard"] = (brassard_mesh(0), None)
    print(f"  medic: pouches at {MED_POUCH_AT} m round the waist, pivots {J['pouch_pivot_l']} {J['pouch_pivot_r']}; "
          f"brassard over a sleeve {100 * BRASSARD['r'].min():.1f}-{100 * BRASSARD['r'].max():.1f} cm round its axis")
    return out


S.build_kit = build_kit
_skin_like = S.skin_like


def skin_like(piece, body, k=4):
    """The brassard takes the weights of the sleeve's own points under it (the body's points carried mostly by the
    left upper arm or forearm), never the jacket's side by his armpit; every other piece as soldier.py skins it."""
    if not piece.name.startswith("brassard"):
        return _skin_like(piece, body, k)
    from mathutils import kdtree
    W, names = S.weights_of(body)
    arm = [names.index(b) for b in ("upperarm_l", "lowerarm_l") if b in names]
    own = [i for i in range(len(W)) if int(np.argmax(W[i])) in arm]
    kd = kdtree.KDTree(len(own))
    for j, i in enumerate(own):
        kd.insert(body.matrix_world @ body.data.vertices[i].co, j)
    kd.balance()
    # (the sleeve's weights taken at each of the band's lattice points, on the cloth under it, over the four bones that
    # carry the band most; every point of the piece then takes them as it lies on the lattice, so the band's walls and
    # the cross upon it move with the band itself and never sink into it nor stand off it)
    lod = int(piece.name.split("LOD")[1][0]) if "LOD" in piece.name else 0
    q, B = BRASSARD_LOD[lod], BRASSARD
    N, cuts = q["sides"], q["cuts"]
    ds = BRASSARD_W / cuts
    node = band_nodes(q)
    Wn = np.zeros((cuts + 1, N, len(names)))
    for j in range(cuts + 1):
        for i in range(N):
            phi, sj = 2 * math.pi * i / N, -BRASSARD_W / 2 + ds * j
            p = B["c"] + B["u"] * sj + (math.cos(phi) * B["o"] + math.sin(phi) * B["w"]) * (node[j, i] - q["lift"])
            tot = 0.0
            for co, jj, d in kd.find_n(Vector(p), k):
                w = 1.0 / max(d, 1e-3)
                Wn[j, i] += W[own[jj]] * w
                tot += w
            Wn[j, i] /= tot
    main = np.argsort(-Wn.sum(axis=(0, 1)))[:4]
    keep = np.zeros(len(names), bool)
    keep[main] = True
    Wn[..., ~keep] = 0.0
    Wn[Wn < 0.03] = 0.0
    Wn /= Wn.sum(axis=2, keepdims=True)
    Wp = np.zeros((len(piece.data.vertices), len(names)))
    Mi = piece.matrix_world
    for v in piece.data.vertices:
        d = np.array(Mi @ v.co) - B["c"]
        sv = float(d @ B["u"])
        phi = math.atan2(float(d @ B["w"]), float(d @ B["o"]))
        for (i, j), f in band_place(q, phi, sv):
            Wp[v.index] += Wn[j, i] * f
    S.set_weights(piece, Wp, names, 4, 0.0)
    print(f"  medic: {piece.name} skinned on {[names[b] for b in main]} as its lattice lies")


S.skin_like = skin_like


# ================================================================================================ his eyes
# MakeHuman's eyeballs come with their faces in an order that is not the same from one run to the next (in blocks of
# six, each turned round by its own step); the eyes' points and triangles in the .json follow it, and the levels of
# detail are thinned from it, and variety.py's faces move the eyes' points by their numbers. So each eyeball's faces
# are put in one order, the one the delivered soldier_USA_medic was made with: each face's place in it, the faces
# numbered as their corners (in their own order) sort.
EYE_ORDER = {
    "l": [9, 58, 56, 59, 57, 55, 11, 14, 12, 10, 0, 13, 19, 17, 15, 1, 18, 16, 24, 22, 20, 2, 23, 21, 29, 27, 25, 3, 28, 26,
          34, 32, 30, 4, 33, 31, 37, 35, 5, 38, 36, 39, 40, 6, 43, 41, 44, 42, 7, 48, 46, 49, 47, 45, 8, 53, 51, 54, 52, 50],
    "r": [55, 9, 58, 56, 59, 57, 0, 13, 11, 14, 12, 10, 18, 16, 19, 17, 15, 1, 21, 24, 22, 20, 2, 23, 29, 27, 25, 3, 28, 26,
          34, 32, 30, 4, 33, 31, 39, 37, 35, 5, 38, 36, 44, 42, 40, 6, 43, 41, 47, 45, 7, 48, 46, 49, 50, 8, 53, 51, 54, 52],
}


def order_eyes(man):
    """Each of his eyeballs' faces put in EYE_ORDER (its points as they are)."""
    import bmesh
    for e in man["eyes"]:
        order = EYE_ORDER[e.name[-1]]
        bm = bmesh.new()
        bm.from_mesh(e.data)
        corners = lambda f: tuple(v.index for v in f.verts)
        canon = sorted(corners(f) for f in bm.faces)
        if len(canon) != len(order):
            raise RuntimeError(f"{e.name}: {len(canon)} faces, EYE_ORDER has {len(order)}")
        place = {canon[k]: n for n, k in enumerate(order)}
        bm.faces.sort(key=lambda f: place[corners(f)])
        bm.to_mesh(e.data)
        bm.free()
        e.data.update()


_mh_body = S.mh_body


HAND_SKIN = {}             # (filled as his man is made: see hand_skin)


def hand_skin(man):
    """Each hand bone's skin (hand, fingers, thumb: MakeHuman's points most weighted to it) as its points furthest out
    each of 26 ways, offsets from the bone's head in the rest pose: go_prone's ground pass keeps them over the ground
    (they follow the posed skin within a few millimetres; the bones' own points miss a hand caught on its edge)."""
    body = man["body"]
    names = {g.index: g.name for g in body.vertex_groups}
    Mw = np.array(body.matrix_world)
    V = np.array([tuple(v.co) for v in body.data.vertices]) @ Mw[:3, :3].T + Mw[:3, 3]
    dom = np.array([max(((g.weight, names[g.group]) for g in v.groups), default=(0.0, ""))[1] for v in body.data.vertices])
    heads = {b.name: np.array(man["armature"].matrix_world @ b.head_local) for b in man["armature"].data.bones}
    ways = np.array([(x, y, z) for x in (-1, 0, 1) for y in (-1, 0, 1) for z in (-1, 0, 1) if (x, y, z) != (0, 0, 0)], float)
    for s_ in "lr":
        for b in ("hand_" + s_, "fingers_" + s_, "thumb_" + s_):
            off = V[dom == b] - heads[b]
            HAND_SKIN[b] = off[np.unique(np.argmax(off @ ways.T, axis=0))]


def mh_body(nation, seed=None):
    man = _mh_body(nation, seed)
    if ON and nation == NATION and seed == SEED:
        order_eyes(man)
        hand_skin(man)
    return man


S.mh_body = mh_body


# ================================================================================================ unarmed
_usa_assemble = S.assemble


def assemble(nation, weapon="rifle", seed=None):
    medic = ON and nation == NATION
    m = _usa_assemble(nation, weapon, SEED if medic and seed is None else seed)
    if medic:
        for k in ("weapon", "weapon_part"):
            ob = m[k]
            me = ob.data
            bpy.data.objects.remove(ob)
            bpy.data.meshes.remove(me)
            m[k] = None
        m["medic"] = True
        m["rig"].medic = True
    return m


S.assemble = assemble
_man_objects = S.man_objects


def man_objects(m):
    return [o for o in _man_objects(m) if o is not None]


S.man_objects = man_objects


def in_hand(rig, st):
    """The weapon bone (and its bolt and bipod) where his right hand would grip a rifle, turned with the hand."""
    D = st["D"]["hand_r"]
    H = st["H"]["hand_r"] + D @ (rig.H0["weapon"] - rig.H0["hand_r"])
    st["D"]["weapon"], st["H"]["weapon"] = D, H
    st["W"] = S.M4(D, H)
    for b in ("bolt", "bipod"):
        rig._child(st, b)
    return st


_clip_state = S.clip_state


def clip_state(rig, name, t, kind=None):
    st = _clip_state(rig, name, t, kind)
    return in_hand(rig, st) if _medic(rig) else st


S.clip_state = clip_state
_mo_prepare = S.mo_prepare


def mo_prepare(rig, name):
    fresh = name not in rig.mo
    d = _mo_prepare(rig, name)
    if fresh and _medic(rig):                    # (no hand round a stock: the fingers as the capture has them)
        d["held"] = {s: np.zeros_like(v) for s, v in d["held"].items()}
        if name == "go_prone":
            prone_rise(rig, d)
    return d


# go_prone, free-handed (soldier.py's rifle drill is not his): the capture's own arms catch him, kept off the ground,
# and over its last 0.65 s his trunk, neck, head, shoulders, arms and hands go to fire_prone's first pose (up on his
# elbows, the forearms flat), the capture's pelvis and legs kept, so the game's blend into fire_prone has nothing to
# cross. Times are the capture's seconds (1.9 long).
RISE = dict(arms=(1.25, 1.75), trunk=(1.35, 1.85), out=0.12, lift=0.03, pole_up=0.25)
RISE_TRUNK = ("spine_01", "spine_02", "chest", "neck", "head", "clavicle_l", "clavicle_r")
HAND_CLEAR = dict(wrist=0.03, knuckles=0.012, tips=0.012, skin=0.003)  # (m over the ground: the bones' points as the
ARM_CLEAR = dict(elbow=0.04, forearm=0.035)                          # drill's free hand has them, and the hands' skin;
GROUND_CLEAR = dict(HAND_CLEAR, **ARM_CLEAR)                         # each under the lying pose's own)


def arm_low_points(rig, s, clear=GROUND_CLEAR):
    """An arm's points that must stay over the ground, as mo_state will pose him (the wrist, knuckles, the fingers'
    and thumb's tips, the hand's skin (HAND_SKIN), as clear has them; the elbow and mid forearm): (mm, f) ->
    ([(point, clearance)], shoulder), for soldier.py's _lift_clear."""
    tips = {b: rig.T0[b] - rig.H0[b] for b in ("fingers_" + s, "thumb_" + s)}
    skin = {b: HAND_SKIN[b] for b in ("hand_" + s, "fingers_" + s, "thumb_" + s)}

    def pts(mm, f):
        J = S._rig_heads(rig, mm, f)
        el, wr = J["lowerarm_" + s][0], J["hand_" + s][0]
        out = []
        if "wrist" in clear:
            out += [(wr, clear["wrist"]), (J["fingers_" + s][0], clear["knuckles"])]
            out += [(J[b][0] + J[b][1] @ off, clear["tips"]) for b, off in tips.items()]
            for b, offs in skin.items():
                out += [(p, clear["skin"]) for p in J[b][0] + offs @ J[b][1].T]
        if "elbow" in clear:
            out += [(el, clear["elbow"]), (0.5 * (el + wr), clear["forearm"])]
        return out, J["upperarm_" + s][0]
    return pts


def ground_low(rig, m, s, frames):
    """The lowest of an arm's points less its clearance over the given frames (m; negative where it dips)."""
    pts = arm_low_points(rig, s)
    return min((min(p[2] - c for p, c in pts(m, f)[0]) for f in frames), default=0.0)


def lift_clear(rig, m, frames, clear):
    """(go_prone) Each arm raised about its shoulder where its hand (or elbow) would dip into the ground, as the
    rifle drill raises its free hand (soldier.py's _lift_clear, measured on our man)."""
    for s_ in "lr":
        S._lift_clear(m, s_, frames, None, steps=24, ours=arm_low_points(rig, s_, clear))


def raise_wrists(rig, m, frames, clear, k=4, passes=4):
    """(go_prone) Each wrist raised where its hand would dip into the ground, the elbow bending to take it up (the
    arm solved anew on our man, its bones' own twists kept, the hand riding the forearm as the capture has it, so
    the wrist's angles are the capture's), as the rifle drill keeps its right hand up (soldier.py's _clear_ground);
    each frame's rise eased in and out over the k frames either side (the largest of the neighbours' rises,
    cos-squared), so the hand does not jump where the ground starts or stops holding it up; done over again (the
    hand tips as the forearm turns) till it is clear."""
    r = m.rest
    F = m.W.shape[0]
    up = np.array((0.0, 0.0, 1.0))
    on = np.zeros(F, bool)
    on[list(frames)] = True
    for s_ in "lr":
        pts = arm_low_points(rig, s_, clear)
        hand = [r.i(b) for b in ("hand_" + s_, "fingers_" + s_, "thumb_" + s_)]
        for _ in range(passes):
            need = np.array([max(0.0, max(c - p[2] for p, c in pts(m, f)[0])) if on[f] else 0.0 for f in range(F)])
            if need.max() < 1e-4:
                break
            eased = np.zeros(F)
            for f in np.flatnonzero(on):
                js = np.arange(max(f - k, 0), min(f + k + 1, F))
                eased[f] = float(np.max(need[js] * np.cos(np.pi / 2 * np.abs(js - f) / (k + 1)) ** 2))
            for f in np.flatnonzero(eased > 1e-5):
                J = S._rig_heads(rig, m, f)
                st = {"H": {b: np.array(v[0]) for b, v in J.items()}, "D": {b: np.array(v[1]) for b, v in J.items()}}
                A, E, Wr = J["upperarm_" + s_][0], J["lowerarm_" + s_][0], J["hand_" + s_][0]
                Uc, Lc = J["upperarm_" + s_][1], J["lowerarm_" + s_][1]
                pole = _n(E - A)
                sol = []
                for target in (Wr, Wr + up * eased[f]):
                    rig.two_bone(st, "upperarm_" + s_, "lowerarm_" + s_, "hand_" + s_, target, pole)
                    sol.append((st["D"]["upperarm_" + s_], st["D"]["lowerarm_" + s_]))
                (U0, L0_), (U1, L1) = sol
                U, L = U1 @ U0.T @ Uc, L1 @ L0_.T @ Lc
                Lw = L @ r.R[r.i("lowerarm_" + s_)]
                for h in hand:                          # (the hand and fingers riding the forearm)
                    m.W[f, h] = Lw @ m.W[f, r.i("lowerarm_" + s_)].T @ m.W[f, h]
                m.W[f, r.i("upperarm_" + s_)] = U @ r.R[r.i("upperarm_" + s_)]
                m.W[f, r.i("lowerarm_" + s_)] = Lw


def prone_rise(rig, d):
    """go_prone's prepared capture (d, from mo_prepare) reworked for the medic: the capture's own arms catching him,
    raised where the hands would dip into the ground; then the rise into fire_prone's first pose over its end, kept
    off the ground."""
    mocap = S.mocap
    r = rig.mrest
    c = S.MO_DATA[S.MOCAP["go_prone"]["src"]]
    F, fps = d["F"], d["fps"]
    t = np.arange(F) / fps
    W = np.zeros((F, len(r.names), 3, 3))
    for b, p in enumerate(r.parent):            # (the capture as mo_prepare carries it over: no carry laid on it)
        n = r.names[b]
        D = mocap.quat_to_mat(np.array(c["bones"][n])) if n in c["bones"] else np.eye(3)
        W[:, b] = (W[:, p] if p >= 0 else np.eye(3)) @ D @ r.local(b)
    m = mocap.Motion(r, W, np.array(d["H"]), fps)
    same = min(float(np.min(np.abs(np.sum(S._quats(m.W[:, r.i(n)] @ r.R[r.i(n)].T) * d["Q"][n], axis=1))))
               for n in d["Q"] if n in r.names)
    if same < 0.9999:
        raise RuntimeError(f"go_prone: the capture rebuilt differs from mo_prepare's ({same})")
    before = {s: ground_low(rig, m, s, range(F)) for s in "lr"}
    fp = S.clip_state(rig, "fire_prone", 0.0)
    sm = lambda a, b: S._sm_arr((t - a) / (b - a))
    wA, wT = sm(*RISE["arms"]), sm(*RISE["trunk"])
    raise_wrists(rig, m, [f for f in range(F) if wA[f] < 1.0], HAND_CLEAR)
    caught = {s: ground_low(rig, m, s, [f for f in range(F) if wA[f] < 1.0]) for s in "lr"}
    # (the trunk, neck, head and shoulders to fire_prone's own turns)
    m0 = m.copy()
    mT = m.copy()
    for n in RISE_TRUNK:
        mT.W[:, r.i(n)] = fp["D"][n] @ r.R[r.i(n)]
    L0, LT = mocap.to_local(m), mocap.to_local(mT)
    for n in RISE_TRUNK:
        L0[:, r.i(n)] = mocap.slerp_mats(L0[:, r.i(n)], LT[:, r.i(n)], wT)
    m = mocap.from_local(r, L0, m.H, fps)
    # (the arms from the capture's own, its hands planted beside his shoulders, to lie as fire_prone's do, placed from
    # his shoulders where the capture's pelvis has them: each wrist slid forward along the ground in a line, the elbow
    # swung up and out to his side and then forward and down to the ground, the arm solved anew each frame; each bone's
    # twist and the hand's turn on the forearm carried over evenly from the capture's to the lying arm's, so nothing
    # jumps where the capture's arm is left; the fingers and thumbs to fire_prone's on the hand)
    up = np.array((0.0, 0.0, 1.0))

    def solve(st, s_, wrist, pole):
        st2 = {"H": dict(st["H"]), "D": dict(st["D"]), "short": {}}
        rig.two_bone(st2, "upperarm_" + s_, "lowerarm_" + s_, "hand_" + s_, wrist, pole)
        return st2["D"]["upperarm_" + s_], st2["D"]["lowerarm_" + s_]
    for f in range(F):
        if wA[f] <= 0.0:
            continue
        J0, J = S._rig_heads(rig, m0, f), S._rig_heads(rig, m, f)
        st = {"H": {b: np.array(v[0]) for b, v in J.items()}, "D": {b: np.array(v[1]) for b, v in J.items()}, "short": {}}
        w = float(wA[f])
        for s_, side in (("l", 1.0), ("r", -1.0)):
            a0, t0, n0 = rig.hand0[s_]
            grip = S.grip_point(rig.J, s_) - rig.H0["hand_" + s_]
            A0, A = J0["upperarm_" + s_][0], st["H"]["upperarm_" + s_]
            target, a1, n1, pole_t = lie_hand(rig, st, side)
            Dt = S.frame2(a1, n1) @ S.frame2(a0, n0).T
            Wt = target - Dt @ grip
            Uc, Lc, Dc = J0["upperarm_" + s_][1], J0["lowerarm_" + s_][1], J0["hand_" + s_][1]
            Wc = J0["hand_" + s_][0] - A0 + A                       # (the capture's wrist, from his shoulder now)
            pole_c = _n(J0["lowerarm_" + s_][0] - A0)
            U0, L0_ = solve(st, s_, Wc, pole_c)                     # (the capture's arm as solved here: its twists apart)
            Ut, Lt = solve(st, s_, Wt, pole_t)
            fwd = st["H"]["neck"] - st["H"]["pelvis"]
            fwd[2] = 0.0
            out = _n(side * np.cross(up, _n(fwd)) + RISE["pole_up"] * up)   # (the elbow's way between: out to his side, up a little)
            pole_t = _n(pole_t)
            sl = lambda x, y, u: _n(x * math.sin((1 - u) * math.acos(np.clip(x @ y, -1, 1))) + y * math.sin(u * math.acos(np.clip(x @ y, -1, 1))))
            pole = sl(pole_c, out, min(2 * w, 1.0)) if w < 0.5 else sl(out, pole_t, 2 * w - 1.0)
            bump = math.sin(math.pi * w)                             # (the hand lifted a little and out round his shoulder)
            out_h = _n(side * np.cross(up, _n(fwd)))
            U, L = solve(st, s_, Wc + (Wt - Wc) * w + out_h * RISE["out"] * bump + up * RISE["lift"] * bump, pole)
            twist = lambda X0, Xc, u: S.mocap.slerp_mats((X0.T @ Xc)[None], np.eye(3)[None], np.array([u]))[0]
            U, L = U @ twist(U0, Uc, w), L @ twist(L0_, Lc, w)
            rel = S.mocap.slerp_mats((Lc.T @ Dc)[None], (Lt.T @ Dt)[None], np.array([w]))[0]
            turns = {"upperarm_" + s_: U, "lowerarm_" + s_: L, "hand_" + s_: L @ rel}
            for b in ("fingers_" + s_, "thumb_" + s_):
                fr = S.mocap.slerp_mats((Dc.T @ J0[b][1])[None], (fp["D"]["hand_" + s_].T @ fp["D"][b])[None], np.array([w]))[0]
                turns[b] = turns["hand_" + s_] @ fr
            for b, D in turns.items():
                m.W[f, r.i(b)] = D @ r.R[r.i(b)]
    risen = {s: ground_low(rig, m, s, range(F)) for s in "lr"}
    rising = [f for f in range(F) if 0.0 < wA[f] < 1.0]
    raise_wrists(rig, m, rising, HAND_CLEAR)
    lift_clear(rig, m, rising, ARM_CLEAR)
    after = {s: ground_low(rig, m, s, range(F)) for s in "lr"}
    for b, n in enumerate(r.names):
        if n in d["Q"]:
            d["Q"][n] = S._quats(m.W[:, b] @ r.R[b].T)
    G, gl = mocap.hand_grips(r)["r"]
    h = r.i("hand_r")
    d["Qg"] = S._quats(m.W[:, h] @ G.T @ S.GUN_AXES)
    d["Pg"] = m.heads()[:, h] + np.einsum("fij,j->fi", m.W[:, h], gl)
    PRONE_REPORT.update(capture=before, caught=caught, risen=risen, cleared=after)
    print("  medic: go_prone's arms over the ground (lowest point less its clearance, cm): " +
          "; ".join(f"{k} " + ", ".join(f"{s} {100 * v[s]:.1f}" for s in "lr") for k, v in PRONE_REPORT.items()))


PRONE_REPORT = {}


S.mo_prepare = mo_prepare


# ================================================================================================ his clips
# The capture's own arms: no carry laid over it, no rifle dropped (in this process only).
for _name, _spec in list(S.MOCAP.items()):
    S.MOCAP[_name] = dict(src=_spec["src"])
S.KEYED_DROP.pop("die_kneel", None)


def PARK(rig, st):
    """(the weapon bone while the pose is solved: on the pelvis; clip_state then puts it in the right hand)"""
    return S.M4(st["D"]["pelvis"], st["H"]["pelvis"])


# arms by their own angles (Rig.arm_place: elevation, plane, rotation, elbow, pronation, wrist flexion, deviation)
HANG = (13.0, 20.0, 15.0, 18.0, 12.0, 0.0, 0.0)          # hanging easy at his sides, the elbows a little bent
READY = (26.0, 45.0, 12.0, 50.0, 12.0, 8.0, 0.0)         # the hands a little before and beside the thighs, ready
# (both clear of the medical pouches at the front of his hips by 10 cm or more from their middles)


def freed(P, l=HANG, r=HANG, fingers=25.0, thumbs=20.0):
    """A pose with no rifle: the weapon bone parked, no sight, each hand by its arm's angles (a tuple) or a spec."""
    P["weapon"] = PARK
    P["sight"] = 0.0
    for k in ("weapon_kind", "bolt", "bipod"):
        P.pop(k, None)
    P["hand_l"] = dict(l) if isinstance(l, dict) else {"arm": tuple(l)}
    P["hand_r"] = dict(r) if isinstance(r, dict) else {"arm": tuple(r)}
    fl, fr = (fingers, fingers) if np.isscalar(fingers) else fingers
    tl, tr = (thumbs, thumbs) if np.isscalar(thumbs) else thumbs
    P.update(fingers_l=fl, fingers_r=fr, thumb_l=tl, thumb_r=tr)
    return P


def at_ease(t=0.0):
    P = S.base_stand(t)
    P["clavicle_l"] = P["clavicle_r"] = (0.0, 2.0)
    return freed(P)


def scan(t, amp=20.0):
    """A look round over four seconds: left, held, across to the right, held, back."""
    return amp * (S.ramp(t, 0.3, 1.0) - 2.0 * S.ramp(t, 1.6, 2.6) + S.ramp(t, 3.2, 3.9))


WATCH_LONG = 4.0


@S.clip("fire_stand", WATCH_LONG, loop=True)
def c_watch_stand(t):
    """(in place of firing standing) Half crouched, watching: the head turning left and right, the hands ready."""
    P = S.base_stand()
    P.update(S.stand_feet(ahead=0.1, apart=0.14, back=-0.1))
    P["pelvis_at"] = (-0.03, 0.0, -0.07)
    P["pelvis"] = (10.0, 0.0, -6.0)
    P["spine_01"], P["spine_02"], P["chest"] = (4.0, 0.0, 1.0), (5.0, 0.0, 1.5), (5.0, 0.0, 2.0)
    y = scan(t)
    P["neck"] = (-6.0, 0.0, 0.45 * y)
    P["head"] = (-5.0, -0.06 * y, 0.55 * y)
    P["clavicle_l"] = P["clavicle_r"] = (2.0, 6.0)
    freed(P, READY, READY, fingers=35.0)
    return S.breathe(P, t, WATCH_LONG, 1.0)


KNEE_ARM_L = (60.0, 66.0, 8.0, 72.0, 0.0, 22.0, 0.0)    # (the left forearm laid over the left knee, the hand drooping)
KNEE_HAND_L = {"arm": KNEE_ARM_L}
KNEE_ARM_R = (22.0, 18.0, 5.0, 42.0, 10.0, 8.0, 0.0)     # (the right arm loose at his side, clear of the pouch)
HALF_ON_KNEE = {"arm": tuple(0.5 * (x + y) for x, y in zip(READY, KNEE_ARM_L))}      # (on the way to the knee or off it)


def kneel_watch(t=0.0):
    """On the right knee, watching: the left forearm across the left knee, the right hand loose by the thigh."""
    P = S.kneel_base(aimed=False)
    P["pelvis"] = (10.0, 0.0, -12.0)
    P["spine_01"], P["spine_02"], P["chest"] = (7.0, 0.0, 3.0), (9.0, 0.0, 3.0), (9.0, 0.0, 4.0)
    P["clavicle_l"], P["clavicle_r"] = (-2.0, 10.0), (0.0, 5.0)
    y = scan(t, 22.0)
    P["neck"] = (-8.0, 0.0, 6.0 + 0.45 * y)
    P["head"] = (-6.0, -0.06 * y, 6.0 + 0.55 * y)
    return freed(P, KNEE_HAND_L, KNEE_ARM_R, fingers=(30.0, 35.0))


@S.clip("fire_kneel", WATCH_LONG, loop=True)
def c_watch_kneel(t):
    """(in place of firing kneeling) Watching from the right knee."""
    return S.breathe(kneel_watch(t), t, WATCH_LONG, 1.0)


# Up on his elbows (fire_prone, lmg_burst, go_prone's end): each arm placed from where its shoulder is, as the low
# crawl plants its elbows (mocap.CRAWL's el_z): the elbow down at LIE_ELBOW_Z, a little ahead of and out from the
# shoulder; the forearm flat on the ground, turned in toward the other (degrees from his heading); the hand flat, palm
# down, the wrist at LIE_WRIST_Z.
LIE = {"l": dict(out=20.0, fore_in=22.0, hand_in=5.0), "r": dict(out=20.0, fore_in=28.0, hand_in=5.0)}
LIE_TILT = 15.0                                  # (the hand pitched up at the knuckles, degrees: its heel and fingertips down)
LIE_ELBOW_Z, LIE_WRIST_Z = 0.045, 0.035
PRONE_HAND_L = PRONE_HAND_R = {"lie": True}
PRONE_FINGERS = (-45.0, -45.0)                   # (the fingers and thumb opened from the rest's half-shut, to lie flat)


def lie_hand(rig, st, side):
    """(Rig.resolve_hand for a hand {"lie": True}) The grip point, hand and palm ways and elbow pole of an arm lying
    as LIE says, from the shoulder where the trunk has put it."""
    s = "l" if side > 0 else "r"
    q, H0 = LIE[s], rig.H0
    A = st["H"]["upperarm_" + s]
    l1 = float(np.linalg.norm(H0["lowerarm_" + s] - H0["upperarm_" + s]))
    l2 = float(np.linalg.norm(H0["hand_" + s] - H0["lowerarm_" + s]))
    fwd = st["H"]["neck"] - st["H"]["pelvis"]
    fwd[2] = 0.0
    fwd = _n(fwd)
    out = side * np.cross((0.0, 0.0, 1.0), fwd)
    way = lambda deg: fwd * math.cos(math.radians(deg)) + out * math.sin(math.radians(deg))
    E = A + way(q["out"]) * math.sqrt(max(l1 * l1 - (A[2] - LIE_ELBOW_Z) ** 2, 1e-4))
    E[2] = LIE_ELBOW_Z
    Wr = E + way(-q["fore_in"]) * math.sqrt(l2 * l2 - (LIE_ELBOW_Z - LIE_WRIST_Z) ** 2)
    Wr[2] = LIE_WRIST_Z
    a1 = _n(way(-q["fore_in"] - q["hand_in"]) + np.array((0.0, 0.0, math.tan(math.radians(LIE_TILT)))))
    n1 = _n(np.array((0.0, 0.0, -1.0)) + a1 * a1[2])
    a0, t0, n0 = rig.hand0[s]
    Dh = S.frame2(a1, n1) @ S.frame2(a0, n0).T
    return Wr + Dh @ (S.grip_point(rig.J, s) - H0["hand_" + s]), a1, n1, E - A


_resolve_hand = S.Rig.resolve_hand


def resolve_hand(self, st, P, h, side):
    if "lie" in h:
        return lie_hand(self, st, side)
    return _resolve_hand(self, st, P, h, side)


S.Rig.resolve_hand = resolve_hand


def prone_watch(t, amp=14.0):
    P = S.prone_base(arch=0.5)
    P["clavicle_l"] = P["clavicle_r"] = (8.0, 12.0)
    P["chest"] = (-6.0, 0.0, 2.0)
    y = scan(t, amp)
    P["neck"] = (-30.0, 0.0, 0.5 * y)
    P["head"] = (-18.0, 0.0, 0.5 * y)
    P["comfort"] = False                         # (the arms lie exactly as placed)
    return freed(P, PRONE_HAND_L, PRONE_HAND_R, fingers=PRONE_FINGERS[0], thumbs=PRONE_FINGERS[1])


@S.clip("fire_prone", WATCH_LONG, loop=True)
def c_watch_prone(t):
    """(in place of firing prone) Up on his elbows, the forearms and hands flat on the ground before him, watching."""
    return S.breathe(prone_watch(t), t, WATCH_LONG, 0.4)


@S.clip("lmg_burst", WATCH_LONG, loop=True)
def c_watch_prone_low(t):
    """(lmg_burst: he has no gun) The same prone watch, the head lower: harmless, no events."""
    P = prone_watch(t, 10.0)
    P["neck"] = S.add3(P["neck"], (8.0, 0.0, 0.0))
    return S.breathe(P, t, WATCH_LONG, 0.4)


@S.clip("kneel_down", 0.5, events=[S.ev(0.4, "knee_down", bone="calf_r")])
def c_kneel_down(t):
    """Down on the right knee, free-handed, into the kneeling watch."""
    mid = freed(dict(S.kneel_base(aimed=False), pelvis_at=(-0.1, 0.0, -0.3),
                     foot_r={"at": (-0.3, -0.18, 0.2), "yaw": -25.0, "pitch": 40.0, "toe": 20.0, "pole": (1.0, -0.2, -0.3)}),
                HALF_ON_KNEE, READY, fingers=30.0)
    landed = kneel_watch(0.0)
    landed["pelvis_at"] = (-0.19, 0.0, -0.475)
    return S.keyed([(0.0, at_ease()), (0.2, mid), (0.4, landed), (0.5, kneel_watch(0.0))])(t)


@S.clip("stand_up", 0.6, events=[S.ev(0.12, "kit_rattle", bone="chest")])
def c_stand_up(t):
    low = freed(S.kneel_base(aimed=False), KNEE_HAND_L, READY, fingers=30.0)
    mid = freed(dict(S.kneel_base(aimed=False), pelvis_at=(-0.08, 0.0, -0.26),
                     foot_r={"at": (-0.2, -0.16, 0.16), "yaw": -20.0, "pitch": 25.0, "toe": 10.0}), HALF_ON_KNEE, READY, fingers=30.0)
    return S.keyed([(0.0, kneel_watch(0.0)), (0.15, low), (0.38, mid), (0.6, at_ease())])(t)


@S.clip("throw_grenade", 1.6)
def c_signal(t):
    """(he throws no grenade) A signal: half turned to his right, calling, the right arm raised and waved over his
    head twice, then down again."""
    stand = c_watch_stand(0.0)
    turn = dict(stand, chest=(3.0, 2.0, -10.0), spine_02=(4.0, 1.0, -5.0), neck=(-8.0, 0.0, -18.0), head=(-8.0, 2.0, -22.0))
    up = lambda arm: freed(dict(turn), READY, arm, fingers=(35.0, 6.0), thumbs=(20.0, 5.0))
    raised = up((150.0, 45.0, -25.0, 30.0, 0.0, 0.0, 0.0))
    out = up((142.0, 22.0, -20.0, 38.0, 0.0, 0.0, 0.0))
    inn = up((156.0, 72.0, -28.0, 20.0, 0.0, 0.0, 0.0))
    return S.keyed([(0.0, stand), (0.28, raised), (0.5, out), (0.72, inn), (0.94, out), (1.14, inn), (1.32, raised),
                    (1.6, stand)])(t)


@S.clip("cower", 3.0, loop=True)
def c_cower(t):
    """Wavering: down on his heels, head pulled down, both hands clamped over his helmet, flinching at each near miss
    and peering up between them."""
    flinch = math.exp(-((t - 0.7) % 3.0) / 0.12) * S.ramp((t - 0.7) % 3.0, 0.0, 0.03) + \
        0.7 * math.exp(-((t - 2.05) % 3.0) / 0.12) * S.ramp((t - 2.05) % 3.0, 0.0, 0.03)
    peer = S.bump(t, 1.15, 1.45, 1.8)
    shiver = S.osc(t, 0.14) * 0.6 + S.osc(t, 0.23, 0.3) * 0.4
    P = {"pelvis_at": (-0.16, 0.0, -0.5 - 0.02 * flinch), "pelvis": (34.0, 0.0, 4.0), "spine_01": (10.0, 0.0, 0.0),
         "spine_02": (12.0 + 4 * flinch - 6 * peer, 0.0, 0.0), "chest": (12.0 + 5 * flinch - 6 * peer + 0.5 * shiver, 0.0, 0.0),
         "neck": (18.0 + 10 * flinch - 18 * peer, 0.0, 6.0 * peer), "head": (16.0 + 8 * flinch - 12 * peer, 2.0, 8.0 * peer),
         "clavicle_l": (14.0 + 10 * flinch, 10.0), "clavicle_r": (14.0 + 10 * flinch, 10.0),
         "foot_l": {"at": (0.1, 0.17, 0.12), "yaw": 15.0, "pitch": 28.0, "toe": 28.0, "pole": (1.0, 0.45, 0.0)},
         "foot_r": {"at": (0.06, -0.17, 0.12), "yaw": -15.0, "pitch": 28.0, "toe": 28.0, "pole": (1.0, -0.45, 0.0)}}
    lift = 0.01 * flinch
    hand = lambda side: {"frame": "chest", "at": (0.15, 0.13 * side, 0.385 + lift), "a": (-0.25, -0.1 * side, 0.96),
                         "n": (0.0, -side, 0.0), "pole": (1.0, 0.25 * side, -0.35)}
    freed(P, hand(1.0), hand(-1.0), fingers=20.0, thumbs=25.0)
    return S.breathe(P, t, 1.0, 1.2)


@S.clip("die_kneel", 1.5, events=[S.ev(0.45, "knee_down", bone="calf_l"), S.ev(1.0, "body_fall", bone="spine_02")])
def c_die_kneel(t):
    """soldier.py's die_kneel with no rifle: hit, both hands to the belly, down onto both knees, forward onto his face."""
    stand = at_ease()
    belly = (24.0, 60.0, 42.0, 105.0, 30.0, 25.0, 0.0)
    hunch = freed(dict(S.base_stand(), chest=(18.0, 0.0, 0.0), spine_02=(12.0, 0.0, 0.0), spine_01=(8.0, 0.0, 0.0), neck=(10.0, 0, 0),
                       head=(12.0, 0, 0), pelvis_at=(0.0, 0.0, -0.06)), belly, belly, fingers=40.0)
    knees = freed({"pelvis_at": (-0.12, 0.0, -0.46), "pelvis": (10.0, 0.0, 0.0), "spine_01": (10.0, 0, 0), "spine_02": (14.0, 0, 0),
                   "chest": (16.0, 0.0, 0.0), "neck": (14.0, 0, 0), "head": (14.0, 0, 0), "clavicle_l": (-4.0, 10.0), "clavicle_r": (-4.0, 10.0),
                   "foot_l": {"at": (-0.5, 0.15, 0.105), "yaw": 10.0, "pitch": 72.0, "toe": 60.0, "pole": (1.0, 0.1, -0.5)},
                   "foot_r": {"at": (-0.5, -0.15, 0.105), "yaw": -10.0, "pitch": 72.0, "toe": 60.0, "pole": (1.0, -0.1, -0.5)}},
                  belly, belly, fingers=40.0)
    sway = dict(knees, pelvis=(14.0, -4.0, 4.0), chest=(20.0, -4.0, 0.0), head=(20.0, -6.0, 0.0))
    down = freed({"pelvis_at": (0.05, 0.08, -0.8), "pelvis": (84.0, -10.0, 14.0), "spine_01": (2.0, 0, 0), "spine_02": (2.0, -2.0, 0),
                  "chest": (2.0, -4.0, 0.0), "neck": (-4.0, 0, -30.0), "head": (0.0, -10.0, -40.0), "clavicle_l": (4.0, 8.0),
                  "clavicle_r": (4.0, 8.0),
                  "foot_l": {"at": (-0.78, 0.27, 0.09), "yaw": 30.0, "pitch": 95.0, "toe": 10.0, "pole": (0.0, 0.3, -1.0)},
                  "foot_r": {"at": (-0.74, -0.14, 0.09), "yaw": -10.0, "pitch": 95.0, "toe": 10.0, "pole": (0.0, -0.3, -1.0)}},
                 (115.0, -4.0, -35.0, 25.0, 50.0, 10.0, 0.0), (35.0, -6.0, 20.0, 30.0, 50.0, 10.0, 0.0), fingers=(30.0, 35.0))
    bounce = dict(down, pelvis_at=(0.05, 0.08, -0.77), chest=(-3.0, -4.0, 0.0))
    return S.keyed([(0.0, stand), (0.22, hunch), (0.45, knees), (0.75, sway), (1.0, down), (1.12, bounce), (1.5, down)])(t)


@S.clip("flee", 0.65, loop=True, stride=2.8, events=[S.ev(0.0, "footstep", bone="foot_l"), S.ev(0.32, "footstep", bone="foot_r")])
def c_flee(t):
    """soldier.py's flee with both arms free: they pump against the legs, a glance back over the left shoulder."""
    P = S.base_stand()
    S.gait_pose(S.FLEE, t, 14.0, 12.0, 0.016, P)
    ph = 2 * math.pi * t / S.FLEE.long
    sw = math.cos(ph)                     # +1 when the left foot is forward
    look = S.bump(t, 0.08, 0.25, 0.5)
    P["chest"] = S.add3(P["chest"], (0.0, 0.0, 12.0 * look))
    P["neck"] = (-8.0, 4.0 * look, 22.0 * look)
    P["head"] = (-6.0, 6.0 * look, 26.0 * look)
    pump = lambda f: S.swing_arm(42.0 * max(f, 0.0) - 32.0 * max(-f, 0.0), out=12.0, rot=25.0, elbow=95.0 + 15.0 * max(f, 0.0), pron=-5.0)
    return freed(P, pump(-sw), pump(sw), fingers=60.0, thumbs=40.0)


# ---- his own clip: tending a man lying across before him, a little to his right (his chest about 0.5 m ahead)
TEND_LONG = 3.5
TEND_AT = np.array((0.5, -0.1, 0.385))         # where the dressing is pressed: the wound, on the chest of a man lying as
                                               # soldier_usa.py's rifleman lies at the end of die_back (his chest ~0.34 m up)
POUCH_REACH_R = np.array((0.155, -0.21, 0.525))   # the right hand on the right medical pouch's flap (from the rig)


def tend_body(t):
    press = 0.5 * (1.0 - math.cos(2 * math.pi * t / 0.875))          # (pressing in beats, four to a loop)
    P = {"pelvis_at": (-0.04 + 0.01 * press, 0.0, -0.42 - 0.006 * press), "pelvis": (30.0 + 2.0 * press, 0.0, -10.0),
         "spine_01": (10.0, 0.0, -2.0), "spine_02": (12.0, 0.0, -3.0), "chest": (10.0 + 1.5 * press, 0.0, -4.0),
         "neck": (14.0, 0.0, -6.0), "head": (16.0, 0.0, -6.0), "clavicle_l": (2.0, 12.0), "clavicle_r": (2.0, 12.0),
         "foot_l": {"at": (-0.46, 0.15, 0.105), "yaw": 10.0, "pitch": 72.0, "toe": 60.0, "pole": (1.0, 0.1, -0.5)},
         "foot_r": {"at": (-0.46, -0.15, 0.105), "yaw": -10.0, "pitch": 72.0, "toe": 60.0, "pole": (1.0, -0.1, -0.5)}}
    return P, press


def press_hand(side, press, at=None):
    """A hand flat on the dressing, the heel of the hand down, the fingers forward: the left a little to the left,
    the right beside it and over it."""
    at = (TEND_AT if at is None else np.asarray(at)) + np.array((0.0, 0.0, -0.012 * press))
    if side > 0:
        return {"at": tuple(at + np.array((0.01, 0.035, 0.0))), "a": (0.75, -0.6, -0.25), "n": (0.1, 0.0, -1.0),
                "pole_world": (-0.2, 0.8, 0.2)}
    return {"at": tuple(at + np.array((-0.01, -0.055, -0.01))), "a": (0.9, 0.35, -0.25), "n": (0.1, 0.0, -1.0),
            "pole_world": (-0.2, -0.8, 0.2)}


@S.clip("tend", TEND_LONG, loop=True, events=[S.ev(0.0, "tend", cue="medic_tend", bone="hand_l")])
def c_tend(t):
    """On both knees by the man, leaning over him: both hands pressing a dressing to the wound in beats; the right hand
    goes to the right medical pouch for another and comes back to pack it in; a glance up and round between."""
    P, press = tend_body(t)
    away = S.ramp(t, 1.2, 1.65) - S.ramp(t, 2.15, 2.65)               # (the right hand off to the pouch and back)
    look = S.bump(t, 2.7, 2.95, 3.3)
    P["neck"] = S.add3(P["neck"], (-14.0 * look, 0.0, 10.0 * look - 8.0 * away))
    P["head"] = S.add3(P["head"], (-12.0 * look, 0.0, 12.0 * look - 10.0 * away))
    hand_l = press_hand(1.0, press * (1.0 - 0.5 * away))
    on = press_hand(-1.0, press)
    pouch = {"at": tuple(POUCH_REACH_R), "a": (0.2, -0.1, -0.97), "n": (-0.75, 0.66, 0.0), "pole_world": (-0.2, -1.0, 0.3)}
    pack = press_hand(-1.0, 0.0, TEND_AT + np.array((0.03, 0.02, -0.012)))
    back = S.ramp(t, 2.15, 2.65) * (1.0 - S.ramp(t, 2.9, 3.3))      # (packing it in beside the left hand, then pressing)
    if away > 1e-3:
        hand_r = {"blend": [(1.0 - away, pack if t > 2.0 else on), (away, pouch)]}
    elif back > 1e-3:
        hand_r = {"blend": [(1.0 - back, on), (back, pack)]}
    else:
        hand_r = on
    freed(P, hand_l, hand_r, fingers=(-25.0, -20.0 + 50.0 * away), thumbs=(5.0, 10.0 + 20.0 * away))   # (flat hands)
    return S.breathe(P, t, TEND_LONG / 2, 0.8)


S.use_mocap()            # (the capture's clips put in again with their new specs, and the game's order kept; tend last)


# ================================================================================================ levels of detail
_make_lods = S.make_lods


def make_lods(m, lod):
    """soldier.py's thinning for everything but the red crosses: the helmet is thinned without its marks and the
    marks made again for that level, as the brassard is."""
    if not _medic(m):
        return _make_lods(m, lod)
    import bmesh
    helmet = m["helmet"]
    plain = helmet.copy()
    plain.data = helmet.data.copy()
    bpy.context.scene.collection.objects.link(plain)
    names = [mt.name.split(".")[0] for mt in plain.data.materials]
    bm = bmesh.new()
    bm.from_mesh(plain.data)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if names[f.material_index] in MARKS], context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.to_mesh(plain.data)
    bm.free()
    br = next(k for k in m["kit"] if k.name.split(".")[0] == "brassard")
    marks = helmet_marks(lod)
    brass = brassard_mesh(lod)
    extra = S.tris_of(marks) + S.tris_of(brass)
    old = S.LOD_TARGET[lod]
    S.LOD_TARGET[lod] = old - extra
    try:
        out = _make_lods(dict(m, helmet=plain, kit=[k for k in m["kit"] if k is not br]), lod)
    finally:
        S.LOD_TARGET[lod] = old
        bpy.data.objects.remove(plain)
    hl = next(o for o in out if o.name.startswith("helmet_"))
    S.skin_rigid(marks, "head")
    for o in bpy.context.selected_objects:
        o.select_set(False)
    marks.select_set(True)
    hl.select_set(True)
    bpy.context.view_layer.objects.active = hl
    with bpy.context.temp_override(active_object=hl, selected_editable_objects=[hl, marks], selected_objects=[hl, marks]):
        bpy.ops.object.join()
    S.skin_like(brass, m["body"])
    S.clean_weights(brass)
    S.attach(brass, m["arm"])
    a = brass.modifiers.new("skin", "ARMATURE")
    a.object = m["arm"]
    out.append(brass)
    return out


S.make_lods = make_lods


def mark_tris(objs):
    """Triangles of the marks' paints: (on the helmet, on the brassard)."""
    h = sum(S.paint_tris(o, MARKS) for o in objs if o.name.startswith("helmet_"))
    b = sum(S.paint_tris(o, MARKS) for o in objs if o.name.startswith("brassard"))
    return h, b


def helmet_share(data):
    """variety.py's choice of the helmet's triangles (its Shell: every corner of a paint counted as the helmet's, on
    the head bone, the middle over 1.5 m), with the helmet's paint alone and with the marks' paints too: how many of
    the marks' triangles fall inside, at each level of detail."""
    hb = [b["name"] for b in data["bones"]].index("head")
    out = {}
    for lod in ("near", "mid", "far"):
        m = data[lod]
        P = np.array(m["points"]).reshape(-1, 3)
        pa = np.array(m["paints"])
        part = np.array(m["part"])
        T = np.array(m["triangles"]).reshape(-1, 3)
        head = (part[T] == hb).all(1) & (P[T].mean(1)[:, 2] > 1.5)
        mk = np.isin(pa[T], HELMET_PAINTS[1:]).all(1)
        only5 = np.isin(pa[T], HELMET_PAINTS[:1]).all(1) & head
        withm = np.isin(pa[T], HELMET_PAINTS).all(1) & head
        out[lod] = {"marks_on_head": int((mk & head).sum()), "marks_elsewhere": int((mk & ~head).sum()),
                    "helmet_paint_only": int(only5.sum()), "with_mark_paints": int(withm.sum()),
                    "marks_inside_with_mark_paints": int((mk & withm).sum()), "marks_inside_paint5_only": int((mk & only5).sum())}
    return out


# ================================================================================================ the files
def about(data):
    data["name"] = "Battle medic (company aid man), USA, Normandy 1944 (our own design)"
    data["about"] = data["about"].replace("Made by Tools/battle_units/soldier.py.",
                                          "Made by Tools/battle_units/soldier_usa_medic.py on soldier_usa.py and soldier.py.")
    data["about"] += (
        " He is soldier_USA's man (the same body, uniform, 32 bones, 5 sockets and clip names) as a company aid man,"
        " with his own face (MakeHuman's mix 140). Unarmed: no weapon mesh; the weapon bone (and the bolt and bipod on it)"
        " rides his right hand in every clip where a rifle's grip would lie, so socket_weapon holds a prop if one is hung"
        " there. His kit: the pistol belt with no cartridge pockets; two medical pouches at the front of the hips, on the"
        " pouch bones (pouch_l, pouch_r), hung from the medical yoke (the straps over his shoulders); the canteen at the"
        " right hip, the first-aid pouch at the left; no haversack. No red cross or crescent is drawn anywhere (the"
        " project's rule: the emblem is protected). He is known by a plain white brassard round the left upper arm"
        " (paint 15 medic_white), skinned with the sleeve, and by his kit and empty hands. His M1 is the plain shell:"
        " no disc or cross is on it, in paint or relief (`helmet_paints` still names 5, 15 and 16; only 5 is used)."
        " Paint 16 medic_red is used by no point. Clips: the motion capture (idle, walk, run, advance, creep, crawl,"
        " go_prone, die_forward, die_back) plays with the capture's own arms: no rifle carried, none dropped; go_prone's"
        " hands are kept off the ground (the wrists raised, the elbows bending, where the capture's hands would go into"
        " it) and over its last 0.65 s he comes up on his elbows into fire_prone's first pose (the capture's pelvis and"
        " legs kept). Keyed"
        " again with free hands: fire_stand (watching, half crouched, the head turning), fire_kneel (watching from the"
        " right knee, the left forearm on the left knee), fire_prone and lmg_burst (watching, up on his elbows, the"
        " forearms and hands flat on the ground), kneel_down and stand_up, flee (both arms pumping), throw_grenade (a signal: half"
        " turned, the right arm raised and waved over his head twice), cower (on his heels, both hands clamped over his"
        " helmet) and die_kneel (both hands to the belly). His own clip tend (3.5 s, loops): on both knees by a man"
        " lying across before him a little to his right (his chest about 0.5 m ahead, a man lying as soldier_USA does"
        " at the end of die_back), both hands flat on the wound about 0.385 m up, pressing in beats; the right hand goes"
        " to the right medical pouch and back to pack it; event medic_tend at its start (no dressing is modelled)."
        " None of his clips fires: no muzzle_flash, rifle_shot, bolt, grenade or rifle_drop events; each clip's"
        " `weapon` is none.")
    data["colours"] = dict(S.LOOK[NATION])
    data["helmet_paints"] = list(HELMET_PAINTS)
    data["new_paints"] = {k: S.FIGURE_PAINTS[k] for k in MARKS}
    for c in data["clips"].values():
        c["weapon"] = "none"
    return data


def export_man(info, m):
    t0 = time.time()
    socks = m["sockets"]
    lods = {0: S.man_objects(m), 1: S.make_lods(m, 1), 2: S.make_lods(m, 2)}
    bpy.context.scene.frame_set(0)
    rec = info.setdefault(BASE, {})
    rec["tris"] = {lod: sum(S.tris_of(o) for o in lods[lod]) for lod in lods}
    rec["parts"] = {o.name.split(".")[0]: S.tris_of(o) for o in lods[0]}
    rec["marks"] = {lod: dict(zip(("helmet", "brassard"), mark_tris(lods[lod]))) for lod in lods}
    rec["plain"] = {lod: plain_marks(lods[lod]) for lod in lods}       # (no red cross: before anything is written)
    rec["helmet"] = {lod: sum(S.tris_of(o) for o in lods[lod] if o.name.startswith("helmet_")) for lod in lods}
    rec["short"] = {k: float(v) for k, v in m["short"].items()}
    S.export_glb(S.stage_path(f"{BASE}.glb"), [m["arm"]] + lods[0] + socks)
    for lod in (1, 2):
        S.export_glb(S.stage_path(f"{BASE}_LOD{lod}.glb"), [m["arm"]] + lods[lod] + socks, anim=False)
    data = S.write_game_json(S.stage_path(f"{BASE}.json"), m, lods)
    data = about(data)
    with open(S.stage_path(f"{BASE}.json"), "w") as f:
        json.dump(data, f, separators=(",", ":"))
    rec["json"] = {k: len(data[k]["triangles"]) // 3 for k in ("near", "mid", "far")}
    rec["share"] = helmet_share(data)
    rec["high"] = data["high"]
    rec["sockets"] = data["sockets"]
    rec["bones"] = [b["name"] for b in data["bones"]]
    rec["clips"] = {k: [v["long"], v["loop"], [e["cue"] for e in v["events"]]] for k, v in data["clips"].items()}
    for lod in (1, 2):
        for o in lods[lod]:
            bpy.data.objects.remove(o)
    print(f"  {BASE} written in {time.time() - t0:.0f} s: {rec['tris']}; marks {rec['marks']}; helmet {rec['helmet']}")
    print("  variety's helmet triangles:", json.dumps(rec["share"]))
    return [f"{BASE}.glb", f"{BASE}_LOD1.glb", f"{BASE}_LOD2.glb", f"{BASE}.json"]


def export_medic(info, deliver=True):
    t0 = time.time()
    kit.fresh(S.FPS)
    m = S.assemble(NATION)
    S.bake_clips(m)
    files = export_man(info, m)
    if deliver:
        for n_ in files:
            S.deliver(n_)
    info["export_s"] = round(time.time() - t0)
    return info


# ================================================================================================ the picture
CAS_ARM = (0.44, 0.10, 0.05)      # the casualty's wrists: from each shoulder toward his feet, out from his side, and up off the ground (m)


def lay_casualty(c, at, head_way):
    """The rifleman as the casualty: at the end of die_back (on his back), his rifle put away, turned so his head
    lies toward head_way and moved so his chest's middle lies over `at` (x, y)."""
    global ON
    rig = c["rig"]
    for k in ("weapon", "weapon_part"):
        c[k].hide_render = True
    for ob in c["kit"]:                             # (his haversack and shovel taken off him: he lies flat)
        if ob.name.split(".")[0] in ("pack", "shovel"):
            ob.hide_render = True
    st = S.clip_state(rig, "die_back", S.CLIPS["die_back"]["long"])
    H = st["H"]
    feet = S._n(H["pelvis"] - H["chest"])
    side = S._n(H["upperarm_l"] - H["upperarm_r"])
    for s, sg in (("l", 1.0), ("r", -1.0)):        # (his arms laid down by his sides, the hands loose and open)
        sh = H["upperarm_" + s]
        wrist = sh + feet * CAS_ARM[0] + side * sg * CAS_ARM[1]
        wrist[2] = CAS_ARM[2]
        rig.two_bone(st, "upperarm_" + s, "lowerarm_" + s, "hand_" + s, wrist, side * sg - np.array((0.0, 0.0, 0.6)))
        rig._child(st, "hand_" + s)
        a0, t0, n0 = rig.hand0[s]
        rig._child(st, "fingers_" + s, S.R_axis(S._n(np.cross(a0, n0)), -15.0))
        kt = S._n(np.cross(rig.T0["thumb_" + s] - rig.H0["thumb_" + s], n0))
        rig._child(st, "thumb_" + s, S.R_axis(kt, -5.0))
    S.pose_once(c["arm"], rig, st, 0)
    hd, pv, ch = st["H"]["head"], st["H"]["pelvis"], st["H"]["chest"]
    ax = hd - pv
    turn = math.atan2(head_way[1], head_way[0]) - math.atan2(ax[1], ax[0])
    Rz = S.R_axis((0, 0, 1), math.degrees(turn))
    p = Rz @ ch
    c["arm"].rotation_euler = (0.0, 0.0, turn)
    c["arm"].location = (at[0] - p[0], at[1] - p[1], 0.0)
    return st


def picture(info, fast=False, deliver=True):
    """pictures/soldier_USA_medic.png: the medic at ease from the front quarter, from behind, and tending a man (the
    rifleman, lying on his back across before him)."""
    global ON
    size = (640, 760, 12) if fast else (1000, 1180, 36)
    kit.fresh(S.FPS)
    m = S.assemble(NATION)
    S.studio_smooth(m)
    plain_marks(S.man_objects(m))
    ON = False
    c = S.assemble(NATION)                          # (soldier_usa.py's rifleman, face 85)
    ON = True
    S.studio_smooth(c)
    rig = m["rig"]
    P = at_ease()
    P["foot_l"] = {"at": (0.12, 0.12, S.ANKLE), "yaw": 6.0}
    P["foot_r"] = {"at": (-0.06, -0.12, S.ANKLE), "yaw": -8.0}
    S.pose_once(m["arm"], rig, P, 0)
    lay_casualty(c, (TEND_AT[0], TEND_AT[1] + 0.05), (0.0, -1.0))
    cas = [ob for ob in [c["arm"]] + S.man_objects(c) if not ob.hide_render]      # (what of him is drawn)
    for ob in cas:
        ob.hide_render = True
    kit.studio()
    kit.backdrop("CFC6B4")
    cam = kit.camera(lens=50)
    S.shared_render(*size)
    shots = []
    for key, yaw, pitch, dist, aim in (("front", 28.0, 6.0, 3.5, (0.0, 0.0, 0.95)), ("back", 205.0, 8.0, 3.5, (0.0, 0.0, 0.95))):
        kit.orbit(cam, aim, dist, pitch, yaw)
        shots.append(kit.still(os.path.join(S.SCRATCH, "pictures", f"medic_{key}.png")))
    for ob in cas:
        ob.hide_render = False
    S.pose_once(m["arm"], rig, S.clip_state(rig, "tend", 0.35), 0)
    kit.orbit(cam, (0.42, 0.28, 0.36), 3.3, 24.0, 6.0)
    shots.append(kit.still(os.path.join(S.SCRATCH, "pictures", "medic_tend.png")))
    out = S.stage_path("pictures", f"{BASE}.png")
    kit.sheet(shots, out, cols=3, labels=["American medic at ease: plain white brassard, no weapon",
                                          "From behind: medical yoke, pouches, canteen; no haversack",
                                          "tend: on his knees by a wounded man, pressing on the wound"],
              title="American company aid man, Normandy 1944: soldier_usa_medic.py (our own design)")
    if deliver and not fast:
        S.deliver(f"pictures/{BASE}.png")
    info["picture"] = out
    return out


def main():
    a = kit.args()
    fast = "--fast" in a
    steps = a[a.index("--only") + 1].split(",") if "--only" in a else ["export", "picture"]
    keep = "--keep" in a
    p = S.stage_path(f"export_info_{BASE}.json")
    info = json.load(open(p)) if os.path.exists(p) else {}
    for step in steps:
        t0 = time.time()
        if step == "export":
            export_medic(info, deliver=not keep)
        elif step == "picture":
            picture(info, fast, deliver=not keep)
        with open(p, "w") as f:
            json.dump(info, f, indent=1, default=str)
        print(f"step {step}: {time.time() - t0:.0f} s")
    if not keep:
        ok = S.check_delivered()
        print("delivered" if ok else "SOME FILES DID NOT LAND")


if __name__ == "__main__":
    main()
