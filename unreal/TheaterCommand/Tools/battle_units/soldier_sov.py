"""The Soviet rifleman of the Eastern Front battles, made beside soldier.py (which stays as it is).

    python soldier_sov.py                     build, rig, animate and export the Soviet man, then his picture
    python soldier_sov.py --only export       (steps: export, picture; --fast for a small test picture)

He is soldier.py's man in every bone and clip: the same MakeHuman body, skeleton, mocap and weapon work, so the game
plays him with the German's clips. What is his own is laid on in this file, by wrapping three of soldier.py's makers
before `assemble` calls them:

- the shirt-tunic of 1943 (a pullover worn outside the trousers): a stand collar in the tunic's own cloth, a short
  placket of three buttons from the collar to the breast, shoulder boards, and no pockets (soldier.py's tunic has a
  button strip to the hem and four pockets: those shapes are taken off);
- breeches full in the thigh, tucked into tall boots;
- his kit: belt, ammunition pouches, bread bag, water bottle and entrenching tool as soldier.py makes them, no Y-straps,
  and the greatcoat rolled and worn as a ring over the LEFT shoulder to the right hip (clear of the rifle's butt);
- the steel helmet of 1940 (soldier.py's "soviet" helmet);
- his colours for the pictures (the game paints him by number from the unit table: cloth 7B7748).

And two winter dresses on the same man, skeleton and clips (`--dress winter,snow`):

- winter: the padded jacket (telogreika) quilted in vertical channels and buttoned down the front, padded trousers,
  felt boots (valenki) to below the knee, and the fur cap (ushanka) with its flaps tied up; no greatcoat roll;
- snow: all that under a white snow suit, smock and trousers, its hood up.

Their paints are the summer man's (the jacket is `tunic`, the felt boots `boots`, the cap or hood `helmet`); each
file's json carries the colours it wants under "colours", since one paint's colour differs by dress.

Everything is our own shape. Files go to battle-models/land/soldier/ as soldier_SOV*.glb and soldier_SOV.json
(soldier_SOV_winter* and soldier_SOV_snow* for the winter dresses).
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
import kit  # noqa: E402
from sculpt import HalfSpace, Layer, RoundBox, RoundCone, Sphere, Torus, frame_from, on_surface  # noqa: E402
from soldier import Blend, Both  # noqa: E402

NATION = "SOV"

# ---- his colours in the pictures (studio only: the game colours him by paint number)
S.LOOK[NATION] = {
    "tunic": "7B7748",      # the unit table's cloth: a pale khaki green, sun-faded
    "trousers": "6C6943",   # the breeches a shade darker
    "boots": "26211D",      # blackened boots
    "lower": "2C2824",
    "straps": "5B4A33",     # brown leather belt and pouches
    "helmet": "4E5A3A",     # the helmet's green (the vehicles' 4F5E3E)
    "pack": "7A7154",       # canvas bread bag and shovel carrier
    "skin": "BDA08E",
    "wood": "6B4429",
    "steel": "34363A",
    "eye": "2A2420",
    "leather": "3A2E25",
    "blanket": "675D4E",    # the greatcoat's grey-brown wool, rolled
}
S.NATION_SEED[NATION] = 19          # (his face: one of MakeHuman's mixes no other army's file uses)

# ---- the winter dresses: the same man (face, body, skeleton) in other clothes. dress: (kit tag, file suffix)
DRESSES = {"summer": (NATION, ""), "winter": ("SOVW", "_winter"), "snow": ("SOVS", "_snow")}
for _tag, _dress, _hat in (("SOVW", "winter", "ushanka"), ("SOVS", "snow", "snowhood")):
    S.NATION_KIT[_tag] = dict(S.NATION_KIT[NATION], helmet=_hat, roll=False, dress=_dress)
    S.NATION_SEED[_tag] = S.NATION_SEED[NATION]
S.LOOK["SOVW"] = dict(S.LOOK[NATION], boots="8C867B", lower="8C867B", helmet="6B6647", blanket="7D7466")   # grey felt boots; cap's cloth, its fur
S.LOOK["SOVS"] = dict(S.LOOK["SOVW"], tunic="E1E3DF", trousers="D9DBD7", helmet="E1E3DF", blanket="E1E3DF")    # the snow suit and its hood, a shade off white


def _ours(spec):
    """Whether a kit is this man's (summer or winter)."""
    return spec is S.NATION_KIT[NATION] or bool(spec.get("dress"))


_mh_body = S.mh_body


def mh_body(nation, seed=None, name=None):
    return _mh_body(NATION if nation in ("SOVW", "SOVS") else nation, seed, name)          # (the same man in every dress)


S.mh_body = mh_body

S.SCRATCH = "/home/user/art/soldier_sov/"
S.STAGE = os.path.join(S.SCRATCH, "out")


def _is_box(p, half, tol=1e-6):
    return isinstance(p, RoundBox) and np.allclose(p.half, half, atol=tol)


# ================================================================================================ the uniform
_uniform_layers = S.uniform_layers
UNIFORM_BUDGET = S.BUDGET["uniform"]


def uniform_layers(man, J, spec, LM):
    layers = _uniform_layers(man, J, spec, LM)
    if not _ours(spec):
        return layers
    # (the quilted channels need the faces to stay through the thinning; the snow suit over them is plain)
    S.BUDGET["uniform"] = UNIFORM_BUDGET + {"winter": 480, "snow": 900}.get(spec.get("dress"), 0)
    tunic, trousers, collar = layers[0], layers[1], layers[3]
    Z = LM["Z"]
    # ---- off: the button strip to the hem and the four pockets with their flaps (soldier.py's tunic)
    pocket_halves = [(0.006, 0.05, 0.058), (0.006, 0.06, 0.07), (0.0075, 0.054, 0.018), (0.0075, 0.064, 0.018)]
    keep = []
    gone = 0
    for how, p, k in tunic.ops:
        strip = isinstance(p, RoundBox) and abs(p.half[0] - 0.006) < 1e-6 and abs(p.half[1] - 0.014) < 1e-6 and p.half[2] > 0.08
        if how == "add" and (strip or any(_is_box(p, h) for h in pocket_halves)):
            gone += 1
            continue
        keep.append((how, p, k))
    tunic.ops = keep
    LM["pockets"] = []
    print(f"  SOV: {gone} shapes of the German tunic's front taken off (strip, pockets, flaps)")
    # ---- the stand collar in the tunic's own cloth
    collar.paint = "tunic"
    if spec.get("dress"):
        padded(man, J, LM, layers, spec["dress"])
        return layers
    # ---- the short placket: from the collar down to the breast, a little proud, its edge to the wearer's left
    nmid, nfwd, nrx, nry = LM["neck_ring"]
    top = nmid + nfwd * nrx
    low = Z(1.3)
    p, nrm = on_surface(tunic, (0.4, 0.004, (top[2] - 0.02 + low) / 2), (-1.0, 0.0, 0.0))
    half = (top[2] - 0.02 - low) / 2
    tunic.add(RoundBox(p - nrm * 0.003, (0.0055, 0.016, half), 0.004, frame_from(nrm, (0.0, 0.0, 1.0))), 0.006)
    LM["placket"] = (top[2] - 0.035, low + 0.015)
    # ---- the breeches: full in the thigh, the fullness on the outside, drawn in above the knee to the boot
    # (only below the tunic's hem, so none of it shows through the skirt)
    below_hem = HalfSpace((0.0, 0.0, LM["hem"] - 0.01), (0.0, 0.0, 1.0))
    for s, side in (("l", 1.0), ("r", -1.0)):
        hip, knee = np.array(J["thigh_" + s]), np.array(J["calf_" + s])
        at = lambda t, out, fwd: hip + (knee - hip) * t + np.array((fwd, side * out, 0.0))
        a, b, c = at(0.36, 0.014, 0.0), at(0.55, 0.03, 0.004), at(0.78, 0.016, 0.0)      # (the fullness to the side)
        trousers.add(Both(RoundCone(a, b, 0.07, 0.083), below_hem), 0.04)
        trousers.add(Both(RoundCone(b, c, 0.083, 0.062), below_hem), 0.04)
    return layers


S.uniform_layers = uniform_layers

