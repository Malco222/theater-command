"""The American rifleman of the Normandy battles (summer 1944), and his BAR gunner, made beside soldier.py (which stays
as it is).

    python soldier_usa.py                     build, rig, animate and export both men, then their picture
    python soldier_usa.py --only export       (steps: export, picture, nco, nco_picture; --fast for a small test picture)

Run it in a checkout of claude/project-thread-8egity's Tools/battle_units (soldier.py, human.py, mocap.py, kit.py and
MakeHuman's base files beside it), with the bpy module (Blender 5.0.1), as soldier_sov.py is run.

He is soldier.py's man in every bone and clip: the same MakeHuman body, skeleton, mocap and keyed clips, so the game
plays him with the German's clips. What is his own is laid on here by wrapping soldier.py's makers before `assemble`
calls them:

- the field jacket of 1941: hip length, a fly front buttoned over the zip, a turn-down collar with points and shoulder
  straps (soldier.py's tunic loses its four patch pockets and stand collar; the two slanted hand pockets are left off,
  as the cartridge belt's pockets hide them);
- olive drab wool trousers tucked into canvas leggings, laced up the outside, over brown ankle boots (the canvas
  stands a few millimetres off the boot and the trousers at its two edges, so each edge is a clean line);
- the cartridge belt with ten pockets of one eight-round rifle clip each (three a side in front on the hinged pouch bones, two
  a side on the hips), the canteen in its cover at the right hip, the first-aid pouch at the left, the haversack high
  on the back with the entrenching tool and the bayonet strapped to it, its shoulder straps hooked to the belt in
  front;
- the M1 helmet (soldier.py's "pot", the shape the unit table gives America);
- the M1 rifle, semi-automatic: his three firing clips are his own (a shot, the recoil, the operating rod slamming
  back and home by itself, the rifle settling back on the mark: no hand ever goes to a bolt), one shot a second;
- the BAR gunner (soldier_USA_bar): the same man with the Browning automatic rifle, bipod down, firing four-round
  bursts at the slow rate in his lmg_burst clip.

Everything is our own shape. Files go to battle-models/land/soldier/: soldier_USA.glb, _LOD1.glb, _LOD2.glb and .json,
and the same for soldier_USA_bar.
"""

import json
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import soldier as S  # noqa: E402  (imports bpy first)
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402
import kit  # noqa: E402
from sculpt import Ellipsoid, HalfSpace, Layer, RoundBox, RoundCone, Sphere, Torus, Tube, frame_from, on_surface, rot_euler  # noqa: E402,F401

NATION = "USA"

# ---- his kit: the M1 helmet ("pot"), leggings over ankle boots (gaiters), the haversack's shoulder straps as soldier.py's
# Y-straps; the rest is made below in build_kit
S.NATION_KIT[NATION] = {"helmet": "pot", "tall_boots": False, "pack": False, "roll": False, "canister": False,
                        "chest_pouches": False, "y_straps": True, "pouches": {"front": 3, "rear": False}}

# ---- his colours in the pictures (studio only: the game colours him by paint number; the json carries them too)
S.LOOK[NATION] = {
    "tunic": "8A7F5C",      # the field jacket's light olive drab, a khaki with green in it
    "trousers": "5E5440",   # the wool trousers' darker brown olive drab
    "boots": "5A3F2B",      # brown rough-side-out ankle boots
    "lower": "5A4F36",      # the canvas leggings: khaki at the in-game OD jacket's value (lighter, they read as spats)
    "straps": "7F7556",     # cotton webbing: cartridge belt, straps, pockets
    "helmet": "4C4E37",     # the helmet's olive drab, sanded flat
    "pack": "7A7254",       # the haversack and the canteen cover
    "skin": "BDA08E",
    "wood": "6E4329",       # the rifle's walnut
    "steel": "3A3C38",      # parkerized steel, grey with green in it
    "eye": "2A2420",
    "leather": "3A2E25",    # the soles, and the jacket's dark brown buttons
    "blanket": "857A58",    # (the canteen's felt cover)
}
S.NATION_SEED[NATION] = 85          # (his face: one of MakeHuman's mixes no other army's file uses)

S.SCRATCH = "/home/user/art/soldier_usa/"
S.STAGE = os.path.join(S.SCRATCH, "out")


def _ours(spec):
    return spec is S.NATION_KIT[NATION]


def _is_box(p, half, tol=1e-6):
    return isinstance(p, RoundBox) and np.allclose(p.half, half, atol=tol)


# ================================================================================================ the uniform
_uniform_layers = S.uniform_layers
TUCK = 0.014            # (m: how far the trousers are drawn in under the jacket's skirt; none of it can be seen)


class Tucked:
    """A cloth's field drawn in by `by` metres above a height (eased in between lo_z and hi_z), as a shape."""

    def __init__(self, a, lo_z, hi_z, by):
        self.a, self.lo_z, self.hi_z, self.by = a, lo_z, hi_z, by
        self.lo, self.hi, self.c, self.R = a.lo, a.hi, a.c, getattr(a, "R", None)

    def d(self, px, py, pz):
        u = np.clip((np.asarray(pz) - self.lo_z) / (self.hi_z - self.lo_z), 0.0, 1.0)
        return self.a.d(px, py, pz) + self.by * u * u * (3.0 - 2.0 * u)


class Flared:
    """A cloth's field pushed out by `by` metres below a height (eased off between lo_z and hi_z), as a shape."""

    def __init__(self, a, lo_z, hi_z, by, up=False):
        self.a, self.lo_z, self.hi_z, self.by, self.up = a, lo_z, hi_z, by, up
        self.lo, self.hi, self.c, self.R = a.lo, a.hi, a.c, getattr(a, "R", None)

    def d(self, px, py, pz):
        u = np.clip((np.asarray(pz) - self.lo_z) / (self.hi_z - self.lo_z), 0.0, 1.0)
        e = u * u * (3.0 - 2.0 * u)
        return self.a.d(px, py, pz) - self.by * (e if self.up else 1.0 - e)


LEGGING_FLARE = (0.004, 0.04)   # (m: the leggings' foot stands this far off the boot, eased off over this height)
LEGGING_RIM = (0.0025, 0.025)   # (m: and their top this far off the trousers tucked into them, over this height)


def uniform_layers(man, J, spec, LM):
    layers = _uniform_layers(man, J, spec, LM)
    if not _ours(spec):
        return layers
    tunic, collar = layers[0], layers[3]
    # ---- off: the four patch pockets and their flaps (soldier.py's tunic); its button strip stays as the jacket's fly
    pocket_halves = [(0.006, 0.05, 0.058), (0.006, 0.06, 0.07), (0.0075, 0.054, 0.018), (0.0075, 0.064, 0.018)]
    before = len(tunic.ops)
    tunic.ops = [(how, p, k) for how, p, k in tunic.ops if not (how == "add" and any(_is_box(p, h) for h in pocket_halves))]
    print(f"  USA: {before - len(tunic.ops)} shapes of the German tunic's front taken off (pockets, flaps)")
    # ---- on this man the wool trousers' thighs stand through the jacket's skirt just above its hem (soldier.py's skirt
    # flares for the German's build): under the skirt, where none of it shows, the trousers are drawn in
    trousers = layers[1]
    how, field, k = trousers.ops[0]
    trousers.ops[0] = (how, Tucked(field, LM["hem"] - 0.012, LM["hem"] + 0.012, TUCK), k)
    # (the jacket's two slanted hand pockets are left off: they sit under the cartridge belt's pockets, and at this
    # many faces their flaps read as a stray ridge)
    LM["pockets"] = []
    # ---- the shoulder straps laid along the shoulder's curve (soldier.py's are straight, and their inner ends stand
    # off the cloth where a stand collar no longer hides them): each one a chain of short pieces on the cloth
    straps_on = [(how, p, k) for how, p, k in tunic.ops if how == "add" and isinstance(p, RoundBox) and p.R is not None
                 and abs(p.half[1] - 0.021) < 1e-6 and abs(p.half[2] - 0.006) < 1e-6]
    tunic.ops = [op for op in tunic.ops if not any(op[1] is q[1] for q in straps_on)]
    LM["shoulder_straps"] = []
    for _, p, _k in straps_on:
        ends = [p.c - p.R[:, 0] * p.half[0], p.c + p.R[:, 0] * p.half[0]]
        chain = []
        for u in np.linspace(0.0, 1.0, 5):
            q = ends[0] + (ends[1] - ends[0]) * u
            hit, nrm = on_surface(tunic, q + p.R[:, 2] * 0.12, -p.R[:, 2])
            chain.append((hit, nrm))
        for (a, na), (b, nb) in zip(chain[:-1], chain[1:]):
            nrm = _n(na + nb)
            seg = float(np.linalg.norm(b - a))
            tunic.add(RoundBox((a + b) / 2 + nrm * 0.0015, (seg / 2 + 0.006, 0.021, 0.0042), 0.004, frame_from(b - a, nrm)), 0.004)
        LM["shoulder_straps"].append(chain)
    J["usa_shoulder_straps"] = LM["shoulder_straps"]
    # ---- the turn-down collar in the jacket's cloth: a low band at the neck's foot, the fall rolled over it lying out
    # on the shoulders (wider behind), and two points down over the front where the fly is buttoned to the throat
    nmid, nfwd, nrx, nry = LM["neck_ring"]
    ax = LM["neck_axis"]
    Rn = np.stack([nfwd, np.cross(ax, nfwd), ax], axis=1)
    collar.ops = []
    collar.paint = "tunic"
    collar.add(Tube(nmid, -0.022, 0.014, (nrx + 0.017, nry + 0.016), (nrx + 0.012, nry + 0.012), 0.008, Rn))
    collar.add(Torus(nmid + ax * 0.004 - nfwd * 0.003, nrx + 0.019, 0.0105, Rn, major_y=nry + 0.021), 0.008)
    throat = nmid + nfwd * nrx
    for side in (1.0, -1.0):
        root, nr = on_surface(tunic, (0.4, 0.026 * side, throat[2] - 0.006), (-1.0, 0.0, 0.0))
        tip, nt = on_surface(tunic, (0.4, 0.072 * side, throat[2] - 0.066), (-1.0, -0.25 * side, 0.0))
        nrm = _n(nr + nt)
        a, b = root + nr * 0.0035, tip + nt * 0.0035
        collar.add(RoundBox((a + b) / 2, (float(np.linalg.norm(b - a)) / 2 + 0.004, 0.016, 0.003), 0.003,
                            frame_from(b - a, nrm)), 0.002)
    # ---- the leggings: laced up the outside (soldier.py's gaiters carry two strap rings: off)
    straps = next((L for L in layers if L.name == "gaiter_straps"), None)
    lower = next((L for L in layers if L.name == "gaiters"), None)
    if lower is not None and LEGGING_FLARE[0] > 0.0:
        # the canvas stands a little off the boot at its foot, so its edge is a clean line round the ankle (on the
        # cloth's own 1 mm over the boot the paints met along a ragged line)
        how, field, k = lower.ops[0]
        lo_z, top_z = LM["ankle"] + 0.01, LM["ankle"] + 0.22
        field = Flared(field, lo_z, lo_z + LEGGING_FLARE[1], LEGGING_FLARE[0])
        if LEGGING_RIM[0] > 0.0:
            field = Flared(field, top_z - LEGGING_RIM[1], top_z, LEGGING_RIM[0], up=True)
        lower.ops[0] = (how, field, k)
        if LEGGING_RIM[0] > 0.0:
            # the wool trousers tucked into the leggings (soldier.py blouses the Pole's over his gaiters, where the
            # two cloths met along a ragged line): their fullness eased a little higher, clear of the canvas's edge
            for i, (how_, q, k_) in enumerate(trousers.ops):
                if how_ == "add" and isinstance(q, Torus) and abs(q.minor - 0.016) < 1e-6 and abs(q.c[2] - (top_z + 0.025)) < 0.03:
                    trousers.ops[i] = (how_, Torus(q.c + np.array((0.0, 0.0, 0.018)), q.mx, 0.012, q.R, major_y=q.my), 0.016)
                    print("  USA: trousers tucked into the legging", "left" if q.c[1] > 0 else "right")
    if straps is not None and lower is not None:
        straps.ops = []
        for s, side in (("l", 1.0), ("r", -1.0)):
            pts = []
            for z in (LM["ankle"] + 0.03, LM["ankle"] + 0.12, LM["ankle"] + 0.215):
                c = S.leg_point(J, s, z)
                p, nrm = on_surface(lower, c + np.array((-0.01, 0.2 * side, 0.0)), (0.0, -side, 0.0))
                pts.append(p + nrm * 0.0015)
            for a, b in zip(pts[:-1], pts[1:]):
                straps.add(RoundCone(a, b, 0.0048, 0.0048), 0.004)
            for k in range(7):                                   # (the hooks the lace is wound on)
                u = (k + 0.5) / 7.0
                q = pts[0] + (pts[2] - pts[0]) * u
                straps.add(Sphere(q + np.array((0.0, 0.004 * side, 0.0)), 0.0042), 0.002)
    return layers


