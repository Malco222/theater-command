"""Light cars: the small open cars every army ran its officers, scouts and messages in, and the machine guns they
carried on posts (the motorcycles' sidecars carry the same guns, motorcycles.py).

    Kubelwagen    the German light car (Volkswagen Type 82, 1940-45): 3.74 m long, 1.60 m wide, a flat-sided steel tub
                  stiffened by pressed ribs, four doors, a sloping front lid with the spare wheel lying on it, the
                  lamps on the wings, the air-cooled engine under louvres in her tail.
    Jeep          the American quarter-ton 4x4 (Willys MB and Ford GPW, 1941-45): 3.36 m long, 1.58 m wide, flat
                  slab sides with an open cut for the men to step in, flat-topped front wings, a grille of nine
                  slots with the lamps on its face, the windscreen folded flat on the bonnet as she went into action,
                  a .30 Browning on a post between the front seats, the spare wheel and a can on her back.

PARTS   vehicle (root) > hull (the body on its springs) > screen (the windscreen on its hinges) | mg_mount > mg_cradle
        (the machine gun on its post: the game trains and lays it)
        vehicle > steer_l > wheel_front_l, steer_r > wheel_front_r (the game steers and rolls them), wheel_back_l/r
CLIPS   a lorry's (idle, start, drive, move_off, stop, hit, knocked_out, burn) and mg_burst
SOCKETS driver, rider.N (a seated man, facing the socket's way: the top of his seat), gunner (his feet);
        grip.driver.l/r, grip.gunner.l/r (their hands), mg (the muzzle); hook (a towed gun's or trailer's eye goes on it);
        engine, exhaust.1, fire.N, hit.*, dust.*
Every shape is our own mesh, made from numbers here.
"""

import math

from mathutils import Vector

from armoured_cars import CarWorks
from carriers import chaikin
from motorpool import LEFT, RIGHT, SIDES, X, Y, Z
from tanks_ger import coat, coats

# What a light car or a motorcycle wears in the field (the E7 surface pass: shade in her creases, dirt by her ground).
LIGHT_WORN = dict(shade=True, shade_deep=0.42, shade_knee=0.5, shade_reach=0.25, layers=2, marks=0.45, gear=0.6, rim=True, foot=0.35, top=0.70)      # (a tank's dirt climbs to 0.95 m and thins out by 1.45: a car is not that tall)


