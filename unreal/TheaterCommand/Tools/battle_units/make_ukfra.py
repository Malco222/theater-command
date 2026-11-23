"""The British and French soldiers for the ground battles, until their own are made: the Pole's man (soldier_POL.json,
the nearest cut, which the battles already draw for them) under his own nation's helmet, the British Brodie (Mk II)
or the French Adrian (M26), each with its chin strap, at near, mid and far, and in its field finishes, made exactly as
every other army's are (variety.py's helmet_entry), so the game reads them as it reads any soldier file.

    /home/user/bpyenv/bin/python make_ukfra.py OUTDIR [UK FRA]

The shells are soldier.py's own helmets (battle-models/land/soldier/helmets/<kind>.glb, the head bone's frame), set on
the Pole's head where his wz.31 was, then lifted until no point of his head shows through (the Brodie as drawn sits a
few centimetres into his skull: it was never fitted to a man before), the chin strap stretched up to its new lugs. The
Pole's helmet and its points are taken out and the new shell's put in their place (every index after them moved by
the difference: triangles, and the faces' `at` and `tint_at`); the mid and far shells are the near one decimated to
the Pole's own helmet's triangle counts. Head points the coarser mid and far shells would cut are pushed under them.
Then his finishes (FINISHES below) and the forehead tucked under the brim, as variety.py does for every file.
The wear arrays (shade, tint, dirt, wet, snow) are baked afterwards by shade.py, as for every soldier file.
"""

import copy
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
# variety.py, helmet_finishes.py and human.py are the looks thread's (the o6cg2k branch's Tools/battle_units): put that
# folder beside this one, or name it in LOOKS_TOOLS. The finishes are the same code every other army's file was made by.
sys.path.insert(1, os.environ.get("LOOKS_TOOLS", "/home/user/work/looks/battle_units"))

import bpy  # noqa: E402,F401
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402
from scipy.spatial import cKDTree  # noqa: E402

import variety  # noqa: E402
from glbio import glb, to_json  # noqa: E402
from fitcheck import crossings  # noqa: E402
import ukfra_finishes as UF  # noqa: E402

SOLDIERS = os.environ.get("SOLDIERS", "/mnt/project-files/battle-models/land/soldier/")   # (soldier_POL.json, helmets/)
LODS = ("near", "mid", "far")
OFF = np.array((-0.0005, 0.0, -0.0015))    # (the glb shells against the files' own helmets: the round one sits here)
KINDS = {"UK": ("brodie", 0.032), "FRA": ("adrian", 0.015)}   # (the shell, and how far it is lifted to clear his head)
CHIN = 1.53                                # (below this the chin strap stays where it is: under his chin)
PER_POINT = ("paints", "part", "part2", "w2", "shade", "shade_clean", "dirt", "dirt_wet", "wet", "snow")
PER_POINT3 = ("points", "facings", "tint")
GAP = 0.003                                # (a head point pushed under the shell goes this far inside it)


def smooth(x):
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3 - 2 * x)


# ------------------------------------------------------------------------------------------------ the shells
def decimate(P, N, T, target):
    """The shell with about `target` triangles (Blender's collapse), its points merged first so the cut seams hold."""
    me = bpy.data.meshes.new("shell")
    Pb = P * (1, -1, 1)
    me.from_pydata([tuple(p) for p in Pb], [], [tuple(int(i) for i in t) for t in T])   # (the y flip turns the file's
    #                                                  winding into Blender's: outward normals)
    me.update()
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new("shell", me)
    bpy.context.scene.collection.objects.link(ob)
    mod = ob.modifiers.new("dec", "DECIMATE")
    mod.ratio = min(1.0, target / max(1, len(me.polygons)))
    mod.use_collapse_triangulate = True
    dg = bpy.context.evaluated_depsgraph_get()
    ev = ob.evaluated_get(dg)
    m2 = bpy.data.meshes.new_from_object(ev)
    bm = bmesh.new()
    bm.from_mesh(m2)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    bm.verts.ensure_lookup_table()
    bm.normal_update()
    V = np.array([v.co[:] for v in bm.verts]) * (1, -1, 1)
    Nv = np.array([v.normal[:] for v in bm.verts]) * (1, -1, 1)
    Tt = np.array([[v.index for v in f.verts] for f in bm.faces])
    bm.free()
    bpy.data.objects.remove(ob)
    return V, Nv, Tt


