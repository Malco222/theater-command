"""The infantry soldier of the real-time battles: one man, rigged and animated, from whom the squad of twelve is made.

Run without Blender's window (Python 3.11 with the `bpy` module, Blender 5):

    python soldier.py                     everything: build, rig, animate, export, pictures
    python soldier.py --only export,movies (steps: export, before_after, hero, turntable, movies, squad, readme;
                                           --fast for small test pictures, --movies walk,prone for some movies)

Nothing is drawn by hand and nothing is fetched. The man is MakeHuman's CC0 base man (human.py): his head, neck and
hands are MakeHuman's mesh, and his uniform is made from MakeHuman's clothing shells read as distance fields
(sculpt.py), each cloth a layer joined hard to the next so that hems, cuffs and boot tops are true edges (the tunic with
its collar, four pockets and skirt; the trousers; tall boots, or ankle boots with gaiters), meshed and thinned to the
triangle budget. His kit (belt and pouches, Y-straps, bread bag, water bottle, entrenching tool, gas-mask canister or
pack), the helmets (twelve kinds, rigid) and the weapons (a bolt-action rifle and a light machine gun, our own designs)
are their own meshes on the same skeleton.

The body's movement in the locomotion, going prone, the falls and the crawl is motion capture: mocap.py's clips (CMU
takes retargeted to human.py's rig, in mocap_cmu/clips.json) carried onto our rig by world turns, the rifle held over
them by mocap.carry_rifle. The weapon work (firing, the bolt, the grenade, kneeling) is posed by this script: the feet
are planted and the hands put on the weapon by two-bone IK solved here. Every frame is written as plain bone keys (no
constraints are left in the file), so what Unreal reads is what the pictures show. Clips are in place: the moving
ones state their stride.

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
import mocap  # noqa: E402
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
    # Japan: the field cap with its sun flaps, ankle boots under puttees to below the knee, one big ammunition pouch
    # either side of the buckle and one behind, no Y-straps: the haversack and water bottle hang on their own slings
    # crossed over the chest; the pack.
    "JAP": {"helmet": "cap", "tall_boots": False, "pack": True, "roll": False, "canister": False, "chest_pouches": False,
            "puttees": {"top": None, "boot": "ankle"}, "y_straps": False, "pouches": {"front": 1, "rear": True},
            "cross_straps": True},
    # China (the Central Army): the soft field cap, cloth shoes under puttees, no belt pouches but a cloth bandolier
    # over the left shoulder, a bag of stick grenades at the left hip, the blanket roll over the right shoulder.
    "CHN": {"helmet": "fieldcap", "tall_boots": False, "pack": False, "roll": True, "canister": False, "chest_pouches": False,
            "puttees": {"top": None, "boot": "shoe"}, "y_straps": False, "pouches": {"front": 0, "rear": False},
            "bandolier": {"shoulder": "left", "pockets": 10}, "grenade_bag": True},
}

# The shared options any nation's kit may carry (each optional; a nation without them is drawn as before):
#   "puttees": {"top": metres of wrap above the ankle joint, or None for just under the knee; "boot": "ankle" (leather
#              ankle boots) or "shoe" (low cloth shoes); "colour": hex for the wrap, else LOOK's "lower"; "turns": how
#              many turns of the spiral are pressed in as grooves (default 0: none; each costs triangles the tunic and
#              the far man's face would have had, and they do not show at battle range)}. Painted "lower", the gaiters' paint, so the game colours it the same way.
#   "bandolier": {"shoulder": "left" or "right" (the one it hangs from); "pockets": cells across the chest; "paint"}.
#   "y_straps": False drops the Y-straps; "pouches": {"front": 3, 1 or 0 cells each side of the buckle, "rear": True
#              for one behind}; "cross_straps": True hangs the haversack and bottle on slings crossed over the chest;
#   "grenade_bag": True, a cloth bag of stick grenades at the left hip.

# Studio colours for the pictures only (the game paints by number from its unit table).
LOOK = {
    "GER": {"tunic": "5F665C", "trousers": "585E57", "boots": "2A2420", "lower": "2C2824", "straps": "2E2620", "helmet": "4F5650",
            "pack": "6E6852", "skin": "BDA08E", "wood": "6B4429", "steel": "34363A", "eye": "2A2420", "leather": "3A2E25", "blanket": "564A3E"},
    "POL": {"tunic": "7C7450", "trousers": "746C4A", "boots": "3A2A1E", "lower": "5E4A34", "straps": "4A3624", "helmet": "5E6044",
            "pack": "7A7254", "skin": "BDA08E", "wood": "6B4429", "steel": "34363A", "eye": "2A2420", "leather": "3A2E25",
            "blanket": "5C5040"},
    "JAP": {"tunic": "A08856", "trousers": "9A8452", "boots": "4A3626", "lower": "948052", "straps": "5A4632", "helmet": "9C8656",
            "pack": "7E7050", "skin": "C2A07E", "wood": "6B4429", "steel": "34363A", "eye": "2A2420", "leather": "4A3626",
            "blanket": "6A5E48"},
    "CHN": {"tunic": "7F8A96", "trousers": "7A8590", "boots": "2E2C2A", "lower": "7A8490", "straps": "6E6450", "helmet": "727E8A",
            "pack": "8A8070", "skin": "C2A07E", "wood": "6B4429", "steel": "34363A", "eye": "2A2420", "leather": "3A2E25",
            "blanket": "6A6A5E"},
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


def mesh_from_layers(name, layers, h, target_tris, smooth_steps=1, sym=False, cut_paints=False, detail=0.0):
    """Sample the layers on a grid, mesh the surface, thin it to about `target_tris`, and paint each face with
    the layer it lies on (with `cut_paints`, the faces are first cut along where one paint meets another, so a hem
    or a boot's top is a clean line, not the thinned faces' teeth). With `detail`, the thinning spares the sharply
    curved places (hems, flaps, folds, seams) by that much and takes its faces from the broad ones. Returns the Blender
    object."""
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
        if detail > 0.0:
            # how sharply the surface turns at each point (its normal against its neighbours'), spread a ring
            e = 1e-4
            gr = np.stack([(field(V + np.eye(3)[i] * e) - field(V - np.eye(3)[i] * e)) / (2 * e) for i in range(3)], axis=1)
            Nv = gr / np.maximum(np.linalg.norm(gr, axis=1), 1e-9)[:, None]
            turn = 1.0 - (Nv[E[:, 0]] * Nv[E[:, 1]]).sum(axis=1)
            k = np.zeros(len(V))
            np.maximum.at(k, E[:, 0], turn)
            np.maximum.at(k, E[:, 1], turn)
            k2 = k.copy()
            np.maximum.at(k2, E[:, 0], k[E[:, 1]])
            np.maximum.at(k2, E[:, 1], k[E[:, 0]])
            sharp = np.clip((k2 - 0.003) / 0.03, 0.0, 1.0)
            vg = ob.vertex_groups.new(name="spare")        # (a point out of the group would never be thinned at all)
            for i in range(len(V)):
                vg.add([i], float(1.0 - 0.95 * sharp[i]), "REPLACE")
            mod.vertex_group = "spare"
            mod.vertex_group_factor = detail
        bpy.context.view_layer.objects.active = ob
        with bpy.context.temp_override(object=ob, active_object=ob):
            bpy.ops.object.modifier_apply(modifier=mod.name)
        for vg_ in list(ob.vertex_groups):
            ob.vertex_groups.remove(vg_)
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
    if cut_paints:
        cut_along_paints(ob, layers)
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


def cut_along_paints(ob, layers):
    """Cut a mesh's faces along the lines where its layers' paints meet (where one paint's nearest layer is as near as
    the nearest of the others), so each face afterwards lies all in one paint."""
    paints = []
    for L in layers:
        if L.paint not in paints:
            paints.append(L.paint)
    if len(paints) < 2:
        return
    of = [[i for i, L in enumerate(layers) if L.paint == p] for p in paints]
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    added = 0
    for k in range(len(paints) - 1):
        bm.verts.ensure_lookup_table()
        bm.verts.index_update()
        V = np.array([v.co for v in bm.verts])
        D = np.stack([L.at_points(V) for L in layers], axis=1)
        Dp = np.stack([D[:, ix].min(axis=1) for ix in of], axis=1)
        f = Dp[:, k] - np.delete(Dp, k, axis=1).min(axis=1)
        on = set()
        for e in list(bm.edges):
            a, b = e.verts
            fa, fb = f[a.index], f[b.index]
            if fa * fb >= 0.0:
                continue
            t = fa / (fa - fb)
            if t < 0.2:
                on.add(a)
            elif t > 0.8:
                on.add(b)
            else:
                ne, nv = bmesh.utils.edge_split(e, a, t)
                on.add(nv)
                added += 1
        for face in list(bm.faces):
            vs = [v for v in face.verts if v in on]
            if len(vs) == 2 and not any(set(ed.verts) == set(vs) for ed in face.edges):
                try:
                    bmesh.utils.face_split(face, vs[0], vs[1])
                except ValueError:
                    pass
        bmesh.ops.triangulate(bm, faces=[fc for fc in bm.faces if len(fc.verts) > 3])
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.update()
    print(f"    {ob.name}: cut along its paints ({added} points added)")


LOOK_NOW = dict(LOOK["GER"])


def paint_material(paint):
    """The material for a paint, named by the paint (the game colours it); the studio colour is for the pictures."""
    if paint == "eye":
        return eye_paint()
    rough = {"steel": 0.45, "boots": 0.42, "straps": 0.55, "helmet": 0.6, "skin": 0.5, "wood": 0.5, "eye": 0.3, "leather": 0.5}.get(paint, 0.85)
    metal = 0.6 if paint == "steel" else 0.0
    noise = {"tunic": 0.18, "trousers": 0.18, "pack": 0.2, "lower": 0.15, "helmet": 0.12, "wood": 0.25, "skin": 0.06,
             "blanket": 0.22}.get(paint, 0.05)
    sheen = 0.4 if paint in ("tunic", "trousers", "pack", "lower", "blanket") else 0.0
    m = kit.material(paint, kit.hex_rgb(LOOK_NOW[paint]), rough=rough, metal=metal, sheen=sheen, noise=noise,
                     noise_scale=90.0, bump=0.15 if paint in ("tunic", "trousers", "pack") else 0.0)
    m = studio_wear(m, paint)
    if paint == "skin":
        skin_studio(m)
    return m


def eye_paint():
    """The eyes: the game paints them one dark colour; the pictures paint the white, a brown iris and the pupil by
    each point's `look` (how near it is to the eyeball's front)."""
    m = bpy.data.materials.get("eye")
    if m:
        return m
    m = bpy.data.materials.new("eye")
    m.use_nodes = True
    m.diffuse_color = (*kit.hex_rgb(LOOK_NOW["eye"]), 1.0)
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = 0.12
    att = nt.nodes.new("ShaderNodeAttribute")
    att.attribute_name = "look"
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    e = ramp.color_ramp.elements
    e[0].position, e[0].color = 0.0, (0.72, 0.69, 0.64, 1)
    e[1].position, e[1].color = 0.87, (0.72, 0.69, 0.64, 1)
    e.new(0.9).color = (0.16, 0.09, 0.045, 1)
    e.new(0.972).color = (0.015, 0.012, 0.01, 1)
    nt.links.new(att.outputs["Fac"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    return m


def skin_studio(m):
    """The skin in the pictures: light under it (subsurface, a short way), the roughness varying, a flush on the nose,
    cheeks and ears, the eyebrows, a shaved shadow on the jaw and upper lip, and the short hair (`scalp`). The face's
    marks are drawn from each point's place about the eyes (the `face` attribute), so they sit right on every face."""
    nt = m.node_tree
    if nt.nodes.get("skin_done"):
        return
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Subsurface Weight"].default_value = 0.2
    bsdf.inputs["Subsurface Radius"].default_value = (1.0, 0.45, 0.3)
    bsdf.inputs["Subsurface Scale"].default_value = 0.006
    L = nt.links.new

    def math_(op, a, b=None, clamp=False):
        n_ = nt.nodes.new("ShaderNodeMath")
        n_.operation = op
        n_.use_clamp = clamp
        for i, v in enumerate((a, b)):
            if v is None:
                continue
            if isinstance(v, (int, float)):
                n_.inputs[i].default_value = v
            else:
                L(v, n_.inputs[i])
        return n_.outputs[0]

    def gauss(v, c, w):                                   # exp(-((v - c) / w)^2)
        d = math_("DIVIDE", math_("SUBTRACT", v, c), w)
        return math_("EXPONENT", math_("MULTIPLY", math_("MULTIPLY", d, d), -1.0))

    def step(v, a, b):                                    # 0 below a, 1 above b, smooth between
        n_ = nt.nodes.new("ShaderNodeMapRange")
        n_.interpolation_type = "SMOOTHSTEP"
        L(v, n_.inputs["Value"])
        n_.inputs["From Min"].default_value, n_.inputs["From Max"].default_value = a, b
        return n_.outputs["Result"]

    face = nt.nodes.new("ShaderNodeAttribute")
    face.attribute_name = "face"
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    L(face.outputs["Vector"], sep.inputs[0])
    x, z = sep.outputs["X"], sep.outputs["Z"]
    ay = math_("ABSOLUTE", sep.outputs["Y"])
    # the brows: an arch above each eye
    u = math_("SUBTRACT", ay, 0.03)
    zb = math_("SUBTRACT", 0.019, math_("MULTIPLY", math_("MULTIPLY", u, u), 3.0))
    brow = math_("MULTIPLY", gauss(z, zb, 0.0048), step(ay, 0.008, 0.016))
    brow = math_("MULTIPLY", brow, math_("SUBTRACT", 1.0, step(ay, 0.05, 0.06)))
    brow = math_("MULTIPLY", brow, step(x, -0.035, -0.018), clamp=True)
    # the shaved shadow: chin, jaw and upper lip, not the lips
    # (its top edge runs under the nose, then down the cheek to the angle of the jaw)
    top = math_("SUBTRACT", -0.05, math_("MULTIPLY", math_("MAXIMUM", math_("SUBTRACT", ay, 0.016), 0.0), 1.15))
    beard = math_("MULTIPLY", step(z, -0.15, -0.128), math_("SUBTRACT", 1.0, step(math_("SUBTRACT", z, top), -0.01, 0.002)))
    beard = math_("MULTIPLY", beard, step(x, -0.085, -0.055))
    lips = math_("MULTIPLY", math_("SUBTRACT", 1.0, step(ay, 0.02, 0.026)), gauss(z, -0.069, 0.007))
    beard = math_("MULTIPLY", beard, math_("SUBTRACT", 1.0, lips), clamp=True)
    # the flush: nose, cheeks, ears
    nose = math_("MULTIPLY", gauss(z, -0.036, 0.012), math_("SUBTRACT", 1.0, step(ay, 0.01, 0.02)))
    cheek = math_("MULTIPLY", math_("MULTIPLY", gauss(z, -0.03, 0.013), gauss(ay, 0.045, 0.012)), step(x, -0.04, -0.02))
    ear = math_("MULTIPLY", math_("MULTIPLY", gauss(z, -0.012, 0.025), step(ay, 0.06, 0.07)), math_("SUBTRACT", 1.0, step(x, -0.06, -0.04)))
    flush = math_("ADD", math_("ADD", nose, math_("MULTIPLY", cheek, 0.7)), math_("MULTIPLY", ear, 0.8), clamp=True)
    base_in = bsdf.inputs["Base Color"]
    src = base_in.links[0].from_socket if base_in.links else None
    if src is None:
        rgb = nt.nodes.new("ShaderNodeRGB")
        rgb.outputs[0].default_value = base_in.default_value[:]
        src = rgb.outputs[0]
    scalp = nt.nodes.new("ShaderNodeAttribute")
    scalp.attribute_name = "scalp"
    nail = nt.nodes.new("ShaderNodeAttribute")
    nail.attribute_name = "nail"
    knuckle = nt.nodes.new("ShaderNodeAttribute")
    knuckle.attribute_name = "knuckle"
    for fac, colour, k in ((flush, "A8604E", 0.38), (beard, "6A625C", 0.24), (brow, "3A2C22", 0.85), (scalp.outputs["Fac"], "2E241D", 1.0),
                           (knuckle.outputs["Fac"], "9A5E50", 0.32), (nail.outputs["Fac"], "D6BCB0", 0.62)):
        mix = nt.nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        L(math_("MULTIPLY", fac, k) if k != 1.0 else fac, mix.inputs["Factor"])
        L(src, mix.inputs["A"])
        mix.inputs["B"].default_value = (*kit.hex_rgb(colour), 1.0)
        src = mix.outputs["Result"]
    L(src, base_in)
    # the roughness: oilier on the nose and brow, drier elsewhere, and a fine variation
    rest = nt.nodes.new("ShaderNodeAttribute")
    rest.attribute_name = "rest"
    nz = nt.nodes.new("ShaderNodeTexNoise")
    nz.inputs["Scale"].default_value = 160.0
    L(rest.outputs["Vector"], nz.inputs["Vector"])
    rough = math_("ADD", math_("MULTIPLY", nz.outputs["Fac"], 0.18), 0.42)
    rough = math_("SUBTRACT", rough, math_("MULTIPLY", nose, 0.1))
    rough = math_("SUBTRACT", rough, math_("MULTIPLY", nail.outputs["Fac"], 0.2))     # (the nails a little glossy)
    L(rough, bsdf.inputs["Roughness"])
    # the knuckles' and the nails' edges drawn in a little (a bump from the same amounts)
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.25
    bump.inputs["Distance"].default_value = 0.0008
    L(math_("ADD", nail.outputs["Fac"], math_("MULTIPLY", knuckle.outputs["Fac"], -0.5)), bump.inputs["Height"])
    if not bsdf.inputs["Normal"].links:
        L(bump.outputs["Normal"], bsdf.inputs["Normal"])
    tag = nt.nodes.new("NodeFrame")
    tag.name = "skin_done"


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
    blot.inputs["Scale"].default_value = 7.0
    blot.inputs["Detail"].default_value = 4.0
    nt.links.new(rest.outputs["Vector"], blot.inputs["Vector"])
    shape = nt.nodes.new("ShaderNodeMapRange")
    shape.inputs["From Min"].default_value = 0.2
    shape.inputs["From Max"].default_value = 0.8
    nt.links.new(blot.outputs["Fac"], shape.inputs["Value"])
    amt = nt.nodes.new("ShaderNodeMath")
    amt.operation = "MULTIPLY"
    nt.links.new(dust.outputs["Fac"], amt.inputs[0])
    nt.links.new(shape.outputs["Result"], amt.inputs[1])
    amt2 = nt.nodes.new("ShaderNodeMath")
    amt2.operation = "MULTIPLY"
    amt2.use_clamp = True
    amt2.inputs[1].default_value = 0.65
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
        d = 0.6 * np.clip((0.2 - z) / 0.17, 0.0, 1.0) ** 1.2                                  # the feet and the boots' feet
        d += 0.25 * np.clip((0.06 - z) / 0.04, 0.0, 1.0) * (x > 0.08)                         # (the toes most)
        d += 0.18 * np.exp(-((z - 0.42) / 0.06) ** 2)                                          # the boot tops
        d += 0.6 * np.exp(-((z - 0.53) / 0.055) ** 2) * (x > 0.0)                             # the knees' fronts
        d += 0.3 * np.exp(-((z - 0.74) / 0.08) ** 2) * (x > 0.04)                             # the thighs (kneeling, lying)
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

    def cord(self, pts, nrm, width, thick, paint, sides=6):
        """A strap as a flattened round tube along pts, its flat side against a surface whose outward normals are nrm."""
        pts, nrm = np.asarray(pts, float), np.asarray(nrm, float)
        N = len(pts)
        verts = []
        for i in range(N):
            T = pts[min(i + 1, N - 1)] - pts[max(i - 1, 0)]
            T /= np.linalg.norm(T)
            n = nrm[i] - T * (nrm[i] @ T)
            n /= np.linalg.norm(n)
            B = np.cross(T, n)
            for k in range(sides):
                t = 2 * math.pi * (k + 0.5) / sides
                verts.append(pts[i] + n * thick * 0.5 + B * math.cos(t) * width * 0.5 + n * math.sin(t) * thick * 0.5)
        faces = [(i * sides + k, i * sides + (k + 1) % sides, (i + 1) * sides + (k + 1) % sides, (i + 1) * sides + k)
                 for i in range(N - 1) for k in range(sides)]
        faces.append(tuple(range(sides - 1, -1, -1)))
        faces.append(tuple((N - 1) * sides + k for k in range(sides)))
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
        step = (d / np.maximum((g ** 2).sum(axis=1), 1e-9))[:, None] * g
        sl = np.linalg.norm(step, axis=1)
        P = P - step * np.minimum(1.0, 0.04 / np.maximum(sl, 1e-12))[:, None]
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


def strap_on(g, L, pts, width, thick, paint="straps", gap=0.0025):
    """A strap laid over a layer's surface: its path pulled onto the surface and eased (it bridges a seam or a
    shoulder strap rather than stepping over it), and each of its two edges laid on the surface too, so it lies
    across a curve instead of standing off it."""
    P, N = project(L, pts)
    for _ in range(3):                                   # (eased along its length, then kept out of the cloth)
        Q = P.copy()
        Q[1:-1] = (P[:-2] + 2 * P[1:-1] + P[2:]) / 4
        d = L.at_points(Q)
        P = Q + N * np.maximum(gap - d, 0.0)[:, None]
    n_ = len(P)
    T = np.gradient(P, axis=0)
    T /= np.linalg.norm(T, axis=1)[:, None]
    B = np.cross(T, N)
    B /= np.linalg.norm(B, axis=1)[:, None]
    edges = []
    for sgn in (1.0, -1.0):
        E_, En = project(L, P + B * sgn * width * 0.5)
        edges.append((E_ + En * gap, En))
    (E1, N1), (E2, N2) = edges
    verts, faces = [], []
    for i in range(n_):
        verts += [E1[i] + N1[i] * thick, E1[i], E2[i], E2[i] + N2[i] * thick]
    for i in range(n_ - 1):
        j = i + 1
        for k in range(4):
            faces.append((4 * i + k, 4 * i + (k + 1) % 4, 4 * j + (k + 1) % 4, 4 * j + k))
    faces.append((3, 2, 1, 0))
    faces.append((4 * (n_ - 1), 4 * (n_ - 1) + 1, 4 * (n_ - 1) + 2, 4 * (n_ - 1) + 3))
    g._add(verts, faces, paint)


# ================================================================================================ the kit
def build_kit(J, spec, body_ls, Z=None):
    """Belt and buckle, ammunition pouches, Y-straps, bread bag, water bottle, entrenching tool, and by nation the
    gas-mask canister or the pack with its bedroll (or a blanket roll across the body). Returns
    {name: (object, bone or None)}: a bone means the piece is rigid on it; None means it takes the body's weights."""
    tunic = body_ls[0]
    Z = Z or (lambda z: z)                     # (heights as set on the sculpted man, mapped onto this one)
    out = {}
    # ---- the belt round the waist, laid on the tunic, and its buckle
    g = Geo()
    ang = np.linspace(0, 2 * math.pi, 28, endpoint=False)
    zb = Z(1.0)
    cx = J["spine_01"][0] + 0.03 if "crown" in J else 0.0
    start = np.stack([cx + np.cos(ang) * 0.2, np.sin(ang) * 0.2, np.full_like(ang, zb)], axis=1)
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
    spots = [((0.3, 0.0, z), (-1.0, 0.0, 0.0)) for z in (Z(1.405), Z(1.325), Z(1.245), Z(1.165), Z(1.085))]
    for side in (1.0, -1.0):
        spots += [((0.3, 0.078 * side, Z(1.336)), (-1.0, -0.25 * side, 0.0)), ((0.3, 0.105 * side, Z(0.918)), (-1.0, -0.25 * side, 0.0))]
    for start, way in spots:
        p, nrm = on_surface(tunic, start, way)
        g.tube(p - nrm * 0.001, p + nrm * 0.0035, 0.0085, 0.0072, "steel", 8)
    collar = body_ls[3]
    for side in (1.0, -1.0):
        p, nrm = on_surface(collar, (0.3, 0.036 * side, Z(1.452)), (-1.0, -0.3 * side, 0.0))
        if "crown" in J:                                # (small patches in the darker field grey)
            g.box(p + nrm * 0.0004, (0.0016, 0.015, 0.0075), "helmet", frame_from(nrm, (0.0, 0.0, 1.0)))
        else:
            g.box(p + nrm * 0.0005, (0.0022, 0.02, 0.0105), "lower", frame_from(nrm, (0.0, 0.0, 1.0)))
    out["details"] = (g.build("details"), None)
    # ---- the ammunition pouches: three cells either side of the buckle, on the belt (or one big one, or none; and
    # one behind if the nation carried it)
    po = dict(spec.get("pouches") or {})
    cells = {3: ((0.06, 0.0195), (0.103, 0.0195), (0.143, 0.0195)), 1: ((0.095, 0.042),), 0: ()}[po.get("front", 3)]
    L = Layer("pouches", "straps")
    for side in (1.0, -1.0):
        for k, (y, hw) in enumerate(cells):
            a = math.atan2(y * side, 0.12)
            p0, n0 = project(tunic, [(cx + math.cos(a) * 0.2, math.sin(a) * 0.2, zb - 0.012)])
            p0, n0 = p0[0], n0[0]
            R = frame_from(n0, (0.0, 0.0, 1.0))
            c = p0 + n0 * 0.028
            L.add(RoundBox(c, (0.021, hw, 0.04), 0.008, R), 0.0)
            L.add(RoundBox(c + R @ np.array((0.004, 0.0, 0.03)), (0.019, hw + 0.0015, 0.016), 0.006, R), 0.003)
    if po.get("rear"):
        p0, n0 = project(tunic, [(cx - 0.2, 0.0, zb - 0.012)])
        p0, n0 = p0[0], n0[0]
        R = frame_from(n0, (0.0, 0.0, 1.0))
        c = p0 + n0 * 0.032
        L.add(RoundBox(c, (0.026, 0.07, 0.045), 0.01, R), 0.0)
        L.add(RoundBox(c + R @ np.array((0.005, 0.0, 0.034)), (0.024, 0.072, 0.017), 0.007, R), 0.003)
    if cells or po.get("rear"):
        out["pouches"] = (mesh_from_layers("pouches", [L], 0.0025, 260 if len(cells) != 1 else 200), None)
    # ---- the Y-straps: from the belt in front, over each shoulder, joining in the middle of the back
    if spec.get("y_straps", True):
        g = Geo()
        for side in (1.0, -1.0):
            ctrl = [(x, y * side, Z(z)) for x, y, z in ((0.13, 0.082, 1.03), (0.13, 0.09, 1.16), (0.125, 0.1, 1.28), (0.09, 0.112, 1.4),
                    (0.025, 0.12, 1.468), (-0.05, 0.112, 1.45), (-0.12, 0.085, 1.37), (-0.142, 0.045, 1.29), (-0.145, 0.006, 1.235))]
            strap_on(g, tunic, spline(ctrl, 22), 0.032, 0.004)
        strap_on(g, tunic, spline([(-0.145, 0.0, Z(1.245)), (-0.14, 0.0, Z(1.13)), (-0.13, 0.0, Z(1.03))], 7), 0.036, 0.004)
        out["straps"] = (g.build("straps"), None)

    def across(side, lo=0.95):
        """A path from the top of one shoulder (side 1 = left) down across the chest to the other hip, and round the
        back up to the shoulder again."""
        pts = ((0.02, 0.12, 1.468), (0.1, 0.1, 1.4), (0.14, 0.04, 1.29), (0.15, -0.035, 1.17), (0.135, -0.105, 1.06),
               (0.08, -0.16, lo + 0.02), (-0.02, -0.185, lo), (-0.12, -0.135, lo + 0.06), (-0.15, -0.04, 1.17),
               (-0.14, 0.055, 1.31), (-0.08, 0.11, 1.42), (0.0, 0.124, 1.468))
        return [(x, y * side, Z(z)) for x, y, z in pts]
    if spec.get("cross_straps"):
        # the haversack's sling over the left shoulder to the right hip, the water bottle's over the right to the left
        g = Geo()
        for side in (1.0, -1.0):
            strap_on(g, tunic, spline(across(side, 0.9)[:7], 26), 0.022, 0.003)
            strap_on(g, tunic, spline(across(side, 0.9)[6:], 22), 0.022, 0.003)
        out["slings"] = (g.build("slings"), None)
    bd = spec.get("bandolier")
    if bd:
        # a cloth bandolier: a tube of cloth sewn into pockets, over one shoulder, across the chest, round the back
        bd = dict(bd) if isinstance(bd, dict) else {}
        side = 1.0 if bd.get("shoulder", "left") == "left" else -1.0
        paint = bd.get("paint", "pack")
        g = Geo()
        path = spline(across(side, 0.97), 46)
        strap_on(g, tunic, path, 0.05, 0.007, paint)
        out["bandolier"] = (g.build("bandolier"), None)
        L = Layer("bandolier_pockets", paint)
        P, N = project(tunic, path)
        front = [i for i in range(len(P)) if P[i][0] > 0.04 and Z(1.02) < P[i][2] < Z(1.42)]
        n = int(bd.get("pockets", 10))
        for i in np.linspace(front[0], front[-1], n).astype(int) if front else []:
            T_ = P[min(i + 1, len(P) - 1)] - P[max(i - 1, 0)]
            R = frame_from(N[i], T_)
            L.add(RoundBox(P[i] + N[i] * 0.014, (0.011, 0.02, 0.017), 0.007, R), 0.0)
        if front:
            out["bandolier_pockets"] = (mesh_from_layers("bandolier_pockets", [L], 0.0025, 30 * n), None)
    # ---- what hangs from the belt: bread bag (right hip), water bottle (behind it), entrenching tool (left hip)
    L = Layer("bread_bag", "pack")
    flap = Layer("bread_flap", "pack")
    p0, n0 = project(tunic, [(-0.03, -0.3, Z(0.905))])
    p0, n0 = p0[0], n0[0]
    R = frame_from(n0, (0.0, 0.0, 1.0))
    c = p0 + n0 * 0.034
    L.add(RoundBox(c, (0.032, 0.11, 0.075), 0.02, R))
    flap.add(RoundBox(c + R @ np.array((0.006, 0.0, 0.03)), (0.03, 0.112, 0.05), 0.016, R))
    out["bread_bag"] = (mesh_from_layers("bread_bag", [L, flap], 0.003, 160), "pelvis")
    # the water bottle: a flat kidney-shaped flask in a felt cover, its cup strapped over the shoulder of it, hung
    # flat on the outside of the bread bag toward the back
    bag_c, bag_R = c, R                                  # (the bread bag's middle and frame: x out, y forward, z up)
    L = Layer("bottle", "blanket")
    cup = Layer("cup", "steel")
    strap = Layer("bottle_strap", "straps")
    fc = bag_c + bag_R @ np.array((0.032 + 0.021, -0.05, -0.012))
    Rf = bag_R @ rot_euler(-8.0, 0, 0)                     # (hanging a little canted)
    L.add(Ellipsoid(fc, (0.021, 0.058, 0.088), Rf))
    L.sub(Ellipsoid(fc - Rf[:, 0] * 0.07, (0.056, 0.085, 0.13), Rf), 0.012)        # (the kidney's hollow, to the bag)
    cup.add(Tube(fc + Rf[:, 2] * 0.048, -0.034, 0.03, (0.027, 0.054), (0.026, 0.052), 0.008, Rf), 0.0)
    cup.sub(Ellipsoid(fc - Rf[:, 0] * 0.07, (0.056, 0.085, 0.13), Rf), 0.008)
    for zz in (0.06, -0.035):                            # (the straps round it, the cup held by the upper one)
        strap.add(Tube(fc + Rf[:, 2] * zz, -0.007, 0.007, (0.029, 0.06), (0.029, 0.06), 0.002, Rf), 0.0)
    strap.sub(Ellipsoid(fc - Rf[:, 0] * 0.07, (0.054, 0.083, 0.13), Rf), 0.0)
    strap.add(RoundBox(fc + Rf @ np.array((0.0, 0.0, 0.1)), (0.008, 0.009, 0.03), 0.003, Rf), 0.004)      # (its hook)
    out["bottle"] = (mesh_from_layers("bottle", [L, cup, strap], 0.0025, 200), "pelvis")
    # the entrenching tool: in its carrier flat on the left hip, a little behind, blade up, the handle down and back
    # along the outside of the thigh (clear of it when he kneels or lies)
    L = Layer("shovel_case", "straps")
    hnd = Layer("shovel_handle", "wood")
    zb_ = Z(1.0)
    p0, n0 = project(tunic, [(-0.1, 0.3, zb_ - 0.085)])
    p0, n0 = p0[0], n0[0]
    n0 = _n(n0 * np.array((1.0, 1.0, 0.0)))
    R = frame_from(n0, (0.0, 0.0, 1.0)) @ rot_euler(28.0, 0, 0)        # (its top tipped forward, so the handle trails back)
    c = p0 + n0 * 0.026
    L.add(RoundBox(c, (0.013, 0.064, 0.08), 0.012, R))
    L.add(RoundBox(c + R @ np.array((0.004, 0.0, 0.062)), (0.012, 0.067, 0.026), 0.01, R), 0.004)      # (its flap)
    hnd.add(RoundCone(c + R @ np.array((0.0, 0.0, -0.06)), c + R @ np.array((0.0, 0.0, -0.2)), 0.0145, 0.0135))
    hnd.add(Sphere(c + R @ np.array((0.0, 0.0, -0.205)), 0.0175), 0.005)
    out["shovel"] = (mesh_from_layers("shovel", [L, hnd], 0.0025, 170), "pelvis")
    # ---- by nation
    if spec.get("grenade_bag"):
        # a cloth bag at the left hip, in front of the entrenching tool, the wooden handles of four stick grenades
        # standing out of its mouth
        L = Layer("grenade_bag", "pack")
        hnd = Layer("grenade_handles", "wood")
        p0, n0 = project(tunic, [(0.12, 0.26, zb_ - 0.09)])
        p0, n0 = p0[0], n0[0]
        n0 = _n(n0 * np.array((1.0, 1.0, 0.0)))
        R = frame_from(n0, (0.0, 0.0, 1.0)) @ rot_euler(-8.0, 0, 0)
        c = p0 + n0 * 0.035
        L.add(RoundBox(c, (0.03, 0.058, 0.068), 0.02, R))
        for k, (u, v) in enumerate(((-0.03, -0.006), (-0.01, 0.008), (0.012, -0.004), (0.032, 0.006))):
            base = c + R @ np.array((v, u, 0.04))
            hnd.add(RoundCone(base, base + R @ np.array((0.004 * k - 0.006, 0.006 * (k - 1.5), 0.085)), 0.0105, 0.0095))
        out["grenade_bag"] = (mesh_from_layers("grenade_bag", [L, hnd], 0.0025, 200), "pelvis")
    if spec["canister"]:
        # the gas-mask canister: upright, slung behind the right hip below the belt, tucked close
        L = Layer("canister", "helmet")
        p0, n0 = project(tunic, [(-0.3, -0.07, zb_ - 0.1)])
        p0, n0 = p0[0], n0[0]
        n0 = _n(n0 * np.array((1.0, 1.0, 0.0)))
        Rax = frame_from(n0, (0.0, 0.0, 1.0)) @ rot_euler(-12.0, 0, 0)
        cc = p0 + n0 * 0.036
        L.add(Tube(cc, -0.1, 0.095, (0.037, 0.037), (0.037, 0.037), 0.011, Rax))
        for t_ in (-0.065, 0.0, 0.065):
            L.add(Torus(cc + Rax[:, 2] * t_, 0.0375, 0.0035, Rax), 0.003)          # (its fluting)
        L.add(Tube(cc + Rax[:, 2] * 0.095, -0.012, 0.013, (0.04, 0.04), (0.039, 0.039), 0.006, Rax), 0.002)     # (the lid)
        out["canister"] = (mesh_from_layers("canister", [L], 0.0025, 190), "pelvis")
    if spec["pack"]:
        L = Layer("pack", "pack")
        roll = Layer("bedroll", "pack")
        p0, n0 = project(tunic, [(-0.3, 0.0, Z(1.25))])
        back = p0[0][0]
        c = np.array((back - 0.068, 0.0, Z(1.235)))
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
            pts.append((0.16 * math.cos(t) - 0.015, -0.03 - 0.2 * lift, Z(1.2 + 0.25 * lift)))
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


