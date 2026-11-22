"""The tank crewmen of the Normandy battles (summer 1944), to stand in the tanks' hatches: the American tanker
(soldier_USA_tanker) and the German Panzer crewman (soldier_GER_panzer). Made beside soldier.py, soldier_usa.py,
crew.py and tank_crew.py (all stay as they are).

    python soldier_tankers.py --man usa --only export,picture       build, rig, animate and export him; his picture
    python soldier_tankers.py --man ger --only export,picture
    (--only look: test stills; --only check: his tank clips checked on him at his tanks' hatches; --keep: stage only)

Run it in a checkout of Tools/battle_units (soldier.py, soldier_usa.py, crew.py, tank_crew.py, human.py, mocap.py,
kit.py and MakeHuman's base files beside it), with the bpy module (Blender 5.0.1), as soldier_usa.py is run.

Each is soldier.py's man in every bone (the same 32, in the same order) and plays the soldier's clips, and on top of
them the tank commander's (tank_crew.py's, solved on his own body): commander_hatch, commander_roof_mg,
commander_roof_mg_burst, die_hatch_slump and die_hatch_drop, each with where it goes from his tanks' `commander`
socket. What is his own:

- the American: the winter combat jacket ("tanker jacket"), waist length, zipped, its collar, cuffs and waistband
  knitted (paint 38, knit), two slash pockets; olive drab wool trousers into canvas leggings; the pistol belt with
  two magazine pouches and the first-aid pouch, no suspenders and no pack; the tanker's helmet of 1938: a fibre dome
  with padded leather ribs over the crown, a brow band, ear flaps with the radio's earphones, the goggles strapped
  round it, their lenses on the front (paint 39, lens); the Thompson submachine gun (soldier_usa.py's squad leader's,
  with his bursts and magazine change); a staff sergeant's chevrons, as most tank commanders were;
- the German: the black Panzer wrap jacket, waist length, double-breasted with wide lapels and its collar laid open
  over the grey shirt (paint 40, shirt), the black trousers gathered over ankle boots; the black side cap with the
  radio headphones over it; the leather belt with the pistol in its holster; the Kar 98k (soldier.py's).
"""

import json
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


def _arg(name, default):
    a = sys.argv
    return a[a.index(name) + 1] if name in a else default


WHO = _arg("--man", "usa")
assert WHO in ("usa", "ger"), WHO
NATION = {"usa": "USA", "ger": "GER"}[WHO]
BASE = {"usa": "soldier_USA_tanker", "ger": "soldier_GER_panzer"}[WHO]
os.environ["CREW_WORK"] = f"/home/user/art/tankers/crew_{WHO}/"     # (crew.py's cache of the man, his own)

import tank_crew as TC  # noqa: E402  (crew.py, soldier.py and bpy with it)
C = TC.C
S = C.S
import soldier_usa as U  # noqa: E402  (the American's wrappers: they leave every other nation as it is)
import bpy  # noqa: E402
import bmesh  # noqa: E402,F401
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402,F401
import kit  # noqa: E402
from sculpt import Ellipsoid, HalfSpace, Layer, RoundBox, RoundCone, Sphere, Torus, Tube, frame_from, rot_euler  # noqa: E402,F401


def on_surface(L, start, way, steps=240, most=0.008):
    """sculpt.py's walk to a layer's surface, in steps of at most `most`: a cloth read from a shell is a field held on a
    grid that reads a flat 0.1 m beyond its band, so a full step from far off can land deep inside, where the field is
    flat again and the surface's normal comes out empty."""
    p = np.array(start, dtype=float)
    w = np.array(way, dtype=float)
    w /= np.linalg.norm(w)
    for _ in range(steps):
        d = float(L.at_points(p[None, :])[0])
        if abs(d) < 1e-5:
            break
        p = p + w * float(np.clip(d, -most, most))
    if abs(float(L.at_points(p[None, :])[0])) > 0.002:
        raise RuntimeError(f"no surface met from {np.round(start, 3)} along {np.round(way, 3)} (ended at {np.round(p, 3)})")
    e = 1e-4
    g = np.array([(L.at_points((p + np.eye(3)[i] * e)[None, :])[0] - L.at_points((p - np.eye(3)[i] * e)[None, :])[0])
                  for i in range(3)])
    if not np.isfinite(g).all() or np.linalg.norm(g) < 1e-9:
        raise RuntimeError(f"no surface met from {np.round(start, 3)} along {np.round(way, 3)}")
    return p, g / np.linalg.norm(g)

S.SCRATCH = f"/home/user/art/tankers/{WHO}/"
S.STAGE = os.path.join(S.SCRATCH, "out")
R_axis, M4, _n = S.R_axis, S.M4, S._n

SEED = {"usa": 214, "ger": 41}[WHO]       # (his face: a mix no other man's file uses; USA 85, 192, 140, 118, 146,
#                                           163, 171 and 127 are taken, GER 3)
NEW_PAINTS = {"usa": {"knit": 38, "lens": 39, "jacket": 40}, "ger": {"phones": 39, "shirt": 40}}[WHO]


# ================================================================================================ shapes on a cloth
class Over:
    """A cloth's own surface stood `off` metres out of it, kept to where `region` is (its inside): a band knitted on,
    a lapel laid over, following the cloth exactly. `src` is a snapshot of the cloth's layer (soldier_usa_para.py's)."""

    def __init__(self, src, off, region):
        self.src, self.off, self.region = src, off, region
        self.lo, self.hi = region.lo, region.hi
        self.c, self.R = region.c, None

    def d(self, px, py, pz):
        bx, by, bz = np.broadcast_arrays(np.asarray(px, float), np.asarray(py, float), np.asarray(pz, float))
        shape = bx.shape
        r = self.region.d(bx, by, bz)
        out = np.array(r, dtype=float, copy=True)
        near = r < 0.02
        if near.any():
            P = np.stack([bx[near], by[near], bz[near]], axis=1)
            out[near] = np.maximum(self.src.at_points(P) - self.off, r[near])
        return out.reshape(shape)


class Drape:
    """Cloth hung over the back as a taut sheet, not laid on the skin: a height field behind him (x as a function of
    y and z) that rests on the shoulder blades and spans the hollows between them (the spine's groove, the small of the
    back) down to the waistband; a slab DEEP thick under it (negative inside), absent where `w` is under a half."""

    DEEP = 0.06

    def __init__(self, ys, zs, xd, w):
        self.ys, self.zs, self.xd, self.w = ys, zs, xd, w
        self.lo = np.array((float(np.nanmin(xd)) - 0.01, ys[0], zs[0]))
        self.hi = np.array((float(np.nanmax(xd)) + self.DEEP + 0.01, ys[-1], zs[-1]))
        self.c, self.R = (self.lo + self.hi) / 2, None

    def _at(self, G, y, z):
        fy = np.clip((y - self.ys[0]) / (self.ys[1] - self.ys[0]), 0.0, len(self.ys) - 1.001)
        fz = np.clip((z - self.zs[0]) / (self.zs[1] - self.zs[0]), 0.0, len(self.zs) - 1.001)
        iy, iz = fy.astype(int), fz.astype(int)
        ty, tz = fy - iy, fz - iz
        return ((G[iy, iz] * (1 - ty) + G[iy + 1, iz] * ty) * (1 - tz)
                + (G[iy, iz + 1] * (1 - ty) + G[iy + 1, iz + 1] * ty) * tz)

    def d(self, px, py, pz):
        bx, by, bz = np.broadcast_arrays(np.asarray(px, float), np.asarray(py, float), np.asarray(pz, float))
        x = self._at(self.xd, by, bz)
        w = self._at(self.w, by, bz)
        out = np.maximum(x - bx, bx - (x + self.DEEP))
        out = np.maximum(out, (0.5 - w) * 0.02)
        inside = (by >= self.ys[0]) & (by <= self.ys[-1]) & (bz >= self.zs[0]) & (bz <= self.zs[-1])
        return np.where(inside, out, 1.0)


