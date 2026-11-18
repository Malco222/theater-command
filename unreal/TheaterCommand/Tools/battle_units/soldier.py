"""The infantry soldier of the real-time battles: one man, rigged and animated, from whom the squad of twelve is made.

Run without Blender's window (Python 3.11 with the `bpy` module, Blender 5):

    python soldier.py                     everything: build, rig, animate, export, pictures
    python soldier.py --only build,export (stages: build, export, pictures; --fast for small test pictures)

Nothing is drawn by hand and nothing is fetched. The man is sculpted from numbers as distance fields (sculpt.py):
anatomical shapes (skull, jaw, brow, nose, ears, ribcage, limbs, hands as mitts with a free thumb) blended into one
smooth body, and the clothes as layers over it (tunic with collar, four pockets and a skirt over the hips; trousers;
tall boots, or ankle boots with gaiters), joined hard so that hems, cuffs and boot tops are true edges. The field is
meshed and thinned to the triangle budget. His kit (belt and pouches, Y-straps, bread bag, water bottle, entrenching
tool, gas-mask canister or pack), the helmets (twelve kinds, rigid) and the weapons (a bolt-action rifle and a light
machine gun, our own designs) are their own meshes on the same skeleton.

The clips are posed by this script: the feet are planted and the hands put on the weapon by two-bone IK solved here,
and every frame is written as plain bone keys (no constraints are left in the file), so what Unreal reads is what
the pictures show. Clips are in place: the walking ones state their stride.

Conventions (kit.py): metres, the man faces +X, Z up, ground at 0; Blender's y is the man's LEFT, the game's JSON has
y to the RIGHT (the writer flips it). Materials are named by paint (FIGURE_PAINTS in build_tank.py, plus `eye`).
"""

import json
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402  (first: it puts Blender's own modules on the path)
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Quaternion, Vector  # noqa: E402

import kit  # noqa: E402
from sculpt import (Ellipsoid, Grid, HalfSpace, Layer, RoundBox, RoundCone, Sphere, Torus, Tube, frame_from,  # noqa: E402
                    on_surface, rot_euler, surface_nets)

OUT = "/mnt/project-files/battle-models/land/soldier/"
SCRATCH = "/home/user/art/soldier/"
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))       # unreal/TheaterCommand

# The figure's paints (build_tank.FIGURE_PAINTS), so the game colours him as it does today's figures.
FIGURE_PAINTS = {"tunic": 0, "trousers": 1, "boots": 2, "lower": 3, "straps": 4, "helmet": 5, "pack": 6, "skin": 7, "wood": 8,
                 "steel": 9, "coat": 10, "coat_dark": 11, "leather": 12, "blanket": 13, "hoof": 14}

# The two armies of the first battle, as FIGURES has them (Rifleman_Stahl, Rifleman_Round), and the rest for later.
NATION_KIT = {
    "GER": {"helmet": "stahl", "tall_boots": True, "pack": False, "roll": False, "canister": True, "chest_pouches": False},
    "POL": {"helmet": "round", "tall_boots": False, "pack": True, "roll": False, "canister": False, "chest_pouches": False},
    "UK": {"helmet": "brodie", "tall_boots": False, "pack": True, "roll": False, "canister": False, "chest_pouches": True},
    "SOV": {"helmet": "soviet", "tall_boots": True, "pack": False, "roll": True, "canister": False, "chest_pouches": False},
}

# Studio colours for the pictures only (the game paints by number from its unit table).
LOOK = {
    "GER": {"tunic": "5F665C", "trousers": "585E57", "boots": "1E1B19", "lower": "1E1B19", "straps": "24201C", "helmet": "4F5650",
            "pack": "6E6852", "skin": "C49A7E", "wood": "6B4429", "steel": "34363A", "eye": "2A2420"},
    "POL": {"tunic": "7C7450", "trousers": "746C4A", "boots": "3A2A1E", "lower": "5E4A34", "straps": "4A3624", "helmet": "5E6044",
            "pack": "7A7254", "skin": "C49A7E", "wood": "6B4429", "steel": "34363A", "eye": "2A2420"},
}


# ================================================================================================ the skeleton
def _n(v):
    v = np.asarray(v, dtype=float)
    return v / np.linalg.norm(v)


def joints():
    """Where his joints are when he stands in the rest pose (arms down and out at 45 degrees), Blender coordinates.
    1.75 m to the crown, the head 0.233 m (7.5 heads), shoulders 0.46 m across the arms."""
    J = {"root": (0.0, 0.0, 0.0), "pelvis": (0.0, 0.0, 0.95), "spine_01": (-0.005, 0.0, 1.03), "spine_02": (-0.01, 0.0, 1.14),
         "chest": (-0.012, 0.0, 1.26), "neck": (-0.022, 0.0, 1.462), "head": (-0.004, 0.0, 1.585), "head_end": (-0.004, 0.0, 1.75)}
    for s, side in (("l", 1.0), ("r", -1.0)):
        sh = np.array((-0.012, 0.178 * side, 1.418))
        el = sh + 0.29 * _n((-0.02, 0.707 * side, -0.707))
        wr = el + 0.255 * _n((0.16, 0.68 * side, -0.71))
        a = _n((0.12, 0.66 * side, -0.74))                                 # along the hand
        n = np.array((0.0, -0.74 * side, -0.66))                             # the way the palm faces (in, and down)
        n = _n(n - a * (n @ a))
        t = np.array((1.0, 0.0, 0.0)) - a * a[0] - n * n[0]                  # toward the thumb (forward)
        t = t / np.linalg.norm(t)
        kn = wr + a * 0.096
        J["clavicle_" + s] = (0.012, 0.024 * side, 1.432)
        J["upperarm_" + s] = tuple(sh)
        J["lowerarm_" + s] = tuple(el)
        J["hand_" + s] = tuple(wr)
        J["fingers_" + s] = tuple(kn + t * -0.004)
        J["fingers_end_" + s] = tuple(kn + a * 0.05 + n * 0.05)
        J["thumb_" + s] = tuple(wr + a * 0.022 + t * 0.026 + n * 0.006)
        J["thumb_end_" + s] = tuple(wr + a * 0.085 + t * 0.052 + n * 0.03)
        J["hand_axes_" + s] = (tuple(a), tuple(t), tuple(n))
        J["thigh_" + s] = (0.0, 0.09 * side, 0.915)
        J["calf_" + s] = (0.018, 0.096 * side, 0.49)
        J["foot_" + s] = (-0.004, 0.1 * side, 0.088)
        J["ball_" + s] = (0.142, 0.104 * side, 0.024)
        J["toe_" + s] = (0.215, 0.105 * side, 0.022)
    return J


# ================================================================================================ the body, sculpted
def hand_layer(J, s):
    """A hand as a mitt: palm, the four fingers together curled half shut, and a thumb of its own that can close."""
    side = 1.0 if s == "l" else -1.0
    a, t, n = (np.array(v) for v in J["hand_axes_" + s])
    w = -n                                                                     # the back of the hand
    wr = np.array(J["hand_" + s])
    L = Layer("hand_" + s, "skin")

    def at(u, v, b):
        return wr + a * u + t * v + w * b

    R_hand = np.stack([a, t, w], axis=1)
    L.add(RoundCone(at(-0.05, 0.0, 0.0), at(0.012, 0.0, 0.002), 0.026, 0.027))                    # wrist (into the cuff)
    L.add(Ellipsoid(at(0.012, 0.0, 0.0), (0.02, 0.031, 0.02), R_hand), 0.012)
    L.add(RoundBox(at(0.052, -0.002, 0.002), (0.045, 0.04, 0.0135), 0.012, R_hand), 0.014)       # palm
    L.add(Ellipsoid(at(0.036, 0.024, -0.008), (0.032, 0.02, 0.016), R_hand), 0.01)               # ball of the thumb
    # the fingers as one mitt, bent at the knuckles and again half way
    kn = at(0.094, -0.003, 0.0)
    d1 = _n(a * math.cos(math.radians(32)) + n * math.sin(math.radians(32)))
    p1 = kn + d1 * 0.044
    d2 = _n(a * math.cos(math.radians(95)) + n * math.sin(math.radians(95)))
    p2 = p1 + d2 * 0.036
    for p_a, p_b, d, half_w in ((kn, p1, d1, 0.039), (p1, p2, d2, 0.036)):
        c = (p_a + p_b) * 0.5
        R = np.stack([d, t, np.cross(d, t)], axis=1)
        L.add(RoundBox(c, ((np.linalg.norm(p_b - p_a)) * 0.5 + 0.008, half_w, 0.0115), 0.0105, R), 0.008)
    L.add(Ellipsoid(kn + w * 0.003, (0.012, 0.042, 0.013), R_hand), 0.008)                      # knuckles
    # the thumb, from the heel of the hand toward the fingers' tips
    t0, t1, t2 = at(0.022, 0.03, -0.004), at(0.058, 0.05, -0.016), at(0.083, 0.052, -0.03)
    L.add(RoundCone(t0, t1, 0.0145, 0.0115), 0.01)
    L.add(RoundCone(t1, t2, 0.0115, 0.0095), 0.004)
    return L


def head_layer(J):
    """The head and neck: skull, jaw, chin, cheekbones, brow, eye hollows, nose, mouth and ears."""
    L = Layer("head", "skin")
    hx = J["head"][0] + 0.004
    L.add(RoundCone((-0.03, 0.0, 1.39), (hx - 0.004, 0.0, 1.585), 0.057, 0.05))                         # neck
    for side in (1.0, -1.0):                                                                        # the neck's cords
        L.add(RoundCone((hx - 0.03, 0.05 * side, 1.6), (hx + 0.03, 0.017 * side, 1.455), 0.019, 0.016), 0.02)
    L.add(Ellipsoid((hx - 0.01, 0.0, 1.655), (0.099, 0.08, 0.095)), 0.0)                              # cranium
    L.add(Ellipsoid((hx - 0.046, 0.0, 1.637), (0.064, 0.069, 0.072)), 0.02)                            # back of the skull
    L.add(Ellipsoid((hx + 0.034, 0.0, 1.598), (0.064, 0.066, 0.074)), 0.025)                           # the face
    for side in (1.0, -1.0):
        L.add(RoundCone((hx - 0.018, 0.058 * side, 1.597), (hx + 0.068, 0.026 * side, 1.537), 0.021, 0.017), 0.015)  # jaw
        L.add(Ellipsoid((hx + 0.07, 0.052 * side, 1.619), (0.026, 0.022, 0.0165)), 0.01)                  # cheekbone
        L.sub(Ellipsoid((hx + 0.086, 0.05 * side, 1.586), (0.013, 0.01, 0.013)), 0.016)                 # the hollow under it
        L.add(Ellipsoid((hx - 0.008, 0.079 * side, 1.632), (0.029, 0.0095, 0.034), rot_euler(0, -12, 0)), 0.006)  # ear
        L.add(Ellipsoid((hx - 0.005, 0.088 * side, 1.635), (0.019, 0.004, 0.022), rot_euler(0, -12, 0)), 0.0)   # its rim
        L.sub(Ellipsoid((hx - 0.002, 0.092 * side, 1.631), (0.009, 0.005, 0.012), rot_euler(0, -12, 0)), 0.002)  # its bowl
    L.add(Sphere((hx + 0.08, 0.0, 1.538), 0.021), 0.018)                                                 # chin
    L.add(RoundCone((hx + 0.071, 0.05, 1.661), (hx + 0.082, 0.0, 1.666), 0.0125, 0.0115), 0.01)          # brow ridge
    L.add(RoundCone((hx + 0.071, -0.05, 1.661), (hx + 0.082, 0.0, 1.666), 0.0125, 0.0115), 0.01)
    for side in (1.0, -1.0):
        L.sub(Sphere((hx + 0.1, 0.034 * side, 1.641), 0.019), 0.012)                                     # eye socket
        L.add(Sphere((hx + 0.081, 0.034 * side, 1.64), 0.0145), 0.004)                                   # eyeball and lids
        L.add(RoundCone((hx + 0.087, 0.024 * side, 1.65), (hx + 0.086, 0.044 * side, 1.649), 0.0052, 0.0048), 0.005)  # upper lid
    L.add(RoundCone((hx + 0.089, 0.0, 1.652), (hx + 0.111, 0.0, 1.607), 0.0085, 0.0112), 0.012)          # nose
    for side in (1.0, -1.0):
        L.add(Sphere((hx + 0.101, 0.013 * side, 1.601), 0.0105), 0.008)                                  # nostril wings
    L.add(Ellipsoid((hx + 0.094, 0.0, 1.568), (0.013, 0.026, 0.013)), 0.008)                             # lips
    L.sub(RoundCone((hx + 0.109, -0.021, 1.5665), (hx + 0.109, 0.021, 1.5665), 0.0028, 0.0028), 0.002)   # mouth
    L.cut(HalfSpace((0.0, 0.0, 1.395), (0.0, 0.0, -1.0)))                                              # (the neck ends inside the collar)
    return L


def eye_layer(J):
    """The dark of each eye (iris and pupil as one), so the face reads from the game's camera."""
    L = Layer("eyes", "eye")
    hx = J["head"][0] + 0.004
    for side in (1.0, -1.0):
        L.add(Ellipsoid((hx + 0.0905, 0.034 * side, 1.6395), (0.0065, 0.0072, 0.0068)))
    return L


def body_layers(J, kitspec):
    """Tunic, trousers and boots (or ankle boots and gaiters): the clothed body below the collar and above the cuffs."""
    tunic, trousers, boots = Layer("tunic", "tunic"), Layer("trousers", "trousers"), Layer("boots", "boots")
    collar, cuffs, soles = Layer("collar", "tunic"), Layer("cuffs", "tunic"), Layer("soles", "boots")
    layers = [tunic, trousers, boots, collar, cuffs, soles]          # (each its own layer: hems and seams are real edges)
    # ---- the tunic: chest, belly, shoulders, back, sleeves, collar, skirt and pockets
    tunic.add(Ellipsoid((-0.01, 0.0, 1.272), (0.121, 0.153, 0.17)))                       # ribcage
    tunic.add(Ellipsoid((0.0, 0.0, 1.352), (0.104, 0.172, 0.092)), 0.04)                  # upper chest, to the shoulders
    for side in (1.0, -1.0):
        tunic.add(Ellipsoid((0.045, 0.066 * side, 1.318), (0.064, 0.078, 0.068)), 0.03)   # breast
        tunic.add(Ellipsoid((-0.066, 0.07 * side, 1.335), (0.06, 0.084, 0.11)), 0.03)     # shoulder blade
        tunic.add(RoundCone((-0.022, 0.035 * side, 1.452), (-0.016, 0.165 * side, 1.418), 0.05, 0.052), 0.035)  # trapezius
    tunic.add(Ellipsoid((0.0, 0.0, 1.11), (0.104, 0.142, 0.13)), 0.05)                    # belly
    tunic.add(Tube((-0.004, 0.0, 0.0), 0.96, 1.08, (0.104, 0.147), (0.105, 0.146), 0.02), 0.03)   # waist, under the belt
    tunic.add(Tube((-0.012, 0.0, 0.0), 0.765, 1.0, (0.142, 0.198), (0.11, 0.152), 0.013), 0.012)  # the skirt over the hips
    for s, side in (("l", 1.0), ("r", -1.0)):
        sh, el, wr = (np.array(J[k + s]) for k in ("upperarm_", "lowerarm_", "hand_"))
        tunic.add(Ellipsoid(sh + np.array((0.004, 0.008 * side, -0.018)), (0.058, 0.052, 0.07),
                            frame_from(el - sh, (1.0, 0.0, 0.0)) @ rot_euler(0, 0, 0)), 0.03)       # deltoid
        tunic.add(RoundCone(sh, el, 0.055, 0.045), 0.03)                                             # upper sleeve
        tunic.add(Sphere(el, 0.044), 0.02)                                                           # elbow
        cuff0 = wr - _n(wr - el) * 0.07
        tunic.add(RoundCone(el, cuff0, 0.045, 0.041), 0.02)                                          # forearm
        Rc = frame_from(wr - el, (1.0, 0.0, 0.0))
        Rc = np.stack([Rc[:, 1], Rc[:, 2], Rc[:, 0]], axis=1)                                         # (its z along the arm)
        cuffs.add(Tube(wr - _n(wr - el) * 0.035, -0.035, 0.028, (0.0445, 0.0435), (0.045, 0.044), 0.006, Rc))   # turned-back cuff
        # folds: the sleeve gathers in rings at the elbow, the cloth bunched where the arm bends
        ax_ = _n(wr - el)
        for k, (off, tilt, ridge) in enumerate(((-0.06, 10, True), (-0.035, -14, False), (-0.012, 8, True), (0.014, -10, False),
                                                 (0.04, 12, True), (0.075, -6, True))):
            Rf = Rc @ rot_euler(tilt, tilt * 0.4, 0)
            c = el + ax_ * off
            rr = 0.0455 - 0.002 * (off > 0.02)
            if ridge:
                tunic.add(Torus(c, rr - 0.005, 0.0115, Rf), 0.012)
        # shoulder strap on the shoulder's top
        mid = np.array((-0.014, 0.115 * side, 1.462))
        Rs = frame_from((0.0, side, -0.24), (0.0, 0.0, 1.0))
        tunic.add(RoundBox(mid, (0.052, 0.022, 0.006), 0.005, Rs), 0.004)
    # the collar: a band round the foot of the neck, leaning forward
    collar.add(Torus((-0.014, 0.0, 1.452), 0.066, 0.019, rot_euler(0, -14, 0), major_y=0.061))
    collar.add(Tube((-0.014, 0.0, 1.43), -0.03, 0.022, (0.074, 0.07), (0.072, 0.066), 0.012, rot_euler(0, -14, 0)), 0.01)
    collar.sub(RoundBox((0.09, 0.0, 1.47), (0.03, 0.009, 0.05), 0.004, rot_euler(0, -14, 0)), 0.002)   # (open a little at the front)
    # the cloth bunched above and below the belt: folds running up from it, more at the back
    for k, ang in enumerate(np.radians([70, 108, 140, 166, 194, 220, 252, 290])):
        lean = 0.012 * math.sin(3.1 * k)
        a0 = np.array((math.cos(ang) * 0.3, math.sin(ang) * 0.3, 1.026))
        a1 = np.array((math.cos(ang + lean * 4) * 0.3, math.sin(ang + lean * 4) * 0.3, 1.075 + 0.012 * math.cos(2.3 * k)))
        (p0, p1), (n0, n1) = project(tunic, [a0, a1])
        tunic.add(RoundCone(p0 - n0 * 0.007, p1 - n1 * 0.0105, 0.0135, 0.011), 0.012)
        b0 = np.array((math.cos(ang + 0.1) * 0.3, math.sin(ang + 0.1) * 0.3, 0.972))
        b1 = np.array((math.cos(ang + 0.13) * 0.3, math.sin(ang + 0.13) * 0.3, 0.925))
        (q0, q1), (m0, m1) = project(tunic, [b0, b1])
        tunic.add(RoundCone(q0 - m0 * 0.009, q1 - m1 * 0.012, 0.0125, 0.011), 0.012)
    # and where the back of the tunic wrinkles across the shoulder blades
    for z, w in ((1.36, 0.06), (1.3, 0.075)):
        (p0, p1), (n0, n1) = project(tunic, [(-0.3, w, z + 0.01), (-0.3, -w, z - 0.008)])
        tunic.add(RoundCone(p0 - n0 * 0.007, p1 - n1 * 0.007, 0.0125, 0.0125), 0.014)
    # the front's button strip and the four pockets (two on the breast, two on the skirt), each a patch with a flap,
    # laid on the cloth where it is
    p, nrm = on_surface(tunic, (0.3, 0.0, 1.2), (-1.0, 0.0, 0.0))
    tunic.add(RoundBox(p - nrm * 0.004, (0.006, 0.014, 0.2), 0.005, frame_from(nrm, (0.0, 0.0, 1.0))), 0.008)
    for side in (1.0, -1.0):
        for aim, half in (((0.3, 0.078 * side, 1.305), (0.006, 0.05, 0.058)), ((0.3, 0.105 * side, 0.875), (0.006, 0.06, 0.07))):
            start = np.array(aim)
            p, nrm = on_surface(tunic, start, (-1.0, -0.25 * side, 0.0))
            R = frame_from(nrm, (0.0, 0.0, 1.0))
            tunic.add(RoundBox(p, half, 0.005, R), 0.004)
            top = p + R @ np.array((0.005, 0.0, half[2] - 0.013))
            tunic.add(RoundBox(top, (0.0075, half[1] + 0.004, 0.018), 0.006, R), 0.003)
    # ---- the trousers: seat, thighs, knees, calves
    trousers.add(Ellipsoid((-0.015, 0.0, 0.93), (0.115, 0.165, 0.13)))
    for s, side in (("l", 1.0), ("r", -1.0)):
        hip, knee, ank = (np.array(J[k + s]) for k in ("thigh_", "calf_", "foot_"))
        trousers.add(Ellipsoid((-0.066, 0.066 * side, 0.865), (0.076, 0.08, 0.1)), 0.03)           # buttock
        trousers.add(RoundCone(hip, knee, 0.088, 0.056), 0.03)                                     # thigh
        trousers.add(Ellipsoid((0.012, 0.104 * side, 0.74), (0.072, 0.07, 0.16)), 0.04)            # its muscle
        trousers.add(Sphere(knee + np.array((0.012, 0.0, 0.0)), 0.052), 0.02)                      # knee
        # creases behind and over the knee where the cloth bends
        Rk = frame_from(knee - hip, (1.0, 0.0, 0.0))
        Rk = np.stack([Rk[:, 1], Rk[:, 2], Rk[:, 0]], axis=1)
        for off, tilt, ridge in ((0.07, 12, True), (0.045, -9, False), (0.02, 14, True), (-0.03, -10, True), (-0.055, 8, False)):
            c = knee + _n(knee - hip) * off
            Rf = Rk @ rot_euler(tilt, tilt * 0.5, 0)
            if ridge:
                trousers.add(Torus(c, 0.047, 0.012, Rf), 0.013)
        trousers.add(RoundCone(knee, ank + np.array((0.0, 0.0, 0.03)), 0.052, 0.038), 0.02)       # shin
        trousers.add(Ellipsoid((-0.022, 0.097 * side, 0.36), (0.05, 0.048, 0.085)), 0.03)         # calf
        if kitspec["tall_boots"]:
            trousers.add(Ellipsoid(knee + np.array((-0.002, 0.0, -0.045)), (0.062, 0.06, 0.06)), 0.03)   # bloused into the boot
            for z, tilt in ((0.445, 8), (0.468, -10)):
                trousers.add(Torus((0.008, 0.097 * side, z), 0.053, 0.011, rot_euler(tilt, 0, 0)), 0.012)
    # ---- the boots: tall marching boots, or ankle boots under gaiters
    lower = None
    if not kitspec["tall_boots"]:
        lower = Layer("gaiters", "lower")
        straps = Layer("gaiter_straps", "straps")
        layers += [lower, straps]
    for s, side in (("l", 1.0), ("r", -1.0)):
        ank = np.array(J["foot_" + s])
        y = ank[1]
        R_out = rot_euler(0, 0, 4.0 * side)
        soles.add(RoundBox((0.074, y, 0.0105), (0.148, 0.054, 0.0105), 0.005, R_out))                  # sole, proud of the upper
        soles.add(RoundBox((-0.031, y, 0.02), (0.043, 0.047, 0.02), 0.004), 0.004)                     # heel
        boots.add(RoundBox((-0.03, y, 0.045), (0.04, 0.042, 0.03), 0.014), 0.012)                      # the heel's counter
        boots.add(Ellipsoid((0.085, y + 0.002 * side, 0.046), (0.112, 0.047, 0.04), R_out), 0.025)       # upper over the foot
        boots.add(Ellipsoid((0.168, y + 0.006 * side, 0.036), (0.05, 0.046, 0.032), R_out), 0.02)        # toe
        boots.add(RoundCone(ank + np.array((0.0, 0.0, 0.0)), (0.095, y, 0.058), 0.046, 0.04), 0.02)      # instep
        if kitspec["tall_boots"]:
            boots.add(RoundCone(ank + np.array((0.0, 0.0, 0.02)), (0.012, y * 0.97, 0.45), 0.046, 0.059), 0.02)   # shaft
        else:
            boots.add(RoundCone(ank + np.array((0.0, 0.0, 0.0)), (0.0, y, 0.13), 0.047, 0.046), 0.02)
            lower.add(RoundCone((-0.004, y, 0.105), (0.008, y * 0.98, 0.32), 0.048, 0.053))              # the gaiter
            for z in (0.17, 0.27):
                straps.add(Torus((0.0, y, z), 0.052 + 0.002 * (z - 0.17) / 0.1, 0.0055))
    boots.cut(HalfSpace((0.0, 0.0, 0.43 if kitspec["tall_boots"] else 0.135), (0.0, 0.0, 1.0)), 0.006)
    if lower is not None:
        lower.cut(HalfSpace((0.0, 0.0, 0.31), (0.0, 0.0, 1.0)), 0.005)
        lower.cut(HalfSpace((0.0, 0.0, 0.1), (0.0, 0.0, -1.0)), 0.0)
        for s, side in (("l", 1.0), ("r", -1.0)):
            trousers.add(Ellipsoid((0.003, 0.098 * side, 0.335), (0.06, 0.059, 0.04)), 0.02)           # bloused over the gaiter
    return layers


def mesh_from_layers(name, layers, h, target_tris, smooth_steps=1, sym=False):
    """Sample the layers on a grid, mesh the surface, thin it to about `target_tris`, and paint each face with
    the layer it lies on. Returns the Blender object."""
    lo = np.min([L.bounds()[0] for L in layers], axis=0)
    hi = np.max([L.bounds()[1] for L in layers], axis=0)
    g = Grid(lo, hi, h)
    F = None
    for L in layers:
        f = g.layer(L)
        F = f if F is None else np.minimum(F, f)
    V, Q = surface_nets(F, g.lo, g.h)
    del F

    def field(P):
        return np.min([L.at_points(P) for L in layers], axis=0)
    # relax the net (each point toward its neighbours, then back onto the surface) so it is even before it is thinned
    E = np.concatenate([Q[:, [0, 1]], Q[:, [1, 2]], Q[:, [2, 3]], Q[:, [3, 0]]])
    cnt = np.bincount(E.ravel(), minlength=len(V)).astype(float)[:, None]
    # pull each point onto the surface (one step along the field's slope): crisper hems, no grid steps
    for it in range(smooth_steps + 2):
        if it < 2:
            acc = np.zeros_like(V)
            np.add.at(acc, E[:, 0], V[E[:, 1]])
            np.add.at(acc, E[:, 1], V[E[:, 0]])
            V = V + 0.6 * (acc / np.maximum(cnt, 1.0) - V)
        e = 1e-4
        d = field(V)
        gr = np.stack([(field(V + np.eye(3)[i] * e) - field(V - np.eye(3)[i] * e)) / (2 * e) for i in range(3)], axis=1)
        gl = np.maximum((gr ** 2).sum(axis=1), 1e-6)
        step = (d / gl)[:, None] * gr
        lim = 0.5 * h
        sl = np.linalg.norm(step, axis=1)
        step *= np.minimum(1.0, lim / np.maximum(sl, 1e-12))[:, None]
        V = V - step
    me = bpy.data.meshes.new(name)
    me.vertices.add(len(V))
    me.vertices.foreach_set("co", V.astype(np.float32).ravel())
    me.loops.add(len(Q) * 4)
    me.loops.foreach_set("vertex_index", Q.astype(np.int32).ravel())
    me.polygons.add(len(Q))
    me.polygons.foreach_set("loop_start", (np.arange(len(Q)) * 4).astype(np.int32))
    me.update(calc_edges=True)
    me.validate()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    tris = len(Q) * 2
    if target_tris and tris > target_tris:
        mod = ob.modifiers.new("thin", "DECIMATE")
        mod.ratio = target_tris / tris
        mod.use_symmetry = sym
        mod.symmetry_axis = "Y"
        bpy.context.view_layer.objects.active = ob
        with bpy.context.temp_override(object=ob, active_object=ob):
            bpy.ops.object.modifier_apply(modifier=mod.name)
        # the thinning can fold a triangle over at a sharp place: relax the thinned net and lay it back on the surface
        me = ob.data
        V = np.zeros(len(me.vertices) * 3)
        me.vertices.foreach_get("co", V)
        V = V.reshape(-1, 3)
        Ed = np.zeros(len(me.edges) * 2, np.int64)
        me.edges.foreach_get("vertices", Ed)
        Ed = Ed.reshape(-1, 2)
        cnt = np.bincount(Ed.ravel(), minlength=len(V)).astype(float)[:, None]
        for it in range(3):
            acc = np.zeros_like(V)
            np.add.at(acc, Ed[:, 0], V[Ed[:, 1]])
            np.add.at(acc, Ed[:, 1], V[Ed[:, 0]])
            V = V + 0.35 * (acc / np.maximum(cnt, 1.0) - V)
            for _ in range(2):
                e = 1e-4
                d = field(V)
                gr = np.stack([(field(V + np.eye(3)[i] * e) - field(V - np.eye(3)[i] * e)) / (2 * e) for i in range(3)], axis=1)
                V = V - (d / np.maximum((gr ** 2).sum(axis=1), 1e-6))[:, None] * gr
        me.vertices.foreach_set("co", V.astype(np.float32).ravel())
        me.update()
    # paint each face from the layer nearest its middle
    me = ob.data
    cs = np.zeros(len(me.polygons) * 3, np.float32)
    me.polygons.foreach_get("center", cs)
    cs = cs.reshape(-1, 3).astype(float)
    D = np.stack([L.at_points(cs) for L in layers], axis=1)
    which = D.argmin(axis=1)
    paints = []
    for L in layers:
        if L.paint not in paints:
            paints.append(L.paint)
    for p in paints:
        me.materials.append(paint_material(p))
    idx = np.array([paints.index(layers[i].paint) for i in which], np.int32)
    me.polygons.foreach_set("material_index", idx)
    me.polygons.foreach_set("use_smooth", np.ones(len(me.polygons), bool))
    me.update()
    return ob


