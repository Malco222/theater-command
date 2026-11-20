"""Where a unit's earned paint goes (the sandbox's reputation marks).

Brian, 2026-11-19: "i want airplane squadrons to get nick names if they keep winning battles and get a reputation",
"for example the Red Tails", "same with tanks and battalions of troops". The rules (PR #16, `src/campaign/reputation.ts`)
save on a unit that has made a name `mark: {at, colour, emblem?}`; /mnt/project-files/sandbox/REPUTATION-MARKS.md says
what each `at` means. This module finds those places on a battle model, so the game can paint them:

- `unit_marks` in the file: `places` (the `at` words this model has, in the order of their bits), `emblems` (where a
  badge goes: a place on each side, the way the skin faces there, its radius and the part it rides), and on a tank with
  an aerial the socket `pennant`.
- `unit_mark` in `near` and `far`: for every point, which places it lies in, as bits (bit k is `places[k]`; 0: none).
  A point can lie in two (the rudder is part of the tail; the front of the cowling is part of the cowling).

The game paints a point in the mark's colour where its bit is set for the unit's `at`, then lays the point's own wear
and soot over it as for any paint (flat and a little worn, never new). An emblem is a decal laid like a marking. The
.glb gives every painted place its own material slots too (`<model>:<paint>|<place>+<place>`), for Unreal's importer.

Everything is found from the finished mesh (the faces' paints, parts and places), so it holds for every aircraft and
armoured vehicle the tools build, the what-if machines among them. Nothing here changes how a model looks (a
tank's two bands are cut into her plates along level lines: more edges, the same shape).
"""
import math

import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

AIR_PLACES = ("tail", "rudder", "nose", "spinner", "cowling", "wingtips", "fuselage band")
LAND_PLACES = ("turret ring", "hull band", "hatches")
AIR_SKIN = ("upper", "upper2", "under", "nose", "line", "rivet", "rivet_under", "seam", "seam_under")
LAND_SKIN = ("upper", "upper2", "shade")
ARMOUR = ("tank", "car")                     # (the arm corps' machines: tanks, assault guns, tank hunters, armoured cars)

HOW = ("A unit that has made a name wears its own paint (the rules' `mark`: at, colour, emblem). `places` are the "
       "`at` words this model has; in `near` and `far`, `unit_mark` gives every point the places it lies in as bits "
       "(bit k: places[k]). Paint a point whose bit for the unit's `at` is set in the mark's colour, then its own wear "
       "and soot over that: flat and a little worn, never new. With an `emblem`, lay the badge as a marking at that "
       "place's entries in `emblems` instead (each side its own entry; `radius` is half its height). `pennant` is the "
       "socket at the top of the aerial where a small pennant in the colour flies.")


# ---- the mesh as numbers ------------------------------------------------------------------------------------

def geometry(m):
    """Every face's middle, the way it faces (a unit vector) and its area, as arrays."""
    V = np.array([tuple(v) for v in m.verts], dtype=float)
    n = len(m.faces)
    C, N, A = np.zeros((n, 3)), np.zeros((n, 3)), np.zeros(n)
    by_len = {}
    for k, f in enumerate(m.faces):
        by_len.setdefault(len(f), []).append(k)
    for L, ks in by_len.items():
        ks = np.array(ks)
        idx = np.array([m.faces[k] for k in ks])
        P = V[idx]                                   # (faces, L, 3)
        C[ks] = P.mean(axis=1)
        nn = np.zeros((len(ks), 3))
        for i in range(L):                           # (Newell's way: right for any polygon)
            nn += np.cross(P[:, i], P[:, (i + 1) % L])
        area = np.linalg.norm(nn, axis=1)
        A[ks] = 0.5 * area
        N[ks] = nn / np.maximum(area, 1e-12)[:, None]
    return C, N, A


def cut_z(m, faces, z0, eps=1e-5):
    """Cuts the faces numbered in `faces` along the level z0, so that a band's edge runs straight across a plate
    instead of stepping from face to face. Each face that crosses it keeps the piece below and the piece above is
    added with the same paint, part and coat; a point made on an edge is shared by both faces of that edge."""
    nf = len(m.faces)
    if hasattr(m, "fedge"):
        m.fedge = list(m.fedge)[:nf] + [0.0] * max(0, nf - len(m.fedge))
    per_face = [a for a in ("fpaint", "fpart", "fsmooth", "fcoat", "fedge") if len(getattr(m, a, ())) == nf]
    per_point = [a for a in ("vcoat", "vblend", "vsoot") if len(getattr(m, a, ())) == len(m.verts)]
    made = {}

    def point(i, j):
        key = (min(i, j), max(i, j))
        if key not in made:
            a, b = m.verts[i], m.verts[j]
            t = (z0 - a.z) / (b.z - a.z)
            made[key] = len(m.verts)
            m.verts.append(a + (b - a) * t)
            for at in per_point:
                lst = getattr(m, at)
                lst.append(lst[i] if (at == "vcoat" and t < 0.5) else lst[j] if at == "vcoat" else lst[i] + (lst[j] - lst[i]) * t)
        return made[key]

    cut = 0
    for fi in faces:
        f = m.faces[fi]
        zs = [m.verts[i].z - z0 for i in f]
        if min(zs) > -eps or max(zs) < eps:
            continue
        below, above = [], []
        for k in range(len(f)):
            i, j, zi, zj = f[k], f[(k + 1) % len(f)], zs[k], zs[(k + 1) % len(f)]
            if zi <= eps:
                below.append(i)
            if zi >= -eps:
                above.append(i)
            if (zi < -eps and zj > eps) or (zi > eps and zj < -eps):
                p = point(i, j)
                below.append(p)
                above.append(p)
        if len(below) >= 3 and len(above) >= 3:
            m.faces[fi] = tuple(below)
            m.faces.append(tuple(above))
            for at in per_face:
                getattr(m, at).append(getattr(m, at)[fi])
            cut += 1
    return cut


