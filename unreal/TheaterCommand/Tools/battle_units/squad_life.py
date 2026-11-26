"""Release 2 squad-life clips for the AI thread (so no squad does the same thing at once), keyed straight onto each
infantry file's own skeleton, as field_clips.py keys its clips: nothing else in the file is touched.

    reload_kneel     2.6 s  one-shot  from fire_kneel's first pose: the rifle down and canted, the bolt opened, a clip
                                      from the right front pouch pressed in, the bolt closed, back on the aim
    reload_prone     2.6 s  one-shot  the same lying, from fire_prone's first pose, the rifle rolled toward him and his
                                      trunk rolled a little onto his left side so his right hand reaches the pouch
    signal_go        1.6 s  one-shot  from idle's first pose: the rifle in the right hand only, the left arm swept up
                                      from behind the shoulder and forward to point the way, then back onto the rifle
    signal_down      1.8 s  one-shot  from idle's first pose: the left arm out, palm down, pressed down twice ("get
                                      down"), back onto the rifle
    look_back_prone  2.2 s  one-shot  from fire_prone's first pose: head and shoulders turn to look back over the right
                                      shoulder (up to LOOK_TURN_PRONE of head turn from his hips, shared down the
                                      spine, never more than HEAD_MOST on his chest: each file's own figure is in its
                                      about and `head_turn_deg`), hold, return
    look_back_kneel  2.2 s  one-shot  the same from fire_kneel's first pose (up to LOOK_TURN)
    peek_cover       3.0 s  one-shot  from fire_kneel's first pose: ducks behind a wall whose face is 0.5 m before his
                                      front knee (the top of his helmet down to 0.80 m), holds, rises slowly to peek
                                      over it (eyes at 1.10 m), looks left and right, ducks again, back onto the aim;
                                      the weapon taken down behind the wall and back up over its top, clear of it
                                      (`cover` gives the wall's face and the tops the clip suits)

Every clip starts and ends on its base clip's first pose (the man's own clip in his own file), so the game chains
base -> squad-life clip -> base without a jump. The rifle stays in his hands throughout: the hand that anchors it
(the left on the fore-end while the right works the bolt or the magazine; the right on the grip while the left
signals) carries the
weapon bone exactly, the other hand is solved onto its own grip.

Which file gets what (the weapon decides the reloads):
  - bolt-action riflemen (GER, GER_detail, SOV, SOV_winter, SOV_snow, ITA, ITA_desert, JAP, CHN, POL, UK, FRA): all
    seven. The bolt is the file's own `bolt` bone moved as its own fire_kneel moves it (lift, then back), the right
    fist on the bolt handle where fire_kneel puts it;
  - magazine weapons (USA_bar's BAR, USA_loader's and USA_radio's carbines): all seven, the reloads as a magazine
    change by the right hand from the right front pouch, then a pull on the charging handle (the magazine is part of
    the weapon mesh, so it stays seated: the hand goes through the motions), the `bolt` bone untouched (the loader's
    rocket and the radio man's handset ride it: they are kept where the base clip keeps them, on his chest);
  - files that already have their own reload_kneel / reload_prone (USA and USA_para: the Garand's; USA_nco: the
    Thompson's magazine change): the five others; their own reloads are kept (--deliver never replaces a clip);
  - USA_medic (unarmed: the weapon bone rides his right hand, `weapon` none) and USA_bazooka (the launcher: the loader
    reloads it, load_bazooka): the five others.

    python3 squad_life.py --stage DIR [names...] [--jobs N]
                                                      DIR/<name>.squad_life.json (a dict of the clips), checks printed
                                                      (N files at a time)
    python3 squad_life.py --check DIR [names...]      the checks on what is staged, against the shared files
    python3 squad_life.py --pictures DIR [names...]   DIR/pictures/<name>_squad_life.png, a few frames per clip
    python3 squad_life.py --deliver DIR [names...]    (NOT RUN: add the staged clips to the shared files: each read
                                                      fresh, backed up, written whole through a temporary file and
                                                      os.replace, read back; never replaces a clip the file has)

Keys are 15 a second (field_clips.bake / times_of), rotations [t, w, x, y, z] and moves [t, x, y, z] in the parent's
frame, as every clip's.
"""
import json
import math
import os
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gore  # noqa: E402
import field_clips as fc  # noqa: E402
from field_clips import Man, R_axis, quat, frame_map, unit, sm, times_of  # noqa: E402

SHARED = "/mnt/project-files/battle-models/land/soldier/"
BACKUP = "/home/user/art/looks/backup/squad_life/"
INFANTRY = ["soldier_USA", "soldier_USA_bar", "soldier_USA_nco", "soldier_USA_medic", "soldier_USA_bazooka",
            "soldier_USA_loader", "soldier_USA_radio", "soldier_USA_para", "soldier_GER", "soldier_GER_detail",
            "soldier_SOV", "soldier_SOV_winter", "soldier_SOV_snow", "soldier_ITA", "soldier_ITA_desert", "soldier_JAP",
            "soldier_CHN", "soldier_POL", "soldier_UK", "soldier_FRA"]
# (the contract with the AI thread: names, lengths, the base clip each starts and ends on)
CLIPS = {"reload_kneel": (2.6, "fire_kneel"), "reload_prone": (2.6, "fire_prone"), "signal_go": (1.6, "idle"),
         "signal_down": (1.8, "idle"), "look_back_prone": (2.2, "fire_prone"), "look_back_kneel": (2.2, "fire_kneel"),
         "peek_cover": (3.0, "fire_kneel")}
RELOADS = ("reload_kneel", "reload_prone")
LOOK_TURN = 75.0             # (degrees of head turn, from the hips, looking back)
LOOK_SHARE = (("spine_01", 0.10), ("spine_02", 0.12), ("chest", 0.14), ("neck", 0.30), ("head", 0.34))
# (lying, more of the turn goes down the spine (his shoulders roll up off the ground): the prone aim already has his
# neck bent far back, so the neck is given less of it. A twist down a lying spine is mostly a roll, which turns his
# face less than the same twist standing, so the prone turn asked is larger: it comes to about 72 degrees of head turn
# from the hips, about 78 on the chest)
LOOK_TURN_PRONE = 100.0
LOOK_HAND_OFF = (-0.04, -0.05, 0.06)    # (unarmed, kneeling: his left hand back, out and up off his knee, from half
                                        # the turn on)
HEAD_MOST = 78.0             # (the most turn of his head on his chest)
LAUNCHER_OUT = 0.16          # (peek_cover: the launcher pushed out from his face as he ducks)
WHIP_STEADY = 0.85           # (how much of the base's upright the radio man's aerial keeps)
LOOK_SHARE_PRONE = (("spine_01", 0.16), ("spine_02", 0.18), ("chest", 0.20), ("neck", 0.21), ("head", 0.25))
DUCK_TOP = 0.80              # (the top of his helmet, ducked: low enough that the cover's top can be under his aim)
PEEK_EYE = 1.10              # (his eyes, peeking)
# (the duck, per unit: his hips sunk back onto his right heel (m), his back bent forward, his neck and head bowed (deg))
DUCK_SINK, DUCK_BEND, DUCK_NECK, DUCK_HEAD = 0.07, 34.0, 18.0, 16.0
EYE_PAINT = 46               # (eye_white: the eyes' place on the head)


# ================================================================================================ small maths
def qslerp(Ra, Rb, u):
    """The turn u of the way from Ra to Rb."""
    if u <= 0.0:
        return Ra.copy()
    if u >= 1.0:
        return Rb.copy()
    qa, qb = quat(Ra), quat(Rb)
    if qa @ qb < 0:
        qb = -qb
    d = float(np.clip(qa @ qb, -1.0, 1.0))
    th = math.acos(d)
    if th < 1e-6:
        q = qa
    else:
        q = (math.sin((1 - u) * th) * qa + math.sin(u * th) * qb) / math.sin(th)
    return gore.qmat(q / np.linalg.norm(q))


def kf(keys, t, ease=None):
    """A keyed value at t: eased (smoothstep, or `ease`) from key to key, held before the first and after the last."""
    ease = ease or sm
    if t <= keys[0][0]:
        return np.array(keys[0][1], float) if not np.isscalar(keys[0][1]) else float(keys[0][1])
    for (t0, a), (t1, b) in zip(keys, keys[1:]):
        if t <= t1:
            u = ease((t - t0) / max(t1 - t0, 1e-9))
            if np.isscalar(a):
                return float(a + (b - a) * u)
            return np.asarray(a, float) + (np.asarray(b, float) - np.asarray(a, float)) * u
    v = keys[-1][1]
    return np.array(v, float) if not np.isscalar(v) else float(v)


def trap(x, a=0.3):
    """An easing from 0 to 1 whose speed rises evenly over the first `a` of the way, holds, and falls evenly over the
    last `a` (a trapezoid of speed: its top is 1 / (1 - a) of the mean, where smoothstep's is 1.5)."""
    x = min(max(x, 0.0), 1.0)
    v = 1.0 / (1.0 - a)
    if x < a:
        return 0.5 * v * x * x / a
    if x > 1.0 - a:
        return 1.0 - 0.5 * v * (1.0 - x) ** 2 / a
    return 0.5 * v * a + v * (x - a)


def angle_of(R):
    q = quat(R)
    return math.degrees(2 * math.acos(min(1.0, abs(float(q[0])))))


def blend_frames(A, B, u):
    """Two placements (G, X) blended: the turn by slerp, the place straight."""
    return qslerp(A[0], B[0], u), A[1] * (1 - u) + B[1] * u


def about_point(G, X, R, pivot):
    """The placement (G, X) turned by the world turn R about `pivot`."""
    return R @ G, pivot + R @ (X - pivot)


# ================================================================================================ the man and his kit
class Fit:
    """One file's man: his rig (field_clips.Man), what his weapon is, and the base poses the clips start from."""

    def __init__(self, name, D):
        self.name, self.D = name, D
        self.man = man = Man(D)
        C = D["clips"]
        self.weapon = C["idle"].get("weapon", "rifle")
        self.has = [c for c in RELOADS if c in C]
        tr = {t["part_name"]: t for t in C["fire_kneel"]["tracks"]}
        bt = tr.get("bolt")
        M = D["near"]
        a = np.array(M["part"])
        bolt_pts = int((a == man.b("bolt")).sum())
        # (the bolt bone: the rifle's own bolt when fire_kneel works it; a prop on the body when it is keyed far off
        # the weapon (the loader's rocket in his bag, the radio man's handset on his strap))
        lq0, mv0 = man.locals_of(C["fire_kneel"], 0.0)
        self.bolt_prop = float(np.linalg.norm(mv0[man.b("bolt")])) > 0.05
        works_bolt = bt is not None and bt.get("rot") and any(abs(k[1]) < 0.99 for k in bt["rot"]) and bolt_pts > 0
        if self.weapon == "none":
            self.kind = "none"
        elif "bazooka" in name:
            self.kind = "launcher"
        elif works_bolt and not self.bolt_prop:
            self.kind = "bolt"
        else:
            self.kind = "mag"
        self.reloads = self.kind in ("bolt", "mag") and not self.has
        self.base = {c: Base(self, c) for c in ("idle", "fire_kneel", "fire_prone")}
        H = man.H
        # (his eyes, in the head bone's frame (as the rest pose's offset from its head))
        P = np.array(M["points"]).reshape(-1, 3)
        pa = np.array(M["paints"])
        eyes = P[(pa == EYE_PAINT) & (a == man.b("head"))]
        self.eye = (eyes.mean(0) if len(eyes) else H[man.b("head")] + np.array((0.09, 0.0, 0.04))) - H[man.b("head")]
        self.head_pts = P[a == man.b("head")] - H[man.b("head")]
        # (the right front pouch's mouth: the top of its front, on pouch_r if he has it, else on the belt)
        bones = [man.b("pouch_r")] if "pouch_r" in man.ix else [man.b("pelvis")]
        sel = np.isin(a, bones) & (P[:, 1] > 0.04) & (P[:, 1] < 0.22) & (P[:, 2] > 0.95) & (P[:, 2] < 1.16)
        if "pouch_r" not in man.ix:
            sel &= np.isin(pa, (4, 6, 9, 12))
        Q = P[sel]
        self.pouch_bone = bones[0]
        self.pouch_l_bone = man.b("pouch_l") if "pouch_l" in man.ix else man.b("pelvis")
        Ql = P[np.isin(a, [self.pouch_l_bone]) & (P[:, 1] < -0.04) & (P[:, 1] > -0.22) & (P[:, 2] > 0.95) &
               (P[:, 2] < 1.16)]
        if len(Q) == 0:
            # (a pouch bone with no mesh of its own on it (the bazooka man's): his left pouch's mirror, or the belt)
            if len(Ql) and self.pouch_l_bone != man.b("pelvis"):
                Q = Ql * np.array((1.0, -1.0, 1.0)) - H[self.pouch_l_bone] * np.array((1.0, -1.0, 1.0)) + H[bones[0]]
            else:
                sel = (a == man.b("pelvis")) & (P[:, 1] > 0.04) & (P[:, 1] < 0.22) & (P[:, 2] > 0.95) & (P[:, 2] < 1.16)
                Q = P[sel] if sel.any() else (H[bones[0]] + np.array(((0.14, 0.12, 1.02),)) - H[bones[0]])
        self.pouch = np.array((np.percentile(Q[:, 0], 85), np.median(Q[:, 1]), np.percentile(Q[:, 2], 92))) - H[bones[0]]
        if len(Ql) == 0:
            Ql = (self.pouch + H[bones[0]])[None] * np.array((1.0, -1.0, 1.0))
        self.pouch_l = (np.array((np.percentile(Ql[:, 0], 85), np.median(Ql[:, 1]), np.percentile(Ql[:, 2], 92)))
                        - H[self.pouch_l_bone])
        # (the bolt: fire_kneel's own lift and pull, and his right fist on its handle there)
        if self.kind == "bolt":
            self._bolt_from(C["fire_kneel"])
        # (the weapon's own frame: where its magazine is (under the receiver) and its sights)
        self.wpts = man.weapon_pts - H[man.b("weapon")]

    def _bolt_from(self, F):
        man = self.man
        bo, w, h = man.b("bolt"), man.b("weapon"), man.b("hand_r")
        tr = {t["part_name"]: t for t in F["tracks"]}["bolt"]
        ts = [k[0] for k in tr["rot"]]
        ang = [angle_of(gore.qmat(k[1:])) for k in tr["rot"]]
        mvx = {k[0]: k[1:] for k in tr.get("move", [])}
        back = [float(np.linalg.norm(mvx.get(t, (0, 0, 0)))) for t in ts]
        i_open = int(np.argmax(back)) if max(back) > 1e-3 else int(np.argmax(ang))
        i_up = next(i for i, a_ in enumerate(ang) if a_ > 0.9 * max(ang))
        i_start = max(i for i in range(i_up) if ang[i] < 0.5)
        self.bolt_rot = gore.qmat(tr["rot"][i_open][1:])
        self.bolt_move = np.array(mvx.get(ts[i_open], (0.0, 0.0, 0.0)), float)
        self.bolt_turn = max(ang)
        # (the right hand on the handle, in the bolt bone's frame, at the open key and as it starts to lift)
        rel = []
        for i in (i_start, i_open):
            G, X = gore.pose(man.rig, F, ts[i])
            rel.append((G[bo].T @ G[h], G[bo].T @ (X[h] - X[bo])))
        self.bolt_hand = rel[1]
        self.bolt_hand_start = rel[0]
        # (the charger guide: the top of the receiver just ahead of the bolt's face, in the weapon's frame)
        H = man.H
        self.bolt_at = H[bo] - H[w]                               # (the bolt's head on the weapon, closed)
        P = man.weapon_pts - H[w]
        near = P[(np.abs(P[:, 0] - self.bolt_at[0] - 0.04) < 0.03) & (np.abs(P[:, 1]) < 0.02)]
        top = float(near[:, 2].max()) if len(near) else self.bolt_at[2] + 0.02
        self.charger = np.array((self.bolt_at[0] + 0.035, 0.0, top + 0.035))

    def bolt_local(self, b):
        """The bolt's turn and move on the weapon, b from 0 (home) to 1 (open): lifted first, then drawn back."""
        u1 = sm(min(b / 0.5, 1.0))
        u2 = sm(max((b - 0.5) / 0.5, 0.0))
        return qslerp(np.eye(3), self.bolt_rot, u1), self.bolt_move * u2


class Base:
    """A base clip's first pose and what holds in it: the rifle on the chest, each hand on the rifle, each arm's lie."""

    def __init__(self, fit, clip):
        man = fit.man
        self.clip = clip
        C = fit.D["clips"][clip]
        self.lq, self.mv = man.locals_of(C, 0.0)
        self.G, self.X = G, X = man.fk(self.lq, self.mv)
        c, w = man.b("chest"), man.b("weapon")
        self.Gc, self.Xc = G[c], X[c]
        self.Gw, self.Xw = G[w], X[w]
        self.hand, self.hand_w, self.arm_ref = {}, {}, {}
        for s in ("l", "r"):
            ua, la, ha = man.b("upperarm_" + s), man.b("lowerarm_" + s), man.b("hand_" + s)
            self.hand[s] = (G[ha], X[ha])
            self.hand_w[s] = (G[w].T @ G[ha], G[w].T @ (X[ha] - X[w]))
            mid = 0.5 * (X[ua] + X[ha])
            pole = unit(X[la] - mid)
            self.arm_ref[s] = (G[c].T @ G[ua], G[c].T @ G[la], G[c].T @ G[ha], G[c].T @ pole,
                               G[c].T @ (X[ha] - X[c]))
        bo = man.b("bolt")
        self.bolt_chest = (G[c].T @ G[bo], G[c].T @ (X[bo] - X[c]))
        self.leg_ref = {}
        for s in ("l", "r"):
            th, ca, fo = man.b("thigh_" + s), man.b("calf_" + s), man.b("foot_" + s)
            mid = 0.5 * (X[th] + X[fo])
            self.leg_ref[s] = (G[th], G[ca], G[fo], X[fo], unit(X[ca] - mid))

    def weapon_on_chest(self):
        return self.Gc.T @ self.Gw, self.Gc.T @ (self.Xw - self.Xc)


# ================================================================================================ posing
def bend(man, lq, bone, axis, deg):
    """A turn of `bone` about one of its own axes (its rest axes: x his forward, y his right, z up)."""
    if abs(deg) > 1e-9:
        lq[man.b(bone)] = lq[man.b(bone)] @ R_axis(axis, deg)
    return lq


def trunk_about_hips(man, lq, mv, R):
    """The pelvis turned by the world turn R about the middle of his hips, his legs kept as they lie."""
    G, X = man.fk(lq, mv)
    pe = man.b("pelvis")
    hips = 0.5 * (X[man.b("thigh_l")] + X[man.b("thigh_r")])
    Gp = R @ G[pe]
    Xp = hips + R @ (X[pe] - hips)
    par = man.par[pe]
    Gpar = G[par] if par is not None else np.eye(3)
    Xpar = X[par] if par is not None else np.zeros(3)
    lq[pe] = Gpar.T @ Gp
    mv[pe] = Gpar.T @ (Xp - Xpar) - (man.H[pe] - (man.H[par] if par is not None else 0.0))
    for s in ("thigh_l", "thigh_r"):
        lq[man.b(s)] = Gp.T @ G[man.b(s)]
    return lq, mv


