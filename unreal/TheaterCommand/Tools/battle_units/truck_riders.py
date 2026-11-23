"""Riders in the back of the lorry and the half-tracks (release 2, Brian 23 November 2026 14:10Z: "i want soldiers in
the back of a truck and they then hop out and start engaging enemies"): the bench seat, its death, getting up from the
bench and sitting down on it, and the ways out over the Truck's tailboard and through the half-tracks' stern doors,
made the way light_riders.py makes the light vehicles' riders (crew.py's pattern man, its solver), checked against the
vehicles' own meshes, written to truck_riders_ref.json for light_riders_fit.py to fit to every infantry file.

    python truck_riders.py --only clips,check,ref      make, check at every bench seat, write the reference
    python truck_riders.py --only pictures [--fast]   the pictures

The parts, each found by its places (vehicle + socket), as the light riders' are:
    bench_ride        a man on a bench facing inboard, his rifle upright between his knees (Truck, Hanomag,
                      M3Halftrack rider.1 .. rider.10)
    die_bench_slump   his death there: he folds forward over his knees inside his own place on the bench
    bench_rise_l/_r   up from the left (rider.1-5) or right (rider.6-10) bench, turning as he stands to face aft,
                      toward the way out, upright in the aisle with his rifle at the port
    bench_sit_l/_r    down onto it: from standing in the aisle facing forward, his back turned to the bench, sitting
The game walks him along the floor between (ride_clips.aisle: each vehicle's floor height and its way out).
"""
import json
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
os.environ.setdefault("CREW_WORK", "/home/user/art/trucks/crew/")
os.environ.setdefault("RIDERS_WORK", "/home/user/art/trucks/")

import light_riders as LR  # noqa: E402  (crew.py and soldier.py with it, bpy)
import mocap  # noqa: E402
import numpy as np  # noqa: E402

C, S = LR.C, LR.S
from soldier import add3, ramp, bump  # noqa: E402

WORK = os.environ["RIDERS_WORK"]
LR.REF = os.path.join(WORK, "truck_riders_ref.json")

# The Truck carries men with her tilt stowed (the ground's rule: the swap `tilt` off, an open body), so she is checked
# so: her `tilt` part scaled to nothing as the game does it. (TRUCK_TILT=on checks under it.)
TILT = os.environ.get("TRUCK_TILT", "off")
_env_tree = C.env_tree


def _env_tree_tilt(objs, drop=None):
    if TILT == "off":
        for o in objs:
            if o.type == "ARMATURE" and "tilt" in o.pose.bones:
                o.pose.bones["tilt"].scale = (0.0, 0.0, 0.0)
                print("  (the Truck's tilt stowed for the check)")
        LR.bpy.context.view_layer.update()
    return _env_tree(objs, drop)


C.env_tree = _env_tree_tilt

# The ways out are checked with the vehicle opened as her `dismount` clip leaves her (its last keys).
OPEN = {"Truck": {"tailboard": 176.0}, "Hanomag": {"door_l": 105.0, "door_r": 105.0}, "M3Halftrack": {"door_rear": 115.0}}
OPENED = {"on": False, "v": None}
_load_piece = C.load_piece


PICT = {"on": False}


def _load_piece_noted(kind, name, *a, **k):
    OPENED["v"] = name
    objs = _load_piece(kind, name, *a, **k)
    if PICT["on"] and kind == "vehicle":            # (the pictures: her tilt stowed, her board or doors open)
        for o in objs:
            if o.type == "ARMATURE" and "tilt" in o.pose.bones:
                o.pose.bones["tilt"].scale = (0.0, 0.0, 0.0)
        open_up(objs, name)
    return objs


C.load_piece = _load_piece_noted


def open_up(objs, v):
    """Her parts turned as the game turns them (each part's own pivot and axis, the game's axes: mirrored into
    Blender's, a turn about an axis goes the other way about the mirrored axis)."""
    from mathutils import Matrix as Mx, Vector as Vx
    parts = {p["name"]: p for p in C.piece_json("vehicle", v)["parts"]}
    for o in objs:
        if o.type != "ARMATURE":
            continue
        Mw = o.matrix_world.copy()
        for name, deg in OPEN.get(v, {}).items():
            if name not in o.pose.bones:
                print(f"  ({v}: no bone {name} to open)")
                continue
            p = parts[name]
            piv = Vx(tuple(C.g2b(p["pivot"])))
            R = Mx.Translation(piv) @ Mx.Rotation(math.radians(-deg), 4, Vx(tuple(C.g2b(p["axis"])))) @ \
                Mx.Translation(-piv)
            pb = o.pose.bones[name]
            LR.bpy.context.view_layer.update()
            pb.matrix = Mw.inverted() @ R @ Mw @ pb.matrix
            print(f"  ({v}: {name} opened {deg:.0f})")
    LR.bpy.context.view_layer.update()


def _env_tree_open(objs, drop=None):
    if OPENED["on"]:
        open_up(objs, OPENED["v"])
    tree, bm = _env_tree_tilt(objs, drop)
    if OPENED["on"] and OPENED["v"] == "Truck":
        from mathutils import Vector as Vx
        hit = tree.ray_cast(Vx((-4.0, 0.0, 0.8)), Vx((1.0, 0.0, 0.0)))
        print(f"  (the Truck's board, hanging: a ray at 0.8 m meets her at x {hit[0].x if hit[0] else None})")
    return tree, bm


C.env_tree = _env_tree_open

CARRIERS = ("Truck", "Hanomag", "M3Halftrack")
NAMES = []
BENCH = [f"rider.{i}" for i in range(1, 11)]
FLOOR = {"Truck": 1.10, "Hanomag": 0.59, "M3Halftrack": 0.94}       # (measured: the floor under the aisle)
SEAT_H = 0.44                                                        # (the benches' tops over their floors: the
                                                                     #  M3's and the Hanomag's 0.44, the Truck's 0.455)
TRUCK_SINK = -0.0075     # (the Truck's bench is 1.5 cm higher over its floor: he sits half of it lower, his boots
                         #  half of it over the floor)
RISE = 1.4               # (s: bench_rise and bench_sit)
STAND_AT = 0.38          # (m: where he stands after rising, ahead of the bench's seat point, toward the aisle:
                         #  the benches' front edges are 0.17-0.20 ahead of it, his near boot clear of them)


SEAT_FWD = {"Truck": 0.0, "Hanomag": 0.015, "M3Halftrack": 0.025}   # (m: the half-tracks' padded backs stand
                                                                    #  0.10-0.17 behind the seat's point, so he sits
                                                                    #  that much further forward on the bench)


def bench_place(v, s):
    return (SEAT_FWD[v], 0.0, TRUCK_SINK if v == "Truck" else 0.0)


PLACES = [("vehicle", v, s, bench_place(v, s)) for v in CARRIERS for s in BENCH]
FEET = 0.36          # (his boots ahead of the seat point: the aisle is 1.36 m across in the Hanomag, so the two
                     #  benches' boots stay clear of each other)