def pieces(m):
    """Which piece of the mesh every face belongs to (faces that share a point are one piece), as a number per face."""
    parent = list(range(len(m.verts)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for fc in m.faces:
        r0 = find(fc[0])
        for i in fc[1:]:
            ri = find(i)
            if ri != r0:
                parent[ri] = r0
    return np.array([find(fc[0]) for fc in m.faces], dtype=np.int64)


def hull2d(P):
    """The convex outline of points P (n, 2), anticlockwise (Andrew's way)."""
    pts = sorted(set(map(tuple, np.round(P, 4))))
    if len(pts) < 3:
        return np.array(pts)

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    lo, hi = [], []
    for p in pts:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], p) <= 0:
            lo.pop()
        lo.append(p)
    for p in reversed(pts):
        while len(hi) >= 2 and cross(hi[-2], hi[-1], p) <= 0:
            hi.pop()
        hi.append(p)
    return np.array(lo[:-1] + hi[:-1])


def outside(poly, Q):
    """For points Q (n, 2): how far outside the anticlockwise outline `poly` each lies (less than 0: inside), and the
    outward way of the outline's nearest edge."""
    a, b = poly, np.roll(poly, -1, axis=0)
    e = b - a
    L2 = np.maximum((e ** 2).sum(axis=1), 1e-12)
    t = np.clip(((Q[:, None, :] - a[None]) * e[None]).sum(axis=2) / L2[None], 0.0, 1.0)
    near = a[None] + t[..., None] * e[None]
    d = np.linalg.norm(Q[:, None, :] - near, axis=2)
    k = d.argmin(axis=1)
    out = np.stack([e[:, 1], -e[:, 0]], axis=1) / np.sqrt(L2)[:, None]      # (anticlockwise: the outward way is to the right)
    side = ((Q - a[k]) * out[k]).sum(axis=1)
    inside_all = np.all(((Q[:, None, :] - a[None]) * out[None]).sum(axis=2) <= 1e-9, axis=1)
    dist = np.where(inside_all, -d[np.arange(len(Q)), k], np.abs(d[np.arange(len(Q)), k]))
    return np.where(inside_all, dist, np.maximum(dist, side)), out[k]


def open_to_view(m, tree, k, c, n, reach=0.5):
    """Whether face k (middle c, facing n) is open to view along its facing: nothing of her stands within `reach` in
    front of it, at its middle and near each corner, for at least half of those (a long plate half behind a fuel drum
    is not hidden; a plate wholly behind one is)."""
    fc = m.faces[k]
    corners = [fc[int(i)] for i in np.linspace(0, len(fc) - 1, min(len(fc), 8))]
    nv = Vector(n)
    pts = [Vector(c)] + [Vector(c) + (m.verts[i] - Vector(c)) * 0.8 for i in corners]
    seen = sum(tree.ray_cast(p + nv * 0.03, nv, reach)[0] is None for p in pts)
    return 2 * seen >= len(pts)


