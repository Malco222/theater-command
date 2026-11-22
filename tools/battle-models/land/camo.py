"""Camouflage: the coat's second colour sprayed over the first in big uneven blotches with soft edges.

A vehicle's coat is the paint `upper`. Every point of the model says how far it is into the second coat colour,
`upper2` (the model's `vblend`; `blend` in the file, 0 to 100), and the game fills the two with a nation's own two
colours (unit-table.json: `paint`, `paint2`): a desert army's tank is sand with brown, a jungle army's green with
black-green. This lays that second colour over about a third of the coat, in blotches a foot to two feet and more
across (`cell` scales them), no two alike, running on over every joint between plates: never stripes, never spots.

How. The blotches are where a smooth field of the place (long waves running every way, summed; the same for the
fine model and the plain one) stands over a level, and the level is found on the vehicle herself so that the
blotches cover the share asked. A plate has points only at its corners, so along the edge of every blotch the
plates are cut finer (an edge at a time, and every face on that edge with it, so no seam opens) down to `grain`.
Then each point takes its share from how far inside the blotch's edge it lies, over a band `soft` wide either
side: a sprayed edge, not a masked one.

Cutting a plate must not change how it takes the light. The corners' facings are read off the model as she stood
(weighted to the big faces, her rounded edges and all) and carried onto the new points: `m.fnormal`, which
works.to_scene lays on the mesh in place of working them out again.

Left in the first colour: whatever spins (wheels), the parts named in `plain_parts` (a barrel), whatever
`plain(point)` says is inside her (an open body is sprayed from outside), and every face that is hardly seen from
anywhere level with her or above (the hull's wall behind the wheels, the back of a plate). Tracks, canvas, wood and
steel are other paints and are not touched. Nothing is cut finer where it looks down (nobody sees a belly), nor a
plate's thin cut-back edge or a small fitting for its own sake (they take their colour from where they stand).

Cost: five to nine thousand triangles on a fine model, about a thousand on a plain one. The call comes last in a
builder (`Works.camouflage`): nothing may be added to her after it.
"""

import math
import zlib

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import tessellate_polygon

HOW = ("`blend` (point by point, 0 to 100) is how far a point of the coat is into the second coat colour: camouflage "
       "blotches. Fill `upper` with the nation's paint and `upper2` with its paint2; a nation with one colour only: fill "
       "`upper2` with `upper`.")


class Field:
    """The smooth field the blotches are cut from: `inside(points)` is how far inside a blotch's edge a place lies."""

    def __init__(self, seed, cell=1.0):
        rs = np.random.RandomState(seed)

        def group(n, lo, hi, amp):
            d = rs.normal(size=(n, 3))
            d /= np.linalg.norm(d, axis=1)[:, None]
            lam = np.exp(rs.uniform(math.log(lo), math.log(hi), n)) * cell
            k = d * (2.0 * math.pi / lam)[:, None]
            k[:, 0] *= 0.8                                   # (a little longer along her length than across it)
            return k, rs.uniform(0.0, 2.0 * math.pi, n), np.full(n, amp / math.sqrt(n))
        g = [group(14, 1.0, 2.0, 1.0), group(8, 0.5, 0.8, 0.22)]      # the blotches; their ragged edges
        self.k = np.concatenate([x[0] for x in g])
        self.ph = np.concatenate([x[1] for x in g])
        self.a = np.concatenate([x[2] for x in g])
        self.level = 0.0
        self.floor = 0.4 * math.sqrt(0.5 * float(np.sum(self.a ** 2 * np.sum(self.k ** 2, axis=1))))

    def raw(self, pts):
        return np.cos(np.asarray(pts, dtype=float).reshape(-1, 3) @ self.k.T + self.ph) @ self.a

    def inside(self, pts):
        """Metres inside the edge of a blotch (negative: outside), by the field's own slope there."""
        arg = np.asarray(pts, dtype=float).reshape(-1, 3) @ self.k.T + self.ph
        f = np.cos(arg) @ self.a
        g = (np.sin(arg) * self.a) @ self.k
        return (f - self.level) / np.maximum(np.linalg.norm(g, axis=1), self.floor)


_LAT, _CELLS = {}, {}


def lattice(n):
    """The points of a triangle cut n times along each edge, as shares of its three corners."""
    if n not in _LAT:
        _LAT[n] = np.array([(i / n, j / n, (n - i - j) / n) for i in range(n + 1) for j in range(n + 1 - i)])
    return _LAT[n]


