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
    "UK": {"base": "soldier_UK", "out": "soldier_UK_lmg", "gun": "bren",
           "name": "Section machine gunner, UK, Bren (our own design)", "date": ["1938-01", "1945-05"]},
    "SOV": {"base": "soldier_SOV", "out": "soldier_SOV_lmg", "gun": "dp",
            "name": "Section machine gunner, SOV, DP (our own design)", "date": ["1928-01", "1945-05"]},
    "JAP": {"base": "soldier_JAP", "out": "soldier_JAP_lmg", "gun": "type96",
            "name": "Section machine gunner, JAP, Type 96 (our own design)", "date": ["1936-01", "1945-08"]},
    "ITA": {"base": "soldier_ITA", "out": "soldier_ITA_lmg", "gun": "breda30",
            "name": "Section machine gunner, ITA, Breda 30 (our own design)", "date": ["1930-01", "1945-05"]},
    "FRA": {"base": "soldier_FRA", "out": "soldier_FRA_lmg", "gun": "fm2429",
            "name": "Section machine gunner, FRA, FM 24/29 (our own design)", "date": ["1930-01", "1945-05"]},
}


def guns():
    """Every gun drawn: mg_guns' own and each army's in its own file (mg_gun_<key>.py, a function <key>() returning an
    mg_guns.Gun)."""
    import importlib
    out = dict(mg_guns.GUNS)
    for spec in ARMIES.values():
        k = spec["gun"]
        if k not in out:
            try:
                out[k] = getattr(importlib.import_module("mg_gun_" + k), k)
            except ImportError:
                pass
    return out

WET = {"steel": 0.15, "bakelite": 0.22, "brass": 0.12, "bullet": 0.12, "wood": 0.22}
SHADE_FLOOR = 0.40           # (the file's floor: the bore and the cooling holes)
SHADE_LOW = 0.56             # (the rifle's darkest shadowed steel, so the gun sits in the same range)

# ---- where a fist holds the gun, in the weapon bone's frame (mg_guns.py's own measures)
ARM_SLACK = 0.115                      # (fist to shoulder past the arm's two bones: the palm, and a little give)
BRUSH_KEYS = 5                         # (a fall: a fist on the rifle for fewer keys than this only brushed it)
HOLD_NEAR, HOLD_FAR = 0.030, 0.060     # (a fist this near the rifle holds it; this far, has let go; eased between)
WRIST_X = (-0.13, 0.07)                # (the rifle's wrist, where his right fist holds it)
DRUM_X = (0.05, 0.17)                  # (the MG 42's belt drum hangs on the left here: a supporting hand goes just ahead
                                       # of it; a gun says its own in grips["keep_out"], or None)
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
        cen = Hw + np.array(gun.grips.get("park", (0.12, 0.0, mg_guns.BORE + 0.012)), float)
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


# ---- his body as a solid, to keep the gun out of him
# (each bone's own part of him (his near points and triangles, not his forearms and hands, which hold the gun) as a
# solid round the bone: how far out his outside lies from the bone's line, at each height along it and each way round
# it, the most of anything there (his clothes, his kit). A point nearer the line than that is inside him, as deep as
# the difference, or as near a free end (the top of his helmet, the soles of his boots) if that is nearer.)
SOLID_AXIS = {"pelvis": "spine_01", "spine_01": "spine_02", "spine_02": "chest", "chest": "neck", "neck": "head",
              "head": None, "clavicle_l": "upperarm_l", "clavicle_r": "upperarm_r", "upperarm_l": "lowerarm_l",
              "upperarm_r": "lowerarm_r", "thigh_l": "calf_l", "thigh_r": "calf_r", "calf_l": "foot_l",
              "calf_r": "foot_r", "foot_l": "ball_l", "foot_r": "ball_r", "pouch_l": None, "pouch_r": None}
SOLID_JOIN = {"pelvis": "spine_01 thigh_l thigh_r", "spine_01": "pelvis spine_02", "spine_02": "spine_01 chest",
              "chest": "spine_02 neck clavicle_l clavicle_r", "neck": "chest head", "head": "neck",
              "clavicle_l": "chest upperarm_l", "clavicle_r": "chest upperarm_r", "upperarm_l": "clavicle_l",
              "upperarm_r": "clavicle_r", "thigh_l": "pelvis calf_l", "thigh_r": "pelvis calf_r",
              "calf_l": "thigh_l foot_l", "calf_r": "thigh_r foot_r", "foot_l": "calf_l", "foot_r": "calf_r",
              "pouch_l": "", "pouch_r": ""}
SOLID_FOLD = {"ball_l": "foot_l", "ball_r": "foot_r"}     # (the toes go with the foot)
SOLID_DS, SOLID_NT = 0.015, 36                            # (heights 1.5 cm apart, 36 ways round)
SOLID_GAP = 0.012                                         # (kit this far clear of him is kit, not him)
# (which clips keep the gun out of him, and how: the turns about the bore tried (every 15 degrees) and the set-offs
# (8 ways round the bore, these far); what a turn and a set-off cost against a centimetre inside him (CLEAR_ROLL
# degrees, CLEAR_OFF metres cost as much as 4 mm inside him); a wrist reaches this share of his arm's two bones; the
# depth inside him (his solid errs outward) let pass; the most the gun turns, and its set-off moves, from one key to
# the next; and how near another clip's first pose a clip's first or last must be to hand on to it)
CLEAR_NAMES = ("crawl", "cower", "flee", "car_ride", "car_ride_back", "kneel_down", "stand_up", "go_prone")
CLEAR_PREFIXES = ("bench_", "out_")
CLEAR_OFFS = (0.03, 0.06, 0.10, 0.15, 0.21, 0.30)
CLEAR_ROLL, CLEAR_OFF = 60.0, 0.05
CLEAR_REACH = 0.99
CLEAR_SHRUG = 15.0            # (where his arm is short of the gun, his shoulder comes forward to it by up to this, degrees)
CLEAR_TOL = 0.005
CLEAR_STEP, CLEAR_MOVE = 22.0, 0.05
CLEAR_HANDON = 0.10
# (an end of a clip where his own carry lies this much less deep in him than the gun as the rifle has it is carried,
# eased into the clip's own over CLEAR_EASE seconds; and where the gun as the clip has it lies deeper in him than
# CLEAR_DEEP, the rifle's path is given up for a straight one from the clip's first hold to its last)
CLEAR_PORT = 0.02
CLEAR_EASE = 0.5
CLEAR_DEEP = 0.05
CLEAR_KEEP = 0.01             # (his own carry is kept unless the rifle's hold comes out this much clearer of him)
CLEAR_SAME = (10.0, 40.0, 0.03, 0.15)   # (the rifle held as at a carried end: within 10 degrees and 3 cm, not by 40, 15)
CLEAR_STEER = (20.0, 0.10)    # (a straight path only from one hold to another this different: degrees, metres)


def clears(name):
    return name in CLEAR_NAMES or name.startswith(CLEAR_PREFIXES)