LOOK_NOW = dict(LOOK["GER"])


def paint_material(paint):
    """The material for a paint, named by the paint (the game colours it); the studio colour is for the pictures."""
    rough = {"steel": 0.45, "boots": 0.42, "straps": 0.55, "helmet": 0.6, "skin": 0.55, "wood": 0.5, "eye": 0.3}.get(paint, 0.85)
    metal = 0.6 if paint == "steel" else 0.0
    noise = {"tunic": 0.18, "trousers": 0.18, "pack": 0.2, "lower": 0.15, "helmet": 0.12, "wood": 0.25, "skin": 0.06}.get(paint, 0.05)
    sheen = 0.4 if paint in ("tunic", "trousers", "pack", "lower") else 0.0
    m = kit.material(paint, kit.hex_rgb(LOOK_NOW[paint]), rough=rough, metal=metal, sheen=sheen, noise=noise,
                     noise_scale=90.0, bump=0.15 if paint in ("tunic", "trousers", "pack") else 0.0)
    return studio_wear(m, paint)


DUST = "B8AA8C"


def studio_wear(m, paint):
    """The pictures' wear on a paint's material (studio only; the exports keep the plain colour): the mottling fixed to
    the cloth (read from the rest-pose position the man's meshes carry, so it does not swim as he moves), darker in
    the folds and creases (ambient occlusion), and dust kicked up on the boots, the trouser bottoms and the knees
    (the `dust` amount each point of the body carries)."""
    nt = m.node_tree
    if nt.nodes.get("wear_done"):
        return m
    bsdf = nt.nodes["Principled BSDF"]
    rest = nt.nodes.new("ShaderNodeAttribute")
    rest.attribute_name = "rest"
    for nz in [n for n in nt.nodes if n.type == "TEX_NOISE"]:
        nt.links.new(rest.outputs["Vector"], nz.inputs["Vector"])
    base_in = bsdf.inputs["Base Color"]
    src = base_in.links[0].from_socket if base_in.links else None
    col = nt.nodes.new("ShaderNodeRGB")
    col.outputs[0].default_value = base_in.default_value[:]
    src = src or col.outputs[0]
    # dust: in splotches, on what the body says is dusty
    dust = nt.nodes.new("ShaderNodeAttribute")
    dust.attribute_name = "dust"
    blot = nt.nodes.new("ShaderNodeTexNoise")
    blot.inputs["Scale"].default_value = 14.0
    blot.inputs["Detail"].default_value = 4.0
    nt.links.new(rest.outputs["Vector"], blot.inputs["Vector"])
    shape = nt.nodes.new("ShaderNodeMapRange")
    shape.inputs["From Min"].default_value = 0.35
    shape.inputs["From Max"].default_value = 0.65
    nt.links.new(blot.outputs["Fac"], shape.inputs["Value"])
    amt = nt.nodes.new("ShaderNodeMath")
    amt.operation = "MULTIPLY"
    nt.links.new(dust.outputs["Fac"], amt.inputs[0])
    nt.links.new(shape.outputs["Result"], amt.inputs[1])
    amt2 = nt.nodes.new("ShaderNodeMath")
    amt2.operation = "MULTIPLY"
    amt2.use_clamp = True
    amt2.inputs[1].default_value = 0.8
    nt.links.new(amt.outputs["Value"], amt2.inputs[0])
    mixd = nt.nodes.new("ShaderNodeMix")
    mixd.data_type = "RGBA"
    nt.links.new(amt2.outputs["Value"], mixd.inputs["Factor"])
    nt.links.new(src, mixd.inputs["A"])
    mixd.inputs["B"].default_value = (*kit.hex_rgb(DUST), 1.0)
    # the folds darker
    ao = nt.nodes.new("ShaderNodeAmbientOcclusion")
    ao.samples = 6
    ao.only_local = True
    ao.inputs["Distance"].default_value = 0.035
    dark = nt.nodes.new("ShaderNodeMapRange")
    dark.inputs["To Min"].default_value = 0.4
    dark.inputs["To Max"].default_value = 1.0
    nt.links.new(ao.outputs["AO"], dark.inputs["Value"])
    mul = nt.nodes.new("ShaderNodeMix")
    mul.data_type = "RGBA"
    mul.blend_type = "MULTIPLY"
    mul.inputs["Factor"].default_value = 1.0
    nt.links.new(mixd.outputs["Result"], mul.inputs["A"])
    shade = nt.nodes.new("ShaderNodeCombineColor")
    for k in range(3):
        nt.links.new(dark.outputs["Result"], shade.inputs[k])
    nt.links.new(shade.outputs["Color"], mul.inputs["B"])
    nt.links.new(mul.outputs["Result"], base_in)
    tag = nt.nodes.new("NodeFrame")
    tag.name = "wear_done"
    return m


def mark_rest(ob, dusty=False):
    """Give a man's mesh its rest-pose position (and, for the body, how dusty each point is) for the studio wear."""
    me = ob.data
    n = len(me.vertices)
    co = np.zeros(n * 3, np.float32)
    me.vertices.foreach_get("co", co)
    if "rest" not in me.attributes:
        me.attributes.new("rest", "FLOAT_VECTOR", "POINT")
    me.attributes["rest"].data.foreach_set("vector", co)
    if dusty:
        P = co.reshape(-1, 3)
        z, x = P[:, 2], P[:, 0]
        d = np.clip((0.62 - z) / 0.5, 0.0, 1.0) ** 1.4                                        # rising from the ground
        d += 0.75 * np.exp(-((z - 0.5) / 0.055) ** 2) * (x > 0.0)                             # the knees' fronts
        d += 0.35 * np.exp(-((z - 0.87) / 0.08) ** 2) * (x > 0.04)                            # the thighs (kneeling, lying)
        d = np.clip(d, 0.0, 1.0).astype(np.float32)
        if "dust" not in me.attributes:
            me.attributes.new("dust", "FLOAT", "POINT")
        me.attributes["dust"].data.foreach_set("value", d)


# ================================================================================================ plain geometry
class Geo:
    """Gathers hard pieces (tubes, boxes, straps, shells) with their paints, and makes one object of them."""

    def __init__(self):
        self.V, self.F, self.P = [], [], []

    def _add(self, verts, faces, paint):
        base = len(self.V)
        self.V += [tuple(map(float, v)) for v in verts]
        self.F += [tuple(base + i for i in f) for f in faces]
        self.P += [paint] * len(faces)

    def tube(self, a, b, r1, r2, paint, sides=8, caps=True, up=(0.0, 0.0, 1.0), squash=1.0):
        a, b = np.asarray(a, float), np.asarray(b, float)
        R = frame_from(b - a, up)
        ys, zs = R[:, 1], R[:, 2]
        verts = []
        for c, r in ((a, r1), (b, r2)):
            for i in range(sides):
                t = 2 * math.pi * (i + 0.5) / sides
                verts.append(c + (ys * math.cos(t) + zs * math.sin(t) * squash) * r)
        faces = [(i, (i + 1) % sides, sides + (i + 1) % sides, sides + i) for i in range(sides)]
        if caps:
            faces.append(tuple(range(sides - 1, -1, -1)))
            faces.append(tuple(range(sides, 2 * sides)))
        self._add(verts, faces, paint)

    def box(self, c, half, paint, R=None, taper=1.0):
        c = np.asarray(c, float)
        R = np.eye(3) if R is None else np.asarray(R)
        verts = []
        for x in (-1, 1):
            for y in (-1, 1):
                for z in (-1, 1):
                    k = taper if z > 0 else 1.0
                    verts.append(c + R @ np.array((x * half[0] * k, y * half[1] * k, z * half[2])))
        faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
        self._add(verts, faces, paint)

    def band(self, pts, nrm, width, thick, paint, closed=False, up_way=None):
        """A strap: a flat band along pts (its outer face), lying on a surface whose outward normals are nrm."""
        pts, nrm = np.asarray(pts, float), np.asarray(nrm, float)
        N = len(pts)
        verts = []
        for i in range(N):
            if closed:
                T = pts[(i + 1) % N] - pts[i - 1]
            else:
                T = pts[min(i + 1, N - 1)] - pts[max(i - 1, 0)]
            T /= np.linalg.norm(T)
            n = nrm[i] - T * (nrm[i] @ T)
            n /= np.linalg.norm(n)
            B = np.cross(T, n)
            c = pts[i]
            verts += [c + B * width * 0.5, c + B * width * 0.5 - n * thick, c - B * width * 0.5 - n * thick, c - B * width * 0.5]
        faces = []
        for i in range(N if closed else N - 1):
            j = (i + 1) % N
            for k in range(4):
                faces.append((4 * i + k, 4 * i + (k + 1) % 4, 4 * j + (k + 1) % 4, 4 * j + k))
        if not closed:
            faces.append((3, 2, 1, 0))
            faces.append((4 * (N - 1), 4 * (N - 1) + 1, 4 * (N - 1) + 2, 4 * (N - 1) + 3))
        self._add(verts, faces, paint)

    def shell(self, rings, paint, close_top=True):
        """A lathe-like shell from rings of points (each ring a list of equal length, going round); the first ring
        may be one point (a crown). Consecutive rings are joined; the last ring joins the first if it is a loop."""
        verts, faces = [], []
        starts = []
        for ring in rings:
            starts.append(len(verts))
            verts += list(ring)
        for r in range(len(rings) - 1):
            a, b = rings[r], rings[r + 1]
            sa, sb = starts[r], starts[r + 1]
            if len(a) == 1:
                n = len(b)
                faces += [(sa, sb + (i + 1) % n, sb + i) for i in range(n)]
            elif len(b) == 1:
                n = len(a)
                faces += [(sa + i, sa + (i + 1) % n, sb) for i in range(n)]
            else:
                n = len(a)
                faces += [(sa + i, sa + (i + 1) % n, sb + (i + 1) % n, sb + i) for i in range(n)]
        self._add(verts, faces, paint)

    def absorb(self, ob):
        """Take in a mesh object (a sculpted piece), keeping its paints, and remove it."""
        me = ob.data
        M = np.array(ob.matrix_world)
        V = [tuple((M @ np.array((*v.co, 1.0)))[:3]) for v in me.vertices]
        names = [m.name.split(".")[0] for m in me.materials]
        base = len(self.V)
        self.V += V
        for p in me.polygons:
            self.F.append(tuple(base + i for i in p.vertices))
            self.P.append(names[p.material_index] if names else "steel")
        bpy.data.objects.remove(ob)
        bpy.data.meshes.remove(me)

    def build(self, name, sharp=40.0, recalc=True):
        me = bpy.data.meshes.new(name)
        me.from_pydata(self.V, [], self.F)
        me.validate()
        paints = []
        for p in self.P:
            if p not in paints:
                paints.append(p)
        for p in paints:
            me.materials.append(paint_material(p))
        me.polygons.foreach_set("material_index", np.array([paints.index(p) for p in self.P], np.int32))
        ob = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(ob)
        bm = bmesh.new()
        bm.from_mesh(me)
        bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
        if recalc:
            bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        for f in bm.faces:
            f.smooth = True
        lim = math.radians(sharp)
        for e in bm.edges:
            if len(e.link_faces) == 2 and e.calc_face_angle(0.0) > lim:
                e.smooth = False
        bm.to_mesh(me)
        bm.free()
        me.update()
        return ob


def smooth_shade(ob, sharp=40.0):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    for f in bm.faces:
        f.smooth = True
    lim = math.radians(sharp)
    for e in bm.edges:
        e.smooth = not (len(e.link_faces) == 2 and e.calc_face_angle(0.0) > lim)
    bm.to_mesh(ob.data)
    bm.free()


def project(L, P, iters=6):
    """Points pulled onto a layer's surface (along its field's slope). Returns (points, outward normals)."""
    P = np.array(P, float)
    e = 1e-4
    for _ in range(iters):
        d = L.at_points(P)
        g = np.stack([(L.at_points(P + np.eye(3)[i] * e) - L.at_points(P - np.eye(3)[i] * e)) / (2 * e) for i in range(3)], axis=1)
        P = P - (d / np.maximum((g ** 2).sum(axis=1), 1e-9))[:, None] * g
    g = np.stack([(L.at_points(P + np.eye(3)[i] * e) - L.at_points(P - np.eye(3)[i] * e)) / (2 * e) for i in range(3)], axis=1)
    return P, g / np.linalg.norm(g, axis=1)[:, None]


def spline(ctrl, n):
    """n points along a Catmull-Rom curve through the control points."""
    C = np.asarray(ctrl, float)
    C = np.vstack([C[0] * 2 - C[1], C, C[-1] * 2 - C[-2]])
    out = []
    segs = len(ctrl) - 1
    for k in range(n):
        u = k / (n - 1) * segs
        i = min(int(u), segs - 1)
        t = u - i
        p0, p1, p2, p3 = C[i], C[i + 1], C[i + 2], C[i + 3]
        out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3))
    return np.array(out)


# ================================================================================================ the kit
def build_kit(J, spec, body_ls):
    """Belt and buckle, ammunition pouches, Y-straps, bread bag, water bottle, entrenching tool, and by nation the
    gas-mask canister or the pack with its bedroll (or a blanket roll across the body). Returns
    {name: (object, bone or None)}: a bone means the piece is rigid on it; None means it takes the body's weights."""
    tunic = body_ls[0]
    out = {}
    # ---- the belt round the waist, laid on the tunic, and its buckle
    g = Geo()
    ang = np.linspace(0, 2 * math.pi, 28, endpoint=False)
    zb = 1.0
    start = np.stack([np.cos(ang) * 0.3, np.sin(ang) * 0.3, np.full_like(ang, zb)], axis=1)
    P, N = project(tunic, start)
    P[:, 2] = zb
    P = P + N * 0.005
    for lift in (0.0,):
        g.band(P + np.array((0, 0, lift)), N, 0.046, 0.006, "straps", closed=True)
    front = P[0] + N[0] * 0.003
    g.box(front, (0.006, 0.03, 0.026), "steel", frame_from(N[0]))
    g.box(front + N[0] * 0.0065, (0.0018, 0.021, 0.017), "steel", frame_from(N[0]))                   # the buckle's raised face
    out["belt"] = (g.build("belt"), None)
    # ---- the small things on the tunic: buttons down the front and on the pocket flaps, the collar tabs
    g = Geo()
    spots = [((0.3, 0.0, z), (-1.0, 0.0, 0.0)) for z in (1.405, 1.325, 1.245, 1.165, 1.085)]
    for side in (1.0, -1.0):
        spots += [((0.3, 0.078 * side, 1.336), (-1.0, -0.25 * side, 0.0)), ((0.3, 0.105 * side, 0.918), (-1.0, -0.25 * side, 0.0))]
    for start, way in spots:
        p, nrm = on_surface(tunic, start, way)
        g.tube(p - nrm * 0.001, p + nrm * 0.0035, 0.0085, 0.0072, "steel", 8)
    collar = body_ls[3]
    for side in (1.0, -1.0):
        p, nrm = on_surface(collar, (0.25, 0.036 * side, 1.452), (-1.0, -0.3 * side, 0.0))
        g.box(p + nrm * 0.0005, (0.0022, 0.02, 0.0105), "lower", frame_from(nrm, (0.0, 0.0, 1.0)))
    out["details"] = (g.build("details"), None)
    # ---- the ammunition pouches: three cells either side of the buckle, on the belt
    L = Layer("pouches", "straps")
    for side in (1.0, -1.0):
        for k, y in enumerate((0.06, 0.103, 0.143)):
            a = math.atan2(y * side, 0.12)
            p0, n0 = project(tunic, [(math.cos(a) * 0.3, math.sin(a) * 0.3, zb - 0.012)])
            p0, n0 = p0[0], n0[0]
            R = frame_from(n0, (0.0, 0.0, 1.0))
            c = p0 + n0 * 0.028
            L.add(RoundBox(c, (0.021, 0.0195, 0.04), 0.008, R), 0.0)
            L.add(RoundBox(c + R @ np.array((0.004, 0.0, 0.03)), (0.019, 0.021, 0.016), 0.006, R), 0.003)
    out["pouches"] = (mesh_from_layers("pouches", [L], 0.0025, 320), None)
    # ---- the Y-straps: from the belt in front, over each shoulder, joining in the middle of the back
    g = Geo()
    for side in (1.0, -1.0):
        ctrl = [(0.13, 0.082 * side, 1.03), (0.13, 0.09 * side, 1.16), (0.125, 0.1 * side, 1.28), (0.09, 0.112 * side, 1.4),
                (0.025, 0.12 * side, 1.468), (-0.05, 0.112 * side, 1.45), (-0.12, 0.085 * side, 1.37), (-0.142, 0.045 * side, 1.29),
                (-0.145, 0.006 * side, 1.235)]
        P, N = project(tunic, spline(ctrl, 14))
        g.band(P + N * 0.004, N, 0.032, 0.004, "straps")
    P, N = project(tunic, spline([(-0.145, 0.0, 1.245), (-0.14, 0.0, 1.13), (-0.13, 0.0, 1.03)], 4))
    g.band(P + N * 0.004, N, 0.036, 0.004, "straps")
    out["straps"] = (g.build("straps"), None)
    # ---- what hangs from the belt: bread bag (right hip), water bottle (behind it), entrenching tool (left hip)
    L = Layer("bread_bag", "pack")
    flap = Layer("bread_flap", "pack")
    p0, n0 = project(tunic, [(-0.03, -0.3, 0.905)])
    p0, n0 = p0[0], n0[0]
    R = frame_from(n0, (0.0, 0.0, 1.0))
    c = p0 + n0 * 0.034
    L.add(RoundBox(c, (0.032, 0.11, 0.075), 0.02, R))
    flap.add(RoundBox(c + R @ np.array((0.006, 0.0, 0.03)), (0.03, 0.112, 0.05), 0.016, R))
    out["bread_bag"] = (mesh_from_layers("bread_bag", [L, flap], 0.003, 160), "pelvis")
    L = Layer("bottle", "pack")
    cup = Layer("cup", "steel")
    p0, n0 = project(tunic, [(-0.3, -0.16, 0.89)])
    p0, n0 = p0[0], n0[0]
    R = frame_from(n0, (0.0, 0.0, 1.0))
    c = p0 + n0 * 0.08
    L.add(Ellipsoid(c, (0.03, 0.05, 0.085), R))
    cup.add(RoundCone(c + R @ np.array((0.0, 0.0, 0.07)), c + R @ np.array((0.0, 0.0, 0.11)), 0.03, 0.028), 0.0)
    out["bottle"] = (mesh_from_layers("bottle", [L, cup], 0.003, 140), "pelvis")
    L = Layer("shovel_case", "straps")
    hnd = Layer("shovel_handle", "wood")
    p0, n0 = project(tunic, [(-0.08, 0.3, 0.88)])
    p0, n0 = p0[0], n0[0]
    R = frame_from(n0, (0.0, 0.0, 1.0))
    c = p0 + n0 * 0.024
    L.add(RoundBox(c, (0.017, 0.072, 0.088), 0.012, R))
    hnd.add(RoundCone(c + R @ np.array((0.0, 0.03, -0.07)), c + R @ np.array((0.0, 0.095, -0.175)), 0.016, 0.015))
    hnd.add(Sphere(c + R @ np.array((0.0, 0.097, -0.18)), 0.02), 0.005)
    out["shovel"] = (mesh_from_layers("shovel", [L, hnd], 0.003, 150), "pelvis")
    # ---- by nation
    if spec["canister"]:
        L = Layer("canister", "helmet")
        a, b = np.array((-0.175, -0.125, 0.845)), np.array((-0.175, 0.125, 0.885))
        p0, n0 = project(tunic, [(-0.3, 0.0, 0.865)])
        off = p0[0][0] - 0.052 - 0.006
        a[0] = b[0] = off
        L.add(RoundCone(a, b, 0.052, 0.052))
        Rax = frame_from(b - a)
        Rax = np.stack([Rax[:, 1], Rax[:, 2], Rax[:, 0]], axis=1)          # (local z along the canister)
        for t in (0.12, 0.5, 0.88):
            L.add(Torus(a + (b - a) * t, 0.0535, 0.0055, Rax), 0.003)
        L.add(Tube(b - (b - a) / np.linalg.norm(b - a) * 0.01, -0.015, 0.012, (0.056, 0.056), (0.055, 0.055), 0.006, Rax), 0.002)
        out["canister"] = (mesh_from_layers("canister", [L], 0.003, 170), "pelvis")
    if spec["pack"]:
        L = Layer("pack", "pack")
        roll = Layer("bedroll", "pack")
        p0, n0 = project(tunic, [(-0.3, 0.0, 1.25)])
        back = p0[0][0]
        c = np.array((back - 0.068, 0.0, 1.235))
        L.add(RoundBox(c, (0.07, 0.15, 0.16), 0.03))
        L.add(RoundBox(c + np.array((-0.004, 0.0, 0.08)), (0.074, 0.154, 0.07), 0.022), 0.004)          # the flap
        roll.add(RoundCone(c + np.array((0.01, 0.17, 0.205)), c + np.array((0.01, -0.17, 0.205)), 0.055, 0.055))
        out["pack"] = (mesh_from_layers("pack", [L, roll], 0.004, 260), "chest")
    if spec["roll"]:
        L = Layer("roll", "pack")
        pts = []
        for k in range(13):
            t = 2.0 * math.pi * k / 12.0
            lift = math.sin(t)
            pts.append((0.16 * math.cos(t) - 0.015, -0.03 - 0.2 * lift, 1.2 + 0.25 * lift))
        for k in range(12):
            L.add(RoundCone(pts[k], pts[k + 1], 0.05, 0.05), 0.02)
        out["roll"] = (mesh_from_layers("roll", [L], 0.005, 260), None)
    return out


# ================================================================================================ the helmets
def _cos3(front, side, back):
    """A smooth function of the bearing round the head (0 = forward) with these values ahead, abeam and behind."""
    a1 = (front - back) * 0.5
    a0 = (front + back) * 0.5 * 0.5 + side * 0.5
    a2 = (front + back) * 0.5 - a0
    return lambda f: a0 + a1 * math.cos(f) + a2 * math.cos(2 * f)


def _ahead(value, width_deg, rest=0.0):
    """A value only toward the front (a peak or a visor), falling away smoothly by `width_deg` to each side."""
    w = math.radians(width_deg)
    return lambda f: rest + (value - rest) * (0.5 + 0.5 * math.cos(math.pi * min(abs(math.atan2(math.sin(f), math.cos(f))) / w, 1.0)))


# Each helmet: the dome's radii (ax ahead, ay abeam, az up), how flat its top is (2 = an egg), where the dome
# ends (edge), then how far the skirt or brim reaches out (flare) and down (drop) beyond that, by bearing.
HELMETS = {
    "stahl": dict(ax=0.122, ay=0.108, az=0.112, dx=-0.004, edge=_cos3(0.03, -0.015, -0.03), flare=_cos3(0.022, 0.026, 0.034),
                  drop=_cos3(0.012, 0.032, 0.04)),
    "round": dict(ax=0.12, ay=0.112, az=0.113, dx=-0.003, edge=_cos3(0.02, -0.03, -0.045), flare=_cos3(0.008, 0.008, 0.009),
                  drop=_cos3(0.006, 0.006, 0.006)),
    "dome": dict(ax=0.124, ay=0.114, az=0.099, dx=-0.004, edge=_cos3(0.002, 0.0, 0.0), flare=_cos3(0.027, 0.027, 0.027),
                 drop=_cos3(0.008, 0.008, 0.008)),
    "brodie": dict(ax=0.118, ay=0.11, az=0.076, dx=-0.005, edge=_cos3(0.026, 0.026, 0.026), flare=_cos3(0.062, 0.054, 0.062),
                   drop=_cos3(0.014, 0.012, 0.014)),
    "pot": dict(ax=0.122, ay=0.114, az=0.116, dx=-0.004, edge=_cos3(0.026, -0.024, -0.036), flare=_cos3(0.013, 0.012, 0.014),
                drop=_cos3(0.008, 0.008, 0.008)),
    "soviet": dict(ax=0.122, ay=0.112, az=0.11, dx=-0.004, edge=_cos3(0.028, -0.02, -0.04), flare=_cos3(0.02, 0.014, 0.02),
                   drop=_cos3(0.01, 0.018, 0.022)),
    "adrian": dict(ax=0.118, ay=0.11, az=0.1, dx=-0.006, edge=_cos3(0.03, 0.002, 0.012), flare=_cos3(0.042, 0.012, 0.046),
                   drop=_cos3(0.012, 0.004, 0.016), crest=True),
    "eastern": dict(ax=0.12, ay=0.112, az=0.108, dx=-0.004, edge=_cos3(0.004, 0.0, 0.0), flare=_cos3(0.016, 0.016, 0.016),
                    drop=_cos3(0.004, 0.004, 0.004)),
    "cap": dict(ax=0.108, ay=0.09, az=0.1, dx=-0.004, n=3.0, edge=_cos3(0.014, 0.014, 0.01), flare=_ahead(0.042, 55.0),
                drop=_ahead(0.012, 55.0), flaps=True, cloth=True),
    "peaked": dict(ax=0.126, ay=0.113, az=0.132, dx=-0.004, edge=_cos3(0.02, -0.02, -0.03), flare=_ahead(0.042, 45.0, 0.004),
                   drop=_ahead(0.006, 45.0, 0.003)),
    "sun": dict(ax=0.13, ay=0.12, az=0.136, dx=-0.006, edge=_cos3(-0.004, -0.006, -0.006), flare=_cos3(0.046, 0.04, 0.062),
                drop=_cos3(0.034, 0.034, 0.05), band=True, cloth=True),
    "fieldcap": dict(ax=0.108, ay=0.092, az=0.098, dx=-0.004, n=3.4, edge=_cos3(0.012, 0.012, 0.012), flare=_ahead(0.036, 48.0),
                     drop=_ahead(0.01, 48.0), cloth=True),
}


