"""The tank commander's clips for the Normandy tanks (the Sherman, the Panzer IV and the Tiger), made in crew.py's way
(the soldier's own rig, two-bone IK to the grips and the floor) and played on the tank crewmen's own files
(soldier_USA_tanker, soldier_GER_panzer). crew.py stays as it is: its clips and the measures it took are used here.

    python tank_crew.py --only check            the new clips (and commander_hatch) checked at the three tanks' hatches
    python tank_crew.py --only pictures [--fast] the proof pictures (pictures/tankers/)

The clips (root on the floor at his feet; where it goes from the tank's `commander` socket is each clip's place):

- commander_hatch (crew.py's): stood in the hatch, forearms on the rim, the glasses; his feet 0.92 under the socket.
- commander_roof_mg_us / _us_burst: the American up on the commander's seat (his feet 0.72 under the socket, the rim
  at his belt), both hands on the spade grips of the .50 on its mount over the turret's roof; and firing it.
- commander_roof_mg_ger / _ger_burst: the German up on his seat in the cupola, the MG 34 on the cupola's ring at his
  shoulder (right hand on the pistol grip, left hand under the receiver); and firing it.
- die_hatch_slump: hit in the hatch, he folds forward over the rim and stays there, arms hanging down the turret.
- die_hatch_drop: hit in the hatch, he drops back down inside; the event `hide` when the top of his head is below the
  rim (stop drawing him).

The roof guns' grips are the tanks' own `grip.roof_mg.l/.r` sockets (the tank modeller's parts, 22 November), read at
rest with the gun laid straight ahead; re-solve the clips if those sockets move.
"""

import json
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
os.environ.setdefault("CREW_WORK", "/home/user/art/tankers/crew/")

import crew as C  # noqa: E402  (soldier.py and bpy with it)
S = C.S
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix  # noqa: E402
from soldier import E, M4, R_axis, _n, add3, bump, osc, ramp, sm  # noqa: E402,F401

PICS = "/home/user/art/tankers/pictures/"

# ---- the three hatches (game coordinates: the `commander` socket's point; the rim 0.25 over it, his feet 0.92 under)
TANKS = {"Sherman": {"rim": 2.615}, "PanzerIV": {"rim": 2.625}, "Tiger": {"rim": 2.80}}
HATCH_OFF = (0.0, 0.0, -0.92)            # his feet from the socket, stood on the turret's floor (crew.py's)
SEAT_OFF = (0.0, 0.0, -0.72)             # up on the commander's seat or step: the rim at his belt (0.97 over his feet)
RIM_Z = C.RIM_Z                          # 1.17: the rim over his feet in the hatch
RIM_SEAT = 1.17 - 0.20                   # 0.97: the rim over his feet up on the seat

# The Sherman and the Tiger are made as the Panzer IV is: the turret a closed shell, the hatch's hole painted on it
# (a black disc level with the rim's top), the ring standing on the roof round it. So, as crew.py does for the Panzer
# IV: the tank's mesh counts against him only over the rim, and the ring as the ring it is (from the foot of its inner
# wall up to the rim: how far he goes into it). The rings, measured from the files (m from the socket's axis):
RINGS = {
    "PanzerIV": {"inner": ((-0.335, -0.045, 0.345), (-0.045, 0.005, 0.282)), "outer": 0.345},
    "Sherman": {"inner": ((-0.055, 0.005, 0.300),), "outer": 0.36},
    "Tiger": {"inner": ((-0.215, -0.035, 0.36), (-0.035, 0.005, 0.31)), "outer": 0.41},
}
for _v, _d in TANKS.items():
    if _v != "PanzerIV":
        C.STAGES[("vehicle", _v)] = {"open_up": True, "cupola": True,
                                     "keep": (lambda rim: (lambda W: W[:, 2] > rim + 0.005))(_d["rim"])}
_SOCK = {}


def _ring_of(Mw):
    """Which tank's ring a root matrix stands in (by the socket's point under it)."""
    if not _SOCK:
        for v in TANKS:
            _SOCK[v] = np.array(C.sockets_of("vehicle", v)["commander"]["at"], float)
    xy = np.asarray(Mw)[:2, 3]
    return min(TANKS, key=lambda v: float(np.linalg.norm(_SOCK[v][:2] - xy)))


