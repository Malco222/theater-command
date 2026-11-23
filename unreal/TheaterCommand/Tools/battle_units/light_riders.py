"""Riders for the light vehicles (release 2: Brian's yes, 23 November 2026 07:01Z, to jeeps and motorcycles): the
seated, astride and standing clips a soldier plays on the thirteen light cars and motorcycles (battle-models/land/
vehicles: Jeep, GAZ67, Kubelwagen, Fiat508, Kurogane, FieldCar, R75, M72, Rikuo, WLA, M20, Alce, Motorcycle), made
the way crew.py makes the crew's (crew.py's solver and pattern man, its sit_body, on_rim, rifle_upright and on_rifle),
each fitted to the seats it is played at and checked there against the vehicle's own meshes.

    python light_riders.py --only clips,check,ref     make the clips on the pattern man (GER, crew.py's), check each at
                                                     every seat it is played at, and write the reference
                                                     (light_riders_ref.json: each bone's turn, the hands' and feet's
                                                     places and the rifle's, 10 or 15 a second)
    python light_riders.py --only pictures [--fast]  the pictures of the riders at their seats

light_riders_fit.py then lays the reference onto each infantry file's own skeleton (no Blender) and adds the clips to
the soldier files.

The clips (the game's axes; each clip's root is at its seat socket, `places`):
    car_drive, car_drive_b, car_drive_c       a car's driver, hands on the wheel at grip.driver.l/.r (the three wheel
                                              places of the six cars), feet on the pedals; gun put away
    car_ride, car_ride_back                   a man in a car's front or back seat, his rifle upright between his knees
    bike_astride, bike_astride_b              a motorcycle's rider astride the saddle, boots on foot.l/.r, hands on the
                                              bars (_b: the WLA's and the Rikuo's footboards); rifle slung
    bike_pillion .. bike_pillion_d            the man on the pillion (R75, M72, Rikuo, Alce: each its own handle and
                                              rests); rifle slung
    sidecar_gunner, sidecar_gunner_b          seated low in the sidecar, hands on grip.gunner.l/.r (M72 and Rikuo;
                                              the R75); gun put away
    sidecar_ride                              the plain Motorcycle's sidecar man (rider.2), no gun to hold
    jeep_gunner                               standing behind the Jeep's post, hands on grip.gunner.l/.r (root: the
                                              gunner socket, his feet); rifle slung
    die_seat_slump, die_ride_slump, die_bike_slump, die_pillion_slump, die_sidecar_slump, die_gunner_slump
                                              each seat's death, from its clip's first pose (`death` on each seat clip)
"""

import json
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
os.environ.setdefault("CREW_WORK", "/home/user/art/riders/crew/")

import crew as C  # noqa: E402  (soldier.py, bpy with it)
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix  # noqa: E402
from soldier import M4, R_axis, _n, add3, bump, ramp  # noqa: E402

S = C.S
WORK = os.environ.get("RIDERS_WORK", "/home/user/art/riders/")
REF = os.path.join(WORK, "light_riders_ref.json")
C.VEH = os.environ.get("RIDERS_VEH", C.VEH)       # (another shelf of the vehicles: a staged set, to fit the riders to it)
RATE = {"loop": 10, "death": 15}        # (keys a second: the seats' gentle loops, the deaths)
LOOP = 4.0                              # (s: every seat's loop)
DIE = 2.4                               # (s: every death; the last pose held from DIE_REST)
DIE_REST = 1.5

# ================================================================================================ the seats
_SO = {}


def socks(v):
    if v not in _SO:
        _SO[v] = {s["name"]: s for s in C.piece_json("vehicle", v)["sockets"]}
    return _SO[v]


def rel(v, seat, name):
    """A socket's point from a seat socket, in the seat's own axes (forward, right, up: the game's)."""
    s = socks(v)
    o = np.array(s[seat]["at"], float)
    f = np.array(s[seat].get("facing", (1.0, 0.0, 0.0)), float)
    yaw = math.atan2(f[1], f[0])
    c, sn = math.cos(yaw), math.sin(yaw)
    d = np.array(s[name]["at"], float) - o
    return np.array((c * d[0] + sn * d[1], -sn * d[0] + c * d[1], d[2]))


def grips(v, seat, kind):
    """(left hand's, right hand's) grip from the seat, the game's axes: told apart by where they are, not by their
    names (the cars' grip.driver.l is on the driver's right)."""
    a, b = rel(v, seat, f"grip.{kind}.l"), rel(v, seat, f"grip.{kind}.r")
    return (a, b) if a[1] <= b[1] else (b, a)


def mean_grips(vs, seat, kind):
    gs = [grips(v, seat, kind) for v in vs]
    return np.mean([g[0] for g in gs], axis=0), np.mean([g[1] for g in gs], axis=0)


def B(v):
    """The game's axes (y right) to the pose's (Blender's: y left)."""
    return C.g2b(v)


CARS = ("Jeep", "GAZ67", "Kubelwagen", "Fiat508", "Kurogane", "FieldCar")


def on_left(v, seat):
    """A seat on the vehicle's left (the game's y < 0): the Fiat 508's and the Kurogane's drivers sit on the right."""
    return float(socks(v)[seat]["at"][1]) < 0.0
# (the cars' seats over the floor their boots are on, from the cars' socket notes: the Jeep's and the GAZ-67's front
#  seats 0.32, the others' 0.34; every back seat 0.38)
DRIVE = {"car_drive": {"cars": ("Jeep", "GAZ67"), "seat_h": 0.32},
         "car_drive_b": {"cars": ("Kubelwagen", "Fiat508"), "seat_h": 0.34},
         "car_drive_c": {"cars": ("FieldCar", "Kurogane"), "seat_h": 0.34}}
RIDE = {"car_ride": {"seat_h": 0.33, "seats": ("rider.1",), "feet": 0.40, "hip_x": -0.07},
        "car_ride_back": {"seat_h": 0.38, "seats": ("rider.2", "rider.3"), "feet": 0.31, "hip_x": -0.05}}
HIP = {"drive": -0.07}
# two men side by side (a car's front pair, its back pair) must not go into each other: knees kept together over his
# boots, the elbows in by his sides (tuned with crew_pairs: each man against the other's body)
_K = json.loads(os.environ.get("RIDERS_KNOBS", "{}"))
KNOBS = {"ride_feet_y": 0.10, "ride_knee": 0.08, "ride_elbow": (-0.5, 0.4, -1.0), "ride_pole_w": 0.03,
         "drive_feet_y": (0.09, -0.10), "drive_knee": 0.15, "sidecar_pole_l": (-0.8, 0.15, -0.8),
         "pillion_back": {"Alce": 0.08, "Rikuo": 0.07}, "pillion_feet_back": {}}
KNOBS.update(_K)
# where a back seat's man goes from the seat's point (forward, toward the car's middle): his back to the backrest (each
# car's measured 0.25 over the seat), his boots clear of the Jeep's wheel arches
BACK_SEAT = {"Jeep": (0.115, 0.08), "GAZ67": (0.19, 0.0), "Kubelwagen": (-0.05, 0.0), "Fiat508": (0.09, 0.0),
             "Kurogane": (0.165, -0.03), "FieldCar": (0.06, 0.04)}


BACK_SEAT.update({k: tuple(v) for k, v in json.loads(os.environ.get("RIDERS_BACK_SEAT", "{}")).items()})

# crew.inside counts crossings along three rays: a ray that grazes a seam (one hit counted two or three times) or runs
# through a part a metre away that is not closed (the Kurogane's rear arches, seen from its driver) flips its answer.
# RIDERS_ROBUST=1 asks nine rays, counts hits closer than 1 mm as one, and takes the most of them.
ROBUST_RAYS = [(0.31, 0.17, 0.93), (-0.62, 0.71, -0.33), (0.45, -0.83, 0.12), (0.83, 0.42, -0.36), (-0.27, -0.55, 0.79),
               (-0.91, -0.12, 0.40), (0.12, 0.95, 0.29), (0.58, -0.21, -0.79), (-0.36, 0.33, 0.87)]


def inside_robust(tree, p, far=6.0):
    from mathutils import Vector
    votes = 0
    for d in ROBUST_RAYS:
        d = Vector(d).normalized()
        q, k, last = Vector(p), 0, None
        for _ in range(60):
            hit = tree.ray_cast(q, d, far)
            if hit[0] is None:
                break
            if last is None or (hit[0] - last).length > 1e-3:
                k += 1
            last, q = hit[0], hit[0] + d * 1e-4
        votes += k % 2
    return 2 * votes > len(ROBUST_RAYS)


C.inside = inside_robust       # (the nine-ray overlap test, in place of crew.inside's three)


def ride_place(v, s):
    if s == "rider.1":
        return (0.0, 0.0, 0.0)
    dx, dy = BACK_SEAT[v]
    return (dx, dy if s == "rider.2" else -dy, 0.0)