def cells(n):
    """The middles of the n * n equal triangles a triangle is cut into."""
    if n not in _CELLS:
        out = []
        for i in range(n):
            for j in range(n - i):
                out.append(((i + 1 / 3) / n, (j + 1 / 3) / n))
                if j < n - i - 1:
                    out.append(((i + 2 / 3) / n, (j + 2 / 3) / n))
        _CELLS[n] = np.array([(a, b, 1.0 - a - b) for a, b in out])
    return _CELLS[n]


def corner_normals(m, crease=50.0):
    """Every face's corners' facings as works.to_scene gives them: sharp past `crease`, weighted to the big faces."""
    me = bpy.data.meshes.new("camo-was")
    me.from_pydata([tuple(v) for v in m.verts], [], m.faces, shade_flat=False)
    me.polygons.foreach_set("use_smooth", m.fsmooth)
    me.update()
    me.set_sharp_from_angle(angle=math.radians(crease))
    ob = bpy.data.objects.new("camo-was", me)
    bpy.context.scene.collection.objects.link(ob)
    mod = ob.modifiers.new("wn", "WEIGHTED_NORMAL")
    mod.keep_sharp, mod.weight, mod.mode = True, 50, "FACE_AREA"
    for o in bpy.context.view_layer.objects:
        if o is not None:
            o.select_set(False)
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.modifier_apply(modifier="wn")
    n = np.zeros(len(me.loops) * 3)
    me.corner_normals.foreach_get("vector", n)
    n = n.reshape(-1, 3)
    if len(n) != sum(len(f) for f in m.faces):
        raise ValueError("camouflage: the mesh has not the model's corners")
    out, at = [], 0
    for f in m.faces:
        out.append(tuple(Vector(n[at + i]) for i in range(len(f))))
        at += len(f)
    bpy.data.objects.remove(ob)
    bpy.data.meshes.remove(me)
    return out


