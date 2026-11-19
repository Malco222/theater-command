"""Akagi: the Imperial Navy's great carrier, as she was at Pearl Harbor (December 1941) and Midway (June 1942).

Our own model, made from numbers: her published dimensions after the 1935-38 rebuild at Sasebo (260.7 m overall, 31.3
m over her bulges, 8.7 m draught; a single flight deck of 249.2 m by 30.5 over two hangars, level amidships and falling
0.5 degrees to the bow and 1.5 to the stern; three lifts, 16.0 by 11.8 m forward and offset to starboard, 11.8 by 13.0
amidships, 12.8 by 8.4 aft), and the layout of the ship as photographs of her in 1941-42 show it: the small island on
her PORT side amidships, four storeys and a rangefinder over the deck with its tripod mast, the big funnel turned down
30-odd degrees over her starboard side abreast it (published: "angled 30 degrees below horizontal with its mouth facing
the sea") and the small upright one abaft it, the forward end of her flight deck a little narrower than the rest and
squared off over the open forecastle, the stern open in tall bays under the deck. Her guns: six 20 cm/50 in casemates
low in her after hull (`sec.P1-3`, `sec.S1-3`), each in a port let into her plating; six twin 12 cm/45 high-angle
mountings of the 10th Year Type open on sponsons below the flight deck's edge, all of them grouped amidships abaft the
island and the funnels (published: her AA was "grouped amidships and placed relatively low on the hull", and "the
island blocked the forward arcs of the port battery") (`sec.P4-6`, `sec.S4-6`); fourteen twin 25 mm (`aa25.1-14`),
every one manned. Nothing is copied from any drawing, plan or model; where the photographs could not settle a place
(the lifts' and the light guns' stations along her) it is our reckoning from them.
"""

import math
import random

from mathutils import Matrix, Vector
from mathutils.geometry import tessellate_polygon

import capitals as cap
import fittings as fit
from rigid import PORT, STBD, X, Y, Z, Model, lerp, smooth
from shipyard import Yard, livery, weather_for


# ---- her hull: the old battlecruiser's, with the bulges of her rebuild -------------------------------------------

# the hull's deck edge (half widths as shares of 14.3 m): a long fine bow, a long parallel middle, a cruiser stern
# (its round end laid on close stations, so that the counter rounds into her sides without a crease)
AKAGI_PLAN = [(-130.35, 0.0), (-130.2, 0.16), (-129.9, 0.27), (-129.4, 0.36), (-128.6, 0.45), (-127.4, 0.53), (-126.0, 0.6), (-123.5, 0.68),
              (-120.0, 0.76), (-116.0, 0.825), (-112.0, 0.87), (-100.0, 0.95), (-86.0, 0.99),
              (-72.0, 1.0), (36.0, 1.0), (50.0, 0.97), (64.0, 0.91), (78.0, 0.82), (90.0, 0.72), (100.0, 0.61), (108.0, 0.5), (115.0, 0.39),
              (120.0, 0.3), (124.0, 0.21), (127.5, 0.12), (130.35, 0.0)]

# the flight deck's edges, aft to fore: 249.2 m, 30.5 wide, its fore end (the length added over the forecastle in the
# rebuild) 28 m wide and squared off with corners of about a metre, its after end rounded at the corners
AKAGI_PORT = [(-129.6, 10.4), (-129.2, 12.3), (-128.1, 13.7), (-126.0, 14.6), (-122.0, 15.1), (-116.0, 15.25), (57.0, 15.25), (61.5, 14.0),
              (118.3, 14.0), (119.05, 13.78), (119.45, 13.4), (119.6, 12.9)]
AKAGI_STAR = [(x, -y) for x, y in AKAGI_PORT]
DECK_FORE, DECK_AFT = 119.6, -129.6

FD_TOP = 20.6                      # the flight deck's top where it is level
FD_THICK = 0.7
LEVEL = (27.0, -104.0)             # (it falls 0.5 degrees to the bow forward of the first, 1.5 to the stern abaft the second)

AKAGI_PAINT = {"planks": "BCA27A", "flight": "BCA27A", "plane": "3E4A3C"}


def fdz(x):
    """The flight deck's top at x."""
    if x > LEVEL[0]:
        return FD_TOP - (min(x, DECK_FORE) - LEVEL[0]) * math.tan(math.radians(0.5))
    if x < LEVEL[1]:
        return FD_TOP - (LEVEL[1] - max(x, DECK_AFT)) * math.tan(math.radians(1.5))
    return FD_TOP


class AkagiHull(cap.PlanHull):
    """A hull laid to her plan, and swelled at the waterline by the bulges of 1935-38: 31.3 m over them, her side
    above rounding in to the 28.6 m of her old upper deck within the lower half of her freeboard."""

    bulge = 0.095

    def swell(self, u):
        return smooth((u + 0.84) / 0.22) * smooth((0.66 - u) / 0.3)

    def half_water(self, u):
        return super().half_water(u) * (1.0 + self.bulge * self.swell(u))

    def point(self, u, where, hand=PORT):
        p = super().point(u, where, hand)
        if where > 0.0:
            s = self.swell(u)
            if s > 0.0:
                base = cap.PlanHull.half_water(self, u)
                extra = base * self.bulge * s
                hd = max(self.half_deck(u), 0.12)
                plain = base + (hd - base) * where
                bulged = base + extra + (hd - base - extra) * where
                # (the bulge rounds in over the lower part of her side: none of it left by half way up)
                k = 1.0 - smooth(where / 0.5)
                p = Vector((p.x, hand * lerp(plain, bulged, k), p.z))
        return p


# ---- small helpers -------------------------------------------------------------------------------------------------

def walls(m, plan, zlo, zhi, paint="hull"):
    """Upright walls round a closed plan (anticlockwise seen from above), each from zlo(x) to zhi(x), facing out."""
    n = len(plan)
    for k in range(n):
        a, b = plan[k], plan[(k + 1) % n]
        out = Vector((b[1] - a[1], -(b[0] - a[0]), 0.0))
        if out.length < 1e-9:
            continue
        q = [Vector((a[0], a[1], zlo(a[0]))), Vector((b[0], b[1], zlo(b[0]))), Vector((b[0], b[1], zhi(b[0]))), Vector((a[0], a[1], zhi(a[0])))]
        m.add(q, [(0, 1, 2, 3)], paint, False, lambda _, o=out: o)


def top_of(m, plan, zf, paint, down=False):
    """A plan's top (or underside) laid at zf(x, y): cut into triangles that follow its bays."""
    pts = [Vector((px, py, zf(px, py))) for px, py in plan]
    tris = tessellate_polygon([[Vector((px, py, 0.0)) for px, py in plan]])
    m.add(pts, [tuple(t) for t in tris], paint, False, (lambda q: -Z) if down else (lambda q: Z))


def round_front(xf, xa, y0, y1, r_back=0.6, steps=6):
    """A plan whose fore end is a half round across its whole width and whose after corners are rounded a little
    (the Japanese island's), anticlockwise."""
    hw = (y1 - y0) * 0.5
    yc = (y0 + y1) * 0.5
    cx = xf - hw
    pts = []
    for k in range(steps + 1):                         # (from the starboard side round the nose to the port side)
        a = -math.pi / 2 + math.pi * k / steps
        pts.append((cx + math.cos(a) * hw, yc + math.sin(a) * hw))
    for q, (ccx, ccy) in enumerate(((xa + r_back, y1 - r_back), (xa + r_back, y0 + r_back))):
        for k in range(3):
            a = math.pi / 2 + (q + k / 2.0) * math.pi / 2
            pts.append((ccx + math.cos(a) * r_back, ccy + math.sin(a) * r_back))
    out = []
    for p in pts:
        if not out or (abs(out[-1][0] - p[0]) > 1e-6 or abs(out[-1][1] - p[1]) > 1e-6):
            out.append(p)
    return out


def brackets(m, x, y_wall, y_out, z, drop=1.8, paint="upper"):
    """A knee under a platform's edge: a slanting strut from the wall to the platform."""
    m.rod((x, y_wall, z - drop), (x, y_out, z - 0.12), 0.08, paint, sides=4)


def railing_arc(m, c, r, a0, a1, z, every=1.2):
    from ships import railing
    n = max(2, int(abs(a1 - a0) * r / every) + 1)
    pts = [Vector((c.x + math.cos(lerp(a0, a1, i / (n - 1))) * r, c.y + math.sin(lerp(a0, a1, i / (n - 1))) * r, z)) for i in range(n)]
    railing(m, pts, high=1.0, every=every)


def put(m, pts):
    """New points in the model, in no face yet; returns their numbers."""
    base = len(m.verts)
    m.verts.extend(Vector(p) for p in pts)
    n = len(pts)
    m.vcoat.extend([-1] * n)
    m.vblend.extend([0.0] * n)
    m.vsoot.extend([0.0] * n)
    return list(range(base, base + n))


def faces_on(m, faces, paint, out, smooth_=True):
    """Faces over points already in the model (by their numbers), each turned to look `out` (f(middle) -> a way), so
    that a face laid against a skin shares the skin's own points and no seam can show between them."""
    pi, pa = m.cur, m.paint_at[paint]
    for f in faces:
        vs = [m.verts[i] for i in f]
        n = (vs[1] - vs[0]).cross(vs[2] - vs[0])
        if len(f) > 3:
            n = n + (vs[2] - vs[0]).cross(vs[3] - vs[0])
        if n.length < 1e-12:
            continue
        mid = sum(vs, Vector()) / len(vs)
        if n.dot(out(mid)) < 0.0:
            f = tuple(reversed(f))
        m.faces.append(tuple(f))
        m.fpaint.append(pa)
        m.fpart.append(pi)
        m.fsmooth.append(smooth_)
        m.fcoat.append(False)


def drop_faces(m, which):
    keep = [i for i in range(len(m.faces)) if i not in which]
    m.faces = [m.faces[i] for i in keep]
    m.fpaint = [m.fpaint[i] for i in keep]
    m.fpart = [m.fpart[i] for i in keep]
    m.fsmooth = [m.fsmooth[i] for i in keep]
    m.fcoat = [m.fcoat[i] for i in keep]


def prune(m):
    """Drops the points that no face uses any more (the plating cut out of her side for her casemates' ports, the
    points of a wall's grid that fell inside an opening)."""
    used = [False] * len(m.verts)
    for f in m.faces:
        for i in f:
            used[i] = True
    if all(used):
        return
    new, k = [-1] * len(m.verts), 0
    for i, u in enumerate(used):
        if u:
            new[i] = k
            k += 1
    m.verts = [v for v, u in zip(m.verts, used) if u]
    m.vcoat = [v for v, u in zip(m.vcoat, used) if u]
    m.vblend = [v for v, u in zip(m.vblend, used) if u]
    m.vsoot = [v for v, u in zip(m.vsoot, used) if u]
    m.faces = [tuple(new[i] for i in f) for f in m.faces]
    m.lying = {new[i]: d for i, d in m.lying.items() if used[i]}


def tube(m, pts, r, paint, sides=6):
    """A round bar along a line of points (a rubbing strake along her side)."""
    pts = [Vector(p) for p in pts]
    n = len(pts)
    rings = []
    for k, p in enumerate(pts):
        t = (pts[min(k + 1, n - 1)] - pts[max(k - 1, 0)]).normalized()
        a = t.cross(Z)
        if a.length < 1e-6:
            a = t.cross(X)
        a.normalize()
        b = a.cross(t)
        rings.append([p + (a * math.cos(2 * math.pi * j / sides) + b * math.sin(2 * math.pi * j / sides)) * r for j in range(sides)])
    m.skin(rings, paint, (True, True))


def key(p):
    return (round(p[0], 3), round(p[1], 3), round(p[2], 3))


# ---- her plating cut for the casemates' ports ---------------------------------------------------------------------

