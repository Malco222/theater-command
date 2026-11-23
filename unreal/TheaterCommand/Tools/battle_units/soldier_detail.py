"""Soldiers to the vehicles' detail, the pilot (Brian, 23 November 2026 14:22Z: "can we make the soldiers at the same
level of artistic detail as the vehicles, tanks, and planes"; release 2; the plan: claude.ai/artifact/PcZi45SfZiBVqvPL3aNcHf).

The German rifleman made again by soldier.py with the art director's gap list worked through, as wrappers round
soldier.py's own builders (soldier.py itself is not changed, so every other army is made as before):

- cloth: the uniform thinned to about 8,800 triangles, not 2,450, so its hems, flaps and folds keep their shape; folds
  where cloth gathers (the crook of each elbow, the sleeve above the cuff, the front of the armpit, the crotch, behind
  and above each knee) and the jackboot's ankle wrinkles; box pleats on the four pockets and the pointed (scalloped)
  flaps the pleats button to;
- small hardware: domed buttons (front, pockets, shoulder straps), the collar patches with their two bars, the belt's
  buckle plate with its round boss, the Y-straps' chest buckles, shoulder rings and back ring, the pouches' closing
  straps and studs, the bread bag's straps;
- kit: every bag, pouch, flask, case and canister made at about three times the triangles on a finer grid; the bayonet
  in its frog over the entrenching tool;
- helmet: 72 sides and twelve rings, a rolled rim, the two vent lugs and three liner rivets, the chin strap's buckle;
- rifle: the stock at 1,400 triangles with finger grooves and the butt's disc and sling slot; a stepped barrel, the
  cleaning rod, the bayonet lug, a hooded front sight, a leaf rear sight, the bolt's cocking piece and safety;
- hands at 900 triangles a hand; the middle level of detail at 2,700 triangles (under the 2,048-point cap).

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
S.BUDGET["uniform"] = int(os.environ.get("DETAIL_UNIFORM", 7800))
S.BUDGET["hand"] = 900
S.LOD_TARGET[1] = 2700
SCALE = {"tris": 1.0, "h": 1.0}
_mesh_from_layers = S.mesh_from_layers


def mesh_from_layers(name, layers, h, target_tris, **kw):
    """soldier.py's, with the kit's budgets scaled while the kit is made."""
    t = int(target_tris * SCALE["tris"]) if target_tris else target_tris
    return _mesh_from_layers(name, layers, h * SCALE["h"], t, **kw)


S.mesh_from_layers = mesh_from_layers


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


def land(L, start, way, step=0.004, n=400):
    """Walk from `start` along `way` onto the layer's surface in short steps (on_surface's sphere tracing can step
    through a thin skirt where the field is not a true distance). Returns (point, outward normal)."""
    p, w = np.array(start, float), _n(np.asarray(way, float))
    for _ in range(n):
        d = float(L.at_points(p[None, :])[0])
        if abs(d) < 2e-5:
            break
        p = p + w * float(np.clip(d, -step, step))
    e = 1e-4
    g = np.array([(L.at_points((p + np.eye(3)[i] * e)[None, :])[0] - L.at_points((p - np.eye(3)[i] * e)[None, :])[0]) for i in range(3)])
    return p, g / np.linalg.norm(g)


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
        way = -self.R[:, 0]
        for _ in range(200):                         # (short steps: a thin skirt is stepped through by long ones)
            d = base.at_points(P)
            if np.abs(d).max() < 2e-5:
                break
            P = P + way[None, :] * np.clip(d, -0.004, 0.004)[:, None]
        self.h = ((P - self.c) @ self.R[:, 0]).reshape(U.shape)

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
            ridge(tunic, [c - d * ln, c, c + d * ln], 0.0068, 0.0038, 0.005)
        # the sleeve gathered above the turned-back cuff: two soft slanting folds, front and back
        cc = wr - fa * 0.075
        for k, (dz, a0, a1) in enumerate(((0.0, -70.0, 60.0), (0.03, 120.0, 230.0))):
            Rk = S.ring_frame(fa) @ rot_euler(8.0 * (1 - 2 * k), 0, 0)
            rx, ry = S.ring_axes(tunic, cc + fa * dz, Rk)
            ridge(tunic, arc(cc + fa * dz, Rk, rx, ry, a0, a1, 6), 0.006, 0.0032, 0.005)
        # the front of the armpit: the sleeve's cloth pulled across to the chest, two folds
        for k in range(2):
            a = sh + np.array((0.07, -side * 0.035, -0.06 - 0.03 * k))
            b = sh + np.array((0.1, -side * 0.095, -0.03 - 0.035 * k))
            ridge(tunic, [a, (a + b) / 2, b], 0.007, 0.0034, 0.006)
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
    # as a bag (and the strip stood out of the hollow of his breastbone). So they are taken out and laid again as
    # patches of the cloth itself, lifted (`Patch`: the cloth's own surface raised so much, inside a cut outline):
    # breast pockets 9.5 cm and skirt pockets 11.5 cm either side of the buttons, 9 to 10 mm proud with a crisp edge (a filled
    # pocket; any lower and it does not read under the game's smooth shading), the flap 3.5 mm over the pocket and pointed in the middle, a box pleat down each, the strip 2 mm proud
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
        LM["pockets"] = []
        for side in (1.0, -1.0):
            for aim_, half in (((0.4, 0.095 * side, Z(1.305)), (0.010, 0.052, 0.058)),
                               ((0.4, 0.118 * side, Z(0.875)), (0.009, 0.062, 0.072))):
                c0, n0 = land(base, np.array(aim_), (-1.0, 0.0, 0.0))
                R0 = frame_from(n0, (0.0, 0.0, 1.0))
                tunic.add(Patch(base, c0, half[1:], 0.006, R0, half[0]), 0.0012)
                LM["pockets"].append((c0, R0, half))
    # ---- the flap over each pocket, drawn to a point in the middle (its button's), and a box pleat down it
    LM["flap_points"] = []
    base = locals().get("base")
    for (p, R, half) in LM.get("pockets", []):
        flap_c = p + R @ np.array((0.0, 0.0, half[2] - 0.016))
        tunic.add(Patch(base, flap_c, (half[1] + 0.004, 0.019), 0.004, R, half[0] + 0.0035), 0.0008)
        tip = p + R @ np.array((0.0, 0.0, half[2] - 0.035))
        tunic.add(Patch(base, tip, (0.0105, 0.0105), 0.003, R @ rot_euler(45.0, 0, 0), half[0] + 0.0035), 0.0008)
        tunic.add(Patch(base, p + R @ np.array((0.0, 0.0, -0.016)), (0.0115, half[2] - 0.022), 0.0015, R,
                        half[0] + 0.0018), 0.0008)
        q, nq = on_surface(tunic, tip + R[:, 0] * 0.05 + R @ np.array((0.0, 0.0, -0.006)), -R[:, 0])
        LM["flap_points"].append((q, nq))
    print(f"  detail: folds, pleats and points laid in {time.time() - t0:.0f} s")
    return ls


S.uniform_layers = uniform_layers


# ================================================================================================ the kit
_build_kit = S.build_kit


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
    P = P + N * 0.005
    g.band(P, N, 0.046, 0.006, "straps", closed=True)
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
    for p, nrm in LM_.get("flap_points", []):
        q, nrm2 = on_surface(tunic, p + nrm * 0.03, -nrm)
        dome(g, q - nrm2 * 0.0004, nrm2, 0.0072, 0.0038)
    nmid, nfwd, nrx, nry = LM_["neck_ring"]
    for side in (1.0, -1.0):
        sh = np.array(J["upperarm_" + ("l" if side > 0 else "r")])
        inner = nmid + np.array((0.0, side * (nry + 0.012), 0.0))
        outer = sh + np.array((0.0, side * 0.035, 0.0))
        at = inner + _n(outer - inner) * 0.028
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
    L = Layer("pouches", "straps")
    for side in (1.0, -1.0):
        for k, (y, hw) in enumerate(cells):
            a = math.atan2(y * side, 0.12)
            p0, n0 = S.project(tunic, [(cx + math.cos(a) * 0.2, math.sin(a) * 0.2, zb - 0.012)])
            p0, n0 = p0[0], n0[0]
            R = frame_from(n0, (0.0, 0.0, 1.0))
            c = p0 + n0 * 0.028
            L.add(RoundBox(c, (0.021, hw, 0.04), 0.008, R), 0.0)
            L.add(RoundBox(c + R @ np.array((0.004, 0.0, 0.03)), (0.019, hw + 0.0015, 0.016), 0.006, R), 0.003)
            L.add(RoundBox(c + R @ np.array((0.0225, 0.0, 0.0)), (0.0016, 0.0055, 0.022), 0.0012, R), 0.0012)     # the strap
            L.add(Ellipsoid(c + R @ np.array((0.0222, 0.0, -0.026)), (0.0032, 0.0042, 0.0042), R), 0.0008)       # its stud
    if po.get("rear"):
        p0, n0 = S.project(tunic, [(cx - 0.2, 0.0, zb - 0.012)])
        p0, n0 = p0[0], n0[0]
        R = frame_from(n0, (0.0, 0.0, 1.0))
        c = p0 + n0 * 0.032
        L.add(RoundBox(c, (0.026, 0.07, 0.045), 0.01, R), 0.0)
        L.add(RoundBox(c + R @ np.array((0.005, 0.0, 0.034)), (0.024, 0.072, 0.017), 0.007, R), 0.003)
    if cells or po.get("rear"):
        out["pouches"] = (_mesh_from_layers("pouches", [L], 0.0016, 1100, detail=0.004), None)
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
    out["bread_bag"] = (_mesh_from_layers("bread_bag", [L, flap, strap], 0.0018, 650, detail=0.004), "pelvis")
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
    if spec.get("y_straps", True):
        g = Geo()                                  # (soldier.py's Y-straps at twice the points along them)
        for side in (1.0, -1.0):
            ctrl = [(x, y * side, Z(z)) for x, y, z in ((0.13, 0.082, 1.03), (0.13, 0.09, 1.16), (0.125, 0.1, 1.28), (0.09, 0.112, 1.4),
                    (0.025, 0.12, 1.468), (-0.05, 0.112, 1.45), (-0.12, 0.085, 1.37), (-0.142, 0.045, 1.29), (-0.145, 0.006, 1.235))]
            S.strap_on(g, tunic, S.spline(ctrl, 48), 0.032, 0.004)
        S.strap_on(g, tunic, S.spline([(-0.145, 0.0, Z(1.245)), (-0.14, 0.0, Z(1.13)), (-0.13, 0.0, Z(1.03))], 14), 0.036, 0.004)
        out["straps"] = (g.build("straps"), None)
        g = Geo()
        for side in (1.0, -1.0):
            p, nrm = on_surface(tunic, (0.3, 0.092 * side, Z(1.2)), (-1.0, 0.0, 0.0))
            buckle(g, p + nrm * 0.0068, nrm, (0.0, 0.0, 1.0), 0.034, 0.03)
            p, nrm = on_surface(tunic, (0.3, 0.108 * side, Z(1.39)), (-1.0, 0.0, 0.0))
            ring(g, p + nrm * 0.008 + np.array((0.0, 0.0, -0.012)), nrm, 0.011, 0.0018, up=(0.0, 0.0, 1.0))
        p, nrm = on_surface(tunic, (-0.3, 0.0, Z(1.245)), (1.0, 0.0, 0.0))
        ring(g, p + nrm * 0.008, nrm, 0.016, 0.0024)
        out["strap_metal"] = (g.build("strap_metal", sharp=50.0), None)
    print(f"  detail: kit's hardware made in {time.time() - t0:.0f} s")
    return out


S.build_kit = build_kit
Geo = S.Geo
S.LOD2_LEFT_OFF = S.LOD2_LEFT_OFF + ("strap_metal", "bayonet")
MID_LEFT_OFF = ("details", "strap_metal")             # (buttons and rings: nothing at 10 m; their triangles go to the shape)
_make_lods = S.make_lods


def make_lods(m, lod):
    if lod != 1:
        return _make_lods(m, lod)
    keep = S.man_objects
    S.man_objects = lambda mm: [o for o in keep(mm) if o.name.split(".")[0] not in MID_LEFT_OFF]
    try:
        return _make_lods(m, lod)
    finally:
        S.man_objects = keep


S.make_lods = make_lods

# the landmarks the kit needs from the uniform (soldier.py passes the kit only the layers and the height map)
_assemble = S.assemble
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
    wood.add(RoundBox((0.405, 0.0, 0.026), (0.255, 0.0172, 0.02), 0.0155), 0.03)                        # fore-end
    wood.add(RoundCone((0.24, 0.0, 0.052), (0.59, 0.0, 0.05), 0.0122, 0.0108), 0.006)                   # upper handguard
    for side in (1.0, -1.0):
        wood.sub(RoundCone((0.2, side * 0.0212, 0.031), (0.37, side * 0.0198, 0.033), 0.0036, 0.0034), 0.0025)   # finger grooves
    wood.sub(RoundBox((-0.245, 0.0, -0.05), (0.005, 0.03, 0.016), 0.002, rot_euler(0, -4, 0)), 0.001)          # the sling slot
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
    g.tube((0.618, 0.0, 0.03), (0.648, 0.0, 0.026), 0.019, 0.0165, st, 20, squash=0.95)                # the nose cap
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
    g.box((0.212, 0.0, 0.056), (0.03, 0.012, 0.004), st, taper=0.9)                                    # rear sight base
    g.box((0.21, 0.0, 0.0625), (0.026, 0.0085, 0.0024), st)                                            # its leaf
    g.box((0.198, 0.0, 0.066), (0.006, 0.011, 0.0035), st)                                             # the slider
    g.box((0.234, 0.0, 0.068), (0.003, 0.0085, 0.006), st)                                             # the notch plate
    g.box((0.085, 0.0, -0.0195), (0.044, 0.015, 0.0035), st)                                           # floorplate
    loop = [(0.0 + 0.026 * math.cos(t), 0.0, -0.016 - 0.018 * math.sin(t)) for t in np.linspace(0.1, math.pi - 0.1, 13)]
    for a, b in zip(loop[:-1], loop[1:]):
        g.tube(a, b, 0.0034, 0.0034, st, 8, caps=False)                                                # trigger guard
    g.tube((0.012, 0.0, -0.012), (0.016, 0.0, -0.02), 0.0024, 0.0022, st, 8)                           # trigger
    g.tube((0.016, 0.0, -0.02), (0.012, 0.0, -0.029), 0.0022, 0.0018, st, 8)
    sl = S.spline([(0.387, 0.0, -0.004), (0.2, 0.0, -0.05), (-0.02, 0.0, -0.075), (-0.2, 0.0, -0.086), (-0.245, 0.0, -0.06)], 26)
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
    game_smooth(near0 + mid0)
    old_near = load_json_lod(SHARED + "soldier_GER.json", "near", "old_near")
    old_mid = load_json_lod(SHARED + "soldier_GER.json", "mid", "old_mid")
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


def chest_check(m):
    """A quick look while working: the new man's chest at 1 m and square on, 640 px (pictures/check_*.png)."""
    out = os.path.join(WORK, "pictures")
    os.makedirs(out, exist_ok=True)
    near0 = m["lods"][0]
    show(m["lods"][1], False)
    show(m["lods"][2], False)
    game_smooth(near0)
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