S.uniform_layers = uniform_layers


def _n(v):
    v = np.asarray(v, float)
    return v / np.linalg.norm(v)


# ================================================================================================ the kit
_build_kit = S.build_kit


def _waist_ring(tunic, cx, z, side, n=91):
    """Points on the jacket round the waist at height z from straight ahead (0) round one side to straight behind,
    with their outward normals and the length round the cloth to each."""
    # (each found along its own ray in from outside: soldier.py's pull down the field's slope bunches the points where
    # the cloth's shells meet, and its belt with them)
    P, N = [], []
    for a in np.linspace(0.0, math.pi, n):
        d = np.array((math.cos(a), side * math.sin(a), 0.0))
        p, nrm = on_surface(tunic, np.array((cx, 0.0, z)) + d * 0.32, -d)
        P.append((p[0], p[1], z))
        nrm = np.array((nrm[0], nrm[1], 0.0))
        N.append(nrm / max(np.linalg.norm(nrm), 1e-9))
    P, N = np.array(P), np.array(N)
    s = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(P, axis=0), axis=1))])
    return P, N, s


def _taut(rings, cx):
    """The rings drawn taut, as a belt pulled round the waist bridges the small of the back and the cloth's creases:
    each ray's point moved out to the waist's convex outline. Returns the rings so mended and the deepest bridge (m)."""
    from scipy.spatial import ConvexHull
    pts = np.concatenate([rings[1.0][0][:, :2], rings[-1.0][0][:, :2]])
    H = pts[ConvexHull(pts).vertices]
    E = np.roll(H, -1, axis=0) - H
    out, worst = {}, 0.0
    for side, (P, N, _) in rings.items():
        Q, M = P.copy(), N.copy()
        for i, p in enumerate(P):
            d = p[:2] - np.array((cx, 0.0))
            d /= np.linalg.norm(d)
            best = None
            for h, e in zip(H, E):                  # (where the ray from the waist's middle leaves the outline)
                A = np.array(((d[0], -e[0]), (d[1], -e[1])))
                if abs(np.linalg.det(A)) < 1e-12:
                    continue
                t, u = np.linalg.solve(A, h - np.array((cx, 0.0)))
                if t > 0 and -1e-9 <= u <= 1 + 1e-9 and (best is None or t > best[0]):
                    best = (t, e)
            if best is None:
                continue
            q = np.array((cx, 0.0)) + d * best[0]
            worst = max(worst, float(np.linalg.norm(q - p[:2])))
            Q[i, :2] = q
            nn = np.array((best[1][1], -best[1][0], 0.0))
            nn /= np.linalg.norm(nn)
            if nn[:2] @ d < 0:
                nn = -nn
            M[i] = nn
        for _ in range(3):                           # (the outline's corners eased)
            M[1:-1] = M[1:-1] * 0.5 + (M[:-2] + M[2:]) * 0.25
            M /= np.linalg.norm(M, axis=1)[:, None]
        s = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(Q, axis=0), axis=1))])
        out[side] = (Q, M, s)
    return out, worst


def _at_length(P, N, s, u):
    i = int(np.clip(np.searchsorted(s, u) - 1, 0, len(s) - 2))
    a = (u - s[i]) / max(s[i + 1] - s[i], 1e-9)
    return P[i] + (P[i + 1] - P[i]) * a, _n(N[i] + (N[i + 1] - N[i]) * a)


POCKET_HALF = (0.019, 0.029, 0.047)        # one pocket of the cartridge belt: an eight-round clip standing in it
POCKET_AT = (0.058, 0.125, 0.192, 0.259, 0.326)  # their middles round the waist from the buckle (m along the cloth)
BELT_W = 0.058


def build_kit(J, spec, body_ls, Z=None):
    out = _build_kit(J, spec, body_ls, Z)
    if not _ours(spec):
        return out
    Z = Z or (lambda z: z)
    tunic = body_ls[0]
    for k in ("belt", "details", "pouches", "bread_bag", "bottle", "shovel"):
        if k in out:
            bpy.data.objects.remove(out.pop(k)[0])
    zb = Z(1.0)
    cx = J["spine_01"][0] + 0.03 if "crown" in J else 0.0
    rings, bridge = _taut({side: _waist_ring(tunic, cx, zb, side) for side in (1.0, -1.0)}, cx)
    print(f"  USA: the belt drawn taut round the waist (bridging at most {100 * bridge:.1f} cm)")
    if os.environ.get("USA_DEBUG"):
        np.save(os.environ["USA_DEBUG"], np.stack([rings[1.0][0], rings[-1.0][0]]))
    # ---- the cartridge belt: wide webbing round the waist, its plate buckle in front
    g = S.Geo()
    Pl, Nl, _ = rings[1.0]
    Pr, Nr, _ = rings[-1.0]
    P = np.concatenate([Pl[:-1:5], Pr[-1:0:-5]])                 # (round from the buckle by his left, back by his right)
    N = np.concatenate([Nl[:-1:5], Nr[-1:0:-5]])
    P = P + N * 0.005
    g.band(P, N, BELT_W, 0.006, "straps", closed=True)
    front = P[0] + N[0] * 0.003
    g.box(front, (0.005, 0.024, 0.022), "steel", frame_from(N[0]))
    g.box(front + N[0] * 0.005, (0.0016, 0.017, 0.015), "steel", frame_from(N[0]))
    out["belt"] = (g.build("belt"), None)
    # ---- its ten pockets: three a side in front (on the hinged pouch bones), two a side on the hips (with the belt)
    front_L, hip_L = [], []
    J["usa_pockets"] = {}
    for k_ in [k_ for k_ in J if k_.startswith("pouch_pivot_")]:
        del J[k_]
    top = zb + BELT_W / 2 + 0.004
    cz = top - POCKET_HALF[2]
    if MAN["who"] == "nco":
        out["pouches"] = (S.mesh_from_layers("pouches", _magazine_pouches(J, rings, top, body_ls), 0.002, 520), None)
    for side in (() if MAN["who"] == "nco" else (1.0, -1.0)):
        Pr, Nr, sr = rings[side]
        hang = []
        for k, u in enumerate(POCKET_AT):
            p0, n0 = _at_length(Pr, Nr, sr, u)
            R = frame_from(n0, (0.0, 0.0, 1.0))
            c = np.array((p0[0], p0[1], cz)) + n0 * (0.011 + POCKET_HALF[0])
            L = Layer(f"pocket_{k}", "straps")
            (front_L if k < 3 else hip_L).append(L)
            L.add(RoundBox(c, POCKET_HALF, 0.0038, R), 0.0)
            J.setdefault("usa_pockets", {}).setdefault("l" if side > 0 else "r", []).append(
                (tuple(float(v) for v in c), tuple(float(v) for v in n0)))
            L.add(RoundBox(c + R @ np.array((0.0032, 0.0, POCKET_HALF[2] - 0.016)), (POCKET_HALF[0], POCKET_HALF[1] + 0.0016, 0.0165),
                           0.003, R), 0.0015)                                       # (its flap)
            if k < 3:
                hang.append(np.array((p0[0], p0[1], top - 0.012)) + n0 * 0.012)
        J["pouch_pivot_" + ("l" if side > 0 else "r")] = tuple(float(v) for v in np.mean(hang, axis=0))
    J["waist_centre"] = (float(cx), 0.0, float(zb))
    if MAN["who"] != "nco":
        out["pouches"] = (S.mesh_from_layers("pouches", front_L, 0.002, 430), None)
        out["belt_pockets"] = (S.mesh_from_layers("belt_pockets", hip_L, 0.002, 290), None)
    print("  USA: waist round the cloth (left, right):", round(float(rings[1.0][2][-1]), 3), round(float(rings[-1.0][2][-1]), 3),
          "pocket pivots", J["pouch_pivot_l"], J["pouch_pivot_r"])
    # ---- the small things on the jacket: the fly's buttons, the pocket flaps' and the shoulder straps'
    g = S.Geo()
    for z in (Z(1.385), Z(1.3), Z(1.215), Z(1.13), Z(0.905)):
        p, nrm = on_surface(tunic, (0.3, 0.0, z), (-1.0, 0.0, 0.0))
        g.tube(p - nrm * 0.001, p + nrm * 0.0035, 0.0078, 0.0068, "leather", 8)
    collar = body_ls[3]
    for chain in J.get("usa_shoulder_straps", []):             # (the shoulder straps', at the collar end)
        p, nrm = chain[1]
        g.tube(p + nrm * 0.004, p + nrm * 0.0085, 0.0068, 0.006, "leather", 8)
    if MAN["who"] == "nco":
        _chevrons(g, tunic, J)
    out["details"] = (g.build("details"), None)
    J.pop("usa_shoulder_straps", None)
    # ---- the canteen in its cover, hung from the belt behind the right hip; the first-aid pouch behind the left
    Pr, Nr, sr = rings[-1.0]
    p0, n0 = _at_length(Pr, Nr, sr, max(sr[-1] - 0.066, POCKET_AT[-1] + POCKET_HALF[1] + 0.07))
    R = frame_from(n0, (0.0, 0.0, 1.0))
    c = np.array((p0[0], p0[1], zb - BELT_W / 2 - 0.082)) + n0 * 0.042
    L = Layer("canteen", "pack")
    flap = Layer("canteen_flap", "pack")
    L.add(RoundBox(c, (0.033, 0.064, 0.084), 0.028, R))
    L.sub(Ellipsoid(c - R[:, 0] * 0.085, (0.06, 0.1, 0.14), R), 0.012)          # (the kidney's hollow, to the hip)
    flap.add(RoundBox(c + R @ np.array((0.003, 0.0, 0.07)), (0.034, 0.066, 0.026), 0.012, R))
    hook = Layer("canteen_hook", "steel")
    hook.add(RoundBox(c + R @ np.array((-0.022, 0.0, 0.1)), (0.004, 0.012, 0.02), 0.003, R))
    out["bottle"] = (S.mesh_from_layers("bottle", [L, flap, hook], 0.0025, 200), "pelvis")
    Pl, Nl, sl = rings[1.0]
    p0, n0 = _at_length(Pl, Nl, sl, max(sl[-1] - 0.07, POCKET_AT[-1] + POCKET_HALF[1] + 0.058))
    R = frame_from(n0, (0.0, 0.0, 1.0))
    c = np.array((p0[0], p0[1], zb - BELT_W / 2 - 0.035)) + n0 * 0.024
    L = Layer("aid_pouch", "pack")
    L.add(RoundBox(c, (0.017, 0.052, 0.045), 0.01, R))
    L.add(RoundBox(c + R @ np.array((0.004, 0.0, 0.028)), (0.016, 0.054, 0.02), 0.006, R), 0.003)
    out["aid_pouch"] = (S.mesh_from_layers("aid_pouch", [L], 0.0025, 110), "pelvis")
    # ---- the haversack, packed tight and carried high, its flap over the top and two binding straps round it; on its
    # back the entrenching tool in its carrier, blade up and the T-handle down to the belt; the bayonet down its left side
    p0, n0 = S.project(tunic, [(-0.3, 0.0, Z(1.25))])
    back = p0[0][0]
    c = np.array((back - 0.052, 0.0, Z(1.235)))
    hx, hy, hz = 0.046, 0.118, 0.152
    L = Layer("pack", "pack")
    flap = Layer("pack_flap", "pack")
    band = Layer("pack_bands", "straps")
    L.add(RoundBox(c, (hx, hy, hz), 0.03))
    flap.add(RoundBox(c + np.array((-0.003, 0.0, hz - 0.055)), (hx + 0.004, hy + 0.004, 0.062), 0.022))
    for dz in (-0.075, 0.005):
        band.add(RoundBox(c + np.array((0.0, 0.0, dz)), (hx + 0.003, hy + 0.003, 0.013), 0.012))
    out["pack"] = (S.mesh_from_layers("pack", [L, flap, band], 0.004, 300), "chest")
    L = Layer("shovel_case", "straps")
    blade = c + np.array((-hx - 0.018, 0.0, hz - 0.098))
    L.add(RoundBox(blade, (0.014, 0.07, 0.092), 0.01))
    L.add(RoundBox(blade + np.array((-0.004, 0.0, 0.07)), (0.012, 0.073, 0.028), 0.008), 0.003)
    h0 = blade + np.array((0.003, 0.0, -0.085))
    h1 = h0 + np.array((0.01, 0.0, -0.16))
    sc = Layer("scabbard", "helmet")
    hilt = Layer("bayonet_hilt", "leather")
    by = c + np.array((0.004, hy + 0.012, 0.0))
    sc.add(RoundBox(by + np.array((0.0, 0.0, -0.035)), (0.01, 0.017, 0.12), 0.007, rot_euler(0, 4, 0)))
    hilt.add(RoundBox(by + np.array((0.0, 0.0, 0.1)), (0.011, 0.012, 0.048), 0.007, rot_euler(0, 4, 0)))
    g = S.Geo()
    g.tube(h0, h1, 0.0128, 0.0122, "wood", 8)                                    # the handle
    g.tube(h1 + np.array((0.0, 0.045, -0.006)), h1 + np.array((0.0, -0.045, -0.006)), 0.011, 0.011, "wood", 8)   # its T
    # (the squad leader's Thompson takes no bayonet: his pack goes without it)
    g.absorb(S.mesh_from_layers("shovel_case", [L] if MAN["who"] == "nco" else [L, sc, hilt], 0.0025, 190))
    out["shovel"] = (g.build("shovel"), "chest")
    return out