def cut_ports(yard, ports, upto, depth=0.8, lip=0.4):
    """Cuts each port of `ports` ((x, z of its middle, hand, half width, half height, corner)) out of her side's
    plating: the skin's faces round it (of those made before `upto`) are taken away, and the plating is laid again from
    where they were cut to the port's edge, on the skin's own points; the port's jambs run `depth` in to a dark back,
    and an armour frame with bevelled edges stands round it, following the curve of her side. Her side's scribed seam
    that ran across the port is laid again up to the frame. Returns, for each port, the height of its frame's foot."""
    m, h = yard.m, yard.h
    fine = m.fine
    hull_p, seam_p = m.paint_at["hull"], m.paint_at["seam"]
    pad = lip + 0.15
    jobs = []
    for (x, zc, hand, a, b, r) in ports:
        pi = m.part_at[yard.half(x)]
        x0, x1, z0, z1 = x - a - pad, x + a + pad, zc - b - pad, zc + b + pad
        cut, seams = set(), set()
        for i in range(upto):
            if m.fpart[i] != pi or m.fpaint[i] not in (hull_p, seam_p):
                continue
            vs = [m.verts[j] for j in m.faces[i]]
            if min(v.y * hand for v in vs) < 1.0:
                continue
            if max(v.x for v in vs) < x0 or min(v.x for v in vs) > x1 or max(v.z for v in vs) < z0 or min(v.z for v in vs) > z1:
                continue
            if m.fpaint[i] == seam_p:
                seams.add(i)
                continue
            nrm = (vs[1] - vs[0]).cross(vs[2] - vs[0])
            if nrm.length > 1e-9 and abs(nrm.normalized().y) > 0.5:
                cut.add(i)
        # the edge of the plating taken away: a loop of the skin's own points round the port
        count = {}
        for i in cut:
            f = m.faces[i]
            for k in range(len(f)):
                e = (min(f[k], f[(k + 1) % len(f)]), max(f[k], f[(k + 1) % len(f)]))
                count[e] = count.get(e, 0) + 1
        nb = {}
        for (p, q), c in count.items():
            if c == 1:
                nb.setdefault(p, []).append(q)
                nb.setdefault(q, []).append(p)
        start = min(nb, key=lambda i: (m.verts[i].x, m.verts[i].z))
        loop, prev, cur = [start], None, start
        while True:
            q = [v for v in nb[cur] if v != prev][0]
            if q == start:
                break
            loop.append(q)
            prev, cur = cur, q
        jobs.append(((x, zc, hand, a, b, r), cut, seams, loop))
    drop_faces(m, set().union(*[j[1] for j in jobs], *[j[2] for j in jobs]))
    feet = []
    for (x, zc, hand, a, b, r), cut, seams, loop in jobs:
        m.use(yard.half(x))
        steps = 3 if fine else 1

        def outline(e):
            return Model.round_rect(x + a + e, x - a - e, zc + b + e, zc - b - e, r + e, steps)
        win2 = outline(0.0)
        on = [h.side(px, pz, hand) for px, pz in win2]
        nrm0 = h.side(x, zc, hand)[1]
        win = put(m, [p for p, _ in on])
        tris = tessellate_polygon([[Vector((m.verts[i].x, m.verts[i].z, 0.0)) for i in loop], [Vector((px, pz, 0.0)) for px, pz in win2]])
        ids = loop + win
        faces_on(m, [tuple(ids[i] for i in t) for t in tris], "hull", lambda q, n=nrm0: n)
        # the jambs, in to the dark back of the port
        nw = len(win)
        inner = put(m, [p - n * depth for p, n in on])
        faces_on(m, [(win[k], win[(k + 1) % nw], inner[(k + 1) % nw], inner[k]) for k in range(nw)], "steel",
                 lambda q, x=x, zc=zc: Vector((x - q.x, 0.0, zc - q.z)), smooth_=False)
        back = put(m, [sum((m.verts[i] for i in inner), Vector()) / nw])[0]
        faces_on(m, [(inner[k], inner[(k + 1) % nw], back) for k in range(nw)], "black", lambda q, n=nrm0: n, smooth_=False)
        if fine:
            # the armour frame round it, bevelled in and out, hugging the curve of her side
            rings = []
            for e, lift in ((0.0, 0.004), (0.13, 0.13), (0.27, 0.13), (lip, -0.03)):
                rings.append([p + n * lift for p, n in (h.side(px, pz, hand) for px, pz in outline(e))])
            for (ra, rb), way in zip(zip(rings, rings[1:]), (-1.0, 0.0, 1.0)):
                m.add(ra + rb, [(k, (k + 1) % nw, nw + (k + 1) % nw, nw + k) for k in range(nw)], "upper", False,
                      lambda q, w=way, n=nrm0, x=x, zc=zc: n + Vector((q.x - x, 0.0, q.z - zc)).normalized() * w)
        if seams and fine:
            xs_ = [m.verts[i].x for i in loop]
            for p0, p1 in ((min(xs_), x - a - lip + 0.03), (x + a + lip - 0.03, max(xs_))):
                if p1 - p0 > 0.1:
                    k = max(1, int((p1 - p0) / 0.8))
                    got = [h.side(lerp(p0, p1, i / k), 0.68 * h.deck_z(lerp(p0, p1, i / k)), hand) for i in range(k + 1)]
                    m.strip([p for p, _ in got], [n for _, n in got], 0.035, "seam", 0.012)
        feet.append(zc - b - lip)
    return feet


# ---- weathering: soot along her starboard side from the great funnel, rust weeping from her ports and hawses --------

def weep(yard, stains, x, z_top, hand, long, wide, rust=0.55, floor=1.0):
    """A weep of rust down her side from z_top: a ribbon lying on her plating (its points at the heights of her side's
    own rows), dark brown in its core and thinning to its edges, fading as it runs down."""
    m, h = yard.m, yard.h
    if not m.fine:
        return
    z_bot = max(z_top - long, floor)
    if z_top - z_bot < 0.2:
        return
    zd = h.deck_z(x)
    rows = [zd * a for a in (0.2, 0.31, 0.43, 0.6, 0.8)]
    zs = [z_top] + [z for z in sorted(rows, reverse=True) if z_bot + 0.05 < z < z_top - 0.05] + [z_bot]
    line = [h.side(x, z, hand) for z in zs]
    got = yard.on(x).ribbon([q for q, _ in line], [d for _, d in line], wide, "hull", 0.008, taper=lambda t: 1.0 - 0.5 * t, core=0.6)
    for k, z in enumerate(zs):
        fade = ((z - z_bot) / (z_top - z_bot)) ** 0.7
        stains[key(got[2][k])] = (rust * fade, 0.7 * rust * fade)
        for j in (1, 3):
            stains[key(got[j][k])] = (0.85 * rust * fade, 0.25 * rust * fade)


def akagi_weather(yard, stains):
    """The yard's weather for a ship in service, and besides: the soot her great funnel lays along her starboard side
    abaft and below its mouth, the rust in the weeps (`weep`)."""
    plain = weather_for(yard)

    def w(p, n, paint, part):
        own = paint[:-len(Model.ON)] if paint.endswith(Model.ON) else paint
        got = plain(p, n, own, part)
        wear, soot = got[0], got[1]
        if own == "hull":
            if p.y < -12.0 and -52.0 < p.x < -2.0 and 5.0 < p.z < 20.5:
                along = min(1.0, max(0.0, (p.x + 52.0) / 38.0)) * min(1.0, max(0.0, (-2.0 - p.x) / 6.0))
                high = max(0.0, 1.0 - abs(p.z - 13.5) / 7.0)
                soot = max(soot, 0.5 * along * high)
            s = stains.get(key(p))
            if s:
                wear, soot = max(wear, s[0]), max(soot, s[1])
        return wear, soot
    return w


# ---- the men's loads -----------------------------------------------------------------------------------------------

def round12(m, base, axis):
    """A fixed round for the 12 cm: its brass case and the shell, the nose drawn to a point."""
    axis = Vector(axis).normalized()
    m.lathe(base, axis, [(0.0, 0.072), (0.03, 0.068), (0.56, 0.064)], "brass", 8)
    m.lathe(base + axis * 0.56, axis, [(0.0, 0.06), (0.22, 0.06), (0.34, 0.038), (0.42, 0.006)], "gunmetal", 8, caps=(True, False))


def magazine25(m, c, f):
    """A Type 96's box magazine of fifteen rounds: a flat steel box, the rounds' brass showing along its lips."""
    yaw = math.degrees(math.atan2(f.y, f.x))
    m.box(c, (0.1, 0.4, 0.22), "gunmetal", yaw=yaw)
    m.box(c + Z * 0.12, (0.06, 0.34, 0.03), "brass", yaw=yaw)


def loader12(m, foot, facing):
    """A loader with a 12 cm round cradled in his arms, its nose up toward the breech."""
    f = Vector((facing[0], facing[1], 0.0)).normalized()
    l = Z.cross(f)
    ax = (f * 0.85 + Z * 0.25).normalized()
    c = foot + Z * 1.02 + f * 0.45
    fit.sailor(m, foot, f, "aim", hands=(c - ax * 0.1 + l * 0.09 - Z * 0.06, c + ax * 0.2 - l * 0.09 - Z * 0.06), navy="JAP", lean=0.06)
    round12(m, c - ax * 0.35, ax)


def bearer12(m, foot, facing):
    """A man bringing up the next round, carried across his chest."""
    f = Vector((facing[0], facing[1], 0.0)).normalized()
    l = Z.cross(f)
    c = foot + Z * 1.1 + f * 0.3
    fit.sailor(m, foot, f, "aim", hands=(c + l * 0.2 - Z * 0.05, c - l * 0.2 - Z * 0.05), navy="JAP")
    round12(m, c - l * 0.49, l)


def loader25(m, foot, facing, high=True):
    """A 25 mm loader with a full magazine: held up over his head to go into the gun, or at his chest to bring it."""
    f = Vector((facing[0], facing[1], 0.0)).normalized()
    l = Z.cross(f)
    c = foot + (Z * 1.86 + f * 0.22 if high else Z * 1.12 + f * 0.3)
    fit.sailor(m, foot, f, "aim", hands=(c + l * 0.17 - Z * 0.1, c - l * 0.17 - Z * 0.1), navy="JAP")
    magazine25(m, c, f)


# ---- her guns ------------------------------------------------------------------------------------------------------

def casemate20(yard, name, x, z, hand, limits=(-60, 60)):
    """A 20 cm/50 of the 3rd Year Type in its casemate low in her after hull: the port in her plating (`cut_ports`),
    and inside it the mounting's curved shield, turned on the gun's own pivot so that, as it trains, it closes the
    port and the gun's slot moves across it; the long barrel out through the slot with a canvas blast bag at its root.
    A `sec.*` turret (its bearing clockwise: -90 to port): the shield and gun train across the port, the gun lifts and
    runs back."""
    m = yard.m
    h = yard.h
    fine = m.fine
    bearing = -90.0 if hand == PORT else 90.0
    skin, nrm = h.side(x, z, hand)
    half = yard.half(x)
    m.use(half)
    d, R = 1.0, 1.05                                     # (the pivot so far in from her plating; the shield's face round it)
    piv = Vector((x, skin.y - hand * d, z))
    tn = m.part(name, half, tuple(piv), axis=(0, 0, -1), limits=list(limits), kind="turn",
                note=f"a 20 cm casemate gun: trains across its port (plus clockwise seen from above); rest bearing {int(bearing)}")
    m.parts[m.part_at[tn]].rest = bearing
    m.parts[m.part_at[tn]].rate = 6.0
    m.use(tn)
    rot = Matrix.Rotation(math.radians(-bearing), 3, "Z")
    f, lat = rot @ X, rot @ Y
    lo, hi = -0.52, 1.32
    k_ = 18 if fine else 7
    rows = []
    for i in range(k_ + 1):
        ph = math.radians(lerp(-130.0, 130.0, i / k_))
        c = piv + (f * math.cos(ph) + lat * math.sin(ph)) * R
        rows.append([c + Z * lo, c + Z * hi])
    m.sheet(rows, "hull", f, smooth=True)
    if fine:
        m.sheet([[r[1], r[1] + (piv + Z * hi - r[1]) * 0.35] for r in rows], "hull", Z, smooth=True)      # its roof
        m.sheet([[r[0] + (piv + Z * lo - r[0]) * 0.35, r[0]] for r in rows], "hull", -Z, smooth=True)     # and its floor
        c0 = piv + f * R
        m.strip([c0 + Z * (lo + 0.1), c0 + Z * (hi - 0.32)], [f, f], 0.27, "black", 0.02)                 # the gun's slot
        ph = math.radians(24.0)
        cs = piv + (f * math.cos(ph) + lat * math.sin(ph)) * R
        m.strip([cs + Z * 0.62, cs + Z * 0.82], [cs - piv, cs - piv], 0.15, "black", 0.02)                # the layer's sighting port
    gp = piv + f * 0.4 + Z * 0.05
    gn = m.part(f"{name}.gun.1", tn, tuple(gp), axis=tuple(-lat), limits=[-5, 25], slide=tuple(-f), slide_limits=[0.0, 0.9], kind="turn",
                note="the gun: plus lifts the muzzle; runs back when it fires")
    m.use(gn)
    e = math.radians(3.0)
    g = (f * math.cos(e) + Z * math.sin(e)).normalized()
    m.lathe(gp - g * 1.2, g, [(0.0, 0.34), (1.6, 0.33), (2.0, 0.27), (3.4, 0.24), (6.5, 0.18), (8.6, 0.155), (8.75, 0.17), (8.9, 0.165)],
            "hull", 12 if fine else 6, caps=(True, False))
    if fine:            # the blast bag where it leaves the shield
        m.lathe(gp + g * 0.5, g, [(0.0, 0.4), (0.14, 0.43), (0.3, 0.38), (0.44, 0.3)], "canvas", 12, caps=(False, False))
    tip = gp + g * 7.7
    m.disc(tip + g * 0.005, g, 0.1, "black", 8)
    m.socket(f"muzzle.{name}.1", tip, g, gn, "flash and smoke leave here")
    m.use(half)
    return tn, [gn]