class LightWorks(CarWorks):
    # ---- the machine gun on a post: one helper for every army's gun ------------------------------------------------
    def post_gun(self, kind, foot, high, arc=(-170.0, 170.0), lay=(-12.0, 45.0), parent="hull", post=True, grips_back=None):
        """A machine gun on a post (a car's pedestal, a sidecar's pintle). Two parts the game aims: `mg_mount` swings it
        round (+ to the right, clockwise seen from above) and `mg_cradle` tips it (+ muzzle up). Sockets on the cradle:
        `mg` at the muzzle, `grip.gunner.l` and `grip.gunner.r` where the gunner's hands go. `foot`: where the post stands;
        `high`: the trunnions over the foot. kind: "m1919" (America's .30 Browning, the box of 250 at its left), "mg34"
        (Germany's MG 34 with its 50-round drum on the left), "dt" (the Soviet tank and vehicle gun, a pan on top),
        "jap" (Japan's light gun, a curved box on top). Returns the muzzle."""
        f = Vector(foot)
        t = f + Z * high
        fine = self.fine
        self.use(parent)
        if post:
            if fine:
                self.lathe(f, Z, [(0.035, 0.0), (0.035, 0.07), (0.012, 0.075), (0.0, 0.075), (0.0, 0.0)], "upper", self.n(16, 8))
            self.rod(f, f + Z * (high - 0.07), 0.034, "upper", 10, r2=0.026)
        self.part("mg_mount", parent, t, axis=(0, 0, -1), limits=list(arc), kind="turn",
                  note="the machine gun on its post: + swings it to the right; the game aims it while a man has hold of it")
        self.rod(t - Z * 0.09, t - Z * 0.02, 0.036, "steel", 10)                                    # the pintle's sleeve
        if fine:
            for s in (LEFT, RIGHT):
                self.slab((t.x - 0.04, t.y + s * 0.035, t.z - 0.05), (t.x + 0.04, t.y + s * 0.045, t.z + 0.02), "steel", 0.003)
        self.part("mg_cradle", "mg_mount", t, axis=(0, -1, 0), limits=list(lay), kind="turn", note="the gun tips on its trunnions: + muzzle up")
        steel, black = "steel", "black"
        if kind == "m1919":
            b = t.z + 0.05
            self.slab((t.x - 0.33, t.y - 0.032, b - 0.06), (t.x + 0.05, t.y + 0.032, b + 0.04), steel, 0.005 if fine else 0.0)       # the receiver
            self.slab((t.x - 0.30, t.y - 0.03, b + 0.04), (t.x - 0.04, t.y + 0.03, b + 0.052), steel, 0.003)                          # the cover
            self.lathe((t.x + 0.05, t.y, b), X, [(0.40, 0.0), (0.40, 0.023), (0.0, 0.025), (0.0, 0.0)], steel, self.n(14, 6))         # the cooling jacket
            if fine:
                for k in range(8):                                                           # its holes, in rows
                    for a in (0.6, 2.2, 3.8, 5.4):
                        w = Vector((0.0, math.cos(a + k * 0.4), math.sin(a + k * 0.4)))
                        self.lathe(Vector((t.x + 0.10 + k * 0.042, t.y, b)) + w * 0.0235, w, [(0.0, 0.0), (0.0, 0.007)], black, 5)
                self.lathe((t.x + 0.45, t.y, b), X, [(0.10, 0.0), (0.10, 0.012), (0.06, 0.02), (0.0, 0.02), (0.0, 0.0)], steel, 10)  # the muzzle booster
                self.slab((t.x - 0.36, t.y - 0.018, b - 0.13), (t.x - 0.31, t.y + 0.018, b - 0.05), "wood", 0.006)                 # the pistol grip
                self.slab((t.x - 0.20, t.y + 0.035, b - 0.16), (t.x + 0.03, t.y + 0.13, b - 0.02), "upper", 0.006)                 # the box of 250
                self.slab((t.x - 0.17, t.y + 0.032, b - 0.025), (t.x - 0.05, t.y + 0.036, b + 0.02), "brass")                      # the belt going in
            muzzle = Vector((t.x + 0.56, t.y, b))
            grips = [Vector((t.x - 0.34, t.y + s * 0.07, b - 0.07)) for s in (LEFT, RIGHT)]
        elif kind == "mg34":
            b = t.z + 0.06
            self.slab((t.x - 0.20, t.y - 0.025, b - 0.045), (t.x + 0.10, t.y + 0.025, b + 0.04), steel, 0.005 if fine else 0.0)     # the receiver
            self.lathe((t.x + 0.10, t.y, b), X, [(0.36, 0.0), (0.36, 0.021), (0.0, 0.024), (0.0, 0.0)], steel, self.n(14, 6))         # the jacket
            self.rod((t.x - 0.20, t.y, b - 0.01), (t.x - 0.55, t.y, b - 0.05), 0.03, black, 8, r2=0.045)                              # the stock
            if fine:
                for k in range(7):
                    for a in (0.9, 2.5, 4.1, 5.7):
                        w = Vector((0.0, math.cos(a), math.sin(a)))
                        self.lathe(Vector((t.x + 0.15 + k * 0.045, t.y, b)) + w * 0.0215, w, [(0.0, 0.0), (0.0, 0.007)], black, 5)
                self.lathe((t.x + 0.46, t.y, b), X, [(0.07, 0.0), (0.07, 0.015), (0.0, 0.017), (0.0, 0.0)], steel, 10)
                self.slab((t.x - 0.16, t.y - 0.015, b - 0.13), (t.x - 0.11, t.y + 0.015, b - 0.04), black, 0.006)                  # the grip
                self.lathe((t.x - 0.04, t.y + 0.11, b - 0.01), Y, [(0.0, 0.0), (0.0, 0.075), (0.07, 0.075), (0.07, 0.0)], steel, 14)  # the drum
            muzzle = Vector((t.x + 0.54, t.y, b))
            grips = [Vector((t.x - 0.14, t.y + 0.05, b - 0.08)), Vector((t.x - 0.14, t.y - 0.05, b - 0.08))]
        elif kind == "dt":
            b = t.z + 0.05
            self.slab((t.x - 0.25, t.y - 0.028, b - 0.05), (t.x + 0.08, t.y + 0.028, b + 0.03), steel, 0.005 if fine else 0.0)
            self.rod((t.x + 0.08, t.y, b), (t.x + 0.55, t.y, b), 0.016, steel, 10)                                                    # the barrel, bare
            self.lathe((t.x - 0.06, t.y, b + 0.03), Z, [(0.0, 0.0), (0.0, 0.13), (0.045, 0.13), (0.045, 0.0)], steel, self.n(18, 8))   # the pan
            self.rod((t.x - 0.25, t.y, b - 0.01), (t.x - 0.52, t.y, b - 0.04), 0.022, steel, 8, r2=0.03)                              # the sliding stock
            if fine:
                self.lathe((t.x + 0.53, t.y, b), X, [(0.06, 0.0), (0.06, 0.022), (0.0, 0.022), (0.0, 0.0)], steel, 10)                 # the flash cone
                self.slab((t.x - 0.22, t.y - 0.015, b - 0.13), (t.x - 0.17, t.y + 0.015, b - 0.05), "wood", 0.006)
            muzzle = Vector((t.x + 0.60, t.y, b))
            grips = [Vector((t.x - 0.20, t.y + 0.06, b - 0.08)), Vector((t.x - 0.20, t.y - 0.06, b - 0.08))]
        else:   # "jap"
            b = t.z + 0.05
            self.slab((t.x - 0.22, t.y - 0.028, b - 0.05), (t.x + 0.08, t.y + 0.028, b + 0.03), steel, 0.005 if fine else 0.0)
            self.rod((t.x + 0.08, t.y, b), (t.x + 0.62, t.y, b), 0.015, steel, 10)
            self.lathe((t.x + 0.12, t.y, b), X, [(0.2, 0.0), (0.2, 0.026), (0.0, 0.026), (0.0, 0.0)], steel, self.n(14, 6))             # the finned barrel's base
            self.extrude([(t.x - 0.10, b + 0.03), (t.x + 0.02, b + 0.03), (t.x + 0.04, b + 0.20), (t.x - 0.06, b + 0.22)], t.y - 0.018, t.y + 0.018, steel, 0.003)  # the box on top
            self.rod((t.x - 0.22, t.y, b - 0.01), (t.x - 0.55, t.y, b - 0.06), 0.022, "wood", 8, r2=0.035)
            muzzle = Vector((t.x + 0.64, t.y, b))
            grips = [Vector((t.x - 0.20, t.y + 0.06, b - 0.08)), Vector((t.x - 0.20, t.y - 0.06, b - 0.08))]
        self.socket("mg", muzzle, X, part="mg_cradle", note="the machine gun's muzzle")
        for (letter, _), g in zip(SIDES, grips):
            self.socket(f"grip.gunner.{letter}", g, Z, part="mg_cradle", note=f"the gunner's {'left' if letter == 'l' else 'right'} hand on the gun")
        self.mg_kind = kind
        self.use(parent)
        return muzzle

    def gun_controls(self, how):
        """The file's word on the gun on a post, after the clips are made."""
        ctl = self.m.info.setdefault("file", {}).setdefault("controls", {})
        if "mg_mount" in self.m.part_at:
            ctl["mg"] = {"part": "mg_mount", "lay": "mg_cradle", "degrees_per_second": 60.0, "kind": self.mg_kind,
                         "what": "aimed by the game inside the two parts' limits while a man has hold of it (his hands on grip.gunner.l and "
                                 "grip.gunner.r): mg_mount + to the right, mg_cradle + muzzle up; with the file's `mg` swap off she has no gun: "
                                 "never aim it or play mg_burst. " + how}

    # ---- a steering wheel on its column ----------------------------------------------------------------------------
    def steering_wheel(self, foot, c, r=0.21):
        """The column up from `foot` to the wheel at `c`, square to the column; returns its two grips (left, right)."""
        foot, c = Vector(foot), Vector(c)
        d = (c - foot).normalized()
        u = Y.copy()
        w = d.cross(u).normalized()
        u = w.cross(d).normalized()
        self.rod(foot, c, 0.022, "black", 8)
        k = self.n(24, 10)
        self.tube([c + (u * math.cos(2 * math.pi * i / k) + w * math.sin(2 * math.pi * i / k)) * r for i in range(k)], 0.012, "black", 6, closed=True)
        self.lathe(c - d * 0.02, d, [(0.0, 0.0), (0.0, 0.045), (0.04, 0.035), (0.05, 0.0)], "black", 10)
        for a in (90.0, 210.0, 330.0):
            q = math.radians(a)
            self.rod(c + d * 0.02, c + (u * math.cos(q) + w * math.sin(q)) * (r - 0.01), 0.008, "black", 5)
        up_ = -w if (-w).z > 0 else w
        return [c + (u * math.cos(math.radians(a)) + up_ * math.sin(math.radians(a))) * r for a in (150.0, 30.0)]

    def seat(self, x0, x1, y0, y1, z, back=0.36, lean=0.06, thick=0.09, paint="canvas"):
        """A cushion from x0 (back) to x1 (front) at height z (its top), and its back rising `back` behind it."""
        self.slab((x0, y0, z - thick), (x1, y1, z), paint, 0.03 if self.fine else 0.0)
        b0, b1 = x0 - 0.02, x0 - 0.02 - 0.07
        self.hull([(b0, y0, z), (b1, y0, z), (b0 - lean, y0, z + back), (b1 - lean, y0, z + back),
                   (b0, y1, z), (b1, y1, z), (b0 - lean, y1, z + back), (b1 - lean, y1, z + back)], paint, 0.025, both=False)