PAD, PAD_LEGS = 0.023, 0.013               # (how much thicker the padded jacket and trousers stand than the summer cloth)
# the quilting, laid into the padding's own thickness: the cloth stands up in round tubes side by side, each tube's
# section a half-round, the stitched seams the creases between (on the body every 24 degrees round him, about 7 cm; down
# the sleeves and the trousers by each limb's own axis). (Tubes laid on as shapes piled into lumps where they bent over
# him, and grooves cut in read faint once the cloth was thinned to its faces: tubes in the thickness read as quilting.)
# (spacing, an angle round each part's own axis; height. A whole number of tubes all round each, 15 round the body, 7 round
#  a sleeve, 9 round a leg, so the rows close up behind with no part-tube)
QUILT = {"body": (math.radians(24.0), 0.012), "arm": (2.0 * math.pi / 7, 0.008), "leg": (2.0 * math.pi / 9, 0.009)}
QUILT_LIFT = 0.006                       # (the cloth under the tubes that much nearer the body, for the same bulk)


def _tubes(u, spacing, height):
    """A row of round tubes across u: zero at each seam (u a whole number of spacings), `height` between. (Smooth
    through the seams: the cloth is pushed out face by face, and a sharp crease came out in steps.)"""
    return height * (0.5 - 0.5 * np.cos(2.0 * np.pi * u / spacing))


def _round_axis(P, a, b, side):
    """For points P near the limb from a to b: how far along it (0 at a, 1 at b), and the angle round it (0 ahead,
    rising outward), how much each faces the body (1 straight at it), and how far each is from the axis."""
    d = (b - a) / np.linalg.norm(b - a)
    t = ((P - a) @ d) / np.linalg.norm(b - a)
    v = (P - a) - np.outer((P - a) @ d, d)
    fwd = np.array((1.0, 0.0, 0.0)) - d * d[0]
    fwd /= np.linalg.norm(fwd)
    out = np.cross(d, fwd) * (1.0 if np.cross(d, fwd)[1] * side > 0 else -1.0)
    ang = np.arctan2(v @ out, v @ fwd)
    inner = np.array((0.0, -side, 0.0)) - d * (-side * d[1])
    inner /= np.linalg.norm(inner)
    rad = np.linalg.norm(v, axis=1)
    facing = (v @ inner) / np.maximum(rad, 1e-9)
    return t, ang, facing, rad


# The body's rows go by length round him, not by angle about one point: at each height the outline of the cloth round
# the trunk (the arms left out), and how far round it each direction lies, so the 15 tubes are as wide down the back as
# across the front (by angle about a point just ahead of the spine they came out 2 cm wide behind and 8 cm in front;
# by length round the shells, under the cloth, still 4.5 cm down the spine, where the cloth bridges the groove). Each
# shell face takes its row where its cloth stands. Laid by padded() for the man being dressed; empty otherwise.
_BODY_ROWS = {}
_BINS = 144                                  # (2.5 degrees)


def _body_table(P):
    """_BODY_ROWS from points P on the trunk's cloth (its shells' faces pushed out to where the cloth stands, the arms
    left out): at each height (every cm) the trunk's middle front to back, and at every direction about it the
    outline's radius and the arc-length angle u there (0 straight ahead, a whole turn round)."""
    dth = 2.0 * np.pi / _BINS
    edges = -np.pi + np.arange(_BINS + 1) * dth
    out = {"z": [], "cx": [], "u": [], "r": [], "L": []}
    for z in np.arange(P[:, 2].min() + 0.02, P[:, 2].max() - 0.02, 0.01):
        sl = P[np.abs(P[:, 2] - z) < 0.012]
        mid = sl[np.abs(sl[:, 1]) < 0.04]
        if len(mid) < 4:
            continue
        cx = 0.5 * (mid[:, 0].max() + mid[:, 0].min())
        a = np.arctan2(sl[:, 1], sl[:, 0] - cx)
        r = np.hypot(sl[:, 0] - cx, sl[:, 1])
        rb = np.full(_BINS, -1.0)
        np.maximum.at(rb, np.clip(((a + np.pi) / dth).astype(int), 0, _BINS - 1), r)
        have = rb > 0.0
        if have.sum() < _BINS // 3:
            continue
        j = np.arange(_BINS)
        rb = np.interp(j, np.concatenate([j[have] - _BINS, j[have], j[have] + _BINS]), np.tile(rb[have], 3))
        for _ in range(3):                                        # (smoothed round: the faces' middles are ragged)
            rb = 0.25 * np.roll(rb, 1) + 0.5 * rb + 0.25 * np.roll(rb, -1)
        dr = (np.roll(rb, -1) - np.roll(rb, 1)) / (2.0 * dth)
        s = np.concatenate([[0.0], np.cumsum(np.sqrt(rb ** 2 + dr ** 2) * dth)])
        u = -np.pi + 2.0 * np.pi * s / s[-1]
        u -= np.interp(0.0, edges, u)                             # (0 straight ahead, where the front edges meet)
        out["z"].append(z)
        out["cx"].append(cx)
        out["u"].append(u)
        out["r"].append(np.concatenate([[0.5 * (rb[-1] + rb[0])], 0.5 * (rb[:-1] + rb[1:]), [0.5 * (rb[-1] + rb[0])]]))
        out["L"].append(s[-1])
    return {k: np.array(v) for k, v in out.items()}


def _body_u(P):
    """The arc-length angle u round the trunk at points P (by _BODY_ROWS), and the metres one unit of u spans there."""
    T = _BODY_ROWS
    z = np.clip(P[:, 2], T["z"][0], T["z"][-1])
    i = np.clip(np.searchsorted(T["z"], z) - 1, 0, len(T["z"]) - 2)
    f = np.clip((z - T["z"][i]) / (T["z"][i + 1] - T["z"][i]), 0.0, 1.0)
    cx = T["cx"][i] * (1.0 - f) + T["cx"][i + 1] * f
    th = np.arctan2(P[:, 1], P[:, 0] - cx)
    x = (th + np.pi) / (2.0 * np.pi) * _BINS
    j = np.clip(x.astype(int), 0, _BINS - 1)
    g = x - j

    def at(A):
        lo = A[i, j] * (1.0 - g) + A[i, j + 1] * g
        hi = A[i + 1, j] * (1.0 - g) + A[i + 1, j + 1] * g
        return lo * (1.0 - f) + hi * f
    u = (at(T["u"]) + np.pi) % (2.0 * np.pi) - np.pi
    L = T["L"][i] * (1.0 - f) + T["L"][i + 1] * f
    per = L / (2.0 * np.pi) * np.hypot(P[:, 0] - cx, P[:, 1]) / np.maximum(at(T["r"]), 1e-3)
    return u, per


QUILT_BONES = {"body": ("pelvis", "spine_01", "spine_02", "chest", "spine_03"), "arm_l": ("upperarm_l", "lowerarm_l"),
               "arm_r": ("upperarm_r", "lowerarm_r"), "leg_l": ("thigh_l", "calf_l"), "leg_r": ("thigh_r", "calf_r")}


def _rows(P, W, names, J, parts, force=None, Pu=None):
    """Where each point lies among the quilting's rows, on `parts` ("body", "arm", "leg"), each point counted to the
    part its bones (W, by `names`) mostly are (or every point to the part `force`): u (its angle round the part's axis;
    a seam wherever u is a whole number of spacings), the rows' spacing and height there, how surely it is quilted (0
    to 1: none across the front edge, under the arms or on the side of a limb toward the body, and faded where one
    part meets the next), the metres one unit of u spans there (its distance from the axis), and its part. On the
    body u is read at Pu where given (where the cloth stands over each point), else at P."""
    n = len(P)
    names = list(names)

    def col(bones):
        have = [names.index(b) for b in bones if b in names]
        return np.sum(W[:, have], axis=1) if have else np.zeros(n)
    keys = list(QUILT_BONES)
    if force:
        best, sure = np.full(n, keys.index(force)), np.ones(n)
    else:
        stack = np.stack([col(QUILT_BONES[k]) for k in keys], axis=1)
        best = np.argmax(stack, axis=1)
        sure = np.clip((stack.max(axis=1) / np.maximum(stack.sum(axis=1), 1e-9) - 0.6) / 0.3, 0.0, 1.0)
    u, sp, h, keep, per = np.zeros(n), np.ones(n), np.zeros(n), np.zeros(n), np.ones(n)
    if "body" in parts:
        m = best == keys.index("body")
        if _BODY_ROWS:
            phi, per[m] = _body_u((P if Pu is None else Pu)[m])
        else:
            cx = J["spine_01"][0] + 0.03
            phi = np.arctan2(P[m, 1], P[m, 0] - cx)
            per[m] = np.hypot(P[m, 0] - cx, P[m, 1])
        u[m], (sp[m], h[m]) = phi, QUILT["body"]
        keep[m] = np.clip((np.abs(phi) - 0.12) / 0.1, 0.0, 1.0) * np.clip((np.abs(np.cos(phi)) - 0.22) / 0.15, 0.0, 1.0)
    for s_, side in (("l", 1.0), ("r", -1.0)):
        if "arm" in parts:
            m = best == keys.index("arm_" + s_)
            sh, el, wr = (np.array(J[b + s_]) for b in ("upperarm_", "lowerarm_", "hand_"))
            up = ((P[m] - sh) @ (el - sh)) / ((el - sh) @ (el - sh)) < 1.0
            t1, a1, f1, r1 = _round_axis(P[m], sh, el, side)
            t2, a2, f2, r2 = _round_axis(P[m], el, wr, side)
            u[m], (sp[m], h[m]) = np.where(up, a1, a2), QUILT["arm"]
            keep[m] = np.clip((0.62 - np.where(up, f1, f2)) / 0.2, 0.0, 1.0) * np.where(up, np.clip((t1 - 0.22) / 0.1, 0.0, 1.0),
                                                                                          np.clip((0.86 - t2) / 0.06, 0.0, 1.0))
            per[m] = np.where(up, r1, r2)
        if "leg" in parts:
            m = best == keys.index("leg_" + s_)
            hip, knee = np.array(J["thigh_" + s_]), np.array(J["calf_" + s_])
            t, ang, fac, rad = _round_axis(P[m], hip, knee, side)
            u[m], (sp[m], h[m]) = ang, QUILT["leg"]
            keep[m] = np.clip((0.5 - fac) / 0.2, 0.0, 1.0)
            per[m] = rad
    return u, sp, h, keep * sure, per, best