def twin12(yard, name, x, y, z, bearing, limits, crew=True):
    """The twin 12 cm/45 high-angle mounting of the 10th Year Type, open, as she kept them to the end (the 12.7 cm meant
    to replace them never came): two guns side by side in one cradle on a pedestal, each with its recoil cylinder over
    it and its breech behind, the layer and trainer seated either side at their sights and handwheels, two loaders at
    the breeches with rounds in their arms, a man bringing up the next. A `sec.*` turret: parts, sockets and clip as
    the yard's."""
    m = yard.m
    half = yard.half(x)
    fine = m.fine
    m.use(half)
    o = Vector((x, y, z))
    m.lathe(o, Z, [(0.0, 1.3), (0.12, 1.25)], "upper", 20 if fine else 8)
    piv0 = o + Z * 0.12
    tn = m.part(name, half, tuple(piv0), axis=(0, 0, -1), limits=list(limits), kind="turn",
                note=f"a twin 12 cm high-angle mounting: plus trains it clockwise seen from above; rest bearing {int(bearing)}; about 12 degrees a second")
    m.parts[m.part_at[tn]].rest = bearing
    m.parts[m.part_at[tn]].rate = 12.0
    m.use(tn)
    rot = Matrix.Rotation(math.radians(-bearing), 3, "Z")
    f, l = rot @ X, rot @ Y

    def W(px, py, pz):
        return piv0 + f * px + l * py + Z * pz
    m.lathe(W(0, 0, 0), Z, [(0.0, 1.95), (0.08, 1.95)], "deck", 24 if fine else 8)        # the turning platform the crew stand on
    m.lathe(W(0, 0, 0.08), Z, [(0.0, 0.62), (0.45, 0.52), (0.95, 0.46)], "upper", 14 if fine else 6)
    for s in (1, -1):                                                                     # the carriage's side frames
        m.box(W(0.05, s * 0.95, 1.12), (1.35, 0.14, 1.3), "upper", yaw=-bearing)
        if fine:
            m.box(W(0.05, s * 0.95, 1.72), (0.5, 0.2, 0.22), "upper", yaw=-bearing)       # the trunnion bearing
    m.box(W(0.0, 0.0, 0.62), (1.2, 2.0, 0.22), "upper", yaw=-bearing)
    guns = []
    for k, s in enumerate((1, -1)):
        gp = W(0.05, s * 0.5, 1.72)
        gn = m.part(f"{name}.gun.{k + 1}", tn, tuple(gp), axis=tuple(-l), limits=[-5, 75], slide=tuple(-f), slide_limits=[0.0, 0.45], kind="turn",
                    note="a gun: plus lifts the muzzle; slides back along its own length when it fires")
        m.use(gn)
        e = math.radians(12.0)
        g = (f * math.cos(e) + Z * math.sin(e)).normalized()
        upg = g.cross(l).normalized()
        m.rod(gp - g * 0.75, gp + g * 1.05, 0.2, "upper", sides=12 if fine else 6)                 # the cradle's sleeve
        m.rod(gp + upg * 0.27 - g * 0.55, gp + upg * 0.27 + g * 1.0, 0.075, "upper", sides=8 if fine else 4)   # the recoil cylinder
        m.lathe(gp + g * 1.05, g, [(0.0, 0.125), (1.2, 0.108), (3.55, 0.085), (3.65, 0.095), (3.8, 0.09)], "upper", 10 if fine else 6, caps=(False, True))
        tip = gp + g * 4.85
        m.disc(tip + g * 0.004, g, 0.05, "black", 8)
        m.box(gp - g * 0.95, (0.5, 0.34, 0.4), "upper", yaw=-bearing, pitch=12.0)                  # the breech
        if fine:
            m.box(gp - g * 1.35 - upg * 0.12, (0.6, 0.2, 0.06), "steel", yaw=-bearing, pitch=12.0)  # the loading tray
            m.rod(gp - g * 0.7 + l * s * 0.24, gp - g * 0.7 + l * s * 0.24 - upg * 0.28, 0.03, "steel", sides=4)
        m.socket(f"muzzle.{name}.{k + 1}", tip, g, gn, "flash, smoke and the shell leave here")
        guns.append(gn)
    m.use(tn)
    for s in (1, -1):                       # the layer's and trainer's seats, sights and handwheels, out each side
        seat = W(-0.25, s * 1.42, 0.62)
        m.box(seat, (0.36, 0.36, 0.06), "gunmetal", yaw=-bearing)
        m.rod(W(-0.25, s * 1.42, 0.08), seat, 0.04, "upper", sides=5)
        m.rod(W(0.0, s * 1.02, 1.3), W(0.45, s * 1.3, 1.35), 0.04, "upper", sides=4)
        m.rod(W(0.2, s * 1.32, 1.42), W(0.7, s * 1.32, 1.42), 0.07, "gunmetal", sides=6)          # his sight
        if fine:
            m.disc(W(0.71, s * 1.32, 1.42), f, 0.06, "glass", 8)
            for hw in (0.13, -0.13):
                m.lathe(W(0.25, s * 1.3 + hw, 1.0), f, [(0.0, 0.15), (0.03, 0.15)], "gunmetal", 10, caps=(False, False))
            m.rod(W(0.0, s * 1.02, 1.0), W(0.25, s * 1.3, 1.0), 0.035, "upper", sides=4)
    if fine:
        m.box(W(-0.95, 1.2, 0.55), (0.6, 0.45, 0.95), "upper", yaw=-bearing)                     # the fuze setter
        m.box(W(-0.95, 1.2, 1.05), (0.5, 0.3, 0.06), "gunmetal", yaw=-bearing)
    if crew and fine:
        for s in (1, -1):
            fit.sailor(m, W(-0.22, s * 1.42, 0.08), f, "sit", seat=0.54, hands=(W(0.22, s * 1.3 + 0.14, 1.0), W(0.22, s * 1.3 - 0.14, 1.0)),
                       navy="JAP", lean=0.1)
            loader12(m, W(-1.82, s * 0.66, 0.08), f)
        bearer12(m, W(-1.3, -1.25, 0.08), f + l * 0.6)
    m.use(half)
    return tn, guns


def twin25(yard, n, x, y, z, bearing, crew=True, parent=None):
    """Japan's twin 25 mm mounting (Type 96): two guns side by side in one cradle on a pedestal, a long flash hider on
    each and the box magazine standing on top, the layer and trainer seated either side; open, no shield, as they
    were in 1941-42. Parts `aa25.N` (trains: plus clockwise seen from above) and `aa25.N.guns` (plus lifts them);
    socket `aa.K` at the muzzles."""
    m = yard.m
    half = parent or yard.half(x)
    m.use(half)
    fine = m.fine
    o = Vector((x, y, z))
    tn = m.part(f"aa25.{n}", half, (x, y, z + 0.2), axis=(0, 0, -1), limits=[-180, 180], kind="turn",
                note=f"a twin 25 mm mounting: plus trains it clockwise seen from above; rest bearing {int(bearing)}; about 30 degrees a second")
    m.parts[m.part_at[tn]].rest = bearing
    m.parts[m.part_at[tn]].rate = 30.0
    m.use(tn)
    rot = Matrix.Rotation(math.radians(-bearing), 3, "Z")
    f, l = rot @ X, rot @ Y

    def W(px, py, pz):
        return o + f * px + l * py + Z * pz
    m.lathe(o, Z, [(0.0, 0.42), (0.12, 0.36), (0.9, 0.26)], "upper", 12 if fine else 6)
    m.lathe(o + Z * 0.2, Z, [(0.0, 1.0), (0.06, 1.0)], "deck", 16 if fine else 6)
    for s in (1, -1):
        m.box(W(0.0, s * 0.36, 0.98), (0.55, 0.08, 0.45), "upper", yaw=-bearing)
    piv = W(0.0, 0.0, 1.18)
    gn = m.part(f"aa25.{n}.guns", tn, tuple(piv), axis=tuple(-l), limits=[-10, 85], kind="turn", note="the two guns: plus lifts them")
    m.use(gn)
    e = math.radians(15.0)
    g = (f * math.cos(e) + Z * math.sin(e)).normalized()
    upg = g.cross(l).normalized()
    for s in (1, -1):
        q = piv + l * s * 0.17
        m.box(q - g * 0.2, (1.05, 0.15, 0.24), "gunmetal", yaw=-bearing, pitch=15.0)
        m.rod(q + g * 0.32, q + g * 1.7, 0.03, "gunmetal", sides=6 if fine else 4)
        m.lathe(q + g * 1.7, g, [(0.0, 0.045), (0.48, 0.05), (0.53, 0.036)], "gunmetal", 8 if fine else 4)
        if fine:
            m.box(q - g * 0.18 + upg * 0.27, (0.34, 0.1, 0.3), "gunmetal", yaw=-bearing, pitch=15.0)     # the magazine
            m.rod(q - g * 0.62, q - g * 0.85 - upg * 0.16, 0.02, "gunmetal", sides=4)                  # the shoulder stock
    m.box(piv - g * 0.05, (0.35, 0.5, 0.12), "gunmetal", yaw=-bearing, pitch=15.0)
    k = sum(1 for s in m.sockets if s["name"].startswith("aa.")) + 1
    m.socket(f"aa.{k}", piv + g * 2.25, g, gn, "a twin 25 mm mounting's muzzles: two streams of tracer start here")
    m.use(tn)
    for s in (1, -1):
        s0 = W(-0.32, s * 0.78, 0.2)
        m.box(s0 + Z * 0.4, (0.32, 0.32, 0.06), "gunmetal", yaw=-bearing)
        m.rod(s0, s0 + Z * 0.38, 0.035, "upper", sides=5)
        m.box(W(0.2, s * 0.62, 1.32), (0.18, 0.1, 0.18), "gunmetal", yaw=-bearing)                  # his sight
        if fine:
            m.lathe(W(0.0, s * 0.62, 0.95), l * s, [(0.0, 0.12), (0.025, 0.12)], "gunmetal", 8, caps=(False, False))
    if crew and fine:
        for s in (1, -1):
            fit.sailor(m, W(-0.3, s * 0.78, 0.2), f, "sit", seat=0.44, hands=(W(0.05, s * 0.66 + 0.1, 0.98), W(0.05, s * 0.66 - 0.1, 0.98)), navy="JAP", lean=0.08)
        loader25(m, W(-1.0, 0.3, 0.2), f, high=True)
        loader25(m, W(-1.45, -0.35, 0.2), f, high=False)
    m.use(half)
    return tn


def director94(yard, name, x, y, z, bearing=0.0, parent=None):
    """The Type 94 high-angle director: a squat round house turning on a pedestal, its rangefinder's arms out each
    side with a hood at each end, the layers' window across its face. A turn part."""
    m = yard.m
    dn = m.part(name, parent or yard.half(x), (x, y, z), axis=(0, 0, -1), kind="turn", note=f"a high-angle director: turns with the 12 cm guns it serves; rest bearing {int(bearing)}")
    m.parts[m.part_at[dn]].rest = bearing
    m.use(dn)
    rot = Matrix.Rotation(math.radians(-bearing), 3, "Z")
    f, lat = rot @ X, rot @ Y
    o = Vector((x, y, z))
    m.lathe(o, Z, [(0.0, 0.7), (0.5, 0.6)], "upper", 12 if m.fine else 6)
    m.lathe(o + Z * 0.5, Z, [(0.0, 1.35), (1.5, 1.3), (1.75, 0.95)], "upper", 18 if m.fine else 8)
    c = o + Z * 1.35
    m.lathe(c - lat * 2.2, lat, [(0.0, 0.2), (0.25, 0.25), (4.15, 0.25), (4.4, 0.2)], "upper", 10 if m.fine else 5)
    for s in (1, -1):
        m.box(c + lat * s * 2.0 + f * 0.15, (0.55, 0.45, 0.55), "upper", yaw=-bearing)
        m.disc(c + lat * s * 2.0 + f * 0.43, f, 0.12, "glass", 8)
    if m.fine:
        m.box(o + Z * 1.55 + f * 1.25, (0.06, 1.5, 0.3), "glass", yaw=-bearing)
    return dn


# ---- her funnels ---------------------------------------------------------------------------------------------------