# the astride riders: hands on the bars (the bars run across, so a hand may sit a little along its grip), boots on
# the footrests or boards
ASTRIDE = {"bike_astride": {"bikes": ("R75", "M72", "M20", "Alce", "Motorcycle")},
           "bike_astride_b": {"bikes": ("WLA", "Rikuo")}}
# the pillions (measured off the bikes' meshes, the seat's axes, the game's): his hands, his boots' rests
PILLION = {"bike_pillion": {"bike": "R75", "hands": (0.17, 0.08, 0.05), "feet": (0.13, 0.22, -0.555)},
           "bike_pillion_b": {"bike": "M72", "hands": (0.17, 0.08, 0.05), "feet": (0.12, 0.17, -0.50)},
           "bike_pillion_c": {"bike": "Rikuo", "hands": (0.09, 0.13, -0.02), "feet": (0.42, 0.17, -0.45)},
           "bike_pillion_d": {"bike": "Alce", "hands": None, "feet": (-0.08, 0.20, -0.42), "ahead": 0.05}}
SIDECAR = {"sidecar_gunner": {"bikes": ("M72", "Rikuo")}, "sidecar_gunner_b": {"bikes": ("R75",)}}
TUB = {"cushion": 0.12, "rim": 0.24, "half": 0.25}     # (the sidecar's floor under the cushion's top, its rim over it)


# ================================================================================================ small pieces
# (the solver keeps a knee off the ground at the root's height by lifting the pelvis; a seated man's root is his seat,
#  and astride a saddle his knees are well under it: poses marked `_seat` are solved without that lift)
_SOLVE = S.Rig.solve
if not getattr(_SOLVE, "_riders", False):
    def _solve(self, P, lift_knees=4):
        return _SOLVE(self, P, lift_knees=0 if P.get("_seat") else lift_knees)
    _solve._riders = True
    S.Rig.solve = _solve


def seat(name, P):
    P["_seat"] = 1.0
    return C.seated(name, P)


def sway(t, long, seed, k=1.0):
    """The vehicle's ride taken in the body: roll and pitch (degrees) and a breath."""
    roll = k * (1.5 * C.wave(t, long, 2, seed) + 0.6 * C.wave(t, long, 5, 0.3 + seed))
    pitch = k * (1.1 * C.wave(t, long, 3, 0.15 + seed) + 0.4 * C.wave(t, long, 7, seed))
    br = C.wave(t, long, 1, 0.6 + seed)
    return roll, pitch, br


def ride_in(P, roll, pitch, br, hips=True):
    """The ride (sway()) laid on a pose: the hips rock, the back gives, the head steadies itself."""
    if hips:
        P["pelvis"] = add3(P["pelvis"], (0.4 * pitch, roll, 0.0))
    P["spine_01"] = add3(P["spine_01"], (0.3 * pitch, -0.3 * roll, 0.0))
    P["spine_02"] = add3(P["spine_02"], (0.3 * pitch - 0.5 * br, -0.3 * roll, 0.0))
    P["chest"] = add3(P["chest"], (-0.9 * br, -0.2 * roll, 0.0))
    P["neck"] = add3(P["neck"], (-0.5 * pitch, -0.3 * roll, 0.0))
    P["head"] = add3(P["head"], (-0.4 * pitch, -0.3 * roll, 0.0))
    P["clavicle_l"] = add3(P["clavicle_l"], (1.0 * br, 0.0))[:2]
    P["clavicle_r"] = add3(P["clavicle_r"], (1.0 * br, 0.0))[:2]
    return P


def look_about(P, t, a=(0.6, 1.0, 1.6), b=(2.4, 2.8, 3.4), yaw=(26.0, -22.0)):
    """A glance to one side and later to the other."""
    g = yaw[0] * bump(t, *a) + yaw[1] * bump(t, *b)
    P["neck"] = add3(P["neck"], (0.0, 0.0, 0.45 * g))
    P["head"] = add3(P["head"], (0.0, 0.0, 0.55 * g))
    return P


def away(P):
    P["weapon"], P["weapon_hidden"] = C.stowed(), True
    return P


def bar_hand(s, g, pole_side=1.0, back=0.0):
    """A fist round a bar that runs across (a handlebar, a grab handle): overhand, the knuckles forward and down, the
    thumb in along the bar, the palm down and back."""
    side = 1.0 if s == "l" else -1.0
    tdir = np.array((0.0, -side, 0.0))                     # (the thumb in along the bar, toward the middle)
    a = _n(np.array((0.75 - back, 0.0, -0.65)))
    a = _n(a - tdir * (tdir @ a))
    n = np.cross(a, tdir) if s == "l" else np.cross(tdir, a)
    return {"at": tuple(np.asarray(g, float)), "a": tuple(a), "n": tuple(n), "pole_world": (-0.3, 1.0 * side * pole_side, -1.0),
            "roll": 25.0}


def spade_hand(s, g):
    """A hand on a gun's spade grip (crew.mg_hands' hold): overhand, the thumbs back toward him at the trigger."""
    side = 1.0 if s == "l" else -1.0
    tdir = _n(np.array((-0.9, -0.35 * side, 0.0)))
    a = _n(np.array((0.45, -0.75 * side, -0.5)))
    a = _n(a - tdir * (tdir @ a))
    n = np.cross(a, tdir) if s == "l" else np.cross(tdir, a)
    return {"at": tuple(np.asarray(g, float)), "a": tuple(a), "n": tuple(n), "pole_world": (-0.7, 1.0 * side, -0.7)}


def flat_hand(s, at, a=(0.8, 0.0, -0.6), n=(0.0, 0.0, -1.0), pole=None):
    """A hand laid flat (on a thigh, a rim)."""
    side = 1.0 if s == "l" else -1.0
    return {"at": tuple(at), "a": tuple(a), "n": tuple(n), "pole_world": pole or (-0.4, 1.0 * side, -0.6)}


def lean_to(P, grips_b, lift=0, start=0.0, top=50.0, easy=0.9):
    """The least lean forward from the hips (spine and chest, the neck and head held level) at which both hands' grips
    come within easy reach (crew.body_for's first step, on a body set out here)."""
    rig = C.MODEL["rig"]
    base = dict(P)
    base.setdefault("weapon", C.stowed())

    def leaned(lean):
        Q = dict(base)
        Q["spine_01"] = add3(base.get("spine_01", (0, 0, 0)), (0.3 * lean, 0.0, 0.0))
        Q["spine_02"] = add3(base.get("spine_02", (0, 0, 0)), (0.35 * lean, 0.0, 0.0))
        Q["chest"] = add3(base.get("chest", (0, 0, 0)), (0.35 * lean, 0.0, 0.0))
        Q["neck"] = add3(base.get("neck", (0, 0, 0)), (-0.4 * lean, 0.0, 0.0))
        Q["head"] = add3(base.get("head", (0, 0, 0)), (-0.3 * lean, 0.0, 0.0))
        Q["hand_l"] = {"frame": "chest", "at": (0.3, 0.16, -0.3), "a": (1, 0, 0), "n": (0, 0, -1)}
        Q["hand_r"] = {"frame": "chest", "at": (0.3, -0.16, -0.3), "a": (1, 0, 0), "n": (0, 0, -1)}
        return Q

    for lean in np.arange(start, top, 1.0):
        st = rig.solve(leaned(lean), lift_knees=lift)
        worst = max(np.linalg.norm(g - st["H"]["upperarm_" + s]) / C.reach_of(s) for s, g in grips_b.items())
        if worst < easy:
            return float(lean)
    return float(top)


def leaned(P, lean):
    for k, f in (("spine_01", 0.3), ("spine_02", 0.35), ("chest", 0.35)):
        P[k] = add3(P.get(k, (0, 0, 0)), (f * lean, 0.0, 0.0))
    for k, f in (("neck", -0.4), ("head", -0.3)):
        P[k] = add3(P.get(k, (0, 0, 0)), (f * lean, 0.0, 0.0))
    return P


# ---- seated on a saddle: the crotch on the saddle's top (the thighs hang either side of it)
ASTRIDE_FIT = {}