S.build_kit = build_kit


# ================================================================================================ the M1 rifle
# Made in soldier.py's rifle frame (origin where the right hand grips the wrist, x to the muzzle, y left, z up, the bore
# 4.2 cm over the grip), so every hold, the aim and the solver's sight line serve it as they serve the bolt rifle:
# the left hand at the same place under the fore-end, the butt plate where the shoulder bears, the comb under the
# cheek, the sights on z = 0.07 (the rear one an aperture on the receiver's heel, close to the eye).
GARAND_BZ = 0.042
S.REAR_NOTCH["rifle"] = 0.04               # (the aperture: just ahead of the wrist; for arms_check's report)


_build_rifle = S.build_rifle


def build_garand():
    """The M1 rifle, our own drawing of it: a one-piece walnut stock with a semi-pistol grip, the receiver with its
    aperture rear sight between protecting ears, wooden handguards over the barrel to the lower band, the gas
    cylinder at the muzzle with the front sight and the bayonet lug, the trigger guard, a leather sling. 1.107 m.
    The operating rod (its handle on the right of the receiver) and the bolt ride the bolt bone: they fly back and
    home by themselves at each shot. Returns (rifle, rod) objects in rifle space."""
    wood = Layer("stock", "wood")
    wood.add(RoundBox((-0.232, 0.0, -0.026), (0.09, 0.0205, 0.057), 0.016, rot_euler(0, -4, 0)))       # butt
    wood.add(RoundBox((-0.12, 0.0, -0.01), (0.06, 0.0185, 0.034), 0.015), 0.035)                        # into the wrist
    wood.add(RoundCone((-0.09, 0.0, -0.002), (0.035, 0.0, 0.012), 0.0185, 0.0175), 0.02)               # the wrist
    wood.add(Ellipsoid((-0.02, 0.0, -0.031), (0.028, 0.017, 0.033)), 0.02)                              # semi-pistol grip
    wood.add(RoundBox((0.12, 0.0, 0.017), (0.13, 0.0195, 0.03), 0.014), 0.02)                            # under the receiver
    wood.add(RoundBox((0.395, 0.0, 0.026), (0.145, 0.0185, 0.02), 0.0155), 0.03)                        # fore-end to the band
    wood.add(RoundCone((0.262, 0.0, 0.051), (0.33, 0.0, 0.051), 0.0138, 0.0134), 0.004)                # rear handguard
    wood.add(RoundCone((0.348, 0.0, 0.052), (0.523, 0.0, 0.05), 0.0132, 0.0122), 0.004)               # front handguard
    rifle = S.mesh_from_layers("rifle_stock", [wood], 0.0025, 330)
    g = S.Geo()
    st = "steel"
    bz = GARAND_BZ
    g.box((-0.3225, 0.0, -0.026), (0.003, 0.0215, 0.058), st, rot_euler(0, -4, 0))                     # butt plate
    g.tube((0.032, 0.0, 0.047), (0.258, 0.0, 0.047), 0.0168, 0.016, st, 14)                              # receiver
    g.box((0.038, 0.0, 0.052), (0.02, 0.0155, 0.014), st)                                                # its heel
    g.box((0.044, 0.0, 0.0705), (0.0022, 0.0075, 0.008), st)                                             # the aperture
    for side in (1.0, -1.0):
        g.box((0.042, 0.0115 * side, 0.068), (0.012, 0.0024, 0.0095), st)                                # its ears
        g.tube((0.04, 0.016 * side, 0.058), (0.04, 0.025 * side, 0.058), 0.0075, 0.0075, st, 10)         # the knobs
    g.tube((0.258, 0.0, bz), (0.785, 0.0, bz), 0.0108, 0.0086, st, 12)                                   # barrel
    g.tube((0.335, 0.0, 0.041), (0.346, 0.0, 0.041), 0.0205, 0.0205, st, 14)                             # handguard band
    g.tube((0.527, 0.0, 0.031), (0.546, 0.0, 0.031), 0.0255, 0.0255, st, 14, squash=1.1)                 # lower band
    g.box((0.536, 0.0, 0.0), (0.006, 0.004, 0.006), st)                                                  # its swivel
    g.tube((0.672, 0.0, bz - 0.003), (0.771, 0.0, bz - 0.003), 0.0128, 0.0128, st, 12)                  # gas cylinder
    g.tube((0.672, 0.0, bz - 0.018), (0.771, 0.0, bz - 0.018), 0.0085, 0.0085, st, 10)                  # its piston tube
    g.tube((0.771, 0.0, bz - 0.006), (0.785, 0.0, bz - 0.006), 0.0098, 0.0092, st, 10)                  # the lock screw
    g.box((0.752, 0.0, bz - 0.031), (0.011, 0.0032, 0.0045), st)                                         # bayonet lug
    g.box((0.765, 0.0, 0.0625), (0.004, 0.0016, 0.0075), st)                                             # front sight
    for side in (1.0, -1.0):
        g.box((0.765, 0.0085 * side, 0.06), (0.0065, 0.0018, 0.0095), st)                                # its ears
    g.box((0.07, 0.0, -0.0175), (0.06, 0.0135, 0.0035), st)                                              # guard plate
    loop = [(0.002 + 0.027 * math.cos(t), 0.0, -0.018 - 0.019 * math.sin(t)) for t in np.linspace(0.15, math.pi - 0.15, 6)]
    for a, b in zip(loop[:-1], loop[1:]):
        g.tube(a, b, 0.0032, 0.0032, st, 4, caps=False)                                                  # trigger guard
    g.box((0.012, 0.0, -0.021), (0.003, 0.002, 0.009), st)                                               # trigger
    sl = S.spline([(0.536, 0.0, -0.006), (0.32, 0.0, -0.05), (0.06, 0.0, -0.08), (-0.2, 0.0, -0.088)], 12)
    g.band(sl, [(0.0, 1.0, 0.0)] * len(sl), 0.03, 0.003, "leather")                                     # sling
    g.absorb(rifle)
    rifle = g.build("rifle")
    b = S.Geo()
    b.box((0.135, 0.0, 0.0625), (0.045, 0.0085, 0.0055), st)                                             # bolt, on top
    b.box((0.2, -0.0195, 0.043), (0.055, 0.0035, 0.0065), st)                                            # rod, along the receiver
    b.box((0.236, -0.026, 0.045), (0.011, 0.0055, 0.0075), st)                                           # its handle
    b.tube((0.255, -0.0165, 0.03), (0.672, -0.008, bz - 0.018), 0.0042, 0.0038, st, 6)                  # the rod forward
    rod = b.build("rifle_bolt")
    return rifle, rod


