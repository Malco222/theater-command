"""Self-propelled artillery: a field gun's own saddle, cradle and piece standing in the open top of a tank's hull, so
the guns keep up with the armour. The M7 (the United States' 105 mm howitzer on the M4's hull, a round machine-gun
pulpit at her right front corner) and the Sexton (Britain's and the Commonwealth's 25-pounder in the same hull, a
plain tall box with no pulpit).

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
from motorpool import LEFT, RIGHT, X, Y, Z
from tanks_ger import coat
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


def _turn(along, up):
    """The turn that lays a box's length along `along` and its top toward `up`."""
    from mathutils import Matrix
    a, u = Vector(along).normalized(), Vector(up).normalized()
    return Matrix((a, u.cross(a), u)).transposed()