def build_helmet(kind, J, sides=None):
    """One helmet as a rigid shell (outside, rolled rim, inside) with its chin strap, placed on the head in the rest
    pose. The head bone's own frame is the model's (x forward, z up), so the helmet file is the same shape moved to
    the head bone's head."""
    k = dict(HELMETS[kind])
    if "crown" in J:
        # MakeHuman's man: seated so the brim comes to the brow with a finger's gap over the crown, as wide as his skull
        sides = sides or 36
        dome_rings = 6
        C = np.array((J["skull_x"] - 0.004 + k["dx"], 0.0, J["crown"] - 0.108))
        k["az"] += 0.013
        k["ay"] *= (J["skull_half"][1] + 0.025) / 0.108
        k["ax"] *= (J["skull_half"][0] + 0.023) / 0.122
    else:
        sides = sides or 24
        dome_rings = 4
        C = np.array((J["head"][0] + 0.004 - 0.01 + k["dx"], 0.0, 1.655))
    th = 0.0045 if not k.get("cloth") else 0.006
    nexp = k.get("n", 2.0)
    outer, inner = [], []
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
            if "eye_x" in J:                           # (MakeHuman's man: in front of the ear, under the jaw, under the chin)
                chin, ex = np.array(J["chin"]), J["eye_x"]
                ctrl = [C + np.array((0.0, side * (r - 0.012), z + 0.004)), np.array((ex - 0.072, side * 0.066, chin[2] + 0.05)),
                        chin + np.array((-0.045, side * 0.042, -0.003)), chin + np.array((-0.016, 0.0, -0.007))]
                centre = np.array((J["skull_x"], 0.0, chin[2] + 0.07))
            else:
                ctrl = [C + np.array((0.0, side * (r - 0.012), z + 0.004)), (hx + 0.02, side * 0.07, 1.575), (hx + 0.055, side * 0.04, 1.528),
                        (hx + 0.064, 0.0, 1.512)]
                centre = np.array((hx, 0.0, 1.6))
            pts = spline(ctrl, 6)
            nrm = []
            for q in pts:
                v = q - centre
                nrm.append(v / np.linalg.norm(v))
            if "eye_x" in J:
                g.cord(spline(ctrl, 9), [_n(q - centre) for q in spline(ctrl, 9)], 0.011, 0.0035, "straps")
            else:
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
    rifle = mesh_from_layers("rifle_stock", [wood], 0.0025, 300)
    g = Geo()
    st = "steel"
    bz = 0.042
    g.box((-0.3225, 0.0, -0.026), (0.003, 0.0215, 0.058), st, rot_euler(0, -4, 0))                     # butt plate
    g.tube((-0.06, 0.0, 0.04), (0.17, 0.0, bz), 0.0165, 0.0155, st, 14)                                  # receiver
    g.tube((0.17, 0.0, bz), (0.78, 0.0, bz), 0.0105, 0.0085, st, 12)                                     # barrel
    g.tube((0.38, 0.0, 0.031), (0.394, 0.0, 0.031), 0.026, 0.026, st, 14, squash=1.1)                    # bands
    g.tube((0.6, 0.0, 0.034), (0.618, 0.0, 0.034), 0.023, 0.023, st, 14, squash=1.0)
    g.box((0.762, 0.0, 0.057), (0.007, 0.0028, 0.011), st)                                               # front sight
    g.tube((0.752, 0.0, 0.058), (0.774, 0.0, 0.058), 0.012, 0.012, st, 12, caps=False)                            # its hood
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
    b.tube((-0.105, 0.0, 0.042), (-0.055, 0.0, 0.042), 0.012, 0.0125, st, 12)                           # bolt
    b.tube((-0.05, -0.006, 0.042), (-0.062, -0.052, 0.024), 0.0042, 0.0042, st, 6)                      # its handle
    b.tube((-0.06, -0.05, 0.025), (-0.066, -0.064, 0.016), 0.0085, 0.0075, st, 10)                      # the knob
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
    g.tube((0.16, 0.0, bz), (0.5, 0.0, bz), 0.023, 0.022, st, 14)                                       # cooling jacket
    for x in (0.2, 0.28, 0.36, 0.44):
        g.tube((x, 0.0, bz), (x + 0.012, 0.0, bz), 0.026, 0.026, st, 10)                                 # its ribs
    g.tube((0.5, 0.0, bz), (0.72, 0.0, bz), 0.011, 0.0105, st, 12)                                       # barrel
    g.tube((0.72, 0.0, bz), (0.78, 0.0, bz), 0.013, 0.0165, st, 12)                                      # flash hider
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
    g = J.get("grip_in_hand_" + s, GRIP_IN_HAND)
    return np.array(J["hand_" + s]) + a * g[0] + t * g[1] + n * g[2]


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


KNEE_FLOOR = 0.045                               # (the lowest a knee's joint may go: a knee on the ground)

# ---- the arms as a real body moves them (range of motion, the coordinator's sheet of 18 November 2026; arms_check.py
# measures every frame against it). Inside ARM_EASY the solver keeps the pose's own elbow and hand; past it the elbow is
# swung round the shoulder-wrist line, and a hand that grips rolled round what it grips (up to its `roll`), to the
# nearest place a man's arm goes.
ARM_EASY = dict(elevation=115.0, behind=15.0, medial=25.0, internal=45.0, external=55.0, elbow=135.0, pronation=55.0,
                supination=60.0, wrist_flex=45.0, wrist_ext=45.0, radial=12.0, ulnar=22.0, tip_up=0.15, tip_in=0.4)
ARM_HARD = dict(elevation=170.0, behind=30.0, medial=40.0, internal=70.0, external=90.0, elbow=145.0, pronation=80.0,
                supination=85.0, wrist_flex=75.0, wrist_ext=65.0, radial=20.0, ulnar=30.0, tip_up=0.5, tip_in=0.7)
ARM_SWING = np.radians(np.arange(-180.0, 180.0, 7.5))      # (the elbow's swing round the shoulder-wrist line, tried)
ARM_PRIOR = 0.012          # (cost of a degree squared of swing or roll away from what the pose asks)
ARM_GENTLE = 0.003         # (cost of a degree squared of the wrist off straight, the forearm off mid-turn)
ARM_FLIP_COST = 6.0        # (cost of taking a rod's grip end for end, so the hand keeps the asked way unless it hurts)


def _nv(V):
    return V / np.maximum(np.linalg.norm(V, axis=-1, keepdims=True), 1e-12)


def arm_angles_batch(Ms, A, K, T, Dup, Dh, flex0, a0, t0, n0):
    """A real arm's angles (degrees) for N candidate poses at once, in his side's terms (Ms: world -> forward, out to
    that side, up of his chest): the shoulder's elevation, plane, behind and across, the humerus's rotation (internal
    +), the elbow's flexion and where its point goes, the forearm's pronation (+) and the wrist's flexion and radial
    deviation. A the shoulder; K, T (N, 3) elbows and wrists; Dup, Dh (N, 3, 3) the upper arms' and hands' turns."""
    u = _nv((K - A) @ Ms.T)
    f = _nv((T - K) @ Ms.T)
    fl = _nv(np.einsum("nij,j->ni", Dup, flex0) @ Ms.T)
    fl = _nv(fl - u * np.sum(fl * u, -1, keepdims=True))
    x = np.array((1.0, 0.0, 0.0))
    k = np.stack((u[:, 1], -u[:, 0], np.zeros(len(u))), -1)        # (0, 0, -1) x u
    c = -u[:, 2]
    ref = c[:, None] * x + np.cross(k, np.broadcast_to(x, u.shape)) + k * (k[:, 0] / np.maximum(1.0 + c, 1e-6))[:, None]
    ref = _nv(ref - u * np.sum(ref * u, -1, keepdims=True))
    hinge = _nv(np.cross(u, fl))
    ah = _nv(np.einsum("nij,j->ni", Dh, a0) @ Ms.T)
    th = _nv(np.einsum("nij,j->ni", Dh, t0) @ Ms.T)
    nh = _nv(np.einsum("nij,j->ni", Dh, n0) @ Ms.T)
    h = _nv(hinge - f * np.sum(hinge * f, -1, keepdims=True))
    npp = _nv(nh - f * np.sum(nh * f, -1, keepdims=True))
    return dict(elevation=np.degrees(np.arccos(np.clip(-u[:, 2], -1, 1))), plane=np.degrees(np.arctan2(u[:, 0], u[:, 1])),
                behind=np.degrees(np.arcsin(np.clip(-u[:, 0], -1, 1))), medial=np.degrees(np.arcsin(np.clip(-u[:, 1], -1, 1))),
                rotation=np.degrees(np.arctan2(np.sum(np.cross(ref, fl) * u, -1), np.sum(ref * fl, -1))),
                elbow=np.degrees(np.arctan2(np.sum(f * fl, -1), np.sum(f * u, -1))), tip_up=-fl[:, 2], tip_in=fl[:, 1],
                pronation=np.degrees(np.arctan2(np.sum(np.cross(h, npp) * f, -1), np.sum(h * npp, -1))),
                wrist_flex=np.degrees(np.arctan2(-np.sum(f * nh, -1), np.sum(f * ah, -1))),
                wrist_dev=np.degrees(np.arctan2(-np.sum(f * th, -1), np.sum(f * ah, -1))))


def arm_cost(an):
    """How far past easy (and, ten times dearer, past the limits) a batch of arms is: degrees squared."""
    def past(x, lim):
        return np.maximum(x - lim, 0.0) ** 2
    bent = np.clip((an["elbow"] - 15.0) / 20.0, 0.0, 1.0)
    cost = 0.0
    for lim, w in ((ARM_EASY, 1.0), (ARM_HARD, 10.0)):
        behind = lim["behind"] + (50.0 - ARM_HARD["behind"]) * (lim is ARM_HARD) * np.clip(1.0 - an["elevation"] / 90.0, 0, 1)
        cost = cost + w * (past(an["elevation"], lim["elevation"]) + past(an["behind"], behind) + past(an["medial"], lim["medial"])
                           + past(an["rotation"], lim["internal"]) + past(-an["rotation"], lim["external"])
                           + past(an["elbow"], lim["elbow"]) + 4.0 * past(-an["elbow"], 0.0)
                           + past(an["pronation"], lim["pronation"]) + past(-an["pronation"], lim["supination"])
                           + past(an["wrist_flex"], lim["wrist_flex"]) + past(-an["wrist_flex"], lim["wrist_ext"])
                           + past(an["wrist_dev"], lim["radial"]) + past(-an["wrist_dev"], lim["ulnar"])
                           + bent * (past(60.0 * an["tip_up"], 60.0 * lim["tip_up"]) + past(60.0 * an["tip_in"], 60.0 * lim["tip_in"])))
    over90 = np.clip((an["elevation"] - 95.0) / 15.0, 0, 1) * (an["plane"] < 45.0)
    # (and a gentle pull toward a straight wrist and a mid-turned forearm everywhere, as a real man's own comfort: the
    # best arm then moves smoothly as the pose does, instead of sliding about a flat floor and snapping at its edge)
    gentle = ARM_GENTLE * (an["wrist_flex"] ** 2 + 2.0 * an["wrist_dev"] ** 2 + 0.3 * an["pronation"] ** 2)
    return cost + 10.0 * over90 * past(an["rotation"], -20.0) + gentle


def hand_flip(h):
    """Whether a hand may take its grip end for end (see Rig.comfortable_arm): its heaviest part says so."""
    if "blend" in h:
        return hand_flip(max(h["blend"], key=lambda x: x[0])[1])
    return bool(h.get("flip", False))


def arm_spec(h):
    """Whether a hand is placed by its arm's own angles (all of it, through any blend): then it is not re-solved."""
    if "blend" in h:
        return all(arm_spec(hh) for w, hh in h["blend"] if abs(w) > 1e-9)
    return "arm" in h


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
        self.mrest, self.mo, self.weapon_kind = None, {}, "rifle"     # (human.py's rest, for the mocap clips; see mo_prepare)
        # the low poses (kneeling, lying) were set out on a man whose pelvis rests 0.95 m up and whose legs reach 0.822 m:
        # on another man the pelvis's drop and the feet's spread from it are scaled to his build (1 on the first man)
        reach = 0.99 * sum(np.linalg.norm(np.array(J[b]) - np.array(J[a])) for a, b in (("thigh_l", "calf_l"), ("calf_l", "foot_l")))
        self.fit = (float(self.H0["pelvis"][2]) / 0.95, float(reach) / 0.822)
        if "eye_r" in J:
            self.eye0 = {s: np.array(J["eye_" + s]) for s in ("l", "r")}
        else:
            self.eye0 = {"r": np.array((J["head"][0] + 0.004 + 0.083, -0.033, 1.64)), "l": np.array((J["head"][0] + 0.004 + 0.083, 0.033, 1.64))}
        # the arms' rest references for the range of motion: the way each forearm swings as its elbow bends, the hand's
        # axes (t toward the thumb), each upper arm's and forearm's rest frames as two_bone builds them
        self.arm0 = {}
        for s in ("l", "r"):
            up, lo = self.H0["lowerarm_" + s] - self.H0["upperarm_" + s], self.H0["hand_" + s] - self.H0["lowerarm_" + s]
            u = _n(up)
            a0, t0, n0 = (np.asarray(v, float) for v in self.hand0[s])
            if (self.T0["thumb_" + s] - self.H0["hand_" + s]) @ t0 < 0:
                t0 = -t0
            line = self.H0["hand_" + s] - self.H0["upperarm_" + s]
            r_pole = up - _n(line) * (up @ _n(line))
            self.arm0[s] = {"flex": _n(lo - u * (lo @ u)), "a": _n(a0), "t": _n(t0), "n": _n(n0),
                            "Fu": frame2(up, r_pole), "Fl": frame2(lo, r_pole),
                            "l1": float(np.linalg.norm(up)), "l2": float(np.linalg.norm(lo))}
        self.pocket0 = self.H0["upperarm_r"] + np.array((POCKET[0], POCKET[1], POCKET[2]))

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

    def pocket(self, st):
        """The right shoulder's pocket (where a rifle's butt goes), riding the right clavicle."""
        return self.at(st, "clavicle_r", self.pocket0)

    def head_tilt(self, st, R):
        """The head's tilt off the sights' line (degrees): forward (+, the face toward the ground) and canted to his
        right (+), its up axis measured from upright in the vertical plane of the bore R[:, 0]."""
        b = R[:, 0]
        zp = _n(np.array((0.0, 0.0, 1.0)) - b * b[2])
        yp = np.cross(zp, b)
        u = st["D"]["head"] @ np.array((0.0, 0.0, 1.0))
        return math.degrees(math.atan2(u @ b, u @ zp)), math.degrees(math.atan2(-(u @ yp), u @ zp))

    def sight_measures(self, st, kind="rifle"):
        """The aim as a rifleman's instructor checks it: the right eye's distance off the sights' line and how far
        it sits behind the rear sight's notch (cm), the head's forward tilt and its cant to the right (degrees), the
        right cheek's height over the comb (cm) and the right elbow's drop below the shoulder (degrees)."""
        W = st["W"]
        R, o = W[:3, :3], W[:3, 3]
        spec = WEAPONS[kind]
        v = self.eye(st, "r") - (o + R @ np.array((0.0, 0.0, spec["sight_z"])))
        along = float(v @ R[:, 0])
        pitch, cant = self.head_tilt(st, R)
        ch = R.T @ (self.at(st, "head", self.eye0["r"] + np.array(CHEEK)) - o)
        sh, el = st["H"]["upperarm_r"], st["H"]["lowerarm_r"]
        return {"eye_line_cm": 100.0 * float(np.linalg.norm(v - R[:, 0] * along)),
                "eye_behind_rear_cm": 100.0 * (REAR_NOTCH[kind] - along),
                "head_pitch": pitch, "head_cant": cant,
                "cheek_over_comb_cm": 100.0 * float(ch[2] - COMB_Z[kind]),
                "elbow_r_drop": math.degrees(math.asin(float(np.clip((sh[2] - el[2]) / np.linalg.norm(el - sh), -1.0, 1.0)))),
                "butt_slide_cm": 100.0 * float(np.linalg.norm(st.get("butt_slide", np.zeros(2))))}

    def head_to_sight(self, st, P, W, w):
        """The rifle brought to the face, not the face down to the rifle: the right eye put on the sights' line (as
        far as `w`) with the head held as a rifleman holds it, tilted forward SIGHT_PITCH and canted onto the comb
        SIGHT_CANT, the neck and head bent, leaned and turned together; the butt may ease up to SIGHT_SLIDE up, down or
        across in the shoulder's pocket to meet the eye. Returns the weapon's place, eased."""
        kind = P.get("weapon_kind", "rifle")
        spec = WEAPONS[kind]
        bn, bh = P.get("neck", (0.0, 0.0, 0.0)), P.get("head", (0.0, 0.0, 0.0))
        # (d: the neck's and the head's own bends forward, then the lean and the turn shared between them: the neck
        # may bend forward while the head tips back up, the head carried forward over the stock still upright)
        split = (np.array((0.4, 0.5)), np.array((0.6, 0.5)))
        on_bipod = kind == "lmg" and P.get("bipod", 0.0) > 50.0
        # (a man lying on his elbows tips his head back further on his neck than one standing: SIGHT_NOD's back limit
        # goes from 15 to 25 degrees as his chest lies down)
        lying = float(np.clip((0.8 - st["D"]["chest"][2, 2]) / 0.5, 0.0, 1.0))
        nod = (SIGHT_NOD[0] - 10.0 * lying, SIGHT_NOD[1])

        def eased(s):
            """The weapon's place with its butt eased s = (across, up) in the pocket: a rifle moved bodily, a gun
            standing on its bipod turned about its feet (its muzzle dipping as the butt comes up)."""
            R, o = W[:3, :3], W[:3, 3]
            if not on_bipod:
                return M4(R, o + R[:, 1] * s[0] + R[:, 2] * s[1])
            foot = bipod_foot()
            arm = np.array(BUTT_AT["lmg"]) - foot
            th = math.degrees(math.atan2(s[1], math.hypot(arm[0], arm[2])))
            Rn = R @ R_axis((0, 1, 0), th)
            return M4(Rn, o + R @ foot - Rn @ foot + R[:, 1] * s[0])

        def put(d, into):
            self._child(into, "neck", E(*(np.asarray(bn, float) + np.array((d[0], split[0][0] * d[2], split[0][1] * d[3])))))
            self._child(into, "head", E(*(np.asarray(bh, float) + np.array((d[1], split[1][0] * d[2], split[1][1] * d[3])))))
            return into

        def band(x, lo, hi):
            return max(x - hi, 0.0) + min(x - lo, 0.0)

        def resid(x):
            d, s = x[:4], x[4:]
            tmp = put(d, {"D": dict(st["D"]), "H": dict(st["H"])})
            W2 = eased(s)
            R = W2[:3, :3]
            v = self.eye(tmp, "r") - (W2[:3, 3] + R @ np.array((0.0, 0.0, spec["sight_z"])))
            rel = float(v @ R[:, 0])
            pitch, cant = self.head_tilt(tmp, R)
            Rn = tmp["D"]["chest"].T @ tmp["D"]["head"]               # (the neck's bend as rom.py measures it)
            nk = math.degrees(math.atan2(Rn[0, 2], Rn[2, 2]))
            return np.array((v @ R[:, 1] / 0.002, v @ R[:, 2] / 0.002, band(rel, *SIGHT_RELIEF) / 0.002,
                             band(pitch, *SIGHT_PITCH) / 0.5, band(cant, *SIGHT_CANT) / 0.5,
                             (pitch - sum(SIGHT_PITCH) / 2) / 4.0, (cant - sum(SIGHT_CANT) / 2) / 4.0,
                             band(nk, *SIGHT_NECK) / 0.5, band(bh[0] + d[1], *nod) / 0.5,
                             band(s[0], -SIGHT_SLIDE, SIGHT_SLIDE) / 0.002, band(s[1], -SIGHT_SLIDE, SIGHT_SLIDE) / 0.002,
                             0.004 * d[0], 0.004 * d[1], 0.004 * (d[0] + d[1]), 0.004 * d[2], 0.004 * d[3], s[0] / 0.05, s[1] / 0.05))
        x = np.zeros(6)
        lo_x = np.array((-40.0, -40.0, -45.0, -60.0, -4.0 * SIGHT_SLIDE, -4.0 * SIGHT_SLIDE))
        hi_x = np.array((40.0, 40.0, 45.0, 60.0, 4.0 * SIGHT_SLIDE, 4.0 * SIGHT_SLIDE))
        scale = np.array((0.5, 0.5, 0.5, 0.5, 0.001, 0.001))
        lam = 1e-3
        r0 = resid(x)
        for _ in range(30):
            J = np.stack([(resid(x + e) - r0) / e.sum() for e in np.diag(scale)], axis=1)
            A = J.T @ J
            step = np.linalg.solve(A + lam * np.diag(np.diag(A) + 1e-9), -J.T @ r0)
            xn = np.clip(x + step, lo_x, hi_x)
            rn = resid(xn)
            if rn @ rn < r0 @ r0:
                x, r0, lam = xn, rn, max(lam * 0.3, 1e-6)
                if np.all(np.abs(step / scale) < 0.02):
                    break
            else:
                lam *= 10.0
                if lam > 1e6:
                    break
        d, s = x[:4], x[4:]
        put(w * d, st)
        st["sight_miss"] = float(np.hypot(r0[0], r0[1]) * 0.002) if w >= 0.999 else None
        st["sight_turn"] = tuple(float(v) for v in w * d)
        st["butt_slide"] = w * s
        return eased(w * s)

    def comfortable_arm(self, st, s, g, a1, n1, pole, roll_axis=None, roll=0.0, prior=ARM_PRIOR, flip=False):
        """The hand's turn and the elbow's pole for a hand gripping at g (its axes a1, n1 asked for): the elbow swung
        round the shoulder-wrist line and the hand rolled round what it grips (up to `roll` degrees, about roll_axis
        through g) to where the arm is easiest (arm_cost) and nearest what was asked. `flip`: the hand may also take
        the grip turned end for end (half round its palm's normal: the thumb the other way along a rod or a rim, still
        round it), at a small cost, where that is the easier arm."""
        if flip:
            best = None
            for k, nn in enumerate((n1, n1)):
                a_k = a1 if k == 0 else -np.asarray(a1, float)
                Dh, V, c = self.comfortable_arm(st, s, g, a_k, n1, pole, roll_axis, roll, prior, False)
                c = c + ARM_FLIP_COST * k
                if best is None or c < best[2]:
                    best = (Dh, V, c)
            return best
        R0 = self.arm0[s]
        a0, t0, n0 = (np.asarray(v, float) for v in self.hand0[s])
        D0 = frame2(a1, n1) @ frame2(a0, n0).T
        g_rest = grip_point(self.J, s)
        A = st["H"]["upperarm_" + s]
        Ms = np.diag((1.0, -1.0 if s == "r" else 1.0, 1.0)) @ st["D"]["chest"].T
        rolls = np.radians(np.linspace(-roll, roll, 9)) if roll > 0 else np.zeros(1)
        ax = _n(roll_axis if roll_axis is not None else np.cross(_n(a1), _n(n1)))
        l1, l2 = R0["l1"], R0["l2"]

        def batch(rs, phis):
            Dh = np.stack([R_axis(ax, math.degrees(r)) @ D0 for r in rs])                   # (nr, 3, 3)
            Wr = g - np.einsum("rij,j->ri", Dh, g_rest - self.H0["hand_" + s])              # (nr, 3)
            d = Wr - A
            dist = np.linalg.norm(d, axis=-1)
            dc = np.clip(dist, abs(l1 - l2) + 1e-4, (l1 + l2) * 0.9995)
            ul = d / dist[:, None]
            e1 = _nv(np.asarray(pole, float) - ul * (ul @ np.asarray(pole, float))[:, None])
            e2 = np.cross(ul, e1)
            ca = (l1 * l1 + dc * dc - l2 * l2) / (2 * l1 * dc)
            sa = np.sqrt(np.maximum(1 - ca * ca, 0.0))
            nr, nphi = len(rs), len(phis)
            V = np.cos(phis)[None, :, None] * e1[:, None] + np.sin(phis)[None, :, None] * e2[:, None]   # (nr, nphi, 3)
            K = A + l1 * (ul[:, None] * ca[:, None, None] + V * sa[:, None, None])
            T = A + ul * dc[:, None]
            T = np.broadcast_to(T[:, None], K.shape)
            a = _nv(K - A)
            nn = _nv(V - a * np.sum(V * a, -1, keepdims=True))
            Dup = np.stack((a, nn, np.cross(a, nn)), axis=-1) @ R0["Fu"].T                     # (nr, nphi, 3, 3)
            DhN = np.broadcast_to(Dh[:, None], (nr, nphi, 3, 3))
            an = arm_angles_batch(Ms, A, K.reshape(-1, 3), T.reshape(-1, 3), Dup.reshape(-1, 3, 3),
                                  DhN.reshape(-1, 3, 3), R0["flex"], R0["a"], R0["t"], R0["n"])
            c = arm_cost(an).reshape(nr, nphi)
            c = c + prior * np.degrees(phis)[None, :] ** 2 + ARM_PRIOR * np.degrees(rs)[:, None] ** 2
            return c, V, Dh
        c, V, Dh = batch(rolls, ARM_SWING)
        i, j = np.unravel_index(int(np.argmin(c)), c.shape)
        # (then a finer grid round the best, a step either way in both, and a parabola through the best of that and its
        # neighbours: the answer moves smoothly from frame to frame instead of from one sample to the next)
        ds = ARM_SWING[1] - ARM_SWING[0]
        dr = (rolls[1] - rolls[0]) if len(rolls) > 1 else 0.0
        fine_s = ARM_SWING[j] + np.linspace(-ds, ds, 9)
        fine_r = np.clip(rolls[i] + np.linspace(-dr, dr, 9), rolls[0], rolls[-1]) if dr > 0 else rolls
        c, V, Dh = batch(fine_r, fine_s)
        i, j = np.unravel_index(int(np.argmin(c)), c.shape)
        phi = fine_s[j]
        if 0 < j < len(fine_s) - 1:
            cm, c0, cp = c[i, j - 1], c[i, j], c[i, j + 1]
            dj = 0.5 * (cm - cp) / (cm - 2 * c0 + cp) if (cm - 2 * c0 + cp) > 1e-9 else 0.0
            phi = phi + float(np.clip(dj, -0.5, 0.5)) * (fine_s[1] - fine_s[0])
        r = fine_r[i]
        if 0 < i < len(fine_r) - 1 and fine_r[i + 1] > fine_r[i - 1]:
            cm, c0, cp = c[i - 1, j], c[i, j], c[i + 1, j]
            di = 0.5 * (cm - cp) / (cm - 2 * c0 + cp) if (cm - 2 * c0 + cp) > 1e-9 else 0.0
            r = r + float(np.clip(di, -0.5, 0.5)) * (fine_r[1] - fine_r[0])
        c1, V1, Dh1 = batch(np.array([r]), np.array([phi]))
        return Dh1[0], V1[0, 0], float(c1[0, 0])

    # ---- the whole pose
    def solve(self, P, lift_knees=4):
        st = {"D": {}, "H": {}, "short": {}}
        st["D"]["root"], st["H"]["root"] = np.eye(3), np.zeros(3)
        pa = np.asarray(P.get("pelvis_at", (0, 0, 0)), float)
        low = float(np.clip((-pa[2] - 0.2) / 0.2, 0.0, 1.0))        # (only the low poses: the gaits are fitted already)
        kz, kl = 1.0 + (self.fit[0] - 1.0) * low, 1.0 + (self.fit[1] - 1.0) * low
        pa = pa * np.array((1.0, 1.0, kz))
        st["H"]["pelvis"] = self.H0["pelvis"] + pa
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
            at = np.asarray(f["at"], float).copy()
            at[:2] = st["H"]["pelvis"][:2] + (at[:2] - st["H"]["pelvis"][:2]) * kl
            st["short"]["leg_" + s] = self.two_bone(st, "thigh_" + s, "calf_" + s, "foot_" + s, at, pole)
            self._child(st, "foot_" + s, Dw=Df)
            self._child(st, "ball_" + s, R_axis((0, 1, 0), -f.get("toe", 0.0)))
        # a knee is never let through the ground: where one would go, the pelvis is lifted and the pose solved again
        knee = min(float(st["H"]["calf_l"][2]), float(st["H"]["calf_r"][2]))
        if lift_knees and knee < KNEE_FLOOR - 0.002:
            pa0 = np.asarray(P.get("pelvis_at", (0, 0, 0)), float).copy()
            pa0[2] += (KNEE_FLOOR - knee) / kz
            return self.solve(dict(P, pelvis_at=tuple(pa0)), lift_knees=lift_knees - 1)
        # the weapon
        W = P["weapon"](self, st) if callable(P["weapon"]) else P["weapon"]
        if P.get("sight", 0.0) > 1e-3:
            W = self.head_to_sight(st, P, W, min(float(P["sight"]), 1.0))
        st["D"]["weapon"], st["H"]["weapon"] = W[:3, :3], W[:3, 3] + W[:3, :3] @ (self.H0["weapon"] - self.H0["weapon"])
        st["H"]["weapon"] = W[:3, 3].copy()
        st["W"] = W
        bk, lift = P.get("bolt", (0.0, 0.0))
        st["D"]["bolt"] = st["D"]["weapon"] @ R_axis((1, 0, 0), -lift)
        st["H"]["bolt"] = st["H"]["weapon"] + st["D"]["weapon"] @ (self.H0["bolt"] - self.H0["weapon"] + np.array((-bk, 0.0, 0.0)))
        self._child(st, "bipod", R_axis((0, 1, 0), -P.get("bipod", 0.0)))
        # the arms: each hand to its grip, or to a place of its own, the elbow and the hand's roll where a man's go
        for s, side in (("l", 1.0), ("r", -1.0)):
            a0, t0, n0 = self.hand0[s]
            target_g, a1, n1, pole = self.resolve_hand(st, P, P["hand_" + s], side)
            if P.get("comfort", ARM_COMFORT) and not arm_spec(P["hand_" + s]):
                rx, rr, pw = self.hand_roll(st, P, P["hand_" + s])
                Dh, pole, st["arm_cost_" + s] = self.comfortable_arm(st, s, target_g, a1, n1, pole, rx, rr, pw,
                                                                     flip=hand_flip(P["hand_" + s]))
            else:
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

    def arm_place(self, st, arm, side):
        """A hand placed by the arm's own angles, as rom.py measures them (degrees, in his side's terms of the chest):
        arm = (elevation, plane, rotation, elbow, pronation[, wrist flexion, radial deviation]); elevation 0 hanging,
        90 level, plane 0 out to the side and 90 straight ahead, rotation internal +, elbow flexion, pronation + from
        mid-turn. Returns what resolve_hand does: (grip point, hand axis, palm, the elbow's pole), all in the world."""
        s = "l" if side > 0 else "r"
        el, pl, rot, fl, pr = (math.radians(float(x)) for x in arm[:5])
        wf, wd = (math.radians(float(x)) for x in (tuple(arm[5:7]) + (0.0, 0.0))[:2])
        C = st["D"]["chest"]
        M = np.diag((1.0, side, 1.0))
        to_w = C @ M
        down = np.array((0.0, 0.0, -1.0))
        u = np.array((math.sin(el) * math.sin(pl), math.sin(el) * math.cos(pl), -math.cos(el)))
        ref = mocap.swing(down[None], u[None])[0] @ np.array((0.0, -1.0, 0.0))
        k = R_axis(u, math.degrees(rot)) @ _n(ref - u * (ref @ u))
        lo = math.cos(fl) * u + math.sin(fl) * np.cross(k, u)
        n = R_axis(lo, math.degrees(pr)) @ k
        a = lo
        a, n = math.cos(wf) * a + math.sin(wf) * n, -math.sin(wf) * a + math.cos(wf) * n
        t = np.cross(n, a)
        a = math.cos(wd) * a + math.sin(wd) * t
        R0 = self.arm0[s]
        A = st["H"]["upperarm_" + s]
        K = A + R0["l1"] * (to_w @ u)
        T = K + R0["l2"] * (to_w @ lo)
        a_w, n_w = to_w @ a, to_w @ n
        a0, t0, n0 = self.hand0[s]
        Dh = frame2(a_w, n_w) @ frame2(a0, n0).T
        g = T + Dh @ (grip_point(self.J, s) - self.H0["hand_" + s])
        w = _n(T - A)
        pole = (K - A) - w * ((K - A) @ w)
        return g, a_w, n_w, pole

    def hand_roll(self, st, P, h):
        """(axis, degrees) a hand may roll round what it holds: a grip on the weapon rolls round the weapon's own line
        (the stock's wrist, the fore-end); a hand given a place of its own, round its palm's across axis."""
        if "blend" in h:
            # (mixed by the blend's weights, not taken from whichever hold weighs most: a roll's axis or reach that
            # jumped where the weights cross would swing the elbow to another basin in one frame)
            parts = [(w, self.hand_roll(st, P, hh)) for w, hh in h["blend"] if w > 1e-6]
            tw = sum(w for w, _ in parts)
            axes = [(w, r[0]) for w, r in parts if r[0] is not None]
            ax = None
            if axes:
                ax = sum(w * a for w, a in axes)
                ax = ax / max(float(np.linalg.norm(ax)), 1e-9)
            return ax, sum(w * r[1] for w, r in parts) / tw, sum(w * r[2] for w, r in parts) / tw
        pw = float(h.get("pole_weight", ARM_PRIOR))
        if "grip" in h:
            return st["W"][:3, :3] @ np.asarray(h.get("roll_axis", (1.0, 0.0, 0.0)), float), float(h.get("roll", 35.0)), pw
        return None, float(h.get("roll", 20.0)), pw

    def resolve_hand(self, st, P, h, side):
        """A hand's spec -> (grip point in the world, hand direction, palm direction, elbow pole)."""
        if "blend" in h:
            # (the hands' turns mixed as quaternions, not their axes one by one: a turn mixed axis by axis bunches
            # most of itself into the middle of the blend, and the wrist snaps there)
            out = [(w, self.resolve_hand(st, P, hh, side)) for w, hh in h["blend"]]
            g = sum(w * r[0] for w, r in out)
            pole = sum(w * r[3] for w, r in out)
            qs = [mocap.mat_to_quat(frame2(r[1], r[2])) for _, r in out]
            ref = qs[max(range(len(out)), key=lambda k: out[k][0])]
            q = sum(w * (qq if float(qq @ ref) >= 0.0 else -qq) for (w, _), qq in zip(out, qs))
            M = mocap.quat_to_mat(q / max(float(np.linalg.norm(q)), 1e-12))
            return g, M[:, 0], M[:, 1], pole
        if "arm" in h:
            return self.arm_place(st, h["arm"], side)
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


