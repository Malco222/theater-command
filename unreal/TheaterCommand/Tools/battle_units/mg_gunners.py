"""Each army's section machine gunner (release 2, Brian, 25 November 14:15Z: "There should be a machine gun teams that
lay down suppressing fire, such as the German MG 42 and the US machine gun"): the army's rifleman file, with his rifle
taken out and the army's light machine gun (mg_guns.py) put in its place, his hands moved onto the gun in every clip,
and his own firing clips. The game reads `soldier_<ARMY>_lmg.json` for a section's gunner (role mg) when it is there,
and the American's `_bar` file otherwise (the ground thread's LandMen).

What changes, and nothing else:
- The rifle's points and triangles (every point on the weapon, bolt and bipod bones) are the gun's: written into the
  same places at each level (near, mid, far), so every other range in the file (the face's points, pack_tris,
  helmet_tris, the faces' tint_at, the gore twin's cuts and seams) still points where it did. Places the gun does not
  fill are parked inside the receiver (points) or drawn there too small to see (triangles).
- Their looks: per-point shade and shade_clean from the gun's own shadowing (holes and the bore dark), wet, snow,
  dirt (a little, more underneath) and dirt_wet; tint 1. Two paints of the gun's own (`new_paints`, `colours`).
- socket_muzzle at the gun's muzzle; top-level `weapon` "lmg" (LandMen then plays lmg_burst for a prone shot).
- Clips: in every clip where he holds the rifle (or the lmg), each hand that held the rifle holds the gun: the right
  fist round the pistol grip where it held the rifle's wrist, the other holds under the gun (the folded bipod, the
  receiver, the butt) at the same place along it, eased in and out as the hand comes to the gun and leaves it (the
  arm keyed again by two-bone reach, the elbow where it was). His fire_stand and fire_kneel fire bursts with his right
  fist on the grip throughout (no bolt to work), fire_prone is lmg_burst's hold slowed to 1.4 s, and in the clips where
  the rifle rides slung or lies by him the gun rides on his back with its drum outward, or lies on its right side.
- The gore twin takes the same gun in the same places, the same per-point looks, and the same hands in its own death
  clips.

    python3 mg_gunners.py --build ARMY [--out DIR]      write soldier_<ARMY>_lmg.json and its twin to DIR (default the
                                                         stage folder) and print the checks
    python3 mg_gunners.py --deliver ARMY [--content]    copy the staged pair into the shared folder (backup of anything
                                                         there, written whole, read back); with --content, into
                                                         Content/Models/Soldiers too
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
import field_clips as fc  # noqa: E402
import gore  # noqa: E402
import mg_guns  # noqa: E402

SHARED = "/mnt/project-files/battle-models/land/soldier/"
CONTENT = os.path.normpath(os.path.join(HERE, "..", "..", "Content", "Models", "Soldiers"))
STAGE = "/home/user/art/mg_gunners/out/"
BACKUP = "/home/user/art/looks/backup/mg_gunners/"
LEVELS = ("near", "mid", "far")
GUN_BONES = ("weapon", "bolt", "bipod")

ARMIES = {
    "GER": {"base": "soldier_GER", "out": "soldier_GER_lmg", "gun": "mg42",
            "name": "Section machine gunner, GER, MG 42 (our own design)",
            "date": ["1942-01", "1945-05"]},
    "GER_lmg34": {"base": "soldier_GER", "out": "soldier_GER_lmg34", "gun": "mg34",
                  "name": "Section machine gunner, GER, MG 34 (our own design)",
                  "date": ["1936-01", "1945-05"]},
}

WET = {"steel": 0.15, "bakelite": 0.22, "brass": 0.12, "bullet": 0.12}
SHADE_FLOOR = 0.40           # (the file's floor: the bore and the cooling holes)
SHADE_LOW = 0.56             # (the rifle's darkest shadowed steel, so the gun sits in the same range)

# ---- where a fist holds the gun, in the weapon bone's frame (mg_guns.py's own measures)
HOLD_NEAR, HOLD_FAR = 0.030, 0.060     # (a fist this near the rifle holds it; this far, has let go; eased between)
WRIST_X = (-0.13, 0.07)                # (the rifle's wrist, where his right fist holds it)
DRUM_X = (0.05, 0.17)                  # (the belt drum hangs on the left here: a supporting hand goes just ahead of it)
# (the rifle's underside along it, as the file's rifle has it: the butt, the wrist and trigger guard, the fore-end and
# barrel. A supporting fist keeps its height over this line on the gun's own underside.)
RIFLE_UNDER = [(-0.33, -0.085), (-0.20, -0.078), (-0.13, -0.050), (-0.10, -0.044), (0.06, -0.044), (0.09, 0.006),
               (0.80, 0.006)]


def lerp_line(pts, x):
    xs = [p[0] for p in pts]
    zs = [p[1] for p in pts]
    return float(np.interp(x, xs, zs))


# ================================================================================================ the gun's points
def blocks(D):
    """Each level's run of gun points and of the triangles drawn on them (one run each, asserted)."""
    names = [b["name"] for b in D["bones"]]
    gb = [names.index(n) for n in GUN_BONES if n in names]
    out = {}
    for L in LEVELS:
        part = np.array(D[L]["part"])
        on = np.where(np.isin(part, gb))[0]
        a, b = int(on[0]), int(on[-1]) + 1
        assert len(on) == b - a, f"{L}: the gun's points are not one run"
        T = np.array(D[L]["triangles"]).reshape(-1, 3)
        uses = np.any((T >= a) & (T < b), axis=1)
        tu = np.where(uses)[0]
        ta, tb = int(tu[0]), int(tu[-1]) + 1
        assert len(tu) == tb - ta, f"{L}: the gun's triangles are not one run"
        assert np.all((T[ta:tb] >= a) & (T[ta:tb] < b)), f"{L}: a gun triangle uses a body point"
        out[L] = (a, b, ta, tb)
    return out


