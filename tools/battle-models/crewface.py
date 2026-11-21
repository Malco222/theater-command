"""An airman's head as it is seen close through glass: a real face under a leather flying helmet (Brian, 2026-11-21:
"can we make their faces more realistic?", "maybe use the current soldier faces as a template?").

The face is the battle soldier's (crew_heads.json, from make_crew_heads.py: MakeHuman's CC0 man in eight faces an army,
the same faces the soldiers wear), chosen for each man by his machine and his seat, from his own country's soldiers (a
Japanese crew's from the Japanese, a German's from the Germans and Poles, a Soviet's from the Russians, an Italian's
from the Italians; an American's, a Briton's or a nameless machine's from all four European armies). Over it:
- his helmet, made from his own head: everything but his face, pushed out to a smooth leather shell that hugs his skull
  and covers his ears, with its edge turned in to his skin round an oval from above his brows to under his chin;
- earphone cups over his ears;
- his goggles, up on his helmet's brow or down over his eyes, with their strap round his head;
- his oxygen mask, where he wears one: a rubber cup of its own over his nose, mouth and chin, snug to his face at its
  edge and rounded over them, with its valve and hose;
- his brows, as two short dark strokes where his goggles do not cover them.
Only at the near detail (`m.fine`); far off he is the plain shapes he always was."""
import json
import math
import os
import zlib

from mathutils import Vector

_DATA = {}
POOLS = {"JAP": ("JAP",), "GER": ("GER", "POL"), "SOV": ("SOV",), "ITA": ("ITA",)}
EUROPE = ("POL", "GER", "SOV", "ITA")
SCALE = 0.95                                             # (his head, a little smaller than the soldier's under his steel helmet)


def heads():
    if not _DATA:
        _DATA.update(json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "crew_heads.json")))["heads"])
    return _DATA


