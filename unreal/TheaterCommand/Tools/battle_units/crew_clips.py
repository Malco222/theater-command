"""Release 2 crew clips for the tripod machine gun and the mortar, keyed straight onto each infantry soldier file's own
skeleton (field_clips.py's way: the file's own Man, its IK, bake and stride; nothing else in the file touched). The
names are the contract with the ground thread, which makes the props (each nation's tripod MG and mortar tube):

    hmg_fire_prone    (loop) the No. 1 lying behind a low-tripod heavy MG, both fists on its spade grips, firing a
                      burst a loop (his shoulders shake with it); his rifle laid by his right side
    hmg_fire_sit      (loop) the No. 1 sitting on the ground behind an M1919-style gun on its tripod, legs forward and
                      apart round the tripod's rear leg, both fists on the grips, a burst a loop; rifle laid
    sit_down, sit_up  (one-shots) into and out of hmg_fire_sit: from kneel_down's last pose he sits down behind the
                      gun, lays his rifle by his right side and takes the grips (sit_up: the same played backward)
    hmg_feed          (loop) the No. 2 lying at the gun's left, his head toward it, guiding the belt into the feed:
                      his right fist at the feed, his left lifting the belt off the box, a glance to the front; rifle
                      laid by his left side
    hmg_carry_gun     (walk loop) the gun over his right shoulder, his right fist forward on it, the left arm swinging
    hmg_carry_tripod  (walk loop) the folded tripod on his shoulders, both fists up on its front legs
    hmg_carry_boxes   (walk loop) an ammunition box hanging from each fist by its handle, swinging with the step
    mortar_load       (loop) kneeling on the tube's right: a round off its case by his left knee, up in both hands to
                      the muzzle, dropped, he ducks and turns his face away (one way, over 0.5 s), mortar_fire
                      0.58 s after the drop (the round's fall) with the top of his head under the muzzle and his fists
                      down by his chest and knee, half a metre under it
    mortar_sight      (loop) kneeling behind the tube's left, his eye to the sight, his fists on the elevation and
                      traverse cranks turning them a little

The props are STAND-INS (the ground thread has not posted its prop files): sockets chosen here for a period low
tripod MG, an M1919-style gun on a low tripod, the gun and the folded tripod as carried, an ammunition box and an 81 mm
class mortar (PROPS below). Each clip carries a `prop` block (kind, stand_in, root and turn_deg in the man's root frame,
sockets in the prop's frame; a carried prop also its pose every key and the bone it rides), `hands` (his fists, Man.grip,
every key, the root's frame) and `holds` (which fist is on which socket when), so the real props can be placed, and the
clips refitted when they are posted.

    python3 crew_clips.py --stage DIR [names...]      write DIR/<name>.crew_clips.json (the 10 clips) and its checks
    python3 crew_clips.py --check DIR [names...]      the checks of staged clips, clip by clip (remade with their
                                                      fits; DIR/crew_checks.json's rows refreshed; !! = stage again)
    python3 crew_clips.py --pictures DIR OUT [names...]   picture sheets of staged clips (near mesh, stand-in props)
    python3 crew_clips.py --deliver DIR [names...]    add the staged clips to the shared soldier files (only these
                                                      clips; each file read fresh, backed up, written whole, read back)

    --src FOLDER   read the soldier files from FOLDER instead of the shared folder (a local copy)

Every clip is made on the file's own skeleton and checked from its baked keys (check_file): the loop's gap (a
one-shot: its first and last keys against the poses it chains from and to), each bone's step between keys and the
head's turn, his feet, knees and elbows under the ground (skinned near mesh), the prone rest of hips and chest, the
head's turn from the chest, each held fist on its socket, a hand or a forearm inside his body or kit (every point of
them, found by knn against the skinned mesh, measured exactly to the surface of its triangles, past an allowance per
bone: his trunk his own idle's and walk's, elsewhere 1.5 cm for a forearm and 0.5 cm for a hand), his body in a
stand-in prop (a free hand too, but for a fist closing on or opening from a held part within HOLD_GRACE), his weapon
and the kit on his bolt bone in a prop, a laid weapon's lowest point, a slung rifle's gap to his back (every point), the bolt-bone kit's height and
its place on his body (kit_off), the gunners' eye over the receiver along the sights, and the two-man gun crew's
overlap (hmg_fire_prone beside hmg_feed).
A one-shot may go as far as the poses it must start and end on have it themselves (low_limit, hand_limit: a capture's
toes in the ground, its thumb at the cheek), no further. A clip that breaks a limit is remade with its tunables moved
a step at a time (FITS: the gun set further out, a free arm swung wider, a box carried further out, the carried gun
turned out, the tripod higher or further back, his eye further back from the sight ...; mortar_load fits its own: the
muzzle, the case, the round's way, his rests, and keeps its picks in `fits` as LOAD_FIT so --check and --pictures
remake it without picking again; then its duck, reach and lift paced slower, singly or all at once (LOAD_PACE), where a
step between keys is over 15 cm) until it passes; the values a file needed go into DIR/crew_checks.json's `fits`
with its checks and skeleton print. Several --stage runs may write
one DIR at once (crew_checks.json is updated under a lock). A radio man's long whip antenna is held still in the world
over the crew clips (steady_whip); the walks keep the file's own sway.

The skeleton's rest frames are the world's (a bone's turn is identity at rest); keys are 15 a second, rotations
[t, w, x, y, z], moves [t, x, y, z] in the parent's frame, as every clip of the files. Metres; x forward, y right, z up;
a yaw (turn_deg) is + toward +y (his right), as the game's.
"""
import json
import math
import os
import shutil
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gore  # noqa: E402
import field_clips as F  # noqa: E402
from field_clips import Man, R_axis, quat, qpow, frame_map, unit, sm, ramp, bake, times_of, stride_of, feet_low  # noqa: E402,F401

SHARED = "/mnt/project-files/battle-models/land/soldier/"
BACKUP = "/home/user/art/looks/backup/crew_clips/"
INFANTRY = ["soldier_USA", "soldier_USA_bar", "soldier_USA_nco", "soldier_USA_medic", "soldier_USA_bazooka",
            "soldier_USA_loader", "soldier_USA_radio", "soldier_USA_para", "soldier_GER", "soldier_GER_detail",
            "soldier_SOV", "soldier_SOV_winter", "soldier_SOV_snow", "soldier_ITA", "soldier_ITA_desert", "soldier_JAP",
            "soldier_CHN", "soldier_POL", "soldier_UK", "soldier_FRA"]
NAMES = ["hmg_fire_prone", "hmg_fire_sit", "sit_down", "sit_up", "hmg_feed", "hmg_carry_gun", "hmg_carry_tripod",
         "hmg_carry_boxes", "mortar_load", "mortar_sight"]
RATE = F.RATE
X_, Y_, Z_ = np.eye(3)
CURL, THUMB = 82.0, 38.0                 # (fingers round a grip, as field_clips)


# ================================================================================================ the stand-in props
class Bar(tuple):
    """A round bar's half sizes (half length, r, r): drawn as a box, checked as the cylinder it stands in for."""


def seg(p0, p1, r):
    """A bar from p0 to p1 of half-thickness r, as an oriented box (centre, half sizes, turn)."""
    p0, p1 = np.asarray(p0, float), np.asarray(p1, float)
    d = p1 - p0
    L = float(np.linalg.norm(d))
    side = Z_ if abs(unit(d) @ Z_) < 0.9 else X_
    R = frame_map(X_, Z_, unit(d), side)
    return ((p0 + p1) / 2, Bar((L / 2, r, r)), R)


def box(c, half, R=None):
    return (np.asarray(c, float), tuple(half), np.eye(3) if R is None else R)


def yaw(deg):
    return R_axis(Z_, deg)


# ---- the low tripod heavy MG, for the prone No. 1 and his No. 2 (MG 42 on its Lafette set low, a Maxim on its
# Sokolov mount, an M1917 or Vickers on its tripod at its lowest): the frame's root on the ground under the gun's pivot,
# x along the bore. Spade grips 0.26 m up, 0.15 m apart, 0.62 m behind the pivot; the receiver's top 0.33 m up (the
# prone gunner, propped on his elbows, has his eye 3 cm over it, along the sights); the belt fed from the left.
HMG_LOW = {
    "kind": "hmg", "name": "tripod MG, low (prone crew)",
    "sockets": {"grip.gunner.l": (-0.62, -0.075, 0.26), "grip.gunner.r": (-0.62, 0.075, 0.26),
                "feed": (-0.30, -0.065, 0.29), "muzzle": (0.64, 0.0, 0.30), "belt_box": (0.02, -0.30, 0.0)},
    "shapes": [("gun", box((-0.30, 0.0, 0.28), (0.25, 0.045, 0.05))),
               ("gun", seg((-0.05, 0.0, 0.28), (0.64, 0.0, 0.30), 0.033)),
               ("gun", box((-0.585, 0.0, 0.26), (0.035, 0.085, 0.012))),
               ("grip", seg((-0.62, -0.075, 0.21), (-0.62, -0.075, 0.305), 0.013)),
               ("grip", seg((-0.62, 0.075, 0.21), (-0.62, 0.075, 0.305), 0.013)),
               ("mount", box((0.0, 0.0, 0.19), (0.07, 0.05, 0.04))),
               ("mount", seg((0.02, 0.0, 0.17), (0.45, -0.36, 0.0), 0.015)),
               ("mount", seg((0.02, 0.0, 0.17), (0.45, 0.36, 0.0), 0.015)),
               ("mount", seg((-0.04, 0.0, 0.17), (-0.48, -0.30, 0.0), 0.015)),
               ("mount", seg((-0.04, 0.0, 0.17), (-0.48, 0.30, 0.0), 0.015)),
               ("box", box((0.02, -0.30, 0.09), (0.14, 0.05, 0.09), yaw(0.0)))],
}

# ---- the M1919-style gun on its low tripod, for the seated No. 1: grips 0.45 m up, the tripod's long rear leg between
# his legs, `seat` the spot on the ground under his seat.
HMG_SIT = {
    "kind": "hmg", "name": "tripod MG, seated gunner (M1919 style)",
    "sockets": {"grip.gunner.l": (-0.42, -0.07, 0.45), "grip.gunner.r": (-0.42, 0.07, 0.45),
                "feed": (-0.13, -0.065, 0.50), "muzzle": (0.74, 0.0, 0.49), "seat": (-1.27, 0.0, 0.0)},
    "shapes": [("gun", box((-0.15, 0.0, 0.48), (0.22, 0.045, 0.055))),
               ("gun", seg((0.07, 0.0, 0.48), (0.74, 0.0, 0.49), 0.03)),
               ("gun", box((-0.39, 0.0, 0.45), (0.03, 0.08, 0.012))),
               ("grip", seg((-0.42, -0.07, 0.40), (-0.42, -0.07, 0.495), 0.013)),
               ("grip", seg((-0.42, 0.07, 0.40), (-0.42, 0.07, 0.495), 0.013)),
               ("mount", box((0.0, 0.0, 0.38), (0.06, 0.05, 0.045))),
               ("mount", seg((0.02, 0.0, 0.36), (0.42, -0.46, 0.0), 0.016)),
               ("mount", seg((0.02, 0.0, 0.36), (0.42, 0.46, 0.0), 0.016)),
               ("mount", seg((-0.03, 0.0, 0.36), (-0.70, 0.0, 0.0), 0.018))],
}

# ---- the gun alone, as carried (its frame's root at its pivot, x along the bore): `carry` under the barrel jacket
# where his right fist closes, `rest` under the receiver where it lies on his shoulder.
GUN = {
    "kind": "hmg", "piece": "gun", "name": "the MG off its tripod, carried",
    "sockets": {"carry": (0.20, 0.062, 0.03), "rest": (-0.18, 0.0, -0.035), "muzzle": (0.64, 0.0, 0.06),
                "grip.gunner.l": (-0.62, -0.075, 0.0), "grip.gunner.r": (-0.62, 0.075, 0.0),
                "feed": (-0.30, -0.065, 0.045)},
    "shapes": [("gun", box((-0.30, 0.0, 0.035), (0.25, 0.045, 0.06))),
               ("gun", seg((-0.05, 0.0, 0.035), (0.64, 0.0, 0.06), 0.035)),
               ("gun", box((-0.585, 0.0, 0.0), (0.035, 0.085, 0.012))),
               ("grip", seg((0.13, 0.062, 0.03), (0.27, 0.062, 0.03), 0.008)),
               ("grip", seg((-0.62, -0.075, -0.05), (-0.62, -0.075, 0.045), 0.013)),
               ("grip", seg((-0.62, 0.075, -0.05), (-0.62, 0.075, 0.045), 0.013))],
}

# ---- the tripod folded for carrying (its frame's root at its head): two front legs along +x, the rear leg folded
# down; `grip.carry.l/r` on the front legs where his fists close, `rest` the head's pad on the back of his shoulders.
_FL = ((0.02, 0.06, 0.0), (0.70, 0.20, 0.0))
GRIP_ALONG = 0.68                   # (his fists this far out along the front legs from the head)
TRIPOD = {
    "kind": "hmg", "piece": "tripod", "name": "the MG's tripod folded, carried",
    "sockets": {"grip.carry.l": (0.02 + GRIP_ALONG * 0.68, -(0.06 + GRIP_ALONG * 0.14), 0.0),
                "grip.carry.r": (0.02 + GRIP_ALONG * 0.68, 0.06 + GRIP_ALONG * 0.14, 0.0), "rest": (0.0, 0.0, -0.045)},
    "shapes": [("mount", box((0.0, 0.0, 0.0), (0.06, 0.07, 0.045))),
               ("mount", seg((0.02, -0.06, 0.0), (0.70, -0.20, 0.0), 0.016)),
               ("mount", seg((0.02, 0.06, 0.0), (0.70, 0.20, 0.0), 0.016)),
               ("mount", seg((-0.03, 0.0, -0.02), (-0.30, 0.0, -0.64), 0.018))],
}

# ---- an MG ammunition box (MG 34/42 Patronenkasten, the US M1 box: about 0.28 x 0.10 x 0.18 m), its root under its
# middle on the ground, `handle` its folding handle's bar where the fist closes.
BOX = {
    "kind": "ammo_box", "name": "MG ammunition box",
    "sockets": {"handle": (0.0, 0.0, 0.205)},
    "shapes": [("box", box((0.0, 0.0, 0.09), (0.14, 0.05, 0.09))),
               ("grip", seg((-0.055, 0.0, 0.205), (0.055, 0.0, 0.205), 0.007)),
               ("grip", seg((-0.055, 0.0, 0.18), (-0.055, 0.0, 0.205), 0.005)),
               ("grip", seg((0.055, 0.0, 0.18), (0.055, 0.0, 0.205), 0.005))]}

# ---- the 81 mm class mortar: its root at the base plate's socket on the ground, x the way it fires. The tube at 60
# degrees, 1.10 m from the ball, its muzzle 1.01 m up; the bipod ahead, the sight on the bipod's left 0.64 m up (its
# eyepiece `sight.eye` behind it); two cranks (stand-in sockets the contract does not name yet: hub, axis and arm).
ELEV = 60.0
TUBE = 1.10
_U = np.array((math.cos(math.radians(ELEV)), 0.0, math.sin(math.radians(ELEV))))
_BALL = np.array((0.0, 0.0, 0.06))
_MUZ = _BALL + TUBE * _U
_COLLAR = _BALL + 0.66 * TUBE * _U
CRANKS = {"crank.traverse": {"hub": (0.50, -0.17, 0.38), "axis": (0.0, 1.0, 0.0), "arm": 0.045},
          "crank.elevation": {"hub": (0.42, -0.19, 0.30), "axis": (0.0, 0.0, 1.0), "arm": 0.05}}


def crank_block():
    """prop.cranks: each crank's hub, axis and arm, and how its cranks_deg turn reads: its handle at the hub plus
    `arm` along `zero` (the prop's +x laid across the axis, or the prop's +x for an upright axis) turned the
    crank's cranks_deg about `axis`, right-handed (+ anticlockwise looking down the axis toward the hub)."""
    out = {}
    for k, c in CRANKS.items():
        ax = unit(c["axis"])
        ref = unit(np.cross(ax, Z_)) if abs(ax @ Z_) < 0.9 else X_
        ref = unit(ref - ax * (ref @ ax))
        out[k] = dict(c, zero=[round(float(v), 4) for v in ref], sense="right-handed about axis")
    return out


MORTAR = {
    "kind": "mortar", "name": "81 mm class mortar",
    "sockets": {"muzzle": tuple(_MUZ.round(4)), "sight": (0.42, -0.32, 0.64), "sight.eye": (0.345, -0.32, 0.675),
                "crank.traverse": CRANKS["crank.traverse"]["hub"], "crank.elevation": CRANKS["crank.elevation"]["hub"]},
    "shapes": [("mount", box((0.0, 0.0, 0.025), (0.24, 0.20, 0.025))),
               ("gun", seg(_BALL, _MUZ, 0.045)),
               ("mount", seg((0.44, 0.0, 0.36), (0.64, -0.27, 0.0), 0.015)),
               ("mount", seg((0.44, 0.0, 0.36), (0.64, 0.27, 0.0), 0.015)),
               ("mount", seg(_COLLAR, (0.44, 0.0, 0.28), 0.022)),
               ("mount", seg((0.42, -0.03, 0.57), (0.42, -0.295, 0.62), 0.01)),
               ("gun", box((0.42, -0.32, 0.64), (0.06, 0.022, 0.035), R_axis(Y_, 20.0))),
               ("mount", seg((0.50, -0.02, 0.38), (0.50, -0.16, 0.38), 0.012)),
               ("mount", seg((0.44, 0.0, 0.30), (0.42, -0.18, 0.30), 0.012))]}

# ---- the mortar round (81 mm: fins, body and fuse, 0.38 m) and the case the rounds lie on by his left knee
ROUND_LONG = 0.38
ROUND = {"kind": "mortar_round", "name": "mortar round",
         "sockets": {"tail": (0.0, 0.0, 0.0), "hold.l": (0.11, 0.045, 0.045), "hold.r": (0.23, 0.045, 0.045)},
         "shapes": [("round", box((0.045, 0.0, 0.0), (0.045, 0.032, 0.032))),
                    ("round", seg((0.08, 0.0, 0.0), (0.32, 0.0, 0.0), 0.04)),
                    ("round", seg((0.32, 0.0, 0.0), (0.38, 0.0, 0.0), 0.018))]}
CASE = {"kind": "ammo_case", "name": "the rounds' case",
        "shapes": [("box", box((0.0, 0.0, 0.16), (0.20, 0.11, 0.16)))]}      # (CASE_SIZE: 0.40 x 0.22 x 0.32 m)


# ================================================================================================ the man and his kit
KIT_FRAME = "spine_02"              # (a part on the bolt bone that is body kit rides this bone's frame)
KIT_CLEAR, KIT_LIFT = 0.003, 0.028  # (lying on it: the part kept this far over the ground, pushed up at most this much)


def crew_man(D):
    """The file's Man, with `kit` set when the bolt bone carries a piece of his kit rather than the rifle's bolt (the
    radio man's handset on his left strap, the bazooka loader's rocket in the bag on his back): the files' own clips
    then key the bolt every frame so the part stays on his body while the weapon moves. Found from the file's own
    clips: the bolt keeps its place on his KIT_FRAME (within 4 cm) through idle, walk and kneel_down while the weapon
    moves 15 cm or more there. For such a file his weapon's points (lay, sling) leave the bolt's out."""
    man = Man(D)
    man.kit = False
    if "bolt" not in man.ix:
        return man
    M = D["near"]
    a = np.array(M["part"])
    bo, sp, wi = man.b("bolt"), man.b(KIT_FRAME), man.b("weapon")
    if not (a == bo).any():
        return man
    spread_b, wpos = [], []
    for cn in ("idle", "walk", "kneel_down"):
        c = D["clips"].get(cn)
        if c is None:
            return man
        rel = []
        for t in times_of(c["long"])[::2]:
            G, X = gore.pose(man.rig, c, t)
            rel.append(G[sp].T @ (X[bo] - X[sp]))
            wpos.append(G[sp].T @ (X[wi] - X[sp]))
        spread_b.append(float(np.ptp(np.array(rel), 0).max()))
    wspread = float(np.ptp(np.array(wpos), 0).max())
    man.kit = max(spread_b) < 0.04 and wspread > 0.15
    if man.kit:
        P = np.array(M["points"]).reshape(-1, 3)
        b, w = np.array(M["part2"]), np.array(M["w2"], float)
        sub = [man.b(x) for x in ("weapon", "bipod") if x in man.ix]
        on = np.isin(a, sub) & (np.isin(b, sub) | (w < 0.5))
        man.weapon_pts = P[on]
    return man


def kit_rel(man, clip, t):
    """The bolt's turn and head in his KIT_FRAME's frame as the file's own `clip` has it at t."""
    G, X = gore.pose(man.rig, man.D["clips"][clip], t)
    sp, bo = man.b(KIT_FRAME), man.b("bolt")
    return G[sp].T @ G[bo], G[sp].T @ (X[bo] - X[sp])


def kit_ref(man, clip, tf=None):
    """A function of t giving the bolt's place on his body (kit_rel) from the file's own `clip` at tf(t) (its last
    pose when tf is None); memoised."""
    memo = man.__dict__.setdefault("_kit_memo", {})
    L = man.D["clips"][clip]["long"]

    def f(t):
        tc = L if tf is None else tf(t)
        k = (clip, round(tc, 5))
        if k not in memo:
            memo[k] = kit_rel(man, clip, tc)
        return memo[k]
    return f


def put_kit(man, lq, mv, R, p):
    """The bolt (his kit's part) set so it sits at (R, p) in his KIT_FRAME's frame, wherever the weapon is."""
    G, X = man.fk(lq, mv)
    sp, bo = man.b(KIT_FRAME), man.b("bolt")
    pa = man.par[bo]
    Gb, Xb = G[sp] @ R, X[sp] + G[sp] @ p
    lq[bo] = G[pa].T @ Gb
    mv[bo] = G[pa].T @ (Xb - X[pa]) - (man.H[bo] - man.H[pa])
    return lq, mv


def keep_kit(man, fn, ref):
    """A clip's pose function with the bolt keyed every frame so his kit's part stays on his body (crew_man.kit)."""
    if not getattr(man, "kit", False) or ref is None:
        return fn

    B = BODY(man)

    def f(t):
        lq, mv = fn(t)
        R, p = ref(t)
        lq, mv = put_kit(man, lq.copy(), mv.copy(), R, p)
        # (lying on it, the part is pushed up off the ground (into his coat) as far as KIT_LIFT, not through it)
        G, X = man.fk(lq, mv)
        if len(B.Pk):
            dz = min(KIT_LIFT, KIT_CLEAR - float(B.skin_kit(G, X)[:, 2].min()))
            if dz > 0.0:
                bo = man.b("bolt")
                mv[bo] = mv[bo] + G[man.par[bo]].T @ (Z_ * dz)
        return lq, mv
    return f


# ================================================================================================ helpers
def set_global(man, lq, mv, name, Gnew):
    G, X = man.fk(lq, mv)
    i = man.b(name)
    lq[i] = G[man.par[i]].T @ Gnew
    return lq


def eye_of(man):
    """The middle of his eyes (the iris paint's points) from the head bone's head, in the rest frame."""
    if not hasattr(man, "_eye"):
        M = man.D["near"]
        P = np.array(M["points"]).reshape(-1, 3)
        paint = np.array(M["paints"])
        iris = man.D.get("new_paints", {}).get("iris")
        sel = (paint == iris) if iris is not None else np.zeros(len(P), bool)
        H = man.H[man.b("head")]
        man._eye = (P[sel].mean(0) - H) if sel.any() else np.array((0.097, 0.0, 0.038))
    return man._eye


def eye_at(man, G, X):
    h = man.b("head")
    return X[h] + G[h] @ eye_of(man)


def look(man, lq, mv, target, neck=0.45, roll=0.0, iters=3, toward=None, level=1.0):
    """His head turned (its share at the neck) so his eyes look at `target` (or along `toward`), level unless
    `roll` (degrees, + his head tipped to his left); `level` below 1 lets the head keep that much less than all of
    the chest's sideways tip."""
    ch, ne, he = man.b("chest"), man.b("neck"), man.b("head")
    for _ in range(iters):
        G, X = man.fk(lq, mv)
        d = unit(toward) if toward is not None else unit(np.asarray(target, float) - eye_at(man, G, X))
        upv = Z_ if level >= 1.0 else unit(level * Z_ + (1.0 - level) * G[ch][:, 2])
        Gd = frame_map(X_, Z_, d, upv)
        if roll:
            Gd = R_axis(d, roll) @ Gd
        Dl = Gd @ G[he].T
        Gn = qpow(Dl, neck) @ G[ne]
        lq[ne] = G[ch].T @ Gn
        lq[he] = Gn.T @ Gd
    return lq


def turn_chain(man, lq, bones, axis, deg):
    """A turn about the bones' own `axis` (their rest frame's), shared out along the chain [(bone, share)]."""
    for b, f in bones:
        lq[man.b(b)] = lq[man.b(b)] @ R_axis(axis, deg * f)
    return lq


SPINE = (("spine_01", 0.35), ("spine_02", 0.35), ("chest", 0.30))


def trunk(man, lq, bend=0.0, turn=0.0, lean=0.0, chain=SPINE):
    """His back bent forward (deg, about y), turned to his right (about z) and leaned to his left (about x), shared up
    the spine."""
    for b, f in chain:
        i = man.b(b)
        lq[i] = lq[i] @ R_axis(Y_, bend * f) @ R_axis(Z_, turn * f) @ R_axis(X_, lean * f)
    return lq


def leg(man, lq, mv, s, T, pole, Gfoot=None):
    """Two-bone IK for a leg: the ankle (foot bone's head) at T, the knee toward `pole`; the foot set to the world
    turn Gfoot (or left as its parent carries it)."""
    G, X = man.fk(lq, mv)
    th, ca, fo = man.b("thigh_" + s), man.b("calf_" + s), man.b("foot_" + s)
    u0, l0 = man.H[ca] - man.H[th], man.H[fo] - man.H[ca]
    l1, l2 = float(np.linalg.norm(u0)), float(np.linalg.norm(l0))
    S = X[th]
    T = np.asarray(T, float)
    dv = T - S
    dist = float(np.linalg.norm(dv))
    dc = min(max(dist, abs(l1 - l2) + 1e-4), (l1 + l2) * 0.999)
    u = dv / max(dist, 1e-9)
    v = unit(np.asarray(pole, float) - u * (np.asarray(pole, float) @ u))
    c = (l1 * l1 + dc * dc - l2 * l2) / (2 * l1 * dc)
    K = S + l1 * (u * c + v * math.sqrt(max(0.0, 1 - c * c)))
    Tc = S + u * dc
    up, lo = K - S, Tc - K
    n = np.cross(up, lo)
    if np.linalg.norm(n) < 1e-6:
        n = np.cross(up, v)
    n = unit(n)
    f1 = unit(np.cross(n, up))
    sg = 1.0 if f1 @ v >= 0 else -1.0
    f1, f2 = sg * f1, sg * unit(np.cross(n, lo))
    r1 = unit(X_ - unit(u0) * (X_ @ unit(u0)))
    r2 = unit(X_ - unit(l0) * (X_ @ unit(l0)))
    Gt = frame_map(u0, r1, up, f1)
    Gc = frame_map(l0, r2, lo, f2)
    lq[th] = G[man.par[th]].T @ Gt
    lq[ca] = Gt.T @ Gc
    if Gfoot is not None:
        lq[fo] = Gc.T @ Gfoot
    return lq


def fist_to(man, lq, mv, s, at, d, t, pole, share=0.5, curl=CURL, thumb=THUMB):
    """His fist (Man.grip) closed on `at`, the hand along d, its thumb toward t, the elbow toward `pole`."""
    d, t = unit(d), unit(t)
    t = unit(t - d * (t @ d))
    for _ in range(2):
        T = man.wrist_for(s, at, d, t)
        lq = man.arm(lq, mv, s, T, pole, d, t, share)
    return man.fingers(lq, s, curl, thumb)