def shells(D, kind, lift):
    """The new helmet at each level: points, facings, triangles (the file's winding), json frame, on his head."""
    hh = np.array([b for b in D["bones"] if b["name"] == "head"][0]["head"])
    g = glb(SOLDIERS + f"helmets/{kind}.glb")["helmet"]
    P = to_json(g["P"]) + hh + OFF + np.array((0.0, 0.0, lift))
    N = to_json(g["N"])
    T = g["T"]
    out = {"near": (P, N / np.linalg.norm(N, axis=1, keepdims=True), T)}
    for lod in ("mid", "far"):
        target = D["helmet_tris"][lod][1]
        out[lod] = decimate(P, N, T, target)
    return out


def straps(D, kind, lift):
    """The chin strap's points at each level (their indices in the file, and where they go): the strap the Pole's
    helmet hangs moved to the new helmet's lugs, as soldier.py hangs it from each helmet, stretched up by the lift."""
    hh = np.array([b for b in D["bones"] if b["name"] == "head"][0]["head"])
    rs = to_json(glb(SOLDIERS + "helmets/round.glb")["straps"]["P"]) + hh + OFF
    ks = to_json(glb(SOLDIERS + f"helmets/{kind}.glb")["straps"]["P"]) + hh + OFF
    hb = [b["name"] for b in D["bones"]].index("head")
    top = ks[:, 2].max()
    disp = ks - rs
    disp[:, 2] += lift * smooth((ks[:, 2] - CHIN) / (top - CHIN))
    m = D["near"]
    P = np.array(m["points"]).reshape(-1, 3)
    pa, part = np.array(m["paints"]), np.array(m["part"])
    near_idx = np.where((pa == variety.PAINT["straps"]) & (part == hb))[0]
    _, k = cKDTree(rs).query(P[near_idx])
    near_disp = disp[k]
    out = {"near": (near_idx, P[near_idx] + near_disp)}
    tree = cKDTree(P[near_idx])
    for lod in ("mid", "far"):
        m = D[lod]
        Q = np.array(m["points"]).reshape(-1, 3)
        pa, part = np.array(m["paints"]), np.array(m["part"])
        idx = np.where((pa == variety.PAINT["straps"]) & (part == hb))[0]
        _, j = tree.query(Q[idx])
        out[lod] = (idx, Q[idx] + near_disp[j])
    return out


# ------------------------------------------------------------------------------------------------ the splice
def splice(D, lod, newP, newN, newT, strap):
    """The Pole's helmet out of one level and the new shell in: its points where his were, every later index moved."""
    m = D[lod]
    a, c = D["helmet_tris"][lod]
    T = np.array(m["triangles"]).reshape(-1, 3)
    H, B = T[a:a + c], T[:a]
    used = np.unique(H)
    assert not np.isin(B, used).any(), f"{lod}: the body shares the helmet's points"
    n_old = len(m["paints"])
    old_P = np.array(m["points"]).reshape(-1, 3)
    s = int(used.min())
    # the strap first (indices before the splice)
    sidx, sP = strap
    for i, p in zip(sidx, sP):
        m["points"][3 * i:3 * i + 3] = [round(float(v), 4) for v in p]
    # the new points' wear and look from the nearest of the old helmet's (baked again by shade.py afterwards)
    _, near = cKDTree(old_P[used]).query(newP)
    src = used[near]
    k = len(newP)
    gone = np.zeros(n_old, bool)
    gone[used] = True
    before = np.cumsum(gone) - gone                    # (how many of the old helmet's points come before each point)
    remap = np.where(np.arange(n_old) < s, np.arange(n_old), np.arange(n_old) - before + k)
    keep_lo = [i for i in range(s) if not gone[i]]
    keep_hi = [i for i in range(s, n_old) if not gone[i]]
    hb = [b["name"] for b in D["bones"]].index("head")
    for key in PER_POINT:
        if key not in m:
            continue
        old = m[key]
        if key == "paints":
            mid = [variety.PAINT["helmet"]] * k
        elif key in ("part", "part2"):
            mid = [hb] * k
        elif key == "w2":
            mid = [0.0] * k
        else:
            mid = [old[int(i)] for i in src]
        m[key] = [old[i] for i in keep_lo] + mid + [old[i] for i in keep_hi]
    for key in PER_POINT3:
        if key not in m:
            continue
        old = m[key]
        if key == "points":
            mid = [round(float(v), 4) for v in newP.ravel()]
        elif key == "facings":
            mid = [round(float(v), 3) for v in newN.ravel()]
        else:
            mid = [v for i in src for v in old[3 * int(i):3 * int(i) + 3]]
        three = lambda idx: [v for i in idx for v in old[3 * i:3 * i + 3]]
        m[key] = three(keep_lo) + mid + three(keep_hi)
    # indices: the body's triangles, then the new shell's last (as variety.py leaves them)
    B2 = remap[B]
    T2 = np.vstack([B2, newT + s])
    m["triangles"] = [int(i) for i in T2.ravel()]
    D["helmet_tris"][lod] = [int(len(B2)), int(len(newT))]
    # the faces' points
    for f in D.get("faces", []):
        fl = f.get(lod)
        if not fl:
            continue
        for key in ("at", "tint_at"):
            if key in fl:
                arr = np.array(fl[key], int)
                assert not gone[arr].any(), f"a face moves a helmet point ({lod})"
                fl[key] = [int(i) for i in remap[arr]]
    delta = k - len(used)
    n_new = len(m["paints"])
    assert n_new == n_old + delta and len(m["points"]) == 3 * n_new
    return s, k