def lay(m, seed=None, cover=0.34, cell=0.66, soft=None, grain=None, plain_parts=("barrel",), plain=None, hidden=None, edge=None, least=0.15, seen=0.2, thin_least=0.05, crease=50.0, passes=64):
    """Lays the second coat colour on a model (see the top of this file). Returns what was done: the share covered,
    how many triangles the cutting added. cover: the share of the seen coat; cell: the blotches' size (1.0 makes them
    half as large again); soft: half the width of a blotch's sprayed edge; grain: how long a triangle's edge may
    stay where a blotch's edge crosses it; edge: cut along the two sides of the sprayed band (the fine model);
    plain(point), hidden(middle, facing): what stays in the first colour, what is not cut; least, thin_least: no
    face shorter, or narrower, is cut for its own sake; seen: the share of clear lines under which a face is left."""
    if "upper" not in m.paint_at or "upper2" not in m.paint_at:
        return None
    up = m.paint_at["upper"]
    soft = soft if soft is not None else (0.025 if m.fine else 0.14)
    grain = grain if grain is not None else (0.22 if m.fine else 0.5)
    seed = zlib.crc32(m.name.encode()) % 100003 if seed is None else seed
    edge = m.fine if edge is None else edge
    field = Field(seed, cell)
    V, faces = m.verts, m.faces
    fedge = getattr(m, "fedge", None)
    if fedge is None:
        fedge = m.fedge = []
    fedge.extend([0.0] * (len(faces) - len(fedge)))
    del fedge[len(faces):]
    before = m.triangles()
    laid = getattr(m, "fnormal", None)
    if laid is not None and len(laid) == len(faces) and abs(getattr(m, "fnormal_crease", -1.0) - crease) < 1e-6:
        fnor = [tuple(Vector(n) for n in nn) for nn in laid]          # (a second laying: the facings the first one carried)
    else:
        fnor = corner_normals(m, crease)
    skip = {i for i, p in enumerate(m.parts) if p.kind == "spin" or p.name in plain_parts}

    # ---- which faces carry the coat, and which of them are cut finer -------------------------------------------
    def newell(f):
        n = Vector()
        for i in range(len(f)):
            a, b = V[f[i]], V[f[(i + 1) % len(f)]]
            n += Vector(((a.y - b.y) * (a.z + b.z), (a.z - b.z) * (a.x + b.x), (a.x - b.x) * (a.y + b.y)))
        return n * 0.5

    coat, elig = [], []
    for fi, f in enumerate(faces):
        is_coat = m.fpaint[fi] == up and not m.fcoat[fi] and m.fpart[fi] not in skip
        coat.append(is_coat)
        ok = False
        if is_coat:
            n = newell(f)
            area = n.length
            if area > 1e-9 and n.z / area > -0.5:
                pts = [V[i] for i in f]
                if len(f) <= 4:
                    reach = max((pts[i] - pts[(i + 1) % len(f)]).length for i in range(len(f)))
                else:
                    reach = max((p - pts[0]).length for p in pts)
                thin = (2.0 if len(f) == 3 else 1.0) * area / max(reach, 1e-9)
                mid = sum(pts, Vector()) / len(f)
                ok = thin >= thin_least and reach >= least and not (plain is not None and plain(mid)) and not (hidden is not None and hidden(mid, n / area))
        elig.append(ok)
    if not any(elig):
        return None

    def tris_of(f):
        """A face as triangles on its own points (turned the way the face is)."""
        if len(f) == 3:
            return [(0, 1, 2)]
        n = newell(f)
        if len(f) == 4:
            p = [V[i] for i in f]
            opts = []
            for tris, (i, j) in ((((0, 1, 2), (0, 2, 3)), (0, 2)), (((0, 1, 3), (1, 2, 3)), (1, 3))):
                good = all((p[b] - p[a]).cross(p[c] - p[a]).dot(n) > -1e-12 for a, b, c in tris)
                opts.append((not good, (p[i] - p[j]).length, tris))
            opts.sort(key=lambda x: (x[0], x[1]))
            return list(opts[0][2])
        out = []
        for a, b, c in tessellate_polygon([[V[i] for i in f]]):
            if (V[f[b]] - V[f[a]]).cross(V[f[c]] - V[f[a]]).dot(n) < 0.0:
                a, b, c = c, b, a
            out.append((a, b, c))
        return out

    # ---- what is hardly seen is neither cut nor painted: a face buried in another, the back of a plate, the hull's
    # wall behind the wheels. (Seen: from places on the face, how many straight lines get clear of her, level or
    # looking down on her; under `seen` of them, the face stays in the first colour.)
    tree = BVHTree.FromPolygons([tuple(p) for p in V], [tuple(f) for f in faces])
    looks = [Vector((math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e)))
             for e in (math.radians(3.0), math.radians(24.0), math.radians(50.0)) for a in (math.radians(22.5 * k + 7.0) for k in range(16))] + [Vector((0.0, 0.0, 1.0))]
    dull, unseen = set(), set()
    for fi, f in enumerate(faces):
        if not elig[fi]:
            continue
        n = newell(f).normalized()
        ways = [d for d in looks if d.dot(n) > 0.08]
        clear = tried = 0
        for a, b, c in tris_of(f):
            pa, pb, pc = V[f[a]], V[f[b]], V[f[c]]
            k = min(5, max(1, int(math.ceil(max((pa - pb).length, (pb - pc).length, (pc - pa).length) / 0.35))))
            for wa, wb, wc in cells(k):
                p = pa * wa + pb * wb + pc * wc + n * 0.004
                tried += len(ways)
                clear += sum(1 for d in ways if tree.ray_cast(p, d)[0] is None)
        if clear < seen * max(tried, 1):
            elig[fi] = False
            dull.update(f)
            unseen.add(fi)
    # (a point a seen face stands on is painted with it, though a hidden face shares it: the roof beside a hatch's
    #  lid, a plate beside the paste. Left plain, every patch stopped short at the plate's edge.)
    for fi, f in enumerate(faces):
        if coat[fi] and fi not in unseen:
            dull.difference_update(f)
    if not any(elig):
        return None

    # ---- the level: the blotches cover the share asked, measured on the coat that is seen ---------------------
    groups = {}
    for fi, f in enumerate(faces):
        if not elig[fi]:
            continue
        for a, b, c in tris_of(f):
            pa, pb, pc = V[f[a]], V[f[b]], V[f[c]]
            area = (pb - pa).cross(pc - pa).length * 0.5
            if area < 1e-8:
                continue
            n = min(24, max(1, int(math.ceil(math.sqrt(area / 0.003)))))
            groups.setdefault(n, []).append((pa, pb, pc, area))
    vals, wts = [], []
    for n, ts in groups.items():
        T = np.array([[tuple(t[0]), tuple(t[1]), tuple(t[2])] for t in ts])
        pts = np.einsum("kb,tbx->tkx", cells(n), T).reshape(-1, 3)
        vals.append(field.raw(pts))
        wts.append(np.repeat(np.array([t[3] for t in ts]) / (n * n), n * n))
    vals, wts = np.concatenate(vals), np.concatenate(wts)
    order = np.argsort(vals)
    run = np.cumsum(wts[order])
    field.level = float(vals[order][min(int(np.searchsorted(run, (1.0 - cover) * run[-1])), len(order) - 1)])

    # ---- how far inside a blotch every point of the coat lies ---------------------------------------------------
    dv = [None] * len(V)
    ids = sorted({i for fi, f in enumerate(faces) if coat[fi] for i in f})
    for i, d in zip(ids, field.inside(np.array([tuple(V[i]) for i in ids]))):
        dv[i] = float(d)

    # ---- the cutting ---------------------------------------------------------------------------------------------
    def key(a, b):
        return (a, b) if a < b else (b, a)

    parts_cut = {m.fpart[fi] for fi in range(len(faces)) if elig[fi]}
    ef = {}
    for fi, f in enumerate(faces):
        if m.fpart[fi] in parts_cut:
            for i in range(len(f)):
                ef.setdefault(key(f[i], f[(i + 1) % len(f)]), set()).add(fi)

    cut_at = set()                                           # (the points of every face the cutting made)

    def replace(fi, tris, nors):
        """Face fi becomes the first of `tris`; the others are new faces with all that fi carries."""
        f = faces[fi]
        for t in tris:
            cut_at.update(t)
        for i in range(len(f)):
            ef[key(f[i], f[(i + 1) % len(f)])].discard(fi)
        out = []
        for j, (t, nn) in enumerate(zip(tris, nors)):
            if j == 0:
                faces[fi], fnor[fi], k = t, nn, fi
            else:
                k = len(faces)
                faces.append(t)
                fnor.append(nn)
                m.fpaint.append(m.fpaint[fi])
                m.fpart.append(m.fpart[fi])
                m.fsmooth.append(m.fsmooth[fi])
                m.fcoat.append(m.fcoat[fi])
                fedge.append(fedge[fi])
                coat.append(coat[fi])
                elig.append(elig[fi])
            for i in range(3):
                ef.setdefault(key(t[i], t[(i + 1) % 3]), set()).add(k)
            out.append(k)
        return out

    def triangulate(fi):
        f, nn = faces[fi], fnor[fi]
        if len(f) == 3:
            return [fi]
        local = tris_of(f)
        return replace(fi, [tuple(f[i] for i in t) for t in local], [tuple(nn[i] for i in t) for t in local])

    def split(ti, new):
        """Cuts a triangle at the new points on its edges (`new`: edge -> (point, share from its lower end))."""
        f, nn = faces[ti], fnor[ti]
        hit = [new.get(key(f[i], f[(i + 1) % 3])) for i in range(3)]
        count = sum(1 for h in hit if h is not None)
        if count == 0:
            return [ti]

        def mid(i):                                          # the new point on the edge from corner i, and its facing
            a, b = f[i], f[(i + 1) % 3]
            na, nb = nn[i], nn[(i + 1) % 3]
            p, t = hit[i]
            if a > b:
                t = 1.0 - t
            n = na * (1.0 - t) + nb * t
            return p, (n.normalized() if n.length > 1e-9 else na)
        if count == 1:
            i = next(k for k in range(3) if hit[k] is not None)
            a, b, c = f[i], f[(i + 1) % 3], f[(i + 2) % 3]
            na, nb, nc = nn[i], nn[(i + 1) % 3], nn[(i + 2) % 3]
            p, n = mid(i)
            return replace(ti, [(a, p, c), (p, b, c)], [(na, n, nc), (n, nb, nc)])
        if count == 2:
            i = next(k for k in range(3) if hit[(k + 2) % 3] is None)     # the edge from the corner before i is whole
            a, b, c = f[i], f[(i + 1) % 3], f[(i + 2) % 3]
            na, nb, nc = nn[i], nn[(i + 1) % 3], nn[(i + 2) % 3]
            p1, n1 = mid(i)
            p2, n2 = mid((i + 1) % 3)
            if (V[a] - V[p2]).length <= (V[c] - V[p1]).length:
                rest, rn = [(a, p1, p2), (a, p2, c)], [(na, n1, n2), (na, n2, nc)]
            else:
                rest, rn = [(a, p1, c), (p1, p2, c)], [(na, n1, nc), (n1, n2, nc)]
            return replace(ti, [(p1, b, p2)] + rest, [(n1, nb, n2)] + rn)
        a, b, c = f
        na, nb, nc = nn
        (p1, n1), (p2, n2), (p3, n3) = mid(0), mid(1), mid(2)
        return replace(ti, [(a, p1, p3), (p1, b, p2), (p3, p2, c), (p1, p2, p3)], [(na, n1, n3), (n1, nb, n2), (n3, n2, nc), (n1, n2, n3)])

    def mixed_of(cand):
        """Which of these triangles the edge of a blotch crosses: not all of one colour."""
        out, deep = [], {}
        for ti in cand:
            a, b, c = faces[ti]
            lo, hi = min(dv[a], dv[b], dv[c]), max(dv[a], dv[b], dv[c])
            if lo < soft and hi > -soft:
                out.append(ti)
                continue
            reach = max((V[a] - V[b]).length, (V[b] - V[c]).length, (V[c] - V[a]).length)
            if reach > 0.09:
                deep.setdefault(min(12, max(2, int(math.ceil(reach / 0.09)))), []).append((ti, lo, hi))
        for n, ts in deep.items():
            T = np.array([[tuple(V[i]) for i in faces[t[0]]] for t in ts])
            d = field.inside(np.einsum("kb,tbx->tkx", lattice(n), T).reshape(-1, 3)).reshape(len(ts), -1)
            for (ti, lo, hi), dmin, dmax in zip(ts, d.min(axis=1), d.max(axis=1)):
                if min(lo, dmin) < soft and max(hi, dmax) > -soft:
                    out.append(ti)
        return out

    def cut(new_at):
        """Makes the new points (`new_at`: edge -> share from its lower end) and cuts every face on those edges.
        Returns the faces that came of it."""
        new = {}
        start = len(V)
        for (a, b), t in new_at.items():
            V.append(V[a] * (1.0 - t) + V[b] * t)
            m.vcoat.append(m.vcoat[a])
            m.vblend.append(0.0)
            m.vsoot.append(m.vsoot[a] * (1.0 - t) + m.vsoot[b] * t)
            new[(a, b)] = (len(V) - 1, t)
        dv.extend(float(d) for d in field.inside(np.array([tuple(p) for p in V[start:]])))
        hit = set()
        for e in new:
            hit |= ef.get(e, set())
        made = []
        for fi in sorted(hit):
            for ti in triangulate(fi):
                made.extend(split(ti, new))
        return made

    # (first, every face the edge of a blotch crosses becomes triangles)
    cand = []
    for fi in [k for k in range(len(faces)) if elig[k]]:
        f = faces[fi]
        if len(f) == 3:
            cand.append(fi)
            continue
        ds = [dv[i] for i in f]
        reach = max((V[i] - V[f[0]]).length for i in f)
        crossed = min(ds) < soft and max(ds) > -soft
        if not crossed and reach > 0.09:
            pts = np.array([[tuple(V[f[i]]) for i in t] for t in tris_of(f)])
            d = field.inside(np.einsum("kb,tbx->tkx", lattice(min(12, max(2, int(math.ceil(reach / 0.09))))), pts).reshape(-1, 3))
            crossed = min(min(ds), float(d.min())) < soft and max(max(ds), float(d.max())) > -soft
        if crossed and reach > grain:
            cand.extend(triangulate(fi))
    done, band = 0, set()
    for _ in range(passes):
        new_at = {}
        for ti in mixed_of(cand):
            a, b, c = faces[ti]
            ls = [((V[a] - V[b]).length, key(a, b)), ((V[b] - V[c]).length, key(b, c)), ((V[c] - V[a]).length, key(c, a))]
            if max(ls)[0] <= grain:
                band.add(ti)
                continue
            new_at[max(ls)[1]] = 0.5
        if not new_at:
            break
        cand = [k for k in cut(new_at) if elig[k]]
        done += 1
    # (then, on the fine model, the edge itself: the triangles the band crosses are cut along its two sides, where the
    #  second colour begins and where it is whole, so the band is as wide as `soft` says whatever the triangles' size)
    for level in ((-soft, soft) if edge else ()):
        for _ in range(4):
            want = {}
            for ti in band:
                f = faces[ti]
                for i in range(3):
                    u, w = f[i], f[(i + 1) % 3]
                    if u > w:
                        u, w = w, u
                    du, dw = dv[u] - level, dv[w] - level
                    if (u, w) not in want and du * dw < 0.0:
                        want[(u, w)] = du / (du - dw)
            if not want:
                break
            # (the field bends along an edge: look where the straight guess falls and guess again from the nearer side)
            es = list(want)
            ds = field.inside(np.array([tuple(V[u] * (1.0 - want[(u, w)]) + V[w] * want[(u, w)]) for u, w in es])) - level
            new_at = {}
            for (u, w), dm in zip(es, ds):
                t0, du, dw = want[(u, w)], dv[u] - level, dv[w] - level
                t = t0 * du / (du - dm) if du * dm < 0.0 else (t0 + (1.0 - t0) * dm / (dm - dw) if dm * dw < 0.0 else t0)
                reach = (V[u] - V[w]).length
                if min(t, 1.0 - t) * reach > 0.012:
                    new_at[(u, w)] = t
            if not new_at:
                break
            band |= {k for k in cut(new_at) if elig[k]}
            done += 1

    # ---- each point's share of the second colour -----------------------------------------------------------------
    # (Where nothing was cut, faces too small to cut or a plate already cut small for her dirt, a blotch's edge can only
    #  step from point to point: there the band is as wide as the points stand apart, so that the edge does not run
    #  along the mesh's own lines as a row of blocks.)
    span = {}
    for fi, f in enumerate(faces):
        if coat[fi]:
            far = max((V[f[i]] - V[f[(i + 1) % len(f)]]).length for i in range(len(f)))
            for i in f:
                if i not in cut_at:
                    span[i] = max(span.get(i, 0.0), far)
    ids = [i for i in range(len(V)) if dv[i] is not None]
    for i in ids:
        if i in dull or (plain is not None and plain(V[i])):
            m.vblend[i] = 0.0
            continue
        wide = max(soft, 0.55 * min(span.get(i, 0.0), 0.3))
        t = min(max(dv[i] / (2.0 * wide) + 0.5, 0.0), 1.0)
        m.vblend[i] = t * t * (3.0 - 2.0 * t)
    got, whole = 0.0, 0.0
    for fi, f in enumerate(faces):
        if elig[fi] and len(f) == 3:
            area = (V[f[1]] - V[f[0]]).cross(V[f[2]] - V[f[0]]).length * 0.5
            got += area * (m.vblend[f[0]] + m.vblend[f[1]] + m.vblend[f[2]]) / 3.0
            whole += area
        elif elig[fi]:
            area = newell(f).length
            got += area * sum(m.vblend[i] for i in f) / len(f)
            whole += area
    m.fnormal = [tuple(tuple(n) for n in nn) for nn in fnor]
    m.fnormal_crease = crease
    m.info.setdefault("file", {})["camouflage"] = {"how": HOW, "cover": round(got / max(whole, 1e-9), 2)}
    return {"cover": got / max(whole, 1e-9), "coat": whole, "added": m.triangles() - before, "passes": done, "seed": seed, "field": field, "soft": soft}