def on_outside(m, C, N, A, cand, pool, b0, b1, piece, tree):
    """The faces of `cand` that lie on the outside of the plates in `pool` at the level b0..b1: not a fitting that
    stands proud of them (a fuel drum, a box, a hood, a smoke discharger), not a plate hidden behind one, nor the inside
    of an open fighting compartment. A face is kept when it looks outward (away from the middle of her at that level),
    nothing stands in front of it within half a metre, and it is either on one of the big pieces of the pool (a quarter
    the area of the biggest or more: her plates) or on the outline drawn round them (where a sponson or a pulpit
    stands out, the plates behind it are still her own; a drum standing off them is not)."""
    if not cand.any() or not pool.any():
        return cand
    area = {}
    for pc, a_ in zip(piece[pool], A[pool]):
        area[pc] = area.get(pc, 0.0) + a_
    big = {pc for pc, a_ in area.items() if a_ >= 0.25 * max(area.values())}
    V = np.array([tuple(v) for v in m.verts], dtype=float)
    idx = sorted({i for k in np.nonzero(pool & np.isin(piece, list(big)))[0] for i in m.faces[k]})
    P = V[idx]
    P = P[(P[:, 2] >= b0 - 0.012) & (P[:, 2] <= b1 + 0.012)]
    if len(P) < 3:
        return cand
    poly = hull2d(P[:, :2])
    if len(poly) < 3:
        return cand
    ks = np.nonzero(cand)[0]
    d, out = outside(poly, C[ks, :2])
    nxy = N[ks, :2]
    unit = nxy / np.maximum(np.linalg.norm(nxy, axis=1), 1e-9)[:, None]
    facing = (unit * out).sum(axis=1)
    rad = C[ks, :2] - poly.mean(axis=0)
    looks_out = (unit * rad).sum(axis=1) > 0.0
    nz = np.clip(np.abs(N[ks, 2]), 0.0, 0.999)
    depth = 0.06 + (b1 - b0) * nz / np.sqrt(1.0 - nz ** 2)                     # (a sloped plate's band reaches in a little)
    on_line = (d <= 0.04) & (d >= -depth) & (facing > 0.3)
    on_plate = np.isin(piece[ks], list(big)) & (d <= 0.04)
    clear = np.array([open_to_view(m, tree, k, C[k], N[k]) for k in ks], dtype=bool)
    keep = looks_out & clear & (on_line | on_plate)
    res = np.zeros(len(cand), dtype=bool)
    res[ks[keep]] = True
    return whole_patches(m, res, b1 - b0)


def whole_patches(m, sel, height, reach=0.45):
    """Of the faces in `sel`, those in a patch (faces sharing a point) that reads as a stripe: at least `reach` across
    and most of the band's height. Drops the odd bits a band would otherwise catch: a visor block, a machine-gun ball,
    the narrow step where two plates meet (a thin line, not a band)."""
    ks = np.nonzero(sel)[0]
    if not len(ks):
        return sel
    parent = {}

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for k in ks:
        fc = m.faces[k]
        for i in fc:
            parent.setdefault(i, i)
        r0 = find(fc[0])
        for i in fc[1:]:
            ri = find(i)
            if ri != r0:
                parent[ri] = r0
    lab = np.array([find(m.faces[k][0]) for k in ks])
    out = sel.copy()
    for pl in set(lab.tolist()):
        mine = ks[lab == pl]
        P = np.array([tuple(m.verts[i]) for k in mine for i in m.faces[k]])
        across = float(np.hypot(*(P[:, :2].max(axis=0) - P[:, :2].min(axis=0))))
        tall = float(P[:, 2].max() - P[:, 2].min())
        if across < reach or tall < 0.5 * height:
            out[mine] = False
    return out


def lookup(xs, f):
    xs = np.asarray(xs, dtype=float)
    return xs, np.array([f(x) for x in xs])


def bits(places, have):
    """The places that have faces, and every face's bits over them."""
    names = [p for p in places if p in have and have[p].any()]
    n = len(next(iter(have.values()))) if have else 0
    out = np.zeros(n, dtype=np.int32)
    for k, p in enumerate(names):
        out[have[p]] |= (1 << k)
    return names, out


def to_file(v):
    return [round(float(v[0]), 3), round(-float(v[1]), 3), round(float(v[2]), 3)]          # (the file's y is to the right)


def mark_of(model):
    """Every face's bits, as long as her faces (a face added after they were found lies in none)."""
    fm = getattr(model, "fmark", None)
    if fm is None:
        return None
    fm = np.asarray(fm, dtype=np.int32)
    if len(fm) < len(model.faces):
        fm = np.concatenate([fm, np.zeros(len(model.faces) - len(fm), dtype=np.int32)])
    return fm


def place_names(model):
    return model.info.get("file", {}).get("unit_marks", {}).get("places", [])


# ---- a badge's place ----------------------------------------------------------------------------------------

def find_disc(m, tree, ok_face, side, prefer, box, r, avoid, step=0.04, facing_min=0.55):
    """The place nearest `prefer` (x, z) inside `box` (x0, x1, z0, z1) on one side (+1 port, -1 starboard, the model's
    y) where a disc of radius r lies on painted skin that `ok_face` allows, flat enough to paint a badge on, and clear of
    everything in `avoid` [(x, z, radius)]. Returns (at, facing) or None."""
    x0, x1, z0, z1 = box
    cands = [(x, z) for x in np.arange(x0, x1 + 1e-6, step) for z in np.arange(z0, z1 + 1e-6, step)]
    cands.sort(key=lambda q: math.hypot(q[0] - prefer[0], 1.5 * (q[1] - prefer[1])))
    for x, z in cands:
        if any(math.hypot(x - ax, z - az) < r + ar + 0.03 for ax, az, ar in avoid):
            continue
        hit = tree.ray_cast(Vector((x, side * 9.0, z)), Vector((0.0, -side, 0.0)), 12.0)
        if hit[0] is None or hit[0].y * side < 0.05 or not ok_face(hit[2]):        # (on her own side: not the inside of
            continue                                                               #  the far wall of an open compartment)
        n = hit[1].normalized()
        if n.y * side < facing_min:
            continue
        up = Vector((0.0, 0.0, 1.0))
        v = (up - n * up.dot(n)).normalized()
        u = v.cross(n)
        good = True
        for rr, k in ((0.5, 6), (1.0, 12)):
            for j in range(k):
                a = 2.0 * math.pi * (j + 0.5 * (rr < 1.0)) / k
                p = hit[0] + (u * math.cos(a) + v * math.sin(a)) * (rr * r)
                h2 = tree.ray_cast(p + n * 0.5, -n, 1.0)
                if h2[0] is None or not ok_face(h2[2]) or h2[1].normalized().dot(n) < 0.86 or abs(h2[3] - 0.5) > 0.22 * r + 0.02:
                    good = False
                    break
            if not good:
                break
        if good:
            return hit[0], n
    return None