def _band(z, z_lo, z_hi):
    return np.clip((z - z_lo) / 0.02, 0.0, 1.0) * np.clip((z_hi - z) / 0.02, 0.0, 1.0)


def _quilting(shell, J, LM, parts, z_lo, z_hi, force=None, Q=None):
    """The quilting's extra thickness at each face of `shell` (a ShellField), on `parts`, kept between z_lo and z_hi
    (the body's rows read at Q, where each face's cloth stands)."""
    def more(Fc):
        u, sp, h, k = _rows(Fc, shell.Wf, shell.names, J, parts, force, Q)[:4]
        return _tubes(u, sp, h) * k * _band(Fc[:, 2], z_lo, z_hi)
    return more


# the quilted cloths of the man being dressed (padded() lays them, shade_dress() reads where their seams fell, then they
# are let go): each its field, the shell it was pushed out from, and how its rows were laid
_QUILT_SRC = []


def _shell_bvh(src, shell, levels=2):
    """The shell `src` as ShellField made it into `shell` (its open ends capped, smoothed), as a BVH: to find, for a
    point of the cloth, the shell face it was pushed out from (the nearest), whose rows its tubes took."""
    import bmesh
    from mathutils.bvhtree import BVHTree
    ob = src.copy()
    ob.data = src.data.copy()
    bpy.context.scene.collection.objects.link(ob)
    for m in list(ob.modifiers):
        ob.modifiers.remove(m)
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if e.is_boundary], sides=0)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(ob.data)
    bm.free()
    ob.modifiers.new("smooth_for_field", "SUBSURF").levels = levels
    dg = bpy.context.evaluated_depsgraph_get()
    eo = ob.evaluated_get(dg)
    me = eo.to_mesh()
    V = np.zeros(len(me.vertices) * 3)
    me.vertices.foreach_get("co", V)
    V = V.reshape(-1, 3)
    F = [tuple(p.vertices) for p in me.polygons]
    eo.to_mesh_clear()
    tmp = ob.data
    bpy.data.objects.remove(ob)
    bpy.data.meshes.remove(tmp)
    Fc = np.array([V[list(f)].mean(axis=0) for f in F])
    if len(Fc) != len(shell.Fc) or not np.allclose(Fc, shell.Fc, atol=1e-6):
        raise RuntimeError("the shell came out differently from ShellField's: its faces would not match")
    return BVHTree.FromPolygons(V.tolist(), F)


def _face_normals(shell, bvh):
    """The outward normal of each face of `shell` (by its BVH, built as ShellField built it)."""
    N = np.zeros_like(shell.Fc)
    near = bvh.find_nearest
    for i, p in enumerate(shell.Fc.tolist()):
        N[i] = near(p)[1]
    return N


def _quilt_bands(LM, Z):
    """The heights the quilting runs between: the jacket (body and sleeves), and the trousers between the boots and the
    jacket's hem."""
    bt = LM["boot_top"]
    return {"jacket": (LM["hem"], Z(1.45)), "skirt": (LM["hem"] + 0.005, Z(1.45)), "legs": (bt + 0.03, LM["hem"] + 0.02)}