S.build_rifle = build_garand


# ================================================================================================ the BAR
# In soldier.py's machine-gun frame (as build_lmg: the right hand's grip at (-0.03, 0, -0.045), the butt plate at
# -0.358, the bore 4 cm over the frame's origin, the sights 8.2 cm, the bipod's hinge at (0.6, 0, 0.018) where its bone
# is), so lmg_burst, the bipod's fold and the gunner's hold serve it. Its muzzle is further out than the generic gun's:
# the gunner's muzzle socket is moved to it.
BAR_BZ = 0.04
BAR_MUZZLE = (0.859, 0.0, BAR_BZ)
S.REAR_NOTCH["lmg"] = -0.005


_build_lmg = S.build_lmg


def build_bar():
    """The Browning automatic rifle, our own drawing of it: a wooden butt with a straight comb and a steeply raked
    wrist (no pistol grip), the tall square receiver with the trigger housing under its rear, the twenty-round box
    magazine under its middle, the cocking handle on the left, a slim wooden forearm round the gas cylinder, the heavy
    barrel, the bipod at the gas cylinder's bracket, the tall leaf rear sight, the front sight between its wings, and
    the cone of the flash hider. 1.214 m. Returns (gun, bipod) objects in its own space."""
    wood = Layer("bar_stock", "wood")
    wood.add(RoundBox((-0.255, 0.0, -0.006), (0.1, 0.0205, 0.05), 0.016, rot_euler(0, -3, 0)))              # butt
    wood.add(RoundBox((-0.13, 0.0, 0.014), (0.055, 0.0178, 0.03), 0.012), 0.03)                             # its small
    wood.add(RoundCone((0.0, 0.0, -0.008), (-0.072, 0.0, -0.07), 0.0185, 0.0195), 0.028)                    # the raked wrist
    fore = Layer("bar_forearm", "wood")
    fore.add(RoundBox((0.43, 0.0, 0.009), (0.135, 0.0215, 0.025), 0.014))
    gun = S.mesh_from_layers("lmg_wood", [wood, fore], 0.0025, 260)
    g = S.Geo()
    st = "steel"
    bz = BAR_BZ
    g.box((-0.358, 0.0, -0.008), (0.003, 0.0205, 0.05), st, rot_euler(0, -3, 0))                         # butt plate
    g.box((-0.349, 0.0, 0.046), (0.01, 0.017, 0.0035), st)                                               # its shoulder rest
    g.box((0.14, 0.0, 0.022), (0.155, 0.0205, 0.04), st)                                                 # receiver
    g.box((0.02, 0.0, -0.028), (0.036, 0.0165, 0.016), st)                                               # trigger housing
    loop = [(0.004 + 0.028 * math.cos(t), 0.0, -0.043 - 0.021 * math.sin(t)) for t in np.linspace(0.12, math.pi - 0.12, 6)]
    for a, b in zip(loop[:-1], loop[1:]):
        g.tube(a, b, 0.0033, 0.0033, st, 4, caps=False)                                                  # trigger guard
    g.box((0.012, 0.0, -0.05), (0.003, 0.002, 0.009), st)                                                # trigger
    g.box((0.045, 0.0215, -0.022), (0.012, 0.002, 0.005), st)                                            # selector, left
    g.box((0.135, 0.0, -0.052), (0.047, 0.0138, 0.04), st)                                               # magazine
    g.box((0.135, 0.0, -0.093), (0.049, 0.0152, 0.003), st)                                              # its floor plate
    g.box((0.15, -0.021, 0.034), (0.042, 0.0016, 0.012), st)                                             # ejection port cover
    g.tube((0.245, 0.02, 0.012), (0.245, 0.038, 0.012), 0.0052, 0.0052, st, 8)                            # cocking handle
    g.tube((0.245, 0.038, 0.012), (0.245, 0.044, 0.012), 0.0075, 0.0075, st, 8)
    g.box((0.0, 0.0, 0.068), (0.022, 0.012, 0.006), st)                                                  # rear sight base
    g.box((-0.004, 0.0, 0.077), (0.004, 0.0095, 0.0085), st)                                             # its leaf
    g.tube((0.295, 0.0, bz), (0.815, 0.0, bz), 0.0128, 0.0112, st, 12)                                   # barrel
    g.tube((0.295, 0.0, bz - 0.027), (0.6, 0.0, bz - 0.027), 0.0095, 0.0095, st, 10)                     # gas cylinder
    g.box((0.6, 0.0, 0.025), (0.012, 0.0165, 0.027), st)                                                 # its bracket
    g.tube((0.612, 0.0, bz - 0.027), (0.632, 0.0, bz - 0.027), 0.0108, 0.0108, st, 10)                   # gas regulator
    g.tube((0.79, 0.0, bz), (0.806, 0.0, bz), 0.0142, 0.0142, st, 12)                                    # front sight band
    g.box((0.798, 0.0, 0.0745), (0.0042, 0.0016, 0.0075), st)                                            # front sight
    for side in (1.0, -1.0):
        g.box((0.798, 0.0092 * side, 0.07), (0.0062, 0.0018, 0.0115), st)                                # its wings
    g.tube((0.815, 0.0, bz), (0.859, 0.0, bz), 0.0118, 0.0168, st, 12)                                   # flash hider
    sl = S.spline([(0.59, 0.0, -0.006), (0.38, 0.0, -0.045), (0.12, 0.0, -0.112), (-0.22, 0.0, -0.07)], 12)
    g.band(sl, [(0.0, 1.0, 0.0)] * len(sl), 0.03, 0.003, "straps")                                     # sling
    g.absorb(gun)
    gun = g.build("lmg")
    b = S.Geo()
    h = np.array(S.WEAPONS["lmg"]["hinge"])
    b.tube(h + (0.0, -0.031, 0.0), h + (0.0, 0.031, 0.0), 0.0075, 0.0075, st, 8)                          # hinge
    for side in (1.0, -1.0):
        foot = h + np.array((-0.31, 0.037 * side, -0.006))
        b.tube(h + (0.0, 0.03 * side, 0.0), foot, 0.0062, 0.0055, st, 6)                                # legs, folded back
        b.box(foot + np.array((-0.012, 0.0, -0.003)), (0.022, 0.008, 0.004), st)                         # skid feet
    bipod = b.build("lmg_bipod")
    return gun, bipod


S.build_lmg = build_bar


# (the rifle and the gun are his only while he is the one being made: the German beside him in the picture keeps his own)
_assemble = S.assemble


def assemble(nation, weapon="rifle", seed=None):
    ours = nation == NATION
    S.build_rifle = (build_thompson if MAN["who"] == "nco" else build_garand) if ours else _build_rifle
    S.build_lmg = build_bar if ours else _build_lmg
    return _assemble(nation, weapon, seed)


S.assemble = assemble


# ================================================================================================ his firing clips
def rod_cycle(P, t, shot=0.1, travel=0.09):
    """The operating rod and the bolt: driven back by the gas as the bullet leaves, home again on the spring, in about
    five hundredths of a second, by themselves (the hands never leave the rifle)."""
    P["bolt"] = (travel * (S.ramp(t, shot + 0.002, shot + 0.018) - S.ramp(t, shot + 0.03, shot + 0.058)), 0.0)
    return P


def settle(P, t, shot=0.1, dip=1.2):
    """Back on the mark after the kick: the muzzle brought down a hair past it and eased up onto it again."""
    d = -dip * S.bump(t, shot + 0.17, shot + 0.3, shot + 0.52)
    if d:
        P["weapon"] = S._kicked(P["weapon"], 0.0, d)
    return P


SHOT = [S.ev(0.1, "muzzle_flash", "socket_muzzle", "rifle_shot")]
FIRE_LONG = 1.0


@S.clip("fire_stand", FIRE_LONG, loop=True, events=SHOT)
def c_fire_stand(t):
    """Standing, aimed: the shot, the recoil, the rod back and home, the rifle settling back on the mark; one a second."""
    P = S.aim_stand(t)
    P = S.breathe(P, t, FIRE_LONG, 0.4)
    P = S.recoil(P, t)
    P = settle(P, t)
    return rod_cycle(P, t)


@S.clip("fire_kneel", FIRE_LONG, loop=True, events=SHOT)
def c_fire_kneel(t):
    P = S.kneel_base(t)
    P = S.breathe(P, t, FIRE_LONG, 0.4)
    P = S.recoil(P, t)
    P = settle(P, t)
    return rod_cycle(P, t)


@S.clip("fire_prone", FIRE_LONG, loop=True, events=SHOT)
def c_fire_prone(t):
    P = S.prone_base(t)
    P = S.breathe(P, t, FIRE_LONG, 0.3)
    P = S.recoil(P, t, body=0.6)
    P = settle(P, t, dip=0.8)
    return rod_cycle(P, t)


# ================================================================================================ his reload
# After the eighth shot the empty clip flies out with a ring (the sound director's garand_clip_ping, played by the game
# with that shot) and the bolt stays open. He drops the butt from his shoulder, the left hand keeping the fore-end, and
# looks down at the open action; the right hand goes to a pocket of his cartridge belt for a full clip, brings it up and
# thumbs it down into the action, its edge holding the operating rod's handle back; the hand comes away, the bolt flies
# home by itself, and the hand goes back to the grip as the rifle comes back up onto the mark. About two seconds.
ROD_TRAVEL = 0.09
CLIP_IN = (0.10, -0.047, 0.036)      # (the right hand's palm against the receiver's right side, the thumb over the top)
RELOAD_LONG = 2.2
RELOAD_EVENTS = [S.ev(0.95, "reload", None, "garand_reload")]
_resolve_hand = S.Rig.resolve_hand


def _pocket_hand(self, st, h, side):
    """A hand at one of the cartridge belt's pockets (h: pocket side and number, how far out and up of its flap, the
    hand's axis and palm in the pocket's frame: x out of the pocket, y along the belt, z up), carried by the pelvis."""
    c, n0 = (np.array(v, float) for v in self.J["usa_pockets"][h["side"]][h["k"]])
    z = np.array((0.0, 0.0, 1.0))
    Rp = np.stack([n0, np.cross(z, n0), z], axis=1)
    p = c + n0 * (POCKET_HALF[0] + h.get("out", 0.02)) + z * (POCKET_HALF[2] * 0.5 + h.get("up", 0.0))
    D, H = st["D"]["pelvis"], st["H"]["pelvis"]
    target = D @ (p - self.H0["pelvis"]) + H
    a = D @ Rp @ _n(h["a"])
    n = np.asarray(h["n"], float)
    n = D @ Rp @ _n(n - _n(h["a"]) * (n @ _n(h["a"])))
    pole = np.asarray(h["pole_world"], float) if "pole_world" in h else st["D"]["chest"] @ np.asarray(h.get("pole", (-0.3, 0.6 * side, -1.0)), float)
    return target, a, n, pole