def build_helmet(kind, J, sides=24):
    """One helmet as a rigid shell (outside, rolled rim, inside) with its chin strap, placed on the head in the rest
    pose. The head bone's own frame is the model's (x forward, z up), so the helmet file is the same shape moved to
    the head bone's head."""
    k = HELMETS[kind]
    C = np.array((J["head"][0] + 0.004 - 0.01 + k["dx"], 0.0, 1.655))
    th = 0.0045 if not k.get("cloth") else 0.006
    nexp = k.get("n", 2.0)
    outer, inner = [], []
    dome_rings = 4
    for i in range(sides):
        f = 2 * math.pi * i / sides
        rho = 1.0 / math.sqrt((math.cos(f) / k["ax"]) ** 2 + (math.sin(f) / k["ay"]) ** 2)
        zE = k["edge"](f)
        # the dome from the crown down to its edge: a superellipse, and straight down below its widest
        a_end = math.asin(max(min(zE / k["az"], 0.999), 0.0) ** (nexp / 2.0)) if zE > 0 else 0.0
        prof = []
        for r in range(1, dome_rings + 1):
            u = r / dome_rings
            a = math.pi / 2 - (math.pi / 2 - a_end) * (u ** 0.85)
            prof.append((rho * math.cos(a) ** (2.0 / nexp), k["az"] * math.sin(a) ** (2.0 / nexp)))
        if zE < 0:
            prof.append((rho, zE))
        else:
            prof.append((prof[-1][0], prof[-1][1] - 1e-4))
        rE, zE2 = prof[-1]
        fl, dr = k["flare"](f), k["drop"](f)
        prof.append((rE + fl * 0.45, zE2 - dr * 0.6))
        prof.append((rE + fl, zE2 - dr))
        # inside: the same curve offset inward, coarser
        ins = [(rE + fl - th * 0.6, zE2 - dr + th * 0.9), (rE - th, zE2 + th * 0.5)]
        for r in (dome_rings, 2):
            rr, zz = prof[r - 1]
            ins.append((max(rr - th, 0.0), zz - th))
        outer.append(prof)
        inner.append(ins)
    P = len(outer[0])
    Q = len(inner[0])

    def ring(lst, j, f_of_i):
        out = []
        for i in range(sides):
            f = 2 * math.pi * i / sides
            r, z = lst[i][j]
            out.append(C + np.array((math.cos(f) * r, math.sin(f) * r, z)))
        return out
    g = Geo()
    paint = "helmet"
    rings = [[C + np.array((0.0, 0.0, k["az"]))]]
    rings += [ring(outer, j, None) for j in range(P)]
    rings += [ring(inner, j, None) for j in range(Q)]
    rings += [[C + np.array((0.0, 0.0, k["az"] - th))]]
    g.shell(rings, paint)
    if k.get("crest"):
        pts, nrm = [], []
        for t in np.linspace(-0.75, 0.75, 9):
            p = C + np.array((math.sin(t) * k["ax"], 0.0, math.cos(t) * k["az"]))
            pts.append(p + np.array((math.sin(t), 0.0, math.cos(t))) * 0.012)
            nrm.append((0.0, 1.0, 0.0))
        pts = np.array(pts) + np.array((0.0, 0.006, 0.0))
        g.band(pts, nrm, 0.028, 0.012, paint)
    if k.get("band"):
        pts, nrm = [], []
        for i in range(sides):
            f = 2 * math.pi * i / sides
            r, z = outer[i][dome_rings - 1]
            pts.append(C + np.array((math.cos(f) * (r + 0.002), math.sin(f) * (r + 0.002), z + 0.012)))
            nrm.append((math.cos(f), math.sin(f), 0.0))
        g.band(pts, nrm, 0.022, 0.004, "straps", closed=True)
    if k.get("flaps"):
        # the sun flap: four cloth strips hanging from the cap's rim behind and at the sides to the collar
        for f_deg in (125.0, 160.0, 200.0, 235.0):
            f = math.radians(f_deg)
            i = int(round(f / (2 * math.pi) * sides)) % sides
            r, z = outer[i][-1]
            top = C + np.array((math.cos(f) * (r + 0.004), math.sin(f) * (r + 0.004), z + 0.004))
            low = top + np.array((math.cos(f) * 0.03, math.sin(f) * 0.03, -0.15))
            Rf = frame_from(np.array((math.cos(f), math.sin(f), 0.0)), (0.0, 0.0, 1.0))
            g.box((top + low) * 0.5, (0.003, 0.046, 0.078), paint, Rf @ rot_euler(0, -10, 0))
    if not k.get("cloth"):
        # the chin strap: from inside the rim at each side, under the chin
        for side in (1.0, -1.0):
            i = sides // 4 if side > 0 else 3 * sides // 4
            r, z = outer[i][-2]
            hx = J["head"][0] + 0.004
            ctrl = [C + np.array((0.0, side * (r - 0.012), z + 0.004)), (hx + 0.02, side * 0.07, 1.575), (hx + 0.055, side * 0.04, 1.528),
                    (hx + 0.064, 0.0, 1.512)]
            pts = spline(ctrl, 6)
            nrm = []
            for q in pts:
                v = q - np.array((hx, 0.0, 1.6))
                nrm.append(v / np.linalg.norm(v))
            g.band(pts, nrm, 0.012, 0.003, "straps")
    ob = g.build("helmet_" + kind, sharp=55.0)
    return ob


# ================================================================================================ the weapons
# Each weapon is made in its own frame: the origin where the right hand grips (the weapon bone's head), x toward the
# muzzle, z up. Both muzzles are at the same place, so socket_muzzle serves either.
MUZZLE = (0.78, 0.0, 0.04)
WEAPONS = {
    "rifle": {"grip_r": (0.0, 0.0, 0.0), "grip_l": (0.3, 0.0, 0.006), "butt": (-0.322, 0.0, -0.024), "sight_z": 0.07,
              "bolt": (-0.064, -0.058, 0.02), "carry": (0.15, 0.0, -0.004)},
    "lmg": {"grip_r": (-0.03, 0.0, -0.045), "grip_l": (0.3, 0.0, 0.004), "grip_butt": (-0.22, 0.0, 0.028), "butt": (-0.355, 0.0, -0.004),
            "sight_z": 0.082, "hinge": (0.6, 0.0, 0.018), "carry": (0.15, 0.0, -0.004), "bolt": (-0.064, -0.058, 0.02)},
}


def build_rifle():
    """A generic bolt-action rifle of our own: a one-piece wooden stock with a half-pistol grip, a straight comb and a
    steel butt plate; the receiver, the bolt and its turned-down handle; two barrel bands, a hooded front sight, a
    ramp rear sight, the trigger guard, and a leather sling. 1.10 m. Returns (rifle, bolt) objects in rifle space."""
    wood = Layer("stock", "wood")
    wood.add(RoundBox((-0.232, 0.0, -0.026), (0.09, 0.0205, 0.057), 0.016, rot_euler(0, -4, 0)))       # butt
    wood.add(RoundBox((-0.12, 0.0, -0.01), (0.06, 0.0185, 0.034), 0.015), 0.035)                        # into the wrist
    wood.add(RoundCone((-0.09, 0.0, -0.002), (0.035, 0.0, 0.012), 0.0185, 0.0175), 0.02)               # the wrist (the grip)
    wood.add(Ellipsoid((-0.022, 0.0, -0.028), (0.026, 0.0165, 0.03)), 0.02)                             # half-pistol grip
    wood.add(RoundBox((0.085, 0.0, 0.012), (0.08, 0.02, 0.03), 0.015), 0.02)                             # round the action
    wood.add(RoundBox((0.405, 0.0, 0.026), (0.255, 0.0172, 0.02), 0.0155), 0.03)                        # fore-end
    wood.add(RoundCone((0.24, 0.0, 0.052), (0.59, 0.0, 0.05), 0.0122, 0.0108), 0.006)                   # upper handguard
    rifle = mesh_from_layers("rifle_stock", [wood], 0.0025, 250)
    g = Geo()
    st = "steel"
    bz = 0.042
    g.box((-0.3225, 0.0, -0.026), (0.003, 0.0215, 0.058), st, rot_euler(0, -4, 0))                     # butt plate
    g.tube((-0.06, 0.0, 0.04), (0.17, 0.0, bz), 0.0165, 0.0155, st, 10)                                  # receiver
    g.tube((0.17, 0.0, bz), (0.78, 0.0, bz), 0.0105, 0.0085, st, 8)                                      # barrel
    g.tube((0.38, 0.0, 0.031), (0.394, 0.0, 0.031), 0.026, 0.026, st, 10, squash=1.1)                    # bands
    g.tube((0.6, 0.0, 0.034), (0.618, 0.0, 0.034), 0.023, 0.023, st, 10, squash=1.0)
    g.box((0.762, 0.0, 0.057), (0.007, 0.0028, 0.011), st)                                               # front sight
    g.tube((0.752, 0.0, 0.058), (0.774, 0.0, 0.058), 0.012, 0.012, st, 6)                               # its hood
    g.box((0.205, 0.0, 0.061), (0.028, 0.011, 0.008), st, taper=0.8)                                     # rear sight
    g.box((0.085, 0.0, -0.019), (0.042, 0.0145, 0.0035), st)                                             # magazine floorplate
    loop = [(0.0 + 0.026 * math.cos(t), 0.0, -0.016 - 0.018 * math.sin(t)) for t in np.linspace(0.15, math.pi - 0.15, 6)]
    for a, b in zip(loop[:-1], loop[1:]):
        g.tube(a, b, 0.003, 0.003, st, 4, caps=False)                                                    # trigger guard
    g.box((0.013, 0.0, -0.02), (0.003, 0.002, 0.009), st)                                                # trigger
    sl = spline([(0.387, 0.0, 0.004), (0.2, 0.0, -0.045), (-0.02, 0.0, -0.07), (-0.2, 0.0, -0.088)], 10)
    g.band(sl + np.array((0.0, 0.0, 0.0)), [(0.0, 1.0, 0.0)] * len(sl), 0.02, 0.003, "straps")          # sling
    g.absorb(rifle)
    rifle = g.build("rifle")
    b = Geo()
    b.tube((-0.105, 0.0, 0.042), (-0.055, 0.0, 0.042), 0.012, 0.0125, st, 8)                            # bolt
    b.tube((-0.05, -0.006, 0.042), (-0.062, -0.052, 0.024), 0.0042, 0.0042, st, 6)                      # its handle
    b.tube((-0.06, -0.05, 0.025), (-0.066, -0.064, 0.016), 0.0085, 0.0075, st, 6)                       # the knob
    bolt = b.build("rifle_bolt")
    return rifle, bolt


def build_lmg():
    """A generic light machine gun of our own: wooden butt with a top the left hand can press on, a pistol grip, a
    square receiver with a box magazine on the left side, a ribbed cooling jacket, a carry handle over it, a flash
    hider, and a bipod that folds back under the jacket. 1.14 m. Returns (gun, bipod) objects in its own space."""
    wood = Layer("lmg_stock", "wood")
    wood.add(RoundBox((-0.255, 0.0, -0.006), (0.1, 0.02, 0.05), 0.016, rot_euler(0, -3, 0)))
    wood.add(RoundBox((-0.13, 0.0, 0.022), (0.05, 0.017, 0.028), 0.012), 0.03)
    grip = Layer("lmg_grip", "wood")
    grip.add(RoundBox((-0.022, 0.0, -0.048), (0.018, 0.0155, 0.046), 0.011, rot_euler(0, 16, 0)))
    gun = mesh_from_layers("lmg_wood", [wood, grip], 0.0025, 240)
    g = Geo()
    st = "steel"
    bz = 0.04
    g.box((-0.358, 0.0, -0.008), (0.003, 0.0205, 0.05), st, rot_euler(0, -3, 0))                         # butt plate
    g.box((0.04, 0.0, 0.032), (0.12, 0.024, 0.033), st)                                                  # receiver
    g.box((0.05, 0.0, 0.068), (0.09, 0.012, 0.004), st)                                                  # its top cover rib
    g.box((0.06, 0.088, 0.034), (0.036, 0.066, 0.017), st, rot_euler(0, 0, -6))                          # magazine, on the left
    g.box((0.064, 0.152, 0.034), (0.032, 0.006, 0.019), st)                                              # its floor
    g.tube((0.16, 0.0, bz), (0.5, 0.0, bz), 0.023, 0.022, st, 10)                                       # cooling jacket
    for x in (0.2, 0.28, 0.36, 0.44):
        g.tube((x, 0.0, bz), (x + 0.012, 0.0, bz), 0.026, 0.026, st, 10)                                 # its ribs
    g.tube((0.5, 0.0, bz), (0.72, 0.0, bz), 0.011, 0.0105, st, 8)                                        # barrel
    g.tube((0.72, 0.0, bz), (0.78, 0.0, bz), 0.013, 0.0165, st, 8)                                       # flash hider
    g.box((0.69, 0.0, 0.062), (0.006, 0.003, 0.012), st)                                                 # front sight
    g.box((0.13, 0.0, 0.079), (0.02, 0.012, 0.01), st)                                                   # rear sight
    for x in (0.285, 0.375):
        g.tube((x, 0.0, 0.058), (x - 0.004, 0.0, 0.112), 0.0055, 0.0055, st, 6)                          # carry handle posts
    g.tube((0.27, 0.0, 0.114), (0.39, 0.0, 0.114), 0.0095, 0.0095, "wood", 8)                            # and its grip
    loop = [(0.025 + 0.03 * math.cos(t), 0.0, -0.002 - 0.022 * math.sin(t)) for t in np.linspace(0.1, math.pi - 0.1, 6)]
    for a, b in zip(loop[:-1], loop[1:]):
        g.tube(a, b, 0.0032, 0.0032, st, 4, caps=False)                                                  # trigger guard
    g.absorb(gun)
    gun = g.build("lmg")
    b = Geo()
    h = np.array(WEAPONS["lmg"]["hinge"])
    b.tube(h + (0.0, -0.016, 0.0), h + (0.0, 0.016, 0.0), 0.009, 0.009, st, 8)                           # hinge
    for side in (1.0, -1.0):
        foot = h + np.array((-0.31, 0.03 * side, -0.004))
        b.tube(h + (0.0, 0.008 * side, 0.0), foot, 0.0065, 0.0058, st, 6)                               # legs, folded back
        b.box(foot + np.array((-0.012, 0.0, -0.002)), (0.02, 0.009, 0.005), st)                          # feet
    bipod = b.build("lmg_bipod")
    return gun, bipod


# ================================================================================================ the rig
def bone_table(J):
    """(name, head, tail, parent) for every bone, parents first. The weapon bone's head is where the right hand grips
    in the rest pose, its tail along the barrel; the bolt and the bipod ride on it."""
    T = [("root", (0.0, 0.0, 0.0), (0.0, 0.0, 0.25), None),
         ("pelvis", J["pelvis"], J["spine_01"], "root"),
         ("spine_01", J["spine_01"], J["spine_02"], "pelvis"),
         ("spine_02", J["spine_02"], J["chest"], "spine_01"),
         ("chest", J["chest"], J["neck"], "spine_02"),
         ("neck", J["neck"], J["head"], "chest"),
         ("head", J["head"], J["head_end"], "neck")]
    for s in ("l", "r"):
        T += [("clavicle_" + s, J["clavicle_" + s], J["upperarm_" + s], "chest"),
              ("upperarm_" + s, J["upperarm_" + s], J["lowerarm_" + s], "clavicle_" + s),
              ("lowerarm_" + s, J["lowerarm_" + s], J["hand_" + s], "upperarm_" + s),
              ("hand_" + s, J["hand_" + s], J["fingers_" + s], "lowerarm_" + s),
              ("fingers_" + s, J["fingers_" + s], J["fingers_end_" + s], "hand_" + s),
              ("thumb_" + s, J["thumb_" + s], J["thumb_end_" + s], "hand_" + s)]
    for s in ("l", "r"):
        T += [("thigh_" + s, J["thigh_" + s], J["calf_" + s], "pelvis"),
              ("calf_" + s, J["calf_" + s], J["foot_" + s], "thigh_" + s),
              ("foot_" + s, J["foot_" + s], J["ball_" + s], "calf_" + s),
              ("ball_" + s, J["ball_" + s], J["toe_" + s], "foot_" + s)]
    g = np.array(grip_point(J, "r"))
    T += [("weapon", tuple(g), tuple(g + np.array((0.25, 0.0, 0.0))), "root")]
    bolt = g + np.array((-0.08, 0.0, 0.042))
    T += [("bolt", tuple(bolt), tuple(bolt + np.array((0.06, 0.0, 0.0))), "weapon")]
    hinge = g + np.array(WEAPONS["lmg"]["hinge"])
    T += [("bipod", tuple(hinge), tuple(hinge + np.array((-0.2, 0.0, 0.0))), "weapon")]
    return T


GRIP_IN_HAND = (0.074, 0.0, 0.031)          # where a grip's middle lies in the fist: along the hand, across, out of the palm


def grip_point(J, s):
    a, t, n = (np.array(v) for v in J["hand_axes_" + s])
    return np.array(J["hand_" + s]) + a * GRIP_IN_HAND[0] + t * GRIP_IN_HAND[1] + n * GRIP_IN_HAND[2]


def build_armature(J):
    arm_data = bpy.data.armatures.new("soldier")
    arm = bpy.data.objects.new("soldier", arm_data)
    bpy.context.scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    arm.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    eb = arm_data.edit_bones
    for name, h, t, parent in bone_table(J):
        b = eb.new(name)
        b.head, b.tail = Vector(h), Vector(t)
        # (a bone's roll: its local z kept toward the model's front or top, so the axes read the same on both sides)
        d = Vector(t) - Vector(h)
        b.align_roll(Vector((0.0, 0.0, 1.0)) if abs(d.normalized().z) < 0.7 else Vector((1.0, 0.0, 0.0)))
        if parent:
            b.parent = eb[parent]
            b.use_connect = False
    bpy.ops.object.mode_set(mode="OBJECT")
    for b in arm_data.bones:
        b.use_deform = b.name not in ("root",)
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    return arm


def bind_body(arm, body):
    """Skin the body by Blender's bone heat (automatic weights), the weapon bones kept out of it."""
    for b in arm.data.bones:
        b.use_deform = b.name not in ("root", "weapon", "bolt", "bipod")
    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    for b in arm.data.bones:
        b.use_deform = b.name != "root"
    clean_weights(body, limit=4)


def clean_weights(ob, limit=4, floor=0.02):
    """At most `limit` bones a point, none under `floor`, summing to one; a point heat missed takes its nearest neighbour's."""
    names = [g.name for g in ob.vertex_groups]
    W = np.zeros((len(ob.data.vertices), len(names)))
    for v in ob.data.vertices:
        for ge in v.groups:
            W[v.index, ge.group] = ge.weight
    empty = W.sum(axis=1) < 1e-6
    if empty.any():
        from mathutils import kdtree
        good = np.where(~empty)[0]
        kd = kdtree.KDTree(len(good))
        for i in good:
            kd.insert(ob.data.vertices[int(i)].co, int(i))
        kd.balance()
        for i in np.where(empty)[0]:
            co, j, d = kd.find(ob.data.vertices[int(i)].co)
            W[i] = W[j]
        print(f"  ({int(empty.sum())} points of {ob.name} had no weight from the heat; they take their neighbours')")
    set_weights(ob, W, names, limit, floor)
    return int(empty.sum())


def set_weights(ob, W, names, limit=4, floor=0.02):
    W = W.copy()
    order = np.argsort(-W, axis=1)
    for i in range(len(W)):
        keep = order[i, :limit]
        row = np.zeros(W.shape[1])
        row[keep] = W[i, keep]
        row[row < floor] = 0.0
        if row.sum() < 1e-9:
            row[order[i, 0]] = 1.0
        W[i] = row / row.sum()
    for g in list(ob.vertex_groups):
        ob.vertex_groups.remove(g)
    groups = [ob.vertex_groups.new(name=n) for n in names]
    for j, gname in enumerate(names):
        idx = np.where(W[:, j] > 0)[0]
        for i in idx:
            groups[j].add([int(i)], float(W[i, j]), "REPLACE")
    return W


def weights_of(ob):
    names = [g.name for g in ob.vertex_groups]
    W = np.zeros((len(ob.data.vertices), len(names)))
    for v in ob.data.vertices:
        for ge in v.groups:
            W[v.index, ge.group] = ge.weight
    return W, names


def skin_like(piece, body, k=4):
    """A piece of kit lying on the body takes the body's weights where it lies (the mean of the nearest few points)."""
    from mathutils import kdtree
    W, names = weights_of(body)
    kd = kdtree.KDTree(len(body.data.vertices))
    for v in body.data.vertices:
        kd.insert(v.co, v.index)
    kd.balance()
    Wp = np.zeros((len(piece.data.vertices), len(names)))
    Mi = piece.matrix_world
    for v in piece.data.vertices:
        found = kd.find_n(Mi @ v.co, k)
        tot = 0.0
        for co, j, d in found:
            w = 1.0 / max(d, 1e-3)
            Wp[v.index] += W[j] * w
            tot += w
        Wp[v.index] /= tot
    set_weights(piece, Wp, names, 4, 0.03)


def skin_rigid(piece, bone):
    for g in list(piece.vertex_groups):
        piece.vertex_groups.remove(g)
    g = piece.vertex_groups.new(name=bone)
    g.add(list(range(len(piece.data.vertices))), 1.0, "REPLACE")


def attach(piece, arm):
    piece.parent = arm
    piece.matrix_parent_inverse = arm.matrix_world.inverted()
    for m in list(piece.modifiers):
        if m.type == "ARMATURE":
            piece.modifiers.remove(m)
    m = piece.modifiers.new("skin", "ARMATURE")
    m.object = arm


def add_socket(arm, name, bone, at, facing=(1.0, 0.0, 0.0)):
    """An empty named socket_<name> on a bone, at a rest-pose place, its x axis along `facing`."""
    e = bpy.data.objects.new("socket_" + name, None)
    e.empty_display_type = "ARROWS"
    e.empty_display_size = 0.08
    bpy.context.scene.collection.objects.link(e)
    R = frame_from(facing, (0.0, 0.0, 1.0))
    M = Matrix.Translation(Vector(at)) @ Matrix([list(R[0]) + [0], list(R[1]) + [0], list(R[2]) + [0], [0, 0, 0, 1]])
    e.parent = arm
    e.parent_type = "BONE"
    e.parent_bone = bone
    bpy.context.view_layer.update()
    # (a bone child is placed from the bone's tail: undo that so the empty sits where it is asked)
    pb = arm.pose.bones[bone]
    Mb = arm.matrix_world @ pb.matrix @ Matrix.Translation((0.0, pb.bone.length, 0.0))
    e.matrix_parent_inverse = Mb.inverted()
    e.matrix_basis = M
    e["at"] = [float(v) for v in at]
    e["facing"] = [float(v) for v in facing]
    e["bone"] = bone
    return e


# ================================================================================================ posing
def R_axis(axis, deg):
    a = np.asarray(axis, float)
    a = a / np.linalg.norm(a)
    t = math.radians(deg)
    K = np.array([[0, -a[2], a[1]], [a[2], 0, -a[0]], [-a[1], a[0], 0]])
    return np.eye(3) + math.sin(t) * K + (1 - math.cos(t)) * (K @ K)


def E(fwd=0.0, left=0.0, turn=0.0):
    """A turn of the body said plainly: bend forward, lean to the left, turn to the left (degrees, model axes)."""
    return R_axis((0, 0, 1), turn) @ R_axis((0, 1, 0), fwd) @ R_axis((1, 0, 0), -left)


def frame2(a, n):
    """A rotation whose columns are a, n (made square to a) and a x n."""
    a = _n(a)
    n = np.asarray(n, float) - a * (np.asarray(n, float) @ a)
    n = _n(n)
    return np.stack([a, n, np.cross(a, n)], axis=1)


def look_rot(x_axis, up=(0.0, 0.0, 1.0)):
    return frame_from(x_axis, up)


def M4(R=None, t=(0.0, 0.0, 0.0)):
    M = np.eye(4)
    if R is not None:
        M[:3, :3] = R
    M[:3, 3] = t
    return M