def reach(fit, lq, mv, s, T, Gh, ref, pole=None, share=0.5):
    """His arm `s` solved so his wrist is at T and his hand turned to Gh, the upper arm and forearm turned the least
    from how they lie in the base pose `ref` (on his chest), so a target equal to the base's gives the base's arm."""
    man = fit.man
    A = man.side[s]
    G, X = man.fk(lq, mv)
    ua, la, ha = man.b("upperarm_" + s), man.b("lowerarm_" + s), man.b("hand_" + s)
    Gc = G[man.b("chest")]
    Ru, Rl, Rh, pol, _ = ref.arm_ref[s]
    Gu_ref, Gl_ref = Gc @ Ru, Gc @ Rl
    up_ref, lo_ref = Gu_ref @ A["u0"], Gl_ref @ A["l0"]
    bend_ref = unit(np.cross(up_ref, lo_ref))
    S = X[ua]
    l1, l2 = A["l1"], A["l2"]
    dv = np.asarray(T, float) - S
    dist = float(np.linalg.norm(dv))
    dc = min(max(dist, abs(l1 - l2) + 1e-4), (l1 + l2) * 0.9995)
    u = dv / max(dist, 1e-9)
    pole = Gc @ pol if pole is None else np.asarray(pole, float)
    v = pole - u * (pole @ u)
    if np.linalg.norm(v) < 1e-6:
        v = np.cross(u, bend_ref)
    v = unit(v)
    ca = (l1 * l1 + dc * dc - l2 * l2) / (2 * l1 * dc)
    K = S + l1 * (u * ca + v * math.sqrt(max(0.0, 1 - ca * ca)))
    Tc = S + u * dc
    up, lo = K - S, Tc - K
    bd = np.cross(up, lo)
    bd = unit(bd) if np.linalg.norm(bd) > 1e-7 else bend_ref
    Gu = frame_map(up_ref, bend_ref, up, bd) @ Gu_ref
    Gl = frame_map(lo_ref, bend_ref, lo, bd) @ Gl_ref
    # (the forearm takes `share` of the hand's turn about it, against the base's hand-on-forearm)
    la_ = unit(lo)
    pred = Gl @ (Gl_ref.T @ (Gc @ Rh)) @ A["t0"]
    want = Gh @ A["t0"]
    pred, want = pred - la_ * (pred @ la_), want - la_ * (want @ la_)
    if np.linalg.norm(pred) > 1e-6 and np.linalg.norm(want) > 1e-6:
        pred, want = unit(pred), unit(want)
        phi = math.degrees(math.atan2(np.cross(pred, want) @ la_, pred @ want))
        Gl = R_axis(la_, share * phi) @ Gl
    lq[ua] = G[man.par[ua]].T @ Gu
    lq[la] = Gu.T @ Gl
    lq[ha] = Gl.T @ Gh
    return lq


def plant_leg(fit, lq, mv, s, ref):
    """His leg `s` solved so its foot keeps the base pose's place and turn (the hip having moved)."""
    man = fit.man
    G, X = man.fk(lq, mv)
    th, ca, fo = man.b("thigh_" + s), man.b("calf_" + s), man.b("foot_" + s)
    Gt0, Gk0, Gf0, T, pole = ref.leg_ref[s]
    H = man.H
    u0, l0 = H[ca] - H[th], H[fo] - H[ca]
    l1, l2 = float(np.linalg.norm(u0)), float(np.linalg.norm(l0))
    up_ref, lo_ref = Gt0 @ u0, Gk0 @ l0
    bend_ref = unit(np.cross(up_ref, lo_ref))
    S = X[th]
    dv = T - S
    dist = float(np.linalg.norm(dv))
    dc = min(max(dist, abs(l1 - l2) + 1e-4), (l1 + l2) * 0.9995)
    u = dv / max(dist, 1e-9)
    v = unit(pole - u * (pole @ u))
    c_ = (l1 * l1 + dc * dc - l2 * l2) / (2 * l1 * dc)
    K = S + l1 * (u * c_ + v * math.sqrt(max(0.0, 1 - c_ * c_)))
    up, lo = K - S, S + u * dc - K
    bd = unit(np.cross(up, lo))
    Gt = frame_map(up_ref, bend_ref, up, bd) @ Gt0
    Gk = frame_map(lo_ref, bend_ref, lo, bd) @ Gk0
    lq[th] = G[man.par[th]].T @ Gt
    lq[ca] = Gt.T @ Gk
    lq[fo] = Gk.T @ Gf0
    return lq


def prone_roll(fit, lq, mv, ref, deg, back=None):
    """Lying: his hips rolled onto his left side by `deg` (his right hip up), about his left hip joint (onto his right
    side about the right hip if `deg` is negative), the spine
    turning `back` of it back again so his shoulders stay on his elbows; his feet kept where they lie (the right knee
    drawn up a little), and the hips let down so they rest on the ground as in the base."""
    if abs(deg) < 1e-6:
        return lq, mv
    back = ROLL_BACK if back is None else back
    man = fit.man
    G, X = man.fk(lq, mv)
    pe = man.b("pelvis")
    piv = X[man.b("thigh_l" if deg > 0 else "thigh_r")]
    ax = unit(G[pe] @ np.array((0.0, 0.0, 1.0)))
    R = R_axis(ax, deg)
    Gp, Xp = about_point(G[pe], X[pe], R, piv)
    par = man.par[pe]
    lq[pe] = G[par].T @ Gp
    mv[pe] = G[par].T @ (Xp - X[par]) - (man.H[pe] - man.H[par])
    for s_ in ("thigh_l", "thigh_r"):
        lq[man.b(s_)] = Gp.T @ G[man.b(s_)]
    for b, f in (("spine_01", 0.4), ("spine_02", 0.35), ("chest", 0.25)):
        bend(man, lq, b, (0, 0, 1), -deg * back * f)
    for s_ in ("l", "r"):
        lq = plant_leg(fit, lq, mv, s_, ref)
    # (down onto the ground again: the hips' lowest point where the base has it)
    P, a, b_, w, Z = man.body_points()
    if getattr(fit, "_hips_low", None) is None:
        Q = gore.skin(P, a, b_, w, ref.G, ref.X, man.H)
        fit._hips_low = float(Q[Z["hips"], 2].min())
    Q = gore.skin(P, a, b_, w, *man.fk(lq, mv), man.H)
    dz = fit._hips_low - float(Q[Z["hips"], 2].min())
    # (nor his belly and chest lower than the base has them: the roll can press the left of his belly down)
    Ps, Fs, as_, bs, ws, Hs, subs, lows, legs_ = body_split(fit)
    c = lows["chest"]
    if getattr(fit, "_chest_low", None) is None:
        fit._chest_low = float(gore.skin(Ps[c], as_[c], bs[c], ws[c], ref.G, ref.X, man.H)[:, 2].min())
    zc = float(gore.skin(Ps[c], as_[c], bs[c], ws[c], *man.fk(lq, mv), man.H)[:, 2].min())
    dz = max(dz, fit._chest_low - zc)
    # (nor his knees, nor any of his trunk and pouches, pressed further into the ground than the base has them: the
    # roll onto his left side would put his left knee and pouch down into it, so his hips are lifted that much more,
    # the legs planted again each time)
    if getattr(fit, "_prone_lows", None) is None:
        fit._prone_lows = lows_of(fit, ref.G, ref.X)
    gain, dk0, lifted = 2.0, None, -dz
    for _ in range(8):
        G2, X2 = man.fk(lq, mv)
        mv[pe] = mv[pe] + G2[par].T @ np.array((0.0, 0.0, dz))
        lifted += dz
        for s_ in ("l", "r"):
            lq = plant_leg(fit, lq, mv, s_, ref)
        lw = lows_of(fit, *man.fk(lq, mv))
        dk = fit._prone_lows["knees"] - ROLL_SLACK - lw["knees"]
        dt = fit._prone_lows["torso"] - ROLL_SLACK - lw["torso"]
        # (a knee, far from the hips with its foot planted, comes up about half as far as the hips are lifted: the
        # first guess, then as far as it was seen to; let down again as far as he was lifted too far, never below
        # where his hips and chest rest)
        if dk0 is not None and abs(dz) > 1e-4 and (dk0 - dk) / dz > 0.2:
            gain = dz / (dk0 - dk)
        dk0 = dk
        dz = max(gain * dk, dt, -lifted)
        if abs(dz) < 0.001:
            break
    return lq, mv


ROLL_SLACK = 0.0             # (rolled, his knees and trunk may go this much lower than the base has them)
ROLL_BACK = 0.6              # (rolled, the share of the roll his spine turns back again)


def lows_of(fit, G, X, keys=("knees", "torso")):
    """The lowest point (z, m) of his knees and of his trunk with its pouches, on the near mesh."""
    S = solid_of(fit)
    out = {}
    for k in keys:
        m = S[k]
        out[k] = float(gore.skin(S["P"][m], S["a"][m], S["b"][m], S["w"][m], G, X, fit.man.H)[:, 2].min())
    return out


def trunk_floor(fit, lq, mv, ref, slack=0.002):
    """Lying: his trunk and pouches no lower in the ground than the base has them: his hips let up as far as a turn
    of his trunk would press his belly down, the legs planted again."""
    man = fit.man
    if getattr(fit, "_prone_lows", None) is None:
        fit._prone_lows = lows_of(fit, ref.G, ref.X)
    pe = man.b("pelvis")
    par = man.par[pe]
    for _ in range(3):
        dz = fit._prone_lows["torso"] - slack - lows_of(fit, *man.fk(lq, mv), keys=("torso",))["torso"]
        if dz <= 1e-4:
            break
        G, X = man.fk(lq, mv)
        mv[pe] = mv[pe] + G[par].T @ np.array((0.0, 0.0, dz + 1e-3))
        for s_ in ("l", "r"):
            lq = plant_leg(fit, lq, mv, s_, ref)
    return lq, mv


def chest_floor(fit, lq, mv, ref, slack=0.005):
    """Lying: his chest (spine_02 and chest) kept off the ground: no lower than the base has it, his back arched up a
    little (about his waist) when a roll or turn would press it down."""
    man = fit.man
    P, F, a, b, w, H, sub, low, legs = body_split(fit)
    Pc, ac, bc, wc = P[low["chest"]], a[low["chest"]], b[low["chest"]], w[low["chest"]]
    if getattr(fit, "_chest_low", None) is None:
        fit._chest_low = float(gore.skin(Pc, ac, bc, wc, ref.G, ref.X, man.H)[:, 2].min())
    floor = fit._chest_low - slack

    def low_at(deg):
        q = lq.copy()
        bend(man, q, "spine_01", (0, 1, 0), -deg * 0.5)
        bend(man, q, "spine_02", (0, 1, 0), -deg * 0.5)
        return float(gore.skin(Pc, ac, bc, wc, *man.fk(q, mv), man.H)[:, 2].min()), q
    z0, _ = low_at(0.0)
    if z0 >= floor:
        return lq
    lo, hi = 0.0, 14.0
    for _ in range(7):
        mid = 0.5 * (lo + hi)
        if low_at(mid)[0] >= floor:
            hi = mid
        else:
            lo = mid
    return low_at(hi)[1]


CHEST_SETTLE = 0.02          # (lying and rolled, his chest let down to no more than this over where the base has it,
CHEST_REST = 0.02            # or to this over the ground, if that is higher: resting on it)
SETTLE_MOST = 14.0           # (the most his back is bent forward at the waist to let it down, degrees)


def settle_bend(fit, lq, mv, ref, slack=None, most=None):
    """Lying: the forward bend of his back at the waist (degrees; spine_01 and spine_02 half each) that lets his chest
    down again when a roll onto his side has lifted it, so it rests no more than `slack` over where the base has it
    (or CHEST_REST over the ground, if that is higher); never so far that it, or his trunk and pouches, go lower in the ground
    than the base has them. The roll turns him about his left hip and his hips are lifted to keep his left knee out
    of the ground, which would otherwise hold his chest up off the ground."""
    man = fit.man
    P, F, a, b, w, H, sub, low, legs = body_split(fit)
    c = low["chest"]
    Pc, ac, bc, wc = P[c], a[c], b[c], w[c]
    if getattr(fit, "_chest_low", None) is None:
        fit._chest_low = float(gore.skin(Pc, ac, bc, wc, ref.G, ref.X, man.H)[:, 2].min())
    if getattr(fit, "_prone_lows", None) is None:
        fit._prone_lows = lows_of(fit, ref.G, ref.X)
    slack = CHEST_SETTLE if slack is None else slack
    most = SETTLE_MOST if most is None else most
    want = max(fit._chest_low + slack, CHEST_REST)

    def at(deg):
        q = lq.copy()
        bend(man, q, "spine_01", (0, 1, 0), deg * 0.5)
        bend(man, q, "spine_02", (0, 1, 0), deg * 0.5)
        G, X = man.fk(q, mv)
        zc = float(gore.skin(Pc, ac, bc, wc, G, X, man.H)[:, 2].min())
        zt = lows_of(fit, G, X, keys=("torso",))["torso"]
        return zc, zt

    def ok(r):
        return r[0] >= min(want, fit._chest_low) - 1e-4 and r[1] >= fit._prone_lows["torso"] - 0.002
    if at(0.0)[0] <= want:
        return 0.0
    # (the most bend that keeps his chest no lower than where it settles, and his trunk out of the ground)
    lo, hi = 0.0, most
    r = at(most)
    if ok(r) and r[0] >= want:
        return most
    for _ in range(9):
        mid = 0.5 * (lo + hi)
        rm = at(mid)
        if ok(rm) and rm[0] >= want:
            lo = mid
        else:
            hi = mid
    return lo


def chest_settle(fit, lq, mv, ref, base, share):
    """Lying and rolled `share` of the way onto his side: his back bent forward at the waist that share of the bend
    that settles his chest at the full roll (settle_bend), and never more than settles it now, so the bend comes
    and goes with the roll instead of all at once when his chest first lifts."""
    man = fit.man
    full = getattr(fit, "_settle_full", None)
    if full is None:
        full = fit._settle_full = {}
    if base not in full:
        q, m = prone_roll(fit, ref.lq.copy(), ref.mv.copy(), ref, STANCE[base]["roll"])
        q = chest_floor(fit, q, m, ref)
        full[base] = settle_bend(fit, q, m, ref)
    if share <= 1e-6 or full[base] <= 1e-6:
        return lq
    deg = min(share * full[base], settle_bend(fit, lq, mv, ref))
    if deg <= 1e-6:
        return lq
    lq = lq.copy()
    bend(man, lq, "spine_01", (0, 1, 0), deg * 0.5)
    bend(man, lq, "spine_02", (0, 1, 0), deg * 0.5)
    return lq


def kneel_hip(fit, lq, mv, ref, rise):
    """Kneeling: his hips raised by `rise` (lowered if negative) as his right knee and shin stay on the ground (the
    hips swing on the thigh about the knee: forward as they rise, back over his heel as they sink), his left foot
    planted."""
    if abs(rise) < 1e-9:
        return lq, mv
    man = fit.man
    G, X = man.fk(lq, mv)
    pe, th, ca = man.b("pelvis"), man.b("thigh_r"), man.b("calf_r")
    K = X[ca]
    L = float(np.linalg.norm(man.H[ca] - man.H[th]))
    hip = X[th]
    z = hip[2] + rise
    dy = hip[1] - K[1]
    dz = z - K[2]
    hx = math.sqrt(max(L * L - dy * dy - dz * dz, 1e-6))
    sign = 1.0 if hip[0] >= K[0] else -1.0
    new = np.array((K[0] + sign * hx, hip[1], z))
    par = man.par[pe]
    mv[pe] = mv[pe] + G[par].T @ (new - hip)
    # (the right thigh turned onto the knee, the shin and foot kept as they lie)
    G2, X2 = man.fk(lq, mv)
    Gt0, Gk0, Gf0 = G[th], G[ca], G[man.b("foot_r")]
    d0, d1 = unit(K - hip), unit(K - X2[th])
    ax = np.cross(d0, d1)
    ang = math.degrees(math.atan2(np.linalg.norm(ax), d0 @ d1))
    Rm = R_axis(ax, ang) if np.linalg.norm(ax) > 1e-9 else np.eye(3)
    Gt = Rm @ Gt0
    lq[th] = G2[pe].T @ Gt
    lq[ca] = Gt.T @ Gk0
    lq[man.b("foot_r")] = Gk0.T @ Gf0
    lq = plant_leg(fit, lq, mv, "l", ref)
    return lq, mv


def look(fit, lq, mv, target, w, neck_share=0.45, most=70.0):
    """His head turned (neck and head sharing it) so his eyes look at `target`, w of the way."""
    if w <= 1e-6:
        return lq
    man = fit.man
    G, X = man.fk(lq, mv)
    h = man.b("head")
    eye = X[h] + G[h] @ fit.eye
    fwd = G[h] @ np.array((1.0, 0.0, 0.0))
    want = unit(np.asarray(target, float) - eye)
    ax = np.cross(fwd, want)
    ang = math.degrees(math.atan2(np.linalg.norm(ax), fwd @ want))
    if np.linalg.norm(ax) < 1e-9:
        return lq
    ang = min(ang, most) * w
    ax = unit(ax)
    n = man.b("neck")
    for b, f in ((n, neck_share), (h, 1.0 - neck_share)):
        Gb = G[b]
        lq[b] = lq[b] @ R_axis(Gb.T @ ax, ang * f)
        G, X = man.fk(lq, mv)
    return lq


def head_turn(man, lq, deg, share=LOOK_SHARE):
    for b, f in share:
        lq[man.b(b)] = lq[man.b(b)] @ R_axis((0, 0, 1), deg * f)
    return lq


def set_weapon(man, lq, mv, Gw, Xw):
    G, X = man.fk(lq, mv)
    wi = man.b("weapon")
    p = man.par[wi]
    lq[wi] = G[p].T @ Gw
    mv[wi] = G[p].T @ (Xw - X[p]) - (man.H[wi] - man.H[p])
    return lq, mv


def hold(fit, lq, mv, Wd, hands, ref, anchor="r", poles=None, shares=None):
    """The rifle wanted at Wd = (G, X); `hands` gives each hand's (G, X) on the rifle's frame (None: that hand is
    placed elsewhere). The anchor hand is solved onto it and the rifle then rides that hand exactly; the other hand is
    solved onto its grip. Returns lq, mv and the rifle's placement."""
    man = fit.man
    poles = poles or {}
    shares = shares or {}
    Gw, Xw = Wd
    order = [anchor] + [s for s in ("l", "r") if s != anchor]
    for k, s in enumerate(order):
        if hands.get(s) is None:
            continue
        Rh, ph = hands[s]
        Gh_t, Xh_t = Gw @ Rh, Xw + Gw @ ph
        lq = reach(fit, lq, mv, s, Xh_t, Gh_t, ref, poles.get(s), shares.get(s, 0.5))
        if k == 0:
            G, X = man.fk(lq, mv)
            h = man.b("hand_" + s)
            # (the rifle rides the anchor hand as it lies, so it never floats off it)
            Gw = G[h] @ Rh.T
            Xw = X[h] - Gw @ ph
    lq, mv = set_weapon(man, lq, mv, Gw, Xw)
    return lq, mv, (Gw, Xw)


def keep_bolt(fit, lq, mv, ref):
    """A prop on the bolt bone (the loader's rocket, the radio man's handset) kept on his chest as in the base."""
    man = fit.man
    if not fit.bolt_prop:
        return lq, mv
    G, X = man.fk(lq, mv)
    c, w, bo = man.b("chest"), man.b("weapon"), man.b("bolt")
    Rb, pb = ref.bolt_chest
    Gb, Xb = G[c] @ Rb, X[c] + G[c] @ pb
    lq[bo] = G[w].T @ Gb
    mv[bo] = G[w].T @ (Xb - X[w]) - (man.H[bo] - man.H[w])
    return lq, mv


def fingers_set(man, lq, s, curl, thumb):
    A = man.side[s]
    lq[man.b("fingers_" + s)] = R_axis(np.cross(A["d0"], A["n0"]), curl)
    lq[man.b("thumb_" + s)] = R_axis(np.cross(A["thumb0"], A["n0"]), thumb)
    return lq


def fingers_mix(man, lq, ref, s, curl, thumb, u):
    """The fingers from the base's (u = 0) to the given curl (u = 1)."""
    if u <= 1e-6:
        return lq
    tmp = fingers_set(man, lq.copy(), s, curl, thumb)
    for b in ("fingers_" + s, "thumb_" + s):
        i = man.b(b)
        lq[i] = qslerp(ref.lq[i], tmp[i], u)
    return lq


# ================================================================================================ the clips
def hand_frame(man, s, d, t):
    A = man.side[s]
    return frame_map(A["d0"], A["t0"], np.asarray(d, float), np.asarray(t, float))


def low_rifle(ref, pivot, pitch, yaw, cant, shift):
    """The base rifle turned about `pivot` (muzzle up by pitch, to his left by yaw, world degrees), canted about its own
    length through the pivot (its right side up by cant), and moved by `shift` (world)."""
    R = R_axis((0, 0, 1), -yaw) @ R_axis((0, 1, 0), -pitch)
    G1, X1 = about_point(ref.Gw, ref.Xw, R, pivot)
    ax = G1 @ np.array((1.0, 0.0, 0.0))
    G2, X2 = about_point(G1, X1, R_axis(ax, cant), pivot)
    return G2, X2 + np.asarray(shift, float)