def emblem_entries(m, tree, ok_face, prefer, box, r, avoid, place, part, facings=(0.9, 0.55)):
    """A badge's place on each side, shrinking it a little where no place has room; a place that looks straight out
    to the side (`facings`, in turn: how squarely) is taken before one turned away (a badge under a bomber's chin, or on
    a turret's back corner, is seen by nobody)."""
    out = []
    for side, word in ((1.0, "left"), (-1.0, "right")):
        got = None
        for fmin in facings:
            for shrink in (1.0, 0.85, 0.7):
                got = find_disc(m, tree, ok_face, side, prefer, box, r * shrink, avoid, facing_min=fmin)
                if got:
                    break
            if got:
                break
        if got:
            at, n = got
            out.append({"place": place, "side": word, "at": to_file(at), "facing": [round(float(n.x), 3), round(-float(n.y), 3), round(float(n.z), 3)],
                        "radius": round(r * shrink, 3), "part": part})
    return out


# ---- aircraft -------------------------------------------------------------------------------------------------

def plane(m, f, G):
    """Finds the places on an aircraft (planes.convert calls it last, when she is whole)."""
    s, hull, wing = f.spec, f.hull, f.wing
    C, N, A = geometry(m)
    names = list(m.paints)
    base = np.array([nm.split(".")[0] for nm in names])
    fpaint, fpart = np.array(m.fpaint), np.array(m.fpart)
    skin = np.isin(base, AIR_SKIN)[fpaint]
    boss = (base == "boss")[fpaint]
    pnames = np.array([p.name for p in m.parts], dtype=object)[fpart]
    x, y, z = C[:, 0], C[:, 1], C[:, 2]
    ay = np.abs(y)

    # her fuselage, as a box round each ring of it
    xs, Zc = lookup(np.linspace(hull.back, hull.front, 400), lambda t: hull.z(-t))
    _, W = lookup(xs, lambda t: hull.w(-t))
    _, H = lookup(xs, lambda t: hull.h(-t))
    xc = np.clip(x, hull.back, hull.front)
    zc, w, h = np.interp(xc, xs, Zc), np.interp(xc, xs, W), np.interp(xc, xs, H)
    in_body = (x >= hull.back - 0.03) & (x <= hull.front + 0.03) & (ay <= w * 1.03 + 0.025) & (np.abs(z - zc) <= h * 1.03 + 0.025)
    length = hull.front - hull.back

    # her fins and tailplane: the faces that lie on them (their skin, and the lines and rivets on it)
    on_fin = np.zeros(len(x), dtype=bool)
    for fin_sections, side in f.fins:
        fin = G["Wing"](fin_sections, 1.0, True, abs(side))
        us = np.linspace(fin.root, fin.tip, 200)
        zu = np.clip(z, fin.root, fin.tip)
        lead, trail = np.interp(zu, us, [fin.lead(u) for u in us]), np.interp(zu, us, [fin.trail(u) for u in us])
        thick = np.interp(zu, us, [fin.thick(u) for u in us])
        on_fin |= (z >= fin.root - 0.03) & (z <= fin.tip + 0.05) & (x <= lead + 0.06) & (x >= trail - 0.06) & (np.abs(ay - abs(side)) <= 0.4 * thick + 0.045)
    tw = G["Wing"](s["tail"])
    us = np.linspace(tw.root, tw.tip, 200)
    uu = np.clip(ay, tw.root, tw.tip)
    lead, trail = np.interp(uu, us, [tw.lead(u) for u in us]), np.interp(uu, us, [tw.trail(u) for u in us])
    zt, thick = np.interp(uu, us, [tw.z(u) for u in us]), np.interp(uu, us, [tw.thick(u) for u in us])
    on_tp = (ay >= tw.root - 0.03) & (ay <= tw.tip + 0.06) & (x <= lead + 0.06) & (x >= trail - 0.06) & (np.abs(z - zt) <= 0.5 * thick + 0.05)
    movers = np.array([str(p).startswith(("rudder", "elevator")) for p in pnames])
    have = {"tail": skin & ~boss & (movers | ((on_fin | on_tp) & ~in_body)),
            "rudder": skin & np.array([str(p).startswith("rudder") for p in pnames]),
            "spinner": boss.copy()}

    # her engines: the cowling of each, and the front of it
    cowl, nose = np.zeros(len(x), dtype=bool), np.zeros(len(x), dtype=bool)
    hoods = s.get("hoods", [])
    hood_front = max((max(r_[0] for r_ in hd[0]) for hd in hoods), default=None)
    hood_back = min((min(r_[0] for r_ in hd[0]) for hd in hoods), default=None)
    pushers = [(px, pz) for px, pz in s.get("pushers", [])]
    painted = skin & ~boss
    for hub in f.hubs:
        if any(abs(hub.x - px) < 0.02 and abs(hub.z - pz) < 0.02 for px, pz in pushers):
            c = painted & in_body & (x >= hub.x - 0.05) & (x <= hub.x + 1.3)        # (an airscrew that pushes: the tail of her nacelle)
            cowl |= c
            continue
        if abs(hub.y) < 0.05:                                                        # (the engine in her nose)
            radial = s.get("engine") == "radial"
            if radial and s.get("nose_back"):
                back = min(s["nose_back"], hub.x - 0.9)                              # (a long cowl may be painted as her coat: the Fw 190's)
            else:
                back = hood_front + 0.05 if hood_front is not None else hull.front - 0.25 * length
            c = painted & in_body & (x >= back) & (x <= hub.x + 0.05)
            band = 0.40
        else:                                                                        # (an engine out on her wing)
            eng = min(s.get("engines", []), key=lambda e: abs(e[0] - abs(hub.y)), default=None)
            radial = bool(eng[3]) if eng else True
            r = np.hypot(y - hub.y, z - hub.z)
            ring = painted & (x <= hub.x - 0.3) & (x >= hub.x - 0.7) & (r < 1.2)
            R = float(np.percentile(r[ring], 95)) if ring.any() else 0.7
            c = painted & (x >= hub.x - (1.35 if radial else 1.9)) & (x <= hub.x + 0.05) & (r <= R + 0.05)
            band = 0.30
        if c.any():
            cowl |= c
            nose |= c & (x >= x[c].max() - band)
    for pod, out in f.pods:                                                          # (a jet: the intake of each pod)
        py = -pod.side
        xs_, Wp = lookup(np.linspace(pod.back, pod.front, 120), lambda t: max(pod.w(-t), pod.h(-t)))
        _, Zp = lookup(xs_, lambda t: pod.z(-t))
        xp = np.clip(x, pod.back, pod.front)
        r = np.hypot(y - py, z - np.interp(xp, xs_, Zp))
        c = painted & (x >= pod.front - 1.0) & (x <= pod.front + 0.05) & (r <= np.interp(xp, xs_, Wp) * 1.06 + 0.03)
        cowl |= c
    if not f.hubs or all(abs(hb.y) > 0.05 for hb in f.hubs):
        nose |= painted & in_body & (x >= hull.front - min(max(0.6, 0.08 * length), 1.2))   # (no engine in her nose: the nose itself)
    have["cowling"], have["nose"] = cowl, nose

    # her wingtips
    tip_band = min(max(0.08 * f.half, 0.4), 1.0)
    wings = np.array([str(p).startswith(("wing.", "aileron.")) for p in pnames])
    have["wingtips"] = painted & wings & (ay >= f.half - tip_band)

    # a band round her rear fuselage: between her wing and her tailplane, clear of her national marking
    a, b = wing.trail(wing.root), tw.lead(tw.root)
    if hood_back is not None:
        a = min(a, hood_back)
    wb = min(max(0.055 * length, 0.30), 0.75)
    sides = [(mk["at"][0], mk["radius"]) for mk in (m.marks or []) if mk.get("place") == "side"]
    codes = [(mk["at"][0], mk["radius"]) for mk in (m.marks or []) if mk.get("place") == "code"]
    pref, best = b + 0.3 * (a - b), None
    for xb in np.arange(b + wb / 2 + 0.1, a - wb / 2 - 0.15, 0.02):
        if any(abs(xb - mx) < wb / 2 + r_ + 0.04 for mx, r_ in sides):
            continue
        cost = abs(xb - pref) + sum(2.0 for mx, r_ in codes if abs(xb - mx) < wb / 2 + r_ + 0.02)
        if best is None or cost < best[0]:
            best = (cost, xb)
    body_parts = np.array([str(p) in ("plane", "tail") for p in pnames])
    have["fuselage band"] = (painted & in_body & body_parts & (np.abs(x - best[1]) <= wb / 2)) if best else np.zeros(len(x), dtype=bool)

    places, fmark = bits(AIR_PLACES, have)
    m.fmark = fmark

    # her badge, on both sides of her nose
    tree = BVHTree.FromPolygons([tuple(v) for v in m.verts], [tuple(fc) for fc in m.faces])
    plane_part = m.part_at["plane"]
    ok_face = lambda k: bool(painted[k]) and m.fpart[k] == plane_part
    r = min(max(0.028 * length, 0.18), 0.42)
    avoid = [(mk["at"][0], mk["at"][2], mk["radius"]) for mk in (m.marks or []) if mk.get("place") in ("side", "code")]
    for lst in ("kills", "missions", "lead"):
        slot = (getattr(m, "tally", None) or {}).get(lst)
        if slot:
            avoid += [(q[0], q[2], slot.get("radius", 0.05)) for q in slot["slots"]]
    nose_hub = next((hb for hb in f.hubs if abs(hb.y) < 0.05 and not any(abs(hb.x - px) < 0.02 for px, _ in pushers)), None)
    if nose_hub is not None and cowl.any():
        cx = x[cowl & in_body]
        x0, x1 = (cx.min(), cx.max()) if len(cx) else (hull.front - 0.3 * length, hull.front)
        px_ = 0.5 * (x0 + x1)
        box = (x0 - 0.5, x1 - r, float(np.interp(px_, xs, Zc) - np.interp(px_, xs, H)), float(np.interp(px_, xs, Zc) + np.interp(px_, xs, H)))
        prefer = (px_, float(np.interp(px_, xs, Zc) + 0.1 * np.interp(px_, xs, H)))
    else:
        px_ = hull.front - 0.18 * length
        box = (hull.front - 0.4 * length, hull.front - 0.04 * length, float(np.interp(px_, xs, Zc) - np.interp(px_, xs, H)), float(np.interp(px_, xs, Zc) + 0.7 * np.interp(px_, xs, H)))
        prefer = (px_, float(np.interp(px_, xs, Zc) - 0.1 * np.interp(px_, xs, H)))
    emblems = emblem_entries(m, tree, ok_face, prefer, box, r, avoid, "nose", "plane")
    m.info.setdefault("file", {})["unit_marks"] = {"how": HOW, "places": places, "emblems": emblems}
    return places