def openness(P, N, T, rays=48, reach=0.20, seed=7):
    """How open each point is to the sky round its facing (0 shut in, 1 open): rays over the half round its facing,
    each tested against the gun's own triangles (Moller-Trumbore), cosine weighted. Also whether straight up is open."""
    rng = np.random.default_rng(seed)
    u = rng.random((rays, 2))
    r = np.sqrt(u[:, 0])
    th = 2 * math.pi * u[:, 1]
    loc = np.stack([r * np.cos(th), r * np.sin(th), np.sqrt(1 - u[:, 0])], 1)        # (cosine weighted, about +z)
    A, B, C = P[T[:, 0]], P[T[:, 1]], P[T[:, 2]]
    E1, E2 = B - A, C - A
    open_ = np.zeros(len(P))
    up_open = np.zeros(len(P))

    def hits(o, d):
        # o (3,), d (k,3) -> (k,) any hit within reach
        pv = np.cross(d[:, None, :], E2[None, :, :])
        det = np.einsum("tj,ktj->kt", E1, pv)
        ok = np.abs(det) > 1e-12
        inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
        tv = o[None, :] - A
        uu = np.einsum("tj,ktj->kt", tv, pv) * inv
        qv = np.cross(tv[None, :, :], E1[None, :, :])
        vv = np.einsum("kj,ktj->kt", d, qv) * inv
        tt = np.einsum("tj,ktj->kt", E2, np.broadcast_to(qv, pv.shape)) * inv
        h = ok & (uu >= 0) & (vv >= 0) & (uu + vv <= 1) & (tt > 2e-4) & (tt < reach)
        return h.any(1)

    for i in range(len(P)):
        n = N[i] / max(np.linalg.norm(N[i]), 1e-9)
        a = np.array((1.0, 0, 0)) if abs(n[0]) < 0.9 else np.array((0, 1.0, 0))
        e1 = np.cross(n, a)
        e1 /= np.linalg.norm(e1)
        e2 = np.cross(n, e1)
        d = loc[:, :1] * e1 + loc[:, 1:2] * e2 + loc[:, 2:] * n
        o = P[i] + n * 3e-4
        open_[i] = 1.0 - hits(o, d).mean()
        up_open[i] = 0.0 if hits(o, np.array([[0.0, 0.0, 1.0]]))[0] else 1.0
    return open_, up_open


def gun_looks(gun, L, new_paint):
    """The gun's points at level L in the file's terms: points (the weapon's frame), facings, paints (indices), bone
    names, and the per-point looks; and its triangles (outward, x y z)."""
    P, N, pnames, bones, dark, T = gun.mesh(L)
    opn, upo = openness(P, N, T)
    shade = np.where(dark, SHADE_FLOOR, SHADE_LOW + (1.0 - SHADE_LOW) * np.clip(opn, 0, 1) ** 0.85)
    paint_ix = {"steel": 9, "wood": 8, "straps": 4}
    paint_ix.update(new_paint)
    paints = [paint_ix[p] for p in pnames]
    wet = np.array([WET.get(p, 0.15) for p in pnames])
    snow = np.round(np.clip(N[:, 2], 0, 1) ** 1.5 * 0.55 * upo * np.where(dark, 0.0, 1.0), 2)
    dirt = np.round(np.clip(0.04 + 0.16 * np.clip(-N[:, 2], 0, 1), 0, 1), 2)
    dirt_wet = np.round(np.clip(dirt * 1.5, 0, 1), 2)
    return {"P": P, "N": N, "paints": paints, "bones": bones, "T": T, "shade": np.round(shade, 3),
            "wet": np.round(wet, 2), "snow": snow, "dirt": dirt, "dirt_wet": dirt_wet, "dark": dark}


def write_gun(D, gun, new_paint, blk):
    """The gun into the rifle's places in D (every level), with its looks. Returns what was written, per level."""
    names = [b["name"] for b in D["bones"]]
    wi = names.index("weapon")
    Hw = np.array(D["bones"][wi]["head"], float)
    rep = {}
    for L in LEVELS:
        a, b, ta, tb = blk[L]
        g = gun_looks(gun, L, new_paint)
        n, m = len(g["P"]), len(g["T"])
        assert n <= b - a and m <= tb - ta, f"{L}: the gun ({n} points, {m} triangles) is over the rifle's places"
        M = D[L]
        world = g["P"] + Hw
        park = b - a - n
        # (parked points: a little ring inside the receiver, on the weapon bone; the spare triangles drawn on them)
        cen = Hw + np.array((0.12, 0.0, mg_guns.BORE + 0.012))
        ring = [cen + 0.0015 * np.array((math.cos(2 * math.pi * k / max(park, 3)), 0.0,
                                         math.sin(2 * math.pi * k / max(park, 3)))) for k in range(park)]
        pts = list(world) + ring
        fac = list(g["N"]) + [np.array((0.0, 0.0, 1.0))] * park
        bone_ix = [names.index(bn) for bn in g["bones"]] + [wi] * park
        arrays = {
            "paints": list(g["paints"]) + [9] * park,
            "part": bone_ix, "part2": bone_ix, "w2": [0.0] * (b - a),
            "shade": list(g["shade"]) + [SHADE_FLOOR] * park,
            "shade_clean": list(g["shade"]) + [SHADE_FLOOR] * park,
            "dirt": list(g["dirt"]) + [0.0] * park, "dirt_wet": list(g["dirt_wet"]) + [0.0] * park,
            "wet": list(g["wet"]) + [0.15] * park, "snow": list(g["snow"]) + [0.0] * park,
        }
        Pf = np.array(M["points"], float).reshape(-1, 3)
        Ff = np.array(M["facings"], float).reshape(-1, 3)
        Pf[a:b] = np.array(pts)
        Ff[a:b] = np.array(fac)
        M["points"] = [round(float(v), 5) for v in Pf.ravel()]
        M["facings"] = [round(float(v), 4) for v in Ff.ravel()]
        for k, v in arrays.items():
            arr = list(M[k])
            arr[a:b] = [round(float(x), 4) if isinstance(x, (float, np.floating)) else int(x) for x in v]
            M[k] = arr
        tint = list(M["tint"])
        tint[3 * a:3 * b] = [1.0] * (3 * (b - a))
        M["tint"] = tint
        # (the files' winding: cross(b - a, c - a) against the facing)
        tris = [(a + i, a + k, a + j) for (i, j, k) in g["T"]]
        spare = (tb - ta) - m
        if spare:
            assert park >= 3, f"{L}: spare triangles but no spare points"
            for s in range(spare):
                i0 = a + n + (s % park)
                i1 = a + n + ((s + 1) % park)
                i2 = a + n + ((s + 2) % park)
                tris.append((i0, i1, i2))
        T = list(M["triangles"])
        T[3 * ta:3 * tb] = [int(v) for tr in tris for v in tr]
        M["triangles"] = T
        rep[L] = {"points": n, "parked": park, "triangles": m, "spare_triangles": spare,
                  "dark": int(g["dark"].sum())}
    return rep


def copy_gun(Tw, D, blk):
    """The twin takes the man's gun places as they are (its first points and the gun's triangles are the man's)."""
    for L in LEVELS:
        a, b, ta, tb = blk[L]
        for k in ("paints", "part", "part2", "w2", "shade", "shade_clean", "dirt", "dirt_wet", "wet", "snow"):
            arr = list(Tw[L][k])
            arr[a:b] = D[L][k][a:b]
            Tw[L][k] = arr
        for k, w in (("points", 3), ("facings", 3), ("tint", 3)):
            arr = list(Tw[L][k])
            arr[w * a:w * b] = D[L][k][w * a:w * b]
            Tw[L][k] = arr
        arr = list(Tw[L]["triangles"])
        arr[3 * ta:3 * tb] = D[L]["triangles"][3 * ta:3 * tb]
        Tw[L]["triangles"] = arr