# ================================================================================================ the bench
def bench_body(t, seed=0.5):
    roll, pitch, br = LR.sway(t, LR.LOOP, seed, k=1.2)
    P = C.sit_body(SEAT_H, slouch=0.8, hip_x=-0.07)
    LR.ride_in(P, roll, pitch, br)
    ank = C.MAN["ankle"]
    P["foot_l"] = {"at": (FEET, 0.09, -SEAT_H + ank), "yaw": 4.0, "pole": (1.0, 0.06, 0.55)}
    P["foot_r"] = {"at": (FEET + 0.02, -0.09, -SEAT_H + ank), "yaw": -4.0, "pole": (1.0, -0.06, 0.55)}
    return P, roll, pitch


def c_bench_ride(t):
    P, roll, pitch = bench_body(t)
    LR.look_about(P, t, a=(0.8, 1.3, 2.0), b=(2.6, 3.1, 3.7), yaw=(-24.0, 20.0))
    LR.rifle_hold(P, SEAT_H, FEET - 0.06, roll, pitch)
    return LR.seat("bench_ride", P)


def make_bench():
    LR.clip("bench_ride", LR.LOOP, True, "seat", "rifle", PLACES,
            "A man on a lorry's or a half-track's bench, facing inboard: sat back against the side, his rifle stood on "
            "its butt between his boots and both hands on it, knees together, the body taking the ride, looking about. "
            "The bench's top is 0.44 over the floor (the Truck's 0.455: he sits 0.75 cm lower there, its root_offset).",
            death="die_bench_slump")(c_bench_ride)
    LR.HOLD["bench_ride"] = lambda t: (1.0, 1.0)


# ---- his death on the bench: folded forward over his knees, inside his own place
def build_bench_death():
    P0 = C.CREW["bench_ride"]["fn"](0.0)
    butt = (FEET - 0.06, 0.0, -SEAT_H)

    def fn(t):
        fall = ramp(t, 0.15, 1.1)
        W = C.rifle_upright(butt, lean_back=10.0 - 30.0 * fall, side=0.0)
        F1 = LR.limp(LR.slumped(P0, fwd=22.0, side=0.0, head=(30.0, 0.0, 6.0)),
                     LR.on_thigh("l", 0.30, 0.12, 0.18), LR.on_thigh("r", 0.30, 0.12, 0.18))
        F2 = LR.slumped(P0, fwd=44.0, side=2.0, head=(52.0, 4.0, 8.0), sink=0.012)
        F2 = LR.limp(F2, LR.hang("l", (0.30, 0.10, -0.16)), LR.hang("r", (0.32, -0.10, -0.18)))
        P = dict(S.keyed([(0.0, P0), (0.1, dict(P0, chest=add3(P0["chest"], (-6.0, 0.0, 0.0)),
                                                  head=add3(P0["head"], (-10.0, 0.0, 0.0)))),
                          (0.6, F1), (LR.DIE_REST, F2)])(t))
        P["weapon"] = W
        hold = 1.0 - ramp(t, 0.1, 0.6)
        e = LR.KNOBS["ride_elbow"]
        HL = C.on_rifle(W, "l", 0.6, (0.75, -0.65, -0.1), pole=(e[0], e[1], e[2]))
        HR = C.on_rifle(W, "r", 0.49, (0.75, 0.65, -0.1), pole=(e[0], -e[1], e[2]))
        if hold > 1e-3:
            P["hand_l"] = {"blend": [(hold, HL), (1.0 - hold, P["hand_l"])]} if hold < 1.0 - 1e-3 else HL
            P["hand_r"] = {"blend": [(hold, HR), (1.0 - hold, P["hand_r"])]} if hold < 1.0 - 1e-3 else HR
        return P
    LR.clip("die_bench_slump", LR.DIE, False, "seat", "rifle", PLACES,
            "A man killed on a bench: struck back, his hands let the rifle go and it tips forward against his knees; "
            "he folds forward over his lap, his arms hanging between his knees, his head down, inside his own place on "
            "the bench (never onto the men beside him); the last pose held. Played from bench_ride.")(
        LR.calm("die_bench_slump", fn, LR.seat))
    LR.HOLD["die_bench_slump"] = lambda t: (1.0 - ramp(t, 0.1, 0.6),) * 2


# ---- up from the bench, and down onto it
# He stands up turning toward the way out (every carrier's is at her stern): the left bench (rider.1-5, on her left,
# facing her right) turns to his right, the right bench (rider.6-10) to his left; he ends upright in the aisle
# STAND_AT ahead of the seat's point, facing aft, his rifle at the port, ready for the game to walk him out. Sitting
# down is the same played backwards from the other side's turn: he comes up the aisle facing forward, turns his back to
# the bench and sits (so the left bench's bench_sit_l is the right bench's rise backwards).
LEFT = BENCH[:5]
RIGHT = BENCH[5:]


def places_of(seats):
    return [("vehicle", v, s, bench_place(v, s)) for v in CARRIERS for s in seats]


def stand_here(x, yaw, crouch=0.03, lean=6.0, apart=0.10, y=0.0):
    """Standing on the floor (SEAT_H under the seat's point) at x ahead of it and y to its left, turned yaw degrees
    (+ to his left), a little crouched for the lorry's sway, his rifle at the port."""
    P = S.base_stand()
    P.update(S.stand_feet(ahead=0.04, apart=apart, back=-0.05))
    pa = P["pelvis_at"]
    P["pelvis_at"] = (pa[0] - 0.01, pa[1], pa[2] - crouch)
    P["chest"] = add3(P["chest"], (lean, 0.0, 0.0))
    P["head"] = add3(P["head"], (-0.5 * lean, 0.0, 0.0))
    P = C.yawed(P, yaw, (x, y, -SEAT_H))
    P["_seat"] = 1.0
    return P


def rise_keys(yaw, y_end=0.0, var="", bench_aft=None):
    P0 = C.CREW["bench_ride"]["fn"](0.0)
    ank = C.MAN["ankle"]
    sg = 1.0 if yaw > 0 else -1.0             # (+: turning to his left)
    # 1: leaning forward over his boots, drawn back under the bench's front edge, the rifle lifted off the floor
    A = dict(P0)
    early = {"": 0.2, "_fore": -0.1, "_aft": 0.2}[var]          # (how far round he has turned when half up)
    mid = {"": 0.7, "_fore": 0.45, "_aft": 0.8}[var]           # (and when up)
    reach = 0.12 if var == "_aft" else 0.20                    # (how far he comes forward off the bench as he rises)
    A["pelvis"] = add3(P0["pelvis"], (16.0 if var == "_aft" else 24.0, 0.0, 0.0))
    A["spine_01"] = add3(P0["spine_01"], (8.0, 0.0, 0.0))
    A["spine_02"] = add3(P0["spine_02"], (6.0, 0.0, 0.0))
    A["foot_l"] = {"at": (FEET - 0.08, 0.11, -SEAT_H + ank), "yaw": 6.0, "pole": (1.0, 0.1, 0.55)}
    A["foot_r"] = {"at": (FEET - 0.06, -0.11, -SEAT_H + ank), "yaw": -6.0, "pole": (1.0, -0.1, 0.55)}
    # 2: off the bench, half up over his boots, beginning to turn
    B = dict(A)
    pa = P0["pelvis_at"]
    B["pelvis_at"] = (pa[0] + reach, pa[1] + (0.3 * y_end if var == "_aft" else 0.0), pa[2] + 0.17)
    turn_b = early * yaw if var != "_fore" else -0.1 * bench_aft       # (the rear pair half up: turned a little forward,
                                                                     #  away from the stern, whichever way they go)
    B["pelvis"] = add3(P0["pelvis"], (22.0 if var == "_aft" else 30.0, 0.0, turn_b))
    B["chest"] = add3(P0["chest"], (-4.0, 0.0, 0.5 * turn_b))
    E = stand_here(STAND_AT, yaw, y=y_end)
    # 3: up, most of the way round, the far boot (the one on the side he turns to) stepped round to its place
    D = stand_here(STAND_AT - 0.03, mid * yaw, crouch=0.06, lean=10.0, y=(1.0 if var == "_aft" else 0.7) * y_end)
    far, near = ("l", "r") if sg > 0 else ("r", "l")
    D["foot_" + far] = E["foot_" + far]
    D["foot_" + near] = dict(A["foot_" + near], yaw=A["foot_" + near]["yaw"] + 0.4 * yaw)
    return [(0.0, P0), (0.3, A), (0.65, B), (1.0, D), (RISE, E)], far, near