def on_saddle(name, P, sink=0.006, half=0.05):
    """The pelvis's height that rests his crotch on the saddle's top (the root): the lowest of the body's points
    within `half` of his middle, between his seat bones and the front of his crotch (measured once per clip on its
    first pose, then the same on every frame)."""
    P["_seat"] = 1.0
    if name not in ASTRIDE_FIT:
        rig, skin = C.MODEL["rig"], C.MODEL["skin"]
        Q = dict(P)
        dz = 0.0
        for _ in range(4):
            st = rig.solve(Q, lift_knees=0)
            pts, bones = skin.points(st)
            sel = (np.abs(pts[:, 1]) < half) & (pts[:, 0] > -0.18) & (pts[:, 0] < 0.12) & \
                np.isin(bones, ("pelvis", "thigh_l", "thigh_r", "spine_01"))
            low = float(pts[sel, 2].min())
            d = -sink - low
            if abs(d) < 0.0005:
                break
            dz += d
            pa = list(Q["pelvis_at"])
            pa[2] += d
            Q["pelvis_at"] = tuple(pa)
        ASTRIDE_FIT[name] = dz
    pa = list(P["pelvis_at"])
    pa[2] += ASTRIDE_FIT[name]
    P["pelvis_at"] = tuple(pa)
    return P


# ================================================================================================ the clips
MODE = {}          # clip -> how his rifle goes: "none" (put away), "rifle" (in his hands), "slung" (on his back)
HOLD = {}          # clip -> t -> (left, right): how much each hand holds the rifle (1 on it), for the rifle clips
DEATH = {}         # seat clip -> its death
LOOK_ONLY = {}     # clip -> places where it is played but not fitted (said in `about`)


def clip(name, long, loop, anchor, mode, places, about, death=None):
    weapon = "rifle" if mode == "rifle" else "none"
    deco = C.crew_clip(name, long, loop=loop, anchor=anchor, weapon=weapon, place=places, about=about)

    def wrap(fn):
        MODE[name] = mode
        if death:
            DEATH[name] = death
        return deco(fn)
    return wrap


# ---- the drivers
def drive_fn(name, cars, seat_h):
    gl, gr = mean_grips(cars, "driver", "driver")
    glb, grb = B(gl), B(gr)
    centre = 0.5 * (glb + grb)
    r = 0.5 * float(np.linalg.norm(glb - grb))
    up = _n(np.array((-0.5, 0.0, 0.866)))                 # (the wheel tipped back 30 degrees: its top toward him)
    base = C.sit_body(seat_h, slouch=0.2, hip_x=HIP["drive"])
    lean = lean_to(base, {"l": glb, "r": grb}, start=4.0, easy=0.86)

    def fn(t):
        roll, pitch, br = sway(t, LOOP, 0.1)
        P = C.sit_body(seat_h, slouch=0.2, hip_x=HIP["drive"])
        leaned(P, lean)
        ride_in(P, roll, pitch, br)
        look_about(P, t)
        ank = C.MAN["ankle"]
        fl, fr = KNOBS["drive_feet_y"]
        kn = KNOBS["drive_knee"]
        P["foot_l"] = {"at": (0.46, fl, -seat_h + ank + 0.03), "yaw": 8.0, "pitch": -12.0, "pole": (0.8, kn, 0.5)}
        P["foot_r"] = {"at": (0.50, fr, -seat_h + ank + 0.03), "yaw": -8.0, "pitch": -16.0, "pole": (0.8, -kn, 0.5)}
        away(P)
        P["hand_l"] = C.on_rim("l", centre, up, r, 90.0, a_lean=0.6)
        P["hand_r"] = C.on_rim("r", centre, up, r, -90.0, a_lean=0.6)
        P["hand_l"]["pole_world"], P["hand_r"]["pole_world"] = (-0.3, 0.45, -1.2), (-0.3, -0.45, -1.2)
        P["hand_l"]["flip"] = P["hand_r"]["flip"] = False     # (the thumb kept up the rim: taken end for end it flickers)
        if name == "car_drive":            # (the Jeep's .30 on its post is beside his right shoulder: kept low)
            P["clavicle_r"] = add3(P["clavicle_r"], (-8.0, 0.0))[:2]
            P["chest"] = add3(P["chest"], (0.0, 5.0, 0.0))
        P["fingers_l"], P["fingers_r"], P["thumb_l"], P["thumb_r"] = 64.0, 64.0, 30.0, 30.0
        return seat(name, P)
    fn.lean = lean
    return fn


def make_drivers():
  for _n_, _d in DRIVE.items():
    _f = drive_fn(_n_, _d["cars"], _d["seat_h"])
    clip(_n_, LOOP, True, "seat", "none", [("vehicle", v, "driver", (0, 0, 0)) for v in _d["cars"]],
         "A car's driver: both hands on the wheel at a quarter to three (grip.driver.l and .r: which is which is taken "
         "from where they are), his feet on the pedals, the body taking the ride, his eyes on the road with a glance "
         "to each side; his rifle put away. Seat %.2f over the floor his boots are on. Fitted to the %s's wheel."
         % (_d["seat_h"], " and the ".join(_d["cars"])), death="die_seat_slump")(_f)


# ---- the men in the cars, rifle upright between the knees
def ride_body(t, seat_h, seed, feet, hip_x):
    roll, pitch, br = sway(t, LOOP, seed)
    P = C.sit_body(seat_h, slouch=0.7, hip_x=hip_x)
    ride_in(P, roll, pitch, br)
    ank = C.MAN["ankle"]
    fy, kn = KNOBS["ride_feet_y"], KNOBS["ride_knee"]
    P["foot_l"] = {"at": (feet, fy, -seat_h + ank), "yaw": 6.0, "pole": (1.0, kn, 0.55)}
    P["foot_r"] = {"at": (feet + 0.02, -fy, -seat_h + ank), "yaw": -6.0, "pole": (1.0, -kn, 0.55)}
    return P, roll, pitch


def rifle_hold(P, seat_h, butt_x, roll, pitch):
    W = C.rifle_upright((butt_x, 0.0, -seat_h), lean_back=10.0 + 0.5 * pitch, side=-0.5 * roll)
    P["weapon"] = W
    e = KNOBS["ride_elbow"]
    P["hand_l"] = C.on_rifle(W, "l", 0.6, (0.75, -0.65, -0.1), pole=(e[0], e[1], e[2]))
    P["hand_r"] = C.on_rifle(W, "r", 0.49, (0.75, 0.65, -0.1), pole=(e[0], -e[1], e[2]))
    for s in ("l", "r"):
        P["hand_" + s]["pole_weight"] = KNOBS["ride_pole_w"]
    P["fingers_l"], P["fingers_r"], P["thumb_l"], P["thumb_r"] = 62.0, 62.0, 45.0, 45.0
    return P


def ride_fn(name, seat_h, feet, seed, hip_x):
    def fn(t):
        P, roll, pitch = ride_body(t, seat_h, seed, feet, hip_x)
        look_about(P, t, a=(1.0, 1.6, 2.4), b=(2.8, 3.2, 3.8), yaw=(-30.0, 18.0))
        rifle_hold(P, seat_h, feet - 0.06, roll, pitch)
        return seat(name, P)
    return fn


def make_riders():
  for _n_, _d in RIDE.items():
    clip(_n_, LOOP, True, "seat", "rifle", [("vehicle", v, s, ride_place(v, s)) for v in CARS for s in _d["seats"]],
         "A man in a car's %s seat: sat back, his rifle stood on its butt between his boots and both hands on it, the "
         "body taking the ride, looking out to the side and ahead. Seat %.2f over the floor."
         % ("front" if _n_ == "car_ride" else "back", _d["seat_h"]), death="die_ride_slump")(ride_fn(_n_, _d["seat_h"], _d["feet"], 0.35 if _n_ == "car_ride" else 0.7, _d["hip_x"]))
    HOLD[_n_] = lambda t: (1.0, 1.0)


# ---- astride a motorcycle
def astride_body(lean, knee_out=0.55):
    P = C.sit_body(0.5, slouch=0.3, hip_x=-0.05, hip_z=0.10)
    P["pelvis"] = (-6.0, 0.0, 0.0)
    leaned(P, lean)
    P["clavicle_l"], P["clavicle_r"] = (2.0, 5.0), (2.0, 5.0)
    return P


def astride_feet(P, foot_l, foot_r, knee_out=0.6, ahead=-0.06):
    ank = C.MAN["ankle"]
    for s, f, side in (("l", foot_l, 1.0), ("r", foot_r, -1.0)):
        f = np.asarray(f, float)
        P["foot_" + s] = {"at": (float(f[0]) + ahead, float(f[1]), float(f[2]) + ank), "yaw": 6.0 * side, "pitch": -8.0,
                          "pole": (0.8, side * knee_out, 0.4)}
    return P