HOW3 = (" `blend2` (the same, where the mesh has it) is how far the point is into a third coat colour, `upper3`, sprayed over the "
        "second where they meet: colour = mix(mix(upper, upper2, blend), upper3, blend2). A reader that knows only `blend` shows two colours.")


def lay3(m, cover=(0.28, 0.26), seed=None, **kw):
    """Two colours sprayed over the first (the German three-colour coat of 1943 on): `lay` twice, each with blotches
    of its own, the second colour's in `vblend` and the third's in `vblend2`. cover: each colour's share of the seen
    coat (they overlap by chance, so the first keeps about (1 - a)(1 - b))."""
    seed = zlib.crc32(m.name.encode()) % 100003 if seed is None else seed
    a = lay(m, seed=seed, cover=cover[0], **kw)
    if a is None:
        return None
    n0, first = len(m.verts), list(m.vblend)
    b = lay(m, seed=seed + 7919, cover=cover[1], **kw)
    second = list(m.vblend)
    new = list(range(n0, len(m.verts)))
    if new:
        # (the points the second cutting made: how far each lies inside the first colour's blotches)
        plain = kw.get("plain")
        for i, d in zip(new, a["field"].inside(np.array([tuple(m.verts[i]) for i in new]))):
            t = min(max(float(d) / (2.0 * a["soft"]) + 0.5, 0.0), 1.0)
            first.append(0.0 if second[i] == 0.0 and plain is not None and plain(m.verts[i]) else t * t * (3.0 - 2.0 * t))
    m.vblend, m.vblend2 = first, second
    m.info["file"]["camouflage"] = {"how": HOW + HOW3, "cover": round(a["cover"], 2), "cover2": round(b["cover"], 2)}
    return {"cover": a["cover"], "cover2": b["cover"], "added": a["added"] + b["added"], "seed": seed}