# ================================================================================================ the body, from MakeHuman
# The man himself is MakeHuman's base man (human.py, CC0): a real face, real anatomy and MakeHuman's own skin weights,
# his hands half shut. His uniform is made from MakeHuman's two clothing helper shells fitted to him (the body-tight
# suit and the skirt): each shell is read as a distance field, every face of it pushed out by how loosely that cloth
# hangs over that bone, and the cloths are joined and cut as layers, so hems, cuffs and boot tops stay true edges. The
# collar, cuffs, folds, pockets, buttons, boots and soles are our own shapes laid on it. Whatever the uniform covers of
# the skin is cut away; the head, the neck and the hands stay, thinned to the triangle budget.
SOLE = 0.02                        # the boots' soles lift him: made 1.73 m tall, he stands 1.75 m in his boots
CURL = (34.0, 62.0, 38.0)          # his fingers half shut in the rest pose (the thumb a third as much), ready for a rifle
GRIP_ALONG, GRIP_OUT = 0.78, 0.03  # a gripped stock's middle: so far from the wrist toward the knuckles, so far off the palm
NATION_SEED = {"GER": 3, "POL": 7, "JAP": 11, "CHN": 13}  # whose face each army's exported man has (the squad's men get faces of their own)
DETAIL = 0.005                                 # (how much the uniform's thinning spares its sharp places)
BUDGET = {"uniform": 2450, "head": 1260, "hand": 520}       # (the uniform gains about 600 more where it is cut along its paints)
BUDGET["puttees"] = 700                        # (more for a uniform whose puttees have grooved turns, which would take it from the tunic)


def _axis_resample(G, q, lo, h, axis, far):
    """G resampled along one axis at coordinates q (linear between samples). Beyond the grid the edge value is taken
    and the distance beyond it added, so the field keeps rising away from the shape."""
    q = np.asarray(q, float)
    n = G.shape[axis]
    qc = np.clip(q, lo, lo + (n - 1) * h)
    f = (qc - lo) / h
    i = np.clip(np.floor(f).astype(np.int64), 0, n - 2)
    t = np.clip(f - i, 0.0, 1.0).astype(np.float32)
    shape = [1, 1, 1]
    shape[axis] = len(q)
    A = np.take(G, i, axis=axis)
    B = np.take(G, i + 1, axis=axis)
    return A + (B - A) * t.reshape(shape) + np.abs(q - qc).astype(np.float32).reshape(shape)


def trilinear(G, lo, h, P, far=0.1):
    """Values of the grid G (corner lo, spacing h) at points P (N, 3), linear between samples; beyond the grid, the
    edge's value and the distance beyond it."""
    P = np.asarray(P, float)
    n = np.array(G.shape)
    hi = lo + (n - 1) * h
    Pc = np.clip(P, lo, hi)
    f = (Pc - lo) / h
    i = np.clip(np.floor(f).astype(np.int64), 0, n - 2)
    t = np.clip(f - i, 0.0, 1.0)
    out = np.zeros(len(f))
    for dx in (0, 1):
        wx = t[:, 0] if dx else 1.0 - t[:, 0]
        for dy in (0, 1):
            wy = t[:, 1] if dy else 1.0 - t[:, 1]
            for dz in (0, 1):
                wz = t[:, 2] if dz else 1.0 - t[:, 2]
                out += wx * wy * wz * G[i[:, 0] + dx, i[:, 1] + dy, i[:, 2] + dz]
    return out + np.linalg.norm(P - Pc, axis=1)


class GridField:
    """A distance field held as samples on a grid (sculpt's shapes' interface: d(px, py, pz), and the box lo..hi
    outside which it need not be asked)."""

    def __init__(self, G, glo, gh, lo=None, hi=None, far=0.1):
        self.G, self.glo, self.gh, self.far = G, np.asarray(glo, float), float(gh), far
        ghi = self.glo + (np.array(G.shape) - 1) * gh
        self.lo = self.glo.copy() if lo is None else np.maximum(np.asarray(lo, float), self.glo)
        self.hi = ghi if hi is None else np.minimum(np.asarray(hi, float), ghi)
        self.c, self.R = (self.lo + self.hi) / 2, None
        self.boxed = lo is not None or hi is not None          # (kept to its box wherever it is asked, not only on the grid)

    def _box(self, px, py, pz):
        """The distance to its box (negative inside): the field is the cloth cut by the box."""
        c, h = (self.lo + self.hi) / 2, (self.hi - self.lo) / 2
        qx, qy, qz = np.abs(px - c[0]) - h[0], np.abs(py - c[1]) - h[1], np.abs(pz - c[2]) - h[2]
        out = np.sqrt(np.maximum(qx, 0) ** 2 + np.maximum(qy, 0) ** 2 + np.maximum(qz, 0) ** 2)
        return out + np.minimum(np.maximum(np.maximum(qx, qy), qz), 0.0)

    def d(self, px, py, pz):
        px, py, pz = np.asarray(px), np.asarray(py), np.asarray(pz)
        if px.ndim == 3 and px.shape[1:] == (1, 1) and py.shape[::2] == (1, 1) and pz.shape[:2] == (1, 1):
            # (a whole block of the sculpting grid: resampled axis by axis)
            G = _axis_resample(self.G, px.ravel(), self.glo[0], self.gh, 0, self.far)
            G = _axis_resample(G, py.ravel(), self.glo[1], self.gh, 1, self.far)
            G = _axis_resample(G, pz.ravel(), self.glo[2], self.gh, 2, self.far)
            if self.boxed:
                G = np.maximum(G, self._box(px, py, pz))
            return G
        bx, by, bz = np.broadcast_arrays(px, py, pz)
        P = np.stack([bx.ravel(), by.ravel(), bz.ravel()], axis=1)
        out = trilinear(self.G, self.glo, self.gh, P, self.far).reshape(bx.shape)
        if self.boxed:
            out = np.maximum(out, self._box(bx, by, bz))
        return out


class Blend:
    """Two shapes joined smoothly (over `k` metres) into one, to be used as one shape."""

    def __init__(self, a, b, k):
        self.a, self.b, self.k = a, b, k
        self.lo, self.hi = np.minimum(a.lo, b.lo), np.maximum(a.hi, b.hi)
        self.c, self.R = (self.lo + self.hi) / 2, None

    def d(self, px, py, pz):
        from sculpt import smin
        return smin(self.a.d(px, py, pz), self.b.d(px, py, pz), self.k)


class Both:
    """Where two shapes overlap (the larger of their distances): a sphere cut flat, to take off one sleeve's end."""

    def __init__(self, a, b):
        self.a, self.b = a, b
        boxes = [(p.lo, p.hi) for p in (a, b) if p.lo is not None]
        self.lo = np.max([bx[0] for bx in boxes], axis=0) if boxes else None
        self.hi = np.min([bx[1] for bx in boxes], axis=0) if boxes else None
        self.c, self.R = (a.c if a.lo is not None else b.c), None

    def d(self, px, py, pz):
        return np.maximum(self.a.d(px, py, pz), self.b.d(px, py, pz))


class ShellField:
    """One of MakeHuman's clothing shells as a distance field: smoothed (subdivided twice), then at every point of a
    grid round it the distance to the nearest point of the shell, signed by the way that face looks, and which face it
    was. A cloth is the shell with each face pushed out by its bones' share of how loose the cloth is (`cloth`)."""

    def __init__(self, ob, h=0.005, band=0.032, levels=2):
        from mathutils.bvhtree import BVHTree
        # its open ends (neck, wrists, waist) capped first, so what lies beyond an opening is outside it
        src = ob
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
        mod = ob.modifiers.new("smooth_for_field", "SUBSURF")
        mod.levels = levels
        dg = bpy.context.evaluated_depsgraph_get()
        eo = ob.evaluated_get(dg)
        me = eo.to_mesh()
        V = np.zeros(len(me.vertices) * 3)
        me.vertices.foreach_get("co", V)
        V = V.reshape(-1, 3)
        self.names = [g.name for g in ob.vertex_groups]
        W = np.zeros((len(V), len(self.names)), np.float32)
        for v in me.vertices:
            for ge in v.groups:
                if ge.group < len(self.names):
                    W[v.index, ge.group] = ge.weight
        F = [tuple(p.vertices) for p in me.polygons]
        eo.to_mesh_clear()
        tmp_me = ob.data
        bpy.data.objects.remove(ob)
        bpy.data.meshes.remove(tmp_me)
        W /= np.maximum(W.sum(axis=1, keepdims=True), 1e-6)
        self.Wf = np.array([W[list(f)].mean(axis=0) for f in F], np.float32)
        self.Fc = np.array([V[list(f)].mean(axis=0) for f in F])
        bvh = BVHTree.FromPolygons(V.tolist(), F)
        lo = V.min(axis=0) - band - 0.012
        hi = V.max(axis=0) + band + 0.012
        # coarse: everywhere (the sign deep inside and far out)
        hc = 4 * h
        nc = np.ceil((hi - lo) / hc).astype(int) + 1
        xs, ys, zs = (lo[a] + np.arange(nc[a]) * hc for a in range(3))
        Pc = np.stack(np.meshgrid(xs, ys, zs, indexing="ij"), axis=-1).reshape(-1, 3)
        Gc = np.empty(len(Pc), np.float32)
        fn = bvh.find_nearest
        for k, p in enumerate(Pc.tolist()):
            loc, nrm, idx, dist = fn(p)
            Gc[k] = dist if (p[0] - loc[0]) * nrm[0] + (p[1] - loc[1]) * nrm[1] + (p[2] - loc[2]) * nrm[2] >= 0 else -dist
        Gc = Gc.reshape(tuple(nc))
        # fine: read from the coarse, then exactly in a band round the shell
        n = np.ceil((hi - lo) / h).astype(int) + 1
        G = GridField(Gc, lo, hc).d(*(np.asarray(lo[a] + np.arange(n[a]) * h).reshape([-1 if b == a else 1 for b in range(3)])
                                       for a in range(3))).astype(np.float32)
        fi = np.full(G.shape, -1, np.int32)
        near = np.argwhere(np.abs(G) < band)
        pts = (lo + near * h).tolist()
        vals = np.empty(len(pts), np.float32)
        faces = np.full(len(pts), -1, np.int32)
        for k, p in enumerate(pts):
            loc, nrm, idx, dist = fn(p, band * 1.6)
            if loc is None:
                vals[k] = np.nan
                continue
            vals[k] = dist if (p[0] - loc[0]) * nrm[0] + (p[1] - loc[1]) * nrm[1] + (p[2] - loc[2]) * nrm[2] >= 0 else -dist
            faces[k] = idx
        ok = ~np.isnan(vals)
        G[tuple(near[ok].T)] = vals[ok]
        fi[tuple(near[ok].T)] = faces[ok]
        self.sd, self.fi, self.lo, self.h = G, fi, lo, h

    def cloth(self, loose, lo=None, hi=None, default=0.0, more=None):
        """The shell pushed out face by face: `loose` {bone: metres}, each face by its bones' share (others `default`),
        and by `more(face middles)` if given. Kept to the box lo..hi."""
        off = self.Wf @ np.array([loose.get(n, default) for n in self.names], np.float32)
        if more is not None:
            off = off + np.asarray(more(self.Fc), np.float32)
        G = self.sd.copy()
        m = self.fi >= 0
        G[m] -= off[self.fi[m]]
        far = self.sd[~m]                              # (away from the shell: inside stays inside, outside outside)
        G[~m] = np.where(far < 0, far - max(float(off.max()), 0.0), far - min(float(off.min()), 0.0))
        return GridField(G, self.lo, self.h, lo, hi)


def mh_body(nation, seed=None, name=None):
    """MakeHuman's man for an army (his face by `seed`), standing on his soles, hands half shut, with low eyeballs."""
    import human
    seed = NATION_SEED.get(nation, 3) if seed is None else seed
    man = human.build(seed=seed, nation=nation, height=1.75 - SOLE, stand=SOLE, curl=CURL, eye_res=(10, 6),
                      name=name or f"man{seed}")
    man["seed"] = seed
    return man


def joints_of(man):
    """Our joints (as `joints()` gives them for the sculpted man) on MakeHuman's man: bone heads from his rig, and the
    hands' axes, the grip, the eyes, the toes and the head's own measures from his joints and his mesh."""
    arm = man["armature"]
    H = {b.name: np.array(b.head_local) for b in arm.data.bones}
    T = {b.name: np.array(b.tail_local) for b in arm.data.bones}
    mj = {k: np.array(v) for k, v in man["mh_joints"].items()}
    J = {b: tuple(float(x) for x in H[b]) for b in H}
    J["root"] = (0.0, 0.0, 0.0)
    J["head_end"] = tuple(T["head"])
    for s, S in (("l", "L"), ("r", "R")):
        g = lambda b, end="head": mj[f"{b}.{S}____{end}"]
        wr = np.array(J["hand_" + s])
        a = _n(g("finger3-1") - wr)
        t = g("finger2-1") - g("finger5-1")
        t = _n(t - a * (a @ t))
        n = np.cross(a, t) if s == "l" else np.cross(t, a)
        J["hand_axes_" + s] = (tuple(a), tuple(t), tuple(n))
        knuckles = np.mean([g(f"finger{f}-1") for f in range(2, 6)], axis=0)
        J["fingers_" + s] = tuple(knuckles)
        J["fingers_end_" + s] = tuple(g("finger3-3", "tail"))
        J["thumb_end_" + s] = tuple(g("finger1-3", "tail"))
        L = float(np.linalg.norm(knuckles - wr))
        J["grip_in_hand_" + s] = (GRIP_ALONG * L, 0.0, GRIP_OUT)
        balls = np.mean([g(f"toe{k}-1") for k in range(1, 6)], axis=0)
        J["ball_" + s] = tuple(balls)
        J["toe_" + s] = tuple(np.mean([g("toe1-2", "tail")] + [g(f"toe{k}-3", "tail") for k in range(2, 6)], axis=0))
    for e, s in zip(man["eyes"], ("l", "r")):
        c = np.mean([np.array(v.co) for v in e.data.vertices], axis=0)
        J["eye_" + s] = tuple(c)
    # the head: the skull's middle and top (for the helmets), and the chin (for their straps)
    B = np.array([v.co for v in man["body"].data.vertices])
    eye_z = J["eye_l"][2]
    skull = B[B[:, 2] > eye_z + 0.03]
    J["crown"] = float(B[:, 2].max())
    J["skull_x"] = float((skull[:, 0].max() + skull[:, 0].min()) / 2)
    J["skull_half"] = (float((skull[:, 0].max() - skull[:, 0].min()) / 2), float(np.abs(skull[:, 1]).max()))
    ex = J["eye_l"][0]
    prof = B[(np.abs(B[:, 1]) < 0.008) & (B[:, 0] > ex - 0.005) & (B[:, 2] < eye_z - 0.05) & (B[:, 2] > J["neck"][2])]
    men = prof[np.argmin(prof[:, 2])]                                  # the bottom of the chin
    J["chin"] = (float(men[0]), 0.0, float(men[2]))
    J["eye_x"] = float(ex)
    return J


def landmarks(J):
    """Heights on this man, and the map from the sculpted man's heights to his (the kit's places were set on that man)."""
    LM = {"ankle": J["foot_l"][2], "knee": J["calf_l"][2], "hip": J["thigh_l"][2], "shoulder": J["upperarm_l"][2],
          "head": J["head"][2], "crown": J["crown"]}
    LM["belt"] = LM["hip"] + 0.105
    LM["hem"] = LM["hip"] - 0.15
    nk, hd = np.array(J["neck"]), np.array(J["head"])
    LM["neck_axis"] = _n(hd - nk)
    LM["collar_c"] = nk - LM["neck_axis"] * 0.012
    LM["collar"] = float(LM["collar_c"][2])
    old = [0.0, 0.088, 0.49, 0.915, 1.0, 1.418, 1.452, 1.585, 1.75]
    new = [0.0, LM["ankle"], LM["knee"], LM["hip"], LM["belt"], LM["shoulder"], LM["collar"], LM["head"], LM["crown"]]
    LM["Z"] = lambda z: float(np.interp(z, old, new))
    return LM


def ring_frame(axis):
    """A frame whose z runs along `axis` and whose x leans forward (for rings round a limb)."""
    z = _n(axis)
    x = np.array((1.0, 0.0, 0.0)) - z * z[0]
    if np.linalg.norm(x) < 1e-3:
        x = np.array((0.0, 0.0, 1.0)) - z * z[2]
    x = _n(x)
    return np.stack([x, np.cross(z, x), z], axis=1)


def ring_axes(L, c, R):
    """The layer's half-widths round c along the frame's x and y (each the mean of the two ways)."""
    dirs = [R[:, 0], -R[:, 0], R[:, 1], -R[:, 1]]
    lo, hi = np.zeros(4), np.full(4, 0.25)
    for _ in range(22):
        mid = (lo + hi) / 2
        inside = L.at_points(np.array([c + dd * m for dd, m in zip(dirs, mid)])) < 0
        lo, hi = np.where(inside, mid, lo), np.where(inside, hi, mid)
    r = (lo + hi) / 2
    return float(r[0] + r[1]) / 2, float(r[2] + r[3]) / 2


def leg_point(J, s, z):
    """The middle of the lower leg at height z."""
    a, k = np.array(J["foot_" + s]), np.array(J["calf_" + s])
    return a + (k - a) * ((z - a[2]) / (k[2] - a[2]))


def neck_size(man, LM):
    """The neck's half-widths (ahead, across) where the collar sits, and the middle of it there."""
    B = np.array([v.co for v in man["body"].data.vertices])
    ax, c = LM["neck_axis"], LM["collar_c"]
    s = (B - c) @ ax
    ring = B[(np.abs(s) < 0.006) & (np.linalg.norm((B - c) - np.outer(s, ax), axis=1) < 0.09)]
    fwd = _n(np.array((1.0, 0.0, 0.0)) - ax * ax[0])
    x = (ring - c) @ fwd
    y = ring[:, 1]
    mid = c + fwd * (x.max() + x.min()) / 2
    return (float(x.max() - x.min()) / 2, float(np.abs(y).max())), mid, fwd


def puttee_spec(spec):
    """A nation's puttees option filled out (see NATION_KIT's notes), or None."""
    p = spec.get("puttees")
    if not p:
        return None
    p = dict(p) if isinstance(p, dict) else {}
    for k, v in (("top", None), ("boot", "ankle"), ("colour", None), ("turns", 0)):
        p.setdefault(k, v)
    return p