# ================================================================================================ the hands
def unit(v):
    v = np.asarray(v, float)
    return v / max(np.linalg.norm(v), 1e-12)


def turn_between(a, b, f=1.0):
    """The least turn taking direction a toward b, `f` of the way."""
    a, b = unit(a), unit(b)
    ax = np.cross(a, b)
    s = np.linalg.norm(ax)
    c = float(np.clip(a @ b, -1, 1))
    if s < 1e-9:
        return np.eye(3)
    return fc.R_axis(ax / s, math.degrees(math.atan2(s, c)) * f)


class Fit:
    """Moves each hand that held the rifle onto the gun, clip by clip, on one file's rig."""

    def __init__(self, D_rifle, D_gun, gun):
        self.man = fc.Man(D_gun)
        self.gun = gun
        self.wi = self.man.b("weapon")
        old = fc.Man(D_rifle)
        self.rifle = old.weapon_pts - self.man.H[self.wi]       # (the rifle's near points, the weapon's frame)
        g = gun.grips
        self.pg = np.array(g["pistol"]["at"], float)
        self.pg_up = -unit(g["pistol"]["axis"])                  # (up the grip: where the thumb lies)
        und = []
        for (p0, p1) in g["under"]:
            und += [(float(p0[0]), float(p0[2])), (float(p1[0]), float(p1[2]))]
        und.sort()
        self.under = und
        self.log = []

    def near_rifle(self, p):
        return float(np.min(np.linalg.norm(self.rifle - p, axis=1)))

    @staticmethod
    def ease(d):
        if d <= HOLD_NEAR:
            return 1.0
        if d >= HOLD_FAR:
            return 0.0
        return 1.0 - fc.sm((d - HOLD_NEAR) / (HOLD_FAR - HOLD_NEAR))

    def target(self, s, p, thumb, force_grip=False, l_at=None):
        """Where fist `s` (at p, the weapon's frame, its thumb along `thumb`) goes on the gun: the weight it holds
        with, the fist's new place, and the turn of the hand (the weapon's frame). `l_at`: the supporting (left) fist
        goes to this place along the gun, at the depth under it it held the rifle."""
        w = 1.0 if force_grip else self.ease(self.near_rifle(p))
        if w <= 0.0:
            return 0.0, p, np.eye(3)
        if s == "r" and (force_grip or WRIST_X[0] <= p[0] <= WRIST_X[1]):
            q = self.pg
            Rl = turn_between(thumb, self.pg_up, w)
            return w, p + w * (q - p), Rl
        x = float(p[0])
        if s == "l" and l_at is not None:
            q = np.array((l_at, p[1], p[2] + lerp_line(self.under, l_at) - lerp_line(RIFLE_UNDER, x)))
            return w, p + w * (q - p), np.eye(3)
        dz = lerp_line(self.under, x) - lerp_line(RIFLE_UNDER, x)
        q = p + np.array((0.0, 0.0, dz))
        if s == "l" and DRUM_X[0] < x < DRUM_X[1]:
            q[0] = DRUM_X[1] + 0.01
            q[2] = p[2] + lerp_line(self.under, q[0]) - lerp_line(RIFLE_UNDER, x)
        return w, p + w * (q - p), np.eye(3)

    def refit(self, clip, force_r=False, sides=("l", "r"), wclip=None, poles=None, l_at=None):
        """The clip with each holding hand's arm keyed again (upperarm, lowerarm, hand), 15 a second: where each fist
        held the rifle is read from `clip`; the gun it goes to is `wclip`'s (the same clip with the gun's own keys
        changed: a burst's jump, a lift off the ground), else `clip`'s. Where a fist cannot reach its place on the gun
        (an arm hanging straight with the gun in one hand), the gun comes to the fist instead, by the miss. `poles`:
        where each elbow points from its shoulder (the world's frame), else where it was. Returns the clip (wclip's
        other keys), the worst miss of a fist that holds fully (m), and which hands hold at all."""
        man = self.man
        wclip = wclip or clip
        ts = fc.times_of(clip["long"])
        keys = {s: {b: [] for b in ("upperarm_", "lowerarm_", "hand_")} for s in sides}
        wkeys = ([], [])
        moved = False
        used = {s: False for s in sides}
        worst = 0.0
        prev = {}
        par = man.par[self.wi]
        for t in ts:
            lq0, mv0 = man.locals_of(clip, t)
            G0, X0 = man.fk(lq0, mv0)
            lqw, mvw = man.locals_of(wclip, t) if wclip is not clip else (lq0.copy(), mv0.copy())
            aims = {}
            for s in sides:
                A = man.side[s]
                h = man.b("hand_" + s)
                f = man.grip(G0, X0, s)
                p = G0[self.wi].T @ (f - X0[self.wi])
                thumb = G0[self.wi].T @ G0[h] @ A["t0"]
                w, q, Rl = self.target(s, p, thumb, force_grip=(force_r and s == "r"), l_at=l_at)
                if w > 0.0:
                    used[s] = True
                    aims[s] = (w, q, Rl)
            shift = np.zeros(3)
            for _round in range(3):
                lq, mv = lqw.copy(), mvw.copy()
                G, X = man.fk(lq, mv)
                if shift.any():
                    mv[self.wi] = mv[self.wi] + G[par].T @ shift
                    G, X = man.fk(lq, mv)
                Gw, Xw = G[self.wi], X[self.wi]
                miss = {}
                for s, (w, q, Rl) in aims.items():
                    A = man.side[s]
                    h = man.b("hand_" + s)
                    G, X = man.fk(lq, mv)
                    Gh = Gw @ Rl @ Gw.T @ G[h]
                    fw = Xw + Gw @ q
                    T = fw - Gh @ (A["d0"] * 0.075 + A["n0"] * 0.022)
                    pole = (np.asarray(poles[s], float) if poles and s in poles else
                            X[man.b("lowerarm_" + s)] - X[man.b("upperarm_" + s)])
                    lq = man.arm(lq, mv, s, T, pole, Gh @ A["d0"], Gh @ A["t0"])
                    G2, X2 = man.fk(lq, mv)
                    miss[s] = (w, man.grip(G2, X2, s) - fw)
                big = {s: m for s, m in miss.items() if np.linalg.norm(m[1]) > 0.006}
                if not big:
                    break
                wsum = sum(m[0] for m in miss.values())
                shift = shift + sum(m[0] * m[1] for m in miss.values()) / max(wsum, 1e-9)
            for s, (w, d) in miss.items() if aims else []:
                if w >= 0.999:
                    worst = max(worst, float(np.linalg.norm(d)))
            if shift.any():
                moved = True
            qw = fc.quat(lq[self.wi])
            if wkeys[0] and float(qw @ np.array(wkeys[0][-1][1:])) < 0:
                qw = -qw
            wkeys[0].append([round(t, 4)] + [round(float(v), 5) for v in qw])
            wkeys[1].append([round(t, 4)] + [round(float(v), 4) for v in mv[self.wi]])
            for s in sides:
                for bn in keys[s]:
                    i = man.b(bn + s)
                    qq = fc.quat(lq[i])
                    if i in prev and float(qq @ prev[i]) < 0:
                        qq = -qq
                    prev[i] = qq
                    keys[s][bn].append([round(t, 4)] + [round(float(v), 5) for v in qq])
        out = dict(clip)
        tracks = [dict(tr) for tr in wclip["tracks"]]
        if moved:
            tracks = [tr for tr in tracks if tr["part"] != self.wi]
            tracks.append({"part": self.wi, "part_name": "weapon", "rot": wkeys[0], "move": wkeys[1]})
        for s in sides:
            if not used[s]:
                continue
            for bn, ks in keys[s].items():
                i = man.b(bn + s)
                hit = [tr for tr in tracks if tr["part"] == i]
                if hit:
                    hit[0]["rot"] = ks
                else:
                    tracks.append({"part": i, "part_name": man.rig.names[i], "rot": ks})
        tracks.sort(key=lambda tr: tr["part"])
        out["tracks"] = tracks
        return out, worst, used

    # ---- the gun on his back, and on the ground
    def slung(self, clip):
        """The gun on his back (as Man.sling, the muzzle up by his right shoulder), its right side to his back so the
        drum is outward; the weapon bone keyed again 15 a second."""
        man = self.man
        Pw = man.weapon_pts - man.H[self.wi]
        lo, hi = Pw[:, 0].min(), Pw[:, 0].max()
        side = Pw[(Pw[:, 1] > 0) & (Pw[:, 1] < 0.03), 1].max()          # (the receiver's right face)
        a = unit((0.0, 0.38, 1.0))
        Rw = fc.frame_map(np.array((1.0, 0, 0)), np.array((0, 1.0, 0)), a, np.array((1.0, 0, 0)))
        ch = man.b("chest")

        def at(t):
            lq, mv = man.locals_of(clip, t)
            G, X = man.fk(lq, mv)
            Gc, Xc = G[ch], X[ch]
            Gw = Gc @ Rw
            mid_local = np.array(((lo + hi) / 2, 0.0, mg_guns.BORE))
            rest = np.array((man.back_x - side - 0.006, 0.04, 1.34))   # (4 cm higher than the rifle: its butt where the rifle's was)
            target = Xc + Gc @ (rest - man.H[ch])
            Xw = target - Gw @ mid_local
            p = man.par[self.wi]
            lq[self.wi] = G[p].T @ Gw
            mv[self.wi] = G[p].T @ (Xw - X[p]) - (man.H[self.wi] - man.H[p])
            return lq, mv
        return self._weapon_track(clip, at)

    def laid(self, clip):
        """The gun on the ground by his right side, along him, on its right side (the drum up)."""
        man = self.man
        Pw = man.weapon_pts - man.H[self.wi]
        mid = np.array(((Pw[:, 0].min() + Pw[:, 0].max()) / 2, 0.0, 0.0))
        half = float(Pw[:, 1].max())                                    # (how far the right side stands off the bore)

        def at(t):
            lq, mv = man.locals_of(clip, t)
            G, X = man.fk(lq, mv)
            along = X[man.b("head")] - X[man.b("pelvis")]
            along[2] = 0.0
            along = unit(along)
            right = X[man.b("upperarm_r")] - X[man.b("upperarm_l")]
            right[2] = 0.0
            right = unit(right)
            Gw = fc.frame_map(np.array((1.0, 0, 0)), np.array((0, 1.0, 0)), along, np.array((0.0, 0.0, -1.0)))
            target = 0.5 * (X[man.b("upperarm_r")] + X[man.b("thigh_r")]) + right * 0.32
            target[2] = half + 0.004
            Xw = target - Gw @ mid
            p = man.par[self.wi]
            lq[self.wi] = G[p].T @ Gw
            mv[self.wi] = G[p].T @ (Xw - X[p]) - (man.H[self.wi] - man.H[p])
            return lq, mv
        return self._weapon_track(clip, at)

    def _weapon_track(self, clip, fn):
        man = self.man
        ts = fc.times_of(clip["long"])
        rot, mov = [], []
        prev = None
        for t in ts:
            lq, mv = fn(t)
            q = fc.quat(lq[self.wi])
            if prev is not None and float(q @ prev) < 0:
                q = -q
            prev = q
            rot.append([round(t, 4)] + [round(float(v), 5) for v in q])
            mov.append([round(t, 4)] + [round(float(v), 4) for v in mv[self.wi]])
        out = dict(clip)
        tracks = [dict(tr) for tr in clip["tracks"] if tr["part"] != self.wi]
        tracks.append({"part": self.wi, "part_name": "weapon", "rot": rot, "move": mov})
        tracks.sort(key=lambda tr: tr["part"])
        out["tracks"] = tracks
        return out

    def lows(self, clip, pts, bones):
        man = self.man
        out = []
        for t in fc.times_of(clip["long"]):
            G, X = gore.pose(man.rig, clip, t)
            W = X[bones] + np.einsum("nij,nj->ni", G[bones], pts - man.H[bones])
            out.append(float(W[:, 2].min()))
        return np.array(out)

    def lift(self, clip, ref):
        """The gun kept off the ground: wherever it would go lower than the rifle went in `ref` (the rifleman's own
        clip, never lower than the ground, so a gun on a lorry's floor stays on it), the weapon is raised by the
        difference (eased over neighbouring keys). The hands follow it in refit."""
        man = self.man
        M = man.D["near"]
        P = np.array(M["points"]).reshape(-1, 3)
        part = np.array(M["part"])
        sel = np.isin(part, [man.b(b) for b in GUN_BONES if b in man.ix])
        gl = self.lows(clip, P[sel], part[sel])
        rp = self.rifle + man.H[self.wi]
        rl = self.lows(ref, rp, np.full(len(rp), self.wi))
        need = np.maximum(0.0, np.minimum(rl, 0.0) - gl + 0.002)
        if need.max() < 0.003:
            return clip, 0.0
        k = len(need)
        wide = np.array([need[max(0, i - 2):i + 3].max() for i in range(k)])
        smooth = np.array([wide[max(0, i - 2):i + 3].mean() for i in range(k)])
        need = np.maximum(need, smooth)
        rot, mov = [], []
        prev = None
        for t, up in zip(fc.times_of(clip["long"]), need):
            lq, mv = man.locals_of(clip, t)
            G, X = man.fk(lq, mv)
            p = man.par[self.wi]
            mv[self.wi] = mv[self.wi] + G[p].T @ np.array((0.0, 0.0, up))
            q = fc.quat(lq[self.wi])
            if prev is not None and float(q @ prev) < 0:
                q = -q
            prev = q
            rot.append([round(t, 4)] + [round(float(v), 5) for v in q])
            mov.append([round(t, 4)] + [round(float(v), 4) for v in mv[self.wi]])
        out = dict(clip)
        tracks = [dict(tr) for tr in clip["tracks"] if tr["part"] != self.wi]
        tracks.append({"part": self.wi, "part_name": "weapon", "rot": rot, "move": mov})
        tracks.sort(key=lambda tr: tr["part"])
        out["tracks"] = tracks
        return out, float(need.max())

    # ---- whole poses keyed again
    def rebake(self, clip, fn):
        """The clip keyed again 15 a second through fn(t, lq, mv) -> (lq, mv): each bone the clip keyed, or fn turned
        or moved."""
        man = self.man
        ts = fc.times_of(clip["long"])
        had = {tr["part"] for tr in clip["tracks"]}
        moved = {tr["part"] for tr in clip["tracks"] if "move" in tr}
        rot = {i: [] for i in range(man.nb)}
        mov = {i: [] for i in range(man.nb)}
        prev = {}
        turned = set()
        for t in ts:
            lq, mv = man.locals_of(clip, t)
            lq, mv = fn(t, lq, mv)
            for i in range(man.nb):
                q = fc.quat(lq[i])
                if i in prev and float(q @ prev[i]) < 0:
                    q = -q
                prev[i] = q
                if abs(abs(q[0]) - 1.0) > 1e-6:
                    turned.add(i)
                if np.abs(mv[i]).max() > 1e-5:
                    moved.add(i)
                rot[i].append([round(t, 4)] + [round(float(v), 5) for v in q])
                mov[i].append([round(t, 4)] + [round(float(v), 4) for v in mv[i]])
        tracks = []
        for i in sorted(had | turned | moved):
            tr = {"part": i, "part_name": man.rig.names[i], "rot": rot[i]}
            if i in moved:
                tr["move"] = mov[i]
            tracks.append(tr)
        out = dict(clip)
        out["tracks"] = tracks
        return out

    def place_weapon(self, lq, mv, Gw, Xw):
        man = self.man
        p = man.par[self.wi]
        G, X = man.fk(lq, mv)
        lq[self.wi] = G[p].T @ Gw
        mv[self.wi] = G[p].T @ (Xw - X[p]) - (man.H[self.wi] - man.H[p])
        return lq, mv

    def carry_hands(self, lq, mv, Gw_old, Ghands_old, Gw_new):
        """Each hand turned with the gun (so it holds it as it did), its arm keyed again later by refit."""
        man = self.man
        G, X = man.fk(lq, mv)
        for s, Gh in Ghands_old.items():
            h = man.b("hand_" + s)
            lq[h] = G[man.par[h]].T @ (Gw_new @ Gw_old.T @ Gh)
        return lq

    def prone_up(self, clip, alpha):
        """Lying behind the gun: his chest raised `alpha` degrees on his elbows about the small of his back (his head
        turned back as much, so his eye stays on the sights), the gun brought level, its butt staying in his right
        shoulder."""
        man = self.man
        sp, nk, hd = man.b("spine_01"), man.b("neck"), man.b("head")
        ua = man.b("upperarm_r")
        pivot = np.array((-0.30, 0.0, mg_guns.BORE))

        def fn(t, lq, mv):
            G, X = man.fk(lq, mv)
            a = X[hd] - X[sp]
            a[2] = 0.0
            k = unit(np.cross(unit(a), np.array((0.0, 0.0, 1.0))))
            lq[sp] = G[man.par[sp]].T @ fc.R_axis(k, alpha) @ G[sp]
            for b_ in (nk, hd):
                G1, _ = man.fk(lq, mv)
                lq[b_] = G1[man.par[b_]].T @ fc.R_axis(k, -alpha / 2) @ G1[b_]
            G3, X3 = man.fk(lq, mv)
            Gw, Xw = G[self.wi], X[self.wi]
            ax = Gw[:, 0]
            flat = ax.copy()
            flat[2] = 0.0
            Gw2 = turn_between(ax, flat) @ Gw
            Xw2 = Xw + Gw @ pivot + (X3[ua] - X[ua]) - Gw2 @ pivot
            lq, mv = self.place_weapon(lq, mv, Gw2, Xw2)
            lq = self.carry_hands(lq, mv, Gw, {s: G[man.b("hand_" + s)] for s in ("l", "r")}, Gw2)
            return lq, mv
        return self.rebake(clip, fn)

    def hip(self, clip, lean, heel, side, bore_z, head_down=4.0):
        """Firing from the hip, standing: his legs as in `clip`, his body leant `lean` degrees forward into the gun
        from the small of his back, his head upright again looking ahead; the gun level at his belt, its heel
        (`heel` x, the root's frame) by his ribs and `side` out to his right, its butt under his right arm; each hand
        turned with the gun (refit then brings the arms to it)."""
        man = self.man
        sp, nk, hd = man.b("spine_01"), man.b("neck"), man.b("head")
        pv = man.b("pelvis")
        G0, X0 = man.fk(*man.locals_of(clip, 0.0))
        heel_l = np.array((-0.349, 0.0, mg_guns.BORE))
        Xw0 = np.array((heel, side, bore_z - mg_guns.BORE)) - heel_l

        def fn(t, lq, mv):
            G, X = man.fk(lq, mv)
            lq[sp] = G[man.par[sp]].T @ fc.R_axis((0.0, 1.0, 0.0), lean) @ G[sp]
            G1, X1 = man.fk(lq, mv)
            # (his head upright and looking ahead, a little down; his neck half way)
            want = fc.R_axis((0.0, 1.0, 0.0), head_down)
            Rn = fc.qpow(want @ G1[nk].T, 0.5) @ G1[nk]
            lq[nk] = G1[man.par[nk]].T @ Rn
            G2, _ = man.fk(lq, mv)
            lq[hd] = G2[man.par[hd]].T @ want
            # (the gun: where it is at t 0, carried with his hips after)
            Rp = G[pv] @ G0[pv].T
            Gw2 = Rp
            Xw2 = X[pv] + Rp @ (Xw0 - X0[pv])
            Gw, _ = G[self.wi], X[self.wi]
            lq, mv = self.place_weapon(lq, mv, Gw2, Xw2)
            lq = self.carry_hands(lq, mv, Gw, {s: G[man.b("hand_" + s)] for s in ("l", "r")}, Gw2)
            return lq, mv
        return self.rebake(clip, fn)

    def legs_down(self, clip, spread=mg_guns.SPREAD, ground=0.0):
        """The bipod's legs keyed down to the ground about the hinge each key, splayed to an A: the feet `spread`
        apart across the gun, ahead of the hinge as far as the legs' length leaves."""
        man = self.man
        gun = self.gun
        legs = {"l": (man.b(mg_guns.LEFT_LEG), -1.0), "r": (man.b(mg_guns.RIGHT_LEG), 1.0)}
        e0 = {s: mg_guns.foot_of(gun, sg) for s, (bi, sg) in legs.items()}
        rot = {s: [] for s in legs}
        prev = {}
        reach = []
        for t in fc.times_of(clip["long"]):
            lq, mv = man.locals_of(clip, t)
            G, X = man.fk(lq, mv)
            Gw, Xw = G[self.wi], X[self.wi]
            fwd = Gw[:, 0].copy()
            fwd[2] = 0.0
            fwd = unit(fwd)
            lat = np.cross(np.array((0.0, 0.0, 1.0)), fwd)          # (to the gun's left)
            for s, (bi, sg) in legs.items():
                Xb = Xw + Gw @ (man.H[bi] - man.H[self.wi])
                L = float(np.linalg.norm(e0[s]))
                vz = (ground + 0.012) - Xb[2]
                half = spread / 2.0
                f2 = L * L - vz * vz - half * half
                if f2 < 0.02 ** 2:
                    half = math.sqrt(max(0.0, L * L - vz * vz - 0.02 ** 2))
                    f2 = 0.02 ** 2
                v = fwd * math.sqrt(f2) + lat * (-sg) * half + np.array((0.0, 0.0, vz))
                vw = Gw.T @ v
                R = fc.frame_map(e0[s], np.array((0.0, sg, 0.0)), vw, Gw.T @ (lat * (-sg)))
                q = fc.quat(R)
                if s in prev and float(q @ prev[s]) < 0:
                    q = -q
                prev[s] = q
                rot[s].append([round(t, 4)] + [round(float(v_), 5) for v_ in q])
                reach.append(half)
        out = dict(clip)
        tracks = [dict(tr) for tr in clip["tracks"] if tr["part"] not in [bi for bi, _ in legs.values()]]
        for s, (bi, _) in legs.items():
            tracks.append({"part": bi, "part_name": man.rig.names[bi], "rot": rot[s]})
        tracks.sort(key=lambda tr: tr["part"])
        out["tracks"] = tracks
        return out, 2 * min(reach)

    def kick(self, clip, shots, deg=1.4, back=0.006):
        """The gun's jump at each shot of a burst: a little up about the butt and back into his shoulder, settling
        before the next (on top of the clip's own weapon keys, 15 a second)."""
        man = self.man
        ts = fc.times_of(clip["long"])
        butt = np.array((-0.33, 0.0, mg_guns.BORE))
        rot, mov = [], []
        prev = None
        for t in ts:
            lq, mv = man.locals_of(clip, t)
            k = sum(math.exp(-(t - s) / 0.045) for s in shots if t >= s)
            k = min(k, 1.6)
            R = fc.R_axis((0.0, 1.0, 0.0), -deg * k)
            # (turn about the butt: the weapon's head moves by (I - R) butt, then back along its own x)
            off = (np.eye(3) - R) @ butt - np.array((back * k, 0.0, 0.0))
            G, X = man.fk(lq, mv)
            p = man.par[self.wi]
            Gw0 = G[self.wi]
            Gw = Gw0 @ R
            Xw = X[self.wi] + Gw0 @ off
            lq[self.wi] = G[p].T @ Gw
            mv[self.wi] = G[p].T @ (Xw - X[p]) - (man.H[self.wi] - man.H[p])
            q = fc.quat(lq[self.wi])
            if prev is not None and float(q @ prev) < 0:
                q = -q
            prev = q
            rot.append([round(t, 4)] + [round(float(v), 5) for v in q])
            mov.append([round(t, 4)] + [round(float(v), 4) for v in mv[self.wi]])
        out = dict(clip)
        tracks = [dict(tr) for tr in clip["tracks"] if tr["part"] != self.wi]
        tracks.append({"part": self.wi, "part_name": "weapon", "rot": rot, "move": mov})
        tracks.sort(key=lambda tr: tr["part"])
        out["tracks"] = tracks
        return out