def cupola_check(clip, Mw, step=6):
    """crew.cupola_check for any of the three rings: the deepest he goes into it over a clip (m, (t, bone))."""
    v = _ring_of(Mw)
    ring, rim = RINGS[v], TANKS[v]["rim"]
    centre = tuple(_SOCK[v][:2])
    inner = tuple((rim + a, rim + b, r) for a, b, r in ring["inner"])
    c = C.CREW[clip]
    rig, skin = C.MODEL["rig"], C.MODEL["skin"]
    Mw = np.asarray(Mw)
    worst, at = 0.0, None
    for f in range(0, int(round(c["long"] * C.FPS)) + 1, step):
        pts, bones = skin.points(rig.solve(c["fn"](f / C.FPS)))
        wp = pts @ Mw[:3, :3].T + Mw[:3, 3]
        d, p = C.cupola_gaps(wp, centre=centre, rim=rim, inner=inner, outer=ring["outer"])
        if d > worst:
            worst, at = d, (round(f / C.FPS, 3), str(bones[int(np.argmin(np.linalg.norm(wp - p, axis=1)))]))
    return worst, at


C.cupola_check = cupola_check
C.CREW["commander_hatch"]["place"] += [("vehicle", "Sherman", "commander", HATCH_OFF), ("vehicle", "Tiger", "commander", HATCH_OFF)]


def _places(off):
    return [("vehicle", v, "commander", off) for v in ("Sherman", "PanzerIV", "Tiger")]


def stood(t, long, raised=1.0):
    """The body stood up in a hatch (crew.py's commander, without his hands): breathing, a little sway."""
    br = C.wave(t, long, 3, 0.2)
    sway = C.wave(t, long, 2, 0.1)
    P = {"pelvis_at": (0.0, 0.0, 0.0), "pelvis": (3.0, 0.5 * sway, 0.0), "spine_01": (3.0, 0.0, 0.0),
         "spine_02": (4.0 - 0.4 * br, -0.3 * sway, 0.0), "chest": (5.0 - 0.8 * br, -0.2 * sway, 0.0),
         "clavicle_l": (7.0 + 1.0 * br, 3.0), "clavicle_r": (7.0 + 1.0 * br, 3.0), "neck": (-3.0, 0.0, 0.0), "head": (-2.0, 0.0, 0.0),
         "foot_l": {"at": (0.03, 0.12, C.MAN["ankle"]), "yaw": 8.0}, "foot_r": {"at": (-0.04, -0.12, C.MAN["ankle"]), "yaw": -8.0},
         "weapon": C.stowed(), "weapon_hidden": True, "fingers_l": 52.0, "fingers_r": 52.0, "thumb_l": 30.0, "thumb_r": 30.0}
    return P


# ================================================================================================ the roof guns
# The guns are the tanks' own (the tank modeller's roof_mg parts, 22 Nov): the Sherman's .50 on a post in front of the
# commander's hatch, its two spade grips upright (sockets grip.roof_mg.l/.r); the Panzer IV's and the Tiger's MG 34 on
# the cupola's ring rail, its butt over the cupola's middle (grip.roof_mg.l the top of the stock for his left hand,
# grip.roof_mg.r the pistol grip). Read from the files at rest (gun ahead, lay 0). Until the files go live they are read
# from the staging folder (land-next); TANKS_FROM names another.
NEXT_VEH = "/mnt/project-files/battle-models/land-next/land/vehicles/"


def _has_grips(folder, v):
    try:
        return any(s["name"] == "grip.roof_mg.l" for s in json.load(open(folder + v + ".json"))["sockets"])
    except (OSError, ValueError, KeyError):
        return False


if os.environ.get("TANKS_FROM"):
    C.VEH = os.environ["TANKS_FROM"]
elif not _has_grips(C.VEH, "Sherman") and _has_grips(NEXT_VEH, "Sherman"):
    C.VEH = NEXT_VEH
GUN_OF = {"Sherman": "us", "PanzerIV": "ger", "Tiger": "ger"}
_GUN = {}


