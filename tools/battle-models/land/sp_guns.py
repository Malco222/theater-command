"""Self-propelled artillery: a field gun's own saddle, cradle and piece standing in the open top of a tank's hull, so
the guns keep up with the armour. The M7 (the United States' 105 mm howitzer on the M4's hull, a round machine-gun
pulpit at her right front corner), the Sexton (Britain's and the Commonwealth's 25-pounder in the same hull, a
plain tall box with no pulpit), the Hummel (Germany's 15 cm heavy field howitzer on a lengthened Panzer IV hull)
and the SU-76 (the Soviet Union's 76 mm divisional gun on a lengthened light tank's hull).

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
from guns import GunWorks, sheet
from motorpool import LEFT, RIGHT, X, Y, Z, bm_box, rot
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

    def rounds(self, x0, x1, y0, y1, z0, high, r=0.055, gap=0.135, paint="brass"):
        """Ready rounds standing nose down in a rack: a tray of plate, and the brass bases of the cases over it."""
        self.use("hull")
        self.slab((x0, min(y0, y1), z0), (x1, max(y0, y1), z0 + high * 0.55), coat, 0.006)
        nx, ny = max(1, int((x1 - x0 - 0.03) / gap)), max(1, int((abs(y1 - y0) - 0.03) / gap))
        n = self.n(12, 6)
        for i in range(nx):
            for j in range(ny):
                x = x0 + (x1 - x0) * (i + 0.5) / nx
                y = y0 + (y1 - y0) * (j + 0.5) / ny
                self.lathe((x, y, z0 + high * 0.55), Z, [(high * 0.45, 0.0), (high * 0.45, r * 0.86), (high * 0.45 - 0.012, r), (0.0, r), (0.0, 0.0)], paint, n)
                if self.fine:
                    self.lathe((x, y, z0 + high), Z, [(0.002, 0.0), (0.002, r * 0.3)], "steel", 6)       # the primer


def m4_open_hull(v, zw, zb, xf, notch, pulpit=None, driver=LEFT, step=None, tag="USA"):
    """The M4's hull with an open box for a gun where her roof and turret were. zw: the top of the walls; zb: the top
    of the back wall, over the engine deck; xf: where the head of the sloped front plate stands; notch: (right edge,
    left edge, foot) of the opening the piece looks through; pulpit: (x, y, radius, top) of a round machine-gun
    position standing in her right front corner; driver: the side his flap is on; step: (x, z) the side walls step
    down to z behind x. Returns the handy numbers."""
    fine = v.fine
    hw, sw, zs, zr, xe = 0.80, 1.33, 1.06, 1.74, -1.02     # between the tracks, over them; the sponson's floor; the engine deck; its bulkhead
    th = 0.022
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
        if fine:
            lamp = lamp_at + gn * 0.14
            for e in (-0.075, 0.075):                    # the guard bent over each lamp
                v.tube([lamp + Y * e - Z * 0.11 + X * 0.02, lamp + Y * e + Z * 0.075 + X * 0.06, lamp + Y * e + Z * 0.075 - X * 0.08, lamp + Y * e - Z * 0.02 - X * 0.2], 0.007, "upper", 5)
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
    if zback > zr + 0.01:
        wall(v, [(xe, -sw, zr - 0.02), (xe, sw, zr - 0.02), (xe, sw, zback), (xe, -sw, zback)], X, th, coat, 0.004)
    if fine:
        v.tube([(xf - 0.004, sw - th / 2, zw + 0.002)] + [(x, sw - th / 2, z + 0.002) for x, z in top[1:]] + [(xe, -sw + th / 2, zback + 0.002)]
               + ([] if pulpit else [(x, -sw + th / 2, z + 0.002) for x, z in reversed(top[1:-1])] + [(xf - 0.004, -sw + th / 2, zw + 0.002)]), 0.013, coat, 6)       # the bead along the walls' top edge

    # ---- the pulpit: a drum of plate in her right front corner, taller than the walls, a ring round its top -----
    mg = None
    if pulpit:
        cx, cy, r, zp = pulpit
        n = v.n(40, 14)
        v.lathe((cx, cy, zs), Z, [(zp - zs, r - th), (zp - zs, r), (0.0, r), (0.0, r - th), (zp - zs, r - th)], coat, n)
        v.lathe((cx, cy, zs + 0.26), Z, [(0.012, 0.0), (0.012, r - th)], "shade", n)                      # the floor he stands on
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
        v.socket("crew.mg", (cx - 0.05, cy, zs + 0.27), X, part="hull", note="the machine-gunner stands here in the pulpit")
        mg = "mg"

    # ---- the engine deck and the tail, as the tank's -------------------------------------------------------------
    v.use("hull")
    m4_deck(v, zr, caps=False)
    m4_tail(v)
    for s in (LEFT, RIGHT):
        v.mark(tag, (-0.1, s * (sw + 0.002), (zs + zback) / 2 + 0.04), Y * s, 0.2, part="hull")
    m4_gear(v, hw)
    v.use("hull")
    return {"hw": hw, "sw": sw, "zs": zs, "zr": zr, "xe": xe, "floor": 1.076, "g_at": g_at, "gn": gn, "gl": gl, "Hc": Hc, "th": th, "zback": zback, "mg": mg, "driver_y": dy}


def _open_sockets(v, h, zw, gunner, loaders, commander, case):
    """The detachment's places in her open box, and what happens to her."""
    zs, zr, sw, xe, fl = h["zs"], h["zr"], h["sw"], h["xe"], h["floor"]
    g_at, gn = h["g_at"], h["gn"]
    v.socket("commander", commander, X, part="hull", note="the gun's commander stands here in the open box")
    v.socket("crew.gunner", gunner, X, part="turret", note="the layer, at the sight and the handwheels on the left of the breech")
    for k, at in enumerate(loaders):
        v.socket("crew.loader" if k == 0 else f"crew.loader{k + 1}", at, X, part="hull", note="a loader stands here, behind the breech")
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
    v = SpWorks("M7", "M7 105 mm howitzer motor carriage", "tank", tag, fine, paints={"canvas.seat": "5A5B40"})
    v.root(hull_pivot=(0.0, 0.0, 0.85))
    zw = 2.04
    h = m4_open_hull(v, zw, 1.90, 1.72, (-0.66, 0.46, 1.36), pulpit=(1.18, -0.90, 0.50, 2.48), driver=LEFT, step=(0.0, 1.90), tag=tag)
    sw, zs, zr, xe, fl = h["sw"], h["zs"], h["zr"], h["xe"], h["floor"]
    px, pz, zt = 1.37, 1.30, 1.81
    v.pedestal((px, 0.0, pz), fl, r=0.36)
    v.saddle(px, zt, 0.22, (15.0, 30.0), rate=6.0, foot=0.34)
    v.cradle(-5.0, 35.0, 0.32, 0.2, 1.2, 0.55, 0.16, over=(-0.45, 1.25, 0.07, 0.235))
    xb = v.piece(2.36, 0.105, 0.78, 0.14, 0.1, 1.07, steps=((0.45, 0.118),), ring=(0.3, 0.36, 0.36), block="right", lug=0.5)
    v.handwheels((px - 0.3, 0.42, zt - 0.14), (px - 0.42, 0.34, zt - 0.30), r=0.12)
    v.sight((px - 0.2, 0.32, zt + 0.04), high=0.26)
    v.seat((px - 0.62, 0.50, zt - 0.62), 1.0, part="turret")
    # ---- inside her: the rounds standing along each sponson, seats, the things a detachment lives with -----------
    v.rounds(-0.92, 0.36, 0.88, sw - 0.05, zs, 0.50)
    v.rounds(-0.92, 0.22, -(sw - 0.05), -0.88, zs, 0.50)
    v.use("hull")
    for at in ((-0.55, 0.52), (-0.55, -0.52)):            # two seats on posts
        v.rod((at[0], at[1], fl), (at[0], at[1], fl + 0.34), 0.022, "steel", 8)
        v.box((0.32, 0.32, 0.06), (at[0], at[1], fl + 0.37), "canvas.seat", 0.02)
    if fine:
        v.rod((0.62, sw - 0.09, 1.72), (0.62, sw - 0.09, 1.98), 0.05, "red", 12)                       # the fire extinguisher on the wall
        v.box((0.34, 0.14, 0.24), (0.95, sw - 0.11, 1.80), "shade", 0.008)                             # the wireless
        v.bucket((-0.86, 0.0, fl), 0.12, 0.24)
    # ---- outside her: bins on the engine deck, a rolled tarpaulin, the tools across her tail ---------------------
    for s in (LEFT, RIGHT):
        v.bin((-1.58, s * 0.74, zr), (-1.06, s * 1.29, zr + 0.25), bevel=0.02, lid_side=(0, s, 0), latches=2)
    v.roll((1.5, sw + 0.12, 1.42), (0.3, sw + 0.12, 1.42), 0.11, straps=3)
    if fine:
        v.cable([(1.9, -sw - 0.025, 1.22), (0.4, -sw - 0.025, 1.16), (-0.9, -sw - 0.025, 1.24)], 0.014)
        rb = Vector((-2.58 + 2.76, 0, 1.66 - 1.18)).normalized()
        rn = Vector((-rb.z, 0, rb.x))
        r_at = lambda u, y: Vector((-2.76, y, 1.18)) + rb * u + rn * 0.002      # noqa: E731
        v.shovel(r_at(0.30, -0.5) + rn * 0.02, 90.0, 0.95)
        v.axe(r_at(0.50, -0.38) + rn * 0.02, 90.0, 0.78)
        for s in (LEFT, RIGHT):
            v.jerrycan((-0.5 + 0.0, s * (sw + 0.09), 1.12), 90.0 if s > 0 else -90.0)
    v.aerial("aerial", "hull", (0.95, sw - 0.04, zw), 2.4)
    _open_sockets(v, h, zw, gunner=(px - 0.55, 0.50, fl), loaders=[(xb - 0.9, -0.25, fl), (xb - 0.7, 0.45, fl)], commander=(-0.75, -0.3, fl), case=(xb - 0.5, -0.2, fl))
    v.tank_clips(recoil=1.0, kick=0.7, gun_low=-5.0, hatches=["hatch_driver"], traverse=6.0, idle_cue="tank_idle_sherman_loop", drive_cue="tank_sherman_drive_loop",
                 gun_cue="field_gun_fire", has_turret=False, mg="mg")
    return v