def rel_hand(W, s, x_w, a_hint, pole, z_w=0.012):
    """on_rifle's hand, kept on the rifle wherever it goes: its place and turn in the rifle's own frame, its elbow's
    pole in the world's (he leans forward over a rifle that stands still: a pole that leaned with his chest swung the
    elbows round). It may roll only a little round the barrel (6 degrees): let roll 20, the easiest roll ran from one
    end to the other in a frame as he leaned forward."""
    h = C.on_rifle(W, s, x_w, a_hint, pole=pole, z_w=z_w)
    R = W[:3, :3]
    return {"grip": "grip_r", "slide": (x_w, 0.0, z_w), "a": tuple(R.T @ np.asarray(h["a"], float)),
            "n": tuple(R.T @ np.asarray(h["n"], float)), "pole_world": tuple(pole), "roll": 6.0}


def lifted(f, h):
    f = dict(f)
    a = f["at"]
    f["at"] = (float(a[0]), float(a[1]), float(a[2]) + h)
    return f


def rise_fn(yaw, y_end=0.0, var="", bench_aft=None):
    keys, far, near = rise_keys(yaw, y_end, var, bench_aft)
    at = S.keyed(keys)
    P0 = keys[0][1]
    W0 = P0["weapon"]
    E = keys[-1][1]
    e = LR.KNOBS["ride_elbow"]
    # (his elbows kept in, down along his sides, as he lifts the rifle: the half-tracks' rolled canvas is just
    #  behind the rear pair's shoulders)
    up_l = dict(rel_hand(W0, "l", 0.6, (0.75, -0.65, -0.1), (e[0], -0.1, e[2])), pole_weight=POLE_HELD)
    up_r = dict(rel_hand(W0, "r", 0.49, (0.75, 0.65, -0.1), (e[0], 0.1, e[2])), pole_weight=POLE_HELD)
    port_l, port_r = E["hand_l"], E["hand_r"]
    # (while the hands change from the barrel to the port's holds each elbow is held near its pole (said in his
    #  chest's frame, so it turns with him), and let go to its own easiest place only once he is at the port: left to
    #  find its easiest place all the way, the elbow chose between two near as easy and swung from one to the other)
    port_lh, port_rh = dict(port_l, pole_weight=POLE_HELD), dict(port_r, pole_weight=POLE_HELD)
    # (the rifle's way up. He lifts it as he stands: from T_LIFT it rides with his chest, moved as the chest moves and
    #  turned about the upright as he turns, so it stays stood upright before him and comes up off the floor with his
    #  hands (they keep their reach: mixed straight from the floor to the port, the rifle ran his top hand up to his
    #  shoulder and the elbow, folded tight, swung over in a frame). Then, over T_PORT, he turns it over and brings it
    #  to the port as he regrips (it stood with its sights toward him; at the port they face out, so it turns near half
    #  round its barrel): its turn is carried the one way round between two fixed turns said in his heading's frame,
    #  measured once (mixed between two places that moved with him, the shorter way round between them, near half a
    #  turn, changed sides from one frame to the next). Near the end it settles into the port's own place)
    rig = C.MODEL["rig"]
    held_at = {}

    def heading(M):
        f = M[:3, 0]
        return math.atan2(float(f[1]), float(f[0]))

    def carried(rig_, st):
        M = rig_.frame(st, "chest")
        dz = C.Rz(math.degrees(heading(M) - held_at["h"]))
        return S.M4(dz @ W0[:3, :3], M[:3, 3] + dz @ (W0[:3, 3] - held_at["p"]))

    def turning(v):
        def place(rig_, st):
            M = rig_.frame(st, "chest")
            qa, qb = held_at["q"]
            om = math.acos(min(1.0, float(qa @ qb)))
            q = qa if om < 1e-6 else (math.sin((1 - v) * om) * qa + math.sin(v * om) * qb) / math.sin(om)
            R = C.Rz(math.degrees(heading(M))) @ mocap.quat_to_mat(q / np.linalg.norm(q))
            Wc, Wp = carried(rig_, st), E["weapon"](rig_, st)
            return S.M4(R, (1.0 - v) * Wc[:3, 3] + v * Wp[:3, 3])
        return place

    def rifle(t):
        if t <= T_LIFT or not held_at:
            return W0
        if t >= RISE - 1e-6:
            return E["weapon"]
        v = ramp(t, T_PORT[0], T_PORT[1])
        if "q" not in held_at:                 # (only while measuring)
            return carried if v <= 0.0 else E["weapon"]
        if v <= 0.0:
            return carried
        m = ramp(t, T_PORT[1] - 0.1, RISE)
        return turning(v) if m <= 0.0 else S.blend_places([(1.0 - m, turning(v)), (m, E["weapon"])])

    def rise(t):
        P = dict(at(t))
        # the boots: the far one lifted as it steps round (0.65 to 1.0), the near one shuffled round after (1.0 to RISE)
        P["foot_" + far] = lifted(P["foot_" + far], 0.06 * bump(t, 0.62, 0.82, 1.0))
        P["foot_" + near] = lifted(P["foot_" + near], 0.04 * bump(t, 1.0, 1.15, RISE - 0.05))
        # the rifle: upright between his knees while he sits, carried up with him, turned over to the port as he
        # stands; the hands on the barrel (their places in the rifle's own frame), then regripping to the port's holds
        P["weapon"] = rifle(t)
        k = ramp(t, T_PORT[0], T_PORT[1])
        free = ramp(t, T_PORT[1], RISE)
        if k <= 0.0 or "regrip" not in held_at:
            P["hand_l"], P["hand_r"] = (up_l, up_r) if k < 0.5 else (port_lh, port_rh)
        elif k < 1.0:
            P["hand_l"], P["hand_r"] = (regrip(held_at["regrip"][s_], k) for s_ in "lr")
        elif free < 1.0:
            P["hand_l"] = {"blend": [(1.0 - free, port_lh), (free, port_l)]}
            P["hand_r"] = {"blend": [(1.0 - free, port_rh), (free, port_r)]}
        else:
            P["hand_l"], P["hand_r"] = port_l, port_r
        P["_seat"] = 1.0
        return P
    M = rig.frame(rig.solve(rise(T_LIFT), lift_knees=0), "chest")
    held_at.update(h=heading(M), p=M[:3, 3].copy())
    stE = rig.solve(rise(RISE), lift_knees=0)
    W_end, h_end = rig.frame(stE, "weapon"), heading(rig.frame(stE, "chest"))
    qa = mocap.mat_to_quat(C.Rz(-math.degrees(held_at["h"])) @ W0[:3, :3])
    qb = mocap.mat_to_quat(C.Rz(-math.degrees(h_end)) @ W_end[:3, :3])
    held_at["q"] = (qa, qb if float(qa @ qb) >= 0.0 else -qb)
    # (the hands' change of grip, from the barrel to the port's holds, made in his chest's frame: each hand's grip
    #  point, its turn and its elbow's pole at the start of the change and at the port, said in the chest's frame
    #  then, and carried between the two the shortest way round there. On the rifle the two grips are a third of a
    #  turn apart round its barrel; mixed as two grips on the turning rifle the hand's turn ran the wrist to its end
    #  and the elbow over. So he regrips: the hands move to the port's holds with the chest as the rifle turns in them)
    ends = {}
    for t_, specs in ((T_PORT[0], (up_l, up_r)), (RISE, (port_l, port_r))):
        P_ = rise(t_)
        st_ = rig.solve(P_, lift_knees=0)
        M = rig.frame(st_, "chest")
        R_, o_ = M[:3, :3], M[:3, 3]
        for s_, h_ in zip("lr", specs):
            g_, a_, n_, pole_ = rig.resolve_hand(st_, P_, h_, 1.0 if s_ == "l" else -1.0)
            ends.setdefault(s_, []).append((R_.T @ (g_ - o_), R_.T @ S.frame2(a_, n_), R_.T @ pole_))
    held_at["regrip"] = ends
    return rise