def padded(man, J, LM, layers, dress):
    """The padded jacket over the tunic's shapes (which it buries): the same cut, standing PAD further out, quilted in
    vertical tubes on the body and down the sleeves, its front edge overlapping; padded trousers quilted the same
    way; felt boots, thick and round, to below the knee. Under the snow suit (dress "snow") the same shapes, plain,
    with the hood's cloth falling soft round the neck onto the shoulders."""
    tunic, trousers, boots = layers[0], layers[1], layers[2]
    # (read out further than soldier.py's own: its fields are exact only within `band` of the shell, and the padded
    # cloth stands up to 7 cm off it; past the band they are coarse, and the cloth there broke into shards)
    tights, skirt = S.ShellField(man["tights"], band=0.08), S.ShellField(man["skirt"], band=0.08)
    Z = LM["Z"]
    BIG, OFF = 3.0, -0.06
    cuts = [op for op in tunic.ops if op[0] in ("cut", "sub")]          # (the hem, the neck and the wrists, laid again after)
    # soldier.py's tunic, each bone's ease plus the padding (the neck only a little: a collar, not a scarf)
    loose = {"pelvis": 0.017, "spine_01": 0.017, "spine_02": 0.015, "chest": 0.012, "clavicle_l": 0.01, "clavicle_r": 0.01,
             "neck": 0.007, "upperarm_l": 0.011, "upperarm_r": 0.011, "lowerarm_l": 0.009, "lowerarm_r": 0.009,
             "hand_l": 0.008, "hand_r": 0.008}
    pad = PAD - (QUILT_LIFT if dress == "winter" else 0.0)
    fat = {b: v + (0.004 if b == "neck" else pad * (0.8 if b.startswith(("lowerarm", "hand")) else 1.0)) for b, v in loose.items()}
    quilt = dress == "winter"
    QB = _quilt_bands(LM, Z)
    ARMS = {b + s_: OFF for b in ("upperarm_", "lowerarm_", "hand_", "fingers_", "thumb_") for s_ in ("l", "r")}
    LEGS = ("pelvis", "spine_01", "thigh_l", "thigh_r")
    flare = lambda P: 0.026 + pad + 0.02 * np.clip((LM["belt"] - P[:, 2]) / (LM["belt"] - LM["hem"]), 0.0, 1.0) ** 1.5
    box = dict(lo=(-BIG, -BIG, LM["hem"] - 0.02), hi=(BIG, BIG, LM["belt"] + 0.03))
    _QUILT_SRC.clear()
    _BODY_ROWS.clear()
    Q_top = Q_hp = Q_sk = None
    if quilt:
        bvh_t, bvh_s = _shell_bvh(man["tights"], tights), _shell_bvh(man["skirt"], skirt)
        # each shell face pushed out to where its cloth stands (before the tubes), and the body's rows laid round that
        N_t, N_s = _face_normals(tights, bvh_t), _face_normals(skirt, bvh_s)
        off_top = tights.Wf @ np.array([fat.get(n, OFF) for n in tights.names], np.float32)
        off_hp = tights.Wf @ np.array([0.0 if n in LEGS else OFF for n in tights.names], np.float32) + flare(tights.Fc)
        off_sk = skirt.Wf @ np.array([ARMS.get(n, 0.0) for n in skirt.names], np.float32) + flare(skirt.Fc)
        Q_top, Q_hp, Q_sk = (tights.Fc + N_t * off_top[:, None], tights.Fc + N_t * off_hp[:, None],
                             skirt.Fc + N_s * off_sk[:, None])
        zt, zs = tights.Fc[:, 2], skirt.Fc[:, 2]
        in_box = lambda z: (z > LM["hem"] - 0.02) & (z < LM["belt"] + 0.03)
        trunk = lambda sh: sh.Wf[:, [i for i, b in enumerate(sh.names)
                                     if b.startswith(("upperarm", "lowerarm", "hand", "fingers", "thumb"))]].sum(axis=1) < 0.5
        tt, ts = trunk(tights), trunk(skirt)
        _BODY_ROWS.update(_body_table(np.concatenate([Q_top[tt & (zt > LM["belt"] - 0.02) & (off_top > 0.0)],
                                                      Q_hp[tt & in_box(zt) & (off_hp > 0.0)],
                                                      Q_sk[ts & in_box(zs) & (off_sk > 0.0)]])))
    q_top = _quilting(tights, J, LM, ("body", "arm"), *QB["jacket"], Q=Q_top) if quilt else (lambda P: 0.0)
    # (below the belt every face counted to the body, though the shells there are mostly the thighs': the rows run on
    #  down the jacket's skirt to the hem)
    q_skirt = _quilting(skirt, J, LM, ("body",), *QB["skirt"], force="body", Q=Q_sk) if quilt else (lambda P: 0.0)
    top = tights.cloth(fat, lo=(-BIG, -BIG, LM["belt"] - 0.02), default=OFF, more=q_top)
    tunic.add(top, 0.02)
    q_hips = _quilting(tights, J, LM, ("body",), *QB["skirt"], force="body", Q=Q_hp) if quilt else (lambda P: 0.0)
    sk = skirt.cloth(ARMS, more=lambda P: flare(P) + q_skirt(P), **box)
    hp = tights.cloth({b: 0.0 for b in LEGS}, more=lambda P: flare(P) + q_hips(P), default=OFF, **box)
    tunic.add(Blend(sk, hp, 0.05), 0.02)
    if quilt:
        _QUILT_SRC.extend([
            dict(paint="tunic", group="top", field=top, bvh=bvh_t, Wf=tights.Wf, names=tights.names, parts=("body", "arm"), band=QB["jacket"], force=None),
            dict(paint="tunic", group="skirt", field=sk, bvh=bvh_s, Wf=skirt.Wf, names=skirt.names, parts=("body",), band=QB["skirt"], force="body"),
            dict(paint="tunic", group="skirt", field=hp, bvh=bvh_t, Wf=tights.Wf, names=tights.names, parts=("body",), band=QB["skirt"], force="body")])
        print("  SOV quilting: tubes in the jacket's thickness, body, skirt and sleeves")
    else:
        # the hood's cloth gathered round the neck and lying on the shoulders, all round (the hood itself rides the head)
        nmid, nfwd, nrx, nry = LM["neck_ring"]
        up = np.array((0.0, 0.0, 1.0)) - nfwd * nfwd[2]
        up /= np.linalg.norm(up)
        Rn = np.stack([nfwd, np.cross(up, nfwd), up], axis=1)
        tunic.add(Torus(nmid - up * 0.034, nrx + 0.046, 0.024, Rn, major_y=nry + 0.06), 0.022)
    tunic.ops += cuts
    # the front edge: the right side's edge lapped over the left, from the collar to the hem
    nmid, nfwd, nrx, nry = LM["neck_ring"]
    top = nmid + nfwd * nrx
    p, nrm = on_surface(tunic, (0.45, 0.012, (top[2] - 0.02 + LM["hem"]) / 2), (-1.0, 0.0, 0.0))
    half = (top[2] - 0.02 - LM["hem"]) / 2
    tunic.add(RoundBox(p - nrm * 0.004, (0.006, 0.02, half), 0.005, frame_from(nrm, (0.0, 0.0, 1.0))), 0.006)
    tunic.cut(HalfSpace((0.0, 0.0, LM["hem"]), (0.0, 0.0, -1.0)), 0.004)
    LM["front_edge"] = (top[2] - 0.035, LM["hem"] + 0.03)
    # ---- the padded trousers: fuller all the way down, drawn in where they go into the felt boots
    bt = LM["boot_top"]
    legs = {"pelvis": 0.012, "spine_01": 0.012, "thigh_l": 0.017, "thigh_r": 0.017, "calf_l": 0.009, "calf_r": 0.009,
            "foot_l": 0.006, "foot_r": 0.006, "ball_l": 0.006, "ball_r": 0.006}
    legs = {b: v + PAD_LEGS - (QUILT_LIFT if dress == "winter" else 0.0) for b, v in legs.items()}
    q_legs = _quilting(tights, J, LM, ("leg",), *QB["legs"]) if quilt else (lambda P: 0.0)
    lg = tights.cloth(legs, hi=(BIG, BIG, LM["belt"] + 0.04), default=OFF,
                      more=lambda P: q_legs(P) - (PAD_LEGS + 0.011) * np.clip((bt + 0.03 - P[:, 2]) / 0.05, 0.0, 1.0))
    trousers.add(lg, 0.02)
    if quilt:
        _QUILT_SRC.append(dict(paint="trousers", group="legs", field=lg, bvh=bvh_t, Wf=tights.Wf, names=tights.names, parts=("leg",),
                               band=QB["legs"], force=None))
    # ---- the felt boots: thick, soft, round in the toe, straight-topped just under the knee
    boots.add(tights.cloth({"calf_l": 0.027, "calf_r": 0.027, "foot_l": 0.016, "foot_r": 0.016, "ball_l": 0.015, "ball_r": 0.015},
                           hi=(BIG, BIG, bt + 0.02), default=OFF), 0.025)
    boots.cut(HalfSpace((0.0, 0.0, bt), (0.0, 0.0, 1.0)), 0.008)
    boots.cut(HalfSpace((0.0, 0.0, S.SOLE - 0.002), (0.0, 0.0, -1.0)), 0.0)
    print(f"  SOV padded: jacket +{PAD} m, trousers +{PAD_LEGS} m, felt boots to {bt:.2f} m")


# ================================================================================================ the kit
_build_kit = S.build_kit


