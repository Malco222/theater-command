"""Many men from one: different faces, and helmets in their field finishes, for the battle soldiers.

Brian, 20 November 2026: "Make different faces for the soldiers so it's all not the same looking person", and "Make
the helmets details with camo and markings on them".

Each army's soldier file (battle-models/land/soldier/soldier_<ARMY>.json) is one man, so a squad drawn from it is
twelve copies of one face under one helmet. This file adds two lists to that same file, and changes nothing else in it:

* `faces`: eight faces for the army's man. Face 0 is the man as the file has him; faces 1 to 7 move the points of
  his face (MakeHuman's open face shapes, a different mix for each, fitted onto the file's own points), and give his
  skin a tone, and his hair, brows, stubble or moustache a colour. Nothing he wears moves: his face is held still
  wherever it comes near his helmet, cap, hood, collar or chin strap, so every face fits the same kit.
* `helmets`: the army's helmet in the finishes it wore in the field (plain, painted camouflage, covers, nets with
  scrim, wire with foliage, whitewash, and the period's markings), each one a small rigid mesh on the head bone, with
  the months and the ground it belongs to and how common it was. `helmet_tris` says which triangles of the man's own
  meshes are his plain helmet, to leave out when another is drawn.

The game picks one face and one helmet for each man (by his block and his place in it, so he keeps them all battle).
How to read and draw both is in the file's `about_variety`, and in battles/3d-handoff-land.md section 7.

    python variety.py                         every army's file in battle-models/land/soldier/, and its picture sheet
    python variety.py --armies GER,SOV --only faces,helmets --dir /some/copy/of/the/soldier/folder/

It works on the files as delivered (no rebuild of the man, his rig or his clips), and can be run again after any
re-export: it takes off what it added before and adds it anew. Run it with Blender's Python (the bpy module).
Everything here is our own: the face shapes are MakeHuman's CC0 targets (makehuman_cc0/, fetch_faces.sh there), the
helmet finishes our own drawing of what the armies wore, from the historian's notes (helmet_finishes.py says which).
"""