def roof_gun(v):
    """The roof gun of tank v at rest, from the commander socket (Blender's axes): its grips, and (the MG 34) the
    middle of its butt plate, the back face of the stock (the gun part's rearmost points)."""
    if v in _GUN:
        return _GUN[v]
    d = C.piece_json("vehicle", v)
    socks = {s["name"]: s for s in d["sockets"]}
    o = np.array(socks["commander"]["at"], float)
    g = {s: C.g2b(np.array(socks["grip.roof_mg." + s]["at"], float) - o) for s in ("l", "r")}
    out = {"grip_l": g["l"], "grip_r": g["r"]}
    names = [p["name"] for p in d["parts"]]
    cr = names.index("roof_mg_cradle")
    P = np.asarray(d["near"]["points"], float).reshape(-1, 3)
    Q = P[np.asarray(d["near"]["part"]) == cr] - o
    back = Q[Q[:, 0] < Q[:, 0].min() + 0.004]
    out["butt"] = C.g2b(np.array((back[:, 0].min(), 0.5 * (back[:, 1].min() + back[:, 1].max()),
                                  0.5 * (back[:, 2].min() + back[:, 2].max()))))
    _GUN[v] = out
    return out


MG50_ROUNDS = [round(0.12 * k, 3) for k in range(6)]       # the .50: about 500 a minute, six-round bursts
MG34_ROUNDS = [round(0.067 * k, 3) for k in range(9)]      # the MG 34: about 900 a minute, nine-round bursts


def roof_body(t, long, lean=6.0):
    """Up on his seat, the rim at his belt, leaning a little into the gun."""
    P = stood(t, long)
    P["pelvis"] = add3(P["pelvis"], (2.0, 0.0, 0.0))
    P["spine_02"] = add3(P["spine_02"], (0.5 * lean, 0.0, 0.0))
    P["chest"] = add3(P["chest"], (0.5 * lean, 0.0, 0.0))
    P["neck"], P["head"] = (-4.0, 0.0, 0.0), (-3.0, 0.0, 0.0)
    P["clavicle_l"], P["clavicle_r"] = (3.0, 6.0), (3.0, 6.0)
    return P


def mg50_hands(P, jolt=0.0):
    """Both hands on the .50's spade grips, upright handles either side of the butterfly trigger: each fist round its
    handle, the knuckles ahead and a little in, the thumbs up over the trigger, the elbows out and down."""
    G = roof_gun("Sherman")
    for s, side in (("l", 1.0), ("r", -1.0)):
        g = G["grip_" + s] - C.g2b(SEAT_OFF) + np.array((-0.004 * jolt, 0.0, 0.0))
        tdir = np.array((0.0, 0.0, 1.0))                         # (the handle, from the heel of the hand to the first finger)
        a = _n(np.array((1.0, -0.2 * side, 0.0)))
        n = np.cross(a, tdir) if s == "l" else np.cross(tdir, a)
        P["hand_" + s] = {"at": tuple(g), "a": tuple(a), "n": tuple(n), "pole_world": (-0.6, 1.0 * side, -0.8)}
        P["fingers_" + s], P["thumb_" + s] = 66.0, 20.0
    return P


# The German: stood on the turret's floor (his feet 0.92 under the socket, as in the hatch), bladed to the right as a
# rifleman stands, the butt in his right shoulder's pocket. The gun cannot move to him, so he is moved to it: each tank's
# root_offset puts his shoulder's pocket (measured on the man, fit_roof) on the middle of that tank's butt plate.
ROOF_FIT = {}
BLADE = {"turn": -60.0, "back": 16.0, "hips_fwd": 0.0}   # (his turn to the right, the lean back of his hips, the hips forward)
POCKET = (0.045, -0.03)          # (m: the pocket from the shoulder joint, in toward his middle and down, in the chest's axes)


def mg34_body(t, long):
    """Stood in the cupola, bladed a third of a turn to the right, leaning into the gun, the right shoulder up to the
    butt; the head turned back to look along the gun."""
    br = C.wave(t, long, 3, 0.2)
    sway = C.wave(t, long, 2, 0.1)
    B = BLADE
    P = stood(t, long)
    tw = B["turn"]
    P.update({"pelvis_at": (B["hips_fwd"], 0.0, ROOF_FIT.get("drop", 0.0)), "pelvis": (3.0 - B["back"], 0.4 * sway, 0.7 * tw),
              "spine_01": (3.0, 0.0, 0.1 * tw), "spine_02": (4.0 - 0.4 * br, -0.3 * sway, 0.1 * tw),
              "chest": (6.0 - 0.8 * br, -2.0, 0.1 * tw), "neck": (2.0, -3.0, -0.45 * tw), "head": (4.0, -8.0, -0.5 * tw),
              "clavicle_l": (2.0, 10.0), "clavicle_r": (16.0 + 1.0 * br, 10.0),
              "foot_l": {"at": (0.16, 0.13, C.MAN["ankle"]), "yaw": -14.0},
              "foot_r": {"at": (-0.17, -0.11, C.MAN["ankle"]), "yaw": -48.0}})
    return P