def retimed(clip, long):
    """The clip played slower or faster to `long` (every key's time scaled)."""
    f = long / clip["long"]
    out = dict(clip, long=long)
    tr2 = []
    for tr in clip["tracks"]:
        t = dict(tr)
        for k in ("rot", "move"):
            if k in t:
                t[k] = [[round(v[0] * f, 4)] + v[1:] for v in t[k]]
        tr2.append(t)
    out["tracks"] = tr2
    return out


def burst(t0, n, gap=0.1, cue="lmg_burst"):
    return [{"t": round(t0 + k * gap, 4), "fx": "muzzle_flash", "socket": "socket_muzzle", "cue": cue if k == 0 else None}
            for k in range(n)]


# (the standing hip hold: the gun's heel this far ahead of his root by his ribs, this far out to his right, its bore
# this high; his lean into it; his elbows; the left fist on the folded legs this far along the gun)
HIP = {"lean": 12.0, "heel": -0.03, "side": 0.215, "bore_z": 1.06, "l_at": 0.36,
       "poles": {"r": (-0.35, 0.15, -1.0), "l": (0.10, -0.55, -1.0)}}
# (behind the gun on its bipod: his chest up this many degrees on his elbows, the gun level, his elbows down on the
# ground: the hinge 23 cm up, his eye 2.5 cm over the sights' line)
PRONE_UP = 5.0
PRONE_POLES = {"l": (0.0, -0.25, -1.0), "r": (0.0, 0.25, -1.0)}
KNEEL_L_AT = 0.205           # (kneeling, the gun in his shoulder: his left fist under the receiver, ahead of the drum)