def hand_dn(man, s, d, n):
    """A hand frame with its fingers along d and its palm facing n (world)."""
    A = man.side[s]
    return frame_map(A["d0"], A["n0"], np.asarray(d, float), np.asarray(n, float))


def wrist_at(man, s, Gh, grip_at, along=0.075, palm=0.022):
    """Where his wrist goes for the fist (turned Gh) to close on `grip_at` (as field_clips' Man.grip reads it)."""
    A = man.side[s]
    return np.asarray(grip_at, float) - Gh @ (A["d0"] * along + A["n0"] * palm)


def fist_point(man, G, X, s, along=0.075, palm=0.022):
    A = man.side[s]
    h = man.b("hand_" + s)
    return X[h] + G[h] @ (A["d0"] * along + A["n0"] * palm)


def on_rifle(W, rel):
    """A hand's (G, X) held in the rifle's frame, placed with the rifle at W."""
    Gw, Xw = W
    return Gw @ rel[0], Xw + Gw @ rel[1]


def rel_blend(a, b, u):
    return qslerp(a[0], b[0], u), a[1] * (1 - u) + b[1] * u


def path(keys, t):
    """A hand's (G, X) along keyed places: keys [(time, place, arc[, free])], place a (G, X) already worked out for
    this t, arc a world offset bowed in on the way into that key (None: straight). The places are passed through on a
    smooth (Hermite) curve that stops only at the first and last keys and where the hand holds (two keys at one place),
    so the hand never darts from stop to stop. A `free` key is a way point not tied to a beat: its time is set by the
    lengths of the legs either side (paced_times), and, if `free` is True rather than "time", its turn too, evenly
    between the fixed keys. The hand turns at the pace it moves."""
    if t <= keys[0][0]:
        return keys[0][1]
    if t >= keys[-1][0]:
        return keys[-1][1]
    n = len(keys)
    Ts = [k[0] for k in keys]
    Ps = [np.asarray(k[1][1], float) for k in keys]
    free = [len(k) > 3 and k[3] for k in keys]
    # (a True way point's turn is spread evenly over the time between the fixed keys, so only a "time" one's turn,
    # fixed, counts in its legs' times)
    Ts = paced_times(Ts, Ps, free, None if any(f is True for f in free) else [k[1][0] for k in keys])
    # (a paced way point's turn is put where its time falls between the fixed keys either side, so the hand turns
    # evenly over the whole way between them, not all at once on a short last leg)
    Gs = [k[1][0] for k in keys]
    if any(free):
        a_ = 0
        while a_ < n - 1:
            b_ = a_ + 1
            while b_ < n - 1 and free[b_]:
                b_ += 1
            for k in range(a_ + 1, b_):
                if keys[k][3] is True:
                    Gs[k] = qslerp(Gs[a_], Gs[b_], (Ts[k] - Ts[a_]) / max(Ts[b_] - Ts[a_], 1e-9))
            a_ = b_
    i = max(j for j in range(n - 1) if Ts[j] <= t)
    t0, t1 = Ts[i], Ts[i + 1]

    def tangent(j):
        if j == 0 or j == n - 1:
            return np.zeros(3)
        if np.linalg.norm(Ps[j + 1] - Ps[j]) < 1e-3 or np.linalg.norm(Ps[j] - Ps[j - 1]) < 1e-3:
            return np.zeros(3)
        if free[j]:
            # (through a paced way point at the mean of the speeds either side, so the pace does not dip there
            # where the way turns)
            v = 0.5 * (np.linalg.norm(Ps[j + 1] - Ps[j]) / max(Ts[j + 1] - Ts[j], 1e-6) +
                       np.linalg.norm(Ps[j] - Ps[j - 1]) / max(Ts[j] - Ts[j - 1], 1e-6))
            return unit(Ps[j + 1] - Ps[j - 1]) * v
        return (Ps[j + 1] - Ps[j - 1]) / (Ts[j + 1] - Ts[j - 1]) * 0.8
    s_ = (t - t0) / max(t1 - t0, 1e-9)
    h00, h10 = 2 * s_ ** 3 - 3 * s_ ** 2 + 1, s_ ** 3 - 2 * s_ ** 2 + s_
    h01, h11 = -2 * s_ ** 3 + 3 * s_ ** 2, s_ ** 3 - s_ ** 2
    dt = t1 - t0
    m0, m1 = tangent(i), tangent(i + 1)
    X = h00 * Ps[i] + h10 * dt * m0 + h01 * Ps[i + 1] + h11 * dt * m1
    arc = keys[i + 1][2]
    if arc is not None:
        X = X + np.asarray(arc, float) * math.sin(math.pi * s_) ** 2
    # (the hand turns at the pace it moves: through a way point it keeps turning, as it keeps going, rather than
    # stopping its turn at every key)
    ch = Ps[i + 1] - Ps[i]
    c2 = float(ch @ ch)
    if c2 > 1e-6 and (np.any(m0) or np.any(m1)):
        a0 = min(max(float(m0 @ ch) * dt / c2, 0.0), 3.0)
        a1 = min(max(float(m1 @ ch) * dt / c2, 0.0), 3.0)
        u = min(max(h10 * a0 + h01 + h11 * a1, 0.0), 1.0)
    else:
        u = sm(s_)
    G = qslerp(Gs[i], Gs[i + 1], u)
    return G, X


PACE_STOP = 1.3              # (a leg that starts or ends at a stop is given this much more time for its length)
PACE_TURN = 0.10             # (and a leg's turn counted as this much more length a radian: about the fist's reach
#                              from the wrist, so a leg on which the hand turns over is given the time its fingers
#                              need)


def turn_of(Ra, Rb):
    return math.acos(min(max((float(np.trace(Ra.T @ Rb)) - 1.0) / 2.0, -1.0), 1.0))


def paced_times(Ts, Ps, free, Gs=None):
    """The times of the `free` keys (ways through, not tied to a beat) put between the fixed keys either side of them
    by the length of each leg (and its turn, PACE_TURN, given the turns Gs), so the hand goes at one even pace from
    fixed key to fixed key (a leg from or to a stop given PACE_STOP more for its length, as it is eased)."""
    if not any(free):
        return Ts
    Ts = list(Ts)
    n = len(Ts)
    i = 0
    while i < n - 1:
        j = i + 1
        while j < n - 1 and free[j]:
            j += 1
        if j > i + 1:
            w = []
            for k in range(i, j):
                d = float(np.linalg.norm(Ps[k + 1] - Ps[k]))
                if Gs is not None:
                    d += PACE_TURN * turn_of(Gs[k], Gs[k + 1])
                stop = (k == i and (i == 0 or np.linalg.norm(Ps[i] - Ps[i - 1]) < 1e-3)) or \
                    (k + 1 == j and (j == n - 1 or np.linalg.norm(Ps[j + 1] - Ps[j]) < 1e-3))
                w.append(d * (PACE_STOP if stop else 1.0) + 1e-4)
            c = np.cumsum(w) / sum(w)
            for k in range(i + 1, j):
                Ts[k] = Ts[i] + (Ts[j] - Ts[i]) * float(c[k - i - 1])
        i = j
    return Ts


def world_of(G, X, bone_i, p):
    return X[bone_i] + G[bone_i] @ p


def pouch_place(fit, G, X, s, side="r", pinch_r=None):
    """His fist pinching at the mouth of his right (left) front pouch: fingers down into it, the palm toward him."""
    man = fit.man
    if side == "r":
        b, p = fit.pouch_bone, fit.pouch
    else:
        b, p = fit.pouch_l_bone, fit.pouch_l
    at = X[b] + G[b] @ p
    # (the hand turned to his hips, not to the pouch bone: a file's kneel may carry the left pouch round on his
    # raised thigh)
    Gp = G[man.b("pelvis")]
    out = 1.0 if side == "r" else -1.0
    if side == "r":
        fd, fn, pinch = POUCH_FINGERS, POUCH_PALM, pinch_r or POUCH_PINCH
    else:
        # (the left fist at the outer corner of its pouch: the left pouch rides under his belly as he kneels)
        fd, fn, pinch = (0.30, 0.10, -1.0), (-1.0, 0.25, 0.15), (0.06, 0.06, 0.04)
    d = Gp @ unit((fd[0], fd[1] * out, fd[2]))
    n = Gp @ unit((fn[0], fn[1] * out, fn[2]))
    Gh = hand_dn(man, s, d, n)
    return Gh, wrist_at(man, s, Gh, at + Gp @ np.array((pinch[0], pinch[1] * out, pinch[2])), along=0.085,
                        palm=0.018), at


POUCH_FINGERS = (0.15, 0.10, -1.0)   # (his right fist at the pouch, in his hips' frame: the fingers down into it, the
POUCH_PALM = (0.0, -1.0, 0.0)        # palm in toward his middle, as a hand hangs (so his forearm is hardly twisted),
POUCH_PINCH = (0.022, 0.018, 0.0)    # the fist's middle this far before, out from and over the pouch's mouth, so the
#                                      fingertips are at its lid)
POUCH_PINCH_OFF = (0.045, 0.03, 0.0)  # (the furthest the fist is held off the pouch, where nearer would put more than
POUCH_SHY = 0.016                     # this (m) of his hand into his body or kit: pouch_pinch)


def pouch_pinch(fit, base):
    """Where the fist pinches at the pouch, for this file and stance: as near as POUCH_PINCH, held off toward
    POUCH_PINCH_OFF only as far as it takes to keep no more than POUCH_SHY of his hand in his body or kit there."""
    cache = fit.__dict__.setdefault("_pinch", {})
    if base not in cache:
        T = reload_times(fit, base)
        p0, p1 = T["pouch"]
        S = solid_of(fit)
        for u in (0.0, 0.25, 0.5, 0.75, 1.0):
            cache[base] = tuple(a + u * (b - a) for a, b in zip(POUCH_PINCH, POUCH_PINCH_OFF))
            deep = 0.0
            for t in (p0, 0.5 * (p0 + p1)):
                lq, mv, _ = reload_pose(fit, base, t)
                V, Vm = skin_both(fit, *fit.man.fk(lq, mv))
                deep = max(deep, inside_depth(fit, V, Vm, S["arm"]["r"])[0])
            if deep <= POUCH_SHY:
                break
    return cache[base]


# ---------------------------------------------------------------------------------------------------------- reloads
# (the reload's beats, per stance: lying, the pouch is further from the rifle, so the hand is given longer to get there)
RELOAD_T = {
    "fire_kneel": {"down": (0.0, 0.42), "up": (2.16, 2.6), "to_bolt": (0.26, 0.46), "bolt_up": (0.46, 0.70),
                   "pouch": (1.10, 1.15), "lift": 1.46, "charger": 1.71, "press": ((1.74, 1.82), (1.85, 1.94)),
                   "handle": 2.12, "bolt_home": (2.12, 2.30), "grip": 2.50, "reload": 1.80},
    "fire_prone": {"down": (0.0, 0.32), "up": (2.22, 2.6), "to_bolt": (0.10, 0.27), "bolt_up": (0.27, 0.48),
                   "out": 0.80, "pouch": (1.14, 1.19), "back": 1.47, "lift": 1.70, "charger": 1.81,
                   "press": ((1.81, 1.89), (1.91, 1.99)), "handle": 2.16, "bolt_home": (2.16, 2.32), "grip": 2.47,
                   "reload": 1.85},
}
# (the magazine change's beats: to the magazine (to_bolt), it drawn out and down (bolt_up), the pouch, the new one up
# under the well (lift), seated (charger) and pressed twice, the charging handle taken (handle) and pulled (bolt_home))
MAG_T = {
    "fire_kneel": {"down": (0.0, 0.42), "up": (2.16, 2.6), "to_bolt": (0.08, 0.30), "bolt_up": (0.30, 0.44),
                   "pouch": (0.92, 0.96), "lift": 1.38, "charger": 1.62, "press": ((1.62, 1.70), (1.73, 1.81)),
                   "handle": 2.02, "bolt_home": (2.02, 2.12), "grip": 2.44, "reload": 1.66},
    "fire_prone": {"down": (0.0, 0.32), "up": (2.22, 2.6), "to_bolt": (0.04, 0.18), "bolt_up": (0.18, 0.26),
                   "out": 0.62, "pouch": (1.00, 1.03), "back": 1.40, "lift": 1.40, "charger": 1.80,
                   "press": ((1.80, 1.86), (1.88, 1.94)), "handle": 2.14, "bolt_home": (2.14, 2.22), "grip": 2.47,
                   "reload": 1.84},
}
PRONE_BY = (-0.10, 0.24, -0.14)      # (lying, the hand's way to the pouch and back: this far from his right shoulder)
PRONE_TURN = 1.0                     # (there, how far the hand is turned as his arm lies, from half way between)
POUCH_UP_OUT = (0.06, 0.10, 0.10)    # (kneeling, the hand's way up from the pouch: out to his side and up, his chest's
#                                      frame, so it rises clear of his belly)
HAND_FLOOR = 0.005           # (lying, the working hand's lowest point kept this far up off the ground)
MAG_PRONE_FWD = 0.14         # (lying, the magazine weapon held further out from his face than the rifle's bolt)


def reload_times(fit, base):
    return MAG_T[base] if fit.kind == "mag" else RELOAD_T[base]


STANCE = {
    # (the rifle low: its bolt's place from the pelvis (kneeling) or from his eyes (lying), in the world's axes as he
    # faces; the rifle's climb and its swing to his left (degrees), its cant (right side up); the trunk's roll onto
    # his left side (a twist down the spine) as the hand goes to the pouch, and its lean)
    "fire_kneel": {"bolt_at": ("pelvis", (0.46, 0.06, 0.28)), "climb": 24.0, "swing": 16.0, "cant": 30.0,
                   "roll": 6.0, "lean": 4.0, "look": 0.75, "mag_at": (0.38, 0.14, 0.42), "mag_swing": 26.0,
                   "by": (0.10, 0.02, 0.0)},
    "fire_prone": {"bolt_at": ("eye", (0.12, 0.08, None)), "bolt_z": 0.15, "mag_z": 0.22, "climb": 7.0, "swing": 10.0,
                   "cant": 26.0, "roll": 22.0, "lean": 0.0, "look": 0.6},
}


def low_place(fit, ref, st):
    """The rifle brought down for loading: its bolt where STANCE puts it, the muzzle climbing and swung to his left,
    canted. For the magazine weapons the magazine takes the bolt's place, canted the other way."""
    man = fit.man
    G, X = ref.G, ref.X
    swing = st["swing"]
    if st["bolt_at"][0] == "pelvis":
        at = X[man.b("pelvis")] + np.array(st["bolt_at"][1])
        if fit.kind == "mag":
            # (the magazine weapon held higher and nearer, so his right hand reaches its magazine and charging
            # handle, and swung further to his left, so its long butt passes out by his right hip, not into his belly)
            at = X[man.b("pelvis")] + np.array(st["mag_at"])
            swing = st["mag_swing"]
    else:
        h = man.b("head")
        eye = X[h] + G[h] @ fit.eye
        o = st["bolt_at"][1]
        z = st["bolt_z"]
        if fit.kind == "mag":
            # (the magazine's foot kept up off the ground, the weapon out from his face)
            o = (o[0] + MAG_PRONE_FWD, o[1], None)
            z = st["mag_z"]
        at = np.array((eye[0] + o[0], eye[1] + o[1], z))
    c, s_ = math.radians(st["climb"]), math.radians(swing)
    fwd = np.array((math.cos(c) * math.cos(s_), -math.cos(c) * math.sin(s_), math.sin(c)))
    side = unit(np.array((-fwd[1], fwd[0], 0.0)))
    Gw = frame_map(np.array((1.0, 0, 0)), np.array((0, 1.0, 0)), fwd, side)
    cant = st["cant"] * (-0.8 if fit.kind == "mag" else 1.0)
    Gw = R_axis(fwd, cant) @ Gw
    pin = fit.bolt_at if fit.kind == "bolt" else np.array((0.08, 0.0, 0.0))
    return Gw, at - Gw @ pin


def out_pole_of(base, sgn=1.0):
    """The working elbow's pole while the hand is at his belt: out and down kneeling, up and back lying."""
    if base == "fire_kneel":
        return unit(np.array((-0.35, 0.8 * sgn, -0.45)))
    return unit(np.array((-0.4, 0.3 * sgn, 0.85)))


def natural_hand(fit, lq, mv, s, T, ref, pole):
    """The hand's turn, his wrist at T, with the forearm and hand as the base has them (no twist of the forearm): his
    arm solved there with the pole, the base's hand-on-forearm carried onto it."""
    man = fit.man
    q = reach(fit, lq.copy(), mv, s, T, np.eye(3), ref, pole=pole, share=0.0)
    G, X = man.fk(q, mv)
    _, Rl, Rh, _, _ = ref.arm_ref[s]
    return G[man.b("lowerarm_" + s)] @ Rl.T @ Rh


