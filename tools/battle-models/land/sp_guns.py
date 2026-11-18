"""Self-propelled artillery: a field gun's own saddle, cradle and piece standing in the open top of a tank's hull, so
the guns keep up with the armour. The M7 (the United States' 105 mm howitzer on the M4's hull, a round machine-gun
pulpit at her right front corner), the Sexton (Britain's and the Commonwealth's 25-pounder in the same hull, a
plain tall box with no pulpit), the Hummel (Germany's 15 cm heavy field howitzer on a lengthened Panzer IV hull)
and the SU-76 (the Soviet Union's 76 mm divisional gun on a lengthened light tank's hull). Last, a self-propelled
gun no one nation owns, our own design: a 75 mm field gun in an open box on the medium tank's hull (tanks_any.py),
for every army with none of its own.

Each is modelled to the real type's measurements and layout from the published dimensions; every shape is our own
mesh, made from numbers here. The gun is the towed gun's own (guns.py), built by the same kit, so the breech the
battle camera sees over her back wall is the right one. They keep the tanks' part and socket names, so the game
drives them the same way: `turret` is the saddle (its limits say how far it trains: for more the driver turns her),
`gun` lifts, `barrel` runs back, `breech` opens. The detachment's places are sockets: `commander`, `crew.gunner`,
`crew.loader`, `crew.loader2`, and `crew.mg` where there is a machine gun.
"""

import math

from mathutils import Vector

from carriers import wall
from guns import GunWorks, edge_rivets, sheet
from motorpool import LEFT, RIGHT, X, Y, Z, bm_box, rot
from tanks_any import medium_hull
from tanks_ger import GER_COATS, coat, coats, pz4_gear
from tanks_usa import m4_deck, m4_gear, m4_tail, m4_tub


class SpWorks(GunWorks):
    """A gun carried in a hull: the saddle is the part `turret`, standing on the `hull`."""
    mount, mount_on = "turret", "hull"

    def pedestal(self, at, floor, r=0.36):
        """The drum the saddle turns on, standing on her floor; sets the pintle."""
        at = Vector(at)
        self.use("hull")
        n = self.n(32, 12)
        self.lathe((at.x, at.y, floor), Z, [(at.z - floor - 0.035, 0.0), (at.z - floor - 0.035, r * 0.8), (at.z - floor - 0.07, r), (0.03, r), (0.0, r + 0.05), (0.0, 0.0)], coat, n)
        self.lathe((at.x, at.y, at.z - 0.035), Z, [(0.035, 0.0), (0.035, r * 0.7), (0.0, r * 0.76), (0.0, 0.0)], "shade", n)      # the race
        if self.fine:
            self.bolts((at.x, at.y, floor + 0.03), Z, r + 0.025, 12, r=0.012, high=0.008)
        self.pintle = at

    def here(self):
        """A mark to `shift` from: everything built after it can be moved as one."""
        m = self.m
        return len(m.verts), len(m.parts), len(m.sockets)

    def shift(self, here, by):
        """Move everything built since `here` (points, parts' pivots, sockets): the gun kit builds on her middle
        line, and this carries the whole gun off to one side of it."""
        m, d = self.m, Vector(by)
        for p in m.verts[here[0]:]:
            p += d
        for part in m.parts[here[1]:]:
            part.pivot = part.pivot + d
        for s in m.sockets[here[2]:]:
            s["at"] = s["at"] + d
        self.pintle = self.pintle + d

    def rounds(self, x0, x1, y0, y1, z0, high, r=0.055, gap=0.135, paint="brass", tray=0.55):
        """Ready rounds standing nose down in a rack: a tray of plate, and the brass bases of the cases over it, each
        with its rim, the groove under it and the primer in its flat base (a plain brass stub with a rounded top reads
        as a sausage). tray: how far up the rounds the tray stands."""
        self.use("hull")
        self.slab((x0, min(y0, y1), z0), (x1, max(y0, y1), z0 + high * tray), coat, 0.006)
        nx, ny = max(1, int((x1 - x0 - 0.03) / gap)), max(1, int((abs(y1 - y0) - 0.03) / gap))
        n = self.n(12, 6)
        for i in range(nx):
            for j in range(ny):
                x = x0 + (x1 - x0) * (i + 0.5) / nx
                y = y0 + (y1 - y0) * (j + 0.5) / ny
                t = high * (1.0 - tray)
                if not self.fine:
                    self.lathe((x, y, z0 + high * tray), Z, [(t, 0.0), (t, r * 1.04), (0.0, r * 1.04), (0.0, 0.0)], paint, n)
                    continue
                self.lathe((x, y, z0 + high * tray), Z, [(t, 0.0), (t, r * 1.08), (t - 0.007, r * 1.08), (t - 0.007, r), (0.0, r), (0.0, 0.0)], paint, n)
                self.lathe((x, y, z0 + high), Z, [(0.0008, 0.0), (0.0008, r * 0.38)], "black", 8)        # the primer's pocket, and the primer in it
                self.lathe((x, y, z0 + high), Z, [(0.002, 0.0), (0.002, r * 0.27)], "steel", 8)

    # ---- plate work: what makes a thin open box read as built, not folded from card ---------------------------
    def edge_bolts(self, a, b, facing, gap=0.17, r=0.011, high=0.007):
        """A row of bolt heads from a to b, on a plate that faces `facing`."""
        if not self.fine:
            return
        a, b = Vector(a), Vector(b)
        n = max(1, round((b - a).length / gap))
        for k in range(n + 1):
            self.bolt(a.lerp(b, k / n), facing, r, high)

    def rib(self, a, b, inward, deep=0.034, wide=0.03, paint=coat):
        """A strip of plate standing `deep` out of a wall's inside from a to b: a stiffener up it, or (thin and
        deep) the angle iron along the inside of its top edge."""
        a, b = Vector(a), Vector(b)
        along = (b - a).normalized()
        n = Vector(inward)
        n = (n - along * n.dot(along)).normalized()
        self.box(((b - a).length, wide, deep), (a + b) / 2 + n * (deep / 2 - 0.002), paint, 0.003, turn=_turn(along, n))

    # ---- what a detachment piles into her ----------------------------------------------------------------------
    def pack(self, at, facing, size=(0.32, 0.13, 0.34), paint="canvas.tarp"):
        """A haversack hung flat on a plate: `at` the middle of its back, `facing` the way out of the plate."""
        f = Vector(facing)
        m = rot(math.degrees(math.atan2(-f.x, f.y)))
        w, d, h = size
        c = Vector(at) + m @ Vector((0, d / 2, 0))
        self.put(bm_box((w, d, h), c, m), paint, 0.035, segs=2)
        self.put(bm_box((w + 0.012, d + 0.014, h * 0.42), c + Z * (h * 0.30), m), paint, 0.022, segs=2)       # its flap
        if self.fine:
            for e in (-0.26, 0.26):                          # two straps down the flap, and the sling it hangs by
                self.put(bm_box((0.026, d + 0.024, h * 0.5), c + m @ Vector((e * w, 0, h * 0.24)), m), "leather")
            self.tube([c + m @ Vector((-w * 0.3, -d * 0.4, h * 0.45)), Vector(at) + Z * (h * 0.5 + 0.09), c + m @ Vector((w * 0.3, -d * 0.4, h * 0.45))], 0.008, "leather", 5)

    def tarp(self, at, size=(0.6, 0.42, 0.1), yaw=0.0, paint="canvas", folds=3):
        """A tarpaulin folded flat: a few layers, none lying quite square on the one below, a strap round it."""
        m, g, at = rot(yaw), self.rng, Vector(at)
        h = size[2] / folds
        for k in range(folds):
            layer = (size[0] * (1 - 0.04 * k), size[1] * (1 - 0.05 * k), h * 1.1)
            self.put(bm_box(layer, at + Z * (h * (k + 0.5)) + m @ Vector((g.uniform(-0.012, 0.012), g.uniform(-0.012, 0.012), 0)), rot(yaw + g.uniform(-3, 3))), paint, h * 0.42, segs=2)
        if self.fine:
            self.put(bm_box((0.03, size[1] + 0.012, size[2] + 0.01), at + Z * (size[2] / 2), m), "leather")

    def helmet(self, at, up=Z, kind="pot", paint="upper", r=0.125):
        """A steel helmet lying or hung by its strap: `up` the way its crown points."""
        if kind == "brodie":       # the British dish: a shallow crown, a wide brim
            prof = [(0.11, 0.0), (0.10, r * 0.45), (0.065, r * 0.78), (0.022, r * 0.9), (0.012, r * 1.28), (0.0, r * 1.3), (0.0, 0.0)]
        elif kind == "stahl":      # the German: a deep crown, the skirt flared out round it
            prof = [(0.16, 0.0), (0.15, r * 0.5), (0.11, r * 0.86), (0.05, r * 0.98), (0.022, r * 1.02), (0.0, r * 1.17), (0.0, 0.0)]
        else:                      # a plain deep pot (the American and the Soviet)
            prof = [(0.155, 0.0), (0.145, r * 0.5), (0.105, r * 0.86), (0.04, r), (0.0, r * 1.03), (0.0, 0.0)]
        self.lathe(at, up, prof, paint, self.n(16, 8))

    def rammer(self, a, b, head=0.05):
        """The rammer: a wooden staff from a, its head at b, in two clips."""
        a, b = Vector(a), Vector(b)
        d = (b - a).normalized()
        self.rod(a, b - d * 0.12, 0.017, "wood", 8)
        self.rod(b - d * 0.13, b, head, "wood", 12)
        if self.fine:
            for t in (0.22, 0.72):
                p = a.lerp(b, t)
                self.rod(p - d * 0.015, p + d * 0.015, 0.03, "steel", 8)

    def sack(self, at, r=0.15, high=0.36, paint="cloth"):
        """A plain kit sack standing, tied at the neck."""
        self.lathe(at, Z, [(high, 0.0), (high, r * 0.24), (high * 0.9, r * 0.2), (high * 0.84, r * 0.34), (high * 0.62, r * 0.88), (high * 0.34, r), (high * 0.1, r * 0.9),
                           (0.0, r * 0.55), (0.0, 0.0)], paint, self.n(14, 7))

    def shells(self, x0, x1, y0, y1, z0, r, high, gap, paint="shade.shell"):
        """Shells standing on their bases in a rack: a low frame of plate round their feet, the copper driving band
        just over it, the noses above. high: a shell's own height."""
        self.use("hull")
        self.slab((x0, min(y0, y1), z0), (x1, max(y0, y1), z0 + high * 0.2), coat, 0.006)
        nx, ny = max(1, int((x1 - x0 - 0.03) / gap)), max(1, int((abs(y1 - y0) - 0.03) / gap))
        n = self.n(14, 7)
        for i in range(nx):
            for j in range(ny):
                x = x0 + (x1 - x0) * (i + 0.5) / nx
                y = y0 + (y1 - y0) * (j + 0.5) / ny
                zf = z0 + 0.04                                                                       # (its base, on the rack's floor)
                self.lathe((x, y, zf), Z, [(high, 0.0), (high * 0.985, r * 0.2), (high * 0.86, r * 0.52), (high * 0.66, r * 0.82), (high * 0.43, r), (0.0, r), (0.0, 0.0)], paint, n)
                self.lathe((x, y, z0 + high * 0.225), Z, [(high * 0.05, 0.0), (high * 0.05, r * 1.035), (0.0, r * 1.035), (0.0, 0.0)], "brass.band", n)     # the copper driving band, just over the rack
                if self.fine:
                    self.lathe((x, y, zf + high - 0.008), Z, [(0.03, 0.0), (0.03, r * 0.16), (0.0, r * 0.26), (0.0, 0.0)], "brass", 8)     # the fuze
                    a = math.radians(180.0 + 34.0 * ((i + 2 * j) % 3 - 1))                               # its stencil: two short lines of lettering, turned as it was stood
                    for k, wide in ((0.36, 0.040), (0.40, 0.028)):
                        self.box((0.0012, wide, 0.008), (x + (r + 0.0003) * math.cos(a), y + (r + 0.0003) * math.sin(a), z0 + high * k), "shade.stencil", turn=rot(math.degrees(a)))

    def spent(self, at, yaw, r, long, paint="brass"):
        """An empty case lying on her floor where it was thrown: at is the floor under its middle."""
        axis = Vector((math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), 0.0))
        base = Vector(at) + Z * r * 1.1 - axis * long / 2
        n = self.n(14, 7)
        self.use("hull")
        self.lathe(base, axis, [(long, 0.0), (long, r), (0.012, r), (0.012, r * 1.1), (0.0, r * 1.1), (0.0, 0.0)], paint, n)
        self.lathe(base + axis * (long + 0.0006), axis, [(0.0006, 0.0), (0.0006, r * 0.88), (0.0, r * 0.88), (0.0, 0.0)], "black", n)       # its open mouth

    def trodden(self, patches, z, paint="torn.mud", edge="torn.mud2"):
        """Mud trodden onto her floor in a path the men wear from the racks to the breech: thin lobed patches
        (x, y, half-length, half-width, yaw each), a shade off the floor's own colour and never a black spill.
        Each lies in a wider patch of a colour halfway to the floor's (`edge`), so the path has no hard rim."""
        self.use("hull")
        for grow, colour, up in ((1.22, edge, 0.0), (1.0, paint, 0.0012)):
            for n, (px, py, rx, ry, yaw) in enumerate(patches):
                c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
                p = (px * 7.3 + py * 3.1) % 6.283                                # each patch its own outline
                plan, top = [], []
                for i in range(30):
                    a = 2.0 * math.pi * i / 30
                    k = grow * (1.0 + 0.17 * math.sin(3 * a + p) + 0.10 * math.sin(5 * a + 2.1 * p) + 0.06 * math.sin(8 * a + 0.7 * p))
                    for into, ring in ((1.0, plan), (0.84, top)):               # the rim runs out to nothing on the floor: a step there throws a thin dark line, like a map's
                        x, y = rx * k * into * math.cos(a), ry * k * into * math.sin(a)
                        ring.append((px + x * c - y * s, py + x * s + y * c))
                self.prism(plan, z, z + 0.0012 + up + 0.0002 * n, colour, top=top)