class Rig:
    """The skeleton's rest, and the solver that turns a pose said in plain terms into bone rotations."""

    def __init__(self, arm, J):
        self.arm, self.J = arm, J
        self.names = [b.name for b in arm.data.bones]
        self.parent = {b.name: (b.parent.name if b.parent else None) for b in arm.data.bones}
        self.H0 = {b.name: np.array(b.head_local) for b in arm.data.bones}
        self.T0 = {b.name: np.array(b.tail_local) for b in arm.data.bones}
        self.M0 = {b.name: b.matrix_local.copy() for b in arm.data.bones}
        self.hand0 = {s: tuple(np.array(v) for v in J["hand_axes_" + s]) for s in ("l", "r")}
        self.prev_q = {}
        self.eye0 = {"r": np.array((J["head"][0] + 0.004 + 0.083, -0.033, 1.64)), "l": np.array((J["head"][0] + 0.004 + 0.083, 0.033, 1.64))}

    # ---- forward kinematics
    def _child(self, st, b, L=None, Dw=None):
        p = self.parent[b]
        Dp, Hpp = st["D"][p], st["H"][p]
        st["H"][b] = Hpp + Dp @ (self.H0[b] - self.H0[p])
        st["D"][b] = Dw if Dw is not None else Dp @ (np.eye(3) if L is None else L)

    def at(self, st, b, rest_point):
        """Where a point that rests at `rest_point` and rides on bone b is now."""
        return st["H"][b] + st["D"][b] @ (np.asarray(rest_point) - self.H0[b])

    def frame(self, st, b):
        return M4(st["D"][b], st["H"][b])

    # ---- two-bone IK
    def two_bone(self, st, up, mid, end, target, pole):
        A = st["H"][up]
        l1 = np.linalg.norm(self.H0[mid] - self.H0[up])
        l2 = np.linalg.norm(self.H0[end] - self.H0[mid])
        d = np.asarray(target, float) - A
        dist = float(np.linalg.norm(d))
        dist_c = min(max(dist, abs(l1 - l2) + 1e-4), (l1 + l2) * 0.9995)
        u = d / max(dist, 1e-9)
        v = np.asarray(pole, float) - u * (np.asarray(pole, float) @ u)
        if np.linalg.norm(v) < 1e-6:
            v = np.cross(u, (0.0, 1.0, 0.0))
        v = _n(v)
        ca = (l1 * l1 + dist_c * dist_c - l2 * l2) / (2 * l1 * dist_c)
        sa = math.sqrt(max(1 - ca * ca, 0.0))
        K = A + l1 * (u * ca + v * sa)
        T = A + u * dist_c
        # rest frames: each bone's direction and the way its joint bends (from the rest pose itself)
        r_mid = self.H0[mid] - self.H0[up]
        r_end = self.H0[end] - self.H0[mid]
        r_line = self.H0[end] - self.H0[up]
        r_pole = r_mid - _n(r_line) * (r_mid @ _n(r_line))
        Fu = frame2(r_mid, r_pole)
        Fl = frame2(r_end, r_pole)
        D_up = frame2(K - A, v) @ Fu.T
        D_lo = frame2(T - K, v) @ Fl.T
        st["D"][up] = D_up
        st["H"][mid] = K
        st["D"][mid] = D_lo
        return dist - dist_c          # (how far short it fell: 0 when reached)

    # ---- the whole pose
    def solve(self, P):
        st = {"D": {}, "H": {}, "short": {}}
        st["D"]["root"], st["H"]["root"] = np.eye(3), np.zeros(3)
        st["H"]["pelvis"] = self.H0["pelvis"] + np.asarray(P.get("pelvis_at", (0, 0, 0)), float)
        st["D"]["pelvis"] = E(*P.get("pelvis", (0, 0, 0)))
        for b in ("spine_01", "spine_02", "chest", "neck", "head"):
            self._child(st, b, E(*P.get(b, (0, 0, 0))))
        for s, side in (("l", 1.0), ("r", -1.0)):
            up, fw = P.get("clavicle_" + s, (0.0, 0.0))
            self._child(st, "clavicle_" + s, R_axis((1, 0, 0), up * side) @ R_axis((0, 0, 1), -fw * side))
            self._child(st, "upperarm_" + s)
        # legs
        for s in ("l", "r"):
            f = P["foot_" + s]
            self._child(st, "thigh_" + s)
            Df = E(f.get("pitch", 0.0), f.get("roll", 0.0), f.get("yaw", 0.0))
            pole = f.get("pole")
            if pole is None:
                pole = Df @ np.array((1.0, 0.0, 0.0))
            st["short"]["leg_" + s] = self.two_bone(st, "thigh_" + s, "calf_" + s, "foot_" + s, f["at"], pole)
            self._child(st, "foot_" + s, Dw=Df)
            self._child(st, "ball_" + s, R_axis((0, 1, 0), -f.get("toe", 0.0)))
        # the weapon
        W = P["weapon"](self, st) if callable(P["weapon"]) else P["weapon"]
        st["D"]["weapon"], st["H"]["weapon"] = W[:3, :3], W[:3, 3] + W[:3, :3] @ (self.H0["weapon"] - self.H0["weapon"])
        st["H"]["weapon"] = W[:3, 3].copy()
        st["W"] = W
        bk, lift = P.get("bolt", (0.0, 0.0))
        st["D"]["bolt"] = st["D"]["weapon"] @ R_axis((1, 0, 0), -lift)
        st["H"]["bolt"] = st["H"]["weapon"] + st["D"]["weapon"] @ (self.H0["bolt"] - self.H0["weapon"] + np.array((-bk, 0.0, 0.0)))
        self._child(st, "bipod", R_axis((0, 1, 0), -P.get("bipod", 0.0)))
        # the arms: each hand to its grip, or to a place of its own
        for s, side in (("l", 1.0), ("r", -1.0)):
            a0, t0, n0 = self.hand0[s]
            target_g, a1, n1, pole = self.resolve_hand(st, P, P["hand_" + s], side)
            Dh = frame2(a1, n1) @ frame2(a0, n0).T
            g_rest = grip_point(self.J, s)
            wrist = target_g - Dh @ (g_rest - self.H0["hand_" + s])
            st["short"]["arm_" + s] = self.two_bone(st, "upperarm_" + s, "lowerarm_" + s, "hand_" + s, wrist, pole)
            self._child(st, "hand_" + s, Dw=Dh)
            k = _n(np.cross(a0, n0))
            self._child(st, "fingers_" + s, R_axis(k, P.get("fingers_" + s, 0.0)))
            dt = self.T0["thumb_" + s] - self.H0["thumb_" + s]
            kt = _n(np.cross(dt, n0))
            self._child(st, "thumb_" + s, R_axis(kt, P.get("thumb_" + s, 0.0)))
        return st

    def resolve_hand(self, st, P, h, side):
        """A hand's spec -> (grip point in the world, hand direction, palm direction, elbow pole)."""
        if "blend" in h:
            out = [(w, self.resolve_hand(st, P, hh, side)) for w, hh in h["blend"]]
            g = sum(w * r[0] for w, r in out)
            a = sum(w * r[1] for w, r in out)
            n = sum(w * r[2] for w, r in out)
            pole = sum(w * r[3] for w, r in out)
            return g, _n(a), _n(n), pole
        W = st["W"]
        Rw = W[:3, :3]
        if "grip" in h:
            spec = WEAPONS[P.get("weapon_kind", "rifle")]
            if h["grip"] == "knob":
                bk, lift = P.get("bolt", (0.0, 0.0))
                b0 = np.array((-0.08, 0.0, 0.042))
                g = b0 + R_axis((1, 0, 0), -lift) @ (np.array(WEAPONS["rifle"]["bolt"]) - b0) - np.array((bk, 0.0, 0.0))
            else:
                g = np.array(spec[h["grip"]])
            g = g + np.asarray(h.get("slide", (0, 0, 0)), float)
            target = W[:3, 3] + Rw @ g
            a1, n1 = Rw @ np.asarray(h["a"], float), Rw @ np.asarray(h["n"], float)
        elif h.get("frame") == "chest":
            C = self.frame(st, "chest")
            target = C[:3, :3] @ np.asarray(h["at"], float) + C[:3, 3]
            a1, n1 = C[:3, :3] @ np.asarray(h["a"], float), C[:3, :3] @ np.asarray(h["n"], float)
        else:
            target = np.asarray(h["at"], float)
            a1, n1 = np.asarray(h["a"], float), np.asarray(h["n"], float)
        if "pole_world" in h:
            pole = np.asarray(h["pole_world"], float)
        else:
            pole = st["D"]["chest"] @ np.asarray(h.get("pole", (-0.3, 0.6 * side, -1.0)), float)
        return target, a1, n1, pole

    # ---- keys
    def key(self, st, frame):
        """Write the solved pose as plain keys (rotation, and location where a bone moves) at `frame`."""
        W = {}
        for b in self.names:
            R0 = np.array(self.M0[b].to_3x3())
            M = Matrix(M4(st["D"][b] @ R0, st["H"][b]).tolist())
            W[b] = M
            pb = self.arm.pose.bones[b]
            p = self.parent[b]
            if p is None:
                basis = self.M0[b].inverted() @ M
            else:
                basis = (self.M0[p].inverted() @ self.M0[b]).inverted() @ W[p].inverted() @ M
            q = basis.to_quaternion()
            last = self.prev_q.get(b)
            if last is not None and last.dot(q) < 0.0:
                q.negate()
            self.prev_q[b] = q.copy()
            pb.rotation_quaternion = q
            pb.location = basis.translation
            pb.keyframe_insert("rotation_quaternion", frame=frame, group=b)
            if b in ("pelvis", "weapon", "bolt", "root"):
                pb.keyframe_insert("location", frame=frame, group=b)
        return W

    def eye(self, st, s="r"):
        return self.at(st, "head", self.eye0[s])


# ================================================================================================ the man, put together
def assemble(nation, weapon="rifle", J=None):
    """Sculpt, dress, arm and rig one soldier of a nation. Returns a dict of his objects and his Rig."""
    J = J or joints()
    spec = NATION_KIT[nation]
    LOOK_NOW.update(LOOK.get(nation, LOOK["GER"]))
    t0 = time.time()
    head = mesh_from_layers("head", [head_layer(J), eye_layer(J)], 0.002, 1700)
    hands = [mesh_from_layers("hand_" + s, [hand_layer(J, s)], 0.002, 190) for s in ("l", "r")]
    ls = body_layers(J, spec)
    body = mesh_from_layers("body", ls, 0.0034, 3950)
    g = Geo()
    for o in [body, head] + hands:
        g.absorb(o)
    body = g.build("body_" + nation, sharp=180.0, recalc=False)
    kitp = build_kit(J, spec, ls)
    helmet = build_helmet(spec["helmet"], J)
    if weapon == "rifle":
        gun, part = build_rifle()
        part_bone = "bolt"
    else:
        gun, part = build_lmg()
        part_bone = "bipod"
    print(f"  {nation}: sculpted in {time.time() - t0:.1f} s")
    arm = build_armature(J)
    arm.name = "soldier_" + nation
    t0 = time.time()
    bind_body(arm, body)
    print(f"  bone heat weights in {time.time() - t0:.1f} s")
    attach(body, arm)
    kit_objs = []
    for name, (ob, bone) in kitp.items():
        if bone:
            skin_rigid(ob, bone)
        else:
            skin_like(ob, body)
        attach(ob, arm)
        kit_objs.append(ob)
    skin_rigid(helmet, "head")
    attach(helmet, arm)
    # the weapon sits on the weapon bone: made in its own frame, moved to the bone's rest place
    Wm = Matrix.Translation(Vector(arm.data.bones["weapon"].head_local))
    for ob, bone in ((gun, "weapon"), (part, part_bone)):
        ob.data.transform(Wm)
        skin_rigid(ob, bone)
        attach(ob, arm)
    mark_rest(body, dusty=True)
    for ob in kit_objs + [helmet, gun, part]:
        mark_rest(ob)
    rig = Rig(arm, J)
    g0 = np.array(arm.data.bones["weapon"].head_local)
    sockets = [add_socket(arm, "muzzle", "weapon", tuple(g0 + np.array(MUZZLE))),
               add_socket(arm, "hand_r", "hand_r", tuple(grip_point(J, "r")), tuple(rig.hand0["r"][0])),
               add_socket(arm, "head", "head", J["head"]),
               add_socket(arm, "chest", "chest", (0.13, 0.0, 1.3)),
               add_socket(arm, "weapon", "weapon", tuple(g0))]
    return {"nation": nation, "arm": arm, "rig": rig, "body": body, "kit": kit_objs, "helmet": helmet, "weapon": gun,
            "weapon_part": part, "sockets": sockets, "J": J, "spec": spec, "weapon_kind": weapon}


def man_objects(m):
    return [m["body"]] + m["kit"] + [m["helmet"], m["weapon"], m["weapon_part"]]


# ================================================================================================ the clips
FPS = 60          # (every clip's length is a whole number of sixtieths: 0.65 s is 39 frames)

# How each hand holds each weapon, in the weapon's own frame: `a` from the wrist to the knuckles, `n` the way the
# palm faces. (The right hand's palm on the right of the stock's wrist, the thumb over the top; the left hand
# cradling the fore-end from beneath, its thumb along the near side.)
HOLD = {
    "r": {"grip": "grip_r", "a": (0.5, 0.0, -0.85), "n": (0.0, 1.0, 0.0), "pole": (-0.3, -0.75, -0.55)},
    "l": {"grip": "grip_l", "a": (0.15, -0.95, -0.28), "n": (0.0, -0.3, 1.0), "pole": (0.0, 0.45, -1.0)},
}


def held(s, **over):
    h = dict(HOLD[s])
    h.update(over)
    return h


def port_arms(lift=0.0, out=0.0, tilt=0.0):
    """The rifle carried across the body at the port: its wrist at the right hip, the muzzle up past the left
    shoulder; placed from the chest, so it rides with the man's breathing and stride."""
    def place(rig, st):
        C = rig.frame(st, "chest")
        o = np.array((0.25 + out, -0.088, -0.215 + lift))
        x = _n((0.3, 0.55, 0.78))
        R = frame_from(x, (1.0, 0.0, 0.25))
        R = R @ R_axis((1, 0, 0), tilt)
        return C @ M4(R, o)
    return place


def aim(yaw=0.0, pitch=0.0, relief=-0.045, kick=0.0, rise=0.0):
    """The rifle shouldered and aimed: its sight line through the right eye, along the bearing given."""
    def place(rig, st):
        eye = rig.eye(st, "r")
        R = R_axis((0, 0, 1), yaw) @ R_axis((0, 1, 0), -(pitch + rise))
        spec = WEAPONS["rifle"]
        o = eye - R @ np.array((relief - kick, 0.0, spec["sight_z"]))
        return M4(R, o)
    return place


def stand_feet(ahead=0.05, apart=0.12, back=-0.06):
    return {"foot_l": {"at": (ahead, apart, 0.088), "yaw": 8.0}, "foot_r": {"at": (back, -apart, 0.088), "yaw": -12.0}}


def base_stand(t=0.0):
    P = {"pelvis_at": (0.0, 0.0, -0.012), "pelvis": (2.0, 0.0, -4.0), "spine_01": (1.0, 0, 1.0), "spine_02": (1.5, 0, 1.0),
         "chest": (1.5, 0, 1.5), "neck": (-2.0, 0, 0.5), "head": (-1.0, 0, 0.5), "clavicle_l": (0.0, 3.0), "clavicle_r": (0.0, 3.0),
         "weapon": port_arms(), "hand_l": held("l"), "hand_r": held("r"), "fingers_l": 25.0, "fingers_r": 25.0, "thumb_l": 20.0,
         "thumb_r": 25.0}
    P.update(stand_feet())
    return P


# ================================================================================================ pictures
def shared_render(w, h, samples):
    """kit.render_setup, then two threads only: other artists share the four cores."""
    sc = kit.render_setup(w, h, samples)
    sc.render.threads_mode = "FIXED"
    sc.render.threads = 2
    return sc


def load_game_model(path, look, name="old", at=(0.0, 0.0, 0.0)):
    """A model in the game's own format (build_tank.write) as a Blender mesh, coloured by its paints."""
    d = json.load(open(path))
    near = d["near"]
    pts = np.array(near["points"]).reshape(-1, 3) * np.array((1.0, -1.0, 1.0)) + np.array(at)
    tri = np.array(near["triangles"]).reshape(-1, 3)[:, ::-1]           # (y turned back: the winding turns too)
    paints = np.array(near["paints"])
    me = bpy.data.meshes.new(name)
    me.from_pydata(pts.tolist(), [], tri.tolist())
    by_index = {v: k for k, v in FIGURE_PAINTS.items()}
    used = sorted(set(paints.tolist()))
    for u in used:
        pn = by_index.get(u, "tunic")
        me.materials.append(paint_material(pn) if pn in LOOK_NOW else kit.material("old_" + pn, kit.hex_rgb("6E6852")))
    face_paint = paints[tri[:, 0]]
    me.polygons.foreach_set("material_index", np.array([used.index(p) for p in face_paint], np.int32))
    me.polygons.foreach_set("use_smooth", np.zeros(len(tri), bool))
    me.update()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def label(path, texts, size=26):
    """Captions written on a picture: [(x, y, text)]."""
    from PIL import Image, ImageDraw
    im = Image.open(path).convert("RGB")
    d = ImageDraw.Draw(im)
    font = kit._font(size)
    for x, y, t in texts:
        d.text((x + 2, y + 2), t, fill=(20, 18, 16), font=font)
        d.text((x, y), t, fill=(246, 240, 228), font=font)
    im.save(path)


def picture_before_after(m, out, fast=False):
    """Today's lay figure (Rifleman_Stahl, from the game's file) beside the new German soldier: same light, same camera."""
    sc = bpy.context.scene
    old = load_game_model(os.path.join(REPO, "Content", "Models", "Rifleman_Stahl.json"), LOOK["GER"], "Rifleman_Stahl", (0.0, -0.62, 0.0))
    m["arm"].location = (0.0, 0.62, 0.0)
    rig = m["rig"]
    P = base_stand()
    P["foot_l"] = {"at": (0.17, 0.11, 0.088), "yaw": 5.0}
    P["foot_r"] = {"at": (-0.1, -0.12, 0.088), "yaw": -10.0, "pitch": 0.0}
    P["pelvis_at"] = (0.02, 0.0, -0.02)
    rig.key(rig.solve(P), 1)
    sc.frame_set(1)
    kit.studio()
    ground = kit.backdrop("CFC6B4")
    cam = kit.camera(lens=50)
    kit.orbit(cam, (0.0, 0.0, 0.9), 5.6, 6.0, 22.0)
    shared_render(640 if fast else 1280, 360 if fast else 720, 16 if fast else 48)
    t = time.time()
    kit.still(out)
    print(f"  before-after: {time.time() - t:.0f} s")
    W = 640 if fast else 1280
    label(out, [(int(W * 0.05), int(W * 0.03), "Today: the lay figure (Rifleman_Stahl)"),
                (int(W * 0.56), int(W * 0.03), "New: the German soldier")], size=13 if fast else 26)
    bpy.data.objects.remove(old)
    bpy.data.objects.remove(ground)
    m["arm"].location = (0.0, 0.0, 0.0)
    return out


# ---- small helpers for motion
def sm(x):
    x = min(max(x, 0.0), 1.0)
    return x * x * (3 - 2 * x)


def ramp(t, t0, t1):
    return sm((t - t0) / (t1 - t0)) if t1 > t0 else float(t >= t0)


def bump(t, t0, t1, t2):
    """0 before t0, rising to 1 at t1, back to 0 at t2."""
    return ramp(t, t0, t1) * (1.0 - ramp(t, t1, t2))


def osc(t, period, phase=0.0):
    return math.sin(2 * math.pi * (t / period + phase))


def add3(a, b):
    return tuple(float(x) + float(y) for x, y in zip(a, b))


def blend_places(items):
    """Weighted mix of weapon placements [(w, place)] (each a matrix, or a callable of the rig and pose)."""
    def place(rig, st):
        Ms = [(w, (p(rig, st) if callable(p) else p)) for w, p in items if abs(w) > 1e-6]
        pos = sum(w * M[:3, 3] for w, M in Ms)
        q0 = Matrix(Ms[0][1][:3, :3].tolist()).to_quaternion()
        acc = np.zeros(4)
        for w, M in Ms:
            q = Matrix(M[:3, :3].tolist()).to_quaternion()
            if q.dot(q0) < 0:
                q.negate()
            acc += w * np.array(q)
        q = Quaternion(acc).normalized()
        return M4(np.array(q.to_matrix()), pos)
    return place


def mix(Ps, ws):
    """A pose that is the weighted mix of several (numbers mixed, weapon placements blended, hands blended)."""
    out = {}
    keys = set()
    for P in Ps:
        keys |= set(P.keys())
    pairs = [(w, P) for w, P in zip(ws, Ps)]
    main = max(pairs, key=lambda x: x[0])[1]
    for k in keys:
        vals = [(w, P[k]) for w, P in pairs if k in P]
        v0 = vals[0][1]
        if k == "weapon":
            out[k] = blend_places(vals)
        elif k.startswith("hand_"):
            out[k] = {"blend": vals}
        elif k.startswith("foot_"):
            f = {}
            for fk in ("at", "yaw", "pitch", "roll", "toe", "pole"):
                acc, any_ = None, False
                for w, fv in vals:
                    x = fv.get(fk)
                    if fk == "pole" and x is None:
                        yaw = math.radians(fv.get("yaw", 0.0))
                        x = (math.cos(yaw), math.sin(yaw), 0.0)
                    if x is None:
                        x = 0.0 if fk != "at" else (0.0, 0.0, 0.0)
                    else:
                        any_ = True
                    x = np.asarray(x, float)
                    acc = w * x if acc is None else acc + w * x
                if any_ or fk in ("at", "pole"):
                    f[fk] = acc if np.ndim(acc) else float(acc)
            out[k] = f
        elif isinstance(v0, str):
            out[k] = main.get(k, v0)
        else:
            acc = None
            for w, x in vals:
                x = np.asarray(x, float)
                acc = w * x if acc is None else acc + w * x
            out[k] = tuple(acc) if np.ndim(acc) else float(acc)
    return out


def keyed(keys):
    """A clip from key poses [(t, P)], passed through smoothly (Catmull-Rom; the first and last held)."""
    times = [k[0] for k in keys]
    poses = [k[1] for k in keys]

    def at(t):
        if t <= times[0]:
            return poses[0]
        if t >= times[-1]:
            return poses[-1]
        i = max(j for j in range(len(times) - 1) if times[j] <= t)
        u = (t - times[i]) / (times[i + 1] - times[i])
        p0, p1, p2, p3 = poses[max(i - 1, 0)], poses[i], poses[i + 1], poses[min(i + 2, len(poses) - 1)]
        w0 = (-u ** 3 + 2 * u * u - u) * 0.5
        w1 = (3 * u ** 3 - 5 * u * u + 2) * 0.5
        w2 = (-3 * u ** 3 + 4 * u * u + u) * 0.5
        w3 = (u ** 3 - u * u) * 0.5
        if i == 0:
            w1, w0 = w1 + w0, 0.0
        if i + 2 >= len(poses):
            w2, w3 = w2 + w3, 0.0
        Ps, ws = [], []
        for P, w in ((p0, w0), (p1, w1), (p2, w2), (p3, w3)):
            if abs(w) > 1e-9:
                Ps.append(P)
                ws.append(w)
        return mix(Ps, ws)
    return at


# ---- walking and running
FOOT_HEEL = np.array((-0.061, 0.0, -0.088))      # the heel's ground point from the ankle
FOOT_BALL = np.array((0.146, 0.0, -0.088))       # the ball's
LEG_REACH = 0.822


class Gait:
    """Each foot planted for `duty` of the loop and carried back at exactly stride/length metres a second, rolling
    from heel to ball; the hips drop wherever a straight leg could not reach. Left heel down at 0, right at `right`."""

    def __init__(self, long, stride, duty, lift, center=0.04, width=0.11, heel=-14.0, push=34.0, flat=(0.13, 0.55),
                 base=-0.015, bob=0.012, right=0.5, toe_out=6.0, lean=0.0):
        self.long, self.stride, self.duty, self.lift = long, stride, duty, lift
        self.v = stride / long
        self.width, self.heel, self.push, self.flat, self.toe_out = width, heel, push, flat, toe_out
        self.right = right
        Ts = duty * long
        self.xh0 = center + self.v * Ts * 0.5 + FOOT_HEEL[0]
        self.base, self.bob, self.lean = base, bob, lean
        n = 240
        ts = np.linspace(0, long, n, endpoint=False)
        z = np.array([base + bob * math.cos(2 * math.pi * (2 * t / long - 2 * self.duty * 0.5 / 1.0)) for t in ts])
        lim = np.array([self._limit(t) for t in ts])
        z = np.minimum(z, lim)
        for _ in range(3):
            k = 9
            zz = np.concatenate([z[-k:], z, z[:k]])
            z = np.convolve(zz, np.ones(2 * k + 1) / (2 * k + 1), mode="same")[k:-k]
            z = np.minimum(z, lim)
        self.zs, self.ts = z, ts

    def phase(self, t, s):
        off = 0.0 if s == "l" else self.right
        return ((t / self.long) - off) % 1.0

    def foot(self, t, s):
        side = 1.0 if s == "l" else -1.0
        ph = self.phase(t, s)
        T, Ts = self.long, self.duty * self.long
        y = self.width * side
        yaw = self.toe_out * side
        if ph < self.duty:
            tau = ph * T
            sx = tau / Ts
            a_heel, a_off = self.flat
            hx = self.xh0 - self.v * tau
            if sx < a_heel:
                th = self.heel * (1 - sm(sx / a_heel))
                c = np.array((hx, y, 0.0))
                ank = c - R_axis((0, 1, 0), th) @ FOOT_HEEL
                toe = 0.0
            elif sx < a_off:
                th = 0.0
                ank = np.array((hx, y, 0.0)) - FOOT_HEEL
                toe = 0.0
            else:
                u = (sx - a_off) / (1 - a_off)
                th = self.push * u ** 1.6
                hx_off = self.xh0 - self.v * a_off * Ts
                bx = hx_off - FOOT_HEEL[0] + FOOT_BALL[0] - self.v * (tau - a_off * Ts)
                ank = np.array((bx, y, 0.0)) - R_axis((0, 1, 0), th) @ FOOT_BALL
                toe = th
            return {"at": tuple(ank), "pitch": th, "toe": toe, "yaw": yaw}, True
        u = (ph - self.duty) / (1 - self.duty)
        a_off = self.flat[1]
        hx_off = self.xh0 - self.v * a_off * Ts
        bx_end = hx_off - FOOT_HEEL[0] + FOOT_BALL[0] - self.v * (1 - a_off) * Ts
        A1 = np.array((bx_end, y, 0.0)) - R_axis((0, 1, 0), self.push) @ FOOT_BALL
        A0 = np.array((self.xh0, y, 0.0)) - R_axis((0, 1, 0), self.heel) @ FOOT_HEEL
        ex = 0.5 - 0.5 * math.cos(math.pi * u)
        p = A1 + (A0 - A1) * ex
        p[2] += self.lift * math.sin(math.pi * min(u ** 0.75, 1.0))
        th = self.push + (self.heel - self.push) * sm(u * 1.1)
        toe = self.push * (1 - sm(u / 0.35))
        return {"at": tuple(p), "pitch": th, "toe": toe, "yaw": yaw}, False

    def _limit(self, t):
        """The highest the pelvis may stand at time t for both feet to be reached."""
        best = 1.0
        for s, side in (("l", 1.0), ("r", -1.0)):
            f, _ = self.foot(t, s)
            a = np.array(f["at"])
            hip = np.array((0.0 - 0.0, 0.09 * side, 0.915))
            dx, dy = a[0] - hip[0] - self.lean, a[1] - hip[1]
            h2 = LEG_REACH ** 2 - dx * dx - dy * dy
            if h2 <= 0:
                continue
            zmax = a[2] + math.sqrt(h2) - hip[2]
            best = min(best, zmax)
        return best

    def pelvis_z(self, t):
        return float(np.interp(t % self.long, np.append(self.ts, self.long), np.append(self.zs, self.zs[0])))

    def stance_side(self, t):
        """+1 while the weight is on the left foot, -1 on the right, smoothly between."""
        return math.cos(2 * math.pi * (t / self.long - self.duty * 0.5 + 0.0) * 1.0)


# ================================================================================================ the clips themselves
CLIPS = {}


def clip(name, long, loop=False, stride=0.0, events=(), weapon="rifle"):
    def deco(fn):
        CLIPS[name] = {"long": long, "loop": loop, "stride": stride, "events": list(events), "fn": fn, "weapon": weapon}
        return fn
    return deco


# The sound director's cue names for the moments that are not shots (the game picks a variant: footstep_01 to _04,
# body_fall_01 to _04, kit_rattle_01 or _02, and the nation's own bolt, bolt_k98 for Germany). No sound files ship.
CUES = {"footstep": "footstep", "knee_down": "knee_down", "body_fall": "body_fall", "bolt": "bolt", "kit_rattle": "kit_rattle",
        "grenade_throw": "grenade_throw"}


def ev(t, fx, socket=None, cue=None, **more):
    e = {"t": t, "fx": fx, "socket": socket, "cue": cue if cue is not None else CUES.get(fx)}
    e.update(more)
    return e


def breathe(P, t, period=4.0, depth=1.0):
    b = osc(t, period) * depth
    P["chest"] = add3(P.get("chest", (0, 0, 0)), (-0.8 * b, 0, 0))
    P["spine_02"] = add3(P.get("spine_02", (0, 0, 0)), (-0.4 * b, 0, 0))
    P["clavicle_l"] = add3(P.get("clavicle_l", (0, 0)), (1.2 * b, 0))[:2]
    P["clavicle_r"] = add3(P.get("clavicle_r", (0, 0)), (1.2 * b, 0))[:2]
    return P


@clip("idle", 4.0, loop=True)
def c_idle(t):
    P = base_stand()
    w = osc(t, 4.0, 0.1)                                     # weight shifts from foot to foot
    P["pelvis_at"] = (0.0, 0.016 * w, -0.014 - 0.004 * abs(w))
    P["pelvis"] = (2.0, -2.2 * w, -4.0 + 1.5 * w)
    P["spine_01"] = (1.0, 1.2 * w, 1.0)
    P["chest"] = (1.5, 0.8 * w, 1.5 - 0.8 * w)
    look = bump(t, 0.9, 1.5, 2.4) - 0.6 * bump(t, 2.5, 3.0, 3.7)
    P["neck"] = (-2.0 + 2.0 * bump(t, 2.5, 3.0, 3.7), 0.0, 0.5 + 14.0 * look)
    P["head"] = (-1.0 - 2.0 * bump(t, 0.9, 1.5, 2.4), -1.5 * look, 0.5 + 16.0 * look)
    P["weapon"] = port_arms(lift=0.006 * osc(t, 4.0, 0.3))
    return breathe(P, t, 4.0)


WALK = Gait(1.1, 1.5, 0.58, 0.07, center=-0.085, width=0.105, heel=-16.0, push=52.0, flat=(0.12, 0.45), base=-0.012, bob=0.012)
RUN = Gait(0.7, 2.45, 0.35, 0.17, center=-0.08, width=0.1, heel=-6.0, push=56.0, flat=(0.1, 0.35), base=-0.07, bob=0.02, lean=0.05)
FLEE = Gait(0.65, 2.8, 0.33, 0.2, center=-0.095, width=0.1, heel=-4.0, push=58.0, flat=(0.1, 0.33), base=-0.045, bob=0.025,
            right=0.32 / 0.65, lean=0.03)


def gait_pose(g, t, lean, twist, sway, P):
    for s in ("l", "r"):
        f, _ = g.foot(t, s)
        P["foot_" + s] = f
    ph = 2 * math.pi * t / g.long
    P["pelvis_at"] = (g.lean, sway * math.cos(ph), g.pelvis_z(t))
    # the pelvis turns with the swinging leg and drops on its side; the chest turns against it
    P["pelvis"] = (lean * 0.4, -2.5 * math.cos(ph) * (sway / 0.02), -twist * math.cos(ph + 0.4))
    P["spine_01"] = (lean * 0.2, 1.0 * math.cos(ph), twist * 0.35 * math.cos(ph + 0.4))
    P["spine_02"] = (lean * 0.2, 0.6 * math.cos(ph), twist * 0.4 * math.cos(ph + 0.4))
    P["chest"] = (lean * 0.2, 0.4 * math.cos(ph), twist * 0.45 * math.cos(ph + 0.4))
    return P