def bare(clip):
    """The clip without the rifle's bolt and bipod keys (the gun's two legs ride those bones)."""
    return dict(clip, tracks=[tr for tr in clip["tracks"] if tr["part_name"] not in ("bolt", "bipod")])


def gun_clips(fit, clips, log):
    """Every clip of one file (the man's, or the twin's deaths) with the gun held, slung or laid."""
    out = {}
    clips = {k: bare(c) for k, c in clips.items()}
    prone = None
    for name, c in clips.items():
        wpn = c.get("weapon")
        if name == "fire_stand":
            shots = [0.1 + 0.1 * k for k in range(5)]
            hp = fit.hip(c, HIP["lean"], HIP["heel"], HIP["side"], HIP["bore_z"])
            wc, up = fit.lift(fit.kick(hp, shots, deg=1.8, back=0.008), c)
            c2, worst, _ = fit.refit(c, force_r=True, wclip=wc, poles=HIP["poles"], l_at=HIP["l_at"])
            c2["events"] = burst(0.1, 5)
            c2["weapon"] = "lmg"
            c2["from"] = (c.get("from", "") + "; mg_gunners.py: firing from the hip, a five-round burst with the gun's "
                          "jump: leant into it, the gun level at his belt, its butt under his right arm, his right fist "
                          "on the grip, his left on the folded legs").strip("; ")
            out[name] = c2
            log.append((name, "hip burst", round(worst * 100, 1), round(up * 100, 1)))
        elif name == "fire_kneel":
            shots = [0.1 + 0.1 * k for k in range(5)]
            wc, up = fit.lift(fit.kick(c, shots), c)
            c2, worst, _ = fit.refit(c, force_r=True, wclip=wc, l_at=KNEEL_L_AT)
            c2["events"] = burst(0.1, 5)
            c2["weapon"] = "lmg"
            c2["from"] = (c.get("from", "") + "; mg_gunners.py: the gun in his shoulder, his left fist under the "
                          "receiver ahead of the drum, a five-round burst with the gun's jump").strip("; ")
            out[name] = c2
            log.append((name, "burst", round(worst * 100, 1), round(up * 100, 1)))
        elif name in ("lmg_burst", "fire_prone"):
            if prone is None:
                src = clips["lmg_burst"]
                upc = fit.prone_up(src, PRONE_UP)
                wc, up = fit.lift(upc, src)
                c2, worst, _ = fit.refit(src, force_r=True, wclip=wc, poles=PRONE_POLES)
                c2, apart = fit.legs_down(c2)
                prone = (c2, worst, up, apart)
                log.append(("lmg_burst", f"prone, level, feet {apart * 100:.0f} cm apart", round(worst * 100, 1),
                            round(up * 100, 1)))
            c2 = dict(prone[0])
            if name == "fire_prone":
                c2 = retimed(c2, 1.4)
                c2["events"] = burst(0.1, 4)
                c2["weapon"] = "lmg"
                c2["loop"] = True
                c2["from"] = ("mg_gunners.py: lmg_burst (his chest up on his elbows, the gun level on its bipod) slowed "
                              "to 1.4 s, a shorter burst")
            else:
                c2["from"] = (clips["lmg_burst"].get("from", "") + "; mg_gunners.py: his chest up on his elbows, the gun "
                              "level, its legs down to the ground splayed apart").strip("; ")
            out[name] = c2
        elif wpn in ("rifle", "lmg"):
            wc, up = fit.lift(c, c)
            c2, worst, used = fit.refit(c, wclip=wc)
            out[name] = c2
            log.append((name, "held " + "".join(s for s in used if used[s]), round(worst * 100, 1), round(up * 100, 1)))
        elif wpn == "slung":
            out[name] = fit.slung(c)
            log.append((name, "slung", 0.0, 0.0))
        elif wpn == "laid":
            out[name] = fit.laid(c)
            log.append((name, "laid", 0.0, 0.0))
        else:
            out[name] = c
    return out