def resolve_hand(self, st, P, h, side):
    if "blend" not in h and h.get("frame") == "pocket":
        return _pocket_hand(self, st, h, side)
    return _resolve_hand(self, st, P, h, side)


S.Rig.resolve_hand = resolve_hand


def lowered(base, up=16.0, cant=-18.0, back=0.04, drop=0.0):
    """The rifle off the shoulder lying down: turned muzzle-up about where the left hand holds it and canted to the
    right, the action drawn back under his face."""
    def place(rig, st):
        M = base(rig, st) if callable(base) else base
        R, o = M[:3, :3], M[:3, 3]
        gl = np.array(S.WEAPONS["rifle"]["grip_l"])
        pivot = o + R @ gl
        Rk = R @ S.R_axis((0, 1, 0), -up) @ S.R_axis((1, 0, 0), cant)
        return S.M4(Rk, pivot - Rk @ gl - R[:, 0] * back + np.array((0.0, 0.0, -drop)))
    return place


def loading(at=(0.3, -0.2, -0.1), barrel=(0.8, 0.3, 0.5), up=(-0.45, -0.35, 0.82)):
    """The rifle held for loading, standing or kneeling, placed from the chest: the butt down beside the right hip, the
    left hand on the fore-end before the chest, the muzzle up and to the left, the action before the right breast canted
    toward the right hand (at: the stock's wrist, in the chest's frame)."""
    x = _n(barrel)
    z = np.asarray(up, float)
    z = _n(z - x * (z @ x))
    R = np.stack([x, np.cross(z, x), z], axis=1)

    def place(rig, st):
        return rig.frame(st, "chest") @ S.M4(R, np.asarray(at, float))
    return place


POCKET_POLE = (0.5, -1.0, -0.3)            # (the elbow out to the side, a little forward, as the hand goes to a pocket)
CLIP_POLE = (0.2, -1.0, -0.3)              # (and out to the side over the action as the thumb presses the clip in)


def reload_keys(base, pocket_k, look=(14.0, 16.0), hold=None, via=None, poles=None, roll=None, hand=None):
    """The reload's key poses on a stance (base: its aimed pose), the right hand to pocket pocket_k of his right side;
    hold: where the rifle is held meanwhile; via: the right hand's way from the grip down to the pocket (chest frame).
    The right hand lets go of the grip as the rifle comes down (the left hand has it, the butt against his side) and
    takes it again only as the rifle goes back up to the mark."""
    poles = poles or {}
    hand = hand or {}
    aimP = dict(base)
    down = dict(base, weapon=hold or loading(), sight=0.0,
                neck=S.add3(base["neck"], (look[0], 0.0, -6.0)), head=S.add3(base["head"], (look[1], 6.0, -8.0)))
    if roll:
        down["pelvis"] = S.add3(base["pelvis"], roll[0])
        down["chest"] = S.add3(down["chest"], roll[1])
    pk = lambda up, out, fingers, thumb: dict(down, hand_r={"frame": "pocket", "side": "r", "k": pocket_k, "up": up, "out": out,
                                                            "a": hand.get("a", (0.4, 0.4, -1.0)),
                                                            "n": hand.get("n", (-0.6, 0.8, 0.0)), "roll": 20.0,
                                                            "pole": poles.get("pocket", POCKET_POLE)},
                                              fingers_r=fingers, thumb_r=thumb)
    if "chest_at" in hand:
        # (lying on the belt's front pockets: the hand goes in under his right side, in the chest's frame)
        c0 = np.asarray(hand["chest_at"], float)
        pk = lambda up, out, fingers, thumb: dict(down, hand_r={"frame": "chest", "at": tuple(c0 + np.array((0.0, -out, up))),
                                                                "a": hand.get("a", (0.0, 0.0, -1.0)), "n": hand.get("n", (0.0, 1.0, 0.0)),
                                                                "roll": 20.0, "pole": poles.get("pocket", POCKET_POLE)},
                                                  fingers_r=fingers, thumb_r=thumb)
    hand_in = {"grip": "clip_in", "a": hand.get("clip_a", (0.95, 0.0, -0.15)), "n": hand.get("clip_n", (0.0, 0.92, -0.4)), "roll": 20.0,
               "pole": poles.get("clip", CLIP_POLE)}
    hold_in = lambda dz, fingers, thumb: dict(down, hand_r=dict(hand_in, slide=(0.0, 0.0, dz)), fingers_r=fingers, thumb_r=thumb)
    off = dict(down, hand_r=dict(hand_in, slide=(-0.03, -0.035, 0.04)), fingers_r=35.0, thumb_r=30.0)
    v = via or {"frame": "chest", "at": (0.16, -0.22, -0.3), "a": (0.3, 0.2, -1.0), "n": (-0.5, 0.85, 0.0), "roll": 20.0}
    v = pk(0.09, 0.07, 20.0, 20.0) if v == "pocket" else dict(down, hand_r=dict(v, pole=poles.get("pocket", POCKET_POLE)), fingers_r=20.0)
    return [(0.0, aimP), (0.3, v), (0.48, pk(0.03, 0.03, 15.0, 15.0)), (0.64, pk(0.0, 0.012, 68.0, 30.0)),
            (0.78, pk(0.07, 0.04, 68.0, 30.0))] + \
           ([(0.88, dict(down, hand_r=dict(hand["mid"], pole=poles.get("clip", CLIP_POLE)), fingers_r=68.0, thumb_r=30.0))]
            if "mid" in hand else []) + \
           [(0.98, hold_in(0.03, 55.0, 15.0)), (1.18, hold_in(-0.004, 50.0, 5.0)),
            (1.34, off), (1.85, dict(aimP)), (RELOAD_LONG, dict(aimP))]


def _arm_angles(rig, P):
    """The right arm's own angles (elevation, plane, rotation, elbow, pronation, wrist flexion, radial deviation, as
    rom.py measures them and Rig.arm_place takes them) in pose P as the solver places it on this man."""
    import rom
    st = rig.solve(dict(P, weapon_kind="rifle"))
    m = {k: float(v[0]) for k, v in rom.measure(rom.Body.of_rig(rig), rom.Poses.of_states([st])).items()}
    u0, u1 = -math.sin(math.radians(m["sh_behind_r"])), -math.sin(math.radians(m["sh_hadd_r"] - 90.0))
    return (m["sh_elev_r"], math.degrees(math.atan2(u0, u1)), m["sh_rot_r"], m["el_flex_r"], m["pron_r"],
            m["wr_flex_r"], m["wr_dev_r"])


def _reload_clip(base_fn, pocket_k, breathe=0.3, **kw):
    """The reload on one stance. Its keys between the grip and the grip again are turned, on the man being made, into
    the right arm's own angles where the solver puts the hand (at the pocket, over the action): so the arm goes from
    key to key angle by angle and never snaps from one way of reaching to another between them."""
    keys = {}

    def prepare(rig):
        ks = reload_keys(base_fn(0.0), pocket_k, **kw)
        prev = None
        for i in range(len(ks)):
            tk, P = ks[i]
            ang = list(_arm_angles(rig, P))
            if prev is not None:                    # (each angle the near way round from the key before)
                for j in (1, 2, 4):
                    ang[j] += 360.0 * round((prev[j] - ang[j]) / 360.0)
            prev = ang
            ks[i] = (tk, dict(P, hand_r={"arm": tuple(ang)}))
        keys[id(rig)] = keys["now"] = S.keyed(ks)
        keys["rig"] = id(rig)

    def fn(t):
        k = keys.get("now")
        if k is None:
            k = keys.setdefault("spec", S.keyed(reload_keys(base_fn(0.0), pocket_k, **kw)))
        P = dict(k(t))                      # (a copy: past its last key `keyed` hands back the key itself)
        P = S.breathe(P, t, 2.0, breathe)
        P["bolt"] = (ROD_TRAVEL * (1.0 - S.ramp(t, 1.26, 1.3)), 0.0)        # (open from the last shot; home as the hand leaves)
        return P
    fn.prepare, fn.keys = prepare, keys
    return fn


_clip_state = S.clip_state


def clip_state(rig, name, t, kind=None):
    """soldier.py's clip_state, the reload's arm keys worked out on this man the first time one of them is asked for."""
    fn = RELOAD_CLIPS.get(name)
    if fn is not None and S.CLIPS.get(name, {}).get("fn") is fn and fn.keys.get("rig") != id(rig):
        fn.prepare(rig) if id(rig) not in fn.keys else fn.keys.update(now=fn.keys[id(rig)], rig=id(rig))
    return _clip_state(rig, name, t, kind)


S.clip_state = clip_state


RELOAD_PRONE = dict(breathe=0.2, look=(6.0, 8.0), hold=lowered(S.aim(), up=0.0, cant=20.0, back=-0.05, drop=0.0),
                    via="pocket", poles={"pocket": (0.2, -1.0, 0.3), "clip": (0.0, -1.0, 0.0)},
                    hand={"chest_at": (0.12, -0.16, -0.22), "a": (0.2, 0.1, -1.0), "n": (-0.3, 1.0, 0.0),
                          "clip_a": (0.9, 0.0, -0.35), "clip_n": (0.0, 1.0, -0.1),
                          "mid": {"frame": "chest", "at": (0.25, -0.2, 0.1), "a": (0.3, 0.3, 1.0), "n": (-0.2, 1.0, 0.0), "roll": 20.0}},
                    roll=((0.0, -20.0, 0.0), (0.0, -20.0 / 3, 0.0)))
RELOAD_CLIPS = {
    "reload_stand": _reload_clip(S.aim_stand, 0),
    "reload_kneel": _reload_clip(S.kneel_base, 0),
    # (lying, he rolls a little onto his left side, the rifle pushed forward and canted, and his hand goes in under
    # his chest to the front pockets he lies on)
    "reload_prone": _reload_clip(S.prone_base, 0, **RELOAD_PRONE),
}


def with_reload(on):
    """The reload clips are the rifleman's only (the BAR's file and any other man made beside him go without)."""
    S.WEAPONS["rifle"]["clip_in"] = CLIP_IN
    for name, fn in RELOAD_CLIPS.items():
        if on:
            S.CLIPS[name] = {"long": RELOAD_LONG, "loop": False, "stride": 0.0, "events": list(RELOAD_EVENTS), "fn": fn,
                             "weapon": "rifle"}
        else:
            S.CLIPS.pop(name, None)


