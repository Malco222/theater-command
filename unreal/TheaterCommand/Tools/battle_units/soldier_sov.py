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
    S.BUDGET["uniform"] = UNIFORM_BUDGET + {"winter": 3000, "snow": 900}.get(spec.get("dress"), 0)
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
QUILT = {"body": (math.radians(24.0), 0.012), "arm": (0.052, 0.008), "leg": (0.062, 0.009)}     # (spacing, height)
QUILT_LIFT = 0.006                       # (the cloth under the tubes that much nearer the body, for the same bulk)


def _tubes(u, spacing, height):
    """A row of round tubes across u: zero at each seam (u a whole number of spacings), `height` between. (Smooth
    through the seams: the cloth is pushed out face by face, and a sharp crease came out in steps.)"""
    return height * (0.5 - 0.5 * np.cos(2.0 * np.pi * u / spacing))


def _round_axis(P, a, b, side):
    """For points P near the limb from a to b: how far along it (0 at a, 1 at b), and the angle round it (0 ahead,
    rising outward), and how much each faces the body (1 straight at it)."""
    d = (b - a) / np.linalg.norm(b - a)
    t = ((P - a) @ d) / np.linalg.norm(b - a)
    v = (P - a) - np.outer((P - a) @ d, d)
    fwd = np.array((1.0, 0.0, 0.0)) - d * d[0]
    fwd /= np.linalg.norm(fwd)
    out = np.cross(d, fwd) * (1.0 if np.cross(d, fwd)[1] * side > 0 else -1.0)
    ang = np.arctan2(v @ out, v @ fwd)
    inner = np.array((0.0, -side, 0.0)) - d * (-side * d[1])
    inner /= np.linalg.norm(inner)
    facing = (v @ inner) / np.maximum(np.linalg.norm(v, axis=1), 1e-9)
    return t, ang, facing


