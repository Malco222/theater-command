"""Tank crews bailing out (release 2; Brian, 23 November 2026 14:53Z: "if a tank gets hit people try and get out
before it explodes"), asked of the soldiers by the effects thread for its bail-out (Battle/LandBail.cpp): up out of
the hatch, down off the hull, and a man on fire running, then falling and rolling. Made the way truck_riders.py makes
its ways out (crew.py's pattern man, light_riders.py's clips and reference), the climb checked against the three
Normandy tanks' own meshes, written to tank_bail_ref.json for light_riders_fit.py to fit to every infantry file and
the two tank crew files.

    python tank_bail.py --only clips,check,ref       make, check, write the reference
    python tank_bail.py --only pictures [--fast]     the pictures

The clips (his rifle put away: weapon none, the game hides it; Blender's axes here, x forward, y left, z up; the
root at his boots where each starts):

    bail_climb_out     stood in the commander's hatch (his boots 0.92 under the tank's `commander` socket, the rim
                       1.17 over them, as commander_hatch): both hands on the rim, a boot up on the seat and the other
                       on a foothold higher inside, heaved up on his arms, his right knee onto the roof beside the
                       hatch, the left boot out, up on the roof crouched. Root motion: up 0.95 (the roof), 0.47 to his
                       right and 0.10 forward.
    bail_jump_down     stood at the edge of the deck: a crouch, the jump out and down 1.5 m, landing deep on both
                       boots and coming up into his first running stride. Root motion: down 1.5, 1.6 forward.
    burning_run        a loop of three running strides (2.1 s, 7.35 m: the game moves him): on fire, his hands at
                       his face, beating at his chest, flailing; head down and shaking.
    burning_fall_roll  from burning_run's first frame: he stumbles, falls forward onto his hands and knees and his
                       front, rolls over one and a half times to his right, writhes on his back and lies still.
                       Root motion: where his pelvis lies (on the ground under it).
"""
import json
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
os.environ.setdefault("CREW_WORK", "/home/user/art/trucks/crew/")
os.environ.setdefault("RIDERS_WORK", "/home/user/art/bail/")

import light_riders as LR  # noqa: E402  (crew.py and soldier.py with it, bpy)
import numpy as np  # noqa: E402

C, S = LR.C, LR.S
from soldier import add3, ramp, bump, sm, E, R_axis, _n, swing_arm, M4  # noqa: E402

WORK = os.environ["RIDERS_WORK"]
LR.REF = os.path.join(WORK, "tank_bail_ref.json")
LR.RATE = dict(LR.RATE, loop=30, death=30)       # (the bail-outs are quick: keys at 30 a second)

TANKS = {"Sherman": {"rim": 2.615}, "PanzerIV": {"rim": 2.625}, "Tiger": {"rim": 2.80}}
for _v, _d in TANKS.items():                     # (tank_crew.py's: the mesh counts only over the rim; the hatch open)
    if _v != "PanzerIV":
        C.STAGES[("vehicle", _v)] = {"open_up": True, "cupola": True,
                                     "keep": (lambda rim: (lambda W: W[:, 2] > rim + 0.005))(_d["rim"])}
HATCH_OFF = (0.0, 0.0, -0.92)                    # (his boots from the `commander` socket, stood in the hatch)
RIM = 1.17                                       # (the rim over his boots)
ROOF = 0.95                                      # (the turret's roof beside the hatch over his boots: the Panzer IV's
                                                 #  0.82, the Tiger's 0.97, the Sherman's 1.07; the game adds the
                                                 #  difference to his height from the knee on, 0.8 s)
DROP = 1.5                                       # (bail_jump_down: the deck's edge over the ground)
NAMES = []
ROOT = {}
MEAS = {}


def ank():
    return C.MAN["ankle"]


KNEEL_PITCH, KNEEL_TOE = 60.0, 68.0


def kneel_ankle(pitch=KNEEL_PITCH, ball=0.045):
    """A kneeling boot (heel up, the toes bent flat): its ankle's height over the ground for the ball of the foot
    to be `ball` over it (the boot's sole there)."""
    H0 = C.MODEL["rig"].H0
    d = np.asarray(H0["ball_r"], float) - np.asarray(H0["foot_r"], float)
    rest = math.atan2(-d[2], math.hypot(d[0], d[1]))
    return ball + float(np.linalg.norm(d)) * math.sin(rest + math.radians(pitch))


def away(P):
    P["weapon"], P["weapon_hidden"] = C.stowed(), True
    return P


def body(x=0.0, y=0.0, z=0.0, lean=0.0, side=0.0, turn=0.0, head=0.0, look=0.0, twist=0.0):
    """Upright as base_stand, the pelvis moved by (x, y, z), bent forward `lean` degrees through hips and spine (his
    head kept up by `head` of it), leaning `side` to his left, turned `turn` to his left (the chest `twist` more),
    looking `look` to his left; no rifle."""
    P = S.base_stand()
    P["pelvis_at"] = (x, y, z)
    P["pelvis"] = add3(P["pelvis"], (0.5 * lean, 0.5 * side, turn))
    P["spine_01"] = add3(P["spine_01"], (0.2 * lean, 0.2 * side, 0.3 * twist))
    P["spine_02"] = add3(P["spine_02"], (0.15 * lean, 0.15 * side, 0.3 * twist))
    P["chest"] = add3(P["chest"], (0.15 * lean, 0.15 * side, 0.4 * twist))
    P["neck"] = add3(P["neck"], (-0.25 * lean + 0.5 * head, 0.0, 0.4 * look))
    P["head"] = add3(P["head"], (-0.25 * lean + 0.5 * head, 0.0, 0.6 * look))
    P["fingers_l"], P["fingers_r"], P["thumb_l"], P["thumb_r"] = 35.0, 35.0, 25.0, 25.0
    return away(P)


def foot(at, yaw=0.0, side="l", **more):
    sg = 1.0 if side == "l" else -1.0
    f = {"at": tuple(float(v) for v in at), "yaw": yaw, "pole": more.pop("pole", (1.0, 0.08 * sg, 0.35))}
    f.update(more)
    return f