@clip("walk", 1.1, loop=True, stride=1.5, events=[ev(0.0, "footstep", bone="foot_l"), ev(0.55, "footstep", bone="foot_r")])
def c_walk(t):
    P = base_stand()
    gait_pose(WALK, t, 5.0, 6.0, 0.016, P)
    ph = 2 * math.pi * t / WALK.long
    P["neck"] = (-3.0, 0.0, 2.0 * math.cos(ph + 0.4))
    P["head"] = (-2.5 + 1.5 * math.cos(2 * ph), 0.0, 1.5 * math.cos(ph + 0.4))
    P["weapon"] = port_arms(lift=0.008 * math.cos(2 * ph + 0.8), tilt=3.0 * math.cos(ph))
    return breathe(P, t, 1.1 * 2, 0.6)


@clip("run", 0.7, loop=True, stride=2.45, events=[ev(0.0, "footstep", bone="foot_l"), ev(0.0, "kit_rattle", bone="chest", first_loop_only=True),
                                                   ev(0.35, "footstep", bone="foot_r")])
def c_run(t):
    P = base_stand()
    gait_pose(RUN, t, 20.0, 9.0, 0.014, P)
    ph = 2 * math.pi * t / RUN.long
    P["neck"] = (-12.0, 0.0, 2.0 * math.cos(ph))
    P["head"] = (-10.0 + 2.0 * math.cos(2 * ph + 0.5), 0.0, 2.0 * math.cos(ph))
    P["weapon"] = port_arms(lift=0.03 + 0.014 * math.cos(2 * ph + 1.0), out=0.04, tilt=6.0 * math.cos(ph))
    P["clavicle_l"] = (2.0, 6.0)
    P["clavicle_r"] = (2.0, 6.0)
    return P


@clip("flee", 0.65, loop=True, stride=2.8, events=[ev(0.0, "footstep", bone="foot_l"), ev(0.32, "footstep", bone="foot_r")])
def c_flee(t):
    P = base_stand()
    gait_pose(FLEE, t, 14.0, 12.0, 0.016, P)
    ph = 2 * math.pi * t / FLEE.long
    sw = math.cos(ph)                     # +1 when the left foot is forward
    look = bump(t, 0.08, 0.25, 0.5)       # a glance back over the left shoulder each loop
    P["chest"] = add3(P["chest"], (0.0, 0.0, 12.0 * look))
    P["neck"] = (-8.0, 4.0 * look, 22.0 * look)
    P["head"] = (-6.0, 6.0 * look, 26.0 * look)
    # the right hand trails the rifle by its middle, swinging with the stride; the left arm pumps
    rh = np.array((0.06 - 0.16 * sw, -0.22, -0.43 + 0.03 * abs(sw)))
    P["hand_r"] = {"grip": "carry", "a": (0.0, 0.25, -0.97), "n": (0.0, 1.0, 0.0), "pole": (-0.8, -0.3, -0.5)}

    def place(rig, st, rh=rh, sw=sw):
        C = rig.frame(st, "chest")
        R = R_axis((0, 0, 1), 6.0 - 4.0 * sw) @ R_axis((0, 1, 0), 2.0 + 9.0 * sw)
        g = np.array(WEAPONS["rifle"]["carry"])
        return C @ M4(R, rh - R @ g)
    P["weapon"] = place
    lh = np.array((0.18 + 0.2 * sw, 0.17, -0.2 + 0.06 * sw))
    P["hand_l"] = {"frame": "chest", "at": tuple(lh), "a": (0.6, -0.3, 0.3 + 0.4 * sw), "n": (0.0, -1.0, 0.0), "pole": (-0.6, 0.4, -0.6)}
    P["fingers_l"] = 70.0
    P["thumb_l"] = 40.0
    P["fingers_r"] = 45.0
    return P


# ---- standing, kneeling and lying to fire
HOLD_KNOB = {"grip": "knob", "a": (0.55, 0.45, -0.7), "n": (0.15, 0.85, 0.5), "pole": (-0.3, -0.9, -0.3)}


def aim_stand(t=0.0):
    """Standing to fire: bladed a third of a turn to the right, left foot forward, the rifle shouldered, the cheek
    down on the stock and the head upright again to look along the sights."""
    P = {"pelvis_at": (-0.02, 0.0, -0.03), "pelvis": (2.0, 0.0, -30.0), "spine_01": (3.0, 0.0, -2.0), "spine_02": (3.0, 0.0, -2.0),
         "chest": (4.0, -2.0, -1.0), "neck": (4.0, -3.0, 16.0), "head": (8.0, -11.0, 17.0),
         "clavicle_l": (0.0, 12.0), "clavicle_r": (8.0, 10.0),
         "foot_l": {"at": (0.2, 0.14, 0.088), "yaw": -12.0}, "foot_r": {"at": (-0.23, -0.14, 0.088), "yaw": -55.0},
         "weapon": aim(), "hand_l": held("l", pole=(0.4, 0.15, -1.0)), "hand_r": held("r", pole=(-0.2, -1.0, -0.05)),
         "fingers_l": 30.0, "fingers_r": 30.0, "thumb_l": 25.0, "thumb_r": 35.0}
    return P


def kneel_base(t=0.0, aimed=True):
    """On the right knee, sitting back toward the heel, the left foot forward with its knee up; aimed, the left
    elbow rests on the left knee."""
    P = {"pelvis_at": (-0.2, 0.0, -0.49), "pelvis": (6.0, 0.0, -24.0), "spine_01": (4.0, 0.0, -2.0), "spine_02": (5.0, 0.0, -2.0),
         "chest": (6.0, -2.0, -2.0), "neck": (2.0, -3.0, 14.0), "head": (7.0, -11.0, 14.0),
         "clavicle_l": (0.0, 14.0), "clavicle_r": (8.0, 10.0),
         "foot_l": {"at": (0.3, 0.17, 0.088), "yaw": -10.0, "pole": (1.0, 0.25, 0.3)},
         "foot_r": {"at": (-0.5, -0.2, 0.105), "yaw": -40.0, "pitch": 72.0, "toe": 62.0, "pole": (1.0, -0.2, -0.6)},
         "weapon": aim(), "hand_l": held("l", pole=(0.2, 0.2, -1.0)), "hand_r": held("r", pole=(-0.2, -1.0, -0.1)),
         "fingers_l": 30.0, "fingers_r": 30.0, "thumb_l": 25.0, "thumb_r": 35.0}
    if not aimed:
        P.update({"neck": (-2.0, 0.0, 6.0), "head": (-2.0, 0.0, 8.0), "weapon": port_arms(lift=0.05, out=0.02),
                  "hand_l": held("l"), "hand_r": held("r"), "clavicle_l": (0.0, 4.0), "clavicle_r": (0.0, 4.0),
                  "chest": (8.0, 0.0, 4.0), "spine_02": (5.0, 0.0, 4.0)})
    return P


PRONE_Z = -0.815


def prone_base(t=0.0, arch=0.5, weapon="rifle"):
    """Lying to fire: the body a little to the right of the line of fire, legs apart and toes dug in, down on the
    elbows with the back a little arched, the head up to look along the sights."""
    P = {"pelvis_at": (0.0, 0.03, PRONE_Z), "pelvis": (86.0, 2.0, -9.0), "spine_01": (-7.0 * arch, 0.0, 3.0),
         "spine_02": (-10.0 * arch, 0.0, 3.0), "chest": (-12.0 * arch, -5.0, 3.0), "neck": (-32.0, -1.0, 4.0), "head": (-22.0, -9.0, 5.0),
         "clavicle_l": (8.0, 14.0), "clavicle_r": (10.0, 10.0),
         "foot_l": {"at": (-0.82, 0.3, 0.115), "yaw": 15.0, "pitch": 82.0, "toe": 50.0, "pole": (-0.1, 0.3, -1.0)},
         "foot_r": {"at": (-0.84, -0.28, 0.115), "yaw": -25.0, "pitch": 82.0, "toe": 50.0, "pole": (-0.1, -0.3, -1.0)},
         "weapon": aim(), "weapon_kind": weapon,
         "hand_l": held("l", slide=(-0.16, 0.0, 0.0), pole_world=(0.0, 0.3, -1.0)),
         "hand_r": held("r", pole_world=(-0.1, -1.0, -1.3)),
         "fingers_l": 30.0, "fingers_r": 30.0, "thumb_l": 25.0, "thumb_r": 35.0}
    return P


def recoil(P, t, shot=0.1, kick=0.035, rise=6.0, body=1.0):
    """The shot: the rifle driven back into the shoulder and its muzzle up, the shoulder and head jolted, then
    settling back."""
    if t < shot:
        return P
    u = t - shot
    k = (1 - math.exp(-u / 0.012)) * math.exp(-u / 0.07)
    w = P["weapon"]
    P["weapon"] = _kicked(w, kick * k, rise * k)
    P["chest"] = add3(P.get("chest", (0, 0, 0)), (-2.5 * k * body, 0.0, -2.0 * k * body))
    P["clavicle_r"] = add3(P.get("clavicle_r", (0, 0)), (2.0 * k * body, -6.0 * k * body))[:2]
    P["head"] = add3(P.get("head", (0, 0, 0)), (-3.0 * k * body, 0.0, 0.0))
    return P


def _kicked(place, back, rise):
    def p(rig, st):
        M = place(rig, st) if callable(place) else place
        R = M[:3, :3]
        pivot = M[:3, 3] + R @ np.array(WEAPONS["rifle"]["butt"])
        Rk = R @ R_axis((0, 1, 0), -rise)
        o = pivot - Rk @ np.array(WEAPONS["rifle"]["butt"]) - R[:, 0] * back
        return M4(Rk, o)
    return p


def bolt_cycle(P, t, t0, t1):
    """The bolt worked: the right hand leaves the grip for the knob, lifts it, draws it back, drives it home, turns
    it down, and returns to the grip, between t0 and t1."""
    L = t1 - t0
    reach = ramp(t, t0 - 0.08, t0 + 0.0) * (1 - ramp(t, t1 - 0.07, t1))
    lift = 62.0 * (ramp(t, t0 + 0.02, t0 + 0.2 * L) - ramp(t, t0 + 0.68 * L, t0 + 0.82 * L))
    back = 0.085 * (ramp(t, t0 + 0.2 * L, t0 + 0.42 * L) - ramp(t, t0 + 0.46 * L, t0 + 0.66 * L))
    P["bolt"] = (back, lift)
    if reach > 0:
        P["hand_r"] = {"blend": [(1 - reach, P["hand_r"]), (reach, HOLD_KNOB)]}
        P["fingers_r"] = P.get("fingers_r", 30.0) * (1 - reach) + 60.0 * reach
        P["head"] = add3(P.get("head", (0, 0, 0)), (-3.0 * reach, 2.0 * reach, 0.0))
    return P


@clip("fire_stand", 1.4, loop=True, events=[ev(0.1, "muzzle_flash", "socket_muzzle", "rifle_shot"), ev(0.55, "bolt", until=0.95)])
def c_fire_stand(t):
    P = aim_stand(t)
    P = breathe(P, t, 1.4, 0.4)
    P = recoil(P, t)
    return bolt_cycle(P, t, 0.55, 0.95)


@clip("fire_kneel", 1.4, loop=True, events=[ev(0.1, "muzzle_flash", "socket_muzzle", "rifle_shot"), ev(0.55, "bolt", until=0.95)])
def c_fire_kneel(t):
    P = kneel_base(t)
    P = breathe(P, t, 1.4, 0.4)
    P = recoil(P, t)
    return bolt_cycle(P, t, 0.55, 0.95)


@clip("fire_prone", 1.4, loop=True, events=[ev(0.1, "muzzle_flash", "socket_muzzle", "rifle_shot"), ev(0.6, "bolt", until=1.0)])
def c_fire_prone(t):
    P = prone_base(t)
    P = breathe(P, t, 1.4, 0.3)
    P = recoil(P, t, body=0.6)
    return bolt_cycle(P, t, 0.6, 1.0)


@clip("kneel_down", 0.5, events=[ev(0.4, "knee_down", bone="calf_r")])
def c_kneel_down(t):
    mid = kneel_base(aimed=False)
    mid = dict(mid, pelvis_at=(-0.1, 0.0, -0.3), foot_r={"at": (-0.3, -0.18, 0.2), "yaw": -25.0, "pitch": 40.0, "toe": 20.0,
                                                          "pole": (1.0, -0.2, -0.3)})
    landed = kneel_base(aimed=False)
    landed["pelvis_at"] = (-0.19, 0.0, -0.475)
    return keyed([(0.0, base_stand()), (0.2, mid), (0.4, landed), (0.5, kneel_base())])(t)


@clip("stand_up", 0.6, events=[ev(0.12, "kit_rattle", bone="chest")])
def c_stand_up(t):
    low = kneel_base(aimed=False)
    mid = dict(kneel_base(aimed=False), pelvis_at=(-0.08, 0.0, -0.26), foot_r={"at": (-0.2, -0.16, 0.16), "yaw": -20.0,
                                                                               "pitch": 25.0, "toe": 10.0})
    return keyed([(0.0, kneel_base()), (0.15, low), (0.38, mid), (0.6, base_stand())])(t)


@clip("go_prone", 1.0, events=[ev(0.32, "knee_down", bone="calf_r"), ev(0.75, "body_fall", bone="spine_02")])
def c_go_prone(t):
    """Down on the right knee, then forward onto the elbows, the legs thrown back, the rifle kept up out of the dirt."""
    knee = kneel_base(aimed=False)
    knee.update({"pelvis_at": (-0.12, 0.0, -0.47), "spine_01": (10.0, 0, 0), "spine_02": (12.0, 0, 0), "chest": (12.0, 0, 0),
                 "foot_l": {"at": (0.32, 0.16, 0.088), "yaw": -5.0, "pole": (1.0, 0.25, 0.3)},
                 "foot_r": {"at": (-0.35, -0.18, 0.105), "yaw": -20.0, "pitch": 70.0, "toe": 55.0, "pole": (1.0, -0.2, -0.6)},
                 "weapon": port_arms(lift=0.08, out=0.12)})
    fall = {"pelvis_at": (0.0, 0.02, -0.6), "pelvis": (55.0, 0.0, -6.0), "spine_01": (4.0, 0, 0), "spine_02": (4.0, 0, 0),
            "chest": (2.0, 0, 0), "neck": (-20.0, 0, 0), "head": (-15.0, 0, 0), "clavicle_l": (4.0, 14.0), "clavicle_r": (4.0, 14.0),
            "foot_l": {"at": (-0.3, 0.2, 0.2), "yaw": 5.0, "pitch": 60.0, "toe": 30.0, "pole": (0.3, 0.3, -1.0)},
            "foot_r": {"at": (-0.5, -0.2, 0.12), "yaw": -15.0, "pitch": 75.0, "toe": 50.0, "pole": (0.3, -0.3, -1.0)},
            "weapon": port_arms(lift=0.12, out=0.3), "hand_l": held("l"), "hand_r": held("r"),
            "fingers_l": 30.0, "fingers_r": 30.0, "thumb_l": 25.0, "thumb_r": 30.0}
    hit = prone_base(arch=0.0)
    hit.update({"pelvis_at": (0.0, 0.03, PRONE_Z - 0.01), "neck": (-12.0, 0, 2.0), "head": (-10.0, -4.0, 2.0),
                "weapon": blend_places([(0.6, aim(pitch=-6.0)), (0.4, port_arms(lift=0.1, out=0.35))])})
    return keyed([(0.0, base_stand()), (0.32, knee), (0.58, fall), (0.75, hit), (1.0, prone_base())])(t)


def aim_bipod(rig, st):
    """The machine gun on its bipod: shouldered and sighted as a rifle is, then dipped or raised about the butt
    until the bipod's feet stand on the ground."""
    eye = rig.eye(st, "r")
    spec = WEAPONS["lmg"]
    R = np.eye(3)
    o = eye - np.array((-0.05, 0.0, spec["sight_z"]))
    foot_local = np.array(spec["hinge"]) + R_axis((0, 1, 0), -104.0) @ np.array((-0.31, 0.0, -0.004)) + np.array((0.0, 0.0, -0.008))
    for _ in range(3):
        fz = (o + R @ foot_local)[2]
        butt = o + R @ np.array(spec["butt"])
        arm_len = np.linalg.norm((R @ (foot_local - np.array(spec["butt"])))[[0, 2]])
        d = math.degrees(math.asin(max(min(fz / arm_len, 0.9), -0.9)))
        Rn = R_axis((0, 1, 0), d) @ R
        o = butt - Rn @ np.array(spec["butt"])
        R = Rn
    return M4(R, o)


def lmg_shake(t, t0=0.1, t1=0.8, rate=10.0):
    """The burst: a jolt each round, the gun walking a little on its bipod."""
    if t < t0 - 0.01 or t > t1 + 0.15:
        return 0.0, 0.0
    on = ramp(t, t0 - 0.01, t0 + 0.02) * (1 - ramp(t, t1, t1 + 0.12))
    ph = ((t - t0) * rate) % 1.0
    jolt = math.exp(-ph / 0.25)
    return on * jolt, on


@clip("lmg_burst", 1.2, loop=True, weapon="lmg",
      events=[ev(0.1, "muzzle_flash", "socket_muzzle", "lmg_burst")] + [ev(round(0.1 + 0.1 * k, 2), "muzzle_flash", "socket_muzzle") for k in range(1, 8)])
def c_lmg_burst(t):
    P = prone_base(arch=0.3, weapon="lmg")
    P["bipod"] = 104.0
    P["neck"] = (-28.0, -1.0, 4.0)
    P["hand_r"] = {"grip": "grip_r", "a": (0.96, 0.0, -0.28), "n": (0.0, 1.0, 0.0), "pole_world": (-0.1, -1.0, -1.2)}
    P["hand_l"] = {"grip": "grip_butt", "a": (0.65, -0.7, -0.2), "n": (0.0, 0.0, -1.0), "pole_world": (0.0, 0.6, -1.0)}
    P["fingers_l"] = 15.0
    j, on = lmg_shake(t)
    P["clavicle_r"] = add3(P["clavicle_r"], (1.5 * j, -5.0 * j))[:2]
    P["chest"] = add3(P["chest"], (-0.8 * j, 0.0, -1.0 * j))
    P["head"] = add3(P["head"], (-1.5 * j + 0.4 * on * osc(t, 0.07), 0.0, 0.3 * on * osc(t, 0.09)))

    def place(rig, st, j=j):
        M = aim_bipod(rig, st)
        R = M[:3, :3] @ R_axis((0, 1, 0), -1.2 * j) @ R_axis((0, 0, 1), 0.5 * j * osc(t, 0.23))
        return M4(R, M[:3, 3] - M[:3, 0] * 0.008 * j)
    P["weapon"] = place
    return breathe(P, t, 1.2, 0.3)


@clip("throw_grenade", 1.6, events=[ev(0.0, "grenade_throw", "socket_hand_r"), ev(0.95, "grenade_release", "socket_hand_r")])
def c_throw(t):
    """The rifle shifted to the left hand, a grenade taken from the belt, the body wound back and the arm thrown
    over the top with a step onto the left foot, then back to the ready."""
    def one_hand(lift=0.0, ahead=0.0):
        def place(rig, st):
            C = rig.frame(st, "chest")
            R = frame_from(_n((0.45 + ahead, 0.15, 0.88)), (1.0, -0.2, 0.0))
            return C @ M4(R, np.array((0.22 + ahead * 0.2, 0.2, -0.25 + lift)) - R @ np.array(WEAPONS["rifle"]["grip_l"]))
        return place
    hold_l = held("l", pole=(-0.2, 0.8, -0.6))
    stand = base_stand()
    shift = dict(base_stand(), weapon=one_hand(), hand_l=hold_l,
                 hand_r={"frame": "chest", "at": (0.12, -0.13, -0.27), "a": (0.3, 0.0, -0.95), "n": (0.0, 1.0, 0.0), "pole": (-0.4, -0.8, -0.4)},
                 fingers_r=60.0)
    wind = dict(base_stand(), weapon=one_hand(lift=0.1, ahead=0.3), hand_l=hold_l,
                pelvis=(-4.0, 0.0, -28.0), spine_01=(-4.0, 3.0, -8.0), spine_02=(-5.0, 3.0, -8.0), chest=(-6.0, 4.0, -10.0),
                neck=(0.0, 0.0, 22.0), head=(-4.0, 0.0, 24.0), clavicle_r=(14.0, -12.0), pelvis_at=(-0.06, 0.0, -0.04),
                hand_r={"at": (-0.32, -0.32, 1.66), "a": (-0.2, 0.0, 1.0), "n": (1.0, 0.0, 0.0), "pole_world": (-0.3, -1.0, -0.6)},
                fingers_r=70.0, thumb_r=50.0,
                foot_l={"at": (0.36, 0.15, 0.088), "yaw": -5.0}, foot_r={"at": (-0.2, -0.13, 0.088), "yaw": -40.0})
    step = dict(wind, foot_l={"at": (0.22, 0.15, 0.17), "yaw": 0.0, "pitch": -10.0})
    release = dict(base_stand(), weapon=one_hand(lift=0.0, ahead=-0.1), hand_l=hold_l,
                   pelvis=(10.0, 0.0, 8.0), spine_01=(8.0, -2.0, 8.0), spine_02=(8.0, -2.0, 10.0), chest=(10.0, -3.0, 12.0),
                   neck=(-8.0, 0.0, -10.0), head=(-6.0, 0.0, -8.0), clavicle_r=(16.0, 14.0), pelvis_at=(0.08, 0.0, -0.06),
                   hand_r={"at": (0.62, -0.18, 1.7), "a": (0.7, 0.0, 0.7), "n": (0.2, 0.9, -0.2), "pole_world": (-0.6, -0.6, -1.0)},
                   fingers_r=5.0, thumb_r=0.0,
                   foot_l={"at": (0.36, 0.15, 0.088), "yaw": -5.0}, foot_r={"at": (-0.2, -0.13, 0.088), "yaw": -25.0, "pitch": 30.0, "toe": 30.0})
    follow = dict(release, pelvis=(16.0, 0.0, 14.0), chest=(14.0, -3.0, 14.0), spine_02=(10.0, -2.0, 10.0),
                  hand_r={"at": (0.45, 0.12, 0.95), "a": (0.3, 0.3, -0.9), "n": (-0.3, 0.9, 0.0), "pole_world": (-0.4, -1.0, 0.2)},
                  fingers_r=20.0, neck=(-10.0, 0.0, -6.0), head=(-8.0, 0.0, -4.0))
    back = dict(base_stand(), foot_l={"at": (0.2, 0.13, 0.15), "yaw": 4.0, "pitch": -8.0})
    return keyed([(0.0, stand), (0.25, shift), (0.5, step), (0.72, wind), (0.95, release), (1.15, follow), (1.38, back),
                  (1.6, stand)])(t)


@clip("cower", 3.0, loop=True)
def c_cower(t):
    """Wavering: down on his heels, the rifle clutched upright against him, head pulled down, flinching at each
    near miss and peering up between them."""
    flinch = math.exp(-((t - 0.7) % 3.0) / 0.12) * ramp((t - 0.7) % 3.0, 0.0, 0.03) + \
        0.7 * math.exp(-((t - 2.05) % 3.0) / 0.12) * ramp((t - 2.05) % 3.0, 0.0, 0.03)
    peer = bump(t, 1.15, 1.45, 1.8)
    shiver = osc(t, 0.14) * 0.6 + osc(t, 0.23, 0.3) * 0.4
    P = {"pelvis_at": (-0.16, 0.0, -0.5 - 0.02 * flinch), "pelvis": (34.0, 0.0, 4.0), "spine_01": (10.0, 0.0, 0.0),
         "spine_02": (12.0 + 4 * flinch - 6 * peer, 0.0, 0.0), "chest": (12.0 + 5 * flinch - 6 * peer + 0.5 * shiver, 0.0, 0.0),
         "neck": (18.0 + 10 * flinch - 22 * peer, 0.0, 6.0 * peer), "head": (16.0 + 8 * flinch - 16 * peer, 2.0, 10.0 * peer),
         "clavicle_l": (10.0 + 12 * flinch, 8.0), "clavicle_r": (10.0 + 12 * flinch, 8.0),
         "foot_l": {"at": (0.1, 0.17, 0.12), "yaw": 15.0, "pitch": 28.0, "toe": 28.0, "pole": (1.0, 0.45, 0.0)},
         "foot_r": {"at": (0.06, -0.17, 0.12), "yaw": -15.0, "pitch": 28.0, "toe": 28.0, "pole": (1.0, -0.45, 0.0)},
         "fingers_l": 60.0, "fingers_r": 60.0, "thumb_l": 40.0, "thumb_r": 40.0}

    R = frame_from(_n((-0.22 - 0.05 * flinch, 0.04, 1.0)), (-1.0, 0.0, 0.0))
    butt = np.array((0.24, 0.0, 0.02))
    P["weapon"] = M4(R, butt - R @ np.array(WEAPONS["rifle"]["butt"]))
    P["hand_l"] = held("l", slide=(0.12, 0.0, 0.0), a=(0.0, -0.95, 0.0), n=(-1.0, 0.0, 0.0), pole_world=(0.0, 1.0, -0.6))
    P["hand_r"] = held("r", slide=(0.24, 0.0, 0.0), a=(0.0, 0.95, 0.0), n=(-1.0, 0.0, 0.0), pole_world=(0.0, -1.0, -0.6), grip="grip_l")
    return breathe(P, t, 1.0, 1.2)


def drop_rifle(at, yaw, side_roll=90.0, lift=0.0):
    R = R_axis((0, 0, 1), yaw) @ R_axis((1, 0, 0), side_roll)
    return M4(R, np.array(at) + np.array((0.0, 0.0, lift)))


def limp(at, a=(0.0, 0.0, -1.0), n=(0.0, 1.0, 0.0), pole=(0.0, 0.0, -1.0)):
    return {"at": at, "a": a, "n": n, "pole_world": pole}