class Solid:
    """His body (bar his forearms and hands) as a solid round each bone (see SOLID_AXIS): `depth` says how deep points
    of the world lie inside him in a pose."""

    def __init__(self, man):
        self.man = man
        M = man.D["near"]
        P = np.array(M["points"], float).reshape(-1, 3)
        a, b, w = np.array(M["part"]), np.array(M["part2"]), np.array(M["w2"], float)
        T = np.array(M["triangles"]).reshape(-1, 3)
        dom = np.where(w >= 0.5, b, a)
        name = [man.rig.names[i] for i in range(man.nb)]
        bone = np.array([SOLID_FOLD.get(name[i], name[i]) for i in dom])
        gun = np.isin(a, [man.b(n) for n in GUN_BONES if n in man.ix])
        T = T[~np.any(gun[T], 1)]
        # (his outside sampled over every triangle, at least every centimetre, each sample with its triangle's bone)
        smp, sb = [], []
        A, B, C = P[T[:, 0]], P[T[:, 1]], P[T[:, 2]]
        edge = np.maximum(np.maximum(np.linalg.norm(B - A, axis=1), np.linalg.norm(C - B, axis=1)),
                          np.linalg.norm(A - C, axis=1))
        nseg = np.clip(np.ceil(edge / 0.01).astype(int), 1, 12)
        tb = bone[T]
        tbone = np.where(tb[:, 1] == tb[:, 2], tb[:, 1], tb[:, 0])
        tn = -np.cross(B - A, C - A)                               # (the files' winding is against the facing)
        tn = tn / np.maximum(np.linalg.norm(tn, axis=1), 1e-12)[:, None]
        sn = []
        for n in np.unique(nseg):
            sel = np.where(nseg == n)[0]
            for i in range(n + 1):
                for j in range(n + 1 - i):
                    u, v = i / n, j / n
                    smp.append(A[sel] + u * (B[sel] - A[sel]) + v * (C[sel] - A[sel]))
                    sb.append(tbone[sel])
                    sn.append(tn[sel])
        smp, sb, sn = np.concatenate(smp), np.concatenate(sb), np.concatenate(sn)
        self.parts = []
        for bn, child in SOLID_AXIS.items():
            if bn not in man.ix:
                continue
            S = smp[sb == bn]
            Sn = sn[sb == bn]
            if len(S) < 20:
                continue
            bi = man.b(bn)
            o = man.H[bi].copy()
            if child and child in man.ix:
                u = unit(man.H[man.b(child)] - o)
            else:
                u = np.array((0.0, 0.0, 1.0))
                c = S.mean(0)
                o = c - u * (u @ (c - o))                       # (a line up through the middle of it)
            e1 = unit(np.cross(u, (1.0, 0.0, 0.0)) if abs(u[0]) < 0.9 else np.cross(u, (0.0, 1.0, 0.0)))
            e2 = np.cross(u, e1)
            d = S - o
            s = d @ u
            rv = d - np.outer(s, u)
            r = np.linalg.norm(rv, axis=1)
            th = np.arctan2(rv @ e2, rv @ e1)
            out_ = np.einsum("ij,ij->i", Sn, rv) / np.maximum(r, 1e-9)    # (the outside facing away from the line)
            s0, s1 = float(s.min()), float(s.max())
            ns = int(math.ceil((s1 - s0) / SOLID_DS)) + 1
            si = np.clip(((s - s0) / SOLID_DS).astype(int), 0, ns - 1)
            ti = ((th + math.pi) / (2 * math.pi) * SOLID_NT).astype(int) % SOLID_NT
            # (each bin: his outside nearest the line out to the first gap of more than SOLID_GAP (R), and kit
            # hanging clear of him beyond it (KA to KB), or none)
            R = np.full((ns, SOLID_NT), -1.0)
            KA = np.full((ns, SOLID_NT), 9.0)
            KB = np.full((ns, SOLID_NT), -9.0)
            key = si * SOLID_NT + ti
            # (the nearest outside of any other bone of his, facing out, in each bin: his own outside lies nearer the
            # line there, so this bone's samples beyond it are kit hanging on him)
            dA = smp - o
            sA = dA @ u
            okA = (sb != bn) & (sA > s0) & (sA < s1)
            rvA = dA[okA] - np.outer(sA[okA], u)
            rA = np.linalg.norm(rvA, axis=1)
            fA = np.einsum("ij,ij->i", sn[okA], rvA) / np.maximum(rA, 1e-9) > 0.3
            siA = np.clip(((sA[okA] - s0) / SOLID_DS).astype(int), 0, ns - 1)
            tiA = ((np.arctan2(rvA @ e2, rvA @ e1) + math.pi) / (2 * math.pi) * SOLID_NT).astype(int) % SOLID_NT
            RO = np.full((ns, SOLID_NT), 9.0)
            np.minimum.at(RO, (siA[fA], tiA[fA]), rA[fA])
            order = np.lexsort((r, key))
            ks, rs_, os_ = key[order], r[order], out_[order]
            cut = np.concatenate([[0], np.where(np.diff(ks) != 0)[0] + 1, [len(ks)]])
            for a0, a1 in zip(cut[:-1], cut[1:]):
                rr, oo = rs_[a0:a1], os_[a0:a1]
                k_, j_ = divmod(int(ks[a0]), SOLID_NT)
                gap = list(np.where(np.diff(rr) > SOLID_GAP)[0]) if bn != "head" else []
                first = gap[0] + 1 if gap else len(rr)
                if oo[:first].mean() < -0.2 or rr[0] > RO[k_, j_] + SOLID_GAP:
                    # (the nearest outside here faces the line: the inside of kit hanging off him, his own outside
                    # (another bone's) lying nearer the line; this bone has none of him this way)
                    R[k_, j_] = 0.0
                    KA[k_, j_], KB[k_, j_] = rr[0], rr[-1]
                elif gap:
                    R[k_, j_] = rr[gap[0]]
                    KA[k_, j_], KB[k_, j_] = rr[gap[0] + 1], rr[-1]
                else:
                    R[k_, j_] = rr[-1]
            # (a height with no sample round some way takes its neighbours' round it, then the nearest height's)
            for k in range(ns):
                row = R[k]
                ok = row >= 0
                if ok.all() or not ok.any():
                    continue
                idx = np.where(ok)[0]
                allx = np.arange(SOLID_NT)
                R[k] = np.interp(allx, np.concatenate([idx - SOLID_NT, idx, idx + SOLID_NT]),
                                 np.concatenate([row[idx]] * 3))
            filled = np.where((R >= 0).all(1))[0]
            for k in range(ns):
                if not (R[k] >= 0).all():
                    R[k] = R[filled[np.argmin(np.abs(filled - k))]]
            joins = SOLID_JOIN.get(bn, "").split()
            # (an end of the bone's solid is free unless the next bone's solid carries on from it)
            ends = [all(j not in man.ix or (man.H[man.b(j)] - o) @ u > s0 + 0.03 for j in joins),
                    all(j not in man.ix or (man.H[man.b(j)] - o) @ u < s1 - 0.03 for j in joins)]
            far = float(np.max(np.linalg.norm(S - man.H[bi], axis=1))) + 0.02
            self.parts.append({"bone": bn, "i": bi, "o": o, "u": u, "e1": e1, "e2": e2, "s0": s0, "s1": s1,
                               "R": R, "KA": KA, "KB": KB, "ends": ends, "far": far})

    def depth(self, G, X, Q, by_bone=False):
        """How deep each point of Q (n x 3, the world) lies inside him in the pose G, X (0 outside): the most over his
        bones' solids."""
        H = self.man.H
        out = np.zeros(len(Q))
        which = np.full(len(Q), -1)
        # (which points lie near which bone at all, for all of them at once)
        cen = X[[pt["i"] for pt in self.parts]]
        D2 = np.einsum("ij,ij->i", Q, Q)[:, None] - 2.0 * Q @ cen.T + np.einsum("ij,ij->i", cen, cen)[None, :]
        for k, pt in enumerate(self.parts):
            bi = pt["i"]
            idx = np.where(D2[:, k] < pt["far"] ** 2)[0]
            if not len(idx):
                continue
            d = (Q[idx] - X[bi]) @ G[bi] + H[bi] - pt["o"]   # (in the bone's own frame, as it lies at rest)
            s = d @ pt["u"]
            inr = (s > pt["s0"]) & (s < pt["s1"])
            if not inr.any():
                continue
            idx, d, s = idx[inr], d[inr], s[inr]
            rv = d - np.outer(s, pt["u"])
            r = np.linalg.norm(rv, axis=1)
            th = np.arctan2(rv @ pt["e2"], rv @ pt["e1"])
            fs = (s - pt["s0"]) / SOLID_DS
            ft = (th + math.pi) / (2 * math.pi) * SOLID_NT - 0.5
            R = pt["R"]
            i0 = np.clip(np.floor(fs).astype(int), 0, len(R) - 1)
            i1 = np.clip(i0 + 1, 0, len(R) - 1)
            a_ = np.clip(fs - np.floor(fs), 0, 1)
            j0 = np.floor(ft).astype(int) % SOLID_NT
            j1 = (j0 + 1) % SOLID_NT
            b_ = ft - np.floor(ft)
            rs = ((1 - a_) * ((1 - b_) * R[i0, j0] + b_ * R[i0, j1]) + a_ * ((1 - b_) * R[i1, j0] + b_ * R[i1, j1]))
            dep = rs - r
            # (in kit hanging clear of him: as deep as its nearer face)
            ka = np.where(b_ < 0.5, np.where(a_ < 0.5, pt["KA"][i0, j0], pt["KA"][i1, j0]),
                          np.where(a_ < 0.5, pt["KA"][i0, j1], pt["KA"][i1, j1]))
            kb = np.where(b_ < 0.5, np.where(a_ < 0.5, pt["KB"][i0, j0], pt["KB"][i1, j0]),
                          np.where(a_ < 0.5, pt["KB"][i0, j1], pt["KB"][i1, j1]))
            dep = np.maximum(dep, np.minimum(r - ka, kb - r))
            if pt["ends"][0]:
                dep = np.minimum(dep, s - pt["s0"])
            if pt["ends"][1]:
                dep = np.minimum(dep, pt["s1"] - s)
            better = dep > out[idx]
            out[idx[better]] = dep[better]
            which[idx[better]] = k
        return (out, which) if by_bone else out


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
        # (up the grip: where the thumb lies; a gun with a rifle's wrist and no pistol grip gives no axis, and the
        # fist keeps the turn it had on the rifle)
        self.pg_up = -unit(g["pistol"]["axis"]) if g["pistol"].get("axis") is not None else None
        self.bore = float(getattr(gun, "bore", mg_guns.BORE))    # (the bore's height over the weapon bone's head)
        self.heel_x = float(gun.mesh("near")[0][:, 0].min())     # (the butt's heel along the gun)
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

    def target(self, s, p, thumb, force_grip=False, l_at=None, w_min=0.0, w0=None):
        """Where fist `s` (at p, the weapon's frame, its thumb along `thumb`) goes on the gun: the weight it holds
        with, the fist's new place, and the turn of the hand (the weapon's frame). `l_at`: the supporting (left) fist
        goes to this place along the gun (or (place, how much of it)), at the depth under it it held the rifle.
        `w_min`: it holds at least this much whatever it did on the rifle (the hip hold: both fists on the gun).
        `w0`: the weight it held the rifle with, if already known (else read from how near the rifle it is)."""
        w0 = 1.0 if force_grip else (self.ease(self.near_rifle(p)) if w0 is None else float(w0))
        w = max(w0, float(w_min))
        if w <= 0.0:
            return 0.0, p, np.eye(3)
        if s == "r" and (force_grip or WRIST_X[0] <= p[0] <= WRIST_X[1] or w_min > w0):
            q = self.pg
            Rl = turn_between(thumb, self.pg_up, w) if self.pg_up is not None else np.eye(3)
            return w, p + w * (q - p), Rl
        x = float(p[0])
        dz = lerp_line(self.under, x) - lerp_line(RIFLE_UNDER, x)
        q = p + np.array((0.0, 0.0, dz))
        ko = self.gun.grips.get("keep_out", DRUM_X)
        if s == "l" and ko and ko[0] < x < ko[1]:
            q[0] = ko[1] + 0.01
            q[2] = p[2] + lerp_line(self.under, q[0]) - lerp_line(RIFLE_UNDER, x)
            if self.gun.grips.get("keep_out_low") is not None:
                q[2] = min(q[2], p[2] + self.gun.grips["keep_out_low"])
        if s == "l" and l_at is not None:
            xa, f = l_at if isinstance(l_at, tuple) else (l_at, 1.0)
            # (as deep under the gun as it was under the rifle; a fist that was not on the rifle, just under the gun)
            c = w0 / w
            qa = np.array((xa, c * p[1], c * (p[2] + lerp_line(self.under, xa) - lerp_line(RIFLE_UNDER, x)) +
                           (1.0 - c) * (lerp_line(self.under, xa) - 0.008)))
            q = f * qa + (1.0 - f) * q
        return w, p + w * (q - p), np.eye(3)

    def grasps(self, clip, sides, hold=None, final=False):
        """How much each fist holds at each key of the rifleman's clip: (what it held the rifle with, what it holds at
        least (`hold[s](t)`)). `final` (a fall): letting go is final. An arm shot away (its fist further from its
        shoulder than the arm is long) holds nothing from then on; a fist kept on the gun by `hold` that has once let
        go stays off; and a fist that only brushes the falling rifle for a few keys (under 0.3 s, not at the clip's
        start or end) does not snatch at the gun."""
        man = self.man
        ts = fc.times_of(clip["long"])
        base = {s: [] for s in sides}
        extra = {s: [] for s in sides}
        reach = {s: [] for s in sides}
        for t in ts:
            lq0, mv0 = man.locals_of(clip, t)
            G0, X0 = man.fk(lq0, mv0)
            for s in sides:
                f = man.grip(G0, X0, s)
                p = G0[self.wi].T @ (f - X0[self.wi])
                base[s].append(self.ease(self.near_rifle(p)))
                extra[s].append(float(hold[s](t)) if hold and s in hold else 0.0)
                reach[s].append(float(np.linalg.norm(f - X0[man.b("upperarm_" + s)])))
        if final:
            n = len(ts)
            for s in sides:
                H = man.H
                arm = float(np.linalg.norm(H[man.b("lowerarm_" + s)] - H[man.b("upperarm_" + s)]) +
                            np.linalg.norm(H[man.b("hand_" + s)] - H[man.b("lowerarm_" + s)]))
                gone = off = False
                for i in range(n):
                    gone = gone or reach[s][i] > arm + ARM_SLACK
                    off = off or (i > 0 and extra[s][i] < 0.05)
                    if gone:
                        base[s][i] = 0.0
                    if gone or off:
                        extra[s][i] = 0.0
                i = 0
                while i < n:
                    if base[s][i] <= 0.0:
                        i += 1
                        continue
                    j = i
                    while j < n and base[s][j] > 0.0:
                        j += 1
                    if i > 0 and j < n and j - i < BRUSH_KEYS and max(extra[s][i:j]) <= 0.0:
                        base[s][i:j] = [0.0] * (j - i)
                    i = j
        return base, extra

    def refit(self, clip, force_r=False, sides=("l", "r"), wclip=None, poles=None, l_at=None, hold=None, final=False,
              shrug=0.0):
        """The clip with each holding hand's arm keyed again (upperarm, lowerarm, hand), 15 a second: where each fist
        held the rifle is read from `clip`; the gun it goes to is `wclip`'s (the same clip with the gun's own keys
        changed: a burst's jump, a lift off the ground), else `clip`'s. Where a fist cannot reach its place on the gun
        (an arm hanging straight with the gun in one hand), the gun comes to the fist instead, by the miss. `poles`:
        where each elbow points from its shoulder (the world's frame), else where it was. `hold[s](t)`: fist s holds
        at least this much at t (and `l_at` may be a function of t giving (place, how much)). `final`: a fall, where
        letting go is final (see grasps). `shrug`: where a fist is out of his arm's reach, his shoulder comes forward
        to it by turning his collarbone up to this many degrees (and the collarbone is keyed too). Returns the clip
        (wclip's other keys), the worst miss of a fist that holds fully (m), and which hands hold at all."""
        man = self.man
        wclip = wclip or clip
        ts = fc.times_of(clip["long"])
        keys = {s: {b: [] for b in (("clavicle_",) if shrug else ()) + ("upperarm_", "lowerarm_", "hand_")}
                for s in sides}
        wkeys = ([], [])
        moved = False
        used = {s: False for s in sides}
        worst = 0.0
        prev = {}
        par = man.par[self.wi]
        base, extra = self.grasps(clip, sides, hold, final)
        for ti, t in enumerate(ts):
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
                w, q, Rl = self.target(s, p, thumb, force_grip=(force_r and s == "r"),
                                       l_at=(l_at(t) if callable(l_at) else l_at),
                                       w_min=extra[s][ti], w0=base[s][ti])
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
                    was = X[man.b("lowerarm_" + s)] - X[man.b("upperarm_" + s)]
                    pole = was
                    if poles and s in poles:
                        # (a direction, or (direction, how much of it over where the elbow was) at this time)
                        pv_ = poles[s](t) if callable(poles[s]) else poles[s]
                        if isinstance(pv_, tuple) and len(pv_) == 2 and not np.isscalar(pv_[0]):
                            pole = (1.0 - pv_[1]) * unit(was) + pv_[1] * unit(pv_[0])
                        else:
                            pole = np.asarray(pv_, float)
                    if shrug:
                        lq = self.shrug(lq, mv, s, T, shrug)
                    lq = man.arm(lq, mv, s, T, pole, Gh @ A["d0"], Gh @ A["t0"])
                    G2, X2 = man.fk(lq, mv)
                    miss[s] = (w, man.grip(G2, X2, s) - fw)
                big = {s: m for s, m in miss.items() if np.linalg.norm(m[1]) > 0.006}
                if not big:
                    break
                # (the gun comes to a fist by as much as the fist holds: one that has all but let go hardly pulls it)
                wsum = sum(m[0] for m in miss.values())
                shift = shift + sum(m[0] * m[1] for m in miss.values()) / max(wsum, 1.0)
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

    def shrug(self, lq, mv, s, T, most):
        """His shoulder brought forward toward his wrist's place T, where T lies beyond his arm's reach (98% of its two
        bones): his collarbone turned about its root, by as little as brings T within reach, at most `most` degrees."""
        man = self.man
        A = man.side[s]
        cl, ua = man.b("clavicle_" + s), man.b("upperarm_" + s)
        G, X = man.fk(lq, mv)
        reach = (A["l1"] + A["l2"]) * 0.98
        C, S = X[cl], X[ua]
        if float(np.linalg.norm(T - S)) <= reach:
            return lq
        ax = np.cross(S - C, T - C)
        if np.linalg.norm(ax) < 1e-9:
            return lq
        ax = unit(ax)

        def gap(a):
            return float(np.linalg.norm(C + fc.R_axis(ax, a) @ (S - C) - T)) - reach
        lo, hi = 0.0, float(most)
        if gap(hi) > 0.0:
            a = hi
        else:
            for _ in range(20):
                mid = 0.5 * (lo + hi)
                lo, hi = (mid, hi) if gap(mid) > 0.0 else (lo, mid)
            a = hi
        lq[cl] = G[man.par[cl]].T @ fc.R_axis(ax, a) @ G[cl]
        return lq

    # ---- the gun on his back, and on the ground
    def slung(self, clip):
        """The gun on his back (as Man.sling, the muzzle up by his right shoulder), its right side to his back so the
        drum is outward; the weapon bone keyed again 15 a second."""
        man = self.man
        Pw = man.weapon_pts - man.H[self.wi]
        lo, hi = Pw[:, 0].min(), Pw[:, 0].max()
        sl = self.gun.grips.get("slung")
        if sl:                                                          # (a gun with a wide top, as a pan, says which of
            side = float(sl["off"])                                     #  its sides lies to his back and how far that
            to_back = np.array(sl["to_back"], float)                    #  side stands off the bore)
        else:
            side = Pw[(Pw[:, 1] > 0) & (Pw[:, 1] < 0.03), 1].max()      # (the receiver's right face)
            to_back = np.array((0, 1.0, 0))
        a = unit((0.0, 0.38, 1.0))
        Rw = fc.frame_map(np.array((1.0, 0, 0)), to_back, a, np.array((1.0, 0, 0)))
        ch = man.b("chest")

        def at(t):
            lq, mv = man.locals_of(clip, t)
            G, X = man.fk(lq, mv)
            Gc, Xc = G[ch], X[ch]
            Gw = Gc @ Rw
            mid_local = np.array(((lo + hi) / 2, 0.0, self.bore))
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
        ld = self.gun.grips.get("laid")                                 # (a gun that cannot lie flat on its side says
                                                                        #  which way is down for it and how far off)
        def at(t):
            lq, mv = man.locals_of(clip, t)
            G, X = man.fk(lq, mv)
            along = X[man.b("head")] - X[man.b("pelvis")]
            along[2] = 0.0
            along = unit(along)
            right = X[man.b("upperarm_r")] - X[man.b("upperarm_l")]
            right[2] = 0.0
            right = unit(right)
            target = 0.5 * (X[man.b("upperarm_r")] + X[man.b("thigh_r")]) + right * 0.32
            if ld:
                n = unit(ld["down"])
                Gw = fc.frame_map(n, np.array((1.0, 0, 0)), np.array((0.0, 0.0, -1.0)), along)
                target[2] = float(ld["off"]) - float(mid @ n) + 0.004
            else:
                Gw = fc.frame_map(np.array((1.0, 0, 0)), np.array((0, 1.0, 0)), along, np.array((0.0, 0.0, -1.0)))
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

    def leap(self, clip):
        """The most the gun or a hand moves between two keys of the clip (m)."""
        man = self.man
        ix = [self.wi, man.b("hand_l"), man.b("hand_r")]
        prev, most = None, 0.0
        for t in fc.times_of(clip["long"]):
            G, X = gore.pose(man.rig, clip, t)
            if prev is not None:
                most = max(most, float(np.linalg.norm(X[ix] - prev, axis=1).max()))
            prev = X[ix].copy()
        return most

    def by_rifle(self, clip, s, t, d0=0.15, d1=0.30):
        """How near fist s still is to the rifle at t of the rifleman's clip: 1 within 15 cm, 0 by 30 (a hand thrown
        clear in a fall, an arm shot away, has let go)."""
        man = self.man
        G, X = gore.pose(man.rig, clip, t)
        p = G[self.wi].T @ (man.grip(G, X, s) - X[self.wi])
        return 1.0 - fc.sm((self.near_rifle(p) - d0) / (d1 - d0))

    def aimed(self, clip, t):
        """Whether the rifleman has his rifle up in his shoulder, aimed, at time t of his own clip, and how he is then:
        "stand", "kneel", "prone", or None. (The gunner never fires his gun like a rifle: standing and kneeling he holds
        it at his hip, so where the rifleman's clip starts or ends aimed, the gunner's starts or ends in his hip hold.)"""
        man = self.man
        G, X = gore.pose(man.rig, clip, t)
        heel = X[self.wi] + G[self.wi] @ np.array((-0.33, 0.0, 0.04))
        if np.linalg.norm(heel - X[man.b("upperarm_r")]) > 0.20:
            return None
        if abs(math.degrees(math.asin(float(np.clip(G[self.wi][2, 0], -1, 1))))) > 12.0:
            return None
        z = float(X[man.b("pelvis")][2])
        return "stand" if z > 0.75 else ("kneel" if z > 0.30 else "prone")

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

    def prone_up(self, clip, alpha, weight=None):
        """Lying behind the gun: his chest raised `alpha` degrees on his elbows about the small of his back (his head
        turned back as much, so his eye stays on the sights), the gun brought level, its butt staying in his right
        shoulder. `weight(t)` (0 to 1, default 1): how much of it at each time (coming down behind the gun)."""
        man = self.man
        sp, nk, hd = man.b("spine_01"), man.b("neck"), man.b("head")
        ua = man.b("upperarm_r")
        pivot = np.array((self.heel_x + 0.05, 0.0, self.bore))

        def fn(t, lq, mv):
            w = 1.0 if weight is None else float(weight(t))
            if w <= 0.0:
                return lq, mv
            G, X = man.fk(lq, mv)
            a = X[hd] - X[sp]
            a[2] = 0.0
            k = unit(np.cross(unit(a), np.array((0.0, 0.0, 1.0))))
            lq[sp] = G[man.par[sp]].T @ fc.R_axis(k, alpha * w) @ G[sp]
            for b_ in (nk, hd):
                G1, _ = man.fk(lq, mv)
                lq[b_] = G1[man.par[b_]].T @ fc.R_axis(k, -alpha * w / 2) @ G1[b_]
            G3, X3 = man.fk(lq, mv)
            Gw, Xw = G[self.wi], X[self.wi]
            ax = Gw[:, 0]
            flat = ax.copy()
            flat[2] = 0.0
            Gw2 = turn_between(ax, flat, w) @ Gw
            Xw2 = Xw + Gw @ pivot + (X3[ua] - X[ua]) - Gw2 @ pivot
            lq, mv = self.place_weapon(lq, mv, Gw2, Xw2)
            lq = self.carry_hands(lq, mv, Gw, {s: G[man.b("hand_" + s)] for s in ("l", "r")}, Gw2)
            return lq, mv
        return self.rebake(clip, fn)

    def hip(self, clip, lean, heel, side, bore_z, head_down=4.0, ref=None, weight=None, t_ref=0.0, delta=False):
        """Firing from the hip: his legs as in `clip`, his body leant `lean` degrees forward into the gun from the
        small of his back, his head upright again looking ahead; the gun level at his belt, its heel (`heel` x, the
        root's frame) by his ribs and `side` out to his right, its butt under his right arm; each hand turned with the
        gun (refit then brings the arms to it). The gun's place is set by his hips in `ref` (default `clip`) at
        `t_ref`, and carried with them. `weight(t)` (0 to 1, default 1) blends the hold in and out from the clip's own
        (into and out of a kneel, the first moment of a fall). `delta`: the gun is not carried with his hips but keeps
        the clip's own motion of the rifle, set off by the difference between the hold and the rifle at `t_ref` (a fall:
        the gun drops as the rifle did, from his hip)."""
        man = self.man
        sp, nk, hd = man.b("spine_01"), man.b("neck"), man.b("head")
        pv = man.b("pelvis")
        G0, X0 = man.fk(*man.locals_of(ref or clip, t_ref))
        heel_l = np.array((self.heel_x, 0.0, self.bore))
        Xw0 = np.array((heel, side, bore_z)) - heel_l
        yax = np.array((0.0, 1.0, 0.0))
        dR, dX = G0[self.wi].T, Xw0 - X0[self.wi]              # (the hold from the rifle at t_ref: the gun level)

        def fn(t, lq, mv):
            w = 1.0 if weight is None else float(weight(t))
            G, X = man.fk(lq, mv)
            lq[sp] = G[man.par[sp]].T @ fc.R_axis(yax, lean * w) @ G[sp]
            G1, X1 = man.fk(lq, mv)
            # (his head upright and looking ahead, a little down; his neck half way)
            want = fc.R_axis(yax, head_down)
            Hn = fc.qpow(want @ G1[hd].T, w) @ G1[hd]
            Rn = fc.qpow(Hn @ G1[nk].T, 0.5) @ G1[nk]
            lq[nk] = G1[man.par[nk]].T @ Rn
            G2, _ = man.fk(lq, mv)
            lq[hd] = G2[man.par[hd]].T @ Hn
            # (the gun: where it is in `ref` at t 0, carried with his hips; blended from where the clip has it)
            # (with his hips' place and the way they face, not their tilt: in a fall the gun does not swing with them)
            f = (G[pv] @ G0[pv].T)[:, 0]
            Rp = (fc.R_axis((0.0, 0.0, 1.0), math.degrees(math.atan2(f[1], f[0])))
                  if math.hypot(f[0], f[1]) > 0.3 else np.eye(3))
            Gw, Xw = G[self.wi], X[self.wi]
            if delta:
                # (the set-off shrinks as the rifle comes down to the ground, so the gun lands where the rifle did)
                Gw2 = Gw @ fc.qpow(dR, w)
                Xw2 = Xw + w * dX * float(np.clip(Xw[2] / max(X0[self.wi][2], 1e-6), 0.0, 1.0))
            else:
                Gw2 = fc.qpow(Rp @ Gw.T, w) @ Gw
                Xw2 = (1.0 - w) * Xw + w * (X[pv] + Rp @ (Xw0 - X0[pv]))
            lq, mv = self.place_weapon(lq, mv, Gw2, Xw2)
            lq = self.carry_hands(lq, mv, Gw, {s: G[man.b("hand_" + s)] for s in ("l", "r")}, Gw2)
            # (his fingers closed round what they hold: the grip, the folded legs)
            for s in ("l", "r"):
                A = man.side[s]
                for bn, tgt in (("fingers_", fc.R_axis(np.cross(A["d0"], A["n0"]), fc.CURL)),
                                ("thumb_", fc.R_axis(np.cross(A["thumb0"], A["n0"]), fc.THUMB))):
                    i = man.b(bn + s)
                    lq[i] = fc.qpow(tgt @ lq[i].T, w) @ lq[i]
            return lq, mv
        return self.rebake(clip, fn)

    def legs_down(self, clip, spread=mg_guns.SPREAD, ground=0.0, weight=None):
        """The bipod's legs keyed down to the ground about the hinge each key, splayed to an A: the feet `spread`
        apart across the gun, ahead of the hinge as far as the legs' length leaves. `weight(t)` (0 folded to 1 down,
        default 1): the legs swinging down as he goes down behind the gun."""
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
                vz = max((ground + 0.012) - Xb[2], -0.98 * L)
                half = min(spread / 2.0, math.sqrt(max(0.0, L * L - vz * vz)))
                f2 = L * L - vz * vz - half * half
                if f2 < 0.02 ** 2:
                    half = math.sqrt(max(0.0, L * L - vz * vz - 0.02 ** 2))
                    f2 = 0.02 ** 2
                v = fwd * math.sqrt(f2) + lat * (-sg) * half + np.array((0.0, 0.0, vz))
                vw = Gw.T @ v
                R = fc.frame_map(e0[s], np.array((0.0, sg, 0.0)), vw, Gw.T @ (lat * (-sg)))
                if weight is not None:
                    R = fc.qpow(R, float(weight(t)))
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

    def carried(self, clip, ref, wfn):
        """The rifleman's clip with his rifle, and his arms to it, where `ref` (his idle) has them at its start, carried
        with his chest, as much as 1 - wfn(t) (blended from the clip's own). Where the rifleman's clip starts or ends
        with his rifle across him at the port (kneel_down's start, stand_up's end), the gunner's comes from, or goes
        to, his own carry: so it hands on from and to idle as idle starts, and the pistol grip is not in his belly."""
        man = self.man
        ch = man.b("chest")
        lqI, mvI = man.locals_of(ref, 0.0)
        GI, XI = man.fk(lqI, mvI)
        rel_R = GI[ch].T @ GI[self.wi]
        rel_X = GI[ch].T @ (XI[self.wi] - XI[ch])
        arms = [man.b(n + s) for s in ("l", "r") for n in ("upperarm_", "lowerarm_", "hand_", "fingers_", "thumb_")]

        def fn(t, lq, mv):
            w = 1.0 - float(wfn(t))
            if w <= 0.0:
                return lq, mv
            for i in arms:
                lq[i] = fc.qpow(lqI[i] @ lq[i].T, w) @ lq[i]
            G, X = man.fk(lq, mv)
            Gw = fc.qpow(G[ch] @ rel_R @ G[self.wi].T, w) @ G[self.wi]
            Xw = (1.0 - w) * X[self.wi] + w * (X[ch] + G[ch] @ rel_X)
            return self.place_weapon(lq, mv, Gw, Xw)
        return self.rebake(clip, fn)

    def depth_at(self, clip, t, ref=None):
        """How deep the gun lies inside him (his solid) at t of the clip: placed as the clip has it, or (ref) as ref
        carries it at its start, carried with his chest."""
        man = self.man
        sol = self.solid
        G, X = man.fk(*man.locals_of(clip, t))
        Gw, Xw = G[self.wi], X[self.wi]
        if ref is not None:
            ch = man.b("chest")
            GI, XI = man.fk(*man.locals_of(ref, 0.0))
            Gw = G[ch] @ GI[ch].T @ GI[self.wi]
            Xw = X[ch] + G[ch] @ GI[ch].T @ (XI[self.wi] - XI[ch])
        return float(sol.depth(G, X, Xw + self._probe @ Gw.T).max())

    def carry_ends(self, clip, ref, skip=()):
        """Where the rifleman's clip starts or ends with his rifle across him at the port, or anywhere else the gun
        would lie inside him, and his own carry (ref: idle's first pose, carried with his chest) would lie CLEAR_PORT
        less deep, the gunner holds the gun there as he carries it, eased into the clip's own over CLEAR_EASE (as
        `carried`): so he gets up from the bench, climbs out of a door, goes down to the ground from his carry, and
        the clip hands on to and from idle. `skip`: ends (0, -1) that hand on to another hold. Returns the clip and
        which ends (start, end) are carried."""
        L = float(clip["long"])
        if ref is None or clip.get("loop"):
            return clip, (False, False)
        ends = []
        for k, t in ((0, 0.0), (-1, L)):
            ends.append(k not in skip and self.depth_at(clip, t, ref) + CLEAR_PORT < self.depth_at(clip, t))
        if not any(ends):
            return clip, (False, False)
        ez = min(CLEAR_EASE, 0.45 * L)
        # (and wherever else in the clip the rifle is held as at that end, at the port all the way through (climbing
        # out of a door), he carries it so too)
        man = self.man
        ch = man.b("chest")

        def rel(t):
            G, X = man.fk(*man.locals_of(clip, t))
            return G[ch].T @ G[self.wi], G[ch].T @ (X[self.wi] - X[ch])
        at = {0: rel(0.0), -1: rel(L)}
        ts = fc.times_of(L)
        near = {}
        for k in (0, -1):
            v = []
            for t in ts:
                R, x = rel(t)
                a = math.degrees(math.acos(float(np.clip((np.trace(R @ at[k][0].T) - 1.0) / 2.0, -1.0, 1.0))))
                d = max((a - CLEAR_SAME[0]) / (CLEAR_SAME[1] - CLEAR_SAME[0]),
                        (float(np.linalg.norm(x - at[k][1])) - CLEAR_SAME[2]) / (CLEAR_SAME[3] - CLEAR_SAME[2]))
                v.append(1.0 - fc.sm(d))
            near[k] = v
        k_of = {round(t, 4): i for i, t in enumerate(ts)}

        def wfn(t):
            i = k_of.get(round(t, 4))
            k = 0.0
            if ends[0]:
                k = max(k, 1.0 - fc.sm(t / ez), near[0][i] if i is not None else 0.0)
            if ends[1]:
                k = max(k, fc.sm((t - (L - ez)) / ez), near[-1][i] if i is not None else 0.0)
            return 1.0 - k
        return self.carried(clip, ref, wfn), tuple(ends)

    def steer(self, clip, deep_at=CLEAR_DEEP, held=None, hold=None):
        """Where the gun as the clip has it lies deeper inside him than `deep_at` (the rifleman takes his rifle
        through himself as he sits down on a lorry's bench or gets up from it, where a gun twice its bulk cannot go;
        the gun swung from his carry into the hip hold through his hip), it goes instead straight from where it is at
        the clip's start to where it is at its end, relative to his hips (their place and the way they face, not
        their tilt), as his body goes from its first pose to its last; eased in and out over the keys either side;
        each holding hand turned with it (`held`: the clip his fists are read from, default this one; `hold` as in
        grasps). `clear` then keeps it out of him on that path."""
        man = self.man
        L = float(clip["long"])
        ts = fc.times_of(L)
        n = len(ts)
        dep = np.array([self.depth_at(clip, t) for t in ts])
        if dep.max() <= deep_at:
            return clip
        deep = (dep > deep_at).astype(float)
        wide = np.array([deep[max(0, i - 3):i + 4].max() for i in range(n)])
        bw = np.array([wide[max(0, i - 2):i + 3].mean() for i in range(n)])
        # (how far his body has gone from its first pose to its last, his root aside)
        pv = man.b("pelvis")
        bones = [man.b(b_) for b_ in ("pelvis", "chest", "head", "calf_l", "calf_r", "foot_l", "foot_r")]

        def sig(t):
            G, X = gore.pose(man.rig, clip, t)
            return ((X[bones] - X[0]) @ G[0]).ravel()
        s0, s1 = sig(0.0), sig(L)
        prog = []
        for t in ts:
            s = sig(t)
            a, b = float(np.linalg.norm(s - s0)), float(np.linalg.norm(s - s1))
            prog.append(a / max(a + b, 1e-9))
        prog = np.maximum.accumulate(np.array(prog))
        prog = prog / max(prog[-1], 1e-9)
        Gs, Xs = man.fk(*man.locals_of(clip, 0.0))
        Rs = Gs[pv]

        def frame(G, X):
            f = (G[pv] @ Rs.T)[:, 0]
            yaw = math.degrees(math.atan2(f[1], f[0])) if math.hypot(f[0], f[1]) > 0.2 else 0.0
            return fc.R_axis((0.0, 0.0, 1.0), yaw), X[pv]
        G1, X1 = man.fk(*man.locals_of(clip, L))
        F0, F1 = frame(Gs, Xs), frame(G1, X1)
        A0 = (F0[0].T @ Gs[self.wi], F0[0].T @ (Xs[self.wi] - F0[1]))
        A1 = (F1[0].T @ G1[self.wi], F1[0].T @ (X1[self.wi] - F1[1]))
        turn = math.degrees(math.acos(float(np.clip((np.trace(A1[0] @ A0[0].T) - 1.0) / 2.0, -1.0, 1.0))))
        if turn < CLEAR_STEER[0] and float(np.linalg.norm(A1[1] - A0[1])) < CLEAR_STEER[1]:
            # (a clip that ends with the gun held as it started (climbing out of a door, at the port both ends)
            # keeps the rifle's own path: the search keeps it out of him)
            return clip
        base, extra = self.grasps(held or clip, ("l", "r"), hold)
        base = {s_: np.maximum(base[s_], extra[s_]) for s_ in base}
        k_of = {round(t, 4): i for i, t in enumerate(ts)}

        def fn(t, lq, mv):
            i = k_of[round(t, 4)]
            w = float(bw[i])
            if w <= 0.0:
                return lq, mv
            G, X = man.fk(lq, mv)
            Fr = frame(G, X)
            r = (Fr[0].T @ G[self.wi], Fr[0].T @ (X[self.wi] - Fr[1]))
            p = float(prog[i])
            a = (fc.qpow(A1[0] @ A0[0].T, p) @ A0[0], (1.0 - p) * A0[1] + p * A1[1])
            Gw2 = Fr[0] @ fc.qpow(a[0] @ r[0].T, w) @ r[0]
            Xw2 = Fr[1] + Fr[0] @ ((1.0 - w) * r[1] + w * a[1])
            lq, mv = self.place_weapon(lq, mv, Gw2, Xw2)
            Rc = Gw2 @ G[self.wi].T
            G2, _ = man.fk(lq, mv)
            for s in ("l", "r"):
                if base[s][i] > 0.0:
                    h = man.b("hand_" + s)
                    lq[h] = G2[man.par[h]].T @ fc.qpow(Rc, min(base[s][i], 1.0)) @ G[h]
            return lq, mv
        return self.rebake(clip, fn)

    # ---- the gun kept out of him
    @property
    def solid(self):
        if getattr(self, "_solid", None) is None:
            self._solid = Solid(self.man)
            # (the gun's points to test, one in each 2 cm cube of it, the weapon's frame)
            Pw = self.man.weapon_pts - self.man.H[self.wi]
            _, first = np.unique(np.floor(Pw / 0.02).astype(int), axis=0, return_index=True)
            self._probe = Pw[np.sort(first)]
        return self._solid

    def turned(self, c):
        """A candidate (turn about the bore, then a swing about the pistol grip across and up and down (yaw, pitch),
        degrees; then a set-off (y, z) in the gun's frame) as a turn and a move of the gun in its own frame."""
        b = np.array((0.0, 0.0, self.bore))
        Rx = fc.R_axis((1.0, 0.0, 0.0), c[0])
        Ryp = fc.R_axis((0.0, 0.0, 1.0), c[1]) @ fc.R_axis((0.0, 1.0, 0.0), c[2])
        return Ryp @ Rx, Ryp @ (b - Rx @ b - self.pg) + self.pg + np.array((0.0, c[3], c[4]))

    def clear_cost(self, poses, cands, floor, low0, aims, w_hand):
        """Each candidate (see `turned`) at one key: how deep the gun goes into him, its lowest point; and its cost:
        the depth inside him, below the floor (and up off it where it lay on it), past the reach of a holding arm,
        and the size of the turn and set-off. `poses` are his pose (G, X) at the key and half a key either side, the
        gun as the clip has it there: the candidate holds for all of them, so the gun stays out of him between keys."""
        sol = self.solid
        S = self._probe
        Ts = [self.turned(c) for c in cands]
        Rs = np.array([t_[0] for t_ in Ts])
        tr = np.array([t_[1] for t_ in Ts])
        pen, low = np.zeros(len(cands)), np.full(len(cands), 9.0)
        for k, (G, X) in enumerate(poses):
            Gw, Xw = G[self.wi], X[self.wi]
            Gc = np.einsum("ij,cjk->cik", Gw, Rs)
            Xc = Xw + tr @ Gw.T
            Q = Xc[:, None, :] + np.einsum("cij,nj->cni", Gc, S)
            pen = np.maximum(pen, sol.depth(G, X, Q.reshape(-1, 3)).reshape(len(cands), -1).max(1))
            low = np.minimum(low, Q[:, :, 2].min(1))
            if k == 0:
                G0, X0, Gc0, Xc0 = G, X, Gc, Xc
        below = np.maximum(0.0, floor - low)
        up = np.maximum(0.0, low - low0 - 0.03) if low0 - floor < 0.02 else np.zeros(len(cands))
        cost = 20.0 * (np.maximum(0.0, pen - CLEAR_TOL) / 0.01) ** 2 + 4.0 * (below / 0.01) ** 2 + (up / 0.02) ** 2
        for s, (v, reach) in aims.items():
            wr = Xc0 + np.einsum("cij,j->ci", Gc0, v)
            ex = np.maximum(0.0, np.linalg.norm(wr - X0[self.man.b("upperarm_" + s)], axis=1) - reach)
            cost = cost + 4.0 * w_hand[s] * (ex / 0.01) ** 2
        C = np.array(cands)
        cost = cost + ((C[:, :3] / CLEAR_ROLL) ** 2).sum(1) + (np.linalg.norm(C[:, 3:], axis=1) / CLEAR_OFF) ** 2
        return cost, pen, low

    def clear_marks(self, cands):
        """Where the gun's ends (its farthest points each way, and the pistol grip) lie under each candidate, in the
        gun's frame: how far one candidate is from another is how far these move."""
        S = self._probe
        M = np.vstack([S[np.argmin(S, 0)], S[np.argmax(S, 0)], self.pg[None, :]])
        out = []
        for c in cands:
            R, tr = self.turned(c)
            out.append(M @ R.T + tr)
        return np.array(out)

    def clear(self, clip, wc, hold=None, pins=None, loop=False):
        """The gun kept out of him (CLEAR_NAMES): wherever the gun, placed as `wc` has it (where the rifleman's rifle
        was, or the hold it is coming into), would lie inside his body, it is turned about its bore, swung about its
        pistol grip and set off from him by the least that clears it, key by key (15 a second): a search over such
        turns and set-offs for the least depth inside him (Solid), not below the floor the rifle kept to, with each
        holding wrist within reach of its shoulder, then the least change; chosen together over the whole clip (a
        shortest path, so the gun turns no more than CLEAR_STEP a key and moves no more than CLEAR_MOVE, and a loop
        ends as it starts) and eased. `pins` {key index: candidate} holds the choice at a key (none where the gun hands
        on to a hold, as another clip has it where it hands on to that one). Each hand that holds the gun turns with it
        (refit then brings the arms). Returns the clip and what was done (or wc itself where nothing needed doing)."""
        man = self.man
        _ = self.solid
        ts = fc.times_of(wc["long"])
        n = len(ts)
        base, extra = self.grasps(clip, ("l", "r"), hold)
        rp = self.rifle + man.H[self.wi]
        rl = self.lows(clip, rp, np.full(len(rp), self.wi))
        Z = (0.0, 0.0, 0.0, 0.0, 0.0)
        rolls = [float(a) for a in range(-165, 181, 15) if a]
        swings = [-45.0, -30.0, -15.0, 15.0, 30.0, 45.0]
        dirs = [(math.cos(math.radians(a)), math.sin(math.radians(a))) for a in range(0, 360, 45)]
        fam = {"roll": [(r_, 0.0, 0.0, 0.0, 0.0) for r_ in rolls],
               "yaw": [(0.0, y_, 0.0, 0.0, 0.0) for y_ in swings],
               "pitch": [(0.0, 0.0, p_, 0.0, 0.0) for p_ in swings],
               "off": [(0.0, 0.0, 0.0, m * dy, m * dz) for (dy, dz) in dirs for m in CLEAR_OFFS]}
        cands = [Z] + [c for v in fam.values() for c in v]
        poses, aims_k, hands_k = [], [], []
        for i, t in enumerate(ts):
            lq, mv = man.locals_of(wc, t)
            G, X = man.fk(lq, mv)
            lq0, mv0 = man.locals_of(clip, t)
            G0, X0 = man.fk(lq0, mv0)
            aims, wh = {}, {}
            for s in ("l", "r"):
                w = max(base[s][i], extra[s][i])
                if w <= 0.0:
                    continue
                A = man.side[s]
                f = man.grip(G0, X0, s)
                p = G0[self.wi].T @ (f - X0[self.wi])
                h = man.b("hand_" + s)
                thumb = G0[self.wi].T @ G0[h] @ A["t0"]
                _, q, Rl = self.target(s, p, thumb, w0=1.0)
                # (his wrist, where refit will put it for the fist to hold there with the hand turned with the gun,
                # in the gun's frame: it must stay within his arm's reach of his shoulder)
                Gw = G[self.wi]
                Gh = Gw @ Rl @ Gw.T @ G[h]
                v = q - Gw.T @ Gh @ (A["d0"] * 0.075 + A["n0"] * 0.022)
                aims[s] = (v, (A["l1"] + A["l2"]) * CLEAR_REACH)
                wh[s] = w
            smp = [(G, X)]
            for t2 in (t - 0.5 / fc.RATE, t + 0.5 / fc.RATE):
                if 0.0 < t2 < wc["long"]:
                    smp.append(man.fk(*man.locals_of(wc, t2)))
            poses.append(smp)
            aims_k.append(aims)
            hands_k.append(wh)

        def costs(cs, i, low0, full=True):
            # (the search over many candidates looks at the key alone; the choice is then nudged, and checked, at the
            # half keys either side too)
            return self.clear_cost(poses[i] if full else poses[i][:1], cs, min(float(rl[i]), 0.0), low0, aims_k[i],
                                   hands_k[i])
        # (first each kind of change alone at every key; then the turns, swings and ways of setting off that did best
        # somewhere, together)
        U1, P1, L1, P0 = [], [], [], []
        for i in range(n):
            _, p0_, l_ = costs([Z], i, 9.0)
            L1.append(float(l_[0]))
            P0.append(float(p0_[0]))
            c_, p_, _ = costs(cands, i, L1[i], full=False)
            U1.append(c_)
            P1.append(p_)
        U1, P1 = np.array(U1), np.array(P1)
        before = max(P0)
        if before <= CLEAR_TOL and not pins:
            return wc, {"before": before, "after": before, "turn": 0.0, "off": 0.0}
        idx = {}
        k0 = 1
        for nm, v in fam.items():
            idx[nm] = list(range(k0, k0 + len(v)))
            k0 += len(v)
        votes = {nm: {} for nm in fam}
        for i in range(n):
            if P1[i, 0] <= CLEAR_TOL:
                continue
            for nm in ("roll", "yaw", "pitch"):
                for j in np.array(idx[nm])[np.argsort(U1[i, idx[nm]])[:3]]:
                    votes[nm][cands[j][:3]] = votes[nm].get(cands[j][:3], 0) + 1
            od = U1[i, idx["off"]].reshape(len(dirs), len(CLEAR_OFFS)).min(1)
            for d in np.argsort(od)[:2]:
                votes["off"][int(d)] = votes["off"].get(int(d), 0) + 1
        top = {nm: [k for k, _ in sorted(v.items(), key=lambda kv: -kv[1])[:{"roll": 6, "off": 3}.get(nm, 4)]]
               for nm, v in votes.items()}
        turns = top["roll"] + top["yaw"] + top["pitch"]
        more = []
        for tt in turns:
            for d in top["off"]:
                for m in CLEAR_OFFS:
                    more.append(tt + (m * dirs[d][0], m * dirs[d][1]))
        for r_ in top["roll"]:
            for sw in top["yaw"] + top["pitch"]:
                more.append((r_[0], sw[1], sw[2], 0.0, 0.0))
        pin_c = [tuple(float(v) for v in p) for p in (pins or {}).values()]
        seen = set(cands)
        extra_c = []
        for c in more + pin_c:
            if c not in seen:
                seen.add(c)
                extra_c.append(c)
        allc = cands + extra_c
        U = np.zeros((n, len(allc)))
        for i in range(n):
            U[i, :len(cands)] = U1[i]
            if extra_c:
                U[i, len(cands):] = costs(extra_c, i, L1[i], full=False)[0]
        for k_, p in (pins or {}).items():
            k_ = k_ % n
            j = allc.index(tuple(float(v) for v in p))
            keep = U[k_, j]
            U[k_, :] = np.inf
            U[k_, j] = keep
        # (the step from one key to the next: a turn of more than CLEAR_STEP degrees, or a set-off moving more than
        # CLEAR_MOVE, is barred; less is costed by how much)
        C = np.array(allc)
        da = np.abs((C[:, None, :3] - C[None, :, :3] + 180.0) % 360.0 - 180.0)
        MK = self.clear_marks(allc)
        dm = np.zeros((len(allc), len(allc)))
        for k in range(MK.shape[1]):
            dm = np.maximum(dm, np.linalg.norm(MK[:, None, k] - MK[None, :, k], axis=2))
        Pair = 0.5 * (dm / 0.02) ** 2 + np.where((da.max(2) > CLEAR_STEP) | (dm > CLEAR_MOVE), 1e6, 0.0)

        def path(U, first=None):
            cost = U[0].copy()
            if first is not None:
                cost = np.where(np.arange(len(cost)) == first, cost, np.inf)
            back = np.zeros((n, len(cost)), dtype=int)
            for i in range(1, n):
                tot = cost[:, None] + Pair
                back[i] = np.argmin(tot, 0)
                cost = tot[back[i], np.arange(len(cost))] + U[i]
            if first is not None:
                cost = np.where(np.arange(len(cost)) == first, cost, np.inf)
            j = int(np.argmin(cost))
            best = float(cost[j])
            out = [j]
            for i in range(n - 1, 0, -1):
                j = int(back[i][j])
                out.append(j)
            return best, out[::-1]
        if loop:
            starts = sorted(set([0] + [int(j) for j in np.argsort(U[0])[:12]]))
            best, pth = min((path(U, s0) for s0 in starts), key=lambda r: r[0])
        else:
            best, pth = path(U)
        raw = C[pth].copy()
        raw[:, 0] = np.degrees(np.unwrap(np.radians(raw[:, 0])))
        # (then each key's choice nudged, finer and finer, to the least cost with its neighbours' as they stand)
        pinned = {k_ % n for k_ in (pins or {})}

        def pair(a, b):
            da_ = np.abs(a[:3] - b[:3])
            ma, mb = self.clear_marks([tuple(a), tuple(b)])
            dm_ = float(np.linalg.norm(ma - mb, axis=1).max())
            return 0.5 * (dm_ / 0.02) ** 2 + (1e6 if da_.max() > CLEAR_STEP or dm_ > CLEAR_MOVE else 0.0)

        def nbrs(i):
            if loop:
                return [(i - 1) % (n - 1), (i + 1) % (n - 1)]
            return [k_ for k_ in (i - 1, i + 1) if 0 <= k_ < n]
        for st_a, st_m in ((7.5, 0.02), (3.75, 0.01), (1.875, 0.005)):
            for _ in range(2):
                for i in range(n - 1 if loop else n):
                    if i in pinned:
                        continue
                    cs = [raw[i]]
                    for d in range(5):
                        for sg in (-1.0, 1.0):
                            v = raw[i].copy()
                            v[d] += sg * (st_a if d < 3 else st_m)
                            cs.append(v)
                    u = costs([tuple(v) for v in cs], i, L1[i])[0]
                    tot = [u[j] + sum(pair(cs[j], raw[k_]) for k_ in nbrs(i)) for j in range(len(cs))]
                    raw[i] = cs[int(np.argmin(tot))]
                    if loop and i == 0:
                        raw[n - 1] = raw[0]

        def pens(P):
            return np.array([float(costs([tuple(P[i])], i, 9.0)[1][0]) for i in range(n)])

        def eased(P, keep):
            for _ in range(2):
                sm_ = P.copy()
                if loop:
                    m_ = n - 1
                    sm_[:m_] = (np.roll(P[:m_], 1, 0) + 2 * P[:m_] + np.roll(P[:m_], -1, 0)) / 4.0
                    sm_[-1] = sm_[0]
                else:
                    sm_[1:-1] = (P[:-2] + 2 * P[1:-1] + P[2:]) / 4.0
                for k_ in keep:
                    sm_[k_] = P[k_]
                P = sm_
            return P
        # (eased: smoothed over a key either side, a loop round its seam, pinned keys kept, and any key the easing
        # would put back into him kept as chosen)
        raw_pen = pens(raw)
        keep = {k_ % n for k_ in (pins or {})}
        for _ in range(4):
            par = eased(raw, keep)
            after = pens(par)
            bad = {i for i in range(n) if after[i] > max(raw_pen[i], CLEAR_TOL) + 0.003} - keep
            if not bad:
                break
            keep |= bad
            if loop and (0 in bad or n - 1 in bad):
                keep |= {0, n - 1}
        if np.abs(par).max() < 1e-6:
            return wc, {"before": before, "after": before, "turn": 0.0, "off": 0.0}
        hs = {s: man.b("hand_" + s) for s in ("l", "r")}
        k_of = {round(t, 4): i for i, t in enumerate(ts)}

        def fn(t, lq, mv):
            i = k_of[round(t, 4)]
            G, X = man.fk(lq, mv)
            Gw, Xw = G[self.wi], X[self.wi]
            R, tr = self.turned(par[i])
            Gw2 = Gw @ R
            Xw2 = Xw + Gw @ tr
            lq, mv = self.place_weapon(lq, mv, Gw2, Xw2)
            # (a hand that holds the gun turns with it, as much as it holds)
            Rc = Gw2 @ Gw.T
            G2, _ = man.fk(lq, mv)
            for s, h in hs.items():
                w = hands_k[i].get(s, 0.0)
                if w > 0.0:
                    lq[h] = G2[man.par[h]].T @ fc.qpow(Rc, min(w, 1.0)) @ G[h]
            return lq, mv
        out = self.rebake(wc, fn)
        return out, {"before": before, "after": float(max(after)), "turn": float(np.abs(par[:, :3]).max()),
                     "off": float(np.linalg.norm(par[:, 3:], axis=1).max()), "par": par,
                     "keys": (P1[:, 0], raw_pen, np.array(after))}

    def clear_log_add(self, name, clip, rep):
        if not hasattr(self, "clear_log"):
            self.clear_log = []
        if rep.get("par") is not None:
            self.cleared = getattr(self, "cleared", {})
            self.cleared[name] = rep["par"]
        self.clear_log.append((name, rep))
        return clip

    def hand_on(self, clip, done, clips):
        """Pins for `clear`: where this clip's first or last pose is (within CLEAR_HANDON, his body about his root)
        a looping clip's first pose, and that clip has had the gun kept out of him, this clip starts or ends with the
        gun turned and set off as that one starts. (Not a looping clip itself: the rides sit him alike, but none hands
        on to another, and each has its own hold of the gun to keep out of him.)"""
        if clip.get("loop"):
            return None
        man = self.man
        done_par = getattr(self, "cleared", {})
        bones = [man.b(n) for n in ("pelvis", "chest", "head", "hand_l", "hand_r", "calf_l", "calf_r", "foot_l",
                                    "foot_r")]

        def sig(c, t):
            G, X = gore.pose(man.rig, c, t)
            return (X[bones] - X[0]) @ G[0]
        pins = {}
        ends = {0: sig(clip, 0.0), -1: sig(clip, clip["long"])}
        for nm, par in done_par.items():
            other = clips.get(nm)
            if other is None or not other.get("loop") or other is clip:
                continue
            s0 = sig(other, 0.0)
            for k, sg in ends.items():
                if float(np.abs(sg - s0).max()) < CLEAR_HANDON:
                    pins[k] = tuple(round(float(v), 4) for v in par[0])
        return pins or None

    def kick(self, clip, shots, deg=1.4, back=0.006):
        """The gun's jump at each shot of a burst: a little up about the butt and back into his shoulder, settling
        before the next (on top of the clip's own weapon keys, 15 a second)."""
        man = self.man
        ts = fc.times_of(clip["long"])
        butt = np.array((self.heel_x + 0.02, 0.0, self.bore))
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
HIP = {"lean": 12.0, "heel": -0.03, "side": 0.215, "bore_z": 1.07, "l_at": 0.36,
       "poles": {"r": (-0.35, 0.15, -1.0), "l": (0.10, -0.55, -1.0)}}