class Steps:
    """A boot's way: planted [(t0, t1, (x, y, z), yaw, extra)] (z the sole's height: the ankle goes ANKLE over it;
    extra the foot's pitch, toe and knee's pole when kneeling), swung between plants in an arc `lift` over the higher
    end (a list per swing, or one for all)."""

    def __init__(self, side, plants, lift=0.08, knee=None):
        self.side, self.plants = side, [p if len(p) == 5 else p + ({},) for p in plants]
        self.lift = lift if isinstance(lift, (list, tuple)) else [lift] * len(plants)
        self.knee = knee or {}                   # ({swing number: the knee's pole mid-swing}: where the boot comes up
                                                 #  level with his hip, the plants' poles lie along the leg)

    def _f(self, p, z_extra=0.0):
        x, y, z = p[2]
        ex = dict(p[4])
        return foot((x, y, z + ank() + z_extra + ex.pop("dz", 0.0)), p[3], self.side, **ex)

    def at(self, t):
        pl = self.plants
        if t <= pl[0][1]:
            return self._f(pl[0])
        for i in range(len(pl) - 1):
            a, b = pl[i], pl[i + 1]
            if t <= a[1]:
                return self._f(a)
            if t < b[0]:
                u = (t - a[1]) / (b[0] - a[1])
                fa, fb = self._f(a), self._f(b)
                s_ = sm(u)
                out = S.mix([fa, fb], [1.0 - s_, s_])
                at = np.asarray(out["at"], float)
                at[2] += self.lift[i] * math.sin(math.pi * u)
                out["at"] = tuple(at)
                if i in self.knee:
                    b = math.sin(math.pi * u)
                    out["pole"] = tuple(_n((1.0 - b) * np.asarray(out["pole"], float) + b * np.asarray(self.knee[i], float)))
                return out
        return self._f(pl[-1])


def clip(name, long, loop, places, about, root=None):
    def deco(fn):
        LR.clip(name, long, loop, "feet", "none", places, about)(fn)
        LR.HOLD[name] = lambda t: (1.0, 1.0)
        if root is not None:
            ROOT[name] = root
        NAMES.append(name)
        return fn
    return deco


def root_track(keys):
    rk = np.array([[t, x, y, z] for t, (x, y, z) in keys], float)

    def root(t):
        return tuple(float(np.interp(t, rk[:, 0], rk[:, i])) for i in (1, 2, 3))
    return root


# ================================================================================================ up out of the hatch
HELD = 0.15                                      # (the elbows held to their poles: truck_riders.py's POLE_HELD)


def rim_hand(s, clock, lift=0.0, r=0.31):
    """A hand pressed flat on the hatch's rim at `clock` degrees round from straight ahead (+ to his left), the
    fingers over its outer edge, the elbow back and out."""
    sg = 1.0 if s == "l" else -1.0
    ph = math.radians(clock)
    radial = np.array((math.cos(ph), math.sin(ph), 0.0))
    at = (r * radial[0], r * radial[1], RIM + 0.03 + lift)
    a = _n(radial + np.array((0.0, 0.0, -0.45)))
    return {"at": at, "a": tuple(a), "n": (0.0, 0.0, -1.0), "pole_world": (-1.0, 0.5 * sg, 0.6), "roll": 10.0,
            "pole_weight": 1.0}


def roof_hand(s, at, a=None):
    """A hand flat on the roof (at: x, y and its height over the roof), the fingers ahead and out."""
    sg = 1.0 if s == "l" else -1.0
    return {"at": (at[0], at[1], ROOF + 0.03 + at[2]), "a": a or (0.8, 0.2 * sg, -0.55), "n": (0.0, 0.0, -1.0),
            "pole_world": (-0.9, 0.8 * sg, 0.1), "roll": 10.0, "pole_weight": HELD}


def free_hand(s, swing=20.0, out=18.0, elbow=60.0):
    return {"arm": swing_arm(swing, out=out, rot=20.0, elbow=elbow, pron=-10.0)}


def hand_way(t, ways):
    """A hand along [(t0, t1, spec)]: each spec held from t0 to t1, blended into the next between."""
    for i, (t0, t1, h) in enumerate(ways):
        if t <= t1:
            if i == 0 or t >= t0:
                return h
            pt1, ph = ways[i - 1][1], ways[i - 1][2]
            k = ramp(t, pt1, t0)
            return {"blend": [(1.0 - k, ph), (k, h)]}
    return ways[-1][2]


def make_climb():
    R = ROOF
    seat, hold = 0.30, 0.70                       # (the commander's seat, a foothold higher inside: the boots' soles)
    kneel = {"pitch": KNEEL_PITCH, "toe": KNEEL_TOE, "pole": (1.0, -0.35, -0.6), "dz": kneel_ankle() - ank()}
    # his pelvis: up the hatch on his legs and arms to the rim's height, then over onto his right knee, leaning far
    # forward over the roof with his hands on it, then up on his feet
    keys = [(0.0, dict(z=-0.02, lean=6.0, head=8.0)),
            (0.20, dict(x=-0.01, z=-0.06, lean=8.0, head=10.0)),
            (0.44, dict(x=0.0, y=-0.02, z=0.20, lean=10.0, head=10.0)),
            (0.62, dict(x=0.03, y=-0.10, z=0.42, lean=22.0, head=16.0, side=-4.0)),
            (0.84, dict(x=0.04, y=-0.34, z=R - 0.53, lean=80.0, head=42.0, side=-6.0, turn=-6.0)),
            (1.02, dict(x=0.06, y=-0.44, z=R - 0.49, lean=72.0, head=38.0, turn=-8.0)),
            (1.22, dict(x=0.10, y=-0.47, z=R - 0.22, lean=32.0, head=16.0, turn=-6.0)),
            (1.50, dict(x=0.10, y=-0.47, z=R - 0.06, lean=10.0, head=6.0, turn=-4.0, look=-30.0))]
    keyed = S.keyed([(t, body(**kw)) for t, kw in keys])
    fr = Steps("r", [(0.0, 0.16, (0.03, -0.12, 0.0), -8.0),
                     (0.30, 0.54, (0.06, -0.15, seat), -10.0),
                     (0.78, 1.10, (-0.26, -0.47, R), -10.0, kneel),
                     (1.32, 1.5, (-0.04, -0.58, R), -14.0)], lift=[0.10, 0.45, 0.12])
    fl = Steps("l", [(0.0, 0.34, (0.03, 0.12, 0.0), 8.0),
                     (0.50, 0.86, (0.02, 0.02, hold), 6.0),
                     (1.08, 1.5, (0.30, -0.40, R), -6.0, {"pole": (1.0, 0.1, 0.5)})], lift=[0.10, 0.36],
               knee={1: (0.4, 0.3, 1.0)})
    # the hands on the rim at his sides (pressing up out of it, the elbows back), then over onto the roof ahead as
    # he leans out over it, then off it as he gets up
    ways_r = [(0.0, 0.60, rim_hand("r", -65.0)),
              (0.78, 1.00, roof_hand("r", (0.58, -0.62, 0.0))),
              (1.24, 1.5, free_hand("r", swing=30.0, out=22.0, elbow=70.0))]
    ways_l = [(0.0, 0.60, rim_hand("l", 65.0)),
              (0.82, 0.98, roof_hand("l", (0.62, -0.16, 0.0))),
              (1.26, 1.5, free_hand("l", swing=24.0, out=22.0, elbow=70.0))]

    def fn(t):
        P = dict(keyed(t))
        P["foot_l"], P["foot_r"] = fl.at(t), fr.at(t)
        P["hand_l"], P["hand_r"] = hand_way(t, ways_l), hand_way(t, ways_r)
        push = bump(t, 0.2, 0.5, 0.8)
        P["clavicle_l"] = add3(P["clavicle_l"], (-10.0 * push, 4.0 * push))[:2]
        P["clavicle_r"] = add3(P["clavicle_r"], (-10.0 * push, 4.0 * push))[:2]
        P["fingers_l"] = P["fingers_r"] = 20.0 + 15.0 * ramp(t, 1.2, 1.5)
        return P

    clip("bail_climb_out", 1.5, False, [("vehicle", v, "commander", HATCH_OFF) for v in TANKS],
         "Bailing out of the commander's hatch: stood in it (his boots 0.92 m under the tank's `commander` socket, the "
         "rim 1.17 m over them, as commander_hatch), both hands on the rim; a boot up on the seat (0.30 m) and the other "
         "on a foothold higher inside (0.70 m), heaved up on his arms and over the rim, his hands onto the roof, his "
         "right knee onto the roof beside the hatch (0.95 m over where his boots began) and his left boot out, and up "
         "on the roof, crouched, looking to his right. Root motion: the root under his boots, up 0.95 m, 0.47 m to his "
         "right and 0.10 m forward (on a roof higher or lower than 0.95 m over the turret's floor, add the difference "
         "to his height from 0.6 s on).",
         root=root_track([(0.0, (0.0, 0.0, 0.0)), (0.30, (0.0, 0.0, 0.0)), (0.84, (-0.02, -0.30, R)),
                          (1.30, (0.10, -0.47, R)), (1.5, (0.10, -0.47, R))]))(fn)


