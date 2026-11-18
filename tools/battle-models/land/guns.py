"""Field guns: the towed pieces the game's artillery units show, one for each army that has its own.

A towed gun stands as she fights: unlimbered, trail down, its spades in the ground. Her origin is on the ground under
the middle of her axle, x toward the muzzle (so she drops into a gun pit's `gun` socket as she is).

PARTS (names the game can rely on):
    vehicle (root)
      carriage (the axle and all that stands on it: it tips and jumps with the shot, and lifts to the limber)
        wheel_l, wheel_r (spin)
        trail_l, trail_r (a split trail's legs: + closes them for the road)
        platform (a 25-pounder's firing platform)
        traverse (the saddle and the shield: trains a few degrees each way, + to the right)
          handwheel_traverse, handwheel_elevate (spin)
          gun (the cradle: lifts at the trunnions, + muzzle up)
            barrel (runs back along its own length)  >  breech (the block: slides open)

CLIPS: idle, fire, hit, limber, unlimber, towed, knocked_out.
"""

import math

import bmesh
from mathutils import Matrix, Quaternion, Vector

import rigid
from motorpool import LEFT, RIGHT, SIDES, X, Y, Z, frame
from tanks_ger import GER_COATS, coats
from works import Works

coat = "upper"


def sheet(v, outline, holes, origin, u, w, thick, paint=coat, bevel=0.002):
    """A flat plate standing in any plane. `outline` and each of `holes` are (a, b) points going round, a point standing
    at origin + u * a + w * b; the plate is `thick` deep behind its face, and its face looks along u x w."""
    o, u, w = Vector(origin), Vector(u).normalized(), Vector(w).normalized()
    n = u.cross(w).normalized()
    bm = bmesh.new()

    def loop(pts):
        vs = [bm.verts.new(o + u * a + w * b) for a, b in pts]
        return [bm.edges.new((vs[i], vs[(i + 1) % len(vs)])) for i in range(len(vs))]
    edges = loop(outline)
    for h in (holes if v.fine else []):
        edges += loop(h)
    bmesh.ops.triangle_fill(bm, use_beauty=True, use_dissolve=False, edges=edges)
    front = bm.faces[:]
    rim = [e for e in bm.edges if len(e.link_faces) == 1]
    twin = {vt: bm.verts.new(vt.co - n * thick) for vt in bm.verts[:]}
    for f in front:
        bm.faces.new([twin[vt] for vt in reversed(f.verts)])
    for e in rim:
        a, b = e.verts
        bm.faces.new((a, b, twin[b], twin[a]))
    v.put(bm, paint, bevel, angle=30.0)


def edge_rivets(v, outline, origin, u, w, inset=0.03, every=0.1, r=0.008, least=0.16, skip=()):
    """Rivet heads along the edges of a sheet (those at least `least` long), set in from the edge. `skip`: edges
    (by number) left bare."""
    if not v.fine:
        return
    o, u, w = Vector(origin), Vector(u).normalized(), Vector(w).normalized()
    n = u.cross(w).normalized()
    k = len(outline)
    area = sum(outline[i][0] * outline[(i + 1) % k][1] - outline[(i + 1) % k][0] * outline[i][1] for i in range(k))
    turn = 1.0 if area > 0 else -1.0                    # (going round anticlockwise, the plate is on the left of each edge)
    for i in range(k):
        if i in skip:
            continue
        (a0, b0), (a1, b1) = outline[i], outline[(i + 1) % k]
        L = math.hypot(a1 - a0, b1 - b0)
        if L < least:
            continue
        # (set in toward the plate's middle)
        ta, tb = (a1 - a0) / L, (b1 - b0) / L
        na, nb = -tb * turn, ta * turn
        cnt = max(2, int(round((L - 2 * inset) / every)) + 1)
        for j in range(cnt):
            t = inset + (L - 2 * inset) * j / (cnt - 1)
            v.rivet(o + u * (a0 + ta * t + na * inset) + w * (b0 + tb * t + nb * inset) + n * 0.0005, n, r)