def regrip(ends, k):
    """A hand k of the way through its change of grip (rise_fn): a place of its own in the chest's frame."""
    (g0, F0, p0), (g1, F1, p1) = ends
    q0, q1 = mocap.mat_to_quat(F0), mocap.mat_to_quat(F1)
    if float(q0 @ q1) < 0.0:
        q1 = -q1
    om = math.acos(min(1.0, float(q0 @ q1)))
    q = q0 if om < 1e-6 else (math.sin((1 - k) * om) * q0 + math.sin(k * om) * q1) / math.sin(om)
    F = mocap.quat_to_mat(q / np.linalg.norm(q))
    return {"frame": "chest", "at": tuple((1 - k) * g0 + k * g1), "a": tuple(F[:, 0]), "n": tuple(F[:, 1]),
            "pole": tuple((1 - k) * p0 + k * p1), "pole_weight": POLE_HELD, "roll": 4.0}


# Where he stands up, along the aisle from his seat (+ aft): in the M3 the front pair would stand into the machine gun
# over the aisle (its pedestal 0.55 m behind the cab, the gun reaching 0.95 m back at their waists), so they step
# 0.62 aft of it as they rise; the rear pairs (the Hanomag's 0.20 ahead of her stern wall, both half-tracks' under the
# canvas rolled across the top of the stern) stand 0.15 forward and turn late.
ALONG = {"": 0.0, "_fore": -0.15, "_aft": 0.62}
T_LIFT, T_PORT = 0.45, (0.5, 1.25)     # (s: the rifle lifted with him from T_LIFT, turned over and brought to the
                                        #  port as he regrips over T_PORT)
POLE_HELD = 0.15                        # (the elbow's hold on its pole while the hands change grips: arm_comfort's
                                        #  cost of a degree squared off it; its own is 0.012)
SPECIAL = {("M3Halftrack", "rider.1"): "_aft", ("M3Halftrack", "rider.6"): "_aft",
           ("Hanomag", "rider.5"): "_fore", ("Hanomag", "rider.10"): "_fore",
           ("M3Halftrack", "rider.5"): "_fore", ("M3Halftrack", "rider.10"): "_fore"}


def places_where(seats, var):
    return [("vehicle", v, s, bench_place(v, s)) for v in CARRIERS for s in seats if SPECIAL.get((v, s), "") == var]


def make_rise():
    aft = {"l": -90.0, "r": 90.0}          # (the turn to face aft: the left bench to his right, the right to his left)
    aft_y = {"l": -1.0, "r": 1.0}          # (aft in his seat's frame: to his right on the left bench, his left on the right)
    for side, seats in (("l", LEFT), ("r", RIGHT)):
        other = "r" if side == "l" else "l"
        for var, along in ALONG.items():
            pl = places_where(seats, var)
            if not pl:
                continue
            y_end = along * aft_y[side]
            up = rise_fn(aft[side], y_end, var, aft[side])
            down_from = rise_fn(aft[other], y_end, var, aft[side])

            def sit(t, f=down_from):
                return f(RISE - t)
            name_r, name_s = "bench_rise_" + side + var, "bench_sit_" + side + var
            bench = "left (rider.1-5)" if side == "l" else "right (rider.6-10)"
            turn = "right" if side == "l" else "left"
            where = "" if not along else (f" and {abs(along):.2f} {'aft' if along > 0 else 'forward'} of it along the "
                                         "aisle")
            why = {"": "", "_fore": " (The half-tracks' rear pair: the stern wall and the rolled canvas over it are just behind their seats.)",
                   "_aft": " (The M3's front pair: her machine gun stands over the aisle beside them at their waists.)"}[var]
            LR.clip(name_r, RISE, False, "seat", "rifle", pl,
                    f"Up from the {bench} bench: he leans forward over his boots, stands, turning to his {turn} as he "
                    f"steps into the aisle, and ends upright on the floor {STAND_AT:.2f} ahead of the seat's point (toward "
                    f"the aisle){where}, facing aft toward the way out, a little crouched, his rifle brought from between "
                    "his knees up to the port. The game walks him from there to the way out (ride_clips.aisle)." + why)(up)
            LR.clip(name_s, RISE, False, "seat", "rifle", pl,
                    f"Down onto the {bench} bench: he comes to stand in the aisle {STAND_AT:.2f} ahead of the seat's "
                    f"point{where}, facing forward (up the aisle from the way in), turns his back to the bench and sits, "
                    "his rifle going down between his knees; then bench_ride. (The other bench's rise, backwards.)"
                    + why)(sit)
            LR.HOLD[name_r] = lambda t: (1.0, 1.0)
            LR.HOLD[name_s] = lambda t: (1.0, 1.0)
            NAMES.extend([name_r, name_s])


# ================================================================================================ the ways out
# Each way out is a clip anchored at the carrier's `step` socket, moved by its root_offset to where he starts: standing
# on her floor near the way out, facing out (the game walks him there from bench_rise along ride_clips.aisle). The clip
# carries root motion: its root track goes with him from there to the ground (the root under his feet, on the floor,
# then the ground), so the game hands him to its own walking where the last root key leaves him. In the clip's frame
# x is out of the vehicle, z up, its floor at 0 (Blender's axes, y to his left).
EXIT_AT = {"Truck": (-2.95, 0.0, 1.10), "Hanomag": (-1.55, 0.0, 0.59), "M3Halftrack": (-2.00, 0.0, 0.94)}
LEAD = {"Truck": 0.0, "Hanomag": 0.35, "M3Halftrack": 0.40}   # (m: the half-tracks' starts stand this much further in
                                                              #  than the clips' first drafts, so he ducks before his
                                                              #  head reaches the rolled canvas: their later keys move
                                                              #  out by it)
