"""Fittings: everything bolted, welded, hinged or strapped to a vehicle.

Guns and their muzzle brakes, hatches that open (each a part of its own), cupolas, visors, machine-gun mounts, lamps,
tools, tow cables, spare links, jerrycans, bins, aerials, silencers, grilles. Small fittings are left off the plain
(far) mesh.
"""

import math

import bpy  # noqa: F401
import bmesh
from mathutils import Matrix, Vector

from motorpool import LEFT, RIGHT, X, Y, Z, bm_box, bm_extrude, bm_move, bm_prism, frame, outline, rot, round_plan


class Fittings:
    # ---- guns ----------------------------------------------------------------------------------------
    def barrel(self, base, length, bore, r0=None, r1=None, brake=None, steps=(), paint="upper", sleeve=None, sides=24, facing=X, part=None, jacket=None):
        """A gun barrel from `base` along `facing`: `r0` thick at the breech end, `r1` at the muzzle. steps: (how far
        along as a fraction, radius) where it steps down; sleeve: (length, radius) of the fixed sleeve at its root;
        brake: None, 'double' (two baffles), 'single', 'pepper' (a perforated sleeve), 'cone' (a flash cone)."""
        r0 = r0 or bore * 1.25
        r1 = r1 or bore * 0.92
        sides = self.n(sides, 8)
        L = length
        prof = []                                       # from the muzzle back to the root, round the outside
        bl = 0.0
        if brake == "double":
            bl = bore * 5.6
            rb = bore * 1.55
            f = L - bl
            prof += [(L, bore * 0.62), (L, rb * 0.78), (L - bore * 0.5, rb), (L - bore * 1.3, rb), (L - bore * 1.5, rb * 0.62), (L - bore * 2.5, rb * 0.62),
                     (L - bore * 2.7, rb * 1.06), (L - bore * 3.7, rb * 1.06), (L - bore * 3.9, rb * 0.66), (f + bore * 0.6, rb * 0.66), (f, r1 * 1.12)]
        elif brake == "single":
            bl = bore * 3.4
            rb = bore * 1.6
            f = L - bl
            prof += [(L, bore * 0.62), (L, rb * 0.8), (L - bore * 0.5, rb), (L - bore * 1.9, rb), (L - bore * 2.2, rb * 0.66), (f + bore * 0.5, rb * 0.66), (f, r1 * 1.12)]
        elif brake == "pepper":
            bl = bore * 4.5
            rb = bore * 1.0
            f = L - bl
            prof += [(L, bore * 0.62), (L, rb), (f + bore * 0.3, rb), (f, r1)]
        elif brake == "cone":
            bl = bore * 2.2
            f = L - bl
            prof += [(L, bore * 0.62), (L, bore * 1.5), (L - 0.012, bore * 1.55), (f, r1)]
        else:
            f = L
            prof += [(L, bore * 0.56), (L, r1 - 0.004), (L - 0.006, r1)]
        last = f
        rr = r1
        for frac, rad in sorted(steps, reverse=True):
            x = L * frac
            ra = lerp_r(r0, r1, x / L)
            prof += [(x + 0.012, lerp_r(r0, r1, (x + 0.012) / L) if rad is None else rr), (x, rad if rad is not None else ra)]
            rr = rad if rad is not None else ra
            last = x
        prof += [(0.0, r0 if not steps else max(r0, rr)), (0.0, 0.0)]
        if part:
            self.m.use(part)
        self.lathe(base, facing, [(L - bore * 1.2, 0.0), (L - bore * 1.2, bore * 0.56), (L, bore * 0.56 if not brake else bore * 0.62)], "black", sides)
        self.lathe(base, facing, prof, paint, sides)
        if brake in ("double", "single") and self.fine:
            # the side openings of the baffles, shown dark
            ax, side, up = frame(facing)
            M = Matrix((ax, side, up)).transposed()
            for d in ((L - bore * 2.0, L - bore * 4.4) if brake == "double" else (L - bore * 2.7,)):
                for s in (1.0, -1.0):
                    bm = bm_box((bore * 0.86, 0.01, bore * 1.0), (0, 0, 0))
                    bm_move(bm, at=Vector(base) + ax * d + side * (s * bore * 0.99), turn=M)
                    self.put(bm, "black")
        if brake == "pepper" and self.fine:
            ax, side, up = frame(facing)
            for row in range(4):
                for k in range(6):
                    a = 2 * math.pi * (k + 0.5 * (row % 2)) / 6
                    c = Vector(base) + ax * (L - bore * (0.7 + row * 1.0)) + (side * math.cos(a) + up * math.sin(a)) * (bore * 1.0 + 0.001)
                    self.lathe(c, side * math.cos(a) + up * math.sin(a), [(0.0, 0.0), (0.0, bore * 0.26)], "black", 6)
        if sleeve:
            sl, sr = sleeve
            self.lathe(base, facing, [(sl, r0), (sl, sr * 0.92), (sl - 0.02, sr), (0.0, sr), (0.0, 0.0)], paint, sides)
        if jacket:      # (a recoil cylinder or cooling jacket beside or over the barrel)
            jl, jr, jo = jacket
            self.rod(Vector(base) + Vector(jo), Vector(base) + Vector(jo) + Vector(facing).normalized() * jl, jr, paint, 14)

    def mg(self, base, facing=X, length=0.55, r=0.016, paint="steel", jacket=True, part=None):
        """A machine gun's barrel in its jacket."""
        b = Vector(base)
        f = Vector(facing).normalized()
        if part:
            self.m.use(part)
        if jacket and self.fine:
            self.lathe(b, f, [(length * 0.8, 0.0), (length * 0.8, r * 1.5), (0.0, r * 1.5), (0.0, 0.0)], paint, 10)
            self.lathe(b, f, [(length, 0.0), (length, r * 0.7), (length * 0.8, r * 0.7)], paint, 8)
            self.lathe(b, f, [(length + 0.001, 0.0), (length + 0.001, r * 0.4)], "black", 6)
        else:
            self.lathe(b, f, [(length, 0.0), (length, r), (0.0, r * 1.2), (0.0, 0.0)], paint, 8)

    def mg_ball(self, at, facing, r=0.11, paint="upper", length=0.42, part=None, socket=None, ring=True):
        """A ball-mounted machine gun in a plate."""
        at, f = Vector(at), Vector(facing).normalized()
        if part:
            self.m.use(part)
        if ring:
            self.lathe(at - f * 0.02, f, [(0.035, r * 0.95), (0.035, r * 1.28), (0.0, r * 1.36), (0.0, 0.0)], paint, self.n(20, 8))
            self.bolts(at + f * 0.016, f, r * 1.2, 8, r=0.011)
        # the ball
        prof = [(r * 0.86, 0.0)] + [(math.sin(a) * r, math.cos(a) * r) for a in (math.radians(d) for d in (62, 44, 26, 8))]
        self.lathe(at - f * 0.02, f, prof, paint, self.n(20, 8))
        self.mg(at + f * (r * 0.8), f, length, part=None)
        if socket:
            self.socket(socket, at + f * (r * 0.8 + length), f)

    # ---- hatches -------------------------------------------------------------------------------------
    def hatch_rect(self, name, parent, corner, size, up=Z, along=X, hinge="back", thick=0.02, opens=95.0, paint="upper", handle=True, well=True, split=False, lip=0.0):
        """A flat rectangular hatch as a part that opens. `corner` is its middle on the plate, `up` the plate's
        outward way, `along` a way lying in the plate (its length runs along it). `hinge`: which edge it hangs on
        (back, front, left, right, taking `along` as forward)."""
        c = Vector(corner)
        n = Vector(up).normalized()
        u = Vector(along)
        u = (u - n * u.dot(n)).normalized()
        v = n.cross(u)
        hx, hy = size[0] / 2.0, size[1] / 2.0
        edge = {"back": (-u * hx, v), "front": (u * hx, -v), "left": (v * hy, u), "right": (-v * hy, -u)}[hinge]
        pivot = c + edge[0] + n * (thick * 0.5)
        axis = edge[1]
        # (plus opens: the lid lifts away from the plate about its hinge line)
        self.part(name, parent, pivot, axis=axis, limits=[0.0, opens], kind="turn", note="a hatch: + opens")
        M = Matrix((u, v, n)).transposed()
        bm = bm_box((size[0], size[1], thick), (0, 0, 0))
        bm_move(bm, at=c + n * (thick * 0.5), turn=M)
        self.put(bm, paint, 0.005)
        if self.fine:
            # hinges and a handle
            for s in (-0.6, 0.6):
                h0 = pivot + axis * (s * (hy if hinge in ("back", "front") else hx)) * 1.0
                self.rod(h0 - axis * 0.04, h0 + axis * 0.04, 0.012, paint, 8)
            if handle:
                far = c - edge[0] * 0.55 + n * thick
                self.handle(far - axis * 0.06, far + axis * 0.06, n, 0.007, 0.035, "steel")
            if lip:
                pass
        if well:
            # what shows when it is open: the dark of the inside, just under the plate's face
            self.use(parent)
            bm = bm_box((size[0] * 0.9, size[1] * 0.9, 0.004), (0, 0, 0))
            bm_move(bm, at=c + n * 0.0015, turn=M)
            self.put(bm, "black")
        self.use(parent)
        return name

    def hatch_round(self, name, parent, c, r, up=Z, hinge_way=-X, thick=0.025, opens=100.0, paint="upper", dome=0.02, handle=True, well=True, split=False):
        """A round hatch lid as a part that opens, hinged on the side `hinge_way`."""
        c = Vector(c)
        n = Vector(up).normalized()
        hw = Vector(hinge_way)
        hw = (hw - n * hw.dot(n)).normalized()
        pivot = c + hw * r + n * (thick * 0.5)
        axis = hw.cross(n)
        self.part(name, parent, pivot, axis=axis, limits=[0.0, opens], kind="turn", note="a hatch: + opens")
        sides = self.n(24, 10)
        self.lathe(c, n, [(thick + dome, 0.0), (thick + dome * 0.7, r * 0.6), (thick, r * 0.94), (thick * 0.6, r), (0.0, r), (0.0, 0.0)], paint, sides)
        if self.fine:
            self.rod(pivot - axis * 0.06, pivot + axis * 0.06, 0.014, paint, 8)
            self.box((0.05, 0.05, 0.012), c + hw * (r * 0.8) + n * (thick + 0.004), paint, turn=None)
            if handle:
                far = c - hw * (r * 0.55) + n * (thick + dome * 0.6)
                self.handle(far - axis * 0.05, far + axis * 0.05, n, 0.007, 0.03, "steel")
        if well:
            self.use(parent)
            self.lathe(c, n, [(0.002, 0.0), (0.002, r * 0.9)], "black", sides)
        self.use(parent)
        return name

    def cupola(self, c, r, high, slits=5, paint="upper", wall=0.05, ring=True, part=None, start=0.0, top_lip=0.03, sides=28):
        """A commander's cupola: a drum with vision slits round it. Returns the middle of its top (for the hatch)."""
        c = Vector(c)
        if part:
            self.m.use(part)
        sides = self.n(sides, 10)
        self.lathe(c, Z, [(high, r - wall), (high, r - 0.012), (high - 0.012, r), (high * 0.62, r * 1.04), (0.03, r * 1.04), (0.0, r * 1.1), (0.0, 0.0)], paint, sides)
        self.lathe(c, Z, [(high - 0.03, 0.0), (high - 0.03, r - wall), (high, r - wall)], "shade", sides)
        if self.fine:
            for k in range(slits):
                a = start + 2 * math.pi * k / slits
                d = Vector((math.cos(a), math.sin(a), 0))
                t = Vector((-d.y, d.x, 0))
                m = Matrix((d, t, Z)).transposed()
                bm = bm_box((0.05, r * 0.5, high * 0.3), (0, 0, 0))
                bm_move(bm, at=c + d * (r * 1.04) + Z * (high * 0.42), turn=m)
                self.put(bm, paint, 0.004)
                bm = bm_box((0.012, r * 0.36, 0.018), (0, 0, 0))
                bm_move(bm, at=c + d * (r * 1.04 + 0.022) + Z * (high * 0.44), turn=m)
                self.put(bm, "glass")
        return c + Z * high

    def periscope(self, at, facing=X, size=(0.07, 0.14, 0.07), paint="upper", part=None, up=Z):
        if not self.fine:
            return
        at = Vector(at)
        f = Vector(facing).normalized()
        yaw = math.degrees(math.atan2(f.y, f.x))
        if part:
            self.m.use(part)
        self.box(size, at + Z * (size[2] / 2), paint, 0.006, rot(yaw))
        self.box((0.006, size[1] * 0.8, size[2] * 0.45), at + Z * (size[2] * 0.6) + f * (size[0] / 2 + 0.001), "glass", turn=rot(yaw))

    def visor(self, at, facing, wide=0.34, high=0.12, thick=0.06, paint="upper", slit=True, part=None, brow=True):
        """A driver's visor block on a plate."""
        at, f = Vector(at), Vector(facing).normalized()
        yaw = math.degrees(math.atan2(f.y, f.x))
        pitch = math.degrees(math.asin(max(-1.0, min(1.0, f.z))))
        m = rot(yaw, pitch)
        if part:
            self.m.use(part)
        self.put(bm_box((thick, wide, high), at + f * (thick / 2), m), paint, 0.008)
        if self.fine:
            if slit:
                self.put(bm_box((0.006, wide * 0.62, high * 0.2), at + f * (thick + 0.001), m), "glass")
            if brow:
                self.put(bm_box((thick * 1.5, wide * 1.06, 0.012), at + f * (thick * 0.75) + (m @ Z) * (high / 2 + 0.006), m), paint)
            for s in (-1, 1):
                self.bolt(at + f * thick + (m @ Y) * (s * wide * 0.42) + (m @ Z) * (-high * 0.3), f, 0.011)

    def port(self, at, facing, r=0.045, paint="upper", part=None):
        """A pistol or vision port: a round plug in a plate."""
        if not self.fine:
            return
        if part:
            self.m.use(part)
        self.lathe(Vector(at), facing, [(0.022, 0.0), (0.022, r * 0.7), (0.012, r), (0.0, r * 1.1), (0.0, 0.0)], paint, 12)

    def vent(self, at, r=0.11, high=0.05, paint="upper", part=None):
        """An armoured ventilator on a roof: a low mushroom."""
        if part:
            self.m.use(part)
        self.lathe(Vector(at), Z, [(high, 0.0), (high, r * 0.7), (high * 0.7, r), (high * 0.45, r), (high * 0.4, r * 0.55), (0.0, r * 0.55), (0.0, 0.0)], paint, self.n(18, 8))

    def lift_eye(self, at, facing=Z, across=Y, r=0.035, paint="upper", part=None):
        """A lifting eye: a half ring standing on a plate."""
        if not self.fine:
            return
        at, f, a = Vector(at), Vector(facing).normalized(), Vector(across).normalized()
        pts = [at + a * (math.cos(t) * r) + f * (math.sin(t) * r) for t in (math.pi * k / 6 for k in range(7))]
        if part:
            self.m.use(part)
        self.tube(pts, r * 0.24, paint, 6)

    def hook(self, at, facing=X, size=0.09, paint="upper", part=None):
        """A towing eye plate with its shackle."""
        at, f = Vector(at), Vector(facing).normalized()
        yaw = math.degrees(math.atan2(f.y, f.x))
        if part:
            self.m.use(part)
        self.put(bm_box((size * 1.2, size * 0.35, size * 1.3), at + f * (size * 0.5), rot(yaw)), paint, 0.008)
        if self.fine:
            c = at + f * (size * 1.0)
            side = Vector((-f.y, f.x, 0))
            pts = [c + side * (s * size * 0.36) + f * (math.sin(t) * size * 0.75) - Z * ((1 - math.cos(t)) * size * 0.2) for s, t in ((1, 0.0), (1, 0.9), (0.6, 1.4), (0, 1.57), (-0.6, 1.4), (-1, 0.9), (-1, 0.0))]
            self.tube(pts, size * 0.11, "steel", 6)

    # ---- lamps, aerials --------------------------------------------------------------------------------
    def headlamp(self, at, facing=X, r=0.075, paint="upper", stalk=0.08, cover=False, part=None):
        at, f = Vector(at), Vector(facing).normalized()
        if part:
            self.m.use(part)
        sides = self.n(16, 8)
        self.lathe(at + Z * stalk, f, [(r * 0.5, r * 0.96), (r * 0.5, r), (0.0, r), (-r * 0.6, r * 0.78), (-r * 0.95, r * 0.35), (-r * 1.0, 0.0)], paint, sides)
        self.lathe(at + Z * stalk, f, [(r * 0.5, 0.0), (r * 0.5, r * 0.96)], "upper" if cover else "lamp", sides)
        if cover and self.fine:
            self.box((0.004, r * 1.0, r * 0.12), at + Z * stalk + f * (r * 0.5 + 0.002), "lamp", turn=rot(math.degrees(math.atan2(f.y, f.x))))
        if stalk > 0 and self.fine:
            self.rod(at, at + Z * (stalk - r * 0.6), r * 0.22, paint, 8)

    def aerial(self, name, parent, at, high=2.0, r=0.006, base=0.035, lean=(0.0, 0.0), paint="steel"):
        """A rod aerial on a sprung base, as a part that can whip."""
        at = Vector(at)
        self.part(name, parent, at, axis=(0, -1, 0), limits=[-25.0, 25.0], kind="turn", note="the aerial: whips fore and aft")
        self.lathe(at, Z, [(base * 2.2, 0.0), (base * 2.2, base * 0.45), (base * 1.2, base * 0.8), (0.0, base), (0.0, 0.0)], "black", 10)
        top = at + Vector((lean[0], lean[1], high))
        self.lathe(at + Z * (base * 2.0), top - at, [(high, 0.0), (high, r * 0.5), (0.0, r), (0.0, 0.0)], paint, 6)
        self.socket("aerial", top, Z)
        self.use(parent)

    # ---- tools and stowage ---------------------------------------------------------------------------
    def shovel(self, at, yaw=0.0, long=0.95, part=None, tilt=0.0, roll=0.0):
        if not self.fine:
            return
        m = rot(yaw, tilt, roll)
        at = Vector(at)
        if part:
            self.m.use(part)
        self.rod(at, at + m @ Vector((long * 0.68, 0, 0)), 0.016, "wood", 8)
        blade = [(long * 0.66, -0.035), (long * 0.72, -0.09), (long * 0.97, -0.085), (long, 0.0), (long * 0.97, 0.085), (long * 0.72, 0.09), (long * 0.66, 0.035)]
        bm = bm_prism(blade, -0.005, 0.005)
        bm_move(bm, at=at, turn=m)
        self.put(bm, "steel")
        self.rod(at + m @ Vector((-0.04, -0.05, 0)), at + m @ Vector((-0.04, 0.05, 0)), 0.014, "wood", 6)

    def axe(self, at, yaw=0.0, long=0.8, part=None, tilt=0.0, roll=0.0):
        if not self.fine:
            return
        m = rot(yaw, tilt, roll)
        at = Vector(at)
        if part:
            self.m.use(part)
        self.rod(at, at + m @ Vector((long, 0, 0)), 0.015, "wood", 8)
        head = [(long * 0.86, -0.02), (long * 0.84, 0.12), (long * 0.99, 0.14), (long * 0.97, -0.02)]
        bm = bm_prism(head, -0.014, 0.014)
        bm_move(bm, at=at, turn=m)
        self.put(bm, "steel")

    def crowbar(self, at, yaw=0.0, long=1.3, part=None, tilt=0.0):
        if not self.fine:
            return
        m = rot(yaw, tilt)
        at = Vector(at)
        if part:
            self.m.use(part)
        self.tube([at + m @ Vector(p) for p in ((0, 0, 0), (long * 0.9, 0, 0), (long * 0.97, 0, 0.012), (long, 0, 0.04))], 0.011, "steel", 6)

    def jack(self, at, yaw=0.0, part=None, long=0.52):
        if not self.fine:
            return
        m = rot(yaw)
        at = Vector(at)
        if part:
            self.m.use(part)
        self.put(bm_box((long, 0.12, 0.13), at + Z * 0.065, m), "upper", 0.008)
        self.rod(at + m @ Vector((-long * 0.6, 0, 0.07)), at + m @ Vector((long * 0.6, 0, 0.07)), 0.03, "upper", 10)
        self.put(bm_box((0.05, 0.18, 0.05), at + m @ Vector((long * 0.3, 0, 0.14)), m), "upper")

    def jack_block(self, at, size=(0.42, 0.22, 0.13), yaw=0.0, part=None):
        """The wooden block a jack stands on, in its steel straps."""
        if not self.fine:
            return
        m = rot(yaw)
        at = Vector(at)
        if part:
            self.m.use(part)
        self.put(bm_box(size, at + Z * size[2] / 2, m), "wood", 0.006)
        for s in (-0.3, 0.3):
            self.put(bm_box((0.03, size[1] + 0.012, size[2] + 0.008), at + m @ Vector((s * size[0], 0, size[2] / 2)), m), "upper")

    def jerrycan(self, at, yaw=0.0, paint="upper", part=None, lay=False):
        """A 20-litre can."""
        m = rot(yaw, 0.0, 90.0 if lay else 0.0)
        at = Vector(at)
        if part:
            self.m.use(part)
        w, d, h = 0.34, 0.16, 0.40
        lift = Vector((0, 0, d / 2 if lay else h / 2))
        self.put(bm_box((w, d, h), at + lift, m), paint, 0.022, segs=2)
        if self.fine:
            top = at + lift + m @ Vector((0, 0, h / 2))
            for s in (-1, 0, 1):
                self.tube([top + m @ Vector(p) for p in ((-0.07, s * 0.035, 0.0), (-0.06, s * 0.035, 0.03), (0.02, s * 0.035, 0.03), (0.03, s * 0.035, 0.0))], 0.007, paint, 5)
            self.rod(top + m @ Vector((0.11, 0, -0.005)), top + m @ Vector((0.13, 0, 0.04)), 0.022, paint, 8)
            # the pressed X in its side
            for s in (1, -1):
                for a in (0.86, -0.86):
                    self.put(bm_box((0.012, 0.004, 0.3), at + lift + m @ Vector((-0.02, s * (d / 2 + 0.001), -0.02)), m @ Matrix.Rotation(a, 3, "Y")), paint)

    def bin(self, a, b, paint="upper", bevel=0.012, lid=True, part=None, latches=2, lid_side=None):
        """A stowage bin by two opposite corners, with a lid line and latches on the side `lid_side` (a unit way)."""
        a, b = Vector(a), Vector(b)
        if part:
            self.m.use(part)
        self.slab(a, b, paint, bevel, segs=2)
        if not self.fine or not lid:
            return
        lo = Vector((min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)))
        hi = Vector((max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)))
        c = (lo + hi) / 2
        s = hi - lo
        self.slab((lo.x - 0.006, lo.y - 0.006, hi.z - 0.035), (hi.x + 0.006, hi.y + 0.006, hi.z + 0.004), paint, 0.006)
        ls = Vector(lid_side) if lid_side is not None else Vector((0, 1 if c.y >= 0 else -1, 0))
        across = Vector((-ls.y, ls.x, 0))
        reach = abs(ls.x) * s.x / 2 + abs(ls.y) * s.y / 2
        span = abs(across.x) * s.x + abs(across.y) * s.y
        for k in range(latches):
            t = (k + 0.5) / latches - 0.5
            p = Vector((c.x, c.y, hi.z - 0.05)) + ls * (reach + 0.008) + across * (t * span * 0.7)
            self.box((0.02 + 0.006 * abs(ls.x), 0.02 + 0.006 * abs(ls.y), 0.06), p, "steel")

    def roll(self, a, b, r=0.11, paint="canvas", straps=2, part=None, sag=0.0):
        """A rolled tarpaulin or bedroll between two points, with straps."""
        a, b = Vector(a), Vector(b)
        if part:
            self.m.use(part)
        ax = (b - a)
        L = ax.length
        g = self.rng
        k = 7 if self.fine else 2
        prof = [(L, 0.0), (L, r * 0.55), (L - r * 0.25, r * 0.95)]
        for i in range(k - 1, 0, -1):
            prof.append((L * i / k, r * (1.0 + g.uniform(-0.06, 0.06))))
        prof += [(r * 0.25, r * 0.95), (0.0, r * 0.55), (0.0, 0.0)]
        self.lathe(a, ax, prof, paint, self.n(14, 6))
        if self.fine:
            for i in range(straps):
                t = (i + 0.5) / straps
                self.lathe(a, ax, [(L * t + 0.02, r * 0.9), (L * t + 0.02, r * 1.04), (L * t - 0.02, r * 1.04), (L * t - 0.02, r * 0.9)], "leather", 14)

    def crate(self, at, size=(0.55, 0.3, 0.25), yaw=0.0, paint="wood", part=None):
        m = rot(yaw)
        at = Vector(at)
        if part:
            self.m.use(part)
        self.put(bm_box(size, at + Z * size[2] / 2, m), paint, 0.006)
        if self.fine:
            for s in (-0.36, 0.36):
                self.put(bm_box((0.035, size[1] + 0.014, size[2] + 0.01), at + m @ Vector((s * size[0], 0, size[2] / 2)), m), paint)
            for s in (-1, 1):
                self.tube([at + m @ Vector(p) for p in ((s * size[0] / 2, -0.06, size[2] * 0.65), (s * (size[0] / 2 + 0.03), -0.05, size[2] * 0.55), (s * (size[0] / 2 + 0.03), 0.05, size[2] * 0.55), (s * size[0] / 2, 0.06, size[2] * 0.65))], 0.006, "rope", 5)

    def bucket(self, at, r=0.13, high=0.26, paint="steel", part=None):
        if not self.fine:
            return
        if part:
            self.m.use(part)
        at = Vector(at)
        self.lathe(at, Z, [(high, r * 0.94), (high, r), (0.0, r * 0.78), (0.0, 0.0)], paint, 14)
        self.lathe(at, Z, [(high * 0.3, 0.0), (high * 0.3, r * 0.8), (high, r * 0.94)], "black", 14)

    def cable(self, points, r=0.012, paint="steel", eyes=True, part=None):
        """A tow cable laid along a row of points, an eye at each end."""
        if not self.fine:
            return
        if part:
            self.m.use(part)
        pts = [Vector(p) for p in points]
        self.tube(pts, r, paint, 6)
        if eyes:
            for end, nxt in ((pts[0], pts[1]), (pts[-1], pts[-2])):
                d = (end - nxt).normalized()
                _, side, up = frame(d)
                if abs(d.z) < 0.9:
                    side = d.cross(Z).normalized()
                c = end + d * 0.06
                ring = [c + d * (math.cos(t) * 0.06) + side * (math.sin(t) * 0.04) for t in (2 * math.pi * k / 10 for k in range(10))]
                self.tube(ring, r * 1.25, paint, 5, closed=True)

    def spare_links(self, a, b, count, pitch, wide, facing, paint="track", thick=0.03, part=None, horn=True):
        """A row of spare track links lying on a plate from a toward b, their backs to the plate."""
        if not self.fine:
            return
        a, b = Vector(a), Vector(b)
        f = Vector(facing).normalized()
        along = (b - a).normalized()
        across = f.cross(along)
        if part:
            self.m.use(part)
        for k in range(count):
            c = a + along * (pitch * (k + 0.5))
            m = Matrix((along, across, f)).transposed()
            bm = bm_box((pitch * 0.9, wide, thick), (0, 0, 0))
            bm_move(bm, at=c + f * (thick / 2), turn=m)
            self.put(bm, paint)
            bm = bm_box((pitch * 0.4, wide - 0.03, 0.012), (0, 0, 0))
            bm_move(bm, at=c + f * (thick + 0.006), turn=m)
            self.put(bm, paint)
            if horn:
                bm = bm_box((pitch * 0.4, 0.03, 0.05), (0, 0, 0))
                bm_move(bm, at=c + f * (thick + 0.03), turn=m)
                self.put(bm, paint)
        # the bar that holds them
        self.rod(a + f * (thick + 0.02) - across * (wide * 0.3), b + f * (thick + 0.02) - across * (wide * 0.3), 0.009, "steel", 5)

    # ---- engines: silencers, grilles -----------------------------------------------------------------
    def silencer(self, a, b, r, paint="rust", bands=2, pipe=None, part=None, tail=None, sides=20):
        """A silencer drum from a to b; `pipe` the points of its inlet pipe; `tail` (point, way) of its outlet stub."""
        a, b = Vector(a), Vector(b)
        if part:
            self.m.use(part)
        L = (b - a).length
        e = min(r * 0.35, L * 0.12)
        self.lathe(a, b - a, [(L, 0.0), (L, r * 0.6), (L - e, r), (e, r), (0.0, r * 0.6), (0.0, 0.0)], paint, self.n(sides, 8))
        if self.fine:
            for k in range(bands):
                t = L * (k + 1) / (bands + 1)
                self.lathe(a, b - a, [(t + 0.015, r), (t + 0.015, r * 1.035), (t - 0.015, r * 1.035), (t - 0.015, r)], "steel", self.n(sides, 8))
            if pipe:
                self.tube(pipe, r * 0.3, paint, 8)
        if tail:
            p, way = Vector(tail[0]), Vector(tail[1]).normalized()
            self.lathe(p, way, [(0.12, r * 0.2), (0.12, r * 0.26), (0.0, r * 0.26)], paint, 10)
            self.lathe(p, way, [(0.02, 0.0), (0.02, r * 0.2), (0.12, r * 0.2)], "black", 10)

    def grille(self, c, size, up=Z, along=X, slats=8, paint="upper", frame_w=0.025, deep=0.02, part=None, mesh=False):
        """An engine grille: a frame with slats over the dark of the engine room. `along` is the way the slats run."""
        c = Vector(c)
        n = Vector(up).normalized()
        u = Vector(along)
        u = (u - n * u.dot(n)).normalized()
        v = n.cross(u)
        M = Matrix((u, v, n)).transposed()
        if part:
            self.m.use(part)

        def bx(sz, at, p, bev=0.0):
            bm = bm_box(sz, (0, 0, 0))
            bm_move(bm, at=c + M @ Vector(at), turn=M)
            self.put(bm, p, bev)
        bx((size[0] - frame_w, size[1] - frame_w, 0.004), (0, 0, 0.002), "black")
        if not self.fine:
            bx((size[0], size[1], deep * 0.6), (0, 0, deep * 0.3), paint)
            return
        for s in (-1, 1):
            bx((size[0], frame_w, deep), (0, s * (size[1] - frame_w) / 2, deep / 2), paint, 0.004)
            bx((frame_w, size[1], deep), (s * (size[0] - frame_w) / 2, 0, deep / 2), paint, 0.004)
        for k in range(slats):
            t = (k + 0.5) / slats - 0.5
            bm = bm_box((size[0] - frame_w * 2, 0.006, deep * 1.3), (0, 0, 0))
            bm_move(bm, turn=Matrix.Rotation(math.radians(32.0), 3, "X"))
            bm_move(bm, at=c + M @ Vector((0, t * (size[1] - frame_w * 2), deep * 0.55)), turn=M)
            self.put(bm, paint)

    def louvres(self, c, size, facing, count=5, paint="upper", part=None, down=Z):
        """Pressed louvres in a side plate: a row of small hoods over slots."""
        if not self.fine:
            return
        c = Vector(c)
        f = Vector(facing).normalized()
        d = Vector(down).normalized()
        u = d.cross(f).normalized()
        if part:
            self.m.use(part)
        M = Matrix((u, d, f)).transposed()
        for k in range(count):
            t = (k + 0.5) / count - 0.5
            at = c + d * (t * size[1])
            bm = bm_box((size[0], size[1] / count * 0.55, 0.004), (0, 0, 0))
            bm_move(bm, at=at + f * 0.001, turn=M)
            self.put(bm, "black")
            bm = bm_box((size[0] + 0.02, 0.006, 0.03), (0, 0, 0))
            bm_move(bm, turn=Matrix.Rotation(math.radians(-35.0), 3, "X"))
            bm_move(bm, at=at - d * (size[1] / count * 0.3) + f * 0.012, turn=M)
            self.put(bm, paint)


def lerp_r(r0, r1, t):
    return r0 + (r1 - r0) * t