def astride_fn(name, bikes):
    gl, gr = mean_grips(bikes, "driver", "driver")
    fl = np.mean([rel(v, "driver", "foot.l") for v in bikes], axis=0)
    fr = np.mean([rel(v, "driver", "foot.r") for v in bikes], axis=0)
    glb, grb, flb, frb = B(gl), B(gr), B(fl), B(fr)
    base = astride_feet(astride_body(0.0), flb, frb)
    lean = lean_to(base, {"l": glb, "r": grb}, lift=0, start=10.0, easy=0.96)

    def fn(t):
        roll, pitch, br = sway(t, LOOP, 0.2, k=0.8)
        P = astride_feet(astride_body(lean), flb, frb)
        ride_in(P, roll, pitch, br)
        look_about(P, t, a=(0.8, 1.2, 1.9), b=(2.6, 3.0, 3.6), yaw=(18.0, -16.0))
        away(P)
        P["hand_l"] = bar_hand("l", glb)
        P["hand_r"] = bar_hand("r", grb)
        P["fingers_l"], P["fingers_r"], P["thumb_l"], P["thumb_r"] = 64.0, 64.0, 35.0, 35.0
        return on_saddle(name, P)
    fn.lean = lean
    return fn


def make_astride():
  for _n_, _d in ASTRIDE.items():
    clip(_n_, LOOP, True, "seat", "slung", [("vehicle", v, "driver", (0, 0, 0)) for v in _d["bikes"]],
         "A motorcycle's rider astride the saddle: hands on the bars (grip.driver.l and .r), boots on the footrests "
         "(foot.l and .r), leaning to the bars, the body taking the ride, a glance to each side; his rifle slung on his "
         "back. Fitted to the %s." % ", the ".join(_d["bikes"]), death="die_bike_slump")(astride_fn(_n_, _d["bikes"]))


def pillion_back(bike):
    """How far back of the bike's pillion socket this pillion sits (m): clear of the rider's back and kit ahead of him."""
    return float(KNOBS["pillion_back"].get(bike, 0.0))


def pillion_fn(name, bike, hands, feet, ahead=-0.04):
    b = pillion_back(bike)                                      # (his root moved back: what he reaches for comes nearer him)
    fb = float(KNOBS["pillion_feet_back"].get(bike, 0.0))
    if hands is None:                                           # (the Alce's second saddle has its own bars)
        gl, gr = np.array((0.17 + b, -0.26, 0.11)), np.array((0.17 + b, 0.26, 0.11))
    else:
        gl = np.array((hands[0] + b, -hands[1], hands[2]))
        gr = np.array((hands[0] + b, hands[1], hands[2]))
    fl = np.array((feet[0] + b - fb, -feet[1], feet[2]))
    fr = np.array((feet[0] + b - fb, feet[1], feet[2]))
    glb, grb, flb, frb = B(gl), B(gr), B(fl), B(fr)
    base = astride_feet(astride_body(0.0), flb, frb, knee_out=0.7, ahead=ahead)
    lean = lean_to(base, {"l": glb, "r": grb}, lift=0, start=2.0)

    def fn(t):
        roll, pitch, br = sway(t, LOOP, 0.45, k=0.9)
        P = astride_feet(astride_body(lean), flb, frb, knee_out=0.7, ahead=ahead)
        ride_in(P, roll, pitch, br)
        look_about(P, t, a=(0.4, 0.9, 1.6), b=(2.2, 2.7, 3.3), yaw=(-34.0, 30.0))
        away(P)
        P["hand_l"] = bar_hand("l", glb, back=0.35)
        P["hand_r"] = bar_hand("r", grb, back=0.35)
        P["fingers_l"], P["fingers_r"], P["thumb_l"], P["thumb_r"] = 64.0, 64.0, 35.0, 35.0
        return on_saddle(name, P)
    fn.lean = lean
    return fn


def make_pillions():
  for _n_, _d in PILLION.items():
    clip(_n_, LOOP, True, "seat", "slung", [("vehicle", _d["bike"], "rider.1", (-pillion_back(_d["bike"]), 0, 0))],
         "The man on the %s's pillion, astride: his hands on %s, his boots on his rests, looking about over his "
         "shoulders; his rifle slung on his back." % (_d["bike"], "the second saddle's own bars" if _d["hands"] is None
                                                       else "the grab handle ahead of him" if _d["bike"] != "Rikuo"
                                                       else "the saddle's springs ahead of him"),
         death="die_pillion_slump")(pillion_fn(_n_, _d["bike"], _d["hands"], _d["feet"], _d.get("ahead", -0.04)))


# ---- in the sidecar
def sidecar_body(seat_h=TUB["cushion"]):
    P = C.sit_body(seat_h, slouch=0.4, hip_x=-0.06, hip_z=0.11)
    P["pelvis"] = (-16.0, 0.0, 0.0)
    ank = C.MAN["ankle"]
    # (heels on the tub's floor, toes up against its nose, which rises from 0.85 ahead)
    P["foot_l"] = {"at": (0.77, 0.08, -seat_h + ank + 0.01), "yaw": 8.0, "pitch": -14.0, "pole": (0.3, 0.05, 1.0)}
    P["foot_r"] = {"at": (0.78, -0.08, -seat_h + ank + 0.01), "yaw": -8.0, "pitch": -14.0, "pole": (0.3, -0.05, 1.0)}
    return P


def sidecar_fn(name, bikes):
    gl, gr = mean_grips(bikes, "gunner", "gunner")
    glb, grb = B(gl), B(gr)
    lean = lean_to(sidecar_body(), {"l": glb, "r": grb}, lift=0, start=0.0)

    def fn(t):
        roll, pitch, br = sway(t, LOOP, 0.6, k=0.8)
        P = leaned(sidecar_body(), lean)
        ride_in(P, roll, pitch, br, hips=False)
        look_about(P, t, a=(1.2, 1.7, 2.3), b=(2.8, 3.2, 3.7), yaw=(16.0, -20.0))
        away(P)
        P["hand_l"] = dict(spade_hand("l", glb), pole_world=tuple(KNOBS["sidecar_pole_l"]))   # (his left elbow in: the bike's rider is there)
        P["hand_r"] = spade_hand("r", grb)
        P["fingers_l"], P["fingers_r"], P["thumb_l"], P["thumb_r"] = 66.0, 66.0, 25.0, 25.0
        return seat(name, P)
    fn.lean = lean
    return fn


def make_sidecars():
  for _n_, _d in SIDECAR.items():
    clip(_n_, LOOP, True, "seat", "none", [("vehicle", v, "gunner", (0, 0, 0)) for v in _d["bikes"]],
         "The sidecar's gunner seated low on his cushion, his legs forward in the tub, both hands on the gun's grips "
         "(grip.gunner.l and .r), looking along it and about; his rifle put away. Fitted to the %s; the game turns him "
         "with the gun's mount if it turns." % " and the ".join(_d["bikes"]), death="die_sidecar_slump")(sidecar_fn(_n_, _d["bikes"]))


@clip("sidecar_ride", LOOP, True, "seat", "none", [("vehicle", "Motorcycle", "rider.2", (0, 0, 0))],
      "The man in the plain motorcycle's sidecar (rider.2), seated low, legs forward in the tub: his right forearm on "
      "the tub's rim, his left hand on his thigh, looking about; his rifle put away.", death="die_sidecar_slump")
def c_sidecar_ride(t):
    roll, pitch, br = sway(t, LOOP, 0.8, k=0.8)
    P = sidecar_body()
    P["spine_02"] = add3(P["spine_02"], (0.0, -3.0, 0.0))
    ride_in(P, roll, pitch, br, hips=False)
    look_about(P, t, a=(0.5, 1.0, 1.8), b=(2.4, 2.9, 3.5), yaw=(-30.0, 24.0))
    away(P)
    rim = B((0.10, TUB["half"] - 0.01, TUB["rim"] + 0.02))
    P["hand_r"] = flat_hand("r", rim, a=(0.6, -0.5, -0.6), n=(0.0, 0.0, -1.0), pole=(-0.5, -1.0, -0.3))
    P["hand_l"] = flat_hand("l", (0.30, 0.08, TUB["rim"] + 0.045), a=(0.9, -0.1, -0.3), n=(0.0, 0.0, -1.0))   # (on the cowl)
    P["fingers_l"], P["fingers_r"], P["thumb_l"], P["thumb_r"] = 25.0, 35.0, 15.0, 20.0
    return seat("sidecar_ride", P)


# ---- standing at the Jeep's gun
JG = {}


def jeep_gunner_base():
    gl, gr = grips("Jeep", "gunner", "gunner")
    JG["l"], JG["r"] = B(gl), B(gr)
    P = {"pelvis_at": (-0.03, 0.0, -0.11), "pelvis": (14.0, 0.0, 0.0), "spine_01": (5.0, 0.0, 0.0), "spine_02": (5.0, 0.0, 0.0),
         "chest": (4.0, 0.0, 0.0), "neck": (-10.0, 0.0, 0.0), "head": (-8.0, 0.0, 0.0),
         "clavicle_l": (2.0, 6.0), "clavicle_r": (2.0, 6.0),
         "foot_l": {"at": (0.0, 0.13, C.MAN["ankle"]), "yaw": 6.0, "pole": (1.0, 0.2, 0.0)},
         "foot_r": {"at": (-0.12, -0.15, C.MAN["ankle"]), "yaw": -16.0, "pole": (1.0, -0.2, 0.0)}}
    return P