def m4_open_hull(v, zw, zb, xf, notch, pulpit=None, driver=LEFT, step=None, tag="USA"):
    """The M4's hull with an open box for a gun where her roof and turret were. zw: the top of the walls; zb: the top
    of the back wall, over the engine deck; xf: where the head of the sloped front plate stands; notch: (right edge,
    left edge, foot) of the opening the piece looks through; pulpit: (x, y, radius, top) of a round machine-gun
    position standing in her right front corner; driver: the side his flap is on; step: (x, z) the side walls step
    down to z behind x. Returns the handy numbers."""
    fine = v.fine
    hw, sw, zs, zr, xe = 0.80, 1.33, 1.06, 1.74, -1.02     # between the tracks, over them; the sponson's floor; the engine deck; its bulkhead
    th, fl = 0.036, 1.076                                  # the box's plate (a little over life, so its top edge reads from the battle camera); her floor
    m4_tub(v, hw)
    v.use("hull")
    # the engine's box behind the bulkhead, and the sponsons' floors over the tracks
    v.hull([(xe, sw, zs), (xe, sw, zr), (-1.60, sw, zr), (-2.58, sw, 1.66), (-2.76, sw, 1.18), (-2.76, sw, zs)], coat, bevel=0.014)
    for s in (LEFT, RIGHT):
        v.slab((xe - 0.02, s * (hw - 0.02), zs - 0.03), (2.47, s * sw, zs), coat, 0.004)
    v.slab((xe, -hw, 1.068), (2.30, hw, 1.076), "shade")                                               # her floor, trodden dark

    # ---- the front plate: sloped back from over the nose, the notch for the piece cut down into it --------------
    gl = Vector((xf - 2.47, 0, zw - zs))
    Hc = gl.length
    gl = gl.normalized()                                              # up the front plate
    gn = Vector((gl.z, 0, -gl.x))                                     # out of it
    g_at = lambda u, y: Vector((2.47, y, zs)) + gl * u + gn * 0.002       # noqa: E731  (a place on the front plate, u metres up it)
    n0, n1, nz = notch
    bn = (nz - zs) / gl.z
    out = [(-sw, -0.02), (sw, -0.02), (sw, Hc), (n1, Hc), (n1, bn), (n0, bn), (n0, Hc), (-sw, Hc)]
    sheet(v, out, [], (2.47, 0.0, zs), Y, gl, th, coat, 0.004)
    if fine:
        v.weld(g_at(0.02, -sw + 0.04), g_at(0.02, sw - 0.04), gn)
        for y in (n0 - 0.03, n1 + 0.03):                    # a stiffening strip up each side of the notch
            v.box((Hc - bn, 0.05, 0.012), g_at((Hc + bn) / 2, y) + gn * 0.006, coat, 0.003, turn=_turn(gl, gn))
    # the driver's flap: a plate hung on its upper edge, a slot to see through
    dy = driver * (((n1 if driver > 0 else -n0) + sw) / 2)
    v.hatch_rect("hatch_driver", "hull", g_at(Hc * 0.58, dy), (0.40, 0.56), gn, gl, hinge="front", opens=85.0, thick=0.024)
    v.visor(g_at(Hc * 0.58, dy) + gn * 0.024, gn, 0.30, 0.07, 0.02, part="hatch_driver", brow=False)
    v.use("hull")
    for s in (LEFT, RIGHT):
        lamp_at = g_at(0.26, s * 1.05)
        v.headlamp(lamp_at + gn * 0.07, X, 0.062, stalk=0.07)
        v.lamp_guard(lamp_at + gn * 0.07 + Z * 0.07, 0.062, lamp_at, gn, Y, gl)          # the brush guard: two hoops of bar crossing over the lamp
        # the track guards: a short flap ahead over each sprocket
        v.plate([(2.46, s * 0.84, zs + 0.004), (2.46, s * 1.31, zs + 0.004), (2.92, s * 1.31, zs - 0.02), (2.92, s * 0.84, zs - 0.02)][::int(s)], 0.008, "upper", 0.003)
        v.plate([(2.92, s * 0.84, zs - 0.02), (2.92, s * 1.31, zs - 0.02), (3.00, s * 1.31, zs - 0.10), (3.00, s * 0.84, zs - 0.10)][::int(s)], 0.008, "upper", 0.003)
        v.plate([(-2.94, s * 0.84, zs - 0.07), (-2.94, s * 1.31, zs - 0.07), (-2.76, s * 1.31, zs + 0.004), (-2.76, s * 0.84, zs + 0.004)][::int(s)], 0.008, "upper", 0.003)

    # ---- the side walls and the back wall: thin plate, upright -------------------------------------------------
    top = [(xf, zw)] + ([(step[0] + 0.14, zw), (step[0], step[1])] if step else []) + [(xe, step[1] if step else zw)]
    zback = step[1] if step else zw
    for s in (LEFT, RIGHT):
        oy = sw - th if s > 0 else -sw
        if s < 0 and pulpit:                                 # (the right wall stops each side of the pulpit, which stands out through it)
            cx, cy, r, _ = pulpit
            d = math.sqrt(max(0.0, r * r - (sw + cy) ** 2)) - 0.006
            xa, xb = cx - d, cx + d
            za = zw if not step or xa > step[0] + 0.14 else step[1]
            rear = [(xa, zs), (xa, za)] + [p for p in top[1:] if p[0] < xa] + [(xe, zs)]
            sheet(v, rear, [], (0.0, oy, 0.0), X, Z, th, coat, 0.004)
            zf = zs + (2.47 - xb) * (zw - zs) / (2.47 - xf)
            sheet(v, [(xb, zs), (2.47, zs), (xb, zf)] if xb > xf else [(xb, zs), (2.47, zs), (xf, zw), (xb, zw)], [], (0.0, oy, 0.0), X, Z, th, coat, 0.004)
        else:
            sheet(v, [(xe, zs), (2.47, zs)] + top, [], (0.0, oy, 0.0), X, Z, th, coat, 0.004)
        if fine:
            v.handle(Vector((-0.2, s * sw, zback - 0.1)), Vector((-0.8, s * sw, zback - 0.1)), Y * s, 0.011, 0.05)
            x_ahead = 2.47 - 0.10 if not (s < 0 and pulpit) else pulpit[0] - pulpit[2]
            v.weld((xe + 0.02, s * (sw + 0.001), zs + 0.004), (x_ahead, s * (sw + 0.001), zs + 0.004), Y * s)          # where the wall stands on the sponson's floor
            for x in (-0.36, 0.52):                         # where the wall's plates join
                if x < x_ahead:
                    v.weld((x, s * (sw + 0.001), zs + 0.02), (x, s * (sw + 0.001), (zback if x < (step[0] if step else 9.0) else zw) - 0.02), Y * s)
            for x in (-0.86, -0.02, 0.36, 0.74):            # loops to lash her stowage to
                if x < x_ahead - 0.05:
                    z = (zback if x < (step[0] if step else 9.0) else zw) - 0.22
                    v.handle(Vector((x - 0.035, s * sw, z)), Vector((x + 0.035, s * sw, z)), Y * s, 0.006, 0.022, "steel")
            for k in range(9):
                v.bolt((xe + 0.2 + 0.36 * k, s * (sw - 0.06), zs - 0.031), -Z, 0.012, 0.008)
            # the plate's own work: the angle along the inside of her top edge, a row of bolts through it, stiffeners up the inside
            inn, yi = -Y * s, s * (sw - th)
            spans = [(xe + th, step[0] - 0.02), (step[0] + 0.16, xf - 0.06)] if step else [(xe + th, xf - 0.06)]
            if s < 0 and pulpit:                            # (nothing where the pulpit stands in the wall)
                spans = [(a, min(b, xa - 0.05)) for a, b in spans if a < xa - 0.25]
            for a, b in spans:
                z = zback if a < (step[0] if step else 9.0) else zw
                v.rib((a, yi, z - 0.05), (b, yi, z - 0.05), inn, deep=0.045, wide=0.012)
                v.edge_bolts((a + 0.06, s * (sw + 0.0005), z - 0.05), (b - 0.04, s * (sw + 0.0005), z - 0.05), Y * s)
                for x in (-0.66, 0.62, 1.26):
                    if a + 0.1 < x < b - 0.1:
                        v.rib((x, yi, zs + 0.004), (x, yi, z - 0.056), inn)
            v.weld((2.47, s * (sw + 0.001), zs + 0.01), (xf + 0.01, s * (sw + 0.001), zw - 0.01), (gn + Y * s).normalized())      # where the front plate meets her side
            if zback > zr + 0.05:
                v.weld((xe - 0.001, s * (sw - 0.004), zr + 0.01), (xe - 0.001, s * (sw - 0.004), zback - 0.01), (Y * s - X).normalized())
    if zback > zr + 0.01:
        wall(v, [(xe, -sw, zr - 0.02), (xe, sw, zr - 0.02), (xe, sw, zback), (xe, -sw, zback)], X, th, coat, 0.004)
        if fine:
            v.rib((xe + th, -sw + th, zback - 0.05), (xe + th, sw - th, zback - 0.05), X, deep=0.045, wide=0.012)
            v.edge_bolts((xe - 0.0005, -sw + 0.12, zback - 0.05), (xe - 0.0005, sw - 0.12, zback - 0.05), -X)
            if zback > zr + 0.3:
                for y in (-0.3, 0.3):
                    v.rib((xe + th, y, zr), (xe + th, y, zback - 0.056), X)
    if fine:
        for y0, y1 in ((-sw + 0.1, n0 - 0.09), (n1 + 0.09, sw - 0.1)):        # the bolts along the front plate's head
            v.edge_bolts(g_at(Hc - 0.05, y0), g_at(Hc - 0.05, y1), gn)

    # ---- the driver's place behind his flap: a seat with a back, the two steering levers, his dials ----------------
    v.use("hull")
    sx = 2.47 - (fl + 0.6 - zs) * (2.47 - xf) / (zw - zs) - 0.56
    v.slab((sx - 0.14, dy - 0.15, fl), (sx + 0.14, dy + 0.15, fl + 0.1), coat, 0.006)
    v.box((0.38, 0.38, 0.07), (sx, dy, fl + 0.13), "canvas.seat", 0.025)
    v.box((0.07, 0.38, 0.44), (sx - 0.2, dy, fl + 0.36), "canvas.seat", 0.025)
    if fine:
        for e in (-0.13, 0.13):
            v.rod((sx + 0.42, dy + e, fl), (sx + 0.36, dy + e, fl + 0.52), 0.012, "steel", 6)
            v.ball((sx + 0.36, dy + e, fl + 0.53), 0.022, "black", 8, 6)
        v.box((0.16, 0.36, 0.05), g_at(Hc * 0.26, dy) - gn * (th + 0.03), "black", 0.006, turn=_turn(gl, gn))

    # ---- the pulpit: a drum of plate in her right front corner, taller than the walls, a ring round its top -----
    mg = None
    if pulpit:
        cx, cy, r, zp = pulpit
        n = v.n(40, 14)
        v.lathe((cx, cy, zs), Z, [(zp - zs, r - th), (zp - zs, r), (0.0, r), (0.0, r - th), (zp - zs, r - th)], coat, n)
        v.lathe((cx, cy, zp - 0.892), Z, [(0.012, 0.0), (0.012, r - th)], "shade", n)                     # the floor he stands on, waist-deep: the gun's grips 1.08 over it
        v.tube([(cx + (r - 0.01) * math.cos(2 * math.pi * k / n), cy + (r - 0.01) * math.sin(2 * math.pi * k / n), zp + 0.015) for k in range(n)], 0.02, "steel", 6, closed=True)
        if fine:
            v.weld((cx - r - 0.001, cy, zs + 0.3), (cx - r - 0.001, cy, zp - 0.02), -X)                  # the seam up its back
            out = Vector((0.0, -1.0, 0.0))
            v.handle(Vector((cx - 0.12, cy - r, zp - 0.2)), Vector((cx + 0.12, cy - r, zp - 0.2)), out, 0.009, 0.04, "steel")
            v.box((0.2, 0.07, 0.012), (cx, cy - r - 0.035, zs + 0.42), coat, 0.003)                       # the step he climbs in by
            for k in range(16):
                a = 2 * math.pi * (k + 0.5) / 16
                d = Vector((math.cos(a), math.sin(a), 0.0))
                v.bolt(Vector((cx, cy, zp - 0.05)) + d * (r + 0.0005), d, 0.011, 0.007)
            for k in range(8):                              # the ring's brackets
                a = 2 * math.pi * (k + 0.5) / 8
                v.box((0.03, 0.05, 0.05), (cx + (r - 0.012) * math.cos(a), cy + (r - 0.012) * math.sin(a), zp - 0.012), coat, 0.004)
        # the heavy machine gun on its cradle on the ring, laid ahead
        pin = Vector((cx + r - 0.03, cy + 0.06, zp + 0.03))
        v.rod(pin, pin + Z * 0.17, 0.02, "steel", 8)
        v.box((0.30, 0.09, 0.11), pin + Z * 0.20 + X * -0.05, "steel", 0.008)
        v.mg(pin + Z * 0.21 + X * 0.08, X, 0.95, r=0.02)
        v.box((0.16, 0.11, 0.2), pin + Z * 0.15 + Y * 0.11 + X * -0.02, "upper", 0.006)
        if fine:
            v.tube([pin + Z * 0.2 + X * -0.2, pin + Z * 0.2 + X * -0.3 + Y * 0.05, pin + Z * 0.14 + X * -0.3 + Y * 0.05], 0.01, "steel", 6)      # the grips
        v.socket("mg", pin + Z * 0.21 + X * 1.05, X, part="hull", note="the heavy machine gun on the pulpit's ring")
        v.socket("crew.mg", (cx + r - 0.73, cy + 0.11, zp - 0.88), X, part="hull",
                 note="the machine-gunner stands here on the pulpit's floor, waist-deep in it: the gun's spade grips are 0.40 ahead of his feet and 1.08 up")
        mg = "mg"

    # ---- the engine deck and the tail, as the tank's -------------------------------------------------------------
    v.use("hull")
    m4_deck(v, zr, caps=False)
    m4_tail(v)
    for s in (LEFT, RIGHT):
        v.mark(tag, (-0.1, s * (sw + 0.002), (zs + zback) / 2 + 0.04), Y * s, 0.2, part="hull", shown="star" if tag == "UK" else None)
    m4_gear(v, hw)
    v.use("hull")
    return {"hw": hw, "sw": sw, "zs": zs, "zr": zr, "xe": xe, "floor": fl, "g_at": g_at, "gn": gn, "gl": gl, "Hc": Hc, "th": th, "zback": zback, "mg": mg, "driver_y": dy}