def _quilting(shell, J, LM, parts, z_lo, z_hi):
    """The quilting's extra thickness at each face of `shell` (a ShellField), on `parts` ("body", "arm", "leg"), each
    face counted to the part its bones mostly are, kept between z_lo and z_hi and faded off at its edges."""
    def more(Fc):
        W = shell.Wf
        names = list(shell.names)
        col = lambda bones: np.sum([W[:, names.index(b)] for b in bones if b in names], axis=0) if any(b in names for b in bones) else np.zeros(len(Fc))
        share = {"body": col(("pelvis", "spine_01", "spine_02", "chest", "spine_03")),
                 "arm_l": col(("upperarm_l", "lowerarm_l")), "arm_r": col(("upperarm_r", "lowerarm_r")),
                 "leg_l": col(("thigh_l", "calf_l")), "leg_r": col(("thigh_r", "calf_r"))}
        keys = list(share)
        stack = np.stack([share[k] for k in keys], axis=1)
        best = np.argmax(stack, axis=1)
        # (faded where a face is shared between parts, so the rows do not end in a step where one part meets the next)
        sure = np.clip((stack.max(axis=1) / np.maximum(stack.sum(axis=1), 1e-9) - 0.6) / 0.3, 0.0, 1.0)
        z = Fc[:, 2]
        band = np.clip((z - z_lo) / 0.02, 0.0, 1.0) * np.clip((z_hi - z) / 0.02, 0.0, 1.0)
        off = np.zeros(len(Fc))
        if "body" in parts:
            m = best == keys.index("body")
            cx = J["spine_01"][0] + 0.03
            phi = np.arctan2(Fc[m, 1], Fc[m, 0] - cx)
            sp, h = QUILT["body"]
            # none across the front edge, faded off under the arms
            keep = np.clip((np.abs(phi) - 0.12) / 0.1, 0.0, 1.0) * np.clip((np.abs(np.cos(phi)) - 0.22) / 0.15, 0.0, 1.0)
            off[m] = _tubes(phi, sp, h) * keep
        for s_, side in (("l", 1.0), ("r", -1.0)):
            if "arm" in parts:
                m = best == keys.index("arm_" + s_)
                sh, el, wr = (np.array(J[b + s_]) for b in ("upperarm_", "lowerarm_", "hand_"))
                up = ((Fc[m] - sh) @ (el - sh)) / ((el - sh) @ (el - sh)) < 1.0
                t1, a1, f1 = _round_axis(Fc[m], sh, el, side)
                t2, a2, f2 = _round_axis(Fc[m], el, wr, side)
                ang, fac = np.where(up, a1, a2), np.where(up, f1, f2)
                r = np.where(up, 0.064, 0.052)
                sp, h = QUILT["arm"]
                keep = np.clip((0.62 - fac) / 0.2, 0.0, 1.0) * np.where(up, np.clip((t1 - 0.22) / 0.1, 0.0, 1.0), np.clip((0.86 - t2) / 0.06, 0.0, 1.0))
                off[m] = _tubes(ang * r, sp, h) * keep
            if "leg" in parts:
                m = best == keys.index("leg_" + s_)
                hip, knee = np.array(J["thigh_" + s_]), np.array(J["calf_" + s_])
                t, ang, fac = _round_axis(Fc[m], hip, knee, side)
                sp, h = QUILT["leg"]
                keep = np.clip((0.5 - fac) / 0.2, 0.0, 1.0)
                off[m] = _tubes(ang * 0.085, sp, h) * keep
        return off * band * sure
    return more


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
    q_top = _quilting(tights, J, LM, ("body", "arm"), LM["hem"], Z(1.45)) if quilt else (lambda P: 0.0)
    q_skirt = _quilting(skirt, J, LM, ("body",), LM["hem"] + 0.005, Z(1.45)) if quilt else (lambda P: 0.0)
    tunic.add(tights.cloth(fat, lo=(-BIG, -BIG, LM["belt"] - 0.02), default=OFF, more=q_top), 0.02)
    ARMS = {b + s_: OFF for b in ("upperarm_", "lowerarm_", "hand_", "fingers_", "thumb_") for s_ in ("l", "r")}
    LEGS = ("pelvis", "spine_01", "thigh_l", "thigh_r")
    flare = lambda P: 0.026 + pad + 0.02 * np.clip((LM["belt"] - P[:, 2]) / (LM["belt"] - LM["hem"]), 0.0, 1.0) ** 1.5
    box = dict(lo=(-BIG, -BIG, LM["hem"] - 0.02), hi=(BIG, BIG, LM["belt"] + 0.03))
    q_hips = _quilting(tights, J, LM, ("body",), LM["hem"] + 0.005, Z(1.45)) if quilt else (lambda P: 0.0)
    tunic.add(Blend(skirt.cloth(ARMS, more=lambda P: flare(P) + q_skirt(P), **box),
                    tights.cloth({b: 0.0 for b in LEGS}, more=lambda P: flare(P) + q_hips(P), default=OFF, **box), 0.05), 0.02)
    if quilt:
        print("  SOV quilting: tubes in the jacket's thickness, body and sleeves")
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
    q_legs = _quilting(tights, J, LM, ("leg",), bt + 0.03, LM["hem"] + 0.02) if quilt else (lambda P: 0.0)
    trousers.add(tights.cloth(legs, hi=(BIG, BIG, LM["belt"] + 0.04), default=OFF,
                              more=lambda P: q_legs(P) - (PAD_LEGS + 0.011) * np.clip((bt + 0.03 - P[:, 2]) / 0.05, 0.0, 1.0)), 0.02)
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
            g.tube(p - nrm * 0.001, p + nrm * 0.004, 0.0085, 0.0075, "steel", 8)
        out["details"] = (g.build("details"), None)
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
    lods = {0: S.man_objects(m), 1: S.make_lods(m, 1), 2: S.make_lods(m, 2)}
    sc.frame_set(0)
    n = {lod: sum(S.tris_of(o) for o in lods[lod]) for lod in lods}
    rec = info.setdefault(dress, {})
    rec["tris"] = n
    rec["parts"] = {o.name.split(".")[0]: S.tris_of(o) for o in lods[0]}
    rec["short"] = {k: float(v) for k, v in m["short"].items()}
    S.export_glb(S.stage_path(f"{base}.glb"), [m["arm"]] + lods[0] + socks)
    for lod in (1, 2):
        S.export_glb(S.stage_path(f"{base}_LOD{lod}.glb"), [m["arm"]] + lods[lod] + socks, anim=False)
    data = S.write_game_json(S.stage_path(f"{base}.json"), m, lods)
    data["about"] = data["about"].replace("soldier.py.", "soldier_sov.py on soldier.py.")
    if dress != "summer":
        data["about"] += f" Dress: {dress}; paint by its colours below (they differ from the summer man's)."
        data["colours"] = dict(S.LOOK[tag])
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