def check_hatch(step=3):
    """The climb at the three tanks' commander hatches: crew.check against each tank's mesh over the rim (only his
    points over the rim: the roof and the turret's inside are below it), tank_crew.py's ring check (how far he goes
    into the hatch's ring), and the hands' gap to the rim and the roof where they are put."""
    import tank_crew as TC
    out = []
    for v in TANKS:
        objs = C.load_piece("vehicle", v)
        st_ = C.STAGES.get(("vehicle", v), {})
        if st_.get("open_up"):
            C.open_up(objs)
        tree, bm = C.env_tree(objs)
        socks = C.sockets_of("vehicle", v)
        Mw = C.place_matrix("vehicle", v, "commander", HATCH_OFF, socks)
        rim_w = TANKS[v]["rim"]
        keep = (lambda W, r=rim_w: W[:, 2] > r + 0.005)
        N = int(round(C.CREW["bail_climb_out"]["long"] * C.FPS))
        r = C.check(C.MODEL["man"], "bail_climb_out", Mw, tree, times=[f / C.FPS for f in range(0, N + 1, step)],
                    keep_point=keep)
        ring, ring_at = TC.cupola_check("bail_climb_out", Mw, step=step)
        rec = {"at": f"{v} commander", "deepest_m": r["deepest"], "where": r["where"], "points_past_1.2cm": r["points_past_tol"],
               "grip_gap_m": r["grip_gap"], "ring_m": round(float(ring), 4), "ring_at": ring_at}
        print(f"  bail_climb_out {v:9s} deepest {r['deepest'] * 100:4.1f} cm {r['where']}, past 1.2 cm {r['points_past_tol']}, "
              f"hands' gap {r['grip_gap'] * 100:.1f} cm, into the ring {ring * 100:.1f} cm {ring_at}")
        out.append(rec)
        bm.free()
        import bpy
        for o in objs:
            if o.name in bpy.data.objects:
                bpy.data.objects.remove(o, do_unlink=True)
    return out