def reload_pose(fit, base, t, L=2.6):
    """The bolt-action reload (the right hand works the bolt and loads a clip from the right front pouch; the left
    holds the rifle by its fore-end), or the magazine weapons' (the left hand changes the magazine; the right holds
    the grip)."""
    man = fit.man
    ref = fit.base[base]
    st = STANCE[base]
    T = reload_times(fit, base)
    lq, mv = ref.lq.copy(), ref.mv.copy()
    if t <= 1e-9 or t >= L - 1e-9:
        return lq, mv, {"on": {"l": ref.hand_w["l"], "r": ref.hand_w["r"]}, "W": (ref.Gw, ref.Xw)}
    low = sm((t - T["down"][0]) / (T["down"][1] - T["down"][0])) * (1 - sm((t - T["up"][0]) / (T["up"][1] - T["up"][0])))
    mag = fit.kind == "mag"
    p0, p1 = T["pouch"]
    # (the trunk: a little lean over the rifle; rolled onto his left side while the hand is at the pouch)
    roll = st["roll"] * kf([(p0 - 0.45, 0.0), (p0 - 0.05, 1.0), (p1 + 0.08, 1.0), (T["charger"], 0.3),
                            (T["up"][0] + 0.1, 0.0)], t)
    lean = st["lean"] * low
    if base == "fire_prone":
        lq, mv = prone_roll(fit, lq, mv, ref, roll)
        lq = chest_floor(fit, lq, mv, ref)
        lq = chest_settle(fit, lq, mv, ref, base, roll / st["roll"] if st["roll"] else 0.0)
        roll = 0.0
    for b, f in (("spine_01", 0.35), ("spine_02", 0.35), ("chest", 0.30)):
        bend(man, lq, b, (0, 0, 1), roll * f)
        bend(man, lq, b, (0, 1, 0), lean * f)
    # (the rifle low and canted, carried by the hand that keeps it)
    anchor = "l"
    Wd = blend_frames((ref.Gw, ref.Xw), low_place(fit, ref, st), low)
    # (on the way down and up the weapon is carried out before him, so its butt passes clear of his chest and belly)
    Wd = (Wd[0], Wd[1] + np.array(st.get("by", (0.0, 0.0, 0.0))) * math.sin(math.pi * low))
    lq, mv, W = hold(fit, lq, mv, Wd, {anchor: ref.hand_w[anchor]}, ref, anchor=anchor)
    G, X = man.fk(lq, mv)
    Gc_ = G[man.b("chest")]
    free = "r"
    grip = on_rifle(W, ref.hand_w[free])
    sgn = 1.0 if free == "r" else -1.0
    rounds = False
    bolt_b = 0.0
    Gpk, Xpk, pouch_at = pouch_place(fit, G, X, free, free, pouch_pinch(fit, base))
    # (on the way to and from the pouch the hand passes out at his side, clear of his belly)
    side_off = (Gc_ @ np.array((0.06, 0.10 * sgn, 0.06))) if base == "fire_kneel" else np.array((0.0, 0.10 * sgn, 0.10))
    up_off = (Gc_ @ np.array((0.02, 0.10 * sgn, 0.04))) if base == "fire_kneel" else np.array((-0.02, 0.12 * sgn, 0.04))
    tb0, tb1 = T["bolt_up"]
    th0, th1 = T["bolt_home"]
    if not mag:
        bolt_b = sm((t - tb0) / (tb1 - tb0)) * (1 - sm((t - th0) / (th1 - th0)))

        def on_bolt(b, rel):
            Rb, mb = fit.bolt_local(b)
            Gb = W[0] @ Rb
            Xb = W[1] + W[0] @ (fit.bolt_at + mb)
            return Gb @ rel[0], Xb + Gb @ rel[1]
        start = on_bolt(0.0, fit.bolt_hand_start)
        u_open = sm((t - tb0) / (tb1 - tb0)) if t < p0 else 1.0 - sm((t - th0) / (th1 - th0))
        handle = on_bolt(bolt_b, rel_blend(fit.bolt_hand_start, fit.bolt_hand, u_open))
        opened = on_bolt(1.0, fit.bolt_hand)
        # (the clip pinched over the charger guide: fingers down the right of the receiver, the thumb over its top)
        ch = W[1] + W[0] @ fit.charger
        Gch = hand_dn(man, "r", W[0] @ unit((0.15, -0.30, -1.0)), W[0] @ unit((0.0, -1.0, 0.25)))
        Xch = wrist_at(man, "r", Gch, ch, along=0.07, palm=0.03)
        press = 0.0
        for q0, q1 in T["press"]:
            press = max(press, math.sin(math.pi * min(max((t - q0) / (q1 - q0), 0.0), 1.0)))
        Xpr = Xch - W[0] @ np.array((0.0, 0.0, 0.028)) * press
        lift = (qslerp(Gpk, Gch, 0.65), Xch + up_off)
        if "out" in T:
            # (lying, the hand goes out past his right shoulder to his hip and back the same way, turned there as his
            # arm lies, so his forearm is not wrung round on the way)
            Sh = X[man.b("upperarm_r")]
            bx = Sh + np.array(PRONE_BY)
            Gn = natural_hand(fit, lq, mv, free, bx, ref, out_pole_of(base, sgn))
            by = (qslerp(qslerp(opened[0], Gpk, 0.5), Gn, PRONE_TURN), bx)
            via = [(T["out"], by, None, "time"), (p0, (Gpk, Xpk), None), (p1, (Gpk, Xpk), None),
                   (T["back"], by, None, "time")]
        else:
            # (kneeling, out at his side on the way to the pouch, and up and out from it, off his belly)
            mid_out = (qslerp(opened[0], Gpk, 0.5), 0.5 * (opened[1] + Xpk) + side_off)
            via = [(0.5 * (tb1 + p0), mid_out, None, True), (p0, (Gpk, Xpk), None), (p1, (Gpk, Xpk), None),
                   (p1 + 0.1, (qslerp(Gpk, Gch, 0.3), Xpk + Gc_ @ (np.array(POUCH_UP_OUT) * (1, sgn, 1))),
                    None, True)]
        # (from the open bolt, where the hand has stopped: the way out starts from rest)
        keys = [(T["to_bolt"][0], grip, None), (tb1 - 0.02, opened, None), (tb1, opened, None)] + via + \
            ([(T["lift"], lift, None, True)] if "out" not in T else []) + [
                (T["charger"], (Gch, Xch), None), (T["press"][1][1] + 0.02, (Gch, Xpr), None),
                (T["handle"], opened, None), (th1, start, None), (T["grip"], grip, None)]
        if t < T["to_bolt"][1]:
            Gh, Xh = path([(T["to_bolt"][0], grip, None), (T["to_bolt"][1], start, None)], t)
        elif t <= tb1:
            Gh, Xh = handle
        elif t < T["handle"]:
            Gh, Xh = path(keys, t)
        elif t <= th1:
            Gh, Xh = handle
        else:
            Gh, Xh = path([(th1, start, None), (T["grip"], grip, None)], t)
        on = {"l": ref.hand_w["l"]}
        if t < T["to_bolt"][0] or t > T["grip"]:
            on["r"] = ref.hand_w["r"]
        work = ch
        # (the clip in his fist from the pouch until its rounds are pressed down into the magazine)
        rounds = 0.5 * (p0 + p1) <= t <= T["press"][1][1] + 0.02
        curl = kf([(T["to_bolt"][0], 0.0), (T["to_bolt"][1], 1.0), (tb1, 1.0), (p0 - 0.05, 0.35), (p0 + 0.1, 0.75),
                   (T["charger"], 0.75), (T["press"][1][1] + 0.02, 0.7), (T["handle"], 1.0), (th1, 1.0), (T["grip"], 0.0)],
                  t)
        fing = (62.0, 40.0)
    else:
        # (the magazine change, done by the right hand as the bolt-action's clip is: to the magazine, drawn out and
        # down (out to his right, lying), to the right front pouch by the bolt-action's way, the new one up under the
        # well, seated and pressed twice, the charging handle on the right of the receiver pulled back, the grip again)
        P = fit.wpts
        sel = (P[:, 0] > -0.02) & (P[:, 0] < 0.32) & (P[:, 2] < -0.05)
        mx = float(P[sel, 0].mean()) if sel.any() else 0.12
        mz = float(P[sel, 2].min()) if sel.any() else -0.1
        mag_at = W[1] + W[0] @ np.array((mx, 0.0, mz - 0.01))
        # (the fist round the magazine's foot, fingers forward along it, the palm up against it)
        Gm = hand_dn(man, "r", W[0] @ unit((1.0, 0.0, -0.25)), W[0] @ np.array((0.0, 0.0, 1.0)))
        Xm = wrist_at(man, "r", Gm, mag_at, along=0.06, palm=0.03)
        push = 0.0
        for q0, q1 in T["press"]:
            push = max(push, math.sin(math.pi * min(max((t - q0) / (q1 - q0), 0.0), 1.0)))
        if base == "fire_prone":
            # (lying, the magazine is drawn out to his right over the ground, not down into it)
            drop = (Gm, Xm + np.array((-0.02, 0.07, 0.01)))
        else:
            drop = (Gm, Xm - W[0] @ np.array((0.0, 0.0, 0.07)))
        # (the charging handle: the right of the receiver over the well, the fingers hooked on it)
        hx = W[1] + W[0] @ np.array((mx + 0.03, 0.045, 0.02))
        Gs = hand_dn(man, "r", W[0] @ unit((0.35, -0.15, -1.0)), W[0] @ np.array((0.0, -1.0, 0.0)))
        Xs = wrist_at(man, "r", Gs, hx)
        Xs2 = Xs - W[0] @ np.array((0.07, 0.0, 0.0))
        if "out" in T:
            # (out at his right side half way from the magazine, held out before his face, to the pouch, turned as
            # his arm lies there)
            Sh = X[man.b("upperarm_r")]
            bx = np.array((0.5 * (drop[1][0] + Xpk[0]), Sh[1] + PRONE_BY[1], Sh[2] + PRONE_BY[2]))
            Gn = natural_hand(fit, lq, mv, free, bx, ref, out_pole_of(base, sgn))
            by = (qslerp(qslerp(drop[0], Gpk, 0.5), Gn, PRONE_TURN), bx)
            via = [(T["out"], by, None, "time"), (p0, (Gpk, Xpk), None), (p1, (Gpk, Xpk), None),
                   (T["back"], by, None, "time")]
        else:
            mid_out = (qslerp(Gm, Gpk, 0.5), 0.5 * (drop[1] + Xpk) + side_off)
            via = [(0.5 * (tb1 + p0), mid_out, None, True), (p0, (Gpk, Xpk), None), (p1, (Gpk, Xpk), None),
                   (p1 + 0.1, (qslerp(Gpk, Gm, 0.3), Xpk + Gc_ @ (np.array(POUCH_UP_OUT) * (1, sgn, 1))),
                    None, True),
                   (T["lift"], drop, None, True)]
        keys = [(T["to_bolt"][0], grip, None), (T["to_bolt"][1], (Gm, Xm), None), (tb1, drop, None)] + via + [
            (T["charger"], (Gm, Xm), None), (T["press"][1][1] + 0.02, (Gm, Xm), None),
            (th0, (Gs, Xs), None), (th1, (Gs, Xs2), None), (T["grip"], grip, None)]
        Gh, Xh = path(keys, t)
        Xh = Xh + W[0] @ np.array((0.0, 0.0, 0.02)) * push
        on = {"l": ref.hand_w["l"]}
        if t < T["to_bolt"][0] or t > T["grip"]:
            on["r"] = ref.hand_w["r"]
        work = mag_at
        rounds = 0.5 * (p0 + p1) <= t <= T["charger"]
        curl = kf([(T["to_bolt"][0], 0.0), (T["to_bolt"][1], 0.8), (p0 - 0.05, 0.5), (p0 + 0.1, 0.8),
                   (T["press"][1][1] + 0.02, 0.8), (th0, 1.0), (th1, 1.0), (T["grip"], 0.0)], t)
        fing = (58.0, 35.0)
    # (the elbow out and up while the hand is at his belt, so the arm clears his side)
    base_pole = Gc_ @ ref.arm_ref[free][3]
    out_pole = out_pole_of(base, sgn)
    if "out" in T:
        wpole = kf([(T["to_bolt"][0], 0.0), (T["out"], 1.0), (T["back"], 1.0), (T["grip"], 0.0)], t)
    else:
        # (the elbow comes back in at an even rate over the whole way up from the pouch, not swung in mid-way)
        wpole = kf([(tb1 - 0.05, 0.0), (p0, 1.0), (p1, 1.0), (T["charger"] + 0.06, 0.0)], t,
                   lambda x: trap(x, 0.35))
    pole = unit(base_pole * (1 - wpole) + out_pole * wpole)
    lq = reach(fit, lq, mv, free, Xh, Gh, ref, pole=pole, share=0.6)
    lq = fingers_mix(man, lq, ref, free, fing[0], fing[1], curl)
    if base == "fire_prone":
        # (lying, his hand kept up off the ground (HAND_FLOOR) wherever the way takes it: raised as much as it would
        # go into it)
        S = solid_of(fit)
        m = S["hand"][free]
        for _ in range(3):
            Gq, Xq = man.fk(lq, mv)
            zl = float(gore.skin(S["P"][m], S["a"][m], S["b"][m], S["w"][m], Gq, Xq, man.H)[:, 2].min())
            if zl >= HAND_FLOOR - 1e-4:
                break
            Xh = Xh + np.array((0.0, 0.0, HAND_FLOOR - zl + 0.002))
            lq = reach(fit, lq, mv, free, Xh, Gh, ref, pole=pole, share=0.6)
            lq = fingers_mix(man, lq, ref, free, fing[0], fing[1], curl)
    # (his eyes on the work, on the pouch while the hand is there)
    wp = kf([(tb1, 0.0), (p0 - 0.05, 1.0), (p1, 1.0), (T["lift"], 0.0)], t)
    lq = look(fit, lq, mv, work * (1 - wp) + pouch_at * wp, low * st["look"], most=45.0)
    if fit.kind == "bolt":
        Rb, mb = fit.bolt_local(bolt_b)
        lq[man.b("bolt")] = Rb
        mv[man.b("bolt")] = mb
    lq, mv = keep_bolt(fit, lq, mv, ref)
    info = {"on": on, "W": W}
    if rounds:
        G2, X2 = man.fk(lq, mv)
        if fit.kind == "bolt" and t >= T["press"][0][0]:
            u = min(1.0, (t - T["press"][0][0]) / (T["press"][1][1] - T["press"][0][0]))
            info["rounds"] = W[1] + W[0] @ (fit.charger - np.array((0.0, 0.0, 0.01 + 0.02 * u)))
        else:
            info["rounds"] = fist_point(man, G2, X2, free, along=0.08, palm=0.025)
    return lq, mv, info


# ---------------------------------------------------------------------------------------------------------- signals
def one_hand(fit, lq, mv, ref, out=0.0):
    """The rifle in his right hand only, as it lies in the base, the hand kept where it is by his hip (on the pelvis,
    so it does not press into his belly as his trunk turns and dips), `out` (m) further out to his right."""
    man = fit.man
    G, X = man.fk(lq, mv)
    # (the launcher lies on his shoulder: his right hand on its grip rides his chest, not his hips)
    pe = man.b("chest" if fit.kind == "launcher" else "pelvis")
    h = man.b("hand_r")
    Rh = ref.G[pe].T @ ref.G[h]
    ph = ref.G[pe].T @ (ref.X[h] - ref.X[pe]) + np.array((0.0, out if fit.kind != "launcher" else 0.0, 0.0))
    Gh, Xh = G[pe] @ Rh, X[pe] + G[pe] @ ph
    lq = reach(fit, lq, mv, "r", Xh, Gh, ref)
    G, X = man.fk(lq, mv)
    h = man.b("hand_r")
    Rw, pw = ref.hand_w["r"]
    Gw = G[h] @ Rw.T
    Xw = X[h] - Gw @ pw
    lq, mv = set_weapon(man, lq, mv, Gw, Xw)
    return lq, mv, (Gw, Xw)


def left_home(fit, ref, G, X, W):
    """Where his left hand goes back to: the rifle's fore-end as in the base (or, unarmed, his chest's)."""
    man = fit.man
    if fit.kind == "none":
        Gc, Xc = G[man.b("chest")], X[man.b("chest")]
        _, _, Rh, _, ph = ref.arm_ref["l"]
        return Gc @ Rh, Xc + Gc @ ph
    return on_rifle(W, ref.hand_w["l"])


# (the left elbow's pole, from the base's to out and down behind him, by weapon: unarmed and with the launcher the
# base's arm is far from the rifleman's, so the pole is changed later, once the hand is up)
GO_FIRST = 0.0
GO_R_OUT = 0.015            # (his right hand eased this far out from his hip while he turns and leans to point)
GO_BACK = ((-0.02, -0.20, 0.04), 0.3)
GO_POLE_KEYS = {"rifle": [(0.0, 0.0), (0.56, 1.0), (1.20, 1.0), (1.6, 0.0)],
                "none": [(0.25, 0.0), (0.80, 1.0), (1.01, 1.0), (1.6, 0.0)],
                "launcher": [(0.45, 0.0), (0.95, 1.0), (1.18, 1.0), (1.6, 0.0)]}
# (the beats: the hand wound back by `back`, on the point at `point`, pushed on at `push`, held to `hold`, home at
# 1.58 s; unarmed and with the launcher the hand starts further from the wind-up and its way is longer, so it is
# given longer; unarmed it also has furthest to go home, to his side, so its hold is the shortest)
GO_T = {"rifle": {"back": 0.52, "point": 1.00, "push": 1.08, "hold": 1.20},
        "launcher": {"back": 0.63, "point": 1.04, "push": 1.11, "hold": 1.18},
        "none": {"back": 0.56, "point": 0.95, "push": 0.98, "hold": 1.01}}


def go_beats(fit):
    return GO_T.get(fit.kind, GO_T["rifle"])


def signal_go_pose(fit, t, L=1.6):
    man = fit.man
    ref = fit.base["idle"]
    lq, mv = ref.lq.copy(), ref.mv.copy()
    if t <= 1e-9 or t >= L - 1e-9:
        return lq, mv, {"on": {"l": ref.hand_w["l"], "r": ref.hand_w["r"]}, "W": (ref.Gw, ref.Xw)}
    # (his trunk turns a little to his left as the arm winds back and he glances at the men behind, then leans into
    # the point; his head looks back over his left shoulder, then the way he points)
    turn = kf([(0.03, 0.0), (0.42, -14.0), (0.90, 6.0), (1.18, 4.0), (1.57, 0.0)], t)
    lean = kf([(0.44, 0.0), (0.94, 6.0), (1.18, 5.0), (1.57, 0.0)], t)
    head = kf([(0.03, 0.0), (0.40, -42.0), (0.86, 4.0), (1.18, 2.0), (1.57, 0.0)], t)
    for b, f in (("spine_01", 0.3), ("spine_02", 0.35), ("chest", 0.35)):
        bend(man, lq, b, (0, 0, 1), turn * f)
        bend(man, lq, b, (0, 1, 0), lean * f)
    bend(man, lq, "neck", (0, 0, 1), head * 0.45)
    bend(man, lq, "head", (0, 0, 1), head * 0.55)
    # (his right hand eased a little out from his hip while his trunk is turned and leant, so the forearm, carried
    # forward with his right shoulder, does not press into his belly)
    lq, mv, W = one_hand(fit, lq, mv, ref, GO_R_OUT * kf([(0.03, 0.0), (0.45, 1.0), (1.25, 1.0), (1.57, 0.0)], t))
    G, X = man.fk(lq, mv)
    Gc = G[man.b("chest")]
    S = X[man.b("upperarm_l")]
    A = man.side["l"]
    reach_ = A["l1"] + A["l2"]
    home = left_home(fit, ref, G, X, W)
    # (out to his side, wound back behind and over the shoulder, swept forward and up to point the way, held)
    side_G = hand_dn(man, "l", Gc @ unit((0.2, -0.6, -0.8)), Gc @ unit((0.0, 1.0, 0.0)))
    side_X = S + Gc @ np.array((0.10, -0.30, -0.22))
    back_G = hand_dn(man, "l", Gc @ unit((-0.35, -0.30, 0.9)), Gc @ unit((1.0, -0.1, 0.3)))
    back_X = S + Gc @ np.array((-0.08, -0.17, 0.07))
    fwd = unit(np.array((1.0, -0.22, 0.34)))
    point_G = hand_dn(man, "l", fwd, unit(np.array((0.0, 1.0, 0.0))))
    point_X = S + fwd * reach_ * 0.88
    push_X = point_X + fwd * 0.03 + np.array((0.0, 0.0, -0.03))
    if fit.kind in ("none", "launcher"):
        # (unarmed his left hand starts low at his side, with the launcher high on the launcher's grip before his
        # right shoulder: out to his side (or out before his left shoulder) first, and a shorter point so the hand
        # has longer to come home)
        if fit.kind == "none":
            first = (qslerp(home[0], back_G, 0.45), side_X)
        else:
            first = (qslerp(home[0], back_G, 0.3), S + Gc @ np.array((0.30, -0.12, 0.02)))
        # (a shorter wind-up: the hand up beside the shoulder rather than behind it)
        bG, bX = qslerp(back_G, point_G, GO_BACK[1]), S + Gc @ np.array(GO_BACK[0])
        B = go_beats(fit)
        keys = [(0.0, home, None)] + ([(GO_FIRST, first, None)] if GO_FIRST else []) + [
                (B["back"], (bG, bX), Gc @ np.array((0.0, -0.04, 0.0))),
                (B["point"], (point_G, point_X), Gc @ np.array((0.0, -0.05, 0.03))),
                (B["push"], (point_G, push_X), None), (B["hold"], (point_G, point_X), None), (1.58, home, None)]
    else:
        B = go_beats(fit)
        keys = [(0.0, home, None), (B["back"], (back_G, back_X), Gc @ np.array((0.0, -0.04, 0.0))),
                (B["point"], (point_G, point_X), Gc @ np.array((0.0, -0.05, 0.03))),
                (B["push"], (point_G, push_X), None), (B["hold"], (point_G, point_X), None),
                (1.58, home, Gc @ np.array((0.0, -0.05, -0.03)))]
    Gh, Xh = path(keys, t)
    # (the elbow's swing out from the base's spread evenly over the wind-up, not thrown out at its start)
    wp = kf(GO_POLE_KEYS.get(fit.kind, GO_POLE_KEYS["rifle"]), t, lambda x: trap(x, 0.25))
    pole = unit(Gc @ ref.arm_ref["l"][3] * (1 - wp) + Gc @ unit((-0.4, -1.0, -0.6)) * wp)
    lq = reach(fit, lq, mv, "l", Xh, Gh, ref, pole=pole)
    open_ = kf([(0.05, 0.0), (0.30, 1.0), (1.18, 1.0), (1.52, 0.0)], t)
    lq = fingers_mix(man, lq, ref, "l", -16.0, 6.0, open_)
    lq, mv = keep_bolt(fit, lq, mv, ref)
    on = {"r": ref.hand_w["r"]}
    if t > 1.58:
        on["l"] = ref.hand_w["l"]
    return lq, mv, {"on": on, "W": W, "point": point_X}


# (signal_down's beats: the two presses, and the left hand's keys (off home, flat out, held to, home again); unarmed
# or with the launcher his left hand's home is further off (at his side, or high on the launcher before his right
# shoulder), so the presses end sooner and the hand is given longer to get back, its fingers no faster than about
# 13 cm a key)
DOWN_T = {"near": {"press": ((0.54, 0.88), (0.90, 1.26)), "keys": (0.04, 0.50, 1.30, 1.70)},
          "far": {"press": ((0.60, 0.90), (0.92, 1.20)), "keys": (0.0, 0.58, 1.21, 1.78)}}