def drape_back(L, y_half, z0, z1, h=0.01, sink=0.002):
    """The cloth's back as it stands, read by rays from behind on a grid; then the sheet over it: a membrane held up
    by the cloth (never under it) and held at the grid's edges, relaxed until it settles. Returns a Drape (or None)."""
    ys = np.arange(-y_half, y_half + 1e-9, h)
    zs = np.arange(z0, z1 + 1e-9, h)
    Y, Z = np.meshgrid(ys, zs, indexing="ij")
    P = np.stack([np.full(Y.size, -0.45), Y.ravel(), Z.ravel()], axis=1)
    for _ in range(80):
        d = L.at_points(P)
        P[:, 0] += np.clip(d, -0.01, 0.01)
    hit = np.abs(L.at_points(P)) < 1e-3
    e = 1e-4
    g = np.stack([(L.at_points(P + np.eye(3)[i] * e) - L.at_points(P - np.eye(3)[i] * e)) / (2 * e) for i in range(3)], axis=1)
    nx = g[:, 0] / np.maximum(np.linalg.norm(g, axis=1), 1e-9)
    ok = (hit & (nx < -0.55)).reshape(Y.shape)
    xt = P[:, 0].reshape(Y.shape)
    hb = np.where(ok, -xt, np.nan)
    floor = np.nanmin(hb)
    hb = np.where(ok, hb, floor) - sink
    fixed = ~ok
    fixed[0, :] = fixed[-1, :] = True
    fixed[:, 0] = fixed[:, -1] = True
    m = hb.copy()
    for _ in range(1500):
        avg = m.copy()
        avg[1:-1, 1:-1] = (m[:-2, 1:-1] + m[2:, 1:-1] + m[1:-1, :-2] + m[1:-1, 2:]) / 4
        m = np.where(fixed, hb, np.maximum(hb, avg))
    fill = np.where(ok, m - hb, 0.0)
    print(f"  tanker: the jacket's back hung as a sheet over {ok.sum()} of {ok.size} points, filling the hollows "
          f"up to {fill.max() * 100:.1f} cm (median where it fills {np.median(fill[fill > 0.001]) * 100 if (fill > 0.001).any() else 0:.1f} cm)")
    return Drape(ys, zs, -m, ok.astype(float))


def snapshot(L):
    c = Layer(L.name + "_was", L.paint)
    c.ops = list(L.ops)
    return c


class Box:
    """An axis box as a region (negative inside)."""

    def __init__(self, lo, hi):
        self.lo, self.hi = np.asarray(lo, float), np.asarray(hi, float)
        self.c, self.R = (self.lo + self.hi) / 2, None

    def d(self, px, py, pz):
        p = np.stack(np.broadcast_arrays(np.asarray(px, float), np.asarray(py, float), np.asarray(pz, float)), axis=-1)
        q = np.abs(p - self.c) - (self.hi - self.lo) / 2
        return np.linalg.norm(np.maximum(q, 0.0), axis=-1) + np.minimum(q.max(-1), 0.0)


class Slab:
    """The region between two planes through points a and b with normal n (negative between them), cut to a box."""

    def __init__(self, a, b, n, box):
        self.a, self.b, self.n = np.asarray(a, float), np.asarray(b, float), _n(n)
        self.box = box
        self.lo, self.hi, self.c, self.R = box.lo, box.hi, box.c, None

    def d(self, px, py, pz):
        p = np.stack(np.broadcast_arrays(np.asarray(px, float), np.asarray(py, float), np.asarray(pz, float)), axis=-1)
        s1 = (p - self.b) @ self.n
        s0 = (self.a - p) @ self.n
        return np.maximum(np.maximum(s0, s1), self.box.d(px, py, pz))


def _is_box(p, half, tol=1e-6):
    return isinstance(p, RoundBox) and np.allclose(p.half, half, atol=tol)


# ================================================================================================ the American
if WHO == "usa":
    U.set_man("nco")                                   # (the squad leader's Thompson, his bursts, his magazine change)
    S.NATION_SEED[NATION] = SEED
    SPEC = S.NATION_KIT[NATION]
    SPEC["y_straps"] = False                           # (no suspenders: the pistol belt alone)
    SPEC["helmet"] = "tanker"
    S.FIGURE_PAINTS.update(NEW_PAINTS)
    S.LOOK[NATION].update({
        "tunic": "77714F",     # (the army's tunic: no longer on him; his jacket is its own paint)
        "jacket": "544E37",    # the winter combat jacket's olive drab poplin, its own paint (paint 40, jacket): the
                               # army's lighter tunic read, lit from behind, as the skin of a bare back
        "knit": "3A3428",      # its knitted collar, cuffs and waistband: dark brown-olive wool, darker than the jacket
        "trousers": "524E3D",  # olive drab wool trousers, darker than the jacket
        "helmet": "3A3229",    # the tanker's helmet: dark brown fibre and leather
        "lens": "56635E",      # the goggles' glass, smoky green-grey
    })

JACKET_HEM = 0.045         # (m: the jacket's hem under the belt's middle; the knitted waistband rises 7 cm from it)
WAISTBAND = 0.07
KNIT_OFF = 0.0032          # (m: the knit stands this far off the jacket's cloth)
SLASH = (0.0035, 0.008, 0.068)   # a slash pocket's welt (half: out, wide, long)
CUFF_AT = 0.05             # (m: the knitted cuff's middle, back from the wrist; the sleeve ends 3 cm back from it)
CUFF_LONG = (0.024, 0.019) # (m: the cuff's length toward the elbow and toward the hand from its middle)
CUFF_OFF = 0.0045          # (m: the knit stands this far off the sleeve)
SLASH_TILT = 24.0          # (degrees from upright, the top toward his middle)
BACK_DRAPE = (0.15, 0.0)
COLLAR_UP = 0.042          # (m: the knitted collar's top over the neck ring's middle: it stands 6.8 cm)
YOKE = (0.05, 0.155, 0.002, 0.0065, 0.0095)   # (m: the yoke's seam this far under the shoulders, this far either side; a
#                                               groove, its middle this far out, this wide; the yoke's welt this far over it)
WAIST_FOLDS = ((0.012, 0.15, 0.006), (0.04, 0.145, 0.009), (0.068, 0.13, 0.008))   # (m over the waistband, half width, how much they wander)   # (m: the back hung as a sheet this far either side of his middle, up to this far over the shoulders)

_uniform_usa = S.uniform_layers