STEP_AT = {"Truck": (-3.50, 0.0, 0.70), "Hanomag": (-2.97, 0.0, 0.50), "M3Halftrack": (-3.33, 0.0, 0.50)}


def exit_offset(v):
    """The start's place in the step socket's frame (forward, right, up of its facing, which is aft, -x)."""
    d = np.asarray(EXIT_AT[v], float) - np.asarray(STEP_AT[v], float)
    return (round(-float(d[0]), 4), round(-float(d[1]), 4), round(float(d[2]), 4))


def exit_body(x, z, drop=0.0, lean=0.0, head=0.0, side=0.0):
    """Upright as base_stand, his pelvis moved x out and z up (the floor or ground he stands on) and down by `drop`
    (knees bent), bent forward `lean` degrees through hips and spine (his head kept up by `head` less of it), his
    rifle at the port."""
    P = S.base_stand()
    pa = P["pelvis_at"]
    P["pelvis_at"] = (pa[0] + x, pa[1] + side, pa[2] + z - drop)
    P["pelvis"] = add3(P["pelvis"], (0.5 * lean, 0.0, 0.0))
    P["spine_01"] = add3(P["spine_01"], (0.2 * lean, 0.0, 0.0))
    P["spine_02"] = add3(P["spine_02"], (0.15 * lean, 0.0, 0.0))
    P["chest"] = add3(P["chest"], (0.15 * lean, 0.0, 0.0))
    P["head"] = add3(P["head"], (-0.5 * lean + head, 0.0, 0.0))
    P["_seat"] = 1.0
    return P


def foot(x, y, z, yaw=0.0, side="l"):
    sg = 1.0 if side == "l" else -1.0
    return {"at": (x, y, z + C.MAN["ankle"]), "yaw": yaw, "pole": (1.0, 0.08 * sg, 0.35)}


def smooth(u):
    u = min(max(u, 0.0), 1.0)
    return u * u * (3.0 - 2.0 * u)


class Steps:
    """A boot's path: planted [(t0, t1, (x, y, z), yaw)], swung between plants in an arc (`lift` over the higher of the
    two ends); a swing marked `fly` goes out fast and falls late (off a ledge: clear of the edge it leaves)."""

    def __init__(self, side, plants, lift=0.07, fly=()):
        self.side, self.plants, self.lift, self.fly = side, plants, lift, set(fly)

    def at(self, t):
        pl = self.plants
        if t <= pl[0][1]:
            return foot(*pl[0][2], pl[0][3], self.side)
        for i in range(len(pl) - 1):
            a, b = pl[i], pl[i + 1]
            if t <= a[1]:
                return foot(*a[2], a[3], self.side)
            if t < b[0]:
                u = (t - a[1]) / (b[0] - a[1])
                p, q = np.asarray(a[2], float), np.asarray(b[2], float)
                if i in self.fly:
                    ux = 1.0 - (1.0 - u) ** 2.5
                    x, y = p[:2] + (q[:2] - p[:2]) * ux
                    z = p[2] + (q[2] - p[2]) * u * u + 0.06 * math.sin(math.pi * u)
                else:
                    s_ = smooth(u)
                    x, y = p[:2] + (q[:2] - p[:2]) * s_
                    z = p[2] + (q[2] - p[2]) * s_ + self.lift * math.sin(math.pi * u)
                return foot(float(x), float(y), float(z), a[3] + (b[3] - a[3]) * smooth(u), self.side)
        return foot(*pl[-1][2], pl[-1][3], self.side)


ROOT = {}          # exit clip -> t -> (x, y, z): the root under his feet in the clip's frame (Blender's axes)
EXITS = []


def exit_clip(name, v, long, body_keys, steps, root_keys, about):
    """body_keys [(t, exit_body args)], steps {l, r: Steps}, root_keys [(t, (x, z))]. The vehicle's LEAD is added to
    every x from the third body key on (he has ducked by then), to the boots' plants from then and to the root."""
    dx, t_lead = LEAD[v], body_keys[2][0]
    body_keys = [(t, dict(kw, x=kw["x"] + (dx if t >= t_lead else 0.0))) for t, kw in body_keys]
    for st in steps.values():
        st.plants = [(a, b, (p[0] + (dx if a >= t_lead - 0.12 else 0.0), p[1], p[2]), y) for a, b, p, y in st.plants]
    root_keys = [(t, (x + (dx if t >= t_lead else 0.0), z)) for t, (x, z) in root_keys]
    keyed = S.keyed([(t, exit_body(**kw)) for t, kw in body_keys])
    rk = np.array([[t, x, z] for t, (x, z) in root_keys], float)

    def fn(t):
        P = dict(keyed(t))
        P["foot_l"], P["foot_r"] = steps["l"].at(t), steps["r"].at(t)
        P["_seat"] = 1.0
        return P

    def root(t):
        return (float(np.interp(t, rk[:, 0], rk[:, 1])), 0.0, float(np.interp(t, rk[:, 0], rk[:, 2])))
    LR.clip(name, long, False, "feet", "rifle", [("vehicle", v, "step", exit_offset(v))], about)(fn)
    LR.HOLD[name] = lambda t: (1.0, 1.0)
    ROOT[name] = root
    EXITS.append(name)
    NAMES.append(name)


