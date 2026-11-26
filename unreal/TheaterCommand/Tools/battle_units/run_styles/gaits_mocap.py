"""The gaits' motion capture (25 November 2026: "can we have different running animations for the soldiers? it looks
like they are all jogging at the same speed"): CMU takes cut into loops on the 1.75 m reference man the way
run_styles_mocap.py cuts the run styles (mocap.py's find_cycle, close_loop, ground_fit, crouch, foot_lock, fit_arms,
ease_spikes), for a ladder of gaits by speed: three sprints, two dashes between cover, a loaded jog, a tired run, a
limp, and a stumble inside the run. Written in run_styles_clips.json's form to gaits_clips.json (so run_styles.py's
Capture reads it), each gait with its speed, stride, length, source, note, and a "pace": the ground speeds it can be
played at on the reference man.

    python mocap_cmu/fetch_cmu.py 102_10 16_46 127_08 127_06 127_07 02_03 35_17 142_12 35_18    (the takes; 35_18
                                                                     is run_a's, for the stumble and the ladder)
    LOOKS_TOOLS=... bpyenv/bin/python gaits_mocap.py [names...]      cut (all, or the named), write gaits_clips.json
    python3 gaits_mocap.py --pictures [OUT_DIR]                      the strips and the ladder (numpy and PIL only)
    python3 gaits_mocap.py --table                                   what is in gaits_clips.json

What a gait's spec may ask for, past make_loop's own (take, span, period, head, ground, crouch, v_tol, h_tol), each
laid over the cut loop in this order, the feet then locked on the ground as in every loop:

    frames   the loop's length in output frames (30 a second): the capture played slower or faster, evenly
    mirror   left and right changed over (before the cut)
    uneven   one step a little longer than the other: time bent by this share of a step
    reach    the feet's fore-and-aft travel about each hip, times this (by IK on the ankles; the stride with it)
    scuff    the swinging foot's height over the ground, times this (under 1: scuffing; over 1: a higher heel)
    bounce   the pelvis's rise and fall about its mean, times this (the legs take it up)
    crouch   (drop m, lean degrees): mocap.crouch, the head kept up
    tiptoe   up on the balls of the feet where the crouch would bend an ankle past this many degrees
    roll     the shoulders' roll over the standing foot, degrees (the head kept level)
    arms     (swing, straighten, hang): the upper arms' swing about their mean times `swing`, the elbows part way
             to straight, the upper arms part way to hanging
    weary    (nod, loll, heave degrees): the head dropping at each footfall and lolling over the standing foot,
             the shoulders heaving
    favour   (the limp) the hurt (left) leg's stance hurried: the loop's time runs 1 + favour as fast in the middle of
             that stance and 1 - favour as fast half a loop away (warp_favour; before the feet are locked)
and after the feet are locked:
    heel_clear  metres: a foot still travelling over the ground keeps its heel this high, turned toe-down about the
             ball (heel_clear: a forefoot landing, no heel tap before the ball lands)
    ease_toes   deg/s: the balls of the feet eased to turn no faster than this (mocap.ease_spikes), toes kept out of
             the ground
    knee_side   degrees: two_bone_ik keeps each knee this near its hinge while the gait is cut (mocap.KNEE_SIDE; 35
             otherwise)

Mended 26 November 2026 after two reviews: the limp's hurt-leg stance was only 12 per cent shorter than the good
leg's (now favour 0.18: 14 frames on the ground against 19); the sprints' and dashes' landing heel tapped the ground
before the ball landed (now heel_clear 4.5 cm); the limp's toe snapped 36 degrees between two keys (now eased); the
stumble's knees bent 22 degrees off their hinge (now knee_side 10: 4 at most).
"""
import json
import math
import os
import sys

import numpy as np

LOOKS = os.environ.get("LOOKS_TOOLS", "/home/user/work/looks/battle_units")
sys.path.insert(0, LOOKS)
import mocap  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "gaits_clips.json")
RUN_STYLES = os.path.join(HERE, "run_styles_clips.json")     # (read only: run_a, for the stumble and the ladder)
REST_REF = os.path.join(LOOKS, "mocap_cmu", "rest_ref.json")
PICS = "/home/user/art/gaits/pics/cut"
FPS = mocap.FPS
GRAVITY = 9.81