def uniform_layers(man, J, spec, LM):
    if WHO != "usa" or not U._ours(spec):
        return _uniform_usa(man, J, spec, LM)
    LM["hem"] = LM["belt"] - JACKET_HEM
    layers = _uniform_usa(man, J, spec, LM)
    tunic, trousers, collar, cuffs = layers[0], layers[1], layers[3], layers[4]
    tunic.paint = "jacket"
    # ---- off: soldier_usa.py's shoulder straps (the jacket has none) and so the buttons on them
    before = len(tunic.ops)
    tunic.ops = [(how, p, k) for how, p, k in tunic.ops
                 if not (how == "add" and isinstance(p, RoundBox) and abs(p.half[1] - 0.021) < 1e-6 and abs(p.half[2] - 0.0042) < 1e-6)]
    J["usa_shoulder_straps"] = []
    LM["shoulder_straps"] = []
    print(f"  tanker: {before - len(tunic.ops)} pieces of the field jacket's shoulder straps taken off")
    # ---- and its buttoned fly down the front (the zip is under a plain front: the strip read as a crease at the chest)
    before = len(tunic.ops)
    tunic.ops = [(how, p, k) for how, p, k in tunic.ops
                 if not (how == "add" and isinstance(p, RoundBox) and abs(p.half[0] - 0.006) < 1e-6 and abs(p.half[1] - 0.014) < 1e-6)]
    print(f"  tanker: {before - len(tunic.ops)} shapes of the fly taken off")
    # ---- the collar knitted: a stand of ribbed wool round the neck, open at the throat where the zip ends
    nmid, nfwd, nrx, nry = LM["neck_ring"]
    J["tanker_neck"] = LM["neck_ring"]
    ax = LM["neck_axis"]
    Rn = np.stack([nfwd, np.cross(ax, nfwd), ax], axis=1)
    collar.ops = []
    collar.paint = "knit"
    collar.add(Tube(nmid, -0.026, COLLAR_UP, (nrx + 0.019, nry + 0.018), (nrx + 0.013, nry + 0.013), 0.01, Rn))
    collar.add(Torus(nmid + ax * COLLAR_UP, nrx + 0.009, 0.0065, Rn, major_y=nry + 0.009), 0.006)
    collar.sub(RoundBox(nmid + nfwd * (nrx + 0.022) + ax * 0.03, (0.03, 0.007, 0.05), 0.004, Rn), 0.002)
    # ---- the cuffs knitted: longer and close round the wrist
    cuffs.paint = "knit"
    cuffs.ops = []
    for s in ("l", "r"):
        el, wr = np.array(J["lowerarm_" + s]), np.array(J["hand_" + s])
        fa = _n(wr - el)
        Rc = S.ring_frame(fa)
        cc = wr - fa * CUFF_AT
        # (each end the sleeve's own width there and a little: a cuff only 1 mm off the cloth at one end came through
        # it in a saw-tooth, and one as wide as the forearm all along swallowed the back of his hand)
        ra = S.ring_axes(tunic, cc - fa * CUFF_LONG[0], Rc)
        rb = S.ring_axes(tunic, cc, Rc)
        cuffs.add(Tube(cc, -CUFF_LONG[0], CUFF_LONG[1], (ra[0] + CUFF_OFF, ra[1] + CUFF_OFF),
                       (rb[0] + CUFF_OFF, rb[1] + CUFF_OFF), 0.005, Rc))
    # ---- the back hung loose: with no suspenders or pack over it the jacket laid on his body showed his shoulder
    # blades and his spine through it (a camera behind the tank read it as a bare back); the poplin hangs from the
    # blades instead, over the spine and the small of his back, down to the waistband
    if BACK_DRAPE:
        dr = drape_back(tunic, BACK_DRAPE[0], LM["hem"] + WAISTBAND - 0.005, float(LM["shoulder"]) + BACK_DRAPE[1])
        if dr is not None:
            tunic.add(dr, 0.006)
    # ---- the yoke's seam straight across the back under the shoulders, and the poplin bloused in soft folds over
    # the waistband (a sheet of khaki with neither read as skin)
    def across(z, ys, wave=0.0):
        pts = []
        for i, y in enumerate(ys):
            zz = z + wave * math.sin(i * 1.9)
            try:
                p, nrm = on_surface(tunic, (-0.45, y, zz), (1.0, 0.0, 0.0))
            except RuntimeError:
                continue
            if nrm[0] < -0.3:
                pts.append((p, nrm))
        return pts
    if YOKE:
        pts = across(float(LM["shoulder"]) - YOKE[0], np.linspace(-YOKE[1], YOKE[1], 13))
        up = np.array((0.0, 0.0, 1.0))
        for (a, na), (b, nb) in zip(pts[:-1], pts[1:]):
            tunic.sub(RoundCone(a + na * YOKE[2], b + nb * YOKE[2], YOKE[3], YOKE[3]), 0.003)
            # (the yoke laid over the back panel: its edge a welt standing just over the seam)
            tunic.add(RoundCone(a + up * YOKE[4] - na * 0.002, b + up * YOKE[4] - nb * 0.002, 0.005, 0.005), 0.004)
        print(f"  tanker: the yoke's seam across his back, {len(pts)} points")
    for up, half, wave in WAIST_FOLDS:
        pts = across(LM["hem"] + WAISTBAND + up, np.linspace(-half, half, 11), wave)
        for (a, na), (b, nb) in zip(pts[:-1], pts[1:]):
            tunic.add(RoundCone(a - na * 0.003, b - nb * 0.003, 0.009, 0.009), 0.012)
    # ---- the waistband knitted: the jacket's own surface stood out round the waist from the hem up
    was = snapshot(tunic)
    band = Layer("waistband", "knit")
    z0 = LM["hem"]
    band.add(Over(was, KNIT_OFF, Box((-0.26, -0.24, z0 - 0.004), (0.26, 0.24, z0 + WAISTBAND))), 0.0)
    layers.append(band)
    # ---- the two slash pockets on the front over the hips, above the waistband: a welt on the cloth, slanted
    Z = LM["Z"]
    for side in (1.0, -1.0):
        p, nrm = on_surface(tunic, np.array((0.4, 0.1 * side, z0 + WAISTBAND + SLASH[2] + 0.012)), (-1.0, -0.3 * side, 0.0))
        R = frame_from(nrm, (0.0, 0.0, 1.0)) @ rot_euler(-SLASH_TILT * side, 0, 0)
        tunic.add(RoundBox(p, SLASH, 0.003, R), 0.003)
    LM["pockets"] = []
    print(f"  tanker: jacket to {LM['hem']:.3f} m (the belt at {LM['belt']:.3f}), knitted collar, cuffs and waistband")
    return layers


S.uniform_layers = uniform_layers
S.BUDGET["uniform"] = (2450 + 50) if WHO != "usa" else 3600   # (the American's: finer, so the knit's edges, the yoke and
#                                                                  the folds stay lines at a camera's close shot)

_kit_usa = S.build_kit


def build_kit(J, spec, body_ls, Z=None):
    out = _kit_usa(J, spec, body_ls, Z)
    if WHO != "usa" or not U._ours(spec):
        return out
    Z = Z or (lambda z: z)
    tunic = body_ls[0]
    for k in ("pack", "shovel", "bottle", "details", "straps"):
        if k in out:
            bpy.data.objects.remove(out.pop(k)[0])
    # ---- the staff sergeant's chevrons on the sleeves (soldier_usa.py's, laid again: its details piece went above)
    g = S.Geo()
    U._chevrons(g, tunic, J)
    out["details"] = (g.build("details"), None)
    return out


S.build_kit = build_kit


# ---- the tanker's helmet of 1938: soldier.py's helmet made close round the skull, its edge low over the ears and the
# nape and high on the brow, then the padded ribs over the crown (front to back, meeting at the brow band and the
# nape), the brow band, the ear flaps with their earphones and the goggles on the front with their strap round it
S.HELMETS["tanker"] = dict(ax=0.114, ay=0.104, az=0.106, dx=-0.004, n=2.2, edge=S._cos3(0.04, -0.072, -0.05),
                           flare=S._cos3(0.004, 0.003, 0.004), drop=S._cos3(0.004, 0.003, 0.004))
RIBS = (-46.0, -28.0, -10.0, 10.0, 28.0, 46.0)    # (degrees: each rib's plane, tilted about the front-to-back line)
RIB = (0.0165, 0.0085)                           # (its width and how far it stands proud)
EARCUP = (0.034, 0.016)                          # (the earphone's housing: radius, how far it stands off the flap)
LENS = (0.026, 0.012)                            # (each goggle lens: radius, how far its frame stands off)
_build_helmet = S.build_helmet


def _dome(J):
    """The tanker's helmet's dome as soldier.build_helmet lays it on this man: its middle and three radii."""
    k = dict(S.HELMETS["tanker"])
    C0 = np.array((J["skull_x"] - 0.004 + k["dx"], 0.0, J["crown"] - 0.108))
    az = k["az"] + 0.013
    ay = k["ay"] * (J["skull_half"][1] + 0.025) / 0.108
    ax = k["ax"] * (J["skull_half"][0] + 0.023) / 0.122
    return C0, ax, ay, az, k


def _dome_point(C0, ax, ay, az, nexp, f, a):
    """soldier.build_helmet's dome at bearing f (0 ahead) and elevation parameter a (pi/2 the top; below 0, the
    straight wall under the widest)."""
    rho = 1.0 / math.sqrt((math.cos(f) / ax) ** 2 + (math.sin(f) / ay) ** 2)
    if a >= 0.0:
        rh, z = rho * math.cos(a) ** (2.0 / nexp), az * math.sin(a) ** (2.0 / nexp)
    else:
        rh, z = rho, rho * math.tan(a)
    return C0 + np.array((math.cos(f) * rh, math.sin(f) * rh, z))


def _on_dome(C0, ax, ay, az, nexp, d):
    """Where the direction d from the dome's middle meets it, and the dome's outward normal there."""
    d = np.asarray(d, float)
    f = math.atan2(d[1], d[0])
    elev = math.atan2(d[2], math.hypot(d[0], d[1]))
    if elev <= 0.0:
        a = elev
    else:
        lo, hi = 0.0, math.pi / 2
        for _ in range(40):
            mid = (lo + hi) / 2
            q = _dome_point(C0, ax, ay, az, nexp, f, mid) - C0
            if math.atan2(q[2], math.hypot(q[0], q[1])) < elev:
                lo = mid
            else:
                hi = mid
        a = (lo + hi) / 2
    p = _dome_point(C0, ax, ay, az, nexp, f, a)
    h = 1e-3
    a0, a1 = (a - h, a + h) if a < math.pi / 2 - h else (a - 2 * h, a)
    tf = _dome_point(C0, ax, ay, az, nexp, f + h, a) - _dome_point(C0, ax, ay, az, nexp, f - h, a)
    ta = _dome_point(C0, ax, ay, az, nexp, f, a1) - _dome_point(C0, ax, ay, az, nexp, f, a0)
    n = np.cross(tf, ta)
    if np.linalg.norm(n) < 1e-9:
        n = p - C0
    n = _n(n)
    if n @ (p - C0) < 0:
        n = -n
    return p, n