# ================================================================================================ down off the hull
def make_jump():
    G = -DROP
    T0, V0, VX = 0.40, 0.75, 1.55               # (off the edge at T0: up at V0 m/s, forward at VX)
    PA0 = -0.03                                  # (the pelvis's height from standing as he leaves)
    g = 9.81
    land_pa = G - 0.10                           # (touching down: his legs near straight)
    tau = (V0 + math.sqrt(V0 * V0 + 2 * g * (PA0 - land_pa))) / g
    T1 = T0 + tau                                # (touchdown)
    MEAS["jump"] = {"takeoff": T0, "touchdown": round(T1, 3)}
    X0 = 0.10

    def fly(t):
        u = t - T0
        return X0 + VX * u, PA0 + V0 * u - 0.5 * g * u * u

    keys = [(0.0, dict(z=-0.02, lean=6.0, head=4.0)),
            (0.16, dict(x=0.02, z=-0.14, lean=22.0, head=14.0)),
            (0.30, dict(x=0.05, z=-0.22, lean=30.0, head=18.0)),
            (T0, dict(x=X0, z=PA0, lean=14.0, head=8.0))]
    n = 6
    for k in range(1, n):
        t = T0 + tau * k / n
        x, z = fly(t)
        keys.append((t, dict(x=x, z=z, lean=10.0 + 4.0 * k / n, head=6.0)))
    xl, _ = fly(T1)
    keys += [(T1, dict(x=xl, z=land_pa, lean=16.0, head=8.0)),
             (T1 + 0.12, dict(x=xl + 0.12, z=G - 0.40, lean=34.0, head=18.0)),
             (T1 + 0.28, dict(x=xl + 0.30, z=G - 0.22, lean=26.0, head=12.0)),
             (T1 + 0.48, dict(x=xl + 0.62, z=G - 0.10, lean=20.0, head=8.0))]
    END = round(T1 + 0.48, 2)
    keyed = S.keyed([(t, body(**kw)) for t, kw in keys])
    H0 = None

    def follow(t, s, P):
        """In the air: his boots under him, the legs a little bent and drawn up, then reaching for the ground."""
        sg = 1.0 if s == "l" else -1.0
        pz = float(H0[2]) + P["pelvis_at"][2]
        u = min(max((t - T0) / tau, 0.0), 1.0)
        tuck = math.sin(math.pi * min(u * 1.25, 1.0)) * 0.16
        ahead = 0.04 + 0.12 * u - (0.06 if s == "l" else 0.0) * (1 - u)
        return {"at": (P["pelvis_at"][0] + ahead, 0.11 * sg, pz - 0.86 + tuck + ank() - 0.05),
                "yaw": 6.0 * sg, "pitch": 18.0 - 14.0 * u, "toe": 0.0, "pole": (1.0, 0.1 * sg, 0.3)}

    plants = {"l": Steps("l", [(0.0, T0 - 0.06, (0.03, 0.11, 0.0), 6.0), (T1, T1 + 0.24, (xl + 0.02, 0.12, G), 6.0),
                               (T1 + 0.44, END, (xl + 0.72, 0.11, G), 6.0)], lift=[0.0, 0.12]),
              "r": Steps("r", [(0.0, T0, (0.0, -0.11, 0.0), -6.0), (T1 + 0.02, END, (xl + 0.08, -0.12, G), -6.0)])}

    def fn(t):
        nonlocal H0
        if H0 is None:
            H0 = np.asarray(C.MODEL["rig"].H0["pelvis"], float)
        P = dict(keyed(t))
        for s in ("l", "r"):
            t_off = T0 - 0.06 if s == "l" else T0
            t_on = T1 if s == "l" else T1 + 0.02
            if t <= t_off or t >= t_on:
                P["foot_" + s] = plants[s].at(t)
            else:
                a = ramp(t, t_off, t_off + 0.10) * (1.0 - ramp(t, t_on - 0.10, t_on))
                ground = plants[s].at(t_off if t < 0.5 * (t_off + t_on) else t_on)
                P["foot_" + s] = S.mix([ground, follow(t, s, P)], [1.0 - sm(a), sm(a)])
        # the arms: back as he crouches, swung forward and up as he goes, out for balance in the air, forward on
        # landing, then pumping into the run
        k_back = bump(t, 0.0, 0.30, T0)
        k_up = bump(t, 0.28, T0 + 0.05, T0 + 0.30)
        k_out = bump(t, T0 + 0.1, T0 + 0.35, T1)
        k_land = bump(t, T1 - 0.1, T1 + 0.12, T1 + 0.32)
        k_run = ramp(t, T1 + 0.2, END)
        sw = -35.0 * k_back + 70.0 * k_up + 35.0 * k_out + 45.0 * k_land
        out = 14.0 + 45.0 * k_out + 10.0 * k_land
        P["hand_l"] = {"arm": swing_arm(sw + k_run * 40.0, out=out, rot=20.0, elbow=40.0 + 40.0 * k_run, pron=-10.0)}
        P["hand_r"] = {"arm": swing_arm(sw - k_run * 30.0, out=out, rot=20.0, elbow=40.0 + 40.0 * k_run, pron=-10.0)}
        return P

    clip("bail_jump_down", END, False, [],
         f"Down off the tank: stood at the edge of the deck, he crouches, jumps out and down the {DROP:.1f} m (off the "
         f"edge at {T0:.2f} s, touching down at {T1:.2f} s), lands deep on both boots and comes up into his first running "
         "stride, his arms out for balance in the air. Root motion: the root under his boots, from the deck's edge to "
         f"the ground, {DROP:.1f} m down and {xl + 0.6:.2f} m forward. From a higher or lower deck, add the difference to his "
         f"height between {T0:.2f} and {T1:.2f} s.",
         root=root_track([(0.0, (0.0, 0.0, 0.0)), (T0, (X0, 0.0, 0.0)), (T1, (xl, 0.0, G)), (END, (xl + 0.6, 0.0, G))]))(fn)


# ================================================================================================ on fire
FACE = {}


def measure_face():
    """Where his face is in his chest's frame, standing: the hands are put there."""
    rig = C.MODEL["rig"]
    st = rig.solve(S.base_stand())
    Cf = rig.frame(st, "chest")
    eye = 0.5 * (rig.eye(st, "l") + rig.eye(st, "r"))
    FACE["eye"] = np.linalg.inv(Cf) @ np.append(eye, 1.0)
    FACE["eye"] = FACE["eye"][:3]


FACE_HELD = 0.5                                   # (a hand at his face is folded tight: its elbow kept out and down)


def face_hand(s, dx=0.0, dz=0.0, wide=0.0, tucked=False):
    """A hand over his face (in the chest's frame), the palm on it; the elbow out and down, or (tucked: lying, rolling)
    in before his chest, so that it never goes into the ground under his side."""
    sg = 1.0 if s == "l" else -1.0
    e = FACE["eye"]
    at = (float(e[0]) + 0.07 + dx, float(e[1]) + sg * (0.045 + wide), float(e[2]) - 0.05 + dz)
    return {"frame": "chest", "at": at, "a": (0.15, -0.25 * sg, 1.0), "n": (-1.0, -0.2 * sg, 0.0),
            "pole": (1.0, 0.25 * sg, -0.6) if tucked else (0.2, 1.0 * sg, -0.6), "roll": 25.0, "pole_weight": FACE_HELD}


def mid_hand(s, at=(0.30, 0.13, 0.12)):
    """A hand before his chest, thumb up, the palm in: on the way between a hand on his face and one flat on the
    ground, which are half a turn apart (a blend across half a turn spins the wrist over)."""
    sg = 1.0 if s == "l" else -1.0
    return {"frame": "chest", "at": (at[0], abs(at[1]) * sg, at[2]), "a": (0.6, 0.0, 0.8), "n": (0.0, -sg, 0.0),
            "pole": (1.0, 0.25 * sg, -0.6), "roll": 25.0, "pole_weight": FACE_HELD}


def beat_hand(s, k, where="chest"):
    """A hand beating at himself: k 0 out, 1 on him (his chest, or his thigh)."""
    sg = 1.0 if s == "l" else -1.0
    if where == "chest":                          # (his own side of his chest: across it, the elbow goes in)
        on, off = (0.17, 0.07 * sg, 0.04), (0.36, 0.16 * sg, 0.10)
    else:
        on, off = (0.14, 0.12 * sg, -0.50), (0.30, 0.22 * sg, -0.34)
    at = tuple(off[i] + (on[i] - off[i]) * k for i in range(3))
    return {"frame": "chest", "at": at, "a": (0.2, 0.3 * sg, -1.0) if where != "chest" else (0.3, -0.6 * sg, 0.6),
            "n": (-1.0, 0.0, 0.0), "pole": (-0.3, 1.0 * sg, -0.8), "roll": 25.0, "pole_weight": FACE_HELD}