# ---- tanks and armoured cars ------------------------------------------------------------------------------------

def vehicle(m):
    """Finds the places on a tank, an assault gun, a tank hunter or an armoured car (vehicles.build calls it last).
    The two bands are cut into her plates along level lines, so that each is a clean stripe of even height."""
    if m.kind not in ARMOUR:
        return []

    def looks():
        C, N, A = geometry(m)
        names = list(m.paints)
        base = np.array([nm.split(".")[0] for nm in names])
        coat = np.array(m.fcoat, dtype=bool) if len(m.fcoat) == len(m.faces) else np.zeros(len(m.faces), dtype=bool)
        skin = np.isin(base, LAND_SKIN)[np.array(m.fpaint)] | coat
        pnames = np.array([p.name for p in m.parts], dtype=object)[np.array(m.fpart)]
        return C, N, A, skin, pnames

    C, N, A, skin, pnames = looks()
    z, nz = C[:, 2], N[:, 2]
    wall = skin & (np.abs(nz) < 0.6)

    # a band round each turret, at the middle of its walls
    turrets, cuts = {}, []
    for tn in [p.name for p in m.parts if p.name in ("turret", "turret_cast", "turret_welded", "turret_fore")]:
        on = pnames == tn
        tw_ = wall & on
        if A[tw_].sum() < 0.3:
            continue
        az = np.degrees(np.arctan2(N[tw_, 1], N[tw_, 0]))
        sectors = {int(((a_ + 360.0) % 360.0) // 45) for a_, ar in zip(az, A[tw_]) if ar > 0.0005}
        roof = skin & on & (nz > 0.8)
        top = float(np.percentile(z[roof], 90)) + 0.02 if roof.any() else float(z[tw_].max())
        tw_ = tw_ & (z <= top)
        if len(sectors) < 6 or not tw_.any():
            continue
        zs, ws = z[tw_], A[tw_]
        order = np.argsort(zs)
        cum = np.cumsum(ws[order]) / ws.sum()
        zlo, zhi = float(zs[order][np.searchsorted(cum, 0.06)]), float(zs[order][min(np.searchsorted(cum, 0.94), len(zs) - 1)])
        if zhi - zlo < 0.22:
            continue
        hb = min(max(0.3 * (zhi - zlo), 0.09), 0.18)
        mid = zlo + 0.5 * (zhi - zlo)
        turrets[tn] = (zlo, zhi, mid, mid - hb / 2, mid + hb / 2)
        ids = np.nonzero(skin & (on | (pnames == "skirt_turret")) & (np.abs(nz) < 0.8))[0]
        cuts.append((ids, (mid - hb / 2, mid + hb / 2)))

    # a band round the hull, a hand's breadth under the top of her sides, at one height all the way round
    hullish = np.array([str(p) == "hull" or (str(p).startswith("skirt_") and str(p) not in ("skirt_turret", "skirt_rail_l", "skirt_rail_r")) for p in pnames])
    hs = skin & hullish
    hband = None
    if hs.any():
        x = C[:, 0]
        x0, x1 = float(x[hs].min()), float(x[hs].max())
        step = 0.1
        nb = int((x1 - x0) / step) + 1
        tops = np.full(nb, -9.0)
        np.maximum.at(tops, ((x[hs] - x0) / step).astype(int), z[hs])
        middle = tops[int(0.25 * nb):max(int(0.75 * nb), int(0.25 * nb) + 1)]
        top = float(np.median(middle[middle > -9.0])) if (middle > -9.0).any() else float(z[hs].max())
        hb = 0.16 if m.kind == "tank" else 0.12
        hband = (top - 0.08 - hb, top - 0.08)
        ids = np.nonzero(hs & (np.abs(nz) < 0.8))[0]
        cuts.append((ids, hband))
    for ids, levels in cuts:
        ids = [int(i) for i in ids]
        for level in levels:                             # (the piece above the first cut is cut again at the second)
            n0 = len(m.faces)
            cut_z(m, ids, level)
            ids += list(range(n0, len(m.faces)))
    if cuts:
        C, N, A, skin, pnames = looks()
        z, nz = C[:, 2], N[:, 2]
        hullish = np.array([str(p) == "hull" or (str(p).startswith("skirt_") and str(p) not in ("skirt_turret", "skirt_rail_l", "skirt_rail_r")) for p in pnames])
        hs = skin & hullish
    x = C[:, 0]
    piece = pieces(m)
    tree = BVHTree.FromPolygons([tuple(v) for v in m.verts], [tuple(fc) for fc in m.faces])
    have = {}
    ring = np.zeros(len(z), dtype=bool)
    for tn, (zlo, zhi, mid, b0, b1) in turrets.items():
        own = skin & (np.abs(nz) < 0.8) & ((pnames == tn) | (pnames == "skirt_turret"))
        ring |= on_outside(m, C, N, A, own & (z >= b0) & (z <= b1), own, b0, b1, piece, tree)
    have["turret ring"] = ring
    if hband:
        lvl = (z >= hband[0]) & (z <= hband[1])
        have["hull band"] = on_outside(m, C, N, A, hs & (np.abs(nz) < 0.7) & lvl, hs & (np.abs(nz) < 0.8), hband[0], hband[1], piece, tree)
    else:
        have["hull band"] = np.zeros(len(z), dtype=bool)
    have["hatches"] = skin & np.array([str(p).startswith("hatch") for p in pnames])

    places, fmark = bits(LAND_PLACES, have)
    m.fmark = fmark
    info = {"how": HOW, "places": places, "emblems": []}

    # the pennant on her aerial
    if "aerial" in m.part_at:
        ai = m.part_at["aerial"]
        pts = [m.verts[i] for k, fc in enumerate(m.faces) if m.fpart[k] == ai for i in fc]
        if pts:
            topv = max(pts, key=lambda p: p.z)
            m.socket("pennant", (topv.x, topv.y, topv.z - 0.04), (-1.0, 0.0, 0.0), part="aerial", note="a unit's pennant in its colour (unit_marks: aerial pennant)")
            info["pennant"] = "pennant"
            info["places"] = places + ["aerial pennant"]

    # her badge: on each side of the turret, or of the hull where she has none (or where the turret has no room, or no
    # side that looks squarely out: a place facing straight out on the hull is taken before a turret corner turned away)
    r = 0.17 if m.kind == "tank" else 0.14
    first = "turret" if "turret" in turrets else next(iter(turrets), None)
    for fmin, on_part in [(fm_, p_) for fm_ in (0.9, 0.75) for p_ in ([first] if first else []) + [None]]:
        on = on_part or "hull"
        avoid = [(mk["at"][0], mk["at"][2], mk["radius"]) for mk in (m.marks or []) if mk.get("part", "hull") == on]
        for pl in m.info.get("file", {}).get("name_place", {}).get("places", []):
            if pl.get("part") == on:
                for t in (-0.5, -0.25, 0.0, 0.25, 0.5):          # (her crew's name keeps its own room: a row of discs along it)
                    avoid.append((pl["at"][0] + pl["along"][0] * pl["long"] * t, pl["at"][2], 0.12 * pl.get("lines", 1) + 0.04))
        if on_part:
            pi = m.part_at[on_part]
            zlo, zhi, mid = turrets[on_part][:3]
            tx = x[pnames == on_part]
            box = (float(tx.min()) + r, float(tx.max()) - r, zlo + 0.03, zhi - 0.03)
            prefer = (float(m.parts[pi].pivot.x) - 0.1, mid + 0.04)
        else:
            pi = m.part_at["hull"]
            hx = x[hs] if hs.any() else x
            tz = float(np.percentile(z[hs], 97)) if hs.any() else 1.5
            box = (float(hx.min()) + 0.4, float(hx.max()) - 0.3, tz - 0.7, tz - 0.08)
            prefer = (float(hx.max()) - 0.35 * (float(hx.max()) - float(hx.min())), tz - 0.3)
        ok_face = (lambda pi_: (lambda k: bool(skin[k]) and m.fpart[k] == pi_))(pi)
        info["emblems"] = emblem_entries(m, tree, ok_face, prefer, box, r, avoid, "turret" if on_part else "hull", on, facings=(fmin,))
        if len(info["emblems"]) == 2:
            break
    m.info.setdefault("file", {})["unit_marks"] = info
    return info["places"]


# ---- the pictures -------------------------------------------------------------------------------------------

def paint(m, wear):
    """For the pictures only: paints the places named in `wear` ({place: hex}) on the model before it is drawn, in a
    paint of their own (`<paint>.unit`, so it keeps its roughness), as the game will."""
    places = place_names(m)
    fm = mark_of(m)
    if fm is None:
        return 0
    if m.weather is not None and not getattr(m, "_unit_weather", False):
        own = m.weather                                  # (the paint wears and soots as the coat under it would)
        m.weather = lambda v, n, paint_, part: own(v, n, "upper" if paint_.startswith("unit.") else paint_, part)
        m._unit_weather = True
    done = 0
    for place, hexc in wear.items():
        if place not in places:
            continue
        bit = 1 << places.index(place)
        paint_name = "unit." + place.replace(" ", "_")
        m.paints[paint_name] = hexc
        m.paint_at[paint_name] = list(m.paints).index(paint_name)
        for k in np.nonzero(fm & bit)[0]:
            m.fpaint[k] = m.paint_at[paint_name]
            if k < len(m.fcoat):
                m.fcoat[k] = False
            done += 1
    return done


def badges(model, ob, arm, colour, places=None):
    """For the pictures only: a plain round badge in `colour` at every emblem place (the game draws the unit's own
    emblem there). Returns the object made, riding her parts."""
    import bpy
    info = model.info.get("file", {}).get("unit_marks", {})
    ems = [e for e in info.get("emblems", []) if places is None or e["place"] in places]
    if not ems:
        return []
    tree = BVHTree.FromPolygons([tuple(v) for v in model.verts], [tuple(f) for f in model.faces])
    verts, faces, cols, groups = [], [], [], []
    for e in ems:
        at = Vector((e["at"][0], -e["at"][1], e["at"][2]))
        n = Vector((e["facing"][0], -e["facing"][1], e["facing"][2])).normalized()
        up = Vector((0, 0, 1))
        v = (up - n * up.dot(n)).normalized()
        u = v.cross(n)
        r = e["radius"]
        shapes = [(0.0, 1.0, colour), (0.78, 0.88, "1D1D1D"), (None, None, "1D1D1D")]
        for layer, (r0, r1, col) in enumerate(shapes):
            if r0 is None:                               # (a plain chevron inside: our own simple device)
                polys = [[(-0.5, 0.05), (0.0, 0.45), (0.5, 0.05), (0.5, -0.2), (0.0, 0.2), (-0.5, -0.2)]]
            else:
                seg = 40
                polys = []
                for k in range(seg):
                    t0, t1 = 2 * math.pi * k / seg, 2 * math.pi * (k + 1) / seg
                    q = [(r1 * math.cos(t0), r1 * math.sin(t0)), (r1 * math.cos(t1), r1 * math.sin(t1))]
                    q += [(r0 * math.cos(t1), r0 * math.sin(t1)), (r0 * math.cos(t0), r0 * math.sin(t0))] if r0 > 0 else [(0.0, 0.0)]
                    polys.append(q)
            for poly in polys:
                from mathutils.geometry import tessellate_polygon
                flat = [Vector((a_, b_, 0.0)) for a_, b_ in poly]
                for tri in tessellate_polygon([flat]):
                    idx = []
                    for i in tri:
                        q = flat[i]
                        p = at + (u * q.x + v * q.y) * r
                        hit = tree.ray_cast(p + n * 0.4, -n, 0.8)
                        if hit[0] is not None:
                            p = hit[0]
                        idx.append(len(verts))
                        verts.append(p + n * (0.006 + 0.002 * layer))
                        groups.append(e["part"])
                    a_, b_, c_ = verts[idx[0]], verts[idx[1]], verts[idx[2]]
                    if (b_ - a_).cross(c_ - a_).dot(n) < 0:
                        idx.reverse()
                    faces.append(idx)
                    cols.append(col)
    me = bpy.data.meshes.new(model.name + "_badges")
    me.from_pydata([tuple(p) for p in verts], [], faces)
    used = sorted(set(cols))
    for c in used:
        mat = bpy.data.materials.get("badge:" + c) or bpy.data.materials.new("badge:" + c)
        mat.use_nodes = True
        bs = mat.node_tree.nodes["Principled BSDF"]
        bs.inputs["Base Color"].default_value = (*[(int(c[i:i + 2], 16) / 255.0) ** 2.2 for i in (0, 2, 4)], 1.0)
        bs.inputs["Roughness"].default_value = 0.85
        me.materials.append(mat)
    me.polygons.foreach_set("material_index", [used.index(c) for c in cols])
    me.update()
    o = bpy.data.objects.new(model.name + "_badges", me)
    bpy.context.scene.collection.objects.link(o)
    o.visible_shadow = False
    for part in sorted(set(groups)):
        g = o.vertex_groups.new(name=part)
        g.add([i for i, p in enumerate(groups) if p == part], 1.0, "REPLACE")
    o.parent = arm
    mod = o.modifiers.new("rig", "ARMATURE")
    mod.object = arm
    return [o]