def _at_height(C0, ax, ay, az, nexp, f, z):
    """The dome at bearing f and height z (over its middle), and its normal."""
    hz = (z - C0[2]) / az
    if hz <= 0.0:
        rho = 1.0 / math.sqrt((math.cos(f) / ax) ** 2 + (math.sin(f) / ay) ** 2)
        return C0 + np.array((math.cos(f) * rho, math.sin(f) * rho, z - C0[2])), np.array((math.cos(f), math.sin(f), 0.0))
    a = math.asin(min(hz, 0.999) ** (nexp / 2.0))
    p = _dome_point(C0, ax, ay, az, nexp, f, a)
    return _on_dome(C0, ax, ay, az, nexp, p - C0)


def tanker_helmet(J, sides=None):
    ob = _build_helmet("tanker", J, sides)
    C0, ax, ay, az, k = _dome(J)
    nexp = k.get("n", 2.0)
    edge = k["edge"]
    g = S.Geo()
    # ---- the ribs: padded rolls over the crown from the brow band to the nape, each in a plane through the dome's
    # front-to-back line tilted to the side, so they meet toward the front and the back
    for b in RIBS:
        beta = math.radians(b)
        up = np.array((0.0, math.sin(beta), math.cos(beta)))
        pts, nrm = [], []
        for u in np.linspace(math.radians(20.0), math.radians(156.0), 15):
            p, n = _on_dome(C0, ax, ay, az, nexp, np.array((math.cos(u), 0.0, 0.0)) + up * math.sin(u))
            f = math.atan2(p[1] - C0[1], p[0] - C0[0])
            if p[2] < C0[2] + edge(f) + 0.026:
                continue
            pts.append(p - n * 0.002)
            nrm.append(n)
        if len(pts) >= 3:
            g.cord(pts, nrm, RIB[0], RIB[1] + 0.002, "helmet", sides=6)
    # ---- the brow band: a padded roll round the front edge, from temple to temple
    pts, nrm = [], []
    for f in np.linspace(-1.25, 1.25, 15):
        p, n = _at_height(C0, ax, ay, az, nexp, f, C0[2] + edge(f) + 0.014)
        pts.append(p - n * 0.003)
        nrm.append(n)
    g.cord(pts, nrm, 0.022, 0.013, "helmet", sides=6)
    # ---- the ear flaps' earphones: a round housing over each ear
    e = np.array(J["eye_l"])
    ear = np.array((e[0] - 0.084, 0.0, e[2] - 0.028))
    for side in (1.0, -1.0):
        f = side * math.pi / 2
        rho = 1.0 / math.sqrt((math.cos(f) / ax) ** 2 + (math.sin(f) / ay) ** 2)
        a = np.array((ear[0], side * (rho - 0.002), ear[2]))
        b = a + np.array((0.0, side * EARCUP[1], 0.0))
        g.tube(a, b, EARCUP[0], EARCUP[0] - 0.004, "helmet", sides=12, up=(1.0, 0.0, 0.0))
        g.tube(b, b + np.array((0.0, side * 0.004, 0.0)), EARCUP[0] - 0.012, EARCUP[0] - 0.014, "helmet", sides=12, up=(1.0, 0.0, 0.0))
    # ---- the goggles: two round lenses in their frames on the front, over the brow band, and the strap round the
    # helmet behind them
    zg = C0[2] + edge(0.0) + 0.036
    for side in (1.0, -1.0):
        p, n = _at_height(C0, ax, ay, az, nexp, side * 0.36, zg)
        g.tube(p - n * 0.003, p + n * LENS[1], LENS[0] + 0.005, LENS[0] + 0.004, "helmet", sides=14, up=(0.0, 0.0, 1.0))
        g.tube(p + n * (LENS[1] - 0.001), p + n * (LENS[1] + 0.0015), LENS[0], LENS[0] - 0.001, "lens", sides=14, up=(0.0, 0.0, 1.0))
    (pa, na), (pb, nb) = (_at_height(C0, ax, ay, az, nexp, f, zg) for f in (0.1, -0.1))
    g.tube(pa + na * 0.007, pb + nb * 0.007, 0.0045, 0.0045, "helmet", sides=6)
    pts, nrm = [], []
    for f in np.linspace(0.62, 2 * math.pi - 0.62, 21):
        p, n = _at_height(C0, ax, ay, az, nexp, f, zg + 0.004 * math.cos(f))
        pts.append(p + n * 0.004)
        nrm.append(n)
    g.band(pts, nrm, 0.02, 0.004, "helmet")
    extra = g.build("helmet_tanker_extra", sharp=50.0)
    print(f"  tanker: the helmet {S.tris_of(ob)} triangles and its ribs, flaps and goggles {S.tris_of(extra)}")
    return S.join_meshes([ob, extra], ob.name.split(".")[0])


def build_helmet(kind, J, sides=None):
    if kind == "tanker" and "crown" in J:
        return tanker_helmet(J, sides)
    return _build_helmet(kind, J, sides)


S.build_helmet = build_helmet


# ================================================================================================ the tank clips
# tank_crew.py's clips (crew.py's way, solved here on this man's own rig) put in soldier.py's clip table under the
# names the game asks for, the weapon put away: the gun 2 m under his feet (out of sight under the tank, as the
# turret's floor is) and in the glb also scaled to nothing (crew.py's way); `weapon` "none" in the json.
TANK_CLIPS = {
    "usa": {"commander_hatch": "commander_hatch", "commander_roof_mg": "commander_roof_mg_us",
            "commander_roof_mg_burst": "commander_roof_mg_us_burst", "die_hatch_slump": "die_hatch_slump",
            "die_hatch_drop": "die_hatch_drop"},
    "ger": {"commander_hatch": "commander_hatch", "commander_roof_mg": "commander_roof_mg_ger",
            "commander_roof_mg_burst": "commander_roof_mg_ger_burst", "die_hatch_slump": "die_hatch_slump",
            "die_hatch_drop": "die_hatch_drop"},
}[WHO]
TANKS_OF = {"usa": ("Sherman",), "ger": ("PanzerIV", "Tiger")}[WHO]
GUN_AWAY = M4(np.eye(3), (0.2, 0.0, -2.0))
_clip_state = S.clip_state


def _tank_fn(src):
    def fn(t):
        P = C.CREW[src]["fn"](t)
        P["weapon"] = GUN_AWAY
        return P
    return fn


def clip_state(rig, name, t, kind=None):
    if name not in TANK_CLIPS:
        return _clip_state(rig, name, t, kind)
    keep = rig.fit
    rig.fit = (1.0, 1.0)                    # (the crew's poses are set out on the man as he is: nothing scaled)
    try:
        return _clip_state(rig, name, t, kind)
    finally:
        rig.fit = keep


S.clip_state = clip_state


def use_man(m):
    """crew.py's clips measure the man they are played on (his ankle, his eyes, his reach): this one."""
    C.MODEL.update(rig=m["rig"], man=m)
    C.measure_man(m["rig"])
    if C.MODEL.get("skin") is None or C.MODEL["skin"] is not m.get("skin"):
        m["skin"] = C.Skin(m)
        C.MODEL["skin"] = m["skin"]
    TC.fit_roof()                            # (where he stands at the German roof gun: his shoulder to its butt)


def add_tank_clips():
    for name, src in TANK_CLIPS.items():
        c = C.CREW[src]
        S.CLIPS[name] = {"long": c["long"], "loop": c["loop"], "stride": 0.0, "events": list(c["events"]),
                         "fn": _tank_fn(src), "weapon": "rifle"}


def hide_gun_in_tank_clips(m):
    """In the glb, the weapon bone keyed to scale 0 through the tank clips (crew.py's way: the gun is put away)."""
    arm = m["arm"]
    pb = arm.pose.bones["weapon"]
    for name in TANK_CLIPS:
        act = m["actions"][name]
        arm.animation_data.action = act
        N = int(round(S.CLIPS[name]["long"] * S.FPS))
        pb.scale = (0.0, 0.0, 0.0)
        for f in (0, N):
            pb.keyframe_insert("scale", frame=f, group="weapon")
        pb.scale = (1.0, 1.0, 1.0)
    arm.animation_data.action = None
    S.reset_pose(arm)