def main_funnel(yard, x, wall_y, z0, rx=4.5, rz=3.4, out=2.4, bend=5.2, down=32.0, run=3.0, sweep=8.0, casing=(6.4, 4.0)):
    """Her great funnel (the uptakes of all nineteen boilers in one), led out through her starboard side in a casing
    faired from a broad rounded oblong on her side into the trunk's oval, swept a little aft as it leaves her, turned
    down `down` degrees below the level and its mouth facing the sea; banded round where its plates are stiffened;
    sprayed with water as she landed her aircraft, so a ring of spray pipes round its mouth."""
    m = yard.on(x)
    fine = m.fine
    n = 32 if fine else 12
    hand = -1.0 if wall_y < 0 else 1.0
    sw = math.radians(sweep)
    u = Vector((-math.sin(sw), hand * math.cos(sw), 0.0))       # out from her side and a little aft
    side = Z.cross(u).normalized()
    a_end = math.radians(down)
    wall = Vector((x, wall_y, z0))

    def section(c, across, up_, a, b, e):
        pts = []
        for k in range(n):
            t = 2 * math.pi * k / n
            ct, st = math.cos(t), math.sin(t)
            pts.append(c + across * (math.copysign(abs(ct) ** (2.0 / e), ct) * a) + up_ * (math.copysign(abs(st) ** (2.0 / e), st) * b))
        return pts
    ca, cb = casing
    across_w = X * (1.0 if side.x >= 0 else -1.0)
    rings, path, tans = [], [], []
    # the casing's foot on her side (and a little way into it, so that no seam shows), easing out into the trunk
    for c in (wall - Vector((0, hand * 0.7, 0)), wall):
        rings.append(section(c, across_w, Z, ca, cb, 5.0))
        path.append(c)
        tans.append(Vector((0, hand, 0)))
    for fr in ((0.15, 0.33, 0.55, 0.78, 1.0) if fine else (0.4, 1.0)):
        s = smooth(fr)
        c = wall + u * (out * fr)
        rings.append(section(c, side, Z, lerp(ca, rx, s), lerp(cb, rz, s), lerp(5.0, 2.0, s)))
        path.append(c)
        tans.append(u)
    first_trunk = len(rings) - 1
    last = wall + u * out
    nb, nr = (8, 3) if fine else (3, 1)
    for i in range(1, nb + nr + 1):
        if i <= nb:
            th = a_end * i / nb
            c = last + u * (math.sin(th) * bend) - Z * ((1.0 - math.cos(th)) * bend)
        else:
            th = a_end
            c = last + u * (math.sin(th) * bend) - Z * ((1.0 - math.cos(th)) * bend) + (u * math.cos(th) - Z * math.sin(th)) * (run * (i - nb) / nr)
        tan = (u * math.cos(th) - Z * math.sin(th)).normalized()
        up_ = (Z - tan * Z.dot(tan)).normalized()
        g = lerp(1.0, 1.08, i / (nb + nr))
        rings.append(section(c, up_.cross(tan), up_, rx * g, rz * g, 2.0))
        path.append(c)
        tans.append(tan)
    m.skin(rings, "upper", (False, False))
    end, tan = path[-1], tans[-1]
    up = (Z - tan * Z.dot(tan)).normalized()
    across = up.cross(tan)
    lip = [end + (q - end) * 1.05 for q in rings[-1]]
    m.skin([rings[-1], lip, [q + tan * 0.3 for q in lip]], "upper", (False, False))
    inner = [end + (q - end) * 0.9 + tan * 0.3 for q in rings[-1]]
    m.skin([[q + tan * 0.3 for q in lip], inner], "upper", (False, False))
    m.disc(end - tan * 0.4, tan, 1.0, "black", 4)
    deep = [end + (q - end) * 0.9 - tan * 1.4 for q in rings[-1]]
    m.skin([deep, inner], "black", (True, False), inside=True, cap_paint="black")
    if fine:
        # bands round it where its plates are stiffened
        for i in (first_trunk, first_trunk + nb // 2, first_trunk + nb):
            cen, t_ = path[i], tans[i]
            r0 = rings[i]
            band = [[cen + (q - cen) * sc + t_ * dt for q in r0] for sc, dt in ((0.995, -0.2), (1.03, -0.15), (1.03, 0.15), (0.995, 0.2))]
            m.skin(band, "upper", (False, False))
        for j in range(-3, 4):                            # bars across its mouth
            c = end + tan * 0.15 + across * (j * rx * 0.26)
            w = rz * math.sqrt(max(0.05, 1.0 - (j * 0.26) ** 2)) * 0.95
            m.rod(c - up * w, c + up * w, 0.05, "steel", sides=4, smooth=False)
        ring = [end - tan * 0.6 + across * math.cos(2 * math.pi * k / 24) * rx * 1.13 + up * math.sin(2 * math.pi * k / 24) * rz * 1.13 for k in range(25)]
        for a, b in zip(ring, ring[1:]):                  # the spray pipe round its mouth, on clips
            m.rod(a, b, 0.07, "steel", sides=5, smooth=False)
        for k in range(0, 24, 4):
            m.rod(ring[k], end - tan * 0.6 + (ring[k] - end + tan * 0.6) * 0.93, 0.05, "steel", sides=4)
    k = sum(1 for s in m.sockets if s["name"].startswith("funnel.")) + 1
    m.socket(f"funnel.{k}", end + tan * 0.8, tan, yard.half(x), "her great funnel's mouth, turned down over her starboard side: her smoke rolls out low over the water and aft")
    yard.soot_at.append((end.x, end.y, end.z + 2.0, 7.0))
    return end


# ---- her island ----------------------------------------------------------------------------------------------------

def island(yard, xi, yi, zd, zg, crew=True):
    """The island on her port side, on a sponson of its own out over her side: four storeys over her deck as
    photographs of her show it (about 11 m to the rangefinder's top, the mast's head about 18 m over the deck). A
    deckhouse with a round front and a searchlight on its nose; over it the chart and flying-control house, windows
    round its front; the navigating bridge, its windows round the front under an eyebrow; the open compass bridge on
    its roof behind a screen hung with rope mattresses, and behind that the air defence station, a small house
    carrying the rangefinder. The tripod mast stands on the deckhouse's roof abaft it, with its yard and halyards, the
    gaff her ensign flies from and a direction-finding loop. Life rafts, life rings, flag bags, ladders up its after
    faces. Returns the gaff's end and the mast's head."""
    m = yard.m
    fine = m.fine
    from ships import bulwark, ladder, railing
    m.use(yard.half(xi))
    # the sponson it stands on, from the gallery up to the deck's level, on knees from her side
    spon = Model.round_rect(xi + 6.2, xi - 6.6, 15.25, yi + 2.9, 1.0, 3 if fine else 1)
    cap.slab(m, spon, zg - 0.4, zd, paint="hull", top="deck")
    w0 = yard.h.half_at(xi) * 0.985
    for dx in (-5.6, -2.0, 1.6, 5.0):
        m.rod((xi + dx, w0, zg - 4.2), (xi + dx, yi + 2.6, zg - 0.5), 0.2, "upper", sides=5)
    if fine:
        railing(m, [Vector((xi + 6.0, yi + 2.75, zd)), Vector((xi - 6.4, yi + 2.75, zd))], every=1.5)
    H = (2.6, 2.5, 2.5)
    tz = zd
    levels = ((xi + 4.6, xi - 5.6, 2.2, H[0], "house"), (xi + 4.3, xi - 4.0, 2.25, H[1], "chart"), (xi + 3.9, xi - 3.6, 2.4, H[2], "bridge"))
    plans = []
    for xf, xa, hw, hh, what in levels:
        pl = round_front(xf, xa, yi - hw, yi + hw, 0.6, 8 if fine else 3)
        plans.append(pl)
        cap.slab(m, pl, tz, tz + hh, paint="upper", top="deck")
        if fine:
            n_ = len(pl)
            for i in range(n_):
                a, b = Vector((*pl[i], 0)), Vector((*pl[(i + 1) % n_], 0))
                d = b - a
                if d.length < 0.3:
                    continue
                o_ = Vector((d.y, -d.x, 0)).normalized()
                c = (a + b) * 0.5 + o_ * 0.02
                front = min(a.x, b.x) > xf - hw - 0.2
                if what == "bridge" and front:            # the navigating bridge's windows round its front
                    u = d.normalized() * (d.length * 0.5 - 0.06)
                    m.add([c - u + Z * (tz + 1.15), c + u + Z * (tz + 1.15), c + u + Z * (tz + 2.05), c - u + Z * (tz + 2.05)], [(0, 1, 2, 3)], "glass", False,
                          lambda q, o=o_: o)
                elif what == "chart" and front:           # smaller windows round the chart house's front
                    u = d.normalized() * (d.length * 0.5 - 0.16)
                    m.add([c - u + Z * (tz + 1.1), c + u + Z * (tz + 1.1), c + u + Z * (tz + 1.72), c - u + Z * (tz + 1.72)], [(0, 1, 2, 3)], "glass", False,
                          lambda q, o=o_: o)
                elif abs(o_.y) > 0.9 and d.length > 3.0 and what != "bridge":       # scuttles down the long sides
                    for j in (-1, 1):
                        m.disc(c + d.normalized() * j * d.length * 0.25 + Z * (tz + (1.7 if what == "house" else 1.6)), o_, 0.17, "glass", 8)
                elif o_.x < -0.9 and what == "house":     # a door in the deckhouse's after end
                    m.add([c - Y * 0.4 + Z * (tz + 0.1), c + Y * 0.4 + Z * (tz + 0.1), c + Y * 0.4 + Z * (tz + 1.95), c - Y * 0.4 + Z * (tz + 1.95)],
                          [(0, 1, 2, 3)], "door", False, lambda q, o=o_: o)
            if what == "bridge":                          # the eyebrow over the bridge's windows
                eb = [p for p in pl if p[0] > xi + 0.2]
                m.strip([Vector((px, py, tz + 2.18)) for px, py in eb], [Z] * len(eb), 0.25, "upper", 0.0)
            if what == "chart":
                eb = [p for p in pl if p[0] > xi + 0.6]
                m.strip([Vector((px, py, tz + 1.82)) for px, py in eb], [Z] * len(eb), 0.12, "upper", 0.0)
        tz += hh
    z_br = tz                                             # the navigating bridge's roof
    # the open compass bridge on the bridge's roof: a floor and a screen round its front and sides, open behind
    pl = round_front(xi + 3.3, xi - 0.4, yi - 2.1, yi + 2.1, 0.6, 8 if fine else 3)
    m.prism(pl, z_br - 0.06, z_br + 0.06, "upper", top_paint="deck", bottom=False)
    star_side = [p for p in pl if p[1] < yi and p[0] > xi + 0.25]
    port_side = [p for p in pl if p[1] >= yi and p[0] > xi + 0.25]
    arc = sorted(star_side, key=lambda p: p[0]) + sorted(port_side, key=lambda p: -p[0])
    bulwark(m, arc, z_br + 0.06, z_br + 1.31, 0.06, "upper")
    if fine:
        for p in arc[1:-1:2]:                             # the voice pipes and the binocular pedestals along the screen
            c = Vector((lerp(p[0], xi + 1.0, 0.18), lerp(p[1], yi, 0.18), z_br + 0.06))
            m.rod(c, c + Z * 1.05, 0.05, "steel", sides=5)
            m.box(c + Z * 1.15, (0.18, 0.32, 0.12), "gunmetal")
        for p in arc[2:-2:2]:                             # rope mattresses lashed outside the screen against splinters
            o_ = Vector((p[0] - (xi + 1.2), p[1] - yi, 0.0)).normalized()
            m.box(Vector((p[0], p[1], z_br + 0.72)) + o_ * 0.08, (0.08, 0.85, 0.85), "bag", yaw=math.degrees(math.atan2(o_.y, o_.x)))
    # the air defence station behind it: a small house carrying the rangefinder
    dh = Model.round_rect(xi - 0.2, xi - 2.8, yi - 1.5, yi + 1.5, 0.5, 3 if fine else 1)
    cap.slab(m, dh, z_br, z_br + 2.4, paint="upper", top="deck")
    z_top = z_br + 2.4
    if fine:
        m.add([Vector((xi - 0.18, yi - 0.95, z_br + 1.45)), Vector((xi - 0.18, yi + 0.95, z_br + 1.45)), Vector((xi - 0.18, yi + 0.95, z_br + 1.8)),
               Vector((xi - 0.18, yi - 0.95, z_br + 1.8))], [(0, 1, 2, 3)], "glass", False, lambda q: X)
        for s in (1, -1):
            m.disc(Vector((xi - 1.5, yi + s * 1.52, z_br + 1.6)), Vector((0, s, 0)), 0.17, "glass", 8)
    fit.rangefinder(yard, "director.fore", xi - 1.5, yi, z_top, 2.0, bearing=0.0, house=0.9, parent=yard.half(xi))
    m.use(yard.half(xi))
    # the searchlight platform on the round fore end of the deckhouse
    c = Vector((xi + 5.2, yi, zd + H[0]))
    m.lathe(c - Z * 0.25, Z, [(0.0, 1.35), (0.25, 1.35)], "upper", 18 if fine else 8)
    m.rod(c - Z * 0.25 + X * 0.2, Vector((xi + 3.8, yi, zd + 0.9)), 0.12, "upper", sides=5)
    if fine:
        railing_arc(m, c, 1.3, -math.pi * 0.55, math.pi * 0.55, c.z, every=0.9)
    yard.searchlight(c.x + 0.1, c.y, c.z, 0.55, (1, 0, 0.05))
    m.use(yard.half(xi))
    # the tripod mast on the deckhouse's roof abaft the upper storeys, its yard, halyards, gaff and loop
    foot = Vector((xi - 4.7, yi, zd + H[0]))
    head = Vector((xi - 5.2, yi, zd + 18.0))

    def px(z):
        return lerp(foot.x, head.x, (z - foot.z) / (head.z - foot.z))
    ln = (head - foot).length
    m.lathe(foot, head - foot, [(0.0, 0.26), (ln * 0.6, 0.2), (ln, 0.09)], "upper", 8 if fine else 5)
    zj = zd + 11.5
    for s in (1, -1):
        m.rod((xi - 5.35, yi + s * 1.7, zd + H[0]), (px(zj), yi, zj), 0.13, "upper", sides=6 if fine else 4)
    zy = zd + 14.5
    m.rod((px(zy), yi - 3.4, zy), (px(zy), yi + 3.4, zy), 0.07, "upper", rb=0.07, sides=4)
    zp = zd + 11.7
    m.lathe((px(zp), yi, zp), Z, [(0.0, 0.7), (0.12, 0.7)], "upper", 12 if fine else 6)
    gaff0 = Vector((px(zd + 12.6), yi, zd + 12.6))
    gaff1 = gaff0 + Vector((-3.3, 0.0, 1.4))
    m.rod(gaff0, gaff1, 0.06, "upper", sides=4)
    if fine:
        for s in (1, -1):                                 # the halyards from the yard's arms down to the bridge's roof
            yard.wire((px(zy), yi + s * 3.2, zy), (xi - 0.5, yi + s * 1.95, z_br + 0.2), 0.015)
        m.lathe(head + Z * 0.1, Z, [(0.0, 0.05), (0.6, 0.04)], "upper", 4)
        m.lathe(head + Z * 0.75, Z, [(0.0, 0.0), (0.12, 0.28), (0.24, 0.0)], "steel", 8)     # a direction-finding loop's frame
        # ladders up its after faces
        ladder(m, Vector((xi - 5.6, yi + 1.0, zd)), zd + H[0], (-1, 0, 0))
        ladder(m, Vector((xi - 4.0, yi - 1.2, zd + H[0])), zd + H[0] + H[1], (-1, 0, 0))
        ladder(m, Vector((xi - 3.6, yi - 1.2, zd + H[0] + H[1])), z_br, (-1, 0, 0))
        ladder(m, Vector((xi - 2.8, yi + 0.8, z_br)), z_top, (-1, 0, 0))
        # rails round the deckhouse's roof abaft the chart house, and round the bridge's roof abaft its front
        railing(m, [Vector((px_, py_, zd + H[0])) for px_, py_ in plans[0] if px_ < xi - 4.1], every=1.2)
        railing(m, [Vector((px_, py_, z_br)) for px_, py_ in plans[2] if px_ < xi + 0.4], every=1.2)
        # flag bags on the bridge's roof each side of the air defence station, under their canvas
        for s in (1, -1):
            m.box(Vector((xi - 1.5, yi + s * 1.95, z_br + 0.45)), (1.3, 0.5, 0.9), "bag")
            m.box(Vector((xi - 1.5, yi + s * 1.95, z_br + 0.93)), (1.36, 0.56, 0.06), "canvas")
        # life rafts on the deckhouse's outboard side and on the chart house's after face; life rings
        fit.float_rack(yard, xi - 1.1, yi + 2.22, zd + 1.2, (0, 1, 0), n=3, long=2.6, wide=1.1)
        fit.float_rack(yard, xi - 4.12, yi + 1.35, zd + H[0] + 0.75, (-1, 0, 0), n=2, long=1.9, wide=1.0)
        m.use(yard.half(xi))
        for (dx, dz, hw) in ((1.6, 1.0, 2.2), (-2.6, H[0] + 0.8, 2.25)):
            for s in (1, -1):
                p = Vector((xi + dx, yi + s * (hw + 0.03), zd + dz))
                m.lathe(p, Vector((0, s, 0)), [(0.0, 0.21), (0.0, 0.34), (0.11, 0.34), (0.11, 0.21), (0.0, 0.21)], "cork", 16, caps=(False, False))
    if crew and fine:
        rg = random.Random(1201)
        for (dx, dy, face) in ((2.1, 0.6, (1, 0)), (1.2, -1.1, (1, -0.4)), (0.4, 1.2, (0.4, 1)), (2.4, -0.4, (1, 0.2))):
            fit.sailor(m, Vector((xi + dx, yi + dy, z_br + 0.06)), Vector((*face, 0)), "stand", navy="JAP")
        for (dx, dy) in ((5.0, 0.95), (5.0, -0.95)):
            fit.sailor(m, Vector((xi + dx, yi + dy, zd + H[0])), Vector((1, rg.uniform(-0.5, 0.5), 0)), "stand", navy="JAP")
        fit.sailor(m, Vector((xi - 2.4, yi + 1.0, z_top)), Vector((0.3, 1, 0)), "stand", navy="JAP")         # a lookout by the rangefinder
        fit.sailor(m, Vector((xi - 2.6, yi + 1.95, z_br)), Vector((0.2, 1, 0)), "stand", navy="JAP")         # a signalman by the flag bags
    return gaff1, head


# ---- her forecastle ------------------------------------------------------------------------------------------------

def chain(m, pts, fine=True, pitch=0.36):
    """A stud-link cable along a line of points: links of about half a metre, each turned a quarter round to the last."""
    pts = [Vector(p) for p in pts]
    segs = [(a, b, (b - a).length) for a, b in zip(pts, pts[1:]) if (b - a).length > 1e-6]
    total = sum(s[2] for s in segs)
    ring_n, tube_n = (8, 4) if fine else (4, 3)
    for k in range(int(total / pitch) + 1):
        s = k * pitch
        for a, b, L in segs:
            if s <= L:
                break
            s -= L
        T = (b - a).normalized()
        P = a + T * min(s, L)
        side = Z.cross(T)
        if side.length < 1e-6:
            side = X.cross(T)
        side.normalize()
        Wd = side if k % 2 == 0 else T.cross(side).normalized()
        N = T.cross(Wd).normalized()
        loops = []
        for i in range(ring_n + 1):
            t = 2 * math.pi * i / ring_n
            c = P + T * (math.cos(t) * 0.21) + Wd * (math.sin(t) * 0.12)
            tg = (-T * (math.sin(t) * 0.21) + Wd * (math.cos(t) * 0.12)).normalized()
            rr = tg.cross(N).normalized()
            loops.append([c + (N * math.cos(2 * math.pi * j / tube_n) + rr * math.sin(2 * math.pi * j / tube_n)) * 0.045 for j in range(tube_n)])
        m.skin(loops, "steel", (False, False))


def forecastle(yard, stains):
    """Her forecastle, open under the flight deck's fore end: each cable led from its hawse pipe over a chain stopper
    to its cable holder, round the holder and down its chain pipe; the anchors drawn up into their hawses on her bow,
    their crowns and flukes against her side; two warping capstans, a low breakwater, mushroom vents and a hatch."""
    m, h = yard.m, yard.h
    fine = m.fine
    D = h.deck_z

    def deck_at(x, y):
        e = h.half_at(x)
        hw = max(e - 0.35, e * 0.5)
        return D(x) + 0.02 + 0.22 * max(0.0, 1.0 - (y / hw) ** 2) * min(1.0, hw / 4.0)
    xh, xs, xw, xn = 120.4, 117.4, 113.6, 111.0           # the hawse pipes, the stoppers, the cable holders, the chain pipes
    for hand in (PORT, STBD):
        yh = hand * min(2.4, h.half_at(xh) * 0.55)
        yw = hand * 1.75
        m.use(yard.half(xh))
        zh = deck_at(xh, yh)
        # the hawse pipe's mouth on the deck: a raised lip round a dark hole
        m.lathe((xh, yh, zh - 0.1), Z, [(0.0, 0.62), (0.28, 0.52), (0.28, 0.4)], "steel", 14 if fine else 6, caps=(False, False))
        m.disc((xh, yh, zh + 0.1), Z, 0.42, "black", 12 if fine else 6)
        # the cable holder: a drum with its groove and whelps on a bed plate, the brake's handwheel beside it
        zw = deck_at(xw, yw)
        m.lathe((xw, yw, zw - 0.05), Z, [(0.0, 0.95), (0.12, 0.95), (0.16, 0.7), (0.3, 0.6), (0.42, 0.78), (0.52, 0.78), (0.58, 0.55), (0.7, 0.5), (0.78, 0.28)],
                "steel", 16 if fine else 8)
        # the chain pipe down to the cable locker
        zn = deck_at(xn, yw)
        m.lathe((xn, yw, zn - 0.1), Z, [(0.0, 0.48), (0.24, 0.4), (0.24, 0.3)], "steel", 12 if fine else 6, caps=(False, False))
        m.disc((xn, yw, zn + 0.08), Z, 0.31, "black", 10 if fine else 5)
        # the stopper: a bed with two cheeks either side of the cable and a bar over it
        ys = lerp(yh, yw, (xh - xs) / (xh - xw))
        zs = deck_at(xs, ys)
        yaw = math.degrees(math.atan2(yw - yh, xw - xh))
        m.box((xs, ys, zs + 0.05), (1.4, 0.75, 0.12), "steel", yaw=yaw)
        if fine:
            for s in (1, -1):
                m.box((xs, ys + s * 0.3, zs + 0.28), (1.0, 0.1, 0.4), "steel", yaw=yaw)
            m.rod((xs + 0.25, ys - 0.33, zs + 0.42), (xs + 0.25, ys + 0.33, zs + 0.42), 0.05, "steel", sides=6)
            for i in range(8):                               # the holder's whelps
                a = 2 * math.pi * i / 8
                m.box((xw + math.cos(a) * 0.62, yw + math.sin(a) * 0.62, zw + 0.3), (0.14, 0.1, 0.2), "steel", yaw=math.degrees(a))
            hw_ = Vector((xw - 0.4, yw + hand * 1.15, zw))
            m.rod(hw_, hw_ + Z * 0.9, 0.05, "steel", sides=6)
            m.lathe(hw_ + Z * 0.9, Z, [(0.0, 0.3), (0.04, 0.3)], "steel", 10, caps=(False, False))
            m.rod(hw_ + Z * 0.92 - Y * 0.3, hw_ + Z * 0.92 + Y * 0.3, 0.025, "steel", sides=4)
            m.rod(hw_ + Z * 0.92 - X * 0.3, hw_ + Z * 0.92 + X * 0.3, 0.025, "steel", sides=4)
        # the cable: up out of the hawse pipe, over the stopper, round the holder's outboard side and down its pipe
        path = [Vector((xh + 0.1, yh, zh - 0.25)), Vector((xh - 0.7, lerp(yh, yw, 0.7 / (xh - xw)), deck_at(xh - 0.7, yh) + 0.1))]
        path += [Vector((xs + 0.7, ys, zs + 0.18)), Vector((xs - 0.7, ys, zs + 0.18))]
        for i in range(9):
            t = math.pi * i / 8.0
            path.append(Vector((xw + math.cos(t) * 0.72, yw + hand * math.sin(t) * 0.72, zw + 0.36)))
        path += [Vector((xn + 0.45, yw, zn + 0.12)), Vector((xn, yw, zn - 0.3))]
        if fine:
            chain(m, path, fine)
        else:
            m.strip(path, [Z] * len(path), 0.14, "steel", 0.03)
        # the anchor, drawn up into its hawse on the bow: the bolster round the mouth, the crown and flukes against her
        p, n = h.side(xh + 2.4, D(xh) - 2.5, hand)
        m.lathe(p - n * 0.1, n, [(0.0, 0.95), (0.16, 0.85), (0.22, 0.62), (0.22, 0.5)], "upper", 14 if fine else 6, caps=(False, False))
        m.disc(p + n * 0.05, n, 0.52, "black", 12 if fine else 6)
        down = (-Z + n * n.z).normalized()
        across = n.cross(down).normalized()
        crown = p + down * 1.55 + n * 0.32
        m.rod(p + n * 0.1, crown, 0.2, "steel", sides=4)
        m.rod(crown - across * 0.75, crown + across * 0.75, 0.24, "steel", sides=6)
        for s in (1, -1):
            m.rod(crown + across * s * 0.55, crown + across * s * 0.6 - down * 1.35 + n * 0.02, 0.24, "steel", rb=0.07, sides=4)
        yard.rust_at.append((p.x, p.y, p.z - 0.3, 5.0))
        weep(yard, stains, p.x - 0.4, p.z - 0.8, hand, 3.2, 0.2, rust=0.5)
        weep(yard, stains, p.x + 0.9, p.z - 1.9, hand, 2.0, 0.12, rust=0.4)
    # two warping capstans
    for hand in (PORT, STBD):
        x_, y_ = 107.6, hand * 3.0
        yard.on(x_).lathe((x_, y_, deck_at(x_, y_) - 0.05), Z, [(0.0, 0.7), (0.1, 0.7), (0.14, 0.42), (0.6, 0.38), (0.75, 0.48), (0.82, 0.55), (0.92, 0.5), (0.96, 0.3)],
                          "steel", 14 if fine else 6)
    if fine:
        from ships import bulwark
        m.use(yard.half(104.0))
        # a low breakwater across her forecastle, its apex forward, stayed from behind
        line = [(102.8, 6.6), (104.1, 3.3), (105.4, 0.0), (104.1, -3.3), (102.8, -6.6)]
        zb = D(104.0)
        bulwark(m, line, zb - 0.1, zb + 1.05, 0.06, "upper")
        for (bx_, by_) in ((103.6, 4.6), (104.4, 2.0), (104.4, -2.0), (103.6, -4.6)):
            m.rod((bx_ - 0.05, by_, zb + 1.0), (bx_ - 0.85, by_, deck_at(bx_ - 0.85, by_)), 0.04, "upper", sides=4)
        for (vx, vy) in ((116.6, 0.0), (101.2, 4.2), (101.2, -4.2)):
            yard.vent(vx, vy, deck_at(vx, vy) - 0.05, r=0.42, high=1.2)
        m.use(yard.half(109.6))
        zc = deck_at(109.6, 0.0)
        m.box((109.6, 0.0, zc + 0.18), (1.6, 1.2, 0.45), "upper")
        m.box((109.6, 0.0, zc + 0.43), (1.5, 1.1, 0.06), "deck")
        m.rod((108.88, -0.4, zc + 0.47), (108.88, 0.4, zc + 0.47), 0.04, "steel", sides=4)


# ---- the ship ------------------------------------------------------------------------------------------------------

def hangar_holes(x_lo, x_hi, skip_lo, skip_hi):
    """The openings in her hangars' sides along one length of them: (x from, x to, from, to over her old deck). The
    lower hangar a row of shuttered ports, every third one a wide loading door; the upper hangar by turns a big
    shuttered opening and a pair of small windows. None where something stands against the wall (`skip_lo`,
    `skip_hi`: lengths kept whole)."""
    holes = []
    k, xc = 0, 91.0
    while xc > -101.0:
        lower = (xc - 2.3, xc + 2.3, 1.6, 3.2) if k % 3 == 1 else (xc - 1.8, xc + 1.8, 2.0, 3.2)
        uppers = [(xc - 2.5, xc + 2.5, 6.8, 8.4)] if k % 2 == 0 else [(xc - 2.3, xc - 0.5, 7.1, 8.1), (xc + 0.5, xc + 2.3, 7.1, 8.1)]
        for hole, skips in [(lower, skip_lo)] + [(u, skip_hi) for u in uppers]:
            xa, xb = hole[0], hole[1]
            if x_lo + 0.6 < xa and xb < x_hi - 0.6 and not any(a < xb + 0.4 and xa - 0.4 < b for a, b in skips):
                holes.append(hole)
        xc -= 9.4
        k += 1
    return holes


def hangar_wall(yard, x0, x1, hand, w_at, D, zhi, holes, depth=0.42):
    """Her hangar's side from x0 to x1 on one hand: plating on a grid of points following her side, from just below
    her old upper deck (D) to the flight deck's underside (zhi), with real openings let into it, each a recess `depth`
    deep with its roller shutter drawn down at the back and a hood over it."""
    m = yard.m
    fine = m.fine
    m.use(yard.half((x0 + x1) * 0.5))
    edges = sorted(set([x0, x1] + [e for hole in holes for e in hole[:2]]))
    k = max(1, int(round((x1 - x0) / (2.3 if fine else 7.5))))
    cols = list(edges)
    for i in range(1, k):
        c = lerp(x0, x1, i / k)
        if all(abs(c - e) > 0.35 for e in edges):
            cols.append(c)
    cols.sort()
    top_min = min(zhi(c) - D(c) for c in cols)
    offs = sorted(set([-0.5, 5.2] + ([10.2] if fine else []) + [o for hole in holes for o in hole[2:]]))
    offs = [o for o in offs if o < top_min - 0.15]
    grid = [put(m, [Vector((c, hand * w_at(c), D(c) + o)) for o in offs] + [Vector((c, hand * w_at(c), zhi(c)))]) for c in cols]
    nr = len(offs) + 1
    quads = []
    for i in range(len(cols) - 1):
        xm = (cols[i] + cols[i + 1]) * 0.5
        for j in range(nr - 1):
            om = (offs[j] + (offs[j + 1] if j + 1 < len(offs) else offs[j] + 1.0)) * 0.5
            if any(xa < xm < xb and lo < om < hi for xa, xb, lo, hi in holes):
                continue
            quads.append((grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]))
    faces_on(m, quads, "hull", lambda q: Vector((0, hand, 0)))
    for xa, xb, lo, hi in holes:
        ia, ib, jl, jh = cols.index(xa), cols.index(xb), offs.index(lo), offs.index(hi)
        ring = [grid[i][jl] for i in range(ia, ib)] + [grid[ib][j] for j in range(jl, jh)] + [grid[i][jh] for i in range(ib, ia, -1)] + \
               [grid[ia][j] for j in range(jh, jl, -1)]
        nn = len(ring)
        inner = put(m, [m.verts[q] - Vector((0, hand * depth, 0)) for q in ring])
        xm, zm = (xa + xb) * 0.5, D((xa + xb) * 0.5) + (lo + hi) * 0.5
        faces_on(m, [(ring[k_], ring[(k_ + 1) % nn], inner[(k_ + 1) % nn], inner[k_]) for k_ in range(nn)], "seam",
                 lambda q, xm=xm, zm=zm: Vector((xm - q.x, 0.0, zm - q.z)), smooth_=False)
        back = put(m, [sum((m.verts[q] for q in inner), Vector()) / nn])[0]
        faces_on(m, [(inner[k_], inner[(k_ + 1) % nn], back) for k_ in range(nn)], "door", lambda q: Vector((0, hand, 0)), smooth_=False)
        if fine:
            zz = lo + 0.2
            while zz < hi - 0.1:                          # the roller shutter's slats
                pts = [Vector((x_, hand * (w_at(x_) - depth + 0.012), D(x_) + zz)) for x_ in (xa + 0.03, xm, xb - 0.03)]
                m.strip(pts, [Vector((0, hand, 0))] * 3, 0.016, "steel", 0.0)
                zz += 0.22
            slope = (w_at(xm + 0.5) - w_at(xm - 0.5)) * hand
            m.box((xm, hand * (w_at(xm) + 0.08), D(xm) + hi + 0.06), (xb - xa + 0.3, 0.16, 0.07), "hull", yaw=math.degrees(math.atan2(slope, 1.0)))