def make_truck_exits():
    """Over the Truck's tailboard (dropped to hang down: her floor ends 0.25 out of the start, 1.10 over the ground;
    the hanging board's face 0.29 out, down to 0.52 under the floor)."""
    G = -1.10
    # a: two steps to the edge, crouch, jump out and down, land deep on both boots, come up and step on
    exit_clip("out_tail_a", "Truck", 2.4,
              [(0.0, dict(x=0.0, z=0.0, drop=0.04, lean=8.0)),
               (0.3, dict(x=0.08, z=0.0, drop=0.06, lean=12.0)),
               (0.55, dict(x=0.12, z=0.0, drop=0.24, lean=34.0, head=14.0)),
               (0.72, dict(x=0.22, z=0.0, drop=0.10, lean=26.0, head=12.0)),
               (0.84, dict(x=0.42, z=-0.10, drop=0.04, lean=14.0, head=10.0)),
               (1.04, dict(x=0.66, z=-0.62, drop=0.06, lean=10.0, head=8.0)),
               (1.20, dict(x=0.78, z=G, drop=0.30, lean=24.0, head=14.0)),
               (1.38, dict(x=0.84, z=G, drop=0.42, lean=34.0, head=18.0)),
               (1.75, dict(x=0.92, z=G, drop=0.16, lean=16.0, head=6.0)),
               (2.05, dict(x=1.08, z=G, drop=0.06, lean=10.0)),
               (2.4, dict(x=1.16, z=G, drop=0.04, lean=8.0))],
              {"r": Steps("r", [(0.0, 0.08, (-0.05, -0.10, 0.0), -8.0), (0.32, 0.78, (0.16, -0.10, 0.0), -4.0),
                                (1.18, 1.62, (0.74, -0.12, G), -6.0), (1.9, 2.4, (1.12, -0.11, G), -8.0)], fly=(1,)),
               "l": Steps("l", [(0.0, 0.30, (0.04, 0.10, 0.0), 6.0), (0.5, 0.76, (0.18, 0.10, 0.0), 4.0),
                                (1.20, 2.4, (0.84, 0.12, G), 6.0)], fly=(1,))},
              [(0.0, (0.0, 0.0)), (0.55, (0.12, 0.0)), (0.78, (0.22, 0.0)), (1.20, (0.80, G)), (2.4, (1.14, G))],
              "Out over the Truck's dropped tailboard: two steps to the edge of her floor, a crouch, a jump out and down "
              "the 1.10 m to the ground, landing deep on both boots, coming up and stepping on; his rifle held at the "
              "port all the way. Root motion: the root goes with him onto the ground.")
    # b: the same way out jumped from one boot, a longer stride and a lower landing (another man's way of it)
    exit_clip("out_tail_b", "Truck", 2.3,
              [(0.0, dict(x=0.0, z=0.0, drop=0.04, lean=8.0)),
               (0.28, dict(x=0.10, z=0.0, drop=0.08, lean=16.0, head=6.0)),
               (0.5, dict(x=0.16, z=0.0, drop=0.18, lean=28.0, head=12.0)),
               (0.66, dict(x=0.32, z=0.02, drop=0.06, lean=18.0, head=10.0)),
               (0.9, dict(x=0.62, z=-0.40, drop=0.04, lean=8.0, head=6.0)),
               (1.03, dict(x=0.78, z=G, drop=0.08, lean=12.0, head=8.0)),     # (touching down, legs near straight)
               (1.16, dict(x=0.85, z=G, drop=0.26, lean=24.0, head=14.0)),
               (1.36, dict(x=0.9, z=G, drop=0.40, lean=30.0, head=16.0)),
               (1.72, dict(x=1.02, z=G, drop=0.12, lean=14.0, head=4.0)),
               (2.3, dict(x=1.22, z=G, drop=0.04, lean=8.0))],
              {"l": Steps("l", [(0.0, 0.06, (0.04, 0.10, 0.0), 6.0), (0.3, 0.6, (0.18, 0.09, 0.0), 2.0),
                                (1.08, 1.5, (0.86, 0.13, G), 8.0), (1.85, 2.3, (1.24, 0.11, G), 6.0)], fly=(1,)),
               "r": Steps("r", [(0.0, 0.22, (-0.05, -0.10, 0.0), -8.0), (1.02, 2.3, (0.78, -0.12, G), -6.0)],
                          fly=(0,))},
              [(0.0, (0.0, 0.0)), (0.6, (0.18, 0.0)), (1.06, (0.84, G)), (2.3, (1.2, G))],
              "Out over the Truck's dropped tailboard, another man's way: a stride to the edge and off it from his left "
              "boot, the right swung out over the board, landing low and coming up. Root motion as out_tail_a.")


def make_hanomag_exits():
    """Through the Hanomag's open stern doors: her floor 0.59 over the ground; under the canvas rolled across the top
    of her stern (0.65 to 0.75 out of the start, 1.18 over her floor: he ducks under it before he moves), up onto
    the lower stern plate's top (0.79 out, 0.35 over her floor) and down the 0.94 to the ground. The keys' x below are
    from the first draft's start: exit_clip adds LEAD (0.35) from the duck on."""
    G, SILL, SX = -0.59, 0.35, 0.44
    exit_clip("out_doors_a", "Hanomag", 2.9,
              [(0.0, dict(x=0.0, z=0.0, drop=0.06, lean=12.0)),
               (0.3, dict(x=0.02, z=0.0, drop=0.26, lean=48.0, head=30.0)),
               (0.6, dict(x=0.12, z=0.0, drop=0.32, lean=56.0, head=36.0)),
               (0.95, dict(x=0.30, z=0.08, drop=0.36, lean=54.0, head=34.0)),
               (1.25, dict(x=0.46, z=SILL, drop=0.46, lean=44.0, head=26.0)),
               (1.5, dict(x=0.54, z=SILL, drop=0.36, lean=30.0, head=14.0)),
               (1.7, dict(x=0.76, z=0.1, drop=0.06, lean=14.0, head=8.0)),
               (1.88, dict(x=0.94, z=G, drop=0.30, lean=24.0, head=14.0)),
               (2.06, dict(x=1.0, z=G, drop=0.38, lean=30.0, head=16.0)),
               (2.45, dict(x=1.12, z=G, drop=0.12, lean=12.0, head=4.0)),
               (2.9, dict(x=1.26, z=G, drop=0.04, lean=8.0))],
              {"r": Steps("r", [(0.0, 0.3, (-0.05, -0.10, 0.0), -8.0), (0.5, 0.65, (0.16, -0.10, 0.0), -4.0),
                                (1.0, 1.56, (SX, -0.09, SILL), -2.0), (1.86, 2.3, (0.90, -0.12, G), -6.0)], lift=0.06,
                          fly=(2,)),
               "l": Steps("l", [(0.0, 0.62, (0.04, 0.10, 0.0), 6.0), (0.82, 0.92, (0.30, 0.10, 0.0), 4.0),
                                (1.2, 1.56, (SX, 0.09, SILL), 2.0), (1.90, 2.55, (1.0, 0.12, G), 6.0),
                                (2.75, 2.9, (1.28, 0.11, G), 6.0)], lift=0.06, fly=(2,))},
              [(0.0, (0.0, 0.0)), (0.95, (0.30, 0.0)), (1.2, (0.44, SILL)), (1.56, (0.52, SILL)), (1.88, (0.94, G)),
               (2.9, (1.26, G))],
              "Out through the Hanomag's open stern doors: ducking low under the canvas rolled across the top of her "
              "stern, up onto the lower stern plate in a crouch, and a jump down the 0.94 m to the ground, landing on "
              "both boots and coming up; his rifle at the port. Root motion: the root goes with him onto the plate "
              "and the ground.")
    exit_clip("out_doors_b", "Hanomag", 3.0,
              [(0.0, dict(x=0.0, z=0.0, drop=0.06, lean=12.0)),
               (0.3, dict(x=0.02, z=0.0, drop=0.28, lean=50.0, head=32.0)),
               (0.65, dict(x=0.14, z=0.0, drop=0.34, lean=56.0, head=36.0)),
               (1.0, dict(x=0.34, z=0.08, drop=0.36, lean=52.0, head=32.0)),
               (1.35, dict(x=0.48, z=SILL, drop=0.46, lean=36.0, head=20.0)),
               (1.68, dict(x=0.64, z=0.10, drop=0.20, lean=22.0, head=12.0)),
               (1.92, dict(x=0.86, z=G, drop=0.26, lean=20.0, head=10.0)),
               (2.4, dict(x=1.04, z=G, drop=0.12, lean=12.0, head=4.0)),
               (3.0, dict(x=1.22, z=G, drop=0.04, lean=8.0))],
              {"l": Steps("l", [(0.0, 0.3, (0.04, 0.10, 0.0), 6.0), (0.5, 0.65, (0.22, 0.10, 0.0), 4.0),
                                (1.05, 1.55, (SX, 0.09, SILL), 2.0), (1.96, 2.5, (0.98, 0.12, G), 6.0)], lift=0.06),
               "r": Steps("r", [(0.0, 0.5, (-0.05, -0.10, 0.0), -8.0), (0.75, 1.15, (0.30, -0.10, 0.0), -4.0),
                                (1.48, 1.48, (0.54, -0.11, SILL + 0.10), -5.0),        # (over the stern plate)
                                (1.76, 2.1, (0.82, -0.12, G), -6.0), (2.7, 3.0, (1.2, -0.11, G), -8.0)], lift=0.10,
                          fly=(2,))},
              [(0.0, (0.0, 0.0)), (1.0, (0.34, 0.0)), (1.3, (0.46, SILL)), (1.55, (0.5, SILL)), (1.92, (0.86, G)),
               (3.0, (1.22, G))],
              "Out through the Hanomag's stern doors, another man's way: ducking under the rolled canvas, one boot up "
              "onto the stern plate and stepping long down off it to the ground from there, the other boot following. "
              "Root motion as out_doors_a.")