def build_kit(J, spec, body_ls, Z=None):
    if not _ours(spec):
        return _build_kit(J, spec, body_ls, Z)
    out = _build_kit(J, spec, body_ls, Z)
    Z = Z or (lambda z: z)
    tunic, collar = body_ls[0], body_ls[3]
    for name in ("straps", "details", "roll"):              # (no Y-straps; his own buttons and roll below)
        ob = out.pop(name, (None, None))[0]
        if ob is not None:
            bpy.data.objects.remove(ob)
    # ---- the small things: three buttons down the placket, one on each shoulder board, a star on the buckle
    g = S.Geo()
    if spec.get("dress"):
        for z in np.linspace(Z(1.43), Z(1.11), 5):                     # (the jacket's five, on its lapped front edge)
            p, nrm = on_surface(tunic, (0.45, 0.012, z), (-1.0, 0.0, 0.0))
            g.tube(p - nrm * 0.001, p + nrm * 0.0055, 0.0085, 0.0075, "steel", 8)
        out["details"] = (g.build("details"), None)
        # the ammunition pouches again, each cell made on its own and the six joined: on the padded jacket the cells'
        # anchors tilt with the tubes, and two cells meeting at a thin seam came out as shards when made as one
        bpy.data.objects.remove(out.pop("pouches")[0])
        zb = Z(1.0)
        cx = J["spine_01"][0] + 0.03 if "crown" in J else 0.0
        cells = S.Geo()
        for side in (1.0, -1.0):
            for y in (0.06, 0.103, 0.143):
                a = math.atan2(y * side, 0.12)
                p0, n0 = S.project(tunic, [(cx + math.cos(a) * 0.2, math.sin(a) * 0.2, zb - 0.012)])
                p0, n0 = p0[0], n0[0]
                R = frame_from(n0, (0.0, 0.0, 1.0))
                c = p0 + n0 * 0.028
                L = Layer("pouches", "straps")
                L.add(RoundBox(c, (0.021, 0.0195, 0.04), 0.008, R), 0.0)
                L.add(RoundBox(c + R @ np.array((0.004, 0.0, 0.03)), (0.019, 0.021, 0.016), 0.006, R), 0.003)
                cells.absorb(S.mesh_from_layers("pouch", [L], 0.0025, 260 // 6))
        out["pouches"] = (cells.build("pouches", sharp=180.0), None)
        return out
    for z in (Z(1.418), Z(1.373), Z(1.328)):
        p, nrm = on_surface(tunic, (0.3, 0.004, z), (-1.0, 0.0, 0.0))
        g.tube(p - nrm * 0.001, p + nrm * 0.0032, 0.0072, 0.0062, "steel", 8)
    for side in (1.0, -1.0):
        p, nrm = on_surface(tunic, (J["clavicle_l"][0], side * 0.1, Z(1.62)), (0.0, 0.0, -1.0))
        g.tube(p - nrm * 0.001, p + nrm * 0.0035, 0.0068, 0.0058, "steel", 8)
    out["details"] = (g.build("details"), None)
    # ---- the greatcoat rolled into a ring, over the left shoulder and down across the chest and back to the right hip
    L = Layer("roll", "blanket")
    pts = []
    for k in range(17):
        t = 2.0 * math.pi * k / 16.0
        lift = math.sin(t)
        pts.append((0.165 * math.cos(t) - 0.02, 0.03 + 0.185 * lift, Z(1.2 + 0.255 * lift)))
    for k in range(16):
        L.add(RoundCone(pts[k], pts[k + 1], 0.047, 0.047), 0.02)
    # the two ends tied where they meet at the right hip, the tie a little proud
    tie = np.array(pts[12])
    L.add(Sphere(tie + np.array((0.0, -0.006, 0.0)), 0.053), 0.012)
    out["roll"] = (S.mesh_from_layers("roll", [L], 0.005, 300), None)
    return out


S.build_kit = build_kit


# ================================================================================================ the winter hats
def _lathe(J, ax, ay, rings, sides=36):
    """Rings about the head, on the helmet's own seat: each ring a function of the angle f round him (0 ahead) giving
    (s, a, z): that ring's point lies at s times the head's oval plus a, at z over the seat. The oval is as wide as his
    skull (as soldier.py seats its helmets)."""
    C = np.array((J["skull_x"] - 0.008, 0.0, J["crown"] - 0.108))
    ax *= (J["skull_half"][0] + 0.023) / 0.122
    ay *= (J["skull_half"][1] + 0.025) / 0.108
    out = []
    for prof in rings:
        ring = []
        for i in range(sides):
            f = 2 * math.pi * i / sides
            rho = 1.0 / math.sqrt((math.cos(f) / ax) ** 2 + (math.sin(f) / ay) ** 2)
            sc, a, z = prof(f)
            r = rho * sc + a
            ring.append(C + np.array((math.cos(f) * r, math.sin(f) * r, z)))
        out.append(ring)
    return C, out


def _by_angle(front, side, back):
    """A value going smoothly from ahead (front) to the sides (side) to behind (back)."""
    def v(f):
        c = math.cos(f)
        return side + (front - side) * c * c if c > 0 else side + (back - side) * c * c
    return v


_build_helmet = S.build_helmet


def build_helmet(kind, J, sides=None):
    if kind not in ("ushanka", "snowhood"):
        return _build_helmet(kind, J, sides)
    g = S.Geo()
    if kind == "ushanka":
        # the fur cap, flaps tied up: a soft, flattish crown, then thick fur all round (the front flap turned up tallest)
        # (the crown is cloth, `helmet`; the fur is `blanket`, which this dress has no other use for)
        lo, hi = _by_angle(0.026, 0.0, -0.022), _by_angle(0.1, 0.118, 0.07)
        az = 0.158
        crown = []
        for deg in (72.0, 52.0, 32.0, 14.0):                           # the crown: a squarish dome down to the fur
            a = math.radians(deg)
            crown.append(lambda f, a=a: (math.cos(a) ** (2 / 3.4), 0.0, hi(f) - 0.01 + (az - hi(f) + 0.01) * math.sin(a) ** (2 / 3.4)))
        crown += [lambda f: (1.0, 0.0, hi(f) - 0.012), lambda f: (0.8, 0.0, hi(f) - 0.03), lambda f: (0.4, 0.0, hi(f) - 0.03)]
        C, rings = _lathe(J, 0.112, 0.104, crown)
        g.shell([[C + np.array((0.0, 0.0, az))]] + rings, "helmet")
        # the fur: the flaps folded up all round, thick and soft, rising highest at the sides where the ear flaps lie
        fur = [lambda f: (1.0, -0.004, hi(f) - 0.004), lambda f: (1.0, 0.014, hi(f) + 0.008), lambda f: (1.0, 0.032, hi(f) - 0.01),
               lambda f: (1.0, 0.04, (hi(f) + lo(f)) / 2), lambda f: (1.0, 0.034, lo(f) + 0.014), lambda f: (1.0, 0.016, lo(f)),
               lambda f: (1.0, -0.004, lo(f) + 0.004)]
        C, rings = _lathe(J, 0.112, 0.104, fur)
        rings.append(rings[0])                                        # (a closed roll: the last ring back to the first)
        g.shell(rings, "blanket")
        # the flaps' tapes tied over the crown
        g.tube(C + np.array((-0.012, -0.06, az - 0.004)), C + np.array((0.004, 0.06, az - 0.004)), 0.0042, 0.0042, "straps", 6)
    else:
        # the snow suit's hood, up: over the crown, framing the face, down to the throat at the sides and behind to the
        # shoulders, a little full at the back
        az = 0.152
        edge = _by_angle(0.075, -0.19, -0.232)

        def at(f, t, inner=0.0):
            z = az + (edge(f) - az) * t
            sc = math.sqrt(max(0.0, 1.0 - (z / az) ** 2)) if z > 0 else 1.0
            back = max(0.0, -math.cos(f))
            a = 0.024 + 0.035 * back * max(0.0, (-0.06 - z) / 0.15) - inner
            a += 0.012 * back * max(0.0, z / az) ** 0.7 * max(0.0, 1.0 - z / az) * 2.0      # (fuller over the back of the head)
            low = max(0.0, (-0.07 - z) / 0.16)                                               # (0 at the ears, 1 at the foot)
            side = 1.0 - max(0.0, math.cos(f)) ** 4                                         # (none across the face)
            a += side * (0.034 * low ** 1.6 + 0.006 * low * math.sin(7.0 * f + 0.6))       # (falling out, in soft folds)
            return sc, a, z
        ts = (0.12, 0.25, 0.4, 0.55, 0.66, 0.76, 0.85, 0.93, 1.0)
        rings = [lambda f, t=t: at(f, t) for t in ts] + [lambda f, t=t: at(f, t, 0.008) for t in ts[::-1]]
        C, rings = _lathe(J, 0.13, 0.122, rings)
        g.shell([[C + np.array((0.0, 0.0, az))]] + rings + [[C + np.array((0.0, 0.0, az - 0.008))]], "helmet")
        # the edge round the face drawn in on its cord: a soft rolled hem from throat to throat over the brow
        C, (rim,) = _lathe(J, 0.13, 0.122, [lambda f: at(f, 0.985, 0.004)])
        n = len(rim)
        for i in range(-n // 4 - 1, n // 4 + 1):
            g.tube(rim[i % n], rim[(i + 1) % n], 0.0068, 0.0068, "helmet", 8)
    return g.build("helmet_" + kind, sharp=55.0)


S.build_helmet = build_helmet


# ================================================================================================ the shade (COLOR_0)
# The winter dresses' seams and wear as a darkening the game multiplies each paint by (the paints stay by number, as
# the soldiers' own table has them): 1 for clean cloth, SHADE["seam"] along the quilting's stitched seams, SHADE["wear"]
# at the knees, the cuffs, the jacket's hem and the felt boots' toes; on the snow suit SHADE["grey"] at its hem and knees
# (greyed, not tinted). On the near model only (the mid and far stay plain), as its colour attribute `shade`, written to
# the glb as COLOR_0. A man without it (the summer dress, every other army) stays plain.
SHADE = {"seam": 0.75, "wear": 0.82, "grey": 0.85}
SEAM_HALF = 0.005       # (each seam a line 1 cm wide: the near model is cut along both its edges and the faces between
                        #  painted dark, since its points lie 2 to 3 cm apart and a line laid on the points missed most)
SEAM_REACH = 0.35       # (a seam whose middle is in the quilted zone is cut whole, its far edge up to this much outside)
SEAM_LEAST = 2.5e-4     # (a seam's piece smaller than this, m2, is a scrap at a zone's edge: left plain)


def _seam_field(P, paint, J, force=None):
    """For points P of the cloth painted `paint`, where each lies among the quilting's rows as its tubes were laid: on
    each quilted cloth laid for that paint, through the shell face that cloth was pushed out from there (the nearest,
    as in ShellField); taken from the cloth it lies on (the least of their fields there), blended with any other within
    its smooth join of that one, as the surface is. Returns w (its place across the rows, in spacings: a seam at every
    whole number), a seam's half-width in those units, the rows all round, how far inside the quilted zone it is
    (above 0: quilted), and its part (a seam runs only between points of one part). On the body u is read at the
    cloth point itself (the tubes' rows were laid where the cloth stands). `force` counts every point to that part."""
    n = len(P)
    w, hw, rnd, q, key = np.zeros(n), np.full(n, 0.1), np.ones(n), np.full(n, -1.0), np.full(n, -1)
    src = [s for s in _QUILT_SRC if s["paint"] == paint]
    if not src:
        return w, hw, rnd, q, key
    D = np.stack([np.asarray(s["field"].d(P[:, 0], P[:, 1], P[:, 2])).ravel() for s in src])
    Ws, HW, R, Q, K = [], [], [], [], []
    for s in src:
        foot, fi = np.zeros((n, 3)), np.zeros(n, np.int64)
        near = s["bvh"].find_nearest
        for j, p in enumerate(P.tolist()):
            loc, _, idx, _ = near(p)
            foot[j], fi[j] = loc, idx
        Wf = s["Wf"][fi]
        fo = force or s["force"]
        u, sp, h, k, _, best = _rows(foot, Wf, s["names"], J, s["parts"], fo, P)
        per = _rows(P, Wf, s["names"], J, s["parts"], fo)[4]                  # (the line's width on the cloth itself)
        Ws.append(u / sp)
        HW.append(SEAM_HALF / np.maximum(per * sp, 1e-6))
        R.append(np.round(2.0 * np.pi / sp))
        Q.append(np.where(h > 0, np.minimum(k - 0.25, _band(foot[:, 2], *s["band"]) - 0.5), -1.0))
        K.append(np.where(h > 0, best, -1))
    Ws, HW, R, Q, K = (np.stack(a) for a in (Ws, HW, R, Q, K))
    o = np.argmin(D, axis=0)
    at = np.arange(n)
    w, hw, rnd, q, key = Ws[o, at], HW[o, at], R[o, at], Q[o, at], K[o, at]
    # (smooth joins: two cloths of the jacket's skirt over 5 cm, the skirt to the jacket above over 2 cm)
    grp = [s["group"] for s in src]
    gid = np.array([grp.index(g) for g in grp])
    tot, dw, dh, dq = np.ones(n), np.zeros(n), np.zeros(n), np.zeros(n)
    for i in range(len(src)):
        kk = np.where(gid[o] == gid[i], 0.05, 0.02)
        g = np.clip(0.5 - 0.5 * (D[i] - D[o, at]) / kk, 0.0, 0.5)
        g[(o == i) | (K[i] != key) | (R[i] != rnd)] = 0.0
        a = g / (1.0 - g)
        d = Ws[i] - w
        d -= rnd * np.round(d / rnd)
        dw, dh, dq, tot = dw + a * d, dh + a * (HW[i] - hw), dq + a * (Q[i] - q), tot + a
    w = w + dw / tot
    w = (w + 0.5 * rnd) % rnd - 0.5 * rnd
    return w, hw + dh / tot, rnd, q + dq / tot, key


def _cut_seams(body, J):
    """Cuts the body's quilted cloth along both edges of every seam (where w is a whole number less or more a seam's
    half-width), however many of them an edge crosses, and joins each cut across its faces; then marks the faces
    between a seam's two cuts whose middle is in the quilted zone, and leaves out scraps. The cuts' new points take
    their neighbours' weights and attributes. Returns the number of points cut in and, face by face of the cut mesh,
    whether it lies on a seam."""
    import bmesh
    me = body.data
    paint_of_mat = [_paint_name(mt) for mt in me.materials]
    fams = ("tunic", "trousers")
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if paint_of_mat[f.material_index] in fams and len(f.verts) > 3])
    bm.verts.ensure_lookup_table()
    bm.verts.index_update()
    P = np.array([v.co[:] for v in bm.verts])
    field = {p: _seam_field(P, p, J) for p in fams}
    # (where the body's points meet a sleeve's on one edge, the body's seams are read on the body's rows at both ends)
    body_f = _seam_field(P, "tunic", J, force="body")
    # (each point's w eased off the cut lines, so no cut falls on or a hair from a point)
    EPS = 0.02
    for w, hw, rnd, q, key in list(field.values()) + [body_f]:
        for s in (-1.0, 1.0):
            dd = w - np.round(w) - s * hw
            close = np.abs(dd) < EPS
            w[close] += np.where(dd[close] >= 0.0, EPS - dd[close], -EPS - dd[close])
    new = {}                                                    # (cut point: w, half-width, part, rows round, zone)
    label = {}                                                  # (cut point: which line it is on)
    jobs = []
    for e in bm.edges:
        fp = {paint_of_mat[f.material_index] for f in e.link_faces}
        if len(fp) != 1 or next(iter(fp)) not in field:
            continue
        p = next(iter(fp))
        w, hw, rnd, q, key = field[p]
        a, b = e.verts[0].index, e.verts[1].index
        if p == "tunic" and key[a] != key[b] and min(key[a], key[b]) == 0:
            w, hw, rnd, q, key = body_f
        if key[a] < 0 or key[a] != key[b] or rnd[a] != rnd[b] or max(q[a], q[b]) <= -SEAM_REACH:
            continue
        R = int(rnd[a])
        d = w[b] - w[a]
        d -= R * np.round(d / R)                               # (across the seam behind, the short way)
        if abs(d) < 1e-9 or abs(d) > 1.5:
            continue
        he = 0.5 * (hw[a] + hw[b])
        lo, hi = min(w[a], w[a] + d), max(w[a], w[a] + d)
        cs = []
        for s in (-1, 1):
            for k in range(int(np.ceil(lo - s * he)), int(np.floor(hi - s * he)) + 1):
                t = (k + s * he - w[a]) / d
                qt = q[a] + t * (q[b] - q[a])
                tc = min(max((k - w[a]) / d, 0.0), 1.0)          # (the seam's middle, or the edge's end nearest it)
                inside = max(qt, q[a] + tc * (q[b] - q[a])) > 0.0
                if 1e-6 < t < 1.0 - 1e-6 and qt > -SEAM_REACH and inside:
                    cs.append((t, (p, int(key[a]), k % R, s), w[a] + t * d, he, int(key[a]), R, qt))
        if cs:
            jobs.append((e, e.verts[0], e.verts[1], sorted(cs)))
    for e, v0, v1, cs in jobs:
        cur, start, t0 = e, v0, 0.0
        for t, lab, wn, he, ky, R, qt in cs:
            _, nv = bmesh.utils.edge_split(cur, start, (t - t0) / (1.0 - t0))
            label[nv], new[nv] = lab, (wn, he, ky, R, qt)
            cur = next(ed for ed in nv.link_edges if v1 in ed.verts)
            start, t0 = nv, t
    # each line joined across every face it crosses (two points of one line in one face)
    pairs = set()
    for f in bm.faces:
        on = {}
        for v in f.verts:
            if v in label:
                on.setdefault(label[v], []).append(v)
        for vs in on.values():
            if len(vs) == 2:
                pairs.add(frozenset(vs))
    joined = 0
    for pr in pairs:
        va, vb = tuple(pr)
        if any(vb in ed.verts for ed in va.link_edges):
            continue
        common = set(va.link_faces) & set(vb.link_faces)
        if common:
            bmesh.utils.face_split(next(iter(common)), va, vb)
            joined += 1
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if paint_of_mat[f.material_index] in fams and len(f.verts) > 3])
    # the faces between a seam's two cuts: their middle in the quilted zone and within a half-width of a whole w, and
    # every corner on or inside the seam's edges (a face the cuts did not close, at a line's end or a paint's edge, left)
    bm.faces.ensure_lookup_table()
    bm.faces.index_update()
    seam = np.zeros(len(bm.faces), bool)
    # (the zone read again at each face's own middle: corners in it can span a face whose middle is not)
    FC = np.array([f.calc_center_median()[:] for f in bm.faces])
    fpaint = np.array([paint_of_mat[f.material_index] for f in bm.faces], dtype=object)
    QC, QCb = np.full(len(FC), -1.0), np.full(len(FC), -1.0)
    for p in fams:
        mm = fpaint == p
        if mm.any():
            QC[mm] = _seam_field(FC[mm], p, J)[3]
    mm = fpaint == "tunic"
    if mm.any():
        QCb[mm] = _seam_field(FC[mm], "tunic", J, force="body")[3]
    for f in bm.faces:
        p = paint_of_mat[f.material_index]
        if p not in field:
            continue
        w, hw, rnd, q, key = field[p]
        orig = {int(key[v.index]) for v in f.verts if v not in new}
        qc = QC[f.index]
        if p == "tunic" and len(orig) > 1 and min(orig) == 0:
            w, hw, rnd, q, key = body_f
            qc = QCb[f.index]
        if qc <= 0.0:
            continue
        vals = [new[v] if v in new else (w[v.index], hw[v.index], int(key[v.index]), int(rnd[v.index]), q[v.index])
                for v in f.verts]
        if vals[0][2] < 0 or len({x[2] for x in vals}) != 1:
            continue
        R, w0 = vals[0][3], vals[0][0]
        ws = np.array([x[0] - R * np.round((x[0] - w0) / R) for x in vals])
        hc, wc = np.mean([x[1] for x in vals]), ws.mean()
        kc = np.round(wc)
        seam[f.index] = abs(wc - kc) < hc and np.all(np.abs(ws - kc) <= 1.35 * hc)
    # scraps left plain: pieces of seam (faces joined by edges) smaller than SEAM_LEAST
    seen, dropped = set(), 0
    for f in bm.faces:
        if not seam[f.index] or f.index in seen:
            continue
        comp, stack = [], [f]
        seen.add(f.index)
        while stack:
            g = stack.pop()
            comp.append(g)
            for ed in g.edges:
                for h in ed.link_faces:
                    if seam[h.index] and h.index not in seen:
                        seen.add(h.index)
                        stack.append(h)
        if sum(g.calc_area() for g in comp) < SEAM_LEAST:
            for g in comp:
                seam[g.index] = False
            dropped += 1
    bm.to_mesh(me)
    bm.free()
    me.update()
    print(f"  SOV seams: {len(new)} points cut in, {joined} lines joined across faces, {int(seam.sum())} seam faces "
          f"({dropped} scraps left plain)")
    return len(new), seam