def sexton(fine=True, tag="UK"):
    """Sexton of 1943: the 25-pounder on the same hull (built in Canada on the Ram's), in a plain tall open box with
    no pulpit, the driver on the right. 6.12 m long, 2.72 m wide, 2.44 m high."""
    v = SpWorks("Sexton", "Sexton, 25-pounder self-propelled", "tank", tag, fine, paints={"canvas.seat": "5A5B40"})
    v.root(hull_pivot=(0.0, 0.0, 0.85))
    zw = 2.16
    h = m4_open_hull(v, zw, zw, 1.88, (-0.46, 0.62, 1.40), pulpit=None, driver=RIGHT, step=None, tag=tag)
    sw, zs, zr, xe, fl = h["sw"], h["zs"], h["zr"], h["xe"], h["floor"]
    px, pz, zt = 1.25, 1.40, 1.90
    v.pedestal((px, 0.0, pz), fl, r=0.34)
    v.saddle(px - 0.05, zt, 0.2, (25.0, 15.0), rate=4.0, foot=0.3)
    v.cradle(-9.0, 40.0, 0.27, 0.19, 1.45, 0.45, 0.13)
    xb = v.piece(2.47, 0.0876, 0.8, 0.118, 0.07, 0.5, brake="double", steps=((0.5, 0.095),), ring=(0.27, 0.3, 0.31), block="down", lug=0.5)
    v.handwheels((px - 0.37, 0.38, zt - 0.12), (px - 0.49, 0.31, zt - 0.26), r=0.11)
    v.sight((px - 0.29, 0.3, zt + 0.02), high=0.22)
    v.seat((px - 0.67, 0.44, zt - 0.56), 1.0, part="turret")
    # ---- inside her: the rounds along each sponson, seats, the wireless ------------------------------------------
    v.rounds(-0.92, 0.50, 0.90, sw - 0.05, zs, 0.44, r=0.046, gap=0.115)
    v.rounds(-0.92, 0.30, -(sw - 0.05), -0.90, zs, 0.44, r=0.046, gap=0.115)
    v.use("hull")
    for at in ((-0.55, 0.52), (-0.55, -0.52)):
        v.rod((at[0], at[1], fl), (at[0], at[1], fl + 0.34), 0.022, "steel", 8)
        v.box((0.32, 0.32, 0.06), (at[0], at[1], fl + 0.37), "canvas.seat", 0.02)
    if fine:
        v.rod((0.9, sw - 0.09, 1.80), (0.9, sw - 0.09, 2.06), 0.05, "red", 12)
        v.box((0.40, 0.16, 0.26), (-0.75, -(sw - 0.12), 1.86), "shade", 0.008)                          # the wireless, at the back on the right
        v.bucket((-0.86, 0.1, fl), 0.12, 0.24)
    # ---- outside her: a bin each side of the engine deck, bedding rolled along her side, the tools on her tail ----
    for s in (LEFT, RIGHT):
        v.bin((-2.30, s * 0.80, 1.71), (-1.66, s * 1.29, 1.93), bevel=0.02, lid_side=(0, s, 0), latches=2)
    v.roll((0.9, -(sw + 0.12), 1.50), (-0.5, -(sw + 0.12), 1.50), 0.11, straps=3)
    if fine:
        v.cable([(1.9, sw + 0.025, 1.22), (0.4, sw + 0.025, 1.16), (-0.9, sw + 0.025, 1.24)], 0.014)
        rb = Vector((-2.58 + 2.76, 0, 1.66 - 1.18)).normalized()
        rn = Vector((-rb.z, 0, rb.x))
        r_at = lambda u, y: Vector((-2.76, y, 1.18)) + rb * u + rn * 0.002      # noqa: E731
        v.shovel(r_at(0.30, -0.5) + rn * 0.02, 90.0, 0.95)
        v.axe(r_at(0.50, -0.38) + rn * 0.02, 90.0, 0.78)
    v.aerial("aerial", "hull", (-0.9, -(sw - 0.04), zw), 2.4)
    _open_sockets(v, h, zw, gunner=(px - 0.6, 0.48, fl), loaders=[(xb - 0.9, -0.25, fl), (xb - 0.7, 0.45, fl)], commander=(-0.75, 0.3, fl), case=(xb - 0.5, -0.2, fl))
    v.tank_clips(recoil=0.5, kick=0.6, gun_low=-9.0, hatches=["hatch_driver"], traverse=4.0, idle_cue="tank_idle_sherman_loop", drive_cue="tank_sherman_drive_loop",
                 gun_cue="field_gun_fire", has_turret=False, mg=None)
    return v