LAY_UP = 0.005                      # (a laid weapon's lowest point this far over the ground)


def lay_rifle(man, lq, mv, at, along):
    """Man.lay_weapon's way with the place and the way given: his weapon on its side on the ground, its middle at
    `at` (x, y), its muzzle along `along`. On its side its rest +y is up, so the side that faces down is its least y:
    its middle stands (mean y - least y) + LAY_UP over the ground (Man.lay_weapon's max |y - mean| floats it by the
    difference of its two sides, about 4 cm on most rifles)."""
    if len(man.weapon_pts) == 0:
        return lq, mv
    G, X = man.fk(lq, mv)
    Pw = man.weapon_pts
    lo, hi = Pw[:, 0].min(), Pw[:, 0].max()
    mid = np.array(((lo + hi) / 2, Pw[:, 1].mean(), Pw[:, 2].mean()))
    half = float(Pw[:, 1].mean() - Pw[:, 1].min())
    a = np.array((along[0], along[1], 0.0))
    Gw = frame_map(X_, Y_, unit(a), Z_)
    target = np.array((at[0], at[1], half + LAY_UP))
    wi = man.b("weapon")
    Xw = target - Gw @ (mid - man.H[wi])
    p = man.par[wi]
    lq[wi] = G[p].T @ Gw
    mv[wi] = G[p].T @ (Xw - X[p]) - (man.H[wi] - man.H[p])
    return lq, mv


def lay_right(man, lq, mv):
    """Man.lay_weapon's place (on the ground by his right side, along him, muzzle toward his head, 0.3 m out from
    between his right shoulder and hip), laid by lay_rifle (its down side on the ground)."""
    G, X = man.fk(lq, mv)
    along = X[man.b("head")] - X[man.b("pelvis")]
    right = X[man.b("upperarm_r")] - X[man.b("upperarm_l")]
    right[2] = 0.0
    at = 0.5 * (X[man.b("upperarm_r")] + X[man.b("thigh_r")]) + unit(right) * 0.3
    return lay_rifle(man, lq, mv, at, along)


def weapon_world(man, lq, mv):
    """The weapon bone's world turn and head as this pose has it (to keep a laid rifle still on the ground)."""
    G, X = man.fk(lq, mv)
    wi = man.b("weapon")
    return G[wi].copy(), X[wi].copy()


def put_weapon(man, lq, mv, W):
    """The weapon bone set to the world turn and head W (from weapon_world)."""
    G, X = man.fk(lq, mv)
    wi = man.b("weapon")
    p = man.par[wi]
    lq[wi] = G[p].T @ W[0]
    mv[wi] = G[p].T @ (W[1] - X[p]) - (man.H[wi] - man.H[p])
    return lq, mv


def sling_left(man, lq, mv, way=None):
    """Man.sling mirrored: muzzle up by his LEFT shoulder, butt by his right hip (his right shoulder carries the
    gun). With `way` (tilt, drop): Man.sling's side (muzzle by his right shoulder), the rifle laid `tilt` across for
    each up (Man.sling's 0.38) and its middle `drop` lower on his back."""
    if len(man.weapon_pts) == 0:
        return lq, mv
    if way is not None:
        return sling_way(man, lq, mv, unit((0.0, way[0], 1.0)), np.array((man.back_x - 0.035, 0.04, 1.3 - way[1])))
    return sling_way(man, lq, mv, unit((0.0, -0.38, 1.0)), np.array((man.back_x - 0.035, -0.04, 1.3)))


def sling_way(man, lq, mv, a, centre_rest):
    """The rifle slung across his back along `a` (his chest's rest frame), its middle at `centre_rest`."""
    G, X = man.fk(lq, mv)
    Pw = man.weapon_pts
    lo, hi = Pw[:, 0].min(), Pw[:, 0].max()
    mid = np.array(((lo + hi) / 2, Pw[:, 1].mean(), Pw[:, 2].mean()))
    Rw = frame_map(X_, Y_, a, -X_)
    ch = man.b("chest")
    Gc, Xc = G[ch], X[ch]
    Gw = Gc @ Rw
    target = Xc + Gc @ (centre_rest - man.H[ch])
    wi = man.b("weapon")
    Xw = target - Gw @ (mid - man.H[wi])
    lq[wi] = G[man.par[wi]].T @ Gw
    mv[wi] = G[man.par[wi]].T @ (Xw - X[man.par[wi]]) - (man.H[wi] - man.H[man.par[wi]])
    return lq, mv