def _paint_name(mt):
    """A material's paint (its name without a copy's number or a picture's army prefix)."""
    nm = mt.name.split(".")[0] if mt else ""
    for pre in ("SOV_", "SOVW_", "SOVS_", "GER_"):
        nm = nm[len(pre):] if nm.startswith(pre) else nm
    return nm


def _wear_regions(P, J, LM, knee_on=None):
    """How much (0 to 1) each point is at the jacket's hem, a cuff, a knee's front or a boot's toe. The knee's patch
    sits on the trousers' front just above the felt boot; its middle lies inside the leg, so it is scaled to be
    full at the darkest of the points `knee_on` (the trousers')."""
    z, x = P[:, 2], P[:, 0]
    hem = np.clip((LM["hem"] + 0.045 - z) / 0.03, 0.0, 1.0) * (z > LM["hem"] - 0.03)
    cuff, knee, toe = np.zeros(len(P)), np.zeros(len(P)), np.zeros(len(P))
    for s_, side in (("l", 1.0), ("r", -1.0)):
        el, wr = np.array(J["lowerarm_" + s_]), np.array(J["hand_" + s_])
        t, _, _, rad = _round_axis(P, el, wr, side)
        cuff = np.maximum(cuff, np.clip((t - 0.7) / 0.07, 0.0, 1.0) * (t < 1.12) * (rad < 0.11))
        k = np.array(J["calf_" + s_]) + np.array((0.05, 0.0, 0.035))
        e = (P - k) / np.array((0.07, 0.075, 0.095))
        ks = np.clip(1.0 - (e ** 2).sum(axis=1), 0.0, 1.0) ** 0.6 * (x > J["calf_" + s_][0] - 0.01)
        if knee_on is not None and ks[knee_on].max() > 0.0:
            ks = np.clip(ks / ks[knee_on].max(), 0.0, 1.0)
        knee = np.maximum(knee, ks)
        b = np.array(J["ball_" + s_])
        toe = np.maximum(toe, np.clip((x - (b[0] - 0.035)) / 0.04, 0.0, 1.0) * np.clip((b[2] + 0.07 - z) / 0.03, 0.0, 1.0)
                         * (np.abs(P[:, 1] - b[1]) < 0.09))
    return hem, cuff, knee, toe