def uniform_layers(man, J, spec, LM):
    """Tunic, trousers and boots (or ankle boots and gaiters, or puttees), collar and cuffs, soles: the uniform as layers."""
    t0 = time.time()
    tights, skirt = ShellField(man["tights"]), ShellField(man["skirt"])
    print(f"  shells read as fields in {time.time() - t0:.0f} s")
    Z = LM["Z"]
    tunic, trousers, boots = Layer("tunic", "tunic"), Layer("trousers", "trousers"), Layer("boots", "boots")
    collar, cuffs, soles = Layer("collar", "trousers"), Layer("cuffs", "tunic"), Layer("soles", "leather")
    layers = [tunic, trousers, boots, collar, cuffs, soles]
    BIGBOX = 3.0
    # ---- the tunic: the suit over the chest, back and arms, loose at the waist; the skirt over the hips to the hem
    loose = {"pelvis": 0.017, "spine_01": 0.017, "spine_02": 0.015, "chest": 0.012, "clavicle_l": 0.01, "clavicle_r": 0.01,
             "neck": 0.007, "upperarm_l": 0.011, "upperarm_r": 0.011, "lowerarm_l": 0.009, "lowerarm_r": 0.009,
             "hand_l": 0.008, "hand_r": 0.008}
    OFF = -0.06                                        # (a bone a cloth does not cover: its part of the shell drawn in, out of sight)
    ARMS = {b + s: OFF for b in ("upperarm_", "lowerarm_", "hand_", "fingers_", "thumb_") for s in ("l", "r")}
    LEGS = ("pelvis", "spine_01", "thigh_l", "thigh_r")
    tunic.add(tights.cloth(loose, lo=(-BIGBOX, -BIGBOX, LM["hem"] - 0.02), default=OFF))
    # the skirt hangs over the hips and the trousers from the belt, flaring a little to the hem
    flare = lambda P: 0.026 + 0.02 * np.clip((LM["belt"] - P[:, 2]) / (LM["belt"] - LM["hem"]), 0.0, 1.0) ** 1.5
    skirt_box = dict(lo=(-BIGBOX, -BIGBOX, LM["hem"] - 0.02), hi=(BIGBOX, BIGBOX, LM["belt"] + 0.016))   # (its top hidden under the belt)
    # (the skirt's shell and the suit's hips joined smoothly into one: the skirt is narrower than the thighs here and there)
    tunic.add(Blend(skirt.cloth(ARMS, more=flare, **skirt_box),
                    tights.cloth({b: 0.0 for b in LEGS}, more=flare, default=OFF, **skirt_box), 0.05), 0.02)
    tunic.cut(HalfSpace((0.0, 0.0, LM["hem"]), (0.0, 0.0, -1.0)), 0.004)
    # the turned hem: the skirt's last 2 cm a little thicker, a clean edge under it
    hem_box = dict(lo=(-BIGBOX, -BIGBOX, LM["hem"] - 0.01), hi=(BIGBOX, BIGBOX, LM["hem"] + 0.021))
    hem = lambda P: flare(P) + 0.0035
    tunic.add(Blend(skirt.cloth(ARMS, more=hem, **hem_box),
                    tights.cloth({b: 0.0 for b in LEGS}, more=hem, default=OFF, **hem_box), 0.05), 0.002)
    tunic.cut(HalfSpace((0.0, 0.0, LM["hem"]), (0.0, 0.0, -1.0)), 0.0025)
    ax = LM["neck_axis"]
    tunic.cut(HalfSpace(LM["collar_c"] + ax * 0.004, ax), 0.004)
    (nrx, nry), nmid, nfwd = neck_size(man, LM)
    for s, side in (("l", 1.0), ("r", -1.0)):
        el, wr = np.array(J["lowerarm_" + s]), np.array(J["hand_" + s])
        fa = _n(wr - el)
        cut_at = wr - fa * 0.03
        tunic.sub(Both(HalfSpace(cut_at, -fa), Sphere(cut_at + fa * 0.05, 0.11)), 0.0)
    # (the shapes over it are placed on the tunic as it stands so far)
    for s, side in (("l", 1.0), ("r", -1.0)):
        sh, el, wr = (np.array(J[k + s]) for k in ("upperarm_", "lowerarm_", "hand_"))
        fa = _n(wr - el)
        Rc = ring_frame(fa)
        # (no rings of folds at the elbow: at this many faces they read as dents, and they crush when the arm bends)
        # the turned-back cuff at the sleeve's end
        cc = wr - fa * 0.048
        rx, ry = ring_axes(tunic, cc, Rc)
        cuffs.add(Tube(cc, -0.02, 0.018, (rx + 0.004, ry + 0.004), (rx + 0.0045, ry + 0.0045), 0.005, Rc))
        # the shoulder strap on the shoulder's top, from the collar to the shoulder's point
        inner = nmid + np.array((0.0, side * (nry + 0.012), 0.0))
        outer = sh + np.array((0.0, side * 0.035, 0.0))
        mid = (inner + outer) / 2
        p, nrm = on_surface(tunic, mid + np.array((0.0, 0.0, 0.25)), (0.0, 0.0, -1.0))
        Rs = frame_from(_n(outer - inner), nrm)
        L_ = float(np.linalg.norm(outer - inner))
        tunic.add(RoundBox(p, (L_ / 2, 0.021, 0.006), 0.005, Rs), 0.004)
    # ---- the collar: a band round the foot of the neck, square to the neck, open a little at the throat
    Rn = np.stack([nfwd, np.cross(ax, nfwd), ax], axis=1)
    collar.add(Tube(nmid, -0.024, 0.026, (nrx + 0.017, nry + 0.016), (nrx + 0.012, nry + 0.012), 0.01, Rn))
    collar.add(Torus(nmid + ax * 0.024, nrx + 0.009, 0.0075, Rn, major_y=nry + 0.009), 0.006)
    collar.sub(RoundBox(nmid + nfwd * (nrx + 0.02) + ax * 0.03, (0.03, 0.009, 0.045), 0.004, Rn), 0.002)
    LM["neck_ring"] = (nmid, nfwd, nrx, nry)
    # the cloth bunched above and below the belt: folds running up from it, more at the back
    zb = LM["belt"]
    cx = J["spine_01"][0] + 0.03
    for k, ang in enumerate(np.radians([70, 108, 140, 166, 194, 220, 252, 290])):
        lean = 0.012 * math.sin(3.1 * k)
        a0 = np.array((cx + math.cos(ang) * 0.2, math.sin(ang) * 0.2, zb + 0.026))
        a1 = np.array((cx + math.cos(ang + lean * 4) * 0.2, math.sin(ang + lean * 4) * 0.2, zb + 0.075 + 0.012 * math.cos(2.3 * k)))
        (p0, p1), (n0, n1) = project(tunic, [a0, a1])
        tunic.add(RoundCone(p0 - n0 * 0.007, p1 - n1 * 0.0105, 0.0135, 0.011), 0.012)
        # (none below it: on the skirt, at this many faces, they read as crumpling)
    # and where the back of the tunic wrinkles across the shoulder blades
    for z, w in ((Z(1.36), 0.06), (Z(1.3), 0.075)):
        (p0, p1), (n0, n1) = project(tunic, [(-0.3, w, z + 0.01), (-0.3, -w, z - 0.008)])
        tunic.add(RoundCone(p0 - n0 * 0.007, p1 - n1 * 0.007, 0.0125, 0.0125), 0.014)
    # the front's button strip and the four pockets (two on the breast, two on the skirt), each a patch with a flap
    top = nmid + nfwd * nrx
    p, nrm = on_surface(tunic, (0.4, 0.0, (top[2] + LM["hem"]) / 2), (-1.0, 0.0, 0.0))
    half = (top[2] - 0.03 - LM["hem"]) / 2
    tunic.add(RoundBox(p - nrm * 0.004, (0.006, 0.014, half), 0.005, frame_from(nrm, (0.0, 0.0, 1.0))), 0.008)
    LM["pockets"] = []
    for side in (1.0, -1.0):
        for aim_, half in (((0.4, 0.08 * side, Z(1.305)), (0.006, 0.05, 0.058)), ((0.4, 0.105 * side, Z(0.875)), (0.006, 0.06, 0.07))):
            p, nrm = on_surface(tunic, np.array(aim_), (-1.0, -0.25 * side, 0.0))
            R = frame_from(nrm, (0.0, 0.0, 1.0))
            tunic.add(RoundBox(p, half, 0.005, R), 0.004)
            flap = p + R @ np.array((0.005, 0.0, half[2] - 0.013))
            tunic.add(RoundBox(flap, (0.0075, half[1] + 0.004, 0.018), 0.006, R), 0.003)
            LM["pockets"].append((p, R, half))
    # ---- the trousers: the suit over the seat and legs, full in the thigh
    pt = puttee_spec(spec)
    put_top = (LM["ankle"] + pt["top"] if pt["top"] else LM["knee"] - 0.07) if pt else None
    shoe = bool(pt) and pt["boot"] == "shoe"
    boot_top = LM["knee"] - 0.075 if spec["tall_boots"] else LM["ankle"] + (-0.012 if shoe else 0.045)
    tuck = boot_top if spec["tall_boots"] else (put_top if pt else LM["ankle"] + 0.22)   # (where the trousers go into the boot, gaiter or puttee)
    trousers.add(tights.cloth({"pelvis": 0.012, "spine_01": 0.012, "thigh_l": 0.017, "thigh_r": 0.017, "calf_l": 0.009,
                               "calf_r": 0.009, "foot_l": 0.006, "foot_r": 0.006, "ball_l": 0.006, "ball_r": 0.006},
                              hi=(BIGBOX, BIGBOX, LM["belt"] + 0.04), default=OFF,
                              more=lambda P: -0.011 * np.clip((tuck + 0.012 - P[:, 2]) / 0.03, 0.0, 1.0)))
    for s, side in (("l", 1.0), ("r", -1.0)):
        if spec["tall_boots"]:
            # the trousers bloused over the boot's top: one soft roll, a little lower at the back
            hip, knee, ank = (np.array(J[k + s]) for k in ("thigh_", "calf_", "foot_"))
            c = leg_point(J, s, boot_top + 0.013)
            Rs_ = ring_frame(knee - ank)
            rx, ry = ring_axes(trousers, c, Rs_)
            trousers.add(Torus(c, rx + 0.002, 0.0125, Rs_ @ rot_euler(0, -6, 0), major_y=ry + 0.002), 0.016)
    # ---- the boots: the suit's feet and calves made boots, with a toe, a heel and a sole of their own
    boots.add(tights.cloth({"calf_l": 0.012, "calf_r": 0.012, "foot_l": 0.009, "foot_r": 0.009, "ball_l": 0.008, "ball_r": 0.008},
                           hi=(BIGBOX, BIGBOX, boot_top + 0.02), default=OFF))
    T = np.array([v.co for v in man["tights"].data.vertices])
    lower = straps = None
    lower_lo = LM["ankle"] + (-0.022 if shoe else 0.01)
    lower_top = put_top if pt else LM["ankle"] + 0.22
    if not spec["tall_boots"] and pt:
        # puttees: a cloth band wound from the boot's top to under the knee, close on the leg, its turns showing as a
        # spiral of low ridges, and the end tied off round the top
        lower, straps = Layer("puttees", "lower"), None
        layers += [lower]
        lower.add(tights.cloth({b + s: 0.0105 for b in ("calf_", "foot_") for s in ("l", "r")}, lo=(-BIGBOX, -BIGBOX, lower_lo),
                               hi=(BIGBOX, BIGBOX, lower_top + 0.012), default=OFF))
        for s, side in (("l", 1.0), ("r", -1.0)):
            # each turn's edge pressed in as a shallow slanting groove (cut, never added: nothing can stand proud of
            # the leg where the calf narrows), and the tape round the top
            n_t = int(pt["turns"])
            axis = np.array(J["calf_" + s]) - np.array(J["foot_" + s])
            for k in range(n_t):
                z = lower_lo + 0.02 + (lower_top - lower_lo - 0.04) * (k + 0.5) / n_t
                c = leg_point(J, s, z)
                Rk = ring_frame(axis) @ rot_euler(9.0 * side, 0, 0)
                rx, ry = ring_axes(lower, c, Rk)
                lower.sub(Torus(c, rx + 0.0008, 0.0026, Rk, major_y=ry + 0.0008), 0.0015)
            c = leg_point(J, s, lower_top - 0.014)
            Rk = ring_frame(axis)
            rx, ry = ring_axes(lower, c, Rk)
            lower.add(Torus(c, rx - 0.0015, 0.003, Rk, major_y=ry - 0.0015), 0.003)                    # (the tape tied round)
    elif not spec["tall_boots"]:
        lower, straps = Layer("gaiters", "lower"), Layer("gaiter_straps", "straps")
        layers += [lower, straps]
        lower.add(tights.cloth({b + s: 0.013 for b in ("calf_", "foot_") for s in ("l", "r")}, lo=(-BIGBOX, -BIGBOX, LM["ankle"] + 0.01),
                               hi=(BIGBOX, BIGBOX, LM["ankle"] + 0.23), default=OFF))
    for s, side in (("l", 1.0), ("r", -1.0)):
        ank, ball = np.array(J["foot_" + s]), np.array(J["ball_" + s])
        foot = T[(T[:, 2] < SOLE + 0.02) & (T[:, 1] * side > 0.05)][:, :2]
        mid = foot.mean(axis=0)
        w_, v_ = np.linalg.eigh(np.cov((foot - mid).T))
        along = v_[:, 1] if v_[0, 1] > 0 else -v_[:, 1]                                               # (the foot's own length)
        Ry = rot_euler(0, 0, math.degrees(math.atan2(along[1], along[0])))
        u = (foot - mid) @ along
        across = np.array((-along[1], along[0]))
        wv = (foot - mid) @ across
        heel_u, toe_u = u.min(), u.max()
        half_w = (wv.max() - wv.min()) / 2 + 0.01
        at = lambda uu, ww=0.0, z=0.0: np.array((*(mid + along * uu + across * ww), z))
        L_ = (toe_u - heel_u) / 2 + 0.024
        soles.add(RoundBox(at((toe_u + heel_u) / 2 + 0.008, (wv.max() + wv.min()) / 2, SOLE * 0.5 + 0.0005),
                           (L_, half_w + 0.006, SOLE * 0.5 + 0.0005), 0.005, Ry))                             # the sole, proud of the upper
        soles.add(RoundBox(at(heel_u + 0.033, 0.0, 0.021), (0.042, half_w - 0.008, 0.02), 0.004, Ry), 0.004)   # the heel
        boots.add(Ellipsoid(at(toe_u - 0.035, 0.0, SOLE + 0.03), (0.062, half_w - 0.008, 0.032), Ry), 0.02)  # the toe cap
        boots.add(RoundBox(at(heel_u + 0.028, 0.0, SOLE + 0.03), (0.035, half_w - 0.014, 0.028), 0.014, Ry), 0.012)  # the heel's counter
        if straps is not None:
            for z in (LM["ankle"] + 0.08, LM["ankle"] + 0.18):
                c = leg_point(J, s, z)
                rx, ry = ring_axes(lower, c, ring_frame((0.0, 0.0, 1.0)))
                straps.add(Torus(c, rx + 0.001, 0.0055, major_y=ry + 0.001))
    boots.cut(HalfSpace((0.0, 0.0, boot_top), (0.0, 0.0, 1.0)), 0.006)
    boots.cut(HalfSpace((0.0, 0.0, SOLE - 0.002), (0.0, 0.0, -1.0)), 0.0)                       # (standing on the sole)
    if lower is not None:
        lower.cut(HalfSpace((0.0, 0.0, lower_top), (0.0, 0.0, 1.0)), 0.005)
        lower.cut(HalfSpace((0.0, 0.0, lower_lo + 0.002), (0.0, 0.0, -1.0)), 0.0)
        for s, side in (("l", 1.0), ("r", -1.0)):
            c = leg_point(J, s, lower_top + 0.025)
            rx, ry = ring_axes(trousers, c, ring_frame((0.0, 0.0, 1.0)))
            trousers.add(Torus(c, rx - 0.002, 0.016, major_y=ry - 0.002), 0.02)                            # bloused over the gaiter
    LM["boot_top"] = boot_top
    LM["puttee_top"] = put_top
    print(f"  uniform laid out in {time.time() - t0:.0f} s")
    return layers


def uniform_weights(piece, man, LM=None):
    """The uniform takes the weights of the shell it was made from: at each point, those of the nearest place on the
    suit (or, for the tunic's skirt, on the skirt), mixed across that face; then evened out along the uniform itself so
    the cloth bends smoothly where two bones meet."""
    from mathutils.bvhtree import BVHTree
    names = [g.name for g in man["tights"].vertex_groups]

    def source(ob, keep=None):
        W, nm = weights_of(ob)
        Wm = np.zeros((len(W), len(names)))
        for j, n in enumerate(nm):
            if n in names:
                Wm[:, names.index(n)] = W[:, j]
        V = [tuple(v.co) for v in ob.data.vertices]
        tris = []
        for p_ in ob.data.polygons:
            vs = list(p_.vertices)
            if keep is not None and not all(keep[i] for i in vs):
                continue
            for i in range(1, len(vs) - 1):
                tris.append((vs[0], vs[i], vs[i + 1]))
        return BVHTree.FromPolygons(V, tris), np.array(V), np.array(tris), Wm

    Vs = np.array([v.co for v in man["skirt"].data.vertices])
    srcs = [source(man["tights"]), source(man["skirt"], Vs[:, 2] > 0.6)]
    me = piece.data
    P = np.array([v.co for v in me.vertices])
    # the tunic's skirt (tunic paint below the belt) hangs from the skirt; all else from the suit
    use_skirt = np.zeros(len(P), bool)
    if LM is not None:
        tun = [i for i, m in enumerate(me.materials) if m.name.split(".")[0] == "tunic"]
        on_tunic = np.zeros(len(P), bool)
        for p_ in me.polygons:
            if p_.material_index in tun:
                on_tunic[list(p_.vertices)] = True
        use_skirt = on_tunic & (P[:, 2] < LM["belt"] - 0.01) & (P[:, 2] > LM["hem"] - 0.05)
    out = np.zeros((len(P), len(names)))
    for i, co in enumerate(P):
        bvh, V, T, W = srcs[1 if use_skirt[i] else 0]
        loc, nrm, fi, d = bvh.find_nearest(co)
        if fi is None:
            bvh, V, T, W = srcs[0]
            loc, nrm, fi, d = bvh.find_nearest(co)
        a, b, c = V[T[fi]]
        q = np.array(loc)
        v0, v1, v2 = b - a, c - a, q - a
        d00, d01, d11, d20, d21 = v0 @ v0, v0 @ v1, v1 @ v1, v2 @ v0, v2 @ v1
        den = max(d00 * d11 - d01 * d01, 1e-12)
        wb = (d11 * d20 - d01 * d21) / den
        wc = (d00 * d21 - d01 * d20) / den
        bary = np.clip(np.array((1.0 - wb - wc, wb, wc)), 0.0, 1.0)
        bary /= bary.sum()
        out[i] = bary @ W[T[fi]]
    # evened out along the uniform: a few steps toward the mean of its neighbours
    Ed = np.zeros(len(me.edges) * 2, np.int64)
    me.edges.foreach_get("vertices", Ed)
    Ed = Ed.reshape(-1, 2)
    cnt = np.bincount(Ed.ravel(), minlength=len(P)).astype(float)[:, None]
    for _ in range(3):
        acc = np.zeros_like(out)
        np.add.at(acc, Ed[:, 0], out[Ed[:, 1]])
        np.add.at(acc, Ed[:, 1], out[Ed[:, 0]])
        out = 0.5 * out + 0.5 * acc / np.maximum(cnt, 1.0)
    set_weights(piece, out, names, 4, 0.02)


def thin(ob, tris, spare=0.0):
    """Decimate a mesh object to about `tris` triangles (its weights and paints go with it). With `spare`, the sharply
    curved places (a knuckle, the cleft between two fingers) are spared by that much and the broad ones thinned more."""
    have = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    if have <= tris:
        return ob
    mod = ob.modifiers.new("thin", "DECIMATE")
    mod.ratio = tris / have
    if spare > 0.0:
        me = ob.data
        Nv = np.zeros(len(me.vertices) * 3)
        me.vertices.foreach_get("normal", Nv)
        Nv = Nv.reshape(-1, 3)
        Ed = np.zeros(len(me.edges) * 2, np.int64)
        me.edges.foreach_get("vertices", Ed)
        Ed = Ed.reshape(-1, 2)
        turn = 1.0 - (Nv[Ed[:, 0]] * Nv[Ed[:, 1]]).sum(axis=1)
        k = np.zeros(len(Nv))
        np.maximum.at(k, Ed[:, 0], turn)
        np.maximum.at(k, Ed[:, 1], turn)
        sharp = np.clip((k - 0.01) / 0.08, 0.0, 1.0)
        vg = ob.vertex_groups.new(name="spare")
        for i in range(len(Nv)):
            vg.add([i], float(1.0 - 0.95 * sharp[i]), "REPLACE")
        mod.vertex_group = vg.name
        mod.vertex_group_factor = spare
    bpy.context.view_layer.objects.active = ob
    with bpy.context.temp_override(object=ob, active_object=ob):
        bpy.ops.object.modifier_apply(modifier=mod.name)
    g = ob.vertex_groups.get("spare")
    if g is not None:
        ob.vertex_groups.remove(g)
    ob.data.validate()
    return ob


def finger_bones(man, s):
    """The hand's finger bones (MakeHuman's joints, as curled): [(finger, joint, head, tail)], finger 1 the thumb."""
    S = s.upper()
    mj = man["mh_joints"]
    return [(f, k, np.array(mj[f"finger{f}-{k}.{S}____head"]), np.array(mj[f"finger{f}-{k}.{S}____tail"]))
            for f in range(1, 6) for k in range(1, 4)]


def _near_bone(P, bones):
    """For each point, the nearest finger bone: its index, the point on it, how far along it (0..1)."""
    best = np.full(len(P), np.inf)
    idx = np.zeros(len(P), int)
    Q = np.zeros_like(P)
    T = np.zeros(len(P))
    for i, (f, k, a, b) in enumerate(bones):
        ab = b - a
        t = np.clip(((P - a) @ ab) / (ab @ ab), 0.0, 1.0)
        q = a + t[:, None] * ab
        d = np.linalg.norm(P - q, axis=1)
        m = d < best
        best[m], idx[m], Q[m], T[m] = d[m], i, q[m], t[m]
    return idx, Q, T, best


def slim_fingers(ob, man, s, by=0.17):
    """MakeHuman's fingers drawn in a little toward their bones (a soldier's hands, not a mitten), from the
    knuckles out."""
    me = ob.data
    P = np.array([v.co for v in me.vertices])
    bones = finger_bones(man, s)
    idx, Q, T, d = _near_bone(P, bones)
    kk = np.array([bones[i][1] for i in idx])
    ff = np.array([bones[i][0] for i in idx])
    w = np.where(kk > 1, 1.0, np.clip((T - 0.25) / 0.4, 0.0, 1.0))
    w = np.where(ff == 1, w * (kk > 1), w)               # (the thumb from its middle joint out)
    w *= d < 0.016
    P = Q + (P - Q) * (1.0 - by * w)[:, None]
    me.vertices.foreach_set("co", P.astype(np.float32).ravel())
    me.update()


def hand_marks(P, man):
    """The nails and the knuckles, as two amounts at each point near a finger (0 elsewhere): for the pictures' skin."""
    nail = np.zeros(len(P), np.float32)
    knuckle = np.zeros(len(P), np.float32)
    for s in ("l", "r"):
        bones = finger_bones(man, s)
        palm = np.mean([b[2] for b in bones if b[1] == 1 and b[0] > 1], axis=0)
        near = np.nonzero(np.linalg.norm(P - palm, axis=1) < 0.16)[0]
        if not len(near):
            continue
        idx, Q, T, d = _near_bone(P[near], bones)
        for j, (i, q, t, dist) in enumerate(zip(idx, Q, T, d)):
            f, k, a, b = bones[i]
            if dist > 0.02:
                continue
            ax = _n(b - a)
            to_palm = palm - q
            to_palm = to_palm - ax * (to_palm @ ax)
            if np.linalg.norm(to_palm) < 1e-6:
                continue
            back = -_n(to_palm)                                   # (the back of the finger: away from the palm)
            out = P[near[j]] - q
            c = (out @ back) / max(np.linalg.norm(out), 1e-9)
            if k == 3:
                nail[near[j]] = float(np.clip((t - 0.42) / 0.14, 0, 1) * np.clip((c - 0.35) / 0.25, 0, 1))
            if k in (1, 2) and not (f == 1 and k == 1):
                knuckle[near[j]] = max(knuckle[near[j]], float(np.exp(-(t / 0.22) ** 2) * np.clip((c - 0.2) / 0.4, 0, 1)))
    return nail, knuckle


def skin_parts(man, layers, nation):
    """What shows of MakeHuman's skin under the uniform: the head and neck, and the hands, each thinned to its budget,
    and the eyeballs. Returns objects already weighted (MakeHuman's weights on our bones)."""
    body = man["body"]
    for m in list(body.modifiers):
        body.modifiers.remove(m)
    me = body.data
    V = np.array([v.co for v in me.vertices])
    d = np.min([L.at_points(V) for L in layers], axis=0)
    hidden = d < -0.003
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.verts.ensure_lookup_table()
    gone = [f for f in bm.faces if all(hidden[v.index] for v in f.verts)]
    bmesh.ops.delete(bm, geom=gone, context="FACES")
    bm.to_mesh(me)
    bm.free()
    me.update()
    # split what is left into islands: the head with the neck, and each hand (crumbs under the cloth go)
    bpy.context.view_layer.objects.active = body
    for o in bpy.context.selected_objects:
        o.select_set(False)
    body.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.separate(type="LOOSE")
    bpy.ops.object.mode_set(mode="OBJECT")
    parts = [o for o in bpy.context.selected_objects if o.type == "MESH"]
    keep = []
    for o in parts:
        n = len(o.data.polygons)
        if n < 40:
            bpy.data.objects.remove(o)
            continue
        z = np.mean([v.co.z for v in o.data.vertices])
        y = np.mean([v.co.y for v in o.data.vertices])
        print(f"    skin island at z {z:.2f}: {n} faces")
        if z > 1.3:
            o.name = "head"
            thin(o, BUDGET["head"])
        else:
            o.name = "hand_" + ("l" if y > 0 else "r")
            slim_fingers(o, man, "l" if y > 0 else "r")
            thin(o, BUDGET["hand"], spare=0.002)
        keep.append(o)
    for o in keep:
        o.data.materials.clear()
        o.data.materials.append(paint_material("skin"))
        for p in o.data.polygons:
            p.material_index = 0
            p.use_smooth = True
    for e in man["eyes"]:
        for m in list(e.modifiers):
            e.modifiers.remove(m)
        e.data.materials.clear()
        e.data.materials.append(paint_material("eye"))
        keep.append(e)
    return keep


def join_meshes(objs, name):
    """One mesh object of several (weights and paints kept, by name)."""
    for o in bpy.context.selected_objects:
        o.select_set(False)
    for o in objs:
        mw = o.matrix_world.copy()
        o.parent = None
        o.matrix_world = mw
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    ob = objs[0]
    ob.name = ob.data.name = name
    return ob


def body_attributes(body, J, man=None):
    """The skin's `scalp` (how much short hair, from MakeHuman's group) and the eyes' `look` (how near a point of an
    eyeball is to its front: the iris and pupil are painted by it in the pictures), as point attributes."""
    me = body.data
    n = len(me.vertices)
    scalp = np.zeros(n, np.float32)
    g = body.vertex_groups.get("scalp")
    if g is not None:
        for v in me.vertices:
            for ge in v.groups:
                if ge.group == g.index:
                    scalp[v.index] = ge.weight
        body.vertex_groups.remove(g)
    look = np.zeros(n, np.float32)
    eye_slot = [i for i, m in enumerate(me.materials) if m.name.split(".")[0] == "eye"]
    if eye_slot:
        idx = {v for p in me.polygons if p.material_index in eye_slot for v in p.vertices}
        for i in idx:
            co = np.array(me.vertices[i].co)
            c = np.array(J["eye_l"] if co[1] > 0 else J["eye_r"])
            look[i] = _n(co - c)[0]
    co_ = np.zeros(n * 3, np.float32)
    me.vertices.foreach_get("co", co_)
    nail, knuckle = hand_marks(co_.reshape(-1, 3).astype(float), man) if man else (np.zeros(n, np.float32),) * 2
    for nm, vals in (("scalp", scalp), ("look", look), ("nail", nail), ("knuckle", knuckle)):
        if nm not in me.attributes:
            me.attributes.new(nm, "FLOAT", "POINT")
        me.attributes[nm].data.foreach_set("value", vals)
    # each point's place about the middle of the eyes (the skin's marks are drawn from it in the pictures)
    mid = (np.array(J["eye_l"]) + np.array(J["eye_r"])) / 2
    co = np.zeros(n * 3, np.float32)
    me.vertices.foreach_get("co", co)
    face = (co.reshape(-1, 3) - mid).astype(np.float32)
    if "face" not in me.attributes:
        me.attributes.new("face", "FLOAT_VECTOR", "POINT")
    me.attributes["face"].data.foreach_set("vector", face.ravel())


# ================================================================================================ the man, put together
def assemble(nation, weapon="rifle", seed=None):
    """Dress, arm and rig one soldier of a nation on MakeHuman's man (his face by `seed`; by default his army's own).
    Returns a dict of his objects and his Rig."""
    spec = NATION_KIT[nation]
    LOOK_NOW.update(LOOK.get(nation, LOOK["GER"]))
    pt = puttee_spec(spec)
    if pt and pt["colour"]:
        LOOK_NOW["lower"] = pt["colour"]
    t0 = time.time()
    man = mh_body(nation, seed)
    J = joints_of(man)
    fit_body(J)
    LM = landmarks(J)
    ls = uniform_layers(man, J, spec, LM)
    cloth = mesh_from_layers("uniform", ls, 0.0034, BUDGET["uniform"] + (BUDGET["puttees"] if pt and pt["turns"] else 0), cut_paints=True,
                             detail=DETAIL)
    uniform_weights(cloth, man, LM)
    parts = skin_parts(man, ls, nation)
    body = join_meshes([cloth] + parts, "body_" + nation)
    body_attributes(body, J, man)
    kitp = build_kit(J, spec, ls, LM["Z"])
    helmet = build_helmet(spec["helmet"], J)
    if weapon == "rifle":
        gun, part = build_rifle()
        part_bone = "bolt"
    else:
        gun, part = build_lmg()
        part_bone = "bipod"
    mrest = mocap.Rest.of(man["armature"])                  # (human.py's own rest: the mocap clips are measured on it)
    for ob in (man["tights"], man["skirt"], man["armature"]):
        bpy.data.objects.remove(ob)
    print(f"  {nation}: dressed in {time.time() - t0:.1f} s ({tris_of(body)} triangles of body and uniform)")
    arm = build_armature(J)
    arm.name = "soldier_" + nation
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
    Wm = Matrix.Translation(Vector(arm.data.bones["weapon"].head_local))
    for ob, bone in ((gun, "weapon"), (part, part_bone)):
        ob.data.transform(Wm)
        skin_rigid(ob, bone)
        attach(ob, arm)
    mark_rest(body, dusty=True)
    for ob in kit_objs + [helmet, gun, part]:
        mark_rest(ob)
    rig = Rig(arm, J)
    rig.mrest, rig.weapon_kind = mrest, weapon
    g0 = np.array(arm.data.bones["weapon"].head_local)
    sockets = [add_socket(arm, "muzzle", "weapon", tuple(g0 + np.array(MUZZLE))),
               add_socket(arm, "hand_r", "hand_r", tuple(grip_point(J, "r")), tuple(rig.hand0["r"][0])),
               add_socket(arm, "head", "head", J["head"]),
               add_socket(arm, "chest", "chest", (J["chest"][0] + 0.15, 0.0, LM["Z"](1.3))),
               add_socket(arm, "weapon", "weapon", tuple(g0))]
    return {"nation": nation, "arm": arm, "rig": rig, "body": body, "kit": kit_objs, "helmet": helmet, "weapon": gun,
            "weapon_part": part, "sockets": sockets, "J": J, "LM": LM, "spec": spec, "weapon_kind": weapon, "seed": man["seed"]}


def assemble_sculpted(nation, weapon="rifle", J=None):
    """The first pass (kept for the before-and-after picture): the man sculpted from numbers, dressed, armed and rigged.
    Returns a dict of his objects and his Rig."""
    J = J or joints()
    fit_body(J)
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


def studio_smooth(m):
    """For the pictures only (never in the exports): one level of subdivision when rendered, on the body and uniform,
    the helmet and the kit's round things, so no curve shows a facet at close range."""
    for ob in [m["body"], m["helmet"]] + [k for k in m["kit"] if k.name.split(".")[0] in ("bottle", "canister", "bread_bag", "pack", "roll",
                                                                                          "pouches", "shovel")]:
        if not any(md.type == "SUBSURF" for md in ob.modifiers):
            md = ob.modifiers.new("studio_smooth", "SUBSURF")
            md.levels = 0
            md.render_levels = 1
            md.quality = 2
            # (a sharp edge of the mesh, a hem's or a flap's, is creased so the smoothing does not melt it)
            me = ob.data
            bm = bmesh.new()
            bm.from_mesh(me)
            cr = np.zeros(len(bm.edges), np.float32)
            for e in bm.edges:
                if len(e.link_faces) == 2:
                    a = e.calc_face_angle(0.0)
                    cr[e.index] = float(np.clip((a - math.radians(50.0)) / math.radians(20.0), 0.0, 0.85))
            bm.free()
            if "crease_edge" not in me.attributes:
                me.attributes.new("crease_edge", "FLOAT", "EDGE")
            me.attributes["crease_edge"].data.foreach_set("value", cr)


def man_objects(m):
    return [m["body"]] + m["kit"] + [m["helmet"], m["weapon"], m["weapon_part"]]


# ================================================================================================ the clips
FPS = 60          # (every clip's length is a whole number of sixtieths: 0.65 s is 39 frames)

# How each hand holds each weapon at the port (the rifle across the body), in the weapon's own frame: `a` from the
# wrist to the knuckles, `n` the way the palm faces. Both are power grips round the wood, found by scanning against
# rom.py's limits with the elbows hanging: the right hand round the stock's wrist from in front, its palm toward the
# body and the knuckles out; the left round the fore-end from below and to the left, the fingers wrapped over it.
HOLD = {
    "r": {"grip": "grip_r", "a": (0.342, -0.94, 0.0), "n": (0.0, 0.0, -1.0), "pole": (-0.3, -0.75, -0.55),
          "roll": 10.0},
    "l": {"grip": "grip_l", "a": (0.342, 0.0, 0.94), "n": (0.0, 1.0, 0.0), "pole": (0.0, 0.45, -1.0), "roll": 10.0},
}
# Aimed (the butt in the shoulder's pocket, the cheek on the comb): the right hand in a power grip round the half-pistol
# grip (its line taken 65 degrees up from the bore, the thumb's end forward), the palm on its right side facing in,
# the hand's axis 57 degrees off the grip's line, so the forearm comes forward and in from an elbow out at the side
# and below the shoulder (upper arm 60-70 degrees out) with the wrist near straight; the left hand under the
# fore-end, the wood across its palm, the elbow under the rifle. (Found by scanning grips against rom.py's limits:
# a hand square across the stock's wrist cannot be reached from a lowered elbow within them.)
GRIP_LINE_R = (0.423, 0.0, 0.906)
HOLD_AIM = {
    "r": {"grip": "grip_r", "a": (0.888, 0.419, 0.187), "n": (-0.453, 0.866, 0.211), "pole": (-0.25, -1.0, -0.75),
          "roll": 15.0, "roll_axis": GRIP_LINE_R},
    "l": {"grip": "grip_l", "a": (0.89, -0.411, 0.199), "n": (-0.15, 0.15, 0.98), "pole_world": (0.15, 0.35, -1.0), "roll": 50.0},
}
ARM_COMFORT = True         # (every pose's arms through comfortable_arm unless the pose says comfort=False)
POCKET = (0.075, 0.06, 0.02)         # the right shoulder's pocket at rest from the shoulder's joint: forward, in, up (m)
                                     # (just under the collarbone, between the deltoid and the chest: level with the joint)
BUTT_AT = {"rifle": (-0.324, 0.0, -0.06), "lmg": (-0.358, 0.0, -0.025)}    # where the pocket bears on each butt plate
                                     # (the rifle's low on its plate: the heel stands up about level with the shoulder)