def flail_hand(s, k):
    """A hand flung up and out by his head (k 0 at his ear, 1 high and wide)."""
    sg = 1.0 if s == "l" else -1.0
    e = FACE["eye"]
    at = (float(e[0]) + 0.10 - 0.05 * k, sg * (0.22 + 0.16 * k), float(e[2]) + 0.02 + 0.22 * k)
    # (the palm in toward his head: turned out, it would be half a turn from the hand on his face, and the wrist would
    #  spin over in the blend between them)
    return {"frame": "chest", "at": at, "a": (0.1, 0.3 * sg, 1.0), "n": (-0.2, -0.9 * sg, 0.2),
            "pole": (-0.2, 1.0 * sg, -0.5), "roll": 25.0, "pole_weight": FACE_HELD}


BRUN = 2.1                                        # (three of the run's strides)


def run_pose(t, fall=0.0):
    G = S.RUN
    P = S.base_stand()
    S.gait_pose(G, t, 22.0 + 14.0 * fall, 11.0, 0.02, P)
    ph = 2 * math.pi * t / BRUN
    P["neck"] = (8.0 + 6.0 * math.sin(3 * ph), 6.0 * math.sin(2 * ph + 0.3), 22.0 * math.sin(5 * ph))
    P["head"] = (10.0 + 4.0 * math.sin(4 * ph + 1.0), 8.0 * math.sin(3 * ph), 26.0 * math.sin(5 * ph + 0.6))
    P["chest"] = add3(P["chest"], (6.0, 4.0 * math.sin(2 * ph), 10.0 * math.sin(ph + 0.4)))
    P["clavicle_l"] = (6.0 + 6.0 * math.sin(4 * ph), 8.0)
    P["clavicle_r"] = (6.0 + 6.0 * math.sin(4 * ph + 1.5), 8.0)
    away(P)
    u = (t % BRUN) / BRUN
    # 0-0.3 both hands at his face; 0.3-0.62 the left at his face, the right beating his chest three times; 0.62-0.95
    # both flung up and out, flailing, back to his face as the loop comes round
    beat = 0.5 - 0.5 * math.cos(2 * math.pi * 3.0 * min(max((u - 0.36) / 0.24, 0.0), 1.0))
    hl_face, hr_face = face_hand("l"), face_hand("r")
    w1 = sm(ramp(u, 0.29, 0.36)) * (1.0 - sm(ramp(u, 0.60, 0.67)))
    w2 = sm(ramp(u, 0.60, 0.67)) * (1.0 - sm(ramp(u, 0.90, 0.98)))
    hr_beat = beat_hand("r", beat, "chest")
    P["hand_r"] = ({"blend": [(max(1.0 - w1 - w2, 0.0), hr_face), (w1, hr_beat),
                              (w2, flail_hand("r", 0.5 + 0.5 * math.sin(6 * ph)))]} if (w1 > 1e-3 or w2 > 1e-3)
                   else hr_face)
    w3 = sm(ramp(u, 0.64, 0.71)) * (1.0 - sm(ramp(u, 0.90, 0.98)))
    P["hand_l"] = ({"blend": [(1.0 - w3, hl_face), (w3, flail_hand("l", 0.5 + 0.5 * math.sin(6 * ph + 1.2)))]}
                   if w3 > 1e-3 else hl_face)
    P["fingers_l"] = P["fingers_r"] = 30.0 + 20.0 * math.sin(6 * ph) ** 2
    P["thumb_l"] = P["thumb_r"] = 25.0
    return P


def make_burning_run():
    clip("burning_run", BRUN, True, [],
         "Running on fire, a loop of three strides (2.1 s, 7.35 m: the game moves him, 3.5 m a second): his hands at "
         "his face, then the left there and the right beating at his chest, then both flung up and "
         "flailing; bent forward, his head down and shaking. In place: `stride` 7.35.")(lambda t: run_pose(t))
    LR.C.CREW["burning_run"]["stride"] = 3 * S.RUN.stride


# ---- falling and rolling
def euler_of(R):
    """(fwd, left, turn) for soldier.E from a rotation (R = Rz(turn) Ry(fwd) Rx(-left))."""
    b = math.degrees(math.asin(max(-1.0, min(1.0, -R[2, 0]))))
    a = math.degrees(math.atan2(R[2, 1], R[2, 2]))
    c = math.degrees(math.atan2(R[1, 0], R[0, 0]))
    out = (b, -a, c)
    assert np.allclose(E(*out), R, atol=1e-6), "euler_of"
    return out


def slerp(Ra, Rb, u):
    """The turn u of the way from Ra to Rb, the short way round."""
    Rr = Ra.T @ Rb
    ang = math.acos(max(-1.0, min(1.0, (np.trace(Rr) - 1.0) / 2.0)))
    if ang < 1e-9:
        return Ra.copy()
    ax = np.array((Rr[2, 1] - Rr[1, 2], Rr[0, 2] - Rr[2, 0], Rr[1, 0] - Rr[0, 1]))
    if np.linalg.norm(ax) < 1e-6:                     # (a half turn: the axis from the symmetric part)
        w, v = np.linalg.eigh((Rr + Rr.T) / 2.0)
        ax = v[:, int(np.argmax(w))]
    return Ra @ R_axis(_n(ax), math.degrees(ang) * u)


def lying(phi_deg, tilt=4.0):
    """His pelvis's turn lying along +x, rolled phi degrees to his right from face down (180: on his back), the head
    end a little up."""
    ph = math.radians(phi_deg)
    return R_axis((1, 0, 0), phi_deg) @ R_axis((0, 1, 0), 90.0 - tilt * math.cos(ph))


FALL_AT = 0.38                                    # (he goes down from here)
DOWN = 0.86                                       # (on his front)
ROLL = (DOWN, 2.30)                               # (one and a half turns)
STILL = 3.30
FALL_LONG = 3.8
PX = 0.95                                         # (his pelvis lying, ahead of where the run left it)
ROLL_Y = 0.80                                     # (a turn of his body along the ground)
LIFT = []                                         # (his pelvis lifted so that the lowest of him touches the ground)