@clip("jeep_gunner", LOOP, True, "feet", "slung", [("vehicle", "Jeep", "gunner", (0, 0, 0))],
      "The Jeep's gunner standing behind the post, knees bent to the low gun and leaning into it, both hands on its "
      "grips (grip.gunner.l and .r), looking along it and about, the ride in his knees; his rifle slung on his back. "
      "Root: the gunner socket (his feet); the game turns him with the gun's mount (mg_mount) if it turns.",
      death="die_gunner_slump")
def c_jeep_gunner(t):
    if not JG:
        jeep_gunner_base()
    roll, pitch, br = sway(t, LOOP, 0.9, k=0.7)
    P = jeep_gunner_base()
    P["pelvis_at"] = add3(P["pelvis_at"], (0.0, 0.006 * roll, -0.004 * abs(pitch)))
    P["pelvis"] = add3(P["pelvis"], (0.3 * pitch, 0.5 * roll, 0.0))
    P["spine_02"] = add3(P["spine_02"], (-0.5 * br, -0.3 * roll, 0.0))
    P["chest"] = add3(P["chest"], (-0.8 * br, 0.0, 0.0))
    look_about(P, t, a=(0.8, 1.3, 2.0), b=(2.5, 3.0, 3.6), yaw=(14.0, -14.0))
    away(P)
    P["hand_l"] = spade_hand("l", JG["l"])
    P["hand_r"] = spade_hand("r", JG["r"])
    P["fingers_l"], P["fingers_r"], P["thumb_l"], P["thumb_r"] = 66.0, 66.0, 25.0, 25.0
    return P


# ================================================================================================ the deaths
def die_keys(src, finals, jolt=(-6.0, -10.0)):
    """A death from a seat clip's first pose: struck (a jolt back through the chest and head), then falling into the
    final pose (finals: [(t, P)]), held. Returns the clip's pose function."""
    P0 = C.CREW[src]["fn"](0.0)
    J = dict(P0)
    J["chest"] = add3(P0["chest"], (jolt[0], 0.0, 0.0))
    J["head"] = add3(P0["head"], (jolt[1], 0.0, 0.0))
    J["neck"] = add3(P0["neck"], (0.5 * jolt[1], 0.0, 0.0))
    keys = [(0.0, P0), (0.1, J)] + list(finals)
    at = S.keyed(keys)
    return lambda t: dict(at(t))        # (a copy: keyed hands back its own first and last poses, which seat() moves)


def slumped(P0, fwd=30.0, side=12.0, head=(38.0, 10.0, 16.0), sink=0.0):
    """P0 slumped: the back folded forward and to one side (+: his left), the head hanging."""
    P = dict(P0)
    P["spine_01"] = add3(P0["spine_01"], (0.3 * fwd, 0.3 * side, 0.0))
    P["spine_02"] = add3(P0["spine_02"], (0.35 * fwd, 0.35 * side, 4.0))
    P["chest"] = add3(P0["chest"], (0.35 * fwd, 0.35 * side, 4.0))
    P["neck"] = add3(P0["neck"], (0.45 * head[0], 0.5 * head[1], 0.4 * head[2]))
    P["head"] = add3(P0["head"], (0.55 * head[0], 0.5 * head[1], 0.6 * head[2]))
    P["clavicle_l"] = (-4.0, 2.0)
    P["clavicle_r"] = (-4.0, 2.0)
    if sink:
        P["pelvis_at"] = add3(P0["pelvis_at"], (0.0, 0.0, -sink))
    return P


def limp(P, hl, hr, fingers=18.0):
    P["hand_l"] = hl
    P["hand_r"] = hr
    P["fingers_l"] = P["fingers_r"] = fingers
    P["thumb_l"] = P["thumb_r"] = 12.0
    return P


def on_thigh(s, x=0.26, y=0.13, z=0.14, chest=False):
    side = 1.0 if s == "l" else -1.0
    return flat_hand(s, (x, side * y, z), a=(0.8, 0.15 * side, -0.55), pole=(-0.5, side, -0.4))


def hang(s, at, a=(0.1, 0.0, -1.0), n=None):
    side = 1.0 if s == "l" else -1.0
    return {"at": tuple(at), "a": tuple(_n(np.asarray(a, float))), "n": tuple(n or (0.0, -side, 0.0)),     # (the palm to his leg)
            "pole_world": (-0.6, 0.8 * side, 0.0)}


CALM = {}
LIMP_POLE = (-0.6, 0.8, -0.3)       # (a dead arm's elbow: back, out and down)


def calm(name, fn, finish):
    """A death's arms without the solver's search for the easiest elbow (it swings an elbow round through his body
    when a hand lets go): from the seat's first pose, whose elbows it keeps, each elbow goes steadily to hang back,
    out and down, the hands turned as the keys ask."""
    def g(t):
        P = fn(t)
        if t <= 1e-6:
            return finish(name, P)
        rig = C.MODEL["rig"]
        if name not in CALM:
            P0 = finish(name, fn(0.0))
            st0 = rig.solve(P0, lift_knees=0)
            CALM[name] = {s: (st0["H"]["lowerarm_" + s] - 0.5 * (st0["H"]["upperarm_" + s] + st0["H"]["hand_" + s]),
                              P0["hand_" + s], st0["D"]["hand_" + s]) for s in "lr"}
        u = ramp(t, 0.1, 0.7)
        P = dict(P)
        P["comfort"] = 0.0
        for s, side in (("l", 1.0), ("r", -1.0)):
            pole0, h0, Dh0 = CALM[name][s]
            p1 = _n(np.array((LIMP_POLE[0], side * LIMP_POLE[1], LIMP_POLE[2])))
            a0, t0_, n0 = rig.hand0[s]
            held = {"a": tuple(Dh0 @ np.asarray(a0, float)), "n": tuple(Dh0 @ np.asarray(n0, float))}

            def first_grip(h):         # (the seat's grip as the solver turned the hand on it at the first key)
                if "blend" in h:
                    return {"blend": [(w, first_grip(x)) for w, x in h["blend"]]}
                return dict(h, **held, roll=0.0) if (h.get("at") == h0.get("at") and "at" in h) or h is h0 else h
            P["hand_" + s] = _with_pole(first_grip(P["hand_" + s]), _n((1.0 - u) * _n(np.asarray(pole0, float)) + u * p1))
        return finish(name, P)
    return g


def _plain(name, P):
    return P