def akagi(fine=True):
    """Akagi in 1941-42."""
    paints = livery("JAP", "carrier")
    paints.update(AKAGI_PAINT)
    paints = fit.crew_paints(paints, "JAP")
    m = Model("Akagi", "Aircraft carrier Akagi, as rebuilt 1935-38, 1941-42 (our own model)", "ship", paints, fine)
    h = AkagiHull(AKAGI_PLAN, L=260.7, B=28.6, D=7.4, T=8.7, sheer=1.6, sheer_aft=0.2, rake=9.0, counter=3.4, flare=0.7, fine=1.6, full=0.72,
                  wl_fore=0.5, bilge=6.0)
    bx = -21.0
    yard = Yard(m, h, break_x=bx)
    yard.soot_at, yard.rust_at = [], []
    stains = {}
    yard.hull("deck", above=[0.1, 0.2, 0.31, 0.43, 0.6, 0.8, 1.0] if fine else [0.25, 0.5, 1.0], frames=150 if fine else None)
    D = h.deck_z
    from ships import railing

    def under(x):                                         # the flight deck's underside
        return fdz(x) - FD_THICK

    def zg_at(x):                                         # the galleries along her sides, just under the deck's edge
        return fdz(x) - 1.55

    def w_at(x):                                          # her hangar's side
        return h.half_at(x) * 0.985

    # ---- the ports of her six 20 cm casemates, cut into her plating low in her after hull (the guns come later)
    z20 = 5.0
    c20 = (-70.0, -82.0, -94.0)
    port_feet = cut_ports(yard, [(cx, z20 + 0.4, hand, 2.45, 1.0, 0.55) for cx in c20 for hand in (PORT, STBD)], len(m.faces))
    for (cx, hand), zf in zip([(cx, hand) for cx in c20 for hand in (PORT, STBD)], port_feet):
        weep(yard, stains, cx - 1.3, zf - 0.04, hand, 2.6, 0.17)
        weep(yard, stains, cx + 0.9, zf - 0.04, hand, 1.5, 0.11, rust=0.4)

    # ---- where things stand along her sides (the openings in the hangars' walls keep clear of them)
    xi, yi = -3.0, 16.6                                   # her island
    xf = -4.0                                             # her great funnel
    xa_f = -17.6                                          # her small funnel
    s12 = {PORT: (-14.0, -28.0, -42.0), STBD: (-26.0, -40.0, -54.0)}       # the 12 cm, all abaft the island and the funnels
    dirs = {PORT: -50.0, STBD: -64.0}                     # the high-angle directors
    ladders = {PORT: (-50.0, -60.0), STBD: (-62.0,)}      # the accommodation ladders, hoisted
    g25 = {PORT: [(86.0, 79.0, 72.0), (12.5, 6.5), (-94.0, -101.0)], STBD: [(86.0, 79.0, 72.0), (19.0, 12.5), (-94.0, -101.0)]}
    skip_lo = {hand: [(sx - 3.8, sx + 3.8) for sx in s12[hand]] + [(lx - 4.2, lx + 4.2) for lx in ladders[hand]] for hand in (PORT, STBD)}
    skip_hi = {hand: [(min(g) - 3.2, max(g) + 3.2) for g in g25[hand]] for hand in (PORT, STBD)}
    skip_lo[STBD].append((-12.5, 4.5))
    skip_hi[STBD] += [(-12.5, 4.5), (-20.5, -14.5)]
    skip_hi[PORT].append((-11.5, 5.0))

    # ---- the hangars' walls: her side carried up from her old upper deck to the flight deck, two hangars high, with
    # the openings in their sides; the walls across her at the hangars' ends and at her break
    hx0, hx1 = 97.0, -106.0
    for x0, x1, part in ((bx, hx0, "fore"), (hx1, bx, "aft")):
        m.use(part)
        for xe, way in ((x0, -X), (x1, X)):
            w = w_at(xe)
            m.add([Vector((xe, -w, D(xe) - 0.5)), Vector((xe, w, D(xe) - 0.5)), Vector((xe, w, under(xe))), Vector((xe, -w, under(xe)))], [(0, 1, 2, 3)],
                  "hull", False, lambda _, o=way: o)
        for hand in (PORT, STBD):
            holes = hangar_holes(x0, x1, skip_lo[hand], skip_hi[hand]) if fine else []
            hangar_wall(yard, x0, x1, hand, w_at, D, under, holes)
    if fine:
        x = hx0 - 6.0
        while x > hx1 + 5.0:
            for hand in (PORT, STBD):
                if hand == STBD and -13.0 < x < 5.0:
                    continue
                for dz in (4.6, 9.8):                                     # scuttles between the openings
                    for dx in (-3.2, 3.2):
                        yard.on(x + dx).disc((x + dx, hand * (w_at(x + dx) + 0.01), D(x + dx) + dz), (0, hand, 0), 0.17, "glass", 8)
                # an upright stiffener on the wall at each bay's end
                sx_ = x + 4.7
                if hx1 + 1.0 < sx_ < hx0 - 1.0 and abs(sx_ - bx) > 0.6 and not (hand == STBD and -13.0 < sx_ < 5.0):
                    slope = (w_at(sx_ + 0.5) - w_at(sx_ - 0.5)) * hand
                    yard.on(sx_).box((sx_, hand * (w_at(sx_) + 0.035), (D(sx_) + under(sx_)) * 0.5), (0.14, 0.07, under(sx_) - D(sx_) - 0.2), "hull",
                                     yaw=math.degrees(math.atan2(slope, 1.0)))
            x -= 9.4
        # the line of the upper hangar's deck along her side
        for hand in (PORT, STBD):
            for a, b in ((hx1 + 0.5, bx), (bx, hx0 - 0.5)):
                xs = [lerp(a, b, k_ / 12.0) for k_ in range(13)]
                m.use(yard.half((a + b) * 0.5))
                m.strip([Vector((x, hand * (w_at(x) + 0.01), D(x) + 5.2)) for x in xs], [Vector((0, hand, 0))] * 13, 0.05, "seam", 0.006)
    # strakes: a rubbing strake at the edge of her old upper deck, and one along the top of her bulges
    for hand in (PORT, STBD):
        for a, b in ((-124.0, bx), (bx, 124.0)):
            k_ = max(4, int((b - a) / (2.0 if fine else 8.0)))
            xs = [lerp(a, b, i / k_) for i in range(k_ + 1)]
            m.use(yard.half((a + b) * 0.5))
            tube(m, [Vector((x, hand * (h.half_at(x) + 0.02), D(x) - 0.1)) for x in xs], 0.12, "upper", 6 if fine else 4)
            if fine:
                lo_, hi_ = max(a, -100.0), min(b, 70.0)
                k_ = max(4, int((hi_ - lo_) / 2.0))
                pts = []
                for i in range(k_ + 1):
                    x = lerp(lo_, hi_, i / k_)
                    p, n = h.side(x, 0.36 * D(x), hand)
                    pts.append(p)
                tube(m, pts, 0.07, "upper", 6)

    # ---- the flight deck: planked, level amidships and falling to each end; its edge plating, underside and beams
    for x0, x1, part in ((bx, DECK_FORE, "fore"), (DECK_AFT, bx, "aft")):
        m.use(part)
        flight_deck(m, x0, x1, fine)
    # its pillars over the open forecastle and over the quarterdeck
    for px, spread in ((116.0, 5.2), (108.5, 6.2), (100.5, 7.0)):
        for hand in (PORT, STBD):
            yy = hand * min(h.half_at(px) * 0.7, spread)
            yard.on(px).rod((px, yy, D(px)), (px, yy * 1.18, under(px) + 0.05), 0.4, "upper", rb=0.32, sides=10 if fine else 5)
            if fine:
                yard.on(px).rod((px, yy * 1.02, D(px)), (px - 3.2, hand * min(h.half_at(px - 3.2) * 0.98, 13.0), under(px - 3.2) + 0.05), 0.16, "upper", sides=6)
    for px in (-112.5, -119.0, -125.5):
        for hand in (PORT, STBD):
            yy = hand * min(h.half_at(px) * 0.55, 6.0)
            yard.on(px).rod((px, yy, D(px)), (px, yy * 1.12, under(px) + 0.05), 0.36, "upper", rb=0.3, sides=10 if fine else 5)
    # the stern's tall open bays: posts up her sides, a lintel under the deck's edge, a bulwark below
    for hand in (PORT, STBD):
        for px in (hx1, -112.5, -119.0, -125.5):
            w = h.half_at(px) * 0.97
            yard.on(px).box((px, hand * w, (D(px) + under(px)) * 0.5), (0.9, 0.5, under(px) - D(px)), "hull")
        xs = [lerp(hx1, -126.5, k / 8.0) for k in range(9)]
        m.use("aft")
        lint = [Vector((x, hand * h.half_at(x) * 0.97, 0)) for x in xs]
        for i in range(len(lint) - 1):
            a, b = lint[i], lint[i + 1]
            q = [Vector((a.x, a.y, under(a.x) - 1.6)), Vector((b.x, b.y, under(b.x) - 1.6)), Vector((b.x, b.y, under(b.x))), Vector((a.x, a.y, under(a.x)))]
            m.add(q, [(0, 1, 2, 3)], "hull", False, lambda _, s=hand: Vector((0, s, 0)))
            q = [Vector((a.x, a.y, D(a.x))), Vector((b.x, b.y, D(b.x))), Vector((b.x, b.y, D(b.x) + 1.1)), Vector((a.x, a.y, D(a.x) + 1.1))]
            m.add(q, [(0, 1, 2, 3)], "hull", False, lambda _, s=hand: Vector((0, s, 0)))
    # the after wall of the hangar facing the quarterdeck, with its doors
    m.use("aft")
    if fine:
        for yy in (-6.0, 0.0, 6.0):
            m.box((hx1 - 0.03, yy, D(hx1) + 1.0), (0.06, 1.0, 1.9), "door")

    # ---- galleries along her sides under the deck's edge, on knees, railed; where sponsons stand they give way
    stops = {PORT: [(-8.8, 3.6)], STBD: [(-15.0, 6.5)]}
    lifts = [(72.0, -1.2, 16.0, 11.8), (20.0, 0.0, 11.8, 13.0), (-82.0, 0.0, 12.8, 8.4)]
    for hand in (PORT, STBD):
        for grp in g25[hand]:
            stops[hand].append((min(grp) - 2.2, max(grp) + 2.2))
        stops[hand].append((dirs[hand] - 1.6, dirs[hand] + 1.6))
    for hand in (PORT, STBD):
        segs, a = [], hx1 + 1.0
        for s0, s1 in sorted(stops[hand]):
            segs.append((a, s0))
            a = s1
        segs.append((a, hx0 - 2.0))
        cut = []
        for a, b in segs:                                 # (at the break too)
            if a < bx < b:
                cut += [(a, bx), (bx, b)]
            else:
                cut.append((a, b))
        for a, b in cut:
            if b - a < 1.0:
                continue
            k_ = max(2, int((b - a) / 4.0))
            xs = [lerp(a, b, i / k_) for i in range(k_ + 1)]
            m.use(yard.half((a + b) * 0.5))
            strip = [Vector((x, hand * (w_at(x) + 0.68), zg_at(x))) for x in xs]
            m.strip(strip, [Z] * len(strip), 0.7, "deck", 0.0)
            m.strip([p - Z * 0.1 for p in strip], [-Z] * len(strip), 0.7, "upper", 0.0)
            if fine:
                railing(m, [Vector((x, hand * (w_at(x) + 1.33), zg_at(x))) for x in xs], high=1.0, every=2.2)
                for x in xs[::2]:
                    m.rod((x, hand * w_at(x), zg_at(x) - 1.5), (x, hand * (w_at(x) + 1.2), zg_at(x) - 0.1), 0.06, "upper", sides=4)
    if fine:                                              # knees under the deck's overhang
        x = 94.0
        while x > -104.0:
            for hand in (PORT, STBD):
                e = abs(cap.edge_at(AKAGI_PORT, x))
                w0 = w_at(x)
                if e - w0 > 0.4 and not (hand == STBD and -16.0 < x < 7.0) and not (hand == PORT and -10.0 < x < 4.0) and \
                        not any(min(g) - 3.0 < x < max(g) + 3.0 for g in g25[hand]):
                    yard.on(x).rod((x, hand * w0, under(x) - 1.9), (x, hand * (e - 0.25), under(x) - 0.04), 0.09, "upper", sides=4)
            x -= 6.5

    # ---- the lifts' places on the deck, the lines, wires, barriers, wind screens, the round-down's stripes, her kana
    iz = FD_TOP
    deck_marks(yard, lifts, fine)

    # ---- the island to port amidships; the great funnel to starboard abreast it, and the small one abaft it
    gaff, mast_head = island(yard, xi, yi, fdz(xi), zg_at(xi), crew=fine)
    main_funnel(yard, xf, -(w_at(xf) - 0.2), 15.3, rx=4.5, rz=3.4, out=2.4, bend=5.2, down=32.0, run=3.0, sweep=8.0, casing=(6.4, 4.0))
    ya = -(15.25 + 1.75)
    m.use(yard.half(xa_f))
    m.box((xa_f, (ya - w_at(xa_f)) * 0.5 - 0.3, 15.4), (3.6, abs(ya) - w_at(xa_f) + 0.6, 2.4), "upper")      # its uptake out from her side
    for dx in (-1.2, 1.2):
        m.rod((xa_f + dx, -w_at(xa_f), 12.4), (xa_f + dx, ya + 0.6, 14.2), 0.14, "upper", sides=5)
    yard.funnel(xa_f, 14.2, fdz(xa_f) + 1.7 - 14.2, 3.4, 2.8, lean=0.0, cap=0.06, y=ya, pipes=True)

    # ---- six 20 cm in casemates low in her after hull, three a side, in the ports cut for them
    n_p = n_s = 0
    for cx in c20:
        for hand in (PORT, STBD):
            if hand == PORT:
                n_p += 1
                nm = f"sec.P{n_p}"
            else:
                n_s += 1
                nm = f"sec.S{n_s}"
            casemate20(yard, nm, cx, z20, hand)

    # ---- six twin 12 cm high-angle on sponsons below the deck's edge, three a side, all abaft the island and the
    # funnels (pointing aft at rest, training over the beam and the bow quarter); their directors
    z12 = 13.3
    for hand in (PORT, STBD):
        for k_, sx in enumerate(s12[hand]):
            w0 = w_at(sx)
            yy = hand * (w0 + 3.0)
            gun_sponson(yard, sx, yy, z12, 3.25, hand * w0)
            nm = f"sec.{'P' if hand == PORT else 'S'}{k_ + 4}"
            lim = (-20, 165) if hand == PORT else (-165, 20)
            twin12(yard, nm, sx, yy, z12, 180.0, lim, crew=fine)
            if fine:                                      # her captain of the mount, and a ready-use locker by the rail
                fit.sailor(m.use(yard.half(sx)), Vector((sx + 2.4, yy - hand * 0.4, z12)), Vector((0, hand, 0)), "stand", navy="JAP")
                a = math.radians(135.0 * hand)
                m.box((sx + math.cos(a) * 2.72, yy + math.sin(a) * 2.72, z12 + 0.45), (0.9, 0.55, 0.9), "upper", yaw=math.degrees(a) + 90.0)
    for hand in (PORT, STBD):
        dx = dirs[hand]
        w0 = w_at(dx)
        yy = hand * (w0 + 1.9)
        m.use(yard.half(dx))
        m.lathe((dx, yy, zg_at(dx) - 0.3), Z, [(0.0, 1.9), (0.3, 1.9)], "upper", 16 if fine else 8)
        brackets(m, dx - 0.8, hand * w0, yy, zg_at(dx) - 0.3)
        brackets(m, dx + 0.8, hand * w0, yy, zg_at(dx) - 0.3)
        director94(yard, "director.port" if hand == PORT else "director.starboard", dx, yy, zg_at(dx), bearing=hand * -90.0)
        if fine:
            m.use(yard.half(dx))
            railing_arc(m, Vector((dx, yy, 0)), 1.85, (0.15 if hand == PORT else -0.15) * math.pi - math.pi * 0.5 * (1 if hand == STBD else -1),
                        (0.15 if hand == PORT else -0.15) * math.pi + math.pi * 0.5 * (1 if hand == PORT else -1) + (math.pi if hand == STBD else 0.0), zg_at(dx))

    # ---- fourteen twin 25 mm in groups on sponsons along her galleries
    q = 0
    for hand in (PORT, STBD):
        for grp in g25[hand]:
            xm = sum(grp) / len(grp)
            w0 = min(w_at(x) for x in grp)
            e0 = max(abs(cap.edge_at(AKAGI_PORT, x)) for x in (max(grp) + 2.0, min(grp) - 2.0) + tuple(grp))
            # (out past the deck's edge, so that they clear it shooting up, and low enough that the men inboard of
            # them stand clear under it)
            z25 = min(fdz(x) for x in grp) - 2.75
            yy = hand * (e0 + 1.45)
            out_ = e0 + 3.1
            plan = Model.round_rect(max(grp) + 2.2, min(grp) - 2.2, min(hand * (w0 - 0.3), hand * out_), max(hand * (w0 - 0.3), hand * out_), 1.6, 3 if fine else 1)
            m.use(yard.half(xm))
            cap.slab(m, plan, z25 - 0.3, z25, paint="upper", top="deck", bottom=True)
            for gx in (max(grp) + 1.2, min(grp) - 1.2) + tuple(grp):
                brackets(m, gx, hand * w0, hand * (out_ - 0.5), z25 - 0.3, drop=3.0)
            if fine:
                m.add([Vector((max(grp) + 1.6, hand * (out_ - 0.35), z25 - 0.3)), Vector((min(grp) - 1.6, hand * (out_ - 0.35), z25 - 0.3)),
                       Vector((min(grp) - 1.6, hand * w0, z25 - 2.4)), Vector((max(grp) + 1.6, hand * w0, z25 - 2.4))], [(0, 1, 2, 3)], "upper", False,
                      lambda _, s=hand: Vector((0, s * 0.6, -0.8)))          # (a plate under it, slanting in to her side)
                pl = [p for p in plan if hand * p[1] > w0 + 0.2]
                railing(m, [Vector((px, py, z25)) for px, py in sorted(pl, key=lambda p: -p[0])], every=1.3)
            for gx in grp:
                q += 1
                face = hand * -90.0 + (-25.0 if gx > 0 else 25.0) * (-hand)
                twin25(yard, q, gx, yy, z25, bearing=face, crew=fine)
                if fine:
                    fit.sailor(m.use(yard.half(gx)), Vector((gx + 1.5, hand * (e0 + 0.15), z25)), Vector((0, hand, 0)), "stand", navy="JAP")
            if fine:                                       # the group's director (Type 95): a small sight on a post
                gx = min(grp) - 1.4
                m.use(yard.half(gx))
                m.rod((gx, hand * (e0 + 2.1), z25), (gx, hand * (e0 + 2.1), z25 + 1.2), 0.08, "upper", sides=6)
                m.lathe((gx - 0.6, hand * (e0 + 2.1), z25 + 1.35), X, [(0.0, 0.12), (1.2, 0.12)], "gunmetal", 8)
                fit.sailor(m, Vector((gx - 0.1, hand * (e0 + 1.3), z25)), Vector((0, hand, 0.0)), "aim",
                           hands=(Vector((gx - 0.3, hand * (e0 + 1.95), z25 + 1.3)), Vector((gx + 0.2, hand * (e0 + 1.95), z25 + 1.3))), navy="JAP", lean=0.05)

    # ---- searchlights on platforms at the galleries' forward ends
    for hand in (PORT, STBD):
        sx = 58.0
        w0 = w_at(sx)
        yy = hand * (w0 + 2.0)
        m.use(yard.half(sx))
        m.lathe((sx, yy, zg_at(sx) - 0.25), Z, [(0.0, 1.5), (0.25, 1.5)], "upper", 14 if fine else 6)
        brackets(m, sx, hand * w0, yy, zg_at(sx) - 0.25)
        yard.searchlight(sx, yy, zg_at(sx), 0.6, (0.6, hand * 0.8, 0.1))

    # ---- radio masts each side, lowered outboard for flying, hinged at the galleries
    for hand, xs in ((PORT, (34.0, 26.0, -58.0, -70.0)), (STBD, (62.0, 52.0, -58.0, -70.0))):
        for mxx in xs:
            foot = Vector((mxx, hand * (w_at(mxx) + 1.35), zg_at(mxx) + 0.3))
            tip = foot + Vector((0.0, hand * 12.5, 1.0))
            m_ = yard.on(mxx)
            m_.box(foot - Vector((0, hand * 0.2, 0.15)), (0.9, 0.5, 0.5), "upper")
            for dx in (-0.32, 0.32):
                m_.rod(foot + X * dx, tip + X * dx * 0.25, 0.07, "upper", sides=4)
            m_.rod(tip + Vector((0, 0, -0.1)) - X * 1.4, tip + Vector((0, 0, -0.1)) + X * 1.4, 0.05, "upper", sides=4)
            if fine:
                for k_ in range(1, 12):
                    a = foot.lerp(tip, k_ / 12.0)
                    m_.rod(a - X * 0.3 * lerp(1.0, 0.25, k_ / 12.0), a + X * 0.3 * lerp(1.0, 0.25, k_ / 12.0), 0.022, "upper", sides=3, smooth=False)

    # ---- her flight deck's crew: handlers before the range and beside it, officers and runners by the island
    if fine:
        rg = random.Random(1941)
        for cx, cy, k_ in ((-9.0, 4.0, 4), (34.0, 7.0, 3), (2.0, 10.5, 4), (88.0, -6.0, 3), (-8.0, -10.5, 2)):
            for i in range(k_):
                a = rg.uniform(0, 2 * math.pi)
                foot = Vector((cx + rg.uniform(-4.0, 4.0), cy + rg.uniform(-3.0, 3.0), 0.0))
                if any(abs(foot.x - lx) < ll * 0.5 + 0.8 and abs(foot.y - ly) < lw * 0.5 + 0.8 for lx, ly, ll, lw in lifts):
                    continue
                foot.z = fdz(foot.x) + 0.01
                fit.sailor(m.use(yard.half(foot.x)), foot, Vector((math.cos(a), math.sin(a), 0)), "stand", navy="JAP")
        for (fx_, fy_) in ((-31.0, 14.2), (-45.0, -14.2), (-59.0, 14.1), (-68.0, -14.1)):     # (the wingtips of the range stand 13 m out)
            fit.sailor(m.use(yard.half(fx_)), Vector((fx_, fy_, fdz(fx_) + 0.01)), Vector((0.3, -math.copysign(1.0, fy_), 0)), "stand", navy="JAP")

    # ---- forecastle and quarterdeck: anchor gear, bollards, rails; boats; scuttles; staffs
    forecastle(yard, stains)
    for bx_ in (124.0, 117.0, 104.0, -116.0, -124.0):
        for hand in (PORT, STBD):
            yard.bollards(bx_, hand * (h.half_at(bx_) - 0.9), D(bx_))
    yard.rails(hx0 + 1.0, h.L * 0.488)
    if fine:
        for hand in (PORT, STBD):                          # the quarterdeck's boats on their chocks
            for bxx, yy, long_ in ((-112.3, 3.0, 11.5), (-123.4, 3.0, 8.4), (-117.0, 8.4, 9.0)):    # (clear of the deck's pillars)
                yard.boat(bxx, hand * yy, D(bxx) + 0.55, long_)
                for t in (-0.3, 0.3):
                    m.box((bxx + t * long_, hand * yy, D(bxx) + 0.28), (0.4, long_ * 0.22, 0.56), "upper")
    if fine:
        for hand in (PORT, STBD):
            # boat booms swung in and stowed along her side above the bulge, on their goosenecks
            for b0, b1 in ((30.0, 18.0), (-40.0, -52.0)):
                p0, _ = h.side(b0, 4.6, hand)
                p1, _ = h.side(b1, 4.6, hand)
                m.use(yard.half(b0))
                m.rod(p0 + Vector((0, hand * 0.25, 0)), p1 + Vector((0, hand * 0.25, 0)), 0.11, "upper", rb=0.08, sides=6)
                m.box(p0 + Vector((0.2, hand * 0.12, 0)), (0.5, 0.3, 0.5), "upper")
                m.box(p1 + Vector((-0.3, hand * 0.1, 0)), (0.25, 0.25, 0.25), "upper")
            # the accommodation ladders hoisted flat against the hangar's side abaft the guns
            for lx in ladders[hand]:
                c = Vector((lx, hand * (w_at(lx) + 0.16), 9.6))
                m.use(yard.half(lx))
                m.box(c, (7.5, 0.24, 0.9), "upper", pitch=-12.0)
                for t in (-3.2, 3.2):
                    m.rod(c + Vector((t, 0, -t * math.tan(math.radians(12.0)))), c + Vector((t, hand * 0.3, -t * math.tan(math.radians(12.0)))), 0.05, "steel", sides=4)
    yard.portholes(-h.L * 0.42, h.L * 0.43, 1.7, every=3.4, skip=[(-99.0, -65.0)])
    yard.portholes(-h.L * 0.4, h.L * 0.41, 3.9, every=3.4, skip=[(-99.0, -65.0)])
    m.use("aft")
    sx = -h.L * 0.497
    m.rod((sx + 0.8, 0, D(sx + 0.8)), (sx - 0.3, 0, D(sx + 0.8) + 5.0), 0.07, "upper", sides=4)
    m.use("fore")
    jx = h.L * 0.493
    m.rod((jx, 0, D(jx)), (jx + 0.4, 0, D(jx) + 3.0), 0.06, "upper", sides=4)
    m.socket("jack", (jx + 0.4, 0, D(jx) + 3.0), (1, 0, 0), "fore")
    yard.screws_and_rudder(screws=4, r=2.2, rudders=1)

    hz = round(iz - (D(0.0) + 2.4), 2)                    # (a lift goes down to her lower hangar)
    finish(yard, lifts, hz, gaff)
    yard.fire_points += [(60.0, 0.0, fdz(60.0)), (-20.0, 0.0, iz), (-70.0, 0.0, iz), (xi, yi, iz + 3.0), (xf, -14.0, 16.0), (20.0, 0.0, iz)]
    from ships import sockets_common
    sockets_common(yard)
    m.weather = akagi_weather(yard, stains)
    cap.carrier_clips(m, yard, len(lifts), hz)
    m.info.setdefault("file", {}).update({"class": "Akagi", "navy": "JAP", "year": 1941, "stands_for": "Carrier_JAP", "beam": 31.3,
                                          "paint": "IJN grey, sooted along her starboard side abaft the funnel; a wooden flight deck with white lines, red and white stripes on its after end and her kana 'a' (katakana) in white"})
    prune(m)
    return m, yard