def _pocket(st):
    """His right shoulder's pocket (where a butt sits) in the clip's root frame: in from the shoulder joint and down a
    little, on the front of his body there (the man's own skin)."""
    Dc, A = st["D"]["chest"], st["H"]["upperarm_r"]
    pts, _ = C.MODEL["skin"].points(st)
    L = (pts - A) @ Dc                                   # (each point in the chest's axes from the joint)
    win = (np.abs(L[:, 1] - POCKET[0]) < 0.02) & (np.abs(L[:, 2] - POCKET[1]) < 0.02) & (L[:, 0] > 0.0)
    front = float(np.sort(L[win, 0])[-3:].mean()) if win.sum() >= 3 else 0.06
    return A + Dc @ np.array((front, POCKET[0], POCKET[1]))


def fit_roof():
    """Measure the man the clips are played on: where his shoulder's pocket comes in mg34_body, and so where he
    stands in each German tank (its root_offset), and his hands on the grips (from the butt, the same on both guns)."""
    rig = C.MODEL.get("rig")
    if rig is None or C.MODEL.get("skin") is None:
        return None
    ROOF_FIT.clear()
    G = roof_gun("PanzerIV")
    want_z = float(G["butt"][2]) - C.g2b(HATCH_OFF)[2]       # (the butt's middle over his feet, stood on the floor)
    drop = 0.0
    for _ in range(3):
        ROOF_FIT["drop"] = drop
        P = mg34_body(0.0, 4.0)
        P["hand_l"] = {"arm": (10.0, 60.0, 0.0, 30.0, 0.0)}
        P["hand_r"] = {"arm": (10.0, 60.0, 0.0, 30.0, 0.0)}
        K = _pocket(rig.solve(P))
        drop += want_z - float(K[2])
        if abs(want_z - float(K[2])) < 0.002:
            break
    ROOF_FIT.update(drop=drop, pocket=K)
    for v in ("PanzerIV", "Tiger"):
        Gv = roof_gun(v)
        off_b = Gv["butt"] - K                               # (the root from the socket, Blender's axes)
        off_b[2] = C.g2b(HATCH_OFF)[2]
        ROOF_FIT[v] = tuple(round(float(x), 4) for x in C.g2b(off_b))
    for name in ("commander_roof_mg_ger", "commander_roof_mg_ger_burst"):
        if name in C.CREW:
            C.CREW[name]["place"] = [("vehicle", v, "commander", ROOF_FIT[v]) for v in ("PanzerIV", "Tiger")]
    print("ROOF_FIT", {k: (np.round(v, 4).tolist() if isinstance(v, np.ndarray) else v) for k, v in ROOF_FIT.items()}, flush=True)
    return ROOF_FIT


_crew_setup = C.setup


def _setup_fit(*a, **k):
    m = _crew_setup(*a, **k)
    fit_roof()
    return m


C.setup = _setup_fit