def signal_down_pose(fit, t, L=1.8):
    man = fit.man
    ref = fit.base["idle"]
    lq, mv = ref.lq.copy(), ref.mv.copy()
    if t <= 1e-9 or t >= L - 1e-9:
        return lq, mv, {"on": {"l": ref.hand_w["l"], "r": ref.hand_w["r"]}, "W": (ref.Gw, ref.Xw)}
    # (he looks along the line to his left, the men he is telling, and his trunk dips with each press)
    head = kf([(0.05, 0.0), (0.42, -30.0), (1.30, -30.0), (1.72, 0.0)], t)
    far = fit.kind in ("none", "launcher")
    DT = DOWN_T["far" if far else "near"]
    press = 0.0
    for p0, p1 in DT["press"]:
        press = max(press, math.sin(math.pi * min(max((t - p0) / (p1 - p0), 0.0), 1.0)))
    turn = kf([(0.05, 0.0), (0.44, -10.0), (1.30, -10.0), (1.72, 0.0)], t)
    for b, f in (("spine_01", 0.3), ("spine_02", 0.35), ("chest", 0.35)):
        bend(man, lq, b, (0, 0, 1), turn * f)
        bend(man, lq, b, (0, 1, 0), 5.0 * press * f)
    bend(man, lq, "neck", (0, 0, 1), head * 0.45)
    bend(man, lq, "head", (0, 0, 1), head * 0.55)
    lq, mv, W = one_hand(fit, lq, mv, ref)
    G, X = man.fk(lq, mv)
    Gc = G[man.b("chest")]
    S = X[man.b("upperarm_l")]
    home = left_home(fit, ref, G, X, W)
    # (the arm out before him and to his left, the hand flat, palm down, pressed down from the elbow and shoulder)
    out_dir = unit(np.array((0.75, -0.62, 0.0)))
    flat_G = hand_dn(man, "l", unit(out_dir + np.array((0.0, 0.0, -0.08))), np.array((0.0, 0.0, -1.0)))
    flat_X = S + out_dir * 0.46 + np.array((0.0, 0.0, -0.10))
    down_G = hand_dn(man, "l", unit(out_dir + np.array((0.0, 0.0, -0.30))), unit(np.array((0.10, 0.0, -1.0))))
    down_X = S + out_dir * 0.42 + np.array((0.0, 0.0, -0.25))
    Gh, Xh = blend_frames((flat_G, flat_X), (down_G, down_X), press)
    # (unarmed or with the launcher, his left hand's home is further off (at his side, or high on the launcher
    # before his right shoulder), so it is given longer to get back)
    t_out, t_flat, t_hold, t_home = DT["keys"]
    keys = [(t_out, home, None), (t_flat, (flat_G, flat_X), Gc @ np.array((0.0, -0.06, 0.05))),
            (t_hold, (flat_G, flat_X), None), (t_home, home, None if far else Gc @ np.array((0.0, -0.06, 0.0)))]
    if t_flat <= t <= t_hold:
        pass
    else:
        Gh, Xh = path(keys, t)
    wp = kf([(0.0, 0.0), (0.40, 1.0), (1.35, 1.0), (L, 0.0)], t)
    pole = unit(Gc @ ref.arm_ref["l"][3] * (1 - wp) + Gc @ unit((-0.3, -1.0, -0.5)) * wp)
    lq = reach(fit, lq, mv, "l", Xh, Gh, ref, pole=pole)
    open_ = kf([(0.06, 0.0), (0.35, 1.0), (1.32, 1.0), (1.66, 0.0)], t)
    lq = fingers_mix(man, lq, ref, "l", -28.0, 0.0, open_)
    lq, mv = keep_bolt(fit, lq, mv, ref)
    on = {"r": ref.hand_w["r"]}
    if t < t_out or t > t_home:
        on["l"] = ref.hand_w["l"]
    return lq, mv, {"on": on, "W": W}


# ---------------------------------------------------------------------------------------------------------- looks
def two_hands(fit, lq, mv, ref, Wd, anchor="l", rel=None, keep="chest", poles=None, shift=None):
    """Both hands on the rifle placed at Wd (unarmed: the hands kept as in the base on his chest, on his hips, on a
    thigh, or where they lie on the ground ("world"), each side its own when `keep` is a dict; the weapon bone on his
    right hand)."""
    man = fit.man
    rel = rel or {"l": ref.hand_w["l"], "r": ref.hand_w["r"]}
    if fit.kind == "none":
        G, X = man.fk(lq, mv)
        for s in ("l", "r"):
            h = man.b("hand_" + s)
            ks = keep[s] if isinstance(keep, dict) else keep
            if ks == "world":
                Gh, Xh = ref.G[h], ref.X[h]
            else:
                k = man.b(ks)
                Gh = G[k] @ (ref.G[k].T @ ref.G[h])
                Xh = X[k] + G[k] @ (ref.G[k].T @ (ref.X[h] - ref.X[k]))
            if shift and s in shift:
                Xh = Xh + np.asarray(shift[s], float)
            lq = reach(fit, lq, mv, s, Xh, Gh, ref)
            # (his elbow no lower than the base has it: a hand carried round with his chest is raised to keep it up)
            e = man.b("lowerarm_" + s)
            for _ in range(2):
                ez = man.fk(lq, mv)[1][e][2]
                if ez >= ref.X[e][2] - 0.005:
                    break
                Xh = Xh + np.array((0.0, 0.0, ref.X[e][2] - ez))
                lq = reach(fit, lq, mv, s, Xh, Gh, ref)
        G, X = man.fk(lq, mv)
        h = man.b("hand_r")
        Rw, pw = ref.hand_w["r"]
        Gw = G[h] @ Rw.T
        lq, mv = set_weapon(man, lq, mv, Gw, X[h] - Gw @ pw)
        return lq, mv, (Gw, X[h] - Gw @ pw), {}
    lq, mv, W = hold(fit, lq, mv, Wd, rel, ref, anchor=anchor, poles=poles)
    return lq, mv, W, dict(rel)


def look_back_pose(fit, base, t, L=2.2):
    man = fit.man
    ref = fit.base[base]
    lq, mv = ref.lq.copy(), ref.mv.copy()
    if t <= 1e-9 or t >= L - 1e-9:
        return lq, mv, {"on": {"l": ref.hand_w["l"], "r": ref.hand_w["r"]}, "W": (ref.Gw, ref.Xw)}
    prone = base == "fire_prone"
    # (the turn in, a held look with a small scan, the turn back)
    turn = LOOK_TURN_PRONE if prone else LOOK_TURN
    deg = kf([(0.12, 0.0), (0.70, turn), (1.00, turn - 4.0), (1.30, turn + 2.0), (1.45, turn),
              (2.05, 0.0)], t)
    lift = kf([(0.10, 0.0), (0.55, 1.0), (1.50, 1.0), (2.00, 0.0)], t)
    head_turn(man, lq, deg, LOOK_SHARE_PRONE if prone else LOOK_SHARE)
    # (his chin comes up off the stock as he turns: the neck straightens a little; lying, his chest rises on the
    # elbows)
    bend(man, lq, "neck", (0, 1, 0), -10.0 * lift)
    bend(man, lq, "head", (0, 1, 0), -6.0 * lift)
    if prone:
        bend(man, lq, "spine_02", (0, 1, 0), -4.0 * lift)
        bend(man, lq, "chest", (0, 1, 0), -3.0 * lift)
    if prone:
        lq = chest_floor(fit, lq, mv, ref)
        lq, mv = trunk_floor(fit, lq, mv, ref)
    # (never more than HEAD_MOST of turn on his chest: what a file's own aim already has counts toward it)
    G, X = man.fk(lq, mv)
    tw = twist_z(G[man.b("chest")].T @ G[man.b("head")])
    if abs(tw) > HEAD_MOST:
        bend(man, lq, "head", (0, 0, 1), -math.copysign(abs(tw) - HEAD_MOST, tw))
    # (the rifle eased off his cheek: the butt down off the shoulder about the left fist, a little forward)
    off = kf([(0.0, 0.0), (0.35, 1.0), (1.60, 1.0), (2.10, 0.0)], t)
    W1 = low_rifle(ref, ref.hand["l"][1], 6.0 if not prone else 4.0, 4.0, 8.0,
                   (0.03, 0.02, -0.03 if not prone else 0.0))
    Wd = blend_frames((ref.Gw, ref.Xw), W1, off)
    # (unarmed and kneeling, his hands stay where they rest, on his knee and by his hip, as he turns his trunk; carried
    # round with his chest they would go down into his thigh)
    # (his left hand lifted a little off his knee, back and out, as his left shoulder comes round, so the forearm does
    # not lie down into his thigh)
    lq, mv, W, on = two_hands(fit, lq, mv, ref, Wd, anchor="l", keep="chest" if prone else "pelvis",
                              shift=None if prone else {"l": np.array(LOOK_HAND_OFF) * min(1.0, 2.0 * deg / turn)})
    lq, mv = keep_bolt(fit, lq, mv, ref)
    return lq, mv, {"on": on, "W": W}


# ---------------------------------------------------------------------------------------------------------- peek
def head_top(fit, G, X):
    h = fit.man.b("head")
    return float((X[h] + (G[h] @ fit.head_pts.T).T)[:, 2].max())


def eye_z(fit, G, X):
    h = fit.man.b("head")
    return float((X[h] + G[h] @ fit.eye)[2])


def cover_rifle(fit, ref, G, X):
    """The rifle carried behind cover: held low before his knees in both hands as idle holds it at the port (the right
    fist on the grip before his right knee, the fore-end out to his left past his left shin, the muzzle forward and to
    his left, short of the cover's face, the sights up and a little toward him), so it is clear of his belly, hips,
    thighs and his bowed head. Placed once on his fully ducked body, then carried on his pelvis. Returns the placement
    and the hands on it."""
    man = fit.man
    idle = fit.base["idle"]
    pe = man.b("pelvis")
    if getattr(fit, "_cover", None) is None:
        dk, rk = peek_levels(fit)
        lq, mv = peek_body(fit, ref.lq.copy(), ref.mv.copy(), ref, dk, 0.0, 0.0)
        G1, X1 = man.fk(lq, mv)
        kr, th = X1[man.b("calf_r")], X1[man.b("thigh_r")]
        wrist = np.array((kr[0] + COVER_GRIP[0], kr[1] + COVER_GRIP[1], th[2] + COVER_GRIP[2]))
        Rr, pr = ref.hand_w["r"]
        xf = cover_of(fit)["face_x"]

        def place(yaw):
            d = unit(np.array((math.cos(math.radians(yaw)), -math.sin(math.radians(yaw)), -0.06)))
            up = unit(np.array((0.0, 0.0, 1.0)) - d * d[2])
            Gw = frame_map(np.array((1.0, 0, 0)), np.array((0, 0, 1.0)), d, up)
            Gw = R_axis(d, -COVER_ROLL) @ Gw
            return Gw, wrist - Gw @ pr
        # (swung further to his left if the muzzle would come near the cover's face, ducked or risen to peek)
        lq2, mv2 = peek_body(fit, ref.lq.copy(), ref.mv.copy(), ref, 0.0, rk, 0.0)
        G2, X2 = man.fk(lq2, mv2)
        fit._cover_pelvis = X1[pe].copy()
        yaw = COVER_YAW
        for _ in range(30):
            Gw, Xw = place(yaw)
            Rp, pp = G1[pe].T @ Gw, G1[pe].T @ (Xw - X1[pe])
            fit._cover = (Rp, pp)
            Gr, Xr = cover_rifle(fit, ref, G2, X2)[0]
            reach_x = max(float((Xw + fit.wpts @ Gw.T)[:, 0].max()), float((Xr + fit.wpts @ Gr.T)[:, 0].max()))
            if reach_x < xf - 0.06 or yaw > 85:
                break
            yaw += 3.0
        fit._cover_yaw = yaw
    Rp, pp = fit._cover
    # (carried with his pelvis as it sinks and rises, but only COVER_FOLLOW of its swing forward and back over the knee,
    # so the muzzle does not reach for the cover as he rises to peek)
    d = X[pe] - fit._cover_pelvis
    d = np.array((d[0] * COVER_FOLLOW, d[1] * COVER_FOLLOW, d[2]))
    return (G[pe] @ Rp, fit._cover_pelvis + d + G[pe] @ pp), {"l": ref.hand_w["l"], "r": ref.hand_w["r"]}


COVER_GRIP = (0.30, 0.07, -0.05)     # (his right wrist ducked: out before his right knee, a little under his hip joint)
COVER_FOLLOW = 0.7        # (how much of his pelvis's swing forward the low rifle follows)
COVER_YAW = 64.0          # (the muzzle's swing to his left of straight ahead, at least)
COVER_ROLL = 15.0         # (the sights turned a little toward him)
COVER_AWAY = 0.08         # (the weapon carried out to his right on the way, off his face)
COVER_LIFT = 10.0         # (the muzzle lifted on the way down and up, at the middle, besides what clears the cover)
COVER_POLE_L = (0.3, -0.5, 0.2)      # (his left elbow forward, out and up, over his raised left knee)
COVER_BACK = 0.50         # (the cover's face this far before his front knee)
COVER_CLEAR = 0.02        # (the aim's lowest point past the face this far over the cover's top, at most)
DUCK_UNDER = 0.07         # (his helmet's top ducked this far under the lowest top the clip suits)
WALL_MARGIN = 0.015       # (the weapon kept this far out of the cover, face and top, as he ducks and comes up)
# (the beats: the rifle off the aim to the port, the duck, the rifle down across his thighs, the hold, the slow rise to
# peek, the scan, the second duck, the rifle back up to the port behind the cover, his rise, the rifle levelled over the
# top onto the aim)
PEEK_T = {"low": (0.02, 0.88), "duck": (0.14, 0.76), "rise": (0.86, 1.40), "scan": (1.42, 1.70), "duck2": (1.70, 2.22),
          "back": (2.24, 2.70), "up": (2.10, 2.98)}
# (a launcher's long tube is taken down and up more slowly, so its ends go no faster than a rifle's muzzle)
PEEK_T_LAUNCHER = {"low": (0.02, 1.00), "up": (1.98, 2.98)}


def peek_beats(fit):
    return dict(PEEK_T, **PEEK_T_LAUNCHER) if fit.kind == "launcher" else PEEK_T


def cover_of(fit):
    """The cover the clip is made for, from his own aim: his front knee's x, the cover's face COVER_BACK before it, and
    the highest top the aim clears (the aim's lowest point past the face, less COVER_CLEAR, and his eyes over it)."""
    if getattr(fit, "_wall", None) is None:
        man = fit.man
        ref = fit.base["fire_kneel"]
        S = solid_of(fit)
        V = gore.skin(S["P"], S["a"], S["b"], S["w"], ref.G, ref.X, man.H)
        kx = float(max(ref.X[man.b("calf_l"), 0], ref.X[man.b("calf_r"), 0]))
        xf = kx + COVER_BACK
        past = V[:, 0] > xf
        hi = eye_z(fit, ref.G, ref.X) - 0.08
        low_past = float(V[past, 2].min()) if past.any() else None
        if low_past is not None:
            hi = min(hi, low_past - COVER_CLEAR)
        fit._wall = {"knee_x": kx, "face_x": xf, "top_hi": hi, "aim_low_past_face": low_past}
    return fit._wall


def lift_axis(W):
    """The axis (world) a turn about which by a positive angle lifts the weapon's muzzle."""
    fwd = W[0] @ np.array((1.0, 0.0, 0.0))
    fh = np.array((fwd[0], fwd[1], 0.0))
    return unit(np.cross(fh, (0.0, 0.0, 1.0))) if np.linalg.norm(fh) > 1e-6 else None


def butt_of(fit, W):
    """The butt's end of the weapon at W (world)."""
    P = fit.wpts
    return W[1] + W[0] @ P[int(P[:, 0].argmin())]


def wall_angle(fit, W, m=WALL_MARGIN):
    """The least turn (degrees), muzzle up about the butt, that keeps every point of the weapon at W out of the cover
    (behind its face or over its top, by m)."""
    wl = cover_of(fit)
    xf, top = wl["face_x"] - m, wl["top_hi"] + m
    P = fit.wpts
    ax = lift_axis(W)
    if ax is None:
        return 0.0
    piv = butt_of(fit, W)

    def pen(th):
        G2, X2 = about_point(W[0], W[1], R_axis(ax, th), piv)
        Q = X2 + P @ G2.T
        k = (Q[:, 0] > xf) & (Q[:, 2] < top)
        return float(np.minimum(Q[k, 0] - xf, top - Q[k, 2]).max()) if k.any() else 0.0
    if pen(0.0) <= 0.0:
        return 0.0
    lo, hi = 0.0, 85.0
    for _ in range(16):
        mid = 0.5 * (lo + hi)
        if pen(mid) > 0.0:
            lo = mid
        else:
            hi = mid
    return hi


def peek_design(fit, Wc, low, lift=True):
    """The weapon's way between the base's aim (in the world) and the low carry Wc, `low` of the way: its muzzle lifted
    on the way (COVER_LIFT at the middle) and by as much more as keeps it out of the cover there (worked out along the
    way, widened and eased: peek_arc)."""
    ref = fit.base["fire_kneel"]
    Wd = blend_frames((ref.Gw, ref.Xw), Wc, low)
    # (out to his right, off his cheek, on the way: his bowed head goes down past where the receiver was)
    Wd = (Wd[0], Wd[1] + np.array((0.0, COVER_AWAY, 0.0)) * math.sin(math.pi * low))
    ax = lift_axis(Wd)
    if ax is not None:
        ang = COVER_LIFT * math.sin(math.pi * low) ** 2
        if lift:
            ang += float(np.interp(low, *peek_arc(fit)[2]))
        Wd = about_point(Wd[0], Wd[1], R_axis(ax, ang), butt_of(fit, Wd))
    return Wd


def peek_arc(fit, n=200):
    """Along `low`: the lift over the cover it needs (the least that clears, widened by about 0.04 of the way and
    eased), and the share of the whole way the weapon's front has gone, for taking the way at an even pace. Worked out
    with the low carry where it is ducked."""
    if getattr(fit, "_peek_arc", None) is None:
        ref = fit.base["fire_kneel"]
        dk, rk = peek_levels(fit)
        # (the low carry follows his pelvis, which sits back as he ducks: the lift is worked out with him upright,
        # half and fully ducked, and the most of the three taken, as the weapon may come down before he is down)
        Wcs = []
        for k in (0.0, 0.5, 1.0):
            lq, mv = peek_body(fit, ref.lq.copy(), ref.mv.copy(), ref, dk * k, 0.0, 0.0)
            Wcs.append(cover_rifle(fit, ref, *fit.man.fk(lq, mv))[0])
        Wc = Wcs[-1]
        us = np.linspace(0.0, 1.0, n + 1)
        g = np.array([max(wall_angle(fit, peek_design(fit, W_, u, lift=False), m=WALL_MARGIN + 0.01) for W_ in Wcs)
                      for u in us])
        k, e = int(0.04 * n), int(0.03 * n)
        gd = np.array([g[max(0, i - k):i + k + 1].max() for i in range(len(g))])
        gs = np.maximum(np.array([gd[max(0, i - e):i + e + 1].mean() for i in range(len(gd))]), g)
        gs[0] = gs[-1] = 0.0
        fit._peek_arc = (None, None, (us, gs))
        # (paced by whichever part of the weapon moves furthest at each step: the muzzle, the butt, or a long
        # launcher's middle, and the grips the hands are on)
        P = fit.wpts
        cuts = np.linspace(P[:, 0].min(), P[:, 0].max(), 8)
        marks = [P[(P[:, 0] >= a_) & (P[:, 0] <= b_)].mean(0) for a_, b_ in zip(cuts[:-1], cuts[1:])
                 if ((P[:, 0] >= a_) & (P[:, 0] <= b_)).any()]
        marks += [ref.hand_w[s_][1] for s_ in ("l", "r")]
        marks = np.array(marks)
        pts = []
        for u in us:
            Wd = peek_design(fit, Wc, u)
            pts.append(Wd[1] + marks @ Wd[0].T)
        seg = np.linalg.norm(np.diff(np.array(pts), axis=0), axis=2).max(1)
        ss = np.concatenate([[0.0], np.cumsum(seg)])
        fit._peek_arc = (us, ss / max(ss[-1], 1e-9), (us, gs))
    return fit._peek_arc


GUARD_STEP = 0.01         # (the guard's angle worked out this often over the clip)
GUARD_HOLD = 0.10         # (each angle held this long either side, then eased over GUARD_EASE: never less than the
GUARD_EASE = 0.08         # least that clears, and no faster than the rest of the move)