# ================================================================================================ checks
def gun_low(fit, clip, ts=None):
    """The gun's lowest point over the ground through the clip (near points, the weapon and bipod bones)."""
    man = fit.man
    M = man.D["near"]
    P = np.array(M["points"]).reshape(-1, 3)
    part = np.array(M["part"])
    sel = np.isin(part, [man.b(b) for b in GUN_BONES if b in man.ix])
    P, part = P[sel], part[sel]
    lows = []
    for t in ts or fc.times_of(clip["long"]):
        G, X = gore.pose(man.rig, clip, t)
        W = X[part] + np.einsum("nij,nj->ni", G[part], P - man.H[part])
        lows.append(float(W[:, 2].min()))
    return min(lows), lows


def bipod_feet(fit, clip):
    """lmg_burst: the bipod's feet over the ground (each leg's lowest point, each key), and how far apart they stand."""
    man = fit.man
    M = man.D["near"]
    P = np.array(M["points"]).reshape(-1, 3)
    part = np.array(M["part"])
    lows, apart = [], []
    for t in fc.times_of(clip["long"]):
        G, X = gore.pose(man.rig, clip, t)
        feet = []
        for bn in (mg_guns.LEFT_LEG, mg_guns.RIGHT_LEG):
            bi = man.b(bn)
            Pb = P[part == bi]
            W = X[bi] + (G[bi] @ (Pb - man.H[bi]).T).T
            lows.append(float(W[:, 2].min()))
            feet.append(W[np.argmin(W[:, 2])])
        apart.append(float(np.linalg.norm((feet[0] - feet[1])[:2])))
    return min(lows), max(lows), min(apart), max(apart)