# the BAR's burst: four rounds at the slow rate (about 400 a minute), then the pause to lay it again
BAR_ROUNDS = (0.1, 0.25, 0.4, 0.55)


@S.clip("lmg_burst", 1.2, loop=True, weapon="lmg",
        events=[S.ev(BAR_ROUNDS[0], "muzzle_flash", "socket_muzzle", "lmg_burst")] +
               [S.ev(t_, "muzzle_flash", "socket_muzzle") for t_ in BAR_ROUNDS[1:]])
def c_bar_burst(t):
    """soldier.py's lmg_burst with the BAR's rate: the gunner prone behind it on its bipod, the left hand under the
    butt pulling it in, a jolt each round."""
    P = S.prone_base(arch=0.5, weapon="lmg")
    P["bipod"] = 104.0
    P["neck"] = (-28.0, -1.0, 4.0)
    P["hand_r"] = {"grip": "grip_r", "a": (0.96, 0.0, -0.28), "n": (0.0, 1.0, 0.0), "pole_world": (-0.1, -1.0, -1.2)}
    P["hand_l"] = {"grip": "grip_butt", "slide": (0.09, 0.0, -0.075), "a": (0.032, -0.997, 0.076), "n": (-0.997, -0.037, -0.065),
                   "pole_world": (0.0, 0.6, -1.0), "roll": 15.0}
    P["fingers_l"] = 40.0
    j, on = S.lmg_shake(t, BAR_ROUNDS[0], BAR_ROUNDS[-1], 1.0 / (BAR_ROUNDS[1] - BAR_ROUNDS[0]))
    P["clavicle_r"] = S.add3(P["clavicle_r"], (1.8 * j, -5.5 * j))[:2]
    P["chest"] = S.add3(P["chest"], (-0.9 * j, 0.0, -1.1 * j))
    P["head"] = S.add3(P["head"], (-1.6 * j + 0.4 * on * S.osc(t, 0.075), 0.0, 0.3 * on * S.osc(t, 0.1)))

    def place(rig, st, j=j):
        M = S.aim_bipod(rig, st)
        R = M[:3, :3] @ S.R_axis((0, 1, 0), -1.3 * j) @ S.R_axis((0, 0, 1), 0.5 * j * S.osc(t, 0.23))
        return S.M4(R, M[:3, 3] - M[:3, 0] * 0.009 * j)
    P["weapon"] = place
    return S.breathe(P, t, 1.2, 0.3)


# ================================================================================================ the squad leader
# soldier_USA_nco: the staff sergeant who leads the rifle squad. The same man's jacket, trousers, leggings, helmet,
# haversack and canteen; his rank on both sleeves (three chevrons over one arc, olive drab on dark blue); a plain web
# belt in place of the rifleman's ten pockets, with a three-cell pouch of thirty-round magazines on each hip (on the
# hinged pouch bones, so they swing clear of the thighs as he kneels); his own face; and the Thompson submachine gun
# (M1A1): it fires in bursts from the shoulder, the cocking handle on its right side flying with each round.
MAN = {"who": "rifleman"}           # who is being made: the rifleman (and his BAR gunner) or "nco"
NCO_SEED = 192
# The Thompson in soldier.py's rifle frame (origin where the right hand holds the pistol grip, x to the muzzle, y left,
# z up): the bore 9 cm over the grip, the sights' line 12, the left hand on the horizontal fore-grip, the butt plate
# 0.288 m behind the grip. 0.813 m long.
THOMPSON_BZ = 0.09
THOMPSON = {"grip_r": (0.0, 0.0, 0.0), "grip_l": (0.25, 0.0, 0.05), "butt": (-0.287, 0.0, 0.025), "sight_z": 0.12,
            "bolt": (-0.064, -0.058, 0.02), "carry": (0.12, 0.0, 0.05)}
THOMPSON_BUTT_AT = (-0.289, 0.0, 0.005)
THOMPSON_MUZZLE = (0.525, 0.0, THOMPSON_BZ)
KNOB_TRAVEL = 0.075                 # (m: the cocking handle runs forward with the bolt to fire each round, and back)
MAG_POUCH = (0.0165, 0.0225, 0.118)  # one cell of the magazine pouch: a thirty-round magazine standing in it
MAG_POUCH_AT = 0.15                  # (m along the belt from the buckle: the middle cell's middle)
RANK = "2A3350"                      # the chevrons' dark blue ground (the stripes are olive drab: the webbing's paint)


def _magazine_pouches(J, rings, top, body_ls):
    """The two three-cell magazine pouches, one on each hip, hung from the belt (their tops at its top) and leaning out
    as far as the jacket's skirt and the thigh under it ask; their pivots are the pouch bones'. Returns their layers."""
    Ls = []
    zc = top - MAG_POUCH[2]
    DU = (-2.0 * MAG_POUCH[1] - 0.001, 0.0, 2.0 * MAG_POUCH[1] + 0.001)
    for side in (1.0, -1.0):
        Pr, Nr, sr = rings[side]
        hang, cells = [], []
        for du in DU:
            p0, n0 = _at_length(Pr, Nr, sr, MAG_POUCH_AT + du)
            n0 = _n(np.array((n0[0], n0[1], 0.0)))
            # (how far out the cloth stands at the pouch's foot, on this cell's line: the pouch leans out to clear it)
            foot = np.array((p0[0], p0[1], zc - MAG_POUCH[2] + 0.02))
            out = 0.0
            for L in body_ls[:2]:
                hit, _ = on_surface(L, foot + n0 * 0.25, -n0)
                out = max(out, float((hit - foot) @ n0))
            cells.append((p0, n0, math.atan2(max(out + 0.006 - 0.011, 0.0), 2.0 * MAG_POUCH[2] - 0.02)))
        lean = max(c[2] for c in cells)              # (the three cells are one pouch: they lean together)
        for k, (p0, n0, _) in enumerate(cells):
            up = _n(np.array((0.0, 0.0, 1.0)) * math.cos(lean) - n0 * math.sin(lean))
            nn = _n(np.cross(np.cross(up, n0), up))
            R = frame_from(nn, up)
            hinge = np.array((p0[0], p0[1], top)) + n0 * 0.011
            c = hinge + nn * MAG_POUCH[0] - up * MAG_POUCH[2]
            L = Layer(f"mag_pouch_{'l' if side > 0 else 'r'}{k}", "straps")
            L.add(RoundBox(c, MAG_POUCH, 0.004, R), 0.0)
            L.add(RoundBox(c + R @ np.array((0.003, 0.0, MAG_POUCH[2] - 0.019)), (MAG_POUCH[0], MAG_POUCH[1] + 0.0014, 0.02),
                           0.003, R), 0.0015)                                            # (its flap)
            L.add(RoundBox(c + R @ np.array((MAG_POUCH[0] + 0.0012, 0.0, MAG_POUCH[2] - 0.036)), (0.002, 0.005, 0.005),
                           0.0018, R), 0.0006)                                           # (the flap's snap)
            Ls.append(L)
            hang.append(hinge + n0 * 0.001 - np.array((0.0, 0.0, 0.012)))
        print(f"  USA nco: the {'left' if side > 0 else 'right'} magazine pouch leans out {math.degrees(lean):.1f} deg")
        J["pouch_pivot_" + ("l" if side > 0 else "r")] = tuple(float(v) for v in np.mean(hang, axis=0))
    return Ls


def _chevrons(g, tunic, J):
    """A staff sergeant's rank on the outside of each upper sleeve, its top a hand's breadth under the shoulder: three
    chevrons, points up, over one arc (the rocker), each an olive drab stripe on a wider dark blue ground, every piece
    laid on the sleeve's curve in short lengths."""
    W, D, GAP = 0.039, 0.024, 0.0125            # (half the width, the drop of a chevron's ends, from one to the next)
    for s, side in (("l", 1.0), ("r", -1.0)):
        sh, el = np.array(J["upperarm_" + s]), np.array(J["lowerarm_" + s])
        ax = _n(el - sh)
        lat = np.array((0.0, side, 0.0))
        lat = _n(lat - ax * (lat @ ax))
        fwd = _n(np.cross(ax, lat) * side)
        if fwd[0] < 0:
            fwd = -fwd
        L = float(np.linalg.norm(el - sh))
        v0 = 0.27 * L

        def at(v, w):
            th = w / 0.058
            d = lat * math.cos(th) + fwd * math.sin(th)
            return on_surface(tunic, sh + ax * v + d * 0.2, -d)

        def stripe(pts, half_w, lift, paint):
            hits = [at(v, w) for v, w in pts]
            for (a, na), (b, nb) in zip(hits[:-1], hits[1:]):
                nrm = _n(na + nb)
                seg = float(np.linalg.norm(b - a))
                g.box((a + b) / 2 + nrm * lift, (seg / 2 + half_w * 0.35, half_w, 0.0011), paint, frame_from(b - a, nrm))

        paths = []
        for k in range(3):
            v = v0 + k * GAP
            paths.append([(v + D * abs(u), W * u) for u in np.linspace(-1.0, 1.0, 9)])
        vb = v0 + 2 * GAP + D
        paths.append([(vb + 0.016 * (1.0 - u * u) + GAP * 0.6, W * u) for u in np.linspace(-1.0, 1.0, 9)])   # the rocker
        for path in paths:
            stripe(path, 0.0062, 0.0013, "rank")
            stripe(path, 0.0042, 0.0026, "straps")
    print("  USA nco: three chevrons and a rocker on each sleeve")