PLACES_ABOUT = ("where each clip goes: the tank's `commander` socket moved by root_offset (forward, right, up of the "
                "socket's facing, the game's axes); the clip's root (the floor at his feet) goes there")


def tank_json(data, rig):
    """The tank clips in the json: weapon none, where they go (each of his tanks' commander socket), what they are,
    and the field glasses' track (crew.py's binoculars prop)."""
    for name, src in TANK_CLIPS.items():
        c = C.CREW[src]
        d = data["clips"][name]
        d["weapon"] = "none"
        d["anchor"] = c["anchor"]
        d["about"] = c["about"]
        d["from"] = f"tank_crew.py's {src}, solved on this man"
        d["places"] = [{"kind": k, "piece": p, "socket": s, "root_offset": [round(float(x), 4) for x in off]}
                       for k, p, s, off in c["place"] if p in TANKS_OF]
        N = int(round(c["long"] * C.FPS))
        tracks = {}
        for f in range(0, N + 1, C.FIT_N):
            t = 0.0 if (c["loop"] and f == N) else f / C.FPS
            P = c["fn"](t)
            for pn, v in P.get("props", {}).items():
                M, shown = v if isinstance(v, tuple) else (v, True)
                tracks.setdefault(pn, []).append([round(f / C.FPS, 4)] + C.game_matrix(M) + [1 if shown else 0])
        if tracks:
            d["props"] = {pn: {"mesh": "crew/props/%s.glb" % pn, "about": C.PROP_ABOUT.get(pn, ""),
                               "track_fields": ["t", "x", "y", "z", "qw", "qx", "qy", "qz", "shown"], "track": tr}
                          for pn, tr in tracks.items()}
    data["tank_clips"] = {"names": list(TANK_CLIPS), "places": PLACES_ABOUT,
                          "weapon_none": "the gun is put away: its bone (weapon, and the bolt and bipod on it) is 2 m "
                                         "under his feet, out of sight under the tank; do not draw the triangles on "
                                         "those bones while such a clip plays",
                          "props": "a clip's `props` track (30 a second, in the clip's root frame, the game's axes) "
                                   "places the prop's mesh (battle-models/land/soldier/crew/props/)",
                          "hide": "die_hatch_drop's event `hide`: stop drawing him (he is down inside the turret)"}
    return data


# ================================================================================================ the German
if WHO == "ger":
    S.NATION_SEED[NATION] = SEED
    SPEC = S.NATION_KIT[NATION]
    SPEC.update({"helmet": "sidecap", "tall_boots": False, "y_straps": False, "pouches": {"front": 0, "rear": False},
                 "canister": False, "pack": False, "roll": False})
    S.FIGURE_PAINTS.update(NEW_PAINTS)
    S.LOOK[NATION].update({
        "tunic": "232427",     # the Panzer wrap's black wool, a little faded
        "trousers": "212225",  # its trousers
        "lower": "212225",     # (the trousers gathered over the ankle boots: soldier.py's gaiters in the trousers' black)
        "boots": "1E1A17",     # black ankle boots
        "straps": "1C1916",    # the black leather belt
        "helmet": "1F2023",    # the black side cap
        "shirt": "6B6D68",     # the field-grey shirt under the wrap
        "phones": "151515",    # the headphones' black housings and band
        "leather": "26201B",
    })

WRAP_HEM = 0.04            # (m: the wrap's hem under the belt's middle: it ends at the waist)
V_TIP = 1.285              # (sculpted height: where the lapels meet, over the breastbone)
LAPEL_OFF = 0.009          # (m: the lapels and the collar stand this far off the jacket's cloth: a thick wool, its
#                            edge the only line that tells them on black)
SHIRT_OFF = 0.0035         # (m: the shirt seen in the opening, over the jacket's own surface there: any closer and
#                            the jacket's cloth came through it in a saw-tooth)
GER_POCKETS = [(0.006, 0.05, 0.058), (0.006, 0.06, 0.07), (0.0075, 0.054, 0.018), (0.0075, 0.064, 0.018)]


class Union:
    """Several regions as one (negative inside any)."""

    def __init__(self, *rs):
        self.rs = rs
        self.lo = np.min([r.lo for r in rs], axis=0)
        self.hi = np.max([r.hi for r in rs], axis=0)
        self.c, self.R = (self.lo + self.hi) / 2, None

    def d(self, px, py, pz):
        return np.minimum.reduce([r.d(px, py, pz) for r in self.rs])


class Wedge:
    """The opening's region on the chest: ahead of the plane x = x0, between heights z0 and z1, and within a width
    that grows from w0 at z0 to w1 at z1, offset by `inner` (negative inside; a band from inner to inner + width
    when `width` is given, on one side)."""

    def __init__(self, x0, z0, z1, w0, w1, side=0.0, width=None):
        self.x0, self.z0, self.z1, self.w0, self.w1, self.side, self.width = x0, z0, z1, w0, w1, side, width
        self.lo = np.array((x0 - 0.02, -0.3, z0 - 0.02))
        self.hi = np.array((0.4, 0.3, z1 + 0.02))
        self.c, self.R = (self.lo + self.hi) / 2, None

    def d(self, px, py, pz):
        x, y, z = np.broadcast_arrays(np.asarray(px, float), np.asarray(py, float), np.asarray(pz, float))
        u = np.clip((z - self.z0) / (self.z1 - self.z0), 0.0, 1.0)
        e = self.w0 + (self.w1 - self.w0) * u
        if self.width is None:
            dy = np.abs(y) - e
        else:
            yy = y * self.side
            wd = self.width[0] + (self.width[1] - self.width[0]) * u
            dy = np.maximum(e - yy, yy - (e + wd))
        return np.maximum.reduce([dy, self.z0 - z, z - self.z1, self.x0 - x])


_uniform_prev = S.uniform_layers


def uniform_layers_ger(man, J, spec, LM):
    if WHO != "ger" or spec is not S.NATION_KIT["GER"]:
        return _uniform_prev(man, J, spec, LM)
    LM["hem"] = LM["belt"] - WRAP_HEM
    layers = _uniform_prev(man, J, spec, LM)
    tunic, collar = layers[0], layers[3]
    Z = LM["Z"]
    # ---- off: the field tunic's four pockets and their flaps, and its button strip (the wrap has none outside)
    before = len(tunic.ops)
    tunic.ops = [(how, p, k) for how, p, k in tunic.ops
                 if not (how == "add" and isinstance(p, RoundBox) and (any(_is_box(p, h) for h in GER_POCKETS) or
                         (abs(p.half[0] - 0.006) < 1e-6 and abs(p.half[1] - 0.014) < 1e-6)))]
    print(f"  panzer: {before - len(tunic.ops)} shapes of the field tunic's front taken off (pockets, flaps, buttons)")
    nmid, nfwd, nrx, nry = LM["neck_ring"]
    J["tanker_neck"] = LM["neck_ring"]
    ax = LM["neck_axis"]
    Rn = np.stack([nfwd, np.cross(ax, nfwd), ax], axis=1)
    # ---- the shirt's collar: a low stand round the neck, open at the throat
    collar.ops = []
    collar.paint = "shirt"
    collar.add(Tube(nmid, -0.022, 0.02, (nrx + 0.012, nry + 0.011), (nrx + 0.008, nry + 0.008), 0.006, Rn))
    collar.sub(RoundBox(nmid + nfwd * (nrx + 0.018) + ax * 0.02, (0.03, 0.012, 0.04), 0.004, Rn), 0.003)
    # ---- the opening: the shirt seen between the lapels, from the collar down to where they meet
    was = snapshot(tunic)
    z_tip, z_top = Z(V_TIP), float(nmid[2]) + 0.01
    x0 = float(nmid[0]) + 0.01
    shirt = Layer("shirt", "shirt")
    shirt.add(Over(was, SHIRT_OFF, Wedge(x0, z_tip, z_top + 0.02, 0.0, nry + 0.012)), 0.0)
    # ---- the lapels, wide, laid back from the opening toward the shoulders, and the big collar round behind the neck
    # over the shoulders' tops: the jacket's own surface stood out (a thick wool), so they follow it
    lap = Layer("lapels", "tunic")
    lapels = [Wedge(x0 - 0.02, z_tip - 0.006, z_top, 0.0, nry + 0.012, side=s_, width=(0.03, 0.085)) for s_ in (1.0, -1.0)]
    back = Box((float(nmid[0]) - 0.16, -(nry + 0.075), z_top - 0.05), (x0 - 0.005, nry + 0.075, z_top + 0.012))
    lap.add(Over(was, LAPEL_OFF, Union(*lapels, back)), 0.0)
    # ---- the edge of the wrap's front panel: from where the lapels meet, down across him to the hem at his right
    pts = []
    for u in np.linspace(0.0, 1.0, 7):
        z = z_tip + (LM["hem"] + 0.035 - z_tip) * u
        y = -0.095 * u ** 1.2
        try:
            p, nrm = on_surface(tunic, np.array((0.4, y, z)), (-1.0, 0.0, 0.0))
        except RuntimeError as err:                    # (a point the walk cannot settle on is left out, not guessed)
            print(f"  panzer: front edge point {u:.2f} left out ({err})")
            continue
        pts.append((p, nrm))
    for (a, na), (b, nb) in zip(pts[:-1], pts[1:]):
        tunic.add(RoundCone(a - na * 0.0035, b - nb * 0.0035, 0.0055, 0.0055), 0.006)
    layers += [shirt, lap]
    LM["pockets"] = []
    print(f"  panzer: wrap to {LM['hem']:.3f} m, the opening from {z_tip:.3f} to {z_top:.3f}, lapels and collar laid over")
    return layers


