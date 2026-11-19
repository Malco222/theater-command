"""Running gear: road wheels, sprockets, idlers, return rollers, and a track laid link by link round them.

Everything that rolls is a part of its own (kind `spin`, with its radius in the file's `running` table, so the game
turns it by distance / radius). The track's links are parts too: the clip `tracks.left` (and `.right`) carries every
link on by one link, so played in a loop the track runs; play it backward and it runs astern; one side each way and she
turns on the spot. All of it hangs from the root, not from the hull: the wheels stay on the ground while the hull rocks.
"""

import math

import bpy  # noqa: F401
import bmesh
from mathutils import Quaternion, Vector

from motorpool import LEFT, RIGHT, SIDES, X, Y, Z, Belt, bm_box, bm_extrude, bm_move, frame


class Gear:
    # ---- wheels ------------------------------------------------------------------------------------
    def wheel_disc(self, c, out, r, w, tyre=0.03, kind="dish", hub=0.34, bolts=6, face=1.0, sides=36, holes=0, rim_paint="upper", simple=False,
                   dish=0.45, cap=0.5, tyre_paint="rubber", ribs=0):
        """One disc of a wheel at centre c, its outer face toward `out`. kind: dish (a pressed dished disc), flat,
        spoke (open spokes, see `spokes`), steel (no tyre: a steel rim). `face` -1 turns the dish to face inward."""
        m = self
        sides = self.n(sides, 10)
        c = Vector(c)
        out = Vector(out).normalized()
        h = w / 2.0
        rr = r - tyre                                   # the rim under the tyre
        rh = r * hub
        ch = min(0.012, tyre * 0.4)
        plain = simple or not self.fine
        if tyre > 1e-4:
            if plain:
                m.lathe(c, out, [(h, rr), (h, r), (-h, r), (-h, rr)], tyre_paint, sides)
            else:
                m.lathe(c, out, [(h, rr), (h, r - ch), (h - ch, r), (-h + ch, r), (-h, r - ch), (-h, rr)], tyre_paint, sides)
        else:
            rr = r
        f = face
        if plain:
            m.lathe(c, out, [(h * 0.9, 0.0), (h * 0.9, rr), (-h * 0.9, rr), (-h * 0.9, 0.0)] if f > 0 else [(h * 0.9, rr * 0.5), (h * 0.9, rr), (-h * 0.9, rr), (-h * 0.9, 0.0)], rim_paint, sides)
            return
        lip = min(0.018, rr * 0.08)
        d_dish = h * (1.0 - 2.0 * dish) * f               # how far the web sits in from the rim's outer edge
        web = min(0.02, w * 0.3)
        capd = h * 0.9 + w * cap * 0.6
        dw = d_dish + web * 0.5
        prof = [(capd, 0.0), (capd, rh * 0.45), (h * 0.9 if f > 0 else dw + 0.01, rh * 0.62), (dw, rh)]
        if kind == "dish":
            prof += [(dw, rr - lip * 2.6), (h, rr - lip), (h, rr), (-h, rr), (-h, rr - lip), (dw - web, rr - lip * 2.6), (dw - web, 0.0)]
        else:   # flat, steel: a plain web
            prof += [(dw, rr - lip * 2.4), (h, rr - lip), (h, rr), (-h, rr), (-h, rr - lip), (dw - web, rr - lip * 2.4), (dw - web, 0.0)]
        m.lathe(c, out, prof, rim_paint, sides)
        _, side, up = frame(out)
        if bolts:
            m.bolts(c + out * (h * 0.9 if f > 0 else d_dish + web), out, rh * 0.8, bolts, r=max(0.009, rh * 0.11), high=0.008, paint=rim_paint)
        if holes:
            # lightening holes in the web, shown as dark sunk discs
            rw = (rh * 1.05 + rr - lip * 2.6) / 2.0
            hr = min((rr - rh) * 0.3, math.pi * rw / holes * 0.62)
            for k in range(holes):
                a = 2 * math.pi * (k + 0.5) / holes
                at = c + out * (d_dish + web * 0.5 + 0.002) + (side * math.cos(a) + up * math.sin(a)) * rw
                m.lathe(at, out, [(0.0, 0.0), (0.0, hr)], "black", m.n(20, 8) if hr > 0.03 else 10)      # (a lorry's big holes are seen close: round)
        if ribs:
            rw0, rw1 = rh * 1.1, rr - lip * 2.6
            for k in range(ribs):
                a = 2 * math.pi * k / ribs
                d = side * math.cos(a) + up * math.sin(a)
                t = d.cross(out)
                p0, p1 = c + out * (d_dish + web * 0.5) + d * rw0, c + out * (d_dish + web * 0.5) + d * rw1
                hgt = min(0.03, (h - d_dish) * 0.7)
                vs = [p0 - t * 0.009, p0 + t * 0.009, p1 + t * 0.009, p1 - t * 0.009, p0 + out * hgt, p1 + out * hgt * 0.5]
                m.m.add(vs, [(0, 3, 5, 4), (1, 4, 5, 2), (0, 4, 1), (3, 2, 5)], rim_paint, False, lambda p, d=d, t=t: out)

    def spokes(self, c, out, r0, r1, count, wide, deep, paint="upper", twist=0.0):
        """Open spokes between a hub and a rim."""
        if not self.fine:
            return
        _, side, up = frame(out)
        for k in range(count):
            a = 2 * math.pi * k / count + twist
            d = side * math.cos(a) + up * math.sin(a)
            t = d.cross(Vector(out).normalized())
            a0, a1 = Vector(c) + d * r0, Vector(c) + d * r1
            o = Vector(out).normalized() * (deep / 2.0)
            w0, w1 = t * (wide / 2.0), t * (wide * 0.38)
            vs = [a0 - w0 - o, a0 + w0 - o, a0 + w0 + o, a0 - w0 + o, a1 - w1 - o, a1 + w1 - o, a1 + w1 + o, a1 - w1 + o]
            self.m.add(vs, [(0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)], paint, False, lambda p, m=(a0 + a1) / 2: p - m)

    def road_wheel(self, name, parent, x, y, z, r, side, w=0.09, tyre=0.03, double=True, gap=0.07, kind="dish", hub=0.34, bolts=6, holes=0, sides=36,
                   axle=0.22, spin=True, **kw):
        """A road wheel as a part of its own. (x, y, z) is the middle of the whole wheel; `side` LEFT or RIGHT."""
        out = Vector((0, side, 0))
        c = Vector((x, y, z))
        self.part(name, parent, c, axis=Y, kind="spin" if spin else "fixed", note=f"rolls: + forward; radius {r:.3f} m")
        if spin:
            self.wheels.append((name, r))
        if not self.fine:
            hw_all = (gap / 2.0 + w) if double else w / 2.0
            self.lathe(c, out, [(hw_all, 0.0), (hw_all, r), (-hw_all, r), (-hw_all, 0.0)], kw.get("tyre_paint", "rubber") if tyre > 0 else "upper", 10)
            self.lathe(c + out * (hw_all + 0.002), out, [(0.0, 0.0), (0.0, r - tyre)], kw.get("rim_paint", "upper"), 10)
            return name
        if double:
            off = gap / 2.0 + w / 2.0
            self.wheel_disc(c + out * off, out, r, w, tyre, kind, hub, bolts, 1.0, sides, holes, **kw)
            self.wheel_disc(c - out * off, out, r, w, tyre, kind, hub, 0, -1.0, max(12, sides * 2 // 3), 0, simple=True, **{k: v for k, v in kw.items() if k in ("rim_paint", "tyre_paint")})
        else:
            self.wheel_disc(c, out, r, w, tyre, kind, hub, bolts, 1.0, sides, holes, **kw)
        if axle > 0 and self.fine:
            self.rod(c - out * (axle + (gap / 2 + w if double else w / 2)), c, r * hub * 0.42, "shade", 8, caps=False)
        return name

    def gear_ring(self, c, out, r_root, r_tip, teeth, thick, paint="upper", flat=0.26, phase=0.0):
        """A toothed ring (a sprocket's)."""
        _, side, up = frame(out)
        c = Vector(c)
        o = Vector(out).normalized()
        p = 2 * math.pi / teeth
        prof = []
        if self.fine:
            for k in range(teeth):
                a = phase + k * p
                for da, rr in ((-0.5 * p + 0.5 * p * (1 - 0.86), r_root), (-flat * p, r_tip), (flat * p, r_tip), (0.5 * p - 0.5 * p * (1 - 0.86), r_root)):
                    prof.append((a + da, rr))
        else:
            prof = [(2 * math.pi * k / 12, (r_root + r_tip) / 2) for k in range(12)]
        bm = bmesh.new()
        A = [bm.verts.new(c + o * (thick / 2) + (side * math.cos(a) + up * math.sin(a)) * rr) for a, rr in prof]
        B = [bm.verts.new(c - o * (thick / 2) + (side * math.cos(a) + up * math.sin(a)) * rr) for a, rr in prof]
        n = len(prof)
        bm.faces.new(A)
        bm.faces.new(B[::-1])
        for k in range(n):
            bm.faces.new((A[k], B[k], B[(k + 1) % n], A[(k + 1) % n]))
        self.put(bm, paint, 0.0)

    def sprocket(self, name, parent, x, y, z, r, side, teeth=18, wide=0.2, tooth=0.035, hub=0.42, kind="dish", bolts=10, holes=0, spokes=0, sides=30, cap=0.6):
        """A drive sprocket: two toothed rings on a drum. r is where the track's inner face rides."""
        out = Vector((0, side, 0))
        c = Vector((x, y, z))
        self.part(name, parent, c, axis=Y, kind="spin", note=f"the driving sprocket: + forward; radius {r:.3f} m; {teeth} teeth")
        self.wheels.append((name, r))
        sides = self.n(sides, 10)
        ring_t = 0.022
        for s in (1.0, -1.0):
            self.gear_ring(c + out * (s * (wide / 2 - ring_t / 2)), out, r - 0.012, r + tooth, teeth, ring_t)
        rh = r * hub
        h = wide / 2.0
        if spokes:
            self.lathe(c, out, [(h + 0.05, 0.0), (h + 0.05, rh * 0.7), (h + 0.02, rh), (-h, rh), (-h, 0.0)], "upper", sides)
            self.spokes(c + out * (h - 0.03), out, rh * 0.9, r - 0.02, spokes, r * 0.2, 0.04)
            self.lathe(c + out * (h - 0.05), out, [(0.04, r - 0.03), (0.04, r - 0.005), (0.0, r - 0.005), (0.0, r - 0.03)], "upper", sides)
        else:
            # a dished face between the hub and the outer ring
            self.lathe(c, out, [(h + 0.06 * cap, 0.0), (h + 0.06 * cap, rh * 0.55), (h + 0.02, rh), (h - 0.035, rh * 1.25), (h - 0.03, r - 0.03), (h - 0.012, r - 0.012),
                                (-h + 0.012, r - 0.012), (-h + 0.03, r - 0.03), (-h, rh), (-h, 0.0)], "upper", sides)
            if holes and self.fine:
                _, sd, up = frame(out)
                rw = (rh * 1.25 + r - 0.03) / 2
                hr = min((r - rh) * 0.24, math.pi * rw / holes * 0.55)
                for k in range(holes):
                    a = 2 * math.pi * (k + 0.5) / holes
                    self.lathe(c + out * (h - 0.026) + (sd * math.cos(a) + up * math.sin(a)) * rw, out, [(0.0, 0.0), (0.0, hr)], "black", 10)
        if bolts:
            self.bolts(c + out * (h + 0.02), out, rh * 0.82, bolts, r=0.013, high=0.01)
        return name

    def idler(self, name, parent, x, y, z, r, side, wide=0.2, kind="spoke", spokes=6, holes=0, sides=30, tyre=0.0, hub=0.3):
        out = Vector((0, side, 0))
        c = Vector((x, y, z))
        self.part(name, parent, c, axis=Y, kind="spin", note=f"the idler: + forward; radius {r:.3f} m")
        self.wheels.append((name, r))
        sides = self.n(sides, 10)
        h = wide / 2.0
        rh = r * hub
        rim = 0.03
        if kind == "spoke" and self.fine:
            for s in (1.0, -1.0):
                cc = c + out * (s * (h - 0.022))
                self.lathe(cc, out, [(0.022, r - rim), (0.022, r), (-0.022, r), (-0.022, r - rim)], "upper", sides)
                self.spokes(cc, out, rh * 0.9, r - rim * 0.6, spokes, r * 0.22, 0.03)
            self.lathe(c, out, [(h + 0.035, 0.0), (h + 0.035, rh * 0.6), (h + 0.01, rh), (-h, rh), (-h, 0.0)], "upper", sides)
            self.bolts(c + out * (h + 0.035), out, rh * 0.36, 6, r=0.011)
        else:
            for s in (1.0, -1.0):
                self.wheel_disc(c + out * (s * (h - wide * 0.22)), out, r, wide * 0.44, tyre, "dish", hub, 6 if s > 0 else 0, s, sides, holes if s > 0 else 0, simple=s < 0)
            self.rod(c - out * h, c + out * h, rh * 0.6, "shade", 10, caps=False)
        return name

    def roller(self, name, parent, x, y, z, r, side, wide=0.1, tyre=0.012, sides=18, reach=0.2):
        out = Vector((0, side, 0))
        c = Vector((x, y, z))
        self.part(name, parent, c, axis=Y, kind="spin", note=f"a return roller: + forward; radius {r:.3f} m")
        self.wheels.append((name, r))
        h = wide / 2
        if not self.fine:
            self.lathe(c, out, [(h, 0.0), (h, r), (-h, r), (-h, 0.0)], "rubber" if tyre > 0 else "upper", 6)
            return name
        if tyre > 0:
            self.lathe(c, out, [(h, r - tyre), (h, r), (-h, r), (-h, r - tyre)], "rubber", sides)
        self.lathe(c, out, [(h + 0.02, 0.0), (h + 0.02, r * 0.3), (h, r * 0.42), (h - 0.006, r - tyre), (-h, r - tyre), (-h, 0.0)], "upper", sides)
        if self.fine:
            self.rod(c - out * reach, c, r * 0.3, "shade", 8, caps=False)
        return name

    # ---- the track -----------------------------------------------------------------------------------
    def link_shape(self, style, pitch, wide, thick):
        """One link in its own axes (u along the track's travel, v across, w outward from the wheels): a list of
        (points, faces, paint). Faces are wound for u, v, w taken as a right-handed set."""
        L = pitch * (0.9 if style.get("gap", True) else 0.98)
        W = wide
        out = []

        def box(u0, u1, v0, v1, w0, w1, paint, taper=0.0, tv=0.0, top=None, shine=(0.0, 0.0, 0.0)):
            # (taper draws the outer face in along u: a grouser bar is a ridge; tv along v; `top` paints its outer face;
            #  `shine` is how far the face next the wheels, the four sides and the outer face are polished to bare steel)
            tu = (u1 - u0) * taper * 0.5
            tw = (v1 - v0) * tv * 0.5
            vs = [(u0, v0, w0), (u1, v0, w0), (u1, v1, w0), (u0, v1, w0), (u0 + tu, v0 + tw, w1), (u1 - tu, v0 + tw, w1), (u1 - tu, v1 - tw, w1), (u0 + tu, v1 - tw, w1)]
            fs = [(0, 3, 2, 1), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
            cap = [(4, 5, 6, 7)]
            if w1 < w0:
                fs = [tuple(reversed(f)) for f in fs]
                cap = [tuple(reversed(f)) for f in cap]
            if top:
                out.append((vs, fs, paint, [shine[0]] + [shine[1]] * 4))
                out.append((vs, cap, top, [shine[2]]))
            else:
                out.append((vs, fs + cap, paint, [shine[0]] + [shine[1]] * 4 + [shine[2]]))

        paint = style.get("paint", "track")
        kind = style.get("kind", "plate")
        g = style.get("grouser", 0.014)
        if kind == "plate":
            # a cast plate link: the plate, a raised bar across it, the pin's eyes at each edge
            # (the wheels polish the face they run on; the ground polishes the cleat and leaves the hollows to rust)
            box(-L / 2, L / 2, -W / 2, W / 2, 0.0, thick, paint, shine=(0.8, 0.0, 0.0))
            if self.fine:
                bar = style.get("bar", 0.34)
                box(-L * bar, L * bar, -W / 2 + 0.012, W / 2 - 0.012, thick, thick + g, paint, taper=0.45, shine=(0.0, 0.3, 1.0))
                if style.get("cleats"):
                    for vv in (-W * 0.3, W * 0.3):
                        box(-L * 0.46, L * 0.46, vv - 0.02, vv + 0.02, thick, thick + g * 0.7, paint, taper=0.2, shine=(0.0, 0.2, 0.9))
                if style.get("spurs"):
                    # ice spurs: short pointed studs bolted on the bar, that bite into ice and packed snow; the ground
                    # wears their points bright
                    k_n = int(style["spurs"])
                    sh = style.get("spur_high", 0.03)
                    for k in range(k_n):
                        vv = W * 0.84 * ((k + 0.5) / k_n - 0.5)
                        box(-L * 0.13, L * 0.13, vv - 0.024, vv + 0.024, thick + g * 0.8, thick + g + sh, paint, taper=0.75, tv=0.75, shine=(0.0, 0.5, 1.0))
                # the pin and its eyes, closing the gap to the next link
                box(L / 2 - 0.004, pitch - L / 2 + 0.004, -W / 2, W / 2, 0.003, thick - 0.002, paint)
        elif kind == "block":
            # a rubber block on two pins, joined to the next by steel end connectors
            box(-L / 2, L / 2, -W / 2 + 0.035, W / 2 - 0.035, 0.0, thick, "rubber")
            if self.fine:
                if style.get("chevron", True):
                    for sgn in (1.0, -1.0):
                        vs = [(-L * 0.36, sgn * 0.01, thick), (-L * 0.16, sgn * 0.01, thick), (L * 0.36, sgn * (W / 2 - 0.045), thick), (L * 0.16, sgn * (W / 2 - 0.045), thick)]
                        hi = [(u, v, thick + g) for u, v, _ in vs]
                        pts = vs + hi
                        fs = [(4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
                        if sgn < 0:
                            fs = [tuple(reversed(f)) for f in fs]
                        out.append((pts, fs, "rubber", None))
                for vv in (-1.0, 1.0):
                    v0, v1 = (W / 2 - 0.035, W / 2) if vv > 0 else (-W / 2, -W / 2 + 0.035)
                    box(L / 2 - 0.035, L / 2 + pitch * 0.14, v0, v1, 0.002, thick - 0.004, "steel")
        elif kind == "skeleton":
            # an open (skeleton) link: two rails and cross bars
            for vv in (-W / 2, W / 2 - W * 0.22):
                box(-L / 2, L / 2, vv, vv + W * 0.22, 0.0, thick, paint, shine=(0.8, 0.0, 0.3))
            box(-L * 0.18, L * 0.18, -W / 2, W / 2, thick * 0.2, thick + g, paint, taper=0.4, shine=(0.0, 0.3, 1.0))
        # the guide horns, standing in toward the wheels
        horn = style.get("horn", "centre")
        hh = style.get("horn_high", 0.07)
        if self.fine and horn:
            if horn == "centre":
                spots = [(0.0, 0.016)]
            elif horn == "twin":
                gv = style.get("horn_gap", 0.1)
                spots = [(-gv, 0.014), (gv, 0.014)]
            else:      # ends: on the end connectors, outside the wheel
                spots = [(-(W / 2 - 0.02), 0.016), (W / 2 - 0.02, 0.016)]
            for vv, hw in spots:
                u0, u1 = (L / 2 - 0.04, L / 2 + pitch * 0.12) if horn == "ends" else (-L * 0.3, L * 0.3)
                vs = [(u0, vv - hw, 0.0), (u1, vv - hw, 0.0), (u1, vv + hw, 0.0), (u0, vv + hw, 0.0),
                      ((u0 + u1) / 2 - 0.012, vv - hw * 0.5, -hh), ((u0 + u1) / 2 + 0.012, vv - hw * 0.5, -hh), ((u0 + u1) / 2 + 0.012, vv + hw * 0.5, -hh), ((u0 + u1) / 2 - 0.012, vv + hw * 0.5, -hh)]
                fs = [(4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
                fs = [tuple(reversed(f)) for f in fs]
                out.append((vs, fs, "steel" if horn == "ends" else paint, None if horn == "ends" else [0.85] * 5))
        return out

    def track(self, letter, side, y, wheels, pitch=0.12, wide=0.4, thick=0.03, style=None, sag=0.03, parent="vehicle", every=1):
        """Lays a track round `wheels` ((x, z, r) in the order the track meets them: along the ground rear to front,
        over the front wheel, back along the top, round the rear wheel). Makes the part `track_<letter>`, a part for every
        link, and the clip that runs the track."""
        style = style or {}
        belt = Belt(wheels, sag=sag)
        n = max(8, int(round(belt.long / pitch)))
        p = belt.long / n
        name = f"track_{letter}"
        self.part(name, parent, (0, y, 0), kind="fixed", note="the track: its links are its children; the clip tracks." + ("left" if side > 0 else "right") + " runs it")
        info = {"part": name, "links": n if self.fine else 0, "pitch": round(p, 5), "long": round(belt.long, 4), "wide": wide, "y": round(y, 4)}
        self.belts[letter] = info
        if not self.fine:
            # the plain mesh: a band
            pts = [belt.pts[0]]
            for q in belt.pts[1:]:
                if (q - pts[-1]).length > 0.16:
                    pts.append(q)
            k = len(pts)
            outer, inner = [], []
            for i in range(k):
                way = (pts[(i + 1) % k] - pts[i - 1]).normalized()
                o = Vector((way.y, -way.x))
                outer.append(pts[i] + o * (thick + 0.012))
                inner.append(pts[i])
            verts, faces = [], []
            for row in (outer, inner):
                for s in (wide / 2, -wide / 2):
                    verts += [Vector((q.x, y + s, q.y)) for q in row]
            for i in range(k):
                j = (i + 1) % k
                for a, b in ((0, 1), (1, 3), (3, 2), (2, 0)):
                    faces.append((a * k + i, a * k + j, b * k + j, b * k + i))
            mid = Vector((sum(q.x for q in pts) / k, y, sum(q.y for q in pts) / k))
            self.m.add(verts, faces, style.get("paint", "track") if style.get("kind") != "block" else "rubber", True, lambda q: Vector((q.x - mid.x, 0, q.z - mid.z)) if abs(q.y - y) < wide / 2 - 1e-4 else Vector((0, q.y - y, 0)))
            return belt
        shape = self.link_shape(style, p, wide, thick)
        clip_tracks = []
        steps_curved = 4
        for i in range(n):
            s0 = i * p
            pt, way, o = belt.at(s0)
            lname = f"{name}.{i + 1:03d}"
            pivot = Vector((pt.x, y, pt.y))
            self.m.part(lname, name, pivot, kind="free", note="")
            U, W = Vector((-way.x, 0, -way.y)), Vector((o.x, 0, o.y))        # (a link's own forward is the way it travels when she drives ahead)
            Vv = W.cross(U)                                                   # so u, v, w is a right-handed set
            mirror = Vv.y * side < 0                                          # keep v outward on both sides (the shape may not be symmetric)
            for vs, fs, paint, shine in shape:
                pts3 = [pivot + U * u + Vv * (-v if mirror else v) + W * w for u, v, w in vs]
                f2 = [tuple(reversed(f)) for f in fs] if mirror else fs
                before = len(self.m.faces)
                self.m.add(pts3, f2, paint, False, None, lname)
                self.flag(before, shine)
            # the clip: this link goes to where the one behind it in travel stands (s falls by one pitch)
            a0 = math.atan2(way.y, way.x)
            keys_r, keys_m = [], []
            pe, waye, _ = belt.at(s0 - p)
            turn_all = (math.atan2(waye.y, waye.x) - a0 + math.pi) % (2 * math.pi) - math.pi
            k_steps = steps_curved if abs(turn_all) > 0.02 else 1
            for j in range(k_steps + 1):
                f = j / k_steps
                pj, wj, _ = belt.at(s0 - p * f)
                da = (math.atan2(wj.y, wj.x) - a0 + math.pi) % (2 * math.pi) - math.pi
                q = Quaternion((0, -1, 0), da)
                keys_r.append([f, q.w, q.x, q.y, q.z])
                keys_m.append([f, pj.x - pt.x, 0.0, pj.y - pt.y])
            clip_tracks.append({"part": lname, "rot": keys_r, "move": keys_m})
        which = "left" if side > 0 else "right"
        long = 0.25
        for tr in clip_tracks:
            for key in ("rot", "move"):
                for k in tr[key]:
                    k[0] *= long
        self.m.clips["tracks." + which] = {
            "long": long, "loop": True, "stride": round(p, 5), "tracks": clip_tracks, "events": [],
            "about": f"the {which} track runs on by one link ({p * 1000:.0f} mm): loop it at (that side's speed in m/s) / stride * long; backward to go astern"}
        self.m.use(name)
        return belt