def flight_deck(m, x0, x1, fine):
    """The piece of her flight deck between x0 and x1 (aft to fore): its planked top falling with the deck's line, its
    edge plating, its underside."""
    marks = [x for x, _ in AKAGI_PORT if x0 < x < x1] + [k for k in LEVEL if x0 < k < x1]
    xs = sorted(set([x0, x1] + marks))
    fx = []
    for a, b in zip(xs, xs[1:]):
        k = max(1, int((b - a) / (4.0 if fine else 12.0)))
        fx += [lerp(a, b, i / k) for i in range(k)]
    fx.append(xs[-1])
    across = 5 if fine else 3
    top, bot = [], []
    for x in fx:
        yp, ys = cap.edge_at(AKAGI_PORT, x), cap.edge_at(AKAGI_STAR, x)
        top.append([Vector((x, lerp(yp, ys, j / (across - 1)), fdz(x))) for j in range(across)])
        bot.append([p - Z * FD_THICK for p in top[-1]])
    m.sheet(top, "planks", Z, smooth=False)
    m.sheet(bot, "upper", -Z, smooth=False)
    for j, hand in ((0, PORT), (across - 1, STBD)):          # its edges
        rows = [[top[i][j], bot[i][j]] for i in range(len(fx))]
        m.add([p for r in rows for p in r], [(2 * i, 2 * i + 1, 2 * i + 3, 2 * i + 2) for i in range(len(rows) - 1)], "upper", False,
              lambda q, s=hand: Vector((0.0, s, 0.0)))
    for i, way in ((0, -X), (len(fx) - 1, X)):                # its ends
        r = top[i] + bot[i]
        n = across
        m.add(r, [(j, j + 1, n + j + 1, n + j) for j in range(n - 1)], "upper", False, lambda q, w=way: w)
    if fine:
        # the beams under it, across her every 6 m, and a strip of edge plating round its lip
        x = math.ceil(x0 / 6.0) * 6.0
        while x < x1 - 0.5:
            yp, ys = cap.edge_at(AKAGI_PORT, x), cap.edge_at(AKAGI_STAR, x)
            m.box((x, (yp + ys) * 0.5, fdz(x) - FD_THICK - 0.25), (0.25, (yp - ys) - 0.6, 0.5), "upper")
            x += 6.0