def mg34_hands(P, jolt=0.0):
    """Right hand round the pistol grip (the first finger on the trigger), left hand under the stock holding the butt
    up into his shoulder."""
    G = roof_gun("PanzerIV")
    K = ROOF_FIT.get("pocket")
    if K is None:
        K = G["butt"] - C.g2b((-0.17, -0.16, HATCH_OFF[2]))
    kick = np.array((-0.010 * jolt, 0.0, 0.0))
    # the pistol grip: its middle, 2.2 cm under the socket's point (the top of the grip) and back along its rake
    g_r = K + (G["grip_r"] - G["butt"]) + np.array((-0.012, 0.0, -0.022)) + kick
    gd = _n(np.array((-0.37, 0.0, -0.93)))                       # (down the grip)
    a_r = _n(np.array((1.0, 0.12, 0.0)) - gd * (gd @ np.array((1.0, 0.12, 0.0))))
    n_r = np.cross(-gd, a_r)                                     # (the palm to his left, round the grip)
    P["hand_r"] = {"at": tuple(g_r), "a": tuple(a_r), "n": tuple(n_r), "pole_world": (-0.3, -1.0, -0.8)}
    # the stock: the hand under it where the socket is over it, palm up, the fingers round its right side, holding
    # it up into his shoulder (over the top, his elbow had to go up over his head: the stock is close in front of him)
    g_l = K + (G["grip_l"] - G["butt"]) + np.array((0.01, 0.0, -0.075)) + kick
    a_l = _n(np.array((0.3, -0.95, 0.15)))
    n_l = _n(np.array((0.0, 0.2, 1.0)))
    P["hand_l"] = {"at": tuple(g_l), "a": tuple(a_l), "n": tuple(_n(n_l - a_l * (a_l @ n_l))), "pole_world": (0.0, 0.4, -1.0)}
    P["fingers_r"], P["thumb_r"], P["fingers_l"], P["thumb_l"] = 70.0, 40.0, 50.0, 25.0
    return P


@C.crew_clip("commander_roof_mg_us", 4.0, loop=True, anchor="feet", weapon="none", place=[("vehicle", "Sherman", "commander", SEAT_OFF)],
             about="The American commander up on his seat in the hatch, the rim at his belt, both hands on the spade grips "
                   "of the .50 on its mount over the turret's roof: watching over it, ready.")
def c_roof_mg_us(t):
    P = roof_body(t, 4.0)
    look = 14.0 * bump(t, 0.6, 1.4, 2.2) - 10.0 * bump(t, 2.3, 3.0, 3.7)
    P["neck"] = add3(P["neck"], (0.0, 0.0, 0.45 * look))
    P["head"] = add3(P["head"], (0.0, 0.0, 0.55 * look))
    return mg50_hands(P)


@C.crew_clip("commander_roof_mg_us_burst", 1.2, loop=True, anchor="feet", weapon="none", place=[("vehicle", "Sherman", "commander", SEAT_OFF)],
             events=[{"t": t0, "fx": "muzzle_flash", "socket": "roof_mg (the vehicle's)", "cue": ""} for t0 in MG50_ROUNDS],
             about="Firing the .50: each of six rounds jolts his shoulders and head, his hands staying on the grips.")
def c_roof_mg_us_burst(t):
    P = roof_body(0.0, 4.0)
    j = C.burst_jolt(t, MG50_ROUNDS, decay=0.04)
    on = ramp(t, 0.0, 0.05) * (1 - ramp(t, 0.85, 1.1))
    P["chest"] = add3(P["chest"], (3.0 * on - 2.4 * j, 0.0, 0.4 * j * osc(t, 0.13)))
    P["spine_02"] = add3(P["spine_02"], (2.0 * on - 1.0 * j, 0.0, 0.0))
    P["neck"] = add3(P["neck"], (2.0 * on + 1.5 * j, 0.0, 0.0))
    P["head"] = add3(P["head"], (2.0 * on - 2.5 * j, 0.6 * j * osc(t, 0.11), 0.0))
    P["clavicle_l"] = add3(P["clavicle_l"], (1.2 * j, -2.0 * j))[:2]
    P["clavicle_r"] = add3(P["clavicle_r"], (1.2 * j, -2.0 * j))[:2]
    return mg50_hands(P, j)


@C.crew_clip("commander_roof_mg_ger", 4.0, loop=True, anchor="feet", weapon="none", place=[("vehicle", v, "commander", (-0.15, -0.16, HATCH_OFF[2])) for v in ("PanzerIV", "Tiger")],
             about="The German commander up on his seat in the cupola, the MG 34 on the cupola's ring at his shoulder: right "
                   "hand on the pistol grip, left hand under the receiver, his cheek on the stock; watching along the sights.")
def c_roof_mg_ger(t):
    P = mg34_body(t, 4.0)
    return mg34_hands(P)