# =====================================================================================================================
def jeep(fine=True, tag="USA", mg=True):
    """The quarter-ton truck (Willys MB, Ford GPW): 3.36 m long, 1.58 m wide over her wings, 1.32 m to the top of her
    steering wheel with the windscreen folded. Four men: the driver, one beside him, two on the seat behind (or the
    gunner standing at the .30 on its post between the front seats)."""
    v = LightWorks("Jeep", "Quarter-ton truck (jeep)", "car", tag, fine)
    v.dice = 0.2
    v.worn_kw = dict(LIGHT_WORN, ground="field")
    v.root(hull_pivot=(0.0, 0.0, 0.55), rock=(-5.0, 5.0))
    R, W, RR = 0.36, 0.16, 0.20
    xf, xr, yw = 1.016, -1.016, 0.6225
    yb, zt, zfl = 0.665, 1.0, 0.48                       # the body's sides, its top edge, its floor
    xg, xc = 1.46, 0.40                                  # the grille's face, the cowl's back (the dash)

    # ---- under her: two driven axles on leaf springs, the shafts, the wheels (on the root) ------------------------
    v.use("vehicle")
    for x in (xf, xr):
        v.rod((x, -yw + 0.1, R), (x, yw - 0.1, R), 0.035, "shade", 10)
        v.ball((x, -0.11, R), 0.11, "shade", 12, 8, squash=0.8)                                     # the differential, to her right
    v.rod((xf - 0.14, -yw + 0.12, R - 0.03), (xf - 0.14, yw - 0.12, R - 0.03), 0.011, "steel", 6)   # the track rod
    v.rod((xf - 0.08, -0.11, R), (-0.10, -0.11, 0.40), 0.022, "steel", 8)
    v.rod((xr + 0.08, -0.11, R), (-0.30, -0.11, 0.40), 0.022, "steel", 8)
    v.slab((-0.32, -0.20, 0.33), (-0.06, -0.02, 0.47), "shade", 0.012)                            # the transfer box
    for s in (LEFT, RIGHT):
        v.leaf_spring(xf, s * 0.37, 0.47, R + 0.05, 0.46, leaves=5, wide=0.045)
        v.leaf_spring(xr, s * 0.37, 0.47, R + 0.05, 0.53, leaves=6, wide=0.045)
    for letter, s in SIDES:
        v.steered_wheel(letter, s, xf, yw, R, W, RR, lock=30.0, bolts=5)
        v.lorry_wheel(f"wheel_back_{letter}", "vehicle", (xr, s * yw, R), s, R, W, RR, bolts=5)
        v.use("vehicle")
        v.socket("dust." + ("left" if s > 0 else "right"), (xr - R - 0.05, s * yw, 0.05), (-1, 0, 0.3), part="vehicle")

    # ---- the frame, the bumper ------------------------------------------------------------------------------------
    v.use("hull")
    for s in (LEFT, RIGHT):
        v.slab((-1.56, s * 0.37 - 0.03, 0.42), (1.58, s * 0.37 + 0.03, 0.53), "shade", 0.005)
    for x in (1.50, 0.45, -0.70, -1.50):
        v.slab((x - 0.03, -0.37, 0.43), (x + 0.03, 0.37, 0.52), "shade")
    v.slab((1.57, -0.78, 0.41), (1.645, 0.78, 0.54), coat, 0.012, 2)                                    # the bumper, a steel channel
    for s in (LEFT, RIGHT):
        v.lathe((1.66, s * 0.42, 0.475), Y, [(0.0, 0.0), (0.0, 0.035), (0.02, 0.035), (0.02, 0.0)], "steel", 8) if fine else None
        v.hook((1.65, s * 0.42, 0.445), X, 0.05)                                                          # the towing shackles

    # ---- the grille: a pressed panel of nine slots, the lamps on its face, the radiator dark behind --------------
    gy, gz0, gz1 = 0.37, 0.50, 0.975
    outline = [(gy, gz0), (gy, 0.90), (gy - 0.03, 0.95), (gy - 0.09, gz1), (-(gy - 0.09), gz1), (-(gy - 0.03), 0.95), (-gy, 0.90), (-gy, gz0)]
    v.hull([(xg - 0.025, y, z) for y, z in outline] + [(xg, y, z) for y, z in outline], coat, 0.006, both=False)
    v.slab((xg - 0.08, -0.22, 0.58), (xg - 0.05, 0.22, 0.92), "black")                                  # the radiator, seen through the slots
    for k in range(9):
        yc = -0.192 + k * 0.048
        v.slab((xg - 0.026, yc - 0.015, 0.585), (xg + 0.001, yc + 0.015, 0.905), "black")              # a slot
    for s in (LEFT, RIGHT):
        v.headlamp((xg + 0.045, s * 0.29, 0.80), X, 0.085, stalk=0.0)
        v.lathe((xg + 0.02, s * 0.29, 0.80), X, [(0.0, 0.0), (0.0, 0.095), (0.03, 0.095), (0.03, 0.0)], coat, v.n(16, 8)) if fine else None

    # ---- the bonnet, the wings, the cowl ---------------------------------------------------------------------------
    hood = [(xg - 0.02, 0.37, 0.88), (xg - 0.02, 0.355, 0.98), (xg - 0.02, 0.32, 0.993), (0.56, 0.37, 0.88), (0.56, 0.355, 1.012), (0.56, 0.32, 1.025)]
    v.hull(hood, coat, 0.01)
    if fine:
        for x in (1.22, 0.78):                                                                            # the bonnet's catches
            for s in (LEFT, RIGHT):
                v.slab((x - 0.03, s * 0.371 - 0.006, 0.90), (x + 0.03, s * 0.371 + 0.006, 0.96), "steel", 0.003)
        for s in (LEFT, RIGHT):                                                                           # the rests for the folded screen
            v.lathe((0.98, s * 0.27, 1.0), Z, [(0.0, 0.0), (0.0, 0.03), (0.03, 0.025), (0.03, 0.0)], "rubber", 8)
    wing = [(1.59, 0.63), (1.575, 0.76), (1.54, 0.845), (1.49, 0.875), (0.50, 0.875), (0.44, 0.855), (0.405, 0.80), (0.39, 0.56)]
    for s in (LEFT, RIGHT):
        v.guard(wing, s * 0.355, s * 0.79, thick=0.012, lip=0.03)
        v.slab((0.45, s * 0.355 - 0.006, 0.55), (xg - 0.03, s * 0.355 + 0.006, 0.875), "shade")            # the apron inside the wing
        v.headlamp((1.47, s * 0.74, 0.876), X, 0.024, stalk=0.03)                                         # the blackout marker lamps
    v.headlamp((1.22, 0.62, 0.876), X, 0.055, stalk=0.06, cover=True)                                     # the blackout driving lamp, on the left wing
    v.hull([(xc, yb, 0.86), (xc, yb, zt), (xc, yb - 0.03, zt + 0.03), (xc, 0.32, zt + 0.035),
            (0.565, yb, 0.86), (0.565, yb, zt - 0.01), (0.565, yb - 0.03, zt + 0.02), (0.565, 0.32, 1.027)], coat, 0.01)        # the cowl's top, full width
    for s in (LEFT, RIGHT):
        v.slab((xc, s * (yb - 0.02), 0.46), (0.565, s * yb, 0.87), coat, 0.004)                                # its sides, the footwells inside
    v.slab((0.565, -yb + 0.02, zfl), (0.585, yb - 0.02, 0.88), "upper")                                      # the firewall

    # ---- the body: flat sides with the open cut, the rolled top edge, the back, the floor ------------------------
    arch = [(xr + 0.43 * math.cos(math.radians(a)), R + 0.43 * math.sin(math.radians(a))) for a in range(160, 15, -15)]
    side = ([(xc, 0.46), (xc, zt), (0.30, zt), (0.25, 0.975), (0.21, 0.90), (0.15, 0.80), (0.05, 0.745), (-0.10, 0.725), (-0.30, 0.725), (-0.40, 0.745),
             (-0.47, 0.80), (-0.51, 0.90), (-0.54, 0.975), (-0.59, zt), (-1.53, zt), (-1.565, 0.99), (-1.58, 0.96), (-1.58, 0.47), (arch[0][0], 0.47)]
            + arch + [(arch[-1][0], 0.46)])
    top_edge = [Vector((x, 0.0, z)) for x, z in side[1:17]]
    for s in (LEFT, RIGHT):
        v.extrude(side, s * (yb - 0.02), s * yb, coat, 0.004)
        if fine:
            v.tube([p + Y * s * (yb - 0.008) + Z * 0.004 for p in top_edge], 0.013, coat, 6)               # the rolled top edge
    v.slab((-1.60, -yb, 0.47), (-1.58, yb, zt - 0.02), coat, 0.008)                                      # the back
    v.tube([(-1.592, -yb + 0.03, zt - 0.02), (-1.592, yb - 0.03, zt - 0.02)], 0.013, coat, 6) if fine else None
    v.slab((-1.58, -yb + 0.02, zfl - 0.02), (0.565, yb - 0.02, zfl), "upper")                              # the floor, on to the firewall
    v.slab((xc - 0.02, -yb + 0.02, 0.78), (xc, yb - 0.02, zt), "upper")                                   # the dash, over the footwells
    for s in (LEFT, RIGHT):                                                                                 # the housings over the back wheels
        hump = [(xr + 0.45 * math.cos(math.radians(a)), R + 0.45 * math.sin(math.radians(a))) for a in range(180, -1, -20)]
        v.extrude([(hump[0][0], zfl)] + hump + [(hump[-1][0], zfl)], s * 0.40, s * (yb - 0.02), "upper", 0.01)
    if fine:                                                                                                # the dials, the glovebox
        for k, y in enumerate((0.22, 0.10, -0.02, 0.36)):
            v.lathe((xc - 0.021, y, 0.86), -X, [(0.0, 0.0), (0.0, 0.032), (0.008, 0.032), (0.008, 0.0)], "black", 10)
        v.slab((xc - 0.025, -0.52, 0.74), (xc - 0.02, -0.22, 0.90), "upper", 0.004)

    # ---- the seats, the wheel, the levers -------------------------------------------------------------------------
    for s in (LEFT, RIGHT):
        v.slab((-0.40, s * 0.12, zfl), (-0.02, s * 0.54, 0.71), "upper", 0.01)                              # the seat boxes (her fuel under his)
        v.seat(-0.42, 0.0, *sorted((s * 0.11, s * 0.55)), 0.80, back=0.38, lean=0.07)
    v.seat(-1.46, -1.04, -0.56, 0.56, 0.86, back=0.32, lean=0.04)                                         # the seat behind, over the wheel housings
    grip_l, grip_r = v.steering_wheel((0.55, 0.33, 0.64), (0.08, 0.33, 1.10), r=0.215)
    if fine:
        for k, (y, lean) in enumerate(((0.16, 0.10), (0.06, 0.05), (0.01, 0.12))):                         # the gear lever and the two of the transfer box
            v.rod((0.18 - 0.02 * k, y, zfl), (0.10 - lean, y, 0.86 - 0.04 * k), 0.008, "black", 5)
            v.ball((0.10 - lean, y, 0.86 - 0.04 * k), 0.02, "black", 8, 6)

    # ---- the windscreen, folded flat on the bonnet as she went into action -----------------------------------------
    hz = zt + 0.04
    v.part("screen", "hull", (xc + 0.01, 0.0, hz), axis=(0, -1, 0), limits=[0.0, 92.0], kind="turn",
           note="the windscreen on its hinges at the cowl: built folded flat forward on the bonnet; + raises it (92 stands it up, leaning back a little)")
    fx0, fx1, fy = xc + 0.03, xc + 0.47, yb - 0.02
    fz = hz + 0.005
    for y0, y1 in ((-fy, fy),):
        v.rod((fx0, y0, fz), (fx0, y1, fz), 0.02, coat, 6)
        v.rod((fx1, y0, fz), (fx1, y1, fz), 0.017, coat, 6)
    for y in (-fy, 0.0, fy):
        v.rod((fx0, y, fz), (fx1, y, fz), 0.016 if y else 0.012, coat, 6)
    for s in (LEFT, RIGHT):
        a, b = sorted((s * 0.02, s * (fy - 0.02)))
        v.slab((fx0 + 0.02, a, fz - 0.003), (fx1 - 0.02, b, fz + 0.003), "glass")
        a, b = sorted((s * 0.012, s * (fy + 0.005)))
        v.hull([(fx0 - 0.005, a, fz + 0.005), (fx1 + 0.012, a, fz + 0.005), (fx0 - 0.005, b, fz + 0.005), (fx1 + 0.012, b, fz + 0.005),
                (fx0 + 0.03, a + 0.02 * s, fz + 0.03), (fx1 - 0.02, a + 0.02 * s, fz + 0.03), (fx0 + 0.03, b - 0.03 * s, fz + 0.026), (fx1 - 0.02, b - 0.03 * s, fz + 0.026)],
               "canvas", 0.01, both=False)                                                              # its canvas cover, against dust and the glint
    if fine:
        v.rod((fx1 + 0.01, 0.30, fz + 0.02), (fx1 - 0.12, 0.42, fz + 0.02), 0.005, "black", 4)              # a wiper
        v.rod((fx0 + 0.05, fy + 0.01, fz), (fx0 + 0.05, fy + 0.10, fz + 0.03), 0.006, "steel", 5)           # the mirror's arm
        v.lathe((fx0 + 0.05, fy + 0.10, fz + 0.03), Z, [(0.0, 0.0), (0.0, 0.05), (0.012, 0.05), (0.012, 0.0)], "steel", 10)
    v.use("hull")

    # ---- her back: the spare wheel, the can, the lamps, the hook, the folded top -----------------------------------
    xb = -1.60
    sc = Vector((xb - 0.115, -0.24, 0.74))
    v.slab((xb - 0.04, -0.30, 0.68), (xb, -0.18, 0.80), "shade", 0.006)                                   # the spare's carrier
    v.tyre(sc, -X, R, RR, W, lugs=18)
    v.wheel_disc(sc - X * (W * 0.1), -X, RR * 1.05, W * 0.6, tyre=0.0, kind="dish", hub=0.42, bolts=5, dish=0.2, cap=0.75, sides=v.n(28, 10))
    v.slab((xb - 0.025, 0.24, 0.52), (xb, 0.58, 0.56), "shade")                                            # the can's bracket
    v.jerrycan((xb - 0.105, 0.41, 0.56), 90.0)
    if fine:
        v.tube([(xb - 0.19, 0.24, 0.86), (xb - 0.20, 0.41, 0.97), (xb - 0.19, 0.58, 0.86)], 0.008, "shade", 5)   # its strap
    for s in (LEFT, RIGHT):
        v.slab((xb - 0.07, s * 0.42, 0.43), (xb, s * 0.60, 0.52), coat, 0.008) if True else None             # the bumperettes
        v.lathe((xb - 0.002, s * 0.58, 0.68), -X, [(0.03, 0.0), (0.03, 0.035), (0.0, 0.04), (0.0, 0.0)], "red", 10)
        if fine:
            v.lathe((xb - 0.002, s * 0.58, 0.60), -X, [(0.025, 0.0), (0.025, 0.028), (0.0, 0.03), (0.0, 0.0)], "lamp", 8)
            for x in (-1.22, -0.80):                                                                        # the grab handles
                v.tube([(x - 0.07, s * yb, 0.90), (x - 0.06, s * (yb + 0.04), 0.88), (x + 0.06, s * (yb + 0.04), 0.88), (x + 0.07, s * yb, 0.90)], 0.009, coat, 5)
    v.slab((xb - 0.05, -0.08, 0.42), (xb, 0.08, 0.52), "shade", 0.006)
    v.hook((xb - 0.06, 0.0, 0.46), -X, 0.07)
    v.socket("hook", (xb - 0.13, 0.0, 0.46), -X, part="hull", note="the pintle: a 37 mm gun's or a trailer's eye goes on it")
    v.roll((-1.47, -0.60, zt + 0.10), (-1.47, 0.60, zt + 0.10), r=0.085, paint="canvas", straps=3)          # the top, folded down on its bows
    if fine:
        for s in (LEFT, RIGHT):
            v.tube([(-1.47, s * 0.60, zt + 0.10), (-1.40, s * (yb - 0.01), zt + 0.02)], 0.012, "shade", 5)
    # the axe and shovel on her left side, under the cut
    v.axe((-0.42, yb + 0.02, 0.57), yaw=0.0, long=0.68, roll=90.0)
    v.shovel((0.30, yb + 0.02, 0.64), yaw=180.0, long=0.86, roll=90.0)
    # the silencer under her right side, the pipe out at her back
    v.silencer((-0.35, -0.50, 0.36), (-0.95, -0.50, 0.36), 0.06, bands=2, pipe=[(0.6, -0.30, 0.40), (0.0, -0.45, 0.36), (-0.35, -0.50, 0.36)],
               tail=((-0.95, -0.50, 0.36), (-1, 0, 0)))
    v.rod((-0.95, -0.50, 0.36), (-1.60, -0.48, 0.38), 0.022, "rust", 8)
    v.socket("exhaust.1", (-1.62, -0.48, 0.38), (-1, 0, 0))

    # ---- the gun on its post between the front seats ---------------------------------------------------------------
    if mg:
        if fine:                                                                                            # the post's three stays to the floor
            for a in (0.0, 120.0, 240.0):
                w = Vector((math.cos(math.radians(a)), math.sin(math.radians(a)), 0.0))
                v.rod(Vector((-0.10, 0.0, zfl)) + w * 0.16, (-0.10, 0.0, zfl + 0.30), 0.012, "upper", 6)
        v.post_gun("m1919", (-0.10, 0.0, zfl), 0.90, arc=(-160.0, 160.0), lay=(-10.0, 45.0))
    v.use("hull")

    # ---- the men, the marks, what happens to her -------------------------------------------------------------------
    v.socket("driver", (-0.21, 0.33, 0.80), X, part="hull", note="the driver, seated: the top of his seat, 0.32 over the floor his boots are on (the footwell runs on to the firewall 0.77 ahead); his hands on grip.driver.l and grip.driver.r")
    v.socket("grip.driver.l", grip_l, Z, part="hull", note="the driver's left hand on the wheel's rim")
    v.socket("grip.driver.r", grip_r, Z, part="hull", note="the driver's right hand on the wheel's rim")
    v.socket("rider.1", (-0.21, -0.33, 0.80), X, part="hull", note="the man beside the driver, seated (the top of his seat, as the driver's; his hands free)")
    v.socket("rider.2", (-1.25, 0.30, 0.86), X, part="hull", note="a man on the seat behind, seated facing ahead: the top of his seat, 0.38 over the floor")
    v.socket("rider.3", (-1.25, -0.30, 0.86), X, part="hull", note="a man on the seat behind, seated facing ahead")
    if mg:
        v.socket("gunner", (-0.92, 0.0, zfl), X, part="hull",
                 note="the gunner's feet, on the floor behind the front seats (in place of rider.2 and rider.3): he stands, his hands on grip.gunner.l and grip.gunner.r about 0.9 up, and turns his body with the gun")
    v.mark(tag, (1.15, 0.0, 1.004), Z, 0.25, part="hull")                                             # (ahead of the folded screen)
    for s in (LEFT, RIGHT):
        v.mark(tag, (-1.10, s * (yb + 0.001), 0.80), Y * s, 0.12, part="hull")
    v.socket("engine", (1.00, 0.0, 1.0), Z, part="hull", note="the bonnet: smoke and the first fire")
    v.socket("fire.1", (-0.30, 0.0, 0.70), Z, part="hull", note="in the seats")
    v.socket("fire.2", (-1.20, 0.0, 0.84), Z, part="hull")
    v.socket("hit.front", (xg + 0.02, 0.0, 0.75), X, part="hull")
    v.socket("hit.left", (-0.20, yb, 0.65), Y, part="hull")
    v.socket("hit.right", (-0.20, -yb, 0.65), -Y, part="hull")
    v.socket("hit.rear", (xb, 0.0, 0.75), -X, part="hull")
    v.carrier_clips("lorry_idle_loop", "lorry_drive_loop", mg="mg" if mg else None, soft=1.5)
    v.gun_controls("The post stands between the front seats: with the screen folded the gun fires over the bonnet; the gunner stands behind it, "
                   "facing the way the gun points.")
    v.m.info.setdefault("file", {})["swaps"] = {
        "how": "mg: off scales the parts of the gun to nothing (a part scaled to nothing is not drawn), and the two men behind sit on their seat.",
        "mg": {"default": "on", "choices": {"on": {"what": "the .30 Browning on its post (scouts, the reconnaissance troops, and many more)", "parts": ["mg_mount", "mg_cradle"]},
                                            "off": {"what": "no gun: a staff or liaison car", "parts": []}}}}
    return v