def calibrate_lift(pose=None, step=1.0 / 30.0, contact=0.006):
    """Lying: at each time, his lowest point brought to the ground (the pack, the belt's kit on his back, his hips
    on their side, an arm under him as he rolls), smoothed over a tenth of a second (never below what it needs)."""
    rig, skin = C.MODEL["rig"], C.MODEL["skin"]
    pose = pose or lying_pose
    LIFT[:] = []
    ts = np.arange(DOWN, FALL_LONG + 1e-6, step)
    need = []
    for t in ts:
        # (without the solver's own knee lift: with the lift added, the knee is clear and that lift is gone)
        pts, _ = skin.points(rig.solve(pose(float(t)), lift_knees=0))
        need.append(contact - float(pts[:, 2].min()))
    need = np.array(need) / float(rig.fit[0])         # (lying is a low pose: the solver scales its pelvis's height)
    k = 1
    pad = np.concatenate([np.full(k, need[0]), need, np.full(k, need[-1])])
    sm_ = np.array([pad[i:i + 2 * k + 1].max() for i in range(len(need))])
    sm_ = np.maximum(need, np.convolve(np.concatenate([np.full(k, sm_[0]), sm_, np.full(k, sm_[-1])]),
                                       np.ones(2 * k + 1) / (2 * k + 1), mode="valid"))
    LIFT[:] = [list(ts), list(sm_)]
    return float(sm_.min()), float(sm_.max())


def roll_phi(t):
    u = min(max((t - ROLL[0]) / (ROLL[1] - ROLL[0]), 0.0), 1.0)
    return 540.0 * (u * u * (3.0 - 2.0 * u) * 0.35 + u * 0.65)


def lying_pose(t):
    """From his front at DOWN: rolled over to his back, writhing, then still."""
    H0 = np.asarray(C.MODEL["rig"].H0["pelvis"], float)
    phi = roll_phi(t)
    wr = ramp(t, ROLL[1] - 0.1, ROLL[1] + 0.2) * (1.0 - ramp(t, STILL - 0.45, STILL))
    still = sm(ramp(t, STILL - 0.5, STILL))
    rock = 14.0 * math.sin(2 * math.pi * 1.6 * (t - ROLL[1])) * wr + 14.0 * still
    Rb = lying(phi + rock)
    y = -ROLL_Y * phi / 360.0
    z = 0.135 + 0.045 * abs(math.sin(math.radians(phi))) + (float(np.interp(t, LIFT[0], LIFT[1])) if LIFT else 0.0)
    P = {"pelvis_at": (PX - H0[0], y - H0[1], z - H0[2]), "pelvis": euler_of(Rb)}
    arch = 10.0 * wr * (0.5 + 0.5 * math.sin(2 * math.pi * 1.3 * (t - ROLL[1])))
    P["spine_01"] = (2.0 - 0.4 * arch, 0.0, 0.0)
    P["spine_02"] = (3.0 - 0.3 * arch, 0.0, 0.0)
    P["chest"] = (4.0 - 0.3 * arch, 0.0, 0.0)
    toss = 28.0 * math.sin(2 * math.pi * 1.1 * (t - ROLL[1])) * wr
    P["neck"] = (6.0 - 10.0 * still, 0.0, 0.5 * toss + 20.0 * still)
    P["head"] = (4.0 - 8.0 * still, 0.0, 0.5 * toss + 25.0 * still)
    P["clavicle_l"], P["clavicle_r"] = (4.0, 6.0), (4.0, 6.0)
    # the legs: near straight and together as he rolls, drawn up and kicking on his back, fallen aside when still
    pel = np.array((PX, y, z))
    for s, sg in (("l", 1.0), ("r", -1.0)):
        k = wr * (0.5 + 0.5 * math.sin(2 * math.pi * 1.25 * (t - ROLL[1]) + (0.0 if s == "l" else math.pi)))
        k = max(k, 0.55 * still)
        off = np.array((0.04 + 0.30 * k, sg * (0.10 + 0.04 * k) + 0.16 * still, -0.82 + 0.40 * k))
        at = pel + Rb @ off
        at[2] = max(at[2], ank() - 0.015)
        Df = Rb @ R_axis((0, 1, 0), 35.0 + 10.0 * k)
        f, l_, yw = euler_of(Df)
        P["foot_" + s] = {"at": tuple(at), "pitch": f, "roll": l_, "yaw": yw, "toe": 10.0,
                          "pole": tuple(Rb @ np.array((1.0, 0.25 * sg, 0.0)))}
    # the hands: at his face as he rolls and writhes (the right beating at his chest), slack on the ground when still
    beat = 0.5 - 0.5 * math.cos(2 * math.pi * 2.2 * (t - ROLL[1]))
    hl, hr = face_hand("l", dx=0.02, tucked=True), face_hand("r", dx=0.02, tucked=True)
    hr_b = beat_hand("r", beat, "chest")
    w_b = wr * sm(ramp(t, ROLL[1], ROLL[1] + 0.25))
    slack_l = {"frame": "chest", "at": (-0.06, 0.34, -0.28), "a": (0.0, 0.3, -1.0), "n": (-1.0, 0.0, 0.0),
               "pole": (-0.6, 1.0, 0.2), "roll": 40.0}
    slack_r = {"frame": "chest", "at": (-0.04, -0.30, -0.22), "a": (0.0, -0.3, -1.0), "n": (-1.0, 0.0, 0.0),
               "pole": (-0.6, -1.0, 0.2), "roll": 40.0}
    P["hand_l"] = hl if still <= 0.0 else {"blend": [(1.0 - still, hl), (still, slack_l)]}
    hr2 = hr if w_b <= 0.0 else {"blend": [(1.0 - w_b, hr), (w_b, hr_b)]}
    P["hand_r"] = hr2 if still <= 0.0 else {"blend": [(1.0 - still, hr2), (still, slack_r)]}
    P["fingers_l"] = P["fingers_r"] = 40.0 - 20.0 * still
    P["thumb_l"] = P["thumb_r"] = 25.0
    return away(P)