# (behind the gun on its bipod: his chest up this many degrees on his elbows, the gun level, his elbows down on the
# ground: the hinge 23 cm up, his eye 2.5 cm over the sights' line)
PRONE_UP = 5.0
PRONE_POLES = {"l": (0.0, -0.25, -1.0), "r": (0.0, 0.25, -1.0)}
# (the kneeling hold is the same under-arm hold, lower: he never brings the gun up into his shoulder like a rifle
# (Brian, 25 November 16:06Z: "a soldier would never shoot this like a rifle position"))
KNEEL = {"lean": 14.0, "heel": -0.22, "side": 0.225, "bore_z": 0.68, "l_at": 0.36,
         "poles": {"r": (-0.35, 0.15, -1.0), "l": (0.10, -0.55, -1.0)}}
HOLDS = {"stand": HIP, "kneel": KNEEL}
EASE = 0.35                  # (seconds a clip takes to come out of, or into, the hip hold where the rifleman's is aimed)
EASE_FALL = 0.8              # (a fall: the gun leaves the hip hold this slowly, so it drops from his hip, not up to where
                             # the rifle was thrown from)


def bare(clip):
    """The clip without the rifle's bolt and bipod keys (the gun's two legs ride those bones)."""
    return dict(clip, tracks=[tr for tr in clip["tracks"] if tr["part_name"] not in ("bolt", "bipod")])