class GunWorks(Works):
    # ---- the root, the axle, the wheels ------------------------------------------------------------------
    def gun_root(self, z):
        """z: how high her axle stands (a wheel's radius, unless she stands on a firing platform)."""
        r = z
        self.part("vehicle", None, (0, 0, 0), kind="root", note="on the ground under the middle of her axle, x toward the muzzle; the game moves and turns this")
        self.part("carriage", "vehicle", (0, 0, r), axis=(0, -1, 0), limits=[-14.0, 14.0], kind="turn",
                  note="the axle and all that stands on it: + muzzle up about the axle (clips tip her about her spades and lift her trail to the limber)")
        self.axle_z = r
        return self

    def tyre(self, c, out, r, rr, w, lugs=22):
        """A pneumatic tyre: round shoulders, two grooves round the tread, bars across the shoulders."""
        c, out = Vector(c), Vector(out).normalized()
        h, t = w / 2.0, r - rr
        sides = self.n(40, 12)
        if not self.fine:
            self.lathe(c, out, [(h * 0.8, rr), (h, r - t * 0.3), (h * 0.6, r), (-h * 0.6, r), (-h, r - t * 0.3), (-h * 0.8, rr)], "rubber", sides)
            return
        g, gw, gd = h * 0.2, 0.008, 0.012
        prof = [(h * 0.72, rr), (h * 0.97, rr + t * 0.22), (h, rr + t * 0.5), (h * 0.96, r - t * 0.22), (h * 0.8, r - t * 0.06), (h * 0.6, r),
                (g + gw, r), (g + gw, r - gd), (g - gw, r - gd), (g - gw, r), (-g + gw, r), (-g + gw, r - gd), (-g - gw, r - gd), (-g - gw, r),
                (-h * 0.6, r), (-h * 0.8, r - t * 0.06), (-h * 0.96, r - t * 0.22), (-h, rr + t * 0.5), (-h * 0.97, rr + t * 0.22), (-h * 0.72, rr)]
        self.lathe(c, out, prof, "rubber", sides)
        _, side, up = frame(out)
        for k in range(lugs * 2):                         # the bars across each shoulder, the two sides out of step
            sgn = 1.0 if k % 2 == 0 else -1.0
            a = math.pi * k / lugs
            d = side * math.cos(a) + up * math.sin(a)
            tan = out.cross(d)
            lean = (d * 0.92 - out * (sgn * -0.38)).normalized()          # the bar lies over the shoulder's curve
            at = c + out * (sgn * h * 0.74) + d * (r - t * 0.035)
            self.box((0.02, h * 0.42, 0.03), at, "rubber", turn=Matrix((lean, tan.cross(lean).normalized(), tan)).transposed())

    def gun_wheel(self, letter, s, y, r, kind="pneumatic", w=0.2, rr=None, holes=0, bolts=6, spokes=12):
        """One wheel on the end of the axle, a part of its own. kind: pneumatic (a lorry's tyre on a dished steel
        disc), solid (a big cast wheel with a solid rubber tyre), spoked (a wooden wheel in a steel tyre)."""
        name = f"wheel_{letter}"
        c, out = Vector((0.0, s * y, self.axle_z)), Vector((0.0, s, 0.0))
        self.part(name, "carriage", c, axis=Y, kind="spin", note=f"rolls: + forward; radius {r:.3f} m")
        self.wheels.append((name, r))
        if kind == "pneumatic":
            rr = rr or r * 0.5
            self.tyre(c, out, r, rr, w)
            self.wheel_disc(c + out * (w * 0.1), out, rr * 1.05, w * 0.6, tyre=0.0, kind="dish", hub=0.42, bolts=bolts, holes=holes, dish=0.2, cap=0.75, sides=32)
        elif kind == "solid":
            self.wheel_disc(c, out, r, w, tyre=0.05, kind="dish", hub=0.27, bolts=bolts, holes=holes, dish=0.32, cap=0.7, sides=44)
            self.wheel_disc(c - out * (w * 0.2), out, r - 0.05, w * 0.5, tyre=0.0, kind="flat", hub=0.27, bolts=0, face=-1.0, sides=self.n(28, 10), simple=True)
        else:   # spoked: steel tyre, wooden felloe, spokes into a nave bound with steel
            h = w / 2.0
            sides = self.n(44, 12)
            self.lathe(c, out, [(h, r - 0.012), (h, r), (-h, r), (-h, r - 0.012)], "steel", sides)
            self.lathe(c, out, [(h * 0.9, r - 0.085), (h * 0.9, r - 0.012), (-h * 0.9, r - 0.012), (-h * 0.9, r - 0.085), (h * 0.9, r - 0.085)], coat, sides)
            nave = 0.105
            if not self.fine:                              # (the plain wheel: a thin web where the spokes are)
                self.lathe(c, out, [(0.012, 0.0), (0.012, r - 0.085), (-0.012, r - 0.085), (-0.012, 0.0)], coat, sides)
            self.spokes(c, out, nave * 0.9, r - 0.08, spokes, 0.062, 0.05, coat)
            n2 = self.n(20, 8)
            self.lathe(c, out, [(0.2, 0.0), (0.2, 0.05), (0.16, 0.058), (0.13, nave * 0.86), (0.05, nave * 1.05), (-0.05, nave * 1.05), (-0.12, nave * 0.86), (-0.12, 0.0)], coat, n2)
            self.lathe(c + out * 0.052, out, [(0.012, nave * 1.05), (0.012, nave * 1.5), (0.0, nave * 1.5), (0.0, nave * 1.05)], "steel", n2)
            self.bolts(c + out * 0.064, out, nave * 1.3, spokes, r=0.011, high=0.007, paint="steel")
            self.lathe(c + out * 0.2, out, [(0.03, 0.0), (0.03, 0.034), (0.0, 0.04), (0.0, 0.0)], "steel", self.n(10, 6))
        return name

    def axle(self, y_in, r=0.05, box=(0.24, 0.38, 0.13, 0.11)):
        """The axle between the wheels and the bottom carriage on it: box = (half length, half width, down, up)."""
        self.use("carriage")
        z = self.axle_z
        self.rod((0, -y_in, z), (0, y_in, z), r, "shade", 14)
        for s in (LEFT, RIGHT):                           # the collar inside each wheel
            self.rod((0, s * (y_in - 0.06), z), (0, s * y_in, z), r * 1.5, "shade", 14)
        bx, by, dn, up = box
        self.slab((-bx, -by, z - dn), (bx, by, z + up), coat, 0.012, 2)
        self.lathe((0, 0, z + up), Z, [(0.035, 0.0), (0.035, by * 0.74), (0.0, by * 0.8), (0.0, 0.0)], "shade", self.n(28, 10))       # the race the saddle turns on
        self.pintle = Vector((0.0, 0.0, z + up + 0.035))

    # ---- the saddle, the cradle, the piece -------------------------------------------------------------------
    def saddle(self, xt, zt, cy, half_traverse, rate=4.0, thick=0.018, foot=0.3, lighten=True):
        """The top carriage: a turntable and two cheeks that carry the trunnions at (xt, +-cy, zt)."""
        p = self.pintle
        self.part("traverse", "carriage", p, axis=(0, 0, -1), limits=[-half_traverse, half_traverse], kind="turn",
                  note="trains on the bottom carriage: + to the right (clockwise from above); the game lays it inside its limits")
        self.lathe(p, Z, [(0.03, 0.0), (0.03, cy + 0.07), (0.0, cy + 0.09), (0.0, 0.0)], coat, self.n(28, 10))
        zb = p.z + 0.02
        for s in (LEFT, RIGHT):
            out = [(-foot, zb), (foot * 0.9, zb), (xt + 0.13, zt - 0.05), (xt + 0.13, zt + 0.075), (xt - 0.13, zt + 0.075), (xt - 0.36, zb + (zt - zb) * 0.5), (-foot, zb + 0.12)]
            holes = []
            if lighten and zt - zb > 0.3:
                mx, mz = xt - 0.06, zb + (zt - zb) * 0.45
                holes = [[(mx - 0.09, mz - 0.07), (mx + 0.1, mz - 0.07), (mx + 0.04, mz + 0.1), (mx - 0.09, mz + 0.06)]]
            sheet(self, out, holes, (0, cy if s > 0 else -cy - thick, 0), X, Z, thick, coat, 0.004)       # (a sheet in this plane is `thick` deep toward +y)
            # the trunnion cap, bolted down over the pin
            self.lathe((xt, s * (cy + thick), zt), (0, s, 0), [(0.02, 0.0), (0.02, 0.058), (0.0, 0.07), (0.0, 0.0)], coat, self.n(18, 8))
            self.bolts((xt, s * (cy + thick + 0.02), zt), (0, s, 0), 0.04, 4, r=0.009, high=0.006)
            self.slab((xt - 0.12, s * cy, zt + 0.07), (xt + 0.12, s * (cy + thick), zt + 0.095), coat, 0.004)
        self.slab((-foot * 0.8, -cy, zb), (-foot * 0.55, cy, zb + 0.1), coat, 0.006)                       # the transoms between the cheeks
        self.slab((foot * 0.5, -cy, zb), (foot * 0.75, cy, zb + 0.1), coat, 0.006)
        self.m.info.setdefault("file", {}).setdefault("controls", {})["traverse"] = {
            "part": "traverse", "what": "trained by the game inside its limits: + to the right", "degrees_per_second": rate}
        self.xt, self.zt, self.cy = xt, zt, cy

    def cradle(self, low, high, w, h, ahead, behind, drop, over=None, arc=True, round_under=False):
        """The cradle the piece slides in: a trough `w` wide and `h` deep under the barrel (its top `drop` below the
        bore), from `behind` the trunnions to `ahead` of them. over: (from, length, radius, rise) a recuperator
        cylinder carried over the barrel."""
        xt, zt, cy = self.xt, self.zt, self.cy
        self.part("gun", "traverse", (xt, 0.0, zt), axis=(0, -1, 0), limits=[low, high], kind="turn", note="the cradle lifts at the trunnions: + muzzle up; the game lays it")
        top = zt - drop
        self.rod((xt, -cy - 0.012, zt), (xt, cy + 0.012, zt), 0.042, "shade", 14)                            # the trunnion pin
        if round_under:      # (a recoil cylinder as the cradle: round below, flat rails above)
            n = self.n(12, 4)
            prof = [(-w / 2, top)] + [(-w / 2 * math.cos(math.pi * i / n), top - h * 0.45 - h * 0.55 * math.sin(math.pi * i / n)) for i in range(n + 1)] + [(w / 2, top)]
            bm_pts = [(y, z) for y, z in prof]
            self.put(_bm_section(bm_pts, xt - behind, xt + ahead), coat, 0.006)
        else:
            self.slab((xt - behind, -w / 2, top - h), (xt + ahead, w / 2, top), coat, 0.012, 2)
        for s in (LEFT, RIGHT):                             # the slide rails, and the arms up to the trunnions
            self.slab((xt - behind, s * (w / 2 - 0.035), top), (xt + ahead, s * (w / 2), top + 0.018), "shade", 0.003)
            self.hull([(xt - 0.16, s * (w / 2 - 0.004), top - h * 0.7), (xt + 0.16, s * (w / 2 - 0.004), top - h * 0.7), (xt + 0.09, s * (cy - 0.004), zt + 0.05), (xt - 0.09, s * (cy - 0.004), zt + 0.05),
                       (xt - 0.16, s * (w / 2 - 0.03), top - h * 0.7), (xt + 0.16, s * (w / 2 - 0.03), top - h * 0.7), (xt + 0.09, s * (cy - 0.03), zt + 0.05), (xt - 0.09, s * (cy - 0.03), zt + 0.05)],
                      coat, 0.005, both=False)
        # the front of it: a cap plate with the heads of the buffer and the recuperator rods
        self.slab((xt + ahead, -w / 2 - 0.006, top - h - 0.006), (xt + ahead + 0.02, w / 2 + 0.006, top + 0.004), coat, 0.004)
        if self.fine:
            for yy in (-w * 0.22, w * 0.22):
                self.lathe((xt + ahead + 0.02, yy, top - h * 0.5), X, [(0.03, 0.0), (0.03, 0.02), (0.012, 0.03), (0.0, 0.03), (0.0, 0.0)], "shade", 6, False)
            self.bolts((xt + ahead + 0.02, 0.0, top - h * 0.5), X, min(w, h) * 0.44, 6, r=0.009, high=0.006)
        if arc:      # the elevating arc: a toothed sector under the cradle, on the right
            ra = min(zt - self.pintle.z - 0.05, h + drop + 0.2)
            sweep = list(range(-128, -51, 7))
            pts = [(xt + ra * math.cos(math.radians(a)), zt + ra * math.sin(math.radians(a))) for a in sweep]
            pts += [(xt + (ra - 0.05) * math.cos(math.radians(a)), zt + (ra - 0.05) * math.sin(math.radians(a))) for a in reversed(sweep)]
            sheet(self, pts, [], (0, -(w / 2 - 0.02), 0), X, Z, 0.03, "shade", 0.002)
        if over:
            x0, L, r, rise = over
            c0 = Vector((xt + x0, 0.0, zt + rise))
            n = self.n(20, 8)
            self.lathe(c0, X, [(L, 0.0), (L, r * 0.6), (L - 0.02, r), (0.02, r), (0.0, r * 0.7), (0.0, 0.0)], coat, n)
            self.lathe(c0 + X * L, X, [(0.035, 0.0), (0.035, r * 0.3), (0.0, r * 0.36), (0.0, 0.0)], "shade", 6, False)
            if self.fine:
                for xx in (L * 0.12, L * 0.86):               # the bands that hold it, and their feet on the cradle
                    self.lathe(c0 + X * xx, X, [(0.05, r), (0.05, r + 0.008), (0.0, r + 0.008), (0.0, r)], "shade", n)
                    for s in (LEFT, RIGHT):
                        self.slab((c0.x + xx, s * (w / 2 - 0.03), top), (c0.x + xx + 0.05, s * (w / 2), c0.z), "shade", 0.003)
        self.cradle_top, self.cradle_w, self.cradle_h, self.cradle_ahead = top, w, h, ahead
        self.m.info.setdefault("file", {}).setdefault("controls", {})["gun"] = {"part": "gun", "what": "laid by the game inside its limits: + muzzle up"}

    def piece(self, L, bore, back, r0, r1, recoil, brake=None, steps=(), ring=(0.28, 0.3, 0.3), block="right", round_ring=False, lug=0.5, swell=None):
        """The barrel in its cradle: `L` long from the breech face, the breech face `back` behind the trunnions.
        ring: the breech ring (long, wide, high); block: the way the breech block slides open, or 'screw' for a
        round breech with a swinging screw. Each kind has its own face, since the battle camera sits behind a gun:
        'down' is a squarish ring, two upright jaws with the block standing between them, a loading notch in its
        head; 'right' is a wider oblong ring, a jaw above and one below, the block lying between them and standing
        out of the ring's right side."""
        xt, zt = self.xt, self.zt
        xb = xt - back
        self.part("barrel", "gun", (xt, 0.0, zt), slide=(-1, 0, 0), slide_limits=[0.0, recoil], axis=(0, -1, 0), kind="slide", note="runs back along its own length when she fires (metres)")
        self.barrel((xb, 0.0, zt), L, bore, r0, r1, brake, steps)
        if swell:                                           # (long, radius): the muzzle swells to a ring
            sl, sr = swell
            self.lathe((xb + L - sl, 0.0, zt), X, [(sl - 0.003, r1 * 0.98), (sl - 0.003, sr - 0.006), (sl - 0.012, sr), (sl * 0.32, sr), (0.0, r1 * 0.98)], coat, self.n(24, 8))
        bl, bw, bh = ring
        if round_ring:
            n = self.n(24, 10)
            self.lathe((xb - 0.03, 0.0, zt), X, [(bl, r0), (bl, bh / 2 * 0.92), (bl - 0.015, bh / 2), (0.015, bh / 2), (0.0, bh / 2 * 0.9), (0.0, 0.0)], coat, n)
        else:
            # a forged ring with its corners cut away: whole round the barrel ahead, and behind that two jaws with
            # the block's mortise between them (upright jaws for a block that drops, level ones for one that slides)
            bw, bh = (bw, bh * 1.12) if block == "down" else (bw * 1.24, bh * 0.88)
            c = min(bw, bh) * (0.1 if block == "down" else 0.16)
            xr, xm = xb - 0.03, xb - 0.03 + bl * 0.5
            sec = [(-bw / 2 + c, -bh / 2), (bw / 2 - c, -bh / 2), (bw / 2, -bh / 2 + c), (bw / 2, bh / 2 - c), (bw / 2 - c, bh / 2), (-bw / 2 + c, bh / 2), (-bw / 2, bh / 2 - c), (-bw / 2, -bh / 2 + c)]
            self.put(_bm_section([(y, zt + z) for y, z in sec], xm, xr + bl), coat, 0.008)
            if block == "down":
                jaw = [(bw * 0.22, -bh / 2), (bw / 2 - c, -bh / 2), (bw / 2, -bh / 2 + c), (bw / 2, bh / 2 - c), (bw / 2 - c, bh / 2), (bw * 0.22, bh / 2)]
                jaws = [jaw, [(-y, z) for y, z in reversed(jaw)]]
            else:
                jaw = [(-bw / 2 + c, bh / 2), (-bw / 2, bh / 2 - c), (-bw / 2, bh * 0.22), (bw / 2, bh * 0.22), (bw / 2, bh / 2 - c), (bw / 2 - c, bh / 2)]
                jaws = [jaw, [(y, -z) for y, z in reversed(jaw)]]
            for j in jaws:
                self.put(_bm_section([(y, zt + z) for y, z in j], xr, xm + 0.004), coat, 0.006)
            if self.fine:                                   # the collar the barrel screws into, the eyes she is lifted by
                self.lathe((xr + bl, 0.0, zt), X, [(0.05, r0 + 0.004), (0.05, r0 + 0.018), (0.0, r0 + 0.026), (0.0, r0 + 0.004)], coat, self.n(24, 10))
                if block != "down":
                    for sy in (LEFT, RIGHT):
                        self.lathe((xr + bl * 0.75, sy * bw * 0.2, zt + bh / 2), Z, [(0.012, 0.0), (0.012, 0.016), (0.0, 0.02), (0.0, 0.0)], "shade", 6, False)
        # the lugs under the barrel that ride the cradle's rails
        for xx in (xb + 0.05, xb + lug):
            self.slab((xx, -self.cradle_w / 2 + 0.04, self.cradle_top + 0.018), (xx + 0.16, self.cradle_w / 2 - 0.04, zt - r0 * 0.5), "shade", 0.004)
        self.m.info["bore"], self.m.info["brake"] = bore, bool(brake)
        self.socket("muzzle", (xb + L, 0.0, zt), X, note="the muzzle: flash, blast")
        self.socket("breech", (xb - 0.05, 0.0, zt), -X, note="the breech face: where the round goes in")
        way = {"right": Vector((0, -1, 0)), "down": Vector((0, 0, -1)), "screw": Vector((0, -1, 0))}[block]
        open_by = 0.16 if block != "screw" else 0.1
        self.part("breech", "barrel", (xb, 0.0, zt), slide=tuple(way), slide_limits=[0.0, open_by], kind="slide", note="the breech block: slides open (metres)")
        if block == "screw":
            self.lathe((xb - 0.05, 0.0, zt), X, [(0.03, 0.0), (0.03, r0 * 0.8), (0.0, r0 * 0.86), (0.0, 0.0)], "shade", self.n(18, 8))
            self.tube([(xb - 0.05, 0.02, zt + r0 * 0.3), (xb - 0.09, 0.04, zt + r0 * 0.6), (xb - 0.09, 0.12, zt + r0 * 0.9)], 0.012, "steel", 6)
        else:
            side = Vector((0, 1, 0)) if block == "down" else Vector((0, 0, 1))
            rn = bore * 0.62                                # the notch the round is fed through: half a circle cut out of the block
            arc = [(rn * math.cos(math.radians(a)), -rn * math.sin(math.radians(a))) for a in (0.0, 30.0, 60.0, 90.0, 120.0, 150.0, 180.0)]
            if block == "down":                             # it stands a little out of the foot of its mortise; the notch is in its head
                y1, z0, z1 = bw * 0.215, -bh / 2 - 0.02, bh / 2 - 0.025
                blk = [(-y1, z0), (y1, z0), (y1, z1)] + [(y, z1 + z) for y, z in arc] + [(-y1, z1)]
            else:                                           # it stands out of the ring's right side; the notch is in its left end
                z1, y0, y1 = bh * 0.215, -bw / 2 - 0.035, bw / 2 - 0.012
                blk = [(y0, -z1), (y1, -z1)] + [(y1 + z, y) for y, z in reversed(arc)] + [(y1, z1), (y0, z1)]
            self.put(_bm_section([(y, zt + z) for y, z in blk], xr + 0.012, xm + 0.002), "shade", 0.004)
            if self.fine:                                   # the firing lock in the block's face
                self.lathe((xr + 0.012, 0.0, zt), -X, [(0.016, 0.0), (0.016, 0.017), (0.0, 0.022), (0.0, 0.0)], "steel", 8)
            # the lever that throws it
            a = Vector((xb - 0.03 + bl * 0.4, 0.0, zt)) + side * ((bw if block == "down" else bh) / 2)
            self.tube([a, a + side * 0.04 - X * 0.02, a + side * 0.06 - X * 0.14], 0.011, "steel", 6)
            self.ball(a + side * 0.06 - X * 0.15, 0.02, "steel", 8, 6)
        self.xb = xb
        return xb

    def handwheels(self, at_e, at_t, r=0.11):
        """The layer's two handwheels on the left of the saddle, each a part that spins."""
        for name, at, ax in (("handwheel_elevate", at_e, Vector((0, 1, 0))), ("handwheel_traverse", at_t, Vector((-0.75, 0.25, 0.6)).normalized())):
            at = Vector(at)
            self.use("traverse")
            self.rod(at - ax * 0.22, at, 0.014, "steel", 8)
            self.part(name, "traverse", at, axis=tuple(ax), kind="spin", note="the layer's handwheel: spins as the gun is laid")
            _, side, up = frame(ax)
            n = self.n(20, 8)
            self.tube([at + (side * math.cos(2 * math.pi * k / n) + up * math.sin(2 * math.pi * k / n)) * r for k in range(n)], 0.011, "steel", 6, closed=True)
            for k in range(3):
                d = side * math.cos(2 * math.pi * k / 3) + up * math.sin(2 * math.pi * k / 3)
                self.rod(at, at + d * r, 0.008, "steel", 6, caps=False)
            self.lathe(at - ax * 0.015, ax, [(0.035, 0.0), (0.035, 0.022), (0.0, 0.026), (0.0, 0.0)], "steel", 8)
            self.rod(at + side * r, at + side * r + ax * 0.075, 0.012, "wood", 8)
        self.use("traverse")

    def sight(self, at, high=0.3):
        """The dial sight on its bracket, left of the breech."""
        at = Vector(at)
        self.use("traverse")
        self.rod(at, at + Z * high, 0.016, "steel", 8)
        self.slab(at + Vector((-0.035, -0.03, high)), at + Vector((0.035, 0.03, high + 0.07)), "shade", 0.006)
        self.rod(at + Z * (high + 0.07), at + Z * (high + 0.15), 0.02, "shade", 10)
        self.rod(at + Vector((-0.09, 0.0, high + 0.035)), at + Vector((-0.035, 0.0, high + 0.035)), 0.014, "shade", 8)
        self.lathe(at + Vector((-0.09, 0.0, high + 0.035)), -X, [(0.002, 0.0), (0.002, 0.011)], "glass", 8)
        self.lathe(at + Z * (high + 0.15), X, [(0.022, 0.0), (0.022, 0.012)], "glass", 8)

    def seat(self, at, s, part="carriage"):
        """A crewman's iron seat on a bracket."""
        at = Vector(at)
        self.use(part)
        n = self.n(14, 6)
        self.lathe(at, Z, [(0.012, 0.0), (0.012, 0.15), (0.03, 0.17), (0.0, 0.16), (0.0, 0.0)], coat, n)
        self.tube([at, at - Z * 0.12 - Y * (s * 0.1), at - Z * 0.2 - Y * (s * 0.2)], 0.014, coat, 6)

    # ---- the shield --------------------------------------------------------------------------------------
    def shield(self, x, half, z0, z1, lean=0.0, slot=None, corner=0.0, top=None, apron=None, wing=None, port=None, thick=0.006, split=None, stays=True, part="traverse"):
        """A gun shield on the saddle: a plate across her at x from z0 to z1, leaning back `lean` degrees.
        slot: (half width, foot, head or None for open to the top) the opening the piece looks through;
        corner: how much of each upper corner is cut away; top: (height, lean) a flap along the head, leaning back
        farther; apron: (depth, swing) a flap hung along the foot; wing: (width, sweep, foot) a wing each side, swept
        back, its foot at that height; port: (y, z, w, h) the layer's sight port; split: half the gap of a shield in two halves."""
        self.use(part)
        sl, cl = math.sin(math.radians(lean)), math.cos(math.radians(lean))
        O, u, w = Vector((x, 0.0, z0)), Y, Vector((-sl, 0.0, cl))
        Hc = (z1 - z0) / cl
        hb = lambda z: (z - z0) / cl           # noqa: E731  (a height as a distance up the plate)
        holes = []
        if split:
            for s in (LEFT, RIGHT):
                out = [(s * split, 0.0), (s * half, 0.0), (s * half, Hc - corner), (s * (half - corner), Hc), (s * split, Hc)]
                sheet(self, out, [], O, u, w, thick)
                edge_rivets(self, out, O, u, w)
        else:
            sw, s0, s1 = slot if slot else (0.0, 0.0, 0.0)
            right = [(half, 0.0), (half, Hc - corner), (half - corner, Hc)] if corner > 0 else [(half, 0.0), (half, Hc)]
            left = [(-(half - corner), Hc), (-half, Hc - corner), (-half, 0.0)] if corner > 0 else [(-half, Hc), (-half, 0.0)]
            if slot and s1 is None:
                out = right + [(sw, Hc), (sw, hb(s0)), (-sw, hb(s0)), (-sw, Hc)] + left
            else:
                out = right + left
                if slot:
                    holes.append([(-sw, hb(s0)), (sw, hb(s0)), (sw, hb(s1)), (-sw, hb(s1))])
            if port:
                py, pz, pw, ph = port
                holes.append([(py - pw / 2, hb(pz) - ph / 2), (py + pw / 2, hb(pz) - ph / 2), (py + pw / 2, hb(pz) + ph / 2), (py - pw / 2, hb(pz) + ph / 2)])
            sheet(self, out, holes, O, u, w, thick)
            edge_rivets(self, out, O, u, w)
            if port and self.fine:                         # the port's sliding cover, pushed up
                py, pz, pw, ph = port
                c = O + u * py + w * (hb(pz) + ph * 0.75) + u.cross(w) * 0.004
                self.box((0.004, pw + 0.03, ph + 0.02), c, coat, turn=Matrix((u.cross(w), u, w)).transposed())
        nrm = u.cross(w)
        gap = split if split else (slot[0] if slot and slot[2] is None else 0.0)
        if top:
            th, tl = top
            a = math.radians(lean + tl)
            w2 = Vector((-math.sin(a), 0.0, math.cos(a)))
            O2 = O + w * Hc
            cut = corner
            for s in ((LEFT, RIGHT) if gap > 0 else (0.0,)):
                if gap > 0:
                    out = [(s * gap, 0.0), (s * (half - cut), 0.0), (s * (half - cut - th * 0.35), th), (s * gap, th)]
                else:
                    out = [(-(half - cut), 0.0), (half - cut, 0.0), (half - cut - th * 0.35, th), (-(half - cut - th * 0.35), th)]
                sheet(self, out, [], O2, u, w2, thick)
                edge_rivets(self, out, O2, u, w2, skip=(0,))
                if self.fine:                                # the hinges it folds on
                    for f in ((0.3, 0.8) if gap > 0 else (-0.6, -0.2, 0.2, 0.6)):
                        yy = (s * (gap + (half - cut - gap) * f)) if gap > 0 else (half - cut) * f
                        self.rod(O2 + u * (yy - 0.04) + nrm * 0.006, O2 + u * (yy + 0.04) + nrm * 0.006, 0.009, coat, 6)
        if apron:
            d, swing = apron
            a = math.radians(swing)
            w3 = Vector((-math.sin(a), 0.0, -math.cos(a)))
            out = [(-half * 0.96, 0.0), (half * 0.96, 0.0), (half * 0.96, d), (-half * 0.96, d)]
            sheet(self, out, [], O - Z * 0.008, -u, w3, thick)
            edge_rivets(self, out, O - Z * 0.008, -u, w3, skip=(0,))
            if self.fine:
                for f in (-0.7, -0.25, 0.25, 0.7):
                    self.rod(O + u * (half * f - 0.04) + X * 0.005, O + u * (half * f + 0.04) + X * 0.005, 0.009, coat, 6)
        if wing:
            ww, sweep, foot = wing
            a = math.radians(sweep)
            for s in (LEFT, RIGHT):
                uw = Vector((-math.sin(a), s * math.cos(a), 0.0))
                Ow = O + u * (s * half)
                b0 = hb(foot)
                out = [(0.0, b0), (ww, b0 + 0.02), (ww, Hc - 0.07), (ww - 0.06, Hc - 0.01), (0.0, Hc)]
                if s < 0:                                    # (the right wing: the same plate, told from its outer edge inward, so its face looks forward too)
                    out, uw = [(-p[0], p[1]) for p in out], -uw
                sheet(self, out, [], Ow, uw, w, thick)
                edge_rivets(self, out, Ow, uw, w)
        if stays and self.fine:                              # two stays from her back to the saddle
            for s in (LEFT, RIGHT):
                yy = s * max(gap + 0.1, min(half * 0.72, self.cy + 0.16))
                top_at = O + u * yy + w * (Hc * 0.82) - nrm * thick
                self.rod(top_at, (self.xt - 0.2, s * (self.cy + 0.01), self.pintle.z + 0.14), 0.012, coat, 6)
        self.socket("hit.shield", O + w * (Hc * 0.6) + u * (half * 0.5), nrm, part=part)
        return O, u, w, Hc

    # ---- trails ------------------------------------------------------------------------------------------
    def spade(self, at, back, wide=0.4, deep=0.24, sole=0.3):
        """A spade on a trail's end: a sole plate that sits on the ground and a blade that digs in below it."""
        at, back = Vector(at), Vector(back).normalized()
        side = Z.cross(back).normalized()
        T = Matrix((back, side, Z)).transposed()
        self.box((sole, wide, 0.022), Vector((at.x, at.y, 0.03)) - back * (sole * 0.3), coat, 0.004, turn=T)
        lean = (Z * -1.0 + back * 0.25).normalized()
        Tb = Matrix((side.cross(lean).normalized(), side, lean)).transposed()
        self.box((0.02, wide, deep + 0.1), Vector((at.x, at.y, 0.03)) + back * (sole * 0.2) + lean * ((deep + 0.1) / 2 - 0.08), coat, 0.004, turn=Tb)
        if self.fine:
            for f in (-0.32, 0.0, 0.32):                    # the ribs that brace the blade to the sole
                p = Vector((at.x, at.y, 0.04)) + side * (wide * f)
                self.hull([p - back * (sole * 0.5), p + back * (sole * 0.18), p + back * (sole * 0.18) + Z * 0.1,
                           p - back * (sole * 0.5) + side * 0.012, p + back * (sole * 0.18) + side * 0.012, p + back * (sole * 0.18) + Z * 0.1 + side * 0.012], coat, 0.0, both=False)

    def lunette(self, at, back, r=0.055):
        """The towing eye on the end of a trail."""
        at, back = Vector(at), Vector(back).normalized()
        side = Z.cross(back).normalized()
        self.rod(at, at + back * 0.12, 0.028, "steel", 8)
        c = at + back * (0.12 + r)
        n = self.n(16, 8)
        self.tube([c + (back * math.cos(2 * math.pi * k / n) + side * math.sin(2 * math.pi * k / n)) * r for k in range(n)], 0.02, "steel", 8, closed=True)
        self.socket("tow", c, back, note="the towing eye: it goes on the lorry's hook")

    def split_trail(self, letter, s, hinge, L, spread, sec0=(0.2, 0.24), sec1=(0.13, 0.15), tube=None, end_z=0.13, eye=False):
        """One leg of a split trail, drawn spread for action: from its hinge on the bottom carriage back `L` metres
        and out `spread` degrees, to a spade. tube: (r at the hinge, r at the end) for a round leg. The part turns
        about its hinge pin: + closes it for the road."""
        H = Vector(hinge)
        a = math.radians(spread)
        d = Vector((-math.cos(a), s * math.sin(a), 0.0))
        E = Vector((H.x, H.y, end_z)) + d * L
        E.z = end_z
        along = (E - H).normalized()
        side = Z.cross(along).normalized()
        up = along.cross(side).normalized()
        toe = math.degrees(math.atan2(abs(H.y) - sec1[0] * 0.55, L))          # (closed, the two ends lie side by side)
        self.use("carriage")
        n = self.n(16, 8)
        self.lathe(H - Z * (sec0[1] / 2 + 0.03), Z, [(sec0[1] + 0.06, 0.0), (sec0[1] + 0.06, 0.05), (0.0, 0.05), (0.0, 0.0)], "shade", n)      # the hinge pin
        self.slab((H.x - 0.02, min(0.0, H.y) if s < 0 else 0.0, H.z - sec0[1] / 2 - 0.02), (H.x + 0.2, max(0.0, H.y) if s > 0 else 0.0, H.z - sec0[1] / 2), coat, 0.004)
        self.slab((H.x - 0.02, min(0.0, H.y) if s < 0 else 0.0, H.z + sec0[1] / 2), (H.x + 0.2, max(0.0, H.y) if s > 0 else 0.0, H.z + sec0[1] / 2 + 0.02), coat, 0.004)
        name = f"trail_{letter}"
        self.part(name, "carriage", H, axis=(0, 0, s), limits=[0.0, round(spread + toe, 2)], kind="turn", note="a leg of the trail: + closes it for the road (0 is spread for action)")
        if tube:
            r0, r1 = tube
            self.rod(H + along * 0.02, E, r0, coat, self.n(18, 8), r2=r1)
            self.lathe(H, Z, [(sec0[1] / 2, 0.0), (sec0[1] / 2, r0 * 1.5), (-sec0[1] / 2, r0 * 1.5), (-sec0[1] / 2, 0.0)], coat, n)
            if self.fine:
                for f in (0.14, 0.5, 0.86):
                    rr = r0 + (r1 - r0) * f
                    self.lathe(H + along * (L * f), along, [(0.05, rr), (0.05, rr + 0.008), (0.0, rr + 0.008), (0.0, rr)], "shade", self.n(18, 8))
        else:
            (w0, h0), (w1, h1) = sec0, sec1
            pts = [H + side * (sa * w0 / 2) + up * (sb * h0 / 2) for sa in (1, -1) for sb in (1, -1)] + [E + side * (sa * w1 / 2) + up * (sb * h1 / 2) for sa in (1, -1) for sb in (1, -1)]
            self.hull(pts, coat, 0.01, both=False)
            self.lathe(H, Z, [(h0 / 2, 0.0), (h0 / 2, w0 * 0.62), (-h0 / 2, w0 * 0.62), (-h0 / 2, 0.0)], coat, n)
            if self.fine:
                cnt = int(L / 0.14)
                for sa in (1, -1):                             # rivets along the top plate's two edges
                    for k in range(1, cnt):
                        f = k / cnt
                        wd, ht = w0 + (w1 - w0) * f, h0 + (h1 - h0) * f
                        self.rivet(H + (E - H) * f + side * (sa * (wd / 2 - 0.02)) + up * (ht / 2), up, 0.008)
        # grab handles for the men who swing her, a spade, the eye or its latch
        outw = side * (-s)                                 # (away from her middle)
        half = (lambda f: tube[0] + (tube[1] - tube[0]) * f) if tube else (lambda f: (sec0[0] + (sec1[0] - sec0[0]) * f) / 2)
        high = (lambda f: tube[0] + (tube[1] - tube[0]) * f) if tube else (lambda f: (sec0[1] + (sec1[1] - sec0[1]) * f) / 2)
        self.handle(H + (E - H) * 0.62 + outw * (half(0.62) - 0.004), H + (E - H) * 0.76 + outw * (half(0.76) - 0.004), outw, r=0.011, reach=0.07)
        self.handle(H + (E - H) * 0.88 + up * (high(0.88) - 0.004), H + (E - H) * 0.97 + up * (high(0.97) - 0.004), up, r=0.011, reach=0.08)
        self.spade(E + along * 0.05, along)
        if eye:
            self.lunette(E + along * 0.1 + Z * 0.08, along)
        elif self.fine:
            self.slab(E + along * 0.02 + Vector((-0.04, -0.04, 0.02)), E + along * 0.02 + Vector((0.04, 0.04, 0.1)), "shade", 0.006)
        self.socket("spade." + ("left" if s > 0 else "right"), (E.x, E.y, 0.0), -Z, note="where the spade digs in")
        self.use("carriage")
        return E, spread + toe

    # ---- what a gun does ---------------------------------------------------------------------------------
    def gun_clips(self, recoil, trail_x, lift=0.05, close=None, hitch=0.62, eye_z=0.2, breech=0.16, cue="field_gun_fire", platform=None, track=1.5, low=-5.0, sink=0.0):
        """The clips a towed gun has. trail_x: where her spades are (x); lift: how far her wheels jump off the ground
        at the shot; close: how far each leg of a split trail turns to close; hitch: how high the lorry's hook is."""
        m = self.m
        C = m.parts[m.part_at["carriage"]].pivot
        R = C.z
        Lt = abs(trail_x)
        ev = lambda t, fx, socket=None, cue=None: {"t": round(t, 3), "fx": fx, "socket": socket, "cue": cue}       # noqa: E731
        up_axis = Vector((0, -1, 0))

        def about(P, axis, deg):
            q = Quaternion(axis, math.radians(deg))
            P = Vector(P)
            return q, (P - C) - q @ (P - C)

        def keys(long, fn, fps=30):
            rk, mk = [], []
            n = max(2, int(round(long * fps)))
            for i in range(n + 1):
                t = long * i / n
                q, mv = fn(t)
                rk.append([round(t, 4), q.w, q.x, q.y, q.z])
                mk.append([round(t, 4), mv.x, mv.y, mv.z])
            return {"part": "carriage", "rot": rk, "move": mk}

        m.clips["idle"] = {"long": 2.0, "loop": True, "about": "in action, standing still", "tracks": [], "events": []}

        # fire: the piece runs back its full length and out again; her wheels jump off the ground about her spades
        def run_back(t):
            if t < 0.07:
                return recoil * rigid.smooth(t / 0.07)
            if t < 0.17:
                return recoil
            return recoil * max(0.0, 1.0 - rigid.smooth((t - 0.17) / 1.15))
        jump = math.degrees(math.asin(min(0.5, lift / Lt)))

        def jumped(t):
            if t < 0.34:
                th = jump * math.sin(math.pi * t / 0.34) ** 0.8
            elif t < 0.54:
                th = jump * 0.22 * math.sin(math.pi * (t - 0.34) / 0.2)
            else:
                th = 0.0
            return about((trail_x, 0.0, 0.0), up_axis, th)
        bk = [[t, round(run_back(t), 4)] for t in (0.0, 0.0175, 0.035, 0.0525, 0.07, 0.17, 0.25, 0.34, 0.44, 0.55, 0.66, 0.78, 0.9, 1.0, 1.1, 1.2, 1.27, 1.32, 2.5)]
        m.clips["fire"] = {"long": 2.5, "loop": False, "after": "hold",
                           "about": "she fires: flash and blast at 0.0, the piece runs back at once and out again over a second, her wheels jump and settle; the breech is thrown open at 1.2 "
                                    "and the empty case falls, the next round goes in and it closes at 2.05",
                           "tracks": [{"part": "barrel", "slide": bk}, keys(0.7, jumped, 60),
                                      {"part": "breech", "slide": [[0.0, 0.0], [1.2, 0.0], [1.36, breech], [1.95, breech], [2.08, 0.0], [2.5, 0.0]]}],
                           "events": [ev(0.0, "muzzle_flash_big", "muzzle", cue), ev(0.0, "muzzle_blast_dust", "dust.muzzle"), ev(1.3, "case_clang", "case", "tank_shell_case"),
                                      ev(2.05, "breech_close", "breech", "field_gun_breech")]}
        m.clips["hit"] = {"long": 0.9, "loop": False, "after": "hold", "about": "struck, not silenced: she jolts",
                          "tracks": [keys(0.9, lambda t: about((0.0, 0.0, 0.0), X, 1.6 * math.exp(-5 * t) * math.sin(2 * math.pi * 2.0 * t)))],
                          "events": [ev(0.0, "hit_sparks", "hit.shield", "shell_impact_near")]}

        # the road: her trail lifted to the lorry's hook (she tips about her axle), a split trail's legs closed
        tip = -math.degrees(math.asin(min(0.9, (hitch - eye_z) / Lt)))

        def road(f):       # f: 0 in action, 1 on the hook (and down off her platform, if she stands on one)
            q, mv = about(C, up_axis, tip * f)
            return q, mv - Z * (sink * f)
        legs = [p for p in ("trail_l", "trail_r") if p in m.part_at]

        def limber_tracks(way):
            g = (lambda t: rigid.smooth((t - 0.9) / 1.0)) if way > 0 else (lambda t: 1.0 - rigid.smooth((t - 0.1) / 1.0))
            c = (lambda t: rigid.smooth((t - 0.1) / 0.9)) if way > 0 else (lambda t: 1.0 - rigid.smooth((t - 1.0) / 0.9))
            tr = [keys(2.0, lambda t: road(g(t)))]
            for p in legs:
                tr.append({"part": p, "turn": [[round(2.0 * i / 20, 3), round(close * c(2.0 * i / 20), 3)] for i in range(21)]})
            if platform:
                q0, mv0 = platform
                tr.append({"part": "platform", "rot": [[round(2.0 * i / 20, 3), *Quaternion().slerp(q0, g(2.0 * i / 20))] for i in range(21)],
                           "move": [[round(2.0 * i / 20, 3), *(mv0 * g(2.0 * i / 20))] for i in range(21)]})
            return tr
        m.clips["limber"] = {"long": 2.0, "loop": False, "after": "hold", "about": "out of action: the trail is closed and lifted to the lorry's hook (the men do it: play it with them)",
                             "tracks": limber_tracks(1.0), "events": [ev(0.2, "trail_close", "tow"), ev(1.9, "hook_on", "tow")]}
        m.clips["unlimber"] = {"long": 2.0, "loop": False, "after": "hold", "about": "into action: off the hook, the trail dropped and spread, the spades down",
                               "tracks": limber_tracks(-1.0), "events": [ev(0.1, "hook_off", "tow"), ev(1.0, "trail_drop", "spade.left" if legs else "tow")]}

        def towed(t):
            q, mv = road(1.0)
            b = 0.006 * math.sin(2 * math.pi * 3 * t / 2.0) + 0.003 * math.sin(2 * math.pi * 7 * t / 2.0)
            q2 = Quaternion(X, math.radians(0.5 * math.sin(2 * math.pi * 2 * t / 2.0)))
            return q2 @ q, mv + Z * b
        tr = [keys(2.0, towed)]
        for p in legs:
            tr.append({"part": p, "turn": [[0.0, close], [2.0, close]]})
        if platform:
            q0, mv0 = platform
            tr.append({"part": "platform", "rot": [[0.0, *q0], [2.0, *q0]], "move": [[0.0, *mv0], [2.0, *mv0]]})
        m.clips["towed"] = {"long": 2.0, "loop": True, "about": "on the road behind her lorry: trail closed and on the hook, she bounces a little (the game rolls her wheels and moves her)",
                            "tracks": tr, "events": [ev(0.0, "dust_trail", "dust.left"), ev(0.0, "dust_trail", "dust.right")]}

        # knocked out: a wheel is smashed off her; she drops on that axle end, the piece falls to its stop
        if "wheel_r" in m.part_at:
            Pw = m.parts[m.part_at["wheel_r"]].pivot
            wheel_r = dict(self.wheels)["wheel_r"]
            fall = math.degrees(math.asin(min(0.9, (R - 0.09) / track)))
            Pl = (0.0, track / 2, 0.0)
            lies = Vector((0.18, -(track / 2 + 0.34 + R * 0.5), 0.09))

            def drop(t):
                f = rigid.smooth((t - 0.12) / 0.5) ** 1.6
                bounce = 0.05 * math.exp(-6 * max(0.0, t - 0.62)) * abs(math.sin(2 * math.pi * 2.4 * max(0.0, t - 0.62)))
                return about(Pl, X, fall * (f - bounce * (1 if t > 0.62 else 0)))
            rk, mk = [], []
            for i in range(31):
                t = 1.5 * i / 30
                qc, mc = drop(t)
                f = rigid.smooth((t - 0.1) / 0.75)
                qw_world = Quaternion(X, math.radians(82.0 * f)) @ Quaternion(Z, math.radians(-24.0 * f))
                qw = qc.inverted() @ qw_world
                # where the wheel's middle is, between its place on the axle (as the carriage then stands) and the ground beside her
                on_axle = C + qc @ (Pw - C) + mc
                want = on_axle.lerp(lies, f)
                psi = math.radians(82.0 * f)
                want.z = max(0.09, wheel_r * math.cos(psi) + 0.09 * math.sin(psi)) if f > 0 else on_axle.z       # (it keeps its rim on the ground as it goes over)
                mvw = qc.inverted() @ (want - C - mc) + C - Pw
                rk.append([round(t, 4), qw.w, qw.x, qw.y, qw.z])
                mk.append([round(t, 4), mvw.x, mvw.y, mvw.z])
            rk.append([3.0, *rk[-1][1:]])
            mk.append([3.0, *mk[-1][1:]])
            tr = [keys(1.5, drop), {"part": "wheel_r", "rot": rk, "move": mk}]
            tr[0]["rot"].append([3.0, *tr[0]["rot"][-1][1:]])
            tr[0]["move"].append([3.0, *tr[0]["move"][-1][1:]])
            if "gun" in m.part_at:
                tr.append({"part": "gun", "turn": [[0.0, 0.0], [0.3, 0.0], [0.8, low * 0.85], [1.0, low], [1.12, low * 0.9], [1.25, low], [3.0, low]]})
            tr.append({"part": "traverse", "turn": [[0.0, 0.0], [0.15, 0.0], [0.5, 2.2], [3.0, 2.2]]})
            m.clips["knocked_out"] = {"long": 3.0, "loop": False, "after": "hold",
                                      "about": "knocked out: struck at 0.0, her right wheel is smashed off and she drops on the axle, the piece falls to its stop; holds its last pose",
                                      "tracks": tr,
                                      "events": [ev(0.0, "hit", "hit.shield", "shell_impact_near_02"), ev(0.1, "smoke_burst", "breech"), ev(0.6, "dust_burst", "dust.right"),
                                                 ev(1.2, "smoke_loop", "breech", "fire_small_loop")]}
        m.info.setdefault("file", {}).setdefault("controls", {})["trail"] = {"parts": legs, "what": "+ closes each leg for the road; the limber and unlimber clips do it", "closed": close}
        return self