def _open_sockets(v, h, zw, gunner, loaders, commander, case, notes=None):
    """The detachment's places in her open box, and what happens to her. notes: her own words for a place
    ({"commander": ..., "crew.gunner": ..., "crew.loader": ...}) where the common ones do not fit."""
    notes = notes or {}
    zs, zr, sw, xe, fl = h["zs"], h["zr"], h["sw"], h["xe"], h["floor"]
    g_at, gn = h["g_at"], h["gn"]
    v.socket("commander", commander, X, part="hull", note=notes.get("commander", "the gun's commander stands here in the open box"))
    v.socket("crew.gunner", gunner, X, part="turret", note=notes.get("crew.gunner", "the layer, at the sight and the handwheels on the left of the breech"))
    for k, at in enumerate(loaders):
        v.socket("crew.loader" if k == 0 else f"crew.loader{k + 1}", at, X, part="hull", note=notes.get("crew.loader", "a loader stands here, behind the breech"))
    v.socket("case", case, -X, part="hull", note="where the empty case falls, on her floor")
    v.socket("engine", (-1.9, 0.0, 1.76), Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", (-1.9, 0.4, 1.76), Z, part="hull")
    v.socket("fire.2", (0.1, 0.0, zw - 0.2), Z, part="hull", note="out of her open box")
    v.socket("fire.3", g_at(h["Hc"] * 0.58, h["driver_y"]) + gn * 0.05, gn, part="hull", note="out of the driver's flap")
    v.socket("ring", (0.2, 0.0, fl), Z, part="hull", note="the middle of her floor: where the blast comes from when her ammunition goes")
    v.socket("hit.front", g_at(0.5, 0.3), gn, part="hull")
    v.socket("hit.left", (0.2, sw, 1.5), Y, part="hull")
    v.socket("hit.right", (0.2, -sw, 1.5), -Y, part="hull")
    v.socket("hit.rear", (-2.7, 0.0, 1.3), -X, part="hull")
    v.socket("hit.turret", (0.4, sw, zw - 0.25), Y, part="hull", note="the open box's side")


def m7(fine=True, tag="USA"):
    """M7 howitzer motor carriage of 1942: the 105 mm howitzer on the M4's hull, in an open box with a round pulpit
    for a heavy machine gun at its right front corner. 6.02 m long, 2.87 m wide, 2.54 m to the pulpit's rim."""
    v = SpWorks("M7", "M7 105 mm howitzer motor carriage", "tank", tag, fine, paints={"canvas.seat": "5A5B40", "canvas.net": "34361F", "canvas.tarp": "6E6A4C"})
    v.root(hull_pivot=(0.0, 0.0, 0.85))
    zw = 2.04
    h = m4_open_hull(v, zw, 1.90, 1.72, (-0.81, 0.31, 1.36), pulpit=(1.18, -0.98, 0.47, 2.48), driver=LEFT, step=(0.0, 1.90), tag=tag)
    sw, zs, zr, xe, fl, th = h["sw"], h["zs"], h["zr"], h["xe"], h["floor"], h["th"]
    px, pz, zt, gy = 1.37, 1.30, 1.81, -0.15            # (the howitzer stands right of her middle line, just inboard of the pulpit)
    here = v.here()
    v.pedestal((px, 0.0, pz), fl, r=0.32)
    v.saddle(px, zt, 0.22, (15.0, 30.0), rate=6.0, foot=0.32)
    v.cradle(-5.0, 35.0, 0.32, 0.2, 1.2, 0.55, 0.16, over=(-0.45, 1.25, 0.07, 0.235))
    xb = v.piece(2.36, 0.105, 0.78, 0.14, 0.1, 1.07, steps=((0.45, 0.118),), ring=(0.3, 0.36, 0.36), block="right", lug=0.5)
    v.handwheels((px - 0.3, 0.42, zt - 0.14), (px - 0.42, 0.34, zt - 0.30), r=0.12, grips="gunner")
    v.sight((px - 0.2, 0.32, zt + 0.04), high=0.26)
    # (her trunnions are only 0.73 over the floor: the layer kneels at the wheels, the sight at his eye; a seat would put the wheels in his lap)
    v.shift(here, (0.0, gy, 0.0))
    # ---- inside her: the rounds standing along each sponson, seats, the things a detachment lives with -----------
    v.rounds(-0.92, 0.36, 0.88, sw - 0.05, zs, 0.50)
    v.rounds(-0.92, 0.22, -(sw - 0.05), -0.88, zs, 0.50)
    v.use("hull")
    for at in ((-0.6, 0.68), (-0.6, -0.68)):              # two seats on posts, against the racks
        v.rod((at[0], at[1], fl), (at[0], at[1], fl + 0.34), 0.022, "steel", 8)
        v.box((0.32, 0.32, 0.06), (at[0], at[1], fl + 0.37), "canvas.seat", 0.02)
    v.rammer((xe + th + 0.035, -0.95, 1.775), (xe + th + 0.035, 0.95, 1.775))                           # across the back wall
    for y in (-0.42, 0.42):                                # the detachment's packs on the bulkhead under it
        v.pack((xe, y, 1.46), X)
    v.jerrycan((0.42, -1.14, zs), 0.0)                    # water and oil, in the corner behind the pulpit
    v.jerrycan((0.42, -0.95, zs), 0.0)
    v.crate((0.74, 1.08, zs), (0.5, 0.3, 0.24), 0.0)     # a box of fuzes under the wireless, a tarpaulin folded on it
    v.tarp((0.74, 1.08, zs + 0.245), (0.46, 0.3, 0.08), 0.0, "canvas.tarp")
    for x, y, yaw in ((0.40, -0.52, 60.0), (-0.15, 0.0, -25.0)):        # two empty cases thrown down behind the breech, clear of the men's feet (and far enough forward for the battle camera to see them over the back wall)
        v.spent((x, y, fl), yaw, 0.06, 0.372)
    if fine:
        v.rod((0.62, sw - 0.09, 1.72), (0.62, sw - 0.09, 1.98), 0.05, "red", 12)                       # the fire extinguisher on the wall
        v.box((0.34, 0.14, 0.24), (0.95, sw - 0.11, 1.80), "shade", 0.008)                             # the wireless
        v.helmet((-0.45, sw - th - 0.05, 1.70), (0.0, -1.0, 0.2))                                      # helmets hung on the walls over the racks (clear of the plate: a brim must never come through it)
        v.helmet((-0.15, -(sw - th - 0.05), 1.70), (0.0, 1.0, 0.2))
        v.helmet((-0.5, -(sw - th - 0.05), 1.70), (0.0, 1.0, 0.2))
    # ---- outside her: bins on the engine deck, the net rolled between them, a rolled tarpaulin and the packs on her
    #      side, the tools across her tail ---------------------------------------------------------------------------
    for s in (LEFT, RIGHT):
        v.bin((-1.58, s * 0.74, zr), (-1.06, s * 1.29, zr + 0.25), bevel=0.02, lid_side=(0, s, 0), latches=2)
    v.roll((-1.112, -0.72, zr + 0.09), (-1.112, 0.72, zr + 0.09), 0.088, paint="canvas.net", straps=3)      # (clear of the engine's air intake)
    v.tarp((-1.32, 1.02, zr + 0.254), (0.44, 0.42, 0.09), 0.0, "canvas.tarp")
    v.helmet((-1.30, 1.0, zr + 0.344))
    v.bucket((-1.32, -1.02, zr + 0.254), 0.12, 0.24)
    v.roll((1.5, sw + 0.12, 1.42), (0.3, sw + 0.12, 1.42), 0.11, paint="canvas.tarp", straps=3)
    for x in (0.52, 0.94):
        v.pack((x, sw, 1.76), Y)
    if fine:
        v.cable([(1.9, -sw - 0.025, 1.22), (0.4, -sw - 0.025, 1.16), (-0.9, -sw - 0.025, 1.24)], 0.014)
        rb = Vector((-2.58 + 2.76, 0, 1.66 - 1.18)).normalized()
        rn = Vector((-rb.z, 0, rb.x))
        r_at = lambda u, y: Vector((-2.76, y, 1.18)) + rb * u + rn * 0.002      # noqa: E731
        v.shovel(r_at(0.30, -0.5) + rn * 0.02, 90.0, 0.95)
        v.axe(r_at(0.50, -0.38) + rn * 0.02, 90.0, 0.78)
    v.aerial("aerial", "hull", (0.95, sw - th / 2, zw), 2.4)
    _open_sockets(v, h, zw, gunner=(px - 0.57, 0.60 + gy, fl), loaders=[(xb - 0.8, -0.3 + gy, fl), (xb - 0.55, 0.42 + gy, fl)], commander=(-0.72, 0.30, fl), case=(xb - 0.5, -0.2 + gy, fl),
                  notes={"crew.gunner": "the layer kneels here on her floor, left of the breech (the point is the floor under his hips): the handwheels' hubs are 0.43 and 0.60 over the floor, their handles 0.16 and 0.39 ahead of him, the sight 0.3 ahead at his eye (his hands: grip.gunner.l, grip.gunner.r)",
                         "commander": "the gun's commander stands here in the open box: its sides come to his waist (0.92 over the floor), so he stands free",
                         "crew.loader": "a loader stands here, behind the breech: it is 0.73 over the floor, at his waist, and he bends to it"})
    v.tank_clips(recoil=1.0, kick=0.7, gun_low=-5.0, hatches=["hatch_driver"], traverse=6.0, idle_cue="tank_idle_sherman_loop", drive_cue="tank_sherman_drive_loop",
                 gun_cue="field_gun_fire", has_turret=False, mg="mg")
    v.name_place({LEFT: (1.51, 1.64), RIGHT: (1.87, 1.34)}, long=0.68, high=0.12, lines=(2, 1), high_two=0.10)    # her crew's name (b477): on the box's side, forward; the pulpit takes the right side's upper plate
    return v


def sexton(fine=True, tag="UK"):
    """Sexton of 1943: the 25-pounder on the same hull (built in Canada on the Ram's), in a plain tall open box with
    no pulpit, the driver on the right. 6.12 m long, 2.72 m wide, 2.44 m high."""
    v = SpWorks("Sexton", "Sexton, 25-pounder self-propelled", "tank", tag, fine, paints={"canvas.seat": "5A5B40", "canvas.net": "34361F", "canvas.tarp": "6E6A4C"})
    v.root(hull_pivot=(0.0, 0.0, 0.85))
    zw = 2.16
    h = m4_open_hull(v, zw, zw, 1.88, (-0.34, 0.74, 1.40), pulpit=None, driver=RIGHT, step=None, tag=tag)
    sw, zs, zr, xe, fl, th = h["sw"], h["zs"], h["zr"], h["xe"], h["floor"], h["th"]
    px, pz, zt, gy = 1.25, 1.40, 1.90, 0.12             # (the 25-pounder stands left of her middle line, the driver on its right)
    here = v.here()
    v.pedestal((px, 0.0, pz), fl, r=0.34)
    v.saddle(px - 0.05, zt, 0.2, (25.0, 15.0), rate=4.0, foot=0.3)
    v.cradle(-9.0, 40.0, 0.27, 0.19, 1.45, 0.45, 0.13)
    xb = v.piece(2.47, 0.0876, 0.8, 0.118, 0.07, 0.5, brake="double", steps=((0.5, 0.095),), ring=(0.27, 0.3, 0.31), block="down", lug=0.5)
    v.handwheels((px - 0.37, 0.38, zt - 0.12), (px - 0.49, 0.31, zt - 0.26), r=0.11, grips="gunner")
    v.sight((px - 0.29, 0.3, zt + 0.02), high=0.22)
    # (her wheels are 0.56 and 0.70 over the floor: the layer kneels at them, the sight at his eye; a seat would put the traversing wheel in his lap)
    v.shift(here, (0.0, gy, 0.0))
    # ---- inside her: the rounds along each sponson, seats, the wireless, the detachment's kit ----------------------
    v.rounds(-0.92, 0.50, 0.90, sw - 0.05, zs, 0.44, r=0.046, gap=0.115)
    v.rounds(-0.92, 0.30, -(sw - 0.05), -0.90, zs, 0.44, r=0.046, gap=0.115)
    v.use("hull")
    for at in ((-0.6, 0.70), (-0.6, -0.70)):
        v.rod((at[0], at[1], fl), (at[0], at[1], fl + 0.34), 0.022, "steel", 8)
        v.box((0.32, 0.32, 0.06), (at[0], at[1], fl + 0.37), "canvas.seat", 0.02)
    v.rammer((-0.95, sw - th - 0.036, 2.02), (0.72, sw - th - 0.036, 2.02))                           # along the left wall, under its top
    for y in (-0.62, 0.0, 0.62):                           # the detachment's packs along the back wall
        v.pack((xe + th, y, 1.84), X)
    v.crate((0.72, -1.08, zs), (0.5, 0.3, 0.2), 0.0, "upper")                                          # charge boxes ahead of the right rack
    v.crate((0.74, -1.08, zs + 0.2), (0.5, 0.3, 0.2), 4.0, "upper")
    v.jerrycan((0.92, 1.12, zs), 0.0)                     # water, ahead of the left rack
    v.tarp((1.36, 1.02, zs), (0.46, 0.4, 0.12), 90.0, "canvas.tarp")
    for x, y, yaw in ((0.22, -0.58, 75.0), (0.10, 0.20, -20.0)):        # two empty cases thrown down behind the breech, clear of the men's feet (and far enough forward for the battle camera to see them over the back wall)
        v.spent((x, y, fl), yaw, 0.046, 0.292)
    if fine:
        v.rod((0.9, sw - 0.09, 1.80), (0.9, sw - 0.09, 2.06), 0.05, "red", 12)
        v.box((0.40, 0.16, 0.26), (-0.75, -(sw - 0.12), 1.86), "shade", 0.008)                          # the wireless, at the back on the right
        for x in (-0.28, 0.04):                            # helmets hung on the right wall
            v.helmet((x, -(sw - th - 0.06), 1.86), (0.0, 1.0, 0.2), kind="brodie")
        v.helmet((0.74, -1.08, zs + 0.405), kind="brodie")
    # ---- outside her: a bin each side of the engine deck, her deck piled, bedding rolled along her side, packs on
    #      the other, the tools on her tail ------------------------------------------------------------------------------
    for s in (LEFT, RIGHT):
        v.bin((-2.30, s * 0.80, 1.71), (-1.66, s * 1.29, 1.93), bevel=0.02, lid_side=(0, s, 0), latches=2)
    v.roll((1.5, -(sw + 0.12), 1.50), (0.35, -(sw + 0.12), 1.50), 0.11, paint="canvas.tarp", straps=3)
    for x in (0.36, 0.74):
        v.pack((x, sw, 1.76), Y)
    v.roll((-1.10, 1.08, zr + 0.13), (-1.62, 1.08, zr + 0.13), 0.13, paint="canvas.net", straps=2)
    v.tarp((-1.98, 1.045, 1.934), (0.5, 0.42, 0.1), 0.0, "canvas.tarp")                               # on the bins: a tarpaulin and a bucket, a box of rations
    v.bucket((-1.98, 1.045, 2.034), 0.12, 0.24)
    v.crate((-1.98, -1.045, 1.934), (0.55, 0.3, 0.25), 0.0)
    v.jerrycan((-1.3, -0.92, zr), 0.0)                    # (all clear of the engine's air intake)
    v.jerrycan((-1.3, -1.11, zr), 0.0)
    if fine:
        v.cable([(1.9, sw + 0.025, 1.22), (0.4, sw + 0.025, 1.16), (-0.9, sw + 0.025, 1.24)], 0.014)
        rb = Vector((-2.58 + 2.76, 0, 1.66 - 1.18)).normalized()
        rn = Vector((-rb.z, 0, rb.x))
        r_at = lambda u, y: Vector((-2.76, y, 1.18)) + rb * u + rn * 0.002      # noqa: E731
        v.shovel(r_at(0.30, -0.5) + rn * 0.02, 90.0, 0.95)
        v.axe(r_at(0.50, -0.38) + rn * 0.02, 90.0, 0.78)
    v.aerial("aerial", "hull", (-0.9, -(sw - th / 2), zw), 2.4)
    _open_sockets(v, h, zw, gunner=(px - 0.68, 0.50 + gy, fl), loaders=[(xb - 0.6, -0.35 + gy, fl), (xb - 0.45, 0.45 + gy, fl)], commander=(-0.74, 0.10, fl), case=(xb - 0.5, -0.2 + gy, fl),
                  notes={"crew.gunner": "the layer kneels here on her floor, left of the breech (the point is the floor under his hips): the handwheels' handles are 0.58 and 0.70 over the floor, 0.20 and 0.42 ahead of him, the sight at his eye (his hands: grip.gunner.l, grip.gunner.r)"})
    v.tank_clips(recoil=0.5, kick=0.6, gun_low=-9.0, hatches=["hatch_driver"], traverse=4.0, idle_cue="tank_idle_sherman_loop", drive_cue="tank_sherman_drive_loop",
                 gun_cue="field_gun_fire", has_turret=False, mg=None)
    v.name_place((1.0, 1.7), long=0.9, high=0.12, lines=(2, 1), high_two=0.10)                     # her gunners' name (b477): on the box's side
    return v


def hummel(fine=True, tag="GER"):
    """Hummel of 1943: the 15 cm heavy field howitzer on a lengthened Panzer IV hull, her engine moved to the middle
    under the gun, a tall open box of thin plate over her back half, the drivers' cab across her front. 7.17 m over
    the gun, 2.97 m wide, 2.81 m high. She came after the grey: dark yellow from her first day."""
    v = SpWorks("Hummel", "Hummel, 15 cm self-propelled howitzer", "tank", tag, fine, paint=GER_COATS["yellow"][0],
                paints={"canvas.seat": "4A4638", "canvas.tarp": "5E5A44", "shade.shell": "5A624C", "shade.stencil": "B4B094", "brass.band": "8E5A3A", "torn.mud": "52472F", "torn.mud2": "5B5135"})
    v.root(hull_pivot=(0.0, 0.0, 0.75))
    m = v.m
    hw, zg, xr, th = 0.925, 1.10, -3.04, 0.03            # half-width between the tracks; the guards; her tail; the box's plate
    # ---- the hull: the Panzer IV's tub, a hand longer behind -------------------------------------------------
    tub = [(xr + 0.12, 0.44), (xr, 0.66), (xr, zg), (1.72, zg), (2.52, 1.00), (2.62, 0.64), (2.25, 0.40), (xr + 0.34, 0.40)]
    v.hull([(x, hw, z) for x, z in tub], coat, bevel=0.014)
    for s in (LEFT, RIGHT):
        y0, y1 = s * 0.93, s * 1.44
        v.slab((xr - 0.06, y0, 1.085), (2.62, y1, 1.10), coat, 0.004)
        v.plate([(2.62, y0, 1.10), (2.62, y1, 1.10), (2.94, y1, 0.90), (2.94, y0, 0.90)][::int(s)], 0.012, "upper", 0.003)
        v.plate([(xr - 0.06, y1, 1.10), (xr - 0.06, y0, 1.10), (xr - 0.26, y0, 0.84), (xr - 0.26, y1, 0.84)][::int(s)], 0.012, "upper", 0.003)
        if fine:
            for x in (-2.5, -1.5, -0.5, 0.5, 1.4, 2.2):    # the brackets under the guards
                v.slab((x - 0.015, s * 0.93, 0.98), (x + 0.015, s * 1.40, 1.085), "shade")
            v.slab((xr - 0.06, s * 1.425, 1.06), (2.62, s * 1.44, 1.10), "upper")
    # the nose: the bolted brake and gearbox hatches on the glacis, spare links across the lower plate
    gl = Vector((2.52 - 1.72, 0, 1.00 - 1.10)).normalized()
    gn = Vector((-gl.z, 0, gl.x))
    for y in (-0.52, 0.52):
        c = Vector((2.12, y, 1.10 + (2.12 - 1.72) * (1.00 - 1.10) / (2.52 - 1.72)))
        v.put(bm_box((0.5, 0.44, 0.022), c + gn * 0.011, rot(0, math.degrees(math.atan2(gl.z, gl.x)))), "upper", 0.006)
        if fine:
            for dx in (-0.2, 0.2):
                for dy in (-0.17, 0.17):
                    v.bolt(c + gl * dx + Y * dy + gn * 0.022, gn, 0.012)
            v.vent(c + gl * 0.02 + gn * 0.02, 0.07, 0.035)
    nf = Vector((0.36, 0, 0.10)).normalized()
    v.spare_links(Vector((2.57, -0.78, 0.82)) + nf * 0.002, Vector((2.57, 0.78, 0.82)) + nf * 0.002, 13, 0.12, 0.34, nf, style={"kind": "plate", "grouser": 0.014})
    for y in (-0.62, 0.62):
        v.hook((2.50, y, 0.50), Vector((0.8, 0, -0.5)).normalized(), 0.085)
        v.hook((xr, y, 0.62), -X, 0.085)
    # ---- the drivers' cab across her front: a low box, its face leaning back, a visor and a lid for each man --
    zc = 1.60
    v.hull([(1.74, 0.92, zg), (1.50, 0.90, zc), (1.00, 0.90, zc + 0.01), (1.00, 0.92, zg)], coat, bevel=0.014)
    cf = Vector((1.50 - 1.74, 0, zc - zg)).normalized()
    cn = Vector((cf.z, 0, -cf.x))
    for s, lid in ((LEFT, "hatch_driver"), (RIGHT, "hatch_radio")):
        v.visor(Vector((1.74, s * 0.50, zg)) + cf * 0.33 + cn * 0.002, cn, 0.34, 0.12, 0.06)
        v.hatch_rect(lid, "hull", (1.25, s * 0.50, zc + 0.008), (0.40, 0.46), Z, X, hinge="front", opens=105.0)
        v.use("hull")
        if fine:
            v.port((1.25, s * 0.902, 1.40), Y * s, 0.045)
    if fine:
        v.weld((1.745, -0.88, zg + 0.004), (1.745, 0.88, zg + 0.004), X)
    # ---- the engine under its covers between the cab and the box, the air louvres built out each side --------
    ze = 1.50
    v.hull([(1.00, 1.36, zg), (1.00, 1.30, ze), (0.30, 1.30, ze), (0.30, 1.36, zg)], coat, bevel=0.014)
    for s in (LEFT, RIGHT):
        v.slab((0.42, s * 0.08, ze), (0.96, s * 1.18, ze + 0.018), "upper", 0.006)
        v.grille((0.69, s * 0.63, ze + 0.018), (0.40, 0.86), Z, Y, slats=7, deep=0.02)
        v.slab((0.36, s * 1.33, 1.16), (0.96, s * 1.41, 1.44), "upper", 0.012)
        v.louvres((0.66, s * 1.411, 1.30), (0.52, 0.24), Y * s, 4)
        if fine:
            v.handle((0.47, s * 0.2, ze + 0.018), (0.47, s * 0.4, ze + 0.018), Z, 0.007, 0.035, "steel")
    # ---- the box: thin plate leaning in, open above; its face is a shield with a slot the piece looks through --
    zt0, zt1, xs0, xs1, xk = 2.76, 2.36, 0.42, -0.12, -1.70    # the top at the front and at the tail; the shield's foot and head; where the top starts to fall
    y_at = lambda z: 1.40 - (z - zg) * 0.18 / (zt0 - zg)      # noqa: E731  (how far out the side plate stands at a height)
    sn = [Vector((0, s * (zt0 - zg), 0.18)).normalized() for s in (1.0, -1.0)]
    side = [(xr + 0.02, zg), (xs0, zg), (xs0, ze), (xs1, zt0), (xk, zt0), (xr + 0.02, zt1)]
    top_at = lambda x: zt0 if x >= xk else zt1 + (x - xr - 0.02) * (zt0 - zt1) / (xk - xr - 0.02)      # noqa: E731  (her top edge over a place along her)
    sf_ = Vector((zt0 - ze, 0, xs0 - xs1)).normalized()                                                   # (out of the shield)
    for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
        wall(v, [(x, s * y_at(z), z) for x, z in side][::int(s)], -n, th, coat, 0.004)
        if fine:
            v.weld((-1.30, s * (y_at(zg + 0.03) + 0.001), zg + 0.03), (-1.30, s * (y_at(zt0 - 0.03) + 0.001), zt0 - 0.03), n)
            for k in range(12):                              # the bolts along her foot, into the guard
                v.bolt((xr + 0.3 + 0.28 * k, s * (y_at(zg + 0.05) + 0.001), zg + 0.05), n, 0.012, 0.008)
            # the plate's own work: bolts along her top edge, the angle inside it, stiffeners up the inside, the seam at the shield
            edge = [(xs1 - 0.06, zt0), (xk, zt0), (xr + 0.1, top_at(xr + 0.1))]
            for (xa, za), (xb_, zb_) in zip(edge, edge[1:]):
                v.edge_bolts((xa, s * (y_at(za - 0.05) + 0.0005), za - 0.05), (xb_, s * (y_at(zb_ - 0.05) + 0.0005), zb_ - 0.05), n)
                v.rib((xa, s * (y_at(za - 0.05) - th), za - 0.05), (xb_, s * (y_at(zb_ - 0.05) - th), zb_ - 0.05), -n, deep=0.045, wide=0.012)
            for x in (-2.45, -1.72, -0.9):
                z = top_at(x) - 0.056
                v.rib((x, s * (y_at(zg + 0.01) - th), zg + 0.01), (x, s * (y_at(z) - th), z), -n)
            v.weld((xs0, s * (y_at(ze) + 0.001), ze), (xs1, s * (y_at(zt0) + 0.001), zt0 - 0.01), (n + sf_).normalized())
            v.weld((xr + 0.02, s * (y_at(zg) - 0.004), zg + 0.02), (xr + 0.02, s * (y_at(zt1) - 0.004), zt1 - 0.02), (n - X).normalized())
            for x in (-2.5, -0.9, -0.3):                    # loops to lash her stowage to
                v.handle(Vector((x - 0.04, s * y_at(2.4), 2.4)), Vector((x + 0.04, s * y_at(2.4), 2.4)), n, 0.006, 0.024, "steel")
        v.mark("GER", (-2.1, s * (y_at(1.86) + 0.002), 1.86), n, 0.2, part="hull")
    sl = Vector((xs1 - xs0, 0, zt0 - ze))
    Hs = sl.length
    sl = sl.normalized()
    sf = Vector((sl.z, 0, -sl.x))
    slot, bn = 0.52, (1.80 - ze) / sl.z
    sheet(v, [(-y_at(ze), 0.0), (y_at(ze), 0.0), (y_at(zt0), Hs), (slot, Hs), (slot, bn), (-slot, bn), (-slot, Hs), (-y_at(zt0), Hs)], [], (xs0, 0.0, ze), Y, sl, th, coat, 0.004)
    if fine:
        s_at = lambda u, y: Vector((xs0, y, ze)) + sl * u + sf * 0.0005      # noqa: E731  (a place on the shield, u metres up it)
        for e in (LEFT, RIGHT):
            v.edge_bolts(s_at(Hs - 0.05, e * (slot + 0.08)), s_at(Hs - 0.05, e * (y_at(zt0) - 0.1)), sf)
            v.box((Hs - bn, 0.05, 0.012), s_at((Hs + bn) / 2, e * (slot + 0.03)) + sf * 0.006, coat, 0.003, turn=_turn(sl, sf))      # a strip up each side of the slot
        v.weld(s_at(0.01, -y_at(ze) + 0.05), s_at(0.01, y_at(ze) - 0.05), sf)
    # the tail plate: two doors, their hinges and a handle
    yb0, yb1 = y_at(zg), y_at(zt1)
    wall(v, [(xr + 0.02, -yb0, zg), (xr + 0.02, yb0, zg), (xr + 0.02, yb1, zt1), (xr + 0.02, -yb1, zt1)], X, th, coat, 0.004)
    for s in (LEFT, RIGHT):
        v.slab((xr + 0.008, s * 0.012, zg + 0.08), (xr + 0.02, s * 0.62, zt1 - 0.1), "upper", 0.004)
        if fine:
            for z in (zg + 0.3, zt1 - 0.32):
                v.hinge((xr + 0.004, s * 0.63, z - 0.05), (xr + 0.004, s * 0.63, z + 0.05), 0.014)
            v.handle((xr + 0.008, s * 0.12, 1.72), (xr + 0.008, s * 0.12, 1.86), -X, 0.008, 0.04, "steel")
            v.edge_bolts((xr + 0.0195, s * 0.7, zt1 - 0.05), (xr + 0.0195, s * (yb1 - 0.1), zt1 - 0.05), -X)
            v.rib((xr + 0.02 + th, s * 0.66, zg + 0.01), (xr + 0.02 + th, s * 0.66, zt1 - 0.02), X)                  # the doors' posts, inside
            v.jerrycan((xr - 0.065, s * 0.98, zg + 0.16), 90.0)                                                  # a can each side of the doors, in a strap frame
            v.slab((xr - 0.15, s * 0.79, zg + 0.14), (xr + 0.02, s * 1.17, zg + 0.16), "upper", 0.004)
            v.slab((xr - 0.153, s * 0.79, zg + 0.36), (xr + 0.02, s * 1.17, zg + 0.39), "steel")
    # her floor, and the engine's cover inside the box's front that the gun stands on
    v.slab((xr + 0.04, -1.36, zg), (xs0 - 0.02, 1.36, zg + 0.008), "shade")
    v.slab((-0.80, -0.64, zg), (xs0 - 0.03, 0.64, 1.52), coat, 0.012)
    if fine:
        v.bolts((-0.55, 0.0, 1.52), Z, 0.0, 1, r=0.012)
        for s in (LEFT, RIGHT):
            v.handle((-0.72, s * 0.3, 1.52), (-0.72, s * 0.5, 1.52), Z, 0.007, 0.035, "steel")

    # ---- the piece: the heavy field howitzer's own saddle and cradle, a small shield of its own before the slot --
    px, pz, zt = -0.25, 1.60, 2.10
    v.pedestal((px, 0.0, pz), 1.52, r=0.40)
    v.saddle(px, zt, 0.26, 15.0, rate=3.0, foot=0.40)
    v.cradle(-3.0, 42.0, 0.36, 0.24, 1.25, 0.7, 0.19, over=(-0.55, 1.75, 0.09, 0.29))
    if True:
        v.use("gun")                                       # (it lifts with the cradle)
        gs = [(-0.42, -0.50), (0.42, -0.50), (0.42, 0.36), (0.32, 0.46), (0.215, 0.46), (0.215, -0.20), (-0.215, -0.20), (-0.215, 0.46), (-0.32, 0.46), (-0.42, 0.36)]
        sheet(v, gs, [], (px + 0.56, 0.0, zt), Y, Z, 0.022, coat, 0.003)
        if fine:
            for s in (LEFT, RIGHT):
                v.rod((px + 0.55, s * 0.30, zt - 0.3), (px + 0.2, s * 0.2, zt - 0.3), 0.012, coat, 6)
    xb = v.piece(4.44, 0.149, 0.62, 0.19, 0.112, 1.1, steps=((0.35, 0.152),), ring=(0.4, 0.46, 0.46), block="right", lug=0.6)
    v.handwheels((px - 0.34, 0.48, zt - 0.14), (px - 0.46, 0.40, zt - 0.30), r=0.13, grips="gunner")
    v.sight((px - 0.22, 0.37, zt + 0.04), high=0.3)
    if fine:                                               # the two spring equilibrators, standing either side of the cradle
        v.use("turret")
        for s in (LEFT, RIGHT):
            v.rod((px + 0.26, s * 0.36, pz + 0.12), (px + 0.18, s * 0.36, zt - 0.16), 0.06, coat, 14)
            v.rod((px + 0.18, s * 0.36, zt - 0.16), (px + 0.15, s * 0.32, zt - 0.05), 0.022, "steel", 8)
    # ---- inside her: the cartridge cases standing at the back on the left, the shells on the right, a locker along
    #      the left wall that the detachment sits on, the box's canvas cover rolled along the right, their kit ------
    v.rounds(-2.9, -1.75, 0.82, 1.24, zg, 0.5, r=0.075, gap=0.19)
    v.shells(-2.9, -1.75, -1.24, -0.82, zg, 0.074, 0.60, 0.19)
    # her floor between the racks and the breech: the path the loaders have trodden, the empty cases they threw down
    v.trodden(((-1.5, -0.75, 0.42, 0.24, 40.0), (-1.5, 0.72, 0.40, 0.22, -40.0), (-1.25, -0.3, 0.45, 0.26, 35.0),
               (-1.3, 0.25, 0.42, 0.25, -30.0), (-1.5, 0.0, 0.5, 0.3, 0.0), (-2.3, -0.1, 0.36, 0.24, 15.0)), zg + 0.008)
    for x, y, yaw in ((-1.10, -0.70, 65.0), (-1.50, -0.68, -12.0), (-2.2, 0.47, -20.0), (-2.72, -0.52, 110.0)):
        v.spent((x, y, zg + 0.008), yaw, 0.079, 0.26)
    v.use("hull")
    v.bin((-1.62, 0.92, zg), (-0.30, 1.26, zg + 0.42), bevel=0.012, lid_side=(0, -1, 0), latches=3)
    for x in (-1.3, -0.62):
        v.box((0.5, 0.3, 0.05), (x, 1.09, zg + 0.45), "canvas.seat", 0.02)
    v.roll((-1.62, -1.14, zg + 0.14), (-0.12, -1.14, zg + 0.14), 0.135, paint="canvas.tarp", straps=4)
    v.crate((0.0, 1.0, zg), (0.55, 0.32, 0.25), 0.0)                                                   # the charges' boxes, by the shield on the left
    v.crate((0.0, 1.0, zg + 0.25), (0.52, 0.32, 0.25), -3.0)
    v.jerrycan((0.12, -0.84, zg), 0.0, coat)                                                             # water and oil, by the shield on the right
    v.jerrycan((0.12, -1.03, zg), 0.0, coat)
    v.rammer((-2.5, y_at(2.3) - th - 0.036, 2.3), (-0.5, y_at(2.3) - th - 0.036, 2.3), head=0.07)       # the rammer along the left wall
    for x in (-2.25, -1.91):                               # packs on the left wall over the cases
        v.pack((x, y_at(2.0) - th, 2.0), -Y)
    if fine:
        v.box((0.42, 0.16, 0.28), (-0.5, -(y_at(2.0) - 0.12), 2.0), "shade", 0.008)                    # the wireless on the right wall
        v.rod((-0.3, y_at(2.2) - 0.08, 2.1), (-0.3, y_at(2.2) - 0.08, 2.36), 0.05, "red", 12)
        for x in (-1.45, -1.13):                           # helmets hung on the right wall
            v.helmet((x, -(y_at(2.2) - th - 0.07), 2.2), (0.0, 1.0, 0.25), kind="stahl", paint="shade")      # (clear of the leaning plate: two brims through it were a pair of eyes outside)
        v.helmet((0.0, 1.0, zg + 0.505), kind="stahl", paint="shade")
        v.bucket((-2.75, 0.3, zg), 0.12, 0.24)
    # ---- outside her: the lamp, the tools on the guards ahead of the box, the barrel's travelling lock folded on
    #      the glacis, the silencer across her tail --------------------------------------------------------------
    v.headlamp((2.38, 1.14, 1.10), X, 0.075, stalk=0.09)
    v.headlamp((2.38, -1.14, 1.10), X, 0.06, stalk=0.07, cover=True)
    v.jack((1.45, -1.22, 1.10), 0.0)
    v.shovel((1.55, 1.30, 1.125), 0.0, 0.95)
    v.crowbar((1.45, 1.12, 1.118), 0.0, 1.2)
    if fine:
        for s in (LEFT, RIGHT):                            # the lock's two legs, and the clamp at their head
            v.tube([(2.36, s * 0.40, 1.03), (1.86, s * 0.10, 1.15)], 0.022, coat, 8)
            v.lathe((2.36, s * 0.40, 1.03), Y, [(0.03, 0.0), (0.03, 0.03), (-0.03, 0.03), (-0.03, 0.0)], coat, 8)
        v.box((0.08, 0.26, 0.06), (1.84, 0.0, 1.16), coat, 0.012)
        for x in (-1.25, -0.5):                             # two spare road wheels on the left wall
            n = sn[0]
            v.wheel_disc(Vector((x, y_at(1.5), 1.5)) + n * 0.05, n, 0.235, 0.085, 0.032, bolts=6, sides=24)
        v.cable([(0.1, -(y_at(1.27) + 0.02), 1.27), (-0.8, -(y_at(1.255) + 0.02), 1.255), (-1.7, -(y_at(1.27) + 0.02), 1.27)], 0.013)      # low and straight along the wall's foot (hung in a V it grinned)
    v.silencer((xr - 0.18, -0.62, 0.93), (xr - 0.18, 0.46, 0.93), 0.15, pipe=[(xr, 0.2, 1.02), (xr - 0.12, 0.2, 1.0), (xr - 0.18, 0.2, 0.96)], tail=((xr - 0.18, -0.62, 0.93), (-0.3, -1, -0.2)))
    v.socket("exhaust.1", (xr - 0.24, -0.78, 0.9), (-0.3, -1, -0.2))
    v.aerial("aerial", "hull", (-0.5, y_at(zt0) - th / 2, zt0), 2.0)
    pz4_gear(v, xs=[c + d for c in (-1.7605, -0.6705, 0.4195, 1.5095) for d in (-0.2515, 0.2515)], idler=(-2.82, 0.64),
             rollers=[(-1.62, 0.905), (-0.56, 0.905), (0.50, 0.905), (1.56, 0.905)], dust_x=-3.05)
    # ---- the detachment's places, and what happens to her ---------------------------------------------------
    v.socket("commander", (-2.55, 0.0, zg), X, part="hull", note="the gun's commander stands here at the back of the open box")
    v.socket("crew.gunner", (px - 0.72, 0.55, zg), X, part="hull",
             note="the layer stands here, left of the breech: the handwheels' handles are at his waist, 0.72 and 0.86 over the floor and 0.27 and 0.51 ahead of him (his hands: grip.gunner.l, grip.gunner.r)")
    v.socket("crew.loader", (xb - 0.95, -0.38, zg), X, part="hull", note="a loader stands here, behind the breech")
    v.socket("crew.loader2", (xb - 0.75, 0.42, zg), X, part="hull", note="the second loader, with the cartridge")
    v.socket("case", (xb - 0.6, -0.2, zg), -X, part="hull", note="where the empty case falls, on her floor")
    v.socket("engine", (0.69, 0.0, ze + 0.05), Z, part="hull", note="the engine's covers under the gun: smoke and the first fire")
    v.socket("fire.1", (0.69, 0.6, ze + 0.05), Z, part="hull")
    v.socket("fire.2", (-1.6, 0.0, 2.3), Z, part="hull", note="out of her open box")
    v.socket("fire.3", (1.25, 0.50, zc + 0.05), Z, part="hull", note="out of the driver's hatch")
    v.socket("ring", (-1.6, 0.0, zg), Z, part="hull", note="the middle of her floor: where the blast comes from when her ammunition goes")
    v.socket("hit.front", (2.62, 0.2, 0.8), X, part="hull")
    v.socket("hit.left", (-1.0, y_at(1.9), 1.9), sn[0], part="hull")
    v.socket("hit.right", (-1.0, -y_at(1.9), 1.9), sn[1], part="hull")
    v.socket("hit.rear", (xr, 0.0, 1.6), -X, part="hull")
    v.socket("hit.turret", (0.1, 0.9, 2.2), sf, part="hull", note="the box's face")
    v.tank_clips(recoil=1.0, kick=1.1, gun_low=-3.0, hatches=["hatch_driver", "hatch_radio"], traverse=3.0, idle_cue="tank_idle_loop", drive_cue="tank_loop",
                 gun_cue="field_gun_fire_02", has_turret=False, mg=None)
    m.info.setdefault("file", {})["swaps"] = {"how": "paint: put the chosen coat's colours in place of the paints of the same names.", "paint": coats(v, "yellow", only=("yellow",))}
    return v


def su76(fine=True, tag="SOV"):
    """SU-76M of 1943: the 76 mm divisional gun on a lengthened light tank's hull, six small wheels a side, in an
    open box over her back half, the gun a little left of her middle with a plate box over its recoil gear, the
    driver under a hood in the middle of her glacis. 4.97 m long, 2.72 m wide, 2.10 m high."""
    v = SpWorks("SU76", "SU-76", "tank", tag, fine, paints={"canvas.seat": "4A4638", "canvas.tarp": "5A5A40", "cloth.sack": "6A6448", "torn.mud": "4C402C", "torn.mud2": "3E3D29"})
    v.root(hull_pivot=(0.0, 0.0, 0.70))
    hw, zg, zr, xr, th = 0.98, 1.00, 1.26, -2.45, 0.028     # half-width between the tracks; the guards; the roof ahead of the box; her tail
    gz = lambda x: 0.74 + (2.44 - x) * (zr - 0.74) / (2.44 - 0.95)       # noqa: E731  (the glacis: one plate from her nose to the roof)
    xg = 2.44 - (zg - 0.74) * (2.44 - 0.95) / (zr - 0.74)                 # (where it crosses the guards' height)
    # ---- the hull: a tub between the tracks, the engines and the driver under a roof ahead of the box ---------
    v.hull([(x, hw, z) for x, z in [(xr + 0.09, 0.40), (xr, 0.66), (xr, zg), (xg, zg), (2.44, 0.74), (2.50, 0.58), (2.20, 0.36), (xr + 0.25, 0.36)]], coat, bevel=0.014)
    v.hull([(xg, 1.30, zg), (0.95, 1.30, zr), (0.42, 1.30, zr), (0.42, 1.30, zg)], coat, bevel=0.014)
    gl = Vector((0.95 - 2.44, 0, zr - 0.74)).normalized()               # up the glacis
    gn = Vector((gl.z, 0, -gl.x))
    for s in (LEFT, RIGHT):
        v.slab((xr - 0.05, s * 0.985, zg - 0.015), (2.30, s * 1.36, zg), coat, 0.004)                  # the guards over the tracks
        v.plate([(2.30, s * 0.985, zg), (2.30, s * 1.36, zg), (2.62, s * 1.36, zg - 0.2), (2.62, s * 0.985, zg - 0.2)][::int(s)], 0.01, "upper", 0.003)
        v.plate([(xr - 0.05, s * 1.36, zg), (xr - 0.05, s * 0.985, zg), (xr - 0.2, s * 0.985, zg - 0.2), (xr - 0.2, s * 1.36, zg - 0.2)][::int(s)], 0.01, "upper", 0.003)
        v.hook((2.47, s * 0.6, 0.52), Vector((0.8, 0, -0.4)).normalized(), 0.08)
        v.hook((xr, s * 0.6, 0.6), -X, 0.08)
    # the driver's hood standing out of the glacis, his flap in its face; a lamp, a horn, the access plates
    v.hull([(1.86, 0.27, gz(1.86) - 0.01), (1.80, 0.25, 1.30), (1.22, 0.25, 1.32), (1.22, 0.27, gz(1.22) - 0.01)], coat, bevel=0.02)
    hf = Vector((1.80 - 1.86, 0, 1.30 - gz(1.86))).normalized()
    hn = Vector((hf.z, 0, -hf.x))
    v.hatch_rect("hatch_driver", "hull", Vector((1.86, 0.0, gz(1.86))) + hf * 0.17 + hn * 0.002, (0.26, 0.40), hn, hf, hinge="front", opens=90.0, thick=0.022)
    v.visor(Vector((1.86, 0.0, gz(1.86))) + hf * 0.17 + hn * 0.026, hn, 0.2, 0.05, 0.016, part="hatch_driver", brow=False)
    v.use("hull")
    v.periscope((1.5, 0.0, 1.315), X, (0.07, 0.14, 0.07))
    v.headlamp(Vector((1.9, 0.72, gz(1.9))) + gn * 0.07, X, 0.07, stalk=0.07)
    for y, w in ((0.72, 0.5), (-0.72, 0.5)):               # the plates over the gearbox and the engines' front
        c = Vector((1.35, y, gz(1.35)))
        v.box((0.5, w, 0.02), c + gn * 0.01, "upper", 0.005, turn=_turn(gl, gn))
        if fine:
            for du in (-0.21, 0.21):
                for dy in (-w / 2 + 0.04, w / 2 - 0.04):
                    v.bolt(c + gl * du + Y * dy + gn * 0.02, gn, 0.011)
    nf = Vector((0.9, 0, 0.35)).normalized()
    v.spare_links(Vector((2.46, -0.5, 0.67)) + nf * 0.002, Vector((2.46, 0.5, 0.67)) + nf * 0.002, 9, 0.11, 0.28, nf, style={"kind": "plate", "grouser": 0.012})
    if fine:
        v.weld((xg - 0.005, -0.94, zg + 0.003), (xg - 0.005, 0.94, zg + 0.003), gn)
        v.shovel(Vector((1.75, -0.36, gz(1.75))) + gn * 0.025, 0.0, 0.8, tilt=math.degrees(math.atan2(gl.z, -gl.x)) * -1.0)
    # the engines' air on the right: a grille in the roof, louvres in her side, the exhaust stub; a bin on the left
    v.slab((0.48, -1.22, zr), (0.92, -0.40, zr + 0.016), "upper", 0.006)
    v.grille((0.70, -0.81, zr + 0.016), (0.36, 0.74), Z, Y, slats=7, deep=0.02)
    v.louvres((0.80, -1.301, 1.13), (0.46, 0.16), -Y, 4)
    v.tube([(0.52, -1.30, 1.12), (0.50, -1.36, 1.12), (0.40, -1.39, 1.10)], 0.035, "rust", v.n(10, 5))
    v.socket("exhaust.1", (0.38, -1.40, 1.10), (-0.6, -1, 0.0))
    v.bin((0.48, 0.45, zr), (0.92, 1.22, zr + 0.17), bevel=0.015, lid_side=(1, 0, 0), latches=2)

    # ---- the box: thin plate leaning in, open above, lower behind; the gun's opening cut down into its face ------
    zt0, zt1, zb, xs0, xs1, xk = 2.10, 1.86, 1.62, 0.42, 0.10, -1.20
    gy = 0.20                                               # the gun stands this far left of her middle
    y_at = lambda z: 1.34 - (z - zg) * 0.14 / (zt0 - zg)   # noqa: E731
    sn = [Vector((0, s * (zt0 - zg), 0.14)).normalized() for s in (1.0, -1.0)]
    side = [(xr + 0.02, zg), (xs0, zg), (xs0, zr), (xs1, zt0), (xk, zt0), (xr + 0.02, zt1)]
    top_at = lambda x: zt0 if x >= xk else zt1 + (x - xr - 0.02) * (zt0 - zt1) / (xk - xr - 0.02)      # noqa: E731  (her top edge over a place along her)
    sf_ = Vector((zt0 - zr, 0, xs0 - xs1)).normalized()                                                   # (out of the box's face)
    for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
        wall(v, [(x, s * y_at(z), z) for x, z in side][::int(s)], -n, th, coat, 0.004)
        if fine:
            v.weld((-0.9, s * (y_at(zg + 0.03) + 0.001), zg + 0.03), (-0.9, s * (y_at(zt0 - 0.03) + 0.001), zt0 - 0.03), n)
            # the plate's own work: bolts along her top edge, the angle inside it, stiffeners up the inside, the seam at her face
            edge = [(xs1 - 0.06, zt0), (xk, zt0), (xr + 0.1, top_at(xr + 0.1))]
            for (xa, za), (xb_, zb_) in zip(edge, edge[1:]):
                v.edge_bolts((xa, s * (y_at(za - 0.045) + 0.0005), za - 0.045), (xb_, s * (y_at(zb_ - 0.045) + 0.0005), zb_ - 0.045), n, gap=0.15, r=0.01)
                v.rib((xa, s * (y_at(za - 0.045) - th), za - 0.045), (xb_, s * (y_at(zb_ - 0.045) - th), zb_ - 0.045), -n, deep=0.04, wide=0.011)
            for x in (-1.85, -1.0, -0.15):
                z = top_at(x) - 0.05
                v.rib((x, s * (y_at(zg + 0.01) - th), zg + 0.01), (x, s * (y_at(z) - th), z), -n, deep=0.03, wide=0.026)
            v.weld((xs0, s * (y_at(zr) + 0.001), zr), (xs1, s * (y_at(zt0) + 0.001), zt0 - 0.01), (n + sf_).normalized())
            v.weld((xr + 0.02, s * (y_at(zg) - 0.004), zg + 0.02), (xr + 0.02, s * (y_at(zb) - 0.004), zb - 0.02), (n - X).normalized())
            for k in range(10):
                v.bolt((xr + 0.25 + 0.28 * k, s * (y_at(zg + 0.05) + 0.001), zg + 0.05), n, 0.011, 0.007)
            v.handle(Vector((-1.9, s * y_at(1.7), 1.7)), Vector((-1.5, s * y_at(1.7), 1.7)), n, 0.009, 0.04)
            v.port(Vector((-0.5, s * (y_at(1.72) + 0.001), 1.72)), n, 0.04)
        v.mark(tag, (-1.2, s * (y_at(1.55) + 0.002), 1.55), n, 0.18, part="hull")
    sl = Vector((xs1 - xs0, 0, zt0 - zr))
    Hs = sl.length
    sl = sl.normalized()
    sf = Vector((sl.z, 0, -sl.x))
    slot, bn = 0.36, (1.44 - zr) / sl.z
    sheet(v, [(-y_at(zr), 0.0), (y_at(zr), 0.0), (y_at(zt0), Hs), (gy + slot, Hs), (gy + slot, bn), (gy - slot, bn), (gy - slot, Hs), (-y_at(zt0), Hs)], [], (xs0, 0.0, zr), Y, sl, th, coat, 0.004)
    if fine:
        s_at = lambda u, y: Vector((xs0, y, zr)) + sl * u + sf * 0.0005      # noqa: E731  (a place on her face, u metres up it)
        v.edge_bolts(s_at(Hs - 0.045, gy + slot + 0.07), s_at(Hs - 0.045, y_at(zt0) - 0.09), sf, gap=0.15, r=0.01)
        v.edge_bolts(s_at(Hs - 0.045, gy - slot - 0.07), s_at(Hs - 0.045, -y_at(zt0) + 0.09), sf, gap=0.15, r=0.01)
        for e in (-1, 1):
            v.box((Hs - bn, 0.045, 0.01), s_at((Hs + bn) / 2, gy + e * (slot + 0.028)) + sf * 0.005, coat, 0.003, turn=_turn(sl, sf))      # a strip up each side of the opening
        v.weld(s_at(0.01, -y_at(zr) + 0.05), s_at(0.01, y_at(zr) - 0.05), sf)
    wall(v, [(xs0, -1.28, zg), (xs0, 1.28, zg), (xs0, 1.28, zr), (xs0, -1.28, zr)], -X, th, coat, 0.004)       # the bulkhead before the engines
    # the tail plate: lower than the sides, a door in its middle
    wall(v, [(xr + 0.02, -y_at(zg), zg), (xr + 0.02, y_at(zg), zg), (xr + 0.02, y_at(zb), zb), (xr + 0.02, -y_at(zb), zb)], X, th, coat, 0.004)
    v.slab((xr + 0.008, -0.3, zg + 0.06), (xr + 0.02, 0.3, zb - 0.04), "upper", 0.004)
    if fine:
        for z in (zg + 0.18, zb - 0.16):
            v.hinge((xr + 0.004, 0.31, z - 0.04), (xr + 0.004, 0.31, z + 0.04), 0.012)
        v.handle((xr + 0.008, -0.2, 1.3), (xr + 0.008, -0.2, 1.42), -X, 0.008, 0.035, "steel")
        for s in (LEFT, RIGHT):
            v.edge_bolts((xr + 0.0195, s * 0.4, zb - 0.045), (xr + 0.0195, s * (y_at(zb) - 0.1), zb - 0.045), -X, gap=0.15, r=0.01)
            v.rib((xr + 0.02 + th, s * 0.34, zg + 0.01), (xr + 0.02 + th, s * 0.34, zb - 0.02), X, deep=0.03, wide=0.026)      # the door's posts, inside
    for s in (LEFT, RIGHT):                                # a bedroll strapped each side of the door
        v.roll((xr - 0.085, s * 0.42, zg + 0.36), (xr - 0.085, s * 1.12, zg + 0.36), 0.095, paint="canvas.tarp", straps=2)
    v.slab((xr + 0.04, -1.3, zg), (xs0 - 0.02, 1.3, zg + 0.008), "shade")                              # her floor, trodden dark

    # ---- the piece: the divisional gun's own saddle and cradle, a plate box over its recoil gear --------------
    px, pz, zt = 0.20, 1.41, 1.72
    here = v.here()
    v.pedestal((px, 0.0, pz), zg, r=0.28)
    v.saddle(px, zt, 0.16, (15.0, 17.0), rate=5.0, foot=0.26)
    v.cradle(-5.0, 35.0, 0.22, 0.14, 1.15, 0.62, 0.1, over=(-0.55, 0.95, 0.052, 0.155))
    v.use("gun")                                           # (the box lifts and trains with the gun)
    v.hull([(px + 0.30, e * 0.30, zt + d) for e in (1, -1) for d in (-0.30, 0.27)] + [(px + 0.98, e * 0.17, zt + d) for e in (1, -1) for d in (-0.22, 0.2)], coat, bevel=0.012, both=False)
    if fine:
        for e in (1, -1):
            for d in (-0.2, 0.0, 0.2):
                v.bolt((px + 0.64, e * 0.238, zt + d * 0.9), Vector((0.19, e, 0.0)), 0.012, 0.008)
    xb = v.piece(3.40, 0.0762, 1.02, 0.088, 0.052, 0.9, brake="double", steps=((0.34, 0.07),), ring=(0.26, 0.25, 0.27), block="down", lug=0.6)
    v.handwheels((px - 0.37, 0.33, zt - 0.095), (px - 0.44, 0.27, zt - 0.205), r=0.1, grips="gunner")      # (where the seated layer's hands reach them: the soldiers' fit check)
    v.sight((px - 0.22, 0.25, zt + 0.025), high=0.2)
    v.shift(here, (0.0, gy, 0.0))
    # ---- inside her: rounds along the right wall and at the back on the left, the layer's seat, the wireless, boxes
    #      of rounds and the detachment's sacks along the left wall -------------------------------------------------
    v.rounds(-2.3, -0.6, -1.22, -0.96, zg, 0.44, r=0.045, gap=0.105, tray=0.72)
    v.rounds(-2.3, -1.3, 0.96, 1.22, zg, 0.44, r=0.045, gap=0.105, tray=0.72)
    # her floor between the racks and the breech: the path the loader has trodden, the empty cases he threw down
    v.trodden(((-1.35, -0.6, 0.36, 0.22, 60.0), (-1.6, 0.6, 0.36, 0.2, -45.0), (-1.15, -0.1, 0.42, 0.24, 30.0),
               (-1.25, 0.25, 0.38, 0.22, -20.0), (-1.85, -0.2, 0.3, 0.2, 10.0)), zg + 0.008)
    for x, y, yaw in ((-1.75, 0.25, 20.0), (-1.0, 0.38, -50.0), (-2.0, 0.02, 80.0)):
        v.spent((x, y, zg + 0.008), yaw, 0.045, 0.385)
    v.use("hull")
    v.rod((-0.40, 0.76, zg), (-0.40, 0.76, zg + 0.34), 0.02, "steel", 8)                              # the layer's seat, the wheels by his right knee
    v.box((0.3, 0.3, 0.055), (-0.40, 0.76, zg + 0.37), "canvas.seat", 0.02)
    v.box((0.34, 0.3, 0.05), (-1.25, -0.80, zg + 0.34), "canvas.seat", 0.02)                           # a seat folded down from the right rack
    v.slab((-1.4, -0.96, zg + 0.3), (-1.1, -0.66, zg + 0.318), coat, 0.004)
    v.crate((-0.92, 1.04, zg), (0.62, 0.3, 0.2), 0.0)                                                  # boxes of rounds, a helmet left on them
    v.crate((-0.90, 1.04, zg + 0.2), (0.62, 0.3, 0.2), 3.0)
    v.sack((-2.2, 0.68, zg), 0.15, 0.36, "cloth.sack")
    v.sack((-1.9, 0.76, zg), 0.13, 0.32, "cloth.sack")
    v.rammer((-1.5, y_at(1.9) - th - 0.034, 1.9), (0.02, y_at(1.9) - th - 0.034, 1.9), head=0.04)      # the cleaning staff along the left wall
    if fine:
        v.box((0.36, 0.14, 0.24), (-0.2, y_at(1.7) - 0.11, 1.7), "shade", 0.008)
        v.bucket((-2.28, 0.3, zg), 0.11, 0.22)
        v.helmet((-0.90, 1.04, zg + 0.405))
        v.helmet((-0.66, -(y_at(1.8) - th - 0.07), 1.8), (0.0, 1.0, 0.25))
        v.helmet((-0.36, -(y_at(1.8) - th - 0.07), 1.8), (0.0, 1.0, 0.25))
    v.aerial("aerial", "hull", (-0.2, y_at(zt0) - th / 2, zt0), 2.0)
    # ---- the running gear: six small wheels a side on torsion bars, three rollers, the drive at the front ------
    ty, r_w = 1.18, 0.275
    xs = [-1.70 + 0.66 * i for i in range(6)]
    v.running(xs, r_w, ty, (2.15, 0.68, 0.29), (-2.20, 0.54, 0.275), rollers=[(-1.05, 0.86, 0.11), (-0.05, 0.86, 0.11), (0.95, 0.86, 0.11)],
              wheel=dict(w=0.13, tyre=0.035, double=False, kind="dish", bolts=6, sides=30, hub=0.36, axle=0.1, holes=6),
              sprocket_kw=dict(teeth=13, wide=0.22, tooth=0.03, holes=0, bolts=8, sides=28, hub=0.42),
              idler_kw=dict(wide=0.14, kind="dish", holes=6),
              roller_kw=dict(wide=0.12, tyre=0.012, reach=0.14),
              track=dict(pitch=0.11, wide=0.30, thick=0.026, sag=0.012, style={"kind": "plate", "horn": "centre", "horn_high": 0.06, "grouser": 0.012, "cleats": False}))
    v.use("vehicle")
    if fine:
        for s in (LEFT, RIGHT):
            for x in xs:                                    # each wheel's arm, trailing from its torsion bar in the tub's side
                v.rod((x + 0.24, s * (hw + 0.01), 0.44), (x + 0.24, s * (ty - 0.1), 0.44), 0.04, "shade", 10)
                v.rod((x + 0.24, s * (ty - 0.1), 0.44), (x, s * (ty - 0.1), r_w), 0.032, "shade", 8)
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.lathe((2.15, s * (hw - 0.02), 0.68), Y * s, [(0.16, 0.0), (0.16, 0.14), (0.1, 0.2), (0.0, 0.22), (0.0, 0.0)], "upper", v.n(22, 8))
    # ---- the detachment's places, and what happens to her ---------------------------------------------------
    v.socket("commander", (-2.05, -0.5, zg), X, part="hull", note="the gun's commander stands here at the back of the open box, on the right")
    v.socket("crew.gunner", (-0.40, 0.76, zg + 0.3975), X, part="hull",
             note="the layer, seated left of the breech: the top of his seat, 0.39 over the floor's top; the handwheels are by his right knee, their handles 0.16 and 0.33 ahead, 0.19 to his right, 0.14 and 0.23 over the seat (his hands: grip.gunner.l, grip.gunner.r)")
    v.socket("crew.loader", (xb - 0.58, gy - 0.5, zg), X, part="hull", note="the loader stands here, right of the breech and clear of its run back")
    v.socket("case", (xb - 0.5, gy - 0.2, zg), -X, part="hull", note="where the empty case falls, on her floor")
    v.socket("engine", (0.70, -0.81, zr + 0.05), Z, part="hull", note="the engines' grille on the right: smoke and the first fire")
    v.socket("fire.1", (0.70, -0.4, zr + 0.05), Z, part="hull")
    v.socket("fire.2", (-1.2, 0.0, 1.8), Z, part="hull", note="out of her open box")
    v.socket("fire.3", (1.5, 0.0, 1.34), Z, part="hull", note="out of the driver's hood")
    v.socket("ring", (-1.2, 0.0, zg), Z, part="hull", note="the middle of her floor: where the blast comes from when her ammunition goes")
    v.socket("hit.front", Vector((1.6, 0.5, gz(1.6))), gn, part="hull")
    v.socket("hit.left", (-0.8, y_at(1.6), 1.6), sn[0], part="hull")
    v.socket("hit.right", (-0.8, -y_at(1.6), 1.6), sn[1], part="hull")
    v.socket("hit.rear", (xr, 0.0, 1.3), -X, part="hull")
    v.socket("hit.turret", (0.25, -0.6, 1.7), sf, part="hull", note="the box's face")
    v.tank_clips(recoil=0.8, kick=0.9, gun_low=-5.0, hatches=["hatch_driver"], traverse=5.0, idle_cue="tank_idle_loop", drive_cue="tank_loop",
                 gun_cue="field_gun_fire", has_turret=False, mg=None)
    v.name_place((-0.3, 1.6), long=1.0, high=0.12, lines=2)           # the slogan (b477): on the box's side
    return v


def sp_gun(fine=True, tag="_"):
    """The self-propelled gun: the medium tank's hull, the driver under a low roof in her nose and behind him an
    open-topped box of thin plate with a 75 mm field gun standing in it, firing ahead over the nose through a notch
    in the box's face, a small shield on its cradle before the notch. For every army with no self-propelled gun of
    its own. 6.0 m long, 2.75 m wide, 2.2 m high over her sight."""
    v = SpWorks("SelfPropelledGun", "Self-propelled gun", "tank", tag, fine,
                paints={"canvas.seat": "4F5038", "canvas.tarp": "5E5E44", "cloth.sack": "6A6448", "torn.mud": "4C402C", "torn.mud2": "3E3D29"})
    h = medium_hull(v, fine, box=False)
    sw, zg, gl, gn, g_at, d_at, dn = h["sw"], h["zg"], h["gl"], h["gn"], h["g_at"], h["d_at"], h["dn"]
    zn, zw, zb, th = 1.44, 2.08, 1.74, 0.028               # the nose's roof; the top of the box's sides and of its back; its plate
    xe, xn, xf, xk = -0.85, 1.42, 1.30, -0.45              # the engine's bulkhead; the foot and the head of the box's face; where the sides begin to fall to the back
    k = 0.12                                               # the sides lean in this much for each metre they rise
    y_at = lambda z: sw - (z - zg) * k                     # noqa: E731
    sn = [Vector((0, s, k)).normalized() for s in (1.0, -1.0)]
    top_at = lambda x: zw if x >= xk else zb + (x - xe) * (zw - zb) / (xk - xe)      # noqa: E731  (her top edge over a place along her)
    xg = 2.60 + gl.x / gl.z * (zn - zg)                    # the head of the glacis
    zf = zg + 0.008                                        # her floor

    # ---- the nose: the driver under a low roof, his lid in it ------------------------------------------------------
    v.use("hull")
    v.hull([(2.60, sw, zg), (xg, y_at(zn), zn), (xn, y_at(zn), zn), (xn, sw, zg)], coat, bevel=0.014)
    v.hatch_rect("hatch_driver", "hull", (1.75, 0.52, zn), (0.42, 0.50), Z, X, hinge="front", opens=105.0)
    v.use("hull")
    v.periscope((1.80, -0.52, zn), X, (0.07, 0.14, 0.07))
    v.vent((1.62, -0.84, zn), 0.09, 0.045)
    if fine:
        v.weld((xg, -y_at(zn) + 0.04, zn), (xg, y_at(zn) - 0.04, zn), (gn + Z).normalized())
        for s in (LEFT, RIGHT):
            v.lift_eye((1.95, s * 0.92, zn), Z, X, 0.03)
    v.slab((xn - 0.006, 0.22, zg + 0.05), (xn, 0.74, zn - 0.07), "shade", 0.004)                        # the way through to the driver's seat, low on the left

    # ---- the box: thin plate leaning in, open above, lower at the back; the gun's notch cut down into its face ------
    side = [(xe, zg), (xn, zg), (xn, zn), (xf, zw), (xk, zw), (xe, zb)]
    sl = Vector((xf - xn, 0, zw - zn))
    Hs = sl.length
    sl = sl.normalized()                                   # up the box's face
    sf = Vector((sl.z, 0, -sl.x))                          # out of it
    for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
        wall(v, [(x, s * y_at(z), z) for x, z in side][::int(s)], -n, th, coat, 0.004)
        if fine:
            v.weld((0.35, s * (y_at(zg + 0.03) + 0.001), zg + 0.03), (0.35, s * (y_at(zw - 0.03) + 0.001), zw - 0.03), n)
            # the plate's own work: bolts along her top edge, the angle inside it, stiffeners up the inside, the seams at her face and her back
            edge = [(xf - 0.06, zw), (xk, zw), (xe + 0.08, top_at(xe + 0.08))]
            for (xa, za), (xb_, zb_) in zip(edge, edge[1:]):
                v.edge_bolts((xa, s * (y_at(za - 0.045) + 0.0005), za - 0.045), (xb_, s * (y_at(zb_ - 0.045) + 0.0005), zb_ - 0.045), n, gap=0.15, r=0.01)
                v.rib((xa, s * (y_at(za - 0.045) - th), za - 0.045), (xb_, s * (y_at(zb_ - 0.045) - th), zb_ - 0.045), -n, deep=0.04, wide=0.011)
            for x in (-0.25, 0.35, 0.95):
                z = top_at(x) - 0.05
                v.rib((x, s * (y_at(zg + 0.01) - th), zg + 0.01), (x, s * (y_at(z) - th), z), -n, deep=0.03, wide=0.026)
            v.weld((xn, s * (y_at(zn) + 0.001), zn), (xf, s * (y_at(zw) + 0.001), zw - 0.01), (n + sf).normalized())
            v.weld((xe + 0.002, s * (y_at(zg) - 0.004), zg + 0.02), (xe + 0.002, s * (y_at(zb) - 0.004), zb - 0.02), (n - X).normalized())
            for i in range(8):
                v.bolt((xe + 0.2 + 0.29 * i, s * (y_at(zg + 0.05) + 0.001), zg + 0.05), n, 0.011, 0.007)
            v.handle(Vector((-0.70, s * y_at(1.56), 1.56)), Vector((-0.34, s * y_at(1.56), 1.56)), n, 0.011, 0.05)
            v.port(Vector((0.95, s * (y_at(1.74) + 0.001), 1.74)), n, 0.04)
        v.mark(tag, (0.15, s * (y_at(1.54) + 0.002), 1.54), n, 0.2, part="hull")
    px, zt, slot = 1.26, 1.80, 0.30                        # the pintle and the trunnions; half the notch
    bn = (zt - 0.31 - zn) / sl.z
    sheet(v, [(-y_at(zn), 0.0), (y_at(zn), 0.0), (y_at(zw), Hs), (slot, Hs), (slot, bn), (-slot, bn), (-slot, Hs), (-y_at(zw), Hs)], [], (xn, 0.0, zn), Y, sl, th, coat, 0.004)
    if fine:
        s_at = lambda u, y: Vector((xn, y, zn)) + sl * u + sf * 0.0005       # noqa: E731  (a place on her face, u metres up it)
        v.edge_bolts(s_at(Hs - 0.045, slot + 0.07), s_at(Hs - 0.045, y_at(zw) - 0.09), sf, gap=0.15, r=0.01)
        v.edge_bolts(s_at(Hs - 0.045, -slot - 0.07), s_at(Hs - 0.045, -y_at(zw) + 0.09), sf, gap=0.15, r=0.01)
        for e in (-1, 1):
            v.box((Hs - bn, 0.045, 0.01), s_at((Hs + bn) / 2, e * (slot + 0.028)) + sf * 0.005, coat, 0.003, turn=_turn(sl, sf))      # a strip up each side of the notch
        v.weld(s_at(0.01, -y_at(zn) + 0.05), s_at(0.01, y_at(zn) - 0.05), sf)
    # the back plate, over the engine's deck
    wall(v, [(xe + 0.002, -y_at(zg), zg), (xe + 0.002, y_at(zg), zg), (xe + 0.002, y_at(zb), zb), (xe + 0.002, -y_at(zb), zb)], X, th, coat, 0.004)
    if fine:
        v.edge_bolts((xe + 0.0015, -y_at(zb) + 0.1, zb - 0.045), (xe + 0.0015, y_at(zb) - 0.1, zb - 0.045), -X, gap=0.15, r=0.01)
        v.rib((xe + 0.002 + th, -y_at(zb) + th, zb - 0.045), (xe + 0.002 + th, y_at(zb) - th, zb - 0.045), X, deep=0.04, wide=0.011)
    v.roll((xe - 0.092, -0.62, 1.60), (xe - 0.092, 0.62, 1.60), r=0.09, paint=v.own_paint("canvas.drab", "5C5B44"))       # her sheet, strapped across the back plate (dark enough to sit in her coat: a tan roll is the lightest thing on her)
    v.slab((xe + 0.03, -(sw - 0.03), zg), (xn - 0.002, sw - 0.03, zf), "shade")                          # her floor, trodden dark

    # ---- the piece: the 75 mm field gun's own saddle, cradle and barrel, a small shield on the cradle ------------
    v.pedestal((px, 0.0, zt - 0.31), zg, r=0.28)
    v.saddle(px, zt, 0.16, (12.0, 12.0), rate=5.0, foot=0.26)
    v.cradle(-5.0, 28.0, 0.2, 0.17, 1.25, 0.88, 0.085, round_under=True)
    v.use("gun")                                           # (the shield lifts and trains with the gun, and covers the notch)
    out = [(-0.38, -0.19), (-0.30, -0.27), (0.30, -0.27), (0.38, -0.19), (0.38, 0.20), (0.27, 0.31), (-0.27, 0.31), (-0.38, 0.20)]
    sheet(v, out, [], (px + 0.26, 0.0, zt), Y, Z, 0.014, coat, 0.003)
    if fine:
        edge_rivets(v, out, (px + 0.26, 0.0, zt), Y, Z)
    xb = v.piece(2.72, 0.075, 0.98, 0.082, 0.058, 0.75, steps=((0.36, 0.068),), ring=(0.24, 0.23, 0.23), block="screw", round_ring=True, lug=0.9)
    v.handwheels((px - 0.37, 0.33, zt - 0.095), (px - 0.44, 0.27, zt - 0.205), r=0.1, grips="gunner")      # (where the seated layer's hands reach them: the soldiers' fit check)
    v.sight((px - 0.22, 0.25, zt + 0.025), high=0.2)

    # ---- inside her: rounds along the right wall and at the back on the left, the layer's seat, the wireless, a box
    #      of fuzes, the detachment's sacks in the corner under the face ---------------------------------------------
    v.rounds(-0.78, 0.60, -1.04, -0.80, zg, 0.44, r=0.045, gap=0.105, tray=0.72)
    v.rounds(-0.78, -0.25, 0.80, 1.04, zg, 0.44, r=0.045, gap=0.105, tray=0.72)
    v.trodden(((-0.35, -0.15, 0.36, 0.22, 60.0), (-0.5, 0.3, 0.34, 0.2, -45.0), (0.15, -0.45, 0.4, 0.22, 30.0), (0.25, 0.25, 0.36, 0.2, -20.0), (0.75, -0.6, 0.3, 0.2, 10.0)), zf)
    for x, y, yaw in ((0.05, 0.25, 20.0), (-0.55, 0.0, 80.0), (0.35, -0.42, -50.0)):                  # the empty cases the loader threw down, clear of the men's feet
        v.spent((x, y, zf), yaw, 0.045, 0.385)
    v.use("hull")
    gs = (px - 0.60, 0.56)
    v.rod((gs[0], gs[1], zg), (gs[0], gs[1], zg + 0.40), 0.02, "steel", 8)                              # the layer's seat, the wheels by his right knee
    v.box((0.3, 0.3, 0.055), (gs[0], gs[1], zg + 0.43), "canvas.seat", 0.02)
    v.crate((0.18, 0.92, zg), (0.62, 0.3, 0.2), 0.0)                                                    # a box of fuzes against the left wall, a helmet left on it
    v.sack((1.18, -0.78, zg), 0.15, 0.36, "cloth.sack")
    v.sack((0.90, -0.88, zg), 0.13, 0.32, "cloth.sack")
    v.jerrycan((1.22, 0.98, zg), 0.0)                                                                  # (lengthways against the wall: across her it stood on the layer's left toe)
    v.rammer((-0.40, y_at(1.92) - th - 0.034, 1.92), (1.15, y_at(1.92) - th - 0.034, 1.92), head=0.04)  # the cleaning staff along the left wall
    for y in (-0.36, 0.36):                                # the detachment's packs on the back plate
        v.pack((xe + 0.002 + th, y, 1.40), X, paint="canvas.tarp")
    if fine:
        v.box((0.36, 0.14, 0.24), (0.80, -(y_at(1.72) - th - 0.075), 1.72), "shade", 0.008)             # the wireless, on the right wall
        v.bucket((0.78, 0.94, zg), 0.11, 0.22)                                                          # (against the wall, clear of the layer's boots)
        v.helmet((0.18, 0.92, zg + 0.205))
        v.helmet((-0.10, y_at(1.78) - th - 0.07, 1.78), (0.0, -1.0, 0.25))                              # helmets hung on the walls (clear of the plate: a brim must never come through it)
        v.helmet((0.22, -(y_at(1.78) - th - 0.07), 1.78), (0.0, 1.0, 0.25))                             # (one a side: two side by side over the roll read as a face from behind)
    v.aerial("aerial", "hull", (0.80, -(y_at(zw) - th / 2), zw), 2.0)

    # ---- the detachment's places, and what happens to her -----------------------------------------------------------
    v.socket("commander", (-0.45, 0.50, zf), X, part="hull", note="the gun's commander stands here at the back of the open box, on the left: its sides come to his chest (1.06 over the floor)")
    v.socket("crew.gunner", (gs[0], gs[1], zg + 0.4575), X, part="hull",
             note="the layer, seated left of the breech: the top of his seat, 0.45 over the floor's top; the handwheels are by his right knee, their handles 0.16 and 0.33 ahead, 0.19 to his right, 0.14 and 0.23 over the seat (his hands: grip.gunner.l, grip.gunner.r)")
    v.socket("crew.loader", (xb - 0.58, -0.5, zf), X, part="hull", note="the loader stands here, right of the breech and clear of its run back")
    v.socket("case", (xb - 0.5, -0.2, zf), -X, part="hull", note="where the empty case falls, on her floor")
    v.socket("engine", d_at(0.6, 0.0) + dn * 0.05, Z, part="hull", note="engine deck: smoke and the first fire")
    v.socket("fire.1", d_at(0.5, 0.5) + dn * 0.05, Z, part="hull")
    v.socket("fire.2", (0.2, 0.0, 1.8), Z, part="hull", note="out of her open box")
    v.socket("fire.3", (1.75, 0.52, zn + 0.05), Z, part="hull", note="out of the driver's hatch")
    v.socket("ring", (0.2, 0.0, zg), Z, part="hull", note="the middle of her floor: where the blast comes from when her ammunition goes")
    v.socket("hit.front", g_at(0.4, -0.3), gn, part="hull")
    v.socket("hit.left", (0.3, y_at(1.6), 1.6), sn[0], part="hull")
    v.socket("hit.right", (0.3, -y_at(1.6), 1.6), sn[1], part="hull")
    v.socket("hit.rear", (-2.8, 0.0, 0.9), -X, part="hull")
    v.socket("hit.turret", Vector((xn, -0.65, zn)) + sl * (Hs * 0.55), sf, part="hull", note="the box's face")
    v.tank_clips(recoil=0.65, kick=0.8, gun_low=-5.0, hatches=["hatch_driver"], traverse=5.0, idle_cue="tank_idle_loop", drive_cue="tank_loop",
                 gun_cue="field_gun_fire", has_turret=False, mg=None)
    # (the second colour is sprayed on her from outside: the inside of the fighting box and what stands in it stay plain)
    v.camouflage(plain=lambda p: p.z > zg - 0.02 and xe + 0.02 < p.x < (xn if p.z < zn else xn + (xf - xn) * (p.z - zn) / (zw - zn)) - 0.02 and abs(p.y) < y_at(p.z) - 0.02)
    return v


def _turn(along, up):
    """The turn that lays a box's length along `along` and its top toward `up`."""
    from mathutils import Matrix
    a, u = Vector(along).normalized(), Vector(up).normalized()
    return Matrix((a, u.cross(a), u)).transposed()