def hip_fire(fit, c, H, shots):
    """A firing loop from the hip hold H: the hold, the burst's jump, the arms to the gun."""
    gg = fit.gun.grips
    hp = fit.hip(c, H["lean"], H["heel"], H["side"] + gg.get("hip_side", 0.0), H["bore_z"])
    wc, up = fit.lift(fit.kick(hp, shots, deg=1.8, back=0.008), c)
    c2, worst, _ = fit.refit(c, force_r=True, wclip=wc, poles=H["poles"], l_at=gg.get("hip_l_at", H["l_at"]))
    c2["events"] = burst(0.1, len(shots))
    c2["weapon"] = "lmg"
    return c2, worst, up


def gun_clips(fit, clips, log):
    """Every clip of one file (the man's, or the twin's deaths) with the gun held, slung or laid."""
    out = {}
    clips = {k: bare(c) for k, c in clips.items()}
    prone = None
    # (the looping clips that keep the gun out of him first, so a clip that hands on to one of them (sitting down on
    # the bench, getting up from it) can start or end with the gun as that one has it)
    order = sorted(clips, key=lambda k: 0 if (clears(k) and clips[k].get("loop")) else 1)
    for name in order:
        c = clips[name]
        wpn = c.get("weapon")
        if name in ("fire_stand", "fire_kneel"):
            H = HIP if name == "fire_stand" else KNEEL
            c2, worst, up = hip_fire(fit, c, H, [0.1 + 0.1 * k for k in range(5)])
            c2["from"] = (c.get("from", "") + "; mg_gunners.py: firing from the hip, a five-round burst with the gun's "
                          "jump: leant into it, the gun level, its butt under his right arm against his ribs, his right "
                          "fist on the grip, his left on the folded legs (never in his shoulder like a rifle)").strip("; ")
            out[name] = c2
            log.append((name, "hip burst", round(worst * 100, 1), round(up * 100, 1)))
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
            L = c["long"]
            s, e = fit.aimed(c, 0.0), fit.aimed(c, L)
            ez = min(EASE_FALL if name.startswith("die_") else EASE, 0.7 * L)
            kind = "held "
            if e == "prone" and s != "prone" and not name.startswith("die_"):
                # (coming down behind the gun: up on his elbows, the gun level, its legs swinging down, as lmg_burst)
                down = 0.6
                wfn = lambda t, L=L, down=down: fc.sm((t - (L - down)) / 0.5)
                ends = (False, False)
                if clears(name):
                    # (going down from his own carry where the rifleman's rifle would put the gun in his thigh)
                    c, ends = fit.carry_ends(c, clips.get("idle"), skip=(-1,))
                pu = fit.prone_up(c, PRONE_UP, weight=wfn)
                if clears(name):
                    # (kept out of him on the way down, until he settles behind the gun as lmg_burst has it: there the
                    # butt comes into the hollow of his shoulder, which the solid fills (his arm raised off his
                    # chest), so the gun comes into the hold as the blend brings it)
                    pins = {-1: (0.0,) * 5}
                    pins.update({i_: (0.0,) * 5 for i_, t_ in enumerate(fc.times_of(L)) if t_ >= L - down})
                    if ends[0]:
                        pins[0] = (0.0,) * 5
                    pu = fit.clear_log_add(name, *fit.clear(c, pu, pins=pins))
                wc, up = fit.lift(pu, c)
                c2, worst, used = fit.refit(c, wclip=wc, poles={k: (lambda t, v=v, wfn=wfn: (np.array(v), wfn(t)))
                                                                for k, v in PRONE_POLES.items()})
                c2, _ = fit.legs_down(c2, weight=wfn)
                kind = "down behind the gun, held "
            elif (s in HOLDS) != (e in HOLDS):
                # (the rifleman's clip starts or ends with his rifle aimed in his shoulder: the gunner's starts or ends
                # in his hip hold)
                at_end = e in HOLDS
                H = HOLDS[e if at_end else s]
                wfn = ((lambda t, L=L, ez=ez: fc.sm((t - (L - ez)) / ez)) if at_end else
                       (lambda t, ez=ez: 1.0 - fc.sm(t / ez)))
                if clears(name) and "idle" in clips:
                    # (standing, he carries the gun as idle starts, not across him at the port like the rifle)
                    c = fit.carried(c, clips["idle"], wfn)
                hp = fit.hip(c, H["lean"], H["heel"], H["side"] + fit.gun.grips.get("hip_side", 0.0), H["bore_z"],
                             weight=wfn, t_ref=(L if at_end else 0.0), delta=True)
                if clears(name):
                    # (kept out of him between the carry and the hold, neither of which changes: where the blend would
                    # swing the gun through his hip, straight from the one to the other relative to his hips)
                    hold = {k: (lambda t, k=k, wfn=wfn, c=c: wfn(t) * fit.by_rifle(c, k, t)) for k in ("l", "r")}
                    hp = fit.steer(hp, deep_at=CLEAR_TOL + 0.01, held=c, hold=hold)
                    hp = fit.clear_log_add(name, *fit.clear(
                        c, hp, hold={k: (lambda t, k=k, wfn=wfn, c=c: wfn(t) * fit.by_rifle(c, k, t)) for k in ("l", "r")},
                        pins={0: (0.0,) * 5, -1: (0.0,) * 5}))
                wc, up = fit.lift(hp, c)
                la = fit.gun.grips.get("hip_l_at", H["l_at"])
                c2, worst, used = fit.refit(c, wclip=wc, poles={k: (lambda t, v=v, wfn=wfn: (np.array(v), wfn(t)))
                                                                for k, v in H["poles"].items()},
                                            l_at=(lambda t, la=la, wfn=wfn: (la, wfn(t))),
                                            hold={k: (lambda t, k=k, wfn=wfn, c=c: wfn(t) * fit.by_rifle(c, k, t))
                                                  for k in ("l", "r")}, final=name.startswith("die_"))
                kind = ("into" if at_end else "out of") + " the hip hold, held "
                if name.startswith("die_"):
                    # (a fall so violent that the hold's start makes the gun or a hand leap between keys (a blast, an
                    # arm shot away) keeps the rifleman's start: he is thrown before the hold could be seen)
                    wc0, up0 = fit.lift(c, c)
                    p2, worst0, used0 = fit.refit(c, wclip=wc0, final=True)
                    if fit.leap(c2) > fit.leap(p2) + 0.12:
                        c2, worst, used, up = p2, worst0, used0, up0
                        kind = "held (too violent a fall for the hip start) "
            else:
                wc = c
                if clears(name):
                    # (a clip that sits him down on the bench or gets him up from it starts or ends with the gun as
                    # bench_ride has it; one that starts or ends with the rifle at the port, with his own carry; between,
                    # where the rifle went through him, straight from the one to the other; and kept out of him)
                    # (where his own carry leaves the gun no clearer of him than the rifle's hold, after the search, as
                    # crouching in a doorway, the rifle's hold is kept)
                    pins0 = fit.hand_on(c, out, clips) or {}
                    tried = []
                    for carry in (True, False):
                        cv, ends = fit.carry_ends(c, clips.get("idle"), skip=tuple(pins0)) if carry else (c, (False,) * 2)
                        if not carry and not any(e_ for e_ in tried[0][2]):
                            break
                        pins = dict(pins0)
                        for k_, e_ in zip((0, -1), ends):
                            if e_:
                                pins[k_] = (0.0,) * 5
                        wv = cv if cv.get("loop") else fit.steer(cv)
                        got, rep = fit.clear(cv, wv, pins=pins or None, loop=bool(cv.get("loop")))
                        tried.append((rep["after"] + (0.0 if carry else CLEAR_KEEP), cv, ends, got, rep))
                        if rep["after"] <= CLEAR_TOL + CLEAR_KEEP:
                            break
                    _, c, _, got, rep = min(tried, key=lambda r: r[0])
                    wc = fit.clear_log_add(name, got, rep)
                wc, up = fit.lift(wc, c)
                c2, worst, used = fit.refit(c, wclip=wc, final=name.startswith("die_"),
                                            shrug=CLEAR_SHRUG if clears(name) else 0.0)
            out[name] = c2
            log.append((name, kind + "".join(s_ for s_ in used if used[s_]), round(worst * 100, 1), round(up * 100, 1)))
        elif wpn == "slung":
            out[name] = fit.slung(c)
            log.append((name, "slung", 0.0, 0.0))
        elif wpn == "laid":
            out[name] = fit.laid(c)
            log.append((name, "laid", 0.0, 0.0))
        else:
            out[name] = c
    return {k: out[k] for k in clips}


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
    gun = guns()[spec["gun"]]()
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
             f"fire_kneel fire five-round bursts from the hip (he never fires it from his shoulder like a rifle), "
             f"fire_prone is lmg_burst slowed to 1.4 s, and where the rifle rode "
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
    kinds = {}
    for n, k, _, _ in notes + tw_notes:
        kinds.setdefault(k.split(",")[0].replace(" held l", "").replace(" held r", "").replace(" held lr", "").strip(), []).append(n)
    for k, v in kinds.items():
        if "hip" in k or "behind" in k:
            log(f"{k}: {len(v)} ({', '.join(v[:40])})")
    worst = sorted(notes + tw_notes, key=lambda x: -x[2])[:8]
    log("worst misses (cm) of a fist holding fully: " + ", ".join(f"{n} {w}" for n, _, w, _ in worst))
    lifts = sorted(notes + tw_notes, key=lambda x: -x[3])[:10]
    log("gun raised off the ground (cm, most): " + ", ".join(f"{n} {u}" for n, _, _, u in lifts))
    if worst and worst[0][2] > 2.0:
        bad.append(f"fist misses up to {worst[0][2]} cm")
    miss = {n: w for n, _, w, _ in notes}
    for name, rep in getattr(fit, "clear_log", []):
        log(f"kept out of him: {name:18s} {rep['before'] * 100:5.1f} -> {rep['after'] * 100:5.1f} cm inside (his solid), "
            f"turned up to {rep['turn']:.0f} deg, set off up to {rep['off'] * 100:.0f} cm; worst fist miss "
            f"{miss.get(name, 0.0):.1f} cm")
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