def shade_dress(body, J, LM, dress):
    """Lays the `shade` colour attribute on the man's body (the uniform's faces; the skin stays 1), per face corner so a
    paint's edge stays clean: for the winter dress the seams first cut into the cloth and their faces painted, then the
    wear over all. Returns the share of the uniform's corners darkened, by kind."""
    me = body.data
    me.validate(verbose=False, clean_customdata=False)          # (a face the thinning left doubled, taken out first)
    cut, seam_face = _cut_seams(body, J) if dress == "winter" else (0, None)
    n = len(me.vertices)
    P = np.zeros(n * 3, np.float32)
    me.vertices.foreach_get("co", P)
    P = P.reshape(-1, 3).astype(float)
    nl, nf = len(me.loops), len(me.polygons)
    lv = np.zeros(nl, np.int64)
    me.loops.foreach_get("vertex_index", lv)
    ls_, lt, mi = np.zeros(nf, np.int64), np.zeros(nf, np.int64), np.zeros(nf, np.int64)
    me.polygons.foreach_get("loop_start", ls_)
    me.polygons.foreach_get("loop_total", lt)
    me.polygons.foreach_get("material_index", mi)
    paint_of_mat = [_paint_name(mt) for mt in me.materials]
    face_paint = np.array([paint_of_mat[i] if i < len(paint_of_mat) else "" for i in mi], dtype=object)
    loop_face = np.repeat(np.arange(nf), lt)
    loop_paint = face_paint[loop_face]
    tun, tro, bts = loop_paint == "tunic", loop_paint == "trousers", loop_paint == "boots"
    on_trousers = np.zeros(n, bool)
    on_trousers[lv[tro]] = True
    hem, cuff, knee, toe = _wear_regions(P, J, LM, on_trousers)
    seam_face = np.zeros(nf) if seam_face is None else seam_face.astype(float)
    dark = np.zeros(nl)
    wear, grey, seam_d = 1.0 - SHADE["wear"], 1.0 - SHADE["grey"], 1.0 - SHADE["seam"]
    if dress == "winter":
        dark[tun] = np.maximum(seam_d * seam_face[loop_face[tun]], wear * np.maximum(hem[lv[tun]], cuff[lv[tun]]))
        dark[tro] = np.maximum(seam_d * seam_face[loop_face[tro]], wear * knee[lv[tro]])
    else:
        dark[tun] = grey * hem[lv[tun]]
        dark[tro] = grey * knee[lv[tro]]
    dark[bts] = wear * np.maximum(toe[lv[bts]], knee[lv[bts]])
    sh = (1.0 - dark).astype(np.float32)
    if "shade" in me.color_attributes:
        me.color_attributes.remove(me.color_attributes["shade"])
    ca = me.color_attributes.new("shade", "FLOAT_COLOR", "CORNER")
    rgba = np.ones((nl, 4), np.float32)
    rgba[:, :3] = sh[:, None]
    ca.data.foreach_set("color", rgba.ravel())
    me.color_attributes.active_color = ca
    me.color_attributes.render_color_index = me.color_attributes.find("shade")
    cloth = tun | tro | bts
    stats = {"seam_points_cut": cut, "seam_faces": int(seam_face.sum()), "darkened": round(float(np.mean(dark[cloth] > 0.02)), 3),
             "min": round(float(sh.min()), 3), "triangles": S.tris_of(body)}
    print(f"  SOV shade ({dress}): {stats}")
    return stats


_assemble = S.assemble


def assemble(nation, weapon="rifle", seed=None):
    try:
        m = _assemble(nation, weapon, seed)
        dress = S.NATION_KIT.get(nation, {}).get("dress")
        if dress:
            m["body_uncut"] = m["body"].data.copy()          # (the mid and far are thinned from the body before its seams)
            m["shade"] = shade_dress(m["body"], m["J"], m["LM"], dress)
    finally:
        _QUILT_SRC.clear()                                   # (the quilted cloths' fields let go)
        _BODY_ROWS.clear()
    return m


S.assemble = assemble

# the pictures' materials multiply by `shade` where a mesh has it (and stay as they are where it has none)
_studio_wear = S.studio_wear


def studio_wear(m, paint):
    m = _studio_wear(m, paint)
    nt = m.node_tree
    if nt.nodes.get("shade_done"):
        return m
    base_in = nt.nodes["Principled BSDF"].inputs["Base Color"]
    if not base_in.links:
        return m
    src = base_in.links[0].from_socket
    at = nt.nodes.new("ShaderNodeAttribute")
    at.attribute_name = "shade"
    sep = nt.nodes.new("ShaderNodeSeparateColor")
    nt.links.new(at.outputs["Color"], sep.inputs["Color"])
    has = nt.nodes.new("ShaderNodeMath")
    has.operation = "GREATER_THAN"
    has.inputs[1].default_value = 0.01
    nt.links.new(sep.outputs["Red"], has.inputs[0])
    pick = nt.nodes.new("ShaderNodeMix")
    pick.data_type = "RGBA"
    nt.links.new(has.outputs["Value"], pick.inputs["Factor"])
    pick.inputs["A"].default_value = (1.0, 1.0, 1.0, 1.0)
    nt.links.new(at.outputs["Color"], pick.inputs["B"])
    mul = nt.nodes.new("ShaderNodeMix")
    mul.data_type = "RGBA"
    mul.blend_type = "MULTIPLY"
    mul.inputs["Factor"].default_value = 1.0
    nt.links.new(src, mul.inputs["A"])
    nt.links.new(pick.outputs["Result"], mul.inputs["B"])
    nt.links.new(mul.outputs["Result"], base_in)
    tag = nt.nodes.new("NodeFrame")
    tag.name = "shade_done"
    return m