def make_fall_roll():
    run_end = run_pose(FALL_AT, fall=1.0)
    knees = body(x=0.52, z=-0.48, lean=74.0, head=34.0)        # (on all fours: his arms straight down to the ground)
    knees["foot_l"] = foot((0.06, 0.13, kneel_ankle()), 6.0, "l", pitch=KNEEL_PITCH, toe=KNEEL_TOE, pole=(1.0, 0.2, -0.6))
    knees["foot_r"] = foot((0.02, -0.13, kneel_ankle()), -6.0, "r", pitch=KNEEL_PITCH, toe=KNEEL_TOE, pole=(1.0, -0.2, -0.6))
    for s, sg in (("l", 1.0), ("r", -1.0)):
        knees["hand_" + s] = {"at": (1.05, 0.20 * sg, 0.05), "a": (0.8, 0.1 * sg, -0.6), "n": (0.0, 0.0, -1.0),
                              "pole_world": (-0.5, 1.0 * sg, 0.2), "roll": 35.0}
    stumble = run_pose(FALL_AT + 0.12, fall=1.0)
    stumble["pelvis_at"] = add3(stumble["pelvis_at"], (0.20, 0.0, -0.12))
    stumble["pelvis"] = add3(stumble["pelvis"], (14.0, 0.0, 0.0))
    for s, sg in (("l", 1.0), ("r", -1.0)):        # (off his face, before his chest, thumbs up, going for the ground)
        stumble["hand_" + s] = mid_hand(s, at=(0.36, 0.16 * sg, 0.02))
    ground = {s: {"at": (PX + 0.45, 0.22 * sg, 0.06), "a": (0.8, 0.1 * sg, -0.6), "n": (0.0, 0.0, -1.0),
                  "pole_world": (-0.4, 1.0 * sg, 0.5), "roll": 35.0} for s, sg in (("l", 1.0), ("r", -1.0))}

    def lying_full(t):
        P = lying_pose(t)
        # the hands off the ground and up to his face: by way of a hand before his chest, thumb up (palm down on
        # the ground to palm on his face is half a turn, and a blend across half a turn spins the wrist over)
        k = ramp(t, DOWN, DOWN + 0.28)
        if k < 1.0:
            for s in ("l", "r"):
                mid = mid_hand(s)
                if k < 0.5:
                    u = sm(2.0 * k)
                    P["hand_" + s] = {"blend": [(1.0 - u, ground[s]), (u, mid)]}
                else:
                    u = sm(2.0 * k - 1.0)
                    P["hand_" + s] = {"blend": [(1.0 - u, mid), (u, P["hand_" + s])]}
        return P

    lo, hi = calibrate_lift(lying_full)
    MEAS["roll_lift"] = (round(lo, 3), round(hi, 3))
    front = lying_pose(DOWN)
    for s in ("l", "r"):
        front["hand_" + s] = ground[s]
    fall = S.keyed([(FALL_AT, run_end), (FALL_AT + 0.12, stumble), (0.64, knees), (DOWN, front)])

    def feet_down(P, t):
        """From his knees to his front the boots turn over end to end (their pitch past 90): turned as rotations,
        not angle by angle, or they swing through the ground on the way."""
        u = sm(ramp(t, 0.64, DOWN))
        for s in ("l", "r"):
            a, b = knees["foot_" + s], front["foot_" + s]
            Da = E(a.get("pitch", 0.0), a.get("roll", 0.0), a.get("yaw", 0.0))
            Db = E(b.get("pitch", 0.0), b.get("roll", 0.0), b.get("yaw", 0.0))
            f, l_, yw = euler_of(slerp(Da, Db, u))
            at = (1.0 - u) * np.asarray(a["at"], float) + u * np.asarray(b["at"], float)
            at[2] = max(at[2], ank() - 0.015)
            pole = _n((1.0 - u) * np.asarray(a["pole"], float) + u * np.asarray(b["pole"], float))
            P["foot_" + s] = {"at": tuple(at), "pitch": f, "roll": l_, "yaw": yw, "pole": tuple(pole),
                              "toe": (1.0 - u) * a.get("toe", 0.0) + u * b.get("toe", 0.0)}
        return P

    def fn(t):
        if t <= FALL_AT:
            return run_pose(t, fall=ramp(t, 0.0, FALL_AT))
        if t < DOWN:
            P = dict(fall(t))
            if t > 0.64:
                feet_down(P, t)
            return away(P)
        return lying_full(t)

    def root(t):
        if t <= FALL_AT:
            return (0.0, 0.0, 0.0)
        y = -ROLL_Y * roll_phi(t) / 360.0
        return (PX * sm(ramp(t, FALL_AT, DOWN)), y, 0.0)

    clip("burning_fall_roll", FALL_LONG, False, [],
         "On fire, from burning_run's first frame: he stumbles, falls forward onto his hands and knees and onto his "
         "front, rolls over one and a half times to his right (stop, drop and roll), writhes on his back, kicking and "
         f"beating at himself, and lies still by {STILL:.1f} s. Root motion: the ground under his pelvis "
         f"({PX:.2f} m ahead, {1.5 * ROLL_Y:.2f} m to his right at the end).", root=root)(fn)


def make_clips():
    NAMES[:] = []
    ROOT.clear()
    measure_face()
    make_climb()
    make_jump()
    make_burning_run()
    make_fall_roll()
    return list(NAMES)


# ================================================================================================ checks
def ground_check(name, step=2):
    """The lowest of his points over the clip (m from the ground he ends on, or from the deck's top while he is on
    it; on the climb, from the turret's roof, of what of him is out past the hatch's ring): nothing of him under it
    past 1.5 cm."""
    c = C.CREW[name]
    rig, skin = C.MODEL["rig"], C.MODEL["skin"]
    N = int(round(c["long"] * C.FPS))
    worst, at = 1.0, None
    for f in range(0, N + 1, step):
        t = f / C.FPS
        st = rig.solve(c["fn"](t))
        pts, bones = skin.points(st)
        floor = 0.0
        if name == "bail_climb_out":
            # (clear of the cupola, and no deeper than the roof's plate: under that he is inside the turret)
            k = (np.hypot(pts[:, 0], pts[:, 1]) > 0.42) & (pts[:, 2] > ROOF - 0.06)
            if not k.any():
                continue
            pts, bones, floor = pts[k], bones[k], ROOF
        if name == "bail_jump_down":
            floor = 0.0 if t < MEAS["jump"]["takeoff"] + 0.05 else -DROP
            if t < MEAS["jump"]["touchdown"] - 0.02 and t > MEAS["jump"]["takeoff"] + 0.05:
                continue                                  # (in the air: nothing under him)
            if t <= MEAS["jump"]["takeoff"] + 0.05:
                k = pts[:, 0] < 0.25                      # (only what is over the deck: the edge is 0.25 ahead)
                pts, bones = pts[k], bones[k]
        i = int(np.argmin(pts[:, 2]))
        d = float(pts[i, 2] - floor)
        if d < worst:
            worst, at = d, (round(t, 3), str(bones[i]))
    return {"lowest_m": round(worst, 4), "at": at}


def joint_speeds(name):
    """The fastest any bone turns between keys at 30 a second (degrees a key), for the fitter's jump check."""
    c = C.CREW[name]
    rig = C.MODEL["rig"]
    prev, worst = None, (0.0, None)
    for f in range(int(round(c["long"] * 30)) + 1):
        t = f / 30.0
        st = rig.solve(c["fn"](t))
        if prev is not None:
            for b in rig.names:
                if b in ("weapon", "bolt", "bipod"):
                    continue
                Rr = prev["D"][b].T @ st["D"][b]
                ang = math.degrees(math.acos(max(-1.0, min(1.0, (np.trace(Rr) - 1.0) / 2.0))))
                if ang > worst[0]:
                    worst = (round(ang, 1), (round(t, 3), b))
        prev = st
    return worst