def build_deaths():
    CALM.clear()
    # a car's driver: struck back, then folded forward over his lap and toward the car's side (his left where he
    # sits on the left, his right in the right-hand-drive cars: never onto the man beside him), hands off the wheel
    P0 = C.CREW["car_drive"]["fn"](0.0)
    drv = [p for c in DRIVE for p in C.CREW[c]["place"]]
    for dname, k, places in (("die_seat_slump", 1.0, [p for p in drv if on_left(p[1], p[2])]),
                             ("die_seat_slump_r", -1.0, [p for p in drv if not on_left(p[1], p[2])])):
        near, far = ("l", "r") if k > 0 else ("r", "l")
        F1 = limp(slumped(P0, fwd=24.0, side=10.0 * k), on_thigh("l", 0.30, 0.16, 0.20), on_thigh("r", 0.30, 0.16, 0.20))
        F2 = slumped(P0, fwd=34.0, side=16.0 * k, head=(44.0, 14.0 * k, 20.0 * k), sink=0.01)
        F2 = limp(F2, *((on_thigh(near, 0.22, 0.2, 0.12), hang(far, (0.18, -0.24 * k, 0.05)))[::1 if k > 0 else -1]))
        fn = die_keys("car_drive", [(0.55, F1), (DIE_REST, F2)])
        side = "left" if k > 0 else "right"
        clip(dname, DIE, False, "seat", "none", places,
             "A car's driver killed in his seat: struck back, his hands slide off the wheel and he folds forward over "
             f"his lap and to his {side} (toward the car's side, away from the man beside him), his head hanging; the "
             "last pose held. Played from the drivers' clips (car_drive, car_drive_b, car_drive_c) at the seats in "
             f"its places (the {'left' if k > 0 else 'right'}-hand drivers').")(calm(dname, fn, seat))

    # a man in a car holding his rifle: the rifle tips forward from his hands, he slumps forward and toward the car's
    # side he sits on (his right on the right, his left on the left)
    P0 = C.CREW["car_ride"]["fn"](0.0)
    seat_h = RIDE["car_ride"]["seat_h"]
    butt = (RIDE["car_ride"]["feet"] - 0.06, 0.0, -seat_h)
    rides = [p for c in RIDE for p in C.CREW[c]["place"]]

    def ride_die_fn(k):                 # (k 1: to his right; -1: to his left)
        def ride_die(t):
            fall = ramp(t, 0.15, 1.1)
            W = C.rifle_upright(butt, lean_back=10.0 - 34.0 * fall, side=-6.0 * fall * k)
            F1 = limp(slumped(P0, fwd=20.0, side=-10.0 * k), on_thigh("l", 0.30, 0.15, 0.20), on_thigh("r", 0.30, 0.15, 0.20))
            F2 = slumped(P0, fwd=30.0, side=-16.0 * k, head=(40.0, -14.0 * k, -18.0 * k), sink=0.01)
            if k > 0:
                F2 = limp(F2, on_thigh("l", 0.24, 0.15, 0.13), hang("r", (0.18, -0.18, 0.08)))
            else:
                F2 = limp(F2, hang("l", (0.18, 0.18, 0.08)), on_thigh("r", 0.24, 0.15, 0.13))
            P = dict(S.keyed([(0.0, P0), (0.1, dict(P0, chest=add3(P0["chest"], (-6.0, 0.0, 0.0)), head=add3(P0["head"], (-10.0, 0.0, 0.0)))),
                              (0.55, F1), (DIE_REST, F2)])(t))
            P["weapon"] = W
            hold = 1.0 - ramp(t, 0.1, 0.6)
            e = KNOBS["ride_elbow"]
            HL = C.on_rifle(W, "l", 0.6, (0.75, -0.65, -0.1), pole=(e[0], e[1], e[2]))
            HR = C.on_rifle(W, "r", 0.49, (0.75, 0.65, -0.1), pole=(e[0], -e[1], e[2]))
            if hold > 1e-3:
                P["hand_l"] = {"blend": [(hold, HL), (1.0 - hold, P["hand_l"])]} if hold < 1.0 - 1e-3 else HL
                P["hand_r"] = {"blend": [(hold, HR), (1.0 - hold, P["hand_r"])]} if hold < 1.0 - 1e-3 else HR
            return P
        return ride_die
    for dname, k, places in (("die_ride_slump", 1.0, [p for p in rides if not on_left(p[1], p[2])]),
                             ("die_ride_slump_l", -1.0, [p for p in rides if on_left(p[1], p[2])])):
        side = "right" if k > 0 else "left"
        clip(dname, DIE, False, "seat", "rifle", places,
             "A man killed in a car's seat with his rifle stood between his knees: struck back, his hands let the rifle "
             f"go and it tips forward against what is ahead of him; he slumps forward and to his {side} (toward the "
             "car's side, away from the man beside him), his head hanging. Played from car_ride and car_ride_back at "
             f"the seats in its places (the {side}-hand seats; fitted at the front seat's 0.33: in the back seat his "
             "boots stay 0.05 over the floor).")(calm(dname, ride_die_fn(k), seat))
        HOLD[dname] = lambda t: (1.0 - ramp(t, 0.1, 0.6),) * 2

    # a motorcycle's rider: struck, slumps forward over the tank, the hands slide off the bars and hang
    P0 = C.CREW["bike_astride"]["fn"](0.0)
    F1 = limp(slumped(P0, fwd=16.0, side=6.0, head=(30.0, 8.0, 10.0)), hang("l", (0.32, 0.26, 0.02)), hang("r", (0.32, -0.26, 0.02)))
    F2 = limp(slumped(P0, fwd=26.0, side=14.0, head=(46.0, 12.0, 18.0), sink=0.005), hang("l", (0.26, 0.30, -0.08)),
              hang("r", (0.30, -0.26, -0.04)))
    fn2 = die_keys("bike_astride", [(0.6, F1), (DIE_REST, F2)])
    clip("die_bike_slump", DIE, False, "seat", "slung", [p for c in ASTRIDE for p in C.CREW[c]["place"]],
         "A motorcycle's rider killed astride: struck back, then slumped forward over the tank and to his left, his "
         "hands off the bars and hanging, still astride; the last pose held (the bike's own fall is the vehicle's). "
         "Played from bike_astride and bike_astride_b.")(calm("die_bike_slump", fn2, on_saddle))

    # the pillion: struck, sags back and to his side, the hands off the handle
    P0 = C.CREW["bike_pillion"]["fn"](0.0)
    F1 = limp(slumped(P0, fwd=-8.0, side=-14.0, head=(-10.0, -16.0, -10.0)), hang("l", (0.10, 0.24, -0.04)), hang("r", (0.06, -0.26, -0.06)))
    F2 = limp(slumped(P0, fwd=10.0, side=-22.0, head=(30.0, -20.0, -16.0), sink=0.005), on_thigh("l", 0.16, 0.12, 0.06),
              hang("r", (0.02, -0.30, -0.14)))
    fn3 = die_keys("bike_pillion", [(0.6, F1), (DIE_REST, F2)], jolt=(-8.0, -14.0))
    clip("die_pillion_slump", DIE, False, "seat", "slung", [p for c in PILLION for p in C.CREW[c]["place"]],
         "The pillion killed: struck back, he sags back and to his right, his hands off the handle, then his head drops "
         "forward; still astride, the last pose held. Played from bike_pillion .. bike_pillion_d.")(
        calm("die_pillion_slump", fn3, on_saddle))

    # the sidecar's man: struck back against the backrest, then slumped to his right over the tub's rim
    P0 = C.CREW["sidecar_gunner"]["fn"](0.0)
    F1 = limp(slumped(P0, fwd=-6.0, side=-10.0, head=(-12.0, -10.0, -8.0)), on_thigh("l", 0.15, 0.12, 0.16), on_thigh("r", 0.15, 0.12, 0.16))
    F2 = limp(slumped(P0, fwd=6.0, side=-22.0, head=(34.0, -22.0, -20.0)), on_thigh("l", 0.14, 0.12, 0.13),
              hang("r", B((0.18, TUB["half"] - 0.03, 0.10))))
    fn4 = die_keys("sidecar_gunner", [(0.6, F1), (DIE_REST, F2)], jolt=(-8.0, -14.0))
    clip("die_sidecar_slump", DIE, False, "seat", "none", [p for c in list(SIDECAR) + ["sidecar_ride"] for p in C.CREW[c]["place"]],
         "A man killed in the sidecar: struck back against the backrest, his hands off the gun, he slumps to his right "
         "against the tub's side, his head hanging; the last pose held. Played from sidecar_gunner, sidecar_gunner_b "
         "and sidecar_ride.")(calm("die_sidecar_slump", fn4, seat))

    # the Jeep's gunner: struck back off the gun, his knees go and he drops back onto the back seat behind him
    P0 = C.CREW["jeep_gunner"]["fn"](0.0)
    back = float(rel("Jeep", "gunner", "rider.2")[0])          # (the back seat's point behind his feet; its top 0.38 up)
    F1 = dict(P0)
    F1 = limp(F1, hang("l", (0.10, 0.30, 0.75)), hang("r", (0.10, -0.30, 0.75)))
    F1["pelvis_at"] = add3(P0["pelvis_at"], (-0.10, 0.0, -0.12))
    F1["pelvis"] = (2.0, 0.0, 0.0)
    F1["spine_02"] = add3(P0["spine_02"], (-8.0, 0.0, 0.0))
    F1["chest"] = add3(P0["chest"], (-8.0, 0.0, 0.0))
    F1["head"] = add3(P0["head"], (-14.0, 0.0, 6.0))
    F2 = dict(F1)
    F2["pelvis_at"] = (back + 0.09, 0.0, 0.38 + 0.17 - C.MAN["pelvis"][2])     # (his kit clear of the backrest)
    F2["pelvis"] = (-14.0, 4.0, 0.0)
    F2["spine_01"], F2["spine_02"], F2["chest"] = (10.0, 6.0, 0.0), (12.0, 6.0, 4.0), (10.0, 4.0, 4.0)
    F2["neck"], F2["head"] = (18.0, 8.0, 10.0), (22.0, 8.0, 12.0)
    F2["foot_l"] = {"at": (0.14, 0.16, C.MAN["ankle"]), "yaw": 14.0, "pole": (1.0, 0.4, 0.6)}
    F2["foot_r"] = {"at": (0.06, -0.18, C.MAN["ankle"]), "yaw": -20.0, "pole": (1.0, -0.4, 0.6)}
    F2 = limp(F2, on_thigh("l", back + 0.30, 0.14, 0.50 - 0.02), hang("r", (back + 0.05, -0.32, 0.42)))
    F15 = S.mix([F1, F2], [0.35, 0.65])
    F15["pelvis_at"] = add3(F15["pelvis_at"], (0.0, 0.0, 0.09))      # (over the seat's edge before he drops onto it)
    fn5 = die_keys("jeep_gunner", [(0.45, F1), (0.9, F15), (DIE_REST, F2)], jolt=(-10.0, -16.0))
    clip("die_gunner_slump", DIE, False, "feet", "slung", list(C.CREW["jeep_gunner"]["place"]),
         "The Jeep's gunner killed at the gun: struck back off its grips, his knees give and he drops back onto the "
         "back seat behind him, slumped there with his head on his chest; the last pose held.")(calm("die_gunner_slump", fn5, _plain))