@clip("die_back", 1.3, events=[ev(0.85, "body_fall", bone="spine_02")])
def c_die_back(t):
    stand = base_stand()
    hit = dict(base_stand(), chest=(-14.0, 0.0, 6.0), spine_02=(-8.0, 0.0, 3.0), neck=(-10.0, 0.0, 0.0), head=(-14.0, 0.0, 4.0),
               pelvis_at=(-0.02, 0.0, -0.02), clavicle_l=(12.0, -6.0), clavicle_r=(12.0, -6.0),
               weapon=port_arms(lift=0.1, out=0.08))
    stagger = {"pelvis_at": (-0.18, 0.0, -0.17), "pelvis": (-12.0, 2.0, 8.0), "spine_01": (-6.0, 0, 3.0), "spine_02": (-10.0, 0, 3.0),
               "chest": (-12.0, 2.0, 4.0), "neck": (-8.0, 0, 10.0), "head": (-10.0, 4.0, 14.0), "clavicle_l": (14.0, -8.0), "clavicle_r": (14.0, -8.0),
               "foot_l": {"at": (0.12, 0.13, 0.12), "yaw": 8.0, "pitch": -15.0}, "foot_r": {"at": (-0.32, -0.15, 0.088), "yaw": -20.0},
               "weapon": drop_rifle((0.35, -0.2, 0.55), 30.0, 60.0), "hand_l": limp((0.05, 0.42, 1.0), (0.2, 0.5, -0.8), (0.0, -1.0, 0.0), (-0.5, 0.6, -0.4)),
               "hand_r": limp((0.0, -0.42, 0.95), (0.2, -0.5, -0.8), (0.0, 1.0, 0.0), (-0.5, -0.6, -0.4)), "fingers_l": 10.0, "fingers_r": 10.0}
    falling = {"pelvis_at": (-0.42, 0.0, -0.55), "pelvis": (-50.0, 4.0, 10.0), "spine_01": (-6.0, 0, 3.0), "spine_02": (-8.0, 0, 3.0),
               "chest": (-6.0, 2.0, 4.0), "neck": (10.0, 0, 10.0), "head": (14.0, 4.0, 12.0), "clavicle_l": (16.0, -4.0), "clavicle_r": (16.0, -4.0),
               "foot_l": {"at": (0.2, 0.15, 0.2), "yaw": 10.0, "pitch": -40.0, "pole": (1.0, 0.3, 0.6)},
               "foot_r": {"at": (-0.08, -0.17, 0.1), "yaw": -20.0, "pitch": -20.0, "pole": (1.0, -0.3, 0.6)},
               "weapon": drop_rifle((0.45, -0.32, 0.04), 40.0), "hand_l": limp((-0.7, 0.55, 0.85), (-0.2, 0.6, 0.3), (0.0, 0.0, -1.0), (-0.5, 0.5, -0.8)),
               "hand_r": limp((-0.65, -0.6, 0.8), (-0.2, -0.6, 0.3), (0.0, 0.0, -1.0), (-0.5, -0.5, -0.8)), "fingers_l": 10.0, "fingers_r": 10.0}
    down = {"pelvis_at": (-0.62, 0.0, -0.8), "pelvis": (-86.0, 3.0, 12.0), "spine_01": (2.0, 0, 2.0), "spine_02": (2.0, 0, 2.0),
            "chest": (3.0, 2.0, 2.0), "neck": (12.0, 0, 8.0), "head": (10.0, 4.0, 20.0), "clavicle_l": (6.0, -4.0), "clavicle_r": (6.0, -4.0),
            "foot_l": {"at": (0.1, 0.22, 0.085), "yaw": 30.0, "pitch": -75.0, "pole": (0.0, 0.5, 1.0)},
            "foot_r": {"at": (0.07, -0.24, 0.085), "yaw": -30.0, "pitch": -70.0, "pole": (0.0, -0.5, 1.0)},
            "weapon": drop_rifle((0.45, -0.32, 0.03), 40.0),
            "hand_l": limp((-0.95, 0.6, 0.045), (-0.6, 0.6, 0.0), (0.0, 0.0, 1.0), (0.3, 0.6, -0.3)),
            "hand_r": limp((-0.85, -0.68, 0.045), (0.0, -1.0, 0.0), (0.0, 0.0, 1.0), (0.3, -0.6, -0.3)), "fingers_l": 30.0, "fingers_r": 35.0}
    bounce = dict(down, pelvis_at=(-0.62, 0.0, -0.77), chest=(-3.0, 2.0, 2.0), neck=(4.0, 0, 8.0),
                  weapon=drop_rifle((0.47, -0.33, 0.03), 41.0))
    rest = dict(down, head=(8.0, 6.0, 28.0), hand_l=limp((-0.98, 0.62, 0.045), (-0.6, 0.6, 0.0), (0.0, 0.0, 1.0), (0.3, 0.6, -0.3)),
                foot_l={"at": (-0.02, 0.24, 0.1), "yaw": 35.0, "pitch": -60.0, "pole": (0.3, 0.6, 1.0)})
    return keyed([(0.0, stand), (0.08, hit), (0.38, stagger), (0.66, falling), (0.85, down), (0.98, bounce), (1.3, rest)])(t)


@clip("die_forward", 1.1, events=[ev(0.7, "body_fall", bone="spine_02")])
def c_die_forward(t):
    stand = base_stand()
    hit = dict(base_stand(), chest=(16.0, 0.0, -4.0), spine_02=(10.0, 0.0, -2.0), neck=(10.0, 0.0, 0.0), head=(12.0, 0.0, 0.0),
               pelvis_at=(0.03, 0.0, -0.03), weapon=port_arms(lift=-0.05, out=0.05))
    buckle = {"pelvis_at": (0.05, 0.0, -0.3), "pelvis": (22.0, 0.0, -4.0), "spine_01": (10.0, 0, 0), "spine_02": (14.0, 0, 0),
              "chest": (14.0, 0.0, 0.0), "neck": (12.0, 0, 0), "head": (10.0, 0, 6.0), "clavicle_l": (-4.0, 10.0), "clavicle_r": (-4.0, 10.0),
              "foot_l": {"at": (0.08, 0.13, 0.11), "yaw": 8.0, "pitch": 25.0, "toe": 25.0, "pole": (1.0, 0.2, -0.2)},
              "foot_r": {"at": (-0.1, -0.13, 0.11), "yaw": -10.0, "pitch": 30.0, "toe": 30.0, "pole": (1.0, -0.2, -0.2)},
              "weapon": drop_rifle((0.55, -0.15, 0.5), 15.0, 50.0), "hand_l": limp((0.42, 0.25, 0.75), (0.3, 0.0, -0.9), (-0.9, 0.0, -0.2), (-0.3, 0.6, -0.6)),
              "hand_r": limp((0.4, -0.25, 0.72), (0.3, 0.0, -0.9), (-0.9, 0.0, -0.2), (-0.3, -0.6, -0.6)), "fingers_l": 15.0, "fingers_r": 15.0}
    falling = {"pelvis_at": (0.12, 0.0, -0.55), "pelvis": (62.0, 2.0, -6.0), "spine_01": (6.0, 0, 0), "spine_02": (6.0, 0, 0),
               "chest": (6.0, 0.0, 0.0), "neck": (-4.0, 0, 0), "head": (-6.0, 0, 8.0), "clavicle_l": (6.0, 14.0), "clavicle_r": (6.0, 14.0),
               "foot_l": {"at": (-0.25, 0.14, 0.11), "yaw": 8.0, "pitch": 70.0, "toe": 40.0, "pole": (0.5, 0.2, -1.0)},
               "foot_r": {"at": (-0.35, -0.14, 0.14), "yaw": -10.0, "pitch": 70.0, "toe": 40.0, "pole": (0.5, -0.2, -1.0)},
               "weapon": drop_rifle((0.85, -0.25, 0.04), 20.0), "hand_l": limp((0.75, 0.32, 0.25), (0.5, 0.2, -0.8), (-0.8, 0.0, -0.5), (-0.4, 0.6, -0.6)),
               "hand_r": limp((0.7, -0.35, 0.22), (0.5, -0.2, -0.8), (-0.8, 0.0, -0.5), (-0.4, -0.6, -0.6)), "fingers_l": 15.0, "fingers_r": 15.0}
    down = {"pelvis_at": (0.2, 0.0, -0.82), "pelvis": (88.0, 2.0, -8.0), "spine_01": (2.0, 0, 0), "spine_02": (2.0, 0, 0),
            "chest": (2.0, -2.0, 0.0), "neck": (-4.0, 0, 30.0), "head": (0.0, 10.0, 40.0), "clavicle_l": (4.0, 8.0), "clavicle_r": (4.0, 8.0),
            "foot_l": {"at": (-0.62, 0.18, 0.09), "yaw": 30.0, "pitch": 95.0, "toe": 10.0, "pole": (0.0, 0.4, -1.0)},
            "foot_r": {"at": (-0.64, -0.2, 0.09), "yaw": -30.0, "pitch": 95.0, "toe": 10.0, "pole": (0.0, -0.4, -1.0)},
            "weapon": drop_rifle((0.85, -0.25, 0.03), 20.0),
            "hand_l": limp((0.9, 0.4, 0.04), (0.6, 0.4, 0.0), (0.0, 0.0, -1.0), (0.0, 0.6, -1.0)),
            "hand_r": limp((0.25, -0.38, 0.04), (-0.3, -0.5, 0.0), (0.0, 0.0, -1.0), (0.4, -0.6, -1.0)), "fingers_l": 30.0, "fingers_r": 35.0}
    bounce = dict(down, pelvis_at=(0.2, 0.0, -0.79), chest=(-3.0, -2.0, 0.0))
    rest = dict(down, head=(0.0, 12.0, 46.0))
    return keyed([(0.0, stand), (0.1, hit), (0.38, buckle), (0.56, falling), (0.7, down), (0.82, bounce), (1.1, rest)])(t)


@clip("die_kneel", 1.5, events=[ev(0.45, "knee_down", bone="calf_l"), ev(1.0, "body_fall", bone="spine_02")])
def c_die_kneel(t):
    stand = base_stand()
    hunch = dict(base_stand(), chest=(18.0, 0.0, 0.0), spine_02=(12.0, 0.0, 0.0), spine_01=(8.0, 0.0, 0.0), neck=(10.0, 0, 0), head=(12.0, 0, 0),
                 pelvis_at=(0.0, 0.0, -0.06), weapon=drop_rifle((0.4, -0.25, 0.5), 20.0, 40.0),
                 hand_l={"frame": "chest", "at": (0.15, 0.04, -0.2), "a": (0.3, -0.9, 0.0), "n": (-1.0, 0.0, 0.0), "pole": (-0.3, 0.8, -0.5)},
                 hand_r={"frame": "chest", "at": (0.16, -0.05, -0.22), "a": (0.3, 0.9, 0.0), "n": (-1.0, 0.0, 0.0), "pole": (-0.3, -0.8, -0.5)},
                 fingers_l=40.0, fingers_r=40.0)
    knees = {"pelvis_at": (-0.12, 0.0, -0.46), "pelvis": (10.0, 0.0, 0.0), "spine_01": (10.0, 0, 0), "spine_02": (14.0, 0, 0),
             "chest": (16.0, 0.0, 0.0), "neck": (14.0, 0, 0), "head": (14.0, 0, 0), "clavicle_l": (-4.0, 10.0), "clavicle_r": (-4.0, 10.0),
             "foot_l": {"at": (-0.5, 0.15, 0.105), "yaw": 10.0, "pitch": 72.0, "toe": 60.0, "pole": (1.0, 0.1, -0.5)},
             "foot_r": {"at": (-0.5, -0.15, 0.105), "yaw": -10.0, "pitch": 72.0, "toe": 60.0, "pole": (1.0, -0.1, -0.5)},
             "weapon": drop_rifle((0.5, -0.3, 0.03), 25.0),
             "hand_l": hunch["hand_l"], "hand_r": hunch["hand_r"], "fingers_l": 40.0, "fingers_r": 40.0}
    sway = dict(knees, pelvis=(14.0, -4.0, 4.0), chest=(20.0, -4.0, 0.0), head=(20.0, -6.0, 0.0))
    down = {"pelvis_at": (0.05, 0.08, -0.8), "pelvis": (84.0, -10.0, 14.0), "spine_01": (2.0, 0, 0), "spine_02": (2.0, -2.0, 0),
            "chest": (2.0, -4.0, 0.0), "neck": (-4.0, 0, -30.0), "head": (0.0, -10.0, -40.0), "clavicle_l": (4.0, 8.0), "clavicle_r": (4.0, 8.0),
            "foot_l": {"at": (-0.78, 0.27, 0.09), "yaw": 30.0, "pitch": 95.0, "toe": 10.0, "pole": (0.0, 0.3, 1.0)},
            "foot_r": {"at": (-0.74, -0.14, 0.09), "yaw": -10.0, "pitch": 95.0, "toe": 10.0, "pole": (0.0, -0.3, 1.0)},
            "weapon": drop_rifle((0.5, -0.3, 0.03), 25.0),
            "hand_l": limp((0.55, 0.45, 0.04), (0.6, 0.4, 0.0), (0.0, 0.0, -1.0), (0.0, 0.6, -1.0)),
            "hand_r": limp((0.4, -0.15, 0.04), (0.3, 0.6, 0.0), (0.0, 0.0, -1.0), (0.4, -0.6, -1.0)), "fingers_l": 30.0, "fingers_r": 35.0}
    bounce = dict(down, pelvis_at=(0.05, 0.08, -0.77), chest=(-3.0, -4.0, 0.0))
    return keyed([(0.0, stand), (0.15, hunch), (0.45, knees), (0.75, sway), (1.0, down), (1.12, bounce), (1.5, down)])(t)


# ================================================================================================ baking the clips
def reset_pose(arm):
    for pb in arm.pose.bones:
        pb.location = (0.0, 0.0, 0.0)
        pb.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
        pb.scale = (1.0, 1.0, 1.0)


def bake_clips(m, names=None):
    """Every clip as its own action on the man's armature: the pose solved at every 60th of a second and written as
    plain keys (linear between), frames 0..N, so the last frame of a loop is its first again."""
    arm, rig = m["arm"], m["rig"]
    arm.animation_data_create()
    acts, short = {}, {}
    for name in (names or list(CLIPS)):
        c = CLIPS[name]
        act = bpy.data.actions.new(name)
        act.use_fake_user = True
        arm.animation_data.action = act
        rig.prev_q = {}
        N = int(round(c["long"] * FPS))
        worst = 0.0
        for f in range(N + 1):
            P = c["fn"](f / FPS)
            P.setdefault("weapon_kind", c["weapon"])
            st = rig.solve(P)
            worst = max([worst] + [abs(v) for v in st["short"].values()])
            rig.key(st, f)
        for fc in kit.fcurves_of(act):
            for kp in fc.keyframe_points:
                kp.interpolation = "LINEAR"
        act.use_frame_range = True
        act.frame_start, act.frame_end = 0, N
        act["loop"] = c["loop"]
        act["stride"] = c["stride"]
        act["events"] = json.dumps(c["events"], separators=(",", ":"))
        acts[name] = act
        short[name] = round(worst, 3)
    arm.animation_data.action = None
    reset_pose(arm)
    m["actions"], m["short"] = acts, short
    return acts


def play(arm, act):
    """Put a clip on an armature."""
    reset_pose(arm)
    arm.animation_data_create()
    arm.animation_data.action = act
    if hasattr(arm.animation_data, "action_slot") and act.slots:
        arm.animation_data.action_slot = act.slots[0]


def pose_once(arm, rig, P, frame=0):
    """One pose keyed on an armature of its own (for stills): a one-key action, so it holds whatever the frame."""
    if arm.animation_data:
        arm.animation_data.action = None
    rig.prev_q = {}
    rig.key(rig.solve(P), frame)


# ================================================================================================ levels of detail
LOD_TARGET = {1: 1800, 2: 600}
LOD2_LEFT_OFF = ("rifle_bolt", "bottle", "shovel", "straps", "lmg_bipod", "details")     # (too small to see from that far)


def tris_of(ob):
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


def make_lods(m, lod):
    """Thinned copies of the man's meshes for a level of detail, skinned to the same armature."""
    objs = [o for o in man_objects(m) if not (lod == 2 and o.name.split(".")[0] in LOD2_LEFT_OFF)]
    total = sum(tris_of(o) for o in objs)
    ratio = LOD_TARGET[lod] / total
    out = []
    for ob in objs:
        c = ob.copy()
        c.data = ob.data.copy()
        c.name = f"{ob.name.split('.')[0]}_LOD{lod}"
        bpy.context.scene.collection.objects.link(c)
        for md in list(c.modifiers):
            c.modifiers.remove(md)
        n = tris_of(c)
        r = min(1.0, max(ratio, 14.0 / max(n, 1)))
        if r < 0.98:
            d = c.modifiers.new("lod", "DECIMATE")
            d.ratio = r
            if ob is m["body"]:
                d.use_symmetry = True
                d.symmetry_axis = "Y"
            bpy.context.view_layer.objects.active = c
            with bpy.context.temp_override(object=c, active_object=c):
                bpy.ops.object.modifier_apply(modifier=d.name)
            c.data.validate()                             # (the thinning can leave a face doubled on itself: gone)
        clean_weights(c)
        a = c.modifiers.new("skin", "ARMATURE")
        a.object = m["arm"]
        out.append(c)
    return out


# ================================================================================================ the game's own file
def bone_list(rig):
    return [n for n in rig.names]


def json_mesh(objs, bone_index):
    """Meshes in the game's form (build_tank.gather's: points, facings, paints, triangles; y to the RIGHT), each point
    with its bone (`part`), a second bone (`part2`) and how much of the second it takes (`w2`, 0 to 0.5)."""
    pts, fac, pai, tri, part, part2, w2 = [], [], [], [], [], [], []
    seen = {}
    for ob in objs:
        me = ob.data
        Mw = ob.matrix_world.copy()
        Nw = Mw.to_3x3().inverted().transposed()
        normals = [cn.vector.copy() for cn in me.corner_normals]
        gname = {vg.index: vg.name for vg in ob.vertex_groups}
        vb = []
        for v in me.vertices:
            gs = sorted(((g.weight, gname[g.group]) for g in v.groups if gname.get(g.group) in bone_index and g.weight > 0.0), reverse=True)
            if not gs:
                vb.append((0, 0, 0.0))
                continue
            b1 = bone_index[gs[0][1]]
            if len(gs) > 1:
                vb.append((b1, bone_index[gs[1][1]], round(min(gs[1][0] / (gs[0][0] + gs[1][0]), 0.5), 3)))
            else:
                vb.append((b1, b1, 0.0))
        paint_of = [FIGURE_PAINTS.get(mt.name.split(".")[0], FIGURE_PAINTS["boots"]) for mt in me.materials]   # (eye: as boots)
        me.calc_loop_triangles()
        for lt in me.loop_triangles:
            paint = paint_of[me.polygons[lt.polygon_index].material_index] if paint_of else 0
            for li in lt.loops:
                vi = me.loops[li].vertex_index
                p = Mw @ me.vertices[vi].co
                n = (Nw @ normals[li]).normalized()
                key = (round(p.x, 4), round(-p.y, 4), round(p.z, 4), round(n.x, 3), round(-n.y, 3), round(n.z, 3), paint) + vb[vi]
                i = seen.get(key)
                if i is None:
                    i = len(pai)
                    seen[key] = i
                    pts.extend(key[0:3])
                    fac.extend(key[3:6])
                    pai.append(paint)
                    part.append(key[7])
                    part2.append(key[8])
                    w2.append(key[9])
                tri.append(i)
    return {"points": pts, "facings": fac, "paints": pai, "triangles": tri, "part": part, "part2": part2, "w2": w2}


def _flip_q(Q):
    return np.array((Q.w, -Q.x, Q.y, -Q.z))


MOVERS = ("pelvis", "weapon", "bolt")


def clips_json(rig, rate=15):
    """Every clip sampled `rate` times a second from the same solver that keyed the actions: each bone's turn in its
    parent's frame (identity at rest) as a quaternion [t, w, x, y, z] in the game's axes, and for the bones that move
    (pelvis, weapon, bolt) the shift of their head [t, x, y, z] in the parent's frame."""
    index = {b: i for i, b in enumerate(rig.names)}
    out = {}
    for name, c in CLIPS.items():
        L = c["long"]
        times = [k / rate for k in range(int(math.floor(L * rate + 1e-6)) + 1)]
        if times[-1] < L - 1e-6:
            times.append(L)
        rot = {b: [] for b in rig.names if rig.parent[b]}
        move = {b: [] for b in MOVERS}
        prev = {}
        for t in times:
            P = c["fn"](t)
            P.setdefault("weapon_kind", c["weapon"])
            st = rig.solve(P)
            for b in rot:
                p = rig.parent[b]
                q = _flip_q(Matrix((st["D"][p].T @ st["D"][b]).tolist()).to_quaternion())
                if b in prev and float(np.dot(prev[b], q)) < 0.0:
                    q = -q
                prev[b] = q
                rot[b].append([round(t, 4)] + [round(float(v), 5) for v in q])
            for b in MOVERS:
                p = rig.parent[b]
                d = st["D"][p].T @ (st["H"][b] - st["H"][p]) - (rig.H0[b] - rig.H0[p])
                move[b].append([round(t, 4), round(float(d[0]), 4), round(float(-d[1]), 4), round(float(d[2]), 4)])
        tracks = []
        for b in rig.names:
            if not rig.parent[b]:
                continue
            still = all(abs(k[1] - 1.0) < 1e-4 for k in rot[b])
            tr = {"part": index[b], "part_name": b}
            if not still:
                tr["rot"] = rot[b]
            if b in MOVERS and any(max(abs(v) for v in k[1:]) > 1e-4 for k in move[b]):
                tr["move"] = move[b]
            if len(tr) > 2:
                tracks.append(tr)
        evs = []
        for e in c["events"]:
            d = {"t": e["t"], "fx": e["fx"], "socket": e["socket"], "cue": e["cue"]}
            for k, v in e.items():
                if k not in d:
                    d[k] = v
            evs.append(d)
        out[name] = {"long": L, "loop": c["loop"], "stride": c["stride"], "weapon": c["weapon"], "tracks": tracks, "events": evs}
    return out


def socket_list(m, index):
    out = []
    for e in m["sockets"]:
        at, fc = list(e["at"]), list(e["facing"])
        out.append({"name": e.name.split(".")[0], "part": index[e["bone"]], "part_name": e["bone"],
                    "at": [round(at[0], 4), round(-at[1], 4), round(at[2], 4)],
                    "facing": [round(fc[0], 4), round(-fc[1], 4), round(fc[2], 4)]})
    return out


def write_game_json(path, m, lods):
    rig = m["rig"]
    index = {b: i for i, b in enumerate(rig.names)}
    near, mid, far = (json_mesh(lods[k], index) for k in (0, 1, 2))
    xs, zs = near["points"][0::3], near["points"][2::3]
    high = round(max(zs) - min(zs), 3)
    data = {
        "name": f"Battle rifleman, {m['nation']} (our own design)",
        "about": "Made by Tools/battle_units/soldier.py. Metres; x forward, y right, z up. Paints: "
                 + ", ".join(f"{v} {k}" for k, v in FIGURE_PAINTS.items())
                 + " (the eyes are painted as boots). near = LOD0, mid = LOD1, far = LOD2, all in the rest pose (arms 45 degrees "
                   "down). Each point rides on bone `part` and, by `w2` (0 to 0.5), on bone `part2`. Skinning: a bone's turn G = "
                   "G_parent * q (q from the clip, identity at rest; G of a bone with no track is its parent's), its head "
                   "X = X_parent + G_parent * (head - head_parent + move), and a point p on it goes to X + G * (p - head). "
                   "Quaternions are [t, w, x, y, z] in these axes (as Unreal's FQuat). Clips are in place: `stride` is the metres "
                   "one loop covers; the game moves the unit.",
        "long": round(max(xs) - min(xs), 3),
        "high": high,
        "marks": {"roof": high},
        "near": near,
        "mid": mid,
        "far": far,
        "bones": [{"name": b, "parent": index[rig.parent[b]] if rig.parent[b] else None, "parent_name": rig.parent[b],
                   "head": [round(float(rig.H0[b][0]), 4), round(float(-rig.H0[b][1]), 4), round(float(rig.H0[b][2]), 4)]}
                  for b in rig.names],
        "sockets": socket_list(m, index),
        "clips": clips_json(rig),
    }
    with open(path, "w") as f:
        json.dump(data, f, separators=(",", ":"))
    return data


# ================================================================================================ the files
STAGE = os.path.join(SCRATCH, "out")
SENT = []


def stage_path(*names):
    p = os.path.join(STAGE, *names)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    return p


def deliver(rel):
    """Copy one finished file from the stage to the shared folder (once)."""
    import shutil
    local = stage_path(rel)
    dest = os.path.join(OUT, rel)
    os.makedirs(os.path.dirname(dest) + "/", exist_ok=True)
    shutil.copyfile(local, dest)
    SENT.append((local, dest, os.path.getsize(local)))
    return dest


def check_delivered(wait=10.0):
    """Read every copied file back after a pause and compare it with the staged one."""
    import hashlib
    if not SENT:
        return True
    time.sleep(wait)
    ok = True
    for local, dest, size in SENT:
        h = lambda p: hashlib.sha1(open(p, "rb").read()).hexdigest()
        good = os.path.exists(dest) and os.path.getsize(dest) == size and h(dest) == h(local)
        note = ""
        if not good and os.path.exists(dest) and dest.endswith((".png", ".gif")):
            from PIL import Image, ImageChops            # (the share re-saves pictures: the same pixels count)
            a, b = Image.open(local), Image.open(dest)
            good = a.size == b.size and getattr(a, "n_frames", 1) == getattr(b, "n_frames", 1) and \
                ImageChops.difference(a.convert("RGB"), b.convert("RGB")).getbbox() is None
            note = " (re-saved by the share, same pixels)" if good else ""
        elif not good and os.path.exists(dest) and dest.endswith(".mp4") and os.path.getsize(dest) > 0.5 * size:
            good, note = True, f" (re-saved by the share: {os.path.getsize(dest)} bytes)"
        ok &= good
        print(f"  {'ok ' if good else 'BAD'} {dest} ({size} bytes){note}")
    SENT.clear()
    return ok


def export_glb(path, objs, anim=True):
    if anim:
        kit.export_glb(path, objs)
        return path
    kit._select_only(objs)
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True, export_animations=False, export_apply=True,
                              export_yup=True, export_skins=True, export_extras=True)
    return path


def export_fbx(path, objs):
    """As kit.export_fbx, but the meshes' armature modifier is left to the exporter (no other modifiers remain)."""
    kit._select_only(objs)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_UNITS",
                             object_types={"ARMATURE", "MESH", "EMPTY"}, use_mesh_modifiers=False, mesh_smooth_type="FACE",
                             add_leaf_bones=False, primary_bone_axis="Y", secondary_bone_axis="X", armature_nodetype="NULL",
                             bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                             bake_anim_use_all_actions=True, bake_anim_force_startend_keying=True, bake_anim_step=1.0,
                             bake_anim_simplify_factor=0.0, axis_forward="X", axis_up="Z")
    return path


def rigid_piece(ob, pivot, parent=None):
    """Make a rigid piece's origin its pivot (the mesh moved, so it stays put), and hang it under `parent`."""
    ob.data.transform(Matrix.Translation(-Vector(pivot)))
    ob.location = Vector(pivot)
    if parent is not None:
        ob.parent = parent
        ob.location = Vector(pivot)
    for g in list(ob.vertex_groups):
        ob.vertex_groups.remove(g)
    return ob


def export_props(info):
    """helmets/<kind>.glb (each one rigid, its origin at the head bone's head, x forward, z up: put it on socket_head
    with no offset) and weapons/rifle.glb, weapons/lmg.glb (origin at the weapon bone's head, the right hand's grip;
    the bolt and the bipod are child nodes turning about their own origins)."""
    J = joints()
    kit.fresh(FPS)
    LOOK_NOW.update(LOOK["GER"])
    info["helmets"] = {}
    for kind in HELMETS:
        kit.fresh(FPS)
        ob = build_helmet(kind, J)
        rigid_piece(ob, J["head"])
        ob.location = (0.0, 0.0, 0.0)
        info["helmets"][kind] = tris_of(ob)
        export_glb(stage_path("helmets", kind + ".glb"), [ob], anim=False)
    kit.fresh(FPS)
    rifle, bolt = build_rifle()
    rigid_piece(bolt, (-0.08, 0.0, 0.042), rifle)
    info["rifle"] = tris_of(rifle) + tris_of(bolt)
    export_glb(stage_path("weapons", "rifle.glb"), [rifle, bolt], anim=False)
    kit.fresh(FPS)
    gun, bipod = build_lmg()
    rigid_piece(bipod, WEAPONS["lmg"]["hinge"], gun)
    info["lmg"] = tris_of(gun) + tris_of(bipod)
    export_glb(stage_path("weapons", "lmg.glb"), [gun, bipod], anim=False)


def give_lmg(m, arm=None):
    """Swap the man's rifle for the light machine gun (on the same weapon bone, the bipod on its own bone)."""
    arm = arm or m["arm"]
    for ob in (m["weapon"], m["weapon_part"]):
        if ob and ob.name in bpy.data.objects:
            bpy.data.objects.remove(ob)
    gun, bipod = build_lmg()
    Wm = Matrix.Translation(Vector(arm.data.bones["weapon"].head_local))
    for ob, bone in ((gun, "weapon"), (bipod, "bipod")):
        ob.data.transform(Wm)
        mark_rest(ob)
        skin_rigid(ob, bone)
        attach(ob, arm)
    m["weapon"], m["weapon_part"], m["weapon_kind"] = gun, bipod, "lmg"
    return m