SIGHT_RELIEF = (-0.22, -0.06)      # how far behind the stock's wrist the eye may sit on the sights' line (m)
# The head on the sights as a rifleman holds it (the coordinator's review, 18 November): the rifle lifted to the face,
# the head only 10-20 degrees forward and canted a little right, the right cheekbone over the comb; the butt may
# ease a centimetre or so about the shoulder's pocket (soft, a few centimetres across) to meet the eye.
SIGHT_PITCH = (10.0, 20.0)         # the head's forward tilt off upright in the plane of the bore (degrees)
SIGHT_CANT = (2.0, 12.0)           # its cant to his right, onto the comb (degrees)
SIGHT_SLIDE = 0.012                # (m) how far the butt may ease across or up and down in the pocket
SIGHT_NECK = (-55.0, 45.0)         # the neck's bend chest to head kept inside rom.py's range (extension 60, flexion 50)
SIGHT_NOD = (-15.0, 20.0)          # the head's own nod on the neck's top (back -, forward +): no craned, turtled head
REAR_NOTCH = {"rifle": 0.177, "lmg": 0.11}       # the rear sight's notch along each weapon (m; build_rifle, build_lmg)
COMB_Z = {"rifle": 0.035, "lmg": 0.05}           # the comb's top under the cheek (m over the bore's axis)
CHEEK = (-0.013, -0.019, -0.037)                 # the right cheekbone's underside from the right eye, at rest (m)


# The arms at the port as rom.py measures them (elevation, plane, rotation, elbow, pronation, wrist flexion, radial
# deviation; see Rig.arm_place): a key that lets go of the rifle starts from these, so the arm moves angle by angle.
PORT_ARMS = {"l": (14.8, 14.2, 28.9, 124.8, -46.1, -30.8, -0.6), "r": (25.9, -16.7, 31.1, 94.4, -18.6, 47.8, -8.0)}


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


def aim(yaw=0.0, pitch=0.0, relief=None, kick=0.0, rise=0.0, cant=0.0):
    """The rifle shouldered and aimed along the bearing given: its butt in the right shoulder's pocket (just inside the
    shoulder's joint and under the collarbone, riding the clavicle), the rifle low on its butt plate so it stands up to
    the face; the head, only a little forward and canted, meets it with the eye on the sights' line (the pose's
    `sight`; see Rig.head_to_sight, which may ease the butt a little in the pocket). (`relief` is kept for old
    callers: the eye's place on the line is the head's to find.)"""
    def place(rig, st):
        R = R_axis((0, 0, 1), yaw) @ R_axis((0, 1, 0), -(pitch + rise)) @ R_axis((1, 0, 0), cant)
        o = rig.pocket(st) - R @ np.array(BUTT_AT["rifle"]) + R[:, 0] * kick
        return M4(R, o)
    return place


def swing_arm(swing, out=10.0, rot=20.0, elbow=90.0, pron=0.0, wf=0.0, wd=0.0):
    """Arm angles (Rig.arm_place) for an arm swung `swing` degrees forward (- back) in his body's own fore-and-aft
    plane, `out` degrees out to the side: the swing of a walking or running arm, passing smoothly under the shoulder."""
    sw, o = math.radians(swing), math.radians(out)
    u = np.array((math.sin(sw) * math.cos(o), math.sin(o), -math.cos(sw) * math.cos(o)))
    return (math.degrees(math.acos(-u[2])), math.degrees(math.atan2(u[0], u[1])), rot, elbow, pron, wf, wd)


def in_left_hand(arm, hold=None, side=1.0):
    """The rifle where the left hand (side -1: the right) has it, that arm placed by its own angles (Rig.arm_place):
    the rifle carried in one hand follows the arm, rather than the arm being bent to wherever a rifle was put."""
    h = hold or HOLD["l" if side > 0 else "r"]

    def place(rig, st):
        g, a_w, n_w, _ = rig.arm_place(st, arm, side)
        R = frame2(a_w, n_w) @ frame2(h["a"], h["n"]).T
        return M4(R, g - R @ (np.array(WEAPONS["rifle"][h["grip"]]) + np.asarray(h.get("slide", (0, 0, 0)), float)))
    return place


def stand_feet(ahead=0.05, apart=0.12, back=-0.06):
    return {"foot_l": {"at": (ahead, apart, ANKLE), "yaw": 8.0}, "foot_r": {"at": (back, -apart, ANKLE), "yaw": -12.0}}


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


def picture_before_after(m, out, fast=False, first=None):
    """Today's lay figure (Rifleman_Stahl, from the game's file), the first pass (`first`: the soldier sculpted from
    numbers) and the new German soldier on MakeHuman's man, side by side: same light, same camera, same stance."""
    sc = bpy.context.scene
    men = [m] + ([first] if first else [])
    old = load_game_model(os.path.join(REPO, "Content", "Models", "Rifleman_Stahl.json"), LOOK["GER"], "Rifleman_Stahl")
    old.location = (0.0, -1.0 if first else -0.62, 0.0)
    turn = math.radians(-16.0) if first else 0.0       # (three abreast, each turned a little to the camera's left)
    old.rotation_euler = (0.0, 0.0, turn)
    for man, y in ((m, 1.0 if first else 0.62), (first, 0.0)):
        if man is None:
            continue
        fit_body(man["J"])                              # (each stands on his own legs' measures)
        man["arm"].location = (0.0, y, 0.0)
        man["arm"].rotation_euler = (0.0, 0.0, turn)
        rig = man["rig"]
        P = base_stand()
        P["foot_l"] = {"at": (0.17, 0.11, ANKLE), "yaw": 5.0}
        P["foot_r"] = {"at": (-0.1, -0.12, ANKLE), "yaw": -10.0, "pitch": 0.0}
        P["pelvis_at"] = (0.02, 0.0, -0.02)
        rig.key(rig.solve(P), 1)
    fit_body(m["J"])
    sc.frame_set(1)
    kit.studio()
    ground = kit.backdrop("CFC6B4")
    cam = kit.camera(lens=50)
    if first:
        cam.data.lens = 60
        kit.orbit(cam, (0.0, 0.0, 0.88), 7.6, 5.0, 0.0)
    else:
        kit.orbit(cam, (0.0, 0.0, 0.9), 5.6, 6.0, 22.0)
    shared_render(640 if fast else 1280, 360 if fast else 720, 16 if fast else 48)
    t = time.time()
    kit.still(out)
    print(f"  before-after: {time.time() - t:.0f} s")
    W = 640 if fast else 1280
    if first:
        size = 13 if fast else 26
        font = kit._font(size)
        texts = []
        for cx, t in ((1 / 6, "Today: the lay figure"), (1 / 2, "First pass: sculpted"), (5 / 6, "New: MakeHuman body")):
            texts.append((int(W * cx - font.getlength(t) / 2), int(W * 0.025), t))
        label(out, texts, size=size)
    else:
        label(out, [(int(W * 0.05), int(W * 0.03), "Today: the lay figure (Rifleman_Stahl)"),
                    (int(W * 0.56), int(W * 0.03), "New: the German soldier")], size=13 if fast else 26)
    bpy.data.objects.remove(old)
    bpy.data.objects.remove(ground)
    for man in men:
        man["arm"].location = (0.0, 0.0, 0.0)
        man["arm"].rotation_euler = (0.0, 0.0, 0.0)
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
            if all(isinstance(v, dict) and "arm" in v and "blend" not in v for w, v in vals) and len(vals) == len(pairs):
                # (arms keyed by their own angles mix angle by angle, so the arm passes through real arms between)
                n = max(len(v["arm"]) for w, v in vals)
                out[k] = {"arm": tuple(sum(w * float((tuple(v["arm"]) + (0.0, 0.0))[i]) for w, v in vals) for i in range(n))}
            else:
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
# (the measures of the man the clips are posed on: set by fit_body from his joints; these are the sculpted man's)
ANKLE = 0.088                                    # the ankle's height above the ground, standing in boots
FOOT_HEEL = np.array((-0.061, 0.0, -0.088))      # the heel's ground point from the ankle
FOOT_BALL = np.array((0.146, 0.0, -0.088))       # the ball's
LEG_REACH = 0.822                                # how far a leg reaches from the hip, all but straight
HIPS = {"l": (0.0, 0.09, 0.915), "r": (0.0, -0.09, 0.915)}
PRONE_HEIGHT = 0.135                             # the pelvis's height lying prone


def fit_body(J):
    """Fit the clips' measures to the man they are posed on (his ankles, hips and legs) and remake the gaits."""
    global ANKLE, FOOT_HEEL, FOOT_BALL, LEG_REACH, HIPS, PRONE_Z, WALK, RUN, FLEE
    ANKLE = float(J["foot_l"][2])
    FOOT_HEEL = np.array((-0.061, 0.0, -ANKLE))
    FOOT_BALL = np.array((0.146, 0.0, -ANKLE))
    lt = np.linalg.norm(np.array(J["calf_l"]) - np.array(J["thigh_l"]))
    lc = np.linalg.norm(np.array(J["foot_l"]) - np.array(J["calf_l"]))
    LEG_REACH = 0.99 * float(lt + lc)
    HIPS = {s: tuple(J["thigh_" + s]) for s in ("l", "r")}
    PRONE_Z = PRONE_HEIGHT - 0.95                  # (the rig scales the pelvis's drop to the man: see Rig.fit)
    WALK, RUN, FLEE = make_gaits()


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
            hip = np.array(HIPS[s])
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


def make_gaits():
    return (Gait(1.1, 1.5, 0.58, 0.07, center=-0.085, width=0.105, heel=-16.0, push=52.0, flat=(0.12, 0.45), base=-0.012, bob=0.012),
            Gait(0.7, 2.45, 0.35, 0.17, center=-0.08, width=0.1, heel=-6.0, push=56.0, flat=(0.1, 0.35), base=-0.07, bob=0.02, lean=0.05),
            Gait(0.65, 2.8, 0.33, 0.2, center=-0.095, width=0.1, heel=-4.0, push=58.0, flat=(0.1, 0.33), base=-0.045, bob=0.025,
                 right=0.32 / 0.65, lean=0.03))


WALK, RUN, FLEE = make_gaits()


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
    # the right hand trails the rifle by its middle, the arm hanging and swinging with the stride (the rifle goes where
    # that hand has it); the left arm pumps against the left leg, the elbow bent; both keyed by the arms' own angles
    arm_r = swing_arm(18.0 * sw, out=9.0, rot=10.0, elbow=25.0 + 8.0 * max(sw, 0.0), pron=0.0, wf=10.0)
    P["weapon"] = in_left_hand(arm_r, {"grip": "carry", "a": (0.0, 0.25, -0.97), "n": (0.0, 1.0, 0.0)}, side=-1.0)
    P["hand_r"] = {"arm": arm_r}
    f = -sw
    P["hand_l"] = {"arm": swing_arm(42.0 * max(f, 0.0) - 32.0 * max(-f, 0.0), out=12.0, rot=25.0, elbow=95.0 + 15.0 * max(f, 0.0),
                                    pron=-5.0)}
    P["fingers_l"] = 70.0
    P["thumb_l"] = 40.0
    P["fingers_r"] = 45.0
    return P


# ---- standing, kneeling and lying to fire
# The bolt knob taken from beneath, the palm up and in, the fingers under it to lift it: the aimed grip turned 60
# degrees round the bore and 20 to the right, so the hand goes from one to the other without a twist past its range.
HOLD_KNOB = {"grip": "knob", "a": (0.851, -0.259, 0.456), "n": (-0.34, 0.39, 0.855), "pole": (-0.3, -0.9, -0.3), "roll": 20.0}


def aim_stand(t=0.0):
    """Standing to fire: bladed a third of a turn to the right, left foot forward, leaning a little into the rifle, the
    right shoulder up, the rifle lifted to the face: the cheek on the comb, the head 10-20 degrees forward and canted a
    little right, the eye on the sights' line."""
    P = {"pelvis_at": (-0.02, 0.0, -0.03), "pelvis": (2.0, 0.0, -30.0), "spine_01": (3.0, 0.0, -2.0), "spine_02": (4.0, 0.0, -4.0),
         "chest": (5.0, -2.0, -4.0), "neck": (4.0, -3.0, 16.0), "head": (8.0, -11.0, 17.0),
         "clavicle_l": (0.0, 12.0), "clavicle_r": (20.0, 10.0),
         "foot_l": {"at": (0.2, 0.14, ANKLE), "yaw": -12.0}, "foot_r": {"at": (-0.23, -0.14, ANKLE), "yaw": -55.0},
         "weapon": aim(), "sight": 1.0, "hand_l": dict(HOLD_AIM["l"]), "hand_r": dict(HOLD_AIM["r"]),
         "fingers_l": 30.0, "fingers_r": 30.0, "thumb_l": 25.0, "thumb_r": 35.0}
    return P


def kneel_base(t=0.0, aimed=True):
    """On the right knee, sitting back toward the heel, the left foot forward with its knee up; aimed, the left
    elbow rests on the left knee."""
    P = {"pelvis_at": (-0.2, 0.0, -0.49), "pelvis": (6.0, 0.0, -24.0), "spine_01": (4.0, 0.0, -2.0), "spine_02": (5.0, 0.0, -4.0),
         "chest": (5.0, -2.0, -5.0), "neck": (2.0, -3.0, 14.0), "head": (7.0, -11.0, 14.0),
         "clavicle_l": (0.0, 14.0), "clavicle_r": (20.0, 10.0),
         "foot_l": {"at": (0.3, 0.17, ANKLE), "yaw": -10.0, "pole": (1.0, 0.25, 0.3)},
         "foot_r": {"at": (-0.5, -0.2, 0.105), "yaw": -40.0, "pitch": 72.0, "toe": 62.0, "pole": (1.0, -0.2, -0.6)},
         "weapon": aim(), "sight": 1.0, "hand_l": dict(HOLD_AIM["l"], pole_world=(0.3, 0.3, -1.0)), "hand_r": dict(HOLD_AIM["r"]),
         "fingers_l": 30.0, "fingers_r": 30.0, "thumb_l": 25.0, "thumb_r": 35.0}
    if not aimed:
        P.update({"neck": (-2.0, 0.0, 6.0), "head": (-2.0, 0.0, 8.0), "weapon": port_arms(lift=0.05, out=0.02), "sight": 0.0,
                  "hand_l": held("l"), "hand_r": held("r"), "clavicle_l": (0.0, 4.0), "clavicle_r": (0.0, 4.0),
                  "chest": (8.0, 0.0, 4.0), "spine_02": (5.0, 0.0, 4.0)})
    return P


PRONE_Z = -0.815