def make_m3_exits():
    """Through the M3's open stern door (0.69 wide): her floor 0.94 over the ground; the canvas rolled across the top
    of her stern 0.78 to 0.90 out of the start, only 0.79 over her floor, so he squats low under it before he moves;
    the door's sill is her floor (0.97 to 1.02 out), the small step outside it 1.03 out and 0.14 under the floor, then
    the ground. The keys' x below are from the first draft's start: exit_clip adds LEAD (0.40) from the squat on."""
    G, ST, SX = -0.94, -0.14, 0.64
    exit_clip("out_door_a", "M3Halftrack", 3.2,
              [(0.0, dict(x=0.0, z=0.0, drop=0.08, lean=16.0)),
               (0.35, dict(x=0.03, z=0.0, drop=0.40, lean=60.0, head=40.0)),
               (0.7, dict(x=0.08, z=0.0, drop=0.54, lean=78.0, head=38.0)),
               (1.05, dict(x=0.24, z=0.0, drop=0.56, lean=80.0, head=40.0)),
               (1.4, dict(x=0.40, z=0.0, drop=0.54, lean=76.0, head=36.0)),
               (1.7, dict(x=0.58, z=ST, drop=0.42, lean=56.0, head=34.0)),
               (2.0, dict(x=0.74, z=ST, drop=0.22, lean=26.0, head=12.0)),
               (2.3, dict(x=0.94, z=G, drop=0.20, lean=18.0, head=8.0)),
               (2.7, dict(x=1.08, z=G, drop=0.08, lean=10.0, head=2.0)),
               (3.2, dict(x=1.20, z=G, drop=0.04, lean=8.0))],
              {"r": Steps("r", [(0.0, 0.4, (-0.05, -0.10, 0.0), -8.0), (0.55, 0.9, (0.14, -0.09, 0.0), -4.0),
                                (1.12, 1.4, (0.34, -0.08, 0.0), -2.0), (1.62, 2.1, (SX, -0.06, ST), -2.0),
                                (2.42, 3.2, (1.12, -0.11, G), -6.0)], lift=0.06),
               "l": Steps("l", [(0.0, 0.75, (0.04, 0.10, 0.0), 6.0), (0.95, 1.2, (0.24, 0.08, 0.0), 4.0),
                                (1.42, 1.75, (0.48, 0.07, 0.0), 2.0), (2.2, 3.2, (1.00, 0.12, G), 6.0)], lift=0.06)},
              [(0.0, (0.0, 0.0)), (1.4, (0.40, 0.0)), (1.7, (0.60, ST)), (2.0, (0.74, ST)), (2.3, (0.94, G)),
               (3.2, (1.2, G))],
              "Out through the M3's open stern door: squatting low under the canvas rolled across the top of her stern "
              "(0.79 m over her floor), short steps to the door, one boot out onto the small step under it and down "
              "to the ground, coming up as he clears the door; his rifle at the port. Root motion: the root goes with "
              "him onto the step and the ground.")
    exit_clip("out_door_b", "M3Halftrack", 2.9,
              [(0.0, dict(x=0.0, z=0.0, drop=0.08, lean=16.0)),
               (0.3, dict(x=0.03, z=0.0, drop=0.42, lean=62.0, head=32.0)),
               (0.65, dict(x=0.12, z=0.0, drop=0.56, lean=80.0, head=40.0)),
               (1.0, dict(x=0.30, z=0.0, drop=0.56, lean=80.0, head=40.0)),
               (1.3, dict(x=0.46, z=0.0, drop=0.50, lean=70.0, head=34.0)),
               (1.5, dict(x=0.62, z=-0.05, drop=0.30, lean=40.0, head=24.0)),
               (1.72, dict(x=0.88, z=G, drop=0.30, lean=26.0, head=14.0)),
               (1.9, dict(x=0.94, z=G, drop=0.36, lean=28.0, head=14.0)),
               (2.3, dict(x=1.06, z=G, drop=0.10, lean=12.0, head=4.0)),
               (2.9, dict(x=1.2, z=G, drop=0.04, lean=8.0))],
              {"l": Steps("l", [(0.0, 0.06, (0.04, 0.10, 0.0), 6.0), (0.35, 0.5, (0.16, 0.08, 0.0), 4.0),
                                (0.8, 0.95, (0.36, 0.07, 0.0), 2.0), (1.2, 1.45, (0.52, 0.07, 0.0), 2.0),
                                (1.7, 2.3, (0.94, 0.12, G), 6.0)], lift=0.06, fly=(3,)),
               "r": Steps("r", [(0.0, 0.2, (-0.05, -0.10, 0.0), -8.0), (0.6, 0.75, (0.26, -0.08, 0.0), -4.0),
                                (1.0, 1.32, (0.46, -0.08, 0.0), -4.0), (1.7, 2.0, (0.84, -0.12, G), -6.0),
                                (2.45, 2.9, (1.18, -0.11, G), -8.0)], lift=0.06, fly=(2,))},
              [(0.0, (0.0, 0.0)), (1.3, (0.46, 0.0)), (1.5, (0.62, -0.05)), (1.72, (0.88, G)), (2.9, (1.2, G))],
              "Out through the M3's stern door, another man's way: squatting under the rolled canvas to the door and "
              "jumping down from her floor past the step to the ground, landing and coming up. Root motion as "
              "out_door_a.")


def make_exits():
    make_truck_exits()
    make_hanomag_exits()
    make_m3_exits()


OUTS = {"Truck": ("out_tail_a", "out_tail_b"), "Hanomag": ("out_doors_a", "out_doors_b"),
        "M3Halftrack": ("out_door_a", "out_door_b")}
HEADROOM = {"Truck": "under her tilt 1.59 m over the floor, its rear opening 1.45: she carries men with it stowed "
                     "(swap tilt off); a man under it creeps (head under 1.45)",
            "Hanomag": "open-topped; the canvas rolled across the top of her stern is 1.18 over the floor at the doors: "
                       "out_doors_* duck under it from their start",
            "M3Halftrack": "open-topped; her machine gun stands over the aisle 0.94-1.15 over the floor from 0.95 behind "
                           "its pedestal (x -0.95) to the cab; the canvas rolled across the top of her stern is only "
                           "0.79 over the floor at the door: out_door_* squat under it from their start"}