# ================================================================================================ the file
def build(army, out_dir=STAGE, log=print):
    spec = ARMIES[army]
    gun = mg_guns.GUNS[spec["gun"]]()
    D = json.load(open(SHARED + spec["base"] + ".json"))
    Tw = json.load(open(SHARED + spec["base"] + "_gore.json"))
    D0 = json.loads(json.dumps(D))
    blk = blocks(D)
    assert blocks(Tw) == blk, "the twin's gun places differ from the man's"
    # (the gun's own paints, after the file's)
    np_ = dict(D.get("new_paints", {}))
    cols = dict(D.get("colours", {}))
    nxt = max([14] + list(np_.values())) + 1
    new_paint = {}
    for nm, hx in gun.paints.items():
        if nm not in np_:
            np_[nm] = nxt
            nxt += 1
        cols[nm] = hx
        new_paint[nm] = np_[nm]
    D["new_paints"], D["colours"] = np_, cols
    Tw["new_paints"] = dict(Tw.get("new_paints", {}), **{k: np_[k] for k in new_paint})
    Tw["colours"] = dict(Tw.get("colours", {}), **{k: cols[k] for k in new_paint})
    # (the legs' bones (the bipod's and the bolt's) to the gun's own hinge, so the legs swing down about it)
    names = [b["name"] for b in D["bones"]]
    if gun.hinge is not None:
        Hw0 = np.array(D["bones"][names.index("weapon")]["head"], float)
        for X in (D, Tw):
            for bn in (mg_guns.LEFT_LEG, mg_guns.RIGHT_LEG):
                X["bones"][names.index(bn)]["head"] = [round(float(v), 4) for v in Hw0 + gun.hinge]
    rep = write_gun(D, gun, new_paint, blk)
    copy_gun(Tw, D, blk)
    log(f"{spec['out']}: gun in the rifle's places: {rep}")
    # (the muzzle socket)
    wi = [b["name"] for b in D["bones"]].index("weapon")
    Hw = np.array(D["bones"][wi]["head"], float)
    for X in (D, Tw):
        for sk in X["sockets"]:
            if sk["name"] == "socket_muzzle":
                sk["at"] = [round(float(v), 4) for v in Hw + gun.muzzle]
                sk["facing"] = [1.0, 0.0, 0.0]
    # (the clips)
    fit = Fit(D0, D, gun)
    notes = []
    D["clips"] = gun_clips(fit, D["clips"], notes)
    tw_notes = []
    Tw["clips"] = gun_clips(fit, Tw["clips"], tw_notes)
    about = (f"The section's machine gunner: {spec['base']}'s rifleman with the {gun.about} in place of his rifle "
             f"(mg_gunners.py, mg_guns.py; our own drawing). The gun's points and triangles are where the rifle's were "
             f"at every level; its spare places are parked inside the receiver. His hands hold the gun in every clip "
             f"where he held the rifle (right fist on the pistol grip, the other under the gun), fire_stand and "
             f"fire_kneel fire five-round bursts, fire_prone is lmg_burst slowed to 1.4 s, and where the rifle rode "
             f"slung the gun rides on his back, drum outward. Weapon: lmg (LandMen plays lmg_burst for a prone shot).")
    for X in (D, Tw):
        X["weapon"] = "lmg"
        X["about_lmg"] = {"about": about, "gun": spec["gun"], "date": spec["date"],
                          "from": spec["base"], "places": rep,
                          "grips": {"pistol": [round(float(v), 4) for v in gun.grips["pistol"]["at"]],
                                    "muzzle": [round(float(v), 4) for v in gun.muzzle]}}
    D["name"] = Tw["name"] = spec["name"]
    os.makedirs(out_dir, exist_ok=True)
    pm, pt = os.path.join(out_dir, spec["out"] + ".json"), os.path.join(out_dir, spec["out"] + "_gore.json")
    for path, X in ((pm, D), (pt, Tw)):
        with open(path + ".tmp", "w") as f:
            json.dump(X, f, separators=(",", ":"))
        os.replace(path + ".tmp", path)
    return D, Tw, fit, notes, tw_notes, D0