# =====================================================================================================================
def kubelwagen(fine=True, tag="GER", paint=None):
    """The light car (Volkswagen Type 82): 3.74 m long, 1.60 m wide, 1.35 m to the top of her windscreen. Four men on
    two seats and a bench. Dark grey to early 1943, dark yellow after, the desert coats in Africa: a paint swap."""
    from tanks_ger import GER_COATS
    v = LightWorks("Kubelwagen", "Light car (Kubelwagen)", "car", tag, fine, paint=GER_COATS[paint or "grey"][0])
    v.dice = 0.2
    v.worn_kw = dict(LIGHT_WORN, ground="field")
    v.root(hull_pivot=(0.0, 0.0, 0.55), rock=(-5.0, 5.0))
    R, W, RR = 0.33, 0.14, 0.19
    xf, xr, yw = 1.20, -1.20, 0.678
    yb, zt, zfl, zb = 0.76, 1.02, 0.42, 0.36            # the sides, their top edge, the floor, the pan's foot
    xn, xs, xd, xe = 1.92, 0.62, -1.06, -1.84          # the nose, the screen's foot, the back of the seats, the tail
    zl0, zl1 = 0.98, 0.79                               # the front lid at the screen and at the nose

    # ---- under her: the front axle's two tubes and trailing arms, the swing axles behind, the wheels ---------------
    v.use("vehicle")
    for dz in (0.0, 0.11):
        v.rod((xf + 0.16, -0.52, 0.36 + dz), (xf + 0.16, 0.52, 0.36 + dz), 0.035, "shade", 10)
    for s in (LEFT, RIGHT):
        v.rod((xf + 0.16, s * 0.50, 0.40), (xf, s * (yw - 0.10), R), 0.03, "shade", 8)
        v.rod((xr + 0.05, s * 0.12, 0.36), (xr, s * (yw - 0.10), R), 0.04, "shade", 10)                 # a swing axle
        v.slab((xr - 0.10, s * (yw - 0.12), R - 0.12), (xr + 0.10, s * (yw - 0.06), R + 0.10), "shade", 0.02)   # its reduction gear at the hub
    v.ball((xr + 0.05, 0.0, 0.40), 0.16, "shade", 12, 8, squash=0.8)                                     # the gearbox and differential
    for letter, s in SIDES:
        v.steered_wheel(letter, s, xf, yw, R, W, RR, lock=30.0, bolts=5)
        v.lorry_wheel(f"wheel_back_{letter}", "vehicle", (xr, s * yw, R), s, R, W, RR, bolts=5)
        v.use("vehicle")
        v.socket("dust." + ("left" if s > 0 else "right"), (xr - R - 0.05, s * yw, 0.05), (-1, 0, 0.3), part="vehicle")

    # ---- the pan, the nose with the spare on its lid, the wings, the lamps ---------------------------------------
    v.use("hull")
    v.slab((xe + 0.08, -0.60, zb), (xn - 0.10, 0.60, zfl), "shade", 0.01)                              # the floor pan and its backbone
    lid_n = Vector((zl0 - zl1, 0.0, xn - 0.12 - xs)).normalized()
    v.hull([(xs, 0.50, zl0), (xn - 0.12, 0.50, zl1), (xn, 0.48, zl1 - 0.06), (xn + 0.02, 0.46, 0.56), (xn - 0.06, 0.46, 0.46), (xs, 0.50, 0.46)], coat, 0.012)
    arch = lambda x0, r, a0, a1, st: [(x0 + r * math.cos(math.radians(a)), R + r * math.sin(math.radians(a))) for a in range(a0, a1, st)]   # noqa: E731
    front_side = ([(xs, 0.46), (xs, zl0 - 0.06), (xn - 0.14, zl1 - 0.04), (xn - 0.02, zl1 - 0.10), (xn + 0.01, 0.56), (xn - 0.04, 0.48)]
                  + arch(xf, 0.42, 20, 165, 12) + [(xf - 0.43, 0.46)])
    for s in (LEFT, RIGHT):
        v.extrude(front_side, s * 0.48, s * yb, coat, 0.008)                                               # the wing and the nose's corner
        v.headlamp((xn - 0.30, s * 0.62, zl1 + 0.02), X, 0.08, stalk=0.07, cover=True)
    v.headlamp((xn - 0.20, 0.42, zl1 + 0.0), X, 0.045, stalk=0.04, cover=True)                            # the blackout lamp
    sc = Vector((1.26, 0.0, zl0 + (zl1 - zl0) * (1.26 - xs) / (xn - 0.12 - xs))) + lid_n * (W / 2 + 0.01)
    v.tyre(sc, lid_n, R, RR, W, lugs=16)                                                                    # the spare, lying on her lid
    v.wheel_disc(sc + lid_n * (W * 0.1), lid_n, RR * 1.05, W * 0.6, tyre=0.0, kind="dish", hub=0.42, bolts=5, dish=0.2, cap=0.75, sides=v.n(26, 10))
    v.slab((xn + 0.02, -0.70, 0.50), (xn + 0.07, 0.70, 0.57), coat, 0.01)                                # the bumper bar
    for s in (LEFT, RIGHT):
        v.slab((xn - 0.06, s * 0.45 - 0.02, 0.50), (xn + 0.03, s * 0.45 + 0.02, 0.56), "shade")
        v.hook((xn - 0.02, s * 0.30, 0.44), X, 0.05)

    # ---- the tub: flat sides from the screen to the tail, four doors, the pressed ribs, the arches behind -------
    side = ([(xs, zfl - 0.04), (xs, zt), (xd, zt), (xe + 0.08, 0.84), (xe, 0.80), (xe, 0.46), (xr - 0.44, 0.46)]
            + arch(xr, 0.42, 168, 10, -12) + [(xr + 0.44, zfl - 0.04)])
    for s in (LEFT, RIGHT):
        v.extrude(side, s * (yb - 0.02), s * yb, coat, 0.004)
        if fine:
            for z in (0.56, 0.68):                                                                          # the ribs pressed in her sides
                v.rod((xs - 0.04, s * yb, z), (xr + 0.47, s * yb, z), 0.009, coat, 5)
            for x in (xs - 0.03, -0.18, -0.21, xr + 0.46):                                                  # the doors' seams
                v.slab((x - 0.003, s * (yb - 0.001), zfl), (x + 0.003, s * (yb + 0.002), zt - 0.01), "black")
            for z in (0.58, 0.88):                                                                          # their hinges: front doors at the front, back doors at the back
                v.hinge((xs - 0.06, s * (yb + 0.01), z - 0.04), (xs - 0.06, s * (yb + 0.01), z + 0.04), 0.011)
                v.hinge((xr + 0.49, s * (yb + 0.01), z - 0.04), (xr + 0.49, s * (yb + 0.01), z + 0.04), 0.011)
            v.handle((-0.28, s * (yb + 0.006), 0.92), (-0.14, s * (yb + 0.006), 0.92), Y * s, r=0.007, reach=0.025, paint="steel")
            v.tube([Vector((x, s * (yb - 0.008), zt + 0.004)) for x in (xs, xd)], 0.012, coat, 6)           # the rolled top edge
    v.hull([(xs, 0.74, zt), (xs, 0.50, zl0), (xs - 0.10, 0.74, zt), (xs - 0.10, 0.50, zt), (xs, 0.74, zl0 - 0.20), (xs - 0.10, 0.74, zl0 - 0.20)], coat, 0.008)   # the cowl
    v.slab((xs - 0.11, -0.74, zfl), (xs - 0.09, 0.74, zt - 0.01), "upper")                                # the dash
    v.slab((xe + 0.02, -0.74, zfl), (xd, 0.74, zfl + 0.01), "upper")
    for s in (LEFT, RIGHT):                                                                                  # the housings over the back wheels
        hump = arch(xr, 0.44, 180, -1, -20)
        v.extrude([(hump[0][0], zfl)] + hump + [(hump[-1][0], zfl)], s * 0.46, s * (yb - 0.02), "upper", 0.01)
    # the engine's lid, sloping down her tail, louvred; the tail panel, the lamps, the silencer across her foot
    v.hull([(xd, 0.74, zt), (xd, 0.74, zt - 0.06), (xe + 0.08, 0.74, 0.84), (xe, 0.74, 0.80), (xe, 0.74, 0.46), (xd, 0.74, 0.46)], coat, 0.012)
    deck = Vector((xe + 0.08 - xd, 0.0, 0.84 - zt)).normalized()
    deck_n = Vector((-deck.z, 0.0, deck.x))
    if deck_n.z < 0:
        deck_n = -deck_n
    mid = Vector(((xd + xe + 0.08) / 2, 0.0, (zt + 0.84) / 2))
    v.louvres(mid + deck_n * 0.004, (0.5, 0.9), deck_n, count=8, down=deck)
    for s in (LEFT, RIGHT):
        v.lathe((xe - 0.002, s * 0.60, 0.70), -X, [(0.03, 0.0), (0.03, 0.035), (0.0, 0.04), (0.0, 0.0)], "red", 10)
    v.rod((xe - 0.08, -0.45, 0.42), (xe - 0.08, 0.45, 0.42), 0.075, "rust", v.n(16, 8))
    v.rod((xe - 0.08, -0.40, 0.42), (xe - 0.20, -0.42, 0.40), 0.02, "rust", 6)
    v.socket("exhaust.1", (xe - 0.21, -0.42, 0.40), (-1, 0, 0))
    v.hook((xe - 0.02, 0.0, 0.50), -X, 0.06)
    v.socket("hook", (xe - 0.08, 0.0, 0.50), -X, part="hull", note="the towing eye: a small trailer's eye goes on it")
    v.roll((xd - 0.02, -0.66, zt + 0.08), (xd - 0.02, 0.66, zt + 0.08), r=0.08, paint="canvas", straps=2)  # the top, folded down
    v.shovel((0.30, -(yb + 0.02), 0.78), yaw=180.0, long=0.86, roll=90.0)

    # ---- inside: two seats and the bench, the wheel ------------------------------------------------------------
    for s in (LEFT, RIGHT):
        v.slab((-0.46, s * 0.10, zfl), (-0.08, s * 0.58, 0.66), "upper", 0.01)
        v.seat(-0.48, -0.06, *sorted((s * 0.10, s * 0.60)), 0.76, back=0.40, lean=0.08, paint="leather")
    v.seat(-0.96, -0.60, -0.62, 0.62, 0.80, back=0.36, lean=0.06, paint="leather")
    grip_l, grip_r = v.steering_wheel((xs + 0.05, 0.36, 0.62), (0.16, 0.36, 1.08), r=0.20)

    # ---- the windscreen, up: one pane in a frame -----------------------------------------------------------------
    v.part("screen", "hull", (xs - 0.02, 0.0, zt + 0.02), axis=(0, 1, 0), limits=[0.0, 80.0], kind="turn",
           note="the windscreen on its hinges at the cowl: built standing; + folds it forward flat on the lid")
    lean = 0.10
    p = lambda y, z: Vector((xs - 0.02 - lean * (z - zt), y, z))                                         # noqa: E731
    fy, z0, z1 = 0.72, zt + 0.03, zt + 0.36
    for a, b in ((p(-fy, z0), p(fy, z0)), (p(-fy, z1), p(fy, z1)), (p(-fy, z0), p(-fy, z1)), (p(fy, z0), p(fy, z1))):
        v.rod(a, b, 0.016, coat, 6)
    v.plate([p(fy - 0.02, z0 + 0.02), p(-fy + 0.02, z0 + 0.02), p(-fy + 0.02, z1 - 0.02), p(fy - 0.02, z1 - 0.02)], 0.006, "glass")
    v.use("hull")

    # ---- the men, the marks, what happens to her -------------------------------------------------------------------
    v.socket("driver", (-0.27, 0.36, 0.76), X, part="hull", note="the driver, seated: the top of his seat, 0.34 over the floor his boots are on (the footwell runs on to the dash 0.78 ahead); his hands on grip.driver.l and grip.driver.r")
    v.socket("grip.driver.l", grip_l, Z, part="hull", note="the driver's left hand on the wheel's rim")
    v.socket("grip.driver.r", grip_r, Z, part="hull", note="the driver's right hand on the wheel's rim")
    v.socket("rider.1", (-0.27, -0.36, 0.76), X, part="hull", note="the man beside the driver, seated (the top of his seat)")
    v.socket("rider.2", (-0.78, 0.32, 0.80), X, part="hull", note="a man on the bench behind, seated facing ahead: the top of the bench, 0.38 over the floor")
    v.socket("rider.3", (-0.78, -0.32, 0.80), X, part="hull", note="a man on the bench behind, seated facing ahead")
    for s in (LEFT, RIGHT):
        v.mark(tag, (-1.30, s * (yb + 0.001), 0.90), Y * s, 0.09, part="hull")
    v.socket("engine", ((xd + xe) / 2, 0.0, 0.94), Z, part="hull", note="the engine's lid, in her tail: smoke and the first fire")
    v.socket("fire.1", (-0.30, 0.0, 0.66), Z, part="hull", note="in the seats")
    v.socket("fire.2", (1.20, 0.0, 0.90), Z, part="hull", note="her fuel tank, under the front lid")
    v.socket("hit.front", (xn, 0.0, 0.70), X, part="hull")
    v.socket("hit.left", (-0.20, yb, 0.70), Y, part="hull")
    v.socket("hit.right", (-0.20, -yb, 0.70), -Y, part="hull")
    v.socket("hit.rear", (xe, 0.0, 0.65), -X, part="hull")
    v.carrier_clips("lorry_idle_loop", "lorry_drive_loop", soft=1.5)
    v.worn(**v.worn_kw)
    v.m.info.setdefault("file", {})["swaps"] = {"how": "paint: put the chosen coat's colours in place of the paints of the same names.", "paint": coats(v, paint or "grey")}
    return v