def guard_profile(fit, L=3.0):
    """peek_cover's guard against the cover: the least lift (about the butt) that clears it, worked out along the
    clip's own path, widened by GUARD_HOLD and eased, so the muzzle rises over the top early and smoothly rather than
    riding down the cover's face."""
    if getattr(fit, "_guard", None) is None:
        ts = np.arange(0.0, L + 1e-9, GUARD_STEP)
        g = np.array([wall_angle(fit, peek_pose(fit, t, L, guard=None)[2]["Wd"]) if 1e-9 < t < L - 1e-9 else 0.0
                      for t in ts])
        k = int(round(GUARD_HOLD / GUARD_STEP))
        gd = np.array([g[max(0, i - k):i + k + 1].max() for i in range(len(g))])
        e = int(round(GUARD_EASE / GUARD_STEP))
        gs = np.array([gd[max(0, i - e):i + e + 1].mean() for i in range(len(gd))])
        # (eased in from nothing at the clip's ends, where the aim itself clears the cover)
        nz = np.where(g > 0.0)[0]
        if len(nz):
            a_, b_ = ts[nz[0]], ts[nz[-1]]
            ramp = np.array([sm(t / max(a_, 1e-6)) * sm((L - t) / max(L - b_, 1e-6)) for t in ts])
            gs = gs * ramp
        fit._guard = (ts, np.maximum(gs, g))
    return fit._guard


def peek_body(fit, lq, mv, ref, duck, rise, yaw):
    """The trunk and head: `duck` (0..) sits him back on his heel, bends him forward and bows his head; `rise` (0..)
    lifts his hips over the knee, straightens his back and lifts his head level; `yaw` turns his head."""
    man = fit.man
    lq, mv = kneel_hip(fit, lq, mv, ref, -DUCK_SINK * duck + 0.035 * rise)
    # (bent and bowed toward the cover, about the world's side to side, not each bone's own: his trunk is turned on
    # the aim, so a bow about its own axes would carry his head off to his right, over the rifle)
    G, X = man.fk(lq, mv)
    side = np.array((0.0, 1.0, 0.0))
    for b, f in (("spine_01", 0.3), ("spine_02", 0.35), ("chest", 0.35)):
        bend(man, lq, b, G[man.b(b)].T @ side, (DUCK_BEND * duck - 6.0 * rise) * f)
    bend(man, lq, "neck", G[man.b("neck")].T @ side, DUCK_NECK * duck - 22.0 * rise)
    bend(man, lq, "head", G[man.b("head")].T @ side, DUCK_HEAD * duck - 12.0 * rise)
    # (the head's cant onto the stock taken out as he lifts it)
    bend(man, lq, "head", (1, 0, 0), 0.0)
    for b, f in (("neck", 0.45), ("head", 0.55)):
        bend(man, lq, b, (0, 0, 1), yaw * f)
    return lq, mv


def solve_scale(fn, want, lo=0.0, hi=2.0, n=22):
    """The scale s in [lo, hi] at which fn(s) meets want (fn monotonic)."""
    flo, fhi = fn(lo), fn(hi)
    if (flo - want) * (fhi - want) > 0:
        return lo if abs(flo - want) < abs(fhi - want) else hi
    for _ in range(n):
        mid = 0.5 * (lo + hi)
        fm = fn(mid)
        if (fm - want) * (flo - want) > 0:
            lo, flo = mid, fm
        else:
            hi = mid
    return 0.5 * (lo + hi)


def peek_levels(fit):
    """The duck and the rise for this man: his helmet's top at DUCK_TOP ducked, his eyes at PEEK_EYE peeking."""
    if getattr(fit, "_peek", None) is None:
        ref = fit.base["fire_kneel"]
        man = fit.man

        def top_at(s):
            lq, mv = peek_body(fit, ref.lq.copy(), ref.mv.copy(), ref, s, 0.0, 0.0)
            return head_top(fit, *man.fk(lq, mv))

        def eye_at(s):
            lq, mv = peek_body(fit, ref.lq.copy(), ref.mv.copy(), ref, 0.0, s, 0.0)
            return eye_z(fit, *man.fk(lq, mv))
        fit._peek = (solve_scale(top_at, DUCK_TOP, 0.0, 1.8), solve_scale(eye_at, PEEK_EYE, 0.0, 2.2))
    return fit._peek


def peek_trunk(fit, t):
    """peek_cover's trunk and head at t (the weapon and arms aside), and how far he is ducked (0..1)."""
    ref = fit.base["fire_kneel"]
    dk, rk = peek_levels(fit)
    T = PEEK_T
    ez = lambda x: trap(x, 0.25)
    duck = kf([(T["duck"][0], 0.0), (T["duck"][1], 1.0), (T["rise"][0], 1.0), (T["rise"][1] - 0.04, 0.0),
               (T["duck2"][0], 0.0), (T["duck2"][1], 1.0), (T["back"][0], 1.0), (T["back"][1], 0.0)], t, ez)
    rise = kf([(T["rise"][0] + 0.06, 0.0), (T["rise"][1], 1.0), (T["scan"][1], 1.0), (T["duck2"][1] - 0.04, 0.0)], t,
              ez)
    s0, s1 = T["scan"]
    yaw = kf([(s0, 0.0), (s0 + 0.10, -26.0), (s0 + 0.13, -26.0), (s1 - 0.04, 24.0), (s1, 24.0),
              (T["duck2"][1] - 0.10, 0.0)], t)
    lq, mv = peek_body(fit, ref.lq.copy(), ref.mv.copy(), ref, dk * duck, rk * rise, yaw)
    return lq, mv, duck


def peek_weapon(fit, G, X, duck, low):
    """The weapon's place (before the guard) `low` of the way from the aim to the low carry, on the body G, X."""
    ref = fit.base["fire_kneel"]
    Wc, rel_c = cover_rifle(fit, ref, G, X)
    # (the aim it leaves and comes back to is the base's own, in the world: the weapon does not dip with his trunk as
    # he ducks, so it moves on its own even path)
    Wd = peek_design(fit, Wc, low)
    if fit.kind == "launcher":
        # (the launcher's tube is before his face while it is high: pushed out off his bowed head)
        Gc = G[fit.man.b("chest")]
        Wd = (Wd[0], Wd[1] + Gc @ np.array((LAUNCHER_OUT, 0.0, 0.0)) * duck * math.sin(math.pi * min(low, 0.5)))
    return Wd, rel_c


PACE_EASE = 0.16          # (the weapon's way down and up: its speed rises over the first 0.16 of the time, falls over
PACE_N = 60               # the last 0.16, and holds between, measured on whichever part of it moves furthest)


def peek_pace(fit, L=3.0):
    """How far along the way (`low`) the weapon is through the "low" and "up" beats, worked out on the clip's own
    moving body so that the part of the weapon (or a hand on it) that moves furthest goes at an even pace, eased in
    and out (PACE_EASE): the body's own sinking and rising is counted with the weapon's swing."""
    if getattr(fit, "_pace", None) is None:
        man = fit.man
        ref = fit.base["fire_kneel"]
        P = fit.wpts
        cuts = np.linspace(P[:, 0].min(), P[:, 0].max(), 8)
        marks = [P[(P[:, 0] >= a_) & (P[:, 0] <= b_)].mean(0) for a_, b_ in zip(cuts[:-1], cuts[1:])
                 if ((P[:, 0] >= a_) & (P[:, 0] <= b_)).any()]
        marks = np.array(marks + [ref.hand_w[s_][1] for s_ in ("l", "r")])
        out = {}
        for phase, (a, b) in (("low", (0.0, 1.0)), ("up", (1.0, 0.0))):
            t0, t1 = peek_beats(fit)[phase]
            taus = np.linspace(0.0, 1.0, PACE_N + 1)
            body = []
            for tau in taus:
                lq, mv, duck = peek_trunk(fit, t0 + tau * (t1 - t0))
                body.append((*man.fk(lq, mv), duck))

            def pts(i, low):
                G, X, duck = body[i]
                Wd = peek_weapon(fit, G, X, duck, low)[0]
                return Wd[1] + marks @ Wd[0].T
            want = np.array([trap(x, PACE_EASE) for x in taus])

            def march(k):
                lows, cur = [a], pts(0, a)
                for i in range(1, len(taus)):
                    d = k * (want[i] - want[i - 1])
                    lo_, hi_ = lows[-1], b
                    if np.linalg.norm(pts(i, lo_) - cur, axis=1).max() >= d:
                        nxt = lo_
                    else:
                        for _ in range(14):
                            mid = 0.5 * (lo_ + hi_)
                            if np.linalg.norm(pts(i, mid) - cur, axis=1).max() < d:
                                lo_ = mid
                            else:
                                hi_ = mid
                        nxt = 0.5 * (lo_ + hi_)
                    lows.append(nxt)
                    cur = pts(i, nxt)
                return np.array(lows)
            # (the whole way's length: the least k that gets all the way there)
            klo, khi = 0.0, 4.0
            for _ in range(14):
                km = 0.5 * (klo + khi)
                if abs(march(km)[-1] - b) < 1e-3:
                    khi = km
                else:
                    klo = km
            lows = march(khi)
            lows[-1] = b
            out[phase] = (t0 + taus * (t1 - t0), lows)
        fit._pace = out
    return fit._pace


def peek_low(fit, t):
    """How far along the way from the aim to the low carry the weapon is at t."""
    T = peek_beats(fit)
    pace = peek_pace(fit)
    if t <= T["low"][0] or t >= T["up"][1]:
        return 0.0
    if t <= T["low"][1]:
        return float(np.interp(t, *pace["low"]))
    if t >= T["up"][0]:
        return float(np.interp(t, *pace["up"]))
    return 1.0


def peek_pose(fit, t, L=3.0, guard=True):
    man = fit.man
    ref = fit.base["fire_kneel"]
    if t <= 1e-9 or t >= L - 1e-9:
        return ref.lq.copy(), ref.mv.copy(), {"on": {"l": ref.hand_w["l"], "r": ref.hand_w["r"]}, "W": (ref.Gw, ref.Xw)}
    T = peek_beats(fit)
    lq, mv, duck = peek_trunk(fit, t)
    G, X = man.fk(lq, mv)
    if fit.kind == "none":
        # (unarmed: his left hand stays on his left knee and his right by his right thigh, each carried on its thigh
        # as he sits back and rises)
        lq, mv, W, on = two_hands(fit, lq, mv, ref, None, keep={"l": "thigh_l", "r": "thigh_r"})
        lq, mv = keep_bolt(fit, lq, mv, ref)
        return lq, mv, {"on": on, "W": W}
    # (the weapon: off the aim and down before his knees (carried from his chest to his pelvis) as he ducks, the muzzle
    # lifted on the way so it comes back off the cover's top before it goes down; held there; back up the same way
    # as he comes up, levelled over the top onto the aim last; every key guarded against the cover)
    # (the weapon's way down and up is paced on the part of it that moves furthest: peek_pace)
    low = peek_low(fit, t)
    Wd, rel_c = peek_weapon(fit, G, X, duck, low)
    Gc = G[man.b("chest")]
    rel = {s: rel_blend(ref.hand_w[s], rel_c[s], low) for s in ("l", "r")}
    if guard is None:
        return lq, mv, {"Wd": Wd}
    if guard:
        ts_, gs_ = guard_profile(fit, L)
        ang = float(np.interp(t, ts_, gs_))
        ax = lift_axis(Wd)
        if ang > 1e-6 and ax is not None:
            Wd = about_point(Wd[0], Wd[1], R_axis(ax, ang), butt_of(fit, Wd))
    # (the left elbow out to his left and down while the rifle is low, so the forearm clears his hip and thigh: as far
    # out as the rifle is on its way down, so the forearm is out by the time it passes his knee)
    Gpe = G[man.b("pelvis")]
    base_pole = Gc @ ref.arm_ref["l"][3]
    poles = {"l": unit(base_pole * (1 - low) + Gpe @ unit(COVER_POLE_L) * low)}
    lq, mv, W, on = two_hands(fit, lq, mv, ref, Wd, anchor="r", rel=rel, poles=poles)
    lq, mv = keep_bolt(fit, lq, mv, ref)
    return lq, mv, {"on": on, "W": W}


MAKERS = {
    "reload_kneel": lambda fit, t: reload_pose(fit, "fire_kneel", t),
    "reload_prone": lambda fit, t: reload_pose(fit, "fire_prone", t),
    "signal_go": signal_go_pose,
    "signal_down": signal_down_pose,
    "look_back_prone": lambda fit, t: look_back_pose(fit, "fire_prone", t),
    "look_back_kneel": lambda fit, t: look_back_pose(fit, "fire_kneel", t),
    "peek_cover": peek_pose,
}


def keep_small_turns(man, tracks, poses):
    """field_clips.bake leaves out a rotation track whose w stays within 1e-4 of 1 (turns under about 1.6 degrees);
    a base clip may hold a bone (the medic's bag) turned that little, so such a track is put back to end on the base
    exactly."""
    have = {tr["part"]: tr for tr in tracks}
    for i in range(man.nb):
        if man.par[i] is None or ("rot" in have.get(i, {})):
            continue
        keys, prev = [], None
        for t, lq, mv in poses:
            q = quat(lq[i])
            if prev is not None and float(q @ prev) < 0:
                q = -q
            prev = q
            keys.append([round(t, 4)] + [round(float(v), 5) for v in q])
        if any(max(abs(v) for v in k[2:]) > 1e-5 for k in keys):
            if i not in have:
                have[i] = {"part": i, "part_name": man.rig.names[i]}
                tracks.append(have[i])
            have[i]["rot"] = keys
    tracks.sort(key=lambda tr: tr["part"])
    for tr in tracks:
        if "rot" in tr and "move" in tr:
            tr["move"] = tr.pop("move")          # (rot before move, as bake writes them)
    return tracks


def steady_whip(fit, lq, mv, ref, k=WHIP_STEADY):
    """The radio man's whip aerial (whip_1..4 on his chest, 2.4 m long): kept mostly as upright as in the base, so
    its tip sways rather than sweeping metres as his trunk turns and ducks."""
    man = fit.man
    if "whip_1" not in man.ix:
        return lq
    G, X = man.fk(lq, mv)
    w = man.b("whip_1")
    lq[w] = G[man.par[w]].T @ qslerp(G[w], ref.G[w], k)
    return lq


# ================================================================================================ what each clip says
def about_of(fit, name, extra):
    w = {"bolt": "bolt-action rifle", "mag": "weapon (magazine-fed)", "none": "hands (he is unarmed)",
         "launcher": "launcher"}[fit.kind]
    base = CLIPS[name][1]
    chain = f"Chain: {base} -> {name} -> {base} (it starts and ends on {base}'s first pose)."
    if name in RELOADS:
        T = reload_times(fit, base)
        lying = name == "reload_prone"
        f2 = lambda a: f"{a:.2f}"
    if name in RELOADS and fit.kind == "bolt":
        return (f"Reloading the bolt-action rifle {'lying' if lying else 'kneeling'}: the rifle comes down off the aim "
                f"and is canted (right side up){', rolled toward him in front of his face' if lying else ', over his left thigh'}"
                "; his left hand keeps the fore-end and carries the weapon bone throughout. The right hand lifts the "
                f"bolt handle and draws the bolt back (the `bolt` bone, {f2(T['bolt_up'][0])}-{f2(T['bolt_up'][1])} s), "
                "goes to the right front pouch "
                f"(pouch_r{'' if 'pouch_r' in fit.man.ix else ': this file has no pouch bones, the pouch on the belt'})"
                f"{', his hips rolled onto his left side to reach it' if lying else ''} ({f2(T['pouch'][0])}-"
                f"{f2(T['pouch'][1])} s), brings a clip of rounds up and presses them down into the magazine twice "
                f"(event `reload` at {f2(T['reload'])} s), closes the bolt ({f2(T['bolt_home'][0])}-"
                f"{f2(T['bolt_home'][1])} s, event `bolt`) and takes the grip again; the rifle is back on the aim at "
                "2.6 s. Props: `rounds` gives where the clip of rounds is (root frame, the game's axes) while it shows "
                "(in his fist from the pouch until its rounds are pressed down into the magazine): draw a small clip "
                "there. " + chain)
    if name in RELOADS:
        return (f"Changing the magazine {'lying' if lying else 'kneeling'}: the weapon comes down off the aim and is "
                "canted (left side up, the magazine toward his right hand); his left hand keeps the fore-end and "
                "carries the weapon bone throughout. The right hand takes the magazine and draws it out "
                f"({f2(T['to_bolt'][0])}-{f2(T['bolt_up'][1])} s), goes to the right front pouch "
                f"(pouch_r{', his hips rolled onto his left side to reach it' if lying else ''}, "
                f"{f2(T['pouch'][0])}-{f2(T['pouch'][1])} s), brings a full one up under the well and seats it, "
                f"pressed twice (event `reload` at {f2(T['reload'])} s), pulls the charging handle on the right of "
                f"the receiver (event `bolt`, {f2(T['bolt_home'][0])}-{f2(T['bolt_home'][1])} s) and takes the grip "
                "again; back on the aim at 2.6 s. The magazine is part of the weapon mesh and stays seated: `rounds` "
                "gives where a stand-in magazine could be drawn in his right fist (root frame) from the pouch to the "
                "well. The `bolt` bone is not this weapon's bolt and is left as the base clip keeps it"
                + (" (on this file it carries a prop on his body, kept where fire_kneel has it)" if fit.bolt_prop
                   else "") + ". " + chain)
    far = fit.kind in ("none", "launcher")
    home = {"none": "his side", "launcher": "the launcher", "bolt": "the fore-end", "mag": "the fore-end"}[fit.kind]
    held = ("his right hand keeps the launcher on his shoulder" if fit.kind == "launcher" else
            "unarmed, his right hand stays by his hip (the weapon bone rides it)" if fit.kind == "none" else
            "his " + w + " in his right hand only (the weapon bone rides that hand as it holds it in idle)")
    if name == "signal_go":
        B = go_beats(fit)
        pt = (f"point the way ({B['point']:.2f}-{B['hold']:.2f} s, pushed on at {B['push']:.2f} s; `point_dir`: the "
              "way he points over that hold, shoulder to wrist, degrees to his left of his facing and up, the mean "
              "and the range)")
        if far:
            return ("Standing, the arm signal to move on: " + held + "; his left hand from " + home + ", up beside "
                    f"his shoulder as he glances back at the men behind (by {B['back']:.2f} s), then swept forward and "
                    "up to " + pt + ", and back to " + home + " by 1.58 s. " + chain)
        return ("Standing, the arm signal to move on: " + held + ", his left hand off the fore-end, swung out and "
                f"back behind his shoulder as he glances back at the men behind (by {B['back']:.2f} s), then swept "
                "forward and up to " + pt + ", and back onto the fore-end by 1.58 s. " + chain)
    if name == "signal_down":
        DT = DOWN_T["far" if far else "near"]
        pr = " and ".join(f"{a:.2f}-{b:.2f}" for a, b in DT["press"]) + " s"
        return ("Standing, the arm signal to get down: " + held + "; his left arm out before him and to his left, the "
                f"hand flat, palm down, pressed down twice ({pr}) as he looks along the line to his left; back to "
                f"{home} by {DT['keys'][3]:.2f} s. " + chain)
    if name.startswith("look_back"):
        lying = name.endswith("prone")
        turn = extra.get("turn_from_hips", LOOK_TURN_PRONE if lying else LOOK_TURN)
        return (f"{'Lying' if lying else 'Kneeling'} on the aim, a look back over his right shoulder: the turn "
                f"({turn:.0f} degrees of head turn from his hips at the most, as baked (`head_turn_deg`), shared down "
                f"the spine, neck and head; never more than {HEAD_MOST:.0f} on his chest, what his aim already has "
                "counted) by 0.7 s, held with a small scan to 1.45 s, back on the aim by 2.05 s. "
                + (("His hands stay as the aim has them, carried round with his chest. " if lying else
                    "His hands stay where they rest, on his left knee and by his hip, the left lifted a little off "
                    "the knee as his shoulder comes round. ") if fit.kind == "none" else
                   "The " + w + " eased off his cheek and held in both hands (the left anchors it). ") + chain)
    if name == "peek_cover":
        T = peek_beats(fit)
        f2 = lambda a: f"{a:.2f}"
        lo_, hi_ = extra.get("top_suits", (None, None))
        return ("Kneeling behind a wall or sandbags about a metre high: from the aim he ducks "
                f"({f2(T['duck'][0])}-{f2(T['duck'][1])} s: sits back on his heel, bows his back and head; the top of "
                f"his helmet at {extra.get('duck_top', DUCK_TOP):.2f} m), holds, rises slowly "
                f"({f2(T['rise'][0])}-{f2(T['rise'][1])} s) to peek over it (his eyes at "
                f"{extra.get('peek_eye', PEEK_EYE):.2f} m), looks left then right ({f2(T['scan'][0])}-"
                f"{f2(T['scan'][1])} s), ducks again ({f2(T['duck2'][0])}-{f2(T['duck2'][1])} s), comes back up "
                f"({f2(T['back'][0])}-{f2(T['back'][1])} s) and is on fire_kneel's own aim at 3.0 s (his eyes at "
                f"{extra.get('aim_eye', 1.04):.2f} m). "
                + ("Behind cover his hands stay where they rest, his left on his left knee and his right by his "
                   "right thigh, each carried on its thigh. "
                   if fit.kind == "none" else
                   ("Behind cover the " + w + " is brought off the aim with its muzzle lifted back over the cover's "
                    "top, then down before his knees, held low in both hands ("
                    + ("the tube" if fit.kind == "launcher" else "it") + " across his thighs, its "
                    "muzzle swung to his left, short of the cover's face), and back up the same way, raised behind "
                    "the cover and levelled over its top onto the aim last "
                    f"({f2(T['low'][0])}-{f2(T['low'][1])} s down, {f2(T['up'][0])}-{f2(T['up'][1])} s up). "))
                + "`cover` gives the cover the clip is made for, in the root frame: its face `face_x` "
                f"({extra.get('face_x', 0.0):.2f} m, {COVER_BACK:.1f} m before his front knee at `knee_x`), and "
                f"`top_suits`, the cover tops it suits ({lo_:.2f}-{hi_:.2f} m: his helmet ducked at least "
                f"{DUCK_UNDER * 100:.0f} cm under the top, and the aim clearing it by {COVER_CLEAR * 100:.0f} cm "
                "where it passes the face, so he can fire from the aim it ends on). Nothing of him or the weapon "
                "goes into such a cover at any key. " + chain)
    return chain