def export_all(info):
    """soldier_GER.glb, soldier_POL.glb (LOD0 with every clip), their LOD1 and LOD2 (.glb, the same skeleton, no clips),
    soldier.fbx (the German, LOD0, every clip a take), soldier_GER_lmg.glb (the German gunner with the LMG, every clip),
    soldier_GER.json (the game's own form), and the rigid helmets and weapons."""
    for nation in ("GER", "POL"):
        t0 = time.time()
        sc = kit.fresh(FPS)
        m = assemble(nation)
        bake_clips(m)
        socks = m["sockets"]
        lods = {0: man_objects(m), 1: make_lods(m, 1), 2: make_lods(m, 2)}
        sc.frame_set(0)
        n = {lod: sum(tris_of(o) for o in lods[lod]) for lod in lods}
        info.setdefault("tris", {})[nation] = n
        info.setdefault("parts", {})[nation] = {o.name.split(".")[0]: tris_of(o) for o in lods[0]}
        info.setdefault("short", {}).update(m["short"])
        export_glb(stage_path(f"soldier_{nation}.glb"), [m["arm"]] + lods[0] + socks)
        for lod in (1, 2):
            export_glb(stage_path(f"soldier_{nation}_LOD{lod}.glb"), [m["arm"]] + lods[lod] + socks, anim=False)
        if nation == "GER":
            export_fbx(stage_path("soldier.fbx"), [m["arm"]] + lods[0] + socks)
            data = write_game_json(stage_path("soldier_GER.json"), m, lods)
            info["json"] = {k: len(data[k]["triangles"]) // 3 for k in ("near", "mid", "far")}
            info["long"], info["high"] = data["long"], data["high"]
            info["bones"] = [(b["name"], b["parent_name"]) for b in data["bones"]]
            info["sockets"] = [(s["name"], s["part_name"], s["at"], s["facing"]) for s in data["sockets"]]
            for o in lods[1] + lods[2]:
                bpy.data.objects.remove(o)
            give_lmg(m)
            info["tris"]["GER_lmg"] = sum(tris_of(o) for o in man_objects(m))
            export_glb(stage_path("soldier_GER_lmg.glb"), [m["arm"]] + man_objects(m) + socks)
        print(f"  {nation} exported in {time.time() - t0:.0f} s: {n}")
    export_props(info)
    names = ["soldier_GER.glb", "soldier_POL.glb", "soldier_GER_LOD1.glb", "soldier_GER_LOD2.glb", "soldier_POL_LOD1.glb",
             "soldier_POL_LOD2.glb", "soldier.fbx", "soldier_GER_lmg.glb", "soldier_GER.json", "weapons/rifle.glb", "weapons/lmg.glb"]
    names += [f"helmets/{k}.glb" for k in HELMETS]
    for n_ in names:
        deliver(n_)
    with open(stage_path("export_info.json"), "w") as f:
        json.dump(info, f, indent=1)
    return info


# ================================================================================================ pictures that move
def flash_on(m, frames_on):
    """A studio muzzle flash (pictures only: the game makes its own from the events) on socket_muzzle."""
    sock = [s for s in m["sockets"] if s.name.startswith("socket_muzzle")][0]
    bpy.ops.mesh.primitive_cone_add(vertices=10, radius1=0.04, radius2=0.0, depth=0.2)
    ob = bpy.context.object
    ob.name = "studio_flash"
    mat = bpy.data.materials.new("studio_flash")
    mat.use_nodes = True
    nt = mat.node_tree
    for nd in list(nt.nodes):
        if nd.type != "OUTPUT_MATERIAL":
            nt.nodes.remove(nd)
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs["Color"].default_value = (1.0, 0.62, 0.22, 1.0)
    em.inputs["Strength"].default_value = 40.0
    nt.links.new(em.outputs["Emission"], nt.nodes["Material Output"].inputs["Surface"])
    ob.data.materials.append(mat)
    ob.parent = sock
    ob.matrix_parent_inverse = Matrix.Identity(4)
    ob.location = (0.1, 0.0, 0.0)
    ob.rotation_euler = (0.0, math.pi / 2, 0.0)
    ob.hide_render = True
    return ob


def render_sequence(seq, folder, m, ground=None, fx=None):
    """seq: [(action, frame, ground_x, time)] -> folder/0001.png ... The effects (if any) are stepped to each frame's
    time; without them a picture already made (a held frame, a repeated loop) is copied, not made again."""
    import shutil
    if os.path.isdir(folder):
        shutil.rmtree(folder)
    os.makedirs(folder)
    sc = bpy.context.scene
    last = None
    done = {}
    made = 0
    for i, (act, f, gx, tg) in enumerate(seq):
        path = os.path.join(folder, f"{i + 1:04d}.png")
        key = (act.name, f, round(gx, 5))
        if fx is None and key in done:
            shutil.copyfile(done[key], path)
            continue
        if act is not last:
            play(m["arm"], act)
            last = act
        sc.frame_set(f)
        if ground is not None:
            ground.location.x = gx
        if fx is not None:
            fx.at(tg)
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)
        done[key] = path
        made += 1
    return folder, made


def clip_frames(name, step=2, repeat=1, start_t=0.0, hold=0):
    """(clip, frame, time) for each picture frame of `repeat` plays of a clip at 60/step a second (a loop's last frame,
    its first again, is left out between plays), then `hold` more of the last pose while the time runs on."""
    c = CLIPS[name]
    N = int(round(c["long"] * FPS))
    out = []
    for r in range(repeat):
        end = N if (r == repeat - 1 and not c["loop"]) else N - 1
        for f in range(0, end + 1, step):
            out.append((name, f, start_t + (r * N + f) / FPS))
    last = out[-1]
    out += [(last[0], last[1], last[2] + (k + 1) * step / FPS) for k in range(hold)]
    return out


MOVIES = {
    # name: ([(clip, repeat, hold)], camera target, distance, pitch, yaw)
    "walk": ([("walk", 2, 0)], (0.0, 0.0, 0.85), 6.5, 20.0, -40.0),
    "run": ([("run", 3, 0)], (0.0, 0.0, 0.85), 6.5, 20.0, -40.0),
    "fire_stand": ([("fire_stand", 2, 0)], (0.1, 0.0, 0.95), 5.5, 18.0, -55.0),
    "prone": ([("go_prone", 1, 0), ("fire_prone", 2, 0)], (0.2, 0.0, 0.45), 6.0, 24.0, -50.0),
    "die_back": ([("die_back", 1, 20), ], (-0.35, 0.0, 0.6), 6.0, 20.0, -70.0),
}


def movie(m, key, ground, fast=False):
    """One clip movie. Shots get the shared Effects' flash, light and smoke at the muzzle; a knee or a body meeting
    the ground raises dust."""
    parts, target, dist, pitch, yaw = MOVIES[key]
    cam = bpy.context.scene.camera
    kit.orbit(cam, target, dist, pitch, yaw)
    rig = m["rig"]
    seq, t0 = [], 0.0
    fx = None
    for name, rep, hold in parts:
        c = CLIPS[name]
        fr = clip_frames(name, 2, rep, t0, hold)
        speed = c["stride"] / c["long"] if c["stride"] else 0.0
        marks = [e for e in c["events"] if e["fx"] in ("muzzle_flash", "body_fall", "knee_down")]
        seen = set()
        for n_, (_, f, t) in enumerate(fr):
            seq.append((m["actions"][name], f, -speed * t, t))
            tl = f / FPS
            for e in marks:
                if 0.0 <= tl - e["t"] < 2 / FPS - 1e-6 and (round(t - tl, 4), e["t"]) not in seen:
                    seen.add((round(t - tl, 4), e["t"]))
                    if fx is None:
                        fx = Effects(seed=3)
                        fx.glare = 0.4
                    P = c["fn"](tl)
                    P.setdefault("weapon_kind", c["weapon"])
                    st = rig.solve(P)
                    if e["fx"] == "muzzle_flash":
                        Mz = Matrix(st["W"].tolist()) @ Matrix.Translation(Vector(MUZZLE))
                        where = (Mz.translation.copy(), (Mz.to_3x3() @ Vector((1.0, 0.0, 0.0))).normalized())
                        fx.rifle(t, where)
                    else:
                        b = e.get("bone", "spine_02")
                        at = Vector(st["H"][b].tolist())
                        at.z = 0.0
                        fx.dust(t, at, size=0.7 if e["fx"] == "body_fall" else 0.4)
        t0 = fr[-1][2] + 2 / FPS
    if fast:
        seq = seq[::2]
    folder = os.path.join(SCRATCH, "frames", key)
    t = time.time()
    _, made = render_sequence(seq, folder, m, ground, fx)
    secs = time.time() - t
    if fx is not None:
        bpy.data.collections.remove(fx.coll)
    kit.encode(folder, stage_path("pictures", key), fps=15 if fast else 30)
    return secs, made, len(seq)


def setup_field(m, fast):
    sc = bpy.context.scene
    kit.studio()
    ground = kit.field_ground()
    cam = kit.camera(lens=50)
    shared_render(640 if fast else 960, 360 if fast else 540, 16)
    return ground, cam


def pictures_movies(info, fast=False, keys=None):
    sc = kit.fresh(FPS)
    m = assemble("GER")
    bake_clips(m, sorted({n for k in MOVIES for n, _, _ in MOVIES[k][0]}))
    ground, cam = setup_field(m, fast)
    bpy.context.scene.cycles.transparent_max_bounces = 32
    times = info.setdefault("render", {})
    for key in (keys or MOVIES):
        secs, n, total = movie(m, key, ground, fast)
        times[key] = f"{n} frames rendered ({total} shown) in {secs / 60:.1f} min ({secs / n:.1f} s a frame)"
        print(f"  movie {key}: {times[key]}")
        deliver(f"pictures/{key}.mp4")
        deliver(f"pictures/{key}.gif")


def two_men(fast=False):
    """The German and the Pole side by side at ease (for the turntable)."""
    men = []
    for nation, y in (("GER", -0.55), ("POL", 0.55)):
        m = assemble(nation)
        for mt in bpy.data.materials:                     # (so the next man gets his own colours)
            if not mt.name.startswith(("GER_", "POL_", "backdrop", "ground")):
                mt.name = nation + "_" + mt.name
        m["arm"].location = (0.0, y, 0.0)
        P = base_stand()
        P["foot_l"] = {"at": (0.12, 0.12, 0.088), "yaw": 6.0}
        P["foot_r"] = {"at": (-0.06, -0.12, 0.088), "yaw": -8.0}
        pose_once(m["arm"], m["rig"], P, 0)
        men.append(m)
    return men


def pictures_turntable(info, fast=False):
    sc = kit.fresh(FPS)
    men = two_men(fast)
    kit.studio()
    kit.backdrop("CFC6B4")
    cam = kit.camera(lens=50)
    frames = 30 if fast else 90
    kit.turntable(cam, (0.0, 0.0, 0.88), 5.4, 10.0, frames, start_yaw=20.0, frame0=1)
    shared_render(640 if fast else 960, 360 if fast else 540, 16)
    folder = os.path.join(SCRATCH, "frames", "turntable")
    import shutil
    if os.path.isdir(folder):
        shutil.rmtree(folder)
    t = time.time()
    kit.frames(folder, 1, frames)
    secs = time.time() - t
    info.setdefault("render", {})["turntable"] = f"{frames} frames in {secs / 60:.1f} min ({secs / frames:.1f} s a frame)"
    kit.encode(folder, stage_path("pictures", "turntable"), fps=15 if not fast else 5)
    deliver("pictures/turntable.mp4")
    deliver("pictures/turntable.gif")


# The squad: eleven riflemen and the gunner in a 20 x 10 m patch, each in a moment of a clip.
SQUAD = [  # x (forward), y (left), turn (degrees), clip, time
    (3.4, -7.6, 4.0, "fire_prone", 0.3),
    (4.1, -1.8, -3.0, "lmg_burst", 0.35),
    (3.0, 3.6, 7.0, "fire_prone", 0.75),
    (3.7, 8.2, -6.0, "fire_prone", 1.15),
    (0.9, -4.8, 9.0, "fire_kneel", 0.2),
    (0.4, 1.0, -7.0, "fire_kneel", 0.75),
    (1.4, 5.9, 3.0, "fire_kneel", 1.05),
    (-1.6, -8.6, 10.0, "fire_stand", 0.13),
    (-2.1, 2.9, -4.0, "throw_grenade", 0.86),
    (-3.1, -2.6, 2.0, "walk", 0.3),
    (-4.2, 7.0, 12.0, "run", 0.2),
    (-1.0, 9.3, -9.0, "fire_stand", 0.8),
]


def clone_man(m, place, turn):
    """Another man from the same meshes on an armature of his own (his own pose), at a place and a turn."""
    arm = m["arm"].copy()
    bpy.context.scene.collection.objects.link(arm)
    arm.animation_data_clear()
    arm.location = place
    arm.rotation_euler = (0.0, 0.0, math.radians(turn))
    pieces = []
    for ob in [m["body"]] + m["kit"] + [m["helmet"]]:
        c = ob.copy()
        bpy.context.scene.collection.objects.link(c)
        c.parent = arm
        c.matrix_parent_inverse = Matrix.Identity(4)
        for md in c.modifiers:
            if md.type == "ARMATURE":
                md.object = arm
        pieces.append(c)
    return arm, pieces


def pictures_squad(info, fast=False):
    sc = kit.fresh(FPS)
    m = assemble("GER")
    for ob in (m["weapon"], m["weapon_part"]):
        ob.hide_render = True
    m["arm"].location = (0.0, 0.0, -50.0)          # (the pattern man is kept out of sight)
    for x, y, turn, name, t in SQUAD:
        arm, pieces = clone_man(m, (x, y, 0.0), turn)
        mm = dict(m, arm=arm)
        if name == "lmg_burst":
            gun, bipod = build_lmg()
            Wm = Matrix.Translation(Vector(arm.data.bones["weapon"].head_local))
            for ob, bone in ((gun, "weapon"), (bipod, "bipod")):
                ob.data.transform(Wm)
                mark_rest(ob)
                skin_rigid(ob, bone)
                attach(ob, arm)
                ob.matrix_parent_inverse = Matrix.Identity(4)
        else:
            for ob in (m["weapon"], m["weapon_part"]):
                c = ob.copy()
                bpy.context.scene.collection.objects.link(c)
                c.hide_render = False
                c.parent = arm
                c.matrix_parent_inverse = Matrix.Identity(4)
                for md in c.modifiers:
                    if md.type == "ARMATURE":
                        md.object = arm
        rig = Rig(arm, m["J"])
        c = CLIPS[name]
        P = c["fn"](t)
        P.setdefault("weapon_kind", c["weapon"])
        pose_once(arm, rig, P, 0)
    sc.frame_set(0)
    kit.studio(sun_elev=34.0, sun_turn=10.0)
    kit.field_ground()
    cam = kit.camera(lens=45)
    kit.orbit(cam, (0.6, -0.9, 0.3), 25.0, 35.0, -150.0)            # (from behind the line, as the game's camera)
    shared_render(640 if fast else 1280, 360 if fast else 720, 16 if fast else 48)
    t = time.time()
    kit.still(stage_path("pictures", "squad.png"))
    info.setdefault("render", {})["squad"] = f"{time.time() - t:.0f} s"
    deliver("pictures/squad.png")


def pictures_before_after(info, fast=False):
    kit.fresh(FPS)
    m = assemble("GER")
    t = time.time()
    picture_before_after(m, stage_path("pictures", "before-after.png"), fast)
    info.setdefault("render", {})["before-after"] = f"{time.time() - t:.0f} s"
    deliver("pictures/before-after.png")


# ================================================================================================ the hero clip
HERO_SECONDS, HERO_FPS = 8.0, 30


def hero_plan(seed=11):
    """The squad's eight seconds: twelve men (the gunner in the middle) advance north in two loose rows across the
    stubble east of the road, each at his own pace and step; one by one they go to ground (three kneel) and open fire,
    each when he is ready; the gunner unfolds his bipod and fires bursts."""
    import random
    rnd = random.Random(seed)
    men = []
    for i in range(12):
        row = i % 2
        man = {"i": i, "x": 6.2 + i * 1.42 + rnd.uniform(-0.35, 0.35), "y": -25.0 - 2.4 * row + rnd.uniform(-0.7, 0.7),
               "turn": 90.0 + rnd.uniform(-6.0, 6.0), "pace": rnd.uniform(0.93, 1.07), "phase": rnd.uniform(0.0, 1.1),
               "stop": rnd.uniform(3.2, 4.7), "delay": rnd.uniform(0.15, 1.1), "rate": rnd.uniform(0.92, 1.08),
               "down": "kneel" if i in (2, 8, 11) else "prone", "weapon": "rifle"}
        men.append(man)
    g = men[5]
    g.update(weapon="lmg", pace=0.95, stop=3.6, delay=0.5, rate=1.0, down="prone")
    return men


BLEND = 0.3


def hero_pose(man, t):
    """(pose, metres walked, mode, clip time) for a man at time t."""
    walk = CLIPS["walk"]["fn"]
    v = man["pace"] * WALK.stride / WALK.long
    ts = man["stop"]
    wk = man["weapon"]
    if t < ts:
        P = walk((man["phase"] + man["pace"] * t) % WALK.long)
        P["weapon_kind"] = wk
        return P, v * t, "walk", None
    u = t - ts
    uu = min(u, BLEND)
    dist = v * ts + v * (uu - uu * uu / (2 * BLEND))
    walkP = walk((man["phase"] + man["pace"] * (ts + uu - uu * uu / (2 * BLEND))) % WALK.long)
    walkP["weapon_kind"] = wk
    down_name = "go_prone" if man["down"] == "prone" else "kneel_down"
    down = CLIPS[down_name]
    if u < down["long"]:
        D = down["fn"](u)
        D["weapon_kind"] = wk
        w = sm(u / BLEND)
        P = D if w >= 1.0 else mix([walkP, D], [1 - w, w])
        P["weapon_kind"] = wk
        return P, dist, down_name, u
    f = u - down["long"]
    if wk == "lmg":
        burst = CLIPS["lmg_burst"]["fn"]
        end = CLIPS["go_prone"]["fn"](1.0)
        end["weapon_kind"] = "lmg"
        if f < 0.4:
            P = mix([end, burst(0.0)], [1 - sm(f / 0.4), sm(f / 0.4)])
        elif f < 0.4 + man["delay"]:
            P = burst(0.0)
        else:
            c = (f - 0.4 - man["delay"]) * man["rate"]
            P = burst(c % 1.2)
            P["weapon_kind"] = "lmg"
            return P, dist, "lmg_burst", c
        P["weapon_kind"] = "lmg"
        return P, dist, "aim", None
    fire_name = "fire_prone" if man["down"] == "prone" else "fire_kneel"
    fire = CLIPS[fire_name]["fn"]
    if f < man["delay"]:
        return fire(0.0), dist, "aim", None
    c = (f - man["delay"]) * man["rate"]
    return fire(c % 1.4), dist, fire_name, c


def hero_events(man):
    """The moments of each man's shots (and rounds), his knee and his body meeting the ground."""
    ts, down = man["stop"], man["down"]
    shots, dust = [], []
    if down == "prone":
        dust += [(ts + 0.32, "knee"), (ts + 0.75, "body")]
        t0 = ts + 1.0
    else:
        dust += [(ts + 0.4, "knee")]
        t0 = ts + 0.5
    if man["weapon"] == "lmg":
        start = t0 + 0.4 + man["delay"]
        for n in range(4):
            for j in range(8):
                shots.append(start + (n * 1.2 + 0.1 + 0.1 * j) / man["rate"])
    else:
        start = t0 + man["delay"]
        for n in range(6):
            shots.append(start + (n * 1.4 + 0.1) / man["rate"])
    return [s for s in shots if s < HERO_SECONDS], dust


def _math(nt, op, a=None, b=None, clamp=False):
    n = nt.nodes.new("ShaderNodeMath")
    n.operation, n.use_clamp = op, clamp
    for k, v in enumerate((a, b)):
        if v is None:
            continue
        if isinstance(v, (int, float)):
            n.inputs[k].default_value = v
        else:
            nt.links.new(v, n.inputs[k])
    return n.outputs[0]


class Effects:
    """Soft puffs that are born, drift, grow and fade, and brief lights: the pictures' stand-ins for the game's own
    effects. Copied from the sea and air team's tested stage.py (shared/sea-air/stage.py, class Effects: puff,
    _place, once, light, at, and its puff material) so land and sea share one look, then scaled down for small arms
    below (rifle, lmg_round, dust). Call at(t) for every frame, in order."""

    def __init__(self, seed=7, wind=(0.5, 0.3, 0.0)):
        import random
        self.rng = random.Random(seed)
        self.wind = Vector(wind)
        self.glare = 1.0                         # (the muzzle light's strength: less where the camera is close)
        self.live, self.spare = [], {}
        self.shots, self.lights = [], []
        self.coll = bpy.data.collections.new("fx")
        bpy.context.scene.collection.children.link(self.coll)
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3, radius=1.0)
        ball = bpy.context.object
        self.mesh = ball.data
        for p in self.mesh.polygons:
            p.use_smooth = True
        bpy.data.objects.remove(ball)
        self.meshes = {}
        for k in ("smoke", "glow"):
            self.meshes[k] = self.mesh.copy()
            self.meshes[k].materials.append(self._mat(k))

    def _mat(self, kind):
        m = bpy.data.materials.new("fx_" + kind)
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.remove(nt.nodes["Principled BSDF"])
        out = nt.nodes["Material Output"]
        info = nt.nodes.new("ShaderNodeObjectInfo")
        tc = nt.nodes.new("ShaderNodeTexCoord")
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = 1.6
        nz.inputs["Detail"].default_value = 4.0
        nt.links.new(tc.outputs["Object"], nz.inputs["Vector"])
        lw = nt.nodes.new("ShaderNodeLayerWeight")              # thick where you look into it, nothing at its edge
        lw.inputs["Blend"].default_value = 0.5
        face = _math(nt, "SUBTRACT", 1.0, lw.outputs["Facing"], clamp=True)
        soft = _math(nt, "POWER", face, {"smoke": 2.4, "glow": 1.3}[kind])
        soft = _math(nt, "MULTIPLY", soft, _math(nt, "SUBTRACT", _math(nt, "MULTIPLY", nz.outputs[0], 1.9), 0.35, clamp=True), clamp=True)
        alpha = _math(nt, "MULTIPLY", soft, info.outputs["Alpha"], clamp=True)
        if kind == "glow":
            body = nt.nodes.new("ShaderNodeEmission")
            body.inputs["Strength"].default_value = 7.0
        else:
            body = nt.nodes.new("ShaderNodeBsdfDiffuse")
        nt.links.new(info.outputs["Color"], body.inputs["Color"])
        clear = nt.nodes.new("ShaderNodeBsdfTransparent")
        ms = nt.nodes.new("ShaderNodeMixShader")
        nt.links.new(alpha, ms.inputs[0])
        nt.links.new(clear.outputs[0], ms.inputs[1])
        nt.links.new(body.outputs[0], ms.inputs[2])
        nt.links.new(ms.outputs[0], out.inputs["Surface"])
        return m

    def puff(self, t0, at, vel=(0, 0, 0), life=6.0, r=(2.0, 8.0), col=(0.5, 0.5, 0.5), col2=None, alpha=0.6, kind="smoke", drag=0.0, rise=0.0,
             gravity=0.0, wind=1.0, squash=(1.0, 1.0, 1.0), fade_in=0.15, grow=0.6, hold=0.0, cool=1.6):
        ob = self.spare[kind].pop() if self.spare.get(kind) else None
        if ob is None:
            ob = bpy.data.objects.new("puff", self.meshes[kind])
            self.coll.objects.link(ob)
            ob.visible_shadow = False
        ob.rotation_euler = (self.rng.uniform(0, 6.28), self.rng.uniform(0, 6.28), self.rng.uniform(0, 6.28))
        ob.hide_render = False
        p = dict(ob=ob, kind=kind, t0=t0, at=Vector(at), vel=Vector(vel), life=life, r=r, col=Vector(col), col2=Vector(col2 if col2 is not None else col),
                 alpha=alpha, drag=drag, rise=rise, gravity=gravity, wind=wind, squash=Vector(squash), fade_in=fade_in, grow=grow, hold=hold, cool=cool)
        self.live.append(p)
        return p

    def _place(self, p, t):
        a = t - p["t0"]
        ob = p["ob"]
        if a < 0 or a > p["life"]:
            ob.hide_render = True
            ob.scale = (0, 0, 0)
            return a <= p["life"]
        ob.hide_render = False
        f = a / p["life"]
        k = p["drag"]
        go = p["vel"] * ((1 - math.exp(-k * a)) / k if k > 1e-6 else a)
        ob.location = p["at"] + go + self.wind * (p["wind"] * a) + Vector((0, 0, p["rise"] * a - 0.5 * p["gravity"] * a * a))
        rad = p["r"][0] + (p["r"][1] - p["r"][0]) * f ** p["grow"]
        ob.scale = (rad * p["squash"][0], rad * p["squash"][1], rad * p["squash"][2])
        fade = min(1.0, a / max(p["fade_in"], 1e-3)) * (1.0 if f < p["hold"] else (1 - (f - p["hold"]) / max(1 - p["hold"], 1e-6)) ** 1.4)
        c = p["col"].lerp(p["col2"], min(1.0, f * p["cool"]))
        ob.color = (c[0], c[1], c[2], p["alpha"] * fade)
        return True

    def once(self, t0, where, make):
        self.shots.append(dict(t=t0, where=where, make=make, done=False))

    def light(self, t0, at, power, long=0.12, col=(1.0, 0.72, 0.4), r=0.1):
        d = bpy.data.lights.new("flash", "POINT")
        d.energy, d.color, d.shadow_soft_size = 0.0, col, r
        ob = bpy.data.objects.new("flash", d)
        self.coll.objects.link(ob)
        ob.location = at
        self.lights.append(dict(ob=ob, t0=t0, long=long, power=power))

    def at(self, t):
        for s in self.shots:
            if not s["done"] and s["t"] <= t + 1e-6:
                w = s["where"]() if callable(s["where"]) else s["where"]
                s["make"](s["t"], Vector(w[0]), Vector(w[1]))
                s["done"] = True
        keep = []
        for p in self.live:
            if self._place(p, t):
                keep.append(p)
            else:
                self.spare.setdefault(p["kind"], []).append(p["ob"])
        self.live = keep
        for l in self.lights:
            a = t - l["t0"]
            l["ob"].data.energy = l["power"] * (1 - a / l["long"]) if 0 <= a < l["long"] else 0.0

    # -- the small arms' own, scaled down from the ship's gun
    def rifle(self, t0, where, size=1.0, light=True):
        """A rifle shot: a short tongue of flame, a brief light, a little grey smoke thrown out and left to drift."""
        g = self.rng

        def make(t, at, facing):
            side = facing.cross(Vector((0, 0, 1))).normalized()
            up = side.cross(facing)
            q = facing.to_track_quat("X", "Z")
            p = self.puff(t, at + facing * 0.07 * size, vel=facing * 1.5, life=0.05, r=(0.035 * size, 0.08 * size), col=(1.0, 0.75, 0.35),
                          col2=(1.0, 0.45, 0.12), alpha=1.0, kind="glow", wind=0.0, squash=(2.2, 1.0, 1.0), fade_in=0.005, grow=0.4)
            p["ob"].rotation_euler = q.to_euler()
            for i in range(4 if size >= 1.0 else 2):
                v = facing * g.uniform(2.5, 6.5) * size + side * g.uniform(-0.6, 0.6) + up * g.uniform(-0.2, 0.6)
                c = g.uniform(0.7, 0.82)
                self.puff(t + 0.01, at + facing * 0.05, vel=v, life=g.uniform(1.6, 2.6), r=(0.03 * size, g.uniform(0.22, 0.38) * size),
                          col=(0.78, 0.74, 0.68), col2=(c, c * 0.98, c * 0.95), alpha=g.uniform(0.3, 0.45) * min(1.0, size + 0.2),
                          drag=g.uniform(3.5, 4.5), rise=0.12, fade_in=0.04, grow=0.5, wind=1.0)
            if light:
                self.light(t, at + facing * 0.15 + up * 0.05, 45.0 * size * self.glare, 0.05)
        self.once(t0, where, make)

    def dust(self, t0, at, size=1.0):
        """A man goes down on the dry ground: tan dust thrown out low and settling downwind."""
        g = self.rng

        def make(t, p0, facing):
            for i in range(int(3 + 4 * size)):
                a = g.uniform(0, 2 * math.pi)
                v = Vector((math.cos(a), math.sin(a), 0.0)) * g.uniform(0.6, 1.6) * size + Vector((0, 0, g.uniform(0.15, 0.5)))
                c = g.uniform(0.6, 0.7)
                self.puff(t + g.uniform(0.0, 0.06), p0 + Vector((g.uniform(-0.25, 0.25), g.uniform(-0.25, 0.25), 0.05)) * size, vel=v,
                          life=g.uniform(1.6, 2.6), r=(0.1 * size, g.uniform(0.45, 0.75) * size), col=(0.66, 0.56, 0.42), col2=(c, c * 0.92, c * 0.8),
                          alpha=g.uniform(0.45, 0.62), drag=2.6, rise=0.12, fade_in=0.05, grow=0.45, wind=0.6, squash=(1.0, 1.0, 0.65))
        self.once(t0, (Vector(at), Vector((0, 0, 1))), make)