S.uniform_layers = uniform_layers_ger

_kit_prev = S.build_kit
HOLSTER_AT = 1.62          # (radians round the waist from straight ahead, to his left: the pistol at his left hip)


def build_kit_ger(J, spec, body_ls, Z=None):
    out = _kit_prev(J, spec, body_ls, Z)
    if WHO != "ger" or spec is not S.NATION_KIT["GER"]:
        return out
    Z = Z or (lambda z: z)
    tunic = body_ls[0]
    for k in ("details", "bread_bag", "bottle", "shovel", "canister", "pouches"):
        if k in out:
            bpy.data.objects.remove(out.pop(k)[0])
    # ---- the pistol in its black holster at his left hip, hung from the belt, the flap over its butt
    zb = Z(1.0)
    cx = J["spine_01"][0] + 0.03
    a = HOLSTER_AT
    (p0,), (n0,) = S.project(tunic, [(cx + math.cos(a) * 0.2, math.sin(a) * 0.2, zb)])
    n0 = _n(np.array((n0[0], n0[1], 0.0)))
    R = frame_from(n0, (0.0, 0.0, 1.0)) @ rot_euler(12.0, 0, 0)
    c = np.array((p0[0], p0[1], zb - 0.062)) + n0 * 0.04
    L = Layer("holster", "straps")
    L.add(RoundBox(c, (0.018, 0.05, 0.058), 0.013, R))
    L.add(RoundBox(c + R @ np.array((0.0, 0.012, -0.06)), (0.015, 0.026, 0.03), 0.01, R), 0.01)
    flap = Layer("holster_flap", "straps")
    flap.add(RoundBox(c + R @ np.array((0.004, 0.0, 0.044)), (0.018, 0.054, 0.026), 0.012, R), 0.0)
    loop = Layer("holster_loop", "straps")
    loop.add(RoundBox(np.array((p0[0], p0[1], zb)) + n0 * 0.012, (0.006, 0.03, 0.03), 0.003, frame_from(n0, (0.0, 0.0, 1.0))))
    out["holster"] = (S.mesh_from_layers("holster", [L, flap, loop], 0.0025, 220), None)
    J.setdefault("pouch_pivot_l", tuple(float(v) for v in np.array((p0[0], p0[1], zb - 0.01)) + n0 * 0.012))
    J.setdefault("pouch_pivot_r", tuple(float(v) for v in np.array((p0[0], -p0[1], zb - 0.01)) + n0 * np.array((1, -1, 1)) * 0.012))
    J.setdefault("waist_centre", (float(cx), 0.0, float(zb)))          # (the pouch bones' seat reads it: none ride them)
    return out


S.build_kit = build_kit_ger


# ---- the black side cap (a boat-shaped cloth cap, its curtain turned up all round) with the radio headphones over it
CAP_H = 0.068              # (m: the cap's ridge over its band, at its middle)
CAP_ENDS = 0.4             # (the ridge falls this share of CAP_H toward the front and back ends: a boat, not a tray)


def cap_h(u):
    """The ridge's height over the band at u (-1 the back end, 0 the middle, 1 the front end)."""
    return CAP_H * (1.0 - CAP_ENDS * np.abs(u) ** 3)
CAP_TENT = (1.6, 0.75)     # (its cross-section: z = band + CAP_H (1 - c^a)^b, c from the ridge (0) to the band (1): a
#                            tent rounded at the ridge, its sides sloped in, not a box)
CAP_SIDES = 16             # (points round each cross-section's arch)
CAP_RINGS = 15             # (cross-sections front to back)
PHONES = (0.034, 0.024)    # (each earphone's housing: radius, depth)
CURTAIN = 0.03             # (m: the turned-up curtain's fold over the band)


def _skin_top(z_from):
    """The man's skin above a height, from his body (made before his helmet): the points a cap must clear."""
    body = next(o for o in bpy.data.objects if o.name.startswith("body_"))
    me = body.data
    names = [mt.name.split(".")[0] for mt in me.materials]
    keep = set()
    for p in me.polygons:
        if names[p.material_index] == "skin":
            keep.update(p.vertices)
    M = np.array(body.matrix_world)
    P = np.array([tuple(me.vertices[i].co) for i in keep])
    P = P @ M[:3, :3].T + M[:3, 3]
    return P[P[:, 2] > z_from]


def sidecap(J, sides=None):
    crown = J["crown"]
    sx = J["skull_x"]
    hx, hy = J["skull_half"]
    Lx, Wy = hx + 0.016, hy + 0.004
    zb = crown - 0.048
    ta, tb = CAP_TENT
    top = _skin_top(crown - 0.09)

    def tent(c):
        return (1.0 - np.clip(c, 0.0, 1.0) ** ta) ** tb

    def cap_z(x, y, zb_, Wy_):
        u = np.clip((x - sx) / Lx, -0.999, 0.999)
        w = Wy_ * np.sqrt(1.0 - u ** 2)
        c = np.clip(np.abs(y) / w, 0.0, 1.0)
        h = cap_h(u)
        return zb_ + h * tent(c), w
    # (seated as low as the skull lets it: every skin point under it at least 5 mm under its cloth, and as wide as the
    # head where its band sits)
    for _ in range(6):
        inside = top[top[:, 2] > zb]
        u = (inside[:, 0] - sx) / Lx
        out = (u ** 2 + (inside[:, 1] / Wy) ** 2)
        if out.max() > 0.96:
            Wy *= math.sqrt(out.max() / 0.96)
        zc, w = cap_z(inside[:, 0], inside[:, 1], zb, Wy)
        need = float(np.max(inside[:, 2] + 0.005 - zc))
        if need > 0:
            zb += need
    print(f"  panzer: the side cap {2 * Lx:.3f} long, {2 * Wy:.3f} wide, its band at {zb:.3f} ({crown - zb:.3f} under the crown)")
    rings = []
    for i in range(CAP_RINGS):
        u = -0.985 + 1.97 * i / (CAP_RINGS - 1)
        x = sx + u * Lx
        w = Wy * math.sqrt(1.0 - u * u)
        h = cap_h(u)
        ring = []
        for k in range(CAP_SIDES + 1):
            ph = math.pi * k / CAP_SIDES
            y = w * math.cos(ph)
            z = zb + h * float(tent(abs(math.cos(ph))))
            ring.append(np.array((x, y, z)))
        for k in range(1, 4):                       # (and back across under the band, inside his head)
            ring.append(np.array((x, w - 2 * w * k / 4, zb - 0.004)))
        rings.append(ring)
    g = S.Geo()
    g.shell([[np.array((sx - Lx, 0.0, zb + 0.035))]] + rings + [[np.array((sx + Lx, 0.0, zb + 0.035))]], "helmet")
    # ---- the curtain's top edge: a fold round the cap, 3 cm over its band, on the sloping side
    pts, nrm = [], []
    for i in range(CAP_RINGS):
        u = -0.95 + 1.9 * i / (CAP_RINGS - 1)
        x = sx + u * Lx
        w = Wy * math.sqrt(1.0 - u * u)
        h = cap_h(u)
        hf = min(CURTAIN, 0.9 * h)
        cf = (1.0 - (hf / h) ** (1.0 / tb)) ** (1.0 / ta)
        pts.append(np.array((x, w * cf + 0.0015, zb + hf)))
        nrm.append(_n(np.array((0.0, 1.0, 0.35))))
    for side in (1.0, -1.0):
        P = np.array(pts) * np.array((1.0, side, 1.0))
        N = np.array(nrm) * side
        g.cord(P, N, 0.006, 0.003, "helmet", sides=4)
    # ---- the headphones: a black housing over each ear, a sprung band over the cap's top between them
    e = np.array(J["eye_l"])
    ear = np.array((e[0] - 0.084, 0.0, e[2] - 0.028))
    near_ear = _skin_top(ear[2] - 0.03)
    sel = near_ear[(np.abs(near_ear[:, 2] - ear[2]) < 0.025) & (np.abs(near_ear[:, 0] - ear[0]) < 0.03)]
    ey = float(np.abs(sel[:, 1]).max()) if len(sel) else hy + 0.02
    zt = float(cap_z(np.array([ear[0]]), np.array([0.0]), zb, Wy)[0][0]) + 0.007
    for side in (1.0, -1.0):
        a = np.array((ear[0], side * (ey + 0.002), ear[2]))
        b = a + np.array((0.0, side * PHONES[1], 0.0))
        g.tube(a, b, PHONES[0], PHONES[0] - 0.004, "phones", sides=14, up=(1.0, 0.0, 0.0))
        g.tube(a - np.array((0.0, side * 0.003, 0.0)), a + np.array((0.0, side * 0.004, 0.0)), PHONES[0] + 0.003, PHONES[0] + 0.003,
               "phones", sides=14, up=(1.0, 0.0, 0.0))
    Yc = ey + PHONES[1] * 0.5
    z0 = ear[2] + PHONES[0] - 0.006
    bpts, bnrm = [], []
    for th in np.linspace(0.0, math.pi, 17):
        y = Yc * math.cos(th)
        z = z0 + (zt - z0) * math.sin(th) ** 0.55
        zc = float(cap_z(np.array([ear[0]]), np.array([y]), zb, Wy)[0][0]) if abs(y) < Wy else -1.0
        z = max(z, zc + 0.006)
        bpts.append(np.array((ear[0], y, z)))
        bnrm.append(_n(np.array((0.0, math.cos(th) * 0.8, math.sin(th)))))
    g.band(bpts, bnrm, 0.016, 0.0035, "phones")
    ob = g.build("helmet_sidecap", sharp=55.0)
    print(f"  panzer: the cap and the headphones {S.tris_of(ob)} triangles (the ears at {ey:.3f} out)")
    return ob