# ================================================================================================ bake and check
def make_clips(fit, names=None):
    man = fit.man
    out = {}
    for name in names or CLIPS:
        L, base = CLIPS[name]
        if name in RELOADS and not fit.reloads:
            continue
        fn = MAKERS[name]
        infos = {}

        def pose(t, fn=fn, infos=infos, base=base):
            lq, mv, info = fn(fit, t)
            infos[round(t, 4)] = info
            return steady_whip(fit, lq, mv, fit.base[base]), mv
        tracks, poses = fc.bake(man, pose, L)
        tracks = keep_small_turns(man, tracks, poses)
        c = {"long": L, "loop": False, "stride": 0.0, "weapon": fit.D["clips"][base].get("weapon", "rifle"),
             "tracks": tracks, "events": [],
             "from": f"squad_life.py: keyed on this file's rig from {base}'s first pose (the file's own clip), "
                     "arms by two-bone IK onto the weapon, the bolt as this file's fire_kneel works it"
                     if name in RELOADS and fit.kind == "bolt" else
                     f"squad_life.py: keyed on this file's rig from {base}'s first pose (the file's own clip), "
                     "arms by two-bone IK onto the weapon"}
        extra = {}
        if name in RELOADS:
            T = reload_times(fit, base)
            c["events"] = [{"t": T["reload"], "fx": "reload", "socket": None, "cue": "reload"},
                           {"t": T["bolt_home"][0], "fx": "bolt", "socket": None, "cue": "bolt",
                            "until": T["bolt_home"][1]}]
            tr = [[t] + [round(float(v), 4) for v in infos[t]["rounds"]] for t in sorted(infos)
                  if infos[t].get("rounds") is not None]
            c["rounds"] = {"track_fields": ["t", "x", "y", "z"], "track": tr,
                           "what": "clip of rounds" if fit.kind == "bolt" else "magazine (stand-in)"}
        if name == "signal_go":
            # (the way he points, from his left shoulder to his left wrist, as the baked keys have it over the hold:
            # the mean, and the hold's span)
            B = go_beats(fit)
            ls, us_ = [], []
            for t, lq, mv in poses:
                if B["point"] - 1e-6 <= t <= B["hold"] + 1e-6:
                    G, X = man.fk(lq, mv)
                    v = X[man.b("hand_l")] - X[man.b("upperarm_l")]
                    ls.append(math.degrees(math.atan2(-v[1], v[0])))
                    us_.append(math.degrees(math.atan2(v[2], math.hypot(v[0], v[1]))))
            c["point_dir"] = {"left": round(float(np.mean(ls)), 1), "up": round(float(np.mean(us_)), 1),
                              "left_range": [round(min(ls), 1), round(max(ls), 1)],
                              "up_range": [round(min(us_), 1), round(max(us_), 1)],
                              "hold": [B["point"], B["hold"]]}
        if name == "peek_cover":
            tops, eyes = [], []
            for t, lq, mv in poses:
                G, X = man.fk(lq, mv)
                tops.append(head_top(fit, G, X))
                eyes.append(eye_z(fit, G, X))
            k1 = [i for i, (t, _, _) in enumerate(poses) if PEEK_T["duck"][1] <= t <= PEEK_T["rise"][0]]
            k2 = [i for i, (t, _, _) in enumerate(poses) if PEEK_T["rise"][1] <= t <= PEEK_T["scan"][1]]
            k3 = [i for i, (t, _, _) in enumerate(poses) if PEEK_T["duck2"][1] - 0.02 <= t <= PEEK_T["back"][0] + 0.04]
            ref = fit.base["fire_kneel"]
            wl = cover_of(fit)
            duck_top = max(max(tops[i] for i in k1), max(tops[i] for i in k3))
            extra = {"duck_top": duck_top, "peek_eye": float(np.mean([eyes[i] for i in k2])),
                     "aim_eye": eye_z(fit, ref.G, ref.X), **wl}
            lo_ = math.ceil((duck_top + DUCK_UNDER) * 100) / 100
            hi_ = math.floor(wl["top_hi"] * 100) / 100
            extra["top_suits"] = [lo_, hi_]
            c["cover"] = {"knee_to_face": COVER_BACK, "face_x": round(wl["face_x"], 3),
                          "knee_x": round(wl["knee_x"], 3), "duck_top": round(duck_top, 3),
                          "peek_eye": round(extra["peek_eye"], 3), "aim_eye": round(extra["aim_eye"], 3),
                          "top_suits": [lo_, hi_]}
        if name.startswith("look_back"):
            # (the head's turn from his hips as baked, the most over the clip, past what the base already has: the
            # about states this file's own figure, as HEAD_MOST may hold a turn short of LOOK_TURN)
            ref = fit.base[base]
            pe, hd = man.b("pelvis"), man.b("head")
            Rb0 = ref.G[pe].T @ ref.G[hd]
            turn = 0.0
            for t, lq, mv in poses:
                G, X = man.fk(lq, mv)
                turn = max(turn, abs(twist_z(Rb0.T @ (G[pe].T @ G[hd]))))
            extra["turn_from_hips"] = turn
            c["head_turn_deg"] = round(turn, 1)
        c["about"] = about_of(fit, name, extra)
        out[name] = c
    return out


def body_split(fit):
    """The near mesh's hand points and body points (no arms, no weapon), with their bones and rest facings."""
    if getattr(fit, "_split", None) is None:
        man = fit.man
        M = fit.D["near"]
        P = np.array(M["points"]).reshape(-1, 3)
        F = np.array(M["facings"]).reshape(-1, 3)
        a, b, w = np.array(M["part"]), np.array(M["part2"]), np.array(M["w2"], float)
        names = man.rig.names
        arm = {i for i, n in enumerate(names) if n.split("_")[0] in ("clavicle", "upperarm", "lowerarm", "hand",
                                                                       "fingers", "thumb")}
        gun = {man.b(n) for n in ("weapon", "bolt", "bipod")} | {i for i, n in enumerate(names) if n.startswith("whip")}
        hand = {s: [man.b(n + "_" + s) for n in ("hand", "fingers", "thumb")] for s in ("l", "r")}
        H = {s: np.isin(a, hand[s]) & (np.isin(b, hand[s]) | (w < 0.3)) for s in ("l", "r")}
        body = ~np.isin(a, list(arm | gun)) & ~np.isin(b, list(arm | gun))
        sub = np.where(body)[0][::2]
        legs = np.isin(a, [man.b(n) for n in ("thigh_l", "calf_l", "foot_l", "ball_l", "thigh_r", "calf_r", "foot_r",
                                                "ball_r")])
        low = {"feet": np.isin(a, [man.b(n) for n in ("foot_l", "ball_l", "foot_r", "ball_r")]),
               "knees": np.isin(a, [man.b("calf_l"), man.b("calf_r")]),
               "elbows": np.isin(a, [man.b("lowerarm_l"), man.b("lowerarm_r")]),
               "hips": np.isin(a, [man.b(n) for n in ("pelvis", "pouch_l", "pouch_r") if n in man.ix]),
               "chest": np.isin(a, [man.b("spine_02"), man.b("chest")]),
               "gun": np.isin(a, list(gun - {i for i, n in enumerate(names) if n.startswith("whip")}))}
        fit._split = (P, F, a, b, w, H, sub, low, legs)
    return fit._split


def hand_depth(fit, G, X):
    """How deep (m) any point of either hand lies inside his body (the nearest body point's facing says which side
    is in), and the hand it is."""
    P, F, a, b, w, H, sub, low, legs = body_split(fit)
    man = fit.man
    V = gore.skin(P, a, b, w, G, X, man.H)
    Bp = V[sub]
    Bn = np.einsum("nij,nj->ni", G[a[sub]], F[sub])
    worst = (0.0, None)
    for s in ("l", "r"):
        Hp = V[H[s]]
        if len(Hp) == 0:
            continue
        d2 = ((Hp[:, None, :] - Bp[None, :, :]) ** 2).sum(-1)
        k = d2.argmin(1)
        dist = np.sqrt(d2[np.arange(len(Hp)), k])
        inside = -np.einsum("ij,ij->i", Hp - Bp[k], Bn[k])
        inside[dist > 0.04] = 0.0
        m = float(inside.max())
        if m > worst[0]:
            worst = (m, s)
    return worst, V


def twist_z(R):
    """The turn of R about its own z (up the spine), degrees: swing-twist."""
    q = quat(R)
    return math.degrees(2 * math.atan2(q[3], q[0])) if abs(q[0]) + abs(q[3]) > 1e-9 else 0.0


# ---------------------------------------------------------------------------------------------------- solid checks
# (whether a point is inside his body is read off the body's surface itself: the winding number of the point about the
# mid mesh's body triangles (no arms, no weapon), about 1 inside and 0 outside; how deep, as the distance to the near
# mesh's nearest body point. The hand-only test above misses a forearm laid into his hip or a rifle butt in his side)
ARM_PARTS = ("upperarm", "lowerarm", "hand", "fingers", "thumb")


def winding(Q, A, B, C):
    """The winding number of each point of Q about the triangles (A, B, C): the solid angle each triangle spans seen
    from the point (two atan2 of the triangle's corners), summed, over the whole sphere's."""
    out = np.zeros(len(Q))
    for k in range(0, len(Q), 48):
        q = Q[k:k + 48, None, :]
        a, b, c = A[None] - q, B[None] - q, C[None] - q
        la, lb, lc = (np.linalg.norm(v, axis=2) for v in (a, b, c))
        det = np.einsum("ijk,ijk->ij", a, np.cross(b, c))
        dot = lambda u, v: np.einsum("ijk,ijk->ij", u, v)
        den = la * lb * lc + dot(a, b) * lc + dot(b, c) * la + dot(c, a) * lb
        out[k:k + 48] = np.arctan2(det, den).sum(1) / (2 * math.pi)
    return out


def solid_of(fit):
    """The near mesh's points by what they are (hand and forearm of each side, the weapon, the body, the knees, the
    feet, the torso) and the mid mesh's body surface, for the solid checks."""
    if getattr(fit, "_solid", None) is None:
        man, D = fit.man, fit.D
        names = man.rig.names
        arm_b = np.array([n.split("_")[0] in ARM_PARTS for n in names])
        gun_b = np.array([n in ("weapon", "bolt", "bipod") or n.startswith("whip") for n in names])

        def body_mask(M):
            a, b, w = np.array(M["part"]), np.array(M["part2"]), np.array(M["w2"], float)
            return ~arm_b[a] & ~gun_b[a] & ~(arm_b[b] & (w > 0.25)) & ~gun_b[b]
        N = D["near"]
        P = np.array(N["points"]).reshape(-1, 3)
        a, b, w = np.array(N["part"]), np.array(N["part2"]), np.array(N["w2"], float)
        body = body_mask(N)
        Mm = D.get("mid") or N
        Pm = np.array(Mm["points"]).reshape(-1, 3)
        am, bm, wm = np.array(Mm["part"]), np.array(Mm["part2"]), np.array(Mm["w2"], float)
        Tm = np.array(Mm["triangles"]).reshape(-1, 3)
        Tm = Tm[body_mask(Mm)[Tm].all(1)]
        ix = lambda ns: [man.b(n) for n in ns if n in man.ix]
        H = man.H
        near_joint = lambda bones, joints: np.isin(a, ix(bones)) & (np.min(
            [np.linalg.norm(P - H[man.b(j)], axis=1) for j in joints], axis=0) < 0.08)
        gun = np.isin(a, ix(("weapon", "bolt", "bipod")))
        fit._solid = {
            "P": P, "a": a, "b": b, "w": w, "body": body, "Pm": Pm, "am": am, "bm": bm, "wm": wm, "Tm": Tm,
            "arm": {s: np.isin(a, ix(("hand_" + s, "fingers_" + s, "thumb_" + s, "lowerarm_" + s))) for s in "lr"},
            "hand": {s: np.isin(a, ix(("hand_" + s, "fingers_" + s, "thumb_" + s))) for s in "lr"},
            # (the weapon itself: not a prop the bolt bone may carry on his body, as the loader's rocket)
            "gun": (a == man.b("weapon")) & ((b == man.b("weapon")) | (w < 0.5)),
            "knees": near_joint(("thigh_l", "calf_l", "thigh_r", "calf_r"), ("calf_l", "calf_r")),
            "elbows": near_joint(("upperarm_l", "lowerarm_l", "upperarm_r", "lowerarm_r"),
                                 ("lowerarm_l", "lowerarm_r")),
            "feet": np.isin(a, ix(("foot_l", "ball_l", "foot_r", "ball_r"))),
            "torso": np.isin(a, ix(("pelvis", "spine_01", "spine_02", "chest", "pouch_l", "pouch_r"))),
            "head": a == man.b("head"),
            "pouch": np.isin(a, [fit.pouch_bone]) & body,
            "still": ~gun & ~np.isin(a, [i for i, n in enumerate(names) if n.startswith("whip")]),
        }
    return fit._solid


def skin_both(fit, G, X):
    S = solid_of(fit)
    H = fit.man.H
    return (gore.skin(S["P"], S["a"], S["b"], S["w"], G, X, H), gore.skin(S["Pm"], S["am"], S["bm"], S["wm"], G, X, H))


def inside_depth(fit, V, Vm, sel, step=2):
    """How deep (m) the deepest of the points `sel` lies inside his body, and how many are inside."""
    S = solid_of(fit)
    Q = V[np.where(sel)[0][::step]]
    Bp = V[S["body"]]
    if len(Q) == 0 or len(Bp) == 0:
        return 0.0, 0
    lo, hi = Bp.min(0) - 0.01, Bp.max(0) + 0.01
    Q = Q[((Q > lo) & (Q < hi)).all(1)]
    if len(Q) == 0:
        return 0.0, 0
    d2 = ((Q[:, None, :] - Bp[None, ::3, :]) ** 2).sum(-1).min(1)
    Q = Q[d2 < 0.16 ** 2]
    if len(Q) == 0:
        return 0.0, 0
    T = S["Tm"]
    ins = np.abs(winding(Q, Vm[T[:, 0]], Vm[T[:, 1]], Vm[T[:, 2]])) > 0.5
    if not ins.any():
        return 0.0, 0
    Qi = Q[ins]
    dd = np.sqrt(((Qi[:, None, :] - Bp[None, :, :]) ** 2).sum(-1).min(1))
    return float(dd.max()), int(ins.sum())


def solid_numbers(fit, G, X, wall=None):
    """One pose's solid numbers: each arm (hand and forearm) and the weapon inside his body (cm), the lowest knee,
    elbow, foot, hand and torso points of the near mesh (cm), and with `wall` (face x, top z) how far into it any point
    goes (cm)."""
    S = solid_of(fit)
    V, Vm = skin_both(fit, G, X)
    r = {"arm_l": inside_depth(fit, V, Vm, S["arm"]["l"])[0] * 100,
         "arm_r": inside_depth(fit, V, Vm, S["arm"]["r"])[0] * 100,
         "gun": inside_depth(fit, V, Vm, S["gun"], step=3)[0] * 100 if fit.kind != "none" else 0.0}
    for k in ("knees", "elbows", "feet", "torso"):
        r[k] = float(V[S[k], 2].min()) * 100
    r["hands"] = float(V[S["hand"]["l"] | S["hand"]["r"], 2].min()) * 100
    if wall is not None:
        xf, top = wall
        m = (V[:, 0] > xf) & (V[:, 2] < top)
        r["wall"] = float(np.minimum(V[m, 0] - xf, top - V[m, 2]).max()) * 100 if m.any() else 0.0
    return r, V