def prone_base(t=0.0, arch=0.5, weapon="rifle"):
    """Lying to fire: the body a little to the right of the line of fire, legs apart, the feet lying toes down with the
    heels turned out, down on the elbows with the back a little arched, the head up to look along the sights (the
    right shoulder shrugged up and drawn back to lift the butt to the face; the head may tip back further on the neck
    than standing, as a man lying on his elbows must)."""
    P = {"pelvis_at": (0.0, 0.03, PRONE_Z), "pelvis": (86.0, 2.0, -9.0), "spine_01": (-7.0 * arch, 0.0, 3.0),
         "spine_02": (-10.0 * arch, 0.0, 3.0), "chest": (-12.0 * arch, -5.0, 3.0), "neck": (-32.0, -1.0, 4.0), "head": (-22.0, -9.0, 5.0),
         "clavicle_l": (8.0, 14.0), "clavicle_r": (24.0, -10.0),
         "foot_l": {"at": (-0.8, 0.3, 0.125), "yaw": 22.0, "pitch": 132.0, "roll": -12.0, "toe": 12.0, "pole": (-0.1, 0.3, -1.0)},
         "foot_r": {"at": (-0.82, -0.28, 0.125), "yaw": -28.0, "pitch": 132.0, "roll": 12.0, "toe": 12.0, "pole": (-0.1, -0.3, -1.0)},
         "weapon": aim(), "weapon_kind": weapon, "sight": 1.0,
         "hand_l": dict(HOLD_AIM["l"], slide=(-0.21, 0.0, 0.0), pole_world=(0.0, 0.3, -1.0)),
         "hand_r": dict(HOLD_AIM["r"], pole_world=(-0.1, -1.0, -1.3)),
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


def bolt_cycle(P, t, t0, t1, ease=0.0):
    """The bolt worked: the right hand leaves the grip for the knob, lifts it, draws it back, drives it home, turns
    it down, and returns to the grip, between t0 and t1 (the lift and the turn down each over a tenth of a second or
    more, so the wrist never turns faster than a real one). `ease` (m): the rifle eased forward out of the shoulder
    meanwhile (lying down, so the bolt drawn back does not crowd the shoulder past the elbow's bend)."""
    L = t1 - t0
    reach = ramp(t, t0 - 0.13, t0 + 0.01) * (1 - ramp(t, t1 - 0.06, t1 + 0.08))
    lift = 62.0 * (ramp(t, t0 + 0.0, t0 + 0.3 * L) - ramp(t, t0 + 0.66 * L, t0 + 0.94 * L))
    back = 0.085 * (ramp(t, t0 + 0.3 * L, t0 + 0.48 * L) - ramp(t, t0 + 0.5 * L, t0 + 0.66 * L))
    P["bolt"] = (back, lift)
    if reach > 0:
        P["hand_r"] = {"blend": [(1 - reach, P["hand_r"]), (reach, HOLD_KNOB)]}
        P["fingers_r"] = P.get("fingers_r", 30.0) * (1 - reach) + 60.0 * reach
        P["head"] = add3(P.get("head", (0, 0, 0)), (-3.0 * reach, 2.0 * reach, 0.0))
        if ease:
            P["weapon"] = _kicked(P["weapon"], -ease * reach, 0.0)
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
    return bolt_cycle(P, t, 0.6, 1.0, ease=0.09)


UPPER = ("weapon", "hand_l", "hand_r", "sight", "neck", "head", "clavicle_l", "clavicle_r", "chest", "spine_02",
         "fingers_l", "fingers_r", "thumb_l", "thumb_r")


def upper_toward(P, Q, w):
    """P with its upper body (the rifle, the hands, the head, the shoulders) taken a fraction w of the way to Q's."""
    if w <= 1e-6:
        return P
    U = mix([P, Q], [1.0 - w, w])
    out = dict(P)
    out.update({k: U[k] for k in UPPER if k in U})
    return out


@clip("kneel_down", 0.5, events=[ev(0.4, "knee_down", bone="calf_r")])
def c_kneel_down(t):
    """Down on the right knee, the rifle brought up to the shoulder on the way down (over 0.4 s: a man cannot shoulder
    it in a tenth, and the wrist would turn faster than a real one)."""
    mid = kneel_base(aimed=False)
    mid = dict(mid, pelvis_at=(-0.1, 0.0, -0.3), foot_r={"at": (-0.3, -0.18, 0.2), "yaw": -25.0, "pitch": 40.0, "toe": 20.0,
                                                          "pole": (1.0, -0.2, -0.3)})
    landed = kneel_base(aimed=False)
    landed["pelvis_at"] = (-0.19, 0.0, -0.475)
    P = keyed([(0.0, base_stand()), (0.2, mid), (0.4, landed), (0.5, kneel_base())])(t)
    return upper_toward(P, kneel_base(), ramp(t, 0.0, 0.5))


@clip("stand_up", 0.6, events=[ev(0.12, "kit_rattle", bone="chest")])
def c_stand_up(t):
    low = kneel_base(aimed=False)
    mid = dict(kneel_base(aimed=False), pelvis_at=(-0.08, 0.0, -0.26), foot_r={"at": (-0.2, -0.16, 0.16), "yaw": -20.0,
                                                                               "pitch": 25.0, "toe": 10.0})
    P = keyed([(0.0, kneel_base()), (0.15, low), (0.38, mid), (0.6, base_stand())])(t)
    return upper_toward(P, kneel_base(), 1.0 - ramp(t, 0.0, 0.5))


@clip("go_prone", 1.0, events=[ev(0.32, "knee_down", bone="calf_r"), ev(0.75, "body_fall", bone="spine_02")])
def c_go_prone(t):
    """Down on the right knee, then forward onto the elbows, the legs thrown back, the rifle kept up out of the dirt."""
    knee = kneel_base(aimed=False)
    knee.update({"pelvis_at": (-0.12, 0.0, -0.47), "spine_01": (10.0, 0, 0), "spine_02": (12.0, 0, 0), "chest": (12.0, 0, 0),
                 "foot_l": {"at": (0.32, 0.16, ANKLE), "yaw": -5.0, "pole": (1.0, 0.25, 0.3)},
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


def bipod_foot():
    """The machine gun's bipod feet (their middle, on the ground) in the gun's own space, the legs let down."""
    return np.array(WEAPONS["lmg"]["hinge"]) + R_axis((0, 1, 0), -104.0) @ np.array((-0.31, 0.0, -0.004)) + np.array((0.0, 0.0, -0.008))


def aim_bipod(rig, st):
    """The machine gun on its bipod: shouldered and sighted as a rifle is, then dipped or raised about the butt
    until the bipod's feet stand on the ground."""
    R = np.eye(3)
    o = rig.pocket(st) - np.array(BUTT_AT["lmg"])
    foot_local = bipod_foot()
    for _ in range(3):
        fz = (o + R @ foot_local)[2]
        butt = o + R @ np.array(BUTT_AT["lmg"])
        arm_len = np.linalg.norm((R @ (foot_local - np.array(BUTT_AT["lmg"])))[[0, 2]])
        d = math.degrees(math.asin(max(min(fz / arm_len, 0.9), -0.9)))
        Rn = R_axis((0, 1, 0), d) @ R
        o = butt - Rn @ np.array(BUTT_AT["lmg"])
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
    P = prone_base(arch=0.5, weapon="lmg")      # (arched up onto the elbows: the head can come up to the sights)
    P["bipod"] = 104.0
    P["neck"] = (-28.0, -1.0, 4.0)
    P["hand_r"] = {"grip": "grip_r", "a": (0.96, 0.0, -0.28), "n": (0.0, 1.0, 0.0), "pole_world": (-0.1, -1.0, -1.2)}
    # (the left hand under the butt's small, its palm to the rear, the fingers round it to the right, pulling the butt
    # into the shoulder: a hand on top of the butt sits by his right cheek and turns the left arm past its range)
    P["hand_l"] = {"grip": "grip_butt", "slide": (0.09, 0.0, -0.075), "a": (0.032, -0.997, 0.076), "n": (-0.997, -0.037, -0.065),
                   "pole_world": (0.0, 0.6, -1.0), "roll": 15.0}
    P["fingers_l"] = 40.0
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
    stand_r = dict(stand, hand_r={"arm": PORT_ARMS["r"]})      # (the right arm leaves the rifle angle by angle)
    # The throwing arm keyed by its own angles (elevation, plane, rotation internal +, elbow, pronation, wrist flexion,
    # radial deviation; see Rig.arm_place), as a thrower's go: down to the pouch at the right hip, up past the shoulder,
    # cocked with the elbow at shoulder height and the forearm up (externally rotated), whipped through with the
    # forearm turning over, let go in front a little over the shoulder's height, and on down across the body.
    # (the rifle in the left hand goes where that arm carries it: upright before him, then held out toward the target
    # as he winds up, then pulled in as he throws)
    left = {k: {"arm": v} for k, v in (("low", (25.0, 75.0, 15.0, 95.0, -20.0)), ("out", (55.0, 80.0, 0.0, 55.0, -20.0)),
                                       ("in", (35.0, 60.0, 10.0, 100.0, -20.0)), ("tuck", (30.0, 45.0, 10.0, 105.0, -20.0)))}
    shift = dict(base_stand(), weapon=in_left_hand(left["low"]["arm"]), hand_l=left["low"], hand_r={"arm": (22.0, 55.0, 35.0, 75.0, 35.0, 10.0, 0.0)},
                 fingers_r=60.0)
    wind = dict(base_stand(), weapon=in_left_hand(left["out"]["arm"]), hand_l=left["out"],
                pelvis=(-4.0, 0.0, -28.0), spine_01=(-4.0, 3.0, -8.0), spine_02=(-5.0, 3.0, -8.0), chest=(-6.0, 4.0, -10.0),
                neck=(0.0, 0.0, 22.0), head=(-4.0, 0.0, 24.0), clavicle_r=(14.0, -12.0), pelvis_at=(-0.06, 0.0, -0.04),
                hand_r={"arm": (95.0, 5.0, -75.0, 95.0, 15.0, -25.0, 0.0)},
                fingers_r=70.0, thumb_r=50.0,
                foot_l={"at": (0.36, 0.15, ANKLE), "yaw": -5.0}, foot_r={"at": (-0.2, -0.13, ANKLE), "yaw": -40.0})
    step = dict(wind, foot_l={"at": (0.22, 0.15, 0.17), "yaw": 0.0, "pitch": -10.0})
    # (on the way up from the belt the hand comes past the right shoulder with the elbow down and out, not through it)
    lift = dict(shift, hand_r={"arm": (55.0, 40.0, -20.0, 120.0, 10.0, 0.0, 0.0)}, fingers_r=70.0)
    release = dict(base_stand(), weapon=in_left_hand(left["in"]["arm"]), hand_l=left["in"],
                   pelvis=(10.0, 0.0, 8.0), spine_01=(6.0, -2.0, 8.0), spine_02=(5.0, -3.0, 10.0), chest=(5.0, -4.0, 12.0),
                   neck=(-8.0, 0.0, -10.0), head=(-6.0, 0.0, -8.0), clavicle_r=(16.0, 14.0), pelvis_at=(0.08, 0.0, -0.06),
                   hand_r={"arm": (110.0, 50.0, -15.0, 25.0, 45.0, 15.0, 0.0)},
                   fingers_r=5.0, thumb_r=0.0,
                   foot_l={"at": (0.36, 0.15, ANKLE), "yaw": -5.0}, foot_r={"at": (-0.2, -0.13, ANKLE), "yaw": -25.0, "pitch": 30.0, "toe": 30.0})
    follow = dict(release, pelvis=(16.0, 0.0, 14.0), chest=(12.0, -3.0, 14.0), spine_02=(10.0, -2.0, 10.0),
                  weapon=in_left_hand(left["tuck"]["arm"]), hand_l=left["tuck"],
                  hand_r={"arm": (50.0, 105.0, 35.0, 35.0, 70.0, 10.0, 0.0)},
                  fingers_r=20.0, neck=(-10.0, 0.0, -6.0), head=(-8.0, 0.0, -4.0))
    # (the arm comes back by its own angles to where it holds the rifle at the port, then takes the grip)
    recover = dict(follow, hand_r={"arm": (35.0, 50.0, 35.0, 70.0, 20.0, 25.0, 0.0)}, fingers_r=40.0)
    back = dict(base_stand(), foot_l={"at": (0.2, 0.13, 0.15), "yaw": 4.0, "pitch": -8.0},
                hand_r={"arm": PORT_ARMS["r"]})
    return keyed([(0.0, stand_r), (0.25, shift), (0.38, lift), (0.5, step), (0.72, wind), (0.95, release), (1.15, follow),
                  (1.27, recover), (1.42, back), (1.6, stand)])(t)


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

    # The rifle stood upright on its butt before his knees, clutched in both fists, one above the other round the
    # fore-end (power grips, the thumbs up toward the muzzle), the elbows down and out: where the arms found it
    # easiest against rom.py's limits (a rifle hugged to the chest drives the elbows back past the body).
    R = frame_from(_n((-0.05 - 0.03 * flinch, 0.22, 1.0)), (-1.0, 0.0, 0.0))     # (past the left of his bowed head)
    butt = np.array((0.5, 0.0, 0.03))
    P["weapon"] = M4(R, butt - R @ np.array(WEAPONS["rifle"]["butt"]))
    P["hand_l"] = {"grip": "grip_l", "slide": (-0.1, 0.0, 0.0), "a": (0.26, -0.97, 0.0), "n": (0.0, 0.0, 1.0),
                   "pole_world": (0.0, 1.0, -0.6), "roll": 40.0}
    P["hand_r"] = {"grip": "grip_l", "slide": (0.04, 0.0, 0.0), "a": (0.42, 0.91, 0.0), "n": (0.0, 0.0, 1.0),
                   "pole_world": (0.0, -1.0, -0.6), "roll": 40.0}
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
               "foot_l": {"at": (0.12, 0.13, 0.12), "yaw": 8.0, "pitch": -15.0}, "foot_r": {"at": (-0.32, -0.15, ANKLE), "yaw": -20.0},
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
    stand = dict(base_stand(), hand_l={"arm": PORT_ARMS["l"]}, hand_r={"arm": PORT_ARMS["r"]})
    hunch = dict(base_stand(), chest=(18.0, 0.0, 0.0), spine_02=(12.0, 0.0, 0.0), spine_01=(8.0, 0.0, 0.0), neck=(10.0, 0, 0), head=(12.0, 0, 0),
                 pelvis_at=(0.0, 0.0, -0.06), weapon=drop_rifle((0.4, -0.25, 0.5), 20.0, 40.0),
                 # (both hands clutched to the belly, placed by the arms' own angles: the elbows down at his sides)
                 hand_l={"arm": (24.0, 60.0, 42.0, 105.0, 30.0, 25.0, 0.0)}, hand_r={"arm": (24.0, 60.0, 42.0, 105.0, 30.0, 25.0, 0.0)},
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
            # (face down, the kneecaps to the ground: a pole up would turn each leg half round in its hip)
            "foot_l": {"at": (-0.78, 0.27, 0.09), "yaw": 30.0, "pitch": 95.0, "toe": 10.0, "pole": (0.0, 0.3, -1.0)},
            "foot_r": {"at": (-0.74, -0.14, 0.09), "yaw": -10.0, "pitch": 95.0, "toe": 10.0, "pole": (0.0, -0.3, -1.0)},
            "weapon": drop_rifle((0.5, -0.3, 0.03), 25.0),
            # (fallen on his face: the left arm flung out along the ground toward his head, the right down by his side)
            "hand_l": {"arm": (115.0, -4.0, -35.0, 25.0, 50.0, 10.0, 0.0)},
            "hand_r": {"arm": (35.0, -6.0, 20.0, 30.0, 50.0, 10.0, 0.0)}, "fingers_l": 30.0, "fingers_r": 35.0}
    bounce = dict(down, pelvis_at=(0.05, 0.08, -0.77), chest=(-3.0, -4.0, 0.0))
    return keyed([(0.0, stand), (0.22, hunch), (0.45, knees), (0.75, sway), (1.0, down), (1.12, bounce), (1.5, down)])(t)


# ================================================================================================ baking the clips
def reset_pose(arm):
    for pb in arm.pose.bones:
        pb.location = (0.0, 0.0, 0.0)
        pb.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
        pb.scale = (1.0, 1.0, 1.0)


# ================================================================================================ the motion capture
# The body's movement in the locomotion, going prone, the falls and the low crawl is real motion capture: mocap.py
# (the mocap animator's) retargets CMU takes onto human.py's rig and keeps them in mocap_cmu/clips.json. Our rig has
# the same joints but bone rolls of its own and its own weapon bone (a child of the root, so a dropped rifle can leave
# the hand), so each clip is carried over by world turns: every bone's turn from its rest, measured on human.py's rest
# (kept from the man's own armature when he is dressed), is given to the same bone here. Over the capture the rifle is
# held by mocap.carry_rifle (both hands on it, at the port or at the ready) or rides the right hand at the agreed grip,
# and our weapon bone is keyed to it. The weapon work (firing, the bolt, the grenade, kneeling) stays keyed by hand.
MOCAP = {
    # our clip: dict(src = the mocap clip, carry = the rifle carry laid over it, until/blend = when the arms go back to
    # the capture and over how long, rifle = what the rifle does: rides the hand, is laid down, is dropped, or is held
    # by the drill)
    "idle": dict(src="idle", carry="port"),
    "walk": dict(src="walk", carry="port"),
    "run": dict(src="run", carry="port"),
    "advance": dict(src="advance", carry="ready"),
    "creep": dict(src="creep", carry="ready"),
    "crawl": dict(src="crawl"),                     # (the keyed low crawl holds the rifle in its own arms)
    "go_prone": dict(src="go_prone", carry="ready", until=0.25, blend=0.45, rifle="drill"),
    "die_forward": dict(src="fall_forward", carry="ready", until=0.06, blend=0.3, rifle="drop", release=0.13, throw=(0.7, -0.25, 0.5)),
    "die_back": dict(src="fall_back", carry="ready", until=0.04, blend=0.25, rifle="drop", release=0.2, throw=(0.6, -0.3, 1.0)),
}
MO_DATA = mocap.load_clips()
GUN_AXES = np.array(((0.0, -1.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0)))   # our gun's axes (x barrel, y left, z up) in mocap's rifle frame (x right, y barrel, z up)
GRIP_CURL = 12.0          # (degrees more curl for the fingers of a hand round the stock)
GUN_LIE = 0.024           # (the grip's height over the ground with the rifle lying on its left side)


def _sm_arr(x):
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3 - 2 * x)


def _quats(M):
    """(F, 3, 3) -> (F, 4) quaternions, no sign flips from frame to frame."""
    q = mocap.mat_to_quat(M)
    for k in range(1, len(q)):
        if np.dot(q[k], q[k - 1]) < 0:
            q[k] = -q[k]
    return q


def _qmat(q):
    return mocap.quat_to_mat(q / np.linalg.norm(q))


def mo_prepare(rig, name):
    """A mocap clip on this man, per capture frame: every bone's turn from its rest (world), the pelvis's head, our
    gun's turn and grip, and how far each hand is round the stock. Kept on the rig."""
    if name in rig.mo:
        return rig.mo[name]
    spec = MOCAP[name]
    c = MO_DATA[spec["src"]]
    r = rig.mrest
    F, fps, loop = int(c["frames"]), int(c["fps"]), bool(c["loop"])
    t = np.arange(F) / fps
    k = 0.5 * (r.leg("l") + r.leg("r")) / c["leg"]
    W = np.zeros((F, len(r.names), 3, 3))
    for b, p in enumerate(r.parent):
        n = r.names[b]
        D = mocap.quat_to_mat(np.array(c["bones"][n])) if n in c["bones"] else np.eye(3)
        W[:, b] = (W[:, p] if p >= 0 else np.eye(3)) @ D @ r.local(b)
    m = mocap.Motion(r, W, r.head[r.i("pelvis")] + np.array(c["pelvis_loc"]) * k, fps)
    held = {"r": np.zeros(F), "l": np.zeros(F)}
    m_cap, mc = m, None
    if spec.get("carry"):
        mc, _, err = mocap.carry_rifle(m.copy(), spec["carry"], cyclic=loop)
        held["r"][:] = 1.0
        if "until" in spec:
            w = _sm_arr((t - spec["until"]) / spec["blend"])
            L = mocap.slerp_mats(mocap.to_local(mc), mocap.to_local(m), w[:, None])
            m = mocap.from_local(r, L, m.H, fps)
            held["l"] = 1.0 - w
        else:
            m = mc
            held["l"][:] = 1.0
    elif name == "crawl":
        held["r"][:] = 1.0
    mode = spec.get("rifle", "hand")
    if mode == "drill":                         # (the drill places the rifle and puts the hands on it)
        m, Dg, Pg = _prone_drill(rig, m, m_cap, mc, t, held)
    hd = m.heads()
    h = r.i("hand_r")
    G, gl = mocap.hand_grips(r)["r"]
    if mode != "drill":
        Dg = m.W[:, h] @ G.T @ GUN_AXES
        Pg = hd[:, h] + np.einsum("fij,j->fi", m.W[:, h], gl)
    out = {"F": F, "fps": fps, "loop": loop, "long": (F - 1) / fps, "k": k, "stride": float(c["stride"]) * k,
           "end": [float(v) * k for v in c.get("end_offset", (0.0, 0.0, 0.0))], "events": []}
    if mode in ("drop", "lay"):
        # (where he ends up: each bone as a segment, so the rifle can be put down clear of him)
        bones = [b for b in range(len(r.names)) if r.parent[b] >= 0]
        a_ = hd[-1, bones]
        b_ = a_ + np.einsum("bij,bj->bi", m.W[-1, bones] @ np.swapaxes(r.R[bones], -1, -2), r.tail[bones] - r.head[bones])
        body = (a_[:, :2], b_[:, :2])
    if mode == "drop":
        out["events"] = _gun_falls(Dg, Pg, held, t, spec["release"], fps, spec.get("throw", (0.0, 0.0, 0.0)), body)
    elif mode == "lay":
        _gun_laid(Dg, Pg, held, t, hd[:, h], spec["until"] + 0.5 * spec["blend"], fps, body)
    out["Q"] = {n: _quats(m.W[:, b] @ r.R[b].T) for b, n in enumerate(r.names) if r.parent[b] >= 0}
    out["H"] = m.H.copy()
    out["Qg"], out["Pg"] = _quats(Dg), Pg
    out["held"] = held
    rig.mo[name] = out
    return out


# go_prone's rifle drill (the coordinator's review of 18 November 2026: a soldier never lets go of his rifle going
# prone): the rifle stays in the right hand the whole way. As he drops, the right hand slides to the rifle's balance
# while the left still holds; the rifle swings butt-down and the butt meets the ground first, ahead of his knees, and
# he comes down on it as it tips forward; the left hand takes the catch the capture gives both hands; the fist comes
# down on its knuckles, the rifle level beside him, as the capture's right hand steps forward; the rifle slides
# forward through the fist to the small of the stock as he goes down on his belly; then he comes up on his elbows
# (his trunk, neck and head going to fire_prone's) and the rifle is brought forward into fire_prone's aim, the left
# hand coming off the ground to take it, so the clip ends in fire_prone's first pose. Times are the capture's seconds,
# places its metres (x forward, y left, z up, the ground at 0).
DRILL = {
    "slide": (0.25, 0.50),                          # the right hand to the balance
    "swing": (0.30, 0.58),                          # swung butt-down until the butt meets the ground
    "butt": ((0.16, -0.26, 0.03), 12.0, 50.0),      # the butt's point there, and the rifle's yaw and pitch (degrees)
    "pivot": (0.58, 0.92, 5.0, 18.0),               # tipping forward on the butt, to this yaw and pitch
    "down": (0.92, 1.12, (0.80, -0.47, 0.06), -4.0, 2.0),   # the fist on its knuckles: the hold's point, yaw, pitch
    "back": (1.12, 1.25),                           # the rifle slid forward through the fist to its small of the stock
    "rise": (1.45, 1.85),                           # up on his elbows: the trunk, neck and head to fire_prone's
    "aim": (1.25, 1.80),                            # brought forward into the aim, the arm turning joint by joint
    "grab": (1.36, 1.88),                           # the left hand off the ground and onto the rifle, the same way
    "swing_out": 3.0,                               # (how early in the aim the right arm swings forward: 1 = evenly)
    "arm_world": True,                              # (the right arm's bones turned in the world over the aim)
    "push": 1.0,                                    # (the fist's speed along the rifle's line as the aim begins, of
                                                    # his shoulder's: it is pushed forward on the ground before that)
}
DRILL_TRUNK = ("spine_01", "spine_02", "chest", "neck", "head", "clavicle_l", "clavicle_r")
DRILL_REPORT = {}                                   # (how far each hand fell short of its place on the rifle, m)


def _rifle_R(yaw, pitch):
    """Our rifle's turn (x barrel, y left, z up) at a yaw (left +) and pitch (muzzle up +), sights up."""
    return R_axis((0.0, 0.0, 1.0), yaw) @ R_axis((0.0, 1.0, 0.0), -pitch)


def _prone_drill(rig, m, m_cap, mc, t, held):
    """go_prone's arms and rifle by the drill (DRILL). m is the capture with the carry eased off it (the left hand
    leaving the rifle), m_cap the bare capture, mc the carry. Returns the Motion (the right arm on the rifle
    throughout, the trunk going to fire_prone's and the left hand onto the rifle at the end) and the rifle's turn
    (F, 3, 3, our gun's axes) and grip point (F, 3)."""
    r = m.rest
    F = len(t)
    Dr = DRILL
    W_ = WEAPONS["rifle"]
    butt, bal = np.array(W_["butt"]), np.array(W_["carry"])
    G, gl = mocap.hand_grips(r)["r"]
    hr, hl = r.i("hand_r"), r.i("hand_l")
    ch, pv = r.i("chest"), r.i("pelvis")
    sh_r, sh_l = r.i("upperarm_r"), r.i("upperarm_l")
    s = lambda a, b: _sm_arr((t - a) / (b - a))
    nz = lambda v: v / np.maximum(np.linalg.norm(v, axis=-1, keepdims=True), 1e-9)
    hv = np.outer(s(*Dr["slide"]) - s(*Dr["back"]), bal)       # (the hold's place on the rifle, from the grip)
    # fire_prone's first pose, set where his pelvis lies at the end
    fp = clip_state(rig, "fire_prone", 0.0, "rifle")
    shift = m.heads()[-1, pv] - fp["H"]["pelvis"]
    Re, Pe = fp["W"][:3, :3], fp["W"][:3, 3] + shift
    rel = lambda n: Re.T @ (fp["H"][n] - fp["W"][:3, 3])
    rel_l, rel_Rl = rel("hand_l"), Re.T @ fp["D"]["hand_l"]
    rel_r, rel_Rr = rel("hand_r"), Re.T @ fp["D"]["hand_r"]
    E_r, E_l = fp["H"]["lowerarm_r"] + shift, fp["H"]["lowerarm_l"] + shift
    # up on his elbows: the trunk's bones turned to fire_prone's, the capture's pelvis and legs kept
    mT = m.copy()
    for n in DRILL_TRUNK:
        mT.W[:, r.i(n)] = fp["D"][n] @ r.R[r.i(n)]
    L0, LT = mocap.to_local(m), mocap.to_local(mT)
    wT = s(*Dr["rise"])
    for n in DRILL_TRUNK:
        L0[:, r.i(n)] = mocap.slerp_mats(L0[:, r.i(n)], LT[:, r.i(n)], wT)
    m = mocap.from_local(r, L0, m.H, m.fps)
    hd = m.heads()
    # the rifle as the carry has it (its grip point and turn)
    hdc = mc.heads()
    Rc = mc.W[:, hr] @ G.T @ GUN_AXES
    Pc = hdc[:, hr] + np.einsum("fij,j->fi", mc.W[:, hr], gl)
    # the rifle's path
    R = Rc.copy()
    P = Pc.copy()
    B0, yaw1, pit1 = Dr["butt"]
    B0 = np.array(B0, float)
    R1 = _rifle_R(yaw1, pit1)
    P1 = B0 - R1 @ butt
    a0, a1 = Dr["swing"]
    p0, p1, yaw2, pit2 = Dr["pivot"]
    R2 = _rifle_R(yaw2, pit2)
    P2 = B0 - R2 @ butt
    d0, d1, H3, yaw3, pit3 = Dr["down"]
    H3 = np.array(H3, float)
    R3 = _rifle_R(yaw3, pit3)
    P3 = H3 - R3 @ bal
    g0, g1 = Dr["aim"]
    ua = np.clip((t - g0) / (g1 - g0), 0.0, 1.0)
    f0, f1 = int(np.searchsorted(t, g0)), min(int(np.searchsorted(t, g1)), F - 1)
    # (the fist pushed forward along the ground, along the rifle's line, as he goes down onto his belly: from still
    # to his shoulder's speed, so it is already moving with the shoulder when the aim's arm swing takes it up)
    bx = nz(R3[:, 0] * np.array((1.0, 1.0, 0.0)))
    vS = bx * float((hd[f0 + 1, sh_r] - hd[f0 - 1, sh_r]) @ bx) / (t[f0 + 1] - t[f0 - 1]) * Dr["push"]
    Ts = t[f0] - d1
    tau = np.clip((t - d1) / Ts, 0.0, 1.0)
    H3f = H3 + np.outer((tau ** 2 - tau ** 3 / 3.0) * Ts, vS)
    for f in range(F):
        x = t[f]
        if x <= a0:
            continue
        if x <= a1:
            u = sm((x - a0) / (a1 - a0))
            R[f] = mocap.slerp_mats(Rc[f], R1, u)
            P[f] = Pc[f] + (P1 - Pc[f]) * u
        elif x <= p1:
            R[f] = mocap.slerp_mats(R1, R2, sm((x - p0) / (p1 - p0)))
            P[f] = B0 - R[f] @ butt                     # (the butt stays where it met the ground)
        elif x <= d1:
            u = sm((x - d0) / (d1 - d0))
            R[f] = mocap.slerp_mats(R2, R3, u)
            P[f] = P2 + (P3 - P2) * u
        elif x <= g0:                               # (the fist on the ground, the rifle sliding forward in it)
            R[f], P[f] = R3, H3f[f] - R3 @ hv[f]
        else:                                       # (in the aim at its end: the way there is the arm's, below)
            R[f], P[f] = (R3, H3f[f]) if ua[f] < 1.0 else (Re, Pe)
    # the right hand on it (the agreed grip, at the hold; fire_prone's own hold in the aim); the elbow as the carry has
    # it, then down (the rifle upright), then as the capture's (out to his right a little), then where fire_prone has it
    wh = (ua >= 1.0)[:, None]
    Rh = np.where(wh[..., None], R @ rel_Rr @ r.R[hr], R @ GUN_AXES.T @ G)
    wrist = np.where(wh, P + np.einsum("fij,j->fi", R, rel_r),
                     P + np.einsum("fij,fj->fi", R, hv) - np.einsum("fij,j->fi", R @ GUN_AXES.T @ G, gl))
    right = np.einsum("fij,j->fi", m_cap.W[:, ch] @ r.R[ch].T, np.array((0.0, -1.0, 0.0)))
    hcap = m_cap.heads()
    pole_c = nz(hdc[:, r.i("lowerarm_r")] - hdc[:, sh_r])
    pole_k = nz(nz(hcap[:, r.i("lowerarm_r")] - hcap[:, sh_r]) + 0.6 * right)
    pole_s = nz(np.array((0.0, 0.0, -1.0)) + 0.5 * right)
    pole_e = nz(E_r - hd[:, sh_r])
    w1, w2 = s(0.25, 0.45)[:, None], s(0.85, 1.0)[:, None]
    pole = np.where(wh, pole_e, nz(nz(pole_c * (1 - w1) + pole_s * w1) * (1 - w2) + pole_k * w2))
    mR = m.copy()
    short_r = mocap.solve_arm(mR, "r", wrist, pole)
    _set_hand(mR, "r", Rh)
    wR = s(0.08, 0.20)                              # (from the carry's arm onto the solved one)
    m2 = mocap.from_local(r, mocap.slerp_mats(mocap.to_local(m), mocap.to_local(mR), wR[:, None]), m.H, m.fps)
    # over the aim the right arm goes from its pose with the fist on the ground to fire_prone's joint by joint (a
    # real arm's way, inside its range), the rifle riding the hand, its hold going over from the agreed grip to
    # fire_prone's
    lin = np.clip((t - t[f0]) / (t[f1] - t[f0]), 0.0, 1.0)
    uj = 1.0 - (1.0 - _sm_arr(lin)) ** Dr["swing_out"]  # (the arm swings forward early, while his shoulder is high)
    if Dr.get("arm_world"):                         # (each bone's turn in the world, so the hand rises as he goes down)
        for n in ("upperarm_r", "lowerarm_r", "hand_r"):
            b = r.i(n)
            m2.W[f0 + 1:f1, b] = mocap.slerp_mats(m2.W[f0, b][None], m2.W[f1, b][None], uj[f0 + 1:f1])
        _set_hand(m2, "r", m2.W[:, hr].copy())
    else:
        L2 = mocap.to_local(m2)
        for n in ("upperarm_r", "lowerarm_r", "hand_r"):
            b = r.i(n)
            L2[f0 + 1:f1, b] = mocap.slerp_mats(L2[f0, b][None], L2[f1, b][None], uj[f0 + 1:f1])
        m2 = mocap.from_local(r, L2, m.H, m.fps)
    Wh, hj = m2.W[:, hr], m2.heads()[:, hr]
    A0, A1 = Wh[f0].T @ R[f0], Wh[f1].T @ R[f1]
    q0, q1 = Wh[f0].T @ (P[f0] - hj[f0]), Wh[f1].T @ (P[f1] - hj[f1])
    on_hand = lambda f, W_, h_: (W_ @ mocap.slerp_mats(A0, A1, uj[f]), h_ + W_ @ (q0 + (q1 - q0) * uj[f]))
    W_pts = np.array([W_["butt"], (0.0, 0.0, 0.0), MUZZLE])

    def low_r(f, hd_):
        Rf_, Pf_ = on_hand(f, m2.W[f, hr], hd_[hr])
        c_ = 0.02 + 0.02 * float(_sm_arr((t[f] - t[f0]) / 0.15))   # (from lying, 2 cm up, to 4 cm clear)
        return [(hd_[hr], 0.03), (hd_[r.i("fingers_r")], 0.02)] + [(Pf_ + Rf_ @ q, c_) for q in W_pts]
    _clear_ground(m2, "r", range(f0 + 1, f1), low_r)    # (the hand and rifle raised where they would dip)
    Wh, hj = m2.W[:, hr], m2.heads()[:, hr]
    for f in range(f0 + 1, f1):
        R[f], P[f] = on_hand(f, Wh[f], hj[f])
    # the left arm from the capture's (the hand on the ground) to fire_prone's on the rifle joint by joint, the arm
    # raised at the shoulder where the hand would dip into the ground on the way
    wl = s(*Dr["grab"])
    mL = m2.copy()
    short_l = mocap.solve_arm(mL, "l", P + np.einsum("fij,j->fi", R, rel_l), nz(E_l - m2.heads()[:, sh_l]))
    _set_hand(mL, "l", R @ rel_Rl @ r.R[hl])
    wL = wl
    m3 = mocap.from_local(r, mocap.slerp_mats(mocap.to_local(m2), mocap.to_local(mL), wl[:, None]), m.H, m.fps)
    _lift_clear(m3, "l", [f for f in range(F) if 0.0 < wl[f] < 1.0],
                lambda f, hd_: [(hd_[hl], 0.03), (hd_[r.i("fingers_l")], 0.02), (hd_[r.i("lowerarm_l")], 0.02)])
    held["r"][:] = 1.0
    held["l"] = np.maximum(held["l"], wl)
    # (the capture's own left hand takes the catch on its edge, fingers first, the knuckles and fingertips into the
    # ground: the arm raised at the shoulder till the hand is over it, measured on our man as mo_state will pose him,
    # the fingers and thumb as curled there)
    a_l, _, n_l = rig.hand0["l"]
    tips = {b: (np.cross(a_l, n_l) if b == "fingers_l" else np.cross(rig.T0[b] - rig.H0[b], n_l), rig.T0[b] - rig.H0[b])
            for b in ("fingers_l", "thumb_l")}

    def hand_pts(mm, f):
        J = _rig_heads(rig, mm, f)
        pts = [(J["hand_l"][0], 0.03), (J["fingers_l"][0], 0.012)]
        for b, (ax_, off) in tips.items():
            pts.append((J[b][0] + J[b][1] @ R_axis(ax_, GRIP_CURL * float(held["l"][f])) @ off, 0.012))
        return pts, J["upperarm_l"][0]
    _lift_clear(m3, "l", [f for f in range(F) if wl[f] < 1.0], None, ours=hand_pts)
    under_l = np.array([max(0.0, max(c - p[2] for p, c in hand_pts(m3, f)[0])) for f in range(F) if wl[f] < 1.0])
    DRILL_REPORT.update(short_r=float(np.max(short_r * (wR > 0.5) * (1 - (uj > 0) * (uj < 1)))),
                        short_l=float(np.max(short_l * (wL > 0.5))), under_l=float(np.max(under_l)))
    return m3, R, P


def _lift_clear(m, side, frames, low, steps=12, most=6.0, k=3, ours=None):
    """Raise an arm about its shoulder, frame by frame, just so far that each point low(f, heads) gives ((point,
    clearance), ...) is that far over the ground (z 0): the whole arm turned in the vertical plane through the
    shoulder and the lowest point, a few degrees at a time; each frame's turn then eased out over k frames each side
    (a cos-squared bump, the largest of the bumps over a frame taken), so the arm does not jerk where the ground
    starts or stops pushing it up, and is not held up long after (a hand going on to something above would rise
    past it). ours(mm, f), if given, gives the points and the shoulder instead, measured as they will be on our man
    ([(point, clearance)], shoulder; mm the arm as turned so far)."""
    r = m.rest
    sh = r.i("upperarm_" + side)
    arm = mocap.subtree(r, "upperarm_" + side)
    up = np.array((0.0, 0.0, 1.0))
    mm = m.copy()
    V = np.zeros((len(m), 3))
    for f in frames:
        Qt = np.eye(3)
        for _ in range(steps):
            if ours is None:
                hd = mm.heads()[f]
                pts, shp = low(f, hd), hd[sh]
            else:
                pts, shp = ours(mm, f)
            p, c = min(pts, key=lambda pc: pc[0][2] - pc[1])
            need = c - p[2]
            if need <= 1e-4:
                break
            d = p - shp
            reach = max(float(np.hypot(d[0], d[1])), 0.2)
            ax = np.cross(d, up)
            ax = ax / max(float(np.linalg.norm(ax)), 1e-9)
            Q = mocap.axis_angle(ax, min(need / reach * 1.05, math.radians(most)))
            Qt = Q @ Qt
            for b in arm:
                mm.W[f, b] = Q @ mm.W[f, b]
        V[f] = mocap.rot_log(Qt[None])[0]
    n = np.linalg.norm(V, axis=1)
    Vs = np.zeros_like(V)
    for f in range(len(V)):
        js = np.arange(max(f - k, 0), min(f + k + 1, len(V)))
        e = n[js] * np.cos(np.pi / 2 * np.abs(js - f) / (k + 1)) ** 2
        j = int(np.argmax(e))
        if e[j] > 0.0:
            Vs[f] = V[js[j]] * (e[j] / n[js[j]])
    for f in frames:
        if np.linalg.norm(Vs[f]) > 1e-6:
            Q = mocap.rot_exp(Vs[f][None])[0]
            for b in arm:
                m.W[f, b] = Q @ m.W[f, b]


def _rig_heads(rig, m, f):
    """Frame f of a carried capture (m, on human.py's rest) put on our man as mo_state puts it: {bone: (head, world
    turn from rest)} (the fingers' grip curl not in it)."""
    r = m.rest
    out = {}
    for b in rig.names:
        p = rig.parent[b]
        if p is None or b in ("weapon", "bolt", "bipod"):
            continue
        D = m.W[f, r.i(b)] @ r.R[r.i(b)].T if b in r.names else out[p][1]
        out[b] = (m.H[f] if b == "pelvis" else out[p][0] + out[p][1] @ (rig.H0[b] - rig.H0[p]), D)
    return out


def _clear_ground(m, side, frames, low, elbow=0.045):
    """Keep an arm off the ground, frame by frame: the wrist raised (the hand keeping its turn, so what it holds goes
    up with it, level) just so far that each point low(f, heads) gives ((point, clearance), ...) is that far over the
    ground (z 0), and the elbow swung round the shoulder-wrist line, the least it takes, to `elbow` over it."""
    r = m.rest
    ua, la, ha = r.i("upperarm_" + side), r.i("lowerarm_" + side), r.i("hand_" + side)
    arm = mocap.subtree(r, "upperarm_" + side)
    L1 = np.linalg.norm(r.head[la] - r.head[ua])
    L2 = np.linalg.norm(r.head[ha] - r.head[la])
    lifted = np.zeros(len(m))
    for f in frames:
        hd = m.heads()[f]
        need = max(0.0, max(c - p[2] for p, c in low(f, hd)))
        S, E = hd[ua], hd[la]
        wr = hd[ha] + np.array((0.0, 0.0, need))
        d = wr - S
        dist = float(np.linalg.norm(d))
        dh = d / max(dist, 1e-9)
        ca = np.clip((L1 ** 2 + dist ** 2 - L2 ** 2) / (2 * L1 * max(dist, 1e-9)), -1.0, 1.0)
        u = (E - S) - dh * float((E - S) @ dh)
        u = u / max(float(np.linalg.norm(u)), 1e-9)
        v = np.cross(dh, u)
        best = None
        for k in range(0, 181, 5):                  # (the elbow's way round, nearest its own first)
            for sg in ((1.0,) if k == 0 else (1.0, -1.0)):
                ph = math.radians(sg * k)
                n_ = math.cos(ph) * u + math.sin(ph) * v
                e_ = S + L1 * (ca * dh + math.sqrt(1.0 - ca * ca) * n_)
                if e_[2] >= elbow or k == 180:
                    best = n_
                    break
            if best is not None:
                break
        if need < 1e-4 and best is u:
            continue
        Wh = m.W[f, ha].copy()
        mm = m.slice(f, f + 1)
        mocap.solve_arm(mm, side, wr[None], best[None])
        _set_hand(mm, side, Wh[None])
        for b in arm:
            m.W[f, b] = mm.W[0, b]
        lifted[f] = need
    return lifted


def _set_hand(m, side, Wh):
    """A hand bone turned to Wh (F, 3, 3, world), the fingers and thumb with it as in the rest pose."""
    r = m.rest
    h = r.i("hand_" + side)
    m.W[:, h] = Wh
    for b in mocap.subtree(r, "hand_" + side)[1:]:
        p = r.parent[b]
        m.W[:, b] = m.W[:, p] @ r.R[p].T @ r.R[b]


def _clear_of(body, at, R, clear=0.24):
    """Slide a rifle lying at `at` (x, y) turned R sideways (to his right first, then his left) until it is `clear`
    of every bone of the body lying there (segments in plan)."""
    A, B = body
    xs = np.linspace(WEAPONS["rifle"]["butt"][0], MUZZLE[0], 12)

    def gap(p):
        P = np.asarray(p)[None, :] + np.outer(xs, R[:2, 0])                # points along the rifle
        d = B - A
        u = np.clip(np.einsum("pbi,bi->pb", P[:, None, :] - A[None], d) / np.maximum((d * d).sum(1), 1e-9), 0.0, 1.0)
        q = A[None] + u[..., None] * d[None]
        return float(np.linalg.norm(P[:, None, :] - q, axis=-1).min())

    for k in range(25):
        for side in ((-1.0,) if k == 0 else (-1.0, 1.0)):
            p = np.array(at, float) + np.array((0.0, side * 0.05 * k))
            if gap(p) >= clear:
                return p
    return np.array(at, float)


def _gun_falls(Dg, Pg, held, t, t0, fps, throw, body, lie_yaw=None):
    """The rifle let go at t0: it flies on with the hand's speed and a throw, falls, turns over and lies on its left
    side (its yaw 25 degrees past what it had, or lie_yaw), clear of where he lands."""
    i0 = int(round(t0 * fps))
    v = (Pg[i0 + 1] - Pg[i0 - 1]) * fps / 2 + np.array(throw)
    p0, R0 = Pg[i0].copy(), Dg[i0].copy()
    g = 9.81
    T = max((v[2] + math.sqrt(max(v[2] ** 2 + 2 * g * (p0[2] - GUN_LIE), 0.0))) / g, 0.25)
    yaw = math.degrees(math.atan2(R0[1, 0], R0[0, 0])) + 25.0 if lie_yaw is None else lie_yaw
    Rg = R_axis((0, 0, 1), yaw) @ R_axis((1, 0, 0), -90.0)
    land = _clear_of(body, p0[:2] + v[:2] * T * 0.7, Rg)
    for f in range(i0 + 1, len(t)):
        tau = t[f] - t[i0]
        if tau < T:
            u = tau / T
            Pg[f, :2] = p0[:2] + (land - p0[:2]) * u
            Pg[f, 2] = max(p0[2] + v[2] * tau - 0.5 * g * tau * tau, GUN_LIE)
            Dg[f] = mocap.slerp_mats(R0, Rg, sm(u))
        else:
            Pg[f] = (land[0], land[1], GUN_LIE)
            Dg[f] = Rg
    held["r"] = held["r"] * (1.0 - _sm_arr((t - t0) / 0.1))
    return [ev(round(t0 + T, 3), "rifle_drop", cue="kit_rattle")]


def _gun_laid(Dg, Pg, held, t, hand, after, fps, body):
    """The rifle rides the right hand until that hand comes down to the ground, then is laid on its left side along
    his line, beside him."""
    F = len(t)
    i0 = next((f for f in range(F) if t[f] >= after and hand[f, 2] < 0.3), F - 1)
    yaw = math.degrees(math.atan2(Dg[i0][1, 0], Dg[i0][0, 0]))
    Rg = R_axis((0, 0, 1), 0.3 * yaw - 4.0) @ R_axis((1, 0, 0), -90.0)
    p = _clear_of(body, (Pg[i0][0] - 0.05, Pg[i0][1] - 0.06), Rg, clear=0.17)
    pg = np.array((p[0], p[1], GUN_LIE))
    w = _sm_arr((t - t[i0]) / 0.3)
    for f in range(i0, F):
        Dg[f] = mocap.slerp_mats(Dg[f], Rg, w[f])
        Pg[f] = Pg[f] + (pg - Pg[f]) * w[f]
    held["r"] = held["r"] * (1.0 - w)


def mo_state(rig, name, t, kind="rifle"):
    """The solved pose (as Rig.solve gives it) of a mocap clip at time t (loops wrap, one-shots hold their end)."""
    d = mo_prepare(rig, name)
    F = d["F"]
    u = t * d["fps"]
    if d["loop"]:
        u = u % (F - 1)
    u = min(max(u, 0.0), F - 1.0)
    i = min(int(u), F - 2)
    a = u - i
    st = {"D": {"root": np.eye(3)}, "H": {"root": np.zeros(3)}, "short": {}}
    lerp = lambda X: X[i] + (X[i + 1] - X[i]) * a
    H0 = rig.H0
    for b in rig.names:
        p = rig.parent[b]
        if p is None:
            continue
        if b in d["Q"]:
            D = _qmat(lerp(d["Q"][b]))
        elif b == "weapon":
            D = _qmat(lerp(d["Qg"]))
        else:
            D = st["D"][p]
        s = b[-1]
        if b.startswith("fingers_") or b.startswith("thumb_"):
            a0, t0, n0 = rig.hand0[s]
            ax = np.cross(a0, n0) if b.startswith("fingers_") else np.cross(rig.T0[b] - H0[b], n0)
            D = D @ R_axis(ax, GRIP_CURL * float(lerp(d["held"][s])))
        st["D"][b] = D
        if b == "pelvis":
            st["H"][b] = lerp(d["H"])
        elif b == "weapon":
            st["H"][b] = lerp(d["Pg"]) - D @ np.array(WEAPONS[kind]["grip_r"])
        else:
            st["H"][b] = st["H"][p] + st["D"][p] @ (H0[b] - H0[p])
    st["W"] = M4(st["D"]["weapon"], st["H"]["weapon"])
    return st


# keyed clips in which he lets the rifle go: it rides his right hand (as it lies in it at the clip's start) until
# `release` (s), then flies on with the hand's speed and a throw (m/s: forward, left, up), falls, turns onto its left
# side and lies clear of where he lies, as in the mocap falls (_gun_falls)
KEYED_DROP = {"die_kneel": dict(release=0.1, throw=(0.3, -1.2, 0.4), lie_yaw=-10.0)}   # (lie_yaw: as it comes to lie)


def keyed_drop(rig, name):
    """The rifle's way in a KEYED_DROP clip, worked out once per man: (t, turn (F, 3, 3), grip point (F, 3), its
    events) at 60 frames a second."""
    cache = rig.__dict__.setdefault("keyed_drops", {})
    if name not in cache:
        c, spec, fps = CLIPS[name], KEYED_DROP[name], 60
        t = np.arange(int(round(c["long"] * fps)) + 1) / fps
        sts = []
        for x in t:
            P = c["fn"](float(x))
            P["weapon_kind"] = "rifle"
            sts.append(rig.solve(P))
        D0, H0_, W0 = sts[0]["D"]["hand_r"], sts[0]["H"]["hand_r"], sts[0]["W"]
        A, q = D0.T @ W0[:3, :3], D0.T @ (W0[:3, 3] - H0_)
        Dg = np.array([s_["D"]["hand_r"] @ A for s_ in sts])
        Pg = np.array([s_["H"]["hand_r"] + s_["D"]["hand_r"] @ q for s_ in sts])
        last = sts[-1]
        bones = [b for b in rig.names if rig.parent[b] is not None and b not in ("weapon", "bolt", "bipod")]
        a_ = np.array([last["H"][b] for b in bones])
        b_ = a_ + np.array([last["D"][b] @ (rig.T0[b] - rig.H0[b]) for b in bones])
        held = {"r": np.ones(len(t))}
        evs = _gun_falls(Dg, Pg, held, t, spec["release"], fps, spec["throw"], (a_[:, :2], b_[:, :2]),
                         spec.get("lie_yaw"))
        cache[name] = (t, Dg, Pg, evs)
    return cache[name]


def clip_state(rig, name, t, kind=None):
    """Any clip's solved pose at time t: from the capture, or from our keyed poses through the solver."""
    c = CLIPS[name]
    if c.get("mocap"):
        return mo_state(rig, name, t, kind or getattr(rig, "weapon_kind", "rifle"))
    P = c["fn"](t)
    P["weapon_kind"] = kind or P.get("weapon_kind", c["weapon"])
    if c["weapon"] == "lmg":
        P["weapon_kind"] = "lmg"
    st = rig.solve(P)
    if name in KEYED_DROP:                          # (the rifle let go: its own way, not the keys')
        ts, Dg, Pg, _ = keyed_drop(rig, name)
        u = float(np.clip(t * 60.0, 0.0, len(ts) - 1.0))
        i = min(int(u), len(ts) - 2)
        a = u - i
        D = mocap.slerp_mats(Dg[i], Dg[i + 1], a)
        W = M4(D, Pg[i] + (Pg[i + 1] - Pg[i]) * a - D @ np.array(WEAPONS[P["weapon_kind"]]["grip_r"]))
        st["D"]["weapon"], st["H"]["weapon"], st["W"] = W[:3, :3], W[:3, 3].copy(), W
        st["D"]["bolt"] = st["D"]["weapon"]
        st["H"]["bolt"] = st["H"]["weapon"] + st["D"]["weapon"] @ (rig.H0["bolt"] - rig.H0["weapon"])
    return st


def clip_stride(rig, name):
    c = CLIPS[name]
    return round(mo_prepare(rig, name)["stride"], 3) if c.get("mocap") else c["stride"]


def mo_events(rig, name):
    """The moments the sound and the effects need, read off the motion: each foot coming down (the heel or the ball
    reaching the ground), a knee meeting the ground, the body landing, each elbow planted in the crawl."""
    d = mo_prepare(rig, name)
    F, fps = d["F"], d["fps"]
    sts = [mo_state(rig, name, f / fps) for f in range(F)]
    H = lambda b: np.array([s["H"][b] for s in sts])
    D = lambda b: np.array([s["D"][b] for s in sts])
    evs = []
    n = F - 1 if d["loop"] else F

    def onsets(z, tol, up=0.035):
        # (down when within tol of its lowest, up again only when `up` clear: a heel and then a ball are one step)
        base = z[:n].min()
        on, out = bool(z[0] < base + tol), []
        for i in range(2 * n if d["loop"] else n):
            f = i % n
            if not on and z[f] < base + tol:
                on = True
                if i >= n or not d["loop"]:
                    out.append(f)
            elif on and z[f] > base + up:
                on = False
        return sorted(out)

    if name in ("walk", "run", "advance", "creep"):
        for s in ("l", "r"):
            fb, bb = "foot_" + s, "ball_" + s
            heel = rig.H0[fb] + np.array((-0.061, 0.0, 0.0))
            heel[2] = 0.0
            sole_b = rig.H0[bb].copy()
            sole_b[2] = 0.0
            zh = (H(fb) + np.einsum("fij,j->fi", D(fb), heel - rig.H0[fb]))[:, 2]
            zb = (H(fb) + np.einsum("fij,j->fi", D(fb), sole_b - rig.H0[fb]))[:, 2]
            for f in onsets(np.minimum(zh, zb), 0.012):
                evs.append(ev(round(f / fps, 3), "footstep", bone=fb))
        if name in ("run", "advance"):
            evs.append(ev(0.0, "kit_rattle", bone="chest", first_loop_only=True))
    elif name == "crawl":
        for s in ("l", "r"):
            for f in onsets(H("lowerarm_" + s)[:, 2], 0.01, up=0.02):
                evs.append(ev(round(f / fps, 3), "elbow_down", cue="footstep", bone="lowerarm_" + s))
    elif name in ("go_prone", "die_forward", "die_back"):
        knee = np.minimum(H("calf_l")[:, 2], H("calf_r")[:, 2])
        zb = H("spine_02")[:, 2]
        land = next((f for f in range(F) if zb[f] < zb[-1] + 0.05), F - 1)
        kf = next((f for f in range(F) if knee[f] < 0.12), None)
        if kf is not None and kf < land:
            side = "l" if H("calf_l")[kf, 2] < H("calf_r")[kf, 2] else "r"
            evs.append(ev(round(kf / fps, 3), "knee_down", bone="calf_" + side))
        evs.append(ev(round(land / fps, 3), "body_fall", bone="spine_02"))
    evs += d["events"]
    return sorted(evs, key=lambda e: e["t"])


def use_mocap():
    """Put the mocap clips in CLIPS in place of our keyed ones (the keyed ones are kept as `keyed_fn`), in the game's
    order."""
    for name, spec in MOCAP.items():
        c = MO_DATA[spec["src"]]
        old = CLIPS.get(name, {})
        CLIPS[name] = {"long": (c["frames"] - 1) / c["fps"], "loop": bool(c["loop"]), "stride": float(c["stride"]),
                       "events": [], "fn": None, "weapon": "rifle", "mocap": spec, "keyed_fn": old.get("fn"),
                       "end": list(c.get("end_offset", (0.0, 0.0, 0.0)))}
    order = ["idle", "walk", "run", "advance", "creep", "flee", "crawl", "kneel_down", "stand_up", "go_prone",
             "fire_stand", "fire_kneel", "fire_prone", "lmg_burst", "throw_grenade", "cower", "die_back", "die_forward",
             "die_kneel"]
    old = dict(CLIPS)
    CLIPS.clear()
    for n in order + [n for n in old if n not in order]:
        CLIPS[n] = old[n]


use_mocap()


def fill_mocap_events(rig):
    """The mocap clips' events and strides, read off this man (the export's); and where the rifle lands in the keyed
    clips that let it go."""
    for name in MOCAP:
        c = CLIPS[name]
        c["events"] = mo_events(rig, name)
        d = mo_prepare(rig, name)
        c["stride"] = round(d["stride"], 3)
        c["end"] = [round(v, 3) for v in d["end"]]
    for name in KEYED_DROP:
        c = CLIPS[name]
        c["events"] = sorted([e for e in c["events"] if e["fx"] != "rifle_drop"] + keyed_drop(rig, name)[3],
                             key=lambda e: e["t"])


# ---- a man playing clips one after another (the movies and the hero clip)
def blend_states(rig, A, B, w):
    """Part way (w) from pose A to pose B: each bone's turn in its parent's frame slerped, the moving heads (pelvis,
    weapon, bolt, root) slid, the rest placed by their parents."""
    if w <= 0.0:
        return A
    if w >= 1.0:
        return B
    st = {"D": {}, "H": {}, "short": {}}
    for b in rig.names:
        p = rig.parent[b]
        if p is None:
            La, Lb = A["D"][b], B["D"][b]
        else:
            La, Lb = A["D"][p].T @ A["D"][b], B["D"][p].T @ B["D"][b]
        L = mocap.slerp_mats(La, Lb, w)
        st["D"][b] = L if p is None else st["D"][p] @ L
        if p is None or b in MOVERS:
            st["H"][b] = A["H"][b] + (B["H"][b] - A["H"][b]) * w
        else:
            st["H"][b] = st["H"][p] + st["D"][p] @ (rig.H0[b] - rig.H0[p])
    st["W"] = M4(st["D"]["weapon"], st["H"]["weapon"])
    return st


def shifted(st, d):
    """A pose moved over the ground by d (x, y)."""
    v = np.array((d[0], d[1], 0.0))
    out = {"D": st["D"], "H": {b: h + v for b, h in st["H"].items()}, "short": st.get("short", {})}
    out["W"] = M4(st["D"]["weapon"], out["H"]["weapon"])
    out.update({k: st[k] for k in ("sight_miss", "sight_turn", "butt_slide") if k in st})     # (for the aim's check)
    return out


class Performance:
    """A man playing clips one after another: segs = [dict(clip, at (s), rate=1, phase=0 (clip time at `at`),
    blend=0.3 (s, into it from the one before))]. A one-shot holds its last pose until the next clip; each clip is set
    on the ground where the one before left his pelvis; the root goes forward at a loop's stride over its length."""

    def __init__(self, rig, segs, kind="rifle", until=10.0):
        self.rig, self.segs, self.kind = rig, segs, kind
        for k, s in enumerate(segs):
            s.setdefault("rate", 1.0)
            s.setdefault("phase", 0.0)
            s.setdefault("blend", 0.3 if k else 0.0)
            c = CLIPS[s["clip"]]
            s["speed"] = clip_stride(rig, s["clip"]) / c["long"] * s["rate"] if c["loop"] else 0.0
            s["shift"] = np.zeros(2)
            if k:
                prev = segs[k - 1]
                a = self.raw(prev, s["at"])
                b = self.raw(s, s["at"])
                s["shift"] = prev["shift"] + a["H"]["pelvis"][:2] - b["H"]["pelvis"][:2]
        dt = 1.0 / 240
        self.ts = np.arange(0.0, until + dt, dt)
        v = np.array([self._speed(t) for t in self.ts])
        self.dist = np.concatenate([[0.0], np.cumsum(0.5 * (v[1:] + v[:-1]) * dt)])

    def clip_time(self, s, t):
        c = CLIPS[s["clip"]]
        ct = s["phase"] + (t - s["at"]) * s["rate"]
        return max(ct, 0.0) if c["loop"] else min(max(ct, 0.0), c["long"])

    def raw(self, s, t):
        return clip_state(self.rig, s["clip"], self.clip_time(s, t), self.kind)

    def _active(self, t):
        k = 0
        for i, s in enumerate(self.segs):
            if s["at"] <= t + 1e-9:
                k = i
        return k

    def _weight(self, k, t):
        s = self.segs[k]
        return 1.0 if k == 0 or s["blend"] <= 0 else sm((t - s["at"]) / s["blend"])

    def _speed(self, t):
        k = self._active(t)
        w = self._weight(k, t)
        v = self.segs[k]["speed"]
        return v if w >= 1.0 else (1 - w) * self.segs[k - 1]["speed"] + w * v

    def state(self, t):
        k = self._active(t)
        s = self.segs[k]
        st = shifted(self.raw(s, t), s["shift"])
        w = self._weight(k, t)
        if w < 1.0:
            p = self.segs[k - 1]
            st = blend_states(self.rig, shifted(self.raw(p, t), p["shift"]), st, w)
        return st

    def travel(self, t):
        return float(np.interp(t, self.ts, self.dist))

    def events(self, end):
        """[(time, event, seg)] of every clip event while its clip plays (a loop's at every turn)."""
        out = []
        for k, s in enumerate(self.segs):
            c = CLIPS[s["clip"]]
            t_end = self.segs[k + 1]["at"] if k + 1 < len(self.segs) else end
            for e in c["events"]:
                n = 0
                while True:
                    ct = e["t"] + n * c["long"]
                    T = s["at"] + (ct - s["phase"]) / s["rate"]
                    if T >= t_end or (not c["loop"] and n):
                        break
                    if T >= s["at"] - 1e-6 and not (e.get("first_loop_only") and n):
                        out.append((T, e, s))
                    n += 1
                    if not c["loop"]:
                        break
        return sorted(out, key=lambda x: x[0])


def bake_clips(m, names=None):
    """Every clip as its own action on the man's armature: the pose solved at every 60th of a second and written as
    plain keys (linear between), frames 0..N, so the last frame of a loop is its first again."""
    arm, rig = m["arm"], m["rig"]
    arm.animation_data_create()
    acts, short = {}, {}
    if rig.mrest is not None:
        fill_mocap_events(rig)
    for name in (names or list(CLIPS)):
        c = CLIPS[name]
        act = bpy.data.actions.new(name)
        act.use_fake_user = True
        arm.animation_data.action = act
        rig.prev_q = {}
        N = int(round(c["long"] * FPS))
        worst = 0.0
        for f in range(N + 1):
            st = clip_state(rig, name, f / FPS, c["weapon"])
            worst = max([worst] + [abs(v) for v in st["short"].values()])
            rig.key(st, f)
        for fc in kit.fcurves_of(act):
            for kp in fc.keyframe_points:
                kp.interpolation = "LINEAR"
        act.use_frame_range = True
        act.frame_start, act.frame_end = 0, N
        act["loop"] = c["loop"]
        act["stride"] = clip_stride(rig, name)
        if c.get("mocap") and not c["loop"]:
            act["end_offset"] = c["end"]
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
    """One pose (a pose said in plain terms, or a solved state) keyed on an armature of its own (for stills): a
    one-key action, so it holds whatever the frame."""
    if arm.animation_data:
        arm.animation_data.action = None
    rig.prev_q = {}
    rig.key(P if "D" in P else rig.solve(P), frame)


# ================================================================================================ levels of detail
LOD_TARGET = {1: 1800, 2: 600}
LOD2_LEFT_OFF = ("rifle_bolt", "bottle", "shovel", "straps", "lmg_bipod", "details")     # (too small to see from that far)


def tris_of(ob):
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


SKIN_FLOOR = {1: 200, 2: 60}          # (a far man keeps at least this many triangles of face and hands)
SKIN_APART = {1: 300, 2: 80}          # (and when they are thinned apart to keep them, this many: the German's own share)
SKIN_PAINTS = ("skin", "eye")


def paint_tris(ob, paints):
    names = [(mt.name.split(".")[0] if mt else "") for mt in ob.data.materials]
    return sum(len(p.vertices) - 2 for p in ob.data.polygons if names and names[p.material_index] in paints)


def _lod_decimate(c, r):
    """Thin c by ratio r."""
    # (where two cloths nearly touch the mesh has an edge shared by more than two faces, which the thinning will
    # not pass: such edges are split first)
    bm = bmesh.new()
    bm.from_mesh(c.data)
    nm = [e for e in bm.edges if not e.is_manifold and not e.is_boundary]
    if nm:
        bmesh.ops.split_edges(bm, edges=nm)
        bm.to_mesh(c.data)
    bm.free()
    d = c.modifiers.new("lod", "DECIMATE")
    d.ratio = r                                       # (not mirrored: MakeHuman's faces are not quite symmetric)
    bpy.context.view_layer.objects.active = c
    with bpy.context.temp_override(object=c, active_object=c):
        bpy.ops.object.modifier_apply(modifier=d.name)
    c.data.validate()                                 # (the thinning can leave a face doubled on itself: gone)


def _drop_faces(ob, keep_skin):
    """Delete ob's faces that are (keep_skin False) or are not (keep_skin True) face and hands, and loose points."""
    names = [(mt.name.split(".")[0] if mt else "") for mt in ob.data.materials]
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    gone = [f for f in bm.faces if (names[f.material_index] in SKIN_PAINTS) != keep_skin]
    bmesh.ops.delete(bm, geom=gone, context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.to_mesh(ob.data)
    bm.free()


def _lod_decimate_apart(c, r, want):
    """Thin the face and hands apart from the cloth: the cloth by r, the skin to about `want` triangles; then one mesh
    again (same paints, same bone groups)."""
    sk = c.copy()
    sk.data = c.data.copy()
    bpy.context.scene.collection.objects.link(sk)
    _drop_faces(c, keep_skin=False)
    _drop_faces(sk, keep_skin=True)
    _lod_decimate(c, r)
    n = tris_of(sk)
    _lod_decimate(sk, min(1.0, want / max(n, 1)))
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    sk.select_set(True)
    c.select_set(True)
    bpy.context.view_layer.objects.active = c
    with bpy.context.temp_override(active_object=c, selected_editable_objects=[c, sk], selected_objects=[c, sk]):
        bpy.ops.object.join()


def make_lods(m, lod):
    """Thinned copies of the man's meshes for a level of detail, skinned to the same armature. Where the cloth's
    sharp places (gaiter straps, puttee turns) would take the whole budget and leave a far man faceless, the face and
    hands are thinned apart from the cloth instead, keeping SKIN_FLOOR triangles."""
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
            floor = min(SKIN_FLOOR[lod], paint_tris(c, SKIN_PAINTS))
            src = c.data.copy() if floor else None
            _lod_decimate(c, r)
            if floor and paint_tris(c, SKIN_PAINTS) < floor:
                old = c.data
                c.data = src.copy()
                bpy.data.meshes.remove(old)
                _lod_decimate_apart(c, r, SKIN_APART[lod])
            if src is not None:
                bpy.data.meshes.remove(src)
            if floor:
                print(f"    {c.name}: {tris_of(c)} triangles, face and hands {paint_tris(c, SKIN_PAINTS)}")
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
    with its bone (`part`), a second bone (`part2`) and how much of the second it takes (`w2`, 0 to 0.5). A mesh with a
    colour attribute (the wear and seam shade, glTF's COLOR_0: grey, 1.0 clean, 0.75 a seam) adds `shade` per point,
    0 to 1, by which the game multiplies the paint's colour; with no such mesh there is no `shade` (all 1)."""
    pts, fac, pai, tri, part, part2, w2, shd = [], [], [], [], [], [], [], []
    seen = {}
    for ob in objs:
        me = ob.data
        ca = me.color_attributes[0] if len(me.color_attributes) else None
        by_loop = ca is not None and ca.domain == "CORNER"
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
                sh = 1.0 if ca is None else round(min(1.0, max(0.0, ca.data[li if by_loop else vi].color[0])), 2)
                key = (round(p.x, 4), round(-p.y, 4), round(p.z, 4), round(n.x, 3), round(-n.y, 3), round(n.z, 3), paint) + vb[vi] + (sh,)
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
                    shd.append(sh)
                tri.append(i)
    out = {"points": pts, "facings": fac, "paints": pai, "triangles": tri, "part": part, "part2": part2, "w2": w2}
    if any(v < 1.0 for v in shd):
        out["shade"] = shd
    return out


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
            st = clip_state(rig, name, t, c["weapon"])
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
        out[name] = {"long": L, "loop": c["loop"], "stride": clip_stride(rig, name), "weapon": c["weapon"], "tracks": tracks, "events": evs}
        if c.get("mocap"):
            src = MOCAP[name]["src"]
            out[name]["from"] = (f"motion capture: mocap.py's {src} ({MO_DATA[src]['source']})" if src != "crawl" else
                                 "mocap.py's crawl, keyed by hand on the rig (CMU has no belly crawl)")
            if not c["loop"]:
                e = c["end"]
                out[name]["end_offset"] = [round(e[0], 3), round(-e[1], 3), round(e[2], 3)]
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
                   "one loop covers; the game moves the unit. A mesh may carry `shade` per point (0 to 1, the wear and seam "
                   "darkening): its colour is the paint's colour times shade; with no `shade` every point is 1.",
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
    kit.fresh(FPS)
    man = mh_body("GER")                                 # (the helmets are seated on MakeHuman's man, as on the soldiers)
    J = joints_of(man)
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


def export_all(info, nations=("GER", "POL")):
    """soldier_GER.glb, soldier_POL.glb (LOD0 with every clip), their LOD1 and LOD2 (.glb, the same skeleton, no clips),
    soldier.fbx (the German, LOD0, every clip a take), soldier_GER_lmg.glb (the German gunner with the LMG, every clip),
    soldier_GER.json (the game's own form), and the rigid helmets and weapons. `--nations JAP,CHN` makes only those
    (their .glb, LODs and .json, and for the German also his .fbx and LMG man); the helmets and weapons are made when
    both the German and the Pole are in the list."""
    staged = []
    for nation in nations:
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
        if nation == "GER":                           # (the mocap clips' events, strides and end offsets, for the README)
            info["mocap"] = {n: {k: CLIPS[n][k] for k in ("events", "stride", "end")} for n in MOCAP}
        export_glb(stage_path(f"soldier_{nation}.glb"), [m["arm"]] + lods[0] + socks)
        staged.append(f"soldier_{nation}.glb")
        for lod in (1, 2):
            export_glb(stage_path(f"soldier_{nation}_LOD{lod}.glb"), [m["arm"]] + lods[lod] + socks, anim=False)
            staged.append(f"soldier_{nation}_LOD{lod}.glb")
        if nation != "GER":                           # (every army's man in the game's own form; the German's below)
            data = write_game_json(stage_path(f"soldier_{nation}.json"), m, lods)
            staged.append(f"soldier_{nation}.json")
            info.setdefault("json_by_nation", {})[nation] = {k: len(data[k]["triangles"]) // 3 for k in ("near", "mid", "far")}
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
            staged += ["soldier.fbx", "soldier_GER.json", "soldier_GER_lmg.glb"]
        print(f"  {nation} exported in {time.time() - t0:.0f} s: {n}")
    if {"GER", "POL"} <= set(nations):                # (the props are the German's and the Pole's helmets and weapons)
        export_props(info)
        staged += ["weapons/rifle.glb", "weapons/lmg.glb"] + [f"helmets/{k}.glb" for k in HELMETS]
    for n_ in staged:                                 # (everything made is delivered, so no file is left stale beside it)
        deliver(n_)
    with open(stage_path("export_info.json"), "w") as f:
        json.dump(info, f, indent=1, default=float)
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


MOVIES = {
    # name: ([(clip, plays, seconds held after)], camera target, distance, pitch, yaw)
    "walk": ([("walk", 2, 0.0)], (0.0, 0.0, 0.85), 6.5, 20.0, -40.0),
    "run": ([("run", 3, 0.0)], (0.0, 0.0, 0.85), 6.5, 20.0, -40.0),
    "advance": ([("advance", 3, 0.0)], (0.0, 0.0, 0.75), 6.0, 16.0, -35.0),
    "fire_stand": ([("fire_stand", 2, 0.0)], (0.1, 0.0, 0.95), 5.5, 18.0, -55.0),
    "prone": ([("go_prone", 1, 0.0), ("fire_prone", 2, 0.0)], (0.45, 0.0, 0.45), 6.0, 24.0, -50.0),
    "crawl": ([("crawl", 2, 0.0)], (0.35, 0.0, 0.25), 4.6, 22.0, -50.0),
    "die_back": ([("die_back", 1, 0.7)], (-0.35, 0.0, 0.6), 6.0, 20.0, -70.0),
}


def movie_performance(rig, parts):
    """The movie's clips one after another as a Performance, and how long it runs."""
    segs, t = [], 0.0
    for name, plays, hold in parts:
        c = CLIPS[name]
        segs.append({"clip": name, "at": t, "blend": 0.25 if segs else 0.0})
        t += c["long"] * plays + hold
    return Performance(rig, segs, until=t + 0.5), t


def movie(m, key, ground, fast=False):
    """One clip movie at 30 frames a second: the man plays the clips (blended one into the next), the ground slides
    under him at a loop's speed. Shots get the shared Effects' flash, light and smoke at the muzzle; a knee or a body
    meeting the ground raises dust."""
    parts, target, dist, pitch, yaw = MOVIES[key]
    cam = bpy.context.scene.camera
    kit.orbit(cam, target, dist, pitch, yaw)
    rig, arm = m["rig"], m["arm"]
    perf, long = movie_performance(rig, parts)
    act = bpy.data.actions.new("movie_" + key)
    arm.animation_data_create()
    arm.animation_data.action = act
    rig.prev_q = {}
    n = int(round(long * 30)) + 1
    seq = []
    for i in range(n):
        t = i / 30
        rig.key(perf.state(t), i)
        seq.append((act, i, -perf.travel(t), t))
    for fc in kit.fcurves_of(act):
        for kp in fc.keyframe_points:
            kp.interpolation = "LINEAR"
    reset_pose(arm)
    fx = None
    for T, e, sg in perf.events(long):
        if e["fx"] not in ("muzzle_flash", "body_fall", "knee_down") or T > long:
            continue
        if fx is None:
            fx = Effects(seed=3)
            fx.glare = 0.4
        st = perf.state(T)
        tf = math.ceil(T * 30 - 1e-6) / 30           # (on the frame it falls on or the next, so it is seen)
        if e["fx"] == "muzzle_flash":
            Mz = Matrix(st["W"].tolist()) @ Matrix.Translation(Vector(MUZZLE))
            where = (Mz.translation.copy(), (Mz.to_3x3() @ Vector((1.0, 0.0, 0.0))).normalized())
            fx.rifle(tf, where)
        else:
            at = Vector(st["H"][e.get("bone", "spine_02")].tolist())
            at.z = 0.0
            fx.dust(tf, at, size=0.7 if e["fx"] == "body_fall" else 0.4)
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
    studio_smooth(m)
    fill_mocap_events(m["rig"])
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
        studio_smooth(m)
        for mt in bpy.data.materials:                     # (so the next man gets his own colours)
            if not mt.name.startswith(("GER_", "POL_", "backdrop", "ground")):
                mt.name = nation + "_" + mt.name
        m["arm"].location = (0.0, y, 0.0)
        P = base_stand()
        P["foot_l"] = {"at": (0.12, 0.12, ANKLE), "yaw": 6.0}
        P["foot_r"] = {"at": (-0.06, -0.12, ANKLE), "yaw": -8.0}
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
    (2.7, 3.6, 7.0, "crawl", 0.75),
    (3.7, 8.2, -6.0, "fire_prone", 1.15),
    (0.9, -4.8, 9.0, "fire_kneel", 0.2),
    (0.4, 1.0, -7.0, "fire_kneel", 0.75),
    (1.4, 5.9, 3.0, "fire_kneel", 1.05),
    (-1.6, -8.6, 10.0, "fire_stand", 0.13),
    (-2.1, 2.9, -4.0, "throw_grenade", 0.86),
    (-3.1, -2.6, 2.0, "walk", 0.3),
    (-4.2, 7.0, 12.0, "advance", 0.2),
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


SQUAD_AT, SQUAD_FACING = (15.0, 8.0), 90.0          # (on the battle stage: east of the road, facing north to the wall)
SQUAD_CAM = ((19.5, -3.5, 2.6), (14.5, 15.0, 0.6))    # behind the line's right, looking north-north-west


def pictures_squad(info, fast=False):
    """The squad still on the battle stage: twelve men facing north, each frozen in a moment of a clip, seen from
    behind the right of the line, with depth behind them (the stone walls, the lone tree, the farm, the wood's edge)
    and a thin haze of drifting smoke across the field between."""
    import battle_set
    sc = kit.fresh(FPS)
    kit.studio(sun_elev=30.0, sun_turn=-20.0)
    stage = battle_set.build()
    m = assemble("GER")
    studio_smooth(m)
    for ob in (m["weapon"], m["weapon_part"]):
        ob.hide_render = True
    m["arm"].location = (0.0, 0.0, -50.0)          # (the pattern man is kept out of sight)
    fx = Effects(seed=5)
    face = math.radians(SQUAD_FACING)
    Rf = Matrix.Rotation(face, 3, "Z")
    for k, (x, y, turn, name, t) in enumerate(SQUAD):
        w = Vector(SQUAD_AT + (0.0,)) + Rf @ Vector((x, y, 0.0))
        w.z = stage.height(w.x, w.y)
        arm, pieces = clone_man(m, tuple(w), SQUAD_FACING - 90.0 + turn)
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
        rig.mrest = m["rig"].mrest
        rig.weapon_kind = "lmg" if name == "lmg_burst" else "rifle"
        st = clip_state(rig, name, t)
        pose_once(arm, rig, st, 0)
        # the firing men's smoke drifting from their last shots; the gunner caught mid-burst
        if name.startswith("fire_") or name == "lmg_burst":
            A = Matrix.Translation(w) @ Matrix.Rotation(math.radians(SQUAD_FACING - 90.0 + turn), 4, "Z")
            Mz = A @ Matrix(st["W"].tolist()) @ Matrix.Translation(Vector(MUZZLE))
            where = (Mz.translation.copy(), (Mz.to_3x3() @ Vector((1.0, 0.0, 0.0))).normalized())
            if name == "lmg_burst":
                for j in range(6):
                    fx.rifle(-0.01 - 0.1 * j, where, size=0.75, light=(j == 0))
            else:
                fx.rifle(-fx.rng.uniform(0.4, 1.4), where, light=False)
    # the haze: across the field in front of the line, and further off toward the walls
    ax, ay = SQUAD_AT
    fx.haze(-9.0, 0.0, (ax - 22.0, ay + 9.0, 0.0), (ax + 18.0, ay + 13.0, 0.0), every=0.12, alpha=(0.08, 0.16))
    fx.haze(-9.0, 0.0, (ax - 30.0, ay + 20.0, 0.0), (ax + 25.0, ay + 24.0, 0.0), every=0.18, r=(2.0, 4.5), alpha=(0.05, 0.1))
    fx.at(0.0)
    sc.frame_set(0)
    cam = kit.camera(lens=32)
    kit.look_at(cam, Vector(SQUAD_CAM[0]), Vector(SQUAD_CAM[1]))
    shared_render(640 if fast else 1280, 360 if fast else 720, 16 if fast else 48)
    sc.cycles.transparent_max_bounces = 48
    t = time.time()
    kit.still(stage_path("pictures", "squad.png"))
    info.setdefault("render", {})["squad"] = f"{time.time() - t:.0f} s"
    deliver("pictures/squad.png")


def pictures_before_after(info, fast=False):
    kit.fresh(FPS)
    first = assemble_sculpted("GER")
    m = assemble("GER")
    studio_smooth(m)
    t = time.time()
    picture_before_after(m, stage_path("pictures", "before-after.png"), fast, first=first)
    info.setdefault("render", {})["before-after"] = f"{time.time() - t:.0f} s"
    deliver("pictures/before-after.png")


# ================================================================================================ the hero clip
HERO_SECONDS, HERO_FPS = 8.0, 30


HERO_SEEDS = [3, 11, 17, 23, 29, 31, 37, 41, 43, 47, 53, 59]    # (each man's face and build: man 0 is the German of the files)


# Going prone is played at its own speed in the hero clip: sped up (it was 1.12 to 1.25) the capture's arms turn past
# 900 degrees a second, faster than a real arm (rom.py).
HERO_PRONE_RATE = 1.0


def hero_plan(seed=11):
    """The squad's eight seconds: twelve men (the gunner in the middle) advance north at the ready in two loose rows
    across the stubble east of the road, each at his own pace and step. The machine-gun team (the gunner and his
    number two) go to ground first and set up; two riflemen throw themselves down and fire; one man is hit and falls on
    his face; three more drop to a knee and fire near the end; the rest keep on. Each man's clips as Performance
    segments."""
    import random
    rnd = random.Random(seed)
    men = []
    for i in range(12):
        row = i % 2
        man = {"i": i, "x": 6.4 + i * 1.4 + rnd.uniform(-0.35, 0.35), "y": -33.0 - 2.6 * row + rnd.uniform(-0.8, 0.8),
               "turn": 90.0 + rnd.uniform(-5.0, 5.0), "rate": rnd.uniform(0.94, 1.06), "phase": rnd.uniform(0.0, 0.8),
               "weapon": "rifle"}
        men.append(man)
    men[5]["weapon"] = "lmg"
    for man in men:
        i, r = man["i"], rnd
        segs = [{"clip": "advance", "at": 0.0, "rate": man["rate"], "phase": man["phase"]}]
        if i in (5, 6):                                       # the machine-gun team
            td = 1.9 if i == 5 else 2.15 + r.uniform(0.0, 0.2)
            segs.append({"clip": "go_prone", "at": td, "rate": HERO_PRONE_RATE, "blend": 0.35})
            up = td + CLIPS["go_prone"]["long"] / HERO_PRONE_RATE
            if i == 5:
                segs.append({"clip": "lmg_burst", "at": up - 0.1, "phase": -0.55, "blend": 0.6})
            else:
                segs.append({"clip": "fire_prone", "at": up - 0.1, "phase": -r.uniform(0.6, 1.0), "rate": r.uniform(0.95, 1.05), "blend": 0.45})
            man["role"] = "gunner" if i == 5 else "number two"
        elif i in (2, 10):                                    # down and firing
            td = r.uniform(2.6, 3.4)
            r.uniform(1.12, 1.25)                         # (still drawn, so every man after draws as before)
            segs.append({"clip": "go_prone", "at": td, "rate": HERO_PRONE_RATE, "blend": 0.35})
            up = td + CLIPS["go_prone"]["long"] / segs[-1]["rate"]
            segs.append({"clip": "fire_prone", "at": up - 0.1, "phase": -r.uniform(0.2, 0.7), "rate": r.uniform(0.94, 1.06), "blend": 0.45})
            man["role"] = "prone"
        elif i == 9:                                          # hit
            segs.append({"clip": "die_forward", "at": 3.7, "blend": 0.1})
            man["role"] = "hit"
        elif i in (0, 4, 7):                                  # to a knee, late
            tk = r.uniform(4.9, 6.4)
            segs.append({"clip": "kneel_down", "at": tk, "blend": 0.3})
            segs.append({"clip": "fire_kneel", "at": tk + CLIPS["kneel_down"]["long"], "phase": -r.uniform(0.1, 0.5), "rate": r.uniform(0.94, 1.06), "blend": 0.1})
            man["role"] = "kneel"
        else:
            man["role"] = "advance"
        man["segs"] = segs
    return men


HERO_CAM = ((25.5, -18.0, 2.4), (26.5, -3.0, 2.6), (12.0, -34.0, 0.6), (12.0, -19.0, 0.4))    # eye and target, start and end


def hero_camera(t):
    k = sm(t / HERO_SECONDS * 0.9 + 0.05)
    e0, e1, g0, g1 = (Vector(v) for v in HERO_CAM)
    return e0.lerp(e1, k), g0.lerp(g1, k)


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
            # the smoke: many small soft puffs, thrown out along the shot, slowing, spreading and drifting
            for i in range(10 if size >= 1.0 else 6):
                v = facing * g.uniform(1.0, 6.5) * size + side * g.uniform(-0.5, 0.5) + up * g.uniform(-0.15, 0.5)
                c = g.uniform(0.7, 0.82)
                self.puff(t + g.uniform(0.0, 0.03), at + facing * 0.05 + side * g.uniform(-0.02, 0.02), vel=v,
                          life=g.uniform(1.6, 3.0), r=(0.02 * size, g.uniform(0.12, 0.26) * size),
                          col=(0.78, 0.74, 0.68), col2=(c, c * 0.98, c * 0.95), alpha=g.uniform(0.22, 0.36) * min(1.0, size + 0.2),
                          drag=g.uniform(3.0, 4.5), rise=g.uniform(0.06, 0.16), fade_in=0.04, grow=0.5, wind=1.0)
            if light:
                self.light(t, at + facing * 0.15 + up * 0.05, 45.0 * size * self.glare, 0.05)
        self.once(t0, where, make)

    def dust(self, t0, at, size=1.0):
        """A man goes down on the dry ground: tan dust in many small soft puffs, thrown out low and settling downwind."""
        g = self.rng

        def make(t, p0, facing):
            for i in range(int(6 + 12 * size)):
                a = g.uniform(0, 2 * math.pi)
                v = Vector((math.cos(a), math.sin(a), 0.0)) * g.uniform(0.3, 1.4) * size + Vector((0, 0, g.uniform(0.1, 0.45)))
                c = g.uniform(0.6, 0.7)
                self.puff(t + g.uniform(0.0, 0.08), p0 + Vector((g.uniform(-0.3, 0.3), g.uniform(-0.3, 0.3), 0.03)) * size, vel=v,
                          life=g.uniform(1.3, 2.4), r=(0.04 * size, g.uniform(0.14, 0.3) * size), col=(0.66, 0.56, 0.42), col2=(c, c * 0.92, c * 0.8),
                          alpha=g.uniform(0.28, 0.42), drag=2.6, rise=0.1, fade_in=0.05, grow=0.45, wind=0.6, squash=(1.0, 1.0, 0.65))
        self.once(t0, (Vector(at), Vector((0, 0, 1))), make)

    def kick(self, t0, at, fwd):
        """A running foot on the dry ground: a wisp or two of dust kicked back."""
        g = self.rng

        def make(t, p0, back):
            for i in range(2):
                v = back * g.uniform(0.3, 0.8) + Vector((g.uniform(-0.2, 0.2), g.uniform(-0.2, 0.2), g.uniform(0.05, 0.25)))
                c = g.uniform(0.62, 0.7)
                self.puff(t + g.uniform(0.0, 0.03), p0 + Vector((0.0, 0.0, 0.02)), vel=v, life=g.uniform(0.8, 1.3),
                          r=(0.02, g.uniform(0.07, 0.12)), col=(0.66, 0.56, 0.42), col2=(c, c * 0.92, c * 0.8),
                          alpha=g.uniform(0.16, 0.24), drag=3.0, rise=0.05, fade_in=0.04, grow=0.5, wind=0.5, squash=(1.0, 1.0, 0.7))
        self.once(t0, (Vector(at), -Vector(fwd)), make)

    def haze(self, t0, t1, a, b, every=0.25, high=(0.6, 3.0), r=(1.2, 3.2), alpha=(0.06, 0.13)):
        """A thin haze of drifting smoke across a stretch of the field (from a to b): big faint soft puffs born along it
        from t0 to t1 and carried off by the wind. Born before 0, it is already there when the picture starts."""
        g = self.rng
        a, b = Vector(a), Vector(b)
        k = 0
        while t0 + k * every < t1:
            t = t0 + k * every
            p = a.lerp(b, g.random())
            p.z += g.uniform(*high)
            c = g.uniform(0.66, 0.78)
            self.puff(t, p, vel=Vector((g.uniform(-0.1, 0.1), g.uniform(-0.1, 0.1), 0.0)), life=g.uniform(7.0, 11.0),
                      r=(r[0] * 0.6, g.uniform(*r)), col=(c, c * 0.98, c * 0.95), alpha=g.uniform(*alpha), drag=0.0,
                      rise=0.04, fade_in=1.5, grow=0.7, wind=1.0, hold=0.3)
            k += 1


def pictures_hero(info, fast=False, men_only=None):
    """The squad advancing over the battle stage, going to ground and opening fire: 8 s at 30 frames a second, the
    men's bodies moving by the motion capture (advance at the ready, go prone, the fall) and our keyed clips for the
    weapon work. Returns the scene, the frame count and the effects (step them with fx.at(t) frame by frame)."""
    import battle_set
    sc = kit.fresh(HERO_FPS)
    kit.studio(sun_elev=30.0, sun_turn=-20.0)
    t0 = time.time()
    stage = battle_set.build()
    print(f"  stage built in {time.time() - t0:.0f} s")
    frames = int(HERO_SECONDS * HERO_FPS)
    men = hero_plan()
    fx = Effects(seed=7)
    t0 = time.time()
    for man in men:
        if men_only is not None and man["i"] not in men_only:
            continue
        yaw = math.radians(man["turn"])
        fwd = Vector((math.cos(yaw), math.sin(yaw), 0.0))
        # each man his own: his face and build from his own seed, dressed and rigged on himself
        m = assemble("GER", weapon=man["weapon"], seed=HERO_SEEDS[man["i"]])
        studio_smooth(m)
        fit_body(m["J"])                                # (his keyed clips on his own legs' measures)
        arm, rig = m["arm"], m["rig"]
        fill_mocap_events(rig)
        perf = Performance(rig, [dict(sg) for sg in man["segs"]], kind=man["weapon"], until=HERO_SECONDS + 0.5)
        arm.rotation_euler = (0.0, 0.0, yaw)
        arm.animation_data_create()
        arm.animation_data.action = bpy.data.actions.new(f"hero_{man['i']}")
        rig.prev_q = {}

        def root_at(t):
            d = perf.travel(t)
            x, y = man["x"] + fwd.x * d, man["y"] + fwd.y * d
            return Vector((x, y, stage.height(x, y)))

        for f in range(frames):
            t = f / HERO_FPS
            rig.key(perf.state(t), f)
            arm.location = root_at(t)
            arm.keyframe_insert("location", frame=f)
        for fc in kit.fcurves_of(arm.animation_data.action):
            for kp in fc.keyframe_points:
                kp.interpolation = "LINEAR"
        # the effects at the clips' own events: shots, dust where a foot, a knee, a body or the dropped rifle lands
        for T, e, sg in perf.events(HERO_SECONDS):
            tf = math.ceil(T * HERO_FPS - 1e-6) / HERO_FPS     # (on the frame it falls on, so it is seen)
            if tf >= HERO_SECONDS:
                continue
            st = perf.state(tf)
            A = Matrix.Translation(root_at(tf)) @ Matrix.Rotation(yaw, 4, "Z")
            if e["fx"] == "muzzle_flash":
                Mz = A @ Matrix(st["W"].tolist()) @ Matrix.Translation(Vector(MUZZLE))
                where = (Mz.translation.copy(), (Mz.to_3x3() @ Vector((1.0, 0.0, 0.0))).normalized())
                fx.rifle(tf, where, size=0.75 if man["weapon"] == "lmg" else 1.0, light=True)
                continue
            size = {"footstep": 0.0, "knee_down": 0.55, "body_fall": 1.0, "rifle_drop": 0.3}.get(e["fx"])
            if size is None:
                continue
            bone = e.get("bone") or "weapon"
            at = A @ Vector(st["H"][bone].tolist())
            at.z = stage.height(at.x, at.y)
            if size == 0.0:
                fx.kick(tf, at, fwd)
            else:
                fx.dust(tf, at, size=size)
        print(f"  man {man['i']} ({man['role']}, seed {HERO_SEEDS[man['i']]}) keyed", flush=True)
    print(f"  squad posed and keyed in {time.time() - t0:.0f} s")
    # the camera: low, ahead and to the right of the line, easing back as the men come on
    cam = kit.camera(lens=35)
    for f in range(frames):
        eye, tgt = hero_camera(f / HERO_FPS)
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
    "idle": "motion capture (CMU 77_02): standing, shifting his weight and looking about, rifle held across the body at the port",
    "walk": "motion capture (CMU 35_01): an ordinary walk, rifle at the port",
    "run": "motion capture (CMU 35_18): an easy jog, rifle at the port",
    "advance": "motion capture (CMU 35_18 with a crouch laid over it): the fast crouched run at the ready, butt in the shoulder, muzzle forward",
    "creep": "motion capture (CMU 136_09): a careful crouched walk at the ready",
    "flee": "keyed: running away in panic, rifle carried low in the right hand, left arm pumping, a look back over the shoulder",
    "crawl": "keyed by the mocap animator (mocap.py; CMU has no belly crawl): the low crawl under fire, rifle across the forearms",
    "kneel_down": "keyed: from standing to kneeling on the right knee, rifle coming up to the aim",
    "stand_up": "keyed: from kneeling (aimed) back to standing at port",
    "go_prone": "motion capture (CMU 77_16, played backwards): from the ready down onto a knee, the hands, and flat on the belly, the rifle by the drill: never out of his right hand, butt to the ground first, the fist on its knuckles as he lies down, then rolled forward into the aim with the left hand coming onto it (it ends in fire_prone's aim)",
    "fire_stand": "keyed: standing, aimed: shot, recoil, works the bolt, back on aim",
    "fire_kneel": "keyed: kneeling, aimed: the same",
    "fire_prone": "keyed: lying, aimed, elbows on the ground: shot, recoil, works the bolt",
    "lmg_burst": "keyed: the gunner prone behind the light machine gun on its bipod: a burst with the shoulder shaking (needs the LMG on the weapon bone)",
    "throw_grenade": "keyed: rifle to the left hand, wind-up and overarm throw from the right, recover",
    "cower": "keyed: crouched low behind the rifle held upright, head down, flinching (pinned)",
    "die_back": "motion capture (CMU 90_18): hit in front, the feet go from under him and he lands flat on his back; the rifle flies from his hands",
    "die_forward": "motion capture (CMU 140_01, played backwards and faster): hit on the move, a jolt, the knees buckle and he falls on his face; the rifle is thrown ahead",
    "die_kneel": "keyed: hit: doubles over, sinks to his knees, falls to the side; the rifle falls from his hands to his right",
}


LIMITS = [
    "The fingers are MakeHuman's own, slimmed a little and half curled in the rest pose; the rig closes the four fingers "
    "together with one bone (fingers_l/r) and the thumb with another, so a hand wraps a grip but cannot point or spread.",
    "The uniform takes its weights from MakeHuman's clothing shells (the nearest place on the suit, or on the skirt for the "
    "tunic's skirt, evened along the cloth); the head, neck and hands keep MakeHuman's own. There is no cloth simulation: in "
    "the deepest poses (prone, the deaths) the tunic can crease at the armpits and the hips.",
    "In the keyed clips the two-bone IK reaches its targets everywhere except throw_grenade (the left foot up to 4 cm "
    "for a moment).",
    "Where a knee would go through the ground, the solver lifts the pelvis for that frame (no knee joint goes below 4.5 cm), "
    "so a man going down can sit a touch high for a moment.",
    "The clips' low poses (kneeling, lying) were set out on the first, sculpted man and are scaled to MakeHuman's man by the "
    "rig (his pelvis's drop and his feet's spread); the standing poses and the gaits are fitted to his legs directly.",
    "The kit on the pelvis (bread bag, water bottle, entrenching tool, gas-mask canister) is rigid: lying down the canister "
    "and the bottle stand up off the back and hip as real kit would, and in the deaths a piece can dip into a thigh or "
    "the ground.",
    "The face is MakeHuman's (a different one for each seed), with no texture: brows, stubble, the flush and the nails are "
    "studio shading from each point's place on the face or hand. The game gets the plain skin paint.",
    "The folds are few and sculpted into the mesh (no normal maps): the cloth bunched above the belt, wrinkles across the "
    "shoulder blades, the turned hem, the trousers bloused over the boot tops. Rings of folds at the elbows and knees were "
    "taken out: at this triangle count they read as dents.",
    "The pictures subdivide the body, the helmet and the round kit once when rendering (studio only, with the sharp edges "
    "creased). The files carry the plain LOD0 mesh, so at the very closest camera the game shows more facets than the "
    "pictures.",
    "The cloth wear in the pictures (mottling, dust on knees and boots, darker folds) is a studio material; the exports carry "
    "plain paint colours for the game to tint.",
    "lmg_burst only makes sense with the LMG (soldier_GER_lmg.glb); on a rifleman's file the rifle goes where the LMG would be.",
    "Every army's man has a game JSON (soldier_GER.json, soldier_POL.json, ...), all on the same skeleton and clips.",
    "The belt's ammunition pouches are skinned like the body under them. In the deepest folds at the hips (creep, going "
    "prone, falling forward, cowering) the thigh comes up against the belly and the front pouches sink into that crease, "
    "up to their whole depth for a moment: no weighting of the existing bones keeps them out (measured: the torso's, the "
    "pelvis's, the thigh's and blends of them). A pouch bone per side that swings out of the fold is the fix, held for "
    "after release 1.",
    "The motion-capture clips are carried from human.py's rig to ours by world turns (the joints are the same; the bone "
    "rolls differ), so they keep mocap.py's own flaws (see its README: the run is an easy jog, the advance is fast, the "
    "falls are reversed or slip-like takes). The hands hold the rifle by mocap.py's grip, the fingers 12 degrees more "
    "curled than at rest; up close the fingers do not wrap the stock exactly.",
    "In die_forward, die_back and die_kneel the rifle leaves the hand and falls by a simple throw (its speed, "
    "gravity, a turn onto its left side) and lies where it lands, put clear of where he lies. There is no bounce, and "
    "it can lie a centimetre into or above uneven ground. In go_prone it never leaves his right hand (the drill, "
    "below).",
    "Going from a mocap clip to a keyed one (go_prone into fire_prone, the advance into kneel_down) the game blends the "
    "two; the pictures blend 0.1 to 0.6 s. go_prone ends in fire_prone's aim (trunk, arms and rifle; the capture lies "
    "3 cm lower and its legs are its own), so the blend into fire_prone hardly moves the rifle.",
    "go_prone's arms are not the capture's: the capture lets go of the rifle, so the right arm is solved onto the "
    "rifle by the drill (the right hand slides to the balance, the butt meets the ground first and he falls forward "
    "over it, the fist comes down on its knuckles with the rifle level, then the rifle is rolled forward into the aim) "
    "and the left arm comes off its catch onto the rifle at the end. Where that would put a hand, the elbow or the "
    "rifle into the ground the arm is raised just clear; the left hand comes onto the rifle from up to 5 cm above it.",
    "The hero clip's men stand on the stage's rolling ground by their root only: on a slope a foot can float or sink a "
    "centimetre or two (the stubble hides most of it).",
]


def write_readme(info):
    """README.md beside the files."""
    tr = info.get("tris", {})
    parts = info.get("parts", {})
    for n, v in info.get("mocap", {}).items():
        CLIPS[n].update(v)
    L_ = []
    w = L_.append
    w("# The infantry soldier\n")
    w("One man, rigged and animated for the battles' close camera, from whom the squad of twelve is made: the German rifleman "
      "(stahl helmet, tall boots, gas-mask canister) and the Polish rifleman (round helmet, ankle boots with gaiters, pack), "
      "as FIGURES has them (Rifleman_Stahl, Rifleman_Round). Our own design throughout: no other game's or maker's names, models, "
      "art or animation were used or looked at.\n")
    w("Made entirely by `Tools/battle_units/soldier.py` with its helpers `human.py` (MakeHuman's man: mesh, skeleton, "
      "weights, clothing shells), `sculpt.py` (distance-field sculpting and meshing) and `kit.py` (scene, camera, export "
      "helpers). Run it with the `bpy` module (Blender 5.0.1): `python soldier.py` does everything; "
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
    w("- **Body**: MakeHuman's base man (CC0; built by `human.py`), 1.73 m and 1.75 m in his boots: his head, neck and "
      "hands are MakeHuman's mesh, thinned to the budget, with his own eyes, a different face for each seed (the German "
      "files are seed 3, the Pole seed 7, the hero squad twelve seeds of their own), and his fingers slimmed a little and "
      "half curled to hold a rifle. The skin the uniform covers is deleted.")
    w("- **Uniform**: made from MakeHuman's clothing shells (the tights and the skirt) read as distance fields, each face "
      "pushed out by how loose the cloth is over its bones, and our own shapes laid on them: the tunic (a stand collar with "
      "small collar patches, a button strip with five buttons, four patch pockets with buttoned flaps, a skirt over the hips "
      "with a turned hem, shoulder straps, turned-back cuffs), the trousers bloused over the boot tops, and boots on proud "
      "soles with heels (tall marching boots for the German; ankle boots with canvas gaiters and their straps for the "
      "Pole). Each cloth is a layer joined hard to the next and the mesh is cut along where the paints meet, so the collar, "
      "cuffs, hem, soles and boot tops are clean edges. The cloth bunches above the belt and wrinkles across the shoulder "
      "blades.")
    w("- **Kit** (separate meshes on the same skeleton): belt with buckle (skinned like the body), three-cell ammunition pouches "
      "either side of the buckle, Y-straps laid over the shoulders, the bread bag on the right hip with the water bottle (a "
      "flat kidney-shaped flask in a felt cover, its cup strapped on) hung flat on it, the entrenching tool in its carrier "
      "flat on the left hip with the handle down and back along the thigh, and the gas-mask canister upright behind the "
      "right hip (German) or the pack with a blanket roll on top (Polish); the bag, bottle, tool and canister ride rigidly "
      "on the pelvis and the pack on the chest.")
    w("- **Helmets**: twelve kinds as rigid shells (outside, rolled rim, inside, chin strap): stahl, round and dome first, then "
      "brodie, pot, soviet, adrian, eastern, cap, peaked, sun and fieldcap. Each one's origin is the head bone's head with "
      "x forward and z up, so it sits on `socket_head` with no offset.")
    w("- **Weapons**: a generic bolt-action rifle of our own (1.10 m: one-piece stock with half-pistol grip and steel butt plate, "
      "receiver, bolt with a turned-down handle, two barrel bands, hooded front sight, ramp rear sight, trigger guard, leather "
      "sling) and a generic light machine gun (a perforated barrel jacket, a side box magazine on the left, a pistol grip, a "
      "carry handle, a folding bipod). Origin at the right hand's grip; both ride on the `weapon` bone (the bolt and the "
      "bipod on their own bones under it).\n")
    w("## The body: MakeHuman\n")
    w("The man is MakeHuman's base mesh, skeleton, weights and proxy shells (the MakeHuman project's assets, released CC0), "
      "read by `human.py`. For our clips `human.py` maps MakeHuman's joints onto our 26 bones: the pelvis takes spine05, "
      "spine_01 takes spine04, spine_02 takes spine03 and spine02, the chest takes spine01 and the breasts; each upper arm takes both "
      "of MakeHuman's upper-arm bones; the shoulder (shoulder01) is shared half and half between the clavicle and the upper "
      "arm; the upper arm's pivot sits three quarters of the way out along the shoulder joint; and `build(..., curl=)` bends "
      "the fingers in the rest mesh.\n")
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
    w("\nBudgets: LOD0 about 9,000 (raised from 6,000 for the folds and the face, as the art lead allowed): uniform about "
      "2,900 with its cut edges, head 1,260, hands 520 each, helmet 1,144, rifle 800; LOD1 about 1,800, "
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
      "lower, straps, helmet, pack, skin, wood, steel, leather` (the soles), `blanket` (the water bottle's felt), and `eye` (in "
      "soldier_GER.json the eyes are painted as `boots`). The "
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
    w("\n`root` stays at the origin (no root motion). `pelvis` moves (its offset is keyed). `fingers_l/r` close the four "
      "fingers together and `thumb_l/r` the thumb. `weapon` (child of root) carries the gun and is placed by the clips; `bolt` slides and lifts "
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
    w(f"{len(CLIPS)}, each its own action (glb: separate animations; fbx: separate takes; json: `clips`), at 60 frames a "
      "second (frames 0..N, so a loop's last frame is its first again); soldier_GER.json samples them 15 times a second. All "
      "baked to plain keys (no constraints in the files).\n")
    w("- **Motion capture** (" + ", ".join(f"`{n}`" for n in MOCAP) + "): the body's movement is the mocap animator's "
      "(mocap.py and mocap_cmu/clips.json: CMU takes retargeted to human.py's rig, see the mocap README in "
      "battle-models/land/soldier/mocap/), carried onto our rig by world turns and resampled from 30 to 60 frames a second. "
      "Over it the rifle is held in both hands by mocap.carry_rifle (at the port in idle, walk and run; at the ready in "
      "advance and creep, and at the start of go_prone and the falls), and our `weapon` bone is keyed to where the right "
      "hand holds it. The low crawl holds the rifle in its own arms. In the falls the rifle leaves his hand. In "
      "go_prone it never does: the capture lets go of it, so his right arm is solved onto it by the drill (the butt "
      "down first, the fist on its knuckles, then rolled forward into the aim) and his left hand takes it at the end.")
    w("- **Keyed** (the weapon work and the rest: " + ", ".join(f"`{n}`" for n in CLIPS if n not in MOCAP) + "): posed "
      "by the script, feet planted and hands put on the weapon by two-bone IK solved in soldier.py.")
    w(f"- **A real body's range**: every joint of every clip (and of every man in the hero clip) is measured on every 60th of a "
      f"second against human ranges of motion by `arms_check.py` (rom.py's limits; joints turning faster than 900 degrees a "
      f"second are flagged); pictures/arms_check.png is its sheet. Aimed (fire_stand, fire_kneel, fire_prone, lmg_burst and "
      f"the hero clip's firers) the rifle is lifted to the face rather than the face dropped to the rifle: the butt in the "
      f"shoulder's pocket under the collarbone, the right shoulder up, the right elbow below the shoulder, the right eye on "
      f"the sights' line (within half a centimetre), the head {SIGHT_PITCH[0]:g}-{SIGHT_PITCH[1]:g} degrees forward and "
      f"canted {SIGHT_CANT[0]:g}-{SIGHT_CANT[1]:g} to the right, the cheekbone on the comb; the eye sits 30-40 cm behind "
      f"the rear sight, which is on the barrel ahead of the receiver.")
    w("- The motion capture credit: *The data used in this project was obtained from mocap.cs.cmu.edu. The database was "
      "created with funding from NSF EIA-0196217.* (mocap_cmu/CREDITS.md lists the takes.)\n")
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
    w("\n- **Strides**: " + "; ".join(f"one {n} loop covers **{c['stride']:g} m in {c['long']:.3g} s** ({c['stride'] / c['long']:.2f} m/s)"
      for n, c in CLIPS.items() if c["loop"] and c["stride"]) + ". Play rate = the unit's speed / those speeds, so the "
      "planted foot keeps still on the ground. (The mocap strides are for this man's legs: mocap.py's for its 1.75 m "
      "reference man scaled by leg length.)")
    w("- **Where a one-shot leaves him**: go_prone, die_forward and die_back move his pelvis over the ground while the "
      "root stays put; `end_offset` (in soldier_GER.json's clips and as an extra on the actions, metres, x forward, y to "
      "his left on the actions and to his right in the JSON, as everywhere else) is where the pelvis ends (y left): " + "; ".join(f"{n} {', '.join(f'{v:g}' for v in CLIPS[n]['end'])}" for n in MOCAP
      if not CLIPS[n]["loop"]) + ". Move the actor's root by its x and y when the clip ends (or blend into the next clip "
      "from there).")
    w("- **Events and cues** (the sound director's names; no sound files ship): shots `rifle_shot` (the game may swap in "
      "a per-army shot) and `lmg_burst` (the first round of each burst; the other rounds are flashes with cue null); "
      "`footstep` (the game picks footstep_01 to _04), `knee_down`, `body_fall` (01 to 04), `bolt` while the bolt is worked "
      "(from `t` to `until`; the game picks the nation's own, bolt_k98 for Germany), `kit_rattle` (01 or 02) as he stands up, "
      "on the first step of a run or advance (`first_loop_only`) and when a dropped rifle lands (`rifle_drop`), "
      "`grenade_throw` at the start of the throw (pin and swish; the blast is the game's). `grenade_release` (0.95) has no "
      "cue: it is where the game lets the grenade go. In the crawl each elbow planted is `elbow_down` with a footstep cue. "
      "The mocap clips' footsteps, knees and falls are read off the motion (a heel or ball reaching the ground, a knee "
      "joint below 12 cm, the mid-back within 5 cm of where it ends). Each event names "
      "its socket or its bone. The events are in soldier_GER.json's clips and on every animation of the .glb files as "
      "extras (`events`, a JSON string), with `loop` and `stride`.")
    w("- Firing loops end where they start, so a man can fire round after round; kneel_down ends on fire_kneel's first frame "
      "and stand_up starts on it. go_prone ends lying in fire_prone's aim: blend into fire_prone (about 0.25 s) from "
      "its end_offset. The deaths end lying still: hold the last frame.")
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
    w("| advance (closing under fire) | advance; creep when slow | advance, or crawl in the open | flee |")
    w("| fight, open ground | fire_stand (a few kneel: kneel_down, fire_kneel) | go_prone, fire_prone; crawl to move | flee |")
    w("| fight, in woods or buildings | kneel_down, fire_kneel | fire_kneel; go_prone, fire_prone at the edge | flee |")
    w("| fight, within 30 m of the enemy | throw_grenade now and then, then back to firing | the same, less often | flee |")
    w("| rout | flee | flee | flee |")
    w("| down (a man lost) | die_back if standing and hit from the front; die_forward if moving; die_kneel if kneeling | the same | die_forward |")
    w("\n- The LMG gunner plays go_prone then lmg_burst whenever his block fights; he walks, runs, flees and dies as the others.")
    w("- From prone or kneeling to moving: stand_up (from kneeling; from prone play go_prone backwards, 1.0 s) and then walk or run.")
    w("- Pinned (heavy fire, wavering, no cover): cower, a few men at a time.\n")
    w("## How to import into Unreal 5\n")
    w("- **One shared skeleton**: import `soldier_GER.glb` first (Interchange glTF importer: Skeletal Mesh, Import Animations on). "
      "It makes the skeleton and the animation sequences. Import `soldier_POL.glb` and `soldier_GER_lmg.glb` choosing that "
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
    w("- `pictures/before-after.png` (1280x720, 48 samples): three abreast under the same camera and sun: today's lay "
      "figure (Rifleman_Stahl from the game's file, painted by number), the first pass (the soldier sculpted from numbers) "
      "and the new German on MakeHuman's man.")
    w("- `pictures/turntable.mp4/.gif`: the German and the Pole side by side, a full turn in 6 s at 15 frames a second "
      "(960x540, 16 samples, denoised).")
    w("- `pictures/walk`, `run`, `advance`, `fire_stand`, `prone` (go_prone then fire_prone twice), `crawl`, `die_back` (.mp4 "
      "and .gif, 30 frames a second, 960x540, 16 samples): a close game camera 4.6 to 6.5 m away, 16 to 24 degrees down, on the "
      "field ground. The clips are in place, so in walk, run, advance and crawl the ground slides back under him at the "
      "clip's own speed (the planted foot should stay put on it); one clip blends into the next as the game would. The muzzle flash, light and smoke in fire_stand and prone, and the dust where he goes "
      "down, are the shared Effects (below), studio stand-ins only; the game makes its own from the events.")
    w("- `pictures/squad_advance.mp4/.gif` (the hero clip, 8 s, 960x540, 30 frames a second, 16 samples): the squad of twelve "
      "(the gunner in the middle), each with his own face and build from his own seed, advances north at the ready (the "
      "mocap advance) across the stubble east of the road on the battle stage (battle_set.py), each man at his own pace "
      "and step. The machine-gun team goes to ground first and sets up (the bipod unfolds, then bursts); two riflemen throw "
      "themselves down (the mocap go_prone) and fire; one man is hit and falls on his face, his rifle thrown ahead; late "
      "on three drop to a knee and fire; the rest keep coming. A low camera ahead and to the right of the line eases back "
      "as they come on. Muzzle flashes (a short flame and a brief light), grey smoke and tan dust (where feet, knees and "
      "bodies meet the ground) as many small soft puffs are the sea and air team's tested Effects (shared/sea-air/stage.py), "
      "copied into soldier.py and scaled down for small arms; they are the pictures' stand-ins, the game makes its own "
      "from the events.")
    w("- `pictures/squad.png` (1280x720, 48 samples): on the battle stage, twelve men (eleven riflemen and the gunner) in a 20 x 10 m patch east of the road, each "
      "frozen in a moment of a clip (prone and kneeling fire, standing fire, the crawl, a grenade throw, the gunner, two "
      "moving up, one walking and one at the advance), "
      "seen low from behind the right of the line, looking north-north-west toward the stone walls, the lone tree, the "
      "farm and the wood's edge, with the firing men's smoke and a thin haze of drifting smoke across the field.")
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
        json.dump(info, f, indent=1, default=float)


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
            export_all(info, tuple(a[a.index("--nations") + 1].split(",")) if "--nations" in a else ("GER", "POL"))
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