_build_helmet2 = S.build_helmet


def build_helmet_ger(kind, J, sides=None):
    if kind == "sidecap" and "crown" in J:
        return sidecap(J, sides)
    return _build_helmet2(kind, J, sides)


S.build_helmet = build_helmet_ger
S.HELMETS["sidecap"] = dict(S.HELMETS["fieldcap"])          # (for anything that asks the kind's sizes)


# ================================================================================================ the files
HELMET_PAINTS = {"usa": ("helmet", "lens"), "ger": ("helmet", "phones")}[WHO]
NAME = {"usa": "Battle tank crewman (tank commander), USA, Normandy 1944 (our own design)",
        "ger": "Battle tank crewman (Panzer commander), GER, Normandy 1944 (our own design)"}[WHO]
ABOUT = {
    "usa": (" He is the American tank crewman of the Normandy battles (summer 1944), to stand in the Sherman's hatch:"
            " soldier.py's man in every bone (the same 32), with the squad leader's Thompson (soldier_usa.py: his"
            " bursts and his magazine change) and his own face (MakeHuman's mix 214). The winter combat jacket, waist"
            " length and zipped, its collar, cuffs and waistband knitted (paint 38, knit), two slash pockets; a staff"
            " sergeant's chevrons on the sleeves (paint 17, rank, and the webbing's); olive drab wool trousers into"
            " canvas leggings; the pistol belt with the two magazine pouches (on pouch_l and pouch_r) and the"
            " first-aid pouch, no suspenders and no pack. The tanker's helmet of 1938: a fibre dome with six padded"
            " ribs over the crown, a brow band, the ear flaps' earphone housings, the goggles strapped round it with"
            " their lenses on the front (paint 39, lens); `helmet_paints` (5, 39) on the head bone, its chin strap in"
            " the straps' paint as every man's."),
    "ger": (" He is the German Panzer crewman of the Normandy battles (summer 1944), to stand in the Panzer IV's and"
            " the Tiger's cupola: soldier.py's man in every bone (the same 32), with the Kar 98k and his own face"
            " (MakeHuman's mix 41). The black Panzer wrap jacket, waist length, double-breasted, its wide lapels and"
            " big collar laid open over the field-grey shirt (paint 40, shirt) and its collar; the black trousers"
            " gathered over black ankle boots (soldier.py's gaiters, in the trousers' black, paint lower); the black"
            " leather belt with the pistol in its holster at his left hip; no pouches, no straps, no bags. The black"
            " side cap with the radio's headphones over it, a black housing over each ear and the sprung band over"
            " the cap (paint 39, phones); `helmet_paints` (5, 39) on the head bone. No insignia."),
}[WHO]


def helmet_share(data):
    hb = [b["name"] for b in data["bones"]].index("head")
    hp = data["helmet_paints"]
    out = {}
    for lod in ("near", "mid", "far"):
        m = data[lod]
        P = np.array(m["points"]).reshape(-1, 3)
        pa = np.array(m["paints"])
        part = np.array(m["part"])
        T = np.array(m["triangles"]).reshape(-1, 3)
        head = (part[T] == hb).all(1)
        hpt = np.isin(pa[T], hp).all(1)
        high = P[T].mean(1)[:, 2] > 1.5
        out[lod] = {"helmet": int((hpt & head & high).sum()), "helmet_paints_on_head_low": int((hpt & head & ~high).sum())}
    return out


def about(data):
    data["name"] = NAME
    data["about"] = data["about"].replace("Made by Tools/battle_units/soldier.py.",
                                          "Made by Tools/battle_units/soldier_tankers.py on soldier.py"
                                          + (" and soldier_usa.py." if WHO == "usa" else "."))
    data["about"] += ABOUT
    data["colours"] = dict(S.LOOK[NATION])
    data["new_paints"] = dict(NEW_PAINTS)
    data["helmet_paints"] = [S.FIGURE_PAINTS[k] for k in HELMET_PAINTS]
    return data