def push_under(D, lod, log):
    """Head points the shell would cut (beyond its outside, seen from the middle of his head) pushed in under it."""
    m = D[lod]
    a, c = D["helmet_tris"][lod]
    T = np.array(m["triangles"]).reshape(-1, 3)
    P = np.array(m["points"]).reshape(-1, 3)
    pa, part = np.array(m["paints"]), np.array(m["part"])
    hb = [b["name"] for b in D["bones"]].index("head")
    used = np.unique(T[a:a + c])
    bvh = BVHTree.FromPolygons([tuple(p) for p in P], [tuple(int(i) for i in t) for t in T[a:a + c]])
    hh = np.array([b for b in D["bones"] if b["name"] == "head"][0]["head"])
    o = hh + np.array((0.0, 0.0, 0.015))
    moved = 0
    for i in np.where((part == hb) & (pa != variety.PAINT["helmet"]) & (pa != variety.PAINT["straps"]))[0]:
        if i in used or P[i, 2] < o[2]:
            continue
        if crossings(bvh, o, P[i]) >= 1:
            d = Vector(P[i] - o)
            hit, _, _, dist = bvh.ray_cast(Vector(o), d.normalized(), d.length)
            if hit is None:
                continue
            q = o + np.array(d.normalized()) * max(0.0, dist - GAP)
            m["points"][3 * i:3 * i + 3] = [round(float(v), 4) for v in q]
            moved += 1
    log(f"   {lod}: {moved} head points pushed under the shell")
    return moved


def roof(D):
    P = np.array(D["near"]["points"]).reshape(-1, 3)
    a, c = D["helmet_tris"]["near"]
    T = np.array(D["near"]["triangles"]).reshape(-1, 3)[a:a + c]
    return round(float(P[np.unique(T), 2].max()), 3)


# ------------------------------------------------------------------------------------------------ one army
def make(army, outdir, log=print):
    kind, lift = KINDS[army]
    D = json.load(open(SOLDIERS + "soldier_POL.json"))
    for k in ("helmets", "inks"):
        D.pop(k, None)
    sh = shells(D, kind, lift)
    st = straps(D, kind, lift)
    for lod in LODS:
        P, N, T = sh[lod]
        was = list(D["helmet_tris"][lod])
        s, k = splice(D, lod, P, N, T, st[lod])
        log(f"   {lod}: {kind} {k} points, {len(T)} triangles (the wz.31's {was[1]})")
    for lod in LODS:
        push_under(D, lod, log)
    tucked = variety.tuck_under(D)
    log(f"   forehead tucked under the brim: {tucked}")
    inks = variety.Inks()
    D["helmets"] = [variety.helmet_entry(D, r, inks) for r in UF.RECIPES[army]]
    D["inks"] = inks.list
    D["marks"]["roof"] = roof(D)
    D["name"] = f"soldier_{army}"
    D["about"] = UF.ABOUT[army] + " " + D["about"]
    out = os.path.join(outdir, f"soldier_{army}.json")
    tmp = out + ".tmp"
    with open(tmp, "w") as f:
        json.dump(D, f, separators=(",", ":"))
    json.load(open(tmp))
    os.replace(tmp, out)
    log(f"{army}: {out} ({os.path.getsize(out):,} bytes), finishes {[h['name'] for h in D['helmets']]}, roof {D['marks']['roof']}")
    return D


if __name__ == "__main__":
    a = [x for x in sys.argv[1:] if x != "--"]
    outdir = a[0]
    os.makedirs(outdir, exist_ok=True)
    for army in (a[1:] or ["UK", "FRA"]):
        make(army, outdir)