def pick(nation, plane, seat):
    """Which army's head, and which of its faces, for the man in `seat` of `plane` flown by `nation`: the same man
    every time she is built."""
    pool = POOLS.get(nation, EUROPE)
    h = zlib.crc32(f"{plane}:{seat}".encode())
    return pool[h % len(pool)], (h // len(pool)) % 8


class Fit:
    """One face of one army fitted out: its skin, its helmet shell, its mask shell, its landmarks (head-local)."""
    _made = {}

    @classmethod
    def of(cls, army, k):
        if (army, k) not in cls._made:
            cls._made[army, k] = cls(army, k)
        return cls._made[army, k]

    def __init__(self, army, k):
        h = heads()[army]
        pts = [Vector(p) for p in h["points"]]
        for i, mv in h["faces"][k]["move"].items():
            pts[int(i)] += Vector(mv)
        self.pts, self.tris, self.eye = pts, [tuple(t) for t in h["triangles"]], h["eye"]
        n = len(pts)
        nb = [set() for _ in range(n)]
        for a, b, c in self.tris:
            nb[a] |= {b, c}
            nb[b] |= {a, c}
            nb[c] |= {a, b}
        self.nb = [sorted(s) for s in nb]
        nrm = [Vector() for _ in range(n)]
        for a, b, c in self.tris:
            f = (pts[b] - pts[a]).cross(pts[c] - pts[a])
            for i in (a, b, c):
                nrm[i] += f
        self.nrm = [v.normalized() if v.length > 1e-12 else Vector((1, 0, 0)) for v in nrm]
        eyes = sorted({i for t, e in zip(self.tris, self.eye) if e for i in t})
        self.z_eye = sum(pts[i].z for i in eyes) / len(eyes)
        self.x_eye = sum(pts[i].x for i in eyes) / len(eyes)
        self.eyes = [Vector((sum(pts[i].x for i in eyes if pts[i].y * s > 0) / max(1, sum(1 for i in eyes if pts[i].y * s > 0)),
                             sum(pts[i].y for i in eyes if pts[i].y * s > 0) / max(1, sum(1 for i in eyes if pts[i].y * s > 0)),
                             self.z_eye)) for s in (1.0, -1.0)]       # (his left eye, then his right)
        front = [p for p in pts if p.x > self.x_eye - 0.02 and abs(p.y) < 0.02]
        self.z_chin = min(p.z for p in front)
        self.z_top = max(p.z for p in pts)
        # (the face his helmet leaves open: an oval seen from ahead, from 3 cm over his eyes to under his chin)
        top, bot = self.z_eye + 0.03, self.z_chin - 0.012
        zc, az, ay = 0.5 * (top + bot), 0.5 * (top - bot), 0.07
        zb = self.z_chin + 0.022                        # (and his neck, under its edge at his nape)
        self.g_open = [min((p.y / ay) ** 2 + ((p.z - zc) / az) ** 2 + max(0.0, (0.02 - p.x) / 0.02) ** 2, 1.0 + (p.z - zb) / 0.01) for p in pts]
        self.open = [g < 1.0 for g in self.g_open]
        self.shell = self.pushed(Vector((0.012, 0.0, 0.03)), 0.0042, 0.0058, 40)
        # (his mask: nose, mouth and chin, from under his eyes, cupped over them, seen from a little below ahead)
        self.mask = self.cupped()
        # (where he sits in the old helmet's room: the middle of his helmet fore and aft where its ball's was, its crown
        #  at the ball's top, whichever army's head he has)
        cov = [q for q, o in zip(self.shell, self.open) if not o]
        self.shift = Vector((0.0085 - SCALE * 0.5 * (min(q.x for q in cov) + max(q.x for q in cov)), 0.0,
                             0.117 - SCALE * max(q.z for q in cov)))

    def pushed(self, c, gap, thick, rounds):
        """His head's points carried out from `c` to a smooth shell over them, never nearer them than `gap`, and
        `thick` beyond that."""
        pts = self.pts
        r0 = [(p - c).length for p in pts]
        r = list(r0)
        for _ in range(rounds):                          # (smoothed over his head, so that it is a shell, not a cast of him)
            r = [0.5 * r[i] + 0.5 * sum(r[j] for j in self.nb[i]) / max(1, len(self.nb[i])) for i in range(len(pts))]
            r = [max(a, b + gap) for a, b in zip(r, r0)]
        return [c + (p - c).normalized() * (ri + thick) for p, ri in zip(pts, r)]

    def cupped(self, gap=0.005, thick=0.003, blow=0.0009, rings=6, spokes=24):
        """His oxygen mask: a rubber cup of its own over his nose, mouth and chin, an oval seen from `way` (a little
        below ahead), not cast from his face. Each of its points looks along `way` for the front of his face; its edge
        lies on his face, and the rest is drawn over his nose and chin like cloth over a frame, never nearer him than
        `gap`, and blown out a little (`blow`) so that it rounds like rubber, not cling film; `thick` more beyond that.
        Returns its points, its faces, the quads that turn its edge in to his skin, where its valve is, and g (< 1
        where his skin is under it)."""
        pts, tris = self.pts, self.tris
        way = Vector((1.0, 0.0, -0.35)).normalized()
        U = Vector((0.0, 1.0, 0.0))
        V = way.cross(U)                                 # (up his face, and a little ahead)
        b_top = V.dot(Vector((self.x_eye + 0.012, 0.0, self.z_eye - 0.012)))
        chin = min((p for p in pts if p.x > self.x_eye - 0.02 and abs(p.y) < 0.02), key=lambda p: p.z)
        b_bot = V.dot(chin) - 0.006
        bc, rb, ra = 0.5 * (b_top + b_bot), 0.5 * (b_top - b_bot), 0.043
        flat = [(p.dot(U), p.dot(V), p.dot(way)) for p in pts]
        near = [t for t in tris if all(abs(flat[i][0]) < ra * 1.2 and abs(flat[i][1] - bc) < rb * 1.2 for i in t)
                and pts[t[0]].x > self.x_eye - 0.05]

        def front(a, b):                                 # (the front of his face there, along `way`)
            best = None
            for t in near:
                (a0, b0, h0), (a1, b1, h1), (a2, b2, h2) = (flat[i] for i in t)
                d = (b1 - b2) * (a0 - a2) + (a2 - a1) * (b0 - b2)
                if abs(d) < 1e-12:
                    continue
                l0 = ((b1 - b2) * (a - a2) + (a2 - a1) * (b - b2)) / d
                l1 = ((b2 - b0) * (a - a2) + (a0 - a2) * (b - b2)) / d
                l2 = 1.0 - l0 - l1
                if min(l0, l1, l2) >= -1e-6:
                    h = l0 * h0 + l1 * h1 + l2 * h2
                    best = h if best is None else max(best, h)
            return best

        grid, face = [], []
        for r in range(rings + 1):
            for k in range(spokes if r else 1):
                th = 2.0 * math.pi * k / spokes
                a, b = ra * (r / rings) * math.cos(th), bc + rb * (r / rings) * math.sin(th)
                grid.append((a, b, r))
                face.append(front(a, b))
        n = len(grid)

        def at(r, k):
            return 0 if r == 0 else 1 + (r - 1) * spokes + k % spokes

        nb = [[] for _ in range(n)]
        for r in range(1, rings + 1):
            for k in range(spokes):
                i = at(r, k)
                nb[i] += [at(r, k - 1), at(r, k + 1), at(r - 1, k)] + ([at(r + 1, k)] if r < rings else [])
                if r == 1:
                    nb[0].append(i)
        for i in range(n):                               # (where his face was not found: as its neighbours, a little back)
            if face[i] is None:
                face[i] = min((face[j] for j in nb[i] if face[j] is not None), default=0.06) - 0.004
        low = [f + (0.0025 if g[2] == rings else gap) for f, g in zip(face, grid)]
        hgt = list(low)
        for _ in range(400):
            hgt = [hgt[i] if grid[i][2] == rings else
                   max(low[i], sum(hgt[j] for j in nb[i]) / len(nb[i]) + blow * (1.0 - (grid[i][2] / rings) ** 2))
                   for i in range(n)]
        out = [U * a + V * b + way * (h + (thick if r < rings else thick * 0.5)) for (a, b, r), h in zip(grid, hgt)]
        faces = []
        for r in range(1, rings + 1):
            for k in range(spokes):
                if r == 1:
                    faces.append((0, at(1, k + 1), at(1, k)))
                else:
                    faces.append((at(r - 1, k), at(r - 1, k + 1), at(r, k + 1), at(r, k)))
        rim = []
        for k in range(spokes):                          # (its edge turned in to his skin)
            i, j = at(rings, k), at(rings, k + 1)
            si, sj = (U * grid[x][0] + V * grid[x][1] + way * (face[x] - 0.002) for x in (i, j))
            rim.append((out[i], out[j], sj, si))
        valve = U * 0.0 + V * (bc - 0.35 * rb)
        hv = max(hgt[i] for i in range(n) if abs(grid[i][0]) < 0.012 and abs(grid[i][1] - (bc - 0.35 * rb)) < 0.012)
        valve = valve + way * (hv + thick)
        g = [((a / ra) ** 2 + ((b - bc) / rb) ** 2) if p.x > self.x_eye - 0.035 else 9.0 for (a, b, h), p in zip(flat, pts)]
        return {"points": out, "faces": faces, "rim": rim, "valve": valve, "way": way, "g": g}

    def cut(self, g, keep_low, where, lift):
        """His head cut cleanly along g = 1 (between points, not along triangles' edges): the part where g < 1 if
        `keep_low`, else where g >= 1. Returns (skin points, skin polygons) and (shell points `where`, shell polygons) of
        that part, both sharing the cut's points, the quads that turn the shell's cut edge in to his skin, and the
        triangle of his head each polygon was cut from."""
        pts, nrm = self.pts, self.nrm
        sp, wp, polys, rims, src = [], [], [], [], []
        at = {}

        def point(key):
            if key not in at:
                if isinstance(key, int):
                    a = b = key
                    t = 0.0
                else:
                    a, b = key
                    t = (1.0 - g[a]) / (g[b] - g[a])
                at[key] = len(sp)
                sp.append(pts[a].lerp(pts[b], t))
                wp.append(where[a].lerp(where[b], t))
                n = nrm[a].lerp(nrm[b], t)
                sp[-1] = sp[-1] + n.normalized() * lift if not isinstance(key, int) else sp[-1]
            return at[key]

        def mine(i):
            return (g[i] < 1.0) == keep_low

        for ti, t in enumerate(self.tris):
            ins = [mine(i) for i in t]
            if not any(ins):
                continue
            src.append(ti)
            if all(ins):
                polys.append(tuple(point(i) for i in t))
                continue
            poly, cross = [], []
            for j in range(3):
                a, b = t[j], t[(j + 1) % 3]
                if mine(a):
                    poly.append(point(a))
                if mine(a) != mine(b):
                    k = point((a, b) if a < b else (b, a))
                    poly.append(k)
                    cross.append(k)
            polys.append(tuple(poly))
            if len(cross) == 2:
                inner = sum((pts[i] for i, s_ in zip(t, ins) if s_), Vector()) / sum(ins)
                rims.append((cross, inner))
        quads = []
        for (k1, k2), inner in rims:                     # (its cut edge turned in to his skin: facing away from what it covers)
            q = (wp[k1], wp[k2], sp[k2], sp[k1])
            away = 0.5 * (sp[k1] + sp[k2]) - inner
            if (q[1] - q[0]).cross(q[3] - q[0]).dot(away) < 0.0:
                q = (q[1], q[0], q[3], q[2])
            quads.append(q)
        return sp, wp, polys, quads, src


def head(m, at, nation, plane, seat, goggles="up", mask=True):
    """His head, helmet, goggles and mask, placed by `at` (the figure's own: ahead, to his left, above, from the middle
    of his old helmet). Returns where his mask's hose leaves it (or None)."""
    army, k = pick(nation, plane, seat)
    f = Fit.of(army, k)

    def to(p):
        return at(*(Vector(p) * SCALE + f.shift))

    pts = f.pts
    covered = [not o for o in f.open]
    # his face: skin, and his eyes dark, cut cleanly round his helmet's edge; and his helmet over the rest
    sp, wp, polys, quads, src = f.cut(f.g_open, True, f.shell, 0.0)
    V = [to(p) for p in sp]
    hid = [mask and all(f.mask['g'][i] < 1.0 for i in t) for t in f.tris]     # (under his mask: not drawn)
    skin_f = [q for q, ti in zip(polys, src) if not f.eye[ti] and not hid[ti]]
    eye_f = [q for q, ti in zip(polys, src) if f.eye[ti]]
    m.add(V, skin_f, "skin", smooth=True)
    if eye_f:
        m.add(V, eye_f, "boot", smooth=True)
    sp, wp, polys, quads, src = f.cut(f.g_open, False, f.shell, 0.0012)
    m.add([to(p) for p in wp], polys, "leather", smooth=True)
    for q in quads:
        m.add([to(p) for p in q], [(0, 1, 2, 3)], "leather", smooth=False)
    shell = [q for q, c in zip(f.shell, covered) if c]
    # his earphones: a round cup of leather over each ear
    for s in (1.0, -1.0):
        e = max(shell, key=lambda q: s * q.y)
        m.ball(to(Vector((e.x - 0.004, e.y + s * 0.002, e.z - 0.004))), (0.034, 0.011, 0.04), "leather", 10, 4)
    # his goggles
    if goggles:
        if goggles == "down":
            mids = [Vector((e.x + 0.026, e.y * 1.05, e.z + 0.002)) for e in f.eyes]
            look = Vector((1.0, 0.0, 0.0))
        else:                                            # (up on his helmet's brow, looking up and ahead)
            zg = f.z_eye + 0.058
            mids = []
            for e in f.eyes:
                near = min((q for q in shell if q.x > 0.0), key=lambda q: (q.y - e.y) ** 2 + (q.z - zg) ** 2)
                mids.append(Vector((near.x + 0.012, e.y, zg)))
            look = Vector((0.75, 0.0, 0.66)).normalized()
        for c in mids:
            m.rod(to(c - look * 0.014), to(c + look * 0.006), 0.025, "goggles", 0.023, sides=14)
            m.disc(to(c + look * 0.0065), at(*look) - at(0, 0, 0), 0.019, "lens", sides=14)
        m.rod(to(mids[0] + Vector((0.0, -0.014, 0.0))), to(mids[1] + Vector((0.0, 0.014, 0.0))), 0.005, "goggles", sides=6)   # (the bridge)
        zs = mids[0].z - (0.006 if goggles == "up" else 0.0)
        band = [Vector((q.x, q.y, 0.0)) for q in shell if abs(q.z - zs) < 0.01]
        ring, faces_ = [], []
        for a in range(40, 325, 10):                     # (the strap round his helmet at their height, lying on it)
            th = math.radians(a)
            way = Vector((math.cos(th), math.sin(th), 0.0))
            near = sorted(band, key=lambda q: -q.normalized().dot(way))[:3]
            if near:
                r = sum(q.length for q in near) / len(near)
                ring.append(to(Vector((0.0, 0.0, zs)) + way * (r + 0.002)))
                faces_.append(at(*way) - at(0, 0, 0))
        side = [i for i, p in enumerate(ring)]
        if len(ring) >= 3:
            m.strip(ring, faces_, 0.0075, "goggles", lift=0.0)
    else:
        # his brows, dark over his eyes
        for e in f.eyes:
            s = 1.0 if e.y > 0 else -1.0
            line = []
            for t in (0.0, 0.25, 0.5, 0.75, 1.0):
                y, z = e.y - s * 0.012 + s * 0.032 * t, e.z + 0.014 + 0.005 * math.sin(math.pi * min(1.0, t * 1.3))
                p = max((q for q in range(len(pts)) if f.open[q] and abs(pts[q].y - y) < 0.006 and abs(pts[q].z - z) < 0.006), key=lambda q: pts[q].x, default=None)
                if p is not None:
                    line.append((to(pts[p] + f.nrm[p] * 0.0012), at(*f.nrm[p]) - at(0, 0, 0)))
            if len(line) >= 3:
                m.strip([q for q, _ in line], [n for _, n in line], 0.0035, "leather", lift=0.0005)
    hose = None
    if mask:
        mk = f.mask
        m.add([to(p) for p in mk["points"]], mk["faces"], "inside", smooth=True)
        for q in mk["rim"]:
            m.add([to(p) for p in q], [(0, 1, 2, 3)], "inside", smooth=False)
        v, w = mk["valve"], mk["way"]
        m.rod(to(v - w * 0.004), to(v + w * 0.007), 0.0105, "goggles", 0.0095, sides=10)       # (its valve, where the hose meets it)
        hose = to(v + w * 0.002 - Vector((0.0, 0.0, 0.008)))
    return hose