def aisle():
    """ride_clips.aisle: each carrier's floor, where her men stand up, and her way out (the game's axes)."""
    out = {}
    for v in CARRIERS:
        st = np.asarray(EXIT_AT[v], float)
        ways = {}
        for n in OUTS[v]:
            e = C.g2b(ROOT[n](C.CREW[n]["long"]))            # (the root's last key, the clip's frame, game axes)
            end = st + np.array((-e[0], -e[1], e[2]))          # (the clip faces aft: its x is the vehicle's -x)
            ways[n] = {"long": C.CREW[n]["long"], "ends_at": [round(float(x), 3) for x in end]}
        rises = {}
        for n in NAMES:
            if n.startswith(("bench_rise_", "bench_sit_")):
                for k, pc, s_, off in C.CREW[n]["place"]:
                    if pc == v:
                        rises.setdefault(s_, {})["rise" if "rise" in n else "sit"] = n
        out[v] = {"floor_z": FLOOR[v], "bench_top_over_floor": 0.455 if v == "Truck" else 0.44,
                  "way_out": {"socket": "step", "start": [round(float(x), 3) for x in st],
                              "root_offset": list(exit_offset(v)), "clips": ways,
                              "open_first": "play her `dismount` clip (it opens the "
                                            + ("tailboard" if v == "Truck" else "stern door" + ("s" if v == "Hanomag" else ""))
                                            + ") before the first man reaches the start"},
                  "seats": rises, "headroom": HEADROOM[v]}
    return out


def make_clips():
    NAMES[:] = ["bench_ride", "die_bench_slump"]
    EXITS[:] = []
    for f in (make_bench, build_bench_death, make_rise, make_exits):
        f()
    return list(NAMES)


def add_pictures():
    """The pictures (crew.build_picture: `eye` and `target` in Blender's axes, y to the vehicle's left)."""
    P = C.PICTURES
    P.clear()
    D = LR.DIE
    P["trucks_bench"] = {"kind": "vehicle", "piece": "Truck",
                         "men": [("bench_ride", 0.4, "rider.1"), ("bench_ride", 1.3, "rider.2"), ("bench_ride", 2.2, "rider.3"),
                                 ("die_bench_slump", D, "rider.4"), ("bench_ride", 3.1, "rider.5"), ("bench_ride", 0.9, "rider.6"),
                                 ("bench_ride", 1.8, "rider.7"), ("bench_rise_r", 0.75, "rider.8"), ("bench_ride", 2.7, "rider.9"),
                                 ("bench_rise_r", RISE, "rider.10")],
                         "eye": (-7.4, 3.4, 3.9), "target": (-1.7, 0.0, 1.3), "lens": 30,
                         "about": "The Truck's benches (her tilt stowed): men riding (bench_ride), one dead (die_bench_slump), "
                                  "one getting up (bench_rise_r) and one up, turned aft toward the tailboard."}
    P["trucks_out"] = {"kind": "vehicle", "piece": "Truck",
                       "men": [("bench_ride", 1.0, "rider.1"), ("bench_ride", 2.0, "rider.6"), ("out_tail_a", 0.55, "step"),
                               ("out_tail_a", 1.0, "step"), ("out_tail_a", 2.4, "step")],
                       "eye": (-5.6, 5.4, 2.0), "target": (-3.6, 0.0, 0.9), "lens": 30,
                       "about": "Over the Truck's tailboard (out_tail_a): crouched at the edge, in the air, and on the ground."}
    P["hanomag_out"] = {"kind": "vehicle", "piece": "Hanomag",
                        "men": [("bench_ride", 1.0, "rider.1"), ("bench_ride", 2.0, "rider.7"), ("out_doors_a", 0.45, "step"),
                                ("out_doors_a", 1.45, "step"), ("out_doors_a", 2.9, "step")],
                        "eye": (-5.0, 4.8, 2.4), "target": (-2.6, 0.0, 0.9), "lens": 30,
                        "about": "Out of the Hanomag's stern doors (out_doors_a): ducking under the rolled canvas, on the "
                                 "stern plate, and on the ground."}
    P["m3_out"] = {"kind": "vehicle", "piece": "M3Halftrack",
                   "men": [("bench_ride", 1.0, "rider.2"), ("bench_rise_r_aft", RISE, "rider.6"), ("out_door_a", 0.9, "step"),
                           ("out_door_a", 1.9, "step"), ("out_door_a", 3.2, "step")],
                   "eye": (-5.8, 5.0, 2.5), "target": (-3.0, 0.0, 1.0), "lens": 30,
                   "about": "Out of the M3's stern door (out_door_a): squatting under the rolled canvas, on the step, "
                            "and on the ground; a front-seat man up, aft of the machine gun."}


def main(argv):
    only = set(argv[argv.index("--only") + 1].split(",")) if "--only" in argv else {"clips", "check", "ref"}
    t0 = time.time()
    import kit
    kit.fresh(C.FPS)
    C.setup("GER", "rifle")
    names = make_clips()
    if "--clips" in argv:
        names = argv[argv.index("--clips") + 1].split(",")
    print(f"setup and clips {time.time() - t0:.0f} s: {names}")
    state_path = os.path.join(WORK, "truck_riders_state.json")
    state = json.load(open(state_path)) if os.path.exists(state_path) else {}
    if "check" in only:
        seats = [n for n in names if n not in EXITS]
        outs = [n for n in names if n in EXITS]
        if seats:
            state.setdefault("checks", {}).update(LR.check_all(seats))
        if outs:
            OPENED["on"] = True
            state.setdefault("checks", {}).update(LR.check_all(outs))
            OPENED["on"] = False
        print(f"checked {time.time() - t0:.0f} s")
    os.makedirs(WORK, exist_ok=True)
    json.dump(state, open(state_path, "w"), indent=1)
    if "check" in only:
        rows = {k: [{"at": r["at"], "deepest_m": r["deepest"], "where": r["where"][:2] if r["where"] else None,
                     "grip_gap_m": r["grip_gap"]} for r in v] for k, v in state["checks"].items() if k in NAMES}
        with open(os.path.join(WORK, "truck_riders_checks.json"), "w") as f:
            json.dump({"how": "truck_riders.py check_all on the pattern man the same way, at every bench seat and way "
                              "out (the Truck with her tilt stowed; the ways out with her board or doors opened as her "
                              "dismount clip leaves them).", "clips": rows}, f, indent=1)
    if "pictures" in only:
        add_pictures()
        PICT["on"] = True
        want = [a for a in argv if a in C.PICTURES] or list(C.PICTURES)
        for pn in want:
            path = C.build_picture(pn, fast="--fast" in argv)
            print("picture", path, f"{time.time() - t0:.0f} s")
            make_clips()           # (build_picture starts afresh: the clips are measured again on its man)
        PICT["on"] = False
    if "ref" in only:
        out = LR.reference(names)
        out["aisle"] = aisle()
        for n in names:
            if n in ROOT:             # (the ways out's root motion: the root under his feet, the game's axes)
                rc = out["clips"][n]
                rc["root"] = [[round(float(x), 5) for x in C.g2b(ROOT[n](t))] for t in rc["times"]]
        with open(LR.REF + ".part", "w") as f:
            json.dump(out, f, separators=(",", ":"))
        os.replace(LR.REF + ".part", LR.REF)
        print(f"reference {time.time() - t0:.0f} s")


if __name__ == "__main__":
    main(sys.argv)