S.studio_wear = studio_wear


def _export_near(path, objs):
    """kit.export_glb's glb (every clip) with the body's `shade` written as COLOR_0."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    kit._select_only(objs)
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True, export_animations=True,
                              export_animation_mode="ACTIONS", export_force_sampling=True, export_frame_step=1,
                              export_def_bones=False, export_apply=True, export_yup=True, export_skins=True,
                              export_morph=False, export_extras=True, export_vertex_color="ACTIVE",
                              export_all_vertex_colors=False)
    return path


# ================================================================================================ the files
def export_sov(info, dress="summer"):
    """soldier_SOV.glb (LOD0, every clip), soldier_SOV_LOD1.glb and _LOD2.glb (same skeleton, no clips) and
    soldier_SOV.json (the game's own form); for a winter dress the same with its suffix (soldier_SOV_winter.glb...).
    No .fbx: the game reads the json and the glb (and the share is near full)."""
    t0 = time.time()
    tag, suffix = DRESSES[dress]
    base = f"soldier_{NATION}{suffix}"
    sc = kit.fresh(S.FPS)
    m = S.assemble(tag)
    m["arm"].name = "soldier_" + NATION                     # (every dress the same rig, by the same names)
    m["body"].name = "body_" + NATION
    S.bake_clips(m)
    socks = m["sockets"]
    lods = {0: S.man_objects(m)}
    cut = m["body"].data
    if "body_uncut" in m:
        # (the seams' cuts, near only, kept out of the mid and far: their thinning shares the triangles by the near count)
        m["body"].data = m["body_uncut"]
    lods[1], lods[2] = S.make_lods(m, 1), S.make_lods(m, 2)
    m["body"].data = cut
    if "body_uncut" in m:
        bpy.data.meshes.remove(m.pop("body_uncut"))
    sc.frame_set(0)
    n = {lod: sum(S.tris_of(o) for o in lods[lod]) for lod in lods}
    rec = info.setdefault(dress, {})
    rec["tris"] = n
    rec["parts"] = {o.name.split(".")[0]: S.tris_of(o) for o in lods[0]}
    rec["short"] = {k: float(v) for k, v in m["short"].items()}
    for lod in (1, 2):                                       # (the mid and far stay plain)
        for o in lods[lod]:
            if o.type == "MESH" and "shade" in o.data.color_attributes:
                o.data.color_attributes.remove(o.data.color_attributes["shade"])
    if "shade" in m:
        _export_near(S.stage_path(f"{base}.glb"), [m["arm"]] + lods[0] + socks)
        rec["shade"] = m["shade"]
    else:
        S.export_glb(S.stage_path(f"{base}.glb"), [m["arm"]] + lods[0] + socks)
    for lod in (1, 2):
        S.export_glb(S.stage_path(f"{base}_LOD{lod}.glb"), [m["arm"]] + lods[lod] + socks, anim=False)
    data = S.write_game_json(S.stage_path(f"{base}.json"), m, lods)
    data["about"] = data["about"].replace("soldier.py.", "soldier_sov.py on soldier.py.")
    if dress != "summer":
        data["about"] += f" Dress: {dress}; paint by its colours below (they differ from the summer man's)."
        data["colours"] = dict(S.LOOK[tag])
        kinds = ([f"seams {SHADE['seam']}"] if dress == "winter" else []) + [f"wear {SHADE['wear']}"] + \
            ([f"the snow suit greyed {SHADE['grey']}"] if dress == "snow" else [])
        data["about"] += f" The near glb carries COLOR_0, a darkening to multiply each paint by: {', '.join(kinds)}; the mid and far are plain."
    with open(S.stage_path(f"{base}.json"), "w") as f:
        json.dump(data, f, separators=(",", ":"))
    rec["json"] = {k: len(data[k]["triangles"]) // 3 for k in ("near", "mid", "far")}
    rec["high"] = data["high"]
    for n_ in (f"{base}.glb", f"{base}_LOD1.glb", f"{base}_LOD2.glb", f"{base}.json"):
        S.deliver(n_)
    rec["export_s"] = round(time.time() - t0)
    print(f"  {base} exported in {time.time() - t0:.0f} s: {n}")
    return info


# ================================================================================================ his picture
def picture(info, fast=False, dresses=None):
    """The Soviet man and the German side by side at ease, from the front quarter and from behind, on the studio
    backdrop: one sheet, pictures/soldier_SOV.png."""
    kit.fresh(S.FPS)
    men = []
    winter = dresses is not None
    cast = [(DRESSES[d][0], y) for d, y in zip(dresses, (-1.1, 0.0, 1.1))] if winter else [("SOV", -0.55), ("GER", 0.55)]
    for nation, y in cast:
        m = S.assemble(nation)
        if "body_uncut" in m:
            bpy.data.meshes.remove(m.pop("body_uncut"))
        S.studio_smooth(m)
        for mt in bpy.data.materials:
            if not mt.name.startswith(("SOV_", "SOVW_", "SOVS_", "GER_", "backdrop", "ground")):
                mt.name = nation + "_" + mt.name
        m["arm"].location = (0.0, y, 0.0)
        P = S.base_stand()
        P["foot_l"] = {"at": (0.12, 0.12, S.ANKLE), "yaw": 6.0}
        P["foot_r"] = {"at": (-0.06, -0.12, S.ANKLE), "yaw": -8.0}
        S.pose_once(m["arm"], m["rig"], P, 0)
        men.append(m)
    kit.studio()
    kit.backdrop("CFC6B4")
    cam = kit.camera(lens=50)
    S.shared_render(640 if fast else 1100, 760 if fast else 1300, 12 if fast else 48)
    shots = []
    views = (("front", 25.0, 6.0, 5.6 if winter else 4.6), ("back", 200.0, 8.0, 5.6 if winter else 4.6), ("near", 15.0, 4.0, 2.1))
    for key, yaw, pitch, dist in views:
        aim_y = (-0.6 if winter else -0.2) if key == "near" else 0.0
        kit.orbit(cam, (0.0, aim_y, 1.0 if key != "near" else 1.3), dist, pitch, yaw)
        path = os.path.join(S.SCRATCH, "pictures", f"sov{'_winter' if winter else ''}_{key}.png")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        kit.still(path)
        shots.append(path)
    name = f"soldier_{NATION}{'_winter' if winter else ''}.png"
    out = S.stage_path("pictures", name)
    if winter:
        kit.sheet(shots, out, cols=3, labels=[" / ".join(dresses) + ", left to right", "From behind",
                                              "Close: quilted jacket, fur cap, felt boots"],
                  title="Soviet rifleman's dresses: soldier_sov.py on soldier.py (our own design)")
    else:
        kit.sheet(shots, out, cols=3, labels=["Soviet rifleman (left) beside the German, at ease", "From behind: the rolled greatcoat",
                                              "Close: stand collar, short placket, shoulder boards"],
                  title="Soviet rifleman: soldier_sov.py on soldier.py (our own design)")
    if not fast:
        S.deliver(f"pictures/{name}")
    info["picture"] = out
    return out


def main():
    a = kit.args()
    fast = "--fast" in a
    steps = a[a.index("--only") + 1].split(",") if "--only" in a else ["export", "picture"]
    dresses = a[a.index("--dress") + 1].split(",") if "--dress" in a else None
    p = S.stage_path(f"export_info_{NATION}.json")
    info = json.load(open(p)) if os.path.exists(p) else {}
    for step in steps:
        t0 = time.time()
        if step == "export":
            for d in dresses or ["summer"]:
                export_sov(info, d)
        elif step == "picture":
            picture(info, fast, ["summer", "winter", "snow"] if dresses else None)
        with open(p, "w") as f:
            json.dump(info, f, indent=1)
        print(f"step {step}: {time.time() - t0:.0f} s")
    ok = S.check_delivered()
    print("delivered" if ok else "SOME FILES DID NOT LAND")


if __name__ == "__main__":
    main()