import json
import math
import os
import random
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402,F401  (first: it puts Blender's own modules on the path)
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402

import human  # noqa: E402

SOLDIERS = "/mnt/project-files/battle-models/land/soldier/"
SCRATCH = "/home/user/art/variety/"
SOLE = 0.02                       # (soldier.SOLE: the man is made 1.73 m tall and stood on his soles)
PAINT = {"tunic": 0, "trousers": 1, "boots": 2, "lower": 3, "straps": 4, "helmet": 5, "pack": 6, "skin": 7, "wood": 8,
         "steel": 9, "coat": 10, "coat_dark": 11, "leather": 12, "blanket": 13, "hoof": 14}
EXTRA_PAINTS = {"medic_white": 15, "medic_red": 16, "rank": 17, "rocket": 18, "radio": 19}   # (the American files' own, by colour name)
GAME_SKIN = "C49A78"              # (the game's figure skin, CampaignSoldiers.cpp: tints are against the file's own skin, else this)
FACES = 8                         # faces per army: 0 is the man as delivered


def _n(v):
    v = np.asarray(v, float)
    return v / (np.linalg.norm(v, axis=-1, keepdims=True) + 1e-12)


def smooth(x, a, b):
    t = np.clip((np.asarray(x, float) - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def hex_rgb(h):
    h = h.lstrip("#")
    return np.array([int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)])


# ================================================================================================ the man's surface
class Man:
    """MakeHuman's man for one look, as human.build() makes him (the same scale, the same lift onto his soles), but as
    plain arrays: no Blender objects. Points in the file's own frame but y to his LEFT (Blender's), metres."""

    _tris = None

    def __init__(self, look):
        V, UV, groups = human._load_obj()
        S = human._shaped(look)
        bf = groups["body"]
        feet = min(S[i].y for f, _ in bf for i in f)
        crown = max(S[i].y for f, _ in bf for i in f)
        k = (1.75 - SOLE) / ((crown - feet) * 0.1)
        lift = Vector((0, 0, -feet * 0.1 * k + SOLE))
        self.P = np.array([tuple(human._bl(v, k) + lift) for v in S])
        skel = json.load(open(os.path.join(human.MH, "default.mhskel")))
        self.J = {n: self.P[idx].mean(axis=0) for n, idx in skel["joints"].items()}
        if Man._tris is None:
            t = []
            for fc, _ in bf:
                t.append((fc[0], fc[1], fc[2]))
                if len(fc) == 4:
                    t.append((fc[0], fc[2], fc[3]))
            Man._tris = np.array(t)
            Man._eyes = {s: sorted({i for f, _ in groups[f"helper-{s}-eye"] for i in f}) for s in ("l", "r")}
        self.tris = Man._tris
        A, B, C = (self.P[self.tris[:, i]] for i in range(3))
        fn = np.cross(B - A, C - A)
        vn = np.zeros_like(self.P)
        for i in range(3):
            np.add.at(vn, self.tris[:, i], fn)
        self.N = _n(vn)
        self.eye = {s: self.P[Man._eyes[s]].mean(axis=0) for s in ("l", "r")}
        self._bvh = None

    def bvh(self):
        if self._bvh is None:
            self._bvh = BVHTree.FromPolygons([tuple(p) for p in self.P], [tuple(int(i) for i in t) for t in self.tris])
        return self._bvh

    def where(self, pts):
        """For each point: the triangle of this surface nearest it, the barycentric weights there, and the distance."""
        bvh = self.bvh()
        tri, bary, dist = np.zeros(len(pts), int), np.zeros((len(pts), 3)), np.zeros(len(pts))
        for i, p in enumerate(pts):
            loc, _, ti, d = bvh.find_nearest(Vector(p))
            a, b, c = (self.P[j] for j in self.tris[ti])
            v0, v1, v2 = b - a, c - a, np.array(loc) - a
            d00, d01, d11, d20, d21 = v0 @ v0, v0 @ v1, v1 @ v1, v2 @ v0, v2 @ v1
            den = d00 * d11 - d01 * d01
            v = (d11 * d20 - d01 * d21) / den
            w = (d00 * d21 - d01 * d20) / den
            tri[i], bary[i], dist[i] = ti, (1 - v - w, v, w), d
        return tri, bary, dist

    def at(self, tri, bary, X=None):
        X = self.P if X is None else X
        return np.einsum("ij,ijk->ik", bary, X[self.tris[tri]])


def kabsch(A, B, w):
    """The turn R and shift t that best take points A onto B (weights w): B ~ A @ R.T + t."""
    w = w / w.sum()
    ca, cb = (w[:, None] * A).sum(0), (w[:, None] * B).sum(0)
    H = ((A - ca) * w[:, None]).T @ (B - cb)
    U, _, Vt = np.linalg.svd(H)
    d = np.sign(np.linalg.det(Vt.T @ U.T))
    R = Vt.T @ np.diag((1.0, 1.0, d)) @ U.T
    return R, cb - R @ ca


# ================================================================================================ the faces
# Each army's eight faces: who they are (the regional mix of MakeHuman's three base shapes, by human.NATION_MIX's
# order: European, African, East Asian), how old, the skin's tone (a multiplier on the file's skin colour), the hair
# (the colour of hair, brows and stubble), and what is on the lip and jaw. Face 0 is the man as delivered: only his
# hair and brows are coloured (the file's man has none).
TONES = {
    "fair": (1.07, 1.04, 1.03), "pale": (1.04, 1.03, 1.03), "ruddy": (1.06, 0.95, 0.9), "medium": (1.0, 1.0, 1.0),
    "weathered": (0.93, 0.88, 0.84), "tanned": (0.88, 0.82, 0.76), "olive": (0.94, 0.92, 0.82), "brown": (0.78, 0.7, 0.64),
    "east_light": (1.03, 1.0, 0.92), "east": (0.97, 0.93, 0.84), "east_tan": (0.88, 0.82, 0.72),
}
HAIR = {"black": "1C1916", "dark": "2E241D", "brown": "4A3727", "mid": "66503A", "light": "86704F", "fair": "A08A62",
        "red": "6E3F25", "grey": "6F6A63", "greying": "574F47"}
EURO, CENTRAL, EAST = (0.75, 0.12, 0.13), (0.42, 0.04, 0.54), (0.1, 0.05, 0.85)
PLAN = {
    "_european": [
        dict(), dict(mix=EURO, age=0.1, tone="fair", hair="fair", lip="clean"),
        dict(mix=EURO, age=0.35, tone="ruddy", hair="red", lip="stubble"),
        dict(mix=EURO, age=0.05, tone="pale", hair="dark", lip="clean", build="lean"),
        dict(mix=EURO, age=0.8, tone="weathered", hair="greying", lip="moustache"),
        dict(mix=EURO, age=0.25, tone="tanned", hair="brown", lip="stubble", build="broad"),
        dict(mix=EURO, age=0.5, tone="medium", hair="light", lip="heavy"),
        dict(mix=EURO, age=0.0, tone="medium", hair="mid", lip="clean", build="round"),
    ],
}
PLAN["GER"] = PLAN["_european"]
PLAN["POL"] = [dict(), *PLAN["_european"][1:4], dict(mix=EURO, age=0.75, tone="weathered", hair="greying", lip="moustache"),
               dict(mix=EURO, age=0.3, tone="ruddy", hair="light", lip="moustache", build="broad"), *PLAN["_european"][6:]]
# The Red Army: mostly Russians, Ukrainians and Belarusians, and many from Central Asia and the Caucasus.
PLAN["SOV"] = [
    dict(), dict(mix=EURO, age=0.05, tone="fair", hair="fair", lip="clean", build="lean"),
    dict(mix=CENTRAL, age=0.2, tone="tanned", hair="black", lip="clean"),
    dict(mix=EURO, age=0.85, tone="weathered", hair="greying", lip="moustache"),
    dict(mix=EURO, age=0.3, tone="ruddy", hair="light", lip="heavy", build="broad"),
    dict(mix=(0.6, 0.04, 0.36), age=0.4, tone="olive", hair="black", lip="moustache"),
    dict(mix=EURO, age=0.15, tone="pale", hair="mid", lip="stubble", build="round"),
    dict(mix=CENTRAL, age=0.55, tone="brown", hair="dark", lip="stubble"),
]
PLAN["ITA"] = [
    dict(), dict(mix=EURO, age=0.1, tone="olive", hair="black", lip="clean"),
    dict(mix=EURO, age=0.45, tone="tanned", hair="dark", lip="moustache"),
    dict(mix=EURO, age=0.05, tone="medium", hair="brown", lip="stubble", build="lean"),
    dict(mix=EURO, age=0.8, tone="weathered", hair="greying", lip="moustache", build="broad"),
    dict(mix=EURO, age=0.3, tone="brown", hair="black", lip="heavy"),
    dict(mix=EURO, age=0.2, tone="fair", hair="light", lip="clean", build="round"),
    dict(mix=EURO, age=0.35, tone="olive", hair="dark", lip="stubble"),
]
PLAN["_eastasian"] = [
    dict(), dict(mix=EAST, age=0.05, tone="east_light", hair="black", lip="clean"),
    dict(mix=EAST, age=0.35, tone="east_tan", hair="black", lip="stubble"),
    dict(mix=EAST, age=0.15, tone="east", hair="dark", lip="clean", build="round"),
    dict(mix=EAST, age=0.7, tone="weathered", hair="greying", lip="moustache"),
    dict(mix=EAST, age=0.25, tone="east_tan", hair="black", lip="clean", build="broad"),
    dict(mix=EAST, age=0.45, tone="east", hair="black", lip="stubble"),
    dict(mix=EAST, age=0.0, tone="east_light", hair="dark", lip="clean"),
]
PLAN["JAP"] = PLAN["CHN"] = PLAN["_eastasian"]
# The American army of 1944: draftees from every state, most of European stock (Irish, German, Italian, Polish, the
# old English and Scots), with Mexican-American and American Indian men in the same squads (the army's
# African-American soldiers served in their own units, which these two files are not).
PLAN["USA"] = [
    dict(), dict(mix=EURO, age=0.05, tone="fair", hair="fair", lip="clean", build="lean"),
    dict(mix=EURO, age=0.3, tone="ruddy", hair="red", lip="stubble"),
    dict(mix=EURO, age=0.2, tone="olive", hair="black", lip="stubble"),
    dict(mix=EURO, age=0.65, tone="weathered", hair="greying", lip="moustache", build="broad"),
    dict(mix=(0.6, 0.05, 0.35), age=0.25, tone="tanned", hair="black", lip="clean"),
    dict(mix=EURO, age=0.1, tone="medium", hair="brown", lip="clean", build="round"),
    dict(mix=EURO, age=0.4, tone="pale", hair="dark", lip="heavy"),
]
FILE_ARMY = {"SOVW": "SOV", "SOVS": "SOV", "ITAD": "ITA", "USAB": "USA", "USAZ": "USA", "USAL": "USA", "USAN": "USA", "USAM": "USA", "USAR": "USA",
             "USAP": "USA"}

# The extra face shapes (fetch_faces.sh), each a range the faces draw from: they make the faces differ more than
# human.look_for's own nose, chin, mouth and brows do.
EXTRA = (("head-rectangular", 0.0, 0.5), ("head-invertedtriangular", 0.0, 0.45), ("head-diamond", 0.0, 0.4),
         ("forehead-scale-vert-incr", -0.3, 0.5), ("forehead-trans-forward", -0.3, 0.4),
         ("eyebrows-trans-up", -0.2, 0.4), ("eyebrows-trans-forward", -0.2, 0.6),
         ("r-eye-scale-incr", -0.35, 0.3), ("r-eye-push1-in", -0.3, 0.6), ("r-eye-trans-up", -0.3, 0.3),
         ("r-eye-height2-incr", -0.3, 0.4), ("r-eye-bag-incr", 0.0, 0.5),
         ("nose-trans-up", -0.3, 0.3), ("nose-trans-forward", -0.2, 0.4), ("nose-point-up", -0.4, 0.4),
         ("nose-curve-convex", -0.4, 0.7), ("nose-greek-incr", -0.2, 0.6), ("nose-volume-incr", -0.3, 0.6),
         ("nose-flaring-incr", -0.2, 0.6), ("nose-nostrils-width-incr", -0.3, 0.5), ("nose-point-width-incr", -0.3, 0.6),
         ("nose-scale-depth-incr", -0.3, 0.5), ("nose-width2-incr", -0.3, 0.5), ("nose-base-up", -0.3, 0.3),
         ("mouth-scale-vert-incr", -0.3, 0.4), ("mouth-upperlip-volume-incr", -0.3, 0.5), ("mouth-angles-up", -0.4, 0.3),
         ("mouth-trans-forward", -0.3, 0.3), ("mouth-scale-depth-incr", -0.3, 0.4),
         ("chin-jaw-drop-incr", -0.3, 0.5), ("chin-height-incr", -0.3, 0.5), ("chin-bones-incr", -0.4, 0.6),
         ("chin-prognathism-incr", -0.3, 0.4), ("chin-cleft-incr", 0.0, 0.7),
         ("r-cheek-volume-incr", -0.4, 0.5), ("r-cheek-inner-incr", -0.3, 0.5), ("r-cheek-trans-down", -0.3, 0.5),
         ("r-ear-scale-incr", -0.2, 0.3), ("r-ear-flap-incr", -0.1, 0.35), ("r-ear-lobe-incr", -0.2, 0.4),
         ("r-ear-rot-backward", -0.15, 0.2), ("r-ear-wing-incr", -0.1, 0.25))
EXTRA_SCALE = 0.9                # (how far into each range a face goes: past about this, MakeHuman's shapes caricature)
SIGN_SCALE = 0.8                  # (the one strong feature of each face, as strong as this: a hooked nose, not a beak)
BUILD_SCALE = 0.7
HEAD_SHAPES = ("head-oval", "head-square", "head-round", "head-triangular")
CLAMPS = {"r-cheek-bones-incr": (-0.3, 0.5), "r-cheek-volume-incr": (-0.45, 0.6), "r-cheek-inner-incr": (-0.35, 0.4),
          "r-cheek-trans-down": (-0.35, 0.4), "nose-hump-incr": (-0.3, 0.6), "nose-scale-vert-incr": (-0.4, 0.6),
          "head-fat-incr": (-0.4, 0.5), "universal-male-young-averagemuscle-minweight": (0.0, 0.7),
          "chin-prominent-incr": (-0.4, 0.7), "chin-bones-incr": (-0.4, 0.6)}
# One strong feature for each of faces 1 to 7, so that no two read alike even at a distance (the rest of each face is
# drawn at random from EXTRA and human.look_for). The armies take them in different orders.
SIGNS = [
    {"nose-scale-vert-incr": 0.7, "nose-hump-incr": 0.9, "nose-scale-depth-incr": 0.6, "nose-point-up": -0.4},          # a long hooked nose
    {"nose-scale-horiz-incr": 0.8, "nose-flaring-incr": 0.9, "nose-point-width-incr": 0.7, "nose-scale-vert-incr": -0.4},  # a broad short nose
    {"head-oval": 1.0, "chin-height-incr": 0.8, "r-cheek-volume-incr": -0.7, "mouth-scale-horiz-incr": -0.3},             # long and narrow
    {"head-square": 1.0, "chin-width-incr": 0.9, "chin-bones-incr": 0.9, "neck-scale-horiz-incr": 0.6},                   # a square jaw
    {"eyebrows-trans-forward": 1.0, "forehead-trans-forward": 0.6, "r-eye-push1-in": 0.9, "nose-greek-incr": 0.6},          # a heavy brow, deep eyes
    {"head-round": 1.0, "head-fat-incr": 0.6, "r-cheek-volume-incr": 0.9, "chin-prominent-incr": -0.5, "nose-point-up": 0.7},  # round, a snub nose
    {"chin-prominent-incr": 1.0, "chin-prognathism-incr": 0.6, "mouth-lowerlip-volume-incr": 0.7, "r-cheek-bones-incr": 0.9},  # a jutting chin, high cheekbones
]
BUILD = {"lean": {"head-fat-incr": -0.6, "universal-male-young-averagemuscle-minweight": 0.8, "r-cheek-volume-incr": -0.5},
         "broad": {"head-square": 0.8, "chin-width-incr": 0.6, "head-fat-incr": 0.2, "neck-scale-horiz-incr": 0.6},
         "round": {"head-round": 0.8, "head-fat-incr": 0.5, "r-cheek-volume-incr": 0.5, "chin-prominent-incr": -0.3}}


def face_look(army, slot, base_seed):
    """MakeHuman's target weights for face `slot` of an army (slot 0: the delivered man's own)."""
    spec = PLAN.get(army, PLAN["_european"])[slot]
    nation = army if army in human.NATION_MIX else "_default"
    if slot == 0:
        return human.look_for(base_seed, nation), spec
    seed = 1000 + 97 * slot + sum(map(ord, army))
    w = human.look_for(seed, nation)
    rnd = random.Random(seed * 31 + 5)
    c, a, s = spec.get("mix", human.NATION_MIX.get(nation, human.NATION_MIX["_default"]))
    j = [rnd.uniform(-0.06, 0.06) for _ in range(3)]
    mix = [max(0.0, c + j[0]), max(0.0, a + j[1]), max(0.0, s + j[2])]
    tot = sum(mix)
    w["caucasian-male-young"], w["african-male-young"], w["asian-male-young"] = (m / tot for m in mix)
    for t, lo, hi in EXTRA:
        v = rnd.uniform(lo, hi) * EXTRA_SCALE
        w[t] = v
        if t.startswith("r-"):                         # (the two sides alike, near enough: no one's face is quite even)
            w["l-" + t[2:]] = v + rnd.uniform(-0.05, 0.05)
    w["head-age-incr"] = spec.get("age", 0.2) * 0.9
    if spec.get("age", 0) > 0.5:
        w["r-eye-bag-incr"] = w["l-eye-bag-incr"] = 0.4 + 0.4 * spec["age"]
    sign = SIGNS[(slot - 1 + sum(map(ord, army))) % len(SIGNS)]
    if any(t in HEAD_SHAPES for t in sign) or any(t in HEAD_SHAPES for t in BUILD.get(spec.get("build"), {})):
        for t in HEAD_SHAPES:                          # (one head shape, not two stacked: that swelled the skull)
            w[t] = 0.0
    for t, v in sign.items():
        w[t] = v * SIGN_SCALE
        if t.startswith("r-"):
            w["l-" + t[2:]] = v * SIGN_SCALE
    for t, v in BUILD.get(spec.get("build"), {}).items():
        w[t] = w.get(t, 0.0) + v * BUILD_SCALE
        if t.startswith("r-"):
            w["l-" + t[2:]] = w.get("l-" + t[2:], 0.0) + v * BUILD_SCALE
    # where several of the above add up on one feature, keep it in a man's range: past these the file's few points
    # over the cheek fold into a shelf or a hollow (a lean build on a narrow face hollowed the cheeks twice over)
    for t, (lo, hi) in CLAMPS.items():
        for k in ([t] if not t.startswith("r-") else [t, "l-" + t[2:]]):
            if k in w:
                w[k] = min(hi, max(lo, w[k]))
    return w, spec


def face_marks(man, X):
    """For points X on (or near) this man's face: how much each is hair (the short hair under the helmet, the
    sideburns and the nape), brow, beard shadow (jaw, chin and upper lip) and moustache, each 0 to 1."""
    J = man.J
    el, er = man.eye["l"], man.eye["r"]
    eye = (el + er) / 2
    head = J["head____head"]
    mid_y = eye[1]
    lower, upper = J["oris01____head"], J["oris05____head"]          # (MakeHuman's lower and upper lip)
    zm = (lower[2] + upper[2]) / 2                                     # the line of the mouth
    z_sn = zm + 0.016                                                  # under the nose
    front = max(lower[0], upper[0]) - 0.018
    x, y, z = X[:, 0], X[:, 1] - mid_y, X[:, 2]
    side = np.abs(y)
    back = eye[0] - 0.03 - x
    # hair, as human.build's scalp group: above a hairline that falls toward the nape; the ears stay skin
    line = eye[2] + 0.07 - 0.55 * np.maximum(0.0, back) - 0.25 * np.maximum(0.0, back - 0.06)
    hair = smooth(z, line - 0.006, line + 0.012) * (z > head[2] - 0.02)
    ears = (side > 0.068) & (z < eye[2] + 0.045) & (back < 0.11)
    hair = np.where(ears, hair * smooth(z, eye[2] + 0.03, eye[2] + 0.045), hair)
    # sideburns: a strip in front of each ear, from the hairline down to the middle of the ear
    burn = smooth(side, 0.058, 0.066) * (1 - smooth(side, 0.074, 0.08)) * smooth(back, 0.015, 0.03) * (1 - smooth(back, 0.07, 0.085))
    burn *= smooth(z, eye[2] - 0.035, eye[2] - 0.02)
    hair = np.maximum(hair, burn * (z < eye[2] + 0.04))
    # brows: an arch over each eye, a little in front of it, thinning toward the temple, with a clear gap over the
    # nose (two brows, never one bar across: under a hood or a cap the whole brow shows)
    brow = np.zeros(len(X))
    for e in (el, er):
        dy = X[:, 1] - e[1]
        arch = e[2] + 0.016 - 9.0 * dy ** 2
        thick = 0.0028 - 0.04 * np.maximum(0.0, np.abs(dy) - 0.008)
        brow = np.maximum(brow, (1 - smooth(np.abs(z - arch), thick, thick + 0.0035)) * (1 - smooth(np.abs(dy), 0.019, 0.026))
                          * smooth(x - e[0], -0.004, 0.006))
    brow *= smooth(side, 0.014, 0.02)               # (wide: the near mesh has few points over the nose to fade across)
    # beard shadow: below the cheekbones and in front of the ears, down under the jaw to the throat; not the lips
    cheek_line = z_sn - 0.002 + 0.35 * np.maximum(0.0, side - 0.02)      # (low on the cheek, as a shaven man's shadow sits)
    beard = (1 - smooth(z, cheek_line - 0.016, cheek_line + 0.004)) * smooth(back, -0.2, -0.0) * (1 - smooth(back, 0.03, 0.055))
    beard *= smooth(z, J["neck03____head"][2] - 0.01, J["neck03____head"][2] + 0.01)
    beard = np.where(side > 0.07, 0.0, beard)
    lips = (1 - smooth(np.abs(y), 0.021, 0.026)) * (1 - smooth(z - zm, 0.004, 0.008)) * smooth(z - zm, -0.012, -0.008) * (x > front)
    beard *= 1 - lips
    # the moustache: from just over the upper lip's edge up to the nose, out past the corners of the mouth and down a
    # little there (a full moustache: never a small square one under the nose)
    lowz = 0.0035 - 0.9 * np.maximum(0.0, side - 0.016)
    tache = ((1 - smooth(side, 0.031, 0.038)) * smooth(z - zm, lowz, lowz + 0.002) * (1 - smooth(z, z_sn - 0.003, z_sn))
             * (x > front - 0.6 * np.maximum(0.0, side - 0.015)) * (1 - lips))
    return {"hair": np.clip(hair, 0, 1), "brow": np.clip(brow, 0, 1), "beard": np.clip(beard, 0, 1), "tache": np.clip(tache, 0, 1)}


LIPS = {"clean": (0.0, 0.0), "stubble": (0.55, 0.55), "heavy": (0.85, 0.85), "moustache": (0.35, 1.0)}   # (shadow, moustache)


class FileMesh:
    """One of a soldier file's meshes (near, mid or far) in Blender's frame (y to his left), with what is what."""

    def __init__(self, data, lod, man):
        m = data[lod]
        self.lod = lod
        self.P = np.array(m["points"], float).reshape(-1, 3) * (1, -1, 1)
        self.N = np.array(m["facings"], float).reshape(-1, 3) * (1, -1, 1)
        self.paint = np.array(m["paints"])
        self.part = np.array(m["part"])
        self.T = np.array(m["triangles"]).reshape(-1, 3)
        names = [b["name"] for b in data["bones"]]
        self.head_bone = names.index("head")
        z = self.P[:, 2]
        self.skin = (self.paint == PAINT["skin"]) & (z > 1.3)
        r_eye = 0.0135
        self.eye_of = np.full(len(self.P), -1)
        for k, s in enumerate(("l", "r")):
            near_eye = (self.paint == PAINT["boots"]) & (np.linalg.norm(self.P - man.eye[s], axis=1) < r_eye + 0.004)
            self.eye_of[near_eye] = k
        self.strap = (self.paint == PAINT["straps"]) & (self.part == self.head_bone) & (z > 1.4)
        face = self.skin | (self.eye_of >= 0) | self.strap
        worn = ~face[self.T].all(axis=1) & (self.P[self.T].mean(axis=1)[:, 2] > 1.3)
        Tw = self.T[worn]
        self.worn_bvh = BVHTree.FromPolygons([tuple(p) for p in self.P], [tuple(int(i) for i in t) for t in Tw]) if len(Tw) else None


def _worn_distance(fm, X):
    """Distance from each point to the nearest thing the man wears near his head, and how far the point stands out
    from that thing's surface along its facing (below 0: inside it)."""
    d = np.full(len(X), 1.0)
    side = np.zeros(len(X))
    if fm.worn_bvh is None:
        return d, side
    for i, p in enumerate(X):
        loc, nrm, _, dist = fm.worn_bvh.find_nearest(Vector(p))
        if loc is not None:
            d[i] = dist
            side[i] = (np.array(p) - np.array(loc)) @ np.array(nrm)
    return d, side


MOST_MOVE = 0.013                 # (no point of a face moves much further than this from the delivered man's: the moves
                                  # ease off toward it, never stop dead at it, so no fold forms where they would)
BOLDER = 1.3                      # (each face's difference from the delivered man, drawn a little larger, as far as it stays
                                  # free of folds: Brian, 21 Nov, "make sure the faces are realistic and not deformed")
SMOOTH_PASSES = (3, 2)            # (passes of smoothing over the file's own skin: of the shape's move, then of the held move)


def skin_graph(T, idx):
    """For the points `idx` (a subset of a mesh with triangles T): each one's neighbours along the mesh's edges among
    the same points, as (rows, cols) into idx, for smoothing a field over the skin."""
    where = np.full(T.max() + 1, -1)
    where[idx] = np.arange(len(idx))
    e = np.vstack([T[:, [0, 1]], T[:, [1, 2]], T[:, [2, 0]]])
    e = where[e]
    e = e[(e >= 0).all(1)]
    e = np.unique(np.sort(e, axis=1), axis=0)
    return np.concatenate([e[:, 0], e[:, 1]]), np.concatenate([e[:, 1], e[:, 0]])


def smooth_field(F, graph, passes, lam=0.5, fixed=None):
    """Laplacian smoothing of a per-point field F (n x k) over the skin graph; `fixed` points keep their values."""
    rows, cols = graph
    n = len(F)
    deg = np.bincount(rows, minlength=n).astype(float)
    F = F.copy()
    for _ in range(passes):
        acc = np.zeros_like(F)
        np.add.at(acc, rows, F[cols])
        avg = np.where(deg[:, None] > 0, acc / np.maximum(deg, 1)[:, None], F)
        G = F + lam * (avg - F)
        if fixed is not None:
            G[fixed] = F[fixed]
        F = G
    return F


def vertex_normals(P, T):
    """Area-weighted point facings of a mesh."""
    fn = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
    vn = np.zeros_like(P)
    for i in range(3):
        np.add.at(vn, T[:, i], fn)
    return _n(vn)


def jaw_weight(man, X):
    """1 over the face and jaw, falling to 0 a little under the line of the jaw (chin to the jaw's angles): the neck
    and throat stay the delivered man's, so the collar fits every face."""
    J = man.J
    P = man.P
    eye_z = man.eye["l"][2]
    prof = P[(np.abs(P[:, 1]) < 0.008) & (P[:, 0] > man.eye["l"][0] - 0.005) & (P[:, 2] < eye_z - 0.05) & (P[:, 2] > J["neck03____head"][2])]
    C = prof[np.argmin(prof[:, 2])]
    hinge = J["jaw____head"]
    G = [np.array((hinge[0] - 0.005, s * 0.052, hinge[2] - 0.052)) for s in (1, -1)]
    n = _n(np.cross(G[0] - C, G[1] - C))
    if n[2] < 0:
        n = -n
    s = (X - C) @ n
    return smooth(s, -0.03, 0.006)                       # (a long ease: a short one creased the jaw's underside)


def face_data(data, army, base_seed, skin_hex=None, count=FACES, report=None):
    """The `faces` list for one soldier file (see the module's note), and a report of what was checked."""
    report = report if report is not None else {}
    looks = [face_look(army, s, base_seed) for s in range(count)]
    base = Man(looks[0][0])
    # (each face is laid onto the man by the skull, the brow, the eyes and the tops of the ears: what lies under or
    # beside the helmet's brim stays where his kit expects it, and the face below differs)
    cran = np.where(base.P[:, 2] > (base.eye["l"][2] - 0.03))[0]
    cran = cran[np.isin(cran, np.unique(base.tris))]
    S = hex_rgb(skin_hex or GAME_SKIN)
    east = army in ("JAP", "CHN")
    fms = {lod: FileMesh(data, lod, base) for lod in ("near", "mid", "far")}
    pre = {}
    for lod, fm in fms.items():
        idx = np.where(fm.skin)[0]
        X = fm.P[idx]
        tri, bary, dist = base.where(X)
        d_worn, side0 = _worn_distance(fm, X)
        w = smooth(d_worn, 0.004, 0.032) * (1 - smooth(dist, 0.006, 0.012)) * jaw_weight(base, X)
        # the ears stay the delivered man's: thin folds of few points, they buckle under any move
        e_mid = (base.eye["l"] + base.eye["r"]) / 2
        e_side, e_back = np.abs(X[:, 1] - e_mid[1]), e_mid[0] - 0.03 - X[:, 0]
        ear = (smooth(e_side, 0.054, 0.064) * smooth(e_back, 0.012, 0.035)
               * smooth(X[:, 2], e_mid[2] - 0.06, e_mid[2] - 0.045) * (1 - smooth(X[:, 2], e_mid[2] + 0.04, e_mid[2] + 0.055)))
        w = w * (1 - ear)
        if lod == "near":
            report["fit_mm"] = (round(float(np.median(dist)) * 1000, 2), round(float(dist.max()) * 1000, 2))
        marks = face_marks(base, base.at(tri, bary))
        sidx = np.where(fm.strap)[0]
        graph = skin_graph(fm.T, idx)
        for key, passes in (("beard", 2), ("tache", 1)):          # (soft edges: a few points apart, a hard edge reads as a smudge)
            marks[key] = smooth_field(marks[key][:, None], graph, passes)[:, 0]
        w = smooth_field(w[:, None], graph, 3, fixed=w < 0.01)[:, 0]       # (the hold eases over the mesh, too)
        skinT = fm.T[fm.skin[fm.T].all(1)]
        pre[lod] = dict(idx=idx, X=X, tri=tri, bary=bary, w=w, d_worn=d_worn, side0=side0, marks=marks, sidx=sidx,
                        graph=graph, skinT=skinT, N0=vertex_normals(fm.P, skinT))
    faces = []
    for slot, (look, spec) in enumerate(looks):
        man = base if slot == 0 else Man(look)
        if slot:
            R, t = kabsch(man.P[cran], base.P[cran], np.ones(len(cran)))
            VP = man.P @ R.T + t
            eye_move = {s: (R @ man.eye[s] + t) - base.eye[s] for s in ("l", "r")}
        tone = np.array(TONES.get(spec.get("tone", "medium"), (1, 1, 1)))
        hair = hex_rgb(HAIR[spec.get("hair", "black" if east else "brown")])
        shadow, tache = LIPS[spec.get("lip", "clean")]
        entry = {"name": f"face {slot}", "tone": [round(float(v), 3) for v in tone], "hair": HAIR[spec.get("hair", "black" if east else "brown")]}
        rep = {}
        for lod, fm in fms.items():
            q = pre[lod]
            out = {}
            if slot:
                # the shape's move at each point of the file's skin, smoothed over that skin (a point by the lips or
                # an eyelid can find the other lip or the eyeball nearest on MakeHuman's man: alone, its move would
                # fold the skin), then held near the kit, eased off toward the most move, and smoothed once more
                raw = base.at(q["tri"], q["bary"], VP) - base.at(q["tri"], q["bary"])
                raw = smooth_field(raw, q["graph"], SMOOTH_PASSES[0])
                cap = np.maximum(np.minimum(0.6 * q["d_worn"], MOST_MOVE), 1e-6)

                def ease(m):
                    n = np.linalg.norm(m, axis=1)
                    return m * (cap * np.tanh(n / cap) / np.maximum(n, 1e-9))[:, None]
                mv = ease(raw * (q["w"] * BOLDER)[:, None])
                mv = ease(smooth_field(mv, q["graph"], SMOOTH_PASSES[1], fixed=q["w"] < 0.01))
                # the facings turn as the file's own skin turns under the moves
                P1 = fm.P.copy()
                P1[q["idx"]] += mv
                N1 = vertex_normals(P1, q["skinT"])
                fac = _n(fm.N[q["idx"]] + (N1 - q["N0"])[q["idx"]])
                moved = {int(i): (mv[k], fac[k]) for k, i in enumerate(q["idx"]) if np.linalg.norm(mv[k]) > 2e-4}
                # the eyes go with their sockets
                for i in np.where(fm.eye_of >= 0)[0]:
                    moved[int(i)] = (eye_move["lr"[fm.eye_of[i]]], fm.N[i])
                # the chin strap follows the skin under it
                if len(q["sidx"]):
                    Xs = fm.P[q["sidx"]]
                    for k, i in enumerate(q["sidx"]):
                        dd = np.linalg.norm(q["X"] - Xs[k], axis=1)
                        j = int(np.argmin(dd))
                        f_ = 1 - smooth(dd[j], 0.006, 0.02)
                        if f_ > 0:
                            moved[int(i)] = (mv[j] * f_, fm.N[i])
                at = sorted(moved)
                out["at"] = at
                out["move"] = [round(float(v), 4) for i in at for v in moved[i][0] * (1, -1, 1)]
                out["facing"] = [round(float(v), 3) for i in at for v in moved[i][1] * (1, -1, 1)]
                # what the moves did: the largest, and any point of skin now on the wrong side of what he wears
                big = np.linalg.norm(mv, axis=1) > 2e-4
                dist1, side = _worn_distance(fm, (q["X"] + mv)[big])
                under = (side < -1e-4) & (dist1 < 0.005) & ~((q["side0"][big] < -1e-4) & (q["d_worn"][big] < 0.005))
                rep[lod] = {"moved": len(at), "most_mm": round(float(np.linalg.norm(mv, axis=1).max()) * 1000, 1),
                            "went_under": int(under.sum())}
            # colour: hair, brows, the beard's shadow and the moustache, as tints on the skin's colour times its tone
            mk = q["marks"]
            skin = S * tone
            a_hair = mk["hair"] * 0.92
            a_brow = mk["brow"] * 0.7
            light = float(hair @ (0.3, 0.59, 0.11))                         # (fair stubble hardly shows; dark shows grey-blue)
            a_beard = mk["beard"] * shadow * 0.3 * (1 - 0.7 * smooth(light, 0.15, 0.5))
            a_tache = mk["tache"] * tache * 0.85
            col = np.tile(skin, (len(q["idx"]), 1))
            for a, c in ((a_beard, hair * 0.45 + skin * 0.55 * 0.7), (a_tache, hair * 0.85 + skin * 0.15), (a_brow, hair * 0.9),
                         (a_hair, hair)):
                col = col * (1 - a[:, None]) + c[None, :] * a[:, None]
            tint = col / skin[None, :]
            keep = np.abs(tint - 1).max(axis=1) > 0.02
            out["tint_at"] = [int(i) for i in q["idx"][keep]]
            out["tint"] = [round(float(v), 3) for v in tint[keep].ravel()]
            entry[lod] = out
        if report is not None:
            report.setdefault("faces", []).append(rep)
        faces.append(entry)
    return faces


# ================================================================================================ seeing it
def paint_colours(army, data):
    """The colour of each paint number for the pictures: the file's own `colours` where it has them, else the army's
    studio colours (soldier.LOOK), as the game would paint him."""
    import soldier as S
    for mod in ("soldier_sov", "soldier_ita"):
        try:
            __import__(mod)
        except Exception:
            pass
    look = dict(S.LOOK.get(army, S.LOOK["GER"]))
    look.update(data.get("colours", {}))
    out = {}
    for name, k in PAINT.items():
        out[k] = hex_rgb(look.get(name, look.get("tunic", "777777")))
    for name, k in (data.get("new_paints") or {}).items():          # (paints a file adds: the rocket, ...)
        out[int(k)] = hex_rgb(look.get(name, "777777"))
    for name, k in EXTRA_PAINTS.items():
        if name in look:
            out[k] = hex_rgb(look[name])
    out[PAINT["boots"]] = hex_rgb(look.get("boots", "2A2420"))
    out["eye"] = hex_rgb(look.get("eye", "2A2420"))
    return out


def _srgb_to_lin(c):
    c = np.asarray(c, float)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def man_object(data, lod, colours, face=None, helmet=None, name="man", inks=None):
    """The file's mesh `lod` as a Blender object in its rest pose, coloured point by point as the game would colour it
    (paint, shade, the face's tone and tints, the helmet's inks), with the face's moves made and, if `helmet` (an
    entry of the file's `helmets`) is given, the man's own helmet swapped for it."""
    m = data[lod]
    P = np.array(m["points"], float).reshape(-1, 3) * (1, -1, 1)
    pa = np.array(m["paints"])
    T = np.array(m["triangles"]).reshape(-1, 3)
    col = np.array([colours.get(int(k), np.ones(3)) for k in pa])
    if "shade" in m:
        col *= np.array(m["shade"])[:, None]
    if face is not None and lod in face:
        f = face[lod]
        if f.get("at"):
            P[f["at"]] += np.array(f["move"]).reshape(-1, 3) * (1, -1, 1)
        skin = pa == PAINT["skin"]
        col[skin] *= np.array(face["tone"])
        if f.get("tint_at"):
            col[f["tint_at"]] *= np.array(f["tint"]).reshape(-1, 3)
    if helmet is not None:
        a, n = data["helmet_tris"][lod]
        keep = np.ones(len(T), bool)
        keep[a:a + n] = False
        T = T[keep]
        h = helmet[lod]
        HP = np.array(h["points"], float).reshape(-1, 3) * (1, -1, 1)
        hc = np.array([colours.get(int(k), np.ones(3)) for k in h["paints"]])
        if "ink" in h:
            for i, k in enumerate(h["ink"]):
                if k >= 0:
                    hc[i] = hex_rgb(inks[k])
        HT = np.array(h["triangles"]).reshape(-1, 3) + len(P)
        P = np.vstack([P, HP])
        col = np.vstack([col, hc])
        T = np.vstack([T, HT])
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(p) for p in P], [], [tuple(int(i) for i in t) for t in T])
    me.update()
    lin = _srgb_to_lin(np.clip(col, 0, 1))
    ca = me.color_attributes.new("col", "FLOAT_COLOR", "POINT")
    ca.data.foreach_set("color", np.hstack([lin, np.ones((len(lin), 1))]).astype(np.float32).ravel())
    for p in me.polygons:
        p.use_smooth = True
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    mat = bpy.data.materials.get("vertex_paint")
    if mat is None:
        mat = bpy.data.materials.new("vertex_paint")
        mat.use_nodes = True
        nt = mat.node_tree
        bsdf = nt.nodes["Principled BSDF"]
        att = nt.nodes.new("ShaderNodeAttribute")
        att.attribute_name = "col"
        nt.links.new(att.outputs["Color"], bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.7
    me.materials.append(mat)
    return ob


def floor(colour="CFC8BA", size=60.0):
    """A plain paper-coloured floor (made without bpy.ops, which can fall over in a scene built from data)."""
    import kit
    me = bpy.data.meshes.new("floor")
    s = size / 2
    me.from_pydata([(-s, -s, 0), (s, -s, 0), (s, s, 0), (-s, s, 0)], [], [(0, 1, 2, 3)])
    ob = bpy.data.objects.new("floor", me)
    bpy.context.scene.collection.objects.link(ob)
    me.materials.append(kit.material("floor", kit.hex_rgb(colour), rough=0.95))
    return ob


def label(text, at, yaw, size=0.022, colour="2A2824", card="EEEAE0"):
    """Text standing at `at` on a pale card, turned to face a camera at compass bearing `yaw` (kit.orbit's)."""
    import kit
    if len(text) > 16 and " " in text:                       # (long names on two lines)
        words, best = text.split(" "), None
        for k in range(1, len(text.split(" "))):
            a, b = " ".join(words[:k]), " ".join(words[k:])
            if best is None or max(len(a), len(b)) < max(len(best[0]), len(best[1])):
                best = (a, b)
        text = best[0] + "\n" + best[1]
    cu = bpy.data.curves.new("label", "FONT")
    cu.body = text
    cu.size = size
    cu.align_x = "CENTER"
    cu.align_y = "CENTER"
    ob = bpy.data.objects.new("label", cu)
    bpy.context.scene.collection.objects.link(ob)
    rot = (math.radians(90.0), 0.0, math.radians(yaw + 90.0))
    ob.location = at
    ob.rotation_euler = rot
    cu.materials.append(kit.material("label", kit.hex_rgb(colour), rough=0.9))
    bpy.context.view_layer.update()
    w, h = ob.dimensions[0] + size * 0.8, ob.dimensions[1] + size * 0.6
    me = bpy.data.meshes.new("card")
    me.from_pydata([(-w / 2, -h / 2, -0.002), (w / 2, -h / 2, -0.002), (w / 2, h / 2, -0.002), (-w / 2, h / 2, -0.002)], [],
                   [(0, 1, 2, 3)])
    cd = bpy.data.objects.new("card", me)
    bpy.context.scene.collection.objects.link(cd)
    cd.location = at
    cd.rotation_euler = rot
    me.materials.append(kit.material("card", kit.hex_rgb(card), rough=0.95))
    return ob


def picture_heads(data, army, faces, out, helmets=None, inks=None, w=1800, h=520, samples=32, yaw=-28.0, lod="near",
                  labels=None, slots=None):
    """The army's faces side by side, close (the near mesh in its rest pose), each under its own helmet if given,
    with a label under each if given."""
    import kit
    kit.fresh()
    colours = paint_colours(army, data)
    n = max(len(faces), slots or 0)                          # (the row's width: fewer men stand where the first would)
    gap = 0.30
    for k, f in enumerate(faces):
        hm = helmets[k % len(helmets)] if helmets else None
        ob = man_object(data, lod, colours, face=f, helmet=hm, name=f"man{k}", inks=inks)
        ob.location.y = (k - (n - 1) / 2) * gap * -1
        if labels:
            label(labels[k], (0.3, ob.location.y, 1.462), yaw, size=0.019)
    kit.studio(sun_elev=30.0, sun_turn=-40.0, strength=3.0, sky=0.6)
    floor()
    cam = kit.camera(lens=50.0)
    span = (n - 1) * gap + 0.4
    dist = span / (2 * math.tan(math.atan(36.0 / 2 / 50.0))) * 1.02
    kit.orbit(cam, (0.1, 0.0, 1.615), dist, 4.0, yaw)
    kit.render_setup(w, h, samples)
    return kit.still(out)


# ================================================================================================ the helmets
# A helmet's finish is built on the helmet the file already has (its shell as delivered, at each level of detail),
# so it sits exactly where the man's own did: painted camouflage is cut into the shell along the edges of its
# blotches, a cloth cover is the shell's outside eased out by the cloth's thickness, and nets, scrim, foliage, bands
# and decals are small pieces laid on it. The game paints a point by its paint number as ever, unless the point has
# an `ink` (an index into the file's `inks`): then that colour.
from mathutils import noise as _noise  # noqa: E402
from mathutils import geometry as _geom  # noqa: E402

HELMET_LIFT = 0.0006               # (a decal's height off the paint: enough not to flicker, too little to see)


class Shell:
    """The helmet (or cap) a file's man wears, at one level of detail, in Blender's frame (y to his left)."""

    def __init__(self, data, lod, paints=(PAINT["helmet"],)):
        m = data[lod]
        P = np.array(m["points"], float).reshape(-1, 3) * (1, -1, 1)
        N = np.array(m["facings"], float).reshape(-1, 3) * (1, -1, 1)
        pa = np.array(m["paints"])
        part = np.array(m["part"])
        T = np.array(m["triangles"]).reshape(-1, 3)
        names = [b["name"] for b in data["bones"]]
        hb = names.index("head")
        self.head_bone = hb
        sel = np.isin(pa[T], paints).all(1) & (part[T] == hb).all(1) & (P[T].mean(1)[:, 2] > 1.5)
        self.tri_index = np.where(sel)[0]                        # (which of the file's triangles they are)
        used = np.unique(T[sel])
        self.used = used                                         # (which of the file's points they are)
        remap = {int(o): k for k, o in enumerate(used)}
        self.P, self.N = P[used], N[used]
        self.T = np.array([[remap[int(i)] for i in t] for t in T[sel]])
        self.paint = pa[used]
        lo, hi = self.P.min(0), self.P.max(0)
        self.top = hi[2]
        self.rim = lo[2]
        self.C = np.array(((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2]))      # the middle of the brim's level
        self.R = float(max(hi[0] - lo[0], hi[1] - lo[1]) / 2)
        self.H = float(hi[2] - lo[2])
        inside = self.C + np.array((0.0, 0.0, 0.45 * self.H))
        cen = self.P[self.T].mean(1)
        fn = np.cross(self.P[self.T[:, 1]] - self.P[self.T[:, 0]], self.P[self.T[:, 2]] - self.P[self.T[:, 0]])
        self.outer = (np.einsum("ij,ij->i", _n(fn), _n(cen - inside)) > 0.15)
        self.bvh = BVHTree.FromPolygons([tuple(p) for p in self.P], [tuple(int(i) for i in t) for t in self.T[self.outer]])

    def surface(self, bearing_deg, up):
        """Where a line from inside the helmet, at `bearing_deg` (0 ahead, 90 to his left) and `up` (0 the brim's
        level, 1 the crown), meets the outside, and the outside's facing there."""
        f = math.radians(bearing_deg)
        z = self.rim + up * self.H
        o = Vector((self.C[0], self.C[1], self.rim + 0.25 * self.H))
        d = Vector((math.cos(f) * self.R, math.sin(f) * self.R, z - o.z)).normalized()
        hit, nrm, _, _ = self.bvh.ray_cast(o, d, 1.0)
        if hit is None:
            return None, None
        return np.array(hit), _n(np.array(nrm))


class Piece:
    """Triangles being gathered for a finish: points, facings, paint and ink at each corner."""

    def __init__(self):
        self.V, self.Nm, self.paint, self.ink, self.T = [], [], [], [], []

    def add(self, p, n, paint, ink=-1):
        self.V.append(np.asarray(p, float))
        self.Nm.append(_n(n))
        self.paint.append(paint)
        self.ink.append(ink)
        return len(self.V) - 1

    def tri(self, a, b, c):
        self.T.append((a, b, c))

    def quad(self, ps, n, paint, ink=-1, both=False):
        """A flat four-cornered piece facing n (both: a back face too, for a leaf or a strip seen from either side)."""
        k = [self.add(p, n, paint, ink) for p in ps]
        self.tri(k[0], k[1], k[2])
        self.tri(k[0], k[2], k[3])
        if both:
            k2 = [self.add(p, -np.asarray(n), paint, ink) for p in ps]
            self.tri(k2[0], k2[2], k2[1])
            self.tri(k2[0], k2[3], k2[2])

    def fan(self, P2, frame, ink, paint=PAINT["helmet"]):
        """A flat polygon (2D points in `frame`'s plane: origin, x axis, y axis, facing) as triangles."""
        O, X, Y, Nn = frame
        loops = [[Vector((float(x), float(y), 0.0)) for x, y in P2]]
        tris = _geom.tessellate_polygon(loops)
        base = len(self.V)
        for x, y in P2:
            self.add(O + X * x + Y * y, Nn, paint, ink)
        for t in tris:
            self.tri(base + t[0], base + t[2], base + t[1]) if False else self.tri(base + t[0], base + t[1], base + t[2])

    def merge(self, other):
        off = len(self.V)
        self.V += other.V
        self.Nm += other.Nm
        self.paint += other.paint
        self.ink += other.ink
        self.T += [(a + off, b + off, c + off) for a, b, c in other.T]


def shell_bmesh(shell):
    bm = bmesh.new()
    lay_n = bm.verts.layers.float_vector.new("nrm")      # (layers first: adding one later drops the points made)
    lay_p = bm.verts.layers.int.new("paint")
    lay_s = bm.verts.layers.int.new("src")
    lay_o = bm.faces.layers.int.new("outer")
    vs = [bm.verts.new(tuple(p)) for p in shell.P]
    for v, n, pa, i in zip(vs, shell.N, shell.paint, shell.used):
        v[lay_n] = tuple(n)
        v[lay_p] = int(pa)
        v[lay_s] = int(i)
    for k, t in enumerate(shell.T):
        try:
            f = bm.faces.new([vs[i] for i in t])
            f[lay_o] = int(shell.outer[k])
        except ValueError:
            pass
    bm.verts.ensure_lookup_table()
    return bm


def cut_by(bm, field, level=0.0):
    """Cut the helmet's outer faces along the line where field(point) == level (a blotch's edge), so that each face
    lies on one side of it; new points take the facing of the edge they cut. (Split faces keep their `outer` mark.)"""
    lay_n = bm.verts.layers.float_vector["nrm"]
    lay_o = bm.faces.layers.int["outer"]
    lay_p = bm.verts.layers.int["paint"]
    lay_s = bm.verts.layers.int["src"]
    # (only the helmet's own paint is cut: other paints on it, the medic's crosses, keep their faces whole)
    edges = {e for f in bm.faces if f[lay_o] and all(v[lay_p] == PAINT["helmet"] for v in f.verts) for e in f.edges}
    val = {}
    for e in edges:
        for v in e.verts:
            if v not in val:
                val[v] = field(np.array(v.co)) - level
    cuts = []
    for e in edges:
        a, b = e.verts
        fa, fb = val[a], val[b]
        if (fa < 0) != (fb < 0):
            t = fa / (fa - fb)
            if 0.03 < t < 0.97:
                cuts.append((e, a, b, t))
    new_verts = []
    for e, a, b, t in cuts:
        na, nb = Vector(a[lay_n]), Vector(b[lay_n])
        _, nv = bmesh.utils.edge_split(e, a, t)
        nv[lay_n] = tuple(na.lerp(nb, t).normalized())
        near = a if t < 0.5 else b                 # (its paint and source point from the nearer end of the edge)
        nv[lay_p], nv[lay_s] = near[lay_p], near[lay_s]
        new_verts.append(nv)
    if new_verts:
        bmesh.ops.connect_verts(bm, verts=new_verts, check_degenerate=False)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
    bm.faces.ensure_lookup_table()
    return len(new_verts)


def blotches(seed, scale, stretch=(1.0, 1.0, 1.0)):
    """A smooth random field over the helmet (about -1 to 1): its level lines are a painted blotch's edge."""
    off = Vector((seed * 7.31, seed * 3.17, seed * 5.53))

    def f(p):
        q = Vector((p[0] / scale / stretch[0], p[1] / scale / stretch[1], p[2] / scale / stretch[2])) + off
        return _noise.noise(q) + 0.35 * _noise.noise(q * 2.3 + Vector((1.7, 9.2, 4.4)))
    return f


def clip_half(poly, a, b, c):
    """The part of a 2D polygon where a*x + b*y <= c (Sutherland-Hodgman, one edge)."""
    out = []
    n = len(poly)
    for i in range(n):
        p, q = poly[i], poly[(i + 1) % n]
        fp, fq = a * p[0] + b * p[1] - c, a * q[0] + b * q[1] - c
        if fp <= 0:
            out.append(p)
        if (fp <= 0) != (fq <= 0):
            t = fp / (fp - fq)
            out.append((p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t))
    return out


def star(R, points=5, inner=0.382, turn=90.0):
    return [((R if k % 2 == 0 else R * inner) * math.cos(math.radians(turn + 180.0 * k / points)),
             (R if k % 2 == 0 else R * inner) * math.sin(math.radians(turn + 180.0 * k / points))) for k in range(2 * points)]


def disc(R, n=20):
    return [(R * math.cos(2 * math.pi * k / n), R * math.sin(2 * math.pi * k / n)) for k in range(n)]


def heater_shield(w, h):
    """A shield's outline: straight top, straight sides two fifths of the way down, then curving to a point."""
    pts = [(-w / 2, h / 2), (w / 2, h / 2), (w / 2, h / 2 - 0.42 * h)]
    for k in range(1, 8):
        t = k / 8
        pts.append((w / 2 * math.cos(t * math.pi / 2) ** 0.9, h / 2 - 0.42 * h - 0.58 * h * math.sin(t * math.pi / 2)))
    pts += [(0.0, -h / 2)]
    for k in range(7, 0, -1):
        t = k / 8
        pts.append((-w / 2 * math.cos(t * math.pi / 2) ** 0.9, h / 2 - 0.42 * h - 0.58 * h * math.sin(t * math.pi / 2)))
    pts.append((-w / 2, h / 2 - 0.42 * h))
    return pts


DECALS = {}


def decal_shapes(kind, size):
    """A decal as a list of (2D polygon, colour), drawn in its own plane: x to its right as seen, y up, metres."""
    if kind == "shield_tricolour":
        # the national shield of 1935-40: three diagonal bands, black, white, red, from the upper left
        w, h = size * 0.82, size
        sh = heater_shield(w, h)
        out = []
        k = 1.0 / math.sqrt(2)
        cuts = [-1e9, -0.17 * size, 0.17 * size, 1e9]
        for (c0, c1), col in zip(zip(cuts[:-1], cuts[1:]), ("1A1A1A", "E8E6E0", "B5261E")):
            poly = clip_half(sh, -k, k, -c0) if c0 > -1e8 else sh       # (bands run from the upper left down to the right)
            poly = clip_half(poly, k, -k, c1) if c1 < 1e8 else poly
            if len(poly) >= 3:
                out.append((poly, col))
        return out
    if kind == "red_star":
        return [(star(size / 2), "A8241C")]
    if kind == "red_star_edged":
        return [(star(size / 2 + 0.003), "1E1E1E"), (star(size / 2), "B02A1E")]
    if kind == "white_star":
        return [(star(size / 2), "E4E2DA")]
    if kind == "yellow_star":
        return [(star(size / 2, inner=0.4), "C9A43A")]
    if kind == "brass_star":
        return [(star(size / 2, inner=0.4), "A88A3E")]
    if kind == "kmt_sun":
        # the round cap badge: a white sun of twelve points on blue, in a white ring
        rays = [((0.47 if k % 2 == 0 else 0.33) * size * math.cos(math.pi * k / 12 + math.pi / 2),
                 (0.47 if k % 2 == 0 else 0.33) * size * math.sin(math.pi * k / 12 + math.pi / 2)) for k in range(24)]
        return [(disc(size / 2 + 0.002, 24), "E6E4DC"), (disc(size / 2, 24), "1F3A78"), (rays, "E6E4DC"),
                (disc(0.24 * size, 18), "1F3A78"), (disc(0.2 * size, 18), "E6E4DC")]
    if kind == "white_disc_red_cross":
        a = size * 0.13
        cross = [(-a, size * 0.33), (a, size * 0.33), (a, a), (size * 0.33, a), (size * 0.33, -a), (a, -a), (a, -size * 0.33),
                 (-a, -size * 0.33), (-a, -a), (-size * 0.33, -a), (-size * 0.33, a), (-a, a)]
        return [(disc(size / 2, 24), "E8E6E0"), (cross, "B5261E")]
    if kind == "ita_infantry":
        # a stencilled arm-of-service badge in black: two crossed rifles under a crown, the regiment's number in a ring
        r, L, t = size * 0.17, size * 0.5, size * 0.035
        out = []
        for s in (1, -1):
            a = math.radians(55 * s)
            dx, dy = math.cos(a) * L, math.sin(a) * L
            nx, ny = -math.sin(a) * t, math.cos(a) * t
            out.append(([(-dx - nx, -dy - ny), (dx - nx, dy - ny), (dx + nx, dy + ny), (-dx + nx, -dy + ny)], "1A1A1A"))
        ring = disc(r, 20)
        hole = disc(r * 0.7, 20)
        out.append((ring, "1A1A1A"))
        out.append((hole, "*"))                                    # (* = the helmet's own colour: the stencil's open middle)
        crown = [(-0.16 * size, 0.27 * size), (0.16 * size, 0.27 * size), (0.2 * size, 0.42 * size), (0.1 * size, 0.35 * size),
                 (0.0, 0.46 * size), (-0.1 * size, 0.35 * size), (-0.2 * size, 0.42 * size)]
        out.append((crown, "1A1A1A"))
        return out
    if kind == "ita_oval":
        # the stencil left showing in an oval of the old grey-green, masked when the helmet was painted sand
        inner = [(poly, "575C47" if col == "*" else col) for poly, col in decal_shapes("ita_infantry", size * 0.78)]
        return [(ellipse(size * 0.45, size * 0.52, 24), "575C47")] + inner
    if kind == "ita_bersaglieri":
        # the light infantry's stencil in black: a flaming grenade on a hunting horn, over two crossed carbines
        out = []
        L, t = size * 0.46, size * 0.03
        for s_ in (1, -1):
            a = math.radians(28 * s_)
            dx, dy = math.cos(a) * L, math.sin(a) * L
            nx, ny = -math.sin(a) * t, math.cos(a) * t
            out.append(([(-dx - nx, -dy - ny - 0.08 * size), (dx - nx, dy - ny - 0.08 * size),
                         (dx + nx, dy + ny - 0.08 * size), (-dx + nx, -dy + ny - 0.08 * size)], "1A1A1A"))
        out.append((disc(size * 0.22, 20), "1A1A1A"))                        # (the horn's coil)
        out.append((disc(size * 0.15, 20), "*"))
        out.append((disc(size * 0.1, 14), "1A1A1A"))                         # (the grenade, at the coil's middle)
        out.append(([(-0.07 * size, 0.08 * size), (0.0, 0.42 * size), (0.07 * size, 0.08 * size)], "1A1A1A"))
        out.append(([(-0.1 * size, 0.06 * size), (-0.13 * size, 0.3 * size), (-0.02 * size, 0.1 * size)], "1A1A1A"))
        out.append(([(0.1 * size, 0.06 * size), (0.02 * size, 0.1 * size), (0.13 * size, 0.3 * size)], "1A1A1A"))
        return out
    if kind == "white_bar_h":
        # a sergeant's bar: a white stripe across the back of the helmet, brushed on (so its ends a little rounded)
        w, h = size / 2, size * 0.16
        return [([(-w, -h), (-w + 0.3 * h, -h * 1.05), (w - 0.3 * h, -h * 1.05), (w, -h), (w * 1.01, 0.0), (w, h),
                  (w - 0.3 * h, h * 1.05), (-w + 0.3 * h, h * 1.05), (-w, h), (-w * 1.01, 0.0)], "E4E2DA")]
    if kind == "red_star_outline":
        # a star drawn as a red line round a star of the helmet's own colour (some units' way)
        return [(star(size / 2), "A8241C"), (star(size / 2 - 0.006, inner=0.33), "*")]
    raise KeyError(kind)


def ellipse(a, b, n=20):
    return [(a * math.cos(2 * math.pi * k / n), b * math.sin(2 * math.pi * k / n)) for k in range(n)]


def _subdivide2d(P2, tris, most=0.006):
    """Split a polygon's triangles until no side is longer than `most` (so a decal follows the helmet's curve)."""
    P2 = [tuple(p) for p in P2]
    out = []
    stack = list(tris)
    while stack:
        t = stack.pop()
        a, b, c = (np.array(P2[i]) for i in t)
        L = [np.linalg.norm(b - a), np.linalg.norm(c - b), np.linalg.norm(a - c)]
        if max(L) <= most:
            out.append(t)
            continue
        k = int(np.argmax(L))
        i, j, o = (t[0], t[1], t[2]) if k == 0 else (t[1], t[2], t[0]) if k == 1 else (t[2], t[0], t[1])
        m = tuple((np.array(P2[i]) + np.array(P2[j])) / 2)
        P2.append(m)
        mi = len(P2) - 1
        stack += [(i, mi, o), (mi, j, o)]
    return P2, out


def lay_decal(piece, surf_bvh, at, facing, kind, size, inks, base_ink=-1, turn=0.0, lift=HELMET_LIFT, most=0.006):
    """Lay decal `kind` on the helmet's outside, its middle at `at` (facing out there), upright as seen."""
    nrm = _n(facing)
    up0 = np.array((0.0, 0.0, 1.0))
    X = _n(np.cross(up0, nrm)) if abs(nrm[2]) < 0.95 else np.array((0.0, 1.0, 0.0))   # (to the right, seen from outside)
    Y = _n(np.cross(nrm, X))
    if turn:
        c, s_ = math.cos(math.radians(turn)), math.sin(math.radians(turn))
        X, Y = X * c + Y * s_, -X * s_ + Y * c
    shapes = decal_shapes(kind, size)
    for layer, (poly, col) in enumerate(shapes):
        ink = base_ink if col == "*" else inks(col)
        tris = _geom.tessellate_polygon([[Vector((float(x), float(y), 0.0)) for x, y in poly]])
        P2, tris = _subdivide2d(poly, [tuple(t) for t in tris], most)
        base = len(piece.V)
        for x, y in P2:
            p = at + X * x + Y * y
            hit, n2, _, _ = surf_bvh.ray_cast(Vector(p + nrm * 0.03), Vector(-nrm), 0.08)
            if hit is None:
                hit, n2 = Vector(p), Vector(nrm)
            n2 = _n(np.array(n2))
            piece.add(np.array(hit) + n2 * (lift + 0.0005 * layer), n2, PAINT["helmet"], ink)   # (a layer well clear of the one under it: their chords sag differently)
        for t in tris:
            # (the polygon was drawn counter-clockwise as seen from outside: keep it facing out)
            a, b, c = (piece.V[base + i] for i in t)
            if np.cross(b - a, c - a) @ nrm < 0:
                piece.tri(base + t[0], base + t[2], base + t[1])
            else:
                piece.tri(base + t[0], base + t[1], base + t[2])


# ------------------------------------------------------------------------------------------------ a finish, built
LODS = ("near", "mid", "far")


def mix_hex(a, b, t):
    c = hex_rgb(a) * (1 - t) + hex_rgb(b) * t
    return "".join("%02X" % int(round(v * 255)) for v in np.clip(c, 0, 1))


class Inks:
    """The file's `inks`: the colours its finishes paint with, grown as they ask for them."""

    def __init__(self, start=()):
        self.list = [h.upper().lstrip("#") for h in start]

    def __call__(self, h):
        h = h.upper().lstrip("#")
        if h not in self.list:
            self.list.append(h)
        return self.list.index(h)


def aim(shell, bearing, elev, lift=0.0):
    """Where a line from the middle of the helmet, at `bearing` (degrees: 0 ahead, 90 to his left, -90 to his right)
    and `elev` (degrees above level, 90 the crown), comes out of it: that point lifted `lift` along the facing there,
    and the facing. (None, None) where the line misses (past the rim)."""
    b, e = math.radians(bearing), math.radians(elev)
    o = Vector((float(shell.C[0]), float(shell.C[1]), float(shell.top) - 0.115))
    d = Vector((math.cos(e) * math.cos(b), math.cos(e) * math.sin(b), math.sin(e)))
    hit, nrm, _, _ = shell.bvh.ray_cast(o, d, 0.6)
    if hit is None:
        return None, None
    n = _n(np.array(nrm))
    if n @ np.array(d) < 0:
        n = -n
    return np.array(hit) + n * lift, n


def rim_distance(shell):
    """How far a point is from the helmet's edge (where its outside turns into its inside)."""
    from mathutils.kdtree import KDTree
    T = shell.T
    edge = sorted(set(T[shell.outer].ravel().tolist()) & set(T[~shell.outer].ravel().tolist()))
    if not edge:
        edge = [int(np.argmin(shell.P[:, 2]))]
    kd = KDTree(len(edge))
    for k, i in enumerate(edge):
        kd.insert(Vector(shell.P[i]), k)
    kd.balance()
    return lambda p: kd.find(Vector(p))[2]


def paint_field(layer, shell, seed, rimd):
    """A paint layer's field over the helmet: the layer's colour goes where it is above 0. (n, the facing, is None
    where a layer is cut in hard; only the soft layers read it.)"""
    k = layer["kind"]
    s = seed + layer.get("seed", 0)
    lv = layer.get("level", 0.0)

    def up(p):
        return (p[2] - shell.rim) / shell.H
    if k == "blotch":            # painted camouflage: blotches, bands or streaks
        f = blotches(s, layer.get("scale", 0.045), layer.get("stretch", (1.0, 1.0, 1.0)))
        return lambda p, n: f(p) - lv
    if k == "wash":              # whitewash brushed on front to back: streaky, and rubbed thin at the rim
        f = blotches(s, 0.02, (3.0, 0.7, 1.2))
        g = blotches(s + 5, 0.035)
        edge = layer.get("edge", 0.012)
        return lambda p, n: 0.3 * f(p) + 0.8 * g(p) - lv - 1.6 * max(0.0, 1.0 - rimd(p) / edge)
    if k == "wear":              # paint worn off along the rim, and chips on the crown and front
        c = blotches(s, 0.008)
        band = layer.get("band", 0.0035)
        crown = shell.top - 0.03
        return lambda p, n: max(1.0 - rimd(p) / band + 0.6 * c(p) - 0.2,
                                (c(p) - 0.62 - lv) if p[2] > crown else -1.0)
    if k in ("dust", "snow", "fade"):   # soft, on what faces the sky
        g = blotches(s, 0.03)
        return lambda p, n: (n[2] if n is not None else 0.6) + 0.35 * g(p) - lv
    if k == "mud":               # smeared or splashed on, thickest low down
        g = blotches(s, layer.get("scale", 0.03))
        hi = layer.get("high", 0.35)
        return lambda p, n: 0.9 * (hi - up(p)) + 0.7 * g(p) - lv
    if k == "all":
        return lambda p, n: 1.0
    raise KeyError(k)


class Out:
    """A finish's mesh at one level of detail, in Blender's frame: points with facing, paint, ink, and the bone
    weights and shade they take from the nearest point of the helmet as delivered (pieces ride on the head bone)."""

    def __init__(self):
        self.V, self.N, self.paint, self.ink, self.T, self.src = [], [], [], [], [], []


def shell_paint(shell, recipe, inks, seed, lod, out):
    """The helmet's own shell in the finish's paint: hard-edged layers cut into it (not at far, too coarse to cut),
    soft ones laid point by point; points are split where colours meet so each side keeps its own."""
    bm = shell_bmesh(shell)
    lay_p = bm.verts.layers.int["paint"]
    lay_s = bm.verts.layers.int["src"]
    lay_n = bm.verts.layers.float_vector["nrm"]
    lay_o = bm.faces.layers.int["outer"]
    rimd = rim_distance(shell)
    layers = [(L, paint_field(L, shell, seed, rimd)) for L in recipe.get("paint", []) if lod in L.get("lods", LODS)]
    hard_ok = lod != "far"
    cut = 0
    for L, f in layers:
        if L.get("hard", True) and hard_ok:
            cut += cut_by(bm, lambda p, f=f: f(p, None))
    # (a point made by a cut takes its paint and its source point from the nearer end of the edge it cut)
    from mathutils.kdtree import KDTree
    kd = KDTree(len(shell.P))
    for i, p in enumerate(shell.P):
        kd.insert(Vector(p), i)
    kd.balance()
    bm.verts.index_update()
    for v in bm.verts:
        if v.index >= len(shell.P) and v[lay_s] < 0:     # (cut_by has set its own; any other new point, the nearest)
            _, j, _ = kd.find(v.co)
            v[lay_p] = int(shell.paint[j])
            v[lay_s] = int(shell.used[j])
    bm.verts.index_update()
    base = recipe.get("base")
    base_ink = inks(base) if base else -1
    inside = recipe.get("inside", "same")
    in_ink = base_ink if inside == "same" else (inks(inside) if inside else -1)
    keyed = {}
    for f in bm.faces:
        helm = all(v[lay_p] == PAINT["helmet"] for v in f.verts)
        cen = np.array(f.calc_center_median())
        fno = np.array(f.normal)
        corners = []
        for v in f.verts:
            if not helm:
                ink = -1
            elif not f[lay_o]:
                ink = in_ink
            else:
                ink = base_ink
                for L, fn in layers:
                    if not L.get("under", True) and fno[2] < -0.45:
                        continue
                    hard = L.get("hard", True) and hard_ok
                    val = fn(cen, fno) if hard else fn(np.array(v.co), np.array(v[lay_n]))
                    if val > 0:
                        ink = inks(L["hex"])
            key = (v.index, ink)
            if key not in keyed:
                keyed[key] = len(out.V)
                out.V.append(np.array(v.co))
                out.N.append(_n(np.array(v[lay_n])))
                out.paint.append(int(v[lay_p]))
                out.ink.append(ink)
                out.src.append(int(v[lay_s]))
            corners.append(keyed[key])
        out.T.append(tuple(corners))
    bm.free()
    return base_ink, cut


def add_piece(out, pc):
    off = len(out.V)
    out.V += [np.asarray(v, float) for v in pc.V]
    out.N += [_n(n) for n in pc.Nm]
    out.paint += [int(p) for p in pc.paint]
    out.ink += list(pc.ink)
    out.src += [-1] * len(pc.V)
    out.T += [(a + off, b + off, c + off) for a, b, c in pc.T]


def ribbon(pc, pts, width, ink, both=False, widths=None):
    """A flat strip along a line of (point, facing) pairs, `width` across (or widths[k] at each point)."""
    pts = [(p, n) for p, n in pts if p is not None]
    if len(pts) < 2:
        return
    L, R = [], []
    for k, (p, n) in enumerate(pts):
        a = pts[max(k - 1, 0)][0]
        b = pts[min(k + 1, len(pts) - 1)][0]
        t = _n(b - a)
        sd = _n(np.cross(t, n))
        w = (widths[k] if widths is not None else width) / 2
        L.append(pc.add(p - sd * w, n, PAINT["helmet"], ink))
        R.append(pc.add(p + sd * w, n, PAINT["helmet"], ink))
    for k in range(len(pts) - 1):
        pc.tri(L[k], R[k], R[k + 1])
        pc.tri(L[k], R[k + 1], L[k + 1])
    if both:
        L2 = [pc.add(pc.V[i], -pc.Nm[i], PAINT["helmet"], ink) for i in L]
        R2 = [pc.add(pc.V[i], -pc.Nm[i], PAINT["helmet"], ink) for i in R]
        for k in range(len(pts) - 1):
            pc.tri(L2[k], R2[k + 1], R2[k])
            pc.tri(L2[k], L2[k + 1], R2[k + 1])


def net_piece(pc, shell, x, inks, seed, lod):
    """A cord net (or wire basket) over the helmet: rings round it and lines from the rim up, the lines thinned out
    toward the crown as a net gathers there."""
    if lod == "far":
        return
    mid = lod == "mid"
    sp = x.get("mesh", 0.035) * (2.0 if mid else 1.0)
    w = x.get("cord", 0.0025) * (1.8 if mid else 1.0)
    lift = x.get("lift", 0.004) + (0.0015 if mid else 0.0)
    ink = inks(x["hex"])
    e0 = x.get("from", -6.0)
    Rd = shell.R * 0.85
    de = math.degrees(sp / Rd)
    nb = max(6, int(round(2 * math.pi * shell.R * 0.95 / sp)))
    rings = []
    e = e0
    while e < 90 - 0.8 * de:
        rings.append(e)
        e += de
    top_ring = rings[-1]
    turn = x.get("turn", 7.0)
    every = x.get("crown_every", 3)
    for i in range(nb):
        b = 360.0 * i / nb + turn
        e_end = 90.0 if i % every == 0 else top_ring
        es = list(np.arange(e0, e_end, de)) + [e_end]
        ribbon(pc, [aim(shell, b, e_, lift) for e_ in es], w, ink)
    for e_ in rings:
        k = nb if e_ < 60 else max(6, nb // 2)
        ribbon(pc, [aim(shell, 360.0 * j / k + turn + 180.0 / nb, e_, lift) for j in range(k + 1)], w, ink)


def band_piece(pc, shell, x, inks, seed, lod):
    """A band round the dome (a strip of inner tube, a strap, cord): a strip standing a little off the paint."""
    n = {"near": 36, "mid": 14, "far": 8}[lod]
    e = x.get("at", 20.0)
    half = math.degrees(x.get("wide", 0.022) / 2 / (shell.R * 0.85))
    lift = x.get("thick", 0.003)
    ink = inks(x["hex"])
    lo = [aim(shell, 360.0 * j / n, e - half, lift) for j in range(n + 1)]
    hi = [aim(shell, 360.0 * j / n, e + half, lift) for j in range(n + 1)]
    for j in range(n):
        if any(q[0] is None for q in (lo[j], lo[j + 1], hi[j], hi[j + 1])):
            continue
        nn = _n(lo[j][1] + hi[j + 1][1])
        a = pc.add(lo[j][0], lo[j][1], PAINT["helmet"], ink)
        b = pc.add(lo[j + 1][0], lo[j + 1][1], PAINT["helmet"], ink)
        c = pc.add(hi[j + 1][0], hi[j + 1][1], PAINT["helmet"], ink)
        d = pc.add(hi[j][0], hi[j][1], PAINT["helmet"], ink)
        if np.cross(pc.V[b] - pc.V[a], pc.V[c] - pc.V[a]) @ nn < 0:
            pc.tri(a, c, b)
            pc.tri(a, d, c)
        else:
            pc.tri(a, b, c)
            pc.tri(a, c, d)


def _leaf(pc, base, l, ln, ll, lw, ink):
    """One leaf: a pointed blade from `base` along `l`, flat across ln (seen from both sides)."""
    w = _n(np.cross(l, ln))
    ps = [base, base + l * ll * 0.45 + w * lw / 2, base + l * ll, base + l * ll * 0.45 - w * lw / 2]
    pc.quad(ps, ln, PAINT["helmet"], ink, both=True)


def foliage_piece(pc, shell, x, inks, seed, lod):
    """Foliage pushed under a band or into a net: leafy sprigs or grass, standing up and back from the helmet. Near,
    each sprig is a stem with its leaves (or a tuft of blades); mid, one card; far, a crossed pair for the biggest."""
    rng = np.random.default_rng(seed + 101)
    n = x.get("count", 6)
    spots = x.get("bearings", [35, -35, 75, -75, 110, -110, 150, -150, 0, 180])
    cols = x["hex"] if isinstance(x["hex"], list) else [x["hex"]]
    kind = x.get("plant", "leaf")
    size = x.get("size", 0.12)
    e_lo, e_hi = x.get("at", (15.0, 25.0)) if isinstance(x.get("at"), (list, tuple)) else (x.get("at", 20.0), x.get("at", 20.0) + 6)
    far_n = x.get("far", 3)
    zup = np.array((0.0, 0.0, 1.0))
    back = np.array((-1.0, 0.0, 0.0))
    for j in range(n):
        pick = np.random.default_rng(seed + 1000 + 31 * j)      # (each sprig's place by its own draw: the same at every level)
        b = spots[j % len(spots)] + pick.normal(0, 9)
        e = pick.uniform(e_lo, e_hi)
        L = size * pick.uniform(0.7, 1.2)
        col = cols[int(pick.integers(len(cols)))]
        wob = pick.normal(0, 0.2, 3)
        at, nrm = aim(shell, b, e, 0.003)
        if at is None:
            continue
        ink = inks(col)
        d = _n(nrm * x.get("out", 0.9) + zup * x.get("rise", 0.45) + back * x.get("back", 0.45) + wob)
        side = _n(np.cross(d, nrm))
        if lod == "near":
            if kind == "grass":
                for k in range(x.get("blades", 6)):
                    bb = at + side * rng.uniform(-0.012, 0.012)
                    dd = _n(d + rng.normal(0, 0.22, 3))
                    ll = L * rng.uniform(0.6, 1.1)
                    sd = _n(np.cross(dd, nrm))
                    tip = bb + dd * ll + zup * -0.15 * ll * rng.uniform(0, 1)
                    lnn = _n(np.cross(dd, sd))
                    a1 = pc.add(bb - sd * 0.003, lnn, PAINT["helmet"], ink)
                    a2 = pc.add(bb + sd * 0.003, lnn, PAINT["helmet"], ink)
                    a3 = pc.add(tip, lnn, PAINT["helmet"], ink)
                    pc.tri(a1, a2, a3)
                    c1 = pc.add(bb - sd * 0.003, -lnn, PAINT["helmet"], ink)
                    c2 = pc.add(bb + sd * 0.003, -lnn, PAINT["helmet"], ink)
                    c3 = pc.add(tip, -lnn, PAINT["helmet"], ink)
                    pc.tri(c1, c3, c2)
            else:
                stem_ink = inks(x.get("stem", "5A4A32"))
                tip = at + d * L
                pc.quad([at - side * 0.0012, at + side * 0.0012, tip + side * 0.0008, tip - side * 0.0008],
                        _n(np.cross(d, side)), PAINT["helmet"], stem_ink, both=True)
                nl = x.get("leaves", 7)
                for k, t in enumerate(np.linspace(0.15, 1.0, nl)):
                    s_ = 1 if k % 2 == 0 else -1
                    l = d if k == nl - 1 else _n(d * 0.7 + side * s_ * 0.75 + rng.normal(0, 0.15, 3))
                    ln = _n(np.cross(l, side) + rng.normal(0, 0.35, 3))
                    ll = L * x.get("leaf", 0.4) * rng.uniform(0.75, 1.25)
                    _leaf(pc, at + d * L * t, l, ln, ll, ll * 0.45, inks(cols[int(rng.integers(len(cols)))]))
        elif lod == "mid" or j < far_n:
            wide = 0.55 if kind != "grass" else 0.4
            cn = _n(np.cross(d, side))
            ps = [at, at + d * L * 0.45 + side * wide * L / 2, at + d * L, at + d * L * 0.45 - side * wide * L / 2]
            pc.quad(ps, cn, PAINT["helmet"], ink, both=True)
            if lod == "far":
                ps = [at, at + d * L * 0.45 + cn * wide * L / 2, at + d * L, at + d * L * 0.45 - cn * wide * L / 2]
                pc.quad(ps, side, PAINT["helmet"], ink, both=True)


def scrim_piece(pc, shell, x, inks, seed, lod):
    """Strips of hessian or cloth through the net, lying down the helmet and hanging past its rim."""
    if lod == "far":
        return
    rng = np.random.default_rng(seed + 202)
    cols = x["hex"] if isinstance(x["hex"], list) else [x["hex"]]
    for j in range(x.get("count", 6)):
        b = 360.0 * j / x.get("count", 6) + rng.normal(0, 18) + 20
        e_top = rng.uniform(35, 65)
        length = rng.uniform(0.08, 0.13)
        width = x.get("wide", 0.02) * rng.uniform(0.8, 1.2)
        ink = inks(cols[int(rng.integers(len(cols)))])
        segs = 4 if lod == "near" else 2
        drift = rng.uniform(-4, 4)
        Rd = shell.R * 0.85
        pts = []
        prev = None
        for k in range(segs + 1):
            e_k = e_top - math.degrees(length * k / segs / Rd)
            q = aim(shell, b + drift * k, e_k, 0.006 + 0.001 * k)
            if q[0] is None or e_k < -12:
                if prev is None:
                    break
                p0, n0 = prev
                q = (p0 + np.array((0.0, 0.0, -length / segs)) + n0 * 0.004, n0)
            pts.append(q)
            prev = q
        ribbon(pc, pts, width, ink, both=True)


def plume_piece(pc, shell, x, inks, seed, lod):
    """A bunch of long black feathers fixed at the side and sweeping back and down past the rim."""
    rng = np.random.default_rng(seed + 303)
    nf = {"near": x.get("count", 14), "mid": 5, "far": 2}[lod]
    segs = {"near": 4, "mid": 2, "far": 1}[lod]
    at, n0 = aim(shell, x.get("bearing", -90.0), x.get("at", 18.0), 0.004)
    if at is None:
        return
    zup = np.array((0.0, 0.0, 1.0))
    cols = x.get("hex", ["16181A", "16181A", "1E2A24"])
    for j in range(nf):
        st = at + rng.normal(0, 0.005, 3) * (1, 1, 0.6)
        d = _n(np.array((-1.0, 0.0, 0.0)) * 0.8 + n0 * 0.28 + zup * 0.38 + rng.normal(0, 0.12, 3))
        L = rng.uniform(0.17, 0.24) * (1.15 if lod != "near" else 1.0)
        ink = inks(cols[int(rng.integers(len(cols)))])
        pts, wid = [], []
        p = st
        prof = [0.004, 0.015, 0.017, 0.011, 0.003]
        spin = rng.uniform(-0.9, 0.9)
        for k in range(segs + 1):
            t = k / segs
            nn = _n(n0 * math.cos(spin) + np.cross(d, n0) * math.sin(spin))
            pts.append((p.copy(), nn))
            wid.append(np.interp(t, np.linspace(0, 1, len(prof)), prof) * (1.6 if lod == "far" else 1.0))
            d = _n(d - zup * 0.35 / segs * 1.6)
            p = p + d * L / segs
        ribbon(pc, pts, 0.01, ink, both=True, widths=wid)


def decal_piece(pc, shell, x, inks, seed, lod, base_ink):
    if lod == "far" and x["size"] < x.get("far_min", 0.09):
        return
    at, n = aim(shell, x["bearing"], x["at"], 0.0)
    if at is None:
        return
    most = {"near": 0.014, "mid": 0.03, "far": 0.05}[lod] if x["size"] >= 0.04 else 1.0   # (a small badge lies flat)
    lift = {"near": HELMET_LIFT, "mid": 0.0012, "far": 0.002}[lod]
    lay_decal(pc, shell.bvh, at, n, x["decal"], x["size"], inks, base_ink=base_ink, turn=x.get("turn", 0.0), lift=lift,
              most=most)


PIECES = {"net": net_piece, "band": band_piece, "foliage": foliage_piece, "scrim": scrim_piece, "plume": plume_piece}


def finish_lod(data, recipe, inks, lod, paints=(PAINT["helmet"],)):
    """One finish at one level of detail: the mesh as the file's meshes are written (json frame, y to his right)."""
    shell = Shell(data, lod, paints)
    seed = sum(ord(c) for c in recipe["name"])
    out = Out()
    paint = list(recipe.get("paint", []))
    if lod == "far":
        # (far off, a net or wire is only a shade over the paint)
        for x in recipe.get("extras", []):
            if x["kind"] == "net" and x.get("far_tint", True):
                paint.append({"kind": "all", "hex": mix_hex(recipe.get("base") or x.get("under", "777777"), x["hex"],
                                                            x.get("far_mix", 0.3)), "hard": False})
    base_ink, cut = shell_paint(shell, dict(recipe, paint=paint), inks, seed, lod, out)
    for xi, x in enumerate(recipe.get("extras", [])):
        pc = Piece()
        if x["kind"] == "decal":
            decal_piece(pc, shell, x, inks, seed, lod, base_ink)
        else:
            PIECES[x["kind"]](pc, shell, x, inks, seed + 7 * xi, lod)
        add_piece(out, pc)
    # (bone weights and shade: a shell point takes its source point's, a piece's point the head bone's)
    m = data[lod]
    hb = shell.head_bone
    part = [int(m["part"][i]) if i >= 0 else hb for i in out.src]
    part2 = [int(m["part2"][i]) if i >= 0 else hb for i in out.src]
    w2 = [float(m["w2"][i]) if i >= 0 else 0.0 for i in out.src]
    V = np.array(out.V) * (1, -1, 1)
    N = np.array(out.N) * (1, -1, 1)
    T = out.T                                     # (flipping y back gives the file's own winding: clockwise seen from outside)
    res = {"points": [round(float(v), 4) for v in V.ravel()], "facings": [round(float(v), 3) for v in N.ravel()],
           "paints": out.paint, "triangles": [int(i) for t in T for i in t], "part": part, "part2": part2,
           "w2": [round(w, 3) for w in w2], "ink": out.ink}
    if "shade" in m:
        res["shade"] = [round(float(m["shade"][i]), 3) if i >= 0 else 1.0 for i in out.src]
    return res, shell


def helmet_entry(data, recipe, inks, paints=(PAINT["helmet"],)):
    """A finish as the file carries it: its name and notes, when and where it was worn, how common, and its mesh at
    each level of detail."""
    e = {"name": recipe["name"], "about": recipe.get("about", ""), "from": recipe.get("from", "1939-09"),
         "to": recipe.get("to", "1945-08"), "season": recipe.get("season", "any"), "front": recipe.get("front", "any"),
         "share": recipe.get("share", 0.1)}
    for lod in LODS:
        e[lod], _ = finish_lod(data, recipe, inks, lod, paints)
    return e


# ================================================================================================ the forehead
def tuck_under(data, paints=(PAINT["helmet"],), gap=0.0025, most_bearing=75.0):
    """Skin that shows through the front of the cap or helmet (the forehead, in two of the files as delivered) pushed
    back under it, along the line from the middle of the head, to `gap` inside. Only points ahead of the ears move.
    Returns how many points moved at each level of detail."""
    moved = {}
    for lod in LODS:
        s = Shell(data, lod, paints)
        m = data[lod]
        P = np.array(m["points"], float).reshape(-1, 3) * (1, -1, 1)
        pa = np.array(m["paints"])
        o = Vector((float(s.C[0]), float(s.C[1]), float(s.top) - 0.115))
        n = 0
        for i in np.where((pa == PAINT["skin"]) & (P[:, 2] > s.rim))[0]:
            dv = Vector(P[i]) - o
            if abs(math.degrees(math.atan2(dv.y, dv.x))) > most_bearing:
                continue
            hit, _, _, dist = s.bvh.ray_cast(o, dv.normalized(), dv.length)
            if hit is None:
                continue
            q = o + dv.normalized() * (dist - gap)
            for k, v in enumerate((q.x, -q.y, q.z)):
                m["points"][3 * i + k] = round(float(v), 4)
            n += 1
        moved[lod] = n
    return moved


# ================================================================================================ writing the files
FILES = {"GER": "soldier_GER.json", "POL": "soldier_POL.json", "SOV": "soldier_SOV.json", "SOVW": "soldier_SOV_winter.json",
         "SOVS": "soldier_SOV_snow.json", "ITA": "soldier_ITA.json", "ITAD": "soldier_ITA_desert.json",
         "JAP": "soldier_JAP.json", "CHN": "soldier_CHN.json", "USA": "soldier_USA.json", "USAB": "soldier_USA_bar.json",
         "USAZ": "soldier_USA_bazooka.json", "USAL": "soldier_USA_loader.json", "USAN": "soldier_USA_nco.json",
         "USAM": "soldier_USA_medic.json", "USAR": "soldier_USA_radio.json", "USAP": "soldier_USA_para.json"}
SEEDS = {"GER": 3, "POL": 7, "SOV": 19, "SOVW": 19, "SOVS": 19, "ITA": 23, "ITAD": 23, "JAP": 11, "CHN": 13, "USA": 29,
         "USAB": 29, "USAZ": 29, "USAL": 29, "USAN": 29, "USAM": 29, "USAR": 29, "USAP": 29}
ADDED = ("faces", "helmets", "inks", "helmet_tris", "about_variety")

ABOUT_VARIETY = (
    "Variety (Tools/battle_units/variety.py and helmet_finishes.py): one man made into many. `faces` (8): face 0 is the "
    "man as he is; for face k, add `move` (metres, this file's axes) to the points listed in `at`, at each level of "
    "detail, then colour: every skin-painted point's colour times `tone`, and each point in `tint_at` times its `tint` "
    "(the hair, brows, stubble or moustache), all on top of the paint (and `shade`) as usual. `helmets`: his helmet (or "
    "cap, ushanka or hood) in the finishes it was worn in. Each is a rigid mesh at near/mid/far in the same form as the "
    "file's own meshes (points, facings, paints, triangles, part, part2, w2, and shade where the file has it), with "
    "`ink` at each point: -1 paints the point by its paint number as ever; any other value is an index into `inks` "
    "(colours, RRGGBB), the colour to paint it instead. To draw a man in a finish, leave out his own helmet's "
    "triangles, `helmet_tris[lod]` = [first, count] (they are the last of the list), and draw the finish's mesh "
    "skinned on the same bones. Each finish has `from` and `to` (YYYY-MM), `season` (any, summer, winter), `front` "
    "(any, east, west, south, desert, pacific, china) and `share` (how common it was). Picking, for each man: the "
    "finishes whose months hold the battle's date and whose season and front fit (any fits all), one chosen by share, "
    "and face = a hash of (block id, k) mod 8 and the helmet from another hash of the same, so a man keeps both all "
    "battle. When no finish fits the date, draw the first finish (or his own helmet). No swastika, rising-sun flag or "
    "fascist badge is drawn: the German eagle decal is left off (its side stays plain paint).")


def write_variety(army, path=None, report=None, do_faces=True, do_helmets=True):
    """Add (or renew) the faces and helmets in one army's soldier file. Everything else in it stays as it was, but
    for the forehead pushed back under the cap where it showed through, and the order of the triangles (the plain
    helmet's last)."""
    import helmet_finishes as HF
    path = path or SOLDIERS + FILES[army]
    data = json.load(open(path))
    for k in ADDED:
        data.pop(k, None)
    rep = report if report is not None else {}
    paints = tuple(data.get("helmet_paints") or HF.PAINTS.get(army, (PAINT["helmet"],)))   # (the medic's crosses: 15, 16)
    rep["tucked"] = tuck_under(data, paints)
    if do_faces:
        frep = {}
        skin = data.get("colours", {}).get("skin")
        data["faces"] = face_data(data, FILE_ARMY.get(army, army), SEEDS[army], skin_hex=skin, report=frep)
        rep["faces"] = frep
    if do_helmets:
        inks = Inks()
        data["helmets"] = [helmet_entry(data, r, inks, paints) for r in HF.RECIPES[army]]
        data["inks"] = inks.list
        tris = {}
        for lod in LODS:
            s = Shell(data, lod, paints)
            T = np.array(data[lod]["triangles"]).reshape(-1, 3)
            keep = np.ones(len(T), bool)
            keep[s.tri_index] = False
            data[lod]["triangles"] = [int(i) for i in np.vstack([T[keep], T[~keep]]).ravel()]
            tris[lod] = [int(keep.sum()), int((~keep).sum())]
        data["helmet_tris"] = tris
        rep["helmets"] = {h["name"]: {l: len(h[l]["triangles"]) // 3 for l in LODS} for h in data["helmets"]}
        rep["most"] = {l: max(len(data[l]["triangles"]) // 3 - tris[l][1] + len(h[l]["triangles"]) // 3
                              for h in data["helmets"]) for l in LODS}
    data["about_variety"] = ABOUT_VARIETY
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(data, f, separators=(",", ":"))
    json.load(open(tmp))                        # (it reads back)
    os.replace(tmp, path)
    rep["bytes"] = os.path.getsize(path)
    return data, rep


# ================================================================================================ the picture sheets
def _stack(paths, out, quality=88):
    """Pictures one under another in one file (jpg)."""
    ims = [bpy.data.images.load(p) for p in paths]
    w = max(im.size[0] for im in ims)
    rows = []
    for im in ims:
        a = np.array(im.pixels[:], dtype=np.float32).reshape(im.size[1], im.size[0], 4)
        if im.size[0] < w:
            pad = np.ones((im.size[1], w - im.size[0], 4), np.float32)
            a = np.concatenate([a, pad], 1)
        rows.append(a)
    full = np.concatenate(rows[::-1], 0)                # (Blender's rows run bottom to top)
    img = bpy.data.images.new("sheet", w, full.shape[0], alpha=False)
    img.pixels.foreach_set(full.ravel())
    scn = bpy.context.scene
    scn.view_settings.view_transform = "Standard"          # (the rows are already finished pictures: no second grade)
    scn.view_settings.look = "None"
    scn.view_settings.exposure = 0.0
    scn.view_settings.gamma = 1.0
    scn.render.image_settings.file_format = "JPEG"
    scn.render.image_settings.color_mode = "RGB"
    scn.render.image_settings.quality = quality
    img.save_render(out, scene=scn)
    return out


def picture_sheet(army, data, out, tmp="/tmp/variety_rows/", samples=20, per_row=4):
    """One army's sheet: its eight faces (in its commonest helmet), then every helmet finish (each on another face),
    each labelled."""
    os.makedirs(tmp, exist_ok=True)
    F, H, inks = data["faces"], data["helmets"], data["inks"]
    rows = []
    for r in range(2):
        p = tmp + f"{army}_faces_{r}.png"
        picture_heads(data, army, F[4 * r:4 * r + 4], p, helmets=[H[0]], inks=inks, samples=samples, w=1600, h=600,
                      yaw=-30, labels=["face %d" % k for k in range(4 * r, 4 * r + 4)])
        rows.append(p)
    for r in range(0, len(H), per_row):
        hs = H[r:r + per_row]
        p = tmp + f"{army}_helmets_{r // per_row}.png"
        picture_heads(data, army, [F[(r + k) % len(F)] for k in range(len(hs))], p, helmets=hs, inks=inks,
                      samples=samples, w=1600, h=600, yaw=-30, labels=[h["name"] for h in hs], slots=per_row)
        rows.append(p)
    return _stack(rows, out)


def main(argv):
    import argparse
    ap = argparse.ArgumentParser(description="Faces and helmet finishes for the battle soldiers' files.")
    ap.add_argument("--armies", default=",".join(FILES), help="army codes, e.g. GER,SOV (default: every file there is)")
    ap.add_argument("--only", default="faces,helmets,pictures", help="any of faces, helmets, pictures")
    ap.add_argument("--dir", default=SOLDIERS, help="the folder holding the soldier_*.json files")
    a = ap.parse_args(argv)
    only = set(a.only.split(","))
    for army in a.armies.split(","):
        t = time.time()
        path = os.path.join(a.dir, FILES[army])
        if not os.path.exists(path):
            print(army, "no file", path, "(skipped)", flush=True)
            continue
        if only & {"faces", "helmets"}:
            data, rep = write_variety(army, path, do_faces="faces" in only, do_helmets="helmets" in only)
            print(army, "written in %.0f s:" % (time.time() - t), "tucked", rep["tucked"], "most triangles", rep.get("most"),
                  "bytes", rep["bytes"], flush=True)
        else:
            data = json.load(open(path))
        if "pictures" in only and "faces" in data and "helmets" in data:
            out = os.path.join(a.dir, "pictures", "variety", "sheet_%s.jpg" % army)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            picture_sheet(army, data, out)
            print(army, "sheet", out, flush=True)


if __name__ == "__main__":
    main(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:])