def hummel(fine=True, tag="GER"):
    """Hummel of 1943: the 15 cm heavy field howitzer on a lengthened Panzer IV hull, her engine moved to the middle
    under the gun, a tall open box of thin plate over her back half, the drivers' cab across her front. 7.17 m over
    the gun, 2.97 m wide, 2.81 m high. She came after the grey: dark yellow from her first day."""
    v = SpWorks("Hummel", "Hummel, 15 cm self-propelled howitzer", "tank", tag, fine, paint=GER_COATS["yellow"][0], paints={"canvas.seat": "4A4638"})
    v.root(hull_pivot=(0.0, 0.0, 0.75))
    m = v.m
    hw, zg, xr, th = 0.925, 1.10, -3.04, 0.02            # half-width between the tracks; the guards; her tail; the box's plate
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
    for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
        wall(v, [(x, s * y_at(z), z) for x, z in side][::int(s)], -n, th, coat, 0.004)
        if fine:
            v.weld((-1.30, s * (y_at(zg + 0.03) + 0.001), zg + 0.03), (-1.30, s * (y_at(zt0 - 0.03) + 0.001), zt0 - 0.03), n)
            for k in range(12):                              # the bolts along her foot, into the guard
                v.bolt((xr + 0.3 + 0.28 * k, s * (y_at(zg + 0.05) + 0.001), zg + 0.05), n, 0.012, 0.008)
            for x in (-2.5, -0.9, -0.3):                    # loops to lash her stowage to
                v.handle(Vector((x - 0.04, s * y_at(2.4), 2.4)), Vector((x + 0.04, s * y_at(2.4), 2.4)), n, 0.006, 0.024, "steel")
        v.mark("GER", (-2.1, s * (y_at(1.86) + 0.002), 1.86), n, 0.2, part="hull")
    sl = Vector((xs1 - xs0, 0, zt0 - ze))
    Hs = sl.length
    sl = sl.normalized()
    sf = Vector((sl.z, 0, -sl.x))
    slot, bn = 0.52, (1.80 - ze) / sl.z
    sheet(v, [(-y_at(ze), 0.0), (y_at(ze), 0.0), (y_at(zt0), Hs), (slot, Hs), (slot, bn), (-slot, bn), (-slot, Hs), (-y_at(zt0), Hs)], [], (xs0, 0.0, ze), Y, sl, th, coat, 0.004)
    # the tail plate: two doors, their hinges and a handle
    yb0, yb1 = y_at(zg), y_at(zt1)
    wall(v, [(xr + 0.02, -yb0, zg), (xr + 0.02, yb0, zg), (xr + 0.02, yb1, zt1), (xr + 0.02, -yb1, zt1)], X, th, coat, 0.004)
    for s in (LEFT, RIGHT):
        v.slab((xr + 0.008, s * 0.012, zg + 0.08), (xr + 0.02, s * 0.62, zt1 - 0.1), "upper", 0.004)
        if fine:
            for z in (zg + 0.3, zt1 - 0.32):
                v.hinge((xr + 0.004, s * 0.63, z - 0.05), (xr + 0.004, s * 0.63, z + 0.05), 0.014)
            v.handle((xr + 0.008, s * 0.12, 1.72), (xr + 0.008, s * 0.12, 1.86), -X, 0.008, 0.04, "steel")
    if fine:                                               # the bead along the box's top edge
        top = [(xs1, y_at(zt0) - th / 2, zt0), (xk, y_at(zt0) - th / 2, zt0), (xr + 0.03, y_at(zt1) - th / 2, zt1)]
        v.tube([Vector(p) + Z * 0.002 for p in top] + [Vector((p[0], -p[1], p[2] + 0.002)) for p in reversed(top)], 0.012, coat, 6)
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
        sheet(v, gs, [], (px + 0.56, 0.0, zt), Y, Z, 0.012, coat, 0.003)
        if fine:
            for s in (LEFT, RIGHT):
                v.rod((px + 0.55, s * 0.30, zt - 0.3), (px + 0.2, s * 0.2, zt - 0.3), 0.012, coat, 6)
    xb = v.piece(4.44, 0.149, 0.62, 0.19, 0.112, 1.1, steps=((0.35, 0.152),), ring=(0.4, 0.46, 0.46), block="right", lug=0.6)
    v.handwheels((px - 0.34, 0.48, zt - 0.14), (px - 0.46, 0.40, zt - 0.30), r=0.13)
    v.sight((px - 0.22, 0.37, zt + 0.04), high=0.3)
    if fine:                                               # the two spring equilibrators, standing either side of the cradle
        v.use("turret")
        for s in (LEFT, RIGHT):
            v.rod((px + 0.26, s * 0.36, pz + 0.12), (px + 0.18, s * 0.36, zt - 0.16), 0.06, coat, 14)
            v.rod((px + 0.18, s * 0.36, zt - 0.16), (px + 0.15, s * 0.32, zt - 0.05), 0.022, "steel", 8)
    # ---- inside her: the cartridge cases standing along each side at the back, seats, the wireless -------------
    v.rounds(-2.9, -1.75, 0.82, 1.24, zg, 0.5, r=0.075, gap=0.19)
    v.rounds(-2.9, -1.75, -1.24, -0.82, zg, 0.5, r=0.075, gap=0.19)
    v.use("hull")
    for at in ((-1.2, 0.9), (-1.2, -0.9)):
        v.rod((at[0], at[1], zg), (at[0], at[1], zg + 0.36), 0.022, "steel", 8)
        v.box((0.32, 0.32, 0.06), (at[0], at[1], zg + 0.39), "canvas.seat", 0.02)
    if fine:
        v.box((0.42, 0.16, 0.28), (-0.5, -(y_at(2.0) - 0.12), 2.0), "shade", 0.008)                    # the wireless on the right wall
        v.rod((-0.3, y_at(2.2) - 0.08, 2.1), (-0.3, y_at(2.2) - 0.08, 2.36), 0.05, "red", 12)
        v.bucket((-2.6, 0.0, zg), 0.12, 0.24)
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
        v.cable([(0.0, -(y_at(1.9) + 0.02), 1.9), (-0.7, -(y_at(1.72) + 0.02), 1.72), (-1.4, -(y_at(1.9) + 0.02), 1.9)], 0.013)
    v.silencer((xr - 0.18, -0.62, 0.93), (xr - 0.18, 0.46, 0.93), 0.15, pipe=[(xr, 0.2, 1.02), (xr - 0.12, 0.2, 1.0), (xr - 0.18, 0.2, 0.96)], tail=((xr - 0.18, -0.62, 0.93), (-0.3, -1, -0.2)))
    v.socket("exhaust.1", (xr - 0.24, -0.78, 0.9), (-0.3, -1, -0.2))
    v.aerial("aerial", "hull", (-0.5, y_at(zt0) - 0.01, zt0), 2.0)
    pz4_gear(v, xs=[c + d for c in (-1.7605, -0.6705, 0.4195, 1.5095) for d in (-0.2515, 0.2515)], idler=(-2.82, 0.64),
             rollers=[(-1.62, 0.905), (-0.56, 0.905), (0.50, 0.905), (1.56, 0.905)], dust_x=-3.05)
    # ---- the detachment's places, and what happens to her ---------------------------------------------------
    v.socket("commander", (-1.9, -0.45, zg), X, part="hull", note="the gun's commander stands here in the open box")
    v.socket("crew.gunner", (px - 0.6, 0.55, zg), X, part="hull", note="the layer, at the sight and the handwheels on the left of the breech")
    v.socket("crew.loader", (xb - 1.0, -0.3, zg), X, part="hull", note="a loader stands here, behind the breech")
    v.socket("crew.loader2", (xb - 0.8, 0.5, zg), X, part="hull", note="the second loader, with the cartridge")
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
    v = SpWorks("SU76", "SU-76", "tank", tag, fine, paints={"canvas.seat": "4A4638"})
    v.root(hull_pivot=(0.0, 0.0, 0.70))
    hw, zg, zr, xr, th = 0.98, 1.00, 1.26, -2.45, 0.018     # half-width between the tracks; the guards; the roof ahead of the box; her tail
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
    for s, n in ((LEFT, sn[0]), (RIGHT, sn[1])):
        wall(v, [(x, s * y_at(z), z) for x, z in side][::int(s)], -n, th, coat, 0.004)
        if fine:
            v.weld((-0.9, s * (y_at(zg + 0.03) + 0.001), zg + 0.03), (-0.9, s * (y_at(zt0 - 0.03) + 0.001), zt0 - 0.03), n)
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
    wall(v, [(xs0, -1.28, zg), (xs0, 1.28, zg), (xs0, 1.28, zr), (xs0, -1.28, zr)], -X, th, coat, 0.004)       # the bulkhead before the engines
    # the tail plate: lower than the sides, a door in its middle
    wall(v, [(xr + 0.02, -y_at(zg), zg), (xr + 0.02, y_at(zg), zg), (xr + 0.02, y_at(zb), zb), (xr + 0.02, -y_at(zb), zb)], X, th, coat, 0.004)
    v.slab((xr + 0.008, -0.3, zg + 0.06), (xr + 0.02, 0.3, zb - 0.04), "upper", 0.004)
    if fine:
        for z in (zg + 0.18, zb - 0.16):
            v.hinge((xr + 0.004, 0.31, z - 0.04), (xr + 0.004, 0.31, z + 0.04), 0.012)
        v.handle((xr + 0.008, -0.2, 1.3), (xr + 0.008, -0.2, 1.42), -X, 0.008, 0.035, "steel")
        top = [(xs1, y_at(zt0) - th / 2, zt0), (xk, y_at(zt0) - th / 2, zt0), (xr + 0.03, y_at(zt1) - th / 2, zt1)]
        for s in (LEFT, RIGHT):                            # the bead along each side's top edge
            v.tube([Vector((p[0], s * p[1], p[2] + 0.002)) for p in top], 0.011, coat, 6)
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
    v.handwheels((px - 0.34, 0.33, zt - 0.095), (px - 0.44, 0.27, zt - 0.255), r=0.1)
    v.sight((px - 0.22, 0.25, zt + 0.025), high=0.2)
    v.shift(here, (0.0, gy, 0.0))
    # ---- inside her: rounds along the right wall and across the back, seats, the wireless ----------------------
    v.rounds(-2.3, -0.6, -1.22, -0.96, zg, 0.44, r=0.045, gap=0.105)
    v.rounds(-2.3, -1.3, 0.96, 1.22, zg, 0.44, r=0.045, gap=0.105)
    v.use("hull")
    for at in ((-0.5, 0.75), (-1.2, -0.45)):
        v.rod((at[0], at[1], zg), (at[0], at[1], zg + 0.34), 0.02, "steel", 8)
        v.box((0.3, 0.3, 0.055), (at[0], at[1], zg + 0.37), "canvas.seat", 0.02)
    if fine:
        v.box((0.36, 0.14, 0.24), (-0.2, y_at(1.7) - 0.11, 1.7), "shade", 0.008)
        v.bucket((-2.2, 0.3, zg), 0.11, 0.22)
    v.aerial("aerial", "hull", (-0.2, y_at(zt0) - 0.01, zt0), 2.0)
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
    v.socket("commander", (-1.5, -0.5, zg), X, part="hull", note="the gun's commander stands here in the open box, on the right")
    v.socket("crew.gunner", (px - 0.55, gy + 0.5, zg), X, part="hull", note="the layer, at the sight and the handwheels on the left of the breech")
    v.socket("crew.loader", (xb - 0.9, gy - 0.3, zg), X, part="hull", note="the loader stands here, behind the breech")
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
    return v


def _turn(along, up):
    """The turn that lays a box's length along `along` and its top toward `up`."""
    from mathutils import Matrix
    a, u = Vector(along).normalized(), Vector(up).normalized()
    return Matrix((a, u.cross(a), u)).transposed()