@C.crew_clip("commander_roof_mg_ger_burst", 1.0, loop=True, anchor="feet", weapon="none", place=[("vehicle", v, "commander", (-0.15, -0.16, HATCH_OFF[2])) for v in ("PanzerIV", "Tiger")],
             events=[{"t": t0, "fx": "muzzle_flash", "socket": "roof_mg (the vehicle's)", "cue": ""} for t0 in MG34_ROUNDS],
             about="Firing the MG 34: nine rounds at its fast rate push the butt back into his shoulder, his head with it.")
def c_roof_mg_ger_burst(t):
    P = mg34_body(0.0, 4.0)
    j = C.burst_jolt(t, MG34_ROUNDS, decay=0.03)
    P["chest"] = add3(P["chest"], (-1.6 * j, 0.0, 0.3 * j * osc(t, 0.09)))
    P["spine_02"] = add3(P["spine_02"], (-0.6 * j, 0.0, 0.0))
    P["head"] = add3(P["head"], (-1.5 * j, 0.5 * j * osc(t, 0.07), 0.0))
    P["clavicle_r"] = add3(P["clavicle_r"], (0.8 * j, -2.5 * j))[:2]
    return mg34_hands(P, j)


# ================================================================================================ the deaths
def hang(theta, s, spread=8.0, elbow=18.0):
    """An arm let go: hanging the way the world's down is from a chest pitched theta degrees forward."""
    return {"arm": (max(5.0, theta - 4.0), 90.0 - spread, 0.0, elbow, 10.0)}


def holding(t0=0.0):
    """The commander's pose at t0 of commander_hatch (the glasses low on the rim under his hands)."""
    return C.c_commander(t0)


def let_go(P, H, w):
    for s in ("l", "r"):
        P["hand_" + s] = {"blend": [(1.0 - w, H["hand_" + s]), (w, P["hand_" + s])]} if w < 0.999 else P["hand_" + s]
    return P


def falling_glasses(H, t, t0, shown_until, drop=0.45):
    """The binoculars out of his hands at t0, falling straight down into the turret and gone."""
    B = np.array(H["props"]["binoculars"], float)
    u = max(0.0, t - t0)
    z = -min(drop, 4.9 * u * u)
    M = B.copy()
    M[:3, 3] = B[:3, 3] + np.array((-0.05 * min(1.0, u / 0.3), 0.0, z))
    M[:3, :3] = R_axis((1, 0, 0), 60.0 * min(1.0, u / 0.4)) @ B[:3, :3]
    return (M, t < shown_until)


SLUMP_LONG = 2.4
SLUMP_YAW = 30.0           # (degrees to his left that he folds over the rim)


@C.crew_clip("die_hatch_slump", SLUMP_LONG, loop=False, anchor="feet", weapon="none", place=_places(HATCH_OFF),
             about="Hit stood in the hatch: his head snaps back, his knees go and he folds forward over the rim's front-left "
                   "(clear of a roof gun's mount ahead of him), the chest on it, head and arms hanging down the turret; he stays "
                   "there. The glasses drop inside.")