def min_gap(P, Q, chunk=24):
    """The least distance between two point sets (every point of each: no thinning, so a near pair is not missed).
    Exact, but quick: a bound first from every few points of P, then P in runs along its longest side, each run
    measured against only the points of Q inside its box grown by the bound so far (no nearer point lies outside)."""
    P, Q = np.asarray(P, float), np.asarray(Q, float)
    if not len(P) or not len(Q):
        return 9.0
    sub = P[::max(1, len(P) // 48)]
    best = float((((sub[:, None, :] - Q[None, :, :]) ** 2).sum(-1)).min())
    r = math.sqrt(best)
    Q = Q[np.all((Q >= P.min(0) - r) & (Q <= P.max(0) + r), axis=1)]
    Ps = P[np.argsort(P[:, int(np.argmax(np.ptp(P, axis=0)))])]
    for i in range(0, len(Ps), chunk):
        A = Ps[i:i + chunk]
        r = math.sqrt(best)
        m = np.all((Q >= A.min(0) - r) & (Q <= A.max(0) + r), axis=1)
        if m.any():
            best = min(best, float((((A[:, None, :] - Q[m][None, :, :]) ** 2).sum(-1)).min()))
    return math.sqrt(best)


def settle_sling(man, lq, mv, keep=0.012, left=False, way=None):
    """The slung rifle (Man.sling, or sling_left, or laid another way: sling_left's `way`) brought in along his
    chest's way until it lies `keep` off his back: Man.sling rides it at its upright place, which stands off his back
    once he bends deep, and behind the furthest point of his back kit (a mess tin, a bread bag) where his back is
    flat."""
    lq, mv = sling_left(man, lq, mv, way) if (left or way is not None) else man.sling(lq, mv)
    if "weapon" not in man.ix or len(man.weapon_pts) == 0:
        return lq, mv
    B = BODY(man)
    wi = man.b("weapon")
    for _ in range(4):
        G, X = man.fk(lq, mv)
        W = B.skin_gun(G, X)
        if not len(W):
            break
        d = min_gap(W, B.skin(G, X, B.zone["trunk"]))       # (as the checks measure it: every point)
        if d <= keep + 0.004:
            break
        f = G[man.b("chest")][:, 0]
        mv[wi] = mv[wi] + G[man.par[wi]].T @ (f * (d - keep))
    return lq, mv


def eased(keys, t, L):
    """Values through keys [(t, array)] round a loop of length L, smoothstep between neighbours (the last key back to
    the first at L)."""
    t = t % L
    n = len(keys)
    for i in range(n):
        t0, v0 = keys[i]
        t1, v1 = keys[i + 1] if i + 1 < n else (L, keys[0][1])
        if t0 <= t < t1 or (i == n - 1 and t >= t0):
            u = sm((t - t0) / max(t1 - t0, 1e-9))
            return np.asarray(v0, float) * (1 - u) + np.asarray(v1, float) * u
    return np.asarray(keys[0][1], float)


def flowed(keys, t, L):
    """Values through keys [(t, array)] round a loop of length L, a cubic through each key that does not stop at it
    when the values run on the same way through it (each value's slope there the harmonic mean of its slopes either
    side, none where they turn back or hold: no overshoot), so a move through several keys does not start and stop at
    each."""
    n = len(keys)
    T = [k[0] for k in keys]
    V = [np.asarray(k[1], float) for k in keys]

    def slope(i, j):                                    # (from key i to the next, j, round the loop)
        dt = (T[j] - T[i]) % L or L
        return (V[j] - V[i]) / dt

    def tangent(i):
        a, b = slope((i - 1) % n, i), slope(i, (i + 1) % n)
        m = np.zeros_like(a)
        ok = (a * b) > 0
        m[ok] = 2.0 / (1.0 / a[ok] + 1.0 / b[ok])
        return m

    t = t % L
    for i in range(n):
        j = (i + 1) % n
        t0, t1 = T[i], T[j] if j else L
        if t0 <= t < t1 or (i == n - 1 and t >= t0):
            h = t1 - t0
            u = (t - t0) / max(h, 1e-9)
            h00, h10, h01, h11 = 2 * u ** 3 - 3 * u ** 2 + 1, u ** 3 - 2 * u ** 2 + u, -2 * u ** 3 + 3 * u ** 2, u ** 3 - u ** 2
            return h00 * V[i] + h10 * h * tangent(i) + h01 * V[j] + h11 * h * tangent(j)
    return V[0]


def ease_flat(x, a=0.25):
    """0 to 1 as x goes 0 to 1: speeding up over the first `a`, even, slowing over the last `a` (its top speed
    1/(1-a) times its mean; smoothstep's is 1.5)."""
    x = min(max(x, 0.0), 1.0)
    v = 1.0 / (1.0 - a)
    if x < a:
        return 0.5 * v * x * x / a
    if x <= 1.0 - a:
        return 0.5 * v * a + v * (x - a)
    return 1.0 - 0.5 * v * (1.0 - x) ** 2 / a


def rot_slerp(Ra, Rb, u):
    return qpow(Rb @ Ra.T, u) @ Ra


# ================================================================================================ his body, for checks
class Body:
    """His near mesh's points by zone, at rest, for setting him on the ground and for the checks."""

    def __init__(self, man):
        M = man.D["near"]
        P = np.array(M["points"]).reshape(-1, 3)
        a, b, w = np.array(M["part"]), np.array(M["part2"]), np.array(M["w2"], float)
        gun = [man.b(x) for x in ("weapon", "bolt", "bipod") if x in man.ix]
        kit = [man.b("bolt")] if getattr(man, "kit", False) else []
        whip = [man.b(x) for x in ("whip_1", "whip_2", "whip_3", "whip_4") if x in man.ix]
        N = np.array(M["facings"]).reshape(-1, 3)
        self.kitpts = np.isin(a, kit)
        self.gunpts = np.isin(a, gun) & ~self.kitpts
        keep = ~np.isin(a, gun) & ~np.isin(b, gun) & ~np.isin(a, whip)
        self.P, self.a, self.b, self.w, self.N = P[keep], a[keep], b[keep], w[keep], N[keep]
        self.Pw, self.aw, self.bw, self.ww = P[self.gunpts], a[self.gunpts], b[self.gunpts], w[self.gunpts]
        self.Nw = N[self.gunpts]
        # (his kit on the bolt bone, when it is kit: checked as his body's kit (a hand in it, it in a prop, under the
        # ground, off its place on him))
        self.Pk, self.ak, self.bk, self.wk = P[self.kitpts], a[self.kitpts], b[self.kitpts], w[self.kitpts]
        self.Nk = N[self.kitpts]
        # (his mesh's triangles, by global point; and each group's global points, so a call's obstacle points (rest of
        # him, his rifle, his kit, concatenated) can be met with the triangles between them: see surface_depth)
        self.tri = np.array(M["triangles"], int).reshape(-1, 3)
        self.g_keep, self.g_gun = np.nonzero(keep)[0], np.nonzero(self.gunpts)[0]
        self.g_kit = np.nonzero(self.kitpts)[0]
        self.n_all = len(P)
        self._tris = {}
        self.man = man
        H = man.H
        on = lambda names: np.isin(self.a, [man.b(x) for x in names if x in man.ix])
        near = lambda bone, r: np.linalg.norm(self.P - H[man.b(bone)], axis=1) < r
        self.zone = {
            "feet": on(("foot_l", "ball_l", "foot_r", "ball_r")),
            "knees": (on(("thigh_l", "calf_l")) & near("calf_l", 0.10)) | (on(("thigh_r", "calf_r")) & near("calf_r", 0.10)),
            "elbows": (on(("upperarm_l", "lowerarm_l")) & near("lowerarm_l", 0.08)) |
                      (on(("upperarm_r", "lowerarm_r")) & near("lowerarm_r", 0.08)),
            "hips": on(("pelvis", "pouch_l", "pouch_r")),
            "chest": on(("spine_02", "chest")),
            "legs": on(("thigh_l", "calf_l", "thigh_r", "calf_r")),
            "thighs": on(("thigh_l", "thigh_r")),
            "hands": on(("hand_l", "fingers_l", "thumb_l", "hand_r", "fingers_r", "thumb_r")),
            "hand_l": on(("hand_l", "fingers_l", "thumb_l")), "hand_r": on(("hand_r", "fingers_r", "thumb_r")),
            "arm_l": on(("upperarm_l", "lowerarm_l")), "arm_r": on(("upperarm_r", "lowerarm_r")),
            "fore": on(("lowerarm_l", "lowerarm_r")),
            "fore_l": on(("lowerarm_l",)), "fore_r": on(("lowerarm_r",)),
            "upper_l": on(("upperarm_l",)), "upper_r": on(("upperarm_r",)),
            "head": on(("head", "neck")),
            "trunk": on(("pelvis", "spine_01", "spine_02", "chest", "neck", "head", "clavicle_l", "clavicle_r",
                         "pouch_l", "pouch_r")),
        }

    def normals(self, G, N, a, b, w):
        n = np.einsum("nij,nj->ni", G[a], N) * (1 - w)[:, None] + np.einsum("nij,nj->ni", G[b], N) * w[:, None]
        return n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-9)

    def through(self, G, X, Q=None, step=1, probe="hands", gun=True, lim=None):
        """How deep a hand (probe "hands") or a forearm ("fore") goes into his body or his kit (his slung or laid
        rifle unless `gun` is False, the kit on his bolt bone): every probe point (`step` 1; every other point missed
        a forearm 2 cm into a thigh that the next point found), knn_depth of each against the rest of him
        (not his arms), so a point deep in a thigh is found too, not only one within a few cm of the skin; each point
        it finds inside then measured exactly to the surface (surface_depth), as an independent check would. With `lim`
        (a depth allowed for each bone the point goes into) also how far the worst point goes past its allowance:
        (depth, where) or (depth, where, excess, where)."""
        Q = self.skin(G, X) if Q is None else Q
        hz = self.zone[probe]
        Hp = Q[hz][::step]
        rest = ~(self.zone["hands"] | self.zone["arm_l"] | self.zone["arm_r"])
        Bp = [Q[rest]]
        Bn = [self.normals(G, self.N[rest], self.a[rest], self.b[rest], self.w[rest])]
        Ba = [self.a[rest]]
        if len(self.Pw) and gun:
            Bp.append(self.skin_gun(G, X))
            Bn.append(self.normals(G, self.Nw, self.aw, self.bw, self.ww))
            Ba.append(self.aw)
        if len(self.Pk):
            Bp.append(self.skin_kit(G, X))
            Bn.append(self.normals(G, self.Nk, self.ak, self.bk, self.wk))
            Ba.append(self.ak)
        Bp, Bn, Ba = np.concatenate(Bp), np.concatenate(Bn), np.concatenate(Ba)
        Ha = self.a[hz][::step]
        dep, near = knn_depth(Hp, Bp, Bn)
        c = np.nonzero(dep > 0.0)[0]
        if len(c):
            # (each point knn finds inside measured exactly: its distance to the nearest point of the surface between
            # the obstacle points, there behind it (knn's median depth along the samples' facings reads a forearm
            # pressed on a thigh 1 to 1.4 cm deeper than it is))
            dep[c], near[c] = surface_depth(Hp[c], Bp, Bn, self.obstacle_tris(rest, gun), dep[c], near[c])
        nm = self.man.rig.names
        i = int(dep.argmax()) if len(dep) else 0
        out = (0.0, None) if not len(dep) or dep[i] <= 0.0 else (float(dep[i]), f"{nm[Ha[i]]} into {nm[Ba[near[i]]]}")
        if lim is None:
            return out
        if not len(dep):
            return out + (0.0, None)
        ex = dep - lim[Ba[near]]
        j = int(ex.argmax())
        return out + ((float(ex[j]), f"{nm[Ha[j]]} into {nm[Ba[near[j]]]}") if ex[j] > 0.0 else (0.0, None))

    def obstacle_tris(self, rest, gun):
        """The triangles between through()'s obstacle points (the rest of him (mask `rest` of his body's points),
        then his rifle's when `gun` and there is one, then his kit's), as rows of that concatenation."""
        key = (gun and len(self.Pw) > 0, rest.tobytes())
        if key not in self._tris:
            row = np.full(self.n_all, -1)
            gr = [self.g_keep[rest]] + ([self.g_gun] if key[0] else []) + ([self.g_kit] if len(self.Pk) else [])
            off = 0
            for g in gr:
                row[g] = off + np.arange(len(g))
                off += len(g)
            T = row[self.tri]
            self._tris = {key: T[(T >= 0).all(1)]}
        return self._tris[key]

    def trunk_bones(self):
        """His trunk's bones (pelvis to chest, his pouches and collar bones): where a sleeve may press into his coat."""
        m = self.man
        return [m.b(x) for x in ("pelvis", "spine_01", "spine_02", "chest", "pouch_l", "pouch_r", "clavicle_l",
                                 "clavicle_r") if x in m.ix]

    def skin(self, G, X, mask=None):
        m = slice(None) if mask is None else mask
        return gore.skin(self.P[m], self.a[m], self.b[m], self.w[m], G, X, self.man.H)

    def skin_gun(self, G, X):
        return gore.skin(self.Pw, self.aw, self.bw, self.ww, G, X, self.man.H)

    def skin_kit(self, G, X):
        return gore.skin(self.Pk, self.ak, self.bk, self.wk, G, X, self.man.H)

    def low(self, G, X, zone):
        return float(self.skin(G, X, self.zone[zone])[:, 2].min())


KNN_SPACING = 0.02                  # (the mesh's sampling: a point inside the surface has a surface point within its
                                    #  depth and this)


def knn_depth(Pp, Bp, Bn, k=6, rad=0.10):
    """How deep each point of Pp lies inside the surface sampled by the points Bp (facings Bn): behind its k nearest
    surface points within `rad`, their median depth along their facings, where at least k-1 of them have it behind
    and that depth is at least half its distance to the nearest of them (less 1 cm for the sampling), and the nearest
    of them is no further than that depth and KNN_SPACING (a point inside a surface is at most its depth from it, and
    the samples lie within the sampling's spacing of any surface point) (0 elsewhere); and its nearest surface point's
    index. (The last two tests: a point well clear of a thin shell's open edge, a helmet's brim seen from in front or
    below, is behind the inner face's points nearest it, but only at a slant: a medic's forearm 9.3 cm from the inside
    of his helmet's rim read 2.8 cm deep in his head without the first, and one 7.2 cm before his brim's front edge
    3.2 cm deep without the second.)"""
    out = np.zeros(len(Pp))
    near = np.zeros(len(Pp), int)
    if len(Pp) == 0:
        return out, near
    lo, hi = Pp.min(0) - rad, Pp.max(0) + rad
    m = np.nonzero(np.all((Bp > lo) & (Bp < hi), axis=1))[0]
    if len(m) < k:
        return out, near
    Bq, Nq = Bp[m], Bn[m]
    for c0 in range(0, len(Pp), 300):
        Pc = Pp[c0:c0 + 300]
        d2 = ((Pc[:, None, :] - Bq[None, :, :]) ** 2).sum(-1)
        nn = np.argpartition(d2, k - 1, axis=1)[:, :k]
        dk = np.sqrt(np.take_along_axis(d2, nn, 1))
        sg = np.einsum("pkj,pkj->pk", Pc[:, None, :] - Bq[nn], Nq[nn])
        dep = -np.median(sg, axis=1)
        ok = ((dk.max(1) < rad) & ((sg < 0).sum(1) >= k - 1) & (dep > 0) & (dep >= 0.5 * (dk.min(1) - 0.01)) &
              (dk.min(1) <= dep + KNN_SPACING))
        out[c0:c0 + 300] = np.where(ok, dep, 0.0)
        near[c0:c0 + 300] = m[nn[np.arange(len(Pc)), np.argmin(np.take_along_axis(d2, nn, 1), axis=1)]]
    return out, near


def tri_closest(p, A, B, C):
    """The nearest point to p on each triangle (A, B, C) and its barycentric weights (Ericson's regions)."""
    ab, ac, ap = B - A, C - A, p - A
    d1, d2 = np.einsum("ij,ij->i", ab, ap), np.einsum("ij,ij->i", ac, ap)
    bp = p - B
    d3, d4 = np.einsum("ij,ij->i", ab, bp), np.einsum("ij,ij->i", ac, bp)
    cp = p - C
    d5, d6 = np.einsum("ij,ij->i", ab, cp), np.einsum("ij,ij->i", ac, cp)
    va, vb, vc = d3 * d6 - d5 * d4, d5 * d2 - d1 * d6, d1 * d4 - d3 * d2
    n = len(A)
    W = np.zeros((n, 3))
    den = np.maximum(va + vb + vc, 1e-30)
    W[:, 1], W[:, 2] = vb / den, vc / den                                 # (inside the face)
    W[:, 0] = 1.0 - W[:, 1] - W[:, 2]
    eps = 1e-30
    m = (va <= 0) & (d4 - d3 >= 0) & (d5 - d6 >= 0)                        # (edge BC)
    w = (d4 - d3) / np.maximum((d4 - d3) + (d5 - d6), eps)
    W[m] = np.stack((0 * w, 1 - w, w), 1)[m]
    m = (vb <= 0) & (d2 >= 0) & (d6 <= 0)                                  # (edge AC)
    w = d2 / np.maximum(d2 - d6, eps)
    W[m] = np.stack((1 - w, 0 * w, w), 1)[m]
    m = (vc <= 0) & (d1 >= 0) & (d3 <= 0)                                  # (edge AB)
    w = d1 / np.maximum(d1 - d3, eps)
    W[m] = np.stack((1 - w, w, 0 * w), 1)[m]
    for m, j in (((d6 >= 0) & (d5 <= d6), 2), ((d3 >= 0) & (d4 <= d3), 1), ((d1 <= 0) & (d2 <= 0), 0)):   # (corners)
        W[m] = 0.0
        W[m, j] = 1.0
    return W[:, :1] * A + W[:, 1:2] * B + W[:, 2:] * C, W


def winding(Pp, V, T, chunk=8):
    """The generalised winding number of each point of Pp about the triangles T between the points V (its size: about
    1 inside a closed surface, 0 outside, whichever way the triangles turn)."""
    out = np.zeros(len(Pp))
    A0, B0, C0 = V[T[:, 0]], V[T[:, 1]], V[T[:, 2]]
    for c0 in range(0, len(Pp), chunk):
        p = Pp[c0:c0 + chunk][:, None, :]
        A, B, C = A0[None] - p, B0[None] - p, C0[None] - p
        la, lb, lc = np.linalg.norm(A, axis=2), np.linalg.norm(B, axis=2), np.linalg.norm(C, axis=2)
        det = np.einsum("pti,pti->pt", A, np.cross(B, C))
        den = (la * lb * lc + np.einsum("pti,pti->pt", A, B) * lc + np.einsum("pti,pti->pt", A, C) * lb +
               np.einsum("pti,pti->pt", B, C) * la)
        out[c0:c0 + chunk] = np.abs(np.arctan2(det, den).sum(1) / (2.0 * np.pi))
    return out


WIND_OUT = 0.3                      # (a point the surface's winding number puts under this is outside it, whatever
                                    #  the facings near it say)


def surface_depth(Pc, V, N, T, dep, near, reach=0.12):
    """Each point of Pc (knn found inside, `dep` deep, its nearest sample `near`) measured on the surface of
    triangles T between the points V (facings N): its distance to the nearest point of that surface where that
    point's facing (its triangle's corners' facings, weighted) has it behind, else 0; and the corner of that triangle
    nearest it. A point with no triangle within `reach` (and its knn depth) keeps knn's reading. A point found so
    inside is kept only where the surface's winding number about it is WIND_OUT or more: an Italian's thumb 9 cm
    from his calf (by the folded leg's inner faces, their facings turned from it) read 8.7 cm inside it by knn and by
    the facings, and winds 0.04."""
    A, B, C = V[T[:, 0]], V[T[:, 1]], V[T[:, 2]]
    cen = (A + B + C) / 3.0
    rad = np.sqrt(np.maximum(np.maximum(((A - cen) ** 2).sum(1), ((B - cen) ** 2).sum(1)), ((C - cen) ** 2).sum(1)))
    out, nr = dep.copy(), near.copy()
    for i, p in enumerate(Pc):
        r = max(reach, dep[i] + 0.05)
        s = np.nonzero(((cen - p) ** 2).sum(1) < (r + rad) ** 2)[0]
        if not len(s):
            continue
        q, W = tri_closest(p, A[s], B[s], C[s])
        d = np.linalg.norm(q - p, axis=1)
        j = int(d.argmin())
        tj = T[s[j]]
        n = W[j] @ N[tj]
        out[i] = float(d[j]) if (p - q[j]) @ n < 0.0 else 0.0
        nr[i] = tj[int(W[j].argmax())]
    ins = np.nonzero(out > 0.0)[0]
    if len(ins):
        out[ins[winding(Pc[ins], V, T) < WIND_OUT]] = 0.0
    return out, nr


def seg_dist(P, p0, p1):
    d = p1 - p0
    u = np.clip(((P - p0) @ d) / max(d @ d, 1e-12), 0.0, 1.0)
    return np.linalg.norm(P - (p0 + u[:, None] * d), axis=1)


def BODY(man):
    if not hasattr(man, "_bodyz"):
        man._bodyz = Body(man)
    return man._bodyz


# ================================================================================================ the props placed
def placed(prop, root, R):
    """The prop's sockets in the man's root frame, given its root and turn there."""
    return {k: np.asarray(root, float) + R @ np.asarray(v, float) for k, v in prop["sockets"].items()}


def prop_block(prop, root, R, extra=None):
    q = quat(R)
    out = {"kind": prop["kind"], "stand_in": True, "name": prop["name"],
           "root": [round(float(v), 4) for v in root],
           "turn_deg": round(math.degrees(math.atan2(R[1, 0], R[0, 0])), 2),
           "sockets": {k: [round(float(x), 4) for x in v] for k, v in prop["sockets"].items()}}
    if "piece" in prop:
        out["piece"] = prop["piece"]
    if abs(R[2, 2] - 1.0) > 1e-6:
        out["turn_quat"] = [round(float(v), 5) for v in q]
    if extra:
        out.update(extra)
    return out


def pose_track(rows):
    """[(t, root, R, shown)] -> a key track in the root's frame: t, x, y, z, qw, qx, qy, qz, shown."""
    tr = []
    prev = None
    for t, p, R, shown in rows:
        q = quat(R)
        if prev is not None and q @ prev < 0:
            q = -q
        prev = q
        tr.append([round(t, 4)] + [round(float(v), 4) for v in p] + [round(float(v), 5) for v in q] + [int(shown)])
    return {"track_fields": ["t", "x", "y", "z", "qw", "qx", "qy", "qz", "shown"], "track": tr}


# ================================================================================================ the man's bases
def kneel_base(man):
    """kneel_down's last pose (as fire_kneel holds it), his right foot laid on its instep behind him (the capture's
    toes go 4 cm into the ground)."""
    K = man.D["clips"]["kneel_down"]
    lq, mv = man.locals_of(K, K["long"])
    B = BODY(man)
    G, X = man.fk(lq, mv)
    fo, ba = man.b("foot_r"), man.b("ball_r")
    shin = unit(X[fo] - X[man.b("calf_r")])
    d0 = unit(man.H[ba] - man.H[fo])
    best = None
    for dn in np.arange(-0.35, 0.36, 0.025):
        d = unit(shin + np.array((0.0, 0.0, dn)))
        Gf = frame_map(d0, Z_, d, -Z_)
        l2 = lq.copy()
        l2[fo] = G[man.par[fo]].T @ Gf
        l2[ba] = np.eye(3)
        G2, X2 = man.fk(l2, mv)
        z = B.low(G2, X2, "feet")
        cost = abs(z - 0.0)
        if best is None or cost < best[0]:
            best = (cost, l2)
    return best[1], mv


KNEEL_CHEST = 10.0                  # (the mortar crews' chests turned this far right of their pelvis,
KNEEL_TILT = 20.0                   #  and leaning this far forward)


def kneel_square(man):
    """kneel_base with his back turned so his chest faces KNEEL_CHEST degrees right of his pelvis: the files' captures
    end kneel_down with the chest turned anywhere from straight ahead to 35 degrees right (the rifle aimed or not), and
    the mortar crews are laid out from the chest; and bent so his chest leans KNEEL_TILT forward (one capture kneels
    hunched over his knee)."""
    lq, mv = kneel_base(man)
    G, X = man.fk(lq, mv)
    fc, fp = facing(G, man, "chest"), facing(G, man, "pelvis")
    have = math.degrees(math.atan2(fc[1], fc[0]) - math.atan2(fp[1], fp[0]))
    have = (have + 180.0) % 360.0 - 180.0
    for _ in range(3):
        lq = trunk(man, lq, turn=KNEEL_CHEST - have)
        G, X = man.fk(lq, mv)
        fc = facing(G, man, "chest")
        have = (math.degrees(math.atan2(fc[1], fc[0]) - math.atan2(fp[1], fp[0])) + 180.0) % 360.0 - 180.0
    for _ in range(3):                                 # (and bent forward KNEEL_TILT, as most captures kneel)
        z = G[man.b("chest")][:, 2]
        tilt = math.degrees(math.atan2(z @ facing(G, man, "chest"), z[2]))
        lq = trunk(man, lq, bend=KNEEL_TILT - tilt)
        G, X = man.fk(lq, mv)
    return lq, mv


PRONE_TIP = (8.0, 12.0, 0.0)        # (settle_prone's search: his body tipped forward up to this, his back bent up to
                                    #  this, his legs' range widened by this; degrees; wider for a file whose kit
                                    #  holds his chest up)
PRONE_ARCH = 30.0                   # (and his upper back (the chest bone) arched up as much as this: propped on his
                                    #  elbows, the belly and the lower chest on the ground, the eyes up at the sights)


def settle_prone(man, lq, mv, toward=X_, eye_min=0.32, hips=-0.01, chest=0.02, chest_max=0.028, eye_max=None):
    """A man lying on his front set on the ground: his body tipped forward about his pelvis, his back bent a little
    (the chest down) and his upper back arched (propped on his elbows), lifted so his hips (the belt and its pouches)
    rest 1 cm into the ground, and his legs let down at the hips until his knees and toes rest on it; the tip, bend and
    arch found so his chest (its lowest point: the lower chest) comes near `chest` while his eyes, looking along
    `toward`, stay `eye_min` up (and under eye_max) as far as his chest can stay within `chest_max` of the ground
    (lying, not propped on his hands), the head within HEAD_MAX of the chest, and least arched."""
    B = BODY(man)
    pe = man.b("pelvis")
    best = None
    for pitch in np.arange(0.0, PRONE_TIP[0] + 0.01, 1.0):
        for bend in np.arange(0.0, PRONE_TIP[1] + 0.01, 1.0):
            l1 = lq.copy()
            l1[pe] = R_axis(Y_, pitch) @ lq[pe]
            l1 = turn_chain(man, l1, (("spine_01", 0.5), ("spine_02", 0.5)), Y_, bend)
            for arch in np.arange(0.0, PRONE_ARCH + 0.01, 2.5):
                l2 = turn_chain(man, l1.copy(), (("chest", 1.0),), Y_, -arch)
                G, X = man.fk(l2, mv)
                lift = hips - B.low(G, X, "hips")
                m2 = mv.copy()
                m2[pe] = m2[pe] + np.array((0.0, 0.0, lift))
                l4 = look(man, l2.copy(), m2, None, toward=toward, iters=2)
                G4, X4 = man.fk(l4, m2)
                eye = eye_at(man, G4, X4)[2]
                zc = B.low(G4, X4, "chest")
                hc = angle_of(G4[man.b("chest")].T @ G4[man.b("head")])
                cost = (abs(zc - chest) + 4.0 * max(0.0, eye_min - eye) + 40.0 * max(0.0, zc - chest_max) +
                        0.0003 * (pitch + bend) + 0.0004 * arch + 0.002 * max(0.0, hc - HEAD_MAX))
                if eye_max is not None:
                    cost += 4.0 * max(0.0, eye - eye_max)
                if best is None or cost < best[0]:
                    best = (cost, l2, m2, pitch)
    _, l2, m2, pitch = best
    G, X = man.fk(l2, m2)
    side = G[pe].T @ Y_
    bl = None
    for hip in np.arange(-pitch - 12.0 - PRONE_TIP[2], 8.01 + PRONE_TIP[2], 0.5):
        l3 = l2.copy()
        for s in ("thigh_l", "thigh_r"):
            l3[man.b(s)] = R_axis(side, -hip) @ l2[man.b(s)]
        G3, X3 = man.fk(l3, m2)
        z = min(B.low(G3, X3, "knees"), B.low(G3, X3, "feet"))
        if bl is None or abs(z + 0.004) < bl[0]:
            bl = (abs(z + 0.004), hip, l3)
    return bl[2], m2


def walk_phase(man, slow, step):
    """The file's walk, slowed by `slow` and its legs' swing shortened to `step` (field_clips.slow_walk's way, with
    its own beat): a function of t returning (lq, mv) and each leg's swing (-1 back .. +1 forward)."""
    W = man.D["clips"]["walk"]
    L = W["long"]
    ts = np.linspace(0.0, L, 61)
    sw = {"l": [], "r": []}
    for tw in ts:
        G, X = gore.pose(man.rig, W, tw)
        for s in ("l", "r"):
            d = X[man.b("calf_" + s)] - X[man.b("thigh_" + s)]
            sw[s].append(math.atan2(d[0], -d[2]))
    for s in ("l", "r"):
        a = np.array(sw[s])
        sw[s] = (a - 0.5 * (a.max() + a.min())) / (0.5 * (a.max() - a.min()))

    def at(t, bend=(3.0, 3.0, 3.0, 4.0, 5.0)):
        tw = (t / slow) % L
        lq, mv = man.locals_of(W, tw)
        G0, X0 = man.fk(lq, mv)
        h0 = feet_low(man, G0, X0)
        for s in ("l", "r"):
            lq[man.b("thigh_" + s)] = qpow(lq[man.b("thigh_" + s)], step)
            lq[man.b("calf_" + s)] = qpow(lq[man.b("calf_" + s)], 0.5 + 0.5 * step)
            lq[man.b("foot_" + s)] = qpow(lq[man.b("foot_" + s)], 0.4 + 0.5 * step)
        pe = man.b("pelvis")
        lq[pe] = qpow(lq[pe], 0.8)
        for b, deg in zip(("spine_01", "spine_02", "chest", "neck", "head"), bend):
            lq[man.b(b)] = lq[man.b(b)] @ R_axis(Y_, deg)
        for s, sg in (("l", 1.0), ("r", -1.0)):
            lq[man.b("clavicle_" + s)] = R_axis(X_, -5.0 * sg) @ R_axis(Y_, 3.0)
        G1, X1 = man.fk(lq, mv)
        mv[pe][2] += h0 - feet_low(man, G1, X1)
        swing = {s: float(np.interp(tw, ts, sw[s])) for s in ("l", "r")}
        return lq, mv, swing
    return at, L * slow


def walk_kit(man):
    """The bolt's place on his body (kit_ref) from the file's walk at the slowed carries' walk time."""
    W = man.D["clips"]["walk"]
    return kit_ref(man, "walk", lambda t: (t / SLOW) % W["long"])


def free_arm(man, lq, mv, s, swing, amp=0.12, out=0.11, curl=22.0):
    """An empty hand hanging at his side, swung `swing` (-1..1) times `amp` m forward, the elbow a little bent."""
    G, X = man.fk(lq, mv)
    A = man.side[s]
    S = X[man.b("upperarm_" + s)]
    o = -A["med"]
    reach = 0.94 * (A["l1"] + A["l2"])
    dx = amp * swing - 0.02
    dy = o * out
    T = S + np.array((dx, dy, -math.sqrt(max(reach * reach - dx * dx - dy * dy, 0.04))))
    d = unit((0.35 + 0.6 * swing * amp / 0.12 * 0.3, -o * 0.15, -1.0))
    tt = unit((1.0, -o * 0.1, 0.25))
    lq = man.arm(lq, mv, s, T, np.array((-1.0, o * 0.4, 0.0)), d, tt)
    return man.fingers(lq, s, curl, 15.0)


# ================================================================================================ the clips
BURST = [0.1 + k / 12.0 for k in range(6)]          # (12 rounds a second, six to a burst)
FIRE_LONG = 1.2


def burst_events():
    return [{"t": round(BURST[0], 4), "fx": "muzzle_flash", "socket": "prop.muzzle", "cue": "hmg_burst"}] + \
        [{"t": round(t, 4), "fx": "muzzle_flash", "socket": "prop.muzzle", "cue": None} for t in BURST[1:]]


def shake(t):
    """The burst's shake: an envelope over the burst and a key-to-key jolt (+1, -1 alternately at 15 a second)."""
    e = ramp(t, BURST[0] - 0.04, BURST[0] + 0.03) * (1.0 - ramp(t, BURST[-1] + 0.02, BURST[-1] + 0.12))
    return e, math.cos(2 * math.pi * 7.5 * t)


def shake_trunk(man, lq, t, k=1.0):
    e, j = shake(t)
    # (pushed back by the recoil through his fists, and jolted round it)
    lq = turn_chain(man, lq, (("spine_02", 0.5), ("chest", 0.5)), Y_, -k * (1.6 * e + 0.9 * e * j))
    lq = turn_chain(man, lq, (("chest", 1.0),), Z_, k * 0.6 * e * j)
    for s, sg in (("l", 1.0), ("r", -1.0)):
        i = man.b("clavicle_" + s)
        lq[i] = lq[i] @ R_axis(Y_, -k * 2.0 * e * j) @ R_axis(X_, sg * k * 1.2 * e)
    return lq


def breathe(man, lq, t, L, deg=0.6, n=1):
    return turn_chain(man, lq, (("spine_02", 0.5), ("chest", 0.5)), Y_, -deg * math.sin(2 * math.pi * n * t / L))


# ---------------------------------------------------------------------------------------- hmg_fire_prone
PRONE_GUN_OUT = 0.08                # (the gun set this much further ahead than where his elbows just rest)
PRONE_EYE = 0.03                    # (his eyes kept this far over the receiver's top: along the sights)


def rifle_says(man, says):
    """What the about says of his rifle; a file with no weapon (the medic) says so instead (weapon none)."""
    return says if len(man.weapon_pts) else "He has no weapon (weapon none, as in his own clips). "


def make_fire_prone(man):
    B = BODY(man)
    lq0, mv0 = man.locals_of(man.D["clips"]["fire_prone"], 0.0)
    lq0 = trunk(man, lq0, turn=0.0)
    top = max(cc[2] + half[2] for part, (cc, half, Rb) in HMG_LOW["shapes"] if part == "gun" and not isinstance(half, Bar))
    lq0, mv0 = settle_prone(man, lq0, mv0, eye_min=top + PRONE_EYE + 0.004, eye_max=top + 0.08)
    G, X = man.fk(lq0, mv0)
    y0 = 0.5 * (X[man.b("upperarm_l")][1] + X[man.b("upperarm_r")][1])
    gs = HMG_LOW["sockets"]
    R = np.eye(3)

    def arms(lq, mv, root):
        S = placed(HMG_LOW, root, R)
        for s, side in (("l", -1.0), ("r", 1.0)):
            lq = fist_to(man, lq, mv, s, S["grip.gunner." + s], (1.0, -side * 0.25, -0.1), (0.15, 0.0, 1.0),
                         (0.0, side * 0.55, -1.0), share=0.6)
        return lq

    # (the gun set so his elbows rest on the ground: its grips' distance ahead of him found once)
    best = None
    for gx in np.arange(0.50, 1.05, 0.01):
        root = np.array((gx - gs["grip.gunner.l"][0], y0, 0.0))
        l2 = arms(lq0.copy(), mv0, root)
        G2, X2 = man.fk(l2, mv0)
        z = B.low(G2, X2, "elbows")
        if best is None or abs(z - 0.004) < best[0]:
            best = (abs(z - 0.004), root)
    root = best[1] + np.array((PRONE_GUN_OUT, 0.0, 0.0))
    target = root + np.array((25.0, 0.0, SIGHT["hmg_fire_prone"][0]))
    S = placed(HMG_LOW, root, R)
    W = weapon_world(man, *lay_right(man, arms(lq0.copy(), mv0.copy(), root), mv0.copy()))

    def fn(t):
        lq, mv = lq0.copy(), mv0.copy()
        lq = breathe(man, lq, t, FIRE_LONG)
        lq = shake_trunk(man, lq, t)
        lq = look(man, lq, mv, target, neck=0.4)
        lq = clamp_head(man, lq)
        lq = arms(lq, mv, root)
        return put_weapon(man, lq, mv, W)

    holds = [{"hand": "l", "socket": "grip.gunner.l", "from": 0.0, "to": FIRE_LONG},
             {"hand": "r", "socket": "grip.gunner.r", "from": 0.0, "to": FIRE_LONG}]
    return {"long": FIRE_LONG, "fn": fn, "weapon": "laid", "events": burst_events(), "holds": holds,
            "prop": prop_block(HMG_LOW, root, R), "sockets_at": lambda t: S,
            "kit_ref": kit_ref(man, "fire_prone", lambda t: 0.0), "kit_from": "fire_prone",
            "from": "crew_clips.py: this file's fire_prone (motion capture, its first pose), set on the ground, his "
                    "upper back arched up onto his elbows; the arms keyed to the gun's grips (Man.arm), the head to "
                    "the sights, a burst's shake keyed",
            "about": ("The No. 1 of a heavy machine gun on its low tripod, lying behind it (chain from fire_prone or "
                      "go_prone before; to fire_prone or crawl after, a blend): belly and lower chest on the ground, "
                      "propped on his elbows, his left fist on the prop's grip.gunner.l and his right on grip.gunner.r "
                      "every key, his eye 3-8 cm over the receiver's top, along the sights (the stand-in's receiver top "
                      "is 0.33 m up). One burst a loop: muzzle_flash six times from 0.1 s at the prop's `muzzle` socket "
                      "(event socket \"prop.muzzle\": the gun prop's socket, not the man's socket_muzzle; cue "
                      "hmg_burst on the first), his shoulders shaking with it. " +
                      rifle_says(man, "His rifle lies on its side on the ground by his right side (weapon laid). ") +
                      "`prop` places the gun (stand-in sockets; the game sets the real gun's root at `root`, turned "
                      "`turn_deg`, or the man's root by the inverse); `hands` gives his fists every key, `holds` which "
                      "fist is on which socket. His No. 2 plays hmg_feed at the same prop.")}


# ---------------------------------------------------------------------------------------- hmg_fire_sit
SIT_LEAN = (2.0, (36.0, 76.0))      # (his pelvis tipped forward this much; his back bent forward over the gun as far,
                                    #  within these, as puts his eye SIT_EYE over the receiver's rear top: degrees)
SIT_EYE = 0.075                     # (his eye this far over the receiver's top, along the sights)
SIT_FEET = (0.75, 0.46)             # (his heels this far before his hips and out from his middle: his knees low and
SIT_KNEE = (0.1, 1.0, 0.3)          #  wide, turned out this way (back, out, up), his forearms over and inside them)
SIT_ELBOW = 1.0                     # (his elbows' poles out to his sides this much for each down)
SIT_RIFLE = (0.30, 0.58)            # (his rifle laid along him by his right side: its middle this far ahead of his
                                    #  pelvis and out to his right, clear of his right leg, where he can reach)
SIT_UP_BEND = 22.0                  # (sit_down and sit_up: his back's bend sitting up, before he leans over the gun)
SIT_SEAT = 0.04                     # (the prop's `seat` socket this far behind his pelvis bone, on the ground)


def sit_base(man, seat_x=0.0, bend=None):
    """Sitting on the ground: pelvis tipped, back bent forward over the gun (`bend`, else the bend that puts his eye
    SIT_EYE over the receiver: sit_bend), legs forward and wide, knees low and out, heels down."""
    if bend is None:
        bend = sit_bend(man)
    B = BODY(man)
    lq = np.tile(np.eye(3), (man.nb, 1, 1))
    mv = np.zeros((man.nb, 3))
    pe = man.b("pelvis")
    lq[pe] = R_axis(Y_, SIT_LEAN[0])
    lq = trunk(man, lq, bend=bend)
    for s, sg in (("l", 1.0), ("r", -1.0)):
        lq[man.b("clavicle_" + s)] = R_axis(Y_, 4.0)
    Gf = {s: R_axis(Z_, sg * 14.0) @ R_axis(Y_, -14.0) for s, sg in (("l", -1.0), ("r", 1.0))}
    pole = {s: (SIT_KNEE[0], sg * SIT_KNEE[1], SIT_KNEE[2]) for s, sg in (("l", -1.0), ("r", 1.0))}

    def legs(lq, mv):
        for s, sg in (("l", -1.0), ("r", 1.0)):
            G, X = man.fk(lq, mv)
            hip = X[man.b("thigh_" + s)]
            T = np.array((hip[0] + SIT_FEET[0], sg * SIT_FEET[1], man.H[man.b("foot_" + s)][2] + 0.02))
            lq = leg(man, lq, mv, s, T, pole[s], Gf[s])
        return lq

    # (his seat on the ground: the pelvis let down until the backs of his thighs are 1 cm into it; the kit hanging
    # from his belt (an entrenching tool, a bayonet, a canteen) does not hold him up)
    z = 0.12
    for _ in range(4):
        mv[pe] = np.array((seat_x, 0.0, z)) - man.H[pe]
        lq = legs(lq, mv)
        G, X = man.fk(lq, mv)
        Q = B.skin(G, X, B.zone["thighs"])
        z += -0.01 - float(Q[:, 2].min())
    mv[pe] = np.array((seat_x, 0.0, z)) - man.H[pe]
    lq = legs(lq, mv)
    # (the feet: heels just on the ground)
    G, X = man.fk(lq, mv)
    dz = -0.003 - B.low(G, X, "feet")
    for s in ("l", "r"):
        G, X = man.fk(lq, mv)
        T = X[man.b("foot_" + s)] + np.array((0.0, 0.0, dz))
        lq = leg(man, lq, mv, s, T, pole[s], Gf[s])
    G, X = man.fk(lq, mv)
    seat = np.array((float(X[pe][0]) - SIT_SEAT, 0.0, 0.0))
    return lq, mv, seat


def sit_top():
    return max(cc[2] + half[2] for part, (cc, half, Rb) in HMG_SIT["shapes"] if part == "gun" and not isinstance(half, Bar))


def sit_bend(man):
    """The back's bend (within SIT_LEAN's) that puts his eye SIT_EYE over the seated gun's receiver top, looking
    along the sights (found once a file)."""
    key = ("sit_bend", SIT_EYE, SIT_LEAN, SIT_FEET, SIT_KNEE, SIT_SEAT)
    memo = man.__dict__.setdefault("_sit_memo", {})
    if key not in memo:
        want = sit_top() + SIT_EYE
        best = None
        for bend in np.arange(SIT_LEAN[1][0], SIT_LEAN[1][1] + 0.1, 1.0):
            lq, mv, seat = sit_base(man, seat_x=sit_seat_x(man), bend=float(bend))
            lq = look(man, lq, mv, None, neck=0.4, toward=X_)
            e = eye_at(man, *man.fk(lq, mv))
            if best is None or abs(e[2] - want) < best[0]:
                best = (abs(e[2] - want), float(bend))
        memo[key] = best[1]
    return memo[key]


def sit_arms(man, lq, mv, S):
    for s, side in (("l", -1.0), ("r", 1.0)):
        lq = fist_to(man, lq, mv, s, S["grip.gunner." + s], (1.0, -side * 0.2, -0.15), (0.1, 0.0, 1.0),
                     (-0.2, side * SIT_ELBOW, -1.0), share=0.6)
    return lq


def make_fire_sit(man):
    lq0, mv0, seat = sit_base(man, seat_x=sit_seat_x(man))
    R = np.eye(3)
    root = seat - np.asarray(HMG_SIT["sockets"]["seat"], float)
    S = placed(HMG_SIT, root, R)
    target = root + np.array((25.0, 0.0, SIGHT["hmg_fire_sit"][0]))
    G, X = man.fk(lq0, mv0)
    W = weapon_world(man, *lay_rifle(man, lq0.copy(), mv0.copy(), (X[man.b("pelvis")][0] + SIT_RIFLE[0],
                                                                     SIT_RIFLE[1]), (1.0, 0.0)))

    def fn(t):
        lq, mv = lq0.copy(), mv0.copy()
        lq = breathe(man, lq, t, FIRE_LONG)
        lq = shake_trunk(man, lq, t)
        lq = look(man, lq, mv, target, neck=0.4)
        lq = clamp_head(man, lq)
        lq = sit_arms(man, lq, mv, S)
        return put_weapon(man, lq, mv, W)

    holds = [{"hand": "l", "socket": "grip.gunner.l", "from": 0.0, "to": FIRE_LONG},
             {"hand": "r", "socket": "grip.gunner.r", "from": 0.0, "to": FIRE_LONG}]
    return {"long": FIRE_LONG, "fn": fn, "weapon": "laid", "events": burst_events(), "holds": holds,
            "prop": prop_block(HMG_SIT, root, R), "sockets_at": lambda t: S, "laid": W,
            "kit_ref": kit_ref(man, "kneel_down"), "kit_from": "kneel_down (its last pose)",
            "from": "crew_clips.py: keyed on the rig (sitting: pelvis, back and legs keyed, the legs by two-bone IK, "
                    "his seat let down onto the ground, his back bent over the gun until his eye is along the "
                    "sights), the arms keyed to the gun's grips, a burst's shake keyed",
            "about": ("The No. 1 of an M1919-style machine gun on its low tripod, sitting on the ground behind it: his "
                      "seat on the prop's `seat` socket (on the ground under him), his legs forward and wide round the "
                      "tripod's rear leg, knees low and turned out, heels down; bent well forward over the gun, his "
                      "left fist on grip.gunner.l and his right on grip.gunner.r every key (grips 0.45 m up, his "
                      "forearms over and inside his knees, elbows out), his eye 5-10 cm over the receiver's rear top "
                      "(0.535 m up), along the sights. One burst a loop: muzzle_flash at prop.muzzle six times from "
                      "0.1 s (event socket \"prop.muzzle\": the gun prop's socket, not the man's socket_muzzle; cue "
                      "hmg_burst on the first), his shoulders shaking with it. " +
                      rifle_says(man, "His rifle lies on its side on the ground by his right side (weapon laid). ") +
                      "Chain kneel_down, sit_down, hmg_fire_sit, sit_up, stand_up: sit_down starts on kneel_down's "
                      "last pose and ends on this clip's first key, sit_up the other way. `prop`, `hands`, `holds` "
                      "as hmg_fire_prone's.")}


# ---------------------------------------------------------------------------------------- sit_down, sit_up
SIT_LONG = 3.2
SIT_FOOT = 0.25                     # (sit_down: his right foot off its toes onto its instep by this)
SIT_BODY = (0.15, 1.45)             # (his back and hips from kneeling to sitting up)
SIT_LEGS = {"r": (0.15, 1.60, 0.14, 0.36), "l": (0.40, 1.40, 0.08, 0.05)}   # (each foot's way to where it sits:
                                    #  from, to, lifted this much and swung out this much at the middle)
SIT_KNEE_OUT = {"l": 0.8, "r": 2.6}  # (each knee's way turned out this much at the middle)
SIT_LAY = (1.30, 2.30)              # (his rifle, riding his chest till then, laid down by his right side)
SIT_BEND = (2.20, 3.20)             # (bent over the gun, his fists onto the grips: the left from SIT_BODY's end)
SIT_LIFT = (0.06, 0.34)             # (the rifle's middle lifted this much and swung out to his right this much on
                                    #  its way down to the ground, clear of his shoulder;
SIT_LAY_LEAN = 12.0                 #  and he leans to his right this much to put it down)
SIT_PORT = (0.25, 0.20, 1.0)        # (the rifle held up before him as he sits, its muzzle this way, moved this far
SIT_PORT_AT = (0.15, 0.10, -0.22)   #  out from where it rides his chest: clear of his face)
SIT_BACK = 0.10                     # (he sits this far behind where his hips are as he kneels)
SIT_FREE = (1.10, 1.45)             # (no rifle: his fists held before his belly till these, left, right, then to the grips;
SIT_READY = (0.60, (0.34, -0.12, -0.05), 0.10, (1.0, -0.5, -0.6))   #  brought there by this (s), this far before, in from and under
                                    #  his shoulder, in his chest's frame: clear of his right knee as it swings round;
                                    #  lifted this much on the way, the elbow turned down and in this way)
SIT_ON_KNEE = ("l",)                # (no rifle: these hands stay on their knees (where kneel_down has the left) till then,
SIT_KNEE_POLE = (-0.2, 1.0, 0.2)    #  the elbow out to the side this way: the forearm over the thigh, not along it)
SIT_ARC = 0.26                      # (his fists' ways to the grips through a point this far before his shoulder,
SIT_MID = {"l": None, "r": (0.06, 0.04)}  #  this far out to its side and this far under it, lifted off his legs at first;
                                          #  None: straight there)
NOT_BODY = ("weapon", "bolt", "bipod", "whip_1", "whip_2", "whip_3", "whip_4")


def sit_seat_x(man):
    """Where his hips sit (x in his root's frame): SIT_BACK behind his hips as kneel_down leaves him."""
    lq, mv = kneel_base(man)
    return float(man.fk(lq, mv)[1][man.b("pelvis")][0]) - SIT_BACK


def blend_pose(man, A, Bp, w, bones=None):
    """Two poses (lq, mv) blended, each bone's turn and move w of the way from A to B (only `bones` if given)."""
    lq, mv = A[0].copy(), A[1].copy()
    idx = range(man.nb) if bones is None else bones
    for i in idx:
        lq[i] = rot_slerp(A[0][i], Bp[0][i], w)
        mv[i] = A[1][i] * (1 - w) + Bp[1][i] * w
    return lq, mv


def world_blend(Wa, Wb, w, lift=0.0):
    """Two world turns and heads blended (the head lifted `lift` at the middle)."""
    return rot_slerp(Wa[0], Wb[0], w), Wa[1] * (1 - w) + Wb[1] * w + Z_ * lift * math.sin(math.pi * w)


def make_sit_down(man):
    """sit_down: from kneel_down's last pose to hmg_fire_sit's first key (sit_up plays it backward). His back and hips
    blended from the kneel to sitting up and then to the sit; his legs by two-bone IK, each foot on its own way above
    the ground; his arms by Man.arm on the rifle while he holds it, then fist_to on arched ways to the gun's grips."""
    memo = man.__dict__.setdefault("_sit_memo", {})
    key = ("sit_down", SIT_EYE, SIT_LEAN, SIT_FEET, SIT_KNEE, SIT_ELBOW, SIT_RIFLE, SIT_BACK, SIT_SEAT, SIT_ARC,
           repr(SIT_MID), SIT_PORT_AT, SIT_LIFT, SIT_PORT)
    if key in memo:
        return memo[key]
    sit = make_fire_sit(man)
    Bend = sit["fn"](0.0)
    # (the pose the game plays first in hmg_fire_sit: its kit kept on his body and a radio man's whip held as there)
    Bplay = keep_kit(man, steady_whip(man, sit["fn"]), sit.get("kit_ref"))(0.0)
    S = sit["sockets_at"](0.0)
    K = man.D["clips"]["kneel_down"]
    A = man.locals_of(K, K["long"])
    A2 = kneel_base(man)
    lqU, mvU, _ = sit_base(man, seat_x=sit_seat_x(man), bend=SIT_UP_BEND)
    U = (lqU, mvU)
    names = man.rig.names
    limb = ("upperarm_", "lowerarm_", "hand_", "fingers_", "thumb_", "thigh_", "calf_", "foot_", "ball_")
    body = [i for i, n in enumerate(names) if n not in NOT_BODY and not n.startswith(limb)]
    arm_b = {s: [man.b(n + s) for n in limb[:5]] for s in "lr"}
    leg_b = {s: [man.b(n + s) for n in limb[5:]] for s in "lr"}
    has_rifle = len(man.weapon_pts) > 0
    gun_b = [man.b(n) for n in ("weapon", "bolt", "bipod") if n in man.ix]
    wi, ch = man.b("weapon"), man.b("chest")
    GA, XA = man.fk(*A)
    GK, XK = man.fk(*A2)
    GB, XB = man.fk(*Bend)
    B = BODY(man)
    foot_m = {s: B.zone["feet"] & np.isin(B.a, leg_b[s]) for s in "lr"}
    low0 = {s: float(B.skin(GA, XA, foot_m[s])[:, 2].min()) for s in "lr"}   # (each foot's lowest as the kneel ends)
    W0 = (GA[wi].copy(), XA[wi].copy())
    W1 = sit["laid"]
    rel = {s: (W0[0].T @ GA[man.b("hand_" + s)], W0[0].T @ (XA[man.b("hand_" + s)] - W0[1])) for s in "lr"}
    el0 = {s: GA[ch].T @ (XA[man.b("lowerarm_" + s)] - XA[man.b("upperarm_" + s)]) for s in "lr"}
    elB = {s: GB[ch].T @ (XB[man.b("lowerarm_" + s)] - XB[man.b("upperarm_" + s)]) for s in "lr"}
    let = {"l": SIT_BODY[1], "r": SIT_LAY[1]} if has_rifle else {"l": SIT_FREE[0], "r": SIT_FREE[1]}
    cache = {}

    def trunk_at(t):
        """His back, hips and head: the kneel, sitting up, then bent over the gun."""
        Ak = blend_pose(man, A, A2, sm(t / SIT_FOOT))
        P = blend_pose(man, Ak, U, sm((t - SIT_BODY[0]) / (SIT_BODY[1] - SIT_BODY[0])), body)
        if t > SIT_BEND[0]:
            P = blend_pose(man, P, Bend, sm((t - SIT_BEND[0]) / (SIT_BEND[1] - SIT_BEND[0])), body)
        if has_rifle and SIT_LAY[0] < t < SIT_LAY[1] + 0.4:
            u = min(1.0, (t - SIT_LAY[0]) / (SIT_LAY[1] + 0.4 - SIT_LAY[0]))
            P = (trunk(man, P[0].copy(), lean=-SIT_LAY_LEAN * math.sin(math.pi * u)), P[1])
        return P

    def legs_at(lq, mv, t):
        """Each foot from where it kneels to where it sits, lifted and swung out on the way (IK), the knee's way
        turned between; before its way starts, the kneel's own leg (the right foot laid onto its instep)."""
        for s, sg in (("l", -1.0), ("r", 1.0)):
            t0, t1, up, out = SIT_LEGS[s]
            w = sm((t - t0) / (t1 - t0))
            fo, ca, th = man.b("foot_" + s), man.b("calf_" + s), man.b("thigh_" + s)
            Ak = blend_pose(man, A, A2, sm(t / SIT_FOOT), leg_b[s])
            k = sm((t - SIT_BODY[0]) / 0.15)           # (from the kneel's own leg into the IK's, its foot planted)
            if k <= 0.0:
                for i in leg_b[s]:
                    lq[i] = Ak[0][i]
                # (the foot turning on its ankle would push its toes deeper than the kneel has them: the ankle lifted
                # by that much instead, the foot kept as turned)
                G, X = man.fk(lq, mv)
                dz = low0[s] - float(B.skin(G, X, foot_m[s])[:, 2].min())
                if dz > 0.0:
                    lq = leg(man, lq, mv, s, X[fo] + Z_ * dz, unit(X[ca] - 0.5 * (X[th] + X[fo])), G[fo].copy())
                continue
            ank = XK[fo] * (1 - w) + XB[fo] * w + Z_ * up * math.sin(math.pi * w) + Y_ * sg * out * math.sin(math.pi * w)
            mid0, mid1 = 0.5 * (XK[th] + XK[fo]), 0.5 * (XB[th] + XB[fo])
            pole = unit(vslerp(unit(XK[ca] - mid0), unit(XB[ca] - mid1), w) +
                        np.array((0.0, sg * SIT_KNEE_OUT[s], 0.3)) * math.sin(math.pi * w))   # (the knee turned out
                                                                                     #  on the way)
            if s + "knee" in prev and w < 1.0:
                pole = vslerp(prev[s + "knee"], pole, max(0.5, w ** 4))       # (and kept turning one way)
            l2 = leg(man, lq.copy(), mv, s, ank, pole, rot_slerp(GK[fo], GB[fo], w))
            G2, X2 = man.fk(l2, mv)
            prev[s + "knee"] = unit(X2[ca] - 0.5 * (X2[th] + X2[fo]))
            l2[man.b("ball_" + s)] = rot_slerp(A2[0][man.b("ball_" + s)], Bend[0][man.b("ball_" + s)], w)
            for i in leg_b[s]:
                lq[i] = rot_slerp(Ak[0][i], l2[i], k)
        return lq

    R_port = frame_map(X_, Y_, unit(np.array(SIT_PORT)), -X_)

    def rifle_at(t):
        if t <= SIT_LAY[0]:
            # (riding his chest, turned up to port before him as he sits: its muzzle up, clear of the gun)
            lq, mv = trunk_at(t)
            G, X = man.fk(lq, mv)
            Rr = G[ch] @ GA[ch].T @ W0[0]
            u = sm((t - SIT_BODY[0]) / (SIT_LAY[0] - SIT_BODY[0]))
            # (first out from his face, then down and turned up: the hand at his cheek moves away from it, not along it)
            uo = np.array((sm(t / 0.25), sm(t / 0.45), sm((t - 0.1) / 0.45)))
            return rot_slerp(Rr, R_port, u), X[ch] + G[ch] @ (GA[ch].T @ (W0[1] - XA[ch])) + np.array(SIT_PORT_AT) * uo
        u = sm((t - SIT_LAY[0]) / (SIT_LAY[1] - SIT_LAY[0]))
        R_, p_ = world_blend(rifle_at(SIT_LAY[0]), W1, u, SIT_LIFT[0])
        return R_, p_ + Y_ * SIT_LIFT[1] * math.sin(math.pi * u)

    def free_arm(lq, mv, s, t, held):
        """From where the hand let go (held: its grip point, hand way, thumb way, elbow way in his chest's frame) to
        the gun's grip, on an arched way, by fist_to; on the grip as hmg_fire_sit has it at the end."""
        side = -1.0 if s == "l" else 1.0
        w = sm((t - let[s]) / (SIT_BEND[1] - let[s]))
        g0, d0, t0, e0, f0 = held
        G, X = man.fk(lq, mv)
        sh = X[man.b("upperarm_" + s)]                 # (up before his chest, over his knee, then out to the grip)
        g1 = S["grip.gunner." + s]
        if SIT_MID[s] is None:                         # (straight there: the hand let go before him, by the grip)
            pc = 0.5 * (g0 + g1)
        else:
            mid = sh + np.array((SIT_ARC, side * SIT_MID[s][0], -SIT_MID[s][1]))
            pc = 2.0 * mid - 0.5 * (g0 + g1)
        at = (1 - w) ** 2 * g0 + 2 * (1 - w) * w * pc + w * w * g1 + Z_ * 0.12 * math.sin(math.pi * min(w / 0.6, 1.0))
        d = unit(d0 * (1 - w) + unit(np.array((1.0, -side * 0.2, -0.15))) * w)
        tt = unit(t0 * (1 - w) + unit(np.array((0.1, 0.0, 1.0))) * w)
        pole = vslerp(unit(prev.get(s + "elbow", G[ch] @ e0)), unit(np.array((-0.2, side * SIT_ELBOW, -1.0))),
                      w)                               # (his elbow kept on its way, turned to the sit's by the end)
        lq = fist_to(man, lq, mv, s, at, d, tt, pole, share=0.6, curl=CURL, thumb=THUMB)
        for i, q0 in f0.items():                       # (the fingers closing as the hand lifts away, not at once)
            lq[i] = rot_slerp(q0, lq[i], sm(w / 0.35))
        return lq

    hand0 = {s: GA[ch].T @ (XA[man.b("hand_" + s)] - XA[ch]) for s in "lr"}
    sh0 = {s: GA[ch].T @ (XA[man.b("upperarm_" + s)] - XA[ch]) for s in "lr"}
    turn0 = {s: GA[ch].T @ GA[man.b("hand_" + s)] for s in "lr"}

    def rides_on(s):
        """No rifle: the leg bone his hand rests on as kneel_down leaves him (the one nearest his fist), and the
        hand's head and turn in its frame: the hand rides it (neither sinking into the leg nor floating off it as the
        leg moves) till it leaves."""
        g = man.grip(GA, XA, s)
        segs = [(bn, XA[man.b(bn)], XA[man.b(nx)]) for bn, nx in (("thigh_l", "calf_l"), ("calf_l", "foot_l"),
                                                                  ("thigh_r", "calf_r"), ("calf_r", "foot_r"))]
        bn = min(segs, key=lambda q: float(seg_dist(g[None, :], q[1], q[2])[0]))[0]
        i, h = man.b(bn), man.b("hand_" + s)
        return i, GA[i].T @ (XA[h] - XA[i]), GA[i].T @ GA[h]
    ride = {s: rides_on(s) for s in "lr"} if not has_rifle else {}

    def ready_arm(lq, mv, s, t):
        """No rifle: the hand from where kneel_down leaves it (riding the leg it rests on) to a fist before his belly
        (in his chest's frame, so it rides his trunk), the fingers closing on the way; a hand in SIT_ON_KNEE stays on
        its knee, fingers as kneel_down has them, till it lets go (free_arm closes them as it lifts away)."""
        side = -1.0 if s == "l" else 1.0
        u = sm((t - SIT_BODY[0]) / (SIT_READY[0] - SIT_BODY[0]))
        G, X = man.fk(lq, mv)
        A_ = man.side[s]
        off = np.array(SIT_READY[1]) * np.array((1.0, side, 1.0))
        rb, rp, rR = ride[s]
        p_leg, R_leg = X[rb] + G[rb] @ rp, G[rb] @ rR
        uf = u                                         # (the fingers' closing)
        if s in SIT_ON_KNEE:                           # (his hand kept on his knee as kneel_down leaves it there, the
            p, Rh, uf = p_leg, R_leg, 0.0              #  elbow turned out over the thigh)
        else:
            uu = np.array((u, u * u, u))               # (up and forward first, in last: clear of his thighs)
            p_end = X[ch] + G[ch] @ (sh0[s] + off)
            p = p_leg * (1 - uu) + p_end * uu + G[ch] @ (Z_ * SIT_READY[2] * math.sin(math.pi * u))
            R1 = frame_map(A_["d0"], A_["t0"], unit((1.0, -side * 0.3, -0.3)), unit((0.0, -side * 0.3, 1.0)))
            Rh = rot_slerp(R_leg, G[ch] @ R1, u)
        pw = SIT_KNEE_POLE if s in SIT_ON_KNEE else SIT_READY[3]
        pole = vslerp(unit(prev.get(s + "elbow", G[ch] @ el0[s])), G[ch] @ unit(np.array(pw) *
                                                                                  np.array((1.0, side, 1.0))), u)
        lq = man.arm(lq, mv, s, p, pole, Rh @ A_["d0"], Rh @ A_["t0"], share=0.6)
        F = man.fingers(lq.copy(), s, CURL, THUMB)
        for nm_ in ("fingers_", "thumb_"):
            i = man.b(nm_ + s)
            lq[i] = rot_slerp(A[0][i], F[i], uf)
        return lq

    def held_at(s):
        """A hand as it lets go: its grip point, hand and thumb ways, the elbow's way in the chest's frame."""
        lq, mv = fn(let[s])
        G, X = man.fk(lq, mv)
        Gh = G[man.b("hand_" + s)]
        A_ = man.side[s]
        return (man.grip(G, X, s), Gh @ A_["d0"], Gh @ A_["t0"],
                G[ch].T @ (X[man.b("lowerarm_" + s)] - X[man.b("upperarm_" + s)]),
                {i: lq[i].copy() for i in (man.b("fingers_" + s), man.b("thumb_" + s))})

    helds = {}

    def fn(t):
        t = min(max(t, 0.0), SIT_LONG)
        k = round(t, 4)
        if k in cache:
            return cache[k][0].copy(), cache[k][1].copy()
        if t <= 1e-6:
            lq, mv = A[0].copy(), A[1].copy()
        elif t >= SIT_LONG - 1e-6:
            lq, mv = Bend[0].copy(), Bend[1].copy()
        else:
            lq, mv = trunk_at(t)
            lq = legs_at(lq, mv, t)
            for s in "lr":
                for i in arm_b[s]:
                    lq[i] = A[0][i]
            Wt = rifle_at(t) if has_rifle else None
            for s in "lr":
                if t <= let[s]:
                    if has_rifle:
                        Rr, pr = rel[s]
                        Gh, ph = Wt[0] @ Rr, Wt[1] + Wt[0] @ pr
                        A_ = man.side[s]
                        G, X = man.fk(lq, mv)
                        pole = prev.get(s + "elbow", G[ch] @ el0[s])     # (his elbow kept on its way: no flip)
                        lq = man.arm(lq, mv, s, ph, pole, Gh @ A_["d0"], Gh @ A_["t0"], share=0.6)
                        for nm_ in ("fingers_", "thumb_"):
                            lq[man.b(nm_ + s)] = A[0][man.b(nm_ + s)]
                    else:                      # (no rifle: riding the leg it rests on, then up before him)
                        lq = ready_arm(lq, mv, s, t)
                else:
                    if s not in helds:
                        helds[s] = held_at(s)
                    lq = free_arm(lq, mv, s, t, helds[s])
            if has_rifle:
                lq, mv = put_weapon(man, lq, mv, Wt)
            else:                                      # (no rifle: the hidden weapon bones from the kneel's to the sit's)
                P = blend_pose(man, A, Bend, sm(t / SIT_LONG), gun_b)
                for i in gun_b:
                    lq[i], mv[i] = P[0][i], P[1][i]
        G, X = man.fk(lq, mv)
        for s in "lr":
            prev[s + "elbow"] = unit(X[man.b("lowerarm_" + s)] - X[man.b("upperarm_" + s)])
        cache[k] = (lq.copy(), mv.copy())
        return lq, mv

    prev = {}
    for t in times_of(SIT_LONG):                       # (made key by key in order: each elbow and knee keeps its way)
        fn(t)
    play = fn
    if "whip_1" in man.ix:
        # (a radio man's whip: turned evenly in the world from the way kneel_down leaves it to the way hmg_fire_sit
        # holds it (steady_whip), its sections straightened on the way, so neither chain pops it)
        wi = man.b("whip_1")
        wr = [man.b(n) for n in WHIP[1:]]
        Wa, Wb = GA[wi].copy(), man.fk(*Bplay)[0][wi].copy()

        def play(t):
            lq, mv = fn(t)
            lq = lq.copy()
            u = sm(min(max(t, 0.0), SIT_LONG) / SIT_LONG)
            G, _ = man.fk(lq, mv)
            lq[wi] = G[man.par[wi]].T @ rot_slerp(Wa, Wb, u)
            for i in wr:
                lq[i] = rot_slerp(A[0][i], I3, u)
            return lq, mv

    T1 = SIT_BODY[1]
    sp = {"long": SIT_LONG, "loop": False, "fn": play, "weapon": "rifle", "whip_done": True,
          "holds": [{"hand": s, "socket": "grip.gunner." + s, "from": SIT_LONG, "to": SIT_LONG} for s in "lr"],
          "rifle_held": [(s, 0.0, let[s]) for s in "lr"] if has_rifle else [],
          "chain": {"start": A, "end": Bplay}, "sockets_at": sit["sockets_at"], "prop": sit["prop"],
          "kit_ref": kit_ref(man, "kneel_down"), "kit_from": "kneel_down (its last pose)",
          "events": [{"t": round(0.5 * (SIT_BODY[0] + T1), 4), "fx": "body_fall", "socket": None, "cue": "body_fall",
                      "bone": "pelvis"}] +
                    ([{"t": round(SIT_LAY[1], 4), "fx": "kit_rattle", "socket": None, "cue": "kit_rattle",
                       "bone": "weapon"}] if has_rifle else []),
          "from": "crew_clips.py: keyed on the rig, from this file's kneel_down (motion capture, its last pose) to "
                  "hmg_fire_sit's first key: back and hips blended bone by bone, the legs by two-bone IK, the arms by "
                  "Man.arm on the rifle and to the grips",
          "about": ("The seated MG No. 1 sitting down behind the gun from a kneel (chain kneel_down, sit_down, "
                    "hmg_fire_sit, sit_up, stand_up; one-shot): on kneel_down's last pose at 0 s, his right foot "
                    f"rolls onto its instep, he sits back onto the ground and brings his legs forward and wide by "
                    f"{T1:.2f} s" + (f", his rifle held to his chest, then lays it down by his right side by "
                                     f"{SIT_LAY[1]:.2f} s (the weapon bone keyed every key, from his hands to the "
                                     "ground)" if has_rifle else
                                     f", no rifle: his left hand riding his left knee till {SIT_FREE[0]:.2f} s, his "
                                     f"right fist brought up before his belly by {SIT_READY[0]:.2f} s") +
                    f", and bends over the gun, his fists onto its grips, ending on hmg_fire_sit's first key at "
                    f"{SIT_LONG} s. `prop` is the gun as hmg_fire_sit places it. " +
                    rifle_says(man, "Weapon rifle: in his hands at the start, laid at the end. "))}
    memo[key] = sp
    return sp


def make_sit_up(man):
    """sit_up: sit_down played backward, from hmg_fire_sit's first key to kneel_down's last pose."""
    d = make_sit_down(man)
    L = d["long"]
    has_rifle = len(man.weapon_pts) > 0
    T1, T2 = SIT_BODY[1], SIT_LAY[1]
    return dict(d, fn=lambda t: d["fn"](L - t), chain={"start": d["chain"]["end"], "end": d["chain"]["start"]},
                holds=[{"hand": s, "socket": "grip.gunner." + s, "from": 0.0, "to": 0.0} for s in "lr"],
                rifle_held=[(s, L - b, L - a) for s, a, b in d.get("rifle_held", [])],
                events=[{"t": round(L - T2, 4), "fx": "kit_rattle", "socket": None, "cue": "kit_rattle",
                         "bone": "weapon"}] if has_rifle else
                [{"t": round(L - 0.5 * (SIT_BODY[0] + T1), 4), "fx": "kit_rattle", "socket": None,
                  "cue": "kit_rattle", "bone": "chest"}],
                weapon="laid",
                about=("The seated MG No. 1 getting up from behind the gun to a kneel (chain hmg_fire_sit, sit_up, "
                       "stand_up; one-shot): sit_down played backward: on hmg_fire_sit's first key at 0 s, he lets go "
                       "of the grips and sits up" +
                       (f", picks up his rifle from by his right side ({L - T2:.2f} s) and holds it to his chest"
                        if has_rifle else "") +
                       f", draws his legs in and comes up onto his left foot and right knee by {L - T1:.2f} s, "
                       f"ending on kneel_down's last pose at {L} s (his right foot back onto its toes, as kneel_down "
                       "leaves it). `prop` is the gun as hmg_fire_sit places it. " +
                       rifle_says(man, "Weapon laid at the start, in his hands at the end. ")))


# ---------------------------------------------------------------------------------------- hmg_feed
FEED_LONG = 2.0
FEED_TURN = -20.0                   # (the gun turned to his left: he lies at its left, angled out from it)
FEED_AHEAD, FEED_RIGHT = 0.32, 0.12  # (the feed this far ahead of his eyes and to his right)
FEED_LIFT = 0.10                    # (his left fist lifts the belt this much over its way from the box to the feed,
FEED_ELBOW = -0.6                   #  his left elbow out to his left this much for each down: clear of his face)


def make_feed(man):
    B = BODY(man)
    lq0, mv0 = man.locals_of(man.D["clips"]["fire_prone"], 0.0)
    lq0 = trunk(man, lq0, turn=14.0, chain=(("spine_02", 0.5), ("chest", 0.5)))
    lq0, mv0 = settle_prone(man, lq0, mv0, toward=unit((1.0, 0.5, -0.1)), eye_min=0.30)
    if getattr(man, "kit", False) and len(B.Pk):
        # (lying on the part of his kit on his chest (the radio man's handset): his body raised off the ground as
        # far as the part, pushed up KIT_LIFT into his coat, still goes into it (at most 1.5 cm; hips and chest
        # stay within 3 cm of the ground))
        R_, p_ = kit_ref(man, "fire_prone", lambda t: 0.0)(0.0)
        lk, mk = put_kit(man, lq0.copy(), mv0.copy(), R_, p_)
        need = KIT_CLEAR - KIT_LIFT - float(B.skin_kit(*man.fk(lk, mk))[:, 2].min())
        if need > 0.0:
            mv0[man.b("pelvis")] = mv0[man.b("pelvis")] + Z_ * min(need + 0.001, 0.015)
    G, X = man.fk(lq0, mv0)
    R = yaw(FEED_TURN)
    fl = np.asarray(HMG_LOW["sockets"]["feed"], float)
    # (the feed ahead of his eyes and a little to his right, where he looks down at it and his right fist reaches it)
    e = eye_at(man, G, X)
    fwd = X[man.b("head")] - X[man.b("pelvis")]
    fwd = unit(np.array((fwd[0], fwd[1], 0.0)))
    rgt = np.array((-fwd[1], fwd[0], 0.0))
    feed = e + fwd * FEED_AHEAD + rgt * FEED_RIGHT
    feed[2] = fl[2]
    root = feed - R @ fl
    root[2] = 0.0
    S = placed(HMG_LOW, root, R)
    front = root + R @ np.array((25.0, 0.0, 0.4))
    feed = S["feed"]
    left = unit(X[man.b("upperarm_l")] - X[man.b("upperarm_r")])
    W = weapon_world(man, *lay_rifle(man, lq0.copy(), mv0.copy(), 0.5 * (X[man.b("upperarm_l")] + X[man.b("thigh_l")]) + left * 0.32,
                                     X[man.b("head")] - X[man.b("pelvis")]))
    box_top = S["belt_box"] + np.array((0.0, 0.0, 0.19))

    def left_fist(t):
        # (the belt lifted off the box toward the feed and let down again: up 0-0.9 s, held, down 1.3-2.0 s)
        u = ramp(t, 0.05, 0.9) * (1.0 - ramp(t, 1.3, 1.95))
        lo = box_top + 0.45 * (feed - box_top) + np.array((-0.03, -0.02, 0.0))
        hi = box_top + 0.62 * (feed - box_top) + np.array((-0.06, -0.03, FEED_LIFT))
        return lo * (1 - u) + hi * u

    def fn(t):
        lq, mv = lq0.copy(), mv0.copy()
        lq = breathe(man, lq, t, FEED_LONG)
        g = ramp(t, 0.90, 1.25) * (1.0 - ramp(t, 1.42, 1.77))     # (the glance: no faster than 13 degrees a key)
        lq = look(man, lq, mv, 0.5 * (feed + left_fist(t)), neck=0.45)
        lq2 = look(man, lq.copy(), mv, front, neck=0.45)
        for b in ("neck", "head"):
            lq[man.b(b)] = rot_slerp(lq[man.b(b)], lq2[man.b(b)], g)
        guide = 0.006 * math.sin(2 * math.pi * 2 * t / FEED_LONG)
        at = feed + (R @ X_) * guide
        lq = fist_to(man, lq, mv, "r", at, (0.6, 0.75, -0.2), (0.3, 0.0, 1.0), (0.0, 0.6, -1.0), curl=70.0)
        lq = fist_to(man, lq, mv, "l", left_fist(t), (0.75, 0.55, -0.3), (0.3, 0.2, 1.0), (0.0, FEED_ELBOW, -1.0),
                     curl=75.0)
        lq = clamp_head(man, lq)
        return put_weapon(man, lq, mv, W)

    holds = [{"hand": "r", "socket": "feed", "from": 0.0, "to": FEED_LONG}]
    return {"long": FEED_LONG, "fn": fn, "weapon": "laid", "events": [], "holds": holds,
            "prop": prop_block(HMG_LOW, root, R), "sockets_at": lambda t: S,
            "kit_ref": kit_ref(man, "fire_prone", lambda t: 0.0), "kit_from": "fire_prone",
            "from": "crew_clips.py: this file's fire_prone (motion capture, its first pose), set on the ground; the "
                    "arms keyed to the gun's feed and the belt (Man.arm), the head keyed",
            "about": ("The No. 2 of the heavy machine gun, lying at its left (the same stand-in gun as hmg_fire_prone, "
                      f"turned {-FEED_TURN:.0f} degrees to his left: his head by it, his feet away from it, clear of the "
                      "No. 1 beside him), feeding: his right fist at the prop's `feed` every key (guiding the belt in, "
                      "within 6 mm), his left fist lifting the belt off the open box (`belt_box`, on the ground at the "
                      "gun's left) and letting it down, his eyes on the feed, a glance to the front at 0.9-1.8 s. Draw "
                      "the belt from the box's top through his left fist (`hands` l) into the feed. No events. " +
                      rifle_says(man, "His rifle lies on its side on the ground by his left side (weapon laid). ") +
                      "Chain from fire_prone or go_prone before (a blend); to fire_prone or crawl after (a blend). "
                      "Play beside hmg_fire_prone: place both men by the one prop (each clip's prop.root and turn_deg "
                      "give where the gun stands in that man's root frame); the two clips run on their own clocks.")}


# ---------------------------------------------------------------------------------------- the carries
SLOW, STEP = 1.15, 0.85             # (a loaded walk: a slower beat, shorter steps; the three carriers keep step)


def chest_ride(man, lq, mv, R0, p0):
    """A prop riding his chest: its turn and root now, from its turn and root at rest (R0, p0, the rest frame's)."""
    G, X = man.fk(lq, mv)
    ch = man.b("chest")
    return G[ch] @ R0, X[ch] + G[ch] @ (p0 - man.H[ch])


def shoulder_top(man, s):
    """The top of his shoulder over the joint, at rest: the highest of his points within 7 cm across of the arm's
    head (inside the arm's joint, over it)."""
    B = BODY(man)
    S = man.H[man.b("upperarm_" + s)]
    m = (np.abs(B.P[:, 1] - S[1]) < 0.045) & (np.abs(B.P[:, 0] - S[0]) < 0.06) & ~B.zone["head"]
    return np.array((S[0], S[1], float(B.P[m, 2].max())))


CARRY_CHAIN = ("A loaded walk: the file's walk slowed, steps shortened; the three carriers (hmg_carry_gun, _tripod, "
               "_boxes) keep step on one clock (same length, same stride, the walk's footstep events at its times x "
               "1.15). Chain from idle or walk before and to idle or walk after (a blend; the load is taken up and put "
               "down off these clips: show it with the clip). ")


def make_carry_gun(man):
    walk, L = walk_phase(man, SLOW, STEP)
    B = BODY(man)
    # (the gun on his right shoulder at rest: its `rest` socket 5 mm over the shoulder's top, a little inside it, the
    # barrel forward and up CARRY_PITCH, the gun turned in CARRY_YAW; both refitted for a file whose kit is in the way)
    top = shoulder_top(man, "r") + np.array((0.0, -CARRY_IN, 0.005))
    R0 = yaw(CARRY_YAW) @ R_axis(Y_, CARRY_PITCH)
    p0 = top - R0 @ np.asarray(GUN["sockets"]["rest"], float)

    def fn(t, keep=None):
        lq, mv, sw = walk(t, bend=(2.0, 2.0, 2.0, 2.0, 3.0))
        lq = trunk(man, lq, lean=-2.0, chain=(("spine_02", 0.5), ("chest", 0.5)))
        i = man.b("clavicle_r")
        lq[i] = lq[i] @ R_axis(X_, -3.0)
        Rg, pg = chest_ride(man, lq, mv, R0, p0)
        car = pg + Rg @ np.asarray(GUN["sockets"]["carry"], float)
        lq = fist_to(man, lq, mv, "r", car, Rg @ unit((0.15, -0.35, 1.0)), Rg @ X_, (0.1, 0.9, -1.0), share=0.6)
        lq = free_arm(man, lq, mv, "l", -sw["l"], out=CARRY_ARM_OUT)
        lq = look(man, lq, mv, None, neck=0.5, roll=4.0, toward=(1.0, 0.0, -0.08))
        lq, mv = settle_sling(man, lq, mv, left=True)
        if keep is not None:
            keep.append((t, pg, Rg))
        return lq, mv

    def sock(t):
        lq, mv = fn(t)
        Rg, pg = chest_ride(man, lq, mv, R0, p0)
        return placed(GUN, pg, Rg)

    holds = [{"hand": "r", "socket": "carry", "from": 0.0, "to": L}]
    ride = {"bone": "chest", "at": [round(float(v), 4) for v in (p0 - man.H[man.b("chest")])],
            "quat": [round(float(v), 5) for v in quat(R0)]}
    return {"long": L, "fn": fn, "weapon": "slung", "walk": True, "holds": holds, "carried": (GUN, R0, p0),
            "kit_ref": walk_kit(man), "kit_from": "walk",
            "sockets_at": sock, "ride": ride,
            "from": "crew_clips.py: this file's walk (motion capture), slowed and shortened, the right arm keyed to "
                    "the gun on his shoulder, the left arm keyed swinging",
            "about": ("The No. 1 carrying the gun off its tripod: the gun over his right shoulder (its `rest` socket on "
                      f"the shoulder's top, barrel forward and {-CARRY_PITCH:.0f} degrees up, turned in "
                      f"{-CARRY_YAW:.0f}), his right fist forward on its `carry` socket "
                      "every key, his left arm swinging in step. " +
                      rifle_says(man, "His rifle slung across his back, flat on it (muzzle by his LEFT shoulder here). ") +
                      CARRY_CHAIN + "The gun rides his chest bone rigidly: "
                      "`prop.rides` gives its root and turn in the chest's frame (at = the root less the chest's rest "
                      "head, rest frame), `prop.track` its root and turn every key in the man's root frame.")}


def tripod_held():
    """The carried tripod with its fists' sockets GRIP_ALONG out along its front legs (a fit may move them out)."""
    g = {"grip.carry.l": (0.02 + GRIP_ALONG * 0.68, -(0.06 + GRIP_ALONG * 0.14), 0.0),
         "grip.carry.r": (0.02 + GRIP_ALONG * 0.68, 0.06 + GRIP_ALONG * 0.14, 0.0)}
    return dict(TRIPOD, sockets=dict(TRIPOD["sockets"], **g))


def make_carry_tripod(man):
    walk, L = walk_phase(man, SLOW, STEP)
    TRI = tripod_held()
    # (the folded tripod over his shoulders at rest: its head TRIPOD_BACK (6 cm) behind the base of his neck, its front legs forward
    # over both shoulders, rising TRIPOD_PITCH (6) degrees)
    B = BODY(man)
    tl, tr = shoulder_top(man, "l"), shoulder_top(man, "r")
    mid = 0.5 * (tl + tr)
    R0 = R_axis(Y_, -TRIPOD_PITCH)
    # (the legs just over the shoulders' tops where they cross them)
    legs = [(np.asarray(TRI["sockets"]["grip.carry.l"], float), tl), (np.asarray(TRI["sockets"]["grip.carry.r"], float), tr)]
    head = np.array((man.back_x - TRIPOD_BACK, 0.0, mid[2] + 0.04))
    for _ in range(3):
        best = None
        for dz in np.arange(-0.05, 0.12, 0.004):
            p0 = head + np.array((0.0, 0.0, dz))
            P = p0 + (R0 @ np.array(((0.02, -0.06, 0.0), (0.70, -0.20, 0.0), (0.02, 0.06, 0.0), (0.70, 0.20, 0.0))).T).T
            # (each front leg's height over its shoulder's top where it passes over it)
            gap = []
            for (a, b_), top in (((P[0], P[1]), tl), ((P[2], P[3]), tr)):
                u = (top[0] - a[0]) / (b_[0] - a[0])
                zz = a[2] + u * (b_[2] - a[2])
                gap.append(zz - 0.016 - top[2])
            g = min(gap)
            if best is None or abs(g - TRIPOD_GAP) < best[0]:
                best = (abs(g - TRIPOD_GAP), p0)
        head = best[1]
    p0 = head

    def fn(t, keep=None):
        lq, mv, sw = walk(t, bend=(2.0, 2.0, 2.0, 4.0, 5.0))
        for s, sg in (("l", 1.0), ("r", -1.0)):
            i = man.b("clavicle_" + s)
            lq[i] = lq[i] @ R_axis(X_, sg * 2.0)
        Rg, pg = chest_ride(man, lq, mv, R0, p0)
        S = placed(TRI, pg, Rg)
        for s, side in (("l", -1.0), ("r", 1.0)):
            leg_dir = Rg @ unit(np.array((0.68, side * 0.14, 0.0)))
            lq = fist_to(man, lq, mv, s, S["grip.carry." + s], Rg @ unit((0.1, -side * 0.9, 0.35)),
                         leg_dir, (0.0, side * 0.9, -1.0), share=0.6)
        lq = look(man, lq, mv, None, neck=0.5, toward=(1.0, 0.0, -0.06))
        lq, mv = settle_sling(man, lq, mv, way=TRIPOD_SLING)
        return lq, mv

    def sock(t):
        lq, mv = fn(t)
        Rg, pg = chest_ride(man, lq, mv, R0, p0)
        return placed(TRI, pg, Rg)

    holds = [{"hand": "l", "socket": "grip.carry.l", "from": 0.0, "to": L},
             {"hand": "r", "socket": "grip.carry.r", "from": 0.0, "to": L}]
    ride = {"bone": "chest", "at": [round(float(v), 4) for v in (p0 - man.H[man.b("chest")])],
            "quat": [round(float(v), 5) for v in quat(R0)]}
    return {"long": L, "fn": fn, "weapon": "slung", "walk": True, "holds": holds, "carried": (TRI, R0, p0),
            "kit_ref": walk_kit(man), "kit_from": "walk",
            "sockets_at": sock, "ride": ride,
            "from": "crew_clips.py: this file's walk (motion capture), slowed and shortened, both arms keyed up to "
                    "the tripod's legs",
            "about": ("The No. 2 carrying the gun's tripod folded: its head behind his neck, its two front legs forward "
                      "over his shoulders, its rear leg down his back (over his slung rifle), both fists up on the front "
                      "legs (grip.carry.l / grip.carry.r) every key. " +
                      rifle_says(man, "His rifle slung flat across his back, under the tripod's rear leg" +
                                 (" (laid further across and lower than his walk has it, its muzzle under the tripod's "
                                  "head)" if TRIPOD_SLING else "") + ". ") +
                      CARRY_CHAIN + "The tripod rides his chest bone rigidly (`prop.rides`; `prop.track` every "
                      "key).")}


BOX_SWING = 9.0                     # (degrees each box swings fore and aft at most)
BOX_OUT = 0.33                      # (its handle this far out from his middle)
CARRY_ARM_OUT = 0.15                # (hmg_carry_gun: his free left arm swung this far out from his side)
CARRY_PITCH = -12.0                 # (hmg_carry_gun: the barrel this much up from level (negative up), turned in
CARRY_YAW = -3.0                    #  this much, its `rest` socket this far in from his shoulder's top)
CARRY_IN = 0.025
TRIPOD_BACK = 0.06                  # (hmg_carry_tripod: its head this far behind the base of his neck, its front
TRIPOD_GAP = 0.012                  #  legs this far over his shoulders' tops where they cross them)
TRIPOD_PITCH = 6.0                  # (its front legs rising this much)
TRIPOD_SLING = None                 # (his rifle slung (tilt, drop): further across and lower than Man.sling, its muzzle
                                    #  under the tripod's head; None: as his walk has it)


def make_carry_boxes(man):
    walk, L = walk_phase(man, SLOW, STEP)
    hb = np.asarray(BOX["sockets"]["handle"], float)

    def boxes(t, lq, mv, sw):
        out = {}
        G, X = man.fk(lq, mv)
        for s, side in (("l", -1.0), ("r", 1.0)):
            ph = -sw[s]
            lag = -walk(t - 0.12 * L, bend=(4.0, 4.0, 4.0, 3.0, 5.0))[2][s]
            S_ = X[man.b("upperarm_" + s)]
            A = man.side[s]
            reach = 0.955 * (A["l1"] + A["l2"])
            fx = S_[0] + 0.05 + 0.06 * ph
            fy = side * BOX_OUT
            d = unit((0.12, side * 0.1, -1.0))
            tt = np.array((1.0, 0.0, 0.0))
            o = man.wrist_for(s, np.zeros(3), d, tt)
            dx, dy = fx - S_[0] + o[0], fy - S_[1] + o[1]
            drop = math.sqrt(max(reach * reach - dx * dx - dy * dy, 0.04))
            fz = S_[2] - drop - o[2]
            fist = np.array((fx, fy, fz))
            Rb = R_axis(Y_, -BOX_SWING * lag) @ R_axis(X_, side * 3.0)
            out[s] = (fist, Rb, fist - Rb @ hb, d, tt)
        return out

    def fn(t, keep=None):
        lq, mv, sw = walk(t, bend=(4.0, 4.0, 4.0, 3.0, 5.0))
        bx = boxes(t, lq, mv, sw)
        for s, side in (("l", -1.0), ("r", 1.0)):
            fist, Rb, root, d, tt = bx[s]
            lq = fist_to(man, lq, mv, s, fist, d, tt, (-1.0, side * 0.3, 0.0), share=0.5)
        lq = look(man, lq, mv, None, neck=0.5, toward=(1.0, 0.0, -0.1))
        lq, mv = settle_sling(man, lq, mv)
        return lq, mv

    def sock(t):
        lq, mv, sw = walk(t, bend=(4.0, 4.0, 4.0, 3.0, 5.0))
        bx = boxes(t, lq, mv, sw)
        return {"handle.l": bx["l"][0], "handle.r": bx["r"][0]}

    def poses(t):
        lq, mv, sw = walk(t, bend=(4.0, 4.0, 4.0, 3.0, 5.0))
        bx = boxes(t, lq, mv, sw)
        return {s: (bx[s][2], bx[s][1]) for s in ("l", "r")}

    holds = [{"hand": "l", "socket": "handle.l", "from": 0.0, "to": L},
             {"hand": "r", "socket": "handle.r", "from": 0.0, "to": L}]
    return {"long": L, "fn": fn, "weapon": "slung", "walk": True, "holds": holds, "boxes": poses,
            "kit_ref": walk_kit(man), "kit_from": "walk",
            "sockets_at": sock,
            "from": "crew_clips.py: this file's walk (motion capture), slowed and shortened, a little bent forward, "
                    "both arms keyed down to the boxes' handles (the stretcher bearers' arms, field_clips.py)",
            "about": ("An ammunition bearer: an MG ammunition box hanging from each fist by its handle, his arms down "
                      "and nearly straight, a little bent forward, the boxes swinging fore and aft with the step (up to "
                      f"{BOX_SWING:.0f} degrees, a little behind the arms). `prop` is one box (its root under its middle, "
                      "`handle` where the fist closes); `prop.track_l` and `prop.track_r` give each box's root and turn "
                      "every key in the man's root frame (the handle is in his fist every key). " +
                      rifle_says(man, "His rifle slung flat across his back. ") + CARRY_CHAIN)}


# ---------------------------------------------------------------------------------------- mortar_load
LOAD_LONG = 3.3
T_GRAB = 0.80                       # (his fists close on the round on its case)
T_UP = 1.78                         # (the round's tail over the muzzle)
DROP_T = 1.98                       # (slid in 6 cm and let go)
DUCK_T = 0.55                       # (he ducks in one move over this long from the drop,
FIRE_T = DROP_T + 0.58              #  and it fires as he is down (the round's fall down the tube),
HANDS_AFTER = 0.36                  #  his fists back on their rests this long after the shot;
DUCK_WAIT = 0.07                    #  (his head and back still this long after the drop, his hands first off the round)
DUCK_HOLD = 0.12                    #  he stays down this long after the shot, then rises back by LOAD_LONG)
DUCK_UNDER = 0.06                   # (ducked, the top of his head this far under the muzzle)
DUCK_CLEAR = 0.25                   # (and no part of his head nearer the muzzle than this;
DUCK_SPARE = 0.015                  #  the bend found with this to spare, his arms and breath moving him a little)
DUCK_LOOK = (25.0, 20.0)            # (ducked, his face turned this far left of his chest's way and down: degrees)
MUZZLE_AHEAD = 0.58                 # (the muzzle this far before his chest, along the way his chest faces)
FIRE_OFF = 25.0                     # (it fires 115 degrees right of his chest's way: he kneels a little behind the
                                    #  muzzle, the base plate before his left knee)
CASE_AT = (0.26, 0.64)              # (the case's middle from his chest: ahead, to his left: beside his left knee)
CASE_TURN = -30.0                   # (its long side turned 30 degrees left of his chest's way)
CASE_SIZE = (0.40, 0.22, 0.32)      # (an ammunition chest standing on its end)
LIFT_UP = 0.26                      # (the round first lifted up off the case this far, and out before him)
LIFT_OUT = 0.12
LIFT_IN = 0.02                      # (then brought over to the muzzle from this far on his side of it,
LIFT_LOW = 0.30                     #  and this far under it)
LOAD_REST = (8.0, 0.0, 0.0)         # (his back between rounds: bend, turn, lean)
LOAD_DUCK = (14.0, -28.0, 28.0)     # (ducked after the drop: bent forward, turned to his left and leaning to his left,
                                    #  away from the tube; the bend from this on, as deep as DUCK_UNDER needs)
REST_R = (0.06, 0.08, 0.85)         # (his right hand hanging by his right thigh (the knee down), the arm nearly
                                    #  straight: its grip point this far forward of his right shoulder, this far out
                                    #  to his right of it, and this share of the arm's length under it)
REST_UP = (0.11, 0.04, 0.04, 0.15, 0.16)    # (his left fist resting this far over his left knee as he sits up;
                                    #  bent down over it, this far over the knee, this far before it and this far out
                                    #  to his left of it (his head comes down over the knee): from as his left shoulder
                                    #  comes down to as it is this far down)
HANDS_PULL = (0.10, 0.08)           # (his hands' ways down from the muzzle, left and right: around his shoulders,
EARLY = 0.85                        # (the left fist's bow and dip done by this share of its way)
ARC_L = 0.05                        # (its bow off the tube toward him: his face is that way)
BOW_R = (0.20, 0.45, 0.50)          # (the right fist taken out past the muzzle by `arc` over this share of its way,
                                    #  then let back in from this share over this share)
HANDS_DIP = (0.22, 0.0)             #  drawn in this much at the middle so his elbows never fold through, and dipped
                                    #  this much, under his face as he ducks)
GRAB_POLE = (-0.2, 0.9, -0.3)       # (his right elbow's pole at the grab in his chest's frame (back, out, down): up and
                                    #  out, so his forearm comes down onto the round over his raised left knee;
GRAB_POLES = ((-0.2, 0.9, -0.6), (-0.4, 0.8, -0.8))  #  higher still, tried by the fitter for a thick-thighed man)
REACH_PEAK = 1.5                    # (their arch highest this far along: w ** REACH_PEAK = 0.5)
REACH_ARC = (0.08, 0.10)            # (his fists' paths down to the round arched up this much: left, right,
REACH_OUT = (0.04, 0.08)            #  and out before him this much)
REACH_WIDE = ((0.08, 0.13), (0.04, 0.16))   # (a wider way for the right fist, tried by the fitter: up and out over
                                            #  his raised left knee where a thick-thighed man's forearm would graze it)
APPROACHES = ((0.09, 0.8, 1.0),)    # (further out, tried by the fitter: a man whose fingers would clip the round's fuse)
APPROACH = (0.06, 0.8, 1.0)         # (they come to the round's side this far out from its holds, closing in on them over
                                    #  the last part of their way from this much of it; from above as much as from the
                                    #  side)
REST_BULGE = 0.025                  # (his left fist's way between the top and the front of his knee bowed out this much)
LOAD_FLOW = False                   # (his back's keys run through (flowed), not held over the grab, instead of eased
                                    #  to a stop at each: for a file whose slung weapon's far end then swings too fast)
LOAD_LEVEL = 0.4                    # (his head keeps 60 percent of his chest's sideways tip)
HEAD_MAX = 76.0                     # (his head kept within this of his chest's way)
REACH_EASE = 0.25                   # (his fists' and right elbow's way down to the round: speeding up and slowing over
                                    #  this share of it each end, even between (ease_flat); less for a man whose elbow
                                    #  would step more than 15 cm a key at the middle (a thick coat's elbow swung wide))
FISTS_UNDER = 0.30                  # (at the shot his fists at least this far under the muzzle's top)
LOAD_FIT = None                     # (make_mortar_load's choices when already made: from crew_checks.json's fits)
I3 = np.eye(3)


def vslerp(u0, u1, w):
    """The unit way w of the way from u0 to u1 (both unit), turning evenly."""
    c = min(1.0, max(-1.0, float(u0 @ u1)))
    a = math.acos(c)
    if a < 1e-6:
        return unit(u0 * (1 - w) + u1 * w)
    return (math.sin((1 - w) * a) * u0 + math.sin(w * a) * u1) / math.sin(a)


def facing(G, man, bone="chest"):
    v = G[man.b(bone)][:, 0]
    return unit(np.array((v[0], v[1], 0.0)))


def legs_of(man, lq, mv):
    """Where his legs are planted: each ankle, foot turn and knee (for replant)."""
    G, X = man.fk(lq, mv)
    return {s: (X[man.b("foot_" + s)].copy(), G[man.b("foot_" + s)].copy(), X[man.b("calf_" + s)].copy())
            for s in ("l", "r")}


def replant(man, lq, mv, planted):
    """His legs set back on their planted ankles, feet and (the way of) knees after his pelvis moved."""
    for s in ("l", "r"):
        at, Gf, kn = planted[s]
        G, X = man.fk(lq, mv)
        hip = X[man.b("thigh_" + s)]
        pole = kn - 0.5 * (hip + at)
        lq = leg(man, lq, mv, s, at, pole, Gf)
    return lq


def pelvis_pitch(man, lq, mv, deg, drop=0.0):
    """His pelvis tipped forward `deg` (hips bending) about its own head, let down `drop` m."""
    pe = man.b("pelvis")
    lq = lq.copy()
    mv = mv.copy()
    G, X = man.fk(lq, mv)
    lq[pe] = R_axis(np.cross(Z_, facing(G, man, "pelvis")), deg) @ lq[pe]
    mv[pe] = mv[pe] - np.array((0.0, 0.0, drop))
    return lq, mv


def reach_trunk(man, lq0, mv0, fists, grid, chain=SPINE, look_at=None, head_ok=50.0):
    """The back's (bend, turn, lean) from `grid` that brings his wrists within reach of the fists' places
    {side: (at, d, t)}, turns his chest so his eyes see `look_at` within `head_ok` degrees of its way (and least
    bent)."""
    best = None
    for bend in grid[0]:
        for tn in grid[1]:
            for lean in grid[2]:
                l2 = trunk(man, lq0.copy(), bend=bend, turn=tn, lean=lean, chain=chain)
                G, X = man.fk(l2, mv0)
                cost = 1e-6 * (bend * bend + tn * tn + lean * lean)
                for s, (at, d, t) in fists.items():
                    A = man.side[s]
                    W = man.wrist_for(s, at, unit(d), unit(t))
                    over = np.linalg.norm(W - X[man.b("upperarm_" + s)]) - 0.93 * (A["l1"] + A["l2"])
                    cost += max(over, 0.0) ** 2 * 100.0 + max(-over - 0.25, 0.0) ** 2
                if look_at is not None:
                    Gc = G[man.b("chest")]
                    v = unit(np.asarray(look_at, float) - eye_at(man, G, X))
                    a = math.degrees(math.acos(min(1.0, max(-1.0, float(v @ Gc[:, 0])))))
                    cost += 1e-3 * max(a - head_ok, 0.0) ** 2
                if best is None or cost < best[0]:
                    best = (cost, (float(bend), float(tn), float(lean)))
    return best[1]


def arm_pole(man, G, s, down=1.0, out=0.7, back=0.3):
    """An elbow's pole in his chest's frame: down, out to his side and back."""
    Gc = G[man.b("chest")]
    sg = -1.0 if s == "l" else 1.0
    return Gc @ np.array((-back, sg * out, 0.0)) + np.array((0.0, 0.0, -down))


def clamp_head(man, lq, limit=HEAD_MAX):
    """His neck and head eased back toward the chest's way until the head is at most `limit` degrees from it."""
    ne, he = man.b("neck"), man.b("head")
    qn, qh = lq[ne].copy(), lq[he].copy()
    if angle_of(qn @ qh) <= limit:
        return lq
    lo, hi = 0.0, 1.0
    for _ in range(18):
        f = 0.5 * (lo + hi)
        if angle_of(rot_slerp(I3, qn, f) @ rot_slerp(I3, qh, f)) > limit:
            hi = f
        else:
            lo = f
    lq[ne], lq[he] = rot_slerp(I3, qn, lo), rot_slerp(I3, qh, lo)
    return lq


def blend_poles(keys, t, L, man, G, s):
    """An elbow's pole eased through keys [(t, (back, out, down))] in his chest's frame."""
    b, o, d = eased([(k, v) for k, v in keys], t, L)
    return arm_pole(man, G, s, down=d, out=o, back=b)


def make_mortar_load(man):
    """mortar_load fitted to his reach (the files' kneels differ), step by step on the keys each choice moves: the
    muzzle brought in and the mortar turned further right (its base plate off his left foot) until both fists reach
    the round at the muzzle; the case set further ahead of his left knee and the round's way up taken further out
    before him until it clears his head, arms and legs; his right hand's hang moved forward and out, and his fists'
    ways down to the round arched further up and out, until his hands and forearms stay clear of his body; his fists'
    ways down from the muzzle taken further out past it and under his face until they clear the tube and his head.
    Each step keeps the first choice that passes (else the least bad); the choices are kept in the clip's `fit`."""
    if LOAD_FIT:
        sp = _mortar_load(man, **{k: (tuple(v) if isinstance(v, list) else v) for k, v in LOAD_FIT.items()})
        sp["fit_load"] = dict(LOAD_FIT)
        return sp
    B = BODY(man)
    T = list(times_of(LOAD_LONG))
    lift = [t for t in T if T_GRAB <= t <= DROP_T + 0.1]       # (and just after he lets go, his hands still up)
    reach_up = [t for t in T if T_GRAB - 0.35 <= t <= T_UP]
    before = [t for t in T if t <= T_GRAB + 0.1]
    after = [t for t in T if t >= DROP_T - 0.1]
    p = {"ahead": MUZZLE_AHEAD, "fire_off": FIRE_OFF, "rest_r": REST_R, "arc": 0.12, "lift_out": LIFT_OUT,
         "reach": 1.0, "case_pos": CASE_AT, "dip": HANDS_DIP, "way": None, "grab_pole": GRAB_POLE}
    best_all = {}

    def pick(options, keys):
        best = None
        for o in options:
            sp = _mortar_load(man, **dict(p, **o))
            sc = fit_score(man, B, sp, keys)
            if best is None or sc < best[0]:
                best = (sc, o, sp)
            if sc == 0.0:
                break
        p.update(best[1])
        best_all["sp"] = best[2]

    pick([{"ahead": a, "fire_off": f} for a in (MUZZLE_AHEAD, MUZZLE_AHEAD - 0.03, MUZZLE_AHEAD - 0.06,
                                                 MUZZLE_AHEAD - 0.09) for f in (FIRE_OFF, FIRE_OFF + 10.0)], lift)
    c0, c1 = CASE_AT
    # (the case judged on his reach down to it too: his right forearm comes down past his raised left thigh, its
    # elbow out and up as far as each of the poles tried (the further the case, the further his elbow swings: a
    # thick-coated man's 17 cm a key to the far case with the low pole, 14.6 to the near one with the high))
    pick([{"case_pos": c, "grab_pole": g}
          for c in (CASE_AT, (c0 - 0.06, c1 - 0.04), (c0 + 0.08, c1 - 0.08), (c0 + 0.16, c1 - 0.14),
                    (c0 - 0.10, c1 + 0.04), (c0 + 0.08, c1 - 0.04), (c0 + 0.04, c1 + 0.08), (c0 + 0.14, c1 - 0.08))
          for g in (GRAB_POLE,) + GRAB_POLES], before + reach_up)
    pick([{"lift_out": v} for v in (LIFT_OUT, LIFT_OUT + 0.06, LIFT_OUT + 0.12)], reach_up)
    f0, g0, o0 = REST_R
    pick([{"rest_r": r} for r in (REST_R, (f0, g0 + 0.04, o0), (f0 + 0.06, g0 + 0.02, o0), (f0, g0 + 0.04, o0 - 0.07),
                                  (f0 + 0.06, g0 + 0.06, o0 - 0.07))], before + after)
    pick([{"reach": 1.0}, {"reach": 1.0, "way": REACH_WIDE}, {"reach": 1.3}, {"reach": 1.7}] +
         [{"reach": r, "grab_pole": g} for g in (GRAB_POLE,) + GRAB_POLES for r in (1.0, 1.3)] +
         [{"reach": 1.0, "approach": a, "grab_pole": g} for a in APPROACHES for g in (GRAB_POLE,) + GRAB_POLES], before)
    d0 = HANDS_DIP[0]
    pick([{"arc": a, "dip": (d, 0.0)} for a in (0.12, 0.08, 0.16) for d in (d0, d0 - 0.06, d0 + 0.06)], after)
    sp = best_all["sp"]
    sp["fit_load"] = {k: (list(v) if isinstance(v, tuple) else v) for k, v in p.items()}
    return sp


def fit_score(man, B, sp, ts):
    """How far a mortar_load try breaks the checks on keys ts: fists off the round, a hand or a forearm in his body
    past its allowance, his body in the mortar, the case or the round, a bone's step between keys over 14.5 cm (a
    radio man's whip left out: it is steadied after), his head's turn between keys over HEAD_STEP (0 when clean)."""
    keep = np.array([n not in WHIP for n in man.rig.names])
    prevX, prevG, prevt, jump, hstep = None, None, None, 0.0, 0.0
    pr = sp["prop"]
    Rm, rt = yaw(pr["turn_deg"]), np.asarray(pr["root"], float)
    ca, cr = sp["case"]
    fixed = ([(rt + Rm @ cc, half, Rm @ Rb) for _, (cc, half, Rb) in MORTAR["shapes"]] +
             [(ca + cr @ cc, half, cr @ Rb) for _, (cc, half, Rb) in CASE["shapes"]])
    lim_arm, lim_hand = allowed(man)
    far = dth = inprop = 0.0
    he = man.b("head")
    for t in ts:
        lq, mv = sp["fn"](t)
        G, X = man.fk(lq, mv)
        if prevX is not None and abs(t - prevt - 1.0 / RATE) < 1e-6:
            jump = max(jump, float(np.linalg.norm(X[keep] - prevX[keep], axis=1).max()))
            hstep = max(hstep, angle_of(prevG[he].T @ G[he]))
        prevX, prevG, prevt = X, G, t
        S = sp["sockets_at"](t)
        if T_GRAB <= t <= DROP_T:
            far = max(far, max(float(np.linalg.norm(man.grip(G, X, s) - S["round.hold." + s])) for s in "lr"))
        Q = B.skin(G, X)
        dth = max(dth, B.through(G, X, Q, lim=lim_hand)[2], B.through(G, X, Q, probe="fore", lim=lim_arm)[2])
        p, Rr, shown = sp["round"](t)
        shapes = fixed + ([(p + Rr @ cc, half, Rr @ Rb) for _, (cc, half, Rb) in ROUND["shapes"]] if shown else [])
        # (his hands too, but while they close on the round and let it go (HOLD_GRACE either side), as the checks)
        Qn = Q[~B.zone["hands"]] if T_GRAB - HOLD_GRACE[0] <= t <= DROP_T + HOLD_GRACE[1] else Q
        inprop = max([inprop] + [float(inside_box(Qn, cc, half, Rb).max()) for cc, half, Rb in shapes])
    return (10.0 * max(far - 0.008, 0.0) + dth + max(inprop - 0.008, 0.0) + max(jump - 0.145, 0.0) +
            0.003 * max(hstep - HEAD_STEP, 0.0))


def head_set(man, lq, mv, Gd, share=0.45):
    """His neck and head turned so the head's world turn is Gd, the neck taking `share` of the head's turn from his
    chest (one split for a given turn, so two ways to the same turn key the same pose)."""
    ch, ne, he = man.b("chest"), man.b("neck"), man.b("head")
    G, X = man.fk(lq, mv)
    rel = G[ch].T @ Gd
    lq[ne] = qpow(rel, share)
    lq[he] = lq[ne].T @ rel
    return lq


def head_of(man, lq, mv):
    return man.fk(lq, mv)[0][man.b("head")].copy()


def _mortar_load(man, ahead=MUZZLE_AHEAD, rest_r=REST_R, fire_off=FIRE_OFF, arc=0.12, lift_out=LIFT_OUT,
                 rest_bend=LOAD_REST[0], reach=1.0, case_pos=CASE_AT, grab_pole=GRAB_POLE, pull=HANDS_PULL,
                 dip=HANDS_DIP, way=None, approach=APPROACH):
    reach_arc, reach_out = way if way else (REACH_ARC, REACH_OUT)
    lq0, mv0 = kneel_square(man)
    G, X = man.fk(lq0, mv0)
    face = facing(G, man, "chest")
    left = np.array((face[1], -face[0], 0.0))
    fyaw = math.degrees(math.atan2(face[1], face[0]))
    R = yaw(fyaw + 90.0 + fire_off)                     # (it fires to his right: he faces the tube from its right)
    ch = X[man.b("chest")]
    chxy = np.array((ch[0], ch[1], 0.0))
    muz_local = np.asarray(MORTAR["sockets"]["muzzle"], float)
    muz = chxy + face * ahead
    muz[2] = muz_local[2]
    root = muz - R @ muz_local
    root[2] = 0.0
    S = placed(MORTAR, root, R)
    u = R @ _U                                         # (the tube's axis in his frame)
    yp = R @ Y_                                        # (the prop's right: toward him)
    xp = R @ X_                                        # (the way it fires, level)
    # (the rounds' case standing at his left beside his left knee)
    case_at = chxy + face * case_pos[0] + left * case_pos[1]
    case_R = yaw(fyaw + CASE_TURN)
    # (the round stands on its tail on the case, its hold side toward him; it turns to the tube's line on the way up,
    # the hold side still toward him)
    Rg0 = frame_map(X_, Y_, Z_, unit(chxy - case_at))
    rp_ground = case_at + np.array((0.0, 0.0, CASE_SIZE[2]))
    Rtube = frame_map(X_, Y_, u, yp)
    above = S["muzzle"] + u * 0.03

    inn = S["muzzle"] - u * 0.06
    front = S["muzzle"] - u * 0.40                     # (where he looks between rounds: the tube, at its middle)

    # (the lift: straight up off the case, then over to the muzzle from his side of it and a little under it, the
    # round turning to the tube's line on the way; eased by the length its hold travels, so it never hurries)
    P1 = rp_ground + np.array((0.0, 0.0, LIFT_UP)) + face * lift_out
    P2 = above + yp * LIFT_IN - Z_ * LIFT_LOW
    hold_mid = 0.5 * (np.asarray(ROUND["sockets"]["hold.l"], float) + np.asarray(ROUND["sockets"]["hold.r"], float))

    def lift(w):
        p = (1 - w) ** 3 * rp_ground + 3 * (1 - w) ** 2 * w * P1 + 3 * (1 - w) * w * w * P2 + w ** 3 * above
        return p, rot_slerp(Rg0, Rtube, sm(min(1.0, max(0.0, (w - 0.2) / 0.65))))

    ws = np.linspace(0.0, 1.0, 241)
    hm = np.array([p + Rr @ hold_mid for p, Rr in (lift(w) for w in ws)])
    run = np.concatenate(([0.0], np.cumsum(np.linalg.norm(np.diff(hm, axis=0), axis=1))))
    run /= run[-1]

    def round_at(t):
        """The round's tail and turn, and whether it is shown."""
        if t < T_GRAB:
            return rp_ground, Rg0, True
        if t < T_UP:
            p, Rr = lift(float(np.interp(sm((t - T_GRAB) / (T_UP - T_GRAB)), run, ws)))
            return p, Rr, True
        if t < DROP_T:
            w = sm((t - T_UP) / (DROP_T - T_UP))
            return above * (1 - w) + inn * w, Rtube, True
        return inn - u * 0.4 * sm(min(1.0, (t - DROP_T) / 0.1)), Rtube, False

    def holds_at(t):
        p, Rr, _ = round_at(min(t, DROP_T))
        return {k: p + Rr @ np.asarray(ROUND["sockets"][k], float) for k in ("hold.l", "hold.r")}, Rr

    def hand_way(Rr):
        return -(Rr @ unit((0.0, 0.7, 0.7))), Rr @ X_

    def fists_at(t):
        hs, Rr = holds_at(t)
        d, ax = hand_way(Rr)
        return {s: (hs["hold." + s], d, ax) for s in ("l", "r")}

    def seen(t):
        hs, _ = holds_at(t)
        return 0.5 * (hs["hold.l"] + hs["hold.r"])

    # (his back at the pick-up, half way up, at the muzzle and at the drop: found so both fists reach the round, his
    # head can see it, and least bent)
    wide = (np.arange(0, 61, 4.0), np.arange(-60, 21, 5.0), np.arange(-10, 31, 5.0))
    near = (np.arange(-6, 31, 3.0), np.arange(-30, 31, 4.0), np.arange(-12, 13, 4.0))
    memo = man.__dict__.setdefault("_load_memo", {})

    def solve(name, t, grid, key):
        k = (name, key)
        if k not in memo:
            memo[k] = reach_trunk(man, lq0, mv0, fists_at(t), grid, look_at=seen(t))
        return memo[k]

    at_case = tuple(round(v, 4) for v in case_pos)
    at_muzzle = (round(ahead, 4), round(fire_off, 3), round(lift_out, 3)) + at_case
    grab = solve("grab", T_GRAB, wide, at_case)
    half = solve("half", 0.5 * (T_GRAB + T_UP), wide, at_muzzle)
    up = solve("up", T_UP, near, at_muzzle)
    drop = solve("drop", DROP_T, near, at_muzzle)

    # (his fists' rests: the left on his raised left knee, the right on the front of his right thigh (the knee down),
    # `rest_r` = (the share of the way up from the knee to the hip, the fist's grip point this far off the thigh's
    # skin, and this far out to his right); the right hand hangs over the thigh, fingers down, thumb in)
    kneeL = X[man.b("calf_l")] + np.array((0.0, 0.0, REST_UP[0])) - face * 0.02 + left * 0.02
    kneeF = X[man.b("calf_l")] + np.array((0.0, 0.0, REST_UP[1])) + face * REST_UP[2] + left * REST_UP[4]
    armR = man.side["r"]["l1"] + man.side["r"]["l2"] + 0.075
    shL0 = float(X[man.b("upperarm_l")][2])

    def rest_of(G, X, s):
        """His fists' rests: the left on his raised left knee (his legs are planted), on its top as he sits up and over
        its front as he bends down onto it (by how far his left shoulder is down); the right hanging by his right
        thigh under his right shoulder (moving with his back as he bends)."""
        if s == "l":
            w = sm((shL0 - float(X[man.b("upperarm_l")][2])) / REST_UP[3])
            return kneeL * (1 - w) + kneeF * w + unit(face + Z_) * REST_BULGE * math.sin(math.pi * w)
        f = facing(G, man, "chest")
        lf = np.array((f[1], -f[0], 0.0))
        at = X[man.b("upperarm_r")] + f * rest_r[0] - lf * rest_r[1] - Z_ * (rest_r[2] * armR)
        at[2] = max(at[2], 0.14)
        return at

    def way_of(G, s):
        if s == "l":
            return unit(face * 0.9 + np.array((0.0, 0.0, -0.45))), unit(Z_ - left * 0.5)
        f = facing(G, man, "chest")
        d = unit(f * 0.15 - Z_)
        return d, unit(f - d * (f @ d))

    # (the duck: how far he bends to get the top of his head DUCK_UNDER under the muzzle, turned away to his left)
    mz = float(S["muzzle"][2])

    def duck_head(lq):
        G1, X1 = man.fk(lq, mv0)
        f = facing(G1, man, "chest")
        way = R_axis(Z_, -DUCK_LOOK[0]) @ f + np.array((0.0, 0.0, -math.tan(math.radians(DUCK_LOOK[1]))))
        lq = look(man, lq, mv0, None, neck=0.5, roll=6.0, toward=way, level=LOAD_LEVEL)
        return clamp_head(man, lq)

    dkey = ("duck",) + tuple(round(float(v), 4) for v in S["muzzle"])
    dk = memo.get(dkey)
    if dk is None:
        B = BODY(man)
        for bend in np.arange(LOAD_DUCK[0], 66.0, 2.0):
            l2 = duck_head(trunk(man, lq0.copy(), bend=bend, turn=LOAD_DUCK[1], lean=LOAD_DUCK[2]))
            G2, X2 = man.fk(l2, mv0)
            Qh = B.skin(G2, X2, B.zone["head"])
            if (float(Qh[:, 2].max()) <= mz - DUCK_UNDER - DUCK_SPARE and
                    float(np.linalg.norm(Qh - S["muzzle"], axis=1).min()) >= DUCK_CLEAR + DUCK_SPARE):
                break
        dk = memo[dkey] = (float(bend), LOAD_DUCK[1], LOAD_DUCK[2])
    duck = dk
    duck2 = (dk[0] + 2.0, dk[1] - 2.0, dk[2])
    T_DUCK, T_RISE = DROP_T + DUCK_T, FIRE_T + DUCK_HOLD
    T_BACK = FIRE_T + HANDS_AFTER                      # (his hands back on his knee and thigh)
    TR = [(0.0, (rest_bend, 0.0, 0.0)), (T_GRAB - 0.1, grab), (T_GRAB + 0.06, grab),
          (0.5 * (T_GRAB + T_UP), (half[0] - 8.0, half[1], half[2])), (T_UP, up),
          (DROP_T, drop), (DROP_T + DUCK_WAIT, drop), (T_DUCK, duck), (T_RISE, duck2)]
    TR_FLOW = [k for k in TR if abs(k[0] - (T_GRAB + 0.06)) > 1e-6]   # (flowed: his back on its way up at the grab)
    # (the elbows' poles in his chest's frame (back, out, down): out and back while his hands rest and reach down to
    # his left, the right elbow forward across him; down and out under the round at the muzzle)
    T_MID = 0.5 * (T_GRAB + T_UP)
    PL = [(0.0, (0.4, 1.0, 0.3)), (T_GRAB - 0.1, (0.8, 1.0, -0.2)), (T_GRAB + 0.06, (0.8, 1.0, -0.2)),
          (T_MID, (0.3, 0.9, 1.0)), (T_UP, (0.2, 0.7, 1.0)), (DROP_T + 0.04, (0.2, 0.7, 1.0)),
          (DROP_T + 0.3, (0.3, 0.9, 0.8)), (T_BACK, (0.4, 1.0, 0.3))]
    PR = [(0.0, (0.6, 0.6, 1.0)), (T_GRAB + 0.06, grab_pole),          # (his right elbow out and up over the whole
                                                                       #  reach: no faster than his fist)
          (T_MID, (0.0, 0.8, 1.0)), (T_UP, (0.2, 0.7, 1.0)), (DROP_T + 0.04, (0.2, 0.7, 1.0)),
          (DROP_T + 0.3, (0.4, 0.8, 0.8)), (T_BACK, (0.6, 0.6, 1.0))]

    def body(t):
        """His back and arms at t (the head not yet turned)."""
        lq, mv = lq0.copy(), mv0.copy()
        bend, tn, lean = flowed(TR_FLOW, t, LOAD_LONG) if LOAD_FLOW else eased(TR, t, LOAD_LONG)
        lq = trunk(man, lq, bend=bend, turn=tn, lean=lean)
        lq = breathe(man, lq, t, LOAD_LONG)
        G, X = man.fk(lq, mv)
        hs, Rr = holds_at(t)
        on = ease_flat((t - 0.0) / (T_GRAB - 0.02), REACH_EASE)   # (hands off his legs and down to the round: an even pace, its
                                                       #  top speed a third over its mean, not half again)
        x = min(1.0, max(0.0, (t - DROP_T - 0.02) / (T_BACK - DROP_T - 0.02)))
        off = 0.5 * x + 0.5 * sm(x)                    # (let go, hands down to his knee and thigh: on their way at once)
        across, ax = hand_way(Rr)
        for s in ("l", "r"):
            rest = rest_of(G, X, s)
            dr, tr_ = way_of(G, s)
            if t < DROP_T:
                hold = hs["hold." + s]
                w = on
                arch = math.sin(math.pi * w ** REACH_PEAK)    # (highest past the middle: down onto the round from
                lift = reach * reach_arc[0 if s == "l" else 1] * arch     # over it, not across his raised knee)
                at = rest * (1 - w) + hold * w + np.array((0.0, 0.0, lift))
                at = at + face * reach * reach_out[0 if s == "l" else 1] * arch
                # (to the round's side first, its hold this far out from it, then in onto it: not through it)
                close = sm((w - approach[1]) / (1.0 - approach[1]))
                at = at + unit(Rr @ unit((0.0, 1.0, 1.0)) + Z_ * approach[2]) * approach[0] * min(1.0, w / 0.5) * \
                    (1.0 - close)
                d = unit(dr * (1 - w) + across * w)
                tt = unit(tr_ * (1 - w) + ax * w)
            else:
                # (down from the muzzle around his shoulder: its way from the shoulder turned evenly from the drop to
                # the rest, its reach drawn in a little at the middle; bowed toward him, off the tube)
                drop_at = holds_at(DROP_T)[0]["hold." + s]
                w = off
                sh = X[man.b("upperarm_" + s)]
                a0, a1 = drop_at - sh, rest - sh
                r0, r1 = float(np.linalg.norm(a0)), float(np.linalg.norm(a1))
                k = 0 if s == "l" else 1
                early = math.sin(math.pi * min(w / EARLY, 1.0))      # (under his face early)
                if k:
                    # (the right fist, on the round's top over the muzzle with the tube leaning out under it: out past
                    # the muzzle the way it fires first, then down clear of the tube's end)
                    bow = xp * arc * sm(w / BOW_R[0]) * (1.0 - sm((w - BOW_R[1]) / BOW_R[2]))
                else:
                    bow = yp * ARC_L * early
                reach_w = r0 * (1 - w) + r1 * w - pull[k] * math.sin(math.pi * w)
                at = sh + vslerp(a0 / r0, a1 / r1, w) * reach_w + bow - Z_ * dip[k] * early
                wd = sm(min(1.0, 2.0 * w))                    # (his hands turned off the tube at once)
                d = unit(across * (1 - wd) + dr * wd)
                tt = unit(ax * (1 - wd) + tr_ * wd)
            grip = sm((t - (T_GRAB - 0.1)) / 0.12) * (1 - sm((t - DROP_T) / 0.08))
            if s == "r" and t < PR[1][0]:
                # (his right elbow's pole turned evenly from its rest to its grab, at his fist's pace: the two eased
                # straight between swung it fastest past the middle, where that blend is shortest)
                bk, ou, dn = vslerp(unit(PR[0][1]), unit(PR[1][1]), ease_flat(t / PR[1][0], REACH_EASE))
                pole = arm_pole(man, G, s, down=dn, out=ou, back=bk)
            else:
                pole = blend_poles(PL if s == "l" else PR, t, LOAD_LONG, man, G, s)
            lq = fist_to(man, lq, mv, s, at, d, tt, pole, share=0.6, curl=30.0 + 40.0 * grip,
                         thumb=20.0 + 20.0 * grip)
        return lq, mv

    def look_before(t, lq, mv):
        """The head before the drop: from the tube down to the round, up with it to the muzzle."""
        w = ramp(t, 0.0, 0.60)
        w2 = ramp(t, T_GRAB + 0.2 * (T_UP - T_GRAB), T_UP - 0.05)    # (up to the muzzle ahead of his hands)
        tgt = front * (1 - w) + (seen(t) * (1 - w2) + S["muzzle"] * w2) * w
        return clamp_head(man, look(man, lq, mv, tgt, neck=0.45, level=LOAD_LEVEL))

    heads = {}

    def head_world(key):
        """The head's world turn at the drop (on the muzzle), ducked (away to his left and down) and at rest (t 0)."""
        if key not in heads:
            if key == "drop":
                lq, mv = body(DROP_T)
                lq = look_before(DROP_T, lq, mv)
            elif key == "duck":
                lq, mv = body(T_DUCK)
                lq = duck_head(lq)
            else:
                lq, mv = body(0.0)
                lq = look_before(0.0, lq, mv)
            heads[key] = head_of(man, lq, mv)
        return heads[key]

    track = []

    def head_before(t):
        """The head's world turn before the drop: look_before's on each key, but turning at most HEAD_EASE a key
        (held back where it would turn faster, as the round rises past his face and his back straightens, then caught
        up; eased back from the drop's own the same way), so it does not snap."""
        if not track:
            ks = [k / RATE for k in range(int(math.ceil(DROP_T * RATE - 1e-9)))] + [DROP_T]
            H = []
            for tk in ks:
                lq, mv = body(tk)
                H.append(head_of(man, look_before(tk, lq.copy(), mv), mv))
            end = H[-1]
            for back in (False, True):
                if back:
                    H[-1] = end                         # (the drop's own: the head held on the muzzle from there)
                for i in (range(len(ks) - 2, -1, -1) if back else range(1, len(ks))):
                    j = i + 1 if back else i - 1
                    a, lim = angle_of(H[j].T @ H[i]), HEAD_EASE * abs(ks[i] - ks[j]) * RATE
                    if a > lim:
                        H[i] = rot_slerp(H[j], H[i], lim / a)
            track.extend((ks, H))
        ks, H = track
        i = min(int(np.searchsorted(ks, t, side="right")) - 1, len(ks) - 2)
        return rot_slerp(H[i], H[i + 1], min(1.0, max(0.0, (t - ks[i]) / (ks[i + 1] - ks[i]))))

    def fn(t):
        lq, mv = body(t)
        if t < DROP_T:
            Gd = head_before(t)
        elif t <= DROP_T + DUCK_WAIT:
            Gd = head_world("drop")
        elif t <= T_DUCK:
            # (after the drop the head turns one way only: from the muzzle, in one even turn over DUCK_T, away to
            # his left and down as he ducks; held through the shot; back to the tube as he rises)
            x = (t - DROP_T - DUCK_WAIT) / (DUCK_T - DUCK_WAIT)
            Gd = rot_slerp(head_world("drop"), head_world("duck"), 0.4 * x + 0.6 * sm(x))
        elif t <= T_RISE:
            Gd = head_world("duck")
        else:
            Gd = rot_slerp(head_world("duck"), head_world("rest"), sm((t - T_RISE) / (LOAD_LONG - T_RISE)))
        lq = clamp_head(man, head_set(man, lq, mv, Gd))
        return settle_sling(man, lq, mv)

    def sock(t):
        hs, _ = holds_at(t)
        return dict(S, **{"round." + k: v for k, v in hs.items()})

    def duck_check(c):
        """At the shot: how far the top of his head is under the muzzle and from it, his fists from their rests."""
        B = BODY(man)
        G1, X1 = gore.pose(man.rig, c, FIRE_T)
        Qh = B.skin(G1, X1, B.zone["head"])
        under = mz - float(Qh[:, 2].max())
        dist = float(np.linalg.norm(Qh - S["muzzle"], axis=1).min())
        fists = max(float(np.linalg.norm(man.grip(G1, X1, s) - rest_of(G1, X1, s))) for s in "lr")
        low = min(mz - float(man.grip(G1, X1, s)[2]) for s in "lr")            # (his fists under the muzzle's top
        off = min(float(np.linalg.norm(man.grip(G1, X1, s) - S["muzzle"])) for s in "lr")   # and off it, at the shot)
        G2, X2 = gore.pose(man.rig, c, T_BACK)
        back = max(float(np.linalg.norm(man.grip(G2, X2, s) - rest_of(G2, X2, s))) for s in "lr")
        fl = {"head not under the muzzle at the shot": under < DUCK_UNDER - 0.01,
              "head near the muzzle at the shot": dist < DUCK_CLEAR,
              "fists not on their rests after": back > 0.02,
              "fists near the muzzle at the shot": low < FISTS_UNDER}
        return {"fire_t": round(FIRE_T, 4), "head_under_muzzle_cm": round(under * 100, 2),
                "head_to_muzzle_cm": round(dist * 100, 2), "fists_off_rest_at_fire_cm": round(fists * 100, 2),
                "fists_off_rest_after_cm": round(back * 100, 2), "fists_under_muzzle_at_fire_cm": round(low * 100, 2),
                "fists_to_muzzle_at_fire_cm": round(off * 100, 2), "duck_flags": fl,
                "duck_severity": 100.0 * (max(DUCK_UNDER - 0.01 - under, 0.0) + max(back - 0.02, 0.0) +
                                          max(FISTS_UNDER - low, 0.0) +
                                          max(DUCK_CLEAR - dist, 0.0))}

    holds = [{"hand": "l", "socket": "round.hold.l", "from": T_GRAB, "to": DROP_T},
             {"hand": "r", "socket": "round.hold.r", "from": T_GRAB, "to": DROP_T}]
    return {"long": LOAD_LONG, "fn": fn, "weapon": "slung", "holds": holds, "sockets_at": sock,
            "events": [{"t": round(FIRE_T, 4), "fx": "mortar_fire", "socket": "prop.muzzle", "cue": "mortar_fire"}],
            "prop": prop_block(MORTAR, root, R, {"cranks": crank_block()}), "round": round_at,
            "case": (case_at, case_R), "duck": duck_check,
            "kit_ref": kit_ref(man, "kneel_down"), "kit_from": "kneel_down (its last pose)",
            "from": "crew_clips.py: this file's kneel_down (motion capture, its last pose; the right foot laid on its "
                    "instep), the back, arms and head keyed, the arms by Man.arm to the round",
            "about": ("The mortar's loader, kneeling on the tube's right facing it (as kneel_down leaves him: chain "
                      "kneel_down, mortar_load, stand_up, each a blend; the mortar fires to his right): his left fist "
                      "resting on his raised left knee and his right on the front of his right thigh, he turns and "
                      "bends to his left and takes a round standing on its tail on the case by his left knee "
                      "(`round_case`, an ammunition chest on end, 0.32 m tall: the round stands on its top), lifts it "
                      "and carries it before him in both hands up to the muzzle, turning it fins down along the tube, "
                      f"slides it in and lets it drop at {DROP_T} s; at once his hands come down and he ducks, bending "
                      f"low and turning his face away to his left and down in one move over {DUCK_T:.2f} s, so the top "
                      f"of his head is under the muzzle when it fires: mortar_fire at the prop's `muzzle` at "
                      f"{FIRE_T:.2f} s (event socket \"prop.muzzle\": the mortar prop's socket, not the man's "
                      f"socket_muzzle; cue mortar_fire), his fists by then down by his chest and knee, at least "
                      f"{FISTS_UNDER * 100:.0f} cm under the muzzle's top, still on their way to his knee and thigh "
                      f"(on them from {T_BACK:.2f} s); he stays down a moment and rises back for the next round by "
                      f"{LOAD_LONG} s. `round` gives the round's tail and turn every key (shown 1 while it is on the "
                      "case and in his hands; 0 once dropped down the tube: draw a fresh round on the case for the "
                      f"loop's start); his fists are on its hold.l / hold.r from {T_GRAB} s to the drop. " +
                      rifle_says(man, "His rifle slung across his back. "))}


# ---------------------------------------------------------------------------------------- mortar_sight
SIGHT_LONG = 2.8
SIGHT_TURN = -25.0                  # (the mortar fires 25 degrees left of the way his chest faces: the tube on his
                                    #  right, his body behind and left of the sight, clear of it)
GLANCE = (1.95, 2.15, 2.35, 2.62)   # (his look up over the sight: rising, up, back down, at the sight again;
GLANCE_RISE = 5.0                   #  his back unbent this much for it)
EYE_DOWN = 20.0                     # (the sight's eyepiece looked into 20 degrees down)
SIGHT_STANDOFF = 0.012              # (his eye this far back from the eyepiece: more for a file whose cap or helmet
                                    #  meets the sight)
SIGHT_HIP = 22.0                    # (his hips bent this much more than kneel_down's, the legs kept planted)


def crank_handle(name, ang):
    c = CRANKS[name]
    hub, ax = np.asarray(c["hub"], float), unit(c["axis"])
    ref = unit(np.cross(ax, Z_)) if abs(ax @ Z_) < 0.9 else X_
    ref = unit(ref - ax * (ref @ ax))
    return hub + c["arm"] * (R_axis(ax, ang) @ ref)


def crank_angles(t):
    """The cranks' turns (degrees): the traverse a little right and back (0-1.4 s), then the elevation (1.2-2.5 s)."""
    tr = 70.0 * math.sin(math.pi * ramp(t, 0.15, 1.35))
    el = -90.0 * math.sin(math.pi * ramp(t, 1.25, 2.55))
    return tr, el


def make_mortar_sight(man):
    lq_k, mv_k = kneel_square(man)
    planted = legs_of(man, lq_k, mv_k)
    lq0, mv0 = pelvis_pitch(man, lq_k, mv_k, SIGHT_HIP, drop=0.03)
    lq0 = replant(man, lq0, mv0, planted)
    Gk, _ = man.fk(lq0, mv0)
    fk = facing(Gk, man, "chest")
    R = yaw(math.degrees(math.atan2(fk[1], fk[0])) + SIGHT_TURN)
    eye_local = np.asarray(MORTAR["sockets"]["sight.eye"], float)
    way = R @ np.array((math.cos(math.radians(EYE_DOWN)), 0.0, -math.sin(math.radians(EYE_DOWN))))

    def body(lq, bend):
        return trunk(man, lq, bend=bend, turn=0.0, lean=-4.0)

    # (bent low over the sight: the back's bend found so his eyes come down to the eyepiece)
    best = None
    for bend in np.arange(0.0, 70.0, 1.0):
        l2 = body(lq0.copy(), bend)
        l2 = look(man, l2, mv0, None, neck=0.5, toward=way)
        G2, X2 = man.fk(l2, mv0)
        e = eye_at(man, G2, X2)
        c = abs(e[2] - eye_local[2])
        if best is None or c < best[0]:
            best = (c, bend, e)
    _, bend0, e0 = best
    root = e0 + way * SIGHT_STANDOFF - R @ eye_local
    root[2] = 0.0
    S0 = placed(MORTAR, root, R)
    G, X = man.fk(lq0, mv0)

    def sock(t):
        tr, el = crank_angles(t)
        S = dict(S0)
        S["crank.traverse.handle"] = root + R @ crank_handle("crank.traverse", tr)
        S["crank.elevation.handle"] = root + R @ crank_handle("crank.elevation", el)
        return S

    def fn(t):
        lq, mv = lq0.copy(), mv0.copy()
        glance = ramp(t, GLANCE[0], GLANCE[1]) * (1.0 - ramp(t, GLANCE[2], GLANCE[3]))
        lq = body(lq, bend0 - GLANCE_RISE * glance)
        lq = breathe(man, lq, t, SIGHT_LONG, deg=0.4)
        S = sock(t)
        lq = look(man, lq, mv, None, neck=0.5, toward=way + np.array((0.0, 0.0, 0.30 * glance)))
        lq = clamp_head(man, lq)
        for s, side, k in (("l", -1.0, "crank.elevation.handle"), ("r", 1.0, "crank.traverse.handle")):
            hub = S["crank." + ("elevation" if s == "l" else "traverse")]
            d = unit((R @ X_) * 0.8 + (hub - X[man.b("upperarm_" + s)]) * 0.6 + np.array((0, 0, -0.3)))
            lq = fist_to(man, lq, mv, s, S[k], d, (0.0, 0.0, 1.0) if s == "l" else R @ Y_,
                         (-0.2, side * 0.9, -0.6), share=0.6, curl=75.0)
        return settle_sling(man, lq, mv)

    holds = [{"hand": "l", "socket": "crank.elevation.handle", "from": 0.0, "to": SIGHT_LONG},
             {"hand": "r", "socket": "crank.traverse.handle", "from": 0.0, "to": SIGHT_LONG}]
    return {"long": SIGHT_LONG, "fn": fn, "weapon": "slung", "events": [], "holds": holds, "sockets_at": sock,
            "prop": prop_block(MORTAR, root, R, {"cranks": crank_block()}), "eye": (GLANCE[0], GLANCE[3]),
            "kit_ref": kit_ref(man, "kneel_down"), "kit_from": "kneel_down (its last pose)",
            "from": "crew_clips.py: this file's kneel_down (motion capture, its last pose; the right foot laid on its "
                    "instep), the hips bent deeper with the legs kept planted (two-bone IK), bent low over the sight; "
                    "the arms keyed to the cranks (Man.arm), the head to the sight",
            "about": ("The mortar's gunner, kneeling behind the tube's left (as kneel_down leaves him: chain kneel_down, "
                      "mortar_sight, stand_up; the mortar fires 25 degrees left of his chest's way: the tube on his right), bent low "
                      "with his eye to the sight's eyepiece (`sight.eye`, stand-in, looked into 20 degrees down), his "
                      "left fist on the elevation crank's handle and his right on the traverse crank's (stand-in sockets "
                      "crank.elevation / crank.traverse: each crank's hub, axis, arm, zero and sense are in "
                      "`prop.cranks`; `cranks_deg` gives each crank's turn every key, so the real cranks turn with his "
                      "fists; `prop.hold_sockets` says how the handles' sockets are found), laying the tube: the "
                      "traverse a little right and back, then the elevation, and a look up over the sight at 2.0-2.6 s. "
                      "No events. " + rifle_says(man, "His rifle slung across his back. "))}


MAKERS = {"hmg_fire_prone": make_fire_prone, "hmg_fire_sit": make_fire_sit, "hmg_feed": make_feed,
          "hmg_carry_gun": make_carry_gun, "hmg_carry_tripod": make_carry_tripod, "hmg_carry_boxes": make_carry_boxes,
          "mortar_load": make_mortar_load, "mortar_sight": make_mortar_sight, "sit_down": make_sit_down,
          "sit_up": make_sit_up}


# ================================================================================================ baking a file's clips
def r4(v):
    return [round(float(x), 4) for x in v]


WHIP = ("whip_1", "whip_2", "whip_3", "whip_4")


def steady_whip(man, fn):
    """A radio man's long whip antenna (whip_1 to whip_4, if the file has it) kept still in the world over a crew
    clip: its foot rides the set, its sections straight (the file's rule: straight, at rest), its way the clip's first
    key's. Rigid on the chest its 3.25 m tip would sweep 30-40 cm a key whenever he turns or bends; its spring base
    gives instead. The walks keep the file's own sway."""
    if "whip_1" not in man.ix:
        return fn
    lq0, mv0 = fn(0.0)
    G0, _ = man.fk(lq0, mv0)
    i = man.b("whip_1")
    Gw = G0[i]
    rest = [man.b(n) for n in WHIP[1:]]

    def f(t):
        lq, mv = fn(t)
        G, _ = man.fk(lq, mv)
        lq = lq.copy()
        lq[i] = G[man.par[i]].T @ Gw
        for k in rest:
            lq[k] = np.eye(3)
        return lq, mv
    return f


# (refitting a clip to a file whose body or kit is in the way: each clip's tunables, tried in turn, a value kept when
# it lowers how far the clip breaks the checks, until it breaks none; the first values are the ones all files start
# from. The values a file needed are kept in crew_checks.json's `fits`, which --check and --pictures use.)
FITS = {"hmg_fire_prone": [("PRONE_TIP", ((8.0, 12.0, 0.0), (14.0, 20.0, 10.0))),
                           ("PRONE_GUN_OUT", (0.08, 0.11, 0.14, 0.17, 0.20)), ("PRONE_EYE", (0.03, 0.04, 0.025))],
        "hmg_fire_sit": [],
        # (a long weapon's butt (a BAR's, a Thompson's drum) down by the gun's mount as he holds it at port: held
        # higher; a bazooka's tube, its back end in the ground held up at port: held across him, nearer level)
        "sit_down": [("SIT_PORT_AT", ((0.15, 0.10, -0.22), (0.15, 0.10, -0.12), (0.15, 0.18, -0.12))),
                     ("SIT_PORT", ((0.25, 0.20, 1.0), (0.6, 0.8, 0.5)))],
        "sit_up": [("SIT_PORT_AT", ((0.15, 0.10, -0.22), (0.15, 0.10, -0.12), (0.15, 0.18, -0.12))),
                   ("SIT_PORT", ((0.25, 0.20, 1.0), (0.6, 0.8, 0.5)))],
        "hmg_feed": [("PRONE_TIP", ((8.0, 12.0, 0.0), (14.0, 20.0, 10.0))), ("FEED_RIGHT", (0.12, 0.16, 0.20, 0.24)),
                     ("FEED_TURN", (-20.0, -26.0, -32.0)), ("FEED_AHEAD", (0.32, 0.27, 0.22, 0.37)),
                     ("FEED_LIFT", (0.10, 0.06, 0.03)), ("FEED_ELBOW", (-0.6, -0.3, 0.0, -1.0))],
        "hmg_carry_gun": [("CARRY_ARM_OUT", (0.15, 0.19, 0.23, 0.27)), ("CARRY_IN", (0.025, 0.0, -0.02)),
                          ("CARRY_PITCH", (-12.0, -6.0, 0.0)), ("CARRY_YAW", (-3.0, -8.0))],
        "hmg_carry_tripod": [("GRIP_ALONG", (0.68, 0.84, 0.76)), ("TRIPOD_GAP", (0.012, 0.025, 0.04)),
                             ("TRIPOD_BACK", (0.06, 0.09, 0.12, 0.16)), ("TRIPOD_SLING", (None, (1.0, 0.10), (1.6, 0.16)))],
        "hmg_carry_boxes": [("BOX_OUT", (0.33, 0.36, 0.39, 0.42))],
        "mortar_load": [("LOAD_FLOW", (False, True)),      # (fitted inside make_mortar_load too: its picks kept)
                        ("DUCK_WAIT", (0.07, 0.03, 0.0)), ("DUCK_T", (0.55, 0.58)),  # (the duck slower: a long
                                                           #  slung weapon's end (a bazooka's tube) swung less a key)
                        ("REACH_EASE", (0.25, 0.18, 0.12)),    # (the reach to the round evener: its elbow's top step)
                        # (all at once (one tunable alone does not lower the top step when two moves step as far: a
                        # long slung tube's end swung by the lift and by the duck): the duck over 0.62 s from the drop
                        # (the head still under the muzzle at the shot), the grab sooner so the lift is slower, and
                        # the reach to the round evener for its shorter time)
                        ("LOAD_PACE", (None, {"DUCK_WAIT": 0.0, "DUCK_T": 0.62, "T_GRAB": 0.74, "REACH_EASE": 0.18},
                                       {"DUCK_WAIT": 0.0, "DUCK_T": 0.62, "T_GRAB": 0.74, "REACH_EASE": 0.12})),
                        # (his right elbow higher and further back as he reaches down past his raised left thigh to the
                        # round (a greatcoat's thigh: the forearm over it, not into it), the reach evener so that
                        # elbow's wider swing keeps under 15 cm a key: lower-case keys go into LOAD_FIT; last, his
                        # right elbow only a little higher at the old pace (a bazooka man paced slower by LOAD_PACE
                        # after his reach was picked at the brisker pace: his forearm 1.9 cm into his raised left
                        # thigh at 0.67 s with GRAB_POLE, clear with GRAB_POLES[0], his elbow's top step unchanged))
                        ("LOAD_REACH", (None, {"grab_pole": [-0.4, 0.8, -0.8], "REACH_EASE": 0.18},
                                        {"grab_pole": [-0.4, 0.8, -0.8], "REACH_EASE": 0.12},
                                        {"way": [[0.08, 0.13], [0.04, 0.16]], "REACH_EASE": 0.18},
                                        {"grab_pole": list(GRAB_POLES[0])}))],
        "mortar_sight": [("SIGHT_STANDOFF", (0.012, 0.025, 0.04))]}


class tuned:
    """Module tunables set for a while: `with tuned({"BOX_OUT": 0.36}): ...`. A tunable read back from
    crew_checks.json's `fits` comes as a list: it is set as the tuple FITS gave (the makers key their memos on them)."""
    def __init__(self, over):
        def tup(v):
            return tuple(tup(x) for x in v) if isinstance(v, list) else v
        self.over = {k: tup(v) for k, v in (over or {}).items()}

    def __enter__(self):
        g = globals()
        self.old = {k: g[k] for k in self.over}
        g.update(self.over)
        return self

    def __exit__(self, *a):
        globals().update(self.old)


HEAD_STEP = 18.0                    # (his head turns at most this much from one key to the next)
HEAD_EASE = 16.5                    # (mortar_load's head before the drop held to this much a key)
HOLD_GRACE = (0.25, 0.10)           # (a fist closing on a held part, this long before its hold, or opening, this long
                                    #  after, may touch it: otherwise a free hand is checked against the props too)
SLING_GAP = 0.03                    # (a slung rifle lies within this of his back)
PAIR_OVERLAP = 0.012                # (the two-man gun crew's bodies overlap no deeper than this: cloth on cloth)


def low_limit(r, z):
    """How low (cm) a clip's feet, knees or elbows may go: 2 cm under the ground, or for a one-shot's feet as low as
    the pose it must start or end on has them (kneel_down's last pose has its toes 8-10 cm in the ground), 0.5 cm
    more."""
    if z == "feet_low_cm" and "chain_pose_feet_low_cm" in r:
        return min(-2.0, r["chain_pose_feet_low_cm"] - 0.5)
    return -2.0


def hand_limit(r, z="hand"):
    """How far (cm) past its allowance a hand (z "hand") or a forearm ("arm") may go into his body: not at all, or
    for a one-shot as far as the pose it must start or end on has it (a capture's thumb at his cheek), 1 cm more."""
    v = r.get(f"chain_pose_{z}_past_cm", 0.0)
    return v + 1.0 if v > 0.0 else 0.0


def severity(r, name):
    """How far a clip's checks break the brief's limits (flags' limits), summed, in cm (degrees for turns); 0 when
    clean."""
    s = max(r.get("loop_gap_cm", 0.0) - 1.0, 0.0) + max(r.get("chain_gap_cm", 0.0) - 1.0, 0.0)
    s += max(r["jump_cm"] - 15.0, 0.0) + 0.3 * max(r["head_step_deg"] - HEAD_STEP, 0.0)
    s += sum(max(low_limit(r, z) - r[z], 0.0) for z in ("feet_low_cm", "knees_low_cm", "elbows_low_cm"))
    s += max(r["head_chest_deg"] - 80.0, 0.0) + 5.0 * max(r["fist_socket_cm"] - 1.5, 0.0)
    s += max(r["hand_past_cm"] - hand_limit(r), 0.0) + max(r["arm_past_cm"] - hand_limit(r, "arm"), 0.0)
    s += max(r["body_in_prop_cm"] - 1.5, 0.0) + max(r["weapon_in_prop_cm"] - 0.3, 0.0)
    if "weapon_low_cm" in r:
        s += max(-0.3 - r["weapon_low_cm"], 0.0) + max(r["weapon_low_cm"] - 1.5, 0.0)
    s += max(r.get("sling_gap_cm", 0.0) - SLING_GAP * 100, 0.0)
    s += max(-0.3 - r.get("kit_low_cm", 9.0), 0.0) + max(r.get("kit_off_cm", 0.0) - 3.0, 0.0)
    if "hips_low_cm" in r:
        s += max(abs(r["hips_low_cm"]) - 3.0, 0.0) + max(abs(r["chest_low_cm"]) - 3.0, 0.0)
    if "eye_over_receiver_cm" in r:
        lo, hi = SIGHT[name][1]
        s += max(lo * 100 - r["eye_over_receiver_cm"][0], 0.0) + max(r["eye_over_receiver_cm"][1] - hi * 100, 0.0)
        s += 2.0 * r["sight_blocked_cm"]
    s += max(r.get("pair_overlap_cm", 0.0) - PAIR_OVERLAP * 100, 0.0)
    s += r.get("duck_severity", 0.0)
    return s


def kit_note(man, sp):
    """The about's line on a part of his kit that rides the bolt bone (crew_man.kit)."""
    if not getattr(man, "kit", False) or sp.get("kit_ref") is None:
        return ""
    return (" The part on his bolt bone (his kit, not the rifle's bolt) is keyed every frame so it keeps its place on his "
            f"body ({KIT_FRAME}'s frame) as his own {sp.get('kit_from', 'clips')} have it, wherever the weapon is; "
            f"where he lies on it, it is pushed up off the ground (at most {KIT_LIFT * 100:.1f} cm).")


def hold_sockets(name):
    """The sockets his fists hold that move with a carried or turned part (not in prop.sockets): how each is found."""
    if name == "hmg_carry_boxes":
        return {"handle.l": "the left box's `handle` socket (prop.sockets.handle) at its prop.track_l pose",
                "handle.r": "the right box's `handle` socket at its prop.track_r pose"}
    if name == "mortar_load":
        return {"round.hold.l": "the round's hold.l socket (round.sockets) at its `round` track pose",
                "round.hold.r": "the round's hold.r socket at its `round` track pose"}
    if name == "mortar_sight":
        return {"crank.traverse.handle": "the traverse crank's handle: prop.cranks[crank.traverse].hub + arm along "
                                         "its zero, turned cranks_deg.traverse about its axis (right-handed)",
                "crank.elevation.handle": "the elevation crank's handle: prop.cranks[crank.elevation].hub + arm "
                                          "along its zero, turned cranks_deg.elevation about its axis (right-handed)"}
    return {}


def clips_for(D, names=NAMES):
    """The named clips made on this file's rig, each refitted (FITS) until it passes the checks: (clips, specs, man);
    specs[name] = (spec, poses), the spec's `fit` the tunables it needed and `checks` its checks. hmg_feed is fitted
    with the made hmg_fire_prone beside it (the two-man crew at one prop: pair_overlap); sit_down and sit_up with
    the made hmg_fire_sit (they chain into and out of it)."""
    man = crew_man(D)
    out, specs = {}, {}
    order = [n for n in NAMES if n in names]
    for name in order:
        fit = {}
        if name in ("sit_down", "sit_up"):
            fit = dict(specs["hmg_fire_sit"][0]["fit"]) if "hmg_fire_sit" in specs else {}

        def trial_of(over):
            with tuned(over):
                c, sp, poses = clip_of(D, man, name)
                r = check_file(D, {name: c}, {name: (sp, poses)}, man)[name]
            if name == "hmg_feed" and "hmg_fire_prone" in out:
                d, where = pair_overlap(man, out["hmg_fire_prone"], c)
                r["pair_overlap_cm"], r["pair_overlap_at"] = round(d * 100, 2), where
            return (severity(r, name), dict(over), c, sp, poses, r)

        best = trial_of(fit)
        if best[3].get("fit_load"):                    # (mortar_load: its own picks kept while the outer tunables move)
            best = best[:1] + (dict(best[1], LOAD_FIT=best[3]["fit_load"]),) + best[2:]
        for _ in range(2):                             # (a second pass when one tunable's move opens another's)
            start = best[0]
            for key, values in FITS.get(name, []):
                if best[0] == 0.0:
                    break
                for v in values:
                    if v == best[1].get(key, values[0]):
                        continue
                    tr = trial_of(moved(best[1], key, v))
                    if tr[0] < best[0]:
                        best = tr
                        if best[0] == 0.0:
                            break
            if best[0] == 0.0 or best[0] >= start:
                break
        _, fit, c, sp, poses, r = best
        if sp.get("fit_load"):
            fit = dict(fit, LOAD_FIT=sp["fit_load"])
        sp["fit"], sp["checks"] = fit, r
        out[name] = c
        specs[name] = (sp, poses)
    return out, specs, man


def moved(over, key, v):
    """The tunables `over` with FITS's `key` set to v; a v that is a dict moves several at once (LOAD_PACE): its
    upper-case keys are module tunables, its lower-case keys go into mortar_load's LOAD_FIT picks."""
    if not isinstance(v, dict):
        return dict(over, **{key: v})
    out = dict(over, **{k: x for k, x in v.items() if not k.islower()})
    picks = {k: x for k, x in v.items() if k.islower()}
    if picks:
        out["LOAD_FIT"] = dict(out.get("LOAD_FIT") or {}, **picks)
    return out


def clip_of(D, man, name):
    """One clip made and baked on this file's rig, with the current tunables: (clip, spec, poses)."""
    sp = MAKERS[name](man)
    if not sp.get("walk") and not sp.get("whip_done"):
        sp["fn"] = steady_whip(man, sp["fn"])
    sp["fn"] = keep_kit(man, sp["fn"], sp.get("kit_ref"))
    L = sp["long"]
    tracks, poses = bake(man, sp["fn"], L)
    hands = []
    for t, lq, mv in poses:
        G, X = man.fk(lq, mv)
        hands.append([round(t, 4)] + r4(man.grip(G, X, "l")) + r4(man.grip(G, X, "r")))
    stride = stride_of(man, poses, L) if sp.get("walk") else 0.0
    if sp.get("walk"):
        W = D["clips"]["walk"]
        evs = [dict(e, t=round(e["t"] * SLOW, 4)) for e in W["events"]]
    else:
        evs = sp["events"]
    weapon = sp["weapon"] if len(man.weapon_pts) else "none"
    c = {"long": round(L, 4), "loop": sp.get("loop", True), "stride": stride, "weapon": weapon, "tracks": tracks,
         "events": evs, "from": sp["from"], "about": sp["about"] + kit_note(man, sp)}
    if "prop" in sp:
        c["prop"] = sp["prop"]
    if "carried" in sp:
        P, R0, p0 = sp["carried"]
        rows = []
        for t, lq, mv in poses:
            Rg, pg = chest_ride(man, lq, mv, R0, p0)
            rows.append((t, pg, Rg, 1))
        c["prop"] = prop_block(P, rows[0][1], rows[0][2], {"rides": sp["ride"], "track": pose_track(rows)})
    if "boxes" in sp:
        tl, tr = [], []
        for t, lq, mv in poses:
            bx = sp["boxes"](t)
            tl.append((t, bx["l"][0], bx["l"][1], 1))
            tr.append((t, bx["r"][0], bx["r"][1], 1))
        c["prop"] = prop_block(BOX, tl[0][1], tl[0][2], {"count": 2, "track_l": pose_track(tl),
                                                          "track_r": pose_track(tr)})
    if "round" in sp:
        rows = [(t,) + tuple(sp["round"](t)) for t, _, _ in poses]
        c["round"] = dict(prop_block(ROUND, rows[0][1], rows[0][2]), **pose_track(rows))
        ca, cr = sp["case"]
        c["round_case"] = {"kind": "ammo_case", "stand_in": True, "root": r4(ca),
                           "turn_deg": round(math.degrees(math.atan2(cr[1, 0], cr[0, 0])), 2),
                           "size": list(CASE_SIZE)}
    if name == "mortar_sight":
        c["cranks_deg"] = {"track_fields": ["t", "traverse", "elevation"],
                           "track": [[round(t, 4)] + [round(v, 2) for v in crank_angles(t)] for t, _, _ in poses]}
    c["hands"] = {"track_fields": ["t", "lx", "ly", "lz", "rx", "ry", "rz"], "track": hands}
    c["holds"] = sp["holds"]
    if "prop" in c and hold_sockets(name):
        c["prop"]["hold_sockets"] = hold_sockets(name)
    return c, sp, poses


# ================================================================================================ the checks
def angle_of(R):
    return math.degrees(math.acos(min(1.0, max(-1.0, (np.trace(R) - 1) / 2))))


def prop_shapes_at(name, c, spec, t):
    """The stand-in props' boxes (centre, half, turn) in the man's root frame at t, with their part names."""
    out = []

    def put(P, root, R, keep=None):
        for part, (cc, half, Rb) in P["shapes"]:
            if keep and part not in keep:
                continue
            out.append((part, np.asarray(root) + R @ cc, half, R @ Rb))

    sp = spec
    if "carried" in sp:
        P, _, _ = sp["carried"]
        tr = c["prop"]["track"]["track"]
        k = min(range(len(tr)), key=lambda i: abs(tr[i][0] - t))
        q = tr[k][4:8]
        put(P, tr[k][1:4], gore.qmat(q))
    elif "boxes" in sp:
        for key in ("track_l", "track_r"):
            tr = c["prop"][key]["track"]
            k = min(range(len(tr)), key=lambda i: abs(tr[i][0] - t))
            put(BOX, tr[k][1:4], gore.qmat(tr[k][4:8]))
    else:
        pr = c["prop"]
        P = {"hmg_fire_prone": HMG_LOW, "hmg_feed": HMG_LOW, "hmg_fire_sit": HMG_SIT, "sit_down": HMG_SIT,
             "sit_up": HMG_SIT}.get(name, MORTAR)
        put(P, pr["root"], yaw(pr["turn_deg"]))
    if "round" in c:
        tr = c["round"]["track"]
        k = min(range(len(tr)), key=lambda i: abs(tr[i][0] - t))
        if tr[k][8]:
            put(ROUND, tr[k][1:4], gore.qmat(tr[k][4:8]))
        rc = c["round_case"]
        put(CASE, rc["root"], yaw(rc["turn_deg"]))
    return out


def inside_box(Q, cc, half, R, shrink=0.0):
    """How far each point is inside a stand-in's box (a bar: inside its cylinder); negative outside."""
    L = (Q - cc) @ R
    h = np.asarray(half, float) - shrink
    if isinstance(half, Bar):
        return np.minimum(h[0] - np.abs(L[:, 0]), h[1] - np.hypot(L[:, 1], L[:, 2]))
    pen = np.min(h - np.abs(L), axis=1)
    return pen


SIGHT = {"hmg_fire_prone": (0.45, (0.02, 0.10)), "hmg_fire_sit": (0.60, (0.05, 0.10))}
# (his eye over the receiver's rear top: (the target's height 25 m out, the eye's least and most height over the
# receiver's top); the line from his eye to the target clear of the gun and its mount)


def receiver_top(name, c):
    """The stand-in receiver's top over the ground, at its rear (the highest face of the gun's boxes)."""
    P = HMG_LOW if name == "hmg_fire_prone" else HMG_SIT
    root = np.asarray(c["prop"]["root"], float)
    return float(max(root[2] + cc[2] + half[2] for part, (cc, half, Rb) in P["shapes"]
                     if part == "gun" and not isinstance(half, Bar)))


def sight_line(man, name, c, G, X):
    """His eye's height over the receiver's rear top, and how deep the line from his eye to the target 25 m out (along
    the bore) runs inside the gun, its grips or its mount (0 when it clears them)."""
    P = HMG_LOW if name == "hmg_fire_prone" else HMG_SIT
    pr = c["prop"]
    root, R = np.asarray(pr["root"], float), yaw(pr["turn_deg"])
    e = eye_at(man, G, X)
    tgt = root + R @ np.array((25.0, 0.0, SIGHT[name][0]))
    s = np.linspace(0.0, 2.5 / float(np.linalg.norm(tgt - e)), 400)[1:]
    line = e[None, :] + s[:, None] * (tgt - e)[None, :]
    block = 0.0
    for part, (cc, half, Rb) in P["shapes"]:
        block = max(block, float(inside_box(line, root + R @ cc, half, R @ Rb).max()))
    return e[2] - receiver_top(name, c), block


def kit_off(man, B, G, X, ref):
    """How far his kit's points on the bolt bone are from their place on his body (ref: the bolt's turn and head in
    his KIT_FRAME's frame, from the file's own clip)."""
    R, p = ref
    sp, bo = man.b(KIT_FRAME), man.b("bolt")
    P0 = B.Pk - man.H[bo]
    now = X[bo] + P0 @ G[bo].T
    want = X[sp] + G[sp] @ p + P0 @ (G[sp] @ R).T
    return float(np.linalg.norm(now - want, axis=1).max())


def own_through(man):
    """The file's own motion capture's worst forearm and hand in his trunk (through, without his rifle: his own clips
    hold it), over idle and walk (his arms down at his sides): the crew clips are held to it where a sleeve meets his
    coat (his trunk's bones). (The aiming captures put a hand 1.3-2.5 cm into his cheek; they are not taken.)"""
    if not hasattr(man, "_own_through"):
        B = BODY(man)
        tb = B.trunk_bones()
        lim = np.full(man.nb, 9.0)
        lim[tb] = 0.0
        fore = hand = 0.0
        for cn in ("idle", "walk"):
            c = man.D["clips"].get(cn)
            if c is None:
                continue
            for t in times_of(c["long"])[::3]:
                G, X = gore.pose(man.rig, c, t)
                Q = B.skin(G, X)
                fore = max(fore, B.through(G, X, Q, probe="fore", gun=False, lim=lim)[2])
                hand = max(hand, B.through(G, X, Q, probe="hands", gun=False, lim=lim)[2])
        man._own_through = (fore, hand)
    return man._own_through


ARM_MIN, HAND_MIN = 0.015, 0.005    # (a forearm or a hand in his body allowed this deep; into his trunk as deep as in
                                    #  his own motion capture and 3 mm more, if that is more)


def allowed(man):
    """The depth a forearm and a hand may go into each of his bones: (arm, hand) arrays by bone."""
    if not hasattr(man, "_allowed"):
        fore, hand = own_through(man)
        tb = BODY(man).trunk_bones()
        la, lh = np.full(man.nb, ARM_MIN), np.full(man.nb, HAND_MIN)
        la[tb] = max(ARM_MIN, fore + 0.003)
        lh[tb] = max(HAND_MIN, hand + 0.003)
        man._allowed = (la, lh)
    return man._allowed


def chain_gap(man, c, at, pose):
    """How far (max over his bones' heads) the baked clip at `at` is from a pose (lq, mv)."""
    G, X = gore.pose(man.rig, c, at)
    G2, X2 = man.fk(*pose)
    keep = np.array([n not in WHIP for n in man.rig.names])
    return float(np.linalg.norm(X[keep] - X2[keep], axis=1).max())


def check_file(D, clips, specs, man):
    """Each clip's checks over its keys (as the game plays them, from the baked keys): the loop's gap (a one-shot: how
    far its first and last keys are from the poses it chains from and to), the biggest step of a bone between keys and
    of his head's turn, the lowest of his feet, knees and elbows (skin), hips and chest (prone), the head's turn from
    the chest, each held fist from its socket, a hand or a forearm in his body or kit (Body.through), his body in a
    stand-in prop, his weapon (and the kit on his bolt bone) in a prop, a laid weapon's lowest point, a slung rifle's
    gap to his back, the kit's place on his body and its height, and for the gunners the line from his eye over the
    sights."""
    B = BODY(man)
    lim_arm, lim_hand = allowed(man)
    res = {}
    for name, c in clips.items():
        sp, poses = specs[name]
        L = c["long"]
        Xs, Gs = [], []
        for t in times_of(L):
            G, X = gore.pose(man.rig, c, t)
            Gs.append(G)
            Xs.append(X)
        base = None
        if c["loop"]:
            gap = float(np.abs(Xs[-1] - Xs[0]).max())
        else:
            gap = max(chain_gap(man, c, 0.0, sp["chain"]["start"]), chain_gap(man, c, L, sp["chain"]["end"]))
            # (the poses a one-shot must start and end on: what they have themselves (a capture's toes in the
            # ground, its thumb at the cheek) is the least the one-shot can have there)
            base = {"feet": 9.0, "hand": 0.0, "arm": 0.0}
            for pose in (sp["chain"]["start"], sp["chain"]["end"]):
                G2, X2 = man.fk(*pose)
                base["feet"] = min(base["feet"], B.low(G2, X2, "feet"))
                base["hand"] = max(base["hand"], B.through(G2, X2, lim=lim_hand, gun=False)[2])
                base["arm"] = max(base["arm"], B.through(G2, X2, probe="fore", lim=lim_arm, gun=False)[2])
        jump = max(float(np.linalg.norm(Xs[k + 1] - Xs[k], axis=1).max()) for k in range(len(Xs) - 1))
        he = man.b("head")
        head_step = max(angle_of(Gs[k][he].T @ Gs[k + 1][he]) for k in range(len(Gs) - 1))
        low = {z: 9.0 for z in ("feet", "knees", "elbows", "hips", "chest", "body")}
        hc, fist, through, fore, inprop, gun_prop = 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
        through_at, fore_at, inprop_at, gun_prop_at = "", "", "", ""
        rifle_hand, wlow, sling, klow, koff = 9.0, 9.0, 0.0, 9.0, 0.0
        eye_err = 0.0
        eye_over, sight_block = [], 0.0
        hand_ex, arm_ex = (0.0, ""), (0.0, "")
        ch = man.b("chest")
        kref = sp.get("kit_ref") if getattr(man, "kit", False) else None
        for k, (t, G, X) in enumerate(zip(times_of(L), Gs, Xs)):
            Q = B.skin(G, X)
            for z in ("feet", "knees", "elbows", "hips", "chest"):
                low[z] = min(low[z], float(Q[B.zone[z], 2].min()))
            low["body"] = min(low["body"], float(Q[:, 2].min()))
            hc = max(hc, angle_of(G[ch].T @ G[he]))
            # (the held fists, as baked and as the game plays them: from the keys)
            Ssock = sp["sockets_at"](t)
            for h in c["holds"]:
                if h["from"] - 1e-6 <= t <= h["to"] + 1e-6:
                    g = man.grip(G, X, h["hand"])
                    fist = max(fist, float(np.linalg.norm(g - Ssock[h["socket"]])))
            if sp.get("eye") and not (sp["eye"][0] <= t <= sp["eye"][1]):
                eye_err = max(eye_err, float(np.linalg.norm(eye_at(man, G, X) - Ssock["sight.eye"])))
            # (a hand or a forearm inside his body or kit; his rifle left out while a fist holds it, and HOLD_GRACE[1]
            # either side, as it lets go or (sit_up, sit_down played backward) closes on it)
            rh = any(a - HOLD_GRACE[1] - 1e-6 <= t <= b + HOLD_GRACE[1] for _, a, b in sp.get("rifle_held", []))
            dth, where, ex, exw = B.through(G, X, Q, lim=lim_hand, gun=not rh)
            if dth > through:
                through, through_at = dth, f"t {t:.2f}: {where}"
            if ex > hand_ex[0]:
                hand_ex = (ex, f"t {t:.2f}: {exw}")
            dth, where, ex, exw = B.through(G, X, Q, probe="fore", lim=lim_arm, gun=not rh)
            if dth > fore:
                fore, fore_at = dth, f"t {t:.2f}: {where}"
            if ex > arm_ex[0]:
                arm_ex = (ex, f"t {t:.2f}: {exw}")
            # (his body inside a stand-in prop: all but the fists holding it (a free hand is checked too); his weapon
            # and kit in one)
            notheld = np.ones(len(Q), bool)
            for h in c["holds"]:
                if h["from"] - HOLD_GRACE[0] <= t <= h["to"] + HOLD_GRACE[1]:
                    notheld &= ~B.zone["hand_" + h["hand"]]
            Qn, an = Q[notheld], B.a[notheld]
            Wq = B.skin_gun(G, X) if len(B.Pw) else np.zeros((0, 3))
            Kq = B.skin_kit(G, X) if len(B.Pk) else np.zeros((0, 3))
            for part, cc, half, Rb in prop_shapes_at(name, c, sp, t):
                pen = inside_box(Qn, cc, half, Rb)
                j = int(pen.argmax())
                if pen[j] > inprop:
                    inprop, inprop_at = float(pen[j]), f"t {t:.2f}: {man.rig.names[an[j]]} in the {part}"
                for lab, Pq in (("weapon", Wq), ("kit", Kq)):
                    if len(Pq):
                        pw = float(inside_box(Pq, cc, half, Rb).max())
                        if pw > gun_prop:
                            gun_prop, gun_prop_at = pw, f"t {t:.2f}: {lab} in the {part}"
            Hp = Q[B.zone["hands"]]
            if len(Wq):
                rifle_hand = min(rifle_hand, float(np.min(np.linalg.norm(Hp[::3, None, :] - Wq[None, ::4, :], axis=2))))
                if c["weapon"] == "laid" or name == "sit_down":    # (sit_down lays it down: its way there too)
                    wlow = min(wlow, float(Wq[:, 2].min()))
                if c["weapon"] == "slung":
                    sling = max(sling, min_gap(Wq, Q[B.zone["trunk"]]))
            if len(Kq):
                klow = min(klow, float(Kq[:, 2].min()))
                if kref is not None:
                    koff = max(koff, kit_off(man, B, G, X, kref(t)))
            if name in SIGHT:
                ov, blk = sight_line(man, name, c, G, X)
                eye_over.append(ov)
                sight_block = max(sight_block, blk)
        r = {"long": L, "loop": c["loop"], "stride": c["stride"], "weapon": c["weapon"], "keys": len(Xs),
             ("loop_gap_cm" if c["loop"] else "chain_gap_cm"): round(gap * 100, 2), "jump_cm": round(jump * 100, 2),
             "head_step_deg": round(head_step, 1),
             "feet_low_cm": round(low["feet"] * 100, 2), "knees_low_cm": round(low["knees"] * 100, 2),
             "elbows_low_cm": round(low["elbows"] * 100, 2), "body_low_cm": round(low["body"] * 100, 2),
             "head_chest_deg": round(hc, 1), "fist_socket_cm": round(fist * 100, 2),
             "hand_in_body_cm": round(through * 100, 2), "arm_in_body_cm": round(fore * 100, 2),
             "body_in_prop_cm": round(max(inprop, 0.0) * 100, 2), "weapon_in_prop_cm": round(max(gun_prop, 0.0) * 100, 2),
             "hand_to_rifle_cm": round(rifle_hand * 100, 2), "hand_in_body_at": through_at, "arm_in_body_at": fore_at,
             "body_in_prop_at": inprop_at, "weapon_in_prop_at": gun_prop_at,
             "arm_past_cm": round(arm_ex[0] * 100, 2), "arm_past_at": arm_ex[1],
             "hand_past_cm": round(hand_ex[0] * 100, 2), "hand_past_at": hand_ex[1],
             "limits_cm": {"arm_in_trunk": round(float(lim_arm.max()) * 100, 2), "arm_elsewhere": ARM_MIN * 100,
                           "hand_in_trunk": round(float(lim_hand.max()) * 100, 2), "hand_elsewhere": HAND_MIN * 100}}
        if (c["weapon"] == "laid" or name == "sit_down") and wlow < 9.0:
            r["weapon_low_cm"] = round(wlow * 100, 2)
        if c["weapon"] == "slung" and len(B.Pw):
            r["sling_gap_cm"] = round(sling * 100, 2)
        if len(B.Pk):
            r["kit_low_cm"] = round(klow * 100, 2)
            if kref is not None:
                r["kit_off_cm"] = round(koff * 100, 2)
        if name in ("hmg_fire_prone", "hmg_feed"):
            r["hips_low_cm"] = round(low["hips"] * 100, 2)
            r["chest_low_cm"] = round(low["chest"] * 100, 2)
        if sp.get("eye"):
            r["eye_sight_cm"] = round(eye_err * 100, 2)
        if eye_over:
            r["eye_over_receiver_cm"] = [round(min(eye_over) * 100, 2), round(max(eye_over) * 100, 2)]
            r["sight_blocked_cm"] = round(sight_block * 100, 2)
        if "duck" in sp:
            r.update(sp["duck"](c))
        if base is not None:
            r["chain_pose_feet_low_cm"] = round(base["feet"] * 100, 2)
            r["chain_pose_hand_past_cm"] = round(base["hand"] * 100, 2)
            r["chain_pose_arm_past_cm"] = round(base["arm"] * 100, 2)
        res[name] = r
    return res


def pair_overlap(man, c1, c2, every=3):
    """The two-man gun crew (hmg_fire_prone's No. 1 and hmg_feed's No. 2 at the one prop, each clip on its own clock
    over 6 s): how deep any point of one man's body lies inside the other's (knn_depth: behind the k nearest surface
    points), and where."""
    B = BODY(man)
    R1, r1 = yaw(c1["prop"]["turn_deg"]), np.asarray(c1["prop"]["root"], float)
    R2, r2 = yaw(c2["prop"]["turn_deg"]), np.asarray(c2["prop"]["root"], float)
    M = R1 @ R2.T
    best, where = 0.0, ""
    nm = man.rig.names
    span = 6.0
    for kk in range(0, int(span * RATE) + 1, every):
        t = kk / RATE
        G1, X1 = gore.pose(man.rig, c1, t % c1["long"])
        G2, X2 = gore.pose(man.rig, c2, t % c2["long"])
        Q1 = B.skin(G1, X1)
        Q2 = (B.skin(G2, X2) - r2) @ M.T + r1
        N1 = B.normals(G1, B.N, B.a, B.b, B.w)
        N2 = B.normals(G2, B.N, B.a, B.b, B.w) @ M.T
        for Pa, Pb, Nb, lab in ((Q2, Q1, N1, "No. 2's {} into No. 1's {}"), (Q1, Q2, N2, "No. 1's {} into No. 2's {}")):
            lo, hi = Pb.min(0) - 0.05, Pb.max(0) + 0.05
            ia = np.nonzero(np.all((Pa > lo) & (Pa < hi), axis=1))[0][::2]
            if len(ia) == 0:
                continue
            # (only points near the other man at all: a coarse pass on every 8th of his points)
            coarse = Pb[::8]
            keep = np.zeros(len(ia), bool)
            for c0 in range(0, len(ia), 2000):
                d2 = ((Pa[ia[c0:c0 + 2000], None, :] - coarse[None, :, :]) ** 2).sum(-1)
                keep[c0:c0 + 2000] = d2.min(1) < 0.06 ** 2
            ia = ia[keep]
            if len(ia) == 0:
                continue
            lo, hi = Pa[ia].min(0) - 0.10, Pa[ia].max(0) + 0.10
            mb = np.nonzero(np.all((Pb > lo) & (Pb < hi), axis=1))[0]
            if len(mb) < 6:
                continue
            dep, near = knn_depth(Pa[ia], Pb[mb], Nb[mb])
            i = int(dep.argmax())
            if dep[i] > best:
                best, where = float(dep[i]), f"t {t:.2f}: " + lab.format(nm[B.a[ia[i]]], nm[B.a[mb[near[i]]]])
    return best, where


def flags(r, name):
    """What breaks the rules (the brief's limits)."""
    out = []
    if r.get("loop_gap_cm", 0.0) > 1.0:
        out.append("loop gap")
    if r.get("chain_gap_cm", 0.0) > 1.0:
        out.append("chain gap")
    if r["jump_cm"] > 15.0:
        out.append("jump")
    if r["head_step_deg"] > HEAD_STEP:
        out.append("head snaps")
    for z in ("feet_low_cm", "knees_low_cm", "elbows_low_cm"):
        if r[z] < low_limit(r, z):
            out.append(z)
    if r["head_chest_deg"] > 80.0:
        out.append("head turn")
    if r["fist_socket_cm"] > 1.5:
        out.append("fist off socket")
    if r["hand_past_cm"] > hand_limit(r):
        out.append("hand in body")
    if r["arm_past_cm"] > hand_limit(r, "arm"):
        out.append("arm in body")
    if r["body_in_prop_cm"] > 1.5:
        out.append("body in prop")
    if r["weapon_in_prop_cm"] > 0.3:
        out.append("weapon in prop")
    if "weapon_low_cm" in r and not (-0.3 <= r["weapon_low_cm"] <= 1.5):
        out.append("laid weapon off the ground")
    if r.get("sling_gap_cm", 0.0) > SLING_GAP * 100:
        out.append("sling off his back")
    if r.get("kit_low_cm", 9.0) < -0.3:
        out.append("kit under the ground")
    if r.get("kit_off_cm", 0.0) > 3.0:
        out.append("kit off its place")
    if "hips_low_cm" in r and (abs(r["hips_low_cm"]) > 3.0 or abs(r["chest_low_cm"]) > 3.0):
        out.append("not resting")
    if "eye_over_receiver_cm" in r:
        lo, hi = SIGHT[name][1]
        if r["eye_over_receiver_cm"][0] < lo * 100 or r["eye_over_receiver_cm"][1] > hi * 100:
            out.append("eye off the sights")
        if r["sight_blocked_cm"] > 0.0:
            out.append("sight line blocked")
    if r.get("pair_overlap_cm", 0.0) > PAIR_OVERLAP * 100:
        out.append("crew overlap")
    for k, v in r.get("duck_flags", {}).items():
        if v:
            out.append(k)
    return out


# ================================================================================================ pictures
PROP_RGB = {"gun": (58, 60, 56), "grip": (90, 60, 40), "mount": (92, 88, 66), "box": (96, 102, 64),
            "round": (156, 146, 96)}


def box_tris(cc, half, R):
    s = np.array([[x, y, z] for x in (-1, 1) for y in (-1, 1) for z in (-1, 1)], float) * np.asarray(half)
    V = cc + s @ R.T
    F4 = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    return [V[list(f)] for f in F4]


def render(img, D, man, c, spec, name, t, view, rect, scale, centre, pal):
    """One cell: the near mesh skinned by the clip at t, the stand-in props as boxes, flat-shaded, painter's order,
    orthographic from `view` (azimuth, elevation in degrees; azimuth from +x toward +y)."""
    from PIL import Image, ImageDraw
    x_at, y_at, w, h = rect
    cellim = Image.new("RGB", (int(w), int(h)), (232, 226, 214))
    dr = ImageDraw.Draw(cellim)
    rect = (0, 0, w, h)
    az, el = map(math.radians, view)
    cvec = np.array((math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el)))
    r = unit(np.cross(cvec, Z_))
    up = np.cross(r, cvec)
    x0, y0, w, h = rect
    G, X = gore.pose(man.rig, c, t)
    M = D["near"]
    P = np.array(M["points"]).reshape(-1, 3)
    a, b, ww = np.array(M["part"]), np.array(M["part2"]), np.array(M["w2"], float)
    V = gore.skin(P, a, b, ww, G, X, man.H)
    T = np.array(M["triangles"]).reshape(-1, 3)
    paints = np.array(M["paints"])
    shade = np.array(M.get("shade", [1.0] * len(paints)), float)
    light = unit((0.4, -0.5, 0.75))

    def proj(Q):
        Q = np.asarray(Q) - centre
        return np.stack((x0 + w / 2 + (Q @ r) * scale, y0 + h / 2 - (Q @ up) * scale), -1)

    polys = []
    # (the ground: a light square under him, drawn first)
    g = np.array(((-1.6, -1.6, 0), (2.4, -1.6, 0), (2.4, 1.6, 0), (-1.6, 1.6, 0)), float) + np.array((centre[0], centre[1], 0))
    polys.append((-1e9, [tuple(p) for p in proj(g)], (198, 190, 168)))
    tri = V[T]
    n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    lit = 0.5 + 0.5 * np.abs(n @ light)
    depth = tri.mean(1) @ cvec
    S2 = proj(tri.reshape(-1, 3)).reshape(-1, 3, 2)
    for k in range(len(T)):
        p = paints[T[k, 0]]
        col = pal.get(int(p), "808080")
        rgb = tuple(int(col[i:i + 2], 16) for i in (0, 2, 4))
        f = lit[k] * float(shade[T[k]].mean())
        polys.append((depth[k], [tuple(q) for q in S2[k]], tuple(int(min(255, v * f)) for v in rgb)))
    for part, cc, half, Rb in prop_shapes_at(name, c, spec, t):
        for quad in box_tris(cc, half, Rb):
            nn = unit(np.cross(quad[1] - quad[0], quad[2] - quad[0]))
            f = 0.55 + 0.45 * abs(nn @ light)
            polys.append((float(quad.mean(0) @ cvec), [tuple(q) for q in proj(quad)],
                          tuple(int(v * f) for v in PROP_RGB.get(part, (90, 90, 90)))))
    polys.sort(key=lambda p: p[0])
    for _, pts, col in polys:
        dr.polygon(pts, fill=col)
    img.paste(cellim, (int(x_at), int(y_at)))


