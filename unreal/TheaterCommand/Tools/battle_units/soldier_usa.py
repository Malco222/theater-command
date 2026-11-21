"""The American rifleman of the Normandy battles (summer 1944), and his BAR gunner, made beside soldier.py (which stays
as it is).

    python soldier_usa.py                     build, rig, animate and export both men, then their picture
    python soldier_usa.py --only export       (steps: export, picture; --fast for a small test picture)

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
- the cartridge belt with ten pockets of two rifle clips each (three a side in front on the hinged pouch bones, two
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
    "lower": "9A8A63",      # the canvas leggings, khaki
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


POCKET_HALF = (0.019, 0.029, 0.047)        # one pocket of the cartridge belt: two eight-round clips standing in it
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
    for k_ in [k_ for k_ in J if k_.startswith("pouch_pivot_")]:
        del J[k_]
    top = zb + BELT_W / 2 + 0.004
    cz = top - POCKET_HALF[2]
    for side in (1.0, -1.0):
        Pr, Nr, sr = rings[side]
        hang = []
        for k, u in enumerate(POCKET_AT):
            p0, n0 = _at_length(Pr, Nr, sr, u)
            R = frame_from(n0, (0.0, 0.0, 1.0))
            c = np.array((p0[0], p0[1], cz)) + n0 * (0.011 + POCKET_HALF[0])
            L = Layer(f"pocket_{k}", "straps")
            (front_L if k < 3 else hip_L).append(L)
            L.add(RoundBox(c, POCKET_HALF, 0.0038, R), 0.0)
            L.add(RoundBox(c + R @ np.array((0.0032, 0.0, POCKET_HALF[2] - 0.016)), (POCKET_HALF[0], POCKET_HALF[1] + 0.0016, 0.0165),
                           0.003, R), 0.0015)                                       # (its flap)
            if k < 3:
                hang.append(np.array((p0[0], p0[1], top - 0.012)) + n0 * 0.012)
        J["pouch_pivot_" + ("l" if side > 0 else "r")] = tuple(float(v) for v in np.mean(hang, axis=0))
    J["waist_centre"] = (float(cx), 0.0, float(zb))
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
    g.absorb(S.mesh_from_layers("shovel_case", [L, sc, hilt], 0.0025, 190))
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
    S.build_rifle = build_garand if ours else _build_rifle
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


# ================================================================================================ the files
def _json_about(data, who):
    data["name"] = f"Battle {who}, USA, Normandy 1944 (our own design)"
    data["about"] = data["about"].replace("Made by Tools/battle_units/soldier.py.",
                                          "Made by Tools/battle_units/soldier_usa.py on soldier.py.")
    data["about"] += (" His firing clips are the M1 rifle's own: semi-automatic, one shot a second, no bolt worked (the"
                      " bolt bone carries the operating rod, which flies back and home by itself at each shot). After the"
                      " eighth shot the sound director's garand_clip_ping, then garand_reload. lmg_burst is the BAR"
                      " gunner's (soldier_USA_bar): four rounds at the slow rate.")
    data["colours"] = dict(S.LOOK[NATION])
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
    m = S.assemble(NATION)
    S.bake_clips(m)
    files = _export_man(info, m, f"soldier_{NATION}", "rifleman")
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
    rig = m["rig"]
    acts = []
    for name, t, yaw, pitch, dist, aim in (("fire_stand", 0.135, 330.0, 4.0, 2.6, (0.25, 0.0, 1.2)),
                                           ("fire_kneel", 0.6, 40.0, 6.0, 2.6, (0.1, 0.0, 0.8))):
        S.pose_once(m["arm"], rig, S.clip_state(rig, name, t, "rifle"), 0)
        kit.orbit(cam, aim, dist, pitch, yaw)
        acts.append(kit.still(os.path.join(S.SCRATCH, "pictures", f"usa_{name}.png")))
    S.give_lmg(m)
    S.pose_once(m["arm"], rig, S.clip_state(rig, "lmg_burst", 0.12, "lmg"), 0)
    kit.orbit(cam, (0.55, -0.05, 0.2), 3.4, 11.0, 335.0)
    acts.append(kit.still(os.path.join(S.SCRATCH, "pictures", "usa_bar.png")))
    out2 = S.stage_path("pictures", "soldier_USA_action.png")
    kit.sheet(acts, out2, cols=3, labels=["Firing standing: the kick, the operating rod thrown back by itself",
                                          "Kneeling on the aim: one shot a second, no bolt to work",
                                          "BAR gunner: down on the bipod, four-round bursts"],
              title="American rifleman and BAR gunner in action (our own design)")
    if deliver and not fast:
        S.deliver("pictures/soldier_USA.png")
        S.deliver("pictures/soldier_USA_action.png")
    info["picture"] = [out, out2]
    return out


def main():
    a = kit.args()
    fast = "--fast" in a
    steps = a[a.index("--only") + 1].split(",") if "--only" in a else ["export", "picture"]
    keep = "--keep" in a                              # (stage only: nothing is copied to the shared folder)
    p = S.stage_path(f"export_info_{NATION}.json")
    info = json.load(open(p)) if os.path.exists(p) else {}
    for step in steps:
        t0 = time.time()
        if step == "export":
            export_usa(info, deliver=not keep)
        elif step == "picture":
            picture(info, fast, deliver=not keep)
        with open(p, "w") as f:
            json.dump(info, f, indent=1)
        print(f"step {step}: {time.time() - t0:.0f} s")
    if not keep:
        ok = S.check_delivered()
        print("delivered" if ok else "SOME FILES DID NOT LAND")


if __name__ == "__main__":
    main()