def deck_marks(yard, lifts, fine):
    """The flight deck's markings and gear: the lifts' places, the white centre line and dashed lines near each edge,
    the wind-direction lines at her fore end, the arresting wires on their sheaves, the crash barriers and the wind
    screens lying flat, the red and white stripes on her round-down, and her kana."""
    m = yard.m
    lift_gap = [(lx - ll * 0.5 - 1.0, lx + ll * 0.5 + 1.0) for lx, ly, ll, lw in lifts]

    def along(a, b, y, half, paint, lift=0.025):
        """A painted line along the deck between x=a and x=b at y, laid on the deck's fall, split at the break."""
        for p0, p1 in ((a, min(b, yard.break_x)), (max(a, yard.break_x), b)):
            if p1 - p0 < 0.2:
                continue
            k = max(1, int((p1 - p0) / 6.0))
            pts = [Vector((lerp(p0, p1, i / k), y, 0)) for i in range(k + 1)]
            for p in pts:
                p.z = fdz(p.x)
            m.use(yard.half((p0 + p1) * 0.5))
            m.strip(pts, [Z] * len(pts), half, paint, lift)

    # the centre line: a broad white line from her fore end to her round-down, broken at the lifts
    pieces, last = [], -105.0
    for g0, g1 in sorted(lift_gap):
        pieces.append((last, g0))
        last = g1
    pieces.append((last, DECK_FORE - 0.8))
    for a, b in pieces:
        if b - a > 0.5:
            along(a, b, 0.0, 0.28, "line")
    # the dashed lines near each edge
    for hand in (PORT, STBD):
        x = DECK_FORE - 1.5
        while x > -112.0:
            e = abs(cap.edge_at(AKAGI_PORT, x))
            if not any(g0 - 1.0 < x < g1 + 1.0 for g0, g1 in lift_gap):
                along(x - 2.0, x, hand * (e - 2.2), 0.22, "line")
            x -= 4.5
    # the wind-direction lines fanning aft from a point on the centre line by her fore end
    apex = DECK_FORE - 3.0
    m.use("fore")
    for ang, long in ((14.0, 9.0), (-14.0, 9.0), (34.0, 11.0), (-34.0, 11.0)):
        a = math.radians(ang)
        pts = [Vector((apex - math.cos(a) * long * t, math.sin(a) * long * t, 0)) for t in (0.08, 0.5, 1.0)]
        for p in pts:
            p.z = fdz(p.x)
        m.strip(pts, [Z] * 3, 0.14, "line", 0.026)
    # the lifts' places: dark wells under the platforms (the platforms themselves are parts: see `finish`)
    # the arresting wires across her after deck, on their sheaves at each end
    for xw in (-36.0, -42.0, -56.0, -62.0, -68.0, -74.0, -94.0, -100.0, -106.0):
        m.use(yard.half(xw))
        w_ = abs(cap.edge_at(AKAGI_PORT, xw)) - 2.0
        z = fdz(xw)
        m.rod((xw, -w_, z + 0.12), (xw, w_, z + 0.12), 0.035, "steel", sides=3, smooth=False)
        for hand in (PORT, STBD):
            m.lathe((xw, hand * w_, z), Z, [(0.0, 0.24), (0.18, 0.2)], "steel", 8 if fine else 4)
    # the crash barriers forward of the landing area, lowered, their posts at the deck's edges
    for xb in (30.0, 36.0):
        m.use(yard.half(xb))
        z = fdz(xb)
        m.rod((xb, -12.2, z + 0.16), (xb, 12.2, z + 0.16), 0.05, "steel", sides=4, smooth=False)
        for hand in (PORT, STBD):
            m.box((xb, hand * 12.7, z + 0.25), (0.45, 0.45, 0.5), "steel")
    # the wind screens, folded flat across the deck: one just before the aircraft ranged aft, one abaft the midships lift
    if fine:
        for xs_ in (-16.0, 8.0):
            m.use(yard.half(xs_))
            z = fdz(xs_)
            for j in range(9):
                m.box((xs_ - 1.6 + j * 0.4, 0.0, z + 0.03), (0.16, 26.0, 0.05), "steel")
            for hand in (PORT, STBD):
                m.box((xs_, hand * 13.3, z + 0.08), (3.8, 0.3, 0.16), "steel")
    # the red and white stripes on her round-down, out toward the deck's edges (none where the rounded corner would
    # leave only a sliver of it)
    m.use("aft")
    aft_edge = [(y, x) for x, y in AKAGI_PORT[:6]]
    xb = DECK_AFT + 8.0
    full = xb - (cap.edge_at(aft_edge, 1.15) + 0.25)
    for k in range(13):
        yy = (k - 6) * 2.3
        xa = cap.edge_at(aft_edge, abs(yy) + 1.15) + 0.25
        if xb - xa > 0.6 * full:
            pts = [Vector((lerp(xa, xb, t), yy, 0)) for t in (0.0, 0.5, 1.0)]
            for p in pts:
                p.z = fdz(p.x)
            m.strip(pts, [Z] * 3, 1.15 if k % 2 == 0 else 1.14, "red" if k % 2 == 0 else "white", 0.022)
    # her kana, a white katakana 'a', big on her after deck, upright for a pilot coming in from astern
    kana(m, -112.0, 0.0, 9.0)


