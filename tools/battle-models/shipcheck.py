"""Two looks over a whole ship that a picture of one corner of her cannot give.

    python3 shipcheck.py loose <names|all> [gap=0.5]   # every piece of her that touches nothing else: a fitting hung in the air
    python3 shipcheck.py boxes <names|all>             # her largest plain boxes and bars, to see that none is out of scale

A piece is a mesh island (points joined by edges). `loose` passes a piece that lies within `gap` metres of another,
looking all along the faces of a small box or bar and not only at its corners. `boxes` found the 20 m sheet over the
Japanese towers, which was one solid piece standing on its post and so not loose at all: run both after a change.
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy  # noqa: F401  (Blender must be loaded before the modules that use it)
import bmesh
from mathutils import Vector
from mathutils.bvhtree import BVHTree

import kit
import rigid
import ships
import stage


def pieces(bm):
    """The mesh's islands, each a list of its points."""
    seen = [False] * len(bm.verts)
    out = []
    for v in bm.verts:
        if seen[v.index]:
            continue
        stack, vs = [v], []
        seen[v.index] = True
        while stack:
            a = stack.pop()
            vs.append(a)
            for e in a.link_edges:
                b = e.other_vert(a)
                if not seen[b.index]:
                    seen[b.index] = True
                    stack.append(b)
        out.append(vs)
    return out


def box(vs):
    lo = Vector((min(q.co.x for q in vs), min(q.co.y for q in vs), min(q.co.z for q in vs)))
    hi = Vector((max(q.co.x for q in vs), max(q.co.y for q in vs), max(q.co.z for q in vs)))
    return lo, hi


def built(name):
    kit.fresh()
    m, _ = ships.BUILDERS[name](True)
    ob, _ = rigid.to_blender(m, stage.ship_look)
    me = ob.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.verts.ensure_lookup_table()
    bm.faces.ensure_lookup_table()
    return ob, bm, [mt.name.split(":")[-1] for mt in me.materials]


def loose(name, gap):
    t0 = time.time()
    ob, bm, mats = built(name)
    groups = [g.name for g in ob.vertex_groups]
    dl = bm.verts.layers.deform.verify()
    isl = pieces(bm)
    at = {}
    for i, vs in enumerate(isl):
        for v in vs:
            at[v.index] = i
    face_piece = [at[f.verts[0].index] for f in bm.faces]
    tree = BVHTree.FromBMesh(bm)
    found = []
    for i, vs in enumerate(isl):
        pts = [v.co for v in vs[::max(1, len(vs) // 120)]]
        if len(vs) <= 64:                                # (a plain box or bar: look all along its faces)
            done = set()
            for v in vs:
                for f in v.link_faces:
                    if f.index in done or len(f.verts) != 4:
                        continue
                    done.add(f.index)
                    a, b, c, d = (q.co for q in f.verts)
                    nu = min(60, max(1, int((b - a).length / 0.4)))
                    nv = min(60, max(1, int((d - a).length / 0.4)))
                    for iu in range(nu + 1):
                        for iv in range(nv + 1):
                            pts.append(a.lerp(b, iu / nu).lerp(d.lerp(c, iu / nu), iv / nv))
        if any(face_piece[fi] != i for p in pts for _, _, fi, _ in tree.find_nearest_range(p, gap)):
            continue
        lo, hi = box(vs)
        f = next(iter(vs[0].link_faces), None)
        w = vs[0][dl].items()
        found.append((tuple(round(q, 1) for q in (lo + hi) * 0.5), tuple(round(q, 1) for q in hi - lo), len(vs),
                      mats[f.material_index] if f else "?", groups[max(w, key=lambda kv: kv[1])[0]] if w else "?"))
    lo, hi = box(bm.verts)
    print(f"{name}: {len(isl)} pieces, {len(bm.verts)} points, x {lo.x:.1f}..{hi.x:.1f}, beam {hi.y - lo.y:.1f}, top {hi.z:.1f}; "
          f"loose {len(found)} ({time.time() - t0:.0f}s)", flush=True)
    for row in found[:40]:
        print("   at", row[0], "size", row[1], "points", row[2], "paint", row[3], "part", row[4], flush=True)
    bm.free()
    return len(found)


def boxes(name, show=9):
    ob, bm, mats = built(name)
    rows = []
    for vs in pieces(bm):
        if len(vs) > 24:
            continue
        lo, hi = box(vs)
        size = hi - lo
        f = vs[0].link_faces[0] if vs[0].link_faces else None
        rows.append((max(size), tuple(round(q, 1) for q in size), tuple(round(q, 1) for q in (lo + hi) * 0.5), mats[f.material_index] if f else "?", len(vs)))
    rows.sort(reverse=True)
    lo, hi = box(bm.verts)
    print(f"{name}: {hi.x - lo.x:.0f} m long", flush=True)
    shown = set()
    for _, size, mid, paint, n in rows:
        if (size, paint) in shown:
            continue
        shown.add((size, paint))
        if len(shown) > show:
            break
        print("   size", size, "at", mid, "paint", paint, "points", n, flush=True)
    bm.free()


if __name__ == "__main__":
    what = sys.argv[1]
    names = list(ships.BUILDERS) if sys.argv[2] == "all" else sys.argv[2].split(",")
    gap = float(next((a.split("=")[1] for a in sys.argv[3:] if a.startswith("gap=")), 0.5))
    bad = 0
    for one in names:
        if what == "loose":
            bad += loose(one, gap)
        else:
            boxes(one)
    sys.exit(1 if bad else 0)