def build_thompson():
    """The Thompson submachine gun, M1A1, our own drawing of it: a walnut butt with a straight comb, the wooden pistol
    grip and the horizontal fore-grip, the squared receiver with its fixed rear sight between two ears, the plain
    barrel with the blade front sight, the trigger guard, the thirty-round box magazine in its well, a web sling on the
    left side. 0.813 m. The cocking handle (on the right of the receiver, shown at the rear: the bolt held open, ready)
    rides the bolt bone. Returns (gun, handle) objects in the rifle frame."""
    wood = Layer("tsmg_wood", "wood")
    wood.add(RoundBox((-0.225, 0.0, 0.032), (0.062, 0.019, 0.055), 0.016, rot_euler(0, -3, 0)))            # butt
    wood.add(RoundBox((-0.1, 0.0, 0.062), (0.062, 0.0165, 0.026), 0.012), 0.03)                             # its small
    wood.add(RoundCone((0.008, 0.0, 0.038), (-0.024, 0.0, -0.058), 0.0165, 0.019), 0.006)                  # pistol grip
    fore = Layer("tsmg_fore", "wood")
    fore.add(RoundBox((0.25, 0.0, 0.06), (0.078, 0.0205, 0.018), 0.012))                                    # fore-grip
    gun = S.mesh_from_layers("tsmg_wood", [wood, fore], 0.0022, 260)
    g = S.Geo()
    st = "steel"
    bz = THOMPSON_BZ
    g.box((-0.2885, 0.0, 0.032), (0.0025, 0.0195, 0.056), st, rot_euler(0, -3, 0))                       # butt plate
    g.box((0.105, 0.0, 0.086), (0.15, 0.0165, 0.026), st)                                                # receiver
    g.tube((-0.04, 0.0, 0.1), (0.25, 0.0, 0.1), 0.014, 0.014, st, 12)                                    # its rounded top
    g.box((0.062, 0.0, 0.048), (0.107, 0.0145, 0.013), st)                                               # the frame
    g.box((0.12, 0.0, 0.03), (0.026, 0.0165, 0.012), st)                                                 # magazine well
    g.box((0.12, 0.0, -0.052), (0.019, 0.0125, 0.112), st)                                               # magazine
    g.box((0.12, 0.0, -0.165), (0.0205, 0.0135, 0.0025), st)                                             # its floor plate
    loop = [(0.048 + 0.03 * math.cos(t), 0.0, 0.036 - 0.026 * math.sin(t)) for t in np.linspace(0.1, math.pi - 0.1, 6)]
    for a, b in zip(loop[:-1], loop[1:]):
        g.tube(a, b, 0.0032, 0.0032, st, 4, caps=False)                                                  # trigger guard
    g.box((0.045, 0.0, 0.022), (0.003, 0.002, 0.011), st)                                                # trigger
    g.box((0.02, 0.016, 0.05), (0.008, 0.002, 0.006), st)                                                # selector, left
    g.box((0.05, 0.016, 0.05), (0.008, 0.002, 0.006), st)                                                # safety, left
    g.tube((0.255, 0.0, bz), (0.525, 0.0, bz), 0.0115, 0.0105, st, 12)                                   # barrel
    g.box((0.512, 0.0, 0.11), (0.004, 0.0018, 0.0095), st)                                               # front sight
    g.box((-0.02, 0.0, 0.114), (0.013, 0.0095, 0.004), st)                                               # rear sight base
    g.box((-0.024, 0.0, 0.1205), (0.0022, 0.0062, 0.0055), st)                                           # its aperture
    for side in (1.0, -1.0):
        g.box((-0.02, 0.0105 * side, 0.12), (0.011, 0.0022, 0.0085), st)                                 # its ears
    g.box((0.19, -0.0175, 0.088), (0.035, 0.0015, 0.009), st)                                            # ejection port
    g.box((0.1, -0.0172, 0.097), (0.048, 0.0015, 0.0028), st)                                            # the handle's slot
    sl = S.spline([(0.29, 0.0225, 0.05), (0.12, 0.03, -0.01), (-0.08, 0.03, -0.012), (-0.24, 0.0215, 0.01)], 12)
    g.band(sl, [(0.0, 1.0, 0.0)] * len(sl), 0.025, 0.003, "straps")                                    # sling, left
    g.absorb(gun)
    gun = g.build("rifle")
    b = S.Geo()
    b.tube((0.07, -0.0165, 0.097), (0.07, -0.03, 0.097), 0.0048, 0.0048, st, 8)                          # cocking handle
    b.tube((0.07, -0.03, 0.097), (0.07, -0.036, 0.097), 0.0075, 0.0068, st, 10)                          # its knob
    handle = b.build("rifle_bolt")
    return gun, handle


# ---- his bursts: four rounds at about 700 a minute, from the shoulder; each round the gun driven back and its muzzle
# climbing, the climb adding up through the burst, then brought back down onto the mark
SMG_ROUNDS = (0.1, 0.186, 0.272, 0.358)
SMG_LONG = 1.3
SMG_EVENTS = [S.ev(SMG_ROUNDS[0], "muzzle_flash", "socket_muzzle", "thompson_burst")] + \
             [S.ev(t_, "muzzle_flash", "socket_muzzle") for t_ in SMG_ROUNDS[1:]]


def smg_burst(P, t, body=0.45, climb=1.7, dip=1.4):
    for r in SMG_ROUNDS:
        P = S.recoil(P, t, shot=r, kick=0.013, rise=climb, body=body)
    held = climb * 0.8 * sum(S.ramp(t, r, r + 0.03) for r in SMG_ROUNDS[1:]) * (1.0 - S.ramp(t, SMG_ROUNDS[-1] + 0.12, SMG_ROUNDS[-1] + 0.45))
    d = held - dip * S.bump(t, SMG_ROUNDS[-1] + 0.3, SMG_ROUNDS[-1] + 0.45, SMG_ROUNDS[-1] + 0.7)
    if abs(d) > 1e-6:
        P["weapon"] = S._kicked(P["weapon"], 0.0, d)
    P["bolt"] = (-KNOB_TRAVEL * sum(S.bump(t, r - 0.014, r, r + 0.032) for r in SMG_ROUNDS), 0.0)
    return P


def c_smg_stand(t):
    """Standing, aimed: a burst of four, the muzzle climbing round by round, then back down onto the mark."""
    P = S.breathe(S.aim_stand(t), t, SMG_LONG, 0.4)
    return smg_burst(P, t)


def c_smg_kneel(t):
    P = S.breathe(S.kneel_base(t), t, SMG_LONG, 0.4)
    return smg_burst(P, t)


def c_smg_prone(t):
    P = S.breathe(S.prone_base(t), t, SMG_LONG, 0.3)
    return smg_burst(P, t, body=0.3, climb=1.2, dip=1.0)


SMG_CLIPS = {"fire_stand": c_smg_stand, "fire_kneel": c_smg_kneel, "fire_prone": c_smg_prone}
_SAVED = {}


def set_man(who):
    """Make the rifleman (and his BAR gunner) or the squad leader: the weapon's frame, the clips and the face."""
    import mocap
    if not _SAVED:
        _SAVED.update(weapon=dict(S.WEAPONS["rifle"]), butt=S.BUTT_AT["rifle"], muzzle=S.MUZZLE, rear=S.REAR_NOTCH["rifle"],
                      comb=S.COMB_Z["rifle"], left=mocap.LEFT_HAND, drop=mocap.LEFT_DROP, seed=S.NATION_SEED[NATION],
                      clips={n: S.CLIPS[n] for n in SMG_CLIPS})
    MAN["who"] = who
    if who == "nco":
        S.WEAPONS["rifle"] = dict(THOMPSON)
        S.BUTT_AT["rifle"], S.MUZZLE = THOMPSON_BUTT_AT, THOMPSON_MUZZLE
        S.REAR_NOTCH["rifle"], S.COMB_Z["rifle"] = -0.02, 0.088
        # (the mocap carries hold the left hand on the fore-grip: nearer the grip than the rifle's and 4.4 cm higher)
        mocap.LEFT_HAND, mocap.LEFT_DROP = 0.29, -0.044
        S.NATION_SEED[NATION] = NCO_SEED
        S.FIGURE_PAINTS["rank"] = 17
        S.LOOK[NATION]["rank"] = RANK
        for name, fn in SMG_CLIPS.items():
            S.CLIPS[name] = {"long": SMG_LONG, "loop": True, "stride": 0.0, "events": list(SMG_EVENTS), "fn": fn, "weapon": "rifle"}
    else:
        S.WEAPONS["rifle"] = dict(_SAVED["weapon"])
        S.BUTT_AT["rifle"], S.MUZZLE = _SAVED["butt"], _SAVED["muzzle"]
        S.REAR_NOTCH["rifle"], S.COMB_Z["rifle"] = _SAVED["rear"], _SAVED["comb"]
        mocap.LEFT_HAND, mocap.LEFT_DROP = _SAVED["left"], _SAVED["drop"]
        S.NATION_SEED[NATION] = _SAVED["seed"]
        S.FIGURE_PAINTS.pop("rank", None)
        S.LOOK[NATION].pop("rank", None)
        S.CLIPS.update(_SAVED["clips"])


def export_nco(info, deliver=True):
    """soldier_USA_nco.glb, _LOD1, _LOD2 and .json: the squad leader with his Thompson."""
    t0 = time.time()
    set_man("nco")
    kit.fresh(S.FPS)
    m = S.assemble(NATION)
    S.bake_clips(m)
    files = _export_man(info, m, f"soldier_{NATION}_nco", "squad leader")
    if deliver:
        for n_ in files:
            S.deliver(n_)
    info["export_nco_s"] = round(time.time() - t0)
    set_man("rifleman")
    return info


def picture_nco(info, fast=False, deliver=True):
    """pictures/soldier_USA_nco.png: the squad leader beside the rifleman at ease, close on his rank and pouches, and
    firing a burst standing and kneeling."""
    size = (640, 760, 12) if fast else (1000, 1180, 36)
    kit.fresh(S.FPS)
    men = {}
    for who, y in (("nco", -0.6), ("rifleman", 0.6)):
        set_man(who)
        m = S.assemble(NATION)
        S.studio_smooth(m)
        for mt in bpy.data.materials:
            if not mt.name.startswith(("USA_", "NCO_", "backdrop", "ground")):
                mt.name = ("NCO_" if who == "nco" else "USA_") + mt.name
        m["arm"].location = (0.0, y, 0.0)
        P = S.base_stand()
        P["foot_l"] = {"at": (0.12, 0.12, S.ANKLE), "yaw": 6.0}
        P["foot_r"] = {"at": (-0.06, -0.12, S.ANKLE), "yaw": -8.0}
        S.pose_once(m["arm"], m["rig"], P, 0)
        men[who] = m
    set_man("nco")
    kit.studio()
    kit.backdrop("CFC6B4")
    cam = kit.camera(lens=50)
    S.shared_render(*size)
    shots = []
    for key, yaw, pitch, dist, aim_y, aim_z in (("front", 25.0, 6.0, 4.6, 0.0, 1.0), ("near", 60.0, 4.0, 1.7, -0.6, 1.2)):
        kit.orbit(cam, (0.0, aim_y, aim_z), dist, pitch, yaw)
        shots.append(kit.still(os.path.join(S.SCRATCH, "pictures", f"nco_{key}.png")))
    rif = men.pop("rifleman")
    for ob in [rif["arm"]] + S.man_objects(rif):
        ob.hide_render = True
    m = men["nco"]
    m["arm"].location = (0.0, 0.0, 0.0)
    S.fit_body(m["J"])                       # (his own measures again: the rifleman's were the last laid down)
    rig = m["rig"]
    for name, t, yaw, pitch, dist, aim in (("fire_stand", 0.29, 330.0, 4.0, 2.4, (0.25, 0.0, 1.2)),
                                           ("fire_kneel", 0.29, 40.0, 6.0, 2.4, (0.1, 0.0, 0.8))):
        S.pose_once(m["arm"], rig, S.clip_state(rig, name, t, "rifle"), 0)
        kit.orbit(cam, aim, dist, pitch, yaw)
        shots.append(kit.still(os.path.join(S.SCRATCH, "pictures", f"nco_{name}.png")))
    out = S.stage_path("pictures", "soldier_USA_nco.png")
    kit.sheet(shots, out, cols=2, labels=["Squad leader (left) beside his rifleman, at ease",
                                          "Close: staff sergeant's stripes, Thompson, magazine pouches",
                                          "Firing a burst standing: the muzzle climbs round by round",
                                          "Kneeling, the burst's third round"],
              title="American squad leader with the Thompson, Normandy 1944 (our own design)")
    if deliver and not fast:
        S.deliver("pictures/soldier_USA_nco.png")
    info["picture_nco"] = out
    set_man("rifleman")
    return out