def export_man(info, m):
    t0 = time.time()
    socks = m["sockets"]
    lods = {0: S.man_objects(m), 1: S.make_lods(m, 1), 2: S.make_lods(m, 2)}
    bpy.context.scene.frame_set(0)
    rec = info.setdefault(BASE, {})
    rec["tris"] = {lod: sum(S.tris_of(o) for o in lods[lod]) for lod in lods}
    for lod in lods:
        rec[f"parts_{lod}"] = {o.name.split(".")[0]: S.tris_of(o) for o in lods[lod]}
    rec["short"] = {k: float(v) for k, v in m["short"].items()}
    S.export_glb(S.stage_path(f"{BASE}.glb"), [m["arm"]] + lods[0] + socks)
    for lod in (1, 2):
        S.export_glb(S.stage_path(f"{BASE}_LOD{lod}.glb"), [m["arm"]] + lods[lod] + socks, anim=False)
    data = S.write_game_json(S.stage_path(f"{BASE}.json"), m, lods)
    data = about(data)
    data = tank_json(data, m["rig"])
    chk = os.path.join(C.WORK, "tank_checks.json")
    if os.path.exists(chk):
        data["tank_clips"]["check"] = json.load(open(chk))
    with open(S.stage_path(f"{BASE}.json"), "w") as f:
        json.dump(data, f, separators=(",", ":"))
    rec["json"] = {k: len(data[k]["triangles"]) // 3 for k in ("near", "mid", "far")}
    rec["share"] = helmet_share(data)
    rec["high"] = data["high"]
    rec["clips"] = {k: [v["long"], v["loop"], v["weapon"]] for k, v in data["clips"].items()}
    for lod in (1, 2):
        for o in lods[lod]:
            bpy.data.objects.remove(o)
    print(f"  {BASE} written in {time.time() - t0:.0f} s: {rec['tris']}; json {rec['json']}; helmet {json.dumps(rec['share'])}")
    return [f"{BASE}.glb", f"{BASE}_LOD1.glb", f"{BASE}_LOD2.glb", f"{BASE}.json"]


def assemble():
    if WHO == "usa":
        U.with_reload(False)                  # (the rifleman's Garand reload is not his: set_man gave him the Thompson's)
    m = S.assemble(NATION, "rifle", seed=SEED)
    use_man(m)
    return m


def export(info):
    t0 = time.time()
    kit.fresh(S.FPS)
    add_tank_clips()
    m = assemble()
    S.bake_clips(m)
    hide_gun_in_tank_clips(m)
    info["files"] = export_man(info, m)
    info["export_s"] = round(time.time() - t0)
    return m


# ================================================================================================ checks and pictures
_setup = C.setup


def setup(nation=NATION, weapon="rifle"):
    """crew.py's pattern man made this man: the tank clips checked and pictured on him."""
    if WHO == "usa":
        U.with_reload(False)
    m = _setup(NATION, weapon)
    return m


C.setup = setup


def check(info):
    keep = dict(C.CREW)
    for k in list(C.CREW):
        if k not in TANK_CLIPS.values():
            C.CREW.pop(k)
    saved = {k: list(c["place"]) for k, c in C.CREW.items()}
    for k, c in C.CREW.items():
        c["place"] = [pl for pl in c["place"] if pl[1] in TANKS_OF]
    try:
        res = C.check_everything("vehicle")
    finally:
        for k, pl in saved.items():
            C.CREW[k]["place"] = pl
        C.CREW.clear()
        C.CREW.update(keep)
    os.makedirs(C.WORK, exist_ok=True)
    json.dump(res, open(os.path.join(C.WORK, "tank_checks.json"), "w"), indent=1)
    info["check"] = {name: [(r.get("at"), r.get("deepest_m"), r.get("where"), r.get("hand_gap_m")) for r in rs] for name, rs in res.items()}
    for name, rs in res.items():
        for r in rs:
            print("CHECK", name, r.get("at"), "deepest", r.get("deepest_m"), r.get("where"), "hand gap", r.get("hand_gap_m"))
    return res


SHOTS = {
    "usa": [("front", "idle", 0.5, (0.0, 0.0, 0.95), 4.3, 6.0, 28.0),
            ("back", "idle", 0.5, (0.0, 0.0, 0.95), 4.3, 8.0, 205.0),
            ("head", "idle", 0.5, (0.02, 0.0, 1.5), 1.25, 6.0, 35.0),
            ("fire", "fire_kneel", 0.3, (0.15, 0.0, 0.7), 3.6, 6.0, 55.0),
            ("hatch", "commander_hatch", 7.9, (0.0, 0.0, 1.1), 3.0, 10.0, 25.0),
            ("roof", "commander_roof_mg", 1.0, (0.1, 0.0, 1.2), 3.0, 10.0, 330.0)],
    "ger": [("front", "idle", 0.5, (0.0, 0.0, 0.95), 4.3, 6.0, 28.0),
            ("back", "idle", 0.5, (0.0, 0.0, 0.95), 4.3, 8.0, 205.0),
            ("head", "idle", 0.5, (0.02, 0.0, 1.5), 1.25, 6.0, 35.0),
            ("fire", "fire_kneel", 0.3, (0.15, 0.0, 0.7), 3.6, 6.0, 55.0),
            ("hatch", "commander_hatch", 7.9, (0.0, 0.0, 1.1), 3.0, 10.0, 25.0),
            ("roof", "commander_roof_mg", 1.0, (0.1, 0.0, 1.2), 3.0, 10.0, 330.0)],
}[WHO]
LABELS = {
    "usa": ["American tank commander at ease: tanker jacket, 1938 helmet, Thompson",
            "From behind: the pistol belt, the knitted waistband",
            "Close: the ribbed helmet, its earphones and goggles; the knitted collar",
            "Firing the Thompson kneeling (fire_kneel)",
            "commander_hatch (no tank): the glasses up",
            "commander_roof_mg (no tank): both hands on the .50's grips"],
    "ger": ["German Panzer commander at ease: black wrap jacket, side cap and headphones",
            "From behind: the big collar over the shoulders",
            "Close: the side cap, the headphones; the shirt between the lapels",
            "Firing kneeling (fire_kneel)",
            "commander_hatch (no tank): the glasses up",
            "commander_roof_mg (no tank): the MG 34 on the cupola's ring"],
}[WHO]


def picture(info, fast=False, shots=None, out_name=None):
    size = (520, 640, 12) if fast else (900, 1080, 36)
    kit.fresh(S.FPS)
    add_tank_clips()
    m = assemble()
    S.studio_smooth(m)
    rig = m["rig"]
    kit.studio()
    kit.backdrop("CFC6B4")
    cam = kit.camera(lens=50)
    S.shared_render(*size)
    ims = []
    for key, name, t, aim, dist, pitch, yaw in (shots or SHOTS):
        st = S.clip_state(rig, name, t, "rifle")
        S.pose_once(m["arm"], rig, st, 0)
        for o in (m["weapon"], m["weapon_part"]):
            o.hide_render = name in TANK_CLIPS
        for o in m["kit"]:
            if o.name.startswith("rifle_mag"):
                o.hide_render = name in TANK_CLIPS
        kit.orbit(cam, aim, dist, pitch, yaw)
        ims.append(kit.still(os.path.join(S.SCRATCH, "pictures", f"{WHO}_{key}.png")))
    out = out_name or S.stage_path("pictures", f"{BASE}.png")
    kit.sheet(ims, out, cols=3, labels=LABELS if not shots else [os.path.basename(p) for p in ims],
              title=NAME.replace("Battle tank crewman", "Tank crewman") + ": soldier_tankers.py")
    info["picture"] = out
    return out


def tank_pictures(fast=False):
    """Him in each of his tanks: in the hatch, at the roof gun, and the slump (crew.py's pictures, on this man)."""
    os.makedirs(TC.PICS, exist_ok=True)
    out = []
    want = os.environ.get("TANK_PICS", "").split(",") if os.environ.get("TANK_PICS") else None
    for piece in TANKS_OF:
        for clip, src, t in (("commander_hatch", "commander_hatch", 7.9), ("commander_roof_mg", TANK_CLIPS["commander_roof_mg"], 0.6),
                             ("die_hatch_slump", "die_hatch_slump", TC.SLUMP_LONG)):
            if want and clip not in want:
                continue
            eye, tgt = TC.eye_of(piece, side=1.0 if piece != "Tiger" else -1.0)
            name = f"{WHO}_{piece}_{clip}"
            C.PICTURES[name] = {"kind": "vehicle", "piece": piece, "men": [(src, t, "commander")], "eye": eye, "target": tgt,
                                "lens": 45, "about": f"{clip} at {t} s on the {piece}"}
            out.append(C.build_picture(name, fast=fast))
    return out


def main():
    a = sys.argv
    fast = "--fast" in a
    steps = a[a.index("--only") + 1].split(",") if "--only" in a else ["export", "picture"]
    os.makedirs(S.STAGE, exist_ok=True)
    p = S.stage_path(f"export_info_{BASE}.json")
    info = json.load(open(p)) if os.path.exists(p) else {}
    for step in steps:
        t0 = time.time()
        if step == "look":
            picture(info, fast=True, shots=SHOTS[:3] + [("side", "idle", 0.5, (0.0, 0.0, 1.0), 3.4, 6.0, 270.0),
                                                       ("headback", "idle", 0.5, (0.0, 0.0, 1.5), 1.2, 10.0, 200.0),
                                                       ("headside", "idle", 0.5, (0.0, 0.0, 1.5), 1.2, 4.0, 90.0)],
                    out_name=os.path.join(S.SCRATCH, "look", "sheet.png"))
        elif step == "backlook":
            # (his back as a camera behind the tank sees it: at the roof gun, from behind and above, near)
            picture(info, fast=True, shots=[("roofback", "commander_roof_mg", 1.0, (0.05, 0.0, 1.25), 2.0, 22.0, 190.0),
                                            ("roofback2", "commander_roof_mg", 1.0, (0.05, 0.0, 1.25), 1.6, 12.0, 150.0),
                                            ("idleback", "idle", 0.5, (0.0, 0.0, 1.25), 1.6, 8.0, 200.0)],
                    out_name=os.path.join(S.SCRATCH, "look", "back_sheet.png"))
        elif step == "export":
            export(info)
        elif step == "picture":
            picture(info, fast)
        elif step == "check":
            check(info)
        elif step == "tanks":
            info["tank_pictures"] = tank_pictures(fast)
        with open(p, "w") as f:
            json.dump(info, f, indent=1, default=str)
        print(f"step {step}: {time.time() - t0:.0f} s", flush=True)


if __name__ == "__main__":
    main()