SEATS, DEATHS = [], []


def make_clips():
    """Every clip (after crew.setup(): the fits are measured on the pattern man)."""
    for f in (make_drivers, make_riders, make_astride, make_pillions, make_sidecars, build_deaths):
        f()
    SEATS[:] = [n for n in C.CREW if n in MODE and not n.startswith("die_")]
    DEATHS[:] = [n for n in C.CREW if n in MODE and n.startswith("die_")]
    return SEATS + DEATHS


# ================================================================================================ checking at the seats
def report(names, step=4):
    """Each clip's hands against their grips and boots against where they go, over its frames (every `step`th key)."""
    rig = C.MODEL["rig"]
    out = {}
    for name in names:
        c = C.CREW[name]
        rate = RATE["loop" if c["loop"] else "death"]
        N = int(round(c["long"] * rate))
        worst = {"hand": 0.0, "leg": 0.0}
        for f in range(0, N + 1, step):
            P = c["fn"](f / rate)
            st = rig.solve(P, lift_knees=0 if c["anchor"] == "seat" else 4)
            r = C.pose_report(P, st)
            worst["hand"] = max([worst["hand"]] + list(r["hand_gap"].values()))
            worst["leg"] = max([worst["leg"]] + list(r["leg_short"].values()))
        out[name] = {k: round(v, 4) for k, v in worst.items()}
        print(f"  {name:20s} hand gap {worst['hand'] * 100:4.1f} cm, leg short {worst['leg'] * 100:4.1f} cm"
              + (f", lean {C.CREW[name]['fn'].lean:.0f}" if hasattr(C.CREW[name]["fn"], "lean") else ""))
    return out


TUB_SEATS = {("R75", "gunner"), ("M72", "gunner"), ("Rikuo", "gunner"), ("Motorcycle", "rider.2")}


def tub_wall(x, z):
    """The sidecar tub's inner half-width at x (forward of the cushion's point) and height z (over the cushion), as the
    tubs measure (rays from the middle: 0.29 to x 0.5, 0.19 at the nose; less low down)."""
    w = 0.29 if x <= 0.5 else 0.29 - (x - 0.5) * 0.25
    return w - (0.04 if z < 0.0 else 0.0)


def tub_check(name, step=3):
    """How far a sidecar man goes into his tub (its walls are a closed shell that crew.check's inside test cannot read):
    under its deck (0.215 over the cushion from 0.2 ahead), through its sides, through its floor (0.13 under)."""
    c = C.CREW[name]
    rig, skin = C.MODEL["rig"], C.MODEL["skin"]
    rate = RATE["loop" if c["loop"] else "death"]
    N = int(round(c["long"] * rate))
    worst = (0.0, None)
    for f in range(0, N + 1, step):
        st = rig.solve(c["fn"](f / rate))
        pts, bones = skin.points(st)
        g = np.array([C.g2b(p) for p in pts])
        for p, b in zip(g, bones):
            x, y, z = p
            d = 0.0
            if 0.2 < x < 0.95 and abs(y) < tub_wall(x, z) and 0.215 < z < 0.255:
                d = min(z - 0.215, 0.255 - z)          # (into the deck: from under it or from its top)
            if -0.2 < x < 0.95 and -0.13 < z < 0.215 and tub_wall(x, z) < abs(y) < tub_wall(x, z) + 0.05:
                d = max(d, abs(y) - tub_wall(x, z))
            if -0.2 < x < 0.95 and abs(y) < tub_wall(x, z) and -0.2 < z < -0.13:
                d = max(d, -0.13 - z)
            if d > worst[0]:
                worst = (round(float(d), 4), (round(f / rate, 3), str(b), tuple(round(float(v), 3) for v in p)))
    return {"deepest": worst[0], "where": worst[1]}


def vehicles_of(names):
    out = {}
    for n in names:
        for k, v, s, off in C.CREW[n]["place"]:
            out.setdefault(v, []).append((n, s, off))
    return out


def check_all(names, step=8):
    """Every clip at every seat it is played at, against the vehicle's meshes (crew.check: how deep the man goes into
    it, past 1.2 cm, and his hands' gap to their grips), every `step`th frame of 60."""
    res = {}
    only = set(filter(None, os.environ.get("RIDERS_ONLY_VEH", "").split(",")))
    for v, items in vehicles_of(names).items():
        if only and v not in only:
            continue
        t0 = time.time()
        objs = C.load_piece("vehicle", v)
        tree, bm = C.env_tree(objs)
        socks_b = C.sockets_of("vehicle", v)
        for n, s, off in items:
            Mw = C.place_matrix("vehicle", v, s, off, socks_b)
            c = C.CREW[n]
            N = int(round(c["long"] * C.FPS))
            r = C.check(C.MODEL["man"], n, Mw, tree, times=[f / C.FPS for f in range(0, N + 1, step)])
            if (v, s) in TUB_SEATS:
                r.update(tub_check(n), points_past_tol=None, how="tub_check (the tub's shell fools crew.check's inside test)")
            r["at"] = f"{v} {s}"
            res.setdefault(n, []).append(r)
            print(f"  {n:20s} {v:10s} {s:8s} deepest {r['deepest'] * 100:4.1f} cm ({r['where']}), past 1.2 cm {r['points_past_tol']}, "
                  f"grip gap {r['grip_gap'] * 100:.1f} cm")
        bm.free()
        for o in objs:
            if o.name in bpy.data.objects:
                bpy.data.objects.remove(o, do_unlink=True)
        print(f" {v}: {time.time() - t0:.0f} s")
    return res


# ================================================================================================ the reference
FLIP = np.diag([1.0, -1.0, 1.0])


def gq(D):
    """A turn in Blender's axes as a quaternion (w, x, y, z) in the game's."""
    q = Matrix((FLIP @ np.asarray(D, float) @ FLIP).tolist()).to_quaternion()
    return [round(q.w, 6), round(q.x, 6), round(q.y, 6), round(q.z, 6)]


def gp(p):
    return [round(float(x), 5) for x in C.g2b(p)]


STEADY = 35.0      # (degrees: an arm turning more than this from one key to the next is solved again, held to the last)


def _ang(A, B):
    c = (np.trace(np.asarray(A).T @ np.asarray(B)) - 1.0) / 2.0
    return math.degrees(math.acos(max(-1.0, min(1.0, c))))


def _with_pole(h, pole):
    if "blend" in h:
        return {"blend": [(w, _with_pole(x, pole)) for w, x in h["blend"]]}
    h = dict(h)
    h.pop("pole", None)
    h["pole_world"] = tuple(float(v) for v in pole)
    h["flip"] = False
    return h


def steady(rig, P, st, prev, lift):
    """The arm solver picks each key's easiest elbow on its own, and where two are near as easy (a blend of two holds,
    a hand let go) it can swing the elbow round between keys: such an arm is solved again with its elbow held to
    the side it was on at the last key."""
    def jump(s_, s):
        return max(_ang(s_["D"][b], prev["D"][b]) for b in ("upperarm_" + s, "lowerarm_" + s))
    bad = [s for s in "lr" if jump(st, s) > STEADY]
    if not bad:
        return st
    P2 = dict(P)
    for s in bad:
        S_, E_, W_ = prev["H"]["upperarm_" + s], prev["H"]["lowerarm_" + s], prev["H"]["hand_" + s]
        P2["hand_" + s] = _with_pole(P["hand_" + s], E_ - 0.5 * (S_ + W_))
    st2 = rig.solve(P2, lift_knees=lift)
    return st2 if max(jump(st2, s) for s in bad) < max(jump(st, s) for s in bad) else st