def c_die_hatch_slump(t):
    H = holding(0.0)
    jerk = bump(t, 0.0, 0.06, 0.22)
    f = sm(ramp(t, 0.12, 1.05))                       # (the fold)
    settle = bump(t, 1.0, 1.15, 1.45)                 # (the last give as he comes to rest)
    P = stood(0.0, 10.0)
    # (his knees give only a little: he hangs by his chest on the rim's front, the lower spine leaned about 40 degrees
    # so that his belly comes onto the lip, the chest and head bent on over it, outside)
    P["pelvis_at"] = (0.035 * f, 0.0, -0.04 * f - 0.01 * settle)
    P["pelvis"] = (3.0 + 8.0 * f, 0.0, 0.0)
    P["spine_01"] = (3.0 + 12.0 * f, 0.0, 0.0)
    P["spine_02"] = (4.0 - 4.0 * jerk + 16.0 * f, 1.5 * f, 0.0)
    P["chest"] = (5.0 - 6.0 * jerk + 24.0 * f + 3.0 * settle, -2.0 * f, 3.0 * f)
    P["neck"] = (-3.0 - 10.0 * jerk + 26.0 * f, 0.0, 6.0 * f)
    P["head"] = (-2.0 - 12.0 * jerk + 34.0 * f + 4.0 * settle, 8.0 * f, 12.0 * f)
    P["clavicle_l"] = (7.0 + 4.0 * jerk - 10.0 * f, 3.0 + 6.0 * f)
    P["clavicle_r"] = (7.0 + 4.0 * jerk - 10.0 * f, 3.0 + 6.0 * f)
    P["foot_l"] = dict(P["foot_l"], pole=(1.0, 0.3, 0.0))
    P["foot_r"] = dict(P["foot_r"], pole=(1.0, -0.3, 0.0))
    # (he goes over the rim's front-left, not straight ahead: the roof guns' mounts stand at the front of the hatch)
    yaw = SLUMP_YAW * f
    P["pelvis"] = add3(P["pelvis"], (0.0, 0.0, 0.6 * yaw))
    P["spine_02"] = add3(P["spine_02"], (0.0, 0.0, 0.2 * yaw))
    P["chest"] = add3(P["chest"], (0.0, 0.0, 0.2 * yaw))
    theta = 3.0 + 8.0 * f + 3.0 + 12.0 * f + 4.0 - 4.0 * jerk + 16.0 * f + 5.0 - 6.0 * jerk + 24.0 * f
    # (the forearms slide forward off the rim as he folds and the arms flop down outside the ring, the hands against
    # its side a little over the roof: the Tiger's roof is 0.96 over his feet, the Panzer IV's 0.83, the Sherman's 0.9)
    Ry = R_axis((0, 0, 1), SLUMP_YAW)
    for s, side in (("l", 1.0), ("r", -1.0)):
        P["hand_" + s] = {"at": tuple(Ry @ np.array((0.60, side * (0.25 - 0.01 * side), 0.99 - 0.015 * (1 - side)))),
                          "a": tuple(Ry @ np.array((0.35, 0.1 * side, -1.0))), "n": tuple(Ry @ np.array((-1.0, 0.0, -0.35))),
                          "pole_world": tuple(Ry @ np.array((0.0, 0.6 * side, -1.0)))}
    P = let_go(P, H, sm(ramp(t, 0.25, 1.0)))
    P["fingers_l"] = P["fingers_r"] = 52.0 - 30.0 * sm(ramp(t, 0.05, 0.6))
    P["thumb_l"] = P["thumb_r"] = 30.0 - 15.0 * sm(ramp(t, 0.05, 0.6))
    P["props"] = {"binoculars": falling_glasses(H, t, 0.08, 0.5)}
    return P


DROP_LONG = 1.6
DROP_HIDE = 0.85


@C.crew_clip("die_hatch_drop", DROP_LONG, loop=False, anchor="feet", weapon="none", place=_places(HATCH_OFF),
             events=[{"t": DROP_HIDE, "fx": "hide", "socket": None, "cue": ""}],
             about="Hit stood in the hatch: his head snaps back and he drops straight down inside, his arms sliding off "
                   "the rim after him; the event `hide` (at 0.85 s) is when the top of his head is under the rim: stop "
                   "drawing him there (the rest of the clip is him slumped on the turret's floor, out of sight).")