def kana(m, xc, yc, size):
    """The katakana 'a' (ア) laid on the deck in white strokes: the first a bar across and its hook curving down to
    the left from its right end, the second down from just under the bar's middle, sweeping out to the left at its
    foot. A pilot coming in from astern reads it upright: its top toward her bow, its right to starboard."""
    def P(u, v):                                          # (u across the character, to the pilot's right; v up it, forward)
        x = xc + (v - 0.5) * size
        y = yc - (u - 0.5) * size
        return Vector((x, y, fdz(x)))

    def stroke(uv, w):
        pts = [P(u, v) for u, v in uv]
        m.strip(pts, [Z] * len(pts), w * size * 0.5, "white", 0.03)
    w = 0.1
    stroke([(0.1, 0.86), (0.5, 0.865), (0.84, 0.87)], w)
    stroke([(0.84, 0.87), (0.815, 0.8), (0.775, 0.73), (0.715, 0.66), (0.64, 0.6), (0.56, 0.555)], w)
    stroke([(0.47, 0.77), (0.47, 0.62), (0.455, 0.47), (0.41, 0.33), (0.335, 0.21), (0.235, 0.115), (0.13, 0.055)], w)
    # (the first stroke's turn: a small square at the corner so the bar and the hook meet without a notch)
    c = P(0.835, 0.868)
    m.add([c + Vector((-w * size * 0.5, -w * size * 0.5, 0.031)), c + Vector((w * size * 0.5, -w * size * 0.5, 0.031)),
           c + Vector((w * size * 0.5, w * size * 0.5, 0.031)), c + Vector((-w * size * 0.5, w * size * 0.5, 0.031))], [(0, 1, 2, 3)], "white", False, lambda q: Z)


def gun_sponson(yard, x, y, z, r, wall_y, paint="upper"):
    """A round platform out from her side for a 12 cm mounting: a deep plate under it on knees with lightening holes,
    a rail round its edge."""
    m = yard.on(x)
    fine = m.fine
    m.lathe((x, y, z - 0.3), Z, [(0.0, r), (0.3, r)], paint, 24 if fine else 8)
    m.disc((x, y, z - 0.3), -Z, r, paint, 24 if fine else 8)
    hand = 1.0 if y >= wall_y else -1.0
    for dx in (-r * 0.65, 0.0, r * 0.65):
        m.rod((x + dx, wall_y, z - 2.6), (x + dx, y + hand * r * 0.35, z - 0.3), 0.1, paint, sides=4)
    if fine:
        m.box((x, (wall_y + y) * 0.5 + hand * 0.3, z - 1.0), (0.1, abs(y - wall_y) + 0.6, 1.4), paint)
        for k in range(3):
            m.disc((x + 0.06, wall_y + hand * (0.9 + k * 0.9), z - 0.95), X, 0.3, "black", 10)
            m.disc((x - 0.06, wall_y + hand * (0.9 + k * 0.9), z - 0.95), -X, 0.3, "black", 10)
        a0 = math.atan2(0.0, hand)                         # (round its outer side, from the wall to the wall)
        railing_arc(m, Vector((x, y, 0)), r - 0.08, a0 - math.radians(110), a0 + math.radians(110), z, every=1.2)


def finish(yard, lifts, hz, gaff):
    """What every carrier's file has, laid on her falling deck: the lifts as parts with their sockets (each platform's
    top follows the deck's fall where it stands), the deck park's part, the launch and landing sockets, the ensign."""
    m = yard.m
    for i, (lx, ly, ll, lw) in enumerate(lifts):
        top = fdz(lx)
        ln = m.part(f"lift.{i + 1}", yard.half(lx), (lx, ly, top), slide=(0, 0, -1), slide_limits=[0.0, hz], kind="slide",
                    note="an aircraft lift: slides down to the hangar deck and back; the dark well under it is part of the deck")
        m.use(yard.half(lx))
        plan = Model.round_rect(lx + ll * 0.5, lx - ll * 0.5, ly - lw * 0.5, ly + lw * 0.5, 0.5, 2)
        top_of(m, plan, lambda px, py: fdz(px) + 0.012, "black")
        m.use(ln)
        walls(m, plan, lambda px: fdz(px) - 0.3, lambda px: fdz(px) + 0.035, paint="upper")
        top_of(m, plan, lambda px, py: fdz(px) + 0.035, "planks")
        top_of(m, plan, lambda px, py: fdz(px) - 0.3, "upper", down=True)
        if m.fine:
            for dx in (-0.5, 0.5):
                m.box((lx + dx * (ll - 0.2), ly, fdz(lx + dx * (ll - 0.2)) + 0.045), (0.14, lw - 0.6, 0.02), "seam")
            for dy in (-0.5, 0.5):
                pts = [Vector((lx + t * (ll - 0.6), ly + dy * (lw - 0.2), 0)) for t in (-0.5, 0.0, 0.5)]
                for p in pts:
                    p.z = fdz(p.x) + 0.036
                m.strip(pts, [Z] * 3, 0.07, "seam", 0.01)
            if i == len(lifts) - 1:                       # the after lift's four lugs, out at its sides
                for dx in (-0.3, 0.3):
                    for s in (1, -1):
                        m.box((lx + dx * ll, ly + s * (lw * 0.5 + 0.35), fdz(lx + dx * ll) - 0.12), (1.0, 0.7, 0.32), "upper")
        m.socket(f"lift.{i + 1}", (lx, ly, top + 0.1), (0, 0, 1), ln, "an aircraft comes up here")
    # (the range of eleven stands from here about 50 m aft, to about x -74: clear of the after lift, which begins at
    # -75.6, so that the lifts may work with the range on deck)
    m.part("deckpark", "aft", (-24.0, 0, FD_TOP), kind="fixed",
           note="where her aircraft are ranged: the range named in `deck_park` stands at this pivot (its x forward) and rides with this part; "
                "it ends forward of the after lift, which may work with the range on deck")
    m.info.setdefault("file", {})["deck_park"] = [{"model": "DeckPark_Zero"}]
    m.socket("launch", (110.0, 0, fdz(110.0) + 1.5), (1, 0, 0), "fore", "an aircraft leaves the deck here, climbing ahead")
    m.socket("land", (-112.0, 0, fdz(-112.0) + 2.5), (-1, 0, 0.1), "aft", "an aircraft comes in over the stern here")
    m.socket("ensign", tuple(gaff), (-1, 0, 0), yard.half(gaff.x), "the battle ensign flies from the island's gaff")


BUILDERS = {"Akagi": lambda fine: akagi(fine)}