STYLES = [
    dict(name="sprint_a", take="102_10", loop=True, head=4, ground=10, frames=17, reach=1.03, v_tol=1.0, kind="sprint",
         heel_clear=0.045,
         note="Subject 102 (basketball), 'RunningNoBall': the fastest steady straight run found in the library, 5.04 m/s "
              "on the reference man, a long driving stride (3.19 m), the heels kicked high, the trunk 30 degrees "
              "forward as he ran it. Driven to 5.8: the loop played in 17 frames for the capture's 19, the feet's "
              "fore-and-aft reach about the hips 3 per cent longer (by IK), the feet locked again; a foot coming in "
              "to land keeps its heel 4.5 cm up (it lands on the ball)."),
    dict(name="sprint_b", take="16_46", loop=True, head=3, ground=10, frames=17, reach=1.04, crouch=(0.02, 10),
         scuff=1.12, v_tol=1.0, kind="sprint", heel_clear=0.045,
         note="Subject 16, 'run/jog' (his fast take, 4.32 m/s as captured; his jog 16_36 is run_c): a clean, even "
              "runner. Driven to 5.3: 17 frames for the capture's 20, reach 4 per cent longer, the trunk 10 degrees "
              "further forward and the pelvis 2 cm lower (mocap.crouch, the head kept up), the swinging heel 12 per "
              "cent higher; the landing foot's heel kept 4.5 cm up."),
    dict(name="sprint_c", take="127_08", loop=True, head=3, ground=10, frames=16, reach=1.065, crouch=(-0.05, -8),
         v_tol=1.6, h_tol=0.05, kind="sprint", heel_clear=0.045,
         note="Subject 127 (action adventure), 'Run': 4.97 m/s as captured, driving hard, hunched 39 degrees forward "
              "with the knees deep. Driven to 6.3 and stood up a little so it is a sprint, not the dash: 16 frames "
              "for the capture's 19, reach 6.5 per cent longer, the pelvis 5 cm higher and the trunk 8 degrees less "
              "forward (mocap.crouch, the other way). The landing foot came in heel first, tapped the ground and "
              "landed again on its ball: its heel is now kept 4.5 cm up until the ball lands (heel_clear)."),
    dict(name="dash_a", take="127_06", loop=True, head=0, ground=10, frames=18, reach=0.88, crouch=(0.10, 4),
         tiptoe=30.0, kind="dash", heel_clear=0.045,
         note="Subject 127, 'Run' (another take, 4.24 m/s): the library's one runner who runs hunched (38 degrees "
              "forward, the knees bent). Made the bound between cover: the pelvis 10 cm lower again and the trunk 4 "
              "degrees further forward (mocap.crouch: deeper than the advance's 14 cm and 28 degrees over an upright "
              "jog), the head held up, the steps shortened to 88 per cent and quickened (18 frames for 20). So deep "
              "a crouch bends the standing ankle further than an ankle goes: he is put up on the ball of the foot "
              "where it passes 30 degrees (the heel lifted about the ball, the knee taking it up); the landing "
              "foot's heel kept 4.5 cm up."),
    dict(name="dash_b", take="127_07", loop=True, head=0, ground=10, frames=17, reach=0.96, crouch=(0.08, 5),
         tiptoe=30.0, v_tol=1.0, h_tol=0.05, kind="dash", heel_clear=0.045,
         note="Subject 127, 'Run' (a third take, 4.63 m/s, 36 degrees forward): the faster dash. The pelvis 8 cm "
              "lower and the trunk 5 degrees further forward (mocap.crouch), the head held up, the steps 96 per cent "
              "and quicker (17 frames for 20), up on the ball of the foot where the ankle would bend past 30 "
              "degrees; the landing heel, which tapped the ground before the ball landed, kept 4.5 cm up."),
    dict(name="jog_loaded", take="02_03", loop=True, head=8, ground=10, reach=0.85, scuff=0.7, bounce=0.4,
         crouch=(0.07, 15), tiptoe=30.0, roll=6.0, arms=(0.45, 0.0, 0.25), kind="jog",
         note="Subject 2, 'run/jog' (2.73 m/s, the slowest jog in the library). No take of a man running under a "
              "load, so the load is laid over it: the steps 85 per cent as long at the capture's own beat, the feet "
              "kept low (70 per cent as high), the pelvis's bounce cut to 40 per cent and 7 cm lower on bent knees, "
              "the trunk 15 degrees into it (mocap.crouch), the shoulders rolling 6 degrees over the standing foot, "
              "the arms held lower and swung 45 per cent, the heel lifted where the bent knee would bend the ankle "
              "past 30 degrees."),
    dict(name="run_tired", take="35_17", loop=True, head=36, ground=10, frames=24, reach=0.88, uneven=0.12,
         scuff=0.45, bounce=0.85, crouch=(0.04, 8), roll=3.0, arms=(0.5, 0.7, 0.6), weary=(12.0, 9.0, 6.0),
         kind="jog",
         note="Subject 35, 'run/jog' (the file's own runner, another take: 3.21 m/s). No take of a tired runner, so the "
              "tiredness is keyed over it: the steps 88 per cent and slower (24 frames for 22), one step an eighth "
              "longer than the other, the swinging feet under half as high (scuffing), the trunk 8 degrees forward and "
              "sagging 4 cm, the head hung 36 degrees and dropping 12 more after each footfall, lolling 9 over the "
              "standing foot, the shoulders heaving 6, the arms low (the elbows 70 per cent of the way to hanging "
              "open, the upper arms 60 per cent of the way to straight down, the swing half)."),
    dict(name="limp_l", take="142_12", loop=True, head=4, span=(3844, 4460), kind="walk", favour=0.18, ease_toes=330.0, knee_side=10.0,
         note="Subject 142 (stylized walks), 'Painfulleftknee': a real limp, the LEFT leg the hurt one as captured "
              "(no mirror): the left knee held stiff (bent 21 degrees in its stance, the right 42), the left foot a "
              "shorter time on the ground, the trunk forward and dipping onto it. His faster pass (1.05 m/s). He "
              "gets off the hurt leg sooner than he did: the loop's time runs 18 per cent faster in the middle of "
              "its stance and slower on the good leg (favour), so the left foot is down 14 frames against the "
              "right's 19 (15 against 17 as captured); the right toe's snap at its toe-off eased (330 deg/s), "
              "the knees kept within 10 degrees of their hinge."),
    dict(name="stumble", make="stumble", loop=False, on="run_a", frames=30, rejoin=16, kind="once", knee_side=10.0,
         note="No trip inside a run in the library (104_13 'StumbleWalk' is a walk; 16_08 and 16_57 are stops), so it "
              "is keyed on run_a (CMU 35_18, the file's own run): the first frame is run_a's first, the last run_a's "
              "frame 16 (0.5333 s). The left toe catches as it swings through and drags, the run checked, the trunk "
              "pitching 40 degrees forward and the pelvis dropping 8 cm, the arms thrown out; then two quick short "
              "steps (left, right) under him, the third (left) lands back in the run. The knees kept within 10 "
              "degrees of their hinge as it is keyed (they bent 22 off it)."),
]
# Takes looked at and left out (measured on the reference man; measure_takes.py, profile_takes.py and rate_check.py
# in /home/user/art/gaits/work/):
#   143_01, 141_01 'Run': read as 7 m/s sprints, but the pelvis falls at 3.2 to 3.7 g in the air: they are 60 a second
#     captures marked 120 in the conversion, really 3.5 m/s jogs (so are 141_03, 143_42).
#   104_48, 104_37 'RunThrough' (4.6, 4.2 m/s): a poor seam (cost 0.39) and feet that do not come down to the ground.
#   16_45 (as 16_46, a little slower), 127_03 (turning), 16_55, 16_56 (accelerating all the way), 139_10 (weaving),
#     102_05 (as 102_10 but dribbling a ball), 09_01..09_11 (3.5 to 3.7 m/s, subject 9: run_b and run_d),
#     35_19..35_26 (3.3 to 3.6 m/s, subject 35 again), 104_01, 104_04 'JogThrough' (3.4 m/s, a bounding lope).
#   No steady sprint over 5.1 m/s is in the library: the three sprints are its three fastest steady runners, driven.
#   Crouched runs: 127_29..32, 37, 38 and 128_04 'Run Duck Underneath' (one duck, not a run), 77_10, 77_11 'careful
#     run' and 139_11 'Run To Sneak' (bursts of a second, speeding up and slowing: no loop).
#   Limps: 91_16 / 105_16 'Limp' (upright, the arms held out, 1.3 m/s: mild), 91_24 / 105_24 'HurtLegWalk' and 91_25 /
#     105_25 'DragBadLegWalk' (0.5 m/s: too slow), 77_19..24 = 139_19..24 'limping, hurt right leg' (bent 50 to 60
#     degrees over: the soldier's old `stumble` loop), 17_05 'walk/hobble' (0.5 to 0.7 m/s, turning).
#   Tired: 91_21 'acheyWalk', 91_14 'DepressedWalk' (walks, 0.4 to 0.5 m/s). Stumbles: 104_13, 91_59, 22_12 (walks).



# ================================================================================================ what is laid over
def mirror(m):
    """Left and right changed over: every bone takes the other side's turn from rest, mirrored in his mid plane."""
    r = m.rest
    S = np.diag((1.0, -1.0, 1.0))
    W = m.W.copy()
    for b, n in enumerate(r.names):
        o = (n[:-2] + ("_r" if n.endswith("_l") else "_l")) if n.endswith(("_l", "_r")) else n
        bo = r.i(o)
        W[:, b] = S @ (m.W[:, bo] @ r.R[bo].T) @ S @ r.R[b]
    m.W = W
    m.H = m.H * np.array((1.0, -1.0, 1.0))
    return m


def _sstep(u):
    u = np.clip(u, 0.0, 1.0)
    return u * u * (3 - 2 * u)


def _pulse(k, a, b, c, d):
    """0 before a, eased up to 1 at b, 1 until c, eased down to 0 at d."""
    return _sstep((k - a) / (b - a)) * (1.0 - _sstep((k - c) / (d - c)))


def close(m):
    m.W[-1], m.H[-1] = m.W[0], m.H[0]


def stances(m, stride, h_tol=0.035, v_tol=0.45):
    """Each foot's stances on a loop, as foot_lock finds them: {side: [(first frame, one past the last), ...]} in
    frames of the loop (a stance over the seam runs past N)."""
    r = m.rest
    N = len(m) - 1
    base = m.slice(0, N)
    work = mocap.Motion(r, np.concatenate([base.W] * 3), np.concatenate([base.H] * 3), m.fps)
    hd = work.heads()
    tt = np.arange(len(work))
    out = {}
    for s in ("l", "r"):
        ft, bl = r.i("foot_" + s), r.i("ball_" + s)
        sh, _ = mocap.sole_heights(work, hd, s)[s]
        trav = hd[:, ft] + np.outer(tt * stride / N, [1, 0, 0])
        pivot = r.head[bl].copy()
        pivot[2] = r.sole["ground"]
        ball = trav + np.einsum("fij,j->fi", work.W[:, ft], r.R[ft].T @ (pivot - r.head[ft]))
        spd = np.minimum(np.linalg.norm(np.gradient(trav[:, :2], axis=0), axis=1),
                         np.linalg.norm(np.gradient(ball[:, :2], axis=0), axis=1)) * work.fps
        base_h = min(float(np.percentile(sh, 3)), r.sole["ground"] + 0.06)
        cs = []
        for c in mocap.contacts(sh, spd, base_h, h_tol, v_tol):
            if cs and c[0] - cs[-1][1] <= 2:
                cs[-1] = (cs[-1][0], c[1])
            else:
                cs.append(c)
        out[s] = [(a - N, b - N) for a, b in cs if N <= a < 2 * N]
    return out