def _bm_section(section, x0, x1):
    """A solid with this (y, z) section from x0 to x1."""
    bm = bmesh.new()
    a = [bm.verts.new((x0, y, z)) for y, z in section]
    b = [bm.verts.new((x1, y, z)) for y, z in section]
    k = len(section)
    bm.faces.new(a)
    bm.faces.new(b[::-1])
    for i in range(k):
        bm.faces.new((a[i], b[i], b[(i + 1) % k], a[(i + 1) % k]))
    return bm


def crew(v, layer, loader, commander, ammo, case):
    """Where her detachment stands (on the ground, on the root) and where the empty cases fall."""
    v.use("vehicle")
    v.socket("crew.layer", layer, X, part="vehicle", note="the layer, at the sight and the handwheels")
    v.socket("crew.loader", loader, X, part="vehicle", note="the loader, behind the breech")
    v.socket("crew.commander", commander, X, part="vehicle", note="the gun commander")
    v.socket("crew.ammo", ammo, X, part="vehicle", note="the man who hands the rounds up")
    v.socket("ammo", (ammo[0] - 0.7, ammo[1] * 1.25, 0.0), X, part="vehicle", note="where the boxes of rounds stand")
    v.socket("case", case, -X, part="vehicle", note="where the empty case falls")


# =====================================================================================================================
def zis3(fine=True, tag="SOV"):
    """76 mm divisional gun of 1942 (ZiS-3): a long slender barrel with a double-baffle brake, low on lorry wheels,
    a wavy shield swept back over them, round trail legs. 1.6 m wide, 1.38 m high."""
    v = GunWorks("ZiS3", "76 mm divisional gun (ZiS-3)", "gun", tag, fine)
    R, T = 0.43, 1.40
    v.gun_root(R)
    for letter, s in SIDES:
        v.gun_wheel(letter, s, T / 2, R, "pneumatic", w=0.17, rr=0.262, holes=6, bolts=6)
    v.axle(T / 2 - 0.085, r=0.045, box=(0.2, 0.3, 0.1, 0.1))
    v.saddle(0.0, 0.875, 0.16, 27.0, rate=5.0, foot=0.26)
    v.cradle(-5.0, 37.0, 0.22, 0.14, 1.15, 0.62, 0.1, over=(-0.55, 0.95, 0.052, 0.155))
    xb = v.piece(3.40, 0.0762, 1.02, 0.088, 0.052, 0.9, brake="double", steps=((0.34, 0.07),), ring=(0.26, 0.25, 0.27), block="down", lug=0.6)
    v.handwheels((-0.34, 0.33, 0.78), (-0.44, 0.27, 0.62), r=0.1)
    v.sight((-0.22, 0.25, 0.9), high=0.2)
    v.shield(0.34, 0.42, 0.38, 1.26, lean=16.0, slot=(0.125, 0.58, None), top=(0.14, 34.0), apron=(0.2, 8.0), wing=(0.4, 26.0, 0.93), port=(0.27, 1.0, 0.11, 0.09))
    E = None
    for letter, s in SIDES:
        E, close = v.split_trail(letter, s, (-0.2, s * 0.25, R - 0.02), 2.75, 27.0, sec0=(0.16, 0.16), sec1=(0.11, 0.11), tube=(0.07, 0.052), end_z=0.12, eye=(s < 0))
    v.use("carriage")
    v.socket("dust.left", (-0.3, T / 2, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.right", (-0.3, -T / 2, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.muzzle", (2.2, 0.0, 0.03), X, part="vehicle", note="the ground under the muzzle: the blast lifts the dust here")
    crew(v, (-0.75, 0.55, 0.0), (-1.75, -0.5, 0.0), (-2.6, 1.5, 0.0), (-2.2, -1.5, 0.0), (-1.6, -0.2, 0.0))
    v.gun_clips(0.9, E.x, lift=0.045, close=close, eye_z=0.2, track=T, low=-5.0)
    return v


def lefh18(fine=True, tag="GER", paint=None):
    """10.5 cm light field howitzer 18: a short thick barrel under its recuperator, big cast wheels with solid
    tyres, a tall narrow shield between them, riveted box trail legs with folding spades. Dark grey to early 1943,
    dark yellow after: one model, a paint swap."""
    v = GunWorks("leFH18", "10.5 cm leFH 18", "gun", tag, fine, paint=GER_COATS[paint or "grey"][0])
    R, T = 0.65, 1.56
    v.gun_root(R)
    for letter, s in SIDES:
        v.gun_wheel(letter, s, T / 2, R, "solid", w=0.13, holes=10, bolts=8)
    v.axle(T / 2 - 0.065, r=0.055, box=(0.26, 0.4, 0.13, 0.1))
    v.saddle(0.0, 1.18, 0.21, 28.0, rate=4.0, foot=0.32)
    v.cradle(-5.0, 42.0, 0.3, 0.2, 0.95, 0.6, 0.15, over=(-0.5, 1.3, 0.078, 0.235))
    v.piece(2.94, 0.105, 0.86, 0.135, 0.092, 1.1, steps=((0.42, 0.115),), ring=(0.32, 0.36, 0.35), block="right", lug=0.55, swell=(0.13, 0.106))
    v.handwheels((-0.3, 0.4, 1.05), (-0.42, 0.33, 0.9), r=0.12)
    v.sight((-0.2, 0.31, 1.22), high=0.26)
    if fine:                                               # the two spring equilibrators, standing either side of the cradle
        v.use("traverse")
        for s in (LEFT, RIGHT):
            v.rod((0.2, s * 0.3, R + 0.16), (0.14, s * 0.3, 1.02), 0.055, coat, 14)
            v.rod((0.14, s * 0.3, 1.02), (0.12, s * 0.27, 1.12), 0.02, "steel", 8)
    v.shield(0.4, 0.69, 0.44, 1.44, lean=6.0, slot=(0.19, 0.76, None), top=(0.3, 40.0), apron=(0.24, 6.0), port=(0.42, 1.2, 0.12, 0.1), corner=0.0)
    E = None
    for letter, s in SIDES:
        E, close = v.split_trail(letter, s, (-0.24, s * 0.3, R - 0.03), 2.95, 28.0, sec0=(0.2, 0.25), sec1=(0.13, 0.15), end_z=0.14, eye=(s < 0))
    v.use("carriage")
    v.socket("dust.left", (-0.3, T / 2, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.right", (-0.3, -T / 2, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.muzzle", (2.0, 0.0, 0.03), X, part="vehicle", note="the ground under the muzzle: the blast lifts the dust here")
    crew(v, (-0.7, 0.6, 0.0), (-1.6, -0.5, 0.0), (-2.7, 1.6, 0.0), (-2.2, -1.6, 0.0), (-1.5, -0.2, 0.0))
    v.gun_clips(1.1, E.x, lift=0.04, close=close, eye_z=0.22, track=T, low=-5.0)
    v.m.info.setdefault("file", {})["swaps"] = {"how": "paint: put the chosen coat's colours in place of the paints of the same names.", "paint": coats(v, paint or "grey")}
    return v


def howitzer105(fine=True, tag="USA"):
    """105 mm howitzer M2A1: a short barrel between a sleigh below and a cylinder above, lorry wheels, a shield in
    two halves with folding tops, long box trail legs."""
    v = GunWorks("Howitzer105", "105 mm howitzer M2A1", "gun", tag, fine)
    R, T = 0.5, 1.82
    v.gun_root(R)
    for letter, s in SIDES:
        v.gun_wheel(letter, s, T / 2, R, "pneumatic", w=0.25, rr=0.262, holes=0, bolts=6)
    v.axle(T / 2 - 0.125, r=0.055, box=(0.26, 0.44, 0.13, 0.12))
    v.saddle(0.0, 1.16, 0.22, 23.0, rate=4.0, foot=0.34)
    v.cradle(-5.0, 66.0, 0.32, 0.2, 1.2, 0.55, 0.16, over=(-0.45, 1.25, 0.07, 0.235))
    v.piece(2.36, 0.105, 0.78, 0.14, 0.1, 1.07, steps=((0.45, 0.118),), ring=(0.3, 0.36, 0.36), block="right", lug=0.5)
    v.handwheels((-0.3, 0.42, 1.02), (-0.42, 0.34, 0.86), r=0.12)
    v.sight((-0.2, 0.32, 1.2), high=0.26)
    v.shield(0.44, 0.76, 0.52, 1.42, lean=4.0, split=0.24, top=(0.26, 32.0), apron=(0.22, 6.0), corner=0.0)
    E = None
    for letter, s in SIDES:
        E, close = v.split_trail(letter, s, (-0.26, s * 0.32, R - 0.02), 3.2, 23.0, sec0=(0.22, 0.26), sec1=(0.14, 0.16), end_z=0.14, eye=(s < 0))
    v.use("carriage")
    v.socket("dust.left", (-0.3, T / 2, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.right", (-0.3, -T / 2, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.muzzle", (1.6, 0.0, 0.03), X, part="vehicle", note="the ground under the muzzle: the blast lifts the dust here")
    crew(v, (-0.7, 0.65, 0.0), (-1.6, -0.5, 0.0), (-2.9, 1.6, 0.0), (-2.3, -1.6, 0.0), (-1.5, -0.2, 0.0))
    v.mark("USA", (0.445, 0.5, 0.86), Vector((1.0, 0.0, 0.07)), 0.13, part="traverse")
    v.gun_clips(1.07, E.x, lift=0.04, close=close, eye_z=0.22, track=T, low=-5.0)
    return v


def pounder25(fine=True, tag="UK"):
    """Ordnance QF 25-pounder: a box cradle as long as the barrel's jacket, lorry wheels standing on a round firing
    platform, a flat shield with its top bent back, a humped box trail."""
    v = GunWorks("25pdr", "25-pounder", "gun", tag, fine)
    R, T, plat = 0.45, 1.78, 0.03
    v.gun_root(R + plat)
    for letter, s in SIDES:
        v.gun_wheel(letter, s, T / 2, R, "pneumatic", w=0.23, rr=0.215, holes=0, bolts=5)
    v.axle(T / 2 - 0.115, r=0.05, box=(0.24, 0.4, 0.12, 0.1))
    v.saddle(-0.05, 1.12, 0.2, 4.0, rate=2.0, foot=0.3)
    v.cradle(-5.0, 40.0, 0.27, 0.19, 1.45, 0.45, 0.13)
    v.piece(2.47, 0.0876, 0.8, 0.118, 0.07, 0.9, steps=((0.5, 0.095),), ring=(0.27, 0.3, 0.31), block="down", lug=0.5)
    v.handwheels((-0.32, 0.38, 1.0), (-0.44, 0.31, 0.86), r=0.11)
    v.sight((-0.24, 0.3, 1.14), high=0.22)
    v.seat((-0.62, 0.36, 0.7), 1.0, part="traverse")
    v.shield(0.36, 0.74, 0.4, 1.36, lean=10.0, slot=(0.165, 0.72, None), top=(0.22, 30.0), port=(0.38, 1.2, 0.12, 0.08))
    # ---- the humped box trail ---------------------------------------------------------------------------
    v.use("carriage")
    v.hull([(-0.18, 0.3, 0.4), (-0.18, 0.3, 0.6), (-1.3, 0.19, 0.52), (-1.3, 0.19, 0.72)], coat, 0.012)
    v.hull([(-1.3, 0.19, 0.52), (-1.3, 0.19, 0.72), (-2.5, 0.1, 0.1), (-2.5, 0.1, 0.27)], coat, 0.012)
    if fine:
        for s in (LEFT, RIGHT):
            for k in range(1, 9):
                f = k / 9
                v.rivet((-0.18 - 1.12 * f, s * (0.3 - 0.11 * f) + 0.0, 0.6 + 0.12 * f - 0.03), (0, s, 0.1), 0.008)
                v.rivet((-1.3 - 1.2 * f, s * (0.19 - 0.09 * f), 0.72 - 0.45 * f - 0.03), (0, s, 0.1), 0.008)
        v.slab((-1.05, -0.12, 0.72), (-0.7, 0.12, 0.84), coat, 0.008)                                  # the tool box on the trail
        v.handle((-1.0, 0.0, 0.84), (-0.76, 0.0, 0.84), Z, r=0.008, reach=0.03)
    v.spade((-2.5, 0.0, 0.1), -X, wide=0.3, deep=0.14, sole=0.26)
    v.lunette((-2.52, 0.0, 0.2), -X)
    v.tube([(-2.3, 0.0, 0.3), (-2.42, 0.0, 0.62), (-2.6, 0.0, 0.7)], 0.02, "steel", 8)                    # the handspike she is swung by
    v.socket("spade.left", (-2.5, 0.0, 0.0), -Z, note="where the trail's spade bites")
    # ---- the firing platform: a wheel of plate she stands on and is swung round on -------------------------
    v.part("platform", "carriage", (0.0, 0.0, 0.02), kind="free", note="the firing platform, on the ground under her wheels (the limber clip stows it under the trail)")
    n = v.n(48, 16)
    v.lathe((0, 0, 0), Z, [(0.014, 0.0), (0.014, 0.76), (plat, 0.8), (plat, 0.98), (0.0, 0.98), (0.0, 0.0)], coat, n)
    v.lathe((0, 0, 0.014), Z, [(0.03, 0.0), (0.03, 0.1), (0.0, 0.12), (0.0, 0.0)], coat, v.n(16, 8))
    if fine:
        for k in range(8):
            a = 2 * math.pi * (k + 0.5) / 8
            d = Vector((math.cos(a), math.sin(a), 0.0))
            v.box((0.7, 0.03, 0.02), d * 0.46 + Z * 0.024, coat, turn=Matrix((d, Z.cross(d), Z)).transposed())
    v.use("carriage")
    v.socket("dust.left", (-0.3, T / 2, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.right", (-0.3, -T / 2, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.muzzle", (1.7, 0.0, 0.03), X, part="vehicle", note="the ground under the muzzle: the blast lifts the dust here")
    crew(v, (-0.75, 0.6, 0.0), (-1.5, -0.5, 0.0), (-2.4, 1.5, 0.0), (-2.0, -1.5, 0.0), (-1.4, -0.2, 0.0))
    stow = (Quaternion(Vector((0, -1, 0)), math.radians(5.6)), Vector((-1.2, 0.0, 0.25)))      # (hung under the trail, behind the hump)
    v.gun_clips(0.9, -2.5, lift=0.03, close=None, eye_z=0.2, platform=stow, track=T, low=-5.0, sink=plat)
    return v


def gun75(fine=True, tag="FRA"):
    """The 75 mm field gun of 1897: wooden wheels as high as a man's chest, the barrel riding on a recoil cylinder
    as long as itself, a shield between the wheels, a single pole trail with two seats."""
    v = GunWorks("Gun75", "75 mm field gun", "gun", tag, fine)
    R, T = 0.665, 1.51
    v.gun_root(R)
    for letter, s in SIDES:
        v.gun_wheel(letter, s, T / 2, R, "spoked", w=0.08, spokes=14)
    v.axle(T / 2 - 0.12, r=0.045, box=(0.2, 0.34, 0.1, 0.09))
    v.saddle(0.05, 1.03, 0.15, 3.0, rate=1.5, foot=0.24, lighten=False)
    v.cradle(-11.0, 18.0, 0.2, 0.2, 1.42, 0.86, 0.085, round_under=True, arc=True)
    v.piece(2.72, 0.075, 0.98, 0.082, 0.058, 1.14, steps=((0.36, 0.068),), ring=(0.24, 0.23, 0.23), block="screw", round_ring=True, lug=0.9)
    if fine:                                               # the collar under the muzzle that rides the cradle's end
        v.use("barrel")
        v.slab((0.05 + 1.2, -0.06, 1.03 - 0.085), (0.05 + 1.3, 0.06, 1.03 - 0.03), "shade", 0.004)
    v.handwheels((-0.3, 0.3, 0.92), (-0.42, 0.25, 0.8), r=0.1)
    v.sight((-0.2, 0.23, 1.06), high=0.2)
    v.shield(0.3, 0.68, 0.66, 1.42, lean=0.0, slot=(0.13, 0.69, 1.24), corner=0.16, apron=(0.34, 4.0), port=(0.3, 1.2, 0.1, 0.08))
    # ---- the pole trail, its seats, its spade and eye ----------------------------------------------------
    v.use("carriage")
    v.hull([(-0.2, 0.25, R - 0.14), (-0.2, 0.25, R + 0.05), (-2.6, 0.085, 0.1), (-2.6, 0.085, 0.27)], coat, 0.012)
    if fine:
        for s in (LEFT, RIGHT):
            for k in range(1, 17):
                f = k / 17
                v.rivet((-0.2 - 2.4 * f, s * (0.25 - 0.165 * f), R + 0.05 - (R - 0.22) * f - 0.03), (0, s, 0.08), 0.008)
        v.slab((-1.5, -0.1, 0.42), (-1.15, 0.1, 0.5), coat, 0.008)                                     # the tool box
    for s in (LEFT, RIGHT):
        v.seat((-0.55, s * 0.42, 0.74), s)
    v.spade((-2.6, 0.0, 0.1), -X, wide=0.34, deep=0.2, sole=0.28)
    v.lunette((-2.62, 0.0, 0.2), -X)
    v.tube([(-2.35, 0.0, 0.3), (-2.5, 0.0, 0.62), (-2.7, 0.0, 0.72)], 0.02, "wood", 8)                    # the handspike
    v.socket("spade.left", (-2.6, 0.0, 0.0), -Z, note="where the trail's spade bites")
    v.socket("dust.left", (-0.3, T / 2, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.right", (-0.3, -T / 2, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.muzzle", (1.8, 0.0, 0.03), X, part="vehicle", note="the ground under the muzzle: the blast lifts the dust here")
    crew(v, (-0.75, 0.55, 0.0), (-1.7, -0.5, 0.0), (-2.6, 1.5, 0.0), (-2.2, -1.5, 0.0), (-1.6, -0.2, 0.0))
    v.gun_clips(1.14, -2.6, lift=0.03, close=None, eye_z=0.2, track=T, low=-11.0)
    return v