def c_die_hatch_drop(t):
    H = holding(0.0)
    jerk = bump(t, 0.0, 0.06, 0.24)
    d = sm(ramp(t, 0.1, 0.8))                        # (the drop)
    curl = sm(ramp(t, 0.35, 1.2))                    # (slumping as he comes down)
    P = stood(0.0, 10.0)
    P["pelvis_at"] = (-0.06 * d, 0.0, -0.70 * d)
    P["pelvis"] = (3.0 - 14.0 * d + 10.0 * curl, 0.0, 0.0)
    P["spine_01"] = (3.0 + 14.0 * curl, 0.0, 0.0)
    P["spine_02"] = (4.0 - 5.0 * jerk + 22.0 * curl, 0.0, 0.0)
    P["chest"] = (5.0 - 8.0 * jerk + 18.0 * curl, 3.0 * curl, 0.0)
    P["neck"] = (-3.0 - 12.0 * jerk + 20.0 * curl, 0.0, 0.0)
    P["head"] = (-2.0 - 14.0 * jerk + 22.0 * curl, -6.0 * curl, 8.0 * curl)
    P["clavicle_l"] = (7.0 + 6.0 * d - 4.0 * curl, 3.0)
    P["clavicle_r"] = (7.0 + 6.0 * d - 4.0 * curl, 3.0)
    P["foot_l"] = dict(P["foot_l"], pole=(1.0, 0.4, 0.2))
    P["foot_r"] = dict(P["foot_r"], pole=(1.0, -0.4, 0.2))
    theta = 3.0 - 14.0 * d + 10.0 * curl + 3.0 + 14.0 * curl + 4.0 - 5.0 * jerk + 22.0 * curl + 5.0 - 8.0 * jerk + 18.0 * curl
    up = sm(ramp(t, 0.03, 0.16)) * (1.0 - sm(ramp(t, 0.55, 0.95)))
    for s in ("l", "r"):
        # (thrown up off the rim by the hit, so that they come in over its lip; up through the hole as the body falls
        # out from under them; then hanging)
        P["hand_" + s] = {"arm": (max(10.0, (1.0 - up) * (theta + 6.0) + up * 115.0), 70.0 - 25.0 * up, 0.0, 35.0 - 10.0 * curl, 10.0)}
    P = let_go(P, H, sm(ramp(t, 0.06, 0.3)))
    P["fingers_l"] = P["fingers_r"] = 52.0 - 25.0 * sm(ramp(t, 0.06, 0.5))
    P["thumb_l"] = P["thumb_r"] = 30.0 - 12.0 * sm(ramp(t, 0.06, 0.5))
    P["props"] = {"binoculars": falling_glasses(H, t, 0.06, DROP_HIDE)}
    return P


NEW = ["commander_roof_mg_us", "commander_roof_mg_us_burst", "commander_roof_mg_ger", "commander_roof_mg_ger_burst",
       "die_hatch_slump", "die_hatch_drop"]
OURS = ["commander_hatch"] + NEW


# ================================================================================================ checks and pictures
def eye_of(piece, side=1.0, back=1.0):
    """A camera ahead of a tank's commander and off to one side (Blender's axes)."""
    s = C.sockets_of("vehicle", piece)["commander"]
    at = np.array(s["at"], float)
    tgt = at + np.array((0.0, 0.0, 0.55))
    return tuple(tgt + np.array((1.9 * back, -2.3 * side, 1.0))), tuple(tgt)


def add_pictures():
    for piece in TANKS:
        for clip, t in (("commander_hatch", 1.0), ("commander_hatch", 7.9), ("die_hatch_slump", SLUMP_LONG),
                        ("die_hatch_slump", 0.45), ("die_hatch_drop", 0.4), ("die_hatch_drop", 0.7),
                        ("commander_roof_mg_us", 0.0), ("commander_roof_mg_ger", 0.0)):
            eye, tgt = eye_of(piece, side=1.0 if piece != "Tiger" else -1.0)
            C.PICTURES[f"tc_{piece}_{clip}_{t}"] = {"kind": "vehicle", "piece": piece, "men": [(clip, t, "commander")],
                                                   "eye": eye, "target": tgt, "lens": 45, "about": f"{clip} at {t} s on the {piece}"}


def main(argv):
    only = set(argv[argv.index("--only") + 1].split(",")) if "--only" in argv else {"check"}
    clips = argv[argv.index("--clips") + 1].split(",") if "--clips" in argv else OURS
    os.makedirs(C.WORK, exist_ok=True)
    if "check" in only:
        keep_crew = dict(C.CREW)
        for k in list(C.CREW):
            if k not in clips:
                C.CREW.pop(k)
        res = C.check_everything("vehicle")
        C.CREW.clear()
        C.CREW.update(keep_crew)
        json.dump(res, open(os.path.join(C.WORK, "tank_checks.json"), "w"), indent=1)
        for name, rs in res.items():
            for r in rs:
                print("CHECK", name, r.get("at"), "deepest", r.get("deepest_m"), r.get("where"), "past 1.2 cm", r.get("points_past_1_2cm"),
                      "hand gap", r.get("hand_gap_m"))
    if "pictures" in only:
        add_pictures()
        os.makedirs(PICS, exist_ok=True)
        want = argv[argv.index("--pics") + 1].split(",") if "--pics" in argv else None
        for name in [n for n in C.PICTURES if n.startswith("tc_")]:
            if want and not any(w in name for w in want):
                continue
            C.build_picture(name, fast="--fast" in argv)


if __name__ == "__main__":
    main(sys.argv)