def checks(D, Tw, fit, notes, tw_notes, D0, log=print):
    bad = []
    blk = blocks(D)
    for L in LEVELS:
        for X, nm in ((D, "man"), (Tw, "twin")):
            M = X[L]
            n = len(M["points"]) // 3
            T = np.array(M["triangles"]).reshape(-1, 3)
            if T.max() >= n:
                bad.append(f"{nm} {L}: a triangle past the points")
            for k in ("facings", "tint"):
                if len(M[k]) != 3 * n:
                    bad.append(f"{nm} {L}: {k} length")
            for k in ("paints", "part", "part2", "w2", "shade", "shade_clean", "dirt", "dirt_wet", "wet", "snow"):
                if len(M[k]) != n:
                    bad.append(f"{nm} {L}: {k} length {len(M[k])} != {n}")
            if nm == "man" and (len(M["points"]) != len(D0[L]["points"]) or
                                len(M["triangles"]) != len(D0[L]["triangles"])):
                bad.append(f"{nm} {L}: the point or triangle count changed")
        # (winding: the gun's triangles face their points' facings, the files' way)
        M = D[L]
        a, b, ta, tb = blk[L]
        P = np.array(M["points"]).reshape(-1, 3)
        F = np.array(M["facings"]).reshape(-1, 3)
        T = np.array(M["triangles"]).reshape(-1, 3)[ta:tb]
        n_ = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
        big = np.linalg.norm(n_, axis=1) > 1e-10
        f_ = F[T[:, 0]] + F[T[:, 1]] + F[T[:, 2]]
        wrong = int(((np.einsum("ij,ij->i", n_, f_) > 0) & big).sum())
        log(f"{L}: gun triangles {tb - ta}, wound against their facings {int(big.sum()) - wrong}, the other way {wrong}")
        if wrong > 0.02 * big.sum():
            bad.append(f"{L}: {wrong} gun triangles wound the wrong way")
    worst = sorted(notes + tw_notes, key=lambda x: -x[2])[:8]
    log("worst misses (cm) of a fist holding fully: " + ", ".join(f"{n} {w}" for n, _, w, _ in worst))
    lifts = sorted(notes + tw_notes, key=lambda x: -x[3])[:10]
    log("gun raised off the ground (cm, most): " + ", ".join(f"{n} {u}" for n, _, _, u in lifts))
    if worst and worst[0][2] > 2.0:
        bad.append(f"fist misses up to {worst[0][2]} cm")
    lo, hi, a0, a1 = bipod_feet(fit, D["clips"]["lmg_burst"])
    log(f"lmg_burst: the bipod's feet {lo * 100:.1f} to {hi * 100:.1f} cm over the ground, {a0 * 100:.0f} to "
        f"{a1 * 100:.0f} cm apart")
    if lo < -0.01 or hi > 0.03:
        bad.append(f"the bipod's feet {lo * 100:.1f} to {hi * 100:.1f} cm over the ground")
    for nm in ("idle", "walk", "fire_stand", "fire_kneel", "fire_prone", "lmg_burst", "crawl", "go_prone", "cower",
               "wounded_waiting", "stretcher_front", "die_back_a", "die_prone_a", "die_forward_a"):
        if nm in D["clips"]:
            m, lows = gun_low(fit, D["clips"][nm])
            log(f"   {nm:16s} the gun's lowest {m * 100:6.1f} cm, at the end {lows[-1] * 100:6.1f} cm")
    return bad


def deliver(army, content=False):
    spec = ARMIES[army]
    os.makedirs(BACKUP, exist_ok=True)
    stamp = time.strftime("%m%d_%H%M%S", time.gmtime())
    for suf in ("", "_gore"):
        src = os.path.join(STAGE, spec["out"] + suf + ".json")
        json.load(open(src))
        dests = [SHARED + spec["out"] + suf + ".json"]
        if content:
            dests.append(os.path.join(CONTENT, spec["out"] + suf + ".json"))
        for d in dests:
            if os.path.exists(d):
                shutil.copy2(d, os.path.join(BACKUP, os.path.basename(d) + "." + stamp))
            shutil.copyfile(src, d + ".tmp_mg")
            os.replace(d + ".tmp_mg", d)
            print("wrote", d)
    time.sleep(10)
    for suf in ("", "_gore"):
        p = SHARED + spec["out"] + suf + ".json"
        X = json.load(open(p))
        print("read back", p, X.get("weapon"), len(X["clips"]), "clips")


def main(argv):
    if "--build" in argv:
        army = argv[argv.index("--build") + 1]
        out = argv[argv.index("--out") + 1] if "--out" in argv else STAGE
        t0 = time.time()
        D, Tw, fit, notes, tw_notes, D0 = build(army, out)
        bad = checks(D, Tw, fit, notes, tw_notes, D0)
        print(f"built in {time.time() - t0:.0f} s; faults: {bad or 'none'}")
        return 1 if bad else 0
    if "--deliver" in argv:
        deliver(argv[argv.index("--deliver") + 1], "--content" in argv)
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