def pictures(stage_dir, out_dir, names, src):
    from PIL import Image, ImageDraw
    sys.path.insert(0, HERE)
    import gore_check
    os.makedirs(out_dir, exist_ok=True)
    paths = []
    for nm in names:
        D = json.load(open(os.path.join(src, nm + ".json")))
        staged = json.load(open(os.path.join(stage_dir, nm + ".crew_clips.json")))
        man = crew_man(D)
        pal = gore_check.palette(nm, D)
        cols, cell, lab = 8, 230, 30
        # (each row: a clip at four keys, each from two sides; mortar_load in two rows, through the grab, the lift, the
        # drop, the duck, the shot and the rise)
        TS = {"hmg_fire_prone": [[0.0, 0.1, 0.2, 0.8]], "hmg_fire_sit": [[0.0, 0.1, 0.2, 0.8]],
              "sit_down": [[0.0, 0.93, 1.6, 2.4]], "sit_up": [[0.8, 1.6, 2.27, 3.2]],
              "mortar_load": [[0.0, T_GRAB, 1.33, T_UP], [DROP_T, DROP_T + DUCK_T * 0.6, FIRE_T, 2.9]],
              "mortar_sight": [[0.0, 0.73, 1.93, 2.27]]}
        rows = [(name, ts) for name in NAMES if name in staged
                for ts in TS.get(name, [[0.0, staged[name]["long"] / 4, staged[name]["long"] / 2,
                                         3 * staged[name]["long"] / 4]])]
        img = Image.new("RGB", (cols * cell, len(rows) * (cell + lab) + 40), (232, 226, 214))
        dr = ImageDraw.Draw(img)
        dr.text((8, 8), f"{nm}: crew clips (crew_clips.py), the near mesh skinned by each clip, stand-in props as boxes;"
                        f" four keys a row, each from two sides (the mortar's from square across its tube's plane)",
                fill=(30, 26, 20))
        fits = fits_of(stage_dir, nm)
        made = {}
        for ri, (name, ts) in enumerate(rows):
            c = staged[name]
            if name not in made:
                with tuned(fits.get(name)):
                    made[name] = MAKERS[name](man)
            sp = made[name]
            ts = [round(round(x * RATE) / RATE, 4) for x in ts]
            views = [(-40.0, 28.0), (90.0, 10.0)]
            if name in ("mortar_load", "mortar_sight"):
                # (square across the plane the tube leans in: from either side, so neither the tube nor the man hides
                # the other along it)
                u = yaw(c["prop"]["turn_deg"]) @ _U
                az = math.degrees(math.atan2(u[1], u[0]))
                views = [(az - 90.0, 15.0), (az + 90.0, 12.0)]
            if name == "hmg_feed":
                views = [(-30.0, 35.0), (160.0, 30.0)]
            # (a row's scale and middle from its first pose and props)
            G, X = gore.pose(man.rig, c, ts[0])
            pts = [X]
            for part, cc, half, Rb in prop_shapes_at(name, c, sp, ts[0]):
                pts.append(np.array([cc]))
            allp = np.concatenate(pts)
            centre = 0.5 * (allp.min(0) + allp.max(0))
            span = float(np.max(allp.max(0) - allp.min(0))) + 0.5
            scale = (cell - 16) / span
            for ci in range(cols):
                t = ts[ci // 2]
                view = views[ci % 2]
                x0, y0 = ci * cell, 40 + ri * (cell + lab)
                render(img, D, man, c, sp, name, t, view, (x0, y0, cell, cell), scale, centre, pal)
                dr.text((x0 + 4, y0 + cell + 2), f"{name}\n t {t:.2f} s  view {int(round(view[0]))}/{int(view[1])}",
                        fill=(40, 34, 28))
        p = os.path.join(out_dir, f"{nm}_crew_clips.png")
        img.save(p)
        paths.append(p)
        print("wrote", p, flush=True)
    return paths


# ================================================================================================ stage, check, deliver
def rig_print(man):
    """A short fingerprint of a file's skeleton (bone names, parents, rest heads to 0.1 mm): staged clips fit only it."""
    import hashlib
    blob = json.dumps([list(man.rig.names), [-1 if p is None else int(p) for p in man.par], np.round(man.H, 4).tolist()])
    return hashlib.sha1(blob.encode()).hexdigest()[:16]


def record(dirpath, nm, res, rig, fit):
    """One staged file's rows put into DIR/crew_checks.json (its skeleton's print, the tunables its clips needed, their
    checks), under a lock and re-read inside it, so files can be staged a few at a time or by several runs at once."""
    import fcntl
    path = os.path.join(dirpath, "crew_checks.json")
    with open(os.path.join(dirpath, ".crew_checks.lock"), "w") as lk:
        fcntl.flock(lk, fcntl.LOCK_EX)
        J = json.load(open(path)) if os.path.exists(path) else {}
        for key, val in (("rigs", rig), ("fits", fit), ("checks", res)):
            J.setdefault(key, {})[nm] = val
        tmp = f"{path}.{os.getpid()}.tmp"
        with open(tmp, "w") as f:
            json.dump(J, f, indent=1)
        os.replace(tmp, path)


def stage(dirpath, names, src):
    """Stage each named file: DIR/<name>.crew_clips.json (the 10 clips), its rows in DIR/crew_checks.json (record)."""
    os.makedirs(dirpath, exist_ok=True)
    summary = {}
    for nm in names:
        path = os.path.join(src, nm + ".json")
        D = json.load(open(path))
        miss = [b for b in ("fire_prone", "kneel_down", "walk") if b not in D["clips"]]
        if miss:
            print(nm, "SKIPPED: no", miss)
            continue
        t0 = time.time()
        clips, specs, man = clips_for(D)
        res = {name: specs[name][0]["checks"] for name in clips}
        fit = {name: specs[name][0]["fit"] for name in clips if specs[name][0]["fit"]}
        tmp = os.path.join(dirpath, nm + ".crew_clips.json.tmp")
        with open(tmp, "w") as f:
            json.dump(clips, f, separators=(",", ":"))
        os.replace(tmp, os.path.join(dirpath, nm + ".crew_clips.json"))
        record(dirpath, nm, res, rig_print(man), fit)
        summary[nm] = res
        print(nm, f"{time.time() - t0:.1f} s", flush=True)
        for name, r in res.items():
            fl = flags(r, name)
            print(f"    {name:17s} " + " ".join(f"{k}={v}" for k, v in r.items() if k not in ("loop",)) +
                  (f"  !! {fl}" if fl else "") + (f"  fit {fit[name]}" if name in fit else ""), flush=True)
    return summary


def fits_of(dirpath, nm):
    """The tunables a staged file's clips were refitted with (crew_checks.json's `fits`)."""
    p = os.path.join(dirpath, "crew_checks.json")
    return json.load(open(p)).get("fits", {}).get(nm, {}) if os.path.exists(p) else {}


def check_staged(dirpath, names, src):
    """The checks of staged clips, remade from the file and their recorded fits; each file's `checks` rows in
    DIR/crew_checks.json replaced by these (its rigs and fits kept), when its skeleton is the one staged on. A clip
    that now breaks a limit is flagged (!!): stage that file again."""
    summary = {}
    for nm in names:
        D = json.load(open(os.path.join(src, nm + ".json")))
        staged = json.load(open(os.path.join(dirpath, nm + ".crew_clips.json")))
        man = crew_man(D)
        fits = fits_of(dirpath, nm)
        res = {}
        for name in staged:
            with tuned(fits.get(name)):
                sp = MAKERS[name](man)
                res.update(check_file(D, {name: staged[name]}, {name: (sp, None)}, man))
        if "hmg_fire_prone" in staged and "hmg_feed" in res:     # (the two-man crew at the one gun)
            d, where = pair_overlap(man, staged["hmg_fire_prone"], staged["hmg_feed"])
            res["hmg_feed"]["pair_overlap_cm"], res["hmg_feed"]["pair_overlap_at"] = round(d * 100, 2), where
        summary[nm] = res
        rigs = json.load(open(os.path.join(dirpath, "crew_checks.json"))).get("rigs", {})
        same = rigs.get(nm) == rig_print(man)
        if same:
            record(dirpath, nm, res, rigs[nm], fits)
        print(nm, "" if same else "(skeleton changed since staging: checks not recorded; stage it again)", flush=True)
        for name, r in res.items():
            fl = flags(r, name)
            print(f"    {name:17s} " + " ".join(f"{k}={v}" for k, v in r.items() if k not in ("loop",)) +
                  (f"  !! {fl}" if fl else ""), flush=True)
    return summary


def deliver(dirpath, names):
    """Each shared file read fresh, the staged crew clips added (nothing else touched), backed up first, written whole
    through a temporary file, and read back."""
    os.makedirs(BACKUP, exist_ok=True)
    rigs = json.load(open(os.path.join(dirpath, "crew_checks.json")))["rigs"]
    for nm in names:
        add = json.load(open(os.path.join(dirpath, nm + ".crew_clips.json")))
        assert sorted(add) == sorted(NAMES), (nm, sorted(add))
        path = SHARED + nm + ".json"
        raw = open(path).read()
        D = json.loads(raw)
        if rig_print(Man(D)) != rigs.get(nm):
            print(nm, "SKIPPED: its skeleton changed since staging; stage it again")
            continue
        bk = BACKUP + nm + ".json"
        if not os.path.exists(bk):
            with open(bk, "w") as f:
                f.write(raw)
        before = set(D["clips"])
        D["clips"].update(add)
        tmp = path + ".tmp"
        with open(tmp, "w") as f:
            json.dump(D, f, separators=(",", ":"))
        os.replace(tmp, path)
        back = json.load(open(path))
        ok = all(back["clips"].get(k) == v for k, v in add.items()) and before <= set(back["clips"])
        print("added", sorted(add), "to", nm, "read back", "ok" if ok else "MISMATCH")


if __name__ == "__main__":
    a = sys.argv[1:]
    src = SHARED
    if "--src" in a:
        i = a.index("--src")
        src = a[i + 1]
        del a[i:i + 2]
    if a and a[0] == "--stage":
        stage(a[1], a[2:] or INFANTRY, src)
    elif a and a[0] == "--check":
        check_staged(a[1], a[2:] or INFANTRY, src)
    elif a and a[0] == "--pictures":
        pictures(a[1], a[2], a[3:] or ["soldier_GER", "soldier_USA"], src)
    elif a and a[0] == "--deliver":
        deliver(a[1], a[2:] or INFANTRY)
    else:
        print(__doc__)