# ================================================================================================ the files
def _json_about(data, who):
    data["name"] = f"Battle {who}, USA, Normandy 1944 (our own design)"
    data["about"] = data["about"].replace("Made by Tools/battle_units/soldier.py.",
                                          "Made by Tools/battle_units/soldier_usa.py on soldier.py.")
    if who == "squad leader":
        data["about"] += (" The squad leader, a staff sergeant: his rank on both sleeves (paint 17, rank: the chevrons'"
                          " dark blue ground), a three-cell magazine pouch on each hip, and the Thompson submachine gun."
                          " His fire_stand, fire_kneel and fire_prone are four-round bursts at about 700 a minute (one"
                          " muzzle_flash a round, the sound director's thompson_burst on the first), the muzzle climbing"
                          " through the burst; the bolt bone carries the cocking handle, which runs forward and back with"
                          " each round. No reload clip yet.")
    else:
        data["about"] += (" His firing clips are the M1 rifle's own: semi-automatic, one shot a second, no bolt worked (the"
                          " bolt bone carries the operating rod, which flies back and home by itself at each shot). After the"
                          " eighth shot the sound director's garand_clip_ping, then reload_stand, reload_kneel or reload_prone"
                          " (rifleman only: 2.2 s, garand_reload at 0.95 s as the clip goes in, the operating rod home at 1.3 s)."
                          " lmg_burst is the BAR gunner's (soldier_USA_bar): four rounds at the slow rate.")
    data["colours"] = dict(S.LOOK[NATION])
    if who == "squad leader":
        data["new_paints"] = {"rank": S.FIGURE_PAINTS["rank"]}     # (paints past soldier.py's 14, by name)
    return data


def _export_man(info, m, base, who):
    t0 = time.time()
    socks = m["sockets"]
    lods = {0: S.man_objects(m), 1: S.make_lods(m, 1), 2: S.make_lods(m, 2)}
    bpy.context.scene.frame_set(0)
    n = {lod: sum(S.tris_of(o) for o in lods[lod]) for lod in lods}
    rec = info.setdefault(base, {})
    rec["tris"] = n
    rec["parts"] = {o.name.split(".")[0]: S.tris_of(o) for o in lods[0]}
    rec["short"] = {k: float(v) for k, v in m["short"].items()}
    S.export_glb(S.stage_path(f"{base}.glb"), [m["arm"]] + lods[0] + socks)
    for lod in (1, 2):
        S.export_glb(S.stage_path(f"{base}_LOD{lod}.glb"), [m["arm"]] + lods[lod] + socks, anim=False)
    data = S.write_game_json(S.stage_path(f"{base}.json"), m, lods)
    data = _json_about(data, who)
    with open(S.stage_path(f"{base}.json"), "w") as f:
        json.dump(data, f, separators=(",", ":"))
    rec["json"] = {k: len(data[k]["triangles"]) // 3 for k in ("near", "mid", "far")}
    rec["high"] = data["high"]
    rec["sockets"] = data["sockets"]
    rec["bones"] = [b["name"] for b in data["bones"]]
    rec["clips"] = {k: [v["long"], v["loop"]] for k, v in data["clips"].items()}
    for lod in (1, 2):
        for o in lods[lod]:
            bpy.data.objects.remove(o)
    print(f"  {base} written in {time.time() - t0:.0f} s: {n}")
    return [f"{base}.glb", f"{base}_LOD1.glb", f"{base}_LOD2.glb", f"{base}.json"]


def export_usa(info, deliver=True):
    """soldier_USA.glb (LOD0, every clip), _LOD1.glb, _LOD2.glb (same skeleton, no clips) and soldier_USA.json (the game's
    own form); then the same man with the BAR: soldier_USA_bar*. No .fbx (the game reads the json and the glb)."""
    t0 = time.time()
    kit.fresh(S.FPS)
    with_reload(True)
    m = S.assemble(NATION)
    S.bake_clips(m)
    files = _export_man(info, m, f"soldier_{NATION}", "rifleman")
    with_reload(False)
    for name in RELOAD_CLIPS:
        if name in bpy.data.actions:
            bpy.data.actions.remove(bpy.data.actions[name])
    # ---- the BAR gunner: the same man, rig and clips with the BAR on the weapon bone and the bipod on its own
    S.give_lmg(m)
    old = m["sockets"][0]
    g0 = np.array(m["arm"].data.bones["weapon"].head_local)
    new = S.add_socket(m["arm"], "muzzle", "weapon", tuple(g0 + np.array(BAR_MUZZLE)))
    bpy.data.objects.remove(old)
    new.name = "socket_muzzle"
    m["sockets"][0] = new
    files += _export_man(info, m, f"soldier_{NATION}_bar", "BAR gunner")
    if deliver:
        for n_ in files:
            S.deliver(n_)
    info["export_s"] = round(time.time() - t0)
    return info


# ================================================================================================ the picture
def picture(info, fast=False, deliver=True):
    """Two sheets. pictures/soldier_USA.png: the American beside the German, at ease, from the front quarter, from behind,
    and close. pictures/soldier_USA_action.png: the rifleman firing standing (the instant after the shot: the kick, the
    operating rod thrown back), kneeling on the aim, and the BAR gunner down behind his gun on its bipod."""
    size = (640, 760, 12) if fast else (1000, 1180, 36)
    kit.fresh(S.FPS)
    men = {}
    for nation, y in (("USA", -0.6), ("GER", 0.6)):
        m = S.assemble(nation)
        S.studio_smooth(m)
        for mt in bpy.data.materials:
            if not mt.name.startswith(("USA_", "GER_", "backdrop", "ground")):
                mt.name = nation + "_" + mt.name
        m["arm"].location = (0.0, y, 0.0)
        P = S.base_stand()
        P["foot_l"] = {"at": (0.12, 0.12, S.ANKLE), "yaw": 6.0}
        P["foot_r"] = {"at": (-0.06, -0.12, S.ANKLE), "yaw": -8.0}
        S.pose_once(m["arm"], m["rig"], P, 0)
        men[nation] = m
    kit.studio()
    kit.backdrop("CFC6B4")
    cam = kit.camera(lens=50)
    S.shared_render(*size)
    shots = []
    for key, yaw, pitch, dist, aim_y, aim_z in (("front", 25.0, 6.0, 4.6, 0.0, 1.0), ("back", 200.0, 8.0, 4.6, 0.0, 1.0),
                                                ("near", 15.0, 4.0, 2.1, -0.6, 1.25)):
        kit.orbit(cam, (0.0, aim_y, aim_z), dist, pitch, yaw)
        shots.append(kit.still(os.path.join(S.SCRATCH, "pictures", f"usa_{key}.png")))
    out = S.stage_path("pictures", "soldier_USA.png")
    kit.sheet(shots, out, cols=3, labels=["American rifleman (left) beside the German, at ease",
                                          "From behind: haversack, entrenching tool, bayonet, canteen",
                                          "Close: M1 helmet, field jacket, cartridge belt, M1 rifle"],
              title="American rifleman, Normandy 1944: soldier_usa.py on soldier.py (our own design)")
    # ---- in action: the German put away, the American alone at the middle
    ger = men.pop("GER")
    for ob in [ger["arm"]] + S.man_objects(ger):
        ob.hide_render = True
    m = men["USA"]
    m["arm"].location = (0.0, 0.0, 0.0)
    S.fit_body(m["J"])                       # (his own measures again: the German's were the last laid down)
    rig = m["rig"]
    acts = []
    with_reload(True)
    for name, t, yaw, pitch, dist, aim in (("fire_stand", 0.135, 330.0, 4.0, 2.6, (0.25, 0.0, 1.2)),
                                           ("fire_kneel", 0.6, 40.0, 6.0, 2.6, (0.1, 0.0, 0.8)),
                                           ("reload_stand", 1.1, 320.0, 6.0, 2.2, (0.2, 0.0, 1.2))):
        S.pose_once(m["arm"], rig, S.clip_state(rig, name, t, "rifle"), 0)
        kit.orbit(cam, aim, dist, pitch, yaw)
        acts.append(kit.still(os.path.join(S.SCRATCH, "pictures", f"usa_{name}.png")))
    S.build_lmg = build_bar                   # (the German made beside him put soldier.py's own gun back as the maker)
    S.give_lmg(m)
    S.pose_once(m["arm"], rig, S.clip_state(rig, "lmg_burst", 0.12, "lmg"), 0)
    kit.orbit(cam, (0.55, -0.05, 0.2), 3.4, 11.0, 335.0)
    acts.append(kit.still(os.path.join(S.SCRATCH, "pictures", "usa_bar.png")))
    out2 = S.stage_path("pictures", "soldier_USA_action.png")
    with_reload(False)
    acts = acts[:2] + acts[3:] + acts[2:3]
    kit.sheet(acts, out2, cols=2, labels=["Firing standing: the kick, the operating rod thrown back by itself",
                                          "Kneeling on the aim: one shot a second, no bolt to work",
                                          "BAR gunner: down on the bipod, four-round bursts",
                                          "Reloading after the eighth shot: a fresh clip thumbed into the action"],
              title="American rifleman and BAR gunner in action (our own design)")
    if deliver and not fast:
        S.deliver("pictures/soldier_USA.png")
        S.deliver("pictures/soldier_USA_action.png")
    info["picture"] = [out, out2]
    return out


def main():
    a = kit.args()
    fast = "--fast" in a
    steps = a[a.index("--only") + 1].split(",") if "--only" in a else ["export", "picture", "nco", "nco_picture"]
    keep = "--keep" in a                              # (stage only: nothing is copied to the shared folder)
    p = S.stage_path(f"export_info_{NATION}.json")
    info = json.load(open(p)) if os.path.exists(p) else {}
    for step in steps:
        t0 = time.time()
        if step == "export":
            export_usa(info, deliver=not keep)
        elif step == "picture":
            picture(info, fast, deliver=not keep)
        elif step == "nco":
            export_nco(info, deliver=not keep)
        elif step == "nco_picture":
            picture_nco(info, fast, deliver=not keep)
        with open(p, "w") as f:
            json.dump(info, f, indent=1)
        print(f"step {step}: {time.time() - t0:.0f} s")
    if not keep:
        ok = S.check_delivered()
        print("delivered" if ok else "SOME FILES DID NOT LAND")


if __name__ == "__main__":
    main()