def step_angle(m, stride, **kw):
    """Per frame, the angle round the loop from the left foot's footfall (0 at it, 2 pi a loop later), and the
    angle at the middle of the left foot's stance."""
    N = len(m) - 1
    st = stances(m, stride, **kw)
    a, b = max(st["l"], key=lambda c: c[1] - c[0]) if st["l"] else (0, N // 2)
    th = 2 * np.pi * (np.arange(N + 1) - a) / N
    return th, np.pi * (b - a) / N


def reach(m, k, mid=None):
    """The feet's fore-and-aft travel about each hip, times k (a number, or one a frame), the ankles moved by IK; the
    feet keep their turns. `mid`: the place under the hip the travel is scaled about (default its mean)."""
    r = m.rest
    N = len(m) - 1
    for s in ("l", "r"):
        hd = m.heads()
        hip, ank = hd[:, r.i("thigh_" + s)], hd[:, r.i("foot_" + s)]
        rel = ank[:, 0] - hip[:, 0]
        c = rel[:N].mean() if mid is None else mid[s]
        T = ank.copy()
        T[:, 0] = hip[:, 0] + c + (rel - c) * k
        mocap.two_bone_ik(m, s, T, np.ones(len(m)), hd)


def scuff(m, k, clear=0.04):
    """The swinging foot's height over the ground, times k: what the ball of each foot rises past `clear` over its
    own lowest (so it still leaves the ground cleanly, and is not read as planted while it swings), the ankle moved
    by IK."""
    r = m.rest
    for s in ("l", "r"):
        hd = m.heads()
        ball = hd[:, r.i("ball_" + s), 2]
        z0 = ball.min() + clear
        T = hd[:, r.i("foot_" + s)].copy()
        T[:, 2] += np.where(ball > z0, (ball - z0) * (k - 1.0), 0.0)
        mocap.two_bone_ik(m, s, T, np.ones(len(m)), hd)


def tiptoe(m, limit=30.0, rounds=3):
    """Up on the balls of the feet where a deep knee over a flat foot would bend the ankle past `limit` degrees
    (rom.py's dorsiflexion: a real ankle gives 25): the foot turned heel-up about the ground under its ball by the
    excess, the leg following by IK. A man running low runs on his forefeet."""
    import rom
    r = m.rest
    body = rom.Body.of_rest(r)
    lat = np.array((0.0, 1.0, 0.0))
    most = 0.0
    for _ in range(rounds):
        an = rom.measure(body, rom.Poses.of_motion(m))
        for s in ("l", "r"):
            over = np.maximum(an["an_dorsi_" + s] - limit, 0.0)
            if over.max() < 0.5:
                continue
            most = max(most, float(over.max()))
            over = mocap.cyc_smooth(over, 1)
            hd = m.heads()
            ft, bl = r.i("foot_" + s), r.i("ball_" + s)
            pivot_rest = r.head[bl].copy()
            pivot_rest[2] = r.sole["ground"]
            piv = hd[:, ft] + np.einsum("fij,j->fi", m.W[:, ft] @ r.R[ft].T, pivot_rest - r.head[ft])
            G = mocap.axis_angle(lat, np.radians(over))
            T = piv + np.einsum("fij,fj->fi", G, hd[:, ft] - piv)
            for b in mocap.subtree(r, "foot_" + s):
                m.W[:, b] = G @ m.W[:, b]
            mocap.two_bone_ik(m, s, T, np.ones(len(m)), hd)
    return round(most, 1)


def clear_swing(m, stride, h=0.04, v0=0.35, v1=0.9):
    """A foot that is moving over the ground is not left skimming it: where the ball of a foot travels faster than
    v0..v1 m/s over the ground (the loop in place, the ground going back at the stride's speed) within `h` of its
    lowest, the foot is lifted to `h` (by IK), so only a planted foot reads as planted: the game's feet test takes
    a ball within 2 cm of its lowest as down, and run_styles.py's plant one within 3.5 cm at a key (2.5 at its second
    try), on legs that are not the reference man's; with 2.6 cm here the faster dash's incoming foot, still 5 cm out
    to the side, was taken as planted on a soldier file and the clip refused. Returns the most it lifted a foot."""
    r = m.rest
    N = len(m) - 1
    most = 0.0
    for s in ("l", "r"):
        hd = m.heads()
        b = hd[:N, r.i("ball_" + s)].copy()
        b[:, 0] += stride / N * np.arange(N)
        d = np.roll(b, -1, 0) - np.roll(b, 1, 0)
        d[:, 0] += np.where(np.arange(N) == N - 1, stride, 0.0) + np.where(np.arange(N) == 0, stride, 0.0)
        sp = np.hypot(d[:, 0], d[:, 1]) * m.fps / 2.0
        z = b[:, 2] - b[:, 2].min()
        need = np.maximum(h - z, 0.0) * _sstep((sp - v0) / (v1 - v0))
        if need.max() < 0.002:
            continue
        most = max(most, float(need.max()))
        need = np.concatenate([need, need[:1]])
        T = hd[:, r.i("foot_" + s)].copy()
        T[:, 2] += need
        mocap.two_bone_ik(m, s, T, (need > 0).astype(float), hd)
    close(m)
    return round(most, 4)


def bounce(m, k):
    """The pelvis's rise and fall about its mean, times k; the ankles stay where they were (the legs take it up)."""
    r = m.rest
    N = len(m) - 1
    hd = m.heads()
    ank = {s: hd[:, r.i("foot_" + s)].copy() for s in ("l", "r")}
    z = m.H[:, 2]
    m.H[:, 2] = z[:N].mean() + (z - z[:N].mean()) * k
    for s in ("l", "r"):
        mocap.two_bone_ik(m, s, ank[s], np.ones(len(m)), m.heads())


def _fwd(m):
    yaw = mocap.yaw_of(m)
    return np.stack([np.cos(yaw), np.sin(yaw), np.zeros_like(yaw)], 1)


def roll(m, deg, th, mid):
    """The shoulders roll over the standing foot: spine_02 and the chest lean `deg` (together) to the side of the
    foot that is down, the neck leaning half of it back so the head stays level."""
    a = -math.radians(deg) * np.cos(th - mid)            # (about his forward axis: + leans him to his RIGHT)
    ax = _fwd(m)
    mocap.bend(m, "spine_02", a * 0.5, ax)
    mocap.bend(m, "chest", a * 0.5, ax)
    mocap.bend(m, "neck", -a * 0.5, ax)


def _mean_rot(Q):
    q = mocap.mat_to_quat(Q)
    q = np.where((q @ q[0])[:, None] < 0, -q, q)
    return mocap.quat_to_mat(q.mean(0))


ELBOW_SLACK = 18.0        # (degrees of bend left in an elbow "straight": a hanging arm is not a locked one)


def arms(m, swing=1.0, straight=0.0, hang=0.0):
    """The upper arms' swing about their mean times `swing`; the elbows `straight` of the way to hanging open (the
    rig's rest elbow is bent 47 degrees: open is ELBOW_SLACK); the upper arms `hang` of the way to straight down."""
    r = m.rest
    N = len(m) - 1
    L = mocap.to_local(m)
    for s in ("l", "r"):
        ua, la, ha = r.i("upperarm_" + s), r.i("lowerarm_" + s), r.i("hand_" + s)
        mean = _mean_rot(L[:N, ua])
        L[:, ua] = mean @ mocap.rot_exp(mocap.rot_log(mean.T @ L[:, ua]) * swing)
        u0, l0 = mocap._norm(r.head[la] - r.head[ua]), mocap._norm(r.head[ha] - r.head[la])
        bend0 = math.acos(float(np.clip(u0 @ l0, -1, 1)))
        G = mocap.axis_angle(np.cross(l0, u0), max(bend0 - math.radians(ELBOW_SLACK), 0.0))
        open_ = r.R[la].T @ G @ r.R[la]
        L[:, la] = mocap.slerp_mats(L[:, la], np.broadcast_to(open_, L[:, la].shape), np.full(len(m), straight))
    m.W[:] = mocap.from_local(r, L, m.H, m.fps).W
    if hang:
        for s in ("l", "r"):
            hd = m.heads()
            ua, la = r.i("upperarm_" + s), r.i("lowerarm_" + s)
            u = mocap._norm(hd[:, la] - hd[:, ua])
            G = mocap.slerp_mats(np.broadcast_to(np.eye(3), (len(m), 3, 3)),
                                 mocap.swing(u, np.broadcast_to(np.array((0.0, 0.0, -1.0)), u.shape)),
                                 np.full(len(m), hang))
            for b in mocap.subtree(r, "upperarm_" + s):
                m.W[:, b] = G @ m.W[:, b]


def weary(m, nod, loll, heave, th, mid):
    """A tired man's head and shoulders: the head drops `nod` degrees after each footfall and comes up again, lolls
    `loll` degrees over the standing foot, and the shoulders heave `heave` degrees (both up together once a step,
    the chest rising with them)."""
    ax = _fwd(m)
    drop = math.radians(nod) * 0.5 * (1.0 - np.cos(2 * (th - 0.9)))        # (lowest a little after each footfall)
    mocap.bend(m, "neck", drop * 0.5)
    mocap.bend(m, "head", drop * 0.5)
    lol = -math.radians(loll) * np.cos(th - mid - 0.5)
    mocap.bend(m, "neck", lol * 0.4, ax)
    mocap.bend(m, "head", lol * 0.6, ax)
    up = math.radians(heave) * 0.5 * (1.0 + np.cos(2 * (th - 2.2)))
    r = m.rest
    for s, sg in (("l", 1.0), ("r", -1.0)):
        mocap.bend(m, "clavicle_" + s, sg * up, ax)                         # (about forward: each shoulder up)
        for b in mocap.subtree(r, "upperarm_" + s):                         # (the arm hangs as it did: turned back)
            m.W[:, b] = mocap.axis_angle(ax, -sg * up) @ m.W[:, b]
    mocap.bend(m, "chest", -up * 0.35)


def warp_uneven(m, share):
    """One step a little longer than the other: the loop's time bent by `share` of a step (a loop in, a loop out)."""
    N = len(m) - 1
    u = np.arange(N + 1) / N
    t = (u + share * 0.5 * np.sin(2 * np.pi * u) / (2 * np.pi) * 2) * N
    out = mocap.resample(m, np.clip(t, 0, N))
    close(out)
    return out


def warp_favour(m, share, c):
    """The hurt leg's stance hurried (an antalgic limp: the man gets off the hurt leg quickly and lingers on the good
    one): the loop's time runs (1 + share) as fast at `c` (a share of the loop, in the capture's own time: the middle
    of the hurt leg's stance) and (1 - share) as fast half a loop away, eased between; the loop's length, its first
    frame and its seam are kept. Returns the warped loop."""
    N = len(m) - 1
    k = share / (2 * np.pi)
    cp = c
    for _ in range(12):                      # (the output time whose source time is c: the warp centred there)
        cp = c - k * math.sin(2 * np.pi * cp)
    u = np.arange(N + 1) / N
    t = u + k * (np.sin(2 * np.pi * (u - cp)) + math.sin(2 * np.pi * cp))
    out = mocap.resample(m, np.clip(t * N, 0, N))
    close(out)
    return out


def heel_clear(m, stride, h=0.045, v0=0.8, v1=2.0, rounds=2):
    """A running foot coming in to land on its ball does not tap its heel on the ground first: where the heel of a
    foot that is still travelling over the ground (faster than v0..v1 m/s, eased in between; a planted or slowing
    foot is left alone) is lower than `h` over the ground, the foot is turned toe-down about the ball of its sole by
    as much as lifts the heel to `h` (not past where the toe would reach the ground), the leg following by IK. The
    loop in place, the ground going back at the stride's speed. Returns the most it turned a foot, degrees."""
    r = m.rest
    N = len(m) - 1
    ground = r.sole["ground"]
    lat = np.array((0.0, 1.0, 0.0))
    most = 0.0
    for s in ("l", "r"):
        ft, bl = r.i("foot_" + s), r.i("ball_" + s)
        f0 = r.head[ft]
        heel_rest = np.array((f0[0] - 0.061, f0[1], ground))
        piv_rest = r.head[bl].copy()
        piv_rest[2] = ground
        toe_rest = np.array((r.head[bl][0] + 0.085, r.head[bl][1], ground + 0.004))
        for _ in range(rounds):
            hd = m.heads()
            Rf = m.W[:, ft] @ r.R[ft].T
            Rb = m.W[:, bl] @ r.R[bl].T
            heel = hd[:, ft] + np.einsum("fij,j->fi", Rf, heel_rest - f0)
            piv = hd[:, ft] + np.einsum("fij,j->fi", Rf, piv_rest - f0)
            toe = hd[:, bl] + np.einsum("fij,j->fi", Rb, toe_rest - r.head[bl])
            hx = heel[:N].copy()
            hx[:, 0] += stride / N * np.arange(N)
            d = np.roll(hx, -1, 0) - np.roll(hx, 1, 0)
            d[N - 1, 0] += stride
            d[0, 0] += stride
            sp = np.hypot(d[:, 0], d[:, 1]) * m.fps / 2.0
            want = np.maximum(ground + h - heel[:N, 2], 0.0) * _sstep((sp - v0) / (v1 - v0))
            if want.max() < 0.001:
                break
            arm = np.linalg.norm((heel - piv)[:N, [0, 2]], axis=1)
            ang = np.arcsin(np.clip(want / np.maximum(arm, 1e-6), 0.0, 0.9))
            # (not past the toe: the toe's tip goes down about the pivot by its own arm's sine)
            room = np.maximum(toe[:N, 2] - ground - 0.006, 0.0)
            tarm = np.maximum(np.linalg.norm((toe - piv)[:N, [0, 2]], axis=1), 1e-6)
            ang = np.minimum(ang, np.arcsin(np.clip(room / tarm, 0.0, 0.9)))
            ang = np.concatenate([ang, ang[:1]])
            if ang.max() < 1e-3:
                break
            most = max(most, math.degrees(float(ang.max())))
            side = np.einsum("fij,j->fi", Rf, lat)
            G = mocap.axis_angle(side, ang)              # (about his crosswise axis: + turns the toe down)
            T = piv + np.einsum("fij,fj->fi", G, hd[:, ft] - piv)
            for b in mocap.subtree(r, "foot_" + s):
                m.W[:, b] = G @ m.W[:, b]
            mocap.two_bone_ik(m, s, T, (ang > 0).astype(float), hd)
    close(m)
    return round(most, 1)


# ================================================================================================ one gait
def make_gait(spec, rest, verbose=True):
    """A loop cut as mocap.make_loop cuts it, with the spec's extras laid over it before the feet are locked."""
    m = mocap.take_motion(spec["take"], rest)
    if spec.get("mirror"):
        sc = m.scale
        mirror(m)
        m.scale = sc
    if spec.get("span"):
        lo, hi = max(0, spec["span"][0] - 1), min(len(m) - 1, spec["span"][1] - 1)
    else:
        lo, hi, _ = mocap.steady_span(m)
    a, b, cost, per = mocap.find_cycle(m, lo, hi, period=spec.get("period"), steps=spec.get("steps", 1))
    cyc = m.slice(a, b + 1)
    if spec.get("head") is not None:
        mocap.level_head(cyc, spec["head"])
    loop, stride = mocap.close_loop(cyc, travel=True)
    N = len(loop) - 1
    n_nat = max(2, int(round(N * FPS / m.fps)))
    n_out = int(spec.get("frames") or n_nat)
    out = mocap.resample(loop, np.linspace(0, N, n_out + 1))
    close(out)
    if spec.get("uneven"):
        out = warp_uneven(out, spec["uneven"])
    kw = dict(h_tol=spec.get("h_tol", 0.035), v_tol=spec.get("v_tol", 0.45))
    favoured = None
    if spec.get("favour"):                  # (the hurt leg's stance hurried: centred on its middle as captured)
        mocap.ground_fit(out, spec.get("ground", 10))
        st0 = stances(out, stride, **kw)
        a0, b0 = max(st0["l"], key=lambda c_: c_[1] - c_[0])
        favoured = {"stance_l_before": [int(a0), int(b0)],
                    "stance_r_before": [[int(x), int(y)] for x, y in st0["r"]]}
        out = warp_favour(out, spec["favour"], ((a0 + b0 - 1) / 2.0 % n_out) / n_out)
    dz = mocap.ground_fit(out, spec.get("ground", 10))
    if spec.get("reach"):
        reach(out, spec["reach"])
        stride *= spec["reach"]
    if spec.get("scuff"):
        scuff(out, spec["scuff"])
    if spec.get("bounce") is not None:
        bounce(out, spec["bounce"])
    if spec.get("crouch"):
        mocap.crouch(out, *spec["crouch"])
    heel_up = tiptoe(out, spec["tiptoe"]) if spec.get("tiptoe") else 0.0
    th, mid = step_angle(out, stride, **kw)
    if spec.get("roll"):
        roll(out, spec["roll"], th, mid)
    if spec.get("arms"):
        arms(out, *spec["arms"])
    if spec.get("weary"):
        weary(out, *spec["weary"], th, mid)
    close(out)
    rep = {}
    mocap.foot_lock(out, stride, loop=True, report=rep, glide=spec.get("glide", 6), **kw)
    rep["swing_lift_max"] = clear_swing(out, stride)
    rep["heel_up_deg_max"] = heel_up
    if spec.get("heel_clear"):
        rep["heel_clear_deg_max"] = heel_clear(out, stride, spec["heel_clear"])
    if spec.get("ease_toes"):
        rep["toes_eased_deg_s"] = mocap.ease_spikes(out, bones=("ball_l", "ball_r"), limit=spec["ease_toes"], loop=True)
        hd = out.heads()
        for s in ("l", "r"):
            mocap._toe_clear(out, s, hd)
        close(out)
    info = {"take": spec["take"], "a": a + 1, "b": b + 1, "cost": round(float(cost), 4), "period": int(per),
            "stride": round(stride, 3), "frames": n_out + 1, "dz": round(float(dz), 3), "contacts": rep,
            "speed": round(stride / (n_out / FPS), 3), "frames_as_captured": n_nat + 1,
            "speed_as_captured": round(stride / spec.get("reach", 1.0) / (n_nat / FPS), 3)}
    if favoured:
        info["favoured"] = favoured
    if verbose:
        print(spec["name"], info, flush=True)
    return out, info


# ================================================================================================ the stumble
STUMBLE = dict(
    catch=(5.5, 7.0, 9.0, 13.0),     # (frames: the left toe caught from, fully, until, free again)
    check=(5.0, 7.0, 9.0, 11.5), slow=0.5,     # (the run's own time runs this much slower while he is caught)
    quick=(9.5, 13.0, 26.0, 30.0),   # (and faster after, by what brings him back into the run at `rejoin`)
    short=(9.5, 12.5, 25.5, 30.0),   # (the recovering steps shorter, by what keeps the planted feet from sliding)
    flat=(10.0, 13.0, 25.0, 29.0), flat_by=0.55,          # (and flatter: the pelvis's bounce less by this share)
    pitch=(5.5, 10.5, 13.0, 26.0), pitch_deg=40.0,
    drop=(6.0, 10.0, 14.0, 25.0), drop_m=0.08,
    fling=(6.0, 9.5, 13.0, 24.0), fling_deg=(48.0, 18.0), toe_deg=38.0)


def make_stumble(spec, rest, verbose=True):
    """A trip inside the run, keyed on the run's own loop (not a loop itself): frame 0 is the run's frame 0, the last
    frame the run's frame `rejoin`. The run's time is bent (checked while the toe is caught, quicker after), the
    recovering steps shortened, and he is carried over the ground at just the speed his planted feet go back at, so
    they do not slide; the game carries him at the run's even speed, so in place his pelvis falls behind while he is
    caught and comes up again in the quick steps. Over that: the left ankle held where the toe caught (toe down,
    dragging), the trunk pitched forward, the pelvis dropped, the arms thrown forward and out; then the feet are
    locked on the ground as they travel (mocap.foot_lock)."""
    P = dict(STUMBLE, **spec.get("stumble", {}))
    src = json.load(open(RUN_STYLES))[spec["on"]]
    R = mocap.motion_of_clip(src, rest)
    N, T, end = len(R) - 1, int(spec["frames"]), int(spec["rejoin"])
    v = float(src["stride"]) / N                              # (metres a frame at the run's speed)
    k = np.arange(T + 1, dtype=float)
    mid = 0.5 * (k[1:] + k[:-1])
    # the run's own time: sum of rate over the T steps = N + end
    dip = -P["slow"] * _pulse(mid, *P["check"])
    hump = _pulse(mid, *P["quick"])
    a = (N + end - T - dip.sum()) / hump.sum()
    rate = 1.0 + dip + a * hump
    ph = np.concatenate([[0.0], np.cumsum(rate)])
    ph[-1] = N + end
    S = mocap.resample(R, ph % N)
    S.W[-1], S.H[-1] = R.W[end], R.H[end]
    # the recovering steps' length: the body's travel = the planted feet's, back at the run's place at the end
    hd = R.heads()
    mids = {s_: float((hd[:N, rest.i("foot_" + s_), 0] - hd[:N, rest.i("thigh_" + s_), 0]).mean()) for s_ in "lr"}
    sh = _pulse(mid, *P["short"])
    b = (np.sum(rate) - T) / np.sum(rate * sh)                # sum(rate * (1 - b sh)) = T
    rk_mid = 1.0 - b * sh
    rk = 1.0 - b * _pulse(k, *P["short"])
    reach(S, rk, mids)
    X = np.concatenate([[0.0], np.cumsum(v * rate * rk_mid)])
    X[-1] = v * T
    S.H[:, 0] += X
    lat = np.array((0.0, 1.0, 0.0))
    # the quick steps flatter (played this fast the run's bounce would be a hop)
    zm = float(R.H[:N, 2].mean())
    hd = S.heads()
    ank = {s_: hd[:, rest.i("foot_" + s_)].copy() for s_ in "lr"}
    S.H[:, 2] = zm + (S.H[:, 2] - zm) * (1.0 - P["flat_by"] * _pulse(k, *P["flat"]))
    for s_ in "lr":
        mocap.two_bone_ik(S, s_, ank[s_], np.ones(T + 1), S.heads())
    # the left toe caught: the ankle held where it was, the toe down and dragging on the ground (not through it)
    w = _pulse(k, *P["catch"])
    hd = S.heads()
    ft = rest.i("foot_l")
    k0 = int(round(P["catch"][0] + 1))
    hold = np.broadcast_to(hd[k0, ft], hd[:, ft].shape).copy()
    mocap.bend(S, "foot_l", math.radians(P["toe_deg"]) * w, lat)
    for _ in range(3):
        mocap.two_bone_ik(S, "l", hold, w, S.heads())
        sh = mocap.sole_heights(S, S.heads(), "l")["l"][0]
        hold[:, 2] += (rest.sole["ground"] + 0.004 - sh) * (w > 0.5)
    # the pelvis drops (the legs keep their feet), the trunk pitches, the head a frame behind and then up
    hd = S.heads()
    ank = {s_: hd[:, rest.i("foot_" + s_)].copy() for s_ in "lr"}
    S.H[:, 2] -= P["drop_m"] * _pulse(k, *P["drop"])
    pt = math.radians(P["pitch_deg"]) * _pulse(k, *P["pitch"])
    for bn in ("spine_01", "spine_02", "chest"):
        mocap.bend(S, bn, pt / 3, lat)
    mocap.bend(S, "neck", -0.55 * math.radians(P["pitch_deg"]) * _pulse(k - 1.5, *P["pitch"]), lat)
    for s_ in "lr":
        mocap.two_bone_ik(S, s_, ank[s_], np.ones(T + 1), S.heads())
    # the arms thrown forward and out, the elbows opening
    fl = _pulse(k, *P["fling"])
    fwd = np.array((1.0, 0.0, 0.0))
    for s_, sg in (("l", 1.0), ("r", -1.0)):
        mocap.bend(S, "upperarm_" + s_, -math.radians(P["fling_deg"][0]) * fl, lat)
        mocap.bend(S, "upperarm_" + s_, sg * math.radians(P["fling_deg"][1]) * fl, fwd)
    L = mocap.to_local(S)
    for s_ in "lr":
        la = rest.i("lowerarm_" + s_)
        L[:, la] = mocap.slerp_mats(L[:, la], np.broadcast_to(np.eye(3), L[:, la].shape), 0.45 * fl)
    S.W[:] = mocap.from_local(rest, L, S.H, S.fps).W
    rep = {}
    mocap.foot_lock(S, 0.0, loop=False, report=rep, v_tol=spec.get("v_tol", 0.7), h_tol=spec.get("h_tol", 0.035))
    S.H[:, 0] -= v * k
    # the ends are the run's own frames exactly; the two frames next to each eased onto them
    plain = mocap.resample(R, ph % N)
    plain.W[-1], plain.H[-1] = R.W[end], R.H[end]
    for f, u in ((0, 1.0), (1, 0.6), (2, 0.25), (T, 1.0), (T - 1, 0.6), (T - 2, 0.25)):
        S.W[f] = mocap.slerp_mats(S.W[f], plain.W[f], np.full(len(rest.names), u))
        S.H[f] = S.H[f] * (1 - u) + plain.H[f] * u
    S.ends = (R.W[0].copy(), R.H[0].copy(), R.W[end].copy(), R.H[end].copy())
    bi = src.get("build", {})
    info = {"take": bi.get("take", "35_18"), "a": bi.get("a"), "b": bi.get("b"), "frames": T + 1, "dz": 0.0,
            "contacts": rep, "stride": round(v * T, 3), "speed": round(v * FPS, 3), "on": spec["on"],
            "rejoin_frame": end, "run_time": [round(float(x), 3) for x in ph / FPS],
            "time_rate": [round(float(rate.min()), 2), round(float(rate.max()), 2)],
            "steps_times": round(float(rk.min()), 3),
            "pelvis_behind_max": round(float(-(S.H[:, 0] - plain.H[:, 0]).min()), 3)}
    if verbose:
        print(spec["name"], {k_: v_ for k_, v_ in info.items() if k_ != "run_time"}, flush=True)
    return S, info


KEYING = ("build.stance: the frames each foot was held on the ground by the foot lock, [first, one past the last], "
          "counted on past the seam for a step that crosses it (the stumble's: on its own frames; its left [7, 10] is "
          "the caught toe held, not a step). A sprint's or a dash's foot is down 3 to 5 frames (0.10 to 0.17 s): "
          "planted on keys at 15 a second a step is left one key, and run_styles.py's plant refused the three sprints "
          "and dash_b on a soldier file (soldier_USA, in memory); so gaits.py plants those on keys at 30 a second and "
          "then keeps every other key, 15 a second as every clip the game has, the loop begun where the kept keys "
          "leave the soles least under the ground between them (a key near the middle of each stance; key_phase). On the "
          "reference man a moving foot is kept 4 cm clear of its lowest, so a ball within 3.5 cm of its lowest is a "
          "planted one. The stumble is not a loop: run_styles.make_clip closes every clip as one, so it needs its "
          "own keying (first key run_a's frame 0, last key run_a's frame at `rejoin`).")


# ================================================================================================ the pace
STEPS = {"sprint": (3.0, 4.7), "dash": (3.0, 4.7), "jog": (2.1, 3.3), "walk": (1.0, 2.7)}
PACE_HOW = ("the ground speeds the clip can be played at (the game plays it at speed / (stride / length), the planted "
            "feet then going back at the ground's speed at any of them, so what limits it is time): no slower than "
            "0.80 and no faster than 1.30 of the capture's own time (slowed film floats, hurried film jerks: a clip "
            "already played faster than captured has less room above), no slower than 0.85 of the clip's own time for "
            "a gait with both feet in the air (it stops reading as that gait), and the steps a second inside a man's "
            "range for the gait (sprint and dash 3.0 to 4.7, jog 2.1 to 3.3, walk 1.0 to 2.7); a walk, with a foot "
            "always down, may be played from 0.565 to 1.33 of its time. Set from these measures and the stills on the "
            "reference man; not watched as moving film. On a soldier file the planted foot's slide decides as well.")


def pace_of(spec, c):
    kind = spec.get("kind", "jog")
    speed = c["speed"]
    if kind == "once":                                  # (played at the run's rate: as far as the run itself goes)
        return [round(math.ceil(speed * 0.85 * 20) / 20, 2), round(math.floor(speed * 1.2 * 20) / 20, 2)]
    steps = 2.0 * FPS / (c["frames"] - 1)
    q = (c["build"]["frames_as_captured"] - 1) / (c["frames"] - 1)
    lo, hi = STEPS[kind][0] / steps, STEPS[kind][1] / steps
    if kind == "walk":
        lo, hi = max(lo, 0.565), min(hi, 1.33)
    else:
        lo, hi = max(lo, 0.80 / q, 0.85), min(hi, 1.30 / q)
    return [round(math.ceil(speed * lo * 20) / 20, 2), round(math.floor(speed * hi * 20) / 20, 2)]


# ================================================================================================ measures
def measure(m, stride, loop=True, **kw):
    """What the picture should show, in numbers: the trunk's lean, the head's pitch, the pelvis's height, the knees,
    the steps, the time in the air and the fall of the pelvis there against gravity."""
    r = m.rest
    N = len(m) - 1 if loop else len(m)
    hd = m.heads()
    d = hd[:N, r.i("neck")] - hd[:N, r.i("pelvis")]
    lean = np.degrees(np.arctan2(d[:, 0], d[:, 2]))
    hb = m.W[:N, r.i("head")][:, :, 1]
    h0 = r.R[r.i("head")][:, 1]
    head = np.degrees(np.arctan2(hb[:, 0], hb[:, 2]) - math.atan2(h0[0], h0[2]))
    out = {"lean_deg": round(float(lean.mean()), 1), "lean_range": [round(float(lean.min()), 1), round(float(lean.max()), 1)],
           "head_pitch_deg": round(float(head.mean()), 1),
           "pelvis_z": round(float(m.H[:N, 2].mean()), 3), "pelvis_rise": round(float(np.ptp(m.H[:N, 2])), 3),
           "pelvis_under_rest": round(float(r.head[r.i("pelvis")][2] - m.H[:N, 2].mean()), 3)}
    knee = {}
    for s in ("l", "r"):
        a = hd[:N, r.i("thigh_" + s)] - hd[:N, r.i("calf_" + s)]
        b = hd[:N, r.i("foot_" + s)] - hd[:N, r.i("calf_" + s)]
        c = np.sum(a * b, 1) / (np.linalg.norm(a, axis=1) * np.linalg.norm(b, axis=1))
        knee[s] = 180.0 - np.degrees(np.arccos(np.clip(c, -1, 1)))
    out["knee_bend_max"] = round(float(max(knee["l"].max(), knee["r"].max())), 1)
    if loop:
        st = stances(m, stride, **kw)
        down = np.zeros((2, N), bool)
        for k, s in enumerate("lr"):
            for a, b in st[s]:
                down[k, np.arange(a, b) % N] = True
        out["stance_share"] = {s: round(float(down[k].mean()), 3) for k, s in enumerate("lr")}
        out["stance_knee_bend"] = {s: round(float(knee[s][down[k]].mean()), 1) if down[k].any() else None
                                   for k, s in enumerate("lr")}
        out["footfalls"] = {s: sorted(round((a % N) / FPS, 3) for a, b in st[s]) for s in "lr"}
        air = ~down.any(0)
        out["air_share"] = round(float(air.mean()), 3)
        out["steps_a_second"] = round(2.0 * FPS / N, 2)
        # the pelvis's fall in the air: its second difference over the frames with both feet up, against gravity
        z = m.H[:N, 2]
        acc = (np.roll(z, -1) + np.roll(z, 1) - 2 * z) * FPS * FPS
        inner = air & np.roll(air, 1) & np.roll(air, -1)
        out["air_fall_g"] = round(float(-acc[inner].mean() / GRAVITY), 2) if inner.any() else None
    return out


# ================================================================================================ the file
def clip_of(m, info, spec):
    c = mocap.clip_data(m, info, spec)
    c["build"] = {k: v for k, v in info.items() if k != "contacts"}
    c["build"]["ik"] = {k: (v.get("ik_err_max") if k in ("l", "r") and isinstance(v, dict) else v)
                        for k, v in info.get("contacts", {}).items()}
    c["build"]["stance"] = {k: [[int(a), int(b)] for a, b in v.get("contacts", [])]
                            for k, v in info.get("contacts", {}).items() if k in ("l", "r") and isinstance(v, dict)}
    c["build"]["laid_over"] = {k: spec[k] for k in ("frames", "mirror", "uneven", "reach", "scuff", "bounce", "crouch",
                                                    "tiptoe", "roll", "arms", "weary", "head", "span", "period",
                                                    "stumble", "favour", "heel_clear", "ease_toes", "knee_side")
                            if k in spec}
    return c


def cut(spec, rest, verbose=True):
    """One gait, cut and measured: its clip in clips.json's form."""
    loop = bool(spec.get("loop"))
    ks0 = mocap.KNEE_SIDE                    # (two_bone_ik keeps a knee this near its hinge while it is cut)
    mocap.KNEE_SIDE = spec.get("knee_side", ks0)
    try:
        if spec.get("make") == "stumble":
            m, info = make_stumble(spec, rest, verbose=verbose)
        else:
            m, info = make_gait(spec, rest, verbose=verbose)
    finally:
        mocap.KNEE_SIDE = ks0
    if spec.get("knee_side"):
        info["knee_side_deg"] = spec["knee_side"]
    info["fit_arms_deg"] = mocap.fit_arms(m, loop=loop)
    info["eased_deg_s"] = mocap.ease_spikes(m, loop=loop)
    if hasattr(m, "ends"):                              # (the stumble's ends stay the run's own frames, exactly)
        m.W[0], m.H[0], m.W[-1], m.H[-1] = m.ends
    c = clip_of(m, info, dict(spec, take=info["take"]))
    if not loop:
        c["length"] = round((len(m) - 1) / FPS, 4)
        c["stride"], c["speed"] = info["stride"], info["speed"]
        c["source"] = f"keyed on run_styles_clips.json's {spec['on']} ({c['source']})"
        c["rejoin"] = [spec["on"], round(info["rejoin_frame"] / FPS, 4)]
    c["measured"] = measure(m, info["stride"], loop=loop,
                            **{k: spec[k] for k in ("h_tol", "v_tol") if k in spec and loop})
    c["pace"] = spec.get("pace") or pace_of(spec, c)
    c["pace_how"] = PACE_HOW
    return c


def build(names=None):
    import kit
    import human
    kit.fresh(mocap.FPS)
    man = human.build(seed=0, nation="GER", stand=0.02, curl=(30, 45, 25))
    rest = mocap.Rest.of(man["armature"], man["body"])
    data = json.load(open(OUT)) if os.path.exists(OUT) else {}
    for spec in STYLES:
        if names and spec["name"] not in names:
            continue
        data[spec["name"]] = c = cut(spec, rest)
        print("   ", c["measured"], "pace", c["pace"], flush=True)
    ref = json.load(open(RUN_STYLES))
    data["_reference"] = {n: dict(measure(mocap.motion_of_clip(ref[n], rest), ref[n]["stride"]), speed=ref[n]["speed"],
                                  stride=ref[n]["stride"], length=ref[n]["length"])
                          for n in ("run_a", "advance_a")}
    data["_reference"]["about"] = ("run_styles_clips.json's run_a and advance_a (the file's own run and advance) "
                                   "measured the same way, to set the gaits against")
    data["_about"] = dict(mocap.ABOUT, made="gaits_mocap.py: CMU takes cut as mocap.py cuts its own, the gaits' "
                                            "postures laid over them (each clip's note and build.laid_over)",
                          keying=KEYING)
    rr = json.load(open(REST_REF))
    data["_rest"] = {"names": rr["names"], "parent": rr["parent"], "R": rr["R"],
                     "about": "the reference man's rest (mocap_cmu/rest_ref.json): bone order (parents first), parent "
                              "names, rest turns (Blender's axes)"}
    order = [s["name"] for s in STYLES if s["name"] in data]
    data = {**{n: data[n] for n in order}, "_about": data["_about"], "_rest": data["_rest"],
            "_reference": data["_reference"]}
    with open(OUT + ".part", "w") as f:
        json.dump(data, f, separators=(",", ":"))
    os.replace(OUT + ".part", OUT)


# ================================================================================================ pictures
# (numpy and PIL only: the reference man drawn from the side, from his right, as jointed limbs from the clips' own
# bones; his left limbs pale, behind; the ground line is his soles' ground)
INK_NEAR, INK_TRUNK, INK_FAR, INK_GROUND = (38, 44, 40), (92, 104, 92), (176, 182, 170), (120, 96, 60)
PAPER = (240, 237, 230)
GROUND = 0.02
LIMBS = [("thigh_", "calf_", 0.150), ("calf_", "foot_", 0.105), ("upperarm_", "lowerarm_", 0.090),
         ("lowerarm_", "hand_", 0.072)]


def _font(size):
    from PIL import ImageFont
    try:
        return ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", size)
    except Exception:
        return None


def _joints(m):
    """What the drawing needs, per frame: every bone's head, and the heels, soles, toes, finger tips, crown, nose."""
    r = m.rest
    hd = m.heads()
    J = {n: hd[:, b] for b, n in enumerate(r.names)}

    def carried(bone, rest_pt):
        b = r.i(bone)
        return hd[:, b] + np.einsum("fij,j->fi", m.W[:, b] @ r.R[b].T, np.asarray(rest_pt) - r.head[b])
    for s in ("l", "r"):
        f, bl, hn = r.head[r.i("foot_" + s)], r.head[r.i("ball_" + s)], r.i("hand_" + s)
        J["heel_" + s] = carried("foot_" + s, (f[0] - 0.061, f[1], GROUND))
        J["sole_" + s] = carried("foot_" + s, (bl[0], bl[1], GROUND))
        J["toe_" + s] = carried("ball_" + s, (bl[0] + 0.085, bl[1], GROUND + 0.004))
        J["toetop_" + s] = carried("ball_" + s, (bl[0] + 0.075, bl[1], bl[2] - 0.004))
        J["tip_" + s] = carried("hand_" + s, r.tail[hn] + 0.6 * (r.tail[hn] - r.head[hn]))
    h = r.i("head")
    J["skull"] = carried("head", r.head[h] + np.array((0.012, 0.0, 0.095)))
    J["nose"] = carried("head", r.head[h] + np.array((0.125, 0.0, 0.085)))
    J["brow"] = carried("head", r.head[h] + np.array((0.105, 0.0, 0.125)))
    J["chin"] = carried("head", r.head[h] + np.array((0.085, 0.0, 0.015)))
    return J


def soles_low(J, s):
    return np.minimum(J["heel_" + s][:, 2], J["sole_" + s][:, 2])


def draw_man(d, J, f, ox, oy, px, dim=False):
    """One frame of the man at pixel (ox, oy) = (his root's place along the ground, the ground), px pixels a metre."""
    def P(n):
        v = J[n][f]
        return (ox + v[0] * px, oy - (v[2] - GROUND) * px)

    def limb(a, b, w, ink):
        A, B = P(a), P(b)
        wd = max(2, int(round(w * px)))
        d.line([A, B], fill=ink, width=wd)
        for c in (A, B):
            d.ellipse([c[0] - wd / 2, c[1] - wd / 2, c[0] + wd / 2, c[1] + wd / 2], fill=ink)

    def side(s, ink):
        for a, b, w in LIMBS:
            limb(a + s, b + s, w, ink)
        limb("hand_" + s, "tip_" + s, 0.055, ink)
        d.polygon([P("foot_" + s), P("heel_" + s), P("sole_" + s), P("toe_" + s), P("toetop_" + s), P("ball_" + s)], fill=ink)
        limb("foot_" + s, "heel_" + s, 0.05, ink)
    far = INK_FAR if not dim else (205, 208, 200)
    side("l", far)
    ink = INK_TRUNK if not dim else (170, 176, 168)
    for a, b, w in (("pelvis", "spine_01", 0.27), ("spine_01", "spine_02", 0.25), ("spine_02", "chest", 0.25),
                    ("chest", "neck", 0.27), ("neck", "head", 0.11)):
        limb(a, b, w, ink)
    c = P("skull")
    rad = 0.105 * px
    d.ellipse([c[0] - rad, c[1] - rad, c[0] + rad, c[1] + rad], fill=ink)
    d.polygon([P("brow"), P("nose"), P("chin")], fill=ink)
    side("r", INK_NEAR if not dim else (120, 126, 120))


def _motion(c, rest):
    m = mocap.motion_of_clip(c, rest)
    return m


def _at(m, frames):
    """The motion at fractional frames (a loop wraps)."""
    return mocap.resample(m, np.asarray(frames, float))


def strip(name, c, rest, out_dir, cells=8, px=150, extra=""):
    from PIL import Image, ImageDraw
    m = _motion(c, rest)
    loop = bool(c.get("loop"))
    n = len(m) - 1
    fr = [n * k / cells for k in range(cells)] if loop else [n * k / (cells - 1) for k in range(cells)]
    J = _joints(_at(m, fr))
    cw, ch, top = int(1.75 * px), int(2.0 * px), 46
    im = Image.new("RGB", (cw * cells, ch + top + 24), PAPER)
    d = ImageDraw.Draw(im)
    long = c["length"] if loop else (len(m) - 1) / c["fps"]
    spd = c.get("speed") or (c["stride"] / long if c.get("stride") else 0.0)
    d.text((8, 4), f"{name}: {spd:.2f} m/s, stride {c['stride']:.2f} m, {long:.3f} s"
                   f"{'' if loop else ' (not a loop)'}   {c['source']}", fill=(20, 20, 20), font=_font(17))
    d.text((8, 26), (extra or c.get("note", ""))[:230], fill=(70, 70, 70), font=_font(12))
    gy = top + ch - int(0.12 * px)
    d.line([(0, gy), (cw * cells, gy)], fill=INK_GROUND, width=2)
    for k in range(cells):
        ox = k * cw + cw // 2
        for s in ("l", "r"):
            if soles_low(J, s)[k] < GROUND + 0.02:
                x = ox + 0.5 * (J["heel_" + s][k, 0] + J["toe_" + s][k, 0]) * px
                d.line([(x - 0.13 * px, gy + 5), (x + 0.13 * px, gy + 5)], fill=(200, 60, 40), width=4)
        draw_man(d, J, k, ox, gy, px)
        d.text((k * cw + 6, gy + 10), f"{fr[k] / c['fps']:.3f} s", fill=(90, 90, 90), font=_font(12))
        if k:
            d.line([(k * cw, top), (k * cw, gy - 2)], fill=(222, 218, 208), width=1)
    os.makedirs(out_dir, exist_ok=True)
    im.save(os.path.join(out_dir, name + ".png"))
    return im


def footfall(m, s="r"):
    """The first frame of the loop at which the foot comes down (its sole within 2 cm of the ground from above)."""
    J = _joints(m)
    z = soles_low(J, s)[:len(m) - 1]
    n = len(z)
    for i in range(n):
        if z[i] < GROUND + 0.02 and z[i - 1] >= GROUND + 0.02:
            return i
    return int(np.argmin(z))


def ladder(data, run_a, rest, out_dir, order=None, px=120):
    """All the gaits and run_a side by side at the same moments: the right foot landing, and a quarter loop on."""
    from PIL import Image, ImageDraw
    names = [("run_a", run_a)] + [(n, data[n]) for n in (order or [k for k in data if not k.startswith("_")])]
    cw, ch, top = int(1.6 * px), int(2.0 * px), 34
    im = Image.new("RGB", (cw * len(names), 2 * (ch + 22) + top + 40), PAPER)
    d = ImageDraw.Draw(im)
    d.text((8, 6), "The gaits on the reference man (from his right; his left limbs pale), all at the same moments: "
                   "above, the right foot landing; below, a quarter of the loop later", fill=(20, 20, 20), font=_font(15))
    for k, (n, c) in enumerate(names):
        m = _motion(c, rest)
        loop = bool(c.get("loop"))
        N = len(m) - 1
        f0 = footfall(m, "r") if loop else N * 0.45
        frs = [f0 % N, (f0 + N / 4.0) % N] if loop else [N * 0.40, N * 0.62]
        J = _joints(_at(m, frs))
        long = c["length"] if loop else N / c["fps"]
        spd = c.get("speed") or (c["stride"] / long if c.get("stride") else 0.0)
        for row in range(2):
            gy = top + (row + 1) * (ch + 22) - int(0.1 * px) - 22
            d.line([(k * cw, gy), ((k + 1) * cw, gy)], fill=INK_GROUND, width=2)
            draw_man(d, J, row, k * cw + cw // 2, gy, px)
        y = top + 2 * (ch + 22) - 14
        d.text((k * cw + 6, y), n, fill=(20, 20, 20), font=_font(15))
        d.text((k * cw + 6, y + 19), f"{spd:.1f} m/s" + ("" if loop else " (once)"), fill=(60, 60, 60), font=_font(14))
        if k:
            d.line([(k * cw, top), (k * cw, y + 36)], fill=(222, 218, 208), width=1)
    os.makedirs(out_dir, exist_ok=True)
    im.save(os.path.join(out_dir, "ladder.png"))
    return im


def pictures(out_dir=PICS, names=None):
    data = json.load(open(OUT))
    rest = mocap.Rest.load(REST_REF)
    order = [s["name"] for s in STYLES if s["name"] in data]
    for n in order:
        if names and n not in names:
            continue
        strip(n, data[n], rest, out_dir)
    run_a = json.load(open(RUN_STYLES))["run_a"]
    strip("run_a", run_a, rest, out_dir)
    ladder(data, run_a, rest, out_dir, order)
    print("pictures in", out_dir)


def table():
    data = json.load(open(OUT))
    for n, c in data.items():
        if n.startswith("_"):
            continue
        ms = c.get("measured", {})
        print(f"{n:11s} {c['speed']:5.2f} m/s  stride {c['stride']:.3f}  {c['length']:.4f} s  {c['frames']} fr  pace {c.get('pace')}  "
              f"{c['source']}  lean {ms.get('lean_deg')} head {ms.get('head_pitch_deg')} pelvis -{ms.get('pelvis_under_rest')} "
              f"rise {ms.get('pelvis_rise')} air {ms.get('air_share')} g {ms.get('air_fall_g')} stance {ms.get('stance_share')} "
              f"knee {ms.get('stance_knee_bend')} falls {ms.get('footfalls')} ik {c['build'].get('ik')}")


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    if args and args[0] == "--pictures":
        pictures(args[1] if len(args) > 1 else PICS, args[2:] or None)
    elif args and args[0] == "--table":
        table()
    else:
        build(args or None)