def check_clip(fit, name, c):
    """The clip's numbers, read back from its baked keys (as the game will play them)."""
    man = fit.man
    L, base = CLIPS[name]
    ref = fit.base[base]
    P, F, a, b, w, H, sub, low, legs = body_split(fit)
    ts = times_of(L)
    G0, X0 = gore.pose(man.rig, c, 0.0)
    G1, X1 = gore.pose(man.rig, c, L)
    Gb, Xb = ref.G, ref.X
    keep = [i for i, n in enumerate(man.rig.names)]
    gap = max(float(np.linalg.norm(X0 - Xb, axis=1).max()), float(np.linalg.norm(X1 - Xb, axis=1).max()))
    turn_gap = max(max(angle_of(G0[i].T @ Gb[i]) for i in keep), max(angle_of(G1[i].T @ Gb[i]) for i in keep))
    mz = [s for s in fit.D["sockets"] if s["name"] == "socket_muzzle"]
    muz = np.array(mz[0]["at"]) - man.H[man.b("weapon")] if mz else np.zeros(3)
    prev = None
    jump = (0.0, None, None)
    groundb = {k: 9.0 for k in ("feet", "knees", "elbows")}
    groundm = {k: 9.0 for k in ("feet", "knees", "elbows")}
    hips = [9.0, -9.0]
    chest = [9.0, -9.0]
    gun_low = 9.0
    head = 0.0
    head_yaw = 0.0
    hips_turn = 0.0
    added = 0.0
    Rb_ = ref.G[man.b("chest")].T @ ref.G[man.b("head")]
    muz_jump = 0.0
    depth = (0.0, None, None)
    hand_gap = (0.0, None, None)
    # (the solid numbers: worst over the keys, with the time, against the base pose's own)
    wall = None
    if "cover" in c and c["cover"].get("face_x") is not None:
        wall = (c["cover"]["face_x"], c["cover"]["top_suits"][1])
    sol_b, _ = solid_numbers(fit, Gb, Xb, wall)
    worst_in = {k: (0.0, None) for k in ("arm_l", "arm_r", "gun", "wall")}
    worst_low = {k: (9e9, None) for k in ("knees", "elbows", "feet", "torso", "hands")}
    torso_high = -9e9
    mesh_jump = (0.0, None, None)
    prevV = None
    S = solid_of(fit)
    pouch_reach = (9.0, None)
    PT = reload_times(fit, base)["pouch"] if name in RELOADS else None
    for t in ts:
        G, X = gore.pose(man.rig, c, t)
        sol, Vn = solid_numbers(fit, G, X, wall)
        for k in worst_in:
            if k in sol and sol[k] > worst_in[k][0]:
                worst_in[k] = (sol[k], round(t, 3))
        for k in worst_low:
            if sol[k] < worst_low[k][0]:
                worst_low[k] = (sol[k], round(t, 3))
        torso_high = max(torso_high, sol["torso"])
        if prevV is not None:
            dm = np.linalg.norm(Vn - prevV, axis=1)
            dm[~S["still"]] = 0.0
            j = int(dm.argmax())
            if dm[j] > mesh_jump[0]:
                mesh_jump = (float(dm[j]), man.rig.names[S["a"][j]], round(t, 3))
        prevV = Vn
        if PT is not None and PT[0] - 0.2 <= t <= PT[1] + 0.2:
            at = X[fit.pouch_bone] + G[fit.pouch_bone] @ fit.pouch
            Hp = Vn[S["hand"]["r"]]
            near_p = S["pouch"] & (np.linalg.norm(Vn - at, axis=1) < 0.07)
            d = float(np.linalg.norm(Hp - at, axis=1).min())
            if near_p.any():
                d = min(d, float(np.sqrt(((Hp[:, None] - Vn[near_p][None]) ** 2).sum(-1)).min()))
            if d < pouch_reach[0]:
                pouch_reach = (d, round(t, 3))
        wi = man.b("weapon")
        pts = np.vstack([X, X[wi] + G[wi] @ muz])
        if prev is not None:
            d = np.linalg.norm(pts - prev, axis=1)
            muz_jump = max(muz_jump, float(d[-1]))
            d = d[:-1]
            if fit.bolt_prop:
                d[man.b("bolt")] = 0.0                 # (the hidden prop on the bolt bone is judged on its own)
            if fit.kind == "none":
                d[[man.b("weapon"), man.b("bolt"), man.b("bipod")]] = 0.0
            if d.max() > jump[0]:
                k = int(d.argmax())
                jump = (float(d.max()), man.rig.names[k], t)
        prev = pts
        for k_, bones in (("feet", ("foot_l", "foot_r", "ball_l", "ball_r")), ("knees", ("calf_l", "calf_r")),
                          ("elbows", ("lowerarm_l", "lowerarm_r"))):
            groundb[k_] = min(groundb[k_], min(float(X[man.b(n), 2]) for n in bones))
        (dp, side), V = hand_depth(fit, G, X)
        if dp > depth[0]:
            depth = (dp, side, t)
        for k_ in ("feet", "knees", "elbows"):
            groundm[k_] = min(groundm[k_], float(V[low[k_], 2].min()))
        hz = float(V[low["hips"], 2].min())
        hips = [min(hips[0], hz), max(hips[1], hz)]
        cz = float(V[low["chest"], 2].min())
        chest = [min(chest[0], cz), max(chest[1], cz)]
        if fit.kind != "none" and low["gun"].any():
            gun_low = min(gun_low, float(V[low["gun"], 2].min()))
        Rch = G[man.b("chest")].T @ G[man.b("head")]
        head = max(head, angle_of(Rch))
        head_yaw = max(head_yaw, abs(twist_z(Rch)))
        pe = man.b("pelvis")
        hips_turn = max(hips_turn, abs(twist_z((Gb[pe].T @ Gb[man.b("head")]).T @ (G[pe].T @ G[man.b("head")]))))
        added = max(added, abs(twist_z(Rb_.T @ Rch)))
        # (the hands that hold the weapon at t, against where the maker put them on it)
        info = MAKERS[name](fit, t)[2]
        if fit.kind != "none":
            for s, rel in info.get("on", {}).items():
                want = X[wi] + G[wi] @ rel[1]
                gg = float(np.linalg.norm(X[man.b("hand_" + s)] - want))
                if gg > hand_gap[0]:
                    hand_gap = (gg, s, t)
    # (the base's own numbers, for what the clip inherits at its ends)
    (bd, _), Vb = hand_depth(fit, Gb, Xb)
    base_ground = {k_: float(Vb[low[k_], 2].min()) for k_ in ("feet", "knees", "elbows")}
    base_rest = {k_: float(Vb[low[k_], 2].min()) for k_ in ("hips", "chest")}
    Rb = Gb[man.b("chest")].T @ Gb[man.b("head")]
    return {"long": c["long"], "loop": c["loop"], "weapon": c["weapon"], "keys": len(ts),
            "end_gap_cm": round(gap * 100, 2), "end_turn_deg": round(turn_gap, 2),
            "jump_cm": round(jump[0] * 100, 1), "jump_at": [jump[1], jump[2]],
            "bones_low_cm": {k_: round(v * 100, 1) for k_, v in groundb.items()},
            "mesh_low_cm": {k_: round(v * 100, 1) for k_, v in groundm.items()},
            "base_mesh_low_cm": {k_: round(v * 100, 1) for k_, v in base_ground.items()},
            "hips_low_cm": [round(hips[0] * 100, 1), round(hips[1] * 100, 1)], "chest_low_cm": [round(chest[0] * 100, 1), round(chest[1] * 100, 1)],
            "base_rest_cm": {k_: round(v * 100, 1) for k_, v in base_rest.items()},
            "weapon_low_cm": round(gun_low * 100, 1) if gun_low < 9 else None,
            "muzzle_jump_cm": round(muz_jump * 100, 1) if fit.kind != "none" else None,
            "head_on_chest_deg": round(head, 1), "head_turn_on_chest_deg": round(head_yaw, 1),
            "base_head_turn_on_chest_deg": round(abs(twist_z(Rb)), 1), "head_turn_from_hips_deg": round(hips_turn, 1),
            "head_turn_added_on_chest_deg": round(added, 1),
            "base_head_on_chest_deg": round(angle_of(Rb), 1),
            "hand_in_body_cm": round(depth[0] * 100, 1), "hand_in_body_at": [depth[1], depth[2]],
            "base_hand_in_body_cm": round(bd * 100, 1),
            "hand_rifle_gap_cm": round(hand_gap[0] * 100, 2), "hand_rifle_gap_at": [hand_gap[1], hand_gap[2]],
            "in_body_cm": {k: [round(v[0], 1), v[1], round(sol_b[k], 1)] for k, v in worst_in.items() if k != "wall"},
            "near_low_cm": {k: [round(v[0], 1), v[1], round(sol_b[k], 1)] for k, v in worst_low.items()},
            "torso_low_highest_cm": round(torso_high, 1),
            "mesh_jump_cm": [round(mesh_jump[0] * 100, 1), mesh_jump[1], mesh_jump[2]],
            **({"wall_cm": [round(worst_in["wall"][0], 1), worst_in["wall"][1]]} if wall else {}),
            **({"pouch_reach_cm": [round(pouch_reach[0] * 100, 1), pouch_reach[1]]} if PT else {})}


def check_all(fit, clips, log=print):
    rep = {}
    for name, c in clips.items():
        r = check_clip(fit, name, c)
        rep[name] = r
        log(f"   {fit.name} {name}: gap {r['end_gap_cm']} cm/{r['end_turn_deg']} deg, jump {r['jump_cm']} cm "
            f"({r['jump_at'][0]}; muzzle {r['muzzle_jump_cm']}), bones low {r['bones_low_cm']}, mesh low {r['mesh_low_cm']} "
            f"(base {r['base_mesh_low_cm']}), hips {r['hips_low_cm']}, chest {r['chest_low_cm']} (base "
            f"{r['base_rest_cm']}), gun low {r['weapon_low_cm']}, head "
            f"turn on chest {r['head_turn_on_chest_deg']} (base {r['base_head_turn_on_chest_deg']}, added "
            f"{r['head_turn_added_on_chest_deg']}), from hips "
            f"{r['head_turn_from_hips_deg']}, all {r['head_on_chest_deg']}, hand in body "
            f"{r['hand_in_body_cm']} (base {r['base_hand_in_body_cm']}) at {r['hand_in_body_at']}, hand-rifle "
            f"{r['hand_rifle_gap_cm']} cm at {r['hand_rifle_gap_at']}")
    return rep


# ================================================================================================ pictures
def paint_man(dr, V, T, cols, ox, oy, scale, yaw, pitch, centre, lines=(), boxes=()):
    """Flat-shaded, painter-sorted, orthographic: the camera at `yaw` degrees round from his front toward his left and
    `pitch` degrees up, looking at `centre` (drawn at ox, oy)."""
    y_, p_ = math.radians(yaw), math.radians(pitch)
    F = np.diag((1.0, -1.0, 1.0))                     # (drawn as seen: the json's y is to his right)
    eye = np.array((math.cos(p_) * math.cos(y_), math.cos(p_) * math.sin(y_), math.sin(p_)))
    fwd = -eye
    right = unit(np.cross(fwd, (0.0, 0.0, 1.0)))
    up = np.cross(right, fwd)
    c = np.asarray(centre, float) @ F
    Vw = V @ F
    sx = ox + ((Vw - c) @ right) * scale
    sy = oy - ((Vw - c) @ up) * scale
    dep = (Vw[T].mean(1) - c) @ fwd
    n = np.cross(Vw[T[:, 1]] - Vw[T[:, 0]], Vw[T[:, 2]] - Vw[T[:, 0]])
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    light = unit((0.4, -0.5, 0.75))
    lit = 0.5 + 0.5 * np.abs(n @ light)
    pr = lambda p: (ox + ((p @ F) - c) @ right * scale, oy - ((p @ F) - c) @ up * scale)
    items = [(dep[i], 0, i) for i in range(len(T))]
    for k, (A, B, col, wd) in enumerate(lines):
        items.append(((((np.asarray(A) + np.asarray(B)) / 2) @ F - c) @ fwd - 0.03, 1, k))
    faces = [(0, 1, 3, 2), (4, 5, 7, 6), (0, 1, 5, 4), (2, 3, 7, 6), (0, 2, 6, 4), (1, 3, 7, 5)]
    for k, (Q, col) in enumerate(boxes):
        for f in faces:
            items.append((((np.asarray(Q)[list(f)].mean(0) @ F) - c) @ fwd, 2, (k, f)))
    items.sort(key=lambda z: -z[0])
    for _, kind, it in items:
        if kind == 0:
            tri = T[it]
            col = cols[tri[0]]
            dr.polygon([(float(sx[j]), float(sy[j])) for j in tri], fill=tuple(int(min(255, v * lit[it])) for v in col))
        elif kind == 1:
            A, B, col, wd = lines[it]
            dr.line([pr(np.asarray(A)), pr(np.asarray(B))], fill=col, width=wd)
        else:
            k, f = it
            Q, col = boxes[k]
            dr.polygon([pr(np.asarray(Q)[j]) for j in f], fill=col, outline=(90, 84, 70))


def box(lo, hi):
    lo, hi = np.asarray(lo, float), np.asarray(hi, float)
    return np.array([[x, y, z] for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])])


PIC_FRAMES = {"reload_kneel": (0.0, 0.42, 0.75, 0.94, 1.09, 1.45, 1.85, 2.2, 2.6),
              "reload_prone": (0.0, 0.4, 0.7, 1.02, 1.16, 1.5, 1.9, 2.2, 2.6),
              "signal_go": (0.0, 0.3, 0.55, 0.8, 1.05, 1.15, 1.4, 1.6),
              "signal_down": (0.0, 0.4, 0.72, 0.9, 1.07, 1.45, 1.8),
              "look_back_prone": (0.0, 0.4, 0.7, 1.0, 1.3, 1.8, 2.2),
              "look_back_kneel": (0.0, 0.4, 0.7, 1.0, 1.3, 1.8, 2.2),
              "peek_cover": (0.0, 0.4, 0.8, 1.2, 1.55, 1.95, 2.3, 2.5, 2.75, 3.0)}
PIC_VIEW = {"reload_kneel": (-55, 18), "reload_prone": (-60, 22), "signal_go": (-35, 8), "signal_down": (-25, 8),
            "look_back_prone": (-70, 18), "look_back_kneel": (-55, 10), "peek_cover": (-70, 8)}


def picture(fit, clips, path, cell=230):
    from PIL import Image, ImageDraw
    import gore_check
    man = fit.man
    D = fit.D
    M = D["near"]
    P = np.array(M["points"]).reshape(-1, 3)
    a, b, w = np.array(M["part"]), np.array(M["part2"]), np.array(M["w2"], float)
    T = np.array(M["triangles"]).reshape(-1, 3)
    pal = gore_check.palette(fit.name, D)
    shade = np.array(M.get("shade", [1.0] * len(P)), float)
    cols = [tuple(int(int(pal.get(p, "808080")[i:i + 2], 16) * (0.6 + 0.4 * sh)) for i in (0, 2, 4))
            for p, sh in zip(M["paints"], shade)]
    names = [n for n in CLIPS if n in clips]
    ncol = max(len(PIC_FRAMES[n]) for n in names)
    head = 40
    im = Image.new("RGB", (ncol * cell, head + len(names) * (cell + 18)), (232, 226, 214))
    dr = ImageDraw.Draw(im)
    dr.text((8, 6), f"{fit.name}: squad-life clips (squad_life.py), near mesh skinned by the staged clips' own keys. "
                    f"Weapon kind: {fit.kind}. Stand-ins: yellow = the clip of rounds / magazine in hand, grey box = "
                    "the cover peek_cover is made for (face 0.5 m before his front knee, the highest top it suits), "
                    "red line = where signal_go points (over the hold), brown line = the ground.",
            fill=(30, 30, 30))
    dr.text((8, 22), "Each row starts and ends on its base clip's first pose (fire_kneel, fire_prone or idle).",
            fill=(30, 30, 30))
    for r, name in enumerate(names):
        c = clips[name]
        yaw, pitch = PIC_VIEW[name]
        prone = "prone" in name
        kneel = name in ("reload_kneel", "look_back_kneel", "peek_cover")
        scale = cell / (2.3 if prone else 1.45 if kneel else 1.95)
        centre = (0.2, 0.0, 0.25) if prone else (0.05, 0.0, 0.62 if kneel else 0.92)
        for k, t in enumerate(PIC_FRAMES[name]):
            G, X = gore.pose(man.rig, c, t)
            V = gore.skin(P, a, b, w, G, X, man.H)
            lines, boxes = [], []
            lines.append(((-1.5, -0.6, 0.0), (1.6, -0.6, 0.0), (120, 96, 70), 1))
            lines.append(((-1.5, 0.6, 0.0), (1.6, 0.6, 0.0), (120, 96, 70), 1))
            if "rounds" in c:
                for row in c["rounds"]["track"]:
                    if abs(row[0] - t) < 0.034:
                        p = np.array(row[1:])
                        boxes.append((box(p - (0.012, 0.012, 0.025), p + (0.012, 0.012, 0.025)), (225, 190, 40)))
            if name == "peek_cover":
                # (the cover as the clip is made for it: its face COVER_BACK before his front knee (as the aim
                # kneels), its top the highest the clip suits)
                xf, top = c["cover"]["face_x"], c["cover"]["top_suits"][1]
                boxes.append((box((xf, -0.55, 0.0), (xf + 0.25, 0.6, top)), (150, 146, 136)))
            B = go_beats(fit)
            if name == "signal_go" and B["point"] - 0.01 <= t <= B["hold"] + 0.01:
                s_ = X[man.b("upperarm_l")]
                v = unit(X[man.b("hand_l")] - s_)
                lines.append((s_, s_ + v * 1.4, (200, 40, 30), 2))
            x0, y0 = k * cell, head + r * (cell + 18)
            # (each frame drawn on its own tile, so the cover's box and the ground lines stay in their own frame)
            tile = Image.new("RGB", (cell, cell + 18), (232, 226, 214))
            dt = ImageDraw.Draw(tile)
            paint_man(dt, V, T, cols, cell / 2, cell * (0.62 if prone else 0.55), scale, yaw, pitch, centre,
                      lines, boxes)
            dt.text((4, cell + 2), f"{name} {t:.2f}s", fill=(40, 34, 28))
            dt.line([(cell - 1, 0), (cell - 1, cell + 17)], fill=(205, 198, 184))
            im.paste(tile, (x0, y0))
    im.save(path)
    return path


# ================================================================================================ stage, check, deliver
def load(name):
    return json.load(open(SHARED + name + ".json"))


def stage_one(args):
    """One file staged: its clips made, written to the stage folder, checked (and pictured); returns its report and
    what it printed."""
    dirpath, nm, pictures = args
    lines = []
    t0 = time.time()
    D = load(nm)
    fit = Fit(nm, D)
    clips = make_clips(fit)
    with open(os.path.join(dirpath, nm + ".squad_life.json"), "w") as f:
        json.dump(clips, f, separators=(",", ":"))
    lines.append(" ".join([nm, f"kind {fit.kind}", f"{time.time() - t0:.1f} s", str(sorted(clips)),
                           f"(kept their own: {fit.has})" if fit.has else ""]))
    rep = {"kind": fit.kind, "kept": fit.has, "clips": check_all(fit, clips, log=lines.append)}
    if nm in pictures:
        os.makedirs(os.path.join(dirpath, "pictures"), exist_ok=True)
        lines.append("   picture " + picture(fit, clips, os.path.join(dirpath, "pictures", nm + "_squad_life.png")))
    return nm, rep, lines


def stage(dirpath, names, pictures=("soldier_GER", "soldier_SOV"), jobs=1):
    """Every file's clips made and checked into the stage folder (never the shared files), `jobs` files at a time."""
    os.makedirs(dirpath, exist_ok=True)
    report = {}
    work = [(dirpath, nm, pictures) for nm in names]
    if jobs > 1:
        from multiprocessing import Pool
        with Pool(jobs) as pool:
            done = list(pool.imap(stage_one, work))
    else:
        done = map(stage_one, work)
    for nm, rep, lines in done:
        for ln in lines:
            print(ln, flush=True)
        report[nm] = rep
    with open(os.path.join(dirpath, "squad_life_checks.json"), "w") as f:
        json.dump(report, f, indent=1)
    return report


def check_staged(dirpath, names):
    report = {}
    for nm in names:
        path = os.path.join(dirpath, nm + ".squad_life.json")
        if not os.path.exists(path):
            print(nm, "not staged")
            continue
        D = load(nm)
        fit = Fit(nm, D)
        clips = json.load(open(path))
        for k in clips:
            if k in D["clips"]:
                print(f"   {nm}: the file already has {k}: --deliver will leave the file's own")
        report[nm] = {"kind": fit.kind, "kept": fit.has, "clips": check_all(fit, clips)}
    with open(os.path.join(dirpath, "squad_life_checks.json"), "w") as f:
        json.dump(report, f, indent=1)
    return report


def pictures_of(dirpath, names):
    os.makedirs(os.path.join(dirpath, "pictures"), exist_ok=True)
    for nm in names:
        D = load(nm)
        fit = Fit(nm, D)
        clips = json.load(open(os.path.join(dirpath, nm + ".squad_life.json")))
        print(picture(fit, clips, os.path.join(dirpath, "pictures", nm + "_squad_life.png")))


def deliver(dirpath, names):
    """NOT RUN in this pass. Each shared file read fresh, backed up, the staged clips added (never one the file has),
    written whole through a temporary file and os.replace, and read back."""
    os.makedirs(BACKUP, exist_ok=True)
    for nm in names:
        add = json.load(open(os.path.join(dirpath, nm + ".squad_life.json")))
        path = SHARED + nm + ".json"
        raw = open(path).read()
        D = json.loads(raw)
        bk = os.path.join(BACKUP, nm + ".json")
        if not os.path.exists(bk):
            with open(bk, "w") as f:
                f.write(raw)
        new = {k: v for k, v in add.items() if k not in D["clips"]}
        D["clips"].update(new)
        tmp = path + ".tmp"
        with open(tmp, "w") as f:
            json.dump(D, f, separators=(",", ":"))
        os.replace(tmp, path)
        back = json.load(open(path))
        assert all(k in back["clips"] for k in new) and len(back["clips"]) == len(D["clips"])
        print("added", sorted(new), "to", nm, f"(kept the file's own {sorted(set(add) - set(new))})" if
              set(add) - set(new) else "")


if __name__ == "__main__":
    a = sys.argv[1:]
    jobs = 1
    if "--jobs" in a:
        i = a.index("--jobs")
        jobs = int(a[i + 1])
        a = a[:i] + a[i + 2:]
    if len(a) >= 2 and a[0] == "--stage":
        stage(a[1], a[2:] or INFANTRY, jobs=jobs)
    elif len(a) >= 2 and a[0] == "--check":
        check_staged(a[1], a[2:] or INFANTRY)
    elif len(a) >= 2 and a[0] == "--pictures":
        pictures_of(a[1], a[2:] or ["soldier_GER", "soldier_SOV"])
    elif len(a) >= 2 and a[0] == "--deliver":
        deliver(a[1], a[2:] or INFANTRY)
    else:
        print(__doc__)