def reference(names):
    """Each clip sampled on the pattern man at its rate (10 a second for the seats' loops, 15 for the deaths): every
    bone's turn from its rest (the world's, the game's axes), the pelvis's place, each hand's wrist and grip, each
    ankle, and the rifle's frame and its butt's floor point where he holds one."""
    rig = C.MODEL["rig"]
    J = rig.J
    names_b = rig.names
    out = {"made": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "by": "light_riders.py (crew.py's pattern man, GER)",
           "bones": names_b, "rest": {b: gp(rig.H0[b]) for b in names_b},
           "grip_rest": {s: gp(S.grip_point(J, s)) for s in ("l", "r")},
           "rifle_pts": [gp(p) for p in C.RIFLE["pts"][::7]], "clips": {}}
    for name in names:
        c = C.CREW[name]
        rate = RATE["loop" if c["loop"] else "death"]
        N = int(round(c["long"] * rate))
        rig.prev_q = {}
        prev = None
        rec = {"long": c["long"], "loop": c["loop"], "rate": rate, "anchor": c["anchor"], "mode": MODE[name], "about": c["about"],
               "places": [{"kind": k, "piece": p, "socket": s, "root_offset": [round(float(x), 4) for x in off]} for k, p, s, off in c["place"]],
               "times": [], "q": [], "pelvis": [], "wrist": {"l": [], "r": []}, "grip": {"l": [], "r": []},
               "ankle": {"l": [], "r": []}}
        if name in DEATH:
            rec["death"] = DEATH[name]
        if MODE[name] == "rifle":
            rec.update(W=[], butt=[], hold=[])
        for f in range(N + 1):
            t = 0.0 if (c["loop"] and f == N) else f / rate
            P = c["fn"](t)
            lift = 0 if c["anchor"] == "seat" else 4
            st = rig.solve(P, lift_knees=lift)
            if prev is not None and not (c["loop"] and f == N):
                st = steady(rig, P, st, prev, lift)
            prev = st
            rec["times"].append(round(f / rate, 4))
            rec["q"].append([gq(st["D"][b]) for b in names_b])
            rec["pelvis"].append(gp(st["H"]["pelvis"]))
            for s in ("l", "r"):
                rec["wrist"][s].append(gp(st["H"]["hand_" + s]))
                rec["grip"][s].append(gp(rig.at(st, "hand_" + s, S.grip_point(J, s))))
                rec["ankle"][s].append(gp(st["H"]["foot_" + s]))
            if MODE[name] == "rifle":
                W = rig.frame(st, "weapon")
                pts = C.RIFLE["pts"] @ W[:3, :3].T + W[:3, 3]
                rec["W"].append(gp(W[:3, 3]) + gq(W[:3, :3]))
                rec["butt"].append(gp(pts[int(np.argmin(pts[:, 2]))]))
                rec["hold"].append([round(float(x), 4) for x in HOLD[name](t)])
        out["clips"][name] = rec
        print(f"  ref {name}: {N + 1} keys")
    os.makedirs(os.path.dirname(REF), exist_ok=True)
    with open(REF + ".part", "w") as f:
        json.dump(out, f, separators=(",", ":"))
    os.replace(REF + ".part", REF)
    return out


# ================================================================================================ the pictures
def add_pictures():
    P = C.PICTURES
    P.clear()
    P["riders_jeep"] = {"kind": "vehicle", "piece": "Jeep", "men": [("car_drive", 0.6, "driver"), ("car_ride", 1.1, "rider.1"),
                                                                    ("jeep_gunner", 1.3, "gunner")],
                        "eye": (2.9, -3.4, 2.6), "target": (-0.4, 0.0, 1.0), "lens": 38,
                        "about": "The Jeep: its driver (car_drive), the man beside him (car_ride) and the gunner standing at the post (jeep_gunner)."}
    P["riders_kubelwagen"] = {"kind": "vehicle", "piece": "Kubelwagen", "men": [("car_drive_b", 0.6, "driver"), ("car_ride", 1.1, "rider.1"),
                                                                                ("car_ride_back", 2.0, "rider.2"), ("car_ride_back", 3.1, "rider.3")],
                              "eye": (2.6, -3.6, 2.9), "target": (-0.4, 0.0, 0.9), "lens": 36,
                              "about": "The Kubelwagen full: driver (car_drive_b), the man beside him (car_ride), two in the back (car_ride_back)."}
    P["riders_r75"] = {"kind": "vehicle", "piece": "R75", "men": [("bike_astride", 0.6, "driver"), ("bike_pillion", 1.2, "rider.1"),
                                                                  ("sidecar_gunner_b", 1.6, "gunner")],
                       "eye": (2.4, -2.9, 1.9), "target": (-0.2, 0.25, 0.9), "lens": 38,
                       "about": "The R75 combination: the rider astride (bike_astride), the pillion (bike_pillion), the sidecar's gunner (sidecar_gunner_b)."}
    P["riders_m72"] = {"kind": "vehicle", "piece": "M72", "men": [("bike_astride", 2.1, "driver"), ("bike_pillion_b", 2.6, "rider.1"),
                                                                  ("sidecar_gunner", 0.6, "gunner")],
                       "eye": (-1.6, 3.4, 2.2), "target": (-0.2, 0.25, 0.9), "lens": 38,
                       "about": "The M72 from its sidecar's side: rider (bike_astride), pillion (bike_pillion_b), gunner (sidecar_gunner)."}
    P["riders_rikuo"] = {"kind": "vehicle", "piece": "Rikuo", "men": [("bike_astride_b", 1.0, "driver"), ("bike_pillion_c", 1.5, "rider.1"),
                                                                      ("sidecar_gunner", 2.2, "gunner")],
                         "eye": (2.4, -2.9, 1.9), "target": (-0.2, 0.25, 0.9), "lens": 38,
                         "about": "The Rikuo: rider on the footboards (bike_astride_b), pillion (bike_pillion_c), gunner (sidecar_gunner)."}
    P["riders_alce_wla"] = {"kind": "vehicle", "piece": "Alce", "men": [("bike_astride", 1.4, "driver"), ("bike_pillion_d", 0.8, "rider.1")],
                            "also": [{"kind": "vehicle", "piece": "WLA", "at": (0.0, 1.6), "yaw": 0.0, "men": [("bike_astride_b", 2.4, "driver")]}],
                            "eye": (1.8, -3.6, 1.7), "target": (-0.3, 0.8, 0.9), "lens": 36,
                            "about": "The Alce with its second man on his own saddle (bike_pillion_d), and the WLA beside it (bike_astride_b)."}
    P["riders_cars"] = {"kind": "vehicle", "piece": "GAZ67", "men": [("car_drive", 1.6, "driver"), ("car_ride_back", 0.4, "rider.3")],
                        "also": [{"kind": "vehicle", "piece": "Fiat508", "at": (0.0, 2.2), "yaw": 0.0, "men": [("car_drive_b", 2.2, "driver"), ("car_ride", 3.0, "rider.1")]},
                                 {"kind": "vehicle", "piece": "Kurogane", "at": (0.0, 4.4), "yaw": 0.0, "men": [("car_drive_c", 0.9, "driver"), ("car_ride_back", 1.9, "rider.2")]}],
                        "eye": (5.8, -2.6, 3.4), "target": (-0.3, 2.2, 0.8), "lens": 32,
                        "about": "The GAZ-67, the Fiat 508 (right-hand drive) and the Kurogane (right-hand drive) with their drivers and men."}
    P["riders_deaths"] = {"kind": "vehicle", "piece": "Jeep", "men": [("die_gunner_slump", DIE, "gunner"), ("die_seat_slump", DIE, "driver"),
                                                                      ("die_ride_slump", DIE, "rider.1")],
                          "also": [{"kind": "vehicle", "piece": "R75", "at": (0.0, 2.2), "yaw": 0.0,
                                    "men": [("die_bike_slump", DIE, "driver"), ("die_pillion_slump", DIE, "rider.1"), ("die_sidecar_slump", DIE, "gunner")]}],
                          "eye": (4.6, -3.0, 3.2), "target": (-0.4, 1.1, 0.8), "lens": 34,
                          "about": "Each seat's death at its end: the Jeep's gunner, driver and the man beside him; the R75's rider, pillion and gunner."}


def main(argv):
    only = set(argv[argv.index("--only") + 1].split(",")) if "--only" in argv else {"clips", "check", "ref"}
    fast = "--fast" in argv
    t0 = time.time()
    import kit
    kit.fresh(C.FPS)
    m = C.setup("GER", "rifle")
    names = make_clips()
    print(f"setup and clips {time.time() - t0:.0f} s: {len(SEATS)} seats, {len(DEATHS)} deaths")
    state_path = os.path.join(WORK, "light_riders_state.json")
    state = json.load(open(state_path)) if os.path.exists(state_path) else {}
    if "clips" in only:
        state["report"] = report(names)
    if "check" in only:
        pick = argv[argv.index("--clips") + 1].split(",") if "--clips" in argv else names
        state["checks"] = check_all(pick)
        print(f"checked {time.time() - t0:.0f} s")
    os.makedirs(WORK, exist_ok=True)
    json.dump(state, open(state_path, "w"), indent=1)
    if "ref" in only:
        reference(names)
        print(f"reference {time.time() - t0:.0f} s")
    if "pictures" in only:
        add_pictures()
        want = [a for a in argv if a.startswith("riders_")] or list(C.PICTURES)
        for pn in want:
            path = C.build_picture(pn, fast=fast)
            print("picture", path, f"{time.time() - t0:.0f} s")
            make_clips()           # (build_picture starts afresh: the clips are measured again on its man)


if __name__ == "__main__":
    main(sys.argv)