def pictures_hero(info, fast=False):
    """The squad advancing over the battle stage, going to ground and opening fire: 8 s at 30 frames a second.
    Returns the scene, the frame count and the effects (step them with fx.at(t) frame by frame)."""
    import battle_set
    sc = kit.fresh(HERO_FPS)
    kit.studio(sun_elev=30.0, sun_turn=-20.0)
    t0 = time.time()
    stage = battle_set.build()
    print(f"  stage built in {time.time() - t0:.0f} s")
    m = assemble("GER")
    for ob in (m["weapon"], m["weapon_part"]):
        ob.hide_render = True
    m["arm"].location = (0.0, 0.0, -50.0)
    frames = int(HERO_SECONDS * HERO_FPS)
    men = hero_plan()
    fx = Effects(seed=7)
    t0 = time.time()
    for man in men:
        yaw = math.radians(man["turn"])
        fwd = Vector((math.cos(yaw), math.sin(yaw), 0.0))
        arm, pieces = clone_man(m, (man["x"], man["y"], 0.0), man["turn"])
        man["arm"] = arm
        if man["weapon"] == "lmg":
            gun, bipod = build_lmg()
            Wm = Matrix.Translation(Vector(arm.data.bones["weapon"].head_local))
            for ob, bone in ((gun, "weapon"), (bipod, "bipod")):
                ob.data.transform(Wm)
                mark_rest(ob)
                skin_rigid(ob, bone)
                attach(ob, arm)
                ob.matrix_parent_inverse = Matrix.Identity(4)
        else:
            for ob in (m["weapon"], m["weapon_part"]):
                c = ob.copy()
                bpy.context.scene.collection.objects.link(c)
                c.hide_render = False
                c.parent = arm
                c.matrix_parent_inverse = Matrix.Identity(4)
                for md in c.modifiers:
                    if md.type == "ARMATURE":
                        md.object = arm
        rig = Rig(arm, m["J"])
        arm.animation_data_create()
        arm.animation_data.action = bpy.data.actions.new(f"hero_{man['i']}")
        shots, dusts = hero_events(man)
        shot_at = {}
        for sh in shots:
            shot_at.setdefault(int(round(sh * HERO_FPS)), []).append(sh)
        chest = {}
        for f in range(frames):
            t = f / HERO_FPS
            P, dist, mode, c = hero_pose(man, t)
            P.setdefault("weapon_kind", man["weapon"])
            st = rig.solve(P)
            rig.key(st, f)
            x, y = man["x"] + fwd.x * dist, man["y"] + fwd.y * dist
            arm.location = (x, y, stage.height(x, y))
            arm.keyframe_insert("location", frame=f)
            A = Matrix.Translation(arm.location) @ Matrix.Rotation(yaw, 4, "Z")
            chest[f] = A @ Vector(st["H"]["chest"].tolist())
            if f in shot_at:
                Mz = A @ Matrix(st["W"].tolist()) @ Matrix.Translation(Vector(MUZZLE))
                where = (Mz.translation.copy(), (Mz.to_3x3() @ Vector((1.0, 0.0, 0.0))).normalized())
                lmg = man["weapon"] == "lmg"
                for k in range(len(shot_at[f])):
                    # (each shot on the frame it falls on, so its flash is seen)
                    fx.rifle(f / HERO_FPS, where, size=0.75 if lmg else 1.0, light=True)
        for fc in kit.fcurves_of(arm.animation_data.action):
            for kp in fc.keyframe_points:
                kp.interpolation = "LINEAR"
        yaw_v = Vector((math.cos(yaw), math.sin(yaw), 0.0))
        for s_, what in dusts:
            f = min(int(round(s_ * HERO_FPS)), frames - 1)
            at = chest[f] + yaw_v * (0.05 if what == "body" else -0.35)
            at.z = stage.height(at.x, at.y)
            fx.dust(s_, at, size=1.0 if what == "body" else 0.55)
    print(f"  squad posed and keyed in {time.time() - t0:.0f} s")
    # the camera: low, behind the right of the line, moving up with the men and easing to a stop as they go down
    cam = kit.camera(lens=32)
    for f in range(frames):
        t = f / HERO_FPS
        k = sm(t / 4.6)
        eye = Vector((27.2 - 0.8 * k, -36.0 + 5.4 * k, 2.4 - 0.3 * k))
        tgt = Vector((13.6 - 0.2 * k, -21.0 + 3.6 * k, 0.55))
        kit.look_at(cam, eye, tgt)
        cam.keyframe_insert("location", frame=f)
        cam.keyframe_insert("rotation_euler", frame=f)
    sc.frame_start, sc.frame_end = 0, frames - 1
    shared_render(640 if fast else 960, 360 if fast else 540, 12 if fast else 16)
    sc.render.use_persistent_data = True
    sc.cycles.transparent_max_bounces = 48
    return sc, frames, fx


def render_hero(info, fast=False, only=None):
    """Render the hero clip frame by frame (frames already on disk are passed over, but the effects still step
    through them, since smoke remembers), then encode and deliver it."""
    sc, frames, fx = pictures_hero(info, fast)
    folder = os.path.join(SCRATCH, "frames", "hero")
    os.makedirs(folder, exist_ok=True)
    t = time.time()
    made = 0
    for f in range(frames):
        sc.frame_set(f)
        fx.at(f / HERO_FPS)
        path = os.path.join(folder, f"{f + 1:04d}.png")
        if (only is not None and f not in only) or (only is None and os.path.exists(path)):
            continue
        tick = time.time()
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)
        made += 1
        print(f"  hero frame {f + 1}/{frames}: {time.time() - tick:.1f} s", flush=True)
    secs = time.time() - t
    if only is None:
        if made:
            info.setdefault("render", {})["squad_advance"] = f"{made} frames in {secs / 60:.1f} min ({secs / made:.1f} s a frame)"
        kit.encode(folder, stage_path("pictures", "squad_advance"), fps=HERO_FPS)
        deliver("pictures/squad_advance.mp4")
        deliver("pictures/squad_advance.gif")
    return secs


# ================================================================================================ the notes
CLIP_ABOUT = {
    "idle": "standing at ease, rifle held across the body at port; breathes, shifts his weight, glances about",
    "walk": "marching pace, rifle at port",
    "run": "doubling forward, rifle at port, leaning in",
    "flee": "running away in panic: rifle carried low in the right hand, left arm pumping, a look back over the shoulder",
    "kneel_down": "from standing to kneeling on the right knee, rifle coming up to the aim",
    "stand_up": "from kneeling (aimed) back to standing at port",
    "fire_stand": "standing, aimed: shot, recoil, works the bolt, back on aim",
    "fire_kneel": "kneeling, aimed: the same",
    "go_prone": "from standing down onto the knees, the left hand, and flat on the belly, rifle to the aim",
    "fire_prone": "lying, aimed, elbows on the ground: shot, recoil, works the bolt",
    "lmg_burst": "the gunner prone behind the light machine gun on its bipod: a burst with the shoulder shaking (needs the LMG on the weapon bone)",
    "throw_grenade": "rifle to the left hand, wind-up and overarm throw from the right, recover",
    "cower": "crouched low behind the rifle held upright, head down, flinching (pinned)",
    "die_back": "hit in front: thrown back, staggers, falls on his back; lies still",
    "die_forward": "hit while advancing: buckles and falls on his face",
    "die_kneel": "hit: doubles over, sinks to his knees, falls to the side",
}


LIMITS = [
    "Hands are mitts (four fingers as one, a separate thumb), as asked: they close round the grips but have no separate fingers.",
    "Weights come from Blender's bone heat. In the deepest poses (prone, the deaths) the shoulders and hips crease and the tunic "
    "can pinch at the armpit; there is no cloth simulation.",
    "In a few transitions the two-bone IK cannot quite reach and a hand trails its target for a moment: go_prone (the right hand "
    "up to 12 cm off the grip for about 0.2 s while the rifle comes down), die_back and die_forward (about 8 cm while the arms "
    "fly), throw_grenade (6 cm). Every other clip reaches within 3 cm, most exactly.",
    "The rigid kit on the pelvis (bread bag, bottle, entrenching tool, gas-mask canister) can dip into the thighs or the ground "
    "in the prone and dying poses.",
    "The face is sculpted, not textured or animated; at the closest camera the triangle budget shows as some faceting.",
    "The folds are soft ridges sculpted into the mesh (there are no normal maps): they read close up and in raking light, "
    "faintly at the game's distance.",
    "The cloth wear in the pictures (mottling, dust on knees and boots, darker folds) is a studio material; the exports carry "
    "plain paint colours for the game to tint.",
    "lmg_burst only makes sense with the LMG (soldier_GER_lmg.glb); on a rifleman's file the rifle goes where the LMG would be.",
    "Only the German has a game JSON, as asked; the Pole's .glb has the same skeleton and clips.",
    "The hero clip's men stand on the stage's rolling ground by their root only: on a slope a foot can float or sink a "
    "centimetre or two (the long grass hides most of it).",
]


def write_readme(info):
    """README.md beside the files."""
    tr = info.get("tris", {})
    parts = info.get("parts", {})
    L_ = []
    w = L_.append
    w("# The infantry soldier\n")
    w("One man, rigged and animated for the battles' close camera, from whom the squad of twelve is made: the German rifleman "
      "(stahl helmet, tall boots, gas-mask canister) and the Polish rifleman (round helmet, ankle boots with gaiters, pack), "
      "as FIGURES has them (Rifleman_Stahl, Rifleman_Round). Our own design throughout: no other game's or maker's names, models, "
      "art or animation were used or looked at.\n")
    w("Made entirely by `Tools/battle_units/soldier.py` with its helper `sculpt.py` (distance-field sculpting and meshing) and "
      "`kit.py` (scene, camera, export helpers). Run it with the `bpy` module (Blender 5.0.1): `python soldier.py` does everything; "
      "`python soldier.py --only export` remakes just the files; steps are export, before_after, hero, turntable, movies, squad, "
      "readme, and `--fast` makes small test pictures. The export takes about three minutes; the pictures some hours on two "
      "cores (times below). The hero clip resumes where it stopped if it is run again.\n")
    w("## Files\n")
    w("| file | what |")
    w("|---|---|")
    w("| `soldier_GER.glb` | the German: body, kit, stahl helmet, rifle; skinned to the one skeleton; all 16 clips as separate animations; sockets |")
    w("| `soldier_POL.glb` | the Pole: the same skeleton and clips, round helmet, ankle boots and gaiters, pack |")
    w("| `soldier_GER_lmg.glb` | the German gunner: as soldier_GER but carrying the light machine gun (bipod on its own bone); all clips |")
    w("| `soldier.fbx` | the German (LOD0), every clip a take, for Unreal's classic FBX importer |")
    w("| `soldier_GER_LOD1.glb`, `soldier_GER_LOD2.glb`, `soldier_POL_LOD1.glb`, `soldier_POL_LOD2.glb` | the lower levels of detail, same skeleton, no clips |")
    w("| `soldier_GER.json` | the German in the game's own model format (near = LOD0, mid = LOD1, far = LOD2) with bones, sockets and clips |")
    w("| `helmets/<kind>.glb` | the twelve helmets, rigid: " + ", ".join(HELMETS) + " |")
    w("| `weapons/rifle.glb`, `weapons/lmg.glb` | the bolt-action rifle and the light machine gun, rigid, the bolt and the bipod as child nodes |")
    w("| `pictures/` | before-after.png, squad_advance (the hero clip), turntable, five clip movies, squad.png (below) |\n")
    w("## What he is\n")
    w("- **Body**: one continuous skinned mesh, 1.75 m tall, about 7.5 heads. Sculpted as distance fields: skull, jaw, a brow "
      "ridge over eye sockets deep enough to hold shadow, dark eyes, cheekbones with hollows under them, nose, lips, ears with "
      "a rim and a bowl; hands as mitts with a separate thumb; the tunic (a stand collar with dark collar tabs, a button strip "
      "with five buttons, four patch pockets with buttoned flaps, a skirt over the hips, shoulder straps, turned-back cuffs), "
      "trousers, and boots on proud soles with heels (tall marching boots for the German; ankle boots with canvas gaiters and "
      "their straps for the Pole). Each cloth is a layer joined hard to the next, so the collar, cuffs, hems, soles and boot "
      "tops are real edges. Folds are sculpted into the mesh: rings of gathered cloth at the elbows and knees, the tunic "
      "bunched above and below the belt (more at the back), wrinkles across the shoulder blades, the trousers bloused over the "
      "boot tops. The head and the hands are their own islands of the same mesh, tucked into the collar and cuffs.")
    w("- **Kit** (separate meshes on the same skeleton): belt with buckle (skinned like the body), three-cell ammunition pouches "
      "either side of the buckle, Y-straps, bread bag, water bottle with cup, entrenching tool, and the gas-mask canister "
      "(German) or the pack with a blanket roll on top (Polish); the bag, bottle, tool, canister ride rigidly on the pelvis and "
      "the pack on the chest.")
    w("- **Helmets**: twelve kinds as rigid shells (outside, rolled rim, inside, chin strap): stahl, round and dome first, then "
      "brodie, pot, soviet, adrian, eastern, cap, peaked, sun and fieldcap. Each one's origin is the head bone's head with "
      "x forward and z up, so it sits on `socket_head` with no offset.")
    w("- **Weapons**: a generic bolt-action rifle of our own (1.10 m: one-piece stock with half-pistol grip and steel butt plate, "
      "receiver, bolt with a turned-down handle, two barrel bands, hooded front sight, ramp rear sight, trigger guard, leather "
      "sling) and a generic light machine gun (a perforated barrel jacket, a side box magazine on the left, a pistol grip, a "
      "carry handle, a folding bipod). Origin at the right hand's grip; both ride on the `weapon` bone (the bolt and the "
      "bipod on their own bones under it).\n")
    w("## Triangles\n")
    w("| | LOD0 | LOD1 | LOD2 |")
    w("|---|---|---|---|")
    for nation in ("GER", "POL"):
        if nation in tr:
            t_ = tr[nation]
            w(f"| {nation} rifleman (man + kit + helmet + rifle) | {t_['0']:,} | {t_['1']:,} | {t_['2']:,} |" if '0' in t_ else
              f"| {nation} rifleman (man + kit + helmet + rifle) | {t_[0]:,} | {t_[1]:,} | {t_[2]:,} |")
    if "GER_lmg" in tr:
        w(f"| GER gunner (with the LMG instead) | {tr['GER_lmg']:,} | | |")
    w("\nBudgets: LOD0 about 9,000 (raised from 6,000 for the folds and the face, as the art lead allowed), LOD1 about 1,800, "
      "LOD2 about 600. LOD1 and LOD2 are the LOD0 meshes thinned; LOD2 leaves off the bolt, buttons and tabs, water bottle, "
      "entrenching tool and Y-straps.\n")
    if "GER" in parts:
        w("LOD0 by piece (German): " + ", ".join(f"{k} {v}" for k, v in parts["GER"].items()) + ".")
    if "POL" in parts:
        w("LOD0 by piece (Polish): " + ", ".join(f"{k.split('.')[0]} {v}" for k, v in parts["POL"].items()) + ".")
    if "helmets" in info:
        w("Helmets: " + ", ".join(f"{k} {v}" for k, v in info["helmets"].items()) + ". Rifle " + str(info.get("rifle")) +
          ", LMG " + str(info.get("lmg")) + " (as separate files).\n")
    w("## Materials\n")
    w("Named by paint, as build_tank.py's FIGURE_PAINTS (the game colours them from the unit table): `tunic, trousers, boots, "
      "lower, straps, helmet, pack, skin, wood, steel`, and `eye` (in soldier_GER.json the eyes are painted as `boots`). The "
      "exports carry plain studio colours; the cloth mottling in the pictures is a studio material only.\n")
    w("## Axes and units\n")
    w("Metres; the man faces +X, Z up, feet on z = 0, between his feet at x = y = 0. Blender, the .glb and the .fbx have y to "
      "his LEFT; soldier_GER.json has y to his RIGHT (as build_tank.py writes). All clips are in place.\n")
    w("## Skeleton\n")
    w("One skeleton for every man, nation and weapon (identical names, hierarchy and rest pose in every file). Rest pose: "
      "standing, arms 45 degrees down. Heads below are in the JSON's axes (y right).\n")
    w("| bone | parent | | bone | parent |")
    w("|---|---|---|---|---|")
    bones = info.get("bones", [])
    half = (len(bones) + 1) // 2
    for i in range(half):
        a = bones[i]
        b = bones[i + half] if i + half < len(bones) else None
        w(f"| `{a[0]}` | {a[1] or '-'} | | " + (f"`{b[0]}` | {b[1] or '-'} |" if b else " | |"))
    w("\n`root` stays at the origin (no root motion). `pelvis` moves (its offset is keyed). `fingers_l/r` close the mitt and "
      "`thumb_l/r` the thumb. `weapon` (child of root) carries the gun and is placed by the clips; `bolt` slides and lifts "
      "on it (the rifle's bolt) and `bipod` folds (the LMG's legs). Only the deforming bones carry body weight; the "
      "weapon, bolt and bipod carry only the gun. Every point has at most four bones.\n")
    w("## Sockets\n")
    w("Empties named `socket_<name>`, parented to bones; each one's +X points along its facing. In the .fbx they arrive as small "
      "extra bones of the same names: add Unreal sockets of the same names there.\n")
    w("| socket | bone | at rest (x, y right, z) | facing | use |")
    w("|---|---|---|---|---|")
    use = {"socket_muzzle": "muzzle flash, tracer start", "socket_hand_r": "grenade release, things put in the hand",
           "socket_head": "the helmet (no offset), voice", "socket_chest": "hit effects, the unit's marker",
           "socket_weapon": "a weapon mesh (no offset): rifle.glb or lmg.glb"}
    for name, bone, at, fc in info.get("sockets", []):
        w(f"| `{name}` | {bone} | {', '.join(f'{v + 0.0:g}' for v in at)} | {', '.join(f'{v + 0.0:g}' for v in fc)} | {use.get(name, '')} |")
    w("\n## Clips\n")
    w("Sixteen, each its own action (glb: separate animations; fbx: separate takes; json: `clips`). Made at 60 frames a second "
      "(frames 0..N, so a loop's last frame is its first again); soldier_GER.json samples them 15 times a second. Posed by the "
      "script: feet planted and hands put on the weapon by two-bone IK solved in soldier.py, then baked to plain keys (no "
      "constraints in the files).\n")
    w("| clip | seconds | loops | stride | what | events (t: fx @ socket, cue) |")
    w("|---|---|---|---|---|---|")
    for name, c in CLIPS.items():
        evs = []
        for e in c["events"]:
            s_ = f"{e['t']:g}"
            if e.get("until") is not None:
                s_ += f"-{e['until']:g}"
            s_ += f": {e['fx']}"
            if e.get("socket"):
                s_ += f" @ {e['socket']}"
            elif e.get("bone"):
                s_ += f" @ bone {e['bone']}"
            if e.get("cue"):
                s_ += f", **{e['cue']}**"
            if e.get("first_loop_only"):
                s_ += " (first loop only)"
            evs.append(s_)
        if name == "lmg_burst":
            evs = ["0.1: muzzle_flash @ socket_muzzle, **lmg_burst**; then a muzzle_flash every 0.1 s to 0.8 (cue null)"]
        stride = f"{c['stride']:g} m ({c['stride'] / c['long']:.2f} m/s)" if c["stride"] else "-"
        w(f"| `{name}` | {c['long']:g} | {'yes' if c['loop'] else 'no'} | {stride} | {CLIP_ABOUT.get(name, '')} | {'; '.join(evs) or '-'} |")
    w("\n- **Strides**: one walk loop (two steps) covers **1.5 m in 1.1 s** (1.36 m/s, 0.75 m a step); one run loop **2.45 m in "
      "0.7 s** (3.5 m/s); one flee loop **2.8 m in 0.65 s** (4.31 m/s). Play rate = the unit's speed / those speeds, so the "
      "planted foot keeps still on the ground (it moves back at exactly stride / length while planted).")
    w("- **Events and cues** (the sound director's names; no sound files ship): shots `rifle_shot` (the game may swap in "
      "a per-army shot) and `lmg_burst` (the first round of each burst; the other rounds are flashes with cue null); "
      "`footstep` (the game picks footstep_01 to _04), `knee_down`, `body_fall` (01 to 04), `bolt` while the bolt is worked "
      "(from `t` to `until`; the game picks the nation's own, bolt_k98 for Germany), `kit_rattle` (01 or 02) as he stands up "
      "and on the first step of a run (`first_loop_only`), `grenade_throw` at the start of the throw (pin and swish; the "
      "blast is the game's). `grenade_release` (0.95) has no cue: it is where the game lets the grenade go. Each event names "
      "its socket or its bone. The events are in soldier_GER.json's clips and on every animation of the .glb files as "
      "extras (`events`, a JSON string), with `loop` and `stride`.")
    w("- Firing loops end where they start, so a man can fire round after round; go_prone ends on fire_prone's first frame, "
      "kneel_down on fire_kneel's, and stand_up starts on fire_kneel's. The deaths end lying still: hold the last frame.")
    w("- lmg_burst is made for the gunner (soldier_GER_lmg.glb): the weapon bone sits the gun on its bipod and the bipod bone "
      "unfolds it. On a rifleman it would put his rifle where the gun's should be.\n")
    w("## What the game plays when\n")
    w("The battle's block of men has an act (idle, march, run, fight, rout, down), a state (steady, wavering, routing) and its "
      "cover. Suggested mapping, each man with his own small random delay and play-rate jitter (0.95 to 1.05) so the twelve "
      "never move as one:\n")
    w("| act | steady | wavering | routing |")
    w("|---|---|---|---|")
    w("| idle | idle | idle; under fire, cower | flee |")
    w("| march | walk | walk | flee |")
    w("| run | run | run | flee |")
    w("| fight, open ground | fire_stand (a few kneel: kneel_down, fire_kneel) | go_prone, fire_prone | flee |")
    w("| fight, in woods or buildings | kneel_down, fire_kneel | fire_kneel; go_prone, fire_prone at the edge | flee |")
    w("| fight, within 30 m of the enemy | throw_grenade now and then, then back to firing | the same, less often | flee |")
    w("| rout | flee | flee | flee |")
    w("| down (a man lost) | die_back if standing and hit from the front; die_forward if moving; die_kneel if kneeling | the same | die_forward |")
    w("\n- The LMG gunner plays go_prone then lmg_burst whenever his block fights; he walks, runs, flees and dies as the others.")
    w("- From prone or kneeling to moving: stand_up (from kneeling; from prone play go_prone backwards, 1.0 s) and then walk or run.")
    w("- Pinned (heavy fire, wavering, no cover): cower, a few men at a time.\n")
    w("## How to import into Unreal 5\n")
    w("- **One shared skeleton**: import `soldier_GER.glb` first (Interchange glTF importer: Skeletal Mesh, Import Animations on). "
      "It makes the skeleton and the sixteen animation sequences. Import `soldier_POL.glb` and `soldier_GER_lmg.glb` choosing that "
      "same skeleton (their bones, hierarchy and rest pose are identical), and skip their animations (the clips are the same).")
    w("- **soldier.fbx** (classic importer): Skeletal Mesh, Import Animations on, *Use T0 As Reference Pose* off, import scale "
      "1.0 (the file carries its units: 1 m arrives as 100 units), X forward, Z up, primary bone axis Y.")
    w("- **LODs**: Skeletal Mesh editor, LOD Settings, *Import LOD* 1 and 2 from the _LOD1/_LOD2 files. Suggested screen sizes "
      "1.0 / 0.3 / 0.1.")
    w("- **Helmets**: import the `helmets/*.glb` as Static Meshes and attach one to `socket_head` with no offset to change "
      "a man's helmet (hide the skinned helmet section first). **Weapons** likewise on `socket_weapon`; the skinned guns in "
      "the soldier files are better while the clips play (their bolt and bipod bones move).")
    w("- **soldier_GER.json**: the game's reader loads `near` and `far` as today; the new keys (`mid`, per-point `part`, "
      "`part2`, `w2`, `bones`, `sockets`, `clips`) are ignored by the old reader. Its `about` gives the skinning formula. Its "
      "clips were checked against the glb: bone heads skinned from the JSON alone land within 0.1 mm of Blender's.\n")
    w("## The pictures\n")
    w("- `pictures/before-after.png` (1280x720, 48 samples): today's lay figure (Rifleman_Stahl from the game's file, painted "
      "by number) beside the new German, the same camera and sun.")
    w("- `pictures/turntable.mp4/.gif`: the German and the Pole side by side, a full turn in 6 s at 15 frames a second "
      "(960x540, 16 samples, denoised).")
    w("- `pictures/walk`, `run`, `fire_stand`, `prone` (go_prone then fire_prone twice), `die_back` (.mp4 and .gif, 30 frames "
      "a second, 960x540, 16 samples): a close game camera 5.5 to 6.5 m away, 18 to 24 degrees down, on the field ground. "
      "The clips are in place, so in walk and run the ground slides back under him at the clip's own speed (the planted foot "
      "should stay put on it). The muzzle flash, light and smoke in fire_stand and prone, and the dust where he goes "
      "down, are the shared Effects (below), studio stand-ins only; the game makes its own from the events.")
    w("- `pictures/squad_advance.mp4/.gif` (the hero clip, 8 s, 960x540, 30 frames a second, 16 samples): the squad of twelve "
      "(the gunner in the middle) advances north across the stubble east of the road on the battle stage (battle_set.py), "
      "each man at his own pace and step; one by one they go to ground (three kneel) and open fire, each when he is ready, and "
      "the gunner unfolds his bipod and fires bursts. A low game camera behind the right of the line moves up with them. "
      "Muzzle flashes (a short flame and a brief light), small grey smoke puffs and tan dust where men drop are the sea and "
      "air team's tested Effects (shared/sea-air/stage.py), copied into soldier.py and scaled down for small arms; they are "
      "the pictures' stand-ins, the game makes its own from the events.")
    w("- `pictures/squad.png` (1280x720, 48 samples): twelve men (eleven riflemen and the gunner) in a 20 x 10 m patch, each "
      "frozen in a moment of a clip (prone and kneeling fire, standing fire, a grenade throw, the gunner, two moving up), "
      "from 25 m behind the line, 35 degrees down.")
    rt = info.get("render", {})
    if rt:
        w("\nRender times (Cycles, CPU, two threads): " + "; ".join(f"{k} {v}" for k, v in rt.items()) + ".")
    w("\n## Known limits\n")
    for line in LIMITS:
        w("- " + line)
    path = stage_path("README.md")
    with open(path, "w") as f:
        f.write("\n".join(L_) + "\n")
    deliver("README.md")
    return path


# ================================================================================================ run
STEPS = ["export", "before_after", "hero", "turntable", "movies", "squad", "readme"]


def load_info():
    p = stage_path("export_info.json")
    return json.load(open(p)) if os.path.exists(p) else {}


def save_info(info):
    with open(stage_path("export_info.json"), "w") as f:
        json.dump(info, f, indent=1)


def main():
    a = kit.args()
    fast = "--fast" in a
    steps = a[a.index("--only") + 1].split(",") if "--only" in a else STEPS
    movies = a[a.index("--movies") + 1].split(",") if "--movies" in a else None
    info = load_info()
    log = {}
    for step in steps:
        t0 = time.time()
        if step == "export":
            export_all(info)
        elif step == "before_after":
            pictures_before_after(info, fast)
        elif step == "turntable":
            pictures_turntable(info, fast)
        elif step == "movies":
            pictures_movies(info, fast, movies)
        elif step == "squad":
            pictures_squad(info, fast)
        elif step == "hero":
            render_hero(info, fast)
        elif step == "readme":
            write_readme(info)
        save_info(info)
        log[step] = round(time.time() - t0, 1)
        print(f"step {step}: {log[step]} s")
    ok = check_delivered()
    print("delivered" if ok else "SOME FILES DID NOT LAND", log)


if __name__ == "__main__":
    main()