# ================================================================================================ pictures
def picture(name, men, eye, target, tanks=(), deck=False, fast=False, lens=30):
    import bpy
    import kit
    from mathutils import Matrix
    kit.fresh(C.FPS)
    m = C.setup("GER", "rifle")
    S.studio_smooth(m)
    make_clips()
    for v, at, yaw in tanks:
        objs = C.load_piece("vehicle", v, at=tuple(C.g2b((at[0], at[1], 0.0))), yaw=-yaw)
        C.open_up(objs)
    if deck:
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(deck[0] - 1.2, deck[1], DROP / 2))
        box = bpy.context.active_object
        box.scale = (2.4 + 0.0, deck[2], DROP)
    for clip_name, t, Mw in men:
        C.man_at(m, clip_name, t, Mw)
    m["arm"].location = (0.0, 0.0, -50.0)
    kit.studio(sun_elev=42, sun_turn=-35)
    kit.backdrop("A49C84")
    cam = kit.camera(lens=lens)
    kit.look_at(cam, eye, target)
    bpy.context.scene.camera = cam
    if fast:
        S.shared_render(640, 360, 16)
    else:
        S.shared_render(1280, 720, 48)
    path = os.path.join(WORK, "out", "pictures", name + ".png")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    kit.still(path)
    return path


def sheet(paths, labels, out, cols=3):
    """The renders tiled, each with its label."""
    from PIL import Image, ImageDraw, ImageFont
    ims = [Image.open(p).convert("RGB") for p in paths]
    w, h = ims[0].size
    rows = (len(ims) + cols - 1) // cols
    S_ = Image.new("RGB", (w * cols, h * rows), (235, 230, 220))
    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", max(16, h // 26))
    except Exception:
        font = ImageFont.load_default()
    for k, (im, lab) in enumerate(zip(ims, labels)):
        x, y = (k % cols) * w, (k // cols) * h
        S_.paste(im, (x, y))
        d = ImageDraw.Draw(S_)
        d.rectangle((x, y, x + w, y + h // 14), fill=(235, 230, 220))
        d.text((x + 10, y + 4), lab, fill=(20, 20, 20), font=font)
    S_.save(out, quality=90)
    return out


def pictures(fast=False, want=None):
    out = []
    pdir = os.path.join(WORK, "out", "pictures")
    if not want or "bail_climb" in want:
        socks = C.sockets_of("vehicle", "Sherman")
        Mw = C.place_matrix("vehicle", "Sherman", "commander", HATCH_OFF, socks)
        paths, labels = [], []
        for t in (0.0, 0.44, 0.64, 0.84, 1.10, 1.5):
            paths.append(picture(f"bail_climb_{t:.2f}", [("bail_climb_out", t, Mw)], eye=(2.6, -4.6, 3.9),
                                 target=(-0.2, -0.75, 2.9), tanks=[("Sherman", (0.0, 0.0), 0.0)], fast=fast, lens=35))
            labels.append(f"bail_climb_out {t:.2f} s")
        out.append(sheet(paths, labels, os.path.join(pdir, "bail_climb.jpg")))
    if not want or "bail_jump" in want:
        G = DROP
        ts = (0.0, 0.30, 0.62, 0.95, MEAS.get("jump", {}).get("touchdown", 1.0) + 0.12, 1.6)
        men = [("bail_jump_down", t, M4(np.eye(3), (0.0, 2.2 * k, G))) for k, t in enumerate(ts)]
        out.append(picture("bail_jump", men, eye=(5.5, -6.5, 2.4), target=(0.8, 5.0, 0.9), deck=(0.22, 5.5, 13.0),
                           fast=fast, lens=26))
    if not want or "burning" in want:
        men = [("burning_run", t, M4(np.eye(3), (0.0, 1.6 * k, 0.0))) for k, t in enumerate((0.1, 0.45, 0.9, 1.3, 1.6, 1.9))]
        out.append(picture("burning_run", men, eye=(4.6, -2.2, 1.6), target=(0.0, 4.0, 0.9), fast=fast, lens=30))
        men = [("burning_fall_roll", t, M4(np.eye(3), (-0.6, 2.6 * k, 0.0)))
               for k, t in enumerate((0.5, 0.75, 1.2, 1.6, 2.6, 3.8))]
        out.append(picture("burning_fall_roll", men, eye=(5.4, -3.6, 2.6), target=(0.4, 5.6, 0.3), fast=fast, lens=26))
    return out


def main(argv):
    only = set(argv[argv.index("--only") + 1].split(",")) if "--only" in argv else {"clips", "check", "ref"}
    t0 = time.time()
    import kit
    if "pictures" in only:
        want = [a for a in argv if a in ("bail_climb", "bail_jump", "burning")]
        for p in pictures(fast="--fast" in argv, want=want):
            print("picture", p, f"{time.time() - t0:.0f} s")
        return
    kit.fresh(C.FPS)
    C.setup("GER", "rifle")
    names = make_clips()
    print(f"setup and clips {time.time() - t0:.0f} s: {names}")
    os.makedirs(WORK, exist_ok=True)
    report = {"made": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "jump": MEAS.get("jump")}
    if "check" in only:
        report["hatch"] = check_hatch()
        report["ground"] = {n: ground_check(n) for n in names}
        report["fastest_turn"] = {n: joint_speeds(n) for n in names}
        for n in names:
            print(f"  {n:18s} ground {report['ground'].get(n)}  fastest turn {report['fastest_turn'][n]}")
        print(f"checked {time.time() - t0:.0f} s")
        with open(os.path.join(WORK, "tank_bail_checks.json"), "w") as f:
            json.dump(report, f, indent=1, default=str)
    if "ref" in only:
        out = LR.reference(names)
        for n in names:
            rc = out["clips"][n]
            if n in ROOT:
                rc["root"] = [[round(float(x), 5) for x in C.g2b(ROOT[n](t))] for t in rc["times"]]
                rc["root_motion"] = True
            if n == "burning_run":
                rc["stride"] = round(3 * S.RUN.stride, 3)
        out["by"] = "tank_bail.py (crew.py's pattern man, GER)"
        with open(LR.REF + ".part", "w") as f:
            json.dump(out, f, separators=(",", ":"))
        os.replace(LR.REF + ".part", LR.REF)
        print(f"reference {time.time() - t0:.0f} s")


if __name__ == "__main__":
    main(sys.argv)
